/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "hal_hw_headers.h"
#include "dp_types.h"
#include "dp_rx.h"
#include "dp_peer.h"
#include "hal_rx.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "qdf_nbuf.h"
#include "hal_api_mon.h"
#include "dp_htt.h"
#include "dp_mon.h"
#include "dp_rx_mon.h"
#include "wlan_cfg.h"
#include "dp_internal.h"
#include "dp_rx_buffer_pool.h"
#include <dp_mon_1.0.h>
#include <dp_rx_mon_1.0.h>

#ifdef WLAN_TX_PKT_CAPTURE_ENH
#include "dp_rx_mon_feature.h"
#endif

/*
 * PPDU id is from 0 to 64k-1. PPDU id read from status ring and PPDU id
 * read from destination ring shall track each other. If the distance of
 * two ppdu id is less than 20000. It is assume no wrap around. Otherwise,
 * It is assume wrap around.
 */
#define NOT_PPDU_ID_WRAP_AROUND 20000
/*
 * The destination ring processing is stuck if the destrination is not
 * moving while status ring moves 16 ppdu. the destination ring processing
 * skips this destination ring ppdu as walkaround
 */
#define MON_DEST_RING_STUCK_MAX_CNT 16

#ifdef WLAN_TX_PKT_CAPTURE_ENH
void
dp_handle_tx_capture(struct dp_soc *soc, struct dp_pdev *pdev,
		     qdf_nbuf_t mon_mpdu)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct hal_rx_ppdu_info *ppdu_info = &mon_pdev->ppdu_info;

	if (mon_pdev->tx_capture_enabled
	    == CDP_TX_ENH_CAPTURE_DISABLED)
		return;

	if ((ppdu_info->sw_frame_group_id ==
	      HAL_MPDU_SW_FRAME_GROUP_CTRL_NDPA) ||
	     (ppdu_info->sw_frame_group_id ==
	      HAL_MPDU_SW_FRAME_GROUP_CTRL_BAR))
		dp_handle_tx_capture_from_dest(soc, pdev, mon_mpdu);
}

#ifdef QCA_MONITOR_PKT_SUPPORT
static void
dp_tx_capture_get_user_id(struct dp_pdev *dp_pdev, void *rx_desc_tlv)
{
	struct dp_mon_pdev *mon_pdev = dp_pdev->monitor_pdev;

	if (mon_pdev->tx_capture_enabled
	    != CDP_TX_ENH_CAPTURE_DISABLED)
		mon_pdev->ppdu_info.rx_info.user_id =
			hal_rx_hw_desc_mpdu_user_id(dp_pdev->soc->hal_soc,
						    rx_desc_tlv);
}
#endif
#else
static void
dp_tx_capture_get_user_id(struct dp_pdev *dp_pdev, void *rx_desc_tlv)
{
}
#endif

#ifdef QCA_MONITOR_PKT_SUPPORT
/**
 * dp_rx_mon_link_desc_return() - Return a MPDU link descriptor to HW
 *			      (WBM), following error handling
 *
 * @dp_pdev: core txrx pdev context
 * @buf_addr_info: void pointer to monitor link descriptor buf addr info
 * @mac_id: mac_id for which the link desc is released.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_rx_mon_link_desc_return(struct dp_pdev *dp_pdev,
	hal_buff_addrinfo_t buf_addr_info, int mac_id)
{
	struct dp_srng *dp_srng;
	hal_ring_handle_t hal_ring_hdl;
	hal_soc_handle_t hal_soc;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	void *src_srng_desc;

	hal_soc = dp_pdev->soc->hal_soc;

	dp_srng = &dp_pdev->soc->rxdma_mon_desc_ring[mac_id];
	hal_ring_hdl = dp_srng->hal_srng;

	qdf_assert(hal_ring_hdl);

	if (qdf_unlikely(hal_srng_access_start(hal_soc, hal_ring_hdl))) {

		/* TODO */
		/*
		 * Need API to convert from hal_ring pointer to
		 * Ring Type / Ring Id combo
		 */
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"%s %d : \
			HAL RING Access For WBM Release SRNG Failed -- %pK",
			__func__, __LINE__, hal_ring_hdl);
		goto done;
	}

	src_srng_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);

	if (qdf_likely(src_srng_desc)) {
		/* Return link descriptor through WBM ring (SW2WBM)*/
		hal_rx_mon_msdu_link_desc_set(hal_soc,
				src_srng_desc, buf_addr_info);
		status = QDF_STATUS_SUCCESS;
	} else {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"%s %d -- Monitor Link Desc WBM Release Ring Full",
			__func__, __LINE__);
	}
done:
	hal_srng_access_end(hal_soc, hal_ring_hdl);
	return status;
}

/**
 * dp_rx_mon_mpdu_pop() - Return a MPDU link descriptor to HW
 *			      (WBM), following error handling
 *
 * @soc: core DP main context
 * @mac_id: mac id which is one of 3 mac_ids
 * @rxdma_dst_ring_desc: void pointer to monitor link descriptor buf addr info
 * @head_msdu: head of msdu to be popped
 * @tail_msdu: tail of msdu to be popped
 * @npackets: number of packet to be popped
 * @ppdu_id: ppdu id of processing ppdu
 * @head: head of descs list to be freed
 * @tail: tail of decs list to be freed
 *
 * Return: number of msdu in MPDU to be popped
 */
