/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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

#include <wlan_utility.h>
#include <dp_internal.h>
#include "dp_rings.h"
#include <dp_htt.h>
#include "dp_be.h"
#include "dp_be_tx.h"
#include "dp_be_rx.h"
#ifdef WIFI_MONITOR_SUPPORT
#if !defined(DISABLE_MON_CONFIG) && (defined(WLAN_PKT_CAPTURE_TX_2_0) || \
	defined(WLAN_PKT_CAPTURE_RX_2_0))
#include "dp_mon_2.0.h"
#endif
#include "dp_mon.h"
#endif
#include <hal_be_api.h>
#ifdef WLAN_SUPPORT_PPEDS
#include "be/dp_ppeds.h"
#include <ppe_vp_public.h>
#include <ppe_drv_sc.h>
#endif

#ifdef WLAN_SUPPORT_PPEDS
static const char *ring_usage_dump[RING_USAGE_MAX] = {
	"100%",
	"Greater than 90%",
	"70 to 90%",
	"50 to 70%",
	"Less than 50%"
};
#endif

/* Generic AST entry aging timer value */
#define DP_AST_AGING_TIMER_DEFAULT_MS	5000

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define DP_TX_VDEV_ID_CHECK_ENABLE 0

static struct wlan_cfg_tcl_wbm_ring_num_map g_tcl_wbm_map_array[MAX_TCL_DATA_RINGS] = {
	{.tcl_ring_num = 0, .wbm_ring_num = 0, .wbm_rbm_id = HAL_BE_WBM_SW0_BM_ID, .for_ipa = 0},
	{1, 4, HAL_BE_WBM_SW4_BM_ID, 0},
	{2, 2, HAL_BE_WBM_SW2_BM_ID, 0},
#ifdef QCA_WIFI_KIWI_V2
	{3, 5, HAL_BE_WBM_SW5_BM_ID, 0},
	{4, 6, HAL_BE_WBM_SW6_BM_ID, 0}
#else
	{3, 6, HAL_BE_WBM_SW5_BM_ID, 0},
	{4, 7, HAL_BE_WBM_SW6_BM_ID, 0}
#endif
};
#else
#define DP_TX_VDEV_ID_CHECK_ENABLE 1

static struct wlan_cfg_tcl_wbm_ring_num_map g_tcl_wbm_map_array[MAX_TCL_DATA_RINGS] = {
	{.tcl_ring_num = 0, .wbm_ring_num = 0, .wbm_rbm_id = HAL_BE_WBM_SW0_BM_ID, .for_ipa = 0},
	{1, 1, HAL_BE_WBM_SW1_BM_ID, 0},
	{2, 2, HAL_BE_WBM_SW2_BM_ID, 0},
	{3, 3, HAL_BE_WBM_SW3_BM_ID, 0},
	{4, 4, HAL_BE_WBM_SW4_BM_ID, 0}
};
#endif

#ifdef WLAN_SUPPORT_PPEDS
static struct cdp_ppeds_txrx_ops dp_ops_ppeds_be = {
	.ppeds_entry_attach = dp_ppeds_attach_vdev_be,
	.ppeds_entry_detach = dp_ppeds_detach_vdev_be,
	.ppeds_set_int_pri2tid = dp_ppeds_set_int_pri2tid_be,
	.ppeds_update_int_pri2tid = dp_ppeds_update_int_pri2tid_be,
	.ppeds_entry_dump = dp_ppeds_dump_ppe_vp_tbl_be,
	.ppeds_enable_pri2tid = dp_ppeds_vdev_enable_pri2tid_be,
	.ppeds_vp_setup_recovery = dp_ppeds_vp_setup_on_fw_recovery,
	.ppeds_stats_sync = dp_ppeds_stats_sync_be,
};

static void dp_ppeds_rings_status(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	dp_print_ring_stat_from_hal(soc, &be_soc->reo2ppe_ring, REO2PPE);
	dp_print_ring_stat_from_hal(soc, &be_soc->ppe2tcl_ring, PPE2TCL);
	dp_print_ring_stat_from_hal(soc, &be_soc->ppeds_wbm_release_ring,
				    WBM2SW_RELEASE);
}

#ifdef GLOBAL_ASSERT_AVOIDANCE
void dp_ppeds_print_assert_war_stats(struct dp_soc_be *be_soc)
{
	DP_PRINT_STATS("PPE-DS Tx WAR stats: [%u] [%u] [%u]",
		       be_soc->ppeds_stats.tx.tx_comp_buf_src,
		       be_soc->ppeds_stats.tx.tx_comp_desc_null,
		       be_soc->ppeds_stats.tx.tx_comp_invalid_flag);
}

static void dp_ppeds_clear_assert_war_stats(struct dp_soc_be *be_soc)
{
	be_soc->ppeds_stats.tx.tx_comp_buf_src = 0;
	be_soc->ppeds_stats.tx.tx_comp_desc_null = 0;
	be_soc->ppeds_stats.tx.tx_comp_invalid_flag = 0;
}
#else
static void dp_ppeds_print_assert_war_stats(struct dp_soc_be *be_soc)
{
}

static void dp_ppeds_clear_assert_war_stats(struct dp_soc_be *be_soc)
{
}
#endif

static void dp_ppeds_inuse_desc(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	DP_PRINT_STATS("PPE-DS Tx Descriptors in Use = %u num_free %u",
		       be_soc->ppeds_tx_desc.num_allocated,
		       be_soc->ppeds_tx_desc.num_free);

	DP_PRINT_STATS("PPE-DS Tx desc alloc failed %u",
		       be_soc->ppeds_stats.tx.desc_alloc_failed);

	dp_ppeds_print_assert_war_stats(be_soc);
}

static void dp_ppeds_clear_stats(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	be_soc->ppeds_stats.tx.desc_alloc_failed = 0;
	dp_ppeds_clear_assert_war_stats(be_soc);
}

static void dp_ppeds_rings_stats(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int i = 0;

	DP_PRINT_STATS("Ring utilization statistics");
	DP_PRINT_STATS("WBM2SW_RELEASE");

	for (i = 0; i < RING_USAGE_MAX; i++)
		DP_PRINT_STATS("\t %s utilized %d instances",
			       ring_usage_dump[i],
			       be_soc->ppeds_wbm_release_ring.stats.util[i]);

	DP_PRINT_STATS("PPE2TCL");

	for (i = 0; i < RING_USAGE_MAX; i++)
		DP_PRINT_STATS("\t %s utilized %d instances",
			       ring_usage_dump[i],
			       be_soc->ppe2tcl_ring.stats.util[i]);
}

static void dp_ppeds_clear_rings_stats(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	memset(&be_soc->ppeds_wbm_release_ring.stats, 0,
	       sizeof(struct ring_util_stats));
	memset(&be_soc->ppe2tcl_ring.stats, 0, sizeof(struct ring_util_stats));
}
#endif

static void dp_soc_cfg_attach_be(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;
	dp_soc_cfg_attach(soc);

	wlan_cfg_set_rx_rel_ring_id(soc_cfg_ctx, WBM2SW_REL_ERR_RING_NUM);

	soc->wlan_cfg_ctx->tcl_wbm_map_array = g_tcl_wbm_map_array;

	/* this is used only when dmac mode is enabled */
	soc->num_rx_refill_buf_rings = 1;

	/*
	 * do not allocate TCL credit ring for BE as we already have
	 * 4 TCL_DATA rings
	 */
	soc->init_tcl_cmd_cred_ring = false;
	soc->wlan_cfg_ctx->notify_frame_support =
				DP_MARK_NOTIFY_FRAME_SUPPORT;
}

qdf_size_t dp_get_context_size_be(enum dp_context_type context_type)
{
	switch (context_type) {
	case DP_CONTEXT_TYPE_SOC:
		return sizeof(struct dp_soc_be);
	case DP_CONTEXT_TYPE_PDEV:
		return sizeof(struct dp_pdev_be);
	case DP_CONTEXT_TYPE_VDEV:
		return sizeof(struct dp_vdev_be);
	case DP_CONTEXT_TYPE_PEER:
		return sizeof(struct dp_peer_be);
	default:
		return 0;
	}
}

#if defined(DP_FEATURE_HW_COOKIE_CONVERSION) || defined(WLAN_SUPPORT_RX_FISA)
static uint64_t dp_get_cmem_chunk(struct dp_soc *soc, uint64_t size,
				  enum CMEM_MEM_CLIENTS client)
{
	uint64_t cmem_chunk;

	dp_info("cmem base 0x%llx, total size 0x%llx avail_size 0x%llx",
		soc->cmem_base, soc->cmem_total_size, soc->cmem_avail_size);

	/* Check if requested cmem space is available */
	if (soc->cmem_avail_size < size) {
		dp_err("cmem_size 0x%llx bytes < requested size 0x%llx bytes",
		       soc->cmem_avail_size, size);
		return 0;
	}

	cmem_chunk = soc->cmem_base +
		     (soc->cmem_total_size - soc->cmem_avail_size);
	soc->cmem_avail_size -= size;
	dp_info("Reserved cmem space 0x%llx, size 0x%llx for client %d",
		cmem_chunk, size, client);

	return cmem_chunk;
}
#endif

#ifdef WLAN_SUPPORT_RX_FISA
static uint64_t dp_get_fst_cmem_base_be(struct dp_soc *soc, uint64_t size)
{
	return dp_get_cmem_chunk(soc, size, FISA_FST);
}

static void dp_initialize_arch_ops_be_fisa(struct dp_arch_ops *arch_ops)
{
	arch_ops->dp_get_fst_cmem_base = dp_get_fst_cmem_base_be;
}
#else
static void dp_initialize_arch_ops_be_fisa(struct dp_arch_ops *arch_ops)
{
}
#endif

#ifdef DP_FEATURE_HW_COOKIE_CONVERSION
#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
/**
 * dp_cc_wbm_sw_en_cfg() - configure HW cookie conversion enablement
 *			   per wbm2sw ring
 *
 * @cc_cfg: HAL HW cookie conversion configuration structure pointer
 *
 * Return: None
 */
#ifdef IPA_OPT_WIFI_DP
static inline
void dp_cc_wbm_sw_en_cfg(struct hal_hw_cc_config *cc_cfg)
{
	cc_cfg->wbm2sw6_cc_en = 1;
	cc_cfg->wbm2sw5_cc_en = 0;
	cc_cfg->wbm2sw4_cc_en = 1;
	cc_cfg->wbm2sw3_cc_en = 1;
	cc_cfg->wbm2sw2_cc_en = 1;
	/* disable wbm2sw1 hw cc as it's for FW */
	cc_cfg->wbm2sw1_cc_en = 0;
	cc_cfg->wbm2sw0_cc_en = 1;
	cc_cfg->wbm2fw_cc_en = 0;
}
#else
static inline
void dp_cc_wbm_sw_en_cfg(struct hal_hw_cc_config *cc_cfg)
{
	cc_cfg->wbm2sw6_cc_en = 1;
	cc_cfg->wbm2sw5_cc_en = 1;
	cc_cfg->wbm2sw4_cc_en = 1;
	cc_cfg->wbm2sw3_cc_en = 1;
	cc_cfg->wbm2sw2_cc_en = 1;
	/* disable wbm2sw1 hw cc as it's for FW */
	cc_cfg->wbm2sw1_cc_en = 0;
	cc_cfg->wbm2sw0_cc_en = 1;
	cc_cfg->wbm2fw_cc_en = 0;
}
#endif
#else
static inline
void dp_cc_wbm_sw_en_cfg(struct hal_hw_cc_config *cc_cfg)
{
	cc_cfg->wbm2sw6_cc_en = 1;
	cc_cfg->wbm2sw5_cc_en = 1;
	cc_cfg->wbm2sw4_cc_en = 1;
	cc_cfg->wbm2sw3_cc_en = 1;
	cc_cfg->wbm2sw2_cc_en = 1;
	cc_cfg->wbm2sw1_cc_en = 1;
	cc_cfg->wbm2sw0_cc_en = 1;
	cc_cfg->wbm2fw_cc_en = 0;
}
#endif

/**
 * dp_cc_reg_cfg_init() - initialize and configure HW cookie
 *			  conversion register
 *
 * @soc: SOC handle
 * @is_4k_align: page address 4k aligned
 *
 * Return: None
 */
static void dp_cc_reg_cfg_init(struct dp_soc *soc,
			       bool is_4k_align)
{
	struct hal_hw_cc_config cc_cfg = { 0 };
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() == QDF_GLOBAL_FTM_MODE)
		return;

	if (!soc->wlan_cfg_ctx->hw_cc_enabled) {
		dp_info("INI skip HW CC register setting");
		return;
	}

	cc_cfg.lut_base_addr_31_0 = be_soc->cc_cmem_base;
	cc_cfg.cc_global_en = true;
	cc_cfg.page_4k_align = is_4k_align;
	cc_cfg.cookie_offset_msb = DP_CC_DESC_ID_SPT_VA_OS_MSB;
	cc_cfg.cookie_page_msb = DP_CC_DESC_ID_PPT_PAGE_OS_MSB;
	/* 36th bit should be 1 then HW know this is CMEM address */
	cc_cfg.lut_base_addr_39_32 = 0x10;

	cc_cfg.error_path_cookie_conv_en = true;
	cc_cfg.release_path_cookie_conv_en = true;
	dp_cc_wbm_sw_en_cfg(&cc_cfg);

	hal_cookie_conversion_reg_cfg_be(soc->hal_soc, &cc_cfg);
}

/**
 * dp_hw_cc_cmem_write() - DP wrapper function for CMEM buffer writing
 * @hal_soc_hdl: HAL SOC handle
 * @offset: CMEM address
 * @value: value to write
 *
 * Return: None.
 */
static inline void dp_hw_cc_cmem_write(hal_soc_handle_t hal_soc_hdl,
				       uint32_t offset,
				       uint32_t value)
{
	hal_cmem_write(hal_soc_hdl, offset, value);
}

/**
 * dp_hw_cc_cmem_addr_init() - Check and initialize CMEM base address for
 *			       HW cookie conversion
 *
 * @soc: SOC handle
 *
 * Return: 0 in case of success, else error value
 */
static inline QDF_STATUS dp_hw_cc_cmem_addr_init(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	be_soc->cc_cmem_base = dp_get_cmem_chunk(soc, DP_CC_PPT_MEM_SIZE,
					      COOKIE_CONVERSION);
	return QDF_STATUS_SUCCESS;
}

#else

static inline void dp_cc_reg_cfg_init(struct dp_soc *soc,
				      bool is_4k_align) {}

static inline void dp_hw_cc_cmem_write(hal_soc_handle_t hal_soc_hdl,
				       uint32_t offset,
				       uint32_t value)
{ }

static inline QDF_STATUS dp_hw_cc_cmem_addr_init(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(DP_FEATURE_HW_COOKIE_CONVERSION) || defined(WLAN_SUPPORT_RX_FISA)
static QDF_STATUS dp_get_cmem_allocation(struct dp_soc *soc,
					 uint8_t for_feature)
{
	QDF_STATUS status = QDF_STATUS_E_NOMEM;

	switch (for_feature) {
	case COOKIE_CONVERSION:
		status = dp_hw_cc_cmem_addr_init(soc);
		break;
	default:
		dp_err("Invalid CMEM request");
	}

	return status;
}
#else
static QDF_STATUS dp_get_cmem_allocation(struct dp_soc *soc,
					 uint8_t for_feature)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
dp_hw_cookie_conversion_attach(struct dp_soc_be *be_soc,
			       struct dp_hw_cookie_conversion_t *cc_ctx,
			       uint32_t num_descs,
			       enum qdf_dp_desc_type desc_type,
			       uint8_t desc_pool_id)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);
	uint32_t num_spt_pages, i = 0;
	struct dp_spt_page_desc *spt_desc;
	struct qdf_mem_dma_page_t *dma_page;
	uint8_t chip_id;

	/* estimate how many SPT DDR pages needed */
	num_spt_pages = qdf_do_div(
				num_descs + (DP_CC_SPT_PAGE_MAX_ENTRIES - 1),
				DP_CC_SPT_PAGE_MAX_ENTRIES);
	num_spt_pages = num_spt_pages <= DP_CC_PPT_MAX_ENTRIES ?
					num_spt_pages : DP_CC_PPT_MAX_ENTRIES;
	dp_info("num_spt_pages needed %d", num_spt_pages);

	dp_desc_multi_pages_mem_alloc(soc, QDF_DP_HW_CC_SPT_PAGE_TYPE,
				      &cc_ctx->page_pool, qdf_page_size,
				      num_spt_pages, 0, false);
	if (!cc_ctx->page_pool.dma_pages) {
		dp_err("spt ddr pages allocation failed");
		return QDF_STATUS_E_RESOURCES;
	}
	cc_ctx->page_desc_base = qdf_mem_malloc(
			num_spt_pages * sizeof(struct dp_spt_page_desc));
	if (!cc_ctx->page_desc_base) {
		dp_err("spt page descs allocation failed");
		goto fail_0;
	}

	chip_id = dp_mlo_get_chip_id(soc);
	cc_ctx->cmem_offset = dp_desc_pool_get_cmem_base(chip_id, desc_pool_id,
							 desc_type);

	/* initial page desc */
	spt_desc = cc_ctx->page_desc_base;
	dma_page = cc_ctx->page_pool.dma_pages;
	while (i < num_spt_pages) {
		/* check if page address 4K aligned */
		if (qdf_unlikely(dma_page[i].page_p_addr & 0xFFF)) {
			dp_err("non-4k aligned pages addr %pK",
			       (void *)dma_page[i].page_p_addr);
			goto fail_1;
		}

		spt_desc[i].page_v_addr =
					dma_page[i].page_v_addr_start;
		spt_desc[i].page_p_addr =
					dma_page[i].page_p_addr;
		i++;
	}

	cc_ctx->total_page_num = num_spt_pages;
	qdf_spinlock_create(&cc_ctx->cc_lock);

	return QDF_STATUS_SUCCESS;
fail_1:
	qdf_mem_free(cc_ctx->page_desc_base);
	cc_ctx->page_desc_base = NULL;
fail_0:
	dp_desc_multi_pages_mem_free(soc, QDF_DP_HW_CC_SPT_PAGE_TYPE,
				     &cc_ctx->page_pool, 0, false);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
