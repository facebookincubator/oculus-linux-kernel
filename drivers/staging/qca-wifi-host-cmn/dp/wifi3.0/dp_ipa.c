/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifdef IPA_OFFLOAD

#include <wlan_ipa_ucfg_api.h>
#include <wlan_ipa_core.h>
#include <qdf_ipa_wdi3.h>
#include <qdf_types.h>
#include <qdf_lock.h>
#include <hal_hw_headers.h>
#include <hal_api.h>
#include <hal_reo.h>
#include <hif.h>
#include <htt.h>
#include <wdi_event.h>
#include <queue.h>
#include "dp_types.h"
#include "dp_htt.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "dp_ipa.h"
#include "dp_internal.h"
#ifdef WIFI_MONITOR_SUPPORT
#include "dp_mon.h"
#endif
#ifdef FEATURE_WDS
#include "dp_txrx_wds.h"
#endif
#ifdef QCA_IPA_LL_TX_FLOW_CONTROL
#include <pld_common.h>
#endif

/* Hard coded config parameters until dp_ops_cfg.cfg_attach implemented */
#define CFG_IPA_UC_TX_BUF_SIZE_DEFAULT            (2048)

/* WAR for IPA_OFFLOAD case. In some cases, its observed that WBM tries to
 * release a buffer into WBM2SW RELEASE ring for IPA, and the ring is full.
 * This causes back pressure, resulting in a FW crash.
 * By leaving some entries with no buffer attached, WBM will be able to write
 * to the ring, and from dumps we can figure out the buffer which is causing
 * this issue.
 */
#define DP_IPA_WAR_WBM2SW_REL_RING_NO_BUF_ENTRIES 16

/**
 * struct dp_ipa_reo_remap_record - history for dp ipa reo remaps
 * @timestamp: Timestamp when remap occurs
 * @ix0_reg: reo destination ring IX0 value
 * @ix2_reg: reo destination ring IX2 value
 * @ix3_reg: reo destination ring IX3 value
 */
struct dp_ipa_reo_remap_record {
	uint64_t timestamp;
	uint32_t ix0_reg;
	uint32_t ix2_reg;
	uint32_t ix3_reg;
};

#define WLAN_IPA_AST_META_DATA_MASK htonl(0x000000FF)
#define WLAN_IPA_META_DATA_MASK htonl(0x00FF0000)

#define REO_REMAP_HISTORY_SIZE 32

struct dp_ipa_reo_remap_record dp_ipa_reo_remap_history[REO_REMAP_HISTORY_SIZE];

static qdf_atomic_t dp_ipa_reo_remap_history_index;
static int dp_ipa_reo_remap_record_index_next(qdf_atomic_t *index)
{
	int next = qdf_atomic_inc_return(index);

	if (next == REO_REMAP_HISTORY_SIZE)
		qdf_atomic_sub(REO_REMAP_HISTORY_SIZE, index);

	return next % REO_REMAP_HISTORY_SIZE;
}

/**
 * dp_ipa_reo_remap_history_add() - Record dp ipa reo remap values
 * @ix0_val: reo destination ring IX0 value
 * @ix2_val: reo destination ring IX2 value
 * @ix3_val: reo destination ring IX3 value
 *
 * Return: None
 */
static void dp_ipa_reo_remap_history_add(uint32_t ix0_val, uint32_t ix2_val,
					 uint32_t ix3_val)
{
	int idx = dp_ipa_reo_remap_record_index_next(
				&dp_ipa_reo_remap_history_index);
	struct dp_ipa_reo_remap_record *record = &dp_ipa_reo_remap_history[idx];

	record->timestamp = qdf_get_log_timestamp();
	record->ix0_reg = ix0_val;
	record->ix2_reg = ix2_val;
	record->ix3_reg = ix3_val;
}

static QDF_STATUS __dp_ipa_handle_buf_smmu_mapping(struct dp_soc *soc,
						   qdf_nbuf_t nbuf,
						   uint32_t size,
						   bool create,
						   const char *func,
						   uint32_t line)
{
	qdf_mem_info_t mem_map_table = {0};
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	qdf_ipa_wdi_hdl_t hdl;

	/* Need to handle the case when one soc will
	 * have multiple pdev(radio's), Currently passing
	 * pdev_id as 0 assuming 1 soc has only 1 radio.
	 */
	hdl = wlan_ipa_get_hdl(soc->ctrl_psoc, 0);
	if (hdl == DP_IPA_HDL_INVALID) {
		dp_err("IPA handle is invalid");
		return QDF_STATUS_E_INVAL;
	}
	qdf_update_mem_map_table(soc->osdev, &mem_map_table,
				 qdf_nbuf_get_frag_paddr(nbuf, 0),
				 size);

	if (create) {
		/* Assert if PA is zero */
		qdf_assert_always(mem_map_table.pa);

		ret = qdf_nbuf_smmu_map_debug(nbuf, hdl, 1, &mem_map_table,
					      func, line);
	} else {
		ret = qdf_nbuf_smmu_unmap_debug(nbuf, hdl, 1, &mem_map_table,
						func, line);
	}
	qdf_assert_always(!ret);

	/* Return status of mapping/unmapping is stored in
	 * mem_map_table.result field, assert if the result
	 * is failure
	 */
	if (create)
		qdf_assert_always(!mem_map_table.result);
	else
		qdf_assert_always(mem_map_table.result >= mem_map_table.size);

	return ret;
}

QDF_STATUS dp_ipa_handle_rx_buf_smmu_mapping(struct dp_soc *soc,
					     qdf_nbuf_t nbuf,
					     uint32_t size,
					     bool create, const char *func,
					     uint32_t line)
{
	struct dp_pdev *pdev;
	int i;

	for (i = 0; i < soc->pdev_count; i++) {
		pdev = soc->pdev_list[i];
		if (pdev && dp_monitor_is_configured(pdev))
			return QDF_STATUS_SUCCESS;
	}

	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx) ||
	    !qdf_mem_smmu_s1_enabled(soc->osdev))
		return QDF_STATUS_SUCCESS;

	/*
	 * Even if ipa pipes is disabled, but if it's unmap
	 * operation and nbuf has done ipa smmu map before,
	 * do ipa smmu unmap as well.
	 */
	if (!(qdf_atomic_read(&soc->ipa_pipes_enabled) &&
				qdf_atomic_read(&soc->ipa_map_allowed))) {
		if (!create && qdf_nbuf_is_rx_ipa_smmu_map(nbuf)) {
			DP_STATS_INC(soc, rx.err.ipa_unmap_no_pipe, 1);
		} else {
			return QDF_STATUS_SUCCESS;
		}
	}

	if (qdf_unlikely(create == qdf_nbuf_is_rx_ipa_smmu_map(nbuf))) {
		if (create) {
			DP_STATS_INC(soc, rx.err.ipa_smmu_map_dup, 1);
		} else {
			DP_STATS_INC(soc, rx.err.ipa_smmu_unmap_dup, 1);
		}
		return QDF_STATUS_E_INVAL;
	}

	qdf_nbuf_set_rx_ipa_smmu_map(nbuf, create);

	return __dp_ipa_handle_buf_smmu_mapping(soc, nbuf, size, create,
						func, line);
}

static QDF_STATUS __dp_ipa_tx_buf_smmu_mapping(
	struct dp_soc *soc,
	struct dp_pdev *pdev,
	bool create,
	const char *func,
	uint32_t line)
{
	uint32_t index;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	uint32_t tx_buffer_cnt = soc->ipa_uc_tx_rsc.alloc_tx_buf_cnt;
	qdf_nbuf_t nbuf;
	uint32_t buf_len;

	if (!ipa_is_ready()) {
		dp_info("IPA is not READY");
		return 0;
	}

	for (index = 0; index < tx_buffer_cnt; index++) {
		nbuf = (qdf_nbuf_t)
			soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned[index];
		if (!nbuf)
			continue;
		buf_len = qdf_nbuf_get_data_len(nbuf);
		ret = __dp_ipa_handle_buf_smmu_mapping(soc, nbuf, buf_len,
						       create, func, line);
	}

	return ret;
}

#ifndef QCA_OL_DP_SRNG_LOCK_LESS_ACCESS
static void dp_ipa_set_reo_ctx_mapping_lock_required(struct dp_soc *soc,
						     bool lock_required)
{
	hal_ring_handle_t hal_ring_hdl;
	int ring;

	for (ring = 0; ring < soc->num_reo_dest_rings; ring++) {
		hal_ring_hdl = soc->reo_dest_ring[ring].hal_srng;
		hal_srng_lock(hal_ring_hdl);
		soc->ipa_reo_ctx_lock_required[ring] = lock_required;
		hal_srng_unlock(hal_ring_hdl);
	}
}
#else
static void dp_ipa_set_reo_ctx_mapping_lock_required(struct dp_soc *soc,
						     bool lock_required)
{
}

#endif

#ifdef RX_DESC_MULTI_PAGE_ALLOC
static QDF_STATUS dp_ipa_handle_rx_buf_pool_smmu_mapping(struct dp_soc *soc,
							 struct dp_pdev *pdev,
							 bool create,
							 const char *func,
							 uint32_t line)
{
	struct rx_desc_pool *rx_pool;
	uint8_t pdev_id;
	uint32_t num_desc, page_id, offset, i;
	uint16_t num_desc_per_page;
	union dp_rx_desc_list_elem_t *rx_desc_elem;
	struct dp_rx_desc *rx_desc;
	qdf_nbuf_t nbuf;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (!qdf_ipa_is_ready())
		return ret;

	if (!qdf_mem_smmu_s1_enabled(soc->osdev))
		return ret;

	pdev_id = pdev->pdev_id;
	rx_pool = &soc->rx_desc_buf[pdev_id];

	dp_ipa_set_reo_ctx_mapping_lock_required(soc, true);
	qdf_spin_lock_bh(&rx_pool->lock);
	dp_ipa_rx_buf_smmu_mapping_lock(soc);
	num_desc = rx_pool->pool_size;
	num_desc_per_page = rx_pool->desc_pages.num_element_per_page;
	for (i = 0; i < num_desc; i++) {
		page_id = i / num_desc_per_page;
		offset = i % num_desc_per_page;
		if (qdf_unlikely(!(rx_pool->desc_pages.cacheable_pages)))
			break;
		rx_desc_elem = dp_rx_desc_find(page_id, offset, rx_pool);
		rx_desc = &rx_desc_elem->rx_desc;
		if ((!(rx_desc->in_use)) || rx_desc->unmapped)
			continue;
		nbuf = rx_desc->nbuf;

		if (qdf_unlikely(create ==
				 qdf_nbuf_is_rx_ipa_smmu_map(nbuf))) {
			if (create) {
				DP_STATS_INC(soc,
					     rx.err.ipa_smmu_map_dup, 1);
			} else {
				DP_STATS_INC(soc,
					     rx.err.ipa_smmu_unmap_dup, 1);
			}
			continue;
		}
		qdf_nbuf_set_rx_ipa_smmu_map(nbuf, create);

		ret = __dp_ipa_handle_buf_smmu_mapping(soc, nbuf,
						       rx_pool->buf_size,
						       create, func, line);
	}
	dp_ipa_rx_buf_smmu_mapping_unlock(soc);
	qdf_spin_unlock_bh(&rx_pool->lock);
	dp_ipa_set_reo_ctx_mapping_lock_required(soc, false);

	return ret;
}
#else
static QDF_STATUS dp_ipa_handle_rx_buf_pool_smmu_mapping(
							 struct dp_soc *soc,
							 struct dp_pdev *pdev,
							 bool create,
							 const char *func,
							 uint32_t line)
{
	struct rx_desc_pool *rx_pool;
	uint8_t pdev_id;
	qdf_nbuf_t nbuf;
	int i;

	if (!qdf_ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	if (!qdf_mem_smmu_s1_enabled(soc->osdev))
		return QDF_STATUS_SUCCESS;

	pdev_id = pdev->pdev_id;
	rx_pool = &soc->rx_desc_buf[pdev_id];

	dp_ipa_set_reo_ctx_mapping_lock_required(soc, true);
	qdf_spin_lock_bh(&rx_pool->lock);
	dp_ipa_rx_buf_smmu_mapping_lock(soc);
	for (i = 0; i < rx_pool->pool_size; i++) {
		if ((!(rx_pool->array[i].rx_desc.in_use)) ||
		    rx_pool->array[i].rx_desc.unmapped)
			continue;

		nbuf = rx_pool->array[i].rx_desc.nbuf;

		if (qdf_unlikely(create ==
				 qdf_nbuf_is_rx_ipa_smmu_map(nbuf))) {
			if (create) {
				DP_STATS_INC(soc,
					     rx.err.ipa_smmu_map_dup, 1);
			} else {
				DP_STATS_INC(soc,
					     rx.err.ipa_smmu_unmap_dup, 1);
			}
			continue;
		}
		qdf_nbuf_set_rx_ipa_smmu_map(nbuf, create);

		__dp_ipa_handle_buf_smmu_mapping(soc, nbuf, rx_pool->buf_size,
						 create, func, line);
	}
	dp_ipa_rx_buf_smmu_mapping_unlock(soc);
	qdf_spin_unlock_bh(&rx_pool->lock);
	dp_ipa_set_reo_ctx_mapping_lock_required(soc, false);

	return QDF_STATUS_SUCCESS;
}
#endif /* RX_DESC_MULTI_PAGE_ALLOC */

QDF_STATUS dp_ipa_set_smmu_mapped(struct cdp_soc_t *soc_hdl, int val)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	qdf_atomic_set(&soc->ipa_map_allowed, val);
	return QDF_STATUS_SUCCESS;
}

int dp_ipa_get_smmu_mapped(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	return qdf_atomic_read(&soc->ipa_map_allowed);
}

static QDF_STATUS dp_ipa_get_shared_mem_info(qdf_device_t osdev,
					     qdf_shared_mem_t *shared_mem,
					     void *cpu_addr,
					     qdf_dma_addr_t dma_addr,
					     uint32_t size)
{
	qdf_dma_addr_t paddr;
	int ret;

	shared_mem->vaddr = cpu_addr;
	qdf_mem_set_dma_size(osdev, &shared_mem->mem_info, size);
	*qdf_mem_get_dma_addr_ptr(osdev, &shared_mem->mem_info) = dma_addr;

	paddr = qdf_mem_paddr_from_dmaaddr(osdev, dma_addr);
	qdf_mem_set_dma_pa(osdev, &shared_mem->mem_info, paddr);

	ret = qdf_mem_dma_get_sgtable(osdev->dev, &shared_mem->sgtable,
				      shared_mem->vaddr, dma_addr, size);
	if (ret) {
		dp_err("Unable to get DMA sgtable");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_dma_get_sgtable_dma_addr(&shared_mem->sgtable);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ipa_get_tx_bank_id() - API to get TCL bank id
 * @soc: dp_soc handle
 * @bank_id: out parameter for bank id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_ipa_get_tx_bank_id(struct dp_soc *soc, uint8_t *bank_id)
{
	if (soc->arch_ops.ipa_get_bank_id) {
		*bank_id = soc->arch_ops.ipa_get_bank_id(soc);
		if (*bank_id < 0) {
			return QDF_STATUS_E_INVAL;
		} else {
			dp_info("bank_id %u", *bank_id);
			return QDF_STATUS_SUCCESS;
		}
	} else {
		return QDF_STATUS_E_NOSUPPORT;
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)
static void dp_ipa_setup_tx_params_bank_id(struct dp_soc *soc,
					   qdf_ipa_wdi_pipe_setup_info_t *tx)
{
	uint8_t bank_id;

	if (QDF_IS_STATUS_SUCCESS(dp_ipa_get_tx_bank_id(soc, &bank_id)))
		QDF_IPA_WDI_SETUP_INFO_RX_BANK_ID(tx, bank_id);
}

static void
dp_ipa_setup_tx_smmu_params_bank_id(struct dp_soc *soc,
				    qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu)
{
	uint8_t bank_id;

	if (QDF_IS_STATUS_SUCCESS(dp_ipa_get_tx_bank_id(soc, &bank_id)))
		QDF_IPA_WDI_SETUP_INFO_SMMU_RX_BANK_ID(tx_smmu, bank_id);
}
#else
static inline void
dp_ipa_setup_tx_params_bank_id(struct dp_soc *soc,
			       qdf_ipa_wdi_pipe_setup_info_t *tx)
{
}

static inline void
dp_ipa_setup_tx_smmu_params_bank_id(struct dp_soc *soc,
				    qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu)
{
}
#endif

#ifdef QCA_IPA_LL_TX_FLOW_CONTROL
static void
dp_ipa_setup_tx_alt_params_pmac_id(struct dp_soc *soc,
				   qdf_ipa_wdi_pipe_setup_info_t *tx)
{
	uint8_t pmac_id = 0;

	/* Set Pmac ID, extract pmac_id from second radio for TX_ALT ring */
	if (soc->pdev_count > 1)
		pmac_id = soc->pdev_list[soc->pdev_count - 1]->lmac_id;

	QDF_IPA_WDI_SETUP_INFO_RX_PMAC_ID(tx, pmac_id);
}

static void
dp_ipa_setup_tx_alt_smmu_params_pmac_id(struct dp_soc *soc,
					qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu)
{
	uint8_t pmac_id = 0;

	/* Set Pmac ID, extract pmac_id from second radio for TX_ALT ring */
	if (soc->pdev_count > 1)
		pmac_id = soc->pdev_list[soc->pdev_count - 1]->lmac_id;

	QDF_IPA_WDI_SETUP_INFO_SMMU_RX_PMAC_ID(tx_smmu, pmac_id);
}

static void
dp_ipa_setup_tx_params_pmac_id(struct dp_soc *soc,
			       qdf_ipa_wdi_pipe_setup_info_t *tx)
{
	uint8_t pmac_id;

	pmac_id = soc->pdev_list[0]->lmac_id;

	QDF_IPA_WDI_SETUP_INFO_RX_PMAC_ID(tx, pmac_id);
}

static void
dp_ipa_setup_tx_smmu_params_pmac_id(struct dp_soc *soc,
				    qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu)
{
	uint8_t pmac_id;

	pmac_id = soc->pdev_list[0]->lmac_id;

	QDF_IPA_WDI_SETUP_INFO_SMMU_RX_PMAC_ID(tx_smmu, pmac_id);
}
#else
static inline void
dp_ipa_setup_tx_alt_params_pmac_id(struct dp_soc *soc,
				   qdf_ipa_wdi_pipe_setup_info_t *tx)
{
}

static inline void
dp_ipa_setup_tx_alt_smmu_params_pmac_id(struct dp_soc *soc,
					qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu)
{
}

static inline void
dp_ipa_setup_tx_params_pmac_id(struct dp_soc *soc,
			       qdf_ipa_wdi_pipe_setup_info_t *tx)
{
}

static inline void
dp_ipa_setup_tx_smmu_params_pmac_id(struct dp_soc *soc,
				    qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu)
{
}
#endif

#ifdef IPA_WDI3_TX_TWO_PIPES
static void dp_ipa_tx_alt_pool_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res;
	qdf_nbuf_t nbuf;
	int idx;

	for (idx = 0; idx < soc->ipa_uc_tx_rsc_alt.alloc_tx_buf_cnt; idx++) {
		nbuf = (qdf_nbuf_t)
			soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned[idx];
		if (!nbuf)
			continue;

		qdf_nbuf_unmap_single(soc->osdev, nbuf, QDF_DMA_BIDIRECTIONAL);
		qdf_mem_dp_tx_skb_cnt_dec();
		qdf_mem_dp_tx_skb_dec(qdf_nbuf_get_end_offset(nbuf));
		qdf_nbuf_free(nbuf);
		soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned[idx] =
						(void *)NULL;
	}

	qdf_mem_free(soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned);
	soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned = NULL;

	ipa_res = &pdev->ipa_resource;
	if (!ipa_res->is_db_ddr_mapped && ipa_res->tx_alt_comp_doorbell_vaddr)
		iounmap(ipa_res->tx_alt_comp_doorbell_vaddr);

	qdf_mem_free_sgtable(&ipa_res->tx_alt_ring.sgtable);
	qdf_mem_free_sgtable(&ipa_res->tx_alt_comp_ring.sgtable);
}

static int dp_ipa_tx_alt_pool_attach(struct dp_soc *soc)
{
	uint32_t tx_buffer_count;
	uint32_t ring_base_align = 8;
	qdf_dma_addr_t buffer_paddr;
	struct hal_srng *wbm_srng = (struct hal_srng *)
			soc->tx_comp_ring[IPA_TX_ALT_COMP_RING_IDX].hal_srng;
	struct hal_srng_params srng_params;
	uint32_t wbm_bm_id;
	void *ring_entry;
	int num_entries;
	qdf_nbuf_t nbuf;
	int retval = QDF_STATUS_SUCCESS;
	int max_alloc_count = 0;

	/*
	 * Uncomment when dp_ops_cfg.cfg_attach is implemented
	 * unsigned int uc_tx_buf_sz =
	 *		dp_cfg_ipa_uc_tx_buf_size(pdev->osif_pdev);
	 */
	unsigned int uc_tx_buf_sz = CFG_IPA_UC_TX_BUF_SIZE_DEFAULT;
	unsigned int alloc_size = uc_tx_buf_sz + ring_base_align - 1;

	wbm_bm_id = wlan_cfg_get_rbm_id_for_index(soc->wlan_cfg_ctx,
						  IPA_TX_ALT_RING_IDX);

	hal_get_srng_params(soc->hal_soc,
			    hal_srng_to_hal_ring_handle(wbm_srng),
			    &srng_params);
	num_entries = srng_params.num_entries;

	max_alloc_count =
		num_entries - DP_IPA_WAR_WBM2SW_REL_RING_NO_BUF_ENTRIES;
	if (max_alloc_count <= 0) {
		dp_err("incorrect value for buffer count %u", max_alloc_count);
		return -EINVAL;
	}

	dp_info("requested %d buffers to be posted to wbm ring",
		max_alloc_count);

	soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned =
		qdf_mem_malloc(num_entries *
		sizeof(*soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned));
	if (!soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned) {
		dp_err("IPA WBM Ring Tx buf pool vaddr alloc fail");
		return -ENOMEM;
	}

	hal_srng_access_start_unlocked(soc->hal_soc,
				       hal_srng_to_hal_ring_handle(wbm_srng));

	/*
	 * Allocate Tx buffers as many as possible.
	 * Leave DP_IPA_WAR_WBM2SW_REL_RING_NO_BUF_ENTRIES empty
	 * Populate Tx buffers into WBM2IPA ring
	 * This initial buffer population will simulate H/W as source ring,
	 * and update HP
	 */
	for (tx_buffer_count = 0;
		tx_buffer_count < max_alloc_count - 1; tx_buffer_count++) {
		nbuf = qdf_nbuf_frag_alloc(soc->osdev, alloc_size, 0,
					   256, FALSE);
		if (!nbuf)
			break;

		ring_entry = hal_srng_dst_get_next_hp(
				soc->hal_soc,
				hal_srng_to_hal_ring_handle(wbm_srng));
		if (!ring_entry) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO,
				  "%s: Failed to get WBM ring entry",
				  __func__);
			qdf_nbuf_free(nbuf);
			break;
		}

		qdf_nbuf_map_single(soc->osdev, nbuf,
				    QDF_DMA_BIDIRECTIONAL);
		buffer_paddr = qdf_nbuf_get_frag_paddr(nbuf, 0);
		qdf_mem_dp_tx_skb_cnt_inc();
		qdf_mem_dp_tx_skb_inc(qdf_nbuf_get_end_offset(nbuf));

		hal_rxdma_buff_addr_info_set(soc->hal_soc, ring_entry,
					     buffer_paddr, 0, wbm_bm_id);

		soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned[
			tx_buffer_count] = (void *)nbuf;
	}

	hal_srng_access_end_unlocked(soc->hal_soc,
				     hal_srng_to_hal_ring_handle(wbm_srng));

	soc->ipa_uc_tx_rsc_alt.alloc_tx_buf_cnt = tx_buffer_count;

	if (tx_buffer_count) {
		dp_info("IPA TX buffer pool2: %d allocated", tx_buffer_count);
	} else {
		dp_err("Failed to allocate IPA TX buffer pool2");
		qdf_mem_free(
			soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned);
		soc->ipa_uc_tx_rsc_alt.tx_buf_pool_vaddr_unaligned = NULL;
		retval = -ENOMEM;
	}

	return retval;
}

