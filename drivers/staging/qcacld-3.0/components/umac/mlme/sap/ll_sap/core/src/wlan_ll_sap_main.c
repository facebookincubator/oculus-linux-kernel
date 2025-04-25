/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wlan_ll_sap_main.h"
#include <wlan_objmgr_global_obj.h>
#include "qca_vendor.h"
#include "wlan_ll_lt_sap_main.h"
#include "target_if_ll_sap.h"

struct ll_sap_ops *global_ll_sap_ops;

static QDF_STATUS ll_sap_psoc_obj_created_notification(struct wlan_objmgr_psoc *psoc, void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct ll_sap_psoc_priv_obj *ll_sap_psoc_obj;

	ll_sap_psoc_obj = qdf_mem_malloc(sizeof(*ll_sap_psoc_obj));
	if (!ll_sap_psoc_obj)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						       WLAN_UMAC_COMP_LL_SAP,
						       ll_sap_psoc_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("ll_sap obj attach with psoc failed");
		goto ll_sap_psoc_obj_fail;
	}

	target_if_ll_sap_register_tx_ops(&ll_sap_psoc_obj->tx_ops);
	target_if_ll_sap_register_rx_ops(&ll_sap_psoc_obj->rx_ops);

	ll_sap_debug("ll sap psoc object created");

	return status;

ll_sap_psoc_obj_fail:
	qdf_mem_free(ll_sap_psoc_obj);

	return status;
}

static QDF_STATUS ll_sap_psoc_obj_destroyed_notification(struct wlan_objmgr_psoc *psoc, void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct ll_sap_psoc_priv_obj *ll_sap_psoc_obj;

	ll_sap_psoc_obj =
		wlan_objmgr_psoc_get_comp_private_obj(psoc,
						      WLAN_UMAC_COMP_LL_SAP);

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_LL_SAP,
						       ll_sap_psoc_obj);
	if (QDF_IS_STATUS_ERROR(status))
		ll_sap_err("ll_sap detach failed");

	qdf_mem_free(ll_sap_psoc_obj);

	ll_sap_debug("ll sap psoc object destroyed");

	return status;
}

static QDF_STATUS ll_sap_vdev_obj_created_notification(
				struct wlan_objmgr_vdev *vdev, void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct ll_sap_vdev_priv_obj *ll_sap_obj;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_SAP_MODE)
		return QDF_STATUS_SUCCESS;

	ll_sap_obj = qdf_mem_malloc(sizeof(*ll_sap_obj));
	if (!ll_sap_obj)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_component_obj_attach(
						vdev,
						WLAN_UMAC_COMP_LL_SAP,
						(void *)ll_sap_obj,
						QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("vdev %d obj attach failed", wlan_vdev_get_id(vdev));
		goto ll_sap_vdev_attach_failed;
	}

	status = ll_lt_sap_init(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto ll_sap_init_failed;

	return status;

ll_sap_init_failed:
	wlan_objmgr_vdev_component_obj_detach(vdev,
					      WLAN_UMAC_COMP_LL_SAP,
					      ll_sap_obj);

ll_sap_vdev_attach_failed:
	qdf_mem_free(ll_sap_obj);
	return status;
}