dp_hw_cookie_conversion_detach(struct dp_soc_be *be_soc,
			       struct dp_hw_cookie_conversion_t *cc_ctx)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);

	dp_desc_multi_pages_mem_free(soc, QDF_DP_HW_CC_SPT_PAGE_TYPE,
				     &cc_ctx->page_pool, 0, false);
	if (cc_ctx->page_desc_base)
		qdf_spinlock_destroy(&cc_ctx->cc_lock);

	qdf_mem_free(cc_ctx->page_desc_base);
	cc_ctx->page_desc_base = NULL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_hw_cookie_conversion_init(struct dp_soc_be *be_soc,
			     struct dp_hw_cookie_conversion_t *cc_ctx)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);
	uint32_t i = 0;
	struct dp_spt_page_desc *spt_desc;
	uint32_t ppt_index;
	uint32_t ppt_id_start;

	if (!cc_ctx->total_page_num) {
		dp_err("total page num is 0");
		return QDF_STATUS_E_INVAL;
	}

	ppt_id_start = DP_CMEM_OFFSET_TO_PPT_ID(cc_ctx->cmem_offset);
	spt_desc = cc_ctx->page_desc_base;
	while (i < cc_ctx->total_page_num) {
		/* write page PA to CMEM */
		dp_hw_cc_cmem_write(soc->hal_soc,
				    (cc_ctx->cmem_offset + be_soc->cc_cmem_base
				     + (i * DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED)),
				    (spt_desc[i].page_p_addr >>
				     DP_CC_PPT_ENTRY_HW_APEND_BITS_4K_ALIGNED));

		ppt_index = ppt_id_start + i;

		if (ppt_index >= DP_CC_PPT_MAX_ENTRIES)
			qdf_assert_always(0);

		spt_desc[i].ppt_index = ppt_index;

		be_soc->page_desc_base[ppt_index].page_v_addr =
				spt_desc[i].page_v_addr;
		i++;
	}
	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
QDF_STATUS
dp_hw_cookie_conversion_deinit(struct dp_soc_be *be_soc,
			       struct dp_hw_cookie_conversion_t *cc_ctx)
{
	uint32_t ppt_index;
	struct dp_spt_page_desc *spt_desc;
	int i = 0;

	spt_desc = cc_ctx->page_desc_base;
	while (i < cc_ctx->total_page_num) {
		ppt_index = spt_desc[i].ppt_index;
		be_soc->page_desc_base[ppt_index].page_v_addr = NULL;
		i++;
	}
	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS
dp_hw_cookie_conversion_deinit(struct dp_soc_be *be_soc,
			       struct dp_hw_cookie_conversion_t *cc_ctx)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);
	uint32_t ppt_index;
	struct dp_spt_page_desc *spt_desc;
	int i = 0;

	spt_desc = cc_ctx->page_desc_base;
	while (i < cc_ctx->total_page_num) {
		/* reset PA in CMEM to NULL */
		dp_hw_cc_cmem_write(soc->hal_soc,
				    (cc_ctx->cmem_offset + be_soc->cc_cmem_base
				     + (i * DP_CC_PPT_ENTRY_SIZE_4K_ALIGNED)),
				    0);

		ppt_index = spt_desc[i].ppt_index;
		be_soc->page_desc_base[ppt_index].page_v_addr = NULL;
		i++;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_SUPPORT_PPEDS
static QDF_STATUS dp_soc_ppeds_attach_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int target_type = hal_get_target_type(soc->hal_soc);
	struct cdp_ops *cdp_ops = soc->cdp_soc.ops;

	/*
	 * Check if PPE DS is enabled and wlan soc supports it.
	 */
	if (!wlan_cfg_get_dp_soc_ppeds_enable(soc->wlan_cfg_ctx) ||
	    !dp_ppeds_target_supported(target_type))
		return QDF_STATUS_SUCCESS;

	if (dp_ppeds_attach_soc_be(be_soc) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_SUCCESS;

	cdp_ops->ppeds_ops = &dp_ops_ppeds_be;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_ppeds_detach_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct cdp_ops *cdp_ops = soc->cdp_soc.ops;

	if (!be_soc->ppeds_handle)
		return QDF_STATUS_E_FAILURE;

	dp_ppeds_detach_soc_be(be_soc);

	cdp_ops->ppeds_ops = NULL;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_peer_ppeds_default_route_be(struct dp_soc *soc,
						 struct dp_peer_be *be_peer,
						 uint8_t vdev_id,
						 uint16_t src_info)
{
	uint16_t service_code;
	uint8_t priority_valid;
	uint8_t use_ppe_ds = PEER_ROUTING_USE_PPE;
	uint8_t peer_routing_enabled = PEER_ROUTING_ENABLED;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	struct dp_vdev_be *be_vdev;

	be_vdev = dp_get_be_vdev_from_dp_vdev(be_peer->peer.vdev);

	/*
	 * Program service code bypass to avoid L2 new mac address
	 * learning exception when fdb learning is disabled.
	 */
	service_code = PPE_DRV_SC_SPF_BYPASS;
	priority_valid = be_peer->priority_valid;

	/*
	 * if FST is enabled then let flow rule take the decision of
	 * routing the pkt to DS or host
	 */
	if (wlan_cfg_is_rx_flow_tag_enabled(cfg))
		use_ppe_ds = 0;

	if (soc->cdp_soc.ol_ops->peer_set_ppeds_default_routing) {
		status =
		soc->cdp_soc.ol_ops->peer_set_ppeds_default_routing
				(soc->ctrl_psoc,
				be_peer->peer.mac_addr.raw,
				service_code, priority_valid,
				src_info, vdev_id, use_ppe_ds,
				peer_routing_enabled);
		if (status != QDF_STATUS_SUCCESS) {
			dp_err("vdev_id: %d, PPE peer routing mac:"
			       QDF_MAC_ADDR_FMT, vdev_id,
			       QDF_MAC_ADDR_REF(be_peer->peer.mac_addr.raw));

			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
QDF_STATUS dp_peer_setup_ppeds_be(struct dp_soc *soc,
				  struct dp_peer *peer,
				  struct dp_vdev_be *be_vdev,
				  void *args)
{
	struct dp_peer *mld_peer;
	struct dp_soc *mld_soc;
	struct dp_soc_be *be_soc;
	struct cdp_soc_t *cdp_soc;
	struct dp_peer_be *be_peer = dp_get_be_peer_from_dp_peer(peer);
	struct cdp_ds_vp_params vp_params = {0};
	struct dp_ppe_vp_profile *ppe_vp_profile = (struct dp_ppe_vp_profile *)args;
	uint16_t src_info = ppe_vp_profile->vp_num;
	uint8_t vdev_id = be_vdev->vdev.vdev_id;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!be_peer) {
		dp_err("BE peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (IS_DP_LEGACY_PEER(peer)) {
		qdf_status = dp_peer_ppeds_default_route_be(soc, be_peer,
							    vdev_id, src_info);
	} else if (IS_MLO_DP_MLD_PEER(peer)) {
		int i;
		struct dp_peer *link_peer = NULL;
		struct dp_mld_link_peers link_peers_info;

		/* get link peers with reference */
		dp_get_link_peers_ref_from_mld_peer(soc, peer, &link_peers_info,
						    DP_MOD_ID_DS);

		for (i = 0; i < link_peers_info.num_links; i++) {
			link_peer = link_peers_info.link_peers[i];
			be_peer = dp_get_be_peer_from_dp_peer(link_peer);
			if (!be_peer) {
				dp_err("BE peer is null");
				continue;
			}

			be_vdev = dp_get_be_vdev_from_dp_vdev(link_peer->vdev);
			if (!be_vdev) {
				dp_err("BE vap is null for peer id %d ",
				       link_peer->peer_id);
				continue;
			}

			vdev_id = be_vdev->vdev.vdev_id;
			soc = link_peer->vdev->pdev->soc;
			qdf_status = dp_peer_ppeds_default_route_be(soc,
								    be_peer,
								    vdev_id,
								    src_info);
		}

		dp_release_link_peers_ref(&link_peers_info, DP_MOD_ID_DS);
	} else {
		mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);

		if (!mld_peer)
			return qdf_status;

		/*
		 * In case of MLO link peer,
		 * Fetch the VP profile from the mld vdev.
		 */
		be_vdev = dp_get_be_vdev_from_dp_vdev(mld_peer->vdev);
		if (!be_vdev) {
			dp_err("BE vap is null");
			return QDF_STATUS_E_NULL_VALUE;
		}

		/*
		 * Extract the VP profile from the vap
		 * in case of MLO peer, we have to get the profile from
		 * the MLD vdev's osif handle and not the link peer.
		 */
		mld_soc = mld_peer->vdev->pdev->soc;
		cdp_soc = &mld_soc->cdp_soc;
		if (!cdp_soc->ol_ops->get_ppeds_profile_info_for_vap) {
			dp_err("%pK: Register PPEDS profile info API before use", cdp_soc);
			return QDF_STATUS_E_NULL_VALUE;
		}

		qdf_status = cdp_soc->ol_ops->get_ppeds_profile_info_for_vap(mld_soc->ctrl_psoc,
									     mld_peer->vdev->vdev_id,
									     &vp_params);
		if (qdf_status == QDF_STATUS_E_NULL_VALUE) {
			dp_err("%pK: Failed to get ppeds profile for mld soc", mld_soc);
			return qdf_status;
		}

		/*
		 * Check if PPE DS routing is enabled on
		 * the associated vap.
		 */
		if (vp_params.ppe_vp_type != PPE_VP_USER_TYPE_DS)
			return qdf_status;

		be_soc = dp_get_be_soc_from_dp_soc(mld_soc);
		ppe_vp_profile = &be_soc->ppe_vp_profile[vp_params.ppe_vp_profile_idx];
		src_info = ppe_vp_profile->vp_num;

		qdf_status = dp_peer_ppeds_default_route_be(soc, be_peer,
							    vdev_id, src_info);
	}

	return qdf_status;
}
#else
static QDF_STATUS dp_peer_setup_ppeds_be(struct dp_soc *soc,
					 struct dp_peer *peer,
					 struct dp_vdev_be *be_vdev
					 void *args)
{
	struct dp_ppe_vp_profile *vp_profile = (struct dp_ppe_vp_profile *)args;
	struct dp_peer_be *be_peer = dp_get_be_peer_from_dp_peer(peer);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!be_peer) {
		dp_err("BE peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	qdf_status = dp_peer_ppeds_default_route_be(soc, be_peer,
						    be_vdev->vdev.vdev_id,
						    vp_profile->vp_num);

	return qdf_status;
}
#endif
#else
static QDF_STATUS dp_ppeds_init_soc_be(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_ppeds_deinit_soc_be(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_soc_ppeds_attach_be(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_soc_ppeds_detach_be(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_peer_setup_ppeds_be(struct dp_soc *soc, struct dp_peer *peer,
				  struct dp_vdev_be *be_vdev,
				  void *args)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_ppeds_stop_soc_be(struct dp_soc *soc)
{
}
#endif /* WLAN_SUPPORT_PPEDS */

void dp_reo_shared_qaddr_detach(struct dp_soc *soc)
{
	qdf_mem_free_consistent(soc->osdev, soc->osdev->dev,
				REO_QUEUE_REF_ML_TABLE_SIZE,
				soc->reo_qref.mlo_reo_qref_table_vaddr,
				soc->reo_qref.mlo_reo_qref_table_paddr, 0);
	qdf_mem_free_consistent(soc->osdev, soc->osdev->dev,
				REO_QUEUE_REF_NON_ML_TABLE_SIZE,
				soc->reo_qref.non_mlo_reo_qref_table_vaddr,
				soc->reo_qref.non_mlo_reo_qref_table_paddr, 0);
}

#ifdef QCA_SUPPORT_DP_GLOBAL_CTX
static void dp_soc_tx_cookie_detach_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int i = 0;
	struct dp_global_context *dp_global;

	dp_global = wlan_objmgr_get_global_ctx();

	dp_global->tx_cookie_ctx_alloc_cnt--;
	if (dp_global->tx_cookie_ctx_alloc_cnt == 0) {
		for (i = 0; i < MAX_TXDESC_POOLS; i++) {
			dp_hw_cookie_conversion_detach(be_soc,
						       dp_global->tx_cc_ctx[i]);
			qdf_mem_free(dp_global->tx_cc_ctx[i]);
		}
	}

	dp_global->spcl_tx_cookie_ctx_alloc_cnt--;
	if (dp_global->spcl_tx_cookie_ctx_alloc_cnt == 0) {
		for (i = 0; i < MAX_TXDESC_POOLS; i++) {
			dp_hw_cookie_conversion_detach(
					be_soc,
					dp_global->spcl_tx_cc_ctx[i]);
			qdf_mem_free(dp_global->spcl_tx_cc_ctx[i]);
		}
	}
}

static QDF_STATUS dp_soc_tx_cookie_attach_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_hw_cookie_conversion_t *cc_ctx;
	struct dp_global_context *dp_global;
	struct dp_hw_cookie_conversion_t *spcl_cc_ctx;
	uint32_t num_entries;
	int i = 0;
	QDF_STATUS qdf_status;

	dp_global = wlan_objmgr_get_global_ctx();
	if (dp_global->tx_cookie_ctx_alloc_cnt == 0) {
		for (i = 0; i < MAX_TXDESC_POOLS; i++) {
			dp_global->tx_cc_ctx[i] =
				qdf_mem_malloc(
				sizeof(struct dp_hw_cookie_conversion_t));
			cc_ctx = dp_global->tx_cc_ctx[i];
			num_entries =
				wlan_cfg_get_num_tx_desc(soc->wlan_cfg_ctx);
			qdf_status =
				dp_hw_cookie_conversion_attach(
						be_soc,
						cc_ctx,
						num_entries,
						QDF_DP_TX_DESC_TYPE, i);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				return QDF_STATUS_E_FAILURE;
		}
	}
	dp_global->tx_cookie_ctx_alloc_cnt++;

	if (dp_global->spcl_tx_cookie_ctx_alloc_cnt == 0) {
		for (i = 0; i < MAX_TXDESC_POOLS; i++) {
			dp_global->spcl_tx_cc_ctx[i] =
				qdf_mem_malloc(
				sizeof(struct dp_hw_cookie_conversion_t));
			spcl_cc_ctx = dp_global->spcl_tx_cc_ctx[i];
			num_entries =
				wlan_cfg_get_num_tx_spl_desc(soc->wlan_cfg_ctx);
			qdf_status =
				dp_hw_cookie_conversion_attach(
						be_soc,
						spcl_cc_ctx,
						num_entries,
						QDF_DP_TX_SPCL_DESC_TYPE, i);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				return QDF_STATUS_E_FAILURE;
		}
	}
	dp_global->spcl_tx_cookie_ctx_alloc_cnt++;
	return QDF_STATUS_SUCCESS;
}

static void dp_soc_tx_cookie_deinit_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_global_context *dp_global;
	int i = 0;

	dp_global = wlan_objmgr_get_global_ctx();

	for (i = 0; i < MAX_TXDESC_POOLS; i++)
		dp_hw_cookie_conversion_deinit(
				be_soc,
				dp_global->tx_cc_ctx[i]);
	for (i = 0; i < MAX_TXDESC_POOLS; i++)
		dp_hw_cookie_conversion_deinit(be_soc,
					       dp_global->spcl_tx_cc_ctx[i]);
}

static QDF_STATUS dp_soc_tx_cookie_init_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_global_context *dp_global;
	struct dp_hw_cookie_conversion_t *cc_ctx;
	struct dp_hw_cookie_conversion_t *spcl_cc_ctx;
	QDF_STATUS qdf_status;
	int i = 0;

	dp_global = wlan_objmgr_get_global_ctx();
	for (i = 0; i < MAX_TXDESC_POOLS; i++) {
		cc_ctx = dp_global->tx_cc_ctx[i];
		qdf_status =
			dp_hw_cookie_conversion_init(be_soc,
						     cc_ctx);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			return QDF_STATUS_E_FAILURE;
	}
	for (i = 0; i < MAX_TXDESC_POOLS; i++) {
		spcl_cc_ctx = dp_global->spcl_tx_cc_ctx[i];
		qdf_status =
			dp_hw_cookie_conversion_init(be_soc,
						     spcl_cc_ctx);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
#else
static void dp_soc_tx_cookie_detach_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int i = 0;

	for (i = 0; i < MAX_TXDESC_POOLS; i++) {
		dp_hw_cookie_conversion_detach(
				be_soc,
				&be_soc->tx_cc_ctx[i]);
	}
}

static QDF_STATUS dp_soc_tx_cookie_attach_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	uint32_t num_entries;
	int i = 0;
	QDF_STATUS qdf_status;

	for (i = 0; i < MAX_TXDESC_POOLS; i++) {
		num_entries = wlan_cfg_get_num_tx_desc(soc->wlan_cfg_ctx);
		qdf_status =
			dp_hw_cookie_conversion_attach(
					be_soc,
					&be_soc->tx_cc_ctx[i],
					num_entries,
					QDF_DP_TX_DESC_TYPE, i);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

static void dp_soc_tx_cookie_deinit_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int i = 0;

	for (i = 0; i < MAX_TXDESC_POOLS; i++)
		dp_hw_cookie_conversion_deinit(
				be_soc,
				&be_soc->tx_cc_ctx[i]);
}

static QDF_STATUS dp_soc_tx_cookie_init_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int i = 0;
	QDF_STATUS qdf_status;

	for (i = 0; i < MAX_TXDESC_POOLS; i++) {
		qdf_status =
			dp_hw_cookie_conversion_init(
					be_soc,
					&be_soc->tx_cc_ctx[i]);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS dp_soc_detach_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	dp_mlo_dev_obj_t mlo_dev_obj;
	int i = 0;

	dp_soc_ppeds_detach_be(soc);
	dp_reo_shared_qaddr_detach(soc);

	mlo_dev_obj = dp_get_mlo_dev_list_obj(be_soc);
	dp_mlo_dev_ctxt_list_detach_wrapper(mlo_dev_obj);
	dp_soc_tx_cookie_detach_be(soc);

	for (i = 0; i < MAX_RXDESC_POOLS; i++)
		dp_hw_cookie_conversion_detach(be_soc,
					       &be_soc->rx_cc_ctx[i]);

	qdf_mem_free(be_soc->page_desc_base);
	be_soc->page_desc_base = NULL;

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_DP_GLOBAL_CTX
static void dp_set_rx_fst_be(struct dp_rx_fst *fst)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	if (dp_global)
		dp_global->fst_ctx = fst;
}

static struct dp_rx_fst *dp_get_rx_fst_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	if (dp_global)
		return dp_global->fst_ctx;

	return NULL;
}

static uint32_t dp_rx_fst_release_ref_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();
	uint32_t rx_fst_ref_cnt;

	if (dp_global) {
		rx_fst_ref_cnt = qdf_atomic_read(&dp_global->rx_fst_ref_cnt);
		qdf_atomic_dec(&dp_global->rx_fst_ref_cnt);
		return rx_fst_ref_cnt;
	}

	return 1;
}

static void dp_rx_fst_get_ref_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	if (dp_global)
		qdf_atomic_inc(&dp_global->rx_fst_ref_cnt);
}

#else
static void dp_set_rx_fst_be(struct dp_rx_fst *fst)
{
}

static struct dp_rx_fst *dp_get_rx_fst_be(void)
{
	return NULL;
}

static uint32_t dp_rx_fst_release_ref_be(void)
{
	return 1;
}

static void dp_rx_fst_get_ref_be(void)
{
}
#endif

#ifdef WLAN_MLO_MULTI_CHIP
#ifdef WLAN_MCAST_MLO
static inline void
dp_mlo_mcast_init(struct dp_soc *soc, struct dp_vdev *vdev)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	be_vdev->mcast_primary = false;

	hal_tx_mcast_mlo_reinject_routing_set(
				soc->hal_soc,
				HAL_TX_MCAST_MLO_REINJECT_TQM_NOTIFY);

	if (vdev->opmode == wlan_op_mode_ap) {
		hal_tx_vdev_mcast_ctrl_set(vdev->pdev->soc->hal_soc,
					   vdev->vdev_id,
					   HAL_TX_MCAST_CTRL_FW_EXCEPTION);
	}
}

static inline void
dp_mlo_mcast_deinit(struct dp_soc *soc, struct dp_vdev *vdev)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	be_vdev->mcast_primary = false;
	vdev->mlo_vdev = 0;
}

