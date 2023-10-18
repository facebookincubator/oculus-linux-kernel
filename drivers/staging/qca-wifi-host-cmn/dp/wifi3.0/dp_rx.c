/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "hal_hw_headers.h"
#include "dp_types.h"
#include "dp_rx.h"
#include "dp_tx.h"
#include "dp_peer.h"
#include "hal_rx.h"
#include "hal_api.h"
#include "qdf_nbuf.h"
#ifdef MESH_MODE_SUPPORT
#include "if_meta_hdr.h"
#endif
#include "dp_internal.h"
#include "dp_ipa.h"
#include "dp_hist.h"
#include "dp_rx_buffer_pool.h"
#ifdef WIFI_MONITOR_SUPPORT
#include "dp_htt.h"
#include <dp_mon.h>
#endif
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif
#ifdef DP_RATETABLE_SUPPORT
#include "dp_ratetable.h"
#endif

#ifdef DUP_RX_DESC_WAR
void dp_rx_dump_info_and_assert(struct dp_soc *soc,
				hal_ring_handle_t hal_ring,
				hal_ring_desc_t ring_desc,
				struct dp_rx_desc *rx_desc)
{
	void *hal_soc = soc->hal_soc;

	hal_srng_dump_ring_desc(hal_soc, hal_ring, ring_desc);
	dp_rx_desc_dump(rx_desc);
}
#else
void dp_rx_dump_info_and_assert(struct dp_soc *soc,
				hal_ring_handle_t hal_ring_hdl,
				hal_ring_desc_t ring_desc,
				struct dp_rx_desc *rx_desc)
{
	hal_soc_handle_t hal_soc = soc->hal_soc;

	dp_rx_desc_dump(rx_desc);
	hal_srng_dump_ring_desc(hal_soc, hal_ring_hdl, ring_desc);
	hal_srng_dump_ring(hal_soc, hal_ring_hdl);
	qdf_assert_always(0);
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef RX_DESC_SANITY_WAR
QDF_STATUS dp_rx_desc_sanity(struct dp_soc *soc, hal_soc_handle_t hal_soc,
			     hal_ring_handle_t hal_ring_hdl,
			     hal_ring_desc_t ring_desc,
			     struct dp_rx_desc *rx_desc)
{
	uint8_t return_buffer_manager;

	if (qdf_unlikely(!rx_desc)) {
		/*
		 * This is an unlikely case where the cookie obtained
		 * from the ring_desc is invalid and hence we are not
		 * able to find the corresponding rx_desc
		 */
		goto fail;
	}

	return_buffer_manager = hal_rx_ret_buf_manager_get(hal_soc, ring_desc);
	if (qdf_unlikely(!(return_buffer_manager ==
				HAL_RX_BUF_RBM_SW1_BM(soc->wbm_sw0_bm_id) ||
			 return_buffer_manager ==
				HAL_RX_BUF_RBM_SW3_BM(soc->wbm_sw0_bm_id)))) {
		goto fail;
	}

	return QDF_STATUS_SUCCESS;

fail:
	DP_STATS_INC(soc, rx.err.invalid_cookie, 1);
	dp_err("Ring Desc:");
	hal_srng_dump_ring_desc(hal_soc, hal_ring_hdl,
				ring_desc);
	return QDF_STATUS_E_NULL_VALUE;

}
#endif
#endif /* QCA_HOST_MODE_WIFI_DISABLED */

/**
 * dp_pdev_frag_alloc_and_map() - Allocate frag for desc buffer and map
 *
 * @dp_soc: struct dp_soc *
 * @nbuf_frag_info_t: nbuf frag info
 * @dp_pdev: struct dp_pdev *
 * @rx_desc_pool: Rx desc pool
 *
 * Return: QDF_STATUS
 */
#ifdef DP_RX_MON_MEM_FRAG
static inline QDF_STATUS
dp_pdev_frag_alloc_and_map(struct dp_soc *dp_soc,
			   struct dp_rx_nbuf_frag_info *nbuf_frag_info_t,
			   struct dp_pdev *dp_pdev,
			   struct rx_desc_pool *rx_desc_pool)
{
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	(nbuf_frag_info_t->virt_addr).vaddr =
			qdf_frag_alloc(NULL, rx_desc_pool->buf_size);

	if (!((nbuf_frag_info_t->virt_addr).vaddr)) {
		dp_err("Frag alloc failed");
		DP_STATS_INC(dp_pdev, replenish.frag_alloc_fail, 1);
		return QDF_STATUS_E_NOMEM;
	}

	ret = qdf_mem_map_page(dp_soc->osdev,
			       (nbuf_frag_info_t->virt_addr).vaddr,
			       QDF_DMA_FROM_DEVICE,
			       rx_desc_pool->buf_size,
			       &nbuf_frag_info_t->paddr);

	if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
		qdf_frag_free((nbuf_frag_info_t->virt_addr).vaddr);
		dp_err("Frag map failed");
		DP_STATS_INC(dp_pdev, replenish.map_err, 1);
		return QDF_STATUS_E_FAULT;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_pdev_frag_alloc_and_map(struct dp_soc *dp_soc,
			   struct dp_rx_nbuf_frag_info *nbuf_frag_info_t,
			   struct dp_pdev *dp_pdev,
			   struct rx_desc_pool *rx_desc_pool)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* DP_RX_MON_MEM_FRAG */

#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
/**
 * dp_rx_refill_ring_record_entry() - Record an entry into refill_ring history
 * @soc: Datapath soc structure
 * @ring_num: Refill ring number
 * @hal_ring_hdl:
 * @num_req: number of buffers requested for refill
 * @num_refill: number of buffers refilled
 *
 * Return: None
 */
static inline void
dp_rx_refill_ring_record_entry(struct dp_soc *soc, uint8_t ring_num,
			       hal_ring_handle_t hal_ring_hdl,
			       uint32_t num_req, uint32_t num_refill)
{
	struct dp_refill_info_record *record;
	uint32_t idx;
	uint32_t tp;
	uint32_t hp;

	if (qdf_unlikely(ring_num >= MAX_PDEV_CNT ||
			 !soc->rx_refill_ring_history[ring_num]))
		return;

	idx = dp_history_get_next_index(&soc->rx_refill_ring_history[ring_num]->index,
					DP_RX_REFILL_HIST_MAX);

	/* No NULL check needed for record since its an array */
	record = &soc->rx_refill_ring_history[ring_num]->entry[idx];

	hal_get_sw_hptp(soc->hal_soc, hal_ring_hdl, &tp, &hp);
	record->timestamp = qdf_get_log_timestamp();
	record->num_req = num_req;
	record->num_refill = num_refill;
	record->hp = hp;
	record->tp = tp;
}
#else
static inline void
dp_rx_refill_ring_record_entry(struct dp_soc *soc, uint8_t ring_num,
			       hal_ring_handle_t hal_ring_hdl,
			       uint32_t num_req, uint32_t num_refill)
{
}
#endif

/**
 * dp_pdev_nbuf_alloc_and_map_replenish() - Allocate nbuf for desc buffer and
 *                                          map
 * @dp_soc: struct dp_soc *
 * @mac_id: Mac id
 * @num_entries_avail: num_entries_avail
 * @nbuf_frag_info_t: nbuf frag info
 * @dp_pdev: struct dp_pdev *
 * @rx_desc_pool: Rx desc pool
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_pdev_nbuf_alloc_and_map_replenish(struct dp_soc *dp_soc,
				     uint32_t mac_id,
				     uint32_t num_entries_avail,
				     struct dp_rx_nbuf_frag_info *nbuf_frag_info_t,
				     struct dp_pdev *dp_pdev,
				     struct rx_desc_pool *rx_desc_pool)
{
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	(nbuf_frag_info_t->virt_addr).nbuf =
		dp_rx_buffer_pool_nbuf_alloc(dp_soc,
					     mac_id,
					     rx_desc_pool,
					     num_entries_avail);
	if (!((nbuf_frag_info_t->virt_addr).nbuf)) {
		dp_err("nbuf alloc failed");
		DP_STATS_INC(dp_pdev, replenish.nbuf_alloc_fail, 1);
		return QDF_STATUS_E_NOMEM;
	}

	ret = dp_rx_buffer_pool_nbuf_map(dp_soc, rx_desc_pool,
					 nbuf_frag_info_t);
	if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
		dp_rx_buffer_pool_nbuf_free(dp_soc,
			(nbuf_frag_info_t->virt_addr).nbuf, mac_id);
		dp_err("nbuf map failed");
		DP_STATS_INC(dp_pdev, replenish.map_err, 1);
		return QDF_STATUS_E_FAULT;
	}

	nbuf_frag_info_t->paddr =
		qdf_nbuf_get_frag_paddr((nbuf_frag_info_t->virt_addr).nbuf, 0);
		dp_ipa_handle_rx_buf_smmu_mapping(dp_soc, (qdf_nbuf_t)(
						  (nbuf_frag_info_t->virt_addr).nbuf),
						  rx_desc_pool->buf_size,
						  true, __func__, __LINE__);

	ret = dp_check_paddr(dp_soc, &((nbuf_frag_info_t->virt_addr).nbuf),
			     &nbuf_frag_info_t->paddr,
			     rx_desc_pool);
	if (ret == QDF_STATUS_E_FAILURE) {
		DP_STATS_INC(dp_pdev, replenish.x86_fail, 1);
		return QDF_STATUS_E_ADDRNOTAVAIL;
	}

	return QDF_STATUS_SUCCESS;
}

#if defined(QCA_DP_RX_NBUF_NO_MAP_UNMAP) && !defined(BUILD_X86)
QDF_STATUS
__dp_rx_buffers_no_map_lt_replenish(struct dp_soc *soc, uint32_t mac_id,
				    struct dp_srng *dp_rxdma_srng,
				    struct rx_desc_pool *rx_desc_pool)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	uint32_t count;
	void *rxdma_ring_entry;
	union dp_rx_desc_list_elem_t *next = NULL;
	void *rxdma_srng;
	qdf_nbuf_t nbuf;
	qdf_dma_addr_t paddr;
	uint16_t num_entries_avail = 0;
	uint16_t num_alloc_desc = 0;
	union dp_rx_desc_list_elem_t *desc_list = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;
	int sync_hw_ptr = 0;

	rxdma_srng = dp_rxdma_srng->hal_srng;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_err("%pK: pdev is null for mac_id = %d", soc, mac_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (qdf_unlikely(!rxdma_srng)) {
		dp_rx_debug("%pK: rxdma srng not initialized", soc);
		return QDF_STATUS_E_FAILURE;
	}

	hal_srng_access_start(soc->hal_soc, rxdma_srng);

	num_entries_avail = hal_srng_src_num_avail(soc->hal_soc,
						   rxdma_srng,
						   sync_hw_ptr);

	dp_rx_debug("%pK: no of available entries in rxdma ring: %d",
		    soc, num_entries_avail);

	if (qdf_unlikely(num_entries_avail <
			 ((dp_rxdma_srng->num_entries * 3) / 4))) {
		hal_srng_access_end(soc->hal_soc, rxdma_srng);
		return QDF_STATUS_E_FAILURE;
	}

	DP_STATS_INC(dp_pdev, replenish.low_thresh_intrs, 1);
	num_alloc_desc = dp_rx_get_free_desc_list(soc, mac_id,
						  rx_desc_pool,
						  num_entries_avail,
						  &desc_list,
						  &tail);

	if (!num_alloc_desc) {
		dp_rx_err("%pK: no free rx_descs in freelist", soc);
		DP_STATS_INC(dp_pdev, err.desc_lt_alloc_fail,
			     num_entries_avail);
		hal_srng_access_end(soc->hal_soc, rxdma_srng);
		return QDF_STATUS_E_NOMEM;
	}

	for (count = 0; count < num_alloc_desc; count++) {
		next = desc_list->next;
		qdf_prefetch(next);
		nbuf = dp_rx_nbuf_alloc(soc, rx_desc_pool);
		if (qdf_unlikely(!nbuf)) {
			DP_STATS_INC(dp_pdev, replenish.nbuf_alloc_fail, 1);
			break;
		}

		paddr = dp_rx_nbuf_sync_no_dsb(soc, nbuf,
					       rx_desc_pool->buf_size);

		rxdma_ring_entry = hal_srng_src_get_next(soc->hal_soc,
							 rxdma_srng);
		qdf_assert_always(rxdma_ring_entry);

		desc_list->rx_desc.nbuf = nbuf;
		desc_list->rx_desc.rx_buf_start = nbuf->data;
		desc_list->rx_desc.unmapped = 0;

		/* rx_desc.in_use should be zero at this time*/
		qdf_assert_always(desc_list->rx_desc.in_use == 0);

		desc_list->rx_desc.in_use = 1;
		desc_list->rx_desc.in_err_state = 0;

		hal_rxdma_buff_addr_info_set(soc->hal_soc, rxdma_ring_entry,
					     paddr,
					     desc_list->rx_desc.cookie,
					     rx_desc_pool->owner);

		desc_list = next;
	}
	qdf_dsb();
	hal_srng_access_end(soc->hal_soc, rxdma_srng);

	/* No need to count the number of bytes received during replenish.
	 * Therefore set replenish.pkts.bytes as 0.
	 */
	DP_STATS_INC_PKT(dp_pdev, replenish.pkts, count, 0);
	DP_STATS_INC(dp_pdev, buf_freelist, (num_alloc_desc - count));
	/*
	 * add any available free desc back to the free list
	 */
	if (desc_list)
		dp_rx_add_desc_list_to_free_list(soc, &desc_list, &tail,
						 mac_id, rx_desc_pool);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
__dp_rx_buffers_no_map_replenish(struct dp_soc *soc, uint32_t mac_id,
				 struct dp_srng *dp_rxdma_srng,
				 struct rx_desc_pool *rx_desc_pool,
				 uint32_t num_req_buffers,
				 union dp_rx_desc_list_elem_t **desc_list,
				 union dp_rx_desc_list_elem_t **tail)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	uint32_t count;
	void *rxdma_ring_entry;
	union dp_rx_desc_list_elem_t *next;
	void *rxdma_srng;
	qdf_nbuf_t nbuf;
	qdf_nbuf_t nbuf_next;
	qdf_nbuf_t nbuf_head = NULL;
	qdf_nbuf_t nbuf_tail = NULL;
	qdf_dma_addr_t paddr;

