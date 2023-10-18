/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "cdp_txrx_cmn_struct.h"
#include "dp_types.h"
#include "dp_tx.h"
#include "dp_be_tx.h"
#include "dp_tx_desc.h"
#include "hal_tx.h"
#include <hal_be_api.h>
#include <hal_be_tx.h>
#include <dp_htt.h>
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define DP_TX_BANK_LOCK_CREATE(lock) qdf_mutex_create(lock)
#define DP_TX_BANK_LOCK_DESTROY(lock) qdf_mutex_destroy(lock)
#define DP_TX_BANK_LOCK_ACQUIRE(lock) qdf_mutex_acquire(lock)
#define DP_TX_BANK_LOCK_RELEASE(lock) qdf_mutex_release(lock)
#else
#define DP_TX_BANK_LOCK_CREATE(lock) qdf_spinlock_create(lock)
#define DP_TX_BANK_LOCK_DESTROY(lock) qdf_spinlock_destroy(lock)
#define DP_TX_BANK_LOCK_ACQUIRE(lock) qdf_spin_lock_bh(lock)
#define DP_TX_BANK_LOCK_RELEASE(lock) qdf_spin_unlock_bh(lock)
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
#ifdef WLAN_MCAST_MLO
/* MLO peer id for reinject*/
#define DP_MLO_MCAST_REINJECT_PEER_ID 0XFFFD
#define MAX_GSN_NUM 0x0FFF

#ifdef QCA_MULTIPASS_SUPPORT
#define INVALID_VLAN_ID         0xFFFF
#define MULTIPASS_WITH_VLAN_ID 0xFFFE
/**
 * struct dp_mlo_mpass_buf - Multipass buffer
 * @vlan_id: vlan_id of frame
 * @nbuf: pointer to skb buf
 */
struct dp_mlo_mpass_buf {
	uint16_t vlan_id;
	qdf_nbuf_t  nbuf;
};
#endif
#endif
#endif

#define DP_TX_WBM_COMPLETION_V3_VDEV_ID_GET(_var) \
	HTT_TX_WBM_COMPLETION_V2_VDEV_ID_GET(_var)
#define DP_TX_WBM_COMPLETION_V3_VALID_GET(_var) \
	HTT_TX_WBM_COMPLETION_V2_VALID_GET(_var)
#define DP_TX_WBM_COMPLETION_V3_SW_PEER_ID_GET(_var) \
	HTT_TX_WBM_COMPLETION_V2_SW_PEER_ID_GET(_var)
#define DP_TX_WBM_COMPLETION_V3_TID_NUM_GET(_var) \
	HTT_TX_WBM_COMPLETION_V2_TID_NUM_GET(_var)
#define DP_TX_WBM_COMPLETION_V3_SCH_CMD_ID_GET(_var) \
	HTT_TX_WBM_COMPLETION_V2_SCH_CMD_ID_GET(_var)
#define DP_TX_WBM_COMPLETION_V3_ACK_FRAME_RSSI_GET(_var) \
	HTT_TX_WBM_COMPLETION_V2_ACK_FRAME_RSSI_GET(_var)

extern uint8_t sec_type_map[MAX_CDP_SEC_TYPE];

#ifdef DP_TX_COMP_RING_DESC_SANITY_CHECK
/*
 * Value to mark ring desc is invalidated by buffer_virt_addr_63_32 field
 * of WBM2SW ring Desc.
 */
#define DP_TX_COMP_DESC_BUFF_VA_32BITS_HI_INVALIDATE 0x12121212

/**
 * dp_tx_comp_desc_check_and_invalidate() - sanity check for ring desc and
 *					    invalidate it after each reaping
 * @tx_comp_hal_desc: ring desc virtual address
 * @r_tx_desc: pointer to current dp TX Desc pointer
 * @tx_desc_va: the original 64 bits Desc VA got from ring Desc
 * @hw_cc_done: HW cookie conversion done or not
 *
 * If HW CC is done, check the buffer_virt_addr_63_32 value to know if
 * ring Desc is stale or not. if HW CC is not done, then compare PA between
 * ring Desc and current TX desc.
 *
 * Return: None.
 */
static inline
void dp_tx_comp_desc_check_and_invalidate(void *tx_comp_hal_desc,
					  struct dp_tx_desc_s **r_tx_desc,
					  uint64_t tx_desc_va,
					  bool hw_cc_done)
{
	qdf_dma_addr_t desc_dma_addr;

	if (qdf_likely(hw_cc_done)) {
		/* Check upper 32 bits */
		if (DP_TX_COMP_DESC_BUFF_VA_32BITS_HI_INVALIDATE ==
		    (tx_desc_va >> 32))
			*r_tx_desc = NULL;

		/* Invalidate the ring desc for 32 ~ 63 bits of VA */
		hal_tx_comp_set_desc_va_63_32(
				tx_comp_hal_desc,
				DP_TX_COMP_DESC_BUFF_VA_32BITS_HI_INVALIDATE);
	} else {
		/* Compare PA between ring desc and current TX desc stored */
		desc_dma_addr = hal_tx_comp_get_paddr(tx_comp_hal_desc);

		if (desc_dma_addr != (*r_tx_desc)->dma_addr)
			*r_tx_desc = NULL;
	}
}
#else
static inline
void dp_tx_comp_desc_check_and_invalidate(void *tx_comp_hal_desc,
					  struct dp_tx_desc_s **r_tx_desc,
					  uint64_t tx_desc_va,
					  bool hw_cc_done)
{
}
#endif

#ifdef DP_FEATURE_HW_COOKIE_CONVERSION
#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
void dp_tx_comp_get_params_from_hal_desc_be(struct dp_soc *soc,
					    void *tx_comp_hal_desc,
					    struct dp_tx_desc_s **r_tx_desc)
{
	uint32_t tx_desc_id;
	uint64_t tx_desc_va = 0;
	bool hw_cc_done =
		hal_tx_comp_get_cookie_convert_done(tx_comp_hal_desc);

	if (qdf_likely(hw_cc_done)) {
		/* HW cookie conversion done */
		tx_desc_va = hal_tx_comp_get_desc_va(tx_comp_hal_desc);
		*r_tx_desc = (struct dp_tx_desc_s *)(uintptr_t)tx_desc_va;

	} else {
		/* SW do cookie conversion to VA */
		tx_desc_id = hal_tx_comp_get_desc_id(tx_comp_hal_desc);
		*r_tx_desc =
		(struct dp_tx_desc_s *)dp_cc_desc_find(soc, tx_desc_id);
	}

	dp_tx_comp_desc_check_and_invalidate(tx_comp_hal_desc,
					     r_tx_desc, tx_desc_va,
					     hw_cc_done);

	if (*r_tx_desc)
		(*r_tx_desc)->peer_id =
				dp_tx_comp_get_peer_id_be(soc,
							  tx_comp_hal_desc);
}
#else
void dp_tx_comp_get_params_from_hal_desc_be(struct dp_soc *soc,
					    void *tx_comp_hal_desc,
					    struct dp_tx_desc_s **r_tx_desc)
{
	uint64_t tx_desc_va;

	tx_desc_va = hal_tx_comp_get_desc_va(tx_comp_hal_desc);
	*r_tx_desc = (struct dp_tx_desc_s *)(uintptr_t)tx_desc_va;

	dp_tx_comp_desc_check_and_invalidate(tx_comp_hal_desc,
					     r_tx_desc,
					     tx_desc_va,
					     true);
	if (*r_tx_desc)
		(*r_tx_desc)->peer_id =
				dp_tx_comp_get_peer_id_be(soc,
							  tx_comp_hal_desc);
}
#endif /* DP_HW_COOKIE_CONVERT_EXCEPTION */
#else

void dp_tx_comp_get_params_from_hal_desc_be(struct dp_soc *soc,
					    void *tx_comp_hal_desc,
					    struct dp_tx_desc_s **r_tx_desc)
{
	uint32_t tx_desc_id;

	/* SW do cookie conversion to VA */
	tx_desc_id = hal_tx_comp_get_desc_id(tx_comp_hal_desc);
	*r_tx_desc =
	(struct dp_tx_desc_s *)dp_cc_desc_find(soc, tx_desc_id);

	dp_tx_comp_desc_check_and_invalidate(tx_comp_hal_desc,
					     r_tx_desc, 0,
					     false);

	if (*r_tx_desc)
		(*r_tx_desc)->peer_id =
				dp_tx_comp_get_peer_id_be(soc,
							  tx_comp_hal_desc);
}
#endif /* DP_FEATURE_HW_COOKIE_CONVERSION */

static inline
void dp_tx_process_mec_notify_be(struct dp_soc *soc, uint8_t *status)
{
	struct dp_vdev *vdev;
	uint8_t vdev_id;
	uint32_t *htt_desc = (uint32_t *)status;

	qdf_assert_always(!soc->mec_fw_offload);

	/*
	 * Get vdev id from HTT status word in case of MEC
	 * notification
	 */
	vdev_id = DP_TX_WBM_COMPLETION_V3_VDEV_ID_GET(htt_desc[4]);
	if (qdf_unlikely(vdev_id >= MAX_VDEV_CNT))
		return;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
				     DP_MOD_ID_HTT_COMP);
	if (!vdev)
		return;
	dp_tx_mec_handler(vdev, status);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_HTT_COMP);
}