#else
static inline void
dp_mlo_mcast_init(struct dp_soc *soc, struct dp_vdev *vdev)
{
}

static inline void
dp_mlo_mcast_deinit(struct dp_soc *soc, struct dp_vdev *vdev)
{
}
#endif

static void dp_get_rx_hash_key_be(struct dp_soc *soc,
				  struct cdp_lro_hash_config *lro_hash)
{
	dp_mlo_get_rx_hash_key(soc, lro_hash);
}

#ifdef WLAN_DP_MLO_DEV_CTX
static inline void
dp_attach_vdev_list_in_mlo_dev_ctxt(struct dp_soc_be *be_soc,
				    struct dp_vdev *vdev,
				    struct dp_mlo_dev_ctxt *mlo_dev_ctxt)
{
	uint8_t pdev_id = vdev->pdev->pdev_id;

	qdf_spin_lock_bh(&mlo_dev_ctxt->vdev_list_lock);
	if (vdev->is_bridge_vdev) {
		if (mlo_dev_ctxt->bridge_vdev[be_soc->mlo_chip_id][pdev_id]
		    != CDP_INVALID_VDEV_ID)
			dp_alert("bridge vdevId in MLO dev ctx is not Invalid"
				 "chip_id: %u, pdev_id: %u,"
				 "existing vdev_id: %u, new vdev_id : %u",
				 be_soc->mlo_chip_id, pdev_id,
				 mlo_dev_ctxt->bridge_vdev[be_soc->mlo_chip_id][pdev_id],
				 vdev->vdev_id);

		mlo_dev_ctxt->bridge_vdev[be_soc->mlo_chip_id][pdev_id] =
								vdev->vdev_id;
		mlo_dev_ctxt->is_bridge_vdev_present = 1;
	} else {
		if (mlo_dev_ctxt->vdev_list[be_soc->mlo_chip_id][pdev_id]
		    != CDP_INVALID_VDEV_ID)
			dp_alert("vdevId in MLO dev ctx is not Invalid"
				 "chip_id: %u, pdev_id: %u,"
				 "existing vdev_id: %u, new vdev_id : %u",
				 be_soc->mlo_chip_id, pdev_id,
				 mlo_dev_ctxt->vdev_list[be_soc->mlo_chip_id][pdev_id],
				 vdev->vdev_id);

		mlo_dev_ctxt->vdev_list[be_soc->mlo_chip_id][pdev_id] =
								vdev->vdev_id;
	}
	mlo_dev_ctxt->vdev_count++;
	qdf_spin_unlock_bh(&mlo_dev_ctxt->vdev_list_lock);
}

static inline QDF_STATUS
dp_detach_vdev_list_in_mlo_dev_ctxt(struct dp_soc_be *be_soc,
				    struct dp_vdev *vdev,
				    struct dp_mlo_dev_ctxt *mlo_dev_ctxt)
{
	uint8_t pdev_id = vdev->pdev->pdev_id;

	if (mlo_dev_ctxt->vdev_list[be_soc->mlo_chip_id][pdev_id] ==
	    CDP_INVALID_VDEV_ID) {
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&mlo_dev_ctxt->vdev_list_lock);
	if (vdev->is_bridge_vdev) {
		mlo_dev_ctxt->bridge_vdev[be_soc->mlo_chip_id][pdev_id] =
							CDP_INVALID_VDEV_ID;
	} else {
		mlo_dev_ctxt->vdev_list[be_soc->mlo_chip_id][pdev_id] =
							CDP_INVALID_VDEV_ID;
	}
	mlo_dev_ctxt->vdev_count--;
	qdf_spin_unlock_bh(&mlo_dev_ctxt->vdev_list_lock);

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_DP_MLO_DEV_CTX */
#else
static inline void
dp_mlo_mcast_init(struct dp_soc *soc, struct dp_vdev *vdev)
{
}

static inline void
dp_mlo_mcast_deinit(struct dp_soc *soc, struct dp_vdev *vdev)
{
}

static void dp_get_rx_hash_key_be(struct dp_soc *soc,
				  struct cdp_lro_hash_config *lro_hash)
{
	dp_get_rx_hash_key_bytes(lro_hash);
}

#ifdef WLAN_DP_MLO_DEV_CTX
static inline void
dp_attach_vdev_list_in_mlo_dev_ctxt(struct dp_soc_be *be_soc,
				    struct dp_vdev *vdev,
				    struct dp_mlo_dev_ctxt *mlo_dev_ctxt)
{
}

static inline QDF_STATUS
dp_detach_vdev_list_in_mlo_dev_ctxt(struct dp_soc_be *be_soc,
				    struct dp_vdev *vdev,
				    struct dp_mlo_dev_ctxt *mlo_dev_ctxt)
{
}
#endif /* WLAN_DP_MLO_DEV_CTX */
#endif

static QDF_STATUS dp_soc_attach_be(struct dp_soc *soc,
				   struct cdp_soc_attach_params *params)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	uint32_t max_tx_rx_desc_num, num_spt_pages;
	uint32_t num_entries;
	int i = 0;
	dp_mlo_dev_obj_t mlo_dev_obj;

	mlo_dev_obj = dp_get_mlo_dev_list_obj(be_soc);
	max_tx_rx_desc_num = WLAN_CFG_NUM_TX_DESC_MAX * MAX_TXDESC_POOLS +
		WLAN_CFG_RX_SW_DESC_NUM_SIZE_MAX * MAX_RXDESC_POOLS +
		WLAN_CFG_NUM_PPEDS_TX_DESC_MAX * MAX_PPE_TXDESC_POOLS;
	/* estimate how many SPT DDR pages needed */
	num_spt_pages = max_tx_rx_desc_num / DP_CC_SPT_PAGE_MAX_ENTRIES;
	num_spt_pages = num_spt_pages <= DP_CC_PPT_MAX_ENTRIES ?
					num_spt_pages : DP_CC_PPT_MAX_ENTRIES;

