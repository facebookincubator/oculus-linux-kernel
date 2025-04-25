/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "dp_types.h"
#include "qdf_mem.h"
#include "qdf_nbuf.h"
#include "cfg_dp.h"
#include "wlan_cfg.h"
#include "dp_types.h"
#include "hal_rx_flow.h"
#include "dp_htt.h"
#include "dp_internal.h"
#include "hif.h"
#include "wlan_dp_rx_thread.h"
#include <wlan_dp_main.h>
#include <wlan_dp_fisa_rx.h>
#include <cdp_txrx_ctrl.h>
#include "qdf_ssr_driver_dump.h"

/* Timeout in milliseconds to wait for CMEM FST HTT response */
#define DP_RX_FST_CMEM_RESP_TIMEOUT 2000

#define INVALID_NAPI 0Xff

#ifdef WLAN_SUPPORT_RX_FISA
void dp_fisa_rx_fst_update_work(void *arg);

static void dp_rx_dump_fisa_table(struct wlan_dp_psoc_context *dp_ctx)
{
	hal_soc_handle_t hal_soc_hdl = dp_ctx->hal_soc;
	struct wlan_dp_psoc_cfg *dp_cfg = &dp_ctx->dp_cfg;
	struct dp_rx_fst *fst = dp_ctx->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	int i;

	/* Check if it is enabled in the INI */
	if (!wlan_dp_cfg_is_rx_fisa_enabled(dp_cfg)) {
		dp_err("RX FISA feature is disabled");
		return;
	}

	if (!fst->fst_in_cmem)
		return hal_rx_dump_fse_table(fst->hal_rx_fst);

	sw_ft_entry = (struct dp_fisa_rx_sw_ft *)fst->base;

	if (hif_force_wake_request(((struct hal_soc *)hal_soc_hdl)->hif_handle)) {
		dp_err("Wake up request failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return;
	}

	for (i = 0; i < fst->max_entries; i++)
		hal_rx_dump_cmem_fse(hal_soc_hdl,
				     sw_ft_entry[i].cmem_offset, i);

	if (hif_force_wake_release(((struct hal_soc *)hal_soc_hdl)->hif_handle)) {
		dp_err("Wake up release failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return;
	}
}

static void dp_print_fisa_stats(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_dp_psoc_cfg *dp_cfg = &dp_ctx->dp_cfg;
	struct dp_rx_fst *fst = dp_ctx->rx_fst;

	/* Check if it is enabled in the INI */
	if (!wlan_dp_cfg_is_rx_fisa_enabled(dp_cfg))
		return;

	dp_info("invalid flow index: %u", fst->stats.invalid_flow_index);
	dp_info("workqueue update deferred: %u", fst->stats.update_deferred);
	dp_info("reo_mismatch: cce_match: %u",
		fst->stats.reo_mismatch.allow_cce_match);
	dp_info("reo_mismatch: allow_fse_metdata_mismatch: %u",
		fst->stats.reo_mismatch.allow_fse_metdata_mismatch);
	dp_info("reo_mismatch: allow_non_aggr: %u",
		fst->stats.reo_mismatch.allow_non_aggr);
}

/* Length of string to store tuple information for printing */
#define DP_TUPLE_STR_LEN 512

/**
 * print_flow_tuple() - Debug function to dump flow tuple
 * @flow_tuple: flow tuple containing tuple info
 * @str: destination buffer
 * @size: size of @str
 *
 * Return: NONE
 */
static
void print_flow_tuple(struct cdp_rx_flow_tuple_info *flow_tuple, char *str,
		      uint32_t size)
{
	qdf_scnprintf(str, size,
		      "dest 0x%x%x%x%x(0x%x) src 0x%x%x%x%x(0x%x) proto 0x%x",
		      flow_tuple->dest_ip_127_96,
		      flow_tuple->dest_ip_95_64,
		      flow_tuple->dest_ip_63_32,
		      flow_tuple->dest_ip_31_0,
		      flow_tuple->dest_port,
		      flow_tuple->src_ip_127_96,
		      flow_tuple->src_ip_95_64,
		      flow_tuple->src_ip_63_32,
		      flow_tuple->src_ip_31_0,
		      flow_tuple->src_port,
		      flow_tuple->l4_protocol);
}

static QDF_STATUS dp_rx_dump_fisa_stats(struct wlan_dp_psoc_context *dp_ctx)
{
	char tuple_str[DP_TUPLE_STR_LEN] = {'\0'};
	struct dp_rx_fst *rx_fst = dp_ctx->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		&((struct dp_fisa_rx_sw_ft *)rx_fst->base)[0];
	int ft_size = rx_fst->max_entries;
	int i;

	dp_info("#flows added %d evicted %d hash collision %d",
		rx_fst->add_flow_count,
		rx_fst->del_flow_count,
		rx_fst->hash_collision_cnt);

	for (i = 0; i < ft_size; i++, sw_ft_entry++) {
		if (!sw_ft_entry->is_populated)
			continue;

		print_flow_tuple(&sw_ft_entry->rx_flow_tuple_info,
				 tuple_str,
				 sizeof(tuple_str));

		dp_info("Flow[%d][%s][%s] ring %d msdu-aggr %d flushes %d bytes-agg %llu avg-bytes-aggr %llu same_mld_vdev_mismatch %llu",
			sw_ft_entry->flow_id,
			sw_ft_entry->is_flow_udp ? "udp" : "tcp",
			tuple_str,
			sw_ft_entry->napi_id,
			sw_ft_entry->aggr_count,
			sw_ft_entry->flush_count,
			sw_ft_entry->bytes_aggregated,
			qdf_do_div(sw_ft_entry->bytes_aggregated,
				   sw_ft_entry->flush_count),
			sw_ft_entry->same_mld_vdev_mismatch);
	}
	return QDF_STATUS_SUCCESS;
}

void dp_set_fst_in_cmem(bool fst_in_cmem)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();

	dp_ctx->fst_in_cmem = fst_in_cmem;
}

void dp_print_fisa_rx_stats(enum cdp_fisa_stats_id stats_id)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();

	switch (stats_id) {
	case CDP_FISA_STATS_ID_ERR_STATS:
		dp_print_fisa_stats(dp_ctx);
		break;
	case CDP_FISA_STATS_ID_DUMP_HW_FST:
		dp_rx_dump_fisa_table(dp_ctx);
		break;
	case CDP_FISA_STATS_ID_DUMP_SW_FST:
		dp_rx_dump_fisa_stats(dp_ctx);
		break;
	default:
		break;
	}
}

