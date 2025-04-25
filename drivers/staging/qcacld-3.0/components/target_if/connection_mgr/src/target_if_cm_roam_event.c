/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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

/**
 * DOC: This file contains definitions for target_if roaming events.
 */
#include "qdf_types.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wmi_unified_api.h"
#include "scheduler_api.h"
#include <wmi_unified.h>
#include "target_if_cm_roam_event.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_mlme_main.h"
#include <../../core/src/wlan_cm_roam_i.h>
#include "wlan_cm_roam_api.h"
#include "target_if_cm_roam_offload.h"
#include <target_if_vdev_mgr_rx_ops.h>
#include <target_if_psoc_wake_lock.h>
#include "wlan_mlo_mgr_peer.h"
#include "wlan_crypto_global_api.h"

struct wlan_cm_roam_rx_ops *
target_if_cm_get_roam_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *psoc_ext_priv;
	struct wlan_cm_roam_rx_ops *rx_ops;

	if (!psoc) {
		target_if_err("psoc object is NULL");
		return NULL;
	}
	psoc_ext_priv = wlan_psoc_mlme_get_ext_hdl(psoc);
	if (!psoc_ext_priv) {
		target_if_err("psoc legacy private object is NULL");
		return NULL;
	}

	rx_ops = &psoc_ext_priv->rso_rx_ops;
	return rx_ops;
}

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
static void target_if_cm_roam_register_vendor_handoff_rx_ops(
					struct wlan_cm_roam_rx_ops *rx_ops)
{
	rx_ops->roam_vendor_handoff_event =
					cm_roam_vendor_handoff_event_handler;
}
#else
static inline void
target_if_cm_roam_register_vendor_handoff_rx_ops(
					struct wlan_cm_roam_rx_ops *rx_ops)
{
}
#endif

void
target_if_cm_roam_register_rx_ops(struct wlan_cm_roam_rx_ops *rx_ops)
{
	rx_ops->roam_sync_event = cm_roam_sync_event_handler;
	rx_ops->roam_sync_frame_event = cm_roam_sync_frame_event_handler;
	rx_ops->roam_sync_key_event = cm_roam_sync_key_event_handler;
	rx_ops->roam_event_rx = cm_roam_event_handler;
	rx_ops->btm_denylist_event = cm_btm_denylist_event_handler;
	rx_ops->vdev_disconnect_event = cm_vdev_disconnect_event_handler;
	rx_ops->roam_scan_chan_list_event = cm_roam_scan_ch_list_event_handler;
	rx_ops->roam_stats_event_rx = cm_roam_stats_event_handler;
	rx_ops->roam_auth_offload_event = cm_roam_auth_offload_event_handler;
	rx_ops->roam_pmkid_request_event_rx = cm_roam_pmkid_request_handler;
	rx_ops->roam_candidate_frame_event = cm_roam_candidate_event_handler;
	target_if_cm_roam_register_vendor_handoff_rx_ops(rx_ops);
}

int target_if_cm_roam_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct roam_offload_roam_event *roam_event = NULL;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;

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

	roam_event = qdf_mem_malloc(sizeof(*roam_event));
	if (!roam_event)
		return -ENOMEM;

	qdf_status = wmi_extract_roam_event(wmi_handle, event, len, roam_event);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		qdf_status = QDF_STATUS_E_INVAL;
		goto done;
	}

	roam_event->psoc = psoc;

	/**
	 * Stop the timer upon RSO stop status success. The timer shall continue
	 * to run upon HO_FAIL status and would be stopped upon HO_FAILED event
	 */
	if ((roam_event->reason == ROAM_REASON_RSO_STATUS &&
	     roam_event->notif_params == WMI_ROAM_SCAN_MODE_NONE) ||
	    roam_event->reason == ROAM_REASON_HO_FAILED)
		target_if_stop_rso_stop_timer(roam_event);

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_event_rx) {
		target_if_err("No valid roam rx ops");
		qdf_status = QDF_STATUS_E_INVAL;
		goto done;
	}

	/**
	 * This can be called from IRQ context for WOW events such as
	 * WOW_REASON_LOW_RSSI and WOW_REASON_HO_FAIL. There is no issue
	 * currently, as these events are posted to schedular thread from
	 * cm_roam_event_handler, to access umac which use mutex.
	 * If any new ROAM event is added in IRQ context in future, avoid taking
	 * mutex. If mutex/sleep is needed, post a message to scheduler thread.
	 */
	qdf_status = roam_rx_ops->roam_event_rx(roam_event);

