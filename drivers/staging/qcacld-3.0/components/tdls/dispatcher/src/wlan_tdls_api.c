/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains tdls link teardown definitions
 */

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_tdls_api.h"
#include "../../core/src/wlan_tdls_main.h"
#include "../../core/src/wlan_tdls_ct.h"
#include "../../core/src/wlan_tdls_mgmt.h"
#include "wlan_tdls_peer.h"
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_cmn.h>
#include "wlan_tdls_cfg_api.h"
#include "wlan_policy_mgr_api.h"

static QDF_STATUS tdls_teardown_flush_cb(struct scheduler_msg *msg)
{
	struct tdls_link_teardown *tdls_teardown = msg->bodyptr;
	struct wlan_objmgr_psoc *psoc = tdls_teardown->psoc;

	wlan_objmgr_psoc_release_ref(psoc, WLAN_TDLS_SB_ID);
	qdf_mem_free(tdls_teardown);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_tdls_teardown_links(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0, };
	struct tdls_link_teardown *link_teardown;

	link_teardown = qdf_mem_malloc(sizeof(*link_teardown));
	if (!link_teardown)
		return QDF_STATUS_E_NOMEM;

	wlan_objmgr_psoc_get_ref(psoc, WLAN_TDLS_SB_ID);
	link_teardown->psoc = psoc;
	msg.bodyptr = link_teardown;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = tdls_teardown_flush_cb;
	msg.type = TDLS_CMD_TEARDOWN_LINKS;

	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post msg fail, %d", status);
		wlan_objmgr_psoc_release_ref(psoc, WLAN_TDLS_SB_ID);
		qdf_mem_free(link_teardown);
	}

	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO
bool wlan_tdls_is_fw_11be_mlo_capable(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_TDLS);
	if (!soc_obj) {
		tdls_err("Failed to get tdls psoc component");
		return false;
	}
	tdls_debug("FW 11BE capability %d", soc_obj->fw_tdls_mlo_capable);

	return soc_obj->fw_tdls_mlo_capable;
}
#endif

static void  wlan_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev)
{
	struct tdls_vdev_priv_obj *vdev_priv_obj;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *tdls_vdev;

	tdls_vdev = tdls_get_vdev(psoc, WLAN_TDLS_NB_ID);
	if (!tdls_vdev)
		return;

	vdev_priv_obj = wlan_vdev_get_tdls_vdev_obj(tdls_vdev);
	if (!vdev_priv_obj) {
		tdls_err("vdev priv is NULL");
		goto release_ref;
	}

	qdf_event_reset(&vdev_priv_obj->tdls_teardown_comp);

	status = wlan_tdls_teardown_links(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("wlan_tdls_teardown_links failed err %d", status);
		goto release_ref;
	}

	tdls_debug("vdev:%d Wait for tdls teardown completion. Timeout %u ms",
		   wlan_vdev_get_id(tdls_vdev),
		   WAIT_TIME_FOR_TDLS_TEARDOWN_LINKS);

	status = qdf_wait_for_event_completion(
					&vdev_priv_obj->tdls_teardown_comp,
					WAIT_TIME_FOR_TDLS_TEARDOWN_LINKS);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err(" Teardown Completion timed out %d", status);
		goto release_ref;
	}

	tdls_debug("TDLS teardown completion status %d ", status);

release_ref:
	wlan_objmgr_vdev_release_ref(tdls_vdev,
				     WLAN_TDLS_NB_ID);
}

void  wlan_tdls_check_and_teardown_links_sync(struct wlan_objmgr_psoc *psoc,
					      struct wlan_objmgr_vdev *vdev)
{
	uint8_t sta_count;
	enum QDF_OPMODE opmode;
	bool tgt_tdls_concurrency_supported;

	tgt_tdls_concurrency_supported =
		wlan_psoc_nif_fw_ext2_cap_get(psoc,
					      WLAN_TDLS_CONCURRENCIES_SUPPORT);
	/* Don't initiate teardown in case of STA + P2P Client concurreny */
	sta_count = policy_mgr_mode_specific_connection_count(psoc,
							      PM_STA_MODE,
							      NULL);
	opmode = wlan_vdev_mlme_get_opmode(vdev);
	if (tgt_tdls_concurrency_supported && opmode == QDF_P2P_CLIENT_MODE &&
	    sta_count) {
		tdls_debug("Don't teardown tdls for existing STA vdev");
		return;
	}

	wlan_tdls_teardown_links_sync(psoc, vdev);
}

#ifdef WLAN_FEATURE_TDLS_CONCURRENCIES
static void wlan_tdls_handle_sap_start(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};

	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_START_BSS;
	msg.bodyptr = psoc;
	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post start bss msg fail");
		return;
	}
}

