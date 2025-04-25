/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wifi_pos_pasn_api.c
 * This file defines the 11az PASN authentication related APIs for wifi_pos
 * component.
 */

#include <wlan_lmac_if_def.h>
#include "wifi_pos_api.h"
#include "wifi_pos_pasn_api.h"
#include "wifi_pos_utils_i.h"
#include "wifi_pos_main_i.h"
#include "os_if_wifi_pos.h"
#include "os_if_wifi_pos_utils.h"
#include "target_if_wifi_pos.h"
#include "target_if_wifi_pos_rx_ops.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_peer_obj.h"
#include "wlan_lmac_if_def.h"
#include "wlan_vdev_mgr_tgt_if_tx_api.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
uint8_t wifi_pos_get_pasn_peer_count(struct wlan_objmgr_vdev *vdev)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return 0;
	}

	return vdev_pos_obj->num_pasn_peers;
}

void wifi_pos_update_pasn_peer_count(struct wlan_objmgr_vdev *vdev,
				     bool is_increment)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return;
	}

	if (is_increment)
		vdev_pos_obj->num_pasn_peers++;
	else if (vdev_pos_obj->num_pasn_peers)
		vdev_pos_obj->num_pasn_peers--;

	wifi_pos_debug("Pasn peer count:%d", vdev_pos_obj->num_pasn_peers);
}
#endif

void wifi_pos_set_11az_failed_peers(struct wlan_objmgr_vdev *vdev,
				    struct qdf_mac_addr *mac_addr)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_11az_context *pasn_context;
	uint8_t i;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return;
	}

	pasn_context = &vdev_pos_obj->pasn_context;
	if (!pasn_context->num_failed_peers)
		goto add_failed_peer;

	for (i = 0; i < WLAN_MAX_11AZ_PEERS; i++) {
		if (qdf_is_macaddr_equal(mac_addr,
					 &pasn_context->failed_peer_list[i])) {
			wifi_pos_debug("Peer: " QDF_MAC_ADDR_FMT " already exists in failed list",
				       QDF_MAC_ADDR_REF(mac_addr->bytes));
			return;
		}
	}

add_failed_peer:
	for (i = 0; i < WLAN_MAX_11AZ_PEERS; i++) {
		if (qdf_is_macaddr_broadcast(
					&pasn_context->failed_peer_list[i])) {
			qdf_copy_macaddr(&pasn_context->failed_peer_list[i],
					 mac_addr);
			pasn_context->num_failed_peers++;
			wifi_pos_debug("Added failed peer: " QDF_MAC_ADDR_FMT " at idx[%d]",
				       QDF_MAC_ADDR_REF(mac_addr->bytes), i);

			return;
		}
	}

	wifi_pos_debug("Not able to set failed peer");
}

void wifi_pos_add_peer_to_list(struct wlan_objmgr_vdev *vdev,
			       struct wlan_pasn_request *req,
			       bool is_peer_create_required)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_11az_context *pasn_context;
	struct wlan_pasn_request *secure_list, *unsecure_list, *dst_entry;
	uint8_t i;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return;
	}

	pasn_context = &vdev_pos_obj->pasn_context;
	secure_list = pasn_context->secure_peer_list;
	unsecure_list = pasn_context->unsecure_peer_list;

	/* Find the 1st empty slot and copy the entry to peer list */
	for (i = 0; i < WLAN_MAX_11AZ_PEERS; i++) {
		if (req->peer_type == WLAN_WIFI_POS_PASN_SECURE_PEER)
			dst_entry = &secure_list[i];
		else
			dst_entry = &unsecure_list[i];

		/* Current slot is not empty */
		if (!qdf_is_macaddr_broadcast(&dst_entry->peer_mac))
			continue;

		*dst_entry = *req;
		if (is_peer_create_required)
			pasn_context->num_pending_peer_creation++;

		if (req->peer_type == WLAN_WIFI_POS_PASN_SECURE_PEER)
			pasn_context->num_secure_peers++;
		else
			pasn_context->num_unsecure_peers++;

		wifi_pos_debug("Added %s peer: " QDF_MAC_ADDR_FMT " at idx[%d]",
			       (req->peer_type == WLAN_WIFI_POS_PASN_SECURE_PEER) ? "secure" : "insecure",
			       QDF_MAC_ADDR_REF(dst_entry->peer_mac.bytes), i);

		break;
	}
}

