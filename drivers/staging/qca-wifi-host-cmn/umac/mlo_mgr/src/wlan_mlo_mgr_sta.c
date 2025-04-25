/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: contains MLO manager STA related api's
 */

#include <wlan_cmn.h>
#include <wlan_mlo_mgr_sta.h>
#include <wlan_cm_public_struct.h>
#include <wlan_mlo_mgr_main.h>
#include <wlan_cm_api.h>
#include <wlan_mlo_mgr_cmn.h>
#include <wlan_scan_api.h>
#include <scheduler_api.h>
#include <wlan_crypto_global_api.h>
#include <utils_mlo.h>
#include <wlan_mlme_cmn.h>
#include <wlan_scan_utils_api.h>
#include <qdf_time.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_scan_api.h>
#include <wlan_mlo_mgr_peer.h>
#include <qdf_module.h>
#include <wlan_mlo_mgr_public_api.h>

#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS mlo_disconnect_req(struct wlan_objmgr_vdev *vdev,
				     enum wlan_cm_source source,
				     enum wlan_reason_code reason_code,
				     struct qdf_mac_addr *bssid,
				     bool validate_req);

void
mlo_allocate_and_copy_ies(struct wlan_cm_connect_req *target,
			  struct wlan_cm_connect_req *source)
{
	target->assoc_ie.ptr = NULL;
	target->scan_ie.ptr = NULL;
	target->crypto.wep_keys.key = NULL;
	target->crypto.wep_keys.seq = NULL;
	target->crypto.wep_keys.key_len = 0;
	target->crypto.wep_keys.seq_len = 0;

	if (source->scan_ie.ptr) {
		target->scan_ie.ptr = qdf_mem_malloc(source->scan_ie.len);
		if (!target->scan_ie.ptr)
			target->scan_ie.len = 0;
		else
			qdf_mem_copy(target->scan_ie.ptr,
				     source->scan_ie.ptr, source->scan_ie.len);
	}

	if (source->assoc_ie.ptr) {
		target->assoc_ie.ptr = qdf_mem_malloc(source->assoc_ie.len);
		if (!target->assoc_ie.ptr)
			target->assoc_ie.len = 0;
		else
			qdf_mem_copy(target->assoc_ie.ptr, source->assoc_ie.ptr,
				     source->assoc_ie.len);
	}
}

/*
 * mlo_get_tdls_link_vdev() - API to get tdls link vdev
 * @mlo_dev_ctx: pointer to mlo dev context
 *
 * Return: MLD tdls link vdev
 */
static inline struct wlan_objmgr_vdev *
mlo_get_tdls_link_vdev(struct wlan_mlo_dev_context *mlo_dev_ctx)
{
	uint8_t i = 0;

	if (!mlo_dev_ctx)
		return NULL;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (wlan_vdev_mlme_is_tdls_vdev(mlo_dev_ctx->wlan_vdev_list[i]))
			return mlo_dev_ctx->wlan_vdev_list[i];
	}
	return NULL;
}

struct wlan_objmgr_vdev *
wlan_mlo_get_tdls_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;

	if (!mlo_dev_ctx || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return NULL;

	return mlo_get_tdls_link_vdev(mlo_dev_ctx);
}

/*
 * mlo_get_assoc_link_vdev - API to get assoc link vdev
 *
 * @mlo_dev_ctx: pointer to mlo dev context
 *
 * Return: MLD assoc link vdev
 */
static inline struct wlan_objmgr_vdev *
mlo_get_assoc_link_vdev(struct wlan_mlo_dev_context *mlo_dev_ctx)
{
	uint8_t i = 0;

	if (!mlo_dev_ctx)
		return NULL;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (wlan_vdev_mlme_is_mlo_vdev(mlo_dev_ctx->wlan_vdev_list[i]) &&
		    !wlan_vdev_mlme_is_mlo_link_vdev(mlo_dev_ctx->wlan_vdev_list[i]))
			return mlo_dev_ctx->wlan_vdev_list[i];
	}
	return NULL;
}

struct wlan_objmgr_vdev *
wlan_mlo_get_assoc_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;

	if (!mlo_dev_ctx || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return NULL;

	return mlo_get_assoc_link_vdev(mlo_dev_ctx);
}

struct wlan_objmgr_vdev *
ucfg_mlo_get_assoc_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	return wlan_mlo_get_assoc_link_vdev(vdev);
}

/**
 * mlo_is_mld_disconnected - Check whether MLD is disconnected
 *
 * @vdev: pointer to vdev
 *
 * Return: true if mld is disconnected, false otherwise
 */
static inline
bool mlo_is_mld_disconnected(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	uint8_t i = 0;

	if (!mlo_dev_ctx || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return true;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (!wlan_cm_is_vdev_disconnected(mlo_dev_ctx->wlan_vdev_list[i]))
			return false;
	}
	return true;
}

bool mlo_is_mld_disconnecting_connecting(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	uint8_t i = 0;

	if (!mlo_dev_ctx || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return false;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;
		if (wlan_cm_is_vdev_disconnecting(mlo_dev_ctx->wlan_vdev_list[i]) ||
		    wlan_cm_is_vdev_connecting(mlo_dev_ctx->wlan_vdev_list[i]))
			return true;
	}
	return false;
}

bool mlo_is_ml_connection_in_progress(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	uint8_t i = 0;
	bool val = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLO_MGR_ID);

	if (!vdev) {
		mlo_err("Invalid vdev");
		return false;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		goto end;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;
		if (qdf_test_bit(i, mlo_dev_ctx->sta_ctx->wlan_connected_links)) {
			if (!wlan_cm_is_vdev_connected(mlo_dev_ctx->wlan_vdev_list[i])) {
				val = true;
				goto end;
			}
		}
	}

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return val;
}

bool ucfg_mlo_is_mld_disconnected(struct wlan_objmgr_vdev *vdev)
{
	return mlo_is_mld_disconnected(vdev);
}

/**
 * mlo_send_link_disconnect- Issue the disconnect request on MLD links
 *
 * @vdev: pointer to vdev
 * @source: disconnect source
 * @reason_code: disconnect reason
 * @bssid: bssid of AP to disconnect, can be null if not known
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
mlo_send_link_disconnect(struct wlan_objmgr_vdev *vdev,
			 enum wlan_cm_source source,
			 enum wlan_reason_code reason_code,
			 struct qdf_mac_addr *bssid)
{
	uint8_t i = 0;
	enum wlan_cm_source link_source = source;
	struct wlan_objmgr_vdev *assoc_vdev =
			mlo_get_assoc_link_vdev(vdev->mlo_dev_ctx);
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS];
	uint16_t vdev_count = 0;
	struct wlan_mlo_sta *sta_ctx = NULL;

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!sta_ctx) {
		mlo_err("Invalid sta_ctx");
		return QDF_STATUS_E_FAILURE;
	}

	if (!assoc_vdev && mlo_mgr_is_link_switch_supported(vdev)) {
		if (!wlan_mlo_mgr_is_link_switch_on_assoc_vdev(vdev))
			return QDF_STATUS_E_FAILURE;

		assoc_vdev = wlan_mlo_mgr_link_switch_get_assoc_vdev(vdev);
		mlo_release_vdev_ref(assoc_vdev);
	}

	/*
	 * Change the source for the link vdev to make sure it's handled as a
	 * Northbound disconnect in VDEV/PEER state machine.
	 */
	if (source != CM_OSIF_DISCONNECT)
		link_source = CM_MLO_LINK_VDEV_DISCONNECT;

	mlo_sta_get_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	for (i =  0; i < vdev_count; i++) {
		if ((wlan_vdev_list[i] != assoc_vdev) &&
		    (qdf_test_bit(i, sta_ctx->wlan_connected_links) ||
		    (wlan_cm_is_vdev_connected(wlan_vdev_list[i]) &&
		    !wlan_peer_is_mlo(wlan_vdev_get_bsspeer(wlan_vdev_list[i]))) ||
		    wlan_cm_is_vdev_idle_due_to_link_switch(wlan_vdev_list[i])))
			wlan_cm_disconnect(wlan_vdev_list[i],
					   link_source, reason_code,
					   NULL);
		mlo_release_vdev_ref(wlan_vdev_list[i]);
	}

	if (assoc_vdev)
		wlan_cm_disconnect(assoc_vdev, source, reason_code, NULL);

	return QDF_STATUS_SUCCESS;
}

static void mlo_free_copied_conn_req(struct wlan_mlo_sta *sta_ctx)
{
	if (sta_ctx) {
		mlo_debug("enter");
		copied_conn_req_lock_acquire(sta_ctx);
		if (sta_ctx->copied_conn_req) {
			wlan_cm_free_connect_req(sta_ctx->copied_conn_req);
			sta_ctx->copied_conn_req = NULL;
		}
		copied_conn_req_lock_release(sta_ctx);
	}
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
int mlo_mgr_get_per_link_chan_info(struct wlan_objmgr_vdev *vdev, int link_id,
				   struct wlan_channel *chan_info)
{
	struct mlo_link_info *ml_link_info;

	ml_link_info = mlo_mgr_get_ap_link_by_link_id(vdev->mlo_dev_ctx,
						      link_id);
	if (!ml_link_info) {
		mlo_debug("ml_link_info null for link_id: %d", link_id);
		return -EINVAL;
	}

	qdf_mem_copy(chan_info, ml_link_info->link_chan_info,
		     sizeof(*chan_info));

	return 0;
}

static QDF_STATUS
mlo_validate_connect_req(struct wlan_objmgr_vdev *vdev,
			 struct wlan_mlo_dev_context *mlo_dev_ctx,
			 struct wlan_cm_connect_req *req)
{
/* check back to back connect handling */
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
mlo_validate_disconn_req(struct wlan_objmgr_vdev *vdev,
			 enum wlan_cm_source source,
			 enum wlan_reason_code reason_code,
			 struct qdf_mac_addr *bssid)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlo_validate_mlo_cap(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void mlo_mld_clear_mlo_cap(struct wlan_objmgr_vdev *vdev)
{ }
#else
/**
 * mlo_is_mld_connected - Check whether MLD is connected
 *
 * @vdev: pointer to vdev
 *
 * Return: true if mld is connected, false otherwise
 */
static inline
bool mlo_is_mld_connected(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	uint8_t i = 0;

	if (!mlo_dev_ctx || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return true;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (qdf_test_bit(i, mlo_dev_ctx->sta_ctx->wlan_connected_links)) {
			if (!wlan_cm_is_vdev_connected(mlo_dev_ctx->wlan_vdev_list[i]))
				return false;
		}
	}
	return true;
}

bool ucfg_mlo_is_mld_connected(struct wlan_objmgr_vdev *vdev)
{
	return mlo_is_mld_connected(vdev);
}

static inline
void mlo_mld_clear_mlo_cap(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	uint8_t i = 0;

	if (!mlo_dev_ctx || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;
		wlan_vdev_mlme_clear_mlo_vdev(mlo_dev_ctx->wlan_vdev_list[i]);
		wlan_vdev_mlme_clear_mlo_link_vdev(mlo_dev_ctx->wlan_vdev_list[i]);
	}
	mlo_clear_bridge_sta_ctx(vdev);
}

void ucfg_mlo_mld_clear_mlo_cap(struct wlan_objmgr_vdev *vdev)
{
	mlo_mld_clear_mlo_cap(vdev);
}

static void
mlo_cm_handle_connect_in_disconnection_state(struct wlan_objmgr_vdev *vdev,
					     struct wlan_cm_connect_req *req)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;

	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is NULL");
		return;
	}

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx->connect_req)
		sta_ctx->connect_req = qdf_mem_malloc(
					sizeof(struct wlan_cm_connect_req));

	if (sta_ctx->connect_req) {
		qdf_mem_copy(sta_ctx->connect_req, req,
			     sizeof(struct wlan_cm_connect_req));
		mlo_allocate_and_copy_ies(sta_ctx->connect_req, req);
	} else {
		mlo_err("Failed to allocate connect req");
	}
}

