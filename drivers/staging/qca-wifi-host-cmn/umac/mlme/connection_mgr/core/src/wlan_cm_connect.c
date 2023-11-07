/*
 * Copyright (c) 2012-2015, 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Implements connect specific APIs of connection manager
 */

#include "wlan_cm_main_api.h"
#include "wlan_scan_api.h"
#include "wlan_cm_roam.h"
#include "wlan_cm_sm.h"
#ifdef WLAN_POLICY_MGR_ENABLE
#include "wlan_policy_mgr_api.h"
#endif
#include <wlan_serialization_api.h>
#ifdef CONN_MGR_ADV_FEATURE
#include "wlan_dlm_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_tdls_api.h"
#include "wlan_mlo_t2lm.h"
#include "wlan_t2lm_api.h"
#endif
#include <wlan_utility.h>
#include <wlan_mlo_mgr_sta.h>
#include "wlan_mlo_mgr_op.h"
#include <wlan_objmgr_vdev_obj.h>
#include "wlan_psoc_mlme_api.h"

void
cm_fill_failure_resp_from_cm_id(struct cnx_mgr *cm_ctx,
				struct wlan_cm_connect_resp *resp,
				wlan_cm_id cm_id,
				enum wlan_cm_connect_fail_reason reason)
{
	resp->connect_status = QDF_STATUS_E_FAILURE;
	resp->cm_id = cm_id;
	resp->vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	resp->reason = reason;
	/* Get bssid and ssid and freq for the cm id from the req list */
	cm_fill_bss_info_in_connect_rsp_by_cm_id(cm_ctx, cm_id, resp);
}

static QDF_STATUS cm_connect_cmd_timeout(struct cnx_mgr *cm_ctx,
					 wlan_cm_id cm_id)
{
	struct wlan_cm_connect_resp *resp;
	QDF_STATUS status;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp)
		return QDF_STATUS_E_NOMEM;

	cm_fill_failure_resp_from_cm_id(cm_ctx, resp, cm_id, CM_SER_TIMEOUT);
	status = cm_sm_deliver_event(cm_ctx->vdev,
				     WLAN_CM_SM_EV_CONNECT_FAILURE,
				     sizeof(*resp), resp);
	qdf_mem_free(resp);

	if (QDF_IS_STATUS_ERROR(status))
		cm_connect_handle_event_post_fail(cm_ctx, cm_id);

	return status;
}

#ifdef WLAN_CM_USE_SPINLOCK
static QDF_STATUS cm_activate_connect_req_sched_cb(struct scheduler_msg *msg)
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

	ret = cm_sm_deliver_event(vdev,
				  WLAN_CM_SM_EV_CONNECT_ACTIVE,
				  sizeof(wlan_cm_id),
				  &cmd->cmd_id);

	/*
	 * Called from scheduler context hence posting failure
	 */
	if (QDF_IS_STATUS_ERROR(ret)) {
		mlme_err(CM_PREFIX_FMT "Activation failed for cmd:%d",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), cmd->cmd_id),
			 cmd->cmd_type);
		cm_connect_handle_event_post_fail(cm_ctx, cmd->cmd_id);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
	return ret;
}

static QDF_STATUS
cm_activate_connect_req(struct wlan_serialization_command *cmd)
{
	struct wlan_objmgr_vdev *vdev = cmd->vdev;
	struct scheduler_msg msg = {0};
	QDF_STATUS ret;

	msg.bodyptr = cmd;
	msg.callback = cm_activate_connect_req_sched_cb;
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
cm_activate_connect_req(struct wlan_serialization_command *cmd)
{
	return cm_sm_deliver_event(cmd->vdev,
				   WLAN_CM_SM_EV_CONNECT_ACTIVE,
				   sizeof(wlan_cm_id),
				   &cmd->cmd_id);
}
#endif

static QDF_STATUS
cm_ser_connect_cb(struct wlan_serialization_command *cmd,
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
		/*
		 * For pending to active reason, use async api to take lock.
		 * For direct activation use sync api to avoid taking lock
		 * as lock is already acquired by the requester.
		 */
		if (cmd->activation_reason == SER_PENDING_TO_ACTIVE)
			status = cm_activate_connect_req(cmd);
		else
			status = cm_sm_deliver_event_sync(cm_ctx,
						   WLAN_CM_SM_EV_CONNECT_ACTIVE,
						   sizeof(wlan_cm_id),
						   &cmd->cmd_id);
		if (QDF_IS_STATUS_SUCCESS(status))
			break;
		/*
		 * Handle failure if posting fails, i.e. the SM state has
		 * changed or head cm_id doesn't match the active cm_id.
		 * connect active should be handled only in JOIN_PENDING. If
		 * new command has been received connect activation should be
		 * aborted from here with connect req cleanup.
		 */
		cm_connect_handle_event_post_fail(cm_ctx, cmd->cmd_id);
		break;
	case WLAN_SER_CB_CANCEL_CMD:
		/* command removed from pending list. */
		break;
	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		mlme_err(CM_PREFIX_FMT "Active command timeout",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), cmd->cmd_id));
		cm_trigger_panic_on_cmd_timeout(cm_ctx->vdev);
		cm_connect_cmd_timeout(cm_ctx, cmd->cmd_id);
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

static QDF_STATUS cm_ser_connect_req(struct wlan_objmgr_pdev *pdev,
				     struct cnx_mgr *cm_ctx,
				     struct cm_connect_req *cm_req)
{
	struct wlan_serialization_command cmd = {0, };
	enum wlan_serialization_status ser_cmd_status;
	QDF_STATUS status;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);

	status = wlan_objmgr_vdev_try_get_ref(cm_ctx->vdev, WLAN_MLME_CM_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "unable to get reference",
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		return status;
	}

	cmd.cmd_type = WLAN_SER_CMD_VDEV_CONNECT;
	cmd.cmd_id = cm_req->cm_id;
	cmd.cmd_cb = cm_ser_connect_cb;
	cmd.source = WLAN_UMAC_COMP_MLME;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = cm_ctx->connect_timeout;
	cmd.vdev = cm_ctx->vdev;
	cmd.is_blocking = true;

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
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id), ser_cmd_status);
		wlan_objmgr_vdev_release_ref(cm_ctx->vdev, WLAN_MLME_CM_ID);

		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void
cm_connect_handle_event_post_fail(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id)
{
	struct wlan_cm_connect_resp *resp;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp)
		return;

	cm_fill_failure_resp_from_cm_id(cm_ctx, resp, cm_id,
					CM_ABORT_DUE_TO_NEW_REQ_RECVD);
	cm_connect_complete(cm_ctx, resp);
	qdf_mem_free(resp);
}

QDF_STATUS
cm_send_connect_start_fail(struct cnx_mgr *cm_ctx,
			   struct cm_connect_req *req,
			   enum wlan_cm_connect_fail_reason reason)
{
	struct wlan_cm_connect_resp *resp;
	QDF_STATUS status;

	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp)
		return QDF_STATUS_E_NOMEM;

	cm_fill_failure_resp_from_cm_id(cm_ctx, resp, req->cm_id, reason);

	status = cm_sm_deliver_event_sync(cm_ctx, WLAN_CM_SM_EV_CONNECT_FAILURE,
					  sizeof(*resp), resp);
	qdf_mem_free(resp);

	return status;
}

#ifdef WLAN_POLICY_MGR_ENABLE

QDF_STATUS cm_handle_hw_mode_change(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id,
				    enum wlan_cm_sm_evt event)
{
	struct cm_req *cm_req;
	enum wlan_cm_connect_fail_reason reason = CM_GENERIC_FAILURE;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	if (!cm_id)
		return QDF_STATUS_E_FAILURE;

	cm_req = cm_get_req_by_cm_id(cm_ctx, *cm_id);
	if (!cm_req)
		return QDF_STATUS_E_INVAL;

	pdev = wlan_vdev_get_pdev(cm_ctx->vdev);
	if (!pdev) {
		mlme_err(CM_PREFIX_FMT "Failed to find pdev",
			 CM_PREFIX_REF(wlan_vdev_get_id(cm_ctx->vdev),
				       cm_req->cm_id));
		goto send_failure;
	}

	if (event == WLAN_CM_SM_EV_HW_MODE_SUCCESS) {
		status = cm_ser_connect_req(pdev, cm_ctx, &cm_req->connect_req);
		if (QDF_IS_STATUS_ERROR(status)) {
			reason = CM_SER_FAILURE;
			goto send_failure;
		}
		return status;
	}

	/* Set reason HW mode fail for event WLAN_CM_SM_EV_HW_MODE_FAILURE */
	reason = CM_HW_MODE_FAILURE;

send_failure:
	return cm_send_connect_start_fail(cm_ctx, &cm_req->connect_req, reason);
}

void cm_hw_mode_change_resp(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			    wlan_cm_id cm_id, QDF_STATUS status)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS qdf_status;
	enum wlan_cm_sm_evt event = WLAN_CM_SM_EV_HW_MODE_SUCCESS;
	struct cnx_mgr *cm_ctx;

	mlme_debug(CM_PREFIX_FMT "Continue connect after HW mode change, status %d",
		   CM_PREFIX_REF(vdev_id, cm_id), status);

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev)
		return;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return;
	}

	if (QDF_IS_STATUS_ERROR(status))
		event = WLAN_CM_SM_EV_HW_MODE_FAILURE;
	qdf_status = cm_sm_deliver_event(vdev, event, sizeof(wlan_cm_id),
					 &cm_id);

	/*
	 * Handle failure if posting fails, i.e. the SM state has
	 * changed or head cm_id doesn't match the active cm_id.
	 * hw mode change resp should be handled only in JOIN_PENDING. If
	 * new command has been received connect should be
	 * aborted from here with connect req cleanup.
	 */
	if (QDF_IS_STATUS_ERROR(status))
		cm_connect_handle_event_post_fail(cm_ctx, cm_id);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

static QDF_STATUS cm_check_for_hw_mode_change(struct wlan_objmgr_psoc *psoc,
					      qdf_list_t *scan_list,
					      uint8_t vdev_id,
					      wlan_cm_id connect_id)
{
	return policy_mgr_change_hw_mode_sta_connect(psoc, scan_list, vdev_id,
						     connect_id);
}


#else

static inline
QDF_STATUS cm_check_for_hw_mode_change(struct wlan_objmgr_psoc *psoc,
				       qdf_list_t *scan_list, uint8_t vdev_id,
				       uint8_t connect_id)
{
	return QDF_STATUS_E_ALREADY;
}

#endif /* WLAN_POLICY_MGR_ENABLE */

static inline void cm_delete_pmksa_for_bssid(struct cnx_mgr *cm_ctx,
					     struct qdf_mac_addr *bssid)
{
	struct wlan_crypto_pmksa pmksa;

	qdf_mem_zero(&pmksa, sizeof(pmksa));
	qdf_copy_macaddr(&pmksa.bssid, bssid);
	wlan_crypto_set_del_pmksa(cm_ctx->vdev, &pmksa, false);
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static inline
void cm_delete_pmksa_for_single_pmk_bssid(struct cnx_mgr *cm_ctx,
					  struct qdf_mac_addr *bssid)
{
	cm_delete_pmksa_for_bssid(cm_ctx, bssid);
}
#else
static inline
void cm_delete_pmksa_for_single_pmk_bssid(struct cnx_mgr *cm_ctx,
					  struct qdf_mac_addr *bssid)
{
}
#endif /* WLAN_SAE_SINGLE_PMK && WLAN_FEATURE_ROAM_OFFLOAD */

static inline void
cm_set_pmf_caps(struct wlan_cm_connect_req *req, struct scan_filter *filter)
{
	if (req->crypto.rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED)
		filter->pmf_cap = WLAN_PMF_REQUIRED;
	else if (req->crypto.rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED)
		filter->pmf_cap = WLAN_PMF_CAPABLE;
	else
		filter->pmf_cap = WLAN_PMF_DISABLED;
}

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static inline
void cm_set_vdev_link_id(struct cnx_mgr *cm_ctx,
			 struct cm_connect_req *req)
{
	uint8_t link_id;

	link_id = req->cur_candidate->entry->ml_info.self_link_id;
	if (cm_ctx->vdev) {
		mlme_debug("setting link ID to %d", link_id);
		wlan_vdev_set_link_id(cm_ctx->vdev, link_id);
	}
}

static void cm_update_vdev_mlme_macaddr(struct cnx_mgr *cm_ctx,
					struct cm_connect_req *req)
{
	struct qdf_mac_addr *mac;
	bool eht_capab;

	if (wlan_vdev_mlme_get_opmode(cm_ctx->vdev) != QDF_STA_MODE)
		return;

	wlan_psoc_mlme_get_11be_capab(wlan_vdev_get_psoc(cm_ctx->vdev),
				      &eht_capab);
	if (!eht_capab)
		return;

	mac = (struct qdf_mac_addr *)wlan_vdev_mlme_get_mldaddr(cm_ctx->vdev);

