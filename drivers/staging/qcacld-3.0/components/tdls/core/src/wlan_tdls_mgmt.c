/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_tdls_mgmt.c
 *
 * TDLS management frames implementation
 */

#include "wlan_tdls_main.h"
#include "wlan_tdls_tgt_api.h"
#include <wlan_serialization_api.h>
#include "wlan_mgmt_txrx_utils_api.h"
#include "wlan_tdls_peer.h"
#include "wlan_tdls_ct.h"
#include "wlan_tdls_mgmt.h"
#include "wlan_policy_mgr_api.h"
#include <wlan_reg_services_api.h>
#include <wlan_mlo_mgr_sta.h>
#include "wlan_mlo_link_force.h"

static
const char *const tdls_action_frames_type[] = { "TDLS Setup Request",
					 "TDLS Setup Response",
					 "TDLS Setup Confirm",
					 "TDLS Teardown",
					 "TDLS Peer Traffic Indication",
					 "TDLS Channel Switch Request",
					 "TDLS Channel Switch Response",
					 "TDLS Peer PSM Request",
					 "TDLS Peer PSM Response",
					 "TDLS Peer Traffic Response",
					 "TDLS Discovery Request"};

QDF_STATUS tdls_set_rssi(struct wlan_objmgr_vdev *vdev,
			 uint8_t *mac, int8_t rssi)
{
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_peer *curr_peer;

	if (!vdev || !mac) {
		tdls_err("null pointer");
		return QDF_STATUS_E_INVAL;
	}

	tdls_debug("rssi %d, peer " QDF_MAC_ADDR_FMT,
		   rssi, QDF_MAC_ADDR_REF(mac));

	tdls_vdev = wlan_objmgr_vdev_get_comp_private_obj(
			vdev, WLAN_UMAC_COMP_TDLS);

	if (!tdls_vdev) {
		tdls_err("null tdls vdev");
		return QDF_STATUS_E_EXISTS;
	}

	curr_peer = tdls_find_peer(tdls_vdev, mac);
	if (!curr_peer) {
		tdls_debug("null peer");
		return QDF_STATUS_E_EXISTS;
	}

	curr_peer->rssi = rssi;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
struct wlan_objmgr_vdev *
tdls_mlo_get_tdls_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	return wlan_mlo_get_tdls_link_vdev(vdev);
}

void
tdls_set_remain_links_unforce(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_objmgr_vdev *mlo_vdev;
	int i;

	/* TDLS link is selected, unforce link for other vdevs */
	mlo_dev_ctx = vdev->mlo_dev_ctx;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		mlo_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!mlo_vdev || mlo_vdev == vdev)
			continue;

		tdls_debug("try to set vdev %d to unforce",
			   wlan_vdev_get_id(mlo_vdev));
		tdls_set_link_unforce(mlo_vdev);
	}
}

QDF_STATUS
tdls_process_mlo_cal_tdls_link_score(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_objmgr_vdev *mlo_vdev;
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_rx_mgmt_frame *rx_mgmt;
	uint32_t score;
	int i;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		score = 0;
		mlo_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!mlo_vdev)
			continue;
		tdls_vdev =
		     wlan_objmgr_vdev_get_comp_private_obj(mlo_vdev,
							   WLAN_UMAC_COMP_TDLS);
		tdls_vdev->link_score = 0;
		rx_mgmt = tdls_vdev->rx_mgmt;
		if (!rx_mgmt)
			continue;

		switch (wlan_reg_freq_to_band(rx_mgmt->rx_freq)) {
		case REG_BAND_2G:
			score += 50;
			break;
		case REG_BAND_5G:
			score += 70;
			break;
		case REG_BAND_6G:
			score += 80;
			break;
		default:
			score += 40;
			break;
		}

		tdls_vdev->link_score = score;
		status = QDF_STATUS_SUCCESS;
	}

	return status;
}

/**
 * tdls_check_wait_more() - wait until the timer timeout if necessary
 * @vdev: vdev object
 *
 * Return: true if need to wait else false
 */