	be_soc->page_desc_base = qdf_mem_malloc(
		DP_CC_PPT_MAX_ENTRIES * sizeof(struct dp_spt_page_desc));
	if (!be_soc->page_desc_base) {
		dp_err("spt page descs allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	soc->wbm_sw0_bm_id = hal_tx_get_wbm_sw0_bm_id();

	qdf_status = dp_get_cmem_allocation(soc, COOKIE_CONVERSION);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		goto fail;

	dp_soc_mlo_fill_params(soc, params);

	/* Initialize common cdp mlo ops */
	dp_soc_initialize_cdp_cmn_mlo_ops(soc);

	/* Initialize MLO device ctxt list */
	dp_mlo_dev_ctxt_list_attach_wrapper(mlo_dev_obj);

	qdf_status = dp_soc_ppeds_attach_be(soc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		goto fail;

	qdf_status = dp_soc_tx_cookie_attach_be(soc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		goto fail;

	for (i = 0; i < MAX_RXDESC_POOLS; i++) {
		num_entries =
			wlan_cfg_get_dp_soc_rx_sw_desc_num(soc->wlan_cfg_ctx);
		qdf_status =
			dp_hw_cookie_conversion_attach(be_soc,
						       &be_soc->rx_cc_ctx[i],
						       num_entries,
						       QDF_DP_RX_DESC_BUF_TYPE,
						       i);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			goto fail;
	}

	return qdf_status;
fail:
	dp_soc_detach_be(soc);
	return qdf_status;
}

static QDF_STATUS dp_soc_deinit_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int i = 0;

	qdf_atomic_set(&soc->cmn_init_done, 0);

	dp_ppeds_stop_soc_be(soc);

	dp_tx_deinit_bank_profiles(be_soc);
	dp_soc_tx_cookie_deinit_be(soc);

	for (i = 0; i < MAX_RXDESC_POOLS; i++)
		dp_hw_cookie_conversion_deinit(be_soc,
					       &be_soc->rx_cc_ctx[i]);

	dp_ppeds_deinit_soc_be(soc);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_deinit_be_wrapper(struct dp_soc *soc)
{
	QDF_STATUS qdf_status;

	qdf_status = dp_soc_deinit_be(soc);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return qdf_status;

	dp_soc_deinit(soc);

	return QDF_STATUS_SUCCESS;
}

static void *dp_soc_init_be(struct dp_soc *soc, HTC_HANDLE htc_handle,
			    struct hif_opaque_softc *hif_handle)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int i = 0;
	void *ret_addr;

	wlan_minidump_log(soc, sizeof(*soc), soc->ctrl_psoc,
			  WLAN_MD_DP_SOC, "dp_soc");

	soc->hif_handle = hif_handle;

	soc->hal_soc = hif_get_hal_handle(soc->hif_handle);
	if (!soc->hal_soc)
		return NULL;

	dp_ppeds_init_soc_be(soc);

	qdf_status = dp_soc_tx_cookie_init_be(soc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		goto fail;

	for (i = 0; i < MAX_RXDESC_POOLS; i++) {
		qdf_status =
			dp_hw_cookie_conversion_init(be_soc,
						     &be_soc->rx_cc_ctx[i]);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			goto fail;
	}

	/* route vdev_id mismatch notification via FW completion */
	hal_tx_vdev_mismatch_routing_set(soc->hal_soc,
					 HAL_TX_VDEV_MISMATCH_FW_NOTIFY);

	qdf_status = dp_tx_init_bank_profiles(be_soc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		goto fail;

	/* write WBM/REO cookie conversion CFG register */
	dp_cc_reg_cfg_init(soc, true);

	ret_addr = dp_soc_init(soc, htc_handle, hif_handle);
	if (!ret_addr)
		goto fail;

	return ret_addr;
fail:
	dp_soc_deinit_be(soc);
	return NULL;
}

static QDF_STATUS dp_pdev_attach_be(struct dp_pdev *pdev,
				    struct cdp_pdev_attach_params *params)
{
	dp_pdev_mlo_fill_params(pdev, params);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_pdev_detach_be(struct dp_pdev *pdev)
{
	dp_mlo_update_link_to_pdev_unmap(pdev->soc, pdev);

	return QDF_STATUS_SUCCESS;
}

#ifdef INTRA_BSS_FWD_OFFLOAD
static
void dp_vdev_set_intra_bss(struct dp_soc *soc, uint16_t vdev_id, bool enable)
{
	soc->cdp_soc.ol_ops->vdev_set_intra_bss(soc->ctrl_psoc, vdev_id,
						enable);
}
#else
static
void dp_vdev_set_intra_bss(struct dp_soc *soc, uint16_t vdev_id, bool enable)
{
}
#endif

static QDF_STATUS dp_vdev_attach_be(struct dp_soc *soc, struct dp_vdev *vdev)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	struct dp_pdev *pdev = vdev->pdev;

	if (vdev->opmode == wlan_op_mode_monitor)
		return QDF_STATUS_SUCCESS;

	be_vdev->vdev_id_check_en = DP_TX_VDEV_ID_CHECK_ENABLE;

	be_vdev->bank_id = dp_tx_get_bank_profile(be_soc, be_vdev);
	vdev->bank_id = be_vdev->bank_id;

	if (be_vdev->bank_id == DP_BE_INVALID_BANK_ID) {
		QDF_BUG(0);
		return QDF_STATUS_E_FAULT;
	}

	if (vdev->opmode == wlan_op_mode_sta) {
		if (soc->cdp_soc.ol_ops->set_mec_timer)
			soc->cdp_soc.ol_ops->set_mec_timer(
					soc->ctrl_psoc,
					vdev->vdev_id,
					DP_AST_AGING_TIMER_DEFAULT_MS);

		if (pdev->isolation)
			hal_tx_vdev_mcast_ctrl_set(soc->hal_soc, vdev->vdev_id,
						   HAL_TX_MCAST_CTRL_FW_EXCEPTION);
		else
			hal_tx_vdev_mcast_ctrl_set(soc->hal_soc, vdev->vdev_id,
						   HAL_TX_MCAST_CTRL_MEC_NOTIFY);
	} else if (vdev->ap_bridge_enabled) {
		dp_vdev_set_intra_bss(soc, vdev->vdev_id, true);
	}

	dp_mlo_mcast_init(soc, vdev);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_vdev_detach_be(struct dp_soc *soc, struct dp_vdev *vdev)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	if (vdev->opmode == wlan_op_mode_monitor)
		return QDF_STATUS_SUCCESS;

	if (vdev->opmode == wlan_op_mode_ap)
		dp_mlo_mcast_deinit(soc, vdev);

	dp_tx_put_bank_profile(be_soc, be_vdev);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_PPEDS
static void dp_soc_txrx_peer_setup_be(struct dp_soc *soc, uint8_t vdev_id,
				      uint8_t *peer_mac)
{
	struct dp_vdev_be *be_vdev;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct dp_soc_be *be_soc;
	struct cdp_ds_vp_params vp_params = {0};
	struct cdp_soc_t *cdp_soc;
	enum wlan_op_mode vdev_opmode;
	struct dp_peer *peer;
	struct dp_peer *tgt_peer = NULL;
	struct dp_soc *tgt_soc = NULL;

	peer = dp_peer_find_hash_find(soc, peer_mac, 0, vdev_id, DP_MOD_ID_CDP);
	if (!peer)
		return;
	vdev_opmode = peer->vdev->opmode;

	if (vdev_opmode != wlan_op_mode_ap &&
	    vdev_opmode != wlan_op_mode_sta) {
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
		return;
	}

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	tgt_soc = tgt_peer->vdev->pdev->soc;
	be_soc = dp_get_be_soc_from_dp_soc(tgt_soc);
	cdp_soc = &tgt_soc->cdp_soc;

	be_vdev = dp_get_be_vdev_from_dp_vdev(tgt_peer->vdev);
	if (!be_vdev) {
		qdf_err("BE vap is null");
		qdf_status = QDF_STATUS_E_NULL_VALUE;
		goto fail;
	}

	/*
	 * Extract the VP profile from the VAP
	 */
	if (!cdp_soc->ol_ops->get_ppeds_profile_info_for_vap) {
		dp_err("%pK: Register get ppeds profile info first", cdp_soc);
		qdf_status = QDF_STATUS_E_NULL_VALUE;
		goto fail;
	}

	/*
	 * Check if PPE DS routing is enabled on the associated vap.
	 */
	qdf_status =
	cdp_soc->ol_ops->get_ppeds_profile_info_for_vap(tgt_soc->ctrl_psoc,
							tgt_peer->vdev->vdev_id,
							&vp_params);
	if (qdf_status == QDF_STATUS_E_NULL_VALUE) {
		dp_err("%pK: Could not find ppeds profile info vdev", be_vdev);
		qdf_status = QDF_STATUS_E_NULL_VALUE;
		goto fail;
	}

	if (vp_params.ppe_vp_type == PPE_VP_USER_TYPE_DS) {
		qdf_status = dp_peer_setup_ppeds_be(tgt_soc, tgt_peer, be_vdev,
						    (void *)&be_soc->ppe_vp_profile[vp_params.ppe_vp_profile_idx]);
	}

fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		dp_err("Unable to do ppeds peer setup");
		qdf_assert_always(0);
	}
}

static inline
void dp_tx_update_vp_profile(struct dp_soc_be *soc,
			     struct dp_vdev_be *vdev)
{
	dp_tx_ppeds_vp_profile_update(soc, vdev);
}
#else
static inline
void dp_soc_txrx_peer_setup_be(struct dp_soc *soc, uint8_t vdev_id,
			       uint8_t *peer_mac)
{
}

static inline
void dp_tx_update_vp_profile(struct dp_soc_be *soc,
			     struct dp_vdev_be *vdev)
{
}
#endif

static QDF_STATUS dp_peer_setup_be(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				   uint8_t *peer_mac,
				   struct cdp_peer_setup_info *setup_info)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	qdf_status = dp_peer_setup_wifi3(soc_hdl, vdev_id, peer_mac,
					 setup_info);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		dp_err("Unable to dp peer setup");
		return qdf_status;
	}

	dp_soc_txrx_peer_setup_be(soc, vdev_id, peer_mac);

	return QDF_STATUS_SUCCESS;
}

qdf_size_t dp_get_soc_context_size_be(void)
{
	return sizeof(struct dp_soc_be);
}

#ifdef CONFIG_WORD_BASED_TLV
/**
 * dp_rxdma_ring_wmask_cfg_be() - Setup RXDMA ring word mask config
 * @soc: Common DP soc handle
 * @htt_tlv_filter: Rx SRNG TLV and filter setting
 *
 * Return: none
 */
static inline void
dp_rxdma_ring_wmask_cfg_be(struct dp_soc *soc,
			   struct htt_rx_ring_tlv_filter *htt_tlv_filter)
{
	htt_tlv_filter->rx_msdu_end_wmask =
				 hal_rx_msdu_end_wmask_get(soc->hal_soc);
	htt_tlv_filter->rx_mpdu_start_wmask =
				 hal_rx_mpdu_start_wmask_get(soc->hal_soc);
}
#else
static inline void
dp_rxdma_ring_wmask_cfg_be(struct dp_soc *soc,
			   struct htt_rx_ring_tlv_filter *htt_tlv_filter)
{
}
#endif
#ifdef WLAN_SUPPORT_PPEDS
static
void dp_free_ppeds_interrupts(struct dp_soc *soc, struct dp_srng *srng,
			      int ring_type, int ring_num)
{
	if (srng->irq >= 0) {
		qdf_dev_clear_irq_status_flags(srng->irq, IRQ_DISABLE_UNLAZY);
		if (ring_type == WBM2SW_RELEASE &&
		    ring_num == WBM2_SW_PPE_REL_RING_ID)
			pld_pfrm_free_irq(soc->osdev->dev, srng->irq, soc);
		else if (ring_type == REO2PPE || ring_type == PPE2TCL)
			pld_pfrm_free_irq(soc->osdev->dev, srng->irq,
					  dp_get_ppe_ds_ctxt(soc));
	}
}

static
int dp_register_ppeds_interrupts(struct dp_soc *soc, struct dp_srng *srng,
				 int vector, int ring_type, int ring_num)
{
	int irq = -1, ret = 0;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int pci_slot = pld_get_pci_slot(soc->osdev->dev);

	srng->irq = -1;
	irq = pld_get_msi_irq(soc->osdev->dev, vector);
	qdf_dev_set_irq_status_flags(irq, IRQ_DISABLE_UNLAZY);

	if (ring_type == WBM2SW_RELEASE &&
	    ring_num == WBM2_SW_PPE_REL_RING_ID) {
		snprintf(be_soc->irq_name[2], DP_PPE_INTR_STRNG_LEN,
			 "pci%d_ppe_wbm_rel", pci_slot);

		ret = pld_pfrm_request_irq(soc->osdev->dev, irq,
					   dp_ppeds_handle_tx_comp,
					   IRQF_SHARED | IRQF_NO_SUSPEND,
					   be_soc->irq_name[2], (void *)soc);

		if (ret)
			goto fail;
	} else if (ring_type == REO2PPE && be_soc->ppeds_int_mode_enabled) {
		snprintf(be_soc->irq_name[0], DP_PPE_INTR_STRNG_LEN,
			 "pci%d_reo2ppe", pci_slot);
		ret = pld_pfrm_request_irq(soc->osdev->dev, irq,
					   dp_ppe_ds_reo2ppe_irq_handler,
					   IRQF_SHARED | IRQF_NO_SUSPEND,
					   be_soc->irq_name[0],
					   dp_get_ppe_ds_ctxt(soc));

		if (ret)
			goto fail;
	} else if (ring_type == PPE2TCL && be_soc->ppeds_int_mode_enabled) {
		snprintf(be_soc->irq_name[1], DP_PPE_INTR_STRNG_LEN,
			 "pci%d_ppe2tcl", pci_slot);
		ret = pld_pfrm_request_irq(soc->osdev->dev, irq,
					   dp_ppe_ds_ppe2tcl_irq_handler,
					   IRQF_NO_SUSPEND,
					   be_soc->irq_name[1],
					   dp_get_ppe_ds_ctxt(soc));
		if (ret)
			goto fail;

		pld_pfrm_disable_irq_nosync(soc->osdev->dev, irq);
	} else {
		return 0;
	}

	srng->irq = irq;

	dp_info("Registered irq %d for soc %pK ring type %d",
		irq, soc, ring_type);

	return 0;
fail:
	dp_err("Unable to config irq : ring type %d irq %d vector %d",
	       ring_type, irq, vector);
	qdf_dev_clear_irq_status_flags(irq, IRQ_DISABLE_UNLAZY);

	return ret;
}

void dp_ppeds_disable_irq(struct dp_soc *soc, struct dp_srng *srng)
{
	if (srng->irq >= 0)
		pld_pfrm_disable_irq_nosync(soc->osdev->dev, srng->irq);
}

void dp_ppeds_enable_irq(struct dp_soc *soc, struct dp_srng *srng)
{
	if (srng->irq >= 0)
		pld_pfrm_enable_irq(soc->osdev->dev, srng->irq);
}
#endif

#ifdef NO_RX_PKT_HDR_TLV
/**
 * dp_rxdma_ring_sel_cfg_be() - Setup RXDMA ring config
 * @soc: Common DP soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_rxdma_ring_sel_cfg_be(struct dp_soc *soc)
{
	int i;
	int mac_id;
	struct htt_rx_ring_tlv_filter htt_tlv_filter = {0};
	struct dp_srng *rx_mac_srng;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	/*
	 * In Beryllium chipset msdu_start, mpdu_end
	 * and rx_attn are part of msdu_end/mpdu_start
	 */
	htt_tlv_filter.msdu_start = 0;
	htt_tlv_filter.mpdu_end = 0;
	htt_tlv_filter.attention = 0;
	htt_tlv_filter.mpdu_start = 1;
	htt_tlv_filter.msdu_end = 1;
	htt_tlv_filter.packet = 1;
	htt_tlv_filter.packet_header = 0;

	htt_tlv_filter.ppdu_start = 0;
	htt_tlv_filter.ppdu_end = 0;
	htt_tlv_filter.ppdu_end_user_stats = 0;
	htt_tlv_filter.ppdu_end_user_stats_ext = 0;
	htt_tlv_filter.ppdu_end_status_done = 0;
	htt_tlv_filter.enable_fp = 1;
	htt_tlv_filter.enable_md = 0;
	htt_tlv_filter.enable_md = 0;
	htt_tlv_filter.enable_mo = 0;

	htt_tlv_filter.fp_mgmt_filter = 0;
	htt_tlv_filter.fp_ctrl_filter = FILTER_CTRL_BA_REQ;
	htt_tlv_filter.fp_data_filter = (FILTER_DATA_UCAST |
					 FILTER_DATA_DATA);
	htt_tlv_filter.fp_data_filter |=
		hal_rx_en_mcast_fp_data_filter(soc->hal_soc) ?
					FILTER_DATA_MCAST : 0;
	htt_tlv_filter.mo_mgmt_filter = 0;
	htt_tlv_filter.mo_ctrl_filter = 0;
	htt_tlv_filter.mo_data_filter = 0;
	htt_tlv_filter.md_data_filter = 0;

	htt_tlv_filter.offset_valid = true;

	/* Not subscribing to mpdu_end, msdu_start and rx_attn */
	htt_tlv_filter.rx_mpdu_end_offset = 0;
	htt_tlv_filter.rx_msdu_start_offset = 0;
	htt_tlv_filter.rx_attn_offset = 0;

	/*
	 * For monitor mode, the packet hdr tlv is enabled later during
	 * filter update
	 */
	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() == QDF_GLOBAL_MONITOR_MODE)
		htt_tlv_filter.rx_packet_offset = soc->rx_mon_pkt_tlv_size;
	else
		htt_tlv_filter.rx_packet_offset = soc->rx_pkt_tlv_size;

	/*Not subscribing rx_pkt_header*/
	htt_tlv_filter.rx_header_offset = 0;
	htt_tlv_filter.rx_mpdu_start_offset =
				hal_rx_mpdu_start_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_msdu_end_offset =
				hal_rx_msdu_end_offset_get(soc->hal_soc);

	dp_rxdma_ring_wmask_cfg_be(soc, &htt_tlv_filter);

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_pdev *pdev = soc->pdev_list[i];

		if (!pdev)
			continue;

		for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++) {
			int mac_for_pdev =
				dp_get_mac_id_for_pdev(mac_id, pdev->pdev_id);
			/*
			 * Obtain lmac id from pdev to access the LMAC ring
			 * in soc context
			 */
			int lmac_id =
				dp_get_lmac_id_for_pdev_id(soc, mac_id,
							   pdev->pdev_id);

			rx_mac_srng = dp_get_rxdma_ring(pdev, lmac_id);

			if (!rx_mac_srng->hal_srng)
				continue;

			htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					    rx_mac_srng->hal_srng,
					    RXDMA_BUF, buf_size,
					    &htt_tlv_filter);
		}
	}
	return status;
}
#else
/**
 * dp_rxdma_ring_sel_cfg_be() - Setup RXDMA ring config
 * @soc: Common DP soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_rxdma_ring_sel_cfg_be(struct dp_soc *soc)
{
	int i;
	int mac_id;
	struct htt_rx_ring_tlv_filter htt_tlv_filter = {0};
	struct dp_srng *rx_mac_srng;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	/*
	 * In Beryllium chipset msdu_start, mpdu_end
	 * and rx_attn are part of msdu_end/mpdu_start
	 */
	htt_tlv_filter.msdu_start = 0;
	htt_tlv_filter.mpdu_end = 0;
	htt_tlv_filter.attention = 0;
	htt_tlv_filter.mpdu_start = 1;
	htt_tlv_filter.msdu_end = 1;
	htt_tlv_filter.packet = 1;
	htt_tlv_filter.packet_header = 1;

	htt_tlv_filter.ppdu_start = 0;
	htt_tlv_filter.ppdu_end = 0;
	htt_tlv_filter.ppdu_end_user_stats = 0;
	htt_tlv_filter.ppdu_end_user_stats_ext = 0;
	htt_tlv_filter.ppdu_end_status_done = 0;
	htt_tlv_filter.enable_fp = 1;
	htt_tlv_filter.enable_md = 0;
	htt_tlv_filter.enable_md = 0;
	htt_tlv_filter.enable_mo = 0;

	htt_tlv_filter.fp_mgmt_filter = 0;
	htt_tlv_filter.fp_ctrl_filter = FILTER_CTRL_BA_REQ;
	htt_tlv_filter.fp_data_filter = (FILTER_DATA_UCAST |
					 FILTER_DATA_DATA);
	htt_tlv_filter.fp_data_filter |=
		hal_rx_en_mcast_fp_data_filter(soc->hal_soc) ?
					FILTER_DATA_MCAST : 0;
	htt_tlv_filter.mo_mgmt_filter = 0;
	htt_tlv_filter.mo_ctrl_filter = 0;
	htt_tlv_filter.mo_data_filter = 0;
	htt_tlv_filter.md_data_filter = 0;

	htt_tlv_filter.offset_valid = true;

	/* Not subscribing to mpdu_end, msdu_start and rx_attn */
	htt_tlv_filter.rx_mpdu_end_offset = 0;
	htt_tlv_filter.rx_msdu_start_offset = 0;
	htt_tlv_filter.rx_attn_offset = 0;

	/*
	 * For monitor mode, the packet hdr tlv is enabled later during
	 * filter update
	 */
	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() == QDF_GLOBAL_MONITOR_MODE)
		htt_tlv_filter.rx_packet_offset = soc->rx_mon_pkt_tlv_size;
	else
		htt_tlv_filter.rx_packet_offset = soc->rx_pkt_tlv_size;

	htt_tlv_filter.rx_header_offset =
				hal_rx_pkt_tlv_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_mpdu_start_offset =
				hal_rx_mpdu_start_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_msdu_end_offset =
				hal_rx_msdu_end_offset_get(soc->hal_soc);

	dp_info("TLV subscription\n"
		"msdu_start %d, mpdu_end %d, attention %d"
		"mpdu_start %d, msdu_end %d, pkt_hdr %d, pkt %d\n"
		"TLV offsets\n"
		"msdu_start %d, mpdu_end %d, attention %d"
		"mpdu_start %d, msdu_end %d, pkt_hdr %d, pkt %d\n",
		htt_tlv_filter.msdu_start,
		htt_tlv_filter.mpdu_end,
		htt_tlv_filter.attention,
		htt_tlv_filter.mpdu_start,
		htt_tlv_filter.msdu_end,
		htt_tlv_filter.packet_header,
		htt_tlv_filter.packet,
		htt_tlv_filter.rx_msdu_start_offset,
		htt_tlv_filter.rx_mpdu_end_offset,
		htt_tlv_filter.rx_attn_offset,
		htt_tlv_filter.rx_mpdu_start_offset,
		htt_tlv_filter.rx_msdu_end_offset,
		htt_tlv_filter.rx_header_offset,
		htt_tlv_filter.rx_packet_offset);

	dp_rxdma_ring_wmask_cfg_be(soc, &htt_tlv_filter);
	for (i = 0; i < MAX_PDEV_CNT; i++) {
		struct dp_pdev *pdev = soc->pdev_list[i];

		if (!pdev)
			continue;

		for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++) {
			int mac_for_pdev =
				dp_get_mac_id_for_pdev(mac_id, pdev->pdev_id);
			/*
			 * Obtain lmac id from pdev to access the LMAC ring
			 * in soc context
			 */
			int lmac_id =
				dp_get_lmac_id_for_pdev_id(soc, mac_id,
							   pdev->pdev_id);

			rx_mac_srng = dp_get_rxdma_ring(pdev, lmac_id);

			if (!rx_mac_srng->hal_srng)
				continue;

			htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					    rx_mac_srng->hal_srng,
					    RXDMA_BUF, buf_size,
					    &htt_tlv_filter);
		}
	}
	return status;

}
#endif

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * dp_service_near_full_srngs_be() - Main bottom half callback for the
 *				near-full IRQs.
 * @soc: Datapath SoC handle
 * @int_ctx: Interrupt context
 * @dp_budget: Budget of the work that can be done in the bottom half
 *
 * Return: work done in the handler
 */
static uint32_t
dp_service_near_full_srngs_be(struct dp_soc *soc, struct dp_intr *int_ctx,
			      uint32_t dp_budget)
{
	int ring = 0;
	int budget = dp_budget;
	uint32_t work_done  = 0;
	uint32_t remaining_quota = dp_budget;
	struct dp_intr_stats *intr_stats = &int_ctx->intr_stats;
	int tx_ring_near_full_mask = int_ctx->tx_ring_near_full_mask;
	int rx_near_full_grp_1_mask = int_ctx->rx_near_full_grp_1_mask;
	int rx_near_full_grp_2_mask = int_ctx->rx_near_full_grp_2_mask;
	int rx_near_full_mask = rx_near_full_grp_1_mask |
				rx_near_full_grp_2_mask;

	dp_verbose_debug("rx_ring_near_full 0x%x tx_ring_near_full 0x%x",
			 rx_near_full_mask,
			 tx_ring_near_full_mask);

	if (rx_near_full_mask) {
		for (ring = 0; ring < soc->num_reo_dest_rings; ring++) {
			if (!(rx_near_full_mask & (1 << ring)))
				continue;

			work_done = dp_rx_nf_process(int_ctx,
					soc->reo_dest_ring[ring].hal_srng,
					ring, remaining_quota);
			if (work_done) {
				intr_stats->num_rx_ring_near_full_masks[ring]++;
				dp_verbose_debug("rx NF mask 0x%x ring %d, work_done %d budget %d",
						 rx_near_full_mask, ring,
						 work_done,
						 budget);
				budget -=  work_done;
				if (budget <= 0)
					goto budget_done;
				remaining_quota = budget;
			}
		}
	}

	if (tx_ring_near_full_mask) {
		for (ring = 0; ring < soc->num_tcl_data_rings; ring++) {
			if (!(tx_ring_near_full_mask & (1 << ring)))
				continue;

			work_done = dp_tx_comp_nf_handler(int_ctx, soc,
					soc->tx_comp_ring[ring].hal_srng,
					ring, remaining_quota);
			if (work_done) {
				intr_stats->num_tx_comp_ring_near_full_masks[ring]++;
				dp_verbose_debug("tx NF mask 0x%x ring %d, work_done %d budget %d",
						 tx_ring_near_full_mask, ring,
						 work_done, budget);
				budget -=  work_done;
				if (budget <= 0)
					break;
				remaining_quota = budget;
			}
		}
	}

	intr_stats->num_near_full_masks++;

budget_done:
	return dp_budget - budget;
}

/**
 * dp_srng_test_and_update_nf_params_be() - Check if the srng is in near full
 *				state and set the reap_limit appropriately
 *				as per the near full state
 * @soc: Datapath soc handle
 * @dp_srng: Datapath handle for SRNG
 * @max_reap_limit: [Output Buffer] Buffer to set the max reap limit as per
 *			the srng near-full state
 *
 * Return: 1, if the srng is in near-full state
 *	   0, if the srng is not in near-full state
 */
static int
dp_srng_test_and_update_nf_params_be(struct dp_soc *soc,
				     struct dp_srng *dp_srng,
				     int *max_reap_limit)
{
	return _dp_srng_test_and_update_nf_params(soc, dp_srng, max_reap_limit);
}

/**
 * dp_init_near_full_arch_ops_be() - Initialize the arch ops handler for the
 *			near full IRQ handling operations.
 * @arch_ops: arch ops handle
 *
 * Return: none
 */
static inline void
dp_init_near_full_arch_ops_be(struct dp_arch_ops *arch_ops)
{
	arch_ops->dp_service_near_full_srngs = dp_service_near_full_srngs_be;
	arch_ops->dp_srng_test_and_update_nf_params =
					dp_srng_test_and_update_nf_params_be;
}

#else
static inline void
dp_init_near_full_arch_ops_be(struct dp_arch_ops *arch_ops)
{
}
#endif

static inline
QDF_STATUS dp_srng_init_be(struct dp_soc *soc, struct dp_srng *srng,
			   int ring_type, int ring_num, int mac_id)
{
	return dp_srng_init_idx(soc, srng, ring_type, ring_num, mac_id, 0);
}

static QDF_STATUS dp_soc_interrupt_attach_be(struct cdp_soc_t *txrx_soc)
{
	return dp_soc_interrupt_attach(txrx_soc);
}

static QDF_STATUS dp_soc_attach_poll_be(struct cdp_soc_t *txrx_soc)
{
	return dp_soc_attach_poll(txrx_soc);
}

static void dp_soc_interrupt_detach_be(struct cdp_soc_t *txrx_soc)
{
	return dp_soc_interrupt_detach(txrx_soc);
}

static uint32_t dp_service_srngs_be(void *dp_ctx, uint32_t dp_budget, int cpu)
{
	return dp_service_srngs(dp_ctx, dp_budget, cpu);
}

