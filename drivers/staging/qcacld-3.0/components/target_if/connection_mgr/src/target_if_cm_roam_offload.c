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
 * DOC: This file contains definitions for target_if roaming offload.
 */

#include "qdf_types.h"
#include "target_if_cm_roam_offload.h"
#include "target_if.h"
#include "wmi_unified_sta_api.h"
#include "wlan_mlme_dbg.h"
#include "wlan_mlme_api.h"
#include "wlan_crypto_global_api.h"
#include "wlan_mlme_main.h"
#include "wlan_cm_roam_api.h"
#include <target_if_vdev_mgr_rx_ops.h>
#include <target_if_vdev_mgr_tx_ops.h>
#include "target_if_cm_roam_event.h"
#include <target_if_psoc_wake_lock.h>
#include "wlan_psoc_mlme_api.h"

static struct wmi_unified
*target_if_cm_roam_get_wmi_handle_from_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;
	struct wmi_unified *wmi_handle;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		target_if_err("PDEV is NULL");
		return NULL;
	}

	wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return NULL;
	}

	return wmi_handle;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * target_if_cm_roam_send_vdev_set_pcl_cmd  - Send set vdev pcl
 * command to wmi.
 * @vdev: VDEV object pointer
 * @req:  Pointer to the pcl request msg
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_vdev_set_pcl_cmd(struct wlan_objmgr_vdev *vdev,
					struct set_pcl_req *req)
{
	wmi_unified_t wmi_handle;
	struct set_pcl_cmd_params params;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	params.weights = &req->chan_weights;
	params.vdev_id = req->vdev_id;

	return wmi_unified_vdev_set_pcl_cmd(wmi_handle, &params);
}

/**
 * target_if_cm_roam_send_roam_invoke_cmd  - Send roam invoke command to wmi.
 * @vdev: VDEV object pointer
 * @req:  Pointer to the roam invoke request msg
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_roam_invoke_cmd(struct wlan_objmgr_vdev *vdev,
				       struct roam_invoke_req *req)
{
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_roam_invoke_cmd(wmi_handle, req);
}

/**
 * target_if_cm_roam_send_roam_sync_complete  - Send roam sync complete to wmi.
 * @vdev: VDEV object pointer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_roam_sync_complete(struct wlan_objmgr_vdev *vdev)
{
	wmi_unified_t wmi_handle;
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	status = wmi_unified_roam_synch_complete_cmd(wmi_handle,
						     wlan_vdev_get_id(vdev));
	target_if_allow_pm_after_roam_sync(psoc);

	return status;
}

/**
 * target_if_roam_set_param() - set roam params in fw
 * @wmi_handle: wmi handle
 * @vdev_id: vdev id
 * @param_id: parameter id
 * @param_value: parameter value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
target_if_roam_set_param(wmi_unified_t wmi_handle, uint8_t vdev_id,
			 uint32_t param_id, uint32_t param_value)
{
	struct vdev_set_params roam_param = {0};

	roam_param.vdev_id = vdev_id;
	roam_param.param_id = param_id;
	roam_param.param_value = param_value;

	return wmi_unified_roam_set_param_send(wmi_handle, &roam_param);
}

/**
 * target_if_cm_roam_rt_stats_config() - Send enable/disable roam event stats
 * commands to wmi
 * @vdev: vdev object
 * @vdev_id: vdev id
 * @rstats_config: roam event stats config parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_rt_stats_config(struct wlan_objmgr_vdev *vdev,
				  uint8_t vdev_id, uint8_t rstats_config)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return status;

	status = target_if_roam_set_param(wmi_handle,
					  vdev_id,
					  WMI_ROAM_PARAM_ROAM_EVENTS_CONFIG,
					  rstats_config);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set "
			      "WMI_ROAM_PARAM_ROAM_EVENTS_CONFIG");

	return status;
}

/**
 * target_if_cm_roam_mcc_disallow() - Send enable/disable roam mcc disallow
 * commands to wmi
 * @vdev: vdev object
 * @vdev_id: vdev id
 * @is_mcc_disallowed: is mcc disallowed
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_mcc_disallow(struct wlan_objmgr_vdev *vdev,
			       uint8_t vdev_id, uint8_t is_mcc_disallowed)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return status;

	status = target_if_roam_set_param(wmi_handle,
					  vdev_id,
					  WMI_ROAM_PARAM_ROAM_MCC_DISALLOW,
					  is_mcc_disallowed);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set roam mcc disallow");

	return status;
}

#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
/**
 * target_if_cm_roam_linkspeed_state() - Send link speed state for roaming
 * commands to wmi
 * @vdev: vdev object
 * @vdev_id: vdev id
 * @is_linkspeed_good: true, don't need low rssi roaming
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_linkspeed_state(struct wlan_objmgr_vdev *vdev,
				  uint8_t vdev_id, bool is_linkspeed_good)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return status;

	status = target_if_roam_set_param(wmi_handle,
					  vdev_id,
					  WMI_ROAM_PARAM_LINKSPEED_STATE,
					  is_linkspeed_good);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set WMI_ROAM_PARAM_LINKSPEED_STATE");

	return status;
}
#else
static inline QDF_STATUS
target_if_cm_roam_linkspeed_state(struct wlan_objmgr_vdev *vdev,
				  uint8_t vdev_id, bool is_linkspeed_good)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
/**
 * target_if_cm_roam_vendor_handoff_config() - Send vendor handoff config
 * command to fw
 * @vdev: vdev object
 * @vdev_id: vdev id
 * @param_id: param id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_vendor_handoff_config(struct wlan_objmgr_vdev *vdev,
					uint8_t vdev_id, uint32_t param_id)
{
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_roam_vendor_handoff_req_cmd(wmi_handle,
						       vdev_id, param_id);
}

/**
 * target_if_cm_roam_register_vendor_handoff_ops() - Register tx ops to send
 * vendor handoff config command to fw
 * @tx_ops: structure of tx function pointers for roaming related commands
 *
 * Return: none
 */
static void target_if_cm_roam_register_vendor_handoff_ops(
					struct wlan_cm_roam_tx_ops *tx_ops)
{
	tx_ops->send_roam_vendor_handoff_config =
				target_if_cm_roam_vendor_handoff_config;
}
#else
static inline void target_if_cm_roam_register_vendor_handoff_ops(
					struct wlan_cm_roam_tx_ops *tx_ops)
{
}
#endif

#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
/**
 * target_if_cm_roam_register_linkspeed_state() - Register tx ops to send
 * roam link speed state command to fw
 * @tx_ops: structure of tx function pointers for roaming related commands
 *
 * Return: none
 */
static inline void
target_if_cm_roam_register_linkspeed_state(struct wlan_cm_roam_tx_ops *tx_ops)
{
	tx_ops->send_roam_linkspeed_state =
				target_if_cm_roam_linkspeed_state;
}
#else
static inline void
target_if_cm_roam_register_linkspeed_state(struct wlan_cm_roam_tx_ops *tx_ops)
{
}
#endif