static bool tdls_check_wait_more(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_objmgr_vdev *mlo_vdev;
	struct tdls_vdev_priv_obj *tdls_vdev;
	/* expect response number */
	int expect_num;
	/* received response number */
	int receive_num;
	int i;

	expect_num = 0;
	receive_num = 0;
	mlo_dev_ctx = vdev->mlo_dev_ctx;
	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		mlo_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!mlo_vdev)
			continue;

		expect_num++;

		tdls_vdev =
		     wlan_objmgr_vdev_get_comp_private_obj(mlo_vdev,
							   WLAN_UMAC_COMP_TDLS);
		if (tdls_vdev->rx_mgmt)
			receive_num++;
	}

	/* +1 means the one received last has not been recorded */
	if (expect_num > receive_num + 1)
		return true;
	else
		return false;
}

struct wlan_objmgr_vdev *
tdls_process_mlo_choice_tdls_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_objmgr_vdev *mlo_vdev;
	struct wlan_objmgr_vdev *select_vdev = NULL;
	struct tdls_vdev_priv_obj *tdls_vdev;
	uint32_t score = 0;
	int i;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		mlo_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!mlo_vdev)
			continue;
		tdls_vdev =
		     wlan_objmgr_vdev_get_comp_private_obj(mlo_vdev,
							   WLAN_UMAC_COMP_TDLS);
		if (score < tdls_vdev->link_score) {
			select_vdev = mlo_vdev;
			score = tdls_vdev->link_score;
		}
	}

	/* free the memory except the choice one */
	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		mlo_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!mlo_vdev || mlo_vdev == select_vdev)
			continue;

		tdls_vdev =
		     wlan_objmgr_vdev_get_comp_private_obj(mlo_vdev,
							   WLAN_UMAC_COMP_TDLS);
		qdf_mem_free(tdls_vdev->rx_mgmt);
		tdls_vdev->rx_mgmt = NULL;
		tdls_vdev->link_score = 0;
	}

	return select_vdev;
}

static struct tdls_vdev_priv_obj
*tdls_get_correct_vdev(struct tdls_vdev_priv_obj *tdls_vdev,
		       struct tdls_rx_mgmt_frame *rx_mgmt)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	struct tdls_link_identifier *linkid_ie;
	uint8_t vdev_id;
	uint8_t *ies;
	uint32_t ie_len;
	uint8_t elem_id_param = WLAN_ELEMID_LINK_IDENTIFIER;

	if (!tdls_vdev || !rx_mgmt)
		return NULL;

	ies = &rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_TDLS_IE_OFFSET];
	ie_len = rx_mgmt->frame_len - TDLS_PUBLIC_ACTION_FRAME_TDLS_IE_OFFSET;

	linkid_ie =
	  (struct tdls_link_identifier *)wlan_get_ie_ptr_from_eid(elem_id_param,
								  ies, ie_len);
	if (!linkid_ie)
		return tdls_vdev;

	pdev = wlan_vdev_get_pdev(tdls_vdev->vdev);
	if (!pdev)
		return tdls_vdev;

	if (!wlan_get_connected_vdev_by_bssid(pdev,
					      linkid_ie->bssid, &vdev_id))
		return tdls_vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_TDLS_NB_ID);
	if (!vdev)
		return tdls_vdev;

	tdls_vdev = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							  WLAN_UMAC_COMP_TDLS);
	rx_mgmt->vdev_id = vdev_id;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	tdls_debug("received discovery response on vdev %d", rx_mgmt->vdev_id);

	return tdls_vdev;
}

static bool tdls_check_peer_mlo_dev(struct wlan_objmgr_vdev *vdev,
				    struct tdls_rx_mgmt_frame *rx_mgmt)
{
	uint8_t *ies;
	const uint8_t *ie;
	uint32_t ie_len;
	const uint8_t ext_id_param = WLAN_EXTN_ELEMID_MULTI_LINK;
	uint8_t elem_id_param = WLAN_ELEMID_LINK_IDENTIFIER;

	if (!vdev || !rx_mgmt)
		return QDF_STATUS_E_INVAL;

	ies = &rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_TDLS_IE_OFFSET];
	ie_len = rx_mgmt->frame_len - TDLS_PUBLIC_ACTION_FRAME_TDLS_IE_OFFSET;

	ie = wlan_get_ie_ptr_from_eid(elem_id_param, ies, ie_len);
	if (ie)
		qdf_trace_hex_dump(QDF_MODULE_ID_TDLS, QDF_TRACE_LEVEL_DEBUG,
				   (void *)&ie[0], ie[1] + 2);

	ie = wlan_get_ext_ie_ptr_from_ext_id(&ext_id_param,
					     1, ies, ie_len);
	if (ie)
		return true;

	return false;
}

