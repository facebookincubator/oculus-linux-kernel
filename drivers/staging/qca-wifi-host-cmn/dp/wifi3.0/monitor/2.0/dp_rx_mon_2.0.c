/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "hal_be_hw_headers.h"
#include "dp_types.h"
#include "hal_be_rx.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "hal_be_api_mon.h"
#include "dp_internal.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include <qdf_flex_mem.h>
#include "qdf_nbuf_frag.h"
#include "dp_mon.h"
#include <dp_rx_mon.h>
#include <dp_mon_2.0.h>
#include <dp_rx_mon.h>
#include <dp_rx_mon_2.0.h>
#include <dp_rx.h>
#include <dp_be.h>
#include <hal_be_api_mon.h>
#ifdef QCA_SUPPORT_LITE_MONITOR
#include "dp_lite_mon.h"
#endif

#define F_MASK 0xFFFF
#define TEST_MASK 0xCBF

#if defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) ||\
	    defined(WLAN_SUPPORT_RX_FLOW_TAG)

#ifdef QCA_TEST_MON_PF_TAGS_STATS

static
void dp_rx_mon_print_tag_buf(uint8_t *buf, uint16_t room)
{
	print_hex_dump(KERN_ERR, "TLV BUFFER: ", DUMP_PREFIX_NONE,
		       32, 2, buf, room, false);
}

#else
static
void dp_rx_mon_print_tag_buf(uint8_t *buf, uint16_t room)
{
}

#endif

/**
 * dp_rx_mon_update_drop_cnt() - Update drop statistics
 *
 * @mon_pdev: monitor pdev
 * @hal_mon_rx_desc: HAL monitor desc
 *
 * Return: void
 */
static inline void
dp_rx_mon_update_drop_cnt(struct dp_mon_pdev *mon_pdev,
			  struct hal_mon_desc *hal_mon_rx_desc)
{
	mon_pdev->rx_mon_stats.empty_desc_ppdu++;
	mon_pdev->rx_mon_stats.ppdu_drop_cnt +=
		hal_mon_rx_desc->ppdu_drop_count;
	mon_pdev->rx_mon_stats.mpdu_drop_cnt +=
		hal_mon_rx_desc->mpdu_drop_count;
	if (hal_mon_rx_desc->end_of_ppdu_dropped)
		mon_pdev->rx_mon_stats.end_of_ppdu_drop_cnt++;
	mon_pdev->rx_mon_stats.tlv_drop_cnt +=
		hal_mon_rx_desc->tlv_drop_count;
}

static
void dp_rx_mon_set_zero(qdf_nbuf_t nbuf)
{
	qdf_mem_zero(qdf_nbuf_head(nbuf), DP_RX_MON_TLV_ROOM);
}

/**
 * dp_rx_mon_get_ppdu_info() - Get PPDU info from freelist
 *
 * @mon_pdev: monitor pdev
 *
 * Return: ppdu_info
 */
static inline struct hal_rx_ppdu_info*
dp_rx_mon_get_ppdu_info(struct dp_mon_pdev *mon_pdev)
{
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct hal_rx_ppdu_info *ppdu_info, *temp_ppdu_info;

	qdf_spin_lock_bh(&mon_pdev_be->ppdu_info_lock);
	TAILQ_FOREACH_SAFE(ppdu_info,
			   &mon_pdev_be->rx_mon_free_queue,
			   ppdu_list_elem,
			   temp_ppdu_info) {
		TAILQ_REMOVE(&mon_pdev_be->rx_mon_free_queue,
			     ppdu_info, ppdu_free_list_elem);

		if (ppdu_info) {
			mon_pdev_be->total_free_elem--;
			break;
		}
	}
	qdf_spin_unlock_bh(&mon_pdev_be->ppdu_info_lock);

	return ppdu_info;
}

static inline void
__dp_rx_mon_free_ppdu_info(struct dp_mon_pdev *mon_pdev,
			   struct hal_rx_ppdu_info *ppdu_info)
{
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	qdf_spin_lock_bh(&mon_pdev_be->ppdu_info_lock);
	if (ppdu_info) {
		TAILQ_INSERT_TAIL(&mon_pdev_be->rx_mon_free_queue, ppdu_info,
				  ppdu_free_list_elem);
		mon_pdev_be->total_free_elem++;
	}
	qdf_spin_unlock_bh(&mon_pdev_be->ppdu_info_lock);
}

/**
 * dp_rx_mon_nbuf_add_rx_frag() -  Add frag to SKB
 *
 * @nbuf: SKB to which frag is going to be added
 * @frag: frag to be added to SKB
 * @frag_len: frag length
 * @offset: frag offset
 * @buf_size: buffer size
 * @frag_ref: take frag ref
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_rx_mon_nbuf_add_rx_frag(qdf_nbuf_t nbuf, qdf_frag_t *frag,
			   uint16_t frag_len, uint16_t offset,
			   uint16_t buf_size, bool frag_ref)
{
	uint8_t num_frags;

	num_frags = qdf_nbuf_get_nr_frags(nbuf);
	if (num_frags < QDF_NBUF_MAX_FRAGS) {
		qdf_nbuf_add_rx_frag(frag, nbuf,
				     offset,
				     frag_len,
				     buf_size,
				     frag_ref);
		return QDF_STATUS_SUCCESS;
	}
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_mon_free_parent_nbuf() - Free parent SKB
 *
 * @mon_pdev: monitor pdev
 * @nbuf: SKB to be freed
 *
 * Return: void
 */
void
dp_mon_free_parent_nbuf(struct dp_mon_pdev *mon_pdev,
			qdf_nbuf_t nbuf)
{
	mon_pdev->rx_mon_stats.parent_buf_free++;
	qdf_nbuf_free(nbuf);
}

void
dp_rx_mon_shift_pf_tag_in_headroom(qdf_nbuf_t nbuf, struct dp_soc *soc,
				   struct hal_rx_ppdu_info *ppdu_info)
{
	uint32_t room = 0;
	uint16_t msdu_count = 0;
	uint16_t *dp = NULL;
	uint16_t *hp = NULL;
	uint16_t tlv_data_len, total_tlv_len;
	uint32_t bytes = 0;

	if (qdf_unlikely(!soc)) {
		dp_mon_err("Soc[%pK] Null. Can't update pftag to nbuf headroom",
			   soc);
		qdf_assert_always(0);
	}

	if (!wlan_cfg_is_rx_mon_protocol_flow_tag_enabled(soc->wlan_cfg_ctx))
		return;

	if (qdf_unlikely(!nbuf))
		return;

	/* Headroom must be have enough space for tlv to be added*/
	if (qdf_unlikely(qdf_nbuf_headroom(nbuf) < DP_RX_MON_TLV_ROOM)) {
		dp_mon_err("Headroom[%d] < DP_RX_MON_TLV_ROOM[%d]",
			   qdf_nbuf_headroom(nbuf), DP_RX_MON_TLV_ROOM);
		return;
	}

	hp = (uint16_t *)qdf_nbuf_head(nbuf);
	msdu_count = *hp;

	if (qdf_unlikely(!msdu_count))
		return;

	dp_mon_debug("msdu_count: %d", msdu_count);

	room = DP_RX_MON_PF_TAG_LEN_PER_FRAG * msdu_count;
	tlv_data_len = DP_RX_MON_TLV_MSDU_CNT + (room);
	total_tlv_len = DP_RX_MON_TLV_HDR_LEN + tlv_data_len;

	//1. store space for MARKER
	dp = (uint16_t *)qdf_nbuf_push_head(nbuf, sizeof(uint16_t));
	if (qdf_likely(dp)) {
		*(uint16_t *)dp = DP_RX_MON_TLV_HDR_MARKER;
		bytes += sizeof(uint16_t);
	}

	//2. store space for total size
	dp = (uint16_t *)qdf_nbuf_push_head(nbuf, sizeof(uint16_t));
	if (qdf_likely(dp)) {
		*(uint16_t *)dp = total_tlv_len;
		bytes += sizeof(uint16_t);
	}

	//create TLV
	bytes += dp_mon_rx_add_tlv(DP_RX_MON_TLV_PF_ID, tlv_data_len, hp, nbuf);

	dp_rx_mon_print_tag_buf(qdf_nbuf_data(nbuf), total_tlv_len);

	qdf_nbuf_pull_head(nbuf, bytes);

}

void
dp_rx_mon_pf_tag_to_buf_headroom_2_0(void *nbuf,
				     struct hal_rx_ppdu_info *ppdu_info,
				     struct dp_pdev *pdev, struct dp_soc *soc)
{
	uint8_t *nbuf_head = NULL;
	uint8_t user_id;
	struct hal_rx_mon_msdu_info *msdu_info;
	uint16_t flow_id;
	uint16_t cce_metadata;
	uint16_t protocol_tag = 0;
	uint32_t flow_tag;
	uint8_t invalid_cce = 0, invalid_fse = 0;

	if (qdf_unlikely(!soc)) {
		dp_mon_err("Soc[%pK] Null. Can't update pftag to nbuf headroom",
			   soc);
		qdf_assert_always(0);
	}

	if (!wlan_cfg_is_rx_mon_protocol_flow_tag_enabled(soc->wlan_cfg_ctx))
		return;

	if (qdf_unlikely(!nbuf))
		return;

	/* Headroom must be have enough space for tlv to be added*/
	if (qdf_unlikely(qdf_nbuf_headroom(nbuf) < DP_RX_MON_TLV_ROOM)) {
		dp_mon_err("Headroom[%d] < DP_RX_MON_TLV_ROOM[%d]",
			   qdf_nbuf_headroom(nbuf), DP_RX_MON_TLV_ROOM);
		return;
	}

	user_id = ppdu_info->user_id;
	if (qdf_unlikely(user_id > HAL_MAX_UL_MU_USERS)) {
		dp_mon_debug("Invalid user_id user_id: %d pdev: %pK", user_id, pdev);
		return;
	}

	msdu_info = &ppdu_info->msdu[user_id];
	flow_id = ppdu_info->rx_msdu_info[user_id].flow_idx;
	cce_metadata = ppdu_info->rx_msdu_info[user_id].cce_metadata -
		       RX_PROTOCOL_TAG_START_OFFSET;

	flow_tag = ppdu_info->rx_msdu_info[user_id].fse_metadata & F_MASK;

	if (qdf_unlikely((cce_metadata > RX_PROTOCOL_TAG_MAX - 1) ||
			 (cce_metadata > 0 && cce_metadata < 4))) {
		dp_mon_debug("Invalid user_id cce_metadata: %d pdev: %pK", cce_metadata, pdev);
		invalid_cce = 1;
		protocol_tag = cce_metadata;
	} else {
		protocol_tag = pdev->rx_proto_tag_map[cce_metadata].tag;
		dp_mon_rx_update_rx_protocol_tag_stats(pdev, cce_metadata);
	}