static QDF_STATUS
mlo_validate_disconn_req(struct wlan_objmgr_vdev *vdev,
			 enum wlan_cm_source source,
			 enum wlan_reason_code reason_code,
			 struct qdf_mac_addr *bssid)
{
	struct wlan_mlo_dev_context *mlo_dev = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = mlo_dev->sta_ctx;
	uint8_t i = 0;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev->wlan_vdev_list[i])
			continue;

		if (wlan_cm_is_vdev_connecting(mlo_dev->wlan_vdev_list[i])) {
			if (!wlan_vdev_mlme_is_mlo_link_vdev(
						mlo_dev->wlan_vdev_list[i]))
				return QDF_STATUS_SUCCESS;

			if (!sta_ctx->disconn_req)
				sta_ctx->disconn_req =
					qdf_mem_malloc(
					sizeof(struct wlan_cm_disconnect_req));

			if (!sta_ctx->disconn_req)
				return QDF_STATUS_SUCCESS;

			sta_ctx->disconn_req->vdev_id =
						wlan_vdev_get_id(vdev);
			sta_ctx->disconn_req->source = source;
			sta_ctx->disconn_req->reason_code = reason_code;
			if (bssid)
				qdf_copy_macaddr(&sta_ctx->disconn_req->bssid,
						 bssid);
			return QDF_STATUS_E_BUSY;
		} else if (wlan_cm_is_vdev_connected(mlo_dev->wlan_vdev_list[i]) &&
			   !wlan_vdev_mlme_is_mlo_link_vdev(
				mlo_dev->wlan_vdev_list[i]) &&
			   wlan_peer_is_mlo(wlan_vdev_get_bsspeer(
						mlo_dev->wlan_vdev_list[i]))) {
				/* If the vdev is moved to connected state but
				 * MLO mgr is not yet notified, defer disconnect
				 * as it can cause race between connect complete
				 * and disconnect initiation
				 */
			if (!qdf_test_bit(i, sta_ctx->wlan_connected_links)) {
				if (!sta_ctx->disconn_req)
					sta_ctx->disconn_req =
						qdf_mem_malloc(
						sizeof(struct wlan_cm_disconnect_req));

				if (!sta_ctx->disconn_req)
					return QDF_STATUS_SUCCESS;

				sta_ctx->disconn_req->vdev_id =
					wlan_vdev_get_id(vdev);
				sta_ctx->disconn_req->source = source;
				sta_ctx->disconn_req->reason_code = reason_code;
				if (bssid)
					qdf_copy_macaddr(&sta_ctx->disconn_req->bssid,
							 bssid);

				return QDF_STATUS_E_BUSY;
			}
		}
	}
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlo_disconnect_no_lock(struct wlan_objmgr_vdev *vdev,
					 enum wlan_cm_source source,
					 enum wlan_reason_code reason_code,
					 struct qdf_mac_addr *bssid)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *assoc_vdev = NULL;
	uint8_t i = 0;

	if (mlo_dev_ctx)
		sta_ctx = mlo_dev_ctx->sta_ctx;
	if (sta_ctx) {
		mlo_free_copied_conn_req(sta_ctx);
	} else {
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		sta_ctx = mlo_dev_ctx->sta_ctx;
		if (!sta_ctx)
			return QDF_STATUS_E_FAILURE;

		assoc_vdev = mlo_get_assoc_link_vdev(mlo_dev_ctx);

		if (sta_ctx->connect_req) {
			wlan_cm_free_connect_req(sta_ctx->connect_req);
			sta_ctx->connect_req = NULL;
		}

		status = mlo_validate_disconn_req(vdev, source,
						  reason_code, bssid);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("Connect in progress, deferring disconnect");
			return status;
		}

		for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			if (!mlo_dev_ctx->wlan_vdev_list[i])
				continue;

			if ((mlo_dev_ctx->wlan_vdev_list[i] != assoc_vdev) &&
			    (qdf_test_bit(i, mlo_dev_ctx->sta_ctx->wlan_connected_links) ||
			    (wlan_cm_is_vdev_connected(mlo_dev_ctx->wlan_vdev_list[i]) &&
			     !wlan_peer_is_mlo(wlan_vdev_get_bsspeer(mlo_dev_ctx->wlan_vdev_list[i])))))
				wlan_cm_disconnect(mlo_dev_ctx->wlan_vdev_list[i],
						   source, reason_code,
						   NULL);
		}

		if (assoc_vdev)
			wlan_cm_disconnect(assoc_vdev, source,
					   reason_code, NULL);
	}

	return status;
}

static void
mlo_cm_handle_connect_in_connection_state(struct wlan_objmgr_vdev *vdev,
					  struct wlan_cm_connect_req *req)
{
	mlo_disconnect_no_lock(vdev, CM_INTERNAL_DISCONNECT,
			       REASON_UNSPEC_FAILURE, NULL);
	mlo_cm_handle_connect_in_disconnection_state(vdev, req);
}

static QDF_STATUS
mlo_validate_connect_req(struct wlan_objmgr_vdev *vdev,
			 struct wlan_mlo_dev_context *mlo_dev_ctx,
			 struct wlan_cm_connect_req *req)
{
	uint8_t i = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev))
		return QDF_STATUS_SUCCESS;

	// Handle connect in various states
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if ((wlan_cm_is_vdev_connected(mlo_dev_ctx->wlan_vdev_list[i])) ||
		    (wlan_cm_is_vdev_connecting(mlo_dev_ctx->wlan_vdev_list[i])) ||
		    (wlan_cm_is_vdev_roaming(mlo_dev_ctx->wlan_vdev_list[i]))) {
			mlo_cm_handle_connect_in_connection_state(mlo_dev_ctx->wlan_vdev_list[i], req);
			return QDF_STATUS_E_BUSY;
		} else if (wlan_cm_is_vdev_disconnecting(mlo_dev_ctx->wlan_vdev_list[i])) {
			mlo_cm_handle_connect_in_disconnection_state(mlo_dev_ctx->wlan_vdev_list[i], req);
			return QDF_STATUS_E_BUSY;
		}

		/*
		 * mlo_connect: update wlan_connect_req_links in
		 * wlan_cfg80211_conect on osif_cm_connect,
		 * Validate pre checks for connection
		 */
		if (qdf_test_bit(i, mlo_dev_ctx->sta_ctx->wlan_connect_req_links)) {
			status = mlo_mlme_validate_conn_req(
					mlo_dev_ctx->wlan_vdev_list[i], NULL);
			if (status != QDF_STATUS_SUCCESS)
				return status;
			/*
			 * clone security params in all partner sta vaps
			 */
			mlo_mlme_clone_sta_security(
				mlo_dev_ctx->wlan_vdev_list[i], req);
		}
	}
	return status;
}

static QDF_STATUS mlo_validate_mlo_cap(struct wlan_objmgr_vdev *vdev)
{
	if (wlan_vdev_mlme_is_mlo_vdev(vdev))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS mlo_set_cu_bpcc(struct wlan_objmgr_vdev *vdev,
			   uint8_t vdev_id, uint8_t bpcc)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct mlo_sta_cu_params *cu_param;
	uint8_t i;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	cu_param = &mlo_dev_ctx->sta_ctx->mlo_cu_param[0];
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (cu_param[i].initialized && cu_param[i].vdev_id == vdev_id) {
			cu_param[i].bpcc = bpcc;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_INVAL;
}

QDF_STATUS mlo_get_cu_bpcc(struct wlan_objmgr_vdev *vdev,
			   uint8_t vdev_id, uint8_t *bpcc)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct mlo_sta_cu_params *cu_param;
	uint8_t i;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	cu_param = &mlo_dev_ctx->sta_ctx->mlo_cu_param[0];
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (cu_param[i].initialized &&
		    cu_param[i].vdev_id == vdev_id) {
			*bpcc = cu_param[i].bpcc;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_INVAL;
}

void mlo_init_cu_bpcc(struct wlan_mlo_dev_context *mlo_dev_ctx,
		      uint8_t vdev_id)
{
	uint8_t i;
	struct mlo_sta_cu_params *cu_param;
	uint8_t empty_slot = 0xff;

	cu_param = &mlo_dev_ctx->sta_ctx->mlo_cu_param[0];

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (cu_param[i].initialized &&
		    cu_param[i].vdev_id == vdev_id) {
			cu_param[i].bpcc = 0;
			return;
		}

		if (!cu_param[i].initialized && empty_slot == 0xff)
			empty_slot = i;
	}

	if (empty_slot != 0xff) {
		cu_param[empty_slot].bpcc = 0;
		cu_param[empty_slot].vdev_id = vdev_id;
		cu_param[empty_slot].initialized = true;
		mlo_debug("init cu bpcc idx %d, vdev_id %d",
			  empty_slot, vdev_id);
	} else {
		mlo_debug("No bpcc idx for vdev_id %d", vdev_id);
	}
}

void mlo_clear_cu_bpcc(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;
	uint32_t size;

	if (!vdev)
		return;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	size = sizeof(sta_ctx->mlo_cu_param);
	qdf_mem_zero(sta_ctx->mlo_cu_param, size);
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static void mlo_clear_sta_key_mgmt(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!vdev)
		return;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	qdf_mem_zero(sta_ctx->key_mgmt, sizeof(sta_ctx->key_mgmt));

	mlo_debug("clear sta_key_mgmt");
}

#else
static void mlo_clear_sta_key_mgmt(struct wlan_objmgr_vdev *vdev)
{
}
#endif