/**
 * dp_rx_flow_send_htt_operation_cmd() - Invalidate FSE cache on FT change
 * @dp_ctx: DP component handle
 * @fse_op: Cache operation code
 * @rx_flow_tuple: flow tuple whose entry has to be invalidated
 *
 * Return: Success if we successfully send FW HTT command
 */
static QDF_STATUS
dp_rx_flow_send_htt_operation_cmd(struct wlan_dp_psoc_context *dp_ctx,
				  enum dp_htt_flow_fst_operation fse_op,
				  struct cdp_rx_flow_tuple_info *rx_flow_tuple)
{
	struct dp_htt_rx_flow_fst_operation fse_op_cmd;
	struct cdp_rx_flow_info rx_flow_info;
	union cdp_fisa_config cfg;

	rx_flow_info.is_addr_ipv4 = true;
	rx_flow_info.op_code = CDP_FLOW_FST_ENTRY_ADD;
	qdf_mem_copy(&rx_flow_info.flow_tuple_info, rx_flow_tuple,
		     sizeof(struct cdp_rx_flow_tuple_info));
	rx_flow_info.fse_metadata = 0xDADA;
	fse_op_cmd.pdev_id = OL_TXRX_PDEV_ID;
	fse_op_cmd.op_code = fse_op;
	fse_op_cmd.rx_flow = &rx_flow_info;

	cfg.fse_op_cmd = &fse_op_cmd;

	return cdp_txrx_fisa_config(dp_ctx->cdp_soc, OL_TXRX_PDEV_ID,
				    CDP_FISA_HTT_RX_FSE_OP_CFG, &cfg);
}

/**
 * dp_fisa_fse_cache_flush_timer() - FSE cache flush timeout handler
 * @arg: SoC handle
 *
 * Return: None
 */