	if (flow_tag > 0) {
		dp_mon_rx_update_rx_flow_tag_stats(pdev, flow_id);
	} else {
		dp_mon_debug("Invalid flow_tag: %d pdev: %pK ", flow_tag, pdev);
		invalid_fse = 1;
	}

	if (invalid_cce && invalid_fse)
		return;

	if (msdu_info->msdu_index >= DP_RX_MON_MAX_MSDU) {
		dp_mon_err("msdu_index causes overflow in headroom");
		return;
	}

	dp_mon_debug("protocol_tag: %d, cce_metadata: %d, flow_tag: %d",
		     protocol_tag, cce_metadata, flow_tag);

	dp_mon_debug("msdu_index: %d", msdu_info->msdu_index);


	nbuf_head = qdf_nbuf_head(nbuf);

	*((uint16_t *)nbuf_head) = msdu_info->msdu_index + 1;
	nbuf_head += DP_RX_MON_TLV_MSDU_CNT;

	nbuf_head += ((msdu_info->msdu_index) * DP_RX_MON_PF_TAG_SIZE);
	if (!invalid_cce)
		*((uint16_t *)nbuf_head) = protocol_tag;
	nbuf_head += sizeof(uint16_t);
	if (!invalid_fse)
		*((uint16_t *)nbuf_head) = flow_tag;
}

#else

static
void dp_rx_mon_set_zero(qdf_nbuf_t nbuf)
{
}

static
void dp_rx_mon_shift_pf_tag_in_headroom(qdf_nbuf_t nbuf, struct dp_soc *soc,
					struct hal_rx_ppdu_info *ppdu_info)
{
}

static
void dp_rx_mon_pf_tag_to_buf_headroom_2_0(void *nbuf,
					  struct hal_rx_ppdu_info *ppdu_info,
					  struct dp_pdev *pdev,
					  struct dp_soc *soc)
{
}

#endif

/**
 * dp_rx_mon_free_mpdu_queue() - Free MPDU queue
 * @mon_pdev: monitor pdev
 * @ppdu_info: PPDU info
 *
 * Return: Void
 */

static void dp_rx_mon_free_mpdu_queue(struct dp_mon_pdev *mon_pdev,
				      struct hal_rx_ppdu_info *ppdu_info)
{
	uint8_t user;
	qdf_nbuf_t mpdu;

	for (user = 0; user < HAL_MAX_UL_MU_USERS; user++) {
		if (!qdf_nbuf_is_queue_empty(&ppdu_info->mpdu_q[user])) {
			while ((mpdu = qdf_nbuf_queue_remove(&ppdu_info->mpdu_q[user])) != NULL)
				dp_mon_free_parent_nbuf(mon_pdev, mpdu);
		}
	}
}

/**
 * dp_rx_mon_free_ppdu_info() - Free PPDU info
 * @pdev: DP pdev
 * @ppdu_info: PPDU info
 *
 * Return: Void
 */
static void
dp_rx_mon_free_ppdu_info(struct dp_pdev *pdev,
			 struct hal_rx_ppdu_info *ppdu_info)
{
	struct dp_mon_pdev *mon_pdev;

	mon_pdev = (struct dp_mon_pdev *)pdev->monitor_pdev;
	dp_rx_mon_free_mpdu_queue(mon_pdev, ppdu_info);
	__dp_rx_mon_free_ppdu_info(mon_pdev, ppdu_info);
}

