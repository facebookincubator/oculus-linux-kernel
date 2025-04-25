/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "dp_tx_desc.h"

#ifndef DESC_PARTITION
#define DP_TX_DESC_SIZE(a) qdf_get_pwr2(a)
#define DP_TX_DESC_PAGE_DIVIDER(soc, num_desc_per_page, pool_id)     \
do {                                                                 \
	uint8_t sig_bit;                                             \
	soc->tx_desc[pool_id].offset_filter = num_desc_per_page - 1; \
	/* Calculate page divider to find page number */             \
	sig_bit = 0;                                                 \
	while (num_desc_per_page) {                                  \
		sig_bit++;                                           \
		num_desc_per_page = num_desc_per_page >> 1;          \
	}                                                            \
	soc->tx_desc[pool_id].page_divider = (sig_bit - 1);          \
} while (0)
#else
#define DP_TX_DESC_SIZE(a) a
#define DP_TX_DESC_PAGE_DIVIDER(soc, num_desc_per_page, pool_id) {}
#endif /* DESC_PARTITION */

/**
 * dp_tx_desc_pool_counter_initialize() - Initialize counters
 * @tx_desc_pool: Handle to DP tx_desc_pool structure
 * @num_elem: Number of descriptor elements per pool
 *
 * Return: None
 */
#ifdef QCA_LL_TX_FLOW_CONTROL_V2
static void
dp_tx_desc_pool_counter_initialize(struct dp_tx_desc_pool_s *tx_desc_pool,
				  uint16_t num_elem)
{
}
#else
static void
dp_tx_desc_pool_counter_initialize(struct dp_tx_desc_pool_s *tx_desc_pool,
				  uint16_t num_elem)
{
	tx_desc_pool->elem_count = num_elem;
	tx_desc_pool->num_free = num_elem;
	tx_desc_pool->num_allocated = 0;
}
#endif

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_tx_desc_clean_up() - Clean up the tx descriptors
 * @ctxt: context passed
 * @elem: element to be cleaned up
 * @elem_list: element list
 *
 */
static void dp_tx_desc_clean_up(void *ctxt, void *elem, void *elem_list)
{
	struct dp_soc *soc = (struct dp_soc *)ctxt;
	struct dp_tx_desc_s *tx_desc = (struct dp_tx_desc_s *)elem;
	qdf_nbuf_t *nbuf_list = (qdf_nbuf_t *)elem_list;
	qdf_nbuf_t nbuf = NULL;

	if (tx_desc->nbuf) {
		nbuf = dp_tx_comp_free_buf(soc, tx_desc, true);
		dp_tx_desc_release(soc, tx_desc, tx_desc->pool_id);

		if (nbuf) {
			if (!nbuf_list) {
				dp_err("potential memory leak");
				qdf_assert_always(0);
			}

			nbuf->next = *nbuf_list;
			*nbuf_list = nbuf;
		}
	}
}

void dp_tx_desc_pool_cleanup(struct dp_soc *soc, qdf_nbuf_t *nbuf_list,
			     bool cleanup)
{
	int i;
	struct dp_tx_desc_pool_s *tx_desc_pool = NULL;
	uint8_t num_pool = wlan_cfg_get_num_tx_desc_pool(soc->wlan_cfg_ctx);

	if (!cleanup)
		return;

	for (i = 0; i < num_pool; i++) {
		tx_desc_pool = dp_get_tx_desc_pool(soc, i);

		TX_DESC_LOCK_LOCK(&tx_desc_pool->lock);
		if (tx_desc_pool)
			qdf_tx_desc_pool_free_bufs(soc,
						   &tx_desc_pool->desc_pages,
						   tx_desc_pool->elem_size,
						   tx_desc_pool->elem_count,
						   true, &dp_tx_desc_clean_up,
						   nbuf_list);

		TX_DESC_LOCK_UNLOCK(&tx_desc_pool->lock);

		tx_desc_pool = dp_get_spcl_tx_desc_pool(soc, i);
		TX_DESC_LOCK_LOCK(&tx_desc_pool->lock);

		if (tx_desc_pool)
			qdf_tx_desc_pool_free_bufs(soc,
						   &tx_desc_pool->desc_pages,
						   tx_desc_pool->elem_size,
						   tx_desc_pool->elem_count,
						   true, &dp_tx_desc_clean_up,
						   nbuf_list);

		TX_DESC_LOCK_UNLOCK(&tx_desc_pool->lock);
	}
}
#endif

