/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wlan_mlo_mgr_main.h"
#include "qdf_module.h"
#include "qdf_types.h"
#include "wlan_cmn.h"
#include "wlan_mlo_mgr_msgq.h"
#include "wlan_objmgr_peer_obj.h"
#include "wlan_mlo_mgr_peer.h"
#include "wlan_mlo_mgr_ap.h"
#include "wlan_crypto_global_api.h"
#include "wlan_mlo_mgr_setup.h"
#include "wlan_utility.h"
#include "wlan_mlo_epcs.h"

static void mlo_partner_peer_create_post(struct wlan_mlo_dev_context *ml_dev,
					 struct wlan_objmgr_vdev *vdev_link,
					 struct wlan_mlo_peer_context *ml_peer,
					 qdf_nbuf_t frm_buf,
					 struct mlo_partner_info *ml_info)
{
	struct peer_create_notif_s peer_create;
	QDF_STATUS status;
	uint8_t i;
	uint8_t link_id;

	if (wlan_objmgr_vdev_try_get_ref(vdev_link, WLAN_MLO_MGR_ID) ==
							QDF_STATUS_SUCCESS) {
		peer_create.vdev_link = vdev_link;
	} else {
		mlo_err("VDEV is not in created state");
		return;
	}

	wlan_mlo_peer_get_ref(ml_peer);
	peer_create.ml_peer = ml_peer;
	link_id = wlan_vdev_get_link_id(vdev_link);
	for (i = 0; i < ml_info->num_partner_links; i++) {
		if (link_id != ml_info->partner_link_info[i].link_id)
			continue;

		qdf_copy_macaddr(&peer_create.addr,
				 &ml_info->partner_link_info[i].link_addr);
		break;
	}

	status = mlo_peer_create_get_frm_buf(ml_peer, &peer_create, frm_buf);

	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_mlo_peer_release_ref(ml_peer);
		wlan_objmgr_vdev_release_ref(vdev_link, WLAN_MLO_MGR_ID);
		mlo_err("nbuf clone is failed");
		return;
	}

	status = mlo_msgq_post(MLO_PEER_CREATE, ml_dev, &peer_create);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(frm_buf);
		wlan_mlo_peer_release_ref(ml_peer);
		wlan_objmgr_vdev_release_ref(vdev_link, WLAN_MLO_MGR_ID);
	}
}

static void mlo_partner_peer_reassoc_post(struct wlan_mlo_dev_context *ml_dev,
					  struct wlan_objmgr_vdev *vdev_link,
					  struct wlan_mlo_peer_context *ml_peer,
					  qdf_nbuf_t frm_buf,
					  struct mlo_partner_info *ml_info)
{
	struct peer_create_notif_s peer_create;
	QDF_STATUS status;
	uint8_t i;
	uint8_t link_id;

	if (wlan_objmgr_vdev_try_get_ref(vdev_link, WLAN_MLO_MGR_ID) ==
							QDF_STATUS_SUCCESS) {
		peer_create.vdev_link = vdev_link;
	} else {
		mlo_err("VDEV is not in created state");
		return;
	}

	wlan_mlo_peer_get_ref(ml_peer);
	peer_create.ml_peer = ml_peer;
	link_id = wlan_vdev_get_link_id(vdev_link);
	for (i = 0; i < ml_info->num_partner_links; i++) {
		if (link_id != ml_info->partner_link_info[i].link_id)
			continue;

		qdf_copy_macaddr(&peer_create.addr,
				 &ml_info->partner_link_info[i].link_addr);
		break;
	}

	status = mlo_peer_create_get_frm_buf(ml_peer, &peer_create, frm_buf);

	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_mlo_peer_release_ref(ml_peer);
		wlan_objmgr_vdev_release_ref(vdev_link, WLAN_MLO_MGR_ID);
		mlo_err("nbuf clone is failed");
		return;
	}

	status = mlo_msgq_post(MLO_PEER_REASSOC, ml_dev, &peer_create);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_nbuf_free(frm_buf);
		wlan_mlo_peer_release_ref(ml_peer);
		wlan_objmgr_vdev_release_ref(vdev_link, WLAN_MLO_MGR_ID);
	}
}

static void mlo_link_peer_assoc_notify(struct wlan_mlo_dev_context *ml_dev,
				       struct wlan_objmgr_peer *peer)
{
	struct peer_assoc_notify_s peer_assoc;
	QDF_STATUS status;

	peer_assoc.peer = peer;
	status = mlo_msgq_post(MLO_PEER_ASSOC, ml_dev, &peer_assoc);
	if (status != QDF_STATUS_SUCCESS)
		wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
}

static void mlo_link_peer_send_assoc_fail(struct wlan_mlo_dev_context *ml_dev,
					  struct wlan_objmgr_peer *peer)
{
	struct peer_assoc_fail_notify_s peer_assoc_fail;
	QDF_STATUS status;

	peer_assoc_fail.peer = peer;
	status = mlo_msgq_post(MLO_PEER_ASSOC_FAIL, ml_dev, &peer_assoc_fail);
	if (status != QDF_STATUS_SUCCESS)
		wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
}

static void mlo_link_peer_disconnect_notify(struct wlan_mlo_dev_context *ml_dev,
					    struct wlan_objmgr_peer *peer)
{
	struct peer_discon_notify_s peer_disconn;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE opmode;

	vdev = wlan_peer_get_vdev(peer);
	opmode = wlan_vdev_mlme_get_opmode(vdev);

	if (opmode == QDF_SAP_MODE) {
		peer_disconn.peer = peer;
		status = mlo_msgq_post(MLO_PEER_DISCONNECT, ml_dev,
				       &peer_disconn);
		if (status != QDF_STATUS_SUCCESS)
			wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
	} else {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
	}
}

static void mlo_link_peer_deauth_init(struct wlan_mlo_dev_context *ml_dev,
				      struct wlan_objmgr_peer *peer,
				      uint8_t is_disassoc)
{
	struct peer_deauth_notify_s peer_deauth;
	QDF_STATUS status;

	peer_deauth.peer = peer;
	peer_deauth.is_disassoc = is_disassoc;
	status = mlo_msgq_post(MLO_PEER_DEAUTH, ml_dev, &peer_deauth);
	if (status != QDF_STATUS_SUCCESS)
		wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
}

#ifdef UMAC_MLO_AUTH_DEFER
static void mlo_peer_process_pending_auth(struct wlan_mlo_dev_context *ml_dev,
					  struct wlan_mlo_peer_context *ml_peer)
{
	struct peer_auth_process_notif_s peer_auth;
	struct mlpeer_auth_params *recv_auth;
	uint8_t i;
	QDF_STATUS status;

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		mlo_peer_lock_acquire(ml_peer);
		recv_auth = ml_peer->pending_auth[i];
		if (!recv_auth) {
			mlo_peer_lock_release(ml_peer);
			continue;
		}
		peer_auth.auth_params = recv_auth;
		ml_peer->pending_auth[i] = NULL;

		mlo_peer_lock_release(ml_peer);

		status = mlo_msgq_post(MLO_PEER_PENDING_AUTH, ml_dev,
				       &peer_auth);
		if (QDF_IS_STATUS_ERROR(status))
			mlo_peer_free_auth_param(peer_auth.auth_params);
	}
}
#else
static void mlo_peer_process_pending_auth(struct wlan_mlo_dev_context *ml_dev,
					  struct wlan_mlo_peer_context *ml_peer)
{
}
#endif

QDF_STATUS
wlan_mlo_peer_is_disconnect_progress(struct wlan_mlo_peer_context *ml_peer)
{
	QDF_STATUS status;

	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state == ML_PEER_DISCONN_INITIATED)
		status = QDF_STATUS_SUCCESS;
	else
		status = QDF_STATUS_E_FAILURE;

	mlo_peer_lock_release(ml_peer);

	return status;
}

QDF_STATUS wlan_mlo_peer_is_assoc_done(struct wlan_mlo_peer_context *ml_peer)
{
	QDF_STATUS status;

	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state == ML_PEER_ASSOC_DONE)
		status = QDF_STATUS_SUCCESS;
	else
		status = QDF_STATUS_E_FAILURE;

	mlo_peer_lock_release(ml_peer);

	return status;
}

qdf_export_symbol(wlan_mlo_peer_is_assoc_done);

struct wlan_objmgr_peer *wlan_mlo_peer_get_assoc_peer(
					struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *assoc_peer = NULL;

	if (!ml_peer)
		return NULL;

	mlo_peer_lock_acquire(ml_peer);

	peer_entry = &ml_peer->peer_list[0];

	if (peer_entry->link_peer)
		assoc_peer = peer_entry->link_peer;

	mlo_peer_lock_release(ml_peer);

	return assoc_peer;
}

qdf_export_symbol(wlan_mlo_peer_get_assoc_peer);

struct wlan_objmgr_peer *wlan_mlo_peer_get_bridge_peer(
					struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry = NULL;
	struct wlan_objmgr_peer *bridge_peer = NULL;
	struct wlan_objmgr_peer *link_peer = NULL;
	int i = 0;

	if (!ml_peer)
		return NULL;

	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry)
			continue;

		link_peer = peer_entry->link_peer;
		if (!link_peer)
			continue;

		if (wlan_peer_get_peer_type(link_peer) ==
		    WLAN_PEER_MLO_BRIDGE) {
			if (peer_entry->is_primary)
				bridge_peer = link_peer;
			else
				mlo_err("Bridge peer is not primary");

			break;
		}
	}
	mlo_peer_lock_release(ml_peer);

	return bridge_peer;
}

qdf_export_symbol(wlan_mlo_peer_get_bridge_peer);

struct wlan_objmgr_vdev *
wlan_mlo_peer_get_primary_link_vdev(struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry = NULL;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_vdev *link_vdev;
	uint8_t i;

	if (!ml_peer) {
		mlo_err("ml_peer is null");
		return NULL;
	}
	mlo_peer_lock_acquire(ml_peer);

	if ((ml_peer->mlpeer_state != ML_PEER_CREATED) &&
	    (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE)) {
		mlo_peer_lock_release(ml_peer);
		mlo_err("ml_peer is not created and association is not done");
		return NULL;
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		link_peer = peer_entry->link_peer;
		if (!link_peer)
			continue;

		if (peer_entry->is_primary) {
			link_vdev = wlan_peer_get_vdev(link_peer);
			if (!link_vdev) {
				mlo_peer_lock_release(ml_peer);
				mlo_err("link vdev not found");
				return NULL;
			}
			if (wlan_objmgr_vdev_try_get_ref(link_vdev, WLAN_MLO_MGR_ID) !=
					QDF_STATUS_SUCCESS) {
				mlo_peer_lock_release(ml_peer);
				mlo_err("taking ref failed");
				return NULL;
			}
			mlo_peer_lock_release(ml_peer);
			return link_vdev;
		}
	}
	mlo_peer_lock_release(ml_peer);
	mlo_err("None of the peer is designated as primary");

	return NULL;
}

