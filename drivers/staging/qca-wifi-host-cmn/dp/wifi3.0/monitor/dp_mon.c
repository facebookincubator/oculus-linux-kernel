/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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
#include <dp_htt.h>
#include <dp_mon.h>
#include <dp_rx_mon.h>
#include <dp_internal.h>
#include "htt_ppdu_stats.h"
#include "dp_cal_client_api.h"
#if defined(DP_CON_MON)
#ifndef REMOVE_PKT_LOG
#include <pktlog_ac_api.h>
#include <pktlog_ac.h>
#endif
#endif
#ifdef FEATURE_PERPKT_INFO
#include "dp_ratetable.h"
#endif
#ifdef QCA_SUPPORT_LITE_MONITOR
#include "dp_lite_mon.h"
#endif

#define DP_INTR_POLL_TIMER_MS	5
#define INVALID_FREE_BUFF 0xffffffff

#ifdef WLAN_RX_PKT_CAPTURE_ENH
#include "dp_rx_mon_feature.h"
#endif /* WLAN_RX_PKT_CAPTURE_ENH */

#ifdef QCA_UNDECODED_METADATA_SUPPORT
#define MAX_STRING_LEN_PER_FIELD 6
#define DP_UNDECODED_ERR_LENGTH (MAX_STRING_LEN_PER_FIELD * CDP_PHYRX_ERR_MAX)
#endif

#ifdef QCA_MCOPY_SUPPORT
static inline void
dp_pdev_disable_mcopy_code(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	mon_pdev->mcopy_mode = M_COPY_DISABLED;
	mon_pdev->mvdev = NULL;
}

static inline void
dp_reset_mcopy_mode(struct dp_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct cdp_mon_ops *cdp_ops;

	if (mon_pdev->mcopy_mode) {
		cdp_ops = dp_mon_cdp_ops_get(pdev->soc);
		if (cdp_ops  && cdp_ops->config_full_mon_mode)
			cdp_ops->soc_config_full_mon_mode((struct cdp_pdev *)pdev,
							  DP_FULL_MON_ENABLE);
		dp_pdev_disable_mcopy_code(pdev);
		dp_mon_filter_reset_mcopy_mode(pdev);
		status = dp_mon_filter_update(pdev);
		if (status != QDF_STATUS_SUCCESS) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  FL("Failed to reset AM copy mode filters"));
		}
		mon_pdev->monitor_configured = false;
	}
}

static QDF_STATUS
dp_config_mcopy_mode(struct dp_pdev *pdev, int val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_ops *mon_ops;
	struct cdp_mon_ops *cdp_ops;

	if (mon_pdev->mvdev)
		return QDF_STATUS_E_RESOURCES;

	mon_pdev->mcopy_mode = val;
	mon_pdev->tx_sniffer_enable = 0;
	mon_pdev->monitor_configured = true;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!wlan_cfg_is_delay_mon_replenish(pdev->soc->wlan_cfg_ctx)) {
		if (mon_ops && mon_ops->mon_vdev_set_monitor_mode_rings)
			mon_ops->mon_vdev_set_monitor_mode_rings(pdev, true);
	}

	/*
	 * Setup the M copy mode filter.
	 */
	cdp_ops = dp_mon_cdp_ops_get(pdev->soc);
	if (cdp_ops  && cdp_ops->config_full_mon_mode)
		cdp_ops->soc_config_full_mon_mode((struct cdp_pdev *)pdev,
						  DP_FULL_MON_ENABLE);
	dp_mon_filter_setup_mcopy_mode(pdev);
	status = dp_mon_filter_update(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  FL("Failed to set M_copy mode filters"));
		dp_mon_filter_reset_mcopy_mode(pdev);
		dp_pdev_disable_mcopy_code(pdev);
		return status;
	}

	if (!mon_pdev->pktlog_ppdu_stats)
		dp_h2t_cfg_stats_msg_send(pdev,
					  DP_PPDU_STATS_CFG_SNIFFER,
					  pdev->pdev_id);

	return status;
}
#else
static inline void
dp_reset_mcopy_mode(struct dp_pdev *pdev)
{
}

static inline QDF_STATUS
dp_config_mcopy_mode(struct dp_pdev *pdev, int val)
{
	return QDF_STATUS_E_INVAL;
}
#endif /* QCA_MCOPY_SUPPORT */

#ifdef QCA_UNDECODED_METADATA_SUPPORT
static QDF_STATUS
dp_reset_undecoded_metadata_capture(struct dp_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (mon_pdev->undecoded_metadata_capture) {
		dp_mon_filter_reset_undecoded_metadata_mode(pdev);
		status = dp_mon_filter_update(pdev);
		if (status != QDF_STATUS_SUCCESS) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  FL("Undecoded capture filter reset failed"));
		}
	}
	mon_pdev->undecoded_metadata_capture = 0;
	return status;
}

static QDF_STATUS
dp_enable_undecoded_metadata_capture(struct dp_pdev *pdev, int val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!mon_pdev->mvdev) {
		qdf_err("monitor_pdev is NULL");
		return QDF_STATUS_E_RESOURCES;
	}

	mon_pdev->undecoded_metadata_capture = val;
	mon_pdev->monitor_configured = true;


	/* Setup the undecoded metadata capture mode filter. */
	dp_mon_filter_setup_undecoded_metadata_mode(pdev);
	status = dp_mon_filter_update(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  FL("Failed to set Undecoded capture filters"));
		dp_mon_filter_reset_undecoded_metadata_mode(pdev);
		return status;
	}

	return status;
}
#else
static inline QDF_STATUS
dp_reset_undecoded_metadata_capture(struct dp_pdev *pdev)
{
	return QDF_STATUS_E_INVAL;
}

static inline QDF_STATUS
dp_enable_undecoded_metadata_capture(struct dp_pdev *pdev, int val)
{
	return QDF_STATUS_E_INVAL;
}
#endif /* QCA_UNDECODED_METADATA_SUPPORT */

QDF_STATUS dp_reset_monitor_mode(struct cdp_soc_t *soc_hdl,
				 uint8_t pdev_id,
				 uint8_t special_monitor)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev;
	struct cdp_mon_ops *cdp_ops;

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;

	qdf_spin_lock_bh(&mon_pdev->mon_lock);

	cdp_ops = dp_mon_cdp_ops_get(soc);
	if (cdp_ops  && cdp_ops->soc_config_full_mon_mode)
		cdp_ops->soc_config_full_mon_mode((struct cdp_pdev *)pdev,
						  DP_FULL_MON_DISABLE);
	mon_pdev->mvdev = NULL;

	/*
	 * Lite monitor mode, smart monitor mode and monitor
	 * mode uses this APIs to filter reset and mode disable
	 */
	if (mon_pdev->mcopy_mode) {
#if defined(QCA_MCOPY_SUPPORT)
		dp_pdev_disable_mcopy_code(pdev);
		dp_mon_filter_reset_mcopy_mode(pdev);
#endif /* QCA_MCOPY_SUPPORT */
	} else if (special_monitor) {
#if defined(ATH_SUPPORT_NAC)
		dp_mon_filter_reset_smart_monitor(pdev);
#endif /* ATH_SUPPORT_NAC */
		/* for mon 2.0 we make use of lite mon to
		 * set filters for smart monitor use case.
		 */
		dp_monitor_lite_mon_disable_rx(pdev);
	} else if (mon_pdev->undecoded_metadata_capture) {
#ifdef QCA_UNDECODED_METADATA_SUPPORT
		dp_reset_undecoded_metadata_capture(pdev);
#endif
	} else {
		dp_mon_filter_reset_mon_mode(pdev);
	}
	status = dp_mon_filter_update(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		dp_rx_mon_dest_err("%pK: Failed to reset monitor filters",
				   soc);
	}

	mon_pdev->monitor_configured = false;

	qdf_spin_unlock_bh(&mon_pdev->mon_lock);
	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_ADVANCE_MON_FILTER_SUPPORT
QDF_STATUS
dp_pdev_set_advance_monitor_filter(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				   struct cdp_monitor_filter *filter_val)
{
	/* Many monitor VAPs can exists in a system but only one can be up at
	 * anytime
	 */
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_vdev *vdev;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev || !pdev->monitor_pdev)
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;
	vdev = mon_pdev->mvdev;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_WARN,
		  "pdev=%pK, pdev_id=%d, soc=%pK vdev=%pK",
		  pdev, pdev_id, soc, vdev);

	/*Check if current pdev's monitor_vdev exists */
	if (!mon_pdev->mvdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "vdev=%pK", vdev);
		qdf_assert(vdev);
	}

	/* update filter mode, type in pdev structure */
	mon_pdev->mon_filter_mode = filter_val->mode;
	mon_pdev->fp_mgmt_filter = filter_val->fp_mgmt;
	mon_pdev->fp_ctrl_filter = filter_val->fp_ctrl;
	mon_pdev->fp_data_filter = filter_val->fp_data;
	mon_pdev->mo_mgmt_filter = filter_val->mo_mgmt;
	mon_pdev->mo_ctrl_filter = filter_val->mo_ctrl;
	mon_pdev->mo_data_filter = filter_val->mo_data;

	dp_mon_filter_setup_mon_mode(pdev);
	status = dp_mon_filter_update(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		dp_rx_mon_dest_err("%pK: Failed to set filter for adv mon mode",
				   soc);
		dp_mon_filter_reset_mon_mode(pdev);
	}

	return status;
}
#endif

QDF_STATUS
dp_deliver_tx_mgmt(struct cdp_soc_t *cdp_soc, uint8_t pdev_id, qdf_nbuf_t nbuf)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)cdp_soc,
						   pdev_id);

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	dp_deliver_mgmt_frm(pdev, nbuf);

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
/**
 * dp_scan_spcl_vap_stats_attach() - alloc spcl vap stats struct
 * @mon_vdev: Datapath mon VDEV handle
 *
 * Return: 0 on success, not 0 on failure
 */
static inline QDF_STATUS
dp_scan_spcl_vap_stats_attach(struct dp_mon_vdev *mon_vdev)
{
	mon_vdev->scan_spcl_vap_stats =
		qdf_mem_malloc(sizeof(struct cdp_scan_spcl_vap_stats));

	if (!mon_vdev->scan_spcl_vap_stats) {
		dp_mon_err("scan spcl vap stats attach fail");
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_scan_spcl_vap_stats_detach() - free spcl vap stats struct
 * @mon_vdev: Datapath mon VDEV handle
 *
 * Return: void
 */
static inline void
dp_scan_spcl_vap_stats_detach(struct dp_mon_vdev *mon_vdev)
{
	if (mon_vdev->scan_spcl_vap_stats) {
		qdf_mem_free(mon_vdev->scan_spcl_vap_stats);
		mon_vdev->scan_spcl_vap_stats = NULL;
	}
}

/**
 * dp_reset_scan_spcl_vap_stats() - reset spcl vap rx stats
 * @vdev: Datapath VDEV handle
 *
 * Return: void
 */
static inline void
dp_reset_scan_spcl_vap_stats(struct dp_vdev *vdev)
{
	struct dp_mon_vdev *mon_vdev;
	struct dp_mon_pdev *mon_pdev;

	mon_pdev = vdev->pdev->monitor_pdev;
	if (!mon_pdev || !mon_pdev->reset_scan_spcl_vap_stats_enable)
		return;

	mon_vdev = vdev->monitor_vdev;
	if (!mon_vdev || !mon_vdev->scan_spcl_vap_stats)
		return;

	qdf_mem_zero(mon_vdev->scan_spcl_vap_stats,
		     sizeof(struct cdp_scan_spcl_vap_stats));
}

/**
 * dp_get_scan_spcl_vap_stats() - get spcl vap rx stats
 * @soc_hdl: Datapath soc handle
 * @vdev_id: vdev id
 * @stats: structure to hold spcl vap stats
 *
 * Return: 0 on success, not 0 on failure
 */
static QDF_STATUS
dp_get_scan_spcl_vap_stats(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			   struct cdp_scan_spcl_vap_stats *stats)
{
	struct dp_mon_vdev *mon_vdev = NULL;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev || !stats) {
		if (vdev)
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_E_INVAL;
	}

	mon_vdev = vdev->monitor_vdev;
	if (!mon_vdev || !mon_vdev->scan_spcl_vap_stats) {
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_copy(stats, mon_vdev->scan_spcl_vap_stats,
		     sizeof(struct cdp_scan_spcl_vap_stats));

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}
#else
static inline void
dp_reset_scan_spcl_vap_stats(struct dp_vdev *vdev)
{
}

static inline QDF_STATUS
dp_scan_spcl_vap_stats_attach(struct dp_mon_vdev *mon_vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_scan_spcl_vap_stats_detach(struct dp_mon_vdev *mon_vdev)
{
}
#endif

/**
 * dp_vdev_set_monitor_mode() - Set DP VDEV to monitor mode
 * @dp_soc: DP soc context
 * @vdev_id: vdev ID
 * @special_monitor: Flag to denote if its smart monitor mode
 *
 * Return: 0 on success, not 0 on failure
 */
static QDF_STATUS dp_vdev_set_monitor_mode(struct cdp_soc_t *dp_soc,
					   uint8_t vdev_id,
					   uint8_t special_monitor)
{
	struct dp_soc *soc = (struct dp_soc *)dp_soc;
	struct dp_pdev *pdev;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev;
	struct cdp_mon_ops *cdp_ops;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	pdev = vdev->pdev;

	if (!pdev || !pdev->monitor_pdev)
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;

	mon_pdev->mvdev = vdev;

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_WARN,
		  "pdev=%pK, pdev_id=%d, soc=%pK vdev=%pK\n",
		  pdev, pdev->pdev_id, pdev->soc, vdev);

	/*
	 * do not configure monitor buf ring and filter for smart and
	 * lite monitor
	 * for smart monitor filters are added along with first NAC
	 * for lite monitor required configuration done through
	 * dp_set_pdev_param
	 */

	if (special_monitor) {
		status = QDF_STATUS_SUCCESS;
		goto fail;
	}

	if (mon_pdev->scan_spcl_vap_configured)
		dp_reset_scan_spcl_vap_stats(vdev);

	/*Check if current pdev's monitor_vdev exists */
	if (mon_pdev->monitor_configured) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  "monitor vap already created vdev=%pK\n", vdev);
		status = QDF_STATUS_E_RESOURCES;
		goto fail;
	}

	mon_pdev->monitor_configured = true;

	/* If advance monitor filter is applied using lite_mon
	 * via vap configuration, required filters are already applied
	 * hence returning SUCCESS from here.
	 */
	if (dp_monitor_lite_mon_is_rx_adv_filter_enable(pdev)) {
		status = QDF_STATUS_SUCCESS;
		goto fail;
	}
	/* disable lite mon if configured, monitor vap takes
	 * priority over lite mon when its created. Lite mon
	 * can be configured later again.
	 */
	dp_monitor_lite_mon_disable_rx(pdev);

	cdp_ops = dp_mon_cdp_ops_get(soc);
	if (cdp_ops  && cdp_ops->soc_config_full_mon_mode)
		cdp_ops->soc_config_full_mon_mode((struct cdp_pdev *)pdev,
						  DP_FULL_MON_ENABLE);
	dp_mon_filter_setup_mon_mode(pdev);
	status = dp_mon_filter_update(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		dp_cdp_err("%pK: Failed to reset monitor filters", soc);
		dp_mon_filter_reset_mon_mode(pdev);
		mon_pdev->monitor_configured = false;
		mon_pdev->mvdev = NULL;
	}

fail:
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return status;
}

#ifdef QCA_TX_CAPTURE_SUPPORT
static QDF_STATUS
dp_config_tx_capture_mode(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	mon_pdev->tx_sniffer_enable = 1;
	mon_pdev->monitor_configured = false;

	if (!mon_pdev->pktlog_ppdu_stats)
		dp_h2t_cfg_stats_msg_send(pdev,
					  DP_PPDU_STATS_CFG_SNIFFER,
					  pdev->pdev_id);

	return QDF_STATUS_SUCCESS;
}
#else
#ifdef QCA_MCOPY_SUPPORT
static QDF_STATUS
dp_config_tx_capture_mode(struct dp_pdev *pdev)
{
	return QDF_STATUS_E_INVAL;
}
#endif
#endif

#if defined(QCA_MCOPY_SUPPORT) || defined(QCA_TX_CAPTURE_SUPPORT)
QDF_STATUS
dp_config_debug_sniffer(struct dp_pdev *pdev, int val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	/*
	 * Note: The mirror copy mode cannot co-exist with any other
	 * monitor modes. Hence disabling the filter for this mode will
	 * reset the monitor destination ring filters.
	 */
	dp_reset_mcopy_mode(pdev);
	switch (val) {
	case 0:
		mon_pdev->tx_sniffer_enable = 0;
		mon_pdev->monitor_configured = false;

		/*
		 * We don't need to reset the Rx monitor status ring  or call
		 * the API dp_ppdu_ring_reset() if all debug sniffer mode is
		 * disabled. The Rx monitor status ring will be disabled when
		 * the last mode using the monitor status ring get disabled.
		 */
		if (!mon_pdev->pktlog_ppdu_stats &&
		    !mon_pdev->enhanced_stats_en &&
		    !mon_pdev->bpr_enable) {
			dp_h2t_cfg_stats_msg_send(pdev, 0, pdev->pdev_id);
		} else if (mon_pdev->enhanced_stats_en &&
			   !mon_pdev->bpr_enable) {
			dp_h2t_cfg_stats_msg_send(pdev,
						  DP_PPDU_STATS_CFG_ENH_STATS,
						  pdev->pdev_id);
		} else if (!mon_pdev->enhanced_stats_en &&
			   mon_pdev->bpr_enable) {
			dp_h2t_cfg_stats_msg_send(pdev,
						  DP_PPDU_STATS_CFG_BPR_ENH,
						  pdev->pdev_id);
		} else {
			dp_h2t_cfg_stats_msg_send(pdev,
						  DP_PPDU_STATS_CFG_BPR,
						  pdev->pdev_id);
		}
		break;

	case 1:
		status = dp_config_tx_capture_mode(pdev);
		break;
	case 2:
	case 4:
		status = dp_config_mcopy_mode(pdev, val);
		break;

	default:
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "Invalid value, mode not supported");
		status = QDF_STATUS_E_INVAL;
		break;
	}
	return status;
}
#endif

#ifdef QCA_UNDECODED_METADATA_SUPPORT
QDF_STATUS
dp_mon_config_undecoded_metadata_capture(struct dp_pdev *pdev, int val)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!mon_pdev->mvdev && !mon_pdev->scan_spcl_vap_configured) {
		qdf_err("No monitor or Special vap, undecoded capture not supported");
		return QDF_STATUS_E_RESOURCES;
	}

	if (val)
		status = dp_enable_undecoded_metadata_capture(pdev, val);
	else
		status = dp_reset_undecoded_metadata_capture(pdev);

	return status;
}
#endif

/**
 * dp_monitor_mode_ring_config() - Send the tlv config to fw for monitor buffer
 *                                 ring based on target
 * @soc: soc handle
 * @mac_for_pdev: WIN- pdev_id, MCL- mac id
 * @pdev: physical device handle
 * @ring_num: mac id
 * @htt_tlv_filter: tlv filter
 *
 * Return: zero on success, non-zero on failure
 */
static inline QDF_STATUS
dp_monitor_mode_ring_config(struct dp_soc *soc, uint8_t mac_for_pdev,
			    struct dp_pdev *pdev, uint8_t ring_num,
			    struct htt_rx_ring_tlv_filter htt_tlv_filter)
{
	QDF_STATUS status;

	if (soc->wlan_cfg_ctx->rxdma1_enable)
		status = htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					     soc->rxdma_mon_buf_ring[ring_num]
					     .hal_srng,
					     RXDMA_MONITOR_BUF,
					     RX_MONITOR_BUFFER_SIZE,
					     &htt_tlv_filter);
	else
		status = htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					     pdev->rx_mac_buf_ring[ring_num]
					     .hal_srng,
					     RXDMA_BUF, RX_DATA_BUFFER_SIZE,
					     &htt_tlv_filter);

	return status;
}

/**
 * dp_get_mon_vdev_from_pdev_wifi3() - Get vdev id of monitor mode
 * @soc_hdl: datapath soc handle
 * @pdev_id: physical device instance id
 *
 * Return: virtual interface id
 */
static uint8_t dp_get_mon_vdev_from_pdev_wifi3(struct cdp_soc_t *soc_hdl,
		uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (qdf_unlikely(!pdev || !pdev->monitor_pdev ||
				!pdev->monitor_pdev->mvdev))
		return -EINVAL;

	return pdev->monitor_pdev->mvdev->vdev_id;
}

#if defined(QCA_TX_CAPTURE_SUPPORT) || defined(QCA_ENHANCED_STATS_SUPPORT)
#ifndef WLAN_TX_PKT_CAPTURE_ENH
void dp_deliver_mgmt_frm(struct dp_pdev *pdev, qdf_nbuf_t nbuf)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (mon_pdev->tx_sniffer_enable || mon_pdev->mcopy_mode) {
		dp_wdi_event_handler(WDI_EVENT_TX_MGMT_CTRL, pdev->soc,
				     nbuf, HTT_INVALID_PEER,
				     WDI_NO_VAL, pdev->pdev_id);
	} else {
		if (!mon_pdev->bpr_enable)
			qdf_nbuf_free(nbuf);
	}
}
#endif
#endif

QDF_STATUS dp_htt_ppdu_stats_attach(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	mon_pdev->ppdu_tlv_buf = qdf_mem_malloc(HTT_T2H_MAX_MSG_SIZE);

	if (!mon_pdev->ppdu_tlv_buf) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_DP, "ppdu_tlv_buf alloc fail");
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

void dp_htt_ppdu_stats_detach(struct dp_pdev *pdev)
{
	struct ppdu_info *ppdu_info, *ppdu_info_next;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;


	TAILQ_FOREACH_SAFE(ppdu_info, &mon_pdev->ppdu_info_list,
			   ppdu_info_list_elem, ppdu_info_next) {
		if (!ppdu_info)
			break;
		TAILQ_REMOVE(&mon_pdev->ppdu_info_list,
			     ppdu_info, ppdu_info_list_elem);
		mon_pdev->list_depth--;
		qdf_assert_always(ppdu_info->nbuf);
		qdf_nbuf_free(ppdu_info->nbuf);
		qdf_mem_free(ppdu_info);
	}

	TAILQ_FOREACH_SAFE(ppdu_info, &mon_pdev->sched_comp_ppdu_list,
			   ppdu_info_list_elem, ppdu_info_next) {
		if (!ppdu_info)
			break;
		TAILQ_REMOVE(&mon_pdev->sched_comp_ppdu_list,
			     ppdu_info, ppdu_info_list_elem);
		mon_pdev->sched_comp_list_depth--;
		qdf_assert_always(ppdu_info->nbuf);
		qdf_nbuf_free(ppdu_info->nbuf);
		qdf_mem_free(ppdu_info);
	}

	if (mon_pdev->ppdu_tlv_buf)
		qdf_mem_free(mon_pdev->ppdu_tlv_buf);
}

QDF_STATUS dp_pdev_get_rx_mon_stats(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				    struct cdp_pdev_mon_stats *stats)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_mon_pdev *mon_pdev;

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(stats, &mon_pdev->rx_mon_stats,
		     sizeof(struct cdp_pdev_mon_stats));

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_UNDECODED_METADATA_SUPPORT
/**
 * dp_pdev_get_undecoded_capture_stats() - Get undecoded metadata captured
 * monitor pdev stats
 * @mon_pdev: Monitor PDEV handle
 * @rx_mon_stats: Monitor pdev status/destination ring stats
 *
 * Return: None
 */
static inline void
dp_pdev_get_undecoded_capture_stats(struct dp_mon_pdev *mon_pdev,
				    struct cdp_pdev_mon_stats *rx_mon_stats)
{
	char undecoded_error[DP_UNDECODED_ERR_LENGTH];
	uint8_t index = 0, i;

	DP_PRINT_STATS("Rx Undecoded Frame count:%d",
		       rx_mon_stats->rx_undecoded_count);
	index = 0;
	for (i = 0; i < (CDP_PHYRX_ERR_MAX); i++) {
		index += qdf_snprint(&undecoded_error[index],
				DP_UNDECODED_ERR_LENGTH - index,
				" %d", rx_mon_stats->rx_undecoded_error[i]);
	}
	DP_PRINT_STATS("Undecoded Error (0-63):%s", undecoded_error);
}
#else
static inline void
dp_pdev_get_undecoded_capture_stats(struct dp_mon_pdev *mon_pdev,
				    struct cdp_pdev_mon_stats *rx_mon_stats)
{
}
#endif

static const char *
dp_preamble_type_str[] = {
	"preamble OFDMA     ",
	"preamble CCK       ",
	"preamble HT        ",
	"preamble VHT       ",
	"preamble HE        ",
	"preamble EHT       ",
	"preamble NO SUPPORT",
};

static const char *
dp_reception_type_str[] = {
	"reception su        ",
	"reception mu_mimo   ",
	"reception ofdma     ",
	"reception ofdma mimo",
};

static const char *
dp_mu_dl_ul_str[] = {
	"MU DL",
	"MU UL",
};

static inline void
dp_print_pdev_mpdu_fcs_ok_cnt(struct cdp_pdev_mon_stats *rx_mon_sts,
			      uint32_t pkt_t, uint32_t rx_t,
			      uint32_t dl_ul, uint32_t user)
{
	DP_PRINT_STATS("%s, %s, %s, user=%d, mpdu_fcs_ok=%d",
		       dp_preamble_type_str[pkt_t],
		       dp_reception_type_str[rx_t],
		       dp_mu_dl_ul_str[dl_ul],
		       user,
		       rx_mon_sts->mpdu_cnt_fcs_ok[pkt_t][rx_t][dl_ul][user]);
}

static inline void
dp_print_pdev_mpdu_fcs_err_cnt(struct cdp_pdev_mon_stats *rx_mon_sts,
			       uint32_t pkt_t, uint32_t rx_t,
			       uint32_t dl_ul, uint32_t user)
{
	DP_PRINT_STATS("%s, %s, %s, user=%d, mpdu_fcs_err=%d",
		       dp_preamble_type_str[pkt_t],
		       dp_reception_type_str[rx_t],
		       dp_mu_dl_ul_str[dl_ul],
		       user,
		       rx_mon_sts->mpdu_cnt_fcs_err[pkt_t][rx_t][dl_ul][user]);
}

static inline void
dp_print_pdev_mpdu_cnt(struct cdp_pdev_mon_stats *rx_mon_sts,
		       uint32_t pkt_t, uint32_t rx_t,
		       uint32_t dl_ul, uint32_t user)
{
	if (rx_mon_sts->mpdu_cnt_fcs_ok[pkt_t][rx_t][dl_ul][user])
		dp_print_pdev_mpdu_fcs_ok_cnt(rx_mon_sts, pkt_t, rx_t,
					      dl_ul, user);

	if (rx_mon_sts->mpdu_cnt_fcs_err[pkt_t][rx_t][dl_ul][user])
		dp_print_pdev_mpdu_fcs_err_cnt(rx_mon_sts, pkt_t, rx_t,
					       dl_ul, user);
}

static inline void
dp_print_pdev_mpdu_user(struct cdp_pdev_mon_stats *rx_mon_sts,
			uint32_t pkt_t, uint32_t rx_t,
			uint32_t dl_ul)
{
	uint32_t user;

	for (user = 0; user < CDP_MU_SNIF_USER_MAX; user++)
		dp_print_pdev_mpdu_cnt(rx_mon_sts, pkt_t, rx_t,
				       dl_ul, user);
}

static inline void
dp_print_pdev_mpdu_dl_ul(struct cdp_pdev_mon_stats *rx_mon_sts,
			 uint32_t pkt_t, uint32_t rx_t)
{
	uint32_t dl_ul;

	for (dl_ul = CDP_MU_TYPE_DL; dl_ul < CDP_MU_TYPE_MAX; dl_ul++)
		dp_print_pdev_mpdu_user(rx_mon_sts, pkt_t, rx_t,
					dl_ul);
}

static inline void
dp_print_pdev_mpdu_rx_type(struct cdp_pdev_mon_stats *rx_mon_sts,
			   uint32_t pkt_t)
{
	uint32_t rx_t;

	for (rx_t = CDP_RX_TYPE_SU; rx_t < CDP_RX_TYPE_MAX; rx_t++)
		dp_print_pdev_mpdu_dl_ul(rx_mon_sts, pkt_t, rx_t);
}

static inline void
dp_print_pdev_mpdu_pkt_type(struct cdp_pdev_mon_stats *rx_mon_sts)
{
	uint32_t pkt_t;

	for (pkt_t = CDP_PKT_TYPE_OFDM; pkt_t < CDP_PKT_TYPE_MAX; pkt_t++)
		dp_print_pdev_mpdu_rx_type(rx_mon_sts, pkt_t);
}