#ifdef QCA_SUPPORT_DP_GLOBAL_CTX
static void dp_tx_desc_pool_alloc_mem(struct dp_soc *soc, int8_t pool_id,
				      bool spcl_tx_desc)
{
	struct dp_global_context *dp_global = NULL;

	dp_global = wlan_objmgr_get_global_ctx();

	if (spcl_tx_desc) {
		dp_global->spcl_tx_desc[soc->arch_id][pool_id] =
			qdf_mem_malloc(sizeof(struct dp_tx_desc_pool_s));
	} else {
		dp_global->tx_desc[soc->arch_id][pool_id] =
			qdf_mem_malloc(sizeof(struct dp_tx_desc_pool_s));
	}
}

static void dp_tx_desc_pool_free_mem(struct dp_soc *soc, int8_t pool_id,
				     bool spcl_tx_desc)
{
	struct dp_global_context *dp_global = NULL;

	dp_global = wlan_objmgr_get_global_ctx();
	if (spcl_tx_desc) {
		if (!dp_global->spcl_tx_desc[soc->arch_id][pool_id])
			return;

		qdf_mem_free(dp_global->spcl_tx_desc[soc->arch_id][pool_id]);
		dp_global->spcl_tx_desc[soc->arch_id][pool_id] = NULL;
	} else {
		if (!dp_global->tx_desc[soc->arch_id][pool_id])
			return;

		qdf_mem_free(dp_global->tx_desc[soc->arch_id][pool_id]);
		dp_global->tx_desc[soc->arch_id][pool_id] = NULL;
	}
}
#else
static void dp_tx_desc_pool_alloc_mem(struct dp_soc *soc, int8_t pool_id,
				      bool spcl_tx_desc)
{
}

static void dp_tx_desc_pool_free_mem(struct dp_soc *soc, int8_t pool_id,
				     bool spcl_tx_desc)
{
}
#endif

