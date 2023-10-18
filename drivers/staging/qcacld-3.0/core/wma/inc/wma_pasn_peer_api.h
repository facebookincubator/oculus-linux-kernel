/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __WMA_PASN_PEER_API_H__
#define __WMA_PASN_PEER_API_H__

#include "qdf_types.h"
#include "osapi_linux.h"
#include "wmi_services.h"
#include "wmi_unified.h"
#include "wmi_unified_api.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wma.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
/**
 * wma_pasn_peer_remove  - Remove RTT pasn peer and send peer delete command to
 * firmware
 * @wma: WMA handle
 * @peer_addr: Peer mac address
 * @vdev_id: vdev id
 * @no_fw_peer_delete: Don't send peer delete to firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wma_pasn_peer_remove(struct wlan_objmgr_psoc *psoc,
		     struct qdf_mac_addr *peer_addr,
		     uint8_t vdev_id,  bool no_fw_peer_delete);

/**
 * wma_pasn_peer_create() - Create RTT PASN peer
 * @psoc: PSOC pointer
 * @peer_addr: Peer address
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wma_pasn_peer_create(struct wlan_objmgr_psoc *psoc,
		     struct qdf_mac_addr *peer_addr,
		     uint8_t vdev_id);

/**
 * wma_pasn_handle_peer_create_conf() - Handle PASN peer create confirm event
 * @wma: WMA handle
 * @peer_mac: peer mac address
 * @status: status
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wma_pasn_handle_peer_create_conf(tp_wma_handle wma,
				 struct qdf_mac_addr *peer_mac,
				 QDF_STATUS status, uint8_t vdev_id);

/**
 * wma_delete_all_pasn_peers() - Delete all PASN peers
 * @wma: WMA handle
 * @vdev: Vdev object pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wma_delete_all_pasn_peers(tp_wma_handle wma, struct wlan_objmgr_vdev *vdev);

QDF_STATUS
wma_resume_vdev_delete(tp_wma_handle wma, uint8_t vdev_id);

QDF_STATUS
wma_pasn_peer_delete_all_complete(struct wlan_objmgr_vdev *vdev);
#else
static inline QDF_STATUS
wma_pasn_peer_create(struct wlan_objmgr_psoc *psoc,
		     struct qdf_mac_addr *peer_addr,
		     uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wma_pasn_peer_remove(struct wlan_objmgr_psoc *psoc,
		     struct qdf_mac_addr *peer_addr,
		     uint8_t vdev_id,  bool no_fw_peer_delete)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wma_pasn_handle_peer_create_conf(tp_wma_handle wma,
				 struct qdf_mac_addr *peer_mac,
				 QDF_STATUS status, uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wma_delete_all_pasn_peers(tp_wma_handle wma, struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wma_resume_vdev_delete(tp_wma_handle wma, uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif
