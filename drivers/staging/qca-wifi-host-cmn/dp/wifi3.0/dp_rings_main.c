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

#include <wlan_ipa_obj_mgmt_api.h>
#include <qdf_types.h>
#include <qdf_lock.h>
#include <qdf_net_types.h>
#include <qdf_lro.h>
#include <qdf_module.h>
#include <hal_hw_headers.h>
#include <hal_api.h>
#include <hif.h>
#include <htt.h>
#include <wdi_event.h>
#include <queue.h>
#include "dp_types.h"
#include "dp_rings.h"
#include "dp_internal.h"
#include "dp_tx.h"
#include "dp_tx_desc.h"
#include "dp_rx.h"
#ifdef DP_RATETABLE_SUPPORT
#include "dp_ratetable.h"
#endif
#include <cdp_txrx_handle.h>
#include <wlan_cfg.h>
#include <wlan_utility.h>
#include "cdp_txrx_cmn_struct.h"
#include "cdp_txrx_stats_struct.h"
#include "cdp_txrx_cmn_reg.h"
#include <qdf_util.h>
#include "dp_peer.h"
#include "htt_stats.h"
#include "dp_htt.h"
#include "htt_ppdu_stats.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "cfg_ucfg_api.h"
#include <wlan_module_ids.h>

#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif
#include "qdf_ssr_driver_dump.h"

#ifdef WLAN_FEATURE_STATS_EXT
#define INIT_RX_HW_STATS_LOCK(_soc) \
	qdf_spinlock_create(&(_soc)->rx_hw_stats_lock)
#define DEINIT_RX_HW_STATS_LOCK(_soc) \
	qdf_spinlock_destroy(&(_soc)->rx_hw_stats_lock)
#else
#define INIT_RX_HW_STATS_LOCK(_soc)  /* no op */
#define DEINIT_RX_HW_STATS_LOCK(_soc) /* no op */
#endif

static QDF_STATUS dp_init_tx_ring_pair_by_index(struct dp_soc *soc,
						uint8_t index);
static void dp_deinit_tx_pair_by_index(struct dp_soc *soc, int index);
static void dp_free_tx_ring_pair_by_index(struct dp_soc *soc, uint8_t index);
static QDF_STATUS dp_alloc_tx_ring_pair_by_index(struct dp_soc *soc,
						 uint8_t index);

/* default_dscp_tid_map - Default DSCP-TID mapping
 *
 * DSCP        TID
 * 000000      0
 * 001000      1
 * 010000      2
 * 011000      3
 * 100000      4
 * 101000      5
 * 110000      6
 * 111000      7
 */
static uint8_t default_dscp_tid_map[DSCP_TID_MAP_MAX] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7,
};

/* default_pcp_tid_map - Default PCP-TID mapping
 *
 * PCP     TID
 * 000      0
 * 001      1
 * 010      2
 * 011      3
 * 100      4
 * 101      5
 * 110      6
 * 111      7
 */
static uint8_t default_pcp_tid_map[PCP_TID_MAP_MAX] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};

uint8_t
dp_cpu_ring_map[DP_NSS_CPU_RING_MAP_MAX][WLAN_CFG_INT_NUM_CONTEXTS_MAX] = {
	{0x0, 0x1, 0x2, 0x0, 0x0, 0x1, 0x2, 0x0, 0x0, 0x1, 0x2},
	{0x1, 0x2, 0x1, 0x2, 0x1, 0x2, 0x1, 0x2, 0x1, 0x2, 0x1},
	{0x0, 0x2, 0x0, 0x2, 0x0, 0x2, 0x0, 0x2, 0x0, 0x2, 0x0},
	{0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2},
	{0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3},
#ifdef WLAN_TX_PKT_CAPTURE_ENH
	{0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1}
#endif
};

qdf_export_symbol(dp_cpu_ring_map);

/**
 * dp_soc_ring_if_nss_offloaded() - find if ring is offloaded to NSS
 * @soc: DP soc handle
 * @ring_type: ring type
 * @ring_num: ring_num
 *
 * Return: 0 if the ring is not offloaded, non-0 if it is offloaded
 */
static uint8_t dp_soc_ring_if_nss_offloaded(struct dp_soc *soc,
					    enum hal_ring_type ring_type,
					    int ring_num)
{
	uint8_t nss_config = wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx);
	uint8_t status = 0;

	switch (ring_type) {
	case WBM2SW_RELEASE:
	case REO_DST:
	case RXDMA_BUF:
	case REO_EXCEPTION:
		status = ((nss_config) & (1 << ring_num));
		break;
	default:
		break;
	}

	return status;
}

#if !defined(DP_CON_MON)
void dp_soc_reset_mon_intr_mask(struct dp_soc *soc)
{
	int i;

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		soc->intr_ctx[i].rx_mon_ring_mask = 0;
		soc->intr_ctx[i].host2rxdma_mon_ring_mask = 0;
	}
}

qdf_export_symbol(dp_soc_reset_mon_intr_mask);

void dp_service_lmac_rings(void *arg)
{
	struct dp_soc *soc = (struct dp_soc *)arg;
	int ring = 0, i;
	struct dp_pdev *pdev = NULL;
	union dp_rx_desc_list_elem_t *desc_list = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;

	/* Process LMAC interrupts */
	for  (ring = 0 ; ring < MAX_NUM_LMAC_HW; ring++) {
		int mac_for_pdev = ring;
		struct dp_srng *rx_refill_buf_ring;

		pdev = dp_get_pdev_for_lmac_id(soc, mac_for_pdev);
		if (!pdev)
			continue;

		rx_refill_buf_ring = &soc->rx_refill_buf_ring[mac_for_pdev];

		dp_monitor_process(soc, NULL, mac_for_pdev,
				   QCA_NAPI_BUDGET);

		for (i = 0;
		     i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++)
			dp_rxdma_err_process(&soc->intr_ctx[i], soc,
					     mac_for_pdev,
					     QCA_NAPI_BUDGET);

		if (!dp_soc_ring_if_nss_offloaded(soc, RXDMA_BUF,
						  mac_for_pdev))
			dp_rx_buffers_replenish(soc, mac_for_pdev,
						rx_refill_buf_ring,
						&soc->rx_desc_buf[mac_for_pdev],
						0, &desc_list, &tail, false);
	}

	qdf_timer_mod(&soc->lmac_reap_timer, DP_INTR_POLL_TIMER_MS);
}

#endif

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * dp_is_reo_ring_num_in_nf_grp1() - Check if the current reo ring is part of
 *				rx_near_full_grp1 mask
 * @soc: Datapath SoC Handle
 * @ring_num: REO ring number
 *
 * Return: 1 if the ring_num belongs to reo_nf_grp1,
 *	   0, otherwise.
 */
static inline int
dp_is_reo_ring_num_in_nf_grp1(struct dp_soc *soc, int ring_num)
{
	return (WLAN_CFG_RX_NEAR_FULL_IRQ_MASK_1 & (1 << ring_num));
}

/**
 * dp_is_reo_ring_num_in_nf_grp2() - Check if the current reo ring is part of
 *				rx_near_full_grp2 mask
 * @soc: Datapath SoC Handle
 * @ring_num: REO ring number
 *
 * Return: 1 if the ring_num belongs to reo_nf_grp2,
 *	   0, otherwise.
 */
static inline int
dp_is_reo_ring_num_in_nf_grp2(struct dp_soc *soc, int ring_num)
{
	return (WLAN_CFG_RX_NEAR_FULL_IRQ_MASK_2 & (1 << ring_num));
}

/**
 * dp_srng_get_near_full_irq_mask() - Get near-full irq mask for a particular
 *				ring type and number
 * @soc: Datapath SoC handle
 * @ring_type: SRNG type
 * @ring_num: ring num
 *
 * Return: near-full irq mask pointer
 */
uint8_t *dp_srng_get_near_full_irq_mask(struct dp_soc *soc,
					enum hal_ring_type ring_type,
					int ring_num)
{
	struct wlan_cfg_dp_soc_ctxt *cfg_ctx = soc->wlan_cfg_ctx;
	uint8_t wbm2_sw_rx_rel_ring_id;
	uint8_t *nf_irq_mask = NULL;

	switch (ring_type) {
	case WBM2SW_RELEASE:
		wbm2_sw_rx_rel_ring_id =
			wlan_cfg_get_rx_rel_ring_id(cfg_ctx);
		if (ring_num != wbm2_sw_rx_rel_ring_id) {
			nf_irq_mask = &soc->wlan_cfg_ctx->
					int_tx_ring_near_full_irq_mask[0];
		}
		break;
	case REO_DST:
		if (dp_is_reo_ring_num_in_nf_grp1(soc, ring_num))
			nf_irq_mask =
			&soc->wlan_cfg_ctx->int_rx_ring_near_full_irq_1_mask[0];
		else if (dp_is_reo_ring_num_in_nf_grp2(soc, ring_num))
			nf_irq_mask =
			&soc->wlan_cfg_ctx->int_rx_ring_near_full_irq_2_mask[0];
		else
			qdf_assert(0);
		break;
	default:
		break;
	}

	return nf_irq_mask;
}

/**
 * dp_srng_set_msi2_ring_params() - Set the msi2 addr/data in the ring params
 * @soc: Datapath SoC handle
 * @ring_params: srng params handle
 * @msi2_addr: MSI2 addr to be set for the SRNG
 * @msi2_data: MSI2 data to be set for the SRNG
 *
 * Return: None
 */
void dp_srng_set_msi2_ring_params(struct dp_soc *soc,
				  struct hal_srng_params *ring_params,
				  qdf_dma_addr_t msi2_addr,
				  uint32_t msi2_data)
{
	ring_params->msi2_addr = msi2_addr;
	ring_params->msi2_data = msi2_data;
}

/**
 * dp_srng_msi2_setup() - Setup MSI2 details for near full IRQ of an SRNG
 * @soc: Datapath SoC handle
 * @ring_params: ring_params for SRNG
 * @ring_type: SENG type
 * @ring_num: ring number for the SRNG
 * @nf_msi_grp_num: near full msi group number
 *
 * Return: None
 */
void dp_srng_msi2_setup(struct dp_soc *soc,
			struct hal_srng_params *ring_params,
			int ring_type, int ring_num, int nf_msi_grp_num)
{
	uint32_t msi_data_start, msi_irq_start, addr_low, addr_high;
	int msi_data_count, ret;

	ret = pld_get_user_msi_assignment(soc->osdev->dev, "DP",
					  &msi_data_count, &msi_data_start,
					  &msi_irq_start);
	if (ret)
		return;

	if (nf_msi_grp_num < 0) {
		dp_init_info("%pK: ring near full IRQ not part of an ext_group; ring_type: %d,ring_num %d",
			     soc, ring_type, ring_num);
		ring_params->msi2_addr = 0;
		ring_params->msi2_data = 0;
		return;
	}

	if (dp_is_msi_group_number_invalid(soc, nf_msi_grp_num,
					   msi_data_count)) {
		dp_init_warn("%pK: 2 msi_groups will share an msi for near full IRQ; msi_group_num %d",
			     soc, nf_msi_grp_num);
		QDF_ASSERT(0);
	}

	pld_get_msi_address(soc->osdev->dev, &addr_low, &addr_high);

	ring_params->nf_irq_support = 1;
	ring_params->msi2_addr = addr_low;
	ring_params->msi2_addr |= (qdf_dma_addr_t)(((uint64_t)addr_high) << 32);
	ring_params->msi2_data = (nf_msi_grp_num % msi_data_count)
		+ msi_data_start;
	ring_params->flags |= HAL_SRNG_MSI_INTR;
}

/* Percentage of ring entries considered as nearly full */
#define DP_NF_HIGH_THRESH_PERCENTAGE	75
/* Percentage of ring entries considered as critically full */
#define DP_NF_CRIT_THRESH_PERCENTAGE	90
/* Percentage of ring entries considered as safe threshold */
#define DP_NF_SAFE_THRESH_PERCENTAGE	50

/**
 * dp_srng_configure_nf_interrupt_thresholds() - Configure the thresholds for
 *			near full irq
 * @soc: Datapath SoC handle
 * @ring_params: ring params for SRNG
 * @ring_type: ring type
 */
void
dp_srng_configure_nf_interrupt_thresholds(struct dp_soc *soc,
					  struct hal_srng_params *ring_params,
					  int ring_type)
{
	if (ring_params->nf_irq_support) {
		ring_params->high_thresh = (ring_params->num_entries *
					    DP_NF_HIGH_THRESH_PERCENTAGE) / 100;
		ring_params->crit_thresh = (ring_params->num_entries *
					    DP_NF_CRIT_THRESH_PERCENTAGE) / 100;
		ring_params->safe_thresh = (ring_params->num_entries *
					    DP_NF_SAFE_THRESH_PERCENTAGE) /100;
	}
}

/**
 * dp_srng_set_nf_thresholds() - Set the near full thresholds to srng data
 *			structure from the ring params
 * @soc: Datapath SoC handle
 * @srng: SRNG handle
 * @ring_params: ring params for a SRNG
 *
 * Return: None
 */
static inline void
dp_srng_set_nf_thresholds(struct dp_soc *soc, struct dp_srng *srng,
			  struct hal_srng_params *ring_params)
{
	srng->crit_thresh = ring_params->crit_thresh;
	srng->safe_thresh = ring_params->safe_thresh;
}

#else
static inline void
dp_srng_set_nf_thresholds(struct dp_soc *soc, struct dp_srng *srng,
			  struct hal_srng_params *ring_params)
{
}
#endif

/**
 * dp_get_num_msi_available()- API to get number of MSIs available
 * @soc: DP soc Handle
 * @interrupt_mode: Mode of interrupts
 *
 * Return: Number of MSIs available or 0 in case of integrated
 */
#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
static int dp_get_num_msi_available(struct dp_soc *soc, int interrupt_mode)
{
	return 0;
}
#else
static int dp_get_num_msi_available(struct dp_soc *soc, int interrupt_mode)
{
	int msi_data_count;
	int msi_data_start;
	int msi_irq_start;
	int ret;

	if (interrupt_mode == DP_INTR_INTEGRATED) {
		return 0;
	} else if (interrupt_mode == DP_INTR_MSI || interrupt_mode ==
		   DP_INTR_POLL) {
		ret = pld_get_user_msi_assignment(soc->osdev->dev, "DP",
						  &msi_data_count,
						  &msi_data_start,
						  &msi_irq_start);
		if (ret) {
			qdf_err("Unable to get DP MSI assignment %d",
				interrupt_mode);
			return -EINVAL;
		}
		return msi_data_count;
	}
	qdf_err("Interrupt mode invalid %d", interrupt_mode);
	return -EINVAL;
}
#endif

/**
 * dp_srng_configure_pointer_update_thresholds() - Retrieve pointer
 * update threshold value from wlan_cfg_ctx
 * @soc: device handle
 * @ring_params: per ring specific parameters
 * @ring_type: Ring type
 * @ring_num: Ring number for a given ring type
 * @num_entries: number of entries to fill
 *
 * Fill the ring params with the pointer update threshold
 * configuration parameters available in wlan_cfg_ctx
 *
 * Return: None
 */
static void
dp_srng_configure_pointer_update_thresholds(
				struct dp_soc *soc,
				struct hal_srng_params *ring_params,
				int ring_type, int ring_num,
				int num_entries)
{
	if (ring_type == REO_DST) {
		ring_params->pointer_timer_threshold =
			wlan_cfg_get_pointer_timer_threshold_rx(
						soc->wlan_cfg_ctx);
		ring_params->pointer_num_threshold =
			wlan_cfg_get_pointer_num_threshold_rx(
						soc->wlan_cfg_ctx);
	}
}