/**
 * target_if_cm_roam_ho_delay_config() - Send roam HO delay value to wmi
 * @vdev: vdev object
 * @vdev_id: vdev id
 * @roam_ho_delay: roam hand-off delay value
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_ho_delay_config(struct wlan_objmgr_vdev *vdev,
				  uint8_t vdev_id, uint16_t roam_ho_delay)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return status;

	status = target_if_roam_set_param(
				wmi_handle,
				vdev_id,
				WMI_ROAM_PARAM_ROAM_HO_DELAY_RUNTIME_CONFIG,
				roam_ho_delay);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set "
			      "WMI_ROAM_PARAM_ROAM_HO_DELAY_RUNTIME_CONFIG");

	return status;
}

/**
 * target_if_cm_exclude_rm_partial_scan_freq() - Indicate to FW whether to
 * exclude the channels in roam full scan that are already scanned as part of
 * partial scan or not.
 * @vdev: vdev object
 * @exclude_rm_partial_scan_freq: Include/exclude the channels in roam full scan
 * that are already scanned as part of partial scan.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_exclude_rm_partial_scan_freq(struct wlan_objmgr_vdev *vdev,
					  uint8_t exclude_rm_partial_scan_freq)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t vdev_id;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return status;

	vdev_id = wlan_vdev_get_id(vdev);
	status = target_if_roam_set_param(
				wmi_handle, vdev_id,
				WMI_ROAM_PARAM_ROAM_CONTROL_FULL_SCAN_CHANNEL_OPTIMIZATION,
				exclude_rm_partial_scan_freq);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set WMI_ROAM_PARAM_ROAM_CONTROL_FULL_SCAN_CHANNEL_OPTIMIZATION");

	return status;
}

/**
 * target_if_cm_roam_full_scan_6ghz_on_disc() - Indicate to FW whether to
 * include the 6 GHz channels in roam full scan only on prior discovery of any
 * 6 GHz support in the environment or by default.
 * @vdev: vdev object
 * @roam_full_scan_6ghz_on_disc: Include the 6 GHz channels in roam full scan:
 * 1 - Include only on prior discovery of any 6 GHz support in the environment
 * 0 - Include all the supported 6 GHz channels by default
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_full_scan_6ghz_on_disc(struct wlan_objmgr_vdev *vdev,
					 uint8_t roam_full_scan_6ghz_on_disc)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t vdev_id;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return status;

	vdev_id = wlan_vdev_get_id(vdev);
	status = target_if_roam_set_param(wmi_handle, vdev_id,
					  WMI_ROAM_PARAM_ROAM_CONTROL_FULL_SCAN_6GHZ_PSC_ONLY_WITH_RNR,
					  roam_full_scan_6ghz_on_disc);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set WMI_ROAM_PARAM_ROAM_CONTROL_FULL_SCAN_6GHZ_PSC_ONLY_WITH_RNR");

	return status;
}

/**
 * target_if_cm_roam_rssi_diff_6ghz() - Send the roam RSSI diff value to FW
 * which is used to decide how better the RSSI of the new/roamable 6GHz AP
 * should be for roaming.
 * @vdev: vdev object
 * @roam_rssi_diff_6ghz: RSSI diff value to be used for roaming to 6 GHz AP
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_rssi_diff_6ghz(struct wlan_objmgr_vdev *vdev,
				 uint8_t roam_rssi_diff_6ghz)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t vdev_id;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return status;

	vdev_id = wlan_vdev_get_id(vdev);
	status = target_if_roam_set_param(
				wmi_handle, vdev_id,
				WMI_ROAM_PARAM_ROAM_RSSI_BOOST_FOR_6GHZ_CAND_AP,
				roam_rssi_diff_6ghz);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set WMI_ROAM_PARAM_ROAM_RSSI_BOOST_FOR_6GHZ_CAND_AP");

	return status;
}

static void
target_if_cm_roam_register_lfr3_ops(struct wlan_cm_roam_tx_ops *tx_ops)
{
	tx_ops->send_vdev_set_pcl_cmd = target_if_cm_roam_send_vdev_set_pcl_cmd;
	tx_ops->send_roam_invoke_cmd = target_if_cm_roam_send_roam_invoke_cmd;
	tx_ops->send_roam_sync_complete_cmd = target_if_cm_roam_send_roam_sync_complete;
	tx_ops->send_roam_rt_stats_config = target_if_cm_roam_rt_stats_config;
	tx_ops->send_roam_ho_delay_config = target_if_cm_roam_ho_delay_config;
	tx_ops->send_roam_mcc_disallow = target_if_cm_roam_mcc_disallow;
	tx_ops->send_exclude_rm_partial_scan_freq =
				target_if_cm_exclude_rm_partial_scan_freq;
	tx_ops->send_roam_full_scan_6ghz_on_disc =
				target_if_cm_roam_full_scan_6ghz_on_disc;
	target_if_cm_roam_register_vendor_handoff_ops(tx_ops);
	target_if_cm_roam_register_linkspeed_state(tx_ops);
}
#else
static inline void
target_if_cm_roam_register_lfr3_ops(struct wlan_cm_roam_tx_ops *tx_ops)
{}

static QDF_STATUS
target_if_cm_roam_rt_stats_config(struct wlan_objmgr_vdev *vdev,
				  uint8_t vdev_id, uint8_t rstats_config)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static QDF_STATUS
target_if_cm_roam_ho_delay_config(struct wlan_objmgr_vdev *vdev,
				  uint8_t vdev_id, uint16_t roam_ho_delay)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static QDF_STATUS
target_if_cm_roam_mcc_disallow(struct wlan_objmgr_vdev *vdev,
			       uint8_t vdev_id, uint8_t is_mcc_disallowed)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static QDF_STATUS
target_if_cm_exclude_rm_partial_scan_freq(struct wlan_objmgr_vdev *vdev,
					  uint8_t exclude_rm_partial_scan_freq)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static QDF_STATUS
target_if_cm_roam_full_scan_6ghz_on_disc(struct wlan_objmgr_vdev *vdev,
					 uint8_t roam_full_scan_6ghz_on_disc)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static QDF_STATUS
target_if_cm_roam_rssi_diff_6ghz(struct wlan_objmgr_vdev *vdev,
				 uint8_t roam_rssi_diff_6ghz)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * target_if_is_vdev_valid - vdev id is valid or not
 * @vdev_id: vdev id
 *
 * Return: true or false
 */
static bool target_if_is_vdev_valid(uint8_t vdev_id)
{
	return (vdev_id < WLAN_MAX_VDEVS ? true : false);
}

/**
 * target_if_vdev_set_param() - set per vdev params in fw
 * @wmi_handle: wmi handle
 * @vdev_id: vdev id
 * @param_id: parameter id
 * @param_value: parameter value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
target_if_vdev_set_param(wmi_unified_t wmi_handle, uint32_t vdev_id,
			 uint32_t param_id, uint32_t param_value)
{
	struct vdev_set_params param = {0};

	if (!target_if_is_vdev_valid(vdev_id)) {
		target_if_err("vdev_id: %d is invalid, reject the req: param id %d val %d",
			      vdev_id, param_id, param_value);
		return QDF_STATUS_E_INVAL;
	}

	param.vdev_id = vdev_id;
	param.param_id = param_id;
	param.param_value = param_value;

	return wmi_unified_vdev_set_param_send(wmi_handle, &param);
}

static QDF_STATUS target_if_cm_roam_scan_offload_mode(
			wmi_unified_t wmi_handle,
			struct wlan_roam_scan_offload_params *rso_mode_cfg)
{
	return wmi_unified_roam_scan_offload_mode_cmd(wmi_handle,
						      rso_mode_cfg);
}

static
QDF_STATUS target_if_check_index_setparam(struct dev_set_param *param,
					  uint32_t paramid,
					  uint32_t paramvalue,
					  uint8_t index, uint8_t n_params)
{
	if (index >= n_params) {
		target_if_err("Index:%d OOB to fill param", index);
		return QDF_STATUS_E_FAILURE;
	}
	param[index].param_id = paramid;
	param[index].param_value = paramvalue;
	return QDF_STATUS_SUCCESS;
}

#define MAX_PARAMS_CM_ROAM_SCAN_BMISS 2
/*
 * params being sent:
 * wmi_vdev_param_bmiss_first_bcnt
 * wmi_vdev_param_bmiss_final_bcnt
 */