static QDF_STATUS dp_ipa_tx_alt_ring_get_resource(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;

	ipa_res->tx_alt_ring_num_alloc_buffer =
		(uint32_t)soc->ipa_uc_tx_rsc_alt.alloc_tx_buf_cnt;

	dp_ipa_get_shared_mem_info(
			soc->osdev, &ipa_res->tx_alt_ring,
			soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_base_vaddr,
			soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_base_paddr,
			soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_size);

	dp_ipa_get_shared_mem_info(
			soc->osdev, &ipa_res->tx_alt_comp_ring,
			soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_base_vaddr,
			soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_base_paddr,
			soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_size);

	if (!qdf_mem_get_dma_addr(soc->osdev,
				  &ipa_res->tx_alt_comp_ring.mem_info))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

static void dp_ipa_tx_alt_ring_resource_setup(struct dp_soc *soc)
{
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;
	struct hal_srng *hal_srng;
	struct hal_srng_params srng_params;
	unsigned long addr_offset, dev_base_paddr;

	/* IPA TCL_DATA Alternative Ring - HAL_SRNG_SW2TCL2 */
	hal_srng = (struct hal_srng *)
		soc->tcl_data_ring[IPA_TX_ALT_RING_IDX].hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);

	soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_base_paddr =
		srng_params.ring_base_paddr;
	soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_base_vaddr =
		srng_params.ring_base_vaddr;
	soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	/*
	 * For the register backed memory addresses, use the scn->mem_pa to
	 * calculate the physical address of the shadow registers
	 */
	dev_base_paddr =
		(unsigned long)
		((struct hif_softc *)(hal_soc->hif_handle))->mem_pa;
	addr_offset = (unsigned long)(hal_srng->u.src_ring.hp_addr) -
		      (unsigned long)(hal_soc->dev_base_addr);
	soc->ipa_uc_tx_rsc_alt.ipa_tcl_hp_paddr =
				(qdf_dma_addr_t)(addr_offset + dev_base_paddr);

	dp_info("IPA TCL_DATA Alt Ring addr_offset=%x, dev_base_paddr=%x, hp_paddr=%x paddr=%pK vaddr=%pK size= %u(%u bytes)",
		(unsigned int)addr_offset,
		(unsigned int)dev_base_paddr,
		(unsigned int)(soc->ipa_uc_tx_rsc_alt.ipa_tcl_hp_paddr),
		(void *)soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_base_paddr,
		(void *)soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_tx_rsc_alt.ipa_tcl_ring_size);

	/* IPA TX Alternative COMP Ring - HAL_SRNG_WBM2SW4_RELEASE */
	hal_srng = (struct hal_srng *)
		soc->tx_comp_ring[IPA_TX_ALT_COMP_RING_IDX].hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);

	soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_base_paddr =
						srng_params.ring_base_paddr;
	soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_base_vaddr =
						srng_params.ring_base_vaddr;
	soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	soc->ipa_uc_tx_rsc_alt.ipa_wbm_hp_shadow_paddr =
		hal_srng_get_hp_addr(hal_soc_to_hal_soc_handle(hal_soc),
				     hal_srng_to_hal_ring_handle(hal_srng));
	addr_offset = (unsigned long)(hal_srng->u.dst_ring.tp_addr) -
		      (unsigned long)(hal_soc->dev_base_addr);
	soc->ipa_uc_tx_rsc_alt.ipa_wbm_tp_paddr =
				(qdf_dma_addr_t)(addr_offset + dev_base_paddr);

	dp_info("IPA TX Alt COMP Ring addr_offset=%x, dev_base_paddr=%x, ipa_wbm_tp_paddr=%x paddr=%pK vaddr=0%pK size= %u(%u bytes)",
		(unsigned int)addr_offset,
		(unsigned int)dev_base_paddr,
		(unsigned int)(soc->ipa_uc_tx_rsc_alt.ipa_wbm_tp_paddr),
		(void *)soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_base_paddr,
		(void *)soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_tx_rsc_alt.ipa_wbm_ring_size);
}

static void dp_ipa_map_ring_doorbell_paddr(struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;
	uint32_t rx_ready_doorbell_dmaaddr;
	uint32_t tx_comp_doorbell_dmaaddr;
	struct dp_soc *soc = pdev->soc;
	int ret = 0;

	if (ipa_res->is_db_ddr_mapped)
		ipa_res->tx_comp_doorbell_vaddr =
				phys_to_virt(ipa_res->tx_comp_doorbell_paddr);
	else
		ipa_res->tx_comp_doorbell_vaddr =
				ioremap(ipa_res->tx_comp_doorbell_paddr, 4);

	if (qdf_mem_smmu_s1_enabled(soc->osdev)) {
		ret = pld_smmu_map(soc->osdev->dev,
				   ipa_res->tx_comp_doorbell_paddr,
				   &tx_comp_doorbell_dmaaddr,
				   sizeof(uint32_t));
		ipa_res->tx_comp_doorbell_paddr = tx_comp_doorbell_dmaaddr;
		qdf_assert_always(!ret);

		ret = pld_smmu_map(soc->osdev->dev,
				   ipa_res->rx_ready_doorbell_paddr,
				   &rx_ready_doorbell_dmaaddr,
				   sizeof(uint32_t));
		ipa_res->rx_ready_doorbell_paddr = rx_ready_doorbell_dmaaddr;
		qdf_assert_always(!ret);
	}

	/* Setup for alternative TX pipe */
	if (!ipa_res->tx_alt_comp_doorbell_paddr)
		return;

	if (ipa_res->is_db_ddr_mapped)
		ipa_res->tx_alt_comp_doorbell_vaddr =
			phys_to_virt(ipa_res->tx_alt_comp_doorbell_paddr);
	else
		ipa_res->tx_alt_comp_doorbell_vaddr =
			ioremap(ipa_res->tx_alt_comp_doorbell_paddr, 4);

	if (qdf_mem_smmu_s1_enabled(soc->osdev)) {
		ret = pld_smmu_map(soc->osdev->dev,
				   ipa_res->tx_alt_comp_doorbell_paddr,
				   &tx_comp_doorbell_dmaaddr,
				   sizeof(uint32_t));
		ipa_res->tx_alt_comp_doorbell_paddr = tx_comp_doorbell_dmaaddr;
		qdf_assert_always(!ret);
	}
}

static void dp_ipa_unmap_ring_doorbell_paddr(struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;
	struct dp_soc *soc = pdev->soc;
	int ret = 0;

	if (!qdf_mem_smmu_s1_enabled(soc->osdev))
		return;

	/* Unmap must be in reverse order of map */
	if (ipa_res->tx_alt_comp_doorbell_paddr) {
		ret = pld_smmu_unmap(soc->osdev->dev,
				     ipa_res->tx_alt_comp_doorbell_paddr,
				     sizeof(uint32_t));
		qdf_assert_always(!ret);
	}

	ret = pld_smmu_unmap(soc->osdev->dev,
			     ipa_res->rx_ready_doorbell_paddr,
			     sizeof(uint32_t));
	qdf_assert_always(!ret);

	ret = pld_smmu_unmap(soc->osdev->dev,
			     ipa_res->tx_comp_doorbell_paddr,
			     sizeof(uint32_t));
	qdf_assert_always(!ret);
}

static QDF_STATUS dp_ipa_tx_alt_buf_smmu_mapping(struct dp_soc *soc,
						 struct dp_pdev *pdev,
						 bool create, const char *func,
						 uint32_t line)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct ipa_dp_tx_rsc *rsc;
	uint32_t tx_buffer_cnt;
	uint32_t buf_len;
	qdf_nbuf_t nbuf;
	uint32_t index;

	if (!ipa_is_ready()) {
		dp_info("IPA is not READY");
		return QDF_STATUS_SUCCESS;
	}

	rsc = &soc->ipa_uc_tx_rsc_alt;
	tx_buffer_cnt = rsc->alloc_tx_buf_cnt;

	for (index = 0; index < tx_buffer_cnt; index++) {
		nbuf = (qdf_nbuf_t)rsc->tx_buf_pool_vaddr_unaligned[index];
		if (!nbuf)
			continue;

		buf_len = qdf_nbuf_get_data_len(nbuf);
		ret = __dp_ipa_handle_buf_smmu_mapping(soc, nbuf, buf_len,
						       create, func, line);
	}

	return ret;
}