static QDF_STATUS
tdls_process_mlo_rx_mgmt_sync(struct tdls_soc_priv_obj *tdls_soc,
			      struct tdls_vdev_priv_obj *tdls_vdev,
			      struct tdls_rx_mgmt_frame *rx_mgmt)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_vdev *mlo_vdev;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	bool peer_mlo;
	bool waitmore = false;
	uint8_t i;

	vdev = tdls_vdev->vdev;
	peer_mlo = tdls_check_peer_mlo_dev(vdev, rx_mgmt);
	if (!peer_mlo) {
		mlo_dev_ctx = vdev->mlo_dev_ctx;
		/* stop all timers */
		for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			mlo_vdev = mlo_dev_ctx->wlan_vdev_list[i];
			if (!mlo_vdev)
				continue;

			tdls_vdev =
			       wlan_objmgr_vdev_get_comp_private_obj(mlo_vdev,
							   WLAN_UMAC_COMP_TDLS);
			tdls_vdev->discovery_sent_cnt = 0;
			if (QDF_TIMER_STATE_RUNNING ==
			    qdf_mc_timer_get_current_state(
					    &tdls_vdev->peer_discovery_timer)) {
				qdf_mc_timer_stop(
					      &tdls_vdev->peer_discovery_timer);
				qdf_atomic_dec(&tdls_soc->timer_cnt);
			}
		}
		tdls_debug("peer is legacy device, timer_cnt %d",
			   qdf_atomic_read(&tdls_soc->timer_cnt));

		status = QDF_STATUS_E_INVAL;
	} else {
		tdls_debug("peer is mlo device, timer_cnt %d",
			   qdf_atomic_read(&tdls_soc->timer_cnt));

		/* If peer is MLD, it uses ml mac address to send the
		 * disconvery response, it needs to find out the
		 * corresponding vdev per the bssid in link identifier ie.
		 */
		tdls_vdev = tdls_get_correct_vdev(tdls_vdev, rx_mgmt);
		status = QDF_STATUS_TDLS_MLO_SYNC;
		if (!tdls_vdev || tdls_vdev->rx_mgmt) {
			tdls_err("rx dup tdls discovery resp on same vdev.");
			goto exit;
		}

		if (qdf_atomic_read(&tdls_soc->timer_cnt) == 1)
			waitmore = tdls_check_wait_more(vdev);

		if (waitmore) {
			/* do not stop the timer */
			tdls_debug("wait more tdls response");
		} else {
			tdls_vdev->discovery_sent_cnt = 0;
			qdf_mc_timer_stop(&tdls_vdev->peer_discovery_timer);
			qdf_atomic_dec(&tdls_soc->timer_cnt);
		}

		tdls_vdev->rx_mgmt = qdf_mem_malloc_atomic(sizeof(*rx_mgmt) +
							   rx_mgmt->frame_len);
		if (tdls_vdev->rx_mgmt) {
			tdls_vdev->rx_mgmt->frame_len = rx_mgmt->frame_len;
			tdls_vdev->rx_mgmt->rx_freq = rx_mgmt->rx_freq;
			tdls_vdev->rx_mgmt->vdev_id = rx_mgmt->vdev_id;
			tdls_vdev->rx_mgmt->frm_type = rx_mgmt->frm_type;
			tdls_vdev->rx_mgmt->rx_rssi = rx_mgmt->rx_rssi;
			qdf_mem_copy(tdls_vdev->rx_mgmt->buf,
				     rx_mgmt->buf, rx_mgmt->frame_len);
		} else {
			tdls_err("alloc rx mgmt buf error");
		}

		if (qdf_atomic_read(&tdls_soc->timer_cnt) == 0) {
			tdls_process_mlo_cal_tdls_link_score(vdev);
			status = QDF_STATUS_SUCCESS;
		}
	}

exit:
	return status;
}