/**
 * target_if_cm_roam_scan_bmiss_cnt() - set bmiss count to fw
 * @wmi_handle: wmi handle
 * @req: bmiss count parameters
 *
 * Set first & final bmiss count to fw.
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_scan_bmiss_cnt(wmi_unified_t wmi_handle,
				 struct wlan_roam_beacon_miss_cnt *req)
{
	QDF_STATUS status;
	struct dev_set_param setparam[MAX_PARAMS_CM_ROAM_SCAN_BMISS];
	struct set_multiple_pdev_vdev_param params = {};
	uint8_t index = 0;

	target_if_debug("vdev_id:%d, first_bcnt: %d, final_bcnt: %d",
			req->vdev_id, req->roam_bmiss_first_bcnt,
			req->roam_bmiss_final_bcnt);

	status = target_if_check_index_setparam(
					   setparam,
					   wmi_vdev_param_bmiss_first_bcnt,
					   req->roam_bmiss_first_bcnt,
					   index++,
					   MAX_PARAMS_CM_ROAM_SCAN_BMISS);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	status = target_if_check_index_setparam(
					   setparam,
					   wmi_vdev_param_bmiss_final_bcnt,
					   req->roam_bmiss_final_bcnt, index++,
					   MAX_PARAMS_CM_ROAM_SCAN_BMISS);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	params.param_type = MLME_VDEV_SETPARAM;
	params.dev_id = req->vdev_id;
	params.n_params = index;
	params.params = setparam;

	status = wmi_unified_multiple_vdev_param_send(wmi_handle, &params);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("failed to set bmiss first,final bcntset params");

error:
	return status;
}

#define MAX_PARAMS_CM_ROAM_SCAN_BMISS_TIMEOUT 2
/*
 * params being sent:
 * wmi_vdev_param_bmiss_first_bcnt
 * wmi_vdev_param_bmiss_final_bcnt
 */

/**
 * target_if_cm_roam_scan_bmiss_timeout() - set conbmiss timeout to fw
 * @wmi_handle: wmi handle
 * @req: bmiss timeout parameters
 *
 * Set bmiss timeout to fw.
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_scan_bmiss_timeout(wmi_unified_t wmi_handle,
				     struct wlan_roam_bmiss_timeout *req)
{
	QDF_STATUS status;
	uint32_t vdev_id;
	uint8_t bmiss_timeout_onwakeup;
	uint8_t bmiss_timeout_onsleep;
	struct dev_set_param setparam[MAX_PARAMS_CM_ROAM_SCAN_BMISS_TIMEOUT];
	struct set_multiple_pdev_vdev_param params = {};
	uint8_t index = 0;

	vdev_id = req->vdev_id;
	bmiss_timeout_onwakeup = req->bmiss_timeout_onwakeup;
	bmiss_timeout_onsleep = req->bmiss_timeout_onsleep;

	target_if_debug("vdev_id %d bmiss_timeout_onwakeup: %dsec, bmiss_timeout_onsleep: %dsec", vdev_id,
			bmiss_timeout_onwakeup, bmiss_timeout_onsleep);
	status = target_if_check_index_setparam(
					setparam,
					wmi_vdev_param_final_bmiss_time_sec,
					req->bmiss_timeout_onwakeup, index++,
					MAX_PARAMS_CM_ROAM_SCAN_BMISS_TIMEOUT);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	status = target_if_check_index_setparam(
					setparam,
					wmi_vdev_param_final_bmiss_time_wow_sec,
					req->bmiss_timeout_onsleep, index++,
					MAX_PARAMS_CM_ROAM_SCAN_BMISS_TIMEOUT);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	params.param_type = MLME_VDEV_SETPARAM;
	params.dev_id = req->vdev_id;
	params.n_params = index;
	params.params = setparam;
	status = wmi_unified_multiple_vdev_param_send(wmi_handle, &params);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("failed to set bmiss first,final bcntset params");

error:
	return status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * target_if_cm_roam_reason_vsie() - set vdev param
 * wmi_vdev_param_enable_disable_roam_reason_vsie
 * @wmi_handle: handle to WMI
 * @req: roam reason vsie enable parameters
 *
 * Return: void
 */
static void
target_if_cm_roam_reason_vsie(wmi_unified_t wmi_handle,
			      struct wlan_roam_reason_vsie_enable *req)
{
	QDF_STATUS status;

	status = target_if_vdev_set_param(
				wmi_handle,
				req->vdev_id,
				wmi_vdev_param_enable_disable_roam_reason_vsie,
				req->enable_roam_reason_vsie);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set vdev param %d",
			      wmi_vdev_param_enable_disable_roam_reason_vsie);
}

/**
 * target_if_cm_roam_triggers() - send roam triggers to WMI
 * @vdev: vdev
 * @req: roam triggers parameters
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_triggers(struct wlan_objmgr_vdev *vdev,
			   struct wlan_roam_triggers *req)
{
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	if (!target_if_is_vdev_valid(req->vdev_id))
		return QDF_STATUS_E_INVAL;

	return wmi_unified_set_roam_triggers(wmi_handle, req);
}

/**
 * target_if_cm_roam_scan_get_cckm_mode() - Get the CCKM auth mode
 * @vdev: vdev object
 * @auth_mode: Auth mode to be converted
 *
 * Based on LFR2.0 or LFR3.0, return the proper auth type
 *
 * Return: if LFR2.0, then return WMI_AUTH_CCKM for backward compatibility
 *         if LFR3.0 then return the appropriate auth type
 */
static uint32_t
target_if_cm_roam_scan_get_cckm_mode(struct wlan_objmgr_vdev *vdev,
				     uint32_t auth_mode)
{
	struct wlan_objmgr_psoc *psoc;
	bool roam_offload_enable;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return WMI_AUTH_CCKM;
	}

	wlan_mlme_get_roaming_offload(psoc, &roam_offload_enable);
	if (roam_offload_enable)
		return auth_mode;
	else
		return WMI_AUTH_CCKM;
}

/* target_if_cm_roam_disconnect_params(): Send the disconnect roam parameters
 * to wmi
 * @wmi_handle: handle to WMI
 * @command: rso command
 * @req: disconnect roam parameters
 *
 * Return: void
 */
static void
target_if_cm_roam_disconnect_params(wmi_unified_t wmi_handle, uint8_t command,
				    struct wlan_roam_disconnect_params *req)
{
	QDF_STATUS status;

	switch (command) {
	case ROAM_SCAN_OFFLOAD_START:
	case ROAM_SCAN_OFFLOAD_UPDATE_CFG:
		if (!req->enable)
			return;
		break;
	case ROAM_SCAN_OFFLOAD_STOP:
		req->enable = false;
		break;
	default:
		break;
	}

	status = wmi_unified_send_disconnect_roam_params(wmi_handle, req);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("failed to send disconnect roam parameters");
}

/* target_if_cm_roam_idle_params(): Send the roam idle parameters to wmi
 * @wmi_handle: handle to WMI
 * @command: rso command
 * @req: roam idle parameters
 *
 * Return: void
 */
static void
target_if_cm_roam_idle_params(wmi_unified_t wmi_handle, uint8_t command,
			      struct wlan_roam_idle_params *req)
{
	QDF_STATUS status;
	bool db2dbm_enabled;

	switch (command) {
	case ROAM_SCAN_OFFLOAD_START:
	case ROAM_SCAN_OFFLOAD_UPDATE_CFG:
		break;
	case ROAM_SCAN_OFFLOAD_STOP:
		req->enable = false;
		break;
	default:
		break;
	}

	db2dbm_enabled = wmi_service_enabled(wmi_handle,
					     wmi_service_hw_db2dbm_support);
	if (!db2dbm_enabled) {
		req->conn_ap_min_rssi -= NOISE_FLOOR_DBM_DEFAULT;
		req->conn_ap_min_rssi &= 0x000000ff;
	}

	status = wmi_unified_send_idle_roam_params(wmi_handle, req);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("failed to send idle roam parameters");
}
#else
static void
target_if_cm_roam_reason_vsie(wmi_unified_t wmi_handle,
			      struct wlan_roam_reason_vsie_enable *req)
{
}

static QDF_STATUS
target_if_cm_roam_triggers(struct wlan_objmgr_vdev *vdev,
			   struct wlan_roam_triggers *req)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static uint32_t
target_if_cm_roam_scan_get_cckm_mode(struct wlan_objmgr_vdev *vdev,
				     uint32_t auth_mode)
{
	return WMI_AUTH_CCKM;
}

static void
target_if_cm_roam_disconnect_params(wmi_unified_t wmi_handle, uint8_t command,
				    struct wlan_roam_disconnect_params *req)
{
}

static void
target_if_cm_roam_idle_params(wmi_unified_t wmi_handle, uint8_t command,
			      struct wlan_roam_idle_params *req)
{
}
#endif

