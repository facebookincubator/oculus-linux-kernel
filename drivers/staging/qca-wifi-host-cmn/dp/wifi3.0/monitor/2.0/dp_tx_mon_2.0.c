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

#include "qdf_types.h"
#include "hal_be_hw_headers.h"
#include "dp_types.h"
#include "hal_be_tx.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "hal_be_api_mon.h"
#include "dp_internal.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "dp_mon.h"
#include <dp_mon_2.0.h>
#include <dp_tx_mon_2.0.h>
#include <dp_be.h>
#include <hal_be_api_mon.h>
#include <dp_mon_filter_2.0.h>
#include "dp_ratetable.h"
#ifdef QCA_SUPPORT_LITE_MONITOR
#include "dp_lite_mon.h"
#endif

#define MAX_TX_MONITOR_STUCK 50

#ifdef TXMON_DEBUG
/*
 * dp_tx_mon_debug_statu() - API to display tx monitor status
 * @tx_mon_be - pointer to dp_pdev_tx_monitor_be
 * @work_done - tx monitor work done
 *
 * Return: void
 */
static inline void
dp_tx_mon_debug_status(struct dp_pdev_tx_monitor_be *tx_mon_be,
		       uint32_t work_done)
{
	if (tx_mon_be->mode && !work_done)
		tx_mon_be->stats.tx_mon_stuck++;
	else if (tx_mon_be->mode && work_done)
		tx_mon_be->stats.tx_mon_stuck = 0;

	if (tx_mon_be->stats.tx_mon_stuck > MAX_TX_MONITOR_STUCK) {
		dp_mon_warn("Tx monitor block got stuck!!!!!");
		tx_mon_be->stats.tx_mon_stuck = 0;
		tx_mon_be->stats.total_tx_mon_stuck++;
	}

	dp_mon_debug_rl("tx_ppdu_info[%u :D %u] STATUS[R %llu: F %llu] PKT_BUF[R %llu: F %llu : P %llu : S %llu]",
			tx_mon_be->tx_ppdu_info_list_depth,
			tx_mon_be->defer_ppdu_info_list_depth,
			tx_mon_be->stats.status_buf_recv,
			tx_mon_be->stats.status_buf_free,
			tx_mon_be->stats.pkt_buf_recv,
			tx_mon_be->stats.pkt_buf_free,
			tx_mon_be->stats.pkt_buf_processed,
			tx_mon_be->stats.pkt_buf_to_stack);
}

#else
/*
 * dp_tx_mon_debug_statu() - API to display tx monitor status
 * @tx_mon_be - pointer to dp_pdev_tx_monitor_be
 * @work_done - tx monitor work done
 *
 * Return: void
 */
static inline void
dp_tx_mon_debug_status(struct dp_pdev_tx_monitor_be *tx_mon_be,
		       uint32_t work_done)
{
	if (tx_mon_be->mode && !work_done)
		tx_mon_be->stats.tx_mon_stuck++;
	else if (tx_mon_be->mode && work_done)
		tx_mon_be->stats.tx_mon_stuck = 0;

	if (tx_mon_be->stats.tx_mon_stuck > MAX_TX_MONITOR_STUCK) {
		dp_mon_warn("Tx monitor block got stuck!!!!!");
		tx_mon_be->stats.tx_mon_stuck = 0;
		tx_mon_be->stats.total_tx_mon_stuck++;
	}
}
#endif

static inline uint32_t
dp_tx_mon_srng_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
			   uint32_t mac_id, uint32_t quota)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	void *tx_mon_dst_ring_desc;
	hal_soc_handle_t hal_soc;
	void *mon_dst_srng;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;
	uint32_t work_done = 0;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	struct dp_pdev_tx_monitor_be *tx_mon_be = NULL;
	struct dp_mon_desc_pool *tx_mon_desc_pool = &mon_soc_be->tx_desc_mon;
	struct dp_tx_mon_desc_list mon_desc_list;
	uint32_t replenish_cnt = 0;

	if (!pdev) {
		dp_mon_err("%pK: pdev is null for mac_id = %d", soc, mac_id);
		return work_done;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_dst_srng = mon_soc_be->tx_mon_dst_ring[mac_id].hal_srng;

	if (!mon_dst_srng || !hal_srng_initialized(mon_dst_srng)) {
		dp_mon_err("%pK: : HAL Monitor Destination Ring Init Failed -- %pK",
			   soc, mon_dst_srng);
		return work_done;
	}

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	if (qdf_unlikely(!mon_pdev_be))
		return work_done;

	tx_mon_be = &mon_pdev_be->tx_monitor_be;
	hal_soc = soc->hal_soc;

	qdf_assert((hal_soc && pdev));

	qdf_spin_lock_bh(&mon_pdev->mon_lock);
	mon_desc_list.desc_list = NULL;
	mon_desc_list.tail = NULL;
	mon_desc_list.tx_mon_reap_cnt = 0;

	if (qdf_unlikely(dp_srng_access_start(int_ctx, soc, mon_dst_srng))) {
		dp_mon_err("%s %d : HAL Mon Dest Ring access Failed -- %pK",
			   __func__, __LINE__, mon_dst_srng);
		qdf_spin_unlock_bh(&mon_pdev->mon_lock);
		return work_done;
	}

	while (qdf_likely((tx_mon_dst_ring_desc =
		(void *)hal_srng_dst_peek(hal_soc, mon_dst_srng))
				&& quota--)) {
		struct hal_mon_desc hal_mon_tx_desc = {0};
		struct dp_mon_desc *mon_desc = NULL;
		qdf_frag_t status_frag = NULL;
		uint32_t end_offset = 0;

		hal_be_get_mon_dest_status(soc->hal_soc,
					   tx_mon_dst_ring_desc,
					   &hal_mon_tx_desc);

		if (hal_mon_tx_desc.empty_descriptor) {
			/* update stats counter */
			dp_mon_debug("P_ID:%d INIT:%d E_DESC:%d R_ID:%d L_CNT:%d  DROP[PPDU:%d MPDU:%d TLV:%d] E_O_PPDU:%d",
				    hal_mon_tx_desc.ppdu_id,
				    hal_mon_tx_desc.initiator,
				    hal_mon_tx_desc.empty_descriptor,
				    hal_mon_tx_desc.ring_id,
				    hal_mon_tx_desc.looping_count,
				    hal_mon_tx_desc.ppdu_drop_count,
				    hal_mon_tx_desc.mpdu_drop_count,
				    hal_mon_tx_desc.tlv_drop_count,
				    hal_mon_tx_desc.end_of_ppdu_dropped);

			tx_mon_be->stats.ppdu_drop_cnt +=
				hal_mon_tx_desc.ppdu_drop_count;
			tx_mon_be->stats.mpdu_drop_cnt +=
				hal_mon_tx_desc.mpdu_drop_count;
			tx_mon_be->stats.tlv_drop_cnt +=
				hal_mon_tx_desc.tlv_drop_count;
			work_done++;
			hal_srng_dst_get_next(hal_soc, mon_dst_srng);
			continue;
		}

		dp_mon_debug("P_ID:%d INIT:%d E_DESC:%d R_ID:%d L_CNT:%d BUF_ADDR: 0x%llx E_OFF: %d E_REA: %d",
			    hal_mon_tx_desc.ppdu_id,
			    hal_mon_tx_desc.initiator,
			    hal_mon_tx_desc.empty_descriptor,
			    hal_mon_tx_desc.ring_id,
			    hal_mon_tx_desc.looping_count,
			    hal_mon_tx_desc.buf_addr,
			    hal_mon_tx_desc.end_offset,
			    hal_mon_tx_desc.end_reason);

		mon_desc = (struct dp_mon_desc *)(uintptr_t)(hal_mon_tx_desc.buf_addr);
		qdf_assert_always(mon_desc);

		if (!mon_desc->unmapped) {
			qdf_mem_unmap_page(soc->osdev, mon_desc->paddr,
					   DP_MON_DATA_BUFFER_SIZE,
					   QDF_DMA_FROM_DEVICE);
			mon_desc->unmapped = 1;
		}

		if (mon_desc->magic != DP_MON_DESC_MAGIC) {
			dp_mon_err("Invalid monitor descriptor");
			qdf_assert_always(0);
		}

		end_offset = hal_mon_tx_desc.end_offset;

		status_frag = (qdf_frag_t)(mon_desc->buf_addr);
		mon_desc->buf_addr = NULL;
		/* increment reap count */
		++mon_desc_list.tx_mon_reap_cnt;

		/* add the mon_desc to free list */
		dp_mon_add_to_free_desc_list(&mon_desc_list.desc_list,
					     &mon_desc_list.tail, mon_desc);


		if (qdf_unlikely(!status_frag)) {
			dp_mon_debug("P_ID:%d INIT:%d E_DESC:%d R_ID:%d L_CNT:%d BUF_ADDR: 0x%llx E_OFF: %d E_REA: %d",
				     hal_mon_tx_desc.ppdu_id,
				     hal_mon_tx_desc.initiator,
				     hal_mon_tx_desc.empty_descriptor,
				     hal_mon_tx_desc.ring_id,
				     hal_mon_tx_desc.looping_count,
				     hal_mon_tx_desc.buf_addr,
				     hal_mon_tx_desc.end_offset,
				     hal_mon_tx_desc.end_reason);

			work_done++;
			hal_srng_dst_get_next(hal_soc, mon_dst_srng);
			continue;
		}

		tx_mon_be->stats.status_buf_recv++;

		if ((hal_mon_tx_desc.end_reason == HAL_MON_FLUSH_DETECTED) ||
		    (hal_mon_tx_desc.end_reason == HAL_MON_PPDU_TRUNCATED)) {
			tx_mon_be->be_ppdu_id = hal_mon_tx_desc.ppdu_id;

			dp_tx_mon_update_end_reason(mon_pdev,
						    hal_mon_tx_desc.ppdu_id,
						    hal_mon_tx_desc.end_reason);
			/* check and free packet buffer from status buffer */
			dp_tx_mon_status_free_packet_buf(pdev, status_frag,
							 end_offset,
							 &mon_desc_list);

			tx_mon_be->stats.status_buf_free++;
			qdf_frag_free(status_frag);

			work_done++;
			hal_srng_dst_get_next(hal_soc, mon_dst_srng);
			continue;
		}

		dp_tx_process_pktlog_be(soc, pdev,
					status_frag,
					end_offset);

		dp_tx_mon_process_status_tlv(soc, pdev,
					     &hal_mon_tx_desc,
					     status_frag,
					     end_offset,
					     &mon_desc_list);

		work_done++;
		hal_srng_dst_get_next(hal_soc, mon_dst_srng);
	}
	dp_srng_access_end(int_ctx, soc, mon_dst_srng);

	if (mon_desc_list.tx_mon_reap_cnt) {
		dp_mon_buffers_replenish(soc, &mon_soc_be->tx_mon_buf_ring,
					 tx_mon_desc_pool,
					 mon_desc_list.tx_mon_reap_cnt,
					 &mon_desc_list.desc_list,
					 &mon_desc_list.tail,
					 &replenish_cnt);
	}
	qdf_spin_unlock_bh(&mon_pdev->mon_lock);
	dp_mon_debug("mac_id: %d, work_done:%d tx_monitor_reap_cnt:%d",
		     mac_id, work_done, mon_desc_list.tx_mon_reap_cnt);

	tx_mon_be->stats.total_tx_mon_reap_cnt += mon_desc_list.tx_mon_reap_cnt;
	tx_mon_be->stats.totat_tx_mon_replenish_cnt += replenish_cnt;
	dp_tx_mon_debug_status(tx_mon_be, work_done);

	return work_done;
}

