/*
 * Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.

 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <wlan_wifi_radar_utils_api.h>
#include <wlan_wifi_radar_tgt_api.h>
#include <qdf_module.h>
#include <wifi_radar_defs_i.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>

QDF_STATUS wlan_wifi_radar_init(void)
{
	if (wlan_objmgr_register_psoc_create_handler(
	    WLAN_UMAC_COMP_WIFI_RADAR,
	    wlan_wifi_radar_psoc_obj_create_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_register_psoc_destroy_handler(
	    WLAN_UMAC_COMP_WIFI_RADAR,
		wlan_wifi_radar_psoc_obj_destroy_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_register_pdev_create_handler(
		WLAN_UMAC_COMP_WIFI_RADAR,
		wlan_wifi_radar_pdev_obj_create_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_register_pdev_destroy_handler(
		WLAN_UMAC_COMP_WIFI_RADAR,
		wlan_wifi_radar_pdev_obj_destroy_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_wifi_radar_deinit(void)
{
	QDF_STATUS ret_status = QDF_STATUS_SUCCESS;

	if (wlan_objmgr_unregister_psoc_create_handler(
		WLAN_UMAC_COMP_WIFI_RADAR,
		wlan_wifi_radar_psoc_obj_create_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		wifi_radar_err("failed to unregister psoc create handler");
		ret_status = QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_unregister_psoc_destroy_handler(
		WLAN_UMAC_COMP_WIFI_RADAR,
		wlan_wifi_radar_psoc_obj_destroy_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		wifi_radar_err("failed to unregister psoc destroy handler");
		ret_status = QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_unregister_pdev_create_handler(
		WLAN_UMAC_COMP_WIFI_RADAR,
		wlan_wifi_radar_pdev_obj_create_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		wifi_radar_err("failed to unregister pdev create handler");
		ret_status = QDF_STATUS_E_FAILURE;
	}
	if (wlan_objmgr_unregister_pdev_destroy_handler(
		WLAN_UMAC_COMP_WIFI_RADAR,
		wlan_wifi_radar_pdev_obj_destroy_handler, NULL)
		!= QDF_STATUS_SUCCESS) {
		wifi_radar_err("failed to unregister pdev destroy handler");
		ret_status = QDF_STATUS_E_FAILURE;
	}
	return ret_status;
}

QDF_STATUS wlan_wifi_radar_pdev_open(struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("WiFi Radar is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	/* RealyFS init */
	status = wifi_radar_streamfs_init(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		wifi_radar_err(
		"wifi_radar_streamfs_init failed with %d",
		status);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_wifi_radar_pdev_close(struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("WiFi Radar is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	status = wifi_radar_streamfs_remove(pdev);

	return status;
}

QDF_STATUS wifi_radar_initialize_pdev(struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("WiFi Radar is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	return status;
}

qdf_export_symbol(wifi_radar_initialize_pdev);

QDF_STATUS wifi_radar_deinitialize_pdev(struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("WiFi Radar is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	return status;
}

qdf_export_symbol(wifi_radar_deinitialize_pdev);

bool wlan_wifi_radar_is_feature_disabled(struct wlan_objmgr_pdev *pdev)
{
	if (!pdev) {
		wifi_radar_err("PDEV is NULL!");
		return true;
	}

	return (wlan_pdev_nif_feat_ext_cap_get(
			pdev, WLAN_PDEV_FEXT_WIFI_RADAR_ENABLE) ? false : true);
}