/**
 * target_if_cm_roam_scan_offload_rssi_thresh() - Send roam scan rssi threshold
 * commands to wmi
 * @wmi_handle: wmi handle
 * @req: roam scan rssi threshold related parameters
 *
 * This function fills some parameters @req and send down roam scan rssi
 * threshold command to wmi
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_scan_offload_rssi_thresh(
				wmi_unified_t wmi_handle,
				struct wlan_roam_offload_scan_rssi_params *req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool db2dbm_enabled;

	db2dbm_enabled = wmi_service_enabled(wmi_handle,
					     wmi_service_hw_db2dbm_support);
	if (!db2dbm_enabled) {
		req->rssi_thresh -= NOISE_FLOOR_DBM_DEFAULT;
		req->rssi_thresh &= 0x000000ff;
		req->hi_rssi_scan_rssi_ub -= NOISE_FLOOR_DBM_DEFAULT;
		req->bg_scan_bad_rssi_thresh -= NOISE_FLOOR_DBM_DEFAULT;
		req->roam_data_rssi_threshold -= NOISE_FLOOR_DBM_DEFAULT;
		req->good_rssi_threshold -= NOISE_FLOOR_DBM_DEFAULT;
		req->good_rssi_threshold &= 0x000000ff;
	}

	req->hi_rssi_scan_rssi_ub &= 0x000000ff;
	/*
	 * The current Noise floor in firmware is -96dBm. Penalty/Boost
	 * threshold is applied on a weaker signal to make it even more weaker.
	 * So, there is a chance that the user may configure a very low
	 * Penalty/Boost threshold beyond the noise floor. If that is the case,
	 * then suppress the penalty/boost threshold to the noise floor.
	 */
	if (req->raise_rssi_thresh_5g < NOISE_FLOOR_DBM_DEFAULT) {
		if (db2dbm_enabled) {
			req->penalty_threshold_5g = RSSI_MIN_VALUE;
			req->boost_threshold_5g = RSSI_MAX_VALUE;
		} else {
			req->penalty_threshold_5g = 0;
		}
	} else {
		if (db2dbm_enabled) {
			req->boost_threshold_5g = req->raise_rssi_thresh_5g;
		} else {
			req->boost_threshold_5g =
				(req->raise_rssi_thresh_5g -
					NOISE_FLOOR_DBM_DEFAULT) & 0x000000ff;
		}
	}

	if (req->drop_rssi_thresh_5g < NOISE_FLOOR_DBM_DEFAULT) {
		if (db2dbm_enabled)
			req->penalty_threshold_5g = RSSI_MIN_VALUE;
		else
			req->penalty_threshold_5g = 0;
	} else {
		if (db2dbm_enabled) {
			req->penalty_threshold_5g = req->drop_rssi_thresh_5g;
		} else {
			req->penalty_threshold_5g =
				(req->drop_rssi_thresh_5g -
					NOISE_FLOOR_DBM_DEFAULT) & 0x000000ff;
		}
	}

	if (req->early_stop_scan_enable) {
		if (!db2dbm_enabled) {
			req->roam_earlystop_thres_min -=
						NOISE_FLOOR_DBM_DEFAULT;
			req->roam_earlystop_thres_max -=
						NOISE_FLOOR_DBM_DEFAULT;
		}
	} else {
		if (db2dbm_enabled) {
			req->roam_earlystop_thres_min = RSSI_MIN_VALUE;
			req->roam_earlystop_thres_max = RSSI_MIN_VALUE;
		} else {
			req->roam_earlystop_thres_min = 0;
			req->roam_earlystop_thres_max = 0;
		}
	}

	target_if_debug("RSO_CFG: vdev %d: db2dbm enabled:%d, good_rssi_threshold:%d, early_stop_thresholds en:%d, min:%d, max:%d, roam_scan_rssi_thresh:%d, roam_rssi_thresh_diff:%d",
			req->vdev_id, db2dbm_enabled, req->good_rssi_threshold,
			req->early_stop_scan_enable,
			req->roam_earlystop_thres_min,
			req->roam_earlystop_thres_max, req->rssi_thresh,
			req->rssi_thresh_diff);
	target_if_debug("RSO_CFG: hirssi max cnt:%d, delta:%d, hirssi upper bound:%d, dense rssi thresh offset:%d, dense min aps cnt:%d, traffic_threshold:%d, dense_status:%d",
			req->hi_rssi_scan_max_count,
			req->hi_rssi_scan_rssi_delta,
			req->hi_rssi_scan_rssi_ub,
			req->dense_rssi_thresh_offset,
			req->dense_min_aps_cnt,
			req->traffic_threshold,
			req->initial_dense_status);
	target_if_debug("RSO_CFG: raise rssi threshold 5g:%d, drop rssi threshold 5g:%d, penalty threshold 5g:%d, boost threshold 5g:%d",
			req->raise_rssi_thresh_5g,
			req->drop_rssi_thresh_5g,
			req->penalty_threshold_5g,
			req->boost_threshold_5g);
	target_if_debug("RSO_CFG: raise factor 5g:%d, drop factor 5g:%d, max raise rssi 5g:%d, max drop rssi 5g:%d, rssi threshold offset 5g:%d",
			req->raise_factor_5g,
			req->raise_factor_5g,
			req->max_raise_rssi_5g,
			req->max_drop_rssi_5g,
			req->rssi_thresh_offset_5g);
	target_if_debug("RSO_CFG: BG Scan Bad RSSI:%d, bitmap:0x%x Offset for 2G to 5G Roam:%d",
			req->bg_scan_bad_rssi_thresh,
			req->bg_scan_client_bitmap,
			req->roam_bad_rssi_thresh_offset_2g);
	target_if_debug("RSO_CFG: Roam data rssi triggers:0x%x, threshold:%d, rx time:%d",
			req->roam_data_rssi_threshold_triggers,
			req->roam_data_rssi_threshold,
			req->rx_data_inactivity_time);

	status = wmi_unified_roam_scan_offload_rssi_thresh_cmd(wmi_handle, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("roam_scan_offload_rssi_thresh_cmd failed %d",
			      status);
		return status;
	}

	return status;
}