	if (req->cur_candidate->entry->ie_list.multi_link_bv &&
	    !qdf_is_macaddr_zero(mac)) {
		wlan_vdev_obj_lock(cm_ctx->vdev);
		/* Use link address for ML connection */
		wlan_vdev_mlme_set_macaddr(cm_ctx->vdev,
					   cm_ctx->vdev->vdev_mlme.linkaddr);
		wlan_vdev_obj_unlock(cm_ctx->vdev);
		wlan_vdev_mlme_set_mlo_vdev(cm_ctx->vdev);
		mlme_debug("set link address for ML connection");
	} else {
		/* Use net_dev address for non-ML connection */
		if (!qdf_is_macaddr_zero(mac)) {
			wlan_vdev_obj_lock(cm_ctx->vdev);
			wlan_vdev_mlme_set_macaddr(cm_ctx->vdev, mac->bytes);
			wlan_vdev_obj_unlock(cm_ctx->vdev);
			mlme_debug(QDF_MAC_ADDR_FMT " for non-ML connection",
				   QDF_MAC_ADDR_REF(mac->bytes));
		}

		wlan_vdev_mlme_clear_mlo_vdev(cm_ctx->vdev);
		mlme_debug("clear MLO cap for non-ML connection");
	}
}
#else
static inline
void cm_set_vdev_link_id(struct cnx_mgr *cm_ctx,
			 struct cm_connect_req *req)
{ }

static void cm_update_vdev_mlme_macaddr(struct cnx_mgr *cm_ctx,
					struct cm_connect_req *req)
{
}
#endif
/**
 * cm_get_bss_peer_mld_addr() - get bss peer mld mac address
 * @req: pointer to cm_connect_req
 *
 * Return: mld mac address
 */
static struct qdf_mac_addr *cm_get_bss_peer_mld_addr(struct cm_connect_req *req)
{
	if (req && req->cur_candidate && req->cur_candidate->entry)
		return &req->cur_candidate->entry->ml_info.mld_mac_addr;
	else
		return NULL;
}

/**
 * cm_bss_peer_is_assoc_peer() - is the bss peer to be created assoc peer or not
 * @req: pointer to cm_connect_req
 *
 * Return: true if the bss peer to be created is assoc peer
 */
static bool cm_bss_peer_is_assoc_peer(struct cm_connect_req *req)
{
	if (req)
		return !req->req.is_non_assoc_link;

	return false;
}

/**
 * cm_candidate_mlo_update() - handle mlo scenario for candidate validating
 * @scan_entry: scan result of the candidate
 * @validate_bss_info: candidate info to be updated
 *
 * Return: None
 */
static void
cm_candidate_mlo_update(struct scan_cache_entry *scan_entry,
			struct validate_bss_data *validate_bss_info)
{
	validate_bss_info->is_mlo = !!scan_entry->ie_list.multi_link_bv;
	validate_bss_info->scan_entry = scan_entry;
}
#else
static inline
void cm_set_vdev_link_id(struct cnx_mgr *cm_ctx,
			 struct cm_connect_req *req)
{ }

static void cm_update_vdev_mlme_macaddr(struct cnx_mgr *cm_ctx,
					struct cm_connect_req *req)
{
}

static struct qdf_mac_addr *cm_get_bss_peer_mld_addr(struct cm_connect_req *req)
{
	return NULL;
}

static bool cm_bss_peer_is_assoc_peer(struct cm_connect_req *req)
{
	return false;
}

static inline void
cm_candidate_mlo_update(struct scan_cache_entry *scan_entry,
			struct validate_bss_data *validate_bss_info)
{
}
#endif

static void cm_create_bss_peer(struct cnx_mgr *cm_ctx,
			       struct cm_connect_req *req)
{
	QDF_STATUS status;
	struct qdf_mac_addr *bssid;
	struct qdf_mac_addr *mld_mac = NULL;
	bool is_assoc_link = false;
	bool eht_capab;

	if (!cm_ctx) {
		mlme_err("invalid cm_ctx");
		return;
	}
	if (!req || !req->cur_candidate || !req->cur_candidate->entry) {
		mlme_err("invalid req");
		return;
	}

	wlan_psoc_mlme_get_11be_capab(wlan_vdev_get_psoc(cm_ctx->vdev),
				      &eht_capab);
	if (eht_capab && wlan_vdev_mlme_is_mlo_vdev(cm_ctx->vdev)) {
		cm_set_vdev_link_id(cm_ctx, req);
		wlan_mlo_init_cu_bpcc(cm_ctx->vdev);
		mld_mac = cm_get_bss_peer_mld_addr(req);
		is_assoc_link = cm_bss_peer_is_assoc_peer(req);
	}

	bssid = &req->cur_candidate->entry->bssid;
	status = mlme_cm_bss_peer_create_req(cm_ctx->vdev, bssid,
					     mld_mac, is_assoc_link);
	if (QDF_IS_STATUS_ERROR(status)) {
		struct wlan_cm_connect_resp *resp;
		uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);

		/* In case of failure try with next candidate */
		mlme_err(CM_PREFIX_FMT "peer create request failed %d",
			 CM_PREFIX_REF(vdev_id, req->cm_id), status);

		resp = qdf_mem_malloc(sizeof(*resp));
		if (!resp)
			return;

		cm_fill_failure_resp_from_cm_id(cm_ctx, resp, req->cm_id,
						CM_PEER_CREATE_FAILED);
		cm_sm_deliver_event_sync(cm_ctx,
				WLAN_CM_SM_EV_CONNECT_GET_NEXT_CANDIDATE,
				sizeof(*resp), resp);
		qdf_mem_free(resp);
	}
}

#if defined(CONN_MGR_ADV_FEATURE) && defined(WLAN_FEATURE_11BE_MLO)
static QDF_STATUS
cm_t2lm_validate_candidate(struct cnx_mgr *cm_ctx,
			   struct scan_cache_entry *scan_entry)
{
	return wlan_t2lm_validate_candidate(cm_ctx, scan_entry);
}
#else
static inline QDF_STATUS
cm_t2lm_validate_candidate(struct cnx_mgr *cm_ctx,
			   struct scan_cache_entry *scan_entry)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static
QDF_STATUS cm_if_mgr_validate_candidate(struct cnx_mgr *cm_ctx,
					struct scan_cache_entry *scan_entry)
{
	struct if_mgr_event_data event_data = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	event_data.validate_bss_info.chan_freq = scan_entry->channel.chan_freq;
	event_data.validate_bss_info.beacon_interval = scan_entry->bcn_int;
	qdf_copy_macaddr(&event_data.validate_bss_info.peer_addr,
			 &scan_entry->bssid);
	cm_candidate_mlo_update(scan_entry, &event_data.validate_bss_info);

	status = cm_t2lm_validate_candidate(cm_ctx, scan_entry);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return if_mgr_deliver_event(cm_ctx->vdev,
				    WLAN_IF_MGR_EV_VALIDATE_CANDIDATE,
				    &event_data);
}

#ifdef CONN_MGR_ADV_FEATURE
#ifdef WLAN_FEATURE_FILS_SK
/*
 * cm_create_fils_realm_hash: API to create hash using realm
 * @fils_info: fils connection info obtained from supplicant
 * @tmp_hash: pointer to new hash
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_create_fils_realm_hash(struct wlan_fils_con_info *fils_info,
			  uint8_t *tmp_hash)
{
	uint8_t *hash;
	uint8_t *data;

	if (!fils_info->realm_len)
		return QDF_STATUS_E_NOSUPPORT;

	hash = qdf_mem_malloc(SHA256_DIGEST_SIZE);
	if (!hash)
		return QDF_STATUS_E_NOMEM;

	data = fils_info->realm;
	qdf_get_hash(SHA256_CRYPTO_TYPE, 1, &data, &fils_info->realm_len, hash);
	qdf_mem_copy(tmp_hash, hash, REALM_HASH_LEN);
	qdf_mem_free(hash);

	return QDF_STATUS_SUCCESS;
}

static void cm_update_fils_scan_filter(struct scan_filter *filter,
				       struct cm_connect_req *cm_req)

{
	uint8_t realm_hash[REALM_HASH_LEN];
	QDF_STATUS status;

	if (!cm_req->req.fils_info.is_fils_connection)
		return;

	status = cm_create_fils_realm_hash(&cm_req->req.fils_info, realm_hash);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	filter->fils_scan_filter.realm_check = true;
	mlme_debug(CM_PREFIX_FMT "creating realm based on fils info",
		   CM_PREFIX_REF(cm_req->req.vdev_id, cm_req->cm_id));
	qdf_mem_copy(filter->fils_scan_filter.fils_realm, realm_hash,
		     REALM_HASH_LEN);
}

static inline bool cm_is_fils_connection(struct wlan_cm_connect_resp *resp)
{
	return resp->is_fils_connection;
}

static QDF_STATUS cm_set_fils_key(struct cnx_mgr *cm_ctx,
				  struct wlan_cm_connect_resp *resp)
{
	struct fils_connect_rsp_params *fils_ie;

	fils_ie = resp->connect_ies.fils_ie;

	if (!fils_ie)
		return QDF_STATUS_E_INVAL;

	cm_store_fils_key(cm_ctx, true, 0, fils_ie->tk_len, fils_ie->tk,
			  &resp->bssid, resp->cm_id);
	cm_store_fils_key(cm_ctx, false, 2, fils_ie->gtk_len, fils_ie->gtk,
			  &resp->bssid, resp->cm_id);
	cm_set_key(cm_ctx, true, 0, &resp->bssid);
	cm_set_key(cm_ctx, false, 2, &resp->bssid);

	return QDF_STATUS_SUCCESS;
}

#else
static inline void cm_update_fils_scan_filter(struct scan_filter *filter,
					      struct cm_connect_req *cm_req)
{ }

static inline bool cm_is_fils_connection(struct wlan_cm_connect_resp *resp)
{
	return false;
}

static inline QDF_STATUS cm_set_fils_key(struct cnx_mgr *cm_ctx,
					 struct wlan_cm_connect_resp *resp)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_FILS_SK */

/**
 * cm_get_vdev_id_with_active_vdev_op() - Get vdev id from serialization
 * pending queue for which disconnect or connect is ongoing
 * @pdev: pdev common object
 * @object: vdev object
 * @arg: vdev operation search arg
 *
 * Return: None
 */
static void cm_get_vdev_id_with_active_vdev_op(struct wlan_objmgr_pdev *pdev,
					       void *object, void *arg)
{
	struct vdev_op_search_arg *vdev_arg = arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);

	if (!vdev_arg)
		return;

	/* Avoid same vdev id check */
	if (vdev_arg->current_vdev_id == vdev_id)
		return;

	if (opmode == QDF_STA_MODE || opmode == QDF_P2P_CLIENT_MODE) {
		if (cm_is_vdev_disconnecting(vdev))
			vdev_arg->sta_cli_vdev_id = vdev_id;
		return;
	}

	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE ||
	    opmode == QDF_NDI_MODE) {
		/* Check if START/STOP AP OP is in progress */
		if (wlan_ser_is_non_scan_cmd_type_in_vdev_queue(vdev,
					WLAN_SER_CMD_VDEV_START_BSS) ||
		    wlan_ser_is_non_scan_cmd_type_in_vdev_queue(vdev,
					WLAN_SER_CMD_VDEV_STOP_BSS))
			vdev_arg->sap_go_vdev_id = vdev_id;
		return;
	}
}

/**
 * cm_is_any_other_vdev_connecting_disconnecting() - check whether any other
 * vdev is in waiting for vdev operations (connect/disconnect or start/stop AP)
 * @cm_ctx: connection manager context
 * @cm_req: Connect request.
 *
 * As Connect is a blocking call this API will make sure the vdev operations on
 * other vdev doesn't starve
 *
 * Return : true if any other vdev has pending operation
 */
static bool
cm_is_any_other_vdev_connecting_disconnecting(struct cnx_mgr *cm_ctx,
					      struct cm_req *cm_req)
{
	struct wlan_objmgr_pdev *pdev;
	uint8_t cur_vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	struct vdev_op_search_arg vdev_arg;

	pdev = wlan_vdev_get_pdev(cm_ctx->vdev);
	if (!pdev) {
		mlme_err(CM_PREFIX_FMT "Failed to find pdev",
			 CM_PREFIX_REF(cur_vdev_id, cm_req->cm_id));
		return false;
	}

	vdev_arg.current_vdev_id = cur_vdev_id;
	vdev_arg.sap_go_vdev_id = WLAN_INVALID_VDEV_ID;
	vdev_arg.sta_cli_vdev_id = WLAN_INVALID_VDEV_ID;
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  cm_get_vdev_id_with_active_vdev_op,
					  &vdev_arg, 0,
					  WLAN_MLME_CM_ID);

	/* For STA/CLI avoid the fist candidate itself if possible */
	if (vdev_arg.sta_cli_vdev_id != WLAN_INVALID_VDEV_ID) {
		mlme_info(CM_PREFIX_FMT "Abort connection as sta/cli vdev %d is disconnecting",
			  CM_PREFIX_REF(cur_vdev_id, cm_req->cm_id),
			  vdev_arg.sta_cli_vdev_id);
		return true;
	}

	/*
	 * For SAP/GO ops pending avoid the next candidate, this is to support
	 * wifi sharing etc use case where we need to connect to AP in parallel
	 * to SAP operation, so try atleast one candidate.
	 */
	if (cm_req->connect_req.cur_candidate &&
	    vdev_arg.sap_go_vdev_id != WLAN_INVALID_VDEV_ID) {
		mlme_info(CM_PREFIX_FMT "Avoid next candidate as SAP/GO/NDI vdev %d has pending vdev op",
			  CM_PREFIX_REF(cur_vdev_id, cm_req->cm_id),
			  vdev_arg.sap_go_vdev_id);
		return true;
	}

	return false;
}

