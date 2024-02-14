/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains TID to Link mapping related functionality
 */
#include <wlan_cmn.h>
#include <wlan_cm_public_struct.h>
#include "wlan_t2lm_api.h"
#include <wlan_mlo_t2lm.h>
#include "wlan_cm_api.h"
#include "wlan_mlo_mgr_roam.h"

#define T2LM_MIN_DIALOG_TOKEN         1
#define T2LM_MAX_DIALOG_TOKEN         0xFF

static
const char *t2lm_get_event_str(enum wlan_t2lm_evt event)
{
	if (event > WLAN_T2LM_EV_ACTION_FRAME_MAX)
		return "";

	switch (event) {
	CASE_RETURN_STRING(WLAN_T2LM_EV_ACTION_FRAME_RX_REQ);
	CASE_RETURN_STRING(WLAN_T2LM_EV_ACTION_FRAME_TX_RESP);
	CASE_RETURN_STRING(WLAN_T2LM_EV_ACTION_FRAME_TX_REQ);
	CASE_RETURN_STRING(WLAN_T2LM_EV_ACTION_FRAME_RX_RESP);
	CASE_RETURN_STRING(WLAN_T2LM_EV_ACTION_FRAME_RX_TEARDOWN);
	CASE_RETURN_STRING(WLAN_T2LM_EV_ACTION_FRAME_TX_TEARDOWN);
	default:
		return "Unknown";
	}
}

static
bool t2lm_is_valid_t2lm_link_map(struct wlan_objmgr_vdev *vdev,
				 struct wlan_t2lm_onging_negotiation_info *t2lm,
				 enum wlan_t2lm_direction *valid_dir)
{
	uint8_t i, tid = 0;
	enum wlan_t2lm_direction dir = WLAN_T2LM_INVALID_DIRECTION;
	uint16_t ieee_link_mask = 0;
	uint16_t provisioned_links = 0;
	bool is_valid_link_mask = false;
	struct wlan_objmgr_vdev *ml_vdev = NULL;
	struct wlan_objmgr_vdev *ml_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = {NULL};
	uint16_t ml_vdev_cnt = 0;

	/* Get the valid hw_link_id map from ML vdev list */
	mlo_get_ml_vdev_list(vdev, &ml_vdev_cnt, ml_vdev_list);
	if (!ml_vdev_cnt) {
		t2lm_err("Number of VDEVs under MLD is reported as 0");
		return false;
	}

	for (i = 0; i < ml_vdev_cnt; i++) {
		ml_vdev = ml_vdev_list[i];
		if (!ml_vdev || !wlan_cm_is_vdev_connected(ml_vdev)) {
			t2lm_err("ML vdev is null");
			continue;
		}

		ieee_link_mask |= BIT(wlan_vdev_get_link_id(ml_vdev));
	}

	if (ml_vdev_cnt) {
		for (i = 0; i < ml_vdev_cnt; i++)
			mlo_release_vdev_ref(ml_vdev_list[i]);
	}

	/* Check if the configured hw_link_id map is valid */
	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		if (t2lm->t2lm_info[dir].direction ==
		    WLAN_T2LM_INVALID_DIRECTION)
			continue;

		if (t2lm->t2lm_info[dir].default_link_mapping &&
		    t2lm->t2lm_info[dir].direction == WLAN_T2LM_BIDI_DIRECTION) {
			is_valid_link_mask = true;
			*valid_dir = dir;
			continue;
		}

		for (tid = 0; tid < T2LM_MAX_NUM_TIDS; tid++) {
			provisioned_links =
				t2lm->t2lm_info[dir].ieee_link_map_tid[tid];

			for (i = 0; i < WLAN_T2LM_MAX_NUM_LINKS; i++) {
				if (!(provisioned_links & BIT(i)))
					continue;

				if (ieee_link_mask & BIT(i)) {
					is_valid_link_mask = true;
					*valid_dir = dir;
					continue;
				} else {
					return false;
				}
			}
		}
	}

	return is_valid_link_mask;
}

