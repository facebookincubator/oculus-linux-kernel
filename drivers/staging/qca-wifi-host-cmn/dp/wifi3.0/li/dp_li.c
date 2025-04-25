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

#include "dp_types.h"
#include "dp_rings.h"
#include <dp_internal.h>
#include <dp_htt.h>
#include "dp_li.h"
#include "dp_li_tx.h"
#include "dp_tx_desc.h"
#include "dp_li_rx.h"
#include "dp_peer.h"
#include <wlan_utility.h>
#include "dp_ipa.h"
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon_1.0.h>
#endif

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
static struct wlan_cfg_tcl_wbm_ring_num_map g_tcl_wbm_map_array[MAX_TCL_DATA_RINGS] = {
	{.tcl_ring_num = 0, .wbm_ring_num = 0, .wbm_rbm_id = HAL_LI_WBM_SW0_BM_ID, .for_ipa = 0},
	/*
	 * INVALID_WBM_RING_NUM implies re-use of an existing WBM2SW ring
	 * as indicated by rbm id.
	 */
	{1, INVALID_WBM_RING_NUM, HAL_LI_WBM_SW0_BM_ID, 0},
	{2, 2, HAL_LI_WBM_SW2_BM_ID, 0}
};
#else
static struct wlan_cfg_tcl_wbm_ring_num_map g_tcl_wbm_map_array[MAX_TCL_DATA_RINGS] = {
	{.tcl_ring_num = 0, .wbm_ring_num = 0, .wbm_rbm_id = HAL_LI_WBM_SW0_BM_ID, .for_ipa = 0},
	{1, 1, HAL_LI_WBM_SW1_BM_ID, 0},
	{2, 2, HAL_LI_WBM_SW2_BM_ID, 0},
	/*
	 * Although using wbm_ring 4, wbm_ring 3 is mentioned in order to match
	 * with the tx_mask in dp_service_srngs. Please be careful while using
	 * this table anywhere else.
	 */
	{3, 3, HAL_LI_WBM_SW4_BM_ID, 0}
};
#endif

#ifdef IPA_WDI3_TX_TWO_PIPES
static inline void
dp_soc_cfg_update_tcl_wbm_map_for_ipa(struct wlan_cfg_dp_soc_ctxt *cfg_ctx)
{
	if (!cfg_ctx->ipa_enabled)
		return;

	cfg_ctx->tcl_wbm_map_array[IPA_TX_ALT_RING_IDX].wbm_ring_num = 4;
	cfg_ctx->tcl_wbm_map_array[IPA_TX_ALT_RING_IDX].wbm_rbm_id =
							   HAL_LI_WBM_SW4_BM_ID;
}
#else
static inline void
dp_soc_cfg_update_tcl_wbm_map_for_ipa(struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx)
{
}
#endif

static void dp_soc_cfg_attach_li(struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;

	dp_soc_cfg_attach(soc);

	wlan_cfg_set_rx_rel_ring_id(soc_cfg_ctx, WBM2SW_REL_ERR_RING_NUM);

	soc_cfg_ctx->tcl_wbm_map_array = g_tcl_wbm_map_array;
	dp_soc_cfg_update_tcl_wbm_map_for_ipa(soc_cfg_ctx);
}

qdf_size_t dp_get_context_size_li(enum dp_context_type context_type)
{
	switch (context_type) {
	case DP_CONTEXT_TYPE_SOC:
		return sizeof(struct dp_soc_li);
	case DP_CONTEXT_TYPE_PDEV:
		return sizeof(struct dp_pdev_li);
	case DP_CONTEXT_TYPE_VDEV:
		return sizeof(struct dp_vdev_li);
	case DP_CONTEXT_TYPE_PEER:
		return sizeof(struct dp_peer_li);
	default:
		return 0;
	}
}