static void dp_ipa_wdi_tx_alt_pipe_params(struct dp_soc *soc,
					  struct dp_ipa_resources *ipa_res,
					  qdf_ipa_wdi_pipe_setup_info_t *tx)
{
	QDF_IPA_WDI_SETUP_INFO_CLIENT(tx) = IPA_CLIENT_WLAN2_CONS1;

	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(tx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->tx_alt_comp_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(tx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_alt_comp_ring.mem_info);

	/* WBM Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(tx) =
		soc->ipa_uc_tx_rsc_alt.ipa_wbm_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_TXR_RN_DB_PCIE_ADDR(tx) = true;

	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(tx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->tx_alt_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(tx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_alt_ring.mem_info);

	/* TCL Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(tx) =
		soc->ipa_uc_tx_rsc_alt.ipa_tcl_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_EVT_RN_DB_PCIE_ADDR(tx) = true;

	QDF_IPA_WDI_SETUP_INFO_NUM_PKT_BUFFERS(tx) =
		ipa_res->tx_alt_ring_num_alloc_buffer;

	QDF_IPA_WDI_SETUP_INFO_PKT_OFFSET(tx) = 0;

	dp_ipa_setup_tx_params_bank_id(soc, tx);

	/* Set Pmac ID, extract pmac_id from second radio for TX_ALT ring */
	dp_ipa_setup_tx_alt_params_pmac_id(soc, tx);
}

static void
dp_ipa_wdi_tx_alt_pipe_smmu_params(struct dp_soc *soc,
				   struct dp_ipa_resources *ipa_res,
				   qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu)
{
	QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(tx_smmu) = IPA_CLIENT_WLAN2_CONS1;

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_BASE(tx_smmu),
		     &ipa_res->tx_alt_comp_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_SIZE(tx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_alt_comp_ring.mem_info);
	/* WBM Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_DOORBELL_PA(tx_smmu) =
		soc->ipa_uc_tx_rsc_alt.ipa_wbm_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_SMMU_IS_TXR_RN_DB_PCIE_ADDR(tx_smmu) = true;

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_BASE(tx_smmu),
		     &ipa_res->tx_alt_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_SIZE(tx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_alt_ring.mem_info);
	/* TCL Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_DOORBELL_PA(tx_smmu) =
		soc->ipa_uc_tx_rsc_alt.ipa_tcl_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_SMMU_IS_EVT_RN_DB_PCIE_ADDR(tx_smmu) = true;

	QDF_IPA_WDI_SETUP_INFO_SMMU_NUM_PKT_BUFFERS(tx_smmu) =
		ipa_res->tx_alt_ring_num_alloc_buffer;
	QDF_IPA_WDI_SETUP_INFO_SMMU_PKT_OFFSET(tx_smmu) = 0;

	dp_ipa_setup_tx_smmu_params_bank_id(soc, tx_smmu);

	/* Set Pmac ID, extract pmac_id from second radio for TX_ALT ring */
	dp_ipa_setup_tx_alt_smmu_params_pmac_id(soc, tx_smmu);
}

static void dp_ipa_setup_tx_alt_pipe(struct dp_soc *soc,
				     struct dp_ipa_resources *res,
				     qdf_ipa_wdi_conn_in_params_t *in)
{
	qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu = NULL;
	qdf_ipa_wdi_pipe_setup_info_t *tx = NULL;
	qdf_ipa_ep_cfg_t *tx_cfg;

	QDF_IPA_WDI_CONN_IN_PARAMS_IS_TX1_USED(in) = true;

	if (qdf_mem_smmu_s1_enabled(soc->osdev)) {
		tx_smmu = &QDF_IPA_WDI_CONN_IN_PARAMS_TX_ALT_PIPE_SMMU(in);
		tx_cfg = &QDF_IPA_WDI_SETUP_INFO_SMMU_EP_CFG(tx_smmu);
		dp_ipa_wdi_tx_alt_pipe_smmu_params(soc, res, tx_smmu);
	} else {
		tx = &QDF_IPA_WDI_CONN_IN_PARAMS_TX_ALT_PIPE(in);
		tx_cfg = &QDF_IPA_WDI_SETUP_INFO_SMMU_EP_CFG(tx);
		dp_ipa_wdi_tx_alt_pipe_params(soc, res, tx);
	}

	QDF_IPA_EP_CFG_NAT_EN(tx_cfg) = IPA_BYPASS_NAT;
	QDF_IPA_EP_CFG_HDR_LEN(tx_cfg) = DP_IPA_UC_WLAN_TX_HDR_LEN;
	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE_VALID(tx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE(tx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_ADDITIONAL_CONST_LEN(tx_cfg) = 0;
	QDF_IPA_EP_CFG_MODE(tx_cfg) = IPA_BASIC;
	QDF_IPA_EP_CFG_HDR_LITTLE_ENDIAN(tx_cfg) = true;
}

static void dp_ipa_set_pipe_db(struct dp_ipa_resources *res,
			       qdf_ipa_wdi_conn_out_params_t *out)
{
	res->tx_comp_doorbell_paddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_TX_UC_DB_PA(out);
	res->rx_ready_doorbell_paddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_RX_UC_DB_PA(out);
	res->tx_alt_comp_doorbell_paddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_TX_UC_ALT_DB_PA(out);
}

static void dp_ipa_setup_iface_session_id(qdf_ipa_wdi_reg_intf_in_params_t *in,
					  uint8_t session_id)
{
	bool is_2g_iface = session_id & IPA_SESSION_ID_SHIFT;

	session_id = session_id >> IPA_SESSION_ID_SHIFT;
	dp_debug("session_id %u is_2g_iface %d", session_id, is_2g_iface);

	QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA(in) = htonl(session_id << 16);
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_IS_TX1_USED(in) = is_2g_iface;
}

static void dp_ipa_tx_comp_ring_init_hp(struct dp_soc *soc,
					struct dp_ipa_resources *res)
{
	struct hal_srng *wbm_srng;

	/* Init first TX comp ring */
	wbm_srng = (struct hal_srng *)
		soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;

	hal_srng_dst_init_hp(soc->hal_soc, wbm_srng,
			     res->tx_comp_doorbell_vaddr);

	/* Init the alternate TX comp ring */
	if (!res->tx_alt_comp_doorbell_paddr)
		return;

	wbm_srng = (struct hal_srng *)
		soc->tx_comp_ring[IPA_TX_ALT_COMP_RING_IDX].hal_srng;

	hal_srng_dst_init_hp(soc->hal_soc, wbm_srng,
			     res->tx_alt_comp_doorbell_vaddr);
}

static void
dp_ipa_tx_comp_ring_update_hp_addr(struct dp_soc *soc,
				   struct dp_ipa_resources *res)
{
	hal_ring_handle_t wbm_srng;

	/* Ring doorbell to WBM2IPA ring with current HW HP value */
	wbm_srng = soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;
	hal_srng_dst_update_hp_addr(soc->hal_soc, wbm_srng);

	if (!res->tx_alt_comp_doorbell_paddr)
		return;

	wbm_srng = soc->tx_comp_ring[IPA_TX_ALT_COMP_RING_IDX].hal_srng;
	hal_srng_dst_update_hp_addr(soc->hal_soc, wbm_srng);
}

static void dp_ipa_set_tx_doorbell_paddr(struct dp_soc *soc,
					 struct dp_ipa_resources *ipa_res)
{
	struct hal_srng *wbm_srng;

	wbm_srng = (struct hal_srng *)
			soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;

	hal_srng_dst_set_hp_paddr_confirm(wbm_srng,
					  ipa_res->tx_comp_doorbell_paddr);

	dp_info("paddr %pK vaddr %pK",
		(void *)ipa_res->tx_comp_doorbell_paddr,
		(void *)ipa_res->tx_comp_doorbell_vaddr);

	/* Setup for alternative TX comp ring */
	if (!ipa_res->tx_alt_comp_doorbell_paddr)
		return;

	wbm_srng = (struct hal_srng *)
			soc->tx_comp_ring[IPA_TX_ALT_COMP_RING_IDX].hal_srng;

	hal_srng_dst_set_hp_paddr_confirm(wbm_srng,
					  ipa_res->tx_alt_comp_doorbell_paddr);

	dp_info("paddr %pK vaddr %pK",
		(void *)ipa_res->tx_alt_comp_doorbell_paddr,
		(void *)ipa_res->tx_alt_comp_doorbell_vaddr);
}

#ifdef IPA_SET_RESET_TX_DB_PA
static QDF_STATUS dp_ipa_reset_tx_doorbell_pa(struct dp_soc *soc,
					      struct dp_ipa_resources *ipa_res)
{
	hal_ring_handle_t wbm_srng;
	qdf_dma_addr_t hp_addr;

	wbm_srng = soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;
	if (!wbm_srng)
		return QDF_STATUS_E_FAILURE;

	hp_addr = soc->ipa_uc_tx_rsc.ipa_wbm_hp_shadow_paddr;

	hal_srng_dst_set_hp_paddr_confirm((struct hal_srng *)wbm_srng, hp_addr);

	dp_info("Reset WBM HP addr paddr: %pK", (void *)hp_addr);

	/* Reset alternative TX comp ring */
	wbm_srng = soc->tx_comp_ring[IPA_TX_ALT_COMP_RING_IDX].hal_srng;
	if (!wbm_srng)
		return QDF_STATUS_E_FAILURE;

	hp_addr = soc->ipa_uc_tx_rsc_alt.ipa_wbm_hp_shadow_paddr;

	hal_srng_dst_set_hp_paddr_confirm((struct hal_srng *)wbm_srng, hp_addr);

	dp_info("Reset WBM HP addr paddr: %pK", (void *)hp_addr);

	return QDF_STATUS_SUCCESS;
}
#endif /* IPA_SET_RESET_TX_DB_PA */

#else /* !IPA_WDI3_TX_TWO_PIPES */

static inline
void dp_ipa_tx_alt_pool_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
}

static inline void dp_ipa_tx_alt_ring_resource_setup(struct dp_soc *soc)
{
}

static inline int dp_ipa_tx_alt_pool_attach(struct dp_soc *soc)
{
	return 0;
}

static inline QDF_STATUS dp_ipa_tx_alt_ring_get_resource(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_ipa_map_ring_doorbell_paddr(struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;
	uint32_t rx_ready_doorbell_dmaaddr;
	uint32_t tx_comp_doorbell_dmaaddr;
	struct dp_soc *soc = pdev->soc;
	int ret = 0;

	if (ipa_res->is_db_ddr_mapped)
		ipa_res->tx_comp_doorbell_vaddr =
				phys_to_virt(ipa_res->tx_comp_doorbell_paddr);
	else
		ipa_res->tx_comp_doorbell_vaddr =
				ioremap(ipa_res->tx_comp_doorbell_paddr, 4);

	if (qdf_mem_smmu_s1_enabled(soc->osdev)) {
		ret = pld_smmu_map(soc->osdev->dev,
				   ipa_res->tx_comp_doorbell_paddr,
				   &tx_comp_doorbell_dmaaddr,
				   sizeof(uint32_t));
		ipa_res->tx_comp_doorbell_paddr = tx_comp_doorbell_dmaaddr;
		qdf_assert_always(!ret);

		ret = pld_smmu_map(soc->osdev->dev,
				   ipa_res->rx_ready_doorbell_paddr,
				   &rx_ready_doorbell_dmaaddr,
				   sizeof(uint32_t));
		ipa_res->rx_ready_doorbell_paddr = rx_ready_doorbell_dmaaddr;
		qdf_assert_always(!ret);
	}
}

static inline void dp_ipa_unmap_ring_doorbell_paddr(struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;
	struct dp_soc *soc = pdev->soc;
	int ret = 0;

	if (!qdf_mem_smmu_s1_enabled(soc->osdev))
		return;

	ret = pld_smmu_unmap(soc->osdev->dev,
			     ipa_res->rx_ready_doorbell_paddr,
			     sizeof(uint32_t));
	qdf_assert_always(!ret);

	ret = pld_smmu_unmap(soc->osdev->dev,
			     ipa_res->tx_comp_doorbell_paddr,
			     sizeof(uint32_t));
	qdf_assert_always(!ret);
}

static inline QDF_STATUS dp_ipa_tx_alt_buf_smmu_mapping(struct dp_soc *soc,
							struct dp_pdev *pdev,
							bool create,
							const char *func,
							uint32_t line)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_ipa_setup_tx_alt_pipe(struct dp_soc *soc, struct dp_ipa_resources *res,
			      qdf_ipa_wdi_conn_in_params_t *in)
{
}

static void dp_ipa_set_pipe_db(struct dp_ipa_resources *res,
			       qdf_ipa_wdi_conn_out_params_t *out)
{
	res->tx_comp_doorbell_paddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_TX_UC_DB_PA(out);
	res->rx_ready_doorbell_paddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_RX_UC_DB_PA(out);
}

#ifdef IPA_WDS_EASYMESH_FEATURE
/**
 * dp_ipa_setup_iface_session_id() - Pass vdev id to IPA
 * @in: ipa in params
 * @session_id: vdev id
 *
 * Pass Vdev id to IPA, IPA metadata order is changed and vdev id
 * is stored at higher nibble so, no shift is required.
 *
 * Return: none
 */
static void dp_ipa_setup_iface_session_id(qdf_ipa_wdi_reg_intf_in_params_t *in,
					  uint8_t session_id)
{
	if (ucfg_ipa_is_wds_enabled())
		QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA(in) = htonl(session_id);
	else
		QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA(in) = htonl(session_id << 16);
}
#else
static void dp_ipa_setup_iface_session_id(qdf_ipa_wdi_reg_intf_in_params_t *in,
					  uint8_t session_id)
{
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA(in) = htonl(session_id << 16);
}
#endif

static inline void dp_ipa_tx_comp_ring_init_hp(struct dp_soc *soc,
					       struct dp_ipa_resources *res)
{
	struct hal_srng *wbm_srng = (struct hal_srng *)
		soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;

	hal_srng_dst_init_hp(soc->hal_soc, wbm_srng,
			     res->tx_comp_doorbell_vaddr);
}

static void
dp_ipa_tx_comp_ring_update_hp_addr(struct dp_soc *soc,
				   struct dp_ipa_resources *res)
{
	hal_ring_handle_t wbm_srng;

	/* Ring doorbell to WBM2IPA ring with current HW HP value */
	wbm_srng = soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;
	hal_srng_dst_update_hp_addr(soc->hal_soc, wbm_srng);
}

static void dp_ipa_set_tx_doorbell_paddr(struct dp_soc *soc,
					 struct dp_ipa_resources *ipa_res)
{
	struct hal_srng *wbm_srng = (struct hal_srng *)
			soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;

	hal_srng_dst_set_hp_paddr_confirm(wbm_srng,
					  ipa_res->tx_comp_doorbell_paddr);

	dp_info("paddr %pK vaddr %pK",
		(void *)ipa_res->tx_comp_doorbell_paddr,
		(void *)ipa_res->tx_comp_doorbell_vaddr);
}

#ifdef IPA_SET_RESET_TX_DB_PA
static QDF_STATUS dp_ipa_reset_tx_doorbell_pa(struct dp_soc *soc,
					      struct dp_ipa_resources *ipa_res)
{
	hal_ring_handle_t wbm_srng =
			soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;
	qdf_dma_addr_t hp_addr;

	if (!wbm_srng)
		return QDF_STATUS_E_FAILURE;

	hp_addr = soc->ipa_uc_tx_rsc.ipa_wbm_hp_shadow_paddr;

	hal_srng_dst_set_hp_paddr_confirm((struct hal_srng *)wbm_srng, hp_addr);

	dp_info("Reset WBM HP addr paddr: %pK", (void *)hp_addr);

	return QDF_STATUS_SUCCESS;
}
#endif /* IPA_SET_RESET_TX_DB_PA */

#endif /* IPA_WDI3_TX_TWO_PIPES */

/**
 * dp_tx_ipa_uc_detach() - Free autonomy TX resources
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * Free allocated TX buffers with WBM SRNG
 *
 * Return: none
 */
static void dp_tx_ipa_uc_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	int idx;
	qdf_nbuf_t nbuf;
	struct dp_ipa_resources *ipa_res;

	for (idx = 0; idx < soc->ipa_uc_tx_rsc.alloc_tx_buf_cnt; idx++) {
		nbuf = (qdf_nbuf_t)
			soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned[idx];
		if (!nbuf)
			continue;
		qdf_nbuf_unmap_single(soc->osdev, nbuf, QDF_DMA_BIDIRECTIONAL);
		qdf_mem_dp_tx_skb_cnt_dec();
		qdf_mem_dp_tx_skb_dec(qdf_nbuf_get_end_offset(nbuf));
		qdf_nbuf_free(nbuf);
		soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned[idx] =
						(void *)NULL;
	}

	qdf_mem_free(soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned);
	soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned = NULL;

	ipa_res = &pdev->ipa_resource;

	qdf_mem_free_sgtable(&ipa_res->tx_ring.sgtable);
	qdf_mem_free_sgtable(&ipa_res->tx_comp_ring.sgtable);
}

/**
 * dp_rx_ipa_uc_detach() - free autonomy RX resources
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * This function will detach DP RX into main device context
 * will free DP Rx resources.
 *
 * Return: none
 */
static void dp_rx_ipa_uc_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;

	qdf_mem_free_sgtable(&ipa_res->rx_rdy_ring.sgtable);
	qdf_mem_free_sgtable(&ipa_res->rx_refill_ring.sgtable);
}

/**
 * dp_rx_alt_ipa_uc_detach() - free autonomy RX resources
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * This function will detach DP RX into main device context
 * will free DP Rx resources.
 *
 * Return: none
 */
#ifdef IPA_WDI3_VLAN_SUPPORT
static void dp_rx_alt_ipa_uc_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;

	if (!wlan_ipa_is_vlan_enabled())
		return;

	qdf_mem_free_sgtable(&ipa_res->rx_alt_rdy_ring.sgtable);
	qdf_mem_free_sgtable(&ipa_res->rx_alt_refill_ring.sgtable);
}
#else
static inline
void dp_rx_alt_ipa_uc_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{ }
#endif

/**
 * dp_ipa_opt_wifi_dp_cleanup() - Cleanup ipa opt wifi dp filter setup
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * This function will cleanup filter setup for optional wifi dp.
 *
 * Return: none
 */

#ifdef IPA_OPT_WIFI_DP
static void dp_ipa_opt_wifi_dp_cleanup(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;
	struct hif_softc *hif = (struct hif_softc *)(hal_soc->hif_handle);
	int count = qdf_atomic_read(&hif->opt_wifi_dp_rtpm_cnt);
	int i;

	for (i = count; i > 0; i--) {
		dp_info("opt_dp: cleanup call pcie link down");
		dp_ipa_pcie_link_down((struct cdp_soc_t *)soc);
	}
}
#else
static inline
void dp_ipa_opt_wifi_dp_cleanup(struct dp_soc *soc, struct dp_pdev *pdev)
{
}
#endif

int dp_ipa_uc_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	/* TX resource detach */
	dp_tx_ipa_uc_detach(soc, pdev);

	/* Cleanup 2nd TX pipe resources */
	dp_ipa_tx_alt_pool_detach(soc, pdev);

	/* RX resource detach */
	dp_rx_ipa_uc_detach(soc, pdev);

	/* Cleanup 2nd RX pipe resources */
	dp_rx_alt_ipa_uc_detach(soc, pdev);

	dp_ipa_opt_wifi_dp_cleanup(soc, pdev);

	return QDF_STATUS_SUCCESS;	/* success */
}

/**
 * dp_tx_ipa_uc_attach() - Allocate autonomy TX resources
 * @soc: data path instance
 * @pdev: Physical device handle
 *
 * Allocate TX buffer from non-cacheable memory
 * Attach allocated TX buffers with WBM SRNG
 *
 * Return: int
 */
static int dp_tx_ipa_uc_attach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	uint32_t tx_buffer_count;
	uint32_t ring_base_align = 8;
	qdf_dma_addr_t buffer_paddr;
	struct hal_srng *wbm_srng = (struct hal_srng *)
			soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;
	struct hal_srng_params srng_params;
	void *ring_entry;
	int num_entries;
	qdf_nbuf_t nbuf;
	int retval = QDF_STATUS_SUCCESS;
	int max_alloc_count = 0;
	uint32_t wbm_bm_id;

	/*
	 * Uncomment when dp_ops_cfg.cfg_attach is implemented
	 * unsigned int uc_tx_buf_sz =
	 *		dp_cfg_ipa_uc_tx_buf_size(pdev->osif_pdev);
	 */
	unsigned int uc_tx_buf_sz = CFG_IPA_UC_TX_BUF_SIZE_DEFAULT;
	unsigned int alloc_size = uc_tx_buf_sz + ring_base_align - 1;

	wbm_bm_id = wlan_cfg_get_rbm_id_for_index(soc->wlan_cfg_ctx,
						  IPA_TCL_DATA_RING_IDX);

	hal_get_srng_params(soc->hal_soc, hal_srng_to_hal_ring_handle(wbm_srng),
			    &srng_params);
	num_entries = srng_params.num_entries;

	max_alloc_count =
		num_entries - DP_IPA_WAR_WBM2SW_REL_RING_NO_BUF_ENTRIES;
	if (max_alloc_count <= 0) {
		dp_err("incorrect value for buffer count %u", max_alloc_count);
		return -EINVAL;
	}

	dp_info("requested %d buffers to be posted to wbm ring",
		max_alloc_count);

	soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned =
		qdf_mem_malloc(num_entries *
		sizeof(*soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned));
	if (!soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned) {
		dp_err("IPA WBM Ring Tx buf pool vaddr alloc fail");
		return -ENOMEM;
	}

	hal_srng_access_start_unlocked(soc->hal_soc,
				       hal_srng_to_hal_ring_handle(wbm_srng));

	/*
	 * Allocate Tx buffers as many as possible.
	 * Leave DP_IPA_WAR_WBM2SW_REL_RING_NO_BUF_ENTRIES empty
	 * Populate Tx buffers into WBM2IPA ring
	 * This initial buffer population will simulate H/W as source ring,
	 * and update HP
	 */
	for (tx_buffer_count = 0;
		tx_buffer_count < max_alloc_count - 1; tx_buffer_count++) {
		nbuf = qdf_nbuf_frag_alloc(soc->osdev, alloc_size, 0,
					   256, FALSE);
		if (!nbuf)
			break;

		ring_entry = hal_srng_dst_get_next_hp(soc->hal_soc,
				hal_srng_to_hal_ring_handle(wbm_srng));
		if (!ring_entry) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO,
				  "%s: Failed to get WBM ring entry",
				  __func__);
			qdf_nbuf_free(nbuf);
			break;
		}

		retval = qdf_nbuf_map_single(soc->osdev, nbuf,
					     QDF_DMA_BIDIRECTIONAL);
		if (qdf_unlikely(retval != QDF_STATUS_SUCCESS)) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  "%s: nbuf map failed", __func__);
			qdf_nbuf_free(nbuf);
			retval = -EFAULT;
			break;
		}
		buffer_paddr = qdf_nbuf_get_frag_paddr(nbuf, 0);
		qdf_mem_dp_tx_skb_cnt_inc();
		qdf_mem_dp_tx_skb_inc(qdf_nbuf_get_end_offset(nbuf));

		/*
		 * TODO - KIWI code can directly call the be handler
		 * instead of hal soc ops.
		 */
		hal_rxdma_buff_addr_info_set(soc->hal_soc, ring_entry,
					     buffer_paddr, 0, wbm_bm_id);

		soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned[tx_buffer_count]
			= (void *)nbuf;
	}

	hal_srng_access_end_unlocked(soc->hal_soc,
				     hal_srng_to_hal_ring_handle(wbm_srng));

	soc->ipa_uc_tx_rsc.alloc_tx_buf_cnt = tx_buffer_count;

	if (tx_buffer_count) {
		dp_info("IPA WDI TX buffer: %d allocated", tx_buffer_count);
	} else {
		dp_err("No IPA WDI TX buffer allocated!");
		qdf_mem_free(soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned);
		soc->ipa_uc_tx_rsc.tx_buf_pool_vaddr_unaligned = NULL;
		retval = -ENOMEM;
	}

	return retval;
}

/**
 * dp_rx_ipa_uc_attach() - Allocate autonomy RX resources
 * @soc: data path instance
 * @pdev: core txrx pdev context
 *
 * This function will attach a DP RX instance into the main
 * device (SOC) context.
 *
 * Return: QDF_STATUS_SUCCESS: success
 *         QDF_STATUS_E_RESOURCES: Error return
 */
static int dp_rx_ipa_uc_attach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

int dp_ipa_uc_attach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	int error;

	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	/* TX resource attach */
	error = dp_tx_ipa_uc_attach(soc, pdev);
	if (error) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s: DP IPA UC TX attach fail code %d",
			  __func__, error);
		if (error == -EFAULT)
			dp_tx_ipa_uc_detach(soc, pdev);
		return error;
	}

	/* Setup 2nd TX pipe */
	error = dp_ipa_tx_alt_pool_attach(soc);
	if (error) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s: DP IPA TX pool2 attach fail code %d",
			  __func__, error);
		dp_tx_ipa_uc_detach(soc, pdev);
		return error;
	}

	/* RX resource attach */
	error = dp_rx_ipa_uc_attach(soc, pdev);
	if (error) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s: DP IPA UC RX attach fail code %d",
			  __func__, error);
		dp_ipa_tx_alt_pool_detach(soc, pdev);
		dp_tx_ipa_uc_detach(soc, pdev);
		return error;
	}

	return QDF_STATUS_SUCCESS;	/* success */
}

#ifdef IPA_WDI3_VLAN_SUPPORT
/**
 * dp_ipa_rx_alt_ring_resource_setup() - setup IPA 2nd RX ring resources
 * @soc: data path SoC handle
 * @pdev: data path pdev handle
 *
 * Return: none
 */