static inline void
print_ppdu_eht_type_mode(
	struct cdp_pdev_mon_stats *rx_mon_stats,
	uint32_t ppdu_type_mode,
	uint32_t dl_ul)
{
	DP_PRINT_STATS("type_mode=%d, dl_ul=%d, cnt=%d",
		       ppdu_type_mode,
		       dl_ul,
		       rx_mon_stats->ppdu_eht_type_mode[ppdu_type_mode][dl_ul]);
}

static inline void
print_ppdu_eth_type_mode_dl_ul(
	struct cdp_pdev_mon_stats *rx_mon_stats,
	uint32_t ppdu_type_mode
)
{
	uint32_t dl_ul;

	for (dl_ul = 0; dl_ul < CDP_MU_TYPE_MAX; dl_ul++) {
		if (rx_mon_stats->ppdu_eht_type_mode[ppdu_type_mode][dl_ul])
			print_ppdu_eht_type_mode(rx_mon_stats,
						 ppdu_type_mode, dl_ul);
	}
}

static inline void
dp_print_pdev_eht_ppdu_cnt(struct dp_pdev *pdev)
{
	struct cdp_pdev_mon_stats *rx_mon_stats;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	uint32_t ppdu_type_mode;

	rx_mon_stats = &mon_pdev->rx_mon_stats;
	DP_PRINT_STATS("Monitor EHT PPDU  Count");
	for (ppdu_type_mode = 0; ppdu_type_mode < CDP_EHT_TYPE_MODE_MAX;
	     ppdu_type_mode++) {
		print_ppdu_eth_type_mode_dl_ul(rx_mon_stats,
					       ppdu_type_mode);
	}
}

static inline void
dp_print_pdev_mpdu_stats(struct dp_pdev *pdev)
{
	struct cdp_pdev_mon_stats *rx_mon_stats;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	rx_mon_stats = &mon_pdev->rx_mon_stats;
	DP_PRINT_STATS("Monitor MPDU Count");
	dp_print_pdev_mpdu_pkt_type(rx_mon_stats);
}

void
dp_print_pdev_rx_mon_stats(struct dp_pdev *pdev)
{
	struct cdp_pdev_mon_stats *rx_mon_stats;
	uint32_t *stat_ring_ppdu_ids;
	uint32_t *dest_ring_ppdu_ids;
	int i, idx;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	rx_mon_stats = &mon_pdev->rx_mon_stats;

	DP_PRINT_STATS("PDEV Rx Monitor Stats:\n");

	DP_PRINT_STATS("status_ppdu_compl_cnt = %d",
		       rx_mon_stats->status_ppdu_compl);
	DP_PRINT_STATS("status_ppdu_start_cnt = %d",
		       rx_mon_stats->status_ppdu_start);
	DP_PRINT_STATS("status_ppdu_end_cnt = %d",
		       rx_mon_stats->status_ppdu_end);
	DP_PRINT_STATS("status_ppdu_start_mis_cnt = %d",
		       rx_mon_stats->status_ppdu_start_mis);
	DP_PRINT_STATS("status_ppdu_end_mis_cnt = %d",
		       rx_mon_stats->status_ppdu_end_mis);

	DP_PRINT_STATS("start_user_info_cnt = %d",
		       rx_mon_stats->start_user_info_cnt);
	DP_PRINT_STATS("end_user_stats_cnt = %d",
		       rx_mon_stats->end_user_stats_cnt);

	DP_PRINT_STATS("status_ppdu_done_cnt = %d",
		       rx_mon_stats->status_ppdu_done);
	DP_PRINT_STATS("dest_ppdu_done_cnt = %d",
		       rx_mon_stats->dest_ppdu_done);
	DP_PRINT_STATS("dest_mpdu_done_cnt = %d",
		       rx_mon_stats->dest_mpdu_done);
	DP_PRINT_STATS("tlv_tag_status_err_cnt = %u",
		       rx_mon_stats->tlv_tag_status_err);
	DP_PRINT_STATS("mon status DMA not done WAR count= %u",
		       rx_mon_stats->status_buf_done_war);
	DP_PRINT_STATS("dest_mpdu_drop_cnt = %d",
		       rx_mon_stats->dest_mpdu_drop);
	DP_PRINT_STATS("dup_mon_linkdesc_cnt = %d",
		       rx_mon_stats->dup_mon_linkdesc_cnt);
	DP_PRINT_STATS("dup_mon_buf_cnt = %d",
		       rx_mon_stats->dup_mon_buf_cnt);
	DP_PRINT_STATS("mon_rx_buf_reaped = %u",
		       rx_mon_stats->mon_rx_bufs_reaped_dest);
	DP_PRINT_STATS("mon_rx_buf_replenished = %u",
		       rx_mon_stats->mon_rx_bufs_replenished_dest);
	DP_PRINT_STATS("ppdu_id_mismatch = %u",
		       rx_mon_stats->ppdu_id_mismatch);
	DP_PRINT_STATS("mpdu_ppdu_id_match_cnt = %d",
		       rx_mon_stats->ppdu_id_match);
	DP_PRINT_STATS("ppdus dropped frm status ring = %d",
		       rx_mon_stats->status_ppdu_drop);
	DP_PRINT_STATS("ppdus dropped frm dest ring = %d",
		       rx_mon_stats->dest_ppdu_drop);
	DP_PRINT_STATS("mpdu_ppdu_id_mismatch_drop = %u",
		       rx_mon_stats->mpdu_ppdu_id_mismatch_drop);
	DP_PRINT_STATS("mpdu_decap_type_invalid = %u",
		       rx_mon_stats->mpdu_decap_type_invalid);
	stat_ring_ppdu_ids =
		(uint32_t *)qdf_mem_malloc(sizeof(uint32_t) * MAX_PPDU_ID_HIST);
	dest_ring_ppdu_ids =
		(uint32_t *)qdf_mem_malloc(sizeof(uint32_t) * MAX_PPDU_ID_HIST);

	if (!stat_ring_ppdu_ids || !dest_ring_ppdu_ids)
		DP_PRINT_STATS("Unable to allocate ppdu id hist mem\n");

	qdf_spin_lock_bh(&mon_pdev->mon_lock);
	idx = rx_mon_stats->ppdu_id_hist_idx;
	qdf_mem_copy(stat_ring_ppdu_ids,
		     rx_mon_stats->stat_ring_ppdu_id_hist,
		     sizeof(uint32_t) * MAX_PPDU_ID_HIST);
	qdf_mem_copy(dest_ring_ppdu_ids,
		     rx_mon_stats->dest_ring_ppdu_id_hist,
		     sizeof(uint32_t) * MAX_PPDU_ID_HIST);
	qdf_spin_unlock_bh(&mon_pdev->mon_lock);

	DP_PRINT_STATS("PPDU Id history:");
	DP_PRINT_STATS("stat_ring_ppdu_ids\t dest_ring_ppdu_ids");
	for (i = 0; i < MAX_PPDU_ID_HIST; i++) {
		idx = (idx + 1) & (MAX_PPDU_ID_HIST - 1);
		DP_PRINT_STATS("%*u\t%*u", 16,
			       rx_mon_stats->stat_ring_ppdu_id_hist[idx], 16,
			       rx_mon_stats->dest_ring_ppdu_id_hist[idx]);
	}
	qdf_mem_free(stat_ring_ppdu_ids);
	qdf_mem_free(dest_ring_ppdu_ids);
	DP_PRINT_STATS("mon_rx_dest_stuck = %d",
		       rx_mon_stats->mon_rx_dest_stuck);

	dp_pdev_get_undecoded_capture_stats(mon_pdev, rx_mon_stats);
	dp_mon_rx_print_advanced_stats(pdev->soc, pdev);

	dp_print_pdev_mpdu_stats(pdev);
	dp_print_pdev_eht_ppdu_cnt(pdev);

}

#ifdef QCA_SUPPORT_BPR
QDF_STATUS
dp_set_bpr_enable(struct dp_pdev *pdev, int val)
{
	struct dp_mon_ops *mon_ops;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_set_bpr_enable)
		return mon_ops->mon_set_bpr_enable(pdev, val);

	return QDF_STATUS_E_FAILURE;
}
#endif

#ifdef WDI_EVENT_ENABLE
#ifdef BE_PKTLOG_SUPPORT
static bool
dp_set_hybrid_pktlog_enable(struct dp_pdev *pdev,
			    struct dp_mon_pdev *mon_pdev,
			    struct dp_soc *soc)
{
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx;
	struct dp_mon_ops *mon_ops = NULL;
	uint16_t num_buffers;

	if (mon_pdev->mvdev) {
		/* Nothing needs to be done if monitor mode is
		 * enabled
		 */
		mon_pdev->pktlog_hybrid_mode = true;
		return false;
	}

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!mon_ops) {
		dp_mon_filter_err("Mon ops uninitialized");
		return QDF_STATUS_E_FAILURE;
	}

	if (!mon_pdev->pktlog_hybrid_mode) {
		mon_pdev->pktlog_hybrid_mode = true;
		soc_cfg_ctx = soc->wlan_cfg_ctx;
		num_buffers =
			wlan_cfg_get_dp_soc_tx_mon_buf_ring_size(soc_cfg_ctx);

		if (mon_ops && mon_ops->set_mon_mode_buf_rings_tx)
			mon_ops->set_mon_mode_buf_rings_tx(pdev, num_buffers);

		dp_mon_filter_setup_pktlog_hybrid(pdev);
		if (dp_tx_mon_filter_update(pdev) !=
		    QDF_STATUS_SUCCESS) {
			dp_cdp_err("Set hybrid filters failed");
			dp_mon_filter_reset_pktlog_hybrid(pdev);
			mon_pdev->rx_pktlog_mode =
				DP_RX_PKTLOG_DISABLED;
			return false;
		}

		dp_monitor_reap_timer_start(soc, CDP_MON_REAP_SOURCE_PKTLOG);
	}

	return true;
}

static void
dp_set_hybrid_pktlog_disable(struct dp_mon_pdev *mon_pdev)
{
	mon_pdev->pktlog_hybrid_mode = false;
}
#else
static void
dp_set_hybrid_pktlog_disable(struct dp_mon_pdev *mon_pdev)
{
}

static bool
dp_set_hybrid_pktlog_enable(struct dp_pdev *pdev,
			    struct dp_mon_pdev *mon_pdev,
			    struct dp_soc *soc)
{
	dp_cdp_err("Hybrid mode is supported only on beryllium");
	return true;
}
#endif
int dp_set_pktlog_wifi3(struct dp_pdev *pdev, uint32_t event,
		        bool enable)
{
	struct dp_soc *soc = NULL;
	int max_mac_rings = wlan_cfg_get_num_mac_rings
					(pdev->wlan_cfg_ctx);
	uint8_t mac_id = 0;
	struct dp_mon_ops *mon_ops;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	soc = pdev->soc;
	mon_ops = dp_mon_ops_get(soc);

	if (!mon_ops)
		return 0;

	dp_update_num_mac_rings_for_dbs(soc, &max_mac_rings);

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  FL("Max_mac_rings %d "),
		  max_mac_rings);

	if (enable) {
		switch (event) {
		case WDI_EVENT_RX_DESC:
			/* Nothing needs to be done if monitor mode is
			 * enabled
			 */
			if (mon_pdev->mvdev)
				return 0;

			if (mon_pdev->rx_pktlog_mode == DP_RX_PKTLOG_FULL)
				break;

			mon_pdev->rx_pktlog_mode = DP_RX_PKTLOG_FULL;
			dp_mon_filter_setup_rx_pkt_log_full(pdev);
			if (dp_mon_filter_update(pdev) != QDF_STATUS_SUCCESS) {
				dp_cdp_err("%pK: Pktlog full filters set failed",
					   soc);
				dp_mon_filter_reset_rx_pkt_log_full(pdev);
				mon_pdev->rx_pktlog_mode =
					DP_RX_PKTLOG_DISABLED;
				return 0;
			}

			dp_monitor_reap_timer_start(soc,
						    CDP_MON_REAP_SOURCE_PKTLOG);
			break;

		case WDI_EVENT_LITE_RX:
			/* Nothing needs to be done if monitor mode is
			 * enabled
			 */
			if (mon_pdev->mvdev)
				return 0;

			if (mon_pdev->rx_pktlog_mode == DP_RX_PKTLOG_LITE)
				break;

			mon_pdev->rx_pktlog_mode = DP_RX_PKTLOG_LITE;

			/*
			 * Set the packet log lite mode filter.
			 */
			dp_mon_filter_setup_rx_pkt_log_lite(pdev);
			if (dp_mon_filter_update(pdev) != QDF_STATUS_SUCCESS) {
				dp_cdp_err("%pK: Pktlog lite filters set failed",
					   soc);
				dp_mon_filter_reset_rx_pkt_log_lite(pdev);
				mon_pdev->rx_pktlog_mode =
					DP_RX_PKTLOG_DISABLED;
				return 0;
			}

			dp_monitor_reap_timer_start(soc,
						    CDP_MON_REAP_SOURCE_PKTLOG);
			break;
		case WDI_EVENT_LITE_T2H:
			for (mac_id = 0; mac_id < max_mac_rings; mac_id++) {
				int mac_for_pdev = dp_get_mac_id_for_pdev(
							mac_id,	pdev->pdev_id);

				mon_pdev->pktlog_ppdu_stats = true;
				dp_h2t_cfg_stats_msg_send(pdev,
					DP_PPDU_TXLITE_STATS_BITMASK_CFG,
					mac_for_pdev);
			}
			break;

		case WDI_EVENT_RX_CBF:
			/* Nothing needs to be done if monitor mode is
			 * enabled
			 */
			if (mon_pdev->mvdev)
				return 0;

			if (mon_pdev->rx_pktlog_cbf)
				break;

			mon_pdev->rx_pktlog_cbf = true;
			mon_pdev->monitor_configured = true;
			if (mon_ops->mon_vdev_set_monitor_mode_buf_rings)
				mon_ops->mon_vdev_set_monitor_mode_buf_rings(
					pdev);

			/*
			 * Set the packet log lite mode filter.
			 */
			qdf_info("Non mon mode: Enable destination ring");

			dp_mon_filter_setup_rx_pkt_log_cbf(pdev);
			if (dp_mon_filter_update(pdev) != QDF_STATUS_SUCCESS) {
				dp_mon_err("Pktlog set CBF filters failed");
				dp_mon_filter_reset_rx_pktlog_cbf(pdev);
				mon_pdev->rx_pktlog_mode =
					DP_RX_PKTLOG_DISABLED;
				mon_pdev->monitor_configured = false;
				return 0;
			}

			dp_monitor_reap_timer_start(soc,
						    CDP_MON_REAP_SOURCE_PKTLOG);
			break;
		case WDI_EVENT_HYBRID_TX:
			if (!dp_set_hybrid_pktlog_enable(pdev, mon_pdev, soc))
				return 0;
			break;

		default:
			/* Nothing needs to be done for other pktlog types */
			break;
		}
	} else {
		switch (event) {
		case WDI_EVENT_RX_DESC:
		case WDI_EVENT_LITE_RX:
			/* Nothing needs to be done if monitor mode is
			 * enabled
			 */
			if (mon_pdev->mvdev)
				return 0;

			if (mon_pdev->rx_pktlog_mode == DP_RX_PKTLOG_DISABLED)
				break;

			mon_pdev->rx_pktlog_mode = DP_RX_PKTLOG_DISABLED;
			dp_mon_filter_reset_rx_pkt_log_full(pdev);
			if (dp_mon_filter_update(pdev) != QDF_STATUS_SUCCESS) {
				dp_cdp_err("%pK: Pktlog filters reset failed",
					   soc);
				return 0;
			}

			dp_mon_filter_reset_rx_pkt_log_lite(pdev);
			if (dp_mon_filter_update(pdev) != QDF_STATUS_SUCCESS) {
				dp_cdp_err("%pK: Pktlog filters reset failed",
					   soc);
				return 0;
			}

			dp_monitor_reap_timer_stop(soc,
						   CDP_MON_REAP_SOURCE_PKTLOG);
			break;
		case WDI_EVENT_LITE_T2H:
			/*
			 * To disable HTT_H2T_MSG_TYPE_PPDU_STATS_CFG in FW
			 * passing value 0. Once these macros will define in htt
			 * header file will use proper macros
			 */
			for (mac_id = 0; mac_id < max_mac_rings; mac_id++) {
				int mac_for_pdev =
						dp_get_mac_id_for_pdev(mac_id,
								pdev->pdev_id);

				mon_pdev->pktlog_ppdu_stats = false;
				if (!mon_pdev->enhanced_stats_en &&
				    !mon_pdev->tx_sniffer_enable &&
				    !mon_pdev->mcopy_mode) {
					dp_h2t_cfg_stats_msg_send(pdev, 0,
								  mac_for_pdev);
				} else if (mon_pdev->tx_sniffer_enable ||
					   mon_pdev->mcopy_mode) {
					dp_h2t_cfg_stats_msg_send(pdev,
						DP_PPDU_STATS_CFG_SNIFFER,
						mac_for_pdev);
				} else if (mon_pdev->enhanced_stats_en) {
					dp_h2t_cfg_stats_msg_send(pdev,
						DP_PPDU_STATS_CFG_ENH_STATS,
						mac_for_pdev);
				}
			}

			break;
		case WDI_EVENT_RX_CBF:
			mon_pdev->rx_pktlog_cbf = false;
			break;

		case WDI_EVENT_HYBRID_TX:
			dp_set_hybrid_pktlog_disable(mon_pdev);
			break;

		default:
			/* Nothing needs to be done for other pktlog types */
			break;
		}
	}
	return 0;
}
#endif

/* MCL specific functions */
#if defined(DP_CON_MON) && !defined(REMOVE_PKT_LOG)
void dp_pktlogmod_exit(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct hif_opaque_softc *scn = soc->hif_handle;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (!scn) {
		dp_mon_err("Invalid hif(scn) handle");
		return;
	}

	dp_monitor_reap_timer_stop(soc, CDP_MON_REAP_SOURCE_PKTLOG);
	pktlogmod_exit(scn);
	mon_pdev->pkt_log_init = false;
}
#endif /*DP_CON_MON*/

#if defined(WDI_EVENT_ENABLE) && defined(QCA_ENHANCED_STATS_SUPPORT)
#ifdef IPA_OFFLOAD
void dp_peer_get_tx_rx_stats(struct dp_peer *peer,
			     struct cdp_interface_peer_stats *peer_stats_intf)
{
	struct dp_rx_tid *rx_tid = NULL;
	uint8_t i = 0;

	for (i = 0; i < DP_MAX_TIDS; i++) {
		rx_tid = &peer->rx_tid[i];
		peer_stats_intf->rx_byte_count +=
			rx_tid->rx_msdu_cnt.bytes;
		peer_stats_intf->rx_packet_count +=
			rx_tid->rx_msdu_cnt.num;
	}
	peer_stats_intf->tx_packet_count =
		peer->monitor_peer->stats.tx.tx_ucast_success.num;
	peer_stats_intf->tx_byte_count =
		peer->monitor_peer->stats.tx.tx_ucast_success.bytes;
}
#else
void dp_peer_get_tx_rx_stats(struct dp_peer *peer,
			     struct cdp_interface_peer_stats *peer_stats_intf)
{
	struct dp_txrx_peer *txrx_peer = NULL;
	struct dp_peer *tgt_peer = NULL;

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	txrx_peer = tgt_peer->txrx_peer;
	peer_stats_intf->rx_packet_count = txrx_peer->to_stack.num;
	peer_stats_intf->rx_byte_count = txrx_peer->to_stack.bytes;
	peer_stats_intf->tx_packet_count =
			txrx_peer->stats.per_pkt_stats.tx.ucast.num;
	peer_stats_intf->tx_byte_count =
			txrx_peer->stats.per_pkt_stats.tx.tx_success.bytes;
}
#endif

QDF_STATUS dp_peer_stats_notify(struct dp_pdev *dp_pdev, struct dp_peer *peer)
{
	struct cdp_interface_peer_stats peer_stats_intf = {0};
	struct dp_mon_peer_stats *mon_peer_stats = NULL;
	struct dp_peer *tgt_peer = NULL;
	struct dp_txrx_peer *txrx_peer = NULL;

	if (qdf_unlikely(!peer || !peer->vdev || !peer->monitor_peer))
		return QDF_STATUS_E_FAULT;

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	if (qdf_unlikely(!tgt_peer))
		return QDF_STATUS_E_FAULT;

	txrx_peer = tgt_peer->txrx_peer;
	if (!qdf_unlikely(txrx_peer))
		return QDF_STATUS_E_FAULT;

	mon_peer_stats = &peer->monitor_peer->stats;

	if (mon_peer_stats->rx.last_snr != mon_peer_stats->rx.snr)
		peer_stats_intf.rssi_changed = true;

	if ((mon_peer_stats->rx.snr && peer_stats_intf.rssi_changed) ||
	    (mon_peer_stats->tx.tx_rate &&
	     mon_peer_stats->tx.tx_rate != mon_peer_stats->tx.last_tx_rate)) {
		qdf_mem_copy(peer_stats_intf.peer_mac, peer->mac_addr.raw,
			     QDF_MAC_ADDR_SIZE);
		peer_stats_intf.vdev_id = peer->vdev->vdev_id;
		peer_stats_intf.last_peer_tx_rate =
					mon_peer_stats->tx.last_tx_rate;
		peer_stats_intf.peer_tx_rate = mon_peer_stats->tx.tx_rate;
		peer_stats_intf.peer_rssi = mon_peer_stats->rx.snr;
		peer_stats_intf.ack_rssi = mon_peer_stats->tx.last_ack_rssi;
		dp_peer_get_tx_rx_stats(peer, &peer_stats_intf);
		peer_stats_intf.per = tgt_peer->stats.tx.last_per;
		peer_stats_intf.free_buff = INVALID_FREE_BUFF;
		dp_wdi_event_handler(WDI_EVENT_PEER_STATS, dp_pdev->soc,
				     (void *)&peer_stats_intf, 0,
				     WDI_NO_VAL, dp_pdev->pdev_id);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef FEATURE_NAC_RSSI
/**
 * dp_rx_nac_filter() - Function to perform filtering of non-associated
 * clients
 * @pdev: DP pdev handle
 * @rx_pkt_hdr: Rx packet Header
 *
 * Return: dp_vdev*
 */
static
struct dp_vdev *dp_rx_nac_filter(struct dp_pdev *pdev,
				 uint8_t *rx_pkt_hdr)
{
	struct ieee80211_frame *wh;
	struct dp_neighbour_peer *peer = NULL;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	wh = (struct ieee80211_frame *)rx_pkt_hdr;

	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) != IEEE80211_FC1_DIR_TODS)
		return NULL;

	qdf_spin_lock_bh(&mon_pdev->neighbour_peer_mutex);
	TAILQ_FOREACH(peer, &mon_pdev->neighbour_peers_list,
		      neighbour_peer_list_elem) {
		if (qdf_mem_cmp(&peer->neighbour_peers_macaddr.raw[0],
				wh->i_addr2, QDF_MAC_ADDR_SIZE) == 0) {
			dp_rx_debug("%pK: NAC configuration matched for mac-%2x:%2x:%2x:%2x:%2x:%2x",
				    pdev->soc,
				    peer->neighbour_peers_macaddr.raw[0],
				    peer->neighbour_peers_macaddr.raw[1],
				    peer->neighbour_peers_macaddr.raw[2],
				    peer->neighbour_peers_macaddr.raw[3],
				    peer->neighbour_peers_macaddr.raw[4],
				    peer->neighbour_peers_macaddr.raw[5]);

				qdf_spin_unlock_bh(&mon_pdev->neighbour_peer_mutex);

			return mon_pdev->mvdev;
		}
	}
	qdf_spin_unlock_bh(&mon_pdev->neighbour_peer_mutex);

	return NULL;
}

QDF_STATUS dp_filter_neighbour_peer(struct dp_pdev *pdev,
				    uint8_t *rx_pkt_hdr)
{
	struct dp_vdev *vdev = NULL;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (mon_pdev->filter_neighbour_peers) {
		/* Next Hop scenario not yet handle */
		vdev = dp_rx_nac_filter(pdev, rx_pkt_hdr);
		if (vdev) {
			dp_rx_mon_deliver(pdev->soc, pdev->pdev_id,
					  pdev->invalid_peer_head_msdu,
					  pdev->invalid_peer_tail_msdu);

			pdev->invalid_peer_head_msdu = NULL;
			pdev->invalid_peer_tail_msdu = NULL;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}
#endif

#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
/**
 * dp_update_filter_neighbour_peers() - set neighbour peers(nac clients)
 * address for smart mesh filtering
 * @soc_hdl: cdp soc handle
 * @vdev_id: id of virtual device object
 * @cmd: Add/Del command
 * @macaddr: nac client mac address
 *
 * Return: success/failure
 */
static int dp_update_filter_neighbour_peers(struct cdp_soc_t *soc_hdl,
					    uint8_t vdev_id,
					    uint32_t cmd, uint8_t *macaddr)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev;
	struct dp_neighbour_peer *peer = NULL;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	struct dp_mon_pdev *mon_pdev;

	if (!vdev || !macaddr)
		goto fail0;

	pdev = vdev->pdev;

	if (!pdev)
		goto fail0;

	mon_pdev = pdev->monitor_pdev;

	/* Store address of NAC (neighbour peer) which will be checked
	 * against TA of received packets.
	 */
	if (cmd == DP_NAC_PARAM_ADD) {
		peer = (struct dp_neighbour_peer *)qdf_mem_malloc(
				sizeof(*peer));

		if (!peer) {
			dp_cdp_err("%pK: DP neighbour peer node memory allocation failed"
				   , soc);
			goto fail0;
		}

		qdf_mem_copy(&peer->neighbour_peers_macaddr.raw[0],
			     macaddr, QDF_MAC_ADDR_SIZE);
		peer->vdev = vdev;

		qdf_spin_lock_bh(&mon_pdev->neighbour_peer_mutex);

		/* add this neighbour peer into the list */
		TAILQ_INSERT_TAIL(&mon_pdev->neighbour_peers_list, peer,
				  neighbour_peer_list_elem);
		qdf_spin_unlock_bh(&mon_pdev->neighbour_peer_mutex);

		/* first neighbour */
		if (!mon_pdev->neighbour_peers_added) {
			QDF_STATUS status = QDF_STATUS_SUCCESS;

			mon_pdev->neighbour_peers_added = true;
			dp_mon_filter_setup_smart_monitor(pdev);
			status = dp_mon_filter_update(pdev);
			if (status != QDF_STATUS_SUCCESS) {
				dp_cdp_err("%pK: smart mon filter setup failed",
					   soc);
				dp_mon_filter_reset_smart_monitor(pdev);
				mon_pdev->neighbour_peers_added = false;
			}
		}

	} else if (cmd == DP_NAC_PARAM_DEL) {
		qdf_spin_lock_bh(&mon_pdev->neighbour_peer_mutex);
		TAILQ_FOREACH(peer, &mon_pdev->neighbour_peers_list,
			      neighbour_peer_list_elem) {
			if (!qdf_mem_cmp(&peer->neighbour_peers_macaddr.raw[0],
					 macaddr, QDF_MAC_ADDR_SIZE)) {
				/* delete this peer from the list */
				TAILQ_REMOVE(&mon_pdev->neighbour_peers_list,
					     peer, neighbour_peer_list_elem);
				qdf_mem_free(peer);
				break;
			}
		}
		/* last neighbour deleted */
		if (TAILQ_EMPTY(&mon_pdev->neighbour_peers_list)) {
			QDF_STATUS status = QDF_STATUS_SUCCESS;

			dp_mon_filter_reset_smart_monitor(pdev);
			status = dp_mon_filter_update(pdev);
			if (status != QDF_STATUS_SUCCESS) {
				dp_cdp_err("%pK: smart mon filter clear failed",
					   soc);
			}
			mon_pdev->neighbour_peers_added = false;
		}
		qdf_spin_unlock_bh(&mon_pdev->neighbour_peer_mutex);
	}
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return 1;

fail0:
	if (vdev)
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return 0;
}
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */

/**
 * dp_update_mon_mac_filter() - Set/reset monitor mac filter
 * @soc_hdl: cdp soc handle
 * @vdev_id: id of virtual device object
 * @cmd: Add/Del command
 *
 * Return: 0 for success. nonzero for failure.
 */
static QDF_STATUS dp_update_mon_mac_filter(struct cdp_soc_t *soc_hdl,
					   uint8_t vdev_id, uint32_t cmd)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	struct dp_mon_pdev *mon_pdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!vdev)
		return status;

	pdev = vdev->pdev;
	if (!pdev) {
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return status;
	}

	mon_pdev = pdev->monitor_pdev;
	if (cmd == DP_NAC_PARAM_ADD) {
		/* first neighbour added */
		dp_mon_filter_set_reset_mon_mac_filter(pdev, true);
		status = dp_mon_filter_update(pdev);
		if (status != QDF_STATUS_SUCCESS) {
			dp_cdp_err("%pK: Mon mac filter set failed", soc);
			dp_mon_filter_set_reset_mon_mac_filter(pdev, false);
		}
	} else if (cmd == DP_NAC_PARAM_DEL) {
		/* last neighbour deleted */
		dp_mon_filter_set_reset_mon_mac_filter(pdev, false);
		status = dp_mon_filter_update(pdev);
		if (status != QDF_STATUS_SUCCESS)
			dp_cdp_err("%pK: Mon mac filter reset failed", soc);
	}

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return status;
}

