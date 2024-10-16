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

#include <dp_types.h>
#include "dp_rx.h"
#include "dp_peer.h"
#include <dp_htt.h>
#include <dp_mon_filter.h>
#include <dp_mon.h>
#include <dp_rx_mon.h>
#include <dp_rx_mon_2.0.h>
#include <dp_mon_2.0.h>
#include <dp_mon_filter_2.0.h>
#include <dp_tx_mon_2.0.h>
#include <hal_be_api_mon.h>
#include <dp_be.h>
#include <htt_ppdu_stats.h>
#ifdef QCA_SUPPORT_LITE_MONITOR
#include "dp_lite_mon.h"
#endif

#if !defined(DISABLE_MON_CONFIG)

QDF_STATUS dp_rx_mon_ppdu_info_cache_create(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	uint16_t obj;
	struct hal_rx_ppdu_info *ppdu_info = NULL;

	mon_pdev_be->ppdu_info_cache =
		qdf_kmem_cache_create("rx_mon_ppdu_info_cache",
				      sizeof(struct hal_rx_ppdu_info));

	if (!mon_pdev_be->ppdu_info_cache) {
		dp_mon_err("cache creation failed pdev :%px", pdev);
		return QDF_STATUS_E_NOMEM;
	}

	TAILQ_INIT(&mon_pdev_be->rx_mon_free_queue);
	for (obj = 0; obj < DP_RX_MON_WQ_THRESHOLD; obj++) {
		ppdu_info =  (struct hal_rx_ppdu_info *)qdf_kmem_cache_alloc(mon_pdev_be->ppdu_info_cache);

		if (ppdu_info) {
			TAILQ_INSERT_TAIL(&mon_pdev_be->rx_mon_free_queue,
					  ppdu_info,
					  ppdu_free_list_elem);
			mon_pdev_be->total_free_elem++;
		}
	}
	qdf_spinlock_create(&mon_pdev_be->ppdu_info_lock);

	return QDF_STATUS_SUCCESS;
}

void dp_rx_mon_ppdu_info_cache_destroy(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);
	struct hal_rx_ppdu_info *ppdu_info = NULL, *temp_ppdu_info = NULL;

	qdf_spin_lock(&mon_pdev_be->ppdu_info_lock);
	TAILQ_FOREACH_SAFE(ppdu_info,
			   &mon_pdev_be->rx_mon_free_queue,
			   ppdu_free_list_elem,
			   temp_ppdu_info) {
		TAILQ_REMOVE(&mon_pdev_be->rx_mon_free_queue,
			     ppdu_info, ppdu_free_list_elem);
		if (ppdu_info) {
			mon_pdev_be->total_free_elem--;
			qdf_kmem_cache_free(mon_pdev_be->ppdu_info_cache,
					    ppdu_info);
		}
	}
	qdf_spin_unlock(&mon_pdev_be->ppdu_info_lock);
	dp_mon_debug(" total free element: %d", mon_pdev_be->total_free_elem);
	qdf_kmem_cache_destroy(mon_pdev_be->ppdu_info_cache);
}

/**
 * dp_mon_pdev_ext_init_2_0() - Init pdev ext param
 *
 * @pdev: DP pdev handle
 *
 * Return:  QDF_STATUS_SUCCESS: Success
 *          QDF_STATUS_E_FAILURE: failure
 */
QDF_STATUS dp_mon_pdev_ext_init_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	qdf_create_work(0, &mon_pdev_be->rx_mon_work,
			dp_rx_mon_process_ppdu, pdev);
	mon_pdev_be->rx_mon_workqueue =
		qdf_alloc_unbound_workqueue("rx_mon_work_queue");

	if (!mon_pdev_be->rx_mon_workqueue) {
		dp_mon_err("failed to create rxmon wq mon_pdev: %pK", mon_pdev);
		goto fail;
	}
	TAILQ_INIT(&mon_pdev_be->rx_mon_queue);

	qdf_spinlock_create(&mon_pdev_be->rx_mon_wq_lock);

	return QDF_STATUS_SUCCESS;

fail:
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_mon_pdev_ext_deinit_2_0() - denit pdev ext param
 *
 * @pdev: DP pdev handle
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS dp_mon_pdev_ext_deinit_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	if (!mon_pdev_be->rx_mon_workqueue)
		return QDF_STATUS_E_FAILURE;

	qdf_err(" total free element: %d", mon_pdev_be->total_free_elem);
	qdf_flush_workqueue(0, mon_pdev_be->rx_mon_workqueue);
	qdf_destroy_workqueue(0, mon_pdev_be->rx_mon_workqueue);
	qdf_flush_work(&mon_pdev_be->rx_mon_work);
	qdf_disable_work(&mon_pdev_be->rx_mon_work);
	dp_rx_mon_drain_wq(pdev);
	mon_pdev_be->rx_mon_workqueue = NULL;
	qdf_spinlock_destroy(&mon_pdev_be->rx_mon_wq_lock);

	return QDF_STATUS_SUCCESS;
}

/*
 * dp_mon_add_desc_list_to_free_list() - append unused desc_list back to
 *					freelist.
 *
 * @soc: core txrx main context
 * @local_desc_list: local desc list provided by the caller
 * @tail: attach the point to last desc of local desc list
 * @mon_desc_pool: monitor descriptor pool pointer
 */
void
dp_mon_add_desc_list_to_free_list(struct dp_soc *soc,
				  union dp_mon_desc_list_elem_t **local_desc_list,
				  union dp_mon_desc_list_elem_t **tail,
				  struct dp_mon_desc_pool *mon_desc_pool)
{
	union dp_mon_desc_list_elem_t *temp_list = NULL;

	qdf_spin_lock_bh(&mon_desc_pool->lock);

	temp_list = mon_desc_pool->freelist;
	mon_desc_pool->freelist = *local_desc_list;
	(*tail)->next = temp_list;
	*tail = NULL;
	*local_desc_list = NULL;

	qdf_spin_unlock_bh(&mon_desc_pool->lock);
}

/*
 * dp_mon_get_free_desc_list() - provide a list of descriptors from
 *				the free mon desc pool.
 *
 * @soc: core txrx main context
 * @mon_desc_pool: monitor descriptor pool pointer
 * @num_descs: number of descs requested from freelist
 * @desc_list: attach the descs to this list (output parameter)
 * @tail: attach the point to last desc of free list (output parameter)
 *
 * Return: number of descs allocated from free list.
 */
static uint16_t
dp_mon_get_free_desc_list(struct dp_soc *soc,
			  struct dp_mon_desc_pool *mon_desc_pool,
			  uint16_t num_descs,
			  union dp_mon_desc_list_elem_t **desc_list,
			  union dp_mon_desc_list_elem_t **tail)
{
	uint16_t count;

	qdf_spin_lock_bh(&mon_desc_pool->lock);

	*desc_list = *tail = mon_desc_pool->freelist;

	for (count = 0; count < num_descs; count++) {
		if (qdf_unlikely(!mon_desc_pool->freelist)) {
			qdf_spin_unlock_bh(&mon_desc_pool->lock);
			return count;
		}
		*tail = mon_desc_pool->freelist;
		mon_desc_pool->freelist = mon_desc_pool->freelist->next;
	}
	(*tail)->next = NULL;
	qdf_spin_unlock_bh(&mon_desc_pool->lock);
	return count;
}

void dp_mon_pool_frag_unmap_and_free(struct dp_soc *soc,
				     struct dp_mon_desc_pool *mon_desc_pool)
{
	int desc_id;
	qdf_frag_t vaddr;
	qdf_dma_addr_t paddr;