bool mlo_peer_is_assoc_peer(struct wlan_mlo_peer_context *ml_peer,
			    struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	bool is_assoc_peer = false;

	if (!ml_peer || !peer)
		return is_assoc_peer;

	peer_entry = &ml_peer->peer_list[0];

	if (peer_entry->link_peer == peer)
		is_assoc_peer = true;

	return is_assoc_peer;
}

bool wlan_mlo_peer_is_assoc_peer(struct wlan_mlo_peer_context *ml_peer,
				 struct wlan_objmgr_peer *peer)
{
	bool is_assoc_peer = false;

	if (!ml_peer || !peer)
		return is_assoc_peer;

	mlo_peer_lock_acquire(ml_peer);

	is_assoc_peer = mlo_peer_is_assoc_peer(ml_peer, peer);

	mlo_peer_lock_release(ml_peer);

	return is_assoc_peer;
}

bool wlan_mlo_peer_is_link_peer(struct wlan_mlo_peer_context *ml_peer,
				struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	bool is_link_peer = false;
	uint16_t i;

	if (!ml_peer || !peer)
		return is_link_peer;

	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer)
			continue;

		if (peer == peer_entry->link_peer) {
			is_link_peer = true;
			break;
		}
	}

	mlo_peer_lock_release(ml_peer);

	return is_link_peer;
}

void wlan_mlo_partner_peer_assoc_post(struct wlan_objmgr_peer *assoc_peer)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_peer *link_peers[MAX_MLO_LINK_PEERS];
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint16_t i;

	ml_peer = assoc_peer->mlo_peer_ctx;
	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state != ML_PEER_CREATED) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	ml_peer->mlpeer_state = ML_PEER_ASSOC_DONE;
	ml_dev = ml_peer->ml_dev;

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		link_peers[i] = NULL;
		peer_entry = &ml_peer->peer_list[i];

		if (!peer_entry->link_peer)
			continue;

		if (peer_entry->link_peer == assoc_peer)
			continue;

		link_peer = peer_entry->link_peer;

		if (wlan_objmgr_peer_try_get_ref(link_peer, WLAN_MLO_MGR_ID) !=
						 QDF_STATUS_SUCCESS)
			continue;

		link_peers[i] = link_peer;
	}
	wlan_mlo_peer_wsi_link_add(ml_peer);
	mlo_peer_lock_release(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		if (!link_peers[i])
			continue;

		/* Prepare and queue message */
		mlo_link_peer_assoc_notify(ml_dev, link_peers[i]);
	}
}

void wlan_mlo_link_peer_assoc_set(struct wlan_objmgr_peer *peer, bool is_sent)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint16_t i;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];

		if (!peer_entry->link_peer)
			continue;

		if (peer_entry->link_peer == peer) {
			peer_entry->peer_assoc_sent = is_sent;
			break;
		}
	}
	mlo_peer_lock_release(ml_peer);
}

qdf_export_symbol(wlan_mlo_link_peer_assoc_set);

void wlan_mlo_peer_get_del_hw_bitmap(struct wlan_objmgr_peer *peer,
				     uint32_t *hw_link_id_bitmap)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint16_t i;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];

		if (!peer_entry->link_peer)
			continue;

		if (peer_entry->link_peer == peer) {
			/* Peer assoc is not sent, no need to send bitmap */
			if (!peer_entry->peer_assoc_sent)
				break;

			continue;
		}
		if (!peer_entry->peer_assoc_sent)
			*hw_link_id_bitmap |= 1 << peer_entry->hw_link_id;
	}
	mlo_peer_lock_release(ml_peer);
}

qdf_export_symbol(wlan_mlo_peer_get_del_hw_bitmap);

void
wlan_mlo_peer_deauth_init(struct wlan_mlo_peer_context *ml_peer,
			  struct wlan_objmgr_peer *src_peer,
			  uint8_t is_disassoc)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_peer *link_peers[MAX_MLO_LINK_PEERS];
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint16_t i;
	uint8_t deauth_sent = 0;

	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state == ML_PEER_DISCONN_INITIATED) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	ml_dev = ml_peer->ml_dev;

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		link_peers[i] = NULL;
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer)
			continue;

		link_peer = peer_entry->link_peer;

		if (wlan_objmgr_peer_try_get_ref(link_peer, WLAN_MLO_MGR_ID) !=
						QDF_STATUS_SUCCESS)
			continue;

		link_peers[i] = link_peer;
	}

	ml_peer->mlpeer_state = ML_PEER_DISCONN_INITIATED;

	mlo_peer_lock_release(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		if (!link_peers[i])
			continue;

		/* Prepare and queue message */
		/* skip sending deauth on src peer */
		if ((deauth_sent) ||
		    (src_peer && (src_peer == link_peers[i]))) {
			mlo_link_peer_disconnect_notify(ml_dev, link_peers[i]);
		} else {
			mlo_link_peer_deauth_init(ml_dev, link_peers[i],
						  is_disassoc);
			deauth_sent = 1;
		}
	}
}

qdf_export_symbol(wlan_mlo_peer_deauth_init);

void wlan_mlo_peer_delete(struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_peer *link_peers[MAX_MLO_LINK_PEERS];
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint16_t i;

	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state == ML_PEER_DISCONN_INITIATED) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	ml_dev = ml_peer->ml_dev;

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		link_peers[i] = NULL;
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer)
			continue;

		link_peer = peer_entry->link_peer;

		if (wlan_objmgr_peer_try_get_ref(link_peer, WLAN_MLO_MGR_ID) !=
						QDF_STATUS_SUCCESS)
			continue;

		link_peers[i] = link_peer;
	}

	ml_peer->mlpeer_state = ML_PEER_DISCONN_INITIATED;

	mlo_peer_lock_release(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		if (!link_peers[i])
			continue;

		mlo_link_peer_disconnect_notify(ml_dev, link_peers[i]);
	}
}

void
wlan_mlo_partner_peer_create_failed_notify(
				struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_peer *link_peers[MAX_MLO_LINK_PEERS];
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint16_t i;

	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state == ML_PEER_DISCONN_INITIATED) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	ml_peer->mlpeer_state = ML_PEER_DISCONN_INITIATED;
	ml_dev = ml_peer->ml_dev;

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		link_peers[i] = NULL;
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer)
			continue;

		link_peer = peer_entry->link_peer;
		if (wlan_objmgr_peer_try_get_ref(link_peer, WLAN_MLO_MGR_ID) !=
						QDF_STATUS_SUCCESS)
			continue;

		link_peers[i] = link_peer;
	}
	mlo_peer_lock_release(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		if (!link_peers[i])
			continue;

		/* Prepare and queue message */
		if (i == 0)
			mlo_link_peer_send_assoc_fail(ml_dev, link_peers[i]);
		else
			mlo_link_peer_disconnect_notify(ml_dev, link_peers[i]);
	}
}

void wlan_mlo_partner_peer_disconnect_notify(struct wlan_objmgr_peer *src_peer)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_peer *link_peers[MAX_MLO_LINK_PEERS];
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_vdev *vdev = NULL;
	uint16_t i;

	ml_peer = src_peer->mlo_peer_ctx;
	if (!ml_peer)
		return;

	vdev = wlan_peer_get_vdev(src_peer);
	if (!vdev)
		return;

	/* Do not change peer state to disconnect initiated for
	 * link switch case, this can lead to not sending deauth frame
	 * incase of actual disconnect and AP might drop the next connect
	 * request as it might think STA is still in connected state.
	 */
	if (wlan_vdev_mlme_is_mlo_link_switch_in_progress(vdev))
		return;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state == ML_PEER_DISCONN_INITIATED) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	wlan_mlo_peer_wsi_link_delete(ml_peer);

	ml_peer->mlpeer_state = ML_PEER_DISCONN_INITIATED;

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	ml_dev = ml_peer->ml_dev;
	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		link_peers[i] = NULL;
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer) {
			mlo_debug("link peer is null");
			continue;
		}

		if (peer_entry->link_peer == src_peer)
			continue;

		link_peer = peer_entry->link_peer;
		if (wlan_objmgr_peer_try_get_ref(link_peer, WLAN_MLO_MGR_ID) !=
						QDF_STATUS_SUCCESS)
			continue;

		link_peers[i] = link_peer;
	}
	mlo_peer_lock_release(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		if (!link_peers[i])
			continue;

		/* Prepare and queue message */
		mlo_link_peer_disconnect_notify(ml_dev, link_peers[i]);
	}
}

qdf_export_symbol(wlan_mlo_partner_peer_disconnect_notify);

static void mlo_peer_populate_link_peer(
			struct wlan_mlo_peer_context *ml_peer,
			struct wlan_objmgr_peer *link_peer)
{
	mlo_peer_lock_acquire(ml_peer);
	wlan_mlo_peer_get_ref(ml_peer);
	link_peer->mlo_peer_ctx = ml_peer;
	mlo_peer_lock_release(ml_peer);
}

static void mlo_reset_link_peer(
			struct wlan_mlo_peer_context *ml_peer,
			struct wlan_objmgr_peer *link_peer)
{
	mlo_peer_lock_acquire(ml_peer);
	link_peer->mlo_peer_ctx = NULL;
	wlan_peer_clear_mlo(link_peer);
	mlo_peer_lock_release(ml_peer);
}

