/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "target_if.h"
#include "target_if_ll_sap.h"
#include "wlan_ll_sap_public_structs.h"
#include "wmi_unified_ll_sap_api.h"
#include "../../umac/mlme/sap/ll_sap/core/src/wlan_ll_sap_main.h"
#include "wlan_ll_sap_api.h"

/**
 * target_if_send_audio_transport_switch_resp() - Send audio transport switch
 * response to fw
 * @psoc: pointer to psoc
 * @req_type: Bearer switch request type
 * @status: Status of the bearer switch request
 *
 * Return: pointer to tx ops
 */
static QDF_STATUS target_if_send_audio_transport_switch_resp(
					struct wlan_objmgr_psoc *psoc,
					enum bearer_switch_req_type req_type,
					enum bearer_switch_status status)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null.");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return wmi_unified_audio_transport_switch_resp_send(wmi_handle,
							    req_type, status);
}

static int target_if_send_audio_transport_switch_req_event_handler(
							ol_scn_t scn,
							uint8_t *event,
							uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct ll_sap_psoc_priv_obj *psoc_ll_sap_obj;
	enum bearer_switch_req_type req_type;
	struct wmi_unified *wmi_handle;
	struct wlan_ll_sap_rx_ops *rx_ops;
	QDF_STATUS qdf_status;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	psoc_ll_sap_obj = wlan_objmgr_psoc_get_comp_private_obj(
							psoc,
							WLAN_UMAC_COMP_LL_SAP);

	if (!psoc_ll_sap_obj) {
		target_if_err("psoc_ll_sap_obj is null");
		return -EINVAL;
	}

	rx_ops = &psoc_ll_sap_obj->rx_ops;
	if (!rx_ops || !rx_ops->audio_transport_switch_req) {
		target_if_err("Invalid ll_sap rx ops");
		return -EINVAL;
	}

	qdf_status = wmi_extract_audio_transport_switch_req_event(wmi_handle,
								  event, len,
								  &req_type);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}
	rx_ops->audio_transport_switch_req(psoc, req_type);

	return 0;
}

void
target_if_ll_sap_register_tx_ops(struct wlan_ll_sap_tx_ops *tx_ops)
{
	tx_ops->send_audio_transport_switch_resp =
		target_if_send_audio_transport_switch_resp;
}

void
target_if_ll_sap_register_rx_ops(struct wlan_ll_sap_rx_ops *rx_ops)
{
	rx_ops->audio_transport_switch_req =
		wlan_ll_sap_fw_bearer_switch_req;
}

QDF_STATUS
target_if_ll_sap_register_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS ret;
	wmi_unified_t handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!handle) {
		target_if_err("handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(
			handle,
			wmi_audio_transport_switch_type_event_id,
			target_if_send_audio_transport_switch_req_event_handler,
			WMI_RX_SERIALIZER_CTX);
	return ret;
}

QDF_STATUS
target_if_ll_sap_deregister_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS ret;
	wmi_unified_t handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!handle) {
		target_if_err("handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_unregister_event_handler(
			handle,
			wmi_audio_transport_switch_type_event_id);
	return ret;
}