QDF_STATUS dp_tx_desc_pool_alloc(struct dp_soc *soc, uint8_t pool_id,
				 uint32_t num_elem, bool spcl_tx_desc)
{
	uint32_t desc_size, num_elem_t;
	struct dp_tx_desc_pool_s *tx_desc_pool;
	QDF_STATUS status;
	enum qdf_dp_desc_type desc_type = QDF_DP_TX_DESC_TYPE;

	desc_size = DP_TX_DESC_SIZE(sizeof(struct dp_tx_desc_s));

	dp_tx_desc_pool_alloc_mem(soc, pool_id, spcl_tx_desc);
	if (spcl_tx_desc) {
		tx_desc_pool = dp_get_spcl_tx_desc_pool(soc, pool_id);
		desc_type = QDF_DP_TX_SPCL_DESC_TYPE;
		num_elem_t = num_elem;
	} else {
		tx_desc_pool = dp_get_tx_desc_pool(soc, pool_id);
		desc_type = QDF_DP_TX_DESC_TYPE;
		num_elem_t = dp_get_updated_tx_desc(soc->ctrl_psoc, pool_id, num_elem);
	}

	tx_desc_pool->desc_pages.page_size = DP_BLOCKMEM_SIZE;
	dp_desc_multi_pages_mem_alloc(soc, desc_type,
				      &tx_desc_pool->desc_pages,
				      desc_size, num_elem_t,
				      0, true);

	if (!tx_desc_pool->desc_pages.num_pages) {
		dp_err("Multi page alloc fail, tx desc");
		return QDF_STATUS_E_NOMEM;
	}

	/* Arch specific TX descriptor allocation */
	status = soc->arch_ops.dp_tx_desc_pool_alloc(soc, num_elem_t, pool_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("failed to allocate arch specific descriptors");
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

void dp_tx_desc_pool_free(struct dp_soc *soc, uint8_t pool_id,
			  bool spcl_tx_desc)
{
	struct dp_tx_desc_pool_s *tx_desc_pool;
	enum qdf_dp_desc_type desc_type = QDF_DP_TX_DESC_TYPE;

	if (spcl_tx_desc) {
		tx_desc_pool = dp_get_spcl_tx_desc_pool(soc, pool_id);
		desc_type = QDF_DP_TX_SPCL_DESC_TYPE;
	} else {
		tx_desc_pool = dp_get_tx_desc_pool(soc, pool_id);
		desc_type = QDF_DP_TX_DESC_TYPE;
	}

	if (tx_desc_pool->desc_pages.num_pages)
		dp_desc_multi_pages_mem_free(soc, QDF_DP_TX_DESC_TYPE,
					     &tx_desc_pool->desc_pages, 0,
					     true);

	/* Free arch specific TX descriptor */
	soc->arch_ops.dp_tx_desc_pool_free(soc, pool_id);
	dp_tx_desc_pool_free_mem(soc, pool_id, spcl_tx_desc);
}

QDF_STATUS dp_tx_desc_pool_init(struct dp_soc *soc, uint8_t pool_id,
				uint32_t num_elem, bool spcl_tx_desc)
{
	struct dp_tx_desc_pool_s *tx_desc_pool = NULL;
	uint32_t desc_size, num_elem_t;

	desc_size = DP_TX_DESC_SIZE(sizeof(struct dp_tx_desc_s));

	if (spcl_tx_desc) {
		tx_desc_pool = dp_get_spcl_tx_desc_pool(soc, pool_id);
		num_elem_t = num_elem;
	} else {
		tx_desc_pool = dp_get_tx_desc_pool(soc, pool_id);
		num_elem_t = dp_get_updated_tx_desc(soc->ctrl_psoc, pool_id, num_elem);
	}
	if (qdf_mem_multi_page_link(soc->osdev,
				    &tx_desc_pool->desc_pages,
				    desc_size, num_elem_t, true)) {
		dp_err("invalid tx desc allocation -overflow num link");
		return QDF_STATUS_E_FAULT;
	}

	tx_desc_pool->freelist = (struct dp_tx_desc_s *)
		*tx_desc_pool->desc_pages.cacheable_pages;
	/* Set unique IDs for each Tx descriptor */
	if (QDF_STATUS_SUCCESS != soc->arch_ops.dp_tx_desc_pool_init(
						soc, num_elem_t,
						pool_id, spcl_tx_desc)) {
		dp_err("initialization per target failed");
		return QDF_STATUS_E_FAULT;
	}

	tx_desc_pool->elem_size = DP_TX_DESC_SIZE(sizeof(struct dp_tx_desc_s));

	dp_tx_desc_pool_counter_initialize(tx_desc_pool, num_elem_t);
	TX_DESC_LOCK_CREATE(&tx_desc_pool->lock);

	return QDF_STATUS_SUCCESS;
}

void dp_tx_desc_pool_deinit(struct dp_soc *soc, uint8_t pool_id,
			    bool spcl_tx_desc)
{
	struct dp_tx_desc_pool_s *tx_desc_pool;

	if (spcl_tx_desc)
		tx_desc_pool = dp_get_spcl_tx_desc_pool(soc, pool_id);
	else
		tx_desc_pool = dp_get_tx_desc_pool(soc, pool_id);
	soc->arch_ops.dp_tx_desc_pool_deinit(soc, tx_desc_pool,
					     pool_id, spcl_tx_desc);
	TX_DESC_POOL_MEMBER_CLEAN(tx_desc_pool);
	TX_DESC_LOCK_DESTROY(&tx_desc_pool->lock);
}

QDF_STATUS
dp_tx_ext_desc_pool_alloc_by_id(struct dp_soc *soc, uint32_t num_elem,
				uint8_t pool_id)
{
	QDF_STATUS status;
	qdf_dma_context_t memctx = 0;
	uint16_t elem_size = HAL_TX_EXT_DESC_WITH_META_DATA;
	struct dp_tx_ext_desc_pool_s *dp_tx_ext_desc_pool;
	uint16_t link_elem_size = sizeof(struct dp_tx_ext_desc_elem_s);

	dp_tx_ext_desc_pool = &((soc)->tx_ext_desc[pool_id]);
	memctx = qdf_get_dma_mem_context(dp_tx_ext_desc_pool, memctx);

	/* Coherent tx extension descriptor alloc */
	dp_desc_multi_pages_mem_alloc(soc, QDF_DP_TX_EXT_DESC_TYPE,
				      &dp_tx_ext_desc_pool->desc_pages,
				      elem_size, num_elem, memctx, false);

	if (!dp_tx_ext_desc_pool->desc_pages.num_pages) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "ext desc page alloc fail");
		return QDF_STATUS_E_NOMEM;
	}

	/*
	 * Cacheable ext descriptor link alloc
	 * This structure also large size already
	 * single element is 24bytes, 2K elements are 48Kbytes
	 * Have to alloc multi page cacheable memory
	 */
	dp_desc_multi_pages_mem_alloc(soc, QDF_DP_TX_EXT_DESC_LINK_TYPE,
				      &dp_tx_ext_desc_pool->desc_link_pages,
				      link_elem_size, num_elem, 0, true);

	if (!dp_tx_ext_desc_pool->desc_link_pages.num_pages) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "ext link desc page alloc fail");
		status = QDF_STATUS_E_NOMEM;
		goto free_ext_desc;
	}

	return QDF_STATUS_SUCCESS;