QDF_STATUS
cm_inform_dlm_connect_complete(struct wlan_objmgr_vdev *vdev,
			       struct wlan_cm_connect_resp *resp)
{
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlme_err(CM_PREFIX_FMT "Failed to find pdev",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), resp->cm_id));
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_IS_STATUS_SUCCESS(resp->connect_status))
		wlan_dlm_update_bssid_connect_params(pdev, resp->bssid,
						     DLM_AP_CONNECTED);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_is_retry_with_same_candidate() - This API check if reconnect attempt is
 * required with the same candidate again
 * @cm_ctx: connection manager context
 * @req: Connect request.
 * @resp: connect resp from previous connection attempt
 *
 * This function return true if same candidate needs to be tried again
 *
 * Return: bool
 */
static bool cm_is_retry_with_same_candidate(struct cnx_mgr *cm_ctx,
					    struct cm_connect_req *req,
					    struct wlan_cm_connect_resp *resp)
{
	uint8_t max_retry_count = CM_MAX_CANDIDATE_RETRIES;
	uint32_t key_mgmt;
	struct wlan_objmgr_psoc *psoc;
	bool sae_connection;
	QDF_STATUS status;
	qdf_freq_t freq;

	psoc = wlan_pdev_get_psoc(wlan_vdev_get_pdev(cm_ctx->vdev));
	key_mgmt = req->cur_candidate->entry->neg_sec_info.key_mgmt;
	freq = req->cur_candidate->entry->channel.chan_freq;

	/* Try once again for the invalid PMKID case without PMKID */
	if (resp->status_code == STATUS_INVALID_PMKID)
		goto use_same_candidate;

	/* Try again for the JOIN timeout if only one candidate */
	if (resp->reason == CM_JOIN_TIMEOUT &&
	    qdf_list_size(req->candidate_list) == 1) {
		/*
		 * If there is a interface connected which can lead to MCC,
		 * do not retry as it can lead to beacon miss on that interface.
		 * Coz as part of vdev start mac remain on candidate freq for 3
		 * sec.
		 */
		if (policy_mgr_will_freq_lead_to_mcc(psoc, freq))
			return false;

		wlan_mlme_get_sae_assoc_retry_count(psoc, &max_retry_count);
		goto use_same_candidate;
	}

	/*
	 * Try again for the ASSOC timeout in SAE connection or
	 * AP has reconnect on assoc timeout OUI.
	 */
	sae_connection = key_mgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE |
				     1 << WLAN_CRYPTO_KEY_MGMT_FT_SAE |
				     1 << WLAN_CRYPTO_KEY_MGMT_SAE_EXT_KEY |
				     1 << WLAN_CRYPTO_KEY_MGMT_FT_SAE_EXT_KEY);
	if (resp->reason == CM_ASSOC_TIMEOUT && (sae_connection ||
	    (mlme_get_reconn_after_assoc_timeout_flag(psoc, resp->vdev_id)))) {
		/* For SAE use max retry count from INI */
		if (sae_connection)
			wlan_mlme_get_sae_assoc_retry_count(psoc,
							    &max_retry_count);
		goto use_same_candidate;
	}

	return false;

use_same_candidate:
	if (req->cur_candidate_retries >= max_retry_count)
		return false;

	status = cm_if_mgr_validate_candidate(cm_ctx,
					      req->cur_candidate->entry);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	mlme_info(CM_PREFIX_FMT "Retry again with " QDF_MAC_ADDR_FMT ", status code %d reason %d key_mgmt 0x%x retry count %d max retry %d",
		  CM_PREFIX_REF(resp->vdev_id, resp->cm_id),
		  QDF_MAC_ADDR_REF(resp->bssid.bytes), resp->status_code,
		  resp->reason, key_mgmt, req->cur_candidate_retries,
		  max_retry_count);

	req->cur_candidate_retries++;

	return true;
}

/*
 * Do not allow last connect attempt after 25 sec, assuming last attempt will
 * complete in max 10 sec, total connect time will not be more than 35 sec.
 * Do not confuse this with active command timeout, that is taken care by
 * CM_MAX_PER_CANDIDATE_CONNECT_TIMEOUT
 */
#define CM_CONNECT_MAX_ACTIVE_TIME 25000

/**
 * cm_is_time_allowed_for_connect_attempt() - This API check if next connect
 * attempt can be tried within allocated time.
 * @cm_ctx: connection manager context
 * @req: Connect request.
 *
 * This function return true if connect attempt can be tried so that total time
 * taken by connect req do not exceed 30-35 seconds.
 *
 * Return: bool
 */
static bool cm_is_time_allowed_for_connect_attempt(struct cnx_mgr *cm_ctx,
						   struct cm_connect_req *req)
{
	qdf_time_t time_since_connect_active;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);

	time_since_connect_active = qdf_mc_timer_get_system_time() -
					req->connect_active_time;
	if (time_since_connect_active >= CM_CONNECT_MAX_ACTIVE_TIME) {
		mlme_info(CM_PREFIX_FMT "Max time allocated (%d ms) for connect completed, cur time %lu, active time %lu and diff %lu",
			  CM_PREFIX_REF(vdev_id, req->cm_id),
			  CM_CONNECT_MAX_ACTIVE_TIME,
			  qdf_mc_timer_get_system_time(),
			  req->connect_active_time,
			  time_since_connect_active);
		return false;
	}

	return true;
}

static inline void cm_update_advance_filter(struct wlan_objmgr_pdev *pdev,
					    struct cnx_mgr *cm_ctx,
					    struct scan_filter *filter,
					    struct cm_connect_req *cm_req)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	/* Select only ESS type */
	filter->bss_type = WLAN_TYPE_BSS;
	filter->enable_adaptive_11r =
		wlan_mlme_adaptive_11r_enabled(psoc);
	if (wlan_vdev_mlme_get_opmode(cm_ctx->vdev) != QDF_STA_MODE)
		return;
	/* For link vdev, we don't filter any channels.
	 * Dual STA mode, one link can be disabled in post connection
	 * if needed.
	 */
	if (!cm_req->req.is_non_assoc_link)
		wlan_cm_dual_sta_roam_update_connect_channels(psoc, filter);
	filter->dot11mode = cm_req->req.dot11mode_filter;
	cm_update_fils_scan_filter(filter, cm_req);
}

static void cm_update_security_filter(struct scan_filter *filter,
				      struct wlan_cm_connect_req *req)
{
	/* Ignore security match for rsn override, OSEN and WPS connection */
	if (req->force_rsne_override || req->is_wps_connection ||
	    req->is_osen_connection) {
		filter->ignore_auth_enc_type = 1;
		return;
	}

	filter->authmodeset = req->crypto.auth_type;
	filter->ucastcipherset = req->crypto.ciphers_pairwise;
	filter->key_mgmt = req->crypto.akm_suites;
	filter->mcastcipherset = req->crypto.group_cipher;
	filter->mgmtcipherset = req->crypto.mgmt_ciphers;
	cm_set_pmf_caps(req, filter);
}

/**
 * cm_set_fils_wep_key() - check and set wep or fils keys if required
 * @cm_ctx: connection manager context
 * @resp: connect resp
 *
 * Context: Can be called from any context and to be used only after posting a
 * msg to SM (ie holding the SM lock) i.e. on successful connection.
 */
static void cm_set_fils_wep_key(struct cnx_mgr *cm_ctx,
				struct wlan_cm_connect_resp *resp)
{
	int32_t cipher;
	struct qdf_mac_addr broadcast_mac = QDF_MAC_ADDR_BCAST_INIT;

	/* Check and set FILS keys */
	if (cm_is_fils_connection(resp)) {
		cm_set_fils_key(cm_ctx, resp);
		return;
	}
	/* Check and set WEP keys */
	cipher = wlan_crypto_get_param(cm_ctx->vdev,
				       WLAN_CRYPTO_PARAM_UCAST_CIPHER);
	if (cipher < 0)
		return;

	if (!(cipher & (1 << WLAN_CRYPTO_CIPHER_WEP_40 |
			1 << WLAN_CRYPTO_CIPHER_WEP_104)))
		return;

	cm_set_key(cm_ctx, true, 0, &resp->bssid);
	cm_set_key(cm_ctx, false, 0, &broadcast_mac);
}

static void cm_teardown_tdls(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;

	wlan_tdls_teardown_links_sync(psoc);
}

#else
static inline bool
cm_is_any_other_vdev_connecting_disconnecting(struct cnx_mgr *cm_ctx,
					      struct cm_req *cm_req)
{
	return false;
}

static inline
bool cm_is_retry_with_same_candidate(struct cnx_mgr *cm_ctx,
				     struct cm_connect_req *req,
				     struct wlan_cm_connect_resp *resp)
{
	return false;
}

static inline
bool cm_is_time_allowed_for_connect_attempt(struct cnx_mgr *cm_ctx,
					    struct cm_connect_req *req)
{
	return true;
}

static inline void cm_update_advance_filter(struct wlan_objmgr_pdev *pdev,
					    struct cnx_mgr *cm_ctx,
					    struct scan_filter *filter,
					    struct cm_connect_req *cm_req)
{
	struct wlan_objmgr_vdev *vdev = cm_ctx->vdev;

	if (cm_ctx->cm_candidate_advance_filter)
		cm_ctx->cm_candidate_advance_filter(vdev, filter);
}

static void cm_update_security_filter(struct scan_filter *filter,
				      struct wlan_cm_connect_req *req)
{
	if (!QDF_HAS_PARAM(req->crypto.auth_type, WLAN_CRYPTO_AUTH_WAPI) &&
	    !QDF_HAS_PARAM(req->crypto.auth_type, WLAN_CRYPTO_AUTH_RSNA) &&
	    !QDF_HAS_PARAM(req->crypto.auth_type, WLAN_CRYPTO_AUTH_WPA)) {
		filter->ignore_auth_enc_type = 1;
		return;
	}

	filter->authmodeset = req->crypto.auth_type;
	filter->ucastcipherset = req->crypto.ciphers_pairwise;
	filter->key_mgmt = req->crypto.akm_suites;
	filter->mcastcipherset = req->crypto.group_cipher;
	filter->mgmtcipherset = req->crypto.mgmt_ciphers;
	cm_set_pmf_caps(req, filter);
}

static inline void cm_set_fils_wep_key(struct cnx_mgr *cm_ctx,
				       struct wlan_cm_connect_resp *resp)
{}

QDF_STATUS
cm_peer_create_on_bss_select_ind_resp(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id)
{
	struct cm_req *cm_req;

	cm_req = cm_get_req_by_cm_id(cm_ctx, *cm_id);
	if (!cm_req)
		return QDF_STATUS_E_FAILURE;

	/* Update vdev mlme mac address based on connection type */
	cm_update_vdev_mlme_macaddr(cm_ctx, &cm_req->connect_req);

	cm_create_bss_peer(cm_ctx, &cm_req->connect_req);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cm_bss_select_ind_rsp(struct wlan_objmgr_vdev *vdev,
				 QDF_STATUS status)
{
	struct cnx_mgr *cm_ctx;
	QDF_STATUS qdf_status;
	wlan_cm_id cm_id;
	uint32_t prefix;
	struct wlan_cm_connect_resp *resp;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	cm_id = cm_ctx->active_cm_id;
	prefix = CM_ID_GET_PREFIX(cm_id);

	if (prefix != CONNECT_REQ_PREFIX) {
		mlme_err(CM_PREFIX_FMT "active req is not connect req",
			 CM_PREFIX_REF(wlan_vdev_get_id(cm_ctx->vdev), cm_id));
		return QDF_STATUS_E_INVAL;
	}

	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_status =
			cm_sm_deliver_event(vdev,
				WLAN_CM_SM_EV_BSS_SELECT_IND_SUCCESS,
				sizeof(wlan_cm_id), &cm_id);
		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			return qdf_status;

		goto post_err;
	}

	/* In case of failure try with next candidate */
	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		qdf_status = QDF_STATUS_E_NOMEM;
		goto post_err;
	}

	cm_fill_failure_resp_from_cm_id(cm_ctx, resp, cm_id,
					CM_BSS_SELECT_IND_FAILED);
	qdf_status =
		cm_sm_deliver_event(vdev,
				    WLAN_CM_SM_EV_CONNECT_GET_NEXT_CANDIDATE,
				    sizeof(*resp), resp);
	qdf_mem_free(resp);
	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		return qdf_status;

post_err:
	/*
	 * If there is a event posting error it means the SM state is not in
	 * JOIN ACTIVE (some new cmd has changed the state of SM), so just
	 * complete the connect command.
	 */
	cm_connect_handle_event_post_fail(cm_ctx, cm_id);
	return qdf_status;
}

