/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: contains EPCS APIs
 */

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_mlo_mgr_public_structs.h>
#include <wlan_mlo_mgr_cmn.h>
#include <qdf_util.h>
#include <wlan_cm_api.h>
#include <utils_mlo.h>
#include <wlan_mlo_epcs.h>

/**
 * wlan_mlo_is_node_epcs_authorized() - API to check mac address is
 * EPCS authorized or not
 * @ml_peer: pointer to mlo context of peer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_is_node_epcs_authorized(struct wlan_mlo_peer_context *ml_peer)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	struct wlan_epcs_context *epcs_ctx;
	enum QDF_OPMODE opmode;
	struct wlan_objmgr_vdev *vdev = NULL;
	int i;

	if (!ml_peer) {
		epcs_err("ml_peer is null");
		return QDF_STATUS_E_INVAL;
	}

	mlo_dev_ctx = ml_peer->ml_dev;
	if (!mlo_dev_ctx) {
		epcs_err("mlo dev ctx is null");
		return QDF_STATUS_E_INVAL;
	}

	/* Get first valid vdev */
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		vdev = mlo_dev_ctx->wlan_vdev_list[i];
		opmode = wlan_vdev_mlme_get_opmode(vdev);
		break;
	}

	if (!vdev) {
		epcs_err("no valid vdev entry found");
		return QDF_STATUS_E_INVAL;
	}

	epcs_debug("ml peer type %d", opmode);
	if (opmode != QDF_SAP_MODE)
		return QDF_STATUS_SUCCESS;

	epcs_ctx = &mlo_dev_ctx->epcs_ctx;
	if (!epcs_ctx) {
		epcs_err("epcs info is null");
		return QDF_STATUS_E_INVAL;
	}

	epcs_dev_lock_acquire(epcs_ctx);
	for (i = 0; i < EPCS_MAX_AUTHORIZE_MAC_ADDR; i++) {
		if (epcs_ctx->authorize_info[i].valid &&
		    !qdf_mem_cmp(epcs_ctx->authorize_info[i].peer_mld_mac,
				 ml_peer->peer_mld_addr.bytes,
				 QDF_MAC_ADDR_SIZE)) {
			epcs_dev_lock_release(epcs_ctx);
			return QDF_STATUS_SUCCESS;
		}
	}
	epcs_dev_lock_release(epcs_ctx);

	return QDF_STATUS_E_INVAL;
}

/**
 * mlo_process_ml_priorityaccess_ie() - API to parse Priority access ML IE
 * @ml_ie: Pointer to start of ML IE
 * @ml_ie_len: Length of ML IE
 * @priority_access_info: pointer to fill multi link priority access information
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
mlo_process_ml_priorityaccess_ie(uint8_t *ml_ie, qdf_size_t ml_ie_len,
				 struct ml_pa_info *priority_access_info)
{
	uint8_t *ml_pa_ie = NULL;
	qdf_size_t ml_pa_ie_len = 0;
	QDF_STATUS status;

	if (!ml_ie) {
		mlo_err("NULL ml_ie");
		return QDF_STATUS_E_INVAL;
	}

	if (!priority_access_info) {
		mlo_err("NULL priority_access_info");
		return QDF_STATUS_E_INVAL;
	}

	status = util_find_mlie_by_variant(ml_ie,
					   ml_ie_len,
					   &ml_pa_ie,
					   &ml_pa_ie_len,
					   WLAN_ML_VARIANT_PRIORITYACCESS);

	if (QDF_IS_STATUS_ERROR(status) || !ml_pa_ie) {
		mlo_debug("ML IE for reconfig variant not found");
		return QDF_STATUS_E_INVAL;
	}
	epcs_debug("PAV ML IE with length %zu is present", ml_pa_ie_len);

	status = util_get_pav_mlie_link_info(ml_pa_ie, ml_pa_ie_len,
					     priority_access_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Unable to get sta link info from ML PAV IE");
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_mlo_parse_epcs_request_action_frame() - API to parse EPCS request action
 * frame.
 * @epcs: Pointer to EPCS structure
 * @action_frm: Pointer to action frame
 * @frm_len: frame length
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_parse_epcs_request_action_frame(struct wlan_epcs_info *epcs,
					 struct wlan_action_frame *action_frm,
					 uint32_t frm_len)
{
	struct epcs_frm *epcs_action_frm;
	struct ml_pa_info *priority_access_info = &epcs->pa_info;
	uint8_t *pa_ie;
	uint16_t pa_ie_len;

	/*
	 * EPCS request action frame
	 *
	 *   1-byte     1-byte     1-byte    variable
	 *--------------------------------------------
	 * |         |           |        |          |
	 * | Category| Protected | Dialog | PA ML IE |
	 * |         |    EHT    | token  |          |
	 * |         |  Action   |        |          |
	 *--------------------------------------------
	 */

	epcs_action_frm = (struct epcs_frm *)action_frm;

	epcs->cat = epcs_action_frm->protected_eht_action;
	epcs->dialog_token = epcs_action_frm->dialog_token;
	epcs_info("EPCS frame rcv : category:%d action:%d dialog_token:%d frmlen %d",
		  epcs_action_frm->category,
		  epcs_action_frm->protected_eht_action,
		  epcs_action_frm->dialog_token, frm_len);

	if (frm_len > EPCS_REQ_MIN_LENGTH) {
		pa_ie = (uint8_t *)epcs_action_frm + EPCS_REQ_MIN_LENGTH;
		pa_ie_len = frm_len - EPCS_REQ_MIN_LENGTH;
		return mlo_process_ml_priorityaccess_ie(pa_ie,
							pa_ie_len,
							priority_access_info);
	} else {
		return QDF_STATUS_SUCCESS;
	}
}

