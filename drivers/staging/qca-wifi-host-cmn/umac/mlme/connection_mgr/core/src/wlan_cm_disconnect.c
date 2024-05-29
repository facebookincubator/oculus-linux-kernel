/*
 * Copyright (c) 2012-2015,2020-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: Implements disconnect specific apis of connection manager
 */
#include "wlan_cm_main_api.h"
#include "wlan_cm_sm.h"
#include "wlan_cm_roam.h"
#include <wlan_serialization_api.h>
#include "wlan_utility.h"
#include "wlan_scan_api.h"
#include "wlan_crypto_global_api.h"
#ifdef CONN_MGR_ADV_FEATURE
#include "wlan_dlm_api.h"
#endif
#include <wlan_mlo_mgr_sta.h>

void cm_send_disconnect_resp(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id)
{
	struct wlan_cm_discon_rsp resp;
	QDF_STATUS status;

	qdf_mem_zero(&resp, sizeof(resp));
	status = cm_fill_disconnect_resp_from_cm_id(cm_ctx, cm_id, &resp);
	if (QDF_IS_STATUS_SUCCESS(status))
		cm_disconnect_complete(cm_ctx, &resp);
}

#ifdef WLAN_CM_USE_SPINLOCK
static QDF_STATUS cm_activate_disconnect_req_sched_cb(struct scheduler_msg *msg)
{
	struct wlan_serialization_command *cmd = msg->bodyptr;
	struct wlan_objmgr_vdev *vdev;
	struct cnx_mgr *cm_ctx;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	if (!cmd) {
		mlme_err("cmd is null");
		return QDF_STATUS_E_INVAL;
	}

	vdev = cmd->vdev;
	if (!vdev) {
		mlme_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	ret = cm_sm_deliver_event(
			cm_ctx->vdev,
			WLAN_CM_SM_EV_DISCONNECT_ACTIVE,
			sizeof(wlan_cm_id),
			&cmd->cmd_id);

	/*
	 * Called from scheduler context hence
	 * handle failure if posting fails
	 */
	if (QDF_IS_STATUS_ERROR(ret)) {
		mlme_err(CM_PREFIX_FMT "Activation failed for cmd:%d",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), cmd->cmd_id),
			 cmd->cmd_type);
		cm_send_disconnect_resp(cm_ctx, cmd->cmd_id);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
	return ret;
}

static QDF_STATUS
cm_activate_disconnect_req(struct wlan_serialization_command *cmd)
{
	struct wlan_objmgr_vdev *vdev = cmd->vdev;
	struct scheduler_msg msg = {0};
	QDF_STATUS ret;

	msg.bodyptr = cmd;
	msg.callback = cm_activate_disconnect_req_sched_cb;
	msg.flush_callback = cm_activate_cmd_req_flush_cb;

	ret = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLME_CM_ID);
	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	ret = scheduler_post_message(QDF_MODULE_ID_MLME,
				     QDF_MODULE_ID_MLME,
				     QDF_MODULE_ID_MLME, &msg);

	if (QDF_IS_STATUS_ERROR(ret)) {
		mlme_err(CM_PREFIX_FMT "Failed to post scheduler_msg",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), cmd->cmd_id));
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return ret;
	}
	mlme_debug(CM_PREFIX_FMT "Cmd act in sched cmd type:%d",
		   CM_PREFIX_REF(wlan_vdev_get_id(vdev), cmd->cmd_id),
		   cmd->cmd_type);

	return ret;
}
#else
static QDF_STATUS
cm_activate_disconnect_req(struct wlan_serialization_command *cmd)
{
	return cm_sm_deliver_event(
			cmd->vdev,
			WLAN_CM_SM_EV_DISCONNECT_ACTIVE,
			sizeof(wlan_cm_id),
			&cmd->cmd_id);
}
#endif

static QDF_STATUS
cm_sm_deliver_disconnect_event(struct cnx_mgr *cm_ctx,
			       struct wlan_serialization_command *cmd)
{
	/*
	 * For pending to active, use async cmnd to take lock.
	 * Use sync command for direct activation as lock is already
	 * acquired.
	 */
	if (cmd->activation_reason == SER_PENDING_TO_ACTIVE)
		return cm_activate_disconnect_req(cmd);
	else
		return cm_sm_deliver_event_sync(
					cm_ctx,
					WLAN_CM_SM_EV_DISCONNECT_ACTIVE,
					sizeof(wlan_cm_id),
					&cmd->cmd_id);
}

