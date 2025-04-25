/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "hal_hw_headers.h"
#include "dp_types.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "dp_be_rx.h"
#include "dp_peer.h"
#include "hal_rx.h"
#include "hal_be_rx.h"
#include "hal_api.h"
#include "hal_be_api.h"
#include "qdf_nbuf.h"
#ifdef MESH_MODE_SUPPORT
#include "if_meta_hdr.h"
#endif
#include "dp_internal.h"
#include "dp_ipa.h"
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif
#include "dp_hist.h"
#include "dp_rx_buffer_pool.h"

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
static inline void
dp_rx_update_flow_info(qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr)
{
	uint32_t fse_metadata;

	/* Set the flow idx valid flag only when there is no timeout */
	if (hal_rx_msdu_flow_idx_timeout_be(rx_tlv_hdr))
		return;

	/*
	 * If invalid bit is not set and the fse metadata indicates that it is
	 * a valid SFE flow match in FSE, do not set the rx flow tag and let it
	 * go via stack instead of VP.
	 */
	fse_metadata = hal_rx_msdu_fse_metadata_get_be(rx_tlv_hdr);
	if (!hal_rx_msdu_flow_idx_invalid_be(rx_tlv_hdr) && (fse_metadata == DP_RX_FSE_FLOW_MATCH_SFE))
		return;

	qdf_nbuf_set_rx_flow_idx_valid(nbuf,
				 !hal_rx_msdu_flow_idx_invalid_be(rx_tlv_hdr));
}
#else
static inline void
dp_rx_update_flow_info(qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr)
{
}
#endif

#ifdef DP_RX_MSDU_DONE_FAIL_HISTORY
static inline void
dp_rx_msdu_done_fail_event_record(struct dp_soc *soc,
				  struct dp_rx_desc *rx_desc,
				  qdf_nbuf_t nbuf)
{
	struct dp_msdu_done_fail_entry *entry;
	uint32_t idx;

	if (qdf_unlikely(!soc->msdu_done_fail_hist))
		return;

	idx = dp_history_get_next_index(&soc->msdu_done_fail_hist->index,
					DP_MSDU_DONE_FAIL_HIST_MAX);
	entry = &soc->msdu_done_fail_hist->entry[idx];
	entry->paddr = qdf_nbuf_get_frag_paddr(nbuf, 0);

	if (rx_desc)
		entry->sw_cookie = rx_desc->cookie;
	else
		entry->sw_cookie = 0xDEAD;
}
#else
static inline void
dp_rx_msdu_done_fail_event_record(struct dp_soc *soc,
				  struct dp_rx_desc *rx_desc,
				  qdf_nbuf_t nbuf)
{
}
#endif

#ifndef AST_OFFLOAD_ENABLE
static void
dp_rx_wds_learn(struct dp_soc *soc,
		struct dp_vdev *vdev,
		uint8_t *rx_tlv_hdr,
		struct dp_txrx_peer *txrx_peer,
		qdf_nbuf_t nbuf)
{
	struct hal_rx_msdu_metadata msdu_metadata;

	hal_rx_msdu_packet_metadata_get_generic_be(rx_tlv_hdr, &msdu_metadata);
	/* WDS Source Port Learning */
	if (qdf_likely(vdev->wds_enabled))
		dp_rx_wds_srcport_learn(soc,
				rx_tlv_hdr,
				txrx_peer,
				nbuf,
				msdu_metadata);
}
#else
#ifdef QCA_SUPPORT_WDS_EXTENDED
/**
 * dp_wds_ext_peer_learn_be() - function to send event to control
 * path on receiving 1st 4-address frame from backhaul.
 * @soc: DP soc
 * @ta_txrx_peer: WDS repeater txrx peer
 * @rx_tlv_hdr: start address of rx tlvs
 * @nbuf: RX packet buffer
 *
 * Return: void
 */
static inline void dp_wds_ext_peer_learn_be(struct dp_soc *soc,
					    struct dp_txrx_peer *ta_txrx_peer,
					    uint8_t *rx_tlv_hdr,
					    qdf_nbuf_t nbuf)
{
	uint8_t wds_ext_src_mac[QDF_MAC_ADDR_SIZE];
	struct dp_peer *ta_base_peer;

	/* instead of checking addr4 is valid or not in per packet path
	 * check for init bit, which will be set on reception of
	 * first addr4 valid packet.
	 */
	if (!ta_txrx_peer->vdev->wds_ext_enabled ||
	    qdf_atomic_test_bit(WDS_EXT_PEER_INIT_BIT,
				&ta_txrx_peer->wds_ext.init))
		return;

	if (qdf_nbuf_is_rx_chfrag_start(nbuf) &&
	    (qdf_nbuf_is_fr_ds_set(nbuf) && qdf_nbuf_is_to_ds_set(nbuf))) {
		qdf_atomic_test_and_set_bit(WDS_EXT_PEER_INIT_BIT,
					    &ta_txrx_peer->wds_ext.init);

		if (qdf_unlikely(ta_txrx_peer->nawds_enabled &&
				 ta_txrx_peer->is_mld_peer)) {
			ta_base_peer = dp_get_primary_link_peer_by_id(
							soc,
							ta_txrx_peer->peer_id,
							DP_MOD_ID_RX);
		} else {
			ta_base_peer = dp_peer_get_ref_by_id(
							soc,
							ta_txrx_peer->peer_id,
							DP_MOD_ID_RX);
		}

		if (!ta_base_peer)
			return;

		qdf_mem_copy(wds_ext_src_mac, &ta_base_peer->mac_addr.raw[0],
			     QDF_MAC_ADDR_SIZE);
		dp_peer_unref_delete(ta_base_peer, DP_MOD_ID_RX);

		soc->cdp_soc.ol_ops->rx_wds_ext_peer_learn(
						soc->ctrl_psoc,
						ta_txrx_peer->peer_id,
						ta_txrx_peer->vdev->vdev_id,
						wds_ext_src_mac);
	}
}
#else
static inline void dp_wds_ext_peer_learn_be(struct dp_soc *soc,
					    struct dp_txrx_peer *ta_txrx_peer,
					    uint8_t *rx_tlv_hdr,
					    qdf_nbuf_t nbuf)
{
}
#endif
static void
dp_rx_wds_learn(struct dp_soc *soc,
		struct dp_vdev *vdev,
		uint8_t *rx_tlv_hdr,
		struct dp_txrx_peer *ta_txrx_peer,
		qdf_nbuf_t nbuf)
{
	dp_wds_ext_peer_learn_be(soc, ta_txrx_peer, rx_tlv_hdr, nbuf);
}
#endif

#ifdef DP_RX_PEEK_MSDU_DONE_WAR
static inline int dp_rx_war_peek_msdu_done(struct dp_soc *soc,
					   struct dp_rx_desc *rx_desc)
{
	uint8_t *rx_tlv_hdr;

	qdf_nbuf_sync_for_cpu(soc->osdev, rx_desc->nbuf, QDF_DMA_FROM_DEVICE);
	rx_tlv_hdr = qdf_nbuf_data(rx_desc->nbuf);

	return hal_rx_tlv_msdu_done_get_be(rx_tlv_hdr);
}

/**
 * dp_rx_delink_n_rel_rx_desc() - unmap & free the nbuf in the rx_desc
 * @soc: DP SoC handle
 * @rx_desc: rx_desc handle of the nbuf to be unmapped & freed
 * @reo_ring_num: REO_RING_NUM corresponding to the REO for which the
 *		  bottom half is being serviced.
 *
 * Return: None
 */
static inline void
dp_rx_delink_n_rel_rx_desc(struct dp_soc *soc, struct dp_rx_desc *rx_desc,
			   uint8_t reo_ring_num)
{
	if (!rx_desc)
		return;

	dp_rx_nbuf_unmap(soc, rx_desc, reo_ring_num);
	dp_rx_nbuf_free(rx_desc->nbuf);
	/*
	 * RX_DESC flags:
	 * in_use = 0 will be set when this rx_desc is added to local freelist
	 * unmapped = 1 will be set by dp_rx_nbuf_unmap
	 * in_err_state = 0 will be set during replenish
	 * has_reuse_nbuf need not be touched.
	 * msdu_done_fail = 0 should be set here ..!!
	 */
	rx_desc->msdu_done_fail = 0;
}

static inline struct dp_rx_desc *
dp_rx_war_store_msdu_done_fail_desc(struct dp_soc *soc,
				    struct dp_rx_desc *rx_desc,
				    uint8_t reo_ring_num)
{
	struct dp_rx_msdu_done_fail_desc_list *msdu_done_fail_desc_list =
						&soc->msdu_done_fail_desc_list;
	struct dp_rx_desc *old_rx_desc;
	uint32_t idx;

	idx = dp_get_next_index(&msdu_done_fail_desc_list->index,
				DP_MSDU_DONE_FAIL_DESCS_MAX);

	old_rx_desc = msdu_done_fail_desc_list->msdu_done_fail_descs[idx];
	dp_rx_delink_n_rel_rx_desc(soc, old_rx_desc, reo_ring_num);

	msdu_done_fail_desc_list->msdu_done_fail_descs[idx] = rx_desc;

	return old_rx_desc;
}

#else
static inline int dp_rx_war_peek_msdu_done(struct dp_soc *soc,
					   struct dp_rx_desc *rx_desc)
{
	return 1;
}

static inline struct dp_rx_desc *
dp_rx_war_store_msdu_done_fail_desc(struct dp_soc *soc,
				    struct dp_rx_desc *rx_desc,
				    uint8_t reo_ring_num)
{
	return NULL;
}
#endif