void dp_tx_process_htt_completion_be(struct dp_soc *soc,
				     struct dp_tx_desc_s *tx_desc,
				     uint8_t *status,
				     uint8_t ring_id)
{
	uint8_t tx_status;
	struct dp_pdev *pdev;
	struct dp_vdev *vdev = NULL;
	struct hal_tx_completion_status ts = {0};
	uint32_t *htt_desc = (uint32_t *)status;
	struct dp_txrx_peer *txrx_peer;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	struct cdp_tid_tx_stats *tid_stats = NULL;
	struct htt_soc *htt_handle;
	uint8_t vdev_id;

	tx_status = HTT_TX_WBM_COMPLETION_V3_TX_STATUS_GET(htt_desc[0]);
	htt_handle = (struct htt_soc *)soc->htt_handle;
	htt_wbm_event_record(htt_handle->htt_logger_handle, tx_status, status);

	/*
	 * There can be scenario where WBM consuming descriptor enqueued
	 * from TQM2WBM first and TQM completion can happen before MEC
	 * notification comes from FW2WBM. Avoid access any field of tx
	 * descriptor in case of MEC notify.
	 */
	if (tx_status == HTT_TX_FW2WBM_TX_STATUS_MEC_NOTIFY)
		return dp_tx_process_mec_notify_be(soc, status);

	/*
	 * If the descriptor is already freed in vdev_detach,
	 * continue to next descriptor
	 */
	if (qdf_unlikely(!tx_desc->flags)) {
		dp_tx_comp_info_rl("Descriptor freed in vdev_detach %d",
				   tx_desc->id);
		return;
	}

	if (qdf_unlikely(tx_desc->vdev_id == DP_INVALID_VDEV_ID)) {
		dp_tx_comp_info_rl("Invalid vdev_id %d", tx_desc->id);
		tx_desc->flags |= DP_TX_DESC_FLAG_TX_COMP_ERR;
		goto release_tx_desc;
	}

	pdev = tx_desc->pdev;

	if (qdf_unlikely(tx_desc->pdev->is_pdev_down)) {
		dp_tx_comp_info_rl("pdev in down state %d", tx_desc->id);
		tx_desc->flags |= DP_TX_DESC_FLAG_TX_COMP_ERR;
		goto release_tx_desc;
	}

	qdf_assert(tx_desc->pdev);

	vdev_id = tx_desc->vdev_id;
	vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
				     DP_MOD_ID_HTT_COMP);

	if (qdf_unlikely(!vdev)) {
		dp_tx_comp_info_rl("Unable to get vdev ref  %d", tx_desc->id);
		tx_desc->flags |= DP_TX_DESC_FLAG_TX_COMP_ERR;
		goto release_tx_desc;
	}

	switch (tx_status) {
	case HTT_TX_FW2WBM_TX_STATUS_OK:
	case HTT_TX_FW2WBM_TX_STATUS_DROP:
	case HTT_TX_FW2WBM_TX_STATUS_TTL:
	{
		uint8_t tid;

		if (DP_TX_WBM_COMPLETION_V3_VALID_GET(htt_desc[3])) {
			ts.peer_id =
				DP_TX_WBM_COMPLETION_V3_SW_PEER_ID_GET(
						htt_desc[3]);
			ts.tid =
				DP_TX_WBM_COMPLETION_V3_TID_NUM_GET(
						htt_desc[3]);
		} else {
			ts.peer_id = HTT_INVALID_PEER;
			ts.tid = HTT_INVALID_TID;
		}
		ts.release_src = HAL_TX_COMP_RELEASE_SOURCE_FW;
		ts.ppdu_id =
			DP_TX_WBM_COMPLETION_V3_SCH_CMD_ID_GET(
					htt_desc[2]);
		ts.ack_frame_rssi =
			DP_TX_WBM_COMPLETION_V3_ACK_FRAME_RSSI_GET(
					htt_desc[2]);

		ts.tsf = htt_desc[4];
		ts.first_msdu = 1;
		ts.last_msdu = 1;
		switch (tx_status) {
		case HTT_TX_FW2WBM_TX_STATUS_OK:
			ts.status = HAL_TX_TQM_RR_FRAME_ACKED;
			break;
		case HTT_TX_FW2WBM_TX_STATUS_DROP:
			ts.status = HAL_TX_TQM_RR_REM_CMD_REM;
			break;
		case HTT_TX_FW2WBM_TX_STATUS_TTL:
			ts.status = HAL_TX_TQM_RR_REM_CMD_TX;
			break;
		}
		tid = ts.tid;
		if (qdf_unlikely(tid >= CDP_MAX_DATA_TIDS))
			tid = CDP_MAX_DATA_TIDS - 1;

		tid_stats = &pdev->stats.tid_stats.tid_tx_stats[ring_id][tid];

		if (qdf_unlikely(pdev->delay_stats_flag) ||
		    qdf_unlikely(dp_is_vdev_tx_delay_stats_enabled(vdev)))
			dp_tx_compute_delay(vdev, tx_desc, tid, ring_id);
		if (tx_status < CDP_MAX_TX_HTT_STATUS)
			tid_stats->htt_status_cnt[tx_status]++;

		txrx_peer = dp_txrx_peer_get_ref_by_id(soc, ts.peer_id,
						       &txrx_ref_handle,
						       DP_MOD_ID_HTT_COMP);
		if (qdf_likely(txrx_peer))
			dp_tx_update_peer_basic_stats(
						txrx_peer,
						qdf_nbuf_len(tx_desc->nbuf),
						tx_status,
						pdev->enhanced_stats_en);

		dp_tx_comp_process_tx_status(soc, tx_desc, &ts, txrx_peer,
					     ring_id);
		dp_tx_comp_process_desc(soc, tx_desc, &ts, txrx_peer);
		dp_tx_desc_release(tx_desc, tx_desc->pool_id);

		if (qdf_likely(txrx_peer))
			dp_txrx_peer_unref_delete(txrx_ref_handle,
						  DP_MOD_ID_HTT_COMP);

		break;
	}
	case HTT_TX_FW2WBM_TX_STATUS_REINJECT:
	{
		uint8_t reinject_reason;

		reinject_reason =
			HTT_TX_WBM_COMPLETION_V3_REINJECT_REASON_GET(
								htt_desc[1]);
		dp_tx_reinject_handler(soc, vdev, tx_desc,
				       status, reinject_reason);
		break;
	}
	case HTT_TX_FW2WBM_TX_STATUS_INSPECT:
	{
		dp_tx_inspect_handler(soc, vdev, tx_desc, status);
		break;
	}
	case HTT_TX_FW2WBM_TX_STATUS_VDEVID_MISMATCH:
	{
		DP_STATS_INC(vdev, tx_i.dropped.fail_per_pkt_vdev_id_check, 1);
		goto release_tx_desc;
	}
	default:
		dp_tx_comp_err("Invalid HTT tx_status %d\n",
			       tx_status);
		goto release_tx_desc;
	}

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_HTT_COMP);
	return;

release_tx_desc:
	dp_tx_comp_free_buf(soc, tx_desc, false);
	dp_tx_desc_release(tx_desc, tx_desc->pool_id);
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_HTT_COMP);
}

#ifdef QCA_OL_TX_MULTIQ_SUPPORT
#ifdef DP_TX_IMPLICIT_RBM_MAPPING
/*
 * dp_tx_get_rbm_id()- Get the RBM ID for data transmission completion.
 * @dp_soc - DP soc structure pointer
 * @ring_id - Transmit Queue/ring_id to be used when XPS is enabled
 *
 * Return - RBM ID corresponding to TCL ring_id
 */
static inline uint8_t dp_tx_get_rbm_id_be(struct dp_soc *soc,
					  uint8_t ring_id)
{
	return 0;
}
#else
static inline uint8_t dp_tx_get_rbm_id_be(struct dp_soc *soc,
					  uint8_t ring_id)
{
	return (ring_id ? soc->wbm_sw0_bm_id + (ring_id - 1) :
			  HAL_WBM_SW2_BM_ID(soc->wbm_sw0_bm_id));
}
#endif /*DP_TX_IMPLICIT_RBM_MAPPING*/
#else
static inline uint8_t dp_tx_get_rbm_id_be(struct dp_soc *soc,
					  uint8_t tcl_index)
{
	uint8_t rbm;

	rbm = wlan_cfg_get_rbm_id_for_index(soc->wlan_cfg_ctx, tcl_index);
	dp_verbose_debug("tcl_id %u rbm %u", tcl_index, rbm);
	return rbm;
}
#endif

#ifdef QCA_SUPPORT_TX_MIN_RATES_FOR_SPECIAL_FRAMES