QDF_STATUS dp_srng_init_idx(struct dp_soc *soc, struct dp_srng *srng,
			    int ring_type, int ring_num, int mac_id,
			    uint32_t idx)
{
	bool idle_check;

	hal_soc_handle_t hal_soc = soc->hal_soc;
	struct hal_srng_params ring_params;

	if (srng->hal_srng) {
		dp_init_err("%pK: Ring type: %d, num:%d is already initialized",
			    soc, ring_type, ring_num);
		return QDF_STATUS_SUCCESS;
	}

	/* memset the srng ring to zero */
	qdf_mem_zero(srng->base_vaddr_unaligned, srng->alloc_size);

	qdf_mem_zero(&ring_params, sizeof(struct hal_srng_params));
	ring_params.ring_base_paddr = srng->base_paddr_aligned;
	ring_params.ring_base_vaddr = srng->base_vaddr_aligned;

	ring_params.num_entries = srng->num_entries;

	dp_info("Ring type: %d, num:%d vaddr %pK paddr %pK entries %u",
		ring_type, ring_num,
		(void *)ring_params.ring_base_vaddr,
		(void *)ring_params.ring_base_paddr,
		ring_params.num_entries);

	if (soc->intr_mode == DP_INTR_MSI && !dp_skip_msi_cfg(soc, ring_type)) {
		dp_srng_msi_setup(soc, srng, &ring_params, ring_type, ring_num);
		dp_verbose_debug("Using MSI for ring_type: %d, ring_num %d",
				 ring_type, ring_num);
	} else {
		ring_params.msi_data = 0;
		ring_params.msi_addr = 0;
		dp_srng_set_msi2_ring_params(soc, &ring_params, 0, 0);
		dp_verbose_debug("Skipping MSI for ring_type: %d, ring_num %d",
				 ring_type, ring_num);
	}

	dp_srng_configure_interrupt_thresholds(soc, &ring_params,
					       ring_type, ring_num,
					       srng->num_entries);

	dp_srng_set_nf_thresholds(soc, srng, &ring_params);
	dp_srng_configure_pointer_update_thresholds(soc, &ring_params,
						    ring_type, ring_num,
						    srng->num_entries);

	if (srng->cached)
		ring_params.flags |= HAL_SRNG_CACHED_DESC;

	idle_check = dp_check_umac_reset_in_progress(soc);

	srng->hal_srng = hal_srng_setup_idx(hal_soc, ring_type, ring_num,
					    mac_id, &ring_params, idle_check,
					    idx);

	if (!srng->hal_srng) {
		dp_srng_free(soc, srng);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(dp_srng_init_idx);

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * dp_service_near_full_srngs() - Bottom half handler to process the near
 *				full IRQ on a SRNG
 * @dp_ctx: Datapath SoC handle
 * @dp_budget: Number of SRNGs which can be processed in a single attempt
 *		without rescheduling
 * @cpu: cpu id
 *
 * Return: remaining budget/quota for the soc device
 */
static
uint32_t dp_service_near_full_srngs(void *dp_ctx, uint32_t dp_budget, int cpu)
{
	struct dp_intr *int_ctx = (struct dp_intr *)dp_ctx;
	struct dp_soc *soc = int_ctx->soc;

	/*
	 * dp_service_near_full_srngs arch ops should be initialized always
	 * if the NEAR FULL IRQ feature is enabled.
	 */
	return soc->arch_ops.dp_service_near_full_srngs(soc, int_ctx,
							dp_budget);
}
#endif

#ifndef QCA_HOST_MODE_WIFI_DISABLED

uint32_t dp_service_srngs(void *dp_ctx, uint32_t dp_budget, int cpu)
{
	struct dp_intr *int_ctx = (struct dp_intr *)dp_ctx;
	struct dp_intr_stats *intr_stats = &int_ctx->intr_stats;
	struct dp_soc *soc = int_ctx->soc;
	int ring = 0;
	int index;
	uint32_t work_done  = 0;
	int budget = dp_budget;
	uint32_t remaining_quota = dp_budget;
	uint8_t tx_mask = 0;
	uint8_t rx_mask = 0;
	uint8_t rx_err_mask = 0;
	uint8_t rx_wbm_rel_mask = 0;
	uint8_t reo_status_mask = 0;

	qdf_atomic_set_bit(cpu, &soc->service_rings_running);

	tx_mask = int_ctx->tx_ring_mask;
	rx_mask = int_ctx->rx_ring_mask;
	rx_err_mask = int_ctx->rx_err_ring_mask;
	rx_wbm_rel_mask = int_ctx->rx_wbm_rel_ring_mask;
	reo_status_mask = int_ctx->reo_status_ring_mask;

	dp_verbose_debug("tx %x rx %x rx_err %x rx_wbm_rel %x reo_status %x rx_mon_ring %x host2rxdma %x rxdma2host %x",
			 tx_mask, rx_mask, rx_err_mask, rx_wbm_rel_mask,
			 reo_status_mask,
			 int_ctx->rx_mon_ring_mask,
			 int_ctx->host2rxdma_ring_mask,
			 int_ctx->rxdma2host_ring_mask);

	/* Process Tx completion interrupts first to return back buffers */
	for (index = 0; index < soc->num_tx_comp_rings; index++) {
		if (!(1 << wlan_cfg_get_wbm_ring_num_for_index(soc->wlan_cfg_ctx, index) & tx_mask))
			continue;
		work_done = dp_tx_comp_handler(int_ctx,
					       soc,
					       soc->tx_comp_ring[index].hal_srng,
					       index, remaining_quota);
		if (work_done) {
			intr_stats->num_tx_ring_masks[index]++;
			dp_verbose_debug("tx mask 0x%x index %d, budget %d, work_done %d",
					 tx_mask, index, budget,
					 work_done);
		}
		budget -= work_done;
		if (budget <= 0)
			goto budget_done;

		remaining_quota = budget;
	}

	/* Process REO Exception ring interrupt */
	if (rx_err_mask) {
		work_done = dp_rx_err_process(int_ctx, soc,
					      soc->reo_exception_ring.hal_srng,
					      remaining_quota);

		if (work_done) {
			intr_stats->num_rx_err_ring_masks++;
			dp_verbose_debug("REO Exception Ring: work_done %d budget %d",
					 work_done, budget);
		}

		budget -=  work_done;
		if (budget <= 0) {
			goto budget_done;
		}
		remaining_quota = budget;
	}

	/* Process Rx WBM release ring interrupt */
	if (rx_wbm_rel_mask) {
		work_done = dp_rx_wbm_err_process(int_ctx, soc,
						  soc->rx_rel_ring.hal_srng,
						  remaining_quota);

		if (work_done) {
			intr_stats->num_rx_wbm_rel_ring_masks++;
			dp_verbose_debug("WBM Release Ring: work_done %d budget %d",
					 work_done, budget);
		}

		budget -=  work_done;
		if (budget <= 0) {
			goto budget_done;
		}
		remaining_quota = budget;
	}

	/* Process Rx interrupts */
	if (rx_mask) {
		for (ring = 0; ring < soc->num_reo_dest_rings; ring++) {
			if (!(rx_mask & (1 << ring)))
				continue;
			work_done = soc->arch_ops.dp_rx_process(int_ctx,
						  soc->reo_dest_ring[ring].hal_srng,
						  ring,
						  remaining_quota);
			if (work_done) {
				intr_stats->num_rx_ring_masks[ring]++;
				dp_verbose_debug("rx mask 0x%x ring %d, work_done %d budget %d",
						 rx_mask, ring,
						 work_done, budget);
				budget -=  work_done;
				if (budget <= 0)
					goto budget_done;
				remaining_quota = budget;
			}
		}
	}

	if (reo_status_mask) {
		if (dp_reo_status_ring_handler(int_ctx, soc))
			int_ctx->intr_stats.num_reo_status_ring_masks++;
	}

	if (qdf_unlikely(!dp_monitor_is_vdev_timer_running(soc))) {
		work_done = dp_process_lmac_rings(int_ctx, remaining_quota);
		if (work_done) {
			budget -=  work_done;
			if (budget <= 0)
				goto budget_done;
			remaining_quota = budget;
		}
	}

	qdf_lro_flush(int_ctx->lro_ctx);
	intr_stats->num_masks++;

budget_done:
	qdf_atomic_clear_bit(cpu, &soc->service_rings_running);

	dp_umac_reset_trigger_pre_reset_notify_cb(soc);

	return dp_budget - budget;
}

#else /* QCA_HOST_MODE_WIFI_DISABLED */

uint32_t dp_service_srngs(void *dp_ctx, uint32_t dp_budget, int cpu)
{
	struct dp_intr *int_ctx = (struct dp_intr *)dp_ctx;
	struct dp_intr_stats *intr_stats = &int_ctx->intr_stats;
	struct dp_soc *soc = int_ctx->soc;
	uint32_t remaining_quota = dp_budget;
	uint32_t work_done  = 0;
	int budget = dp_budget;
	uint8_t reo_status_mask = int_ctx->reo_status_ring_mask;

	if (reo_status_mask) {
		if (dp_reo_status_ring_handler(int_ctx, soc))
			int_ctx->intr_stats.num_reo_status_ring_masks++;
	}

	if (qdf_unlikely(!dp_monitor_is_vdev_timer_running(soc))) {
		work_done = dp_process_lmac_rings(int_ctx, remaining_quota);
		if (work_done) {
			budget -=  work_done;
			if (budget <= 0)
				goto budget_done;
			remaining_quota = budget;
		}
	}

	qdf_lro_flush(int_ctx->lro_ctx);
	intr_stats->num_masks++;

budget_done:
	return dp_budget - budget;
}

#endif /* QCA_HOST_MODE_WIFI_DISABLED */

QDF_STATUS dp_soc_attach_poll(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	int i;
	int lmac_id = 0;

	qdf_mem_set(&soc->mon_intr_id_lmac_map,
		    sizeof(soc->mon_intr_id_lmac_map), DP_MON_INVALID_LMAC_ID);
	soc->intr_mode = DP_INTR_POLL;

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		soc->intr_ctx[i].dp_intr_id = i;
		soc->intr_ctx[i].tx_ring_mask =
			wlan_cfg_get_tx_ring_mask(soc->wlan_cfg_ctx, i);
		soc->intr_ctx[i].rx_ring_mask =
			wlan_cfg_get_rx_ring_mask(soc->wlan_cfg_ctx, i);
		soc->intr_ctx[i].rx_mon_ring_mask =
			wlan_cfg_get_rx_mon_ring_mask(soc->wlan_cfg_ctx, i);
		soc->intr_ctx[i].rx_err_ring_mask =
			wlan_cfg_get_rx_err_ring_mask(soc->wlan_cfg_ctx, i);
		soc->intr_ctx[i].rx_wbm_rel_ring_mask =
			wlan_cfg_get_rx_wbm_rel_ring_mask(soc->wlan_cfg_ctx, i);
		soc->intr_ctx[i].reo_status_ring_mask =
			wlan_cfg_get_reo_status_ring_mask(soc->wlan_cfg_ctx, i);
		soc->intr_ctx[i].rxdma2host_ring_mask =
			wlan_cfg_get_rxdma2host_ring_mask(soc->wlan_cfg_ctx, i);
		soc->intr_ctx[i].soc = soc;
		soc->intr_ctx[i].lro_ctx = qdf_lro_init();

		if (dp_is_mon_mask_valid(soc, &soc->intr_ctx[i])) {
			hif_event_history_init(soc->hif_handle, i);
			soc->mon_intr_id_lmac_map[lmac_id] = i;
			lmac_id++;
		}
	}

	qdf_timer_init(soc->osdev, &soc->int_timer,
		       dp_interrupt_timer, (void *)soc,
		       QDF_TIMER_TYPE_WAKE_APPS);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_NEAR_FULL_IRQ
/**
 * dp_soc_near_full_interrupt_attach() - Register handler for DP near fill irq
 * @soc: DP soc handle
 * @num_irq: IRQ number
 * @irq_id_map: IRQ map
 * @intr_id: interrupt context ID
 *
 * Return: 0 for success. nonzero for failure.
 */
static inline int
dp_soc_near_full_interrupt_attach(struct dp_soc *soc, int num_irq,
				  int irq_id_map[], int intr_id)
{
	return hif_register_ext_group(soc->hif_handle,
				      num_irq, irq_id_map,
				      dp_service_near_full_srngs,
				      &soc->intr_ctx[intr_id], "dp_nf_intr",
				      HIF_EXEC_NAPI_TYPE,
				      QCA_NAPI_DEF_SCALE_BIN_SHIFT);
}
#else
static inline int
dp_soc_near_full_interrupt_attach(struct dp_soc *soc, int num_irq,
				  int *irq_id_map, int intr_id)
{
	return 0;
}
#endif

#ifdef DP_CON_MON_MSI_SKIP_SET
static inline bool dp_skip_rx_mon_ring_mask_set(struct dp_soc *soc)
{
	return !!(soc->cdp_soc.ol_ops->get_con_mode() !=
		 QDF_GLOBAL_MONITOR_MODE &&
		 !dp_mon_mode_local_pkt_capture(soc));
}
#else
static inline bool dp_skip_rx_mon_ring_mask_set(struct dp_soc *soc)
{
	return false;
}
#endif

void dp_soc_interrupt_detach(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	int i;

	if (soc->intr_mode == DP_INTR_POLL) {
		qdf_timer_free(&soc->int_timer);
	} else {
		hif_deconfigure_ext_group_interrupts(soc->hif_handle);
		hif_deregister_exec_group(soc->hif_handle, "dp_intr");
		hif_deregister_exec_group(soc->hif_handle, "dp_nf_intr");
	}

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		soc->intr_ctx[i].tx_ring_mask = 0;
		soc->intr_ctx[i].rx_ring_mask = 0;
		soc->intr_ctx[i].rx_mon_ring_mask = 0;
		soc->intr_ctx[i].rx_err_ring_mask = 0;
		soc->intr_ctx[i].rx_wbm_rel_ring_mask = 0;
		soc->intr_ctx[i].reo_status_ring_mask = 0;
		soc->intr_ctx[i].rxdma2host_ring_mask = 0;
		soc->intr_ctx[i].host2rxdma_ring_mask = 0;
		soc->intr_ctx[i].host2rxdma_mon_ring_mask = 0;
		soc->intr_ctx[i].rx_near_full_grp_1_mask = 0;
		soc->intr_ctx[i].rx_near_full_grp_2_mask = 0;
		soc->intr_ctx[i].tx_ring_near_full_mask = 0;
		soc->intr_ctx[i].tx_mon_ring_mask = 0;
		soc->intr_ctx[i].host2txmon_ring_mask = 0;
		soc->intr_ctx[i].umac_reset_intr_mask = 0;

		hif_event_history_deinit(soc->hif_handle, i);
		qdf_lro_deinit(soc->intr_ctx[i].lro_ctx);
	}

	qdf_mem_set(&soc->mon_intr_id_lmac_map,
		    sizeof(soc->mon_intr_id_lmac_map),
		    DP_MON_INVALID_LMAC_ID);
}

QDF_STATUS dp_soc_interrupt_attach(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	int i = 0;
	int num_irq = 0;
	int rx_err_ring_intr_ctxt_id = HIF_MAX_GROUP;
	int lmac_id = 0;
	int napi_scale;

	qdf_mem_set(&soc->mon_intr_id_lmac_map,
		    sizeof(soc->mon_intr_id_lmac_map), DP_MON_INVALID_LMAC_ID);

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		int ret = 0;

		/* Map of IRQ ids registered with one interrupt context */
		int irq_id_map[HIF_MAX_GRP_IRQ];

		int tx_mask =
			wlan_cfg_get_tx_ring_mask(soc->wlan_cfg_ctx, i);
		int rx_mask =
			wlan_cfg_get_rx_ring_mask(soc->wlan_cfg_ctx, i);
		int rx_mon_mask =
			dp_soc_get_mon_mask_for_interrupt_mode(soc, i);
		int tx_mon_ring_mask =
			wlan_cfg_get_tx_mon_ring_mask(soc->wlan_cfg_ctx, i);
		int rx_err_ring_mask =
			wlan_cfg_get_rx_err_ring_mask(soc->wlan_cfg_ctx, i);
		int rx_wbm_rel_ring_mask =
			wlan_cfg_get_rx_wbm_rel_ring_mask(soc->wlan_cfg_ctx, i);
		int reo_status_ring_mask =
			wlan_cfg_get_reo_status_ring_mask(soc->wlan_cfg_ctx, i);
		int rxdma2host_ring_mask =
			wlan_cfg_get_rxdma2host_ring_mask(soc->wlan_cfg_ctx, i);
		int host2rxdma_ring_mask =
			wlan_cfg_get_host2rxdma_ring_mask(soc->wlan_cfg_ctx, i);
		int host2rxdma_mon_ring_mask =
			wlan_cfg_get_host2rxdma_mon_ring_mask(
				soc->wlan_cfg_ctx, i);
		int rx_near_full_grp_1_mask =
			wlan_cfg_get_rx_near_full_grp_1_mask(soc->wlan_cfg_ctx,
							     i);
		int rx_near_full_grp_2_mask =
			wlan_cfg_get_rx_near_full_grp_2_mask(soc->wlan_cfg_ctx,
							     i);
		int tx_ring_near_full_mask =
			wlan_cfg_get_tx_ring_near_full_mask(soc->wlan_cfg_ctx,
							    i);
		int host2txmon_ring_mask =
			wlan_cfg_get_host2txmon_ring_mask(soc->wlan_cfg_ctx, i);
		int umac_reset_intr_mask =
			wlan_cfg_get_umac_reset_intr_mask(soc->wlan_cfg_ctx, i);

		if (dp_skip_rx_mon_ring_mask_set(soc))
			rx_mon_mask = 0;

		soc->intr_ctx[i].dp_intr_id = i;
		soc->intr_ctx[i].tx_ring_mask = tx_mask;
		soc->intr_ctx[i].rx_ring_mask = rx_mask;
		soc->intr_ctx[i].rx_mon_ring_mask = rx_mon_mask;
		soc->intr_ctx[i].rx_err_ring_mask = rx_err_ring_mask;
		soc->intr_ctx[i].rxdma2host_ring_mask = rxdma2host_ring_mask;
		soc->intr_ctx[i].host2rxdma_ring_mask = host2rxdma_ring_mask;
		soc->intr_ctx[i].rx_wbm_rel_ring_mask = rx_wbm_rel_ring_mask;
		soc->intr_ctx[i].reo_status_ring_mask = reo_status_ring_mask;
		soc->intr_ctx[i].host2rxdma_mon_ring_mask =
			 host2rxdma_mon_ring_mask;
		soc->intr_ctx[i].rx_near_full_grp_1_mask =
						rx_near_full_grp_1_mask;
		soc->intr_ctx[i].rx_near_full_grp_2_mask =
						rx_near_full_grp_2_mask;
		soc->intr_ctx[i].tx_ring_near_full_mask =
						tx_ring_near_full_mask;
		soc->intr_ctx[i].tx_mon_ring_mask = tx_mon_ring_mask;
		soc->intr_ctx[i].host2txmon_ring_mask = host2txmon_ring_mask;
		soc->intr_ctx[i].umac_reset_intr_mask = umac_reset_intr_mask;

		soc->intr_ctx[i].soc = soc;

		num_irq = 0;

		dp_soc_interrupt_map_calculate(soc, i, &irq_id_map[0],
					       &num_irq);

		if (rx_near_full_grp_1_mask | rx_near_full_grp_2_mask |
		    tx_ring_near_full_mask) {
			dp_soc_near_full_interrupt_attach(soc, num_irq,
							  irq_id_map, i);
		} else {
			napi_scale = wlan_cfg_get_napi_scale_factor(
							    soc->wlan_cfg_ctx);
			if (!napi_scale)
				napi_scale = QCA_NAPI_DEF_SCALE_BIN_SHIFT;

			ret = hif_register_ext_group(soc->hif_handle,
				num_irq, irq_id_map, dp_service_srngs_wrapper,
				&soc->intr_ctx[i], "dp_intr",
				HIF_EXEC_NAPI_TYPE, napi_scale);
		}

		dp_debug(" int ctx %u num_irq %u irq_id_map %u %u",
			 i, num_irq, irq_id_map[0], irq_id_map[1]);

		if (ret) {
			dp_init_err("%pK: failed, ret = %d", soc, ret);
			dp_soc_interrupt_detach(txrx_soc);
			return QDF_STATUS_E_FAILURE;
		}

		hif_event_history_init(soc->hif_handle, i);
		soc->intr_ctx[i].lro_ctx = qdf_lro_init();

		if (rx_err_ring_mask)
			rx_err_ring_intr_ctxt_id = i;

		if (dp_is_mon_mask_valid(soc, &soc->intr_ctx[i])) {
			soc->mon_intr_id_lmac_map[lmac_id] = i;
			lmac_id++;
		}
	}

	hif_configure_ext_group_interrupts(soc->hif_handle);
	if (rx_err_ring_intr_ctxt_id != HIF_MAX_GROUP)
		hif_config_irq_clear_cpu_affinity(soc->hif_handle,
						  rx_err_ring_intr_ctxt_id, 0);

	return QDF_STATUS_SUCCESS;
}

#define AVG_MAX_MPDUS_PER_TID 128
#define AVG_TIDS_PER_CLIENT 2
#define AVG_FLOWS_PER_TID 2
#define AVG_MSDUS_PER_FLOW 128
#define AVG_MSDUS_PER_MPDU 4

void dp_hw_link_desc_pool_banks_free(struct dp_soc *soc, uint32_t mac_id)
{
	struct qdf_mem_multi_page_t *pages;

	if (mac_id != WLAN_INVALID_PDEV_ID) {
		pages = dp_monitor_get_link_desc_pages(soc, mac_id);
	} else {
		pages = &soc->link_desc_pages;
	}

	if (!pages) {
		dp_err("can not get link desc pages");
		QDF_ASSERT(0);
		return;
	}

	if (pages->dma_pages) {
		wlan_minidump_remove((void *)
				     pages->dma_pages->page_v_addr_start,
				     pages->num_pages * pages->page_size,
				     soc->ctrl_psoc,
				     WLAN_MD_DP_SRNG_WBM_IDLE_LINK,
				     "hw_link_desc_bank");
		dp_desc_multi_pages_mem_free(soc, QDF_DP_HW_LINK_DESC_TYPE,
					     pages, 0, false);
	}
}

qdf_export_symbol(dp_hw_link_desc_pool_banks_free);