QDF_STATUS mlo_connect(struct wlan_objmgr_vdev *vdev,
		       struct wlan_cm_connect_req *req)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (mlo_dev_ctx)
		sta_ctx = mlo_dev_ctx->sta_ctx;
	if (sta_ctx) {
		status = mlo_validate_mlo_cap(vdev);
		if (QDF_IS_STATUS_ERROR(status))
			return wlan_cm_start_connect(vdev, req);

		mlo_dev_lock_acquire(mlo_dev_ctx);
		status = mlo_validate_connect_req(vdev, mlo_dev_ctx, req);
		copied_conn_req_lock_acquire(sta_ctx);
		if (!sta_ctx->copied_conn_req)
			sta_ctx->copied_conn_req = qdf_mem_malloc(
					sizeof(struct wlan_cm_connect_req));
		else
			wlan_cm_free_connect_req_param(sta_ctx->copied_conn_req);

		if (sta_ctx->copied_conn_req) {
			qdf_mem_copy(sta_ctx->copied_conn_req, req,
				     sizeof(struct wlan_cm_connect_req));
			mlo_allocate_and_copy_ies(sta_ctx->copied_conn_req,
						  req);
			copied_conn_req_lock_release(sta_ctx);
		} else {
			mlo_err("Failed to allocate orig connect req");
			copied_conn_req_lock_release(sta_ctx);
			mlo_dev_lock_release(mlo_dev_ctx);

			return QDF_STATUS_E_NOMEM;
		}

		if (QDF_IS_STATUS_SUCCESS(status)) {
			mlo_clear_cu_bpcc(vdev);
			mlo_clear_connected_links_bmap(vdev);
			mlo_clear_sta_key_mgmt(vdev);
			mlo_dev_lock_release(mlo_dev_ctx);

			status = wlan_cm_start_connect(vdev, req);
			if (QDF_IS_STATUS_ERROR(status))
				mlo_mld_clear_mlo_cap(vdev);
			return status;
		}

		mlo_dev_lock_release(mlo_dev_ctx);

		return status;
	}

	return wlan_cm_start_connect(vdev, req);
}

static inline void
mlo_update_connect_req_chan_info(struct wlan_cm_connect_req *req)
{
	req->chan_freq = 0;
	req->chan_freq_hint = 0;
}

/**
 * mlo_prepare_and_send_connect- Prepare and send the connect req
 *
 * @vdev: vdev pointer
 * @ml_parnter_info: ml partner link info
 * @link_info: link info on which connect req will be sent
 * @ssid: ssid to connect
 * @mld_addr: MLD address for connect request.
 *
 * Return: none
 */

static void
mlo_prepare_and_send_connect(struct wlan_objmgr_vdev *vdev,
			     struct mlo_partner_info ml_parnter_info,
			     struct mlo_link_info link_info,
			     struct wlan_ssid ssid,
			     struct qdf_mac_addr *mld_addr)
{
	struct wlan_cm_connect_req req = {0};
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;

	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is NULL");
		return;
	}

	sta_ctx = mlo_dev_ctx->sta_ctx;

	mlo_debug("Partner link connect mac:" QDF_MAC_ADDR_FMT
		  " bssid:" QDF_MAC_ADDR_FMT " vdev_id:%d",
		  QDF_MAC_ADDR_REF(wlan_vdev_mlme_get_macaddr(vdev)),
		  QDF_MAC_ADDR_REF(link_info.link_addr.bytes),
		  wlan_vdev_get_id(vdev));

	if (sta_ctx->copied_conn_req)
		qdf_mem_copy(&req, sta_ctx->copied_conn_req,
			     sizeof(struct wlan_cm_connect_req));
	else
		mlo_err("Invalid copied_conn_req");

	mlo_update_connect_req_chan_info(&req);

	qdf_mem_copy(req.bssid.bytes,
		     link_info.link_addr.bytes,
		     QDF_MAC_ADDR_SIZE);

	qdf_mem_copy(&req.ml_parnter_info,
		     &ml_parnter_info,
		     sizeof(struct mlo_partner_info));

	req.vdev_id = wlan_vdev_get_id(vdev);
	req.ssid.length = ssid.length;
	qdf_mem_copy(&req.ssid.ssid, &ssid.ssid, ssid.length);
	if (mld_addr)
		qdf_copy_macaddr(&req.mld_addr, mld_addr);

	if (sta_ctx->copied_conn_req)
		mlo_allocate_and_copy_ies(&req, sta_ctx->copied_conn_req);

	if (!req.assoc_ie.ptr)
		mlo_err("Failed to allocate assoc IEs");

	if (!req.scan_ie.ptr)
		mlo_err("Failed to allocate scan IEs");

	/* Reset crypto auth type for partner link.
	 * It will be set based on partner scan cache entry
	 */
	req.crypto.auth_type = 0;

	wlan_cm_start_connect(vdev, &req);
	wlan_cm_free_connect_req_param(&req);
}

/**
 * mlo_send_link_connect- Create/Issue the connection on secondary link
 *
 * @vdev: vdev pointer
 * @resp: Connection resp of assoc VDEV
 *
 * Return: none
 */
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static void mlo_send_link_connect(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_connect_resp *resp)
{
	/* Create the secondary interface, Send keys if the last link */
	uint8_t i, partner_idx = 0;
	struct wlan_ssid ssid = {0};
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS];
	uint16_t vdev_count = 0;
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct mlo_partner_info *ml_parnter_info = &resp->ml_parnter_info;

	mlo_debug("Sending link connect on partner interface");
	wlan_vdev_mlme_get_ssid(
			vdev, ssid.ssid,
			&ssid.length);

	if (!ml_parnter_info->num_partner_links) {
		mlo_err("No partner info in connect resp");
		return;
	}

	if(wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		return;

	mlo_sta_get_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	for (i = 0; i < vdev_count; i++) {
		if (wlan_vdev_list[i] == vdev) {
			mlo_release_vdev_ref(wlan_vdev_list[i]);
			continue;
		}
		wlan_vdev_mlme_set_mlo_vdev(mlo_dev_ctx->wlan_vdev_list[i]);
		wlan_vdev_mlme_set_mlo_link_vdev(mlo_dev_ctx->wlan_vdev_list[i]);
		wlan_vdev_set_link_id(
		      wlan_vdev_list[i],
		      ml_parnter_info->partner_link_info[partner_idx].link_id);
		ml_parnter_info->partner_link_info[partner_idx].vdev_id =
			       wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]);
		wlan_crypto_free_vdev_key(wlan_vdev_list[i]);
		mlo_prepare_and_send_connect(
				wlan_vdev_list[i],
				*ml_parnter_info,
				ml_parnter_info->partner_link_info[partner_idx],
				ssid, &resp->mld_addr);
		mlo_update_connected_links(wlan_vdev_list[i], 1);
		partner_idx++;
		mlo_release_vdev_ref(wlan_vdev_list[i]);
	}
}
#else
static void mlo_send_link_connect(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_connect_resp *resp)
{
	struct wlan_ssid ssid = {0};
	uint8_t i = 0;
	uint8_t j = 0;
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct mlo_partner_info *ml_parnter_info = &resp->ml_parnter_info;

	if (!ml_parnter_info->num_partner_links) {
		mlo_err("No partner info in connect resp");
		return;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	if (!wlan_cm_is_vdev_connected(vdev)) {
		mlo_dev_lock_release(mlo_dev_ctx);
		return;
	}

	for (j = 0; j < ml_parnter_info->num_partner_links; j++) {
		for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			if (!mlo_dev_ctx->wlan_vdev_list[i])
				continue;
			/*
			* mlo_connect: update wlan_connected_links bitmap from
			* assoc resp parsing
			*/
			if (qdf_test_bit(i, mlo_dev_ctx->sta_ctx->wlan_connected_links)) {
				if (wlan_cm_is_vdev_disconnected(
					mlo_dev_ctx->wlan_vdev_list[i])) {
					if (mlo_dev_ctx->wlan_vdev_list[i]->vdev_mlme.mlo_link_id
						== ml_parnter_info->partner_link_info[j].link_id) {
						wlan_vdev_mlme_get_ssid(
							vdev, ssid.ssid,
							&ssid.length);
						mlo_prepare_and_send_connect(
							mlo_dev_ctx->wlan_vdev_list[i],
							*ml_parnter_info,
							ml_parnter_info->partner_link_info[j],
							ssid, NULL);
						mlo_dev_lock_release(mlo_dev_ctx);
						return;
					}
				}
			}
		}
	}
	mlo_dev_lock_release(mlo_dev_ctx);
}
#endif

void
mlo_update_connected_links_bmap(struct wlan_mlo_dev_context *mlo_dev_ctx,
				struct mlo_partner_info ml_parnter_info)
{
	uint8_t i = 0;
	uint8_t j = 0;

	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is NULL");
		return;
	}

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		for (j = 0; j < ml_parnter_info.num_partner_links; j++) {
			if (wlan_vdev_get_link_id(mlo_dev_ctx->wlan_vdev_list[i]) ==
			    ml_parnter_info.partner_link_info[j].link_id)
				mlo_update_connected_links(
					mlo_dev_ctx->wlan_vdev_list[i], 1);
		}
	}
}

void mlo_clear_connected_links_bmap(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!vdev)
		return;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	qdf_mem_zero(sta_ctx->wlan_connected_links,
		     sizeof(sta_ctx->wlan_connected_links));
}

static QDF_STATUS ml_activate_disconnect_req_sched_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;

	if (!vdev) {
		mlme_err("Null input vdev");
		return QDF_STATUS_E_INVAL;
	}

	mlo_disconnect(vdev, CM_OSIF_DISCONNECT,
		       REASON_UNSPEC_FAILURE, NULL);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ml_activate_disconnect_req_flush_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;

	if (!vdev) {
		mlme_err("Null input vdev");
		return QDF_STATUS_E_INVAL;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ml_activate_pend_disconn_req_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!vdev) {
		mlme_err("Null input vdev");
		return QDF_STATUS_E_INVAL;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx || !sta_ctx->disconn_req)
		goto release_ref;

	mlo_disconnect_req(vdev, sta_ctx->disconn_req->source,
			   sta_ctx->disconn_req->reason_code,
			   &sta_ctx->disconn_req->bssid, false);

	qdf_mem_free(sta_ctx->disconn_req);
	sta_ctx->disconn_req = NULL;