static QDF_STATUS
cm_ser_disconnect_cb(struct wlan_serialization_command *cmd,
		     enum wlan_serialization_cb_reason reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev;
	struct cnx_mgr *cm_ctx;

	if (!cmd) {
		mlme_err("cmd is NULL, reason: %d", reason);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev = cmd->vdev;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_NULL_VALUE;

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		status = cm_sm_deliver_disconnect_event(cm_ctx, cmd);
		if (QDF_IS_STATUS_SUCCESS(status))
			break;
		/*
		 * Handle failure if posting fails, i.e. the SM state has
		 * changes. Disconnect should be handled in JOIN_PENDING,
		 * JOIN-SCAN state as well apart from DISCONNECTING.
		 * Also no need to check for head list as diconnect needs to be
		 * completed always once active.
		 */

		cm_send_disconnect_resp(cm_ctx, cmd->cmd_id);
		break;
	case WLAN_SER_CB_CANCEL_CMD:
		/* command removed from pending list. */
		break;
	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		mlme_err(CM_PREFIX_FMT "Active command timeout",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), cmd->cmd_id));
		cm_trigger_panic_on_cmd_timeout(cm_ctx->vdev);
		cm_send_disconnect_resp(cm_ctx, cmd->cmd_id);
		break;
	case WLAN_SER_CB_RELEASE_MEM_CMD:
		cm_reset_active_cm_id(vdev, cmd->cmd_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		break;
	default:
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

static QDF_STATUS cm_ser_disconnect_req(struct wlan_objmgr_pdev *pdev,
					struct cnx_mgr *cm_ctx,
					struct cm_disconnect_req *req)
{
	struct wlan_serialization_command cmd = {0, };
	enum wlan_serialization_status ser_cmd_status;
	QDF_STATUS status;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);

	status = wlan_objmgr_vdev_try_get_ref(cm_ctx->vdev, WLAN_MLME_CM_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "unable to get reference",
			 CM_PREFIX_REF(vdev_id, req->cm_id));
		return status;
	}

	cmd.cmd_type = WLAN_SER_CMD_VDEV_DISCONNECT;
	cmd.cmd_id = req->cm_id;
	cmd.cmd_cb = cm_ser_disconnect_cb;
	cmd.source = WLAN_UMAC_COMP_MLME;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = DISCONNECT_TIMEOUT;
	cmd.vdev = cm_ctx->vdev;
	cmd.is_blocking = cm_ser_get_blocking_cmd();

	ser_cmd_status = wlan_serialization_request(&cmd);
	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list.Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	default:
		mlme_err(CM_PREFIX_FMT "ser cmd status %d",
			 CM_PREFIX_REF(vdev_id, req->cm_id), ser_cmd_status);
		wlan_objmgr_vdev_release_ref(cm_ctx->vdev, WLAN_MLME_CM_ID);

		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static void
cm_if_mgr_inform_disconnect_complete(struct wlan_objmgr_vdev *vdev)
{
	struct if_mgr_event_data *disconnect_complete;

	disconnect_complete = qdf_mem_malloc(sizeof(*disconnect_complete));
	if (!disconnect_complete)
		return;

	disconnect_complete->status = QDF_STATUS_SUCCESS;

	if_mgr_deliver_event(vdev, WLAN_IF_MGR_EV_DISCONNECT_COMPLETE,
			     disconnect_complete);
	qdf_mem_free(disconnect_complete);
}

static void
cm_if_mgr_inform_disconnect_start(struct wlan_objmgr_vdev *vdev)
{
	struct if_mgr_event_data *disconnect_start;

	disconnect_start = qdf_mem_malloc(sizeof(*disconnect_start));
	if (!disconnect_start)
		return;

	disconnect_start->status = QDF_STATUS_SUCCESS;

	if_mgr_deliver_event(vdev, WLAN_IF_MGR_EV_DISCONNECT_START,
			     disconnect_start);
	qdf_mem_free(disconnect_start);
}