static inline void cm_teardown_tdls(struct wlan_objmgr_vdev *vdev) {}

#endif /* CONN_MGR_ADV_FEATURE */

static void cm_connect_prepare_scan_filter(struct wlan_objmgr_pdev *pdev,
					   struct cnx_mgr *cm_ctx,
					   struct cm_connect_req *cm_req,
					   struct scan_filter *filter,
					   bool security_valid_for_6ghz)
{
	if (!qdf_is_macaddr_zero(&cm_req->req.bssid)) {
		filter->num_of_bssid = 1;
		qdf_copy_macaddr(&filter->bssid_list[0], &cm_req->req.bssid);
	}

	qdf_copy_macaddr(&filter->bssid_hint, &cm_req->req.bssid_hint);
	filter->num_of_ssid = 1;
	qdf_mem_copy(&filter->ssid_list[0], &cm_req->req.ssid,
		     sizeof(struct wlan_ssid));

	if (cm_req->req.chan_freq) {
		filter->num_of_channels = 1;
		filter->chan_freq_list[0] = cm_req->req.chan_freq;
	}

	/* Security is not valid for 6Ghz so ignore 6Ghz APs */
	if (!security_valid_for_6ghz)
		filter->ignore_6ghz_channel = true;

	if (cm_req->req.is_non_assoc_link)
		filter->ignore_6ghz_channel = false;

	cm_update_security_filter(filter, &cm_req->req);
	cm_update_advance_filter(pdev, cm_ctx, filter, cm_req);
}

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static QDF_STATUS cm_is_scan_support(struct cm_connect_req *cm_req)
{
	if (cm_req->req.is_non_assoc_link)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS cm_is_scan_support(struct cm_connect_req *cm_req)
{
	if (cm_req->req.ml_parnter_info.num_partner_links)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#endif
#else
static QDF_STATUS cm_is_scan_support(struct cm_connect_req *cm_req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_11BE_MLO_ADV_FEATURE)
#define CFG_MLO_ASSOC_LINK_BAND_MAX 0x70

static QDF_STATUS cm_update_mlo_filter(struct wlan_objmgr_pdev *pdev,
				       struct cm_connect_req *cm_req,
				       struct scan_filter *filter)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	filter->band_bitmap = wlan_mlme_get_sta_mlo_conn_band_bmp(psoc);
	/* Apply assoc band filter only for assoc link */
	if (cm_req->req.is_non_assoc_link) {
		filter->band_bitmap =  filter->band_bitmap |
				       CFG_MLO_ASSOC_LINK_BAND_MAX;
	}
	mlme_debug("band bitmap: 0x%x", filter->band_bitmap);

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS cm_update_mlo_filter(struct wlan_objmgr_pdev *pdev,
				       struct cm_connect_req *cm_req,
				       struct scan_filter *filter)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS
cm_connect_fetch_candidates(struct wlan_objmgr_pdev *pdev,
			    struct cnx_mgr *cm_ctx,
			    struct cm_connect_req *cm_req,
			    qdf_list_t **fetched_candidate_list,
			    uint32_t *num_bss_found)
{
	struct scan_filter *filter;
	uint32_t num_bss = 0;
	enum QDF_OPMODE op_mode;
	qdf_list_t *candidate_list;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	bool security_valid_for_6ghz;
	const uint8_t *rsnxe;

	rsnxe = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSNXE,
					 cm_req->req.assoc_ie.ptr,
					 cm_req->req.assoc_ie.len);
	security_valid_for_6ghz =
		wlan_cm_6ghz_allowed_for_akm(wlan_pdev_get_psoc(pdev),
					     cm_req->req.crypto.akm_suites,
					     cm_req->req.crypto.rsn_caps,
					     rsnxe, cm_req->req.sae_pwe,
					     cm_req->req.is_wps_connection);

	/*
	 * Ignore connect req if the freq is provided and its 6Ghz and
	 * security is not valid for 6Ghz
	 */
	if (cm_req->req.chan_freq && !security_valid_for_6ghz &&
	    WLAN_REG_IS_6GHZ_CHAN_FREQ(cm_req->req.chan_freq)) {
		mlme_info(CM_PREFIX_FMT "6ghz freq (%d) given and 6Ghz not allowed for the security in connect req",
			  CM_PREFIX_REF(vdev_id, cm_req->cm_id),
			  cm_req->req.chan_freq);
		return QDF_STATUS_E_INVAL;
	}
	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		return QDF_STATUS_E_NOMEM;

	cm_connect_prepare_scan_filter(pdev, cm_ctx, cm_req, filter,
				       security_valid_for_6ghz);

	cm_update_mlo_filter(pdev, cm_req, filter);

	candidate_list = wlan_scan_get_result(pdev, filter);
	if (candidate_list) {
		num_bss = qdf_list_size(candidate_list);
		mlme_debug(CM_PREFIX_FMT "num_entries found %d",
			   CM_PREFIX_REF(vdev_id, cm_req->cm_id), num_bss);
	}
	*num_bss_found = num_bss;
	op_mode = wlan_vdev_mlme_get_opmode(cm_ctx->vdev);
	if (num_bss && op_mode == QDF_STA_MODE &&
	    !cm_req->req.is_non_assoc_link)
		cm_calculate_scores(cm_ctx, pdev, filter, candidate_list);
	qdf_mem_free(filter);

	if (!candidate_list || !qdf_list_size(candidate_list)) {
		if (candidate_list)
			wlan_scan_purge_results(candidate_list);
		return QDF_STATUS_E_EMPTY;
	}
	*fetched_candidate_list = candidate_list;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS cm_connect_get_candidates(struct wlan_objmgr_pdev *pdev,
					    struct cnx_mgr *cm_ctx,
					    struct cm_connect_req *cm_req)
{
	uint32_t num_bss = 0;
	qdf_list_t *candidate_list = NULL;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	QDF_STATUS status;

	status = cm_connect_fetch_candidates(pdev, cm_ctx, cm_req,
					     &candidate_list, &num_bss);
	if (QDF_IS_STATUS_ERROR(status)) {
		if (candidate_list)
			wlan_scan_purge_results(candidate_list);
		mlme_info(CM_PREFIX_FMT "no valid candidate found, num_bss %d scan_id %d",
			  CM_PREFIX_REF(vdev_id, cm_req->cm_id), num_bss,
			  cm_req->scan_id);

		/*
		 * If connect scan was already done OR candidate were found
		 * but none of them were valid OR if ML link connection
		 * return QDF_STATUS_E_EMPTY.
		 */
		if (cm_req->scan_id || num_bss ||
		    QDF_IS_STATUS_ERROR(cm_is_scan_support(cm_req)))
			return QDF_STATUS_E_EMPTY;

		/* Try connect scan to search for any valid candidate */
		status = cm_sm_deliver_event_sync(cm_ctx, WLAN_CM_SM_EV_SCAN,
						  sizeof(*cm_req), cm_req);
		/*
		 * If connect scan is initiated, return pending, so that
		 * connect start after scan complete
		 */
		if (QDF_IS_STATUS_SUCCESS(status))
			status = QDF_STATUS_E_PENDING;

		return status;
	}
	cm_req->candidate_list = candidate_list;

	return status;
}

#ifdef CONN_MGR_ADV_FEATURE
static void cm_update_candidate_list(struct cnx_mgr *cm_ctx,
				     struct cm_connect_req *cm_req,
				     struct scan_cache_node *prev_candidate)
{
	struct wlan_objmgr_pdev *pdev;
	uint32_t num_bss = 0;
	qdf_list_t *candidate_list = NULL;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	QDF_STATUS status;
	struct scan_cache_node *scan_entry;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct qdf_mac_addr *bssid;
	bool found;

	pdev = wlan_vdev_get_pdev(cm_ctx->vdev);
	if (!pdev) {
		mlme_err(CM_PREFIX_FMT "Failed to find pdev",
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		return;
	}

	status = cm_connect_fetch_candidates(pdev, cm_ctx, cm_req,
					     &candidate_list, &num_bss);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug(CM_PREFIX_FMT "failed to fetch bss: %d",
			   CM_PREFIX_REF(vdev_id, cm_req->cm_id), num_bss);
		goto free_list;
	}

	if (qdf_list_peek_front(candidate_list, &cur_node) !=
					QDF_STATUS_SUCCESS) {
		mlme_err(CM_PREFIX_FMT"failed to peer front of candidate_list",
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		goto free_list;
	}

	while (cur_node) {
		qdf_list_peek_next(candidate_list, cur_node, &next_node);

		scan_entry = qdf_container_of(cur_node, struct scan_cache_node,
					      node);
		bssid = &scan_entry->entry->bssid;
		if (qdf_is_macaddr_zero(bssid) ||
		    qdf_is_macaddr_broadcast(bssid))
			goto next;
		found = cm_find_bss_from_candidate_list(cm_req->candidate_list,
							bssid, NULL);
		if (found)
			goto next;
		status = qdf_list_remove_node(candidate_list, cur_node);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err(CM_PREFIX_FMT"failed to remove node for BSS "QDF_MAC_ADDR_FMT" from candidate list",
				 CM_PREFIX_REF(vdev_id, cm_req->cm_id),
				 QDF_MAC_ADDR_REF(scan_entry->entry->bssid.bytes));
			goto free_list;
		}

		status = qdf_list_insert_after(cm_req->candidate_list,
					       &scan_entry->node,
					       &prev_candidate->node);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err(CM_PREFIX_FMT"failed to insert node for BSS "QDF_MAC_ADDR_FMT" from candidate list",
				 CM_PREFIX_REF(vdev_id, cm_req->cm_id),
				 QDF_MAC_ADDR_REF(scan_entry->entry->bssid.bytes));
			util_scan_free_cache_entry(scan_entry->entry);
			qdf_mem_free(scan_entry);
			goto free_list;
		}
		prev_candidate = scan_entry;
		mlme_debug(CM_PREFIX_FMT"insert new node BSS "QDF_MAC_ADDR_FMT" to existing candidate list",
			   CM_PREFIX_REF(vdev_id, cm_req->cm_id),
			   QDF_MAC_ADDR_REF(scan_entry->entry->bssid.bytes));
next:
		cur_node = next_node;
		next_node = NULL;
	}

free_list:
	if (candidate_list)
		wlan_scan_purge_results(candidate_list);
}
#else
static inline void
cm_update_candidate_list(struct cnx_mgr *cm_ctx,
			 struct cm_connect_req *cm_req,
			 struct scan_cache_node *prev_candidate)
{
}
#endif

QDF_STATUS cm_if_mgr_inform_connect_complete(struct wlan_objmgr_vdev *vdev,
					     QDF_STATUS connect_status)
{
	struct if_mgr_event_data *connect_complete;

	connect_complete = qdf_mem_malloc(sizeof(*connect_complete));
	if (!connect_complete)
		return QDF_STATUS_E_NOMEM;

	connect_complete->status = connect_status;
	if_mgr_deliver_event(vdev, WLAN_IF_MGR_EV_CONNECT_COMPLETE,
			     connect_complete);
	qdf_mem_free(connect_complete);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
cm_if_mgr_inform_connect_start(struct wlan_objmgr_vdev *vdev)
{
	return if_mgr_deliver_event(vdev, WLAN_IF_MGR_EV_CONNECT_START, NULL);
}

QDF_STATUS
cm_handle_connect_req_in_non_init_state(struct cnx_mgr *cm_ctx,
					struct cm_connect_req *cm_req,
					enum wlan_cm_sm_state cm_state_substate)
{
	if (cm_state_substate != WLAN_CM_S_CONNECTED &&
	    cm_is_connect_req_reassoc(&cm_req->req)) {
		cm_req->req.reassoc_in_non_connected = true;
		mlme_debug(CM_PREFIX_FMT "Reassoc received in %d state",
			   CM_PREFIX_REF(wlan_vdev_get_id(cm_ctx->vdev),
					 cm_req->cm_id),
			   cm_state_substate);
	}

	switch (cm_state_substate) {
	case WLAN_CM_S_ROAMING:
		/* for FW roam/LFR3 remove the req from the list */
		if (cm_roam_offload_enabled(wlan_vdev_get_psoc(cm_ctx->vdev)))
			cm_flush_pending_request(cm_ctx, ROAM_REQ_PREFIX,
						 false);
		fallthrough;
	case WLAN_CM_S_CONNECTED:
	case WLAN_CM_SS_JOIN_ACTIVE:
		/*
		 * In roaming state, there would be no
		 * pending command, so for new connect request, queue internal
		 * disconnect. The preauth and reassoc process will be aborted
		 * as the state machine will be moved to connecting state and
		 * preauth/reassoc/roam start event posting will fail.
		 *
		 * In connected state, there would be no pending command, so
		 * for new connect request, queue internal disconnect
		 *
		 * In join active state there would be only one active connect
		 * request in the cm req list, so to abort at certain stages and
		 * to cleanup after its completion, queue internal disconnect.
		 */
		cm_initiate_internal_disconnect(cm_ctx);
		break;
	case WLAN_CM_SS_SCAN:
		/* In the scan state abort the ongoing scan */
		cm_vdev_scan_cancel(wlan_vdev_get_pdev(cm_ctx->vdev),
				    cm_ctx->vdev);
		fallthrough;
	case WLAN_CM_SS_JOIN_PENDING:
		/*
		 * In case of scan or join pending there could be 2 scenarios:-
		 *
		 * 1. There is a connect request pending, so just remove
		 *    the pending connect req. As we will queue a new connect
		 *    req, all resp for pending connect req will be dropped.
		 * 2. There is a connect request in active and
		 *    and a internal disconnect followed by a connect req in
		 *    pending. In this case the disconnect will take care of
		 *    cleaning up the active connect request and thus only
		 *    remove the pending connect.
		 */
		cm_flush_pending_request(cm_ctx, CONNECT_REQ_PREFIX, false);
		break;
	case WLAN_CM_S_DISCONNECTING:
		/*
		 * Flush failed pending connect req as new req is received
		 * and its no longer the latest one.
		 */
		if (cm_ctx->connect_count)
			cm_flush_pending_request(cm_ctx, CONNECT_REQ_PREFIX,
						 true);
		/*
		 * In case of disconnecting state, there could be 2 scenarios:-
		 * In both case no state specific action is required.
		 * 1. There is disconnect request in the cm_req list, no action
		 *    required to cleanup.
		 *    so just add the connect request to the list.
		 * 2. There is a connect request activated, followed by
		 *    disconnect in pending queue. So keep the disconnect
		 *    to cleanup the active connect and no action required to
		 *    cleanup.
		 */
		break;
	default:
		mlme_err("Vdev %d Connect req in invalid state %d",
			 wlan_vdev_get_id(cm_ctx->vdev),
			 cm_state_substate);
		return QDF_STATUS_E_FAILURE;
	};

	/* Queue the new connect request after state specific actions */
	return cm_add_connect_req_to_list(cm_ctx, cm_req);
}

QDF_STATUS cm_connect_start(struct cnx_mgr *cm_ctx,
			    struct cm_connect_req *cm_req)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	enum wlan_cm_connect_fail_reason reason = CM_GENERIC_FAILURE;
	QDF_STATUS status;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);

	/* Interface event */
	pdev = wlan_vdev_get_pdev(cm_ctx->vdev);
	if (!pdev) {
		mlme_err(CM_PREFIX_FMT "Failed to find pdev",
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		goto connect_err;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		mlme_err(CM_PREFIX_FMT "Failed to find psoc",
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		goto connect_err;
	}

	/*
	 * Do not initiate the duplicate ifmanager and connect start ind if
	 * this is called from Scan for ssid
	 */
	if (!cm_req->scan_id) {
		cm_if_mgr_inform_connect_start(cm_ctx->vdev);
		status = mlme_cm_connect_start_ind(cm_ctx->vdev, &cm_req->req);
		if (QDF_IS_STATUS_ERROR(status)) {
			reason = CM_NO_CANDIDATE_FOUND;
			goto connect_err;
		}
	}

	status = cm_connect_get_candidates(pdev, cm_ctx, cm_req);

	/* In case of status pending connect will continue after scan */
	if (status == QDF_STATUS_E_PENDING)
		return QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_ERROR(status)) {
		reason = CM_NO_CANDIDATE_FOUND;
		goto connect_err;
	}

	status = cm_check_for_hw_mode_change(psoc, cm_req->candidate_list,
					     vdev_id, cm_req->cm_id);
	if (QDF_IS_STATUS_ERROR(status) && status != QDF_STATUS_E_ALREADY) {
		reason = CM_HW_MODE_FAILURE;
		mlme_err(CM_PREFIX_FMT "Failed to set HW mode change",
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		goto connect_err;
	} else if (QDF_IS_STATUS_SUCCESS(status)) {
		mlme_debug(CM_PREFIX_FMT "Connect will continue after HW mode change",
			   CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		return QDF_STATUS_SUCCESS;
	}

	status = cm_ser_connect_req(pdev, cm_ctx, cm_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		reason = CM_SER_FAILURE;
		goto connect_err;
	}

	return QDF_STATUS_SUCCESS;

connect_err:
	return cm_send_connect_start_fail(cm_ctx, cm_req, reason);
}

/**
 * cm_get_valid_candidate() - This API will be called to get the next valid
 * candidate
 * @cm_ctx: connection manager context
 * @cm_req: Connect request.
 * @resp: connect resp from previous connection attempt
 * @same_candidate_used: this will be set if same candidate used
 *
 * This function return a valid candidate to try connection. It return failure
 * if no valid candidate is present or all valid candidate are tried.
 *
 * Return: QDF status
 */
static QDF_STATUS cm_get_valid_candidate(struct cnx_mgr *cm_ctx,
					 struct cm_req *cm_req,
					 struct wlan_cm_connect_resp *resp,
					 bool *same_candidate_used)
{
	struct wlan_objmgr_psoc *psoc;
	struct scan_cache_node *scan_node = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct scan_cache_node *new_candidate = NULL, *prev_candidate;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	bool use_same_candidate = false;
	int32_t akm;
	struct qdf_mac_addr *pmksa_mac;

	psoc = wlan_vdev_get_psoc(cm_ctx->vdev);
	if (!psoc) {
		mlme_err(CM_PREFIX_FMT "Failed to find psoc",
			 CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		return QDF_STATUS_E_FAILURE;
	}

	prev_candidate = cm_req->connect_req.cur_candidate;
	/*
	 * In case of STA/CLI + STA/CLI, if a STA/CLI is in connecting state and
	 * a disconnect is received on any other STA/CLI, the disconnect can
	 * timeout waiting for the connection on first STA/CLI to get completed.
	 * This is because the connect is a blocking serialization command and
	 * it can try multiple candidates and thus can take upto 30+ sec to
	 * complete.
	 *
	 * Now osif will proceed with vdev delete after disconnect timeout.
	 * This can lead to vdev delete sent without vdev down/stop/peer delete
	 * for the vdev.
	 *
	 * Same way if a SAP/GO has start/stop command or peer disconnect in
	 * pending queue, delay in processing it can cause timeouts and other
	 * issues.
	 *
	 * So abort the next connection attempt if any of the vdev is waiting
	 * for vdev operation to avoid timeouts
	 */
	if (cm_is_any_other_vdev_connecting_disconnecting(cm_ctx, cm_req)) {
		status = QDF_STATUS_E_FAILURE;
		goto flush_single_pmk;
	}

	if (cm_req->connect_req.connect_attempts >=
	    cm_ctx->max_connect_attempts) {
		mlme_info(CM_PREFIX_FMT "%d attempts tried, max %d",
			  CM_PREFIX_REF(vdev_id, cm_req->cm_id),
			  cm_req->connect_req.connect_attempts,
			  cm_ctx->max_connect_attempts);
		status = QDF_STATUS_E_FAILURE;
		goto flush_single_pmk;
	}

	/* From 2nd attempt onward, check if time allows for a new attempt */
	if (cm_req->connect_req.connect_attempts &&
	    !cm_is_time_allowed_for_connect_attempt(cm_ctx,
						    &cm_req->connect_req)) {
		status = QDF_STATUS_E_FAILURE;
		goto flush_single_pmk;
	}

	if (prev_candidate && resp &&
	    cm_is_retry_with_same_candidate(cm_ctx, &cm_req->connect_req,
					    resp)) {
		new_candidate = prev_candidate;
		use_same_candidate = true;
		goto try_same_candidate;
	}

	/*
	 * Get next candidate if prev_candidate is not NULL, else get
	 * the first candidate
	 */
	if (prev_candidate) {
		/* Fetch new candidate list and append new entries to the
		 * current candidate list.
		 */
		cm_update_candidate_list(cm_ctx, &cm_req->connect_req,
					 prev_candidate);
		qdf_list_peek_next(cm_req->connect_req.candidate_list,
				   &prev_candidate->node, &cur_node);
	} else {
		qdf_list_peek_front(cm_req->connect_req.candidate_list,
				    &cur_node);
	}

	while (cur_node) {
		qdf_list_peek_next(cm_req->connect_req.candidate_list,
				   cur_node, &next_node);
		scan_node = qdf_container_of(cur_node, struct scan_cache_node,
					     node);
		status = cm_if_mgr_validate_candidate(cm_ctx, scan_node->entry);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			new_candidate = scan_node;
			break;
		}

		/*
		 * stored failure response for first candidate only but
		 * indicate the failure response to osif for all candidates.
		 */
		cm_store_n_send_failed_candidate(cm_ctx, cm_req->cm_id);

		cur_node = next_node;
		next_node = NULL;
	}

	/*
	 * If cur_node is NULL prev candidate was last to be tried so no more
	 * candidates left for connect now.
	 */
	if (!cur_node) {
		mlme_debug(CM_PREFIX_FMT "No more candidates left",
			   CM_PREFIX_REF(vdev_id, cm_req->cm_id));
		cm_req->connect_req.cur_candidate = NULL;
		status = QDF_STATUS_E_FAILURE;
		goto flush_single_pmk;
	}

	/* Reset current candidate retries when a new candidate is tried */
	cm_req->connect_req.cur_candidate_retries = 0;

try_same_candidate:
	cm_req->connect_req.connect_attempts++;
	cm_req->connect_req.cur_candidate = new_candidate;

flush_single_pmk:
	akm = wlan_crypto_get_param(cm_ctx->vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	/*
	 * If connection fails with Single PMK bssid (prev candidate),
	 * clear the pmk entry. Flush only in case if we are not trying again
	 * with same candidate again.
	 */
	if (prev_candidate && !use_same_candidate &&
	    util_scan_entry_single_pmk(psoc, prev_candidate->entry) &&
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_SAE)) {
		pmksa_mac = &prev_candidate->entry->bssid;
		cm_delete_pmksa_for_single_pmk_bssid(cm_ctx, pmksa_mac);

		/* If the candidate is ML capable, the PMKSA entry might
		 * exist with it's MLD address, so check and purge the
		 * PMKSA entry with MLD address for ML candidate.
		 */
		pmksa_mac = (struct qdf_mac_addr *)
				util_scan_entry_mldaddr(prev_candidate->entry);
		if (pmksa_mac)
			cm_delete_pmksa_for_single_pmk_bssid(cm_ctx, pmksa_mac);
	}

	if (same_candidate_used)
		*same_candidate_used = use_same_candidate;

	return status;
}

static QDF_STATUS
cm_send_bss_select_ind(struct cnx_mgr *cm_ctx, struct cm_connect_req *req)
{
	QDF_STATUS status;
	struct wlan_cm_vdev_connect_req vdev_req;
	struct wlan_cm_connect_resp *resp;

	vdev_req.vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	vdev_req.cm_id = req->cm_id;
	vdev_req.bss = req->cur_candidate;

	status = mlme_cm_bss_select_ind(cm_ctx->vdev, &vdev_req);
	if (QDF_IS_STATUS_SUCCESS(status) ||
	    status == QDF_STATUS_E_NOSUPPORT)
		return status;

	/* In supported and failure try with next candidate */
	mlme_err(CM_PREFIX_FMT "mlme candidate select indication failed %d",
		 CM_PREFIX_REF(vdev_req.vdev_id, req->cm_id), status);
	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp)
		return QDF_STATUS_E_FAILURE;

	cm_fill_failure_resp_from_cm_id(cm_ctx, resp, req->cm_id,
					CM_BSS_SELECT_IND_FAILED);
	cm_sm_deliver_event_sync(cm_ctx,
				 WLAN_CM_SM_EV_CONNECT_GET_NEXT_CANDIDATE,
				 sizeof(*resp), resp);
	qdf_mem_free(resp);

	return QDF_STATUS_SUCCESS;
}

static void cm_update_ser_timer_for_new_candidate(struct cnx_mgr *cm_ctx,
						  wlan_cm_id cm_id)
{
	struct wlan_serialization_command cmd;

	cmd.cmd_type = WLAN_SER_CMD_VDEV_CONNECT;
	cmd.cmd_id = cm_id;
	cmd.cmd_timeout_duration = cm_ctx->connect_timeout;
	cmd.vdev = cm_ctx->vdev;

	wlan_serialization_update_timer(&cmd);
}

QDF_STATUS cm_try_next_candidate(struct cnx_mgr *cm_ctx,
				 struct wlan_cm_connect_resp *resp)
{
	QDF_STATUS status;
	struct cm_req *cm_req;
	bool same_candidate_used = false;

	cm_req = cm_get_req_by_cm_id(cm_ctx, resp->cm_id);
	if (!cm_req)
		return QDF_STATUS_E_FAILURE;

	status = cm_get_valid_candidate(cm_ctx, cm_req, resp,
					&same_candidate_used);
	if (QDF_IS_STATUS_ERROR(status))
		goto connect_err;

	/*
	 * cached the first failure response if candidate is different from
	 * previous.
	 * Do not indicate to OSIF if same candidate is used again as we are not
	 * done with this candidate. So inform once we move to next candidate.
	 * This will also avoid flush for the scan entry.
	 */
	if (!same_candidate_used) {
		cm_store_first_candidate_rsp(cm_ctx, resp->cm_id, resp);
		mlme_cm_osif_failed_candidate_ind(cm_ctx->vdev, resp);
	}

	cm_update_ser_timer_for_new_candidate(cm_ctx, resp->cm_id);

	status = cm_send_bss_select_ind(cm_ctx, &cm_req->connect_req);

	/*
	 * If candidate select indication is not supported continue with bss
	 * peer create, else peer will be created after resp.
	 */
	if (status == QDF_STATUS_E_NOSUPPORT) {
		/* Update vdev mlme mac address based on connection type */
		cm_update_vdev_mlme_macaddr(cm_ctx, &cm_req->connect_req);

		cm_create_bss_peer(cm_ctx, &cm_req->connect_req);
	} else if (QDF_IS_STATUS_ERROR(status)) {
		goto connect_err;
	}

	return QDF_STATUS_SUCCESS;

connect_err:
	return cm_sm_deliver_event_sync(cm_ctx, WLAN_CM_SM_EV_CONNECT_FAILURE,
					sizeof(*resp), resp);

}

bool cm_connect_resp_cmid_match_list_head(struct cnx_mgr *cm_ctx,
					  struct wlan_cm_connect_resp *resp)
{
	return cm_check_cmid_match_list_head(cm_ctx, &resp->cm_id);
}

void cm_fill_vdev_crypto_params(struct cnx_mgr *cm_ctx,
				struct wlan_cm_connect_req *req)
{
	/* fill vdev crypto from the connect req */
	wlan_crypto_set_vdev_param(cm_ctx->vdev, WLAN_CRYPTO_PARAM_AUTH_MODE,
				   req->crypto.auth_type);
	wlan_crypto_set_vdev_param(cm_ctx->vdev, WLAN_CRYPTO_PARAM_KEY_MGMT,
				   req->crypto.akm_suites);
	wlan_crypto_set_vdev_param(cm_ctx->vdev, WLAN_CRYPTO_PARAM_UCAST_CIPHER,
				   req->crypto.ciphers_pairwise);
	wlan_crypto_set_vdev_param(cm_ctx->vdev, WLAN_CRYPTO_PARAM_MCAST_CIPHER,
				   req->crypto.group_cipher);
	wlan_crypto_set_vdev_param(cm_ctx->vdev, WLAN_CRYPTO_PARAM_MGMT_CIPHER,
				   req->crypto.mgmt_ciphers);
	wlan_crypto_set_vdev_param(cm_ctx->vdev, WLAN_CRYPTO_PARAM_RSN_CAP,
				   req->crypto.rsn_caps);
}

static QDF_STATUS
cm_if_mgr_inform_connect_active(struct wlan_objmgr_vdev *vdev)
{
	return if_mgr_deliver_event(vdev, WLAN_IF_MGR_EV_CONNECT_ACTIVE, NULL);
}

QDF_STATUS cm_connect_active(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id)
{
	struct cm_req *cm_req;
	QDF_STATUS status;
	struct wlan_cm_connect_req *req;

	cm_if_mgr_inform_connect_active(cm_ctx->vdev);

	cm_ctx->active_cm_id = *cm_id;
	cm_req = cm_get_req_by_cm_id(cm_ctx, *cm_id);
	if (!cm_req) {
		/*
		 * Remove the command from serialization active queue, if
		 * connect req was not found, to avoid active cmd timeout.
		 * This can happen if a thread tried to flush the pending
		 * connect request and while doing so, it removed the
		 * CM pending request, but before it tried to remove pending
		 * command from serialization, the command becomes active in
		 * another thread.
		 */
		cm_remove_cmd_from_serialization(cm_ctx, *cm_id);
		return QDF_STATUS_E_INVAL;
	}

	cm_req->connect_req.connect_active_time =
				qdf_mc_timer_get_system_time();
	req = &cm_req->connect_req.req;
	wlan_vdev_mlme_set_ssid(cm_ctx->vdev, req->ssid.ssid, req->ssid.length);
	/*
	 * free vdev keys before setting crypto params for 1x/ owe roaming,
	 * link vdev keys would be cleaned in osif
	 */
	if (!wlan_vdev_mlme_is_mlo_link_vdev(cm_ctx->vdev) &&
	    !wlan_cm_check_mlo_roam_auth_status(cm_ctx->vdev))
		wlan_crypto_free_vdev_key(cm_ctx->vdev);
	cm_fill_vdev_crypto_params(cm_ctx, req);
	cm_store_wep_key(cm_ctx, &req->crypto, *cm_id);

	status = cm_get_valid_candidate(cm_ctx, cm_req, NULL, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		goto connect_err;

	status = cm_send_bss_select_ind(cm_ctx, &cm_req->connect_req);
	/*
	 * If candidate select indication is not supported continue with bss
	 * peer create, else peer will be created after resp.
	 */
	if (status == QDF_STATUS_E_NOSUPPORT) {
		/* Update vdev mlme mac address based on connection type */
		cm_update_vdev_mlme_macaddr(cm_ctx, &cm_req->connect_req);

		cm_create_bss_peer(cm_ctx, &cm_req->connect_req);
	} else if (QDF_IS_STATUS_ERROR(status)) {
		goto connect_err;
	}

	return QDF_STATUS_SUCCESS;

connect_err:
	return cm_send_connect_start_fail(cm_ctx,
					  &cm_req->connect_req, CM_JOIN_FAILED);
}

#ifdef WLAN_FEATURE_FILS_SK
static void cm_copy_fils_info(struct wlan_cm_vdev_connect_req *req,
			      struct cm_req *cm_req)
{
	req->fils_info = &cm_req->connect_req.req.fils_info;
}

static inline void cm_set_fils_connection(struct cnx_mgr *cm_ctx,
					  struct wlan_cm_connect_resp *resp)
{
	int32_t key_mgmt;

	/*
	 * Check and set only in case of failure and when
	 * resp->is_fils_connection is not already set, else return.
	 */
	if (QDF_IS_STATUS_SUCCESS(resp->connect_status) ||
	    resp->is_fils_connection)
		return;

	key_mgmt = wlan_crypto_get_param(cm_ctx->vdev,
					 WLAN_CRYPTO_PARAM_KEY_MGMT);

	if (key_mgmt & (1 << WLAN_CRYPTO_KEY_MGMT_FILS_SHA256 |
			  1 << WLAN_CRYPTO_KEY_MGMT_FILS_SHA384 |
			  1 << WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256 |
			  1 << WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384))
		resp->is_fils_connection = true;
}
#else
static inline void cm_copy_fils_info(struct wlan_cm_vdev_connect_req *req,
				     struct cm_req *cm_req)
{
}

static inline void cm_set_fils_connection(struct cnx_mgr *cm_ctx,
					  struct wlan_cm_connect_resp *resp)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
static inline
void cm_update_ml_partner_info(struct wlan_cm_connect_req *req,
			       struct wlan_cm_vdev_connect_req *connect_req)
{
	if (req->ml_parnter_info.num_partner_links)
		qdf_mem_copy(&connect_req->ml_parnter_info,
			     &req->ml_parnter_info,
			     sizeof(struct mlo_partner_info));
}
#else
static inline
void cm_update_ml_partner_info(struct wlan_cm_connect_req *req,
			       struct wlan_cm_vdev_connect_req *connect_req)
{
}
#endif

static
void cm_update_per_peer_key_mgmt_crypto_params(struct wlan_objmgr_vdev *vdev,
					struct security_info *neg_sec_info)
{
	int32_t key_mgmt = 0;
	int32_t neg_akm = neg_sec_info->key_mgmt;

	/*
	 * As there can be multiple AKM present select the most secured AKM
	 * present
	 */
	if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FILS_SHA384))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FILS_SHA384);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FILS_SHA256))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FILS_SHA256);
	else if (QDF_HAS_PARAM(neg_akm,
			       WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X_SHA384))
		QDF_SET_PARAM(key_mgmt,
			      WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X_SHA384);
	else if (QDF_HAS_PARAM(neg_akm,
			       WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192))
		QDF_SET_PARAM(key_mgmt,
			      WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FT_SAE_EXT_KEY))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_SAE_EXT_KEY);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FT_SAE))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_SAE);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_SAE_EXT_KEY))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_SAE_EXT_KEY);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_SAE))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_SAE);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_OWE))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_OWE);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_DPP))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_DPP);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_IEEE8021X);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FT_PSK_SHA384))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_PSK_SHA384);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_PSK_SHA384))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_PSK_SHA384);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_PSK_SHA256))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_PSK_SHA256);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_FT_PSK))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_PSK);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_PSK))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_PSK);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_WAPI_PSK))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_WAPI_PSK);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_WAPI_CERT))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_WAPI_CERT);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_CCKM))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_CCKM);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_OSEN))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_OSEN);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_WPS))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_WPS);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_NO_WPA))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_NO_WPA);
	else if (QDF_HAS_PARAM(neg_akm, WLAN_CRYPTO_KEY_MGMT_WPA_NONE))
		QDF_SET_PARAM(key_mgmt, WLAN_CRYPTO_KEY_MGMT_WPA_NONE);
	else /* use original if no akm match */
		key_mgmt = neg_akm;

	wlan_crypto_set_vdev_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT, key_mgmt);
	/*
	 * Overwrite the key mgmt with single key_mgmt if multiple are present
	 */
	neg_sec_info->key_mgmt = key_mgmt;
}