#ifdef ATH_SUPPORT_NAC_RSSI
/**
 * dp_vdev_get_neighbour_rssi() - Store RSSI for configured NAC
 * @soc_hdl: DP soc handle
 * @vdev_id: id of DP vdev handle
 * @mac_addr: neighbour mac
 * @rssi: rssi value
 *
 * Return: 0 for success. nonzero for failure.
 */
static QDF_STATUS  dp_vdev_get_neighbour_rssi(struct cdp_soc_t *soc_hdl,
					      uint8_t vdev_id,
					      char *mac_addr,
					      uint8_t *rssi)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	struct dp_pdev *pdev;
	struct dp_neighbour_peer *peer = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct dp_mon_pdev *mon_pdev;

	if (!vdev)
		return status;

	pdev = vdev->pdev;
	mon_pdev = pdev->monitor_pdev;

	*rssi = 0;
	qdf_spin_lock_bh(&mon_pdev->neighbour_peer_mutex);
	TAILQ_FOREACH(peer, &mon_pdev->neighbour_peers_list,
		      neighbour_peer_list_elem) {
		if (qdf_mem_cmp(&peer->neighbour_peers_macaddr.raw[0],
				mac_addr, QDF_MAC_ADDR_SIZE) == 0) {
			*rssi = peer->rssi;
			status = QDF_STATUS_SUCCESS;
			break;
		}
	}
	qdf_spin_unlock_bh(&mon_pdev->neighbour_peer_mutex);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return status;
}

static QDF_STATUS
dp_config_for_nac_rssi(struct cdp_soc_t *cdp_soc,
		       uint8_t vdev_id,
		       enum cdp_nac_param_cmd cmd, char *bssid,
		       char *client_macaddr,
		       uint8_t chan_num)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	struct dp_pdev *pdev;
	struct dp_mon_pdev *mon_pdev;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	pdev = (struct dp_pdev *)vdev->pdev;

	mon_pdev = pdev->monitor_pdev;
	mon_pdev->nac_rssi_filtering = 1;
	/* Store address of NAC (neighbour peer) which will be checked
	 * against TA of received packets.
	 */

	if (cmd == CDP_NAC_PARAM_ADD) {
		dp_update_filter_neighbour_peers(cdp_soc, vdev->vdev_id,
						 DP_NAC_PARAM_ADD,
						 (uint8_t *)client_macaddr);
	} else if (cmd == CDP_NAC_PARAM_DEL) {
		dp_update_filter_neighbour_peers(cdp_soc, vdev->vdev_id,
						 DP_NAC_PARAM_DEL,
						 (uint8_t *)client_macaddr);
	}

	if (soc->cdp_soc.ol_ops->config_bssid_in_fw_for_nac_rssi)
		soc->cdp_soc.ol_ops->config_bssid_in_fw_for_nac_rssi
			(soc->ctrl_psoc, pdev->pdev_id,
			 vdev->vdev_id, cmd, bssid, client_macaddr);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}
#endif

bool
dp_enable_mon_reap_timer(struct cdp_soc_t *soc_hdl,
			 enum cdp_mon_reap_source source,
			 bool enable)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);

	if (enable)
		return dp_monitor_reap_timer_start(soc, source);
	else
		return dp_monitor_reap_timer_stop(soc, source);
}

#if defined(DP_CON_MON)
#ifndef REMOVE_PKT_LOG
void dp_pkt_log_init(struct cdp_soc_t *soc_hdl, uint8_t pdev_id, void *scn)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *handle =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_mon_pdev *mon_pdev;

	if (!handle) {
		dp_mon_err("pdev handle is NULL");
		return;
	}

	mon_pdev = handle->monitor_pdev;

	if (mon_pdev->pkt_log_init) {
		dp_mon_err("%pK: Packet log not initialized", soc);
		return;
	}

	pktlog_sethandle(&mon_pdev->pl_dev, scn);
	pktlog_set_pdev_id(mon_pdev->pl_dev, pdev_id);
	pktlog_set_callback_regtype(PKTLOG_DEFAULT_CALLBACK_REGISTRATION);

	if (pktlogmod_init(scn)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s: pktlogmod_init failed", __func__);
		mon_pdev->pkt_log_init = false;
	} else {
		mon_pdev->pkt_log_init = true;
	}
}

/**
 * dp_pkt_log_con_service() - connect packet log service
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @scn: device context
 *
 * Return: none
 */
static void dp_pkt_log_con_service(struct cdp_soc_t *soc_hdl,
				   uint8_t pdev_id, void *scn)
{
	dp_pkt_log_init(soc_hdl, pdev_id, scn);
	pktlog_htc_attach();
}

/**
 * dp_pkt_log_exit() - Wrapper API to cleanup pktlog info
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 *
 * Return: none
 */
static void dp_pkt_log_exit(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev) {
		dp_err("pdev handle is NULL");
		return;
	}

	dp_pktlogmod_exit(pdev);
}

#else
static void dp_pkt_log_con_service(struct cdp_soc_t *soc_hdl,
				   uint8_t pdev_id, void *scn)
{
}

static void dp_pkt_log_exit(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
}
#endif
#endif

void dp_neighbour_peers_detach(struct dp_pdev *pdev)
{
	struct dp_neighbour_peer *peer = NULL;
	struct dp_neighbour_peer *temp_peer = NULL;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	TAILQ_FOREACH_SAFE(peer, &mon_pdev->neighbour_peers_list,
			   neighbour_peer_list_elem, temp_peer) {
		/* delete this peer from the list */
		TAILQ_REMOVE(&mon_pdev->neighbour_peers_list,
			     peer, neighbour_peer_list_elem);
		qdf_mem_free(peer);
	}

	qdf_spinlock_destroy(&mon_pdev->neighbour_peer_mutex);
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_tx_enable_enhanced_stats() - Enable enhanced Tx stats
 * @pdev: Datapath pdev handle
 *
 * Return: void
 */
static void dp_mon_tx_enable_enhanced_stats(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(soc);
	if (mon_ops && mon_ops->mon_tx_enable_enhanced_stats)
		mon_ops->mon_tx_enable_enhanced_stats(pdev);
}

/**
 * dp_enable_enhanced_stats()- API to enable enhanced statistcs
 * @soc: DP_SOC handle
 * @pdev_id: id of DP_PDEV handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_enable_enhanced_stats(struct cdp_soc_t *soc, uint8_t pdev_id)
{
	struct dp_pdev *pdev = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						  pdev_id);

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;

	if (!mon_pdev)
		return QDF_STATUS_E_FAILURE;

	if (mon_pdev->enhanced_stats_en == 0)
		dp_cal_client_timer_start(mon_pdev->cal_client_ctx);

	mon_pdev->enhanced_stats_en = 1;
	pdev->enhanced_stats_en = true;

	dp_mon_filter_setup_enhanced_stats(pdev);
	status = dp_mon_filter_update(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		dp_cdp_err("%pK: Failed to set enhanced mode filters", soc);
		dp_mon_filter_reset_enhanced_stats(pdev);
		dp_cal_client_timer_stop(mon_pdev->cal_client_ctx);
		mon_pdev->enhanced_stats_en = 0;
		pdev->enhanced_stats_en = false;
		return QDF_STATUS_E_FAILURE;
	}

	dp_mon_tx_enable_enhanced_stats(pdev);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_mon_tx_disable_enhanced_stats() - Disable enhanced Tx stats
 * @pdev: Datapath pdev handle
 *
 * Return: void
 */
static void dp_mon_tx_disable_enhanced_stats(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(soc);
	if (mon_ops && mon_ops->mon_tx_disable_enhanced_stats)
		mon_ops->mon_tx_disable_enhanced_stats(pdev);
}

/**
 * dp_disable_enhanced_stats()- API to disable enhanced statistcs
 *
 * @soc: the soc handle
 * @pdev_id: pdev_id of pdev
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
dp_disable_enhanced_stats(struct cdp_soc_t *soc, uint8_t pdev_id)
{
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	struct dp_mon_pdev *mon_pdev;


	if (!pdev || !pdev->monitor_pdev)
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;

	if (mon_pdev->enhanced_stats_en == 1)
		dp_cal_client_timer_stop(mon_pdev->cal_client_ctx);

	mon_pdev->enhanced_stats_en = 0;
	pdev->enhanced_stats_en = false;

	dp_mon_tx_disable_enhanced_stats(pdev);

	dp_mon_filter_reset_enhanced_stats(pdev);
	if (dp_mon_filter_update(pdev) != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  FL("Failed to reset enhanced mode filters"));
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WDI_EVENT_ENABLE
QDF_STATUS dp_peer_qos_stats_notify(struct dp_pdev *dp_pdev,
				    struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	struct cdp_interface_peer_qos_stats qos_stats_intf = {0};

	if (qdf_unlikely(ppdu_user->peer_id == HTT_INVALID_PEER)) {
		dp_mon_warn("Invalid peer id");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(qos_stats_intf.peer_mac, ppdu_user->mac_addr,
		     QDF_MAC_ADDR_SIZE);
	qos_stats_intf.frame_control = ppdu_user->frame_control;
	qos_stats_intf.frame_control_info_valid =
			ppdu_user->frame_control_info_valid;
	qos_stats_intf.qos_control = ppdu_user->qos_control;
	qos_stats_intf.qos_control_info_valid =
			ppdu_user->qos_control_info_valid;
	qos_stats_intf.vdev_id = ppdu_user->vdev_id;
	dp_wdi_event_handler(WDI_EVENT_PEER_QOS_STATS, dp_pdev->soc,
			     (void *)&qos_stats_intf, 0,
			     WDI_NO_VAL, dp_pdev->pdev_id);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_peer_qos_stats_notify(struct dp_pdev *dp_pdev,
			 struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif /* QCA_ENHANCED_STATS_SUPPORT */

/**
 * dp_enable_peer_based_pktlog() - Set Flag for peer based filtering
 * for pktlog
 * @soc: cdp_soc handle
 * @pdev_id: id of dp pdev handle
 * @mac_addr: Peer mac address
 * @enb_dsb: Enable or disable peer based filtering
 *
 * Return: QDF_STATUS
 */
static int
dp_enable_peer_based_pktlog(struct cdp_soc_t *soc, uint8_t pdev_id,
			    uint8_t *mac_addr, uint8_t enb_dsb)
{
	struct dp_peer *peer;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						   pdev_id);
	struct dp_mon_pdev *mon_pdev;

	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	mon_pdev = pdev->monitor_pdev;

	peer = dp_peer_find_hash_find((struct dp_soc *)soc, mac_addr,
				      0, DP_VDEV_ALL, DP_MOD_ID_CDP);

	if (!peer) {
		dp_mon_err("Invalid Peer");
		return QDF_STATUS_E_FAILURE;
	}

	if (!IS_MLO_DP_MLD_PEER(peer) && peer->monitor_peer) {
		peer->monitor_peer->peer_based_pktlog_filter = enb_dsb;
		mon_pdev->dp_peer_based_pktlog = enb_dsb;
		status = QDF_STATUS_SUCCESS;
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}

/**
 * dp_peer_update_pkt_capture_params() - Set Rx & Tx Capture flags for a peer
 * @soc: DP_SOC handle
 * @pdev_id: id of DP_PDEV handle
 * @is_rx_pkt_cap_enable: enable/disable Rx packet capture in monitor mode
 * @is_tx_pkt_cap_enable: enable/disable/delete/print
 * Tx packet capture in monitor mode
 * @peer_mac: MAC address for which the above need to be enabled/disabled
 *
 * Return: Success if Rx & Tx capture is enabled for peer, false otherwise
 */
#if defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(WLAN_RX_PKT_CAPTURE_ENH)
static QDF_STATUS
dp_peer_update_pkt_capture_params(ol_txrx_soc_handle soc,
				  uint8_t pdev_id,
				  bool is_rx_pkt_cap_enable,
				  uint8_t is_tx_pkt_cap_enable,
				  uint8_t *peer_mac)
{
	struct dp_peer *peer;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct dp_pdev *pdev =
			dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
							   pdev_id);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	peer = dp_peer_find_hash_find((struct dp_soc *)soc,
				      peer_mac, 0, DP_VDEV_ALL,
				      DP_MOD_ID_CDP);
	if (!peer)
		return QDF_STATUS_E_FAILURE;

	/* we need to set tx pkt capture for non associated peer */
	if (!IS_MLO_DP_MLD_PEER(peer)) {
		status = dp_monitor_tx_peer_filter(pdev, peer,
						   is_tx_pkt_cap_enable,
						   peer_mac);

		status = dp_peer_set_rx_capture_enabled(pdev, peer,
							is_rx_pkt_cap_enable,
							peer_mac);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);

	return status;
}
#endif

#ifdef QCA_MCOPY_SUPPORT
QDF_STATUS dp_mcopy_check_deliver(struct dp_pdev *pdev,
				  uint16_t peer_id,
				  uint32_t ppdu_id,
				  uint8_t first_msdu)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (mon_pdev->mcopy_mode) {
		if (mon_pdev->mcopy_mode == M_COPY) {
			if ((mon_pdev->m_copy_id.tx_ppdu_id == ppdu_id) &&
			    (mon_pdev->m_copy_id.tx_peer_id == peer_id)) {
				return QDF_STATUS_E_INVAL;
			}
		}

		if (!first_msdu)
			return QDF_STATUS_E_INVAL;

		mon_pdev->m_copy_id.tx_ppdu_id = ppdu_id;
		mon_pdev->m_copy_id.tx_peer_id = peer_id;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WDI_EVENT_ENABLE
#ifndef REMOVE_PKT_LOG
static void *dp_get_pldev(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev = dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);

	if (!pdev || !pdev->monitor_pdev)
		return NULL;

	return pdev->monitor_pdev->pl_dev;
}
#else
static void *dp_get_pldev(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	return NULL;
}
#endif
#endif

QDF_STATUS dp_rx_populate_cbf_hdr(struct dp_soc *soc,
				  uint32_t mac_id,
				  uint32_t event,
				  qdf_nbuf_t mpdu,
				  uint32_t msdu_timestamp)
{
	uint32_t data_size, hdr_size, ppdu_id, align4byte;
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	uint32_t *msg_word;

	if (!pdev)
		return QDF_STATUS_E_INVAL;

	ppdu_id = pdev->monitor_pdev->ppdu_info.com_info.ppdu_id;

	hdr_size = HTT_T2H_PPDU_STATS_IND_HDR_SIZE
		+ qdf_offsetof(htt_ppdu_stats_rx_mgmtctrl_payload_tlv, payload);

	data_size = qdf_nbuf_len(mpdu);

	qdf_nbuf_push_head(mpdu, hdr_size);

	msg_word = (uint32_t *)qdf_nbuf_data(mpdu);
	/*
	 * Populate the PPDU Stats Indication header
	 */
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_T2H_MSG_TYPE_PPDU_STATS_IND);
	HTT_T2H_PPDU_STATS_MAC_ID_SET(*msg_word, mac_id);
	HTT_T2H_PPDU_STATS_PDEV_ID_SET(*msg_word, pdev->pdev_id);
	align4byte = ((data_size +
		qdf_offsetof(htt_ppdu_stats_rx_mgmtctrl_payload_tlv, payload)
		+ 3) >> 2) << 2;
	HTT_T2H_PPDU_STATS_PAYLOAD_SIZE_SET(*msg_word, align4byte);
	msg_word++;
	HTT_T2H_PPDU_STATS_PPDU_ID_SET(*msg_word, ppdu_id);
	msg_word++;

	*msg_word = msdu_timestamp;
	msg_word++;
	/* Skip reserved field */
	msg_word++;
	/*
	 * Populate MGMT_CTRL Payload TLV first
	 */
	HTT_STATS_TLV_TAG_SET(*msg_word,
			      HTT_PPDU_STATS_RX_MGMTCTRL_PAYLOAD_TLV);

	align4byte = ((data_size - sizeof(htt_tlv_hdr_t) +
		qdf_offsetof(htt_ppdu_stats_rx_mgmtctrl_payload_tlv, payload)
		+ 3) >> 2) << 2;
	HTT_STATS_TLV_LENGTH_SET(*msg_word, align4byte);
	msg_word++;

	HTT_PPDU_STATS_RX_MGMTCTRL_TLV_FRAME_LENGTH_SET(
		*msg_word, data_size);
	msg_word++;

	dp_wdi_event_handler(event, soc, (void *)mpdu,
			     HTT_INVALID_PEER, WDI_NO_VAL, pdev->pdev_id);

	qdf_nbuf_pull_head(mpdu, hdr_size);

	return QDF_STATUS_SUCCESS;
}

#ifdef ATH_SUPPORT_EXT_STAT
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
/**
 * dp_pdev_clear_link_airtime_stats() - clear pdev airtime stats for given peer
 * @pdev: DP PDEV handle
 */
static inline
void dp_pdev_clear_link_airtime_stats(struct dp_pdev *pdev)
{
	uint8_t ac;

	for (ac = 0; ac < WME_AC_MAX; ac++)
		pdev->stats.telemetry_stats.link_airtime[ac] = 0;
}

/**
 * dp_peer_update_telemetry_stats() - update peer telemetry stats
 * @peer: Datapath peer
 */
static inline
void dp_peer_update_telemetry_stats(struct dp_peer *peer)
{
	struct dp_pdev *pdev;
	struct dp_vdev *vdev;
	struct dp_mon_peer *mon_peer = NULL;
	uint8_t ac;

	vdev = peer->vdev;
	if (!vdev)
		return;

	pdev = vdev->pdev;
	if (!pdev)
		return;

	mon_peer = peer->monitor_peer;
	if (qdf_likely(mon_peer)) {
		for (ac = 0; ac < WME_AC_MAX; ac++) {
			mon_peer->stats.airtime_consumption[ac].avg_consumption_per_sec =
					mon_peer->stats.airtime_consumption[ac].consumption;
			/* Store each peer airtime consumption in pdev
			 * link_airtime to calculate pdev's total airtime
			 * consumption
			 */
			DP_STATS_INC(
				pdev,
				telemetry_stats.link_airtime[ac],
				mon_peer->stats.airtime_consumption[ac].consumption);
			mon_peer->stats.airtime_consumption[ac].consumption = 0;
		}
	}
}
#else
static inline
void dp_pdev_clear_link_airtime_stats(struct dp_pdev *pdev)
{ }

static inline
void dp_peer_update_telemetry_stats(struct dp_peer *peer)
{ }
#endif

/**
 * dp_peer_cal_clients_stats_update() - update peer stats on cal client timer
 * @soc: Datapath SOC
 * @peer: Datapath peer
 * @arg: argument to iter function
 */
#ifdef IPA_OFFLOAD
static void
dp_peer_cal_clients_stats_update(struct dp_soc *soc,
				 struct dp_peer *peer,
				 void *arg)
{
	struct cdp_calibr_stats_intf peer_stats_intf = {0};
	struct dp_peer *tgt_peer = NULL;
	struct dp_txrx_peer *txrx_peer = NULL;

	dp_peer_update_telemetry_stats(peer);

	if (!dp_peer_is_primary_link_peer(peer))
		return;

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	if (!tgt_peer || !(tgt_peer->txrx_peer))
		return;

	txrx_peer = tgt_peer->txrx_peer;
	peer_stats_intf.to_stack = txrx_peer->to_stack;
	peer_stats_intf.tx_success =
				peer->monitor_peer->stats.tx.tx_ucast_success;
	peer_stats_intf.tx_ucast =
				peer->monitor_peer->stats.tx.tx_ucast_total;

	dp_cal_client_update_peer_stats_wifi3(&peer_stats_intf,
					      &tgt_peer->stats);
	dp_peer_get_rxtid_stats_ipa(peer, dp_peer_update_tid_stats_from_reo);
}
#else
static void
dp_peer_cal_clients_stats_update(struct dp_soc *soc,
				 struct dp_peer *peer,
				 void *arg)
{
	struct cdp_calibr_stats_intf peer_stats_intf = {0};
	struct dp_peer *tgt_peer = NULL;
	struct dp_txrx_peer *txrx_peer = NULL;

	dp_peer_update_telemetry_stats(peer);

	if (!dp_peer_is_primary_link_peer(peer))
		return;

	tgt_peer = dp_get_tgt_peer_from_peer(peer);
	if (!tgt_peer || !(tgt_peer->txrx_peer))
		return;

	txrx_peer = tgt_peer->txrx_peer;
	peer_stats_intf.to_stack = txrx_peer->to_stack;
	peer_stats_intf.tx_success =
				txrx_peer->stats.per_pkt_stats.tx.tx_success;
	peer_stats_intf.tx_ucast =
				txrx_peer->stats.per_pkt_stats.tx.ucast;

	dp_cal_client_update_peer_stats_wifi3(&peer_stats_intf,
					      &tgt_peer->stats);
}
#endif

/**
 * dp_iterate_update_peer_list() - update peer stats on cal client timer
 * @pdev_hdl: pdev handle
 */
static void dp_iterate_update_peer_list(struct cdp_pdev *pdev_hdl)
{
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_hdl;

	/* Clear current airtime stats as the below API will increment the stats
	 * for all peers on top of current value
	 */
	dp_pdev_clear_link_airtime_stats(pdev);
	dp_pdev_iterate_peer(pdev, dp_peer_cal_clients_stats_update, NULL,
			     DP_MOD_ID_CDP);
}
#else
static void  dp_iterate_update_peer_list(struct cdp_pdev *pdev_hdl)
{
}
#endif

#ifdef ATH_SUPPORT_NAC
int dp_set_filter_neigh_peers(struct dp_pdev *pdev,
			      bool val)
{
	/* Enable/Disable smart mesh filtering. This flag will be checked
	 * during rx processing to check if packets are from NAC clients.
	 */
	pdev->monitor_pdev->filter_neighbour_peers = val;
	return 0;
}
#endif /* ATH_SUPPORT_NAC */

#ifdef WLAN_ATF_ENABLE
void dp_set_atf_stats_enable(struct dp_pdev *pdev, bool value)
{
	if (!pdev) {
		dp_cdp_err("Invalid pdev");
		return;
	}

	pdev->monitor_pdev->dp_atf_stats_enable = value;
}
#endif

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_process_ppdu_stats_tx_mgmtctrl_payload_tlv() - Process
 * htt_ppdu_stats_tx_mgmtctrl_payload_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the htt_ppdu_stats_tx_mgmtctrl_payload_tlv
 * @ppdu_id: PPDU Id
 *
 * Return: QDF_STATUS_SUCCESS if nbuf has to be freed in caller
 */
static QDF_STATUS
dp_process_ppdu_stats_tx_mgmtctrl_payload_tlv(struct dp_pdev *pdev,
					      qdf_nbuf_t tag_buf,
					      uint32_t ppdu_id)
{
	uint32_t *nbuf_ptr;
	uint8_t trim_size;
	size_t head_size;
	struct cdp_tx_mgmt_comp_info *ptr_mgmt_comp_info;
	uint32_t *msg_word;
	uint32_t tsf_hdr;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if ((!mon_pdev->tx_sniffer_enable) && (!mon_pdev->mcopy_mode) &&
	    (!mon_pdev->bpr_enable) && (!mon_pdev->tx_capture_enabled))
		return QDF_STATUS_SUCCESS;

	/*
	 * get timestamp from htt_t2h_ppdu_stats_ind_hdr_t
	 */
	msg_word = (uint32_t *)qdf_nbuf_data(tag_buf);
	msg_word = msg_word + 2;
	tsf_hdr = *msg_word;

	trim_size = ((mon_pdev->mgmtctrl_frm_info.mgmt_buf +
		      HTT_MGMT_CTRL_TLV_HDR_RESERVERD_LEN) -
		      qdf_nbuf_data(tag_buf));

	if (!qdf_nbuf_pull_head(tag_buf, trim_size))
		return QDF_STATUS_SUCCESS;

	qdf_nbuf_trim_tail(tag_buf, qdf_nbuf_len(tag_buf) -
			    mon_pdev->mgmtctrl_frm_info.mgmt_buf_len);

	if (mon_pdev->tx_capture_enabled) {
		head_size = sizeof(struct cdp_tx_mgmt_comp_info);
		if (qdf_unlikely(qdf_nbuf_headroom(tag_buf) < head_size)) {
			qdf_err("Fail to get headroom h_sz %zu h_avail %d\n",
				head_size, qdf_nbuf_headroom(tag_buf));
			qdf_assert_always(0);
			return QDF_STATUS_E_NOMEM;
		}
		ptr_mgmt_comp_info = (struct cdp_tx_mgmt_comp_info *)
					qdf_nbuf_push_head(tag_buf, head_size);
		qdf_assert_always(ptr_mgmt_comp_info);
		ptr_mgmt_comp_info->ppdu_id = ppdu_id;
		ptr_mgmt_comp_info->is_sgen_pkt = true;
		ptr_mgmt_comp_info->tx_tsf = tsf_hdr;
	} else {
		head_size = sizeof(ppdu_id);
		nbuf_ptr = (uint32_t *)qdf_nbuf_push_head(tag_buf, head_size);
		*nbuf_ptr = ppdu_id;
	}
	if (mon_pdev->bpr_enable) {
		dp_wdi_event_handler(WDI_EVENT_TX_BEACON, pdev->soc,
				     tag_buf, HTT_INVALID_PEER,
				     WDI_NO_VAL, pdev->pdev_id);
	}

	dp_deliver_mgmt_frm(pdev, tag_buf);

	return QDF_STATUS_E_ALREADY;
}

int
dp_htt_get_ppdu_sniffer_ampdu_tlv_bitmap(uint32_t bitmap)
{
	if (bitmap == (HTT_PPDU_SNIFFER_AMPDU_TLV_BITMAP_64))
		return HTT_PPDU_SNIFFER_AMPDU_TLV_BITMAP_64;
	else if (bitmap == (HTT_PPDU_SNIFFER_AMPDU_TLV_BITMAP_256))
		return HTT_PPDU_SNIFFER_AMPDU_TLV_BITMAP_256;

	return 0;
}

/**
 * dp_peer_copy_delay_stats() - copy ppdu stats to peer delayed stats.
 * @peer: Datapath peer handle
 * @ppdu: User PPDU Descriptor
 * @cur_ppdu_id: PPDU_ID
 *
 * Return: None
 *
 * on Tx data frame, we may get delayed ba set
 * in htt_ppdu_stats_user_common_tlv. which mean we get Block Ack(BA) after we
 * request Block Ack Request(BAR). Successful msdu is received only after Block
 * Ack. To populate peer stats we need successful msdu(data frame).
 * So we hold the Tx data stats on delayed_ba for stats update.
 */
static void
dp_peer_copy_delay_stats(struct dp_peer *peer,
			 struct cdp_tx_completion_ppdu_user *ppdu,
			 uint32_t cur_ppdu_id)
{
	struct dp_pdev *pdev;
	struct dp_vdev *vdev;
	struct dp_mon_peer *mon_peer = peer->monitor_peer;

	if (mon_peer->last_delayed_ba) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "BA not yet recv for prev delayed ppdu[%d] - cur ppdu[%d]",
			  mon_peer->last_delayed_ba_ppduid, cur_ppdu_id);
		vdev = peer->vdev;
		if (vdev) {
			pdev = vdev->pdev;
			pdev->stats.cdp_delayed_ba_not_recev++;
		}
	}

	mon_peer->delayed_ba_ppdu_stats.ltf_size = ppdu->ltf_size;
	mon_peer->delayed_ba_ppdu_stats.stbc = ppdu->stbc;
	mon_peer->delayed_ba_ppdu_stats.he_re = ppdu->he_re;
	mon_peer->delayed_ba_ppdu_stats.txbf = ppdu->txbf;
	mon_peer->delayed_ba_ppdu_stats.bw = ppdu->bw;
	mon_peer->delayed_ba_ppdu_stats.nss = ppdu->nss;
	mon_peer->delayed_ba_ppdu_stats.gi = ppdu->gi;
	mon_peer->delayed_ba_ppdu_stats.dcm = ppdu->dcm;
	mon_peer->delayed_ba_ppdu_stats.ldpc = ppdu->ldpc;
	mon_peer->delayed_ba_ppdu_stats.dcm = ppdu->dcm;
	mon_peer->delayed_ba_ppdu_stats.mpdu_tried_ucast =
					ppdu->mpdu_tried_ucast;
	mon_peer->delayed_ba_ppdu_stats.mpdu_tried_mcast =
					ppdu->mpdu_tried_mcast;
	mon_peer->delayed_ba_ppdu_stats.frame_ctrl = ppdu->frame_ctrl;
	mon_peer->delayed_ba_ppdu_stats.qos_ctrl = ppdu->qos_ctrl;
	mon_peer->delayed_ba_ppdu_stats.dcm = ppdu->dcm;

	mon_peer->delayed_ba_ppdu_stats.ru_start = ppdu->ru_start;
	mon_peer->delayed_ba_ppdu_stats.ru_tones = ppdu->ru_tones;
	mon_peer->delayed_ba_ppdu_stats.is_mcast = ppdu->is_mcast;

	mon_peer->delayed_ba_ppdu_stats.user_pos = ppdu->user_pos;
	mon_peer->delayed_ba_ppdu_stats.mu_group_id = ppdu->mu_group_id;

	mon_peer->last_delayed_ba = true;

	ppdu->debug_copied = true;
}

