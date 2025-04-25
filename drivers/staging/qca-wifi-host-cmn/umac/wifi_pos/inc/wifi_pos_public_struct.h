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
 * DOC: wifi_pos_public_struct.h
 * This file declares public structures of wifi positioning component
 */
#ifndef _WIFI_POS_PUBLIC_STRUCT_H_
#define _WIFI_POS_PUBLIC_STRUCT_H_

/* Include files */
#include "qdf_types.h"
#include "qdf_status.h"
#include "qdf_trace.h"
#include <wlan_cmn.h>

#define WLAN_MAX_11AZ_PEERS 16

/**
 * enum wifi_pos_pasn_peer_type  - PASN peer type
 * @WLAN_WIFI_POS_PASN_UNSECURE_PEER: Unsecure ranging peer
 * @WLAN_WIFI_POS_PASN_SECURE_PEER: Secure ranging peer
 * @WLAN_WIFI_POS_PASN_PEER_TYPE_MAX: Max peer type
 */
enum wifi_pos_pasn_peer_type {
	WLAN_WIFI_POS_PASN_UNSECURE_PEER,
	WLAN_WIFI_POS_PASN_SECURE_PEER,
	WLAN_WIFI_POS_PASN_PEER_TYPE_MAX,
};

/**
 * enum wifi_pos_pasn_peer_delete_actions  - Actions on receiving a peer
 * delete event for PASN peer
 * @WIFI_POS_PEER_DELETE_ACTION_ALREADY_DELETED: Peer is already deleted at
 * target. Cleanup the host objmgr peer.
 * @WIFI_POS_PEER_DELETE_ACTION_FLUSH_KEYS: Flush the derived keys for this
 * peer at userspace.
 */
enum wifi_pos_pasn_peer_delete_actions {
	WIFI_POS_PEER_DELETE_ACTION_ALREADY_DELETED = BIT(0),
	WIFI_POS_PEER_DELETE_ACTION_FLUSH_KEYS = BIT(1),
};

#define WIFI_POS_IS_PEER_ALREADY_DELETED(flag) \
			((flag) & WIFI_POS_PEER_DELETE_ACTION_ALREADY_DELETED)
#define WIFI_POS_IS_FLUSH_KEYS_REQUIRED(flag) \
			((flag) & WIFI_POS_PEER_DELETE_ACTION_FLUSH_KEYS)

#define CFG_RESPONDER_11AZ_NTB_SUPPORT 0x1
#define CFG_RESPONDER_11AZ_TB_SUPPORT 0x2
#define CFG_RESPONDER_11AZ_MAX_SUPPORT (CFG_RESPONDER_11AZ_NTB_SUPPORT | \
		CFG_RESPONDER_11AZ_TB_SUPPORT)
/**
 * struct wlan_pasn_request  - PASN peer create request data
 * @peer_mac: Peer mac address
 * @peer_type: Peer type of enum wifi_pos_pasn_peer_type
 * @self_mac: Self mac address to be used for frame exchange & key
 * derivation
 * @force_self_mac_usage: If this flag is true, the supplicant
 * should use the provided self mac address
 * @is_ltf_keyseed_required: Is set LTF keyseed required
 * @control_flags: Control flags to indicate if its required to flush
 * the keys
 */
struct wlan_pasn_request {
	struct qdf_mac_addr peer_mac;
	enum wifi_pos_pasn_peer_type peer_type;
	struct qdf_mac_addr self_mac;
	bool force_self_mac_usage;
	bool is_ltf_keyseed_required;
	uint16_t control_flags;
};

/**
 * struct wifi_pos_11az_context  - 11az Security context
 * @secure_peer_list: Mac address list of secure peers
 * @num_secure_peers: Total number of secure peers
 * @unsecure_peer_list: Mac address list of unsecure peers
 * @num_unsecure_peers: Total number of unsecure peers
 * @failed_peer_list: List of failed peers
 * @num_failed_peers: Total number of failed peers
 * @num_pending_peer_creation: Number of pending peer create commands for which
 * peer create confirmation is pending.
 */
struct wifi_pos_11az_context {
	struct wlan_pasn_request secure_peer_list[WLAN_MAX_11AZ_PEERS];
	uint8_t num_secure_peers;
	struct wlan_pasn_request unsecure_peer_list[WLAN_MAX_11AZ_PEERS];
	uint8_t num_unsecure_peers;
	struct qdf_mac_addr failed_peer_list[WLAN_MAX_11AZ_PEERS];
	uint8_t num_failed_peers;
	uint8_t num_pending_peer_creation;
};

/**
 * struct wifi_pos_vdev_priv_obj  - Wifi Pos module vdev private object
 * @pasn_context: 11az security peers context.
 * @num_pasn_peers: Total number of PASN peers
 * @is_delete_all_pasn_peer_in_progress: Delete all the VDEV PASN peers in
 * progress
 */
struct wifi_pos_vdev_priv_obj {
	struct wifi_pos_11az_context pasn_context;
	uint8_t num_pasn_peers;
	bool is_delete_all_pasn_peer_in_progress;
};

/**
 * enum wlan_pasn_auth_status_code  - PASN auth status code
 * @WLAN_PASN_AUTH_STATUS_SUCCESS: PASN auth is successful
 * @WLAN_PASN_AUTH_STATUS_PASN_FAILED: PASN authentication failed
 * @WLAN_PASN_AUTH_STATUS_PEER_CREATE_FAILED: PASN peer create confirm received
 * with failure status.
 * @WLAN_PASN_AUTH_STATUS_PEER_ALREADY_EXISTS: Peer already exists
 * @WLAN_PASN_AUTH_STATUS_HOST_INTERNAL_ERROR: WLAN driver internal error
 */
enum wlan_pasn_auth_status_code {
	WLAN_PASN_AUTH_STATUS_SUCCESS = 0,
	WLAN_PASN_AUTH_STATUS_PASN_FAILED = 1,
	WLAN_PASN_AUTH_STATUS_PEER_CREATE_FAILED = 2,
	WLAN_PASN_AUTH_STATUS_PEER_ALREADY_EXISTS = 3,
	WLAN_PASN_AUTH_STATUS_HOST_INTERNAL_ERROR = 4,
};

/**
 * struct wlan_pasn_auth_status_peer_info - PASN authentication status peer
 * info
 * @peer_mac: Peer mac address
 * @self_mac: Self mac address
 * @status: PASN auth status code
 */
struct wlan_pasn_auth_status_peer_info {
	struct qdf_mac_addr peer_mac;
	struct qdf_mac_addr self_mac;
	enum wlan_pasn_auth_status_code status;
};

/**
 * struct wlan_pasn_auth_status  - PASN authentication status
 * @vdev_id:  vdev_id
 * @num_peers: Number of peers for which auth status is to be sent
 * @auth_status: Auth status details
 */
struct wlan_pasn_auth_status {
	uint8_t vdev_id;
	uint8_t num_peers;
	struct wlan_pasn_auth_status_peer_info auth_status[WLAN_MAX_11AZ_PEERS];
};

/**
 * struct wlan_wifi_pos_peer_priv_obj - WLAN wifi pos peer private object
 * @is_ltf_keyseed_required: Is LTF keyseed required for peer
 */
struct wlan_wifi_pos_peer_priv_obj {
	bool is_ltf_keyseed_required;
};
#endif /* _WIFI_POS_PUBLIC_STRUCT_H_ */
