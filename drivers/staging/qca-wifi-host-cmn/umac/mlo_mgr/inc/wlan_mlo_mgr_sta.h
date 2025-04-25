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
 * DOC: contains MLO manager public file containing STA functionality
 */
#ifndef _WLAN_MLO_MGR_STA_H_
#define _WLAN_MLO_MGR_STA_H_

#include <wlan_cm_ucfg_api.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_mlo_mgr_cmn.h>
#ifdef WLAN_FEATURE_11BE_MLO
#include <wlan_mlo_mgr_public_structs.h>
#include <wlan_mlo_mgr_peer.h>

/**
 * mlo_connect - Start the connection process
 * @vdev: pointer to vdev
 * @req: connection request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_connect(struct wlan_objmgr_vdev *vdev,
		       struct wlan_cm_connect_req *req);

/**
 * mlo_sta_link_connect_notify - Called by connection manager to notify the
 * STA link connect is complete
 * @vdev: pointer to vdev
 * @rsp: MLO connect response
 *
 * Connection manager will notify the MLO manager when the link has started
 * and MLO manager will start the subsequent connections, if necessary
 *
 * Return: none
 */
void
mlo_sta_link_connect_notify(struct wlan_objmgr_vdev *vdev,
			    struct wlan_cm_connect_resp *rsp);

/**
 * mlo_disconnect - Start the disconnection process
 * @vdev: pointer to vdev
 * @source: source of the request (can be connect or disconnect request)
 * @reason_code: reason for disconnect
 * @bssid: BSSID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_disconnect(struct wlan_objmgr_vdev *vdev,
			  enum wlan_cm_source source,
			  enum wlan_reason_code reason_code,
			  struct qdf_mac_addr *bssid);

/**
 * mlo_sync_disconnect - Start the sync disconnection process
 * @vdev: pointer to vdev
 * @source: source of the request (can be connect or disconnect request)
 * @reason_code: reason for disconnect
 * @bssid: BSSID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_sync_disconnect(struct wlan_objmgr_vdev *vdev,
			       enum wlan_cm_source source,
			       enum wlan_reason_code reason_code,
			       struct qdf_mac_addr *bssid);

/**
 * mlo_sta_link_disconn_notify - Notifies that STA link disconnect completion
 * @vdev: pointer to vdev
 * @resp: disconnect resp
 *
 * Return: none
 */
void mlo_sta_link_disconn_notify(struct wlan_objmgr_vdev *vdev,
				 struct wlan_cm_discon_rsp *resp);

/**
 * mlo_handle_sta_link_connect_failure - Notifies that STA link connect failure
 * @vdev: pointer to vdev
 * @rsp: connect resp
 *
 * Return: none
 */
void mlo_handle_sta_link_connect_failure(struct wlan_objmgr_vdev *vdev,
					 struct wlan_cm_connect_resp *rsp);

/**
 * mlo_handle_pending_disconnect - Handle pending disconnect if received
 * while link connect is ongoing.
 * @vdev: pointer to vdev
 *
 * Return: none
 */
