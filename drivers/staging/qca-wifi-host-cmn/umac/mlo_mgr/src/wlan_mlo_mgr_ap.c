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

/*
 * DOC: contains MLO manager ap related functionality
 */
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_mlo_mgr_ap.h"
#include <wlan_mlo_mgr_cmn.h>
#include <wlan_mlo_mgr_main.h>
#include <wlan_utility.h>
#ifdef WLAN_MLO_MULTI_CHIP
#include "cdp_txrx_mlo.h"
#endif
#include "wlan_mlo_mgr_peer.h"

#ifdef WLAN_MLO_MULTI_CHIP
bool mlo_ap_vdev_attach(struct wlan_objmgr_vdev *vdev,
			uint8_t link_id,
			uint16_t vdev_count)
{
	struct wlan_mlo_dev_context *dev_ctx;
	uint8_t pr_vdev_ids[WLAN_UMAC_MLO_MAX_VDEVS] = { CDP_INVALID_VDEV_ID };
	struct wlan_objmgr_psoc *psoc;
	int i;

	if (!vdev || !vdev->mlo_dev_ctx || !vdev->mlo_dev_ctx->ap_ctx) {
		mlo_err("Invalid input");
		return false;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return false;

	dev_ctx = vdev->mlo_dev_ctx;
	wlan_vdev_set_link_id(vdev, link_id);
	wlan_vdev_mlme_set_mlo_vdev(vdev);

	/*
	 * every link will trigger mlo_ap_vdev_attach,
	 * and they should provide the same vdev_count.
	 */
	mlo_dev_lock_acquire(dev_ctx);
	dev_ctx->ap_ctx->num_ml_vdevs = vdev_count;
	mlo_dev_lock_release(dev_ctx);

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (dev_ctx->wlan_vdev_list[i])
			pr_vdev_ids[i] = wlan_vdev_get_id(dev_ctx->wlan_vdev_list[i]);
	}

	if (cdp_update_mlo_ptnr_list(wlan_psoc_get_dp_handle(psoc),
				pr_vdev_ids, WLAN_UMAC_MLO_MAX_VDEVS,
				wlan_vdev_get_id(vdev)) != QDF_STATUS_SUCCESS) {
		mlo_debug("Failed to add vdev to partner vdev list, vdev id:%d",
			 wlan_vdev_get_id(vdev));
	}

	return true;
}
#else
bool mlo_ap_vdev_attach(struct wlan_objmgr_vdev *vdev,
			uint8_t link_id,
			uint16_t vdev_count)
{
	struct wlan_mlo_dev_context *dev_ctx;

	if (!vdev || !vdev->mlo_dev_ctx || !vdev->mlo_dev_ctx->ap_ctx) {
		mlo_err("Invalid input");
		return false;
	}

	dev_ctx = vdev->mlo_dev_ctx;
	wlan_vdev_set_link_id(vdev, link_id);
	wlan_vdev_mlme_set_mlo_vdev(vdev);

	/*
	 * every link will trigger mlo_ap_vdev_attach,
	 * and they should provide the same vdev_count.
	 */
	mlo_dev_lock_acquire(dev_ctx);
	dev_ctx->ap_ctx->num_ml_vdevs = vdev_count;
	mlo_dev_lock_release(dev_ctx);

	return true;
}
#endif