/**
 * dp_peer_copy_stats_to_bar() - copy delayed stats to ppdu stats.
 * @peer: Datapath peer handle
 * @ppdu: PPDU Descriptor
 *
 * Return: None
 *
 * For Tx BAR, PPDU stats TLV include Block Ack info. PPDU info
 * from Tx BAR frame not required to populate peer stats.
 * But we need successful MPDU and MSDU to update previous
 * transmitted Tx data frame. Overwrite ppdu stats with the previous
 * stored ppdu stats.
 */
static void
dp_peer_copy_stats_to_bar(struct dp_peer *peer,
			  struct cdp_tx_completion_ppdu_user *ppdu)
{
	struct dp_mon_peer *mon_peer = peer->monitor_peer;

	ppdu->ltf_size = mon_peer->delayed_ba_ppdu_stats.ltf_size;
	ppdu->stbc = mon_peer->delayed_ba_ppdu_stats.stbc;
	ppdu->he_re = mon_peer->delayed_ba_ppdu_stats.he_re;
	ppdu->txbf = mon_peer->delayed_ba_ppdu_stats.txbf;
	ppdu->bw = mon_peer->delayed_ba_ppdu_stats.bw;
	ppdu->nss = mon_peer->delayed_ba_ppdu_stats.nss;
	ppdu->gi = mon_peer->delayed_ba_ppdu_stats.gi;
	ppdu->dcm = mon_peer->delayed_ba_ppdu_stats.dcm;
	ppdu->ldpc = mon_peer->delayed_ba_ppdu_stats.ldpc;
	ppdu->dcm = mon_peer->delayed_ba_ppdu_stats.dcm;
	ppdu->mpdu_tried_ucast =
			mon_peer->delayed_ba_ppdu_stats.mpdu_tried_ucast;
	ppdu->mpdu_tried_mcast =
			mon_peer->delayed_ba_ppdu_stats.mpdu_tried_mcast;
	ppdu->frame_ctrl = mon_peer->delayed_ba_ppdu_stats.frame_ctrl;
	ppdu->qos_ctrl = mon_peer->delayed_ba_ppdu_stats.qos_ctrl;
	ppdu->dcm = mon_peer->delayed_ba_ppdu_stats.dcm;

	ppdu->ru_start = mon_peer->delayed_ba_ppdu_stats.ru_start;
	ppdu->ru_tones = mon_peer->delayed_ba_ppdu_stats.ru_tones;
	ppdu->is_mcast = mon_peer->delayed_ba_ppdu_stats.is_mcast;

	ppdu->user_pos = mon_peer->delayed_ba_ppdu_stats.user_pos;
	ppdu->mu_group_id = mon_peer->delayed_ba_ppdu_stats.mu_group_id;

	mon_peer->last_delayed_ba = false;

	ppdu->debug_copied = true;
}

/**
 * dp_tx_rate_stats_update() - Update rate per-peer statistics
 * @peer: Datapath peer handle
 * @ppdu: PPDU Descriptor
 *
 * Return: None
 */
static void
dp_tx_rate_stats_update(struct dp_peer *peer,
			struct cdp_tx_completion_ppdu_user *ppdu)
{
	uint32_t ratekbps = 0;
	uint64_t ppdu_tx_rate = 0;
	uint32_t rix;
	uint16_t ratecode = 0;
	struct dp_mon_peer *mon_peer = NULL;

	if (!peer || !ppdu)
		return;

	if (ppdu->completion_status != HTT_PPDU_STATS_USER_STATUS_OK)
		return;

	mon_peer = peer->monitor_peer;
	if (!mon_peer)
		return;

	ratekbps = dp_getrateindex(ppdu->gi,
				   ppdu->mcs,
				   ppdu->nss,
				   ppdu->preamble,
				   ppdu->bw,
				   ppdu->punc_mode,
				   &rix,
				   &ratecode);

	if (!ratekbps)
		return;

	/* Calculate goodput in non-training period
	 * In training period, don't do anything as
	 * pending pkt is send as goodput.
	 */
	if ((!peer->bss_peer) && (!ppdu->sa_is_training)) {
		ppdu->sa_goodput = ((ratekbps / CDP_NUM_KB_IN_MB) *
				(CDP_PERCENT_MACRO - ppdu->current_rate_per));
	}
	ppdu->rix = rix;
	ppdu->tx_ratekbps = ratekbps;
	ppdu->tx_ratecode = ratecode;
	DP_STATS_UPD(mon_peer, tx.tx_rate, ratekbps);
	mon_peer->stats.tx.avg_tx_rate =
		dp_ath_rate_lpf(mon_peer->stats.tx.avg_tx_rate, ratekbps);
	ppdu_tx_rate = dp_ath_rate_out(mon_peer->stats.tx.avg_tx_rate);
	DP_STATS_UPD(mon_peer, tx.rnd_avg_tx_rate, ppdu_tx_rate);

	mon_peer->stats.tx.bw_info = ppdu->bw;
	mon_peer->stats.tx.gi_info = ppdu->gi;
	mon_peer->stats.tx.nss_info = ppdu->nss;
	mon_peer->stats.tx.mcs_info = ppdu->mcs;
	mon_peer->stats.tx.preamble_info = ppdu->preamble;
	if (peer->vdev) {
		/*
		 * In STA mode:
		 *	We get ucast stats as BSS peer stats.
		 *
		 * In AP mode:
		 *	We get mcast stats as BSS peer stats.
		 *	We get ucast stats as assoc peer stats.
		 */
		if (peer->vdev->opmode == wlan_op_mode_ap && peer->bss_peer) {
			peer->vdev->stats.tx.mcast_last_tx_rate = ratekbps;
			peer->vdev->stats.tx.mcast_last_tx_rate_mcs = ppdu->mcs;
		} else {
			peer->vdev->stats.tx.last_tx_rate = ratekbps;
			peer->vdev->stats.tx.last_tx_rate_mcs = ppdu->mcs;
		}
	}
}

#if defined(FEATURE_PERPKT_INFO) && defined(WDI_EVENT_ENABLE)
void dp_send_stats_event(struct dp_pdev *pdev, struct dp_peer *peer,
			 uint16_t peer_id)
{
	struct cdp_interface_peer_stats peer_stats_intf = {0};
	struct dp_mon_peer *mon_peer = peer->monitor_peer;
	struct dp_txrx_peer *txrx_peer = NULL;

	if (qdf_unlikely(!mon_peer))
		return;

	mon_peer->stats.rx.rx_snr_measured_time = qdf_system_ticks();
	peer_stats_intf.rx_avg_snr = mon_peer->stats.rx.avg_snr;

	txrx_peer = dp_get_txrx_peer(peer);
	if (qdf_likely(txrx_peer)) {
		peer_stats_intf.rx_byte_count = txrx_peer->to_stack.bytes;
		peer_stats_intf.tx_byte_count =
			txrx_peer->stats.per_pkt_stats.tx.tx_success.bytes;
	}

	dp_wdi_event_handler(WDI_EVENT_UPDATE_DP_STATS, pdev->soc,
			     &peer_stats_intf, peer_id,
			     UPDATE_PEER_STATS, pdev->pdev_id);
}
#endif

#ifdef WLAN_FEATURE_11BE
/**
 * dp_get_ru_index_frm_ru_tones() - get ru index
 * @ru_tones: ru tones
 *
 * Return: ru index
 */
static inline enum cdp_ru_index dp_get_ru_index_frm_ru_tones(uint16_t ru_tones)
{
	enum cdp_ru_index ru_index;

	switch (ru_tones) {
	case RU_26:
		ru_index = RU_26_INDEX;
		break;
	case RU_52:
		ru_index = RU_52_INDEX;
		break;
	case RU_52_26:
		ru_index = RU_52_26_INDEX;
		break;
	case RU_106:
		ru_index = RU_106_INDEX;
		break;
	case RU_106_26:
		ru_index = RU_106_26_INDEX;
		break;
	case RU_242:
		ru_index = RU_242_INDEX;
		break;
	case RU_484:
		ru_index = RU_484_INDEX;
		break;
	case RU_484_242:
		ru_index = RU_484_242_INDEX;
		break;
	case RU_996:
		ru_index = RU_996_INDEX;
		break;
	case RU_996_484:
		ru_index = RU_996_484_INDEX;
		break;
	case RU_996_484_242:
		ru_index = RU_996_484_242_INDEX;
		break;
	case RU_2X996:
		ru_index = RU_2X996_INDEX;
		break;
	case RU_2X996_484:
		ru_index = RU_2X996_484_INDEX;
		break;
	case RU_3X996:
		ru_index = RU_3X996_INDEX;
		break;
	case RU_3X996_484:
		ru_index = RU_2X996_484_INDEX;
		break;
	case RU_4X996:
		ru_index = RU_4X996_INDEX;
		break;
	default:
		ru_index = RU_INDEX_MAX;
		break;
	}

	return ru_index;
}

/**
 * dp_mon_get_ru_width_from_ru_size() - get ru_width from ru_size enum
 * @ru_size: HTT ru_size enum
 *
 * Return: ru_width of uint32_t type
 */
static uint32_t dp_mon_get_ru_width_from_ru_size(uint16_t ru_size)
{
	uint32_t width = 0;

	switch (ru_size) {
	case HTT_PPDU_STATS_RU_26:
		width = RU_26;
		break;
	case HTT_PPDU_STATS_RU_52:
		width = RU_52;
		break;
	case HTT_PPDU_STATS_RU_52_26:
		width = RU_52_26;
		break;
	case HTT_PPDU_STATS_RU_106:
		width = RU_106;
		break;
	case HTT_PPDU_STATS_RU_106_26:
		width = RU_106_26;
		break;
	case HTT_PPDU_STATS_RU_242:
		width = RU_242;
		break;
	case HTT_PPDU_STATS_RU_484:
		width = RU_484;
		break;
	case HTT_PPDU_STATS_RU_484_242:
		width = RU_484_242;
		break;
	case HTT_PPDU_STATS_RU_996:
		width = RU_996;
		break;
	case HTT_PPDU_STATS_RU_996_484:
		width = RU_996_484;
		break;
	case HTT_PPDU_STATS_RU_996_484_242:
		width = RU_996_484_242;
		break;
	case HTT_PPDU_STATS_RU_996x2:
		width = RU_2X996;
		break;
	case HTT_PPDU_STATS_RU_996x2_484:
		width = RU_2X996_484;
		break;
	case HTT_PPDU_STATS_RU_996x3:
		width = RU_3X996;
		break;
	case HTT_PPDU_STATS_RU_996x3_484:
		width = RU_3X996_484;
		break;
	case HTT_PPDU_STATS_RU_996x4:
		width = RU_4X996;
		break;
	default:
		dp_mon_debug("Unsupported ru_size: %d rcvd", ru_size);
	}

	return width;
}
#else
static inline enum cdp_ru_index dp_get_ru_index_frm_ru_tones(uint16_t ru_tones)
{
	enum cdp_ru_index ru_index;

	switch (ru_tones) {
	case RU_26:
		ru_index = RU_26_INDEX;
		break;
	case RU_52:
		ru_index = RU_52_INDEX;
		break;
	case RU_106:
		ru_index = RU_106_INDEX;
		break;
	case RU_242:
		ru_index = RU_242_INDEX;
		break;
	case RU_484:
		ru_index = RU_484_INDEX;
		break;
	case RU_996:
		ru_index = RU_996_INDEX;
		break;
	default:
		ru_index = RU_INDEX_MAX;
		break;
	}

	return ru_index;
}

static uint32_t dp_mon_get_ru_width_from_ru_size(uint16_t ru_size)
{
	uint32_t width = 0;

	switch (ru_size) {
	case HTT_PPDU_STATS_RU_26:
		width = RU_26;
		break;
	case HTT_PPDU_STATS_RU_52:
		width = RU_52;
		break;
	case HTT_PPDU_STATS_RU_106:
		width = RU_106;
		break;
	case HTT_PPDU_STATS_RU_242:
		width = RU_242;
		break;
	case HTT_PPDU_STATS_RU_484:
		width = RU_484;
		break;
	case HTT_PPDU_STATS_RU_996:
		width = RU_996;
		break;
	default:
		dp_mon_debug("Unsupported ru_size: %d rcvd", ru_size);
	}

	return width;
}
#endif

#ifdef WLAN_TELEMETRY_STATS_SUPPORT
/**
 * dp_pdev_telemetry_stats_update() - Update pdev telemetry stats
 * @pdev: Datapath pdev handle
 * @ppdu: PPDU Descriptor
 *
 * Return: None
 */
static void
dp_pdev_telemetry_stats_update(
		struct dp_pdev *pdev,
		struct cdp_tx_completion_ppdu_user *ppdu)
{
	uint16_t mpdu_tried;
	uint16_t mpdu_failed;
	uint16_t num_mpdu;
	uint8_t ac = 0;

	num_mpdu = ppdu->mpdu_success;
	mpdu_tried = ppdu->mpdu_tried_ucast + ppdu->mpdu_tried_mcast;
	mpdu_failed = mpdu_tried - num_mpdu;

	ac = TID_TO_WME_AC(ppdu->tid);

	DP_STATS_INC(pdev, telemetry_stats.tx_mpdu_failed[ac],
		     mpdu_failed);

	DP_STATS_INC(pdev, telemetry_stats.tx_mpdu_total[ac],
		     mpdu_tried);
}
#else
static inline void
dp_pdev_telemetry_stats_update(
		struct dp_pdev *pdev,
		struct cdp_tx_completion_ppdu_user *ppdu)
{ }
#endif

/**
 * dp_tx_stats_update() - Update per-peer statistics
 * @pdev: Datapath pdev handle
 * @peer: Datapath peer handle
 * @ppdu: PPDU Descriptor
 * @ack_rssi: RSSI of last ack received
 *
 * Return: None
 */