QDF_STATUS dp_hw_link_desc_pool_banks_alloc(struct dp_soc *soc, uint32_t mac_id)
{
	hal_soc_handle_t hal_soc = soc->hal_soc;
	int link_desc_size = hal_get_link_desc_size(soc->hal_soc);
	int link_desc_align = hal_get_link_desc_align(soc->hal_soc);
	uint32_t max_clients = wlan_cfg_get_max_clients(soc->wlan_cfg_ctx);
	uint32_t num_mpdus_per_link_desc = hal_num_mpdus_per_link_desc(hal_soc);
	uint32_t num_msdus_per_link_desc = hal_num_msdus_per_link_desc(hal_soc);
	uint32_t num_mpdu_links_per_queue_desc =
		hal_num_mpdu_links_per_queue_desc(hal_soc);
	uint32_t max_alloc_size = wlan_cfg_max_alloc_size(soc->wlan_cfg_ctx);
	uint32_t *total_link_descs, total_mem_size;
	uint32_t num_mpdu_link_descs, num_mpdu_queue_descs;
	uint32_t num_tx_msdu_link_descs, num_rx_msdu_link_descs;
	uint32_t num_entries;
	struct qdf_mem_multi_page_t *pages;
	struct dp_srng *dp_srng;
	uint8_t minidump_str[MINIDUMP_STR_SIZE];

	/* Only Tx queue descriptors are allocated from common link descriptor
	 * pool Rx queue descriptors are not included in this because (REO queue
	 * extension descriptors) they are expected to be allocated contiguously
	 * with REO queue descriptors
	 */
	if (mac_id != WLAN_INVALID_PDEV_ID) {
		pages = dp_monitor_get_link_desc_pages(soc, mac_id);
		/* dp_monitor_get_link_desc_pages returns NULL only
		 * if monitor SOC is  NULL
		 */
		if (!pages) {
			dp_err("can not get link desc pages");
			QDF_ASSERT(0);
			return QDF_STATUS_E_FAULT;
		}
		dp_srng = &soc->rxdma_mon_desc_ring[mac_id];
		num_entries = dp_srng->alloc_size /
			hal_srng_get_entrysize(soc->hal_soc,
					       RXDMA_MONITOR_DESC);
		total_link_descs = dp_monitor_get_total_link_descs(soc, mac_id);
		qdf_str_lcopy(minidump_str, "mon_link_desc_bank",
			      MINIDUMP_STR_SIZE);
	} else {
		num_mpdu_link_descs = (max_clients * AVG_TIDS_PER_CLIENT *
			AVG_MAX_MPDUS_PER_TID) / num_mpdus_per_link_desc;

		num_mpdu_queue_descs = num_mpdu_link_descs /
			num_mpdu_links_per_queue_desc;

		num_tx_msdu_link_descs = (max_clients * AVG_TIDS_PER_CLIENT *
			AVG_FLOWS_PER_TID * AVG_MSDUS_PER_FLOW) /
			num_msdus_per_link_desc;

		num_rx_msdu_link_descs = (max_clients * AVG_TIDS_PER_CLIENT *
			AVG_MAX_MPDUS_PER_TID * AVG_MSDUS_PER_MPDU) / 6;

		num_entries = num_mpdu_link_descs + num_mpdu_queue_descs +
			num_tx_msdu_link_descs + num_rx_msdu_link_descs;

		pages = &soc->link_desc_pages;
		total_link_descs = &soc->total_link_descs;
		qdf_str_lcopy(minidump_str, "link_desc_bank",
			      MINIDUMP_STR_SIZE);
	}

	/* If link descriptor banks are allocated, return from here */
	if (pages->num_pages)
		return QDF_STATUS_SUCCESS;

	/* Round up to power of 2 */
	*total_link_descs = 1;
	while (*total_link_descs < num_entries)
		*total_link_descs <<= 1;

	dp_init_info("%pK: total_link_descs: %u, link_desc_size: %d",
		     soc, *total_link_descs, link_desc_size);
	total_mem_size =  *total_link_descs * link_desc_size;
	total_mem_size += link_desc_align;

	dp_init_info("%pK: total_mem_size: %d",
		     soc, total_mem_size);

	dp_set_max_page_size(pages, max_alloc_size);
	dp_desc_multi_pages_mem_alloc(soc, QDF_DP_HW_LINK_DESC_TYPE,
				      pages,
				      link_desc_size,
				      *total_link_descs,
				      0, false);
	if (!pages->num_pages) {
		dp_err("Multi page alloc fail for hw link desc pool");
		return QDF_STATUS_E_FAULT;
	}

	wlan_minidump_log(pages->dma_pages->page_v_addr_start,
			  pages->num_pages * pages->page_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_WBM_IDLE_LINK,
			  "hw_link_desc_bank");

	return QDF_STATUS_SUCCESS;
}

void dp_hw_link_desc_ring_free(struct dp_soc *soc)
{
	uint32_t i;
	uint32_t size = soc->wbm_idle_scatter_buf_size;
	void *vaddr = soc->wbm_idle_link_ring.base_vaddr_unaligned;
	qdf_dma_addr_t paddr;

	if (soc->wbm_idle_scatter_buf_base_vaddr[0]) {
		for (i = 0; i < MAX_IDLE_SCATTER_BUFS; i++) {
			vaddr = soc->wbm_idle_scatter_buf_base_vaddr[i];
			paddr = soc->wbm_idle_scatter_buf_base_paddr[i];
			if (vaddr) {
				qdf_mem_free_consistent(soc->osdev,
							soc->osdev->dev,
							size,
							vaddr,
							paddr,
							0);
				vaddr = NULL;
			}
		}
	} else {
		wlan_minidump_remove(soc->wbm_idle_link_ring.base_vaddr_unaligned,
				     soc->wbm_idle_link_ring.alloc_size,
				     soc->ctrl_psoc,
				     WLAN_MD_DP_SRNG_WBM_IDLE_LINK,
				     "wbm_idle_link_ring");
		dp_srng_free(soc, &soc->wbm_idle_link_ring);
	}
}

QDF_STATUS dp_hw_link_desc_ring_alloc(struct dp_soc *soc)
{
	uint32_t entry_size, i;
	uint32_t total_mem_size;
	qdf_dma_addr_t *baseaddr = NULL;
	struct dp_srng *dp_srng;
	uint32_t ring_type;
	uint32_t max_alloc_size = wlan_cfg_max_alloc_size(soc->wlan_cfg_ctx);
	uint32_t tlds;

	ring_type = WBM_IDLE_LINK;
	dp_srng = &soc->wbm_idle_link_ring;
	tlds = soc->total_link_descs;

	entry_size = hal_srng_get_entrysize(soc->hal_soc, ring_type);
	total_mem_size = entry_size * tlds;

	if (total_mem_size <= max_alloc_size) {
		if (dp_srng_alloc(soc, dp_srng, ring_type, tlds, 0)) {
			dp_init_err("%pK: Link desc idle ring setup failed",
				    soc);
			goto fail;
		}

		wlan_minidump_log(soc->wbm_idle_link_ring.base_vaddr_unaligned,
				  soc->wbm_idle_link_ring.alloc_size,
				  soc->ctrl_psoc,
				  WLAN_MD_DP_SRNG_WBM_IDLE_LINK,
				  "wbm_idle_link_ring");
	} else {
		uint32_t num_scatter_bufs;
		uint32_t buf_size = 0;

		soc->wbm_idle_scatter_buf_size =
			hal_idle_list_scatter_buf_size(soc->hal_soc);
		num_scatter_bufs = hal_idle_list_num_scatter_bufs(
					soc->hal_soc, total_mem_size,
					soc->wbm_idle_scatter_buf_size);

		if (num_scatter_bufs > MAX_IDLE_SCATTER_BUFS) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  FL("scatter bufs size out of bounds"));
			goto fail;
		}

		for (i = 0; i < num_scatter_bufs; i++) {
			baseaddr = &soc->wbm_idle_scatter_buf_base_paddr[i];
			buf_size = soc->wbm_idle_scatter_buf_size;
			soc->wbm_idle_scatter_buf_base_vaddr[i] =
				qdf_mem_alloc_consistent(soc->osdev,
							 soc->osdev->dev,
							 buf_size,
							 baseaddr);

			if (!soc->wbm_idle_scatter_buf_base_vaddr[i]) {
				QDF_TRACE(QDF_MODULE_ID_DP,
					  QDF_TRACE_LEVEL_ERROR,
					  FL("Scatter lst memory alloc fail"));
				goto fail;
			}
		}
		soc->num_scatter_bufs = num_scatter_bufs;
	}
	return QDF_STATUS_SUCCESS;

fail:
	for (i = 0; i < MAX_IDLE_SCATTER_BUFS; i++) {
		void *vaddr = soc->wbm_idle_scatter_buf_base_vaddr[i];
		qdf_dma_addr_t paddr = soc->wbm_idle_scatter_buf_base_paddr[i];

		if (vaddr) {
			qdf_mem_free_consistent(soc->osdev, soc->osdev->dev,
						soc->wbm_idle_scatter_buf_size,
						vaddr,
						paddr, 0);
			vaddr = NULL;
		}
	}
	return QDF_STATUS_E_NOMEM;
}

qdf_export_symbol(dp_hw_link_desc_pool_banks_alloc);

QDF_STATUS dp_hw_link_desc_ring_init(struct dp_soc *soc)
{
	struct dp_srng *dp_srng = &soc->wbm_idle_link_ring;

	if (dp_srng->base_vaddr_unaligned) {
		if (dp_srng_init(soc, dp_srng, WBM_IDLE_LINK, 0, 0))
			return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

void dp_hw_link_desc_ring_deinit(struct dp_soc *soc)
{
	dp_srng_deinit(soc, &soc->wbm_idle_link_ring, WBM_IDLE_LINK, 0);
}

#ifdef IPA_OFFLOAD
#define USE_1_IPA_RX_REO_RING 1
#define USE_2_IPA_RX_REO_RINGS 2
#define REO_DST_RING_SIZE_QCA6290 1023
#ifndef CONFIG_WIFI_EMULATION_WIFI_3_0
#define REO_DST_RING_SIZE_QCA8074 1023
#define REO_DST_RING_SIZE_QCN9000 2048
#else
#define REO_DST_RING_SIZE_QCA8074 8
#define REO_DST_RING_SIZE_QCN9000 8
#endif /* CONFIG_WIFI_EMULATION_WIFI_3_0 */

#ifdef IPA_WDI3_TX_TWO_PIPES
#ifdef DP_MEMORY_OPT
static int dp_ipa_init_alt_tx_ring(struct dp_soc *soc)
{
	return dp_init_tx_ring_pair_by_index(soc, IPA_TX_ALT_RING_IDX);
}

static void dp_ipa_deinit_alt_tx_ring(struct dp_soc *soc)
{
	dp_deinit_tx_pair_by_index(soc, IPA_TX_ALT_RING_IDX);
}

static int dp_ipa_alloc_alt_tx_ring(struct dp_soc *soc)
{
	return dp_alloc_tx_ring_pair_by_index(soc, IPA_TX_ALT_RING_IDX);
}

static void dp_ipa_free_alt_tx_ring(struct dp_soc *soc)
{
	dp_free_tx_ring_pair_by_index(soc, IPA_TX_ALT_RING_IDX);
}

#else /* !DP_MEMORY_OPT */
static int dp_ipa_init_alt_tx_ring(struct dp_soc *soc)
{
	return 0;
}

static void dp_ipa_deinit_alt_tx_ring(struct dp_soc *soc)
{
}

static int dp_ipa_alloc_alt_tx_ring(struct dp_soc *soc)
{
	return 0;
}

static void dp_ipa_free_alt_tx_ring(struct dp_soc *soc)
{
}
#endif /* DP_MEMORY_OPT */

void dp_ipa_hal_tx_init_alt_data_ring(struct dp_soc *soc)
{
	hal_tx_init_data_ring(soc->hal_soc,
			      soc->tcl_data_ring[IPA_TX_ALT_RING_IDX].hal_srng);
}

#else /* !IPA_WDI3_TX_TWO_PIPES */
static int dp_ipa_init_alt_tx_ring(struct dp_soc *soc)
{
	return 0;
}

static void dp_ipa_deinit_alt_tx_ring(struct dp_soc *soc)
{
}

static int dp_ipa_alloc_alt_tx_ring(struct dp_soc *soc)
{
	return 0;
}

static void dp_ipa_free_alt_tx_ring(struct dp_soc *soc)
{
}

void dp_ipa_hal_tx_init_alt_data_ring(struct dp_soc *soc)
{
}

#endif /* IPA_WDI3_TX_TWO_PIPES */

#else

#define REO_DST_RING_SIZE_QCA6290 1024

static int dp_ipa_init_alt_tx_ring(struct dp_soc *soc)
{
	return 0;
}

static void dp_ipa_deinit_alt_tx_ring(struct dp_soc *soc)
{
}

static int dp_ipa_alloc_alt_tx_ring(struct dp_soc *soc)
{
	return 0;
}

static void dp_ipa_free_alt_tx_ring(struct dp_soc *soc)
{
}

void dp_ipa_hal_tx_init_alt_data_ring(struct dp_soc *soc)
{
}

#endif /* IPA_OFFLOAD */

/**
 * dp_soc_reset_cpu_ring_map() - Reset cpu ring map
 * @soc: Datapath soc handler
 *
 * This api resets the default cpu ring map
 */
void dp_soc_reset_cpu_ring_map(struct dp_soc *soc)
{
	uint8_t i;
	int nss_config = wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx);

	for (i = 0; i < WLAN_CFG_INT_NUM_CONTEXTS; i++) {
		switch (nss_config) {
		case dp_nss_cfg_first_radio:
			/*
			 * Setting Tx ring map for one nss offloaded radio
			 */
			soc->tx_ring_map[i] = dp_cpu_ring_map[DP_NSS_FIRST_RADIO_OFFLOADED_MAP][i];
			break;

		case dp_nss_cfg_second_radio:
			/*
			 * Setting Tx ring for two nss offloaded radios
			 */
			soc->tx_ring_map[i] = dp_cpu_ring_map[DP_NSS_SECOND_RADIO_OFFLOADED_MAP][i];
			break;

		case dp_nss_cfg_dbdc:
			/*
			 * Setting Tx ring map for 2 nss offloaded radios
			 */
			soc->tx_ring_map[i] =
				dp_cpu_ring_map[DP_NSS_DBDC_OFFLOADED_MAP][i];
			break;

		case dp_nss_cfg_dbtc:
			/*
			 * Setting Tx ring map for 3 nss offloaded radios
			 */
			soc->tx_ring_map[i] =
				dp_cpu_ring_map[DP_NSS_DBTC_OFFLOADED_MAP][i];
			break;

		default:
			dp_err("tx_ring_map failed due to invalid nss cfg");
			break;
		}
	}
}

/**
 * dp_soc_disable_unused_mac_intr_mask() - reset interrupt mask for
 *					  unused WMAC hw rings
 * @soc: DP Soc handle
 * @mac_num: wmac num
 *
 * Return: Return void
 */
static void dp_soc_disable_unused_mac_intr_mask(struct dp_soc *soc,
						int mac_num)
{
	uint8_t *grp_mask = NULL;
	int group_number;

	grp_mask = &soc->wlan_cfg_ctx->int_host2rxdma_ring_mask[0];
	group_number = dp_srng_find_ring_in_mask(mac_num, grp_mask);
	if (group_number < 0)
		dp_init_debug("%pK: ring not part of any group; ring_type: RXDMA_BUF, mac_num %d",
			      soc, mac_num);
	else
		wlan_cfg_set_host2rxdma_ring_mask(soc->wlan_cfg_ctx,
						  group_number, 0x0);

	grp_mask = &soc->wlan_cfg_ctx->int_rx_mon_ring_mask[0];
	group_number = dp_srng_find_ring_in_mask(mac_num, grp_mask);
	if (group_number < 0)
		dp_init_debug("%pK: ring not part of any group; ring_type: RXDMA_MONITOR_DST, mac_num %d",
			      soc, mac_num);
	else
		wlan_cfg_set_rx_mon_ring_mask(soc->wlan_cfg_ctx,
					      group_number, 0x0);

	grp_mask = &soc->wlan_cfg_ctx->int_rxdma2host_ring_mask[0];
	group_number = dp_srng_find_ring_in_mask(mac_num, grp_mask);
	if (group_number < 0)
		dp_init_debug("%pK: ring not part of any group; ring_type: RXDMA_DST, mac_num %d",
			      soc, mac_num);
	else
		wlan_cfg_set_rxdma2host_ring_mask(soc->wlan_cfg_ctx,
						  group_number, 0x0);

	grp_mask = &soc->wlan_cfg_ctx->int_host2rxdma_mon_ring_mask[0];
	group_number = dp_srng_find_ring_in_mask(mac_num, grp_mask);
	if (group_number < 0)
		dp_init_debug("%pK: ring not part of any group; ring_type: RXDMA_MONITOR_BUF, mac_num %d",
			      soc, mac_num);
	else
		wlan_cfg_set_host2rxdma_mon_ring_mask(soc->wlan_cfg_ctx,
						      group_number, 0x0);
}

#ifdef IPA_OFFLOAD
#ifdef IPA_WDI3_VLAN_SUPPORT
/**
 * dp_soc_reset_ipa_vlan_intr_mask() - reset interrupt mask for IPA offloaded
 *                                     ring for vlan tagged traffic
 * @soc: DP Soc handle
 *
 * Return: Return void
 */
void dp_soc_reset_ipa_vlan_intr_mask(struct dp_soc *soc)
{
	uint8_t *grp_mask = NULL;
	int group_number, mask;

	if (!wlan_ipa_is_vlan_enabled())
		return;

	grp_mask = &soc->wlan_cfg_ctx->int_rx_ring_mask[0];

	group_number = dp_srng_find_ring_in_mask(IPA_ALT_REO_DEST_RING_IDX, grp_mask);
	if (group_number < 0) {
		dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
			      soc, REO_DST, IPA_ALT_REO_DEST_RING_IDX);
		return;
	}

	mask =  wlan_cfg_get_rx_ring_mask(soc->wlan_cfg_ctx, group_number);

	/* reset the interrupt mask for offloaded ring */
	mask &= (~(1 << IPA_ALT_REO_DEST_RING_IDX));

	/*
	 * set the interrupt mask to zero for rx offloaded radio.
	 */
	wlan_cfg_set_rx_ring_mask(soc->wlan_cfg_ctx, group_number, mask);
}
#else
inline
void dp_soc_reset_ipa_vlan_intr_mask(struct dp_soc *soc)
{ }
#endif /* IPA_WDI3_VLAN_SUPPORT */
#else
inline
void dp_soc_reset_ipa_vlan_intr_mask(struct dp_soc *soc)
{ }
#endif /* IPA_OFFLOAD */

/**
 * dp_soc_reset_intr_mask() - reset interrupt mask
 * @soc: DP Soc handle
 *
 * Return: Return void
 */
void dp_soc_reset_intr_mask(struct dp_soc *soc)
{
	uint8_t j;
	uint8_t *grp_mask = NULL;
	int group_number, mask, num_ring;

	/* number of tx ring */
	num_ring = soc->num_tcl_data_rings;

	/*
	 * group mask for tx completion  ring.
	 */
	grp_mask =  &soc->wlan_cfg_ctx->int_tx_ring_mask[0];

	/* loop and reset the mask for only offloaded ring */
	for (j = 0; j < WLAN_CFG_NUM_TCL_DATA_RINGS; j++) {
		/*
		 * Group number corresponding to tx offloaded ring.
		 */
		group_number = dp_srng_find_ring_in_mask(j, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
				      soc, WBM2SW_RELEASE, j);
			continue;
		}

		mask = wlan_cfg_get_tx_ring_mask(soc->wlan_cfg_ctx, group_number);
		if (!dp_soc_ring_if_nss_offloaded(soc, WBM2SW_RELEASE, j) &&
		    (!mask)) {
			continue;
		}

		/* reset the tx mask for offloaded ring */
		mask &= (~(1 << j));

		/*
		 * reset the interrupt mask for offloaded ring.
		 */
		wlan_cfg_set_tx_ring_mask(soc->wlan_cfg_ctx, group_number, mask);
	}

	/* number of rx rings */
	num_ring = soc->num_reo_dest_rings;

	/*
	 * group mask for reo destination ring.
	 */
	grp_mask = &soc->wlan_cfg_ctx->int_rx_ring_mask[0];

	/* loop and reset the mask for only offloaded ring */
	for (j = 0; j < WLAN_CFG_NUM_REO_DEST_RING; j++) {
		/*
		 * Group number corresponding to rx offloaded ring.
		 */
		group_number = dp_srng_find_ring_in_mask(j, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
				      soc, REO_DST, j);
			continue;
		}

		mask =  wlan_cfg_get_rx_ring_mask(soc->wlan_cfg_ctx, group_number);
		if (!dp_soc_ring_if_nss_offloaded(soc, REO_DST, j) &&
		    (!mask)) {
			continue;
		}

		/* reset the interrupt mask for offloaded ring */
		mask &= (~(1 << j));

		/*
		 * set the interrupt mask to zero for rx offloaded radio.
		 */
		wlan_cfg_set_rx_ring_mask(soc->wlan_cfg_ctx, group_number, mask);
	}

	/*
	 * group mask for Rx buffer refill ring
	 */
	grp_mask = &soc->wlan_cfg_ctx->int_host2rxdma_ring_mask[0];

	/* loop and reset the mask for only offloaded ring */
	for (j = 0; j < MAX_PDEV_CNT; j++) {
		int lmac_id = wlan_cfg_get_hw_mac_idx(soc->wlan_cfg_ctx, j);

		if (!dp_soc_ring_if_nss_offloaded(soc, RXDMA_BUF, j)) {
			continue;
		}

		/*
		 * Group number corresponding to rx offloaded ring.
		 */
		group_number = dp_srng_find_ring_in_mask(lmac_id, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
				      soc, REO_DST, lmac_id);
			continue;
		}

		/* set the interrupt mask for offloaded ring */
		mask =  wlan_cfg_get_host2rxdma_ring_mask(soc->wlan_cfg_ctx,
							  group_number);
		mask &= (~(1 << lmac_id));

		/*
		 * set the interrupt mask to zero for rx offloaded radio.
		 */
		wlan_cfg_set_host2rxdma_ring_mask(soc->wlan_cfg_ctx,
						  group_number, mask);
	}

	grp_mask = &soc->wlan_cfg_ctx->int_rx_err_ring_mask[0];

	for (j = 0; j < num_ring; j++) {
		if (!dp_soc_ring_if_nss_offloaded(soc, REO_EXCEPTION, j)) {
			continue;
		}

		/*
		 * Group number corresponding to rx err ring.
		 */
		group_number = dp_srng_find_ring_in_mask(j, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
				      soc, REO_EXCEPTION, j);
			continue;
		}

		wlan_cfg_set_rx_err_ring_mask(soc->wlan_cfg_ctx,
					      group_number, 0);
	}
}