static inline uint32_t
dp_rx_mon_mpdu_pop(struct dp_soc *soc, uint32_t mac_id,
	hal_rxdma_desc_t rxdma_dst_ring_desc, qdf_nbuf_t *head_msdu,
	qdf_nbuf_t *tail_msdu, uint32_t *npackets, uint32_t *ppdu_id,
	union dp_rx_desc_list_elem_t **head,
	union dp_rx_desc_list_elem_t **tail)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	void *rx_desc_tlv, *first_rx_desc_tlv = NULL;
	void *rx_msdu_link_desc;
	qdf_nbuf_t msdu;
	qdf_nbuf_t last;
	struct hal_rx_msdu_list msdu_list;
	uint16_t num_msdus;
	uint32_t rx_buf_size, rx_pkt_offset;
	struct hal_buf_info buf_info;
	uint32_t rx_bufs_used = 0;
	uint32_t msdu_ppdu_id, msdu_cnt;
	uint8_t *data = NULL;
	uint32_t i;
	uint32_t total_frag_len = 0, frag_len = 0;
	bool is_frag, is_first_msdu;
	bool drop_mpdu = false, is_frag_non_raw = false;
	uint8_t bm_action = HAL_BM_ACTION_PUT_IN_IDLE_LIST;
	qdf_dma_addr_t buf_paddr = 0;
	uint32_t rx_link_buf_info[HAL_RX_BUFFINFO_NUM_DWORDS];
	struct cdp_mon_status *rs;
	struct dp_mon_pdev *mon_pdev;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_mon_dest_debug("%pK: pdev is null for mac_id = %d", soc, mac_id);
		return rx_bufs_used;
	}

	mon_pdev = dp_pdev->monitor_pdev;
	msdu = 0;

	last = NULL;

	hal_rx_reo_ent_buf_paddr_get(soc->hal_soc, rxdma_dst_ring_desc,
				     &buf_info, &msdu_cnt);

	rs = &mon_pdev->rx_mon_recv_status;
	rs->cdp_rs_rxdma_err = false;
	if ((hal_rx_reo_ent_rxdma_push_reason_get(rxdma_dst_ring_desc) ==
		HAL_RX_WBM_RXDMA_PSH_RSN_ERROR)) {
		uint8_t rxdma_err =
			hal_rx_reo_ent_rxdma_error_code_get(
				rxdma_dst_ring_desc);
		if (qdf_unlikely((rxdma_err == HAL_RXDMA_ERR_FLUSH_REQUEST) ||
		   (rxdma_err == HAL_RXDMA_ERR_MPDU_LENGTH) ||
		   (rxdma_err == HAL_RXDMA_ERR_OVERFLOW) ||
		   (rxdma_err == HAL_RXDMA_ERR_FCS && mon_pdev->mcopy_mode) ||
		   (rxdma_err == HAL_RXDMA_ERR_FCS &&
		    mon_pdev->rx_pktlog_cbf))) {
			drop_mpdu = true;
			mon_pdev->rx_mon_stats.dest_mpdu_drop++;
		}
		rs->cdp_rs_rxdma_err = true;
	}

	is_frag = false;
	is_first_msdu = true;

	do {
		/* WAR for duplicate link descriptors received from HW */
		if (qdf_unlikely(mon_pdev->mon_last_linkdesc_paddr ==
		    buf_info.paddr)) {
			mon_pdev->rx_mon_stats.dup_mon_linkdesc_cnt++;
			return rx_bufs_used;
		}

		rx_msdu_link_desc =
			dp_rx_cookie_2_mon_link_desc(dp_pdev,
						     buf_info, mac_id);

		qdf_assert_always(rx_msdu_link_desc);

		hal_rx_msdu_list_get(soc->hal_soc, rx_msdu_link_desc,
				     &msdu_list, &num_msdus);

		for (i = 0; i < num_msdus; i++) {
			uint16_t l2_hdr_offset;
			struct dp_rx_desc *rx_desc = NULL;
			struct rx_desc_pool *rx_desc_pool;

			rx_desc = dp_rx_get_mon_desc(soc,
						     msdu_list.sw_cookie[i]);

			qdf_assert_always(rx_desc);

			msdu = DP_RX_MON_GET_NBUF_FROM_DESC(rx_desc);
			buf_paddr = dp_rx_mon_get_paddr_from_desc(rx_desc);

			/* WAR for duplicate buffers received from HW */
			if (qdf_unlikely(mon_pdev->mon_last_buf_cookie ==
				msdu_list.sw_cookie[i] ||
				DP_RX_MON_IS_BUFFER_ADDR_NULL(rx_desc) ||
				msdu_list.paddr[i] != buf_paddr ||
				!rx_desc->in_use)) {
				/* Skip duplicate buffer and drop subsequent
				 * buffers in this MPDU
				 */
				drop_mpdu = true;
				mon_pdev->rx_mon_stats.dup_mon_buf_cnt++;
				mon_pdev->mon_last_linkdesc_paddr =
					buf_info.paddr;
				continue;
			}

			if (rx_desc->unmapped == 0) {
				rx_desc_pool = dp_rx_get_mon_desc_pool(soc,
								       mac_id,
								dp_pdev->pdev_id);
				dp_rx_mon_buffer_unmap(soc, rx_desc,
						       rx_desc_pool->buf_size);
				rx_desc->unmapped = 1;
			}

			if (dp_rx_buffer_pool_refill(soc, msdu,
						     rx_desc->pool_id)) {
				drop_mpdu = true;
				msdu = NULL;
				mon_pdev->mon_last_linkdesc_paddr =
					buf_info.paddr;
				goto next_msdu;
			}

			if (drop_mpdu) {
				mon_pdev->mon_last_linkdesc_paddr =
					buf_info.paddr;
				dp_rx_mon_buffer_free(rx_desc);
				msdu = NULL;
				goto next_msdu;
			}

			data = dp_rx_mon_get_buffer_data(rx_desc);
			rx_desc_tlv = HAL_RX_MON_DEST_GET_DESC(data);

			dp_rx_mon_dest_debug("%pK: i=%d, ppdu_id=%x, num_msdus = %u",
					     soc, i, *ppdu_id, num_msdus);

			if (is_first_msdu) {
				if (!hal_rx_mpdu_start_tlv_tag_valid(
						soc->hal_soc,
						rx_desc_tlv)) {
					drop_mpdu = true;
					dp_rx_mon_buffer_free(rx_desc);
					msdu = NULL;
					mon_pdev->mon_last_linkdesc_paddr =
						buf_info.paddr;
					goto next_msdu;
				}

				msdu_ppdu_id = hal_rx_hw_desc_get_ppduid_get(
						soc->hal_soc,
						rx_desc_tlv,
						rxdma_dst_ring_desc);
				is_first_msdu = false;

				dp_rx_mon_dest_debug("%pK: msdu_ppdu_id=%x",
						     soc, msdu_ppdu_id);

				if (*ppdu_id > msdu_ppdu_id)
					dp_rx_mon_dest_debug("%pK: ppdu_id=%d "
							     "msdu_ppdu_id=%d", soc,
							     *ppdu_id, msdu_ppdu_id);

				if ((*ppdu_id < msdu_ppdu_id) && (
					(msdu_ppdu_id - *ppdu_id) <
						NOT_PPDU_ID_WRAP_AROUND)) {
					*ppdu_id = msdu_ppdu_id;
					return rx_bufs_used;
				} else if ((*ppdu_id > msdu_ppdu_id) && (
					(*ppdu_id - msdu_ppdu_id) >
						NOT_PPDU_ID_WRAP_AROUND)) {
					*ppdu_id = msdu_ppdu_id;
					return rx_bufs_used;
				}

				dp_tx_capture_get_user_id(dp_pdev,
							  rx_desc_tlv);

				if (*ppdu_id == msdu_ppdu_id)
					mon_pdev->rx_mon_stats.ppdu_id_match++;
				else
					mon_pdev->rx_mon_stats.ppdu_id_mismatch
						++;

				mon_pdev->mon_last_linkdesc_paddr =
					buf_info.paddr;

				if (dp_rx_mon_alloc_parent_buffer(head_msdu)
				    != QDF_STATUS_SUCCESS) {
					DP_STATS_INC(dp_pdev,
						     replenish.nbuf_alloc_fail,
						     1);
					qdf_frag_free(rx_desc_tlv);
					dp_rx_mon_dest_debug("failed to allocate parent buffer to hold all frag");
					drop_mpdu = true;
					goto next_msdu;
				}
			}

			if (hal_rx_desc_is_first_msdu(soc->hal_soc,
						      rx_desc_tlv))
				hal_rx_mon_hw_desc_get_mpdu_status(soc->hal_soc,
					rx_desc_tlv,
					&mon_pdev->ppdu_info.rx_status);

			dp_rx_mon_parse_desc_buffer(soc,
						    &(msdu_list.msdu_info[i]),
						    &is_frag,
						    &total_frag_len,
						    &frag_len,
						    &l2_hdr_offset,
						    rx_desc_tlv,
						    &first_rx_desc_tlv,
						    &is_frag_non_raw, data);
			if (!is_frag)
				msdu_cnt--;

			dp_rx_mon_dest_debug("total_len %u frag_len %u flags %u",
					     total_frag_len, frag_len,
				      msdu_list.msdu_info[i].msdu_flags);

			rx_pkt_offset = dp_rx_mon_get_rx_pkt_tlv_size(soc);

			rx_buf_size = rx_pkt_offset + l2_hdr_offset
					+ frag_len;

			dp_rx_mon_buffer_set_pktlen(msdu, rx_buf_size);
#if 0
			/* Disable it.see packet on msdu done set to 0 */
			/*
			 * Check if DMA completed -- msdu_done is the
			 * last bit to be written
			 */
			if (!hal_rx_attn_msdu_done_get(rx_desc_tlv)) {

				QDF_TRACE(QDF_MODULE_ID_DP,
					  QDF_TRACE_LEVEL_ERROR,
					  "%s:%d: Pkt Desc",
					  __func__, __LINE__);

				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP,
					QDF_TRACE_LEVEL_ERROR,
					rx_desc_tlv, 128);

				qdf_assert_always(0);
			}
#endif
			dp_rx_mon_dest_debug("%pK: rx_pkt_offset=%d, l2_hdr_offset=%d, msdu_len=%d, frag_len %u",
					     soc, rx_pkt_offset, l2_hdr_offset,
					     msdu_list.msdu_info[i].msdu_len,
					     frag_len);

			if (dp_rx_mon_add_msdu_to_list(soc, head_msdu, msdu,
						       &last, rx_desc_tlv,
						       frag_len, l2_hdr_offset)
					!= QDF_STATUS_SUCCESS) {
				dp_rx_mon_add_msdu_to_list_failure_handler(rx_desc_tlv,
						dp_pdev, &last, head_msdu,
						tail_msdu, __func__);
				drop_mpdu = true;
				goto next_msdu;
			}

next_msdu:
			mon_pdev->mon_last_buf_cookie = msdu_list.sw_cookie[i];
			rx_bufs_used++;
			dp_rx_add_to_free_desc_list(head,
				tail, rx_desc);
		}

		/*
		 * Store the current link buffer into to the local
		 * structure to be  used for release purpose.
		 */
		hal_rxdma_buff_addr_info_set(soc->hal_soc, rx_link_buf_info,
					     buf_info.paddr,
					     buf_info.sw_cookie, buf_info.rbm);

		hal_rx_mon_next_link_desc_get(soc->hal_soc, rx_msdu_link_desc,
					      &buf_info);
		if (dp_rx_monitor_link_desc_return(dp_pdev,
						   (hal_buff_addrinfo_t)
						   rx_link_buf_info,
						   mac_id,
						   bm_action)
						   != QDF_STATUS_SUCCESS)
			dp_err_rl("monitor link desc return failed");
	} while (buf_info.paddr && msdu_cnt);

	dp_rx_mon_init_tail_msdu(head_msdu, msdu, last, tail_msdu);
	dp_rx_mon_remove_raw_frame_fcs_len(soc, head_msdu, tail_msdu);

	return rx_bufs_used;
}

#if !defined(DISABLE_MON_CONFIG) && \
	(defined(MON_ENABLE_DROP_FOR_NON_MON_PMAC) || \
	 defined(MON_ENABLE_DROP_FOR_MAC))
/**
 * dp_rx_mon_drop_one_mpdu() - Drop one mpdu from one rxdma monitor destination
 *			       ring.
 * @pdev: DP pdev handle
 * @mac_id: MAC id which is being currently processed
 * @rxdma_dst_ring_desc: RXDMA monitor destination ring entry
 * @head: HEAD if the rx_desc list to be freed
 * @tail: TAIL of the rx_desc list to be freed
 *
 * Return: Number of msdus which are dropped.
 */
static int dp_rx_mon_drop_one_mpdu(struct dp_pdev *pdev,
				   uint32_t mac_id,
				   hal_rxdma_desc_t rxdma_dst_ring_desc,
				   union dp_rx_desc_list_elem_t **head,
				   union dp_rx_desc_list_elem_t **tail)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_soc *soc = pdev->soc;
	hal_soc_handle_t hal_soc = soc->hal_soc;
	struct hal_buf_info buf_info;
	uint32_t msdu_count = 0;
	uint32_t rx_bufs_used = 0;
	void *rx_msdu_link_desc;
	struct hal_rx_msdu_list msdu_list;
	uint16_t num_msdus;
	qdf_nbuf_t nbuf;
	uint32_t i;
	uint8_t bm_action = HAL_BM_ACTION_PUT_IN_IDLE_LIST;
	uint32_t rx_link_buf_info[HAL_RX_BUFFINFO_NUM_DWORDS];
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = dp_rx_get_mon_desc_pool(soc, mac_id, pdev->pdev_id);
	hal_rx_reo_ent_buf_paddr_get(hal_soc, rxdma_dst_ring_desc,
				     &buf_info, &msdu_count);

	do {
		rx_msdu_link_desc = dp_rx_cookie_2_mon_link_desc(pdev,
								 buf_info,
								 mac_id);
		if (qdf_unlikely(!rx_msdu_link_desc)) {
			mon_pdev->rx_mon_stats.mon_link_desc_invalid++;
			return rx_bufs_used;
		}

		hal_rx_msdu_list_get(soc->hal_soc, rx_msdu_link_desc,
				     &msdu_list, &num_msdus);

		for (i = 0; i < num_msdus; i++) {
			struct dp_rx_desc *rx_desc;
			qdf_dma_addr_t buf_paddr;

			rx_desc = dp_rx_get_mon_desc(soc,
						     msdu_list.sw_cookie[i]);

			if (qdf_unlikely(!rx_desc)) {
				mon_pdev->rx_mon_stats.
						mon_rx_desc_invalid++;
				continue;
			}

			nbuf = DP_RX_MON_GET_NBUF_FROM_DESC(rx_desc);
			buf_paddr =
				 dp_rx_mon_get_paddr_from_desc(rx_desc);

			if (qdf_unlikely(!rx_desc->in_use || !nbuf ||
					 msdu_list.paddr[i] !=
					 buf_paddr)) {
				mon_pdev->rx_mon_stats.
						mon_nbuf_sanity_err++;
				continue;
			}
			rx_bufs_used++;

			if (!rx_desc->unmapped) {
				dp_rx_mon_buffer_unmap(soc, rx_desc,
						       rx_desc_pool->buf_size);
				rx_desc->unmapped = 1;
			}

			qdf_nbuf_free(nbuf);
			dp_rx_add_to_free_desc_list(head, tail, rx_desc);

			if (!(msdu_list.msdu_info[i].msdu_flags &
			      HAL_MSDU_F_MSDU_CONTINUATION))
				msdu_count--;
		}

		/*
		 * Store the current link buffer into to the local
		 * structure to be  used for release purpose.
		 */
		hal_rxdma_buff_addr_info_set(soc->hal_soc,
					     rx_link_buf_info,
					     buf_info.paddr,
					     buf_info.sw_cookie,
					     buf_info.rbm);

		hal_rx_mon_next_link_desc_get(soc->hal_soc,
					      rx_msdu_link_desc,
					      &buf_info);
		if (dp_rx_monitor_link_desc_return(pdev,
						   (hal_buff_addrinfo_t)
						   rx_link_buf_info,
						   mac_id, bm_action) !=
		    QDF_STATUS_SUCCESS)
			dp_info_rl("monitor link desc return failed");
	} while (buf_info.paddr && msdu_count);

	return rx_bufs_used;
}
#endif