/**
 * wlan_mlo_parse_epcs_response_action_frame() - API to parse EPCS response
 * action frame.
 * @epcs: Pointer to EPCS structure
 * @action_frm: Pointer to action frame
 * @frm_len: frame length
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_parse_epcs_response_action_frame(struct wlan_epcs_info *epcs,
					  struct wlan_action_frame *action_frm,
					  uint32_t frm_len)
{
	struct epcs_frm *epcs_action_frm;
	struct ml_pa_info *priority_access_info = &epcs->pa_info;
	uint8_t *pa_ie;
	uint16_t pa_ie_len;

	/*
	 * EPCS response action frame
	 *
	 *   1-byte     1-byte     1-byte   1-byte   variable
	 *----------------------------------------------------
	 * |         |           |        |        |         |
	 * | Category| Protected | Dialog | Status | PA   IE |
	 * |         |    EHT    | token  |  code  |         |
	 * |         |  Action   |        |        |         |
	 *----------------------------------------------------
	 */

	epcs_action_frm = (struct epcs_frm *)action_frm;

	epcs->cat = epcs_action_frm->protected_eht_action;
	epcs->dialog_token = epcs_action_frm->dialog_token;
	QDF_SET_BITS(epcs->status, 0, 8, epcs_action_frm->resp.status_code[0]);
	QDF_SET_BITS(epcs->status, 8, 8, epcs_action_frm->resp.status_code[1]);
	epcs_info("EPCS frame rcv : category:%d action:%d dialog_token:%d status %x %x frmlen %d",
		  epcs_action_frm->category,
		  epcs_action_frm->protected_eht_action,
		  epcs_action_frm->dialog_token,
		  epcs_action_frm->resp.status_code[0],
		  epcs_action_frm->resp.status_code[1], frm_len);

	if (frm_len > EPCS_RESP_MIN_LENGTH) {
		pa_ie = (uint8_t *)epcs_action_frm + EPCS_RESP_MIN_LENGTH;
		pa_ie_len = frm_len - EPCS_RESP_MIN_LENGTH;
		return mlo_process_ml_priorityaccess_ie(pa_ie,
							pa_ie_len,
							priority_access_info);
	} else {
		return QDF_STATUS_SUCCESS;
	}
}

