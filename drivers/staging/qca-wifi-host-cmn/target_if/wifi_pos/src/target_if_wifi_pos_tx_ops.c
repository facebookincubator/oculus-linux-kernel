/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: target_if_wifi_pos_tx_ops.c
 * This file defines the functions pertinent to wifi positioning component's
 * target if layer TX operations.
 */
#include "wifi_pos_utils_pub.h"

#include "wmi_unified_api.h"
#include "wlan_lmac_if_def.h"
#include "target_if_wifi_pos.h"
#include "target_if_wifi_pos_tx_ops.h"
#include "wifi_pos_utils_i.h"
#include "wifi_pos_api.h"
#include "wifi_pos_pasn_api.h"
#include "target_if.h"

/**
 * target_if_wifi_pos_oem_data_req() - start OEM data request to target
 * @pdev: pointer to pdev object mgr
 * @req: start request params
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_wifi_pos_oem_data_req(struct wlan_objmgr_pdev *pdev,
				struct oem_data_req *req)
{
	QDF_STATUS status;
	wmi_unified_t wmi_hdl = get_wmi_unified_hdl_from_pdev(pdev);

	target_if_debug("Send oem data req to target");

	if (!req || !req->data) {
		target_if_err("oem_data_req is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!wmi_hdl) {
		target_if_err("WMA closed, can't send oem data req cmd");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_start_oem_data_cmd(wmi_hdl, req->data_len,
						req->data);

	if (!QDF_IS_STATUS_SUCCESS(status))
		target_if_err("wmi cmd send failed");

	return status;
}

#if !defined(CNSS_GENL) && defined(WLAN_RTT_MEASUREMENT_NOTIFICATION)
static QDF_STATUS
target_if_wifi_pos_parse_measreq_chan_info(struct wlan_objmgr_pdev *pdev,
					   uint32_t data_len, uint8_t *data,
					   struct rtt_channel_info *chinfo)
{
	QDF_STATUS status;
	wmi_unified_t wmi_hdl = get_wmi_unified_hdl_from_pdev(pdev);

	if (!data) {
		target_if_err("data is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!wmi_hdl) {
		target_if_err("wmi_hdl is null");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_extract_measreq_chan_info(wmi_hdl, data_len, data,
						       chinfo);
	if (!QDF_IS_STATUS_SUCCESS(status))
		target_if_err("wmi_unified_extract_measreq_chan_info failed");

	return status;
}
#else
static inline QDF_STATUS
target_if_wifi_pos_parse_measreq_chan_info(struct wlan_objmgr_pdev *pdev,
					   uint32_t data_len, uint8_t *data,
					   struct rtt_channel_info *chinfo)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_RTT_MEASUREMENT_NOTIFICATION */

#ifdef WLAN_FEATURE_RTT_11AZ_SUPPORT
static QDF_STATUS
target_if_wifi_pos_send_rtt_pasn_auth_status(struct wlan_objmgr_psoc *psoc,
					     struct wlan_pasn_auth_status *data)
{
	QDF_STATUS status;
	wmi_unified_t wmi = GET_WMI_HDL_FROM_PSOC(psoc);

	if (!psoc || !wmi) {
		target_if_err("%s is null", !psoc ? "psoc" : "wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_send_rtt_pasn_auth_status_cmd(wmi, data);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("send pasn auth status cmd failed");

	return status;
}

static QDF_STATUS
target_if_wifi_pos_send_rtt_pasn_deauth(struct wlan_objmgr_psoc *psoc,
					struct qdf_mac_addr *peer_mac)
{
	QDF_STATUS status;
	wmi_unified_t wmi = GET_WMI_HDL_FROM_PSOC(psoc);

	if (!psoc || !wmi) {
		target_if_err("%s is null", !psoc ? "psoc" : "wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_send_rtt_pasn_deauth_cmd(wmi, peer_mac);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("send pasn deauth cmd failed");

	return status;
}

static void target_if_wifi_pos_register_11az_ops(
			struct wlan_lmac_if_wifi_pos_tx_ops *tx_ops)
{
	tx_ops->send_rtt_pasn_auth_status =
			target_if_wifi_pos_send_rtt_pasn_auth_status;
	tx_ops->send_rtt_pasn_deauth = target_if_wifi_pos_send_rtt_pasn_deauth;
}
#else
static inline
void target_if_wifi_pos_register_11az_ops(
			struct wlan_lmac_if_wifi_pos_tx_ops *tx_ops)
{}
#endif

#ifdef WIFI_POS_CONVERGED
#ifdef WLAN_FEATURE_RTT_11AZ_SUPPORT
static QDF_STATUS
target_if_wifi_pos_register_11az_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_rtt_pasn_peer_create_req_eventid,
			target_if_wifi_pos_pasn_peer_create_ev_handler,
			WMI_RX_EXECUTION_CTX);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("register pasn peer create event_handler failed");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_rtt_pasn_peer_delete_eventid,
			target_if_wifi_pos_pasn_peer_delete_ev_handler,
			WMI_RX_EXECUTION_CTX);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("register pasn peer delete event_handler failed");
		return status;
	}

