/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

bool mlo_peer_is_assoc_peer(struct wlan_mlo_peer_context *ml_peer,
			    struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	bool is_assoc_peer = false;

	if (!ml_peer || !peer)
		return is_assoc_peer;

	peer_entry = &ml_peer->peer_list[0];

	if (peer_entry->link_peer != peer)
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
	mlo_peer_lock_release(ml_peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		if (!link_peers[i])
			continue;

		/* Prepare and queue message */
		mlo_link_peer_assoc_notify(ml_dev, link_peers[i]);
	}
}

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

	return;
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

	mlo_peer_lock_acquire(ml_peer);

	if (ml_peer->mlpeer_state == ML_PEER_DISCONN_INITIATED) {
		mlo_peer_lock_release(ml_peer);
		return;
	}

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

		wlan_objmgr_peer_release_ref(link_peer, WLAN_MLO_MGR_ID);
		peer_entry->link_peer = NULL;
		ml_peer->link_peer_cnt--;
		status = QDF_STATUS_SUCCESS;
		break;
	}
	mlo_peer_lock_release(ml_peer);

	return status;
}

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
		vdev_link = mlo_get_vdev_by_link_id(vdev, link_id);
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
#else
static void
wlan_mlo_peer_set_t2lm_enable_val(struct wlan_mlo_peer_context *ml_peer,
				  struct mlo_partner_info *ml_info)
{}
#endif /* WLAN_FEATURE_11BE */

QDF_STATUS wlan_mlo_peer_create(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_peer *link_peer,
				struct mlo_partner_info *ml_info,
				qdf_nbuf_t frm_buf,
				uint16_t aid)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct wlan_mlo_peer_context *ml_peer = NULL;
	struct wlan_objmgr_vdev *link_vdevs[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_objmgr_vdev *vdev_link;
	QDF_STATUS status;
	uint16_t i;
	struct wlan_objmgr_peer *assoc_peer;
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

	/* Allocate Primary UMAC */
	mlo_peer_allocate_primary_umac(ml_dev, ml_peer, link_vdevs);

	/* Store AID, MLO Peer pointer in link peer, take link peer ref count */
	mlo_peer_populate_link_peer(ml_peer, link_peer);

	mlo_peer_populate_nawds_params(ml_peer, ml_info);
	mlo_peer_populate_mesh_params(ml_peer, ml_info);

	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) ||
		((wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) &&
			!is_ml_peer_attached)) {
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
		if (ml_peer->max_links == ml_peer->link_peer_cnt) {
			assoc_peer = ml_peer->peer_list[0].link_peer;
			if (assoc_peer)
				mlo_mlme_peer_assoc_resp(assoc_peer);
		}
	}

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

QDF_STATUS wlan_mlo_link_peer_delete(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer;

	ml_peer = peer->mlo_peer_ctx;

	if (!ml_peer)
		return QDF_STATUS_E_NOENT;

	mlo_reset_link_peer(ml_peer, peer);
	mlo_peer_detach_link_peer(ml_peer, peer);
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
		ml_links->link_info[ix].vdev_id = wlan_vdev_get_id(link_vdev);
		ml_links->link_info[ix].hw_mld_link_id = peer_entry->hw_link_id;
		ml_links->num_partner_links++;
	}
	mlo_peer_lock_release(ml_peer);
}

qdf_export_symbol(wlan_mlo_peer_get_links_info);

uint8_t wlan_mlo_peer_get_primary_peer_link_id(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *link_peer;
	struct wlan_objmgr_vdev *link_vdev;
	uint8_t i, vdev_link_id;

	ml_peer = peer->mlo_peer_ctx;

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

qdf_export_symbol(wlan_mlo_peer_get_primary_peer_link_id);

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
		 * If this link is primary TQM, then delete MLO connection till
		 * primary umac migration is implemented
		 */
		if (wlan_mlo_peer_get_primary_peer_link_id(src_peer) !=
			wlan_vdev_get_link_id(vdev))
			return false;
	}

	return true;
}
#endif