static
void cm_update_per_peer_ucastcipher_crypto_params(struct wlan_objmgr_vdev *vdev,
					struct security_info *neg_sec_info)
{
	int32_t ucastcipherset = 0;

	/*
	 * As there can be multiple ucastcipher present select the most secured
	 * ucastcipher present.
	 */
	if (QDF_HAS_PARAM(neg_sec_info->ucastcipherset,
			  WLAN_CRYPTO_CIPHER_AES_GCM_256))
		QDF_SET_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_AES_GCM_256);
	else if (QDF_HAS_PARAM(neg_sec_info->ucastcipherset,
			       WLAN_CRYPTO_CIPHER_AES_CCM_256))
		QDF_SET_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_AES_CCM_256);
	else if (QDF_HAS_PARAM(neg_sec_info->ucastcipherset,
			       WLAN_CRYPTO_CIPHER_AES_GCM))
		QDF_SET_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_AES_GCM);
	else if (QDF_HAS_PARAM(neg_sec_info->ucastcipherset,
			       WLAN_CRYPTO_CIPHER_AES_CCM))
		QDF_SET_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_AES_CCM);
	else if (QDF_HAS_PARAM(neg_sec_info->ucastcipherset,
			       WLAN_CRYPTO_CIPHER_TKIP))
		QDF_SET_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_TKIP);
	else
		ucastcipherset = neg_sec_info->ucastcipherset;

	wlan_crypto_set_vdev_param(vdev, WLAN_CRYPTO_PARAM_UCAST_CIPHER,
				   ucastcipherset);
	/*
	 * Overwrite the ucastcipher with single ucast cipher if multiple are
	 * present
	 */
	neg_sec_info->ucastcipherset = ucastcipherset;
}

