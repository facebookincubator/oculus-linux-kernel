/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains fw offload south bound interface definitions
 */

#include "scheduler_api.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_fwol_public_structs.h"
#include "wlan_fwol_ucfg_api.h"
#include "wlan_fwol_tgt_api.h"
#include "wlan_fw_offload_main.h"
#include "target_if.h"

QDF_STATUS tgt_fwol_register_ev_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_tx_ops *tx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!psoc) {
		fwol_err("NULL psoc handle");
		return QDF_STATUS_E_INVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL Obj");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops = &fwol_obj->tx_ops;
	if (tx_ops->reg_evt_handler) {
		status = tx_ops->reg_evt_handler(psoc, NULL);
		fwol_debug("reg_evt_handler, status:%d", status);
	} else {
		fwol_alert("No reg_evt_handler");
	}

	return status;
}

QDF_STATUS tgt_fwol_unregister_ev_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_tx_ops *tx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!psoc) {
		fwol_err("NNULL psoc handle");
		return QDF_STATUS_E_INVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL Obj");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops = &fwol_obj->tx_ops;
	if (tx_ops->unreg_evt_handler) {
		status = tx_ops->unreg_evt_handler(psoc, NULL);
		fwol_debug("unreg_evt_handler, status:%d", status);
	} else {
		fwol_alert("No unreg_evt_handler");
	}

	return status;
}

/**
 * fwol_flush_callback() - fw offload message flush callback
 * @msg: fw offload message
 *
 * Return: QDF_STATUS_SUCCESS on success.
 */
__attribute__((unused))
static QDF_STATUS fwol_flush_callback(struct scheduler_msg *msg)
{
	struct wlan_fwol_rx_event *event;

	if (!msg) {
		fwol_err("NULL pointer for eLNA message");
		return QDF_STATUS_E_INVAL;
	}

	event = msg->bodyptr;
	msg->bodyptr = NULL;
	fwol_release_rx_event(event);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ELNA
/**
 * tgt_fwol_get_elna_bypass_resp() - handler for get eLNA bypass response
 * @psoc: psoc handle
 * @resp: status for last channel config
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
tgt_fwol_get_elna_bypass_resp(struct wlan_objmgr_psoc *psoc,
			      struct get_elna_bypass_response *resp)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct wlan_fwol_rx_event *event;

	event = qdf_mem_malloc(sizeof(*event));
	if (!event)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_FWOL_SB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		fwol_err("Failed to get psoc ref");
		fwol_release_rx_event(event);
		return status;
	}

	event->psoc = psoc;
	event->event_id = WLAN_FWOL_EVT_GET_ELNA_BYPASS_RESPONSE;
	event->get_elna_bypass_response = *resp;
	msg.type = WLAN_FWOL_EVT_GET_ELNA_BYPASS_RESPONSE;
	msg.bodyptr = event;
	msg.callback = fwol_process_event;
	msg.flush_callback = fwol_flush_callback;
	status = scheduler_post_message(QDF_MODULE_ID_FWOL,
					QDF_MODULE_ID_FWOL,
					QDF_MODULE_ID_TARGET_IF, &msg);

	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	fwol_err("failed to send WLAN_FWOL_GET_ELNA_BYPASS_RESPONSE msg");
	fwol_flush_callback(&msg);

	return status;
}

static void tgt_fwol_register_elna_rx_ops(struct wlan_fwol_rx_ops *rx_ops)
{
	rx_ops->get_elna_bypass_resp = tgt_fwol_get_elna_bypass_resp;
}
#else
static void tgt_fwol_register_elna_rx_ops(struct wlan_fwol_rx_ops *rx_ops)
{
}
#endif /* WLAN_FEATURE_ELNA */

#ifdef FW_THERMAL_THROTTLE_SUPPORT
/**
 * notify_thermal_throttle_handler() - Thermal throttle stats event handler
 * @psoc: psoc object
 * @info: thermal throttle stats info from target if layer
 *
 * The handle will be registered to target if layer. Target if layer
 * will notify the new level from firmware thermal stats event.
 *
 * Return: QDF_STATUS_SUCCESS for success
 */
static QDF_STATUS
notify_thermal_throttle_handler(struct wlan_objmgr_psoc *psoc,
				struct thermal_throttle_info *info)
{
	struct wlan_fwol_psoc_obj *fwol_obj;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct fwol_thermal_callbacks *thermal_cbs;

	if (!psoc) {
		fwol_err("NULL psoc handle");
		return QDF_STATUS_E_INVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL Obj");
		return QDF_STATUS_E_INVAL;
	}
	thermal_cbs = &fwol_obj->thermal_cbs;
	fwol_nofl_debug("thermal evt: pdev %d lvl %d",
			info->pdev_id, info->level);
	if (info->pdev_id <= fwol_obj->thermal_throttle.pdev_id ||
	    fwol_obj->thermal_throttle.pdev_id == WLAN_INVALID_PDEV_ID) {
		fwol_obj->thermal_throttle.level = info->level;
		fwol_obj->thermal_throttle.pdev_id = info->pdev_id;
		if (thermal_cbs->notify_thermal_throttle_handler)
			status =
			thermal_cbs->notify_thermal_throttle_handler(psoc,
								     info);
		else
			fwol_debug("no thermal throttle handler");
	}

	return status;
}
#endif

#ifdef THERMAL_STATS_SUPPORT
static QDF_STATUS
tgt_fwol_get_thermal_stats_resp(struct wlan_objmgr_psoc *psoc,
				struct thermal_throttle_info *resp)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct wlan_fwol_rx_event *event;

	event = qdf_mem_malloc(sizeof(*event));
	if (!event)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_FWOL_SB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		fwol_err("Failed to get psoc ref");
		fwol_release_rx_event(event);
		return status;
	}

	event->psoc = psoc;
	event->event_id = WLAN_FWOL_EVT_GET_THERMAL_STATS_RESPONSE;
	event->get_thermal_stats_response = *resp;
	msg.type = WLAN_FWOL_EVT_GET_THERMAL_STATS_RESPONSE;
	msg.bodyptr = event;
	msg.callback = fwol_process_event;
	msg.flush_callback = fwol_flush_callback;
	status = scheduler_post_message(QDF_MODULE_ID_FWOL,
					QDF_MODULE_ID_FWOL,
					QDF_MODULE_ID_TARGET_IF, &msg);

	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	fwol_err("failed to send WLAN_FWOL_EVT_GET_THERMAL_STATS_RESPONSE msg");
	fwol_flush_callback(&msg);

	return status;

}
#endif