void mlo_ap_get_vdev_list(struct wlan_objmgr_vdev *vdev,
			  uint16_t *vdev_count,
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
		if (dev_ctx->wlan_vdev_list[i] &&
		    wlan_vdev_mlme_is_mlo_ap(dev_ctx->wlan_vdev_list[i])) {
			status = wlan_objmgr_vdev_try_get_ref(
						dev_ctx->wlan_vdev_list[i],
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

void mlo_ap_get_active_vdev_list(struct wlan_objmgr_vdev *vdev,
				 uint16_t *vdev_count,
				 struct wlan_objmgr_vdev **wlan_vdev_list)
{
	struct wlan_mlo_dev_context *dev_ctx;
	int i;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *partner_vdev = NULL;

	*vdev_count = 0;

	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Invalid input");
		return;
	}

	dev_ctx = vdev->mlo_dev_ctx;

	mlo_dev_lock_acquire(dev_ctx);
	*vdev_count = 0;
	for (i = 0; i < QDF_ARRAY_SIZE(dev_ctx->wlan_vdev_list); i++) {
		partner_vdev = dev_ctx->wlan_vdev_list[i];
		if (partner_vdev &&
		    wlan_vdev_mlme_is_mlo_ap(partner_vdev)) {
			if (wlan_vdev_chan_config_valid(partner_vdev) !=
						 QDF_STATUS_SUCCESS)
				continue;

			status = wlan_objmgr_vdev_try_get_ref(partner_vdev,
							      WLAN_MLO_MGR_ID);
			if (QDF_IS_STATUS_ERROR(status))
				break;
			wlan_vdev_list[*vdev_count] = partner_vdev;
			(*vdev_count) += 1;
		}
	}
	mlo_dev_lock_release(dev_ctx);
}

void mlo_ap_get_partner_vdev_list_from_mld(
		struct wlan_objmgr_vdev *vdev,
		uint16_t *vdev_count,
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
		if (dev_ctx->wlan_vdev_list[i] &&
		    (QDF_SAP_MODE ==
		     wlan_vdev_mlme_get_opmode(dev_ctx->wlan_vdev_list[i]))) {
			status = wlan_objmgr_vdev_try_get_ref(
						dev_ctx->wlan_vdev_list[i],
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

/**
 * mlo_ap_vdev_is_start_resp_rcvd() - Is start response received on this vdev
 * @vdev: vdev pointer
 *
 * Return: SUCCESS if start response is received, ERROR otherwise.
 */
static QDF_STATUS mlo_ap_vdev_is_start_resp_rcvd(struct wlan_objmgr_vdev *vdev)
{
	enum wlan_vdev_state state;

	if (!vdev) {
		mlme_err("vdev is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_vdev_mlme_is_mlo_ap(vdev))
		return QDF_STATUS_E_FAILURE;

	state = wlan_vdev_mlme_get_state(vdev);
	if ((state == WLAN_VDEV_S_UP) ||
	    (state == WLAN_VDEV_S_DFS_CAC_WAIT) ||
	    (state == WLAN_VDEV_S_SUSPEND))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

uint16_t wlan_mlo_ap_get_active_links(struct wlan_objmgr_vdev *vdev)
{
	uint16_t vdev_count = 0;
	struct wlan_mlo_dev_context *dev_ctx;
	int i;

	if (!vdev || !vdev->mlo_dev_ctx || !vdev->mlo_dev_ctx->ap_ctx) {
		mlo_err("Invalid input");
		return vdev_count;
	}

	dev_ctx = vdev->mlo_dev_ctx;

	mlo_dev_lock_acquire(dev_ctx);
	for (i = 0; i < QDF_ARRAY_SIZE(dev_ctx->wlan_vdev_list); i++) {
		if (dev_ctx->wlan_vdev_list[i] && QDF_IS_STATUS_SUCCESS(
		    mlo_ap_vdev_is_start_resp_rcvd(dev_ctx->wlan_vdev_list[i])))
			vdev_count++;
	}

	mlo_dev_lock_release(dev_ctx);

	return vdev_count;
}

/**
 * mlo_is_ap_vdev_up_allowed() - Is mlo ap allowed to come up
 * @vdev: vdev pointer
 *
 * Return: true if given ap is allowed to up, false otherwise.
 */
static bool mlo_is_ap_vdev_up_allowed(struct wlan_objmgr_vdev *vdev)
{
	uint16_t vdev_count = 0;
	bool up_allowed = false;
	struct wlan_mlo_dev_context *dev_ctx;

	if (!vdev) {
		mlo_err("Invalid input");
		return up_allowed;
	}

	dev_ctx = vdev->mlo_dev_ctx;

	vdev_count = wlan_mlo_ap_get_active_links(vdev);
	if (vdev_count == dev_ctx->ap_ctx->num_ml_vdevs)
		up_allowed = true;

	return up_allowed;
}

/**
 * mlo_pre_link_up() - Carry out preparation before bringing up the link
 * @vdev: vdev pointer
 *
 * Return: true if preparation is done successfully
 */
static bool mlo_pre_link_up(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev) {
		mlo_err("vdev is NULL");
		return false;
	}

	if ((wlan_vdev_mlme_get_state(vdev) == WLAN_VDEV_S_UP) &&
	    (wlan_vdev_mlme_get_substate(vdev) ==
	     WLAN_VDEV_SS_MLO_SYNC_WAIT))
		return true;

	return false;
}

/**
 * mlo_handle_link_ready() - Check if mlo ap is allowed to up or not.
 *                           If it is allowed, for every link in the
 *                           WLAN_VDEV_SS_MLO_SYNC_WAIT state, deliver
 *                           event WLAN_VDEV_SM_EV_MLO_SYNC_COMPLETE.
 *
 * This function is triggered once a link gets start response or enters
 * WLAN_VDEV_SS_MLO_SYNC_WAIT state
 *
 * @vdev: vdev pointer
 *
 * Return: true if MLO_SYNC_COMPLETE is posted, else false
 */
static bool mlo_handle_link_ready(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_vdev *vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {NULL};
	uint16_t num_links = 0;
	uint8_t i;

	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Invalid input");
		return false;
	}

	if (!mlo_is_ap_vdev_up_allowed(vdev))
		return false;

	mlo_ap_get_vdev_list(vdev, &num_links, vdev_list);
	if (!num_links || (num_links > QDF_ARRAY_SIZE(vdev_list))) {
		mlo_err("Invalid number of VDEVs under AP-MLD");
		return false;
	}

	mlo_ap_lock_acquire(vdev->mlo_dev_ctx->ap_ctx);
	for (i = 0; i < num_links; i++) {
		if (mlo_pre_link_up(vdev_list[i])) {
			if (vdev_list[i] != vdev)
				wlan_vdev_mlme_sm_deliver_evt(
					vdev_list[i],
					WLAN_VDEV_SM_EV_MLO_SYNC_COMPLETE,
					0, NULL);
		}
		/* Release ref taken as part of mlo_ap_get_vdev_list */
		mlo_release_vdev_ref(vdev_list[i]);
	}
	mlo_ap_lock_release(vdev->mlo_dev_ctx->ap_ctx);
	return true;
}

bool mlo_ap_link_sync_wait_notify(struct wlan_objmgr_vdev *vdev)
{
	return mlo_handle_link_ready(vdev);
}

void mlo_ap_link_start_rsp_notify(struct wlan_objmgr_vdev *vdev)
{
	mlo_handle_link_ready(vdev);
}

void mlo_ap_vdev_detach(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Invalid input");
		return;
	}
	wlan_vdev_mlme_clear_mlo_vdev(vdev);
}

void mlo_ap_link_down_cmpl_notify(struct wlan_objmgr_vdev *vdev)
{
	mlo_ap_vdev_detach(vdev);
}

uint16_t mlo_ap_ml_peerid_alloc(void)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint16_t i;
	uint16_t mlo_peer_id;

	ml_peerid_lock_acquire(mlo_ctx);
	mlo_peer_id = mlo_ctx->last_mlo_peer_id;
	for (i = 0; i < mlo_ctx->max_mlo_peer_id; i++) {
		mlo_peer_id = (mlo_peer_id + 1) % mlo_ctx->max_mlo_peer_id;

		if (!mlo_peer_id)
			continue;

		if (qdf_test_bit(mlo_peer_id, mlo_ctx->mlo_peer_id_bmap))
			continue;

		qdf_set_bit(mlo_peer_id, mlo_ctx->mlo_peer_id_bmap);
		break;
	}
	mlo_ctx->last_mlo_peer_id = mlo_peer_id;
	ml_peerid_lock_release(mlo_ctx);

	if (i == mlo_ctx->max_mlo_peer_id)
		return MLO_INVALID_PEER_ID;

	mlo_debug(" ML peer id %d is allocated", mlo_peer_id);

	return mlo_peer_id;
}

void mlo_ap_ml_peerid_free(uint16_t mlo_peer_id)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();

	if ((mlo_peer_id == 0) || (mlo_peer_id == MLO_INVALID_PEER_ID)) {
		mlo_err(" ML peer id %d is invalid", mlo_peer_id);
		return;
	}

	if (mlo_peer_id > mlo_ctx->max_mlo_peer_id) {
		mlo_err(" ML peer id %d is invalid", mlo_peer_id);
		QDF_BUG(0);
		return;
	}

	ml_peerid_lock_acquire(mlo_ctx);
	if (qdf_test_bit(mlo_peer_id, mlo_ctx->mlo_peer_id_bmap))
		qdf_clear_bit(mlo_peer_id, mlo_ctx->mlo_peer_id_bmap);

	ml_peerid_lock_release(mlo_ctx);

	mlo_debug(" ML peer id %d is freed", mlo_peer_id);
}

void mlo_ap_vdev_quiet_set(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mld_ctx = vdev->mlo_dev_ctx;
	uint8_t idx;

	if (!mld_ctx || !wlan_vdev_mlme_is_mlo_ap(vdev))
		return;

	idx = mlo_get_link_vdev_ix(mld_ctx, vdev);
	if (idx == MLO_INVALID_LINK_IDX)
		return;

	mlo_debug("Quiet set for PSOC:%d vdev:%d",
		  wlan_psoc_get_id(wlan_vdev_get_psoc(vdev)),
		  wlan_vdev_get_id(vdev));

	wlan_util_change_map_index(mld_ctx->ap_ctx->mlo_vdev_quiet_bmap,
				   idx, 1);
}

void mlo_ap_vdev_quiet_clear(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mld_ctx = vdev->mlo_dev_ctx;
	uint8_t idx;

	if (!mld_ctx || !wlan_vdev_mlme_is_mlo_ap(vdev))
		return;

	idx = mlo_get_link_vdev_ix(mld_ctx, vdev);
	if (idx == MLO_INVALID_LINK_IDX)
		return;

	mlo_debug("Quiet clear for PSOC:%d vdev:%d",
		  wlan_psoc_get_id(wlan_vdev_get_psoc(vdev)),
		  wlan_vdev_get_id(vdev));

	wlan_util_change_map_index(mld_ctx->ap_ctx->mlo_vdev_quiet_bmap,
				   idx, 0);
}

bool mlo_ap_vdev_quiet_is_any_idx_set(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mld_ctx = vdev->mlo_dev_ctx;

	if (!mld_ctx || !wlan_vdev_mlme_is_mlo_ap(vdev))
		return false;

	return wlan_util_map_is_any_index_set(
			mld_ctx->ap_ctx->mlo_vdev_quiet_bmap,
			sizeof(mld_ctx->ap_ctx->mlo_vdev_quiet_bmap));
}

QDF_STATUS
mlo_peer_create_get_frm_buf(
		struct wlan_mlo_peer_context *ml_peer,
		struct peer_create_notif_s *peer_create,
		qdf_nbuf_t frm_buf)
{
	if (wlan_mlo_peer_is_nawds(ml_peer) ||
	    wlan_mlo_peer_is_mesh(ml_peer)) {
		peer_create->frm_buf = NULL;
		return QDF_STATUS_SUCCESS;
	}

	if (!frm_buf)
		return QDF_STATUS_E_FAILURE;

	peer_create->frm_buf = qdf_nbuf_clone(frm_buf);
	if (!peer_create->frm_buf)
		return QDF_STATUS_E_NOMEM;

	return QDF_STATUS_SUCCESS;
}

#ifdef UMAC_SUPPORT_MLNAWDS
void mlo_peer_populate_nawds_params(
		struct wlan_mlo_peer_context *ml_peer,
		struct mlo_partner_info *ml_info)
{
	uint8_t i;
	uint8_t null_mac[QDF_MAC_ADDR_SIZE] = {0x00, 0x00, 0x00,
					       0x00, 0x00, 0x00};
	struct mlnawds_config nawds_config;

	mlo_peer_lock_acquire(ml_peer);
	ml_peer->is_nawds_ml_peer = false;
	for (i = 0; i < ml_info->num_partner_links; i++) {
		nawds_config = ml_info->partner_link_info[i].nawds_config;
		/*
		 * if ml_info->partner_link_info[i].nawds_config has valid
		 * config(check for non-null mac or non-0 caps), then mark
		 * ml_peer's is_nawds_ml_peer true & copy the config
		 */
		if ((nawds_config.caps) ||
		    (qdf_mem_cmp(null_mac,
				 nawds_config.mac,
				 sizeof(null_mac)))) {
			ml_peer->is_nawds_ml_peer = true;
			ml_peer->nawds_config[i] = nawds_config;
		}
	}
	mlo_peer_lock_release(ml_peer);
}
#endif

#ifdef MESH_MODE_SUPPORT
void mlo_peer_populate_mesh_params(
		struct wlan_mlo_peer_context *ml_peer,
		struct mlo_partner_info *ml_info)
{
	uint8_t i;
	uint8_t null_mac[QDF_MAC_ADDR_SIZE] = {0};
	struct mlnawds_config mesh_config;

	mlo_peer_lock_acquire(ml_peer);
	ml_peer->is_mesh_ml_peer = false;
	for (i = 0; i < ml_info->num_partner_links; i++) {
		mesh_config = ml_info->partner_link_info[i].mesh_config;
		/*
		 * if ml_info->partner_link_info[i].mesh_config has valid
		 * config(check for non-null mac or non-0 caps), then mark
		 * ml_peer's is_mesh_ml_peer true & copy the config
		 */
		if ((mesh_config.caps) ||
		    (qdf_mem_cmp(null_mac,
				 mesh_config.mac,
				 sizeof(null_mac)))) {
			ml_peer->is_mesh_ml_peer = true;
			ml_peer->mesh_config[i] = mesh_config;
		}
	}
	mlo_peer_lock_release(ml_peer);
}
#endif