release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ml_activate_pend_disconn_req_flush_cb(
					struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;

	if (!vdev) {
		mlme_err("Null input vdev");
		return QDF_STATUS_E_INVAL;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static inline
QDF_STATUS mlo_post_disconnect_msg(struct scheduler_msg *msg)
{
	return scheduler_post_message(
			QDF_MODULE_ID_OS_IF,
			QDF_MODULE_ID_SCAN,
			QDF_MODULE_ID_OS_IF,
			msg);
}
#else
static inline
QDF_STATUS mlo_post_disconnect_msg(struct scheduler_msg *msg)
{
	return scheduler_post_message(
			QDF_MODULE_ID_MLME,
			QDF_MODULE_ID_MLME,
			QDF_MODULE_ID_MLME,
			msg);
}
#endif

void mlo_handle_sta_link_connect_failure(struct wlan_objmgr_vdev *vdev,
					 struct wlan_cm_connect_resp *rsp)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct scheduler_msg msg = {0};
	QDF_STATUS ret;
	struct wlan_objmgr_vdev *assoc_vdev;

	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is NULL");
		return;
	}

	assoc_vdev = mlo_get_assoc_link_vdev(mlo_dev_ctx);
	if (!assoc_vdev) {
		mlo_err("Assoc Vdev is NULL");
		return;
	}

	if (vdev != assoc_vdev) {
		mlo_update_connected_links(vdev, 0);
		if (rsp->reason == CM_NO_CANDIDATE_FOUND ||
		    rsp->reason == CM_HW_MODE_FAILURE ||
		    rsp->reason == CM_SER_FAILURE) {
			ret = wlan_objmgr_vdev_try_get_ref(
					assoc_vdev, WLAN_MLO_MGR_ID);
			if (QDF_IS_STATUS_ERROR(ret)) {
				mlo_err("Failed to get ref vdev_id %d",
					wlan_vdev_get_id(assoc_vdev));
				return;
			}
			/* Since these failures happen in same context. use
			 * scheduler to avoid deadlock by deferring context
			 */
			msg.bodyptr = assoc_vdev;
			msg.callback = ml_activate_disconnect_req_sched_cb;
			msg.flush_callback =
				ml_activate_disconnect_req_flush_cb;
			ret = mlo_post_disconnect_msg(&msg);
			if (QDF_IS_STATUS_ERROR(ret)) {
				wlan_objmgr_vdev_release_ref(
						assoc_vdev,
						WLAN_MLO_MGR_ID);
				return;
			}
		} else {
			mlo_disconnect(vdev, CM_OSIF_DISCONNECT,
				       REASON_UNSPEC_FAILURE, NULL);
		}
	}
}

void mlo_handle_pending_disconnect(struct wlan_objmgr_vdev *vdev)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS ret;

	ret = wlan_objmgr_vdev_try_get_ref(
			vdev, WLAN_MLO_MGR_ID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		mlo_err("Failed to get ref vdev_id %d",
			wlan_vdev_get_id(vdev));
		return;
	}

	msg.bodyptr = vdev;
	msg.callback = ml_activate_pend_disconn_req_cb;
	msg.flush_callback =
		ml_activate_pend_disconn_req_flush_cb;
	ret = mlo_post_disconnect_msg(&msg);
	if (QDF_IS_STATUS_ERROR(ret)) {
		mlo_err("Failed to post scheduler msg");
		wlan_objmgr_vdev_release_ref(
				vdev,
				WLAN_MLO_MGR_ID);
		QDF_BUG(0);
		return;
	}
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static bool
mlo_sta_ignore_link_connect_fail(struct wlan_objmgr_vdev *vdev)
{
	if (wlan_cm_is_vdev_disconnected(vdev)) {
		if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
			return false;
		else
			return true;
	}

	return false;
}
#else
static inline bool
mlo_sta_ignore_link_connect_fail(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

void mlo_sta_link_connect_notify(struct wlan_objmgr_vdev *vdev,
				 struct wlan_cm_connect_resp *rsp)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (mlo_dev_ctx) {
		sta_ctx = mlo_dev_ctx->sta_ctx;
	} else {
		mlo_debug_rl("mlo_dev_ctx is NULL");
		return;
	}

	if (sta_ctx && sta_ctx->disconn_req) {
		mlo_debug("Handle pending disocnnect for vdev %d",
			  wlan_vdev_get_id(vdev));
		mlo_handle_pending_disconnect(vdev);
		return;
	}

	if (wlan_cm_is_link_switch_connect_resp(rsp)) {
		mlo_info("Skip for link switch connect request");
		return;
	}

	if (mlo_sta_ignore_link_connect_fail(vdev))
		return;

	if (wlan_cm_is_vdev_disconnected(vdev))
		mlo_free_copied_conn_req(sta_ctx);

	if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		mlo_debug("Vdev: %d", wlan_vdev_get_id(vdev));
		if (wlan_cm_is_vdev_disconnected(vdev)) {
			mlo_handle_sta_link_connect_failure(vdev, rsp);
			return;
		} else if (!wlan_cm_is_vdev_connected(vdev)) {
			/* If vdev is not in disconnected or connected state,
			 * then the event is received due to connect req being
			 * flushed. Hence, ignore this event
			 */
			return;
		}

		if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev) && sta_ctx) {
			if (sta_ctx->assoc_rsp.ptr) {
				qdf_mem_free(sta_ctx->assoc_rsp.ptr);
				sta_ctx->assoc_rsp.ptr = NULL;
			}
			sta_ctx->assoc_rsp.len = rsp->connect_ies.assoc_rsp.len;
			sta_ctx->assoc_rsp.ptr =
				qdf_mem_malloc(rsp->connect_ies.assoc_rsp.len);
			if (!sta_ctx->assoc_rsp.ptr) {
				QDF_ASSERT(0);
				return;
			}
			if (rsp->connect_ies.assoc_rsp.ptr)
				qdf_mem_copy(sta_ctx->assoc_rsp.ptr,
					     rsp->connect_ies.assoc_rsp.ptr,
					     rsp->connect_ies.assoc_rsp.len);
			sta_ctx->ml_partner_info = rsp->ml_parnter_info;
			/* Update connected_links_bmap for all vdev taking
			 * part in association
			 */
			mlo_update_connected_links(vdev, 1);
			mlo_update_connected_links_bmap(mlo_dev_ctx,
							rsp->ml_parnter_info);
		}
		mlo_send_link_connect(vdev, rsp);
	}
}

/**
 * mlo_send_link_disconnect_sync- Issue sync the disconnect request on MLD links
 *
 * @mlo_dev_ctx: pointer to mlo dev context
 * @source: disconnect source
 * @reason_code: disconnect reason
 * @bssid: bssid of AP to disconnect, can be null if not known
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
mlo_send_link_disconnect_sync(struct wlan_mlo_dev_context *mlo_dev_ctx,
			      enum wlan_cm_source source,
			      enum wlan_reason_code reason_code,
			      struct qdf_mac_addr *bssid)
{
	uint8_t i;
	struct wlan_objmgr_vdev *sync_vdev =
		mlo_get_assoc_link_vdev(mlo_dev_ctx);

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i] ||
		    mlo_dev_ctx->wlan_vdev_list[i] == sync_vdev) {
			continue;
		}

		/**
		 * If the assoc vdev isn't present, use the first link dev as
		 * sync candidate.
		 */
		if (!sync_vdev) {
			sync_vdev = mlo_dev_ctx->wlan_vdev_list[i];
			continue;
		}

		/**
		 * To initiate disconnect on all links at once, no need to use
		 * sync API for all link vdevs.
		 */
		wlan_cm_disconnect(mlo_dev_ctx->wlan_vdev_list[i],
				   source, reason_code, NULL);
	}

	if (sync_vdev)
		wlan_cm_disconnect_sync(sync_vdev, source, reason_code);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlo_disconnect_req(struct wlan_objmgr_vdev *vdev,
				     enum wlan_cm_source source,
				     enum wlan_reason_code reason_code,
				     struct qdf_mac_addr *bssid,
				     bool validate_req)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (mlo_dev_ctx)
		sta_ctx = mlo_dev_ctx->sta_ctx;
	if (mlo_dev_ctx && wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		mlo_dev_lock_acquire(mlo_dev_ctx);
		if (sta_ctx && sta_ctx->connect_req &&
		    source != CM_INTERNAL_DISCONNECT) {
			wlan_cm_free_connect_req(sta_ctx->connect_req);
			sta_ctx->connect_req = NULL;
		}

		if (validate_req) {
			status = mlo_validate_disconn_req(vdev, source,
							  reason_code, bssid);
			if (QDF_IS_STATUS_ERROR(status)) {
				mlo_err("Connect in progress, deferring disconnect");
				mlo_dev_lock_release(mlo_dev_ctx);
				return status;
			}
		}

		mlo_dev_lock_release(mlo_dev_ctx);

		status = mlo_send_link_disconnect(vdev, source,
						  reason_code, bssid);
		if (QDF_IS_STATUS_SUCCESS(status))
			mlo_free_copied_conn_req(sta_ctx);

		return status;
	}
	status = wlan_cm_disconnect(vdev, source,
				    reason_code, NULL);
	if (QDF_IS_STATUS_SUCCESS(status))
		mlo_free_copied_conn_req(sta_ctx);

	return status;
}

QDF_STATUS mlo_disconnect(struct wlan_objmgr_vdev *vdev,
			  enum wlan_cm_source source,
			  enum wlan_reason_code reason_code,
			  struct qdf_mac_addr *bssid)
{
	return mlo_disconnect_req(vdev, source, reason_code, bssid, true);
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * mlo_wait_for_mld_disconnection - API to wait for sync mld disconnection
 * @vdev: pointer to vdev
 *
 * Return: none
 */
static inline
void mlo_wait_for_mld_disconnection(struct wlan_objmgr_vdev *vdev)
{ }
#else
#define ML_DISCONNECT_CMD_TIMEOUT 1000
#define ML_DISCONNECT_CMD_TIMEOUT_CNT 30 /* 30*1000 ms */

static void mlo_wait_for_mld_disconnection(struct wlan_objmgr_vdev *vdev)
{
	int waitcnt = 0;
	qdf_event_t wait_event;

	qdf_mem_zero(&wait_event, sizeof(wait_event));
	qdf_event_create(&wait_event);
	qdf_event_reset(&wait_event);

	while (!mlo_is_mld_disconnected(vdev) &&
	       waitcnt < ML_DISCONNECT_CMD_TIMEOUT_CNT) {
		qdf_wait_single_event(&wait_event, ML_DISCONNECT_CMD_TIMEOUT);
		waitcnt++;
	}
	qdf_event_destroy(&wait_event);
}
#endif
QDF_STATUS mlo_sync_disconnect(struct wlan_objmgr_vdev *vdev,
			       enum wlan_cm_source source,
			       enum wlan_reason_code reason_code,
			       struct qdf_mac_addr *bssid)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (mlo_dev_ctx)
		sta_ctx = mlo_dev_ctx->sta_ctx;
	if (mlo_dev_ctx && wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		if (sta_ctx && sta_ctx->connect_req) {
			wlan_cm_free_connect_req(sta_ctx->connect_req);
			sta_ctx->connect_req = NULL;
		}

		status = mlo_validate_disconn_req(vdev, source,
						  reason_code, bssid);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("Connect in progress, deferring disconnect");
			return status;
		}