static
void dp_ipa_rx_alt_ring_resource_setup(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;
	struct hal_srng *hal_srng;
	struct hal_srng_params srng_params;
	unsigned long addr_offset, dev_base_paddr;
	qdf_dma_addr_t hp_addr;

	if (!wlan_ipa_is_vlan_enabled())
		return;

	dev_base_paddr =
		(unsigned long)
		((struct hif_softc *)(hal_soc->hif_handle))->mem_pa;

	/* IPA REO_DEST Ring - HAL_SRNG_REO2SW3 */
	hal_srng = (struct hal_srng *)
			soc->reo_dest_ring[IPA_ALT_REO_DEST_RING_IDX].hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);

	soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_base_paddr =
						srng_params.ring_base_paddr;
	soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_base_vaddr =
						srng_params.ring_base_vaddr;
	soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	addr_offset = (unsigned long)(hal_srng->u.dst_ring.tp_addr) -
		      (unsigned long)(hal_soc->dev_base_addr);
	soc->ipa_uc_rx_rsc_alt.ipa_reo_tp_paddr =
				(qdf_dma_addr_t)(addr_offset + dev_base_paddr);

	dp_info("IPA REO_DEST Ring addr_offset=%x, dev_base_paddr=%x, tp_paddr=%x paddr=%pK vaddr=%pK size= %u(%u bytes)",
		(unsigned int)addr_offset,
		(unsigned int)dev_base_paddr,
		(unsigned int)(soc->ipa_uc_rx_rsc_alt.ipa_reo_tp_paddr),
		(void *)soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_base_paddr,
		(void *)soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_size);

	hal_srng = (struct hal_srng *)
			pdev->rx_refill_buf_ring3.hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);
	soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_base_paddr =
		srng_params.ring_base_paddr;
	soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_base_vaddr =
		srng_params.ring_base_vaddr;
	soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	hp_addr = hal_srng_get_hp_addr(hal_soc_to_hal_soc_handle(hal_soc),
				       hal_srng_to_hal_ring_handle(hal_srng));
	soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_hp_paddr =
		qdf_mem_paddr_from_dmaaddr(soc->osdev, hp_addr);

	dp_info("IPA REFILL_BUF Ring hp_paddr=%x paddr=%pK vaddr=%pK size= %u(%u bytes)",
		(unsigned int)(soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_hp_paddr),
		(void *)soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_base_paddr,
		(void *)soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_size);
}
#else
static inline
void dp_ipa_rx_alt_ring_resource_setup(struct dp_soc *soc, struct dp_pdev *pdev)
{ }
#endif
int dp_ipa_ring_resource_setup(struct dp_soc *soc,
		struct dp_pdev *pdev)
{
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;
	struct hal_srng *hal_srng;
	struct hal_srng_params srng_params;
	qdf_dma_addr_t hp_addr;
	unsigned long addr_offset, dev_base_paddr;
	uint32_t ix0;
	uint8_t ix0_map[8];

	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	/* IPA TCL_DATA Ring - HAL_SRNG_SW2TCL3 */
	hal_srng = (struct hal_srng *)
			soc->tcl_data_ring[IPA_TCL_DATA_RING_IDX].hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);

	soc->ipa_uc_tx_rsc.ipa_tcl_ring_base_paddr =
		srng_params.ring_base_paddr;
	soc->ipa_uc_tx_rsc.ipa_tcl_ring_base_vaddr =
		srng_params.ring_base_vaddr;
	soc->ipa_uc_tx_rsc.ipa_tcl_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	/*
	 * For the register backed memory addresses, use the scn->mem_pa to
	 * calculate the physical address of the shadow registers
	 */
	dev_base_paddr =
		(unsigned long)
		((struct hif_softc *)(hal_soc->hif_handle))->mem_pa;
	addr_offset = (unsigned long)(hal_srng->u.src_ring.hp_addr) -
		      (unsigned long)(hal_soc->dev_base_addr);
	soc->ipa_uc_tx_rsc.ipa_tcl_hp_paddr =
				(qdf_dma_addr_t)(addr_offset + dev_base_paddr);

	dp_info("IPA TCL_DATA Ring addr_offset=%x, dev_base_paddr=%x, hp_paddr=%x paddr=%pK vaddr=%pK size= %u(%u bytes)",
		(unsigned int)addr_offset,
		(unsigned int)dev_base_paddr,
		(unsigned int)(soc->ipa_uc_tx_rsc.ipa_tcl_hp_paddr),
		(void *)soc->ipa_uc_tx_rsc.ipa_tcl_ring_base_paddr,
		(void *)soc->ipa_uc_tx_rsc.ipa_tcl_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_tx_rsc.ipa_tcl_ring_size);

	/* IPA TX COMP Ring - HAL_SRNG_WBM2SW2_RELEASE */
	hal_srng = (struct hal_srng *)
			soc->tx_comp_ring[IPA_TX_COMP_RING_IDX].hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);

	soc->ipa_uc_tx_rsc.ipa_wbm_ring_base_paddr =
						srng_params.ring_base_paddr;
	soc->ipa_uc_tx_rsc.ipa_wbm_ring_base_vaddr =
						srng_params.ring_base_vaddr;
	soc->ipa_uc_tx_rsc.ipa_wbm_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	soc->ipa_uc_tx_rsc.ipa_wbm_hp_shadow_paddr =
		hal_srng_get_hp_addr(hal_soc_to_hal_soc_handle(hal_soc),
				     hal_srng_to_hal_ring_handle(hal_srng));
	addr_offset = (unsigned long)(hal_srng->u.dst_ring.tp_addr) -
		      (unsigned long)(hal_soc->dev_base_addr);
	soc->ipa_uc_tx_rsc.ipa_wbm_tp_paddr =
				(qdf_dma_addr_t)(addr_offset + dev_base_paddr);

	dp_info("IPA TX COMP Ring addr_offset=%x, dev_base_paddr=%x, ipa_wbm_tp_paddr=%x paddr=%pK vaddr=0%pK size= %u(%u bytes)",
		(unsigned int)addr_offset,
		(unsigned int)dev_base_paddr,
		(unsigned int)(soc->ipa_uc_tx_rsc.ipa_wbm_tp_paddr),
		(void *)soc->ipa_uc_tx_rsc.ipa_wbm_ring_base_paddr,
		(void *)soc->ipa_uc_tx_rsc.ipa_wbm_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_tx_rsc.ipa_wbm_ring_size);

	dp_ipa_tx_alt_ring_resource_setup(soc);

	/* IPA REO_DEST Ring - HAL_SRNG_REO2SW4 */
	hal_srng = (struct hal_srng *)
			soc->reo_dest_ring[IPA_REO_DEST_RING_IDX].hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);

	soc->ipa_uc_rx_rsc.ipa_reo_ring_base_paddr =
						srng_params.ring_base_paddr;
	soc->ipa_uc_rx_rsc.ipa_reo_ring_base_vaddr =
						srng_params.ring_base_vaddr;
	soc->ipa_uc_rx_rsc.ipa_reo_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	addr_offset = (unsigned long)(hal_srng->u.dst_ring.tp_addr) -
		      (unsigned long)(hal_soc->dev_base_addr);
	soc->ipa_uc_rx_rsc.ipa_reo_tp_paddr =
				(qdf_dma_addr_t)(addr_offset + dev_base_paddr);

	dp_info("IPA REO_DEST Ring addr_offset=%x, dev_base_paddr=%x, tp_paddr=%x paddr=%pK vaddr=%pK size= %u(%u bytes)",
		(unsigned int)addr_offset,
		(unsigned int)dev_base_paddr,
		(unsigned int)(soc->ipa_uc_rx_rsc.ipa_reo_tp_paddr),
		(void *)soc->ipa_uc_rx_rsc.ipa_reo_ring_base_paddr,
		(void *)soc->ipa_uc_rx_rsc.ipa_reo_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_rx_rsc.ipa_reo_ring_size);

	hal_srng = (struct hal_srng *)
			pdev->rx_refill_buf_ring2.hal_srng;
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    hal_srng_to_hal_ring_handle(hal_srng),
			    &srng_params);
	soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_base_paddr =
		srng_params.ring_base_paddr;
	soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_base_vaddr =
		srng_params.ring_base_vaddr;
	soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	hp_addr = hal_srng_get_hp_addr(hal_soc_to_hal_soc_handle(hal_soc),
				       hal_srng_to_hal_ring_handle(hal_srng));
	soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_hp_paddr =
		qdf_mem_paddr_from_dmaaddr(soc->osdev, hp_addr);

	dp_info("IPA REFILL_BUF Ring hp_paddr=%x paddr=%pK vaddr=%pK size= %u(%u bytes)",
		(unsigned int)(soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_hp_paddr),
		(void *)soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_base_paddr,
		(void *)soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_base_vaddr,
		srng_params.num_entries,
		soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_size);

	/*
	 * Set DEST_RING_MAPPING_4 to SW2 as default value for
	 * DESTINATION_RING_CTRL_IX_0.
	 */
	ix0_map[0] = REO_REMAP_SW1;
	ix0_map[1] = REO_REMAP_SW1;
	ix0_map[2] = REO_REMAP_SW2;
	ix0_map[3] = REO_REMAP_SW3;
	ix0_map[4] = REO_REMAP_SW2;
	ix0_map[5] = REO_REMAP_RELEASE;
	ix0_map[6] = REO_REMAP_FW;
	ix0_map[7] = REO_REMAP_FW;

	dp_ipa_opt_dp_ixo_remap(ix0_map);
	ix0 = hal_gen_reo_remap_val(soc->hal_soc, HAL_REO_REMAP_REG_IX0,
				    ix0_map);

	hal_reo_read_write_ctrl_ix(soc->hal_soc, false, &ix0, NULL, NULL, NULL);

	dp_ipa_rx_alt_ring_resource_setup(soc, pdev);
	return 0;
}

#ifdef IPA_WDI3_VLAN_SUPPORT
/**
 * dp_ipa_rx_alt_ring_get_resource() - get IPA 2nd RX ring resources
 * @pdev: data path pdev handle
 *
 * Return: Success if resourece is found
 */