uint32_t
dp_tx_mon_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
		      uint32_t mac_id, uint32_t quota)
{
	uint32_t work_done;

	work_done = dp_tx_mon_srng_process_2_0(soc, int_ctx, mac_id, quota);

	return work_done;
}

void
dp_tx_mon_print_ring_stat_2_0(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	int lmac_id;

	lmac_id = dp_get_lmac_id_for_pdev_id(soc, 0, pdev->pdev_id);
	dp_print_ring_stat_from_hal(soc, &mon_soc_be->tx_mon_buf_ring,
				    TX_MONITOR_BUF);
	dp_print_ring_stat_from_hal(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id],
				    TX_MONITOR_DST);
}

void
dp_tx_mon_buf_desc_pool_deinit(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	dp_mon_desc_pool_deinit(&mon_soc_be->tx_desc_mon);
}

QDF_STATUS
dp_tx_mon_buf_desc_pool_init(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	uint32_t num_entries;

	num_entries =
		wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc->wlan_cfg_ctx);

	return dp_mon_desc_pool_init(&mon_soc_be->tx_desc_mon, num_entries);
}

void dp_tx_mon_buf_desc_pool_free(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	if (mon_soc_be)
		dp_mon_desc_pool_free(soc, &mon_soc_be->tx_desc_mon,
				      DP_MON_TX_DESC_POOL_TYPE);
}

QDF_STATUS dp_tx_mon_soc_init_2_0(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	if (dp_srng_init(soc, &mon_soc_be->tx_mon_buf_ring,
			 TX_MONITOR_BUF, 0, 0)) {
		dp_mon_err("%pK: " RNG_ERR "tx_mon_buf_ring", soc);
		goto fail;
	}

	if (dp_tx_mon_buf_desc_pool_init(soc)) {
		dp_mon_err("%pK: " RNG_ERR "tx mon desc pool init", soc);
		goto fail;
	}

	return QDF_STATUS_SUCCESS;
fail:
	return QDF_STATUS_E_FAILURE;
}

void dp_tx_mon_soc_deinit_2_0(struct dp_soc *soc, uint32_t lmac_id)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	dp_tx_mon_buffers_free(soc);
	dp_tx_mon_buf_desc_pool_deinit(soc);
	dp_srng_deinit(soc, &mon_soc_be->tx_mon_buf_ring, TX_MONITOR_BUF, 0);
}

QDF_STATUS
dp_tx_mon_buf_desc_pool_alloc(struct dp_soc *soc)
{
	struct dp_mon_desc_pool *tx_mon_desc_pool;
	int entries;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	entries = wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc_cfg_ctx);


	tx_mon_desc_pool = &mon_soc_be->tx_desc_mon;

	qdf_print("%s:%d tx mon buf desc pool entries: %d", __func__, __LINE__, entries);
	return dp_mon_desc_pool_alloc(soc, DP_MON_TX_DESC_POOL_TYPE,
				      entries, tx_mon_desc_pool);
}

void
dp_tx_mon_buffers_free(struct dp_soc *soc)
{
	struct dp_mon_desc_pool *tx_mon_desc_pool;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	tx_mon_desc_pool = &mon_soc_be->tx_desc_mon;

	dp_mon_pool_frag_unmap_and_free(soc, tx_mon_desc_pool);
}