		status = mlo_send_link_disconnect_sync(mlo_dev_ctx, source,
						       reason_code, bssid);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			mlo_free_copied_conn_req(sta_ctx);
			mlo_wait_for_mld_disconnection(vdev);
		}

		return status;
	}
	status = wlan_cm_disconnect_sync(vdev, source,
					 reason_code);
	if (QDF_IS_STATUS_SUCCESS(status))
		mlo_free_copied_conn_req(sta_ctx);

	return status;
}

/**
 * mlo_handle_disconnect_resp- Issue desired actions on partner link vdev
 *
 * @vdev: pointer to vdev
 * @resp: disconnect resp
 *
 * Return: none
 */
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static
void mlo_handle_disconnect_resp(struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_discon_rsp *resp)
{
/* If it is secondary link then delete vdev object from mlo device. */
	enum wlan_cm_source source;
	enum wlan_reason_code reason_code;
	uint8_t i = 0;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS];
	uint16_t vdev_count = 0;

	mlo_sta_get_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	for (i =  0; i < vdev_count; i++) {
		if (wlan_cm_is_vdev_connected(wlan_vdev_list[i])) {
			if (wlan_vdev_mlme_is_mlo_link_vdev(
					wlan_vdev_list[i])) {
				source = resp->req.req.source;
				reason_code = resp->req.req.reason_code;
				wlan_cm_disconnect(
						wlan_vdev_list[i],
						source, reason_code, NULL);
			}
		}
		mlo_release_vdev_ref(wlan_vdev_list[i]);
	}
}
#else
static
void mlo_handle_disconnect_resp(struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_discon_rsp *resp)
{ }

static QDF_STATUS ml_activate_connect_req_sched_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;
	uint8_t i = 0;
	struct mlo_partner_info partner_info;
	struct mlo_link_info partner_link_info;
	struct wlan_objmgr_vdev *tmp_vdev;

	if (!vdev) {
		mlme_err("Null input vdev");
		return QDF_STATUS_E_INVAL;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_INVAL;
	}

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx || !sta_ctx->connect_req) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_INVAL;
	}

	if (sta_ctx->connect_req->ml_parnter_info.num_partner_links) {
		partner_info = sta_ctx->connect_req->ml_parnter_info;
		wlan_vdev_mlme_set_mlo_vdev(vdev);
		wlan_vdev_mlme_clear_mlo_link_vdev(vdev);
		mlo_clear_connect_req_links_bmap(vdev);
		mlo_update_connect_req_links(vdev, 1);
		for (i = 0; i < partner_info.num_partner_links; i++) {
			partner_link_info = partner_info.partner_link_info[i];
			tmp_vdev = mlo_get_ml_vdev_by_mac(
					vdev,
					&partner_link_info.link_addr);
			if (tmp_vdev) {
				mlo_update_connect_req_links(tmp_vdev, 1);
				wlan_vdev_mlme_set_mlo_vdev(tmp_vdev);
				wlan_vdev_mlme_set_mlo_link_vdev(tmp_vdev);
				wlan_vdev_set_link_id(
					tmp_vdev,
					partner_link_info.link_id);
			}
		}
	}

	mlo_connect(vdev, sta_ctx->connect_req);
	wlan_cm_free_connect_req(sta_ctx->connect_req);
	sta_ctx->connect_req = NULL;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ml_activate_connect_req_flush_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;

	if (!vdev) {
		mlme_err("Null input vdev");
		return QDF_STATUS_E_INVAL;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static inline
void mlo_sta_link_handle_pending_connect(struct wlan_objmgr_vdev *vdev)
{ }
#else
static inline
void mlo_sta_link_handle_pending_connect(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct scheduler_msg msg = {0};
	QDF_STATUS ret;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is null");
		return;
	}
	sta_ctx = mlo_dev_ctx->sta_ctx;
	ret = wlan_objmgr_vdev_try_get_ref(
			vdev,
			WLAN_MLO_MGR_ID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wlan_cm_free_connect_req(sta_ctx->connect_req);
		sta_ctx->connect_req = NULL;
		return;
	}
	msg.bodyptr = vdev;
	msg.callback = ml_activate_connect_req_sched_cb;
	msg.flush_callback = ml_activate_connect_req_flush_cb;

	ret = scheduler_post_message(QDF_MODULE_ID_MLME,
				     QDF_MODULE_ID_MLME,
				     QDF_MODULE_ID_MLME, &msg);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wlan_cm_free_connect_req(sta_ctx->connect_req);
		sta_ctx->connect_req = NULL;
		wlan_objmgr_vdev_release_ref(vdev,
					     WLAN_MLO_MGR_ID);
		return;
	}
}
#endif

void mlo_sta_link_disconn_notify(struct wlan_objmgr_vdev *vdev,
				 struct wlan_cm_discon_rsp *resp)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	struct wlan_objmgr_vdev *assoc_vdev = NULL;

	if (!mlo_dev_ctx || !(wlan_vdev_mlme_is_mlo_vdev(vdev)))
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	if (!wlan_cm_is_vdev_disconnected(vdev))
		return;

	mlo_update_connected_links(vdev, 0);
	if (mlo_is_mld_disconnected(vdev)) {
		if (sta_ctx->connect_req) {
			assoc_vdev = mlo_get_assoc_link_vdev(mlo_dev_ctx);
			if (!assoc_vdev)
				return;
			mlo_sta_link_handle_pending_connect(assoc_vdev);
		}
	}

	if (!wlan_cm_is_link_switch_disconnect_resp(resp))
		mlo_handle_disconnect_resp(vdev, resp);
}

bool mlo_is_mld_sta(struct wlan_objmgr_vdev *vdev)
{
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) &&
	    wlan_vdev_mlme_is_mlo_vdev(vdev))
		return true;

	return false;
}

qdf_export_symbol(mlo_is_mld_sta);
#ifndef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
struct wlan_objmgr_vdev *
mlo_get_ml_vdev_by_mac(struct wlan_objmgr_vdev *vdev,
		       struct qdf_mac_addr *macaddr)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	uint8_t i = 0;

	if (!mlo_dev_ctx)
		return NULL;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if(qdf_mem_cmp(macaddr,
			       wlan_vdev_mlme_get_macaddr(mlo_dev_ctx->wlan_vdev_list[i]),
			       QDF_MAC_ADDR_SIZE) == 0) {
			return mlo_dev_ctx->wlan_vdev_list[i];
		}
	}
	return NULL;
}
#endif

qdf_freq_t
mlo_get_chan_freq_by_bssid(struct wlan_objmgr_pdev *pdev,
			   struct qdf_mac_addr *bssid)
{
	struct scan_filter *scan_filter;
	int8_t ch_freq = 0;
	qdf_list_t *list = NULL;
	struct scan_cache_node *first_node = NULL;
	qdf_list_node_t *cur_node = NULL;

	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter)
		return ch_freq;

	scan_filter->num_of_bssid = 1;
	qdf_mem_copy(scan_filter->bssid_list[0].bytes,
		     bssid, sizeof(struct qdf_mac_addr));
	list = wlan_scan_get_result(pdev, scan_filter);
	qdf_mem_free(scan_filter);

	if (!list || (list && !qdf_list_size(list))) {
		mlo_debug("scan list empty");
		goto error;
	}

	qdf_list_peek_front(list, &cur_node);
	first_node = qdf_container_of(cur_node,
				      struct scan_cache_node,
				      node);
	if (first_node && first_node->entry)
		ch_freq = first_node->entry->channel.chan_freq;
error:
	if (list)
		wlan_scan_purge_results(list);

	return ch_freq;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * mlo_get_reassoc_rsp() - To get reassoc response
 * @vdev: objmgr vdev
 * @reassoc_rsp_frame: reassoc rsp
 *
 * Return: NA
 */
static
void mlo_get_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
			 struct element_info **reassoc_rsp_frame)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx->copied_reassoc_rsp) {
		mlo_err("Reassoc rsp not present for vdev_id %d",
			wlan_vdev_get_id(vdev));
		*reassoc_rsp_frame = NULL;
		return;
	}

	if (!sta_ctx->copied_reassoc_rsp->connect_ies.assoc_rsp.len ||
	    !sta_ctx->copied_reassoc_rsp->connect_ies.assoc_rsp.ptr) {
		mlo_err("Reassoc Resp info empty vdev_id %d assoc len %d",
			wlan_vdev_get_id(vdev),
			sta_ctx->copied_reassoc_rsp->connect_ies.assoc_rsp.len);
		*reassoc_rsp_frame = NULL;
		return;
	}

	*reassoc_rsp_frame =
		&sta_ctx->copied_reassoc_rsp->connect_ies.assoc_rsp;
}
#else
static inline
void mlo_get_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
			 struct element_info **reassoc_rsp_frame)
{
	*reassoc_rsp_frame = NULL;
}
#endif

void mlo_get_assoc_rsp(struct wlan_objmgr_vdev *vdev,
		       struct element_info *assoc_rsp_frame)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	struct element_info *mlo_reassoc_rsp = NULL;

	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (sta_ctx->assoc_rsp.len && sta_ctx->assoc_rsp.ptr) {
		*assoc_rsp_frame = sta_ctx->assoc_rsp;
		return;
	}
	mlo_get_reassoc_rsp(vdev, &mlo_reassoc_rsp);
	if (mlo_reassoc_rsp)
		*assoc_rsp_frame = *mlo_reassoc_rsp;
}

QDF_STATUS mlo_sta_save_quiet_status(struct wlan_mlo_dev_context *mlo_dev_ctx,
				     uint8_t link_id,
				     bool quiet_status)
{
	struct wlan_mlo_sta *sta_ctx;
	int i;
	bool find_free_buffer = false;
	int free_idx;

	if (!mlo_dev_ctx) {
		mlo_err("invalid mlo_dev_ctx");
		return QDF_STATUS_E_INVAL;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx) {
		mlo_err("invalid sta_ctx");
		mlo_dev_lock_release(mlo_dev_ctx);
		return QDF_STATUS_E_INVAL;
	}
	for (i = 0; i < QDF_ARRAY_SIZE(sta_ctx->mlo_quiet_status); i++) {
		if (!sta_ctx->mlo_quiet_status[i].valid_status) {
			if (!find_free_buffer) {
				free_idx = i;
				find_free_buffer = true;
			}
		} else if (link_id == sta_ctx->mlo_quiet_status[i].link_id) {
			sta_ctx->mlo_quiet_status[i].quiet_status =
							quiet_status;
			mlo_debug("mld mac " QDF_MAC_ADDR_FMT " link id %d quiet status update %d",
				  QDF_MAC_ADDR_REF(mlo_dev_ctx->mld_addr.bytes),
				  link_id, quiet_status);
			mlo_dev_lock_release(mlo_dev_ctx);
			return QDF_STATUS_SUCCESS;
		}
	}
	if (!find_free_buffer) {
		mlo_err("no free buffer for link id %d to save quiet_status",
			link_id);
		mlo_dev_lock_release(mlo_dev_ctx);
		return QDF_STATUS_E_INVAL;
	}
	sta_ctx->mlo_quiet_status[free_idx].quiet_status = quiet_status;
	sta_ctx->mlo_quiet_status[free_idx].link_id = link_id;
	sta_ctx->mlo_quiet_status[free_idx].valid_status = true;

	mlo_debug("mld mac " QDF_MAC_ADDR_FMT " link id %d in quiet status %d",
		  QDF_MAC_ADDR_REF(mlo_dev_ctx->mld_addr.bytes),
		  link_id, quiet_status);
	mlo_dev_lock_release(mlo_dev_ctx);

	return QDF_STATUS_SUCCESS;
}

