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
				 uint8_t *valid_dir)
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

		if (t2lm->t2lm_info[dir].default_link_mapping) {
			is_valid_link_mask = true;
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

QDF_STATUS t2lm_handle_rx_req(struct wlan_objmgr_vdev *vdev,
			      struct wlan_objmgr_peer *peer,
			      void *event_data, uint8_t *token)
{
	struct wlan_t2lm_onging_negotiation_info t2lm_req = {0};
	struct wlan_t2lm_info *t2lm_info;
	uint8_t dir = WLAN_T2LM_MAX_DIRECTION;
	bool valid_map = false;
	QDF_STATUS status;
	struct wlan_mlo_peer_context *ml_peer;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	status = wlan_mlo_parse_t2lm_action_frame(&t2lm_req, event_data,
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
			      void *event_data, uint8_t *token)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS t2lm_handle_rx_resp(struct wlan_objmgr_vdev *vdev,
			       void *event_data, uint8_t *token)
{
	return QDF_STATUS_SUCCESS;
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
			      void *event_data, uint8_t *token)
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
		status = t2lm_handle_rx_req(vdev, peer, event_data, token);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_TX_RESP:
		status = t2lm_handle_tx_resp(vdev, event_data, token);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_TX_REQ:
		status = t2lm_handle_tx_req(vdev, event_data, token);
		break;
	case WLAN_T2LM_EV_ACTION_FRAME_RX_RESP:
		status = t2lm_handle_rx_resp(vdev, event_data, token);
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

QDF_STATUS wlan_t2lm_deliver_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   enum wlan_t2lm_evt event,
				   void *event_data,
				   uint8_t *dialog_token)
{
	return t2lm_deliver_event(vdev, peer, event, event_data, dialog_token);
}