static void dp_fisa_fse_cache_flush_timer(void *arg)
{
	struct wlan_dp_psoc_context *dp_ctx =
					(struct wlan_dp_psoc_context *)arg;
	struct dp_rx_fst *fisa_hdl = dp_ctx->rx_fst;
	struct cdp_rx_flow_tuple_info rx_flow_tuple_info = { 0 };
	static uint32_t fse_cache_flush_rec_idx;
	struct fse_cache_flush_history *fse_cache_flush_rec;
	QDF_STATUS status;

	if (!fisa_hdl)
		return;

	if (qdf_atomic_read(&fisa_hdl->pm_suspended)) {
		qdf_atomic_set(&fisa_hdl->fse_cache_flush_posted, 0);
		return;
	}

	fse_cache_flush_rec = &fisa_hdl->cache_fl_rec[fse_cache_flush_rec_idx %
							MAX_FSE_CACHE_FL_HST];
	fse_cache_flush_rec->timestamp = qdf_get_log_timestamp();
	fse_cache_flush_rec->flows_added =
			qdf_atomic_read(&fisa_hdl->fse_cache_flush_posted);
	fse_cache_flush_rec_idx++;
	dp_info("FSE cache flush for %d flows",
		fse_cache_flush_rec->flows_added);

	status =
	 dp_rx_flow_send_htt_operation_cmd(dp_ctx,
					   DP_HTT_FST_CACHE_INVALIDATE_FULL,
					   &rx_flow_tuple_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to send the cache invalidation");
		/*
		 * Not big impact cache entry gets updated later
		 */
	}

	qdf_atomic_set(&fisa_hdl->fse_cache_flush_posted, 0);
}

/**
 * dp_rx_fst_cmem_deinit() - De-initialize CMEM parameters
 * @fst: Pointer to DP FST
 *
 * Return: None
 */
static void dp_rx_fst_cmem_deinit(struct dp_rx_fst *fst)
{
	struct dp_fisa_rx_fst_update_elem *elem;
	qdf_list_node_t *node;
	int i;

	qdf_cancel_work(&fst->fst_update_work);
	qdf_flush_work(&fst->fst_update_work);
	qdf_flush_workqueue(0, fst->fst_update_wq);
	qdf_destroy_workqueue(0, fst->fst_update_wq);

	qdf_spin_lock_bh(&fst->dp_rx_fst_lock);
	while (qdf_list_peek_front(&fst->fst_update_list, &node) ==
	       QDF_STATUS_SUCCESS) {
		elem = (struct dp_fisa_rx_fst_update_elem *)node;
		qdf_list_remove_front(&fst->fst_update_list, &node);
		qdf_mem_free(elem);
	}
	qdf_spin_unlock_bh(&fst->dp_rx_fst_lock);

	qdf_list_destroy(&fst->fst_update_list);
	qdf_event_destroy(&fst->cmem_resp_event);

	for (i = 0; i < MAX_REO_DEST_RINGS; i++)
		qdf_spinlock_destroy(&fst->dp_rx_sw_ft_lock[i]);
}

/**
 * dp_rx_fst_cmem_init() - Initialize CMEM parameters
 * @fst: Pointer to DP FST
 *
 * Return: Success/Failure
 */
