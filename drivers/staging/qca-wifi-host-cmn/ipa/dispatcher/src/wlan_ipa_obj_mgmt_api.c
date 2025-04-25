/*
 * Copyright (c) 2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: public API related to the wlan ipa called by north bound HDD/OSIF
 */

#include "wlan_ipa_obj_mgmt_api.h"
#include "wlan_ipa_main.h"
#include "wlan_objmgr_global_obj.h"
#include <wlan_objmgr_global_obj_i.h>
#include "target_if_ipa.h"
#include "wlan_ipa_ucfg_api.h"
#include "qdf_platform.h"
#include "qdf_module.h"

/* This is as per IPA capbility */
#define MAX_INSTANCES_SUPPORTED 2

uint8_t g_instances_added;
static bool g_ipa_is_ready;
qdf_mutex_t g_init_deinit_lock;

bool ipa_cb_is_ready(void)
{
	return g_ipa_is_ready;
}

void ipa_disable_register_cb(void)
{
	ipa_debug("Don't register ready cb with IPA driver");
	g_ipa_is_ready = false;
}

qdf_export_symbol(ipa_disable_register_cb);

void ipa_init_deinit_lock(void)
{
	qdf_mutex_acquire(&g_init_deinit_lock);
}

void ipa_init_deinit_unlock(void)
{
	qdf_mutex_release(&g_init_deinit_lock);
}

/**
 * ipa_pdev_obj_destroy_notification() - IPA pdev object destroy notification
 * @pdev: pdev handle
 * @arg_list: arguments list
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
ipa_pdev_obj_destroy_notification(struct wlan_objmgr_pdev *pdev,
				  void *arg_list)
{
	QDF_STATUS status;
	struct wlan_ipa_priv *ipa_obj;

	ipa_debug("ipa pdev destroyed");
	if (!ipa_config_is_enabled()) {
		ipa_debug("IPA is disabled");
		return QDF_STATUS_SUCCESS;
	}

	ipa_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
							WLAN_UMAC_COMP_IPA);
	if (!ipa_obj) {
		ipa_err("Failed to get ipa pdev object");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_pdev_component_obj_detach(pdev,
						       WLAN_UMAC_COMP_IPA,
						       ipa_obj);
	if (QDF_IS_STATUS_ERROR(status))
		ipa_err("Failed to detach ipa pdev object");

	qdf_mem_free(ipa_obj);

	return status;
}

/**
 * ipa_pdev_obj_create_notification() - IPA pdev object creation notification
 * @pdev: pdev handle
 * @arg_list: arguments list
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
ipa_pdev_obj_create_notification(struct wlan_objmgr_pdev *pdev,
				 void *arg_list)
{
	QDF_STATUS status;
	struct wlan_ipa_priv *ipa_obj;

	ipa_debug("ipa pdev created");

	if (!ipa_config_is_enabled()) {
		ipa_info("IPA is disabled");
		return QDF_STATUS_SUCCESS;
	}

	ipa_obj = qdf_mem_malloc(sizeof(*ipa_obj));
	if (!ipa_obj)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_pdev_component_obj_attach(pdev,
						       WLAN_UMAC_COMP_IPA,
						       (void *)ipa_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to attach pdev ipa component");
		qdf_mem_free(ipa_obj);
		return status;
	}

	ipa_obj->pdev = pdev;

	ipa_debug("ipa pdev attached");

	return status;
}

static void ipa_register_ready_cb(void *user_data)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	qdf_device_t qdf_dev;
	qdf_ipa_wdi_capabilities_out_params_t out_param;
	uint8_t pdev_id;

	if (!ipa_config_is_enabled()) {
		ipa_info("IPA config is disabled");
		return;
	}

	if (!user_data) {
		ipa_err("User_data object is NULL");
		return;
	}

	/* Validate driver state to determine ipa_obj is valid or not */
	if (qdf_is_driver_state_module_stop()) {
		ipa_err("Driver modules stop in-progress or done");
		return;
	}

	ipa_init_deinit_lock();

	/*
	 * Meanwhile acquiring lock, driver stop modules can happen in parallel,
	 * validate driver state once again to proceed with IPA init.
	 */
	if (qdf_is_driver_state_module_stop()) {
		ipa_err("Driver modules stop in-progress/done, releasing lock");
		goto out;
	}

	g_ipa_is_ready = true;
	ipa_info("IPA ready callback invoked: ipa_register_ready_cb");

	/* Make call to get num_instances supported by IPA */
	qdf_ipa_wdi_get_capabilities(&out_param);

	ipa_obj = (struct wlan_ipa_priv *)user_data;

	pdev = ipa_priv_obj_get_pdev(ipa_obj);
	if (!pdev) {
		qdf_err("Pdev is NULL for");
		goto out;
	}

	pdev_id = pdev->pdev_objmgr.wlan_pdev_id;
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		qdf_err("Psoc is NULL for pdev_id %d", pdev_id);
		goto out;
	}

	if (ipa_obj->handle_initialized) {
		ipa_info("ipa_obj hdl is true for pdev_id %d", pdev_id);
		goto out;
	}

	/* Update instance_id for current pdev */
	ipa_obj->instance_id = psoc->soc_objmgr.psoc_id;

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	if (!qdf_dev) {
		ipa_err("QDF device context is NULL");
		goto out;
	}

	status = ipa_obj_setup(ipa_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		g_ipa_is_ready = false;
		ipa_err("Failed to ipa_obj_setup");
		goto out;
	}
	if (ucfg_ipa_uc_ol_init(pdev, qdf_dev)) {
		ipa_err("IPA ucfg_ipa_uc_ol_init failed");
		ipa_obj_cleanup(ipa_obj);
		g_ipa_is_ready = false;
		goto out;
	}

	ipa_obj->handle_initialized = true;