/**
 * wifi_pos_move_peers_to_fail_list  - Move the peers in secure/insecure list
 * to failed peer list
 * @vdev: Vdev pointer
 * @peer_mac: Peer mac address
 * @peer_type: Secure or insecure PASN peer
 *
 * Return: None
 */
static
void wifi_pos_move_peers_to_fail_list(struct wlan_objmgr_vdev *vdev,
				      struct qdf_mac_addr *peer_mac,
				      enum wifi_pos_pasn_peer_type peer_type)
{
	uint8_t i;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_11az_context *pasn_context;
	struct wlan_pasn_request *secure_list, *unsecure_list, *list = NULL;
	struct qdf_mac_addr entry_to_copy;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return;
	}

	pasn_context = &vdev_pos_obj->pasn_context;

	/*
	 * Broadcast mac address will be sent by caller when initiate
	 * external auth fails and to move the entire list to failed
	 * peers list
	 */
	if (qdf_is_macaddr_broadcast(peer_mac)) {
		/* Clear the entire list and move it to failed peers list */
		if (peer_type == WLAN_WIFI_POS_PASN_SECURE_PEER)
			list = pasn_context->secure_peer_list;
		else if (peer_type == WLAN_WIFI_POS_PASN_UNSECURE_PEER)
			list = pasn_context->unsecure_peer_list;

		if (!list) {
			wifi_pos_err("No Valid list exists");
			return;
		}

		for (i = 0; i < WLAN_MAX_11AZ_PEERS; i++) {
			/*
			 * if valid entry exist in the list, set that mac
			 * address to failed list and clear that mac from the
			 * secure/insecure list
			 */
			if (!qdf_is_macaddr_broadcast(&list[i].peer_mac)) {
				wifi_pos_set_11az_failed_peers(
						vdev, &list[i].peer_mac);
				qdf_set_macaddr_broadcast(&list[i].peer_mac);
			}
		}

		return;
	}

	secure_list = pasn_context->secure_peer_list;
	unsecure_list = pasn_context->unsecure_peer_list;
	/*
	 * This condition is hit when peer create confirm for a pasn
	 * peer is received with failure status
	 */
	for (i = 0; i < WLAN_MAX_11AZ_PEERS; i++) {
		/*
		 * Clear the individual entry that exist for the given
		 * mac address in secure/insecure list
		 */
		if (qdf_is_macaddr_equal(peer_mac, &secure_list[i].peer_mac)) {
			entry_to_copy = secure_list[i].peer_mac;
			qdf_set_macaddr_broadcast(&secure_list[i].peer_mac);
			pasn_context->num_secure_peers--;
		} else if (qdf_is_macaddr_equal(peer_mac,
			   &unsecure_list[i].peer_mac)) {
			entry_to_copy = unsecure_list[i].peer_mac;
			qdf_set_macaddr_broadcast(&unsecure_list[i].peer_mac);
			pasn_context->num_unsecure_peers--;
		} else {
			continue;
		}

		wifi_pos_set_11az_failed_peers(vdev, &entry_to_copy);
		break;
	}
}

static QDF_STATUS
wifi_pos_request_external_pasn_auth(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    struct wlan_pasn_request *peer_list,
				    uint8_t num_peers)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_osif_ops *osif_cb;
	QDF_STATUS status;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return QDF_STATUS_E_INVAL;

	osif_cb = wifi_pos_get_osif_callbacks();
	if (!osif_cb || !osif_cb->osif_initiate_pasn_cb) {
		wifi_pos_err("OSIF %s cb is NULL",
			     !osif_cb ? "" : "PASN");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return QDF_STATUS_E_FAILURE;
	}

	status = osif_cb->osif_initiate_pasn_cb(vdev, peer_list,
						num_peers, true);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("Initiate PASN auth failed");

	return status;
}

static QDF_STATUS
wifi_pos_request_flush_pasn_keys(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev,
				 struct wlan_pasn_request *peer_list,
				 uint8_t num_peers)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_osif_ops *osif_cb;
	QDF_STATUS status;

	osif_cb = wifi_pos_get_osif_callbacks();
	if (!osif_cb || !osif_cb->osif_initiate_pasn_cb) {
		wifi_pos_err("OSIF %s cb is NULL",
			     !osif_cb ? "" : "PASN");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return QDF_STATUS_E_FAILURE;
	}

	status = osif_cb->osif_initiate_pasn_cb(vdev, peer_list, num_peers,
						false);

	return status;
}