static QDF_STATUS dp_rx_fst_cmem_init(struct dp_rx_fst *fst)
{
	int i;

	fst->fst_update_wq =
		qdf_alloc_high_prior_ordered_workqueue("dp_rx_fst_update_wq");
	if (!fst->fst_update_wq) {
		dp_err("failed to allocate fst update wq");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_create_work(0, &fst->fst_update_work,
			dp_fisa_rx_fst_update_work, fst);
	qdf_list_create(&fst->fst_update_list, 128);
	qdf_event_create(&fst->cmem_resp_event);

	for (i = 0; i < MAX_REO_DEST_RINGS; i++)
		qdf_spinlock_create(&fst->dp_rx_sw_ft_lock[i]);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_RX_FISA_HIST
static
QDF_STATUS dp_rx_sw_ft_hist_init(struct dp_fisa_rx_sw_ft *sw_ft,
				 uint32_t max_entries,
				 uint32_t rx_pkt_tlv_size)
{
	int i;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	for (i = 0; i < max_entries; i++) {
		sw_ft[i].pkt_hist.tlv_hist =
			(uint8_t *)qdf_mem_malloc(rx_pkt_tlv_size *
						  FISA_FLOW_MAX_AGGR_COUNT);
		if (!sw_ft[i].pkt_hist.tlv_hist) {
			dp_err("unable to allocate tlv history");
			qdf_status = QDF_STATUS_E_NOMEM;
			break;
		}
	}
	return qdf_status;
}

static void dp_rx_sw_ft_hist_deinit(struct dp_fisa_rx_sw_ft *sw_ft,
				    uint32_t max_entries)
{
	int i;

	for (i = 0; i < max_entries; i++) {
		if (sw_ft[i].pkt_hist.tlv_hist)
			qdf_mem_free(sw_ft[i].pkt_hist.tlv_hist);
	}
}

#else

static
QDF_STATUS dp_rx_sw_ft_hist_init(struct dp_fisa_rx_sw_ft *sw_ft,
				 uint32_t max_entries,
				 uint32_t rx_pkt_tlv_size)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_rx_sw_ft_hist_deinit(struct dp_fisa_rx_sw_ft *sw_ft,
				    uint32_t max_entries)
{
}
#endif

QDF_STATUS dp_rx_fst_attach(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_soc *soc = (struct dp_soc *)dp_ctx->cdp_soc;
	struct wlan_dp_psoc_cfg *dp_cfg = &dp_ctx->dp_cfg;
	struct dp_rx_fst *fst;
	struct dp_fisa_rx_sw_ft *ft_entry;
	cdp_config_param_type soc_param;
	int i = 0;
	QDF_STATUS status;

	/* Check if it is enabled in the INI */
	if (!wlan_dp_cfg_is_rx_fisa_enabled(dp_cfg)) {
		dp_err("RX FISA feature is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

#ifdef NOT_YET /* Not required for now */
	/* Check if FW supports */
	if (!wlan_psoc_nif_fw_ext_cap_get((void *)pdev->ctrl_pdev,
					  WLAN_SOC_CEXT_RX_FSE_SUPPORT)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "rx fse disabled in FW\n");
		wlan_cfg_set_rx_flow_tag_enabled(cfg, false);
		return QDF_STATUS_E_NOSUPPORT;
	}
#endif
	if (dp_ctx->rx_fst) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "RX FST already allocated\n");
		return QDF_STATUS_SUCCESS;
	}

	fst = qdf_mem_malloc(sizeof(struct dp_rx_fst));
	if (!fst)
		return QDF_STATUS_E_NOMEM;

	fst->rx_pkt_tlv_size = 0;
	status = cdp_txrx_get_psoc_param(dp_ctx->cdp_soc, CDP_RX_PKT_TLV_SIZE,
					 &soc_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Unable to fetch RX pkt tlv size");
		return status;
	}

	fst->rx_pkt_tlv_size = soc_param.rx_pkt_tlv_size;

	/* This will clear entire FISA params */
	soc_param.fisa_params.rx_toeplitz_hash_key = NULL;
	status = cdp_txrx_get_psoc_param(dp_ctx->cdp_soc, CDP_CFG_FISA_PARAMS,
					 &soc_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Unable to fetch fisa params");
		return status;
	}

	fst->max_skid_length = soc_param.fisa_params.rx_flow_max_search;
	fst->max_entries = soc_param.fisa_params.fisa_fst_size;
	fst->rx_toeplitz_hash_key = soc_param.fisa_params.rx_toeplitz_hash_key;

	fst->hash_mask = fst->max_entries - 1;
	fst->num_entries = 0;
	dp_info("FST setup params FT size %d, hash_mask 0x%x, skid_length %d",
		fst->max_entries, fst->hash_mask, fst->max_skid_length);

	/* Allocate the software flowtable */
	fst->base = (uint8_t *)dp_context_alloc_mem(soc, DP_FISA_RX_FT_TYPE,
				DP_RX_GET_SW_FT_ENTRY_SIZE * fst->max_entries);

	if (!fst->base)
		goto free_rx_fst;

	ft_entry = (struct dp_fisa_rx_sw_ft *)fst->base;

	for (i = 0; i < fst->max_entries; i++)
		ft_entry[i].napi_id = INVALID_NAPI;

	status = dp_rx_sw_ft_hist_init(ft_entry, fst->max_entries,
				       fst->rx_pkt_tlv_size);
	if (QDF_IS_STATUS_ERROR(status))
		goto free_hist;

	fst->hal_rx_fst = hal_rx_fst_attach(dp_ctx->hal_soc,
					    dp_ctx->qdf_dev,
					    &fst->hal_rx_fst_base_paddr,
					    fst->max_entries,
					    fst->max_skid_length,
					    fst->rx_toeplitz_hash_key,
					    dp_ctx->fst_cmem_base);

	if (qdf_unlikely(!fst->hal_rx_fst)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "Rx Hal fst allocation failed, #entries:%d\n",
			  fst->max_entries);
		goto free_hist;
	}

	qdf_spinlock_create(&fst->dp_rx_fst_lock);

	status = qdf_timer_init(dp_ctx->qdf_dev, &fst->fse_cache_flush_timer,
				dp_fisa_fse_cache_flush_timer, (void *)dp_ctx,
				QDF_TIMER_TYPE_WAKE_APPS);
	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "Failed to init cache_flush_timer\n");
		goto timer_init_fail;
	}

	qdf_atomic_init(&fst->fse_cache_flush_posted);

	fst->fse_cache_flush_allow = true;
	fst->rx_hash_enabled = wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx);
	fst->soc_hdl = soc;
	fst->dp_ctx = dp_ctx;
	dp_ctx->rx_fst = fst;
	dp_ctx->fisa_enable = true;
	dp_ctx->fisa_lru_del_enable =
				wlan_dp_cfg_is_rx_fisa_lru_del_enabled(dp_cfg);

	qdf_atomic_init(&dp_ctx->skip_fisa_param.skip_fisa);
	qdf_atomic_init(&fst->pm_suspended);

	QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
		  "Rx FST attach successful, #entries:%d\n",
		  fst->max_entries);

	qdf_ssr_driver_dump_register_region("dp_fisa", fst, sizeof(*fst));
	qdf_ssr_driver_dump_register_region("dp_fisa_sw_fse_table", fst->base,
					    DP_RX_GET_SW_FT_ENTRY_SIZE *
						   fst->max_entries);

	return QDF_STATUS_SUCCESS;

