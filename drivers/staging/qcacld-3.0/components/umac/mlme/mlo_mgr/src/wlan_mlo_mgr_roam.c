/*
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
 * DOC: contains MLO manager roaming related functionality
 */
#include <wlan_cmn.h>
#include <wlan_cm_public_struct.h>
#include <wlan_cm_roam_public_struct.h>
#include "wlan_mlo_mgr_cmn.h"
#include "wlan_mlo_mgr_main.h"
#include "wlan_mlo_mgr_roam.h"
#include "wlan_mlo_mgr_public_structs.h"
#include "wlan_mlo_mgr_sta.h"
#include <../../core/src/wlan_cm_roam_i.h>
#include "wlan_cm_roam_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include <include/wlan_mlme_cmn.h>
#include <wlan_cm_api.h>
#include <utils_mlo.h>
#include <wlan_mlo_mgr_peer.h>
#include "wlan_mlo_link_force.h"

#ifdef WLAN_FEATURE_11BE_MLO
static bool
mlo_check_connect_req_bmap(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;
	uint8_t i = 0;

	if (!mlo_dev_ctx)
		return false;

	sta_ctx = mlo_dev_ctx->sta_ctx;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (vdev == mlo_dev_ctx->wlan_vdev_list[i])
			return qdf_test_bit(i, sta_ctx->wlan_connect_req_links);
	}

	mlo_err("vdev:%d not found in ml dev ctx list", wlan_vdev_get_id(vdev));

	return false;
}

static void
mlo_update_for_multi_link_roam(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id,
			       uint8_t ml_link_vdev_id)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    ml_link_vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev) {
		mlo_err("VDEV is null");
		return;
	}

	if (vdev_id == ml_link_vdev_id) {
		wlan_vdev_mlme_set_mlo_vdev(vdev);
		goto end;
	}

	wlan_vdev_mlme_set_mlo_vdev(vdev);
	wlan_vdev_mlme_set_mlo_link_vdev(vdev);

	mlo_update_connect_req_links(vdev, true);

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

static void
mlo_cleanup_link(struct wlan_objmgr_vdev *vdev, uint8_t num_setup_links)
{
	/*
	 * Cleanup the non-assoc link link in below cases,
	 * 1. Roamed to single link-MLO AP
	 * 2. Roamed to an MLO AP but 4-way handshake is offloaded to
	 *    userspace, i.e.auth_status = ROAM_AUTH_STATUS_CONNECTED
	 * 3. Roamed to non-MLO AP(num_setup_links = 0)
	 * This covers all supported combinations. So cleanup the link always.
	 */
	if (wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		cm_cleanup_mlo_link(vdev);
	/*
	 * Clear the MLO vdev flag when roam to a non-MLO AP to prepare the
	 * roam done indication to userspace in non-MLO format
	 * i.e. without MLD/link info
	 */
	else if (wlan_vdev_mlme_is_mlo_vdev(vdev) && !num_setup_links)
		wlan_vdev_mlme_clear_mlo_vdev(vdev);
}

static void
mlo_update_vdev_after_roam(struct wlan_objmgr_psoc *psoc,
			   uint8_t vdev_id, uint8_t num_setup_links)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	uint8_t i;
	struct wlan_objmgr_vdev *vdev, *tmp_vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev) {
		mlo_err("VDEV:%d is null", vdev_id);
		return;
	}

	if (!vdev->mlo_dev_ctx)
		goto end;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		tmp_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		mlo_cleanup_link(tmp_vdev, num_setup_links);
	}

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