uint32_t dp_rx_process_be(struct dp_intr *int_ctx,
			  hal_ring_handle_t hal_ring_hdl, uint8_t reo_ring_num,
			  uint32_t quota)
{
	hal_ring_desc_t ring_desc;
	hal_ring_desc_t last_prefetched_hw_desc;
	hal_soc_handle_t hal_soc;
	struct dp_rx_desc *rx_desc = NULL;
	struct dp_rx_desc *last_prefetched_sw_desc = NULL;
	qdf_nbuf_t nbuf, next;
	bool near_full;
	union dp_rx_desc_list_elem_t *head[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT];
	union dp_rx_desc_list_elem_t *tail[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT];
	uint32_t num_pending = 0;
	uint32_t rx_bufs_used = 0, rx_buf_cookie;
	uint16_t msdu_len = 0;
	uint16_t peer_id;
	uint8_t vdev_id;
	struct dp_txrx_peer *txrx_peer;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	struct dp_vdev *vdev;
	uint32_t pkt_len = 0;
	enum hal_reo_error_status error;
	uint8_t *rx_tlv_hdr;
	uint32_t rx_bufs_reaped[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT];
	uint8_t mac_id = 0;
	struct dp_pdev *rx_pdev;
	uint8_t enh_flag;
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;
	struct dp_soc *soc = int_ctx->soc;
	struct cdp_tid_rx_stats *tid_stats;
	qdf_nbuf_t nbuf_head;
	qdf_nbuf_t nbuf_tail;
	qdf_nbuf_t deliver_list_head;
	qdf_nbuf_t deliver_list_tail;
	uint32_t num_rx_bufs_reaped = 0;
	uint32_t intr_id;
	struct hif_opaque_softc *scn;
	int32_t tid = 0;
	bool is_prev_msdu_last = true;
	uint32_t num_entries_avail = 0;
	uint32_t rx_ol_pkt_cnt = 0;
	uint32_t num_entries = 0;
	QDF_STATUS status;
	qdf_nbuf_t ebuf_head;
	qdf_nbuf_t ebuf_tail;
	uint8_t pkt_capture_offload = 0;
	struct dp_srng *rx_ring = &soc->reo_dest_ring[reo_ring_num];
	int max_reap_limit, ring_near_full;
	struct dp_soc *replenish_soc;
	uint8_t chip_id;
	uint64_t current_time = 0;
	uint32_t old_tid;
	uint32_t peer_ext_stats;
	uint32_t dsf;
	uint32_t l3_pad;
	uint8_t link_id = 0;
	uint16_t buf_size;

	DP_HIST_INIT();

	qdf_assert_always(soc && hal_ring_hdl);
	hal_soc = soc->hal_soc;
	qdf_assert_always(hal_soc);

	scn = soc->hif_handle;
	intr_id = int_ctx->dp_intr_id;
	num_entries = hal_srng_get_num_entries(hal_soc, hal_ring_hdl);
	dp_runtime_pm_mark_last_busy(soc);
	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

more_data:
	/* reset local variables here to be re-used in the function */
	nbuf_head = NULL;
	nbuf_tail = NULL;
	deliver_list_head = NULL;
	deliver_list_tail = NULL;
	txrx_peer = NULL;
	vdev = NULL;
	num_rx_bufs_reaped = 0;
	ebuf_head = NULL;
	ebuf_tail = NULL;
	ring_near_full = 0;
	max_reap_limit = dp_rx_get_loop_pkt_limit(soc);

	qdf_mem_zero(rx_bufs_reaped, sizeof(rx_bufs_reaped));
	qdf_mem_zero(head, sizeof(head));
	qdf_mem_zero(tail, sizeof(tail));
	old_tid = 0xff;
	dsf = 0;
	peer_ext_stats = 0;
	rx_pdev = NULL;
	tid_stats = NULL;

	dp_pkt_get_timestamp(&current_time);

	ring_near_full = _dp_srng_test_and_update_nf_params(soc, rx_ring,
							    &max_reap_limit);

	peer_ext_stats = wlan_cfg_is_peer_ext_stats_enabled(soc->wlan_cfg_ctx);
	if (qdf_unlikely(dp_rx_srng_access_start(int_ctx, soc, hal_ring_hdl))) {
		/*
		 * Need API to convert from hal_ring pointer to
		 * Ring Type / Ring Id combo
		 */
		DP_STATS_INC(soc, rx.err.hal_ring_access_fail, 1);
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  FL("HAL RING Access Failed -- %pK"), hal_ring_hdl);
		goto done;
	}

	hal_srng_update_ring_usage_wm_no_lock(soc->hal_soc, hal_ring_hdl);

	if (!num_pending)
		num_pending = hal_srng_dst_num_valid(hal_soc, hal_ring_hdl, 0);

	if (num_pending > quota)
		num_pending = quota;

	dp_srng_dst_inv_cached_descs(soc, hal_ring_hdl, num_pending);
	last_prefetched_hw_desc = dp_srng_dst_prefetch_32_byte_desc(hal_soc,
							    hal_ring_hdl,
							    num_pending);
	/*
	 * start reaping the buffers from reo ring and queue
	 * them in per vdev queue.
	 * Process the received pkts in a different per vdev loop.
	 */
	while (qdf_likely(num_pending)) {
		ring_desc = dp_srng_dst_get_next(soc, hal_ring_hdl);

		if (qdf_unlikely(!ring_desc))
			break;

		error = HAL_RX_ERROR_STATUS_GET(ring_desc);

		if (qdf_unlikely(error == HAL_REO_ERROR_DETECTED)) {
			dp_rx_err("%pK: HAL RING 0x%pK:error %d",
				  soc, hal_ring_hdl, error);
			DP_STATS_INC(soc, rx.err.hal_reo_error[reo_ring_num],
				     1);
			/* Don't know how to deal with this -- assert */
			qdf_assert(0);
		}

		dp_rx_ring_record_entry(soc, reo_ring_num, ring_desc);
		rx_buf_cookie = HAL_RX_REO_BUF_COOKIE_GET(ring_desc);
		status = dp_rx_cookie_check_and_invalidate(ring_desc);
		if (qdf_unlikely(QDF_IS_STATUS_ERROR(status))) {
			DP_STATS_INC(soc, rx.err.stale_cookie, 1);
			break;
		}

		rx_desc = (struct dp_rx_desc *)
				hal_rx_get_reo_desc_va(ring_desc);
		dp_rx_desc_sw_cc_check(soc, rx_buf_cookie, &rx_desc);

		status = dp_rx_desc_sanity(soc, hal_soc, hal_ring_hdl,
					   ring_desc, rx_desc);
		if (QDF_IS_STATUS_ERROR(status)) {
			if (qdf_unlikely(rx_desc && rx_desc->nbuf)) {
				qdf_assert_always(!rx_desc->unmapped);
				dp_rx_nbuf_unmap(soc, rx_desc, reo_ring_num);
				dp_rx_buffer_pool_nbuf_free(soc, rx_desc->nbuf,
							    rx_desc->pool_id);
				dp_rx_add_to_free_desc_list(
					&head[rx_desc->chip_id][rx_desc->pool_id],
					&tail[rx_desc->chip_id][rx_desc->pool_id],
					rx_desc);
			}
			continue;
		}

		/*
		 * this is a unlikely scenario where the host is reaping
		 * a descriptor which it already reaped just a while ago
		 * but is yet to replenish it back to HW.
		 * In this case host will dump the last 128 descriptors
		 * including the software descriptor rx_desc and assert.
		 */

		if (qdf_unlikely(!rx_desc->in_use)) {
			DP_STATS_INC(soc, rx.err.hal_reo_dest_dup, 1);
			dp_info_rl("Reaping rx_desc not in use!");
			dp_rx_dump_info_and_assert(soc, hal_ring_hdl,
						   ring_desc, rx_desc);
			continue;
		}

		status = dp_rx_desc_nbuf_sanity_check(soc, ring_desc, rx_desc);
		if (qdf_unlikely(QDF_IS_STATUS_ERROR(status))) {
			DP_STATS_INC(soc, rx.err.nbuf_sanity_fail, 1);
			dp_info_rl("Nbuf sanity check failure!");
			dp_rx_dump_info_and_assert(soc, hal_ring_hdl,
						   ring_desc, rx_desc);
			rx_desc->in_err_state = 1;
			continue;
		}

		if (qdf_unlikely(!dp_rx_desc_check_magic(rx_desc))) {
			dp_err("Invalid rx_desc cookie=%d", rx_buf_cookie);
			DP_STATS_INC(soc, rx.err.rx_desc_invalid_magic, 1);
			dp_rx_dump_info_and_assert(soc, hal_ring_hdl,
						   ring_desc, rx_desc);
		}

		pkt_capture_offload =
			dp_rx_copy_desc_info_in_nbuf_cb(soc, ring_desc,
							rx_desc->nbuf,
							reo_ring_num);

		if (qdf_unlikely(qdf_nbuf_is_rx_chfrag_cont(rx_desc->nbuf))) {
			/* In dp_rx_sg_create() until the last buffer,
			 * end bit should not be set. As continuation bit set,
			 * this is not a last buffer.
			 */
			qdf_nbuf_set_rx_chfrag_end(rx_desc->nbuf, 0);

			/* previous msdu has end bit set, so current one is
			 * the new MPDU
			 */
			if (is_prev_msdu_last) {
				/* Get number of entries available in HW ring */
				num_entries_avail =
				hal_srng_dst_num_valid(hal_soc,
						       hal_ring_hdl, 1);

				/* For new MPDU check if we can read complete
				 * MPDU by comparing the number of buffers
				 * available and number of buffers needed to
				 * reap this MPDU
				 */
				if ((QDF_NBUF_CB_RX_PKT_LEN(rx_desc->nbuf) /
				     (buf_size -
				      soc->rx_pkt_tlv_size) + 1) >
				    num_pending) {
					DP_STATS_INC(soc,
						     rx.msdu_scatter_wait_break,
						     1);
					dp_rx_cookie_reset_invalid_bit(
								     ring_desc);
					/* As we are going to break out of the
					 * loop because of unavailability of
					 * descs to form complete SG, we need to
					 * reset the TP in the REO destination
					 * ring.
					 */
					hal_srng_dst_dec_tp(hal_soc,
							    hal_ring_hdl);
					break;
				}
				is_prev_msdu_last = false;
			}
		} else if (qdf_unlikely(!dp_rx_war_peek_msdu_done(soc,
								  rx_desc))) {
			struct dp_rx_desc *old_rx_desc =
					dp_rx_war_store_msdu_done_fail_desc(
								soc, rx_desc,
								reo_ring_num);
			if (qdf_likely(old_rx_desc)) {
				rx_bufs_reaped[rx_desc->chip_id][rx_desc->pool_id]++;
				dp_rx_add_to_free_desc_list
					(&head[rx_desc->chip_id][rx_desc->pool_id],
					 &tail[rx_desc->chip_id][rx_desc->pool_id],
					 old_rx_desc);
				quota -= 1;
				num_pending -= 1;
				num_rx_bufs_reaped++;
			}
			rx_desc->msdu_done_fail = 1;
			DP_STATS_INC(soc, rx.err.msdu_done_fail, 1);
			dp_err("MSDU DONE failure %d",
			       soc->stats.rx.err.msdu_done_fail);
			dp_rx_msdu_done_fail_event_record(soc, rx_desc,
							  rx_desc->nbuf);
			continue;
		}

		if (!is_prev_msdu_last &&
		    !(qdf_nbuf_is_rx_chfrag_cont(rx_desc->nbuf)))
			is_prev_msdu_last = true;

		rx_bufs_reaped[rx_desc->chip_id][rx_desc->pool_id]++;

		/*
		 * move unmap after scattered msdu waiting break logic
		 * in case double skb unmap happened.
		 */
		dp_rx_nbuf_unmap(soc, rx_desc, reo_ring_num);
		DP_RX_PROCESS_NBUF(soc, nbuf_head, nbuf_tail, ebuf_head,
				   ebuf_tail, rx_desc);

		quota -= 1;
		num_pending -= 1;

		dp_rx_add_to_free_desc_list
			(&head[rx_desc->chip_id][rx_desc->pool_id],
			 &tail[rx_desc->chip_id][rx_desc->pool_id], rx_desc);
		num_rx_bufs_reaped++;

		dp_rx_prefetch_hw_sw_nbuf_32_byte_desc(soc, hal_soc,
					       num_pending,
					       hal_ring_hdl,
					       &last_prefetched_hw_desc,
					       &last_prefetched_sw_desc);

		/*
		 * only if complete msdu is received for scatter case,
		 * then allow break.
		 */
		if (is_prev_msdu_last &&
		    dp_rx_reap_loop_pkt_limit_hit(soc, num_rx_bufs_reaped,
						  max_reap_limit))
			break;
	}