void mlo_handle_pending_disconnect(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_is_mld_sta - Check if MLD associated with the vdev is a station
 * @vdev: pointer to vdev
 *
 * Return: true if MLD is a station, false otherwise
 */
bool mlo_is_mld_sta(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_mlo_is_mld_disconnected - Check whether MLD is disconnected
 * @vdev: pointer to vdev
 *
 * Return: true if mld is disconnected, false otherwise
 */
bool ucfg_mlo_is_mld_disconnected(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_is_mld_disconnecting_connecting - Check whether MLD is disconnecting or
 * connecting
 * @vdev: pointer to vdev
 *
 * Return: true if mld is disconnecting or connecting, false otherwise
 */
bool mlo_is_mld_disconnecting_connecting(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_is_ml_connection_in_progress - Check whether MLD assoc or link vdev is
 * connecting
 *
 * @psoc: pointer to psoc
 * @vdev_id: vdev id
 *
 * Return: true if mld is disconnecting, false otherwise
 */
bool mlo_is_ml_connection_in_progress(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id);

#ifndef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * ucfg_mlo_is_mld_connected - Check whether MLD is connected
 * @vdev: pointer to vdev
 *
 * Return: true if mld is connected, false otherwise
 */
bool ucfg_mlo_is_mld_connected(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_mlo_mld_clear_mlo_cap - Clear MLO cap for all vdevs in MLD
 * @vdev: pointer to vdev
 *
 * Return: None
 */
void ucfg_mlo_mld_clear_mlo_cap(struct wlan_objmgr_vdev *vdev);
#endif

#ifdef WLAN_MLO_MULTI_CHIP
/**
 * mlo_get_central_umac_id - get central umac in 3 link topology
 * @psoc_ids: pointer to psoc_ids
 *
 * Return: central umac if success, -1 all soc are adjacent to each other
 */

int8_t mlo_get_central_umac_id(uint8_t *psoc_ids);

/**
 * mlo_check_topology() - check topology of the psoc's in STA
 * @pdev: pdev pointer
 * @vdev: vdev pointer
 * @aplinks: Number of active links on ap side
 *
 * Return: QDF_STATUS
 */

QDF_STATUS mlo_check_topology(struct wlan_objmgr_pdev *pdev,
			      struct wlan_objmgr_vdev *vdev,
			      uint8_t aplinks);
/**
 * mlo_update_partner_bridge_info() - Update parter info of bridge vap
 * @ml_dev: ML dev context
 * @partner_info: partner info that needs to be updated
 *
 * Return: none
 */

void mlo_update_partner_bridge_info(struct wlan_mlo_dev_context *ml_dev,
				    struct mlo_partner_info *partner_info);
/**
 * mlo_get_total_links() - get total links supported by device
 * @pdev: pdev pointer
 *
 * Return: Number of total links supported
 */

uint8_t mlo_get_total_links(struct wlan_objmgr_pdev *pdev);

/**
 * mlo_is_sta_bridge_vdev() - Check if the vdev is sta bridge vdev
 * @vdev: vdev pointer
 *
 * Return: True if STA bridge vdev else false
 */

bool mlo_is_sta_bridge_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_sta_bridge_exists() - Check if bridge vdev exists in a STA MLD
 * @vdev: vdev pointer
 *
 * Return: True if Bridge Vdev exists else false
 */
bool mlo_sta_bridge_exists(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_is_force_central_primary() - Check if central vdev is forced
 * as primary
 * @vdev: vdev pointer
 *
 * Return: True if Central Vdev is force as primary else false
 */

bool mlo_is_force_central_primary(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_set_sta_ctx_bss_mld_addr() - Save BSS MLD in sta ctx
 * @vdev: vdev pointer
 * @bss_mld_addr: MLD Address of BSS
 *
 * Return: none
 */

static inline
void mlo_set_sta_ctx_bss_mld_addr(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr *bss_mld_addr)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;

	if (!vdev || !vdev->mlo_dev_ctx || !bss_mld_addr)
		return;

	ml_dev = vdev->mlo_dev_ctx;
	if (mlo_sta_bridge_exists(vdev) &&
	    qdf_is_macaddr_zero(&ml_dev->bridge_sta_ctx->bss_mld_addr)) {
		if (qdf_is_macaddr_zero(bss_mld_addr))
			return;
		qdf_copy_macaddr(&ml_dev->bridge_sta_ctx->bss_mld_addr,
				 bss_mld_addr);
	}
}

/**
 * mlo_get_sta_ctx_bss_mld_addr() - Get BSS MLD from sta ctx
 * @vdev: vdev pointer
 *
 * Return: pointer to qdf_mac_addr
 */

static inline
struct qdf_mac_addr *mlo_get_sta_ctx_bss_mld_addr(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;
	struct qdf_mac_addr *mld_addr;

	if (!vdev || !vdev->mlo_dev_ctx)
		return NULL;

	ml_dev = vdev->mlo_dev_ctx;
	if (mlo_sta_bridge_exists(vdev) &&
	    !qdf_is_macaddr_zero(&ml_dev->bridge_sta_ctx->bss_mld_addr)) {
		mld_addr = &ml_dev->bridge_sta_ctx->bss_mld_addr;
		return mld_addr;
	}
	return NULL;
}

/**
 * mlo_clear_bridge_sta_ctx() - clear bridge_sta_ctx
 * @vdev: vdev object
 *
 * Return: none
 */
static inline
void mlo_clear_bridge_sta_ctx(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;
	struct wlan_objmgr_vdev *tmp_vdev = NULL;
	uint8_t bridge_umac_id = -1;

	if (!vdev || !vdev->mlo_dev_ctx)
		return;

	ml_dev = vdev->mlo_dev_ctx;
	if (ml_dev->bridge_sta_ctx) {
		bridge_umac_id = ml_dev->bridge_sta_ctx->bridge_umac_id;
		qdf_mem_zero(ml_dev->bridge_sta_ctx,
			     sizeof(struct wlan_mlo_bridge_sta));
	}
	if (mlo_is_force_central_primary(vdev)) {
		tmp_vdev = mlo_get_link_vdev_from_psoc_id(ml_dev,
							  bridge_umac_id,
							  false);
		if (!tmp_vdev) {
			mlo_err("VDEV derivation failed for %u psoc wds bridge",
				bridge_umac_id);
			return;
		}
		tmp_vdev->vdev_objmgr.mlo_central_vdev = false;
		wlan_objmgr_vdev_release_ref(tmp_vdev, WLAN_MLO_MGR_ID);
	}
}

#else
static inline
void mlo_set_sta_ctx_bss_mld_addr(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr *bss_mld_addr)
{ }

static inline
struct qdf_mac_addr *mlo_get_sta_ctx_bss_mld_addr(struct wlan_objmgr_vdev *vdev)
{
	return NULL;
}

static inline
bool mlo_is_sta_bridge_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool mlo_sta_bridge_exists(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool mlo_is_force_central_primary(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
void mlo_clear_bridge_sta_ctx(struct wlan_objmgr_vdev *vdev)
{ }
#endif
/**
 * wlan_mlo_get_tdls_link_vdev() - API to get tdls link vdev
 * @vdev: vdev object
 *
 * Return: MLD tdls link vdev
 */
struct wlan_objmgr_vdev *
wlan_mlo_get_tdls_link_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_mlo_get_assoc_link_vdev - API to get assoc link vdev
 * @vdev: vdev object
 *
 * Return: MLD assoc link vdev
 */
struct wlan_objmgr_vdev *
ucfg_mlo_get_assoc_link_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_get_assoc_link_vdev - API to get assoc link vdev
 * @vdev: vdev object
 *
 * Return: MLD assoc link vdev
 */
struct wlan_objmgr_vdev *
wlan_mlo_get_assoc_link_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_update_connected_links_bmap: update connected links bitmap
 * @mlo_dev_ctx: mlo dev context ptr
 * @ml_partner_info: ml parnter info ptr
 *
 * Return: none
 */
void
mlo_update_connected_links_bmap(struct wlan_mlo_dev_context *mlo_dev_ctx,
				struct mlo_partner_info ml_partner_info);

/**
 * mlo_clear_connected_links_bmap() - clear connected links bitmap
 * @vdev: vdev object
 *
 * Return: none
 */
void mlo_clear_connected_links_bmap(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_set_cu_bpcc() - set the bpcc per link id
 * @vdev: vdev object
 * @vdev_id: the id of vdev
 * @bpcc: bss parameters change count
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_set_cu_bpcc(struct wlan_objmgr_vdev *vdev, uint8_t vdev_id,
			   uint8_t bpcc);

/**
 * mlo_get_cu_bpcc() - get the bpcc per link id
 * @vdev: vdev object
 * @vdev_id: the id of vdev
 * @bpcc: the bss parameters change count pointer to save value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_get_cu_bpcc(struct wlan_objmgr_vdev *vdev, uint8_t vdev_id,
			   uint8_t *bpcc);

/**
 * mlo_init_cu_bpcc() - initialize the bpcc for vdev
 * @mlo_dev_ctx: wlan mlo dev context
 * @vdev_id: vdev id
 *
 * Return: void
 */
void mlo_init_cu_bpcc(struct wlan_mlo_dev_context *mlo_dev_ctx,
		      uint8_t vdev_id);

/**
 * mlo_clear_cu_bpcc() - clear the bpcc info
 * @vdev: vdev object
 *
 * Return: void
 */
void mlo_clear_cu_bpcc(struct wlan_objmgr_vdev *vdev);

/**
 * typedef mlo_vdev_op_handler() - API to have operation on ml vdevs
 * @vdev: vdev object
 * @arg: operation-specific argument
 */
typedef void (*mlo_vdev_op_handler)(struct wlan_objmgr_vdev *vdev,
				    void *arg);

/**
 * mlo_iterate_connected_vdev_list: Iterate on connected ML links
 * @vdev: vdev object
 * @handler: the handler will be called for each object in ML list
 * @arg: argument to be passed to handler
 *
 * Return: none
 */
static inline
void mlo_iterate_connected_vdev_list(struct wlan_objmgr_vdev *vdev,
				     mlo_vdev_op_handler handler,
				     void *arg)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	uint8_t i = 0;

	if (!mlo_dev_ctx || !(wlan_vdev_mlme_is_mlo_vdev(vdev)))
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;
		if (qdf_test_bit(i, sta_ctx->wlan_connected_links)) {
			if (handler)
				handler(mlo_dev_ctx->wlan_vdev_list[i], arg);
		}
	}
}

/**
 * call_handler_for_standalone_ap: Iterate on all standalone ML vdevs in
 * ML AP context and call handler only for standalone AP
 *
 * @ap_dev_ctx: AP vdev context
 * @handler: the handler will be called for each object in ML list
 * @arg: argument to be passed to handler
 *
 * Return: none
 */
static inline void
call_handler_for_standalone_ap(struct wlan_mlo_dev_context *ap_dev_ctx,
			       mlo_vdev_op_handler handler, void *arg)
{
	struct wlan_objmgr_vdev *ml_ap_vdev = NULL;
	int i;

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		/* For each vdev in ML AP context, check if its PDEV has any
		 * STA. If it doesn't, call the handler for that particular
		 * VDEV.
		 */
		if (!ap_dev_ctx->wlan_vdev_list[i])
			continue;
		ml_ap_vdev = ap_dev_ctx->wlan_vdev_list[i];
		handler(ml_ap_vdev, arg);
	}
}

/**
 * mlo_iterate_ml_standalone_vdev_list: Iterate on all standalone ML vdevs in
 * ML link
 *
 * @vdev: vdev object
 * @handler: the handler will be called for each object in ML list
 * @arg: argument to be passed to handler
 *
 * Return: none
 */
static inline
void mlo_iterate_ml_standalone_vdev_list(struct wlan_objmgr_vdev *vdev,
					 mlo_vdev_op_handler handler,
					 void *arg)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	uint8_t i = 0;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev_temp = NULL;
	struct wlan_mlo_dev_context *ap_ml_ctx;
	qdf_list_t *vdev_list;

	if (!mlo_dev_ctx || !(wlan_vdev_mlme_is_mlo_vdev(vdev)) || !handler)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	/* If repeater is configured as dependent WDS repeater,
	 * bring up/bring down all the standalone AP vaps in it once all
	 * the other AP vaps present in the AP ML context are up/down.
	 */

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		pdev = wlan_vdev_get_pdev(mlo_dev_ctx->wlan_vdev_list[i]);
		vdev_list = &pdev->pdev_objmgr.wlan_vdev_list;
		vdev_temp = wlan_pdev_vdev_list_peek_head(vdev_list);
		while (vdev_temp) {
			// Get all VDEVs of the STA vap from its PDEV
			if ((vdev_temp != vdev) &&
			    wlan_vdev_mlme_get_opmode(vdev_temp) ==
			    QDF_SAP_MODE) {
				ap_ml_ctx = vdev_temp->mlo_dev_ctx;
				if (!ap_ml_ctx)
					return;

				call_handler_for_standalone_ap(ap_ml_ctx,
							       handler, arg);
			}

			vdev_temp = wlan_vdev_get_next_vdev_of_pdev(
							vdev_list, vdev_temp);
		}
	}
}

/**
 * mlo_update_connect_req_links: update connect req links index
 * @vdev: vdev object
 * @value: set/clear the bit
 *
 * Return: none
 */
static inline void
mlo_update_connect_req_links(struct wlan_objmgr_vdev *vdev, uint8_t value)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	uint8_t i = 0;

	if (!mlo_dev_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (vdev == mlo_dev_ctx->wlan_vdev_list[i]) {
			if (value)
				qdf_set_bit(i, sta_ctx->wlan_connect_req_links);
			else
				qdf_clear_bit(
					i, sta_ctx->wlan_connect_req_links);
		}
	}
}

/**
 * mlo_is_vdev_connect_req_link: API to check if vdev is in active connection
 * @vdev: vdev object
 *
 * Return: true is vdev is participating in active connect else false
 */
static inline bool
mlo_is_vdev_connect_req_link(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!mlo_dev_ctx)
		return false;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return false;

	if (qdf_test_bit(
		mlo_get_link_vdev_ix(vdev->mlo_dev_ctx, vdev),
		sta_ctx->wlan_connect_req_links))
		return true;

	return false;
}

/**
 * mlo_clear_connect_req_links_bmap() - clear connect req links bitmap
 * @vdev: vdev object
 *
 * Return: none
 */
static inline
void mlo_clear_connect_req_links_bmap(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;

	if (!mlo_dev_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	qdf_mem_zero(sta_ctx->wlan_connect_req_links,
		     sizeof(sta_ctx->wlan_connect_req_links));
}

/**
 * mlo_update_connected_links: update connected links index
 * @vdev: vdev object
 * @value: set/clear the bit
 *
 * Return: none
 */
static inline void
mlo_update_connected_links(struct wlan_objmgr_vdev *vdev, uint8_t value)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = vdev->mlo_dev_ctx;
	struct wlan_mlo_sta *sta_ctx = NULL;
	uint8_t i = 0;

	if (!mlo_dev_ctx)
		return;

	sta_ctx = mlo_dev_ctx->sta_ctx;
	if (!sta_ctx)
		return;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		if (vdev == mlo_dev_ctx->wlan_vdev_list[i]) {
			if (value)
				qdf_set_bit(i, sta_ctx->wlan_connected_links);
			else
				qdf_clear_bit(i, sta_ctx->wlan_connected_links);
		}
	}
}

#ifndef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * mlo_get_ml_vdev_by_mac: get ml vdev from mac
 * @vdev: vdev object
 * @macaddr: mac of vdev to be returned
 *
 * Return: vdev object if found else NULL
 */
struct wlan_objmgr_vdev *
mlo_get_ml_vdev_by_mac(struct wlan_objmgr_vdev *vdev,
		       struct qdf_mac_addr *macaddr);
#endif

/**
 * mlo_get_chan_freq_by_bssid - Get channel freq by bssid
 * @pdev: pdev pointer
 * @bssid: link mac address
 *
 * Return: chan frequency
 */
qdf_freq_t
mlo_get_chan_freq_by_bssid(struct wlan_objmgr_pdev *pdev,
			   struct qdf_mac_addr *bssid);

/**
 * mlo_get_assoc_rsp - Get Assoc response from mlo manager
 * @vdev: vdev obj mgr
 * @assoc_rsp_frame: association response frame ptr
 *
 * Return: none
 */
void mlo_get_assoc_rsp(struct wlan_objmgr_vdev *vdev,
		       struct element_info *assoc_rsp_frame);

/**
 * mlo_sta_save_quiet_status - save quiet status for given link of mlo station
 * @mlo_dev_ctx: mlo context
 * @link_id: link id
 * @quiet_status: True if quiet starts. False if quiet stops.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_sta_save_quiet_status(struct wlan_mlo_dev_context *mlo_dev_ctx,
				     uint8_t link_id,
				     bool quiet_status);

/**
 * mlo_is_sta_in_quiet_status - is the link of given mlo sta is in quiet status
 * @mlo_dev_ctx: mlo context
 * @link_id: link id
 *
 * Return: true if the link of given mlo sta is in quiet status
 */
bool mlo_is_sta_in_quiet_status(struct wlan_mlo_dev_context *mlo_dev_ctx,
				uint8_t link_id);

/**
 * mlo_is_sta_inactivity_allowed_with_quiet() - Is link OK to force inactivity
 *                                              based on current quiet status
 *                                              of mlo connection
 * @psoc: pointer to psoc
 * @vdev_id_list: vdev id list
 * @num_mlo: number of mlo vdev
 * @mlo_idx: list of index of vdev_id_list if it is vdev id of mlo vdev
 * @affected_links: number of links to be set inactivity
 * @affected_list: list of vdev id to be set inactivity
 *
 * Return: true if any link not in mlo_vdev_list is not in quiet mode
 */
bool mlo_is_sta_inactivity_allowed_with_quiet(struct wlan_objmgr_psoc *psoc,
					      uint8_t *vdev_id_list,
					      uint8_t num_mlo, uint8_t *mlo_idx,
					      uint8_t affected_links,
					      uint8_t *affected_list);

/**
 * mlo_is_sta_csa_synced - Is mlo sta csa parameters are synced or not
 * @mlo_dev_ctx: mlo context
 * @link_id: link id
 *
 * Return: true if mlo sta csa parameters of given link id is synced
 */
bool mlo_is_sta_csa_synced(struct wlan_mlo_dev_context *mlo_dev_ctx,
			   uint8_t link_id);

/**
 * mlo_sta_csa_save_params - Save csa parameters for mlo station
 * @mlo_dev_ctx: mlo context
 * @link_id: link id
 * @csa_param: csa parameters to be saved
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_sta_csa_save_params(struct wlan_mlo_dev_context *mlo_dev_ctx,
				   uint8_t link_id,
				   struct csa_offload_params *csa_param);
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * mlo_sta_handle_csa_standby_link - Handle csa parameters for standby link
 * @mlo_dev_ctx: mlo context
 * @link_id: link id
 * @csa_param: csa parameters to be saved
 * @vdev: vdev obj mgr
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_sta_handle_csa_standby_link(
			struct wlan_mlo_dev_context *mlo_dev_ctx,
			uint8_t link_id,
			struct csa_offload_params *csa_param,
			struct wlan_objmgr_vdev *vdev);
#else
static inline
QDF_STATUS mlo_sta_handle_csa_standby_link(
			struct wlan_mlo_dev_context *mlo_dev_ctx,
			uint8_t link_id,
			struct csa_offload_params *csa_param,
			struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif
/**
 * mlo_sta_up_active_notify - mlo sta up active notify
 * @vdev: vdev obj mgr
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_sta_up_active_notify(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_is_sta_csa_param_handled - Is given csa_param handled or not
 * @vdev: vdev obj mgr
 * @csa_param: csa parameters to be checked
 *
 * Return: true if given csa parameters is handled
 */
bool mlo_is_sta_csa_param_handled(struct wlan_objmgr_vdev *vdev,
				  struct csa_offload_params *csa_param);

/**
 * mlo_internal_disconnect_links - Internal disconnect for connection manager
 * @vdev: vdev obj mgr
 *
 * Return: none
 */
void mlo_internal_disconnect_links(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_sta_vdev_get_reconfig_timer_state() - Get ml reconfig timer state on
 * vdev
 * @vdev: vdev pointer
 *
 * Return: true if reconfig timer is active, otherwise false
 */
bool mlo_sta_vdev_get_reconfig_timer_state(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_sta_stop_reconfig_timer_by_vdev() - Stop ml reconfig timer
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlo_sta_stop_reconfig_timer_by_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_sta_stop_reconfig_timer() - Stop reconfig timer on all vdev on ml dev
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlo_sta_stop_reconfig_timer(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_sta_get_vdev_list() - get mlo vdev list
 * @vdev: vdev pointer
 * @vdev_count: vdev count
 * @wlan_vdev_list: vdev list
 *
 * Return: None
 */
void mlo_sta_get_vdev_list(struct wlan_objmgr_vdev *vdev, uint16_t *vdev_count,
			   struct wlan_objmgr_vdev **wlan_vdev_list);

/**
 * mlo_process_ml_reconfig_ie() - process ml reconfig ie for vdev
 * @vdev: vdev pointer
 * @scan_entry: RootAP scan entry
 * @ml_ie: Pointer to ML IE
 * @ml_ie_len: Length of ML IE
 * @partner_info: Cached partner info
 *
 * Return: None
 */
void mlo_process_ml_reconfig_ie(struct wlan_objmgr_vdev *vdev,
				struct scan_cache_entry *scan_entry,
				uint8_t *ml_ie, qdf_size_t ml_ie_len,
				struct mlo_partner_info *partner_info);

/**
 * wlan_mlo_send_vdev_pause() - send MLO vdev pause to FW
 * @psoc: pointer to psoc
 * @vdev: vdev pointer
 * @session_id: session ID
 * @vdev_pause_dur: vdev pause duration
 *
 * Return: None
 */
void wlan_mlo_send_vdev_pause(struct wlan_objmgr_psoc *psoc,
			      struct wlan_objmgr_vdev *vdev,
			      uint16_t session_id,
			      uint16_t vdev_pause_dur);

/**
 * mlo_allocate_and_copy_ies() - allocate and copy ies
 * @target: target connect req pointer
 * @source: source connect req pointer
 *
 * Return: None
 */
void
mlo_allocate_and_copy_ies(struct wlan_cm_connect_req *target,
			  struct wlan_cm_connect_req *source);

/**
 * mlo_free_connect_ies() - free connect ies
 * @connect_req: connect req pointer
 *
 * Return: None
 */
void
mlo_free_connect_ies(struct wlan_cm_connect_req *connect_req);

/**
 * mlo_get_link_state_context() - get ml link context
 * @psoc: psoc handler
 * @resp_cb: api to handle link state callback
 * @context: response context
 * @vdev_id: vdev id
 */
QDF_STATUS
mlo_get_link_state_context(struct wlan_objmgr_psoc *psoc,
			   get_ml_link_state_cb *resp_cb,
			   void **context, uint8_t vdev_id);
#else
static inline
void mlo_clear_bridge_sta_ctx(struct wlan_objmgr_vdev *vdev)
{ }

static inline
void mlo_set_sta_ctx_bss_mld_addr(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr *bss_mld_addr)
{ }

static inline
struct qdf_mac_addr *mlo_get_sta_ctx_bss_mld_addr(struct wlan_objmgr_vdev *vdev)
{
	return NULL;
}

static inline
bool mlo_is_sta_bridge_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool mlo_sta_bridge_exists(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool mlo_is_force_central_primary(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
QDF_STATUS mlo_connect(struct wlan_objmgr_vdev *vdev,
		       struct wlan_cm_connect_req *req)
{
	return wlan_cm_start_connect(vdev, req);
}

static inline
void mlo_sta_link_connect_notify(struct wlan_objmgr_vdev *vdev,
				 struct wlan_cm_connect_resp *rsp)
{ }

static inline
QDF_STATUS mlo_disconnect(struct wlan_objmgr_vdev *vdev,
			  enum wlan_cm_source source,
			  enum wlan_reason_code reason_code,
			  struct qdf_mac_addr *bssid)
{
	QDF_STATUS status;

	status = wlan_cm_disconnect(vdev, source,
				    reason_code,
				    bssid);
	return status;
}

static inline
QDF_STATUS mlo_sync_disconnect(struct wlan_objmgr_vdev *vdev,
			       enum wlan_cm_source source,
			       enum wlan_reason_code reason_code,
			       struct qdf_mac_addr *bssid)
{
	return wlan_cm_disconnect_sync(vdev, CM_OSIF_DISCONNECT,
				       reason_code);
}

static inline
void mlo_sta_link_disconn_notify(struct wlan_objmgr_vdev *vdev,
				 struct wlan_cm_discon_rsp *resp)
{ }

#ifndef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static inline
bool ucfg_mlo_is_mld_connected(struct wlan_objmgr_vdev *vdev)
{
	return true;
}

static inline
bool ucfg_mlo_is_mld_disconnected(struct wlan_objmgr_vdev *vdev)
{
	return true;
}
#endif

static inline
bool mlo_is_mld_disconnecting_connecting(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool mlo_is_ml_connection_in_progress(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id)
{
	return false;
}

static inline
bool mlo_is_mld_sta(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
struct wlan_objmgr_vdev *
ucfg_mlo_get_assoc_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	return vdev;
}

static inline void
mlo_update_connect_req_links(struct wlan_objmgr_vdev *vdev, uint8_t value)
{ }

static inline void
mlo_update_connected_links_bmap(struct wlan_mlo_dev_context *mlo_dev_ctx,
				struct mlo_partner_info ml_parnter_info)
{ }

static inline bool
mlo_is_vdev_connect_req_link(struct wlan_objmgr_vdev *vdev)
{
	return true;
}

static inline void
mlo_update_connected_links(struct wlan_objmgr_vdev *vdev, uint8_t value)
{ }

static inline void
mlo_clear_connect_req_links_bmap(struct wlan_objmgr_vdev *vdev)
{ }

static inline void
mlo_clear_connected_links_bmap(struct wlan_objmgr_vdev *vdev)
{ }

static inline struct wlan_objmgr_vdev *
mlo_get_ml_vdev_by_mac(struct wlan_objmgr_vdev *vdev,
		       struct qdf_mac_addr *macaddr)
{
	return vdev;
}

static inline qdf_freq_t
mlo_get_chan_freq_by_bssid(struct wlan_objmgr_pdev *pdev,
			   struct qdf_mac_addr *bssid)
{
	return 0;
}

static inline void
mlo_get_assoc_rsp(struct wlan_objmgr_vdev *vdev,
		  struct element_info *assoc_rsp_frame)
{
}

static inline bool
mlo_is_sta_csa_param_handled(struct wlan_objmgr_vdev *vdev,
			     struct csa_offload_params *csa_param)
{
	return false;
}

static inline void mlo_internal_disconnect_links(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void mlo_sta_get_vdev_list(struct wlan_objmgr_vdev *vdev,
			   uint16_t *vdev_count,
			   struct wlan_objmgr_vdev **wlan_vdev_list)
{
}

static inline bool
mlo_sta_vdev_get_reconfig_timer_state(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
mlo_sta_stop_reconfig_timer_by_vdev(struct wlan_objmgr_vdev *vdev)
{
}

static inline void mlo_sta_stop_reconfig_timer(struct wlan_objmgr_vdev *vdev)
{
}

static inline
void mlo_process_ml_reconfig_ie(struct wlan_objmgr_vdev *vdev,
				struct scan_cache_entry *scan_entry,
				uint8_t *ml_ie, qdf_size_t ml_ie_len,
				struct mlo_partner_info *partner_info)
{ }
#ifdef WLAN_FEATURE_11BE_MLO
static inline QDF_STATUS
mlo_get_link_state_context(struct wlan_objmgr_psoc *psoc,
			   get_ml_link_state_cb *resp_cb,
			   void **context, uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static inline
void wlan_mlo_send_vdev_pause(struct wlan_objmgr_psoc *psoc,
			      struct wlan_objmgr_vdev *vdev,
			      uint16_t session_id,
			      uint16_t vdev_pause_dur)
{}
#endif

#if defined(WLAN_FEATURE_11BE_MLO_ADV_FEATURE) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * mlo_defer_set_keys: Defer MLO set keys for link
 * @vdev: vdev obj
 * @link_id: link_id
 * @value: bool true or false
 * Return: none
 */
void mlo_defer_set_keys(struct wlan_objmgr_vdev *vdev,
			uint8_t link_id, bool value);

/**
 * mlo_is_set_key_defered: Verify whether the set key deferred for the link
 * @vdev: vdev obj
 * @link_id: link_id
 * Return: boolean value true or false
 */
bool mlo_is_set_key_defered(struct wlan_objmgr_vdev *vdev,
			    uint8_t link_id);

/**
 * mlo_is_any_link_disconnecting: Check if any ML link is disconnecting
 * @vdev: vdev obj
 *
 * Check connection manager state machine if any ML link is disconnecting
 *
 * Return: boolean value true or false
 */
bool mlo_is_any_link_disconnecting(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_set_chan_switch_in_progress: Set/clear the flag upon CSA at MLD level
 * @vdev: vdev obj
 * @val: Carries true if CSA is started on any of the ML links. This carries
 *       false once CSA is completed.
 *
 * This API is to set/clear the flag ml_chan_switch_in_progress upon
 * CSA start/CSA completion on any of the ML links.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_set_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev, bool val);

/**
 * mlo_is_chan_switch_in_progress: Check if ML level CSA flag is set
 * @vdev: vdev obj
 *
 * This API is to check the flag ml_chan_switch_in_progress upon
 * CSA start/CSA completion on any of the ML links.
 *
 * Return: True if flag ml_chan_switch_in_progress is set, false otherwise
 */
bool mlo_is_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev);
#else
static inline
void mlo_defer_set_keys(struct wlan_objmgr_vdev *vdev,
			uint8_t link_id, bool value)
{
}

static inline
bool mlo_is_set_key_defered(struct wlan_objmgr_vdev *vdev,
			    uint8_t link_id)
{
	return false;
}

static inline
bool mlo_is_any_link_disconnecting(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline QDF_STATUS
mlo_set_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev, bool val)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool
mlo_is_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * mlo_mgr_get_per_link_chan_info: Get wlan channel info per link id
 * @vdev: vdev obj
 * @link_id: link id
 * @chan_info: wlan channel info buffer
 *
 * Return: zero for success, non-zero for failure
 */
int mlo_mgr_get_per_link_chan_info(struct wlan_objmgr_vdev *vdev, int link_id,
				   struct wlan_channel *chan_info);
#else
static inline int
mlo_mgr_get_per_link_chan_info(struct wlan_objmgr_vdev *vdev, int link_id,
			       struct wlan_channel *chan_info)
{
	return -EINVAL;
}
#endif

#endif