	qdf_spin_lock_bh(&mon_desc_pool->lock);
	for (desc_id = 0; desc_id < mon_desc_pool->pool_size; desc_id++) {
		if (mon_desc_pool->array[desc_id].mon_desc.in_use) {
			vaddr = mon_desc_pool->array[desc_id].mon_desc.buf_addr;
			paddr = mon_desc_pool->array[desc_id].mon_desc.paddr;

			if (!(mon_desc_pool->array[desc_id].mon_desc.unmapped)) {
				qdf_mem_unmap_page(soc->osdev, paddr,
						   mon_desc_pool->buf_size,
						   QDF_DMA_FROM_DEVICE);
				mon_desc_pool->array[desc_id].mon_desc.unmapped = 1;
				mon_desc_pool->array[desc_id].mon_desc.cookie = desc_id;
			}
			qdf_frag_free(vaddr);
		}
	}
	qdf_spin_unlock_bh(&mon_desc_pool->lock);
}

static inline QDF_STATUS
dp_mon_frag_alloc_and_map(struct dp_soc *dp_soc,
			  struct dp_mon_desc *mon_desc,
			  struct dp_mon_desc_pool *mon_desc_pool)
{
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	mon_desc->buf_addr = qdf_frag_alloc(&mon_desc_pool->pf_cache,
					    mon_desc_pool->buf_size);