free_ext_desc:
	dp_desc_multi_pages_mem_free(soc, QDF_DP_TX_EXT_DESC_TYPE,
				     &dp_tx_ext_desc_pool->desc_pages,
				     memctx, false);
	return status;
}

QDF_STATUS dp_tx_ext_desc_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
				     uint32_t num_elem)
{
	QDF_STATUS status;
	uint8_t pool_id, count;

	for (pool_id = 0; pool_id < num_pool; pool_id++) {
		status = dp_tx_ext_desc_pool_alloc_by_id(soc, num_elem, pool_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_err("failed to allocate tx ext desc pool %d", pool_id);
			goto free_ext_desc_pool;
		}
	}

	return QDF_STATUS_SUCCESS;

free_ext_desc_pool:
	for (count = 0; count < pool_id; count++)
		dp_tx_ext_desc_pool_free_by_id(soc, count);

	return status;
}

QDF_STATUS dp_tx_ext_desc_pool_init_by_id(struct dp_soc *soc, uint32_t num_elem,
					  uint8_t pool_id)
{
	uint32_t i;
	struct dp_tx_ext_desc_elem_s *c_elem, *p_elem;
	struct qdf_mem_dma_page_t *page_info;
	struct qdf_mem_multi_page_t *pages;
	struct dp_tx_ext_desc_pool_s *dp_tx_ext_desc_pool;
	QDF_STATUS status;

	/* link tx descriptors into a freelist */
	dp_tx_ext_desc_pool = &((soc)->tx_ext_desc[pool_id]);
	soc->tx_ext_desc[pool_id].elem_size =
		HAL_TX_EXT_DESC_WITH_META_DATA;
	soc->tx_ext_desc[pool_id].link_elem_size =
		sizeof(struct dp_tx_ext_desc_elem_s);
	soc->tx_ext_desc[pool_id].elem_count = num_elem;

	dp_tx_ext_desc_pool->freelist = (struct dp_tx_ext_desc_elem_s *)
		*dp_tx_ext_desc_pool->desc_link_pages.cacheable_pages;

	if (qdf_mem_multi_page_link(soc->osdev,
				    &dp_tx_ext_desc_pool->desc_link_pages,
				    dp_tx_ext_desc_pool->link_elem_size,
				    dp_tx_ext_desc_pool->elem_count,
				    true)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "ext link desc page linking fail");
		status = QDF_STATUS_E_FAULT;
		goto fail;
	}

	/* Assign coherent memory pointer into linked free list */
	pages = &dp_tx_ext_desc_pool->desc_pages;
	page_info = dp_tx_ext_desc_pool->desc_pages.dma_pages;
	c_elem = dp_tx_ext_desc_pool->freelist;
	p_elem = c_elem;
	for (i = 0; i < dp_tx_ext_desc_pool->elem_count; i++) {
		if (!(i % pages->num_element_per_page)) {
		/**
		 * First element for new page,
		 * should point next page
		 */
			if (!pages->dma_pages->page_v_addr_start) {
				QDF_TRACE(QDF_MODULE_ID_DP,
					  QDF_TRACE_LEVEL_ERROR,
					  "link over flow");
				status = QDF_STATUS_E_FAULT;
				goto fail;
			}

			c_elem->vaddr =
				(void *)page_info->page_v_addr_start;
			c_elem->paddr = page_info->page_p_addr;
			page_info++;
		} else {
			c_elem->vaddr = (void *)(p_elem->vaddr +
				dp_tx_ext_desc_pool->elem_size);
			c_elem->paddr = (p_elem->paddr +
				dp_tx_ext_desc_pool->elem_size);
		}
		p_elem = c_elem;
		c_elem = c_elem->next;
		if (!c_elem)
			break;
	}
	dp_tx_ext_desc_pool->num_free = num_elem;
	qdf_spinlock_create(&dp_tx_ext_desc_pool->lock);

	return QDF_STATUS_SUCCESS;