#ifdef WLAN_SUPPORT_PPEDS
static void dp_soc_ppeds_srng_deinit(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	if (!be_soc->ppeds_handle)
		return;

	dp_srng_deinit(soc, &be_soc->ppe2tcl_ring, PPE2TCL, 0);
	wlan_minidump_remove(be_soc->ppe2tcl_ring.base_vaddr_unaligned,
			     be_soc->ppe2tcl_ring.alloc_size,
			     soc->ctrl_psoc,
			     WLAN_MD_DP_SRNG_PPE2TCL,
			     "ppe2tcl_ring");

	dp_srng_deinit(soc, &be_soc->reo2ppe_ring, REO2PPE, 0);
	wlan_minidump_remove(be_soc->reo2ppe_ring.base_vaddr_unaligned,
			     be_soc->reo2ppe_ring.alloc_size,
			     soc->ctrl_psoc,
			     WLAN_MD_DP_SRNG_REO2PPE,
			     "reo2ppe_ring");

	dp_srng_deinit(soc, &be_soc->ppeds_wbm_release_ring, WBM2SW_RELEASE,
		       WBM2_SW_PPE_REL_RING_ID);
	wlan_minidump_remove(be_soc->ppeds_wbm_release_ring.base_vaddr_unaligned,
			     be_soc->ppeds_wbm_release_ring.alloc_size,
			     soc->ctrl_psoc,
			     WLAN_MD_DP_SRNG_PPE_WBM2SW_RELEASE,
			     "ppeds_wbm_release_ring");

}

static void dp_soc_ppeds_srng_free(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	dp_srng_free(soc, &be_soc->ppeds_wbm_release_ring);

	dp_srng_free(soc, &be_soc->ppe2tcl_ring);

	dp_srng_free(soc, &be_soc->reo2ppe_ring);
}

static QDF_STATUS dp_soc_ppeds_srng_alloc(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	uint32_t entries;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	if (!be_soc->ppeds_handle)
		return QDF_STATUS_SUCCESS;

	entries = wlan_cfg_get_dp_soc_reo2ppe_ring_size(soc_cfg_ctx);

	if (dp_srng_alloc(soc, &be_soc->reo2ppe_ring, REO2PPE,
			  entries, 0)) {
		dp_err("%pK: dp_srng_alloc failed for reo2ppe", soc);
		goto fail;
	}

	entries = wlan_cfg_get_dp_soc_ppe2tcl_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &be_soc->ppe2tcl_ring, PPE2TCL,
			  entries, 0)) {
		dp_err("%pK: dp_srng_alloc failed for ppe2tcl_ring", soc);
		goto fail;
	}

	entries = wlan_cfg_tx_comp_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &be_soc->ppeds_wbm_release_ring, WBM2SW_RELEASE,
			  entries, 1)) {
		dp_err("%pK: dp_srng_alloc failed for ppeds_wbm_release_ring",
		       soc);
		goto fail;
	}

	return QDF_STATUS_SUCCESS;
fail:
	dp_soc_ppeds_srng_free(soc);
	return QDF_STATUS_E_NOMEM;
}

static QDF_STATUS dp_soc_ppeds_srng_init(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	hal_soc_handle_t hal_soc = soc->hal_soc;

	struct dp_ppe_ds_idxs idx = {0};

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	if (!be_soc->ppeds_handle)
		return QDF_STATUS_SUCCESS;

	if (dp_ppeds_register_soc_be(be_soc, &idx)) {
		dp_err("%pK: ppeds registration failed", soc);
		goto fail;
	}

	if (dp_srng_init_idx(soc, &be_soc->reo2ppe_ring, REO2PPE, 0, 0,
			     idx.reo2ppe_start_idx)) {
		dp_err("%pK: dp_srng_init failed for reo2ppe", soc);
		goto fail;
	}

	wlan_minidump_log(be_soc->reo2ppe_ring.base_vaddr_unaligned,
			  be_soc->reo2ppe_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_REO2PPE,
			  "reo2ppe_ring");

	hal_reo_config_reo2ppe_dest_info(hal_soc);

	if (dp_srng_init_idx(soc, &be_soc->ppe2tcl_ring, PPE2TCL, 0, 0,
			     idx.ppe2tcl_start_idx)) {
		dp_err("%pK: dp_srng_init failed for ppe2tcl_ring", soc);
		goto fail;
	}

	wlan_minidump_log(be_soc->ppe2tcl_ring.base_vaddr_unaligned,
			  be_soc->ppe2tcl_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_PPE2TCL,
			  "ppe2tcl_ring");

	hal_tx_config_rbm_mapping_be(soc->hal_soc,
				     be_soc->ppe2tcl_ring.hal_srng,
				     WBM2_SW_PPE_REL_MAP_ID);

	if (dp_srng_init(soc, &be_soc->ppeds_wbm_release_ring, WBM2SW_RELEASE,
			 WBM2_SW_PPE_REL_RING_ID, 0)) {
		dp_err("%pK: dp_srng_init failed for ppeds_wbm_release_ring",
		       soc);
		goto fail;
	}

	wlan_minidump_log(be_soc->ppeds_wbm_release_ring.base_vaddr_unaligned,
			  be_soc->ppeds_wbm_release_ring.alloc_size,
			  soc->ctrl_psoc, WLAN_MD_DP_SRNG_PPE_WBM2SW_RELEASE,
			  "ppeds_wbm_release_ring");

	return QDF_STATUS_SUCCESS;
fail:
	dp_soc_ppeds_srng_deinit(soc);
	return QDF_STATUS_E_NOMEM;
}
#else
static void dp_soc_ppeds_srng_deinit(struct dp_soc *soc)
{
}

static void dp_soc_ppeds_srng_free(struct dp_soc *soc)
{
}

static QDF_STATUS dp_soc_ppeds_srng_alloc(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_ppeds_srng_init(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static void dp_soc_srng_deinit_be(struct dp_soc *soc)
{
	uint32_t i;

	dp_soc_ppeds_srng_deinit(soc);

	if (soc->features.dmac_cmn_src_rxbuf_ring_enabled) {
		for (i = 0; i < soc->num_rx_refill_buf_rings; i++) {
			dp_ssr_dump_srng_unregister("rx_refill_buf_ring", i);
			dp_srng_deinit(soc, &soc->rx_refill_buf_ring[i],
				       RXDMA_BUF, 0);
		}
	}
}

static void dp_soc_srng_free_be(struct dp_soc *soc)
{
	uint32_t i;

	dp_soc_ppeds_srng_free(soc);

	if (soc->features.dmac_cmn_src_rxbuf_ring_enabled) {
		for (i = 0; i < soc->num_rx_refill_buf_rings; i++)
			dp_srng_free(soc, &soc->rx_refill_buf_ring[i]);
	}
}

static QDF_STATUS dp_soc_srng_alloc_be(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	uint32_t ring_size;
	uint32_t i;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	ring_size = wlan_cfg_get_dp_soc_rxdma_refill_ring_size(soc_cfg_ctx);
	if (soc->features.dmac_cmn_src_rxbuf_ring_enabled) {
		for (i = 0; i < soc->num_rx_refill_buf_rings; i++) {
			if (dp_srng_alloc(soc, &soc->rx_refill_buf_ring[i],
					  RXDMA_BUF, ring_size, 0)) {
				dp_err("%pK: dp_srng_alloc failed refill ring",
				       soc);
				goto fail;
			}
		}
	}

	if (dp_soc_ppeds_srng_alloc(soc)) {
		dp_err("%pK: ppe rings alloc failed",
		       soc);
		goto fail;
	}

	return QDF_STATUS_SUCCESS;
fail:
	dp_soc_srng_free_be(soc);
	return QDF_STATUS_E_NOMEM;
}

static QDF_STATUS dp_soc_srng_init_be(struct dp_soc *soc)
{
	int i = 0;

	if (soc->features.dmac_cmn_src_rxbuf_ring_enabled) {
		for (i = 0; i < soc->num_rx_refill_buf_rings; i++) {
			if (dp_srng_init(soc, &soc->rx_refill_buf_ring[i],
					 RXDMA_BUF, 0, 0)) {
				dp_err("%pK: dp_srng_init failed refill ring",
				       soc);
				goto fail;
			}
			dp_ssr_dump_srng_register("rx_refill_buf_ring",
						  &soc->rx_refill_buf_ring[i],
						  i);
		}
	}

	if (dp_soc_ppeds_srng_init(soc)) {
		dp_err("%pK: ppe ds rings init failed",
		       soc);
		goto fail;
	}

	return QDF_STATUS_SUCCESS;
fail:
	dp_soc_srng_deinit_be(soc);
	return QDF_STATUS_E_NOMEM;
}

#ifdef WLAN_FEATURE_11BE_MLO
static inline unsigned
dp_mlo_peer_find_hash_index(dp_mld_peer_hash_obj_t mld_hash_obj,
			    union dp_align_mac_addr *mac_addr)
{
	uint32_t index;

	index =
		mac_addr->align2.bytes_ab ^
		mac_addr->align2.bytes_cd ^
		mac_addr->align2.bytes_ef;

	index ^= index >> mld_hash_obj->mld_peer_hash.idx_bits;
	index &= mld_hash_obj->mld_peer_hash.mask;

	return index;
}

QDF_STATUS
dp_mlo_peer_find_hash_attach_be(dp_mld_peer_hash_obj_t mld_hash_obj,
				int hash_elems)
{
	int i, log2;

	if (!mld_hash_obj)
		return QDF_STATUS_E_FAILURE;

	hash_elems *= DP_PEER_HASH_LOAD_MULT;
	hash_elems >>= DP_PEER_HASH_LOAD_SHIFT;
	log2 = dp_log2_ceil(hash_elems);
	hash_elems = 1 << log2;

	mld_hash_obj->mld_peer_hash.mask = hash_elems - 1;
	mld_hash_obj->mld_peer_hash.idx_bits = log2;
	/* allocate an array of TAILQ peer object lists */
	mld_hash_obj->mld_peer_hash.bins = qdf_mem_malloc(
		hash_elems * sizeof(TAILQ_HEAD(anonymous_tail_q, dp_peer)));
	if (!mld_hash_obj->mld_peer_hash.bins)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < hash_elems; i++)
		TAILQ_INIT(&mld_hash_obj->mld_peer_hash.bins[i]);

	qdf_spinlock_create(&mld_hash_obj->mld_peer_hash_lock);

	return QDF_STATUS_SUCCESS;
}

void
dp_mlo_peer_find_hash_detach_be(dp_mld_peer_hash_obj_t mld_hash_obj)
{
	if (!mld_hash_obj)
		return;

	if (mld_hash_obj->mld_peer_hash.bins) {
		qdf_mem_free(mld_hash_obj->mld_peer_hash.bins);
		mld_hash_obj->mld_peer_hash.bins = NULL;
		qdf_spinlock_destroy(&mld_hash_obj->mld_peer_hash_lock);
	}
}

#ifdef WLAN_MLO_MULTI_CHIP
static QDF_STATUS dp_mlo_peer_find_hash_attach_wrapper(struct dp_soc *soc)
{
	/* In case of MULTI chip MLO peer hash table when MLO global object
	 * is created, avoid from SOC attach path
	 */
	return QDF_STATUS_SUCCESS;
}

static void dp_mlo_peer_find_hash_detach_wrapper(struct dp_soc *soc)
{
}

void dp_mlo_dev_ctxt_list_attach_wrapper(dp_mlo_dev_obj_t mlo_dev_obj)
{
}

void dp_mlo_dev_ctxt_list_detach_wrapper(dp_mlo_dev_obj_t mlo_dev_obj)
{
}
#else
static QDF_STATUS dp_mlo_peer_find_hash_attach_wrapper(struct dp_soc *soc)
{
	dp_mld_peer_hash_obj_t mld_hash_obj;

	mld_hash_obj = dp_mlo_get_peer_hash_obj(soc);

	if (!mld_hash_obj)
		return QDF_STATUS_E_FAILURE;

	return dp_mlo_peer_find_hash_attach_be(mld_hash_obj, soc->max_peers);
}

static void dp_mlo_peer_find_hash_detach_wrapper(struct dp_soc *soc)
{
	dp_mld_peer_hash_obj_t mld_hash_obj;

	mld_hash_obj = dp_mlo_get_peer_hash_obj(soc);

	if (!mld_hash_obj)
		return;

	return dp_mlo_peer_find_hash_detach_be(mld_hash_obj);
}

void dp_mlo_dev_ctxt_list_attach_wrapper(dp_mlo_dev_obj_t mlo_dev_obj)
{
	dp_mlo_dev_ctxt_list_attach(mlo_dev_obj);
}

void dp_mlo_dev_ctxt_list_detach_wrapper(dp_mlo_dev_obj_t mlo_dev_obj)
{
	dp_mlo_dev_ctxt_list_detach(mlo_dev_obj);
}
#endif

#ifdef QCA_ENHANCED_STATS_SUPPORT
static uint8_t
dp_get_hw_link_id_be(struct dp_pdev *pdev)
{
	struct dp_pdev_be *be_pdev = dp_get_be_pdev_from_dp_pdev(pdev);

	return be_pdev->mlo_link_id;
}
#else
static uint8_t
dp_get_hw_link_id_be(struct dp_pdev *pdev)
{
	return 0;
}
#endif /* QCA_ENHANCED_STATS_SUPPORT */

static struct dp_peer *
dp_mlo_peer_find_hash_find_be(struct dp_soc *soc,
			      uint8_t *peer_mac_addr,
			      int mac_addr_is_aligned,
			      enum dp_mod_id mod_id,
			      uint8_t vdev_id)
{
	union dp_align_mac_addr local_mac_addr_aligned, *mac_addr;
	uint32_t index;
	struct dp_peer *peer;
	struct dp_vdev *vdev;
	dp_mld_peer_hash_obj_t mld_hash_obj;

	mld_hash_obj = dp_mlo_get_peer_hash_obj(soc);
	if (!mld_hash_obj)
		return NULL;

	if (!mld_hash_obj->mld_peer_hash.bins)
		return NULL;

	if (mac_addr_is_aligned) {
		mac_addr = (union dp_align_mac_addr *)peer_mac_addr;
	} else {
		qdf_mem_copy(
			&local_mac_addr_aligned.raw[0],
			peer_mac_addr, QDF_MAC_ADDR_SIZE);
		mac_addr = &local_mac_addr_aligned;
	}

	if (vdev_id != DP_VDEV_ALL) {
		vdev = dp_vdev_get_ref_by_id(soc, vdev_id, mod_id);
		if (!vdev) {
			dp_err("vdev is null");
			return NULL;
		}
	} else {
		vdev = NULL;
	}

	/* search mld peer table if no link peer for given mac address */
	index = dp_mlo_peer_find_hash_index(mld_hash_obj, mac_addr);
	qdf_spin_lock_bh(&mld_hash_obj->mld_peer_hash_lock);
	TAILQ_FOREACH(peer, &mld_hash_obj->mld_peer_hash.bins[index],
		      hash_list_elem) {
		if (dp_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0) {
			if ((vdev_id == DP_VDEV_ALL) || (
				dp_peer_find_mac_addr_cmp(
						&peer->vdev->mld_mac_addr,
						&vdev->mld_mac_addr) == 0)) {
				/* take peer reference before returning */
				if (dp_peer_get_ref(NULL, peer, mod_id) !=
						QDF_STATUS_SUCCESS)
					peer = NULL;

				if (vdev)
					dp_vdev_unref_delete(soc, vdev, mod_id);

				qdf_spin_unlock_bh(
					&mld_hash_obj->mld_peer_hash_lock);
				return peer;
			}
		}
	}

	if (vdev)
		dp_vdev_unref_delete(soc, vdev, mod_id);

	qdf_spin_unlock_bh(&mld_hash_obj->mld_peer_hash_lock);

	return NULL; /* failure */
}

static void
dp_mlo_peer_find_hash_remove_be(struct dp_soc *soc, struct dp_peer *peer)
{
	uint32_t index;
	struct dp_peer *tmppeer = NULL;
	int found = 0;
	dp_mld_peer_hash_obj_t mld_hash_obj;

	mld_hash_obj = dp_mlo_get_peer_hash_obj(soc);

	if (!mld_hash_obj)
		return;

	index = dp_mlo_peer_find_hash_index(mld_hash_obj, &peer->mac_addr);
	QDF_ASSERT(!TAILQ_EMPTY(&mld_hash_obj->mld_peer_hash.bins[index]));

	qdf_spin_lock_bh(&mld_hash_obj->mld_peer_hash_lock);
	TAILQ_FOREACH(tmppeer, &mld_hash_obj->mld_peer_hash.bins[index],
		      hash_list_elem) {
		if (tmppeer == peer) {
			found = 1;
			break;
		}
	}
	QDF_ASSERT(found);
	TAILQ_REMOVE(&mld_hash_obj->mld_peer_hash.bins[index], peer,
		     hash_list_elem);

	dp_info("Peer %pK (" QDF_MAC_ADDR_FMT ") removed. (found %u)",
		peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw), found);
	dp_peer_unref_delete(peer, DP_MOD_ID_CONFIG);
	qdf_spin_unlock_bh(&mld_hash_obj->mld_peer_hash_lock);

}