static void mlo_peer_free(struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_dev_context *ml_dev;

	ml_dev = ml_peer->ml_dev;
	if (!ml_dev) {
		mlo_err("ML DEV is NULL");
		return;
	}

	mlo_debug("ML Peer " QDF_MAC_ADDR_FMT " is freed",
		  QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
	mlo_peer_lock_destroy(ml_peer);
	epcs_dev_peer_lock_destroy(&ml_peer->epcs_info);
	mlo_ap_ml_ptqm_peerid_free(ml_dev, ml_peer->mlo_peer_id);
	mlo_ap_ml_peerid_free(ml_peer->mlo_peer_id);
	mlo_peer_free_aid(ml_dev, ml_peer);
	mlo_peer_free_primary_umac(ml_dev, ml_peer);
	qdf_mem_free(ml_peer);
}

void mlo_peer_cleanup(struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_dev_context *ml_dev;

	if (!ml_peer) {
		mlo_err("ML PEER is NULL");
		return;
	}
	ml_dev = ml_peer->ml_dev;
	if (!ml_dev) {
		mlo_err("ML DEV is NULL");
		return;
	}

	mlo_dev_mlpeer_detach(ml_dev, ml_peer);
	/* If any Auth req is received during ML peer delete */
	mlo_peer_process_pending_auth(ml_dev, ml_peer);
	mlo_peer_free(ml_peer);
}

static QDF_STATUS mlo_peer_attach_link_peer(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_peer *link_peer,
		qdf_nbuf_t frm_buf)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	QDF_STATUS status = QDF_STATUS_E_RESOURCES;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	uint16_t i;

	if (!link_peer)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_peer_get_vdev(link_peer);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state != ML_PEER_CREATED) {
		mlo_peer_lock_release(ml_peer);
		mlo_err("ML Peer " QDF_MAC_ADDR_FMT " is not in created state (state %d)",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			ml_peer->mlpeer_state);
		return status;
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (peer_entry->link_peer)
			continue;

		if (wlan_objmgr_peer_try_get_ref(link_peer, WLAN_MLO_MGR_ID) !=
						QDF_STATUS_SUCCESS) {
			mlo_err("ML Peer " QDF_MAC_ADDR_FMT ", link peer " QDF_MAC_ADDR_FMT " is not in valid state",
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
				QDF_MAC_ADDR_REF
					(wlan_peer_get_macaddr(link_peer)));
			break;
		}
		peer_entry->link_peer = link_peer;
		qdf_atomic_inc(&vdev->vdev_objmgr.wlan_ml_peer_count);
		qdf_copy_macaddr(&peer_entry->link_addr,
				 (struct qdf_mac_addr *)&link_peer->macaddr[0]);

		peer_entry->link_ix = wlan_vdev_get_link_id(vdev);
		pdev = wlan_vdev_get_pdev(wlan_peer_get_vdev(link_peer));
		peer_entry->hw_link_id = wlan_mlo_get_pdev_hw_link_id(pdev);
		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		if (frm_buf)
			peer_entry->assoc_rsp_buf = frm_buf;
		else
			peer_entry->assoc_rsp_buf = NULL;

		status = QDF_STATUS_SUCCESS;
		break;
	}
	if (QDF_IS_STATUS_SUCCESS(status))
		ml_peer->link_peer_cnt++;

	mlo_peer_lock_release(ml_peer);

	return status;
}

qdf_nbuf_t mlo_peer_get_link_peer_assoc_resp_buf(
		struct wlan_mlo_peer_context *ml_peer,
		uint8_t link_ix)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	qdf_nbuf_t frm_buf = NULL;
	uint8_t i;

	if (!ml_peer)
		return NULL;

	if (link_ix > MAX_MLO_LINK_PEERS)
		return NULL;

	mlo_peer_lock_acquire(ml_peer);
	if ((ml_peer->mlpeer_state != ML_PEER_CREATED) &&
	    (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE)) {
		mlo_peer_lock_release(ml_peer);
		return NULL;
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];

		if (!peer_entry->link_peer)
			continue;

		if (peer_entry->link_ix == link_ix) {
			if (!peer_entry->assoc_rsp_buf)
				break;

			frm_buf = qdf_nbuf_clone(peer_entry->assoc_rsp_buf);
			break;
		}
	}
	mlo_peer_lock_release(ml_peer);

	return frm_buf;
}

void wlan_mlo_peer_free_all_link_assoc_resp_buf(
			struct wlan_objmgr_peer *link_peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_mlo_peer_context *ml_peer;
	uint8_t i;

	ml_peer = link_peer->mlo_peer_ctx;
	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];

		if (peer_entry->assoc_rsp_buf) {
			qdf_nbuf_free(peer_entry->assoc_rsp_buf);
			peer_entry->assoc_rsp_buf = NULL;
		}
	}
	mlo_peer_lock_release(ml_peer);
}

static QDF_STATUS mlo_peer_detach_link_peer(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_peer *link_peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	QDF_STATUS status = QDF_STATUS_E_RESOURCES;
	uint16_t i;
	struct wlan_objmgr_vdev *vdev;

	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer)
			continue;

		if (peer_entry->link_peer != link_peer)
			continue;

		if (peer_entry->assoc_rsp_buf) {
			qdf_nbuf_free(peer_entry->assoc_rsp_buf);
			peer_entry->assoc_rsp_buf = NULL;
		}
		vdev =  wlan_peer_get_vdev(link_peer);
		if (vdev) {
			qdf_atomic_dec(&vdev->vdev_objmgr.wlan_ml_peer_count);
		} else {
			mlo_err("vdev is null for ml_peer: " QDF_MAC_ADDR_FMT
				"mld mac addr: " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(link_peer->macaddr),
				QDF_MAC_ADDR_REF(link_peer->mldaddr));
			qdf_assert_always(vdev);
		}
		wlan_objmgr_peer_release_ref(link_peer, WLAN_MLO_MGR_ID);
		peer_entry->link_peer = NULL;
		ml_peer->link_peer_cnt--;
		status = QDF_STATUS_SUCCESS;
		break;
	}
	mlo_peer_lock_release(ml_peer);

	return status;
}
#if defined (SAP_MULTI_LINK_EMULATION)
/*Skip link vdev check. Second link does not have vdev*/
static QDF_STATUS mlo_dev_get_link_vdevs(
			struct wlan_objmgr_vdev *vdev,
			struct wlan_mlo_dev_context *ml_dev,
			struct mlo_partner_info *ml_info,
			struct wlan_objmgr_vdev *link_vdevs[])
{
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS mlo_dev_get_link_vdevs(
			struct wlan_objmgr_vdev *vdev,
			struct wlan_mlo_dev_context *ml_dev,
			struct mlo_partner_info *ml_info,
			struct wlan_objmgr_vdev *link_vdevs[])
{
	uint16_t i, j;
	struct wlan_objmgr_vdev *vdev_link;
	uint8_t link_id;

	if (!ml_dev) {
		mlo_err("ml_dev is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!ml_info) {
		mlo_err("ml_info is null");
		return QDF_STATUS_E_INVAL;
	}

	mlo_debug("num_partner_links %d", ml_info->num_partner_links);
	for (i = 0; i < ml_info->num_partner_links; i++) {
		link_id = ml_info->partner_link_info[i].link_id;
		vdev_link = mlo_get_vdev_by_link_id(vdev, link_id,
						    WLAN_MLO_MGR_ID);
		if (vdev_link) {
			link_vdevs[i] = vdev_link;
		} else {
			/* release ref which were taken before failure */
			for (j = 0; j < i; j++) {
				vdev_link = link_vdevs[j];
				if (!vdev_link)
					continue;

				wlan_objmgr_vdev_release_ref(vdev_link,
							     WLAN_MLO_MGR_ID);
			}
			return QDF_STATUS_E_INVAL;
		}
	}

	return QDF_STATUS_SUCCESS;
}
#endif

static void mlo_dev_release_link_vdevs(
			struct wlan_objmgr_vdev *link_vdevs[])
{
	uint16_t i;
	struct wlan_objmgr_vdev *vdev_link;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_link = link_vdevs[i];
		if (!vdev_link)
			continue;

