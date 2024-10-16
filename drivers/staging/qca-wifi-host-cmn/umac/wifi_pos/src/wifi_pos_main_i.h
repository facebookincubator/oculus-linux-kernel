/*
 * Copyright (c) 2017, 2019-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wifi_pos_main_i.h
 * This file prototyps the important functions pertinent to wifi positioning
 * component.
 */

#ifndef _WIFI_POS_MAIN_H_
#define _WIFI_POS_MAIN_H_

#ifdef CNSS_GENL
#define ENHNC_FLAGS_LEN 4
#define NL_ENABLE_OEM_REQ_RSP 0x00000001
#endif

/* forward reference */
struct wlan_objmgr_psoc;

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
/**
 * wifi_pos_init_11az_context  - Initialize 11az context
 * @vdev_pos_obj: Vdev private object of WIFI Pos component
 *
 * Return: None
 */
void
wifi_pos_init_11az_context(struct wifi_pos_vdev_priv_obj *vdev_pos_obj);
#else
static inline void
wifi_pos_init_11az_context(struct wifi_pos_vdev_priv_obj *vdev_pos_obj)
{}
#endif

/**
 * wifi_pos_psoc_obj_created_notification: callback registered to be called when
 * psoc object is created.
 * @psoc: pointer to psoc object just created
 * @arg_list: argument list
 *
 * This function will:
 *         create WIFI POS psoc object and attach to psoc
 *         register TLV vs nonTLV callbacks
 * Return: status of operation
 */
QDF_STATUS wifi_pos_psoc_obj_created_notification(
		struct wlan_objmgr_psoc *psoc, void *arg_list);

/**
 * wifi_pos_psoc_obj_destroyed_notification: callback registered to be called
 * when psoc object is destroyed.
 * @psoc: pointer to psoc object just about to be destroyed
 * @arg_list: argument list
 *
 * This function will:
 *         detach WIFI POS from psoc object and free
 * Return: status of operation
 */
QDF_STATUS  wifi_pos_psoc_obj_destroyed_notification(
				struct wlan_objmgr_psoc *psoc, void *arg_list);

/**
 * wifi_pos_vdev_created_notification() - Vdev created notification callback
 * @vdev: Vdev object
 * @arg_list: argument list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_vdev_created_notification(struct wlan_objmgr_vdev *vdev,
				   void *arg_list);

/**
 * wifi_pos_vdev_destroyed_notification() - Wifi Pos vdev destroyed callback
 * @vdev: Vdev object
 * @arg_list: argument list
 *
 * This function will detach the Wifi Pos vdev private object and free it
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_vdev_destroyed_notification(struct wlan_objmgr_vdev *vdev,
				     void *arg_list);

/**
 * wifi_pos_peer_object_created_notification() - Handle peer object created
 * notification.
 * @peer: Objmgr peer
 * @arg: Argument
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_peer_object_created_notification(struct wlan_objmgr_peer *peer,
					  void *arg);

/**
 * wifi_pos_peer_object_destroyed_notification() - Handler for peer object
 * deleted notification
 * @peer: Objmgr peer
 * @arg: Argument
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_peer_object_destroyed_notification(struct wlan_objmgr_peer *peer,
					    void *arg);

/**
 * wifi_pos_oem_rsp_handler: lmac rx ops registered
 * @psoc: pointer to psoc object
 * @oem_rsp: response from firmware
 *
 * Return: status of operation
 */
int wifi_pos_oem_rsp_handler(struct wlan_objmgr_psoc *psoc,
			     struct oem_data_rsp *oem_rsp);
#endif