/**
 * target_if_cm_roam_scan_offload_scan_period() - set roam offload scan period
 * @wmi_handle: wmi handle
 * @req:  roam scan period parameters
 *
 * Send WMI_ROAM_SCAN_PERIOD parameters to fw.
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_scan_offload_scan_period(
				wmi_unified_t wmi_handle,
				struct wlan_roam_scan_period_params *req)
{
	if (!target_if_is_vdev_valid(req->vdev_id)) {
		target_if_err("Invalid vdev id:%d", req->vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_roam_scan_offload_scan_period(wmi_handle, req);
}

/**
 * target_if_cm_roam_scan_offload_ap_profile() - send roam ap profile to
 * firmware
 * @vdev: vdev object
 * @wmi_handle: wmi handle
 * @req: roam ap profile parameters
 *
 * Send WMI_ROAM_AP_PROFILE parameters to firmware
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_scan_offload_ap_profile(
				struct wlan_objmgr_vdev *vdev,
				wmi_unified_t wmi_handle,
				struct ap_profile_params *req)
{
	uint32_t rsn_authmode;
	bool db2dbm_enabled;

	if (!target_if_is_vdev_valid(req->vdev_id)) {
		target_if_err("Invalid vdev id:%d", req->vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	rsn_authmode = req->profile.rsn_authmode;
	if (rsn_authmode == WMI_AUTH_CCKM_WPA ||
	    rsn_authmode == WMI_AUTH_CCKM_RSNA)
		req->profile.rsn_authmode =
		target_if_cm_roam_scan_get_cckm_mode(vdev, rsn_authmode);

	db2dbm_enabled = wmi_service_enabled(wmi_handle,
					     wmi_service_hw_db2dbm_support);
	if (!req->profile.rssi_abs_thresh) {
		if (db2dbm_enabled)
			req->profile.rssi_abs_thresh = RSSI_MIN_VALUE;
	} else {
		if (!db2dbm_enabled)
			req->profile.rssi_abs_thresh -=
						NOISE_FLOOR_DBM_DEFAULT;
	}

	if (!db2dbm_enabled) {
		req->min_rssi_params[DEAUTH_MIN_RSSI].min_rssi -=
						NOISE_FLOOR_DBM_DEFAULT;
		req->min_rssi_params[DEAUTH_MIN_RSSI].min_rssi &= 0x000000ff;

		req->min_rssi_params[BMISS_MIN_RSSI].min_rssi -=
						NOISE_FLOOR_DBM_DEFAULT;
		req->min_rssi_params[BMISS_MIN_RSSI].min_rssi &= 0x000000ff;

		req->min_rssi_params[MIN_RSSI_2G_TO_5G_ROAM].min_rssi -=
						NOISE_FLOOR_DBM_DEFAULT;
		req->min_rssi_params[MIN_RSSI_2G_TO_5G_ROAM].min_rssi &=
						0x000000ff;

	}

	return wmi_unified_send_roam_scan_offload_ap_cmd(wmi_handle, req);
}

/**
 * target_if_cm_roam_scan_mawc_params() - send roam macw to
 * firmware
 * @wmi_handle: wmi handle
 * @req: roam macw parameters
 *
 * Send WMI_ROAM_CONFIGURE_MAWC_CMDID parameters to firmware
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_scan_mawc_params(wmi_unified_t wmi_handle,
				   struct wlan_roam_mawc_params *req)
{
	if (!wmi_service_enabled(wmi_handle, wmi_service_hw_db2dbm_support))
		req->best_ap_rssi_threshold -= NOISE_FLOOR_DBM_DEFAULT;

	return wmi_unified_roam_mawc_params_cmd(wmi_handle, req);
}

/**
 * target_if_cm_roam_scan_filter() - send roam scan filter to firmware
 * @wmi_handle: wmi handle
 * @command: rso command
 * @req: roam scan filter parameters
 *
 * Send WMI_ROAM_FILTER_CMDID parameters to firmware
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_scan_filter(wmi_unified_t wmi_handle, uint8_t command,
			      struct wlan_roam_scan_filter_params *req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!target_if_is_vdev_valid(req->filter_params.vdev_id)) {
		target_if_err("Invalid vdev id:%d",
			      req->filter_params.vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (command != ROAM_SCAN_OFFLOAD_STOP) {
		switch (req->reason) {
		case REASON_ROAM_SET_DENYLIST_BSSID:
		case REASON_ROAM_SET_SSID_ALLOWED:
		case REASON_ROAM_SET_FAVORED_BSSID:
			break;
		case REASON_CTX_INIT:
			if (command == ROAM_SCAN_OFFLOAD_START) {
				req->filter_params.op_bitmap |=
				ROAM_FILTER_OP_BITMAP_LCA_DISALLOW |
				ROAM_FILTER_OP_BITMAP_RSSI_REJECTION_OCE;
			} else {
				target_if_debug("Roam Filter need not be sent");
				return QDF_STATUS_SUCCESS;
			}
			break;
		default:
			if (command != ROAM_SCAN_OFFLOAD_START) {
				target_if_debug("Roam Filter need not be sent");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	target_if_debug("RSO_CFG: vdev %d op_bitmap:0x%x num_rssi_rejection_ap:%d delta_rssi:%d",
			req->filter_params.vdev_id,
			req->filter_params.op_bitmap,
			req->filter_params.num_rssi_rejection_ap,
			req->filter_params.delta_rssi);
	status = wmi_unified_roam_scan_filter_cmd(wmi_handle,
						  &req->filter_params);
	return status;
}

/**
 * target_if_cm_roam_scan_btm_offload() - send roam scan btm offload to firmware
 * @wmi_handle: wmi handle
 * @req: roam scan btm offload parameters
 *
 * Send WMI_ROAM_BTM_CONFIG_CMDID parameters to firmware
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_scan_btm_offload(wmi_unified_t wmi_handle,
				   struct wlan_roam_btm_config *req)
{
	return wmi_unified_send_btm_config(wmi_handle, req);
}

/**
 * target_if_cm_roam_offload_11k_params() - send 11k offload params to firmware
 * @wmi_handle: wmi handle
 * @req: 11k offload parameters
 *
 * Send WMI_11K_OFFLOAD_REPORT_CMDID parameters to firmware
 *
 * Return: QDF status
 */
static QDF_STATUS
target_if_cm_roam_offload_11k_params(wmi_unified_t wmi_handle,
				     struct wlan_roam_11k_offload_params *req)
{
	QDF_STATUS status;

	if (!wmi_service_enabled(wmi_handle,
				 wmi_service_11k_neighbour_report_support)) {
		target_if_err("FW doesn't support 11k offload");
		return QDF_STATUS_SUCCESS;
	}

	/* If 11k enable command and ssid length is 0, drop it */
	if (req->offload_11k_bitmask &&
	    !req->neighbor_report_params.ssid.length) {
		target_if_debug("SSID Len 0");
		return QDF_STATUS_SUCCESS;
	}

	status = wmi_unified_offload_11k_cmd(wmi_handle, req);

	if (status != QDF_STATUS_SUCCESS)
		target_if_err("failed to send 11k offload command");

	return status;
}

/**
 * target_if_cm_roam_bss_load_config() - send bss load config params to firmware
 * @wmi_handle: wmi handle
 * @req: bss load config parameters
 *
 * Send WMI_ROAM_BSS_LOAD_CONFIG_CMDID parameters to firmware
 *
 * Return: QDF status
 */
static void
target_if_cm_roam_bss_load_config(wmi_unified_t wmi_handle,
				  struct wlan_roam_bss_load_config *req)
{
	QDF_STATUS status;
	bool db2dbm_enabled;

	db2dbm_enabled = wmi_service_enabled(wmi_handle,
					     wmi_service_hw_db2dbm_support);
	if (!db2dbm_enabled) {
		req->rssi_threshold_6ghz -= NOISE_FLOOR_DBM_DEFAULT;
		req->rssi_threshold_6ghz &= 0x000000ff;

		req->rssi_threshold_5ghz -= NOISE_FLOOR_DBM_DEFAULT;
		req->rssi_threshold_5ghz &= 0x000000ff;

		req->rssi_threshold_24ghz -= NOISE_FLOOR_DBM_DEFAULT;
		req->rssi_threshold_24ghz &= 0x000000ff;
	}

	target_if_debug("RSO_CFG: bss load trig params vdev_id:%u threshold:%u sample_time:%u 5Ghz RSSI threshold:%d 2.4G rssi threshold:%d",
			req->vdev_id, req->bss_load_threshold,
			req->bss_load_sample_time, req->rssi_threshold_5ghz,
			req->rssi_threshold_24ghz);

	status = wmi_unified_send_bss_load_config(wmi_handle, req);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("failed to send bss load trigger config command");
}

static uint32_t
target_if_get_wmi_roam_offload_flag(uint32_t flag)
{
	uint32_t roam_offload_flag = 0;

	if (flag & WLAN_ROAM_FW_OFFLOAD_ENABLE)
		roam_offload_flag |= WMI_ROAM_FW_OFFLOAD_ENABLE_FLAG;

	if (flag & WLAN_ROAM_BMISS_FINAL_SCAN_ENABLE)
		roam_offload_flag |= WMI_ROAM_BMISS_FINAL_SCAN_ENABLE_FLAG;

	if (flag & WLAN_ROAM_SKIP_EAPOL_4WAY_HANDSHAKE)
		roam_offload_flag |=
			wmi_vdev_param_skip_roam_eapol_4way_handshake;

	if (flag & WLAN_ROAM_BMISS_FINAL_SCAN_TYPE)
		roam_offload_flag |= WMI_ROAM_BMISS_FINAL_SCAN_TYPE_FLAG;

	if (flag & WLAN_ROAM_SKIP_SAE_ROAM_4WAY_HANDSHAKE)
		roam_offload_flag |=
				wmi_vdev_param_skip_sae_roam_4way_handshake;

	return roam_offload_flag;
}