static void
dp_mlo_peer_find_hash_add_be(struct dp_soc *soc, struct dp_peer *peer)
{
	uint32_t index;
	dp_mld_peer_hash_obj_t mld_hash_obj;

	mld_hash_obj = dp_mlo_get_peer_hash_obj(soc);

	if (!mld_hash_obj)
		return;

	index = dp_mlo_peer_find_hash_index(mld_hash_obj, &peer->mac_addr);

	qdf_spin_lock_bh(&mld_hash_obj->mld_peer_hash_lock);

	if (QDF_IS_STATUS_ERROR(dp_peer_get_ref(NULL, peer,
						DP_MOD_ID_CONFIG))) {
		dp_err("fail to get peer ref:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(peer->mac_addr.raw));
		qdf_spin_unlock_bh(&mld_hash_obj->mld_peer_hash_lock);
		return;
	}
	TAILQ_INSERT_TAIL(&mld_hash_obj->mld_peer_hash.bins[index], peer,
			  hash_list_elem);
	qdf_spin_unlock_bh(&mld_hash_obj->mld_peer_hash_lock);

	dp_info("Peer %pK (" QDF_MAC_ADDR_FMT ") added",
		peer, QDF_MAC_ADDR_REF(peer->mac_addr.raw));
}

void dp_print_mlo_ast_stats_be(struct dp_soc *soc)
{
	uint32_t index;
	struct dp_peer *peer;
	dp_mld_peer_hash_obj_t mld_hash_obj;

	mld_hash_obj = dp_mlo_get_peer_hash_obj(soc);

	if (!mld_hash_obj)
		return;

	qdf_spin_lock_bh(&mld_hash_obj->mld_peer_hash_lock);
	for (index = 0; index < mld_hash_obj->mld_peer_hash.mask; index++) {
		TAILQ_FOREACH(peer, &mld_hash_obj->mld_peer_hash.bins[index],
			      hash_list_elem) {
			dp_print_peer_ast_entries(soc, peer, NULL);
		}
	}
	qdf_spin_unlock_bh(&mld_hash_obj->mld_peer_hash_lock);
}
#else /* WLAN_FEATURE_11BE_MLO */
void dp_mlo_dev_ctxt_list_attach_wrapper(dp_mlo_dev_obj_t mlo_dev_obj)
{
}

void dp_mlo_dev_ctxt_list_detach_wrapper(dp_mlo_dev_obj_t mlo_dev_obj)
{
}
#endif /* WLAN_FEATURE_11BE_MLO */

#if defined(DP_UMAC_HW_HARD_RESET) && defined(DP_UMAC_HW_RESET_SUPPORT)
static void dp_reconfig_tx_vdev_mcast_ctrl_be(struct dp_soc *soc,
					      struct dp_vdev *vdev)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	hal_soc_handle_t hal_soc = soc->hal_soc;
	uint8_t vdev_id = vdev->vdev_id;

	if (vdev->opmode == wlan_op_mode_sta) {
		if (vdev->pdev->isolation)
			hal_tx_vdev_mcast_ctrl_set(hal_soc, vdev_id,
						HAL_TX_MCAST_CTRL_FW_EXCEPTION);
		else
			hal_tx_vdev_mcast_ctrl_set(hal_soc, vdev_id,
						HAL_TX_MCAST_CTRL_MEC_NOTIFY);
	} else if (vdev->opmode == wlan_op_mode_ap) {
		hal_tx_mcast_mlo_reinject_routing_set(
					hal_soc,
					HAL_TX_MCAST_MLO_REINJECT_TQM_NOTIFY);
		if (vdev->mlo_vdev) {
			hal_tx_vdev_mcast_ctrl_set(
						hal_soc,
						vdev_id,
						HAL_TX_MCAST_CTRL_NO_SPECIAL);
		} else {
			hal_tx_vdev_mcast_ctrl_set(hal_soc,
						   vdev_id,
						   HAL_TX_MCAST_CTRL_FW_EXCEPTION);
		}
	}
}

static void dp_bank_reconfig_be(struct dp_soc *soc, struct dp_vdev *vdev)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	union hal_tx_bank_config *bank_config;

	if (!be_vdev || be_vdev->bank_id == DP_BE_INVALID_BANK_ID)
		return;

	bank_config = &be_soc->bank_profiles[be_vdev->bank_id].bank_config;

	hal_tx_populate_bank_register(be_soc->soc.hal_soc, bank_config,
				      be_vdev->bank_id);
}

#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP) && \
	defined(WLAN_MCAST_MLO)
static void dp_mlo_mcast_reset_pri_mcast(struct dp_vdev_be *be_vdev,
					 struct dp_vdev *ptnr_vdev,
					 void *arg)
{
	struct dp_vdev_be *be_ptnr_vdev =
				dp_get_be_vdev_from_dp_vdev(ptnr_vdev);

	be_ptnr_vdev->mcast_primary = false;
}

#if defined(CONFIG_MLO_SINGLE_DEV)
static void dp_txrx_set_mlo_mcast_primary_vdev_param_be(
					struct dp_vdev *vdev,
					cdp_config_param_type val)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(
						be_vdev->vdev.pdev->soc);

	be_vdev->mcast_primary = val.cdp_vdev_param_mcast_vdev;
	vdev->mlo_vdev = 1;

	if (be_vdev->mcast_primary) {
		struct cdp_txrx_peer_params_update params = {0};

		dp_mlo_iter_ptnr_vdev(be_soc, be_vdev,
				      dp_mlo_mcast_reset_pri_mcast,
				      (void *)&be_vdev->mcast_primary,
				      DP_MOD_ID_TX_MCAST,
				      DP_LINK_VDEV_ITER,
				      DP_VDEV_ITERATE_SKIP_SELF);

		params.chip_id = be_soc->mlo_chip_id;
		params.pdev_id = be_vdev->vdev.pdev->pdev_id;
		params.vdev_id = vdev->vdev_id;
		dp_wdi_event_handler(
				WDI_EVENT_MCAST_PRIMARY_UPDATE,
				be_vdev->vdev.pdev->soc,
				(void *)&params, CDP_INVALID_PEER,
				WDI_NO_VAL, params.pdev_id);
	}
}

static void dp_get_vdev_stats_for_unmap_peer_mlo(struct dp_vdev *vdev,
						    struct dp_peer *peer)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	struct cdp_vdev_stats *vdev_stats = &be_vdev->mlo_stats;
	struct dp_txrx_peer *txrx_peer = dp_get_txrx_peer(peer);
	struct dp_pdev *pdev = vdev->pdev;
	struct dp_soc *soc = vdev->pdev->soc;
	uint8_t link_id = dp_get_peer_hw_link_id(soc, pdev);
	struct dp_peer_per_pkt_stats *per_pkt_stats;

	if (!txrx_peer)
		goto link_stats;

	dp_peer_aggregate_tid_stats(peer);

	if (!IS_MLO_DP_LINK_PEER(peer)) {
		per_pkt_stats = &txrx_peer->stats[0].per_pkt_stats;
		dp_update_vdev_basic_stats(txrx_peer, vdev_stats);
		DP_UPDATE_PER_PKT_STATS(vdev_stats, per_pkt_stats);
	}

	if (IS_MLO_DP_LINK_PEER(peer)) {
		link_id = dp_get_peer_hw_link_id(soc, pdev);
		if (link_id > 0) {
			per_pkt_stats =
				&txrx_peer->stats[link_id].per_pkt_stats;
			DP_UPDATE_PER_PKT_STATS(vdev_stats, per_pkt_stats);
		}
	}

link_stats:
	dp_monitor_peer_get_stats(soc, peer, vdev_stats, UPDATE_VDEV_STATS_MLD);
}

static
void dp_get_vdev_stats_for_unmap_peer_be(struct dp_vdev *vdev,
					 struct dp_peer *peer)
{

	if (IS_DP_LEGACY_PEER(peer))
		dp_get_vdev_stats_for_unmap_peer_legacy(vdev, peer);
	else
		dp_get_vdev_stats_for_unmap_peer_mlo(vdev, peer);
}
#else
static void dp_txrx_set_mlo_mcast_primary_vdev_param_be(
					struct dp_vdev *vdev,
					cdp_config_param_type val)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(
						be_vdev->vdev.pdev->soc);

	be_vdev->mcast_primary = val.cdp_vdev_param_mcast_vdev;
	vdev->mlo_vdev = 1;
	hal_tx_vdev_mcast_ctrl_set(vdev->pdev->soc->hal_soc,
				   vdev->vdev_id,
				   HAL_TX_MCAST_CTRL_NO_SPECIAL);

	if (be_vdev->mcast_primary) {
		struct cdp_txrx_peer_params_update params = {0};

		dp_mlo_iter_ptnr_vdev(be_soc, be_vdev,
				      dp_mlo_mcast_reset_pri_mcast,
				      (void *)&be_vdev->mcast_primary,
				      DP_MOD_ID_TX_MCAST,
				      DP_LINK_VDEV_ITER,
				      DP_VDEV_ITERATE_SKIP_SELF);

		params.chip_id = be_soc->mlo_chip_id;
		params.pdev_id = vdev->pdev->pdev_id;
		params.vdev_id = vdev->vdev_id;
		dp_wdi_event_handler(
				WDI_EVENT_MCAST_PRIMARY_UPDATE,
				vdev->pdev->soc,
				(void *)&params, CDP_INVALID_PEER,
				WDI_NO_VAL, params.pdev_id);
	}
}
#endif

static void dp_txrx_reset_mlo_mcast_primary_vdev_param_be(
					struct dp_vdev *vdev,
					cdp_config_param_type val)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	be_vdev->mcast_primary = false;
	vdev->mlo_vdev = 0;
	hal_tx_vdev_mcast_ctrl_set(vdev->pdev->soc->hal_soc,
				   vdev->vdev_id,
				   HAL_TX_MCAST_CTRL_FW_EXCEPTION);
}

/**
 * dp_txrx_get_vdev_mcast_param_be() - Target specific ops for getting vdev
 *                                      params related to multicast
 * @soc: DP soc handle
 * @vdev: pointer to vdev structure
 * @val: buffer address
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS dp_txrx_get_vdev_mcast_param_be(struct dp_soc *soc,
					   struct dp_vdev *vdev,
					   cdp_config_param_type *val)
{
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	if (be_vdev->mcast_primary)
		val->cdp_vdev_param_mcast_vdev = true;
	else
		val->cdp_vdev_param_mcast_vdev = false;

	return QDF_STATUS_SUCCESS;
}
#else
static void dp_txrx_set_mlo_mcast_primary_vdev_param_be(
					struct dp_vdev *vdev,
					cdp_config_param_type val)
{
}

static void dp_txrx_reset_mlo_mcast_primary_vdev_param_be(
					struct dp_vdev *vdev,
					cdp_config_param_type val)
{
}

static
QDF_STATUS dp_txrx_get_vdev_mcast_param_be(struct dp_soc *soc,
					   struct dp_vdev *vdev,
					   cdp_config_param_type *val)
{
	return QDF_STATUS_SUCCESS;
}

static
void dp_get_vdev_stats_for_unmap_peer_be(struct dp_vdev *vdev,
					 struct dp_peer *peer)
{
}
#endif

#ifdef DP_TX_IMPLICIT_RBM_MAPPING
static void dp_tx_implicit_rbm_set_be(struct dp_soc *soc,
				      uint8_t tx_ring_id,
				      uint8_t bm_id)
{
	hal_tx_config_rbm_mapping_be(soc->hal_soc,
				     soc->tcl_data_ring[tx_ring_id].hal_srng,
				     bm_id);
}
#else
static void dp_tx_implicit_rbm_set_be(struct dp_soc *soc,
				      uint8_t tx_ring_id,
				      uint8_t bm_id)
{
}
#endif

/**
 * dp_txrx_set_vdev_param_be() - Target specific ops while setting vdev params
 * @soc: DP soc handle
 * @vdev: pointer to vdev structure
 * @param: parameter type to get value
 * @val: value
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS dp_txrx_set_vdev_param_be(struct dp_soc *soc,
				     struct dp_vdev *vdev,
				     enum cdp_vdev_param_type param,
				     cdp_config_param_type val)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_vdev_be *be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	switch (param) {
	case CDP_TX_ENCAP_TYPE:
	case CDP_UPDATE_DSCP_TO_TID_MAP:
	case CDP_UPDATE_TDLS_FLAGS:
		dp_tx_update_bank_profile(be_soc, be_vdev);
		dp_tx_update_vp_profile(be_soc, be_vdev);
		break;
	case CDP_ENABLE_CIPHER:
		if (vdev->tx_encap_type == htt_cmn_pkt_type_raw)
			dp_tx_update_bank_profile(be_soc, be_vdev);
		break;
	case CDP_SET_MCAST_VDEV:
		dp_txrx_set_mlo_mcast_primary_vdev_param_be(vdev, val);
		break;
	case CDP_RESET_MLO_MCAST_VDEV:
		dp_txrx_reset_mlo_mcast_primary_vdev_param_be(vdev, val);
		break;
	default:
		dp_warn("invalid param %d", param);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef DP_USE_REDUCED_PEER_ID_FIELD_WIDTH
static inline void
dp_soc_max_peer_id_set(struct dp_soc *soc)
{
	soc->peer_id_shift = dp_log2_ceil(soc->max_peers);
	soc->peer_id_mask = (1 << soc->peer_id_shift) - 1;
	/*
	 * Double the peers since we use ML indication bit
	 * alongwith peer_id to find peers.
	 */
	soc->max_peer_id = 1 << (soc->peer_id_shift + 1);
}
#else
static inline void
dp_soc_max_peer_id_set(struct dp_soc *soc)
{
	soc->max_peer_id =
		(1 << (HTT_RX_PEER_META_DATA_V1_ML_PEER_VALID_S + 1)) - 1;
}
#endif /* DP_USE_REDUCED_PEER_ID_FIELD_WIDTH */
#else
static inline void
dp_soc_max_peer_id_set(struct dp_soc *soc)
{
	soc->max_peer_id = soc->max_peers;
}
#endif /* WLAN_FEATURE_11BE_MLO */

static void dp_peer_map_detach_be(struct dp_soc *soc)
{
	if (soc->host_ast_db_enable)
		dp_peer_ast_hash_detach(soc);
}

static QDF_STATUS dp_peer_map_attach_be(struct dp_soc *soc)
{
	QDF_STATUS status;

	if (soc->host_ast_db_enable) {
		status = dp_peer_ast_hash_attach(soc);
		if (QDF_IS_STATUS_ERROR(status))
			return status;
	}

	dp_soc_max_peer_id_set(soc);

	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_DP_MLO_DEV_CTX)

void dp_mlo_dev_ctxt_list_attach(dp_mlo_dev_obj_t mlo_dev_obj)
{
	TAILQ_INIT(&mlo_dev_obj->mlo_dev_list);
	qdf_spinlock_create(&mlo_dev_obj->mlo_dev_list_lock);
}

void dp_mlo_dev_ctxt_list_detach(dp_mlo_dev_obj_t mlo_dev_obj)
{
	struct dp_mlo_dev_ctxt *mld_ctxt = NULL;
	struct dp_mlo_dev_ctxt *tmp_mld_ctxt = NULL;

	if (!TAILQ_EMPTY(&mlo_dev_obj->mlo_dev_list)) {
		dp_alert("DP MLO dev list is not empty");
		qdf_spin_lock_bh(&mlo_dev_obj->mlo_dev_list_lock);
		TAILQ_FOREACH_SAFE(mld_ctxt, &mlo_dev_obj->mlo_dev_list,
				   ml_dev_list_elem, tmp_mld_ctxt) {
			if (mld_ctxt) {
				dp_alert("MLD MAC " QDF_MAC_ADDR_FMT " ",
					 QDF_MAC_ADDR_REF(
						&mld_ctxt->mld_mac_addr.raw));
				qdf_mem_free(mld_ctxt);
			}
		}
		qdf_spin_unlock_bh(&mlo_dev_obj->mlo_dev_list_lock);
	}

	qdf_spinlock_destroy(&mlo_dev_obj->mlo_dev_list_lock);
}

void dp_mlo_dev_ctxt_unref_delete(struct dp_mlo_dev_ctxt *mlo_dev_ctxt,
				  enum dp_mod_id mod_id)
{
	QDF_ASSERT(qdf_atomic_dec_return(&mlo_dev_ctxt->mod_refs[mod_id]) >= 0);

	/* Return if this is not the last reference*/
	if (!qdf_atomic_dec_and_test(&mlo_dev_ctxt->ref_cnt))
		return;

	QDF_ASSERT(mlo_dev_ctxt->ref_delete_pending);
	qdf_spinlock_destroy(&mlo_dev_ctxt->vdev_list_lock);
	qdf_mem_free(mlo_dev_ctxt);
}

QDF_STATUS dp_mlo_dev_get_ref(struct dp_mlo_dev_ctxt *mlo_dev_ctxt,
			      enum dp_mod_id mod_id)
{
	if (!qdf_atomic_inc_return(&mlo_dev_ctxt->ref_cnt))
		return QDF_STATUS_E_INVAL;

	qdf_atomic_inc(&mlo_dev_ctxt->mod_refs[mod_id]);

	return QDF_STATUS_SUCCESS;
}

struct dp_mlo_dev_ctxt *
dp_get_mlo_dev_ctx_by_mld_mac_addr(struct dp_soc_be *be_soc,
				   uint8_t *mldaddr,
				   enum dp_mod_id mod_id)
{
	struct dp_mlo_dev_ctxt *mld_cur = NULL;
	struct dp_mlo_dev_ctxt *tmp_mld_cur = NULL;
	dp_mlo_dev_obj_t mlo_dev_obj;

	mlo_dev_obj = dp_get_mlo_dev_list_obj(be_soc);
	if (!mlo_dev_obj) {
		dp_err("DP Global MLO Context is NULL");
		return NULL;
	}

	/*
	 * Iterate through ml dev list, till mldaddr matches with
	 * entry of list
	 */
	qdf_spin_lock_bh(&mlo_dev_obj->mlo_dev_list_lock);
	TAILQ_FOREACH_SAFE(mld_cur, &mlo_dev_obj->mlo_dev_list,
			   ml_dev_list_elem, tmp_mld_cur) {
		if (!qdf_mem_cmp(&mld_cur->mld_mac_addr.raw, mldaddr,
				 QDF_MAC_ADDR_SIZE)) {
			if (dp_mlo_dev_get_ref(mld_cur, mod_id)
			    == QDF_STATUS_SUCCESS) {
				qdf_spin_unlock_bh(&mlo_dev_obj->mlo_dev_list_lock);
				return mld_cur;
			}
		}
	}
	qdf_spin_unlock_bh(&mlo_dev_obj->mlo_dev_list_lock);
	return NULL;
}