fail:
	return status;
}

QDF_STATUS dp_tx_ext_desc_pool_init(struct dp_soc *soc, uint8_t num_pool,
				    uint32_t num_elem)
{
	uint8_t pool_id;
	QDF_STATUS status;

	for (pool_id = 0; pool_id < num_pool; pool_id++) {
		status = dp_tx_ext_desc_pool_init_by_id(soc, num_elem, pool_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_err("failed to init ext desc pool %d", pool_id);
			goto fail;
		}
	}

	return QDF_STATUS_SUCCESS;
fail:
	return status;
}

void dp_tx_ext_desc_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id)
{
	struct dp_tx_ext_desc_pool_s *dp_tx_ext_desc_pool;
	qdf_dma_context_t memctx = 0;

	dp_tx_ext_desc_pool = &((soc)->tx_ext_desc[pool_id]);
	memctx = qdf_get_dma_mem_context(dp_tx_ext_desc_pool, memctx);

	dp_desc_multi_pages_mem_free(soc, QDF_DP_TX_EXT_DESC_LINK_TYPE,
				     &dp_tx_ext_desc_pool->desc_link_pages,
				     0, true);

	dp_desc_multi_pages_mem_free(soc, QDF_DP_TX_EXT_DESC_TYPE,
				     &dp_tx_ext_desc_pool->desc_pages,
				     memctx, false);
}

void dp_tx_ext_desc_pool_free(struct dp_soc *soc, uint8_t num_pool)
{
	uint8_t pool_id;

	for (pool_id = 0; pool_id < num_pool; pool_id++)
		dp_tx_ext_desc_pool_free_by_id(soc, pool_id);
}

void dp_tx_ext_desc_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id)
{
	struct dp_tx_ext_desc_pool_s *dp_tx_ext_desc_pool;

	dp_tx_ext_desc_pool = &((soc)->tx_ext_desc[pool_id]);
	qdf_spinlock_destroy(&dp_tx_ext_desc_pool->lock);
}