static QDF_STATUS
wifi_pos_check_and_initiate_pasn_authentication(struct wlan_objmgr_psoc *psoc,
						struct wlan_objmgr_vdev *vdev,
						struct wifi_pos_11az_context *pasn_ctx)
{
	struct qdf_mac_addr bcast_mac = QDF_MAC_ADDR_BCAST_INIT;
	QDF_STATUS status;

	if (pasn_ctx->num_pending_peer_creation ||
	    !pasn_ctx->num_secure_peers)
		return QDF_STATUS_SUCCESS;

	status = wifi_pos_request_external_pasn_auth(psoc, vdev,
						     pasn_ctx->secure_peer_list,
						     pasn_ctx->num_secure_peers);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_pos_err("Initiate Pasn Authentication failed");
		wifi_pos_move_peers_to_fail_list(vdev, &bcast_mac,
						 WLAN_WIFI_POS_PASN_SECURE_PEER);
		/* TODO send PASN_STATUS cmd from here */
	}

	return status;
}

QDF_STATUS wifi_pos_handle_ranging_peer_create(struct wlan_objmgr_psoc *psoc,
					       struct wlan_pasn_request *req,
					       uint8_t vdev_id,
					       uint8_t total_entries)
{
	struct wifi_pos_legacy_ops *legacy_cb;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_vdev *vdev;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_11az_context *pasn_context;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i;

	legacy_cb = wifi_pos_get_legacy_ops();
	if (!legacy_cb || !legacy_cb->pasn_peer_create_cb) {
		wifi_pos_err("legacy callbacks is not registered");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_WIFI_POS_CORE_ID);
	if (!vdev) {
		wifi_pos_err("Vdev object is null");
		return QDF_STATUS_E_FAILURE;
	}

	wifi_pos_debug("vdev:%d PASN peer create request received. Num peers:%d",
		       vdev_id, total_entries);
	for (i = 0; i < total_entries; i++) {
		peer = wlan_objmgr_get_peer_by_mac(psoc, req[i].peer_mac.bytes,
						   WLAN_WIFI_POS_CORE_ID);
		/*
		 * If already PASN peer is found, then this is a request to
		 * initiate PASN authentication alone and not to send
		 * peer create to fw
		 */
		if (peer &&
		    (wlan_peer_get_peer_type(peer) == WLAN_PEER_RTT_PASN)) {
			wifi_pos_debug("PASN Peer: " QDF_MAC_ADDR_FMT "already exists",
				       QDF_MAC_ADDR_REF(req[i].peer_mac.bytes));
			wifi_pos_add_peer_to_list(vdev, &req[i], false);

			if (req[i].is_ltf_keyseed_required)
				wifi_pos_set_peer_ltf_keyseed_required(peer,
								       true);
			else
				wifi_pos_set_peer_ltf_keyseed_required(peer,
								       false);
			wlan_objmgr_peer_release_ref(peer,
						     WLAN_WIFI_POS_CORE_ID);
			continue;
		} else if (peer) {
			/*
			 * If a peer with given mac address already exists which
			 * is not a PASN peer, then move this peer to failed
			 * list
			 */
			wifi_pos_debug("Peer: " QDF_MAC_ADDR_FMT "of type:%d already exist",
				       QDF_MAC_ADDR_REF(req[i].peer_mac.bytes),
				       wlan_peer_get_peer_type(peer));
			wifi_pos_set_11az_failed_peers(vdev, &req[i].peer_mac);
			wlan_objmgr_peer_release_ref(peer,
						     WLAN_WIFI_POS_CORE_ID);
			continue;
		}

		status = legacy_cb->pasn_peer_create_cb(psoc, &req[i].peer_mac,
							vdev_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			wifi_pos_set_11az_failed_peers(vdev, &req[i].peer_mac);
			continue;
		}

		wifi_pos_update_pasn_peer_count(vdev, true);
		if (req[i].is_ltf_keyseed_required) {
			peer = wlan_objmgr_get_peer_by_mac(psoc,
							   req[i].peer_mac.bytes,
							   WLAN_WIFI_POS_CORE_ID);
			if (peer) {
				wifi_pos_set_peer_ltf_keyseed_required(peer,
								       true);
				wlan_objmgr_peer_release_ref(peer,
							     WLAN_WIFI_POS_CORE_ID);
			}
		}

		/* Track the peers only for I-STA mode */
		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)
			wifi_pos_add_peer_to_list(vdev, &req[i], true);
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		goto end;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * If peer already exists for all the entries provided in the request,
	 * then fw peer create will not be sent again. Just the secure list
	 * will be updated and num_pending_peer_creation will be 0.
	 * In this case initiate the PASN auth directly without waiting for
	 * peer create response.
	 */
	pasn_context = &vdev_pos_obj->pasn_context;
	status = wifi_pos_check_and_initiate_pasn_authentication(psoc, vdev,
								 pasn_context);
end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);

	return status;
}