QDF_STATUS
dp_tx_mon_buffers_alloc(struct dp_soc *soc, uint32_t size)
{
	struct dp_srng *mon_buf_ring;
	struct dp_mon_desc_pool *tx_mon_desc_pool;
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	mon_buf_ring = &mon_soc_be->tx_mon_buf_ring;

	tx_mon_desc_pool = &mon_soc_be->tx_desc_mon;

	return dp_mon_buffers_replenish(soc, mon_buf_ring,
					tx_mon_desc_pool,
					size,
					&desc_list, &tail, NULL);
}

#ifdef WLAN_TX_PKT_CAPTURE_ENH_BE

/*
 * dp_tx_mon_nbuf_get_num_frag() - get total number of fragments
 * @buf: Network buf instance
 *
 * Return: number of fragments
 */
static inline
uint32_t dp_tx_mon_nbuf_get_num_frag(qdf_nbuf_t nbuf)
{
	uint32_t num_frag = 0;

	if (qdf_unlikely(!nbuf))
		return num_frag;

	num_frag = qdf_nbuf_get_nr_frags_in_fraglist(nbuf);

	return num_frag;
}

/*
 * dp_tx_mon_free_usr_mpduq() - API to free user mpduq
 * @tx_ppdu_info - pointer to tx_ppdu_info
 * @usr_idx - user index
 * @tx_mon_be - pointer to tx capture be
 *
 * Return: void
 */
void dp_tx_mon_free_usr_mpduq(struct dp_tx_ppdu_info *tx_ppdu_info,
			      uint8_t usr_idx,
			      struct dp_pdev_tx_monitor_be *tx_mon_be)
{
	qdf_nbuf_queue_t *mpdu_q;
	uint32_t num_frag = 0;
	qdf_nbuf_t buf = NULL;

	if (qdf_unlikely(!tx_ppdu_info))
		return;

	mpdu_q = &TXMON_PPDU_USR(tx_ppdu_info, usr_idx, mpdu_q);

	while ((buf = qdf_nbuf_queue_remove(mpdu_q)) != NULL) {
		num_frag += dp_tx_mon_nbuf_get_num_frag(buf);
		qdf_nbuf_free(buf);
	}
	tx_mon_be->stats.pkt_buf_free += num_frag;
}

/*
 * dp_tx_mon_free_ppdu_info() - API to free dp_tx_ppdu_info
 * @tx_ppdu_info - pointer to tx_ppdu_info
 * @tx_mon_be - pointer to tx capture be
 *
 * Return: void
 */
void dp_tx_mon_free_ppdu_info(struct dp_tx_ppdu_info *tx_ppdu_info,
			      struct dp_pdev_tx_monitor_be *tx_mon_be)
{
	uint32_t user = 0;

	for (; user < TXMON_PPDU_HAL(tx_ppdu_info, num_users); user++) {
		qdf_nbuf_queue_t *mpdu_q;
		uint32_t num_frag = 0;
		qdf_nbuf_t buf = NULL;

		mpdu_q = &TXMON_PPDU_USR(tx_ppdu_info, user, mpdu_q);

		while ((buf = qdf_nbuf_queue_remove(mpdu_q)) != NULL) {
			num_frag += dp_tx_mon_nbuf_get_num_frag(buf);
			qdf_nbuf_free(buf);
		}
		tx_mon_be->stats.pkt_buf_free += num_frag;
	}

	TXMON_PPDU_HAL(tx_ppdu_info, is_used) = 0;
	qdf_mem_free(tx_ppdu_info);
}

/*
 * dp_tx_mon_get_ppdu_info() - API to allocate dp_tx_ppdu_info
 * @pdev - pdev handle
 * @type - type of ppdu_info data or protection
 * @num_user - number user in a ppdu_info
 * @ppdu_id - ppdu_id number
 *
 * Return: pointer to dp_tx_ppdu_info
 */
struct dp_tx_ppdu_info *dp_tx_mon_get_ppdu_info(struct dp_pdev *pdev,
						enum tx_ppdu_info_type type,
						uint8_t num_user,
						uint32_t ppdu_id)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_pdev_tx_monitor_be *tx_mon_be =
			&mon_pdev_be->tx_monitor_be;
	struct dp_tx_ppdu_info *tx_ppdu_info;
	size_t sz_ppdu_info = 0;
	uint8_t i;

	/* allocate new tx_ppdu_info */
	sz_ppdu_info = (sizeof(struct dp_tx_ppdu_info) +
			(sizeof(struct mon_rx_user_status) * num_user));

	tx_ppdu_info = (struct dp_tx_ppdu_info *)qdf_mem_malloc(sz_ppdu_info);
	if (!tx_ppdu_info) {
		dp_mon_err("allocation of tx_ppdu_info type[%d] failed!!!",
			   type);
		return NULL;
	}

	TXMON_PPDU_HAL(tx_ppdu_info, is_used) = 0;
	TXMON_PPDU_HAL(tx_ppdu_info, num_users) = num_user;
	TXMON_PPDU_HAL(tx_ppdu_info, ppdu_id) = ppdu_id;
	TXMON_PPDU(tx_ppdu_info, ppdu_id) = ppdu_id;

	for (i = 0; i < num_user; i++) {
		qdf_nbuf_queue_t *mpdu_q;

		mpdu_q = &TXMON_PPDU_USR(tx_ppdu_info, i, mpdu_q);
		qdf_nbuf_queue_init(mpdu_q);
	}

	/* assign tx_ppdu_info to monitor pdev for reference */
	if (type == TX_PROT_PPDU_INFO) {
		qdf_mem_zero(&tx_mon_be->prot_status_info, sizeof(struct hal_tx_status_info));
		tx_mon_be->tx_prot_ppdu_info = tx_ppdu_info;
		TXMON_PPDU_HAL(tx_ppdu_info, is_data) = 0;
	} else {
		qdf_mem_zero(&tx_mon_be->data_status_info, sizeof(struct hal_tx_status_info));
		tx_mon_be->tx_data_ppdu_info = tx_ppdu_info;
		TXMON_PPDU_HAL(tx_ppdu_info, is_data) = 1;
	}

	return tx_ppdu_info;
}

/*
 * dp_print_pdev_tx_monitor_stats_2_0: print tx capture stats
 * @pdev: DP PDEV handle
 *
 * return: void
 */
void dp_print_pdev_tx_monitor_stats_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_pdev_tx_monitor_be *tx_mon_be =
			&mon_pdev_be->tx_monitor_be;
	struct dp_tx_monitor_drop_stats stats = {0};

	qdf_mem_copy(&stats, &tx_mon_be->stats,
		     sizeof(struct dp_tx_monitor_drop_stats));

	/* TX monitor stats needed for beryllium */
	DP_PRINT_STATS("\n\tTX Capture BE stats mode[%d]:", tx_mon_be->mode);
	DP_PRINT_STATS("\tbuffer pending : %u", tx_mon_be->last_frag_q_idx);
	DP_PRINT_STATS("\treplenish count: %llu",
		       stats.totat_tx_mon_replenish_cnt);
	DP_PRINT_STATS("\treap count     : %llu", stats.total_tx_mon_reap_cnt);
	DP_PRINT_STATS("\tmonitor stuck  : %u", stats.total_tx_mon_stuck);
	DP_PRINT_STATS("\tStatus buffer");
	DP_PRINT_STATS("\t\treceived  : %llu", stats.status_buf_recv);
	DP_PRINT_STATS("\t\tfree      : %llu", stats.status_buf_free);
	DP_PRINT_STATS("\tPacket buffer");
	DP_PRINT_STATS("\t\treceived  : %llu", stats.pkt_buf_recv);
	DP_PRINT_STATS("\t\tfree      : %llu", stats.pkt_buf_free);
	DP_PRINT_STATS("\t\tprocessed : %llu", stats.pkt_buf_processed);
	DP_PRINT_STATS("\t\tto stack  : %llu", stats.pkt_buf_to_stack);
	DP_PRINT_STATS("\tppdu info");
	DP_PRINT_STATS("\t\tthreshold : %llu", stats.ppdu_info_drop_th);
	DP_PRINT_STATS("\t\tflush     : %llu", stats.ppdu_info_drop_flush);
	DP_PRINT_STATS("\t\ttruncated : %llu", stats.ppdu_info_drop_trunc);
	DP_PRINT_STATS("\tDrop stats");
	DP_PRINT_STATS("\t\tppdu drop : %llu", stats.ppdu_drop_cnt);
	DP_PRINT_STATS("\t\tmpdu drop : %llu", stats.mpdu_drop_cnt);
	DP_PRINT_STATS("\t\ttlv drop : %llu", stats.tlv_drop_cnt);
}