		wlan_objmgr_vdev_release_ref(vdev_link, WLAN_MLO_MGR_ID);
	}
}

#ifdef WLAN_FEATURE_11BE
static void
wlan_mlo_peer_set_t2lm_enable_val(struct wlan_mlo_peer_context *ml_peer,
				  struct mlo_partner_info *ml_info)
{
	ml_peer->t2lm_policy.t2lm_enable_val = ml_info->t2lm_enable_val;
}

static void
wlan_mlo_peer_initialize_epcs_info(struct wlan_mlo_peer_context *ml_peer)
{
	ml_peer->epcs_info.state = EPCS_DOWN;
	ml_peer->epcs_info.self_gen_dialog_token = 0;
}

#else
static void
wlan_mlo_peer_set_t2lm_enable_val(struct wlan_mlo_peer_context *ml_peer,
				  struct mlo_partner_info *ml_info)
{}

static void
wlan_mlo_peer_initialize_epcs_info(struct wlan_mlo_peer_context *ml_peer)
{}
#endif /* WLAN_FEATURE_11BE */

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
struct wlan_objmgr_vdev*
mlo_get_link_vdev_from_psoc_id(struct wlan_mlo_dev_context *ml_dev,
			       uint8_t psoc_id, bool get_bridge_vdev)
{
	uint8_t i;
	uint16_t num_bridge_vdev = 0;
	uint8_t max_vdevs = WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS;
	struct wlan_objmgr_vdev *link_vdev;
	QDF_STATUS status;

	if (WLAN_OBJMGR_MAX_DEVICES <= psoc_id)
		return NULL;

	if (!ml_dev)
		return NULL;

	/* if there are no bridge vdevs available,
	 * fall back to actual link vdevs
	 */
	status = mlo_ap_get_bridge_vdev_count(ml_dev, &num_bridge_vdev);
	if (!get_bridge_vdev || (status != QDF_STATUS_SUCCESS) ||
	    !num_bridge_vdev)
		max_vdevs = WLAN_UMAC_MLO_MAX_VDEVS;

	for (i = 0; i < max_vdevs; i++) {
		if (max_vdevs == WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS)
			link_vdev = ml_dev->wlan_bridge_vdev_list[i];
		else
			link_vdev = ml_dev->wlan_vdev_list[i];
		if (!link_vdev)
			continue;
		if (psoc_id != wlan_vdev_get_psoc_id(link_vdev))
			continue;
		if (wlan_objmgr_vdev_try_get_ref(link_vdev, WLAN_MLO_MGR_ID) !=
							QDF_STATUS_SUCCESS) {
			mlo_err("VDEV is not in created state");
			return NULL;
		}
		return link_vdev;
	}

	return NULL;
}

static
QDF_STATUS mlo_bridge_peer_create_post(struct wlan_mlo_dev_context *ml_dev,
				       struct wlan_mlo_peer_context *ml_peer,
				       uint8_t psoc_id)
{
	struct peer_create_notif_s peer_create;
	struct wlan_objmgr_vdev *vdev_link;
	QDF_STATUS status;

	/* Bridge peer not required */
	if (psoc_id >= WLAN_OBJMGR_MAX_DEVICES)
		return QDF_STATUS_SUCCESS;

	vdev_link = mlo_get_link_vdev_from_psoc_id(ml_dev, psoc_id, true);

	if (!vdev_link) {
		mlo_err("VDEV derivation in unsuccessful for %u psoc",
			psoc_id);
		return QDF_STATUS_E_FAILURE;
	}

	peer_create.vdev_link = vdev_link;
	wlan_mlo_peer_get_ref(ml_peer);
	peer_create.ml_peer = ml_peer;

	qdf_copy_macaddr(&peer_create.addr,
			 &ml_peer->peer_mld_addr);

	peer_create.frm_buf = NULL;

	status = mlo_msgq_post(MLO_BRIDGE_PEER_CREATE, ml_dev, &peer_create);
	if (status != QDF_STATUS_SUCCESS) {
		wlan_mlo_peer_release_ref(ml_peer);
		wlan_objmgr_vdev_release_ref(vdev_link, WLAN_MLO_MGR_ID);
	}

	return status;
}

static QDF_STATUS
wlan_mlo_get_bridge_peer_psoc_id(struct wlan_objmgr_vdev *vdev,
				 struct wlan_mlo_peer_context *ml_peer,
				 struct wlan_objmgr_vdev *link_vdevs[],
				 uint8_t num_partner_links,
				 uint8_t *bridge_psoc_id)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_objmgr_vdev *ml_vdev;
	uint8_t psoc_ids[WLAN_NUM_TWO_LINK_PSOC];
	uint8_t comp_psoc_id, i, is_adjacent;
	uint8_t bridge_peer_psoc_id = WLAN_OBJMGR_MAX_DEVICES;
	uint16_t num_bridge_vdev = 0;
	uint8_t max_vdevs = WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS;
	QDF_STATUS status;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_SAP_MODE)
		return QDF_STATUS_SUCCESS;

	/* Return from here for 3 link association */
	if (WLAN_NUM_TWO_LINK_PSOC != num_partner_links)
		return QDF_STATUS_SUCCESS;

	ml_dev = vdev->mlo_dev_ctx;
	if (!ml_dev)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < WLAN_NUM_TWO_LINK_PSOC; i++)
		psoc_ids[i] = wlan_vdev_get_psoc_id(link_vdevs[i]);

	status = mlo_chip_adjacent(psoc_ids[0], psoc_ids[1],
				   &is_adjacent);
	if (QDF_STATUS_SUCCESS != status) {
		mlo_err("Unable to get chip adjacency for " QDF_MAC_ADDR_FMT ", psoc_0 %u, psoc_1 %u",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			psoc_ids[0], psoc_ids[1]);
		return status;
	}

	if (is_adjacent)
		return QDF_STATUS_SUCCESS;

	/* if there are no bridge vdevs available,
	 * fall back to actual link vdevs
	 */
	status = mlo_ap_get_bridge_vdev_count(ml_dev, &num_bridge_vdev);
	if ((status != QDF_STATUS_SUCCESS) || !num_bridge_vdev) {
		mlo_err_rl("Using actual vdev as bridge vdev");
		max_vdevs = WLAN_UMAC_MLO_MAX_VDEVS;
	}

	for (i = 0; i < max_vdevs; i++) {
		if (max_vdevs == WLAN_UMAC_MLO_MAX_BRIDGE_VDEVS)
			ml_vdev = ml_dev->wlan_bridge_vdev_list[i];
		else
			ml_vdev = ml_dev->wlan_vdev_list[i];
		if (!ml_vdev || (wlan_vdev_is_up(ml_vdev) != QDF_STATUS_SUCCESS))
			continue;
		comp_psoc_id = wlan_vdev_get_psoc_id(ml_vdev);
		if ((comp_psoc_id != psoc_ids[0]) &&
		    (comp_psoc_id != psoc_ids[1])) {
			bridge_peer_psoc_id = comp_psoc_id;
			break;
		}
	}

	if (bridge_peer_psoc_id >= WLAN_OBJMGR_MAX_DEVICES) {
		mlo_err("Invalid psoc or psoc not found for " QDF_MAC_ADDR_FMT " psoc %u",
			ether_sprintf(ml_peer->peer_mld_addr.bytes),
			bridge_peer_psoc_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* Check if derived psoc is adjecent to both links */
	for (i = 0; i < WLAN_NUM_TWO_LINK_PSOC; i++) {
		status = mlo_chip_adjacent(psoc_ids[i], bridge_peer_psoc_id,
					   &is_adjacent);
		if (QDF_STATUS_SUCCESS != status) {
			mlo_err("Unable to get chip adjacency for " QDF_MAC_ADDR_FMT ", psoc_0 %u, psoc_1 %u",
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
				psoc_ids[i], bridge_peer_psoc_id);
			return status;
		}
		if (!is_adjacent) {
			mlo_err("Derived psoc is not adjecent to one of the links for " QDF_MAC_ADDR_FMT ", psoc_0 %u, psoc_1 %u psoc_2 %u",
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
				psoc_ids[0], psoc_ids[1], bridge_peer_psoc_id);
			return QDF_STATUS_E_FAILURE;
		}
	}

	/* Add vdev corresponds to bridge link to link_vdevs so
	 * that primary UMAC derivation follows 3-link assoc.
	 */
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (link_vdevs[i])
			continue;

		link_vdevs[i] = mlo_get_link_vdev_from_psoc_id(ml_dev,
							       bridge_peer_psoc_id,
							       true);
		if (!link_vdevs[i])
			return QDF_STATUS_E_FAILURE;

		ml_peer->max_links++;
		*bridge_psoc_id = bridge_peer_psoc_id;
		break;
	}

	if (i == WLAN_UMAC_MLO_MAX_VDEVS) {
		mlo_err("Unable to append bridge peer vdev to link vdev list");
		return QDF_STATUS_E_FAILURE;
	}

	mlo_debug("%u is adjecent psoc for " QDF_MAC_ADDR_FMT " is_adjacent %u",
		  bridge_peer_psoc_id,
		  QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
		  is_adjacent);

	return status;
}
#else
static
QDF_STATUS mlo_bridge_peer_create_post(struct wlan_mlo_dev_context *ml_dev,
				       struct wlan_mlo_peer_context *ml_peer,
				       uint8_t psoc_id)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wlan_mlo_get_bridge_peer_psoc_id(struct wlan_objmgr_vdev *vdev,
				 struct wlan_mlo_peer_context *ml_peer,
				 struct wlan_objmgr_vdev *link_vdevs[],
				 uint8_t num_partner_links,
				 uint8_t *bridge_psoc_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlan_mlo_peer_asreq(struct wlan_objmgr_vdev *vdev,
			       struct wlan_objmgr_peer *link_peer,
			       struct mlo_partner_info *ml_info,
			       qdf_nbuf_t frm_buf)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_mlo_peer_context *ml_peer = NULL;
	struct wlan_objmgr_vdev *link_vdevs[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_objmgr_vdev *vdev_link;
	QDF_STATUS status;
	uint16_t i;
	uint16_t j;
	uint16_t link_count;
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *iter_peer;

	/* get ML VDEV from VDEV */
	ml_dev = vdev->mlo_dev_ctx;
	if (!ml_dev) {
		mlo_err("ML dev ctx is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ml_peer = link_peer->mlo_peer_ctx;
	if (!ml_peer) {
		mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " ML peer is NULL",
			ml_dev->mld_id,
			QDF_MAC_ADDR_REF(link_peer->mldaddr));

		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_mlo_peer_is_assoc_peer(ml_peer, link_peer)) {
		mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT "Assoc req received on non-assoc peer" QDF_MAC_ADDR_FMT,
			ml_dev->mld_id,
			QDF_MAC_ADDR_REF(link_peer->mldaddr),
			QDF_MAC_ADDR_REF(link_peer->macaddr));

		return QDF_STATUS_E_FAILURE;
	}

	status = mlo_dev_get_link_vdevs(vdev, ml_dev,
					ml_info, link_vdevs);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " get link vdevs failed",
			ml_dev->mld_id,
			QDF_MAC_ADDR_REF(link_peer->mldaddr));
		return QDF_STATUS_E_FAILURE;
	}

	/* If ML peer state is not in Assoc done state, drop REASSOC req */
	if (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE) {
		mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " invalid state %d",
			ml_dev->mld_id,
			QDF_MAC_ADDR_REF(link_peer->mldaddr),
			ml_peer->mlpeer_state);

		mlo_dev_release_link_vdevs(link_vdevs);

		return QDF_STATUS_E_FAILURE;
	}

	link_count = 0;
	for (j = 0; j < ml_peer->max_links; j++) {
		peer_entry = &ml_peer->peer_list[j];
		iter_peer = peer_entry->link_peer;
		if (!iter_peer)
			continue;

		if (wlan_peer_get_peer_type(iter_peer) ==
						WLAN_PEER_MLO_BRIDGE) {
			link_count++;
			continue;
		}

		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			vdev_link = link_vdevs[i];
			if (!vdev_link)
				continue;

			if (vdev_link != wlan_peer_get_vdev(iter_peer))
				continue;

			link_count++;
			break;
		}
	}
	if (link_count != ml_peer->link_peer_cnt) {
		mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " incorrect link peers",
			ml_dev->mld_id,
			QDF_MAC_ADDR_REF(link_peer->mldaddr));
		mlo_dev_release_link_vdevs(link_vdevs);

		return QDF_STATUS_E_INVAL;
	}

	/* Notify other vdevs about assoc req */
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_link = link_vdevs[i];
		if (!vdev_link)
			continue;

		if (vdev_link == vdev)
			continue;

		mlo_partner_peer_reassoc_post(ml_dev, vdev_link,
					      ml_peer, frm_buf, ml_info);
	}

	mlo_dev_release_link_vdevs(link_vdevs);

	if (QDF_IS_STATUS_ERROR(wlan_mlo_validate_reassocreq(ml_peer))) {
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " reassoc proc is failed %pK",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer);

		return QDF_STATUS_E_FAILURE;
	}

	mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " reassoc handled %pK",
		 ml_dev->mld_id,
		 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
		 ml_peer);

	return QDF_STATUS_SUCCESS;
}
#if defined (SAP_MULTI_LINK_EMULATION)
static void
set_assoc_peer_for_2link_sap(struct wlan_objmgr_peer *assoc_peer,
			     struct wlan_mlo_peer_context *ml_peer)
{
	assoc_peer = ml_peer->peer_list[0].link_peer;
	if (assoc_peer)
		mlo_mlme_peer_assoc_resp(assoc_peer);
}
#else
static void
set_assoc_peer_for_2link_sap(struct wlan_objmgr_peer *assoc_peer,
			     struct wlan_mlo_peer_context *ml_peer)
{
}
#endif
QDF_STATUS wlan_mlo_peer_create(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_peer *link_peer,
				struct mlo_partner_info *ml_info,
				qdf_nbuf_t frm_buf,
				uint16_t aid)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_mlo_peer_context *ml_peer = NULL;
	struct wlan_objmgr_vdev *link_vdevs[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_objmgr_vdev *tmp_link_vdevs[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_objmgr_vdev *vdev_link;
	QDF_STATUS status;
	uint16_t i, j;
	struct wlan_objmgr_peer *assoc_peer = NULL;
	uint8_t bridge_peer_psoc_id = WLAN_OBJMGR_MAX_DEVICES;
	bool is_ml_peer_attached = false;

	/* get ML VDEV from VDEV */
	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev) {
		mlo_err("ML dev ctx is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* Check resources of Partner VDEV */
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
		if (wlan_mlo_is_mld_ctx_exist(
		    (struct qdf_mac_addr *)&link_peer->mldaddr[0])) {
			mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " is matching with one of the MLD address in the system",
				ml_dev->mld_id,
				QDF_MAC_ADDR_REF(link_peer->mldaddr));
			return QDF_STATUS_E_FAILURE;
		}
		/* Limit max assoc links */
		if (ml_info->num_partner_links > WLAN_UMAC_MLO_ASSOC_MAX_SUPPORTED_LINKS) {
			mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " exceeds MAX assoc limit of %d",
				ml_dev->mld_id,
				QDF_MAC_ADDR_REF(link_peer->mldaddr),
				WLAN_UMAC_MLO_ASSOC_MAX_SUPPORTED_LINKS);
			return QDF_STATUS_E_RESOURCES;
		}

		status = mlo_dev_get_link_vdevs(vdev, ml_dev,
						ml_info, link_vdevs);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " get link vdevs failed",
				ml_dev->mld_id,
				QDF_MAC_ADDR_REF(link_peer->mldaddr));
			return QDF_STATUS_E_FAILURE;
		}
		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			vdev_link = link_vdevs[i];
			if (!vdev_link) {
				mlo_debug("vdev_link is null");
				continue;
			}

			if (wlan_vdev_is_mlo_peer_create_allowed(vdev_link)
					!= QDF_STATUS_SUCCESS) {
				mlo_dev_release_link_vdevs(link_vdevs);

				mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " create not allowed on link vdev %d",
					ml_dev->mld_id,
					QDF_MAC_ADDR_REF
						(link_peer->mldaddr),
					wlan_vdev_get_id(vdev_link));
				return QDF_STATUS_E_INVAL;
			}
		}

		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			vdev_link = link_vdevs[i];
			if (vdev_link && (vdev_link != vdev) &&
			    (wlan_vdev_get_peer_count(vdev_link) >
			     wlan_vdev_get_max_peer_count(vdev_link))) {
				mlo_dev_release_link_vdevs(link_vdevs);
				mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " Max peer count reached on link vdev %d",
					ml_dev->mld_id,
					QDF_MAC_ADDR_REF
						(link_peer->mldaddr),
					wlan_vdev_get_id(vdev_link));
				return QDF_STATUS_E_RESOURCES;
			}
		}
	}
	/* When roam to MLO AP, partner link vdev1 is updated first,
	 * ml peer need be created and attached for partner link peer.
	 *
	 * When roam target AP and current AP have same MLD address, don't
	 * delete old ML peer and re-create new one, just update different
	 * info.
	 */
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		ml_peer = wlan_mlo_get_mlpeer(ml_dev,
				 (struct qdf_mac_addr *)&link_peer->mldaddr[0]);
		if (ml_peer) {
			mlo_debug("ML Peer " QDF_MAC_ADDR_FMT
				" existed, state %d",
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
				ml_peer->mlpeer_state);
			ml_peer->mlpeer_state = ML_PEER_CREATED;
			ml_peer->max_links = ml_info->num_partner_links;
			wlan_mlo_peer_set_t2lm_enable_val(ml_peer, ml_info);
			is_ml_peer_attached = true;
		}
	}
	if (!ml_peer) {
		/* Allocate MLO peer */
		ml_peer = qdf_mem_malloc(sizeof(*ml_peer));
		if (!ml_peer) {
			mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " mem alloc failed",
				ml_dev->mld_id,
				QDF_MAC_ADDR_REF(link_peer->mldaddr));
			mlo_dev_release_link_vdevs(link_vdevs);
			return QDF_STATUS_E_NOMEM;
		}

		qdf_atomic_init(&ml_peer->ref_cnt);
		mlo_peer_lock_create(ml_peer);
		ml_peer->ml_dev = ml_dev;
		ml_peer->mlpeer_state = ML_PEER_CREATED;
		ml_peer->max_links = ml_info->num_partner_links;
		ml_peer->primary_umac_psoc_id = ML_PRIMARY_UMAC_ID_INVAL;
		ml_peer->migrate_primary_umac_psoc_id =
						ML_PRIMARY_UMAC_ID_INVAL;
		ml_peer->primary_umac_migration_in_progress = false;

		ml_peer->mlo_peer_id = mlo_ap_ml_peerid_alloc();
		if (ml_peer->mlo_peer_id == MLO_INVALID_PEER_ID) {
			mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " invalid ml peer id",
				ml_dev->mld_id,
				QDF_MAC_ADDR_REF
				(ml_peer->peer_mld_addr.bytes));
			mlo_peer_free(ml_peer);
			mlo_dev_release_link_vdevs(link_vdevs);
			return QDF_STATUS_E_RESOURCES;
		}

		qdf_copy_macaddr((struct qdf_mac_addr *)&ml_peer->peer_mld_addr,
				 (struct qdf_mac_addr *)&link_peer->mldaddr[0]);
		wlan_mlo_peer_set_t2lm_enable_val(ml_peer, ml_info);
		epcs_dev_peer_lock_create(&ml_peer->epcs_info);
		wlan_mlo_peer_initialize_epcs_info(ml_peer);

		/* Allocate AID */
		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
			if (aid == (uint16_t)-1) {
				status = mlo_peer_allocate_aid(ml_dev, ml_peer);
				if (status != QDF_STATUS_SUCCESS) {
					mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " aid alloc failed",
						ml_dev->mld_id,
						QDF_MAC_ADDR_REF
						(ml_peer->peer_mld_addr.bytes));
					mlo_peer_free(ml_peer);
					mlo_dev_release_link_vdevs(link_vdevs);
					return status;
				}
			} else {
				ml_peer->assoc_id = aid;
			}
		}
	}

	/* Populate Link peer pointer, peer MAC address,
	 * MLD address. HW link ID, update ref count
	 */
	status = mlo_peer_attach_link_peer(ml_peer, link_peer, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " link peer attach failed",
			ml_dev->mld_id,
			QDF_MAC_ADDR_REF
			(ml_peer->peer_mld_addr.bytes));
		/* If there is another link peer attached for this ML peer,
		 * ml peer can't be detached and freed.
		 */
		if (is_ml_peer_attached && ml_peer->link_peer_cnt)
			return status;
		if (is_ml_peer_attached)
			mlo_dev_mlpeer_detach(ml_dev, ml_peer);
		mlo_peer_free(ml_peer);
		mlo_dev_release_link_vdevs(link_vdevs);
		return status;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			if (!link_vdevs[i]) {
				mlo_debug("vdev_link is null");
				continue;
			}

			if (wlan_objmgr_vdev_try_get_ref(link_vdevs[i], WLAN_MLO_MGR_ID) !=
					QDF_STATUS_SUCCESS) {
				mlo_err("VDEV is not in created state");
				/* release ref which were taken before failure */
				for (j = 0; j < i; j++) {
					if (!link_vdevs[i])
						continue;

					wlan_objmgr_vdev_release_ref(link_vdevs[i],
								     WLAN_MLO_MGR_ID);
				}
				mlo_reset_link_peer(ml_peer, link_peer);
				mlo_peer_free(ml_peer);
				mlo_dev_release_link_vdevs(link_vdevs);

				return QDF_STATUS_E_FAILURE;
			}
			tmp_link_vdevs[i] = link_vdevs[i];
		}

		status = wlan_mlo_get_bridge_peer_psoc_id(vdev, ml_peer,
							  tmp_link_vdevs,
							  ml_info->num_partner_links,
							  &bridge_peer_psoc_id);
		if (QDF_STATUS_SUCCESS != status) {
			mlo_err("MLD ID %d: Failed to derive bridge peer psoc id",
				ml_dev->mld_id);
			mlo_reset_link_peer(ml_peer, link_peer);
			mlo_peer_free(ml_peer);
			mlo_dev_release_link_vdevs(link_vdevs);
			mlo_dev_release_link_vdevs(tmp_link_vdevs);
			wlan_objmgr_peer_release_ref(link_peer,
						     WLAN_MLO_MGR_ID);
			return QDF_STATUS_E_FAILURE;
		}
	}

	/* Allocate Primary UMAC */
	mlo_peer_allocate_primary_umac(ml_dev, ml_peer, tmp_link_vdevs);

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
		mlo_dev_release_link_vdevs(tmp_link_vdevs);

	/* Store AID, MLO Peer pointer in link peer, take link peer ref count */
	mlo_peer_populate_link_peer(ml_peer, link_peer);

	mlo_peer_populate_nawds_params(ml_peer, ml_info);
	mlo_peer_populate_mesh_params(ml_peer, ml_info);

	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) ||
		((wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) &&
			!is_ml_peer_attached)) {
		/* Reject creation for AP mode, If ML peer is present with
		 * MLD MAC address, For PSTA case, all MLD STAs are connected
		 * to same MLD AP, it can have duplicate MLD address entries
		 * for STA MLDs
		 */
		if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
		    mlo_mgr_ml_peer_exist_on_diff_ml_ctx(&link_peer->mldaddr[0],
							 NULL)) {
			mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " is exists, creation failed",
				ml_dev->mld_id,
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
			mlo_reset_link_peer(ml_peer, link_peer);
			mlo_peer_free(ml_peer);
			mlo_dev_release_link_vdevs(link_vdevs);
			wlan_objmgr_peer_release_ref(link_peer,
						     WLAN_MLO_MGR_ID);
			return QDF_STATUS_E_EXISTS;
		}

		/* Attach MLO peer to ML Peer table */
		status = mlo_dev_mlpeer_attach(ml_dev, ml_peer);
		if (status != QDF_STATUS_SUCCESS) {
			mlo_err("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " attach failed",
				ml_dev->mld_id,
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
			mlo_reset_link_peer(ml_peer, link_peer);
			wlan_objmgr_peer_release_ref(link_peer,
						     WLAN_MLO_MGR_ID);
			mlo_peer_free(ml_peer);
			mlo_dev_release_link_vdevs(link_vdevs);
			return status;
		}
	}

	wlan_mlo_peer_get_ref(ml_peer);

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
		/* Notify other vdevs about link peer creation */
		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			vdev_link = link_vdevs[i];
			if (!vdev_link)
				continue;

			if (vdev_link == vdev)
				continue;

			mlo_partner_peer_create_post(ml_dev, vdev_link,
						     ml_peer, frm_buf, ml_info);
		}
		/* Create bridge peer */
		status = mlo_bridge_peer_create_post(ml_dev, ml_peer,
						     bridge_peer_psoc_id);
		if (QDF_STATUS_SUCCESS != status) {
			mlo_err("MLD ID %d: Bridge peer creation failed",
				ml_dev->mld_id);
			wlan_mlo_partner_peer_create_failed_notify(ml_peer);
			mlo_dev_release_link_vdevs(link_vdevs);
			wlan_mlo_peer_release_ref(ml_peer);
			return QDF_STATUS_E_FAILURE;
		}
	}
	mlo_dev_release_link_vdevs(link_vdevs);

	if (ml_peer->mlpeer_state == ML_PEER_DISCONN_INITIATED) {
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " allocation failed",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		wlan_mlo_peer_release_ref(ml_peer);
		return QDF_STATUS_E_FAILURE;
	}

	mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " allocated %pK",
		 ml_dev->mld_id,
		 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
		 ml_peer);

	/*
	 * wlan_mlo_peer_create() is trigggered after getting peer
	 * assoc confirm from FW. For single link MLO connection, it is
	 * OK to trigger assoc response from here.
	 */
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
	    (!wlan_mlo_peer_is_nawds(ml_peer))) {
		if ((ml_peer->max_links == 1) &&
		    (ml_peer->link_peer_cnt == 1)) {
			assoc_peer = ml_peer->peer_list[0].link_peer;
			if (assoc_peer)
				mlo_mlme_peer_assoc_resp(assoc_peer);
		} else {
			set_assoc_peer_for_2link_sap(assoc_peer, ml_peer);
		}
	}


	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)
		wlan_clear_peer_level_tid_to_link_mapping(vdev);

	wlan_mlo_peer_release_ref(ml_peer);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_link_peer_attach(struct wlan_mlo_peer_context *ml_peer,
				     struct wlan_objmgr_peer *peer,
				     qdf_nbuf_t frm_buf)
{
	QDF_STATUS status;
	struct wlan_objmgr_peer *assoc_peer;
	struct wlan_objmgr_vdev *vdev = NULL;

	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	/* Populate Link peer pointer, peer MAC address,
	 * MLD address. HW link ID, update ref count
	 */
	status = mlo_peer_attach_link_peer(ml_peer, peer, frm_buf);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	/* Store AID, MLO Peer pointer in link peer, take link peer ref count */
	mlo_peer_populate_link_peer(ml_peer, peer);

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
		if (ml_peer->max_links == ml_peer->link_peer_cnt) {
			assoc_peer = ml_peer->peer_list[0].link_peer;
			if (assoc_peer)
				mlo_mlme_peer_assoc_resp(assoc_peer);
		}
	}

	return status;
}