void cm_initiate_internal_disconnect(struct cnx_mgr *cm_ctx)
{
	struct cm_req *cm_req;
	struct cm_disconnect_req *disconnect_req;
	QDF_STATUS status;

	cm_req = qdf_mem_malloc(sizeof(*cm_req));

	if (!cm_req)
		return;

	disconnect_req = &cm_req->discon_req;
	disconnect_req->req.vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	disconnect_req->req.source = CM_INTERNAL_DISCONNECT;

	if (wlan_vdev_mlme_is_mlo_vdev(cm_ctx->vdev))
		mlo_internal_disconnect_links(cm_ctx->vdev);

	status = cm_add_disconnect_req_to_list(cm_ctx, disconnect_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "failed to add disconnect req",
			 CM_PREFIX_REF(disconnect_req->req.vdev_id,
				       disconnect_req->cm_id));
		qdf_mem_free(cm_req);
		return;
	}

	cm_disconnect_start(cm_ctx, disconnect_req);
}

QDF_STATUS cm_disconnect_start(struct cnx_mgr *cm_ctx,
			       struct cm_disconnect_req *req)
{
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(cm_ctx->vdev);
	if (!pdev) {
		cm_send_disconnect_resp(cm_ctx, req->cm_id);
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_vdev_mlme_is_mlo_vdev(cm_ctx->vdev))
		mlo_internal_disconnect_links(cm_ctx->vdev);

	cm_vdev_scan_cancel(pdev, cm_ctx->vdev);
	mlme_cm_disconnect_start_ind(cm_ctx->vdev, &req->req);
	cm_if_mgr_inform_disconnect_start(cm_ctx->vdev);
	mlme_cm_osif_disconnect_start_ind(cm_ctx->vdev);

	/* Serialize disconnect req, Handle failure status */
	status = cm_ser_disconnect_req(pdev, cm_ctx, req);

	if (QDF_IS_STATUS_ERROR(status))
		cm_send_disconnect_resp(cm_ctx, req->cm_id);

	return QDF_STATUS_SUCCESS;
}

void
cm_update_scan_mlme_on_disconnect(struct wlan_objmgr_vdev *vdev,
				  struct cm_disconnect_req *req)
{
	struct wlan_objmgr_pdev *pdev;
	struct bss_info bss_info;
	struct mlme_info mlme;
	struct wlan_channel *chan;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlme_err(CM_PREFIX_FMT "failed to find pdev",
			 CM_PREFIX_REF(req->req.vdev_id, req->cm_id));
		return;
	}

	chan = wlan_vdev_get_active_channel(vdev);
	if (!chan) {
		mlme_err(CM_PREFIX_FMT "failed to get active channel",
			 CM_PREFIX_REF(req->req.vdev_id, req->cm_id));
		return;
	}

	status = wlan_vdev_mlme_get_ssid(vdev, bss_info.ssid.ssid,
					 &bss_info.ssid.length);

	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "failed to get ssid",
			 CM_PREFIX_REF(req->req.vdev_id, req->cm_id));
		return;
	}

	mlme.assoc_state = SCAN_ENTRY_CON_STATE_NONE;
	qdf_copy_macaddr(&bss_info.bssid, &req->req.bssid);

	bss_info.freq = chan->ch_freq;

	wlan_scan_update_mlme_by_bssinfo(pdev, &bss_info, &mlme);
}

QDF_STATUS cm_disconnect_active(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id)
{
	struct wlan_cm_vdev_discon_req *req;
	struct cm_req *cm_req;
	QDF_STATUS status = QDF_STATUS_E_NOSUPPORT;

	cm_ctx->active_cm_id = *cm_id;
	cm_req = cm_get_req_by_cm_id(cm_ctx, *cm_id);
	if (!cm_req) {
		/*
		 * Remove the command from serialization active queue, if
		 * disconnect req was not found, to avoid active cmd timeout.
		 * This can happen if a thread tried to flush the pending
		 * disconnect request and while doing so, it removed the
		 * CM pending request, but before it tried to remove pending
		 * command from serialization, the command becomes active in
		 * another thread.
		 */
		cm_remove_cmd_from_serialization(cm_ctx, *cm_id);
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_vdev_mlme_get_opmode(cm_ctx->vdev) == QDF_STA_MODE)
		status = mlme_cm_rso_stop_req(cm_ctx->vdev);

	if (status != QDF_STATUS_E_NOSUPPORT)
		return status;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	req->cm_id = *cm_id;
	req->req.vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	req->req.source = cm_req->discon_req.req.source;
	req->req.reason_code = cm_req->discon_req.req.reason_code;
	req->req.is_no_disassoc_disconnect =
			cm_req->discon_req.req.is_no_disassoc_disconnect;

	cm_disconnect_continue_after_rso_stop(cm_ctx->vdev, req);
	qdf_mem_free(req);

	return status;
}