	rxdma_srng = dp_rxdma_srng->hal_srng;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_err("%pK: pdev is null for mac_id = %d",
			  soc, mac_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (qdf_unlikely(!rxdma_srng)) {
		dp_rx_debug("%pK: rxdma srng not initialized", soc);
		DP_STATS_INC(dp_pdev, replenish.rxdma_err, num_req_buffers);
		return QDF_STATUS_E_FAILURE;
	}

	/* Allocate required number of nbufs */
	for (count = 0; count < num_req_buffers; count++) {
		nbuf = dp_rx_nbuf_alloc(soc, rx_desc_pool);
		if (qdf_unlikely(!nbuf)) {
			DP_STATS_INC(dp_pdev, replenish.nbuf_alloc_fail, 1);
			/* Update num_req_buffers to nbufs allocated count */
			num_req_buffers = count;
			break;
		}

		paddr = dp_rx_nbuf_sync_no_dsb(soc, nbuf,
					       rx_desc_pool->buf_size);

		QDF_NBUF_CB_PADDR(nbuf) = paddr;
		DP_RX_LIST_APPEND(nbuf_head,
				  nbuf_tail,
				  nbuf);
	}
	qdf_dsb();

	nbuf = nbuf_head;
	hal_srng_access_start(soc->hal_soc, rxdma_srng);

	for (count = 0; count < num_req_buffers; count++) {
		next = (*desc_list)->next;
		nbuf_next = nbuf->next;
		qdf_prefetch(next);

		rxdma_ring_entry = (struct dp_buffer_addr_info *)
			hal_srng_src_get_next(soc->hal_soc, rxdma_srng);

		if (!rxdma_ring_entry)
			break;

		(*desc_list)->rx_desc.nbuf = nbuf;
		(*desc_list)->rx_desc.rx_buf_start = nbuf->data;
		(*desc_list)->rx_desc.unmapped = 0;

		/* rx_desc.in_use should be zero at this time*/
		qdf_assert_always((*desc_list)->rx_desc.in_use == 0);

		(*desc_list)->rx_desc.in_use = 1;
		(*desc_list)->rx_desc.in_err_state = 0;

		hal_rxdma_buff_addr_info_set(soc->hal_soc, rxdma_ring_entry,
					     QDF_NBUF_CB_PADDR(nbuf),
					     (*desc_list)->rx_desc.cookie,
					     rx_desc_pool->owner);

		*desc_list = next;
		nbuf = nbuf_next;
	}
	hal_srng_access_end(soc->hal_soc, rxdma_srng);

	/* No need to count the number of bytes received during replenish.
	 * Therefore set replenish.pkts.bytes as 0.
	 */
	DP_STATS_INC_PKT(dp_pdev, replenish.pkts, count, 0);
	DP_STATS_INC(dp_pdev, buf_freelist, (num_req_buffers - count));
	/*
	 * add any available free desc back to the free list
	 */
	if (*desc_list)
		dp_rx_add_desc_list_to_free_list(soc, desc_list, tail,
						 mac_id, rx_desc_pool);
	while (nbuf) {
		nbuf_next = nbuf->next;
		dp_rx_nbuf_unmap_pool(soc, rx_desc_pool, nbuf);
		qdf_nbuf_free(nbuf);
		nbuf = nbuf_next;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS __dp_pdev_rx_buffers_no_map_attach(struct dp_soc *soc,
					      uint32_t mac_id,
					      struct dp_srng *dp_rxdma_srng,
					      struct rx_desc_pool *rx_desc_pool,
					      uint32_t num_req_buffers)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	uint32_t count;
	uint32_t nr_descs = 0;
	void *rxdma_ring_entry;
	union dp_rx_desc_list_elem_t *next;
	void *rxdma_srng;
	qdf_nbuf_t nbuf;
	qdf_dma_addr_t paddr;
	union dp_rx_desc_list_elem_t *desc_list = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;

	rxdma_srng = dp_rxdma_srng->hal_srng;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_err("%pK: pdev is null for mac_id = %d",
			  soc, mac_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (qdf_unlikely(!rxdma_srng)) {
		dp_rx_debug("%pK: rxdma srng not initialized", soc);
		DP_STATS_INC(dp_pdev, replenish.rxdma_err, num_req_buffers);
		return QDF_STATUS_E_FAILURE;
	}

	dp_rx_debug("%pK: requested %d buffers for replenish",
		    soc, num_req_buffers);

	nr_descs = dp_rx_get_free_desc_list(soc, mac_id, rx_desc_pool,
					    num_req_buffers, &desc_list, &tail);
	if (!nr_descs) {
		dp_err("no free rx_descs in freelist");
		DP_STATS_INC(dp_pdev, err.desc_alloc_fail, num_req_buffers);
		return QDF_STATUS_E_NOMEM;
	}

	dp_debug("got %u RX descs for driver attach", nr_descs);

	hal_srng_access_start(soc->hal_soc, rxdma_srng);

	for (count = 0; count < nr_descs; count++) {
		next = desc_list->next;
		qdf_prefetch(next);
		nbuf = dp_rx_nbuf_alloc(soc, rx_desc_pool);
		if (qdf_unlikely(!nbuf)) {
			DP_STATS_INC(dp_pdev, replenish.nbuf_alloc_fail, 1);
			break;
		}

		paddr = dp_rx_nbuf_sync_no_dsb(soc, nbuf,
					       rx_desc_pool->buf_size);
		rxdma_ring_entry = (struct dp_buffer_addr_info *)
			hal_srng_src_get_next(soc->hal_soc, rxdma_srng);
		if (!rxdma_ring_entry)
			break;

		qdf_assert_always(rxdma_ring_entry);

		desc_list->rx_desc.nbuf = nbuf;
		desc_list->rx_desc.rx_buf_start = nbuf->data;
		desc_list->rx_desc.unmapped = 0;

		/* rx_desc.in_use should be zero at this time*/
		qdf_assert_always(desc_list->rx_desc.in_use == 0);

		desc_list->rx_desc.in_use = 1;
		desc_list->rx_desc.in_err_state = 0;

		hal_rxdma_buff_addr_info_set(soc->hal_soc, rxdma_ring_entry,
					     paddr,
					     desc_list->rx_desc.cookie,
					     rx_desc_pool->owner);

		desc_list = next;
	}
	qdf_dsb();
	hal_srng_access_end(soc->hal_soc, rxdma_srng);

	/* No need to count the number of bytes received during replenish.
	 * Therefore set replenish.pkts.bytes as 0.
	 */
	DP_STATS_INC_PKT(dp_pdev, replenish.pkts, count, 0);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef DP_UMAC_HW_RESET_SUPPORT
#if defined(QCA_DP_RX_NBUF_NO_MAP_UNMAP) && !defined(BUILD_X86)
static inline
qdf_dma_addr_t dp_rx_rep_retrieve_paddr(struct dp_soc *dp_soc, qdf_nbuf_t nbuf,
					uint32_t buf_size)
{
	return dp_rx_nbuf_sync_no_dsb(dp_soc, nbuf, buf_size);
}
#else
static inline
qdf_dma_addr_t dp_rx_rep_retrieve_paddr(struct dp_soc *dp_soc, qdf_nbuf_t nbuf,
					uint32_t buf_size)
{
	return qdf_nbuf_get_frag_paddr(nbuf, 0);
}
#endif

/**
 * dp_rx_desc_replenish() - Replenish the rx descriptors one at a time
 * @soc: core txrx main context
 * @dp_rxdma_srng: rxdma ring
 * @rx_desc_pool: rx descriptor pool
 * @rx_desc:rx descriptor
 *
 * Return: void
 */
static inline
void dp_rx_desc_replenish(struct dp_soc *soc, struct dp_srng *dp_rxdma_srng,
			  struct rx_desc_pool *rx_desc_pool,
			  struct dp_rx_desc *rx_desc)
{
	void *rxdma_srng;
	void *rxdma_ring_entry;
	qdf_dma_addr_t paddr;

	rxdma_srng = dp_rxdma_srng->hal_srng;

	/* No one else should be accessing the srng at this point */
	hal_srng_access_start_unlocked(soc->hal_soc, rxdma_srng);

	rxdma_ring_entry = hal_srng_src_get_next(soc->hal_soc, rxdma_srng);

	qdf_assert_always(rxdma_ring_entry);
	rx_desc->in_err_state = 0;

	paddr = dp_rx_rep_retrieve_paddr(soc, rx_desc->nbuf,
					 rx_desc_pool->buf_size);
	hal_rxdma_buff_addr_info_set(soc->hal_soc, rxdma_ring_entry, paddr,
				     rx_desc->cookie, rx_desc_pool->owner);

	hal_srng_access_end_unlocked(soc->hal_soc, rxdma_srng);
}

void dp_rx_desc_reuse(struct dp_soc *soc, qdf_nbuf_t *nbuf_list)
{
	int mac_id, i, j;
	union dp_rx_desc_list_elem_t *head = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;

	for (mac_id = 0; mac_id < MAX_PDEV_CNT; mac_id++) {
		struct dp_srng *dp_rxdma_srng =
					&soc->rx_refill_buf_ring[mac_id];
		struct rx_desc_pool *rx_desc_pool = &soc->rx_desc_buf[mac_id];
		uint32_t rx_sw_desc_num = rx_desc_pool->pool_size;
		/* Only fill up 1/3 of the ring size */
		uint32_t num_req_decs;

		if (!dp_rxdma_srng || !dp_rxdma_srng->hal_srng ||
		    !rx_desc_pool->array)
			continue;

		num_req_decs = dp_rxdma_srng->num_entries / 3;

		for (i = 0, j = 0; i < rx_sw_desc_num; i++) {
			struct dp_rx_desc *rx_desc =
				(struct dp_rx_desc *)&rx_desc_pool->array[i];

			if (rx_desc->in_use) {
				if (j < (dp_rxdma_srng->num_entries - 1)) {
					dp_rx_desc_replenish(soc, dp_rxdma_srng,
							     rx_desc_pool,
							     rx_desc);
				} else {
					dp_rx_nbuf_unmap(soc, rx_desc, 0);
					rx_desc->unmapped = 0;

					rx_desc->nbuf->next = *nbuf_list;
					*nbuf_list = rx_desc->nbuf;

					dp_rx_add_to_free_desc_list(&head,
								    &tail,
								    rx_desc);
				}
				j++;
			}
		}

		if (head)
			dp_rx_add_desc_list_to_free_list(soc, &head, &tail,
							 mac_id, rx_desc_pool);

		/* If num of descs in use were less, then we need to replenish
		 * the ring with some buffers
		 */
		head = NULL;
		tail = NULL;

		if (j < (num_req_decs - 1))
			dp_rx_buffers_replenish(soc, mac_id, dp_rxdma_srng,
						rx_desc_pool,
						((num_req_decs - 1) - j),
						&head, &tail, true);
	}
}
#endif

QDF_STATUS __dp_rx_buffers_replenish(struct dp_soc *dp_soc, uint32_t mac_id,
				struct dp_srng *dp_rxdma_srng,
				struct rx_desc_pool *rx_desc_pool,
				uint32_t num_req_buffers,
				union dp_rx_desc_list_elem_t **desc_list,
				union dp_rx_desc_list_elem_t **tail,
				bool req_only, const char *func_name)
{
	uint32_t num_alloc_desc;
	uint16_t num_desc_to_free = 0;
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(dp_soc, mac_id);
	uint32_t num_entries_avail;
	uint32_t count;
	uint32_t extra_buffers;
	int sync_hw_ptr = 1;
	struct dp_rx_nbuf_frag_info nbuf_frag_info = {0};
	void *rxdma_ring_entry;
	union dp_rx_desc_list_elem_t *next;
	QDF_STATUS ret;
	void *rxdma_srng;
	union dp_rx_desc_list_elem_t *desc_list_append = NULL;
	union dp_rx_desc_list_elem_t *tail_append = NULL;
	union dp_rx_desc_list_elem_t *temp_list = NULL;

	rxdma_srng = dp_rxdma_srng->hal_srng;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_err("%pK: pdev is null for mac_id = %d",
			  dp_soc, mac_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (qdf_unlikely(!rxdma_srng)) {
		dp_rx_debug("%pK: rxdma srng not initialized", dp_soc);
		DP_STATS_INC(dp_pdev, replenish.rxdma_err, num_req_buffers);
		return QDF_STATUS_E_FAILURE;
	}

	dp_verbose_debug("%pK: requested %d buffers for replenish",
			 dp_soc, num_req_buffers);

	hal_srng_access_start(dp_soc->hal_soc, rxdma_srng);

	num_entries_avail = hal_srng_src_num_avail(dp_soc->hal_soc,
						   rxdma_srng,
						   sync_hw_ptr);

	dp_verbose_debug("%pK: no of available entries in rxdma ring: %d",
			 dp_soc, num_entries_avail);

	if (!req_only && !(*desc_list) && (num_entries_avail >
		((dp_rxdma_srng->num_entries * 3) / 4))) {
		num_req_buffers = num_entries_avail;
		DP_STATS_INC(dp_pdev, replenish.low_thresh_intrs, 1);
	} else if (num_entries_avail < num_req_buffers) {
		num_desc_to_free = num_req_buffers - num_entries_avail;
		num_req_buffers = num_entries_avail;
	} else if ((*desc_list) &&
		   dp_rxdma_srng->num_entries - num_entries_avail <
		   CRITICAL_BUFFER_THRESHOLD) {
		/* set extra buffers to CRITICAL_BUFFER_THRESHOLD only if
		 * total buff requested after adding extra buffers is less
		 * than or equal to num entries available, else set it to max
		 * possible additional buffers available at that moment
		 */
		extra_buffers =
			((num_req_buffers + CRITICAL_BUFFER_THRESHOLD) > num_entries_avail) ?
			(num_entries_avail - num_req_buffers) :
			CRITICAL_BUFFER_THRESHOLD;
		/* Append some free descriptors to tail */
		num_alloc_desc =
			dp_rx_get_free_desc_list(dp_soc, mac_id,
						 rx_desc_pool,
						 extra_buffers,
						 &desc_list_append,
						 &tail_append);

		if (num_alloc_desc) {
			temp_list = *desc_list;
			*desc_list = desc_list_append;
			tail_append->next = temp_list;
			num_req_buffers += num_alloc_desc;

			DP_STATS_DEC(dp_pdev,
				     replenish.free_list,
				     num_alloc_desc);
		} else
			dp_err_rl("%pK:  no free rx_descs in freelist", dp_soc);
	}

	if (qdf_unlikely(!num_req_buffers)) {
		num_desc_to_free = num_req_buffers;
		hal_srng_access_end(dp_soc->hal_soc, rxdma_srng);
		goto free_descs;
	}

	/*
	 * if desc_list is NULL, allocate the descs from freelist
	 */
	if (!(*desc_list)) {
		num_alloc_desc = dp_rx_get_free_desc_list(dp_soc, mac_id,
							  rx_desc_pool,
							  num_req_buffers,
							  desc_list,
							  tail);

		if (!num_alloc_desc) {
			dp_rx_err("%pK: no free rx_descs in freelist", dp_soc);
			DP_STATS_INC(dp_pdev, err.desc_alloc_fail,
					num_req_buffers);
			hal_srng_access_end(dp_soc->hal_soc, rxdma_srng);
			return QDF_STATUS_E_NOMEM;
		}

		dp_verbose_debug("%pK: %d rx desc allocated", dp_soc,
				 num_alloc_desc);
		num_req_buffers = num_alloc_desc;
	}


	count = 0;

	while (count < num_req_buffers) {
		/* Flag is set while pdev rx_desc_pool initialization */
		if (qdf_unlikely(rx_desc_pool->rx_mon_dest_frag_enable))
			ret = dp_pdev_frag_alloc_and_map(dp_soc,
							 &nbuf_frag_info,
							 dp_pdev,
							 rx_desc_pool);
		else
			ret = dp_pdev_nbuf_alloc_and_map_replenish(dp_soc,
								   mac_id,
					num_entries_avail, &nbuf_frag_info,
					dp_pdev, rx_desc_pool);

		if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
			if (qdf_unlikely(ret  == QDF_STATUS_E_FAULT))
				continue;
			break;
		}

		count++;

		rxdma_ring_entry = hal_srng_src_get_next(dp_soc->hal_soc,
							 rxdma_srng);
		qdf_assert_always(rxdma_ring_entry);

		next = (*desc_list)->next;

		/* Flag is set while pdev rx_desc_pool initialization */
		if (qdf_unlikely(rx_desc_pool->rx_mon_dest_frag_enable))
			dp_rx_desc_frag_prep(&((*desc_list)->rx_desc),
					     &nbuf_frag_info);
		else
			dp_rx_desc_prep(&((*desc_list)->rx_desc),
					&nbuf_frag_info);

		/* rx_desc.in_use should be zero at this time*/
		qdf_assert_always((*desc_list)->rx_desc.in_use == 0);

		(*desc_list)->rx_desc.in_use = 1;
		(*desc_list)->rx_desc.in_err_state = 0;
		dp_rx_desc_update_dbg_info(&(*desc_list)->rx_desc,
					   func_name, RX_DESC_REPLENISHED);
		dp_verbose_debug("rx_netbuf=%pK, paddr=0x%llx, cookie=%d",
				 nbuf_frag_info.virt_addr.nbuf,
				 (unsigned long long)(nbuf_frag_info.paddr),
				 (*desc_list)->rx_desc.cookie);

		hal_rxdma_buff_addr_info_set(dp_soc->hal_soc, rxdma_ring_entry,
					     nbuf_frag_info.paddr,
						(*desc_list)->rx_desc.cookie,
						rx_desc_pool->owner);

		*desc_list = next;

	}

	dp_rx_refill_ring_record_entry(dp_soc, dp_pdev->lmac_id, rxdma_srng,
				       num_req_buffers, count);

	hal_srng_access_end(dp_soc->hal_soc, rxdma_srng);

	dp_rx_schedule_refill_thread(dp_soc);

	dp_verbose_debug("replenished buffers %d, rx desc added back to free list %u",
			 count, num_desc_to_free);

	/* No need to count the number of bytes received during replenish.
	 * Therefore set replenish.pkts.bytes as 0.
	 */
	DP_STATS_INC_PKT(dp_pdev, replenish.pkts, count, 0);
	DP_STATS_INC(dp_pdev, replenish.free_list, num_req_buffers - count);

free_descs:
	DP_STATS_INC(dp_pdev, buf_freelist, num_desc_to_free);
	/*
	 * add any available free desc back to the free list
	 */
	if (*desc_list)
		dp_rx_add_desc_list_to_free_list(dp_soc, desc_list, tail,
			mac_id, rx_desc_pool);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(__dp_rx_buffers_replenish);

void
dp_rx_deliver_raw(struct dp_vdev *vdev, qdf_nbuf_t nbuf_list,
		  struct dp_txrx_peer *txrx_peer)
{
	qdf_nbuf_t deliver_list_head = NULL;
	qdf_nbuf_t deliver_list_tail = NULL;
	qdf_nbuf_t nbuf;

	nbuf = nbuf_list;
	while (nbuf) {
		qdf_nbuf_t next = qdf_nbuf_next(nbuf);

		DP_RX_LIST_APPEND(deliver_list_head, deliver_list_tail, nbuf);

		DP_STATS_INC(vdev->pdev, rx_raw_pkts, 1);
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.raw, 1,
					      qdf_nbuf_len(nbuf));
		/*
		 * reset the chfrag_start and chfrag_end bits in nbuf cb
		 * as this is a non-amsdu pkt and RAW mode simulation expects
		 * these bit s to be 0 for non-amsdu pkt.
		 */
		if (qdf_nbuf_is_rx_chfrag_start(nbuf) &&
			 qdf_nbuf_is_rx_chfrag_end(nbuf)) {
			qdf_nbuf_set_rx_chfrag_start(nbuf, 0);
			qdf_nbuf_set_rx_chfrag_end(nbuf, 0);
		}

		nbuf = next;
	}

	vdev->osif_rsim_rx_decap(vdev->osif_vdev, &deliver_list_head,
				 &deliver_list_tail);

	vdev->osif_rx(vdev->osif_vdev, deliver_list_head);
}

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifndef FEATURE_WDS
void dp_rx_da_learn(struct dp_soc *soc, uint8_t *rx_tlv_hdr,
		    struct dp_txrx_peer *ta_peer, qdf_nbuf_t nbuf)
{
}
#endif

#ifdef QCA_SUPPORT_TX_MIN_RATES_FOR_SPECIAL_FRAMES
/**
 * dp_classify_critical_pkts() - API for marking critical packets
 * @soc: dp_soc context
 * @vdev: vdev on which packet is to be sent
 * @nbuf: nbuf that has to be classified
 *
 * The function parses the packet, identifies whether its a critical frame and
 * marks QDF_NBUF_CB_TX_EXTRA_IS_CRITICAL bit in qdf_nbuf_cb for the nbuf.
 * Code for marking which frames are CRITICAL is accessed via callback.
 * EAPOL, ARP, DHCP, DHCPv6, ICMPv6 NS/NA are the typical critical frames.
 *
 * Return: None
 */
static
void dp_classify_critical_pkts(struct dp_soc *soc, struct dp_vdev *vdev,
			       qdf_nbuf_t nbuf)
{
	if (vdev->tx_classify_critical_pkt_cb)
		vdev->tx_classify_critical_pkt_cb(vdev->osif_vdev, nbuf);
}
#else
static inline
void dp_classify_critical_pkts(struct dp_soc *soc, struct dp_vdev *vdev,
			       qdf_nbuf_t nbuf)
{
}
#endif

#ifdef QCA_OL_TX_MULTIQ_SUPPORT
static inline
void dp_rx_nbuf_queue_mapping_set(qdf_nbuf_t nbuf, uint8_t ring_id)
{
	qdf_nbuf_set_queue_mapping(nbuf, ring_id);
}
#else
static inline
void dp_rx_nbuf_queue_mapping_set(qdf_nbuf_t nbuf, uint8_t ring_id)
{
}
#endif

bool dp_rx_intrabss_mcbc_fwd(struct dp_soc *soc, struct dp_txrx_peer *ta_peer,
			     uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf,
			     struct cdp_tid_rx_stats *tid_stats)
{
	uint16_t len;
	qdf_nbuf_t nbuf_copy;

	if (dp_rx_intrabss_eapol_drop_check(soc, ta_peer, rx_tlv_hdr,
					    nbuf))
		return true;

	if (!dp_rx_check_ndi_mdns_fwding(ta_peer, nbuf))
		return false;

	/* If the source peer in the isolation list
	 * then dont forward instead push to bridge stack
	 */
	if (dp_get_peer_isolation(ta_peer))
		return false;

	nbuf_copy = qdf_nbuf_copy(nbuf);
	if (!nbuf_copy)
		return false;

	len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);

	qdf_mem_set(nbuf_copy->cb, 0x0, sizeof(nbuf_copy->cb));
	dp_classify_critical_pkts(soc, ta_peer->vdev, nbuf_copy);

	if (soc->arch_ops.dp_rx_intrabss_mcast_handler(soc, ta_peer,
						       nbuf_copy,
						       tid_stats))
		return false;

	/* Don't send packets if tx is paused */
	if (!soc->is_tx_pause &&
	    !dp_tx_send((struct cdp_soc_t *)soc,
			ta_peer->vdev->vdev_id, nbuf_copy)) {
		DP_PEER_PER_PKT_STATS_INC_PKT(ta_peer, rx.intra_bss.pkts, 1,
					      len);
		tid_stats->intrabss_cnt++;
	} else {
		DP_PEER_PER_PKT_STATS_INC_PKT(ta_peer, rx.intra_bss.fail, 1,
					      len);
		tid_stats->fail_cnt[INTRABSS_DROP]++;
		dp_rx_nbuf_free(nbuf_copy);
	}
	return false;
}

bool dp_rx_intrabss_ucast_fwd(struct dp_soc *soc, struct dp_txrx_peer *ta_peer,
			      uint8_t tx_vdev_id,
			      uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf,
			      struct cdp_tid_rx_stats *tid_stats)
{
	uint16_t len;

	len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);

	/* linearize the nbuf just before we send to
	 * dp_tx_send()
	 */
	if (qdf_unlikely(qdf_nbuf_is_frag(nbuf))) {
		if (qdf_nbuf_linearize(nbuf) == -ENOMEM)
			return false;

		nbuf = qdf_nbuf_unshare(nbuf);
		if (!nbuf) {
			DP_PEER_PER_PKT_STATS_INC_PKT(ta_peer,
						      rx.intra_bss.fail,
						      1, len);
			/* return true even though the pkt is
			 * not forwarded. Basically skb_unshare
			 * failed and we want to continue with
			 * next nbuf.
			 */
			tid_stats->fail_cnt[INTRABSS_DROP]++;
			return false;
		}
	}

	qdf_mem_set(nbuf->cb, 0x0, sizeof(nbuf->cb));
	dp_classify_critical_pkts(soc, ta_peer->vdev, nbuf);

	/* Don't send packets if tx is paused */
	if (!soc->is_tx_pause && !dp_tx_send((struct cdp_soc_t *)soc,
					     tx_vdev_id, nbuf)) {
		DP_PEER_PER_PKT_STATS_INC_PKT(ta_peer, rx.intra_bss.pkts, 1,
					      len);
	} else {
		DP_PEER_PER_PKT_STATS_INC_PKT(ta_peer, rx.intra_bss.fail, 1,
					      len);
		tid_stats->fail_cnt[INTRABSS_DROP]++;
		return false;
	}

	return true;
}

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#ifdef MESH_MODE_SUPPORT

void dp_rx_fill_mesh_stats(struct dp_vdev *vdev, qdf_nbuf_t nbuf,
			   uint8_t *rx_tlv_hdr,
			   struct dp_txrx_peer *txrx_peer)
{
	struct mesh_recv_hdr_s *rx_info = NULL;
	uint32_t pkt_type;
	uint32_t nss;
	uint32_t rate_mcs;
	uint32_t bw;
	uint8_t primary_chan_num;
	uint32_t center_chan_freq;
	struct dp_soc *soc = vdev->pdev->soc;
	struct dp_peer *peer;
	struct dp_peer *primary_link_peer;
	struct dp_soc *link_peer_soc;
	cdp_peer_stats_param_t buf = {0};

	/* fill recv mesh stats */
	rx_info = qdf_mem_malloc(sizeof(struct mesh_recv_hdr_s));

	/* upper layers are responsible to free this memory */

	if (!rx_info) {
		dp_rx_err("%pK: Memory allocation failed for mesh rx stats",
			  vdev->pdev->soc);
		DP_STATS_INC(vdev->pdev, mesh_mem_alloc, 1);
		return;
	}

	rx_info->rs_flags = MESH_RXHDR_VER1;
	if (qdf_nbuf_is_rx_chfrag_start(nbuf))
		rx_info->rs_flags |= MESH_RX_FIRST_MSDU;

	if (qdf_nbuf_is_rx_chfrag_end(nbuf))
		rx_info->rs_flags |= MESH_RX_LAST_MSDU;

	peer = dp_peer_get_ref_by_id(soc, txrx_peer->peer_id, DP_MOD_ID_MESH);
	if (peer) {
		if (hal_rx_tlv_get_is_decrypted(soc->hal_soc, rx_tlv_hdr)) {
			rx_info->rs_flags |= MESH_RX_DECRYPTED;
			rx_info->rs_keyix = hal_rx_msdu_get_keyid(soc->hal_soc,
								  rx_tlv_hdr);
			if (vdev->osif_get_key)
				vdev->osif_get_key(vdev->osif_vdev,
						   &rx_info->rs_decryptkey[0],
						   &peer->mac_addr.raw[0],
						   rx_info->rs_keyix);
		}

		dp_peer_unref_delete(peer, DP_MOD_ID_MESH);
	}

	primary_link_peer = dp_get_primary_link_peer_by_id(soc,
							   txrx_peer->peer_id,
							   DP_MOD_ID_MESH);

	if (qdf_likely(primary_link_peer)) {
		link_peer_soc = primary_link_peer->vdev->pdev->soc;
		dp_monitor_peer_get_stats_param(link_peer_soc,
						primary_link_peer,
						cdp_peer_rx_snr, &buf);
		rx_info->rs_snr = buf.rx_snr;
		dp_peer_unref_delete(primary_link_peer, DP_MOD_ID_MESH);
	}

	rx_info->rs_rssi = rx_info->rs_snr + DP_DEFAULT_NOISEFLOOR;

	soc = vdev->pdev->soc;
	primary_chan_num = hal_rx_tlv_get_freq(soc->hal_soc, rx_tlv_hdr);
	center_chan_freq = hal_rx_tlv_get_freq(soc->hal_soc, rx_tlv_hdr) >> 16;

	if (soc->cdp_soc.ol_ops && soc->cdp_soc.ol_ops->freq_to_band) {
		rx_info->rs_band = soc->cdp_soc.ol_ops->freq_to_band(
							soc->ctrl_psoc,
							vdev->pdev->pdev_id,
							center_chan_freq);
	}
	rx_info->rs_channel = primary_chan_num;
	pkt_type = hal_rx_tlv_get_pkt_type(soc->hal_soc, rx_tlv_hdr);
	rate_mcs = hal_rx_tlv_rate_mcs_get(soc->hal_soc, rx_tlv_hdr);
	bw = hal_rx_tlv_bw_get(soc->hal_soc, rx_tlv_hdr);
	nss = hal_rx_msdu_start_nss_get(soc->hal_soc, rx_tlv_hdr);
	rx_info->rs_ratephy1 = rate_mcs | (nss << 0x8) | (pkt_type << 16) |
				(bw << 24);

	qdf_nbuf_set_rx_fctx_type(nbuf, (void *)rx_info, CB_FTYPE_MESH_RX_INFO);

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_MED,
		FL("Mesh rx stats: flags %x, rssi %x, chn %x, rate %x, kix %x, snr %x"),
						rx_info->rs_flags,
						rx_info->rs_rssi,
						rx_info->rs_channel,
						rx_info->rs_ratephy1,
						rx_info->rs_keyix,
						rx_info->rs_snr);

}

QDF_STATUS dp_rx_filter_mesh_packets(struct dp_vdev *vdev, qdf_nbuf_t nbuf,
					uint8_t *rx_tlv_hdr)
{
	union dp_align_mac_addr mac_addr;
	struct dp_soc *soc = vdev->pdev->soc;

	if (qdf_unlikely(vdev->mesh_rx_filter)) {
		if (vdev->mesh_rx_filter & MESH_FILTER_OUT_FROMDS)
			if (hal_rx_mpdu_get_fr_ds(soc->hal_soc,
						  rx_tlv_hdr))
				return  QDF_STATUS_SUCCESS;

		if (vdev->mesh_rx_filter & MESH_FILTER_OUT_TODS)
			if (hal_rx_mpdu_get_to_ds(soc->hal_soc,
						  rx_tlv_hdr))
				return  QDF_STATUS_SUCCESS;

		if (vdev->mesh_rx_filter & MESH_FILTER_OUT_NODS)
			if (!hal_rx_mpdu_get_fr_ds(soc->hal_soc,
						   rx_tlv_hdr) &&
			    !hal_rx_mpdu_get_to_ds(soc->hal_soc,
						   rx_tlv_hdr))
				return  QDF_STATUS_SUCCESS;

		if (vdev->mesh_rx_filter & MESH_FILTER_OUT_RA) {
			if (hal_rx_mpdu_get_addr1(soc->hal_soc,
						  rx_tlv_hdr,
					&mac_addr.raw[0]))
				return QDF_STATUS_E_FAILURE;

			if (!qdf_mem_cmp(&mac_addr.raw[0],
					&vdev->mac_addr.raw[0],
					QDF_MAC_ADDR_SIZE))
				return  QDF_STATUS_SUCCESS;
		}

		if (vdev->mesh_rx_filter & MESH_FILTER_OUT_TA) {
			if (hal_rx_mpdu_get_addr2(soc->hal_soc,
						  rx_tlv_hdr,
						  &mac_addr.raw[0]))
				return QDF_STATUS_E_FAILURE;

			if (!qdf_mem_cmp(&mac_addr.raw[0],
					&vdev->mac_addr.raw[0],
					QDF_MAC_ADDR_SIZE))
				return  QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

#else
void dp_rx_fill_mesh_stats(struct dp_vdev *vdev, qdf_nbuf_t nbuf,
				uint8_t *rx_tlv_hdr, struct dp_txrx_peer *peer)
{
}

QDF_STATUS dp_rx_filter_mesh_packets(struct dp_vdev *vdev, qdf_nbuf_t nbuf,
					uint8_t *rx_tlv_hdr)
{
	return QDF_STATUS_E_FAILURE;
}

#endif

#ifdef RX_PEER_INVALID_ENH
uint8_t dp_rx_process_invalid_peer(struct dp_soc *soc, qdf_nbuf_t mpdu,
				   uint8_t mac_id)
{
	struct dp_invalid_peer_msg msg;
	struct dp_vdev *vdev = NULL;
	struct dp_pdev *pdev = NULL;
	struct ieee80211_frame *wh;
	qdf_nbuf_t curr_nbuf, next_nbuf;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(mpdu);
	uint8_t *rx_pkt_hdr = NULL;
	int i = 0;

	if (!HAL_IS_DECAP_FORMAT_RAW(soc->hal_soc, rx_tlv_hdr)) {
		dp_rx_debug("%pK: Drop decapped frames", soc);
		goto free;
	}

	/* In RAW packet, packet header will be part of data */
	rx_pkt_hdr = rx_tlv_hdr + soc->rx_pkt_tlv_size;
	wh = (struct ieee80211_frame *)rx_pkt_hdr;

	if (!DP_FRAME_IS_DATA(wh)) {
		dp_rx_debug("%pK: NAWDS valid only for data frames", soc);
		goto free;
	}

	if (qdf_nbuf_len(mpdu) < sizeof(struct ieee80211_frame)) {
		dp_rx_err("%pK: Invalid nbuf length", soc);
		goto free;
	}

	/* In DMAC case the rx_desc_pools are common across PDEVs
	 * so PDEV cannot be derived from the pool_id.
	 *
	 * link_id need to derived from the TLV tag word which is
	 * disabled by default. For now adding a WAR to get vdev
	 * with brute force this need to fixed with word based subscription
	 * support is added by enabling TLV tag word
	 */
	if (soc->features.dmac_cmn_src_rxbuf_ring_enabled) {
		for (i = 0; i < MAX_PDEV_CNT; i++) {
			pdev = soc->pdev_list[i];

			if (!pdev || qdf_unlikely(pdev->is_pdev_down))
				continue;

			TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
				if (qdf_mem_cmp(wh->i_addr1, vdev->mac_addr.raw,
						QDF_MAC_ADDR_SIZE) == 0) {
					goto out;
				}
			}
		}
	} else {
		pdev = dp_get_pdev_for_lmac_id(soc, mac_id);

		if (!pdev || qdf_unlikely(pdev->is_pdev_down)) {
			dp_rx_err("%pK: PDEV %s",
				  soc, !pdev ? "not found" : "down");
			goto free;
		}

		if (dp_monitor_filter_neighbour_peer(pdev, rx_pkt_hdr) ==
		    QDF_STATUS_SUCCESS)
			return 0;

		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			if (qdf_mem_cmp(wh->i_addr1, vdev->mac_addr.raw,
					QDF_MAC_ADDR_SIZE) == 0) {
				goto out;
			}
		}
	}

	if (!vdev) {
		dp_rx_err("%pK: VDEV not found", soc);
		goto free;
	}
out:
	msg.wh = wh;
	qdf_nbuf_pull_head(mpdu, soc->rx_pkt_tlv_size);
	msg.nbuf = mpdu;
	msg.vdev_id = vdev->vdev_id;

	/*
	 * NOTE: Only valid for HKv1.
	 * If smart monitor mode is enabled on RE, we are getting invalid
	 * peer frames with RA as STA mac of RE and the TA not matching
	 * with any NAC list or the the BSSID.Such frames need to dropped
	 * in order to avoid HM_WDS false addition.
	 */
	if (pdev->soc->cdp_soc.ol_ops->rx_invalid_peer) {
		if (dp_monitor_drop_inv_peer_pkts(vdev) == QDF_STATUS_SUCCESS) {
			dp_rx_warn("%pK: Drop inv peer pkts with STA RA:%pm",
				   soc, wh->i_addr1);
			goto free;
		}
		pdev->soc->cdp_soc.ol_ops->rx_invalid_peer(
				(struct cdp_ctrl_objmgr_psoc *)soc->ctrl_psoc,
				pdev->pdev_id, &msg);
	}

free:
	/* Drop and free packet */
	curr_nbuf = mpdu;
	while (curr_nbuf) {
		next_nbuf = qdf_nbuf_next(curr_nbuf);
		dp_rx_nbuf_free(curr_nbuf);
		curr_nbuf = next_nbuf;
	}

	return 0;
}

void dp_rx_process_invalid_peer_wrapper(struct dp_soc *soc,
					qdf_nbuf_t mpdu, bool mpdu_done,
					uint8_t mac_id)
{
	/* Only trigger the process when mpdu is completed */
	if (mpdu_done)
		dp_rx_process_invalid_peer(soc, mpdu, mac_id);
}
#else
uint8_t dp_rx_process_invalid_peer(struct dp_soc *soc, qdf_nbuf_t mpdu,
				   uint8_t mac_id)
{
	qdf_nbuf_t curr_nbuf, next_nbuf;
	struct dp_pdev *pdev;
	struct dp_vdev *vdev = NULL;
	struct ieee80211_frame *wh;
	struct dp_peer *peer = NULL;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(mpdu);
	uint8_t *rx_pkt_hdr = hal_rx_pkt_hdr_get(soc->hal_soc, rx_tlv_hdr);

	wh = (struct ieee80211_frame *)rx_pkt_hdr;

	if (!DP_FRAME_IS_DATA(wh)) {
		QDF_TRACE_ERROR_RL(QDF_MODULE_ID_DP,
				   "only for data frames");
		goto free;
	}

	if (qdf_nbuf_len(mpdu) < sizeof(struct ieee80211_frame)) {
		dp_rx_info_rl("%pK: Invalid nbuf length", soc);
		goto free;
	}

	pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	if (!pdev) {
		dp_rx_info_rl("%pK: PDEV not found", soc);
		goto free;
	}

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	DP_PDEV_ITERATE_VDEV_LIST(pdev, vdev) {
		if (qdf_mem_cmp(wh->i_addr1, vdev->mac_addr.raw,
				QDF_MAC_ADDR_SIZE) == 0) {
			qdf_spin_unlock_bh(&pdev->vdev_list_lock);
			goto out;
		}
	}
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);

	if (!vdev) {
		dp_rx_info_rl("%pK: VDEV not found", soc);
		goto free;
	}

out:
	if (vdev->opmode == wlan_op_mode_ap) {
		peer = dp_peer_find_hash_find(soc, wh->i_addr2, 0,
					      vdev->vdev_id,
					      DP_MOD_ID_RX_ERR);
		/* If SA is a valid peer in vdev,
		 * don't send disconnect
		 */
		if (peer) {
			dp_peer_unref_delete(peer, DP_MOD_ID_RX_ERR);
			DP_STATS_INC(soc, rx.err.decrypt_err_drop, 1);
			dp_err_rl("invalid peer frame with correct SA/RA is freed");
			goto free;
		}
	}

	if (soc->cdp_soc.ol_ops->rx_invalid_peer)
		soc->cdp_soc.ol_ops->rx_invalid_peer(vdev->vdev_id, wh);
free:

	/* Drop and free packet */
	curr_nbuf = mpdu;
	while (curr_nbuf) {
		next_nbuf = qdf_nbuf_next(curr_nbuf);
		dp_rx_nbuf_free(curr_nbuf);
		curr_nbuf = next_nbuf;
	}

	/* Reset the head and tail pointers */
	pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	if (pdev) {
		pdev->invalid_peer_head_msdu = NULL;
		pdev->invalid_peer_tail_msdu = NULL;
	}

	return 0;
}

void dp_rx_process_invalid_peer_wrapper(struct dp_soc *soc,
					qdf_nbuf_t mpdu, bool mpdu_done,
					uint8_t mac_id)
{
	/* Process the nbuf */
	dp_rx_process_invalid_peer(soc, mpdu, mac_id);
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED

#ifdef RECEIVE_OFFLOAD
/**
 * dp_rx_print_offload_info() - Print offload info from RX TLV
 * @soc: dp soc handle
 * @msdu: MSDU for which the offload info is to be printed
 *
 * Return: None
 */
static void dp_rx_print_offload_info(struct dp_soc *soc,
				     qdf_nbuf_t msdu)
{
	dp_verbose_debug("----------------------RX DESC LRO/GRO----------------------");
	dp_verbose_debug("lro_eligible 0x%x",
			 QDF_NBUF_CB_RX_LRO_ELIGIBLE(msdu));
	dp_verbose_debug("pure_ack 0x%x", QDF_NBUF_CB_RX_TCP_PURE_ACK(msdu));
	dp_verbose_debug("chksum 0x%x", QDF_NBUF_CB_RX_TCP_CHKSUM(msdu));
	dp_verbose_debug("TCP seq num 0x%x", QDF_NBUF_CB_RX_TCP_SEQ_NUM(msdu));
	dp_verbose_debug("TCP ack num 0x%x", QDF_NBUF_CB_RX_TCP_ACK_NUM(msdu));
	dp_verbose_debug("TCP window 0x%x", QDF_NBUF_CB_RX_TCP_WIN(msdu));
	dp_verbose_debug("TCP protocol 0x%x", QDF_NBUF_CB_RX_TCP_PROTO(msdu));
	dp_verbose_debug("TCP offset 0x%x", QDF_NBUF_CB_RX_TCP_OFFSET(msdu));
	dp_verbose_debug("toeplitz 0x%x", QDF_NBUF_CB_RX_FLOW_ID(msdu));
	dp_verbose_debug("---------------------------------------------------------");
}

void dp_rx_fill_gro_info(struct dp_soc *soc, uint8_t *rx_tlv,
			 qdf_nbuf_t msdu, uint32_t *rx_ol_pkt_cnt)
{
	struct hal_offload_info offload_info;

	if (!wlan_cfg_is_gro_enabled(soc->wlan_cfg_ctx))
		return;

	if (hal_rx_tlv_get_offload_info(soc->hal_soc, rx_tlv, &offload_info))
		return;

	*rx_ol_pkt_cnt = *rx_ol_pkt_cnt + 1;

	QDF_NBUF_CB_RX_LRO_ELIGIBLE(msdu) = offload_info.lro_eligible;
	QDF_NBUF_CB_RX_TCP_PURE_ACK(msdu) = offload_info.tcp_pure_ack;
	QDF_NBUF_CB_RX_TCP_CHKSUM(msdu) =
			hal_rx_tlv_get_tcp_chksum(soc->hal_soc,
						  rx_tlv);
	QDF_NBUF_CB_RX_TCP_SEQ_NUM(msdu) = offload_info.tcp_seq_num;
	QDF_NBUF_CB_RX_TCP_ACK_NUM(msdu) = offload_info.tcp_ack_num;
	QDF_NBUF_CB_RX_TCP_WIN(msdu) = offload_info.tcp_win;
	QDF_NBUF_CB_RX_TCP_PROTO(msdu) = offload_info.tcp_proto;
	QDF_NBUF_CB_RX_IPV6_PROTO(msdu) = offload_info.ipv6_proto;
	QDF_NBUF_CB_RX_TCP_OFFSET(msdu) = offload_info.tcp_offset;
	QDF_NBUF_CB_RX_FLOW_ID(msdu) = offload_info.flow_id;

	dp_rx_print_offload_info(soc, msdu);
}
#endif /* RECEIVE_OFFLOAD */

/**
 * dp_rx_adjust_nbuf_len() - set appropriate msdu length in nbuf.
 *
 * @soc: DP soc handle
 * @nbuf: pointer to msdu.
 * @mpdu_len: mpdu length
 * @l3_pad_len: L3 padding length by HW
 *
 * Return: returns true if nbuf is last msdu of mpdu else returns false.
 */
static inline bool dp_rx_adjust_nbuf_len(struct dp_soc *soc,
					 qdf_nbuf_t nbuf,
					 uint16_t *mpdu_len,
					 uint32_t l3_pad_len)
{
	bool last_nbuf;
	uint32_t pkt_hdr_size;

	pkt_hdr_size = soc->rx_pkt_tlv_size + l3_pad_len;

	if ((*mpdu_len + pkt_hdr_size) > RX_DATA_BUFFER_SIZE) {
		qdf_nbuf_set_pktlen(nbuf, RX_DATA_BUFFER_SIZE);
		last_nbuf = false;
		*mpdu_len -= (RX_DATA_BUFFER_SIZE - pkt_hdr_size);
	} else {
		qdf_nbuf_set_pktlen(nbuf, (*mpdu_len + pkt_hdr_size));
		last_nbuf = true;
		*mpdu_len = 0;
	}

	return last_nbuf;
}

/**
 * dp_get_l3_hdr_pad_len() - get L3 header padding length.
 *
 * @soc: DP soc handle
 * @nbuf: pointer to msdu.
 *
 * Return: returns padding length in bytes.
 */
static inline uint32_t dp_get_l3_hdr_pad_len(struct dp_soc *soc,
					     qdf_nbuf_t nbuf)
{
	uint32_t l3_hdr_pad = 0;
	uint8_t *rx_tlv_hdr;
	struct hal_rx_msdu_metadata msdu_metadata;

	while (nbuf) {
		if (!qdf_nbuf_is_rx_chfrag_cont(nbuf)) {
			/* scattered msdu end with continuation is 0 */
			rx_tlv_hdr = qdf_nbuf_data(nbuf);
			hal_rx_msdu_metadata_get(soc->hal_soc,
						 rx_tlv_hdr,
						 &msdu_metadata);
			l3_hdr_pad = msdu_metadata.l3_hdr_pad;
			break;
		}
		nbuf = nbuf->next;
	}

	return l3_hdr_pad;
}

qdf_nbuf_t dp_rx_sg_create(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
	qdf_nbuf_t parent, frag_list, next = NULL;
	uint16_t frag_list_len = 0;
	uint16_t mpdu_len;
	bool last_nbuf;
	uint32_t l3_hdr_pad_offset = 0;

	/*
	 * Use msdu len got from REO entry descriptor instead since
	 * there is case the RX PKT TLV is corrupted while msdu_len
	 * from REO descriptor is right for non-raw RX scatter msdu.
	 */
	mpdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);

	/*
	 * this is a case where the complete msdu fits in one single nbuf.
	 * in this case HW sets both start and end bit and we only need to
	 * reset these bits for RAW mode simulator to decap the pkt
	 */
	if (qdf_nbuf_is_rx_chfrag_start(nbuf) &&
					qdf_nbuf_is_rx_chfrag_end(nbuf)) {
		qdf_nbuf_set_pktlen(nbuf, mpdu_len + soc->rx_pkt_tlv_size);
		qdf_nbuf_pull_head(nbuf, soc->rx_pkt_tlv_size);
		return nbuf;
	}

	l3_hdr_pad_offset = dp_get_l3_hdr_pad_len(soc, nbuf);
	/*
	 * This is a case where we have multiple msdus (A-MSDU) spread across
	 * multiple nbufs. here we create a fraglist out of these nbufs.
	 *
	 * the moment we encounter a nbuf with continuation bit set we
	 * know for sure we have an MSDU which is spread across multiple
	 * nbufs. We loop through and reap nbufs till we reach last nbuf.
	 */
	parent = nbuf;
	frag_list = nbuf->next;
	nbuf = nbuf->next;

	/*
	 * set the start bit in the first nbuf we encounter with continuation
	 * bit set. This has the proper mpdu length set as it is the first
	 * msdu of the mpdu. this becomes the parent nbuf and the subsequent
	 * nbufs will form the frag_list of the parent nbuf.
	 */
	qdf_nbuf_set_rx_chfrag_start(parent, 1);
	/*
	 * L3 header padding is only needed for the 1st buffer
	 * in a scattered msdu
	 */
	last_nbuf = dp_rx_adjust_nbuf_len(soc, parent, &mpdu_len,
					  l3_hdr_pad_offset);

	/*
	 * MSDU cont bit is set but reported MPDU length can fit
	 * in to single buffer
	 *
	 * Increment error stats and avoid SG list creation
	 */
	if (last_nbuf) {
		DP_STATS_INC(soc, rx.err.msdu_continuation_err, 1);
		qdf_nbuf_pull_head(parent,
				   soc->rx_pkt_tlv_size + l3_hdr_pad_offset);
		return parent;
	}

	/*
	 * this is where we set the length of the fragments which are
	 * associated to the parent nbuf. We iterate through the frag_list
	 * till we hit the last_nbuf of the list.
	 */
	do {
		last_nbuf = dp_rx_adjust_nbuf_len(soc, nbuf, &mpdu_len, 0);
		qdf_nbuf_pull_head(nbuf,
				   soc->rx_pkt_tlv_size);
		frag_list_len += qdf_nbuf_len(nbuf);

		if (last_nbuf) {
			next = nbuf->next;
			nbuf->next = NULL;
			break;
		} else if (qdf_nbuf_is_rx_chfrag_end(nbuf)) {
			dp_err("Invalid packet length\n");
			qdf_assert_always(0);
		}
		nbuf = nbuf->next;
	} while (!last_nbuf);

	qdf_nbuf_set_rx_chfrag_start(nbuf, 0);
	qdf_nbuf_append_ext_list(parent, frag_list, frag_list_len);
	parent->next = next;

	qdf_nbuf_pull_head(parent,
			   soc->rx_pkt_tlv_size + l3_hdr_pad_offset);
	return parent;
}

#ifdef DP_RX_SG_FRAME_SUPPORT
bool dp_rx_is_sg_supported(void)
{
	return true;
}
#else
bool dp_rx_is_sg_supported(void)
{
	return false;
}
#endif

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#ifdef QCA_PEER_EXT_STATS
void dp_rx_compute_tid_delay(struct cdp_delay_tid_stats *stats,
			     qdf_nbuf_t nbuf)
{
	struct cdp_delay_rx_stats  *rx_delay = &stats->rx_delay;
	uint32_t to_stack = qdf_nbuf_get_timedelta_ms(nbuf);

	dp_hist_update_stats(&rx_delay->to_stack_delay, to_stack);
}
#endif /* QCA_PEER_EXT_STATS */

void dp_rx_compute_delay(struct dp_vdev *vdev, qdf_nbuf_t nbuf)
{
	uint8_t ring_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	int64_t current_ts = qdf_ktime_to_ms(qdf_ktime_get());
	uint32_t to_stack = qdf_nbuf_get_timedelta_ms(nbuf);
	uint8_t tid = qdf_nbuf_get_tid_val(nbuf);
	uint32_t interframe_delay =
		(uint32_t)(current_ts - vdev->prev_rx_deliver_tstamp);
	struct cdp_tid_rx_stats *rstats =
		&vdev->pdev->stats.tid_stats.tid_rx_stats[ring_id][tid];

	dp_update_delay_stats(NULL, rstats, to_stack, tid,
			      CDP_DELAY_STATS_REAP_STACK, ring_id, false);
	/*
	 * Update interframe delay stats calculated at deliver_data_ol point.
	 * Value of vdev->prev_rx_deliver_tstamp will be 0 for 1st frame, so
	 * interframe delay will not be calculate correctly for 1st frame.
	 * On the other side, this will help in avoiding extra per packet check
	 * of vdev->prev_rx_deliver_tstamp.
	 */
	dp_update_delay_stats(NULL, rstats, interframe_delay, tid,
			      CDP_DELAY_STATS_RX_INTERFRAME, ring_id, false);
	vdev->prev_rx_deliver_tstamp = current_ts;
}

/**
 * dp_rx_drop_nbuf_list() - drop an nbuf list
 * @pdev: dp pdev reference
 * @buf_list: buffer list to be dropepd
 *
 * Return: int (number of bufs dropped)
 */
static inline int dp_rx_drop_nbuf_list(struct dp_pdev *pdev,
				       qdf_nbuf_t buf_list)
{
	struct cdp_tid_rx_stats *stats = NULL;
	uint8_t tid = 0, ring_id = 0;
	int num_dropped = 0;
	qdf_nbuf_t buf, next_buf;

	buf = buf_list;
	while (buf) {
		ring_id = QDF_NBUF_CB_RX_CTX_ID(buf);
		next_buf = qdf_nbuf_queue_next(buf);
		tid = qdf_nbuf_get_tid_val(buf);
		if (qdf_likely(pdev)) {
			stats = &pdev->stats.tid_stats.tid_rx_stats[ring_id][tid];
			stats->fail_cnt[INVALID_PEER_VDEV]++;
			stats->delivered_to_stack--;
		}
		dp_rx_nbuf_free(buf);
		buf = next_buf;
		num_dropped++;
	}

	return num_dropped;
}

#ifdef QCA_SUPPORT_WDS_EXTENDED
/**
 * dp_rx_deliver_to_stack_ext() - Deliver to netdev per sta
 * @soc: core txrx main context
 * @vdev: vdev
 * @txrx_peer: txrx peer
 * @nbuf_head: skb list head
 *
 * Return: true if packet is delivered to netdev per STA.
 */
static inline bool
dp_rx_deliver_to_stack_ext(struct dp_soc *soc, struct dp_vdev *vdev,
			   struct dp_txrx_peer *txrx_peer, qdf_nbuf_t nbuf_head)
{
	/*
	 * When extended WDS is disabled, frames are sent to AP netdevice.
	 */
	if (qdf_likely(!vdev->wds_ext_enabled))
		return false;