static void
mlo_clear_link_bmap(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev) {
		mlo_err("VDEV is null");
		return;
	}

	mlo_clear_connect_req_links_bmap(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

static QDF_STATUS
mlo_roam_abort_req(struct wlan_objmgr_psoc *psoc,
		   uint8_t *event, uint8_t vdev_id)
{
	struct roam_offload_synch_ind *sync_ind = NULL;

	sync_ind = (struct roam_offload_synch_ind *)event;

	if (!sync_ind) {
		mlme_err("Roam Sync ind ptr is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_mlo_roam_abort_on_link(psoc, event, sync_ind->roamed_vdev_id);

	return QDF_STATUS_SUCCESS;
}
#else
static inline void
mlo_clear_link_bmap(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{}

static inline void
mlo_update_vdev_after_roam(struct wlan_objmgr_psoc *psoc,
			   uint8_t vdev_id, uint8_t num_setup_links)
{}

static inline void
mlo_cleanup_link(struct wlan_objmgr_vdev *vdev, uint8_t num_setup_links)
{}

static inline void
mlo_update_for_multi_link_roam(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id,
			       uint8_t ml_link_vdev_id)
{}

static inline bool
mlo_check_connect_req_bmap(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline QDF_STATUS
mlo_roam_abort_req(struct wlan_objmgr_psoc *psoc,
		   uint8_t *event, uint8_t vdev_id)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

static void mlo_roam_update_vdev_macaddr(struct wlan_objmgr_psoc *psoc,
					 struct roam_offload_synch_ind *sync_ind,
					 uint8_t vdev_id,
					 bool is_non_ml_connection)
{
	struct wlan_objmgr_vdev *vdev;
	struct qdf_mac_addr *mld_mac;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_MLO_MGR_ID);
	if (!vdev) {
		mlo_err("VDEV is null");
		return;
	}

	if (is_non_ml_connection) {
		mld_mac = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(vdev);
		if (!qdf_is_macaddr_zero(mld_mac))
			wlan_vdev_mlme_set_macaddr(vdev, mld_mac->bytes);
	} else {
		struct qdf_mac_addr *vdev_link_addr;
		uint8_t i;

		wlan_vdev_mlme_set_macaddr(vdev,
					   wlan_vdev_mlme_get_linkaddr(vdev));
		/* Update the link address received from fw to assoc vdev */
		for (i = 0; i < sync_ind->num_setup_links; i++) {
			if (vdev_id != sync_ind->ml_link[i].vdev_id)
				continue;

			vdev_link_addr = &sync_ind->ml_link[i].self_link_addr;
			if (qdf_is_macaddr_zero(vdev_link_addr) ||
			    qdf_is_macaddr_broadcast(vdev_link_addr))
				continue;

			wlan_vdev_mlme_set_macaddr(vdev, vdev_link_addr->bytes);
			wlan_vdev_mlme_set_linkaddr(vdev,
						    vdev_link_addr->bytes);
		}
	}

	mlme_debug("vdev_id %d self mac " QDF_MAC_ADDR_FMT,
		   vdev_id,
		   QDF_MAC_ADDR_REF(wlan_vdev_mlme_get_macaddr(vdev)));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
}

QDF_STATUS mlo_fw_roam_sync_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				void *event, uint32_t event_data_len)
{
	struct roam_offload_synch_ind *sync_ind;
	QDF_STATUS status;
	uint8_t i;
	bool is_non_mlo_ap = false;

	sync_ind = (struct roam_offload_synch_ind *)event;
	if (!sync_ind)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < sync_ind->num_setup_links; i++)
		mlo_update_for_multi_link_roam(psoc, vdev_id,
					       sync_ind->ml_link[i].vdev_id);

	if (!sync_ind->num_setup_links) {
		mlo_debug("MLO_ROAM: Roamed to Legacy");
		is_non_mlo_ap = true;
		mlo_set_single_link_ml_roaming(psoc, vdev_id, false);
	} else if (sync_ind->num_setup_links == 1 ||
		sync_ind->auth_status == ROAM_AUTH_STATUS_CONNECTED) {
		mlo_debug("MLO_ROAM: Roamed to single link MLO");
		mlo_set_single_link_ml_roaming(psoc, vdev_id, true);
	} else {
		mlo_debug("MLO_ROAM: Roamed to MLO with %d links",
			  sync_ind->num_setup_links);
		mlo_set_single_link_ml_roaming(psoc, vdev_id, false);
	}

	mlo_roam_update_vdev_macaddr(psoc, sync_ind, vdev_id, is_non_mlo_ap);
	ml_nlink_conn_change_notify(
		psoc, vdev_id, ml_nlink_roam_sync_start_evt, NULL);

	status = cm_fw_roam_sync_req(psoc, vdev_id, event, event_data_len);

	if (QDF_IS_STATUS_ERROR(status))
		mlo_clear_link_bmap(psoc, vdev_id);

	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static struct mlo_link_info *
mlo_mgr_get_link_info_by_self_addr(struct wlan_objmgr_vdev *vdev,
				   struct qdf_mac_addr *self_addr)
{
	uint8_t iter;
	struct mlo_link_info *mlo_link;

	if (!vdev || !vdev->mlo_dev_ctx || !vdev->mlo_dev_ctx->link_ctx ||
	    !self_addr || qdf_is_macaddr_zero(self_addr))
		return NULL;

	for (iter = 0; iter < WLAN_MAX_ML_BSS_LINKS; iter++) {
		mlo_link = &vdev->mlo_dev_ctx->link_ctx->links_info[iter];

		if (qdf_is_macaddr_equal(&mlo_link->link_addr, self_addr))
			return mlo_link;
	}

	return NULL;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
uint8_t mlo_mgr_num_roam_links(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_cm_connect_resp *reassoc_rsp;

	if (!vdev->mlo_dev_ctx)
		return 1;

	if (!vdev->mlo_dev_ctx->sta_ctx)
		return 0;

	reassoc_rsp = vdev->mlo_dev_ctx->sta_ctx->copied_reassoc_rsp;
	if (!reassoc_rsp || !reassoc_rsp->roaming_info)
		return 0;

	return reassoc_rsp->roaming_info->num_setup_links;
}
#endif

void mlo_mgr_roam_update_ap_link_info(struct wlan_objmgr_vdev *vdev,
				      struct ml_setup_link_param *src_link_info,
				      struct wlan_channel *channel)
{
	struct mlo_link_info *link_info;

	if (!src_link_info)
		return;

	link_info = mlo_mgr_get_link_info_by_self_addr(vdev,
						       &src_link_info->self_link_addr);
	if (!link_info) {
		mlo_err("No link info found for vdev %d with " QDF_MAC_ADDR_FMT,
			src_link_info->vdev_id,
			QDF_MAC_ADDR_REF(src_link_info->self_link_addr.bytes));
		QDF_BUG(0);
		return;
	}

	if (link_info->vdev_id != src_link_info->vdev_id) {
		mlo_err("Host(%d)-FW(%d) VDEV-MAC addr mismatch",
			link_info->vdev_id, src_link_info->vdev_id);
		QDF_BUG(0);
		return;
	}

	link_info->link_id = src_link_info->link_id;
	qdf_copy_macaddr(&link_info->ap_link_addr, &src_link_info->link_addr);
	qdf_mem_copy(link_info->link_chan_info, channel, sizeof(*channel));

	mlo_debug("link_id: %d, vdev_id:%d freq:%d ap_link_addr: "QDF_MAC_ADDR_FMT", self_link_addr: "QDF_MAC_ADDR_FMT,
		  link_info->link_id, link_info->vdev_id,
		  link_info->link_chan_info->ch_freq,
		  QDF_MAC_ADDR_REF(link_info->ap_link_addr.bytes),
		  QDF_MAC_ADDR_REF(link_info->link_addr.bytes));
}

QDF_STATUS mlo_cm_roam_sync_cb(struct wlan_objmgr_vdev *vdev,
			       void *event, uint32_t event_data_len)
{
	QDF_STATUS status;
	struct roam_offload_synch_ind *sync_ind;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *link_vdev = NULL;
	uint8_t i;
	uint8_t vdev_id;

	sync_ind = (struct roam_offload_synch_ind *)event;
	vdev_id = wlan_vdev_get_id(vdev);
	psoc = wlan_vdev_get_psoc(vdev);

	/* Clean up link vdev in following cases
	 * 1. When roamed to legacy, num_setup_links = 0
	 * 2. When roamed to single link, num_setup_links = 1
	 * 3. Roamed to AP with auth_status = ROAMED_AUTH_STATUS_CONNECTED
	 */
	if (sync_ind->num_setup_links < 2 ||
	    sync_ind->auth_status == ROAM_AUTH_STATUS_CONNECTED) {
		mlme_debug("Roam auth status %d", sync_ind->auth_status);
		mlo_update_vdev_after_roam(psoc, vdev_id,
					   sync_ind->num_setup_links);
	}

	/* If EAPOL is offloaded to supplicant, link vdev/s are not up
	 * at FW, in that case complete roam sync on assoc vdev
	 * link vdev will be initialized after set key is complete.
	 */
	if (sync_ind->auth_status == ROAM_AUTH_STATUS_CONNECTED)
		return QDF_STATUS_SUCCESS;

	for (i = 0; i < sync_ind->num_setup_links; i++) {
		if (vdev_id == sync_ind->ml_link[i].vdev_id)
			continue;

		/* Standby Link */
		if (sync_ind->ml_link[i].vdev_id == WLAN_INVALID_VDEV_ID)
			continue;

		link_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
								 sync_ind->ml_link[i].vdev_id,
								 WLAN_MLME_SB_ID);
		if (!link_vdev) {
			mlo_err("Link vdev:%d is null",
				sync_ind->ml_link[i].vdev_id);
			return QDF_STATUS_E_FAILURE;
		}

		if (mlo_check_connect_req_bmap(link_vdev)) {
			struct qdf_mac_addr *vdev_link_addr;

			mlo_update_connect_req_links(link_vdev, false);

			vdev_link_addr = &sync_ind->ml_link[i].self_link_addr;
			if (!qdf_is_macaddr_zero(vdev_link_addr) &&
			    !qdf_is_macaddr_broadcast(vdev_link_addr)) {
				wlan_vdev_mlme_set_macaddr(link_vdev,
							   vdev_link_addr->bytes);
				wlan_vdev_mlme_set_linkaddr(link_vdev,
							    vdev_link_addr->bytes);
			}

			status = cm_fw_roam_sync_req(psoc,
						     sync_ind->ml_link[i].vdev_id,
						     event, event_data_len);
			if (QDF_IS_STATUS_ERROR(status)) {
				mlo_clear_connect_req_links_bmap(link_vdev);
				mlo_roam_abort_req(psoc, event,
						   sync_ind->ml_link[i].vdev_id);
				wlan_objmgr_vdev_release_ref(link_vdev,
							     WLAN_MLME_SB_ID);
				return QDF_STATUS_E_FAILURE;
			}
		}
		wlan_objmgr_vdev_release_ref(link_vdev,
					     WLAN_MLME_SB_ID);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

void
mlo_fw_ho_fail_req(struct wlan_objmgr_psoc *psoc,
		   uint8_t vdev_id, struct qdf_mac_addr bssid)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	uint8_t i;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_MLME_SB_ID);

	if (!vdev) {
		mlo_err("vdev is null");
		return;
	}

	if (!vdev->mlo_dev_ctx)
		goto end;

	mlo_dev_ctx = vdev->mlo_dev_ctx;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i] ||
		    mlo_dev_ctx->wlan_vdev_list[i] == vdev)
			continue;
		cm_fw_ho_fail_req(psoc,
				  wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]),
				  bssid);
	}

