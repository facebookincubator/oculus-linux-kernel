/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_cmn.h>
#include "wlan_tdls_cfg_api.h"

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

void  wlan_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_vdev_priv_obj *vdev_priv_obj;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = tdls_get_vdev(psoc, WLAN_TDLS_NB_ID);
	if (!vdev)
		return;

	vdev_priv_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
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

	tdls_debug("Wait for tdls teardown completion. Timeout %u ms",
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
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_TDLS_NB_ID);
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
