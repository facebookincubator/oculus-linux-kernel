/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "dp_types.h"
#include <dp_internal.h>
#include <dp_htt.h>
#include "dp_rh.h"
#include "dp_rh_tx.h"
#include "dp_rh_htt.h"
#include "dp_tx_desc.h"
#include "dp_rh_rx.h"
#include "dp_peer.h"
#include <wlan_utility.h>
#include <dp_rings.h>
#include <ce_api.h>
#include <ce_internal.h>

static QDF_STATUS
dp_srng_init_rh(struct dp_soc *soc, struct dp_srng *srng, int ring_type,
		int ring_num, int mac_id)
{
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

	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() ==
	    QDF_GLOBAL_MONITOR_MODE) {
		if (soc->intr_mode == DP_INTR_MSI &&
		    !dp_skip_msi_cfg(soc, ring_type)) {
			dp_srng_msi_setup(soc, srng, &ring_params,
					  ring_type, ring_num);
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
	}

	srng->hal_srng = hal_srng_setup(hal_soc, ring_type, ring_num,
					mac_id, &ring_params, 0);

	if (!srng->hal_srng) {
		dp_srng_free(soc, srng);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
dp_peer_setup_rh(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		 uint8_t *peer_mac,
		 struct cdp_peer_setup_info *setup_info)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_vdev *vdev = NULL;
	struct dp_peer *peer =
			dp_peer_find_hash_find(soc, peer_mac, 0, vdev_id,
					       DP_MOD_ID_CDP);
	enum wlan_op_mode vdev_opmode;

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

	dp_info("pdev: %d vdev :%d opmode:%u",
		pdev->pdev_id, vdev->vdev_id, vdev->opmode);

	/*
	 * There are corner cases where the AD1 = AD2 = "VAPs address"
	 * i.e both the devices have same MAC address. In these
	 * cases we want such pkts to be processed in NULL Q handler
	 * which is REO2TCL ring. for this reason we should
	 * not setup reo_queues and default route for bss_peer.
	 */
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

	if (vdev_opmode != wlan_op_mode_monitor)
		dp_peer_rx_init(pdev, peer);

	dp_peer_ppdu_delayed_ba_init(peer);

fail:
	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return status;
}

#ifdef AST_OFFLOAD_ENABLE
static void dp_peer_map_detach_rh(struct dp_soc *soc)
{
	dp_soc_wds_detach(soc);
	dp_peer_ast_table_detach(soc);
	dp_peer_ast_hash_detach(soc);
	dp_peer_mec_hash_detach(soc);
}

static QDF_STATUS dp_peer_map_attach_rh(struct dp_soc *soc)
{
	QDF_STATUS status;

	soc->max_peer_id = soc->max_peers;

	status = dp_peer_ast_table_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = dp_peer_ast_hash_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto ast_table_detach;

	status = dp_peer_mec_hash_attach(soc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto hash_detach;

	dp_soc_wds_attach(soc);

	return QDF_STATUS_SUCCESS;

hash_detach:
	dp_peer_ast_hash_detach(soc);
ast_table_detach:
	dp_peer_ast_table_detach(soc);

	return status;
}
#else
static void dp_peer_map_detach_rh(struct dp_soc *soc)
{
}

static QDF_STATUS dp_peer_map_attach_rh(struct dp_soc *soc)
{
	soc->max_peer_id = soc->max_peers;

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_soc_cfg_init_rh() - initialize target specific configuration
 *		       during dp_soc_init
 * @soc: dp soc handle
 */
static void dp_soc_cfg_init_rh(struct dp_soc *soc)
{
	uint32_t target_type;

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_WCN6450:
		wlan_cfg_set_raw_mode_war(soc->wlan_cfg_ctx, true);
		soc->ast_override_support = 1;
		soc->wlan_cfg_ctx->rxdma1_enable = 0;
		break;
	default:
		qdf_print("%s: Unknown tgt type %d\n", __func__, target_type);
		qdf_assert_always(0);
		break;
	}
}

static void dp_soc_cfg_attach_rh(struct dp_soc *soc)
{
	int target_type;

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_WCN6450:
		soc->wlan_cfg_ctx->rxdma1_enable = 0;
		break;
	default:
		qdf_print("%s: Unknown tgt type %d\n", __func__, target_type);
		qdf_assert_always(0);
		break;
	}

	/*
	 * keeping TCL and completion rings number, this data
	 * is equivalent number of TX interface rings.
	 */
	soc->num_tx_comp_rings =
		wlan_cfg_num_tx_comp_rings(soc->wlan_cfg_ctx);
	soc->num_tcl_data_rings =
		wlan_cfg_num_tcl_data_rings(soc->wlan_cfg_ctx);
}

qdf_size_t dp_get_context_size_rh(enum dp_context_type context_type)
{
	switch (context_type) {
	case DP_CONTEXT_TYPE_SOC:
		return sizeof(struct dp_soc_rh);
	case DP_CONTEXT_TYPE_PDEV:
		return sizeof(struct dp_pdev_rh);
	case DP_CONTEXT_TYPE_VDEV:
		return sizeof(struct dp_vdev_rh);
	case DP_CONTEXT_TYPE_PEER:
		return sizeof(struct dp_peer_rh);
	default:
		return 0;
	}
}

qdf_size_t dp_mon_get_context_size_rh(enum dp_context_type context_type)
{
	switch (context_type) {
	case DP_CONTEXT_TYPE_MON_PDEV:
		return sizeof(struct dp_mon_pdev_rh);
	case DP_CONTEXT_TYPE_MON_SOC:
		return sizeof(struct dp_mon_soc_rh);
	default:
		return 0;
	}
}

static QDF_STATUS dp_soc_attach_rh(struct dp_soc *soc,
				   struct cdp_soc_attach_params *params)
{
	soc->wbm_sw0_bm_id = hal_tx_get_wbm_sw0_bm_id();
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_detach_rh(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_deinit_rh(struct dp_soc *soc)
{
	struct htt_soc *htt_soc = soc->htt_handle;

	qdf_atomic_set(&soc->cmn_init_done, 0);

	/*Degister RX offload flush handlers*/
	hif_offld_flush_cb_deregister(soc->hif_handle);

	dp_monitor_soc_deinit(soc);

	/* free peer tables & AST tables allocated during peer_map_attach */
	if (soc->peer_map_attach_success) {
		dp_peer_find_detach(soc);
		dp_peer_map_detach_rh(soc);
		soc->peer_map_attach_success = FALSE;
	}

	qdf_flush_work(&soc->htt_stats.work);
	qdf_disable_work(&soc->htt_stats.work);

	qdf_spinlock_destroy(&soc->htt_stats.lock);

	qdf_spinlock_destroy(&soc->ast_lock);

	dp_peer_mec_spinlock_destroy(soc);

	qdf_nbuf_queue_free(&soc->htt_stats.msg);

	qdf_nbuf_queue_free(&soc->invalid_buf_queue);

	qdf_spinlock_destroy(&soc->rx.defrag.defrag_lock);

	qdf_spinlock_destroy(&soc->vdev_map_lock);

	dp_soc_tx_desc_sw_pools_deinit(soc);

	dp_soc_srng_deinit(soc);

	dp_hw_link_desc_ring_deinit(soc);

	dp_soc_print_inactive_objects(soc);
	qdf_spinlock_destroy(&soc->inactive_peer_list_lock);
	qdf_spinlock_destroy(&soc->inactive_vdev_list_lock);

	htt_soc_htc_dealloc(soc->htt_handle);

	htt_soc_detach(htt_soc);

	wlan_minidump_remove(soc, sizeof(*soc), soc->ctrl_psoc,
			     WLAN_MD_DP_SOC, "dp_soc");

	return QDF_STATUS_SUCCESS;
}

static void *dp_soc_init_rh(struct dp_soc *soc, HTC_HANDLE htc_handle,
			    struct hif_opaque_softc *hif_handle)
{
	struct htt_soc *htt_soc = (struct htt_soc *)soc->htt_handle;
	bool is_monitor_mode = false;
	uint8_t i;

	wlan_minidump_log(soc, sizeof(*soc), soc->ctrl_psoc,
			  WLAN_MD_DP_SOC, "dp_soc");

	soc->hif_handle = hif_handle;

	soc->hal_soc = hif_get_hal_handle(soc->hif_handle);
	if (!soc->hal_soc)
		goto fail1;

	htt_soc = htt_soc_attach(soc, htc_handle);
	if (!htt_soc)
		goto fail1;

	soc->htt_handle = htt_soc;

	if (htt_soc_htc_prealloc(htt_soc) != QDF_STATUS_SUCCESS)
		goto fail2;

	htt_set_htc_handle(htt_soc, htc_handle);

	dp_soc_cfg_init_rh(soc);

	dp_monitor_soc_cfg_init(soc);

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

	if (is_monitor_mode)
		wlan_cfg_fill_interrupt_mask(soc->wlan_cfg_ctx, 0,
					     soc->intr_mode, is_monitor_mode,
					     false, soc->umac_reset_supported);
	if (dp_soc_srng_init(soc)) {
		dp_init_err("%pK: dp_soc_srng_init failed", soc);
		goto fail3;
	}

	if (dp_htt_soc_initialize_rh(soc->htt_handle, soc->ctrl_psoc,
				     htt_get_htc_handle(htt_soc),
				     soc->hal_soc, soc->osdev) == NULL)
		goto fail4;

	/* Initialize descriptors in TCL Rings */
	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		hal_tx_init_data_ring(soc->hal_soc,
				      soc->tcl_data_ring[i].hal_srng);
	}

	if (dp_soc_tx_desc_sw_pools_init(soc)) {
		dp_init_err("%pK: dp_tx_soc_attach failed", soc);
		goto fail5;
	}

	wlan_cfg_set_rx_hash(soc->wlan_cfg_ctx,
			     cfg_get(soc->ctrl_psoc, CFG_DP_RX_HASH));
	soc->cce_disable = false;
	soc->max_ast_ageout_count = MAX_AST_AGEOUT_COUNT;

	soc->sta_mode_search_policy = DP_TX_ADDR_SEARCH_ADDR_POLICY;
	qdf_mem_zero(&soc->vdev_id_map, sizeof(soc->vdev_id_map));
	qdf_spinlock_create(&soc->vdev_map_lock);
	qdf_atomic_init(&soc->num_tx_outstanding);
	qdf_atomic_init(&soc->num_tx_exception);
	soc->num_tx_allowed =
		wlan_cfg_get_dp_soc_tx_device_limit(soc->wlan_cfg_ctx);

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

	qdf_nbuf_queue_init(&soc->invalid_buf_queue);

	TAILQ_INIT(&soc->inactive_peer_list);
	qdf_spinlock_create(&soc->inactive_peer_list_lock);
	TAILQ_INIT(&soc->inactive_vdev_list);
	qdf_spinlock_create(&soc->inactive_vdev_list_lock);
	qdf_spinlock_create(&soc->htt_stats.lock);
	/* initialize work queue for stats processing */
	qdf_create_work(0, &soc->htt_stats.work, htt_t2h_stats_handler, soc);

	/*Register RX offload flush handlers*/
	hif_offld_flush_cb_register(soc->hif_handle, dp_rx_data_flush);

	dp_info("Mem stats: DMA = %u HEAP = %u SKB = %u",
		qdf_dma_mem_stats_read(),
		qdf_heap_mem_stats_read(),
		qdf_skb_total_mem_stats_read());

	soc->vdev_stats_id_map = 0;

	return soc;
fail5:
	htt_soc_htc_dealloc(soc->htt_handle);
fail4:
	dp_soc_srng_deinit(soc);
fail3:
	htt_htc_pkt_pool_free(htt_soc);
fail2:
	htt_soc_detach(htt_soc);
fail1:
	return NULL;
}

static void dp_soc_interrupt_detach_rh(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	int i;

	if (soc->intr_mode == DP_INTR_POLL) {
		qdf_timer_free(&soc->int_timer);
	} else {
		hif_deconfigure_ext_group_interrupts(soc->hif_handle);
		hif_deregister_exec_group(soc->hif_handle, "dp_intr");
	}

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		soc->intr_ctx[i].rx_mon_ring_mask = 0;
		soc->intr_ctx[i].rxdma2host_ring_mask = 0;

		hif_event_history_deinit(soc->hif_handle, i);
		qdf_lro_deinit(soc->intr_ctx[i].lro_ctx);
	}

	qdf_mem_set(&soc->mon_intr_id_lmac_map,
		    sizeof(soc->mon_intr_id_lmac_map),
		    DP_MON_INVALID_LMAC_ID);
}

static QDF_STATUS dp_soc_interrupt_attach_rh(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	int i = 0;
	int num_irq = 0;
	int lmac_id = 0;
	int napi_scale;

	qdf_mem_set(&soc->mon_intr_id_lmac_map,
		    sizeof(soc->mon_intr_id_lmac_map), DP_MON_INVALID_LMAC_ID);

	if (soc->cdp_soc.ol_ops->get_con_mode &&
	    soc->cdp_soc.ol_ops->get_con_mode() !=
	    QDF_GLOBAL_MONITOR_MODE)
		return QDF_STATUS_SUCCESS;

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		int ret = 0;

		/* Map of IRQ ids registered with one interrupt context */
		int irq_id_map[HIF_MAX_GRP_IRQ];

		int rx_mon_mask =
			dp_soc_get_mon_mask_for_interrupt_mode(soc, i);
		int rxdma2host_ring_mask =
			wlan_cfg_get_rxdma2host_ring_mask(soc->wlan_cfg_ctx, i);

		soc->intr_ctx[i].dp_intr_id = i;
		soc->intr_ctx[i].rx_mon_ring_mask = rx_mon_mask;
		soc->intr_ctx[i].rxdma2host_ring_mask = rxdma2host_ring_mask;
		soc->intr_ctx[i].soc = soc;

		num_irq = 0;

		dp_soc_interrupt_map_calculate(soc, i, &irq_id_map[0],
					       &num_irq);

		napi_scale = wlan_cfg_get_napi_scale_factor(soc->wlan_cfg_ctx);
		if (!napi_scale)
			napi_scale = QCA_NAPI_DEF_SCALE_BIN_SHIFT;

		ret = hif_register_ext_group(soc->hif_handle,
					     num_irq, irq_id_map, dp_service_srngs_wrapper,
					     &soc->intr_ctx[i], "dp_intr",
					     HIF_EXEC_NAPI_TYPE, napi_scale);

		dp_debug(" int ctx %u num_irq %u irq_id_map %u %u",
			 i, num_irq, irq_id_map[0], irq_id_map[1]);

		if (ret) {
			dp_init_err("%pK: failed, ret = %d", soc, ret);
			dp_soc_interrupt_detach_rh(txrx_soc);
			return QDF_STATUS_E_FAILURE;
		}

		hif_event_history_init(soc->hif_handle, i);
		soc->intr_ctx[i].lro_ctx = qdf_lro_init();

		if (dp_is_mon_mask_valid(soc, &soc->intr_ctx[i])) {
			soc->mon_intr_id_lmac_map[lmac_id] = i;
			lmac_id++;
		}
	}

	hif_configure_ext_group_interrupts(soc->hif_handle);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_attach_poll_rh(struct cdp_soc_t *txrx_soc)
{
	struct dp_soc *soc = (struct dp_soc *)txrx_soc;
	uint32_t lmac_id = 0;
	int i;

	qdf_mem_set(&soc->mon_intr_id_lmac_map,
		    sizeof(soc->mon_intr_id_lmac_map), DP_MON_INVALID_LMAC_ID);
	soc->intr_mode = DP_INTR_POLL;

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		soc->intr_ctx[i].rx_mon_ring_mask =
				wlan_cfg_get_rx_mon_ring_mask(soc->wlan_cfg_ctx, i);

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

static uint32_t dp_service_srngs_rh(void *dp_ctx, uint32_t dp_budget, int cpu)
{
	struct dp_intr *int_ctx = (struct dp_intr *)dp_ctx;
	struct dp_soc *soc = int_ctx->soc;
	uint32_t work_done  = 0;
	int budget = dp_budget;
	uint32_t remaining_quota = dp_budget;

	if (qdf_unlikely(!dp_monitor_is_vdev_timer_running(soc))) {
		work_done = dp_process_lmac_rings(int_ctx, remaining_quota);
		if (work_done) {
			budget -=  work_done;
			if (budget <= 0)
				goto budget_done;
			remaining_quota = budget;
		}
	}

budget_done:
	qdf_atomic_clear_bit(cpu, &soc->service_rings_running);

	if (soc->notify_fw_callback)
		soc->notify_fw_callback(soc);

	return dp_budget - budget;
}

/**
 * dp_pdev_fill_tx_endpoint_info_rh() - Prefill fixed TX endpoint information
 *					that is used during packet transmit
 * @pdev: Handle to DP pdev struct
 *
 * Return: QDF_STATUS_SUCCESS/QDF_STATUS_E_NOENT
 */
static QDF_STATUS dp_pdev_fill_tx_endpoint_info_rh(struct dp_pdev *pdev)
{
	struct dp_pdev_rh *rh_pdev = dp_get_rh_pdev_from_dp_pdev(pdev);
	struct dp_soc_rh *rh_soc = dp_get_rh_soc_from_dp_soc(pdev->soc);
	struct dp_tx_ep_info_rh *tx_ep_info = &rh_pdev->tx_ep_info;
	struct hif_opaque_softc *hif_handle = pdev->soc->hif_handle;
	int ul_is_polled, dl_is_polled;
	uint8_t ul_pipe, dl_pipe;
	int status;

	status = hif_map_service_to_pipe(hif_handle, HTT_DATA2_MSG_SVC,
					 &ul_pipe, &dl_pipe,
					 &ul_is_polled, &dl_is_polled);
	if (status) {
		hif_err("Failed to map tx pipe: %d", status);
		return QDF_STATUS_E_NOENT;
	}

	tx_ep_info->ce_tx_hdl = hif_get_ce_handle(hif_handle, ul_pipe);

	tx_ep_info->download_len = HAL_TX_DESC_LEN_BYTES +
				   sizeof(struct tlv_32_hdr) +
				   DP_RH_TX_HDR_SIZE_OUTER_HDR_MAX +
				   DP_RH_TX_HDR_SIZE_802_1Q +
				   DP_RH_TX_HDR_SIZE_LLC_SNAP +
				   DP_RH_TX_HDR_SIZE_IP;

	tx_ep_info->tx_endpoint = rh_soc->tx_endpoint;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_pdev_attach_rh(struct dp_pdev *pdev,
				    struct cdp_pdev_attach_params *params)
{
	return dp_pdev_fill_tx_endpoint_info_rh(pdev);
}

static QDF_STATUS dp_pdev_detach_rh(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_vdev_attach_rh(struct dp_soc *soc, struct dp_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_vdev_detach_rh(struct dp_soc *soc, struct dp_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

qdf_size_t dp_get_soc_context_size_rh(void)
{
	return sizeof(struct dp_soc_rh);
}

#ifdef NO_RX_PKT_HDR_TLV
/**
 * dp_rxdma_ring_sel_cfg_rh() - Setup RXDMA ring config
 * @soc: Common DP soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_rxdma_ring_sel_cfg_rh(struct dp_soc *soc)
{
	int i;
	int mac_id;
	struct htt_rx_ring_tlv_filter htt_tlv_filter = {0};
	struct dp_srng *rx_mac_srng;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	htt_tlv_filter.mpdu_start = 1;
	htt_tlv_filter.msdu_start = 1;
	htt_tlv_filter.mpdu_end = 1;
	htt_tlv_filter.msdu_end = 1;
	htt_tlv_filter.attention = 1;
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
					 FILTER_DATA_MCAST |
					 FILTER_DATA_DATA);
	htt_tlv_filter.mo_mgmt_filter = 0;
	htt_tlv_filter.mo_ctrl_filter = 0;
	htt_tlv_filter.mo_data_filter = 0;
	htt_tlv_filter.md_data_filter = 0;

	htt_tlv_filter.offset_valid = true;

	htt_tlv_filter.rx_packet_offset = soc->rx_pkt_tlv_size;
	/*Not subscribing rx_pkt_header*/
	htt_tlv_filter.rx_header_offset = 0;
	htt_tlv_filter.rx_mpdu_start_offset =
				hal_rx_mpdu_start_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_mpdu_end_offset =
				hal_rx_mpdu_end_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_msdu_start_offset =
				hal_rx_msdu_start_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_msdu_end_offset =
				hal_rx_msdu_end_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_attn_offset =
				hal_rx_attn_offset_get(soc->hal_soc);

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
			htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					    rx_mac_srng->hal_srng,
					    RXDMA_BUF, buf_size,
					    &htt_tlv_filter);
		}
	}

	if (QDF_IS_STATUS_SUCCESS(status))
		status = dp_htt_h2t_rx_ring_rfs_cfg(soc->htt_handle);

	return status;
}
#else

static QDF_STATUS
dp_rxdma_ring_sel_cfg_rh(struct dp_soc *soc)
{
	int i;
	int mac_id;
	struct htt_rx_ring_tlv_filter htt_tlv_filter = {0};
	struct dp_srng *rx_mac_srng;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	htt_tlv_filter.mpdu_start = 1;
	htt_tlv_filter.msdu_start = 1;
	htt_tlv_filter.mpdu_end = 1;
	htt_tlv_filter.msdu_end = 1;
	htt_tlv_filter.attention = 1;
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
					 FILTER_DATA_MCAST |
					 FILTER_DATA_DATA);
	htt_tlv_filter.mo_mgmt_filter = 0;
	htt_tlv_filter.mo_ctrl_filter = 0;
	htt_tlv_filter.mo_data_filter = 0;
	htt_tlv_filter.md_data_filter = 0;

	htt_tlv_filter.offset_valid = true;

	htt_tlv_filter.rx_packet_offset = soc->rx_pkt_tlv_size;
	htt_tlv_filter.rx_header_offset =
				hal_rx_pkt_tlv_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_mpdu_start_offset =
				hal_rx_mpdu_start_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_mpdu_end_offset =
				hal_rx_mpdu_end_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_msdu_start_offset =
				hal_rx_msdu_start_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_msdu_end_offset =
				hal_rx_msdu_end_offset_get(soc->hal_soc);
	htt_tlv_filter.rx_attn_offset =
				hal_rx_attn_offset_get(soc->hal_soc);

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
			htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					    rx_mac_srng->hal_srng,
					    RXDMA_BUF, buf_size,
					    &htt_tlv_filter);
		}
	}

	if (QDF_IS_STATUS_SUCCESS(status))
		status = dp_htt_h2t_rx_ring_rfs_cfg(soc->htt_handle);

	return status;
}
#endif