QDF_STATUS wlan_mlo_link_asresp_attach(struct wlan_mlo_peer_context *ml_peer,
				       struct wlan_objmgr_peer *peer,
				       qdf_nbuf_t frm_buf)
{
	uint16_t i;
	struct wlan_mlo_link_peer_entry *peer_entry;

	if (!ml_peer) {
		mlo_err(" ml peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!peer) {
		ml_peer->link_asresp_cnt++;
		mlo_err(" ml peer or link peer pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer)
			continue;

		if (peer == peer_entry->link_peer) {
			ml_peer->link_asresp_cnt++;
			/* Free previous assoc resp buffer */
			if (peer_entry->assoc_rsp_buf) {
				qdf_nbuf_free(peer_entry->assoc_rsp_buf);
				peer_entry->assoc_rsp_buf = NULL;
			}

			if (frm_buf)
				peer_entry->assoc_rsp_buf = frm_buf;
			else
				peer_entry->assoc_rsp_buf = NULL;

			break;
		}
	}

	mlo_peer_lock_release(ml_peer);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_link_peer_delete(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer;

	ml_peer = peer->mlo_peer_ctx;

	if (!ml_peer)
		return QDF_STATUS_E_NOENT;

	if (ml_peer->mlpeer_state != ML_PEER_DISCONN_INITIATED)
		wlan_mlo_peer_wsi_link_delete(ml_peer);

	mlo_reset_link_peer(ml_peer, peer);
	mlo_peer_detach_link_peer(ml_peer, peer);

	if (ml_peer->mlpeer_state != ML_PEER_DISCONN_INITIATED)
		wlan_mlo_peer_wsi_link_add(ml_peer);

	wlan_mlo_peer_release_ref(ml_peer);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(wlan_mlo_link_peer_delete);

qdf_nbuf_t mlo_peer_get_link_peer_assoc_req_buf(
			struct wlan_mlo_peer_context *ml_peer,
			uint8_t link_ix)
{
	struct wlan_objmgr_peer *peer = NULL;
	qdf_nbuf_t assocbuf = NULL;

	if (!ml_peer)
		return NULL;

	peer = wlan_mlo_peer_get_assoc_peer(ml_peer);
	if (!peer)
		return NULL;

	assocbuf = mlo_mlme_get_link_assoc_req(peer, link_ix);

	return assocbuf;
}

void wlan_mlo_peer_get_links_info(struct wlan_objmgr_peer *peer,
				  struct mlo_tgt_partner_info *ml_links)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_vdev *link_vdev;
	uint8_t i, ix, idx = 0;
	struct wlan_mlo_eml_cap *ml_emlcap;

	ml_peer = peer->mlo_peer_ctx;
	ml_links->num_partner_links = 0;

	if (!ml_peer)
		return;

	ml_emlcap = &ml_peer->mlpeer_emlcap;

	mlo_peer_lock_acquire(ml_peer);

	if ((ml_peer->mlpeer_state != ML_PEER_CREATED) &&
	    (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE)) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		link_peer = peer_entry->link_peer;

		if (!link_peer)
			continue;
		idx++;
		if (link_peer == peer)
			continue;
		link_vdev = wlan_peer_get_vdev(link_peer);
		if (!link_vdev)
			continue;

		if (ml_links->num_partner_links >= WLAN_UMAC_MLO_MAX_VDEVS)
			break;

		ix = ml_links->num_partner_links;
		ml_links->link_info[ix].vdev_id = wlan_vdev_get_id(link_vdev);
		ml_links->link_info[ix].hw_mld_link_id = peer_entry->hw_link_id;
		ml_links->link_info[ix].mlo_enabled = 1;
		ml_links->link_info[ix].mlo_assoc_link =
			wlan_peer_mlme_is_assoc_peer(link_peer);
		ml_links->link_info[ix].mlo_primary_umac =
			peer_entry->is_primary;
		ml_links->link_info[ix].mlo_logical_link_index_valid = 1;
		ml_links->link_info[ix].emlsr_support = ml_emlcap->emlsr_supp;
		ml_links->link_info[ix].logical_link_index = idx - 1;
		ml_links->link_info[ix].mlo_bridge_peer = link_peer->mlo_bridge_peer;
		ml_links->num_partner_links++;
	}
	mlo_peer_lock_release(ml_peer);
}

qdf_export_symbol(wlan_mlo_peer_get_links_info);

uint8_t wlan_mlo_peer_get_primary_peer_link_id(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer;

	ml_peer = peer->mlo_peer_ctx;

	if (!ml_peer) {
		mlo_err("ml_peer is null");
		return WLAN_LINK_ID_INVALID;
	}

	return wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(ml_peer);
}

qdf_export_symbol(wlan_mlo_peer_get_primary_peer_link_id);

uint8_t wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(
				struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_vdev *link_vdev;
	uint8_t i, vdev_link_id;

	if (!ml_peer) {
		mlo_err("ml_peer is null");
		return WLAN_LINK_ID_INVALID;
	}
	mlo_peer_lock_acquire(ml_peer);

	if ((ml_peer->mlpeer_state != ML_PEER_CREATED) &&
	    (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE)) {
		mlo_peer_lock_release(ml_peer);
		mlo_err("ml_peer is not created and association is not done");
		return WLAN_LINK_ID_INVALID;
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		link_peer = peer_entry->link_peer;
		if (!link_peer)
			continue;

		if (peer_entry->is_primary) {
			link_vdev = wlan_peer_get_vdev(link_peer);
			if (!link_vdev) {
				mlo_peer_lock_release(ml_peer);
				mlo_err("link vdev not found");
				return WLAN_LINK_ID_INVALID;
			}
			vdev_link_id = wlan_vdev_get_link_id(link_vdev);
			mlo_peer_lock_release(ml_peer);
			return vdev_link_id;
		}
	}
	mlo_peer_lock_release(ml_peer);
	mlo_err("None of the peer is designated as primary");
	return WLAN_LINK_ID_INVALID;
}

qdf_export_symbol(wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer);

#ifdef WLAN_MLO_MULTI_CHIP
void wlan_mlo_peer_get_str_capability(struct wlan_objmgr_peer *peer,
				      uint8_t *str_capability)
{
	struct wlan_mlo_peer_context *ml_peer;
	uint8_t i;

	if (!str_capability)
		return;

	*str_capability = 1;
	if (!peer)
		return;
	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);
	if ((ml_peer->mlpeer_state != ML_PEER_CREATED) &&
	    (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE)) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (ml_peer->mlpeer_nstrinfo[i].link_id >= MAX_MLO_LINKS)
			continue;
		if (ml_peer->mlpeer_nstrinfo[i].nstr_lp_present) {
			*str_capability = 0;
			break;
		}
	}
	mlo_peer_lock_release(ml_peer);
}