#ifdef IPA_OFFLOAD
bool dp_reo_remap_config(struct dp_soc *soc, uint32_t *remap0,
			 uint32_t *remap1, uint32_t *remap2)
{
	uint32_t ring[WLAN_CFG_NUM_REO_DEST_RING_MAX] = {
				REO_REMAP_SW1, REO_REMAP_SW2, REO_REMAP_SW3,
				REO_REMAP_SW5, REO_REMAP_SW6, REO_REMAP_SW7};

	switch (soc->arch_id) {
	case CDP_ARCH_TYPE_BE:
		hal_compute_reo_remap_ix2_ix3(soc->hal_soc, ring,
					      soc->num_reo_dest_rings -
					      USE_2_IPA_RX_REO_RINGS, remap1,
					      remap2);
		break;

	case CDP_ARCH_TYPE_LI:
		if (wlan_ipa_is_vlan_enabled()) {
			hal_compute_reo_remap_ix2_ix3(
					soc->hal_soc, ring,
					soc->num_reo_dest_rings -
					USE_2_IPA_RX_REO_RINGS, remap1,
					remap2);

		} else {
			hal_compute_reo_remap_ix2_ix3(
					soc->hal_soc, ring,
					soc->num_reo_dest_rings -
					USE_1_IPA_RX_REO_RING, remap1,
					remap2);
		}

		hal_compute_reo_remap_ix0(soc->hal_soc, remap0);
		break;
	default:
		dp_err("unknown arch_id 0x%x", soc->arch_id);
		QDF_BUG(0);
	}

	dp_debug("remap1 %x remap2 %x", *remap1, *remap2);

	return true;
}

#ifdef IPA_WDI3_TX_TWO_PIPES
static bool dp_ipa_is_alt_tx_ring(int index)
{
	return index == IPA_TX_ALT_RING_IDX;
}

static bool dp_ipa_is_alt_tx_comp_ring(int index)
{
	return index == IPA_TX_ALT_COMP_RING_IDX;
}
#else /* !IPA_WDI3_TX_TWO_PIPES */
static bool dp_ipa_is_alt_tx_ring(int index)
{
	return false;
}

static bool dp_ipa_is_alt_tx_comp_ring(int index)
{
	return false;
}
#endif /* IPA_WDI3_TX_TWO_PIPES */

/**
 * dp_ipa_get_tx_ring_size() - Get Tx ring size for IPA
 *
 * @tx_ring_num: Tx ring number
 * @tx_ipa_ring_sz: Return param only updated for IPA.
 * @soc_cfg_ctx: dp soc cfg context
 *
 * Return: None
 */
static void dp_ipa_get_tx_ring_size(int tx_ring_num, int *tx_ipa_ring_sz,
				    struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx)
{
	if (!soc_cfg_ctx->ipa_enabled)
		return;

	if (tx_ring_num == IPA_TCL_DATA_RING_IDX)
		*tx_ipa_ring_sz = wlan_cfg_ipa_tx_ring_size(soc_cfg_ctx);
	else if (dp_ipa_is_alt_tx_ring(tx_ring_num))
		*tx_ipa_ring_sz = wlan_cfg_ipa_tx_alt_ring_size(soc_cfg_ctx);
}

/**
 * dp_ipa_get_tx_comp_ring_size() - Get Tx comp ring size for IPA
 *
 * @tx_comp_ring_num: Tx comp ring number
 * @tx_comp_ipa_ring_sz: Return param only updated for IPA.
 * @soc_cfg_ctx: dp soc cfg context
 *
 * Return: None
 */
static void dp_ipa_get_tx_comp_ring_size(int tx_comp_ring_num,
					 int *tx_comp_ipa_ring_sz,
				       struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx)
{
	if (!soc_cfg_ctx->ipa_enabled)
		return;

	if (tx_comp_ring_num == IPA_TCL_DATA_RING_IDX)
		*tx_comp_ipa_ring_sz =
				wlan_cfg_ipa_tx_comp_ring_size(soc_cfg_ctx);
	else if (dp_ipa_is_alt_tx_comp_ring(tx_comp_ring_num))
		*tx_comp_ipa_ring_sz =
				wlan_cfg_ipa_tx_alt_comp_ring_size(soc_cfg_ctx);
}
#else
static uint8_t dp_reo_ring_selection(uint32_t value, uint32_t *ring)
{
	uint8_t num = 0;

	switch (value) {
	/* should we have all the different possible ring configs */
	case 0xFF:
		num = 8;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW2;
		ring[2] = REO_REMAP_SW3;
		ring[3] = REO_REMAP_SW4;
		ring[4] = REO_REMAP_SW5;
		ring[5] = REO_REMAP_SW6;
		ring[6] = REO_REMAP_SW7;
		ring[7] = REO_REMAP_SW8;
		break;

	case 0x3F:
		num = 6;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW2;
		ring[2] = REO_REMAP_SW3;
		ring[3] = REO_REMAP_SW4;
		ring[4] = REO_REMAP_SW5;
		ring[5] = REO_REMAP_SW6;
		break;

	case 0xF:
		num = 4;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW2;
		ring[2] = REO_REMAP_SW3;
		ring[3] = REO_REMAP_SW4;
		break;
	case 0xE:
		num = 3;
		ring[0] = REO_REMAP_SW2;
		ring[1] = REO_REMAP_SW3;
		ring[2] = REO_REMAP_SW4;
		break;
	case 0xD:
		num = 3;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW3;
		ring[2] = REO_REMAP_SW4;
		break;
	case 0xC:
		num = 2;
		ring[0] = REO_REMAP_SW3;
		ring[1] = REO_REMAP_SW4;
		break;
	case 0xB:
		num = 3;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW2;
		ring[2] = REO_REMAP_SW4;
		break;
	case 0xA:
		num = 2;
		ring[0] = REO_REMAP_SW2;
		ring[1] = REO_REMAP_SW4;
		break;
	case 0x9:
		num = 2;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW4;
		break;
	case 0x8:
		num = 1;
		ring[0] = REO_REMAP_SW4;
		break;
	case 0x7:
		num = 3;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW2;
		ring[2] = REO_REMAP_SW3;
		break;
	case 0x6:
		num = 2;
		ring[0] = REO_REMAP_SW2;
		ring[1] = REO_REMAP_SW3;
		break;
	case 0x5:
		num = 2;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW3;
		break;
	case 0x4:
		num = 1;
		ring[0] = REO_REMAP_SW3;
		break;
	case 0x3:
		num = 2;
		ring[0] = REO_REMAP_SW1;
		ring[1] = REO_REMAP_SW2;
		break;
	case 0x2:
		num = 1;
		ring[0] = REO_REMAP_SW2;
		break;
	case 0x1:
		num = 1;
		ring[0] = REO_REMAP_SW1;
		break;
	default:
		dp_err("unknown reo ring map 0x%x", value);
		QDF_BUG(0);
	}
	return num;
}

bool dp_reo_remap_config(struct dp_soc *soc,
			 uint32_t *remap0,
			 uint32_t *remap1,
			 uint32_t *remap2)
{
	uint8_t offload_radio = wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx);
	uint32_t reo_config = wlan_cfg_get_reo_rings_mapping(soc->wlan_cfg_ctx);
	uint8_t num;
	uint32_t ring[WLAN_CFG_NUM_REO_DEST_RING_MAX];
	uint32_t value;

	switch (offload_radio) {
	case dp_nss_cfg_default:
		value = reo_config & WLAN_CFG_NUM_REO_RINGS_MAP_MAX;
		num = dp_reo_ring_selection(value, ring);
		hal_compute_reo_remap_ix2_ix3(soc->hal_soc, ring,
					      num, remap1, remap2);
		hal_compute_reo_remap_ix0(soc->hal_soc, remap0);

		break;
	case dp_nss_cfg_first_radio:
		value = reo_config & 0xE;
		num = dp_reo_ring_selection(value, ring);
		hal_compute_reo_remap_ix2_ix3(soc->hal_soc, ring,
					      num, remap1, remap2);

		break;
	case dp_nss_cfg_second_radio:
		value = reo_config & 0xD;
		num = dp_reo_ring_selection(value, ring);
		hal_compute_reo_remap_ix2_ix3(soc->hal_soc, ring,
					      num, remap1, remap2);

		break;
	case dp_nss_cfg_dbdc:
	case dp_nss_cfg_dbtc:
		/* return false if both or all are offloaded to NSS */
		return false;
	}

	dp_debug("remap1 %x remap2 %x offload_radio %u",
		 *remap1, *remap2, offload_radio);
	return true;
}

static void dp_ipa_get_tx_ring_size(int ring_num, int *tx_ipa_ring_sz,
				    struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx)
{
}

static void dp_ipa_get_tx_comp_ring_size(int tx_comp_ring_num,
					 int *tx_comp_ipa_ring_sz,
				       struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx)
{
}
#endif /* IPA_OFFLOAD */

/**
 * dp_reo_frag_dst_set() - configure reo register to set the
 *                        fragment destination ring
 * @soc: Datapath soc
 * @frag_dst_ring: output parameter to set fragment destination ring
 *
 * Based on offload_radio below fragment destination rings is selected
 * 0 - TCL
 * 1 - SW1
 * 2 - SW2
 * 3 - SW3
 * 4 - SW4
 * 5 - Release
 * 6 - FW
 * 7 - alternate select
 *
 * Return: void
 */
void dp_reo_frag_dst_set(struct dp_soc *soc, uint8_t *frag_dst_ring)
{
	uint8_t offload_radio = wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx);

	switch (offload_radio) {
	case dp_nss_cfg_default:
		*frag_dst_ring = REO_REMAP_TCL;
		break;
	case dp_nss_cfg_first_radio:
		/*
		 * This configuration is valid for single band radio which
		 * is also NSS offload.
		 */
	case dp_nss_cfg_dbdc:
	case dp_nss_cfg_dbtc:
		*frag_dst_ring = HAL_SRNG_REO_ALTERNATE_SELECT;
		break;
	default:
		dp_init_err("%pK: dp_reo_frag_dst_set invalid offload radio config", soc);
		break;
	}
}

#ifdef WLAN_FEATURE_STATS_EXT
static inline void dp_create_ext_stats_event(struct dp_soc *soc)
{
	qdf_event_create(&soc->rx_hw_stats_event);
}
#else
static inline void dp_create_ext_stats_event(struct dp_soc *soc)
{
}
#endif

static void dp_deinit_tx_pair_by_index(struct dp_soc *soc, int index)
{
	int tcl_ring_num, wbm_ring_num;

	wlan_cfg_get_tcl_wbm_ring_num_for_index(soc->wlan_cfg_ctx,
						index,
						&tcl_ring_num,
						&wbm_ring_num);

	if (tcl_ring_num == -1) {
		dp_err("incorrect tcl ring num for index %u", index);
		return;
	}

	dp_ssr_dump_srng_unregister("tcl_data_ring", index);
	dp_ssr_dump_srng_unregister("tx_comp_ring", index);

	wlan_minidump_remove(soc->tcl_data_ring[index].base_vaddr_unaligned,
			     soc->tcl_data_ring[index].alloc_size,
			     soc->ctrl_psoc,
			     WLAN_MD_DP_SRNG_TCL_DATA,
			     "tcl_data_ring");
	dp_info("index %u tcl %u wbm %u", index, tcl_ring_num, wbm_ring_num);
	dp_srng_deinit(soc, &soc->tcl_data_ring[index], TCL_DATA,
		       tcl_ring_num);

	if (wbm_ring_num == INVALID_WBM_RING_NUM)
		return;

	wlan_minidump_remove(soc->tx_comp_ring[index].base_vaddr_unaligned,
			     soc->tx_comp_ring[index].alloc_size,
			     soc->ctrl_psoc,
			     WLAN_MD_DP_SRNG_TX_COMP,
			     "tcl_comp_ring");
	dp_srng_deinit(soc, &soc->tx_comp_ring[index], WBM2SW_RELEASE,
		       wbm_ring_num);
}

/**
 * dp_init_tx_ring_pair_by_index() - The function inits tcl data/wbm completion
 * ring pair
 * @soc: DP soc pointer
 * @index: index of soc->tcl_data or soc->tx_comp to initialize
 *
 * Return: QDF_STATUS_SUCCESS on success, error code otherwise.
 */
static QDF_STATUS dp_init_tx_ring_pair_by_index(struct dp_soc *soc,
						uint8_t index)
{
	int tcl_ring_num, wbm_ring_num;
	uint8_t bm_id;

	if (index >= MAX_TCL_DATA_RINGS) {
		dp_err("unexpected index!");
		QDF_BUG(0);
		goto fail1;
	}

	wlan_cfg_get_tcl_wbm_ring_num_for_index(soc->wlan_cfg_ctx,
						index,
						&tcl_ring_num,
						&wbm_ring_num);

	if (tcl_ring_num == -1) {
		dp_err("incorrect tcl ring num for index %u", index);
		goto fail1;
	}

	dp_info("index %u tcl %u wbm %u", index, tcl_ring_num, wbm_ring_num);
	if (dp_srng_init(soc, &soc->tcl_data_ring[index], TCL_DATA,
			 tcl_ring_num, 0)) {
		dp_err("dp_srng_init failed for tcl_data_ring");
		goto fail1;
	}
	wlan_minidump_log(soc->tcl_data_ring[index].base_vaddr_unaligned,
			  soc->tcl_data_ring[index].alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_TCL_DATA,
			  "tcl_data_ring");

	if (wbm_ring_num == INVALID_WBM_RING_NUM)
		goto set_rbm;

	if (dp_srng_init(soc, &soc->tx_comp_ring[index], WBM2SW_RELEASE,
			 wbm_ring_num, 0)) {
		dp_err("dp_srng_init failed for tx_comp_ring");
		goto fail1;
	}

	dp_ssr_dump_srng_register("tcl_data_ring",
				  &soc->tcl_data_ring[index], index);
	dp_ssr_dump_srng_register("tx_comp_ring",
				  &soc->tx_comp_ring[index], index);

	wlan_minidump_log(soc->tx_comp_ring[index].base_vaddr_unaligned,
			  soc->tx_comp_ring[index].alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_TX_COMP,
			  "tcl_comp_ring");
set_rbm:
	bm_id = wlan_cfg_get_rbm_id_for_index(soc->wlan_cfg_ctx, tcl_ring_num);

	soc->arch_ops.tx_implicit_rbm_set(soc, tcl_ring_num, bm_id);

	return QDF_STATUS_SUCCESS;

fail1:
	return QDF_STATUS_E_FAILURE;
}

static void dp_free_tx_ring_pair_by_index(struct dp_soc *soc, uint8_t index)
{
	dp_debug("index %u", index);
	dp_srng_free(soc, &soc->tcl_data_ring[index]);
	dp_srng_free(soc, &soc->tx_comp_ring[index]);
}

/**
 * dp_alloc_tx_ring_pair_by_index() - The function allocs tcl data/wbm2sw
 * ring pair for the given "index"
 * @soc: DP soc pointer
 * @index: index of soc->tcl_data or soc->tx_comp to initialize
 *
 * Return: QDF_STATUS_SUCCESS on success, error code otherwise.
 */