void dp_tx_ext_desc_pool_deinit(struct dp_soc *soc, uint8_t num_pool)
{
	uint8_t pool_id;

	for (pool_id = 0; pool_id < num_pool; pool_id++)
		dp_tx_ext_desc_pool_deinit_by_id(soc, pool_id);
}

#if defined(FEATURE_TSO)
QDF_STATUS dp_tx_tso_desc_pool_alloc_by_id(struct dp_soc *soc, uint32_t num_elem,
					   uint8_t pool_id)
{
	struct dp_tx_tso_seg_pool_s *tso_desc_pool;
	uint32_t desc_size;

	desc_size = DP_TX_DESC_SIZE(sizeof(struct qdf_tso_seg_elem_t));

	tso_desc_pool = &soc->tx_tso_desc[pool_id];
	tso_desc_pool->num_free = 0;
	dp_desc_multi_pages_mem_alloc(soc, QDF_DP_TX_TSO_DESC_TYPE,
				      &tso_desc_pool->desc_pages,
				      desc_size, num_elem, 0, true);
	if (!tso_desc_pool->desc_pages.num_pages) {
		dp_err("Multi page alloc fail, tx desc");
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_desc_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
				     uint32_t num_elem)
{
	uint32_t pool_id, i;
	QDF_STATUS status;

	for (pool_id = 0; pool_id < num_pool; pool_id++) {
		status = dp_tx_tso_desc_pool_alloc_by_id(soc, num_elem,
							 pool_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_err("failed to allocate TSO desc pool %d", pool_id);
			goto fail;
		}
	}

	return QDF_STATUS_SUCCESS;

fail:
	for (i = 0; i < pool_id; i++)
		dp_tx_tso_desc_pool_free_by_id(soc, i);

	return QDF_STATUS_E_NOMEM;
}

void dp_tx_tso_desc_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id)
{
	struct dp_tx_tso_seg_pool_s *tso_desc_pool;

	tso_desc_pool = &soc->tx_tso_desc[pool_id];
	dp_desc_multi_pages_mem_free(soc, QDF_DP_TX_TSO_DESC_TYPE,
				     &tso_desc_pool->desc_pages,
				     0, true);
}

void dp_tx_tso_desc_pool_free(struct dp_soc *soc, uint8_t num_pool)
{
	uint32_t pool_id;

	for (pool_id = 0; pool_id < num_pool; pool_id++)
		dp_tx_tso_desc_pool_free_by_id(soc, pool_id);
}

QDF_STATUS dp_tx_tso_desc_pool_init_by_id(struct dp_soc *soc, uint32_t num_elem,
					  uint8_t pool_id)
{
	struct dp_tx_tso_seg_pool_s *tso_desc_pool;
	uint32_t desc_size;

	desc_size = DP_TX_DESC_SIZE(sizeof(struct qdf_tso_seg_elem_t));

	tso_desc_pool = &soc->tx_tso_desc[pool_id];

	if (qdf_mem_multi_page_link(soc->osdev,
				    &tso_desc_pool->desc_pages,
				    desc_size,
				    num_elem, true)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "invalid tso desc allocation - overflow num link");
		return QDF_STATUS_E_FAULT;
	}

	tso_desc_pool->freelist = (struct qdf_tso_seg_elem_t *)
		*tso_desc_pool->desc_pages.cacheable_pages;
	tso_desc_pool->num_free = num_elem;

	TSO_DEBUG("Number of free descriptors: %u\n",
		  tso_desc_pool->num_free);
	tso_desc_pool->pool_size = num_elem;
	qdf_spinlock_create(&tso_desc_pool->lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_desc_pool_init(struct dp_soc *soc, uint8_t num_pool,
				    uint32_t num_elem)
{
	QDF_STATUS status;
	uint32_t pool_id;

	for (pool_id = 0; pool_id < num_pool; pool_id++) {
		status = dp_tx_tso_desc_pool_init_by_id(soc, num_elem,
							pool_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_err("failed to initialise TSO desc pool %d", pool_id);
			return status;
		}
	}

	return QDF_STATUS_SUCCESS;
}

void dp_tx_tso_desc_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id)
{
	struct dp_tx_tso_seg_pool_s *tso_desc_pool;

	tso_desc_pool = &soc->tx_tso_desc[pool_id];

	if (tso_desc_pool->pool_size) {
		qdf_spin_lock_bh(&tso_desc_pool->lock);
		tso_desc_pool->freelist = NULL;
		tso_desc_pool->num_free = 0;
		tso_desc_pool->pool_size = 0;
		qdf_spin_unlock_bh(&tso_desc_pool->lock);
		qdf_spinlock_destroy(&tso_desc_pool->lock);
	}
}