#ifdef QCA_SUPPORT_LITE_MONITOR
static void dp_lite_mon_free_tx_peers(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_lite_mon_tx_config *lite_mon_tx_config;

	lite_mon_tx_config = mon_pdev_be->lite_mon_tx_config;
	qdf_spin_lock_bh(&lite_mon_tx_config->lite_mon_tx_lock);
	dp_lite_mon_free_peers(pdev, &lite_mon_tx_config->tx_config);
	qdf_spin_unlock_bh(&lite_mon_tx_config->lite_mon_tx_lock);
}
#else
static void dp_lite_mon_free_tx_peers(struct dp_pdev *pdev)
{
}
#endif

/*
 * dp_config_enh_tx_monitor_2_0()- API to enable/disable enhanced tx capture
 * @pdev_handle: DP_PDEV handle
 * @val: user provided value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_config_enh_tx_monitor_2_0(struct dp_pdev *pdev, uint8_t val)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_pdev_tx_monitor_be *tx_mon_be =
			&mon_pdev_be->tx_monitor_be;
	struct dp_soc *soc = pdev->soc;
	uint16_t num_of_buffers;
	QDF_STATUS status;

	soc_cfg_ctx = soc->wlan_cfg_ctx;
	switch (val) {
	case TX_MON_BE_DISABLE:
	{
		tx_mon_be->mode = TX_MON_BE_DISABLE;
		mon_pdev_be->tx_mon_mode = 0;
		mon_pdev_be->tx_mon_filter_length = DMA_LENGTH_64B;
		/* Free any peers that were added for tx peer filtering */
		dp_lite_mon_free_tx_peers(pdev);
		break;
	}
	case TX_MON_BE_FULL_CAPTURE:
	{
		num_of_buffers = wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc_cfg_ctx);
		status = dp_vdev_set_monitor_mode_buf_rings_tx_2_0(pdev,
								   num_of_buffers);
		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("Tx monitor buffer allocation failed");
			return status;
		}
		qdf_mem_zero(&tx_mon_be->stats,
			     sizeof(struct dp_tx_monitor_drop_stats));
		tx_mon_be->last_tsft = 0;
		tx_mon_be->last_ppdu_timestamp = 0;
		tx_mon_be->mode = TX_MON_BE_FULL_CAPTURE;
		mon_pdev_be->tx_mon_mode = 1;
		mon_pdev_be->tx_mon_filter_length = DEFAULT_DMA_LENGTH;
		break;
	}
	case TX_MON_BE_PEER_FILTER:
	{
		status = dp_vdev_set_monitor_mode_buf_rings_tx_2_0(pdev,
								   DP_MON_RING_FILL_LEVEL_DEFAULT);
		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("Tx monitor buffer allocation failed");
			return status;
		}
		tx_mon_be->mode = TX_MON_BE_PEER_FILTER;
		mon_pdev_be->tx_mon_mode = 2;
		mon_pdev_be->tx_mon_filter_length = DMA_LENGTH_256B;
		break;
	}
	default:
	{
		return QDF_STATUS_E_INVAL;
	}
	}

	dp_mon_info("Tx monitor mode:%d mon_mode_flag:%d config_length:%d",
		    tx_mon_be->mode, mon_pdev_be->tx_mon_mode,
		    mon_pdev_be->tx_mon_filter_length);

	dp_mon_filter_setup_tx_mon_mode(pdev);
	dp_tx_mon_filter_update(pdev);

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_peer_set_tx_capture_enabled_2_0() -  add tx monitor peer filter
 * @pdev: Datapath PDEV handle
 * @peer: Datapath PEER handle
 * @is_tx_pkt_cap_enable: flag for tx capture enable/disable
 * @peer_mac: peer mac address
 *
 * Return: status
 */
QDF_STATUS dp_peer_set_tx_capture_enabled_2_0(struct dp_pdev *pdev_handle,
					      struct dp_peer *peer_handle,
					      uint8_t is_tx_pkt_cap_enable,
					      uint8_t *peer_mac)
{
	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_LITE_MONITOR
static void dp_fill_lite_mon_vdev(struct cdp_tx_indication_info *tx_cap_info,
				  struct dp_mon_pdev_be *mon_pdev_be)
{
	struct dp_lite_mon_config *config;
	struct dp_vdev *lite_mon_vdev;

	config = &mon_pdev_be->lite_mon_tx_config->tx_config;
	lite_mon_vdev = config->lite_mon_vdev;

	if (lite_mon_vdev)
		tx_cap_info->osif_vdev = lite_mon_vdev->osif_vdev;
}

/**
 * dp_lite_mon_filter_ppdu() - Filter frames at ppdu level
 * @mpdu_count: mpdu count in the nbuf queue
 * @level: Lite monitor filter level
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_lite_mon_filter_ppdu(uint8_t mpdu_count, uint8_t level)
{
	if (level == CDP_LITE_MON_LEVEL_PPDU && mpdu_count > 1)
		return QDF_STATUS_E_CANCELED;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_lite_mon_filter_peer() - filter frames with peer
 * @config: Lite monitor configuration
 * @wh: Pointer to ieee80211_frame
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_lite_mon_filter_peer(struct dp_lite_mon_tx_config *config,
			struct ieee80211_frame_min_one *wh)
{
	struct dp_lite_mon_peer *peer;

	/* Return here if sw peer filtering is not required or if peer count
	 * is zero
	 */
	if (!config->sw_peer_filtering || !config->tx_config.peer_count)
		return QDF_STATUS_SUCCESS;

	TAILQ_FOREACH(peer, &config->tx_config.peer_list, peer_list_elem) {
		if (!qdf_mem_cmp(&peer->peer_mac.raw[0],
				 &wh->i_addr1[0], QDF_MAC_ADDR_SIZE)) {
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_ABORTED;
}

/**
 * dp_lite_mon_filter_subtype() - filter frames with subtype
 * @config: Lite monitor configuration
 * @wh: Pointer to ieee80211_frame
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_lite_mon_filter_subtype(struct dp_lite_mon_tx_config *config,
			   struct ieee80211_frame_min_one *wh)
{
	uint16_t mgmt_filter, ctrl_filter, data_filter, type, subtype;
	uint8_t is_mcast = 0;

	/* Return here if subtype filtering is not required */
	if (!config->subtype_filtering)
		return QDF_STATUS_SUCCESS;

	mgmt_filter = config->tx_config.mgmt_filter[DP_MON_FRM_FILTER_MODE_FP];
	ctrl_filter = config->tx_config.ctrl_filter[DP_MON_FRM_FILTER_MODE_FP];
	data_filter = config->tx_config.data_filter[DP_MON_FRM_FILTER_MODE_FP];

	type = (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
	subtype = ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) >>
		IEEE80211_FC0_SUBTYPE_SHIFT);

	switch (type) {
	case QDF_IEEE80211_FC0_TYPE_MGT:
		if (mgmt_filter >> subtype & 0x1)
			return QDF_STATUS_SUCCESS;
		else
			return QDF_STATUS_E_ABORTED;
	case QDF_IEEE80211_FC0_TYPE_CTL:
		if (ctrl_filter >> subtype & 0x1)
			return QDF_STATUS_SUCCESS;
		else
			return QDF_STATUS_E_ABORTED;
	case QDF_IEEE80211_FC0_TYPE_DATA:
		is_mcast = DP_FRAME_IS_MULTICAST(wh->i_addr1);
		if ((is_mcast && (data_filter & FILTER_DATA_MCAST)) ||
		    (!is_mcast && (data_filter & FILTER_DATA_UCAST)))
			return QDF_STATUS_SUCCESS;
		return QDF_STATUS_E_ABORTED;
	default:
		return QDF_STATUS_E_INVAL;
	}
}