/**
 * target_if_cm_roam_send_roam_init  - Send roam module init/deinit to firmware
 * @vdev:  Pointer to Objmgr vdev
 * @params: Roam offload init params
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_roam_init(struct wlan_objmgr_vdev *vdev,
				 struct wlan_roam_offload_init_params *params)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle;
	uint32_t flag;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	flag = target_if_get_wmi_roam_offload_flag(params->roam_offload_flag);
	status = target_if_vdev_set_param(wmi_handle, params->vdev_id,
					  wmi_vdev_param_roam_fw_offload, flag);

	return status;
}

/**
 * target_if_cm_roam_scan_rssi_change_cmd()  - Send WMI_ROAM_SCAN_RSSI_CHANGE
 * command to firmware
 * @wmi_handle: WMI handle
 * @params: RSSI change parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS target_if_cm_roam_scan_rssi_change_cmd(
			wmi_unified_t wmi_handle,
			struct wlan_roam_rssi_change_params *params)
{
	/*
	 * Start new rssi triggered scan only if it changes by
	 * RoamRssiDiff value. Beacon weight of 14 means average rssi
	 * is taken over 14 previous samples + 2 times the current
	 * beacon's rssi.
	 */
	return wmi_unified_roam_scan_offload_rssi_change_cmd(wmi_handle,
							     params);
}

/**
 * target_if_cm_roam_offload_chan_list  - Send WMI_ROAM_CHAN_LIST command to
 * firmware
 * @wmi_handle: Pointer to wmi handle
 * @rso_chan_info: RSO channel list info
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS target_if_cm_roam_offload_chan_list(
		wmi_unified_t wmi_handle,
		struct wlan_roam_scan_channel_list *rso_chan_info)
{
	return wmi_unified_roam_scan_offload_chan_list_cmd(wmi_handle,
							   rso_chan_info);
}

/**
 * target_if_cm_roam_send_time_sync_cmd  - Send time of the day in millisecs
 * to firmware.
 * @wmi_handle: WMI handle
 *
 * Return: None
 */
static void
target_if_cm_roam_send_time_sync_cmd(wmi_unified_t wmi_handle)
{
	return wmi_send_time_stamp_sync_cmd_tlv(wmi_handle);
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * target_if_cm_roam_send_mlo_config() - Send roam mlo related commands
 * to wmi
 * @vdev: vdev object
 * @req: roam mlo config parameters
 *
 * This function is used to send roam mlo related commands to wmi
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_mlo_config(struct wlan_objmgr_vdev *vdev,
				  struct wlan_roam_mlo_config *req)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	status = wmi_unified_roam_mlo_config_cmd(wmi_handle, req);

	if (status != QDF_STATUS_SUCCESS)
		target_if_err("failed to send WMI_ROAM_MLO_CONFIG_CMDID command");

	return status;
}

static void
target_if_cm_roam_register_mlo_req_ops(struct wlan_cm_roam_tx_ops *tx_ops)
{
	tx_ops->send_roam_mlo_config = target_if_cm_roam_send_mlo_config;
}
#else
static QDF_STATUS
target_if_cm_roam_send_mlo_config(struct wlan_objmgr_vdev *vdev,
				  struct wlan_roam_mlo_config *req)
{
	return QDF_STATUS_SUCCESS;
}

static void
target_if_cm_roam_register_mlo_req_ops(struct wlan_cm_roam_tx_ops *tx_ops)
{
}
#endif

/**
 * target_if_cm_roam_send_start() - Send roam start related commands
 * to wmi
 * @vdev: vdev object
 * @req: roam start config parameters
 *
 * This function is used to send roam start related commands to wmi
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_start(struct wlan_objmgr_vdev *vdev,
			     struct wlan_roam_start_config *req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	wmi_unified_t wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	bool bss_load_enabled;
	bool eht_capab = false;
	bool is_mcc_disallowed;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	target_if_cm_roam_send_time_sync_cmd(wmi_handle);

	status = target_if_cm_roam_scan_offload_rssi_thresh(
							wmi_handle,
							&req->rssi_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Sending roam scan offload rssi thresh failed");
		goto end;
	}

	status = target_if_cm_roam_scan_bmiss_cnt(wmi_handle,
						  &req->beacon_miss_cnt);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev set bmiss bcnt param failed");
		goto end;
	}
	status = target_if_cm_roam_scan_bmiss_timeout(wmi_handle,
						      &req->bmiss_timeout);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev set bmiss timeout param failed");
		goto end;
	}

	target_if_cm_roam_reason_vsie(wmi_handle, &req->reason_vsie_enable);

	target_if_cm_roam_triggers(vdev, &req->roam_triggers);

	/* Opportunistic scan runs on a timer, value set by
	 * empty_scan_refresh_period. Age out the entries after 3 such
	 * cycles.
	 */
	if (req->scan_period_params.empty_scan_refresh_period > 0) {
		status = target_if_cm_roam_scan_offload_scan_period(
						wmi_handle,
						&req->scan_period_params);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;
	}

	status = target_if_cm_roam_scan_rssi_change_cmd(
			wmi_handle, &req->rssi_change_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev:%d Sending rssi change threshold failed",
			      req->rssi_change_params.vdev_id);
		goto end;
	}

	status = target_if_cm_roam_scan_offload_ap_profile(
							vdev, wmi_handle,
							&req->profile_params);
	if (QDF_IS_STATUS_ERROR(status))
		goto end;

	status = target_if_cm_roam_offload_chan_list(wmi_handle,
						     &req->rso_chan_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev:%d Send channel list command failed",
			      req->rso_chan_info.vdev_id);
		goto end;
	}

	if (wmi_service_enabled(wmi_handle, wmi_service_mawc_support)) {
		status = target_if_cm_roam_scan_mawc_params(wmi_handle,
							    &req->mawc_params);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Sending roaming MAWC params failed");
			goto end;
		}
	} else {
		target_if_debug("MAWC roaming not supported by firmware");
	}

	status = target_if_cm_roam_scan_offload_mode(wmi_handle,
						     &req->rso_config);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev:%d Send RSO mode cmd failed",
			      req->rso_config.vdev_id);
		goto end;
	}

	status = target_if_cm_roam_scan_filter(wmi_handle,
					       ROAM_SCAN_OFFLOAD_START,
					       &req->scan_filter_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Sending start for roam scan filter failed");
		goto end;
	}

	status = target_if_cm_roam_scan_btm_offload(wmi_handle,
						    &req->btm_config);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Sending BTM config to fw failed");
		goto end;
	}

	/*
	 * Send 11k offload enable and bss load trigger parameters
	 * to FW as part of RSO Start
	 */
	status = target_if_cm_roam_offload_11k_params(wmi_handle,
						      &req->roam_11k_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("11k offload enable not sent, status %d", status);
		goto end;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	wlan_mlme_get_bss_load_enabled(psoc, &bss_load_enabled);
	if (bss_load_enabled)
		target_if_cm_roam_bss_load_config(wmi_handle,
						  &req->bss_load_config);

	target_if_cm_roam_disconnect_params(wmi_handle, ROAM_SCAN_OFFLOAD_START,
					    &req->disconnect_params);

	target_if_cm_roam_idle_params(wmi_handle, ROAM_SCAN_OFFLOAD_START,
				      &req->idle_params);
	wlan_psoc_mlme_get_11be_capab(psoc, &eht_capab);
	if (eht_capab)
		target_if_cm_roam_send_mlo_config(vdev, &req->roam_mlo_params);

	vdev_id = wlan_vdev_get_id(vdev);
	if (req->wlan_roam_rt_stats_config)
		target_if_cm_roam_rt_stats_config(vdev, vdev_id,
						req->wlan_roam_rt_stats_config);

	if (req->wlan_roam_ho_delay_config)
		target_if_cm_roam_ho_delay_config(
				vdev, vdev_id, req->wlan_roam_ho_delay_config);

	if (req->wlan_exclude_rm_partial_scan_freq)
		target_if_cm_exclude_rm_partial_scan_freq(
				vdev, req->wlan_exclude_rm_partial_scan_freq);

	if (req->wlan_roam_full_scan_6ghz_on_disc)
		target_if_cm_roam_full_scan_6ghz_on_disc(
				vdev, req->wlan_roam_full_scan_6ghz_on_disc);

	is_mcc_disallowed = !wlan_cm_same_band_sta_allowed(psoc);
	target_if_cm_roam_mcc_disallow(vdev, vdev_id, is_mcc_disallowed);

	if (req->wlan_roam_rssi_diff_6ghz)
		target_if_cm_roam_rssi_diff_6ghz(vdev,
						 req->wlan_roam_rssi_diff_6ghz);

	/* add other wmi commands */
end:
	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS
target_if_start_rso_stop_timer(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	struct wlan_lmac_if_mlme_rx_ops *rx_ops;
	struct vdev_response_timer *vdev_rsp;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_id = wlan_vdev_get_id(vdev);
	rx_ops = target_if_vdev_mgr_get_rx_ops(psoc);
	if (!rx_ops || !rx_ops->psoc_get_vdev_response_timer_info) {
		mlme_err("VEV_%d: PSOC_%d No Rx Ops", vdev_id,
			 wlan_psoc_get_id(psoc));
		return QDF_STATUS_E_INVAL;
	}

	vdev_rsp = rx_ops->psoc_get_vdev_response_timer_info(psoc, vdev_id);
	if (!vdev_rsp) {
		mlme_err("VDEV_%d: PSOC_%d No vdev rsp timer", vdev_id,
			 wlan_psoc_get_id(psoc));
		return QDF_STATUS_E_INVAL;
	}

	vdev_rsp->expire_time = RSO_STOP_RESPONSE_TIMER;

	return target_if_vdev_mgr_rsp_timer_start(psoc, vdev_rsp,
						  RSO_STOP_RESPONSE_BIT);
}

QDF_STATUS
target_if_stop_rso_stop_timer(struct roam_offload_roam_event *roam_event)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct vdev_response_timer *vdev_rsp;
	struct wlan_lmac_if_mlme_rx_ops *rx_ops;

	roam_event->rso_timer_stopped = false;
	rx_ops = target_if_vdev_mgr_get_rx_ops(roam_event->psoc);
	if (!rx_ops || !rx_ops->vdev_mgr_start_response) {
		mlme_err("No Rx Ops");
		return QDF_STATUS_E_INVAL;
	}
	vdev_rsp = rx_ops->psoc_get_vdev_response_timer_info(roam_event->psoc,
							roam_event->vdev_id);
	if (!vdev_rsp) {
		mlme_err("vdev response timer is null VDEV_%d PSOC_%d",
			 roam_event->vdev_id,
			 wlan_psoc_get_id(roam_event->psoc));
		return QDF_STATUS_E_INVAL;
	}

	if (!qdf_atomic_test_bit(RSO_STOP_RESPONSE_BIT,
				 &vdev_rsp->rsp_status)) {
		mlme_debug("rso stop timer is not started");
		return QDF_STATUS_SUCCESS;
	}

	if ((roam_event->reason == ROAM_REASON_RSO_STATUS &&
	     (roam_event->notif == CM_ROAM_NOTIF_SCAN_MODE_SUCCESS ||
	      roam_event->notif == CM_ROAM_NOTIF_SCAN_MODE_FAIL)) ||
	    roam_event->reason == ROAM_REASON_HO_FAILED) {
		status = target_if_vdev_mgr_rsp_timer_stop(roam_event->psoc,
					vdev_rsp, RSO_STOP_RESPONSE_BIT);
		if (QDF_IS_STATUS_SUCCESS(status))
			roam_event->rso_timer_stopped = true;
		else
			mlme_err("PSOC_%d VDEV_%d: VDE MGR RSP Timer stop failed",
				 roam_event->psoc->soc_objmgr.psoc_id,
				 roam_event->vdev_id);
	} else if (roam_event->reason == ROAM_REASON_RSO_STATUS &&
		   roam_event->notif == CM_ROAM_NOTIF_HO_FAIL) {
		mlme_debug("HO_FAIL happened, wait for HO_FAIL event vdev_id: %u",
			   roam_event->vdev_id);
	}

	return status;
}

