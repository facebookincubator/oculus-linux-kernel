/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains interface manager public api
 */
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_if_mgr_ap.h"
#include "wlan_if_mgr_roam.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_if_mgr_main.h"
#include "wlan_p2p_cfg_api.h"
#include "wlan_tdls_api.h"
#include "wlan_p2p_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_vdev_mgr_utils_api.h"
#include "wlan_tdls_tgt_api.h"
#include "wlan_policy_mgr_ll_sap.h"

QDF_STATUS if_mgr_ap_start_bss(struct wlan_objmgr_vdev *vdev,
			       struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	wlan_tdls_notify_start_bss(psoc, vdev);

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE ||
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE)
		wlan_handle_emlsr_sta_concurrency(psoc, true, false);

	if (policy_mgr_is_hw_mode_change_in_progress(psoc)) {
		if (!QDF_IS_STATUS_SUCCESS(
		    policy_mgr_wait_for_connection_update(psoc))) {
			ifmgr_err("qdf wait for event failed!!");
			return QDF_STATUS_E_FAILURE;
		}
	}
	if (policy_mgr_is_chan_switch_in_progress(psoc)) {
		status = policy_mgr_wait_chan_switch_complete_evt(psoc);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			ifmgr_err("qdf wait for csa event failed!!");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (policy_mgr_is_sta_active_connection_exists(psoc))
		/* Disable Roaming on all vdev's before starting bss */
		if_mgr_disable_roaming(pdev, vdev, RSO_START_BSS);

	/* abort p2p roc before starting the BSS for sync event */
	ucfg_p2p_cleanup_roc_by_psoc(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
if_mgr_ap_start_bss_complete(struct wlan_objmgr_vdev *vdev,
			     struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Due to audio share glitch with P2P GO caused by
	 * roam scan on concurrent interface, disable
	 * roaming if "p2p_disable_roam" ini is enabled.
	 * Donot re-enable roaming again on other STA interface
	 * if p2p GO is active on any vdev.
	 */
	if (cfg_p2p_is_roam_config_disabled(psoc) &&
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE) {
		ifmgr_debug("p2p go mode, keep roam disabled");
	} else {
		/* Enable Roaming after start bss in case of failure/success */
		if_mgr_enable_roaming(pdev, vdev, RSO_START_BSS);
	}
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE)
		policy_mgr_check_sap_go_force_scc(psoc, vdev,
						  CSA_REASON_GO_BSS_STARTED);
	ifmgr_debug("check for SAP restart");

	if (policy_mgr_is_vdev_ll_lt_sap(psoc, wlan_vdev_get_id(vdev)))
		policy_mgr_ll_lt_sap_restart_concurrent_sap(psoc, true);
	else
		policy_mgr_check_concurrent_intf_and_restart_sap(
				psoc,
				wlan_util_vdev_mgr_get_acs_mode_for_vdev(vdev));
	/*
	 * Enable TDLS again on concurrent STA
	 */
	if (event_data && QDF_IS_STATUS_ERROR(event_data->status))
		wlan_tdls_notify_start_bss_failure(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS if_mgr_ap_stop_bss(struct wlan_objmgr_vdev *vdev,
			      struct if_mgr_event_data *event_data)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
if_mgr_ap_stop_bss_complete(struct wlan_objmgr_vdev *vdev,
			    struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	uint8_t mcc_scc_switch;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE ||
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE)
		wlan_handle_emlsr_sta_concurrency(psoc, false, true);
	/*
	 * Due to audio share glitch with P2P GO caused by
	 * roam scan on concurrent interface, disable
	 * roaming if "p2p_disable_roam" ini is enabled.
	 * Re-enable roaming on other STA interface if p2p GO
	 * is active on any vdev.
	 */
	if (cfg_p2p_is_roam_config_disabled(psoc) &&
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE) {
		ifmgr_debug("p2p go disconnected enable roam");
		if_mgr_enable_roaming(pdev, vdev, RSO_START_BSS);
	}

	ifmgr_debug("SAP/P2P-GO is stopped, re-enable roaming if it's stopped due to SAP/P2P-GO CSA");
	if_mgr_enable_roaming(pdev, vdev, RSO_SAP_CHANNEL_CHANGE);

	policy_mgr_get_mcc_scc_switch(psoc, &mcc_scc_switch);
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE &&
	    mcc_scc_switch == QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL)
		policy_mgr_check_concurrent_intf_and_restart_sap(psoc, false);

	if (policy_mgr_is_vdev_ll_lt_sap(psoc, wlan_vdev_get_id(vdev)))
		policy_mgr_ll_lt_sap_restart_concurrent_sap(psoc, false);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
if_mgr_ap_csa_complete(struct wlan_objmgr_vdev *vdev,
		       struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	status = wlan_p2p_check_and_force_scc_go_plus_go(psoc, vdev);
	if (QDF_IS_STATUS_ERROR(status))
		ifmgr_err("force scc failure with status: %d", status);

	wlan_tdls_notify_channel_switch_complete(psoc, wlan_vdev_get_id(vdev));

	return status;
}

QDF_STATUS
if_mgr_ap_csa_start(struct wlan_objmgr_vdev *vdev,
		    struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	enum QDF_OPMODE op_mode;

	op_mode = wlan_vdev_mlme_get_opmode(vdev);
	if (op_mode != QDF_SAP_MODE && op_mode != QDF_P2P_GO_MODE)
		return QDF_STATUS_SUCCESS;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Disable TDLS off-channel before VDEV restart
	 */
	wlan_tdls_notify_channel_switch_start(psoc, vdev);

	return QDF_STATUS_SUCCESS;
}