void wlan_mlo_peer_get_eml_capability(struct wlan_objmgr_peer *peer,
				      uint8_t *is_emlsr_capable,
				      uint8_t *is_emlmr_capable)
{
	struct wlan_mlo_peer_context *ml_peer;

	if (!is_emlsr_capable || !is_emlmr_capable)
		return;

	*is_emlsr_capable = 0;
	*is_emlmr_capable = 0;
	if (!peer)
		return;
	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);
	if ((ml_peer->mlpeer_state != ML_PEER_CREATED) &&
	    (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE)) {
		mlo_peer_lock_release(ml_peer);
		return;
	}
	*is_emlsr_capable = ml_peer->mlpeer_emlcap.emlsr_supp;
	*is_emlmr_capable = ml_peer->mlpeer_emlcap.emlmr_supp;
	mlo_peer_lock_release(ml_peer);
}
#endif

void wlan_mlo_peer_get_partner_links_info(struct wlan_objmgr_peer *peer,
					  struct mlo_partner_info *ml_links)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_vdev *link_vdev;
	uint8_t i, ix;

	ml_peer = peer->mlo_peer_ctx;
	ml_links->num_partner_links = 0;

	if (!ml_peer)
		return;

	mlo_peer_lock_acquire(ml_peer);

	if ((ml_peer->mlpeer_state != ML_PEER_CREATED) &&
	    (ml_peer->mlpeer_state != ML_PEER_ASSOC_DONE)) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		link_peer = peer_entry->link_peer;

		if (!link_peer)
			continue;

		if (link_peer == peer)
			continue;

		link_vdev = wlan_peer_get_vdev(link_peer);
		if (!link_vdev)
			continue;

		if (ml_links->num_partner_links >= WLAN_UMAC_MLO_MAX_VDEVS)
			break;

		ix = ml_links->num_partner_links;
		ml_links->partner_link_info[ix].link_id = peer_entry->link_ix;
		ml_links->partner_link_info[ix].is_bridge =
		   (wlan_peer_get_peer_type(link_peer) == WLAN_PEER_MLO_BRIDGE);

		qdf_copy_macaddr(&ml_links->partner_link_info[ix].link_addr,
				 &peer_entry->link_addr);
		ml_links->num_partner_links++;
	}
	mlo_peer_lock_release(ml_peer);
}

qdf_export_symbol(wlan_mlo_peer_get_partner_links_info);

#ifdef UMAC_SUPPORT_MLNAWDS
bool wlan_mlo_peer_is_nawds(struct wlan_mlo_peer_context *ml_peer)
{
	bool status = false;

	if (!ml_peer)
		return status;

	mlo_peer_lock_acquire(ml_peer);
	if (ml_peer->is_nawds_ml_peer)
		status = true;
	mlo_peer_lock_release(ml_peer);

	return status;
}