static QDF_STATUS dp_ipa_rx_alt_ring_get_resource(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;

	if (!wlan_ipa_is_vlan_enabled())
		return QDF_STATUS_SUCCESS;

	dp_ipa_get_shared_mem_info(soc->osdev, &ipa_res->rx_alt_rdy_ring,
				   soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_base_vaddr,
				   soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_base_paddr,
				   soc->ipa_uc_rx_rsc_alt.ipa_reo_ring_size);

	dp_ipa_get_shared_mem_info(
			soc->osdev, &ipa_res->rx_alt_refill_ring,
			soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_base_vaddr,
			soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_base_paddr,
			soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_ring_size);

	if (!qdf_mem_get_dma_addr(soc->osdev,
				  &ipa_res->rx_alt_rdy_ring.mem_info) ||
	    !qdf_mem_get_dma_addr(soc->osdev,
				  &ipa_res->rx_alt_refill_ring.mem_info))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS dp_ipa_rx_alt_ring_get_resource(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS dp_ipa_get_resource(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_ipa_resources *ipa_res;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	ipa_res = &pdev->ipa_resource;
	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	ipa_res->tx_num_alloc_buffer =
		(uint32_t)soc->ipa_uc_tx_rsc.alloc_tx_buf_cnt;

	dp_ipa_get_shared_mem_info(soc->osdev, &ipa_res->tx_ring,
				   soc->ipa_uc_tx_rsc.ipa_tcl_ring_base_vaddr,
				   soc->ipa_uc_tx_rsc.ipa_tcl_ring_base_paddr,
				   soc->ipa_uc_tx_rsc.ipa_tcl_ring_size);

	dp_ipa_get_shared_mem_info(soc->osdev, &ipa_res->tx_comp_ring,
				   soc->ipa_uc_tx_rsc.ipa_wbm_ring_base_vaddr,
				   soc->ipa_uc_tx_rsc.ipa_wbm_ring_base_paddr,
				   soc->ipa_uc_tx_rsc.ipa_wbm_ring_size);

	dp_ipa_get_shared_mem_info(soc->osdev, &ipa_res->rx_rdy_ring,
				   soc->ipa_uc_rx_rsc.ipa_reo_ring_base_vaddr,
				   soc->ipa_uc_rx_rsc.ipa_reo_ring_base_paddr,
				   soc->ipa_uc_rx_rsc.ipa_reo_ring_size);

	dp_ipa_get_shared_mem_info(
			soc->osdev, &ipa_res->rx_refill_ring,
			soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_base_vaddr,
			soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_base_paddr,
			soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_ring_size);

	if (!qdf_mem_get_dma_addr(soc->osdev, &ipa_res->tx_ring.mem_info) ||
	    !qdf_mem_get_dma_addr(soc->osdev,
				  &ipa_res->tx_comp_ring.mem_info) ||
	    !qdf_mem_get_dma_addr(soc->osdev, &ipa_res->rx_rdy_ring.mem_info) ||
	    !qdf_mem_get_dma_addr(soc->osdev,
				  &ipa_res->rx_refill_ring.mem_info))
		return QDF_STATUS_E_FAILURE;

	if (dp_ipa_tx_alt_ring_get_resource(pdev))
		return QDF_STATUS_E_FAILURE;

	if (dp_ipa_rx_alt_ring_get_resource(pdev))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

#ifdef IPA_SET_RESET_TX_DB_PA
#define DP_IPA_SET_TX_DB_PADDR(soc, ipa_res)
#else
#define DP_IPA_SET_TX_DB_PADDR(soc, ipa_res) \
		dp_ipa_set_tx_doorbell_paddr(soc, ipa_res)
#endif

#ifdef IPA_WDI3_VLAN_SUPPORT
/**
 * dp_ipa_map_rx_alt_ring_doorbell_paddr() - Map 2nd rx ring doorbell paddr
 * @pdev: data path pdev handle
 *
 * Return: none
 */
static void dp_ipa_map_rx_alt_ring_doorbell_paddr(struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;
	uint32_t rx_ready_doorbell_dmaaddr;
	struct dp_soc *soc = pdev->soc;
	struct hal_srng *reo_srng = (struct hal_srng *)
			soc->reo_dest_ring[IPA_ALT_REO_DEST_RING_IDX].hal_srng;
	int ret = 0;

	if (!wlan_ipa_is_vlan_enabled())
		return;

	if (qdf_mem_smmu_s1_enabled(soc->osdev)) {
		ret = pld_smmu_map(soc->osdev->dev,
				   ipa_res->rx_alt_ready_doorbell_paddr,
				   &rx_ready_doorbell_dmaaddr,
				   sizeof(uint32_t));
		ipa_res->rx_alt_ready_doorbell_paddr =
					rx_ready_doorbell_dmaaddr;
		qdf_assert_always(!ret);
	}

	hal_srng_dst_set_hp_paddr_confirm(reo_srng,
					  ipa_res->rx_alt_ready_doorbell_paddr);
}

/**
 * dp_ipa_unmap_rx_alt_ring_doorbell_paddr() - Unmap 2nd rx ring doorbell paddr
 * @pdev: data path pdev handle
 *
 * Return: none
 */
static void dp_ipa_unmap_rx_alt_ring_doorbell_paddr(struct dp_pdev *pdev)
{
	struct dp_ipa_resources *ipa_res = &pdev->ipa_resource;
	struct dp_soc *soc = pdev->soc;
	int ret = 0;

	if (!wlan_ipa_is_vlan_enabled())
		return;

	if (!qdf_mem_smmu_s1_enabled(soc->osdev))
		return;

	ret = pld_smmu_unmap(soc->osdev->dev,
			     ipa_res->rx_alt_ready_doorbell_paddr,
			     sizeof(uint32_t));
	qdf_assert_always(!ret);
}
#else
static inline void dp_ipa_map_rx_alt_ring_doorbell_paddr(struct dp_pdev *pdev)
{ }

static inline void dp_ipa_unmap_rx_alt_ring_doorbell_paddr(struct dp_pdev *pdev)
{ }
#endif

QDF_STATUS dp_ipa_set_doorbell_paddr(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_ipa_resources *ipa_res;
	struct hal_srng *reo_srng = (struct hal_srng *)
			soc->reo_dest_ring[IPA_REO_DEST_RING_IDX].hal_srng;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	ipa_res = &pdev->ipa_resource;
	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	dp_ipa_map_ring_doorbell_paddr(pdev);
	dp_ipa_map_rx_alt_ring_doorbell_paddr(pdev);

	DP_IPA_SET_TX_DB_PADDR(soc, ipa_res);

	/*
	 * For RX, REO module on Napier/Hastings does reordering on incoming
	 * Ethernet packets and writes one or more descriptors to REO2IPA Rx
	 * ring.It then updates the ring’s Write/Head ptr and rings a doorbell
	 * to IPA.
	 * Set the doorbell addr for the REO ring.
	 */
	hal_srng_dst_set_hp_paddr_confirm(reo_srng,
					  ipa_res->rx_ready_doorbell_paddr);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_iounmap_doorbell_vaddr(struct cdp_soc_t *soc_hdl,
					 uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_ipa_resources *ipa_res;

	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	ipa_res = &pdev->ipa_resource;
	if (!ipa_res->is_db_ddr_mapped)
		iounmap(ipa_res->tx_comp_doorbell_vaddr);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_op_response(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			      uint8_t *op_msg)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_cfg_is_ipa_enabled(pdev->soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	if (pdev->ipa_uc_op_cb) {
		pdev->ipa_uc_op_cb(op_msg, pdev->usr_ctxt);
	} else {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
		    "%s: IPA callback function is not registered", __func__);
		qdf_mem_free(op_msg);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_register_op_cb(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				 ipa_uc_op_cb_type op_cb,
				 void *usr_ctxt)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_cfg_is_ipa_enabled(pdev->soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	pdev->ipa_uc_op_cb = op_cb;
	pdev->usr_ctxt = usr_ctxt;

	return QDF_STATUS_SUCCESS;
}

void dp_ipa_deregister_op_cb(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("Invalid instance");
		return;
	}

	dp_debug("Deregister OP handler callback");
	pdev->ipa_uc_op_cb = NULL;
	pdev->usr_ctxt = NULL;
}

QDF_STATUS dp_ipa_get_stat(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	/* TBD */
	return QDF_STATUS_SUCCESS;
}

qdf_nbuf_t dp_tx_send_ipa_data_frame(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				     qdf_nbuf_t skb)
{
	qdf_nbuf_t ret;

	/* Terminate the (single-element) list of tx frames */
	qdf_nbuf_set_next(skb, NULL);
	ret = dp_tx_send(soc_hdl, vdev_id, skb);
	if (ret) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Failed to tx", __func__);
		return ret;
	}

	return NULL;
}

#ifdef QCA_IPA_LL_TX_FLOW_CONTROL
/**
 * dp_ipa_is_target_ready() - check if target is ready or not
 * @soc: datapath soc handle
 *
 * Return: true if target is ready
 */
static inline
bool dp_ipa_is_target_ready(struct dp_soc *soc)
{
	if (hif_get_target_status(soc->hif_handle) == TARGET_STATUS_RESET)
		return false;
	else
		return true;
}

/**
 * dp_ipa_update_txr_db_status() - Indicate transfer ring DB is SMMU mapped or not
 * @dev: Pointer to device
 * @txrx_smmu: WDI TX/RX configuration
 *
 * Return: None
 */
static inline
void dp_ipa_update_txr_db_status(struct device *dev,
				 qdf_ipa_wdi_pipe_setup_info_smmu_t *txrx_smmu)
{
	int pcie_slot = pld_get_pci_slot(dev);

	if (pcie_slot)
		QDF_IPA_WDI_SETUP_INFO_SMMU_IS_TXR_RN_DB_PCIE_ADDR(txrx_smmu) = false;
	else
		QDF_IPA_WDI_SETUP_INFO_SMMU_IS_TXR_RN_DB_PCIE_ADDR(txrx_smmu) = true;
}

/**
 * dp_ipa_update_evt_db_status() - Indicate evt ring DB is SMMU mapped or not
 * @dev: Pointer to device
 * @txrx_smmu: WDI TX/RX configuration
 *
 * Return: None
 */
static inline
void dp_ipa_update_evt_db_status(struct device *dev,
				 qdf_ipa_wdi_pipe_setup_info_smmu_t *txrx_smmu)
{
	int pcie_slot = pld_get_pci_slot(dev);

	if (pcie_slot)
		QDF_IPA_WDI_SETUP_INFO_SMMU_IS_EVT_RN_DB_PCIE_ADDR(txrx_smmu) = false;
	else
		QDF_IPA_WDI_SETUP_INFO_SMMU_IS_EVT_RN_DB_PCIE_ADDR(txrx_smmu) = true;
}
#else
static inline
bool dp_ipa_is_target_ready(struct dp_soc *soc)
{
	return true;
}

static inline
void dp_ipa_update_txr_db_status(struct device *dev,
				 qdf_ipa_wdi_pipe_setup_info_smmu_t *txrx_smmu)
{
	QDF_IPA_WDI_SETUP_INFO_SMMU_IS_TXR_RN_DB_PCIE_ADDR(txrx_smmu) = true;
}

static inline
void dp_ipa_update_evt_db_status(struct device *dev,
				 qdf_ipa_wdi_pipe_setup_info_smmu_t *txrx_smmu)
{
	QDF_IPA_WDI_SETUP_INFO_SMMU_IS_EVT_RN_DB_PCIE_ADDR(txrx_smmu) = true;
}
#endif

QDF_STATUS dp_ipa_enable_autonomy(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	uint32_t ix0;
	uint32_t ix2;
	uint8_t ix_map[8];

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	if (!hif_is_target_ready(HIF_GET_SOFTC(soc->hif_handle)))
		return QDF_STATUS_E_AGAIN;

	if (!dp_ipa_is_target_ready(soc))
		return QDF_STATUS_E_AGAIN;

	/* Call HAL API to remap REO rings to REO2IPA ring */
	ix_map[0] = REO_REMAP_SW1;
	ix_map[1] = REO_REMAP_SW4;
	ix_map[2] = REO_REMAP_SW1;
	if (wlan_ipa_is_vlan_enabled())
		ix_map[3] = REO_REMAP_SW3;
	else
		ix_map[3] = REO_REMAP_SW4;
	ix_map[4] = REO_REMAP_SW4;
	ix_map[5] = REO_REMAP_RELEASE;
	ix_map[6] = REO_REMAP_FW;
	ix_map[7] = REO_REMAP_FW;

	ix0 = hal_gen_reo_remap_val(soc->hal_soc, HAL_REO_REMAP_REG_IX0,
				    ix_map);

	if (wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx)) {
		ix_map[0] = REO_REMAP_SW4;
		ix_map[1] = REO_REMAP_SW4;
		ix_map[2] = REO_REMAP_SW4;
		ix_map[3] = REO_REMAP_SW4;
		ix_map[4] = REO_REMAP_SW4;
		ix_map[5] = REO_REMAP_SW4;
		ix_map[6] = REO_REMAP_SW4;
		ix_map[7] = REO_REMAP_SW4;

		ix2 = hal_gen_reo_remap_val(soc->hal_soc, HAL_REO_REMAP_REG_IX2,
					    ix_map);

		hal_reo_read_write_ctrl_ix(soc->hal_soc, false, &ix0, NULL,
					   &ix2, &ix2);
		dp_ipa_reo_remap_history_add(ix0, ix2, ix2);
	} else {
		hal_reo_read_write_ctrl_ix(soc->hal_soc, false, &ix0, NULL,
					   NULL, NULL);
		dp_ipa_reo_remap_history_add(ix0, 0, 0);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_disable_autonomy(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	uint8_t ix0_map[8];
	uint32_t ix0;
	uint32_t ix1;
	uint32_t ix2;
	uint32_t ix3;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	if (!hif_is_target_ready(HIF_GET_SOFTC(soc->hif_handle)))
		return QDF_STATUS_E_AGAIN;

	if (!dp_ipa_is_target_ready(soc))
		return QDF_STATUS_E_AGAIN;

	ix0_map[0] = REO_REMAP_SW1;
	ix0_map[1] = REO_REMAP_SW1;
	ix0_map[2] = REO_REMAP_SW2;
	ix0_map[3] = REO_REMAP_SW3;
	ix0_map[4] = REO_REMAP_SW2;
	ix0_map[5] = REO_REMAP_RELEASE;
	ix0_map[6] = REO_REMAP_FW;
	ix0_map[7] = REO_REMAP_FW;

	/* Call HAL API to remap REO rings to REO2IPA ring */
	ix0 = hal_gen_reo_remap_val(soc->hal_soc, HAL_REO_REMAP_REG_IX0,
				    ix0_map);

	if (wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx)) {
		dp_reo_remap_config(soc, &ix1, &ix2, &ix3);

		hal_reo_read_write_ctrl_ix(soc->hal_soc, false, &ix0, NULL,
					   &ix2, &ix3);
		dp_ipa_reo_remap_history_add(ix0, ix2, ix3);
	} else {
		hal_reo_read_write_ctrl_ix(soc->hal_soc, false, &ix0, NULL,
					   NULL, NULL);
		dp_ipa_reo_remap_history_add(ix0, 0, 0);
	}

	return QDF_STATUS_SUCCESS;
}

/* This should be configurable per H/W configuration enable status */
#define L3_HEADER_PADDING	2

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)) || \
	defined(CONFIG_IPA_WDI_UNIFIED_API)

#if !defined(QCA_LL_TX_FLOW_CONTROL_V2) && !defined(QCA_IPA_LL_TX_FLOW_CONTROL)
static inline void dp_setup_mcc_sys_pipes(
		qdf_ipa_sys_connect_params_t *sys_in,
		qdf_ipa_wdi_conn_in_params_t *pipe_in)
{
	int i = 0;
	/* Setup MCC sys pipe */
	QDF_IPA_WDI_CONN_IN_PARAMS_NUM_SYS_PIPE_NEEDED(pipe_in) =
			DP_IPA_MAX_IFACE;
	for (i = 0; i < DP_IPA_MAX_IFACE; i++)
		memcpy(&QDF_IPA_WDI_CONN_IN_PARAMS_SYS_IN(pipe_in)[i],
		       &sys_in[i], sizeof(qdf_ipa_sys_connect_params_t));
}
#else
static inline void dp_setup_mcc_sys_pipes(
		qdf_ipa_sys_connect_params_t *sys_in,
		qdf_ipa_wdi_conn_in_params_t *pipe_in)
{
	QDF_IPA_WDI_CONN_IN_PARAMS_NUM_SYS_PIPE_NEEDED(pipe_in) = 0;
}
#endif

static void dp_ipa_wdi_tx_params(struct dp_soc *soc,
				 struct dp_ipa_resources *ipa_res,
				 qdf_ipa_wdi_pipe_setup_info_t *tx,
				 bool over_gsi)
{
	if (over_gsi)
		QDF_IPA_WDI_SETUP_INFO_CLIENT(tx) = IPA_CLIENT_WLAN2_CONS;
	else
		QDF_IPA_WDI_SETUP_INFO_CLIENT(tx) = IPA_CLIENT_WLAN1_CONS;

	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(tx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->tx_comp_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(tx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_comp_ring.mem_info);

	/* WBM Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(tx) =
		soc->ipa_uc_tx_rsc.ipa_wbm_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_TXR_RN_DB_PCIE_ADDR(tx) = true;

	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(tx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->tx_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(tx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_ring.mem_info);

	/* TCL Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(tx) =
		soc->ipa_uc_tx_rsc.ipa_tcl_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_EVT_RN_DB_PCIE_ADDR(tx) = true;

	QDF_IPA_WDI_SETUP_INFO_NUM_PKT_BUFFERS(tx) =
		ipa_res->tx_num_alloc_buffer;

	QDF_IPA_WDI_SETUP_INFO_PKT_OFFSET(tx) = 0;

	dp_ipa_setup_tx_params_bank_id(soc, tx);

	/* Set Pmac ID, extract pmac_id from pdev_id 0 for TX ring */
	dp_ipa_setup_tx_params_pmac_id(soc, tx);
}

static void dp_ipa_wdi_rx_params(struct dp_soc *soc,
				 struct dp_ipa_resources *ipa_res,
				 qdf_ipa_wdi_pipe_setup_info_t *rx,
				 bool over_gsi)
{
	if (over_gsi)
		QDF_IPA_WDI_SETUP_INFO_CLIENT(rx) =
					IPA_CLIENT_WLAN2_PROD;
	else
		QDF_IPA_WDI_SETUP_INFO_CLIENT(rx) =
					IPA_CLIENT_WLAN1_PROD;

	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(rx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->rx_rdy_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(rx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_rdy_ring.mem_info);

	/* REO Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(rx) =
		soc->ipa_uc_rx_rsc.ipa_reo_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_TXR_RN_DB_PCIE_ADDR(rx) = true;

	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(rx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->rx_refill_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(rx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_refill_ring.mem_info);

	/* FW Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(rx) =
		soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_EVT_RN_DB_PCIE_ADDR(rx) = false;

	QDF_IPA_WDI_SETUP_INFO_PKT_OFFSET(rx) =
		soc->rx_pkt_tlv_size + L3_HEADER_PADDING;
}

static void
dp_ipa_wdi_tx_smmu_params(struct dp_soc *soc,
			  struct dp_ipa_resources *ipa_res,
			  qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu,
			  bool over_gsi,
			  qdf_ipa_wdi_hdl_t hdl)
{
	if (over_gsi) {
		if (hdl == DP_IPA_HDL_FIRST)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(tx_smmu) =
				IPA_CLIENT_WLAN2_CONS;
		else if (hdl == DP_IPA_HDL_SECOND)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(tx_smmu) =
				IPA_CLIENT_WLAN4_CONS;
		else if (hdl == DP_IPA_HDL_THIRD)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(tx_smmu) =
				IPA_CLIENT_WLAN1_CONS;
	} else {
		QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(tx_smmu) =
			IPA_CLIENT_WLAN1_CONS;
	}

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_BASE(tx_smmu),
		     &ipa_res->tx_comp_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_SIZE(tx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_comp_ring.mem_info);
	/* WBM Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_DOORBELL_PA(tx_smmu) =
		soc->ipa_uc_tx_rsc.ipa_wbm_tp_paddr;
	dp_ipa_update_txr_db_status(soc->osdev->dev, tx_smmu);

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_BASE(tx_smmu),
		     &ipa_res->tx_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_SIZE(tx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->tx_ring.mem_info);
	/* TCL Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_DOORBELL_PA(tx_smmu) =
		soc->ipa_uc_tx_rsc.ipa_tcl_hp_paddr;
	dp_ipa_update_evt_db_status(soc->osdev->dev, tx_smmu);

	QDF_IPA_WDI_SETUP_INFO_SMMU_NUM_PKT_BUFFERS(tx_smmu) =
		ipa_res->tx_num_alloc_buffer;
	QDF_IPA_WDI_SETUP_INFO_SMMU_PKT_OFFSET(tx_smmu) = 0;

	dp_ipa_setup_tx_smmu_params_bank_id(soc, tx_smmu);

	/* Set Pmac ID, extract pmac_id from first pdev for TX ring */
	dp_ipa_setup_tx_smmu_params_pmac_id(soc, tx_smmu);
}

static void
dp_ipa_wdi_rx_smmu_params(struct dp_soc *soc,
			  struct dp_ipa_resources *ipa_res,
			  qdf_ipa_wdi_pipe_setup_info_smmu_t *rx_smmu,
			  bool over_gsi,
			  qdf_ipa_wdi_hdl_t hdl)
{
	if (over_gsi) {
		if (hdl == DP_IPA_HDL_FIRST)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(rx_smmu) =
				IPA_CLIENT_WLAN2_PROD;
		else if (hdl == DP_IPA_HDL_SECOND)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(rx_smmu) =
				IPA_CLIENT_WLAN3_PROD;
		else if (hdl == DP_IPA_HDL_THIRD)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(rx_smmu) =
				IPA_CLIENT_WLAN1_PROD;
	} else {
		QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(rx_smmu) =
					IPA_CLIENT_WLAN1_PROD;
	}

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_BASE(rx_smmu),
		     &ipa_res->rx_rdy_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_SIZE(rx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_rdy_ring.mem_info);
	/* REO Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_DOORBELL_PA(rx_smmu) =
		soc->ipa_uc_rx_rsc.ipa_reo_tp_paddr;
	dp_ipa_update_txr_db_status(soc->osdev->dev, rx_smmu);

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_BASE(rx_smmu),
		     &ipa_res->rx_refill_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_SIZE(rx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_refill_ring.mem_info);

	/* FW Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_DOORBELL_PA(rx_smmu) =
		soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_SMMU_IS_EVT_RN_DB_PCIE_ADDR(rx_smmu) = false;

	QDF_IPA_WDI_SETUP_INFO_SMMU_PKT_OFFSET(rx_smmu) =
		soc->rx_pkt_tlv_size + L3_HEADER_PADDING;
}

#ifdef IPA_WDI3_VLAN_SUPPORT
/**
 * dp_ipa_wdi_rx_alt_pipe_smmu_params() - Setup 2nd rx pipe smmu params
 * @soc: data path soc handle
 * @ipa_res: ipa resource pointer
 * @rx_smmu: smmu pipe info handle
 * @over_gsi: flag for IPA offload over gsi
 * @hdl: ipa registered handle
 *
 * Return: none
 */
static void
dp_ipa_wdi_rx_alt_pipe_smmu_params(struct dp_soc *soc,
				   struct dp_ipa_resources *ipa_res,
				   qdf_ipa_wdi_pipe_setup_info_smmu_t *rx_smmu,
				   bool over_gsi,
				   qdf_ipa_wdi_hdl_t hdl)
{
	if (!wlan_ipa_is_vlan_enabled())
		return;

	if (over_gsi) {
		if (hdl == DP_IPA_HDL_FIRST)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(rx_smmu) =
				IPA_CLIENT_WLAN2_PROD1;
		else if (hdl == DP_IPA_HDL_SECOND)
			QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(rx_smmu) =
				IPA_CLIENT_WLAN3_PROD1;
		else if (hdl == DP_IPA_HDL_THIRD)
			QDF_IPA_WDI_SETUP_INFO_CLIENT(rx_smmu) =
				IPA_CLIENT_WLAN1_PROD1;
	} else {
		QDF_IPA_WDI_SETUP_INFO_SMMU_CLIENT(rx_smmu) =
					IPA_CLIENT_WLAN1_PROD;
	}

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_BASE(rx_smmu),
		     &ipa_res->rx_alt_rdy_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_SIZE(rx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_alt_rdy_ring.mem_info);
	/* REO Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_TRANSFER_RING_DOORBELL_PA(rx_smmu) =
		soc->ipa_uc_rx_rsc_alt.ipa_reo_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_SMMU_IS_TXR_RN_DB_PCIE_ADDR(rx_smmu) = true;

	qdf_mem_copy(&QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_BASE(rx_smmu),
		     &ipa_res->rx_alt_refill_ring.sgtable,
		     sizeof(sgtable_t));
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_SIZE(rx_smmu) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_alt_refill_ring.mem_info);

	/* FW Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_SMMU_EVENT_RING_DOORBELL_PA(rx_smmu) =
		soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_SMMU_IS_EVT_RN_DB_PCIE_ADDR(rx_smmu) = false;

	QDF_IPA_WDI_SETUP_INFO_SMMU_PKT_OFFSET(rx_smmu) =
		soc->rx_pkt_tlv_size + L3_HEADER_PADDING;
}

/**
 * dp_ipa_wdi_rx_alt_pipe_params() - Setup 2nd rx pipe params
 * @soc: data path soc handle
 * @ipa_res: ipa resource pointer
 * @rx: pipe info handle
 * @over_gsi: flag for IPA offload over gsi
 * @hdl: ipa registered handle
 *
 * Return: none
 */
static void dp_ipa_wdi_rx_alt_pipe_params(struct dp_soc *soc,
					  struct dp_ipa_resources *ipa_res,
					  qdf_ipa_wdi_pipe_setup_info_t *rx,
					  bool over_gsi,
					  qdf_ipa_wdi_hdl_t hdl)
{
	if (!wlan_ipa_is_vlan_enabled())
		return;

	if (over_gsi) {
		if (hdl == DP_IPA_HDL_FIRST)
			QDF_IPA_WDI_SETUP_INFO_CLIENT(rx) =
				IPA_CLIENT_WLAN2_PROD1;
		else if (hdl == DP_IPA_HDL_SECOND)
			QDF_IPA_WDI_SETUP_INFO_CLIENT(rx) =
				IPA_CLIENT_WLAN3_PROD1;
		else if (hdl == DP_IPA_HDL_THIRD)
			QDF_IPA_WDI_SETUP_INFO_CLIENT(rx) =
				IPA_CLIENT_WLAN1_PROD1;
	} else {
		QDF_IPA_WDI_SETUP_INFO_CLIENT(rx) =
					IPA_CLIENT_WLAN1_PROD;
	}

	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(rx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->rx_alt_rdy_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(rx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_alt_rdy_ring.mem_info);

	/* REO Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(rx) =
		soc->ipa_uc_rx_rsc_alt.ipa_reo_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_TXR_RN_DB_PCIE_ADDR(rx) = true;

	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(rx) =
		qdf_mem_get_dma_addr(soc->osdev,
				     &ipa_res->rx_alt_refill_ring.mem_info);
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(rx) =
		qdf_mem_get_dma_size(soc->osdev,
				     &ipa_res->rx_alt_refill_ring.mem_info);

	/* FW Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(rx) =
		soc->ipa_uc_rx_rsc_alt.ipa_rx_refill_buf_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_IS_EVT_RN_DB_PCIE_ADDR(rx) = false;

	QDF_IPA_WDI_SETUP_INFO_PKT_OFFSET(rx) =
		soc->rx_pkt_tlv_size + L3_HEADER_PADDING;
}

/**
 * dp_ipa_setup_rx_alt_pipe() - Setup 2nd rx pipe for IPA offload
 * @soc: data path soc handle
 * @res: ipa resource pointer
 * @in: pipe in handle
 * @over_gsi: flag for IPA offload over gsi
 * @hdl: ipa registered handle
 *
 * Return: none
 */
static void dp_ipa_setup_rx_alt_pipe(struct dp_soc *soc,
				     struct dp_ipa_resources *res,
				     qdf_ipa_wdi_conn_in_params_t *in,
				     bool over_gsi,
				     qdf_ipa_wdi_hdl_t hdl)
{
	qdf_ipa_wdi_pipe_setup_info_smmu_t *rx_smmu = NULL;
	qdf_ipa_wdi_pipe_setup_info_t *rx = NULL;
	qdf_ipa_ep_cfg_t *rx_cfg;

	if (!wlan_ipa_is_vlan_enabled())
		return;

	QDF_IPA_WDI_CONN_IN_PARAMS_IS_RX1_USED(in) = true;
	if (qdf_mem_smmu_s1_enabled(soc->osdev)) {
		rx_smmu = &QDF_IPA_WDI_CONN_IN_PARAMS_RX_ALT_SMMU(in);
		rx_cfg = &QDF_IPA_WDI_SETUP_INFO_SMMU_EP_CFG(rx_smmu);
		dp_ipa_wdi_rx_alt_pipe_smmu_params(soc, res, rx_smmu,
						   over_gsi, hdl);
	} else {
		rx = &QDF_IPA_WDI_CONN_IN_PARAMS_RX_ALT(in);
		rx_cfg = &QDF_IPA_WDI_SETUP_INFO_SMMU_EP_CFG(rx);
		dp_ipa_wdi_rx_alt_pipe_params(soc, res, rx, over_gsi, hdl);
	}

	QDF_IPA_EP_CFG_NAT_EN(rx_cfg) = IPA_BYPASS_NAT;
	/* Update with wds len(96) + 4 if wds support is enabled */
	if (ucfg_ipa_is_wds_enabled())
		QDF_IPA_EP_CFG_HDR_LEN(rx_cfg) = DP_IPA_UC_WLAN_RX_HDR_LEN_AST_VLAN;
	else
		QDF_IPA_EP_CFG_HDR_LEN(rx_cfg) = DP_IPA_UC_WLAN_TX_VLAN_HDR_LEN;
	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE_VALID(rx_cfg) = 1;
	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE(rx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_ADDITIONAL_CONST_LEN(rx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_OFST_METADATA_VALID(rx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_METADATA_REG_VALID(rx_cfg) = 1;
	QDF_IPA_EP_CFG_MODE(rx_cfg) = IPA_BASIC;
	QDF_IPA_EP_CFG_HDR_LITTLE_ENDIAN(rx_cfg) = true;
}

/**
 * dp_ipa_set_rx_alt_pipe_db() - Setup 2nd rx pipe doorbell
 * @res: ipa resource pointer
 * @out: pipe out handle
 *
 * Return: none
 */
static void dp_ipa_set_rx_alt_pipe_db(struct dp_ipa_resources *res,
				      qdf_ipa_wdi_conn_out_params_t *out)
{
	if (!wlan_ipa_is_vlan_enabled())
		return;

	res->rx_alt_ready_doorbell_paddr =
			QDF_IPA_WDI_CONN_OUT_PARAMS_RX_ALT_UC_DB_PA(out);
	dp_debug("Setting DB 0x%x for RX alt pipe",
		 res->rx_alt_ready_doorbell_paddr);
}
#else
static inline
void dp_ipa_setup_rx_alt_pipe(struct dp_soc *soc,
			      struct dp_ipa_resources *res,
			      qdf_ipa_wdi_conn_in_params_t *in,
			      bool over_gsi,
			      qdf_ipa_wdi_hdl_t hdl)
{ }

static inline
void dp_ipa_set_rx_alt_pipe_db(struct dp_ipa_resources *res,
			       qdf_ipa_wdi_conn_out_params_t *out)
{ }
#endif

QDF_STATUS dp_ipa_setup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			void *ipa_i2w_cb, void *ipa_w2i_cb,
			void *ipa_wdi_meter_notifier_cb,
			uint32_t ipa_desc_size, void *ipa_priv,
			bool is_rm_enabled, uint32_t *tx_pipe_handle,
			uint32_t *rx_pipe_handle, bool is_smmu_enabled,
			qdf_ipa_sys_connect_params_t *sys_in, bool over_gsi,
			qdf_ipa_wdi_hdl_t hdl, qdf_ipa_wdi_hdl_t id,
			void *ipa_ast_notify_cb)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_ipa_resources *ipa_res;
	qdf_ipa_ep_cfg_t *tx_cfg;
	qdf_ipa_ep_cfg_t *rx_cfg;
	qdf_ipa_wdi_pipe_setup_info_t *tx = NULL;
	qdf_ipa_wdi_pipe_setup_info_t *rx = NULL;
	qdf_ipa_wdi_pipe_setup_info_smmu_t *tx_smmu;
	qdf_ipa_wdi_pipe_setup_info_smmu_t *rx_smmu = NULL;
	qdf_ipa_wdi_conn_in_params_t *pipe_in = NULL;
	qdf_ipa_wdi_conn_out_params_t pipe_out;
	int ret;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	ipa_res = &pdev->ipa_resource;
	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	pipe_in = qdf_mem_malloc(sizeof(*pipe_in));
	if (!pipe_in)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(&pipe_out, sizeof(pipe_out));

	if (is_smmu_enabled)
		QDF_IPA_WDI_CONN_IN_PARAMS_SMMU_ENABLED(pipe_in) = true;
	else
		QDF_IPA_WDI_CONN_IN_PARAMS_SMMU_ENABLED(pipe_in) = false;

	dp_setup_mcc_sys_pipes(sys_in, pipe_in);

	/* TX PIPE */
	if (QDF_IPA_WDI_CONN_IN_PARAMS_SMMU_ENABLED(pipe_in)) {
		tx_smmu = &QDF_IPA_WDI_CONN_IN_PARAMS_TX_SMMU(pipe_in);
		tx_cfg = &QDF_IPA_WDI_SETUP_INFO_SMMU_EP_CFG(tx_smmu);
	} else {
		tx = &QDF_IPA_WDI_CONN_IN_PARAMS_TX(pipe_in);
		tx_cfg = &QDF_IPA_WDI_SETUP_INFO_EP_CFG(tx);
	}

	QDF_IPA_EP_CFG_NAT_EN(tx_cfg) = IPA_BYPASS_NAT;
	QDF_IPA_EP_CFG_HDR_LEN(tx_cfg) = DP_IPA_UC_WLAN_TX_HDR_LEN;
	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE_VALID(tx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE(tx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_ADDITIONAL_CONST_LEN(tx_cfg) = 0;
	QDF_IPA_EP_CFG_MODE(tx_cfg) = IPA_BASIC;
	QDF_IPA_EP_CFG_HDR_LITTLE_ENDIAN(tx_cfg) = true;

	/*
	 * Transfer Ring: WBM Ring
	 * Transfer Ring Doorbell PA: WBM Tail Pointer Address
	 * Event Ring: TCL ring
	 * Event Ring Doorbell PA: TCL Head Pointer Address
	 */
	if (is_smmu_enabled)
		dp_ipa_wdi_tx_smmu_params(soc, ipa_res, tx_smmu, over_gsi, id);
	else
		dp_ipa_wdi_tx_params(soc, ipa_res, tx, over_gsi);

	dp_ipa_setup_tx_alt_pipe(soc, ipa_res, pipe_in);

	/* RX PIPE */
	if (QDF_IPA_WDI_CONN_IN_PARAMS_SMMU_ENABLED(pipe_in)) {
		rx_smmu = &QDF_IPA_WDI_CONN_IN_PARAMS_RX_SMMU(pipe_in);
		rx_cfg = &QDF_IPA_WDI_SETUP_INFO_SMMU_EP_CFG(rx_smmu);
	} else {
		rx = &QDF_IPA_WDI_CONN_IN_PARAMS_RX(pipe_in);
		rx_cfg = &QDF_IPA_WDI_SETUP_INFO_EP_CFG(rx);
	}

	QDF_IPA_EP_CFG_NAT_EN(rx_cfg) = IPA_BYPASS_NAT;
	if (ucfg_ipa_is_wds_enabled())
		QDF_IPA_EP_CFG_HDR_LEN(rx_cfg) = DP_IPA_UC_WLAN_RX_HDR_LEN_AST;
	else
		QDF_IPA_EP_CFG_HDR_LEN(rx_cfg) = DP_IPA_UC_WLAN_RX_HDR_LEN;

	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE_VALID(rx_cfg) = 1;
	QDF_IPA_EP_CFG_HDR_OFST_PKT_SIZE(rx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_ADDITIONAL_CONST_LEN(rx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_OFST_METADATA_VALID(rx_cfg) = 0;
	QDF_IPA_EP_CFG_HDR_METADATA_REG_VALID(rx_cfg) = 1;
	QDF_IPA_EP_CFG_MODE(rx_cfg) = IPA_BASIC;
	QDF_IPA_EP_CFG_HDR_LITTLE_ENDIAN(rx_cfg) = true;

	/*
	 * Transfer Ring: REO Ring
	 * Transfer Ring Doorbell PA: REO Tail Pointer Address
	 * Event Ring: FW ring
	 * Event Ring Doorbell PA: FW Head Pointer Address
	 */
	if (is_smmu_enabled)
		dp_ipa_wdi_rx_smmu_params(soc, ipa_res, rx_smmu, over_gsi, id);
	else
		dp_ipa_wdi_rx_params(soc, ipa_res, rx, over_gsi);

	/* setup 2nd rx pipe */
	dp_ipa_setup_rx_alt_pipe(soc, ipa_res, pipe_in, over_gsi, id);

	QDF_IPA_WDI_CONN_IN_PARAMS_NOTIFY(pipe_in) = ipa_w2i_cb;
	QDF_IPA_WDI_CONN_IN_PARAMS_PRIV(pipe_in) = ipa_priv;
	QDF_IPA_WDI_CONN_IN_PARAMS_HANDLE(pipe_in) = hdl;
	dp_ipa_ast_notify_cb(pipe_in, ipa_ast_notify_cb);

	/* Connect WDI IPA PIPEs */
	ret = qdf_ipa_wdi_conn_pipes(pipe_in, &pipe_out);

	if (ret) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: ipa_wdi_conn_pipes: IPA pipe setup failed: ret=%d",
			  __func__, ret);
		qdf_mem_free(pipe_in);
		return QDF_STATUS_E_FAILURE;
	}

	/* IPA uC Doorbell registers */
	dp_info("Tx DB PA=0x%x, Rx DB PA=0x%x",
		(unsigned int)QDF_IPA_WDI_CONN_OUT_PARAMS_TX_UC_DB_PA(&pipe_out),
		(unsigned int)QDF_IPA_WDI_CONN_OUT_PARAMS_RX_UC_DB_PA(&pipe_out));

	dp_ipa_set_pipe_db(ipa_res, &pipe_out);
	dp_ipa_set_rx_alt_pipe_db(ipa_res, &pipe_out);

	ipa_res->is_db_ddr_mapped =
		QDF_IPA_WDI_CONN_OUT_PARAMS_IS_DB_DDR_MAPPED(&pipe_out);

	soc->ipa_first_tx_db_access = true;
	qdf_mem_free(pipe_in);

	qdf_spinlock_create(&soc->ipa_rx_buf_map_lock);
	soc->ipa_rx_buf_map_lock_initialized = true;

	return QDF_STATUS_SUCCESS;
}

#ifdef IPA_WDI3_VLAN_SUPPORT
/**
 * dp_ipa_set_rx1_used() - Set rx1 used flag for 2nd rx offload ring
 * @in: pipe in handle
 *
 * Return: none
 */
static inline
void dp_ipa_set_rx1_used(qdf_ipa_wdi_reg_intf_in_params_t *in)
{
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_IS_RX1_USED(in) = true;
}

/**
 * dp_ipa_set_v4_vlan_hdr() - Set v4 vlan hdr
 * @in: pipe in handle
 * @hdr: pointer to hdr
 *
 * Return: none
 */
static inline
void dp_ipa_set_v4_vlan_hdr(qdf_ipa_wdi_reg_intf_in_params_t *in,
			    qdf_ipa_wdi_hdr_info_t *hdr)
{
	qdf_mem_copy(&(QDF_IPA_WDI_REG_INTF_IN_PARAMS_HDR_INFO(in)[IPA_IP_v4_VLAN]),
		     hdr, sizeof(qdf_ipa_wdi_hdr_info_t));
}

/**
 * dp_ipa_set_v6_vlan_hdr() - Set v6 vlan hdr
 * @in: pipe in handle
 * @hdr: pointer to hdr
 *
 * Return: none
 */
static inline
void dp_ipa_set_v6_vlan_hdr(qdf_ipa_wdi_reg_intf_in_params_t *in,
			    qdf_ipa_wdi_hdr_info_t *hdr)
{
	qdf_mem_copy(&(QDF_IPA_WDI_REG_INTF_IN_PARAMS_HDR_INFO(in)[IPA_IP_v6_VLAN]),
		     hdr, sizeof(qdf_ipa_wdi_hdr_info_t));
}
#else
static inline
void dp_ipa_set_rx1_used(qdf_ipa_wdi_reg_intf_in_params_t *in)
{ }

static inline
void dp_ipa_set_v4_vlan_hdr(qdf_ipa_wdi_reg_intf_in_params_t *in,
			    qdf_ipa_wdi_hdr_info_t *hdr)
{ }

static inline
void dp_ipa_set_v6_vlan_hdr(qdf_ipa_wdi_reg_intf_in_params_t *in,
			    qdf_ipa_wdi_hdr_info_t *hdr)
{ }
#endif

#ifdef IPA_WDS_EASYMESH_FEATURE
/**
 * dp_ipa_set_wdi_hdr_type() - Set wdi hdr type for IPA
 * @hdr_info: Header info
 *
 * Return: None
 */
static inline void
dp_ipa_set_wdi_hdr_type(qdf_ipa_wdi_hdr_info_t *hdr_info)
{
	if (ucfg_ipa_is_wds_enabled())
		QDF_IPA_WDI_HDR_INFO_HDR_TYPE(hdr_info) =
			IPA_HDR_L2_ETHERNET_II_AST;
	else
		QDF_IPA_WDI_HDR_INFO_HDR_TYPE(hdr_info) =
			IPA_HDR_L2_ETHERNET_II;
}

/**
 * dp_ipa_setup_meta_data_mask() - Pass meta data mask to IPA
 * @in: ipa in params
 *
 * Pass meta data mask to IPA.
 *
 * Return: none
 */
static void dp_ipa_setup_meta_data_mask(qdf_ipa_wdi_reg_intf_in_params_t *in)
{
	if (ucfg_ipa_is_wds_enabled())
		QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA_MASK(in) = WLAN_IPA_AST_META_DATA_MASK;
	else
		QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA_MASK(in) = WLAN_IPA_META_DATA_MASK;
}
#else
static inline void
dp_ipa_set_wdi_hdr_type(qdf_ipa_wdi_hdr_info_t *hdr_info)
{
	QDF_IPA_WDI_HDR_INFO_HDR_TYPE(hdr_info) = IPA_HDR_L2_ETHERNET_II;
}

static void dp_ipa_setup_meta_data_mask(qdf_ipa_wdi_reg_intf_in_params_t *in)
{
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA_MASK(in) = WLAN_IPA_META_DATA_MASK;
}
#endif

#ifdef IPA_WDI3_VLAN_SUPPORT
/**
 * dp_ipa_set_wdi_vlan_hdr_type() - Set wdi vlan hdr type for IPA
 * @hdr_info: Header info
 *
 * Return: None
 */
static inline void
dp_ipa_set_wdi_vlan_hdr_type(qdf_ipa_wdi_hdr_info_t *hdr_info)
{
	if (ucfg_ipa_is_wds_enabled())
		QDF_IPA_WDI_HDR_INFO_HDR_TYPE(hdr_info) =
			IPA_HDR_L2_802_1Q_AST;
	else
		QDF_IPA_WDI_HDR_INFO_HDR_TYPE(hdr_info) =
			IPA_HDR_L2_802_1Q;
}
#else
static inline void
dp_ipa_set_wdi_vlan_hdr_type(qdf_ipa_wdi_hdr_info_t *hdr_info)
{ }
#endif

QDF_STATUS dp_ipa_setup_iface(char *ifname, uint8_t *mac_addr,
			      qdf_ipa_client_type_t prod_client,
			      qdf_ipa_client_type_t cons_client,
			      uint8_t session_id, bool is_ipv6_enabled,
			      qdf_ipa_wdi_hdl_t hdl)
{
	qdf_ipa_wdi_reg_intf_in_params_t in;
	qdf_ipa_wdi_hdr_info_t hdr_info;
	struct dp_ipa_uc_tx_hdr uc_tx_hdr;
	struct dp_ipa_uc_tx_hdr uc_tx_hdr_v6;
	struct dp_ipa_uc_tx_vlan_hdr uc_tx_vlan_hdr;
	struct dp_ipa_uc_tx_vlan_hdr uc_tx_vlan_hdr_v6;
	int ret = -EINVAL;

	qdf_mem_zero(&in, sizeof(qdf_ipa_wdi_reg_intf_in_params_t));

	/* Need to reset the values to 0 as all the fields are not
	 * updated in the Header, Unused fields will be set to 0.
	 */
	qdf_mem_zero(&uc_tx_vlan_hdr, sizeof(struct dp_ipa_uc_tx_vlan_hdr));
	qdf_mem_zero(&uc_tx_vlan_hdr_v6, sizeof(struct dp_ipa_uc_tx_vlan_hdr));

	dp_debug("Add Partial hdr: %s, "QDF_MAC_ADDR_FMT, ifname,
		 QDF_MAC_ADDR_REF(mac_addr));
	qdf_mem_zero(&hdr_info, sizeof(qdf_ipa_wdi_hdr_info_t));
	qdf_ether_addr_copy(uc_tx_hdr.eth.h_source, mac_addr);

	/* IPV4 header */
	uc_tx_hdr.eth.h_proto = qdf_htons(ETH_P_IP);

	QDF_IPA_WDI_HDR_INFO_HDR(&hdr_info) = (uint8_t *)&uc_tx_hdr;
	QDF_IPA_WDI_HDR_INFO_HDR_LEN(&hdr_info) = DP_IPA_UC_WLAN_TX_HDR_LEN;
	dp_ipa_set_wdi_hdr_type(&hdr_info);

	QDF_IPA_WDI_HDR_INFO_DST_MAC_ADDR_OFFSET(&hdr_info) =
		DP_IPA_UC_WLAN_HDR_DES_MAC_OFFSET;

	QDF_IPA_WDI_REG_INTF_IN_PARAMS_NETDEV_NAME(&in) = ifname;
	qdf_mem_copy(&(QDF_IPA_WDI_REG_INTF_IN_PARAMS_HDR_INFO(&in)[IPA_IP_v4]),
		     &hdr_info, sizeof(qdf_ipa_wdi_hdr_info_t));
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_ALT_DST_PIPE(&in) = cons_client;
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_IS_META_DATA_VALID(&in) = 1;
	dp_ipa_setup_meta_data_mask(&in);
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_HANDLE(&in) = hdl;
	dp_ipa_setup_iface_session_id(&in, session_id);
	dp_debug("registering for session_id: %u", session_id);

	/* IPV6 header */
	if (is_ipv6_enabled) {
		qdf_mem_copy(&uc_tx_hdr_v6, &uc_tx_hdr,
			     DP_IPA_UC_WLAN_TX_HDR_LEN);
		uc_tx_hdr_v6.eth.h_proto = qdf_htons(ETH_P_IPV6);
		QDF_IPA_WDI_HDR_INFO_HDR(&hdr_info) = (uint8_t *)&uc_tx_hdr_v6;
		qdf_mem_copy(&(QDF_IPA_WDI_REG_INTF_IN_PARAMS_HDR_INFO(&in)[IPA_IP_v6]),
			     &hdr_info, sizeof(qdf_ipa_wdi_hdr_info_t));
	}

	if (wlan_ipa_is_vlan_enabled()) {
		/* Add vlan specific headers if vlan supporti is enabled */
		qdf_mem_zero(&hdr_info, sizeof(qdf_ipa_wdi_hdr_info_t));
		dp_ipa_set_rx1_used(&in);
		qdf_ether_addr_copy(uc_tx_vlan_hdr.eth.h_source, mac_addr);
		/* IPV4 Vlan header */
		uc_tx_vlan_hdr.eth.h_vlan_proto = qdf_htons(ETH_P_8021Q);
		uc_tx_vlan_hdr.eth.h_vlan_encapsulated_proto = qdf_htons(ETH_P_IP);

		QDF_IPA_WDI_HDR_INFO_HDR(&hdr_info) =
				(uint8_t *)&uc_tx_vlan_hdr;
		QDF_IPA_WDI_HDR_INFO_HDR_LEN(&hdr_info) =
				DP_IPA_UC_WLAN_TX_VLAN_HDR_LEN;
		dp_ipa_set_wdi_vlan_hdr_type(&hdr_info);

		QDF_IPA_WDI_HDR_INFO_DST_MAC_ADDR_OFFSET(&hdr_info) =
			DP_IPA_UC_WLAN_HDR_DES_MAC_OFFSET;

		dp_ipa_set_v4_vlan_hdr(&in, &hdr_info);

		/* IPV6 Vlan header */
		if (is_ipv6_enabled) {
			qdf_mem_copy(&uc_tx_vlan_hdr_v6, &uc_tx_vlan_hdr,
				     DP_IPA_UC_WLAN_TX_VLAN_HDR_LEN);
			uc_tx_vlan_hdr_v6.eth.h_vlan_proto =
					qdf_htons(ETH_P_8021Q);
			uc_tx_vlan_hdr_v6.eth.h_vlan_encapsulated_proto =
					qdf_htons(ETH_P_IPV6);
			QDF_IPA_WDI_HDR_INFO_HDR(&hdr_info) =
					(uint8_t *)&uc_tx_vlan_hdr_v6;
			dp_ipa_set_v6_vlan_hdr(&in, &hdr_info);
		}
	}

	ret = qdf_ipa_wdi_reg_intf(&in);
	if (ret) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: ipa_wdi_reg_intf: register IPA interface failed: ret=%d",
			  __func__, ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#else /* !CONFIG_IPA_WDI_UNIFIED_API */
QDF_STATUS dp_ipa_setup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			void *ipa_i2w_cb, void *ipa_w2i_cb,
			void *ipa_wdi_meter_notifier_cb,
			uint32_t ipa_desc_size, void *ipa_priv,
			bool is_rm_enabled, uint32_t *tx_pipe_handle,
			uint32_t *rx_pipe_handle)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_ipa_resources *ipa_res;
	qdf_ipa_wdi_pipe_setup_info_t *tx;
	qdf_ipa_wdi_pipe_setup_info_t *rx;
	qdf_ipa_wdi_conn_in_params_t pipe_in;
	qdf_ipa_wdi_conn_out_params_t pipe_out;
	struct tcl_data_cmd *tcl_desc_ptr;
	uint8_t *desc_addr;
	uint32_t desc_size;
	int ret;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	ipa_res = &pdev->ipa_resource;
	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return QDF_STATUS_SUCCESS;

	qdf_mem_zero(&tx, sizeof(qdf_ipa_wdi_pipe_setup_info_t));
	qdf_mem_zero(&rx, sizeof(qdf_ipa_wdi_pipe_setup_info_t));
	qdf_mem_zero(&pipe_in, sizeof(pipe_in));
	qdf_mem_zero(&pipe_out, sizeof(pipe_out));

	/* TX PIPE */
	/*
	 * Transfer Ring: WBM Ring
	 * Transfer Ring Doorbell PA: WBM Tail Pointer Address
	 * Event Ring: TCL ring
	 * Event Ring Doorbell PA: TCL Head Pointer Address
	 */
	tx = &QDF_IPA_WDI_CONN_IN_PARAMS_TX(&pipe_in);
	QDF_IPA_WDI_SETUP_INFO_NAT_EN(tx) = IPA_BYPASS_NAT;
	QDF_IPA_WDI_SETUP_INFO_HDR_LEN(tx) = DP_IPA_UC_WLAN_TX_HDR_LEN;
	QDF_IPA_WDI_SETUP_INFO_HDR_OFST_PKT_SIZE_VALID(tx) = 0;
	QDF_IPA_WDI_SETUP_INFO_HDR_OFST_PKT_SIZE(tx) = 0;
	QDF_IPA_WDI_SETUP_INFO_HDR_ADDITIONAL_CONST_LEN(tx) = 0;
	QDF_IPA_WDI_SETUP_INFO_MODE(tx) = IPA_BASIC;
	QDF_IPA_WDI_SETUP_INFO_HDR_LITTLE_ENDIAN(tx) = true;
	QDF_IPA_WDI_SETUP_INFO_CLIENT(tx) = IPA_CLIENT_WLAN1_CONS;
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(tx) =
		ipa_res->tx_comp_ring_base_paddr;
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(tx) =
		ipa_res->tx_comp_ring_size;
	/* WBM Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(tx) =
		soc->ipa_uc_tx_rsc.ipa_wbm_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(tx) =
		ipa_res->tx_ring_base_paddr;
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(tx) = ipa_res->tx_ring_size;
	/* TCL Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(tx) =
		soc->ipa_uc_tx_rsc.ipa_tcl_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_NUM_PKT_BUFFERS(tx) =
		ipa_res->tx_num_alloc_buffer;
	QDF_IPA_WDI_SETUP_INFO_PKT_OFFSET(tx) = 0;

	/* Preprogram TCL descriptor */
	desc_addr =
		(uint8_t *)QDF_IPA_WDI_SETUP_INFO_DESC_FORMAT_TEMPLATE(tx);
	desc_size = sizeof(struct tcl_data_cmd);
	HAL_TX_DESC_SET_TLV_HDR(desc_addr, HAL_TX_TCL_DATA_TAG, desc_size);
	tcl_desc_ptr = (struct tcl_data_cmd *)
		(QDF_IPA_WDI_SETUP_INFO_DESC_FORMAT_TEMPLATE(tx) + 1);
	tcl_desc_ptr->buf_addr_info.return_buffer_manager =
						HAL_RX_BUF_RBM_SW2_BM;
	tcl_desc_ptr->addrx_en = 1;	/* Address X search enable in ASE */
	tcl_desc_ptr->encap_type = HAL_TX_ENCAP_TYPE_ETHERNET;
	tcl_desc_ptr->packet_offset = 2;	/* padding for alignment */

	/* RX PIPE */
	/*
	 * Transfer Ring: REO Ring
	 * Transfer Ring Doorbell PA: REO Tail Pointer Address
	 * Event Ring: FW ring
	 * Event Ring Doorbell PA: FW Head Pointer Address
	 */
	rx = &QDF_IPA_WDI_CONN_IN_PARAMS_RX(&pipe_in);
	QDF_IPA_WDI_SETUP_INFO_NAT_EN(rx) = IPA_BYPASS_NAT;
	QDF_IPA_WDI_SETUP_INFO_HDR_LEN(rx) = DP_IPA_UC_WLAN_RX_HDR_LEN;
	QDF_IPA_WDI_SETUP_INFO_HDR_OFST_PKT_SIZE_VALID(rx) = 0;
	QDF_IPA_WDI_SETUP_INFO_HDR_OFST_PKT_SIZE(rx) = 0;
	QDF_IPA_WDI_SETUP_INFO_HDR_ADDITIONAL_CONST_LEN(rx) = 0;
	QDF_IPA_WDI_SETUP_INFO_HDR_OFST_METADATA_VALID(rx) = 0;
	QDF_IPA_WDI_SETUP_INFO_HDR_METADATA_REG_VALID(rx) = 1;
	QDF_IPA_WDI_SETUP_INFO_MODE(rx) = IPA_BASIC;
	QDF_IPA_WDI_SETUP_INFO_HDR_LITTLE_ENDIAN(rx) = true;
	QDF_IPA_WDI_SETUP_INFO_CLIENT(rx) = IPA_CLIENT_WLAN1_PROD;
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(rx) =
						ipa_res->rx_rdy_ring_base_paddr;
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(rx) =
						ipa_res->rx_rdy_ring_size;
	/* REO Tail Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(rx) =
					soc->ipa_uc_rx_rsc.ipa_reo_tp_paddr;
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(rx) =
					ipa_res->rx_refill_ring_base_paddr;
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(rx) =
						ipa_res->rx_refill_ring_size;
	/* FW Head Pointer Address */
	QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(rx) =
				soc->ipa_uc_rx_rsc.ipa_rx_refill_buf_hp_paddr;
	QDF_IPA_WDI_SETUP_INFO_PKT_OFFSET(rx) = soc->rx_pkt_tlv_size +
						L3_HEADER_PADDING;
	QDF_IPA_WDI_CONN_IN_PARAMS_NOTIFY(&pipe_in) = ipa_w2i_cb;
	QDF_IPA_WDI_CONN_IN_PARAMS_PRIV(&pipe_in) = ipa_priv;

	/* Connect WDI IPA PIPE */
	ret = qdf_ipa_wdi_conn_pipes(&pipe_in, &pipe_out);
	if (ret) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: ipa_wdi_conn_pipes: IPA pipe setup failed: ret=%d",
			  __func__, ret);
		return QDF_STATUS_E_FAILURE;
	}

	/* IPA uC Doorbell registers */
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Tx DB PA=0x%x, Rx DB PA=0x%x",
		  __func__,
		(unsigned int)QDF_IPA_WDI_CONN_OUT_PARAMS_TX_UC_DB_PA(&pipe_out),
		(unsigned int)QDF_IPA_WDI_CONN_OUT_PARAMS_RX_UC_DB_PA(&pipe_out));

	ipa_res->tx_comp_doorbell_paddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_TX_UC_DB_PA(&pipe_out);
	ipa_res->tx_comp_doorbell_vaddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_TX_UC_DB_VA(&pipe_out);
	ipa_res->rx_ready_doorbell_paddr =
		QDF_IPA_WDI_CONN_OUT_PARAMS_RX_UC_DB_PA(&pipe_out);

	soc->ipa_first_tx_db_access = true;

	qdf_spinlock_create(&soc->ipa_rx_buf_map_lock);
	soc->ipa_rx_buf_map_lock_initialized = true;

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Tx: %s=%pK, %s=%d, %s=%pK, %s=%pK, %s=%d, %s=%pK, %s=%d, %s=%pK",
		  __func__,
		  "transfer_ring_base_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(tx),
		  "transfer_ring_size",
		  QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(tx),
		  "transfer_ring_doorbell_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(tx),
		  "event_ring_base_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(tx),
		  "event_ring_size",
		  QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(tx),
		  "event_ring_doorbell_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(tx),
		  "num_pkt_buffers",
		  QDF_IPA_WDI_SETUP_INFO_NUM_PKT_BUFFERS(tx),
		  "tx_comp_doorbell_paddr",
		  (void *)ipa_res->tx_comp_doorbell_paddr);

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Rx: %s=%pK, %s=%d, %s=%pK, %s=%pK, %s=%d, %s=%pK, %s=%d, %s=%pK",
		  __func__,
		  "transfer_ring_base_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_BASE_PA(rx),
		  "transfer_ring_size",
		  QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_SIZE(rx),
		  "transfer_ring_doorbell_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_TRANSFER_RING_DOORBELL_PA(rx),
		  "event_ring_base_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_EVENT_RING_BASE_PA(rx),
		  "event_ring_size",
		  QDF_IPA_WDI_SETUP_INFO_EVENT_RING_SIZE(rx),
		  "event_ring_doorbell_pa",
		  (void *)QDF_IPA_WDI_SETUP_INFO_EVENT_RING_DOORBELL_PA(rx),
		  "num_pkt_buffers",
		  QDF_IPA_WDI_SETUP_INFO_NUM_PKT_BUFFERS(rx),
		  "tx_comp_doorbell_paddr",
		  (void *)ipa_res->rx_ready_doorbell_paddr);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_setup_iface(char *ifname, uint8_t *mac_addr,
			      qdf_ipa_client_type_t prod_client,
			      qdf_ipa_client_type_t cons_client,
			      uint8_t session_id, bool is_ipv6_enabled,
			      qdf_ipa_wdi_hdl_t hdl)
{
	qdf_ipa_wdi_reg_intf_in_params_t in;
	qdf_ipa_wdi_hdr_info_t hdr_info;
	struct dp_ipa_uc_tx_hdr uc_tx_hdr;
	struct dp_ipa_uc_tx_hdr uc_tx_hdr_v6;
	int ret = -EINVAL;

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Add Partial hdr: %s, "QDF_MAC_ADDR_FMT,
		  __func__, ifname, QDF_MAC_ADDR_REF(mac_addr));

	qdf_mem_zero(&hdr_info, sizeof(qdf_ipa_wdi_hdr_info_t));
	qdf_ether_addr_copy(uc_tx_hdr.eth.h_source, mac_addr);

	/* IPV4 header */
	uc_tx_hdr.eth.h_proto = qdf_htons(ETH_P_IP);

	QDF_IPA_WDI_HDR_INFO_HDR(&hdr_info) = (uint8_t *)&uc_tx_hdr;
	QDF_IPA_WDI_HDR_INFO_HDR_LEN(&hdr_info) = DP_IPA_UC_WLAN_TX_HDR_LEN;
	QDF_IPA_WDI_HDR_INFO_HDR_TYPE(&hdr_info) = IPA_HDR_L2_ETHERNET_II;
	QDF_IPA_WDI_HDR_INFO_DST_MAC_ADDR_OFFSET(&hdr_info) =
		DP_IPA_UC_WLAN_HDR_DES_MAC_OFFSET;

	QDF_IPA_WDI_REG_INTF_IN_PARAMS_NETDEV_NAME(&in) = ifname;
	qdf_mem_copy(&(QDF_IPA_WDI_REG_INTF_IN_PARAMS_HDR_INFO(&in)[IPA_IP_v4]),
		     &hdr_info, sizeof(qdf_ipa_wdi_hdr_info_t));
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_IS_META_DATA_VALID(&in) = 1;
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA(&in) =
		htonl(session_id << 16);
	QDF_IPA_WDI_REG_INTF_IN_PARAMS_META_DATA_MASK(&in) = htonl(0x00FF0000);

	/* IPV6 header */
	if (is_ipv6_enabled) {
		qdf_mem_copy(&uc_tx_hdr_v6, &uc_tx_hdr,
			     DP_IPA_UC_WLAN_TX_HDR_LEN);
		uc_tx_hdr_v6.eth.h_proto = qdf_htons(ETH_P_IPV6);
		QDF_IPA_WDI_HDR_INFO_HDR(&hdr_info) = (uint8_t *)&uc_tx_hdr_v6;
		qdf_mem_copy(&(QDF_IPA_WDI_REG_INTF_IN_PARAMS_HDR_INFO(&in)[IPA_IP_v6]),
			     &hdr_info, sizeof(qdf_ipa_wdi_hdr_info_t));
	}

	ret = qdf_ipa_wdi_reg_intf(&in);
	if (ret) {
		dp_err("ipa_wdi_reg_intf: register IPA interface failed: ret=%d",
		       ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#endif /* CONFIG_IPA_WDI_UNIFIED_API */

QDF_STATUS dp_ipa_cleanup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			  uint32_t tx_pipe_handle, uint32_t rx_pipe_handle,
			  qdf_ipa_wdi_hdl_t hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_pdev *pdev;
	int ret;

	ret = qdf_ipa_wdi_disconn_pipes(hdl);
	if (ret) {
		dp_err("ipa_wdi_disconn_pipes: IPA pipe cleanup failed: ret=%d",
		       ret);
		status = QDF_STATUS_E_FAILURE;
	}

	if (soc->ipa_rx_buf_map_lock_initialized) {
		qdf_spinlock_destroy(&soc->ipa_rx_buf_map_lock);
		soc->ipa_rx_buf_map_lock_initialized = false;
	}

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	if (qdf_unlikely(!pdev)) {
		dp_err_rl("Invalid pdev for pdev_id %d", pdev_id);
		status = QDF_STATUS_E_FAILURE;
		goto exit;
	}

	dp_ipa_unmap_ring_doorbell_paddr(pdev);
	dp_ipa_unmap_rx_alt_ring_doorbell_paddr(pdev);
exit:
	return status;
}

QDF_STATUS dp_ipa_cleanup_iface(char *ifname, bool is_ipv6_enabled,
				qdf_ipa_wdi_hdl_t hdl)
{
	int ret;

	ret = qdf_ipa_wdi_dereg_intf(ifname, hdl);
	if (ret) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: ipa_wdi_dereg_intf: IPA pipe deregistration failed: ret=%d",
			  __func__, ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef IPA_SET_RESET_TX_DB_PA
#define DP_IPA_EP_SET_TX_DB_PA(soc, ipa_res) \
				dp_ipa_set_tx_doorbell_paddr((soc), (ipa_res))
#define DP_IPA_RESET_TX_DB_PA(soc, ipa_res) \
				dp_ipa_reset_tx_doorbell_pa((soc), (ipa_res))
#else
#define DP_IPA_EP_SET_TX_DB_PA(soc, ipa_res)
#define DP_IPA_RESET_TX_DB_PA(soc, ipa_res)
#endif

QDF_STATUS dp_ipa_enable_pipes(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			       qdf_ipa_wdi_hdl_t hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_ipa_resources *ipa_res;
	QDF_STATUS result;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	ipa_res = &pdev->ipa_resource;

	qdf_atomic_set(&soc->ipa_pipes_enabled, 1);
	DP_IPA_EP_SET_TX_DB_PA(soc, ipa_res);

	if (!ipa_config_is_opt_wifi_dp_enabled()) {
		qdf_atomic_set(&soc->ipa_map_allowed, 1);
		dp_ipa_handle_rx_buf_pool_smmu_mapping(soc, pdev, true,
						       __func__, __LINE__);
	}

	result = qdf_ipa_wdi_enable_pipes(hdl);
	if (result) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Enable WDI PIPE fail, code %d",
			  __func__, result);
		qdf_atomic_set(&soc->ipa_pipes_enabled, 0);
		DP_IPA_RESET_TX_DB_PA(soc, ipa_res);
		if (qdf_atomic_read(&soc->ipa_map_allowed)) {
			qdf_atomic_set(&soc->ipa_map_allowed, 0);
			dp_ipa_handle_rx_buf_pool_smmu_mapping(
					soc, pdev, false, __func__, __LINE__);
		}
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ipa_first_tx_db_access) {
		dp_ipa_tx_comp_ring_init_hp(soc, ipa_res);
		soc->ipa_first_tx_db_access = false;
	} else {
		dp_ipa_tx_comp_ring_update_hp_addr(soc, ipa_res);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_disable_pipes(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				qdf_ipa_wdi_hdl_t hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	QDF_STATUS result;
	struct dp_ipa_resources *ipa_res;

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	ipa_res = &pdev->ipa_resource;

	qdf_sleep(TX_COMP_DRAIN_WAIT_TIMEOUT_MS);
	/*
	 * Reset the tx completion doorbell address before invoking IPA disable
	 * pipes API to ensure that there is no access to IPA tx doorbell
	 * address post disable pipes.
	 */
	DP_IPA_RESET_TX_DB_PA(soc, ipa_res);

	result = qdf_ipa_wdi_disable_pipes(hdl);
	if (result) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Disable WDI PIPE fail, code %d",
			  __func__, result);
		qdf_assert_always(0);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_atomic_set(&soc->ipa_pipes_enabled, 0);

	if (!ipa_config_is_opt_wifi_dp_enabled()) {
		qdf_atomic_set(&soc->ipa_map_allowed, 0);
		dp_ipa_handle_rx_buf_pool_smmu_mapping(soc, pdev, false,
						       __func__, __LINE__);
	}

	return result ? QDF_STATUS_E_FAILURE : QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_set_perf_level(int client, uint32_t max_supported_bw_mbps,
				 qdf_ipa_wdi_hdl_t hdl)
{
	qdf_ipa_wdi_perf_profile_t profile;
	QDF_STATUS result;

	profile.client = client;
	profile.max_supported_bw_mbps = max_supported_bw_mbps;

	result = qdf_ipa_wdi_set_perf_profile(hdl, &profile);
	if (result) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: ipa_wdi_set_perf_profile fail, code %d",
			  __func__, result);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_WDS_EXTENDED
/**
 * dp_ipa_rx_wdsext_iface() - Forward RX exception packets to wdsext interface
 * @soc_hdl: data path soc handle
 * @peer_id: Peer id to get respective peer
 * @skb: socket buffer
 *
 * Return: true on success, else false
 */
bool dp_ipa_rx_wdsext_iface(struct cdp_soc_t *soc_hdl, uint8_t peer_id,
			    qdf_nbuf_t skb)
{
	struct dp_txrx_peer *txrx_peer;
	dp_txrx_ref_handle txrx_ref_handle = NULL;
	struct dp_soc *dp_soc = cdp_soc_t_to_dp_soc(soc_hdl);
	bool status = false;

	txrx_peer = dp_tgt_txrx_peer_get_ref_by_id(soc_hdl, peer_id,
						   &txrx_ref_handle,
						   DP_MOD_ID_IPA);

	if (qdf_likely(txrx_peer)) {
		if (dp_rx_deliver_to_stack_ext(dp_soc, txrx_peer->vdev,
					       txrx_peer, skb)
			status =  true;
		dp_txrx_peer_unref_delete(txrx_ref_handle, DP_MOD_ID_IPA);
	}
	return status;
}
#endif

/**
 * dp_ipa_intrabss_send() - send IPA RX intra-bss frames
 * @pdev: pdev
 * @vdev: vdev
 * @nbuf: skb
 *
 * Return: nbuf if TX fails and NULL if TX succeeds
 */
static qdf_nbuf_t dp_ipa_intrabss_send(struct dp_pdev *pdev,
				       struct dp_vdev *vdev,
				       qdf_nbuf_t nbuf)
{
	struct dp_peer *vdev_peer;
	uint16_t len;

	vdev_peer = dp_vdev_bss_peer_ref_n_get(pdev->soc, vdev, DP_MOD_ID_IPA);
	if (qdf_unlikely(!vdev_peer))
		return nbuf;

	if (qdf_unlikely(!vdev_peer->txrx_peer)) {
		dp_peer_unref_delete(vdev_peer, DP_MOD_ID_IPA);
		return nbuf;
	}

	qdf_mem_zero(nbuf->cb, sizeof(nbuf->cb));
	len = qdf_nbuf_len(nbuf);

	if (dp_tx_send((struct cdp_soc_t *)pdev->soc, vdev->vdev_id, nbuf)) {
		DP_PEER_PER_PKT_STATS_INC_PKT(vdev_peer->txrx_peer,
					      rx.intra_bss.fail, 1, len,
					      0);
		dp_peer_unref_delete(vdev_peer, DP_MOD_ID_IPA);
		return nbuf;
	}

	DP_PEER_PER_PKT_STATS_INC_PKT(vdev_peer->txrx_peer,
				      rx.intra_bss.pkts, 1, len, 0);
	dp_peer_unref_delete(vdev_peer, DP_MOD_ID_IPA);
	return NULL;
}

#ifdef IPA_OPT_WIFI_DP
/**
 * dp_ipa_rx_super_rule_setup()- pass cce super rule params to fw from ipa
 *
 * @soc_hdl: cdp soc
 * @flt_params: filter tuple
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ipa_rx_super_rule_setup(struct cdp_soc_t *soc_hdl,
				      void *flt_params)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	return htt_h2t_rx_cce_super_rule_setup(soc->htt_handle, flt_params);
}

/**
 * dp_ipa_wdi_opt_dpath_notify_flt_add_rem_cb()- send cce super rule filter
 * add/remove result to ipa
 *
 * @flt0_rslt : result for filter0 add/remove
 * @flt1_rslt : result for filter1 add/remove
 *
 * Return: void
 */
void dp_ipa_wdi_opt_dpath_notify_flt_add_rem_cb(int flt0_rslt, int flt1_rslt)
{
	wlan_ipa_wdi_opt_dpath_notify_flt_add_rem_cb(flt0_rslt, flt1_rslt);
}

int dp_ipa_pcie_link_up(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;
	int response = 0;

	response = hif_prevent_l1((hal_soc->hif_handle));
	return response;
}

void dp_ipa_pcie_link_down(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;

	hif_allow_l1(hal_soc->hif_handle);
}

/**
 * dp_ipa_wdi_opt_dpath_notify_flt_rlsd()- send cce super rule release
 * notification to ipa
 *
 * @flt0_rslt : result for filter0 release
 * @flt1_rslt : result for filter1 release
 *
 *Return: void
 */
void dp_ipa_wdi_opt_dpath_notify_flt_rlsd(int flt0_rslt, int flt1_rslt)
{
	wlan_ipa_wdi_opt_dpath_notify_flt_rlsd(flt0_rslt, flt1_rslt);
}

/**
 * dp_ipa_wdi_opt_dpath_notify_flt_rsvd()- send cce super rule reserve
 * notification to ipa
 *
 *@is_success : result of filter reservatiom
 *
 *Return: void
 */
void dp_ipa_wdi_opt_dpath_notify_flt_rsvd(bool is_success)
{
	wlan_ipa_wdi_opt_dpath_notify_flt_rsvd(is_success);
}
#endif

#ifdef IPA_WDS_EASYMESH_FEATURE
/**
 * dp_ipa_peer_check() - Check for peer for given mac
 * @soc: dp soc object
 * @peer_mac_addr: peer mac address
 * @vdev_id: vdev id
 *
 * Return: true if peer is found, else false
 */
static inline bool dp_ipa_peer_check(struct dp_soc *soc,
				     uint8_t *peer_mac_addr, uint8_t vdev_id)
{
	struct dp_ast_entry *ast_entry = NULL;
	struct dp_peer *peer = NULL;

	qdf_spin_lock_bh(&soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_soc(soc, peer_mac_addr);

	if ((!ast_entry) ||
	    (ast_entry->delete_in_progress && !ast_entry->callback)) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return false;
	}

	peer = dp_peer_get_ref_by_id(soc, ast_entry->peer_id,
				     DP_MOD_ID_IPA);

	if (!peer) {
		qdf_spin_unlock_bh(&soc->ast_lock);
		return false;
	} else {
		if (peer->vdev->vdev_id == vdev_id) {
			dp_peer_unref_delete(peer, DP_MOD_ID_IPA);
			qdf_spin_unlock_bh(&soc->ast_lock);
			return true;
		}
		dp_peer_unref_delete(peer, DP_MOD_ID_IPA);
		qdf_spin_unlock_bh(&soc->ast_lock);
		return false;
	}
}
#else
static inline bool dp_ipa_peer_check(struct dp_soc *soc,
				     uint8_t *peer_mac_addr, uint8_t vdev_id)
{
	struct cdp_peer_info peer_info = {0};
	struct dp_peer *peer = NULL;

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac_addr, false,
				 CDP_WILD_PEER_TYPE);

	peer = dp_peer_hash_find_wrapper(soc, &peer_info, DP_MOD_ID_IPA);
	if (peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_IPA);
		return true;
	} else {
		return false;
	}
}
#endif

bool dp_ipa_rx_intrabss_fwd(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			    qdf_nbuf_t nbuf, bool *fwd_success)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_IPA);
	struct dp_pdev *pdev;
	qdf_nbuf_t nbuf_copy;
	uint8_t da_is_bcmc;
	struct ethhdr *eh;
	bool status = false;

	*fwd_success = false; /* set default as failure */

	/*
	 * WDI 3.0 skb->cb[] info from IPA driver
	 * skb->cb[0] = vdev_id
	 * skb->cb[1].bit#1 = da_is_bcmc
	 */
	da_is_bcmc = ((uint8_t)nbuf->cb[1]) & 0x2;

	if (qdf_unlikely(!vdev))
		return false;

	pdev = vdev->pdev;
	if (qdf_unlikely(!pdev))
		goto out;

	/* no fwd for station mode and just pass up to stack */
	if (vdev->opmode == wlan_op_mode_sta)
		goto out;

	if (da_is_bcmc) {
		nbuf_copy = qdf_nbuf_copy(nbuf);
		if (!nbuf_copy)
			goto out;

		if (dp_ipa_intrabss_send(pdev, vdev, nbuf_copy))
			qdf_nbuf_free(nbuf_copy);
		else
			*fwd_success = true;

		/* return false to pass original pkt up to stack */
		goto out;
	}

	eh = (struct ethhdr *)qdf_nbuf_data(nbuf);

	if (!qdf_mem_cmp(eh->h_dest, vdev->mac_addr.raw, QDF_MAC_ADDR_SIZE))
		goto out;

	if (!dp_ipa_peer_check(soc, eh->h_dest, vdev->vdev_id))
		goto out;

	if (!dp_ipa_peer_check(soc, eh->h_source, vdev->vdev_id))
		goto out;

	/*
	 * In intra-bss forwarding scenario, skb is allocated by IPA driver.
	 * Need to add skb to internal tracking table to avoid nbuf memory
	 * leak check for unallocated skb.
	 */
	qdf_net_buf_debug_acquire_skb(nbuf, __FILE__, __LINE__);

	if (dp_ipa_intrabss_send(pdev, vdev, nbuf))
		qdf_nbuf_free(nbuf);
	else
		*fwd_success = true;

	status = true;
out:
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_IPA);
	return status;
}

#ifdef MDM_PLATFORM
bool dp_ipa_is_mdm_platform(void)
{
	return true;
}
#else
bool dp_ipa_is_mdm_platform(void)
{
	return false;
}
#endif

/**
 * dp_ipa_frag_nbuf_linearize() - linearize nbuf for IPA
 * @soc: soc
 * @nbuf: source skb
 *
 * Return: new nbuf if success and otherwise NULL
 */
static qdf_nbuf_t dp_ipa_frag_nbuf_linearize(struct dp_soc *soc,
					     qdf_nbuf_t nbuf)
{
	uint8_t *src_nbuf_data;
	uint8_t *dst_nbuf_data;
	qdf_nbuf_t dst_nbuf;
	qdf_nbuf_t temp_nbuf = nbuf;
	uint32_t nbuf_len = qdf_nbuf_len(nbuf);
	bool is_nbuf_head = true;
	uint32_t copy_len = 0;
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	dst_nbuf = qdf_nbuf_alloc(soc->osdev, buf_size,
				  RX_BUFFER_RESERVATION,
				  RX_DATA_BUFFER_ALIGNMENT, FALSE);

	if (!dst_nbuf) {
		dp_err_rl("nbuf allocate fail");
		return NULL;
	}

	if ((nbuf_len + L3_HEADER_PADDING) > buf_size) {
		qdf_nbuf_free(dst_nbuf);
		dp_err_rl("nbuf is jumbo data");
		return NULL;
	}

	/* prepeare to copy all data into new skb */
	dst_nbuf_data = qdf_nbuf_data(dst_nbuf);
	while (temp_nbuf) {
		src_nbuf_data = qdf_nbuf_data(temp_nbuf);
		/* first head nbuf */
		if (is_nbuf_head) {
			qdf_mem_copy(dst_nbuf_data, src_nbuf_data,
				     soc->rx_pkt_tlv_size);
			/* leave extra 2 bytes L3_HEADER_PADDING */
			dst_nbuf_data += (soc->rx_pkt_tlv_size +
					  L3_HEADER_PADDING);
			src_nbuf_data += soc->rx_pkt_tlv_size;
			copy_len = qdf_nbuf_headlen(temp_nbuf) -
						soc->rx_pkt_tlv_size;
			temp_nbuf = qdf_nbuf_get_ext_list(temp_nbuf);
			is_nbuf_head = false;
		} else {
			copy_len = qdf_nbuf_len(temp_nbuf);
			temp_nbuf = qdf_nbuf_queue_next(temp_nbuf);
		}
		qdf_mem_copy(dst_nbuf_data, src_nbuf_data, copy_len);
		dst_nbuf_data += copy_len;
	}

	qdf_nbuf_set_len(dst_nbuf, nbuf_len);
	/* copy is done, free original nbuf */
	qdf_nbuf_free(nbuf);

	return dst_nbuf;
}

qdf_nbuf_t dp_ipa_handle_rx_reo_reinject(struct dp_soc *soc, qdf_nbuf_t nbuf)
{

	if (!wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx))
		return nbuf;

	/* WLAN IPA is run-time disabled */
	if (!qdf_atomic_read(&soc->ipa_pipes_enabled))
		return nbuf;

	if (!qdf_nbuf_is_frag(nbuf))
		return nbuf;

	/* linearize skb for IPA */
	return dp_ipa_frag_nbuf_linearize(soc, nbuf);
}

QDF_STATUS dp_ipa_tx_buf_smmu_mapping(
	struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	const char *func, uint32_t line)
{
	QDF_STATUS ret;

	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	if (!qdf_mem_smmu_s1_enabled(soc->osdev)) {
		dp_debug("SMMU S1 disabled");
		return QDF_STATUS_SUCCESS;
	}
	ret = __dp_ipa_tx_buf_smmu_mapping(soc, pdev, true, func, line);
	if (ret)
		return ret;

	ret = dp_ipa_tx_alt_buf_smmu_mapping(soc, pdev, true, func, line);
	if (ret)
		__dp_ipa_tx_buf_smmu_mapping(soc, pdev, false, func, line);
	return ret;
}

QDF_STATUS dp_ipa_tx_buf_smmu_unmapping(
	struct cdp_soc_t *soc_hdl, uint8_t pdev_id, const char *func,
	uint32_t line)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!qdf_mem_smmu_s1_enabled(soc->osdev)) {
		dp_debug("SMMU S1 disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!pdev) {
		dp_err("Invalid pdev instance pdev_id:%d", pdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (__dp_ipa_tx_buf_smmu_mapping(soc, pdev, false, func, line) ||
	    dp_ipa_tx_alt_buf_smmu_mapping(soc, pdev, false, func, line))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_ipa_rx_buf_pool_smmu_mapping(
	struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	bool create, const char *func, uint32_t line)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("Invalid instance");
		return QDF_STATUS_E_FAILURE;
	}

	if (!qdf_mem_smmu_s1_enabled(soc->osdev)) {
		dp_debug("SMMU S1 disabled");
		return QDF_STATUS_SUCCESS;
	}

	dp_ipa_handle_rx_buf_pool_smmu_mapping(soc, pdev, create, func, line);
	return QDF_STATUS_SUCCESS;
}
#ifdef IPA_WDS_EASYMESH_FEATURE
QDF_STATUS dp_ipa_ast_create(struct cdp_soc_t *soc_hdl,
			     qdf_ipa_ast_info_type_t *data)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	uint8_t *rx_tlv_hdr;
	struct dp_peer *peer;
	struct hal_rx_msdu_metadata msdu_metadata;
	qdf_ipa_ast_info_type_t *ast_info;

	if (!data) {
		dp_err("Data is NULL !!!");
		return QDF_STATUS_E_FAILURE;
	}
	ast_info = data;

	rx_tlv_hdr = qdf_nbuf_data(ast_info->skb);
	peer = dp_peer_get_ref_by_id(soc, ast_info->ta_peer_id,
				     DP_MOD_ID_IPA);
	if (!peer) {
		dp_err("Peer is NULL !!!!");
		return QDF_STATUS_E_FAILURE;
	}

	hal_rx_msdu_metadata_get(soc->hal_soc, rx_tlv_hdr, &msdu_metadata);

	dp_rx_ipa_wds_srcport_learn(soc, peer, ast_info->skb, msdu_metadata,
				    ast_info->mac_addr_ad4_valid,
				    ast_info->first_msdu_in_mpdu_flag);

	dp_peer_unref_delete(peer, DP_MOD_ID_IPA);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef QCA_ENHANCED_STATS_SUPPORT
QDF_STATUS dp_ipa_update_peer_rx_stats(struct cdp_soc_t *soc,
				       uint8_t vdev_id, uint8_t *peer_mac,
				       qdf_nbuf_t nbuf)
{
	struct dp_peer *peer = dp_peer_find_hash_find((struct dp_soc *)soc,
						      peer_mac, 0, vdev_id,
						      DP_MOD_ID_IPA);
	struct dp_txrx_peer *txrx_peer;
	uint8_t da_is_bcmc;
	qdf_ether_header_t *eh;

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	txrx_peer = dp_get_txrx_peer(peer);

	if (!txrx_peer) {
		dp_peer_unref_delete(peer, DP_MOD_ID_IPA);
		return QDF_STATUS_E_FAILURE;
	}

	da_is_bcmc = ((uint8_t)nbuf->cb[1]) & 0x2;
	eh = (qdf_ether_header_t *)qdf_nbuf_data(nbuf);

	if (da_is_bcmc) {
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.multicast, 1,
					      qdf_nbuf_len(nbuf), 0);
		if (QDF_IS_ADDR_BROADCAST(eh->ether_dhost))
			DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.bcast,
						      1, qdf_nbuf_len(nbuf), 0);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_IPA);

	return QDF_STATUS_SUCCESS;
}

void
dp_peer_aggregate_tid_stats(struct dp_peer *peer)
{
	uint8_t i = 0;
	struct dp_rx_tid *rx_tid = NULL;
	struct cdp_pkt_info rx_total = {0};
	struct dp_txrx_peer *txrx_peer = NULL;

	if (!peer->rx_tid)
		return;

	txrx_peer = dp_get_txrx_peer(peer);

	if (!txrx_peer)
		return;

	for (i = 0; i < DP_MAX_TIDS; i++) {
		rx_tid = &peer->rx_tid[i];
		rx_total.num += rx_tid->rx_msdu_cnt.num;
		rx_total.bytes += rx_tid->rx_msdu_cnt.bytes;
	}

	DP_PEER_PER_PKT_STATS_UPD(txrx_peer, rx.rx_total.num,
				  rx_total.num, 0);
	DP_PEER_PER_PKT_STATS_UPD(txrx_peer, rx.rx_total.bytes,
				  rx_total.bytes, 0);
}

/**
 * dp_ipa_update_vdev_stats(): update vdev stats
 * @soc: soc handle
 * @srcobj: DP_PEER object
 * @arg: point to vdev stats structure
 *
 * Return: void
 */
static inline
void dp_ipa_update_vdev_stats(struct dp_soc *soc, struct dp_peer *srcobj,
			      void *arg)
{
	dp_peer_aggregate_tid_stats(srcobj);
	dp_update_vdev_stats(soc, srcobj, arg);
}

/**
 * dp_ipa_aggregate_vdev_stats - Aggregate vdev_stats
 * @vdev: Data path vdev
 * @vdev_stats: buffer to hold vdev stats
 *
 * Return: void
 */
static inline
void dp_ipa_aggregate_vdev_stats(struct dp_vdev *vdev,
				 struct cdp_vdev_stats *vdev_stats)
{
	struct dp_soc *soc = NULL;

	if (!vdev || !vdev->pdev)
		return;

	soc = vdev->pdev->soc;
	dp_update_vdev_ingress_stats(vdev);
	dp_copy_vdev_stats_to_tgt_buf(vdev_stats, &vdev->stats, DP_XMIT_LINK);
	dp_vdev_iterate_peer(vdev, dp_ipa_update_vdev_stats, vdev_stats,
			     DP_MOD_ID_GENERIC_STATS);
	dp_update_vdev_rate_stats(vdev_stats, &vdev->stats);

	vdev_stats->tx.ucast.num = vdev_stats->tx.tx_ucast_total.num;
	vdev_stats->tx.ucast.bytes = vdev_stats->tx.tx_ucast_total.bytes;
	vdev_stats->tx.tx_success.num = vdev_stats->tx.tx_ucast_success.num;
	vdev_stats->tx.tx_success.bytes = vdev_stats->tx.tx_ucast_success.bytes;

	if (vdev_stats->rx.rx_total.num >= vdev_stats->rx.multicast.num)
		vdev_stats->rx.unicast.num = vdev_stats->rx.rx_total.num -
					vdev_stats->rx.multicast.num;
	if (vdev_stats->rx.rx_total.bytes >=  vdev_stats->rx.multicast.bytes)
		vdev_stats->rx.unicast.bytes = vdev_stats->rx.rx_total.bytes -
					vdev_stats->rx.multicast.bytes;
	vdev_stats->rx.to_stack.num = vdev_stats->rx.rx_total.num;
	vdev_stats->rx.to_stack.bytes = vdev_stats->rx.rx_total.bytes;
}

/**
 * dp_ipa_aggregate_pdev_stats - Aggregate pdev stats
 * @pdev: Data path pdev
 *
 * Return: void
 */
static inline
void dp_ipa_aggregate_pdev_stats(struct dp_pdev *pdev)
{
	struct dp_vdev *vdev = NULL;
	struct dp_soc *soc;
	struct cdp_vdev_stats *vdev_stats =
			qdf_mem_malloc_atomic(sizeof(struct cdp_vdev_stats));

	if (!vdev_stats) {
		dp_err("%pK: DP alloc failure - unable to get alloc vdev stats",
		       pdev->soc);
		return;
	}

	soc = pdev->soc;

	qdf_mem_zero(&pdev->stats.tx, sizeof(pdev->stats.tx));
	qdf_mem_zero(&pdev->stats.rx, sizeof(pdev->stats.rx));
	qdf_mem_zero(&pdev->stats.tx_i, sizeof(pdev->stats.tx_i));
	qdf_mem_zero(&pdev->stats.rx_i, sizeof(pdev->stats.rx_i));

	qdf_spin_lock_bh(&pdev->vdev_list_lock);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		dp_ipa_aggregate_vdev_stats(vdev, vdev_stats);
		dp_update_pdev_stats(pdev, vdev_stats);
		dp_update_pdev_ingress_stats(pdev, vdev);
	}
	qdf_spin_unlock_bh(&pdev->vdev_list_lock);
	qdf_mem_free(vdev_stats);
}