#if !defined(DISABLE_MON_CONFIG) && defined(MON_ENABLE_DROP_FOR_NON_MON_PMAC)
/**
 * dp_rx_mon_check_n_drop_mpdu() - Check if the current MPDU is not from the
 *				   PMAC which is being currently processed, and
 *				   if yes, drop the MPDU.
 * @pdev: DP pdev handle
 * @mac_id: MAC id which is being currently processed
 * @rxdma_dst_ring_desc: RXDMA monitor destination ring entry
 * @head: HEAD if the rx_desc list to be freed
 * @tail: TAIL of the rx_desc list to be freed
 * @rx_bufs_dropped: Number of msdus dropped
 *
 * Return: QDF_STATUS_SUCCESS, if the mpdu was to be dropped
 *	   QDF_STATUS_E_INVAL/QDF_STATUS_E_FAILURE, if the mdpu was not dropped
 */
static QDF_STATUS
dp_rx_mon_check_n_drop_mpdu(struct dp_pdev *pdev, uint32_t mac_id,
			    hal_rxdma_desc_t rxdma_dst_ring_desc,
			    union dp_rx_desc_list_elem_t **head,
			    union dp_rx_desc_list_elem_t **tail,
			    uint32_t *rx_bufs_dropped)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	uint32_t lmac_id = DP_MON_INVALID_LMAC_ID;
	uint8_t src_link_id;
	QDF_STATUS status;

	if (mon_pdev->mon_chan_band == REG_BAND_UNKNOWN)
		goto drop_mpdu;

	lmac_id = pdev->ch_band_lmac_id_mapping[mon_pdev->mon_chan_band];

	status = hal_rx_reo_ent_get_src_link_id(soc->hal_soc,
						rxdma_dst_ring_desc,
						&src_link_id);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	if (src_link_id == lmac_id)
		return QDF_STATUS_E_INVAL;

drop_mpdu:
	*rx_bufs_dropped = dp_rx_mon_drop_one_mpdu(pdev, mac_id,
						   rxdma_dst_ring_desc,
						   head, tail);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_rx_mon_check_n_drop_mpdu(struct dp_pdev *pdev, uint32_t mac_id,
			    hal_rxdma_desc_t rxdma_dst_ring_desc,
			    union dp_rx_desc_list_elem_t **head,
			    union dp_rx_desc_list_elem_t **tail,
			    uint32_t *rx_bufs_dropped)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

void dp_rx_mon_dest_process(struct dp_soc *soc, struct dp_intr *int_ctx,
			    uint32_t mac_id, uint32_t quota)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	uint8_t pdev_id;
	hal_rxdma_desc_t rxdma_dst_ring_desc;
	hal_soc_handle_t hal_soc;
	void *mon_dst_srng;
	union dp_rx_desc_list_elem_t *head = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;
	uint32_t ppdu_id;
	uint32_t rx_bufs_used;
	uint32_t mpdu_rx_bufs_used;
	int mac_for_pdev = mac_id;
	struct cdp_pdev_mon_stats *rx_mon_stats;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_rx_mon_dest_debug("%pK: pdev is null for mac_id = %d", soc, mac_id);
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_dst_srng = dp_rxdma_get_mon_dst_ring(pdev, mac_for_pdev);

	if (!mon_dst_srng || !hal_srng_initialized(mon_dst_srng)) {
		dp_rx_mon_dest_err("%pK: : HAL Monitor Destination Ring Init Failed -- %pK",
				   soc, mon_dst_srng);
		return;
	}

	hal_soc = soc->hal_soc;

	qdf_assert((hal_soc && pdev));

	qdf_spin_lock_bh(&mon_pdev->mon_lock);

	if (qdf_unlikely(dp_srng_access_start(int_ctx, soc, mon_dst_srng))) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s %d : HAL Mon Dest Ring access Failed -- %pK",
			  __func__, __LINE__, mon_dst_srng);
		qdf_spin_unlock_bh(&mon_pdev->mon_lock);
		return;
	}

	pdev_id = pdev->pdev_id;
	ppdu_id = mon_pdev->ppdu_info.com_info.ppdu_id;
	rx_bufs_used = 0;
	rx_mon_stats = &mon_pdev->rx_mon_stats;

	while (qdf_likely(rxdma_dst_ring_desc =
		hal_srng_dst_peek(hal_soc, mon_dst_srng))) {
		qdf_nbuf_t head_msdu, tail_msdu;
		uint32_t npackets;
		uint32_t rx_bufs_dropped;

		rx_bufs_dropped = 0;
		head_msdu = (qdf_nbuf_t)NULL;
		tail_msdu = (qdf_nbuf_t)NULL;

		if (QDF_STATUS_SUCCESS ==
		    dp_rx_mon_check_n_drop_mpdu(pdev, mac_id,
						rxdma_dst_ring_desc,
						&head, &tail,
						&rx_bufs_dropped)) {
			/* Increment stats */
			rx_bufs_used += rx_bufs_dropped;
			hal_srng_dst_get_next(hal_soc, mon_dst_srng);
			continue;
		}

		mpdu_rx_bufs_used =
			dp_rx_mon_mpdu_pop(soc, mac_id,
					   rxdma_dst_ring_desc,
					   &head_msdu, &tail_msdu,
					   &npackets, &ppdu_id,
					   &head, &tail);

		rx_bufs_used += mpdu_rx_bufs_used;

		if (mpdu_rx_bufs_used)
			mon_pdev->mon_dest_ring_stuck_cnt = 0;
		else
			mon_pdev->mon_dest_ring_stuck_cnt++;

		if (mon_pdev->mon_dest_ring_stuck_cnt >
		    MON_DEST_RING_STUCK_MAX_CNT) {
			dp_info("destination ring stuck");
			dp_info("ppdu_id status=%d dest=%d",
				mon_pdev->ppdu_info.com_info.ppdu_id, ppdu_id);
			rx_mon_stats->mon_rx_dest_stuck++;
			mon_pdev->ppdu_info.com_info.ppdu_id = ppdu_id;
			continue;
		}

		if (ppdu_id != mon_pdev->ppdu_info.com_info.ppdu_id) {
			rx_mon_stats->stat_ring_ppdu_id_hist[
				rx_mon_stats->ppdu_id_hist_idx] =
				mon_pdev->ppdu_info.com_info.ppdu_id;
			rx_mon_stats->dest_ring_ppdu_id_hist[
				rx_mon_stats->ppdu_id_hist_idx] = ppdu_id;
			rx_mon_stats->ppdu_id_hist_idx =
				(rx_mon_stats->ppdu_id_hist_idx + 1) &
					(MAX_PPDU_ID_HIST - 1);
			mon_pdev->mon_ppdu_status = DP_PPDU_STATUS_START;
			qdf_mem_zero(&mon_pdev->ppdu_info.rx_status,
				     sizeof(mon_pdev->ppdu_info.rx_status));
			dp_rx_mon_dest_debug("%pK: ppdu_id %x != ppdu_info.com_info.ppdu_id %x",
					     soc, ppdu_id,
					     mon_pdev->ppdu_info.com_info.ppdu_id);
			break;
		}

		if (qdf_likely((head_msdu) && (tail_msdu))) {
			rx_mon_stats->dest_mpdu_done++;
			dp_rx_mon_deliver(soc, mac_id, head_msdu, tail_msdu);
		}

		rxdma_dst_ring_desc =
			hal_srng_dst_get_next(hal_soc,
					      mon_dst_srng);
	}

	dp_srng_access_end(int_ctx, soc, mon_dst_srng);

	qdf_spin_unlock_bh(&mon_pdev->mon_lock);

	if (rx_bufs_used) {
		rx_mon_stats->dest_ppdu_done++;
		dp_rx_buffers_replenish(soc, mac_id,
					dp_rxdma_get_mon_buf_ring(pdev,
								  mac_for_pdev),
					dp_rx_get_mon_desc_pool(soc, mac_id,
								pdev_id),
					rx_bufs_used, &head, &tail, false);
	}
}

QDF_STATUS
dp_rx_pdev_mon_buf_buffers_alloc(struct dp_pdev *pdev, uint32_t mac_id,
				 bool delayed_replenish)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct dp_srng *mon_buf_ring;
	uint32_t num_entries;
	struct rx_desc_pool *rx_desc_pool;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;

	mon_buf_ring = dp_rxdma_get_mon_buf_ring(pdev, mac_id);

	num_entries = mon_buf_ring->num_entries;

	rx_desc_pool = dp_rx_get_mon_desc_pool(soc, mac_id, pdev_id);

	dp_debug("Mon RX Desc Pool[%d] entries=%u", pdev_id, num_entries);

	/* Replenish RXDMA monitor buffer ring with 8 buffers only
	 * delayed_replenish_entries is actually 8 but when we call
	 * dp_pdev_rx_buffers_attach() we pass 1 less than 8, hence
	 * added 1 to delayed_replenish_entries to ensure we have 8
	 * entries. Once the monitor VAP is configured we replenish
	 * the complete RXDMA monitor buffer ring.
	 */
	if (delayed_replenish) {
		num_entries = soc_cfg_ctx->delayed_replenish_entries + 1;
		status = dp_pdev_rx_buffers_attach(soc, mac_id, mon_buf_ring,
						   rx_desc_pool,
						   num_entries - 1);
	} else {
		union dp_rx_desc_list_elem_t *tail = NULL;
		union dp_rx_desc_list_elem_t *desc_list = NULL;

		status = dp_rx_buffers_replenish(soc, mac_id,
						 mon_buf_ring,
						 rx_desc_pool,
						 num_entries,
						 &desc_list,
						 &tail, false);
	}

	return status;
}