end:
	cm_fw_ho_fail_req(psoc, vdev_id, bssid);
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_MLME_SB_ID);
}

QDF_STATUS
mlo_get_sta_link_mac_addr(uint8_t vdev_id,
			  struct roam_offload_synch_ind *sync_ind,
			  struct qdf_mac_addr *link_mac_addr)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i;

	if (!sync_ind || !sync_ind->num_setup_links)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < sync_ind->num_setup_links; i++) {
		if (sync_ind->ml_link[i].vdev_id == vdev_id) {
			qdf_copy_macaddr(link_mac_addr,
					 &sync_ind->ml_link[i].link_addr);
			return status;
		}
	}

	if (i == sync_ind->num_setup_links) {
		mlo_err("Link mac addr not found");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

uint32_t
mlo_roam_get_chan_freq(uint8_t vdev_id,
		       struct roam_offload_synch_ind *sync_ind)
{
	uint8_t i;

	if (!sync_ind || !sync_ind->num_setup_links)
		return 0;

	for (i = 0; i < sync_ind->num_setup_links; i++) {
		if (sync_ind->ml_link[i].vdev_id == vdev_id)
			return sync_ind->ml_link[i].channel.mhz;
	}

	return 0;
}

uint32_t
mlo_roam_get_link_id(uint8_t vdev_id,
		     struct roam_offload_synch_ind *sync_ind)
{
	uint8_t i;

	if (!sync_ind || !sync_ind->num_setup_links)
		return 0;

	for (i = 0; i < sync_ind->num_setup_links; i++) {
		if (sync_ind->ml_link[i].vdev_id == vdev_id)
			return sync_ind->ml_link[i].link_id;
	}

	return 0;
}

bool is_multi_link_roam(struct roam_offload_synch_ind *sync_ind)
{
	if (!sync_ind)
		return false;

	if (sync_ind->num_setup_links)
		return true;

	return false;
}

uint8_t
mlo_roam_get_num_of_setup_links(struct roam_offload_synch_ind *sync_ind)
{
	if (!sync_ind) {
		mlo_err("Roam Sync ind is null");
		return WLAN_INVALID_VDEV_ID;
	}

	return sync_ind->num_setup_links;
}

uint32_t
mlo_roam_get_link_freq_from_mac_addr(struct roam_offload_synch_ind *sync_ind,
				     uint8_t *link_mac_addr)
{
	uint8_t i;

	if (!sync_ind)
		return 0;

	/* Non-MLO roaming */
	if (!sync_ind->num_setup_links)
		return sync_ind->chan_freq;

	if (!link_mac_addr) {
		mlo_debug("link_mac_addr is NULL");
		return 0;
	}

	for (i = 0; i < sync_ind->num_setup_links; i++)
		if (!qdf_mem_cmp(sync_ind->ml_link[i].link_addr.bytes,
				 link_mac_addr,
				 QDF_MAC_ADDR_SIZE))
			return sync_ind->ml_link[i].channel.mhz;

	mlo_debug("Mac address not found in ml_link info" QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(link_mac_addr));

	return 0;
}

QDF_STATUS
mlo_roam_get_link_id_from_mac_addr(struct roam_offload_synch_ind *sync_ind,
				   uint8_t *link_mac_addr, uint32_t *link_id)
{
	uint8_t i;

	if (!sync_ind || !sync_ind->num_setup_links || !link_mac_addr)
		return QDF_STATUS_E_INVAL;

	for (i = 0; i < sync_ind->num_setup_links; i++)
		if (!qdf_mem_cmp(sync_ind->ml_link[i].link_addr.bytes,
				 link_mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
			*link_id = sync_ind->ml_link[i].link_id;
			return QDF_STATUS_SUCCESS;
		}

	return QDF_STATUS_E_INVAL;
}

QDF_STATUS mlo_enable_rso(struct wlan_objmgr_pdev *pdev,
			  struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *rsp)
{
	struct wlan_objmgr_vdev *assoc_vdev;
	uint8_t num_partner_links;

	if (!rsp) {
		mlo_err("Connect resp is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	num_partner_links = rsp->ml_parnter_info.num_partner_links;

	if (num_partner_links &&
	    (!wlan_vdev_mlme_is_mlo_link_vdev(vdev) ||
	     !mlo_check_if_all_links_up(vdev)))
		return QDF_STATUS_SUCCESS;

	assoc_vdev = wlan_mlo_get_assoc_link_vdev(vdev);
	if (!assoc_vdev) {
		mlo_err("Assoc vdev is null");
		return QDF_STATUS_E_NULL_VALUE;
	}
	cm_roam_start_init_on_connect(pdev, wlan_vdev_get_id(assoc_vdev));

	return QDF_STATUS_SUCCESS;
}

void
mlo_roam_copy_partner_info(struct mlo_partner_info *partner_info,
			   struct roam_offload_synch_ind *sync_ind,
			   uint8_t skip_vdev_id, bool fill_all_links)
{
	uint8_t i, j;
	struct mlo_link_info *link;

	if (!sync_ind)
		return;

	for (i = 0, j = 0; i < sync_ind->num_setup_links; i++) {
		if (!fill_all_links &&
		    sync_ind->ml_link[i].vdev_id == skip_vdev_id)
			continue;

		link = &partner_info->partner_link_info[j];
		link->link_id = sync_ind->ml_link[i].link_id;
		link->vdev_id = sync_ind->ml_link[i].vdev_id;

		qdf_copy_macaddr(&link->link_addr,
				 &sync_ind->ml_link[i].link_addr);
		link->chan_freq = sync_ind->ml_link[i].channel.mhz;
		mlo_debug("vdev_id %d link_id %d freq %d bssid" QDF_MAC_ADDR_FMT,
			  link->vdev_id, link->link_id, link->chan_freq,
			  QDF_MAC_ADDR_REF(link->link_addr.bytes));
		j++;
	}
	partner_info->num_partner_links = j;
	mlo_debug("vdev_to_skip:%d num_setup_links %d fill_all_links:%d",
		  skip_vdev_id, partner_info->num_partner_links,
		  fill_all_links);
}

void mlo_roam_init_cu_bpcc(struct wlan_objmgr_vdev *vdev,
			   struct roam_offload_synch_ind *sync_ind)
{
	uint8_t i;
	struct wlan_mlo_dev_context *mlo_dev_ctx;

	if (!vdev) {
		mlo_err("vdev is NULL");
		return;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		mlo_err("ML dev ctx is NULL");
		return;
	}

	mlo_clear_cu_bpcc(vdev);
	for (i = 0; i < sync_ind->num_setup_links; i++)
		mlo_init_cu_bpcc(mlo_dev_ctx, sync_ind->ml_link[i].vdev_id);

	mlo_debug("update cu info from roam sync");
}

void
mlo_roam_update_connected_links(struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_connect_resp *connect_rsp)
{
	mlo_clear_connected_links_bmap(vdev);
	if (mlo_get_single_link_ml_roaming(wlan_vdev_get_psoc(vdev),
					   wlan_vdev_get_id(vdev)))
		mlo_update_connected_links(vdev, 1);
	else
		mlo_update_connected_links_bmap(vdev->mlo_dev_ctx,
						connect_rsp->ml_parnter_info);
}

QDF_STATUS
wlan_mlo_roam_abort_on_link(struct wlan_objmgr_psoc *psoc,
			    uint8_t *event, uint8_t vdev_id)
{
	uint8_t i;
	QDF_STATUS status;
	struct roam_offload_synch_ind *sync_ind = NULL;

	sync_ind = (struct roam_offload_synch_ind *)event;

	if (!sync_ind) {
		mlo_err("Roam Sync ind ptr is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	for (i = 0; i < sync_ind->num_setup_links; i++) {
		if (sync_ind->ml_link[i].vdev_id != vdev_id) {
			status = cm_fw_roam_abort_req(psoc,
						      sync_ind->ml_link[i].vdev_id);
			if (QDF_IS_STATUS_ERROR(status)) {
				mlo_err("LFR3: Fail to abort roam on vdev: %u",
					sync_ind->ml_link[i].vdev_id);
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

void
mlo_set_single_link_ml_roaming(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id,
			       bool is_single_link_ml_roaming)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev) {
		mlo_err("VDEV is null");
		return;
	}

	if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		mlme_set_single_link_mlo_roaming(vdev,
						 is_single_link_ml_roaming);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

bool
mlo_get_single_link_ml_roaming(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id)
{
	bool is_single_link_ml_roaming = false;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev) {
		mlo_err("VDEV is null");
		return is_single_link_ml_roaming;
	}

	is_single_link_ml_roaming = mlme_get_single_link_mlo_roaming(vdev);
	mlo_debug("MLO:is_single_link_ml_roaming %d",
		  is_single_link_ml_roaming);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

	return is_single_link_ml_roaming;
}

QDF_STATUS
mlo_roam_get_bssid_chan_for_link(uint8_t vdev_id,
				 struct roam_offload_synch_ind *sync_ind,
				 struct qdf_mac_addr *bssid,
				 wmi_channel *chan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i;

	if (!sync_ind || !sync_ind->num_setup_links)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < sync_ind->num_setup_links; i++) {
		if (vdev_id == sync_ind->ml_link[i].vdev_id) {
			qdf_mem_copy(chan, &sync_ind->ml_link[i].channel,
				     sizeof(wmi_channel));
			qdf_copy_macaddr(bssid,
					 &sync_ind->ml_link[i].link_addr);
			return status;
		}
	}

	if (i == sync_ind->num_setup_links) {
		mlo_err("roam sync info not found for vdev id %d", vdev_id);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

bool
mlo_check_if_all_links_up(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;
	uint8_t i;

	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Vdev is null");
		return false;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo sta ctx is null");
		return false;
	}

	sta_ctx = mlo_dev_ctx->sta_ctx;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (qdf_test_bit(i, sta_ctx->wlan_connected_links) &&
		    !wlan_cm_is_vdev_connected(mlo_dev_ctx->wlan_vdev_list[i])) {
			mlo_debug("Vdev id %d is not in connected state",
				  wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]));
			return false;
		}
	}

	if (i == WLAN_UMAC_MLO_MAX_VDEVS) {
		mlo_debug("all links are up");
		return true;
	}

	return false;
}

bool
mlo_check_if_all_vdev_up(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;
	uint8_t i;

	if (!vdev || !vdev->mlo_dev_ctx) {
		mlo_err("Vdev is null");
		return false;
	}

	if (QDF_IS_STATUS_ERROR(wlan_vdev_is_up(vdev))) {
		mlo_debug("Vdev id %d is not in up state",
			  wlan_vdev_get_id(vdev));
			return false;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx->sta_ctx) {
		mlo_err("mlo sta ctx is null");
		return false;
	}
	sta_ctx = mlo_dev_ctx->sta_ctx;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if ((qdf_test_bit(i, sta_ctx->wlan_connected_links) ||
		     qdf_test_bit(i, sta_ctx->wlan_connect_req_links)) &&
		    !QDF_IS_STATUS_SUCCESS(wlan_vdev_is_up(mlo_dev_ctx->wlan_vdev_list[i]))) {
			mlo_debug("Vdev id %d is not in up state",
				  wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]));
			return false;
		}
	}

	if (i == WLAN_UMAC_MLO_MAX_VDEVS) {
		mlo_debug("all links are up");
		return true;
	}

	return false;
}

void
mlo_roam_set_link_id(struct wlan_objmgr_vdev *vdev,
		     struct roam_offload_synch_ind *sync_ind)
{
	uint8_t i;
	uint8_t j;
	struct wlan_mlo_dev_context *mlo_dev_ctx;

	if (!vdev || !sync_ind || !vdev->mlo_dev_ctx) {
		mlo_debug("Invalid input");
		return;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!vdev)
			continue;

		wlan_vdev_set_link_id(vdev, WLAN_LINK_ID_INVALID);
		for (j = 0; j < sync_ind->num_setup_links; j++) {
			if (sync_ind->ml_link[j].vdev_id ==
			    wlan_vdev_get_id(vdev)) {
				wlan_vdev_set_link_id(
					vdev, sync_ind->ml_link[j].link_id);
				mlme_debug("Set link for vdev id %d link id %d",
					   wlan_vdev_get_id(vdev),
					   sync_ind->ml_link[j].link_id);
			}
		}
	}
}