/**
 * dp_lite_mon_filter_peer_subtype() - filter frames with subtype and peer
 * @config: Lite monitor configuration
 * @buf: Pointer to nbuf
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_lite_mon_filter_peer_subtype(struct dp_lite_mon_tx_config *config,
				qdf_nbuf_t buf)
{
	struct ieee80211_frame_min_one *wh;
	qdf_nbuf_t nbuf;
	QDF_STATUS ret;

	/* Return here if subtype and peer filtering is not required */
	if (!config->subtype_filtering && !config->sw_peer_filtering &&
	    !config->tx_config.peer_count)
		return QDF_STATUS_SUCCESS;

	if (dp_tx_mon_nbuf_get_num_frag(buf)) {
		wh = (struct ieee80211_frame_min_one *)qdf_nbuf_get_frag_addr(buf, 0);
	} else {
		nbuf = qdf_nbuf_get_ext_list(buf);
		if (nbuf)
			wh = (struct ieee80211_frame_min_one *)qdf_nbuf_data(nbuf);
		else
			return QDF_STATUS_E_INVAL;
	}

	ret = dp_lite_mon_filter_subtype(config, wh);
	if (ret)
		return ret;

	ret = dp_lite_mon_filter_peer(config, wh);
	if (ret)
		return ret;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_tx_lite_mon_filtering() - Additional filtering for lite monitor
 * @pdev: Pointer to physical device
 * @tx_ppdu_info: pointer to dp_tx_ppdu_info structure
 * @buf: qdf nbuf structure of buffer
 * @mpdu_count: mpdu count in the nbuf queue
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_tx_lite_mon_filtering(struct dp_pdev *pdev,
			 struct dp_tx_ppdu_info *tx_ppdu_info,
			 qdf_nbuf_t buf, int mpdu_count)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
		dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_lite_mon_tx_config *config =
		mon_pdev_be->lite_mon_tx_config;
	QDF_STATUS ret;

	if (!dp_lite_mon_is_tx_enabled(mon_pdev) &&
	    !config->tx_config.peer_count)
		return QDF_STATUS_SUCCESS;

	/* PPDU level filtering */
	ret = dp_lite_mon_filter_ppdu(mpdu_count, config->tx_config.level);
	if (ret)
		return ret;

	/* Subtype and peer filtering */
	ret = dp_lite_mon_filter_peer_subtype(config, buf);
	if (ret)
		return ret;

	return QDF_STATUS_SUCCESS;
}

#else
static void dp_fill_lite_mon_vdev(struct cdp_tx_indication_info *tx_cap_info,
				  struct dp_mon_pdev_be *mon_pdev_be)
{
}

/**
 * dp_tx_lite_mon_filtering() - Additional filtering for lite monitor
 * @pdev: Pointer to physical device
 * @tx_ppdu_info: pointer to dp_tx_ppdu_info structure
 * @buf: qdf nbuf structure of buffer
 * @mpdu_count: mpdu count in the nbuf queue
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_tx_lite_mon_filtering(struct dp_pdev *pdev,
			 struct dp_tx_ppdu_info *tx_ppdu_info,
			 qdf_nbuf_t buf, int mpdu_count)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_LOCAL_PKT_CAPTURE
/**
 * dp_tx_mon_lpc_type_filtering() - Additional filtering for lpc
 * @pdev: Pointer to physical device
 * @tx_ppdu_info: pointer to dp_tx_ppdu_info structure
 * @buf: qdf nbuf structure of buffer
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_tx_mon_lpc_type_filtering(struct dp_pdev *pdev,
			     struct dp_tx_ppdu_info *tx_ppdu_info,
			     qdf_nbuf_t buf)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	qdf_nbuf_t nbuf;
	struct ieee80211_frame_min_one *wh;
	uint16_t mgmt_filter, ctrl_filter, data_filter, type;

	if (qdf_unlikely(!IS_LOCAL_PKT_CAPTURE_RUNNING(mon_pdev,
			is_local_pkt_capture_running)))
		return QDF_STATUS_E_ABORTED;

	if (dp_tx_mon_nbuf_get_num_frag(buf)) {
		wh = (struct ieee80211_frame_min_one *)qdf_nbuf_get_frag_addr(buf, 0);
	} else {
		nbuf = qdf_nbuf_get_ext_list(buf);
		if (nbuf)
			wh = (struct ieee80211_frame_min_one *)qdf_nbuf_data(nbuf);
		else
			return QDF_STATUS_E_ABORTED;
	}

	mgmt_filter = mon_pdev->fp_mgmt_filter;
	ctrl_filter = mon_pdev->fp_ctrl_filter;
	data_filter = mon_pdev->fp_data_filter;

	type = (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);

	switch (type) {
	case QDF_IEEE80211_FC0_TYPE_MGT:
		return mgmt_filter ? QDF_STATUS_SUCCESS : QDF_STATUS_E_ABORTED;
	case QDF_IEEE80211_FC0_TYPE_CTL:
		return ctrl_filter ? QDF_STATUS_SUCCESS : QDF_STATUS_E_ABORTED;
	case QDF_IEEE80211_FC0_TYPE_DATA:
		return data_filter ? QDF_STATUS_SUCCESS : QDF_STATUS_E_ABORTED;
	default:
		return QDF_STATUS_E_ABORTED;
	}

	return QDF_STATUS_SUCCESS;
}

static int
dp_tx_handle_local_pkt_capture(struct dp_pdev *pdev, qdf_nbuf_t nbuf)
{
	struct dp_mon_vdev *mon_vdev;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!mon_pdev->mvdev) {
		dp_mon_err("Monitor vdev is NULL !!");
		return 1;
	}

	mon_vdev = mon_pdev->mvdev->monitor_vdev;

	if (mon_vdev && mon_vdev->osif_rx_mon)
		mon_vdev->osif_rx_mon(mon_pdev->mvdev->osif_vdev, nbuf, NULL);

	return 0;
}
#else
static int
dp_tx_handle_local_pkt_capture(struct dp_pdev *pdev, qdf_nbuf_t nbuf)
{
	return 0;
}

static inline QDF_STATUS
dp_tx_mon_lpc_type_filtering(struct dp_pdev *pdev,
			     struct dp_tx_ppdu_info *tx_ppdu_info,
			     qdf_nbuf_t buf)
{
	return QDF_STATUS_SUCCESS;
}

#endif

/**
 * dp_tx_mon_send_to_stack() - API to send to stack
 * @pdev: pdev Handle
 * @mpdu: pointer to mpdu
 * @num_frag: number of frag in mpdu
 * @ppdu_id: ppdu id of the mpdu
 *
 * Return: void
 */