void
dp_rx_pdev_mon_buf_desc_pool_init(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct dp_srng *mon_buf_ring;
	uint32_t num_entries;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t rx_desc_pool_size;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	mon_buf_ring = &soc->rxdma_mon_buf_ring[mac_id];

	num_entries = mon_buf_ring->num_entries;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	/* If descriptor pool is already initialized, do not initialize it */
	if (rx_desc_pool->freelist)
		return;

	dp_debug("Mon RX Desc buf Pool[%d] init entries=%u",
		 pdev_id, num_entries);

	rx_desc_pool_size = wlan_cfg_get_dp_soc_rx_sw_desc_weight(soc_cfg_ctx) *
		num_entries;

	rx_desc_pool->owner = HAL_RX_BUF_RBM_SW3_BM(soc->wbm_sw0_bm_id);
	rx_desc_pool->buf_size = RX_MONITOR_BUFFER_SIZE;
	rx_desc_pool->buf_alignment = RX_MONITOR_BUFFER_ALIGNMENT;
	/* Enable frag processing if feature is enabled */
	dp_rx_enable_mon_dest_frag(rx_desc_pool, true);

	dp_rx_desc_pool_init(soc, mac_id, rx_desc_pool_size, rx_desc_pool);

	mon_pdev->mon_last_linkdesc_paddr = 0;

	mon_pdev->mon_last_buf_cookie = DP_RX_DESC_COOKIE_MAX + 1;

	/* Attach full monitor mode resources */
	dp_full_mon_attach(pdev);
}

static void
dp_rx_pdev_mon_buf_desc_pool_deinit(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Desc buf Pool[%d] deinit", pdev_id);

	dp_rx_desc_pool_deinit(soc, rx_desc_pool, mac_id);

	/* Detach full monitor mode resources */
	dp_full_mon_detach(pdev);
}

static void
dp_rx_pdev_mon_buf_desc_pool_free(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Buf Desc Pool Free pdev[%d]", pdev_id);

	dp_rx_desc_pool_free(soc, rx_desc_pool);
}

void dp_rx_pdev_mon_buf_buffers_free(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Buf buffers Free pdev[%d]", pdev_id);

	if (rx_desc_pool->rx_mon_dest_frag_enable)
		dp_rx_desc_frag_free(soc, rx_desc_pool);
	else
		dp_rx_desc_nbuf_free(soc, rx_desc_pool, true);
}

QDF_STATUS
dp_rx_pdev_mon_buf_desc_pool_alloc(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct dp_srng *mon_buf_ring;
	uint32_t num_entries;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t rx_desc_pool_size;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;

	mon_buf_ring = &soc->rxdma_mon_buf_ring[mac_id];

	num_entries = mon_buf_ring->num_entries;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Desc Pool[%d] entries=%u",
		 pdev_id, num_entries);

	rx_desc_pool_size = wlan_cfg_get_dp_soc_rx_sw_desc_weight(soc_cfg_ctx) *
		num_entries;

	if (dp_rx_desc_pool_is_allocated(rx_desc_pool) == QDF_STATUS_SUCCESS)
		return QDF_STATUS_SUCCESS;

	return dp_rx_desc_pool_alloc(soc, rx_desc_pool_size, rx_desc_pool);
}

#if !defined(DISABLE_MON_CONFIG) && defined(MON_ENABLE_DROP_FOR_MAC)
uint32_t
dp_mon_dest_srng_drop_for_mac(struct dp_pdev *pdev, uint32_t mac_id,
			      bool force_flush)
{
	struct dp_soc *soc = pdev->soc;
	hal_rxdma_desc_t rxdma_dst_ring_desc;
	hal_soc_handle_t hal_soc;
	void *mon_dst_srng;
	union dp_rx_desc_list_elem_t *head = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;
	uint32_t rx_bufs_used = 0;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t reap_cnt = 0;
	uint32_t rx_bufs_dropped;
	struct dp_mon_pdev *mon_pdev;
	bool is_rxdma_dst_ring_common;

	if (qdf_unlikely(!soc || !soc->hal_soc))
		return reap_cnt;

	mon_dst_srng = dp_rxdma_get_mon_dst_ring(pdev, mac_id);

	if (qdf_unlikely(!mon_dst_srng || !hal_srng_initialized(mon_dst_srng)))
		return reap_cnt;

	hal_soc = soc->hal_soc;
	mon_pdev = pdev->monitor_pdev;

	qdf_spin_lock_bh(&mon_pdev->mon_lock);

	if (qdf_unlikely(hal_srng_access_start(hal_soc, mon_dst_srng))) {
		qdf_spin_unlock_bh(&mon_pdev->mon_lock);
		return reap_cnt;
	}

	rx_desc_pool = dp_rx_get_mon_desc_pool(soc, mac_id, pdev->pdev_id);
	is_rxdma_dst_ring_common = dp_is_rxdma_dst_ring_common(pdev);

	while ((rxdma_dst_ring_desc =
		hal_srng_dst_peek(hal_soc, mon_dst_srng)) &&
		(reap_cnt < MON_DROP_REAP_LIMIT || force_flush)) {
		if (is_rxdma_dst_ring_common && !force_flush) {
			if (QDF_STATUS_SUCCESS ==
			    dp_rx_mon_check_n_drop_mpdu(pdev, mac_id,
							rxdma_dst_ring_desc,
							&head, &tail,
							&rx_bufs_dropped)) {
				/* Increment stats */
				rx_bufs_used += rx_bufs_dropped;
			} else {
				/*
				 * If the mpdu was not dropped, we need to
				 * wait for the entry to be processed, along
				 * with the status ring entry for the other
				 * mac. Hence we bail out here.
				 */
				break;
			}
		} else {
			rx_bufs_used += dp_rx_mon_drop_one_mpdu(pdev, mac_id,
								rxdma_dst_ring_desc,
								&head, &tail);
		}
		reap_cnt++;
		rxdma_dst_ring_desc = hal_srng_dst_get_next(hal_soc,
							    mon_dst_srng);
	}

	hal_srng_access_end(hal_soc, mon_dst_srng);

	qdf_spin_unlock_bh(&mon_pdev->mon_lock);

	if (rx_bufs_used) {
		dp_rx_buffers_replenish(soc, mac_id,
					dp_rxdma_get_mon_buf_ring(pdev, mac_id),
					rx_desc_pool,
					rx_bufs_used, &head, &tail, false);
	}

	return reap_cnt;
}
#endif

static void
dp_rx_pdev_mon_dest_desc_pool_free(struct dp_pdev *pdev, int mac_for_pdev)
{
	struct dp_soc *soc = pdev->soc;

	dp_rx_pdev_mon_buf_desc_pool_free(pdev, mac_for_pdev);
	dp_hw_link_desc_pool_banks_free(soc, mac_for_pdev);
}

static void
dp_rx_pdev_mon_dest_desc_pool_deinit(struct dp_pdev *pdev, int mac_for_pdev)
{
	struct dp_soc *soc = pdev->soc;

	if (!soc->wlan_cfg_ctx->rxdma1_enable)
		return;

	dp_rx_pdev_mon_buf_desc_pool_deinit(pdev, mac_for_pdev);
}

static void
dp_rx_pdev_mon_dest_desc_pool_init(struct dp_pdev *pdev, uint32_t mac_for_pdev)
{
	struct dp_soc *soc = pdev->soc;

	if (!soc->wlan_cfg_ctx->rxdma1_enable ||
	    !wlan_cfg_is_delay_mon_replenish(soc->wlan_cfg_ctx))
		return;

	dp_rx_pdev_mon_buf_desc_pool_init(pdev, mac_for_pdev);
	dp_link_desc_ring_replenish(soc, mac_for_pdev);
}

static void
dp_rx_pdev_mon_dest_buffers_free(struct dp_pdev *pdev, int mac_for_pdev)
{
	struct dp_soc *soc = pdev->soc;

	if (!soc->wlan_cfg_ctx->rxdma1_enable)
		return;

	dp_rx_pdev_mon_buf_buffers_free(pdev, mac_for_pdev);
}

static QDF_STATUS
dp_rx_pdev_mon_dest_buffers_alloc(struct dp_pdev *pdev, int mac_for_pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;
	bool delayed_replenish;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	delayed_replenish = soc_cfg_ctx->delayed_replenish_entries ? 1 : 0;
	if (!soc->wlan_cfg_ctx->rxdma1_enable ||
	    !wlan_cfg_is_delay_mon_replenish(soc->wlan_cfg_ctx))
		return status;

	status = dp_rx_pdev_mon_buf_buffers_alloc(pdev, mac_for_pdev,
						  delayed_replenish);
	if (!QDF_IS_STATUS_SUCCESS(status))
		dp_err("dp_rx_pdev_mon_buf_desc_pool_alloc() failed");

	return status;
}

static QDF_STATUS
dp_rx_pdev_mon_dest_desc_pool_alloc(struct dp_pdev *pdev, uint32_t mac_for_pdev)
{
	struct dp_soc *soc = pdev->soc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!soc->wlan_cfg_ctx->rxdma1_enable ||
	    !wlan_cfg_is_delay_mon_replenish(soc->wlan_cfg_ctx))
		return status;

	/* Allocate sw rx descriptor pool for monitor RxDMA buffer ring */
	status = dp_rx_pdev_mon_buf_desc_pool_alloc(pdev, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("dp_rx_pdev_mon_buf_desc_pool_alloc() failed");
		goto fail;
	}

	/* Allocate link descriptors for the monitor link descriptor ring */
	status = dp_hw_link_desc_pool_banks_alloc(soc, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("dp_hw_link_desc_pool_banks_alloc() failed");
		goto mon_buf_dealloc;
	}

	return status;

mon_buf_dealloc:
	dp_rx_pdev_mon_status_desc_pool_free(pdev, mac_for_pdev);
fail:
	return status;
}
#else
static void
dp_rx_pdev_mon_dest_desc_pool_free(struct dp_pdev *pdev, int mac_for_pdev)
{
}

static void
dp_rx_pdev_mon_dest_desc_pool_deinit(struct dp_pdev *pdev, int mac_for_pdev)
{
}

static void
dp_rx_pdev_mon_dest_desc_pool_init(struct dp_pdev *pdev, uint32_t mac_for_pdev)
{
}

static void
dp_rx_pdev_mon_dest_buffers_free(struct dp_pdev *pdev, int mac_for_pdev)
{
}

static QDF_STATUS
dp_rx_pdev_mon_dest_buffers_alloc(struct dp_pdev *pdev, int mac_for_pdev)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dp_rx_pdev_mon_dest_desc_pool_alloc(struct dp_pdev *pdev, uint32_t mac_for_pdev)
{
	return QDF_STATUS_SUCCESS;
}

#if !defined(DISABLE_MON_CONFIG) && defined(MON_ENABLE_DROP_FOR_MAC)
uint32_t
dp_mon_dest_srng_drop_for_mac(struct dp_pdev *pdev, uint32_t mac_id)
{
	return 0;
}
#endif

