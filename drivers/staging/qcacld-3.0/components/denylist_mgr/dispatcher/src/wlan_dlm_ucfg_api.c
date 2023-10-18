/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: define UCFG APIs exposed by the denylist mgr component
 */

#include <wlan_dlm_ucfg_api.h>
#include <wlan_dlm_core.h>
#include <wlan_dlm_api.h>
#include "wlan_pmo_obj_mgmt_api.h"

QDF_STATUS ucfg_dlm_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_pdev_create_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_pdev_object_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("pdev create register notification failed");
		goto fail_create_pdev;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_pdev_object_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("pdev destroy register notification failed");
		goto fail_destroy_pdev;
	}

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_psoc_object_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("psoc create register notification failed");
		goto fail_create_psoc;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_psoc_object_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("psoc destroy register notification failed");
		goto fail_destroy_psoc;
	}

	return QDF_STATUS_SUCCESS;

fail_destroy_psoc:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_DENYLIST_MGR,
				   dlm_psoc_object_created_notification, NULL);
fail_create_psoc:
	wlan_objmgr_unregister_pdev_destroy_handler(
				 WLAN_UMAC_COMP_DENYLIST_MGR,
				 dlm_pdev_object_destroyed_notification, NULL);
fail_destroy_pdev:
	wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_DENYLIST_MGR,
				   dlm_pdev_object_created_notification, NULL);
fail_create_pdev:
	return status;
}

QDF_STATUS ucfg_dlm_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_psoc_object_destroyed_notification,
			NULL);

	status = wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_psoc_object_created_notification,
			NULL);

	status = wlan_objmgr_unregister_pdev_destroy_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_pdev_object_destroyed_notification,
			NULL);

	status = wlan_objmgr_unregister_pdev_create_handler(
			WLAN_UMAC_COMP_DENYLIST_MGR,
			dlm_pdev_object_created_notification,
			NULL);

	return status;
}

QDF_STATUS ucfg_dlm_psoc_set_suspended(struct wlan_objmgr_psoc *psoc,
				       bool state)
{
	struct dlm_psoc_priv_obj *dlm_psoc_obj;

	dlm_psoc_obj = dlm_get_psoc_obj(psoc);

	if (!dlm_psoc_obj) {
		dlm_err("DLM psoc obj NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dlm_psoc_obj->is_suspended = state;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_dlm_psoc_get_suspended(struct wlan_objmgr_psoc *psoc,
				       bool *state)
{
	struct dlm_psoc_priv_obj *dlm_psoc_obj;

	dlm_psoc_obj = dlm_get_psoc_obj(psoc);

	if (!dlm_psoc_obj) {
		dlm_err("DLM psoc obj NULL");
		*state = true;
		return QDF_STATUS_E_FAILURE;
	}

	*state = dlm_psoc_obj->is_suspended;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ucfg_dlm_suspend_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	ucfg_dlm_psoc_set_suspended(psoc, true);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ucfg_dlm_resume_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	ucfg_dlm_psoc_set_suspended(psoc, false);
	dlm_update_reject_ap_list_to_fw(psoc);
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_dlm_register_pmo_handler(void)
{
	pmo_register_suspend_handler(WLAN_UMAC_COMP_DENYLIST_MGR,
				     ucfg_dlm_suspend_handler, NULL);
	pmo_register_resume_handler(WLAN_UMAC_COMP_DENYLIST_MGR,
				    ucfg_dlm_resume_handler, NULL);
}

static inline void
ucfg_dlm_unregister_pmo_handler(void)
{
	pmo_unregister_suspend_handler(WLAN_UMAC_COMP_DENYLIST_MGR,
				       ucfg_dlm_suspend_handler);
	pmo_unregister_resume_handler(WLAN_UMAC_COMP_DENYLIST_MGR,
				      ucfg_dlm_resume_handler);
}

QDF_STATUS ucfg_dlm_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	ucfg_dlm_register_pmo_handler();
	return dlm_cfg_psoc_open(psoc);
}

QDF_STATUS ucfg_dlm_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	ucfg_dlm_unregister_pmo_handler();
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_dlm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info)
{
	return dlm_add_bssid_to_reject_list(pdev, ap_info);
}

QDF_STATUS
ucfg_dlm_add_userspace_deny_list(struct wlan_objmgr_pdev *pdev,
				 struct qdf_mac_addr *bssid_deny_list,
				 uint8_t num_of_bssid)
{
	return dlm_add_userspace_deny_list(pdev, bssid_deny_list,
					    num_of_bssid);
}

void
ucfg_dlm_dump_deny_list_ap(struct wlan_objmgr_pdev *pdev)
{
	return wlan_dlm_dump_denylist_bssid(pdev);
}

void
ucfg_dlm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum dlm_connection_state con_state)
{
	wlan_dlm_update_bssid_connect_params(pdev, bssid, con_state);
}

void
ucfg_dlm_wifi_off(struct wlan_objmgr_pdev *pdev)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	struct dlm_config *cfg;
	QDF_STATUS status;

	if (!pdev) {
		dlm_err("pdev is NULL");
		return;
	}

	dlm_ctx = dlm_get_pdev_obj(pdev);
	dlm_psoc_obj = dlm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!dlm_ctx || !dlm_psoc_obj) {
		dlm_err("dlm_ctx or dlm_psoc_obj is NULL");
		return;
	}

	status = qdf_mutex_acquire(&dlm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	cfg = &dlm_psoc_obj->dlm_cfg;

	dlm_flush_reject_ap_list(dlm_ctx);
	dlm_send_reject_ap_list_to_fw(pdev, &dlm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&dlm_ctx->reject_ap_list_lock);
}