static
void cm_update_per_peer_crypto_params(struct wlan_objmgr_vdev *vdev,
				      struct cm_connect_req *connect_req)
{
	struct security_info *neg_sec_info;
	uint16_t rsn_caps;

	/* Do only for WPA/WPA2/WPA3 */
	if (!connect_req->req.crypto.wpa_versions)
		return;

	/*
	 * Some non PMF AP misbehave if in assoc req RSN IE contain PMF capable
	 * bit set. Thus only if AP and self are capable, try PMF connection
	 * else set PMF as 0. The PMF filtering is already taken care in
	 * get scan results.
	 */
	neg_sec_info = &connect_req->cur_candidate->entry->neg_sec_info;
	rsn_caps = connect_req->req.crypto.rsn_caps;
	if (!(neg_sec_info->rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED &&
	     rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED)) {
		rsn_caps &= ~WLAN_CRYPTO_RSN_CAP_MFP_ENABLED;
		rsn_caps &= ~WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED;
		rsn_caps &= ~WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED;
	}

	/* Update the new rsn caps */
	wlan_crypto_set_vdev_param(vdev, WLAN_CRYPTO_PARAM_RSN_CAP,
				   rsn_caps);

	cm_update_per_peer_key_mgmt_crypto_params(vdev, neg_sec_info);
	cm_update_per_peer_ucastcipher_crypto_params(vdev, neg_sec_info);
}