QDF_STATUS
mlo_get_link_mac_addr_from_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
				       struct qdf_mac_addr *link_mac_addr)
{
	uint8_t i;
	struct wlan_mlo_sta *sta_ctx;
	struct wlan_cm_connect_resp *rsp;
	struct mlo_partner_info parnter_info;
	uint8_t vdev_id;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	vdev_id = wlan_vdev_get_id(vdev);

	if (!vdev->mlo_dev_ctx) {
		mlo_err("mlo dev ctx is null, vdev id %d", vdev_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!sta_ctx || !sta_ctx->copied_reassoc_rsp ||
	    !sta_ctx->copied_reassoc_rsp->roaming_info) {
		mlo_debug("sta ctx or copied reassoc rsp is null for vdev id %d", vdev_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	rsp = sta_ctx->copied_reassoc_rsp;
	if (rsp->roaming_info->auth_status != ROAM_AUTH_STATUS_CONNECTED) {
		mlo_debug("Roam auth status is not connected");
		return QDF_STATUS_E_FAILURE;
	}

	parnter_info = rsp->ml_parnter_info;
	for (i = 0; i < parnter_info.num_partner_links; i++) {
		if (parnter_info.partner_link_info[i].vdev_id == vdev_id) {
			qdf_copy_macaddr(link_mac_addr,
					 &parnter_info.partner_link_info[i].link_addr);
			return QDF_STATUS_SUCCESS;
		}
	}

	if (i == parnter_info.num_partner_links) {
		mlo_debug("Link mac addr not found");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
mlo_roam_copy_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *reassoc_rsp)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;
	struct wlan_connect_rsp_ies *connect_ies;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	if (!reassoc_rsp)
		return QDF_STATUS_E_NULL_VALUE;

	/* Store reassoc rsp only if roamed to 2 link AP */
	if (reassoc_rsp->ml_parnter_info.num_partner_links < 2)
		return QDF_STATUS_E_INVAL;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	wlan_cm_free_connect_resp(sta_ctx->copied_reassoc_rsp);
	/* Free assoc rsp, so that reassoc rsp can be used during
	 * reassociation.
	 */
	if (sta_ctx->assoc_rsp.ptr) {
		qdf_mem_free(sta_ctx->assoc_rsp.ptr);
		sta_ctx->assoc_rsp.ptr = NULL;
		sta_ctx->assoc_rsp.len = 0;
	}

	sta_ctx->ml_partner_info = reassoc_rsp->ml_parnter_info;

	sta_ctx->copied_reassoc_rsp = qdf_mem_malloc(
			sizeof(struct wlan_cm_connect_resp));
	if (!sta_ctx->copied_reassoc_rsp)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(sta_ctx->copied_reassoc_rsp, reassoc_rsp,
		     sizeof(struct wlan_cm_connect_resp));

	sta_ctx->copied_reassoc_rsp->roaming_info = qdf_mem_malloc(
			sizeof(struct wlan_roam_sync_info));

	if (!sta_ctx->copied_reassoc_rsp->roaming_info) {
		qdf_mem_free(sta_ctx->copied_reassoc_rsp);
		sta_ctx->copied_reassoc_rsp = NULL;
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_copy(sta_ctx->copied_reassoc_rsp->roaming_info,
		     reassoc_rsp->roaming_info,
		     sizeof(struct wlan_roam_sync_info));

	connect_ies = &sta_ctx->copied_reassoc_rsp->connect_ies;

	connect_ies->assoc_rsp.len =
		reassoc_rsp->connect_ies.assoc_rsp.len;

	connect_ies->assoc_rsp.ptr = qdf_mem_malloc(
			connect_ies->assoc_rsp.len);

	if (!connect_ies->assoc_rsp.ptr) {
		qdf_mem_free(sta_ctx->copied_reassoc_rsp->roaming_info);
		sta_ctx->copied_reassoc_rsp->roaming_info = NULL;
		qdf_mem_free(sta_ctx->copied_reassoc_rsp);
		sta_ctx->copied_reassoc_rsp = NULL;
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_copy(connect_ies->assoc_rsp.ptr,
		     reassoc_rsp->connect_ies.assoc_rsp.ptr,
		     reassoc_rsp->connect_ies.assoc_rsp.len);

	connect_ies->assoc_req.len = 0;
	connect_ies->assoc_req.ptr = NULL;
	connect_ies->bcn_probe_rsp.len = 0;
	connect_ies->bcn_probe_rsp.ptr = NULL;
	connect_ies->link_bcn_probe_rsp.len = 0;
	connect_ies->link_bcn_probe_rsp.ptr = NULL;
	connect_ies->fils_ie = NULL;

	mlo_debug("Copied reassoc response for vdev: %d len: %d",
		  wlan_vdev_get_id(vdev), connect_ies->assoc_rsp.len);

	return QDF_STATUS_SUCCESS;
}

static bool
mlo_roam_is_internal_disconnect(struct wlan_objmgr_vdev *link_vdev)
{
	struct wlan_cm_vdev_discon_req *disconn_req;

	if (wlan_vdev_mlme_is_mlo_link_vdev(link_vdev) &&
	    wlan_cm_is_vdev_disconnecting(link_vdev)) {
		mlo_debug("Disconnect is ongoing on vdev %d",
			  wlan_vdev_get_id(link_vdev));

		disconn_req = qdf_mem_malloc(sizeof(*disconn_req));
		if (!disconn_req) {
			mlme_err("Malloc failed for disconnect req");
			return false;
		}

		if (!wlan_cm_get_active_disconnect_req(link_vdev,
						       disconn_req)) {
			mlme_err("vdev: %d: Active disconnect not found",
				 wlan_vdev_get_id(link_vdev));
			qdf_mem_free(disconn_req);
			return false;
		}

		mlo_debug("Disconnect source %d", disconn_req->req.source);

		if (disconn_req->req.source == CM_MLO_ROAM_INTERNAL_DISCONNECT) {
			qdf_mem_free(disconn_req);
			return true;
		}

		qdf_mem_free(disconn_req);
	}
	/* Disconnect is not ongoing */
	return true;
}

static QDF_STATUS
mlo_roam_validate_req(struct wlan_objmgr_vdev *vdev,
		      struct wlan_objmgr_vdev *link_vdev,
		      struct wlan_cm_connect_resp *rsp)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx;

	if (!vdev) {
		mlo_debug_rl("vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		mlo_debug_rl("mlo_dev_ctx is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (sta_ctx && sta_ctx->disconn_req) {
		mlo_debug("Handle pending disconnect for vdev %d",
			  wlan_vdev_get_id(vdev));
		mlo_handle_pending_disconnect(vdev);
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_cm_is_vdev_disconnected(vdev) ||
	    (wlan_vdev_mlme_is_mlo_link_vdev(link_vdev) &&
	     (wlan_cm_is_vdev_connecting(link_vdev) ||
	      !mlo_roam_is_internal_disconnect(link_vdev)))) {
		if (sta_ctx) {
			if (sta_ctx->copied_reassoc_rsp) {
				wlan_cm_free_connect_resp(sta_ctx->copied_reassoc_rsp);
				sta_ctx->copied_reassoc_rsp = NULL;
			}
			copied_conn_req_lock_acquire(sta_ctx);
			if (sta_ctx->copied_conn_req) {
				wlan_cm_free_connect_req(sta_ctx->copied_conn_req);
				sta_ctx->copied_conn_req = NULL;
			}
			copied_conn_req_lock_release(sta_ctx);
		}
	}

	if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		mlo_debug("Vdev: %d", wlan_vdev_get_id(vdev));
		if (wlan_cm_is_vdev_disconnected(vdev)) {
			mlo_handle_sta_link_connect_failure(vdev, rsp);
			return QDF_STATUS_E_FAILURE;
		} else if (!wlan_cm_is_vdev_connected(vdev)) {
			/* If vdev is not in disconnected or connected state,
			 * then the event is received due to connect req being
			 * flushed. Hence, ignore this event
			 */
			if (sta_ctx && sta_ctx->copied_reassoc_rsp) {
				wlan_cm_free_connect_resp(sta_ctx->copied_reassoc_rsp);
				sta_ctx->copied_reassoc_rsp = NULL;
			}
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (wlan_vdev_mlme_is_mlo_link_vdev(link_vdev) &&
	    (wlan_cm_is_vdev_connecting(link_vdev) ||
	     !mlo_roam_is_internal_disconnect(link_vdev))) {
		return QDF_STATUS_E_FAILURE;
	}

	if (sta_ctx && !wlan_vdev_mlme_is_mlo_link_vdev(vdev)) {
		if (sta_ctx->assoc_rsp.ptr) {
			qdf_mem_free(sta_ctx->assoc_rsp.ptr);
			sta_ctx->assoc_rsp.ptr = NULL;
		}
		sta_ctx->assoc_rsp.len = rsp->connect_ies.assoc_rsp.len;
		sta_ctx->assoc_rsp.ptr =
			qdf_mem_malloc(rsp->connect_ies.assoc_rsp.len);
		if (!sta_ctx->assoc_rsp.ptr)
			return QDF_STATUS_E_FAILURE;
		if (rsp->connect_ies.assoc_rsp.ptr)
			qdf_mem_copy(sta_ctx->assoc_rsp.ptr,
				     rsp->connect_ies.assoc_rsp.ptr,
				     rsp->connect_ies.assoc_rsp.len);
		/* Update connected_links_bmap for all vdev taking
		 * part in association
		 */
		mlo_update_connected_links(vdev, 1);
		mlo_update_connected_links_bmap(mlo_dev_ctx,
						rsp->ml_parnter_info);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
mlo_roam_prepare_and_send_link_connect_req(struct wlan_objmgr_vdev *assoc_vdev,
					   struct wlan_objmgr_vdev *link_vdev,
					   struct wlan_cm_connect_resp *rsp,
					   struct qdf_mac_addr *link_addr,
					   uint16_t chan_freq)
{
	struct wlan_mlo_sta *sta_ctx;
	struct wlan_cm_connect_req req = {0};
	struct wlan_ssid ssid = {0};
	struct rso_config *rso_cfg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!assoc_vdev->mlo_dev_ctx || !assoc_vdev->mlo_dev_ctx->sta_ctx)
		return QDF_STATUS_E_FAILURE;

	sta_ctx = assoc_vdev->mlo_dev_ctx->sta_ctx;

	wlan_vdev_mlme_get_ssid(assoc_vdev, ssid.ssid,
				&ssid.length);

	req.vdev_id = wlan_vdev_get_id(link_vdev);
	req.source = CM_MLO_LINK_VDEV_CONNECT;
	req.chan_freq = chan_freq;
	qdf_mem_copy(&req.bssid.bytes, link_addr->bytes, QDF_MAC_ADDR_SIZE);

	req.ssid.length = ssid.length;
	qdf_mem_copy(&req.ssid.ssid, &ssid.ssid, ssid.length);
	qdf_copy_macaddr(&req.mld_addr, &rsp->mld_addr);

	req.ml_parnter_info = rsp->ml_parnter_info;

	rso_cfg = wlan_cm_get_rso_config(assoc_vdev);
	if (rso_cfg) {
		req.crypto.rsn_caps = rso_cfg->orig_sec_info.rsn_caps;
		req.crypto.auth_type = rso_cfg->orig_sec_info.authmodeset;
		req.crypto.ciphers_pairwise =
				rso_cfg->orig_sec_info.ucastcipherset;
		req.crypto.group_cipher = rso_cfg->orig_sec_info.mcastcipherset;
		req.crypto.mgmt_ciphers = rso_cfg->orig_sec_info.mgmtcipherset;
		req.crypto.akm_suites = rso_cfg->orig_sec_info.key_mgmt;
		req.assoc_ie.len = rso_cfg->assoc_ie.len;

		req.assoc_ie.ptr = qdf_mem_malloc(req.assoc_ie.len);
		if (!req.assoc_ie.ptr)
			return QDF_STATUS_E_NOMEM;

		if (rso_cfg->assoc_ie.len)
			qdf_mem_copy(req.assoc_ie.ptr, rso_cfg->assoc_ie.ptr,
				     rso_cfg->assoc_ie.len);
	}

	mlme_cm_osif_roam_get_scan_params(assoc_vdev, &req.scan_ie,
					  &req.dot11mode_filter);

	mlme_info("vdev:%d Connecting to " QDF_SSID_FMT " link_addr: " QDF_MAC_ADDR_FMT " freq %d rsn_caps:0x%x auth_type:0x%x pairwise:0x%x grp:0x%x mcast:0x%x akms:0x%x assoc_ie_len:%d f_rsne:%d is_wps:%d dot11_filter:%d",
		  req.vdev_id, QDF_SSID_REF(req.ssid.length, req.ssid.ssid),
		  QDF_MAC_ADDR_REF(link_addr->bytes),
		  req.chan_freq, req.crypto.rsn_caps, req.crypto.auth_type,
		  req.crypto.ciphers_pairwise, req.crypto.group_cipher,
		  req.crypto.mgmt_ciphers, req.crypto.akm_suites,
		  req.assoc_ie.len, req.force_rsne_override,
		  req.is_wps_connection, req.dot11mode_filter);

	copied_conn_req_lock_acquire(sta_ctx);
	if (!sta_ctx->copied_conn_req)
		sta_ctx->copied_conn_req =
			qdf_mem_malloc(sizeof(struct wlan_cm_connect_req));
	else
		wlan_cm_free_connect_req_param(sta_ctx->copied_conn_req);

	if (!sta_ctx->copied_conn_req) {
		mlo_err("MLO_ROAM: vdev:%d Failed to allocate connect req",
			req.vdev_id);
		copied_conn_req_lock_release(sta_ctx);
		status = QDF_STATUS_E_NOMEM;
		goto err;
	}

	qdf_mem_copy(sta_ctx->copied_conn_req, &req,
		     sizeof(struct wlan_cm_connect_req));
	mlo_allocate_and_copy_ies(sta_ctx->copied_conn_req, &req);
	copied_conn_req_lock_release(sta_ctx);

	status = mlo_roam_validate_req(assoc_vdev, link_vdev, rsp);
	if (QDF_IS_STATUS_ERROR(status))
		goto err;

	status = wlan_cm_start_connect(link_vdev, &req);
	if (QDF_IS_STATUS_ERROR(status))
		goto err;

	mlo_update_connected_links(link_vdev, 1);
err:
	qdf_mem_free(req.assoc_ie.ptr);

	return status;
}

void mlo_roam_free_copied_reassoc_rsp(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_sta *sta_ctx;

	if (!vdev)
		return;

	if (!vdev->mlo_dev_ctx)
		return;

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!sta_ctx || !sta_ctx->copied_reassoc_rsp ||
	    !sta_ctx->copied_reassoc_rsp->roaming_info)
		return;

	wlan_cm_free_connect_resp(sta_ctx->copied_reassoc_rsp);
	sta_ctx->copied_reassoc_rsp = NULL;
}

void mlo_roam_connect_complete(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_sta *sta_ctx;
	uint8_t auth_status;

	if (!vdev)
		return;

	if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		return;

	if (!vdev->mlo_dev_ctx)
		return;

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!sta_ctx || !sta_ctx->copied_reassoc_rsp ||
	    !sta_ctx->copied_reassoc_rsp->roaming_info)
		return;

	auth_status = sta_ctx->copied_reassoc_rsp->roaming_info->auth_status;
	if (!mlo_check_connect_req_bmap(vdev) &&
	    auth_status == ROAM_AUTH_STATUS_CONNECTED) {
		wlan_cm_free_connect_resp(sta_ctx->copied_reassoc_rsp);
		sta_ctx->copied_reassoc_rsp = NULL;
	}
}

bool
mlo_roam_is_auth_status_connected(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	bool status = false;
	struct wlan_mlo_sta *sta_ctx;
	struct wlan_cm_connect_resp *rsp;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc)
		return status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev)
		return status;

	if (!vdev->mlo_dev_ctx)
		goto end;

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!sta_ctx || !sta_ctx->copied_reassoc_rsp ||
	    !sta_ctx->copied_reassoc_rsp->roaming_info)
		goto end;

	rsp = sta_ctx->copied_reassoc_rsp;
	if (rsp->roaming_info->auth_status == ROAM_AUTH_STATUS_CONNECTED)
		status = true;

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

QDF_STATUS
mlo_roam_link_connect_notify(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_mlo_sta *sta_ctx = NULL;
	struct wlan_cm_connect_resp *rsp;
	struct wlan_objmgr_vdev *assoc_vdev;
	struct wlan_objmgr_vdev *link_vdev = NULL;
	struct wlan_objmgr_vdev *vdev;
	struct mlo_partner_info partner_info;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i;
	uint8_t assoc_vdev_id;
	uint8_t link_vdev_id;

	if (!psoc)
		return QDF_STATUS_E_NULL_VALUE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	if (!vdev->mlo_dev_ctx) {
		mlo_err("mlo dev ctx is null");
		status = QDF_STATUS_E_FAILURE;
		goto err;
	}

	sta_ctx = vdev->mlo_dev_ctx->sta_ctx;
	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		mlo_debug("MLO_ROAM: Ignore if not mlo vdev");
		status = QDF_STATUS_E_FAILURE;
		goto err;
	}

	assoc_vdev = wlan_mlo_get_assoc_link_vdev(vdev);
	if (!assoc_vdev) {
		status =  QDF_STATUS_E_NULL_VALUE;
		goto err;
	}

	assoc_vdev_id = wlan_vdev_get_id(assoc_vdev);
	if (!sta_ctx || !sta_ctx->copied_reassoc_rsp) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto err;
	}

	rsp = sta_ctx->copied_reassoc_rsp;
	partner_info = rsp->ml_parnter_info;
	mlo_debug("partner links %d", partner_info.num_partner_links);

	for (i = 0; i < partner_info.num_partner_links; i++) {
		link_vdev_id = partner_info.partner_link_info[i].vdev_id;
		if (assoc_vdev_id == link_vdev_id)
			continue;
		link_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
								 link_vdev_id,
								 WLAN_MLME_SB_ID);
		if (!link_vdev) {
			mlo_err("Link vdev is null");
			status = QDF_STATUS_E_NULL_VALUE;
			goto err;
		}

		if (mlo_check_connect_req_bmap(link_vdev)) {
			status = mlo_roam_prepare_and_send_link_connect_req(assoc_vdev,
							link_vdev,
							rsp,
							&partner_info.partner_link_info[i].link_addr,
							partner_info.partner_link_info[i].chan_freq);
			if (QDF_IS_STATUS_ERROR(status))
				goto err;
			else {
				mlo_update_connect_req_links(link_vdev, false);
				goto end;
			}
		}
	}
err:
	if (link_vdev)
		mlo_clear_connect_req_links_bmap(link_vdev);
	if (sta_ctx && sta_ctx->copied_reassoc_rsp) {
		wlan_cm_free_connect_resp(sta_ctx->copied_reassoc_rsp);
		sta_ctx->copied_reassoc_rsp = NULL;
	}
end:
	if (link_vdev)
		wlan_objmgr_vdev_release_ref(link_vdev, WLAN_MLME_SB_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

bool
mlo_is_roaming_in_progress(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	bool is_roaming_in_progress = false;
	uint8_t link_vdev_id;
	uint8_t i;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", vdev_id);
		return false;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		mlme_err("mlo_dev_ctx object is NULL for vdev %d", vdev_id);
		goto end;
	}

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		link_vdev_id = wlan_vdev_get_id(mlo_dev_ctx->wlan_vdev_list[i]);
		if (link_vdev_id == WLAN_INVALID_VDEV_ID) {
			mlme_err("invalid vdev id");
			goto end;
		}

		if (wlan_cm_is_roam_sync_in_progress(psoc, link_vdev_id)) {
			is_roaming_in_progress = true;
			goto end;
		}
	}

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
	return is_roaming_in_progress;
}

QDF_STATUS
mlo_add_all_link_probe_rsp_to_scan_db(struct wlan_objmgr_psoc *psoc,
			struct roam_scan_candidate_frame *rcvd_frame)
{
	uint8_t *ml_ie, link_id, idx, ies_offset;
	qdf_size_t ml_ie_total_len, gen_frame_len;
	QDF_STATUS status;
	struct mlo_partner_info ml_partner_info = {0};
	struct element_info rcvd_probe_rsp, gen_probe_rsp = {0, NULL};
	struct roam_scan_candidate_frame entry = {0};
	struct qdf_mac_addr self_link_addr;
	struct wlan_objmgr_vdev *vdev;

	/* Add the received scan entry as it is */
	wlan_cm_add_frame_to_scan_db(psoc, rcvd_frame);

	ies_offset = WLAN_MAC_HDR_LEN_3A + WLAN_PROBE_RESP_IES_OFFSET;
	if (rcvd_frame->frame_length < ies_offset) {
		mlme_err("No IEs in probe rsp");
		return QDF_STATUS_E_FAILURE;
	}

	status = util_find_mlie(rcvd_frame->frame + ies_offset,
				rcvd_frame->frame_length - ies_offset,
				&ml_ie, &ml_ie_total_len);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_SUCCESS;

	status = util_get_bvmlie_persta_partner_info(ml_ie,
						     ml_ie_total_len,
						     &ml_partner_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Per STA profile parsing failed");
		return status;
	}

	gen_frame_len = MAX_MGMT_MPDU_LEN;

	gen_probe_rsp.ptr = qdf_mem_malloc(gen_frame_len);
	if (!gen_probe_rsp.ptr)
		return QDF_STATUS_E_NOMEM;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, rcvd_frame->vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto done;
	}
	qdf_mem_copy(self_link_addr.bytes,
		     wlan_vdev_mlme_get_macaddr(vdev),
		     QDF_MAC_ADDR_SIZE);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	rcvd_probe_rsp.ptr = rcvd_frame->frame + WLAN_MAC_HDR_LEN_3A;
	rcvd_probe_rsp.len = rcvd_frame->frame_length - WLAN_MAC_HDR_LEN_3A;