/**
 * dp_ipa_get_peer_stats - Get peer stats
 * @peer: Data path peer
 * @peer_stats: buffer to hold peer stats
 *
 * Return: void
 */
static
void dp_ipa_get_peer_stats(struct dp_peer *peer,
			   struct cdp_peer_stats *peer_stats)
{
	dp_peer_aggregate_tid_stats(peer);
	dp_get_peer_stats(peer, peer_stats);

	peer_stats->tx.tx_success.num =
			peer_stats->tx.tx_ucast_success.num;
	peer_stats->tx.tx_success.bytes =
			peer_stats->tx.tx_ucast_success.bytes;
	peer_stats->tx.ucast.num =
			peer_stats->tx.tx_ucast_total.num;
	peer_stats->tx.ucast.bytes =
			peer_stats->tx.tx_ucast_total.bytes;

	if (peer_stats->rx.rx_total.num >=  peer_stats->rx.multicast.num)
		peer_stats->rx.unicast.num = peer_stats->rx.rx_total.num -
						peer_stats->rx.multicast.num;

	if (peer_stats->rx.rx_total.bytes >= peer_stats->rx.multicast.bytes)
		peer_stats->rx.unicast.bytes = peer_stats->rx.rx_total.bytes -
						peer_stats->rx.multicast.bytes;
}