QDF_STATUS
wifi_pos_handle_ranging_peer_create_rsp(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					struct qdf_mac_addr *peer_mac,
					uint8_t peer_create_status)
{
	struct wlan_objmgr_vdev *vdev;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_11az_context *pasn_context;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_WIFI_POS_CORE_ID);
	if (!vdev) {
		wifi_pos_err("Vdev object is null");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return QDF_STATUS_E_FAILURE;
	}

	pasn_context = &vdev_pos_obj->pasn_context;
	if (pasn_context->num_pending_peer_creation)
		pasn_context->num_pending_peer_creation--;

	wifi_pos_debug("Received peer create response for " QDF_MAC_ADDR_FMT " status:%d pending_count:%d",
		       QDF_MAC_ADDR_REF(peer_mac->bytes), peer_create_status,
		       pasn_context->num_pending_peer_creation);

	if (peer_create_status) {
		wifi_pos_move_peers_to_fail_list(vdev, peer_mac,
						 WLAN_WIFI_POS_PASN_PEER_TYPE_MAX);
		wifi_pos_update_pasn_peer_count(vdev, false);
	}

	status = wifi_pos_check_and_initiate_pasn_authentication(psoc, vdev,
								 pasn_context);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wifi_pos_handle_ranging_peer_delete(struct wlan_objmgr_psoc *psoc,
					       struct wlan_pasn_request *req,
					       uint8_t vdev_id,
					       uint8_t total_entries)
{
	struct wifi_pos_legacy_ops *legacy_cb;
	struct wlan_objmgr_peer *peer;
	struct wlan_pasn_request *del_peer_list;
	struct wlan_objmgr_vdev *vdev;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	bool no_fw_peer_delete;
	uint8_t peer_count = 0, i;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_WIFI_POS_CORE_ID);
	if (!vdev) {
		wifi_pos_err("Vdev object is null");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (vdev_pos_obj->is_delete_all_pasn_peer_in_progress) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		wifi_pos_err("Vdev delete all peer in progress. Ignore individual peer delete");
		return QDF_STATUS_SUCCESS;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);

	legacy_cb = wifi_pos_get_legacy_ops();
	if (!legacy_cb || !legacy_cb->pasn_peer_delete_cb) {
		wifi_pos_err("legacy callback is not registered");
		return QDF_STATUS_E_FAILURE;
	}

	del_peer_list = qdf_mem_malloc(sizeof(*del_peer_list) * total_entries);
	if (!del_peer_list)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < total_entries; i++) {
		peer = wlan_objmgr_get_peer_by_mac(psoc, req[i].peer_mac.bytes,
						   WLAN_WIFI_POS_CORE_ID);
		if (peer &&
		    (wlan_peer_get_peer_type(peer) == WLAN_PEER_RTT_PASN)) {
			no_fw_peer_delete = WIFI_POS_IS_PEER_ALREADY_DELETED(
							req[i].control_flags);
			wifi_pos_debug("Delete PASN Peer: " QDF_MAC_ADDR_FMT,
				       QDF_MAC_ADDR_REF(req[i].peer_mac.bytes));

			del_peer_list[peer_count] = req[i];
			peer_count++;

			status = legacy_cb->pasn_peer_delete_cb(
					psoc, &req[i].peer_mac,
					vdev_id, no_fw_peer_delete);

			wlan_objmgr_peer_release_ref(peer,
						     WLAN_WIFI_POS_CORE_ID);
			continue;
		} else {
			wifi_pos_debug("PASN Peer: " QDF_MAC_ADDR_FMT "doesn't exist",
				       QDF_MAC_ADDR_REF(req[i].peer_mac.bytes));
			if (peer)
				wlan_objmgr_peer_release_ref(
						peer, WLAN_WIFI_POS_CORE_ID);

			continue;
		}
	}

	if (!peer_count) {
		wifi_pos_debug("No Peers to delete ");
		goto no_peer;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_WIFI_POS_CORE_ID);
	if (!vdev) {
		wifi_pos_err("Vdev object is null");
		qdf_mem_free(del_peer_list);
		return QDF_STATUS_E_FAILURE;
	}

	status = wifi_pos_request_flush_pasn_keys(psoc, vdev,
						  del_peer_list,
						  peer_count);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("Failed to indicate peer deauth to userspace");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);