done:
	dp_rx_srng_access_end(int_ctx, soc, hal_ring_hdl);
	qdf_dsb();

	dp_rx_per_core_stats_update(soc, reo_ring_num, num_rx_bufs_reaped);

	for (chip_id = 0; chip_id < WLAN_MAX_MLO_CHIPS; chip_id++) {
		for (mac_id = 0; mac_id < MAX_PDEV_CNT; mac_id++) {
			/*
			 * continue with next mac_id if no pkts were reaped
			 * from that pool
			 */
			if (!rx_bufs_reaped[chip_id][mac_id])
				continue;

			replenish_soc = dp_rx_replenish_soc_get(soc, chip_id);

			dp_rxdma_srng =
				&replenish_soc->rx_refill_buf_ring[mac_id];

			rx_desc_pool = &replenish_soc->rx_desc_buf[mac_id];

			dp_rx_buffers_replenish_simple(replenish_soc, mac_id,
					       dp_rxdma_srng,
					       rx_desc_pool,
					       rx_bufs_reaped[chip_id][mac_id],
					       &head[chip_id][mac_id],
					       &tail[chip_id][mac_id]);
		}
	}

	/* Peer can be NULL is case of LFR */
	if (qdf_likely(txrx_peer))
		vdev = NULL;

	/*
	 * BIG loop where each nbuf is dequeued from global queue,
	 * processed and queued back on a per vdev basis. These nbufs
	 * are sent to stack as and when we run out of nbufs
	 * or a new nbuf dequeued from global queue has a different
	 * vdev when compared to previous nbuf.
	 */
	nbuf = nbuf_head;
	while (nbuf) {
		next = nbuf->next;
		dp_rx_prefetch_nbuf_data_be(nbuf, next);
		if (qdf_unlikely(dp_rx_is_raw_frame_dropped(nbuf))) {
			nbuf = next;
			dp_verbose_debug("drop raw frame");
			DP_STATS_INC(soc, rx.err.raw_frm_drop, 1);
			continue;
		}

		rx_tlv_hdr = qdf_nbuf_data(nbuf);
		vdev_id = QDF_NBUF_CB_RX_VDEV_ID(nbuf);
		peer_id = dp_rx_get_peer_id_be(nbuf);
		dp_rx_set_mpdu_seq_number_be(nbuf, rx_tlv_hdr);

		if (dp_rx_is_list_ready(deliver_list_head, vdev, txrx_peer,
					peer_id, vdev_id)) {
			dp_rx_deliver_to_stack(soc, vdev, txrx_peer,
					       deliver_list_head,
					       deliver_list_tail);
			deliver_list_head = NULL;
			deliver_list_tail = NULL;
		}

		/* Get TID from struct cb->tid_val, save to tid */
		tid = qdf_nbuf_get_tid_val(nbuf);
		if (qdf_unlikely(tid >= CDP_MAX_DATA_TIDS)) {
			DP_STATS_INC(soc, rx.err.rx_invalid_tid_err, 1);
			dp_verbose_debug("drop invalid tid");
			dp_rx_nbuf_free(nbuf);
			nbuf = next;
			continue;
		}

		if (qdf_unlikely(!txrx_peer)) {
			txrx_peer = dp_rx_get_txrx_peer_and_vdev(soc, nbuf,
								 peer_id,
								 &txrx_ref_handle,
								 pkt_capture_offload,
								 &vdev,
								 &rx_pdev, &dsf,
								 &old_tid);
			if (qdf_unlikely(!txrx_peer) || qdf_unlikely(!vdev)) {
				dp_verbose_debug("drop no peer frame");
				nbuf = next;
				continue;
			}
			enh_flag = rx_pdev->enhanced_stats_en;
		} else if (txrx_peer && txrx_peer->peer_id != peer_id) {
			dp_txrx_peer_unref_delete(txrx_ref_handle,
						  DP_MOD_ID_RX);

			txrx_peer = dp_rx_get_txrx_peer_and_vdev(soc, nbuf,
								 peer_id,
								 &txrx_ref_handle,
								 pkt_capture_offload,
								 &vdev,
								 &rx_pdev, &dsf,
								 &old_tid);
			if (qdf_unlikely(!txrx_peer) || qdf_unlikely(!vdev)) {
				dp_verbose_debug("drop by unmatch peer_id");
				nbuf = next;
				continue;
			}
			enh_flag = rx_pdev->enhanced_stats_en;
		}

		if (txrx_peer) {
			QDF_NBUF_CB_DP_TRACE_PRINT(nbuf) = false;
			qdf_dp_trace_set_track(nbuf, QDF_RX);
			QDF_NBUF_CB_RX_DP_TRACE(nbuf) = 1;
			QDF_NBUF_CB_RX_PACKET_TRACK(nbuf) =
				QDF_NBUF_RX_PKT_DATA_TRACK;
		}

		rx_bufs_used++;

		/* MLD Link Peer Statistics support */
		if (txrx_peer->is_mld_peer && rx_pdev->link_peer_stats) {
			link_id = dp_rx_get_stats_arr_idx_from_link_id(
								nbuf,
								txrx_peer);
		} else {
			link_id = 0;
		}

		dp_rx_set_nbuf_band(nbuf, txrx_peer, link_id);

		/* when hlos tid override is enabled, save tid in
		 * skb->priority
		 */
		if (qdf_unlikely(vdev->skip_sw_tid_classification &
					DP_TXRX_HLOS_TID_OVERRIDE_ENABLED))
			qdf_nbuf_set_priority(nbuf, tid);

		DP_RX_TID_SAVE(nbuf, tid);
		if (qdf_unlikely(dsf) || qdf_unlikely(peer_ext_stats) ||
		    dp_rx_pkt_tracepoints_enabled())
			qdf_nbuf_set_timestamp(nbuf);

		if (qdf_likely(old_tid != tid)) {
			tid_stats =
		&rx_pdev->stats.tid_stats.tid_rx_stats[reo_ring_num][tid];
			old_tid = tid;
		}

		/*
		 * Check if DMA completed -- msdu_done is the last bit
		 * to be written
		 */
		if (qdf_unlikely(!qdf_nbuf_is_rx_chfrag_cont(nbuf) &&
				 !hal_rx_tlv_msdu_done_get_be(rx_tlv_hdr))) {
			DP_STATS_INC(soc, rx.err.msdu_done_fail, 1);
			dp_err("MSDU DONE failure %d",
			       soc->stats.rx.err.msdu_done_fail);
			hal_rx_dump_pkt_tlvs(hal_soc, rx_tlv_hdr,
					     QDF_TRACE_LEVEL_INFO);
			dp_rx_msdu_done_fail_event_record(soc, NULL, nbuf);
			tid_stats->fail_cnt[MSDU_DONE_FAILURE]++;
			dp_rx_nbuf_free(nbuf);
			qdf_assert(0);
			nbuf = next;
			continue;
		}

		DP_HIST_PACKET_COUNT_INC(vdev->pdev->pdev_id);
		/*
		 * First IF condition:
		 * 802.11 Fragmented pkts are reinjected to REO
		 * HW block as SG pkts and for these pkts we only
		 * need to pull the RX TLVS header length.
		 * Second IF condition:
		 * The below condition happens when an MSDU is spread
		 * across multiple buffers. This can happen in two cases
		 * 1. The nbuf size is smaller then the received msdu.
		 *    ex: we have set the nbuf size to 2048 during
		 *        nbuf_alloc. but we received an msdu which is
		 *        2304 bytes in size then this msdu is spread
		 *        across 2 nbufs.
		 *
		 * 2. AMSDUs when RAW mode is enabled.
		 *    ex: 1st MSDU is in 1st nbuf and 2nd MSDU is spread
		 *        across 1st nbuf and 2nd nbuf and last MSDU is
		 *        spread across 2nd nbuf and 3rd nbuf.
		 *
		 * for these scenarios let us create a skb frag_list and
		 * append these buffers till the last MSDU of the AMSDU
		 * Third condition:
		 * This is the most likely case, we receive 802.3 pkts
		 * decapsulated by HW, here we need to set the pkt length.
		 */
		if (qdf_unlikely(qdf_nbuf_is_frag(nbuf))) {
			bool is_mcbc, is_sa_vld, is_da_vld;

			is_mcbc = hal_rx_msdu_end_da_is_mcbc_get(soc->hal_soc,
								 rx_tlv_hdr);
			is_sa_vld =
				hal_rx_msdu_end_sa_is_valid_get(soc->hal_soc,
								rx_tlv_hdr);
			is_da_vld =
				hal_rx_msdu_end_da_is_valid_get(soc->hal_soc,
								rx_tlv_hdr);

			qdf_nbuf_set_da_mcbc(nbuf, is_mcbc);
			qdf_nbuf_set_da_valid(nbuf, is_da_vld);
			qdf_nbuf_set_sa_valid(nbuf, is_sa_vld);

			qdf_nbuf_pull_head(nbuf, soc->rx_pkt_tlv_size);
		} else if (qdf_nbuf_is_rx_chfrag_cont(nbuf)) {
			msdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);
			nbuf = dp_rx_sg_create(soc, nbuf);
			next = nbuf->next;

			if (qdf_nbuf_is_raw_frame(nbuf)) {
				DP_STATS_INC(vdev->pdev, rx_raw_pkts, 1);
				DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer,
							      rx.raw, 1,
							      msdu_len,
							      link_id);
			} else {
				DP_STATS_INC(soc, rx.err.scatter_msdu, 1);

				if (!dp_rx_is_sg_supported()) {
					dp_rx_nbuf_free(nbuf);
					dp_info_rl("sg msdu len %d, dropped",
						   msdu_len);
					nbuf = next;
					continue;
				}
			}
		} else {
			l3_pad = hal_rx_get_l3_pad_bytes_be(nbuf, rx_tlv_hdr);
			msdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);
			pkt_len = msdu_len + l3_pad + soc->rx_pkt_tlv_size;

			qdf_nbuf_set_pktlen(nbuf, pkt_len);
			dp_rx_skip_tlvs(soc, nbuf, l3_pad);
		}

		dp_rx_send_pktlog(soc, rx_pdev, nbuf, QDF_TX_RX_STATUS_OK);

		if (!dp_wds_rx_policy_check(rx_tlv_hdr, vdev, txrx_peer)) {
			dp_rx_err("%pK: Policy Check Drop pkt", soc);
			DP_PEER_PER_PKT_STATS_INC(txrx_peer,
						  rx.policy_check_drop,
						  1, link_id);
			tid_stats->fail_cnt[POLICY_CHECK_DROP]++;
			/* Drop & free packet */
			dp_rx_nbuf_free(nbuf);
			/* Statistics */
			nbuf = next;
			continue;
		}

		/*
		 * Drop non-EAPOL frames from unauthorized peer.
		 */
		if (qdf_likely(txrx_peer) &&
		    qdf_unlikely(!txrx_peer->authorize) &&
		    !qdf_nbuf_is_raw_frame(nbuf)) {
			bool is_eapol = qdf_nbuf_is_ipv4_eapol_pkt(nbuf) ||
					qdf_nbuf_is_ipv4_wapi_pkt(nbuf);

			if (!is_eapol) {
				DP_PEER_PER_PKT_STATS_INC(txrx_peer,
							  rx.peer_unauth_rx_pkt_drop,
							  1, link_id);
				dp_verbose_debug("drop by unauthorized peer");
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				continue;
			}
		}

		dp_rx_cksum_offload(vdev->pdev, nbuf, rx_tlv_hdr);
		dp_rx_update_flow_info(nbuf, rx_tlv_hdr);

		if (qdf_unlikely(!rx_pdev->rx_fast_flag)) {
			/*
			 * process frame for mulitpass phrase processing
			 */
			if (qdf_unlikely(vdev->multipass_en)) {
				if (dp_rx_multipass_process(txrx_peer, nbuf,
							    tid) == false) {
					DP_PEER_PER_PKT_STATS_INC
						(txrx_peer,
						 rx.multipass_rx_pkt_drop,
						 1, link_id);
					dp_verbose_debug("drop multi pass");
					dp_rx_nbuf_free(nbuf);
					nbuf = next;
					continue;
				}
			}
			if (qdf_unlikely(txrx_peer &&
					 (txrx_peer->nawds_enabled) &&
					 (qdf_nbuf_is_da_mcbc(nbuf)) &&
					 (hal_rx_get_mpdu_mac_ad4_valid_be
						(rx_tlv_hdr) == false))) {
				tid_stats->fail_cnt[NAWDS_MCAST_DROP]++;
				DP_PEER_PER_PKT_STATS_INC(txrx_peer,
							  rx.nawds_mcast_drop,
							  1, link_id);
				dp_verbose_debug("drop nawds");
				dp_rx_nbuf_free(nbuf);
				nbuf = next;
				continue;
			}

			/* Update the protocol tag in SKB based on CCE metadata
			 */
			dp_rx_update_protocol_tag(soc, vdev, nbuf, rx_tlv_hdr,
						  reo_ring_num, false, true);

			/* Update the flow tag in SKB based on FSE metadata */
			dp_rx_update_flow_tag(soc, vdev, nbuf, rx_tlv_hdr,
					      true);

			if (qdf_unlikely(vdev->mesh_vdev)) {
				if (dp_rx_filter_mesh_packets(vdev, nbuf,
							      rx_tlv_hdr)
						== QDF_STATUS_SUCCESS) {
					dp_rx_info("%pK: mesh pkt filtered",
						   soc);
					tid_stats->fail_cnt[MESH_FILTER_DROP]++;
					DP_STATS_INC(vdev->pdev,
						     dropped.mesh_filter, 1);

					dp_rx_nbuf_free(nbuf);
					nbuf = next;
					continue;
				}
				dp_rx_fill_mesh_stats(vdev, nbuf, rx_tlv_hdr,
						      txrx_peer);
			}
		}

		if (qdf_likely(vdev->rx_decap_type ==
			       htt_cmn_pkt_type_ethernet) &&
		    qdf_likely(!vdev->mesh_vdev)) {
			dp_rx_wds_learn(soc, vdev,
					rx_tlv_hdr,
					txrx_peer,
					nbuf);
		}

		dp_rx_msdu_stats_update(soc, nbuf, rx_tlv_hdr, txrx_peer,
					reo_ring_num, tid_stats, link_id);

		if (qdf_likely(vdev->rx_decap_type ==
			       htt_cmn_pkt_type_ethernet) &&
		    qdf_likely(!vdev->mesh_vdev)) {
			/* Intrabss-fwd */
			if (dp_rx_check_ap_bridge(vdev))
				if (dp_rx_intrabss_fwd_be(soc, txrx_peer,
							  rx_tlv_hdr,
							  nbuf,
							  link_id)) {
					nbuf = next;
					tid_stats->intrabss_cnt++;
					continue; /* Get next desc */
				}
		}

		dp_rx_fill_gro_info(soc, rx_tlv_hdr, nbuf, &rx_ol_pkt_cnt);

		dp_rx_mark_first_packet_after_wow_wakeup(vdev->pdev, rx_tlv_hdr,
							 nbuf);

		dp_rx_update_stats(soc, nbuf);

		dp_pkt_add_timestamp(txrx_peer->vdev, QDF_PKT_RX_DRIVER_ENTRY,
				     current_time, nbuf);

		DP_RX_LIST_APPEND(deliver_list_head,
				  deliver_list_tail,
				  nbuf);

		DP_PEER_TO_STACK_INCC_PKT(txrx_peer, 1,
					  QDF_NBUF_CB_RX_PKT_LEN(nbuf),
					  enh_flag);
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer,
					      rx.rx_success, 1,
					      QDF_NBUF_CB_RX_PKT_LEN(nbuf),
					      link_id);

		if (qdf_unlikely(txrx_peer->in_twt))
			DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer,
						      rx.to_stack_twt, 1,
						      QDF_NBUF_CB_RX_PKT_LEN(nbuf),
						      link_id);

		tid_stats->delivered_to_stack++;
		nbuf = next;
	}

	DP_RX_DELIVER_TO_STACK(soc, vdev, txrx_peer, peer_id,
			       pkt_capture_offload,
			       deliver_list_head,
			       deliver_list_tail);

	if (qdf_likely(txrx_peer))
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);

	/*
	 * If we are processing in near-full condition, there are 3 scenario
	 * 1) Ring entries has reached critical state
	 * 2) Ring entries are still near high threshold
	 * 3) Ring entries are below the safe level
	 *
	 * One more loop will move the state to normal processing and yield
	 */
	if (ring_near_full && quota)
		goto more_data;

	if (dp_rx_enable_eol_data_check(soc) && rx_bufs_used) {
		if (quota) {
			num_pending =
				dp_rx_srng_get_num_pending(hal_soc,
							   hal_ring_hdl,
							   num_entries,
							   &near_full);
			if (num_pending) {
				DP_STATS_INC(soc, rx.hp_oos2, 1);

				if (!hif_exec_should_yield(scn, intr_id))
					goto more_data;

				if (qdf_unlikely(near_full)) {
					DP_STATS_INC(soc, rx.near_full, 1);
					goto more_data;
				}
			}
		}

		if (vdev && vdev->osif_fisa_flush)
			vdev->osif_fisa_flush(soc, reo_ring_num);

		if (vdev && vdev->osif_gro_flush && rx_ol_pkt_cnt) {
			vdev->osif_gro_flush(vdev->osif_vdev,
					     reo_ring_num);
		}
	}

	/* Update histogram statistics by looping through pdev's */
	DP_RX_HIST_STATS_PER_PDEV();

	return rx_bufs_used; /* Assume no scale factor for now */
}