void tdls_set_no_force_vdev(struct wlan_objmgr_vdev *vdev, bool flag)
{
	uint8_t i;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *mlo_vdev;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	bool is_mlo_vdev;
	struct ml_nlink_change_event data;
	QDF_STATUS status;

	if (!vdev)
		return;

	is_mlo_vdev = wlan_vdev_mlme_is_mlo_vdev(vdev);
	if (!is_mlo_vdev)
		return;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;

	qdf_mem_zero(&data, sizeof(data));

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		mlo_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!mlo_vdev)
			continue;

		/* flag: true means no force all vdevs,
		 * false means except the current one
		 */
		if (!flag && (mlo_vdev == vdev))
			continue;
		data.evt.tdls.link_bitmap |=
				1 << wlan_vdev_get_link_id(mlo_vdev);
		data.evt.tdls.mlo_vdev_lst[data.evt.tdls.vdev_count] =
				wlan_vdev_get_id(mlo_vdev);
		data.evt.tdls.vdev_count++;
	}

	data.evt.tdls.mode = MLO_LINK_FORCE_MODE_NO_FORCE;
	data.evt.tdls.reason = MLO_LINK_FORCE_REASON_TDLS;
	status = ml_nlink_conn_change_notify(psoc,
					     wlan_vdev_get_id(vdev),
					     ml_nlink_tdls_request_evt,
					     &data);
}
#else
struct wlan_objmgr_vdev *
tdls_mlo_get_tdls_link_vdev(struct wlan_objmgr_vdev *vdev)
{
	return NULL;
}

void
tdls_set_remain_links_unforce(struct wlan_objmgr_vdev *vdev)
{
}