#else
static inline QDF_STATUS
target_if_start_rso_stop_timer(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
target_if_cm_send_rso_stop_failure_rsp(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id)
{
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct roam_offload_roam_event roam_event = {0};

	roam_event.vdev_id = vdev_id;
	roam_event.psoc = psoc;
	roam_event.reason = ROAM_REASON_RSO_STATUS;
	roam_event.notif = CM_ROAM_NOTIF_SCAN_MODE_FAIL;
	roam_event.rso_timer_stopped = true;

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_event_rx) {
		target_if_err("No valid roam rx ops");
		return QDF_STATUS_E_INVAL;
	}
	roam_rx_ops->roam_event_rx(&roam_event);

	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS
target_if_cm_roam_abort_rso_stop_timer(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id)
{
	struct vdev_response_timer *vdev_rsp;
	struct wlan_lmac_if_mlme_rx_ops *rx_ops;

	rx_ops = target_if_vdev_mgr_get_rx_ops(psoc);
	if (!rx_ops || !rx_ops->vdev_mgr_start_response) {
		mlme_err("No Rx Ops");
		return QDF_STATUS_E_INVAL;
	}
	vdev_rsp = rx_ops->psoc_get_vdev_response_timer_info(psoc, vdev_id);
	if (!vdev_rsp) {
		mlme_err("vdev response timer is null VDEV_%d PSOC_%d",
			 vdev_id, wlan_psoc_get_id(psoc));
		return QDF_STATUS_E_INVAL;
	}

	return target_if_vdev_mgr_rsp_timer_stop(psoc, vdev_rsp,
						 RSO_STOP_RESPONSE_BIT);
}

/**
 * target_if_cm_roam_send_stop() - Send roam stop related commands
 * to wmi
 * @vdev: vdev object
 * @req: roam stop config parameters
 *
 * This function is used to send roam stop related commands to wmi
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_stop(struct wlan_objmgr_vdev *vdev,
			    struct wlan_roam_stop_config *req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS timer_start_status = QDF_STATUS_E_NOSUPPORT;
	QDF_STATUS rso_stop_status = QDF_STATUS_E_INVAL;
	wmi_unified_t wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Send 11k offload disable command to FW as part of RSO Stop */
	status = target_if_cm_roam_offload_11k_params(wmi_handle,
						      &req->roam_11k_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("11k offload disable not sent, status %d",
			      status);
		goto end;
	}

	/* Send BTM config as disabled during RSO Stop */
	status = target_if_cm_roam_scan_btm_offload(wmi_handle,
						    &req->btm_config);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Sending BTM config to fw failed");
		goto end;
	}

	if (req->start_rso_stop_timer)
		timer_start_status = target_if_start_rso_stop_timer(vdev);

	rso_stop_status = target_if_cm_roam_scan_offload_mode(wmi_handle,
							      &req->rso_config);
	if (QDF_IS_STATUS_ERROR(rso_stop_status)) {
		target_if_err("vdev:%d Send RSO mode cmd failed",
			      req->rso_config.vdev_id);
		goto end;
	}

	/*
	 * After sending the roam scan mode because of a disconnect,
	 * clear the scan bitmap client as well by sending
	 * the following command
	 */
	target_if_cm_roam_scan_offload_rssi_thresh(wmi_handle,
						   &req->rssi_params);

	/*
	 * If the STOP command is due to a disconnect, then
	 * send the filter command to clear all the filter
	 * entries. If it is roaming scenario, then do not
	 * send the cleared entries.
	 */
	if (!req->middle_of_roaming) {
		status = target_if_cm_roam_scan_filter(
					wmi_handle, ROAM_SCAN_OFFLOAD_STOP,
					&req->scan_filter_params);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("clear for roam scan filter failed");
			goto end;
		}
	}

	target_if_cm_roam_disconnect_params(wmi_handle, ROAM_SCAN_OFFLOAD_STOP,
					    &req->disconnect_params);

	target_if_cm_roam_idle_params(wmi_handle, ROAM_SCAN_OFFLOAD_STOP,
				      &req->idle_params);
	/*
	 * Disable all roaming triggers if RSO stop is as part of
	 * disconnect
	 */
	vdev_id = wlan_vdev_get_id(vdev);
	if (req->rso_config.rso_mode_info.roam_scan_mode ==
	    WMI_ROAM_SCAN_MODE_NONE) {
		req->roam_triggers.vdev_id = vdev_id;
		req->roam_triggers.trigger_bitmap = 0;
		req->roam_triggers.roam_scan_scheme_bitmap = 0;
		target_if_cm_roam_triggers(vdev, &req->roam_triggers);
	}