QDF_STATUS
cm_resume_connect_after_peer_create(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id)
{
	struct wlan_cm_vdev_connect_req req;
	struct cm_req *cm_req;
	QDF_STATUS status;
	struct security_info *neg_sec_info;
	uint8_t country_code[REG_ALPHA2_LEN + 1] = {0};
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(wlan_vdev_get_pdev(cm_ctx->vdev));

	cm_req = cm_get_req_by_cm_id(cm_ctx, *cm_id);
	if (!cm_req)
		return QDF_STATUS_E_FAILURE;

	/*
	 * As keymgmt and ucast cipher can be multiple.
	 * Choose one keymgmt and one ucastcipherset based on higher security.
	 */
	cm_update_per_peer_crypto_params(cm_ctx->vdev, &cm_req->connect_req);

	req.vdev_id = wlan_vdev_get_id(cm_ctx->vdev);
	req.cm_id = *cm_id;
	req.force_rsne_override = cm_req->connect_req.req.force_rsne_override;
	req.is_wps_connection = cm_req->connect_req.req.is_wps_connection;
	req.is_osen_connection = cm_req->connect_req.req.is_osen_connection;
	req.assoc_ie = cm_req->connect_req.req.assoc_ie;
	req.scan_ie = cm_req->connect_req.req.scan_ie;
	req.bss = cm_req->connect_req.cur_candidate;
	cm_copy_fils_info(&req, cm_req);
	req.ht_caps = cm_req->connect_req.req.ht_caps;
	req.ht_caps_mask = cm_req->connect_req.req.ht_caps_mask;
	req.vht_caps = cm_req->connect_req.req.vht_caps;
	req.vht_caps_mask = cm_req->connect_req.req.vht_caps_mask;
	req.is_non_assoc_link = cm_req->connect_req.req.is_non_assoc_link;
	cm_update_ml_partner_info(&cm_req->connect_req.req, &req);

	neg_sec_info = &cm_req->connect_req.cur_candidate->entry->neg_sec_info;
	if (util_scan_entry_is_hidden_ap(req.bss->entry) &&
	    QDF_HAS_PARAM(neg_sec_info->key_mgmt, WLAN_CRYPTO_KEY_MGMT_OWE)) {
		mlme_debug("OWE transition candidate has wildcard ssid");
		req.owe_trans_ssid = cm_req->connect_req.req.ssid;
	}

	wlan_reg_get_cc_and_src(psoc, country_code);
	mlme_nofl_info(CM_PREFIX_FMT "Connecting to " QDF_SSID_FMT " " QDF_MAC_ADDR_FMT " rssi: %d freq: %d akm 0x%x cipher: uc 0x%x mc 0x%x, wps %d osen %d force RSN %d CC: %c%c",
		       CM_PREFIX_REF(req.vdev_id, req.cm_id),
		       QDF_SSID_REF(cm_req->connect_req.req.ssid.length,
				    cm_req->connect_req.req.ssid.ssid),
		       QDF_MAC_ADDR_REF(req.bss->entry->bssid.bytes),
		       req.bss->entry->rssi_raw,
		       req.bss->entry->channel.chan_freq,
		       neg_sec_info->key_mgmt, neg_sec_info->ucastcipherset,
		       neg_sec_info->mcastcipherset, req.is_wps_connection,
		       req.is_osen_connection, req.force_rsne_override,
		       country_code[0],
		       country_code[1]);

	status = mlme_cm_connect_req(cm_ctx->vdev, &req);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err(CM_PREFIX_FMT "connect request failed",
			 CM_PREFIX_REF(req.vdev_id, req.cm_id));
		/* try delete bss peer if req fails */
		mlme_cm_bss_peer_delete_req(cm_ctx->vdev);
		status = cm_send_connect_start_fail(cm_ctx,
						    &cm_req->connect_req,
						    CM_JOIN_FAILED);
	}

	return status;
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_11BE_MLO_ADV_FEATURE)
static void cm_inform_bcn_probe_handler(struct cnx_mgr *cm_ctx,
					struct scan_cache_entry *bss,
					wlan_cm_id cm_id)
{
	struct element_info *bcn_probe_rsp;
	int32_t rssi;
	qdf_freq_t freq;

	bcn_probe_rsp = &bss->raw_frame;
	rssi = bss->rssi_raw;
	freq = util_scan_entry_channel_frequency(bss);

	cm_inform_bcn_probe(cm_ctx, bcn_probe_rsp->ptr, bcn_probe_rsp->len,
			    freq, rssi, cm_id);
}

static void cm_update_partner_link_scan_db(struct cnx_mgr *cm_ctx,
					   wlan_cm_id cm_id,
					   qdf_list_t *candidate_list,
					   struct scan_cache_entry *cur_bss)
{
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct scan_cache_node *candidate;
	struct scan_cache_entry *bss;

	qdf_list_peek_front(candidate_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(candidate_list, cur_node, &next_node);
		candidate = qdf_container_of(cur_node, struct scan_cache_node,
					     node);
		bss = candidate->entry;
		/*
		 * If BSS is ML and not current bss and BSS mld mac is same as
		 * cur bss then inform it to scan cache to avoid scan cache
		 * ageing out.
		 */
		if (!qdf_is_macaddr_equal(&bss->bssid, &cur_bss->bssid) &&
		    bss->ml_info.num_links &&
		    cur_bss->ml_info.num_links &&
		    qdf_is_macaddr_equal(&bss->ml_info.mld_mac_addr,
					 &cur_bss->ml_info.mld_mac_addr)) {
			mlme_debug("Inform Partner bssid: " QDF_MAC_ADDR_FMT " to kernel",
					QDF_MAC_ADDR_REF(bss->bssid.bytes));
			cm_inform_bcn_probe_handler(cm_ctx, bss, cm_id);
		}
		cur_node = next_node;
		next_node = NULL;
	}
}
#else
static
inline void cm_update_partner_link_scan_db(struct cnx_mgr *cm_ctx,
					   wlan_cm_id cm_id,
					   qdf_list_t *candidate_list,
					   struct scan_cache_entry *cur_bss)
{
}
#endif

/**
 * cm_update_scan_db_on_connect_success() - update scan db with beacon or
 * probe resp
 * @cm_ctx: connection manager context
 * @resp: connect resp
 *
 * update scan db, so that kernel and driver do not age out
 * the connected AP entry.
 *
 * Context: Can be called from any context and to be used only if connect
 * is successful and SM is in connected state. i.e. SM lock is hold.
 *
 * Return: void
 */
static void
cm_update_scan_db_on_connect_success(struct cnx_mgr *cm_ctx,
				     struct wlan_cm_connect_resp *resp)
{
	struct element_info *bcn_probe_rsp;
	struct cm_req *cm_req;
	int32_t rssi;
	struct scan_cache_node *cur_candidate;

	if (!cm_is_vdev_connected(cm_ctx->vdev))
		return;

	cm_req = cm_get_req_by_cm_id(cm_ctx, resp->cm_id);
	if (!cm_req)
		return;
	/* if reassoc get from roam req else from connect req */
	if (resp->is_reassoc)
		cur_candidate = cm_req->roam_req.cur_candidate;
	else
		cur_candidate = cm_req->connect_req.cur_candidate;

	if (!cur_candidate)
		return;

	/*
	 * Get beacon or probe resp from connect response, and if not present
	 * use cur candidate to get beacon or probe resp
	 */
	if (resp->connect_ies.bcn_probe_rsp.ptr)
		bcn_probe_rsp = &resp->connect_ies.bcn_probe_rsp;
	else
		bcn_probe_rsp = &cur_candidate->entry->raw_frame;

	rssi = cur_candidate->entry->rssi_raw;

	cm_inform_bcn_probe(cm_ctx, bcn_probe_rsp->ptr, bcn_probe_rsp->len,
			    resp->freq, rssi, resp->cm_id);

	/*
	 * If vdev is an MLO vdev and not reassoc then use partner link info to
	 * inform partner link scan entry to kernel.
	 */
	if (!resp->is_reassoc && wlan_vdev_mlme_is_mlo_vdev(cm_ctx->vdev))
		cm_update_partner_link_scan_db(cm_ctx, resp->cm_id,
				cm_req->connect_req.candidate_list,
				cur_candidate->entry);
}

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
static inline void
cm_clear_vdev_mlo_cap(struct wlan_objmgr_vdev *vdev)
{
	wlan_vdev_mlme_clear_mlo_vdev(vdev);
}
#else /*WLAN_FEATURE_11BE_MLO_ADV_FEATURE*/
static inline void
cm_clear_vdev_mlo_cap(struct wlan_objmgr_vdev *vdev)
{
	/* If the connect req fails on assoc link, reset
	 * the MLO cap flags. The flags will be updated based
	 * on next connect req
	 */
	if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		ucfg_mlo_mld_clear_mlo_cap(vdev);
}
#endif /*WLAN_FEATURE_11BE_MLO_ADV_FEATURE*/
#else /*WLAN_FEATURE_11BE_MLO*/
static inline void
cm_clear_vdev_mlo_cap(struct wlan_objmgr_vdev *vdev)
{ }
#endif /*WLAN_FEATURE_11BE_MLO*/

/**
 * cm_is_connect_id_reassoc_in_non_connected()
 * @cm_ctx: connection manager context
 * @cm_id: cm id
 *
 * If connect req is a reassoc req and received in not connected state.
 * Caller should take cm_ctx lock.
 *
 * Return: bool
 */
static bool cm_is_connect_id_reassoc_in_non_connected(struct cnx_mgr *cm_ctx,
						      wlan_cm_id cm_id)
{
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct cm_req *cm_req;
	uint32_t prefix = CM_ID_GET_PREFIX(cm_id);
	bool is_reassoc = false;

	if (prefix != CONNECT_REQ_PREFIX)
		return is_reassoc;

	qdf_list_peek_front(&cm_ctx->req_list, &cur_node);
	while (cur_node) {
		qdf_list_peek_next(&cm_ctx->req_list, cur_node, &next_node);
		cm_req = qdf_container_of(cur_node, struct cm_req, node);

		if (cm_req->cm_id == cm_id) {
			if (cm_req->connect_req.req.reassoc_in_non_connected)
				is_reassoc = true;
			return is_reassoc;
		}

		cur_node = next_node;
		next_node = NULL;
	}

	return is_reassoc;
}

#ifdef CONN_MGR_ADV_FEATURE
/**
 * cm_osif_connect_complete() - This API will send the response to osif layer
 * @cm_ctx: connection manager context
 * @resp: connect resp sent to osif
 *
 * This function fetches the first response in case of connect failure and sent
 * it to the osif layer, otherwise, sent the provided response to osif.
 *
 * Return:void
 */
static void cm_osif_connect_complete(struct cnx_mgr *cm_ctx,
				     struct wlan_cm_connect_resp *resp)
{
	struct wlan_cm_connect_resp first_failure_resp = {0};
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_cm_connect_resp *connect_rsp = resp;

	if (QDF_IS_STATUS_ERROR(resp->connect_status)) {
		status = cm_get_first_candidate_rsp(cm_ctx, resp->cm_id,
						    &first_failure_resp);
		if (QDF_IS_STATUS_SUCCESS(status))
			connect_rsp = &first_failure_resp;
	}

	mlme_cm_osif_connect_complete(cm_ctx->vdev, connect_rsp);

	if (QDF_IS_STATUS_SUCCESS(status))
		cm_free_connect_rsp_ies(connect_rsp);
}
#else
static void cm_osif_connect_complete(struct cnx_mgr *cm_ctx,
				     struct wlan_cm_connect_resp *resp)
{
	mlme_cm_osif_connect_complete(cm_ctx->vdev, resp);
}
#endif