	/*
	 * There can be 2 cases:
	 * 1. Send frame to parent netdev if its not for netdev per STA
	 * 2. If frame is meant for netdev per STA:
	 *    a. Send frame to appropriate netdev using registered fp.
	 *    b. If fp is NULL, drop the frames.
	 */
	if (!txrx_peer->wds_ext.init)
		return false;

	if (txrx_peer->osif_rx)
		txrx_peer->osif_rx(txrx_peer->wds_ext.osif_peer, nbuf_head);
	else
		dp_rx_drop_nbuf_list(vdev->pdev, nbuf_head);

	return true;
}

#else
static inline bool
dp_rx_deliver_to_stack_ext(struct dp_soc *soc, struct dp_vdev *vdev,
			   struct dp_txrx_peer *txrx_peer, qdf_nbuf_t nbuf_head)
{
	return false;
}
#endif

#ifdef PEER_CACHE_RX_PKTS
void dp_rx_flush_rx_cached(struct dp_peer *peer, bool drop)
{
	struct dp_peer_cached_bufq *bufqi;
	struct dp_rx_cached_buf *cache_buf = NULL;
	ol_txrx_rx_fp data_rx = NULL;
	int num_buff_elem;
	QDF_STATUS status;

	/*
	 * Flush dp cached frames only for mld peers and legacy peers, as
	 * link peers don't store cached frames
	 */
	if (IS_MLO_DP_LINK_PEER(peer))
		return;

	if (!peer->txrx_peer) {
		dp_err("txrx_peer NULL!! peer mac_addr("QDF_MAC_ADDR_FMT")",
			QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		return;
	}

	if (qdf_atomic_inc_return(&peer->txrx_peer->flush_in_progress) > 1) {
		qdf_atomic_dec(&peer->txrx_peer->flush_in_progress);
		return;
	}

	qdf_spin_lock_bh(&peer->peer_info_lock);
	if (peer->state >= OL_TXRX_PEER_STATE_CONN && peer->vdev->osif_rx)
		data_rx = peer->vdev->osif_rx;
	else
		drop = true;
	qdf_spin_unlock_bh(&peer->peer_info_lock);

	bufqi = &peer->txrx_peer->bufq_info;

	qdf_spin_lock_bh(&bufqi->bufq_lock);
	qdf_list_remove_front(&bufqi->cached_bufq,
			      (qdf_list_node_t **)&cache_buf);
	while (cache_buf) {
		num_buff_elem = QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(
								cache_buf->buf);
		bufqi->entries -= num_buff_elem;
		qdf_spin_unlock_bh(&bufqi->bufq_lock);
		if (drop) {
			bufqi->dropped = dp_rx_drop_nbuf_list(peer->vdev->pdev,
							      cache_buf->buf);
		} else {
			/* Flush the cached frames to OSIF DEV */
			status = data_rx(peer->vdev->osif_vdev, cache_buf->buf);
			if (status != QDF_STATUS_SUCCESS)
				bufqi->dropped = dp_rx_drop_nbuf_list(
							peer->vdev->pdev,
							cache_buf->buf);
		}
		qdf_mem_free(cache_buf);
		cache_buf = NULL;
		qdf_spin_lock_bh(&bufqi->bufq_lock);
		qdf_list_remove_front(&bufqi->cached_bufq,
				      (qdf_list_node_t **)&cache_buf);
	}
	qdf_spin_unlock_bh(&bufqi->bufq_lock);
	qdf_atomic_dec(&peer->txrx_peer->flush_in_progress);
}

/**
 * dp_rx_enqueue_rx() - cache rx frames
 * @peer: peer
 * @txrx_peer: DP txrx_peer
 * @rx_buf_list: cache buffer list
 *
 * Return: None
 */
static QDF_STATUS
dp_rx_enqueue_rx(struct dp_peer *peer,
		 struct dp_txrx_peer *txrx_peer,
		 qdf_nbuf_t rx_buf_list)
{
	struct dp_rx_cached_buf *cache_buf;
	struct dp_peer_cached_bufq *bufqi = &txrx_peer->bufq_info;
	int num_buff_elem;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct dp_soc *soc = txrx_peer->vdev->pdev->soc;
	struct dp_peer *ta_peer = NULL;

	/*
	 * If peer id is invalid which likely peer map has not completed,
	 * then need caller provide dp_peer pointer, else it's ok to use
	 * txrx_peer->peer_id to get dp_peer.
	 */
	if (peer) {
		if (QDF_STATUS_SUCCESS ==
		    dp_peer_get_ref(soc, peer, DP_MOD_ID_RX))
			ta_peer = peer;
	} else {
		ta_peer = dp_peer_get_ref_by_id(soc, txrx_peer->peer_id,
						DP_MOD_ID_RX);
	}

	if (!ta_peer) {
		bufqi->dropped = dp_rx_drop_nbuf_list(txrx_peer->vdev->pdev,
						      rx_buf_list);
		return QDF_STATUS_E_INVAL;
	}

	dp_debug_rl("bufq->curr %d bufq->drops %d", bufqi->entries,
		    bufqi->dropped);
	if (!ta_peer->valid) {
		bufqi->dropped = dp_rx_drop_nbuf_list(txrx_peer->vdev->pdev,
						      rx_buf_list);
		ret = QDF_STATUS_E_INVAL;
		goto fail;
	}

	qdf_spin_lock_bh(&bufqi->bufq_lock);
	if (bufqi->entries >= bufqi->thresh) {
		bufqi->dropped = dp_rx_drop_nbuf_list(txrx_peer->vdev->pdev,
						      rx_buf_list);
		qdf_spin_unlock_bh(&bufqi->bufq_lock);
		ret = QDF_STATUS_E_RESOURCES;
		goto fail;
	}
	qdf_spin_unlock_bh(&bufqi->bufq_lock);

	num_buff_elem = QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(rx_buf_list);

	cache_buf = qdf_mem_malloc_atomic(sizeof(*cache_buf));
	if (!cache_buf) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Failed to allocate buf to cache rx frames");
		bufqi->dropped = dp_rx_drop_nbuf_list(txrx_peer->vdev->pdev,
						      rx_buf_list);
		ret = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	cache_buf->buf = rx_buf_list;

	qdf_spin_lock_bh(&bufqi->bufq_lock);
	qdf_list_insert_back(&bufqi->cached_bufq,
			     &cache_buf->node);
	bufqi->entries += num_buff_elem;
	qdf_spin_unlock_bh(&bufqi->bufq_lock);

fail:
	dp_peer_unref_delete(ta_peer, DP_MOD_ID_RX);
	return ret;
}

static inline
bool dp_rx_is_peer_cache_bufq_supported(void)
{
	return true;
}
#else
static inline
bool dp_rx_is_peer_cache_bufq_supported(void)
{
	return false;
}

static inline QDF_STATUS
dp_rx_enqueue_rx(struct dp_peer *peer,
		 struct dp_txrx_peer *txrx_peer,
		 qdf_nbuf_t rx_buf_list)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifndef DELIVERY_TO_STACK_STATUS_CHECK
/**
 * dp_rx_check_delivery_to_stack() - Deliver pkts to network
 * using the appropriate call back functions.
 * @soc: soc
 * @vdev: vdev
 * @txrx_peer: peer
 * @nbuf_head: skb list head
 *
 * Return: None
 */
static void dp_rx_check_delivery_to_stack(struct dp_soc *soc,
					  struct dp_vdev *vdev,
					  struct dp_txrx_peer *txrx_peer,
					  qdf_nbuf_t nbuf_head)
{
	if (qdf_unlikely(dp_rx_deliver_to_stack_ext(soc, vdev,
						    txrx_peer, nbuf_head)))
		return;

	/* Function pointer initialized only when FISA is enabled */
	if (vdev->osif_fisa_rx)
		/* on failure send it via regular path */
		vdev->osif_fisa_rx(soc, vdev, nbuf_head);
	else
		vdev->osif_rx(vdev->osif_vdev, nbuf_head);
}

#else
/**
 * dp_rx_check_delivery_to_stack() - Deliver pkts to network
 * using the appropriate call back functions.
 * @soc: soc
 * @vdev: vdev
 * @txrx_peer: txrx peer
 * @nbuf_head: skb list head
 *
 * Check the return status of the call back function and drop
 * the packets if the return status indicates a failure.
 *
 * Return: None
 */
static void dp_rx_check_delivery_to_stack(struct dp_soc *soc,
					  struct dp_vdev *vdev,
					  struct dp_txrx_peer *txrx_peer,
					  qdf_nbuf_t nbuf_head)
{
	int num_nbuf = 0;
	QDF_STATUS ret_val = QDF_STATUS_E_FAILURE;

	/* Function pointer initialized only when FISA is enabled */
	if (vdev->osif_fisa_rx)
		/* on failure send it via regular path */
		ret_val = vdev->osif_fisa_rx(soc, vdev, nbuf_head);
	else if (vdev->osif_rx)
		ret_val = vdev->osif_rx(vdev->osif_vdev, nbuf_head);

	if (!QDF_IS_STATUS_SUCCESS(ret_val)) {
		num_nbuf = dp_rx_drop_nbuf_list(vdev->pdev, nbuf_head);
		DP_STATS_INC(soc, rx.err.rejected, num_nbuf);
		if (txrx_peer)
			DP_PEER_STATS_FLAT_DEC(txrx_peer, to_stack.num,
					       num_nbuf);
	}
}
#endif /* ifdef DELIVERY_TO_STACK_STATUS_CHECK */

/**
 * dp_rx_validate_rx_callbacks() - validate rx callbacks
 * @soc: DP soc
 * @vdev: DP vdev handle
 * @txrx_peer: pointer to the txrx peer object
 * @nbuf_head: skb list head
 *
 * Return: QDF_STATUS - QDF_STATUS_SUCCESS
 *			QDF_STATUS_E_FAILURE
 */
static inline QDF_STATUS
dp_rx_validate_rx_callbacks(struct dp_soc *soc,
			    struct dp_vdev *vdev,
			    struct dp_txrx_peer *txrx_peer,
			    qdf_nbuf_t nbuf_head)
{
	int num_nbuf;

	if (qdf_unlikely(!vdev || vdev->delete.pending)) {
		num_nbuf = dp_rx_drop_nbuf_list(NULL, nbuf_head);
		/*
		 * This is a special case where vdev is invalid,
		 * so we cannot know the pdev to which this packet
		 * belonged. Hence we update the soc rx error stats.
		 */
		DP_STATS_INC(soc, rx.err.invalid_vdev, num_nbuf);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * highly unlikely to have a vdev without a registered rx
	 * callback function. if so let us free the nbuf_list.
	 */
	if (qdf_unlikely(!vdev->osif_rx)) {
		if (txrx_peer && dp_rx_is_peer_cache_bufq_supported()) {
			dp_rx_enqueue_rx(NULL, txrx_peer, nbuf_head);
		} else {
			num_nbuf = dp_rx_drop_nbuf_list(vdev->pdev,
							nbuf_head);
			DP_PEER_TO_STACK_DECC(txrx_peer, num_nbuf,
					      vdev->pdev->enhanced_stats_en);
		}
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_deliver_to_stack(struct dp_soc *soc,
				  struct dp_vdev *vdev,
				  struct dp_txrx_peer *txrx_peer,
				  qdf_nbuf_t nbuf_head,
				  qdf_nbuf_t nbuf_tail)
{
	if (dp_rx_validate_rx_callbacks(soc, vdev, txrx_peer, nbuf_head) !=
					QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_FAILURE;

	if (qdf_unlikely(vdev->rx_decap_type == htt_cmn_pkt_type_raw) ||
			(vdev->rx_decap_type == htt_cmn_pkt_type_native_wifi)) {
		vdev->osif_rsim_rx_decap(vdev->osif_vdev, &nbuf_head,
					 &nbuf_tail);
	}

	dp_rx_check_delivery_to_stack(soc, vdev, txrx_peer, nbuf_head);

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_EAPOL_OVER_CONTROL_PORT
QDF_STATUS dp_rx_eapol_deliver_to_stack(struct dp_soc *soc,
					struct dp_vdev *vdev,
					struct dp_txrx_peer *txrx_peer,
					qdf_nbuf_t nbuf_head,
					qdf_nbuf_t nbuf_tail)
{
	if (dp_rx_validate_rx_callbacks(soc, vdev, txrx_peer, nbuf_head) !=
					QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_FAILURE;

	vdev->osif_rx_eapol(vdev->osif_vdev, nbuf_head);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED
#ifdef VDEV_PEER_PROTOCOL_COUNT
#define dp_rx_msdu_stats_update_prot_cnts(vdev_hdl, nbuf, txrx_peer) \
{ \
	qdf_nbuf_t nbuf_local; \
	struct dp_txrx_peer *txrx_peer_local; \
	struct dp_vdev *vdev_local = vdev_hdl; \
	do { \
		if (qdf_likely(!((vdev_local)->peer_protocol_count_track))) \
			break; \
		nbuf_local = nbuf; \
		txrx_peer_local = txrx_peer; \
		if (qdf_unlikely(qdf_nbuf_is_frag((nbuf_local)))) \
			break; \
		else if (qdf_unlikely(qdf_nbuf_is_raw_frame((nbuf_local)))) \
			break; \
		dp_vdev_peer_stats_update_protocol_cnt((vdev_local), \
						       (nbuf_local), \
						       (txrx_peer_local), 0, 1); \
	} while (0); \
}
#else
#define dp_rx_msdu_stats_update_prot_cnts(vdev_hdl, nbuf, txrx_peer)
#endif

#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
/**
 * dp_rx_rates_stats_update() - update rate stats
 * from rx msdu.
 * @soc: datapath soc handle
 * @nbuf: received msdu buffer
 * @rx_tlv_hdr: rx tlv header
 * @txrx_peer: datapath txrx_peer handle
 * @sgi: Short Guard Interval
 * @mcs: Modulation and Coding Set
 * @nss: Number of Spatial Streams
 * @bw: BandWidth
 * @pkt_type: Corresponds to preamble
 *
 * To be precisely record rates, following factors are considered:
 * Exclude specific frames, ARP, DHCP, ssdp, etc.
 * Make sure to affect rx throughput as least as possible.
 *
 * Return: void
 */
static void
dp_rx_rates_stats_update(struct dp_soc *soc, qdf_nbuf_t nbuf,
			 uint8_t *rx_tlv_hdr, struct dp_txrx_peer *txrx_peer,
			 uint32_t sgi, uint32_t mcs,
			 uint32_t nss, uint32_t bw, uint32_t pkt_type)
{
	uint32_t rix;
	uint16_t ratecode;
	uint32_t avg_rx_rate;
	uint32_t ratekbps;
	enum cdp_punctured_modes punc_mode = NO_PUNCTURE;

	if (soc->high_throughput ||
	    dp_rx_data_is_specific(soc->hal_soc, rx_tlv_hdr, nbuf)) {
		return;
	}

	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.rx_rate, mcs);

	/* In 11b mode, the nss we get from tlv is 0, invalid and should be 1 */
	if (qdf_unlikely(pkt_type == DOT11_B))
		nss = 1;

	/* here pkt_type corresponds to preamble */
	ratekbps = dp_getrateindex(sgi,
				   mcs,
				   nss - 1,
				   pkt_type,
				   bw,
				   punc_mode,
				   &rix,
				   &ratecode);
	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.last_rx_rate, ratekbps);
	avg_rx_rate =
		dp_ath_rate_lpf(txrx_peer->stats.extd_stats.rx.avg_rx_rate,
				ratekbps);
	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.avg_rx_rate, avg_rx_rate);
	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.nss_info, nss);
	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.mcs_info, mcs);
	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.bw_info, bw);
	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.gi_info, sgi);
	DP_PEER_EXTD_STATS_UPD(txrx_peer, rx.preamble_info, pkt_type);
}
#else
static inline void
dp_rx_rates_stats_update(struct dp_soc *soc, qdf_nbuf_t nbuf,
			 uint8_t *rx_tlv_hdr, struct dp_txrx_peer *txrx_peer,
			 uint32_t sgi, uint32_t mcs,
			 uint32_t nss, uint32_t bw, uint32_t pkt_type)
{
}
#endif /* FEATURE_RX_LINKSPEED_ROAM_TRIGGER */

#ifndef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_rx_msdu_extd_stats_update(): Update Rx extended path stats for peer
 *
 * @soc: datapath soc handle
 * @nbuf: received msdu buffer
 * @rx_tlv_hdr: rx tlv header
 * @txrx_peer: datapath txrx_peer handle
 *
 * Return: void
 */
static inline
void dp_rx_msdu_extd_stats_update(struct dp_soc *soc, qdf_nbuf_t nbuf,
				  uint8_t *rx_tlv_hdr,
				  struct dp_txrx_peer *txrx_peer)
{
	bool is_ampdu;
	uint32_t sgi, mcs, tid, nss, bw, reception_type, pkt_type;
	uint8_t dst_mcs_idx;

	/*
	 * TODO - For KIWI this field is present in ring_desc
	 * Try to use ring desc instead of tlv.
	 */
	is_ampdu = hal_rx_mpdu_info_ampdu_flag_get(soc->hal_soc, rx_tlv_hdr);
	DP_PEER_EXTD_STATS_INCC(txrx_peer, rx.ampdu_cnt, 1, is_ampdu);
	DP_PEER_EXTD_STATS_INCC(txrx_peer, rx.non_ampdu_cnt, 1, !(is_ampdu));

	sgi = hal_rx_tlv_sgi_get(soc->hal_soc, rx_tlv_hdr);
	mcs = hal_rx_tlv_rate_mcs_get(soc->hal_soc, rx_tlv_hdr);
	tid = qdf_nbuf_get_tid_val(nbuf);
	bw = hal_rx_tlv_bw_get(soc->hal_soc, rx_tlv_hdr);
	reception_type = hal_rx_msdu_start_reception_type_get(soc->hal_soc,
							      rx_tlv_hdr);
	nss = hal_rx_msdu_start_nss_get(soc->hal_soc, rx_tlv_hdr);
	pkt_type = hal_rx_tlv_get_pkt_type(soc->hal_soc, rx_tlv_hdr);
	/* do HW to SW pkt type conversion */
	pkt_type = (pkt_type >= HAL_DOT11_MAX ? DOT11_MAX :
		    hal_2_dp_pkt_type_map[pkt_type]);

	DP_PEER_EXTD_STATS_INCC(txrx_peer, rx.rx_mpdu_cnt[mcs], 1,
		      ((mcs < MAX_MCS) && QDF_NBUF_CB_RX_CHFRAG_START(nbuf)));
	DP_PEER_EXTD_STATS_INCC(txrx_peer, rx.rx_mpdu_cnt[MAX_MCS - 1], 1,
		      ((mcs >= MAX_MCS) && QDF_NBUF_CB_RX_CHFRAG_START(nbuf)));
	DP_PEER_EXTD_STATS_INC(txrx_peer, rx.bw[bw], 1);
	/*
	 * only if nss > 0 and pkt_type is 11N/AC/AX,
	 * then increase index [nss - 1] in array counter.
	 */
	if (nss > 0 && CDP_IS_PKT_TYPE_SUPPORT_NSS(pkt_type))
		DP_PEER_EXTD_STATS_INC(txrx_peer, rx.nss[nss - 1], 1);

	DP_PEER_EXTD_STATS_INC(txrx_peer, rx.sgi_count[sgi], 1);
	DP_PEER_PER_PKT_STATS_INCC(txrx_peer, rx.err.mic_err, 1,
				   hal_rx_tlv_mic_err_get(soc->hal_soc,
				   rx_tlv_hdr));
	DP_PEER_PER_PKT_STATS_INCC(txrx_peer, rx.err.decrypt_err, 1,
				   hal_rx_tlv_decrypt_err_get(soc->hal_soc,
				   rx_tlv_hdr));

	DP_PEER_EXTD_STATS_INC(txrx_peer, rx.wme_ac_type[TID_TO_WME_AC(tid)], 1);
	DP_PEER_EXTD_STATS_INC(txrx_peer, rx.reception_type[reception_type], 1);

	dst_mcs_idx = dp_get_mcs_array_index_by_pkt_type_mcs(pkt_type, mcs);
	if (MCS_INVALID_ARRAY_INDEX != dst_mcs_idx)
		DP_PEER_EXTD_STATS_INC(txrx_peer,
				       rx.pkt_type[pkt_type].mcs_count[dst_mcs_idx],
				       1);

	dp_rx_rates_stats_update(soc, nbuf, rx_tlv_hdr, txrx_peer,
				 sgi, mcs, nss, bw, pkt_type);
}
#else
static inline
void dp_rx_msdu_extd_stats_update(struct dp_soc *soc, qdf_nbuf_t nbuf,
				  uint8_t *rx_tlv_hdr,
				  struct dp_txrx_peer *txrx_peer)
{
}
#endif

#if defined(DP_PKT_STATS_PER_LMAC) && defined(WLAN_FEATURE_11BE_MLO)
static inline void
dp_peer_update_rx_pkt_per_lmac(struct dp_txrx_peer *txrx_peer,
			       qdf_nbuf_t nbuf)
{
	uint8_t lmac_id = qdf_nbuf_get_lmac_id(nbuf);

	if (qdf_unlikely(lmac_id >= CDP_MAX_LMACS)) {
		dp_err_rl("Invalid lmac_id: %u vdev_id: %u",
			  lmac_id, QDF_NBUF_CB_RX_VDEV_ID(nbuf));

		if (qdf_likely(txrx_peer))
			dp_err_rl("peer_id: %u", txrx_peer->peer_id);

		return;
	}

	/* only count stats per lmac for MLO connection*/
	DP_PEER_PER_PKT_STATS_INCC_PKT(txrx_peer, rx.rx_lmac[lmac_id], 1,
				       QDF_NBUF_CB_RX_PKT_LEN(nbuf),
				       txrx_peer->mld_peer);
}
#else
static inline void
dp_peer_update_rx_pkt_per_lmac(struct dp_txrx_peer *txrx_peer,
			       qdf_nbuf_t nbuf)
{
}
#endif

void dp_rx_msdu_stats_update(struct dp_soc *soc, qdf_nbuf_t nbuf,
			     uint8_t *rx_tlv_hdr,
			     struct dp_txrx_peer *txrx_peer,
			     uint8_t ring_id,
			     struct cdp_tid_rx_stats *tid_stats)
{
	bool is_not_amsdu;
	struct dp_vdev *vdev = txrx_peer->vdev;
	bool enh_flag;
	qdf_ether_header_t *eh;
	uint16_t msdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);

	dp_rx_msdu_stats_update_prot_cnts(vdev, nbuf, txrx_peer);
	is_not_amsdu = qdf_nbuf_is_rx_chfrag_start(nbuf) &
			qdf_nbuf_is_rx_chfrag_end(nbuf);
	DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.rcvd_reo[ring_id], 1,
				      msdu_len);
	DP_PEER_PER_PKT_STATS_INCC(txrx_peer, rx.non_amsdu_cnt, 1,
				   is_not_amsdu);
	DP_PEER_PER_PKT_STATS_INCC(txrx_peer, rx.amsdu_cnt, 1, !is_not_amsdu);
	DP_PEER_PER_PKT_STATS_INCC(txrx_peer, rx.rx_retries, 1,
				   qdf_nbuf_is_rx_retry_flag(nbuf));
	dp_peer_update_rx_pkt_per_lmac(txrx_peer, nbuf);
	tid_stats->msdu_cnt++;
	if (qdf_unlikely(qdf_nbuf_is_da_mcbc(nbuf) &&
			 (vdev->rx_decap_type == htt_cmn_pkt_type_ethernet))) {
		eh = (qdf_ether_header_t *)qdf_nbuf_data(nbuf);
		enh_flag = vdev->pdev->enhanced_stats_en;
		DP_PEER_MC_INCC_PKT(txrx_peer, 1, msdu_len, enh_flag);
		tid_stats->mcast_msdu_cnt++;
		if (QDF_IS_ADDR_BROADCAST(eh->ether_dhost)) {
			DP_PEER_BC_INCC_PKT(txrx_peer, 1, msdu_len, enh_flag);
			tid_stats->bcast_msdu_cnt++;
		}
	}