bool mlo_is_sta_in_quiet_status(struct wlan_mlo_dev_context *mlo_dev_ctx,
				uint8_t link_id)
{
	struct wlan_mlo_sta *sta_ctx;
	int i;
	bool quiet_status = false;

	if (!mlo_dev_ctx) {
		mlo_err("invalid mlo_dev_ctx");
		return quiet_status;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx) {
		mlo_err("invalid sta_ctx");
		mlo_dev_lock_release(mlo_dev_ctx);
		return quiet_status;
	}
	for (i = 0; i < QDF_ARRAY_SIZE(sta_ctx->mlo_quiet_status); i++) {
		if (sta_ctx->mlo_quiet_status[i].valid_status &&
		    link_id == sta_ctx->mlo_quiet_status[i].link_id) {
			quiet_status =
				sta_ctx->mlo_quiet_status[i].quiet_status;
			break;
		}
	}
	mlo_dev_lock_release(mlo_dev_ctx);

	return quiet_status;
}

bool mlo_is_sta_inactivity_allowed_with_quiet(struct wlan_objmgr_psoc *psoc,
					      uint8_t *vdev_id_list,
					      uint8_t num_mlo, uint8_t *mlo_idx,
					      uint8_t affected_links,
					      uint8_t *affected_list)
{
	uint8_t i, j;
	struct wlan_objmgr_vdev *vdev;
	bool allowed = false;

	for (i = 0; i < num_mlo; i++) {
		for (j = 0; j < affected_links; j++) {
			if (vdev_id_list[mlo_idx[i]] == affected_list[j])
				break;
		}
		if (j != affected_links)
			continue;
		/* find vdev not in affected_list */
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				psoc, vdev_id_list[mlo_idx[i]],
				WLAN_IF_MGR_ID);
		if (!vdev) {
			mlo_err("invalid vdev for id %d",
				vdev_id_list[mlo_idx[i]]);
			continue;
		}

		/* for not affected vdev, check the vdev is in quiet or not*/
		allowed = !mlo_is_sta_in_quiet_status(
				vdev->mlo_dev_ctx, wlan_vdev_get_link_id(vdev));
		wlan_objmgr_vdev_release_ref(vdev, WLAN_IF_MGR_ID);
		if (allowed) {
			mlo_debug("vdev id %d link id %d is not in quiet, allow partner link to trigger inactivity",
				  wlan_vdev_get_id(vdev),
				  wlan_vdev_get_link_id(vdev));
			break;
		}
	}

	return allowed;
}

bool mlo_is_sta_csa_synced(struct wlan_mlo_dev_context *mlo_dev_ctx,
			   uint8_t link_id)
{
	struct wlan_mlo_sta *sta_ctx;
	int i;
	bool sta_csa_synced = false;

	if (!mlo_dev_ctx) {
		mlo_err("invalid mlo_dev_ctx");
		return sta_csa_synced;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx) {
		mlo_err("invalid sta_ctx");
		mlo_dev_lock_release(mlo_dev_ctx);
		return sta_csa_synced;
	}
	for (i = 0; i < QDF_ARRAY_SIZE(sta_ctx->mlo_csa_param); i++) {
		if (link_id == sta_ctx->mlo_csa_param[i].link_id &&
		    (sta_ctx->mlo_csa_param[i].valid_csa_param ||
		     sta_ctx->mlo_csa_param[i].mlo_csa_synced)) {
			sta_csa_synced =
				sta_ctx->mlo_csa_param[i].mlo_csa_synced;
			break;
		}
	}
	mlo_dev_lock_release(mlo_dev_ctx);

	return sta_csa_synced;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
QDF_STATUS mlo_sta_handle_csa_standby_link(
			struct wlan_mlo_dev_context *mlo_dev_ctx,
			uint8_t link_id,
			struct csa_offload_params *csa_param,
			struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct mlo_link_info *link_info;
	struct mlo_link_bss_params params = {0};
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;

	if (!mlo_dev_ctx) {
		mlo_err("invalid mlo_dev_ctx");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlo_err("null pdev");
		return QDF_STATUS_E_NULL_VALUE;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		mlo_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = &psoc->soc_cb.tx_ops->mlo_ops;

	link_info = mlo_mgr_get_ap_link_by_link_id(mlo_dev_ctx, link_id);
	if (!link_info) {
		mlo_err("link info null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (link_info->vdev_id != WLAN_INVALID_VDEV_ID) {
		mlo_debug("vdev id %d link id %d ", link_info->vdev_id,
			  link_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_mgr_update_csa_link_info(pdev, mlo_dev_ctx, csa_param, link_id);

	params.link_id = link_info->link_id;
	params.chan = qdf_mem_malloc(sizeof(struct wlan_channel));
	if (!params.chan) {
		mlo_err("no mem allocated");
		return QDF_STATUS_E_NOMEM;
	}

	wlan_vdev_get_bss_peer_mld_mac(
				vdev,
				(struct qdf_mac_addr *)&params.ap_mld_mac[0]);

	params.chan->ch_freq = link_info->link_chan_info->ch_freq;
	params.chan->ch_cfreq1 = link_info->link_chan_info->ch_cfreq1;
	params.chan->ch_cfreq2 = link_info->link_chan_info->ch_cfreq2;
	params.chan->ch_phymode = link_info->link_chan_info->ch_phymode;

	mlo_debug("link id %d chan freq %d cfreq1 %d cfreq2 %d host phymode %d ap mld mac " QDF_MAC_ADDR_FMT,
		  link_info->link_id, link_info->link_chan_info->ch_freq,
		  link_info->link_chan_info->ch_cfreq1,
		  link_info->link_chan_info->ch_cfreq2,
		  link_info->link_chan_info->ch_phymode,
		  QDF_MAC_ADDR_REF(&params.ap_mld_mac[0]));

	if (!mlo_tx_ops->send_link_set_bss_params_cmd) {
		mlo_err("handler is not registered");
		qdf_mem_free(params.chan);
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = mlo_tx_ops->send_link_set_bss_params_cmd(psoc, &params);

	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("failed to send link set bss request command to FW");
		qdf_mem_free(params.chan);
		return QDF_STATUS_E_NULL_VALUE;
	}
	qdf_mem_free(params.chan);
	return status;
}

static void mlo_sta_handle_link_reconfig_standby_link(
			struct wlan_objmgr_vdev *vdev,
			struct ml_rv_info *reconfig_info)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;
	if (vdev_mlme->ops &&
	    vdev_mlme->ops->mlme_vdev_reconfig_notify_standby) {
		vdev_mlme->ops->mlme_vdev_reconfig_notify_standby(
				vdev_mlme,
				reconfig_info);
	}
}
#else
static void mlo_sta_handle_link_reconfig_standby_link(
			struct wlan_objmgr_vdev *vdev,
			struct ml_rv_info *reconfig_info)
{
}
#endif

QDF_STATUS mlo_sta_csa_save_params(struct wlan_mlo_dev_context *mlo_dev_ctx,
				   uint8_t link_id,
				   struct csa_offload_params *csa_param)
{
	struct wlan_mlo_sta *sta_ctx;
	int i;
	bool find_free_buffer = false;
	int free_idx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!mlo_dev_ctx) {
		mlo_err("invalid mlo_dev_ctx");
		status = QDF_STATUS_E_INVAL;
		goto done;
	}

	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx) {
		mlo_err("invalid sta_ctx");
		status = QDF_STATUS_E_INVAL;
		goto rel_lock;
	}
	for (i = 0; i < QDF_ARRAY_SIZE(sta_ctx->mlo_csa_param); i++) {
		if (!sta_ctx->mlo_csa_param[i].valid_csa_param &&
		    !sta_ctx->mlo_csa_param[i].mlo_csa_synced) {
			if (!find_free_buffer) {
				free_idx = i;
				find_free_buffer = true;
			}
		} else if (link_id == sta_ctx->mlo_csa_param[i].link_id) {
			qdf_mem_copy(&sta_ctx->mlo_csa_param[i].csa_param,
				     csa_param, sizeof(*csa_param));
			mlo_debug("mld mac " QDF_MAC_ADDR_FMT " link id %d update csa",
				  QDF_MAC_ADDR_REF(mlo_dev_ctx->mld_addr.bytes),
				  link_id);
			goto rel_lock;
		}
	}
	if (!find_free_buffer) {
		mlo_err("no free buffer of csa param for link %d in sta_ctx",
			link_id);
		status = QDF_STATUS_E_INVAL;
		goto rel_lock;
	}
	qdf_mem_copy(&sta_ctx->mlo_csa_param[free_idx].csa_param,
		     csa_param, sizeof(*csa_param));
	sta_ctx->mlo_csa_param[free_idx].link_id = link_id;
	sta_ctx->mlo_csa_param[free_idx].valid_csa_param = true;
	mlo_debug("mld mac " QDF_MAC_ADDR_FMT " link id %d RX csa",
		  QDF_MAC_ADDR_REF(mlo_dev_ctx->mld_addr.bytes),
		  link_id);

rel_lock:
	mlo_dev_lock_release(mlo_dev_ctx);

done:

	return status;
}

QDF_STATUS mlo_sta_up_active_notify(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_sta *sta_ctx;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	uint8_t link_id;
	int i;
	bool find_free_buffer = false;
	int free_idx;
	struct csa_offload_params csa_param;
	struct wlan_channel *chan;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev) {
		mlo_err("invalid vdev");
		status = QDF_STATUS_E_INVAL;
		goto done;
	}
	link_id = wlan_vdev_get_link_id(vdev);
	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		mlo_err("invalid mlo_dev_ctx");
		status = QDF_STATUS_E_INVAL;
		goto done;
	}
	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx) {
		mlo_err("invalid sta_ctx");
		status = QDF_STATUS_E_INVAL;
		goto rel_lock;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(sta_ctx->mlo_csa_param); i++) {
		if (!sta_ctx->mlo_csa_param[i].valid_csa_param &&
		    !sta_ctx->mlo_csa_param[i].mlo_csa_synced) {
			if (!find_free_buffer) {
				free_idx = i;
				find_free_buffer = true;
			}
		} else if (link_id == sta_ctx->mlo_csa_param[i].link_id) {
			if (sta_ctx->mlo_csa_param[i].valid_csa_param &&
			    !sta_ctx->mlo_csa_param[i].mlo_csa_synced) {
				mlo_debug("mld mac " QDF_MAC_ADDR_FMT " vdev id %d link id %d handle csa",
					  QDF_MAC_ADDR_REF(
						mlo_dev_ctx->mld_addr.bytes),
					  wlan_vdev_get_id(vdev), link_id);
				csa_param = sta_ctx->mlo_csa_param[i].csa_param;
				sta_ctx->mlo_csa_param[i].mlo_csa_synced = true;
				mlo_dev_lock_release(mlo_dev_ctx);
				chan = wlan_vdev_mlme_get_bss_chan(vdev);
				if (csa_param.csa_chan_freq && chan &&
				    csa_param.csa_chan_freq != chan->ch_freq)
					mlo_mlme_handle_sta_csa_param(
						vdev, &csa_param);
				goto done;
			}
			sta_ctx->mlo_csa_param[i].mlo_csa_synced = true;
			goto rel_lock;
		}
	}
	if (!find_free_buffer) {
		mlo_err("no free buffer of csa param for link %d in sta_ctx",
			link_id);
		goto rel_lock;
	}
	sta_ctx->mlo_csa_param[free_idx].mlo_csa_synced = true;
	sta_ctx->mlo_csa_param[free_idx].link_id = link_id;
	mlo_debug("mld mac " QDF_MAC_ADDR_FMT " link id %d UP Active",
		  QDF_MAC_ADDR_REF(mlo_dev_ctx->mld_addr.bytes),
		  link_id);