end:
	if (QDF_IS_STATUS_SUCCESS(timer_start_status)) {
		if (QDF_IS_STATUS_SUCCESS(rso_stop_status)) {
			/*
			 * Started the timer and send RSO stop to firmware
			 * successfully. Wait for RSO STOP response from fw.
			 */
			req->send_rso_stop_resp = false;
		} else {
			/*
			 * Started the timer and but failed to send RSO stop to
			 * firmware. Stop the timer and let the response be
			 * poseted from CM.
			 */
			target_if_cm_roam_abort_rso_stop_timer(psoc,
						wlan_vdev_get_id(vdev));
		}
	}

	return status;
}

/**
 * target_if_cm_roam_send_update_config() - Send roam update config related
 * commands to wmi
 * @vdev: vdev object
 * @req: roam update config parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_update_config(struct wlan_objmgr_vdev *vdev,
				     struct wlan_roam_update_config *req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	wmi_unified_t wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	bool is_mcc_disallowed;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	status = target_if_cm_roam_scan_bmiss_cnt(wmi_handle,
						  &req->beacon_miss_cnt);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev set bmiss bcnt param failed");
		goto end;
	}

	status = target_if_cm_roam_scan_bmiss_timeout(wmi_handle,
						      &req->bmiss_timeout);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev set bmiss timeout param failed");
		goto end;
	}

	status = target_if_cm_roam_scan_filter(wmi_handle,
					       ROAM_SCAN_OFFLOAD_UPDATE_CFG,
					       &req->scan_filter_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Sending update for roam scan filter failed");
		goto end;
	}

	status = target_if_cm_roam_scan_offload_rssi_thresh(
							wmi_handle,
							&req->rssi_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Sending roam scan offload rssi thresh failed");
		goto end;
	}

	if (req->scan_period_params.empty_scan_refresh_period > 0) {
		status = target_if_cm_roam_scan_offload_scan_period(
						wmi_handle,
						&req->scan_period_params);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;
	}

	status = target_if_cm_roam_scan_rssi_change_cmd(
			wmi_handle, &req->rssi_change_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev:%d Sending rssi change threshold failed",
			      req->rssi_change_params.vdev_id);
		goto end;
	}

	status = target_if_cm_roam_scan_offload_ap_profile(
							vdev, wmi_handle,
							&req->profile_params);
	if (QDF_IS_STATUS_ERROR(status))
		goto end;

	status = target_if_cm_roam_offload_chan_list(wmi_handle,
						     &req->rso_chan_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("vdev:%d Send channel list command failed",
			      req->rso_chan_info.vdev_id);
		goto end;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = wlan_vdev_get_id(vdev);

	if (MLME_IS_ROAM_STATE_RSO_ENABLED(psoc, vdev_id)) {
		status = target_if_cm_roam_scan_offload_mode(wmi_handle,
							     &req->rso_config);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("vdev:%d Send RSO mode cmd failed",
				      req->rso_config.vdev_id);
			goto end;
		}

		target_if_cm_roam_disconnect_params(
				wmi_handle, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
				&req->disconnect_params);

		target_if_cm_roam_idle_params(
				wmi_handle, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
				&req->idle_params);
		target_if_cm_roam_triggers(vdev, &req->roam_triggers);

		if (req->wlan_roam_rt_stats_config)
			target_if_cm_roam_rt_stats_config(
						vdev, vdev_id,
						req->wlan_roam_rt_stats_config);

		if (req->wlan_roam_ho_delay_config)
			target_if_cm_roam_ho_delay_config(
						vdev, vdev_id,
						req->wlan_roam_ho_delay_config);

		if (req->wlan_exclude_rm_partial_scan_freq)
			target_if_cm_exclude_rm_partial_scan_freq(
				vdev, req->wlan_exclude_rm_partial_scan_freq);

		if (req->wlan_roam_full_scan_6ghz_on_disc)
			target_if_cm_roam_full_scan_6ghz_on_disc(
				vdev, req->wlan_roam_full_scan_6ghz_on_disc);

		is_mcc_disallowed = !wlan_cm_same_band_sta_allowed(psoc);
		target_if_cm_roam_mcc_disallow(vdev, vdev_id,
					       is_mcc_disallowed);

		if (req->wlan_roam_rssi_diff_6ghz)
			target_if_cm_roam_rssi_diff_6ghz(
					vdev, req->wlan_roam_rssi_diff_6ghz);
	}
end:
	return status;
}

/**
 * target_if_cm_roam_abort() - Send roam abort to wmi
 * @vdev: vdev object
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_abort(struct wlan_objmgr_vdev *vdev, uint8_t vdev_id)
{
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	if (!target_if_is_vdev_valid(vdev_id)) {
		target_if_err("Invalid vdev id:%d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	return wmi_unified_roam_scan_offload_cmd(wmi_handle,
						 WMI_ROAM_SCAN_STOP_CMD,
						 vdev_id);
}

/**
 * target_if_cm_roam_per_config() - Send roam per config related
 * commands to wmi
 * @vdev: vdev object
 * @req: roam per config parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_per_config(struct wlan_objmgr_vdev *vdev,
			     struct wlan_per_roam_config_req *req)
{
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_set_per_roam_config(wmi_handle, req);
}

/**
 * target_if_cm_roam_send_disable_config() - Send roam disable config related
 * commands to wmi
 * @vdev: vdev object
 * @req: roam disable config parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_cm_roam_send_disable_config(struct wlan_objmgr_vdev *vdev,
				      struct roam_disable_cfg *req)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	wmi_unified_t wmi_handle;
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		target_if_err("Failed to get vdev mlme obj!");
		goto end;
	}

	if (vdev_mlme->mgmt.generic.type != WMI_VDEV_TYPE_STA ||
	    vdev_mlme->mgmt.generic.subtype != 0) {
		target_if_err("This isn't a STA: %d", req->vdev_id);
		goto end;
	}

	wmi_handle = target_if_cm_roam_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		goto end;

	status = target_if_vdev_set_param(
				wmi_handle,
				req->vdev_id,
				wmi_vdev_param_roam_11kv_ctrl,
				req->cfg);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to set wmi_vdev_param_roam_11kv_ctrl");

end:
	return status;
}

/**
 * target_if_cm_roam_register_rso_req_ops() - Register rso req tx ops functions
 * @tx_ops: tx ops
 *
 * This function is used to register rso req tx ops functions
 *
 * Return: none
 */
static void
target_if_cm_roam_register_rso_req_ops(struct wlan_cm_roam_tx_ops *tx_ops)
{
	tx_ops->send_roam_offload_init_req = target_if_cm_roam_send_roam_init;
	tx_ops->send_roam_start_req = target_if_cm_roam_send_start;
	tx_ops->send_roam_stop_offload = target_if_cm_roam_send_stop;
	tx_ops->send_roam_update_config = target_if_cm_roam_send_update_config;
	tx_ops->send_roam_abort = target_if_cm_roam_abort;
	tx_ops->send_roam_per_config = target_if_cm_roam_per_config;
	tx_ops->send_roam_triggers = target_if_cm_roam_triggers;
	tx_ops->send_roam_disable_config =
					target_if_cm_roam_send_disable_config;
	target_if_cm_roam_register_mlo_req_ops(tx_ops);
}

QDF_STATUS target_if_cm_roam_register_tx_ops(struct wlan_cm_roam_tx_ops *tx_ops)
{
	if (!tx_ops) {
		target_if_err("target if tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	target_if_cm_roam_register_lfr3_ops(tx_ops);
	target_if_cm_roam_register_rso_req_ops(tx_ops);

	return QDF_STATUS_SUCCESS;
}