	txrx_peer->stats.per_pkt_stats.rx.last_rx_ts = qdf_system_ticks();

	dp_rx_msdu_extd_stats_update(soc, nbuf, rx_tlv_hdr, txrx_peer);
}

#ifndef WDS_VENDOR_EXTENSION
int dp_wds_rx_policy_check(uint8_t *rx_tlv_hdr,
			   struct dp_vdev *vdev,
			   struct dp_txrx_peer *txrx_peer)
{
	return 1;
}
#endif

#ifdef RX_DESC_DEBUG_CHECK
QDF_STATUS dp_rx_desc_nbuf_sanity_check(struct dp_soc *soc,
					hal_ring_desc_t ring_desc,
					struct dp_rx_desc *rx_desc)
{
	struct hal_buf_info hbi;

	hal_rx_reo_buf_paddr_get(soc->hal_soc, ring_desc, &hbi);
	/* Sanity check for possible buffer paddr corruption */
	if (dp_rx_desc_paddr_sanity_check(rx_desc, (&hbi)->paddr))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_rx_desc_nbuf_len_sanity_check - Add sanity check to catch Rx buffer
 *				      out of bound access from H.W
 *
 * @soc: DP soc
 * @pkt_len: Packet length received from H.W
 *
 * Return: NONE
 */
static inline void
dp_rx_desc_nbuf_len_sanity_check(struct dp_soc *soc,
				 uint32_t pkt_len)
{
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_buf[0];
	qdf_assert_always(pkt_len <= rx_desc_pool->buf_size);
}
#else
static inline void
dp_rx_desc_nbuf_len_sanity_check(struct dp_soc *soc, uint32_t pkt_len) { }
#endif

#ifdef DP_RX_PKT_NO_PEER_DELIVER
#ifdef DP_RX_UDP_OVER_PEER_ROAM
/**
 * dp_rx_is_udp_allowed_over_roam_peer() - check if udp data received
 *					   during roaming
 * @vdev: dp_vdev pointer
 * @rx_tlv_hdr: rx tlv header
 * @nbuf: pkt skb pointer
 *
 * This function will check if rx udp data is received from authorised
 * roamed peer before peer map indication is received from FW after
 * roaming. This is needed for VoIP scenarios in which packet loss
 * expected during roaming is minimal.
 *
 * Return: bool
 */
static bool dp_rx_is_udp_allowed_over_roam_peer(struct dp_vdev *vdev,
						uint8_t *rx_tlv_hdr,
						qdf_nbuf_t nbuf)
{
	char *hdr_desc;
	struct ieee80211_frame *wh = NULL;

	hdr_desc = hal_rx_desc_get_80211_hdr(vdev->pdev->soc->hal_soc,
					     rx_tlv_hdr);
	wh = (struct ieee80211_frame *)hdr_desc;

	if (vdev->roaming_peer_status ==
	    WLAN_ROAM_PEER_AUTH_STATUS_AUTHENTICATED &&
	    !qdf_mem_cmp(vdev->roaming_peer_mac.raw, wh->i_addr2,
	    QDF_MAC_ADDR_SIZE) && (qdf_nbuf_is_ipv4_udp_pkt(nbuf) ||
	    qdf_nbuf_is_ipv6_udp_pkt(nbuf)))
		return true;

	return false;
}
#else
static bool dp_rx_is_udp_allowed_over_roam_peer(struct dp_vdev *vdev,
						uint8_t *rx_tlv_hdr,
						qdf_nbuf_t nbuf)
{
	return false;
}
#endif
void dp_rx_deliver_to_stack_no_peer(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
	uint16_t peer_id;
	uint8_t vdev_id;
	struct dp_vdev *vdev = NULL;
	uint32_t l2_hdr_offset = 0;
	uint16_t msdu_len = 0;
	uint32_t pkt_len = 0;
	uint8_t *rx_tlv_hdr;
	uint32_t frame_mask = FRAME_MASK_IPV4_ARP | FRAME_MASK_IPV4_DHCP |
				FRAME_MASK_IPV4_EAPOL | FRAME_MASK_IPV6_DHCP;
	bool is_special_frame = false;
	struct dp_peer *peer = NULL;

	peer_id = QDF_NBUF_CB_RX_PEER_ID(nbuf);
	if (peer_id > soc->max_peer_id)
		goto deliver_fail;

	vdev_id = QDF_NBUF_CB_RX_VDEV_ID(nbuf);
	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_RX);
	if (!vdev || vdev->delete.pending)
		goto deliver_fail;