/**
 * wlan_mlo_parse_epcs_teardown_action_frame() - API to parse EPCS teardown
 * action frame.
 * @epcs: Pointer to EPCS structure
 * @action_frm: Pointer to action frame
 * @frm_len: frame length
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_parse_epcs_teardown_action_frame(struct wlan_epcs_info *epcs,
					  struct wlan_action_frame *action_frm,
					  uint32_t frm_len)
{
	struct epcs_frm *epcs_action_frm;

	/*
	 * EPCS teardown action frame
	 *
	 *   1-byte     1-byte
	 *------------------------
	 * |         |           |
	 * | Category| Protected |
	 * |         |    EHT    |
	 * |         |  Action   |
	 *------------------------
	 */

	epcs_action_frm = (struct epcs_frm *)action_frm;

	epcs->cat = epcs_action_frm->protected_eht_action;
	epcs_info("EPCS frame rcv : category:%d action:%d frmlen %d",
		  epcs_action_frm->category,
		  epcs_action_frm->protected_eht_action, frm_len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlo_parse_epcs_action_frame(struct wlan_epcs_info *epcs,
				 struct wlan_action_frame *action_frm,
				 uint32_t frm_len)
{
	QDF_STATUS ret_val = QDF_STATUS_SUCCESS;

	switch (action_frm->action) {
	case WLAN_EPCS_CATEGORY_REQUEST:
		return wlan_mlo_parse_epcs_request_action_frame(
				epcs, action_frm, frm_len);
	case WLAN_EPCS_CATEGORY_RESPONSE:
		return wlan_mlo_parse_epcs_response_action_frame(
				epcs, action_frm, frm_len);
	case WLAN_EPCS_CATEGORY_TEARDOWN:
		return wlan_mlo_parse_epcs_teardown_action_frame(
				epcs, action_frm, frm_len);
	default:
		ret_val = QDF_STATUS_E_INVAL;
			epcs_err("Invalid action :%d", action_frm->action);
	}

	return ret_val;
}

static uint8_t *
wlan_mlo_add_epcs_request_action_frame(uint8_t *frm,
				       struct wlan_action_frame_args *args,
				       uint8_t *buf)
{
	*frm++ = args->category;
	*frm++ = args->action;
	/* Dialog token*/
	*frm++ = args->arg1;

	epcs_info("EPCS frame: category:%d action:%d dialog_token:%d",
		  args->category, args->action, args->arg1);

	/* Add priority access ml ie in caller for AP mode */
	return frm;
}

static uint8_t *
wlan_mlo_add_epcs_response_action_frame(uint8_t *frm,
					struct wlan_action_frame_args *args,
					uint8_t *buf)
{
	*frm++ = args->category;
	*frm++ = args->action;
	/* Dialog token*/
	*frm++ = args->arg1;
	/* Status code (2 bytes) */
	*frm++ = QDF_GET_BITS(args->arg2, 0, 8);
	*frm++ = QDF_GET_BITS(args->arg2, 8, 8);

	epcs_info("EPCS response frame: category:%d action:%d dialog_token:%d status_code:%d",
		  args->category, args->action, args->arg1, args->arg2);

	/* Add priority access ml ie for AP mode */
	return frm;
}

uint8_t *
wlan_mlo_add_epcs_action_frame(uint8_t *frm,
			       struct wlan_action_frame_args *args,
			       uint8_t *buf)
{
	switch (args->action) {
	case WLAN_EPCS_CATEGORY_REQUEST:
		return wlan_mlo_add_epcs_request_action_frame(frm, args,
							      buf);
	case WLAN_EPCS_CATEGORY_RESPONSE:
		return wlan_mlo_add_epcs_response_action_frame(frm, args,
							      buf);
	case WLAN_EPCS_CATEGORY_TEARDOWN:
		*frm++ = args->category;
		*frm++ = args->action;
		return frm;
	default:
		epcs_err("Invalid category:%d", args->category);
	}

	return frm;
}

QDF_STATUS
wlan_mlo_peer_rcv_cmd(struct wlan_mlo_peer_context *ml_peer,
		      struct wlan_epcs_info *epcs,
		      bool *updparam)
{
	uint32_t cur_state;
	uint32_t new_state;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (!ml_peer) {
		epcs_err("Null MLO peer");
		return QDF_STATUS_E_INVAL;
	}

	*updparam = false;

	epcs_dev_peer_lock_acquire(&ml_peer->epcs_info);
	cur_state = ml_peer->epcs_info.state;
	switch (ml_peer->epcs_info.state) {
	case EPCS_DOWN:
		if (epcs->cat == WLAN_EPCS_CATEGORY_REQUEST) {
		/* check authorization */
			if (wlan_mlo_is_node_epcs_authorized(ml_peer) ==
			   QDF_STATUS_SUCCESS) {
				status = QDF_STATUS_SUCCESS;
				epcs->dialog_token =
				  ++ml_peer->epcs_info.self_gen_dialog_token;
			} else {
				epcs_info("peer not authorized to enable EPCS");
			}
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_TEARDOWN) {
			epcs_info("peer already in EPCS down state");
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_RESPONSE) {
			epcs_err("Invalid command");
		}
		break;
	case EPCS_ENABLE:
		if (epcs->cat == WLAN_EPCS_CATEGORY_TEARDOWN) {
			ml_peer->epcs_info.state = EPCS_DOWN;
			status = QDF_STATUS_SUCCESS;
			*updparam = true;
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_REQUEST) {
			epcs_info("peer already in EPCS enable state");
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_RESPONSE) {
			epcs_err("Invalid command");
		}
		break;
	default:
		epcs_err("Invalid peer state %d",
			 ml_peer->epcs_info.state);
	}

	new_state = ml_peer->epcs_info.state;
	epcs_debug("cmd:old state %d new state %d ev cat %d dialog token %d status %d",
		   cur_state, new_state, epcs->cat,
		   epcs->dialog_token, epcs->status);

	epcs_dev_peer_lock_release(&ml_peer->epcs_info);

	return status;
}

QDF_STATUS
wlan_mlo_peer_rcv_action_frame(struct wlan_mlo_peer_context *ml_peer,
			       struct wlan_epcs_info *epcs,
			       bool *respond,
			       bool *updparam)
{
	uint32_t cur_state;
	uint32_t new_state;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (!ml_peer) {
		epcs_err("Null MLO peer");
		return QDF_STATUS_E_INVAL;
	}

	*respond = false;
	*updparam = false;

	epcs_dev_peer_lock_acquire(&ml_peer->epcs_info);
	cur_state = ml_peer->epcs_info.state;
	switch (ml_peer->epcs_info.state) {
	case EPCS_DOWN:
		if (epcs->cat == WLAN_EPCS_CATEGORY_RESPONSE) {
			if (epcs->status == STATUS_SUCCESS) {
				if (epcs->dialog_token ==
				    ml_peer->epcs_info.self_gen_dialog_token) {
					ml_peer->epcs_info.state = EPCS_ENABLE;
					status = QDF_STATUS_SUCCESS;
					*updparam = true;
				} else {
					epcs_err("Response dialog token mismatch self_gen_dialog_token %d response token %d", ml_peer->epcs_info.self_gen_dialog_token, epcs->dialog_token);
				}
			} else {
				epcs_info("epcs rejected with status code %d",
					  epcs->status);
			}
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_REQUEST) {
			/* check authorization */
			if (wlan_mlo_is_node_epcs_authorized(ml_peer) ==
			   QDF_STATUS_SUCCESS) {
				ml_peer->epcs_info.state = EPCS_ENABLE;
				status = QDF_STATUS_SUCCESS;
				*respond = true;
				*updparam = true;
			} else {
				epcs_info("peer not authorized to enable EPCS");
			}
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_TEARDOWN) {
			epcs_info("peer not in EPCS enable state");
		}
		break;
	case EPCS_ENABLE:
		if (epcs->cat == WLAN_EPCS_CATEGORY_TEARDOWN) {
			ml_peer->epcs_info.state = EPCS_DOWN;
			status = QDF_STATUS_SUCCESS;
			*updparam = true;
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_REQUEST) {
			epcs_info("peer already in EPCS enable state");
		} else if (epcs->cat == WLAN_EPCS_CATEGORY_RESPONSE) {
			epcs_info("peer already in EPCS enable state");
		}
		break;
	default:
		epcs_err("Invalid peer state %d", ml_peer->epcs_info.state);
	}

	new_state = ml_peer->epcs_info.state;
	epcs_debug("action:old state %d new state %d ev cat %d dialog token %d status %d",
		   cur_state, new_state, epcs->cat,
		   epcs->dialog_token, epcs->status);

	epcs_dev_peer_lock_release(&ml_peer->epcs_info);

	return status;
}

QDF_STATUS
wlan_mlo_update_authorize_epcs_mac_addr(struct wlan_objmgr_vdev *vdev,
					uint8_t *peer_mld_mac)
{
	bool found_entry = false;
	int free_index = -1;
	int i = 0;
	struct wlan_epcs_context *epcs_ctx;

	if (!vdev) {
		epcs_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	epcs_ctx = &vdev->mlo_dev_ctx->epcs_ctx;

	epcs_dev_lock_acquire(epcs_ctx);
	for (i = 0; i < EPCS_MAX_AUTHORIZE_MAC_ADDR; i++) {
		/* Finding first available slot */
		if ((!epcs_ctx->authorize_info[i].valid) && (free_index < 0))
			free_index = i;

		/* Checking for already available valid entry */
		if (epcs_ctx->authorize_info[i].valid &&
		    !qdf_mem_cmp(epcs_ctx->authorize_info[i].peer_mld_mac,
				 peer_mld_mac,
				 QDF_MAC_ADDR_SIZE)) {
			found_entry = true;
			break;
		}
	}

	if (found_entry) {
		epcs_debug("Mac add "QDF_MAC_ADDR_FMT" is already authorized",
			   QDF_MAC_ADDR_REF(peer_mld_mac));
		epcs_dev_lock_release(epcs_ctx);
		return QDF_STATUS_E_INVAL;
	}

	if (free_index < 0) {
		epcs_debug("EPCS authorize database is full");
		epcs_dev_lock_release(epcs_ctx);
		return QDF_STATUS_E_INVAL;
	}

	epcs_ctx->authorize_info[free_index].valid = true;
	qdf_mem_copy(epcs_ctx->authorize_info[free_index]. peer_mld_mac,
		     peer_mld_mac,
		     QDF_MAC_ADDR_SIZE);
	epcs_dev_lock_release(epcs_ctx);

	epcs_debug("EPCS Stored authorize mac addr is"QDF_MAC_ADDR_FMT" at index %d",
		   QDF_MAC_ADDR_REF(peer_mld_mac), free_index);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlo_update_deauthorize_epcs_mac_addr(struct wlan_objmgr_vdev *vdev,
					  uint8_t *peer_mld_mac)
{
	int i = 0;
	struct wlan_epcs_context *epcs_ctx;
	bool found_entry = false;

	if (!vdev) {
		epcs_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	epcs_ctx = &vdev->mlo_dev_ctx->epcs_ctx;

	epcs_dev_lock_acquire(epcs_ctx);
	for (i = 0; i < EPCS_MAX_AUTHORIZE_MAC_ADDR; i++) {
		if (!qdf_mem_cmp(epcs_ctx->authorize_info[i].peer_mld_mac,
				 peer_mld_mac,
				 QDF_MAC_ADDR_SIZE)) {
			found_entry = true;
			break;
		}
	}

	if (!found_entry) {
		epcs_debug("Mac addr "QDF_MAC_ADDR_FMT" not found in authorized database",
			   QDF_MAC_ADDR_REF(peer_mld_mac));
		epcs_dev_lock_release(epcs_ctx);
		return QDF_STATUS_E_INVAL;
	}

	if (found_entry && !epcs_ctx->authorize_info[i].valid) {
		epcs_debug("Mac addr "QDF_MAC_ADDR_FMT" is already deauthorized in database",
			   QDF_MAC_ADDR_REF(peer_mld_mac));
		epcs_dev_lock_release(epcs_ctx);
		return QDF_STATUS_E_INVAL;
	}

	epcs_ctx->authorize_info[i].valid = false;
	epcs_dev_lock_release(epcs_ctx);
	epcs_debug("EPCS Stored authorize mac addr is "QDF_MAC_ADDR_FMT" at idx %d is removed",
		   QDF_MAC_ADDR_REF(peer_mld_mac), i);

	return QDF_STATUS_SUCCESS;
}