timer_init_fail:
	qdf_spinlock_destroy(&fst->dp_rx_fst_lock);
	hal_rx_fst_detach(dp_ctx->hal_soc, fst->hal_rx_fst, dp_ctx->qdf_dev,
			  dp_ctx->fst_cmem_base);
free_hist:
	dp_rx_sw_ft_hist_deinit((struct dp_fisa_rx_sw_ft *)fst->base,
				fst->max_entries);
	dp_context_free_mem(soc, DP_FISA_RX_FT_TYPE, fst->base);
free_rx_fst:
	qdf_mem_free(fst);
	return QDF_STATUS_E_NOMEM;
}

/**
 * dp_rx_fst_check_cmem_support() - Check if FW can allocate FSE in CMEM,
 * allocate FSE in DDR if FW doesn't support CMEM allocation
 * @dp_ctx: DP component context
 *
 * Return: None
 */
static void dp_rx_fst_check_cmem_support(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_rx_fst *fst = dp_ctx->rx_fst;
	QDF_STATUS status;

	/**
	 * FW doesn't support CMEM FSE, keep it in DDR
	 * dp_ctx->fst_cmem_base is non-NULL then CMEM support is
	 * already present
	 */
	if (!dp_ctx->fst_in_cmem && dp_ctx->fst_cmem_base == 0)
		return;

	status = dp_rx_fst_cmem_init(fst);
	if (status != QDF_STATUS_SUCCESS)
		return;

	hal_rx_fst_detach(dp_ctx->hal_soc, fst->hal_rx_fst, dp_ctx->qdf_dev,
			  dp_ctx->fst_cmem_base);
	fst->hal_rx_fst = NULL;
	fst->hal_rx_fst_base_paddr = 0;
	fst->flow_deletion_supported = true;
	fst->fst_in_cmem = true;
}

/**
 * dp_rx_flow_send_fst_fw_setup() - Program FST parameters in FW/HW post-attach
 * @dp_ctx: DP component context
 *
 * Return: Success when fst parameters are programmed in FW, error otherwise
 */