	for (idx = 0; idx < ml_partner_info.num_partner_links; idx++) {
		link_id = ml_partner_info.partner_link_info[idx].link_id;
		status = util_gen_link_probe_rsp(rcvd_probe_rsp.ptr,
				rcvd_probe_rsp.len,
				link_id,
				self_link_addr,
				gen_probe_rsp.ptr,
				gen_frame_len,
				(qdf_size_t *)&gen_probe_rsp.len);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err("MLO: Link %d probe resp gen failed %d",
				 link_id, status);
			status = QDF_STATUS_E_FAILURE;
			goto done;
		}

		mlme_debug("MLO: link probe rsp size:%u orig probe rsp :%u",
			   gen_probe_rsp.len, rcvd_probe_rsp.len);

		entry.vdev_id = rcvd_frame->vdev_id;
		entry.frame = gen_probe_rsp.ptr;
		entry.frame_length = gen_probe_rsp.len;
		entry.rssi = rcvd_frame->rssi;

		wlan_cm_add_frame_to_scan_db(psoc, &entry);
	}
done:
	qdf_mem_free(gen_probe_rsp.ptr);

	return status;
}

bool
mlo_is_enable_roaming_on_connected_sta_allowed(struct wlan_objmgr_vdev *vdev)
{
	struct mlo_partner_info *partner_info;

	if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		return true;

	if (!vdev->mlo_dev_ctx || !vdev->mlo_dev_ctx->sta_ctx ||
	    !vdev->mlo_dev_ctx->sta_ctx->copied_reassoc_rsp)
		return true;

	partner_info =
	       &vdev->mlo_dev_ctx->sta_ctx->copied_reassoc_rsp->ml_parnter_info;
	if (partner_info->num_partner_links <= 1)
		return true;

	/* Roamed to MLO AP, do nothing if link vdev is disconnected */
	return false;
}

bool
mlo_check_is_given_vdevs_on_same_mld(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id_1, uint8_t vdev_id_2)
{
	struct wlan_objmgr_vdev *vdev1;
	struct wlan_mlo_dev_context *ml_dev_ctx1;
	struct wlan_objmgr_vdev **vdev_list;
	bool is_same_mld = false;
	uint8_t i;

	vdev1 = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id_1,
						     WLAN_MLME_CM_ID);
	if (!vdev1)
		return false;

	ml_dev_ctx1 = vdev1->mlo_dev_ctx;
	if (!ml_dev_ctx1)
		goto end;

	vdev_list = ml_dev_ctx1->wlan_vdev_list;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!vdev_list[i])
			continue;

		if (wlan_vdev_get_id(vdev_list[i]) == vdev_id_2) {
			is_same_mld = true;
			goto end;
		}
	}

end:
	if (vdev1)
		wlan_objmgr_vdev_release_ref(vdev1, WLAN_MLME_CM_ID);

	return is_same_mld;
}