	if (!mon_desc->buf_addr) {
		dp_mon_err("Frag alloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	ret = qdf_mem_map_page(dp_soc->osdev,
			       mon_desc->buf_addr,
			       QDF_DMA_FROM_DEVICE,
			       mon_desc_pool->buf_size,
			       &mon_desc->paddr);

	if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
		qdf_frag_free(mon_desc->buf_addr);
		dp_mon_err("Frag map failed");
		return QDF_STATUS_E_FAULT;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_mon_buffers_replenish(struct dp_soc *dp_soc,
			 struct dp_srng *dp_mon_srng,
			 struct dp_mon_desc_pool *mon_desc_pool,
			 uint32_t num_req_buffers,
			 union dp_mon_desc_list_elem_t **desc_list,
			 union dp_mon_desc_list_elem_t **tail,
			 uint32_t *replenish_cnt_ref)
{
	uint32_t num_alloc_desc;
	uint32_t num_entries_avail;
	uint32_t count = 0;
	int sync_hw_ptr = 1;
	struct dp_mon_desc mon_desc = {0};
	void *mon_ring_entry;
	union dp_mon_desc_list_elem_t *next;
	void *mon_srng;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;
	struct dp_mon_soc *mon_soc = dp_soc->monitor_soc;

	if (!num_req_buffers) {
		dp_mon_debug("%pK: Received request for 0 buffers replenish",
			     dp_soc);
		ret = QDF_STATUS_E_INVAL;
		goto free_desc;
	}

	mon_srng = dp_mon_srng->hal_srng;

	hal_srng_access_start(dp_soc->hal_soc, mon_srng);

	num_entries_avail = hal_srng_src_num_avail(dp_soc->hal_soc,
						   mon_srng, sync_hw_ptr);

	if (!num_entries_avail) {
		hal_srng_access_end(dp_soc->hal_soc, mon_srng);
		goto free_desc;
	}
	if (num_entries_avail < num_req_buffers) {
		num_req_buffers = num_entries_avail;
	}

	/*
	 * if desc_list is NULL, allocate the descs from freelist
	 */
	if (!(*desc_list)) {
		num_alloc_desc = dp_mon_get_free_desc_list(dp_soc,
							   mon_desc_pool,
							   num_req_buffers,
							   desc_list,
							   tail);

		if (!num_alloc_desc) {
			dp_mon_debug("%pK: no free rx_descs in freelist", dp_soc);
			hal_srng_access_end(dp_soc->hal_soc, mon_srng);
			return QDF_STATUS_E_NOMEM;
		}

		dp_mon_info("%pK: %d rx desc allocated",
			    dp_soc, num_alloc_desc);

		num_req_buffers = num_alloc_desc;
	}

	while (count <= num_req_buffers - 1) {
		ret = dp_mon_frag_alloc_and_map(dp_soc,
						&mon_desc,
						mon_desc_pool);

		if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
			if (qdf_unlikely(ret  == QDF_STATUS_E_FAULT))
				continue;
			break;
		}

		count++;
		next = (*desc_list)->next;
		mon_ring_entry = hal_srng_src_get_next(
						dp_soc->hal_soc,
						mon_srng);

		if (!mon_ring_entry)
			break;

		qdf_assert_always((*desc_list)->mon_desc.in_use == 0);

		(*desc_list)->mon_desc.in_use = 1;
		(*desc_list)->mon_desc.unmapped = 0;
		(*desc_list)->mon_desc.buf_addr = mon_desc.buf_addr;
		(*desc_list)->mon_desc.paddr = mon_desc.paddr;
		(*desc_list)->mon_desc.magic = DP_MON_DESC_MAGIC;

		mon_soc->stats.frag_alloc++;
		hal_mon_buff_addr_info_set(dp_soc->hal_soc,
					   mon_ring_entry,
					   &((*desc_list)->mon_desc),
					   mon_desc.paddr);

		*desc_list = next;
	}

	hal_srng_access_end(dp_soc->hal_soc, mon_srng);
	if (replenish_cnt_ref)
		*replenish_cnt_ref += count;

free_desc:
	/*
	 * add any available free desc back to the free list
	 */
	if (*desc_list) {
		dp_mon_add_desc_list_to_free_list(dp_soc, desc_list, tail,
						  mon_desc_pool);
	}

	return ret;
}

QDF_STATUS
dp_mon_desc_pool_init(struct dp_mon_desc_pool *mon_desc_pool,
		      uint32_t pool_size)
{
	int desc_id;
	/* Initialize monitor desc lock */
	qdf_spinlock_create(&mon_desc_pool->lock);

	qdf_spin_lock_bh(&mon_desc_pool->lock);

	mon_desc_pool->buf_size = DP_MON_DATA_BUFFER_SIZE;
	/* link SW descs into a freelist */
	mon_desc_pool->freelist = &mon_desc_pool->array[0];
	mon_desc_pool->pool_size = pool_size - 1;
	qdf_mem_zero(mon_desc_pool->freelist,
		     mon_desc_pool->pool_size *
		     sizeof(union dp_mon_desc_list_elem_t));

	for (desc_id = 0; desc_id < mon_desc_pool->pool_size; desc_id++) {
		if (desc_id == mon_desc_pool->pool_size - 1)
			mon_desc_pool->array[desc_id].next = NULL;
		else
			mon_desc_pool->array[desc_id].next =
				&mon_desc_pool->array[desc_id + 1];
		mon_desc_pool->array[desc_id].mon_desc.in_use = 0;
		mon_desc_pool->array[desc_id].mon_desc.cookie = desc_id;
	}
	qdf_spin_unlock_bh(&mon_desc_pool->lock);

	return QDF_STATUS_SUCCESS;
}

void dp_mon_desc_pool_deinit(struct dp_mon_desc_pool *mon_desc_pool)
{
	qdf_spin_lock_bh(&mon_desc_pool->lock);

	mon_desc_pool->freelist = NULL;
	mon_desc_pool->pool_size = 0;

	qdf_spin_unlock_bh(&mon_desc_pool->lock);
	qdf_spinlock_destroy(&mon_desc_pool->lock);
}

void dp_mon_desc_pool_free(struct dp_mon_desc_pool *mon_desc_pool)
{
	qdf_mem_free(mon_desc_pool->array);
}

QDF_STATUS dp_mon_desc_pool_alloc(uint32_t pool_size,
				  struct dp_mon_desc_pool *mon_desc_pool)
{
	mon_desc_pool->pool_size = pool_size - 1;
	mon_desc_pool->array = qdf_mem_malloc((mon_desc_pool->pool_size) *
				     sizeof(union dp_mon_desc_list_elem_t));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_vdev_set_monitor_mode_buf_rings_rx_2_0(struct dp_pdev *pdev)
{
	int rx_mon_max_entries;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	QDF_STATUS status;

	if (!mon_soc_be) {
		dp_mon_err("DP MON SOC is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	soc_cfg_ctx = soc->wlan_cfg_ctx;
	rx_mon_max_entries = wlan_cfg_get_dp_soc_rx_mon_buf_ring_size(soc_cfg_ctx);

	hal_set_low_threshold(soc->rxdma_mon_buf_ring[0].hal_srng,
			      MON_BUF_MIN_ENTRIES << 2);
	status = htt_srng_setup(soc->htt_handle, 0,
				soc->rxdma_mon_buf_ring[0].hal_srng,
				RXDMA_MONITOR_BUF);

	if (status != QDF_STATUS_SUCCESS) {
		dp_mon_err("Failed to send htt srng setup message for Rx mon buf ring");
		return status;
	}

	if (mon_soc_be->rx_mon_ring_fill_level < rx_mon_max_entries) {
		status = dp_rx_mon_buffers_alloc(soc,
						 (rx_mon_max_entries -
						 mon_soc_be->rx_mon_ring_fill_level));
		if (status != QDF_STATUS_SUCCESS) {
			dp_mon_err("%pK: Rx mon buffers allocation failed", soc);
			return status;
		}
		mon_soc_be->rx_mon_ring_fill_level +=
				(rx_mon_max_entries -
				mon_soc_be->rx_mon_ring_fill_level);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_vdev_set_monitor_mode_buf_rings_tx_2_0(struct dp_pdev *pdev,
						     uint16_t num_of_buffers)
{
	int tx_mon_max_entries;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	QDF_STATUS status;

	if (!mon_soc_be) {
		dp_mon_err("DP MON SOC is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	soc_cfg_ctx = soc->wlan_cfg_ctx;
	tx_mon_max_entries =
		wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc_cfg_ctx);

	hal_set_low_threshold(mon_soc_be->tx_mon_buf_ring.hal_srng,
			      tx_mon_max_entries >> 2);
	status = htt_srng_setup(soc->htt_handle, 0,
				mon_soc_be->tx_mon_buf_ring.hal_srng,
				TX_MONITOR_BUF);

	if (status != QDF_STATUS_SUCCESS) {
		dp_mon_err("Failed to send htt srng setup message for Tx mon buf ring");
		return status;
	}

	if (mon_soc_be->tx_mon_ring_fill_level < num_of_buffers) {
		if (dp_tx_mon_buffers_alloc(soc,
					    (num_of_buffers -
					     mon_soc_be->tx_mon_ring_fill_level))) {
			dp_mon_err("%pK: Tx mon buffers allocation failed",
				   soc);
			return QDF_STATUS_E_FAILURE;
		}
		mon_soc_be->tx_mon_ring_fill_level +=
					(num_of_buffers -
					mon_soc_be->tx_mon_ring_fill_level);
	}

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_vdev_set_monitor_mode_buf_rings_2_0(struct dp_pdev *pdev)
{
	int status;
	struct dp_soc *soc = pdev->soc;

	status = dp_vdev_set_monitor_mode_buf_rings_rx_2_0(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		dp_mon_err("%pK: Rx monitor extra buffer allocation failed",
			   soc);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_vdev_set_monitor_mode_rings_2_0(struct dp_pdev *pdev,
					      uint8_t delayed_replenish)
{
	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_tx_enable_enhanced_stats_2_0() - Send HTT cmd to FW to enable stats
 * @pdev: Datapath pdev handle
 *
 * Return: none
 */
static void dp_mon_tx_enable_enhanced_stats_2_0(struct dp_pdev *pdev)
{
	dp_h2t_cfg_stats_msg_send(pdev, DP_PPDU_STATS_CFG_ENH_STATS,
				  pdev->pdev_id);
}

/**
 * dp_mon_tx_disable_enhanced_stats_2_0() - Send HTT cmd to FW to disable stats
 * @pdev: Datapath pdev handle
 *
 * Return: none
 */
static void dp_mon_tx_disable_enhanced_stats_2_0(struct dp_pdev *pdev)
{
	dp_h2t_cfg_stats_msg_send(pdev, 0, pdev->pdev_id);
}
#endif

#if defined(QCA_ENHANCED_STATS_SUPPORT) && defined(WLAN_FEATURE_11BE)
void
dp_mon_tx_stats_update_2_0(struct dp_mon_peer *mon_peer,
			   struct cdp_tx_completion_ppdu_user *ppdu)
{
	uint8_t preamble, mcs, punc_mode;

	preamble = ppdu->preamble;
	mcs = ppdu->mcs;

	punc_mode = dp_mon_get_puncture_type(ppdu->punc_pattern_bitmap,
					     ppdu->bw);
	ppdu->punc_mode = punc_mode;

	DP_STATS_INC(mon_peer, tx.punc_bw[punc_mode], ppdu->num_msdu);
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[MAX_MCS - 1],
		      ppdu->num_msdu,
		      ((mcs >= MAX_MCS_11BE) && (preamble == DOT11_BE)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[mcs],
		      ppdu->num_msdu,
		      ((mcs < MAX_MCS_11BE) && (preamble == DOT11_BE)));
	DP_STATS_INCC(mon_peer,
		      tx.su_be_ppdu_cnt.mcs_count[MAX_MCS - 1], 1,
		      ((mcs >= MAX_MCS_11BE) && (preamble == DOT11_BE) &&
		      (ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_SU)));
	DP_STATS_INCC(mon_peer,
		      tx.su_be_ppdu_cnt.mcs_count[mcs], 1,
		      ((mcs < MAX_MCS_11BE) && (preamble == DOT11_BE) &&
		      (ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_SU)));
	DP_STATS_INCC(mon_peer,
		      tx.mu_be_ppdu_cnt[TXRX_TYPE_MU_OFDMA].mcs_count[MAX_MCS - 1],
		      1, ((mcs >= MAX_MCS_11BE) &&
		      (preamble == DOT11_BE) &&
		      (ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA)));
	DP_STATS_INCC(mon_peer,
		      tx.mu_be_ppdu_cnt[TXRX_TYPE_MU_OFDMA].mcs_count[mcs],
		      1, ((mcs < MAX_MCS_11BE) &&
		      (preamble == DOT11_BE) &&
		      (ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA)));
	DP_STATS_INCC(mon_peer,
		      tx.mu_be_ppdu_cnt[TXRX_TYPE_MU_MIMO].mcs_count[MAX_MCS - 1],
		      1, ((mcs >= MAX_MCS_11BE) &&
		      (preamble == DOT11_BE) &&
		      (ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO)));
	DP_STATS_INCC(mon_peer,
		      tx.mu_be_ppdu_cnt[TXRX_TYPE_MU_MIMO].mcs_count[mcs],
		      1, ((mcs < MAX_MCS_11BE) &&
		      (preamble == DOT11_BE) &&
		      (ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO)));
}

enum cdp_punctured_modes
dp_mon_get_puncture_type(uint16_t puncture_pattern, uint8_t bw)
{
	uint16_t mask;
	uint8_t punctured_bits;

	if (!puncture_pattern)
		return NO_PUNCTURE;

	switch (bw) {
	case CMN_BW_80MHZ:
		mask = PUNCTURE_80MHZ_MASK;
		break;
	case CMN_BW_160MHZ:
		mask = PUNCTURE_160MHZ_MASK;
		break;
	case CMN_BW_320MHZ:
		mask = PUNCTURE_320MHZ_MASK;
		break;
	default:
		return NO_PUNCTURE;
	}

	/* 0s in puncture pattern received in TLV indicates punctured 20Mhz,
	 * after complement, 1s will indicate punctured 20Mhz
	 */
	puncture_pattern = ~puncture_pattern;
	puncture_pattern &= mask;

	if (puncture_pattern) {
		punctured_bits = 0;
		while (puncture_pattern != 0) {
			punctured_bits++;
			puncture_pattern &= (puncture_pattern - 1);
		}

		if (bw == CMN_BW_80MHZ) {
			if (punctured_bits == IEEE80211_PUNC_MINUS20MHZ)
				return PUNCTURED_20MHZ;
			else
				return NO_PUNCTURE;
		} else if (bw == CMN_BW_160MHZ) {
			if (punctured_bits == IEEE80211_PUNC_MINUS20MHZ)
				return PUNCTURED_20MHZ;
			else if (punctured_bits == IEEE80211_PUNC_MINUS40MHZ)
				return PUNCTURED_40MHZ;
			else
				return NO_PUNCTURE;
		} else if (bw == CMN_BW_320MHZ) {
			if (punctured_bits == IEEE80211_PUNC_MINUS40MHZ)
				return PUNCTURED_40MHZ;
			else if (punctured_bits == IEEE80211_PUNC_MINUS80MHZ)
				return PUNCTURED_80MHZ;
			else if (punctured_bits == IEEE80211_PUNC_MINUS120MHZ)
				return PUNCTURED_120MHZ;
			else
				return NO_PUNCTURE;
		}
	}
	return NO_PUNCTURE;
}
#endif

#if defined(QCA_ENHANCED_STATS_SUPPORT) && !defined(WLAN_FEATURE_11BE)
void
dp_mon_tx_stats_update_2_0(struct dp_mon_peer *mon_peer,
			   struct cdp_tx_completion_ppdu_user *ppdu)
{
	ppdu->punc_mode = NO_PUNCTURE;
}

enum cdp_punctured_modes
dp_mon_get_puncture_type(uint16_t puncture_pattern, uint8_t bw)
{
	return NO_PUNCTURE;
}
#endif /* QCA_ENHANCED_STATS_SUPPORT && WLAN_FEATURE_11BE */

#ifdef QCA_SUPPORT_BPR
static QDF_STATUS
dp_set_bpr_enable_2_0(struct dp_pdev *pdev, int val)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_SUPPORT_BPR */

#ifdef QCA_ENHANCED_STATS_SUPPORT
#ifdef WDI_EVENT_ENABLE
/**
 * dp_ppdu_desc_notify_2_0 - Notify upper layer for PPDU indication via WDI
 *
 * @pdev: Datapath pdev handle
 * @nbuf: Buffer to be shipped
 *
 * Return: void
 */
static void dp_ppdu_desc_notify_2_0(struct dp_pdev *pdev, qdf_nbuf_t nbuf)
{
	struct cdp_tx_completion_ppdu *ppdu_desc = NULL;

	ppdu_desc = (struct cdp_tx_completion_ppdu *)qdf_nbuf_data(nbuf);

	if (ppdu_desc->num_mpdu != 0 && ppdu_desc->num_users != 0 &&
	    ppdu_desc->frame_ctrl & HTT_FRAMECTRL_DATATYPE) {
		dp_wdi_event_handler(WDI_EVENT_TX_PPDU_DESC,
				     pdev->soc,
				     nbuf, HTT_INVALID_PEER,
				     WDI_NO_VAL,
				     pdev->pdev_id);
	} else {
		qdf_nbuf_free(nbuf);
	}
}
#endif

/**
 * dp_ppdu_stats_feat_enable_check_2_0 - Check if feature(s) is enabled to
 *				consume ppdu stats from FW
 *
 * @pdev: Datapath pdev handle
 *
 * Return: true if enabled, else return false
 */
static bool dp_ppdu_stats_feat_enable_check_2_0(struct dp_pdev *pdev)
{
	return pdev->monitor_pdev->enhanced_stats_en;
}
#endif

static
QDF_STATUS dp_mon_soc_htt_srng_setup_2_0(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	QDF_STATUS status;

	hal_set_low_threshold(soc->rxdma_mon_buf_ring[0].hal_srng,
			      MON_BUF_MIN_ENTRIES << 2);
	status = htt_srng_setup(soc->htt_handle, 0,
				soc->rxdma_mon_buf_ring[0].hal_srng,
				RXDMA_MONITOR_BUF);

	if (status != QDF_STATUS_SUCCESS) {
		dp_err("Failed to send htt srng setup message for Rx mon buf ring");
		return status;
	}

	hal_set_low_threshold(mon_soc_be->tx_mon_buf_ring.hal_srng, 0);
	status = htt_srng_setup(soc->htt_handle, 0,
				mon_soc_be->tx_mon_buf_ring.hal_srng,
				TX_MONITOR_BUF);
	if (status != QDF_STATUS_SUCCESS) {
		dp_err("Failed to send htt srng setup message for Tx mon buf ring");
		return status;
	}

	return status;
}

static
QDF_STATUS dp_mon_pdev_htt_srng_setup_2_0(struct dp_soc *soc,
					  struct dp_pdev *pdev,
					  int mac_id,
					  int mac_for_pdev)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	QDF_STATUS status;

	if (!soc->rxdma_mon_dst_ring[mac_id].hal_srng)
		return QDF_STATUS_SUCCESS;

	status = htt_srng_setup(soc->htt_handle, mac_for_pdev,
				soc->rxdma_mon_dst_ring[mac_id].hal_srng,
				RXDMA_MONITOR_DST);

	if (status != QDF_STATUS_SUCCESS) {
		dp_mon_err("Failed to send htt srng setup message for Rxdma dst ring");
		return status;
	}

	if (!mon_soc_be->tx_mon_dst_ring[mac_id].hal_srng)
		return QDF_STATUS_SUCCESS;

	status = htt_srng_setup(soc->htt_handle, mac_for_pdev,
				mon_soc_be->tx_mon_dst_ring[mac_id].hal_srng,
				TX_MONITOR_DST);

	if (status != QDF_STATUS_SUCCESS) {
		dp_mon_err("Failed to send htt srng message for Tx mon dst ring");
		return status;
	}

	return status;
}

QDF_STATUS dp_tx_mon_refill_buf_ring_2_0(struct dp_intr *int_ctx)
{
	struct dp_soc *soc  = int_ctx->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_srng *tx_mon_buf_ring;
	struct dp_intr_stats *intr_stats = &int_ctx->intr_stats;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	uint32_t num_entries_avail;
	int sync_hw_ptr = 1;
	void *hal_srng;

	tx_mon_buf_ring = &mon_soc_be->tx_mon_buf_ring;
	hal_srng = tx_mon_buf_ring->hal_srng;

	intr_stats->num_host2txmon_ring__masks++;
	mon_soc_be->tx_low_thresh_intrs++;
	hal_srng_access_start(soc->hal_soc, hal_srng);
	num_entries_avail = hal_srng_src_num_avail(soc->hal_soc,
						   hal_srng,
						   sync_hw_ptr);
	hal_srng_access_end(soc->hal_soc, hal_srng);

	if (num_entries_avail)
		dp_mon_buffers_replenish(soc, tx_mon_buf_ring,
					 &mon_soc_be->tx_desc_mon,
					 num_entries_avail, &desc_list, &tail,
					 NULL);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_mon_refill_buf_ring_2_0(struct dp_intr *int_ctx)
{
	struct dp_soc *soc  = int_ctx->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_srng *rx_mon_buf_ring;
	struct dp_intr_stats *intr_stats = &int_ctx->intr_stats;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	uint32_t num_entries_avail;
	int sync_hw_ptr = 1, hp = 0, tp = 0, num_entries;
	void *hal_srng;

	rx_mon_buf_ring = &soc->rxdma_mon_buf_ring[0];
	hal_srng = rx_mon_buf_ring->hal_srng;

	intr_stats->num_host2rxdma_ring_masks++;
	mon_soc_be->rx_low_thresh_intrs++;
	hal_srng_access_start(soc->hal_soc, hal_srng);
	num_entries_avail = hal_srng_src_num_avail(soc->hal_soc,
						   hal_srng,
						   sync_hw_ptr);
	hal_get_sw_hptp(soc->hal_soc, (hal_ring_handle_t)hal_srng, &tp, &hp);
	hal_srng_access_end(soc->hal_soc, hal_srng);

	num_entries = num_entries_avail;
	if (mon_soc_be->rx_mon_ring_fill_level < rx_mon_buf_ring->num_entries)
		num_entries = num_entries_avail - mon_soc_be->rx_mon_ring_fill_level;

	if (num_entries)
		dp_mon_buffers_replenish(soc, rx_mon_buf_ring,
					 &mon_soc_be->rx_desc_mon,
					 num_entries, &desc_list, &tail,
					 NULL);

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS dp_mon_soc_detach_2_0(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	if (!mon_soc_be) {
		dp_mon_err("DP MON SOC NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dp_rx_mon_buf_desc_pool_free(soc);
	dp_srng_free(soc, &soc->rxdma_mon_buf_ring[0]);
	dp_tx_mon_buf_desc_pool_free(soc);
	dp_srng_free(soc, &mon_soc_be->tx_mon_buf_ring);

	return QDF_STATUS_SUCCESS;
}

static void dp_mon_soc_deinit_2_0(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	if (!mon_soc_be->is_dp_mon_soc_initialized)
		return;

	dp_rx_mon_buffers_free(soc);
	dp_tx_mon_buffers_free(soc);

	dp_rx_mon_buf_desc_pool_deinit(soc);
	dp_tx_mon_buf_desc_pool_deinit(soc);

	dp_srng_deinit(soc, &soc->rxdma_mon_buf_ring[0], RXDMA_MONITOR_BUF, 0);
	dp_srng_deinit(soc, &mon_soc_be->tx_mon_buf_ring, TX_MONITOR_BUF, 0);

	mon_soc_be->is_dp_mon_soc_initialized = false;
}

static
QDF_STATUS dp_rx_mon_soc_init_2_0(struct dp_soc *soc)
{
	if (dp_srng_init(soc, &soc->rxdma_mon_buf_ring[0],
			 RXDMA_MONITOR_BUF, 0, 0)) {
		dp_mon_err("%pK: " RNG_ERR "rx_mon_buf_ring", soc);
		goto fail;
	}

	if (dp_rx_mon_buf_desc_pool_init(soc)) {
		dp_mon_err("%pK: " RNG_ERR "rx mon desc pool init", soc);
		goto fail;
	}

	/* monitor buffers for src */
	if (dp_rx_mon_buffers_alloc(soc, DP_MON_RING_FILL_LEVEL_DEFAULT)) {
		dp_mon_err("%pK: Rx mon buffers allocation failed", soc);
		goto fail;
	}

	return QDF_STATUS_SUCCESS;
fail:
	return QDF_STATUS_E_FAILURE;
}

static
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

static
QDF_STATUS dp_mon_soc_init_2_0(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	if (soc->rxdma_mon_buf_ring[0].hal_srng) {
		dp_mon_info("%pK: mon soc init is done", soc);
		return QDF_STATUS_SUCCESS;
	}

	if (dp_rx_mon_soc_init_2_0(soc)) {
		dp_mon_err("%pK: " RNG_ERR "tx_mon_buf_ring", soc);
		goto fail;
	}

	if (dp_tx_mon_soc_init_2_0(soc)) {
		dp_mon_err("%pK: " RNG_ERR "tx_mon_buf_ring", soc);
		goto fail;
	}

	mon_soc_be->tx_mon_ring_fill_level = 0;
	mon_soc_be->rx_mon_ring_fill_level = DP_MON_RING_FILL_LEVEL_DEFAULT;

	mon_soc_be->is_dp_mon_soc_initialized = true;
	return QDF_STATUS_SUCCESS;
fail:
	dp_mon_soc_deinit_2_0(soc);
	return QDF_STATUS_E_FAILURE;
}

static
QDF_STATUS dp_mon_soc_attach_2_0(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be =
		dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);
	int entries;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;

	soc_cfg_ctx = soc->wlan_cfg_ctx;
	if (!mon_soc_be) {
		dp_mon_err("DP MON SOC is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	entries = wlan_cfg_get_dp_soc_rx_mon_buf_ring_size(soc_cfg_ctx);
	qdf_print("%s:%d rx mon buf entries: %d", __func__, __LINE__, entries);
	if (dp_srng_alloc(soc, &soc->rxdma_mon_buf_ring[0],
			  RXDMA_MONITOR_BUF, entries, 0)) {
		dp_mon_err("%pK: " RNG_ERR "rx_mon_buf_ring", soc);
		goto fail;
	}

	entries = wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc_cfg_ctx);
	qdf_print("%s:%d tx mon buf entries: %d", __func__, __LINE__, entries);
	if (dp_srng_alloc(soc, &mon_soc_be->tx_mon_buf_ring,
			  TX_MONITOR_BUF, entries, 0)) {
		dp_mon_err("%pK: " RNG_ERR "tx_mon_buf_ring", soc);
		goto fail;
	}

	/* allocate sw desc pool */
	if (dp_rx_mon_buf_desc_pool_alloc(soc)) {
		dp_mon_err("%pK: Rx mon desc pool allocation failed", soc);
		goto fail;
	}

	if (dp_tx_mon_buf_desc_pool_alloc(soc)) {
		dp_mon_err("%pK: Tx mon desc pool allocation failed", soc);
		goto fail;
	}

	return QDF_STATUS_SUCCESS;
fail:
	dp_mon_soc_detach_2_0(soc);
	return QDF_STATUS_E_NOMEM;
}

static
void dp_pdev_mon_rings_deinit_2_0(struct dp_pdev *pdev)
{
	int mac_id = 0;
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	for (mac_id = 0; mac_id < DP_NUM_MACS_PER_PDEV; mac_id++) {
		int lmac_id = dp_get_lmac_id_for_pdev_id(soc, mac_id,
							 pdev->pdev_id);

		dp_srng_deinit(soc, &soc->rxdma_mon_dst_ring[lmac_id],
			       RXDMA_MONITOR_DST, pdev->pdev_id);
		dp_srng_deinit(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id],
			       TX_MONITOR_DST, pdev->pdev_id);
	}
}

static
QDF_STATUS dp_pdev_mon_rings_init_2_0(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	int mac_id = 0;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	for (mac_id = 0; mac_id < DP_NUM_MACS_PER_PDEV; mac_id++) {
		int lmac_id = dp_get_lmac_id_for_pdev_id(soc, mac_id,
							 pdev->pdev_id);

		if (dp_srng_init(soc, &soc->rxdma_mon_dst_ring[lmac_id],
				 RXDMA_MONITOR_DST, pdev->pdev_id, lmac_id)) {
			dp_mon_err("%pK: " RNG_ERR "rxdma_mon_dst_ring", soc);
			goto fail;
		}

		if (dp_srng_init(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id],
				 TX_MONITOR_DST, pdev->pdev_id, lmac_id)) {
			dp_mon_err("%pK: " RNG_ERR "tx_mon_dst_ring", soc);
			goto fail;
		}
	}
	return QDF_STATUS_SUCCESS;

fail:
	dp_pdev_mon_rings_deinit_2_0(pdev);
	return QDF_STATUS_E_NOMEM;
}

static
void dp_pdev_mon_rings_free_2_0(struct dp_pdev *pdev)
{
	int mac_id = 0;
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	for (mac_id = 0; mac_id < DP_NUM_MACS_PER_PDEV; mac_id++) {
		int lmac_id = dp_get_lmac_id_for_pdev_id(soc, mac_id,
							 pdev->pdev_id);

		dp_srng_free(soc, &soc->rxdma_mon_dst_ring[lmac_id]);
		dp_srng_free(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id]);
	}
}

static
QDF_STATUS dp_pdev_mon_rings_alloc_2_0(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	int mac_id = 0;
	int entries;
	struct wlan_cfg_dp_pdev_ctxt *pdev_cfg_ctx;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_soc_be *mon_soc_be = dp_get_be_mon_soc_from_dp_mon_soc(mon_soc);

	pdev_cfg_ctx = pdev->wlan_cfg_ctx;

	for (mac_id = 0; mac_id < DP_NUM_MACS_PER_PDEV; mac_id++) {
		int lmac_id =
		dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev->pdev_id);

		entries = wlan_cfg_get_dma_rx_mon_dest_ring_size(pdev_cfg_ctx);
		if (dp_srng_alloc(soc, &soc->rxdma_mon_dst_ring[lmac_id],
				  RXDMA_MONITOR_DST, entries, 0)) {
			dp_err("%pK: " RNG_ERR "rxdma_mon_dst_ring", pdev);
			goto fail;
		}

		entries = wlan_cfg_get_dma_tx_mon_dest_ring_size(pdev_cfg_ctx);
		if (dp_srng_alloc(soc, &mon_soc_be->tx_mon_dst_ring[lmac_id],
				  TX_MONITOR_DST, entries, 0)) {
			dp_err("%pK: " RNG_ERR "tx_mon_dst_ring", pdev);
			goto fail;
		}
	}
	return QDF_STATUS_SUCCESS;

fail:
	dp_pdev_mon_rings_free_2_0(pdev);
	return QDF_STATUS_E_NOMEM;
}

static
void dp_mon_pdev_free_2_0(struct dp_pdev *pdev)
{
}

static
QDF_STATUS dp_mon_pdev_alloc_2_0(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_pdev_be *mon_pdev_be =
			dp_get_be_mon_pdev_from_dp_mon_pdev(mon_pdev);

	if (!mon_pdev_be) {
		dp_mon_err("DP MON PDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#else
static inline
QDF_STATUS dp_mon_htt_srng_setup_2_0(struct dp_soc *soc,
				     struct dp_pdev *pdev,
				     int mac_id,
				     int mac_for_pdev)
{
	return QDF_STATUS_SUCCESS;
}

static uint32_t
dp_tx_mon_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
		      uint32_t mac_id, uint32_t quota)
{
	return 0;
}

static void
dp_tx_mon_print_ring_stat_2_0(struct dp_pdev *pdev)
{
}

static inline
QDF_STATUS dp_mon_soc_attach_2_0(struct dp_soc *soc)
{
	return status;
}

static inline
QDF_STATUS dp_mon_soc_detach_2_0(struct dp_soc *soc)
{
	return status;
}

static inline
void dp_pdev_mon_rings_deinit_2_0(struct dp_pdev *pdev)
{
}

static inline
QDF_STATUS dp_pdev_mon_rings_init_2_0(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_pdev_mon_rings_free_2_0(struct dp_pdev *pdev)
{
}

static inline
QDF_STATUS dp_pdev_mon_rings_alloc_2_0(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_mon_pdev_free_2_0(struct dp_pdev *pdev)
{
}

static inline
QDF_STATUS dp_mon_pdev_alloc_2_0(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_vdev_set_monitor_mode_buf_rings_2_0(struct dp_pdev *pdev)
{
}

static inline
QDF_STATUS dp_vdev_set_monitor_mode_rings_2_0(struct dp_pdev *pdev,
					      uint8_t delayed_replenish)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(WDI_EVENT_ENABLE) &&\
	(defined(QCA_ENHANCED_STATS_SUPPORT) || !defined(REMOVE_PKT_LOG))
static inline
void dp_mon_ppdu_stats_handler_register(struct dp_mon_soc *mon_soc)
{
	mon_soc->mon_ops->mon_ppdu_stats_ind_handler =
					dp_ppdu_stats_ind_handler;
}
#else
static inline
void dp_mon_ppdu_stats_handler_register(struct dp_mon_soc *mon_soc)
{
}
#endif

static void dp_mon_register_intr_ops_2_0(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	mon_soc->mon_ops->rx_mon_refill_buf_ring =
			dp_rx_mon_refill_buf_ring_2_0,
	mon_soc->mon_ops->tx_mon_refill_buf_ring =
			NULL,
	mon_soc->mon_rx_process = dp_rx_mon_process_2_0;
	dp_mon_ppdu_stats_handler_register(mon_soc);
}

/**
 * dp_mon_register_feature_ops_2_0() - register feature ops
 *
 * @soc: dp soc context
 *
 * @return: void
 */
static void
dp_mon_register_feature_ops_2_0(struct dp_soc *soc)
{
	struct dp_mon_ops *mon_ops = dp_mon_ops_get(soc);

	if (!mon_ops) {
		dp_err("mon_ops is NULL, feature ops registration failed");
		return;
	}

	mon_ops->mon_config_debug_sniffer = dp_config_debug_sniffer;
	mon_ops->mon_peer_tx_init = NULL;
	mon_ops->mon_peer_tx_cleanup = NULL;
	mon_ops->mon_htt_ppdu_stats_attach = dp_htt_ppdu_stats_attach;
	mon_ops->mon_htt_ppdu_stats_detach = dp_htt_ppdu_stats_detach;
	mon_ops->mon_print_pdev_rx_mon_stats = dp_print_pdev_rx_mon_stats;
	mon_ops->mon_set_bsscolor = dp_mon_set_bsscolor;
	mon_ops->mon_pdev_get_filter_ucast_data =
					dp_lite_mon_get_filter_ucast_data;
	mon_ops->mon_pdev_get_filter_mcast_data =
					dp_lite_mon_get_filter_mcast_data;
	mon_ops->mon_pdev_get_filter_non_data =
					dp_lite_mon_get_filter_non_data;
	mon_ops->mon_neighbour_peer_add_ast = NULL;
#ifndef DISABLE_MON_CONFIG
	mon_ops->mon_tx_process = dp_tx_mon_process_2_0;
	mon_ops->print_txmon_ring_stat = dp_tx_mon_print_ring_stat_2_0;
#endif
#ifdef WLAN_TX_PKT_CAPTURE_ENH_BE
	mon_ops->mon_peer_tid_peer_id_update = NULL;
	mon_ops->mon_tx_capture_debugfs_init = NULL;
	mon_ops->mon_tx_add_to_comp_queue = NULL;
	mon_ops->mon_print_pdev_tx_capture_stats =
					dp_print_pdev_tx_monitor_stats_2_0;
	mon_ops->mon_config_enh_tx_capture = dp_config_enh_tx_monitor_2_0;
	mon_ops->mon_tx_peer_filter = dp_peer_set_tx_capture_enabled_2_0;
#endif
#if (defined(WIFI_MONITOR_SUPPORT) && !defined(WLAN_TX_PKT_CAPTURE_ENH_BE))
	mon_ops->mon_peer_tid_peer_id_update = NULL;
	mon_ops->mon_tx_capture_debugfs_init = NULL;
	mon_ops->mon_tx_add_to_comp_queue = NULL;
	mon_ops->mon_print_pdev_tx_capture_stats = NULL;
	mon_ops->mon_config_enh_tx_capture = dp_config_enh_tx_core_monitor_2_0;
	mon_ops->mon_tx_peer_filter = NULL;
#endif
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	mon_ops->mon_config_enh_rx_capture = NULL;
#endif
#ifdef QCA_SUPPORT_BPR
	mon_ops->mon_set_bpr_enable = dp_set_bpr_enable_2_0;
#endif
#ifdef ATH_SUPPORT_NAC
	mon_ops->mon_set_filter_neigh_peers = NULL;
#endif
#ifdef WLAN_ATF_ENABLE
	mon_ops->mon_set_atf_stats_enable = dp_set_atf_stats_enable;
#endif
#ifdef FEATURE_NAC_RSSI
	mon_ops->mon_filter_neighbour_peer = NULL;
#endif
#ifdef QCA_MCOPY_SUPPORT
	mon_ops->mon_filter_setup_mcopy_mode = NULL;
	mon_ops->mon_filter_reset_mcopy_mode = NULL;
	mon_ops->mon_mcopy_check_deliver = NULL;
#endif
#ifdef QCA_ENHANCED_STATS_SUPPORT
	mon_ops->mon_filter_setup_enhanced_stats =
				dp_mon_filter_setup_enhanced_stats_2_0;
	mon_ops->mon_filter_reset_enhanced_stats =
				dp_mon_filter_reset_enhanced_stats_2_0;
	mon_ops->mon_tx_enable_enhanced_stats =
				dp_mon_tx_enable_enhanced_stats_2_0;
	mon_ops->mon_tx_disable_enhanced_stats =
				dp_mon_tx_disable_enhanced_stats_2_0;
	mon_ops->mon_ppdu_stats_feat_enable_check =
				dp_ppdu_stats_feat_enable_check_2_0;
	mon_ops->mon_tx_stats_update = dp_mon_tx_stats_update_2_0;
	mon_ops->mon_ppdu_desc_deliver = dp_ppdu_desc_deliver;
#ifdef WDI_EVENT_ENABLE
	mon_ops->mon_ppdu_desc_notify = dp_ppdu_desc_notify_2_0;
#endif
#endif
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	mon_ops->mon_filter_setup_rx_enh_capture = NULL;
#endif
#ifdef WDI_EVENT_ENABLE
	mon_ops->mon_set_pktlog_wifi3 = dp_set_pktlog_wifi3;
	mon_ops->mon_filter_setup_rx_pkt_log_full =
				dp_mon_filter_setup_rx_pkt_log_full_2_0;
	mon_ops->mon_filter_reset_rx_pkt_log_full =
				dp_mon_filter_reset_rx_pkt_log_full_2_0;
	mon_ops->mon_filter_setup_rx_pkt_log_lite =
				dp_mon_filter_setup_rx_pkt_log_lite_2_0;
	mon_ops->mon_filter_reset_rx_pkt_log_lite =
				dp_mon_filter_reset_rx_pkt_log_lite_2_0;
	mon_ops->mon_filter_setup_rx_pkt_log_cbf =
				dp_mon_filter_setup_rx_pkt_log_cbf_2_0;
	mon_ops->mon_filter_reset_rx_pkt_log_cbf =
				dp_mon_filter_reset_rx_pktlog_cbf_2_0;
	mon_ops->mon_filter_setup_pktlog_hybrid =
				dp_mon_filter_setup_pktlog_hybrid_2_0;
	mon_ops->mon_filter_reset_pktlog_hybrid =
				dp_mon_filter_reset_pktlog_hybrid_2_0;
#endif
#if defined(DP_CON_MON) && !defined(REMOVE_PKT_LOG)
	mon_ops->mon_pktlogmod_exit = dp_pktlogmod_exit;
#endif
	mon_ops->rx_hdr_length_set = dp_rx_mon_hdr_length_set;
	mon_ops->rx_packet_length_set = dp_rx_mon_packet_length_set;
	mon_ops->rx_mon_enable = dp_rx_mon_enable_set;
	mon_ops->rx_wmask_subscribe = dp_rx_mon_word_mask_subscribe;
	mon_ops->rx_enable_mpdu_logging = dp_rx_mon_enable_mpdu_logging;
	mon_ops->mon_neighbour_peers_detach = NULL;
	mon_ops->mon_vdev_set_monitor_mode_buf_rings =
				dp_vdev_set_monitor_mode_buf_rings_2_0;
	mon_ops->mon_vdev_set_monitor_mode_rings =
				dp_vdev_set_monitor_mode_rings_2_0;
#ifdef QCA_ENHANCED_STATS_SUPPORT
	mon_ops->mon_rx_stats_update = dp_rx_mon_stats_update_2_0;
	mon_ops->mon_rx_populate_ppdu_usr_info =
			dp_rx_mon_populate_ppdu_usr_info_2_0;
	mon_ops->mon_rx_populate_ppdu_info = dp_rx_mon_populate_ppdu_info_2_0;
#endif
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	mon_ops->mon_config_undecoded_metadata_capture =
		dp_mon_config_undecoded_metadata_capture;
	mon_ops->mon_filter_setup_undecoded_metadata_capture =
		dp_mon_filter_setup_undecoded_metadata_capture_2_0;
	mon_ops->mon_filter_reset_undecoded_metadata_capture =
		dp_mon_filter_reset_undecoded_metadata_capture_2_0;
#endif
	mon_ops->rx_enable_fpmo = dp_rx_mon_enable_fpmo;
	mon_ops->mon_rx_print_advanced_stats =
		dp_mon_rx_print_advanced_stats_2_0;
	mon_ops->mon_mac_filter_set = NULL;
}

struct dp_mon_ops monitor_ops_2_0 = {
	.mon_soc_cfg_init = dp_mon_soc_cfg_init,
	.mon_soc_attach = dp_mon_soc_attach_2_0,
	.mon_soc_detach = dp_mon_soc_detach_2_0,
	.mon_soc_init = dp_mon_soc_init_2_0,
	.mon_soc_deinit = dp_mon_soc_deinit_2_0,
	.mon_pdev_alloc = dp_mon_pdev_alloc_2_0,
	.mon_pdev_free = dp_mon_pdev_free_2_0,
	.mon_pdev_attach = dp_mon_pdev_attach,
	.mon_pdev_detach = dp_mon_pdev_detach,
	.mon_pdev_init = dp_mon_pdev_init,
	.mon_pdev_deinit = dp_mon_pdev_deinit,
	.mon_vdev_attach = dp_mon_vdev_attach,
	.mon_vdev_detach = dp_mon_vdev_detach,
	.mon_peer_attach = dp_mon_peer_attach,
	.mon_peer_detach = dp_mon_peer_detach,
	.mon_peer_get_peerstats_ctx = dp_mon_peer_get_peerstats_ctx,
	.mon_peer_reset_stats = dp_mon_peer_reset_stats,
	.mon_peer_get_stats = dp_mon_peer_get_stats,
	.mon_invalid_peer_update_pdev_stats =
				dp_mon_invalid_peer_update_pdev_stats,
	.mon_peer_get_stats_param = dp_mon_peer_get_stats_param,
	.mon_flush_rings = NULL,
#if !defined(DISABLE_MON_CONFIG)
	.mon_pdev_htt_srng_setup = dp_mon_pdev_htt_srng_setup_2_0,
	.mon_soc_htt_srng_setup = dp_mon_soc_htt_srng_setup_2_0,
#endif
#if defined(DP_CON_MON)
	.mon_service_rings = NULL,
#endif
#ifndef DISABLE_MON_CONFIG
	.mon_rx_process = NULL,
#endif
#if !defined(DISABLE_MON_CONFIG) && defined(MON_ENABLE_DROP_FOR_MAC)
	.mon_drop_packets_for_mac = NULL,
#endif
	.mon_vdev_timer_init = NULL,
	.mon_vdev_timer_start = NULL,
	.mon_vdev_timer_stop = NULL,
	.mon_vdev_timer_deinit = NULL,
	.mon_reap_timer_init = NULL,
	.mon_reap_timer_start = NULL,
	.mon_reap_timer_stop = NULL,
	.mon_reap_timer_deinit = NULL,
	.mon_filter_setup_rx_mon_mode = dp_mon_filter_setup_rx_mon_mode_2_0,
	.mon_filter_reset_rx_mon_mode = dp_mon_filter_reset_rx_mon_mode_2_0,
	.mon_filter_setup_tx_mon_mode = dp_mon_filter_setup_tx_mon_mode_2_0,
	.mon_filter_reset_tx_mon_mode = dp_mon_filter_reset_tx_mon_mode_2_0,
	.tx_mon_filter_update = dp_tx_mon_filter_update_2_0,
	.rx_mon_filter_update = dp_rx_mon_filter_update_2_0,
	.set_mon_mode_buf_rings_tx = dp_vdev_set_monitor_mode_buf_rings_tx_2_0,
	.tx_mon_filter_alloc = dp_mon_filter_alloc_2_0,
	.tx_mon_filter_dealloc = dp_mon_filter_dealloc_2_0,
	.mon_rings_alloc = dp_pdev_mon_rings_alloc_2_0,
	.mon_rings_free = dp_pdev_mon_rings_free_2_0,
	.mon_rings_init = dp_pdev_mon_rings_init_2_0,
	.mon_rings_deinit = dp_pdev_mon_rings_deinit_2_0,
	.rx_mon_desc_pool_init = NULL,
	.rx_mon_desc_pool_deinit = NULL,
	.rx_mon_desc_pool_alloc = NULL,
	.rx_mon_desc_pool_free = NULL,
	.rx_mon_buffers_alloc = NULL,
	.rx_mon_buffers_free = NULL,
	.tx_mon_desc_pool_init = NULL,
	.tx_mon_desc_pool_deinit = NULL,
	.tx_mon_desc_pool_alloc = NULL,
	.tx_mon_desc_pool_free = NULL,
#ifndef DISABLE_MON_CONFIG
	.mon_register_intr_ops = dp_mon_register_intr_ops_2_0,
#endif
	.mon_register_feature_ops = dp_mon_register_feature_ops_2_0,
#ifdef WLAN_TX_PKT_CAPTURE_ENH_BE
	.mon_tx_ppdu_stats_attach = dp_tx_ppdu_stats_attach_2_0,
	.mon_tx_ppdu_stats_detach = dp_tx_ppdu_stats_detach_2_0,
	.mon_peer_tx_capture_filter_check = NULL,
#endif
#if (defined(WIFI_MONITOR_SUPPORT) && !defined(WLAN_TX_PKT_CAPTURE_ENH_BE))
	.mon_tx_ppdu_stats_attach = NULL,
	.mon_tx_ppdu_stats_detach = NULL,
	.mon_peer_tx_capture_filter_check = NULL,
#endif
	.mon_pdev_ext_init = dp_mon_pdev_ext_init_2_0,
	.mon_pdev_ext_deinit = dp_mon_pdev_ext_deinit_2_0,
	.mon_lite_mon_alloc = dp_lite_mon_alloc,
	.mon_lite_mon_dealloc = dp_lite_mon_dealloc,
	.mon_lite_mon_vdev_delete = dp_lite_mon_vdev_delete,
	.mon_lite_mon_disable_rx = dp_lite_mon_disable_rx,
	.mon_lite_mon_is_rx_adv_filter_enable = dp_lite_mon_is_rx_adv_filter_enable,
	.mon_rx_ppdu_info_cache_create = dp_rx_mon_ppdu_info_cache_create,
	.mon_rx_ppdu_info_cache_destroy = dp_rx_mon_ppdu_info_cache_destroy,
};

struct cdp_mon_ops dp_ops_mon_2_0 = {
	.txrx_reset_monitor_mode = dp_reset_monitor_mode,
	/* Added support for HK advance filter */
	.txrx_set_advance_monitor_filter = NULL,
	.txrx_deliver_tx_mgmt = dp_deliver_tx_mgmt,
	.config_full_mon_mode = NULL,
	.soc_config_full_mon_mode = NULL,
	.get_mon_pdev_rx_stats = dp_pdev_get_rx_mon_stats,
	.txrx_enable_mon_reap_timer = NULL,
#ifdef QCA_SUPPORT_LITE_MONITOR
	.txrx_set_lite_mon_config = dp_lite_mon_set_config,
	.txrx_get_lite_mon_config = dp_lite_mon_get_config,
	.txrx_set_lite_mon_peer_config = dp_lite_mon_set_peer_config,
	.txrx_get_lite_mon_peer_config = dp_lite_mon_get_peer_config,
	.txrx_is_lite_mon_enabled = dp_lite_mon_is_enabled,
	.txrx_get_lite_mon_legacy_feature_enabled =
				dp_lite_mon_get_legacy_feature_enabled,
#endif
	.txrx_set_mon_pdev_params_rssi_dbm_conv =
				dp_mon_pdev_params_rssi_dbm_conv,
};

#ifdef QCA_MONITOR_OPS_PER_SOC_SUPPORT
void dp_mon_ops_register_2_0(struct dp_mon_soc *mon_soc)
{
	struct dp_mon_ops *mon_ops = NULL;

	if (mon_soc->mon_ops) {
		dp_mon_err("monitor ops is allocated");
		return;
	}

	mon_ops = qdf_mem_malloc(sizeof(struct dp_mon_ops));
	if (!mon_ops) {
		dp_mon_err("Failed to allocate memory for mon ops");
		return;
	}

	qdf_mem_copy(mon_ops, &monitor_ops_2_0, sizeof(struct dp_mon_ops));
	mon_soc->mon_ops = mon_ops;
}

void dp_mon_cdp_ops_register_2_0(struct cdp_ops *ops)
{
	struct cdp_mon_ops *mon_ops = NULL;

	if (ops->mon_ops) {
		dp_mon_err("cdp monitor ops is allocated");
		return;
	}

	mon_ops = qdf_mem_malloc(sizeof(struct cdp_mon_ops));
	if (!mon_ops) {
		dp_mon_err("Failed to allocate memory for mon ops");
		return;
	}

	qdf_mem_copy(mon_ops, &dp_ops_mon_2_0, sizeof(struct cdp_mon_ops));
	ops->mon_ops = mon_ops;
}
#else
void dp_mon_ops_register_2_0(struct dp_mon_soc *mon_soc)
{
	mon_soc->mon_ops = &monitor_ops_2_0;
}

void dp_mon_cdp_ops_register_2_0(struct cdp_ops *ops)
{
	ops->mon_ops = &dp_ops_mon_2_0;
}
#endif

#if defined(WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG) ||\
	defined(WLAN_SUPPORT_RX_FLOW_TAG)
/** dp_mon_rx_update_rx_protocol_tag_stats() - Update mon protocols's
 *					      statistics
 * @pdev: pdev handle
 * @protocol_index: Protocol index for which the stats should be incremented
 * @ring_index: REO ring number from which this tag was received.
 *
 * Return: void
 */
void dp_mon_rx_update_rx_protocol_tag_stats(struct dp_pdev *pdev,
					    uint16_t protocol_index)
{
	pdev->mon_proto_tag_stats[protocol_index].tag_ctr++;
}
#else
void dp_mon_rx_update_rx_protocol_tag_stats(struct dp_pdev *pdev,
					    uint16_t protocol_index)
{
}
#endif