#ifdef THERMAL_STATS_SUPPORT
static void
tgt_fwol_register_thermal_stats_resp(struct wlan_fwol_rx_ops *rx_ops)
{
	rx_ops->get_thermal_stats_resp = tgt_fwol_get_thermal_stats_resp;
}
#else
static void
tgt_fwol_register_thermal_stats_resp(struct wlan_fwol_rx_ops *rx_ops)
{
}
#endif
#ifdef FW_THERMAL_THROTTLE_SUPPORT
static void
tgt_fwol_register_notify_thermal_throttle_evt(struct wlan_fwol_rx_ops *rx_ops)
{
	rx_ops->notify_thermal_throttle_handler =
					notify_thermal_throttle_handler;
}
#else
static void
tgt_fwol_register_notify_thermal_throttle_evt(struct wlan_fwol_rx_ops *rx_ops)
{
}
#endif

static void tgt_fwol_register_thermal_rx_ops(struct wlan_fwol_rx_ops *rx_ops)
{
	tgt_fwol_register_notify_thermal_throttle_evt(rx_ops);
	tgt_fwol_register_thermal_stats_resp(rx_ops);
}

QDF_STATUS tgt_fwol_register_rx_ops(struct wlan_fwol_rx_ops *rx_ops)
{
	tgt_fwol_register_elna_rx_ops(rx_ops);
	tgt_fwol_register_thermal_rx_ops(rx_ops);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_fwol_pdev_param_send(struct wlan_objmgr_pdev *pdev,
				    struct pdev_params pdev_param)
{
	struct wmi_unified *wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_pdev_param_send(wmi_handle, &pdev_param,
					   FWOL_WILDCARD_PDEV_ID);
}

QDF_STATUS tgt_fwol_vdev_param_send(struct wlan_objmgr_psoc *psoc,
				    struct vdev_set_params vdev_param)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_vdev_set_param_send(wmi_handle, &vdev_param);
}