QDF_STATUS
dp_ipa_txrx_get_pdev_stats(struct cdp_soc_t *soc, uint8_t pdev_id,
			   struct cdp_pdev_stats *pdev_stats)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	dp_ipa_aggregate_pdev_stats(pdev);
	qdf_mem_copy(pdev_stats, &pdev->stats, sizeof(struct cdp_pdev_stats));

	return QDF_STATUS_SUCCESS;
}

int dp_ipa_txrx_get_vdev_stats(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			       void *buf, bool is_aggregate)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct cdp_vdev_stats *vdev_stats;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_IPA);

	if (!vdev)
		return 1;

	vdev_stats = (struct cdp_vdev_stats *)buf;
	dp_ipa_aggregate_vdev_stats(vdev, buf);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_IPA);

	return 0;
}

QDF_STATUS dp_ipa_txrx_get_peer_stats(struct cdp_soc_t *soc, uint8_t vdev_id,
				      uint8_t *peer_mac,
				      struct cdp_peer_stats *peer_stats)
{
	struct dp_peer *peer = NULL;
	struct cdp_peer_info peer_info = { 0 };

	DP_PEER_INFO_PARAMS_INIT(&peer_info, vdev_id, peer_mac, false,
				 CDP_WILD_PEER_TYPE);