void dp_rx_mon_drain_wq(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev;
	struct hal_rx_ppdu_info *ppdu_info = NULL;
	struct hal_rx_ppdu_info *temp_ppdu_info = NULL;
	struct dp_mon_pdev_be *mon_pdev_be;

	if (qdf_unlikely(!pdev)) {
		dp_mon_debug("Pdev is NULL");
		return;
	}

	mon_pdev = (struct dp_mon_pdev *)pdev->monitor_pdev;
	if (qdf_unlikely(!mon_pdev)) {
		dp_mon_debug("monitor pdev is NULL");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	qdf_spin_lock_bh(&mon_pdev_be->rx_mon_wq_lock);
	TAILQ_FOREACH_SAFE(ppdu_info,
			   &mon_pdev_be->rx_mon_queue,
			   ppdu_list_elem,
			   temp_ppdu_info) {
		mon_pdev_be->rx_mon_queue_depth--;
		TAILQ_REMOVE(&mon_pdev_be->rx_mon_queue,
			     ppdu_info, ppdu_list_elem);

		dp_rx_mon_free_ppdu_info(pdev, ppdu_info);
	}
	qdf_spin_unlock_bh(&mon_pdev_be->rx_mon_wq_lock);
}

/**
 * dp_rx_mon_deliver_mpdu() - Deliver MPDU to osif layer
 *
 * @mon_pdev: monitor pdev
 * @mpdu: MPDU nbuf
 * @rx_status: monitor status
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_rx_mon_deliver_mpdu(struct dp_mon_pdev *mon_pdev,
		       qdf_nbuf_t mpdu,
		       struct mon_rx_status *rx_status)
{
	qdf_nbuf_t nbuf;

	if (mon_pdev->mvdev && mon_pdev->mvdev->monitor_vdev->osif_rx_mon) {
		mon_pdev->rx_mon_stats.mpdus_buf_to_stack++;
		nbuf = qdf_nbuf_get_ext_list(mpdu);

		while (nbuf) {
			mon_pdev->rx_mon_stats.mpdus_buf_to_stack++;
			nbuf = nbuf->next;
		}
		mon_pdev->mvdev->monitor_vdev->osif_rx_mon(mon_pdev->mvdev->osif_vdev,
							   mpdu,
							   rx_status);
	} else {
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_mon_process_ppdu_info() - Process PPDU info
 * @pdev: DP pdev
 * @ppdu_info: PPDU info
 *
 * Return: Void
 */
static void
dp_rx_mon_process_ppdu_info(struct dp_pdev *pdev,
			    struct hal_rx_ppdu_info *ppdu_info)
{
	struct dp_mon_pdev *mon_pdev = (struct dp_mon_pdev *)pdev->monitor_pdev;
	uint8_t user;
	qdf_nbuf_t mpdu;

	if (!ppdu_info)
		return;

	for (user = 0; user < ppdu_info->com_info.num_users; user++) {
		uint16_t mpdu_count;
		uint16_t mpdu_idx;
		struct hal_rx_mon_mpdu_info *mpdu_meta;
		QDF_STATUS status;

		if (user >= HAL_MAX_UL_MU_USERS) {
			dp_mon_err("num user exceeds max limit");
			return;
		}

		mpdu_count  = ppdu_info->mpdu_count[user];
		ppdu_info->rx_status.rx_user_status =
					&ppdu_info->rx_user_status[user];
		for (mpdu_idx = 0; mpdu_idx < mpdu_count; mpdu_idx++) {
			mpdu = qdf_nbuf_queue_remove(&ppdu_info->mpdu_q[user]);

			if (!mpdu)
				continue;

			mpdu_meta = (struct hal_rx_mon_mpdu_info *)qdf_nbuf_data(mpdu);

			if (dp_lite_mon_is_rx_enabled(mon_pdev)) {
				status = dp_lite_mon_rx_mpdu_process(pdev, ppdu_info,
								     mpdu, mpdu_idx, user);
				if (status != QDF_STATUS_SUCCESS) {
					dp_mon_free_parent_nbuf(mon_pdev, mpdu);
					continue;
				}
			} else {
				if (mpdu_meta->full_pkt) {
					if (qdf_unlikely(mpdu_meta->truncated)) {
						dp_mon_free_parent_nbuf(mon_pdev, mpdu);
						continue;
					}

					status = dp_rx_mon_handle_full_mon(pdev,
									   ppdu_info, mpdu);
					if (status != QDF_STATUS_SUCCESS) {
						dp_mon_free_parent_nbuf(mon_pdev, mpdu);
						continue;
					}
				} else {
					dp_mon_free_parent_nbuf(mon_pdev, mpdu);
					continue;
				}

				/* reset mpdu metadata and apply radiotap header over MPDU */
				qdf_mem_zero(mpdu_meta, sizeof(struct hal_rx_mon_mpdu_info));
				if (!qdf_nbuf_update_radiotap(&ppdu_info->rx_status,
							      mpdu,
							      qdf_nbuf_headroom(mpdu))) {
					dp_mon_err("failed to update radiotap pdev: %pK",
						   pdev);
				}

				dp_rx_mon_shift_pf_tag_in_headroom(mpdu,
								   pdev->soc,
								   ppdu_info);

				dp_rx_mon_process_dest_pktlog(pdev->soc,
							      pdev->pdev_id,
							      mpdu);
				/* Deliver MPDU to osif layer */
				status = dp_rx_mon_deliver_mpdu(mon_pdev,
								mpdu,
								&ppdu_info->rx_status);
				if (status != QDF_STATUS_SUCCESS)
					dp_mon_free_parent_nbuf(mon_pdev, mpdu);
			}
		}
	}

	dp_rx_mon_free_mpdu_queue(mon_pdev, ppdu_info);
}

/**
 * dp_rx_mon_process_ppdu()-  Deferred monitor processing
 * This workqueue API handles:
 * a. Full monitor
 * b. Lite monitor
 *
 * @context: Opaque work context
 *
 * Return: none
 */
void dp_rx_mon_process_ppdu(void *context)
{
	struct dp_pdev *pdev = (struct dp_pdev *)context;
	struct dp_mon_pdev *mon_pdev;
	struct hal_rx_ppdu_info *ppdu_info = NULL;
	struct hal_rx_ppdu_info *temp_ppdu_info = NULL;
	struct dp_mon_pdev_be *mon_pdev_be;

	if (qdf_unlikely(!pdev)) {
		dp_mon_debug("Pdev is NULL");
		return;
	}

	mon_pdev = (struct dp_mon_pdev *)pdev->monitor_pdev;
	if (qdf_unlikely(!mon_pdev)) {
		dp_mon_debug("monitor pdev is NULL");
		return;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	qdf_spin_lock_bh(&mon_pdev_be->rx_mon_wq_lock);
	TAILQ_FOREACH_SAFE(ppdu_info,
			   &mon_pdev_be->rx_mon_queue,
			   ppdu_list_elem, temp_ppdu_info) {
		TAILQ_REMOVE(&mon_pdev_be->rx_mon_queue,
			     ppdu_info, ppdu_list_elem);

		mon_pdev_be->rx_mon_queue_depth--;
		dp_rx_mon_process_ppdu_info(pdev, ppdu_info);
		__dp_rx_mon_free_ppdu_info(mon_pdev, ppdu_info);
	}
	qdf_spin_unlock_bh(&mon_pdev_be->rx_mon_wq_lock);
}

/**
 * dp_rx_mon_add_ppdu_info_to_wq() - Add PPDU info to workqueue
 *
 * @pdev: monitor pdev
 * @ppdu_info: ppdu info to be added to workqueue
 *
 * Return: SUCCESS or FAILIRE
 */

static QDF_STATUS
dp_rx_mon_add_ppdu_info_to_wq(struct dp_pdev *pdev,
			      struct hal_rx_ppdu_info *ppdu_info)
{
	struct dp_mon_pdev *mon_pdev = (struct dp_mon_pdev *)pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
		dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	/* Full monitor or lite monitor mode is not enabled, return */
	if (!mon_pdev->monitor_configured &&
	    !dp_lite_mon_is_rx_enabled(mon_pdev))
		return QDF_STATUS_E_FAILURE;

	if (qdf_likely(ppdu_info)) {
		if (mon_pdev_be->rx_mon_queue_depth < DP_RX_MON_WQ_THRESHOLD) {
			qdf_spin_lock_bh(&mon_pdev_be->rx_mon_wq_lock);
			TAILQ_INSERT_TAIL(&mon_pdev_be->rx_mon_queue,
					  ppdu_info, ppdu_list_elem);
			mon_pdev_be->rx_mon_queue_depth++;
			mon_pdev->rx_mon_stats.total_ppdu_info_enq++;
		} else {
			mon_pdev->rx_mon_stats.total_ppdu_info_drop++;
			dp_rx_mon_free_ppdu_info(pdev, ppdu_info);
		}
		qdf_spin_unlock_bh(&mon_pdev_be->rx_mon_wq_lock);

		if (mon_pdev_be->rx_mon_queue_depth > DP_MON_QUEUE_DEPTH_MAX) {
			qdf_queue_work(0, mon_pdev_be->rx_mon_workqueue,
				       &mon_pdev_be->rx_mon_work);
		}
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_rx_mon_handle_full_mon(struct dp_pdev *pdev,
			  struct hal_rx_ppdu_info *ppdu_info,
			  qdf_nbuf_t mpdu)
{
	uint32_t wifi_hdr_len, sec_hdr_len, msdu_llc_len,
		 mpdu_buf_len, decap_hdr_pull_bytes, dir,
		 is_amsdu, amsdu_pad, frag_size, tot_msdu_len;
	struct hal_rx_mon_mpdu_info *mpdu_meta;
	struct hal_rx_mon_msdu_info *msdu_meta;
	char *hdr_desc;
	uint8_t num_frags, frag_iter, l2_hdr_offset;
	struct ieee80211_frame *wh;
	struct ieee80211_qoscntl *qos;
	uint32_t hdr_frag_size, frag_page_offset, pad_byte_pholder;
	qdf_nbuf_t head_msdu, msdu_cur;
	void *frag_addr;
	bool prev_msdu_end_received = false;
	bool is_nbuf_head = true;

	/***************************************************************************
	 *********************** Non-raw packet ************************************
	 ---------------------------------------------------------------------------
	 |      | frag-0   | frag-1    | frag - 2 | frag - 3  | frag - 4 | frag - 5  |
	 | skb  | rx_hdr-1 | rx_msdu-1 | rx_hdr-2 | rx_msdu-2 | rx_hdr-3 | rx-msdu-3 |
	 ---------------------------------------------------------------------------
	 **************************************************************************/

	if (!mpdu) {
		dp_mon_debug("nbuf is NULL, return");
		return QDF_STATUS_E_FAILURE;
	}

	head_msdu = mpdu;

	mpdu_meta = (struct hal_rx_mon_mpdu_info *)qdf_nbuf_data(mpdu);

	if (mpdu_meta->decap_type == HAL_HW_RX_DECAP_FORMAT_RAW) {
		qdf_nbuf_trim_add_frag_size(mpdu,
					    qdf_nbuf_get_nr_frags(mpdu) - 1,
					    -HAL_RX_FCS_LEN, 0);
		return QDF_STATUS_SUCCESS;
	}

	num_frags = qdf_nbuf_get_nr_frags(mpdu);
	if (qdf_unlikely(num_frags < DP_MON_MIN_FRAGS_FOR_RESTITCH)) {
		dp_mon_debug("not enough frags(%d) for restitch", num_frags);
		return QDF_STATUS_E_FAILURE;
	}

	l2_hdr_offset = DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE;

	/* hdr_desc points to 80211 hdr */
	hdr_desc = qdf_nbuf_get_frag_addr(mpdu, 0);

	/* Calculate Base header size */
	wifi_hdr_len = sizeof(struct ieee80211_frame);
	wh = (struct ieee80211_frame *)hdr_desc;

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

	if (dir == IEEE80211_FC1_DIR_DSTODS)
		wifi_hdr_len += 6;

	is_amsdu = 0;
	if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
		qos = (struct ieee80211_qoscntl *)
			(hdr_desc + wifi_hdr_len);
		wifi_hdr_len += 2;

		is_amsdu = (qos->i_qos[0] & IEEE80211_QOS_AMSDU);
	}

	/*Calculate security header length based on 'Protected'
	 * and 'EXT_IV' flag
	 */
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		char *iv = (char *)wh + wifi_hdr_len;

		if (iv[3] & KEY_EXTIV)
			sec_hdr_len = 8;
		else
			sec_hdr_len = 4;
	} else {
		sec_hdr_len = 0;
	}
	wifi_hdr_len += sec_hdr_len;

	/* MSDU related stuff LLC - AMSDU subframe header etc */
	msdu_llc_len = is_amsdu ? (DP_RX_MON_DECAP_HDR_SIZE +
				   DP_RX_MON_LLC_SIZE +
				   DP_RX_MON_SNAP_SIZE) :
				   (DP_RX_MON_LLC_SIZE + DP_RX_MON_SNAP_SIZE);

	mpdu_buf_len = wifi_hdr_len + msdu_llc_len;

	/* "Decap" header to remove from MSDU buffer */
	decap_hdr_pull_bytes = DP_RX_MON_DECAP_HDR_SIZE;

	amsdu_pad = 0;
	tot_msdu_len = 0;
	tot_msdu_len = 0;

	/*
	 * Update protocol and flow tag for MSDU
	 * update frag index in ctx_idx field.
	 * Reset head pointer data of nbuf before updating.
	 */
	QDF_NBUF_CB_RX_CTX_ID(mpdu) = 0;

	/* Construct destination address */
	hdr_frag_size = qdf_nbuf_get_frag_size_by_idx(mpdu, 0);

	/* Adjust page frag offset to point to 802.11 header */
	if (hdr_frag_size > mpdu_buf_len)
		qdf_nbuf_trim_add_frag_size(head_msdu, 0, -(hdr_frag_size - mpdu_buf_len), 0);

	msdu_meta = (struct hal_rx_mon_msdu_info *)(((void *)qdf_nbuf_get_frag_addr(mpdu, 1)) - (DP_RX_MON_PACKET_OFFSET + DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE));


	/* Adjust page frag offset to appropriate after decap header */
	frag_page_offset =
		decap_hdr_pull_bytes;
	qdf_nbuf_move_frag_page_offset(head_msdu, 1, frag_page_offset);

	frag_size = qdf_nbuf_get_frag_size_by_idx(head_msdu, 1);
	pad_byte_pholder =
		RX_MONITOR_BUFFER_SIZE - (frag_size + DP_RX_MON_PACKET_OFFSET + DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE);

	if (msdu_meta->first_buffer && msdu_meta->last_buffer) {
		/* MSDU with single buffer */
		amsdu_pad = frag_size & 0x3;
		amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;
		if (amsdu_pad && (amsdu_pad <= pad_byte_pholder)) {
			char *frag_addr_temp;

			qdf_nbuf_trim_add_frag_size(mpdu, 1, amsdu_pad, 0);
			frag_addr_temp =
				(char *)qdf_nbuf_get_frag_addr(mpdu, 1);
			frag_addr_temp = (frag_addr_temp +
					  qdf_nbuf_get_frag_size_by_idx(mpdu, 1)) -
				amsdu_pad;
			qdf_mem_zero(frag_addr_temp, amsdu_pad);
			amsdu_pad = 0;
		}
	} else {
		tot_msdu_len = frag_size;
		amsdu_pad = 0;
	}

	pad_byte_pholder = 0;
	for (msdu_cur = mpdu; msdu_cur;) {
		/* frag_iter will start from 0 for second skb onwards */
		if (msdu_cur == mpdu)
			frag_iter = 2;
		else
			frag_iter = 0;

		num_frags = qdf_nbuf_get_nr_frags(msdu_cur);

		for (; frag_iter < num_frags; frag_iter++) {
			/* Construct destination address
			 *  ----------------------------------------------------------
			 * |            | L2_HDR_PAD   |   Decap HDR | Payload | Pad  |
			 * |            | (First buffer)             |         |      |
			 * |            |                            /        /       |
			 * |            >Frag address points here   /        /        |
			 * |            \                          /        /         |
			 * |             \ This bytes needs to    /        /          |
			 * |              \  removed to frame pkt/        /           |
			 * |               ----------------------        /            |
			 * |                                     |     /     Add      |
			 * |                                     |    /   amsdu pad   |
			 * |   LLC HDR will be added here      <-|    |   Byte for    |
			 * |        |                            |    |   last frame  |
			 * |         >Dest addr will point       |    |    if space   |
			 * |            somewhere in this area   |    |    available  |
			 * |  And amsdu_pad will be created if   |    |               |
			 * | dint get added in last buffer       |    |               |
			 * |       (First Buffer)                |    |               |
			 *  ----------------------------------------------------------
			 */
			/* If previous msdu end has received, modify next frag's offset to point to LLC */
			if (prev_msdu_end_received) {
				hdr_frag_size = qdf_nbuf_get_frag_size_by_idx(msdu_cur, frag_iter);
				/* Adjust page frag offset to point to llc/snap header */
				if (hdr_frag_size > msdu_llc_len)
					qdf_nbuf_trim_add_frag_size(msdu_cur, frag_iter, -(hdr_frag_size - msdu_llc_len), 0);
				prev_msdu_end_received = false;
				continue;
			}

			frag_addr =
				qdf_nbuf_get_frag_addr(msdu_cur, frag_iter) -
						       (DP_RX_MON_PACKET_OFFSET +
						       DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE);
			msdu_meta = (struct hal_rx_mon_msdu_info *)frag_addr;

			/*
			 * Update protocol and flow tag for MSDU
			 * update frag index in ctx_idx field
			 */
			QDF_NBUF_CB_RX_CTX_ID(msdu_cur) = frag_iter;

			frag_size = qdf_nbuf_get_frag_size_by_idx(msdu_cur,
					frag_iter);

			/* If Middle buffer, dont add any header */
			if ((!msdu_meta->first_buffer) &&
					(!msdu_meta->last_buffer)) {
				tot_msdu_len += frag_size;
				amsdu_pad = 0;
				pad_byte_pholder = 0;
				continue;
			}

			/* Calculate if current buffer has placeholder
			 * to accommodate amsdu pad byte
			 */
			pad_byte_pholder =
				RX_MONITOR_BUFFER_SIZE - (frag_size + (DP_RX_MON_PACKET_OFFSET +
							  DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE));
			/*
			 * We will come here only only three condition:
			 * 1. Msdu with single Buffer
			 * 2. First buffer in case MSDU is spread in multiple
			 *    buffer
			 * 3. Last buffer in case MSDU is spread in multiple
			 *    buffer
			 *
			 *         First buffER | Last buffer
			 * Case 1:      1       |     1
			 * Case 2:      1       |     0
			 * Case 3:      0       |     1
			 *
			 * In 3rd case only l2_hdr_padding byte will be Zero and
			 * in other case, It will be 2 Bytes.
			 */
			if (msdu_meta->first_buffer)
				l2_hdr_offset =
					DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE;
			else
				l2_hdr_offset = DP_RX_MON_RAW_L2_HDR_PAD_BYTE;

			if (msdu_meta->first_buffer) {
				/* Adjust page frag offset to point to 802.11 header */
				hdr_frag_size = qdf_nbuf_get_frag_size_by_idx(msdu_cur, frag_iter-1);
				if (hdr_frag_size > (msdu_llc_len + amsdu_pad))
					qdf_nbuf_trim_add_frag_size(msdu_cur, frag_iter - 1, -(hdr_frag_size - (msdu_llc_len + amsdu_pad)), 0);

				/* Adjust page frag offset to appropriate after decap header */
				frag_page_offset =
					(decap_hdr_pull_bytes + l2_hdr_offset);
				if (frag_size > (decap_hdr_pull_bytes + l2_hdr_offset)) {
					qdf_nbuf_move_frag_page_offset(msdu_cur, frag_iter, frag_page_offset);
					frag_size = frag_size - (l2_hdr_offset + decap_hdr_pull_bytes);
				}


				/*
				 * Calculate new page offset and create hole
				 * if amsdu_pad required.
				 */
				tot_msdu_len = frag_size;
				/*
				 * No amsdu padding required for first frame of
				 * continuation buffer
				 */
				if (!msdu_meta->last_buffer) {
					amsdu_pad = 0;
					continue;
				}
			} else {
				tot_msdu_len += frag_size;
			}

			/* Will reach to this place in only two case:
			 * 1. Single buffer MSDU
			 * 2. Last buffer of MSDU in case of multiple buf MSDU
			 */

			/* This flag is used to identify msdu boundary */
			prev_msdu_end_received = true;
			/* Check size of buffer if amsdu padding required */
			amsdu_pad = tot_msdu_len & 0x3;
			amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;

			/* Create placeholder if current buffer can
			 * accommodate padding.
			 */
			if (amsdu_pad && (amsdu_pad <= pad_byte_pholder)) {
				char *frag_addr_temp;

				qdf_nbuf_trim_add_frag_size(msdu_cur,
						frag_iter,
						amsdu_pad, 0);
				frag_addr_temp = (char *)qdf_nbuf_get_frag_addr(msdu_cur,
						frag_iter);
				frag_addr_temp = (frag_addr_temp +
						qdf_nbuf_get_frag_size_by_idx(msdu_cur, frag_iter)) -
					amsdu_pad;
				qdf_mem_zero(frag_addr_temp, amsdu_pad);
				amsdu_pad = 0;
			}

			/* reset tot_msdu_len */
			tot_msdu_len = 0;
		}
		if (is_nbuf_head) {
			msdu_cur = qdf_nbuf_get_ext_list(msdu_cur);
			is_nbuf_head = false;
		} else {
			msdu_cur = qdf_nbuf_queue_next(msdu_cur);
		}
	}

	return QDF_STATUS_SUCCESS;
}

static inline int
dp_rx_mon_flush_packet_tlv(struct dp_pdev *pdev, void *buf, uint16_t end_offset,
			   union dp_mon_desc_list_elem_t **desc_list,
			   union dp_mon_desc_list_elem_t **tail)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	uint16_t work_done = 0;
	qdf_frag_t addr;
	uint8_t *rx_tlv;
	uint8_t *rx_tlv_start;
	uint16_t tlv_status = HAL_TLV_STATUS_BUF_DONE;
	struct hal_rx_ppdu_info *ppdu_info;

	if (!buf)
		return work_done;

	ppdu_info = &mon_pdev->ppdu_info;
	if (!ppdu_info) {
		dp_mon_err("ppdu_info malloc failed pdev: %pK", pdev);
		return work_done;
	}
	qdf_mem_zero(ppdu_info, sizeof(struct hal_rx_ppdu_info));
	rx_tlv = buf;
	rx_tlv_start = buf;

	do {
		tlv_status = hal_rx_status_get_tlv_info(rx_tlv,
							ppdu_info,
							pdev->soc->hal_soc,
							buf);

		if (tlv_status == HAL_TLV_STATUS_MON_BUF_ADDR) {
			struct dp_mon_desc *mon_desc = (struct dp_mon_desc *)(uintptr_t)ppdu_info->packet_info.sw_cookie;

			qdf_assert_always(mon_desc);
			addr = mon_desc->buf_addr;

			if (!mon_desc->unmapped) {
				qdf_mem_unmap_page(soc->osdev,
						   (qdf_dma_addr_t)mon_desc->paddr,
						   DP_MON_DATA_BUFFER_SIZE,
						   QDF_DMA_FROM_DEVICE);
				mon_desc->unmapped = 1;
			}
			dp_mon_add_to_free_desc_list(desc_list, tail, mon_desc);
			work_done++;

			if (addr) {
				qdf_frag_free(addr);
				DP_STATS_INC(mon_soc, frag_free, 1);
			}
		}

		rx_tlv = hal_rx_status_get_next_tlv(rx_tlv, 1);

		if ((rx_tlv - rx_tlv_start) >= (end_offset + 1))
			break;

	} while ((tlv_status == HAL_TLV_STATUS_PPDU_NOT_DONE) ||
		 (tlv_status == HAL_TLV_STATUS_HEADER) ||
		 (tlv_status == HAL_TLV_STATUS_MPDU_END) ||
		 (tlv_status == HAL_TLV_STATUS_MSDU_END) ||
		 (tlv_status == HAL_TLV_STATUS_MON_BUF_ADDR) ||
		 (tlv_status == HAL_TLV_STATUS_MPDU_START));

	return work_done;
}

/**
 * dp_rx_mon_flush_status_buf_queue() - Flush status buffer queue
 *
 * @pdev: DP pdev handle
 *
 *Return: void
 */
static inline void
dp_rx_mon_flush_status_buf_queue(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
		dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_mon_desc *mon_desc;
	uint16_t idx;
	void *buf;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	struct dp_mon_desc_pool *rx_mon_desc_pool = &mon_soc_be->rx_desc_mon;
	uint16_t work_done = 0;
	uint16_t status_buf_count;
	uint16_t end_offset = 0;

	if (!mon_pdev_be->desc_count) {
		dp_mon_info("no of status buffer count is zero: %pK", pdev);
		return;
	}

	status_buf_count = mon_pdev_be->desc_count;
	for (idx = 0; idx < status_buf_count; idx++) {
		mon_desc = mon_pdev_be->status[idx];
		if (!mon_desc) {
			qdf_assert_always(0);
			return;
		}

		buf = mon_desc->buf_addr;
		end_offset = mon_desc->end_offset;

		dp_mon_add_to_free_desc_list(&desc_list, &tail, mon_desc);
		work_done++;

		work_done += dp_rx_mon_flush_packet_tlv(pdev, buf, end_offset,
							&desc_list, &tail);

		/* set status buffer pointer to NULL */
		mon_pdev_be->status[idx] = NULL;
		mon_pdev_be->desc_count--;

		qdf_frag_free(buf);
		DP_STATS_INC(mon_soc, frag_free, 1);
	}

	if (work_done) {
		mon_pdev->rx_mon_stats.mon_rx_bufs_replenished_dest +=
			work_done;
		if (desc_list)
			dp_mon_add_desc_list_to_free_list(soc,
							  &desc_list, &tail,
							  rx_mon_desc_pool);
	}
}

/**
 * dp_rx_mon_handle_flush_n_trucated_ppdu() - Handle flush and truncated ppdu
 *
 * @soc: DP soc handle
 * @pdev: pdev handle
 * @mon_desc: mon sw desc
 */
static inline void
dp_rx_mon_handle_flush_n_trucated_ppdu(struct dp_soc *soc,
				       struct dp_pdev *pdev,
				       struct dp_mon_desc *mon_desc)
{
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
			dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	struct dp_mon_desc_pool *rx_mon_desc_pool = &mon_soc_be->rx_desc_mon;
	uint16_t work_done;
	void *buf;
	uint16_t end_offset = 0;

	/* Flush status buffers in queue */
	dp_rx_mon_flush_status_buf_queue(pdev);
	buf = mon_desc->buf_addr;
	end_offset = mon_desc->end_offset;
	qdf_frag_free(mon_desc->buf_addr);
	DP_STATS_INC(mon_soc, frag_free, 1);
	dp_mon_add_to_free_desc_list(&desc_list, &tail, mon_desc);
	work_done = 1;
	work_done += dp_rx_mon_flush_packet_tlv(pdev, buf, end_offset,
						&desc_list, &tail);
	if (desc_list)
		dp_mon_add_desc_list_to_free_list(soc, &desc_list, &tail,
						  rx_mon_desc_pool);
}

uint8_t dp_rx_mon_process_tlv_status(struct dp_pdev *pdev,
				     struct hal_rx_ppdu_info *ppdu_info,
				     void *status_frag,
				     uint16_t tlv_status,
				     union dp_mon_desc_list_elem_t **desc_list,
				     union dp_mon_desc_list_elem_t **tail)
{
	struct dp_soc *soc  = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	qdf_nbuf_t nbuf, tmp_nbuf;
	qdf_frag_t addr;
	uint8_t user_id = ppdu_info->user_id;
	uint8_t mpdu_idx = ppdu_info->mpdu_count[user_id];
	uint16_t num_frags;
	uint8_t num_buf_reaped = 0;
	QDF_STATUS status;

	if (!mon_pdev->monitor_configured &&
	    !dp_lite_mon_is_rx_enabled(mon_pdev)) {
		return num_buf_reaped;
	}

	switch (tlv_status) {
	case HAL_TLV_STATUS_HEADER: {
		/* If this is first RX_HEADER for MPDU, allocate skb
		 * else add frag to already allocated skb
		 */

		if (!ppdu_info->mpdu_info[user_id].mpdu_start_received) {

			nbuf = qdf_nbuf_alloc(pdev->soc->osdev,
					      DP_RX_MON_TLV_ROOM +
					      DP_RX_MON_MAX_RADIO_TAP_HDR,
					      DP_RX_MON_TLV_ROOM +
					      DP_RX_MON_MAX_RADIO_TAP_HDR,
					      4, FALSE);

			/* Set *head_msdu->next as NULL as all msdus are
			 *                          * mapped via nr frags
			 *                                                   */
			if (qdf_unlikely(!nbuf)) {
				dp_mon_err("malloc failed pdev: %pK ", pdev);
				return num_buf_reaped;
			}

			mon_pdev->rx_mon_stats.parent_buf_alloc++;

			dp_rx_mon_set_zero(nbuf);

			qdf_nbuf_set_next(nbuf, NULL);

			qdf_nbuf_queue_add(&ppdu_info->mpdu_q[user_id], nbuf);

			status = dp_rx_mon_nbuf_add_rx_frag(nbuf, status_frag,
							    ppdu_info->hdr_len - DP_RX_MON_RX_HDR_OFFSET,
							    ppdu_info->data - (unsigned char *)status_frag + 4,
							    DP_MON_DATA_BUFFER_SIZE, true);
			if (qdf_unlikely(status != QDF_STATUS_SUCCESS)) {
				dp_mon_err("num_frags exceeding MAX frags");
				qdf_assert_always(0);
			}
			ppdu_info->mpdu_info[ppdu_info->user_id].mpdu_start_received = true;
			ppdu_info->mpdu_info[user_id].first_rx_hdr_rcvd = true;
			/* initialize decap type to invalid, this will be set to appropriate
			 * value once the mpdu start tlv is received
			 */
			ppdu_info->mpdu_info[user_id].decap_type = DP_MON_DECAP_FORMAT_INVALID;
		} else {
			if (ppdu_info->mpdu_info[user_id].decap_type ==
					HAL_HW_RX_DECAP_FORMAT_RAW) {
				return num_buf_reaped;
			}

			if (dp_lite_mon_is_rx_enabled(mon_pdev) &&
			    !dp_lite_mon_is_level_msdu(mon_pdev))
				break;

			nbuf = qdf_nbuf_queue_last(&ppdu_info->mpdu_q[user_id]);
			if (qdf_unlikely(!nbuf)) {
				dp_mon_debug("nbuf is NULL");
				return num_buf_reaped;
			}

			tmp_nbuf = qdf_get_nbuf_valid_frag(nbuf);

			if (!tmp_nbuf) {
				tmp_nbuf = qdf_nbuf_alloc(pdev->soc->osdev,
							  DP_RX_MON_MAX_MONITOR_HEADER,
							  DP_RX_MON_MAX_MONITOR_HEADER,
							  4, FALSE);
				if (qdf_unlikely(!tmp_nbuf)) {
					dp_mon_err("nbuf is NULL");
					qdf_assert_always(0);
				}
				mon_pdev->rx_mon_stats.parent_buf_alloc++;
				/* add new skb to frag list */
				qdf_nbuf_append_ext_list(nbuf, tmp_nbuf,
							 qdf_nbuf_len(tmp_nbuf));
			}
			dp_rx_mon_nbuf_add_rx_frag(tmp_nbuf, status_frag,
						   ppdu_info->hdr_len - DP_RX_MON_RX_HDR_OFFSET,
						   ppdu_info->data - (unsigned char *)status_frag + 4,
						   DP_MON_DATA_BUFFER_SIZE,
						   true);
		}
		ppdu_info->rx_hdr_rcvd[user_id] = true;
	}
	break;
	case HAL_TLV_STATUS_MON_BUF_ADDR:
	{
		struct hal_rx_mon_msdu_info *buf_info;
		struct hal_mon_packet_info *packet_info = &ppdu_info->packet_info;
		struct dp_mon_desc *mon_desc = (struct dp_mon_desc *)(uintptr_t)ppdu_info->packet_info.sw_cookie;
		struct hal_rx_mon_mpdu_info *mpdu_info;
		uint16_t frag_idx = 0;

		qdf_assert_always(mon_desc);

		if (mon_desc->magic != DP_MON_DESC_MAGIC)
			qdf_assert_always(0);

		addr = mon_desc->buf_addr;
		qdf_assert_always(addr);

		mpdu_info = &ppdu_info->mpdu_info[user_id];
		if (!mon_desc->unmapped) {
			qdf_mem_unmap_page(soc->osdev,
					   (qdf_dma_addr_t)mon_desc->paddr,
				   DP_MON_DATA_BUFFER_SIZE,
					   QDF_DMA_FROM_DEVICE);
			mon_desc->unmapped = 1;
		}
		dp_mon_add_to_free_desc_list(desc_list, tail, mon_desc);
		num_buf_reaped++;

		mon_pdev->rx_mon_stats.pkt_buf_count++;

		if (qdf_unlikely(!ppdu_info->rx_hdr_rcvd[user_id])) {

			/* WAR: RX_HDR is not received for this MPDU, drop this frame */
			mon_pdev->rx_mon_stats.rx_hdr_not_received++;
			DP_STATS_INC(mon_soc, frag_free, 1);
			qdf_frag_free(addr);
			return num_buf_reaped;
		}

		nbuf = qdf_nbuf_queue_last(&ppdu_info->mpdu_q[user_id]);
		if (qdf_unlikely(!nbuf)) {
			dp_mon_debug("nbuf is NULL");
			return num_buf_reaped;
		}

		if (mpdu_info->decap_type == DP_MON_DECAP_FORMAT_INVALID) {
			/* decap type is invalid, drop the frame */
			mon_pdev->rx_mon_stats.mpdu_decap_type_invalid++;
			DP_STATS_INC(mon_soc, frag_free, 1);
			mon_pdev->rx_mon_stats.parent_buf_free++;
			qdf_frag_free(addr);
			qdf_nbuf_queue_remove_last(&ppdu_info->mpdu_q[user_id]);
			qdf_nbuf_free(nbuf);
			/* if invalid decap type handling is disabled, assert */
			if (soc->wlan_cfg_ctx->is_handle_invalid_decap_type_disabled) {
				dp_mon_err("Decap type invalid");
				qdf_assert_always(0);
			}
			return num_buf_reaped;
		}

		tmp_nbuf = qdf_get_nbuf_valid_frag(nbuf);

		if (!tmp_nbuf) {
			tmp_nbuf = qdf_nbuf_alloc(pdev->soc->osdev,
						  DP_RX_MON_MAX_MONITOR_HEADER,
						  DP_RX_MON_MAX_MONITOR_HEADER,
						  4, FALSE);
			if (qdf_unlikely(!tmp_nbuf)) {
				dp_mon_err("nbuf is NULL");
				DP_STATS_INC(mon_soc, frag_free, 1);
				mon_pdev->rx_mon_stats.parent_buf_free++;
				qdf_frag_free(addr);
				/* remove this nbuf from queue */
				qdf_nbuf_queue_remove_last(&ppdu_info->mpdu_q[user_id]);
				qdf_nbuf_free(nbuf);
				return num_buf_reaped;
			}
			mon_pdev->rx_mon_stats.parent_buf_alloc++;
			/* add new skb to frag list */
			qdf_nbuf_append_ext_list(nbuf, tmp_nbuf,
						 qdf_nbuf_len(tmp_nbuf));
		}
		mpdu_info->full_pkt = true;

		if (mpdu_info->decap_type == HAL_HW_RX_DECAP_FORMAT_RAW) {
			if (mpdu_info->first_rx_hdr_rcvd) {
				qdf_nbuf_remove_frag(nbuf, frag_idx, DP_MON_DATA_BUFFER_SIZE);
				dp_rx_mon_nbuf_add_rx_frag(nbuf, addr,
							   packet_info->dma_length,
							   DP_RX_MON_PACKET_OFFSET,
							   DP_MON_DATA_BUFFER_SIZE,
							   false);
				DP_STATS_INC(mon_soc, frag_free, 1);
				mpdu_info->first_rx_hdr_rcvd = false;
			} else {
				dp_rx_mon_nbuf_add_rx_frag(tmp_nbuf, addr,
							   packet_info->dma_length,
							   DP_RX_MON_PACKET_OFFSET,
							   DP_MON_DATA_BUFFER_SIZE,
							   false);
				DP_STATS_INC(mon_soc, frag_free, 1);
			}
		} else {
			dp_rx_mon_nbuf_add_rx_frag(tmp_nbuf, addr,
						   packet_info->dma_length,
						   DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE +
						   DP_RX_MON_PACKET_OFFSET,
						   DP_MON_DATA_BUFFER_SIZE,
						   false);
				DP_STATS_INC(mon_soc, frag_free, 1);
			buf_info = addr;

			if (!ppdu_info->msdu[user_id].first_buffer) {
				buf_info->first_buffer = true;
				ppdu_info->msdu[user_id].first_buffer = true;
			} else {
				buf_info->first_buffer = false;
			}

			if (packet_info->msdu_continuation)
				buf_info->last_buffer = false;
			else
				buf_info->last_buffer = true;

			buf_info->frag_len = packet_info->dma_length;
		}
		if (qdf_unlikely(packet_info->truncated))
			mpdu_info->truncated = true;
	}
	break;
	case HAL_TLV_STATUS_MSDU_END:
	{
		struct hal_rx_mon_msdu_info *msdu_info = &ppdu_info->msdu[user_id];
		struct hal_rx_mon_msdu_info *last_buf_info;
		/* update msdu metadata at last buffer of msdu in MPDU */
		if (qdf_unlikely(!ppdu_info->rx_hdr_rcvd[user_id])) {
			/* reset msdu info for next msdu for same user */
			qdf_mem_zero(msdu_info, sizeof(*msdu_info));
			dp_mon_debug(" <%d> nbuf is NULL, return user: %d mpdu_idx: %d",
				     __LINE__, user_id, mpdu_idx);
			break;
		}
		nbuf = qdf_nbuf_queue_last(&ppdu_info->mpdu_q[user_id]);
		if (qdf_unlikely(!nbuf)) {
			dp_mon_debug("nbuf is NULL");
			break;
		}
		num_frags = qdf_nbuf_get_nr_frags(nbuf);
		if (ppdu_info->mpdu_info[user_id].decap_type ==
				HAL_HW_RX_DECAP_FORMAT_RAW) {
			break;
		}
		/* This points to last buffer of MSDU . update metadata here */
		addr = qdf_nbuf_get_frag_addr(nbuf, num_frags - 1) -
					      (DP_RX_MON_PACKET_OFFSET +
					       DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE);
		last_buf_info = addr;

		last_buf_info->first_msdu = msdu_info->first_msdu;
		last_buf_info->last_msdu = msdu_info->last_msdu;
		last_buf_info->decap_type = msdu_info->decap_type;
		last_buf_info->msdu_index = msdu_info->msdu_index;
		last_buf_info->user_rssi = msdu_info->user_rssi;
		last_buf_info->reception_type = msdu_info->reception_type;
		last_buf_info->msdu_len = msdu_info->msdu_len;

		/* If flow classification is enabled,
		 * update protocol and flow tag to buf headroom
		 */
		dp_rx_mon_pf_tag_to_buf_headroom_2_0(nbuf, ppdu_info, pdev,
						     soc);

		/* reset msdu info for next msdu for same user */
		qdf_mem_zero(msdu_info, sizeof(*msdu_info));
	}
	break;
	case HAL_TLV_STATUS_MPDU_START:
	{
		struct hal_rx_mon_mpdu_info *mpdu_info, *mpdu_meta;

		if (qdf_unlikely(!ppdu_info->rx_hdr_rcvd[user_id])) {
			dp_mon_debug(" <%d> nbuf is NULL, return user: %d mpdu_idx: %d", __LINE__, user_id, mpdu_idx);
			break;
		}
		nbuf = qdf_nbuf_queue_last(&ppdu_info->mpdu_q[user_id]);
		if (qdf_unlikely(!nbuf)) {
			dp_mon_debug("nbuf is NULL");
			break;
		}
		mpdu_meta = (struct hal_rx_mon_mpdu_info *)qdf_nbuf_data(nbuf);
		mpdu_info = &ppdu_info->mpdu_info[user_id];
		mpdu_meta->decap_type = mpdu_info->decap_type;
		ppdu_info->mpdu_info[ppdu_info->user_id].mpdu_start_received = true;
	break;
	}
	case HAL_TLV_STATUS_MPDU_END:
	{
		struct hal_rx_mon_mpdu_info *mpdu_info, *mpdu_meta;
		mpdu_info = &ppdu_info->mpdu_info[user_id];
		if (qdf_unlikely(!ppdu_info->rx_hdr_rcvd[user_id])) {
			/* reset mpdu info for next mpdu for same user */
			qdf_mem_zero(mpdu_info, sizeof(*mpdu_info));
			dp_mon_debug(" <%d> nbuf is NULL, return user: %d mpdu_idx: %d",
				     __LINE__, user_id, mpdu_idx);
			break;
		}
		nbuf = qdf_nbuf_queue_last(&ppdu_info->mpdu_q[user_id]);
		if (qdf_unlikely(!nbuf)) {
			dp_mon_debug("nbuf is NULL");
			break;
		}
		mpdu_meta = (struct hal_rx_mon_mpdu_info *)qdf_nbuf_data(nbuf);
		mpdu_meta->mpdu_length_err = mpdu_info->mpdu_length_err;
		mpdu_meta->fcs_err = mpdu_info->fcs_err;
		ppdu_info->rx_status.rs_fcs_err = mpdu_info->fcs_err;
		mpdu_meta->overflow_err = mpdu_info->overflow_err;
		mpdu_meta->decrypt_err = mpdu_info->decrypt_err;
		mpdu_meta->full_pkt = mpdu_info->full_pkt;
		mpdu_meta->truncated = mpdu_info->truncated;

		/* reset mpdu info for next mpdu for same user */
		qdf_mem_zero(mpdu_info, sizeof(*mpdu_info));
		ppdu_info->mpdu_info[ppdu_info->user_id].mpdu_start_received = false;
		ppdu_info->mpdu_count[user_id]++;
		ppdu_info->rx_hdr_rcvd[user_id] = false;
	}
	break;
	case HAL_TLV_STATUS_MON_DROP:
	{
		mon_pdev->rx_mon_stats.ppdu_drop_cnt +=
			ppdu_info->drop_cnt.ppdu_drop_cnt;
		mon_pdev->rx_mon_stats.mpdu_drop_cnt +=
			ppdu_info->drop_cnt.mpdu_drop_cnt;
		mon_pdev->rx_mon_stats.end_of_ppdu_drop_cnt +=
			ppdu_info->drop_cnt.end_of_ppdu_drop_cnt;
		mon_pdev->rx_mon_stats.tlv_drop_cnt +=
			ppdu_info->drop_cnt.tlv_drop_cnt;
	}
	break;
	}
	return num_buf_reaped;
}

/**
 * dp_rx_mon_process_status_tlv() - Handle mon status process TLV
 *
 * @pdev: DP pdev handle
 *
 * Return
 */
static inline struct hal_rx_ppdu_info *
dp_rx_mon_process_status_tlv(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_mon_desc *mon_desc;
	uint8_t user;
	uint16_t idx;
	void *buf;
	struct hal_rx_ppdu_info *ppdu_info;
	uint8_t *rx_tlv;
	uint8_t *rx_tlv_start;
	uint16_t end_offset = 0;
	uint16_t tlv_status = HAL_TLV_STATUS_BUF_DONE;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	struct dp_mon_desc_pool *rx_mon_desc_pool = &mon_soc_be->rx_desc_mon;
	uint16_t work_done = 0;
	uint16_t status_buf_count;

	if (!mon_pdev_be->desc_count) {
		dp_mon_err("no of status buffer count is zero: %pK", pdev);
		return NULL;
	}

	ppdu_info = dp_rx_mon_get_ppdu_info(mon_pdev);

	if (!ppdu_info) {
		dp_mon_err("ppdu_info malloc failed pdev: %pK", pdev);
		dp_rx_mon_flush_status_buf_queue(pdev);
		return NULL;
	}

	qdf_mem_zero(ppdu_info, sizeof(struct hal_rx_ppdu_info));
	mon_pdev->rx_mon_stats.total_ppdu_info_alloc++;

	for (user = 0; user < HAL_MAX_UL_MU_USERS; user++)
		qdf_nbuf_queue_init(&ppdu_info->mpdu_q[user]);

	status_buf_count = mon_pdev_be->desc_count;
	for (idx = 0; idx < status_buf_count; idx++) {
		mon_desc = mon_pdev_be->status[idx];
		if (!mon_desc) {
			qdf_assert_always(0);
			return NULL;
		}

		buf = mon_desc->buf_addr;
		end_offset = mon_desc->end_offset;

		dp_mon_add_to_free_desc_list(&desc_list, &tail, mon_desc);
		work_done++;

		rx_tlv = buf;
		rx_tlv_start = buf;

		do {
			tlv_status = hal_rx_status_get_tlv_info(rx_tlv,
								ppdu_info,
								pdev->soc->hal_soc,
								buf);

			work_done += dp_rx_mon_process_tlv_status(pdev,
								  ppdu_info,
								  buf,
								  tlv_status,
								  &desc_list,
								  &tail);
			rx_tlv = hal_rx_status_get_next_tlv(rx_tlv, 1);

			/* HW provides end_offset (how many bytes HW DMA'ed)
			 * as part of descriptor, use this as delimiter for
			 * status buffer
			 */
			if ((rx_tlv - rx_tlv_start) >= (end_offset + 1))
				break;

	} while ((tlv_status == HAL_TLV_STATUS_PPDU_NOT_DONE) ||
			(tlv_status == HAL_TLV_STATUS_HEADER) ||
			(tlv_status == HAL_TLV_STATUS_MPDU_END) ||
			(tlv_status == HAL_TLV_STATUS_MSDU_END) ||
			(tlv_status == HAL_TLV_STATUS_MON_BUF_ADDR) ||
			(tlv_status == HAL_TLV_STATUS_MPDU_START));

		/* set status buffer pointer to NULL */
		mon_pdev_be->status[idx] = NULL;
		mon_pdev_be->desc_count--;

		qdf_frag_free(buf);
		DP_STATS_INC(mon_soc, frag_free, 1);
		mon_pdev->rx_mon_stats.status_buf_count++;
	}

	dp_mon_rx_stats_update_rssi_dbm_params(mon_pdev, ppdu_info);
	if (work_done) {
		mon_pdev->rx_mon_stats.mon_rx_bufs_replenished_dest +=
			work_done;
		if (desc_list)
			dp_mon_add_desc_list_to_free_list(soc,
							  &desc_list, &tail,
							  rx_mon_desc_pool);
	}

	ppdu_info->rx_status.tsft = ppdu_info->rx_status.tsft +
				    pdev->timestamp.mlo_offset_lo_us +
				    ((uint64_t)pdev->timestamp.mlo_offset_hi_us
				    << 32);

	return ppdu_info;
}

#ifdef WLAN_FEATURE_11BE_MLO
#define DP_PEER_ID_MASK 0x3FFF
/**
 * dp_rx_mon_update_peer_id() - Update sw_peer_id with link peer_id
 *
 * @pdev: DP pdev handle
 * @ppdu_info: HAL PPDU Info buffer
 *
 * Return: none
 */
static inline
void dp_rx_mon_update_peer_id(struct dp_pdev *pdev,
			      struct hal_rx_ppdu_info *ppdu_info)
{
	uint32_t i;
	uint16_t peer_id;
	struct dp_soc *soc = pdev->soc;
	uint32_t num_users = ppdu_info->com_info.num_users;

	for (i = 0; i < num_users; i++) {
		peer_id = ppdu_info->rx_user_status[i].sw_peer_id;
		if (peer_id == HTT_INVALID_PEER)
			continue;
		/*
		+---------------------------------------------------------------------+
		| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
		+---------------------------------------------------------------------+
		| CHIP ID | ML |                     PEER ID                          |
		+---------------------------------------------------------------------+
		*/
		peer_id &= DP_PEER_ID_MASK;
		peer_id = dp_get_link_peer_id_by_lmac_id(soc, peer_id,
							 pdev->lmac_id);
		ppdu_info->rx_user_status[i].sw_peer_id = peer_id;
	}
}
#else
static inline
void dp_rx_mon_update_peer_id(struct dp_pdev *pdev,
			      struct hal_rx_ppdu_info *ppdu_info)
{
}
#endif

/*
 * HAL_RX_PKT_TYPE_11A     0 -> CDP_PKT_TYPE_OFDM
 * HAL_RX_PKT_TYPE_11B     1 -> CDP_PKT_TYPE_CCK
 * HAL_RX_PKT_TYPE_11N     2 -> CDP_PKT_TYPE_HT
 * HAL_RX_PKT_TYPE_11AC    3 -> CDP_PKT_TYPE_VHT
 * HAL_RX_PKT_TYPE_11AX    4 -> CDP_PKT_TYPE_HE
 * HAL_RX_PKT_TYPE_11BE    6 -> CDP_PKT_TYPE_EHT
 */

static uint32_t const cdp_preamble_type_map[] = {
	CDP_PKT_TYPE_OFDM,
	CDP_PKT_TYPE_CCK,
	CDP_PKT_TYPE_HT,
	CDP_PKT_TYPE_VHT,
	CDP_PKT_TYPE_HE,
	CDP_PKT_TYPE_NO_SUP,
#ifdef WLAN_FEATURE_11BE
	CDP_PKT_TYPE_EHT,
#endif
	CDP_PKT_TYPE_MAX,
};

/*
 * HAL_RX_RECEPTION_TYPE_SU       -> CDP_RX_TYPE_SU
 * HAL_RX_RECEPTION_TYPE_MU_MIMO  -> CDP_RX_TYPE_MU_MIMO
 * HAL_RX_RECEPTION_TYPE_OFDMA    -> CDP_RX_TYPE_MU_OFDMA
 * HAL_RX_RECEPTION_TYPE_MU_OFDMA -> CDP_RX_TYPE_MU_OFDMA_MIMO
 */
static uint32_t const cdp_reception_type_map[] = {
	CDP_RX_TYPE_SU,
	CDP_RX_TYPE_MU_MIMO,
	CDP_RX_TYPE_MU_OFDMA,
	CDP_RX_TYPE_MU_OFDMA_MIMO,
};

static uint32_t const cdp_mu_dl_up_map[] = {
	CDP_MU_TYPE_DL,
	CDP_MU_TYPE_UL,
};

static inline void
dp_rx_mu_stats_update(
	struct hal_rx_ppdu_info *ppdu_info,
	struct cdp_pdev_mon_stats *rx_mon_sts,
	uint32_t preamble_type,
	uint32_t  recept_type,
	uint32_t  mu_dl_ul,
	uint32_t i
)
{
	struct mon_rx_user_status *rx_user_status;

	rx_user_status =  &ppdu_info->rx_user_status[i];
	rx_mon_sts->mpdu_cnt_fcs_ok[preamble_type][recept_type][mu_dl_ul][i]
			+= rx_user_status->mpdu_cnt_fcs_ok;
	rx_mon_sts->mpdu_cnt_fcs_err[preamble_type][recept_type][mu_dl_ul][i]
			+= rx_user_status->mpdu_cnt_fcs_err;
}

static inline void
dp_rx_he_ppdu_stats_update(
	struct cdp_pdev_mon_stats *stats,
	struct hal_rx_u_sig_info *u_sig
)
{
	stats->ppdu_eht_type_mode[u_sig->ppdu_type_comp_mode][u_sig->ul_dl]++;
}

static inline void
dp_rx_he_ppdu_stats(struct dp_pdev *pdev, struct hal_rx_ppdu_info *ppdu_info)
{
	struct dp_mon_pdev *mon_pdev;
	struct cdp_pdev_mon_stats *rx_mon_stats;

	mon_pdev = pdev->monitor_pdev;
	rx_mon_stats = &mon_pdev->rx_mon_stats;

	if (ppdu_info->u_sig_info.ppdu_type_comp_mode < CDP_EHT_TYPE_MODE_MAX &&
	    ppdu_info->u_sig_info.ul_dl < CDP_MU_TYPE_MAX)
		dp_rx_he_ppdu_stats_update(
			rx_mon_stats,
			&ppdu_info->u_sig_info);
		else
			qdf_assert(0);
}

static inline void
dp_rx_mu_stats(struct dp_pdev *pdev, struct hal_rx_ppdu_info *ppdu_info)
{
	struct dp_mon_pdev *mon_pdev;
	struct cdp_pdev_mon_stats *rx_mon_stats;
	struct mon_rx_status *rx_status;
	uint32_t preamble_type, reception_type, mu_dl_ul, num_users, i;

	mon_pdev = pdev->monitor_pdev;
	rx_mon_stats = &mon_pdev->rx_mon_stats;
	rx_status = &ppdu_info->rx_status;

	num_users = ppdu_info->com_info.num_users;

	if (rx_status->preamble_type < CDP_PKT_TYPE_MAX)
		preamble_type = cdp_preamble_type_map[rx_status->preamble_type];
	else
		preamble_type = CDP_PKT_TYPE_NO_SUP;

	reception_type = cdp_reception_type_map[rx_status->reception_type];
	mu_dl_ul = cdp_mu_dl_up_map[rx_status->mu_dl_ul];

	for (i = 0; i < num_users; i++) {
		if (i >= CDP_MU_SNIF_USER_MAX)
			return;

		dp_rx_mu_stats_update(ppdu_info, rx_mon_stats, preamble_type,
				      reception_type, mu_dl_ul, i);
	}

	if (rx_status->eht_flags)
		dp_rx_he_ppdu_stats(pdev, ppdu_info);
}

static inline uint32_t
dp_rx_mon_srng_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
			   uint32_t mac_id, uint32_t quota)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	struct dp_mon_desc_pool *rx_mon_desc_pool = &mon_soc_be->rx_desc_mon;
	hal_soc_handle_t hal_soc = soc->hal_soc;
	void *rx_mon_dst_ring_desc;
	void *mon_dst_srng;
	uint32_t work_done = 0;
	struct hal_rx_ppdu_info *ppdu_info = NULL;
	QDF_STATUS status;
	if (!pdev) {
		dp_mon_err("%pK: pdev is null for mac_id = %d", soc, mac_id);
		return work_done;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	mon_dst_srng = soc->rxdma_mon_dst_ring[mac_id].hal_srng;

	if (!mon_dst_srng || !hal_srng_initialized(mon_dst_srng)) {
		dp_mon_err("%pK: : HAL Monitor Destination Ring Init Failed -- %pK",
			   soc, mon_dst_srng);
		return work_done;
	}

	hal_soc = soc->hal_soc;

	qdf_assert((hal_soc && pdev));

	qdf_spin_lock_bh(&mon_pdev->mon_lock);

	if (qdf_unlikely(dp_srng_access_start(int_ctx, soc, mon_dst_srng))) {
		dp_mon_err("%s %d : HAL Mon Dest Ring access Failed -- %pK",
			   __func__, __LINE__, mon_dst_srng);
		qdf_spin_unlock_bh(&mon_pdev->mon_lock);
		return work_done;
	}

	while (qdf_likely((rx_mon_dst_ring_desc =
			  (void *)hal_srng_dst_peek(hal_soc, mon_dst_srng))
				&& quota--)) {
		struct hal_mon_desc hal_mon_rx_desc = {0};
		struct dp_mon_desc *mon_desc;
		hal_be_get_mon_dest_status(soc->hal_soc,
					   rx_mon_dst_ring_desc,
					   &hal_mon_rx_desc);
		/* If it's empty descriptor, skip processing
		 * and process next hW desc
		 */
		if (hal_mon_rx_desc.empty_descriptor == 1) {
			dp_mon_debug("empty descriptor found mon_pdev: %pK",
				     mon_pdev);
			rx_mon_dst_ring_desc =
				hal_srng_dst_get_next(hal_soc, mon_dst_srng);
			dp_rx_mon_update_drop_cnt(mon_pdev, &hal_mon_rx_desc);
			continue;
		}
		mon_desc = (struct dp_mon_desc *)(uintptr_t)(hal_mon_rx_desc.buf_addr);
		qdf_assert_always(mon_desc);

		if ((mon_desc == mon_pdev_be->prev_rxmon_desc) &&
		    (mon_desc->cookie == mon_pdev_be->prev_rxmon_cookie)) {
			dp_mon_err("duplicate descritout found mon_pdev: %pK mon_desc: %pK cookie: %d",
				   mon_pdev, mon_desc, mon_desc->cookie);
			mon_pdev->rx_mon_stats.dup_mon_buf_cnt++;
			hal_srng_dst_get_next(hal_soc, mon_dst_srng);
			continue;
		}
		mon_pdev_be->prev_rxmon_desc = mon_desc;
		mon_pdev_be->prev_rxmon_cookie = mon_desc->cookie;

		if (!mon_desc->unmapped) {
			qdf_mem_unmap_page(soc->osdev, mon_desc->paddr,
					   rx_mon_desc_pool->buf_size,
					   QDF_DMA_FROM_DEVICE);
			mon_desc->unmapped = 1;
		}
		mon_desc->end_offset = hal_mon_rx_desc.end_offset;

		/* Flush and truncated status buffers content
		 * need to discarded
		 */
		if (hal_mon_rx_desc.end_reason == HAL_MON_FLUSH_DETECTED ||
		    hal_mon_rx_desc.end_reason == HAL_MON_PPDU_TRUNCATED) {
			dp_mon_debug("end_resaon: %d mon_pdev: %pK",
				     hal_mon_rx_desc.end_reason, mon_pdev);
			mon_pdev->rx_mon_stats.status_ppdu_drop++;
			dp_rx_mon_handle_flush_n_trucated_ppdu(soc,
							       pdev,
							       mon_desc);
			rx_mon_dst_ring_desc = hal_srng_dst_get_next(hal_soc,
							mon_dst_srng);
			continue;
		}
		if (mon_pdev_be->desc_count >= DP_MON_MAX_STATUS_BUF)
			qdf_assert_always(0);

		mon_pdev_be->status[mon_pdev_be->desc_count++] = mon_desc;

		rx_mon_dst_ring_desc = hal_srng_dst_get_next(hal_soc, mon_dst_srng);

		dp_rx_process_pktlog_be(soc, pdev, ppdu_info,
					mon_desc->buf_addr,
					hal_mon_rx_desc.end_offset);

		if (hal_mon_rx_desc.end_reason == HAL_MON_STATUS_BUFFER_FULL)
			continue;

		mon_pdev->rx_mon_stats.status_ppdu_done++;

		ppdu_info = dp_rx_mon_process_status_tlv(pdev);

		if (ppdu_info) {
			mon_pdev->rx_mon_stats.start_user_info_cnt +=
				ppdu_info->start_user_info_cnt;
			ppdu_info->start_user_info_cnt = 0;

			mon_pdev->rx_mon_stats.end_user_stats_cnt +=
				ppdu_info->end_user_stats_cnt;
			ppdu_info->end_user_stats_cnt = 0;

			dp_rx_mon_update_peer_id(pdev, ppdu_info);
			dp_rx_mu_stats(pdev, ppdu_info);
		}

		/* Call enhanced stats update API */
		if (mon_pdev->enhanced_stats_en && ppdu_info)
			dp_rx_handle_ppdu_stats(soc, pdev, ppdu_info);
		else if (dp_cfr_rcc_mode_status(pdev) && ppdu_info)
			dp_rx_handle_cfr(soc, pdev, ppdu_info);

		dp_rx_mon_update_user_ctrl_frame_stats(pdev, ppdu_info);

		status = dp_rx_mon_add_ppdu_info_to_wq(pdev, ppdu_info);
		if (status != QDF_STATUS_SUCCESS) {
			if (ppdu_info)
				__dp_rx_mon_free_ppdu_info(mon_pdev, ppdu_info);
		}

		work_done++;

		/* desc_count should be zero  after PPDU status processing */
		if (mon_pdev_be->desc_count > 0)
			qdf_assert_always(0);

		mon_pdev_be->desc_count = 0;
	}
	dp_srng_access_end(int_ctx, soc, mon_dst_srng);

	qdf_spin_unlock_bh(&mon_pdev->mon_lock);
	dp_mon_info("mac_id: %d, work_done:%d", mac_id, work_done);
	return work_done;
}