static void
dp_tx_stats_update(struct dp_pdev *pdev, struct dp_peer *peer,
		   struct cdp_tx_completion_ppdu_user *ppdu,
		   uint32_t ack_rssi)
{
	uint8_t preamble, mcs;
	uint16_t num_msdu;
	uint16_t num_mpdu;
	uint16_t mpdu_tried;
	uint16_t mpdu_failed;
	struct dp_mon_ops *mon_ops;
	enum cdp_ru_index ru_index;
	struct dp_mon_peer *mon_peer = NULL;
	uint32_t ratekbps = 0;
	uint64_t tx_byte_count;

	preamble = ppdu->preamble;
	mcs = ppdu->mcs;
	num_msdu = ppdu->num_msdu;
	num_mpdu = ppdu->mpdu_success;
	mpdu_tried = ppdu->mpdu_tried_ucast + ppdu->mpdu_tried_mcast;
	mpdu_failed = mpdu_tried - num_mpdu;
	tx_byte_count = ppdu->success_bytes;

	/* If the peer statistics are already processed as part of
	 * per-MSDU completion handler, do not process these again in per-PPDU
	 * indications
	 */
	if (pdev->soc->process_tx_status)
		return;

	mon_peer = peer->monitor_peer;
	if (!mon_peer)
		return;

	if (!ppdu->is_mcast) {
		DP_STATS_INC(mon_peer, tx.tx_ucast_total.num, num_msdu);
		DP_STATS_INC(mon_peer, tx.tx_ucast_total.bytes,
			     tx_byte_count);
	}

	if (ppdu->completion_status != HTT_PPDU_STATS_USER_STATUS_OK) {
		/*
		 * All failed mpdu will be retried, so incrementing
		 * retries mpdu based on mpdu failed. Even for
		 * ack failure i.e for long retries we get
		 * mpdu failed equal mpdu tried.
		 */
		DP_STATS_INC(mon_peer, tx.retries, mpdu_failed);
		dp_pdev_telemetry_stats_update(pdev, ppdu);
		return;
	}

	if (ppdu->is_ppdu_cookie_valid)
		DP_STATS_INC(mon_peer, tx.num_ppdu_cookie_valid, 1);

	if (ppdu->mu_group_id <= MAX_MU_GROUP_ID &&
	    ppdu->ppdu_type != HTT_PPDU_STATS_PPDU_TYPE_SU) {
		if (qdf_unlikely(ppdu->mu_group_id &&
				 !(ppdu->mu_group_id & (MAX_MU_GROUP_ID - 1))))
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
				  "mu_group_id out of bound!!\n");
		else
			DP_STATS_UPD(mon_peer, tx.mu_group_id[ppdu->mu_group_id],
				     (ppdu->user_pos + 1));
	}

	if (ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA ||
	    ppdu->ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA) {
		DP_STATS_UPD(mon_peer, tx.ru_tones, ppdu->ru_tones);
		DP_STATS_UPD(mon_peer, tx.ru_start, ppdu->ru_start);
		ru_index = dp_get_ru_index_frm_ru_tones(ppdu->ru_tones);
		if (ru_index != RU_INDEX_MAX) {
			DP_STATS_INC(mon_peer, tx.ru_loc[ru_index].num_msdu,
				     num_msdu);
			DP_STATS_INC(mon_peer, tx.ru_loc[ru_index].num_mpdu,
				     num_mpdu);
			DP_STATS_INC(mon_peer, tx.ru_loc[ru_index].mpdu_tried,
				     mpdu_tried);
		}
	}

	/*
	 * All failed mpdu will be retried, so incrementing
	 * retries mpdu based on mpdu failed. Even for
	 * ack failure i.e for long retries we get
	 * mpdu failed equal mpdu tried.
	 */
	DP_STATS_INC(mon_peer, tx.retries, mpdu_failed);

	DP_STATS_INC(mon_peer, tx.transmit_type[ppdu->ppdu_type].num_msdu,
		     num_msdu);
	DP_STATS_INC(mon_peer, tx.transmit_type[ppdu->ppdu_type].num_mpdu,
		     num_mpdu);
	DP_STATS_INC(mon_peer, tx.transmit_type[ppdu->ppdu_type].mpdu_tried,
		     mpdu_tried);

	DP_STATS_INC(mon_peer, tx.sgi_count[ppdu->gi], num_msdu);
	DP_STATS_INC(mon_peer, tx.bw[ppdu->bw], num_msdu);
	DP_STATS_INC(mon_peer, tx.nss[ppdu->nss], num_msdu);
	if (ppdu->tid < CDP_DATA_TID_MAX) {
		DP_STATS_INC(mon_peer, tx.wme_ac_type[TID_TO_WME_AC(ppdu->tid)],
			     num_msdu);
		DP_STATS_INC(mon_peer,
			     tx.wme_ac_type_bytes[TID_TO_WME_AC(ppdu->tid)],
			     tx_byte_count);
	}

	DP_STATS_INCC(mon_peer, tx.stbc, num_msdu, ppdu->stbc);
	DP_STATS_INCC(mon_peer, tx.ldpc, num_msdu, ppdu->ldpc);
	if (!(ppdu->is_mcast) && ppdu->ack_rssi_valid)
		DP_STATS_UPD(mon_peer, tx.last_ack_rssi, ack_rssi);

	if (!ppdu->is_mcast) {
		DP_STATS_INC(mon_peer, tx.tx_ucast_success.num, num_msdu);
		DP_STATS_INC(mon_peer, tx.tx_ucast_success.bytes,
			     tx_byte_count);
	}

	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
		      ((mcs >= MAX_MCS_11A) && (preamble == DOT11_A)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[mcs], num_msdu,
		      ((mcs < MAX_MCS_11A) && (preamble == DOT11_A)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
		      ((mcs >= MAX_MCS_11B) && (preamble == DOT11_B)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[mcs], num_msdu,
		      ((mcs < (MAX_MCS_11B)) && (preamble == DOT11_B)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
		      ((mcs >= MAX_MCS_11A) && (preamble == DOT11_N)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[mcs], num_msdu,
		      ((mcs < MAX_MCS_11A) && (preamble == DOT11_N)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
		      ((mcs >= MAX_MCS_11AC) && (preamble == DOT11_AC)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[mcs], num_msdu,
		      ((mcs < MAX_MCS_11AC) && (preamble == DOT11_AC)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[MAX_MCS - 1], num_msdu,
		      ((mcs >= MAX_MCS_11AX) && (preamble == DOT11_AX)));
	DP_STATS_INCC(mon_peer,
		      tx.pkt_type[preamble].mcs_count[mcs], num_msdu,
		      ((mcs < MAX_MCS_11AX) && (preamble == DOT11_AX)));
	DP_STATS_INCC(mon_peer, tx.ampdu_cnt, num_mpdu, ppdu->is_ampdu);
	DP_STATS_INCC(mon_peer, tx.non_ampdu_cnt, num_mpdu, !(ppdu->is_ampdu));
	DP_STATS_INCC(mon_peer, tx.pream_punct_cnt, 1, ppdu->pream_punct);
	DP_STATS_INC(mon_peer, tx.tx_ppdus, 1);
	DP_STATS_INC(mon_peer, tx.tx_mpdus_success, num_mpdu);
	DP_STATS_INC(mon_peer, tx.tx_mpdus_tried, mpdu_tried);

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (mon_ops && mon_ops->mon_tx_stats_update)
		mon_ops->mon_tx_stats_update(mon_peer, ppdu);

	dp_tx_rate_stats_update(peer, ppdu);
	dp_pdev_telemetry_stats_update(pdev, ppdu);

	dp_peer_stats_notify(pdev, peer);

	ratekbps = mon_peer->stats.tx.tx_rate;
	DP_STATS_UPD(mon_peer, tx.last_tx_rate, ratekbps);

	dp_send_stats_event(pdev, peer, ppdu->peer_id);
}

/**
 * dp_get_ppdu_info_user_index() - Find and allocate a per-user
 * descriptor for a PPDU, if a new peer id arrives in a PPDU
 * @pdev: DP pdev handle
 * @peer_id: peer unique identifier
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: user index to be populated
 */
static uint8_t dp_get_ppdu_info_user_index(struct dp_pdev *pdev,
					   uint16_t peer_id,
					   struct ppdu_info *ppdu_info)
{
	uint8_t user_index = 0;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	while ((user_index + 1) <= ppdu_info->last_user) {
		ppdu_user_desc = &ppdu_desc->user[user_index];
		if (ppdu_user_desc->peer_id != peer_id) {
			user_index++;
			continue;
		} else {
			/* Max users possible is 8 so user array index should
			 * not exceed 7
			 */
			qdf_assert_always(user_index <= (ppdu_desc->max_users - 1));
			return user_index;
		}
	}

	ppdu_info->last_user++;
	/* Max users possible is 8 so last user should not exceed 8 */
	qdf_assert_always(ppdu_info->last_user <= ppdu_desc->max_users);
	return ppdu_info->last_user - 1;
}

/**
 * dp_process_ppdu_stats_common_tlv() - Process htt_ppdu_stats_common_tlv
 * @pdev: DP pdev handle
 * @tag_buf: buffer containing the tlv htt_ppdu_stats_common_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void
dp_process_ppdu_stats_common_tlv(struct dp_pdev *pdev,
				 uint32_t *tag_buf,
				 struct ppdu_info *ppdu_info)
{
	uint16_t frame_type;
	uint16_t frame_ctrl;
	uint16_t freq;
	struct dp_soc *soc = NULL;
	struct cdp_tx_completion_ppdu *ppdu_desc = NULL;
	uint64_t ppdu_start_timestamp;
	uint32_t *start_tag_buf;

	start_tag_buf = tag_buf;
	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	ppdu_desc->ppdu_id = ppdu_info->ppdu_id;

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(RING_ID_SCH_CMD_ID);
	ppdu_info->sched_cmdid =
		HTT_PPDU_STATS_COMMON_TLV_SCH_CMDID_GET(*tag_buf);
	ppdu_desc->num_users =
		HTT_PPDU_STATS_COMMON_TLV_NUM_USERS_GET(*tag_buf);

	qdf_assert_always(ppdu_desc->num_users <= ppdu_desc->max_users);

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(QTYPE_FRM_TYPE);
	frame_type = HTT_PPDU_STATS_COMMON_TLV_FRM_TYPE_GET(*tag_buf);
	ppdu_desc->htt_frame_type = frame_type;

	frame_ctrl = ppdu_desc->frame_ctrl;

	ppdu_desc->bar_ppdu_id = ppdu_info->ppdu_id;

	switch (frame_type) {
	case HTT_STATS_FTYPE_TIDQ_DATA_SU:
	case HTT_STATS_FTYPE_TIDQ_DATA_MU:
	case HTT_STATS_FTYPE_SGEN_QOS_NULL:
		/*
		 * for management packet, frame type come as DATA_SU
		 * need to check frame_ctrl before setting frame_type
		 */
		if (HTT_GET_FRAME_CTRL_TYPE(frame_ctrl) <= FRAME_CTRL_TYPE_CTRL)
			ppdu_desc->frame_type = CDP_PPDU_FTYPE_CTRL;
		else
			ppdu_desc->frame_type = CDP_PPDU_FTYPE_DATA;
	break;
	case HTT_STATS_FTYPE_SGEN_MU_BAR:
	case HTT_STATS_FTYPE_SGEN_BAR:
	case HTT_STATS_FTYPE_SGEN_BE_MU_BAR:
		ppdu_desc->frame_type = CDP_PPDU_FTYPE_BAR;
	break;
	default:
		ppdu_desc->frame_type = CDP_PPDU_FTYPE_CTRL;
	break;
	}

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(FES_DUR_US);
	ppdu_desc->tx_duration = *tag_buf;

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(START_TSTMP_L32_US);
	ppdu_desc->ppdu_start_timestamp = *tag_buf;

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(CHAN_MHZ_PHY_MODE);
	freq = HTT_PPDU_STATS_COMMON_TLV_CHAN_MHZ_GET(*tag_buf);
	if (freq != ppdu_desc->channel) {
		soc = pdev->soc;
		ppdu_desc->channel = freq;
		pdev->operating_channel.freq = freq;
		if (soc && soc->cdp_soc.ol_ops->freq_to_channel)
			pdev->operating_channel.num =
			    soc->cdp_soc.ol_ops->freq_to_channel(soc->ctrl_psoc,
								 pdev->pdev_id,
								 freq);

		if (soc && soc->cdp_soc.ol_ops->freq_to_band)
			pdev->operating_channel.band =
			       soc->cdp_soc.ol_ops->freq_to_band(soc->ctrl_psoc,
								 pdev->pdev_id,
								 freq);
	}

	ppdu_desc->phy_mode = HTT_PPDU_STATS_COMMON_TLV_PHY_MODE_GET(*tag_buf);

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(RESV_NUM_UL_BEAM);
	ppdu_desc->phy_ppdu_tx_time_us =
		HTT_PPDU_STATS_COMMON_TLV_PHY_PPDU_TX_TIME_US_GET(*tag_buf);
	ppdu_desc->beam_change =
		HTT_PPDU_STATS_COMMON_TLV_BEAM_CHANGE_GET(*tag_buf);
	ppdu_desc->doppler =
		HTT_PPDU_STATS_COMMON_TLV_DOPPLER_INDICATION_GET(*tag_buf);
	ppdu_desc->spatial_reuse =
		HTT_PPDU_STATS_COMMON_TLV_SPATIAL_REUSE_GET(*tag_buf);

	dp_tx_capture_htt_frame_counter(pdev, frame_type);

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(START_TSTMP_U32_US);
	ppdu_start_timestamp = *tag_buf;
	ppdu_desc->ppdu_start_timestamp |= ((ppdu_start_timestamp <<
					     HTT_SHIFT_UPPER_TIMESTAMP) &
					    HTT_MASK_UPPER_TIMESTAMP);

	ppdu_desc->ppdu_end_timestamp = ppdu_desc->ppdu_start_timestamp +
					ppdu_desc->tx_duration;
	/* Ack time stamp is same as end time stamp*/
	ppdu_desc->ack_timestamp = ppdu_desc->ppdu_end_timestamp;

	ppdu_desc->ppdu_end_timestamp = ppdu_desc->ppdu_start_timestamp +
					ppdu_desc->tx_duration;

	ppdu_desc->bar_ppdu_start_timestamp = ppdu_desc->ppdu_start_timestamp;
	ppdu_desc->bar_ppdu_end_timestamp = ppdu_desc->ppdu_end_timestamp;
	ppdu_desc->bar_tx_duration = ppdu_desc->tx_duration;

	/* Ack time stamp is same as end time stamp*/
	ppdu_desc->ack_timestamp = ppdu_desc->ppdu_end_timestamp;

	tag_buf = start_tag_buf + HTT_GET_STATS_CMN_INDEX(BSSCOLOR_OBSS_PSR);
	ppdu_desc->bss_color =
		HTT_PPDU_STATS_COMMON_TLV_BSS_COLOR_ID_GET(*tag_buf);
}

/**
 * dp_process_ppdu_stats_user_common_tlv() - Process ppdu_stats_user_common
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the tlv htt_ppdu_stats_user_common_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_user_common_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	uint16_t peer_id;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	uint8_t curr_user_index = 0;
	struct dp_peer *peer;
	struct dp_vdev *vdev;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;
	peer_id = HTT_PPDU_STATS_USER_RATE_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index =
		dp_get_ppdu_info_user_index(pdev,
					    peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);

	ppdu_desc->vdev_id =
		HTT_PPDU_STATS_USER_COMMON_TLV_VAP_ID_GET(*tag_buf);

	ppdu_user_desc->peer_id = peer_id;

	tag_buf++;

	if (HTT_PPDU_STATS_USER_COMMON_TLV_DELAYED_BA_GET(*tag_buf)) {
		ppdu_user_desc->delayed_ba = 1;
		ppdu_desc->delayed_ba = 1;
	}

	if (HTT_PPDU_STATS_USER_COMMON_TLV_MCAST_GET(*tag_buf)) {
		ppdu_user_desc->is_mcast = true;
		ppdu_user_desc->mpdu_tried_mcast =
		HTT_PPDU_STATS_USER_COMMON_TLV_MPDUS_TRIED_GET(*tag_buf);
		ppdu_user_desc->num_mpdu = ppdu_user_desc->mpdu_tried_mcast;
	} else {
		ppdu_user_desc->mpdu_tried_ucast =
		HTT_PPDU_STATS_USER_COMMON_TLV_MPDUS_TRIED_GET(*tag_buf);
	}

	ppdu_user_desc->is_seq_num_valid =
	HTT_PPDU_STATS_USER_COMMON_TLV_IS_SQNUM_VALID_IN_BUFFER_GET(*tag_buf);
	tag_buf++;

	ppdu_user_desc->qos_ctrl =
		HTT_PPDU_STATS_USER_COMMON_TLV_QOS_CTRL_GET(*tag_buf);
	ppdu_user_desc->frame_ctrl =
		HTT_PPDU_STATS_USER_COMMON_TLV_FRAME_CTRL_GET(*tag_buf);
	ppdu_desc->frame_ctrl = ppdu_user_desc->frame_ctrl;

	if (ppdu_user_desc->delayed_ba)
		ppdu_user_desc->mpdu_success = 0;

	tag_buf += 3;

	if (HTT_PPDU_STATS_IS_OPAQUE_VALID_GET(*tag_buf)) {
		ppdu_user_desc->ppdu_cookie =
			HTT_PPDU_STATS_HOST_OPAQUE_COOKIE_GET(*tag_buf);
		ppdu_user_desc->is_ppdu_cookie_valid = 1;
	}

	/* returning earlier causes other feilds unpopulated */
	if (peer_id == DP_SCAN_PEER_ID) {
		vdev = dp_vdev_get_ref_by_id(pdev->soc, ppdu_desc->vdev_id,
					     DP_MOD_ID_TX_PPDU_STATS);
		if (!vdev)
			return;
		qdf_mem_copy(ppdu_user_desc->mac_addr, vdev->mac_addr.raw,
			     QDF_MAC_ADDR_SIZE);
		dp_vdev_unref_delete(pdev->soc, vdev, DP_MOD_ID_TX_PPDU_STATS);
	} else {
		peer = dp_peer_get_ref_by_id(pdev->soc, peer_id,
					     DP_MOD_ID_TX_PPDU_STATS);
		if (!peer) {
			/*
			 * fw sends peer_id which is about to removed but
			 * it was already removed in host.
			 * eg: for disassoc, fw send ppdu stats
			 * with peer id equal to previously associated
			 * peer's peer_id but it was removed
			 */
			vdev = dp_vdev_get_ref_by_id(pdev->soc,
						     ppdu_desc->vdev_id,
						     DP_MOD_ID_TX_PPDU_STATS);
			if (!vdev)
				return;
			qdf_mem_copy(ppdu_user_desc->mac_addr,
				     vdev->mac_addr.raw, QDF_MAC_ADDR_SIZE);
			dp_vdev_unref_delete(pdev->soc, vdev,
					     DP_MOD_ID_TX_PPDU_STATS);
			return;
		}
		qdf_mem_copy(ppdu_user_desc->mac_addr,
			     peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);
		dp_peer_unref_delete(peer, DP_MOD_ID_TX_PPDU_STATS);
	}
}

/**
 * dp_process_ppdu_stats_user_rate_tlv() - Process htt_ppdu_stats_user_rate_tlv
 * @pdev: DP pdev handle
 * @tag_buf: T2H message buffer carrying the user rate TLV
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void
dp_process_ppdu_stats_user_rate_tlv(struct dp_pdev *pdev,
				    uint32_t *tag_buf,
				    struct ppdu_info *ppdu_info)
{
	uint16_t peer_id;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	uint8_t curr_user_index = 0;
	struct dp_vdev *vdev;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);
	uint8_t bw, ru_format;
	uint16_t ru_size;

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;
	peer_id = HTT_PPDU_STATS_USER_RATE_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index =
		dp_get_ppdu_info_user_index(pdev,
					    peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);
	if (peer_id == DP_SCAN_PEER_ID) {
		vdev = dp_vdev_get_ref_by_id(pdev->soc, ppdu_desc->vdev_id,
					     DP_MOD_ID_TX_PPDU_STATS);
		if (!vdev)
			return;
		dp_vdev_unref_delete(pdev->soc, vdev,
				     DP_MOD_ID_TX_PPDU_STATS);
	}
	ppdu_user_desc->peer_id = peer_id;

	ppdu_user_desc->tid =
		HTT_PPDU_STATS_USER_RATE_TLV_TID_NUM_GET(*tag_buf);

	tag_buf += 1;

	ppdu_user_desc->user_pos =
		HTT_PPDU_STATS_USER_RATE_TLV_USER_POS_GET(*tag_buf);
	ppdu_user_desc->mu_group_id =
		HTT_PPDU_STATS_USER_RATE_TLV_MU_GROUPID_GET(*tag_buf);

	ru_format = HTT_PPDU_STATS_USER_RATE_TLV_RU_FORMAT_GET(*tag_buf);

	tag_buf += 1;

	if (!ru_format) {
		/* ru_format = 0: ru_end, ru_start */
		ppdu_user_desc->ru_start =
			HTT_PPDU_STATS_USER_RATE_TLV_RU_START_GET(*tag_buf);
		ppdu_user_desc->ru_tones =
			(HTT_PPDU_STATS_USER_RATE_TLV_RU_END_GET(*tag_buf) -
			HTT_PPDU_STATS_USER_RATE_TLV_RU_START_GET(*tag_buf)) + 1;
	} else if (ru_format == 1) {
		/* ru_format = 1: ru_index, ru_size */
		ru_size = HTT_PPDU_STATS_USER_RATE_TLV_RU_SIZE_GET(*tag_buf);
		ppdu_user_desc->ru_tones =
				dp_mon_get_ru_width_from_ru_size(ru_size);
	} else {
		dp_mon_debug("Unsupported ru_format: %d rcvd", ru_format);
	}
	ppdu_desc->usr_ru_tones_sum += ppdu_user_desc->ru_tones;

	tag_buf += 2;

	ppdu_user_desc->ppdu_type =
		HTT_PPDU_STATS_USER_RATE_TLV_PPDU_TYPE_GET(*tag_buf);

	tag_buf++;
	ppdu_user_desc->tx_rate = *tag_buf;

	ppdu_user_desc->ltf_size =
		HTT_PPDU_STATS_USER_RATE_TLV_LTF_SIZE_GET(*tag_buf);
	ppdu_user_desc->stbc =
		HTT_PPDU_STATS_USER_RATE_TLV_STBC_GET(*tag_buf);
	ppdu_user_desc->he_re =
		HTT_PPDU_STATS_USER_RATE_TLV_HE_RE_GET(*tag_buf);
	ppdu_user_desc->txbf =
		HTT_PPDU_STATS_USER_RATE_TLV_TXBF_GET(*tag_buf);
	bw = HTT_PPDU_STATS_USER_RATE_TLV_BW_GET(*tag_buf);
	/* Align bw value as per host data structures */
	if (bw == HTT_PPDU_STATS_BANDWIDTH_320MHZ)
		ppdu_user_desc->bw = bw - 3;
	else
		ppdu_user_desc->bw = bw - 2;
	ppdu_user_desc->nss = HTT_PPDU_STATS_USER_RATE_TLV_NSS_GET(*tag_buf);
	ppdu_desc->usr_nss_sum += ppdu_user_desc->nss;
	ppdu_user_desc->mcs = HTT_PPDU_STATS_USER_RATE_TLV_MCS_GET(*tag_buf);
	ppdu_user_desc->preamble =
		HTT_PPDU_STATS_USER_RATE_TLV_PREAMBLE_GET(*tag_buf);
	ppdu_user_desc->gi = HTT_PPDU_STATS_USER_RATE_TLV_GI_GET(*tag_buf);
	ppdu_user_desc->dcm = HTT_PPDU_STATS_USER_RATE_TLV_DCM_GET(*tag_buf);
	ppdu_user_desc->ldpc = HTT_PPDU_STATS_USER_RATE_TLV_LDPC_GET(*tag_buf);

	tag_buf += 2;
	ppdu_user_desc->punc_pattern_bitmap =
		HTT_PPDU_STATS_USER_RATE_TLV_PUNC_PATTERN_BITMAP_GET(*tag_buf);
}

/**
 * dp_process_ppdu_stats_enq_mpdu_bitmap_64_tlv() - Process
 * htt_ppdu_stats_enq_mpdu_bitmap_64_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the tlv htt_ppdu_stats_enq_mpdu_bitmap_64_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_enq_mpdu_bitmap_64_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	htt_ppdu_stats_enq_mpdu_bitmap_64_tlv *dp_stats_buf =
		(htt_ppdu_stats_enq_mpdu_bitmap_64_tlv *)tag_buf;

	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	uint8_t curr_user_index = 0;
	uint16_t peer_id;
	uint32_t size = CDP_BA_64_BIT_MAP_SIZE_DWORDS;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;

	peer_id =
	HTT_PPDU_STATS_ENQ_MPDU_BITMAP_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index = dp_get_ppdu_info_user_index(pdev, peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);
	ppdu_user_desc->peer_id = peer_id;

	ppdu_user_desc->start_seq = dp_stats_buf->start_seq;
	qdf_mem_copy(&ppdu_user_desc->enq_bitmap, &dp_stats_buf->enq_bitmap,
		     sizeof(uint32_t) * CDP_BA_64_BIT_MAP_SIZE_DWORDS);

	dp_process_ppdu_stats_update_failed_bitmap(pdev,
						   (void *)ppdu_user_desc,
						   ppdu_info->ppdu_id,
						   size);
}

/**
 * dp_process_ppdu_stats_enq_mpdu_bitmap_256_tlv() - Process
 * htt_ppdu_stats_enq_mpdu_bitmap_256_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the tlv htt_ppdu_stats_enq_mpdu_bitmap_256_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_enq_mpdu_bitmap_256_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	htt_ppdu_stats_enq_mpdu_bitmap_256_tlv *dp_stats_buf =
		(htt_ppdu_stats_enq_mpdu_bitmap_256_tlv *)tag_buf;

	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	uint8_t curr_user_index = 0;
	uint16_t peer_id;
	uint32_t size = CDP_BA_256_BIT_MAP_SIZE_DWORDS;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;

	peer_id =
	HTT_PPDU_STATS_ENQ_MPDU_BITMAP_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index = dp_get_ppdu_info_user_index(pdev, peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);
	ppdu_user_desc->peer_id = peer_id;

	ppdu_user_desc->start_seq = dp_stats_buf->start_seq;
	qdf_mem_copy(&ppdu_user_desc->enq_bitmap, &dp_stats_buf->enq_bitmap,
		     sizeof(uint32_t) * CDP_BA_256_BIT_MAP_SIZE_DWORDS);

	dp_process_ppdu_stats_update_failed_bitmap(pdev,
						   (void *)ppdu_user_desc,
						   ppdu_info->ppdu_id,
						   size);
}

/**
 * dp_process_ppdu_stats_user_cmpltn_common_tlv() - Process
 * htt_ppdu_stats_user_cmpltn_common_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the tlv htt_ppdu_stats_user_cmpltn_common_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_user_cmpltn_common_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	uint16_t peer_id;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	uint8_t curr_user_index = 0;
	uint8_t bw_iter;
	htt_ppdu_stats_user_cmpltn_common_tlv *dp_stats_buf =
		(htt_ppdu_stats_user_cmpltn_common_tlv *)tag_buf;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;
	peer_id =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index = dp_get_ppdu_info_user_index(pdev, peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);
	ppdu_user_desc->peer_id = peer_id;

	ppdu_user_desc->completion_status =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_COMPLETION_STATUS_GET(
				*tag_buf);

	ppdu_user_desc->tid =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_TID_NUM_GET(*tag_buf);

	tag_buf++;
	if (qdf_likely(ppdu_user_desc->completion_status ==
			HTT_PPDU_STATS_USER_STATUS_OK)) {
		ppdu_desc->ack_rssi = dp_stats_buf->ack_rssi;
		ppdu_user_desc->usr_ack_rssi = dp_stats_buf->ack_rssi;
		ppdu_user_desc->ack_rssi_valid = 1;
	} else {
		ppdu_user_desc->ack_rssi_valid = 0;
	}

	tag_buf++;

	ppdu_user_desc->mpdu_success =
	HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_MPDU_SUCCESS_GET(*tag_buf);

	ppdu_user_desc->mpdu_failed =
	HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_MPDU_TRIED_GET(*tag_buf) -
						ppdu_user_desc->mpdu_success;

	tag_buf++;

	ppdu_user_desc->long_retries =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_LONG_RETRY_GET(*tag_buf);

	ppdu_user_desc->short_retries =
	HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_SHORT_RETRY_GET(*tag_buf);
	ppdu_user_desc->retry_mpdus =
		ppdu_user_desc->long_retries + ppdu_user_desc->short_retries;

	ppdu_user_desc->is_ampdu =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_IS_AMPDU_GET(*tag_buf);
	ppdu_info->is_ampdu = ppdu_user_desc->is_ampdu;

	ppdu_desc->resp_type =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_RESP_TYPE_GET(*tag_buf);
	ppdu_desc->mprot_type =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_MPROT_TYPE_GET(*tag_buf);
	ppdu_desc->rts_success =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_RTS_SUCCESS_GET(*tag_buf);
	ppdu_desc->rts_failure =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_RTS_FAILURE_GET(*tag_buf);

	ppdu_user_desc->mprot_type = ppdu_desc->mprot_type;
	ppdu_user_desc->rts_success = ppdu_desc->rts_success;
	ppdu_user_desc->rts_failure = ppdu_desc->rts_failure;

	ppdu_user_desc->pream_punct =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_PREAM_PUNC_TX_GET(*tag_buf);

	ppdu_info->compltn_common_tlv++;

	/*
	 * MU BAR may send request to n users but we may received ack only from
	 * m users. To have count of number of users respond back, we have a
	 * separate counter bar_num_users per PPDU that get increment for every
	 * htt_ppdu_stats_user_cmpltn_common_tlv
	 */
	ppdu_desc->bar_num_users++;

	tag_buf++;
	for (bw_iter = 0; bw_iter < CDP_RSSI_CHAIN_LEN; bw_iter++) {
		ppdu_user_desc->rssi_chain[bw_iter] =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_CHAIN_RSSI_GET(*tag_buf);
		tag_buf++;
	}

	ppdu_user_desc->sa_tx_antenna =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_TX_ANTENNA_MASK_GET(*tag_buf);

	tag_buf++;
	ppdu_user_desc->sa_is_training =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_IS_TRAINING_GET(*tag_buf);
	if (ppdu_user_desc->sa_is_training) {
		ppdu_user_desc->sa_goodput =
			HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_PENDING_TRAINING_PKTS_GET(*tag_buf);
	}

	tag_buf++;
	for (bw_iter = 0; bw_iter < CDP_NUM_SA_BW; bw_iter++) {
		ppdu_user_desc->sa_max_rates[bw_iter] =
			HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_MAX_RATES_GET(tag_buf[bw_iter]);
	}

	tag_buf += CDP_NUM_SA_BW;
	ppdu_user_desc->current_rate_per =
		HTT_PPDU_STATS_USER_CMPLTN_COMMON_TLV_CURRENT_RATE_PER_GET(*tag_buf);
}

/**
 * dp_process_ppdu_stats_user_compltn_ba_bitmap_64_tlv() - Process
 * htt_ppdu_stats_user_compltn_ba_bitmap_64_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the htt_ppdu_stats_user_compltn_ba_bitmap_64_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_user_compltn_ba_bitmap_64_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	htt_ppdu_stats_user_compltn_ba_bitmap_64_tlv *dp_stats_buf =
		(htt_ppdu_stats_user_compltn_ba_bitmap_64_tlv *)tag_buf;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	uint8_t curr_user_index = 0;
	uint16_t peer_id;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;

	peer_id =
	HTT_PPDU_STATS_USER_CMPLTN_BA_BITMAP_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index = dp_get_ppdu_info_user_index(pdev, peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);
	ppdu_user_desc->peer_id = peer_id;

	ppdu_user_desc->ba_seq_no = dp_stats_buf->ba_seq_no;
	qdf_mem_copy(&ppdu_user_desc->ba_bitmap, &dp_stats_buf->ba_bitmap,
		     sizeof(uint32_t) * CDP_BA_64_BIT_MAP_SIZE_DWORDS);
	ppdu_user_desc->ba_size = CDP_BA_64_BIT_MAP_SIZE_DWORDS * 32;
}

/**
 * dp_process_ppdu_stats_user_compltn_ba_bitmap_256_tlv() - Process
 * htt_ppdu_stats_user_compltn_ba_bitmap_256_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the htt_ppdu_stats_user_compltn_ba_bitmap_256_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_user_compltn_ba_bitmap_256_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	htt_ppdu_stats_user_compltn_ba_bitmap_256_tlv *dp_stats_buf =
		(htt_ppdu_stats_user_compltn_ba_bitmap_256_tlv *)tag_buf;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	uint8_t curr_user_index = 0;
	uint16_t peer_id;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;

	peer_id =
	HTT_PPDU_STATS_USER_CMPLTN_BA_BITMAP_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index = dp_get_ppdu_info_user_index(pdev, peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);
	ppdu_user_desc->peer_id = peer_id;

	ppdu_user_desc->ba_seq_no = dp_stats_buf->ba_seq_no;
	qdf_mem_copy(&ppdu_user_desc->ba_bitmap, &dp_stats_buf->ba_bitmap,
		     sizeof(uint32_t) * CDP_BA_256_BIT_MAP_SIZE_DWORDS);
	ppdu_user_desc->ba_size = CDP_BA_256_BIT_MAP_SIZE_DWORDS * 32;
}

/**
 * dp_process_ppdu_stats_user_compltn_ack_ba_status_tlv() - Process
 * htt_ppdu_stats_user_compltn_ack_ba_status_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the htt_ppdu_stats_user_compltn_ack_ba_status_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_user_compltn_ack_ba_status_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	uint16_t peer_id;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	uint8_t curr_user_index = 0;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf += 2;
	peer_id =
	HTT_PPDU_STATS_USER_CMPLTN_ACK_BA_STATUS_TLV_SW_PEER_ID_GET(*tag_buf);

	curr_user_index = dp_get_ppdu_info_user_index(pdev, peer_id, ppdu_info);
	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);
	if (!ppdu_user_desc->ack_ba_tlv) {
		ppdu_user_desc->ack_ba_tlv = 1;
	} else {
		pdev->stats.ack_ba_comes_twice++;
		return;
	}

	ppdu_user_desc->peer_id = peer_id;

	tag_buf++;
	/* not to update ppdu_desc->tid from this TLV */
	ppdu_user_desc->num_mpdu =
		HTT_PPDU_STATS_USER_CMPLTN_ACK_BA_STATUS_TLV_NUM_MPDU_GET(*tag_buf);

	ppdu_user_desc->num_msdu =
		HTT_PPDU_STATS_USER_CMPLTN_ACK_BA_STATUS_TLV_NUM_MSDU_GET(*tag_buf);

	ppdu_user_desc->success_msdus = ppdu_user_desc->num_msdu;

	tag_buf++;
	ppdu_user_desc->start_seq =
		HTT_PPDU_STATS_USER_CMPLTN_ACK_BA_STATUS_TLV_START_SEQ_GET(
			*tag_buf);

	tag_buf++;
	ppdu_user_desc->success_bytes = *tag_buf;

	/* increase ack ba tlv counter on successful mpdu */
	if (ppdu_user_desc->num_mpdu)
		ppdu_info->ack_ba_tlv++;

	if (ppdu_user_desc->ba_size == 0) {
		ppdu_user_desc->ba_seq_no = ppdu_user_desc->start_seq;
		ppdu_user_desc->ba_bitmap[0] = 1;
		ppdu_user_desc->ba_size = 1;
	}
}

/**
 * dp_process_ppdu_stats_user_common_array_tlv() - Process
 * htt_ppdu_stats_user_common_array_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the htt_ppdu_stats_user_compltn_ack_ba_status_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_stats_user_common_array_tlv(
		struct dp_pdev *pdev, uint32_t *tag_buf,
		struct ppdu_info *ppdu_info)
{
	uint32_t peer_id;
	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct cdp_tx_completion_ppdu_user *ppdu_user_desc;
	uint8_t curr_user_index = 0;
	struct htt_tx_ppdu_stats_info *dp_stats_buf;
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);

	ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);

	tag_buf++;
	dp_stats_buf = (struct htt_tx_ppdu_stats_info *)tag_buf;
	tag_buf += 3;
	peer_id =
		HTT_PPDU_STATS_ARRAY_ITEM_TLV_PEERID_GET(*tag_buf);

	if (!dp_peer_find_by_id_valid(pdev->soc, peer_id)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid peer");
		return;
	}

	curr_user_index = dp_get_ppdu_info_user_index(pdev, peer_id, ppdu_info);

	ppdu_user_desc = &ppdu_desc->user[curr_user_index];
	ppdu_user_desc->tlv_bitmap |= (1 << tlv_type);

	ppdu_user_desc->retry_bytes = dp_stats_buf->tx_retry_bytes;
	ppdu_user_desc->failed_bytes = dp_stats_buf->tx_failed_bytes;

	tag_buf++;

	ppdu_user_desc->success_msdus =
		HTT_PPDU_STATS_ARRAY_ITEM_TLV_TX_SUCC_MSDUS_GET(*tag_buf);
	ppdu_user_desc->retry_msdus =
		HTT_PPDU_STATS_ARRAY_ITEM_TLV_TX_RETRY_MSDUS_GET(*tag_buf);
	tag_buf++;
	ppdu_user_desc->failed_msdus =
		HTT_PPDU_STATS_ARRAY_ITEM_TLV_TX_FAILED_MSDUS_GET(*tag_buf);
}

/**
 * dp_process_ppdu_stats_user_compltn_flush_tlv() - Process
 * htt_ppdu_stats_flush_tlv
 * @pdev: DP PDEV handle
 * @tag_buf: buffer containing the htt_ppdu_stats_flush_tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void
dp_process_ppdu_stats_user_compltn_flush_tlv(struct dp_pdev *pdev,
					     uint32_t *tag_buf,
					     struct ppdu_info *ppdu_info)
{
	struct cdp_tx_completion_ppdu *ppdu_desc;
	uint32_t peer_id;
	uint8_t tid;
	struct dp_peer *peer;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_peer *mon_peer = NULL;

	ppdu_desc = (struct cdp_tx_completion_ppdu *)
				qdf_nbuf_data(ppdu_info->nbuf);
	ppdu_desc->is_flush = 1;

	tag_buf++;
	ppdu_desc->drop_reason = *tag_buf;

	tag_buf++;
	ppdu_desc->num_msdu = HTT_PPDU_STATS_FLUSH_TLV_NUM_MSDU_GET(*tag_buf);
	ppdu_desc->num_mpdu = HTT_PPDU_STATS_FLUSH_TLV_NUM_MPDU_GET(*tag_buf);
	ppdu_desc->flow_type = HTT_PPDU_STATS_FLUSH_TLV_FLOW_TYPE_GET(*tag_buf);

	tag_buf++;
	peer_id = HTT_PPDU_STATS_FLUSH_TLV_SW_PEER_ID_GET(*tag_buf);
	tid = HTT_PPDU_STATS_FLUSH_TLV_TID_NUM_GET(*tag_buf);

	ppdu_desc->num_users = 1;
	ppdu_desc->user[0].peer_id = peer_id;
	ppdu_desc->user[0].tid = tid;

	ppdu_desc->queue_type =
			HTT_PPDU_STATS_FLUSH_TLV_QUEUE_TYPE_GET(*tag_buf);

	peer = dp_peer_get_ref_by_id(pdev->soc, peer_id,
				     DP_MOD_ID_TX_PPDU_STATS);
	if (!peer)
		goto add_ppdu_to_sched_list;

	if (ppdu_desc->drop_reason == HTT_FLUSH_EXCESS_RETRIES) {
		mon_peer = peer->monitor_peer;
		DP_STATS_INC(mon_peer,
			     tx.excess_retries_per_ac[TID_TO_WME_AC(tid)],
			     ppdu_desc->num_msdu);
	}

	dp_peer_unref_delete(peer, DP_MOD_ID_TX_PPDU_STATS);

add_ppdu_to_sched_list:
	ppdu_info->done = 1;
	TAILQ_REMOVE(&mon_pdev->ppdu_info_list, ppdu_info, ppdu_info_list_elem);
	mon_pdev->list_depth--;
	TAILQ_INSERT_TAIL(&mon_pdev->sched_comp_ppdu_list, ppdu_info,
			  ppdu_info_list_elem);
	mon_pdev->sched_comp_list_depth++;
}

/**
 * dp_process_ppdu_stats_sch_cmd_status_tlv() - Process schedule command status tlv
 * Here we are not going to process the buffer.
 * @pdev: DP PDEV handle
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void
dp_process_ppdu_stats_sch_cmd_status_tlv(struct dp_pdev *pdev,
					 struct ppdu_info *ppdu_info)
{
	struct cdp_tx_completion_ppdu *ppdu_desc;
	struct dp_peer *peer;
	uint8_t num_users;
	uint8_t i;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	ppdu_desc = (struct cdp_tx_completion_ppdu *)
				qdf_nbuf_data(ppdu_info->nbuf);

	num_users = ppdu_desc->bar_num_users;

	for (i = 0; i < num_users; i++) {
		if (ppdu_desc->user[i].user_pos == 0) {
			if (ppdu_desc->frame_type == CDP_PPDU_FTYPE_BAR) {
				/* update phy mode for bar frame */
				ppdu_desc->phy_mode =
					ppdu_desc->user[i].preamble;
				ppdu_desc->user[0].mcs = ppdu_desc->user[i].mcs;
				break;
			}
			if (ppdu_desc->frame_type == CDP_PPDU_FTYPE_CTRL) {
				ppdu_desc->frame_ctrl =
					ppdu_desc->user[i].frame_ctrl;
				break;
			}
		}
	}

	if (ppdu_desc->frame_type == CDP_PPDU_FTYPE_DATA &&
	    ppdu_desc->delayed_ba) {
		qdf_assert_always(ppdu_desc->num_users <= ppdu_desc->max_users);

		for (i = 0; i < ppdu_desc->num_users; i++) {
			struct cdp_delayed_tx_completion_ppdu_user *delay_ppdu;
			uint64_t start_tsf;
			uint64_t end_tsf;
			uint32_t ppdu_id;
			struct dp_mon_peer *mon_peer;

			ppdu_id = ppdu_desc->ppdu_id;
			peer = dp_peer_get_ref_by_id
				(pdev->soc, ppdu_desc->user[i].peer_id,
				 DP_MOD_ID_TX_PPDU_STATS);
			/*
			 * This check is to make sure peer is not deleted
			 * after processing the TLVs.
			 */
			if (!peer)
				continue;

			if (!peer->monitor_peer) {
				dp_peer_unref_delete(peer,
						     DP_MOD_ID_TX_PPDU_STATS);
				continue;
			}

			mon_peer = peer->monitor_peer;
			delay_ppdu = &mon_peer->delayed_ba_ppdu_stats;
			start_tsf = ppdu_desc->ppdu_start_timestamp;
			end_tsf = ppdu_desc->ppdu_end_timestamp;
			/*
			 * save delayed ba user info
			 */
			if (ppdu_desc->user[i].delayed_ba) {
				dp_peer_copy_delay_stats(peer,
							 &ppdu_desc->user[i],
							 ppdu_id);
				mon_peer->last_delayed_ba_ppduid = ppdu_id;
				delay_ppdu->ppdu_start_timestamp = start_tsf;
				delay_ppdu->ppdu_end_timestamp = end_tsf;
			}
			ppdu_desc->user[i].peer_last_delayed_ba =
				mon_peer->last_delayed_ba;

			dp_peer_unref_delete(peer, DP_MOD_ID_TX_PPDU_STATS);

			if (ppdu_desc->user[i].delayed_ba &&
			    !ppdu_desc->user[i].debug_copied) {
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_INFO_MED,
					  "%s: %d ppdu_id[%d] bar_ppdu_id[%d] num_users[%d] usr[%d] htt_frame_type[%d]\n",
					  __func__, __LINE__,
					  ppdu_desc->ppdu_id,
					  ppdu_desc->bar_ppdu_id,
					  ppdu_desc->num_users,
					  i,
					  ppdu_desc->htt_frame_type);
			}
		}
	}

	/*
	 * when frame type is BAR and STATS_COMMON_TLV is set
	 * copy the store peer delayed info to BAR status
	 */
	if (ppdu_desc->frame_type == CDP_PPDU_FTYPE_BAR) {
		for (i = 0; i < ppdu_desc->bar_num_users; i++) {
			struct cdp_delayed_tx_completion_ppdu_user *delay_ppdu;
			uint64_t start_tsf;
			uint64_t end_tsf;
			struct dp_mon_peer *mon_peer;

			peer = dp_peer_get_ref_by_id
				(pdev->soc,
				 ppdu_desc->user[i].peer_id,
				 DP_MOD_ID_TX_PPDU_STATS);
			/*
			 * This check is to make sure peer is not deleted
			 * after processing the TLVs.
			 */
			if (!peer)
				continue;

			if (!peer->monitor_peer) {
				dp_peer_unref_delete(peer,
						     DP_MOD_ID_TX_PPDU_STATS);
				continue;
			}

			mon_peer = peer->monitor_peer;
			if (ppdu_desc->user[i].completion_status !=
			    HTT_PPDU_STATS_USER_STATUS_OK) {
				dp_peer_unref_delete(peer,
						     DP_MOD_ID_TX_PPDU_STATS);
				continue;
			}

			delay_ppdu = &mon_peer->delayed_ba_ppdu_stats;
			start_tsf = delay_ppdu->ppdu_start_timestamp;
			end_tsf = delay_ppdu->ppdu_end_timestamp;

			if (mon_peer->last_delayed_ba) {
				dp_peer_copy_stats_to_bar(peer,
							  &ppdu_desc->user[i]);
				ppdu_desc->ppdu_id =
					mon_peer->last_delayed_ba_ppduid;
				ppdu_desc->ppdu_start_timestamp = start_tsf;
				ppdu_desc->ppdu_end_timestamp = end_tsf;
			}
			ppdu_desc->user[i].peer_last_delayed_ba =
						mon_peer->last_delayed_ba;
			dp_peer_unref_delete(peer, DP_MOD_ID_TX_PPDU_STATS);
		}
	}

	TAILQ_REMOVE(&mon_pdev->ppdu_info_list, ppdu_info, ppdu_info_list_elem);
	mon_pdev->list_depth--;
	TAILQ_INSERT_TAIL(&mon_pdev->sched_comp_ppdu_list, ppdu_info,
			  ppdu_info_list_elem);
	mon_pdev->sched_comp_list_depth++;
}