	return status;
}

static void
target_if_wifi_pos_unregister_11az_events(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc || !GET_WMI_HDL_FROM_PSOC(psoc)) {
		target_if_err("psoc or psoc->tgt_if_handle is null");
		return;
	}

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_rtt_pasn_peer_create_req_eventid);

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_rtt_pasn_peer_delete_eventid);
}
#else
static QDF_STATUS
target_if_wifi_pos_register_11az_events(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static void
target_if_wifi_pos_unregister_11az_events(struct wlan_objmgr_psoc *psoc)
{}
#endif /* WLAN_FEATURE_RTT_11AZ_SUPPORT */

static
QDF_STATUS target_if_wifi_pos_register_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS ret;

	if (!psoc || !GET_WMI_HDL_FROM_PSOC(psoc)) {
		target_if_err("psoc or psoc->tgt_if_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	/* wmi_oem_response_event_id is not defined for legacy targets.
	 * So do not check for error for this event.
	 */
	wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_response_event_id,
			target_if_wifi_pos_oem_rsp_ev_handler,
			WMI_RX_WORK_CTX);

	ret = wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_cap_event_id,
			wifi_pos_oem_cap_ev_handler,
			WMI_RX_WORK_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("register_event_handler failed: err %d", ret);
		return QDF_STATUS_E_INVAL;
	}

	ret = wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_meas_report_event_id,
			wifi_pos_oem_meas_rpt_ev_handler,
			WMI_RX_WORK_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("register_event_handler failed: err %d", ret);
		return QDF_STATUS_E_INVAL;
	}

	ret = wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_report_event_id,
			wifi_pos_oem_err_rpt_ev_handler,
			WMI_RX_WORK_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("register_event_handler failed: err %d", ret);
		return QDF_STATUS_E_INVAL;
	}

	target_if_wifi_pos_register_11az_events(psoc);

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS target_if_wifi_pos_deregister_events(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc || !GET_WMI_HDL_FROM_PSOC(psoc)) {
		target_if_err("psoc or psoc->tgt_if_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_response_event_id);
	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_cap_event_id);
	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_meas_report_event_id);
	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_oem_report_event_id);
	target_if_wifi_pos_unregister_11az_events(psoc);

	return QDF_STATUS_SUCCESS;
}

void target_if_wifi_pos_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_wifi_pos_tx_ops *wifi_pos_tx_ops;

	wifi_pos_tx_ops = &tx_ops->wifi_pos_tx_ops;
	wifi_pos_tx_ops->wifi_pos_register_events =
			target_if_wifi_pos_register_events;
	wifi_pos_tx_ops->wifi_pos_deregister_events =
			target_if_wifi_pos_deregister_events;
	wifi_pos_tx_ops->data_req_tx = target_if_wifi_pos_oem_data_req;
	wifi_pos_tx_ops->wifi_pos_convert_pdev_id_host_to_target =
		target_if_wifi_pos_convert_pdev_id_host_to_target;
	wifi_pos_tx_ops->wifi_pos_convert_pdev_id_target_to_host =
		target_if_wifi_pos_convert_pdev_id_target_to_host;
	wifi_pos_tx_ops->wifi_pos_get_vht_ch_width =
		target_if_wifi_pos_get_vht_ch_width;
	wifi_pos_tx_ops->data_req_tx = target_if_wifi_pos_oem_data_req;
	wifi_pos_tx_ops->wifi_pos_parse_measreq_chan_info =
			target_if_wifi_pos_parse_measreq_chan_info;

	target_if_wifi_pos_register_11az_ops(wifi_pos_tx_ops);
}
#endif