	if (qdf_unlikely(qdf_nbuf_is_frag(nbuf)))
		goto deliver_fail;

	rx_tlv_hdr = qdf_nbuf_data(nbuf);
	l2_hdr_offset =
		hal_rx_msdu_end_l3_hdr_padding_get(soc->hal_soc, rx_tlv_hdr);

	msdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);
	pkt_len = msdu_len + l2_hdr_offset + soc->rx_pkt_tlv_size;
	QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(nbuf) = 1;

	qdf_nbuf_set_pktlen(nbuf, pkt_len);
	qdf_nbuf_pull_head(nbuf, soc->rx_pkt_tlv_size + l2_hdr_offset);

	is_special_frame = dp_rx_is_special_frame(nbuf, frame_mask);
	if (qdf_likely(vdev->osif_rx)) {
		if (is_special_frame ||
		    dp_rx_is_udp_allowed_over_roam_peer(vdev, rx_tlv_hdr,
							nbuf)) {
			qdf_nbuf_set_exc_frame(nbuf, 1);
			if (QDF_STATUS_SUCCESS !=
			    vdev->osif_rx(vdev->osif_vdev, nbuf))
				goto deliver_fail;

			DP_STATS_INC(soc, rx.err.pkt_delivered_no_peer, 1);
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);
			return;
		}
	} else if (is_special_frame) {
		/*
		 * If MLO connection, txrx_peer for link peer does not exist,
		 * try to store these RX packets to txrx_peer's bufq of MLD
		 * peer until vdev->osif_rx is registered from CP and flush
		 * them to stack.
		 */
		peer = dp_peer_get_tgt_peer_by_id(soc, peer_id,
						  DP_MOD_ID_RX);
		if (!peer)
			goto deliver_fail;

		/* only check for MLO connection */
		if (IS_MLO_DP_MLD_PEER(peer) && peer->txrx_peer &&
		    dp_rx_is_peer_cache_bufq_supported()) {
			qdf_nbuf_set_exc_frame(nbuf, 1);

			if (QDF_STATUS_SUCCESS ==
			    dp_rx_enqueue_rx(peer, peer->txrx_peer, nbuf)) {
				DP_STATS_INC(soc,
					     rx.err.pkt_delivered_no_peer,
					     1);
			} else {
				DP_STATS_INC(soc,
					     rx.err.rx_invalid_peer.num,
					     1);
			}

			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);
			dp_peer_unref_delete(peer, DP_MOD_ID_RX);
			return;
		}

		dp_peer_unref_delete(peer, DP_MOD_ID_RX);
	}