QDF_STATUS
tdls_process_mlo_cal_tdls_link_score(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

struct wlan_objmgr_vdev *
tdls_process_mlo_choice_tdls_vdev(struct wlan_objmgr_vdev *vdev)
{
	return NULL;
}

static struct tdls_vdev_priv_obj
*tdls_get_correct_vdev(struct tdls_vdev_priv_obj *tdls_vdev,
		       struct tdls_rx_mgmt_frame *rx_mgmt)
{
	return NULL;
}

static QDF_STATUS
tdls_process_mlo_rx_mgmt_sync(struct tdls_soc_priv_obj *tdls_soc,
			      struct tdls_vdev_priv_obj *tdls_vdev,
			      struct tdls_rx_mgmt_frame *rx_mgmt)
{
	return QDF_STATUS_SUCCESS;
}

void tdls_set_no_force_vdev(struct wlan_objmgr_vdev *vdev, bool flag)
{
}
#endif

static bool
tdls_needs_wait_discovery_response(struct wlan_objmgr_vdev *vdev,
				   struct tdls_soc_priv_obj *tdls_soc)
{
	bool is_mlo_vdev;
	bool wait = false;

	is_mlo_vdev = wlan_vdev_mlme_is_mlo_vdev(vdev);
	if (is_mlo_vdev && qdf_atomic_read(&tdls_soc->timer_cnt))
		wait = true;

	return wait;
}

/**
 * tdls_process_rx_mgmt() - process tdls rx mgmt frames
 * @rx_mgmt_event: tdls rx mgmt event
 * @tdls_vdev: tdls vdev object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tdls_process_rx_mgmt(
	struct tdls_rx_mgmt_event *rx_mgmt_event,
	struct tdls_vdev_priv_obj *tdls_vdev)
{
	struct tdls_rx_mgmt_frame *rx_mgmt;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	uint8_t *mac;
	enum tdls_actioncode action_frame_type;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_vdev *tdls_link_vdev;
	bool tdls_vdev_select = false;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (!rx_mgmt_event)
		return QDF_STATUS_E_INVAL;

	tdls_soc_obj = rx_mgmt_event->tdls_soc_obj;
	rx_mgmt = rx_mgmt_event->rx_mgmt;

	if (!tdls_soc_obj || !rx_mgmt) {
		tdls_err("invalid psoc object or rx mgmt");
		return QDF_STATUS_E_INVAL;
	}

	vdev = tdls_vdev->vdev;
	tdls_debug("received mgmt on vdev %d", wlan_vdev_get_id(vdev));
	tdls_debug("soc:%pK, frame_len:%d, rx_freq:%d, vdev_id:%d, frm_type:%d, rx_rssi:%d, buf:%pK",
		   tdls_soc_obj->soc, rx_mgmt->frame_len,
		   rx_mgmt->rx_freq, rx_mgmt->vdev_id, rx_mgmt->frm_type,
		   rx_mgmt->rx_rssi, rx_mgmt->buf);

	if (rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_OFFSET + 1] ==
	    TDLS_PUBLIC_ACTION_DISC_RESP) {
		if (tdls_needs_wait_discovery_response(vdev, tdls_soc_obj)) {
			status = tdls_process_mlo_rx_mgmt_sync(tdls_soc_obj,
							       tdls_vdev,
							       rx_mgmt);
			if (status == QDF_STATUS_TDLS_MLO_SYNC) {
				return QDF_STATUS_SUCCESS;
			} else if (status == QDF_STATUS_SUCCESS) {
				vdev = tdls_process_mlo_choice_tdls_vdev(vdev);
				tdls_vdev =
				     wlan_objmgr_vdev_get_comp_private_obj(vdev,
							   WLAN_UMAC_COMP_TDLS);
				rx_mgmt = tdls_vdev->rx_mgmt;
				tdls_vdev_select = true;
				tdls_debug("choice vdev %d as tdls vdev",
					   wlan_vdev_get_id(vdev));
			} else {
				tdls_vdev = tdls_get_correct_vdev(tdls_vdev,
								  rx_mgmt);
				if (!tdls_vdev)
					return QDF_STATUS_SUCCESS;
				vdev = tdls_vdev->vdev;
			}
		} else {
			if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
				tdls_link_vdev =
					      tdls_mlo_get_tdls_link_vdev(vdev);
				tdls_vdev =
				      tdls_get_correct_vdev(tdls_vdev, rx_mgmt);
				if (!tdls_link_vdev || !tdls_vdev) {
					tdls_debug("not expected frame");
					return QDF_STATUS_SUCCESS;
				}
				if (tdls_link_vdev != tdls_vdev->vdev) {
					tdls_debug("not forward to userspace");
					return QDF_STATUS_SUCCESS;
				}
				rx_mgmt->vdev_id =
					       wlan_vdev_get_id(tdls_link_vdev);
				vdev = tdls_link_vdev;
			}
		}

		/* this is mld mac address for mlo case*/
		mac = &rx_mgmt->buf[TDLS_80211_PEER_ADDR_OFFSET];
		tdls_notice("[TDLS] TDLS Discovery Response,"
			    QDF_MAC_ADDR_FMT " RSSI[%d] <--- OTA",
			    QDF_MAC_ADDR_REF(mac), rx_mgmt->rx_rssi);

		tdls_debug("discovery resp on vdev %d", wlan_vdev_get_id(vdev));
		tdls_recv_discovery_resp(tdls_vdev, mac);
		tdls_set_rssi(tdls_vdev->vdev, mac, rx_mgmt->rx_rssi);
	}

	if (rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_OFFSET] ==
	    TDLS_ACTION_FRAME) {
		action_frame_type =
			rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_OFFSET + 1];
		if (action_frame_type >= TDLS_ACTION_FRAME_TYPE_MAX) {
			tdls_debug("[TDLS] unknown[%d] <--- OTA",
				   action_frame_type);
		} else {
			tdls_notice("[TDLS] %s <--- OTA",
				   tdls_action_frames_type[action_frame_type]);
		}
	}

	/* tdls_soc_obj->tdls_rx_cb ==> wlan_cfg80211_tdls_rx_callback() */
	if (tdls_soc_obj && tdls_soc_obj->tdls_rx_cb)
		tdls_soc_obj->tdls_rx_cb(tdls_soc_obj->tdls_rx_cb_data,
					 rx_mgmt);
	else
		tdls_debug("rx mgmt, but no valid up layer callback");

	if (tdls_vdev_select && tdls_vdev->rx_mgmt) {
		qdf_mem_free(tdls_vdev->rx_mgmt);
		tdls_vdev->rx_mgmt = NULL;
		tdls_vdev->link_score = 0;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_process_rx_frame(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev;
	struct tdls_rx_mgmt_event *tdls_rx;
	struct tdls_vdev_priv_obj *tdls_vdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!(msg->bodyptr)) {
		tdls_err("invalid message body");
		return QDF_STATUS_E_INVAL;
	}

	tdls_rx = (struct tdls_rx_mgmt_event *) msg->bodyptr;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(tdls_rx->tdls_soc_obj->soc,
				tdls_rx->rx_mgmt->vdev_id, WLAN_TDLS_NB_ID);

	if (vdev) {
		tdls_debug("tdls rx mgmt frame received");
		tdls_vdev = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							WLAN_UMAC_COMP_TDLS);
		if (tdls_vdev)
			status = tdls_process_rx_mgmt(tdls_rx, tdls_vdev);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	}

	qdf_mem_free(tdls_rx->rx_mgmt);
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;

	return status;
}