/**
 * dp_validate_fix_ppdu_tlv() - Function to validate the length of PPDU
 * @pdev: DP pdev handle
 * @tag_buf: TLV buffer
 * @tlv_expected_size: Expected size of Tag
 * @tlv_len: TLV length received from FW
 *
 * If the TLV length sent as part of PPDU TLV is less that expected size i.e
 * size of corresponding data structure, pad the remaining bytes with zeros
 * and continue processing the TLVs
 *
 * Return: Pointer to updated TLV
 */
static inline uint32_t *dp_validate_fix_ppdu_tlv(struct dp_pdev *pdev,
						 uint32_t *tag_buf,
						 uint16_t tlv_expected_size,
						 uint16_t tlv_len)
{
	uint32_t *tlv_desc = tag_buf;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	qdf_assert_always(tlv_len != 0);

	if (tlv_len < tlv_expected_size) {
		qdf_mem_zero(mon_pdev->ppdu_tlv_buf, tlv_expected_size);
		qdf_mem_copy(mon_pdev->ppdu_tlv_buf, tag_buf, tlv_len);
		tlv_desc = mon_pdev->ppdu_tlv_buf;
	}

	return tlv_desc;
}

/**
 * dp_process_ppdu_tag() - Function to process the PPDU TLVs
 * @pdev: DP pdev handle
 * @tag_buf: TLV buffer
 * @tlv_len: length of tlv
 * @ppdu_info: per ppdu tlv structure
 *
 * Return: void
 */
static void dp_process_ppdu_tag(struct dp_pdev *pdev,
				uint32_t *tag_buf,
				uint32_t tlv_len,
				struct ppdu_info *ppdu_info)
{
	uint32_t tlv_type = HTT_STATS_TLV_TAG_GET(*tag_buf);
	uint16_t tlv_expected_size;
	uint32_t *tlv_desc;