static void dp_soc_srng_deinit_rh(struct dp_soc *soc)
{
}

static void dp_soc_srng_free_rh(struct dp_soc *soc)
{
}

static QDF_STATUS dp_soc_srng_alloc_rh(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_srng_init_rh(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_tx_implicit_rbm_set_rh(struct dp_soc *soc,
				      uint8_t tx_ring_id,
				      uint8_t bm_id)
{
}

static QDF_STATUS dp_txrx_set_vdev_param_rh(struct dp_soc *soc,
					    struct dp_vdev *vdev,
					    enum cdp_vdev_param_type param,
					    cdp_config_param_type val)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_get_rx_hash_key_rh(struct dp_soc *soc,
				  struct cdp_lro_hash_config *lro_hash)
{
	dp_get_rx_hash_key_bytes(lro_hash);
}

#if defined(DP_POWER_SAVE) || defined(FEATURE_RUNTIME_PM)
static void dp_update_ring_hptp_rh(struct dp_soc *soc, bool force_flush)
{
	struct dp_pdev_rh *rh_pdev =
			dp_get_rh_pdev_from_dp_pdev(soc->pdev_list[0]);
	struct dp_tx_ep_info_rh *tx_ep_info = &rh_pdev->tx_ep_info;

	ce_flush_tx_ring_write_idx(tx_ep_info->ce_tx_hdl, force_flush);
}
#endif

void dp_initialize_arch_ops_rh(struct dp_arch_ops *arch_ops)
{
	arch_ops->tx_hw_enqueue = dp_tx_hw_enqueue_rh;
	arch_ops->tx_comp_get_params_from_hal_desc =
		dp_tx_comp_get_params_from_hal_desc_rh;
	arch_ops->dp_tx_process_htt_completion =
			dp_tx_process_htt_completion_rh;
	arch_ops->dp_wbm_get_rx_desc_from_hal_desc =
			dp_wbm_get_rx_desc_from_hal_desc_rh;
	arch_ops->dp_tx_desc_pool_alloc = dp_tx_desc_pool_alloc_rh;
	arch_ops->dp_tx_desc_pool_free = dp_tx_desc_pool_free_rh;
	arch_ops->dp_tx_desc_pool_init = dp_tx_desc_pool_init_rh;
	arch_ops->dp_tx_desc_pool_deinit = dp_tx_desc_pool_deinit_rh;
	arch_ops->dp_rx_desc_pool_init = dp_rx_desc_pool_init_rh;
	arch_ops->dp_rx_desc_pool_deinit = dp_rx_desc_pool_deinit_rh;
	arch_ops->dp_tx_compute_hw_delay = dp_tx_compute_tx_delay_rh;
	arch_ops->txrx_get_context_size = dp_get_context_size_rh;
	arch_ops->txrx_get_mon_context_size = dp_mon_get_context_size_rh;
	arch_ops->txrx_soc_attach = dp_soc_attach_rh;
	arch_ops->txrx_soc_detach = dp_soc_detach_rh;
	arch_ops->txrx_soc_init = dp_soc_init_rh;
	arch_ops->txrx_soc_deinit = dp_soc_deinit_rh;
	arch_ops->txrx_soc_srng_alloc = dp_soc_srng_alloc_rh;
	arch_ops->txrx_soc_srng_init = dp_soc_srng_init_rh;
	arch_ops->txrx_soc_srng_deinit = dp_soc_srng_deinit_rh;
	arch_ops->txrx_soc_srng_free = dp_soc_srng_free_rh;
	arch_ops->txrx_pdev_attach = dp_pdev_attach_rh;
	arch_ops->txrx_pdev_detach = dp_pdev_detach_rh;
	arch_ops->txrx_vdev_attach = dp_vdev_attach_rh;
	arch_ops->txrx_vdev_detach = dp_vdev_detach_rh;
	arch_ops->txrx_peer_map_attach = dp_peer_map_attach_rh;
	arch_ops->txrx_peer_map_detach = dp_peer_map_detach_rh;
	arch_ops->get_rx_hash_key = dp_get_rx_hash_key_rh;
	arch_ops->dp_rx_desc_cookie_2_va =
			dp_rx_desc_cookie_2_va_rh;
	arch_ops->dp_rx_intrabss_mcast_handler =
					dp_rx_intrabss_handle_nawds_rh;
	arch_ops->dp_rx_word_mask_subscribe = dp_rx_word_mask_subscribe_rh;
	arch_ops->dp_rxdma_ring_sel_cfg = dp_rxdma_ring_sel_cfg_rh;
	arch_ops->dp_rx_peer_metadata_peer_id_get =
					dp_rx_peer_metadata_peer_id_get_rh;
	arch_ops->soc_cfg_attach = dp_soc_cfg_attach_rh;
	arch_ops->tx_implicit_rbm_set = dp_tx_implicit_rbm_set_rh;
	arch_ops->txrx_set_vdev_param = dp_txrx_set_vdev_param_rh;
	arch_ops->txrx_print_peer_stats = dp_print_peer_txrx_stats_rh;
	arch_ops->dp_peer_rx_reorder_queue_setup =
					dp_peer_rx_reorder_queue_setup_rh;
	arch_ops->peer_get_reo_hash = dp_peer_get_reo_hash_rh;
	arch_ops->reo_remap_config = dp_reo_remap_config_rh;
	arch_ops->txrx_peer_setup = dp_peer_setup_rh;
	arch_ops->txrx_srng_init = dp_srng_init_rh;
#if defined(DP_POWER_SAVE) || defined(FEATURE_RUNTIME_PM)
	arch_ops->dp_update_ring_hptp = dp_update_ring_hptp_rh;
#endif
	arch_ops->dp_flush_tx_ring = dp_flush_tx_ring_rh;
	arch_ops->dp_soc_interrupt_attach = dp_soc_interrupt_attach_rh;
	arch_ops->dp_soc_attach_poll = dp_soc_attach_poll_rh;
	arch_ops->dp_soc_interrupt_detach = dp_soc_interrupt_detach_rh;
	arch_ops->dp_service_srngs = dp_service_srngs_rh;
}