QDF_STATUS tdls_mgmt_rx_ops(struct wlan_objmgr_psoc *psoc,
	bool isregister)
{
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info;
	QDF_STATUS status;
	int num_of_entries;

	tdls_debug("psoc:%pK, is register rx:%d", psoc, isregister);

	frm_cb_info.frm_type = MGMT_ACTION_TDLS_DISCRESP;
	frm_cb_info.mgmt_rx_cb = tgt_tdls_mgmt_frame_rx_cb;
	num_of_entries = 1;

	if (isregister)
		status = wlan_mgmt_txrx_register_rx_cb(psoc,
				WLAN_UMAC_COMP_TDLS, &frm_cb_info,
				num_of_entries);
	else
		status = wlan_mgmt_txrx_deregister_rx_cb(psoc,
				WLAN_UMAC_COMP_TDLS, &frm_cb_info,
				num_of_entries);

	return status;
}

static QDF_STATUS
tdls_internal_send_mgmt_tx_done(struct tdls_action_frame_request *req,
				QDF_STATUS status)
{
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct tdls_osif_indication indication;

	if (!req || !req->vdev)
		return QDF_STATUS_E_NULL_VALUE;

	indication.status = status;
	indication.vdev = req->vdev;
	tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(req->vdev);
	if (tdls_soc_obj && tdls_soc_obj->tdls_event_cb)
		tdls_soc_obj->tdls_event_cb(tdls_soc_obj->tdls_evt_cb_data,
			TDLS_EVENT_MGMT_TX_ACK_CNF, &indication);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tdls_activate_send_mgmt_request_flush_cb(
	struct scheduler_msg *msg)
{
	struct tdls_send_mgmt_request *tdls_mgmt_req;

	tdls_mgmt_req = msg->bodyptr;

	qdf_mem_free(tdls_mgmt_req);
	msg->bodyptr = NULL;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
tdls_activate_send_mgmt_request(struct tdls_action_frame_request *action_req)
{
	struct tdls_soc_priv_obj *tdls_soc_obj;
	QDF_STATUS status;
	struct tdls_send_mgmt_request *tdls_mgmt_req;
	struct wlan_objmgr_peer *peer;
	struct scheduler_msg msg = {0};
	struct tdls_vdev_priv_obj *tdls_vdev;

	if (!action_req || !action_req->vdev)
		return QDF_STATUS_E_NULL_VALUE;

	tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(action_req->vdev);
	if (!tdls_soc_obj) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_cmd;
	}

	tdls_mgmt_req = qdf_mem_malloc(sizeof(struct tdls_send_mgmt_request) +
				action_req->tdls_mgmt.len);
	if (!tdls_mgmt_req) {
		status = QDF_STATUS_E_NOMEM;
		goto release_cmd;
	}

	tdls_debug("session_id %d "
		   "tdls_mgmt.dialog %d "
		   "tdls_mgmt.frame_type %d "
		   "tdls_mgmt.status_code %d "
		   "tdls_mgmt.responder %d "
		   "tdls_mgmt.peer_capability %d",
		   action_req->session_id,
		   action_req->tdls_mgmt.dialog,
		   action_req->tdls_mgmt.frame_type,
		   action_req->tdls_mgmt.status_code,
		   action_req->tdls_mgmt.responder,
		   action_req->tdls_mgmt.peer_capability);

	tdls_mgmt_req->session_id = action_req->session_id;
	tdls_mgmt_req->req_type = action_req->tdls_mgmt.frame_type;
	tdls_mgmt_req->dialog = action_req->tdls_mgmt.dialog;
	tdls_mgmt_req->status_code = action_req->tdls_mgmt.status_code;
	tdls_mgmt_req->responder = action_req->tdls_mgmt.responder;
	tdls_mgmt_req->peer_capability = action_req->tdls_mgmt.peer_capability;

	peer = wlan_objmgr_vdev_try_get_bsspeer(action_req->vdev,
						WLAN_TDLS_SB_ID);
	if (!peer) {
		tdls_err("bss peer is null");
		qdf_mem_free(tdls_mgmt_req);
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_cmd;
	}

	qdf_mem_copy(tdls_mgmt_req->bssid.bytes,
		     wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);

	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_SB_ID);

	qdf_mem_copy(tdls_mgmt_req->peer_mac.bytes,
		     action_req->tdls_mgmt.peer_mac.bytes, QDF_MAC_ADDR_SIZE);

	if (action_req->tdls_mgmt.len) {
		qdf_mem_copy(tdls_mgmt_req->add_ie, action_req->tdls_mgmt.buf,
			     action_req->tdls_mgmt.len);
	}

	tdls_mgmt_req->length = sizeof(struct tdls_send_mgmt_request) +
				action_req->tdls_mgmt.len;
	if (action_req->use_default_ac)
		tdls_mgmt_req->ac = WIFI_AC_VI;
	else
		tdls_mgmt_req->ac = WIFI_AC_BK;

	if (wlan_vdev_mlme_is_mlo_vdev(action_req->vdev) &&
	    !tdls_mlo_get_tdls_link_vdev(action_req->vdev) &&
	    tdls_mgmt_req->req_type == TDLS_DISCOVERY_REQUEST) {
		tdls_vdev = wlan_vdev_get_tdls_vdev_obj(action_req->vdev);
		if (QDF_TIMER_STATE_RUNNING !=
		    qdf_mc_timer_get_current_state(
					  &tdls_vdev->peer_discovery_timer)) {
			tdls_timer_restart(tdls_vdev->vdev,
				     &tdls_vdev->peer_discovery_timer,
				     tdls_vdev->threshold_config.tx_period_t -
				     TDLS_DISCOVERY_TIMEOUT_ERE_UPDATE);
			qdf_atomic_inc(&tdls_soc_obj->timer_cnt);
		} else {
			qdf_mem_free(tdls_mgmt_req);
			status = QDF_STATUS_E_NULL_VALUE;
			goto release_cmd;
		}
	}

	/* Send the request to PE. */
	qdf_mem_zero(&msg, sizeof(msg));

	tdls_debug("sending TDLS Mgmt Frame req to PE ");
	tdls_mgmt_req->message_type = tdls_soc_obj->tdls_send_mgmt_req;

	msg.type = tdls_soc_obj->tdls_send_mgmt_req;
	msg.bodyptr = tdls_mgmt_req;
	msg.flush_callback = tdls_activate_send_mgmt_request_flush_cb;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(tdls_mgmt_req);

release_cmd:
	/*update tdls nss infornation based on action code */
	tdls_reset_nss(tdls_soc_obj, action_req->chk_frame.action_code);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_internal_send_mgmt_tx_done(action_req, status);
		tdls_release_serialization_command(action_req->vdev,
						   WLAN_SER_CMD_TDLS_SEND_MGMT);
	}

	return status;
}