void dp_tx_tso_desc_pool_deinit(struct dp_soc *soc, uint8_t num_pool)
{
	uint32_t pool_id;

	for (pool_id = 0; pool_id < num_pool; pool_id++)
		dp_tx_tso_desc_pool_deinit_by_id(soc, pool_id);
}

QDF_STATUS dp_tx_tso_num_seg_pool_alloc_by_id(struct dp_soc *soc,
					      uint32_t num_elem,
					      uint8_t pool_id)
{
	struct dp_tx_tso_num_seg_pool_s *tso_num_seg_pool;
	uint32_t desc_size;

	desc_size = DP_TX_DESC_SIZE(sizeof(struct qdf_tso_num_seg_elem_t));

	tso_num_seg_pool = &soc->tx_tso_num_seg[pool_id];
	tso_num_seg_pool->num_free = 0;
	dp_desc_multi_pages_mem_alloc(soc, QDF_DP_TX_TSO_NUM_SEG_TYPE,
				      &tso_num_seg_pool->desc_pages,
				      desc_size,
				      num_elem, 0, true);

	if (!tso_num_seg_pool->desc_pages.num_pages) {
		dp_err("Multi page alloc fail, tso_num_seg_pool");
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_num_seg_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
					uint32_t num_elem)
{
	uint32_t pool_id, i;
	QDF_STATUS status;

	for (pool_id = 0; pool_id < num_pool; pool_id++) {
		status = dp_tx_tso_num_seg_pool_alloc_by_id(soc, num_elem,
							    pool_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_err("failed to allocate TSO num seg pool %d", pool_id);
			goto fail;
		}
	}

	return QDF_STATUS_SUCCESS;

fail:
	for (i = 0; i < pool_id; i++)
		dp_tx_tso_num_seg_pool_free_by_id(soc, pool_id);

	return QDF_STATUS_E_NOMEM;
}

void dp_tx_tso_num_seg_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id)
{
	struct dp_tx_tso_num_seg_pool_s *tso_num_seg_pool;

	tso_num_seg_pool = &soc->tx_tso_num_seg[pool_id];
	dp_desc_multi_pages_mem_free(soc, QDF_DP_TX_TSO_NUM_SEG_TYPE,
				     &tso_num_seg_pool->desc_pages,
				     0, true);
}

void dp_tx_tso_num_seg_pool_free(struct dp_soc *soc, uint8_t num_pool)
{
	uint32_t pool_id;

	for (pool_id = 0; pool_id < num_pool; pool_id++)
		dp_tx_tso_num_seg_pool_free_by_id(soc, pool_id);
}