#if !defined(DISABLE_MON_CONFIG) && defined(MON_ENABLE_DROP_FOR_NON_MON_PMAC)
static QDF_STATUS
dp_rx_mon_check_n_drop_mpdu(struct dp_pdev *pdev, uint32_t mac_id,
			    hal_rxdma_desc_t rxdma_dst_ring_desc,
			    union dp_rx_desc_list_elem_t **head,
			    union dp_rx_desc_list_elem_t **tail,
			    uint32_t *rx_bufs_dropped)
{
	return QDF_STATUS_E_FAILURE;
}
#endif
#endif

static void
dp_rx_pdev_mon_cmn_desc_pool_free(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t pdev_id = pdev->pdev_id;
	int mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev_id);

	dp_rx_pdev_mon_status_desc_pool_free(pdev, mac_for_pdev);
	dp_rx_pdev_mon_dest_desc_pool_free(pdev, mac_for_pdev);
}

void dp_rx_pdev_mon_desc_pool_free(struct dp_pdev *pdev)
{
	int mac_id;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_desc_pool_free(pdev, mac_id);
}

static void
dp_rx_pdev_mon_cmn_desc_pool_deinit(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t pdev_id = pdev->pdev_id;
	int mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev_id);

	dp_rx_pdev_mon_status_desc_pool_deinit(pdev, mac_for_pdev);

	dp_rx_pdev_mon_dest_desc_pool_deinit(pdev, mac_for_pdev);
}

void
dp_rx_pdev_mon_desc_pool_deinit(struct dp_pdev *pdev)
{
	int mac_id;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_desc_pool_deinit(pdev, mac_id);
	qdf_spinlock_destroy(&pdev->monitor_pdev->mon_lock);
}

static void
dp_rx_pdev_mon_cmn_desc_pool_init(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint32_t mac_for_pdev;

	mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev->pdev_id);
	dp_rx_pdev_mon_status_desc_pool_init(pdev, mac_for_pdev);

	dp_rx_pdev_mon_dest_desc_pool_init(pdev, mac_for_pdev);
}

void
dp_rx_pdev_mon_desc_pool_init(struct dp_pdev *pdev)
{
	int mac_id;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_desc_pool_init(pdev, mac_id);
	qdf_spinlock_create(&pdev->monitor_pdev->mon_lock);
}

static void
dp_rx_pdev_mon_cmn_buffers_free(struct dp_pdev *pdev, int mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	int mac_for_pdev;

	mac_for_pdev = dp_get_lmac_id_for_pdev_id(pdev->soc, mac_id, pdev_id);
	dp_rx_pdev_mon_status_buffers_free(pdev, mac_for_pdev);

	dp_rx_pdev_mon_dest_buffers_free(pdev, mac_for_pdev);
}

void
dp_rx_pdev_mon_buffers_free(struct dp_pdev *pdev)
{
	int mac_id;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_buffers_free(pdev, mac_id);
	pdev->monitor_pdev->pdev_mon_init = 0;
}

QDF_STATUS
dp_rx_pdev_mon_buffers_alloc(struct dp_pdev *pdev)
{
	int mac_id;
	int mac_for_pdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t pdev_id = pdev->pdev_id;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = pdev->soc->wlan_cfg_ctx;

	for (mac_id = 0; mac_id < soc_cfg_ctx->num_rxdma_status_rings_per_pdev;
	     mac_id++) {
		mac_for_pdev = dp_get_lmac_id_for_pdev_id(pdev->soc, mac_id,
							  pdev_id);
		status = dp_rx_pdev_mon_status_buffers_alloc(pdev,
							     mac_for_pdev);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			dp_err("dp_rx_pdev_mon_status_desc_pool_alloc() failed");
			goto mon_status_buf_fail;
		}
	}

	for (mac_id = 0; mac_id < soc_cfg_ctx->num_rxdma_dst_rings_per_pdev;
	     mac_id++) {
		mac_for_pdev = dp_get_lmac_id_for_pdev_id(pdev->soc, mac_id,
							  pdev_id);
		status = dp_rx_pdev_mon_dest_buffers_alloc(pdev, mac_for_pdev);
		if (!QDF_IS_STATUS_SUCCESS(status))
			goto mon_stat_buf_dealloc;
	}

	return status;

mon_stat_buf_dealloc:
	dp_rx_pdev_mon_status_buffers_free(pdev, mac_for_pdev);
mon_status_buf_fail:
	return status;
}

static QDF_STATUS
dp_rx_pdev_mon_cmn_desc_pool_alloc(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t pdev_id = pdev->pdev_id;
	uint32_t mac_for_pdev;
	QDF_STATUS status;

	mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev_id);

	/* Allocate sw rx descriptor pool for monitor status ring */
	status = dp_rx_pdev_mon_status_desc_pool_alloc(pdev, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("dp_rx_pdev_mon_status_desc_pool_alloc() failed");
		goto fail;
	}

	status = dp_rx_pdev_mon_dest_desc_pool_alloc(pdev, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto mon_status_dealloc;

	return status;

mon_status_dealloc:
	dp_rx_pdev_mon_status_desc_pool_free(pdev, mac_for_pdev);
fail:
	return status;
}

QDF_STATUS
dp_rx_pdev_mon_desc_pool_alloc(struct dp_pdev *pdev)
{
	QDF_STATUS status;
	int mac_id, count;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++) {
		status = dp_rx_pdev_mon_cmn_desc_pool_alloc(pdev, mac_id);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			dp_rx_mon_dest_err("%pK: %d failed\n",
					   pdev->soc, mac_id);

			for (count = 0; count < mac_id; count++)
				dp_rx_pdev_mon_cmn_desc_pool_free(pdev, count);

			return status;
		}
	}
	return status;
}

#ifdef QCA_WIFI_MONITOR_MODE_NO_MSDU_START_TLV_SUPPORT
static inline void
hal_rx_populate_buf_info(struct dp_soc *soc,
			 struct hal_rx_mon_dest_buf_info *buf_info,
			 void *rx_desc)
{
	hal_rx_priv_info_get_from_tlv(soc->hal_soc, rx_desc,
				      (uint8_t *)buf_info,
				      sizeof(*buf_info));
}

static inline uint8_t
hal_rx_frag_msdu_get_l2_hdr_offset(struct dp_soc *soc,
				   struct hal_rx_mon_dest_buf_info *buf_info,
				   void *rx_desc, bool is_first_frag)
{
	if (is_first_frag)
		return buf_info->l2_hdr_pad;
	else
		return DP_RX_MON_RAW_L2_HDR_PAD_BYTE;
}
#else
static inline void
hal_rx_populate_buf_info(struct dp_soc *soc,
			 struct hal_rx_mon_dest_buf_info *buf_info,
			 void *rx_desc)
{
	if (hal_rx_tlv_decap_format_get(soc->hal_soc, rx_desc) ==
	    HAL_HW_RX_DECAP_FORMAT_RAW)
		buf_info->is_decap_raw = 1;

	if (hal_rx_tlv_mpdu_len_err_get(soc->hal_soc, rx_desc))
		buf_info->mpdu_len_err = 1;
}

static inline uint8_t
hal_rx_frag_msdu_get_l2_hdr_offset(struct dp_soc *soc,
				   struct hal_rx_mon_dest_buf_info *buf_info,
				   void *rx_desc, bool is_first_frag)
{
	return hal_rx_msdu_end_l3_hdr_padding_get(soc->hal_soc, rx_desc);
}
#endif

static inline
void dp_rx_msdus_set_payload(struct dp_soc *soc, qdf_nbuf_t msdu,
			     uint8_t l2_hdr_offset)
{
	uint8_t *data;
	uint32_t rx_pkt_offset;

	data = qdf_nbuf_data(msdu);
	rx_pkt_offset = dp_rx_mon_get_rx_pkt_tlv_size(soc);
	qdf_nbuf_pull_head(msdu, rx_pkt_offset + l2_hdr_offset);
}

static inline qdf_nbuf_t
dp_rx_mon_restitch_mpdu_from_msdus(struct dp_soc *soc,
				   uint32_t mac_id,
				   qdf_nbuf_t head_msdu,
				   qdf_nbuf_t last_msdu,
				   struct cdp_mon_status *rx_status)
{
	qdf_nbuf_t msdu, mpdu_buf, prev_buf, msdu_orig, head_frag_list;
	uint32_t wifi_hdr_len, sec_hdr_len, msdu_llc_len,
		mpdu_buf_len, decap_hdr_pull_bytes, frag_list_sum_len, dir,
		is_amsdu, is_first_frag, amsdu_pad;
	void *rx_desc;
	char *hdr_desc;
	unsigned char *dest;
	struct ieee80211_frame *wh;
	struct ieee80211_qoscntl *qos;
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct dp_mon_pdev *mon_pdev;
	struct hal_rx_mon_dest_buf_info buf_info;
	uint8_t l2_hdr_offset;