static QDF_STATUS
tdls_send_mgmt_serialize_callback(struct wlan_serialization_command *cmd,
	 enum wlan_serialization_cb_reason reason)
{
	struct tdls_action_frame_request *req;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!cmd || !cmd->umac_cmd) {
		tdls_err("invalid params cmd: %pK, ", cmd);
		return QDF_STATUS_E_NULL_VALUE;
	}
	req = cmd->umac_cmd;

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		/* command moved to active list */
		status = tdls_activate_send_mgmt_request(req);
		break;

	case WLAN_SER_CB_CANCEL_CMD:
	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		/* command removed from pending list.
		 * notify status complete with failure
		 */
		status = tdls_internal_send_mgmt_tx_done(req,
				QDF_STATUS_E_FAILURE);
		break;

	case WLAN_SER_CB_RELEASE_MEM_CMD:
		/* command successfully completed.
		 * release tdls_action_frame_request memory
		 */
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(req);
		break;

	default:
		/* Do nothing but logging */
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO
QDF_STATUS tdls_set_link_mode(struct tdls_action_frame_request *req)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *mlo_tdls_vdev;
	bool is_mlo_vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct ml_nlink_change_event data;
	uint8_t num_ml_sta = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};

	is_mlo_vdev = wlan_vdev_mlme_is_mlo_vdev(req->vdev);
	if (!is_mlo_vdev)
		return status;

	psoc = wlan_vdev_get_psoc(req->vdev);
	if (!psoc) {
		tdls_err("psoc is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}


	if (req->tdls_mgmt.frame_type == TDLS_DISCOVERY_RESPONSE ||
	    req->tdls_mgmt.frame_type == TDLS_DISCOVERY_REQUEST) {
		mlo_tdls_vdev = wlan_mlo_get_tdls_link_vdev(req->vdev);
		if (mlo_tdls_vdev)
			return status;

		status = policy_mgr_is_ml_links_in_mcc_allowed(
						psoc, req->vdev,
						ml_sta_vdev_lst,
						&num_ml_sta);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			tdls_err("ML STA Links in MCC, so don't send the TDLS frames");
			return QDF_STATUS_E_FAILURE;
		}

		qdf_mem_zero(&data, sizeof(data));
		data.evt.tdls.link_bitmap =
				1 << wlan_vdev_get_link_id(req->vdev);
		data.evt.tdls.mlo_vdev_lst[0] = wlan_vdev_get_id(req->vdev);
		data.evt.tdls.vdev_count = 1;
		data.evt.tdls.mode = MLO_LINK_FORCE_MODE_ACTIVE;
		data.evt.tdls.reason = MLO_LINK_FORCE_REASON_TDLS;
		status =
		ml_nlink_conn_change_notify(psoc,
					    wlan_vdev_get_id(req->vdev),
					    ml_nlink_tdls_request_evt,
					    &data);
		if (status == QDF_STATUS_SUCCESS ||
		    status == QDF_STATUS_E_PENDING)
			req->link_active = true;
	} else if (req->tdls_mgmt.frame_type == TDLS_MAX_ACTION_CODE) {
		qdf_mem_zero(&data, sizeof(data));
		data.evt.tdls.link_bitmap =
				1 << wlan_vdev_get_link_id(req->vdev);
		data.evt.tdls.mlo_vdev_lst[0] = wlan_vdev_get_id(req->vdev);
		data.evt.tdls.vdev_count = 1;
		data.evt.tdls.mode = MLO_LINK_FORCE_MODE_NO_FORCE;
		data.evt.tdls.reason = MLO_LINK_FORCE_REASON_TDLS;
		status =
		ml_nlink_conn_change_notify(psoc,
					    wlan_vdev_get_id(req->vdev),
					    ml_nlink_tdls_request_evt,
					    &data);
	}
	if (status == QDF_STATUS_E_PENDING)
		status = QDF_STATUS_SUCCESS;

	return status;
}
#else
QDF_STATUS tdls_set_link_mode(struct tdls_action_frame_request *req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS tdls_process_mgmt_req(
			struct tdls_action_frame_request *tdls_mgmt_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_serialization_command cmd = {0, };
	enum wlan_serialization_status ser_cmd_status;

	/* If connected and in Infra. Only then allow this */
	status = tdls_validate_mgmt_request(tdls_mgmt_req);
	if (status != QDF_STATUS_SUCCESS) {
		status = tdls_internal_send_mgmt_tx_done(tdls_mgmt_req,
							 status);
		goto error_mgmt;
	}

	status = tdls_set_link_mode(tdls_mgmt_req);
	if (status != QDF_STATUS_SUCCESS) {
		tdls_err("failed to set link:%d active",
			 wlan_vdev_get_link_id(tdls_mgmt_req->vdev));
		status = tdls_internal_send_mgmt_tx_done(tdls_mgmt_req,
							 status);
		goto error_mgmt;
	}

	/* update the responder, status code information
	 * after the  cmd validation
	 */
	tdls_mgmt_req->tdls_mgmt.responder =
			!tdls_mgmt_req->chk_frame.responder;
	tdls_mgmt_req->tdls_mgmt.status_code =
			tdls_mgmt_req->chk_frame.status_code;

	cmd.cmd_type = WLAN_SER_CMD_TDLS_SEND_MGMT;
	/* Cmd Id not applicable for non scan cmds */
	cmd.cmd_id = 0;
	cmd.cmd_cb = tdls_send_mgmt_serialize_callback;
	cmd.umac_cmd = tdls_mgmt_req;
	cmd.source = WLAN_UMAC_COMP_TDLS;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = TDLS_DEFAULT_SERIALIZE_CMD_TIMEOUT;

	cmd.vdev = tdls_mgmt_req->vdev;
	cmd.is_blocking = true;

	ser_cmd_status = wlan_serialization_request(&cmd);
	tdls_debug("wlan_serialization_request status:%d", ser_cmd_status);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list.Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	case WLAN_SER_CMD_DENIED_LIST_FULL:
	case WLAN_SER_CMD_DENIED_RULES_FAILED:
	case WLAN_SER_CMD_DENIED_UNSPECIFIED:
		status = QDF_STATUS_E_FAILURE;
		goto error_mgmt;
	default:
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		goto error_mgmt;
	}
	return status;

error_mgmt:
	wlan_objmgr_vdev_release_ref(tdls_mgmt_req->vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(tdls_mgmt_req);
	return status;
}
