/*
 * Copyright (c) 2012-2015, 2020, The Linux Foundation. All rights reserved.
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
#include <wlan_serialization_api.h>
#include "wlan_utility.h"
#include "wlan_scan_api.h"
#ifdef CONN_MGR_ADV_FEATURE
#include "wlan_blm_api.h"
#endif

static void
cm_send_disconnect_resp(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id)
{
	struct wlan_cm_discon_rsp resp;
	QDF_STATUS status;

	status = cm_fill_disconnect_resp_from_cm_id(cm_ctx, cm_id, &resp);
	if (QDF_IS_STATUS_SUCCESS(status))
		cm_disconnect_complete(cm_ctx, &resp);
}

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
		return cm_sm_deliver_event(
					cm_ctx->vdev,
					WLAN_CM_SM_EV_DISCONNECT_ACTIVE,
					sizeof(wlan_cm_id),
					&cmd->cmd_id);
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
		QDF_ASSERT(0);
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

#define DISCONNECT_TIMEOUT   STOP_RESPONSE_TIMER + DELETE_RESPONSE_TIMER + 1000

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

	cmd.cmd_type = WLAN_SER_CMD_VDEV_CONNECT;
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

#ifdef WLAN_FEATURE_INTERFACE_MGR
static void
cm_inform_if_mgr_disconnect_complete(struct wlan_objmgr_vdev *vdev)
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
cm_inform_if_mgr_disconnect_start(struct wlan_objmgr_vdev *vdev)
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

#else
static inline void
cm_inform_if_mgr_disconnect_complete(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
cm_inform_if_mgr_disconnect_start(struct wlan_objmgr_vdev *vdev)
{
}
#endif

void
cm_initiate_internal_disconnect(struct cnx_mgr *cm_ctx)
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

	status = cm_add_disconnect_req_to_list(cm_ctx, disconnect_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "failed to add disconnect req",
			 CM_PREFIX_REF(disconnect_req->req.vdev_id,
				       disconnect_req->cm_id));
		qdf_mem_free(cm_req);
		return;
	}

	status = cm_disconnect_start(cm_ctx, disconnect_req);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(cm_req);
}

QDF_STATUS cm_disconnect_start(struct cnx_mgr *cm_ctx,
			       struct cm_disconnect_req *req)
{
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(cm_ctx->vdev);
	if (!pdev)
		return QDF_STATUS_E_INVAL;

	 /* disconnect TDLS, P2P roc cleanup */

	cm_inform_if_mgr_disconnect_start(cm_ctx->vdev);
	cm_vdev_scan_cancel(wlan_vdev_get_pdev(cm_ctx->vdev), cm_ctx->vdev);
	mlme_cm_osif_disconnect_start_ind(cm_ctx->vdev);

	/* Serialize disconnect req, Handle failure status */
	status = cm_ser_disconnect_req(pdev, cm_ctx, req);

	if (QDF_IS_STATUS_ERROR(status))
		cm_send_disconnect_resp(cm_ctx, req->cm_id);

	return QDF_STATUS_SUCCESS;
}

static void
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
	struct qdf_mac_addr bssid;
	QDF_STATUS status;

	cm_req = cm_get_req_by_cm_id(cm_ctx, *cm_id);
	if (!cm_req)
		return QDF_STATUS_E_INVAL;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	cm_ctx->active_cm_id = *cm_id;

	wlan_vdev_get_bss_peer_mac(cm_ctx->vdev, &bssid);
	qdf_copy_macaddr(&req->req.bssid, &bssid);

	req->cm_id = *cm_id;
	req->req.vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	req->req.source = cm_req->discon_req.req.source;
	req->req.reason_code = cm_req->discon_req.req.reason_code;

	cm_update_scan_mlme_on_disconnect(cm_ctx->vdev,
					  &cm_req->discon_req);

	mlme_debug(CM_PREFIX_FMT "disconnect " QDF_MAC_ADDR_FMT " source %d reason %d",
		   CM_PREFIX_REF(req->req.vdev_id, req->cm_id),
		   QDF_MAC_ADDR_REF(req->req.bssid.bytes),
		   req->req.source, req->req.reason_code);
	status = mlme_cm_disconnect_req(cm_ctx->vdev, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "disconnect req fail",
			 CM_PREFIX_REF(req->req.vdev_id, req->cm_id));
		cm_send_disconnect_resp(cm_ctx, req->cm_id);
	}
	qdf_mem_free(req);

	return status;
}

#ifdef CONN_MGR_ADV_FEATURE
static void
cm_inform_blm_disconnect_complete(struct wlan_objmgr_vdev *vdev,
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

	wlan_blm_update_bssid_connect_params(pdev, resp->req.req.bssid,
					     BLM_AP_DISCONNECTED);
}
#else
static inline void
cm_inform_blm_disconnect_complete(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_discon_rsp *resp)
{}
#endif

QDF_STATUS cm_disconnect_complete(struct cnx_mgr *cm_ctx,
				  struct wlan_cm_discon_rsp *resp)
{
	/*
	 * If the entry is not present in the list, it must have been cleared
	 * already.
	 */
	if (!cm_get_req_by_cm_id(cm_ctx, resp->req.cm_id))
		return QDF_STATUS_SUCCESS;

	mlme_cm_disconnect_complete_ind(cm_ctx->vdev, resp);
	mlme_cm_osif_disconnect_complete(cm_ctx->vdev, resp);

	cm_inform_if_mgr_disconnect_complete(cm_ctx->vdev);

	cm_inform_blm_disconnect_complete(cm_ctx->vdev, resp);

	cm_remove_cmd(cm_ctx, resp->req.cm_id);

	return QDF_STATUS_SUCCESS;
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

	disconnect_req = &cm_req->discon_req;
	disconnect_req->req = *req;

	status = cm_sm_deliver_event(vdev, WLAN_CM_SM_EV_DISCONNECT_REQ,
				     sizeof(*disconnect_req), disconnect_req);
	/* free the req if disconnect is not handled */
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(cm_req);

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
