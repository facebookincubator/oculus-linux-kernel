/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: contains coex target if functions
 */
#include <wlan_coex_main.h>
#include <target_if_coex.h>
#include "wlan_coex_public_structs.h"

/**
 * target_if_coex_config_send() - Function to send coex config command
 * @pdev: PDEV object
 * @param: Pointer to coex config parameters
 *
 * Return: QDF STATUS
 */
static QDF_STATUS
target_if_coex_config_send(struct wlan_objmgr_pdev *pdev,
			   struct coex_config_params *param)
{
	wmi_unified_t pdev_wmi_handle;

	pdev_wmi_handle = GET_WMI_HDL_FROM_PDEV(pdev);
	if (!pdev_wmi_handle) {
		coex_err("Invalid PDEV WMI handle");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_send_coex_config_cmd(pdev_wmi_handle, param);
}

/**
 * target_if_coex_multi_config_send() - Function to send coex multiple config
 * command
 * @pdev: PDEV object
 * @param: Pointer to coex multiple config parameters
 *
 * Return: QDF STATUS
 */
static QDF_STATUS
target_if_coex_multi_config_send(struct wlan_objmgr_pdev *pdev,
				 struct coex_multi_config *param)
{
	wmi_unified_t pdev_wmi_handle;

	pdev_wmi_handle = GET_WMI_HDL_FROM_PDEV(pdev);
	if (!pdev_wmi_handle) {
		coex_err("Invalid PDEV WMI handle");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_send_coex_multi_config_cmd(pdev_wmi_handle, param);
}

/**
 * target_if_coex_get_multi_config_support() - Function to get coex multiple
 * config command support
 * @psoc: PSOC object
 *
 * Return: true if target support coex multiple config command
 */
static bool
target_if_coex_get_multi_config_support(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return false;
	}

	return wmi_service_enabled(wmi_handle,
				   wmi_service_multiple_coex_config_support);
}

QDF_STATUS
target_if_coex_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_coex_tx_ops *coex_ops;

	if (!tx_ops) {
		coex_err("target if tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	coex_ops = &tx_ops->coex_ops;
	coex_ops->coex_config_send = target_if_coex_config_send;
	coex_ops->coex_multi_config_send = target_if_coex_multi_config_send;
	coex_ops->coex_get_multi_config_support =
				target_if_coex_get_multi_config_support;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_DBAM_CONFIG
QDF_STATUS
target_if_dbam_process_event(struct wlan_objmgr_psoc *psoc,
			     enum coex_dbam_comp_status resp)
{
	struct coex_psoc_obj *coex_obj;
	struct wlan_coex_callback *cb;

	if (!psoc) {
		coex_err("psoc is null");
		return QDF_STATUS_E_INVAL;
	}

	coex_obj = wlan_psoc_get_coex_obj(psoc);
	if (!coex_obj) {
		coex_err("failed to get coex_obj");
		return QDF_STATUS_E_INVAL;
	}

	cb = &coex_obj->cb;
	if (cb->set_dbam_config_cb)
		cb->set_dbam_config_cb(cb->set_dbam_config_ctx, &resp);

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_dbam_response_event_handler() - function to handle dbam response
 * event from firmware.
 * @scn: scn handle
 * @data: data buffer foe the event
 * @len: data length
 *
 * Return: 0 on success, and error code on failure
 */
static int target_if_dbam_response_event_handler(ol_scn_t scn,
						 uint8_t *data,
						 uint32_t len)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	wmi_unified_t wmi_handle;
	struct wlan_lmac_if_dbam_rx_ops *rx_ops;
	struct coex_dbam_config_resp resp = {0};

	target_if_debug("scn:%pK, data:%pK, datalen:%d", scn, data, len);
	if (!scn || !data) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, data);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is Null");
		return -EINVAL;
	}

	rx_ops = wlan_psoc_get_dbam_rx_ops(psoc);
	if (!rx_ops || !rx_ops->dbam_resp_event) {
		target_if_err("callback not registered");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	status = wmi_extract_dbam_config_response(wmi_handle, data, &resp);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Failed to extract dbam config response");
		return -EINVAL;
	}

	status = rx_ops->dbam_resp_event(psoc, resp.dbam_resp);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("process dbam response event failed");
		return -EINVAL;
	}

	return 0;
}

/**
 * target_if_dbam_config_send() - Send WMI command for DBAM configuration
 * @psoc: psoc pointer
 * @param: dbam config parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_dbam_config_send(struct wlan_objmgr_psoc *psoc,
			   struct coex_dbam_config_params *param)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid WMI handle");
		return QDF_STATUS_E_FAILURE;
	}

	status = wmi_unified_send_dbam_config_cmd(wmi_handle, param);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to send DBAM config %d", status);

	return status;
}

static QDF_STATUS
target_if_dbam_register_event_handler(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid WMI handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_register_event_handler(wmi_handle,
					wmi_coex_dbam_complete_event_id,
					target_if_dbam_response_event_handler,
					WMI_RX_WORK_CTX);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to register dbam complete event cb");

	return status;
}

static QDF_STATUS
target_if_dbam_unregister_event_handler(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid WMI handle");
		return QDF_STATUS_E_INVAL;
	}

	wmi_unified_unregister_event_handler(wmi_handle,
					     wmi_coex_dbam_complete_event_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_dbam_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_dbam_tx_ops *dbam_tx_ops;

	if (!tx_ops) {
		target_if_err("target if tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	dbam_tx_ops = &tx_ops->dbam_tx_ops;
	if (!dbam_tx_ops) {
		target_if_err("target if dbam ops is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	dbam_tx_ops->set_dbam_config = target_if_dbam_config_send;
	dbam_tx_ops->dbam_event_attach = target_if_dbam_register_event_handler;
	dbam_tx_ops->dbam_event_detach =
		target_if_dbam_unregister_event_handler;

	return QDF_STATUS_SUCCESS;
}
#endif