static QDF_STATUS dp_alloc_tx_ring_pair_by_index(struct dp_soc *soc,
						 uint8_t index)
{
	int tx_ring_size;
	int tx_comp_ring_size;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;
	int cached = 0;

	if (index >= MAX_TCL_DATA_RINGS) {
		dp_err("unexpected index!");
		QDF_BUG(0);
		goto fail1;
	}

	dp_debug("index %u", index);
	tx_ring_size = wlan_cfg_tx_ring_size(soc_cfg_ctx);
	dp_ipa_get_tx_ring_size(index, &tx_ring_size, soc_cfg_ctx);

	if (dp_srng_alloc(soc, &soc->tcl_data_ring[index], TCL_DATA,
			  tx_ring_size, cached)) {
		dp_err("dp_srng_alloc failed for tcl_data_ring");
		goto fail1;
	}

	tx_comp_ring_size = wlan_cfg_tx_comp_ring_size(soc_cfg_ctx);
	dp_ipa_get_tx_comp_ring_size(index, &tx_comp_ring_size, soc_cfg_ctx);
	/* Enable cached TCL desc if NSS offload is disabled */
	if (!wlan_cfg_get_dp_soc_nss_cfg(soc_cfg_ctx))
		cached = WLAN_CFG_DST_RING_CACHED_DESC;

	if (wlan_cfg_get_wbm_ring_num_for_index(soc->wlan_cfg_ctx, index) ==
	    INVALID_WBM_RING_NUM)
		return QDF_STATUS_SUCCESS;

	if (dp_srng_alloc(soc, &soc->tx_comp_ring[index], WBM2SW_RELEASE,
			  tx_comp_ring_size, cached)) {
		dp_err("dp_srng_alloc failed for tx_comp_ring");
		goto fail1;
	}

	return QDF_STATUS_SUCCESS;

fail1:
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_dscp_tid_map_setup() - Initialize the dscp-tid maps
 * @pdev: DP_PDEV handle
 *
 * Return: void
 */
void
dp_dscp_tid_map_setup(struct dp_pdev *pdev)
{
	uint8_t map_id;
	struct dp_soc *soc = pdev->soc;

	if (!soc)
		return;

	for (map_id = 0; map_id < DP_MAX_TID_MAPS; map_id++) {
		qdf_mem_copy(pdev->dscp_tid_map[map_id],
			     default_dscp_tid_map,
			     sizeof(default_dscp_tid_map));
	}

	for (map_id = 0; map_id < soc->num_hw_dscp_tid_map; map_id++) {
		hal_tx_set_dscp_tid_map(soc->hal_soc,
					default_dscp_tid_map,
					map_id);
	}
}

/**
 * dp_pcp_tid_map_setup() - Initialize the pcp-tid maps
 * @pdev: DP_PDEV handle
 *
 * Return: void
 */
void
dp_pcp_tid_map_setup(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;

	if (!soc)
		return;

	qdf_mem_copy(soc->pcp_tid_map, default_pcp_tid_map,
		     sizeof(default_pcp_tid_map));
	hal_tx_set_pcp_tid_map_default(soc->hal_soc, default_pcp_tid_map);
}

#ifndef DP_UMAC_HW_RESET_SUPPORT
static inline
#endif
void dp_reo_desc_freelist_destroy(struct dp_soc *soc)
{
	struct reo_desc_list_node *desc;
	struct dp_rx_tid *rx_tid;

	qdf_spin_lock_bh(&soc->reo_desc_freelist_lock);
	while (qdf_list_remove_front(&soc->reo_desc_freelist,
		(qdf_list_node_t **)&desc) == QDF_STATUS_SUCCESS) {
		rx_tid = &desc->rx_tid;
		qdf_mem_unmap_nbytes_single(soc->osdev,
			rx_tid->hw_qdesc_paddr,
			QDF_DMA_BIDIRECTIONAL,
			rx_tid->hw_qdesc_alloc_size);
		qdf_mem_free(rx_tid->hw_qdesc_vaddr_unaligned);
		qdf_mem_free(desc);
	}
	qdf_spin_unlock_bh(&soc->reo_desc_freelist_lock);
	qdf_list_destroy(&soc->reo_desc_freelist);
	qdf_spinlock_destroy(&soc->reo_desc_freelist_lock);
}

#ifdef WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY
/**
 * dp_reo_desc_deferred_freelist_create() - Initialize the resources used
 *                                          for deferred reo desc list
 * @soc: Datapath soc handle
 *
 * Return: void
 */
static void dp_reo_desc_deferred_freelist_create(struct dp_soc *soc)
{
	qdf_spinlock_create(&soc->reo_desc_deferred_freelist_lock);
	qdf_list_create(&soc->reo_desc_deferred_freelist,
			REO_DESC_DEFERRED_FREELIST_SIZE);
	soc->reo_desc_deferred_freelist_init = true;
}

/**
 * dp_reo_desc_deferred_freelist_destroy() - loop the deferred free list &
 *                                           free the leftover REO QDESCs
 * @soc: Datapath soc handle
 *
 * Return: void
 */
static void dp_reo_desc_deferred_freelist_destroy(struct dp_soc *soc)
{
	struct reo_desc_deferred_freelist_node *desc;

	qdf_spin_lock_bh(&soc->reo_desc_deferred_freelist_lock);
	soc->reo_desc_deferred_freelist_init = false;
	while (qdf_list_remove_front(&soc->reo_desc_deferred_freelist,
	       (qdf_list_node_t **)&desc) == QDF_STATUS_SUCCESS) {
		qdf_mem_unmap_nbytes_single(soc->osdev,
					    desc->hw_qdesc_paddr,
					    QDF_DMA_BIDIRECTIONAL,
					    desc->hw_qdesc_alloc_size);
		qdf_mem_free(desc->hw_qdesc_vaddr_unaligned);
		qdf_mem_free(desc);
	}
	qdf_spin_unlock_bh(&soc->reo_desc_deferred_freelist_lock);

	qdf_list_destroy(&soc->reo_desc_deferred_freelist);
	qdf_spinlock_destroy(&soc->reo_desc_deferred_freelist_lock);
}
#else
static inline void dp_reo_desc_deferred_freelist_create(struct dp_soc *soc)
{
}

static inline void dp_reo_desc_deferred_freelist_destroy(struct dp_soc *soc)
{
}
#endif /* !WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY */

/**
 * dp_soc_reset_txrx_ring_map() - reset tx ring map
 * @soc: DP SOC handle
 *
 */
static void dp_soc_reset_txrx_ring_map(struct dp_soc *soc)
{
	uint32_t i;

	for (i = 0; i < WLAN_CFG_INT_NUM_CONTEXTS; i++)
		soc->tx_ring_map[i] = 0;
}

/**
 * dp_soc_deinit() - Deinitialize txrx SOC
 * @txrx_soc: Opaque DP SOC handle
 *
 * Return: None
 */
void dp_soc_deinit(void *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	struct htt_soc *htt_soc = soc->htt_handle;

	dp_monitor_soc_deinit(soc);

	/* free peer tables & AST tables allocated during peer_map_attach */
	if (soc->peer_map_attach_success) {
		dp_peer_find_detach(soc);
		soc->arch_ops.txrx_peer_map_detach(soc);
		soc->peer_map_attach_success = FALSE;
	}

	qdf_flush_work(&soc->htt_stats.work);
	qdf_disable_work(&soc->htt_stats.work);

	qdf_spinlock_destroy(&soc->htt_stats.lock);

	dp_soc_reset_txrx_ring_map(soc);

	dp_reo_desc_freelist_destroy(soc);
	dp_reo_desc_deferred_freelist_destroy(soc);

	DEINIT_RX_HW_STATS_LOCK(soc);

	qdf_spinlock_destroy(&soc->ast_lock);

	dp_peer_mec_spinlock_destroy(soc);

	qdf_nbuf_queue_free(&soc->htt_stats.msg);

	qdf_nbuf_queue_free(&soc->invalid_buf_queue);

	qdf_spinlock_destroy(&soc->rx.defrag.defrag_lock);

	qdf_spinlock_destroy(&soc->vdev_map_lock);

	dp_reo_cmdlist_destroy(soc);
	qdf_spinlock_destroy(&soc->rx.reo_cmd_lock);

	dp_soc_tx_desc_sw_pools_deinit(soc);

	dp_soc_srng_deinit(soc);

	dp_hw_link_desc_ring_deinit(soc);

	dp_soc_print_inactive_objects(soc);
	qdf_spinlock_destroy(&soc->inactive_peer_list_lock);
	qdf_spinlock_destroy(&soc->inactive_vdev_list_lock);

	htt_soc_htc_dealloc(soc->htt_handle);

	htt_soc_detach(htt_soc);

	/* Free wbm sg list and reset flags in down path */
	dp_rx_wbm_sg_list_deinit(soc);

	wlan_minidump_remove(soc, sizeof(*soc), soc->ctrl_psoc,
			     WLAN_MD_DP_SOC, "dp_soc");
}

#ifdef QCA_HOST2FW_RXBUF_RING
void
dp_htt_setup_rxdma_err_dst_ring(struct dp_soc *soc, int mac_id,
				int lmac_id)
{
	if (soc->rxdma_err_dst_ring[lmac_id].hal_srng)
		htt_srng_setup(soc->htt_handle, mac_id,
			       soc->rxdma_err_dst_ring[lmac_id].hal_srng,
			       RXDMA_DST);
}
#endif

void dp_vdev_get_default_reo_hash(struct dp_vdev *vdev,
				  enum cdp_host_reo_dest_ring *reo_dest,
				  bool *hash_based)
{
	struct dp_soc *soc;
	struct dp_pdev *pdev;

	pdev = vdev->pdev;
	soc = pdev->soc;
	/*
	 * hash based steering is disabled for Radios which are offloaded
	 * to NSS
	 */
	if (!wlan_cfg_get_dp_pdev_nss_enabled(pdev->wlan_cfg_ctx))
		*hash_based = wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx);

	/*
	 * Below line of code will ensure the proper reo_dest ring is chosen
	 * for cases where toeplitz hash cannot be generated (ex: non TCP/UDP)
	 */
	*reo_dest = pdev->reo_dest;
}

#ifdef IPA_OFFLOAD
/**
 * dp_is_vdev_subtype_p2p() - Check if the subtype for vdev is P2P
 * @vdev: Virtual device
 *
 * Return: true if the vdev is of subtype P2P
 *	   false if the vdev is of any other subtype
 */
static inline bool dp_is_vdev_subtype_p2p(struct dp_vdev *vdev)
{
	if (vdev->subtype == wlan_op_subtype_p2p_device ||
	    vdev->subtype == wlan_op_subtype_p2p_cli ||
	    vdev->subtype == wlan_op_subtype_p2p_go)
		return true;

	return false;
}

/**
 * dp_peer_setup_get_reo_hash() - get reo dest ring and hash values for a peer
 * @vdev: Datapath VDEV handle
 * @setup_info:
 * @reo_dest: pointer to default reo_dest ring for vdev to be populated
 * @hash_based: pointer to hash value (enabled/disabled) to be populated
 * @lmac_peer_id_msb:
 *
 * If IPA is enabled in ini, for SAP mode, disable hash based
 * steering, use default reo_dst ring for RX. Use config values for other modes.
 *
 * Return: None
 */
static void dp_peer_setup_get_reo_hash(struct dp_vdev *vdev,
				       struct cdp_peer_setup_info *setup_info,
				       enum cdp_host_reo_dest_ring *reo_dest,
				       bool *hash_based,
				       uint8_t *lmac_peer_id_msb)
{
	struct dp_soc *soc;
	struct dp_pdev *pdev;

	pdev = vdev->pdev;
	soc = pdev->soc;

	dp_vdev_get_default_reo_hash(vdev, reo_dest, hash_based);

	/* For P2P-GO interfaces we do not need to change the REO
	 * configuration even if IPA config is enabled
	 */
	if (dp_is_vdev_subtype_p2p(vdev))
		return;

	/*
	 * If IPA is enabled, disable hash-based flow steering and set
	 * reo_dest_ring_4 as the REO ring to receive packets on.
	 * IPA is configured to reap reo_dest_ring_4.
	 *
	 * Note - REO DST indexes are from 0 - 3, while cdp_host_reo_dest_ring
	 * value enum value is from 1 - 4.
	 * Hence, *reo_dest = IPA_REO_DEST_RING_IDX + 1
	 */
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		if (dp_ipa_is_mdm_platform()) {
			*reo_dest = IPA_REO_DEST_RING_IDX + 1;
			if (vdev->opmode == wlan_op_mode_ap)
				*hash_based = 0;
		} else {
			dp_debug("opt_dp: default HOST reo ring is set");
		}
	}
}

#else

/**
 * dp_peer_setup_get_reo_hash() - get reo dest ring and hash values for a peer
 * @vdev: Datapath VDEV handle
 * @setup_info:
 * @reo_dest: pointer to default reo_dest ring for vdev to be populated
 * @hash_based: pointer to hash value (enabled/disabled) to be populated
 * @lmac_peer_id_msb:
 *
 * Use system config values for hash based steering.
 * Return: None
 */
static void dp_peer_setup_get_reo_hash(struct dp_vdev *vdev,
				       struct cdp_peer_setup_info *setup_info,
				       enum cdp_host_reo_dest_ring *reo_dest,
				       bool *hash_based,
				       uint8_t *lmac_peer_id_msb)
{
	struct dp_soc *soc = vdev->pdev->soc;

	soc->arch_ops.peer_get_reo_hash(vdev, setup_info, reo_dest, hash_based,
					lmac_peer_id_msb);
}
#endif /* IPA_OFFLOAD */
#if defined WLAN_FEATURE_11BE_MLO && defined DP_MLO_LINK_STATS_SUPPORT

static inline uint8_t
dp_peer_get_local_link_id(struct dp_peer *peer, struct dp_txrx_peer *txrx_peer)
{
	struct dp_local_link_id_peer_map *ll_id_peer_map =
						&txrx_peer->ll_id_peer_map[0];
	int i;

	/*
	 * Search for the peer entry in the
	 * local_link_id to peer mac_addr mapping table
	 */
	for (i = 0; i < DP_MAX_MLO_LINKS; i++) {
		if (ll_id_peer_map[i].in_use &&
		    !qdf_mem_cmp(&peer->mac_addr.raw[0],
				 &ll_id_peer_map[i].mac_addr.raw[0],
				 QDF_MAC_ADDR_SIZE))
			return ll_id_peer_map[i].local_link_id + 1;
	}

	/*
	 * Create new entry for peer in the
	 * local_link_id to peer mac_addr mapping table
	 */
	for (i = 0; i < DP_MAX_MLO_LINKS; i++) {
		if (ll_id_peer_map[i].in_use)
			continue;

		ll_id_peer_map[i].in_use = 1;
		ll_id_peer_map[i].local_link_id = i;
		qdf_mem_copy(&ll_id_peer_map[i].mac_addr.raw[0],
			     &peer->mac_addr.raw[0], QDF_MAC_ADDR_SIZE);
		return ll_id_peer_map[i].local_link_id + 1;
	}

	/* We should not hit this case..!! Assert ?? */
	return 0;
}

/**
 *  dp_peer_set_local_link_id() - Set local link id
 *  @peer: dp peer handle
 *
 *  Return: None
 */
static inline void
dp_peer_set_local_link_id(struct dp_peer *peer)
{
	struct dp_txrx_peer *txrx_peer;

	if (!IS_MLO_DP_LINK_PEER(peer))
		return;

	txrx_peer = dp_get_txrx_peer(peer);
	if (txrx_peer)
		peer->local_link_id = dp_peer_get_local_link_id(peer,
								txrx_peer);

	dp_info("Peer " QDF_MAC_ADDR_FMT " txrx_peer %pK local_link_id %d",
		QDF_MAC_ADDR_REF(peer->mac_addr.raw), txrx_peer,
		peer->local_link_id);
}
#else
static inline void
dp_peer_set_local_link_id(struct dp_peer *peer)
{
}
#endif

/**
 * dp_peer_setup_wifi3() - initialize the peer
 * @soc_hdl: soc handle object
 * @vdev_id: vdev_id of vdev object
 * @peer_mac: Peer's mac address
 * @setup_info: peer setup info for MLO
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_peer_setup_wifi3(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		    uint8_t *peer_mac,
		    struct cdp_peer_setup_info *setup_info)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev;
	bool hash_based = 0;
	enum cdp_host_reo_dest_ring reo_dest;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_vdev *vdev = NULL;
	struct dp_peer *peer =
			dp_peer_find_hash_find(soc, peer_mac, 0, vdev_id,
					       DP_MOD_ID_CDP);
	struct dp_peer *mld_peer = NULL;
	enum wlan_op_mode vdev_opmode;
	uint8_t lmac_peer_id_msb = 0;

	if (!peer)
		return QDF_STATUS_E_FAILURE;

	vdev = peer->vdev;
	if (!vdev) {
		status = QDF_STATUS_E_FAILURE;
		goto fail;
	}

	/* save vdev related member in case vdev freed */
	vdev_opmode = vdev->opmode;
	pdev = vdev->pdev;
	dp_peer_setup_get_reo_hash(vdev, setup_info,
				   &reo_dest, &hash_based,
				   &lmac_peer_id_msb);

	dp_cfg_event_record_peer_setup_evt(soc, DP_CFG_EVENT_PEER_SETUP,
					   peer, vdev, vdev->vdev_id,
					   setup_info);
	dp_info("pdev: %d vdev :%d opmode:%u peer %pK (" QDF_MAC_ADDR_FMT ") "
		"hash-based-steering:%d default-reo_dest:%u",
		pdev->pdev_id, vdev->vdev_id,
		vdev->opmode, peer,
		QDF_MAC_ADDR_REF(peer->mac_addr.raw), hash_based, reo_dest);

	/*
	 * There are corner cases where the AD1 = AD2 = "VAPs address"
	 * i.e both the devices have same MAC address. In these
	 * cases we want such pkts to be processed in NULL Q handler
	 * which is REO2TCL ring. for this reason we should
	 * not setup reo_queues and default route for bss_peer.
	 */
	if (!IS_MLO_DP_MLD_PEER(peer))
		dp_monitor_peer_tx_init(pdev, peer);

	if (!setup_info)
		if (dp_peer_legacy_setup(soc, peer) !=
				QDF_STATUS_SUCCESS) {
			status = QDF_STATUS_E_RESOURCES;
			goto fail;
		}

	if (peer->bss_peer && vdev->opmode == wlan_op_mode_ap) {
		status = QDF_STATUS_E_FAILURE;
		goto fail;
	}

	if (soc->cdp_soc.ol_ops->peer_set_default_routing) {
		/* TODO: Check the destination ring number to be passed to FW */
		soc->cdp_soc.ol_ops->peer_set_default_routing(
				soc->ctrl_psoc,
				peer->vdev->pdev->pdev_id,
				peer->mac_addr.raw,
				peer->vdev->vdev_id, hash_based, reo_dest,
				lmac_peer_id_msb);
	}

	qdf_atomic_set(&peer->is_default_route_set, 1);

	status = dp_peer_mlo_setup(soc, peer, vdev->vdev_id, setup_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_peer_err("peer mlo setup failed");
		qdf_assert_always(0);
	}

	if (vdev_opmode != wlan_op_mode_monitor) {
		/* In case of MLD peer, switch peer to mld peer and
		 * do peer_rx_init.
		 */
		if (hal_reo_shared_qaddr_is_enable(soc->hal_soc) &&
		    IS_MLO_DP_LINK_PEER(peer)) {
			if (setup_info && setup_info->is_first_link) {
				mld_peer = DP_GET_MLD_PEER_FROM_PEER(peer);
				if (mld_peer)
					dp_peer_rx_init(pdev, mld_peer);
				else
					dp_peer_err("MLD peer null. Primary link peer:%pK", peer);
			}
		} else {
			dp_peer_rx_init_wrapper(pdev, peer, setup_info);
		}
	}

	dp_peer_set_local_link_id(peer);

	if (!IS_MLO_DP_MLD_PEER(peer))
		dp_peer_ppdu_delayed_ba_init(peer);

fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return status;
}

/**
 * dp_set_ba_aging_timeout() - set ba aging timeout per AC
 * @txrx_soc: cdp soc handle
 * @ac: Access category
 * @value: timeout value in millisec
 *
 * Return: void
 */
void dp_set_ba_aging_timeout(struct cdp_soc_t *txrx_soc,
			     uint8_t ac, uint32_t value)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	hal_set_ba_aging_timeout(soc->hal_soc, ac, value);
}

/**
 * dp_get_ba_aging_timeout() - get ba aging timeout per AC
 * @txrx_soc: cdp soc handle
 * @ac: access category
 * @value: timeout value in millisec
 *
 * Return: void
 */
void dp_get_ba_aging_timeout(struct cdp_soc_t *txrx_soc,
			     uint8_t ac, uint32_t *value)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;

	hal_get_ba_aging_timeout(soc->hal_soc, ac, value);
}

/**
 * dp_set_pdev_reo_dest() - set the reo destination ring for this pdev
 * @txrx_soc: cdp soc handle
 * @pdev_id: id of physical device object
 * @val: reo destination ring index (1 - 4)
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_set_pdev_reo_dest(struct cdp_soc_t *txrx_soc, uint8_t pdev_id,
		     enum cdp_host_reo_dest_ring val)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)txrx_soc,
						   pdev_id);

	if (pdev) {
		pdev->reo_dest = val;
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_get_pdev_reo_dest() - get the reo destination for this pdev
 * @txrx_soc: cdp soc handle
 * @pdev_id: id of physical device object
 *
 * Return: reo destination ring index
 */
enum cdp_host_reo_dest_ring
dp_get_pdev_reo_dest(struct cdp_soc_t *txrx_soc, uint8_t pdev_id)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)txrx_soc,
						   pdev_id);

	if (pdev)
		return pdev->reo_dest;
	else
		return cdp_host_reo_dest_ring_unknown;
}

void dp_rx_bar_stats_cb(struct dp_soc *soc, void *cb_ctxt,
	union hal_reo_status *reo_status)
{
	struct dp_pdev *pdev = (struct dp_pdev *)cb_ctxt;
	struct hal_reo_queue_status *queue_status = &(reo_status->queue_status);

	if (!dp_check_pdev_exists(soc, pdev)) {
		dp_err_rl("pdev doesn't exist");
		return;
	}

	if (!qdf_atomic_read(&soc->cmn_init_done))
		return;

	if (queue_status->header.status != HAL_REO_CMD_SUCCESS) {
		DP_PRINT_STATS("REO stats failure %d",
			       queue_status->header.status);
		qdf_atomic_set(&(pdev->stats_cmd_complete), 1);
		return;
	}

	pdev->stats.rx.bar_recv_cnt += queue_status->bar_rcvd_cnt;
	qdf_atomic_set(&(pdev->stats_cmd_complete), 1);
}

/**
 * dp_dump_wbm_idle_hptp() - dump wbm idle ring, hw hp tp info.
 * @soc: dp soc.
 * @pdev: dp pdev.
 *
 * Return: None.
 */
void
dp_dump_wbm_idle_hptp(struct dp_soc *soc, struct dp_pdev *pdev)
{
	uint32_t hw_head;
	uint32_t hw_tail;
	struct dp_srng *srng;

	if (!soc) {
		dp_err("soc is NULL");
		return;
	}

	if (!pdev) {
		dp_err("pdev is NULL");
		return;
	}

	srng = &pdev->soc->wbm_idle_link_ring;
	if (!srng) {
		dp_err("wbm_idle_link_ring srng is NULL");
		return;
	}

	hal_get_hw_hptp(soc->hal_soc, srng->hal_srng, &hw_head,
			&hw_tail, WBM_IDLE_LINK);

	dp_debug("WBM_IDLE_LINK: HW hp: %d, HW tp: %d",
		 hw_head, hw_tail);
}

#ifdef WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
static void dp_update_soft_irq_limits(struct dp_soc *soc, uint32_t tx_limit,
				      uint32_t rx_limit)
{
	soc->wlan_cfg_ctx->tx_comp_loop_pkt_limit = tx_limit;
	soc->wlan_cfg_ctx->rx_reap_loop_pkt_limit = rx_limit;
}

#else

static inline
void dp_update_soft_irq_limits(struct dp_soc *soc, uint32_t tx_limit,
			       uint32_t rx_limit)
{
}
#endif /* WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT */

/**
 * dp_display_srng_info() - Dump the srng HP TP info
 * @soc_hdl: CDP Soc handle
 *
 * This function dumps the SW hp/tp values for the important rings.
 * HW hp/tp values are not being dumped, since it can lead to
 * READ NOC error when UMAC is in low power state. MCC does not have
 * device force wake working yet.
 *
 * Return: rings are empty
 */
bool dp_display_srng_info(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	hal_soc_handle_t hal_soc = soc->hal_soc;
	uint32_t hp, tp, i;
	bool ret = true;

	dp_info("SRNG HP-TP data:");
	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		hal_get_sw_hptp(hal_soc, soc->tcl_data_ring[i].hal_srng,
				&tp, &hp);
		dp_info("TCL DATA ring[%d]: hp=0x%x, tp=0x%x", i, hp, tp);

		if (wlan_cfg_get_wbm_ring_num_for_index(soc->wlan_cfg_ctx, i) ==
		    INVALID_WBM_RING_NUM)
			continue;

		hal_get_sw_hptp(hal_soc, soc->tx_comp_ring[i].hal_srng,
				&tp, &hp);
		dp_info("TX comp ring[%d]: hp=0x%x, tp=0x%x", i, hp, tp);
	}

	for (i = 0; i < soc->num_reo_dest_rings; i++) {
		hal_get_sw_hptp(hal_soc, soc->reo_dest_ring[i].hal_srng,
				&tp, &hp);
		if (hp != tp)
			ret = false;

		dp_info("REO DST ring[%d]: hp=0x%x, tp=0x%x", i, hp, tp);
	}

	hal_get_sw_hptp(hal_soc, soc->reo_exception_ring.hal_srng, &tp, &hp);
	dp_info("REO exception ring: hp=0x%x, tp=0x%x", hp, tp);

	hal_get_sw_hptp(hal_soc, soc->rx_rel_ring.hal_srng, &tp, &hp);
	dp_info("WBM RX release ring: hp=0x%x, tp=0x%x", hp, tp);

	hal_get_sw_hptp(hal_soc, soc->wbm_desc_rel_ring.hal_srng, &tp, &hp);
	dp_info("WBM desc release ring: hp=0x%x, tp=0x%x", hp, tp);

	return ret;
}

/**
 * dp_set_pdev_pcp_tid_map_wifi3() - update pcp tid map in pdev
 * @psoc: dp soc handle
 * @pdev_id: id of DP_PDEV handle
 * @pcp: pcp value
 * @tid: tid value passed by the user
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS dp_set_pdev_pcp_tid_map_wifi3(ol_txrx_soc_handle psoc,
					 uint8_t pdev_id,
					 uint8_t pcp, uint8_t tid)
{
	struct dp_soc *soc = (struct dp_soc *)psoc;

	soc->pcp_tid_map[pcp] = tid;

	hal_tx_update_pcp_tid_map(soc->hal_soc, pcp, tid);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_set_vdev_pcp_tid_map_wifi3() - update pcp tid map in vdev
 * @soc_hdl: DP soc handle
 * @vdev_id: id of DP_VDEV handle
 * @pcp: pcp value
 * @tid: tid value passed by the user
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS dp_set_vdev_pcp_tid_map_wifi3(struct cdp_soc_t *soc_hdl,
					 uint8_t vdev_id,
					 uint8_t pcp, uint8_t tid)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	vdev->pcp_tid_map[pcp] = tid;

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

#if defined(FEATURE_RUNTIME_PM) || defined(DP_POWER_SAVE)
QDF_STATUS dp_drain_txrx(struct cdp_soc_t *soc_handle, uint8_t rx_only)
{
	struct dp_soc *soc = (struct dp_soc *)soc_handle;
	uint32_t cur_tx_limit, cur_rx_limit;
	uint32_t budget = 0xffff;
	uint32_t val;
	int i;
	int cpu = dp_srng_get_cpu();
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	cur_tx_limit = soc->wlan_cfg_ctx->tx_comp_loop_pkt_limit;
	cur_rx_limit = soc->wlan_cfg_ctx->rx_reap_loop_pkt_limit;

	/* Temporarily increase soft irq limits when going to drain
	 * the UMAC/LMAC SRNGs and restore them after polling.
	 * Though the budget is on higher side, the TX/RX reaping loops
	 * will not execute longer as both TX and RX would be suspended
	 * by the time this API is called.
	 */
	dp_update_soft_irq_limits(soc, budget, budget);

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		if (rx_only && !soc->intr_ctx[i].rx_ring_mask)
			continue;
		soc->arch_ops.dp_service_srngs(&soc->intr_ctx[i], budget, cpu);
	}

	dp_update_soft_irq_limits(soc, cur_tx_limit, cur_rx_limit);

	status = hif_try_complete_dp_tasks(soc->hif_handle);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to complete DP tasks");
		return status;
	}

	/* Do a dummy read at offset 0; this will ensure all
	 * pendings writes(HP/TP) are flushed before read returns.
	 */
	val = HAL_REG_READ((struct hal_soc *)soc->hal_soc, 0);
	dp_debug("Register value at offset 0: %u", val);

	return status;
}
#endif

#if defined(DP_POWER_SAVE) || defined(FEATURE_RUNTIME_PM)
/**
 * dp_flush_ring_hptp() - Update ring shadow
 *			  register HP/TP address when runtime
 *                        resume
 * @soc: DP soc context
 * @hal_srng: srng
 *
 * Return: None
 */
static void dp_flush_ring_hptp(struct dp_soc *soc, hal_ring_handle_t hal_srng)
{
	if (hal_srng && hal_srng_get_clear_event(hal_srng,
						 HAL_SRNG_FLUSH_EVENT)) {
		/* Acquire the lock */
		hal_srng_access_start(soc->hal_soc, hal_srng);

		hal_srng_access_end(soc->hal_soc, hal_srng);

		hal_srng_set_flush_last_ts(hal_srng);

		dp_debug("flushed");
	}
}

void dp_update_ring_hptp(struct dp_soc *soc, bool force_flush_tx)
{
	 uint8_t i;

	if (force_flush_tx) {
		for (i = 0; i < soc->num_tcl_data_rings; i++) {
			hal_srng_set_event(soc->tcl_data_ring[i].hal_srng,
					   HAL_SRNG_FLUSH_EVENT);
			dp_flush_ring_hptp(soc, soc->tcl_data_ring[i].hal_srng);
		}

		return;
	}

	for (i = 0; i < soc->num_tcl_data_rings; i++)
		dp_flush_ring_hptp(soc, soc->tcl_data_ring[i].hal_srng);

	dp_flush_ring_hptp(soc, soc->reo_cmd_ring.hal_srng);
}
#endif

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
/*
 * dp_flush_tcl_ring() - flush TCL ring hp
 * @pdev: dp pdev
 * @ring_id: TCL ring id
 *
 * Return: 0 on success and error code on failure
 */
int dp_flush_tcl_ring(struct dp_pdev *pdev, int ring_id)
{
	struct dp_soc *soc = pdev->soc;
	hal_ring_handle_t hal_ring_hdl =
			soc->tcl_data_ring[ring_id].hal_srng;
	int ret;

	ret = hal_srng_try_access_start(soc->hal_soc, hal_ring_hdl);
	if (ret)
		return ret;

	ret = hif_rtpm_get(HIF_RTPM_GET_ASYNC, HIF_RTPM_ID_DP);
	if (ret) {
		hal_srng_access_end_reap(soc->hal_soc, hal_ring_hdl);
		hal_srng_set_event(hal_ring_hdl, HAL_SRNG_FLUSH_EVENT);
		hal_srng_inc_flush_cnt(hal_ring_hdl);
		return ret;
	}

	hal_srng_access_end(soc->hal_soc, hal_ring_hdl);
	hif_rtpm_put(HIF_RTPM_PUT_ASYNC, HIF_RTPM_ID_DP);

	return ret;
}
#else
int dp_flush_tcl_ring(struct dp_pdev *pdev, int ring_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_STATS_EXT
/* rx hw stats event wait timeout in ms */
#define DP_REO_STATUS_STATS_TIMEOUT 100

/**
 * dp_rx_hw_stats_cb() - request rx hw stats response callback
 * @soc: soc handle
 * @cb_ctxt: callback context
 * @reo_status: reo command response status
 *
 * Return: None
 */
static void dp_rx_hw_stats_cb(struct dp_soc *soc, void *cb_ctxt,
			      union hal_reo_status *reo_status)
{
	struct hal_reo_queue_status *queue_status = &reo_status->queue_status;
	bool is_query_timeout;

	qdf_spin_lock_bh(&soc->rx_hw_stats_lock);
	is_query_timeout = soc->rx_hw_stats->is_query_timeout;
	/* free the cb_ctxt if all pending tid stats query is received */
	if (qdf_atomic_dec_and_test(&soc->rx_hw_stats->pending_tid_stats_cnt)) {
		if (!is_query_timeout) {
			qdf_event_set(&soc->rx_hw_stats_event);
			soc->is_last_stats_ctx_init = false;
		}

		qdf_mem_free(soc->rx_hw_stats);
		soc->rx_hw_stats = NULL;
	}

	if (queue_status->header.status != HAL_REO_CMD_SUCCESS) {
		dp_info("REO stats failure %d",
			queue_status->header.status);
		qdf_spin_unlock_bh(&soc->rx_hw_stats_lock);
		return;
	}

	if (!is_query_timeout) {
		soc->ext_stats.rx_mpdu_received +=
					queue_status->mpdu_frms_cnt;
		soc->ext_stats.rx_mpdu_missed +=
					queue_status->hole_cnt;
	}
	qdf_spin_unlock_bh(&soc->rx_hw_stats_lock);
}

/**
 * dp_request_rx_hw_stats() - request rx hardware stats
 * @soc_hdl: soc handle
 * @vdev_id: vdev id
 *
 * Return: None
 */
QDF_STATUS
dp_request_rx_hw_stats(struct cdp_soc_t *soc_hdl, uint8_t vdev_id)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	struct dp_peer *peer = NULL;
	QDF_STATUS status;
	int rx_stats_sent_cnt = 0;
	uint32_t last_rx_mpdu_received;
	uint32_t last_rx_mpdu_missed;

	if (soc->rx_hw_stats) {
		dp_err_rl("Stats already requested");
		status = QDF_STATUS_E_ALREADY;
		goto out;
	}

	if (!vdev) {
		dp_err("vdev is null for vdev_id: %u", vdev_id);
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	peer = dp_vdev_bss_peer_ref_n_get(soc, vdev, DP_MOD_ID_CDP);

	if (!peer) {
		dp_err("Peer is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	soc->rx_hw_stats = qdf_mem_malloc(sizeof(*soc->rx_hw_stats));

	if (!soc->rx_hw_stats) {
		dp_err("malloc failed for hw stats structure");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	qdf_event_reset(&soc->rx_hw_stats_event);
	qdf_spin_lock_bh(&soc->rx_hw_stats_lock);
	/* save the last soc cumulative stats and reset it to 0 */
	last_rx_mpdu_received = soc->ext_stats.rx_mpdu_received;
	last_rx_mpdu_missed = soc->ext_stats.rx_mpdu_missed;
	soc->ext_stats.rx_mpdu_received = 0;
	soc->ext_stats.rx_mpdu_missed = 0;

	dp_debug("HW stats query start");
	rx_stats_sent_cnt =
		dp_peer_rxtid_stats(peer, dp_rx_hw_stats_cb, soc->rx_hw_stats);
	if (!rx_stats_sent_cnt) {
		dp_err("no tid stats sent successfully");
		qdf_mem_free(soc->rx_hw_stats);
		soc->rx_hw_stats = NULL;
		qdf_spin_unlock_bh(&soc->rx_hw_stats_lock);
		status = QDF_STATUS_E_INVAL;
		goto out;
	}
	qdf_atomic_set(&soc->rx_hw_stats->pending_tid_stats_cnt,
		       rx_stats_sent_cnt);
	soc->rx_hw_stats->is_query_timeout = false;
	soc->is_last_stats_ctx_init = true;
	qdf_spin_unlock_bh(&soc->rx_hw_stats_lock);

	status = qdf_wait_single_event(&soc->rx_hw_stats_event,
				       DP_REO_STATUS_STATS_TIMEOUT);
	dp_debug("HW stats query end with %d", rx_stats_sent_cnt);

	qdf_spin_lock_bh(&soc->rx_hw_stats_lock);
	if (status != QDF_STATUS_SUCCESS) {
		if (soc->rx_hw_stats) {
			dp_info("partial rx hw stats event collected with %d",
				qdf_atomic_read(
				  &soc->rx_hw_stats->pending_tid_stats_cnt));
			if (soc->is_last_stats_ctx_init)
				soc->rx_hw_stats->is_query_timeout = true;
		}

		/*
		 * If query timeout happened, use the last saved stats
		 * for this time query.
		 */
		soc->ext_stats.rx_mpdu_received = last_rx_mpdu_received;
		soc->ext_stats.rx_mpdu_missed = last_rx_mpdu_missed;
		DP_STATS_INC(soc, rx.rx_hw_stats_timeout, 1);

	}
	qdf_spin_unlock_bh(&soc->rx_hw_stats_lock);

out:
	if (peer)
		dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	DP_STATS_INC(soc, rx.rx_hw_stats_requested, 1);

	return status;
}

/**
 * dp_reset_rx_hw_ext_stats() - Reset rx hardware ext stats
 * @soc_hdl: soc handle
 *
 * Return: None
 */
void dp_reset_rx_hw_ext_stats(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	soc->ext_stats.rx_mpdu_received = 0;
	soc->ext_stats.rx_mpdu_missed = 0;
}
#endif /* WLAN_FEATURE_STATS_EXT */

uint32_t dp_get_tx_rings_grp_bitmap(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;

	return soc->wlan_cfg_ctx->tx_rings_grp_bitmap;
}

void dp_soc_set_txrx_ring_map(struct dp_soc *soc)
{
	uint32_t i;

	for (i = 0; i < WLAN_CFG_INT_NUM_CONTEXTS; i++) {
		soc->tx_ring_map[i] = dp_cpu_ring_map[DP_NSS_DEFAULT_MAP][i];
	}
}

qdf_export_symbol(dp_soc_set_txrx_ring_map);

static void dp_soc_cfg_dump(struct dp_soc *soc, uint32_t target_type)
{
	dp_init_info("DP soc Dump for Target = %d", target_type);
	dp_init_info("ast_override_support = %d da_war_enabled = %d",
		     soc->ast_override_support, soc->da_war_enabled);

	wlan_cfg_dp_soc_ctx_dump(soc->wlan_cfg_ctx);
}

/**
 * dp_soc_cfg_init() - initialize target specific configuration
 *		       during dp_soc_init
 * @soc: dp soc handle
 */
static void dp_soc_cfg_init(struct dp_soc *soc)
{
	uint32_t target_type;

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_QCA6290:
		wlan_cfg_set_reo_dst_ring_size(soc->wlan_cfg_ctx,
					       REO_DST_RING_SIZE_QCA6290);
		soc->ast_override_support = 1;
		soc->da_war_enabled = false;
		break;
	case TARGET_TYPE_QCA6390:
	case TARGET_TYPE_QCA6490:
	case TARGET_TYPE_QCA6750:
		wlan_cfg_set_reo_dst_ring_size(soc->wlan_cfg_ctx,
					       REO_DST_RING_SIZE_QCA6290);
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, true);
		soc->ast_override_support = 1;
		if (soc->cdp_soc.ol_ops->get_con_mode &&
		    soc->cdp_soc.ol_ops->get_con_mode() ==
		    QDF_GLOBAL_MONITOR_MODE) {
			int int_ctx;

			for (int_ctx = 0; int_ctx < WLAN_CFG_INT_NUM_CONTEXTS; int_ctx++) {
				soc->wlan_cfg_ctx->int_rx_ring_mask[int_ctx] = 0;
				soc->wlan_cfg_ctx->int_rxdma2host_ring_mask[int_ctx] = 0;
			}
		}
		soc->wlan_cfg_ctx->rxdma1_enable = 0;
		break;
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
	case TARGET_TYPE_PEACH:
		soc->ast_override_support = 1;
		soc->per_tid_basize_max_tid = 8;

		if (soc->cdp_soc.ol_ops->get_con_mode &&
		    soc->cdp_soc.ol_ops->get_con_mode() ==
		    QDF_GLOBAL_MONITOR_MODE) {
			int int_ctx;

			for (int_ctx = 0; int_ctx < WLAN_CFG_INT_NUM_CONTEXTS;
			     int_ctx++) {
				soc->wlan_cfg_ctx->int_rx_ring_mask[int_ctx] = 0;
				if (dp_is_monitor_mode_using_poll(soc))
					soc->wlan_cfg_ctx->int_rxdma2host_ring_mask[int_ctx] = 0;
			}
		}

		soc->wlan_cfg_ctx->rxdma1_enable = 0;
		soc->wlan_cfg_ctx->num_rxdma_dst_rings_per_pdev = 1;
		break;
	case TARGET_TYPE_QCA8074:
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, true);
		soc->da_war_enabled = true;
		soc->is_rx_fse_full_cache_invalidate_war_enabled = true;
		break;
	case TARGET_TYPE_QCA8074V2:
	case TARGET_TYPE_QCA6018:
	case TARGET_TYPE_QCA9574:
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, false);
		soc->ast_override_support = 1;
		soc->per_tid_basize_max_tid = 8;
		soc->num_hw_dscp_tid_map = HAL_MAX_HW_DSCP_TID_V2_MAPS;
		soc->da_war_enabled = false;
		soc->is_rx_fse_full_cache_invalidate_war_enabled = true;
		break;
	case TARGET_TYPE_QCN9000:
		soc->ast_override_support = 1;
		soc->da_war_enabled = false;
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, false);
		soc->per_tid_basize_max_tid = 8;
		soc->num_hw_dscp_tid_map = HAL_MAX_HW_DSCP_TID_V2_MAPS;
		soc->lmac_polled_mode = 0;
		soc->wbm_release_desc_rx_sg_support = 1;
		soc->is_rx_fse_full_cache_invalidate_war_enabled = true;
		break;
	case TARGET_TYPE_QCA5018:
	case TARGET_TYPE_QCN6122:
	case TARGET_TYPE_QCN9160:
		soc->ast_override_support = 1;
		soc->da_war_enabled = false;
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, false);
		soc->per_tid_basize_max_tid = 8;
		soc->num_hw_dscp_tid_map = HAL_MAX_HW_DSCP_TID_MAPS_11AX;
		soc->disable_mac1_intr = 1;
		soc->disable_mac2_intr = 1;
		soc->wbm_release_desc_rx_sg_support = 1;
		break;
	case TARGET_TYPE_QCN9224:
		soc->umac_reset_supported = true;
		soc->ast_override_support = 1;
		soc->da_war_enabled = false;
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, false);
		soc->per_tid_basize_max_tid = 8;
		soc->wbm_release_desc_rx_sg_support = 1;
		soc->rxdma2sw_rings_not_supported = 1;
		soc->wbm_sg_last_msdu_war = 1;
		soc->ast_offload_support = AST_OFFLOAD_ENABLE_STATUS;
		soc->mec_fw_offload = FW_MEC_FW_OFFLOAD_ENABLED;
		soc->num_hw_dscp_tid_map = HAL_MAX_HW_DSCP_TID_V2_MAPS;
		wlan_cfg_set_txmon_hw_support(soc->wlan_cfg_ctx, true);
		soc->host_ast_db_enable = cfg_get(soc->ctrl_psoc,
						  CFG_DP_HOST_AST_DB_ENABLE);
		soc->features.wds_ext_ast_override_enable = true;
		break;
	case TARGET_TYPE_QCA5332:
	case TARGET_TYPE_QCN6432:
		soc->umac_reset_supported = true;
		soc->ast_override_support = 1;
		soc->da_war_enabled = false;
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, false);
		soc->per_tid_basize_max_tid = 8;
		soc->wbm_release_desc_rx_sg_support = 1;
		soc->rxdma2sw_rings_not_supported = 1;
		soc->wbm_sg_last_msdu_war = 1;
		soc->ast_offload_support = AST_OFFLOAD_ENABLE_STATUS;
		soc->mec_fw_offload = FW_MEC_FW_OFFLOAD_ENABLED;
		soc->num_hw_dscp_tid_map = HAL_MAX_HW_DSCP_TID_V2_MAPS_5332;
		wlan_cfg_set_txmon_hw_support(soc->wlan_cfg_ctx, true);
		soc->host_ast_db_enable = cfg_get(soc->ctrl_psoc,
						  CFG_DP_HOST_AST_DB_ENABLE);
		soc->features.wds_ext_ast_override_enable = true;
		break;
	default:
		qdf_print("%s: Unknown tgt type %d\n", __func__, target_type);
		qdf_assert_always(0);
		break;
	}
	dp_soc_cfg_dump(soc, target_type);
}