	head_frag_list = NULL;
	mpdu_buf = NULL;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_mon_dest_debug("%pK: pdev is null for mac_id = %d",
				     soc, mac_id);
		return NULL;
	}

	mon_pdev = dp_pdev->monitor_pdev;

	/* The nbuf has been pulled just beyond the status and points to the
	 * payload
	 */
	if (!head_msdu)
		goto mpdu_stitch_fail;

	msdu_orig = head_msdu;

	rx_desc = qdf_nbuf_data(msdu_orig);
	qdf_mem_zero(&buf_info, sizeof(buf_info));
	hal_rx_populate_buf_info(soc, &buf_info, rx_desc);

	if (buf_info.mpdu_len_err) {
		/* It looks like there is some issue on MPDU len err */
		/* Need further investigate if drop the packet */
		DP_STATS_INC(dp_pdev, dropped.mon_rx_drop, 1);
		return NULL;
	}

	rx_desc = qdf_nbuf_data(last_msdu);

	rx_status->cdp_rs_fcs_err = hal_rx_tlv_mpdu_fcs_err_get(soc->hal_soc,
								rx_desc);
	mon_pdev->ppdu_info.rx_status.rs_fcs_err = rx_status->cdp_rs_fcs_err;

	/* Fill out the rx_status from the PPDU start and end fields */
	/*   HAL_RX_GET_PPDU_STATUS(soc, mac_id, rx_status); */

	rx_desc = qdf_nbuf_data(head_msdu);

	/* Easy case - The MSDU status indicates that this is a non-decapped
	 * packet in RAW mode.
	 */
	if (buf_info.is_decap_raw) {
		/* Note that this path might suffer from headroom unavailabilty
		 * - but the RX status is usually enough
		 */

		l2_hdr_offset = hal_rx_frag_msdu_get_l2_hdr_offset(soc,
								   &buf_info,
								   rx_desc,
								   true);
		dp_rx_msdus_set_payload(soc, head_msdu, l2_hdr_offset);

		dp_rx_mon_dest_debug("%pK: decap format raw head %pK head->next %pK last_msdu %pK last_msdu->next %pK",
				     soc, head_msdu, head_msdu->next,
				     last_msdu, last_msdu->next);

		mpdu_buf = head_msdu;

		prev_buf = mpdu_buf;

		frag_list_sum_len = 0;
		msdu = qdf_nbuf_next(head_msdu);
		is_first_frag = 1;

		while (msdu) {
			l2_hdr_offset = hal_rx_frag_msdu_get_l2_hdr_offset(
							soc, &buf_info,
							rx_desc, false);
			dp_rx_msdus_set_payload(soc, msdu, l2_hdr_offset);

			if (is_first_frag) {
				is_first_frag = 0;
				head_frag_list  = msdu;
			}

			frag_list_sum_len += qdf_nbuf_len(msdu);

			/* Maintain the linking of the cloned MSDUS */
			qdf_nbuf_set_next_ext(prev_buf, msdu);

			/* Move to the next */
			prev_buf = msdu;
			msdu = qdf_nbuf_next(msdu);
		}

		qdf_nbuf_trim_tail(prev_buf, HAL_RX_FCS_LEN);

		/* If there were more fragments to this RAW frame */
		if (head_frag_list) {
			if (frag_list_sum_len <
				sizeof(struct ieee80211_frame_min_one)) {
				DP_STATS_INC(dp_pdev, dropped.mon_rx_drop, 1);
				return NULL;
			}
			frag_list_sum_len -= HAL_RX_FCS_LEN;
			qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list,
						 frag_list_sum_len);
			qdf_nbuf_set_next(mpdu_buf, NULL);
		}

		goto mpdu_stitch_done;
	}

	/* Decap mode:
	 * Calculate the amount of header in decapped packet to knock off based
	 * on the decap type and the corresponding number of raw bytes to copy
	 * status header
	 */
	rx_desc = qdf_nbuf_data(head_msdu);

	hdr_desc = hal_rx_desc_get_80211_hdr(soc->hal_soc, rx_desc);

	dp_rx_mon_dest_debug("%pK: decap format not raw", soc);

	/* Base size */
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

	/* Calculate security header length based on 'Protected'
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
	msdu_llc_len = is_amsdu ? (14 + 8) : 8;

	mpdu_buf_len = wifi_hdr_len + msdu_llc_len;

	/* "Decap" header to remove from MSDU buffer */
	decap_hdr_pull_bytes = 14;

	/* Allocate a new nbuf for holding the 802.11 header retrieved from the
	 * status of the now decapped first msdu. Leave enough headroom for
	 * accommodating any radio-tap /prism like PHY header
	 */
	mpdu_buf = qdf_nbuf_alloc(soc->osdev,
				  MAX_MONITOR_HEADER + mpdu_buf_len,
				  MAX_MONITOR_HEADER, 4, FALSE);

	if (!mpdu_buf)
		goto mpdu_stitch_done;

	/* Copy the MPDU related header and enc headers into the first buffer
	 * - Note that there can be a 2 byte pad between heaader and enc header
	 */

	prev_buf = mpdu_buf;
	dest = qdf_nbuf_put_tail(prev_buf, wifi_hdr_len);
	if (!dest)
		goto mpdu_stitch_fail;

	qdf_mem_copy(dest, hdr_desc, wifi_hdr_len);
	hdr_desc += wifi_hdr_len;

#if 0
	dest = qdf_nbuf_put_tail(prev_buf, sec_hdr_len);
	adf_os_mem_copy(dest, hdr_desc, sec_hdr_len);
	hdr_desc += sec_hdr_len;
#endif

	/* The first LLC len is copied into the MPDU buffer */
	frag_list_sum_len = 0;

	msdu_orig = head_msdu;
	is_first_frag = 1;
	amsdu_pad = 0;

	while (msdu_orig) {

		/* TODO: intra AMSDU padding - do we need it ??? */

		msdu = msdu_orig;

		if (is_first_frag) {
			head_frag_list  = msdu;
		} else {
			/* Reload the hdr ptr only on non-first MSDUs */
			rx_desc = qdf_nbuf_data(msdu_orig);
			hdr_desc = hal_rx_desc_get_80211_hdr(soc->hal_soc,
							     rx_desc);
		}

		/* Copy this buffers MSDU related status into the prev buffer */

		if (is_first_frag)
			is_first_frag = 0;

		/* Update protocol and flow tag for MSDU */
		dp_rx_mon_update_protocol_flow_tag(soc, dp_pdev,
						   msdu_orig, rx_desc);

		dest = qdf_nbuf_put_tail(prev_buf,
					 msdu_llc_len + amsdu_pad);

		if (!dest)
			goto mpdu_stitch_fail;

		dest += amsdu_pad;
		qdf_mem_copy(dest, hdr_desc, msdu_llc_len);

		l2_hdr_offset = hal_rx_frag_msdu_get_l2_hdr_offset(soc,
								   &buf_info,
								   rx_desc,
								   true);
		dp_rx_msdus_set_payload(soc, msdu, l2_hdr_offset);

		/* Push the MSDU buffer beyond the decap header */
		qdf_nbuf_pull_head(msdu, decap_hdr_pull_bytes);
		frag_list_sum_len += msdu_llc_len + qdf_nbuf_len(msdu)
			+ amsdu_pad;

		/* Set up intra-AMSDU pad to be added to start of next buffer -
		 * AMSDU pad is 4 byte pad on AMSDU subframe
		 */
		amsdu_pad = (msdu_llc_len + qdf_nbuf_len(msdu)) & 0x3;
		amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;

		/* TODO FIXME How do we handle MSDUs that have fraglist - Should
		 * probably iterate all the frags cloning them along the way and
		 * and also updating the prev_buf pointer
		 */

		/* Move to the next */
		prev_buf = msdu;
		msdu_orig = qdf_nbuf_next(msdu_orig);
	}

#if 0
	/* Add in the trailer section - encryption trailer + FCS */
	qdf_nbuf_put_tail(prev_buf, HAL_RX_FCS_LEN);
	frag_list_sum_len += HAL_RX_FCS_LEN;
#endif

	frag_list_sum_len -= msdu_llc_len;

	/* TODO: Convert this to suitable adf routines */
	qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list,
				 frag_list_sum_len);

	dp_rx_mon_dest_debug("%pK: mpdu_buf %pK mpdu_buf->len %u",
			     soc, mpdu_buf, mpdu_buf->len);

mpdu_stitch_done:
	/* Check if this buffer contains the PPDU end status for TSF */
	/* Need revist this code to see where we can get tsf timestamp */
#if 0
	/* PPDU end TLV will be retrieved from monitor status ring */
	last_mpdu =
		(*(((u_int32_t *)&rx_desc->attention)) &
		RX_ATTENTION_0_LAST_MPDU_MASK) >>
		RX_ATTENTION_0_LAST_MPDU_LSB;

	if (last_mpdu)
		rx_status->rs_tstamp.tsf = rx_desc->ppdu_end.tsf_timestamp;

#endif
	return mpdu_buf;

mpdu_stitch_fail:
	if ((mpdu_buf) && !buf_info.is_decap_raw) {
		dp_rx_mon_dest_err("%pK: mpdu_stitch_fail mpdu_buf %pK",
				   soc, mpdu_buf);
		/* Free the head buffer */
		qdf_nbuf_free(mpdu_buf);
	}
	return NULL;
}

#ifdef DP_RX_MON_MEM_FRAG
/**
 * dp_rx_mon_fraglist_prepare() - Prepare nbuf fraglist from chained skb
 *
 * @head_msdu: Parent SKB
 * @tail_msdu: Last skb in the chained list
 *
 * Return: Void
 */
void dp_rx_mon_fraglist_prepare(qdf_nbuf_t head_msdu, qdf_nbuf_t tail_msdu)
{
	qdf_nbuf_t msdu, mpdu_buf, head_frag_list;
	uint32_t frag_list_sum_len;

	dp_err("[%s][%d] decap format raw head %pK head->next %pK last_msdu %pK last_msdu->next %pK",
	       __func__, __LINE__, head_msdu, head_msdu->next,
	       tail_msdu, tail_msdu->next);

	/* Single skb accommodating MPDU worth Data */
	if (tail_msdu == head_msdu)
		return;

	mpdu_buf = head_msdu;
	frag_list_sum_len = 0;

	msdu = qdf_nbuf_next(head_msdu);
	/* msdu can't be NULL here as it is multiple skb case here */

	/* Head frag list to point to second skb */
	head_frag_list  = msdu;

	while (msdu) {
		frag_list_sum_len += qdf_nbuf_len(msdu);
		msdu = qdf_nbuf_next(msdu);
	}

	qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list, frag_list_sum_len);

	/* Make Parent skb next to NULL */
	qdf_nbuf_set_next(mpdu_buf, NULL);
}

/**
 * dp_rx_mon_frag_restitch_mpdu_from_msdus() - Restitch logic to
 *      convert to 802.3 header and adjust frag memory pointing to
 *      dot3 header and payload in case of Non-Raw frame.
 *
 * @soc: struct dp_soc *
 * @mac_id: MAC id
 * @head_msdu: MPDU containing all MSDU as a frag
 * @tail_msdu: last skb which accommodate MPDU info
 * @rx_status: struct cdp_mon_status *
 *
 * Return: Adjusted nbuf containing MPDU worth info.
 */