void wlan_tdls_notify_channel_switch_complete(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *tdls_vdev;
	struct tdls_vdev_priv_obj *tdls_vdev_priv;
	struct tdls_soc_priv_obj *tdls_soc_priv;
	QDF_STATUS status;

	tdls_vdev = tdls_get_vdev(psoc, WLAN_TDLS_NB_ID);
	if (!tdls_vdev)
		return;

	status = tdls_get_vdev_objects(tdls_vdev, &tdls_vdev_priv,
				       &tdls_soc_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to get TDLS objects");
		goto exit;
	}

	tdls_debug("CSA complete");
	/*
	 * Channel Switch can cause SCC -> MCC switch on
	 * STA vdev. Disable TDLS if CSA causes STA vdev to be in MCC with
	 * other vdev.
	 */
	if (!tdls_is_concurrency_allowed(psoc)) {
		tdls_disable_offchan_and_teardown_links(tdls_vdev);
		tdls_debug("Disable the tdls in FW after CSA");
	} else {
		tdls_process_enable_for_vdev(tdls_vdev);
		tdls_set_tdls_offchannelmode(tdls_vdev, ENABLE_CHANSWITCH);
	}

exit:
	wlan_objmgr_vdev_release_ref(tdls_vdev, WLAN_TDLS_NB_ID);
}

static QDF_STATUS
wlan_tdls_post_set_off_channel_mode(struct wlan_objmgr_psoc *psoc,
				    uint8_t off_chan_mode)
{
	struct wlan_objmgr_vdev *tdls_vdev;
	struct tdls_vdev_priv_obj *tdls_vdev_priv;
	struct tdls_soc_priv_obj *tdls_soc_priv;
	struct tdls_set_offchanmode *req;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	tdls_vdev = tdls_get_vdev(psoc, WLAN_TDLS_NB_ID);
	if (!tdls_vdev)
		return QDF_STATUS_E_FAILURE;

	status = tdls_get_vdev_objects(tdls_vdev, &tdls_vdev_priv,
				       &tdls_soc_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(tdls_vdev, WLAN_TDLS_NB_ID);
		tdls_err("Failed to get TDLS objects");
		return QDF_STATUS_E_FAILURE;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		wlan_objmgr_vdev_release_ref(tdls_vdev, WLAN_TDLS_NB_ID);
		return QDF_STATUS_E_NOMEM;
	}

	req->offchan_mode = off_chan_mode;
	req->vdev = tdls_vdev;
	req->callback = wlan_tdls_offchan_parms_callback;

	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_SET_OFFCHANMODE;
	msg.bodyptr = req;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS, QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post set offchanmode msg fail");
		wlan_objmgr_vdev_release_ref(tdls_vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(req);
	}

	return QDF_STATUS_SUCCESS;
}

void wlan_tdls_notify_channel_switch_start(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev)
{
	tdls_debug("Send Channel Switch start to TDLS module");
	wlan_tdls_post_set_off_channel_mode(psoc, DISABLE_ACTIVE_CHANSWITCH);
}

void wlan_tdls_handle_p2p_client_connect(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_vdev *vdev)
{
	if (!policy_mgr_get_connection_count(psoc))
		return;

	/*
	 * Disable TDLS off-channel when P2P CLI comes up as
	 * 3rd interface. It will be re-enabled based on the
	 * concurrency once P2P connection is complete
	 */
	wlan_tdls_post_set_off_channel_mode(psoc, DISABLE_ACTIVE_CHANSWITCH);
}
#else
static inline void
wlan_tdls_handle_sap_start(struct wlan_objmgr_psoc *psoc)
{}
#endif

void wlan_tdls_notify_start_bss(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev)
{
	if (tdls_is_concurrency_allowed(psoc)) {
		wlan_tdls_handle_sap_start(psoc);
		return;
	}

	wlan_tdls_check_and_teardown_links_sync(psoc, vdev);
}

void wlan_tdls_notify_start_bss_failure(struct wlan_objmgr_psoc *psoc)
{
	tdls_notify_decrement_session(psoc);
}