QDF_STATUS
cm_disconnect_continue_after_rso_stop(struct wlan_objmgr_vdev *vdev,
				      struct wlan_cm_vdev_discon_req *req)
{
	struct cm_req *cm_req;
	QDF_STATUS status;
	struct qdf_mac_addr bssid = QDF_MAC_ADDR_ZERO_INIT;
	struct cnx_mgr *cm_ctx = cm_get_cm_ctx(vdev);

	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	cm_req = cm_get_req_by_cm_id(cm_ctx, req->cm_id);
	if (!cm_req)
		return QDF_STATUS_E_INVAL;

	wlan_vdev_get_bss_peer_mac(cm_ctx->vdev, &bssid);

	qdf_copy_macaddr(&req->req.bssid, &bssid);
	/*
	 * for northbound req, bssid is not provided so update it from vdev
	 * in case bssid is not present
	 */
	if (qdf_is_macaddr_zero(&cm_req->discon_req.req.bssid) ||
	    qdf_is_macaddr_broadcast(&cm_req->discon_req.req.bssid))
		qdf_copy_macaddr(&cm_req->discon_req.req.bssid,
				 &req->req.bssid);
	cm_update_scan_mlme_on_disconnect(cm_ctx->vdev,
					  &cm_req->discon_req);

	mlme_debug(CM_PREFIX_FMT "disconnect " QDF_MAC_ADDR_FMT
		   " source %d reason %d",
		   CM_PREFIX_REF(req->req.vdev_id, req->cm_id),
		   QDF_MAC_ADDR_REF(req->req.bssid.bytes),
		   req->req.source, req->req.reason_code);

	status = mlme_cm_disconnect_req(cm_ctx->vdev, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "disconnect req fail",
			 CM_PREFIX_REF(req->req.vdev_id, req->cm_id));
		cm_send_disconnect_resp(cm_ctx, req->cm_id);
	}

	return status;
}

QDF_STATUS
cm_handle_rso_stop_rsp(struct wlan_objmgr_vdev *vdev,
		       struct wlan_cm_vdev_discon_req *req)
{
	struct cnx_mgr *cm_ctx = cm_get_cm_ctx(vdev);
	wlan_cm_id cm_id;

	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	cm_id = cm_ctx->active_cm_id;

	if ((CM_ID_GET_PREFIX(req->cm_id)) != DISCONNECT_REQ_PREFIX ||
	    cm_id != req->cm_id) {
		mlme_err(CM_PREFIX_FMT "active req is not disconnect req",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), req->cm_id));
		return QDF_STATUS_E_INVAL;
	}

	return cm_sm_deliver_event(vdev, WLAN_CM_SM_EV_RSO_STOP_RSP,
				   sizeof(*req), req);
}

#ifdef CONN_MGR_ADV_FEATURE
static void
cm_inform_dlm_disconnect_complete(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_discon_rsp *resp)
{
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlme_err(CM_PREFIX_FMT "failed to find pdev",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev),
				       resp->req.cm_id));
		return;
	}

	wlan_dlm_update_bssid_connect_params(pdev, resp->req.req.bssid,
					     DLM_AP_DISCONNECTED);
}

#else
static inline void
cm_inform_dlm_disconnect_complete(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_discon_rsp *resp)
{}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static inline void
cm_clear_vdev_mlo_cap(struct wlan_objmgr_vdev *vdev,
		      struct wlan_cm_discon_rsp *rsp)
{
	wlan_vdev_mlme_clear_mlo_vdev(vdev);
}
#else /*WLAN_FEATURE_11BE_MLO_ADV_FEATURE*/
static inline void
cm_clear_vdev_mlo_cap(struct wlan_objmgr_vdev *vdev,
		      struct wlan_cm_discon_rsp *rsp)
{
	if (mlo_is_mld_sta(vdev) && ucfg_mlo_is_mld_disconnected(vdev))
		ucfg_mlo_mld_clear_mlo_cap(vdev);
	if (rsp->req.req.reason_code == REASON_HOST_TRIGGERED_LINK_DELETE) {
		wlan_vdev_mlme_clear_mlo_vdev(vdev);
		wlan_vdev_mlme_clear_mlo_link_vdev(vdev);
	}
}
#endif /*WLAN_FEATURE_11BE_MLO_ADV_FEATURE*/
#else /*WLAN_FEATURE_11BE_MLO*/
static inline void
cm_clear_vdev_mlo_cap(struct wlan_objmgr_vdev *vdev,
		      struct wlan_cm_discon_rsp *rsp)
{ }
#endif /*WLAN_FEATURE_11BE_MLO*/