QDF_STATUS cm_notify_connect_complete(struct cnx_mgr *cm_ctx,
				      struct wlan_cm_connect_resp *resp,
				      bool acquire_lock)
{
	enum wlan_cm_sm_state sm_state;

	sm_state = cm_get_state(cm_ctx);

	mlme_cm_connect_complete_ind(cm_ctx->vdev, resp);
	mlo_sta_link_connect_notify(cm_ctx->vdev, resp);
	/*
	 * If connect req was a reassoc req and was received in not connected
	 * state send disconnect instead of connect resp to kernel to cleanup
	 * kernel flags
	 */
	if (QDF_IS_STATUS_ERROR(resp->connect_status) &&
	    sm_state == WLAN_CM_S_INIT) {
		if (acquire_lock)
			cm_req_lock_acquire(cm_ctx);
		if (cm_is_connect_id_reassoc_in_non_connected(cm_ctx,
							      resp->cm_id)) {
			resp->send_disconnect = true;
			mlme_debug(CM_PREFIX_FMT "Set send disconnect to true to indicate disconnect instead of connect resp",
				   CM_PREFIX_REF(wlan_vdev_get_id(cm_ctx->vdev),
						 resp->cm_id));
		}
		if (acquire_lock)
			cm_req_lock_release(cm_ctx);
	}
	cm_osif_connect_complete(cm_ctx, resp);
	cm_if_mgr_inform_connect_complete(cm_ctx->vdev,
					  resp->connect_status);
	cm_inform_dlm_connect_complete(cm_ctx->vdev, resp);
	if (QDF_IS_STATUS_ERROR(resp->connect_status) &&
	    sm_state == WLAN_CM_S_INIT)
		cm_clear_vdev_mlo_cap(cm_ctx->vdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cm_connect_complete(struct cnx_mgr *cm_ctx,
			       struct wlan_cm_connect_resp *resp)
{
	enum wlan_cm_sm_state sm_state;
	struct bss_info bss_info;
	struct mlme_info mlme_info;
	bool send_ind = true;

	/*
	 * If the entry is not present in the list, it must have been cleared
	 * already.
	 */
	if (!cm_get_req_by_cm_id(cm_ctx, resp->cm_id))
		return QDF_STATUS_SUCCESS;

	sm_state = cm_get_state(cm_ctx);
	cm_set_fils_connection(cm_ctx, resp);
	if (QDF_IS_STATUS_SUCCESS(resp->connect_status) &&
	    sm_state == WLAN_CM_S_CONNECTED) {
		cm_update_scan_db_on_connect_success(cm_ctx, resp);
		/* set WEP and FILS key on success */
		cm_set_fils_wep_key(cm_ctx, resp);
	}

	/* In case of reassoc failure no need to inform osif/legacy/ifmanager */
	if (resp->is_reassoc && QDF_IS_STATUS_ERROR(resp->connect_status))
		send_ind = false;

	if (send_ind)
		cm_notify_connect_complete(cm_ctx, resp, 1);

	/* Update scan entry in case connect is success or fails with bssid */
	if (!qdf_is_macaddr_zero(&resp->bssid)) {
		if (QDF_IS_STATUS_SUCCESS(resp->connect_status))
			mlme_info.assoc_state  = SCAN_ENTRY_CON_STATE_ASSOC;
		else
			mlme_info.assoc_state = SCAN_ENTRY_CON_STATE_NONE;
		qdf_copy_macaddr(&bss_info.bssid, &resp->bssid);
		bss_info.freq = resp->freq;
		bss_info.ssid.length = resp->ssid.length;
		qdf_mem_copy(&bss_info.ssid.ssid, resp->ssid.ssid,
			     bss_info.ssid.length);
		wlan_scan_update_mlme_by_bssinfo(
					wlan_vdev_get_pdev(cm_ctx->vdev),
					&bss_info, &mlme_info);
	}

	mlme_debug(CM_PREFIX_FMT,
		   CM_PREFIX_REF(wlan_vdev_get_id(cm_ctx->vdev),
				 resp->cm_id));
	cm_remove_cmd(cm_ctx, &resp->cm_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cm_add_connect_req_to_list(struct cnx_mgr *cm_ctx,
				      struct cm_connect_req *req)
{
	QDF_STATUS status;
	struct cm_req *cm_req;

	cm_req = qdf_container_of(req, struct cm_req, connect_req);
	req->cm_id = cm_get_cm_id(cm_ctx, req->req.source);
	cm_req->cm_id = req->cm_id;
	status = cm_add_req_to_list_and_indicate_osif(cm_ctx, cm_req,
						      req->req.source);

	return status;
}

QDF_STATUS cm_connect_rsp(struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *resp)
{
	struct cnx_mgr *cm_ctx;
	QDF_STATUS qdf_status;
	wlan_cm_id cm_id;
	uint32_t prefix;
	struct qdf_mac_addr pmksa_mac = QDF_MAC_ADDR_ZERO_INIT;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	cm_id = cm_ctx->active_cm_id;
	prefix = CM_ID_GET_PREFIX(cm_id);

	if (prefix != CONNECT_REQ_PREFIX || cm_id != resp->cm_id) {
		mlme_err(CM_PREFIX_FMT " Active cm_id 0x%x is different",
			 CM_PREFIX_REF(wlan_vdev_get_id(vdev), resp->cm_id),
			 cm_id);
		qdf_status = QDF_STATUS_E_FAILURE;
		goto post_err;
	}

	cm_connect_rsp_get_mld_addr_or_bssid(resp, &pmksa_mac);

	if (QDF_IS_STATUS_SUCCESS(resp->connect_status)) {
		/*
		 * On successful connection to sae single pmk AP,
		 * clear all the single pmk AP.
		 */
		if (cm_is_cm_id_current_candidate_single_pmk(cm_ctx, cm_id))
			wlan_crypto_selective_clear_sae_single_pmk_entries(vdev,
								&pmksa_mac);
		qdf_status =
			cm_sm_deliver_event(vdev,
					    WLAN_CM_SM_EV_CONNECT_SUCCESS,
					    sizeof(*resp), resp);
		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			return qdf_status;
		/*
		 * failure mean that the new connect/disconnect is received so
		 * cleanup.
		 */
		goto post_err;
	}

	/*
	 * Delete the PMKID of the BSSID for which the assoc reject is
	 * received from the AP due to invalid PMKID reason.
	 * This will avoid the driver trying to connect to same AP with
	 * the same stale PMKID. when connection is tried again with this AP.
	 */
	if (resp->status_code == STATUS_INVALID_PMKID)
		cm_delete_pmksa_for_bssid(cm_ctx, &pmksa_mac);

	/* In case of failure try with next candidate */
	qdf_status =
		cm_sm_deliver_event(vdev,
				    WLAN_CM_SM_EV_CONNECT_GET_NEXT_CANDIDATE,
				    sizeof(*resp), resp);

	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		return qdf_status;
	/*
	 * If connection fails with Single PMK bssid, clear this pmk
	 * entry in case of post failure.
	 */
	if (cm_is_cm_id_current_candidate_single_pmk(cm_ctx, cm_id))
		cm_delete_pmksa_for_single_pmk_bssid(cm_ctx, &pmksa_mac);
post_err:
	/*
	 * If there is a event posting error it means the SM state is not in
	 * JOIN ACTIVE (some new cmd has changed the state of SM), so just
	 * complete the connect command.
	 */
	cm_connect_complete(cm_ctx, resp);

	return qdf_status;
}

QDF_STATUS cm_bss_peer_create_rsp(struct wlan_objmgr_vdev *vdev,
				  QDF_STATUS status,
				  struct qdf_mac_addr *peer_mac)
{
	struct cnx_mgr *cm_ctx;
	QDF_STATUS qdf_status;
	wlan_cm_id cm_id;
	uint32_t prefix;
	struct wlan_cm_connect_resp *resp;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	cm_id = cm_ctx->active_cm_id;
	prefix = CM_ID_GET_PREFIX(cm_id);

	if (prefix != CONNECT_REQ_PREFIX) {
		mlme_err(CM_PREFIX_FMT "active req is not connect req",
			 CM_PREFIX_REF(wlan_vdev_get_id(cm_ctx->vdev), cm_id));
		mlme_cm_bss_peer_delete_req(vdev);
		return QDF_STATUS_E_INVAL;
	}

	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_status =
			cm_sm_deliver_event(vdev,
					  WLAN_CM_SM_EV_BSS_CREATE_PEER_SUCCESS,
					  sizeof(wlan_cm_id), &cm_id);
		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			return qdf_status;

		mlme_cm_bss_peer_delete_req(vdev);
		goto post_err;
	}

	/* In case of failure try with next candidate */
	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		qdf_status = QDF_STATUS_E_NOMEM;
		goto post_err;
	}

	cm_fill_failure_resp_from_cm_id(cm_ctx, resp, cm_id,
					CM_PEER_CREATE_FAILED);
	qdf_status =
		cm_sm_deliver_event(vdev,
				    WLAN_CM_SM_EV_CONNECT_GET_NEXT_CANDIDATE,
				    sizeof(*resp), resp);
	qdf_mem_free(resp);
	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		return qdf_status;

post_err:
	/*
	 * If there is a event posting error it means the SM state is not in
	 * JOIN ACTIVE (some new cmd has changed the state of SM), so just
	 * complete the connect command.
	 */
	cm_connect_handle_event_post_fail(cm_ctx, cm_id);
	return qdf_status;
}

static void
cm_copy_crypto_prarams(struct wlan_cm_connect_crypto_info *dst_params,
		       struct wlan_crypto_params  *src_params)
{
	/*
	 * As akm suites and ucast ciphers can be multiple. So, do ORing to
	 * keep it along with newly added one's (newly added one will anyway
	 * be part of it)
	 */
	dst_params->akm_suites |= src_params->key_mgmt;
	dst_params->auth_type = src_params->authmodeset;
	dst_params->ciphers_pairwise |= src_params->ucastcipherset;
	dst_params->group_cipher = src_params->mcastcipherset;
	dst_params->mgmt_ciphers = src_params->mgmtcipherset;
	dst_params->rsn_caps = src_params->rsn_caps;
}

static void
cm_set_crypto_params_from_ie(struct wlan_cm_connect_req *req)
{
	struct wlan_crypto_params crypto_params;
	QDF_STATUS status;
	uint8_t wsc_oui[OUI_LENGTH];
	uint8_t osen_oui[OUI_LENGTH];
	uint32_t oui_cpu;

	if (!req->assoc_ie.ptr)
		return;

	oui_cpu = qdf_be32_to_cpu(WSC_OUI);
	qdf_mem_copy(wsc_oui, &oui_cpu, OUI_LENGTH);
	oui_cpu = qdf_be32_to_cpu(OSEN_OUI);
	qdf_mem_copy(osen_oui, &oui_cpu, OUI_LENGTH);
	if (wlan_get_vendor_ie_ptr_from_oui(osen_oui, OUI_LENGTH,
					    req->assoc_ie.ptr,
					    req->assoc_ie.len))
		req->is_osen_connection = true;

	if (wlan_get_vendor_ie_ptr_from_oui(wsc_oui, OUI_LENGTH,
					    req->assoc_ie.ptr,
					    req->assoc_ie.len))
		req->is_wps_connection = true;

	status = wlan_get_crypto_params_from_rsn_ie(&crypto_params,
						    req->assoc_ie.ptr,
						    req->assoc_ie.len);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		cm_copy_crypto_prarams(&req->crypto, &crypto_params);
		return;
	}

	status = wlan_get_crypto_params_from_wpa_ie(&crypto_params,
						    req->assoc_ie.ptr,
						    req->assoc_ie.len);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		cm_copy_crypto_prarams(&req->crypto, &crypto_params);
		return;
	}

	status = wlan_get_crypto_params_from_wapi_ie(&crypto_params,
						     req->assoc_ie.ptr,
						     req->assoc_ie.len);
	if (QDF_IS_STATUS_SUCCESS(status))
		cm_copy_crypto_prarams(&req->crypto, &crypto_params);
}

static QDF_STATUS
cm_allocate_and_copy_ies_and_keys(struct wlan_cm_connect_req *target,
				  struct wlan_cm_connect_req *source)
{
	/* Reset the copied pointers of target */
	target->assoc_ie.ptr = NULL;
	target->crypto.wep_keys.key = NULL;
	target->crypto.wep_keys.seq = NULL;
	target->scan_ie.ptr = NULL;

	if (source->scan_ie.ptr) {
		target->scan_ie.ptr = qdf_mem_malloc(source->scan_ie.len);
		if (!target->scan_ie.ptr)
			target->scan_ie.len = 0;
		else
			qdf_mem_copy(target->scan_ie.ptr,
				     source->scan_ie.ptr, source->scan_ie.len);
	}

	if (source->assoc_ie.ptr) {
		target->assoc_ie.ptr = qdf_mem_malloc(source->assoc_ie.len);
		if (!target->assoc_ie.ptr)
			return QDF_STATUS_E_NOMEM;

		qdf_mem_copy(target->assoc_ie.ptr, source->assoc_ie.ptr,
			     source->assoc_ie.len);
	}

	if (source->crypto.wep_keys.key) {
		target->crypto.wep_keys.key =
			qdf_mem_malloc(source->crypto.wep_keys.key_len);
		if (!target->crypto.wep_keys.key)
			return QDF_STATUS_E_NOMEM;

		qdf_mem_copy(target->crypto.wep_keys.key,
			     source->crypto.wep_keys.key,
			     source->crypto.wep_keys.key_len);
	}

	if (source->crypto.wep_keys.seq) {
		target->crypto.wep_keys.seq =
			qdf_mem_malloc(source->crypto.wep_keys.seq_len);
		if (!target->crypto.wep_keys.seq)
			return QDF_STATUS_E_NOMEM;

		qdf_mem_copy(target->crypto.wep_keys.seq,
			     source->crypto.wep_keys.seq,
			     source->crypto.wep_keys.seq_len);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cm_connect_start_req(struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_connect_req *req)
{
	struct cnx_mgr *cm_ctx;
	struct cm_req *cm_req;
	struct cm_connect_req *connect_req;
	QDF_STATUS status;

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx)
		return QDF_STATUS_E_INVAL;

	cm_vdev_scan_cancel(wlan_vdev_get_pdev(cm_ctx->vdev), cm_ctx->vdev);

	/*
	 * This would be freed as part of removal from cm req list if adding
	 * to list is success after posting WLAN_CM_SM_EV_CONNECT_REQ.
	 */
	cm_req = qdf_mem_malloc(sizeof(*cm_req));
	if (!cm_req)
		return QDF_STATUS_E_NOMEM;

	if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
	    wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		req->is_non_assoc_link = 1;

	connect_req = &cm_req->connect_req;
	connect_req->req = *req;

	status = cm_allocate_and_copy_ies_and_keys(&connect_req->req, req);
	if (QDF_IS_STATUS_ERROR(status))
		goto err;

	cm_set_crypto_params_from_ie(&connect_req->req);

	if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		cm_teardown_tdls(vdev);

	status = cm_sm_deliver_event(vdev, WLAN_CM_SM_EV_CONNECT_REQ,
				     sizeof(*connect_req), connect_req);

err:
	/* free the req if connect is not handled */
	if (QDF_IS_STATUS_ERROR(status)) {
		cm_free_connect_req_mem(connect_req);
		qdf_mem_free(cm_req);
	}

	return status;
}