out:
	ipa_init_deinit_unlock();
}

QDF_STATUS ipa_register_is_ipa_ready(struct wlan_objmgr_pdev *pdev)
{
	int ret;
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_info("IPA config is disabled");
		return QDF_STATUS_SUCCESS;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ret = qdf_ipa_register_ipa_ready_cb(ipa_register_ready_cb,
					    (void *)ipa_obj);
	if (ret == -EEXIST) {
		ipa_info("IPA is ready, invoke callback");
		ipa_register_ready_cb((void *)ipa_obj);
	} else if (ret) {
		ipa_err("Failed to check IPA readiness %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	/* Acquire lock */
	ipa_init_deinit_lock();
	g_instances_added++;
	ipa_info("No. of instances added for IPA is %d", g_instances_added);
	/* Unlock */
	ipa_init_deinit_unlock();

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(ipa_register_is_ipa_ready);

QDF_STATUS ipa_init(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	ipa_info("ipa module dispatcher init");

	if (!ipa_check_hw_present()) {
		ipa_info("ipa hw not present");
		return status;
	}

	status = wlan_objmgr_register_pdev_create_handler(WLAN_UMAC_COMP_IPA,
		ipa_pdev_obj_create_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to register pdev create handler for ipa");

		return status;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(WLAN_UMAC_COMP_IPA,
		ipa_pdev_obj_destroy_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to register pdev destroy handler for ipa");
		goto fail_delete_pdev;
	}

	qdf_mutex_create(&g_init_deinit_lock);

	return status;

fail_delete_pdev:
	wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_IPA,
		ipa_pdev_obj_create_notification, NULL);

	return status;
}

QDF_STATUS ipa_deinit(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	ipa_info("ipa module dispatcher deinit");

	if (!ipa_is_hw_support()) {
		ipa_info("ipa hw is not present");
		return status;
	}

	qdf_mutex_destroy(&g_init_deinit_lock);

	status = wlan_objmgr_unregister_pdev_destroy_handler(WLAN_UMAC_COMP_IPA,
				ipa_pdev_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ipa_err("Failed to unregister pdev destroy handler");

	status = wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_IPA,
				ipa_pdev_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ipa_err("Failed to unregister pdev create handler");

	return status;
}

qdf_ipa_wdi_hdl_t wlan_ipa_get_hdl(void *soc, uint8_t pdev_id)
{
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)soc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ipa_priv *ipa_obj;
	qdf_ipa_wdi_hdl_t hdl;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, pdev_id, WLAN_IPA_ID);

	if (!pdev) {
		ipa_err("Failed to get pdev handle");
		return IPA_INVALID_HDL;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL for pdev_id[%d]", pdev_id);
		wlan_objmgr_pdev_release_ref(pdev, WLAN_IPA_ID);
		return IPA_INVALID_HDL;
	}
	hdl = ipa_obj->hdl;

	wlan_objmgr_pdev_release_ref(pdev, WLAN_IPA_ID);
	return hdl;
}

qdf_export_symbol(wlan_ipa_get_hdl);

bool wlan_ipa_is_vlan_enabled(void)
{
	return ipa_config_is_vlan_enabled();
}

qdf_export_symbol(wlan_ipa_is_vlan_enabled);