static void
dp_tx_mon_send_to_stack(struct dp_pdev *pdev, qdf_nbuf_t mpdu,
			uint32_t num_frag, uint32_t ppdu_id)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_pdev_tx_monitor_be *tx_mon_be =
			&mon_pdev_be->tx_monitor_be;
	struct cdp_tx_indication_info tx_capture_info = {0};

	tx_mon_be->stats.pkt_buf_to_stack += num_frag;

	tx_capture_info.radiotap_done = 1;
	tx_capture_info.mpdu_nbuf = mpdu;
	tx_capture_info.mpdu_info.ppdu_id = ppdu_id;

	if (qdf_unlikely(IS_LOCAL_PKT_CAPTURE_RUNNING(mon_pdev,
			is_local_pkt_capture_running))) {
		int ret = dp_tx_handle_local_pkt_capture(pdev, mpdu);

		/*
		 * On error, free the memory here,
		 * otherwise it will be freed by the network stack
		 */
		if (ret)
			qdf_nbuf_free(mpdu);
		return;
	} else if (!dp_lite_mon_is_tx_enabled(mon_pdev)) {
		dp_wdi_event_handler(WDI_EVENT_TX_PKT_CAPTURE,
				     pdev->soc,
				     &tx_capture_info,
				     HTT_INVALID_PEER,
				     WDI_NO_VAL,
				     pdev->pdev_id);
	} else {
		dp_fill_lite_mon_vdev(&tx_capture_info, mon_pdev_be);
		dp_wdi_event_handler(WDI_EVENT_LITE_MON_TX,
				     pdev->soc,
				     &tx_capture_info,
				     HTT_INVALID_PEER,
				     WDI_NO_VAL,
				     pdev->pdev_id);
	}
	if (tx_capture_info.mpdu_nbuf)
		qdf_nbuf_free(tx_capture_info.mpdu_nbuf);
}

/**
 * dp_tx_mon_send_per_usr_mpdu() - API to send per usr mpdu to stack
 * @pdev: pdev Handle
 * @ppdu_info: pointer to dp_tx_ppdu_info
 * @user_idx: current user index
 *
 * Return: void
 */
static void
dp_tx_mon_send_per_usr_mpdu(struct dp_pdev *pdev,
			    struct dp_tx_ppdu_info *ppdu_info,
			    uint8_t user_idx)
{
	qdf_nbuf_queue_t *usr_mpdu_q = NULL;
	qdf_nbuf_t buf = NULL;
	uint8_t mpdu_count = 0;

	usr_mpdu_q = &TXMON_PPDU_USR(ppdu_info, user_idx, mpdu_q);

	while ((buf = qdf_nbuf_queue_remove(usr_mpdu_q)) != NULL) {
		uint32_t num_frag = dp_tx_mon_nbuf_get_num_frag(buf);

		ppdu_info->hal_txmon.rx_status.rx_user_status =
				&ppdu_info->hal_txmon.rx_user_status[user_idx];

		if (dp_tx_lite_mon_filtering(pdev, ppdu_info, buf,
					     ++mpdu_count) ||
		    dp_tx_mon_lpc_type_filtering(pdev, ppdu_info, buf)) {
			qdf_nbuf_free(buf);
			continue;
		}

		qdf_nbuf_update_radiotap(&ppdu_info->hal_txmon.rx_status,
					 buf, qdf_nbuf_headroom(buf));

		dp_tx_mon_send_to_stack(pdev, buf, num_frag,
					TXMON_PPDU(ppdu_info, ppdu_id));
	}
}

#define PHY_MEDIUM_MHZ	960
#define PHY_TIMESTAMP_WRAP (0xFFFFFFFF / PHY_MEDIUM_MHZ)

/**
 * dp_populate_tsft_from_phy_timestamp() - API to get tsft from phy timestamp
 * @pdev: pdev Handle
 * @ppdu_info: ppdi_info Handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_populate_tsft_from_phy_timestamp(struct dp_pdev *pdev,
				    struct dp_tx_ppdu_info *ppdu_info)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_pdev_tx_monitor_be *tx_mon_be =
			&mon_pdev_be->tx_monitor_be;
	uint64_t tsft = 0;
	uint32_t ppdu_timestamp = 0;

	tsft = TXMON_PPDU_COM(ppdu_info, tsft);
	ppdu_timestamp = TXMON_PPDU_COM(ppdu_info, ppdu_timestamp);

	if (tsft && ppdu_timestamp) {
		/* update tsft and ppdu timestamp */
		tx_mon_be->last_tsft = tsft;
		tx_mon_be->last_ppdu_timestamp = ppdu_timestamp;
	} else if (!tx_mon_be->last_ppdu_timestamp || !tx_mon_be->last_tsft) {
		return QDF_STATUS_E_EMPTY;
	}

	if (!tsft && ppdu_timestamp) {
		/* response window */
		uint32_t cur_usec = ppdu_timestamp / PHY_MEDIUM_MHZ;
		uint32_t last_usec = (tx_mon_be->last_ppdu_timestamp /
				      PHY_MEDIUM_MHZ);
		uint32_t diff = 0;

		if (last_usec < cur_usec) {
			diff = cur_usec - last_usec;
			tsft = tx_mon_be->last_tsft + diff;
		} else {
			diff = (PHY_TIMESTAMP_WRAP - last_usec) + cur_usec;
			tsft = tx_mon_be->last_tsft + diff;
		}
		TXMON_PPDU_COM(ppdu_info, tsft) = tsft;
		/* update tsft and ppdu timestamp */
		tx_mon_be->last_tsft = tsft;
		tx_mon_be->last_ppdu_timestamp = ppdu_timestamp;
	}

	if (!TXMON_PPDU_COM(ppdu_info, tsft) &&
	    !TXMON_PPDU_COM(ppdu_info, ppdu_timestamp))
		return QDF_STATUS_E_EMPTY;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_tx_mon_update_channel_freq() - API to update channel frequency and number
 * @pdev: pdev Handle
 * @soc: soc Handle
 * @freq: Frequency
 *
 * Return: void
 */
static inline void
dp_tx_mon_update_channel_freq(struct dp_pdev *pdev, struct dp_soc *soc,
			      uint16_t freq)
{
	if (soc && soc->cdp_soc.ol_ops->freq_to_channel) {
		uint8_t c_num;

		c_num = soc->cdp_soc.ol_ops->freq_to_channel(soc->ctrl_psoc,
							     pdev->pdev_id,
							     freq);
		pdev->operating_channel.num = c_num;
	}

	if (soc && soc->cdp_soc.ol_ops->freq_to_band) {
		uint8_t band;

		band = soc->cdp_soc.ol_ops->freq_to_band(soc->ctrl_psoc,
							 pdev->pdev_id,
							 freq);
		pdev->operating_channel.band = band;
	}
}

/**
 * dp_tx_mon_update_radiotap() - API to update radiotap information
 * @pdev: pdev Handle
 * @ppdu_info: pointer to dp_tx_ppdu_info
 *
 * Return: void
 */
static void
dp_tx_mon_update_radiotap(struct dp_pdev *pdev,
			  struct dp_tx_ppdu_info *ppdu_info)
{
	uint32_t usr_idx = 0;
	uint32_t num_users = 0;

	num_users = TXMON_PPDU_HAL(ppdu_info, num_users);