	switch (tlv_type) {
	case HTT_PPDU_STATS_COMMON_TLV:
		tlv_expected_size = sizeof(htt_ppdu_stats_common_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_common_tlv(pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_COMMON_TLV:
		tlv_expected_size = sizeof(htt_ppdu_stats_user_common_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_common_tlv(pdev, tlv_desc,
						      ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_RATE_TLV:
		tlv_expected_size = sizeof(htt_ppdu_stats_user_rate_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_rate_tlv(pdev, tlv_desc,
						    ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_MPDU_ENQ_BITMAP_64_TLV:
		tlv_expected_size =
			sizeof(htt_ppdu_stats_enq_mpdu_bitmap_64_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_enq_mpdu_bitmap_64_tlv(
				pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_MPDU_ENQ_BITMAP_256_TLV:
		tlv_expected_size =
			sizeof(htt_ppdu_stats_enq_mpdu_bitmap_256_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_enq_mpdu_bitmap_256_tlv(
				pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_COMPLTN_COMMON_TLV:
		tlv_expected_size =
			sizeof(htt_ppdu_stats_user_cmpltn_common_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_cmpltn_common_tlv(
				pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_COMPLTN_BA_BITMAP_64_TLV:
		tlv_expected_size =
			sizeof(htt_ppdu_stats_user_compltn_ba_bitmap_64_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_compltn_ba_bitmap_64_tlv(
				pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_COMPLTN_BA_BITMAP_256_TLV:
		tlv_expected_size =
			sizeof(htt_ppdu_stats_user_compltn_ba_bitmap_256_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_compltn_ba_bitmap_256_tlv(
				pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_COMPLTN_ACK_BA_STATUS_TLV:
		tlv_expected_size =
			sizeof(htt_ppdu_stats_user_compltn_ack_ba_status_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_compltn_ack_ba_status_tlv(
				pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_COMMON_ARRAY_TLV:
		tlv_expected_size =
			sizeof(htt_ppdu_stats_usr_common_array_tlv_v);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_common_array_tlv(
				pdev, tlv_desc, ppdu_info);
		break;
	case HTT_PPDU_STATS_USR_COMPLTN_FLUSH_TLV:
		tlv_expected_size = sizeof(htt_ppdu_stats_flush_tlv);
		tlv_desc = dp_validate_fix_ppdu_tlv(pdev, tag_buf,
						    tlv_expected_size, tlv_len);
		dp_process_ppdu_stats_user_compltn_flush_tlv(pdev, tlv_desc,
							     ppdu_info);
		break;
	case HTT_PPDU_STATS_SCH_CMD_STATUS_TLV:
		dp_process_ppdu_stats_sch_cmd_status_tlv(pdev, ppdu_info);
		break;
	default:
		break;
	}
}

#ifdef WLAN_TELEMETRY_STATS_SUPPORT
static inline
void dp_ppdu_desc_user_airtime_consumption_update(
			struct dp_peer *peer,
			struct cdp_tx_completion_ppdu_user *user)
{
	struct dp_mon_peer *mon_peer = NULL;
	uint8_t ac = 0;

	mon_peer = peer->monitor_peer;
	if (qdf_unlikely(!mon_peer))
		return;

	ac = TID_TO_WME_AC(user->tid);
	DP_STATS_INC(mon_peer, airtime_consumption[ac].consumption,
		     user->phy_tx_time_us);
}
#else
static inline
void dp_ppdu_desc_user_airtime_consumption_update(
			struct dp_peer *peer,
			struct cdp_tx_completion_ppdu_user *user)
{ }
#endif

#if defined(WLAN_ATF_ENABLE) || defined(WLAN_TELEMETRY_STATS_SUPPORT)
static void
dp_ppdu_desc_user_phy_tx_time_update(struct dp_pdev *pdev,
				     struct dp_peer *peer,
				     struct cdp_tx_completion_ppdu *ppdu_desc,
				     struct cdp_tx_completion_ppdu_user *user)
{
	uint32_t nss_ru_width_sum = 0;
	struct dp_mon_peer *mon_peer = NULL;

	if (!pdev || !ppdu_desc || !user || !peer)
		return;

	if (ppdu_desc->frame_type != CDP_PPDU_FTYPE_DATA)
		return;

	mon_peer = peer->monitor_peer;
	if (qdf_unlikely(!mon_peer))
		return;

	nss_ru_width_sum = ppdu_desc->usr_nss_sum * ppdu_desc->usr_ru_tones_sum;
	if (!nss_ru_width_sum)
		nss_ru_width_sum = 1;

	/*
	 * For SU-MIMO PPDU phy Tx time is same for the single user.
	 * For MU-MIMO phy Tx time is calculated per user as below
	 *     user phy tx time =
	 *           Entire PPDU duration * MU Ratio * OFDMA Ratio
	 *     MU Ratio = usr_nss / Sum_of_nss_of_all_users
	 *     OFDMA_ratio = usr_ru_width / Sum_of_ru_width_of_all_users
	 *     usr_ru_widt = ru_end – ru_start + 1
	 */
	if (ppdu_desc->htt_frame_type == HTT_STATS_FTYPE_TIDQ_DATA_SU) {
		user->phy_tx_time_us = ppdu_desc->phy_ppdu_tx_time_us;
	} else {
		user->phy_tx_time_us = (ppdu_desc->phy_ppdu_tx_time_us *
				user->nss * user->ru_tones) / nss_ru_width_sum;
	}

	dp_ppdu_desc_user_airtime_consumption_update(peer, user);
}
#else
static void
dp_ppdu_desc_user_phy_tx_time_update(struct dp_pdev *pdev,
				     struct dp_peer *peer,
				     struct cdp_tx_completion_ppdu *ppdu_desc,
				     struct cdp_tx_completion_ppdu_user *user)
{
}
#endif

#ifdef WLAN_SUPPORT_CTRL_FRAME_STATS
static void
dp_tx_ctrl_stats_update(struct dp_pdev *pdev, struct dp_peer *peer,
			struct cdp_tx_completion_ppdu_user *user)
{
	struct dp_mon_peer *mon_peer = NULL;
	uint16_t fc = 0;

	if (!pdev || !peer || !user)
		return;

	mon_peer = peer->monitor_peer;
	if (qdf_unlikely(!mon_peer))
		return;

	if (user->mprot_type) {
		DP_STATS_INCC(mon_peer,
			      tx.rts_success, 1, user->rts_success);
		DP_STATS_INCC(mon_peer,
			      tx.rts_failure, 1, user->rts_failure);
	}
	fc = user->frame_ctrl;
	if ((qdf_cpu_to_le16(fc) & QDF_IEEE80211_FC0_TYPE_MASK) ==
	    QDF_IEEE80211_FC0_TYPE_CTL) {
		if ((qdf_cpu_to_le16(fc) & QDF_IEEE80211_FC0_SUBTYPE_MASK) ==
		    QDF_IEEE80211_FC0_SUBTYPE_VHT_NDP_AN)
			DP_STATS_INC(mon_peer, tx.ndpa_cnt, 1);
		if ((qdf_cpu_to_le16(fc) & QDF_IEEE80211_FC0_SUBTYPE_MASK) ==
		    QDF_IEEE80211_FC0_SUBTYPE_BAR)
			DP_STATS_INC(mon_peer, tx.bar_cnt, 1);
	}
}
#else
static void
dp_tx_ctrl_stats_update(struct dp_pdev *pdev, struct dp_peer *peer,
			struct cdp_tx_completion_ppdu_user *user)
{
}
#endif /* WLAN_SUPPORT_CTRL_FRAME_STATS */

void
dp_ppdu_desc_user_stats_update(struct dp_pdev *pdev,
			       struct ppdu_info *ppdu_info)
{
	struct cdp_tx_completion_ppdu *ppdu_desc = NULL;
	struct dp_peer *peer = NULL;
	uint32_t tlv_bitmap_expected;
	uint32_t tlv_bitmap_default;
	uint16_t i;
	uint32_t num_users;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	ppdu_desc = (struct cdp_tx_completion_ppdu *)
		qdf_nbuf_data(ppdu_info->nbuf);

	if (ppdu_desc->frame_type != CDP_PPDU_FTYPE_BAR)
		ppdu_desc->ppdu_id = ppdu_info->ppdu_id;

	tlv_bitmap_expected = HTT_PPDU_DEFAULT_TLV_BITMAP;
	if (mon_pdev->tx_sniffer_enable || mon_pdev->mcopy_mode ||
	    mon_pdev->tx_capture_enabled) {
		if (ppdu_info->is_ampdu)
			tlv_bitmap_expected =
				dp_htt_get_ppdu_sniffer_ampdu_tlv_bitmap(
					ppdu_info->tlv_bitmap);
	}

	tlv_bitmap_default = tlv_bitmap_expected;

	if (ppdu_desc->frame_type == CDP_PPDU_FTYPE_BAR) {
		num_users = ppdu_desc->bar_num_users;
		ppdu_desc->num_users = ppdu_desc->bar_num_users;
	} else {
		num_users = ppdu_desc->num_users;
	}
	qdf_assert_always(ppdu_desc->num_users <= ppdu_desc->max_users);

	for (i = 0; i < num_users; i++) {
		ppdu_desc->num_mpdu += ppdu_desc->user[i].num_mpdu;
		ppdu_desc->num_msdu += ppdu_desc->user[i].num_msdu;

		peer = dp_peer_get_ref_by_id(pdev->soc,
					     ppdu_desc->user[i].peer_id,
					     DP_MOD_ID_TX_PPDU_STATS);
		/*
		 * This check is to make sure peer is not deleted
		 * after processing the TLVs.
		 */
		if (!peer)
			continue;

		ppdu_desc->user[i].is_bss_peer = peer->bss_peer;

		dp_ppdu_desc_user_phy_tx_time_update(pdev, peer, ppdu_desc,
						     &ppdu_desc->user[i]);

		dp_tx_ctrl_stats_update(pdev, peer, &ppdu_desc->user[i]);

		/*
		 * different frame like DATA, BAR or CTRL has different
		 * tlv bitmap expected. Apart from ACK_BA_STATUS TLV, we
		 * receive other tlv in-order/sequential from fw.
		 * Since ACK_BA_STATUS TLV come from Hardware it is
		 * asynchronous So we need to depend on some tlv to confirm
		 * all tlv is received for a ppdu.
		 * So we depend on both SCHED_CMD_STATUS_TLV and
		 * ACK_BA_STATUS_TLV. for failure packet we won't get
		 * ACK_BA_STATUS_TLV.
		 */
		if (!(ppdu_info->tlv_bitmap &
		      (1 << HTT_PPDU_STATS_SCH_CMD_STATUS_TLV)) ||
		    (!(ppdu_info->tlv_bitmap &
		       (1 << HTT_PPDU_STATS_USR_COMPLTN_ACK_BA_STATUS_TLV)) &&
		     (ppdu_desc->user[i].completion_status ==
		      HTT_PPDU_STATS_USER_STATUS_OK))) {
			dp_peer_unref_delete(peer, DP_MOD_ID_TX_PPDU_STATS);
			continue;
		}

		/*
		 * Update tx stats for data frames having Qos as well as
		 * non-Qos data tid
		 */

		if ((ppdu_desc->user[i].tid < CDP_DATA_TID_MAX ||
		     (ppdu_desc->user[i].tid == CDP_DATA_NON_QOS_TID) ||
		     (ppdu_desc->htt_frame_type ==
		      HTT_STATS_FTYPE_SGEN_QOS_NULL) ||
		     ((ppdu_desc->frame_type == CDP_PPDU_FTYPE_BAR) &&
		      (ppdu_desc->num_mpdu > 1))) &&
		      (ppdu_desc->frame_type != CDP_PPDU_FTYPE_CTRL)) {
			dp_tx_stats_update(pdev, peer,
					   &ppdu_desc->user[i],
					   ppdu_desc->ack_rssi);
		}

		dp_peer_unref_delete(peer, DP_MOD_ID_TX_PPDU_STATS);
		tlv_bitmap_expected = tlv_bitmap_default;
	}
}

#if !defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(QCA_MONITOR_2_0_SUPPORT)
/**
 * dp_tx_ppdu_desc_notify() - Notify to upper layer about PPDU via WDI
 *
 * @pdev: Datapath pdev handle
 * @nbuf: Buffer to be delivered to upper layer
 *
 * Return: void
 */
static void dp_tx_ppdu_desc_notify(struct dp_pdev *pdev, qdf_nbuf_t nbuf)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(soc);
	if (mon_ops && mon_ops->mon_ppdu_desc_notify)
		mon_ops->mon_ppdu_desc_notify(pdev, nbuf);
	else
		qdf_nbuf_free(nbuf);
}

void dp_ppdu_desc_deliver(struct dp_pdev *pdev,
			  struct ppdu_info *ppdu_info)
{
	struct ppdu_info *s_ppdu_info = NULL;
	struct ppdu_info *ppdu_info_next = NULL;
	struct cdp_tx_completion_ppdu *ppdu_desc = NULL;
	qdf_nbuf_t nbuf;
	uint32_t time_delta = 0;
	bool starved = 0;
	bool matched = 0;
	bool recv_ack_ba_done = 0;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if (ppdu_info->tlv_bitmap &
	    (1 << HTT_PPDU_STATS_USR_COMPLTN_ACK_BA_STATUS_TLV) &&
	    ppdu_info->done)
		recv_ack_ba_done = 1;

	mon_pdev->last_sched_cmdid = ppdu_info->sched_cmdid;

	s_ppdu_info = TAILQ_FIRST(&mon_pdev->sched_comp_ppdu_list);

	TAILQ_FOREACH_SAFE(s_ppdu_info, &mon_pdev->sched_comp_ppdu_list,
			   ppdu_info_list_elem, ppdu_info_next) {
		if (s_ppdu_info->tsf_l32 > ppdu_info->tsf_l32)
			time_delta = (MAX_TSF_32 - s_ppdu_info->tsf_l32) +
					ppdu_info->tsf_l32;
		else
			time_delta = ppdu_info->tsf_l32 - s_ppdu_info->tsf_l32;

		if (!s_ppdu_info->done && !recv_ack_ba_done) {
			if (time_delta < MAX_SCHED_STARVE) {
				dp_mon_info("pdev[%d] ppdu_id[%d] sched_cmdid[%d] TLV_B[0x%x] TSF[%u] D[%d]",
					    pdev->pdev_id,
					    s_ppdu_info->ppdu_id,
					    s_ppdu_info->sched_cmdid,
					    s_ppdu_info->tlv_bitmap,
					    s_ppdu_info->tsf_l32,
					    s_ppdu_info->done);
				break;
			}
			starved = 1;
		}

		mon_pdev->delivered_sched_cmdid = s_ppdu_info->sched_cmdid;
		TAILQ_REMOVE(&mon_pdev->sched_comp_ppdu_list, s_ppdu_info,
			     ppdu_info_list_elem);
		mon_pdev->sched_comp_list_depth--;

		nbuf = s_ppdu_info->nbuf;
		qdf_assert_always(nbuf);
		ppdu_desc = (struct cdp_tx_completion_ppdu *)
				qdf_nbuf_data(nbuf);
		ppdu_desc->tlv_bitmap = s_ppdu_info->tlv_bitmap;

		if (starved) {
			dp_mon_info("ppdu starved fc[0x%x] h_ftype[%d] tlv_bitmap[0x%x] cs[%d]\n",
				    ppdu_desc->frame_ctrl,
				    ppdu_desc->htt_frame_type,
				    ppdu_desc->tlv_bitmap,
				    ppdu_desc->user[0].completion_status);
			starved = 0;
		}

		if (ppdu_info->ppdu_id == s_ppdu_info->ppdu_id &&
		    ppdu_info->sched_cmdid == s_ppdu_info->sched_cmdid)
			matched = 1;

		dp_ppdu_desc_user_stats_update(pdev, s_ppdu_info);

		qdf_mem_free(s_ppdu_info);

		dp_tx_ppdu_desc_notify(pdev, nbuf);

		if (matched)
			break;
	}
}
#endif

/**
 * dp_tx_ppdu_desc_deliver() - Deliver PPDU desc to upper layer
 * @pdev: Datapath pdev handle
 * @ppdu_info: per PPDU TLV descriptor
 *
 * Return: void
 */
static void dp_tx_ppdu_desc_deliver(struct dp_pdev *pdev,
				    struct ppdu_info *ppdu_info)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(soc);

	if (mon_ops && mon_ops->mon_ppdu_desc_deliver) {
		mon_ops->mon_ppdu_desc_deliver(pdev, ppdu_info);
	} else {
		qdf_nbuf_free(ppdu_info->nbuf);
		ppdu_info->nbuf = NULL;
		qdf_mem_free(ppdu_info);
	}
}

/**
 * dp_get_ppdu_desc() - Function to allocate new PPDU status
 * desc for new ppdu id
 * @pdev: DP pdev handle
 * @ppdu_id: PPDU unique identifier
 * @tlv_type: TLV type received
 * @tsf_l32: timestamp received along with ppdu stats indication header
 * @max_users: Maximum user for that particular ppdu
 *
 * Return: ppdu_info per ppdu tlv structure
 */
static
struct ppdu_info *dp_get_ppdu_desc(struct dp_pdev *pdev, uint32_t ppdu_id,
				   uint8_t tlv_type, uint32_t tsf_l32,
				   uint8_t max_users)
{
	struct ppdu_info *ppdu_info = NULL;
	struct ppdu_info *s_ppdu_info = NULL;
	struct ppdu_info *ppdu_info_next = NULL;
	struct cdp_tx_completion_ppdu *ppdu_desc = NULL;
	uint32_t size = 0;
	struct cdp_tx_completion_ppdu *tmp_ppdu_desc = NULL;
	struct cdp_tx_completion_ppdu_user *tmp_user;
	uint32_t time_delta;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	/*
	 * Find ppdu_id node exists or not
	 */
	TAILQ_FOREACH_SAFE(ppdu_info, &mon_pdev->ppdu_info_list,
			   ppdu_info_list_elem, ppdu_info_next) {
		if (ppdu_info && (ppdu_info->ppdu_id == ppdu_id)) {
			if (ppdu_info->tsf_l32 > tsf_l32)
				time_delta  = (MAX_TSF_32 -
					       ppdu_info->tsf_l32) + tsf_l32;
			else
				time_delta  = tsf_l32 - ppdu_info->tsf_l32;

			if (time_delta > WRAP_DROP_TSF_DELTA) {
				TAILQ_REMOVE(&mon_pdev->ppdu_info_list,
					     ppdu_info, ppdu_info_list_elem);
				mon_pdev->list_depth--;
				pdev->stats.ppdu_wrap_drop++;
				tmp_ppdu_desc =
					(struct cdp_tx_completion_ppdu *)
					qdf_nbuf_data(ppdu_info->nbuf);
				tmp_user = &tmp_ppdu_desc->user[0];
				dp_htt_tx_stats_info("S_PID [%d] S_TSF[%u] TLV_BITMAP[0x%x] [CMPLTN - %d ACK_BA - %d] CS[%d] - R_PID[%d] R_TSF[%u] R_TLV_TAG[0x%x]\n",
						     ppdu_info->ppdu_id,
						     ppdu_info->tsf_l32,
						     ppdu_info->tlv_bitmap,
						     tmp_user->completion_status,
						     ppdu_info->compltn_common_tlv,
						     ppdu_info->ack_ba_tlv,
						     ppdu_id, tsf_l32,
						     tlv_type);
				qdf_nbuf_free(ppdu_info->nbuf);
				ppdu_info->nbuf = NULL;
				qdf_mem_free(ppdu_info);
			} else {
				break;
			}
		}
	}

	/*
	 * check if it is ack ba tlv and if it is not there in ppdu info
	 * list then check it in sched completion ppdu list
	 */
	if (!ppdu_info &&
	    tlv_type == HTT_PPDU_STATS_USR_COMPLTN_ACK_BA_STATUS_TLV) {
		TAILQ_FOREACH(s_ppdu_info,
			      &mon_pdev->sched_comp_ppdu_list,
			      ppdu_info_list_elem) {
			if (s_ppdu_info && (s_ppdu_info->ppdu_id == ppdu_id)) {
				if (s_ppdu_info->tsf_l32 > tsf_l32)
					time_delta  = (MAX_TSF_32 -
						       s_ppdu_info->tsf_l32) +
							tsf_l32;
				else
					time_delta  = tsf_l32 -
						s_ppdu_info->tsf_l32;
				if (time_delta < WRAP_DROP_TSF_DELTA) {
					ppdu_info = s_ppdu_info;
					break;
				}
			} else {
				/*
				 * ACK BA STATUS TLV comes sequential order
				 * if we received ack ba status tlv for second
				 * ppdu and first ppdu is still waiting for
				 * ACK BA STATUS TLV. Based on fw comment
				 * we won't receive it tlv later. So we can
				 * set ppdu info done.
				 */
				if (s_ppdu_info)
					s_ppdu_info->done = 1;
			}
		}
	}

	if (ppdu_info) {
		if (ppdu_info->tlv_bitmap & (1 << tlv_type)) {
			/*
			 * if we get tlv_type that is already been processed
			 * for ppdu, that means we got a new ppdu with same
			 * ppdu id. Hence Flush the older ppdu
			 * for MUMIMO and OFDMA, In a PPDU we have
			 * multiple user with same tlv types. tlv bitmap is
			 * used to check whether SU or MU_MIMO/OFDMA
			 */
			if (!(ppdu_info->tlv_bitmap &
			    (1 << HTT_PPDU_STATS_SCH_CMD_STATUS_TLV)))
				return ppdu_info;

			ppdu_desc = (struct cdp_tx_completion_ppdu *)
				qdf_nbuf_data(ppdu_info->nbuf);

			/*
			 * apart from ACK BA STATUS TLV rest all comes in order
			 * so if tlv type not ACK BA STATUS TLV we can deliver
			 * ppdu_info
			 */
			if ((tlv_type ==
			     HTT_PPDU_STATS_USR_COMPLTN_ACK_BA_STATUS_TLV) &&
			    ((ppdu_desc->htt_frame_type ==
			     HTT_STATS_FTYPE_SGEN_MU_BAR) ||
			    (ppdu_desc->htt_frame_type ==
			     HTT_STATS_FTYPE_SGEN_BE_MU_BAR)))
				return ppdu_info;

			dp_tx_ppdu_desc_deliver(pdev, ppdu_info);
		} else {
			return ppdu_info;
		}
	}

	/*
	 * Flush the head ppdu descriptor if ppdu desc list reaches max
	 * threshold
	 */
	if (mon_pdev->list_depth > HTT_PPDU_DESC_MAX_DEPTH) {
		ppdu_info = TAILQ_FIRST(&mon_pdev->ppdu_info_list);
		TAILQ_REMOVE(&mon_pdev->ppdu_info_list,
			     ppdu_info, ppdu_info_list_elem);
		mon_pdev->list_depth--;
		pdev->stats.ppdu_drop++;
		qdf_nbuf_free(ppdu_info->nbuf);
		ppdu_info->nbuf = NULL;
		qdf_mem_free(ppdu_info);
	}

	size = sizeof(struct cdp_tx_completion_ppdu) +
		(max_users * sizeof(struct cdp_tx_completion_ppdu_user));

	/*
	 * Allocate new ppdu_info node
	 */
	ppdu_info = qdf_mem_malloc(sizeof(struct ppdu_info));
	if (!ppdu_info)
		return NULL;

	ppdu_info->nbuf = qdf_nbuf_alloc(pdev->soc->osdev, size,
					 0, 4, TRUE);
	if (!ppdu_info->nbuf) {
		qdf_mem_free(ppdu_info);
		return NULL;
	}

	ppdu_info->ppdu_desc =
		(struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_info->nbuf);
	qdf_mem_zero(qdf_nbuf_data(ppdu_info->nbuf), size);

	if (!qdf_nbuf_put_tail(ppdu_info->nbuf, size)) {
		dp_mon_err("No tailroom for HTT PPDU");
		qdf_nbuf_free(ppdu_info->nbuf);
		ppdu_info->nbuf = NULL;
		ppdu_info->last_user = 0;
		qdf_mem_free(ppdu_info);
		return NULL;
	}

	ppdu_info->ppdu_desc->max_users = max_users;
	ppdu_info->tsf_l32 = tsf_l32;
	/*
	 * No lock is needed because all PPDU TLVs are processed in
	 * same context and this list is updated in same context
	 */
	TAILQ_INSERT_TAIL(&mon_pdev->ppdu_info_list, ppdu_info,
			  ppdu_info_list_elem);
	mon_pdev->list_depth++;
	return ppdu_info;
}

/**
 * dp_htt_process_tlv() - Function to process each PPDU TLVs
 * @pdev: DP pdev handle
 * @htt_t2h_msg: HTT target to host message
 *
 * Return: ppdu_info per ppdu tlv structure
 */
static struct ppdu_info *dp_htt_process_tlv(struct dp_pdev *pdev,
					    qdf_nbuf_t htt_t2h_msg)
{
	uint32_t length;
	uint32_t ppdu_id;
	uint8_t tlv_type;
	uint32_t tlv_length, tlv_bitmap_expected;
	uint8_t *tlv_buf;
	struct ppdu_info *ppdu_info = NULL;
	struct cdp_tx_completion_ppdu *ppdu_desc = NULL;
	uint8_t max_users = CDP_MU_MAX_USERS;
	uint32_t tsf_l32;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	uint32_t *msg_word = (uint32_t *)qdf_nbuf_data(htt_t2h_msg);

	length = HTT_T2H_PPDU_STATS_PAYLOAD_SIZE_GET(*msg_word);

	msg_word = msg_word + 1;
	ppdu_id = HTT_T2H_PPDU_STATS_PPDU_ID_GET(*msg_word);

	msg_word = msg_word + 1;
	tsf_l32 = (uint32_t)(*msg_word);

	msg_word = msg_word + 2;
	while (length > 0) {
		tlv_buf = (uint8_t *)msg_word;
		tlv_type = HTT_STATS_TLV_TAG_GET(*msg_word);
		tlv_length = HTT_STATS_TLV_LENGTH_GET(*msg_word);
		if (qdf_likely(tlv_type < CDP_PPDU_STATS_MAX_TAG))
			pdev->stats.ppdu_stats_counter[tlv_type]++;

		if (tlv_length == 0)
			break;

		tlv_length += HTT_TLV_HDR_LEN;

		/*
		 * Not allocating separate ppdu descriptor for MGMT Payload
		 * TLV as this is sent as separate WDI indication and it
		 * doesn't contain any ppdu information
		 */
		if (tlv_type == HTT_PPDU_STATS_TX_MGMTCTRL_PAYLOAD_TLV) {
			mon_pdev->mgmtctrl_frm_info.mgmt_buf = tlv_buf;
			mon_pdev->mgmtctrl_frm_info.ppdu_id = ppdu_id;
			mon_pdev->mgmtctrl_frm_info.mgmt_buf_len =
				HTT_PPDU_STATS_TX_MGMTCTRL_TLV_FRAME_LENGTH_GET
						(*(msg_word + 1));
			msg_word =
				(uint32_t *)((uint8_t *)tlv_buf + tlv_length);
			length -= (tlv_length);
			continue;
		}

		/*
		 * retrieve max_users if it's USERS_INFO,
		 * else, it's 1 for COMPLTN_FLUSH,
		 * else, use CDP_MU_MAX_USERS
		 */
		if (tlv_type == HTT_PPDU_STATS_USERS_INFO_TLV) {
			max_users =
				HTT_PPDU_STATS_USERS_INFO_TLV_MAX_USERS_GET(*(msg_word + 1));
		} else if (tlv_type == HTT_PPDU_STATS_USR_COMPLTN_FLUSH_TLV) {
			max_users = 1;
		}

		ppdu_info = dp_get_ppdu_desc(pdev, ppdu_id, tlv_type,
					     tsf_l32, max_users);
		if (!ppdu_info)
			return NULL;

		ppdu_info->ppdu_id = ppdu_id;
		ppdu_info->tlv_bitmap |= (1 << tlv_type);

		dp_process_ppdu_tag(pdev, msg_word, tlv_length, ppdu_info);

		/*
		 * Increment pdev level tlv count to monitor
		 * missing TLVs
		 */
		mon_pdev->tlv_count++;
		ppdu_info->last_tlv_cnt = mon_pdev->tlv_count;
		msg_word = (uint32_t *)((uint8_t *)tlv_buf + tlv_length);
		length -= (tlv_length);
	}

	if (!ppdu_info)
		return NULL;

	mon_pdev->last_ppdu_id = ppdu_id;

	tlv_bitmap_expected = HTT_PPDU_DEFAULT_TLV_BITMAP;

	if (mon_pdev->tx_sniffer_enable || mon_pdev->mcopy_mode ||
	    mon_pdev->tx_capture_enabled) {
		if (ppdu_info->is_ampdu)
			tlv_bitmap_expected =
				dp_htt_get_ppdu_sniffer_ampdu_tlv_bitmap(
					ppdu_info->tlv_bitmap);
	}

	ppdu_desc = ppdu_info->ppdu_desc;

	if (!ppdu_desc)
		return NULL;

	if (ppdu_desc->user[ppdu_desc->last_usr_index].completion_status !=
	    HTT_PPDU_STATS_USER_STATUS_OK) {
		tlv_bitmap_expected = tlv_bitmap_expected & 0xFF;
	}

	/*
	 * for frame type DATA and BAR, we update stats based on MSDU,
	 * successful msdu and mpdu are populate from ACK BA STATUS TLV
	 * which comes out of order. successful mpdu also populated from
	 * COMPLTN COMMON TLV which comes in order. for every ppdu_info
	 * we store successful mpdu from both tlv and compare before delivering
	 * to make sure we received ACK BA STATUS TLV. For some self generated
	 * frame we won't get ack ba status tlv so no need to wait for
	 * ack ba status tlv.
	 */
	if (ppdu_desc->frame_type != CDP_PPDU_FTYPE_CTRL &&
	    ppdu_desc->htt_frame_type != HTT_STATS_FTYPE_SGEN_QOS_NULL) {
		/*
		 * most of the time bar frame will have duplicate ack ba
		 * status tlv
		 */
		if (ppdu_desc->frame_type == CDP_PPDU_FTYPE_BAR &&
		    (ppdu_info->compltn_common_tlv != ppdu_info->ack_ba_tlv))
			return NULL;
		/*
		 * For data frame, compltn common tlv should match ack ba status
		 * tlv and completion status. Reason we are checking first user
		 * for ofdma, completion seen at next MU BAR frm, for mimo
		 * only for first user completion will be immediate.
		 */
		if (ppdu_desc->frame_type == CDP_PPDU_FTYPE_DATA &&
		    (ppdu_desc->user[0].completion_status == 0 &&
		     (ppdu_info->compltn_common_tlv != ppdu_info->ack_ba_tlv)))
			return NULL;
	}

	/*
	 * Once all the TLVs for a given PPDU has been processed,
	 * return PPDU status to be delivered to higher layer.
	 * tlv_bitmap_expected can't be available for different frame type.
	 * But SCHED CMD STATS TLV is the last TLV from the FW for a ppdu.
	 * apart from ACK BA TLV, FW sends other TLV in sequential order.
	 * flush tlv comes separate.
	 */
	if ((ppdu_info->tlv_bitmap != 0 &&
	     (ppdu_info->tlv_bitmap &
	      (1 << HTT_PPDU_STATS_SCH_CMD_STATUS_TLV))) ||
	    (ppdu_info->tlv_bitmap &
	     (1 << HTT_PPDU_STATS_USR_COMPLTN_FLUSH_TLV))) {
		ppdu_info->done = 1;
		return ppdu_info;
	}

	return NULL;
}
#endif /* QCA_ENHANCED_STATS_SUPPORT */

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_tx_ppdu_stats_feat_enable_check() - Check if feature(s) is enabled to
 *			consume stats received from FW via HTT
 * @pdev: Datapath pdev handle
 *
 * Return: void
 */
static bool dp_tx_ppdu_stats_feat_enable_check(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(soc);
	if (mon_ops && mon_ops->mon_ppdu_stats_feat_enable_check)
		return mon_ops->mon_ppdu_stats_feat_enable_check(pdev);
	else
		return false;
}
#endif

#if defined(WDI_EVENT_ENABLE)
#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_txrx_ppdu_stats_handler() - Function to process HTT PPDU stats from FW
 * @soc: DP SOC handle
 * @pdev_id: pdev id
 * @htt_t2h_msg: HTT message nbuf
 *
 * Return: void
 */
static bool dp_txrx_ppdu_stats_handler(struct dp_soc *soc,
				       uint8_t pdev_id, qdf_nbuf_t htt_t2h_msg)
{
	struct dp_pdev *pdev;
	struct ppdu_info *ppdu_info = NULL;
	bool free_buf = true;
	struct dp_mon_pdev *mon_pdev;

	if (pdev_id >= MAX_PDEV_CNT)
		return true;

	pdev = soc->pdev_list[pdev_id];
	if (!pdev)
		return true;

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev)
		return true;

	if (!dp_tx_ppdu_stats_feat_enable_check(pdev))
		return free_buf;

	qdf_spin_lock_bh(&mon_pdev->ppdu_stats_lock);
	ppdu_info = dp_htt_process_tlv(pdev, htt_t2h_msg);

	if (mon_pdev->mgmtctrl_frm_info.mgmt_buf) {
		if (dp_process_ppdu_stats_tx_mgmtctrl_payload_tlv
		    (pdev, htt_t2h_msg, mon_pdev->mgmtctrl_frm_info.ppdu_id) !=
		    QDF_STATUS_SUCCESS)
			free_buf = false;
	}

	if (ppdu_info)
		dp_tx_ppdu_desc_deliver(pdev, ppdu_info);

	mon_pdev->mgmtctrl_frm_info.mgmt_buf = NULL;
	mon_pdev->mgmtctrl_frm_info.mgmt_buf_len = 0;
	mon_pdev->mgmtctrl_frm_info.ppdu_id = 0;

	qdf_spin_unlock_bh(&mon_pdev->ppdu_stats_lock);

	return free_buf;
}
#elif (!defined(REMOVE_PKT_LOG))
static bool dp_txrx_ppdu_stats_handler(struct dp_soc *soc,
				       uint8_t pdev_id, qdf_nbuf_t htt_t2h_msg)
{
	return true;
}
#endif/* QCA_ENHANCED_STATS_SUPPORT */
#endif

#if defined(WDI_EVENT_ENABLE) &&\
	(defined(QCA_ENHANCED_STATS_SUPPORT) || !defined(REMOVE_PKT_LOG))
bool
dp_ppdu_stats_ind_handler(struct htt_soc *soc,
			  uint32_t *msg_word,
			  qdf_nbuf_t htt_t2h_msg)
{
	u_int8_t pdev_id;
	u_int8_t target_pdev_id;
	bool free_buf;

	target_pdev_id = HTT_T2H_PPDU_STATS_PDEV_ID_GET(*msg_word);
	pdev_id = dp_get_host_pdev_id_for_target_pdev_id(soc->dp_soc,
							 target_pdev_id);
	dp_wdi_event_handler(WDI_EVENT_LITE_T2H, soc->dp_soc,
			     htt_t2h_msg, HTT_INVALID_PEER, WDI_NO_VAL,
			     pdev_id);

	free_buf = dp_txrx_ppdu_stats_handler(soc->dp_soc, pdev_id,
					      htt_t2h_msg);

	return free_buf;
}
#endif

void
dp_mon_set_bsscolor(struct dp_pdev *pdev, uint8_t bsscolor)
{
	pdev->monitor_pdev->rx_mon_recv_status.bsscolor = bsscolor;
}

bool dp_pdev_get_filter_ucast_data(struct cdp_pdev *pdev_handle)
{
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if ((mon_pdev->fp_data_filter & FILTER_DATA_UCAST) ||
	    (mon_pdev->mo_data_filter & FILTER_DATA_UCAST))
		return true;

	return false;
}

bool dp_pdev_get_filter_mcast_data(struct cdp_pdev *pdev_handle)
{
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if ((mon_pdev->fp_data_filter & FILTER_DATA_MCAST) ||
	    (mon_pdev->mo_data_filter & FILTER_DATA_MCAST))
		return true;

	return false;
}

bool dp_pdev_get_filter_non_data(struct cdp_pdev *pdev_handle)
{
	struct dp_pdev *pdev = (struct dp_pdev *)pdev_handle;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	if ((mon_pdev->fp_mgmt_filter & FILTER_MGMT_ALL) ||
	    (mon_pdev->mo_mgmt_filter & FILTER_MGMT_ALL)) {
		if ((mon_pdev->fp_ctrl_filter & FILTER_CTRL_ALL) ||
		    (mon_pdev->mo_ctrl_filter & FILTER_CTRL_ALL)) {
			return true;
		}
	}

	return false;
}

QDF_STATUS dp_mon_soc_cfg_init(struct dp_soc *soc)
{
	int target_type;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct cdp_mon_ops *cdp_ops;

	cdp_ops = dp_mon_cdp_ops_get(soc);
	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_QCA6290:
	case TARGET_TYPE_QCA6390:
	case TARGET_TYPE_QCA6490:
	case TARGET_TYPE_QCA6750:
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
		/* do nothing */
		break;
	case TARGET_TYPE_QCA8074:
		wlan_cfg_set_mon_delayed_replenish_entries(soc->wlan_cfg_ctx,
							   MON_BUF_MIN_ENTRIES);
		break;
	case TARGET_TYPE_QCA8074V2:
	case TARGET_TYPE_QCA6018:
	case TARGET_TYPE_QCA9574:
		wlan_cfg_set_mon_delayed_replenish_entries(soc->wlan_cfg_ctx,
							   MON_BUF_MIN_ENTRIES);
		mon_soc->hw_nac_monitor_support = 1;
		break;
	case TARGET_TYPE_QCN9000:
		wlan_cfg_set_mon_delayed_replenish_entries(soc->wlan_cfg_ctx,
							   MON_BUF_MIN_ENTRIES);
		mon_soc->hw_nac_monitor_support = 1;
		if (cfg_get(soc->ctrl_psoc, CFG_DP_FULL_MON_MODE)) {
			if (cdp_ops  && cdp_ops->config_full_mon_mode)
				cdp_ops->config_full_mon_mode((struct cdp_soc_t *)soc, 1);
		}
		break;
	case TARGET_TYPE_QCA5018:
	case TARGET_TYPE_QCN6122:
	case TARGET_TYPE_QCN9160:
		wlan_cfg_set_mon_delayed_replenish_entries(soc->wlan_cfg_ctx,
							   MON_BUF_MIN_ENTRIES);
		mon_soc->hw_nac_monitor_support = 1;
		break;
	case TARGET_TYPE_QCN9224:
	case TARGET_TYPE_QCA5332:
		wlan_cfg_set_mon_delayed_replenish_entries(soc->wlan_cfg_ctx,
							   MON_BUF_MIN_ENTRIES);
		mon_soc->hw_nac_monitor_support = 1;
		mon_soc->monitor_mode_v2 = 1;
		break;
	default:
		dp_mon_info("%s: Unknown tgt type %d\n", __func__, target_type);
		qdf_assert_always(0);
		break;
	}

	dp_mon_info("hw_nac_monitor_support = %d",
		    mon_soc->hw_nac_monitor_support);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_mon_pdev_per_target_config() - Target specific monitor pdev configuration
 * @pdev: PDEV handle [Should be valid]
 *
 * Return: None
 */
static void dp_mon_pdev_per_target_config(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	int target_type;

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
		mon_pdev->is_tlv_hdr_64_bit = true;
		break;
	default:
		mon_pdev->is_tlv_hdr_64_bit = false;
		break;
	}
}

QDF_STATUS dp_mon_pdev_attach(struct dp_pdev *pdev)
{
	struct dp_soc *soc;
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_ops *mon_ops;
	qdf_size_t mon_pdev_context_size;

	if (!pdev) {
		dp_mon_err("pdev is NULL");
		goto fail0;
	}

	soc = pdev->soc;

	mon_pdev_context_size = soc->arch_ops.txrx_get_mon_context_size(DP_CONTEXT_TYPE_MON_PDEV);
	mon_pdev = dp_context_alloc_mem(soc, DP_MON_PDEV_TYPE, mon_pdev_context_size);
	if (!mon_pdev) {
		dp_mon_err("%pK: MONITOR pdev allocation failed", pdev);
		goto fail0;
	}

	pdev->monitor_pdev = mon_pdev;
	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!mon_ops) {
		dp_mon_err("%pK: Invalid monitor ops", pdev);
		goto fail1;
	}

	if (mon_ops->mon_pdev_alloc) {
		if (mon_ops->mon_pdev_alloc(pdev)) {
			dp_mon_err("%pK: MONITOR pdev alloc failed", pdev);
			goto fail1;
		}
	}

	if (mon_ops->mon_rings_alloc) {
		if (mon_ops->mon_rings_alloc(pdev)) {
			dp_mon_err("%pK: MONITOR rings setup failed", pdev);
			goto fail2;
		}
	}

	/* Rx monitor mode specific init */
	if (mon_ops->rx_mon_desc_pool_alloc) {
		if (mon_ops->rx_mon_desc_pool_alloc(pdev)) {
			dp_mon_err("%pK: dp_rx_pdev_mon_attach failed", pdev);
			goto fail3;
		}
	}

	if (mon_ops->mon_rx_ppdu_info_cache_create) {
		if (mon_ops->mon_rx_ppdu_info_cache_create(pdev)) {
			dp_mon_err("%pK: dp_rx_pdev_mon_attach failed", pdev);
			goto fail4;
		}
	}
	pdev->monitor_pdev = mon_pdev;
	dp_mon_pdev_per_target_config(pdev);

	return QDF_STATUS_SUCCESS;
fail4:
	if (mon_ops->rx_mon_desc_pool_free)
		mon_ops->rx_mon_desc_pool_free(pdev);
fail3:
	if (mon_ops->mon_rings_free)
		mon_ops->mon_rings_free(pdev);
fail2:
	if (mon_ops->mon_pdev_free)
		mon_ops->mon_pdev_free(pdev);
fail1:
	pdev->monitor_pdev = NULL;
	dp_context_free_mem(soc, DP_MON_PDEV_TYPE, mon_pdev);
fail0:
	return QDF_STATUS_E_NOMEM;
}

QDF_STATUS dp_mon_pdev_detach(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_ops *mon_ops = NULL;

	if (!pdev) {
		dp_mon_err("pdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_err("Monitor pdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!mon_ops) {
		dp_mon_err("Monitor ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (mon_ops->mon_rx_ppdu_info_cache_destroy)
		mon_ops->mon_rx_ppdu_info_cache_destroy(pdev);
	if (mon_ops->rx_mon_desc_pool_free)
		mon_ops->rx_mon_desc_pool_free(pdev);
	if (mon_ops->mon_rings_free)
		mon_ops->mon_rings_free(pdev);
	if (mon_ops->mon_pdev_free)
		mon_ops->mon_pdev_free(pdev);

	dp_context_free_mem(pdev->soc, DP_MON_PDEV_TYPE, mon_pdev);
	pdev->monitor_pdev = NULL;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_mon_pdev_init(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev;
	struct dp_mon_ops *mon_ops = NULL;

	if (!pdev) {
		dp_mon_err("pdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mon_pdev = pdev->monitor_pdev;

	mon_pdev->invalid_mon_peer = qdf_mem_malloc(sizeof(struct dp_mon_peer));
	if (!mon_pdev->invalid_mon_peer) {
		dp_mon_err("%pK: Memory allocation failed for invalid "
			   "monitor peer", pdev);
		return QDF_STATUS_E_NOMEM;
	}

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!mon_ops) {
		dp_mon_err("Monitor ops is NULL");
		goto fail0;
	}

	mon_pdev->filter = dp_mon_filter_alloc(mon_pdev);
	if (!mon_pdev->filter) {
		dp_mon_err("%pK: Memory allocation failed for monitor filter",
			   pdev);
		goto fail0;
	}

	if (mon_ops->tx_mon_filter_alloc) {
		if (mon_ops->tx_mon_filter_alloc(pdev)) {
			dp_mon_err("%pK: Memory allocation failed for tx monitor "
				   "filter", pdev);
			goto fail1;
		}
	}

	qdf_spinlock_create(&mon_pdev->ppdu_stats_lock);
	qdf_spinlock_create(&mon_pdev->neighbour_peer_mutex);
	mon_pdev->monitor_configured = false;
	mon_pdev->mon_chan_band = REG_BAND_UNKNOWN;

	TAILQ_INIT(&mon_pdev->neighbour_peers_list);
	mon_pdev->neighbour_peers_added = false;
	mon_pdev->monitor_configured = false;
	/* Monitor filter init */
	mon_pdev->mon_filter_mode = MON_FILTER_ALL;
	mon_pdev->fp_mgmt_filter = FILTER_MGMT_ALL;
	mon_pdev->fp_ctrl_filter = FILTER_CTRL_ALL;
	mon_pdev->fp_data_filter = FILTER_DATA_ALL;
	mon_pdev->mo_mgmt_filter = FILTER_MGMT_ALL;
	mon_pdev->mo_ctrl_filter = FILTER_CTRL_ALL;
	mon_pdev->mo_data_filter = FILTER_DATA_ALL;

	/*
	 * initialize ppdu tlv list
	 */
	TAILQ_INIT(&mon_pdev->ppdu_info_list);
	TAILQ_INIT(&mon_pdev->sched_comp_ppdu_list);

	mon_pdev->list_depth = 0;
	mon_pdev->tlv_count = 0;
	/* initlialize cal client timer */
	dp_cal_client_attach(&mon_pdev->cal_client_ctx,
			     dp_pdev_to_cdp_pdev(pdev),
			     pdev->soc->osdev,
			     &dp_iterate_update_peer_list);
	if (dp_htt_ppdu_stats_attach(pdev) != QDF_STATUS_SUCCESS)
		goto fail2;

	if (mon_ops->mon_lite_mon_alloc) {
		if (mon_ops->mon_lite_mon_alloc(pdev) != QDF_STATUS_SUCCESS) {
			dp_mon_err("%pK: lite mon alloc failed", pdev);
			goto fail3;
		}
	}

	if (mon_ops->mon_rings_init) {
		if (mon_ops->mon_rings_init(pdev)) {
			dp_mon_err("%pK: MONITOR rings setup failed", pdev);
			goto fail4;
		}
	}

	/* initialize sw monitor rx descriptors */
	if (mon_ops->rx_mon_desc_pool_init)
		mon_ops->rx_mon_desc_pool_init(pdev);

	/* allocate buffers and replenish the monitor RxDMA ring */
	if (mon_ops->rx_mon_buffers_alloc) {
		if (mon_ops->rx_mon_buffers_alloc(pdev)) {
			dp_mon_err("%pK: rx mon buffers alloc failed", pdev);
			goto fail5;
		}
	}

	/* attach monitor function */
	dp_monitor_tx_ppdu_stats_attach(pdev);

	/* mon pdev extended init */
	if (mon_ops->mon_pdev_ext_init)
		mon_ops->mon_pdev_ext_init(pdev);

	mon_pdev->is_dp_mon_pdev_initialized = true;

	return QDF_STATUS_SUCCESS;

fail5:
	if (mon_ops->rx_mon_desc_pool_deinit)
		mon_ops->rx_mon_desc_pool_deinit(pdev);

	if (mon_ops->mon_rings_deinit)
		mon_ops->mon_rings_deinit(pdev);
fail4:
	if (mon_ops->mon_lite_mon_dealloc)
		mon_ops->mon_lite_mon_dealloc(pdev);
fail3:
	dp_htt_ppdu_stats_detach(pdev);
fail2:
	qdf_spinlock_destroy(&mon_pdev->neighbour_peer_mutex);
	qdf_spinlock_destroy(&mon_pdev->ppdu_stats_lock);
	if (mon_ops->tx_mon_filter_dealloc)
		mon_ops->tx_mon_filter_dealloc(pdev);
fail1:
	dp_mon_filter_dealloc(mon_pdev);
fail0:
	qdf_mem_free(mon_pdev->invalid_mon_peer);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS dp_mon_pdev_deinit(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(pdev->soc);
	if (!mon_ops) {
		dp_mon_err("Monitor ops is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!mon_pdev->is_dp_mon_pdev_initialized)
		return QDF_STATUS_SUCCESS;

	dp_mon_filters_reset(pdev);

	/* mon pdev extended deinit */
	if (mon_ops->mon_pdev_ext_deinit)
		mon_ops->mon_pdev_ext_deinit(pdev);

	/* detach monitor function */
	dp_monitor_tx_ppdu_stats_detach(pdev);

	if (mon_ops->rx_mon_buffers_free)
		mon_ops->rx_mon_buffers_free(pdev);
	if (mon_ops->rx_mon_desc_pool_deinit)
		mon_ops->rx_mon_desc_pool_deinit(pdev);
	if (mon_ops->mon_rings_deinit)
		mon_ops->mon_rings_deinit(pdev);
	dp_cal_client_detach(&mon_pdev->cal_client_ctx);
	if (mon_ops->mon_lite_mon_dealloc)
		mon_ops->mon_lite_mon_dealloc(pdev);
	dp_htt_ppdu_stats_detach(pdev);
	qdf_spinlock_destroy(&mon_pdev->ppdu_stats_lock);
	dp_neighbour_peers_detach(pdev);
	dp_pktlogmod_exit(pdev);
	if (mon_ops->tx_mon_filter_dealloc)
		mon_ops->tx_mon_filter_dealloc(pdev);
	if (mon_pdev->filter)
		dp_mon_filter_dealloc(mon_pdev);
	if (mon_ops->mon_rings_deinit)
		mon_ops->mon_rings_deinit(pdev);
	if (mon_pdev->invalid_mon_peer)
		qdf_mem_free(mon_pdev->invalid_mon_peer);
	mon_pdev->is_dp_mon_pdev_initialized = false;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_mon_vdev_attach(struct dp_vdev *vdev)
{
	struct dp_mon_vdev *mon_vdev;
	struct dp_pdev *pdev = vdev->pdev;

	mon_vdev = (struct dp_mon_vdev *)qdf_mem_malloc(sizeof(*mon_vdev));
	if (!mon_vdev) {
		dp_mon_err("%pK: Monitor vdev allocation failed", vdev);
		return QDF_STATUS_E_NOMEM;
	}

	if (pdev && pdev->monitor_pdev &&
	    pdev->monitor_pdev->scan_spcl_vap_configured)
		dp_scan_spcl_vap_stats_attach(mon_vdev);

	vdev->monitor_vdev = mon_vdev;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_mon_vdev_detach(struct dp_vdev *vdev)
{
	struct dp_mon_vdev *mon_vdev = vdev->monitor_vdev;
	struct dp_pdev *pdev = vdev->pdev;
	struct dp_mon_ops *mon_ops = dp_mon_ops_get(pdev->soc);

	if (!mon_ops)
		return QDF_STATUS_E_FAILURE;

	if (!mon_vdev)
		return QDF_STATUS_E_FAILURE;

	if (pdev->monitor_pdev->scan_spcl_vap_configured)
		dp_scan_spcl_vap_stats_detach(mon_vdev);

	qdf_mem_free(mon_vdev);
	vdev->monitor_vdev = NULL;
	/* set mvdev to NULL only if detach is called for monitor/special vap
	 */
	if (pdev->monitor_pdev->mvdev == vdev)
		pdev->monitor_pdev->mvdev = NULL;

	if (mon_ops->mon_lite_mon_vdev_delete)
		mon_ops->mon_lite_mon_vdev_delete(pdev, vdev);

	return QDF_STATUS_SUCCESS;
}

#if defined(FEATURE_PERPKT_INFO) && WDI_EVENT_ENABLE
/**
 * dp_mon_peer_attach_notify() - Raise WDI event for peer create
 * @peer: DP Peer handle
 *
 * Return: none
 */
static inline
void dp_mon_peer_attach_notify(struct dp_peer *peer)
{
	struct dp_mon_peer *mon_peer = peer->monitor_peer;
	struct dp_pdev *pdev;
	struct dp_soc *soc;
	struct cdp_peer_cookie peer_cookie;

	pdev = peer->vdev->pdev;
	soc = pdev->soc;

	qdf_mem_copy(peer_cookie.mac_addr, peer->mac_addr.raw,
		     QDF_MAC_ADDR_SIZE);

	peer_cookie.ctx = NULL;
	peer_cookie.pdev_id = pdev->pdev_id;
	peer_cookie.cookie = pdev->next_peer_cookie++;

	dp_wdi_event_handler(WDI_EVENT_PEER_CREATE, soc,
			     (void *)&peer_cookie,
			     peer->peer_id, WDI_NO_VAL, pdev->pdev_id);

	if (soc->peerstats_enabled) {
		if (!peer_cookie.ctx) {
			pdev->next_peer_cookie--;
			qdf_err("Failed to initialize peer rate stats");
			mon_peer->peerstats_ctx = NULL;
		} else {
			mon_peer->peerstats_ctx =
				(struct cdp_peer_rate_stats_ctx *)
				 peer_cookie.ctx;
		}
	}
}

/**
 * dp_mon_peer_detach_notify() - Raise WDI event for peer destroy
 * @peer: DP Peer handle
 *
 * Return: none
 */
static inline
void dp_mon_peer_detach_notify(struct dp_peer *peer)
{
	struct dp_mon_peer *mon_peer = peer->monitor_peer;
	struct dp_pdev *pdev;
	struct dp_soc *soc;
	struct cdp_peer_cookie peer_cookie;

	pdev = peer->vdev->pdev;
	soc = pdev->soc;
	/* send peer destroy event to upper layer */
	qdf_mem_copy(peer_cookie.mac_addr, peer->mac_addr.raw,
		     QDF_MAC_ADDR_SIZE);
	peer_cookie.ctx = NULL;
	peer_cookie.ctx = (struct cdp_stats_cookie *)mon_peer->peerstats_ctx;

	dp_wdi_event_handler(WDI_EVENT_PEER_DESTROY,
			     soc,
			     (void *)&peer_cookie,
			     peer->peer_id,
			     WDI_NO_VAL,
			     pdev->pdev_id);

	mon_peer->peerstats_ctx = NULL;
}
#else
static inline
void dp_mon_peer_attach_notify(struct dp_peer *peer)
{
	peer->monitor_peer->peerstats_ctx = NULL;
}

static inline
void dp_mon_peer_detach_notify(struct dp_peer *peer)
{
	peer->monitor_peer->peerstats_ctx = NULL;
}
#endif

#if defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(FEATURE_PERPKT_INFO)
QDF_STATUS dp_mon_peer_attach(struct dp_peer *peer)
{
	struct dp_mon_peer *mon_peer;
	struct dp_pdev *pdev;

	mon_peer = (struct dp_mon_peer *)qdf_mem_malloc(sizeof(*mon_peer));
	if (!mon_peer) {
		dp_mon_err("%pK: MONITOR peer allocation failed", peer);
		return QDF_STATUS_E_NOMEM;
	}

	peer->monitor_peer = mon_peer;
	pdev = peer->vdev->pdev;
	/*
	 * In tx_monitor mode, filter may be set for unassociated peer
	 * when unassociated peer get associated peer need to
	 * update tx_cap_enabled flag to support peer filter.
	 */
	dp_monitor_peer_tx_capture_filter_check(pdev, peer);

	DP_STATS_INIT(mon_peer);
	DP_STATS_UPD(mon_peer, rx.avg_snr, CDP_INVALID_SNR);

	dp_mon_peer_attach_notify(peer);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS dp_mon_peer_detach(struct dp_peer *peer)
{
	struct dp_mon_peer *mon_peer = peer->monitor_peer;

	if (!mon_peer)
		return QDF_STATUS_SUCCESS;

	dp_mon_peer_detach_notify(peer);

	qdf_mem_free(mon_peer);
	peer->monitor_peer = NULL;

	return QDF_STATUS_SUCCESS;
}

#ifndef DISABLE_MON_CONFIG
void dp_mon_register_intr_ops(struct dp_soc *soc)
{
	struct dp_mon_ops *mon_ops = NULL;

	mon_ops = dp_mon_ops_get(soc);
	if (!mon_ops) {
		dp_mon_err("Monitor ops is NULL");
		return;
	}
	if (mon_ops->mon_register_intr_ops)
		mon_ops->mon_register_intr_ops(soc);
}
#endif

struct cdp_peer_rate_stats_ctx *dp_mon_peer_get_peerstats_ctx(struct
							      dp_peer *peer)
{
	struct dp_mon_peer *mon_peer = peer->monitor_peer;

	if (mon_peer)
		return mon_peer->peerstats_ctx;
	else
		return NULL;
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
void dp_mon_peer_reset_stats(struct dp_peer *peer)
{
	struct dp_mon_peer *mon_peer = NULL;

	mon_peer = peer->monitor_peer;
	if (!mon_peer)
		return;

	DP_STATS_CLR(mon_peer);
	DP_STATS_UPD(mon_peer, rx.avg_snr, CDP_INVALID_SNR);
}

void dp_mon_peer_get_stats(struct dp_peer *peer, void *arg,
			   enum cdp_stat_update_type type)
{
	struct dp_mon_peer *mon_peer = peer->monitor_peer;
	struct dp_mon_peer_stats *mon_peer_stats;

	if (!mon_peer || !arg)
		return;

	mon_peer_stats = &mon_peer->stats;

	switch (type) {
	case UPDATE_PEER_STATS:
	{
		struct cdp_peer_stats *peer_stats =
						(struct cdp_peer_stats *)arg;
		DP_UPDATE_MON_STATS(peer_stats, mon_peer_stats);
		break;
	}
	case UPDATE_VDEV_STATS:
	{
		struct cdp_vdev_stats *vdev_stats =
						(struct cdp_vdev_stats *)arg;
		DP_UPDATE_MON_STATS(vdev_stats, mon_peer_stats);
		break;
	}
	default:
		dp_mon_err("Invalid stats_update_type");
	}
}

void dp_mon_invalid_peer_update_pdev_stats(struct dp_pdev *pdev)
{
	struct dp_mon_peer *mon_peer;
	struct dp_mon_peer_stats *mon_peer_stats;
	struct cdp_pdev_stats *pdev_stats;

	if (!pdev || !pdev->monitor_pdev)
		return;

	mon_peer = pdev->monitor_pdev->invalid_mon_peer;
	if (!mon_peer)
		return;

	mon_peer_stats = &mon_peer->stats;
	pdev_stats = &pdev->stats;
	DP_UPDATE_MON_STATS(pdev_stats, mon_peer_stats);
}

QDF_STATUS
dp_mon_peer_get_stats_param(struct dp_peer *peer, enum cdp_peer_stats_type type,
			    cdp_peer_stats_param_t *buf)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct dp_mon_peer *mon_peer;

	mon_peer = peer->monitor_peer;
	if (!mon_peer)
		return QDF_STATUS_E_FAILURE;

	switch (type) {
	case cdp_peer_tx_rate:
		buf->tx_rate = mon_peer->stats.tx.tx_rate;
		break;
	case cdp_peer_tx_last_tx_rate:
		buf->last_tx_rate = mon_peer->stats.tx.last_tx_rate;
		break;
	case cdp_peer_tx_ratecode:
		buf->tx_ratecode = mon_peer->stats.tx.tx_ratecode;
		break;
	case cdp_peer_rx_rate:
		buf->rx_rate = mon_peer->stats.rx.rx_rate;
		break;
	case cdp_peer_rx_last_rx_rate:
		buf->last_rx_rate = mon_peer->stats.rx.last_rx_rate;
		break;
	case cdp_peer_rx_ratecode:
		buf->rx_ratecode = mon_peer->stats.rx.rx_ratecode;
		break;
	case cdp_peer_rx_avg_snr:
		buf->rx_avg_snr = mon_peer->stats.rx.avg_snr;
		break;
	case cdp_peer_rx_snr:
		buf->rx_snr = mon_peer->stats.rx.snr;
		break;
	default:
		dp_err("Invalid stats type requested");
		ret = QDF_STATUS_E_FAILURE;
	}

	return ret;
}
#endif

void dp_mon_ops_register(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	uint32_t target_type;

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_QCA6290:
	case TARGET_TYPE_QCA6390:
	case TARGET_TYPE_QCA6490:
	case TARGET_TYPE_QCA6750:
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
	case TARGET_TYPE_QCA8074:
	case TARGET_TYPE_QCA8074V2:
	case TARGET_TYPE_QCA6018:
	case TARGET_TYPE_QCA9574:
	case TARGET_TYPE_QCN9160:
	case TARGET_TYPE_QCN9000:
	case TARGET_TYPE_QCA5018:
	case TARGET_TYPE_QCN6122:
		dp_mon_ops_register_1_0(mon_soc);
		break;
	case TARGET_TYPE_QCN9224:
	case TARGET_TYPE_QCA5332:
#ifdef QCA_MONITOR_2_0_SUPPORT
		dp_mon_ops_register_2_0(mon_soc);
#endif
		break;
	default:
		dp_mon_err("%s: Unknown tgt type %d", __func__, target_type);
		qdf_assert_always(0);
		break;
	}
}

#ifdef QCA_MONITOR_OPS_PER_SOC_SUPPORT
void dp_mon_ops_free(struct dp_soc *soc)
{
	struct cdp_ops *ops = soc->cdp_soc.ops;
	struct cdp_mon_ops *cdp_mon_ops = ops->mon_ops;
	struct dp_mon_soc *mon_soc = soc->monitor_soc;
	struct dp_mon_ops *mon_ops = mon_soc->mon_ops;

	if (cdp_mon_ops)
		qdf_mem_free(cdp_mon_ops);

	if (mon_ops)
		qdf_mem_free(mon_ops);
}
#else
void dp_mon_ops_free(struct dp_soc *soc)
{
}
#endif

void dp_mon_cdp_ops_register(struct dp_soc *soc)
{
	struct cdp_ops *ops = soc->cdp_soc.ops;
	uint32_t target_type;

	if (!ops) {
		dp_mon_err("cdp_ops is NULL");
		return;
	}

	target_type = hal_get_target_type(soc->hal_soc);
	switch (target_type) {
	case TARGET_TYPE_QCA6290:
	case TARGET_TYPE_QCA6390:
	case TARGET_TYPE_QCA6490:
	case TARGET_TYPE_QCA6750:
	case TARGET_TYPE_KIWI:
	case TARGET_TYPE_MANGO:
	case TARGET_TYPE_QCA8074:
	case TARGET_TYPE_QCA8074V2:
	case TARGET_TYPE_QCA6018:
	case TARGET_TYPE_QCA9574:
	case TARGET_TYPE_QCN9160:
	case TARGET_TYPE_QCN9000:
	case TARGET_TYPE_QCA5018:
	case TARGET_TYPE_QCN6122:
		dp_mon_cdp_ops_register_1_0(ops);
#ifdef ATH_SUPPORT_NAC_RSSI
		ops->ctrl_ops->txrx_vdev_config_for_nac_rssi =
					dp_config_for_nac_rssi;
		ops->ctrl_ops->txrx_vdev_get_neighbour_rssi =
					dp_vdev_get_neighbour_rssi;
#endif
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
		ops->ctrl_ops->txrx_update_filter_neighbour_peers =
					dp_update_filter_neighbour_peers;
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */
#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
		dp_cfr_filter_register_1_0(ops);
#endif
		if (target_type == TARGET_TYPE_QCN9000)
			ops->ctrl_ops->txrx_update_mon_mac_filter =
					dp_update_mon_mac_filter;
		break;
	case TARGET_TYPE_QCN9224:
	case TARGET_TYPE_QCA5332:
#ifdef QCA_MONITOR_2_0_SUPPORT
		dp_mon_cdp_ops_register_2_0(ops);
#ifdef ATH_SUPPORT_NAC_RSSI
		ops->ctrl_ops->txrx_vdev_config_for_nac_rssi =
				dp_lite_mon_config_nac_rssi_peer;
		ops->ctrl_ops->txrx_vdev_get_neighbour_rssi =
				dp_lite_mon_get_nac_peer_rssi;
#endif
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
		ops->ctrl_ops->txrx_update_filter_neighbour_peers =
					dp_lite_mon_config_nac_peer;
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */
#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
		dp_cfr_filter_register_2_0(ops);
#endif
#endif /* QCA_MONITOR_2_0_SUPPORT */
		break;
	default:
		dp_mon_err("%s: Unknown tgt type %d", __func__, target_type);
		qdf_assert_always(0);
		break;
	}

	ops->cmn_drv_ops->txrx_set_monitor_mode = dp_vdev_set_monitor_mode;
	ops->cmn_drv_ops->txrx_get_mon_vdev_from_pdev =
				dp_get_mon_vdev_from_pdev_wifi3;
#ifdef DP_PEER_EXTENDED_API
	ops->misc_ops->pkt_log_init = dp_pkt_log_init;
	ops->misc_ops->pkt_log_con_service = dp_pkt_log_con_service;
	ops->misc_ops->pkt_log_exit = dp_pkt_log_exit;
#endif
	ops->ctrl_ops->enable_peer_based_pktlog =
				dp_enable_peer_based_pktlog;
#if defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(WLAN_RX_PKT_CAPTURE_ENH)
	ops->ctrl_ops->txrx_update_peer_pkt_capture_params =
				 dp_peer_update_pkt_capture_params;
#endif /* WLAN_TX_PKT_CAPTURE_ENH || WLAN_RX_PKT_CAPTURE_ENH */
#ifdef QCA_ENHANCED_STATS_SUPPORT
	ops->host_stats_ops->txrx_enable_enhanced_stats =
					dp_enable_enhanced_stats;
	ops->host_stats_ops->txrx_disable_enhanced_stats =
					dp_disable_enhanced_stats;
#endif /* QCA_ENHANCED_STATS_SUPPORT */
#ifdef WDI_EVENT_ENABLE
	ops->ctrl_ops->txrx_get_pldev = dp_get_pldev;
#endif
#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
	ops->host_stats_ops->txrx_get_scan_spcl_vap_stats =
					dp_get_scan_spcl_vap_stats;
#endif
	return;
}

#ifdef QCA_MONITOR_OPS_PER_SOC_SUPPORT
static inline void
dp_mon_cdp_mon_ops_deregister(struct cdp_ops *ops)
{
	if (ops->mon_ops) {
		qdf_mem_free(ops->mon_ops);
		ops->mon_ops = NULL;
	}
}
#else
static inline void
dp_mon_cdp_mon_ops_deregister(struct cdp_ops *ops)
{
	ops->mon_ops = NULL;
}
#endif

void dp_mon_cdp_ops_deregister(struct dp_soc *soc)
{
	struct cdp_ops *ops = soc->cdp_soc.ops;

	if (!ops) {
		dp_mon_err("cdp_ops is NULL");
		return;
	}

	dp_mon_cdp_mon_ops_deregister(ops);

#if defined(WLAN_CFR_ENABLE) && defined(WLAN_ENH_CFR_ENABLE)
	ops->cfr_ops->txrx_cfr_filter = NULL;
#endif
	ops->cmn_drv_ops->txrx_set_monitor_mode = NULL;
	ops->cmn_drv_ops->txrx_get_mon_vdev_from_pdev = NULL;
#ifdef DP_PEER_EXTENDED_API
	ops->misc_ops->pkt_log_init = NULL;
	ops->misc_ops->pkt_log_con_service = NULL;
	ops->misc_ops->pkt_log_exit = NULL;
#endif
#ifdef ATH_SUPPORT_NAC_RSSI
	ops->ctrl_ops->txrx_vdev_config_for_nac_rssi = NULL;
	ops->ctrl_ops->txrx_vdev_get_neighbour_rssi = NULL;
#endif
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
	ops->ctrl_ops->txrx_update_filter_neighbour_peers = NULL;
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */
	ops->ctrl_ops->enable_peer_based_pktlog = NULL;
#if defined(WLAN_TX_PKT_CAPTURE_ENH) || defined(WLAN_RX_PKT_CAPTURE_ENH)
	ops->ctrl_ops->txrx_update_peer_pkt_capture_params = NULL;
#endif /* WLAN_TX_PKT_CAPTURE_ENH || WLAN_RX_PKT_CAPTURE_ENH */
#ifdef FEATURE_PERPKT_INFO
	ops->host_stats_ops->txrx_enable_enhanced_stats = NULL;
	ops->host_stats_ops->txrx_disable_enhanced_stats = NULL;
#endif /* FEATURE_PERPKT_INFO */
#ifdef WDI_EVENT_ENABLE
	ops->ctrl_ops->txrx_get_pldev = NULL;
#endif
	return;
}

#if defined(WDI_EVENT_ENABLE) &&\
	(defined(QCA_ENHANCED_STATS_SUPPORT) || !defined(REMOVE_PKT_LOG))
static inline
void dp_mon_ppdu_stats_handler_deregister(struct dp_mon_soc *mon_soc)
{
	mon_soc->mon_ops->mon_ppdu_stats_ind_handler = NULL;
}
#else
static inline
void dp_mon_ppdu_stats_handler_deregister(struct dp_mon_soc *mon_soc)
{
}
#endif

#ifdef QCA_RSSI_DB2DBM
/**
 * dp_mon_compute_min_nf() - calculate the min nf value in the
 *                      active chains 20 MHz subbands.
 * @conv_params: cdp_rssi_dbm_conv_param_dp structure value
 * @min_nf: location to store min NF value
 * @chain_idx: active chain index in nfHwdbm array
 *
 * computation: Need to calculate nfInDbm[][] to A_MIN(nfHwDbm[][])
 *              considering row index as active chains and column
 *              index as 20MHZ subbands per chain.
 * example: chain_mask = 0x07 (consider 3 active chains 0,1,2 index)
 *          BandWidth = 40MHZ (40MHZ includes two 20MHZ subbands so need to
 *                      consider 0,1 index calculate min_nf value)
 *
 * Return: QDF_STATUS_SUCCESS if value set successfully
 *         QDF_STATUS_E_INVAL false if error
 */
static QDF_STATUS
dp_mon_compute_min_nf(struct cdp_rssi_dbm_conv_param_dp *conv_params,
		      int8_t *min_nf, int chain_idx)
{
	int j;
	*min_nf = conv_params->nf_hw_dbm[chain_idx][0];

	switch (conv_params->curr_bw) {
	case CHAN_WIDTH_20:
	case CHAN_WIDTH_5:
	case CHAN_WIDTH_10:
		break;
	case CHAN_WIDTH_40:
		for (j = 1; j < SUB40BW; j++) {
			if (conv_params->nf_hw_dbm[chain_idx][j] < *min_nf)
				*min_nf = conv_params->nf_hw_dbm[chain_idx][j];
		}
		break;
	case CHAN_WIDTH_80:
		for (j = 1; j < SUB80BW; j++) {
			if (conv_params->nf_hw_dbm[chain_idx][j] < *min_nf)
				*min_nf = conv_params->nf_hw_dbm[chain_idx][j];
		}
		break;
	case CHAN_WIDTH_160:
	case CHAN_WIDTH_80P80:
	case CHAN_WIDTH_165:
		for (j = 1; j < SUB160BW; j++) {
			if (conv_params->nf_hw_dbm[chain_idx][j] < *min_nf)
				*min_nf = conv_params->nf_hw_dbm[chain_idx][j];
		}
		break;
	case CHAN_WIDTH_160P160:
	case CHAN_WIDTH_320:
		for (j = 1; j < SUB320BW; j++) {
			if (conv_params->nf_hw_dbm[chain_idx][j] < *min_nf)
				*min_nf = conv_params->nf_hw_dbm[chain_idx][j];
		}
		break;
	default:
		dp_cdp_err("Invalid bandwidth %u", conv_params->curr_bw);
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_mon_pdev_params_rssi_dbm_conv() - to set rssi in dbm conversion
 *                                      params into monitor pdev.
 * @cdp_soc: dp soc handle.
 * @params: cdp_rssi_db2dbm_param_dp structure value.
 *
 * Return: QDF_STATUS_SUCCESS if value set successfully
 *         QDF_STATUS_E_INVAL false if error
 */
QDF_STATUS
dp_mon_pdev_params_rssi_dbm_conv(struct cdp_soc_t *cdp_soc,
				 struct cdp_rssi_db2dbm_param_dp *params)
{
	struct cdp_rssi_db2dbm_param_dp *dp_rssi_params = params;
	uint8_t pdev_id = params->pdev_id;
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_pdev *pdev =
		dp_get_pdev_from_soc_pdev_id_wifi3(soc, pdev_id);
	struct dp_mon_pdev *mon_pdev;
	struct cdp_rssi_temp_off_param_dp temp_off_param;
	struct cdp_rssi_dbm_conv_param_dp conv_params;
	int8_t min_nf = 0;
	int i;

	if (!soc->features.rssi_dbm_conv_support) {
		dp_cdp_err("rssi dbm conversion support is false");
		return QDF_STATUS_E_INVAL;
	}
	if (!pdev || !pdev->monitor_pdev) {
		dp_cdp_err("Invalid pdev_id %u", pdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_pdev->rssi_dbm_conv_support =
				soc->features.rssi_dbm_conv_support;

	if (dp_rssi_params->rssi_temp_off_present) {
		temp_off_param = dp_rssi_params->temp_off_param;
		mon_pdev->rssi_offsets.rssi_temp_offset =
				temp_off_param.rssi_temp_offset;
	}
	if (dp_rssi_params->rssi_dbm_info_present) {
		conv_params = dp_rssi_params->rssi_dbm_param;
		for (i = 0; i < CDP_MAX_NUM_ANTENNA; i++) {
			if (conv_params.curr_rx_chainmask & (0x01 << i)) {
				if (QDF_STATUS_E_INVAL == dp_mon_compute_min_nf
						(&conv_params, &min_nf, i))
					return QDF_STATUS_E_INVAL;
			} else {
				continue;
			}
		}
		mon_pdev->rssi_offsets.xlna_bypass_offset =
					conv_params.xlna_bypass_offset;
		mon_pdev->rssi_offsets.xlna_bypass_threshold =
					conv_params.xlna_bypass_threshold;
		mon_pdev->rssi_offsets.xbar_config = conv_params.xbar_config;
		mon_pdev->rssi_offsets.min_nf_dbm = min_nf;
		mon_pdev->rssi_offsets.rssi_offset =
					mon_pdev->rssi_offsets.min_nf_dbm +
				     mon_pdev->rssi_offsets.rssi_temp_offset;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

void dp_mon_intr_ops_deregister(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc = soc->monitor_soc;

	mon_soc->mon_rx_process = NULL;
	dp_mon_ppdu_stats_handler_deregister(mon_soc);
}

void dp_mon_feature_ops_deregister(struct dp_soc *soc)
{
	struct dp_mon_ops *mon_ops = dp_mon_ops_get(soc);

	if (!mon_ops) {
		dp_err("mon_ops is NULL");
		return;
	}

	mon_ops->mon_config_debug_sniffer = NULL;
	mon_ops->mon_peer_tx_init = NULL;
	mon_ops->mon_peer_tx_cleanup = NULL;
	mon_ops->mon_htt_ppdu_stats_attach = NULL;
	mon_ops->mon_htt_ppdu_stats_detach = NULL;
	mon_ops->mon_print_pdev_rx_mon_stats = NULL;
	mon_ops->mon_set_bsscolor = NULL;
	mon_ops->mon_pdev_get_filter_ucast_data = NULL;
	mon_ops->mon_pdev_get_filter_mcast_data = NULL;
	mon_ops->mon_pdev_get_filter_non_data = NULL;
	mon_ops->mon_neighbour_peer_add_ast = NULL;
#ifdef WLAN_TX_PKT_CAPTURE_ENH
	mon_ops->mon_peer_tid_peer_id_update = NULL;
	mon_ops->mon_tx_ppdu_stats_attach = NULL;
	mon_ops->mon_tx_ppdu_stats_detach = NULL;
	mon_ops->mon_tx_capture_debugfs_init = NULL;
	mon_ops->mon_tx_add_to_comp_queue = NULL;
	mon_ops->mon_peer_tx_capture_filter_check = NULL;
	mon_ops->mon_print_pdev_tx_capture_stats = NULL;
	mon_ops->mon_config_enh_tx_capture = NULL;
#endif
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	mon_ops->mon_config_enh_rx_capture = NULL;
#endif
#ifdef QCA_SUPPORT_BPR
	mon_ops->mon_set_bpr_enable = NULL;
#endif
#ifdef ATH_SUPPORT_NAC
	mon_ops->mon_set_filter_neigh_peers = NULL;
#endif
#ifdef WLAN_ATF_ENABLE
	mon_ops->mon_set_atf_stats_enable = NULL;
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
	mon_ops->mon_filter_setup_enhanced_stats = NULL;
	mon_ops->mon_tx_enable_enhanced_stats = NULL;
	mon_ops->mon_tx_disable_enhanced_stats = NULL;
	mon_ops->mon_ppdu_desc_deliver = NULL;
	mon_ops->mon_ppdu_desc_notify = NULL;
	mon_ops->mon_ppdu_stats_feat_enable_check = NULL;
#ifdef WLAN_FEATURE_11BE
	mon_ops->mon_tx_stats_update = NULL;
#endif
#endif
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
	mon_ops->mon_filter_setup_smart_monitor = NULL;
#endif
	mon_ops->mon_filter_set_reset_mon_mac_filter = NULL;
#ifdef WLAN_RX_PKT_CAPTURE_ENH
	mon_ops->mon_filter_setup_rx_enh_capture = NULL;
#endif
#ifdef WDI_EVENT_ENABLE
	mon_ops->mon_set_pktlog_wifi3 = NULL;
	mon_ops->mon_filter_setup_rx_pkt_log_full = NULL;
	mon_ops->mon_filter_reset_rx_pkt_log_full = NULL;
	mon_ops->mon_filter_setup_rx_pkt_log_lite = NULL;
	mon_ops->mon_filter_reset_rx_pkt_log_lite = NULL;
	mon_ops->mon_filter_setup_rx_pkt_log_cbf = NULL;
	mon_ops->mon_filter_reset_rx_pkt_log_cbf = NULL;
#ifdef BE_PKTLOG_SUPPORT
	mon_ops->mon_filter_setup_pktlog_hybrid = NULL;
	mon_ops->mon_filter_reset_pktlog_hybrid = NULL;
#endif
#endif
#if defined(DP_CON_MON) && !defined(REMOVE_PKT_LOG)
	mon_ops->mon_pktlogmod_exit = NULL;
#endif
	mon_ops->rx_hdr_length_set = NULL;
	mon_ops->rx_packet_length_set = NULL;
	mon_ops->rx_wmask_subscribe = NULL;
	mon_ops->rx_enable_mpdu_logging = NULL;
	mon_ops->rx_enable_fpmo = NULL;
	mon_ops->mon_neighbour_peers_detach = NULL;
	mon_ops->mon_vdev_set_monitor_mode_buf_rings = NULL;
	mon_ops->mon_vdev_set_monitor_mode_rings = NULL;
#ifdef QCA_ENHANCED_STATS_SUPPORT
	mon_ops->mon_rx_stats_update = NULL;
	mon_ops->mon_rx_populate_ppdu_usr_info = NULL;
	mon_ops->mon_rx_populate_ppdu_info = NULL;
#endif
}

QDF_STATUS dp_mon_soc_attach(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc;

	if (!soc) {
		dp_mon_err("dp_soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mon_soc = (struct dp_mon_soc *)qdf_mem_malloc(sizeof(*mon_soc));
	if (!mon_soc) {
		dp_mon_err("%pK: mem allocation failed", soc);
		return QDF_STATUS_E_NOMEM;
	}
	/* register monitor ops */
	soc->monitor_soc = mon_soc;
	dp_mon_ops_register(soc);
	dp_mon_register_intr_ops(soc);

	dp_mon_cdp_ops_register(soc);
	dp_mon_register_feature_ops(soc);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_mon_soc_detach(struct dp_soc *soc)
{
	struct dp_mon_soc *mon_soc;

	if (!soc) {
		dp_mon_err("dp_soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mon_soc = soc->monitor_soc;
	dp_monitor_vdev_timer_deinit(soc);
	dp_mon_cdp_ops_deregister(soc);
	soc->monitor_soc = NULL;
	qdf_mem_free(mon_soc);
	return QDF_STATUS_SUCCESS;
}