/**
 * dp_soc_get_ap_mld_mode() - store ap mld mode from ini
 * @soc: Opaque DP SOC handle
 *
 * Return: none
 */
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
static inline void dp_soc_get_ap_mld_mode(struct dp_soc *soc)
{
	if (soc->cdp_soc.ol_ops->get_dp_cfg_param) {
		soc->mld_mode_ap =
		soc->cdp_soc.ol_ops->get_dp_cfg_param(soc->ctrl_psoc,
					CDP_CFG_MLD_NETDEV_MODE_AP);
	}
	dp_info("DP mld_mode_ap-%u\n", soc->mld_mode_ap);
}
#else
static inline void dp_soc_get_ap_mld_mode(struct dp_soc *soc)
{
	(void)soc;
}
#endif

#ifdef QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT
/**
 * dp_soc_hw_txrx_stats_init() - Initialize hw_txrx_stats_en in dp_soc
 * @soc: Datapath soc handle
 *
 * Return: none
 */
static inline
void dp_soc_hw_txrx_stats_init(struct dp_soc *soc)
{
	soc->hw_txrx_stats_en =
		wlan_cfg_get_vdev_stats_hw_offload_config(soc->wlan_cfg_ctx);
}
#else
static inline
void dp_soc_hw_txrx_stats_init(struct dp_soc *soc)
{
	soc->hw_txrx_stats_en = 0;
}
#endif

/**
 * dp_soc_init() - Initialize txrx SOC
 * @soc: Opaque DP SOC handle
 * @htc_handle: Opaque HTC handle
 * @hif_handle: Opaque HIF handle
 *
 * Return: DP SOC handle on success, NULL on failure
 */
void *dp_soc_init(struct dp_soc *soc, HTC_HANDLE htc_handle,
		  struct hif_opaque_softc *hif_handle)
{
	struct htt_soc *htt_soc = (struct htt_soc *)soc->htt_handle;
	bool is_monitor_mode = false;
	uint8_t i;
	int num_dp_msi;
	bool ppeds_attached = false;

	htt_soc = htt_soc_attach(soc, htc_handle);
	if (!htt_soc)
		goto fail1;

	soc->htt_handle = htt_soc;

	if (htt_soc_htc_prealloc(htt_soc) != QDF_STATUS_SUCCESS)
		goto fail2;

	htt_set_htc_handle(htt_soc, htc_handle);

	dp_soc_cfg_init(soc);

	dp_monitor_soc_cfg_init(soc);
	/* Reset/Initialize wbm sg list and flags */
	dp_rx_wbm_sg_list_reset(soc);

	/* Note: Any SRNG ring initialization should happen only after
	 * Interrupt mode is set and followed by filling up the
	 * interrupt mask. IT SHOULD ALWAYS BE IN THIS ORDER.
	 */
	dp_soc_set_interrupt_mode(soc);
	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() ==
	    QDF_GLOBAL_MONITOR_MODE) {
		is_monitor_mode = true;
		soc->curr_rx_pkt_tlv_size = soc->rx_mon_pkt_tlv_size;
	} else {
		soc->curr_rx_pkt_tlv_size = soc->rx_pkt_tlv_size;
	}

	num_dp_msi = dp_get_num_msi_available(soc, soc->intr_mode);
	if (num_dp_msi < 0) {
		dp_init_err("%pK: dp_interrupt assignment failed", soc);
		goto fail3;
	}

	if (soc->arch_ops.ppeds_handle_attached)
		ppeds_attached = soc->arch_ops.ppeds_handle_attached(soc);

	wlan_cfg_fill_interrupt_mask(soc->wlan_cfg_ctx, num_dp_msi,
				     soc->intr_mode, is_monitor_mode,
				     ppeds_attached,
				     soc->umac_reset_supported);

	/* initialize WBM_IDLE_LINK ring */
	if (dp_hw_link_desc_ring_init(soc)) {
		dp_init_err("%pK: dp_hw_link_desc_ring_init failed", soc);
		goto fail3;
	}

	dp_link_desc_ring_replenish(soc, WLAN_INVALID_PDEV_ID);

	if (dp_soc_srng_init(soc)) {
		dp_init_err("%pK: dp_soc_srng_init failed", soc);
		goto fail4;
	}

	if (htt_soc_initialize(soc->htt_handle, soc->ctrl_psoc,
			       htt_get_htc_handle(htt_soc),
			       soc->hal_soc, soc->osdev) == NULL)
		goto fail5;

	/* Initialize descriptors in TCL Rings */
	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		hal_tx_init_data_ring(soc->hal_soc,
				      soc->tcl_data_ring[i].hal_srng);
	}

	if (dp_soc_tx_desc_sw_pools_init(soc)) {
		dp_init_err("%pK: dp_tx_soc_attach failed", soc);
		goto fail6;
	}

	if (soc->arch_ops.txrx_soc_ppeds_start) {
		if (soc->arch_ops.txrx_soc_ppeds_start(soc)) {
			dp_init_err("%pK: ppeds start failed", soc);
			goto fail7;
		}
	}

	wlan_cfg_set_rx_hash(soc->wlan_cfg_ctx,
			     cfg_get(soc->ctrl_psoc, CFG_DP_RX_HASH));
#ifdef WLAN_SUPPORT_RX_FLOW_TAG
	wlan_cfg_set_rx_rr(soc->wlan_cfg_ctx,
			   cfg_get(soc->ctrl_psoc, CFG_DP_RX_RR));
#endif
	soc->cce_disable = false;
	soc->max_ast_ageout_count = MAX_AST_AGEOUT_COUNT;

	soc->sta_mode_search_policy = DP_TX_ADDR_SEARCH_ADDR_POLICY;
	qdf_mem_zero(&soc->vdev_id_map, sizeof(soc->vdev_id_map));
	qdf_spinlock_create(&soc->vdev_map_lock);
	qdf_atomic_init(&soc->num_tx_outstanding);
	qdf_atomic_init(&soc->num_tx_exception);
	soc->num_tx_allowed =
		wlan_cfg_get_dp_soc_tx_device_limit(soc->wlan_cfg_ctx);
	soc->num_tx_spl_allowed =
		wlan_cfg_get_dp_soc_tx_spl_device_limit(soc->wlan_cfg_ctx);
	soc->num_reg_tx_allowed = soc->num_tx_allowed - soc->num_tx_spl_allowed;
	if (soc->cdp_soc.ol_ops->get_dp_cfg_param) {
		int ret = soc->cdp_soc.ol_ops->get_dp_cfg_param(soc->ctrl_psoc,
				CDP_CFG_MAX_PEER_ID);

		if (ret != -EINVAL)
			wlan_cfg_set_max_peer_id(soc->wlan_cfg_ctx, ret);

		ret = soc->cdp_soc.ol_ops->get_dp_cfg_param(soc->ctrl_psoc,
				CDP_CFG_CCE_DISABLE);
		if (ret == 1)
			soc->cce_disable = true;
	}

	/*
	 * Skip registering hw ring interrupts for WMAC2 on IPQ6018
	 * and IPQ5018 WMAC2 is not there in these platforms.
	 */
	if (hal_get_target_type(soc->hal_soc) == TARGET_TYPE_QCA6018 ||
	    soc->disable_mac2_intr)
		dp_soc_disable_unused_mac_intr_mask(soc, 0x2);

	/*
	 * Skip registering hw ring interrupts for WMAC1 on IPQ5018
	 * WMAC1 is not there in this platform.
	 */
	if (soc->disable_mac1_intr)
		dp_soc_disable_unused_mac_intr_mask(soc, 0x1);

	/* setup the global rx defrag waitlist */
	TAILQ_INIT(&soc->rx.defrag.waitlist);
	soc->rx.defrag.timeout_ms =
		wlan_cfg_get_rx_defrag_min_timeout(soc->wlan_cfg_ctx);
	soc->rx.defrag.next_flush_ms = 0;
	soc->rx.flags.defrag_timeout_check =
		wlan_cfg_get_defrag_timeout_check(soc->wlan_cfg_ctx);
	qdf_spinlock_create(&soc->rx.defrag.defrag_lock);

	dp_monitor_soc_init(soc);

	qdf_atomic_set(&soc->cmn_init_done, 1);

	qdf_nbuf_queue_init(&soc->htt_stats.msg);

	qdf_spinlock_create(&soc->ast_lock);
	dp_peer_mec_spinlock_create(soc);

	qdf_spinlock_create(&soc->reo_desc_freelist_lock);
	qdf_list_create(&soc->reo_desc_freelist, REO_DESC_FREELIST_SIZE);
	INIT_RX_HW_STATS_LOCK(soc);

	qdf_nbuf_queue_init(&soc->invalid_buf_queue);
	/* fill the tx/rx cpu ring map*/
	dp_soc_set_txrx_ring_map(soc);

	TAILQ_INIT(&soc->inactive_peer_list);
	qdf_spinlock_create(&soc->inactive_peer_list_lock);
	TAILQ_INIT(&soc->inactive_vdev_list);
	qdf_spinlock_create(&soc->inactive_vdev_list_lock);
	qdf_spinlock_create(&soc->htt_stats.lock);
	/* initialize work queue for stats processing */
	qdf_create_work(0, &soc->htt_stats.work, htt_t2h_stats_handler, soc);

	dp_reo_desc_deferred_freelist_create(soc);

	dp_info("Mem stats: DMA = %u HEAP = %u SKB = %u",
		qdf_dma_mem_stats_read(),
		qdf_heap_mem_stats_read(),
		qdf_skb_total_mem_stats_read());

	soc->vdev_stats_id_map = 0;

	dp_soc_hw_txrx_stats_init(soc);

	dp_soc_get_ap_mld_mode(soc);

	return soc;
fail7:
	dp_soc_tx_desc_sw_pools_deinit(soc);
fail6:
	htt_soc_htc_dealloc(soc->htt_handle);
fail5:
	dp_soc_srng_deinit(soc);
fail4:
	dp_hw_link_desc_ring_deinit(soc);
fail3:
	htt_htc_pkt_pool_free(htt_soc);
fail2:
	htt_soc_detach(htt_soc);
fail1:
	return NULL;
}

#ifndef WLAN_DP_DISABLE_TCL_CMD_CRED_SRNG
static inline QDF_STATUS dp_soc_tcl_cmd_cred_srng_init(struct dp_soc *soc)
{
	QDF_STATUS status;

	if (soc->init_tcl_cmd_cred_ring) {
		status =  dp_srng_init(soc, &soc->tcl_cmd_credit_ring,
				       TCL_CMD_CREDIT, 0, 0);
		if (QDF_IS_STATUS_ERROR(status))
			return status;

		wlan_minidump_log(soc->tcl_cmd_credit_ring.base_vaddr_unaligned,
				  soc->tcl_cmd_credit_ring.alloc_size,
				  soc->ctrl_psoc,
				  WLAN_MD_DP_SRNG_TCL_CMD,
				  "wbm_desc_rel_ring");
	}

	return QDF_STATUS_SUCCESS;
}

static inline void dp_soc_tcl_cmd_cred_srng_deinit(struct dp_soc *soc)
{
	if (soc->init_tcl_cmd_cred_ring) {
		wlan_minidump_remove(soc->tcl_cmd_credit_ring.base_vaddr_unaligned,
				     soc->tcl_cmd_credit_ring.alloc_size,
				     soc->ctrl_psoc, WLAN_MD_DP_SRNG_TCL_CMD,
				     "wbm_desc_rel_ring");
		dp_srng_deinit(soc, &soc->tcl_cmd_credit_ring,
			       TCL_CMD_CREDIT, 0);
	}
}

static inline QDF_STATUS dp_soc_tcl_cmd_cred_srng_alloc(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;
	uint32_t entries;
	QDF_STATUS status;

	entries = wlan_cfg_get_dp_soc_tcl_cmd_credit_ring_size(soc_cfg_ctx);
	if (soc->init_tcl_cmd_cred_ring) {
		status = dp_srng_alloc(soc, &soc->tcl_cmd_credit_ring,
				       TCL_CMD_CREDIT, entries, 0);
		if (QDF_IS_STATUS_ERROR(status))
			return status;
	}

	return QDF_STATUS_SUCCESS;
}

static inline void dp_soc_tcl_cmd_cred_srng_free(struct dp_soc *soc)
{
	if (soc->init_tcl_cmd_cred_ring)
		dp_srng_free(soc, &soc->tcl_cmd_credit_ring);
}

inline void dp_tx_init_cmd_credit_ring(struct dp_soc *soc)
{
	if (soc->init_tcl_cmd_cred_ring)
		hal_tx_init_cmd_credit_ring(soc->hal_soc,
					    soc->tcl_cmd_credit_ring.hal_srng);
}
#else
static inline QDF_STATUS dp_soc_tcl_cmd_cred_srng_init(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_soc_tcl_cmd_cred_srng_deinit(struct dp_soc *soc)
{
}

static inline QDF_STATUS dp_soc_tcl_cmd_cred_srng_alloc(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_soc_tcl_cmd_cred_srng_free(struct dp_soc *soc)
{
}

inline void dp_tx_init_cmd_credit_ring(struct dp_soc *soc)
{
}
#endif

#ifndef WLAN_DP_DISABLE_TCL_STATUS_SRNG
static inline QDF_STATUS dp_soc_tcl_status_srng_init(struct dp_soc *soc)
{
	QDF_STATUS status;

	status =  dp_srng_init(soc, &soc->tcl_status_ring, TCL_STATUS, 0, 0);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wlan_minidump_log(soc->tcl_status_ring.base_vaddr_unaligned,
			  soc->tcl_status_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_TCL_STATUS,
			  "wbm_desc_rel_ring");

	return QDF_STATUS_SUCCESS;
}

static inline void dp_soc_tcl_status_srng_deinit(struct dp_soc *soc)
{
	wlan_minidump_remove(soc->tcl_status_ring.base_vaddr_unaligned,
			     soc->tcl_status_ring.alloc_size,
			     soc->ctrl_psoc, WLAN_MD_DP_SRNG_TCL_STATUS,
			     "wbm_desc_rel_ring");
	dp_srng_deinit(soc, &soc->tcl_status_ring, TCL_STATUS, 0);
}

static inline QDF_STATUS dp_soc_tcl_status_srng_alloc(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;
	uint32_t entries;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	entries = wlan_cfg_get_dp_soc_tcl_status_ring_size(soc_cfg_ctx);
	status = dp_srng_alloc(soc, &soc->tcl_status_ring,
			       TCL_STATUS, entries, 0);

	return status;
}

static inline void dp_soc_tcl_status_srng_free(struct dp_soc *soc)
{
	dp_srng_free(soc, &soc->tcl_status_ring);
}
#else
static inline QDF_STATUS dp_soc_tcl_status_srng_init(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_soc_tcl_status_srng_deinit(struct dp_soc *soc)
{
}

static inline QDF_STATUS dp_soc_tcl_status_srng_alloc(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_soc_tcl_status_srng_free(struct dp_soc *soc)
{
}
#endif

/**
 * dp_soc_srng_deinit() - de-initialize soc srng rings
 * @soc: Datapath soc handle
 *
 */
void dp_soc_srng_deinit(struct dp_soc *soc)
{
	uint32_t i;

	if (soc->arch_ops.txrx_soc_srng_deinit)
		soc->arch_ops.txrx_soc_srng_deinit(soc);

	/* Free the ring memories */
	/* Common rings */
	wlan_minidump_remove(soc->wbm_desc_rel_ring.base_vaddr_unaligned,
			     soc->wbm_desc_rel_ring.alloc_size,
			     soc->ctrl_psoc, WLAN_MD_DP_SRNG_WBM_DESC_REL,
			     "wbm_desc_rel_ring");
	dp_srng_deinit(soc, &soc->wbm_desc_rel_ring, SW2WBM_RELEASE, 0);
	dp_ssr_dump_srng_unregister("wbm_desc_rel_ring", -1);

	/* Tx data rings */
	for (i = 0; i < soc->num_tcl_data_rings; i++)
		dp_deinit_tx_pair_by_index(soc, i);

	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		dp_deinit_tx_pair_by_index(soc, IPA_TCL_DATA_RING_IDX);
		dp_ipa_deinit_alt_tx_ring(soc);
	}

	/* TCL command and status rings */
	dp_soc_tcl_cmd_cred_srng_deinit(soc);
	dp_soc_tcl_status_srng_deinit(soc);

	for (i = 0; i < soc->num_reo_dest_rings; i++) {
		/* TODO: Get number of rings and ring sizes
		 * from wlan_cfg
		 */
		dp_ssr_dump_srng_unregister("reo_dest_ring", i);
		wlan_minidump_remove(soc->reo_dest_ring[i].base_vaddr_unaligned,
				     soc->reo_dest_ring[i].alloc_size,
				     soc->ctrl_psoc, WLAN_MD_DP_SRNG_REO_DEST,
				     "reo_dest_ring");
		dp_srng_deinit(soc, &soc->reo_dest_ring[i], REO_DST, i);
	}

	dp_ssr_dump_srng_unregister("reo_reinject_ring", -1);
	/* REO reinjection ring */
	wlan_minidump_remove(soc->reo_reinject_ring.base_vaddr_unaligned,
			     soc->reo_reinject_ring.alloc_size,
			     soc->ctrl_psoc, WLAN_MD_DP_SRNG_REO_REINJECT,
			     "reo_reinject_ring");
	dp_srng_deinit(soc, &soc->reo_reinject_ring, REO_REINJECT, 0);

	dp_ssr_dump_srng_unregister("rx_rel_ring", -1);
	/* Rx release ring */
	wlan_minidump_remove(soc->rx_rel_ring.base_vaddr_unaligned,
			     soc->rx_rel_ring.alloc_size,
			     soc->ctrl_psoc, WLAN_MD_DP_SRNG_RX_REL,
			     "reo_release_ring");
	dp_srng_deinit(soc, &soc->rx_rel_ring, WBM2SW_RELEASE, 0);

	/* Rx exception ring */
	/* TODO: Better to store ring_type and ring_num in
	 * dp_srng during setup
	 */
	dp_ssr_dump_srng_unregister("reo_exception_ring", -1);
	wlan_minidump_remove(soc->reo_exception_ring.base_vaddr_unaligned,
			     soc->reo_exception_ring.alloc_size,
			     soc->ctrl_psoc, WLAN_MD_DP_SRNG_REO_EXCEPTION,
			     "reo_exception_ring");
	dp_srng_deinit(soc, &soc->reo_exception_ring, REO_EXCEPTION, 0);

	/* REO command and status rings */
	dp_ssr_dump_srng_unregister("reo_cmd_ring", -1);
	wlan_minidump_remove(soc->reo_cmd_ring.base_vaddr_unaligned,
			     soc->reo_cmd_ring.alloc_size,
			     soc->ctrl_psoc, WLAN_MD_DP_SRNG_REO_CMD,
			     "reo_cmd_ring");
	dp_srng_deinit(soc, &soc->reo_cmd_ring, REO_CMD, 0);
	dp_ssr_dump_srng_unregister("reo_status_ring", -1);
	wlan_minidump_remove(soc->reo_status_ring.base_vaddr_unaligned,
			     soc->reo_status_ring.alloc_size,
			     soc->ctrl_psoc, WLAN_MD_DP_SRNG_REO_STATUS,
			     "reo_status_ring");
	dp_srng_deinit(soc, &soc->reo_status_ring, REO_STATUS, 0);
}

/**
 * dp_soc_srng_init() - Initialize soc level srng rings
 * @soc: Datapath soc handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS dp_soc_srng_init(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	uint8_t i;
	uint8_t wbm2_sw_rx_rel_ring_id;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	dp_enable_verbose_debug(soc);

	/* WBM descriptor release ring */
	if (dp_srng_init(soc, &soc->wbm_desc_rel_ring, SW2WBM_RELEASE, 0, 0)) {
		dp_init_err("%pK: dp_srng_init failed for wbm_desc_rel_ring", soc);
		goto fail1;
	}
	dp_ssr_dump_srng_register("wbm_desc_rel_ring",
				  &soc->wbm_desc_rel_ring, -1);

	wlan_minidump_log(soc->wbm_desc_rel_ring.base_vaddr_unaligned,
			  soc->wbm_desc_rel_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_WBM_DESC_REL,
			  "wbm_desc_rel_ring");

	/* TCL command and status rings */
	if (dp_soc_tcl_cmd_cred_srng_init(soc)) {
		dp_init_err("%pK: dp_srng_init failed for tcl_cmd_ring", soc);
		goto fail1;
	}

	if (dp_soc_tcl_status_srng_init(soc)) {
		dp_init_err("%pK: dp_srng_init failed for tcl_status_ring", soc);
		goto fail1;
	}

	/* REO reinjection ring */
	if (dp_srng_init(soc, &soc->reo_reinject_ring, REO_REINJECT, 0, 0)) {
		dp_init_err("%pK: dp_srng_init failed for reo_reinject_ring", soc);
		goto fail1;
	}
	dp_ssr_dump_srng_register("reo_reinject_ring",
				  &soc->reo_reinject_ring, -1);

	wlan_minidump_log(soc->reo_reinject_ring.base_vaddr_unaligned,
			  soc->reo_reinject_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_REO_REINJECT,
			  "reo_reinject_ring");

	wbm2_sw_rx_rel_ring_id = wlan_cfg_get_rx_rel_ring_id(soc_cfg_ctx);
	/* Rx release ring */
	if (dp_srng_init(soc, &soc->rx_rel_ring, WBM2SW_RELEASE,
			 wbm2_sw_rx_rel_ring_id, 0)) {
		dp_init_err("%pK: dp_srng_init failed for rx_rel_ring", soc);
		goto fail1;
	}
	dp_ssr_dump_srng_register("rx_rel_ring", &soc->rx_rel_ring, -1);

	wlan_minidump_log(soc->rx_rel_ring.base_vaddr_unaligned,
			  soc->rx_rel_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_RX_REL,
			  "reo_release_ring");

	/* Rx exception ring */
	if (dp_srng_init(soc, &soc->reo_exception_ring,
			 REO_EXCEPTION, 0, MAX_REO_DEST_RINGS)) {
		dp_init_err("%pK: dp_srng_init failed - reo_exception", soc);
		goto fail1;
	}
	dp_ssr_dump_srng_register("reo_exception_ring",
				  &soc->reo_exception_ring, -1);

	wlan_minidump_log(soc->reo_exception_ring.base_vaddr_unaligned,
			  soc->reo_exception_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_REO_EXCEPTION,
			  "reo_exception_ring");

	/* REO command and status rings */
	if (dp_srng_init(soc, &soc->reo_cmd_ring, REO_CMD, 0, 0)) {
		dp_init_err("%pK: dp_srng_init failed for reo_cmd_ring", soc);
		goto fail1;
	}
	dp_ssr_dump_srng_register("reo_cmd_ring", &soc->reo_cmd_ring, -1);

	wlan_minidump_log(soc->reo_cmd_ring.base_vaddr_unaligned,
			  soc->reo_cmd_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_REO_CMD,
			  "reo_cmd_ring");

	hal_reo_init_cmd_ring(soc->hal_soc, soc->reo_cmd_ring.hal_srng);
	TAILQ_INIT(&soc->rx.reo_cmd_list);
	qdf_spinlock_create(&soc->rx.reo_cmd_lock);

	if (dp_srng_init(soc, &soc->reo_status_ring, REO_STATUS, 0, 0)) {
		dp_init_err("%pK: dp_srng_init failed for reo_status_ring", soc);
		goto fail1;
	}
	dp_ssr_dump_srng_register("reo_status_ring", &soc->reo_status_ring, -1);

	wlan_minidump_log(soc->reo_status_ring.base_vaddr_unaligned,
			  soc->reo_status_ring.alloc_size,
			  soc->ctrl_psoc,
			  WLAN_MD_DP_SRNG_REO_STATUS,
			  "reo_status_ring");

	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		if (dp_init_tx_ring_pair_by_index(soc, i))
			goto fail1;
	}

	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		if (dp_init_tx_ring_pair_by_index(soc, IPA_TCL_DATA_RING_IDX))
			goto fail1;

		if (dp_ipa_init_alt_tx_ring(soc))
			goto fail1;
	}

	dp_create_ext_stats_event(soc);

	for (i = 0; i < soc->num_reo_dest_rings; i++) {
		/* Initialize REO destination ring */
		if (dp_srng_init(soc, &soc->reo_dest_ring[i], REO_DST, i, 0)) {
			dp_init_err("%pK: dp_srng_init failed for reo_dest_ringn", soc);
			goto fail1;
		}

		dp_ssr_dump_srng_register("reo_dest_ring",
					  &soc->reo_dest_ring[i], i);
		wlan_minidump_log(soc->reo_dest_ring[i].base_vaddr_unaligned,
				  soc->reo_dest_ring[i].alloc_size,
				  soc->ctrl_psoc,
				  WLAN_MD_DP_SRNG_REO_DEST,
				  "reo_dest_ring");
	}

	if (soc->arch_ops.txrx_soc_srng_init) {
		if (soc->arch_ops.txrx_soc_srng_init(soc)) {
			dp_init_err("%pK: dp_srng_init failed for arch rings",
				    soc);
			goto fail1;
		}
	}

	return QDF_STATUS_SUCCESS;
fail1:
	/*
	 * Cleanup will be done as part of soc_detach, which will
	 * be called on pdev attach failure
	 */
	dp_soc_srng_deinit(soc);
	return QDF_STATUS_E_FAILURE;
}