QDF_STATUS
dp_tx_tso_num_seg_pool_init_by_id(struct dp_soc *soc, uint32_t num_elem,
				  uint8_t pool_id)
{
	struct dp_tx_tso_num_seg_pool_s *tso_num_seg_pool;
	uint32_t desc_size;

	desc_size = DP_TX_DESC_SIZE(sizeof(struct qdf_tso_num_seg_elem_t));
	tso_num_seg_pool = &soc->tx_tso_num_seg[pool_id];

	if (qdf_mem_multi_page_link(soc->osdev,
				    &tso_num_seg_pool->desc_pages,
				    desc_size,
				    num_elem, true)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "invalid tso desc allocation - overflow num link");
		return QDF_STATUS_E_FAULT;
	}

	tso_num_seg_pool->freelist = (struct qdf_tso_num_seg_elem_t *)
		*tso_num_seg_pool->desc_pages.cacheable_pages;
	tso_num_seg_pool->num_free = num_elem;
	tso_num_seg_pool->num_seg_pool_size = num_elem;

	qdf_spinlock_create(&tso_num_seg_pool->lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_num_seg_pool_init(struct dp_soc *soc, uint8_t num_pool,
				       uint32_t num_elem)
{
	uint32_t pool_id;
	QDF_STATUS status;

	for (pool_id = 0; pool_id < num_pool; pool_id++) {
		status = dp_tx_tso_num_seg_pool_init_by_id(soc, num_elem,
							   pool_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			dp_err("failed to initialise TSO num seg pool %d", pool_id);
			return status;
		}
	}

	return QDF_STATUS_SUCCESS;
}

void dp_tx_tso_num_seg_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id)
{
	struct dp_tx_tso_num_seg_pool_s *tso_num_seg_pool;

	tso_num_seg_pool = &soc->tx_tso_num_seg[pool_id];

	if (tso_num_seg_pool->num_seg_pool_size) {
		qdf_spin_lock_bh(&tso_num_seg_pool->lock);
		tso_num_seg_pool->freelist = NULL;
		tso_num_seg_pool->num_free = 0;
		tso_num_seg_pool->num_seg_pool_size = 0;
		qdf_spin_unlock_bh(&tso_num_seg_pool->lock);
		qdf_spinlock_destroy(&tso_num_seg_pool->lock);
	}
}

void dp_tx_tso_num_seg_pool_deinit(struct dp_soc *soc, uint8_t num_pool)
{
	uint32_t pool_id;

	for (pool_id = 0; pool_id < num_pool; pool_id++)
		dp_tx_tso_num_seg_pool_deinit_by_id(soc, pool_id);
}
#else
QDF_STATUS dp_tx_tso_desc_pool_alloc_by_id(struct dp_soc *soc, uint32_t num_elem,
					   uint8_t pool_id)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_desc_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
				     uint32_t num_elem)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_desc_pool_init_by_id(struct dp_soc *soc, uint32_t num_elem,
					  uint8_t pool_id)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_desc_pool_init(struct dp_soc *soc, uint8_t num_pool,
				    uint32_t num_elem)
{
	return QDF_STATUS_SUCCESS;
}

void dp_tx_tso_desc_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id)
{
}

void dp_tx_tso_desc_pool_free(struct dp_soc *soc, uint8_t num_pool)
{
}

void dp_tx_tso_desc_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id)
{
}

void dp_tx_tso_desc_pool_deinit(struct dp_soc *soc, uint8_t num_pool)
{
}

QDF_STATUS dp_tx_tso_num_seg_pool_alloc_by_id(struct dp_soc *soc,
					      uint32_t num_elem,
					      uint8_t pool_id)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_num_seg_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
					uint32_t num_elem)
{
	return QDF_STATUS_SUCCESS;
}

void dp_tx_tso_num_seg_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id)
{
}

void dp_tx_tso_num_seg_pool_free(struct dp_soc *soc, uint8_t num_pool)
{
}

QDF_STATUS
dp_tx_tso_num_seg_pool_init_by_id(struct dp_soc *soc, uint32_t num_elem,
				  uint8_t pool_id)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_tx_tso_num_seg_pool_init(struct dp_soc *soc, uint8_t num_pool,
				       uint32_t num_elem)
{
	return QDF_STATUS_SUCCESS;
}

void dp_tx_tso_num_seg_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id)
{
}

void dp_tx_tso_num_seg_pool_deinit(struct dp_soc *soc, uint8_t num_pool)
{
}
#endif