static QDF_STATUS tdls_notify_flush_cb(struct scheduler_msg *msg)
{
	struct tdls_sta_notify_params *notify = msg->bodyptr;
	struct wlan_objmgr_vdev *vdev = notify->vdev;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(notify);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
tdls_notify_disconnect(struct tdls_sta_notify_params *notify_info)
{
	struct scheduler_msg msg = {0, };
	struct tdls_sta_notify_params *notify;
	QDF_STATUS status;

	if (!notify_info || !notify_info->vdev) {
		tdls_err("notify_info %pK", notify_info);
		return QDF_STATUS_E_NULL_VALUE;
	}

	tdls_debug("Enter ");

	notify = qdf_mem_malloc(sizeof(*notify));
	if (!notify) {
		wlan_objmgr_vdev_release_ref(notify_info->vdev, WLAN_TDLS_NB_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	*notify = *notify_info;

	msg.bodyptr = notify;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_NOTIFY_STA_DISCONNECTION;
	msg.flush_callback = tdls_notify_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(notify);
	}

	tdls_debug("Exit ");

	return QDF_STATUS_SUCCESS;
}

void wlan_tdls_notify_sta_disconnect(uint8_t vdev_id,
				     bool lfr_roam, bool user_disconnect,
				     struct wlan_objmgr_vdev *vdev)
{
	struct tdls_sta_notify_params notify_info = {0};
	QDF_STATUS status;

	if (!vdev) {
		tdls_err("vdev is NULL");
		return;
	}

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		return;
	}

	notify_info.session_id = vdev_id;
	notify_info.lfr_roam = lfr_roam;
	notify_info.tdls_chan_swit_prohibited = false;
	notify_info.tdls_prohibited = false;
	notify_info.vdev = vdev;
	notify_info.user_disconnect = user_disconnect;
	tdls_notify_disconnect(&notify_info);
}

static QDF_STATUS
tdls_notify_connect(struct tdls_sta_notify_params *notify_info)
{
	struct scheduler_msg msg = {0, };
	struct tdls_sta_notify_params *notify;
	QDF_STATUS status;

	if (!notify_info || !notify_info->vdev) {
		tdls_err("notify_info %pK", notify_info);
		return QDF_STATUS_E_NULL_VALUE;
	}
	tdls_debug("Enter ");

	notify = qdf_mem_malloc(sizeof(*notify));
	if (!notify) {
		wlan_objmgr_vdev_release_ref(notify_info->vdev,
					     WLAN_TDLS_NB_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	*notify = *notify_info;

	msg.bodyptr = notify;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_NOTIFY_STA_CONNECTION;
	msg.flush_callback = tdls_notify_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(notify);
	}

	tdls_debug("Exit ");
	return status;
}

void
wlan_tdls_notify_sta_connect(uint8_t session_id,
			     bool tdls_chan_swit_prohibited,
			     bool tdls_prohibited,
			     struct wlan_objmgr_vdev *vdev)
{
	struct tdls_sta_notify_params notify_info = {0};
	QDF_STATUS status;

	if (!vdev) {
		tdls_err("vdev is NULL");
		return;
	}
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		return;
	}

	notify_info.session_id = session_id;
	notify_info.vdev = vdev;
	notify_info.tdls_chan_swit_prohibited = tdls_chan_swit_prohibited;
	notify_info.tdls_prohibited = tdls_prohibited;
	tdls_notify_connect(&notify_info);
}

#ifdef FEATURE_SET
void wlan_tdls_get_features_info(struct wlan_objmgr_psoc *psoc,
				 struct wlan_tdls_features *tdls_feature_set)
{
	cfg_tdls_get_support_enable(psoc, &tdls_feature_set->enable_tdls);
	if (tdls_feature_set->enable_tdls) {
		cfg_tdls_get_off_channel_enable(
				psoc,
				&tdls_feature_set->enable_tdls_offchannel);
		tdls_feature_set->max_tdls_peers =
					cfg_tdls_get_max_peer_count(psoc);
		tdls_feature_set->enable_tdls_capability_enhance = true;
	}
}
#endif

void wlan_tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr)
{
	tdls_update_tx_pkt_cnt(vdev, mac_addr);
}

void wlan_tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr,
				 struct qdf_mac_addr *dest_mac_addr)
{
	tdls_update_rx_pkt_cnt(vdev, mac_addr, dest_mac_addr);
}

void wlan_tdls_increment_discovery_attempts(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id,
					    uint8_t *peer_addr)
{
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_peer *peer;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_TDLS_NB_ID);
	if (!vdev)
		return;

	status = tdls_get_vdev_objects(vdev, &tdls_vdev_obj, &tdls_soc_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to get TDLS objects");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
		return;
	}

	peer = tdls_get_peer(tdls_vdev_obj, peer_addr);
	if (!peer) {
		tdls_err("tdls_peer is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
		return;
	}

	peer->discovery_attempt++;
	tdls_debug("vdev:%d peer: " QDF_MAC_ADDR_FMT " discovery attempts:%d ", vdev_id,
		   QDF_MAC_ADDR_REF(peer_addr), peer->discovery_attempt);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
}