	peer = dp_peer_hash_find_wrapper((struct dp_soc *)soc, &peer_info,
					 DP_MOD_ID_IPA);

	qdf_mem_zero(peer_stats, sizeof(struct cdp_peer_stats));

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	dp_ipa_get_peer_stats(peer, peer_stats);
	dp_peer_unref_delete(peer, DP_MOD_ID_IPA);

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_ipa_get_wdi_version() - Get WDI version
 * @soc_hdl: data path soc handle
 * @wdi_ver: Out parameter for wdi version
 *
 * Get WDI version based on soc arch
 *
 * Return: None
 */
void dp_ipa_get_wdi_version(struct cdp_soc_t *soc_hdl, uint8_t *wdi_ver)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (soc->arch_ops.ipa_get_wdi_ver)
		soc->arch_ops.ipa_get_wdi_ver(wdi_ver);
	else
		*wdi_ver = IPA_WDI_3;
}

#ifdef IPA_WDI3_TX_TWO_PIPES
bool dp_ipa_is_ring_ipa_tx(struct dp_soc *soc, uint8_t ring_id)
{
	if (!soc->wlan_cfg_ctx->ipa_enabled)
		return false;

	return (ring_id == IPA_TCL_DATA_RING_IDX) ||
		((ring_id == IPA_TX_ALT_RING_IDX) &&
		 wlan_cfg_is_ipa_two_tx_pipes_enabled(soc->wlan_cfg_ctx));
}
#else
bool dp_ipa_is_ring_ipa_tx(struct dp_soc *soc, uint8_t ring_id)
{
	if (!soc->wlan_cfg_ctx->ipa_enabled)
		return false;

	return (ring_id == IPA_TCL_DATA_RING_IDX);
}
#endif /* IPA_WDI3_TX_TWO_PIPES */
#endif