	if (qdf_unlikely(TXMON_PPDU_COM(ppdu_info, chan_freq) == 0 &&
			 TXMON_PPDU_COM(ppdu_info, chan_num) == 0)) {
		TXMON_PPDU_COM(ppdu_info, chan_freq) =
				pdev->operating_channel.freq;
		TXMON_PPDU_COM(ppdu_info, chan_num) =
				pdev->operating_channel.num;
	} else if (TXMON_PPDU_COM(ppdu_info, chan_freq) != 0 &&
		   TXMON_PPDU_COM(ppdu_info, chan_num) == 0) {
		uint16_t freq = TXMON_PPDU_COM(ppdu_info, chan_freq);

		if (qdf_unlikely(pdev->operating_channel.freq != freq)) {
			dp_tx_mon_update_channel_freq(pdev, pdev->soc, freq);
			pdev->operating_channel.freq = freq;
		}
		TXMON_PPDU_COM(ppdu_info,
			       chan_num) = pdev->operating_channel.num;
	}

	if (QDF_STATUS_SUCCESS !=
	    dp_populate_tsft_from_phy_timestamp(pdev, ppdu_info))
		return;

	/* update mlo timestamp */
	TXMON_PPDU_COM(ppdu_info, tsft) =
			(TXMON_PPDU_COM(ppdu_info, tsft) +
			 pdev->timestamp.mlo_offset_lo_us +
			 ((uint64_t)pdev->timestamp.mlo_offset_hi_us << 32));

	for (usr_idx = 0; usr_idx < num_users; usr_idx++) {
		qdf_nbuf_queue_t *mpdu_q = NULL;

		/* set AMPDU flag if number mpdu is more than 1 */
		mpdu_q = &TXMON_PPDU_USR(ppdu_info, usr_idx, mpdu_q);
		if (mpdu_q && (qdf_nbuf_queue_len(mpdu_q) > 1)) {
			TXMON_PPDU_COM(ppdu_info,
				       rs_flags) |= IEEE80211_AMPDU_FLAG;
			TXMON_PPDU_USR(ppdu_info, usr_idx, is_ampdu) = 1;
		}

		if (qdf_unlikely(!TXMON_PPDU_COM(ppdu_info, rate))) {
			uint32_t rate = 0;
			uint32_t rix = 0;
			uint16_t ratecode = 0;

			rate = dp_getrateindex(TXMON_PPDU_COM(ppdu_info, sgi),
					       TXMON_PPDU_USR(ppdu_info,
							      usr_idx, mcs),
					       TXMON_PPDU_COM(ppdu_info, nss),
					       TXMON_PPDU_COM(ppdu_info,
							      preamble_type),
					       TXMON_PPDU_COM(ppdu_info, bw),
					       0,
					       &rix, &ratecode);

			/* update rate */
			TXMON_PPDU_COM(ppdu_info, rate) = rate;
		}

		dp_tx_mon_send_per_usr_mpdu(pdev, ppdu_info, usr_idx);
	}
}

/**
 * dp_tx_mon_ppdu_process - Deferred PPDU stats handler
 * @context: Opaque work context (PDEV)
 *
 * Return: none
 */
static void dp_tx_mon_ppdu_process(void *context)
{
	struct dp_pdev *pdev = (struct dp_pdev *)context;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;
	struct dp_tx_ppdu_info *defer_ppdu_info = NULL;
	struct dp_tx_ppdu_info *defer_ppdu_info_next = NULL;
	struct dp_pdev_tx_monitor_be *tx_mon_be;

	/* sanity check */
	if (qdf_unlikely(!pdev))
		return;

	mon_pdev = pdev->monitor_pdev;

	if (qdf_unlikely(!mon_pdev))
		return;

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	if (qdf_unlikely(!mon_pdev_be))
		return;

	tx_mon_be = &mon_pdev_be->tx_monitor_be;
	if (qdf_unlikely(TX_MON_BE_DISABLE == tx_mon_be->mode &&
			 !dp_lite_mon_is_tx_enabled(mon_pdev)))
		return;

	/* take lock here */
	qdf_spin_lock_bh(&tx_mon_be->tx_mon_list_lock);
	STAILQ_CONCAT(&tx_mon_be->defer_tx_ppdu_info_queue,
		      &tx_mon_be->tx_ppdu_info_queue);
	tx_mon_be->defer_ppdu_info_list_depth +=
		tx_mon_be->tx_ppdu_info_list_depth;
	tx_mon_be->tx_ppdu_info_list_depth = 0;
	qdf_spin_unlock_bh(&tx_mon_be->tx_mon_list_lock);

	STAILQ_FOREACH_SAFE(defer_ppdu_info,
			    &tx_mon_be->defer_tx_ppdu_info_queue,
			    tx_ppdu_info_queue_elem, defer_ppdu_info_next) {
		/* remove dp_tx_ppdu_info from the list */
		STAILQ_REMOVE(&tx_mon_be->defer_tx_ppdu_info_queue,
			      defer_ppdu_info,
			      dp_tx_ppdu_info,
			      tx_ppdu_info_queue_elem);
		tx_mon_be->defer_ppdu_info_list_depth--;

		dp_tx_mon_update_radiotap(pdev, defer_ppdu_info);

		/* free the ppdu_info */
		dp_tx_mon_free_ppdu_info(defer_ppdu_info, tx_mon_be);
		defer_ppdu_info = NULL;
	}
}

void dp_tx_ppdu_stats_attach_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;
	struct dp_pdev_tx_monitor_be *tx_mon_be;

	if (qdf_unlikely(!pdev))
		return;

	mon_pdev = pdev->monitor_pdev;

	if (qdf_unlikely(!mon_pdev))
		return;

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	if (qdf_unlikely(!mon_pdev_be))
		return;

	tx_mon_be = &mon_pdev_be->tx_monitor_be;

	STAILQ_INIT(&tx_mon_be->tx_ppdu_info_queue);
	tx_mon_be->tx_ppdu_info_list_depth = 0;

	STAILQ_INIT(&tx_mon_be->defer_tx_ppdu_info_queue);
	tx_mon_be->defer_ppdu_info_list_depth = 0;

	qdf_spinlock_create(&tx_mon_be->tx_mon_list_lock);
	/* Work queue setup for TX MONITOR post handling */
	qdf_create_work(0, &tx_mon_be->post_ppdu_work,
			dp_tx_mon_ppdu_process, pdev);

	tx_mon_be->post_ppdu_workqueue =
			qdf_alloc_unbound_workqueue("tx_mon_ppdu_work_queue");
}