done:
	qdf_mem_free(roam_event);
	return qdf_status_to_os_return(qdf_status);
}

QDF_STATUS
target_if_roam_register_common_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS ret;
	wmi_unified_t handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!handle) {
		target_if_err("handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* Register for LFR2/3 common roam event */
	ret = wmi_unified_register_event_handler(handle, wmi_roam_event_id,
						 target_if_cm_roam_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void
target_if_free_roam_synch_frame_ind(struct roam_synch_frame_ind *frame_ind)
{
	if (frame_ind->bcn_probe_rsp) {
		qdf_mem_free(frame_ind->bcn_probe_rsp);
		frame_ind->bcn_probe_rsp_len = 0;
		frame_ind->bcn_probe_rsp = NULL;
	}
	if (frame_ind->link_bcn_probe_rsp) {
		qdf_mem_free(frame_ind->link_bcn_probe_rsp);
		frame_ind->link_bcn_probe_rsp_len = 0;
		frame_ind->link_bcn_probe_rsp = NULL;
	}
	if (frame_ind->reassoc_req) {
		qdf_mem_free(frame_ind->reassoc_req);
		frame_ind->reassoc_req_len = 0;
		frame_ind->reassoc_req = NULL;
	}
	if (frame_ind->reassoc_rsp) {
		qdf_mem_free(frame_ind->reassoc_rsp);
		frame_ind->reassoc_rsp_len = 0;
		frame_ind->reassoc_rsp = NULL;
	}

	qdf_mem_free(frame_ind);
}

int
target_if_cm_roam_sync_frame_event(ol_scn_t scn,
				   uint8_t *event,
				   uint32_t len)
{
	QDF_STATUS qdf_status;
	struct roam_synch_frame_ind *frame_ind_ptr;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	int status = 0;

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

	frame_ind_ptr = qdf_mem_malloc(sizeof(*frame_ind_ptr));
	if (!frame_ind_ptr)
		return -ENOMEM;

	qdf_status = wmi_extract_roam_sync_frame_event(wmi_handle, event,
						       len,
						       frame_ind_ptr);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		status = -EINVAL;
		goto err;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);

	if (!roam_rx_ops || !roam_rx_ops->roam_sync_frame_event) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		goto err;
	}

	qdf_status = roam_rx_ops->roam_sync_frame_event(psoc,
						    frame_ind_ptr);

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		status = -EINVAL;
		goto err;
	}
	qdf_mem_free(frame_ind_ptr);

	return 0;

err:
	target_if_free_roam_synch_frame_ind(frame_ind_ptr);

	return status;
}

int target_if_cm_roam_sync_event(ol_scn_t scn, uint8_t *event,
				 uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct roam_offload_synch_ind *sync_ind = NULL;
	int status = 0;

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

	target_if_prevent_pm_during_roam_sync(psoc);

	qdf_status = wmi_extract_roam_sync_event(wmi_handle, event,
						 len, &sync_ind);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		status = -EINVAL;
		goto err;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);

	if (!roam_rx_ops || !roam_rx_ops->roam_sync_event) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		goto err;
	}

	qdf_status = roam_rx_ops->roam_sync_event(psoc,
						  event,
						  len,
						  sync_ind);

	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

err:
	if (status == -EINVAL)
		target_if_allow_pm_after_roam_sync(psoc);

	if (sync_ind && sync_ind->ric_tspec_data)
		qdf_mem_free(sync_ind->ric_tspec_data);
	if (sync_ind)
		qdf_mem_free(sync_ind);
	return status;
}

static int
target_if_cm_btm_denylist_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct roam_denylist_event *dst_list = NULL;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;

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

	qdf_status = wmi_extract_btm_denylist_event(wmi_handle, event, len,
						    &dst_list);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	if (!dst_list) {
		/* No APs to denylist, just return */
		target_if_err_rl("No APs in denylist received");
		return 0;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->btm_denylist_event) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		goto done;
	}
	qdf_status = roam_rx_ops->btm_denylist_event(psoc, dst_list);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

done:
	qdf_mem_free(dst_list);
	return status;
}

int
target_if_cm_roam_vdev_disconnect_event_handler(ol_scn_t scn, uint8_t *event,
						uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct vdev_disconnect_event_data *data;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;

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

	data = qdf_mem_malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;
	qdf_status = wmi_extract_vdev_disconnect_event(wmi_handle, event, len,
						       data);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		goto done;
	}

	data->psoc = psoc;
	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->vdev_disconnect_event) {
		target_if_err("No valid roam rx ops");
		qdf_status = QDF_STATUS_E_INVAL;
		goto done;
	}

	/**
	 * This can be called from IRQ context for WOW events. There is no
	 * issue currently as this event is posted to scheduler thread from
	 * wma_handle_disconnect_reason(). Avoid acquiring mutex/sleep in this
	 * context in future and post a message to scheduler thread if needed.
	 */
	qdf_status = roam_rx_ops->vdev_disconnect_event(data);

done:
	qdf_mem_free(data);
	return qdf_status_to_os_return(qdf_status);
}

int
target_if_cm_roam_scan_chan_list_event_handler(ol_scn_t scn, uint8_t *event,
					       uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct cm_roam_scan_ch_resp *data = NULL;

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

	qdf_status = wmi_extract_roam_scan_chan_list(wmi_handle, event, len,
						     &data);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_scan_chan_list_event) {
		target_if_err("No valid roam rx ops");
		qdf_mem_free(data);
		return -EINVAL;
	}
	qdf_status = roam_rx_ops->roam_scan_chan_list_event(data);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

	return status;
}

int
target_if_cm_roam_stats_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct roam_stats_event *stats_info = NULL;

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

	qdf_status = wmi_extract_roam_stats_event(wmi_handle, event, len,
						  &stats_info);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_stats_event_rx) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		if (stats_info) {
			if (stats_info->roam_msg_info)
				qdf_mem_free(stats_info->roam_msg_info);
			qdf_mem_free(stats_info);
		}
		goto err;
	}

	qdf_status = roam_rx_ops->roam_stats_event_rx(psoc, stats_info);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

err:
	return status;
}

int
target_if_cm_roam_auth_offload_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct auth_offload_event auth_event = {0};

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

	qdf_status = wmi_extract_auth_offload_event(wmi_handle, event, len,
						    &auth_event);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_auth_offload_event) {
		target_if_err("No valid roam rx ops");
		return -EINVAL;
	}
	qdf_status = roam_rx_ops->roam_auth_offload_event(&auth_event);
	if (QDF_IS_STATUS_ERROR(status))
		status = -EINVAL;

	return status;
}

int
target_if_pmkid_request_event_handler(ol_scn_t scn, uint8_t *event,
				      uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct roam_pmkid_req_event *data = NULL;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;

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

	qdf_status = wmi_extract_roam_pmkid_request(wmi_handle, event, len,
						    &data);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		goto done;
	}

	data->psoc = psoc;

	roam_rx_ops = target_if_cm_get_roam_rx_ops(data->psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_pmkid_request_event_rx) {
		target_if_err("No valid roam rx ops");
		qdf_status = QDF_STATUS_E_INVAL;
		goto done;
	}

	/**
	 * This can be called from IRQ context for WOW events. There is no
	 * issue currently as this event doesn't take any mutex.
	 * If there is a mutex/sleep is needed in future, post a message to
	 * scheduler thread.
	 */
	qdf_status = roam_rx_ops->roam_pmkid_request_event_rx(data);

done:
	if (data)
		qdf_mem_free(data);
	return qdf_status_to_os_return(qdf_status);
}

int
target_if_roam_frame_event_handler(ol_scn_t scn, uint8_t *event,
				   uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct roam_scan_candidate_frame frame = {0};
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
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

	qdf_status = wmi_extract_roam_candidate_frame_event(wmi_handle, event,
							    len, &frame);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_candidate_frame_event) {
		target_if_err("No valid roam rx ops");
		return -EINVAL;
	}

	qdf_status = roam_rx_ops->roam_candidate_frame_event(psoc,
							     &frame);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return -EINVAL;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