QDF_STATUS cm_notify_disconnect_complete(struct cnx_mgr *cm_ctx,
					 struct wlan_cm_discon_rsp *resp)
{
	mlme_cm_disconnect_complete_ind(cm_ctx->vdev, resp);
	mlo_sta_link_disconn_notify(cm_ctx->vdev, resp);
	mlme_cm_osif_disconnect_complete(cm_ctx->vdev, resp);
	cm_if_mgr_inform_disconnect_complete(cm_ctx->vdev);
	cm_inform_dlm_disconnect_complete(cm_ctx->vdev, resp);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cm_disconnect_complete(struct cnx_mgr *cm_ctx,
				  struct wlan_cm_discon_rsp *resp)
{
	/*
	 * If the entry is not present in the list, it must have been cleared
	 * already.
	 */
	if (!cm_get_req_by_cm_id(cm_ctx, resp->req.cm_id))
		return QDF_STATUS_SUCCESS;

	cm_notify_disconnect_complete(cm_ctx, resp);

	/*
	 * Remove all pending disconnect if this is an active disconnect
	 * complete.
	 */
	if (resp->req.cm_id == cm_ctx->active_cm_id)
		cm_flush_pending_request(cm_ctx, DISCONNECT_REQ_PREFIX, false);

	cm_remove_cmd(cm_ctx, &resp->req.cm_id);
	mlme_debug(CM_PREFIX_FMT "disconnect count %d connect count %d",
		   CM_PREFIX_REF(wlan_vdev_get_id(cm_ctx->vdev),
				 resp->req.cm_id),
		   cm_ctx->disconnect_count, cm_ctx->connect_count);
	/* Flush failed connect req as pending disconnect is completed */
	if (!cm_ctx->disconnect_count && cm_ctx->connect_count)
		cm_flush_pending_request(cm_ctx, CONNECT_REQ_PREFIX, true);