void dp_tx_ppdu_stats_detach_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_pdev_be *mon_pdev_be;
	struct dp_pdev_tx_monitor_be *tx_mon_be;
	struct dp_tx_ppdu_info *tx_ppdu_info = NULL;
	struct dp_tx_ppdu_info *tx_ppdu_info_next = NULL;

	if (qdf_unlikely(!pdev))
		return;

	mon_pdev = pdev->monitor_pdev;

	if (qdf_unlikely(!mon_pdev))
		return;

	mon_pdev_be = dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	if (qdf_unlikely(!mon_pdev_be))
		return;

	tx_mon_be = &mon_pdev_be->tx_monitor_be;
	/* TODO: disable tx_monitor, to avoid further packet from HW */
	dp_monitor_config_enh_tx_capture(pdev, TX_MON_BE_DISABLE);

	/* flush workqueue */
	qdf_flush_workqueue(0, tx_mon_be->post_ppdu_workqueue);
	qdf_destroy_workqueue(0, tx_mon_be->post_ppdu_workqueue);

	/*
	 * TODO: iterate both tx_ppdu_info and defer_ppdu_info_list
	 * free the tx_ppdu_info and decrement depth
	 */
	qdf_spin_lock_bh(&tx_mon_be->tx_mon_list_lock);
	STAILQ_FOREACH_SAFE(tx_ppdu_info,
			    &tx_mon_be->tx_ppdu_info_queue,
			    tx_ppdu_info_queue_elem, tx_ppdu_info_next) {
		/* remove dp_tx_ppdu_info from the list */
		STAILQ_REMOVE(&tx_mon_be->tx_ppdu_info_queue, tx_ppdu_info,
			      dp_tx_ppdu_info, tx_ppdu_info_queue_elem);
		/* decrement list length */
		tx_mon_be->tx_ppdu_info_list_depth--;
		/* free tx_ppdu_info */
		dp_tx_mon_free_ppdu_info(tx_ppdu_info, tx_mon_be);
	}
	qdf_spin_unlock_bh(&tx_mon_be->tx_mon_list_lock);

	qdf_spin_lock_bh(&tx_mon_be->tx_mon_list_lock);
	STAILQ_FOREACH_SAFE(tx_ppdu_info,
			    &tx_mon_be->defer_tx_ppdu_info_queue,
			    tx_ppdu_info_queue_elem, tx_ppdu_info_next) {
		/* remove dp_tx_ppdu_info from the list */
		STAILQ_REMOVE(&tx_mon_be->defer_tx_ppdu_info_queue,
			      tx_ppdu_info,
			      dp_tx_ppdu_info, tx_ppdu_info_queue_elem);
		/* decrement list length */
		tx_mon_be->defer_ppdu_info_list_depth--;
		/* free tx_ppdu_info */
		dp_tx_mon_free_ppdu_info(tx_ppdu_info, tx_mon_be);
	}
	qdf_spin_unlock_bh(&tx_mon_be->tx_mon_list_lock);

	qdf_spinlock_destroy(&tx_mon_be->tx_mon_list_lock);
}
#endif /* WLAN_TX_PKT_CAPTURE_ENH_BE */

#if (defined(WIFI_MONITOR_SUPPORT) && defined(WLAN_TX_MON_CORE_DEBUG))
/*
 * dp_config_enh_tx_core_monitor_2_0()- API to validate core framework
 * @pdev_handle: DP_PDEV handle
 * @val: user provided value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_config_enh_tx_core_monitor_2_0(struct dp_pdev *pdev, uint8_t val)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct dp_pdev_tx_monitor_be *tx_mon_be =
			&mon_pdev_be->tx_monitor_be;
	struct dp_soc *soc = pdev->soc;
	uint16_t num_of_buffers;
	QDF_STATUS status;

	soc_cfg_ctx = soc->wlan_cfg_ctx;
	switch (val) {
	case TX_MON_BE_FRM_WRK_DISABLE:
	{
		tx_mon_be->mode = val;
		mon_pdev_be->tx_mon_mode = 0;
		mon_pdev_be->tx_mon_filter_length = DMA_LENGTH_64B;
		break;
	}
	case TX_MON_BE_FRM_WRK_FULL_CAPTURE:
	{
		num_of_buffers = wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc_cfg_ctx);
		status = dp_vdev_set_monitor_mode_buf_rings_tx_2_0(pdev,
								   num_of_buffers);
		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("Tx monitor buffer allocation failed");
			return status;
		}
		tx_mon_be->mode = val;
		qdf_mem_zero(&tx_mon_be->stats,
			     sizeof(struct dp_tx_monitor_drop_stats));
		tx_mon_be->mode = val;
		mon_pdev_be->tx_mon_mode = 1;
		mon_pdev_be->tx_mon_filter_length = DEFAULT_DMA_LENGTH;
		break;
	}
	case TX_MON_BE_FRM_WRK_128B_CAPTURE:
	{
		status = dp_vdev_set_monitor_mode_buf_rings_tx_2_0(pdev,
								   DP_MON_RING_FILL_LEVEL_DEFAULT);
		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("Tx monitor buffer allocation failed");
			return status;
		}
		tx_mon_be->mode = val;
		mon_pdev_be->tx_mon_mode = 1;
		mon_pdev_be->tx_mon_filter_length = DMA_LENGTH_128B;
		break;
	}
	default:
	{
		return QDF_STATUS_E_INVAL;
	}
	}

	dp_mon_debug("Tx monitor mode:%d mon_mode_flag:%d config_length:%d",
		    tx_mon_be->mode, mon_pdev_be->tx_mon_mode,
		    mon_pdev_be->tx_mon_filter_length);

	/* send HTT msg to configure TLV based on mode */
	dp_mon_filter_setup_tx_mon_mode(pdev);
	dp_tx_mon_filter_update(pdev);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_PKT_CAPTURE_TX_2_0
QDF_STATUS dp_tx_mon_pdev_htt_srng_setup_2_0(struct dp_soc *soc,
					     struct dp_pdev *pdev,
					     int mac_id,
					     int mac_for_pdev)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	return htt_srng_setup(soc->htt_handle, mac_for_pdev,
			      mon_soc_be->tx_mon_dst_ring[mac_id].hal_srng,
			      TX_MONITOR_DST);
}

QDF_STATUS dp_tx_mon_soc_htt_srng_setup_2_0(struct dp_soc *soc,
					    int mac_id)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	hal_set_low_threshold(mon_soc_be->tx_mon_buf_ring.hal_srng, 0);
	return htt_srng_setup(soc->htt_handle, mac_id,
				mon_soc_be->tx_mon_buf_ring.hal_srng,
				TX_MONITOR_BUF);
}

QDF_STATUS dp_tx_mon_pdev_rings_alloc_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
	struct dp_soc *soc = pdev->soc;
	int entries;
	struct wlan_cfg_dp_pdev_ctxt *pdev_cfg_ctx;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	pdev_cfg_ctx = pdev->wlan_cfg_ctx;
	entries = wlan_cfg_get_dma_tx_mon_dest_ring_size(pdev_cfg_ctx);

	return dp_srng_alloc(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id],
				  TX_MONITOR_DST, entries, 0);
}

void dp_tx_mon_pdev_rings_free_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	dp_srng_free(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id]);
}

QDF_STATUS dp_tx_mon_pdev_rings_init_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	return dp_srng_init(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id],
				 TX_MONITOR_DST, pdev->pdev_id, lmac_id);
}

void dp_tx_mon_pdev_rings_deinit_2_0(struct dp_pdev *pdev, uint32_t lmac_id)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
			dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	dp_srng_deinit(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id],
		       TX_MONITOR_DST, pdev->pdev_id);
}

QDF_STATUS dp_tx_mon_soc_attach_2_0(struct dp_soc *soc, uint32_t lmac_id)
{
	int entries;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	entries = wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc_cfg_ctx);
	qdf_print("%s:%d tx mon buf entries: %d", __func__, __LINE__, entries);

	return dp_srng_alloc(soc, &mon_soc_be->tx_mon_buf_ring,
			  TX_MONITOR_BUF, entries, 0);
}

QDF_STATUS dp_tx_mon_soc_detach_2_0(struct dp_soc *soc, uint32_t lmac_id)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
			dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	if (!mon_soc_be) {
		dp_mon_err("DP MON SOC NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dp_tx_mon_buf_desc_pool_free(soc);
	dp_srng_free(soc, &mon_soc_be->tx_mon_buf_ring);
	return QDF_STATUS_SUCCESS;
}

#endif