int target_if_get_roam_vendor_control_param_event_handler(ol_scn_t scn,
							  uint8_t *event,
							  uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct roam_vendor_handoff_params *vendor_handoff_params = NULL;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	QDF_STATUS qdf_status;
	int ret = 0;

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

	qdf_status = wmi_extract_roam_vendor_control_param_event(wmi_handle,
							event, len,
							&vendor_handoff_params);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		ret = -EINVAL;
		goto done;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_vendor_handoff_event) {
		target_if_err("No valid roam rx ops");
		ret = -EINVAL;
		goto done;
	}

	roam_rx_ops->roam_vendor_handoff_event(psoc, vendor_handoff_params);

done:
	if (vendor_handoff_params)
		qdf_mem_free(vendor_handoff_params);

	return ret;
}

static QDF_STATUS
target_if_register_roam_vendor_control_param_event(wmi_unified_t handle)
{
	QDF_STATUS ret;

	ret = wmi_unified_register_event_handler(handle,
			wmi_get_roam_vendor_control_param_event_id,
			target_if_get_roam_vendor_control_param_event_handler,
			WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_get_roam_vendor_control_param_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
target_if_register_roam_vendor_control_param_event(wmi_unified_t handle)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_11BE_MLO)
static void
target_if_update_pairwise_key_peer_mac(struct wlan_crypto_key_entry *crypto_entry,
				       struct qdf_mac_addr *ap_link_addr)
{
	uint8_t i;

	if (crypto_entry->link_id == MLO_INVALID_LINK_IDX)
		return;

	for (i = 0; i < WLAN_CRYPTO_MAX_VLANKEYIX; i++) {
		if (!crypto_entry->keys.key[i])
			continue;

		if (crypto_entry->keys.key[i]->key_type ==
		    WLAN_CRYPTO_KEY_TYPE_UNICAST)
			qdf_copy_macaddr((struct qdf_mac_addr *)crypto_entry->keys.key[i]->macaddr,
					 ap_link_addr);
	}
}

static int
target_if_roam_synch_key_event_handler(ol_scn_t scn, uint8_t *event,
				       uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct wlan_crypto_key_entry *keys;
	struct qdf_mac_addr mld_addr;
	struct wlan_mlo_dev_context *ml_ctx = NULL;
	struct wlan_objmgr_vdev *vdev_list;
	struct mlo_link_info *link_info;
	uint8_t num_keys = 0;
	int ret = 0;
	QDF_STATUS status;
	uint8_t i, j;

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

	status = wmi_extract_roam_synch_key_event(wmi_handle, event, len, &keys,
						  &num_keys, &mld_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("parsing of roam sync key event failed");
		ret = -EINVAL;
		goto done;
	}

	wlan_mlo_get_mlpeer_by_peer_mladdr(&mld_addr, &ml_ctx);
	if (!ml_ctx) {
		target_if_err("ML context is not found mld addr: "
			      QDF_MAC_ADDR_FMT,
			      QDF_MAC_ADDR_REF(mld_addr.bytes));
		ret = -EINVAL;
		goto done;
	}

	target_if_debug("num_keys:%d ML context is found mld addr: "
			QDF_MAC_ADDR_FMT, num_keys,
			QDF_MAC_ADDR_REF(mld_addr.bytes));

	/*
	 * Fill VDEV ID & AP mac address for the pairwise keys
	 * from link id received in the key event
	 */
	for (i = 0; i < num_keys; i++) {
		keys[i].vdev_id = WLAN_INVALID_VDEV_ID;
		for (j = 0; j < WLAN_UMAC_MLO_MAX_VDEVS; j++) {
			vdev_list = ml_ctx->wlan_vdev_list[j];
			if (!vdev_list)
				continue;

			if (keys[i].link_id ==
			    wlan_vdev_get_link_id(vdev_list)) {
				keys[i].vdev_id = wlan_vdev_get_id(vdev_list);
				qdf_copy_macaddr((struct qdf_mac_addr *)keys[i].mac_addr.raw,
						 (struct qdf_mac_addr *)vdev_list->vdev_mlme.linkaddr);
				link_info = mlo_mgr_get_ap_link_by_link_id(
							vdev_list->mlo_dev_ctx,
							keys[i].link_id);
				if (!link_info) {
					target_if_err("Link info not found for link_id:%d",
						      keys[i].link_id);
					break;
				}
				target_if_debug("i:%d link_id:%d vdev_id:%d self link_addr: " QDF_MAC_ADDR_FMT " AP link addr: " QDF_MAC_ADDR_FMT,
						i, keys[i].link_id, keys[i].vdev_id,
						QDF_MAC_ADDR_REF(keys[i].mac_addr.raw),
						QDF_MAC_ADDR_REF(link_info->ap_link_addr.bytes));

				target_if_update_pairwise_key_peer_mac(&keys[i], &link_info->ap_link_addr);
				break;
			}
		}

		/* update for standby vdev also here from link_switch context*/
		if (keys[i].vdev_id == WLAN_INVALID_VDEV_ID &&
		    keys[i].link_id != MLO_INVALID_LINK_IDX &&
		    ml_ctx->link_ctx) {
			for (j = 0; j < WLAN_MAX_ML_BSS_LINKS; j++) {
				link_info = &ml_ctx->link_ctx->links_info[j];
				if (qdf_is_macaddr_zero(&link_info->ap_link_addr))
					continue;

				if (qdf_is_macaddr_zero(&link_info->link_addr))
					continue;

				if (link_info->link_id == keys[i].link_id) {
					target_if_debug("i:%d Standby vdev: link_id:%d ap_link_addr: " QDF_MAC_ADDR_FMT,
							i, keys[i].link_id,
							QDF_MAC_ADDR_REF(link_info->ap_link_addr.bytes));
					qdf_copy_macaddr((struct qdf_mac_addr *)keys[i].mac_addr.raw,
							 &link_info->link_addr);
					target_if_update_pairwise_key_peer_mac(&keys[i],
									       &link_info->ap_link_addr);
				}
			}
		}
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_sync_key_event) {
		target_if_err("No valid roam rx ops");
		ret = -EINVAL;
		goto done;
	}

	status = roam_rx_ops->roam_sync_key_event(psoc, keys, num_keys);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Add keys failed");
		ret = 0;
	}
done:
	qdf_mem_zero(keys, WLAN_MAX_ML_BSS_LINKS * sizeof(*keys));
	qdf_mem_free(keys);

	return ret;
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_11BE_MLO)
static void target_if_register_mlo_roam_events(wmi_unified_t handle)
{
	QDF_STATUS status;

	status = wmi_unified_register_event_handler(
				handle,
				wmi_roam_synch_key_event_id,
				target_if_roam_synch_key_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("wmi event(%u) registration failed, status: %d",
			      wmi_roam_synch_key_event_id, status);
}
#else
static inline void target_if_register_mlo_roam_events(wmi_unified_t handle)
{}
#endif

QDF_STATUS
target_if_roam_offload_register_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS ret;
	wmi_unified_t handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!handle) {
		target_if_err("handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* Register for roam offload event */
	ret = wmi_unified_register_event_handler(handle,
						 wmi_roam_synch_event_id,
						 target_if_cm_roam_sync_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	/* Register for roam offload event */
	ret = wmi_unified_register_event_handler(handle,
						 wmi_roam_synch_frame_event_id,
						 target_if_cm_roam_sync_frame_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
					wmi_roam_denylist_event_id,
					target_if_cm_btm_denylist_event,
					WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_denylist_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_vdev_disconnect_event_id,
				target_if_cm_roam_vdev_disconnect_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_vdev_disconnect_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_roam_scan_chan_list_id,
				target_if_cm_roam_scan_chan_list_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_scan_chan_list_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
						 wmi_roam_stats_event_id,
						 target_if_cm_roam_stats_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_roam_auth_offload_event_id,
				target_if_cm_roam_auth_offload_event,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_auth_offload_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_roam_pmkid_request_event_id,
				target_if_pmkid_request_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_stats_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	target_if_register_roam_vendor_control_param_event(handle);

	ret = wmi_unified_register_event_handler(handle,
				wmi_roam_frame_event_id,
				target_if_roam_frame_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_frame_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	target_if_register_mlo_roam_events(handle);

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