uint32_t
dp_rx_mon_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
		      uint32_t mac_id, uint32_t quota)
{
	uint32_t work_done;

	work_done = dp_rx_mon_srng_process_2_0(soc, int_ctx, mac_id, quota);

	return work_done;
}

void
dp_rx_mon_buf_desc_pool_deinit(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	/* Drain page frag cachce before pool deinit */
	qdf_frag_cache_drain(&mon_soc_be->rx_desc_mon.pf_cache);
	dp_mon_desc_pool_deinit(&mon_soc_be->rx_desc_mon);
}

QDF_STATUS
dp_rx_mon_buf_desc_pool_init(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	uint32_t num_entries;

	num_entries =
		wlan_cfg_get_dp_soc_rx_mon_buf_ring_size(soc->wlan_cfg_ctx);
	return dp_mon_desc_pool_init(&mon_soc_be->rx_desc_mon, num_entries);
}

void dp_rx_mon_buf_desc_pool_free(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	if (mon_soc)
		dp_mon_desc_pool_free(&mon_soc_be->rx_desc_mon);
}

QDF_STATUS
dp_rx_mon_buf_desc_pool_alloc(struct dp_soc *soc)
{
	struct dp_mon_desc_pool *rx_mon_desc_pool;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	int entries;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	entries = wlan_cfg_get_dp_soc_rx_mon_buf_ring_size(soc_cfg_ctx);

	rx_mon_desc_pool = &mon_soc_be->rx_desc_mon;

	qdf_print("%s:%d rx mon buf desc pool entries: %d", __func__, __LINE__, entries);
	return dp_mon_desc_pool_alloc(entries, rx_mon_desc_pool);
}