/*
 * dp_tx_set_min_rates_for_critical_frames()- sets min-rates for critical pkts
 * @dp_soc - DP soc structure pointer
 * @hal_tx_desc - HAL descriptor where fields are set
 * nbuf - skb to be considered for min rates
 *
 * The function relies on upper layers to set QDF_NBUF_CB_TX_EXTRA_IS_CRITICAL
 * and uses it to determine if the frame is critical. For a critical frame,
 * flow override bits are set to classify the frame into HW's high priority
 * queue. The HW will pick pre-configured min rates for such packets.
 *
 * Return - None
 */
static void
dp_tx_set_min_rates_for_critical_frames(struct dp_soc *soc,
					uint32_t *hal_tx_desc,
					qdf_nbuf_t nbuf)
{
/*
 * Critical frames should be queued to the high priority queue for the TID on
 * on which they are sent out (for the concerned peer).
 * FW is using HTT_MSDU_Q_IDX 2 for HOL (high priority) queue.
 * htt_msdu_idx = (2 * who_classify_info_sel) + flow_override
 * Hence, using who_classify_info_sel = 1, flow_override = 0 to select
 * HOL queue.
 */
	if (QDF_NBUF_CB_TX_EXTRA_IS_CRITICAL(nbuf)) {
		hal_tx_desc_set_flow_override_enable(hal_tx_desc, 1);
		hal_tx_desc_set_flow_override(hal_tx_desc, 0);
		hal_tx_desc_set_who_classify_info_sel(hal_tx_desc, 1);
		hal_tx_desc_set_tx_notify_frame(hal_tx_desc,
						TX_SEMI_HARD_NOTIFY_E);
	}
}
#else
static inline void
dp_tx_set_min_rates_for_critical_frames(struct dp_soc *soc,
					uint32_t *hal_tx_desc_cached,
					qdf_nbuf_t nbuf)
{
}
#endif

#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
/**
 * dp_tx_set_particular_tx_queue() - set particular TX TQM flow queue 3 for
 *				     TX packets, currently TCP ACK only
 * @soc: DP soc structure pointer
 * @hal_tx_desc: HAL descriptor where fields are set
 * @nbuf: skb to be considered for particular TX queue
 *
 * Return: None
 */
static inline
void dp_tx_set_particular_tx_queue(struct dp_soc *soc,
				   uint32_t *hal_tx_desc,
				   qdf_nbuf_t nbuf)
{
	if (!soc->wlan_cfg_ctx->tx_pkt_inspect_for_ilp)
		return;

	if (qdf_unlikely(QDF_NBUF_CB_GET_PACKET_TYPE(nbuf) ==
			 QDF_NBUF_CB_PACKET_TYPE_TCP_ACK)) {
		hal_tx_desc_set_flow_override_enable(hal_tx_desc, 1);
		hal_tx_desc_set_flow_override(hal_tx_desc, 1);
		hal_tx_desc_set_who_classify_info_sel(hal_tx_desc, 1);
	}
}
#else
static inline
void dp_tx_set_particular_tx_queue(struct dp_soc *soc,
				   uint32_t *hal_tx_desc,
				   qdf_nbuf_t nbuf)
{
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP) && \
	defined(WLAN_MCAST_MLO)
#ifdef QCA_MULTIPASS_SUPPORT
/**
 * dp_tx_mlo_mcast_multipass_lookup() - lookup vlan_id in mpass peer list
 * @be_vdev: Handle to DP be_vdev structure
 * @ptnr_vdev: DP ptnr_vdev handle
 * @arg: pointer to dp_mlo_mpass_ buf
 *
 * Return: None
 */
static void
dp_tx_mlo_mcast_multipass_lookup(struct dp_vdev_be *be_vdev,
				 struct dp_vdev *ptnr_vdev,
				 void *arg)
{
	struct dp_mlo_mpass_buf *ptr = (struct dp_mlo_mpass_buf *)arg;
	struct dp_txrx_peer *txrx_peer = NULL;
	struct vlan_ethhdr *veh = NULL;
	qdf_ether_header_t *eh = (qdf_ether_header_t *)qdf_nbuf_data(ptr->nbuf);
	uint16_t vlan_id = 0;
	bool not_vlan = ((ptnr_vdev->tx_encap_type == htt_cmn_pkt_type_raw) ||
			(htons(eh->ether_type) != ETH_P_8021Q));

	if (qdf_unlikely(not_vlan))
		return;
	veh = (struct vlan_ethhdr *)eh;
	vlan_id = (ntohs(veh->h_vlan_TCI) & VLAN_VID_MASK);

	qdf_spin_lock_bh(&ptnr_vdev->mpass_peer_mutex);
	TAILQ_FOREACH(txrx_peer, &ptnr_vdev->mpass_peer_list,
		      mpass_peer_list_elem) {
		if (vlan_id == txrx_peer->vlan_id) {
			qdf_spin_unlock_bh(&ptnr_vdev->mpass_peer_mutex);
			ptr->vlan_id = vlan_id;
			return;
		}
	}
	qdf_spin_unlock_bh(&ptnr_vdev->mpass_peer_mutex);
}

/**
 * dp_tx_mlo_mcast_multipass_send() - send multipass MLO Mcast packets
 * @be_vdev: Handle to DP be_vdev structure
 * @ptnr_vdev: DP ptnr_vdev handle
 * @arg: pointer to dp_mlo_mpass_ buf
 *
 * Return: None
 */
static void
dp_tx_mlo_mcast_multipass_send(struct dp_vdev_be *be_vdev,
			       struct dp_vdev *ptnr_vdev,
			       void *arg)
{
	struct dp_mlo_mpass_buf *ptr = (struct dp_mlo_mpass_buf *)arg;
	struct dp_tx_msdu_info_s msdu_info;
	struct dp_vdev_be *be_ptnr_vdev = NULL;
	qdf_nbuf_t  nbuf_clone;
	uint16_t group_key = 0;

	be_ptnr_vdev = dp_get_be_vdev_from_dp_vdev(ptnr_vdev);
	if (be_vdev != be_ptnr_vdev) {
		nbuf_clone = qdf_nbuf_clone(ptr->nbuf);
		if (qdf_unlikely(!nbuf_clone)) {
			dp_tx_debug("nbuf clone failed");
			return;
		}
	} else {
		nbuf_clone = ptr->nbuf;
	}
	qdf_mem_zero(&msdu_info, sizeof(msdu_info));
	dp_tx_get_queue(ptnr_vdev, nbuf_clone, &msdu_info.tx_queue);
	msdu_info.gsn = be_vdev->seq_num;
	be_ptnr_vdev->seq_num = be_vdev->seq_num;

	if (ptr->vlan_id == MULTIPASS_WITH_VLAN_ID) {
		msdu_info.tid = HTT_TX_EXT_TID_INVALID;
		HTT_TX_MSDU_EXT2_DESC_FLAG_VALID_KEY_FLAGS_SET(
						msdu_info.meta_data[0], 1);
	} else {
		/* return when vlan map is not initialized */
		if (!ptnr_vdev->iv_vlan_map)
			return;
		group_key = ptnr_vdev->iv_vlan_map[ptr->vlan_id];

		/*
		 * If group key is not installed, drop the frame.
		 */

		if (!group_key)
			return;

		dp_tx_remove_vlan_tag(ptnr_vdev, nbuf_clone);
		dp_tx_add_groupkey_metadata(ptnr_vdev, &msdu_info, group_key);
		msdu_info.exception_fw = 1;
	}

	nbuf_clone = dp_tx_send_msdu_single(
					ptnr_vdev,
					nbuf_clone,
					&msdu_info,
					DP_MLO_MCAST_REINJECT_PEER_ID,
					NULL);
	if (qdf_unlikely(nbuf_clone)) {
		dp_info("pkt send failed");
		qdf_nbuf_free(nbuf_clone);
		return;
	}
}

/**
 * dp_tx_mlo_mcast_multipass_handler - If frame needs multipass processing
 * @soc: DP soc handle
 * @vdev: DP vdev handle
 * @nbuf: nbuf to be enqueued
 *
 * Return: true if handling is done else false
 */