static inline qdf_nbuf_t
dp_rx_mon_frag_restitch_mpdu_from_msdus(struct dp_soc *soc,
					uint32_t mac_id,
					qdf_nbuf_t head_msdu,
					qdf_nbuf_t tail_msdu,
					struct cdp_mon_status *rx_status)
{
	uint32_t wifi_hdr_len, sec_hdr_len, msdu_llc_len,
		mpdu_buf_len, decap_hdr_pull_bytes, dir,
		is_amsdu, amsdu_pad, frag_size, tot_msdu_len;
	qdf_frag_t rx_desc, rx_src_desc, rx_dest_desc, frag_addr;
	char *hdr_desc;
	uint8_t num_frags, frags_iter, l2_hdr_offset;
	struct ieee80211_frame *wh;
	struct ieee80211_qoscntl *qos;
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	int16_t frag_page_offset = 0;
	struct hal_rx_mon_dest_buf_info buf_info;
	uint32_t pad_byte_pholder = 0;
	qdf_nbuf_t msdu_curr;
	uint16_t rx_mon_tlv_size = soc->rx_mon_pkt_tlv_size;
	struct dp_mon_pdev *mon_pdev;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_mon_dest_debug("%pK: pdev is null for mac_id = %d",
				     soc, mac_id);
		return NULL;
	}

	mon_pdev = dp_pdev->monitor_pdev;
	qdf_mem_zero(&buf_info, sizeof(struct hal_rx_mon_dest_buf_info));

	if (!head_msdu || !tail_msdu)
		goto mpdu_stitch_fail;

	rx_desc = qdf_nbuf_get_frag_addr(head_msdu, 0) - rx_mon_tlv_size;

	if (hal_rx_tlv_mpdu_len_err_get(soc->hal_soc, rx_desc)) {
		/* It looks like there is some issue on MPDU len err */
		/* Need further investigate if drop the packet */
		DP_STATS_INC(dp_pdev, dropped.mon_rx_drop, 1);
		return NULL;
	}

	/* Look for FCS error */
	num_frags = qdf_nbuf_get_nr_frags(tail_msdu);
	rx_desc = qdf_nbuf_get_frag_addr(tail_msdu, num_frags - 1) -
				rx_mon_tlv_size;
	rx_status->cdp_rs_fcs_err = hal_rx_tlv_mpdu_fcs_err_get(soc->hal_soc,
								rx_desc);
	mon_pdev->ppdu_info.rx_status.rs_fcs_err = rx_status->cdp_rs_fcs_err;

	rx_desc = qdf_nbuf_get_frag_addr(head_msdu, 0) - rx_mon_tlv_size;
	hal_rx_priv_info_get_from_tlv(soc->hal_soc, rx_desc,
				      (uint8_t *)&buf_info,
				      sizeof(buf_info));

	/* Easy case - The MSDU status indicates that this is a non-decapped
	 * packet in RAW mode.
	 */
	if (buf_info.is_decap_raw == 1) {
		dp_rx_mon_fraglist_prepare(head_msdu, tail_msdu);
		goto mpdu_stitch_done;
	}

	l2_hdr_offset = DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE;

	/* Decap mode:
	 * Calculate the amount of header in decapped packet to knock off based
	 * on the decap type and the corresponding number of raw bytes to copy
	 * status header
	 */
	hdr_desc = hal_rx_desc_get_80211_hdr(soc->hal_soc, rx_desc);

	dp_rx_mon_dest_debug("%pK: decap format not raw", soc);

	/* Base size */
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
	msdu_llc_len = is_amsdu ? (14 + 8) : 8;

	mpdu_buf_len = wifi_hdr_len + msdu_llc_len;

	/* "Decap" header to remove from MSDU buffer */
	decap_hdr_pull_bytes = 14;

	amsdu_pad = 0;
	tot_msdu_len = 0;

	/*
	 * keeping first MSDU ops outside of loop to avoid multiple
	 * check handling
	 */

	/* Construct src header */
	rx_src_desc = hdr_desc;

	/*
	 * Update protocol and flow tag for MSDU
	 * update frag index in ctx_idx field.
	 * Reset head pointer data of nbuf before updating.
	 */
	QDF_NBUF_CB_RX_CTX_ID(head_msdu) = 0;
	dp_rx_mon_update_protocol_flow_tag(soc, dp_pdev, head_msdu, rx_desc);

	/* Construct destination address */
	frag_addr = qdf_nbuf_get_frag_addr(head_msdu, 0);
	frag_size = qdf_nbuf_get_frag_size_by_idx(head_msdu, 0);
	/* We will come here in 2 scenario:
	 * 1. First MSDU of MPDU with single buffer
	 * 2. First buffer of First MSDU of MPDU with continuation
	 *
	 *  ------------------------------------------------------------
	 * | SINGLE BUFFER (<= RX_MONITOR_BUFFER_SIZE - RX_PKT_TLVS_LEN)|
	 *  ------------------------------------------------------------
	 *
	 *  ------------------------------------------------------------
	 * | First BUFFER with Continuation             | ...           |
	 * | (RX_MONITOR_BUFFER_SIZE - RX_PKT_TLVS_LEN) |               |
	 *  ------------------------------------------------------------
	 */
	pad_byte_pholder =
		(RX_MONITOR_BUFFER_SIZE - soc->rx_pkt_tlv_size) - frag_size;
	/* Construct destination address
	 *  --------------------------------------------------------------
	 * | RX_PKT_TLV | L2_HDR_PAD   |   Decap HDR   |      Payload     |
	 * |            |                              /                  |
	 * |            >Frag address points here     /                   |
	 * |            \                            /                    |
	 * |             \ This bytes needs to      /                     |
	 * |              \  removed to frame pkt  /                      |
	 * |               -----------------------                        |
	 * |                                      |                       |
	 * |                                      |                       |
	 * |   WIFI +LLC HDR will be added here <-|                       |
	 * |        |                             |                       |
	 * |         >Dest addr will point        |                       |
	 * |            somewhere in this area    |                       |
	 *  --------------------------------------------------------------
	 */
	rx_dest_desc =
		(frag_addr + decap_hdr_pull_bytes + l2_hdr_offset) -
					mpdu_buf_len;
	/* Add WIFI and LLC header for 1st MSDU of MPDU */
	qdf_mem_copy(rx_dest_desc, rx_src_desc, mpdu_buf_len);

	frag_page_offset =
		(decap_hdr_pull_bytes + l2_hdr_offset) - mpdu_buf_len;

	qdf_nbuf_move_frag_page_offset(head_msdu, 0, frag_page_offset);

	frag_size = qdf_nbuf_get_frag_size_by_idx(head_msdu, 0);

	if (buf_info.first_buffer && buf_info.last_buffer) {
		/* MSDU with single buffer */
		amsdu_pad = frag_size & 0x3;
		amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;
		if (amsdu_pad && (amsdu_pad <= pad_byte_pholder)) {
			char *frag_addr_temp;

			qdf_nbuf_trim_add_frag_size(head_msdu, 0, amsdu_pad,
						    0);
			frag_addr_temp =
				(char *)qdf_nbuf_get_frag_addr(head_msdu, 0);
			frag_addr_temp = (frag_addr_temp +
				qdf_nbuf_get_frag_size_by_idx(head_msdu, 0)) -
					amsdu_pad;
			qdf_mem_zero(frag_addr_temp, amsdu_pad);
			amsdu_pad = 0;
		}
	} else {
		/*
		 * First buffer of Continuation frame and hence
		 * amsdu_padding doesn't need to be added
		 * Increase tot_msdu_len so that amsdu_pad byte
		 * will be calculated for last frame of MSDU
		 */
		tot_msdu_len = frag_size;
		amsdu_pad = 0;
	}

	/* Here amsdu_pad byte will have some value if 1sf buffer was
	 * Single buffer MSDU and dint had pholder to adjust amsdu padding
	 * byte in the end
	 * So dont initialize to ZERO here
	 */
	pad_byte_pholder = 0;
	for (msdu_curr = head_msdu; msdu_curr;) {
		/* frag_iter will start from 0 for second skb onwards */
		if (msdu_curr == head_msdu)
			frags_iter = 1;
		else
			frags_iter = 0;

		num_frags = qdf_nbuf_get_nr_frags(msdu_curr);

		for (; frags_iter < num_frags; frags_iter++) {
		/* Construct destination address
		 *  ----------------------------------------------------------
		 * | RX_PKT_TLV | L2_HDR_PAD   |   Decap HDR | Payload | Pad  |
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
			frag_addr =
				qdf_nbuf_get_frag_addr(msdu_curr, frags_iter);
			rx_desc = frag_addr - rx_mon_tlv_size;

			/*
			 * Update protocol and flow tag for MSDU
			 * update frag index in ctx_idx field
			 */
			QDF_NBUF_CB_RX_CTX_ID(msdu_curr) = frags_iter;
			dp_rx_mon_update_protocol_flow_tag(soc, dp_pdev,
							   msdu_curr, rx_desc);

			/* Read buffer info from stored data in tlvs */
			hal_rx_priv_info_get_from_tlv(soc->hal_soc, rx_desc,
						      (uint8_t *)&buf_info,
						      sizeof(buf_info));

			frag_size = qdf_nbuf_get_frag_size_by_idx(msdu_curr,
								  frags_iter);

			/* If Middle buffer, dont add any header */
			if ((!buf_info.first_buffer) &&
			    (!buf_info.last_buffer)) {
				tot_msdu_len += frag_size;
				amsdu_pad = 0;
				pad_byte_pholder = 0;
				continue;
			}

			/* Calculate if current buffer has placeholder
			 * to accommodate amsdu pad byte
			 */
			pad_byte_pholder =
				(RX_MONITOR_BUFFER_SIZE - soc->rx_pkt_tlv_size)
				- frag_size;
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
			if (buf_info.first_buffer)
				l2_hdr_offset =
					DP_RX_MON_NONRAW_L2_HDR_PAD_BYTE;
			else
				l2_hdr_offset = DP_RX_MON_RAW_L2_HDR_PAD_BYTE;

			if (buf_info.first_buffer) {
				/* Src addr from where llc header needs to be copied */
				rx_src_desc =
					hal_rx_desc_get_80211_hdr(soc->hal_soc,
								  rx_desc);

				/* Size of buffer with llc header */
				frag_size = frag_size -
					(l2_hdr_offset + decap_hdr_pull_bytes);
				frag_size += msdu_llc_len;

				/* Construct destination address */
				rx_dest_desc = frag_addr +
					decap_hdr_pull_bytes + l2_hdr_offset;
				rx_dest_desc = rx_dest_desc - (msdu_llc_len);

				qdf_mem_copy(rx_dest_desc, rx_src_desc,
					     msdu_llc_len);

				/*
				 * Calculate new page offset and create hole
				 * if amsdu_pad required.
				 */
				frag_page_offset = l2_hdr_offset +
						decap_hdr_pull_bytes;
				frag_page_offset = frag_page_offset -
						(msdu_llc_len + amsdu_pad);

				qdf_nbuf_move_frag_page_offset(msdu_curr,
							       frags_iter,
							       frag_page_offset);

				tot_msdu_len = frag_size;
				/*
				 * No amsdu padding required for first frame of
				 * continuation buffer
				 */
				if (!buf_info.last_buffer) {
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

			/* Check size of buffer if amsdu padding required */
			amsdu_pad = tot_msdu_len & 0x3;
			amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;

			/* Create placeholder if current buffer can
			 * accommodate padding.
			 */
			if (amsdu_pad && (amsdu_pad <= pad_byte_pholder)) {
				char *frag_addr_temp;

				qdf_nbuf_trim_add_frag_size(msdu_curr,
							    frags_iter,
							    amsdu_pad, 0);
				frag_addr_temp = (char *)qdf_nbuf_get_frag_addr(msdu_curr,
										frags_iter);
				frag_addr_temp = (frag_addr_temp +
					qdf_nbuf_get_frag_size_by_idx(msdu_curr, frags_iter)) -
					amsdu_pad;
				qdf_mem_zero(frag_addr_temp, amsdu_pad);
				amsdu_pad = 0;
			}

			/* reset tot_msdu_len */
			tot_msdu_len = 0;
		}
		msdu_curr = qdf_nbuf_next(msdu_curr);
	}

	dp_rx_mon_fraglist_prepare(head_msdu, tail_msdu);

	dp_rx_mon_dest_debug("%pK: head_msdu %pK head_msdu->len %u",
			     soc, head_msdu, head_msdu->len);

mpdu_stitch_done:
	return head_msdu;

mpdu_stitch_fail:
	dp_rx_mon_dest_err("%pK: mpdu_stitch_fail head_msdu %pK",
			   soc, head_msdu);
	return NULL;
}
#endif

#ifdef DP_RX_MON_MEM_FRAG
qdf_nbuf_t dp_rx_mon_restitch_mpdu(struct dp_soc *soc, uint32_t mac_id,
				   qdf_nbuf_t head_msdu, qdf_nbuf_t tail_msdu,
				   struct cdp_mon_status *rs)
{
	if (qdf_nbuf_get_nr_frags(head_msdu))
		return dp_rx_mon_frag_restitch_mpdu_from_msdus(soc, mac_id,
							       head_msdu,
							       tail_msdu, rs);
	else
		return dp_rx_mon_restitch_mpdu_from_msdus(soc, mac_id,
							  head_msdu,
							  tail_msdu, rs);
}
#else
qdf_nbuf_t dp_rx_mon_restitch_mpdu(struct dp_soc *soc, uint32_t mac_id,
				   qdf_nbuf_t head_msdu, qdf_nbuf_t tail_msdu,
				   struct cdp_mon_status *rs)
{
	return dp_rx_mon_restitch_mpdu_from_msdus(soc, mac_id, head_msdu,
						  tail_msdu, rs);
}
#endif

#ifdef DP_RX_MON_MEM_FRAG
#if defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) ||\
	defined(WLAN_SUPPORT_RX_FLOW_TAG)
void dp_rx_mon_update_pf_tag_to_buf_headroom(struct dp_soc *soc,
					     qdf_nbuf_t nbuf)
{
	qdf_nbuf_t ext_list;

	if (qdf_unlikely(!soc)) {
		dp_err("Soc[%pK] Null. Can't update pftag to nbuf headroom\n",
		       soc);
		qdf_assert_always(0);
	}

	if (!wlan_cfg_is_rx_mon_protocol_flow_tag_enabled(soc->wlan_cfg_ctx))
		return;

	if (qdf_unlikely(!nbuf))
		return;

	/* Return if it dint came from mon Path */
	if (!qdf_nbuf_get_nr_frags(nbuf))
		return;

	/* Headroom must be double of PF_TAG_SIZE as we copy it 1stly to head */
	if (qdf_unlikely(qdf_nbuf_headroom(nbuf) < (DP_RX_MON_TOT_PF_TAG_LEN * 2))) {
		dp_err("Nbuf avail Headroom[%d] < 2 * DP_RX_MON_PF_TAG_TOT_LEN[%lu]",
		       qdf_nbuf_headroom(nbuf), DP_RX_MON_TOT_PF_TAG_LEN);
		return;
	}

	qdf_nbuf_push_head(nbuf, DP_RX_MON_TOT_PF_TAG_LEN);
	qdf_mem_copy(qdf_nbuf_data(nbuf), qdf_nbuf_head(nbuf),
		     DP_RX_MON_TOT_PF_TAG_LEN);
	qdf_nbuf_pull_head(nbuf, DP_RX_MON_TOT_PF_TAG_LEN);

	ext_list = qdf_nbuf_get_ext_list(nbuf);
	while (ext_list) {
		/* Headroom must be double of PF_TAG_SIZE
		 * as we copy it 1stly to head
		 */
		if (qdf_unlikely(qdf_nbuf_headroom(ext_list) < (DP_RX_MON_TOT_PF_TAG_LEN * 2))) {
			dp_err("Fraglist Nbuf avail Headroom[%d] < 2 * DP_RX_MON_PF_TAG_TOT_LEN[%lu]",
			       qdf_nbuf_headroom(ext_list),
			       DP_RX_MON_TOT_PF_TAG_LEN);
			ext_list = qdf_nbuf_queue_next(ext_list);
			continue;
		}
		qdf_nbuf_push_head(ext_list, DP_RX_MON_TOT_PF_TAG_LEN);
		qdf_mem_copy(qdf_nbuf_data(ext_list), qdf_nbuf_head(ext_list),
			     DP_RX_MON_TOT_PF_TAG_LEN);
		qdf_nbuf_pull_head(ext_list, DP_RX_MON_TOT_PF_TAG_LEN);
		ext_list = qdf_nbuf_queue_next(ext_list);
	}
}
#endif
#endif

#ifdef QCA_MONITOR_PKT_SUPPORT
QDF_STATUS dp_mon_htt_dest_srng_setup(struct dp_soc *soc,
				      struct dp_pdev *pdev,
				      int mac_id,
				      int mac_for_pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (soc->wlan_cfg_ctx->rxdma1_enable) {
		status = htt_srng_setup(soc->htt_handle, mac_for_pdev,
					soc->rxdma_mon_buf_ring[mac_id]
					.hal_srng,
					RXDMA_MONITOR_BUF);

		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("Failed to send htt srng setup message for Rxdma mon buf ring");
			return status;
		}

		status = htt_srng_setup(soc->htt_handle, mac_for_pdev,
					soc->rxdma_mon_dst_ring[mac_id]
					.hal_srng,
					RXDMA_MONITOR_DST);

		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("Failed to send htt srng setup message for Rxdma mon dst ring");
			return status;
		}

		status = htt_srng_setup(soc->htt_handle, mac_for_pdev,
					soc->rxdma_mon_desc_ring[mac_id]
					.hal_srng,
					RXDMA_MONITOR_DESC);

		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("Failed to send htt srng message for Rxdma mon desc ring");
			return status;
		}
	}

	return status;
}
#endif /* QCA_MONITOR_PKT_SUPPORT */