rel_lock:
	mlo_dev_lock_release(mlo_dev_ctx);

done:

	return status;
}

bool mlo_is_sta_csa_param_handled(struct wlan_objmgr_vdev *vdev,
				  struct csa_offload_params *csa_param)
{
	struct wlan_mlo_sta *sta_ctx;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	uint8_t link_id;
	int i;
	bool handled = false;

	if (!vdev) {
		mlo_err("invalid vdev");
		goto done;
	}
	link_id = wlan_vdev_get_link_id(vdev);
	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		mlo_err("invalid mlo_dev_ctx");
		goto done;
	}
	mlo_dev_lock_acquire(mlo_dev_ctx);
	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx) {
		mlo_err("invalid sta_ctx");
		goto rel_lock;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(sta_ctx->mlo_csa_param); i++) {
		if (link_id == sta_ctx->mlo_csa_param[i].link_id &&
		    (sta_ctx->mlo_csa_param[i].valid_csa_param ||
		     sta_ctx->mlo_csa_param[i].mlo_csa_synced))
			break;
	}

	if (i >= QDF_ARRAY_SIZE(sta_ctx->mlo_csa_param)) {
		mlo_debug("mlo csa synced does not happen before csa FW event");
		goto rel_lock;
	}
	if (!sta_ctx->mlo_csa_param[i].csa_offload_event_recvd) {
		sta_ctx->mlo_csa_param[i].csa_offload_event_recvd = true;
		if (sta_ctx->mlo_csa_param[i].valid_csa_param &&
		    !qdf_mem_cmp(&sta_ctx->mlo_csa_param[i].csa_param,
				 csa_param, sizeof(*csa_param)))
			handled = true;
	}

rel_lock:
	mlo_dev_lock_release(mlo_dev_ctx);

done:

	return handled;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
void mlo_internal_disconnect_links(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	struct wlan_mlo_sta *sta_ctx = NULL;
	uint8_t i;
	struct wlan_objmgr_vdev *assoc_vdev;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS];
	uint16_t vdev_count = 0;

	if (!vdev)
		return;

	if (!wlan_vdev_mlme_is_assoc_sta_vdev(vdev) &&
	    !wlan_mlo_mgr_is_link_switch_on_assoc_vdev(vdev)) {
		mlo_debug("Not an assoc vdev, so ignore disconnect req");
		return;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (mlo_dev_ctx) {
		sta_ctx = mlo_dev_ctx->sta_ctx;
	} else {
		mlo_err("Invalid mlo_dev_ctx");
		return;
	}

	if (sta_ctx) {
		mlo_free_copied_conn_req(sta_ctx);
	} else {
		mlo_err("Invalid sta_ctx");
		return;
	}

	if (sta_ctx->connect_req) {
		wlan_cm_free_connect_req(sta_ctx->connect_req);
		sta_ctx->connect_req = NULL;
	}

	assoc_vdev = mlo_get_assoc_link_vdev(mlo_dev_ctx);
	if (!assoc_vdev) {
		assoc_vdev = wlan_mlo_mgr_link_switch_get_assoc_vdev(vdev);
		if (!assoc_vdev) {
			mlo_debug("Couldn't get assoc vdev");
			return;
		}
		mlo_release_vdev_ref(assoc_vdev);
	}

	mlo_sta_get_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	for (i =  0; i < vdev_count; i++) {
		if (wlan_vdev_list[i] != assoc_vdev &&
		    (wlan_cm_is_vdev_connected(wlan_vdev_list[i]) ||
		     wlan_cm_is_vdev_connecting(wlan_vdev_list[i]) ||
		     wlan_cm_is_vdev_idle_due_to_link_switch(wlan_vdev_list[i])))
			wlan_cm_disconnect(wlan_vdev_list[i],
					   CM_MLO_LINK_VDEV_DISCONNECT,
					   REASON_UNSPEC_FAILURE,
					   NULL);
		mlo_release_vdev_ref(wlan_vdev_list[i]);
	}
}
#else
void mlo_internal_disconnect_links(struct wlan_objmgr_vdev *vdev)
{
}
#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */

void mlo_sta_get_vdev_list(struct wlan_objmgr_vdev *vdev, uint16_t *vdev_count,
			   struct wlan_objmgr_vdev **wlan_vdev_list)
{
	struct wlan_mlo_dev_context *dev_ctx;
	int i;
	QDF_STATUS status;

	*vdev_count = 0;

	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Invalid input");
		return;
	}

	dev_ctx = vdev->mlo_dev_ctx;

	mlo_dev_lock_acquire(dev_ctx);
	*vdev_count = 0;
	for (i = 0; i < QDF_ARRAY_SIZE(dev_ctx->wlan_vdev_list); i++) {
		if (dev_ctx->wlan_vdev_list[i]) {
			status =
			wlan_objmgr_vdev_try_get_ref(dev_ctx->wlan_vdev_list[i],
						     WLAN_MLO_MGR_ID);
			if (QDF_IS_STATUS_ERROR(status))
				break;
			wlan_vdev_list[*vdev_count] =
				dev_ctx->wlan_vdev_list[i];
			(*vdev_count) += 1;
		}
	}
	mlo_dev_lock_release(dev_ctx);
}

bool mlo_sta_vdev_get_reconfig_timer_state(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	if (!vdev || !mlo_is_mld_sta(vdev))
		return false;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlo_err("vdev mlme is null");
		return false;
	}

	return vdev_mlme->ml_reconfig_started;
}

void mlo_sta_stop_reconfig_timer_by_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	if (!vdev || !mlo_is_mld_sta(vdev))
		return;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlo_err("vdev mlme is null");
		return;
	}

	if (!vdev_mlme->ml_reconfig_started)
		return;

	qdf_timer_stop(&vdev_mlme->ml_reconfig_timer);

	mlo_debug("vdev %d reconfig timer active to stop",
		  wlan_vdev_get_id(vdev));
	vdev_mlme->ml_reconfig_started = false;
}

