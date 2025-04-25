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
 * DOC: wifi_pos_pasn_api.h
 * This file declares public APIs of wifi positioning component
 */
#ifndef _WIFI_POS_PASN_API_H_
#define _WIFI_POS_PASN_API_H_

/* Include files */
#include "wlan_objmgr_cmn.h"
#include "wifi_pos_utils_pub.h"
#include "wifi_pos_public_struct.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
/**
 * wifi_pos_set_peer_ltf_keyseed_required() - Set LTF keyseed required
 *                                            for the peer
 * @peer:  Peer object
 * @value: Value to set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_set_peer_ltf_keyseed_required(struct wlan_objmgr_peer *peer,
				       bool value);
/**
 * wifi_pos_is_ltf_keyseed_required_for_peer() - Is LTF keyseed required for
 *                                               the given peer
 * @peer: Peer object
 *
 * Return: true or false
 */
bool wifi_pos_is_ltf_keyseed_required_for_peer(struct wlan_objmgr_peer *peer);

/**
 * wifi_pos_handle_ranging_peer_create() - Handle ranging peer create
 * @psoc: Pointer to PSOC
 * @req: PASN request
 * @vdev_id: vdev id
 * @total_entries: Total entries
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wifi_pos_handle_ranging_peer_create(struct wlan_objmgr_psoc *psoc,
					       struct wlan_pasn_request *req,
					       uint8_t vdev_id,
					       uint8_t total_entries);

/**
 * wifi_pos_set_11az_failed_peers() - Update the 11az failed peers to the
 *                                    context
 * @vdev: Objmgr vdev pointer
 * @mac_addr: mac address
 */
void wifi_pos_set_11az_failed_peers(struct wlan_objmgr_vdev *vdev,
				    struct qdf_mac_addr *mac_addr);

/**
 * wifi_pos_add_peer_to_list() - Add the peer mac to the secure/unsecure
 *                               peer list in the 11az context
 * @vdev: Pointer to vdev object
 * @req: PASN peer create request pointer
 * @is_peer_create_required: True if we need to send peer create command to
 *                           firmware
 */
void wifi_pos_add_peer_to_list(struct wlan_objmgr_vdev *vdev,
			       struct wlan_pasn_request *req,
			       bool is_peer_create_required);

/**
 * wifi_pos_handle_ranging_peer_create_rsp() - Ranging peer create response
 *                                             handler
 * @psoc: Pointer to PSOC object
 * @vdev_id: vdev id
 * @peer_mac: Peer mac address
 * @status: Status
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_handle_ranging_peer_create_rsp(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					struct qdf_mac_addr *peer_mac,
					uint8_t status);

/**
 * wifi_pos_handle_ranging_peer_delete() - Handle ranging PASN peer delete
 * @psoc: Pointer to PSOC object
 * @req: PASN request
 * @vdev_id: vdev id
 * @total_entries: Total number of peers
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_handle_ranging_peer_delete(struct wlan_objmgr_psoc *psoc,
				    struct wlan_pasn_request *req,
				    uint8_t vdev_id,
				    uint8_t total_entries);

/**
 * wifi_pos_send_pasn_auth_status() - Send PASN auth status to firmware
 * @psoc: Pointer to PSOC object
 * @data: pointer to auth status data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_send_pasn_auth_status(struct wlan_objmgr_psoc *psoc,
			       struct wlan_pasn_auth_status *data);

/**
 * wifi_pos_send_pasn_peer_deauth() - Send PASN peer deauth
 * @psoc: Pointer to PSOC object
 * @peer_mac: Peer mac address
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_send_pasn_peer_deauth(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *peer_mac);

/**
 * wifi_pos_get_pasn_peer_count() - Wifi POS get total pasn peer count
 * @vdev: Pointer to vdev object
 *
 * Return: Total number of pasn peers
 */
uint8_t
wifi_pos_get_pasn_peer_count(struct wlan_objmgr_vdev *vdev);

/**
 * wifi_pos_update_pasn_peer_count() - Increment pasn peer count
 * @vdev: Pointer to vdev object
 * @is_increment: flag to indicate if peer count needs to be incremented
 *
 * Return: None
 */
void wifi_pos_update_pasn_peer_count(struct wlan_objmgr_vdev *vdev,
				     bool is_increment);

/**
 * wifi_pos_cleanup_pasn_peers() - Delete all PASN peer objects for
 *                                 given vdev
 * @psoc: Pointer to psoc object
 * @vdev: Pointer to vdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_cleanup_pasn_peers(struct wlan_objmgr_psoc *psoc,
			    struct wlan_objmgr_vdev *vdev);

/**
 * wifi_pos_vdev_delete_all_ranging_peers() - Delete all ranging peers
 *                                            associated with given vdev
 * @vdev: Vdev object pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_vdev_delete_all_ranging_peers(struct wlan_objmgr_vdev *vdev);

/**
 * wifi_pos_vdev_delete_all_ranging_peers_rsp() - Delete all vdev peers response
 *                                                handler
 * @psoc: Psoc pointer
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wifi_pos_vdev_delete_all_ranging_peers_rsp(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id);

/**
 * wifi_pos_is_delete_all_peer_in_progress() - Check if delete all pasn peers
 *                                             command is already in progress
 *                                             for a given vdev
 * @vdev: Vdev object pointer
 *
 * Return: True if delete all pasn peer is in progress
 */
bool wifi_pos_is_delete_all_peer_in_progress(struct wlan_objmgr_vdev *vdev);

/**
 * wifi_pos_set_delete_all_peer_in_progress() - API to set/unset delete all
 *                                              ranging peers is in progress
 * @vdev: Pointer to vdev object
 * @flag: value to indicate set or unset the flag
 *
 * Return: None
 */
void wifi_pos_set_delete_all_peer_in_progress(struct wlan_objmgr_vdev *vdev,
					      bool flag);
#else
static inline
QDF_STATUS wifi_pos_handle_ranging_peer_create(struct wlan_objmgr_psoc *psoc,
					       struct wlan_pasn_request *req,
					       uint8_t vdev_id,
					       uint8_t total_entries)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void wifi_pos_set_11az_failed_peers(struct wlan_objmgr_vdev *vdev,
				    struct qdf_mac_addr *mac_addr)
{}

static inline
void wifi_pos_add_peer_to_list(struct wlan_objmgr_vdev *vdev,
			       struct wlan_pasn_request *req,
			       bool is_peer_create_required)
{}

static inline QDF_STATUS
wifi_pos_handle_ranging_peer_create_rsp(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					struct qdf_mac_addr *peer_mac,
					uint8_t status)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wifi_pos_handle_ranging_peer_delete(struct wlan_objmgr_psoc *psoc,
				    struct wlan_pasn_request *req,
				    uint8_t vdev_id,
				    uint8_t total_entries)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wifi_pos_send_pasn_peer_deauth(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *peer_mac)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
wifi_pos_update_pasn_peer_count(struct wlan_objmgr_vdev *vdev,
				bool is_increment)
{}

static inline uint8_t
wifi_pos_get_pasn_peer_count(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline QDF_STATUS
wifi_pos_vdev_delete_all_ranging_peers(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
bool wifi_pos_is_delete_all_peer_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline QDF_STATUS
wifi_pos_cleanup_pasn_peers(struct wlan_objmgr_psoc *psoc,
			    struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void wifi_pos_set_delete_all_peer_in_progress(struct wlan_objmgr_vdev *vdev,
					      bool flag)
{}
#endif /* WIFI_POS_CONVERGED && WLAN_FEATURE_RTT_11AZ_SUPPORT */
#endif /* _WIFI_POS_PASN_API_H_ */