no_peer:
	qdf_mem_free(del_peer_list);

	return status;
}

QDF_STATUS
wifi_pos_send_pasn_auth_status(struct wlan_objmgr_psoc *psoc,
			       struct wlan_pasn_auth_status *data)
{
	struct wlan_lmac_if_wifi_pos_tx_ops *tx_ops;
	QDF_STATUS status;
	uint8_t vdev_id = data->vdev_id;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct wifi_pos_11az_context *pasn_context;
	struct wlan_objmgr_vdev *vdev;
	uint8_t i, failed_peers_counter = 0, total_peers_to_fill = 0;

	tx_ops = wifi_pos_get_tx_ops(psoc);
	if (!tx_ops || !tx_ops->send_rtt_pasn_auth_status) {
		wifi_pos_err("%s is null",
			     tx_ops ? "Tx_ops" : "send_auth_status cb");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_WIFI_POS_CORE_ID);
	if (!vdev) {
		wifi_pos_err("vdev obj is null");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		return QDF_STATUS_E_FAILURE;
	}

	pasn_context = &vdev_pos_obj->pasn_context;
	total_peers_to_fill = data->num_peers + pasn_context->num_failed_peers;
	for (i = data->num_peers; i < total_peers_to_fill; i++) {
		data->auth_status[i].peer_mac =
			pasn_context->failed_peer_list[failed_peers_counter];
		data->auth_status[i].status =
			WLAN_PASN_AUTH_STATUS_PEER_CREATE_FAILED;

		failed_peers_counter++;
		if (failed_peers_counter >= pasn_context->num_failed_peers)
			break;
	}

	status = tx_ops->send_rtt_pasn_auth_status(psoc, data);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("Failed to send PASN authentication status");

	wifi_pos_init_11az_context(vdev_pos_obj);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);

	return status;
}

QDF_STATUS
wifi_pos_send_pasn_peer_deauth(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *peer_mac)
{
	struct wlan_lmac_if_wifi_pos_tx_ops *tx_ops;
	QDF_STATUS status;

	tx_ops = wifi_pos_get_tx_ops(psoc);
	if (!tx_ops || !tx_ops->send_rtt_pasn_deauth) {
		wifi_pos_err("%s is null",
			     tx_ops ? "Tx_ops" : "send_pasn deauth cb");
		return QDF_STATUS_E_FAILURE;
	}

	status = tx_ops->send_rtt_pasn_deauth(psoc, peer_mac);

	return status;
}

QDF_STATUS
wifi_pos_set_peer_ltf_keyseed_required(struct wlan_objmgr_peer *peer,
				       bool value)
{
	struct wlan_wifi_pos_peer_priv_obj *peer_priv;

	peer_priv = wifi_pos_get_peer_private_object(peer);
	if (!peer_priv) {
		wifi_pos_err("peer private object is null");
		return QDF_STATUS_E_FAILURE;
	}

	peer_priv->is_ltf_keyseed_required = value;
	wifi_pos_debug("peer_mac:" QDF_MAC_ADDR_FMT " value:%d",
		       QDF_MAC_ADDR_REF(wlan_peer_get_macaddr(peer)), value);

	return QDF_STATUS_SUCCESS;
}

bool wifi_pos_is_ltf_keyseed_required_for_peer(struct wlan_objmgr_peer *peer)
{
	struct wlan_wifi_pos_peer_priv_obj *peer_priv;

	peer_priv = wifi_pos_get_peer_private_object(peer);
	if (!peer_priv) {
		wifi_pos_err("peer private object is null");
		return QDF_STATUS_E_FAILURE;
	}

	return peer_priv->is_ltf_keyseed_required;
}

