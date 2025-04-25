/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 * DOC: contains nud event tracking main function definitions
 */

#include "osif_sync.h"
#include "wlan_hdd_main.h"
#include "wlan_dp_ucfg_api.h"
#include "wlan_dlm_ucfg_api.h"
#include "hdd_dp_cfg.h"
#include <cdp_txrx_misc.h>
#include "wlan_cm_roam_ucfg_api.h"
#include "wlan_hdd_nud_tracking.h"

static void
hdd_handle_nud_fail_sta(struct hdd_context *hdd_ctx,
			struct hdd_adapter *adapter)
{
	struct reject_ap_info ap_info;
	struct hdd_station_ctx *sta_ctx;
	struct qdf_mac_addr bssid;

	if (hdd_is_roaming_in_progress(hdd_ctx)) {
		hdd_debug("Roaming already in progress, cannot trigger roam.");
		return;
	}

	hdd_debug("nud fail detected, try roaming to better BSSID, vdev id: %d",
		  adapter->deflink->vdev_id);

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter->deflink);

	qdf_mem_zero(&ap_info, sizeof(struct reject_ap_info));
	ap_info.bssid = sta_ctx->conn_info.bssid;
	ap_info.reject_ap_type = DRIVER_AVOID_TYPE;
	ap_info.reject_reason = REASON_NUD_FAILURE;
	ap_info.source = ADDED_BY_DRIVER;
	ucfg_dlm_add_bssid_to_reject_list(hdd_ctx->pdev, &ap_info);

	if (roaming_offload_enabled(hdd_ctx)) {
		qdf_zero_macaddr(&bssid);
		ucfg_wlan_cm_roam_invoke(hdd_ctx->pdev,
					 adapter->deflink->vdev_id,
					 &bssid, 0, CM_ROAMING_NUD_FAILURE);
	}
}

static void
hdd_handle_nud_fail_non_sta(struct wlan_hdd_link_info *link_info)
{
	wlan_hdd_cm_issue_disconnect(link_info,
				     REASON_GATEWAY_REACHABILITY_FAILURE,
				     false);
}

/**
 * __hdd_nud_failure_work() - work for nud event
 * @adapter: HDD adapter
 *
 * Return: None
 */
static void
__hdd_nud_failure_work(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	int status;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	hdd_enter();

	status = hdd_validate_adapter(adapter);
	if (status)
		return;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return;

	if (!hdd_cm_is_vdev_associated(adapter->deflink)) {
		hdd_debug("Not in Connected State");
		return;
	}

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_debug("wlan is suspended, ignore NUD failure event");
		return;
	}

	if (soc && ucfg_dp_nud_fail_data_stall_evt_enabled()) {
		hdd_dp_err("Data stall due to NUD failure");
		cdp_post_data_stall_event
			(soc,
			 DATA_STALL_LOG_INDICATOR_HOST_DRIVER,
			 DATA_STALL_LOG_NUD_FAILURE,
			 OL_TXRX_PDEV_ID, 0XFF,
			 DATA_STALL_LOG_RECOVERY_TRIGGER_PDR);
	}

	if (adapter->device_mode == QDF_STA_MODE &&
	    ucfg_dp_is_roam_after_nud_enabled(hdd_ctx->psoc)) {
		hdd_handle_nud_fail_sta(hdd_ctx, adapter);
		return;
	}
	hdd_handle_nud_fail_non_sta(adapter->deflink);

	hdd_exit();
}

void hdd_nud_failure_work(hdd_cb_handle context, qdf_netdev_t netdev)
{
	struct hdd_adapter *adapter;
	struct osif_vdev_sync *vdev_sync;

	adapter = WLAN_HDD_GET_PRIV_PTR(netdev);
	if (!adapter) {
		hdd_err("adapter is null");
		return;
	}

	if (osif_vdev_sync_op_start(adapter->dev, &vdev_sync))
		return;

	__hdd_nud_failure_work(adapter);

	osif_vdev_sync_op_stop(vdev_sync);
}
