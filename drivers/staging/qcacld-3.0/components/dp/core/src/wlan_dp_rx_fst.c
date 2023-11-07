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

/* Timeout in milliseconds to wait for CMEM FST HTT response */
#define DP_RX_FST_CMEM_RESP_TIMEOUT 2000

#define INVALID_NAPI 0Xff

#ifdef WLAN_SUPPORT_RX_FISA
void dp_fisa_rx_fst_update_work(void *arg);

void dp_rx_dump_fisa_table(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	hal_soc_handle_t hal_soc_hdl = soc->hal_soc;
	struct dp_rx_fst *fst = soc->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	int i;

	/* Check if it is enabled in the INI */
	if (!wlan_cfg_is_rx_fisa_enabled(cfg)) {
		dp_err("RX FISA feature is disabled");
		return;
	}

	if (!fst->fst_in_cmem)
		return hal_rx_dump_fse_table(soc->rx_fst->hal_rx_fst);

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

void dp_print_fisa_stats(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	struct dp_rx_fst *fst = soc->rx_fst;

	/* Check if it is enabled in the INI */
	if (!wlan_cfg_is_rx_fisa_enabled(cfg))
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

/**
 * dp_rx_flow_send_htt_operation_cmd() - Invalidate FSE cache on FT change
 * @pdev: handle to DP pdev
 * @fse_op: Cache operation code
 * @rx_flow_tuple: flow tuple whose entry has to be invalidated
 *
 * Return: Success if we successfully send FW HTT command
 */
static QDF_STATUS
dp_rx_flow_send_htt_operation_cmd(struct dp_pdev *pdev,
				  enum dp_htt_flow_fst_operation fse_op,
				  struct cdp_rx_flow_tuple_info *rx_flow_tuple)
{
	struct dp_htt_rx_flow_fst_operation fse_op_cmd;
	struct cdp_rx_flow_info rx_flow_info;

	rx_flow_info.is_addr_ipv4 = true;
	rx_flow_info.op_code = CDP_FLOW_FST_ENTRY_ADD;
	qdf_mem_copy(&rx_flow_info.flow_tuple_info, rx_flow_tuple,
		     sizeof(struct cdp_rx_flow_tuple_info));
	rx_flow_info.fse_metadata = 0xDADA;
	fse_op_cmd.pdev_id = pdev->pdev_id;
	fse_op_cmd.op_code = fse_op;
	fse_op_cmd.rx_flow = &rx_flow_info;

	return dp_htt_rx_flow_fse_operation(pdev, &fse_op_cmd);
}

/**
 * dp_fisa_fse_cache_flush_timer() - FSE cache flush timeout handler
 * @arg: SoC handle
 *
 * Return: None
 */
static void dp_fisa_fse_cache_flush_timer(void *arg)
{
	struct dp_soc *soc = (struct dp_soc *)arg;
	struct dp_rx_fst *fisa_hdl = soc->rx_fst;
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
	 dp_rx_flow_send_htt_operation_cmd(soc->pdev_list[0],
					   DP_HTT_FST_CACHE_INVALIDATE_FULL,
					   &rx_flow_tuple_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		dp_err("Failed to send the cache invalidation\n");
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
		dp_err("failed to allocate fst update wq\n");
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

/**
 * dp_rx_fst_attach() - Initialize Rx FST and setup necessary parameters
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: Handle to flow search table entry
 */
QDF_STATUS dp_rx_fst_attach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *fst;
	struct dp_fisa_rx_sw_ft *ft_entry;
	uint8_t *hash_key;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	int i = 0;
	QDF_STATUS status;

	/* Check if it is enabled in the INI */
	if (!wlan_cfg_is_rx_fisa_enabled(cfg)) {
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
	if (soc->rx_fst) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "RX FST already allocated\n");
		return QDF_STATUS_SUCCESS;
	}

	fst = qdf_mem_malloc(sizeof(struct dp_rx_fst));
	if (!fst)
		return QDF_STATUS_E_NOMEM;

	fst->max_skid_length = wlan_cfg_rx_fst_get_max_search(cfg);
	fst->max_entries = wlan_cfg_get_rx_flow_search_table_size(cfg);
	hash_key = wlan_cfg_rx_fst_get_hash_key(cfg);

	fst->hash_mask = fst->max_entries - 1;
	fst->num_entries = 0;
	dp_err("FST setup params FT size %d, hash_mask 0x%x, skid_length %d",
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
				       soc->rx_pkt_tlv_size);
	if (QDF_IS_STATUS_ERROR(status))
		goto free_hist;

	fst->hal_rx_fst = hal_rx_fst_attach(soc->hal_soc,
					    soc->osdev,
					    &fst->hal_rx_fst_base_paddr,
					    fst->max_entries,
					    fst->max_skid_length, hash_key,
					    soc->fst_cmem_base);

	if (qdf_unlikely(!fst->hal_rx_fst)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "Rx Hal fst allocation failed, #entries:%d\n",
			  fst->max_entries);
		goto free_hist;
	}

	qdf_spinlock_create(&fst->dp_rx_fst_lock);

	status = qdf_timer_init(soc->osdev, &fst->fse_cache_flush_timer,
				dp_fisa_fse_cache_flush_timer, (void *)soc,
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
	soc->rx_fst = fst;
	soc->fisa_enable = true;
	soc->fisa_lru_del_enable = wlan_cfg_is_rx_fisa_lru_del_enabled(cfg);

	qdf_atomic_init(&soc->skip_fisa_param.skip_fisa);
	qdf_atomic_init(&fst->pm_suspended);

	QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
		  "Rx FST attach successful, #entries:%d\n",
		  fst->max_entries);

	return QDF_STATUS_SUCCESS;

timer_init_fail:
	qdf_spinlock_destroy(&fst->dp_rx_fst_lock);
	hal_rx_fst_detach(soc->hal_soc, fst->hal_rx_fst, soc->osdev,
			  soc->fst_cmem_base);
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
 * @soc: DP SoC handle
 *
 * Return: None
 */
static void dp_rx_fst_check_cmem_support(struct dp_soc *soc)
{
	struct dp_rx_fst *fst = soc->rx_fst;
	QDF_STATUS status;

	/**
	 * FW doesn't support CMEM FSE, keep it in DDR
	 * soc->fst_cmem_base is non-NULL then CMEM support is already present
	 */
	if (!soc->fst_in_cmem && (soc->fst_cmem_base == 0))
		return;

	status = dp_rx_fst_cmem_init(fst);
	if (status != QDF_STATUS_SUCCESS)
		return;

	hal_rx_fst_detach(soc->hal_soc, fst->hal_rx_fst, soc->osdev,
			  soc->fst_cmem_base);
	fst->hal_rx_fst = NULL;
	fst->hal_rx_fst_base_paddr = 0;
	fst->flow_deletion_supported = true;
	fst->fst_in_cmem = true;
}

/**
 * dp_rx_flow_send_fst_fw_setup() - Program FST parameters in FW/HW post-attach
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: Success when fst parameters are programmed in FW, error otherwise
 */
QDF_STATUS dp_rx_flow_send_fst_fw_setup(struct dp_soc *soc,
					struct dp_pdev *pdev)
{
	struct dp_htt_rx_flow_fst_setup fisa_hw_fst_setup_cmd = {0};
	struct dp_rx_fst *fst = soc->rx_fst;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	QDF_STATUS status;

	/* check if FW has support to place FST in CMEM */
	dp_rx_fst_check_cmem_support(soc);

	/* mac_id = 0 is used to configure both macs with same FT */
	fisa_hw_fst_setup_cmd.pdev_id = 0;
	fisa_hw_fst_setup_cmd.max_entries = fst->max_entries;
	fisa_hw_fst_setup_cmd.max_search = fst->max_skid_length;
	if (soc->fst_cmem_base) {
		fisa_hw_fst_setup_cmd.base_addr_lo =
			soc->fst_cmem_base & 0xffffffff;
		/* Higher order bits are mostly 0, Always use 0x10 */
		fisa_hw_fst_setup_cmd.base_addr_hi =
			(soc->fst_cmem_base >> 32) | 0x10;
		dp_info("cmem base address 0x%llx\n", soc->fst_cmem_base);
	} else {
		fisa_hw_fst_setup_cmd.base_addr_lo =
			fst->hal_rx_fst_base_paddr & 0xffffffff;
		fisa_hw_fst_setup_cmd.base_addr_hi =
			(fst->hal_rx_fst_base_paddr >> 32);
	}

	fisa_hw_fst_setup_cmd.ip_da_sa_prefix =	HTT_RX_IPV4_COMPATIBLE_IPV6;
	fisa_hw_fst_setup_cmd.hash_key_len = HAL_FST_HASH_KEY_SIZE_BYTES;
	fisa_hw_fst_setup_cmd.hash_key = wlan_cfg_rx_fst_get_hash_key(cfg);

	status = dp_htt_rx_flow_fst_setup(pdev, &fisa_hw_fst_setup_cmd);

	if (!fst->fst_in_cmem || soc->fst_cmem_base) {
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

/**
 * dp_rx_fst_detach() - De-initialize Rx FST
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: None
 */
void dp_rx_fst_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *dp_fst;

	dp_fst = soc->rx_fst;
	if (qdf_likely(dp_fst)) {
		qdf_timer_sync_cancel(&dp_fst->fse_cache_flush_timer);
		if (dp_fst->fst_in_cmem)
			dp_rx_fst_cmem_deinit(dp_fst);
		else
			hal_rx_fst_detach(soc->hal_soc, dp_fst->hal_rx_fst,
					  soc->osdev, soc->fst_cmem_base);

		dp_rx_sw_ft_hist_deinit((struct dp_fisa_rx_sw_ft *)dp_fst->base,
					dp_fst->max_entries);
		dp_context_free_mem(soc, DP_FISA_RX_FT_TYPE, dp_fst->base);
		qdf_spinlock_destroy(&dp_fst->dp_rx_fst_lock);
		qdf_mem_free(dp_fst);
	}
	soc->rx_fst = NULL;
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
	struct dp_rx_fst *fst = soc->rx_fst;

	fst->max_entries = num_entries;
	fst->hash_mask = fst->max_entries - 1;
	fst->cmem_ba = cmem_ba_lo;

	/* Address is not NULL then address is already known during init */
	if (soc->fst_cmem_base == 0)
		qdf_event_set(&fst->cmem_resp_event);
}

void dp_rx_fst_update_pm_suspend_status(struct dp_soc *soc, bool suspended)
{
	struct dp_rx_fst *fst = soc->rx_fst;

	if (!fst)
		return;

	if (suspended)
		qdf_atomic_set(&fst->pm_suspended, 1);
	else
		qdf_atomic_set(&fst->pm_suspended, 0);
}

void dp_rx_fst_requeue_wq(struct dp_soc *soc)
{
	struct dp_rx_fst *fst = soc->rx_fst;

	if (!fst || !fst->fst_wq_defer)
		return;

	fst->fst_wq_defer = false;
	qdf_queue_work(fst->soc_hdl->osdev,
		       fst->fst_update_wq,
		       &fst->fst_update_work);

	dp_info("requeued defer fst update task");
}

#else /* WLAN_SUPPORT_RX_FISA */

#endif /* !WLAN_SUPPORT_RX_FISA */

