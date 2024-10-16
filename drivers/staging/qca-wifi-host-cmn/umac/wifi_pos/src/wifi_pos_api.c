/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wifi_pos_api.c
 * This file defines the APIs wifi_pos component.
 */

#include <wlan_lmac_if_def.h>
#include "wifi_pos_api.h"
#include "wifi_pos_utils_i.h"
#include "wifi_pos_main_i.h"
#include "os_if_wifi_pos.h"
#include "target_if_wifi_pos.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_peer_obj.h"
#include "wlan_lmac_if_def.h"

struct wlan_lmac_if_wifi_pos_rx_ops *
wifi_pos_get_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return NULL;
	}

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		wifi_pos_err("rx_ops is NULL");
		return NULL;
	}

	return &rx_ops->wifi_pos_rx_ops;
}

struct wifi_pos_legacy_ops *wifi_pos_get_legacy_ops(void)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_obj =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_obj)
		return NULL;

	return wifi_pos_obj->legacy_ops;
}

QDF_STATUS
wifi_pos_set_legacy_ops(struct wlan_objmgr_psoc *psoc,
			struct wifi_pos_legacy_ops *legacy_ops)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_obj =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_obj)
		return QDF_STATUS_E_FAILURE;

	wifi_pos_obj->legacy_ops = legacy_ops;

	return QDF_STATUS_SUCCESS;
}

struct wlan_lmac_if_wifi_pos_tx_ops *
wifi_pos_get_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return NULL;
	}

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		wifi_pos_err("tx_ops is NULL");
		return NULL;
	}

	return &tx_ops->wifi_pos_tx_ops;
}

QDF_STATUS wifi_pos_init(void)
{
	QDF_STATUS status;

	wifi_pos_lock_init();

	/* register psoc create handler functions. */
	status = wlan_objmgr_register_psoc_create_handler(
		WLAN_UMAC_COMP_WIFI_POS,
		wifi_pos_psoc_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("register_psoc_create_handler failed, status: %d",
			     status);
		return status;
	}

	/* register psoc delete handler functions. */
	status = wlan_objmgr_register_psoc_destroy_handler(
		WLAN_UMAC_COMP_WIFI_POS,
		wifi_pos_psoc_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("register_psoc_destroy_handler failed, status: %d",
			     status);
		goto fail_psoc_destroy_handler;
	}

	status = wlan_objmgr_register_vdev_create_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_vdev_created_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("register_vdev_create_handler failed, status: %d",
			     status);
		goto fail_vdev_create_handler;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_vdev_destroyed_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("register_vdev_destroy_handler failed, status: %d",
			     status);
		goto fail_vdev_destroy_handler;
	}

	status =  wlan_objmgr_register_peer_create_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_peer_object_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("peer create register notification failed");
		goto fail_peer_create_handler;
	}

	status = wlan_objmgr_register_peer_destroy_handler(
				WLAN_UMAC_COMP_WIFI_POS,
				wifi_pos_peer_object_destroyed_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("peer destroy register notification failed");
		goto fail_peer_destroy_handler;
	}

	return status;

fail_peer_destroy_handler:
	wlan_objmgr_unregister_peer_create_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_peer_object_created_notification,
			NULL);
fail_peer_create_handler:
	wlan_objmgr_unregister_vdev_destroy_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_vdev_destroyed_notification, NULL);

fail_vdev_destroy_handler:
	wlan_objmgr_unregister_vdev_create_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_vdev_created_notification, NULL);

fail_vdev_create_handler:
	wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_psoc_obj_destroyed_notification, NULL);

fail_psoc_destroy_handler:
	wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_WIFI_POS,
			wifi_pos_psoc_obj_created_notification, NULL);

	return status;
}

