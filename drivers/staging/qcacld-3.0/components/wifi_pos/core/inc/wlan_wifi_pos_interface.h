/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: declare WIFI POS interface APIs exposed by the mlme component
 */

#ifndef _WLAN_WIFI_POS_IFACE_API_H_
#define _WLAN_WIFI_POS_IFACE_API_H_

#include <wlan_objmgr_vdev_obj.h>
#include "lim_types.h"
#include "wifi_pos_pasn_api.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
/**
 * wlan_wifi_pos_pasn_peer_delete_all  - Delete all pasn peer callback
 * @psoc: Psoc pointer
 * @vdev_id: vdev for which PASN peers are to be deleted
 *
 * Return: True if PASN peer delete all is required
 */
bool wlan_wifi_pos_pasn_peer_delete_all(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id);

/**
 * wifi_pos_register_legacy_ops() - Register wifi pos legacy callbacks
 * @psoc: Psoc pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_register_legacy_ops(struct wlan_objmgr_psoc *psoc);

/**
 * wifi_pos_deregister_legacy_ops() - Deregister wifi pos legacy callbacks
 * @psoc: Psoc pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_deregister_legacy_ops(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS
wifi_pos_register_legacy_ops(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wifi_pos_deregister_legacy_ops(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool
wlan_wifi_pos_pasn_peer_delete_all(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id)
{
	return false;
}
#endif
#endif /* _WLAN_WIFI_POS_IFACE_API_H_ */
