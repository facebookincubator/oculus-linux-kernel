/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_dlm_main.c
 *
 * WLAN Denylist Mgr related APIs
 *
 */

/* Include files */

#include "target_if_dlm.h"
#include <wlan_dlm_ucfg_api.h>
#include "cfg_ucfg_api.h"
#include <wlan_dlm_core.h>

struct dlm_pdev_priv_obj *
dlm_get_pdev_obj(struct wlan_objmgr_pdev *pdev)
{
	struct dlm_pdev_priv_obj *dlm_pdev_obj;

	dlm_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						  WLAN_UMAC_COMP_DENYLIST_MGR);

	return dlm_pdev_obj;
}

struct dlm_psoc_priv_obj *
dlm_get_psoc_obj(struct wlan_objmgr_psoc *psoc)
{
	struct dlm_psoc_priv_obj *dlm_psoc_obj;

	dlm_psoc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
						  WLAN_UMAC_COMP_DENYLIST_MGR);

	return dlm_psoc_obj;
}

QDF_STATUS
dlm_pdev_object_created_notification(struct wlan_objmgr_pdev *pdev,
				     void *arg)
{
	struct dlm_pdev_priv_obj *dlm_ctx;
	QDF_STATUS status;

	dlm_ctx = qdf_mem_malloc(sizeof(*dlm_ctx));

	if (!dlm_ctx)
		return QDF_STATUS_E_FAILURE;

	status = qdf_mutex_create(&dlm_ctx->reject_ap_list_lock);

	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("Failed to create mutex");
		qdf_mem_free(dlm_ctx);
		return status;
	}
	qdf_list_create(&dlm_ctx->reject_ap_list, MAX_BAD_AP_LIST_SIZE);

	target_if_dlm_register_tx_ops(&dlm_ctx->dlm_tx_ops);
	status = wlan_objmgr_pdev_component_obj_attach(pdev,
						   WLAN_UMAC_COMP_DENYLIST_MGR,
						   dlm_ctx,
						   QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("Failed to attach pdev_ctx with pdev");
		qdf_list_destroy(&dlm_ctx->reject_ap_list);
		qdf_mutex_destroy(&dlm_ctx->reject_ap_list_lock);
		qdf_mem_free(dlm_ctx);
	}

	return status;
}

QDF_STATUS
dlm_pdev_object_destroyed_notification(struct wlan_objmgr_pdev *pdev,
				       void *arg)
{
	struct dlm_pdev_priv_obj *dlm_ctx;

	dlm_ctx = dlm_get_pdev_obj(pdev);

	if (!dlm_ctx) {
		dlm_err("DLM Pdev obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/* Clear away the memory allocated for the bad BSSIDs */
	dlm_flush_reject_ap_list(dlm_ctx);
	qdf_list_destroy(&dlm_ctx->reject_ap_list);
	qdf_mutex_destroy(&dlm_ctx->reject_ap_list_lock);

	wlan_objmgr_pdev_component_obj_detach(pdev,
					      WLAN_UMAC_COMP_DENYLIST_MGR,
					      dlm_ctx);
	qdf_mem_free(dlm_ctx);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dlm_psoc_object_created_notification(struct wlan_objmgr_psoc *psoc,
				     void *arg)
{
	struct dlm_psoc_priv_obj *dlm_psoc_obj;
	QDF_STATUS status;

	dlm_psoc_obj = qdf_mem_malloc(sizeof(*dlm_psoc_obj));

	if (!dlm_psoc_obj)
		return QDF_STATUS_E_FAILURE;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						   WLAN_UMAC_COMP_DENYLIST_MGR,
						   dlm_psoc_obj,
						   QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		dlm_err("Failed to attach psoc_ctx with psoc");
		qdf_mem_free(dlm_psoc_obj);
	}

	return status;
}

QDF_STATUS
dlm_psoc_object_destroyed_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct dlm_psoc_priv_obj *dlm_psoc_obj;

	dlm_psoc_obj = dlm_get_psoc_obj(psoc);

	if (!dlm_psoc_obj) {
		dlm_err("DLM psoc obj NULL");
		return QDF_STATUS_E_FAILURE;
	}
	wlan_objmgr_psoc_component_obj_detach(psoc,
					      WLAN_UMAC_COMP_DENYLIST_MGR,
					      dlm_psoc_obj);
	qdf_mem_free(dlm_psoc_obj);

	return QDF_STATUS_SUCCESS;
}

static void
dlm_init_cfg(struct wlan_objmgr_psoc *psoc, struct dlm_config *dlm_cfg)
{
	dlm_cfg->avoid_list_exipry_time =
				cfg_get(psoc, CFG_AVOID_LIST_EXPIRY_TIME);
	dlm_cfg->deny_list_exipry_time =
				cfg_get(psoc, CFG_DENY_LIST_EXPIRY_TIME);
	dlm_cfg->bad_bssid_counter_reset_time =
				cfg_get(psoc, CFG_BAD_BSSID_RESET_TIME);
	dlm_cfg->bad_bssid_counter_thresh =
				cfg_get(psoc, CFG_BAD_BSSID_COUNTER_THRESHOLD);
	dlm_cfg->delta_rssi =
				cfg_get(psoc, CFG_DENYLIST_RSSI_THRESHOLD);
}

QDF_STATUS
dlm_cfg_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	struct dlm_psoc_priv_obj *dlm_psoc_obj;

	dlm_psoc_obj = dlm_get_psoc_obj(psoc);

	if (!dlm_psoc_obj) {
		dlm_err("DLM psoc obj NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dlm_init_cfg(psoc, &dlm_psoc_obj->dlm_cfg);

	return QDF_STATUS_SUCCESS;
}