qdf_export_symbol(wlan_mlo_peer_is_nawds);
#endif

#ifdef MESH_MODE_SUPPORT
bool wlan_mlo_peer_is_mesh(struct wlan_mlo_peer_context *ml_peer)
{
	bool status = false;

	if (!ml_peer)
		return status;

	mlo_peer_lock_acquire(ml_peer);
	if (ml_peer->is_mesh_ml_peer)
		status = true;
	mlo_peer_lock_release(ml_peer);

	return status;
}

qdf_export_symbol(wlan_mlo_peer_is_mesh);
#endif

#ifdef UMAC_MLO_AUTH_DEFER
void mlo_peer_free_auth_param(struct mlpeer_auth_params *auth_params)
{
	if (auth_params->rs)
		qdf_mem_free(auth_params->rs);

	if (auth_params->wbuf)
		qdf_nbuf_free(auth_params->wbuf);

	qdf_mem_free(auth_params);
}

QDF_STATUS mlo_peer_link_auth_defer(struct wlan_mlo_peer_context *ml_peer,
				    struct qdf_mac_addr *link_mac,
				    struct mlpeer_auth_params *auth_params)
{
	uint8_t i;
	uint8_t free_entries = 0;
	struct mlpeer_auth_params *recv_auth;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!ml_peer)
		return status;

	mlo_peer_lock_acquire(ml_peer);
	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		recv_auth = ml_peer->pending_auth[i];
		if (!recv_auth) {
			free_entries++;
			continue;
		}
		/* overwrite the entry with latest entry */
		if (qdf_is_macaddr_equal(link_mac, &recv_auth->link_addr)) {
			mlo_peer_free_auth_param(recv_auth);
			ml_peer->pending_auth[i] = auth_params;
			mlo_peer_lock_release(ml_peer);

			return QDF_STATUS_SUCCESS;
		}
	}

	if (!free_entries) {
		mlo_peer_lock_release(ml_peer);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		recv_auth = ml_peer->pending_auth[i];
		if (!recv_auth) {
			ml_peer->pending_auth[i] = auth_params;
			status = QDF_STATUS_SUCCESS;
			break;
		}
	}
	mlo_peer_lock_release(ml_peer);

	return status;
}

bool wlan_mlo_partner_peer_delete_is_allowed(struct wlan_objmgr_peer *src_peer)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_mlo_peer_context *ml_peer;

	vdev = wlan_peer_get_vdev(src_peer);
	if (!vdev)
		return false;

	ml_peer = src_peer->mlo_peer_ctx;
	if (!wlan_peer_is_mlo(src_peer) || !ml_peer)
		return false;

	if (wlan_vdev_mlme_op_flags_get(vdev, WLAN_VDEV_OP_MLO_STOP_LINK_DEL) ||
	    wlan_vdev_mlme_op_flags_get(vdev,
					WLAN_VDEV_OP_MLO_LINK_TBTT_COMPLETE)) {
		/* Single LINK MLO connection */
		if (ml_peer->link_peer_cnt == 1)
			return false;

		/*
		 * If this link is primary TQM and there is no ongoing migration
		 * from this link, then issue full disconnect.
		 */
		if ((wlan_mlo_peer_get_primary_peer_link_id(src_peer) !=
		    wlan_vdev_get_link_id(vdev)) ||
		    ml_peer->primary_umac_migration_in_progress)
			return false;
	}

	return true;
}

qdf_export_symbol(wlan_mlo_partner_peer_delete_is_allowed);
#endif

QDF_STATUS wlan_mlo_validate_reassocreq(struct wlan_mlo_peer_context *ml_peer)
{
	uint16_t i;
	struct wlan_mlo_link_peer_entry *peer_entry;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!ml_peer) {
		mlo_err(" ml peer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlo_peer_lock_acquire(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer)
			continue;

		/* Check non-assoc link peer Assoc resp buf is valid
		 * (exclude bridge peer)
		 */
		if (i && !peer_entry->assoc_rsp_buf &&
		    (wlan_peer_get_peer_type(peer_entry->link_peer) !=
						WLAN_PEER_MLO_BRIDGE)) {
			status = QDF_STATUS_E_FAILURE;
			break;
		}
	}

	mlo_peer_lock_release(ml_peer);

	return status;
}

#ifdef WLAN_WSI_STATS_SUPPORT
static uint32_t
wlan_mlo_psoc_get_mlo_grp_idx(struct mlo_mgr_context *mlo_mgr,
			      uint32_t psoc_id)
{
	struct mlo_wsi_info *wsi_info = NULL;
	struct mlo_wsi_psoc_grp *mlo_psoc_grp;
	uint8_t grp_id, i;

	if (!mlo_mgr)
		return MLO_WSI_MAX_MLO_GRPS;

	/* Retrieve the WSI link info structure */
	wsi_info = mlo_mgr->wsi_info;
	if (!wsi_info)
		return MLO_WSI_MAX_MLO_GRPS;

	for (grp_id = 0; grp_id < MLO_WSI_MAX_MLO_GRPS; grp_id++) {
		mlo_psoc_grp = &wsi_info->mlo_psoc_grp[grp_id];
		for (i = 0; i < mlo_psoc_grp->num_psoc; i++) {
			if (mlo_psoc_grp->psoc_order[i] == psoc_id)
				return grp_id;
		}
	}

	return MLO_WSI_MAX_MLO_GRPS;
}

QDF_STATUS wlan_mlo_wsi_link_info_send_cmd(void)
{
	struct mlo_mgr_context *mlo_mgr = NULL;
	struct mlo_wsi_info *wsi_info = NULL;
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops = NULL;
	struct wlan_objmgr_psoc *psoc_objmgr = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS error = QDF_STATUS_SUCCESS;
	int i, j;

	/* Retrieve the MLO manager context */
	mlo_mgr = wlan_objmgr_get_mlo_ctx();
	if (!mlo_mgr)
		return QDF_STATUS_E_INVAL;

	/* Retrieve the WSI link info structure */
	wsi_info = mlo_mgr->wsi_info;
	if (!wsi_info)
		return QDF_STATUS_E_INVAL;

	if (wsi_info->block_wmi_cmd) {
		mlo_debug("WMI command is blocked");
		return error;
	}
	/*
	 * Check each PSOC if the WMI command needs to be sent
	 * and send it for each PDEV in that PSOC
	 */
	for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
		if (!wsi_info->link_stats[i].send_wmi_cmd)
			continue;

		psoc_objmgr = wlan_objmgr_get_psoc_by_id(i, WLAN_MLO_MGR_ID);
		if (!psoc_objmgr) {
			mlo_err("Could not get PSOC obj for index %d", i);
			continue;
		}

		mlo_tx_ops = &psoc_objmgr->soc_cb.tx_ops->mlo_ops;
		if (!mlo_tx_ops || !mlo_tx_ops->send_wsi_link_info_cmd) {
			mlo_err("mlo_tx_ops is null");
			wlan_objmgr_psoc_release_ref(psoc_objmgr,
						     WLAN_MLO_MGR_ID);
			return QDF_STATUS_E_NULL_VALUE;
		}

		for (j = 0; j < psoc_objmgr->soc_objmgr.wlan_pdev_count; j++) {
			pdev = psoc_objmgr->soc_objmgr.wlan_pdev_list[j];
			if (!pdev)
				continue;

			if (mlo_tx_ops->send_wsi_link_info_cmd(
					pdev, &wsi_info->link_stats[i]) !=
						QDF_STATUS_SUCCESS) {
				mlo_err("Could not send WMI command for PDEV %d in PSOC %d",
					j, i);
				error = QDF_STATUS_E_FAILURE;
			} else {
				wsi_info->link_stats[i].send_wmi_cmd = false;
			}
		}
		wlan_objmgr_psoc_release_ref(psoc_objmgr, WLAN_MLO_MGR_ID);
	}

	return error;
}

static uint32_t wlan_mlo_psoc_get_ix_in_grp(struct mlo_mgr_context *mlo_mgr,
					    uint32_t grp_id,
					    uint32_t prim_psoc_id)
{
	uint32_t i;

	if (!mlo_mgr) {
		mlo_err("NULL mlo_mgr");
		return 0xFF;
	}

	/* Iterate through the PSOC order and find the ID */
	for (i = 0; i < mlo_mgr->wsi_info->mlo_psoc_grp[grp_id].num_psoc; i++) {
		if (mlo_mgr->wsi_info->mlo_psoc_grp[grp_id].psoc_order[i] ==
		    prim_psoc_id) {
			return i;
		}
	}

	return 0xFF;
}

static uint32_t
wlan_mlo_get_wsi_next_psoc(struct mlo_wsi_psoc_grp *mlo_psoc_grp,
			   uint32_t prim_psoc_id, uint32_t n)
{
	uint32_t i, j;
	uint32_t psoc_index = MLO_WSI_PSOC_ID_MAX;

	if (!n)
		return psoc_index;

	for (i = 0; i < mlo_psoc_grp->num_psoc; i++) {
		if (mlo_psoc_grp->psoc_order[i] == prim_psoc_id) {
			psoc_index = i;
			break;
		}
	}

	if (psoc_index == MLO_WSI_PSOC_ID_MAX)
		return psoc_index;

	for (i = 1; i <= WLAN_OBJMGR_MAX_DEVICES; i++) {
		j = (psoc_index + i) % WLAN_OBJMGR_MAX_DEVICES;
		if (mlo_psoc_grp->psoc_order[j] != MLO_WSI_PSOC_ID_MAX)
			n--;
		if (!n)
			return mlo_psoc_grp->psoc_order[j];
	}

	return MLO_WSI_PSOC_ID_MAX;
}