static QDF_STATUS dp_soc_attach_li(struct dp_soc *soc,
				   struct cdp_soc_attach_params *params)
{
	soc->wbm_sw0_bm_id = hal_tx_get_wbm_sw0_bm_id();

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_detach_li(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_soc_interrupt_attach_li(struct cdp_soc_t *txrx_soc)
{
	return dp_soc_interrupt_attach(txrx_soc);
}

static QDF_STATUS dp_soc_attach_poll_li(struct cdp_soc_t *txrx_soc)
{
	return dp_soc_attach_poll(txrx_soc);
}

static void dp_soc_interrupt_detach_li(struct cdp_soc_t *txrx_soc)
{
	return dp_soc_interrupt_detach(txrx_soc);
}

static uint32_t dp_service_srngs_li(void *dp_ctx, uint32_t dp_budget, int cpu)
{
	return dp_service_srngs(dp_ctx, dp_budget, cpu);
}

static void *dp_soc_init_li(struct dp_soc *soc, HTC_HANDLE htc_handle,
			    struct hif_opaque_softc *hif_handle)
{
	wlan_minidump_log(soc, sizeof(*soc), soc->ctrl_psoc,
			  WLAN_MD_DP_SOC, "dp_soc");

	soc->hif_handle = hif_handle;

	soc->hal_soc = hif_get_hal_handle(soc->hif_handle);
	if (!soc->hal_soc)
		return NULL;

	return dp_soc_init(soc, htc_handle, hif_handle);
}

static QDF_STATUS dp_soc_deinit_li(struct dp_soc *soc)
{
	qdf_atomic_set(&soc->cmn_init_done, 0);

	dp_soc_deinit(soc);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_pdev_attach_li(struct dp_pdev *pdev,
				    struct cdp_pdev_attach_params *params)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_pdev_detach_li(struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_vdev_attach_li(struct dp_soc *soc, struct dp_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS dp_vdev_detach_li(struct dp_soc *soc, struct dp_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

#ifdef AST_OFFLOAD_ENABLE
static void dp_peer_map_detach_li(struct dp_soc *soc)
{
	dp_soc_wds_detach(soc);
	dp_peer_ast_table_detach(soc);
	dp_peer_ast_hash_detach(soc);
	dp_peer_mec_hash_detach(soc);
}

static QDF_STATUS dp_peer_map_attach_li(struct dp_soc *soc)
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
static void dp_peer_map_detach_li(struct dp_soc *soc)
{
}

static QDF_STATUS dp_peer_map_attach_li(struct dp_soc *soc)
{
	soc->max_peer_id = soc->max_peers;

	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS dp_peer_setup_li(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				   uint8_t *peer_mac,
				   struct cdp_peer_setup_info *setup_info)
{
	return dp_peer_setup_wifi3(soc_hdl, vdev_id, peer_mac, setup_info);
}

qdf_size_t dp_get_soc_context_size_li(void)
{
	return sizeof(struct dp_soc);
}

#ifdef NO_RX_PKT_HDR_TLV
/**
 * dp_rxdma_ring_sel_cfg_li() - Setup RXDMA ring config
 * @soc: Common DP soc handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_rxdma_ring_sel_cfg_li(struct dp_soc *soc)
{
	int i;
	int mac_id;
	struct htt_rx_ring_tlv_filter htt_tlv_filter = {0};
	struct dp_srng *rx_mac_srng;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t target_type = hal_get_target_type(soc->hal_soc);
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	if (target_type == TARGET_TYPE_QCN9160)
		return status;

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
	return status;
}
#else

static QDF_STATUS
dp_rxdma_ring_sel_cfg_li(struct dp_soc *soc)
{
	int i;
	int mac_id;
	struct htt_rx_ring_tlv_filter htt_tlv_filter = {0};
	struct dp_srng *rx_mac_srng;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t target_type = hal_get_target_type(soc->hal_soc);
	uint16_t buf_size;

	buf_size = wlan_cfg_rx_buffer_size(soc->wlan_cfg_ctx);

	if (target_type == TARGET_TYPE_QCN9160)
		return status;

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
	return status;

}
#endif

static inline
QDF_STATUS dp_srng_init_li(struct dp_soc *soc, struct dp_srng *srng,
			   int ring_type, int ring_num, int mac_id)
{
	return dp_srng_init_idx(soc, srng, ring_type, ring_num, mac_id, 0);
}

#ifdef QCA_DP_ENABLE_TX_COMP_RING4
static inline
void dp_deinit_txcomp_ring4(struct dp_soc *soc)
{
	if (soc) {
		wlan_minidump_remove(soc->tx_comp_ring[3].base_vaddr_unaligned,
				     soc->tx_comp_ring[3].alloc_size,
				     soc->ctrl_psoc, WLAN_MD_DP_SRNG_TX_COMP,
				     "Transmit_completion_ring");
		dp_srng_deinit(soc, &soc->tx_comp_ring[3], WBM2SW_RELEASE, 0);
	}
}

static inline
QDF_STATUS dp_init_txcomp_ring4(struct dp_soc *soc)
{
	if (soc) {
		if (dp_srng_init(soc, &soc->tx_comp_ring[3],
				 WBM2SW_RELEASE, WBM2SW_TXCOMP_RING4_NUM, 0)) {
			dp_err("%pK: dp_srng_init failed for rx_rel_ring",
			       soc);
			return QDF_STATUS_E_FAILURE;
		}
		wlan_minidump_log(soc->tx_comp_ring[3].base_vaddr_unaligned,
				  soc->tx_comp_ring[3].alloc_size,
				  soc->ctrl_psoc, WLAN_MD_DP_SRNG_TX_COMP,
				  "Transmit_completion_ring");
	}
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_free_txcomp_ring4(struct dp_soc *soc)
{
	if (soc)
		dp_srng_free(soc, &soc->tx_comp_ring[3]);
}

static inline
QDF_STATUS dp_alloc_txcomp_ring4(struct dp_soc *soc, uint32_t tx_comp_ring_size,
				 uint32_t cached)
{
	if (soc) {
		if (dp_srng_alloc(soc, &soc->tx_comp_ring[3], WBM2SW_RELEASE,
				  tx_comp_ring_size, cached)) {
			dp_err("dp_srng_alloc failed for tx_comp_ring");
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}
#else
static inline
void dp_deinit_txcomp_ring4(struct dp_soc *soc)
{
}

static inline
QDF_STATUS dp_init_txcomp_ring4(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void dp_free_txcomp_ring4(struct dp_soc *soc)
{
}

static inline
QDF_STATUS dp_alloc_txcomp_ring4(struct dp_soc *soc, uint32_t tx_comp_ring_size,
				 uint32_t cached)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static void dp_soc_srng_deinit_li(struct dp_soc *soc)
{
	/* Tx Complete ring */
	dp_deinit_txcomp_ring4(soc);
}

static void dp_soc_srng_free_li(struct dp_soc *soc)
{
	dp_free_txcomp_ring4(soc);
}

static QDF_STATUS dp_soc_srng_alloc_li(struct dp_soc *soc)
{
	uint32_t tx_comp_ring_size;
	uint32_t cached = WLAN_CFG_DST_RING_CACHED_DESC;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;

	soc_cfg_ctx = soc->wlan_cfg_ctx;

	tx_comp_ring_size = wlan_cfg_tx_comp_ring_size(soc_cfg_ctx);
	/* Disable cached desc if NSS offload is enabled */
	if (wlan_cfg_get_dp_soc_nss_cfg(soc_cfg_ctx))
		cached = 0;

	if (dp_alloc_txcomp_ring4(soc, tx_comp_ring_size, cached))
		goto fail1;
	return QDF_STATUS_SUCCESS;
fail1:
	dp_soc_srng_free_li(soc);
	return QDF_STATUS_E_NOMEM;
}

static QDF_STATUS dp_soc_srng_init_li(struct dp_soc *soc)
{
	/* Tx comp ring 3 */
	if (dp_init_txcomp_ring4(soc))
		goto fail1;

	return QDF_STATUS_SUCCESS;
fail1:
	/*
	 * Cleanup will be done as part of soc_detach, which will
	 * be called on pdev attach failure
	 */
	dp_soc_srng_deinit_li(soc);
	return QDF_STATUS_E_FAILURE;
}

static void dp_tx_implicit_rbm_set_li(struct dp_soc *soc,
				      uint8_t tx_ring_id,
				      uint8_t bm_id)
{
}

static QDF_STATUS dp_txrx_set_vdev_param_li(struct dp_soc *soc,
					    struct dp_vdev *vdev,
					    enum cdp_vdev_param_type param,
					    cdp_config_param_type val)
{
	return QDF_STATUS_SUCCESS;
}

bool
dp_rx_intrabss_handle_nawds_li(struct dp_soc *soc, struct dp_txrx_peer *ta_peer,
			       qdf_nbuf_t nbuf_copy,
			       struct cdp_tid_rx_stats *tid_stats,
			       uint8_t link_id)
{
	return false;
}

static void dp_rx_word_mask_subscribe_li(struct dp_soc *soc,
					 uint32_t *msg_word,
					 void *rx_filter)
{
}

static void dp_get_rx_hash_key_li(struct dp_soc *soc,
				  struct cdp_lro_hash_config *lro_hash)
{
	dp_get_rx_hash_key_bytes(lro_hash);
}

static void dp_peer_get_reo_hash_li(struct dp_vdev *vdev,
				    struct cdp_peer_setup_info *setup_info,
				    enum cdp_host_reo_dest_ring *reo_dest,
				    bool *hash_based,
				    uint8_t *lmac_peer_id_msb)
{
	dp_vdev_get_default_reo_hash(vdev, reo_dest, hash_based);
}

static bool dp_reo_remap_config_li(struct dp_soc *soc,
				   uint32_t *remap0,
				   uint32_t *remap1,
				   uint32_t *remap2)
{
	return dp_reo_remap_config(soc, remap0, remap1, remap2);
}

static uint8_t dp_soc_get_num_soc_li(struct dp_soc *soc)
{
	return 1;
}

static QDF_STATUS dp_txrx_get_vdev_mcast_param_li(struct dp_soc *soc,
						  struct dp_vdev *vdev,
						  cdp_config_param_type *val)
{
	return QDF_STATUS_SUCCESS;
}

static uint8_t dp_get_hw_link_id_li(struct dp_pdev *pdev)
{
	return 0;
}

static void dp_get_vdev_stats_for_unmap_peer_li(
					struct dp_vdev *vdev,
					struct dp_peer *peer)
{
	dp_get_vdev_stats_for_unmap_peer_legacy(vdev, peer);
}

static struct
dp_soc *dp_get_soc_by_chip_id_li(struct dp_soc *soc,
				 uint8_t chip_id)
{
	return soc;
}

void dp_initialize_arch_ops_li(struct dp_arch_ops *arch_ops)
{
#ifndef QCA_HOST_MODE_WIFI_DISABLED
	arch_ops->tx_hw_enqueue = dp_tx_hw_enqueue_li;
	arch_ops->dp_rx_process = dp_rx_process_li;
	arch_ops->dp_tx_send_fast = dp_tx_send;
	arch_ops->tx_comp_get_params_from_hal_desc =
		dp_tx_comp_get_params_from_hal_desc_li;
	arch_ops->dp_tx_process_htt_completion =
			dp_tx_process_htt_completion_li;
	arch_ops->dp_wbm_get_rx_desc_from_hal_desc =
			dp_wbm_get_rx_desc_from_hal_desc_li;
	arch_ops->dp_tx_desc_pool_alloc = dp_tx_desc_pool_alloc_li;
	arch_ops->dp_tx_desc_pool_free = dp_tx_desc_pool_free_li;
	arch_ops->dp_tx_desc_pool_init = dp_tx_desc_pool_init_li;
	arch_ops->dp_tx_desc_pool_deinit = dp_tx_desc_pool_deinit_li;
	arch_ops->dp_rx_desc_pool_init = dp_rx_desc_pool_init_li;
	arch_ops->dp_rx_desc_pool_deinit = dp_rx_desc_pool_deinit_li;
	arch_ops->dp_tx_compute_hw_delay = dp_tx_compute_tx_delay_li;
	arch_ops->dp_rx_chain_msdus = dp_rx_chain_msdus_li;
	arch_ops->dp_rx_wbm_err_reap_desc = dp_rx_wbm_err_reap_desc_li;
	arch_ops->dp_rx_null_q_desc_handle = dp_rx_null_q_desc_handle_li;
#else
	arch_ops->dp_rx_desc_pool_init = dp_rx_desc_pool_init_generic;
	arch_ops->dp_rx_desc_pool_deinit = dp_rx_desc_pool_deinit_generic;
#endif
	arch_ops->txrx_get_context_size = dp_get_context_size_li;
#ifdef WIFI_MONITOR_SUPPORT
	arch_ops->txrx_get_mon_context_size = dp_mon_get_context_size_li;
#endif
	arch_ops->txrx_soc_attach = dp_soc_attach_li;
	arch_ops->txrx_soc_detach = dp_soc_detach_li;
	arch_ops->txrx_soc_init = dp_soc_init_li;
	arch_ops->txrx_soc_deinit = dp_soc_deinit_li;
	arch_ops->txrx_soc_srng_alloc = dp_soc_srng_alloc_li;
	arch_ops->txrx_soc_srng_init = dp_soc_srng_init_li;
	arch_ops->txrx_soc_srng_deinit = dp_soc_srng_deinit_li;
	arch_ops->txrx_soc_srng_free = dp_soc_srng_free_li;
	arch_ops->txrx_pdev_attach = dp_pdev_attach_li;
	arch_ops->txrx_pdev_detach = dp_pdev_detach_li;
	arch_ops->txrx_vdev_attach = dp_vdev_attach_li;
	arch_ops->txrx_vdev_detach = dp_vdev_detach_li;
	arch_ops->txrx_peer_map_attach = dp_peer_map_attach_li;
	arch_ops->txrx_peer_map_detach = dp_peer_map_detach_li;
	arch_ops->get_rx_hash_key = dp_get_rx_hash_key_li;
	arch_ops->dp_set_rx_fst = NULL;
	arch_ops->dp_get_rx_fst = NULL;
	arch_ops->dp_rx_fst_ref = NULL;
	arch_ops->dp_rx_fst_deref = NULL;
	arch_ops->txrx_peer_setup = dp_peer_setup_li;
	arch_ops->dp_rx_desc_cookie_2_va =
			dp_rx_desc_cookie_2_va_li;
	arch_ops->dp_rx_intrabss_mcast_handler =
					dp_rx_intrabss_handle_nawds_li;
	arch_ops->dp_rx_word_mask_subscribe = dp_rx_word_mask_subscribe_li;
	arch_ops->dp_rxdma_ring_sel_cfg = dp_rxdma_ring_sel_cfg_li;
	arch_ops->dp_rx_peer_metadata_peer_id_get =
					dp_rx_peer_metadata_peer_id_get_li;
	arch_ops->soc_cfg_attach = dp_soc_cfg_attach_li;
	arch_ops->tx_implicit_rbm_set = dp_tx_implicit_rbm_set_li;
	arch_ops->txrx_set_vdev_param = dp_txrx_set_vdev_param_li;
	arch_ops->txrx_print_peer_stats = dp_print_peer_txrx_stats_li;
	arch_ops->dp_peer_rx_reorder_queue_setup =
					dp_peer_rx_reorder_queue_setup_li;
	arch_ops->peer_get_reo_hash = dp_peer_get_reo_hash_li;
	arch_ops->reo_remap_config = dp_reo_remap_config_li;
	arch_ops->dp_get_soc_by_chip_id = dp_get_soc_by_chip_id_li;
	arch_ops->dp_soc_get_num_soc = dp_soc_get_num_soc_li;
	arch_ops->get_reo_qdesc_addr = dp_rx_get_reo_qdesc_addr_li;
	arch_ops->txrx_get_vdev_mcast_param = dp_txrx_get_vdev_mcast_param_li;
	arch_ops->get_hw_link_id = dp_get_hw_link_id_li;
	arch_ops->txrx_srng_init = dp_srng_init_li;
	arch_ops->dp_get_vdev_stats_for_unmap_peer =
					dp_get_vdev_stats_for_unmap_peer_li;
	arch_ops->dp_get_interface_stats = dp_txrx_get_vdev_stats;
#if defined(DP_POWER_SAVE) || defined(FEATURE_RUNTIME_PM)
	arch_ops->dp_update_ring_hptp = dp_update_ring_hptp;
#endif
	arch_ops->dp_flush_tx_ring = dp_flush_tcl_ring;
	arch_ops->dp_soc_interrupt_attach = dp_soc_interrupt_attach_li;
	arch_ops->dp_soc_attach_poll = dp_soc_attach_poll_li;
	arch_ops->dp_soc_interrupt_detach = dp_soc_interrupt_detach_li;
	arch_ops->dp_service_srngs = dp_service_srngs_li;
}

#ifdef QCA_DP_TX_HW_SW_NBUF_DESC_PREFETCH
void dp_tx_comp_get_prefetched_params_from_hal_desc(
					struct dp_soc *soc,
					void *tx_comp_hal_desc,
					struct dp_tx_desc_s **r_tx_desc)
{
	uint8_t pool_id;
	uint32_t tx_desc_id;

	tx_desc_id = hal_tx_comp_get_desc_id(tx_comp_hal_desc);
	pool_id = (tx_desc_id & DP_TX_DESC_ID_POOL_MASK) >>
		DP_TX_DESC_ID_POOL_OS;

	/* Find Tx descriptor */
	*r_tx_desc = dp_tx_desc_find(soc, pool_id,
			(tx_desc_id & DP_TX_DESC_ID_PAGE_MASK) >>
			DP_TX_DESC_ID_PAGE_OS,
			(tx_desc_id & DP_TX_DESC_ID_OFFSET_MASK) >>
			DP_TX_DESC_ID_OFFSET_OS,
			(tx_desc_id & DP_TX_DESC_ID_SPCL_MASK));
	qdf_prefetch((uint8_t *)*r_tx_desc);
}
#endif