deliver_fail:
	DP_STATS_INC_PKT(soc, rx.err.rx_invalid_peer, 1,
			 QDF_NBUF_CB_RX_PKT_LEN(nbuf));
	dp_rx_nbuf_free(nbuf);
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);
}
#else
void dp_rx_deliver_to_stack_no_peer(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
	DP_STATS_INC_PKT(soc, rx.err.rx_invalid_peer, 1,
			 QDF_NBUF_CB_RX_PKT_LEN(nbuf));
	dp_rx_nbuf_free(nbuf);
}
#endif

uint32_t dp_rx_srng_get_num_pending(hal_soc_handle_t hal_soc,
				    hal_ring_handle_t hal_ring_hdl,
				    uint32_t num_entries,
				    bool *near_full)
{
	uint32_t num_pending = 0;

	num_pending = hal_srng_dst_num_valid_locked(hal_soc,
						    hal_ring_hdl,
						    true);

	if (num_entries && (num_pending >= num_entries >> 1))
		*near_full = true;
	else
		*near_full = false;

	return num_pending;
}

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

#ifdef WLAN_SUPPORT_RX_FISA
void dp_rx_skip_tlvs(struct dp_soc *soc, qdf_nbuf_t nbuf, uint32_t l3_padding)
{
	QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(nbuf) = l3_padding;
	qdf_nbuf_pull_head(nbuf, l3_padding + soc->rx_pkt_tlv_size);
}
#else
void dp_rx_skip_tlvs(struct dp_soc *soc, qdf_nbuf_t nbuf, uint32_t l3_padding)
{
	qdf_nbuf_pull_head(nbuf, l3_padding + soc->rx_pkt_tlv_size);
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED

#ifdef DP_RX_DROP_RAW_FRM
bool dp_rx_is_raw_frame_dropped(qdf_nbuf_t nbuf)
{
	if (qdf_nbuf_is_raw_frame(nbuf)) {
		dp_rx_nbuf_free(nbuf);
		return true;
	}

	return false;
}
#endif

#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
void
dp_rx_ring_record_entry(struct dp_soc *soc, uint8_t ring_num,
			hal_ring_desc_t ring_desc)
{
	struct dp_buf_info_record *record;
	struct hal_buf_info hbi;
	uint32_t idx;

	if (qdf_unlikely(!soc->rx_ring_history[ring_num]))
		return;

	hal_rx_reo_buf_paddr_get(soc->hal_soc, ring_desc, &hbi);

	/* buffer_addr_info is the first element of ring_desc */
	hal_rx_buf_cookie_rbm_get(soc->hal_soc, (uint32_t *)ring_desc,
				  &hbi);

	idx = dp_history_get_next_index(&soc->rx_ring_history[ring_num]->index,
					DP_RX_HIST_MAX);

	/* No NULL check needed for record since its an array */
	record = &soc->rx_ring_history[ring_num]->entry[idx];

	record->timestamp = qdf_get_log_timestamp();
	record->hbi.paddr = hbi.paddr;
	record->hbi.sw_cookie = hbi.sw_cookie;
	record->hbi.rbm = hbi.rbm;
}
#endif

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
void dp_rx_update_stats(struct dp_soc *soc, qdf_nbuf_t nbuf)
{
	DP_STATS_INC_PKT(soc, rx.ingress, 1,
			 QDF_NBUF_CB_RX_PKT_LEN(nbuf));
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
void dp_rx_deliver_to_pkt_capture(struct dp_soc *soc,  struct dp_pdev *pdev,
				  uint16_t peer_id, uint32_t is_offload,
				  qdf_nbuf_t netbuf)
{
	if (wlan_cfg_get_pkt_capture_mode(soc->wlan_cfg_ctx))
		dp_wdi_event_handler(WDI_EVENT_PKT_CAPTURE_RX_DATA, soc, netbuf,
				     peer_id, is_offload, pdev->pdev_id);
}

void dp_rx_deliver_to_pkt_capture_no_peer(struct dp_soc *soc, qdf_nbuf_t nbuf,
					  uint32_t is_offload)
{
	if (wlan_cfg_get_pkt_capture_mode(soc->wlan_cfg_ctx))
		dp_wdi_event_handler(WDI_EVENT_PKT_CAPTURE_RX_DATA_NO_PEER,
				     soc, nbuf, HTT_INVALID_VDEV,
				     is_offload, 0);
}
#endif

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

QDF_STATUS dp_rx_vdev_detach(struct dp_vdev *vdev)
{
	QDF_STATUS ret;

	if (vdev->osif_rx_flush) {
		ret = vdev->osif_rx_flush(vdev->osif_vdev, vdev->vdev_id);
		if (!QDF_IS_STATUS_SUCCESS(ret)) {
			dp_err("Failed to flush rx pkts for vdev %d\n",
			       vdev->vdev_id);
			return ret;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dp_pdev_nbuf_alloc_and_map(struct dp_soc *dp_soc,
			   struct dp_rx_nbuf_frag_info *nbuf_frag_info_t,
			   struct dp_pdev *dp_pdev,
			   struct rx_desc_pool *rx_desc_pool)
{
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	(nbuf_frag_info_t->virt_addr).nbuf =
		qdf_nbuf_alloc(dp_soc->osdev, rx_desc_pool->buf_size,
			       RX_BUFFER_RESERVATION,
			       rx_desc_pool->buf_alignment, FALSE);
	if (!((nbuf_frag_info_t->virt_addr).nbuf)) {
		dp_err("nbuf alloc failed");
		DP_STATS_INC(dp_pdev, replenish.nbuf_alloc_fail, 1);
		return ret;
	}

	ret = qdf_nbuf_map_nbytes_single(dp_soc->osdev,
					 (nbuf_frag_info_t->virt_addr).nbuf,
					 QDF_DMA_FROM_DEVICE,
					 rx_desc_pool->buf_size);

	if (qdf_unlikely(QDF_IS_STATUS_ERROR(ret))) {
		qdf_nbuf_free((nbuf_frag_info_t->virt_addr).nbuf);
		dp_err("nbuf map failed");
		DP_STATS_INC(dp_pdev, replenish.map_err, 1);
		return ret;
	}

	nbuf_frag_info_t->paddr =
		qdf_nbuf_get_frag_paddr((nbuf_frag_info_t->virt_addr).nbuf, 0);

	ret = dp_check_paddr(dp_soc, &((nbuf_frag_info_t->virt_addr).nbuf),
			     &nbuf_frag_info_t->paddr,
			     rx_desc_pool);
	if (ret == QDF_STATUS_E_FAILURE) {
		dp_err("nbuf check x86 failed");
		DP_STATS_INC(dp_pdev, replenish.x86_fail, 1);
		return ret;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_pdev_rx_buffers_attach(struct dp_soc *dp_soc, uint32_t mac_id,
			  struct dp_srng *dp_rxdma_srng,
			  struct rx_desc_pool *rx_desc_pool,
			  uint32_t num_req_buffers)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(dp_soc, mac_id);
	hal_ring_handle_t rxdma_srng = dp_rxdma_srng->hal_srng;
	union dp_rx_desc_list_elem_t *next;
	void *rxdma_ring_entry;
	qdf_dma_addr_t paddr;
	struct dp_rx_nbuf_frag_info *nf_info;
	uint32_t nr_descs, nr_nbuf = 0, nr_nbuf_total = 0;
	uint32_t buffer_index, nbuf_ptrs_per_page;
	qdf_nbuf_t nbuf;
	QDF_STATUS ret;
	int page_idx, total_pages;
	union dp_rx_desc_list_elem_t *desc_list = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;
	int sync_hw_ptr = 1;
	uint32_t num_entries_avail;

	if (qdf_unlikely(!dp_pdev)) {
		dp_rx_err("%pK: pdev is null for mac_id = %d",
			  dp_soc, mac_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (qdf_unlikely(!rxdma_srng)) {
		DP_STATS_INC(dp_pdev, replenish.rxdma_err, num_req_buffers);
		return QDF_STATUS_E_FAILURE;
	}

	dp_debug("requested %u RX buffers for driver attach", num_req_buffers);

	hal_srng_access_start(dp_soc->hal_soc, rxdma_srng);
	num_entries_avail = hal_srng_src_num_avail(dp_soc->hal_soc,
						   rxdma_srng,
						   sync_hw_ptr);
	hal_srng_access_end(dp_soc->hal_soc, rxdma_srng);

	if (!num_entries_avail) {
		dp_err("Num of available entries is zero, nothing to do");
		return QDF_STATUS_E_NOMEM;
	}

	if (num_entries_avail < num_req_buffers)
		num_req_buffers = num_entries_avail;

	nr_descs = dp_rx_get_free_desc_list(dp_soc, mac_id, rx_desc_pool,
					    num_req_buffers, &desc_list, &tail);
	if (!nr_descs) {
		dp_err("no free rx_descs in freelist");
		DP_STATS_INC(dp_pdev, err.desc_alloc_fail, num_req_buffers);
		return QDF_STATUS_E_NOMEM;
	}

	dp_debug("got %u RX descs for driver attach", nr_descs);

	/*
	 * Try to allocate pointers to the nbuf one page at a time.
	 * Take pointers that can fit in one page of memory and
	 * iterate through the total descriptors that need to be
	 * allocated in order of pages. Reuse the pointers that
	 * have been allocated to fit in one page across each
	 * iteration to index into the nbuf.
	 */
	total_pages = (nr_descs * sizeof(*nf_info)) / DP_BLOCKMEM_SIZE;

	/*
	 * Add an extra page to store the remainder if any
	 */
	if ((nr_descs * sizeof(*nf_info)) % DP_BLOCKMEM_SIZE)
		total_pages++;
	nf_info = qdf_mem_malloc(DP_BLOCKMEM_SIZE);
	if (!nf_info) {
		dp_err("failed to allocate nbuf array");
		DP_STATS_INC(dp_pdev, replenish.rxdma_err, num_req_buffers);
		QDF_BUG(0);
		return QDF_STATUS_E_NOMEM;
	}
	nbuf_ptrs_per_page = DP_BLOCKMEM_SIZE / sizeof(*nf_info);

	for (page_idx = 0; page_idx < total_pages; page_idx++) {
		qdf_mem_zero(nf_info, DP_BLOCKMEM_SIZE);

		for (nr_nbuf = 0; nr_nbuf < nbuf_ptrs_per_page; nr_nbuf++) {
			/*
			 * The last page of buffer pointers may not be required
			 * completely based on the number of descriptors. Below
			 * check will ensure we are allocating only the
			 * required number of descriptors.
			 */
			if (nr_nbuf_total >= nr_descs)
				break;
			/* Flag is set while pdev rx_desc_pool initialization */
			if (qdf_unlikely(rx_desc_pool->rx_mon_dest_frag_enable))
				ret = dp_pdev_frag_alloc_and_map(dp_soc,
						&nf_info[nr_nbuf], dp_pdev,
						rx_desc_pool);
			else
				ret = dp_pdev_nbuf_alloc_and_map(dp_soc,
						&nf_info[nr_nbuf], dp_pdev,
						rx_desc_pool);
			if (QDF_IS_STATUS_ERROR(ret))
				break;

			nr_nbuf_total++;
		}

		hal_srng_access_start(dp_soc->hal_soc, rxdma_srng);

		for (buffer_index = 0; buffer_index < nr_nbuf; buffer_index++) {
			rxdma_ring_entry =
				hal_srng_src_get_next(dp_soc->hal_soc,
						      rxdma_srng);
			qdf_assert_always(rxdma_ring_entry);

			next = desc_list->next;
			paddr = nf_info[buffer_index].paddr;
			nbuf = nf_info[buffer_index].virt_addr.nbuf;

			/* Flag is set while pdev rx_desc_pool initialization */
			if (qdf_unlikely(rx_desc_pool->rx_mon_dest_frag_enable))
				dp_rx_desc_frag_prep(&desc_list->rx_desc,
						     &nf_info[buffer_index]);
			else
				dp_rx_desc_prep(&desc_list->rx_desc,
						&nf_info[buffer_index]);
			desc_list->rx_desc.in_use = 1;
			dp_rx_desc_alloc_dbg_info(&desc_list->rx_desc);
			dp_rx_desc_update_dbg_info(&desc_list->rx_desc,
						   __func__,
						   RX_DESC_REPLENISHED);

			hal_rxdma_buff_addr_info_set(dp_soc->hal_soc ,rxdma_ring_entry, paddr,
						     desc_list->rx_desc.cookie,
						     rx_desc_pool->owner);

			dp_ipa_handle_rx_buf_smmu_mapping(
					dp_soc, nbuf,
					rx_desc_pool->buf_size, true,
					__func__, __LINE__);

			dp_audio_smmu_map(dp_soc->osdev,
					  qdf_mem_paddr_from_dmaaddr(dp_soc->osdev,
								     QDF_NBUF_CB_PADDR(nbuf)),
					  QDF_NBUF_CB_PADDR(nbuf),
					  rx_desc_pool->buf_size);

			desc_list = next;
		}

		dp_rx_refill_ring_record_entry(dp_soc, dp_pdev->lmac_id,
					       rxdma_srng, nr_nbuf, nr_nbuf);
		hal_srng_access_end(dp_soc->hal_soc, rxdma_srng);
	}

	dp_info("filled %u RX buffers for driver attach", nr_nbuf_total);
	qdf_mem_free(nf_info);

	if (!nr_nbuf_total) {
		dp_err("No nbuf's allocated");
		QDF_BUG(0);
		return QDF_STATUS_E_RESOURCES;
	}

	/* No need to count the number of bytes received during replenish.
	 * Therefore set replenish.pkts.bytes as 0.
	 */
	DP_STATS_INC_PKT(dp_pdev, replenish.pkts, nr_nbuf, 0);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(dp_pdev_rx_buffers_attach);

#ifdef DP_RX_MON_MEM_FRAG
void dp_rx_enable_mon_dest_frag(struct rx_desc_pool *rx_desc_pool,
				bool is_mon_dest_desc)
{
	rx_desc_pool->rx_mon_dest_frag_enable = is_mon_dest_desc;
	if (is_mon_dest_desc)
		dp_alert("Feature DP_RX_MON_MEM_FRAG for mon_dest is enabled");
}
#else
void dp_rx_enable_mon_dest_frag(struct rx_desc_pool *rx_desc_pool,
				bool is_mon_dest_desc)
{
	rx_desc_pool->rx_mon_dest_frag_enable = false;
	if (is_mon_dest_desc)
		dp_alert("Feature DP_RX_MON_MEM_FRAG for mon_dest is disabled");
}
#endif

qdf_export_symbol(dp_rx_enable_mon_dest_frag);

QDF_STATUS
dp_rx_pdev_desc_pool_alloc(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	uint32_t rxdma_entries;
	uint32_t rx_sw_desc_num;
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t status = QDF_STATUS_SUCCESS;
	int mac_for_pdev;

	mac_for_pdev = pdev->lmac_id;
	if (wlan_cfg_get_dp_pdev_nss_enabled(pdev->wlan_cfg_ctx)) {
		dp_rx_info("%pK: nss-wifi<4> skip Rx refil %d",
			   soc, mac_for_pdev);
		return status;
	}

	dp_rxdma_srng = &soc->rx_refill_buf_ring[mac_for_pdev];
	rxdma_entries = dp_rxdma_srng->num_entries;

	rx_desc_pool = &soc->rx_desc_buf[mac_for_pdev];
	rx_sw_desc_num = wlan_cfg_get_dp_soc_rx_sw_desc_num(soc->wlan_cfg_ctx);

	rx_desc_pool->desc_type = DP_RX_DESC_BUF_TYPE;
	status = dp_rx_desc_pool_alloc(soc,
				       rx_sw_desc_num,
				       rx_desc_pool);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	return status;
}

void dp_rx_pdev_desc_pool_free(struct dp_pdev *pdev)
{
	int mac_for_pdev = pdev->lmac_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_buf[mac_for_pdev];

	dp_rx_desc_pool_free(soc, rx_desc_pool);
}

QDF_STATUS dp_rx_pdev_desc_pool_init(struct dp_pdev *pdev)
{
	int mac_for_pdev = pdev->lmac_id;
	struct dp_soc *soc = pdev->soc;
	uint32_t rxdma_entries;
	uint32_t rx_sw_desc_num;
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_buf[mac_for_pdev];
	if (wlan_cfg_get_dp_pdev_nss_enabled(pdev->wlan_cfg_ctx)) {
		/*
		 * If NSS is enabled, rx_desc_pool is already filled.
		 * Hence, just disable desc_pool frag flag.
		 */
		dp_rx_enable_mon_dest_frag(rx_desc_pool, false);

		dp_rx_info("%pK: nss-wifi<4> skip Rx refil %d",
			   soc, mac_for_pdev);
		return QDF_STATUS_SUCCESS;
	}

	if (dp_rx_desc_pool_is_allocated(rx_desc_pool) == QDF_STATUS_E_NOMEM)
		return QDF_STATUS_E_NOMEM;

	dp_rxdma_srng = &soc->rx_refill_buf_ring[mac_for_pdev];
	rxdma_entries = dp_rxdma_srng->num_entries;

	soc->process_rx_status = CONFIG_PROCESS_RX_STATUS;

	rx_sw_desc_num =
	wlan_cfg_get_dp_soc_rx_sw_desc_num(soc->wlan_cfg_ctx);

	rx_desc_pool->owner = dp_rx_get_rx_bm_id(soc);
	rx_desc_pool->buf_size = RX_DATA_BUFFER_SIZE;
	rx_desc_pool->buf_alignment = RX_DATA_BUFFER_ALIGNMENT;
	/* Disable monitor dest processing via frag */
	dp_rx_enable_mon_dest_frag(rx_desc_pool, false);

	dp_rx_desc_pool_init(soc, mac_for_pdev,
			     rx_sw_desc_num, rx_desc_pool);
	return QDF_STATUS_SUCCESS;
}

void dp_rx_pdev_desc_pool_deinit(struct dp_pdev *pdev)
{
	int mac_for_pdev = pdev->lmac_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_buf[mac_for_pdev];

	dp_rx_desc_pool_deinit(soc, rx_desc_pool, mac_for_pdev);
}

QDF_STATUS
dp_rx_pdev_buffers_alloc(struct dp_pdev *pdev)
{
	int mac_for_pdev = pdev->lmac_id;
	struct dp_soc *soc = pdev->soc;
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t rxdma_entries;

	dp_rxdma_srng = &soc->rx_refill_buf_ring[mac_for_pdev];
	rxdma_entries = dp_rxdma_srng->num_entries;

	rx_desc_pool = &soc->rx_desc_buf[mac_for_pdev];

	/* Initialize RX buffer pool which will be
	 * used during low memory conditions
	 */
	dp_rx_buffer_pool_init(soc, mac_for_pdev);

	return dp_pdev_rx_buffers_attach_simple(soc, mac_for_pdev,
						dp_rxdma_srng,
						rx_desc_pool,
						rxdma_entries - 1);
}

void
dp_rx_pdev_buffers_free(struct dp_pdev *pdev)
{
	int mac_for_pdev = pdev->lmac_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_buf[mac_for_pdev];

	dp_rx_desc_nbuf_free(soc, rx_desc_pool, false);
	dp_rx_buffer_pool_deinit(soc, mac_for_pdev);
}

#ifdef DP_RX_SPECIAL_FRAME_NEED
bool dp_rx_deliver_special_frame(struct dp_soc *soc,
				 struct dp_txrx_peer *txrx_peer,
				 qdf_nbuf_t nbuf, uint32_t frame_mask,
				 uint8_t *rx_tlv_hdr)
{
	uint32_t l2_hdr_offset = 0;
	uint16_t msdu_len = 0;
	uint32_t skip_len;

	l2_hdr_offset =
		hal_rx_msdu_end_l3_hdr_padding_get(soc->hal_soc, rx_tlv_hdr);

	if (qdf_unlikely(qdf_nbuf_is_frag(nbuf))) {
		skip_len = l2_hdr_offset;
	} else {
		msdu_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);
		skip_len = l2_hdr_offset + soc->rx_pkt_tlv_size;
		qdf_nbuf_set_pktlen(nbuf, msdu_len + skip_len);
	}

	QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(nbuf) = 1;
	dp_rx_set_hdr_pad(nbuf, l2_hdr_offset);
	qdf_nbuf_pull_head(nbuf, skip_len);

	if (txrx_peer->vdev) {
		dp_rx_send_pktlog(soc, txrx_peer->vdev->pdev, nbuf,
				  QDF_TX_RX_STATUS_OK);
	}

	if (dp_rx_is_special_frame(nbuf, frame_mask)) {
		dp_info("special frame, mpdu sn 0x%x",
			hal_rx_get_rx_sequence(soc->hal_soc, rx_tlv_hdr));
		qdf_nbuf_set_exc_frame(nbuf, 1);
		dp_rx_deliver_to_stack(soc, txrx_peer->vdev, txrx_peer,
				       nbuf, NULL);
		return true;
	}

	return false;
}
#endif

#ifdef WLAN_FEATURE_MARK_FIRST_WAKEUP_PACKET
void dp_rx_mark_first_packet_after_wow_wakeup(struct dp_pdev *pdev,
					      uint8_t *rx_tlv,
					      qdf_nbuf_t nbuf)
{
	struct dp_soc *soc;

	if (!pdev->is_first_wakeup_packet)
		return;

	soc = pdev->soc;
	if (hal_get_first_wow_wakeup_packet(soc->hal_soc, rx_tlv)) {
		qdf_nbuf_mark_wakeup_frame(nbuf);
		dp_info("First packet after WOW Wakeup rcvd");
	}
}
#endif