static QDF_STATUS ll_sap_vdev_obj_destroyed_notification(
				struct wlan_objmgr_vdev *vdev, void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct ll_sap_vdev_priv_obj *ll_sap_obj;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_SAP_MODE)
		return QDF_STATUS_SUCCESS;

	ll_lt_sap_deinit(vdev);

	ll_sap_obj = ll_sap_get_vdev_priv_obj(vdev);

	if (!ll_sap_obj) {
		ll_sap_err("vdev %d ll sap obj null",
			   wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	status = wlan_objmgr_vdev_component_obj_detach(vdev,
						       WLAN_UMAC_COMP_LL_SAP,
						       ll_sap_obj);
	if (QDF_IS_STATUS_ERROR(status))
		ll_sap_err("vdev %d ll sap obj detach failed, status %d",
			   wlan_vdev_get_id(vdev), status);

	qdf_mem_free(ll_sap_obj);

	return status;
}

QDF_STATUS ll_sap_init(void)
{
	QDF_STATUS status;

	/* register psoc create handler functions. */
	status = wlan_objmgr_register_psoc_create_handler(WLAN_UMAC_COMP_LL_SAP,
							  ll_sap_psoc_obj_created_notification,
							  NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_register_psoc_create_handler failed");
		return status;
	}

	/* register psoc delete handler functions. */
	status = wlan_objmgr_register_psoc_destroy_handler(WLAN_UMAC_COMP_LL_SAP,
							   ll_sap_psoc_obj_destroyed_notification,
							   NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_register_psoc_destroy_handler failed");
		goto err_psoc_destroy_reg;
	}

	/* register vdev create handler functions. */
	status = wlan_objmgr_register_vdev_create_handler(
		WLAN_UMAC_COMP_LL_SAP,
		ll_sap_vdev_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_register_vdev_create_handler failed");
		goto err_vdev_create_reg;
	}

	/* register vdev delete handler functions. */
	status = wlan_objmgr_register_vdev_destroy_handler(
		WLAN_UMAC_COMP_LL_SAP,
		ll_sap_vdev_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_register_vdev_destroy_handler failed");
		goto err_vdev_destroy_reg;
	}

	return status;
err_vdev_destroy_reg:
	wlan_objmgr_unregister_vdev_create_handler(
					WLAN_UMAC_COMP_LL_SAP,
					ll_sap_vdev_obj_created_notification,
					NULL);

err_vdev_create_reg:
	wlan_objmgr_unregister_psoc_destroy_handler(
					WLAN_UMAC_COMP_LL_SAP,
					ll_sap_psoc_obj_destroyed_notification,
					NULL);

err_psoc_destroy_reg:
	wlan_objmgr_unregister_psoc_create_handler(
					WLAN_UMAC_COMP_LL_SAP,
					ll_sap_psoc_obj_created_notification,
					NULL);

	return status;
}

QDF_STATUS ll_sap_deinit(void)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS, status;

	/* de-register vdev delete handler functions. */
	status = wlan_objmgr_unregister_vdev_destroy_handler(
					WLAN_UMAC_COMP_LL_SAP,
					ll_sap_vdev_obj_destroyed_notification,
					NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_unregister_vdev_destroy_handler failed");
		ret = status;
	}

	/* de-register vdev create handler functions. */
	status = wlan_objmgr_unregister_vdev_create_handler(
					WLAN_UMAC_COMP_LL_SAP,
					ll_sap_vdev_obj_created_notification,
					NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_unregister_vdev_create_handler failed");
		ret = status;
	}

	/* unregister psoc destroy handler functions. */
	status = wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_LL_SAP,
							     ll_sap_psoc_obj_destroyed_notification,
							     NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_deregister_psoc_destroy_handler failed");
		ret = status;
	}

	/* unregister psoc create handler functions. */
	status = wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_LL_SAP,
							    ll_sap_psoc_obj_created_notification,
							    NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ll_sap_err("objmgr_unregister_psoc_create_handler failed");
		ret = status;
	}

	return ret;
}

void ll_sap_register_os_if_cb(struct ll_sap_ops *ll_sap_global_ops)
{
	global_ll_sap_ops = ll_sap_global_ops;
}

void ll_sap_unregister_os_if_cb(void)
{
	global_ll_sap_ops = NULL;
}

struct ll_sap_ops *ll_sap_get_osif_cbk(void)
{
	return global_ll_sap_ops;
}

QDF_STATUS ll_sap_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return target_if_ll_sap_register_events(psoc);
}

QDF_STATUS ll_sap_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return target_if_ll_sap_deregister_events(psoc);
}