/**
 * dp_mlo_dev_ctxt_create() - Allocate DP MLO dev context
 * @soc_hdl: SOC handle
 * @mld_mac_addr: MLD MAC address
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS dp_mlo_dev_ctxt_create(struct cdp_soc_t *soc_hdl,
				  uint8_t *mld_mac_addr)
{
	struct dp_mlo_dev_ctxt *mlo_dev_ctxt = NULL;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	dp_mlo_dev_obj_t mlo_dev_obj;

	mlo_dev_obj = dp_get_mlo_dev_list_obj(be_soc);
	if (!mlo_dev_obj) {
		dp_err("DP Global MLO Context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* check if MLO dev ctx already available */
	mlo_dev_ctxt = dp_get_mlo_dev_ctx_by_mld_mac_addr(be_soc,
							  mld_mac_addr,
							  DP_MOD_ID_MLO_DEV);
	if (mlo_dev_ctxt) {
		dp_mlo_dev_ctxt_unref_delete(mlo_dev_ctxt, DP_MOD_ID_MLO_DEV);
		/* assert if we get two create request for same MLD MAC */
		qdf_assert_always(0);
	}

	/* Allocate MLO dev ctx */
	mlo_dev_ctxt = qdf_mem_malloc(sizeof(struct dp_mlo_dev_ctxt));

	if (!mlo_dev_ctxt) {
		dp_err("Failed to allocate DP MLO Dev Context");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_copy_macaddr((struct qdf_mac_addr *)&mlo_dev_ctxt->mld_mac_addr.raw[0],
			 (struct qdf_mac_addr *)mld_mac_addr);

	qdf_mem_set(mlo_dev_ctxt->vdev_list,
		    WLAN_MAX_MLO_CHIPS * WLAN_MAX_MLO_LINKS_PER_SOC,
		    CDP_INVALID_VDEV_ID);
	qdf_mem_set(mlo_dev_ctxt->bridge_vdev,
		    WLAN_MAX_MLO_CHIPS * WLAN_MAX_MLO_LINKS_PER_SOC,
		    CDP_INVALID_VDEV_ID);
	mlo_dev_ctxt->seq_num = 0;

	/* Add mlo_dev_ctxt to the global DP MLO list */
	qdf_spin_lock_bh(&mlo_dev_obj->mlo_dev_list_lock);
	TAILQ_INSERT_TAIL(&mlo_dev_obj->mlo_dev_list,
			  mlo_dev_ctxt, ml_dev_list_elem);
	qdf_spin_unlock_bh(&mlo_dev_obj->mlo_dev_list_lock);

	/* Ref for MLO ctxt saved in global list */
	dp_mlo_dev_get_ref(mlo_dev_ctxt, DP_MOD_ID_CONFIG);

	mlo_dev_ctxt->ref_delete_pending = 0;
	qdf_spinlock_create(&mlo_dev_ctxt->vdev_list_lock);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_mlo_dev_ctxt_destroy() - Destroy DP MLO dev context
 * @soc_hdl: SOC handle
 * @mld_mac_addr: MLD MAC address
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS dp_mlo_dev_ctxt_destroy(struct cdp_soc_t *soc_hdl,
				   uint8_t *mld_mac_addr)
{
	struct dp_mlo_dev_ctxt *mlo_dev_ctxt = NULL;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	dp_mlo_dev_obj_t mlo_dev_obj;

	mlo_dev_obj = dp_get_mlo_dev_list_obj(be_soc);
	if (!mlo_dev_obj) {
		dp_err("DP Global MLO Context is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* GET mlo_dev_ctxt from the global list */
	mlo_dev_ctxt = dp_get_mlo_dev_ctx_by_mld_mac_addr(be_soc,
							  mld_mac_addr,
							  DP_MOD_ID_MLO_DEV);
	if (!mlo_dev_ctxt) {
		dp_err("Failed to get DP MLO Dev Context by MLD mac addr");
		return QDF_STATUS_E_INVAL;
	}

	if (mlo_dev_ctxt->vdev_count)
		dp_alert("deleting MLO dev ctxt with non zero vdev count");

	qdf_spin_lock_bh(&mlo_dev_obj->mlo_dev_list_lock);
	TAILQ_REMOVE(&mlo_dev_obj->mlo_dev_list,
		     mlo_dev_ctxt, ml_dev_list_elem);
	qdf_spin_unlock_bh(&mlo_dev_obj->mlo_dev_list_lock);

	/* unref for MLO ctxt ref released from Global list */
	dp_mlo_dev_ctxt_unref_delete(mlo_dev_ctxt, DP_MOD_ID_CONFIG);

	mlo_dev_ctxt->ref_delete_pending = 1;
	dp_mlo_dev_ctxt_unref_delete(mlo_dev_ctxt, DP_MOD_ID_MLO_DEV);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_mlo_dev_ctxt_vdev_attach() - Attach vdev to DP MLO dev context
 * @soc_hdl: SOC handle
 * @vdev_id: vdev id for the vdev to be attached
 * @mld_mac_addr: MLD MAC address
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS dp_mlo_dev_ctxt_vdev_attach(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *mld_mac_addr)
{
	struct dp_mlo_dev_ctxt *mlo_dev_ctxt = NULL;
	struct dp_vdev *vdev = NULL;
	struct dp_vdev_be *be_vdev = NULL;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;
	be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	/* GET mlo_dev_ctxt from the global list */
	mlo_dev_ctxt = dp_get_mlo_dev_ctx_by_mld_mac_addr(be_soc,
							  mld_mac_addr,
							  DP_MOD_ID_MLO_DEV);
	if (!mlo_dev_ctxt) {
		dp_err("Failed to get MLO ctxt for " QDF_MAC_ADDR_FMT "",
		       QDF_MAC_ADDR_REF(mld_mac_addr));
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_E_INVAL;
	}

	dp_attach_vdev_list_in_mlo_dev_ctxt(be_soc, vdev, mlo_dev_ctxt);
	be_vdev->mlo_dev_ctxt = mlo_dev_ctxt;

	/* ref for holding MLO ctxt in be_vdev */
	dp_mlo_dev_get_ref(mlo_dev_ctxt, DP_MOD_ID_CHILD);

	/* unref for mlo ctxt taken at the start of this function */
	dp_mlo_dev_ctxt_unref_delete(mlo_dev_ctxt, DP_MOD_ID_MLO_DEV);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_mlo_dev_ctxt_vdev_detach() - Detach vdev from DP MLO dev context
 * @soc_hdl: SOC handle
 * @vdev_id: vdev id for the vdev to be attached
 * @mld_mac_addr: MLD MAC address
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS dp_mlo_dev_ctxt_vdev_detach(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *mld_mac_addr)
{
	struct dp_vdev *vdev = NULL;
	struct dp_vdev_be *be_vdev = NULL;
	struct dp_mlo_dev_ctxt *mlo_dev_ctxt = NULL;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	/* GET mlo_dev_ctxt from the global list */
	mlo_dev_ctxt = dp_get_mlo_dev_ctx_by_mld_mac_addr(be_soc,
							  mld_mac_addr,
							  DP_MOD_ID_MLO_DEV);

	if (!mlo_dev_ctxt) {
		dp_err("Failed to get DP MLO Dev Context by MLD mac addr");
		if (!be_vdev->mlo_dev_ctxt) {
			dp_err("Failed to get DP MLO Dev Context from vdev");
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
			return QDF_STATUS_E_INVAL;
		}
		mlo_dev_ctxt = be_vdev->mlo_dev_ctxt;
		dp_mlo_dev_get_ref(mlo_dev_ctxt, DP_MOD_ID_MLO_DEV);
	}

	if (dp_detach_vdev_list_in_mlo_dev_ctxt(be_soc, vdev, mlo_dev_ctxt)
	    != QDF_STATUS_SUCCESS) {
		dp_mlo_dev_ctxt_unref_delete(mlo_dev_ctxt, DP_MOD_ID_MLO_DEV);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_SUCCESS;
	}

	be_vdev->mlo_dev_ctxt = NULL;

	/* Save vdev stats in MLO dev ctx */
	dp_update_mlo_mld_vdev_ctxt_stats(&mlo_dev_ctxt->stats, &vdev->stats);

	/* reset vdev stats to zero */
	qdf_mem_set(&vdev->stats, sizeof(struct dp_vdev_stats), 0);

	/* unref for mlo ctxt removed from be_vdev*/
	dp_mlo_dev_ctxt_unref_delete(mlo_dev_ctxt, DP_MOD_ID_CHILD);

	/* unref for mlo ctxt taken at the start of this function */
	dp_mlo_dev_ctxt_unref_delete(mlo_dev_ctxt, DP_MOD_ID_MLO_DEV);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}
#else
void dp_mlo_dev_ctxt_list_attach(dp_mlo_dev_obj_t mlo_dev_obj)
{
}

void dp_mlo_dev_ctxt_list_detach(dp_mlo_dev_obj_t mlo_dev_obj)
{
}

static inline
QDF_STATUS dp_mlo_dev_ctxt_create(struct cdp_soc_t *soc_hdl,
				  uint8_t *mld_mac_addr)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_mlo_dev_ctxt_destroy(struct cdp_soc_t *soc_hdl,
				   uint8_t *mld_mac_addr)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_mlo_dev_ctxt_vdev_attach(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *mld_mac_addr)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_mlo_dev_ctxt_vdev_detach(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id,
				       uint8_t *mld_mac_addr)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_DP_MLO_DEV_CTX */

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_MCAST_MLO
static inline void
dp_initialize_arch_ops_be_mcast_mlo(struct dp_arch_ops *arch_ops)
{
	arch_ops->dp_tx_mcast_handler = dp_tx_mlo_mcast_handler_be;
	arch_ops->dp_rx_mcast_handler = dp_rx_mlo_igmp_handler;
	arch_ops->dp_tx_is_mcast_primary = dp_tx_mlo_is_mcast_primary_be;
}
#else /* WLAN_MCAST_MLO */
static inline void
dp_initialize_arch_ops_be_mcast_mlo(struct dp_arch_ops *arch_ops)
{
}
#endif /* WLAN_MCAST_MLO */

#ifdef WLAN_MLO_MULTI_CHIP
static inline void
dp_initialize_arch_ops_be_mlo_multi_chip(struct dp_arch_ops *arch_ops)
{
	arch_ops->dp_partner_chips_map = dp_mlo_partner_chips_map;
	arch_ops->dp_partner_chips_unmap = dp_mlo_partner_chips_unmap;
	arch_ops->dp_soc_get_by_idle_bm_id = dp_soc_get_by_idle_bm_id;
	arch_ops->dp_get_soc_by_chip_id = dp_get_soc_by_chip_id_be;
	arch_ops->dp_mlo_print_ptnr_info = dp_mlo_debug_print_ptnr_info;
	arch_ops->dp_get_interface_stats = dp_get_interface_stats_be;
	arch_ops->mlo_get_chip_id = dp_mlo_get_chip_id;
	arch_ops->mlo_link_peer_find_hash_find_by_chip_id =
				dp_mlo_link_peer_hash_find_by_chip_id;
}
#else
static inline void
dp_initialize_arch_ops_be_mlo_multi_chip(struct dp_arch_ops *arch_ops)
{
}
#endif

static inline void
dp_initialize_arch_ops_be_mlo(struct dp_arch_ops *arch_ops)
{
	dp_initialize_arch_ops_be_mcast_mlo(arch_ops);
	dp_initialize_arch_ops_be_mlo_multi_chip(arch_ops);
	arch_ops->mlo_peer_find_hash_detach =
	dp_mlo_peer_find_hash_detach_wrapper;
	arch_ops->mlo_peer_find_hash_attach =
	dp_mlo_peer_find_hash_attach_wrapper;
	arch_ops->mlo_peer_find_hash_add = dp_mlo_peer_find_hash_add_be;
	arch_ops->mlo_peer_find_hash_remove = dp_mlo_peer_find_hash_remove_be;
	arch_ops->mlo_peer_find_hash_find = dp_mlo_peer_find_hash_find_be;
	arch_ops->get_hw_link_id = dp_get_hw_link_id_be;
}

static struct cdp_cmn_mlo_ops dp_cmn_mlo_ops = {
	.mlo_dev_ctxt_create = dp_mlo_dev_ctxt_create,
	.mlo_dev_ctxt_attach = dp_mlo_dev_ctxt_vdev_attach,
	.mlo_dev_ctxt_detach = dp_mlo_dev_ctxt_vdev_detach,
	.mlo_dev_ctxt_destroy = dp_mlo_dev_ctxt_destroy,
};

void dp_soc_initialize_cdp_cmn_mlo_ops(struct dp_soc *soc)
{
	soc->cdp_soc.ops->cmn_mlo_ops = &dp_cmn_mlo_ops;
}
#else /* WLAN_FEATURE_11BE_MLO */
static inline void
dp_initialize_arch_ops_be_mlo(struct dp_arch_ops *arch_ops)
{
}

void dp_soc_initialize_cdp_cmn_mlo_ops(struct dp_soc *soc)
{
}
#endif /* WLAN_FEATURE_11BE_MLO */

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
#define DP_LMAC_PEER_ID_MSB_LEGACY 2
#define DP_LMAC_PEER_ID_MSB_MLO 3

static void dp_peer_get_reo_hash_be(struct dp_vdev *vdev,
				    struct cdp_peer_setup_info *setup_info,
				    enum cdp_host_reo_dest_ring *reo_dest,
				    bool *hash_based,
				    uint8_t *lmac_peer_id_msb)
{
	struct dp_soc *soc = vdev->pdev->soc;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!be_soc->mlo_enabled)
		return dp_vdev_get_default_reo_hash(vdev, reo_dest,
						    hash_based);

	*hash_based = wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx);
	*reo_dest = vdev->pdev->reo_dest;

	/* Not a ML link peer use non-mlo */
	if (!setup_info) {
		*lmac_peer_id_msb = DP_LMAC_PEER_ID_MSB_LEGACY;
		return;
	}

	/* For STA ML VAP we do not have num links info at this point
	 * use MLO case always
	 */
	if (vdev->opmode == wlan_op_mode_sta) {
		*lmac_peer_id_msb = DP_LMAC_PEER_ID_MSB_MLO;
		return;
	}

	/* For AP ML VAP consider the peer as ML only it associates with
	 * multiple links
	 */
	if (setup_info->num_links == 1) {
		*lmac_peer_id_msb = DP_LMAC_PEER_ID_MSB_LEGACY;
		return;
	}

	*lmac_peer_id_msb = DP_LMAC_PEER_ID_MSB_MLO;
}

static bool dp_reo_remap_config_be(struct dp_soc *soc,
				   uint32_t *remap0,
				   uint32_t *remap1,
				   uint32_t *remap2)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	uint32_t reo_config = wlan_cfg_get_reo_rings_mapping(soc->wlan_cfg_ctx);
	uint32_t reo_mlo_config =
		wlan_cfg_mlo_rx_ring_map_get(soc->wlan_cfg_ctx);

	if (!be_soc->mlo_enabled)
		return dp_reo_remap_config(soc, remap0, remap1, remap2);

	*remap0 = hal_reo_ix_remap_value_get_be(soc->hal_soc, reo_mlo_config);
	*remap1 = hal_reo_ix_remap_value_get_be(soc->hal_soc, reo_config);
	*remap2 = hal_reo_ix_remap_value_get_be(soc->hal_soc, reo_mlo_config);

	return true;
}
#else
static void dp_peer_get_reo_hash_be(struct dp_vdev *vdev,
				    struct cdp_peer_setup_info *setup_info,
				    enum cdp_host_reo_dest_ring *reo_dest,
				    bool *hash_based,
				    uint8_t *lmac_peer_id_msb)
{
	dp_vdev_get_default_reo_hash(vdev, reo_dest, hash_based);
}

static bool dp_reo_remap_config_be(struct dp_soc *soc,
				   uint32_t *remap0,
				   uint32_t *remap1,
				   uint32_t *remap2)
{
	return dp_reo_remap_config(soc, remap0, remap1, remap2);
}
#endif

#ifdef CONFIG_MLO_SINGLE_DEV
static inline
void dp_initialize_arch_ops_be_single_dev(struct dp_arch_ops *arch_ops)
{
	arch_ops->dp_tx_mlo_mcast_send = dp_tx_mlo_mcast_send_be;
}
#else
static inline
void dp_initialize_arch_ops_be_single_dev(struct dp_arch_ops *arch_ops)
{
}
#endif

#ifdef IPA_OFFLOAD
static int8_t dp_ipa_get_bank_id_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	return be_soc->ipa_bank_id;
}

#ifdef QCA_IPA_LL_TX_FLOW_CONTROL
static void dp_ipa_get_wdi_version_be(uint8_t *wdi_ver)
{
	*wdi_ver = IPA_WDI_4;
}
#else
static inline void dp_ipa_get_wdi_version_be(uint8_t *wdi_ver)
{
}
#endif

static inline void dp_initialize_arch_ops_be_ipa(struct dp_arch_ops *arch_ops)
{
	arch_ops->ipa_get_bank_id = dp_ipa_get_bank_id_be;
	arch_ops->ipa_get_wdi_ver = dp_ipa_get_wdi_version_be;
}
#else /* !IPA_OFFLOAD */
static inline void dp_initialize_arch_ops_be_ipa(struct dp_arch_ops *arch_ops)
{
}
#endif /* IPA_OFFLOAD */

void dp_initialize_arch_ops_be(struct dp_arch_ops *arch_ops)
{
#ifndef QCA_HOST_MODE_WIFI_DISABLED
	arch_ops->tx_hw_enqueue = dp_tx_hw_enqueue_be;
	arch_ops->dp_rx_process = dp_rx_process_be;
	arch_ops->dp_tx_send_fast = dp_tx_fast_send_be;
	arch_ops->tx_comp_get_params_from_hal_desc =
		dp_tx_comp_get_params_from_hal_desc_be;
	arch_ops->dp_tx_process_htt_completion =
				dp_tx_process_htt_completion_be;
	arch_ops->dp_tx_desc_pool_alloc = dp_tx_desc_pool_alloc_be;
	arch_ops->dp_tx_desc_pool_free = dp_tx_desc_pool_free_be;
	arch_ops->dp_tx_desc_pool_init = dp_tx_desc_pool_init_be;
	arch_ops->dp_tx_desc_pool_deinit = dp_tx_desc_pool_deinit_be;
	arch_ops->dp_rx_desc_pool_init = dp_rx_desc_pool_init_be;
	arch_ops->dp_rx_desc_pool_deinit = dp_rx_desc_pool_deinit_be;
	arch_ops->dp_wbm_get_rx_desc_from_hal_desc =
				dp_wbm_get_rx_desc_from_hal_desc_be;
	arch_ops->dp_tx_compute_hw_delay = dp_tx_compute_tx_delay_be;
	arch_ops->dp_rx_wbm_err_reap_desc = dp_rx_wbm_err_reap_desc_be;
	arch_ops->dp_rx_null_q_desc_handle = dp_rx_null_q_desc_handle_be;
#endif
	arch_ops->txrx_get_context_size = dp_get_context_size_be;
#ifdef WIFI_MONITOR_SUPPORT
	arch_ops->txrx_get_mon_context_size = dp_mon_get_context_size_be;
#endif
	arch_ops->dp_rx_desc_cookie_2_va =
			dp_rx_desc_cookie_2_va_be;
	arch_ops->dp_rx_intrabss_mcast_handler =
				dp_rx_intrabss_mcast_handler_be;
	arch_ops->dp_rx_word_mask_subscribe = dp_rx_word_mask_subscribe_be;

	arch_ops->txrx_soc_attach = dp_soc_attach_be;
	arch_ops->txrx_soc_detach = dp_soc_detach_be;
	arch_ops->txrx_soc_init = dp_soc_init_be;
	arch_ops->txrx_soc_deinit = dp_soc_deinit_be_wrapper;
	arch_ops->txrx_soc_srng_alloc = dp_soc_srng_alloc_be;
	arch_ops->txrx_soc_srng_init = dp_soc_srng_init_be;
	arch_ops->txrx_soc_srng_deinit = dp_soc_srng_deinit_be;
	arch_ops->txrx_soc_srng_free = dp_soc_srng_free_be;
	arch_ops->txrx_pdev_attach = dp_pdev_attach_be;
	arch_ops->txrx_pdev_detach = dp_pdev_detach_be;
	arch_ops->txrx_vdev_attach = dp_vdev_attach_be;
	arch_ops->txrx_vdev_detach = dp_vdev_detach_be;
	arch_ops->txrx_peer_setup = dp_peer_setup_be;
	arch_ops->txrx_peer_map_attach = dp_peer_map_attach_be;
	arch_ops->txrx_peer_map_detach = dp_peer_map_detach_be;
	arch_ops->dp_rxdma_ring_sel_cfg = dp_rxdma_ring_sel_cfg_be;
	arch_ops->dp_rx_peer_metadata_peer_id_get =
					dp_rx_peer_metadata_peer_id_get_be;
	arch_ops->soc_cfg_attach = dp_soc_cfg_attach_be;
	arch_ops->tx_implicit_rbm_set = dp_tx_implicit_rbm_set_be;
	arch_ops->txrx_set_vdev_param = dp_txrx_set_vdev_param_be;
	dp_initialize_arch_ops_be_mlo(arch_ops);
	arch_ops->dp_soc_get_num_soc = dp_soc_get_num_soc_be;
	arch_ops->dp_peer_rx_reorder_queue_setup =
					dp_peer_rx_reorder_queue_setup_be;
	arch_ops->dp_rx_peer_set_link_id = dp_rx_set_link_id_be;
	arch_ops->txrx_print_peer_stats = dp_print_peer_txrx_stats_be;
#if defined(DP_UMAC_HW_HARD_RESET) && defined(DP_UMAC_HW_RESET_SUPPORT)
	arch_ops->dp_bank_reconfig = dp_bank_reconfig_be;
	arch_ops->dp_reconfig_tx_vdev_mcast_ctrl =
					dp_reconfig_tx_vdev_mcast_ctrl_be;
	arch_ops->dp_cc_reg_cfg_init = dp_cc_reg_cfg_init;
#endif

#ifdef WLAN_SUPPORT_PPEDS
	arch_ops->ppeds_handle_attached = dp_ppeds_handle_attached;
	arch_ops->dp_txrx_ppeds_rings_status = dp_ppeds_rings_status;
	arch_ops->txrx_soc_ppeds_start = dp_ppeds_start_soc_be;
	arch_ops->txrx_soc_ppeds_stop = dp_ppeds_stop_soc_be;
	arch_ops->dp_register_ppeds_interrupts = dp_register_ppeds_interrupts;
	arch_ops->dp_free_ppeds_interrupts = dp_free_ppeds_interrupts;
	arch_ops->dp_tx_ppeds_inuse_desc = dp_ppeds_inuse_desc;
	arch_ops->dp_ppeds_clear_stats = dp_ppeds_clear_stats;
	arch_ops->dp_txrx_ppeds_rings_stats = dp_ppeds_rings_stats;
	arch_ops->dp_txrx_ppeds_clear_rings_stats = dp_ppeds_clear_rings_stats;
	arch_ops->dp_tx_ppeds_cfg_astidx_cache_mapping =
				dp_tx_ppeds_cfg_astidx_cache_mapping;
#ifdef DP_UMAC_HW_RESET_SUPPORT
	arch_ops->txrx_soc_ppeds_interrupt_stop = dp_ppeds_interrupt_stop_be;
	arch_ops->txrx_soc_ppeds_interrupt_start = dp_ppeds_interrupt_start_be;
	arch_ops->txrx_soc_ppeds_service_status_update =
					dp_ppeds_service_status_update_be;
	arch_ops->txrx_soc_ppeds_enabled_check = dp_ppeds_is_enabled_on_soc;
	arch_ops->txrx_soc_ppeds_txdesc_pool_reset =
					dp_ppeds_tx_desc_pool_reset;
#endif
#endif
	dp_init_near_full_arch_ops_be(arch_ops);
	arch_ops->get_reo_qdesc_addr = dp_rx_get_reo_qdesc_addr_be;
	arch_ops->get_rx_hash_key = dp_get_rx_hash_key_be;
	arch_ops->dp_set_rx_fst = dp_set_rx_fst_be;
	arch_ops->dp_get_rx_fst = dp_get_rx_fst_be;
	arch_ops->dp_rx_fst_deref = dp_rx_fst_release_ref_be;
	arch_ops->dp_rx_fst_ref = dp_rx_fst_get_ref_be;
	arch_ops->print_mlo_ast_stats = dp_print_mlo_ast_stats_be;
	arch_ops->peer_get_reo_hash = dp_peer_get_reo_hash_be;
	arch_ops->reo_remap_config = dp_reo_remap_config_be;
	arch_ops->txrx_get_vdev_mcast_param = dp_txrx_get_vdev_mcast_param_be;
	arch_ops->txrx_srng_init = dp_srng_init_be;
	arch_ops->dp_get_vdev_stats_for_unmap_peer =
					dp_get_vdev_stats_for_unmap_peer_be;
#if defined(DP_POWER_SAVE) || defined(FEATURE_RUNTIME_PM)
	arch_ops->dp_update_ring_hptp = dp_update_ring_hptp;
#endif
	arch_ops->dp_flush_tx_ring = dp_flush_tcl_ring;
	arch_ops->dp_soc_interrupt_attach = dp_soc_interrupt_attach_be;
	arch_ops->dp_soc_attach_poll = dp_soc_attach_poll_be;
	arch_ops->dp_soc_interrupt_detach = dp_soc_interrupt_detach_be;
	arch_ops->dp_service_srngs = dp_service_srngs_be;
	dp_initialize_arch_ops_be_ipa(arch_ops);
	dp_initialize_arch_ops_be_single_dev(arch_ops);
	dp_initialize_arch_ops_be_fisa(arch_ops);
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
static void
dp_primary_link_migration(struct dp_soc *soc, void *cb_ctxt,
			  union hal_reo_status *reo_status)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *dp_mlo = be_soc->ml_ctxt;
	struct dp_soc *pr_soc = NULL;
	struct dp_peer_info *pr_peer_info = (struct dp_peer_info *)cb_ctxt;
	struct dp_peer *new_primary_peer = NULL;
	struct dp_peer *mld_peer = NULL;
	uint8_t primary_vdev_id;
	struct cdp_txrx_peer_params_update params = {0};
	uint8_t tid;
	uint8_t is_wds = 0;
	uint16_t hw_peer_id;
	uint16_t ast_hash;

	pr_soc = dp_mlo_get_soc_ref_by_chip_id(dp_mlo, pr_peer_info->chip_id);
	if (!pr_soc) {
		dp_htt_err("Invalid soc");
		qdf_mem_free(pr_peer_info);
		return;
	}

	new_primary_peer = pr_soc->peer_id_to_obj_map[
				pr_peer_info->primary_peer_id];
	if (!new_primary_peer) {
		dp_htt_err("New primary peer is NULL");
		qdf_mem_free(pr_peer_info);
		return;
	}

	mld_peer = DP_GET_MLD_PEER_FROM_PEER(new_primary_peer);
	if (!mld_peer) {
		dp_htt_err("MLD peer is NULL");
		qdf_mem_free(pr_peer_info);
		return;
	}

	new_primary_peer->primary_link = 1;

	hw_peer_id = pr_peer_info->hw_peer_id;
	ast_hash = pr_peer_info->ast_hash;
	/* Add ast enteries for new primary peer */
	if (pr_soc->ast_offload_support && pr_soc->host_ast_db_enable) {
		dp_peer_host_add_map_ast(pr_soc, mld_peer->peer_id, mld_peer->mac_addr.raw,
					 hw_peer_id, new_primary_peer->vdev->vdev_id,
					 ast_hash, is_wds);
	}

	/*
	 * Check if reo_qref_table_en is set and if
	 * rx_tid qdesc for tid 0 is already setup and perform
	 * qref write to LUT for Tid 0 and 16.
	 *
	 */
	if (hal_reo_shared_qaddr_is_enable(pr_soc->hal_soc) &&
	    mld_peer->rx_tid[0].hw_qdesc_vaddr_unaligned) {
		for (tid = 0; tid < DP_MAX_TIDS; tid++)
			hal_reo_shared_qaddr_write(pr_soc->hal_soc,
						   mld_peer->peer_id,
						   tid,
						   mld_peer->rx_tid[tid].hw_qdesc_paddr);
	}

	if (pr_soc && pr_soc->cdp_soc.ol_ops->update_primary_link)
		pr_soc->cdp_soc.ol_ops->update_primary_link(pr_soc->ctrl_psoc,
						new_primary_peer->mac_addr.raw);

	primary_vdev_id = new_primary_peer->vdev->vdev_id;

	dp_vdev_unref_delete(soc, mld_peer->vdev, DP_MOD_ID_CHILD);
	mld_peer->vdev = dp_vdev_get_ref_by_id(pr_soc, primary_vdev_id,
			 DP_MOD_ID_CHILD);
	mld_peer->txrx_peer->vdev = mld_peer->vdev;

	params.vdev_id = new_primary_peer->vdev->vdev_id;
	params.peer_mac = mld_peer->mac_addr.raw;
	params.chip_id = pr_peer_info->chip_id;
	params.pdev_id = new_primary_peer->vdev->pdev->pdev_id;

	if (new_primary_peer->vdev->opmode == wlan_op_mode_sta) {
		dp_wdi_event_handler(
				WDI_EVENT_STA_PRIMARY_UMAC_UPDATE,
				pr_soc, (void *)&params,
				new_primary_peer->peer_id,
				WDI_NO_VAL, params.pdev_id);
	} else {
		dp_wdi_event_handler(
				WDI_EVENT_PEER_PRIMARY_UMAC_UPDATE,
				pr_soc, (void *)&params,
				new_primary_peer->peer_id,
				WDI_NO_VAL, params.pdev_id);
	}
	qdf_mem_free(pr_peer_info);
}

#ifdef WLAN_SUPPORT_PPEDS
static QDF_STATUS dp_get_ppe_info_for_vap(struct dp_soc *pr_soc,
					  struct dp_peer *pr_peer,
					  uint16_t *src_info)
{
	struct dp_soc_be *be_soc_mld = NULL;
	struct cdp_ds_vp_params vp_params = {0};
	struct dp_ppe_vp_profile *ppe_vp_profile;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct cdp_soc_t *cdp_soc = &pr_soc->cdp_soc;

	/*
	 * Extract the VP profile from the VAP
	 */
	if (!cdp_soc->ol_ops->get_ppeds_profile_info_for_vap) {
		dp_err("%pK: Register get ppeds profile info first", cdp_soc);
		return QDF_STATUS_E_NULL_VALUE;
	}

	/*
	 * Check if PPE DS routing is enabled on the associated vap.
	 */
	qdf_status = cdp_soc->ol_ops->get_ppeds_profile_info_for_vap(
							pr_soc->ctrl_psoc,
							pr_peer->vdev->vdev_id,
							&vp_params);

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		dp_err("Could not find ppeds profile info");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* Check if PPE DS routing is enabled on
	 * the associated vap.
	 */
	if (vp_params.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return qdf_status;

	be_soc_mld = dp_get_be_soc_from_dp_soc(pr_soc);
	ppe_vp_profile = &be_soc_mld->ppe_vp_profile[
				vp_params.ppe_vp_profile_idx];
	*src_info = ppe_vp_profile->vp_num;

	return qdf_status;
}
#else
static QDF_STATUS dp_get_ppe_info_for_vap(struct dp_soc *pr_soc,
					  struct dp_peer *pr_peer,
					  uint16_t *src_info)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

QDF_STATUS dp_htt_reo_migration(struct dp_soc *soc, uint16_t peer_id,
				uint16_t ml_peer_id, uint16_t vdev_id,
				uint8_t pdev_id, uint8_t chip_id)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *dp_mlo = be_soc->ml_ctxt;
	uint16_t mld_peer_id = dp_gen_ml_peer_id(soc, ml_peer_id);
	struct dp_soc *pr_soc = NULL;
	struct dp_soc *current_pr_soc = NULL;
	struct hal_reo_cmd_params params;
	struct dp_rx_tid *rx_tid;
	struct dp_peer *pr_peer = NULL;
	struct dp_peer *mld_peer = NULL;
	struct dp_soc *mld_soc = NULL;
	struct dp_peer *current_pr_peer = NULL;
	struct dp_peer_info *peer_info;
	struct dp_vdev_be *be_vdev;
	uint16_t src_info = 0;
	QDF_STATUS status;
	struct dp_ast_entry *ast_entry;
	uint16_t hw_peer_id;
	uint16_t ast_hash;
	int i = 0;

	if (!dp_mlo) {
		dp_htt_err("Invalid dp_mlo ctxt");
		return QDF_STATUS_E_FAILURE;
	}

	pr_soc = dp_mlo_get_soc_ref_by_chip_id(dp_mlo, chip_id);
	if (!pr_soc) {
		dp_htt_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	pr_peer = pr_soc->peer_id_to_obj_map[peer_id];
	if (!pr_peer || !(IS_MLO_DP_LINK_PEER(pr_peer))) {
		dp_htt_err("Invalid peer");
		return QDF_STATUS_E_FAILURE;
	}

	mld_peer = DP_GET_MLD_PEER_FROM_PEER(pr_peer);

	if (!mld_peer || (mld_peer->peer_id != mld_peer_id)) {
		dp_htt_err("Invalid mld peer");
		return QDF_STATUS_E_FAILURE;
	}

	be_vdev = dp_get_be_vdev_from_dp_vdev(pr_peer->vdev);
	if (!be_vdev) {
		dp_htt_err("Invalid be vdev");
		return QDF_STATUS_E_FAILURE;
	}

	mld_soc = mld_peer->vdev->pdev->soc;
	status = dp_get_ppe_info_for_vap(pr_soc, pr_peer, &src_info);
	if (status == QDF_STATUS_E_NULL_VALUE) {
		dp_htt_err("Invalid ppe info for the vdev");
		return QDF_STATUS_E_FAILURE;
	}

	current_pr_peer = dp_get_primary_link_peer_by_id(
						pr_soc,
						mld_peer->peer_id,
						DP_MOD_ID_HTT);
	/* Making existing primary peer as non primary */
	if (current_pr_peer) {
		current_pr_peer->primary_link = 0;
		dp_peer_unref_delete(current_pr_peer, DP_MOD_ID_HTT);
	}

	current_pr_soc = mld_peer->vdev->pdev->soc;
	dp_peer_rx_reo_shared_qaddr_delete(current_pr_soc, mld_peer);

	/* delete ast entry for current primary peer */
	qdf_spin_lock_bh(&current_pr_soc->ast_lock);
	ast_entry = dp_peer_ast_hash_find_soc_by_type(
				current_pr_soc,
				mld_peer->mac_addr.raw,
				CDP_TXRX_AST_TYPE_MLD);
	if (!ast_entry) {
		dp_htt_err("Invalid ast entry");
		qdf_spin_unlock_bh(&current_pr_soc->ast_lock);
		return QDF_STATUS_E_FAILURE;
	}

	hw_peer_id = ast_entry->ast_idx;
	ast_hash = ast_entry->ast_hash_value;
	dp_peer_unlink_ast_entry(current_pr_soc, ast_entry, mld_peer);

	if (ast_entry->is_mapped)
		current_pr_soc->ast_table[ast_entry->ast_idx] = NULL;

	dp_peer_free_ast_entry(current_pr_soc, ast_entry);

	mld_peer->self_ast_entry = NULL;
	qdf_spin_unlock_bh(&current_pr_soc->ast_lock);

	peer_info = qdf_mem_malloc(sizeof(struct dp_peer_info));
	if (!peer_info) {
		dp_htt_err("Malloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	peer_info->primary_peer_id = peer_id;
	peer_info->chip_id = chip_id;
	peer_info->hw_peer_id = hw_peer_id;
	peer_info->ast_hash = ast_hash;


	for (i = 0; i < DP_MAX_TIDS; i++) {
		rx_tid = &mld_peer->rx_tid[i];
		if (!rx_tid)
			continue;

		qdf_mem_zero(&params, sizeof(params));
		params.std.need_status = 1;
		params.std.addr_lo = rx_tid->hw_qdesc_paddr & 0xffffffff;
		params.std.addr_hi = (uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;

		status = dp_reo_send_cmd(current_pr_soc, CMD_FLUSH_QUEUE, &params,
					 NULL,NULL);
	}

	qdf_mem_zero(&params, sizeof(params));

	rx_tid = &mld_peer->rx_tid[0];
	params.std.need_status = 1;
	params.std.addr_lo = rx_tid->hw_qdesc_paddr & 0xffffffff;
	params.std.addr_hi = (uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
	params.u.fl_cache_params.flush_no_inval = 0;
	params.u.fl_cache_params.flush_entire_cache = 1;
	status = dp_reo_send_cmd(current_pr_soc, CMD_FLUSH_CACHE, &params,
				 dp_primary_link_migration,
				 (void *)peer_info);

	if (status != QDF_STATUS_SUCCESS) {
		dp_htt_err("Reo flush failed");
		qdf_mem_free(peer_info);
		dp_h2t_ptqm_migration_msg_send(pr_soc, vdev_id, pdev_id,
					       chip_id, peer_id, ml_peer_id,
					       src_info, QDF_STATUS_E_FAILURE);
	}

	qdf_mem_zero(&params, sizeof(params));
	params.std.need_status = 0;
	params.std.addr_lo = rx_tid->hw_qdesc_paddr & 0xffffffff;
	params.std.addr_hi = (uint64_t)(rx_tid->hw_qdesc_paddr) >> 32;
	params.u.unblk_cache_params.type = UNBLOCK_CACHE;
	dp_reo_send_cmd(current_pr_soc, CMD_UNBLOCK_CACHE, &params, NULL, NULL);

	dp_h2t_ptqm_migration_msg_send(pr_soc, vdev_id, pdev_id,
				       chip_id, peer_id, ml_peer_id,
				       src_info, QDF_STATUS_SUCCESS);
	return QDF_STATUS_SUCCESS;
}
#endif