static
void wifi_pos_delete_objmgr_ranging_peer(struct wlan_objmgr_psoc *psoc,
					 void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = object;
	struct wlan_objmgr_vdev *vdev = arg;
	uint8_t vdev_id, peer_vdev_id;
	enum wlan_peer_type peer_type;
	QDF_STATUS status;

	if (!peer) {
		wifi_pos_err("Peer is NULL");
		return;
	}

	peer_type = wlan_peer_get_peer_type(peer);
	if (peer_type != WLAN_PEER_RTT_PASN)
		return;

	if (!vdev) {
		wifi_pos_err("VDEV is NULL");
		return;
	}

	vdev_id = wlan_vdev_get_id(vdev);
	peer_vdev_id = wlan_vdev_get_id(wlan_peer_get_vdev(peer));
	if (vdev_id != peer_vdev_id)
		return;

	status = wlan_objmgr_peer_obj_delete(peer);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("Failed to delete peer");

	wifi_pos_update_pasn_peer_count(vdev, false);
}

QDF_STATUS
wifi_pos_cleanup_pasn_peers(struct wlan_objmgr_psoc *psoc,
			    struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;

	wifi_pos_debug("Iterate and delete PASN peers");
	status = wlan_objmgr_iterate_obj_list(psoc, WLAN_PEER_OP,
					      wifi_pos_delete_objmgr_ranging_peer,
					      vdev, 0, WLAN_WIFI_POS_CORE_ID);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("Delete objmgr peers failed");

	/*
	 * PASN Peer count should be zero here
	 */
	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (vdev_pos_obj)
		vdev_pos_obj->num_pasn_peers = 0;

	return status;
}

QDF_STATUS
wifi_pos_vdev_delete_all_ranging_peers(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct vdev_mlme_obj *vdev_mlme;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	struct peer_delete_all_params param;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (!vdev_pos_obj->num_pasn_peers)
		return QDF_STATUS_SUCCESS;

	vdev_pos_obj->is_delete_all_pasn_peer_in_progress = true;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		wifi_pos_err(" VDEV MLME component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	param.vdev_id = wlan_vdev_get_id(vdev);
	param.peer_type_bitmap = BIT(WLAN_PEER_RTT_PASN);

	status = tgt_vdev_mgr_peer_delete_all_send(vdev_mlme, &param);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("Send vdev delete all peers failed");

	return status;
}

QDF_STATUS
wifi_pos_vdev_delete_all_ranging_peers_rsp(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id)
{
	struct wifi_pos_legacy_ops *legacy_cb;
	struct wlan_objmgr_vdev *vdev;
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_WIFI_POS_CORE_ID);
	if (!vdev) {
		wifi_pos_err(" VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		return QDF_STATUS_E_FAILURE;
	}

	status = wifi_pos_cleanup_pasn_peers(psoc, vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		return status;
	}

	/*
	 * Should have deleted all the pasn peers when we reach here
	 */
	vdev_pos_obj->is_delete_all_pasn_peer_in_progress = false;
	vdev_pos_obj->num_pasn_peers = 0;

	legacy_cb = wifi_pos_get_legacy_ops();
	if (!legacy_cb || !legacy_cb->pasn_vdev_delete_resume_cb) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);
		wifi_pos_err("legacy callbacks is not registered");
		return QDF_STATUS_E_FAILURE;
	}

	status = legacy_cb->pasn_vdev_delete_resume_cb(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_pos_err("Delete all PASN peer failed");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_WIFI_POS_CORE_ID);

	return status;
}

bool wifi_pos_is_delete_all_peer_in_progress(struct wlan_objmgr_vdev *vdev)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return false;
	}

	return vdev_pos_obj->is_delete_all_pasn_peer_in_progress;
}

void wifi_pos_set_delete_all_peer_in_progress(struct wlan_objmgr_vdev *vdev,
					      bool flag)
{
	struct wifi_pos_vdev_priv_obj *vdev_pos_obj;

	vdev_pos_obj = wifi_pos_get_vdev_priv_obj(vdev);
	if (!vdev_pos_obj) {
		wifi_pos_err("Wifi pos vdev priv obj is null");
		return;
	}

	vdev_pos_obj->is_delete_all_pasn_peer_in_progress = flag;
}