void mlo_sta_stop_reconfig_timer(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	uint16_t vdev_count = 0;
	uint8_t i;

	if (!vdev || !mlo_is_mld_sta(vdev))
		return;

	mlo_get_ml_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	if (!vdev_count) {
		mlo_err("vdev num 0 in mld dev");
		return;
	}

	for (i = 0; i < vdev_count; i++) {
		if (!wlan_vdev_list[i]) {
			mlo_err("vdev is null in mld");
			goto release_ref;
		}
		mlo_sta_stop_reconfig_timer_by_vdev(wlan_vdev_list[i]);
	}

release_ref:
	for (i = 0; i < vdev_count; i++)
		mlo_release_vdev_ref(wlan_vdev_list[i]);
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
void mlo_defer_set_keys(struct wlan_objmgr_vdev *vdev,
			uint8_t link_id, bool value)
{
	struct wlan_mlo_sta *sta_ctx;
	uint8_t link_iter;

	if (!vdev || !vdev->mlo_dev_ctx)
		return;

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;
	if (value) {
		for (link_iter = 0; link_iter < WLAN_MAX_ML_BSS_LINKS;
		     link_iter++) {
			if (sta_ctx->key_mgmt[link_iter].keys_saved &&
			    sta_ctx->key_mgmt[link_iter].link_id == link_id)
				return;
		}

		for (link_iter = 0; link_iter < WLAN_MAX_ML_BSS_LINKS;
		     link_iter++) {
			if (!sta_ctx->key_mgmt[link_iter].keys_saved) {
				sta_ctx->key_mgmt[link_iter].link_id = link_id;
				sta_ctx->key_mgmt[link_iter].keys_saved = true;
				mlo_debug("set key link id %d value %d iter %d",
					  link_id, value, link_iter);
				return;
			}
		}
	} else {
		for (link_iter = 0; link_iter < WLAN_MAX_ML_BSS_LINKS;
		     link_iter++) {
			if (sta_ctx->key_mgmt[link_iter].link_id == link_id) {
				sta_ctx->key_mgmt[link_iter].keys_saved = false;
				mlo_debug("set key link id %d value %d iter %d",
					  link_id, value, link_iter);
				return;
			}
		}
	}
}

bool mlo_is_set_key_defered(struct wlan_objmgr_vdev *vdev,
			    uint8_t link_id)
{
	struct wlan_mlo_sta *sta_ctx;
	uint8_t link_iter;

	if (!vdev || !vdev->mlo_dev_ctx)
		return false;

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return false;

	for (link_iter = 0; link_iter < WLAN_MAX_ML_BSS_LINKS;
	     link_iter++) {
		if (sta_ctx->key_mgmt[link_iter].link_id == link_id)
			return sta_ctx->key_mgmt[link_iter].keys_saved;
	}
	return false;
}
#endif

static uint16_t
mlo_get_bcn_interval_by_bssid(struct wlan_objmgr_pdev *pdev,
			      uint8_t *bssid)
{
	struct scan_filter *scan_filter;
	uint16_t bcn_int = 0;
	qdf_list_t *list = NULL;
	struct scan_cache_node *first_node = NULL;
	qdf_list_node_t *cur_node = NULL;

	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter)
		return bcn_int;

	scan_filter->num_of_bssid = 1;
	qdf_mem_copy(scan_filter->bssid_list[0].bytes,
		     bssid, sizeof(struct qdf_mac_addr));
	list = wlan_scan_get_result(pdev, scan_filter);
	qdf_mem_free(scan_filter);

	if (!list || (list && !qdf_list_size(list))) {
		mlo_debug("scan list empty");
		goto error;
	}

	qdf_list_peek_front(list, &cur_node);
	first_node = qdf_container_of(cur_node,
				      struct scan_cache_node,
				      node);
	if (first_node && first_node->entry)
		bcn_int = first_node->entry->bcn_int;
error:
	if (list)
		wlan_scan_purge_results(list);

	return bcn_int;
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
static QDF_STATUS
mlo_sta_handle_ptqm_migration(struct wlan_objmgr_vdev *removal_vdev)
{
	struct wlan_objmgr_peer *bss_peer;

	if (!wlan_cm_is_vdev_connected(removal_vdev))
		return QDF_STATUS_E_INVAL;

	bss_peer = wlan_vdev_get_bsspeer(removal_vdev);
	if (!bss_peer)
		return QDF_STATUS_E_INVAL;

	/* Invoke migration only if the link being removed is the primary */
	if (wlan_mlo_peer_get_primary_peer_link_id(bss_peer)
		!= wlan_vdev_get_link_id(removal_vdev))
		return QDF_STATUS_SUCCESS;

	return wlan_mlo_set_ptqm_migration(removal_vdev, bss_peer->mlo_peer_ctx,
					   false, WLAN_LINK_ID_INVALID, true);
}
#else
static QDF_STATUS
mlo_sta_handle_ptqm_migration(struct wlan_objmgr_vdev *removal_vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static void mlo_process_link_remove(struct wlan_objmgr_vdev *vdev,
				    struct ml_rv_partner_link_info *link_info)
{
	struct vdev_mlme_obj *vdev_mlme = NULL;
	struct wlan_objmgr_peer *bss_peer = NULL;
	uint16_t bcn_int = 0;
	uint16_t tbtt_count = 0;
	QDF_STATUS status;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	bss_peer = wlan_vdev_get_bsspeer(vdev);
	if (!bss_peer)
		return;

	/* Link delete triggered from AP,
	 * start timer with tbtt count * beacon interval
	 */
	tbtt_count = link_info->ap_removal_timer;
	bcn_int = mlo_get_bcn_interval_by_bssid(
			wlan_vdev_get_pdev(vdev),
			wlan_peer_get_macaddr(bss_peer));
	if (!bcn_int)
		return;

	if (vdev_mlme->ops &&
	    vdev_mlme->ops->mlme_vdev_reconfig_notify) {
		status = vdev_mlme->ops->mlme_vdev_reconfig_notify(
				vdev_mlme, &tbtt_count, bcn_int);
		if (QDF_IS_STATUS_ERROR(status))
			return;
	}

	/* Handle PTQM migration upon seeing AP removal for the first time */
	if (!vdev_mlme->ml_reconfig_started)
		mlo_sta_handle_ptqm_migration(vdev);

	vdev_mlme->ml_reconfig_started = true;
	qdf_timer_mod(&vdev_mlme->ml_reconfig_timer,
		      qdf_time_uint_to_ms(tbtt_count * bcn_int));
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static inline
QDF_STATUS mlo_process_link_add(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct mlo_partner_info *cache_partner_info,
				struct mlo_partner_info *partner_info,
				uint16_t vdev_count)
{
	return QDF_STATUS_E_INVAL;
}
#else
static
QDF_STATUS mlo_process_link_add(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct mlo_partner_info *cache_partner_info,
				struct mlo_partner_info *partner_info,
				uint16_t vdev_count)
{
	struct wlan_mlo_dev_context *ml_ctx = vdev->mlo_dev_ctx;

	/* Check if ini to support dynamic link add is enable
	 * or not
	 */
	if (!mlme_mlo_is_reconfig_reassoc_enable(psoc)) {
		mlo_debug("ML Reconfig link add support disabled");
		return QDF_STATUS_E_INVAL;
	}

	if (vdev_count == ml_ctx->wlan_vdev_count) {
		/* All links are participating in current ML connection */
		return QDF_STATUS_E_INVAL;
	}

	/* check if any new link in scan entry */
	if (partner_info->num_partner_links ==
	    cache_partner_info->num_partner_links) {
		if (!qdf_mem_cmp(cache_partner_info, partner_info,
				 sizeof(struct mlo_partner_info))) {
			mlo_debug("No new link found");
			return QDF_STATUS_E_INVAL;
		}
	}

	/* mlo connected on all links already */
	if (partner_info->num_partner_links <= (vdev_count - 1))
		return QDF_STATUS_E_INVAL;

	/* Partner info changed compared to the cached scan entry.
	 * Process link add
	 */
	mlo_err("Link addition detected, issue disconnect");
	mlo_disconnect(vdev, CM_SB_DISCONNECT,
		       REASON_UNSPEC_FAILURE, NULL);
	return QDF_STATUS_SUCCESS;
}
#endif

void mlo_process_ml_reconfig_ie(struct wlan_objmgr_vdev *vdev,
				struct scan_cache_entry *scan_entry,
				uint8_t *ml_ie, qdf_size_t ml_ie_len,
				struct mlo_partner_info *partner_info)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *co_mld_vdev = NULL;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {NULL};
	uint16_t vdev_count = 0;
	uint8_t idx = 0;
	uint8_t i = 0;
	uint8_t link_ix = 0;
	struct ml_rv_info reconfig_info = {0};
	struct mlo_partner_info ml_partner_info = {0};
	uint8_t *ml_rv_ie = NULL;
	qdf_size_t ml_rv_ie_len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!vdev || !mlo_is_mld_sta(vdev))
		return;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return;

	mlo_get_ml_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	if (!vdev_count) {
		mlo_debug("Number of VDEVs under MLD is reported as 0");
		return;
	}

	if (!scan_entry ||
	    QDF_IS_STATUS_ERROR(util_scan_get_ml_partner_info(scan_entry,
							      &ml_partner_info))) {
		mlo_debug("Unable to fetch the partner info in scan db");
		goto check_ml_rv;
	}

	/* Processing for link add */
	if (QDF_IS_STATUS_SUCCESS(mlo_process_link_add(psoc, vdev,
						       partner_info,
						       &ml_partner_info,
						       vdev_count))) {
		mlo_err("Issued MLD disconnect for link add");
		goto err_release_refs;
	}

check_ml_rv:
	/* Processing for ML Reconfig IE */
	if (vdev_count == 1) {
		/* Single link MLO, no need to process link delete */
		goto err_release_refs;
	}

	status = util_find_mlie_by_variant(ml_ie,
					   ml_ie_len,
					   &ml_rv_ie,
					   &ml_rv_ie_len,
					   WLAN_ML_VARIANT_RECONFIG);
	if (QDF_IS_STATUS_ERROR(status) || !ml_rv_ie) {
		mlo_debug("ML IE for reconfig variant not found");
		goto err_release_refs;
	}

	status = util_get_rvmlie_persta_link_info(ml_rv_ie, ml_rv_ie_len,
						  &reconfig_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Unable to get persta link info from ML RV IE");
		goto err_release_refs;
	}

	if (!reconfig_info.num_links) {
		mlo_err("No. of links is 0 in ML reconfig IE");
		goto err_release_refs;
	}

	for (idx = 0; idx < vdev_count; idx++) {
		co_mld_vdev = wlan_vdev_list[idx];
		if (!co_mld_vdev) {
			mlo_debug("VDEV in MLD VDEV list is NULL");
			goto err_release_refs;
		}

		link_ix = wlan_vdev_get_link_id(co_mld_vdev);
		for (i = 0; i < reconfig_info.num_links; i++) {
			if (link_ix == reconfig_info.link_info[i].link_id)
				mlo_process_link_remove(co_mld_vdev,
							&reconfig_info.link_info[i]);
		}
	}

	mlo_sta_handle_link_reconfig_standby_link(vdev, &reconfig_info);

err_release_refs:

	for (i = 0; i < vdev_count; i++)
		mlo_release_vdev_ref(wlan_vdev_list[i]);
}

QDF_STATUS
mlo_get_link_state_context(struct wlan_objmgr_psoc *psoc,
			   get_ml_link_state_cb *resp_cb,
			   void **context, uint8_t vdev_id)
{
	struct wlan_mlo_dev_context *mlo_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLO_MGR_ID);
	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_ctx = vdev->mlo_dev_ctx;

	if (!mlo_ctx) {
		mlo_err("null mlo_dev_ctx");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	sta_ctx = mlo_ctx->sta_ctx;

	if (!sta_ctx) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_INVAL;
	}

	mlo_dev_lock_acquire(mlo_ctx);
	*resp_cb = sta_ctx->ml_link_state.ml_link_state_resp_cb;
	*context = sta_ctx->ml_link_state.ml_link_state_req_context;

	mlo_dev_lock_release(mlo_ctx);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

void
wlan_mlo_send_vdev_pause(struct wlan_objmgr_psoc *psoc,
			 struct wlan_objmgr_vdev *vdev,
			 uint16_t session_id,
			 uint16_t vdev_pause_dur)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct mlo_vdev_pause vdev_pause_info;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	mlo_tx_ops = &psoc->soc_cb.tx_ops->mlo_ops;
	if (!mlo_tx_ops) {
		mlo_err("tx_ops is null!");
		return;
	}

	if (!mlo_tx_ops->send_vdev_pause) {
		mlo_err("send_vdev_pause is null");
		return;
	}

	vdev_pause_info.vdev_id = session_id;
	vdev_pause_info.vdev_pause_duration = vdev_pause_dur;
	status = mlo_tx_ops->send_vdev_pause(psoc, &vdev_pause_info);
	if (QDF_IS_STATUS_ERROR(status))
		mlo_err("Failed to send vdev pause to FW");
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
bool mlo_is_any_link_disconnecting(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS];
	uint16_t vdev_count = 0, i;
	bool status = false;

	if (!vdev)
		return status;

	mlo_sta_get_vdev_list(vdev, &vdev_count, wlan_vdev_list);
	for (i =  0; i < vdev_count; i++) {
		if (!status && wlan_cm_is_vdev_disconnecting(wlan_vdev_list[i]))
			status = true;
		mlo_release_vdev_ref(wlan_vdev_list[i]);
	}

	return status;
}

QDF_STATUS
mlo_set_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;

	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx)
		return QDF_STATUS_E_INVAL;

	mlo_dev_ctx->sta_ctx->ml_chan_switch_in_progress = val;
	mlo_debug("Set ml_chan_switch_in_progress: %d vdev %d",
		  val, wlan_vdev_get_id(vdev));

	return QDF_STATUS_SUCCESS;
}

bool mlo_is_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;

	if (!mlo_dev_ctx || !mlo_dev_ctx->sta_ctx)
		return false;

	return mlo_dev_ctx->sta_ctx->ml_chan_switch_in_progress;
}
#endif
#endif