#ifdef RX_DESC_MULTI_PAGE_ALLOC
/**
 * dp_rx_desc_pool_init_be_cc() - initial RX desc pool for cookie conversion
 * @soc: Handle to DP Soc structure
 * @rx_desc_pool: Rx descriptor pool handler
 * @pool_id: Rx descriptor pool ID
 *
 * Return: QDF_STATUS_SUCCESS - succeeded, others - failed
 */
static QDF_STATUS
dp_rx_desc_pool_init_be_cc(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   uint32_t pool_id)
{
	struct dp_hw_cookie_conversion_t *cc_ctx;
	struct dp_soc_be *be_soc;
	union dp_rx_desc_list_elem_t *rx_desc_elem;
	struct dp_spt_page_desc *page_desc;
	uint32_t ppt_idx = 0;
	uint32_t avail_entry_index = 0;

	if (!rx_desc_pool->pool_size) {
		dp_err("desc_num 0 !!");
		return QDF_STATUS_E_FAILURE;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	cc_ctx  = &be_soc->rx_cc_ctx[pool_id];

	page_desc = &cc_ctx->page_desc_base[0];
	rx_desc_elem = rx_desc_pool->freelist;
	while (rx_desc_elem) {
		if (avail_entry_index == 0) {
			if (ppt_idx >= cc_ctx->total_page_num) {
				dp_alert("insufficient secondary page tables");
				qdf_assert_always(0);
			}
			page_desc = &cc_ctx->page_desc_base[ppt_idx++];
		}

		/* put each RX Desc VA to SPT pages and
		 * get corresponding ID
		 */
		DP_CC_SPT_PAGE_UPDATE_VA(page_desc->page_v_addr,
					 avail_entry_index,
					 &rx_desc_elem->rx_desc);
		rx_desc_elem->rx_desc.cookie =
			dp_cc_desc_id_generate(page_desc->ppt_index,
					       avail_entry_index);
		rx_desc_elem->rx_desc.chip_id = dp_mlo_get_chip_id(soc);
		rx_desc_elem->rx_desc.pool_id = pool_id;
		rx_desc_elem->rx_desc.in_use = 0;
		rx_desc_elem = rx_desc_elem->next;

		avail_entry_index = (avail_entry_index + 1) &
					DP_CC_SPT_PAGE_MAX_ENTRIES_MASK;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS
dp_rx_desc_pool_init_be_cc(struct dp_soc *soc,
			   struct rx_desc_pool *rx_desc_pool,
			   uint32_t pool_id)
{
	struct dp_hw_cookie_conversion_t *cc_ctx;
	struct dp_soc_be *be_soc;
	struct dp_spt_page_desc *page_desc;
	uint32_t ppt_idx = 0;
	uint32_t avail_entry_index = 0;
	int i = 0;

	if (!rx_desc_pool->pool_size) {
		dp_err("desc_num 0 !!");
		return QDF_STATUS_E_FAILURE;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	cc_ctx  = &be_soc->rx_cc_ctx[pool_id];

	page_desc = &cc_ctx->page_desc_base[0];
	for (i = 0; i <= rx_desc_pool->pool_size - 1; i++) {
		if (i == rx_desc_pool->pool_size - 1)
			rx_desc_pool->array[i].next = NULL;
		else
			rx_desc_pool->array[i].next =
				&rx_desc_pool->array[i + 1];

		if (avail_entry_index == 0) {
			if (ppt_idx >= cc_ctx->total_page_num) {
				dp_alert("insufficient secondary page tables");
				qdf_assert_always(0);
			}
			page_desc = &cc_ctx->page_desc_base[ppt_idx++];
		}

		/* put each RX Desc VA to SPT pages and
		 * get corresponding ID
		 */
		DP_CC_SPT_PAGE_UPDATE_VA(page_desc->page_v_addr,
					 avail_entry_index,
					 &rx_desc_pool->array[i].rx_desc);
		rx_desc_pool->array[i].rx_desc.cookie =
			dp_cc_desc_id_generate(page_desc->ppt_index,
					       avail_entry_index);
		rx_desc_pool->array[i].rx_desc.pool_id = pool_id;
		rx_desc_pool->array[i].rx_desc.in_use = 0;
		rx_desc_pool->array[i].rx_desc.chip_id =
					dp_mlo_get_chip_id(soc);

		avail_entry_index = (avail_entry_index + 1) &
					DP_CC_SPT_PAGE_MAX_ENTRIES_MASK;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

static void
dp_rx_desc_pool_deinit_be_cc(struct dp_soc *soc,
			     struct rx_desc_pool *rx_desc_pool,
			     uint32_t pool_id)
{
	struct dp_spt_page_desc *page_desc;
	struct dp_soc_be *be_soc;
	int i = 0;
	struct dp_hw_cookie_conversion_t *cc_ctx;

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	cc_ctx  = &be_soc->rx_cc_ctx[pool_id];

	for (i = 0; i < cc_ctx->total_page_num; i++) {
		page_desc = &cc_ctx->page_desc_base[i];
		qdf_mem_zero(page_desc->page_v_addr, qdf_page_size);
	}
}

QDF_STATUS dp_rx_desc_pool_init_be(struct dp_soc *soc,
				   struct rx_desc_pool *rx_desc_pool,
				   uint32_t pool_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Only regular RX buffer desc pool use HW cookie conversion */
	if (rx_desc_pool->desc_type == QDF_DP_RX_DESC_BUF_TYPE) {
		dp_info("rx_desc_buf pool init");
		status = dp_rx_desc_pool_init_be_cc(soc,
						    rx_desc_pool,
						    pool_id);
	} else {
		dp_info("non_rx_desc_buf_pool init");
		status = dp_rx_desc_pool_init_generic(soc, rx_desc_pool,
						      pool_id);
	}

	return status;
}

void dp_rx_desc_pool_deinit_be(struct dp_soc *soc,
			       struct rx_desc_pool *rx_desc_pool,
			       uint32_t pool_id)
{
	if (rx_desc_pool->desc_type == QDF_DP_RX_DESC_BUF_TYPE)
		dp_rx_desc_pool_deinit_be_cc(soc, rx_desc_pool, pool_id);
}

#ifdef DP_FEATURE_HW_COOKIE_CONVERSION
#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
QDF_STATUS dp_wbm_get_rx_desc_from_hal_desc_be(struct dp_soc *soc,
					       void *ring_desc,
					       struct dp_rx_desc **r_rx_desc)
{
	if (hal_rx_wbm_get_cookie_convert_done(ring_desc)) {
		/* HW cookie conversion done */
		*r_rx_desc = (struct dp_rx_desc *)
				hal_rx_wbm_get_desc_va(ring_desc);
	} else {
		/* SW do cookie conversion */
		uint32_t cookie = HAL_RX_BUF_COOKIE_GET(ring_desc);

		*r_rx_desc = (struct dp_rx_desc *)
				dp_cc_desc_find(soc, cookie);
	}

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS dp_wbm_get_rx_desc_from_hal_desc_be(struct dp_soc *soc,
					       void *ring_desc,
					       struct dp_rx_desc **r_rx_desc)
{
	 *r_rx_desc = (struct dp_rx_desc *)
			hal_rx_wbm_get_desc_va(ring_desc);

	return QDF_STATUS_SUCCESS;
}
#endif /* DP_HW_COOKIE_CONVERT_EXCEPTION */
struct dp_rx_desc *dp_rx_desc_ppeds_cookie_2_va(struct dp_soc *soc,
						unsigned long cookie)
{
	return (struct dp_rx_desc *)cookie;
}

#else
struct dp_rx_desc *dp_rx_desc_ppeds_cookie_2_va(struct dp_soc *soc,
						unsigned long cookie)
{
	if (!cookie)
		return NULL;

	return (struct dp_rx_desc *)dp_cc_desc_find(soc, cookie);
}

QDF_STATUS dp_wbm_get_rx_desc_from_hal_desc_be(struct dp_soc *soc,
					       void *ring_desc,
					       struct dp_rx_desc **r_rx_desc)
{
	/* SW do cookie conversion */
	uint32_t cookie = HAL_RX_BUF_COOKIE_GET(ring_desc);

	*r_rx_desc = (struct dp_rx_desc *)
			dp_cc_desc_find(soc, cookie);

	return QDF_STATUS_SUCCESS;
}
#endif /* DP_FEATURE_HW_COOKIE_CONVERSION */

struct dp_rx_desc *dp_rx_desc_cookie_2_va_be(struct dp_soc *soc,
					     uint32_t cookie)
{
	return (struct dp_rx_desc *)dp_cc_desc_find(soc, cookie);
}

#if defined(WLAN_FEATURE_11BE_MLO)
#if defined(WLAN_MLO_MULTI_CHIP) && defined(WLAN_MCAST_MLO)
#define DP_RANDOM_MAC_ID_BIT_MASK	0xC0
#define DP_RANDOM_MAC_OFFSET	1
#define DP_MAC_LOCAL_ADMBIT_MASK	0x2
#define DP_MAC_LOCAL_ADMBIT_OFFSET	0
static inline void dp_rx_dummy_src_mac(struct dp_vdev *vdev,
				       qdf_nbuf_t nbuf)
{
	qdf_ether_header_t *eh =
			(qdf_ether_header_t *)qdf_nbuf_data(nbuf);

	eh->ether_shost[DP_MAC_LOCAL_ADMBIT_OFFSET] =
				eh->ether_shost[DP_MAC_LOCAL_ADMBIT_OFFSET] |
				DP_MAC_LOCAL_ADMBIT_MASK;
}

#ifdef QCA_SUPPORT_WDS_EXTENDED
static inline bool dp_rx_mlo_igmp_wds_ext_handler(struct dp_txrx_peer *peer)
{
	return qdf_atomic_test_bit(WDS_EXT_PEER_INIT_BIT, &peer->wds_ext.init);
}
#else
static inline bool dp_rx_mlo_igmp_wds_ext_handler(struct dp_txrx_peer *peer)
{
	return false;
}
#endif

#ifdef EXT_HYBRID_MLO_MODE
static inline
bool dp_rx_check_ext_hybrid_mode(struct dp_soc *soc, struct dp_vdev *vdev)
{
	return ((DP_MLD_MODE_HYBRID_NONBOND == soc->mld_mode_ap) &&
		(wlan_op_mode_ap == vdev->opmode));
}
#else
static inline
bool dp_rx_check_ext_hybrid_mode(struct dp_soc *soc, struct dp_vdev *vdev)
{
	return false;
}
#endif

bool dp_rx_mlo_igmp_handler(struct dp_soc *soc,
			    struct dp_vdev *vdev,
			    struct dp_txrx_peer *peer,
			    qdf_nbuf_t nbuf,
			    uint8_t link_id)
{
	qdf_nbuf_t nbuf_copy;
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	uint8_t tid = qdf_nbuf_get_tid_val(nbuf);
	struct cdp_tid_rx_stats *tid_stats = &peer->vdev->pdev->stats.
					tid_stats.tid_rx_wbm_stats[0][tid];

	if (!(qdf_nbuf_is_ipv4_igmp_pkt(nbuf) ||
	      qdf_nbuf_is_ipv6_igmp_pkt(nbuf)))
		return false;

	if (qdf_unlikely(vdev->multipass_en)) {
		if (dp_rx_multipass_process(peer, nbuf, tid) == false) {
			DP_PEER_PER_PKT_STATS_INC(peer,
						  rx.multipass_rx_pkt_drop,
						  1, link_id);
			return false;
		}
	}

	if (!peer->bss_peer) {
		if (dp_rx_intrabss_mcbc_fwd(soc, peer, NULL, nbuf,
					    tid_stats, link_id))
			dp_rx_err("forwarding failed");
	}

	qdf_nbuf_set_next(nbuf, NULL);

	/* REO sends IGMP to driver only if AP is operating in hybrid
	 *  mld mode.
	 */

	if (qdf_unlikely(dp_rx_mlo_igmp_wds_ext_handler(peer))) {
		/* send the IGMP to the netdev corresponding to the interface
		 * its received on
		 */
		goto send_pkt;
	}

	if (dp_rx_check_ext_hybrid_mode(soc, vdev)) {
		/* send the IGMP to the netdev corresponding to the interface
		 * its received on
		 */
		goto send_pkt;
	}

	/*
	 * In the case of ME5/ME6, Backhaul WDS for a mld peer, NAWDS,
	 * legacy non-mlo AP vdev & non-AP vdev(which is very unlikely),
	 * send the igmp pkt on the same link where it received, as these
	 *  features will use peer based tcl metadata.
	 */
	if (vdev->mcast_enhancement_en ||
	    peer->is_mld_peer ||
	    peer->nawds_enabled ||
	    !vdev->mlo_vdev ||
	    qdf_unlikely(wlan_op_mode_ap != vdev->opmode)) {
		/* send the IGMP to the netdev corresponding to the interface
		 * its received on
		 */
		goto send_pkt;
	}

	/* We are here, it means a legacy non-wds sta is connected
	 * to a hybrid mld ap, So send a clone of the IGPMP packet
	 * on the interface where it was received.
	 */
	nbuf_copy = qdf_nbuf_copy(nbuf);
	if (qdf_likely(nbuf_copy))
		dp_rx_deliver_to_stack(soc, vdev, peer, nbuf_copy, NULL);

	dp_rx_dummy_src_mac(vdev, nbuf);
	/* Set the ml peer valid bit in skb peer metadata, so that osif
	 * can deliver the SA mangled IGMP packet to mld netdev.
	 */
	QDF_NBUF_CB_RX_PEER_ID(nbuf) |= CDP_RX_ML_PEER_VALID_MASK;
	/* Deliver the original IGMP with dummy src on the mld netdev */
send_pkt:
	dp_rx_deliver_to_stack(be_vdev->vdev.pdev->soc,
			       &be_vdev->vdev,
			       peer,
			       nbuf,
			       NULL);
	return true;
}
#else
bool dp_rx_mlo_igmp_handler(struct dp_soc *soc,
			    struct dp_vdev *vdev,
			    struct dp_txrx_peer *peer,
			    qdf_nbuf_t nbuf,
			    uint8_t link_id)
{
	return false;
}
#endif
#endif

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
uint32_t dp_rx_nf_process(struct dp_intr *int_ctx,
			  hal_ring_handle_t hal_ring_hdl,
			  uint8_t reo_ring_num,
			  uint32_t quota)
{
	struct dp_soc *soc = int_ctx->soc;
	struct dp_srng *rx_ring = &soc->reo_dest_ring[reo_ring_num];
	uint32_t work_done = 0;

	if (dp_srng_get_near_full_level(soc, rx_ring) <
			DP_SRNG_THRESH_NEAR_FULL)
		return 0;

	qdf_atomic_set(&rx_ring->near_full, 1);
	work_done++;

	return work_done;
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef WLAN_FEATURE_11BE_MLO
/**
 * dp_rx_intrabss_fwd_mlo_allow() - check if MLO forwarding is allowed
 * @ta_peer: transmitter peer handle
 * @da_peer: destination peer handle
 *
 * Return: true - MLO forwarding case, false: not
 */
static inline bool
dp_rx_intrabss_fwd_mlo_allow(struct dp_txrx_peer *ta_peer,
			     struct dp_txrx_peer *da_peer)
{
	/* TA peer and DA peer's vdev should be partner MLO vdevs */
	if (dp_peer_find_mac_addr_cmp(&ta_peer->vdev->mld_mac_addr,
				      &da_peer->vdev->mld_mac_addr))
		return false;

	return true;
}
#else
static inline bool
dp_rx_intrabss_fwd_mlo_allow(struct dp_txrx_peer *ta_peer,
			     struct dp_txrx_peer *da_peer)
{
	return false;
}
#endif

#ifdef INTRA_BSS_FWD_OFFLOAD
/**
 * dp_rx_intrabss_ucast_check_be() - Check if intrabss is allowed
 *				     for unicast frame
 * @nbuf: RX packet buffer
 * @ta_peer: transmitter DP peer handle
 * @rx_tlv_hdr: Rx TLV header
 * @msdu_metadata: MSDU meta data info
 * @params: params to be filled in
 *
 * Return: true - intrabss allowed
 *	   false - not allow
 */
static bool
dp_rx_intrabss_ucast_check_be(qdf_nbuf_t nbuf,
			      struct dp_txrx_peer *ta_peer,
			      uint8_t *rx_tlv_hdr,
			      struct hal_rx_msdu_metadata *msdu_metadata,
			      struct dp_be_intrabss_params *params)
{
	uint8_t dest_chip_id, dest_chip_pmac_id;
	struct dp_vdev_be *be_vdev =
		dp_get_be_vdev_from_dp_vdev(ta_peer->vdev);
	struct dp_soc_be *be_soc =
		dp_get_be_soc_from_dp_soc(params->dest_soc);
	uint16_t da_peer_id;
	struct dp_peer *da_peer = NULL;

	if (!qdf_nbuf_is_intra_bss(nbuf))
		return false;

	hal_rx_tlv_get_dest_chip_pmac_id(rx_tlv_hdr,
					 &dest_chip_id,
					 &dest_chip_pmac_id);

	if (dp_assert_always_internal_stat(
				(dest_chip_id <= (DP_MLO_MAX_DEST_CHIP_ID - 1)),
				&be_soc->soc, rx.err.intra_bss_bad_chipid))
		return false;

	params->dest_soc =
		dp_mlo_get_soc_ref_by_chip_id(be_soc->ml_ctxt,
					      dest_chip_id);
	if (!params->dest_soc)
		return false;

	da_peer_id = HAL_RX_PEER_ID_GET(msdu_metadata);

	da_peer = dp_peer_get_tgt_peer_by_id(params->dest_soc, da_peer_id,
					     DP_MOD_ID_RX);
	if (da_peer) {
		if (da_peer->bss_peer || (da_peer->txrx_peer == ta_peer)) {
			dp_peer_unref_delete(da_peer, DP_MOD_ID_RX);
			return false;
		}
		dp_peer_unref_delete(da_peer, DP_MOD_ID_RX);
	}

	if (!be_vdev->mlo_dev_ctxt) {
		params->tx_vdev_id = ta_peer->vdev->vdev_id;
		return true;
	}

	if (dest_chip_id == be_soc->mlo_chip_id) {
		if (dest_chip_pmac_id == ta_peer->vdev->pdev->pdev_id)
			params->tx_vdev_id = ta_peer->vdev->vdev_id;
		else
			params->tx_vdev_id =
				be_vdev->mlo_dev_ctxt->vdev_list[dest_chip_id]
							  [dest_chip_pmac_id];
		return true;
	}

	params->tx_vdev_id =
		be_vdev->mlo_dev_ctxt->vdev_list[dest_chip_id]
						[dest_chip_pmac_id];

	return true;
}
#else
#ifdef WLAN_MLO_MULTI_CHIP
static bool
dp_rx_intrabss_ucast_check_be(qdf_nbuf_t nbuf,
			      struct dp_txrx_peer *ta_peer,
			      uint8_t *rx_tlv_hdr,
			      struct hal_rx_msdu_metadata *msdu_metadata,
			      struct dp_be_intrabss_params *params)
{
	uint16_t da_peer_id;
	struct dp_txrx_peer *da_peer;
	bool ret = false;
	uint8_t dest_chip_id;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	struct dp_vdev_be *be_vdev =
		dp_get_be_vdev_from_dp_vdev(ta_peer->vdev);
	struct dp_soc_be *be_soc =
		dp_get_be_soc_from_dp_soc(params->dest_soc);

	if (!(qdf_nbuf_is_da_valid(nbuf) || qdf_nbuf_is_da_mcbc(nbuf)))
		return false;

	dest_chip_id = HAL_RX_DEST_CHIP_ID_GET(msdu_metadata);
	if (dp_assert_always_internal_stat(
				(dest_chip_id <= (DP_MLO_MAX_DEST_CHIP_ID - 1)),
				&be_soc->soc, rx.err.intra_bss_bad_chipid))
		return false;

	da_peer_id = HAL_RX_PEER_ID_GET(msdu_metadata);

	/* use dest chip id when TA is MLD peer and DA is legacy */
	if (be_soc->mlo_enabled &&
	    ta_peer->mld_peer &&
	    !(da_peer_id & HAL_RX_DA_IDX_ML_PEER_MASK)) {
		/* validate chip_id, get a ref, and re-assign soc */
		params->dest_soc =
			dp_mlo_get_soc_ref_by_chip_id(be_soc->ml_ctxt,
						      dest_chip_id);
		if (!params->dest_soc)
			return false;

		da_peer = dp_txrx_peer_get_ref_by_id(params->dest_soc,
						     da_peer_id,
						     &txrx_ref_handle,
						     DP_MOD_ID_RX);
		if (!da_peer)
			return false;

	} else {
		da_peer = dp_txrx_peer_get_ref_by_id(params->dest_soc,
						     da_peer_id,
						     &txrx_ref_handle,
						     DP_MOD_ID_RX);
		if (!da_peer)
			return false;

		params->dest_soc = da_peer->vdev->pdev->soc;
		if (!params->dest_soc)
			goto rel_da_peer;

	}

	params->tx_vdev_id = da_peer->vdev->vdev_id;

	/* If the source or destination peer in the isolation
	 * list then dont forward instead push to bridge stack.
	 */
	if (dp_get_peer_isolation(ta_peer) ||
	    dp_get_peer_isolation(da_peer)) {
		ret = false;
		goto rel_da_peer;
	}

	if (da_peer->bss_peer || (da_peer == ta_peer)) {
		ret = false;
		goto rel_da_peer;
	}

	/* Same vdev, support Inra-BSS */
	if (da_peer->vdev == ta_peer->vdev) {
		ret = true;
		goto rel_da_peer;
	}

	if (!be_vdev->mlo_dev_ctxt)
		ret = false;
		goto rel_da_peer;
	}

	/* MLO specific Intra-BSS check */
	if (dp_rx_intrabss_fwd_mlo_allow(ta_peer, da_peer)) {
		/* use dest chip id for legacy dest peer */
		if (!(da_peer_id & HAL_RX_DA_IDX_ML_PEER_MASK)) {
			if (!(be_vdev->mlo_dev_ctxt->vdev_list[dest_chip_id][0]
			      == params->tx_vdev_id) &&
			    !(be_vdev->mlo_dev_ctxt->vdev_list[dest_chip_id][1]
			      == params->tx_vdev_id)) {
				/*dp_soc_unref_delete(soc);*/
				goto rel_da_peer;
			}
		}
		ret = true;
	}

rel_da_peer:
	dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
	return ret;
}
#else
static bool
dp_rx_intrabss_ucast_check_be(qdf_nbuf_t nbuf,
			      struct dp_txrx_peer *ta_peer,
			      uint8_t *rx_tlv_hdr,
			      struct hal_rx_msdu_metadata *msdu_metadata,
			      struct dp_be_intrabss_params *params)
{
	uint16_t da_peer_id;
	struct dp_txrx_peer *da_peer;
	bool ret = false;
	dp_txrx_ref_handle txrx_ref_handle = NULL;

	if (!qdf_nbuf_is_da_valid(nbuf) || qdf_nbuf_is_da_mcbc(nbuf))
		return false;

	da_peer_id = dp_rx_peer_metadata_peer_id_get_be(
						params->dest_soc,
						msdu_metadata->da_idx);

	da_peer = dp_txrx_peer_get_ref_by_id(params->dest_soc, da_peer_id,
					     &txrx_ref_handle, DP_MOD_ID_RX);
	if (!da_peer)
		return false;

	params->tx_vdev_id = da_peer->vdev->vdev_id;
	/* If the source or destination peer in the isolation
	 * list then dont forward instead push to bridge stack.
	 */
	if (dp_get_peer_isolation(ta_peer) ||
	    dp_get_peer_isolation(da_peer))
		goto rel_da_peer;

	if (da_peer->bss_peer || da_peer == ta_peer)
		goto rel_da_peer;

	/* Same vdev, support Inra-BSS */
	if (da_peer->vdev == ta_peer->vdev) {
		ret = true;
		goto rel_da_peer;
	}

	/* MLO specific Intra-BSS check */
	if (dp_rx_intrabss_fwd_mlo_allow(ta_peer, da_peer)) {
		ret = true;
		goto rel_da_peer;
	}

rel_da_peer:
	dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_RX);
	return ret;
}
#endif /* WLAN_MLO_MULTI_CHIP */
#endif /* INTRA_BSS_FWD_OFFLOAD */

#if defined(WLAN_PKT_CAPTURE_RX_2_0) || defined(CONFIG_WORD_BASED_TLV)
void dp_rx_word_mask_subscribe_be(struct dp_soc *soc,
				  uint32_t *msg_word,
				  void *rx_filter)
{
	struct htt_rx_ring_tlv_filter *tlv_filter =
				(struct htt_rx_ring_tlv_filter *)rx_filter;

	if (!msg_word || !tlv_filter)
		return;

	/* tlv_filter->enable is set to 1 for monitor rings */
	if (tlv_filter->enable)
		return;

	/* if word mask is zero, FW will set the default values */
	if (!(tlv_filter->rx_mpdu_start_wmask > 0 &&
	      tlv_filter->rx_msdu_end_wmask > 0)) {
		return;
	}

	HTT_RX_RING_SELECTION_CFG_WORD_MASK_COMPACTION_ENABLE_SET(*msg_word, 1);

	/* word 14 */
	msg_word += 3;
	*msg_word = 0;

	HTT_RX_RING_SELECTION_CFG_RX_MPDU_START_WORD_MASK_SET(
				*msg_word,
				tlv_filter->rx_mpdu_start_wmask);

	/* word 15 */
	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_SELECTION_CFG_RX_MSDU_END_WORD_MASK_SET(
				*msg_word,
				tlv_filter->rx_msdu_end_wmask);
}
#else
void dp_rx_word_mask_subscribe_be(struct dp_soc *soc,
				  uint32_t *msg_word,
				  void *rx_filter)
{
}
#endif

#if defined(WLAN_MCAST_MLO) && defined(CONFIG_MLO_SINGLE_DEV)
static inline
bool dp_rx_intrabss_mlo_mcbc_fwd(struct dp_soc *soc, struct dp_vdev *vdev,
				 qdf_nbuf_t nbuf_copy)
{
	struct dp_vdev *mcast_primary_vdev = NULL;
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct cdp_tx_exception_metadata tx_exc_metadata = {0};

	tx_exc_metadata.is_mlo_mcast = 1;
	tx_exc_metadata.tx_encap_type = CDP_INVALID_TX_ENCAP_TYPE;
	tx_exc_metadata.sec_type = CDP_INVALID_SEC_TYPE;
	tx_exc_metadata.peer_id = CDP_INVALID_PEER;
	tx_exc_metadata.tid = CDP_INVALID_TID;

	mcast_primary_vdev = dp_mlo_get_mcast_primary_vdev(be_soc,
							   be_vdev,
							   DP_MOD_ID_RX);

	if (!mcast_primary_vdev)
		return false;

	nbuf_copy = dp_tx_send_exception((struct cdp_soc_t *)
					 mcast_primary_vdev->pdev->soc,
					 mcast_primary_vdev->vdev_id,
					 nbuf_copy, &tx_exc_metadata);

	if (nbuf_copy)
		qdf_nbuf_free(nbuf_copy);

	dp_vdev_unref_delete(mcast_primary_vdev->pdev->soc,
			     mcast_primary_vdev, DP_MOD_ID_RX);
	return true;
}
#else
static inline
bool dp_rx_intrabss_mlo_mcbc_fwd(struct dp_soc *soc, struct dp_vdev *vdev,
				 qdf_nbuf_t nbuf_copy)
{
	return false;
}
#endif

bool
dp_rx_intrabss_mcast_handler_be(struct dp_soc *soc,
				struct dp_txrx_peer *ta_txrx_peer,
				qdf_nbuf_t nbuf_copy,
				struct cdp_tid_rx_stats *tid_stats,
				uint8_t link_id)
{
	if (qdf_unlikely(ta_txrx_peer->vdev->nawds_enabled)) {
		struct cdp_tx_exception_metadata tx_exc_metadata = {0};
		uint16_t len = QDF_NBUF_CB_RX_PKT_LEN(nbuf_copy);

		tx_exc_metadata.peer_id = ta_txrx_peer->peer_id;
		tx_exc_metadata.is_intrabss_fwd = 1;
		tx_exc_metadata.tid = HTT_TX_EXT_TID_INVALID;

		if (dp_tx_send_exception((struct cdp_soc_t *)soc,
					  ta_txrx_peer->vdev->vdev_id,
					  nbuf_copy,
					  &tx_exc_metadata)) {
			DP_PEER_PER_PKT_STATS_INC_PKT(ta_txrx_peer,
						      rx.intra_bss.fail, 1,
						      len, link_id);
			tid_stats->fail_cnt[INTRABSS_DROP]++;
			qdf_nbuf_free(nbuf_copy);
		} else {
			DP_PEER_PER_PKT_STATS_INC_PKT(ta_txrx_peer,
						      rx.intra_bss.pkts, 1,
						      len, link_id);
			tid_stats->intrabss_cnt++;
		}
		return true;
	}

	if (dp_rx_intrabss_mlo_mcbc_fwd(soc, ta_txrx_peer->vdev,
					nbuf_copy))
		return true;

	return false;
}

bool dp_rx_intrabss_fwd_be(struct dp_soc *soc, struct dp_txrx_peer *ta_peer,
			   uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf,
			   uint8_t link_id)
{
	uint8_t tid = qdf_nbuf_get_tid_val(nbuf);
	uint8_t ring_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	struct cdp_tid_rx_stats *tid_stats = &ta_peer->vdev->pdev->stats.
					tid_stats.tid_rx_stats[ring_id][tid];
	bool ret = false;
	struct dp_be_intrabss_params params;
	struct hal_rx_msdu_metadata msdu_metadata;

	/* if it is a broadcast pkt (eg: ARP) and it is not its own
	 * source, then clone the pkt and send the cloned pkt for
	 * intra BSS forwarding and original pkt up the network stack
	 * Note: how do we handle multicast pkts. do we forward
	 * all multicast pkts as is or let a higher layer module
	 * like igmpsnoop decide whether to forward or not with
	 * Mcast enhancement.
	 */
	if (qdf_nbuf_is_da_mcbc(nbuf) && !ta_peer->bss_peer) {
		return dp_rx_intrabss_mcbc_fwd(soc, ta_peer, rx_tlv_hdr,
					       nbuf, tid_stats, link_id);
	}

	if (dp_rx_intrabss_eapol_drop_check(soc, ta_peer, rx_tlv_hdr,
					    nbuf))
		return true;

	hal_rx_msdu_packet_metadata_get_generic_be(rx_tlv_hdr, &msdu_metadata);
	params.dest_soc = soc;
	if (dp_rx_intrabss_ucast_check_be(nbuf, ta_peer, rx_tlv_hdr,
					  &msdu_metadata, &params)) {
		ret = dp_rx_intrabss_ucast_fwd(params.dest_soc, ta_peer,
					       params.tx_vdev_id,
					       rx_tlv_hdr, nbuf, tid_stats,
					       link_id);
	}

	return ret;
}
#endif

#ifndef BE_WBM_RELEASE_DESC_RX_SG_SUPPORT
/**
 * dp_rx_chain_msdus_be() - Function to chain all msdus of a mpdu
 *			    to pdev invalid peer list
 *
 * @soc: core DP main context
 * @nbuf: Buffer pointer
 * @rx_tlv_hdr: start of rx tlv header
 * @mac_id: mac id
 *
 *  Return: bool: true for last msdu of mpdu
 */
static bool dp_rx_chain_msdus_be(struct dp_soc *soc, qdf_nbuf_t nbuf,
				 uint8_t *rx_tlv_hdr, uint8_t mac_id)
{
	bool mpdu_done = false;
	qdf_nbuf_t curr_nbuf = NULL;
	qdf_nbuf_t tmp_nbuf = NULL;

	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);

	if (!dp_pdev) {
		dp_rx_debug("%pK: pdev is null for mac_id = %d", soc, mac_id);
		return mpdu_done;
	}
	/* if invalid peer SG list has max values free the buffers in list
	 * and treat current buffer as start of list
	 *
	 * current logic to detect the last buffer from attn_tlv is not reliable
	 * in OFDMA UL scenario hence add max buffers check to avoid list pile
	 * up
	 */
	if (!dp_pdev->first_nbuf ||
	    (dp_pdev->invalid_peer_head_msdu &&
	    QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST
	    (dp_pdev->invalid_peer_head_msdu) >= DP_MAX_INVALID_BUFFERS)) {
		qdf_nbuf_set_rx_chfrag_start(nbuf, 1);
		dp_pdev->first_nbuf = true;

		/* If the new nbuf received is the first msdu of the
		 * amsdu and there are msdus in the invalid peer msdu
		 * list, then let us free all the msdus of the invalid
		 * peer msdu list.
		 * This scenario can happen when we start receiving
		 * new a-msdu even before the previous a-msdu is completely
		 * received.
		 */
		curr_nbuf = dp_pdev->invalid_peer_head_msdu;
		while (curr_nbuf) {
			tmp_nbuf = curr_nbuf->next;
			dp_rx_nbuf_free(curr_nbuf);
			curr_nbuf = tmp_nbuf;
		}

		dp_pdev->invalid_peer_head_msdu = NULL;
		dp_pdev->invalid_peer_tail_msdu = NULL;

		dp_monitor_get_mpdu_status(dp_pdev, soc, rx_tlv_hdr);
	}

	if (qdf_nbuf_is_rx_chfrag_end(nbuf) &&
	    hal_rx_attn_msdu_done_get(soc->hal_soc, rx_tlv_hdr)) {
		qdf_assert_always(dp_pdev->first_nbuf);
		dp_pdev->first_nbuf = false;
		mpdu_done = true;
	}

	/*
	 * For MCL, invalid_peer_head_msdu and invalid_peer_tail_msdu
	 * should be NULL here, add the checking for debugging purpose
	 * in case some corner case.
	 */
	DP_PDEV_INVALID_PEER_MSDU_CHECK(dp_pdev->invalid_peer_head_msdu,
					dp_pdev->invalid_peer_tail_msdu);
	DP_RX_LIST_APPEND(dp_pdev->invalid_peer_head_msdu,
			  dp_pdev->invalid_peer_tail_msdu,
			  nbuf);

	return mpdu_done;
}
#else
static bool dp_rx_chain_msdus_be(struct dp_soc *soc, qdf_nbuf_t nbuf,
				 uint8_t *rx_tlv_hdr, uint8_t mac_id)
{
	return false;
}
#endif

qdf_nbuf_t
dp_rx_wbm_err_reap_desc_be(struct dp_intr *int_ctx, struct dp_soc *soc,
			   hal_ring_handle_t hal_ring_hdl, uint32_t quota,
			   uint32_t *rx_bufs_used)
{
	hal_ring_desc_t ring_desc;
	hal_soc_handle_t hal_soc;
	struct dp_rx_desc *rx_desc;
	union dp_rx_desc_list_elem_t
		*head[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT] = { { NULL } };
	union dp_rx_desc_list_elem_t
		*tail[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT] = { { NULL } };
	uint32_t rx_bufs_reaped[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT] = { { 0 } };
	uint8_t mac_id;
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;
	qdf_nbuf_t nbuf_head = NULL;
	qdf_nbuf_t nbuf_tail = NULL;
	qdf_nbuf_t nbuf;
	uint8_t msdu_continuation = 0;
	bool process_sg_buf = false;
	QDF_STATUS status;
	struct dp_soc *replenish_soc;
	uint8_t chip_id;
	union hal_wbm_err_info_u wbm_err = { 0 };

	qdf_assert(soc && hal_ring_hdl);
	hal_soc = soc->hal_soc;
	qdf_assert(hal_soc);

	if (qdf_unlikely(dp_srng_access_start(int_ctx, soc, hal_ring_hdl))) {
		/* TODO */
		/*
		 * Need API to convert from hal_ring pointer to
		 * Ring Type / Ring Id combo
		 */
		dp_rx_err_err("%pK: HAL RING Access Failed -- %pK",
			      soc, hal_ring_hdl);
		goto done;
	}

	while (qdf_likely(quota)) {
		ring_desc = hal_srng_dst_get_next(hal_soc, hal_ring_hdl);

		if (qdf_unlikely(!ring_desc))
			break;

		/* Get SW Desc from HAL desc */
		if (dp_wbm_get_rx_desc_from_hal_desc_be(soc,
							ring_desc,
							&rx_desc)) {
			dp_rx_err_err("get rx sw desc from hal_desc failed");
			continue;
		}

		if (dp_assert_always_internal_stat(rx_desc, soc,
						   rx.err.rx_desc_null))
			continue;

		if (!dp_rx_desc_check_magic(rx_desc)) {
			dp_rx_err_err("%pK: Invalid rx_desc %pK",
				      soc, rx_desc);
			continue;
		}

		/*
		 * this is a unlikely scenario where the host is reaping
		 * a descriptor which it already reaped just a while ago
		 * but is yet to replenish it back to HW.
		 * In this case host will dump the last 128 descriptors
		 * including the software descriptor rx_desc and assert.
		 */
		if (qdf_unlikely(!rx_desc->in_use)) {
			DP_STATS_INC(soc, rx.err.hal_wbm_rel_dup, 1);
			dp_rx_dump_info_and_assert(soc, hal_ring_hdl,
						   ring_desc, rx_desc);
			continue;
		}

		status = dp_rx_wbm_desc_nbuf_sanity_check(soc, hal_ring_hdl,
							  ring_desc, rx_desc);
		if (qdf_unlikely(QDF_IS_STATUS_ERROR(status))) {
			DP_STATS_INC(soc, rx.err.nbuf_sanity_fail, 1);
			dp_info_rl("Rx error Nbuf %pK sanity check failure!",
				   rx_desc->nbuf);
			rx_desc->in_err_state = 1;
			continue;
		}

		nbuf = rx_desc->nbuf;

		/*
		 * Read wbm err info , MSDU info , MPDU info , peer meta data,
		 * from desc. Save all the info in nbuf CB/TLV.
		 * We will need this info when we do the actual nbuf processing
		 */
		wbm_err.info = dp_rx_wbm_err_copy_desc_info_in_nbuf(
							soc,
							ring_desc,
							nbuf,
							rx_desc->pool_id);
		/*
		 * For WBM ring, expect only MSDU buffers
		 */
		if (dp_assert_always_internal_stat(
				wbm_err.info_bit.buffer_or_desc_type ==
						HAL_RX_WBM_BUF_TYPE_REL_BUF,
				soc, rx.err.wbm_err_buf_rel_type))
			continue;
		/*
		 * Errors are handled only if the source is RXDMA or REO
		 */
		qdf_assert((wbm_err.info_bit.wbm_err_src ==
			    HAL_RX_WBM_ERR_SRC_RXDMA) ||
			   (wbm_err.info_bit.wbm_err_src ==
			    HAL_RX_WBM_ERR_SRC_REO));

		rx_desc_pool = &soc->rx_desc_buf[rx_desc->pool_id];
		dp_ipa_rx_buf_smmu_mapping_lock(soc);
		dp_rx_nbuf_unmap_pool(soc, rx_desc_pool, nbuf);
		rx_desc->unmapped = 1;
		dp_ipa_rx_buf_smmu_mapping_unlock(soc);

		if (qdf_unlikely(
			soc->wbm_release_desc_rx_sg_support &&
			dp_rx_is_sg_formation_required(&wbm_err.info_bit))) {
			/* SG is detected from continuation bit */
			msdu_continuation =
				dp_rx_wbm_err_msdu_continuation_get(soc,
								    ring_desc,
								    nbuf);
			if (msdu_continuation &&
			    !(soc->wbm_sg_param.wbm_is_first_msdu_in_sg)) {
				/* Update length from first buffer in SG */
				soc->wbm_sg_param.wbm_sg_desc_msdu_len =
					hal_rx_msdu_start_msdu_len_get(
						soc->hal_soc,
						qdf_nbuf_data(nbuf));
				soc->wbm_sg_param.wbm_is_first_msdu_in_sg =
									true;
			}

			if (msdu_continuation) {
				/* MSDU continued packets */
				qdf_nbuf_set_rx_chfrag_cont(nbuf, 1);
				QDF_NBUF_CB_RX_PKT_LEN(nbuf) =
					soc->wbm_sg_param.wbm_sg_desc_msdu_len;
			} else {
				/* This is the terminal packet in SG */
				qdf_nbuf_set_rx_chfrag_start(nbuf, 1);
				qdf_nbuf_set_rx_chfrag_end(nbuf, 1);
				QDF_NBUF_CB_RX_PKT_LEN(nbuf) =
					soc->wbm_sg_param.wbm_sg_desc_msdu_len;
				process_sg_buf = true;
			}
		} else {
			qdf_nbuf_set_rx_chfrag_cont(nbuf, 0);
		}

		rx_bufs_reaped[rx_desc->chip_id][rx_desc->pool_id]++;

		if (qdf_nbuf_is_rx_chfrag_cont(nbuf) || process_sg_buf) {
			DP_RX_LIST_APPEND(soc->wbm_sg_param.wbm_sg_nbuf_head,
					  soc->wbm_sg_param.wbm_sg_nbuf_tail,
					  nbuf);
			if (process_sg_buf) {
				if (!dp_rx_buffer_pool_refill(
					soc,
					soc->wbm_sg_param.wbm_sg_nbuf_head,
					rx_desc->pool_id))
					DP_RX_MERGE_TWO_LIST(
					  nbuf_head, nbuf_tail,
					  soc->wbm_sg_param.wbm_sg_nbuf_head,
					  soc->wbm_sg_param.wbm_sg_nbuf_tail);
				dp_rx_wbm_sg_list_last_msdu_war(soc);
				dp_rx_wbm_sg_list_reset(soc);
				process_sg_buf = false;
			}
		} else if (!dp_rx_buffer_pool_refill(soc, nbuf,
						     rx_desc->pool_id)) {
			DP_RX_LIST_APPEND(nbuf_head, nbuf_tail, nbuf);
		}

		dp_rx_add_to_free_desc_list
			(&head[rx_desc->chip_id][rx_desc->pool_id],
			 &tail[rx_desc->chip_id][rx_desc->pool_id], rx_desc);

		/*
		 * if continuation bit is set then we have MSDU spread
		 * across multiple buffers, let us not decrement quota
		 * till we reap all buffers of that MSDU.
		 */
		if (qdf_likely(!msdu_continuation))
			quota -= 1;
	}
done:
	dp_srng_access_end(int_ctx, soc, hal_ring_hdl);

	for (chip_id = 0; chip_id < WLAN_MAX_MLO_CHIPS; chip_id++) {
		for (mac_id = 0; mac_id < MAX_PDEV_CNT; mac_id++) {
			/*
			 * continue with next mac_id if no pkts were reaped
			 * from that pool
			 */
			if (!rx_bufs_reaped[chip_id][mac_id])
				continue;

			replenish_soc = dp_rx_replenish_soc_get(soc, chip_id);

			dp_rxdma_srng =
				&replenish_soc->rx_refill_buf_ring[mac_id];

			rx_desc_pool = &replenish_soc->rx_desc_buf[mac_id];

			dp_rx_buffers_replenish_simple(replenish_soc, mac_id,
						dp_rxdma_srng,
						rx_desc_pool,
						rx_bufs_reaped[chip_id][mac_id],
						&head[chip_id][mac_id],
						&tail[chip_id][mac_id]);
			*rx_bufs_used += rx_bufs_reaped[chip_id][mac_id];
		}
	}
	return nbuf_head;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * check_extap_multicast_loopback() - Check if rx packet is a loopback packet.
 *
 * @vdev: vdev on which rx packet is received
 * @addr: src address of the received packet
 *
 */
static bool check_extap_multicast_loopback(struct dp_vdev *vdev, uint8_t *addr)
{
	 /* if src mac addr matches with vdev mac address then drop the pkt */
	if (!(qdf_mem_cmp(addr, vdev->mac_addr.raw, QDF_MAC_ADDR_SIZE)))
		return true;

	 /* if src mac addr matches with mld mac address then drop the pkt */
	if (!(qdf_mem_cmp(addr, vdev->mld_mac_addr.raw, QDF_MAC_ADDR_SIZE)))
		return true;

	return false;
}
#else
static bool check_extap_multicast_loopback(struct dp_vdev *vdev, uint8_t *addr)
{
	return false;
}
#endif

QDF_STATUS
dp_rx_null_q_desc_handle_be(struct dp_soc *soc, qdf_nbuf_t nbuf,
			    uint8_t *rx_tlv_hdr, uint8_t pool_id,
			    struct dp_txrx_peer *txrx_peer,
			    bool is_reo_exception,
			    uint8_t link_id)
{
	uint32_t pkt_len;
	uint16_t msdu_len;
	struct dp_vdev *vdev;
	uint8_t tid;
	qdf_ether_header_t *eh;
	struct hal_rx_msdu_metadata msdu_metadata;
	uint16_t sa_idx = 0;
	bool is_eapol = 0;
	bool enh_flag;
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	qdf_nbuf_set_rx_chfrag_start(
				nbuf,
				hal_rx_msdu_end_first_msdu_get(soc->hal_soc,
							       rx_tlv_hdr));
	qdf_nbuf_set_rx_chfrag_end(nbuf,
				   hal_rx_msdu_end_last_msdu_get(soc->hal_soc,
								 rx_tlv_hdr));
	qdf_nbuf_set_da_mcbc(nbuf, hal_rx_msdu_end_da_is_mcbc_get(soc->hal_soc,
								  rx_tlv_hdr));
	qdf_nbuf_set_da_valid(nbuf,
			      hal_rx_msdu_end_da_is_valid_get(soc->hal_soc,
							      rx_tlv_hdr));
	qdf_nbuf_set_sa_valid(nbuf,
			      hal_rx_msdu_end_sa_is_valid_get(soc->hal_soc,
							      rx_tlv_hdr));

	tid = hal_rx_tid_get(soc->hal_soc, rx_tlv_hdr);
	hal_rx_msdu_metadata_get(soc->hal_soc, rx_tlv_hdr, &msdu_metadata);
	msdu_len = hal_rx_msdu_start_msdu_len_get(soc->hal_soc, rx_tlv_hdr);
	pkt_len = msdu_len + msdu_metadata.l3_hdr_pad + soc->rx_pkt_tlv_size;

	if (qdf_likely(!qdf_nbuf_is_frag(nbuf))) {
		if (dp_rx_check_pkt_len(soc, pkt_len))
			goto drop_nbuf;

		/* Set length in nbuf */
		qdf_nbuf_set_pktlen(nbuf, qdf_min(pkt_len, (uint32_t)buf_size));
	}

	/*
	 * Check if DMA completed -- msdu_done is the last bit
	 * to be written
	 */
	if (!hal_rx_attn_msdu_done_get(soc->hal_soc, rx_tlv_hdr)) {
		dp_err_rl("MSDU DONE failure");
		hal_rx_dump_pkt_tlvs(soc->hal_soc, rx_tlv_hdr,
				     QDF_TRACE_LEVEL_INFO);
		qdf_assert(0);
	}

	if (!txrx_peer &&
	    dp_rx_null_q_handle_invalid_peer_id_exception(soc, pool_id,
							  rx_tlv_hdr, nbuf))
		return QDF_STATUS_E_FAILURE;

	if (!txrx_peer) {
		bool mpdu_done = false;
		struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, pool_id);

		if (!pdev) {
			dp_err_rl("pdev is null for pool_id = %d", pool_id);
			return QDF_STATUS_E_FAILURE;
		}

		dp_err_rl("txrx_peer is NULL");
		DP_STATS_INC_PKT(soc, rx.err.rx_invalid_peer, 1,
				 qdf_nbuf_len(nbuf));

		/* QCN9000 has the support enabled */
		if (qdf_unlikely(soc->wbm_release_desc_rx_sg_support)) {
			mpdu_done = true;
			nbuf->next = NULL;
			/* Trigger invalid peer handler wrapper */
			dp_rx_process_invalid_peer_wrapper(soc,
							   nbuf,
							   mpdu_done,
							   pool_id);
		} else {
			mpdu_done = dp_rx_chain_msdus_be(soc, nbuf, rx_tlv_hdr,
							 pool_id);

			/* Trigger invalid peer handler wrapper */
			dp_rx_process_invalid_peer_wrapper(
					soc,
					pdev->invalid_peer_head_msdu,
					mpdu_done, pool_id);
		}

		if (mpdu_done) {
			pdev->invalid_peer_head_msdu = NULL;
			pdev->invalid_peer_tail_msdu = NULL;
		}

		return QDF_STATUS_E_FAILURE;
	}

	vdev = txrx_peer->vdev;
	if (!vdev) {
		dp_err_rl("Null vdev!");
		DP_STATS_INC(soc, rx.err.invalid_vdev, 1);
		goto drop_nbuf;
	}

	/*
	 * Advance the packet start pointer by total size of
	 * pre-header TLV's
	 */
	if (qdf_nbuf_is_frag(nbuf))
		qdf_nbuf_pull_head(nbuf, soc->rx_pkt_tlv_size);
	else
		qdf_nbuf_pull_head(nbuf, (msdu_metadata.l3_hdr_pad +
				   soc->rx_pkt_tlv_size));

	DP_STATS_INC_PKT(vdev, rx_i.null_q_desc_pkt, 1, qdf_nbuf_len(nbuf));

	dp_vdev_peer_stats_update_protocol_cnt(vdev, nbuf, NULL, 0, 1);

	if (dp_rx_err_drop_3addr_mcast(vdev, rx_tlv_hdr)) {
		DP_PEER_PER_PKT_STATS_INC(txrx_peer, rx.mcast_3addr_drop, 1,
					  link_id);
		goto drop_nbuf;
	}

	if (hal_rx_msdu_end_sa_is_valid_get(soc->hal_soc, rx_tlv_hdr)) {
		sa_idx = hal_rx_msdu_end_sa_idx_get(soc->hal_soc, rx_tlv_hdr);

		if ((sa_idx < 0) ||
		    (sa_idx >= wlan_cfg_get_max_ast_idx(soc->wlan_cfg_ctx))) {
			DP_STATS_INC(soc, rx.err.invalid_sa_da_idx, 1);
			goto drop_nbuf;
		}
	}

	if ((!soc->mec_fw_offload) &&
	    dp_rx_mcast_echo_check(soc, txrx_peer, rx_tlv_hdr, nbuf)) {
		/* this is a looped back MCBC pkt, drop it */
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.mec_drop, 1,
					      qdf_nbuf_len(nbuf), link_id);
		goto drop_nbuf;
	}

	/*
	 * In qwrap mode if the received packet matches with any of the vdev
	 * mac addresses, drop it. Donot receive multicast packets originated
	 * from any proxysta.
	 */
	if (check_qwrap_multicast_loopback(vdev, nbuf)) {
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.mec_drop, 1,
					      qdf_nbuf_len(nbuf), link_id);
		goto drop_nbuf;
	}

	if (qdf_unlikely(txrx_peer->nawds_enabled &&
			 hal_rx_msdu_end_da_is_mcbc_get(soc->hal_soc,
							rx_tlv_hdr))) {
		dp_err_rl("free buffer for multicast packet");
		DP_PEER_PER_PKT_STATS_INC(txrx_peer, rx.nawds_mcast_drop, 1,
					  link_id);
		goto drop_nbuf;
	}

	if (!dp_wds_rx_policy_check(rx_tlv_hdr, vdev, txrx_peer)) {
		dp_err_rl("mcast Policy Check Drop pkt");
		DP_PEER_PER_PKT_STATS_INC(txrx_peer, rx.policy_check_drop, 1,
					  link_id);
		goto drop_nbuf;
	}
	/* WDS Source Port Learning */
	if (!soc->ast_offload_support &&
	    qdf_likely(vdev->rx_decap_type == htt_cmn_pkt_type_ethernet &&
		       vdev->wds_enabled))
		dp_rx_wds_srcport_learn(soc, rx_tlv_hdr, txrx_peer, nbuf,
					msdu_metadata);

	if (hal_rx_is_unicast(soc->hal_soc, rx_tlv_hdr)) {
		struct dp_peer *peer;
		struct dp_rx_tid *rx_tid;

		peer = dp_peer_get_ref_by_id(soc, txrx_peer->peer_id,
					     DP_MOD_ID_RX_ERR);
		if (peer) {
			rx_tid = &peer->rx_tid[tid];
			qdf_spin_lock_bh(&rx_tid->tid_lock);
			if (!peer->rx_tid[tid].hw_qdesc_vaddr_unaligned) {
			/* For Mesh peer, if on one of the mesh AP the
			 * mesh peer is not deleted, the new addition of mesh
			 * peer on other mesh AP doesn't do BA negotiation
			 * leading to mismatch in BA windows.
			 * To avoid this send max BA window during init.
			 */
				if (qdf_unlikely(vdev->mesh_vdev) ||
				    qdf_unlikely(txrx_peer->nawds_enabled))
					dp_rx_tid_setup_wifi3(
						peer, BIT(tid),
						hal_get_rx_max_ba_window(soc->hal_soc,tid),
						IEEE80211_SEQ_MAX);
				else
					dp_rx_tid_setup_wifi3(peer, BIT(tid), 1,
							      IEEE80211_SEQ_MAX);
			}
			qdf_spin_unlock_bh(&rx_tid->tid_lock);
			/* IEEE80211_SEQ_MAX indicates invalid start_seq */
			dp_peer_unref_delete(peer, DP_MOD_ID_RX_ERR);
		}
	}

	eh = (qdf_ether_header_t *)qdf_nbuf_data(nbuf);

	if (!txrx_peer->authorize) {
		is_eapol = qdf_nbuf_is_ipv4_eapol_pkt(nbuf);

		if (is_eapol || qdf_nbuf_is_ipv4_wapi_pkt(nbuf)) {
			if (!dp_rx_err_match_dhost(eh, vdev))
				goto drop_nbuf;
		} else {
			goto drop_nbuf;
		}
	}

	/*
	 * Drop packets in this path if cce_match is found. Packets will come
	 * in following path depending on whether tidQ is setup.
	 * 1. If tidQ is setup: WIFILI_HAL_RX_WBM_REO_PSH_RSN_ROUTE and
	 * cce_match = 1
	 *    Packets with WIFILI_HAL_RX_WBM_REO_PSH_RSN_ROUTE are already
	 *    dropped.
	 * 2. If tidQ is not setup: WIFILI_HAL_RX_WBM_REO_PSH_RSN_ERROR and
	 * cce_match = 1
	 *    These packets need to be dropped and should not get delivered
	 *    to stack.
	 */
	if (qdf_unlikely(dp_rx_err_cce_drop(soc, vdev, nbuf, rx_tlv_hdr)))
		goto drop_nbuf;

	/*
	 * In extap mode if the received packet matches with mld mac address
	 * drop it. For non IP packets conversion might not be possible
	 * due to that MEC entry will not be updated, resulting loopback.
	 */
	if (qdf_unlikely(check_extap_multicast_loopback(vdev,
							eh->ether_shost))) {
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.mec_drop, 1,
					      qdf_nbuf_len(nbuf), link_id);
		goto drop_nbuf;
	}

	if (qdf_unlikely(vdev->rx_decap_type == htt_cmn_pkt_type_raw)) {
		qdf_nbuf_set_raw_frame(nbuf, 1);
		qdf_nbuf_set_next(nbuf, NULL);
		dp_rx_deliver_raw(vdev, nbuf, txrx_peer, link_id);
	} else {
		enh_flag = vdev->pdev->enhanced_stats_en;
		qdf_nbuf_set_next(nbuf, NULL);
		DP_PEER_TO_STACK_INCC_PKT(txrx_peer, 1, qdf_nbuf_len(nbuf),
					  enh_flag);
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer,
					      rx.rx_success, 1,
					      qdf_nbuf_len(nbuf),
					      link_id);
		/*
		 * Update the protocol tag in SKB based on
		 * CCE metadata
		 */
		dp_rx_update_protocol_tag(soc, vdev, nbuf, rx_tlv_hdr,
					  EXCEPTION_DEST_RING_ID,
					  true, true);

		/* Update the flow tag in SKB based on FSE metadata */
		dp_rx_update_flow_tag(soc, vdev, nbuf,
				      rx_tlv_hdr, true);

		if (qdf_unlikely(hal_rx_msdu_end_da_is_mcbc_get(
				 soc->hal_soc, rx_tlv_hdr) &&
				 (vdev->rx_decap_type ==
				  htt_cmn_pkt_type_ethernet))) {
			DP_PEER_MC_INCC_PKT(txrx_peer, 1, qdf_nbuf_len(nbuf),
					    enh_flag, link_id);

			if (QDF_IS_ADDR_BROADCAST(eh->ether_dhost))
				DP_PEER_BC_INCC_PKT(txrx_peer, 1,
						    qdf_nbuf_len(nbuf),
						    enh_flag,
						    link_id);
		} else {
			DP_PEER_UC_INCC_PKT(txrx_peer, 1,
					    qdf_nbuf_len(nbuf),
					    enh_flag,
					    link_id);
		}

		qdf_nbuf_set_exc_frame(nbuf, 1);

		if (qdf_unlikely(vdev->multipass_en)) {
			if (dp_rx_multipass_process(txrx_peer, nbuf,
						    tid) == false) {
				DP_PEER_PER_PKT_STATS_INC
					(txrx_peer,
					 rx.multipass_rx_pkt_drop,
					 1, link_id);
				goto drop_nbuf;
			}
		}

		dp_rx_deliver_to_osif_stack(soc, vdev, txrx_peer, nbuf, NULL,
					    is_eapol);
	}
	return QDF_STATUS_SUCCESS;

drop_nbuf:
	dp_rx_nbuf_free(nbuf);
	return QDF_STATUS_E_FAILURE;
}