	/* Set the disconnect wait event once all disconnect are completed */
	if (!cm_ctx->disconnect_count) {
		/*
		 * Clear MLO cap only when it is the last disconnect req
		 * For 1x/owe roaming, link vdev mlo flags are not cleared
		 * as connect req is queued on link vdev after this.
		 */
		if (!wlan_cm_check_mlo_roam_auth_status(cm_ctx->vdev))
			cm_clear_vdev_mlo_cap(cm_ctx->vdev, resp);
		qdf_event_set(&cm_ctx->disconnect_complete);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_handle_discon_req_in_non_connected_state(struct cnx_mgr *cm_ctx,
					struct cm_disconnect_req *cm_req,
					enum wlan_cm_sm_state cm_state_substate)
{
	enum wlan_cm_sm_state cur_state = cm_get_state(cm_ctx);

	/*
	 * South bound and peer disconnect requests are meant for only in
	 * connected state, so if the state is connecting a new connect has
	 * been received, hence skip the non-osif disconnect request. Also allow
	 * MLO link vdev disconnect in connecting state, as this can be
	 * initiated due to disconnect on assoc vdev, which may be in connected
	 * state.
	 */
	if (cur_state == WLAN_CM_S_CONNECTING &&
	    (cm_req->req.source != CM_OSIF_DISCONNECT &&
	    cm_req->req.source != CM_OSIF_CFG_DISCONNECT &&
	    cm_req->req.source != CM_MLO_LINK_VDEV_DISCONNECT)) {
		mlme_info("Vdev %d ignore disconnect req from source %d in state %d",
			  wlan_vdev_get_id(cm_ctx->vdev), cm_req->req.source,
			  cm_state_substate);
		return QDF_STATUS_E_INVAL;
	}

	switch (cm_state_substate) {
	case WLAN_CM_S_DISCONNECTING:
		/*
		 * There would be pending disconnect requests in the list, and
		 * if they are flushed as part of new disconnect
		 * (cm_flush_pending_request), OS_IF would inform the kernel
		 * about the disconnect done even though the disconnect is still
		 * pending. So update OS_IF with invalid CM_ID so that the resp
		 * of only the new disconnect req is given to kernel.
		 */
		mlme_cm_osif_update_id_and_src(cm_ctx->vdev,
					       CM_SOURCE_INVALID,
					       CM_ID_INVALID);

		/* Flush for non mlo link vdev only */
		if (!wlan_vdev_mlme_is_mlo_vdev(cm_ctx->vdev) ||
		    !wlan_vdev_mlme_is_mlo_link_vdev(cm_ctx->vdev))
			cm_flush_pending_request(cm_ctx, DISCONNECT_REQ_PREFIX,
						 false);
		/*
		 * Flush failed pending connect req as new req is received
		 * and its no longer the latest one.
		 */
		if (cm_ctx->connect_count)
			cm_flush_pending_request(cm_ctx, CONNECT_REQ_PREFIX,
						 true);
		break;
	case WLAN_CM_S_ROAMING:
		/* for FW roam/LFR3 remove the req from the list */
		if (cm_roam_offload_enabled(wlan_vdev_get_psoc(cm_ctx->vdev)))
			cm_flush_pending_request(cm_ctx, ROAM_REQ_PREFIX,
						 false);
		fallthrough;
	case WLAN_CM_SS_JOIN_ACTIVE:
		/*
		 * In join active/roaming state, there would be no pending
		 * command, so no action required. so for new disconnect
		 * request, queue disconnect and move the state to
		 * disconnecting.
		 */
		break;
	case WLAN_CM_SS_SCAN:
		/* In the scan state abort the ongoing scan */
		cm_vdev_scan_cancel(wlan_vdev_get_pdev(cm_ctx->vdev),
				    cm_ctx->vdev);
		fallthrough;
	case WLAN_CM_SS_JOIN_PENDING:
		/*
		 * There would be pending disconnect requests in the list, and
		 * if they are flushed as part of new disconnect
		 * (cm_flush_pending_request), OS_IF would inform the kernel
		 * about the disconnect done even though the disconnect is still
		 * pending. So update OS_IF with invalid CM_ID so that the resp
		 * of only the new disconnect req is given to kernel.
		 */
		mlme_cm_osif_update_id_and_src(cm_ctx->vdev,
					       CM_SOURCE_INVALID,
					       CM_ID_INVALID);
		/*
		 * In case of scan or join pending there could be a connect and
		 * disconnect requests pending, so flush all the requests except
		 * the activated request.
		 */
		cm_flush_pending_request(cm_ctx, CONNECT_REQ_PREFIX, false);
		cm_flush_pending_request(cm_ctx, DISCONNECT_REQ_PREFIX, false);
		break;
	case WLAN_CM_S_INIT:
		/*
		 * In this case the vdev is already disconnected and thus the
		 * indication to upper layer, would have been sent as part of
		 * previous disconnect/connect failure.
		 *
		 * If upper layer is in process of connecting, sending
		 * disconnect indication back again may cause it to incorrectly
		 * think it as a connect failure. So sending disconnect
		 * indication again is not advisable.
		 *
		 * So no need to do anything here, just return failure and drop
		 * disconnect.
		 */
		mlme_info("vdev %d dropping disconnect req from source %d in INIT state",
			  wlan_vdev_get_id(cm_ctx->vdev), cm_req->req.source);
		return QDF_STATUS_E_ALREADY;
	default:
		mlme_err("Vdev %d disconnect req in invalid state %d",
			 wlan_vdev_get_id(cm_ctx->vdev),
			 cm_state_substate);
		return QDF_STATUS_E_FAILURE;
	};

	/* Queue the new disconnect request after state specific actions */
	return cm_add_disconnect_req_to_list(cm_ctx, cm_req);
}

QDF_STATUS cm_add_disconnect_req_to_list(struct cnx_mgr *cm_ctx,
					 struct cm_disconnect_req *req)
{
	QDF_STATUS status;
	struct cm_req *cm_req;

	cm_req = qdf_container_of(req, struct cm_req, discon_req);
	req->cm_id = cm_get_cm_id(cm_ctx, req->req.source);
	cm_req->cm_id = req->cm_id;
	status = cm_add_req_to_list_and_indicate_osif(cm_ctx, cm_req,
						      req->req.source);

	return status;
}

QDF_STATUS cm_disconnect_start_req(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_disconnect_req *req)
{
	struct cnx_mgr *cm_ctx;
	struct cm_req *cm_req;
	struct cm_disconnect_req *disconnect_req;
	QDF_STATUS status;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	/*
	 * This would be freed as part of removal from cm req list if adding
	 * to list is success after posting WLAN_CM_SM_EV_DISCONNECT_REQ.
	 */
	cm_req = qdf_mem_malloc(sizeof(*cm_req));

	if (!cm_req)
		return QDF_STATUS_E_NOMEM;

	if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	    !wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		req->is_no_disassoc_disconnect = 1;

	disconnect_req = &cm_req->discon_req;
	disconnect_req->req = *req;

	status = cm_sm_deliver_event(vdev, WLAN_CM_SM_EV_DISCONNECT_REQ,
				     sizeof(*disconnect_req), disconnect_req);
	/* free the req if disconnect is not handled */
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(cm_req);

	return status;
}

QDF_STATUS cm_disconnect_start_req_sync(struct wlan_objmgr_vdev *vdev,
					struct wlan_cm_disconnect_req *req)
{
	struct cnx_mgr *cm_ctx;
	QDF_STATUS status;
	uint32_t timeout;
	bool is_assoc_vdev = false;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	    !wlan_vdev_mlme_is_mlo_link_vdev(vdev)) {
		req->is_no_disassoc_disconnect = 1;
		is_assoc_vdev = true;
	}
	qdf_event_reset(&cm_ctx->disconnect_complete);
	status = cm_disconnect_start_req(vdev, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Disconnect failed with status %d", status);
		return status;
	}

	if (is_assoc_vdev)
		timeout = CM_DISCONNECT_CMD_TIMEOUT +
			  CM_DISCONNECT_ASSOC_VDEV_EXTRA_TIMEOUT;
	else
		timeout = CM_DISCONNECT_CMD_TIMEOUT;
	status = qdf_wait_single_event(&cm_ctx->disconnect_complete,
				       timeout);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("Disconnect timeout with status %d", status);

	return status;
}

QDF_STATUS cm_disconnect_rsp(struct wlan_objmgr_vdev *vdev,
			     struct wlan_cm_discon_rsp *resp)
{
	struct cnx_mgr *cm_ctx;
	QDF_STATUS qdf_status;
	wlan_cm_id cm_id;
	uint32_t prefix;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	cm_id = cm_ctx->active_cm_id;
	prefix = CM_ID_GET_PREFIX(cm_id);