static QDF_STATUS
wlan_mlo_peer_wsi_link_update(struct wlan_mlo_peer_context *ml_peer, bool add)
{
	struct mlo_mgr_context *mlo_mgr = NULL;
	struct mlo_wsi_info *wsi_info = NULL;
	struct wlan_mlo_link_peer_entry *peer_entry = NULL;
	uint32_t prim_psoc_id = 0xFF;
	uint32_t sec_psoc_id[MAX_MLO_LINK_PEERS] = {0xFF};
	uint32_t hops, hop_id;
	uint32_t prim_grp_idx, sec_grp_idx;
	uint32_t prim_psoc_ix_grp, sec_psoc_ix_grp;
	uint32_t i, j, n;
	struct mlo_wsi_psoc_grp *mlo_psoc_grp;
	struct wlan_objmgr_psoc *psoc;

	/* Check if ml_peer is valid */
	if (!ml_peer) {
		mlo_err("ML peer is null");
		return QDF_STATUS_E_INVAL;
	}

	/* Retrieve the MLO manager context */
	mlo_mgr = wlan_objmgr_get_mlo_ctx();
	if (!mlo_mgr) {
		mlo_err("ML MGR is NULL for ML peer" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	/* Retrieve the WSI link info structure */
	wsi_info = mlo_mgr->wsi_info;
	if (!wsi_info) {
		mlo_err("WSI Info is NULL for ML peer" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	prim_psoc_id = ml_peer->primary_umac_psoc_id;
	/* Populate the primary PSOC ID of the respective group */
	prim_grp_idx = wlan_mlo_psoc_get_mlo_grp_idx(mlo_mgr, prim_psoc_id);
	if (prim_grp_idx == MLO_WSI_MAX_MLO_GRPS) {
		mlo_err("Group index is invalid for ML peer" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	/* Populate MLO primary and secondary link */
	for (i = 0, j = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry->link_peer) {
			mlo_debug("link peer is null");
			continue;
		}

		psoc = wlan_peer_get_psoc(peer_entry->link_peer);
		if (!mlo_get_wsi_stats_info_support(psoc)) {
			mlo_debug("WSI stats support is not enabled on psoc %d",
				  wlan_psoc_get_id(psoc));
			return QDF_STATUS_E_INVAL;
		}

		/*
		 * Consider all non-primary peers as secondary links.
		 * Additionally, consider all active peers as secondary link if
		 * the TQM is on another PSOC with bridge peer.
		 */
		if (!peer_entry->is_primary) {
			sec_psoc_id[j] = wlan_vdev_get_psoc_id(
						wlan_peer_get_vdev(
						peer_entry->link_peer));

			sec_grp_idx = wlan_mlo_psoc_get_mlo_grp_idx
							(mlo_mgr,
							 sec_psoc_id[j]);
			if (sec_grp_idx != prim_grp_idx) {
				mlo_err("Secondary link is not part of same MLO group as primary link");
				continue;
			}
			j++;
		}
	}

	/*
	 * Logic for finding ingress and egress stats:
	 * For a given MLO group, there is a PSOC order
	 *
	 * If there is secondary link, Increment the egress count for
	 * the primary PSOC
	 * (1) Iterate through each secondary link
	 *      (1.1) Set the WMI command to true for that PSOC
	 *      (1.2) Calculate the number of hops from the secondary link
	 *            to that primary link within the group.
	 *      (1.3) Iterate through a decrement sequence from the hop_count
	 *            (1.3.1) Increment the ingress count for that PSOC index
	 */
	prim_psoc_ix_grp = wlan_mlo_psoc_get_ix_in_grp(mlo_mgr, prim_grp_idx,
						       prim_psoc_id);

	wsi_info->link_stats[prim_psoc_id].send_wmi_cmd = true;
	mlo_psoc_grp = &wsi_info->mlo_psoc_grp[prim_grp_idx];

	if (j) {
		wsi_info->link_stats[prim_psoc_id].send_wmi_cmd = true;
		if (add)
			wsi_info->link_stats[prim_psoc_id].egress_cnt++;
		else
			wsi_info->link_stats[prim_psoc_id].egress_cnt--;
	}

	n = 1;
	for (i = 0; i < j; i++) {
		sec_psoc_ix_grp = wlan_mlo_psoc_get_ix_in_grp(mlo_mgr,
							      prim_grp_idx,
							      sec_psoc_id[i]);
		hops = qdf_min(mlo_psoc_grp->num_psoc -
			       (prim_psoc_ix_grp - sec_psoc_ix_grp),
				(sec_psoc_ix_grp - prim_psoc_ix_grp));


		if (hops > n) {
			hop_id = wlan_mlo_get_wsi_next_psoc(mlo_psoc_grp,
							    prim_psoc_id, n);
			n++;
			if (hop_id == MLO_WSI_PSOC_ID_MAX)
				continue;

			wsi_info->link_stats[hop_id].send_wmi_cmd = true;
			if (add)
				wsi_info->link_stats[hop_id].ingress_cnt++;
			else
				wsi_info->link_stats[hop_id].ingress_cnt--;
		}
	}

	/* Check if WMI bit is set, if yes, then send the command */
	if (wlan_mlo_wsi_link_info_send_cmd() != QDF_STATUS_SUCCESS) {
		mlo_err("WSI link info not sent for ML peer" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_peer_wsi_link_add(struct wlan_mlo_peer_context *ml_peer)
{
	if (!ml_peer) {
		mlo_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	return wlan_mlo_peer_wsi_link_update(ml_peer, 1);
}

QDF_STATUS wlan_mlo_peer_wsi_link_delete(struct wlan_mlo_peer_context *ml_peer)
{
	if (!ml_peer) {
		mlo_err("Invalid peer");
		return QDF_STATUS_E_INVAL;
	}

	return wlan_mlo_peer_wsi_link_update(ml_peer, 0);
}

void wlan_mlo_wsi_stats_block_cmd(void)
{
	struct mlo_mgr_context *mlo_mgr = NULL;
	struct mlo_wsi_info *wsi_info = NULL;

	/* Retrieve the MLO manager context */
	mlo_mgr = wlan_objmgr_get_mlo_ctx();
	if (!mlo_mgr)
		return;

	/* Retrieve the WSI link info structure */
	wsi_info = mlo_mgr->wsi_info;
	if (!wsi_info)
		return;

	wsi_info->block_wmi_cmd++;
}

void wlan_mlo_wsi_stats_allow_cmd(void)
{
	struct mlo_mgr_context *mlo_mgr = NULL;
	struct mlo_wsi_info *wsi_info = NULL;

	/* Retrieve the MLO manager context */
	mlo_mgr = wlan_objmgr_get_mlo_ctx();
	if (!mlo_mgr)
		return;

	/* Retrieve the WSI link info structure */
	wsi_info = mlo_mgr->wsi_info;
	if (!wsi_info)
		return;

	if (wsi_info->block_wmi_cmd)
		wsi_info->block_wmi_cmd--;
}

#else
void wlan_mlo_wsi_stats_block_cmd(void)
{
}

void wlan_mlo_wsi_stats_allow_cmd(void)
{
}

QDF_STATUS wlan_mlo_wsi_link_info_send_cmd(void)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_peer_wsi_link_add(struct wlan_mlo_peer_context *ml_peer)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlo_peer_wsi_link_delete(struct wlan_mlo_peer_context *ml_peer)
{
	return QDF_STATUS_SUCCESS;
}
#endif

void wlan_mlo_ap_vdev_add_assoc_entry(struct wlan_objmgr_vdev *vdev,
				      struct qdf_mac_addr *mld_addr)
{
	struct wlan_mlo_dev_context *mld_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta_assoc_pending_list *assoc_list;
	struct wlan_mlo_sta_entry *sta_entry = NULL;

	if (!mld_ctx || !wlan_vdev_mlme_is_mlo_ap(vdev))
		return;

	assoc_list = &mld_ctx->ap_ctx->assoc_list;

	sta_entry = wlan_mlo_ap_vdev_find_assoc_entry(vdev, mld_addr);
	if (sta_entry) {
		mlo_err("Duplicate entry " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mld_addr->bytes));
		return;
	}

	sta_entry = qdf_mem_malloc(sizeof(*sta_entry));
	if (!sta_entry)
		return;

	qdf_copy_macaddr((struct qdf_mac_addr *)&sta_entry->peer_mld_addr,
			 (struct qdf_mac_addr *)&mld_addr[0]);

	qdf_spin_lock_bh(&assoc_list->list_lock);
	qdf_list_insert_back(&assoc_list->peer_list, &sta_entry->mac_node);
	qdf_spin_unlock_bh(&assoc_list->list_lock);
}

void wlan_mlo_ap_vdev_del_assoc_entry(struct wlan_objmgr_vdev *vdev,
				      struct qdf_mac_addr *mld_addr)
{
	struct wlan_mlo_dev_context *mld_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta_assoc_pending_list *assoc_list;
	struct wlan_mlo_sta_entry *sta_entry = NULL;

	if (!mld_ctx || !wlan_vdev_mlme_is_mlo_ap(vdev))
		return;

	assoc_list = &mld_ctx->ap_ctx->assoc_list;
	sta_entry = wlan_mlo_ap_vdev_find_assoc_entry(vdev, mld_addr);
	if (!sta_entry)
		return;

	qdf_spin_lock_bh(&assoc_list->list_lock);
	qdf_list_remove_node(&assoc_list->peer_list, &sta_entry->mac_node);
	qdf_spin_unlock_bh(&assoc_list->list_lock);

	qdf_mem_free(sta_entry);
}

static inline struct wlan_mlo_sta_entry *
wlan_mlo_assoc_list_peek_head(qdf_list_t *assoc_list)
{
	struct wlan_mlo_sta_entry *sta_entry = NULL;
	qdf_list_node_t *list_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (qdf_list_peek_front(assoc_list, &list_node) != QDF_STATUS_SUCCESS)
		return NULL;

	sta_entry = qdf_container_of(list_node,
				     struct wlan_mlo_sta_entry, mac_node);
	return sta_entry;
}

static inline struct wlan_mlo_sta_entry *
wlan_mlo_sta_get_next_sta_entry(qdf_list_t *assoc_list,
				struct wlan_mlo_sta_entry *sta_entry)
{
	struct wlan_mlo_sta_entry *next_sta_entry = NULL;
	qdf_list_node_t *node = &sta_entry->mac_node;
	qdf_list_node_t *next_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (!node)
		return NULL;

	if (qdf_list_peek_next(assoc_list, node, &next_node) !=
				QDF_STATUS_SUCCESS)
		return NULL;

	next_sta_entry = qdf_container_of(next_node,
					  struct wlan_mlo_sta_entry, mac_node);
	return next_sta_entry;
}

struct wlan_mlo_sta_entry *
wlan_mlo_ap_vdev_find_assoc_entry(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr *mld_addr)
{
	struct wlan_mlo_dev_context *mld_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta_assoc_pending_list *assoc_list;
	struct wlan_mlo_sta_entry *sta_entry = NULL;
	struct wlan_mlo_sta_entry *next_sta_entry = NULL;

	if (!mld_ctx || !wlan_vdev_mlme_is_mlo_ap(vdev))
		return NULL;

	assoc_list = &mld_ctx->ap_ctx->assoc_list;
	if (qdf_list_empty(&assoc_list->peer_list)) {
		mlo_info("list is empty");
		return NULL;
	}
	qdf_spin_lock_bh(&assoc_list->list_lock);

	sta_entry = wlan_mlo_assoc_list_peek_head(&assoc_list->peer_list);
	while (sta_entry) {
		if (qdf_is_macaddr_equal(&sta_entry->peer_mld_addr, mld_addr)) {
			qdf_spin_unlock_bh(&assoc_list->list_lock);
			return sta_entry;
		}

		next_sta_entry =
		wlan_mlo_sta_get_next_sta_entry(&assoc_list->peer_list,
						sta_entry);
		sta_entry = next_sta_entry;
	}
	qdf_spin_unlock_bh(&assoc_list->list_lock);

	return NULL;
}