QDF_STATUS wifi_pos_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_peer_destroy_handler(
				WLAN_UMAC_COMP_WIFI_POS,
				wifi_pos_peer_object_destroyed_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("unable to unregister peer destroy handle");

	status = wlan_objmgr_unregister_peer_create_handler(
				WLAN_UMAC_COMP_WIFI_POS,
				wifi_pos_peer_object_created_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("unable to unregister peer create handle");

	status = wlan_objmgr_unregister_vdev_destroy_handler(
				WLAN_UMAC_COMP_WIFI_POS,
				wifi_pos_vdev_destroyed_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("unregister_vdev_destroy_handler failed, status: %d",
			     status);

	status = wlan_objmgr_unregister_vdev_create_handler(
				WLAN_UMAC_COMP_WIFI_POS,
				wifi_pos_vdev_created_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("unregister_vdev_create_handler failed, status: %d",
			     status);

	/* deregister psoc create handler functions. */
	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_WIFI_POS,
				wifi_pos_psoc_obj_created_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("unregister_psoc_create_handler failed, status: %d",
			     status);
		return status;
	}

	/* deregister psoc delete handler functions. */
	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_WIFI_POS,
				wifi_pos_psoc_obj_destroyed_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("unregister_psoc_destroy_handler failed, status: %d",
			     status);
	}

	wifi_pos_lock_deinit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wifi_pos_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct wlan_lmac_if_wifi_pos_tx_ops *tx_ops;

	tx_ops = wifi_pos_get_tx_ops(psoc);
	if (!tx_ops) {
		wifi_pos_err("tx_ops is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = tx_ops->wifi_pos_register_events(psoc);

	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("target_if_wifi_pos_register_events failed");

	return status;
}

QDF_STATUS wifi_pos_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct wlan_lmac_if_wifi_pos_tx_ops *tx_ops;

	tx_ops = wifi_pos_get_tx_ops(psoc);
	if (!tx_ops) {
		wifi_pos_err("tx_ops is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = tx_ops->wifi_pos_deregister_events(psoc);

	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("target_if_wifi_pos_deregister_events failed");

	return QDF_STATUS_SUCCESS;
}

struct wlan_wifi_pos_peer_priv_obj *
wifi_pos_get_peer_private_object(struct wlan_objmgr_peer *peer)
{
	struct wlan_wifi_pos_peer_priv_obj *peer_priv;

	if (!peer) {
		wifi_pos_err("Peer is NULL");
		return NULL;
	}

	peer_priv =
		wlan_objmgr_peer_get_comp_private_obj(peer,
						      WLAN_UMAC_COMP_WIFI_POS);

	return peer_priv;
}

void wifi_pos_set_oem_target_type(struct wlan_objmgr_psoc *psoc, uint32_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->oem_target_type = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_oem_fw_version(struct wlan_objmgr_psoc *psoc, uint32_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->oem_fw_version = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_drv_ver_major(struct wlan_objmgr_psoc *psoc, uint8_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->driver_version.major = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_drv_ver_minor(struct wlan_objmgr_psoc *psoc, uint8_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->driver_version.minor = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_drv_ver_patch(struct wlan_objmgr_psoc *psoc, uint8_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->driver_version.patch = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_drv_ver_build(struct wlan_objmgr_psoc *psoc, uint8_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->driver_version.build = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_dwell_time_min(struct wlan_objmgr_psoc *psoc, uint16_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->allowed_dwell_time_min = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}
void wifi_pos_set_dwell_time_max(struct wlan_objmgr_psoc *psoc, uint16_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->allowed_dwell_time_max = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_current_dwell_time_max(struct wlan_objmgr_psoc *psoc,
					 uint16_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->current_dwell_time_max = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

void wifi_pos_set_current_dwell_time_min(struct wlan_objmgr_psoc *psoc,
					 uint16_t val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->current_dwell_time_max = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

uint32_t wifi_pos_get_app_pid(struct wlan_objmgr_psoc *psoc)
{
	uint32_t app_pid;
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
				wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return 0;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	app_pid = wifi_pos_psoc->app_pid;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);

	return app_pid;

}

bool wifi_pos_is_app_registered(struct wlan_objmgr_psoc *psoc)
{
	bool is_app_registered;
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
				wifi_pos_get_psoc_priv_obj(psoc);

	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return false;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	is_app_registered = wifi_pos_psoc->is_app_registered;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);

	return is_app_registered;
}

#ifdef WLAN_FEATURE_CIF_CFR
QDF_STATUS wifi_pos_init_cir_cfr_rings(struct wlan_objmgr_psoc *psoc,
				   void *hal_soc, uint8_t num_mac, void *buf)
{
	return target_if_wifi_pos_init_cir_cfr_rings(psoc, hal_soc,
						     num_mac, buf);
}
#endif

QDF_STATUS
wifi_pos_register_get_phy_mode_cb(struct wlan_objmgr_psoc *psoc,
				  void (*handler)(qdf_freq_t, uint32_t,
						  uint32_t *))
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		wifi_pos_err("Null callback");
		return QDF_STATUS_E_NULL_VALUE;
	}
	wifi_pos_psoc = wifi_pos_get_psoc_priv_obj(psoc);
	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc->wifi_pos_get_phy_mode = handler;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wifi_pos_register_get_fw_phy_mode_for_freq_cb(
				struct wlan_objmgr_psoc *psoc,
				void (*handler)(uint32_t, uint32_t, uint32_t *))
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		wifi_pos_err("Null callback");
		return QDF_STATUS_E_NULL_VALUE;
	}
	wifi_pos_psoc = wifi_pos_get_psoc_priv_obj(psoc);
	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc->wifi_pos_get_fw_phy_mode_for_freq = handler;

	return QDF_STATUS_SUCCESS;
}

#ifndef CNSS_GENL
QDF_STATUS wifi_pos_register_get_pdev_id_by_dev_name(
		struct wlan_objmgr_psoc *psoc,
		QDF_STATUS (*handler)(char *dev_name, uint8_t *pdev_id,
				      struct wlan_objmgr_psoc **psoc))
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		wifi_pos_err("Null callback");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc = wifi_pos_get_psoc_priv_obj(psoc);
	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc->wifi_pos_get_pdev_id_by_dev_name = handler;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_RTT_MEASUREMENT_NOTIFICATION
QDF_STATUS wifi_pos_register_measurement_request_notification(
		struct wlan_objmgr_psoc *psoc,
		QDF_STATUS (*handler)(struct wlan_objmgr_pdev *pdev,
				      struct rtt_channel_info *chinfo))
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		wifi_pos_err("Null callback");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc = wifi_pos_get_psoc_priv_obj(psoc);
	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc->wifi_pos_measurement_request_notification = handler;

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_RTT_MEASUREMENT_NOTIFICATION */

QDF_STATUS wifi_pos_register_get_max_fw_phymode_for_channels(
		struct wlan_objmgr_psoc *psoc,
		QDF_STATUS (*handler)(struct wlan_objmgr_pdev *pdev,
				      struct wifi_pos_channel_power *chan_list,
				      uint16_t wifi_pos_num_chans))
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		wifi_pos_err("Null callback");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc = wifi_pos_get_psoc_priv_obj(psoc);
	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc->wifi_pos_get_max_fw_phymode_for_channels = handler;

	return QDF_STATUS_SUCCESS;
}
#endif /* CNSS_GENL */

QDF_STATUS wifi_pos_register_send_action(
				struct wlan_objmgr_psoc *psoc,
				void (*handler)(struct wlan_objmgr_psoc *psoc,
						uint32_t sub_type,
						uint8_t *buf,
						uint32_t buf_len))
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc;

	if (!psoc) {
		wifi_pos_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		wifi_pos_err("Null callback");
		return QDF_STATUS_E_NULL_VALUE;
	}
	wifi_pos_psoc = wifi_pos_get_psoc_priv_obj(psoc);
	if (!wifi_pos_psoc) {
		wifi_pos_err("wifi_pos priv obj is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_psoc->wifi_pos_send_action = handler;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wifi_pos_register_osif_callbacks(struct wifi_pos_osif_ops *ops)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_obj =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_obj) {
		wifi_pos_err("wifi_pos priv obj is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wifi_pos_obj->osif_cb = ops;

	return QDF_STATUS_SUCCESS;
}

struct wifi_pos_osif_ops *wifi_pos_get_osif_callbacks(void)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_obj =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_obj) {
		wifi_pos_err("wifi_pos priv obj is null");
		return NULL;
	}

	return wifi_pos_obj->osif_cb;
}

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
void wifi_pos_set_rsta_sec_ltf_cap(bool val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_psoc) {
		wifi_pos_alert("unable to get wifi_pos psoc obj");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->enable_rsta_secure_ltf_support = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

bool wifi_pos_get_rsta_sec_ltf_cap(void)
{
	bool value;
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_psoc) {
		wifi_pos_alert("unable to get wifi_pos psoc obj");
		return false;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	value = wifi_pos_psoc->enable_rsta_secure_ltf_support;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);

	return value;
}

void wifi_pos_set_rsta_11az_ranging_cap(bool val)
{
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_psoc) {
		wifi_pos_alert("unable to get wifi_pos psoc obj");
		return;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	wifi_pos_psoc->enable_rsta_11az_ranging = val;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);
}

bool wifi_pos_get_rsta_11az_ranging_cap(void)
{
	bool value;
	struct wifi_pos_psoc_priv_obj *wifi_pos_psoc =
			wifi_pos_get_psoc_priv_obj(wifi_pos_get_psoc());

	if (!wifi_pos_psoc) {
		wifi_pos_alert("unable to get wifi_pos psoc obj");
		return false;
	}

	qdf_spin_lock_bh(&wifi_pos_psoc->wifi_pos_lock);
	value = wifi_pos_psoc->enable_rsta_11az_ranging;
	qdf_spin_unlock_bh(&wifi_pos_psoc->wifi_pos_lock);

	return value;
}
#endif