	if (prefix != DISCONNECT_REQ_PREFIX || cm_id != resp->req.cm_id) {
		mlme_err(CM_PREFIX_FMT "Active cm_id 0x%x is different",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), resp->req.cm_id),
			 cm_id);
		qdf_status = QDF_STATUS_E_FAILURE;
		goto disconnect_complete;
	}
	qdf_status =
		cm_sm_deliver_event(vdev,
				    WLAN_CM_SM_EV_DISCONNECT_DONE,
				    sizeof(*resp), resp);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		goto disconnect_complete;

	return qdf_status;

disconnect_complete:
	/*
	 * If there is a event posting error it means the SM state is not in
	 * DISCONNECTING (some new cmd has changed the state of SM), so just
	 * complete the disconnect command.
	 */
	return cm_disconnect_complete(cm_ctx, resp);
}

QDF_STATUS cm_bss_peer_delete_req(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr *peer_mac)
{
	mlme_debug("vdev-id %d, delete peer" QDF_MAC_ADDR_FMT,
		   wlan_vdev_get_id(vdev), QDF_MAC_ADDR_REF(peer_mac->bytes));

	return mlme_cm_bss_peer_delete_req(vdev);
}

QDF_STATUS cm_vdev_down_req(struct wlan_objmgr_vdev *vdev, uint32_t status)
{
	mlme_debug("vdev id %d down req status %d",
		   wlan_vdev_get_id(vdev), status);

	return mlme_cm_vdev_down_req(vdev);
}