void
dp_rx_mon_buffers_free(struct dp_soc *soc)
{
	struct dp_mon_desc_pool *rx_mon_desc_pool;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	rx_mon_desc_pool = &mon_soc_be->rx_desc_mon;

	dp_mon_pool_frag_unmap_and_free(soc, rx_mon_desc_pool);
}

QDF_STATUS
dp_rx_mon_buffers_alloc(struct dp_soc *soc, uint32_t size)
{
	struct dp_srng *mon_buf_ring;
	struct dp_mon_desc_pool *rx_mon_desc_pool;
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	mon_buf_ring = &soc->rxdma_mon_buf_ring[0];

	rx_mon_desc_pool = &mon_soc_be->rx_desc_mon;

	return dp_mon_buffers_replenish(soc, mon_buf_ring,
					rx_mon_desc_pool,
					size,
					&desc_list, &tail, NULL);
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
void
dp_rx_mon_populate_ppdu_usr_info_2_0(struct mon_rx_user_status *rx_user_status,
				     struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	ppdu_user->mpdu_retries = rx_user_status->retry_mpdu;
}

#ifdef WLAN_FEATURE_11BE
void dp_rx_mon_stats_update_2_0(struct dp_mon_peer *mon_peer,
				struct cdp_rx_indication_ppdu *ppdu,
				struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	uint8_t mcs, preamble, ppdu_type, punc_mode;
	uint32_t num_msdu;

	preamble = ppdu->u.preamble;
	ppdu_type = ppdu->u.ppdu_type;
	num_msdu = ppdu_user->num_msdu;
	punc_mode = ppdu->punc_bw;

	if (ppdu_type == HAL_RX_TYPE_SU)
		mcs = ppdu->u.mcs;
	else
		mcs = ppdu_user->mcs;

	DP_STATS_INC(mon_peer, rx.mpdu_retry_cnt, ppdu_user->mpdu_retries);
	DP_STATS_INC(mon_peer, rx.punc_bw[punc_mode], num_msdu);
	DP_STATS_INCC(mon_peer,
		      rx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
		      ((mcs >= MAX_MCS_11BE) && (preamble == DOT11_BE)));
	DP_STATS_INCC(mon_peer,
		      rx.pkt_type[preamble].mcs_count[mcs], num_msdu,
		      ((mcs < MAX_MCS_11BE) && (preamble == DOT11_BE)));
	DP_STATS_INCC(mon_peer,
		      rx.su_be_ppdu_cnt.mcs_count[MAX_MCS - 1], 1,
		      ((mcs >= (MAX_MCS_11BE)) && (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_SU)));
	DP_STATS_INCC(mon_peer,
		      rx.su_be_ppdu_cnt.mcs_count[mcs], 1,
		      ((mcs < (MAX_MCS_11BE)) && (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_SU)));
	DP_STATS_INCC(mon_peer,
		      rx.mu_be_ppdu_cnt[TXRX_TYPE_MU_OFDMA].mcs_count[MAX_MCS - 1],
		      1, ((mcs >= (MAX_MCS_11BE)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_OFDMA)));
	DP_STATS_INCC(mon_peer,
		      rx.mu_be_ppdu_cnt[TXRX_TYPE_MU_OFDMA].mcs_count[mcs],
		      1, ((mcs < (MAX_MCS_11BE)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_OFDMA)));
	DP_STATS_INCC(mon_peer,
		      rx.mu_be_ppdu_cnt[TXRX_TYPE_MU_MIMO].mcs_count[MAX_MCS - 1],
		      1, ((mcs >= (MAX_MCS_11BE)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_MIMO)));
	DP_STATS_INCC(mon_peer,
		      rx.mu_be_ppdu_cnt[TXRX_TYPE_MU_MIMO].mcs_count[mcs],
		      1, ((mcs < (MAX_MCS_11BE)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_MIMO)));
}

void
dp_rx_mon_populate_ppdu_info_2_0(struct hal_rx_ppdu_info *hal_ppdu_info,
				 struct cdp_rx_indication_ppdu *ppdu)
{
	uint16_t puncture_pattern;
	enum cdp_punctured_modes punc_mode;

	/* Align bw value as per host data structures */
	if (hal_ppdu_info->rx_status.bw == HAL_FULL_RX_BW_320)
		ppdu->u.bw = CMN_BW_320MHZ;
	else
		ppdu->u.bw = hal_ppdu_info->rx_status.bw;
	if (hal_ppdu_info->rx_status.preamble_type == HAL_RX_PKT_TYPE_11BE) {
		/* Align preamble value as per host data structures */
		ppdu->u.preamble = DOT11_BE;
		ppdu->u.stbc = hal_ppdu_info->rx_status.is_stbc;
		ppdu->u.dcm = hal_ppdu_info->rx_status.dcm;
	} else {
		ppdu->u.preamble = hal_ppdu_info->rx_status.preamble_type;
	}

	puncture_pattern = hal_ppdu_info->rx_status.punctured_pattern;
	punc_mode = dp_mon_get_puncture_type(puncture_pattern,
					     ppdu->u.bw);
	ppdu->punc_bw = punc_mode;
}
#else
void dp_rx_mon_stats_update_2_0(struct dp_mon_peer *mon_peer,
				struct cdp_rx_indication_ppdu *ppdu,
				struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	DP_STATS_INC(mon_peer, rx.mpdu_retry_cnt, ppdu_user->mpdu_retries);
}

void
dp_rx_mon_populate_ppdu_info_2_0(struct hal_rx_ppdu_info *hal_ppdu_info,
				 struct cdp_rx_indication_ppdu *ppdu)
{
	ppdu->punc_bw = NO_PUNCTURE;
}
#endif
void dp_mon_rx_print_advanced_stats_2_0(struct dp_soc *soc,
					struct dp_pdev *pdev)
{
	struct cdp_pdev_mon_stats *rx_mon_stats;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_soc *mon_soc = pdev->soc->monitor_soc;
	struct dp_mon_pdev_be *mon_pdev_be =
				dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	rx_mon_stats = &mon_pdev->rx_mon_stats;

	DP_PRINT_STATS("total_ppdu_info_alloc = %d",
		       rx_mon_stats->total_ppdu_info_alloc);
	DP_PRINT_STATS("total_ppdu_info_free = %d",
		       rx_mon_stats->total_ppdu_info_free);
	DP_PRINT_STATS("total_ppdu_info_enq = %d",
		       rx_mon_stats->total_ppdu_info_enq);
	DP_PRINT_STATS("total_ppdu_info_drop = %d",
		       rx_mon_stats->total_ppdu_info_drop);
	DP_PRINT_STATS("rx_hdr_not_received = %d",
		       rx_mon_stats->rx_hdr_not_received);
	DP_PRINT_STATS("parent_buf_alloc = %d",
		       rx_mon_stats->parent_buf_alloc);
	DP_PRINT_STATS("parent_buf_free = %d",
		       rx_mon_stats->parent_buf_free);
	DP_PRINT_STATS("mpdus_buf_to_stack = %d",
		       rx_mon_stats->mpdus_buf_to_stack);
	DP_PRINT_STATS("frag_alloc = %d",
		       mon_soc->stats.frag_alloc);
	DP_PRINT_STATS("frag_free = %d",
		       mon_soc->stats.frag_free);
	DP_PRINT_STATS("status_buf_count = %d",
		       rx_mon_stats->status_buf_count);
	DP_PRINT_STATS("pkt_buf_count = %d",
		       rx_mon_stats->pkt_buf_count);
	DP_PRINT_STATS("rx_mon_queue_depth= %d",
		       mon_pdev_be->rx_mon_queue_depth);
	DP_PRINT_STATS("empty_desc= %d",
		       mon_pdev->rx_mon_stats.empty_desc_ppdu);
	DP_PRINT_STATS("mpdu_dropped_due_invalid_decap= %d",
		       mon_pdev->rx_mon_stats.mpdu_decap_type_invalid);
	DP_PRINT_STATS("total_free_elem= %d",
		       mon_pdev_be->total_free_elem);
	DP_PRINT_STATS("ppdu_drop_cnt= %d",
		       mon_pdev->rx_mon_stats.ppdu_drop_cnt);
	DP_PRINT_STATS("mpdu_drop_cnt= %d",
		       mon_pdev->rx_mon_stats.mpdu_drop_cnt);
	DP_PRINT_STATS("end_of_ppdu_drop_cnt= %d",
		       mon_pdev->rx_mon_stats.end_of_ppdu_drop_cnt);
	DP_PRINT_STATS("tlv_drop_cnt= %d",
		       mon_pdev->rx_mon_stats.tlv_drop_cnt);
}
#endif