static QDF_STATUS
dp_rx_flow_send_fst_fw_setup(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_htt_rx_flow_fst_setup fisa_hw_fst_setup_cmd = {0};
	struct dp_rx_fst *fst = dp_ctx->rx_fst;
	union cdp_fisa_config cfg;
	QDF_STATUS status;

	/* check if FW has support to place FST in CMEM */
	dp_rx_fst_check_cmem_support(dp_ctx);

	/* mac_id = 0 is used to configure both macs with same FT */
	fisa_hw_fst_setup_cmd.pdev_id = 0;
	fisa_hw_fst_setup_cmd.max_entries = fst->max_entries;
	fisa_hw_fst_setup_cmd.max_search = fst->max_skid_length;
	if (dp_ctx->fst_cmem_base) {
		fisa_hw_fst_setup_cmd.base_addr_lo =
			dp_ctx->fst_cmem_base & 0xffffffff;
		/* Higher order bits are mostly 0, Always use 0x10 */
		fisa_hw_fst_setup_cmd.base_addr_hi =
			(dp_ctx->fst_cmem_base >> 32) | 0x10;
		dp_info("cmem base address 0x%llx", dp_ctx->fst_cmem_base);
	} else {
		fisa_hw_fst_setup_cmd.base_addr_lo =
			fst->hal_rx_fst_base_paddr & 0xffffffff;
		fisa_hw_fst_setup_cmd.base_addr_hi =
			(fst->hal_rx_fst_base_paddr >> 32);
	}

	fisa_hw_fst_setup_cmd.ip_da_sa_prefix =	HTT_RX_IPV4_COMPATIBLE_IPV6;
	fisa_hw_fst_setup_cmd.hash_key_len = HAL_FST_HASH_KEY_SIZE_BYTES;
	fisa_hw_fst_setup_cmd.hash_key = fst->rx_toeplitz_hash_key;

	cfg.fse_setup_info = &fisa_hw_fst_setup_cmd;

	status = cdp_txrx_fisa_config(dp_ctx->cdp_soc, OL_TXRX_PDEV_ID,
				      CDP_FISA_HTT_RX_FSE_SETUP_CFG, &cfg);
	if (!fst->fst_in_cmem || dp_ctx->fst_cmem_base) {
		/**
		 * Return from here if fst_cmem is not enabled or cmem address
		 * is known at init time
		 */
		return status;
	}

	status = qdf_wait_single_event(&fst->cmem_resp_event,
				       DP_RX_FST_CMEM_RESP_TIMEOUT);

	dp_err("FST params after CMEM update FT size %d, hash_mask 0x%x",
	       fst->max_entries, fst->hash_mask);

	return status;
}

void dp_rx_fst_detach(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_soc *soc = (struct dp_soc *)dp_ctx->cdp_soc;
	struct dp_rx_fst *dp_fst;

	dp_fst = dp_ctx->rx_fst;
	if (qdf_likely(dp_fst)) {
		qdf_ssr_driver_dump_unregister_region("dp_fisa_sw_fse_table");
		qdf_ssr_driver_dump_unregister_region("dp_fisa");
		qdf_timer_sync_cancel(&dp_fst->fse_cache_flush_timer);
		if (dp_fst->fst_in_cmem)
			dp_rx_fst_cmem_deinit(dp_fst);
		else
			hal_rx_fst_detach(soc->hal_soc, dp_fst->hal_rx_fst,
					  soc->osdev, dp_ctx->fst_cmem_base);

		dp_rx_sw_ft_hist_deinit((struct dp_fisa_rx_sw_ft *)dp_fst->base,
					dp_fst->max_entries);
		dp_context_free_mem(soc, DP_FISA_RX_FT_TYPE, dp_fst->base);
		qdf_spinlock_destroy(&dp_fst->dp_rx_fst_lock);
		qdf_mem_free(dp_fst);
	}

	dp_ctx->rx_fst = NULL;
	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
		  "Rx FST detached\n");
}

/*
 * dp_rx_fst_update_cmem_params() - Update CMEM FST params
 * @soc:		DP SoC context
 * @num_entries:	Number of flow search entries
 * @cmem_ba_lo:		CMEM base address low
 * @cmem_ba_hi:		CMEM base address high
 *
 * Return: None
 */