static uint8_t
t2lm_gen_dialog_token(struct wlan_mlo_peer_t2lm_policy *t2lm_policy)
{
	if (!t2lm_policy)
		return 0;

	if (t2lm_policy->self_gen_dialog_token == T2LM_MAX_DIALOG_TOKEN)
		/* wrap is ok */
		t2lm_policy->self_gen_dialog_token = T2LM_MIN_DIALOG_TOKEN;
	else
		t2lm_policy->self_gen_dialog_token += 1;

	t2lm_debug("gen dialog token %d", t2lm_policy->self_gen_dialog_token);
	return t2lm_policy->self_gen_dialog_token;
}

QDF_STATUS t2lm_handle_rx_req(struct wlan_objmgr_vdev *vdev,
			      struct wlan_objmgr_peer *peer,
			      void *event_data, uint32_t frame_len,
			      uint8_t *token)
{
	struct wlan_t2lm_onging_negotiation_info t2lm_req = {0};
	struct wlan_t2lm_info *t2lm_info;
	enum wlan_t2lm_direction dir = WLAN_T2LM_MAX_DIRECTION;
	bool valid_map = false;
	QDF_STATUS status;
	struct wlan_mlo_peer_context *ml_peer;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	status = wlan_mlo_parse_t2lm_action_frame(&t2lm_req, event_data,
						  frame_len,
						  WLAN_T2LM_CATEGORY_REQUEST);
	if (status != QDF_STATUS_SUCCESS) {
		mlme_err("Unable to parse T2LM request action frame");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Check if ML vdevs are connected and link id matches with T2LM
	 * negotiation action request link id
	 */
	valid_map = t2lm_is_valid_t2lm_link_map(vdev, &t2lm_req, &dir);
	if (valid_map) {
		mlme_debug("Link match found,accept t2lm conf");
		status = QDF_STATUS_SUCCESS;
	} else {
		status = QDF_STATUS_E_FAILURE;
		mlme_err("reject t2lm conf");
	}

	if (dir >= WLAN_T2LM_MAX_DIRECTION) {
		mlme_err("Received T2LM IE has invalid direction");
		status = QDF_STATUS_E_INVAL;
	}

	if (QDF_IS_STATUS_SUCCESS(status) &&
	    t2lm_req.t2lm_info[dir].direction != WLAN_T2LM_INVALID_DIRECTION) {
		/* Apply T2LM config to peer T2LM ctx */
		t2lm_info = &ml_peer->t2lm_policy.t2lm_negotiated_info.t2lm_info[dir];
		qdf_mem_copy(t2lm_info, &t2lm_req.t2lm_info[dir],
			     sizeof(struct wlan_t2lm_info));
	}

	*token = t2lm_req.dialog_token;

	return status;
}

QDF_STATUS t2lm_handle_tx_resp(struct wlan_objmgr_vdev *vdev,
			       void *event_data, uint8_t *token)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS t2lm_handle_tx_req(struct wlan_objmgr_vdev *vdev,
			      struct wlan_objmgr_peer *peer,
			      void *event_data, uint8_t *token)
{
	struct wlan_t2lm_onging_negotiation_info *t2lm_neg;
	struct wlan_action_frame_args args;
	QDF_STATUS status;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	if (!event_data) {
		t2lm_err("Null event data ptr");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_neg = (struct wlan_t2lm_onging_negotiation_info *)event_data;
	args.category = ACTION_CATEGORY_PROTECTED_EHT;
	args.action = EHT_T2LM_REQUEST;
	args.arg1 = *token;

	status = lim_send_t2lm_action_req_frame(vdev,
						wlan_peer_get_macaddr(peer),
						&args, t2lm_neg,
						*token);

	if (QDF_IS_STATUS_ERROR(status)) {
		t2lm_err("Failed to send T2LM action request frame");
	} else {
		t2lm_debug("Copy the ongoing neg to peer");
		qdf_mem_copy(&peer->mlo_peer_ctx->t2lm_policy.ongoing_tid_to_link_mapping,
			     t2lm_neg, sizeof(struct wlan_t2lm_onging_negotiation_info));
	}

	return status;
}

QDF_STATUS t2lm_handle_rx_resp(struct wlan_objmgr_vdev *vdev,
			       struct wlan_objmgr_peer *peer,
			       void *event_data, uint32_t frame_len,
			       uint8_t *token)
{
	struct wlan_t2lm_onging_negotiation_info t2lm_rsp = {0};
	struct wlan_t2lm_onging_negotiation_info *t2lm_req;
	QDF_STATUS status;
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_t2lm_info *t2lm_info;
	uint8_t dir;

	if (!peer) {
		t2lm_err("peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer) {
		t2lm_err("ml peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* ignore the frame if all links are not connected */
	if (!mlo_check_if_all_links_up(vdev))
		return QDF_STATUS_SUCCESS;

	status = wlan_mlo_parse_t2lm_action_frame(&t2lm_rsp, event_data,
						  frame_len,
						  WLAN_T2LM_CATEGORY_RESPONSE);
	if (status != QDF_STATUS_SUCCESS) {
		mlme_err("Unable to parse T2LM request action frame");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_debug("t2lm rsp dialog token %d", t2lm_rsp.dialog_token);
	mlme_debug("t2lm rsp is %d", t2lm_rsp.t2lm_resp_type);
	t2lm_req = &ml_peer->t2lm_policy.ongoing_tid_to_link_mapping;
	if (!t2lm_req) {
		t2lm_err("Ongoing tid neg is null");
		return QDF_STATUS_E_FAILURE;
	}

	for (dir = 0; dir < WLAN_T2LM_MAX_DIRECTION; dir++) {
		t2lm_info = &t2lm_req->t2lm_info[dir];
		if (t2lm_info &&
		    t2lm_info->direction != WLAN_T2LM_INVALID_DIRECTION) {
			if (t2lm_rsp.dialog_token == t2lm_req->dialog_token &&
			    t2lm_rsp.t2lm_resp_type == WLAN_T2LM_RESP_TYPE_SUCCESS) {
				status = wlan_send_tid_to_link_mapping(vdev,
								       t2lm_info);
				if (QDF_IS_STATUS_ERROR(status)) {
					t2lm_err("sending t2lm wmi failed");
					break;
				}
			} else if (t2lm_rsp.dialog_token == t2lm_req->dialog_token &&
				   t2lm_rsp.t2lm_resp_type != WLAN_T2LM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING) {
				t2lm_debug("T2LM rsp status denied, clear ongoing tid mapping");
				wlan_t2lm_clear_ongoing_negotiation(peer);
			}
		}
	}

	return status;
}

QDF_STATUS t2lm_handle_rx_teardown(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   void *event_data)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_t2lm_context *t2lm_ctx;

	if (!peer) {
		t2lm_err("peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!vdev) {
		t2lm_err("vdev is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx) {
		t2lm_err("mlo dev ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	t2lm_ctx = &mlo_dev_ctx->t2lm_ctx;
	if (!t2lm_ctx) {
		t2lm_err("t2lm ctx is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_t2lm_clear_peer_negotiation(peer);

	/* Notify the registered caller about the link update*/
	wlan_mlo_dev_t2lm_notify_link_update(vdev,
					     &t2lm_ctx->established_t2lm.t2lm);
	wlan_send_tid_to_link_mapping(vdev,
				      &t2lm_ctx->established_t2lm.t2lm);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS t2lm_handle_tx_teardown(struct wlan_objmgr_vdev *vdev,
				   void *event_data)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS t2lm_deliver_event(struct wlan_objmgr_vdev *vdev,
			      struct wlan_objmgr_peer *peer,
			      enum wlan_t2lm_evt event,
			      void *event_data, uint32_t frame_len,
			      uint8_t *token)
{
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	mlme_debug("T2LM event received: %s(%d)",
		   t2lm_get_event_str(event), event);

	switch (event) {
	case WLAN_T2LM_EV_ACTION_FRAME_RX_REQ:
		status = t2lm_handle_rx_req(vdev, peer, event_data,
					    frame_len, token);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_TX_RESP:
		status = t2lm_handle_tx_resp(vdev, event_data, token);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_TX_REQ:
		status = t2lm_handle_tx_req(vdev, peer, event_data, token);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_RX_RESP:
		status = t2lm_handle_rx_resp(vdev, peer, event_data,
					     frame_len, token);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_RX_TEARDOWN:
		status = t2lm_handle_rx_teardown(vdev, peer, event_data);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_TX_TEARDOWN:
		status = t2lm_handle_tx_teardown(vdev, event_data);
		break;
	default:
		status = QDF_STATUS_E_FAILURE;
		mlme_err("Unhandled T2LM event");
	}

	return status;
}

static uint16_t
t2lm_get_tids_mapped_link_id(uint16_t link_map_tid)
{
	uint16_t all_tids_mapped_link_id = 0;
	uint8_t i;
	uint8_t bit_mask = 1;

	for (i = 0; i < WLAN_T2LM_MAX_NUM_LINKS; i++) {
		if (link_map_tid & bit_mask)
			all_tids_mapped_link_id = i;
		bit_mask = bit_mask << 1;
	}

	return all_tids_mapped_link_id;
}

static QDF_STATUS
t2lm_find_tid_mapped_link_id(struct wlan_t2lm_info *t2lm_info,
			     uint16_t *tid_mapped_link_id)
{
	uint16_t link_map_tid;
	uint8_t tid;

	if (!t2lm_info)
		return QDF_STATUS_E_NULL_VALUE;

	if (t2lm_info->default_link_mapping) {
		t2lm_debug("T2LM ie has default link mapping");
		*tid_mapped_link_id = 0xFFFF;
		return QDF_STATUS_SUCCESS;
	}

	link_map_tid = t2lm_info->ieee_link_map_tid[0];
	for (tid = 1; tid < T2LM_MAX_NUM_TIDS; tid++) {
		if (link_map_tid != t2lm_info->ieee_link_map_tid[tid]) {
			mlme_debug("all tids are not mapped to same link set");
			return QDF_STATUS_E_FAILURE;
		}
	}

	*tid_mapped_link_id = t2lm_get_tids_mapped_link_id(link_map_tid);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_t2lm_validate_candidate(struct cnx_mgr *cm_ctx,
			     struct scan_cache_entry *scan_entry)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_t2lm_context t2lm_ctx;
	uint16_t tid_map_link_id;
	uint16_t established_tid_mapped_link_id = 0;
	uint16_t upcoming_tid_mapped_link_id = 0;

	if (!scan_entry)
		return QDF_STATUS_E_NULL_VALUE;

	if (!cm_ctx || !cm_ctx->vdev)
		return QDF_STATUS_E_NULL_VALUE;

	vdev = cm_ctx->vdev;

	if (wlan_vdev_mlme_is_mlo_link_vdev(vdev)) {
		mlme_debug("Skip t2lm validation for link vdev");
		return QDF_STATUS_SUCCESS;
	}

	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) &&
	    scan_entry->ie_list.t2lm[0]) {
		status = wlan_mlo_parse_bcn_prbresp_t2lm_ie(&t2lm_ctx,
						scan_entry->ie_list.t2lm[0]);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;

		status =
		   t2lm_find_tid_mapped_link_id(&t2lm_ctx.established_t2lm.t2lm,
					       &established_tid_mapped_link_id);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;

		status =
		      t2lm_find_tid_mapped_link_id(&t2lm_ctx.upcoming_t2lm.t2lm,
						  &upcoming_tid_mapped_link_id);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;
		t2lm_debug("established_tid_mapped_link_id %x, upcoming_tid_mapped_link_id %x",
			   established_tid_mapped_link_id,
			   upcoming_tid_mapped_link_id);

		tid_map_link_id =
		   established_tid_mapped_link_id & upcoming_tid_mapped_link_id;
		if (!tid_map_link_id)
			tid_map_link_id = established_tid_mapped_link_id;

		if (tid_map_link_id == scan_entry->ml_info.self_link_id) {
			t2lm_debug("self link id %d, tid map link id %d match",
				   scan_entry->ml_info.self_link_id,
				   tid_map_link_id);
			status = QDF_STATUS_SUCCESS;
		} else {
			t2lm_debug("self link id %d, tid map link id %d do not match",
				   scan_entry->ml_info.self_link_id,
				   tid_map_link_id);
			status = QDF_STATUS_E_FAILURE;
		}
	} else {
		t2lm_debug("T2LM IE is not present in scan entry");
		status = QDF_STATUS_SUCCESS;
	}

end:
	return status;
}

void
wlan_t2lm_clear_ongoing_negotiation(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_t2lm_onging_negotiation_info *ongoing_tid_to_link_mapping;
	uint8_t i;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer) {
		t2lm_err("ml peer is null");
		return;
	}

	ongoing_tid_to_link_mapping = &ml_peer->t2lm_policy.ongoing_tid_to_link_mapping;
	if (!ongoing_tid_to_link_mapping) {
		t2lm_err("ongoing tid mapping is null");
		return;
	}

	qdf_mem_zero(&ongoing_tid_to_link_mapping->t2lm_info,
		     sizeof(struct wlan_t2lm_info) * WLAN_T2LM_MAX_DIRECTION);

	ongoing_tid_to_link_mapping->dialog_token = 0;
	ongoing_tid_to_link_mapping->category = WLAN_T2LM_CATEGORY_NONE;
	ongoing_tid_to_link_mapping->t2lm_resp_type = WLAN_T2LM_RESP_TYPE_INVALID;
	ongoing_tid_to_link_mapping->t2lm_tx_status = WLAN_T2LM_TX_STATUS_NONE;

	for (i = 0; i < WLAN_T2LM_MAX_DIRECTION; i++)
		ongoing_tid_to_link_mapping->t2lm_info[i].direction =
				WLAN_T2LM_INVALID_DIRECTION;
}

void
wlan_t2lm_clear_peer_negotiation(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_prev_t2lm_negotiated_info *t2lm_negotiated_info;
	uint8_t i;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer) {
		t2lm_err("ml peer is null");
		return;
	}

	qdf_mem_zero(&ml_peer->t2lm_policy.t2lm_negotiated_info.t2lm_info,
		     sizeof(struct wlan_t2lm_info) * WLAN_T2LM_MAX_DIRECTION);

	ml_peer->t2lm_policy.t2lm_negotiated_info.dialog_token = 0;
	t2lm_negotiated_info = &ml_peer->t2lm_policy.t2lm_negotiated_info;
	for (i = 0; i < WLAN_T2LM_MAX_DIRECTION; i++)
		t2lm_negotiated_info->t2lm_info[i].direction =
				WLAN_T2LM_INVALID_DIRECTION;
}

void
wlan_t2lm_clear_all_tid_mapping(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_t2lm_context *t2lm_ctx;

	if (!vdev) {
		t2lm_err("Vdev is null");
		return;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev))
		return;

	if (!vdev->mlo_dev_ctx) {
		t2lm_err("mlo dev ctx is null");
		return;
	}

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;
	peer = wlan_vdev_get_bsspeer(vdev);
	if (!peer) {
		t2lm_err("peer is null");
		return;
	}
	qdf_mem_zero(&t2lm_ctx->established_t2lm,
		     sizeof(struct wlan_mlo_t2lm_ie));
	t2lm_ctx->established_t2lm.t2lm.direction = WLAN_T2LM_BIDI_DIRECTION;
	t2lm_ctx->established_t2lm.t2lm.default_link_mapping = 1;
	t2lm_ctx->established_t2lm.t2lm.link_mapping_size = 0;

	qdf_mem_zero(&t2lm_ctx->upcoming_t2lm,
		     sizeof(struct wlan_mlo_t2lm_ie));
	t2lm_ctx->upcoming_t2lm.t2lm.direction = WLAN_T2LM_INVALID_DIRECTION;

	wlan_t2lm_clear_peer_negotiation(peer);
	wlan_t2lm_clear_ongoing_negotiation(peer);
	wlan_mlo_t2lm_timer_stop(vdev);
}

static bool
wlan_is_ml_link_disabled(uint32_t link_id_bitmap,
			 uint8_t ml_link_id)
{
	uint8_t link;

	if (!link_id_bitmap) {
		t2lm_err("Link id bitmap is 0");
		return false;
	}

	for (link = 0; link < WLAN_T2LM_MAX_NUM_LINKS; link++) {
		if ((link == ml_link_id) &&
		    (link_id_bitmap & BIT(link))) {
			return true;
		}
	}

	return false;
}

static void
wlan_t2lm_set_link_mapping_of_tids(uint8_t link_id,
				   struct wlan_t2lm_info *t2lm_info,
				   bool set)
{
	uint8_t tid_num;

	if (link_id >= WLAN_T2LM_MAX_NUM_LINKS) {
		t2lm_err("Max 16 t2lm links are supported");
		return;
	}

	for (tid_num = 0; tid_num < T2LM_MAX_NUM_TIDS; tid_num++) {
		if (set)
			t2lm_info->ieee_link_map_tid[tid_num] |= BIT(link_id);
		else
			t2lm_info->ieee_link_map_tid[tid_num] &= ~BIT(link_id);
	}
}

QDF_STATUS
wlan_populate_link_disable_t2lm_frame(struct wlan_objmgr_vdev *vdev,
				      struct mlo_link_disable_request_evt_params *params)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_mlo_dev_context *ml_dev_ctx;
	struct wlan_mlo_peer_t2lm_policy *t2lm_policy;
	struct wlan_objmgr_vdev *tmp_vdev;
	struct wlan_t2lm_onging_negotiation_info t2lm_neg = {0};
	uint8_t dir = WLAN_T2LM_BIDI_DIRECTION;
	uint8_t i = 0;
	QDF_STATUS status;
	uint8_t link_id;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev,
						WLAN_MLO_MGR_ID);

	if (!peer) {
		t2lm_err("peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!vdev->mlo_dev_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	t2lm_policy = &peer->mlo_peer_ctx->t2lm_policy;
	t2lm_neg = t2lm_policy->ongoing_tid_to_link_mapping;

	t2lm_neg.category = WLAN_T2LM_CATEGORY_REQUEST;
	t2lm_neg.dialog_token = t2lm_gen_dialog_token(t2lm_policy);
	qdf_mem_zero(&t2lm_neg.t2lm_info,
		     sizeof(struct wlan_t2lm_info) * WLAN_T2LM_MAX_DIRECTION);
	for (i = 0; i < WLAN_T2LM_MAX_DIRECTION; i++)
		t2lm_neg.t2lm_info[i].direction = WLAN_T2LM_INVALID_DIRECTION;

	t2lm_neg.t2lm_info[dir].default_link_mapping = 0;
	t2lm_neg.t2lm_info[dir].direction = WLAN_T2LM_BIDI_DIRECTION;
	t2lm_neg.t2lm_info[dir].mapping_switch_time_present = 0;
	t2lm_neg.t2lm_info[dir].expected_duration_present = 0;
	t2lm_neg.t2lm_info[dir].link_mapping_size = 1;

	t2lm_debug("dir %d", t2lm_neg.t2lm_info[dir].direction);
	ml_dev_ctx = vdev->mlo_dev_ctx;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!ml_dev_ctx->wlan_vdev_list[i])
			continue;

		tmp_vdev = ml_dev_ctx->wlan_vdev_list[i];
		link_id = wlan_vdev_get_link_id(tmp_vdev);

		/* if link id matches disabled link id bitmap
		 * set that bit as 0.
		 */
		if (wlan_is_ml_link_disabled(params->link_id_bitmap,
					     link_id)) {
			wlan_t2lm_set_link_mapping_of_tids(link_id,
						&t2lm_neg.t2lm_info[dir],
						0);
			t2lm_debug("Disabled link id %d", link_id);
		} else {
			wlan_t2lm_set_link_mapping_of_tids(link_id,
						&t2lm_neg.t2lm_info[dir],
						1);
			t2lm_debug("Enabled link id %d", link_id);
		}
	}

	status = t2lm_deliver_event(vdev, peer,
				    WLAN_T2LM_EV_ACTION_FRAME_TX_REQ,
				    &t2lm_neg,
				    0,
				    &t2lm_neg.dialog_token);

	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);
	return status;
}

QDF_STATUS wlan_t2lm_deliver_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   enum wlan_t2lm_evt event,
				   void *event_data,
				   uint32_t frame_len,
				   uint8_t *dialog_token)
{
	return t2lm_deliver_event(vdev, peer, event, event_data,
				  frame_len, dialog_token);
}
