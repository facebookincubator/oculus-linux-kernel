/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_nan.c
 *
 * WLAN Host Device Driver NAN API implementation
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/cfg80211.h>
#include <ani_global.h>
#include "sme_api.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_nan.h"
#include "osif_sync.h"
#include <qca_vendor.h>
#include "cfg_nan_api.h"
#include "os_if_nan.h"
#include "../../core/src/nan_main_i.h"
#include "spatial_reuse_api.h"
#include "wlan_nan_api.h"
#include "spatial_reuse_ucfg_api.h"

/**
 * wlan_hdd_nan_is_supported() - HDD NAN support query function
 * @hdd_ctx: Pointer to hdd context
 *
 * This function is called to determine if NAN is supported by the
 * driver and by the firmware.
 *
 * Return: true if NAN is supported by the driver and firmware
 */
bool wlan_hdd_nan_is_supported(struct hdd_context *hdd_ctx)
{
	return cfg_nan_get_enable(hdd_ctx->psoc) &&
		sme_is_feature_supported_by_fw(NAN);
}

/**
 * __wlan_hdd_cfg80211_nan_ext_request() - cfg80211 NAN extended request handler
 * @wiphy: driver's wiphy struct
 * @wdev: wireless device to which the request is targeted
 * @data: actual request data (netlink-encapsulated)
 * @data_len: length of @data
 *
 * Handles NAN Extended vendor commands, sends the command to NAN component
 * which parses and forwards the NAN requests.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_nan_ext_request(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	int ret_val;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	hdd_enter_dev(wdev->netdev);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err_rl("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (!wlan_hdd_nan_is_supported(hdd_ctx)) {
		hdd_debug_rl("NAN is not supported");
		return -EPERM;
	}

	if (hdd_is_connection_in_progress(NULL, NULL)) {
		hdd_err("Connection refused: conn in progress");
		return -EAGAIN;
	}

	return os_if_process_nan_req(hdd_ctx->pdev, adapter->deflink->vdev_id,
				     data, data_len);
}

int wlan_hdd_cfg80211_nan_ext_request(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)

{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_nan_ext_request(wiphy, wdev,
						    data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

void hdd_nan_concurrency_update(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	int ret;

	hdd_enter();
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return;

	wlan_twt_concurrency_update(hdd_ctx);
	hdd_exit();
}

#ifdef WLAN_FEATURE_NAN
#ifdef WLAN_FEATURE_SR
void hdd_nan_sr_concurrency_update(struct nan_event_params *nan_evt)
{
	struct wlan_objmgr_vdev *sta_vdev = NULL;
	uint32_t conc_vdev_id = WLAN_INVALID_VDEV_ID;
	uint8_t sr_ctrl;
	bool is_sr_enabled = false;
	uint32_t sta_vdev_id = WLAN_INVALID_VDEV_ID;
	uint8_t sta_cnt, i;
	uint32_t conn_count;
	uint8_t non_srg_max_pd_offset = 0;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {
							WLAN_INVALID_VDEV_ID};
	struct nan_psoc_priv_obj *psoc_obj =
				nan_get_psoc_priv_obj(nan_evt->psoc);
	struct connection_info info[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t mac_id;
	QDF_STATUS status;

	if (!psoc_obj) {
		nan_err("nan psoc priv object is NULL");
		return;
	}
	conn_count = policy_mgr_get_connection_info(nan_evt->psoc, info);
	if (!conn_count)
		return;
	sta_cnt = policy_mgr_get_mode_specific_conn_info(nan_evt->psoc, NULL,
							 vdev_id_list,
							 PM_STA_MODE);
	/*
	 * Get all active sta vdevs. STA + STA SR concurrency is not supported
	 * so break whenever a first sta with SR enabled is found.
	 */
	for (i = 0; i < sta_cnt; i++) {
		if (vdev_id_list[i] != WLAN_INVALID_VDEV_ID) {
			sta_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						    nan_evt->psoc,
						    vdev_id_list[i],
						    WLAN_OSIF_ID);
			if (!sta_vdev) {
				nan_err("sta vdev invalid for vdev id %d",
					vdev_id_list[i]);
				continue;
			}
			ucfg_spatial_reuse_get_sr_config(
					sta_vdev, &sr_ctrl,
					&non_srg_max_pd_offset, &is_sr_enabled);
			if (is_sr_enabled) {
				sta_vdev_id = vdev_id_list[i];
				break;
			}
			wlan_objmgr_vdev_release_ref(sta_vdev, WLAN_OSIF_ID);
		}
	}
	if (sta_cnt && sta_vdev &&
	    (!(sr_ctrl & NON_SRG_PD_SR_DISALLOWED) ||
	    (sr_ctrl & SRG_INFO_PRESENT)) &&
	     is_sr_enabled) {
		if (nan_evt->evt_type == nan_event_id_enable_rsp) {
			wlan_vdev_mlme_set_sr_disable_due_conc(
					sta_vdev, true);
			wlan_spatial_reuse_osif_event(
						sta_vdev, SR_OPERATION_SUSPEND,
						SR_REASON_CODE_CONCURRENCY);
		}
		if (nan_evt->evt_type == nan_event_id_disable_ind) {
			if (conn_count > 2) {
				status =
				policy_mgr_get_mac_id_by_session_id(
					nan_evt->psoc, sta_vdev_id,
					&mac_id);
				if (QDF_IS_STATUS_ERROR(status)) {
					hdd_err("get mac id failed");
					goto exit;
				}
				conc_vdev_id =
				policy_mgr_get_conc_vdev_on_same_mac(
					nan_evt->psoc, sta_vdev_id,
					mac_id);
				/*
				 * Don't enable SR, if concurrent vdev is not
				 * NAN and SR concurrency on same mac is not
				 * allowed.
				 */
				if (conc_vdev_id != WLAN_INVALID_VDEV_ID &&
				    !policy_mgr_sr_same_mac_conc_enabled(
				    nan_evt->psoc)) {
					hdd_debug("don't enable SR in SCC/MCC");
					goto exit;
				}
			}
			wlan_vdev_mlme_set_sr_disable_due_conc(sta_vdev, false);
			wlan_spatial_reuse_osif_event(
						sta_vdev, SR_OPERATION_RESUME,
						SR_REASON_CODE_CONCURRENCY);
		}
	}
exit:
	if (sta_vdev && is_sr_enabled)
		wlan_objmgr_vdev_release_ref(sta_vdev, WLAN_OSIF_ID);
}
#endif
#endif
