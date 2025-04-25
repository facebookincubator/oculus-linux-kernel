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

/**
 * DOC: contains mlo mgr north bound interface api
 */

#ifndef _WLAN_MLO_MGR_PUBLIC_API_H_
#define _WLAN_MLO_MGR_PUBLIC_API_H_

#include <wlan_mlo_mgr_link_switch.h>

/**
 * wlan_mlo_mgr_register_link_switch_notifier() - Components to register
 * notifier callback on start of link switch.
 * @comp_id: Component ID to register callback.
 * @cb: Callback to register.
 *
 * The global MLO MGR will store the list of callbacks registered via this
 * API and call each callback on start of link switch.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
wlan_mlo_mgr_register_link_switch_notifier(enum wlan_umac_comp_id comp_id,
					   mlo_mgr_link_switch_notifier_cb cb)
{
	return mlo_mgr_register_link_switch_notifier(comp_id, cb);
}

/**
 * wlan_mlo_mgr_unregister_link_switch_notifier() - Components to unregister
 * notifier callback on start of link switch.
 * @comp_id: Component ID to unregister the callback.
 *
 * The global MLO MGR will remove the registered callback for @comp_id for
 * notifying start of link switch.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
wlan_mlo_mgr_unregister_link_switch_notifier(enum wlan_umac_comp_id comp_id)
{
	return mlo_mgr_unregister_link_switch_notifier(comp_id);
}

/**
 * wlan_mlo_mgr_is_link_switch_in_progress() - Check link switch in progress
 * on MLO dev context level.
 * @vdev: VDEV object manager
 *
 * This API will check if current state of MLO manager link switch state is
 * in progress or not. The return value of true shall not be treated as @vdev
 * is in link switch in progress. To know the status of VDEV in link switch or
 * not need to use wlan_vdev_mlme_is_mlo_link_switch_in_progress() API
 *
 * Return: bool
 */
static inline bool
wlan_mlo_mgr_is_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return mlo_mgr_is_link_switch_in_progress(vdev);
}

/**
 * wlan_mlo_mgr_is_link_switch_on_assoc_vdev() - Is link switch in progress is
 * on assoc VDEV.
 * @vdev: VDEV object manager.
 *
 * Return true if current link switch in progress is on assoc VDEV or not.
 *
 * Return: void
 */
static inline bool
wlan_mlo_mgr_is_link_switch_on_assoc_vdev(struct wlan_objmgr_vdev *vdev)
{
	return mlo_mgr_is_link_switch_on_assoc_vdev(vdev);
}

/**
 * wlan_mlo_mgr_link_switch_get_assoc_vdev() - Return assco VDEV pointer
 * if it is in link switch.
 * @vdev: VDEV object manager
 *
 * Return: VDEV object manager pointer
 */
static inline struct wlan_objmgr_vdev *
wlan_mlo_mgr_link_switch_get_assoc_vdev(struct wlan_objmgr_vdev *vdev)
{
	return mlo_mgr_link_switch_get_assoc_vdev(vdev);
}

/**
 * wlan_mlo_mgr_link_switch_set_mac_addr_resp() - Dispatcher API to call to
 * notify about status of set mac addr request.
 * @vdev: VDEV object manager
 * @resp_status: Status of request
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
wlan_mlo_mgr_link_switch_set_mac_addr_resp(struct wlan_objmgr_vdev *vdev,
					   uint8_t resp_status)
{
	return mlo_mgr_link_switch_set_mac_addr_resp(vdev, resp_status);
}

/**
 * wlan_mlo_mgr_link_switch_defer_disconnect_req() - Defer disconnect requests
 * from source other than link switch.
 * @vdev: VDEV object manager
 * @source: Disconnect requestor
 * @reason: Reason for disconnect
 *
 * Return: QDF_STATUS.
 */
static inline QDF_STATUS
wlan_mlo_mgr_link_switch_defer_disconnect_req(struct wlan_objmgr_vdev *vdev,
					      enum wlan_cm_source source,
					      enum wlan_reason_code reason)
{
	return mlo_mgr_link_switch_defer_disconnect_req(vdev, source, reason);
}
#endif /* _WLAN_MLO_MGR_PUBLIC_API_H_ */