void dp_rx_fst_update_cmem_params(struct dp_soc *soc, uint16_t num_entries,
				  uint32_t cmem_ba_lo, uint32_t cmem_ba_hi)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();
	struct dp_rx_fst *fst = dp_ctx->rx_fst;

	fst->max_entries = num_entries;
	fst->hash_mask = fst->max_entries - 1;
	fst->cmem_ba = cmem_ba_lo;

	/* Address is not NULL then address is already known during init */
	if (dp_ctx->fst_cmem_base == 0)
		qdf_event_set(&fst->cmem_resp_event);
}

void dp_rx_fst_update_pm_suspend_status(struct wlan_dp_psoc_context *dp_ctx,
					bool suspended)
{
	struct dp_rx_fst *fst = dp_ctx->rx_fst;

	if (!fst)
		return;

	if (suspended)
		qdf_atomic_set(&fst->pm_suspended, 1);
	else
		qdf_atomic_set(&fst->pm_suspended, 0);
}

void dp_rx_fst_requeue_wq(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_rx_fst *fst = dp_ctx->rx_fst;

	if (!fst || !fst->fst_wq_defer)
		return;

	fst->fst_wq_defer = false;
	qdf_queue_work(fst->soc_hdl->osdev,
		       fst->fst_update_wq,
		       &fst->fst_update_work);

	dp_info("requeued defer fst update task");
}

QDF_STATUS dp_rx_fst_target_config(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(dp_ctx->cdp_soc);
	QDF_STATUS status;
	struct dp_rx_fst *fst = dp_ctx->rx_fst;

	/* Check if it is enabled in the INI */
	if (!dp_ctx->fisa_enable) {
		dp_err("RX FISA feature is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	status = dp_rx_flow_send_fst_fw_setup(dp_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("dp_rx_flow_send_fst_fw_setup failed %d",
		       status);
		return status;
	}

	if (dp_ctx->fst_cmem_base) {
		dp_ctx->fst_in_cmem = true;
		dp_rx_fst_update_cmem_params(soc, fst->max_entries,
					     dp_ctx->fst_cmem_base & 0xffffffff,
					     dp_ctx->fst_cmem_base >> 32);
	}
	return status;
}

static uint8_t
dp_rx_fisa_get_max_aggr_supported(struct wlan_dp_psoc_context *dp_ctx)
{
	if (!dp_ctx->fisa_dynamic_aggr_size_support)
		return DP_RX_FISA_MAX_AGGR_COUNT_DEFAULT;

	switch (hal_get_target_type(dp_ctx->hal_soc)) {
	case TARGET_TYPE_WCN6450:
		return DP_RX_FISA_MAX_AGGR_COUNT_1;
	default:
		return DP_RX_FISA_MAX_AGGR_COUNT_DEFAULT;
	}
}

#define FISA_MAX_TIMEOUT 0xffffffff
#define FISA_DISABLE_TIMEOUT 0
QDF_STATUS dp_rx_fisa_config(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_htt_rx_fisa_cfg fisa_config;
	union cdp_fisa_config cfg;

	fisa_config.pdev_id = 0;
	fisa_config.fisa_timeout = FISA_MAX_TIMEOUT;
	fisa_config.max_aggr_supported =
		dp_rx_fisa_get_max_aggr_supported(dp_ctx);

	cfg.fisa_config = &fisa_config;

	return cdp_txrx_fisa_config(dp_ctx->cdp_soc, OL_TXRX_PDEV_ID,
				    CDP_FISA_HTT_RX_FISA_CFG, &cfg);
}

void dp_fisa_cfg_init(struct wlan_dp_psoc_cfg *config,
		      struct wlan_objmgr_psoc *psoc)
{
	config->fisa_enable = cfg_get(psoc, CFG_DP_RX_FISA_ENABLE);
	config->is_rx_fisa_enabled = cfg_get(psoc, CFG_DP_RX_FISA_ENABLE);
	config->is_rx_fisa_lru_del_enabled =
				cfg_get(psoc, CFG_DP_RX_FISA_LRU_DEL_ENABLE);
}
#else /* WLAN_SUPPORT_RX_FISA */

#endif /* !WLAN_SUPPORT_RX_FISA */