static bool
dp_tx_mlo_mcast_multipass_handler(struct dp_soc *soc,
				  struct dp_vdev *vdev,
				  qdf_nbuf_t nbuf)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	qdf_nbuf_t nbuf_copy = NULL;
	struct dp_mlo_mpass_buf mpass_buf;

	memset(&mpass_buf, 0, sizeof(struct dp_mlo_mpass_buf));
	mpass_buf.vlan_id = INVALID_VLAN_ID;
	mpass_buf.nbuf = nbuf;

	dp_tx_mlo_mcast_multipass_lookup(be_vdev, vdev, &mpass_buf);
	if (mpass_buf.vlan_id == INVALID_VLAN_ID) {
		dp_mcast_mlo_iter_ptnr_vdev(be_soc, be_vdev,
					    dp_tx_mlo_mcast_multipass_lookup,
					    &mpass_buf, DP_MOD_ID_TX);
		/*
		 * Do not drop the frame when vlan_id doesn't match.
		 * Send the frame as it is.
		 */
		if (mpass_buf.vlan_id == INVALID_VLAN_ID)
			return false;
	}

	/* AP can have classic clients, special clients &
	 * classic repeaters.
	 * 1. Classic clients & special client:
	 *	Remove vlan header, find corresponding group key
	 *	index, fill in metaheader and enqueue multicast
	 *	frame to TCL.
	 * 2. Classic repeater:
	 *	Pass through to classic repeater with vlan tag
	 *	intact without any group key index. Hardware
	 *	will know which key to use to send frame to
	 *	repeater.
	 */
	nbuf_copy = qdf_nbuf_copy(nbuf);

	/*
	 * Send multicast frame to special peers even
	 * if pass through to classic repeater fails.
	 */
	if (nbuf_copy) {
		struct dp_mlo_mpass_buf mpass_buf_copy = {0};

		mpass_buf_copy.vlan_id = MULTIPASS_WITH_VLAN_ID;
		mpass_buf_copy.nbuf = nbuf_copy;
		/* send frame on partner vdevs */
		dp_mcast_mlo_iter_ptnr_vdev(be_soc, be_vdev,
					    dp_tx_mlo_mcast_multipass_send,
					    &mpass_buf_copy, DP_MOD_ID_TX);

		/* send frame on mcast primary vdev */
		dp_tx_mlo_mcast_multipass_send(be_vdev, vdev, &mpass_buf_copy);

		if (qdf_unlikely(be_vdev->seq_num > MAX_GSN_NUM))
			be_vdev->seq_num = 0;
		else
			be_vdev->seq_num++;
	}

	dp_mcast_mlo_iter_ptnr_vdev(be_soc, be_vdev,
				    dp_tx_mlo_mcast_multipass_send,
				    &mpass_buf, DP_MOD_ID_TX);
	dp_tx_mlo_mcast_multipass_send(be_vdev, vdev, &mpass_buf);

	if (qdf_unlikely(be_vdev->seq_num > MAX_GSN_NUM))
		be_vdev->seq_num = 0;
	else
		be_vdev->seq_num++;

	return true;
}
#else
static bool
dp_tx_mlo_mcast_multipass_handler(struct dp_soc *soc, struct dp_vdev *vdev,
				  qdf_nbuf_t nbuf)
{
	return false;
}
#endif

void
dp_tx_mlo_mcast_pkt_send(struct dp_vdev_be *be_vdev,
			 struct dp_vdev *ptnr_vdev,
			 void *arg)
{
	qdf_nbuf_t  nbuf = (qdf_nbuf_t)arg;
	qdf_nbuf_t  nbuf_clone;
	struct dp_vdev_be *be_ptnr_vdev = NULL;
	struct dp_tx_msdu_info_s msdu_info;

	be_ptnr_vdev = dp_get_be_vdev_from_dp_vdev(ptnr_vdev);
	if (be_vdev != be_ptnr_vdev) {
		nbuf_clone = qdf_nbuf_clone(nbuf);
		if (qdf_unlikely(!nbuf_clone)) {
			dp_tx_debug("nbuf clone failed");
			return;
		}
	} else {
		nbuf_clone = nbuf;
	}

	qdf_mem_zero(&msdu_info, sizeof(msdu_info));
	dp_tx_get_queue(ptnr_vdev, nbuf_clone, &msdu_info.tx_queue);
	msdu_info.gsn = be_vdev->seq_num;
	be_ptnr_vdev->seq_num = be_vdev->seq_num;

	nbuf_clone = dp_tx_send_msdu_single(
					ptnr_vdev,
					nbuf_clone,
					&msdu_info,
					DP_MLO_MCAST_REINJECT_PEER_ID,
					NULL);
	if (qdf_unlikely(nbuf_clone)) {
		dp_info("pkt send failed");
		qdf_nbuf_free(nbuf_clone);
		return;
	}
}

static inline void
dp_tx_vdev_id_set_hal_tx_desc(uint32_t *hal_tx_desc_cached,
			      struct dp_vdev *vdev,
			      struct dp_tx_msdu_info_s *msdu_info)
{
	hal_tx_desc_set_vdev_id(hal_tx_desc_cached, msdu_info->vdev_id);
}

void dp_tx_mlo_mcast_handler_be(struct dp_soc *soc,
				struct dp_vdev *vdev,
				qdf_nbuf_t nbuf)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (qdf_unlikely(vdev->multipass_en) &&
	    dp_tx_mlo_mcast_multipass_handler(soc, vdev, nbuf))
		return;
	/* send frame on partner vdevs */
	dp_mcast_mlo_iter_ptnr_vdev(be_soc, be_vdev,
				    dp_tx_mlo_mcast_pkt_send,
				    nbuf, DP_MOD_ID_REINJECT);

	/* send frame on mcast primary vdev */
	dp_tx_mlo_mcast_pkt_send(be_vdev, vdev, nbuf);

	if (qdf_unlikely(be_vdev->seq_num > MAX_GSN_NUM))
		be_vdev->seq_num = 0;
	else
		be_vdev->seq_num++;
}

bool dp_tx_mlo_is_mcast_primary_be(struct dp_soc *soc,
				   struct dp_vdev *vdev)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	if (be_vdev->mcast_primary)
		return true;

	return false;
}
#else
static inline void
dp_tx_vdev_id_set_hal_tx_desc(uint32_t *hal_tx_desc_cached,
			      struct dp_vdev *vdev,
			      struct dp_tx_msdu_info_s *msdu_info)
{
	hal_tx_desc_set_vdev_id(hal_tx_desc_cached, vdev->vdev_id);
}
#endif
#if defined(WLAN_FEATURE_11BE_MLO) && !defined(WLAN_MLO_MULTI_CHIP) && \
	!defined(WLAN_MCAST_MLO)
void dp_tx_mlo_mcast_handler_be(struct dp_soc *soc,
				struct dp_vdev *vdev,
				qdf_nbuf_t nbuf)
{
}

bool dp_tx_mlo_is_mcast_primary_be(struct dp_soc *soc,
				   struct dp_vdev *vdev)
{
	return false;
}
#endif

#ifdef CONFIG_SAWF
void dp_sawf_config_be(struct dp_soc *soc, uint32_t *hal_tx_desc_cached,
		       uint16_t *fw_metadata, qdf_nbuf_t nbuf)
{
	uint8_t q_id = 0;

	if (!wlan_cfg_get_sawf_config(soc->wlan_cfg_ctx))
		return;

	dp_sawf_tcl_cmd(fw_metadata, nbuf);
	q_id = dp_sawf_queue_id_get(nbuf);

	if (q_id == DP_SAWF_DEFAULT_Q_INVALID)
		return;
	hal_tx_desc_set_hlos_tid(hal_tx_desc_cached,
				 (q_id & (CDP_DATA_TID_MAX - 1)));
	hal_tx_desc_set_flow_override_enable(hal_tx_desc_cached,
					     DP_TX_FLOW_OVERRIDE_ENABLE);
	hal_tx_desc_set_flow_override(hal_tx_desc_cached,
				      DP_TX_FLOW_OVERRIDE_GET(q_id));
	hal_tx_desc_set_who_classify_info_sel(hal_tx_desc_cached,
					      DP_TX_WHO_CLFY_INF_SEL_GET(q_id));
}

#else

static inline
void dp_sawf_config_be(struct dp_soc *soc, uint32_t *hal_tx_desc_cached,
		       uint16_t *fw_metadata, qdf_nbuf_t nbuf)
{
}