#ifdef QCA_MONITOR_PKT_SUPPORT
void dp_mon_dest_rings_deinit(struct dp_pdev *pdev, int lmac_id)
{
	struct dp_soc *soc = pdev->soc;

	if (soc->wlan_cfg_ctx->rxdma1_enable) {
		dp_srng_deinit(soc, &soc->rxdma_mon_buf_ring[lmac_id],
			       RXDMA_MONITOR_BUF, 0);
		dp_srng_deinit(soc, &soc->rxdma_mon_dst_ring[lmac_id],
			       RXDMA_MONITOR_DST, 0);
		dp_srng_deinit(soc, &soc->rxdma_mon_desc_ring[lmac_id],
			       RXDMA_MONITOR_DESC, 0);
	}
}

void dp_mon_dest_rings_free(struct dp_pdev *pdev, int lmac_id)
{
	struct dp_soc *soc = pdev->soc;

	if (soc->wlan_cfg_ctx->rxdma1_enable) {
		dp_srng_free(soc, &soc->rxdma_mon_buf_ring[lmac_id]);
		dp_srng_free(soc, &soc->rxdma_mon_dst_ring[lmac_id]);
		dp_srng_free(soc, &soc->rxdma_mon_desc_ring[lmac_id]);
	}
}

QDF_STATUS dp_mon_dest_rings_init(struct dp_pdev *pdev, int lmac_id)
{
	struct dp_soc *soc = pdev->soc;

	if (soc->wlan_cfg_ctx->rxdma1_enable) {
		if (dp_srng_init(soc, &soc->rxdma_mon_buf_ring[lmac_id],
				 RXDMA_MONITOR_BUF, 0, lmac_id)) {
			dp_mon_err("%pK: " RNG_ERR "rxdma_mon_buf_ring ", soc);
			goto fail1;
		}

		if (dp_srng_init(soc, &soc->rxdma_mon_dst_ring[lmac_id],
				 RXDMA_MONITOR_DST, 0, lmac_id)) {
			dp_mon_err("%pK: " RNG_ERR "rxdma_mon_dst_ring", soc);
			goto fail1;
		}

		if (dp_srng_init(soc, &soc->rxdma_mon_desc_ring[lmac_id],
				 RXDMA_MONITOR_DESC, 0, lmac_id)) {
			dp_mon_err("%pK: " RNG_ERR "rxdma_mon_desc_ring", soc);
			goto fail1;
		}
	}
	return QDF_STATUS_SUCCESS;

fail1:
	return QDF_STATUS_E_NOMEM;
}

QDF_STATUS dp_mon_dest_rings_alloc(struct dp_pdev *pdev, int lmac_id)
{
	int entries;
	struct dp_soc *soc = pdev->soc;
	struct wlan_cfg_dp_pdev_ctxt *pdev_cfg_ctx = pdev->wlan_cfg_ctx;

	if (soc->wlan_cfg_ctx->rxdma1_enable) {
		entries = wlan_cfg_get_dma_mon_buf_ring_size(pdev_cfg_ctx);
		if (dp_srng_alloc(soc, &soc->rxdma_mon_buf_ring[lmac_id],
				  RXDMA_MONITOR_BUF, entries, 0)) {
			dp_mon_err("%pK: " RNG_ERR "rxdma_mon_buf_ring ", soc);
			goto fail1;
		}
		entries = wlan_cfg_get_dma_rx_mon_dest_ring_size(pdev_cfg_ctx);
		if (dp_srng_alloc(soc, &soc->rxdma_mon_dst_ring[lmac_id],
				  RXDMA_MONITOR_DST, entries, 0)) {
			dp_mon_err("%pK: " RNG_ERR "rxdma_mon_dst_ring", soc);
			goto fail1;
		}
		entries = wlan_cfg_get_dma_mon_desc_ring_size(pdev_cfg_ctx);
		if (dp_srng_alloc(soc, &soc->rxdma_mon_desc_ring[lmac_id],
				  RXDMA_MONITOR_DESC, entries, 0)) {
			dp_mon_err("%pK: " RNG_ERR "rxdma_mon_desc_ring", soc);
			goto fail1;
		}
	}
	return QDF_STATUS_SUCCESS;

fail1:
	return QDF_STATUS_E_NOMEM;
}
#endif