/**
 * dp_soc_srng_free() - free soc level srng rings
 * @soc: Datapath soc handle
 *
 */
void dp_soc_srng_free(struct dp_soc *soc)
{
	uint32_t i;

	if (soc->arch_ops.txrx_soc_srng_free)
		soc->arch_ops.txrx_soc_srng_free(soc);

	dp_srng_free(soc, &soc->wbm_desc_rel_ring);

	for (i = 0; i < soc->num_tcl_data_rings; i++)
		dp_free_tx_ring_pair_by_index(soc, i);

	/* Free IPA rings for TCL_TX and TCL_COMPL ring */
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		dp_free_tx_ring_pair_by_index(soc, IPA_TCL_DATA_RING_IDX);
		dp_ipa_free_alt_tx_ring(soc);
	}

	dp_soc_tcl_cmd_cred_srng_free(soc);
	dp_soc_tcl_status_srng_free(soc);

	for (i = 0; i < soc->num_reo_dest_rings; i++)
		dp_srng_free(soc, &soc->reo_dest_ring[i]);

	dp_srng_free(soc, &soc->reo_reinject_ring);
	dp_srng_free(soc, &soc->rx_rel_ring);

	dp_srng_free(soc, &soc->reo_exception_ring);

	dp_srng_free(soc, &soc->reo_cmd_ring);
	dp_srng_free(soc, &soc->reo_status_ring);
}

/**
 * dp_soc_srng_alloc() - Allocate memory for soc level srng rings
 * @soc: Datapath soc handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_NOMEM on failure
 */
QDF_STATUS dp_soc_srng_alloc(struct dp_soc *soc)
{
	uint32_t entries;
	uint32_t i;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	uint32_t cached = WLAN_CFG_DST_RING_CACHED_DESC;
	uint32_t reo_dst_ring_size;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	/* sw2wbm link descriptor release ring */
	entries = wlan_cfg_get_dp_soc_wbm_release_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &soc->wbm_desc_rel_ring, SW2WBM_RELEASE,
			  entries, 0)) {
		dp_init_err("%pK: dp_srng_alloc failed for wbm_desc_rel_ring", soc);
		goto fail1;
	}

	/* TCL command and status rings */
	if (dp_soc_tcl_cmd_cred_srng_alloc(soc)) {
		dp_init_err("%pK: dp_srng_alloc failed for tcl_cmd_ring", soc);
		goto fail1;
	}

	if (dp_soc_tcl_status_srng_alloc(soc)) {
		dp_init_err("%pK: dp_srng_alloc failed for tcl_status_ring", soc);
		goto fail1;
	}

	/* REO reinjection ring */
	entries = wlan_cfg_get_dp_soc_reo_reinject_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &soc->reo_reinject_ring, REO_REINJECT,
			  entries, 0)) {
		dp_init_err("%pK: dp_srng_alloc failed for reo_reinject_ring", soc);
		goto fail1;
	}

	/* Rx release ring */
	entries = wlan_cfg_get_dp_soc_rx_release_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &soc->rx_rel_ring, WBM2SW_RELEASE,
			  entries, 0)) {
		dp_init_err("%pK: dp_srng_alloc failed for rx_rel_ring", soc);
		goto fail1;
	}

	/* Rx exception ring */
	entries = wlan_cfg_get_dp_soc_reo_exception_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &soc->reo_exception_ring, REO_EXCEPTION,
			  entries, 0)) {
		dp_init_err("%pK: dp_srng_alloc failed - reo_exception", soc);
		goto fail1;
	}

	/* REO command and status rings */
	entries = wlan_cfg_get_dp_soc_reo_cmd_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &soc->reo_cmd_ring, REO_CMD, entries, 0)) {
		dp_init_err("%pK: dp_srng_alloc failed for reo_cmd_ring", soc);
		goto fail1;
	}

	entries = wlan_cfg_get_dp_soc_reo_status_ring_size(soc_cfg_ctx);
	if (dp_srng_alloc(soc, &soc->reo_status_ring, REO_STATUS,
			  entries, 0)) {
		dp_init_err("%pK: dp_srng_alloc failed for reo_status_ring", soc);
		goto fail1;
	}

	reo_dst_ring_size = wlan_cfg_get_reo_dst_ring_size(soc_cfg_ctx);

	/* Disable cached desc if NSS offload is enabled */
	if (wlan_cfg_get_dp_soc_nss_cfg(soc_cfg_ctx))
		cached = 0;

	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		if (dp_alloc_tx_ring_pair_by_index(soc, i))
			goto fail1;
	}

	/* IPA rings for TCL_TX and TX_COMP will be allocated here */
	if (wlan_cfg_is_ipa_enabled(soc->wlan_cfg_ctx)) {
		if (dp_alloc_tx_ring_pair_by_index(soc, IPA_TCL_DATA_RING_IDX))
			goto fail1;

		if (dp_ipa_alloc_alt_tx_ring(soc))
			goto fail1;
	}

	for (i = 0; i < soc->num_reo_dest_rings; i++) {
		/* Setup REO destination ring */
		if (dp_srng_alloc(soc, &soc->reo_dest_ring[i], REO_DST,
				  reo_dst_ring_size, cached)) {
			dp_init_err("%pK: dp_srng_alloc failed for reo_dest_ring", soc);
			goto fail1;
		}
	}

	if (soc->arch_ops.txrx_soc_srng_alloc) {
		if (soc->arch_ops.txrx_soc_srng_alloc(soc)) {
			dp_init_err("%pK: dp_srng_alloc failed for arch rings",
				    soc);
			goto fail1;
		}
	}

	return QDF_STATUS_SUCCESS;

fail1:
	dp_soc_srng_free(soc);
	return QDF_STATUS_E_NOMEM;
}

/**
 * dp_soc_cfg_attach() - set target specific configuration in
 *			 dp soc cfg.
 * @soc: dp soc handle
 */
void dp_soc_cfg_attach(struct dp_soc *soc)
{
	int target_type;
	int nss_cfg = 0;

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_QCA6290:
		wlan_cfg_set_reo_dst_ring_size(soc->wlan_cfg_ctx,
					       REO_DST_RING_SIZE_QCA6290);
		break;
	case TARGET_TYPE_QCA6390:
	case TARGET_TYPE_QCA6490:
	case TARGET_TYPE_QCA6750:
		wlan_cfg_set_reo_dst_ring_size(soc->wlan_cfg_ctx,
					       REO_DST_RING_SIZE_QCA6290);
		soc->wlan_cfg_ctx->rxdma1_enable = 0;
		break;
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
	case TARGET_TYPE_PEACH:
		soc->wlan_cfg_ctx->rxdma1_enable = 0;
		break;
	case TARGET_TYPE_QCA8074:
		wlan_cfg_set_tso_desc_attach_defer(soc->wlan_cfg_ctx, 1);
		break;
	case TARGET_TYPE_QCA8074V2:
	case TARGET_TYPE_QCA6018:
	case TARGET_TYPE_QCA9574:
	case TARGET_TYPE_QCN6122:
	case TARGET_TYPE_QCA5018:
		wlan_cfg_set_tso_desc_attach_defer(soc->wlan_cfg_ctx, 1);
		wlan_cfg_set_rxdma1_enable(soc->wlan_cfg_ctx);
		break;
	case TARGET_TYPE_QCN9160:
		wlan_cfg_set_tso_desc_attach_defer(soc->wlan_cfg_ctx, 1);
		soc->wlan_cfg_ctx->rxdma1_enable = 0;
		break;
	case TARGET_TYPE_QCN9000:
		wlan_cfg_set_tso_desc_attach_defer(soc->wlan_cfg_ctx, 1);
		wlan_cfg_set_rxdma1_enable(soc->wlan_cfg_ctx);
		break;
	case TARGET_TYPE_QCN9224:
	case TARGET_TYPE_QCA5332:
	case TARGET_TYPE_QCN6432:
		wlan_cfg_set_tso_desc_attach_defer(soc->wlan_cfg_ctx, 1);
		wlan_cfg_set_rxdma1_enable(soc->wlan_cfg_ctx);
		break;
	default:
		qdf_print("%s: Unknown tgt type %d\n", __func__, target_type);
		qdf_assert_always(0);
		break;
	}

	if (soc->cdp_soc.ol_ops->get_soc_nss_cfg)
		nss_cfg = soc->cdp_soc.ol_ops->get_soc_nss_cfg(soc->ctrl_psoc);

	wlan_cfg_set_dp_soc_nss_cfg(soc->wlan_cfg_ctx, nss_cfg);

	if (wlan_cfg_get_dp_soc_nss_cfg(soc->wlan_cfg_ctx)) {
		wlan_cfg_set_num_tx_desc_pool(soc->wlan_cfg_ctx, 0);
		wlan_cfg_set_num_tx_ext_desc_pool(soc->wlan_cfg_ctx, 0);
		wlan_cfg_set_num_tx_desc(soc->wlan_cfg_ctx, 0);
		wlan_cfg_set_num_tx_spl_desc(soc->wlan_cfg_ctx, 0);
		wlan_cfg_set_num_tx_ext_desc(soc->wlan_cfg_ctx, 0);
		soc->init_tcl_cmd_cred_ring = false;
		soc->num_tcl_data_rings =
			wlan_cfg_num_nss_tcl_data_rings(soc->wlan_cfg_ctx);
		soc->num_reo_dest_rings =
			wlan_cfg_num_nss_reo_dest_rings(soc->wlan_cfg_ctx);

	} else {
		soc->init_tcl_cmd_cred_ring = true;
		soc->num_tx_comp_rings =
			wlan_cfg_num_tx_comp_rings(soc->wlan_cfg_ctx);
		soc->num_tcl_data_rings =
			wlan_cfg_num_tcl_data_rings(soc->wlan_cfg_ctx);
		soc->num_reo_dest_rings =
			wlan_cfg_num_reo_dest_rings(soc->wlan_cfg_ctx);
	}

}

void dp_pdev_set_default_reo(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;

	switch (pdev->pdev_id) {
	case 0:
		pdev->reo_dest =
			wlan_cfg_radio0_default_reo_get(soc->wlan_cfg_ctx);
		break;

	case 1:
		pdev->reo_dest =
			wlan_cfg_radio1_default_reo_get(soc->wlan_cfg_ctx);
		break;

	case 2:
		pdev->reo_dest =
			wlan_cfg_radio2_default_reo_get(soc->wlan_cfg_ctx);
		break;

	default:
		dp_init_err("%pK: Invalid pdev_id %d for reo selection",
			    soc, pdev->pdev_id);
		break;
	}
}

#ifdef WLAN_SUPPORT_DPDK
void dp_soc_reset_dpdk_intr_mask(struct dp_soc *soc)
{
	uint8_t j;
	uint8_t *grp_mask = NULL;
	int group_number, mask, num_ring;

	/* number of tx ring */
	num_ring = soc->num_tcl_data_rings;

	/*
	 * group mask for tx completion  ring.
	 */
	grp_mask =  &soc->wlan_cfg_ctx->int_tx_ring_mask[0];

	for (j = 0; j < WLAN_CFG_NUM_TCL_DATA_RINGS; j++) {
		/*
		 * Group number corresponding to tx offloaded ring.
		 */
		group_number = dp_srng_find_ring_in_mask(j, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d, ring_num %d",
				      soc, WBM2SW_RELEASE, j);
			continue;
		}

		mask = wlan_cfg_get_tx_ring_mask(soc->wlan_cfg_ctx,
						 group_number);

		/* reset the tx mask for offloaded ring */
		mask &= (~(1 << j));

		/*
		 * reset the interrupt mask for offloaded ring.
		 */
		wlan_cfg_set_tx_ring_mask(soc->wlan_cfg_ctx,
					  group_number, mask);
	}

	/* number of rx rings */
	num_ring = soc->num_reo_dest_rings;

	/*
	 * group mask for reo destination ring.
	 */
	grp_mask = &soc->wlan_cfg_ctx->int_rx_ring_mask[0];

	for (j = 0; j < WLAN_CFG_NUM_REO_DEST_RING; j++) {
		/*
		 * Group number corresponding to rx offloaded ring.
		 */
		group_number = dp_srng_find_ring_in_mask(j, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
				      soc, REO_DST, j);
			continue;
		}

		mask =  wlan_cfg_get_rx_ring_mask(soc->wlan_cfg_ctx,
						  group_number);

		/* reset the interrupt mask for offloaded ring */
		mask &= (~(1 << j));

		/*
		 * set the interrupt mask to zero for rx offloaded radio.
		 */
		wlan_cfg_set_rx_ring_mask(soc->wlan_cfg_ctx,
					  group_number, mask);
	}

	/*
	 * group mask for Rx buffer refill ring
	 */
	grp_mask = &soc->wlan_cfg_ctx->int_host2rxdma_ring_mask[0];

	for (j = 0; j < MAX_PDEV_CNT; j++) {
		int lmac_id = wlan_cfg_get_hw_mac_idx(soc->wlan_cfg_ctx, j);

		/*
		 * Group number corresponding to rx offloaded ring.
		 */
		group_number = dp_srng_find_ring_in_mask(lmac_id, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
				      soc, REO_DST, lmac_id);
			continue;
		}

		/* set the interrupt mask for offloaded ring */
		mask =  wlan_cfg_get_host2rxdma_ring_mask(soc->wlan_cfg_ctx,
							  group_number);
		mask &= (~(1 << lmac_id));

		/*
		 * set the interrupt mask to zero for rx offloaded radio.
		 */
		wlan_cfg_set_host2rxdma_ring_mask(soc->wlan_cfg_ctx,
						  group_number, mask);
	}

	grp_mask = &soc->wlan_cfg_ctx->int_rx_err_ring_mask[0];

	for (j = 0; j < num_ring; j++) {
		/*
		 * Group number corresponding to rx err ring.
		 */
		group_number = dp_srng_find_ring_in_mask(j, grp_mask);
		if (group_number < 0) {
			dp_init_debug("%pK: ring not part of any group; ring_type: %d,ring_num %d",
				      soc, REO_EXCEPTION, j);
			continue;
		}

		wlan_cfg_set_rx_err_ring_mask(soc->wlan_cfg_ctx,
					      group_number, 0);
	}
}
#endif