static inline
QDF_STATUS dp_sawf_tx_enqueue_peer_stats(struct dp_soc *soc,
					 struct dp_tx_desc_s *tx_desc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_sawf_tx_enqueue_fail_peer_stats(struct dp_soc *soc,
					      struct dp_tx_desc_s *tx_desc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_SUPPORT_PPEDS
/**
 * dp_ppeds_tx_comp_handler()- Handle tx completions for ppe2tcl ring
 * @soc: Handle to DP Soc structure
 * @quota: Max number of tx completions to process
 *
 * Return: Number of tx completions processed
 */
int dp_ppeds_tx_comp_handler(struct dp_soc_be *be_soc, uint32_t quota)
{
	uint32_t num_avail_for_reap = 0;
	void *tx_comp_hal_desc;
	uint8_t buf_src;
	uint32_t count = 0;
	struct dp_tx_desc_s *tx_desc = NULL;
	struct dp_tx_desc_s *head_desc = NULL;
	struct dp_tx_desc_s *tail_desc = NULL;
	struct dp_soc *soc = &be_soc->soc;
	void *last_prefetch_hw_desc = NULL;
	struct dp_tx_desc_s *last_prefetch_sw_desc = NULL;
	hal_soc_handle_t hal_soc = soc->hal_soc;
	hal_ring_handle_t hal_ring_hdl =
				be_soc->ppeds_wbm_release_ring.hal_srng;

	if (qdf_unlikely(dp_srng_access_start(NULL, soc, hal_ring_hdl))) {
		dp_err("HAL RING Access Failed -- %pK", hal_ring_hdl);
		return 0;
	}

	num_avail_for_reap = hal_srng_dst_num_valid(hal_soc, hal_ring_hdl, 0);

	if (num_avail_for_reap >= quota)
		num_avail_for_reap = quota;

	dp_srng_dst_inv_cached_descs(soc, hal_ring_hdl, num_avail_for_reap);

	last_prefetch_hw_desc = dp_srng_dst_prefetch(hal_soc, hal_ring_hdl,
						     num_avail_for_reap);

	while (qdf_likely(num_avail_for_reap--)) {
		tx_comp_hal_desc =  dp_srng_dst_get_next(soc, hal_ring_hdl);
		if (qdf_unlikely(!tx_comp_hal_desc))
			break;

		buf_src = hal_tx_comp_get_buffer_source(hal_soc,
							tx_comp_hal_desc);

		if (qdf_unlikely(buf_src != HAL_TX_COMP_RELEASE_SOURCE_TQM &&
				 buf_src != HAL_TX_COMP_RELEASE_SOURCE_FW)) {
			dp_err("Tx comp release_src != TQM | FW but from %d",
			       buf_src);
			qdf_assert_always(0);
		}

		dp_tx_comp_get_params_from_hal_desc_be(soc, tx_comp_hal_desc,
						       &tx_desc);

		if (!tx_desc) {
			dp_err("unable to retrieve tx_desc!");
			qdf_assert_always(0);
			continue;
		}

		if (qdf_unlikely(!(tx_desc->flags &
				   DP_TX_DESC_FLAG_ALLOCATED) ||
				 !(tx_desc->flags & DP_TX_DESC_FLAG_PPEDS))) {
			qdf_assert_always(0);
			continue;
		}

		tx_desc->buffer_src = buf_src;

		if (qdf_unlikely(buf_src == HAL_TX_COMP_RELEASE_SOURCE_FW)) {
			qdf_nbuf_free(tx_desc->nbuf);
			dp_ppeds_tx_desc_free(soc, tx_desc);
		} else {
			tx_desc->tx_status =
				hal_tx_comp_get_tx_status(tx_comp_hal_desc);

			if (!head_desc) {
				head_desc = tx_desc;
				tail_desc = tx_desc;
			}

			tail_desc->next = tx_desc;
			tx_desc->next = NULL;
			tail_desc = tx_desc;

			count++;

			dp_tx_prefetch_hw_sw_nbuf_desc(soc, hal_soc,
						       num_avail_for_reap,
						       hal_ring_hdl,
						       &last_prefetch_hw_desc,
						       &last_prefetch_sw_desc);
		}
	}

	dp_srng_access_end(NULL, soc, hal_ring_hdl);

	if (head_desc)
		dp_tx_comp_process_desc_list(soc, head_desc,
					     CDP_MAX_TX_COMP_PPE_RING);

	return count;
}
#endif

#if defined(QCA_SUPPORT_WDS_EXTENDED)
static inline void
dp_get_peer_from_tx_exc_meta(struct dp_soc *soc, uint32_t *hal_tx_desc_cached,
			     struct cdp_tx_exception_metadata *tx_exc_metadata,
			     uint16_t *ast_idx, uint16_t *ast_hash)
{
	struct dp_peer *peer = NULL;

	if (tx_exc_metadata->is_wds_extended) {
		peer = dp_peer_get_ref_by_id(soc, tx_exc_metadata->peer_id,
					     DP_MOD_ID_TX);
		if (peer) {
			*ast_idx = peer->ast_idx;
			*ast_hash = peer->ast_hash;
			hal_tx_desc_set_index_lookup_override
							(soc->hal_soc,
							 hal_tx_desc_cached,
							 0x1);
			dp_peer_unref_delete(peer, DP_MOD_ID_TX);
		}
	} else {
		return;
	}
}

#else
static inline void
dp_get_peer_from_tx_exc_meta(struct dp_soc *soc, uint32_t *hal_tx_desc_cached,
			     struct cdp_tx_exception_metadata *tx_exc_metadata,
			     uint16_t *ast_idx, uint16_t *ast_hash)
{
}
#endif

QDF_STATUS
dp_tx_hw_enqueue_be(struct dp_soc *soc, struct dp_vdev *vdev,
		    struct dp_tx_desc_s *tx_desc, uint16_t fw_metadata,
		    struct cdp_tx_exception_metadata *tx_exc_metadata,
		    struct dp_tx_msdu_info_s *msdu_info)
{
	void *hal_tx_desc;
	uint32_t *hal_tx_desc_cached;
	int coalesce = 0;
	struct dp_tx_queue *tx_q = &msdu_info->tx_queue;
	uint8_t ring_id = tx_q->ring_id;
	uint8_t tid = msdu_info->tid;
	struct dp_vdev_be *be_vdev;
	uint8_t cached_desc[HAL_TX_DESC_LEN_BYTES] = { 0 };
	uint8_t bm_id = dp_tx_get_rbm_id_be(soc, ring_id);
	hal_ring_handle_t hal_ring_hdl = NULL;
	QDF_STATUS status = QDF_STATUS_E_RESOURCES;
	uint8_t num_desc_bytes = HAL_TX_DESC_LEN_BYTES;
	uint16_t ast_idx = vdev->bss_ast_idx;
	uint16_t ast_hash = vdev->bss_ast_hash;

	be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	if (!dp_tx_is_desc_id_valid(soc, tx_desc->id)) {
		dp_err_rl("Invalid tx desc id:%d", tx_desc->id);
		return QDF_STATUS_E_RESOURCES;
	}

	if (qdf_unlikely(tx_exc_metadata)) {
		qdf_assert_always((tx_exc_metadata->tx_encap_type ==
				   CDP_INVALID_TX_ENCAP_TYPE) ||
				   (tx_exc_metadata->tx_encap_type ==
				    vdev->tx_encap_type));

		if (tx_exc_metadata->tx_encap_type == htt_cmn_pkt_type_raw)
			qdf_assert_always((tx_exc_metadata->sec_type ==
					   CDP_INVALID_SEC_TYPE) ||
					   tx_exc_metadata->sec_type ==
					   vdev->sec_type);
		dp_get_peer_from_tx_exc_meta(soc, (void *)cached_desc,
					     tx_exc_metadata,
					     &ast_idx, &ast_hash);
	}

	hal_tx_desc_cached = (void *)cached_desc;

	if (dp_sawf_tag_valid_get(tx_desc->nbuf)) {
		dp_sawf_config_be(soc, hal_tx_desc_cached,
				  &fw_metadata, tx_desc->nbuf);
		dp_sawf_tx_enqueue_peer_stats(soc, tx_desc);
	}

	hal_tx_desc_set_buf_addr_be(soc->hal_soc, hal_tx_desc_cached,
				    tx_desc->dma_addr, bm_id, tx_desc->id,
				    (tx_desc->flags & DP_TX_DESC_FLAG_FRAG));
	hal_tx_desc_set_lmac_id_be(soc->hal_soc, hal_tx_desc_cached,
				   vdev->lmac_id);

	hal_tx_desc_set_search_index_be(soc->hal_soc, hal_tx_desc_cached,
					ast_idx);
	/*
	 * Bank_ID is used as DSCP_TABLE number in beryllium
	 * So there is no explicit field used for DSCP_TID_TABLE_NUM.
	 */

	hal_tx_desc_set_cache_set_num(soc->hal_soc, hal_tx_desc_cached,
				      (ast_hash & 0xF));

	hal_tx_desc_set_fw_metadata(hal_tx_desc_cached, fw_metadata);
	hal_tx_desc_set_buf_length(hal_tx_desc_cached, tx_desc->length);
	hal_tx_desc_set_buf_offset(hal_tx_desc_cached, tx_desc->pkt_offset);

	if (tx_desc->flags & DP_TX_DESC_FLAG_TO_FW)
		hal_tx_desc_set_to_fw(hal_tx_desc_cached, 1);

	/* verify checksum offload configuration*/
	if ((qdf_nbuf_get_tx_cksum(tx_desc->nbuf) ==
				   QDF_NBUF_TX_CKSUM_TCP_UDP) ||
	      qdf_nbuf_is_tso(tx_desc->nbuf)) {
		hal_tx_desc_set_l3_checksum_en(hal_tx_desc_cached, 1);
		hal_tx_desc_set_l4_checksum_en(hal_tx_desc_cached, 1);
	}

	hal_tx_desc_set_bank_id(hal_tx_desc_cached, vdev->bank_id);

	dp_tx_vdev_id_set_hal_tx_desc(hal_tx_desc_cached, vdev, msdu_info);

	if (tid != HTT_TX_EXT_TID_INVALID)
		hal_tx_desc_set_hlos_tid(hal_tx_desc_cached, tid);

	dp_tx_set_min_rates_for_critical_frames(soc, hal_tx_desc_cached,
						tx_desc->nbuf);
	dp_tx_set_particular_tx_queue(soc, hal_tx_desc_cached,
				      tx_desc->nbuf);
	dp_tx_desc_set_ktimestamp(vdev, tx_desc);

	hal_ring_hdl = dp_tx_get_hal_ring_hdl(soc, ring_id);

	if (qdf_unlikely(dp_tx_hal_ring_access_start(soc, hal_ring_hdl))) {
		dp_err("HAL RING Access Failed -- %pK", hal_ring_hdl);
		DP_STATS_INC(soc, tx.tcl_ring_full[ring_id], 1);
		DP_STATS_INC(vdev, tx_i.dropped.enqueue_fail, 1);
		dp_sawf_tx_enqueue_fail_peer_stats(soc, tx_desc);
		return status;
	}

	hal_tx_desc = hal_srng_src_get_next(soc->hal_soc, hal_ring_hdl);
	if (qdf_unlikely(!hal_tx_desc)) {
		dp_verbose_debug("TCL ring full ring_id:%d", ring_id);
		DP_STATS_INC(soc, tx.tcl_ring_full[ring_id], 1);
		DP_STATS_INC(vdev, tx_i.dropped.enqueue_fail, 1);
		dp_sawf_tx_enqueue_fail_peer_stats(soc, tx_desc);
		goto ring_access_fail;
	}

	tx_desc->flags |= DP_TX_DESC_FLAG_QUEUED_TX;
	dp_vdev_peer_stats_update_protocol_cnt_tx(vdev, tx_desc->nbuf);

	/* Sync cached descriptor with HW */
	hal_tx_desc_sync(hal_tx_desc_cached, hal_tx_desc, num_desc_bytes);

	coalesce = dp_tx_attempt_coalescing(soc, vdev, tx_desc, tid,
					    msdu_info, ring_id);

	DP_STATS_INC_PKT(vdev, tx_i.processed, 1, dp_tx_get_pkt_len(tx_desc));
	DP_STATS_INC(soc, tx.tcl_enq[ring_id], 1);
	dp_tx_update_stats(soc, tx_desc, ring_id);
	status = QDF_STATUS_SUCCESS;

	dp_tx_hw_desc_update_evt((uint8_t *)hal_tx_desc_cached,
				 hal_ring_hdl, soc, ring_id);

ring_access_fail:
	dp_tx_ring_access_end_wrapper(soc, hal_ring_hdl, coalesce);
	dp_pkt_add_timestamp(vdev, QDF_PKT_TX_DRIVER_EXIT,
			     qdf_get_log_timestamp(), tx_desc->nbuf);
	return status;
}

#ifdef IPA_OFFLOAD
static void
dp_tx_get_ipa_bank_config(struct dp_soc_be *be_soc,
			  union hal_tx_bank_config *bank_config)
{
	bank_config->epd = 0;
	bank_config->encap_type = wlan_cfg_pkt_type(be_soc->soc.wlan_cfg_ctx);
	bank_config->encrypt_type = 0;

	bank_config->src_buffer_swap = 0;
	bank_config->link_meta_swap = 0;

	bank_config->index_lookup_enable = 0;
	bank_config->mcast_pkt_ctrl = HAL_TX_MCAST_CTRL_FW_EXCEPTION;
	bank_config->addrx_en = 1;
	bank_config->addry_en = 1;

	bank_config->mesh_enable = 0;
	bank_config->dscp_tid_map_id = 0;
	bank_config->vdev_id_check_en = 0;
	bank_config->pmac_id = 0;
}

static void dp_tx_init_ipa_bank_profile(struct dp_soc_be *be_soc)
{
	union hal_tx_bank_config ipa_config = {0};
	int bid;

	if (!wlan_cfg_is_ipa_enabled(be_soc->soc.wlan_cfg_ctx)) {
		be_soc->ipa_bank_id = DP_BE_INVALID_BANK_ID;
		return;
	}

	dp_tx_get_ipa_bank_config(be_soc, &ipa_config);

	/* Let IPA use last HOST owned bank */
	bid = be_soc->num_bank_profiles - 1;

	be_soc->bank_profiles[bid].is_configured = true;
	be_soc->bank_profiles[bid].bank_config.val = ipa_config.val;
	hal_tx_populate_bank_register(be_soc->soc.hal_soc,
				      &be_soc->bank_profiles[bid].bank_config,
				      bid);
	qdf_atomic_inc(&be_soc->bank_profiles[bid].ref_count);

	dp_info("IPA bank at slot %d config:0x%x", bid,
		be_soc->bank_profiles[bid].bank_config.val);

	be_soc->ipa_bank_id = bid;
}
#else /* !IPA_OFFLOAD */
static inline void dp_tx_init_ipa_bank_profile(struct dp_soc_be *be_soc)
{
}
#endif /* IPA_OFFLOAD */

QDF_STATUS dp_tx_init_bank_profiles(struct dp_soc_be *be_soc)
{
	int i, num_tcl_banks;

	num_tcl_banks = hal_tx_get_num_tcl_banks(be_soc->soc.hal_soc);

	qdf_assert_always(num_tcl_banks);
	be_soc->num_bank_profiles = num_tcl_banks;

	be_soc->bank_profiles = qdf_mem_malloc(num_tcl_banks *
					       sizeof(*be_soc->bank_profiles));
	if (!be_soc->bank_profiles) {
		dp_err("unable to allocate memory for DP TX Profiles!");
		return QDF_STATUS_E_NOMEM;
	}

	DP_TX_BANK_LOCK_CREATE(&be_soc->tx_bank_lock);

	for (i = 0; i < num_tcl_banks; i++) {
		be_soc->bank_profiles[i].is_configured = false;
		qdf_atomic_init(&be_soc->bank_profiles[i].ref_count);
	}
	dp_info("initialized %u bank profiles", be_soc->num_bank_profiles);

	dp_tx_init_ipa_bank_profile(be_soc);

	return QDF_STATUS_SUCCESS;
}

void dp_tx_deinit_bank_profiles(struct dp_soc_be *be_soc)
{
	qdf_mem_free(be_soc->bank_profiles);
	DP_TX_BANK_LOCK_DESTROY(&be_soc->tx_bank_lock);
}

static
void dp_tx_get_vdev_bank_config(struct dp_vdev_be *be_vdev,
				union hal_tx_bank_config *bank_config)
{
	struct dp_vdev *vdev = &be_vdev->vdev;

	bank_config->epd = 0;

	bank_config->encap_type = vdev->tx_encap_type;

	/* Only valid for raw frames. Needs work for RAW mode */
	if (vdev->tx_encap_type == htt_cmn_pkt_type_raw) {
		bank_config->encrypt_type = sec_type_map[vdev->sec_type];
	} else {
		bank_config->encrypt_type = 0;
	}

	bank_config->src_buffer_swap = 0;
	bank_config->link_meta_swap = 0;

	if ((vdev->search_type == HAL_TX_ADDR_INDEX_SEARCH) &&
	    vdev->opmode == wlan_op_mode_sta) {
		bank_config->index_lookup_enable = 1;
		bank_config->mcast_pkt_ctrl = HAL_TX_MCAST_CTRL_MEC_NOTIFY;
		bank_config->addrx_en = 0;
		bank_config->addry_en = 0;
	} else {
		bank_config->index_lookup_enable = 0;
		bank_config->mcast_pkt_ctrl = HAL_TX_MCAST_CTRL_FW_EXCEPTION;
		bank_config->addrx_en =
			(vdev->hal_desc_addr_search_flags &
			 HAL_TX_DESC_ADDRX_EN) ? 1 : 0;
		bank_config->addry_en =
			(vdev->hal_desc_addr_search_flags &
			 HAL_TX_DESC_ADDRY_EN) ? 1 : 0;
	}

	bank_config->mesh_enable = vdev->mesh_vdev ? 1 : 0;

	bank_config->dscp_tid_map_id = vdev->dscp_tid_map_id;

	/* Disabling vdev id check for now. Needs revist. */
	bank_config->vdev_id_check_en = be_vdev->vdev_id_check_en;

	bank_config->pmac_id = vdev->lmac_id;
}

int dp_tx_get_bank_profile(struct dp_soc_be *be_soc,
			   struct dp_vdev_be *be_vdev)
{
	char *temp_str = "";
	bool found_match = false;
	int bank_id = DP_BE_INVALID_BANK_ID;
	int i;
	int unconfigured_slot = DP_BE_INVALID_BANK_ID;
	int zero_ref_count_slot = DP_BE_INVALID_BANK_ID;
	union hal_tx_bank_config vdev_config = {0};

	/* convert vdev params into hal_tx_bank_config */
	dp_tx_get_vdev_bank_config(be_vdev, &vdev_config);

	DP_TX_BANK_LOCK_ACQUIRE(&be_soc->tx_bank_lock);
	/* go over all banks and find a matching/unconfigured/unused bank */
	for (i = 0; i < be_soc->num_bank_profiles; i++) {
		if (be_soc->bank_profiles[i].is_configured &&
		    (be_soc->bank_profiles[i].bank_config.val ^
						vdev_config.val) == 0) {
			found_match = true;
			break;
		}

		if (unconfigured_slot == DP_BE_INVALID_BANK_ID &&
		    !be_soc->bank_profiles[i].is_configured)
			unconfigured_slot = i;
		else if (zero_ref_count_slot  == DP_BE_INVALID_BANK_ID &&
		    !qdf_atomic_read(&be_soc->bank_profiles[i].ref_count))
			zero_ref_count_slot = i;
	}

	if (found_match) {
		temp_str = "matching";
		bank_id = i;
		goto inc_ref_and_return;
	}
	if (unconfigured_slot != DP_BE_INVALID_BANK_ID) {
		temp_str = "unconfigured";
		bank_id = unconfigured_slot;
		goto configure_and_return;
	}
	if (zero_ref_count_slot != DP_BE_INVALID_BANK_ID) {
		temp_str = "zero_ref_count";
		bank_id = zero_ref_count_slot;
	}
	if (bank_id == DP_BE_INVALID_BANK_ID) {
		dp_alert("unable to find TX bank!");
		QDF_BUG(0);
		return bank_id;
	}

configure_and_return:
	be_soc->bank_profiles[bank_id].is_configured = true;
	be_soc->bank_profiles[bank_id].bank_config.val = vdev_config.val;
	hal_tx_populate_bank_register(be_soc->soc.hal_soc,
				      &be_soc->bank_profiles[bank_id].bank_config,
				      bank_id);
inc_ref_and_return:
	qdf_atomic_inc(&be_soc->bank_profiles[bank_id].ref_count);
	DP_TX_BANK_LOCK_RELEASE(&be_soc->tx_bank_lock);

	dp_info("found %s slot at index %d, input:0x%x match:0x%x ref_count %u",
		temp_str, bank_id, vdev_config.val,
		be_soc->bank_profiles[bank_id].bank_config.val,
		qdf_atomic_read(&be_soc->bank_profiles[bank_id].ref_count));

	dp_info("epd:%x encap:%x encryp:%x src_buf_swap:%x link_meta_swap:%x addrx_en:%x addry_en:%x mesh_en:%x vdev_id_check:%x pmac_id:%x mcast_pkt_ctrl:%x",
		be_soc->bank_profiles[bank_id].bank_config.epd,
		be_soc->bank_profiles[bank_id].bank_config.encap_type,
		be_soc->bank_profiles[bank_id].bank_config.encrypt_type,
		be_soc->bank_profiles[bank_id].bank_config.src_buffer_swap,
		be_soc->bank_profiles[bank_id].bank_config.link_meta_swap,
		be_soc->bank_profiles[bank_id].bank_config.addrx_en,
		be_soc->bank_profiles[bank_id].bank_config.addry_en,
		be_soc->bank_profiles[bank_id].bank_config.mesh_enable,
		be_soc->bank_profiles[bank_id].bank_config.vdev_id_check_en,
		be_soc->bank_profiles[bank_id].bank_config.pmac_id,
		be_soc->bank_profiles[bank_id].bank_config.mcast_pkt_ctrl);

	return bank_id;
}

void dp_tx_put_bank_profile(struct dp_soc_be *be_soc,
			    struct dp_vdev_be *be_vdev)
{
	DP_TX_BANK_LOCK_ACQUIRE(&be_soc->tx_bank_lock);
	qdf_atomic_dec(&be_soc->bank_profiles[be_vdev->bank_id].ref_count);
	DP_TX_BANK_LOCK_RELEASE(&be_soc->tx_bank_lock);
}

void dp_tx_update_bank_profile(struct dp_soc_be *be_soc,
			       struct dp_vdev_be *be_vdev)
{
	dp_tx_put_bank_profile(be_soc, be_vdev);
	be_vdev->bank_id = dp_tx_get_bank_profile(be_soc, be_vdev);
	be_vdev->vdev.bank_id = be_vdev->bank_id;
}

QDF_STATUS dp_tx_desc_pool_init_be(struct dp_soc *soc,
				   uint32_t num_elem,
				   uint8_t pool_id)
{
	struct dp_tx_desc_pool_s *tx_desc_pool;
	struct dp_hw_cookie_conversion_t *cc_ctx;
	struct dp_soc_be *be_soc;
	struct dp_spt_page_desc *page_desc;
	struct dp_tx_desc_s *tx_desc;
	uint32_t ppt_idx = 0;
	uint32_t avail_entry_index = 0;

	if (!num_elem) {
		dp_err("desc_num 0 !!");
		return QDF_STATUS_E_FAILURE;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	tx_desc_pool = &soc->tx_desc[pool_id];
	cc_ctx  = &be_soc->tx_cc_ctx[pool_id];

	tx_desc = tx_desc_pool->freelist;
	page_desc = &cc_ctx->page_desc_base[0];
	while (tx_desc) {
		if (avail_entry_index == 0) {
			if (ppt_idx >= cc_ctx->total_page_num) {
				dp_alert("insufficient secondary page tables");
				qdf_assert_always(0);
			}
			page_desc = &cc_ctx->page_desc_base[ppt_idx++];
		}

		/* put each TX Desc VA to SPT pages and
		 * get corresponding ID
		 */
		DP_CC_SPT_PAGE_UPDATE_VA(page_desc->page_v_addr,
					 avail_entry_index,
					 tx_desc);
		tx_desc->id =
			dp_cc_desc_id_generate(page_desc->ppt_index,
					       avail_entry_index);
		tx_desc->pool_id = pool_id;
		dp_tx_desc_set_magic(tx_desc, DP_TX_MAGIC_PATTERN_FREE);
		tx_desc = tx_desc->next;
		avail_entry_index = (avail_entry_index + 1) &
					DP_CC_SPT_PAGE_MAX_ENTRIES_MASK;
	}

	return QDF_STATUS_SUCCESS;
}

void dp_tx_desc_pool_deinit_be(struct dp_soc *soc,
			       struct dp_tx_desc_pool_s *tx_desc_pool,
			       uint8_t pool_id)
{
	struct dp_spt_page_desc *page_desc;
	struct dp_soc_be *be_soc;
	int i = 0;
	struct dp_hw_cookie_conversion_t *cc_ctx;

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	cc_ctx  = &be_soc->tx_cc_ctx[pool_id];

	for (i = 0; i < cc_ctx->total_page_num; i++) {
		page_desc = &cc_ctx->page_desc_base[i];
		qdf_mem_zero(page_desc->page_v_addr, qdf_page_size);
	}
}

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
uint32_t dp_tx_comp_nf_handler(struct dp_intr *int_ctx, struct dp_soc *soc,
			       hal_ring_handle_t hal_ring_hdl, uint8_t ring_id,
			       uint32_t quota)
{
	struct dp_srng *tx_comp_ring = &soc->tx_comp_ring[ring_id];
	uint32_t work_done = 0;

	if (dp_srng_get_near_full_level(soc, tx_comp_ring) <
			DP_SRNG_THRESH_NEAR_FULL)
		return 0;

	qdf_atomic_set(&tx_comp_ring->near_full, 1);
	work_done++;

	return work_done;
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP) && \
	defined(WLAN_CONFIG_TX_DELAY)
#define PPDUID_GET_HW_LINK_ID(PPDU_ID, LINK_ID_OFFSET, LINK_ID_BITS) \
	(((PPDU_ID) >> (LINK_ID_OFFSET)) & ((1 << (LINK_ID_BITS)) - 1))

#define HW_TX_DELAY_MAX                       0x1000000
#define TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US    10
#define HW_TX_DELAY_MASK                      0x1FFFFFFF
#define TX_COMPL_BUFFER_TSTAMP_US(TSTAMP) \
	(((TSTAMP) << TX_COMPL_SHIFT_BUFFER_TIMESTAMP_US) & \
	 HW_TX_DELAY_MASK)

static inline
QDF_STATUS dp_mlo_compute_hw_delay_us(struct dp_soc *soc,
				      struct dp_vdev *vdev,
				      struct hal_tx_completion_status *ts,
				      uint32_t *delay_us)
{
	uint32_t ppdu_id;
	uint8_t link_id_offset, link_id_bits;
	uint8_t hw_link_id;
	uint32_t msdu_tqm_enqueue_tstamp_us, final_msdu_tqm_enqueue_tstamp_us;
	uint32_t msdu_compl_tsf_tstamp_us, final_msdu_compl_tsf_tstamp_us;
	uint32_t delay;
	int32_t delta_tsf2, delta_tqm;

	if (!ts->valid)
		return QDF_STATUS_E_INVAL;

	link_id_offset = soc->link_id_offset;
	link_id_bits = soc->link_id_bits;
	ppdu_id = ts->ppdu_id;
	hw_link_id = PPDUID_GET_HW_LINK_ID(ppdu_id, link_id_offset,
					   link_id_bits);

	msdu_tqm_enqueue_tstamp_us =
		TX_COMPL_BUFFER_TSTAMP_US(ts->buffer_timestamp);
	msdu_compl_tsf_tstamp_us = ts->tsf;

	delta_tsf2 = dp_mlo_get_delta_tsf2_wrt_mlo_offset(soc, hw_link_id);
	delta_tqm = dp_mlo_get_delta_tqm_wrt_mlo_offset(soc);

	final_msdu_tqm_enqueue_tstamp_us = (msdu_tqm_enqueue_tstamp_us +
			delta_tqm) & HW_TX_DELAY_MASK;

	final_msdu_compl_tsf_tstamp_us = (msdu_compl_tsf_tstamp_us +
			delta_tsf2) & HW_TX_DELAY_MASK;

	delay = (final_msdu_compl_tsf_tstamp_us -
		final_msdu_tqm_enqueue_tstamp_us) & HW_TX_DELAY_MASK;

	if (delay > HW_TX_DELAY_MAX)
		return QDF_STATUS_E_FAILURE;

	if (delay_us)
		*delay_us = delay;

	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS dp_mlo_compute_hw_delay_us(struct dp_soc *soc,
				      struct dp_vdev *vdev,
				      struct hal_tx_completion_status *ts,
				      uint32_t *delay_us)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS dp_tx_compute_tx_delay_be(struct dp_soc *soc,
				     struct dp_vdev *vdev,
				     struct hal_tx_completion_status *ts,
				     uint32_t *delay_us)
{
	return dp_mlo_compute_hw_delay_us(soc, vdev, ts, delay_us);
}

static inline
qdf_dma_addr_t dp_tx_nbuf_map_be(struct dp_vdev *vdev,
				 struct dp_tx_desc_s *tx_desc,
				 qdf_nbuf_t nbuf)
{
	qdf_nbuf_dma_clean_range_no_dsb((void *)nbuf->data,
					(void *)(nbuf->data + 256));

	return (qdf_dma_addr_t)qdf_mem_virt_to_phys(nbuf->data);
}

static inline
void dp_tx_nbuf_unmap_be(struct dp_soc *soc,
			 struct dp_tx_desc_s *desc)
{
}

/**
 * dp_tx_fast_send_be() - Transmit a frame on a given VAP
 * @soc: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @nbuf: skb
 *
 * Entry point for Core Tx layer (DP_TX) invoked from
 * hard_start_xmit in OSIF/HDD or from dp_rx_process for intravap forwarding
 * cases
 *
 * Return: NULL on success,
 *         nbuf when it fails to send
 */
#ifdef QCA_DP_TX_NBUF_LIST_FREE
qdf_nbuf_t dp_tx_fast_send_be(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			      qdf_nbuf_t nbuf)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = NULL;
	struct dp_pdev *pdev = NULL;
	struct dp_tx_desc_s *tx_desc;
	uint16_t desc_pool_id;
	uint16_t pkt_len;
	qdf_dma_addr_t paddr;
	QDF_STATUS status = QDF_STATUS_E_RESOURCES;
	uint8_t cached_desc[HAL_TX_DESC_LEN_BYTES] = { 0 };
	hal_ring_handle_t hal_ring_hdl = NULL;
	uint32_t *hal_tx_desc_cached;
	void *hal_tx_desc;
	uint8_t desc_size = DP_TX_FAST_DESC_SIZE;

	if (qdf_unlikely(vdev_id >= MAX_VDEV_CNT))
		return nbuf;

	vdev = soc->vdev_id_map[vdev_id];
	if (qdf_unlikely(!vdev))
		return nbuf;

	desc_pool_id = qdf_nbuf_get_queue_mapping(nbuf) & DP_TX_QUEUE_MASK;

	pkt_len = qdf_nbuf_headlen(nbuf);
	DP_STATS_INC_PKT(vdev, tx_i.rcvd, 1, pkt_len);
	DP_STATS_INC(vdev, tx_i.rcvd_in_fast_xmit_flow, 1);
	DP_STATS_INC(vdev, tx_i.rcvd_per_core[desc_pool_id], 1);

	pdev = vdev->pdev;
	if (dp_tx_limit_check(vdev, nbuf))
		return nbuf;

	tx_desc = dp_tx_desc_alloc(soc, desc_pool_id);

	if (qdf_unlikely(!tx_desc)) {
		DP_STATS_INC(vdev, tx_i.dropped.desc_na.num, 1);
		DP_STATS_INC(vdev, tx_i.dropped.desc_na_exc_alloc_fail.num, 1);
		return nbuf;
	}

	dp_tx_outstanding_inc(pdev);

	/* Initialize the SW tx descriptor */
	tx_desc->nbuf = nbuf;
	tx_desc->shinfo_addr = skb_end_pointer(nbuf);
	tx_desc->frm_type = dp_tx_frm_std;
	tx_desc->tx_encap_type = vdev->tx_encap_type;
	tx_desc->vdev_id = vdev_id;
	tx_desc->pdev = pdev;
	tx_desc->pkt_offset = 0;
	tx_desc->length = pkt_len;
	tx_desc->flags |= DP_TX_DESC_FLAG_SIMPLE;
	tx_desc->nbuf->fast_recycled = 1;

	if (nbuf->is_from_recycler && nbuf->fast_xmit)
		tx_desc->flags |= DP_TX_DESC_FLAG_FAST;

	paddr =  dp_tx_nbuf_map_be(vdev, tx_desc, nbuf);
	if (!paddr) {
		/* Handle failure */
		dp_err("qdf_nbuf_map failed");
		DP_STATS_INC(vdev, tx_i.dropped.dma_error, 1);
		goto release_desc;
	}

	tx_desc->dma_addr = paddr;

	hal_tx_desc_cached = (void *)cached_desc;
	hal_tx_desc_cached[0] = (uint32_t)tx_desc->dma_addr;
	hal_tx_desc_cached[1] = tx_desc->id <<
		TCL_DATA_CMD_BUF_ADDR_INFO_SW_BUFFER_COOKIE_LSB;

	/* bank_id */
	hal_tx_desc_cached[2] = vdev->bank_id << TCL_DATA_CMD_BANK_ID_LSB;
	hal_tx_desc_cached[3] = vdev->htt_tcl_metadata <<
		TCL_DATA_CMD_TCL_CMD_NUMBER_LSB;

	hal_tx_desc_cached[4] = tx_desc->length;
	/* l3 and l4 checksum enable */
	hal_tx_desc_cached[4] |= DP_TX_L3_L4_CSUM_ENABLE <<
		TCL_DATA_CMD_IPV4_CHECKSUM_EN_LSB;

	hal_tx_desc_cached[5] = vdev->lmac_id << TCL_DATA_CMD_PMAC_ID_LSB;
	hal_tx_desc_cached[5] |= vdev->vdev_id << TCL_DATA_CMD_VDEV_ID_LSB;

	if (vdev->opmode == wlan_op_mode_sta) {
		hal_tx_desc_cached[6] = vdev->bss_ast_idx |
			((vdev->bss_ast_hash & 0xF) <<
			 TCL_DATA_CMD_CACHE_SET_NUM_LSB);
		desc_size = DP_TX_FAST_DESC_SIZE + 4;
	}

	hal_ring_hdl = dp_tx_get_hal_ring_hdl(soc, desc_pool_id);

	if (qdf_unlikely(dp_tx_hal_ring_access_start(soc, hal_ring_hdl))) {
		dp_err("HAL RING Access Failed -- %pK", hal_ring_hdl);
		DP_STATS_INC(soc, tx.tcl_ring_full[desc_pool_id], 1);
		DP_STATS_INC(vdev, tx_i.dropped.enqueue_fail, 1);
		goto ring_access_fail2;
	}

	hal_tx_desc = hal_srng_src_get_next(soc->hal_soc, hal_ring_hdl);
	if (qdf_unlikely(!hal_tx_desc)) {
		dp_verbose_debug("TCL ring full ring_id:%d", desc_pool_id);
		DP_STATS_INC(soc, tx.tcl_ring_full[desc_pool_id], 1);
		DP_STATS_INC(vdev, tx_i.dropped.enqueue_fail, 1);
		goto ring_access_fail;
	}

	tx_desc->flags |= DP_TX_DESC_FLAG_QUEUED_TX;

	/* Sync cached descriptor with HW */
	qdf_mem_copy(hal_tx_desc, hal_tx_desc_cached, desc_size);
	qdf_dsb();

	DP_STATS_INC_PKT(vdev, tx_i.processed, 1, tx_desc->length);
	DP_STATS_INC(soc, tx.tcl_enq[desc_pool_id], 1);
	status = QDF_STATUS_SUCCESS;

ring_access_fail:
	dp_tx_ring_access_end_wrapper(soc, hal_ring_hdl, 0);

ring_access_fail2:
	if (status != QDF_STATUS_SUCCESS) {
		dp_tx_nbuf_unmap_be(soc, tx_desc);
		goto release_desc;
	}

	return NULL;

release_desc:
	dp_tx_desc_release(tx_desc, desc_pool_id);

	return nbuf;
}
#endif
