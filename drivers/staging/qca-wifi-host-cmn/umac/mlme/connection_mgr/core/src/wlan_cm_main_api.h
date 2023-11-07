/*
 * Copyright (c) 2012-2015, 2020-2021, The Linux Foundation. All rights reserved.
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

/**
 * DOC: wlan_cm_main_api.h
 *
 * This header file maintain connect, disconnect APIs of connection manager
 */

#ifndef __WLAN_CM_MAIN_API_H__
#define __WLAN_CM_MAIN_API_H__

#include "wlan_cm_main.h"
#include "wlan_cm_sm.h"
#include <include/wlan_mlme_cmn.h>
#include <wlan_crypto_global_api.h>
#include <wlan_if_mgr_api.h>
#ifdef WLAN_CM_USE_SPINLOCK
#include <scheduler_api.h>
#endif

#define CONNECT_REQ_PREFIX          0x0C000000
#define DISCONNECT_REQ_PREFIX       0x0D000000
#define ROAM_REQ_PREFIX             0x0F000000

#define CM_ID_MASK                  0x0000FFFF

#define CM_ID_GET_PREFIX(cm_id)     cm_id & 0xFF000000
#define CM_VDEV_ID_SHIFT            16
#define CM_VDEV_ID_MASK             0x00FF0000
#define CM_ID_GET_VDEV_ID(cm_id) (cm_id & CM_VDEV_ID_MASK) >> CM_VDEV_ID_SHIFT
#define CM_ID_SET_VDEV_ID(cm_id, vdev_id) ((vdev_id << CM_VDEV_ID_SHIFT) & \
					   CM_VDEV_ID_MASK) | cm_id

#define CM_PREFIX_FMT "vdev %d cm_id 0x%x: "
#define CM_PREFIX_REF(vdev_id, cm_id) (vdev_id), (cm_id)

/*************** CONNECT APIs ****************/

/**
 * cm_fill_failure_resp_from_cm_id() - This API will fill failure connect
 * response
 * @cm_ctx: connection manager context
 * @resp: connect failure resp
 * @cm_id: cm_id for connect response to be filled.
 * @reason: connect failure reason
 *
 * This function will fill connect failure response structure with the provided
 * reason with the help of given cm id.
 *
 * Return: void
 */
void cm_fill_failure_resp_from_cm_id(struct cnx_mgr *cm_ctx,
				     struct wlan_cm_connect_resp *resp,
				     wlan_cm_id cm_id,
				     enum wlan_cm_connect_fail_reason reason);

/**
 * cm_connect_start() - This API will be called to initiate the connect
 * process
 * @cm_ctx: connection manager context
 * @req: Connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_start(struct cnx_mgr *cm_ctx, struct cm_connect_req *req);

/**
 * cm_if_mgr_inform_connect_complete() - inform ifmanager the connect complete
 * @vdev: vdev for which connect cmpleted
 * @connect_status: connect status
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_if_mgr_inform_connect_complete(struct wlan_objmgr_vdev *vdev,
					     QDF_STATUS connect_status);

/**
 * cm_handle_connect_req_in_non_init_state() - Handle connect request in non
 * init state.
 * @cm_ctx: connection manager context
 * @cm_req: cm request
 * @cm_state_substate: state of CM SM
 *
 * Context: Can be called only while handling connection manager event
 *          ie holding state machine lock
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_handle_connect_req_in_non_init_state(struct cnx_mgr *cm_ctx,
					struct cm_connect_req *cm_req,
					enum wlan_cm_sm_state cm_state_substate);

/**
 * cm_handle_discon_req_in_non_connected_state() - Handle disconnect req in non
 * connected state.
 * @cm_ctx: connection manager context
 * @cm_req: cm request
 * @cm_state_substate: state of CM SM
 *
 * Context: Can be called only while handling connection manager event
 *          ie holding state machine lock
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_handle_discon_req_in_non_connected_state(struct cnx_mgr *cm_ctx,
					struct cm_disconnect_req *cm_req,
					enum wlan_cm_sm_state cm_state_substate);

/**
 * cm_connect_scan_start() - This API will be called to initiate the connect
 * scan if no candidate are found in scan db.
 * @cm_ctx: connection manager context
 * @req: Connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_scan_start(struct cnx_mgr *cm_ctx,
				 struct cm_connect_req *req);

/**
 * cm_connect_scan_resp() - Handle the connect scan resp and next action
 * scan if no candidate are found in scan db.
 * @cm_ctx: connection manager context
 * @scan_id: scan id of the req
 * @status: Connect scan status
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_scan_resp(struct cnx_mgr *cm_ctx, wlan_scan_id *scan_id,
				QDF_STATUS status);

/**
 * cm_connect_handle_event_post_fail() - initiate connect failure if msg posting
 * to SM fails
 * @cm_ctx: connection manager context
 * @cm_id: cm_id for connect req for which post fails
 *
 * Context: Can be called from any context and to be used only after posting a
 * msg to SM fails from external event e.g. peer create resp,
 * HW mode change resp  serialization cb.
 *
 * Return: QDF_STATUS
 */
void
cm_connect_handle_event_post_fail(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id);

/**
 * wlan_cm_scan_cb() - Callback function for scan for ssid
 * @vdev: VDEV MLME comp object
 * @event: scan event definition
 * @arg: reference to connection manager context
 *
 * API handles scan success/failure case
 *
 * Context: Can be called from any context.
 * Return: None
 */
void wlan_cm_scan_cb(struct wlan_objmgr_vdev *vdev,
		     struct scan_event *event, void *arg);

/**
 * cm_connect_resp_cmid_match_list_head() - Check if resp cmid is same as list
 * head
 * @cm_ctx: connection manager context
 * @resp: connect resp
 *
 * Return: bool
 */
bool cm_connect_resp_cmid_match_list_head(struct cnx_mgr *cm_ctx,
					  struct wlan_cm_connect_resp *resp);

/**
 * cm_connect_active() - This API would be called after the connect
 * request gets activated in serialization.
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_active(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id);

/**
 * cm_try_next_candidate() - This API would try to connect to next valid
 * candidate and fail if no candidate left
 * request gets activated in serialization.
 * @cm_ctx: connection manager context
 * @connect_resp: connect resp.
 *
 * Return: QDF status
 */
QDF_STATUS cm_try_next_candidate(struct cnx_mgr *cm_ctx,
				 struct wlan_cm_connect_resp *connect_resp);

/**
 * cm_resume_connect_after_peer_create() - Called after bss create rsp
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 *
 * Return: QDF status
 */
QDF_STATUS
cm_resume_connect_after_peer_create(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id);

/**
 * cm_bss_peer_create_rsp() - handle bss peer create response
 * @vdev: vdev
 * @status: bss peer create status
 * @peer_mac: peer mac
 *
 * Return: QDF status
 */
QDF_STATUS cm_bss_peer_create_rsp(struct wlan_objmgr_vdev *vdev,
				  QDF_STATUS status,
				  struct qdf_mac_addr *peer_mac);

/**
 * cm_connect_rsp() - Connection manager connect response
 * @vdev: vdev pointer
 * @resp: Connect response
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_connect_rsp(struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *resp);

/**
 * cm_notify_connect_complete() - This API would be called for sending
 * connect response notification
 * @cm_ctx: connection manager context
 * @resp: connection complete resp.
 * @acquire_lock: Flag to indicate whether this function needs
 * cm_ctx lock or not.
 *
 * This API would be called after connection completion resp from VDEV mgr
 *
 * Return: QDF status
 */
QDF_STATUS cm_notify_connect_complete(struct cnx_mgr *cm_ctx,
				      struct wlan_cm_connect_resp *resp,
				      bool acquire_lock);
/**
 * cm_connect_complete() - This API would be called after connect complete
 * request from the serialization.
 * @cm_ctx: connection manager context
 * @resp: Connection complete resp.
 *
 * This API would be called after connection completion resp from VDEV mgr
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_complete(struct cnx_mgr *cm_ctx,
			       struct wlan_cm_connect_resp *resp);

/**
 * cm_add_connect_req_to_list() - add connect req to the connection manager
 * req list
 * @cm_ctx: connection manager context
 * @req: Connection req provided
 *
 * Return: QDF status
 */
QDF_STATUS cm_add_connect_req_to_list(struct cnx_mgr *cm_ctx,
				      struct cm_connect_req *req);

/**
 * cm_connect_start_req() - Connect start req from the requester
 * @vdev: vdev on which connect is received
 * @req: Connection req provided
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_start_req(struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_connect_req *req);

/**
 * cm_send_connect_start_fail() - initiate connect failure
 * @cm_ctx: connection manager context
 * @req: connect req for which connect failed
 * @reason: failure reason
 *
 * Context: Can be called from any context and to be used only after posting a
 * msg to SM (ie holding the SM lock) to avoid use after free for req.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_send_connect_start_fail(struct cnx_mgr *cm_ctx,
			   struct cm_connect_req *req,
			   enum wlan_cm_connect_fail_reason reason);

/**
 * cm_find_bss_from_candidate_list() - get bss entry by bssid value
 * @candidate_list: candidate list
 * @bssid: bssid to find
 * @entry_found: found bss entry
 *
 * Return: true if find bss entry with bssid
 */
bool cm_find_bss_from_candidate_list(qdf_list_t *candidate_list,
				     struct qdf_mac_addr *bssid,
				     struct scan_cache_node **entry_found);

#ifdef WLAN_POLICY_MGR_ENABLE
/**
 * cm_hw_mode_change_resp() - HW mode change response
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @cm_id: connection ID which gave the hw mode change request
 * @status: status of the HW mode change.
 *
 * Return: void
 */
void cm_hw_mode_change_resp(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			    wlan_cm_id cm_id, QDF_STATUS status);

/**
 * cm_handle_hw_mode_change() - SM handling of hw mode change resp
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 * @event: HW mode success or failure event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_handle_hw_mode_change(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id,
				    enum wlan_cm_sm_evt event);
#else
static inline
QDF_STATUS cm_handle_hw_mode_change(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id,
				    enum wlan_cm_sm_evt event)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/*************** DISCONNECT APIs ****************/

/**
 * cm_disconnect_start() - Initiate the disconnect process
 * @cm_ctx: connection manager context
 * @req: Disconnect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_start(struct cnx_mgr *cm_ctx,
			       struct cm_disconnect_req *req);

/**
 * cm_disconnect_active() - This API would be called after the disconnect
 * request gets activated in serialization.
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_active(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id);

/**
 * cm_notify_disconnect_complete() - This API would be called for sending
 * disconnect response notification
 * @cm_ctx: connection manager context
 * @resp: disconnection complete resp.
 *
 * This API would be called after disconnect completion resp from VDEV mgr
 *
 * Return: QDF status
 */
QDF_STATUS cm_notify_disconnect_complete(struct cnx_mgr *cm_ctx,
					 struct wlan_cm_discon_rsp *resp);
/**
 * cm_disconnect_complete() - This API would be called after disconnect complete
 * request from the serialization.
 * @cm_ctx: connection manager context
 * @resp: disconnection complete resp.
 *
 * This API would be called after connection completion resp from VDEV mgr
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_complete(struct cnx_mgr *cm_ctx,
				  struct wlan_cm_discon_rsp *resp);

/**
 * cm_add_disconnect_req_to_list() - add disconnect req to the connection
 * manager req list
 * @cm_ctx: connection manager context
 * @req: Disconnection req provided
 *
 * Return: QDF status
 */
QDF_STATUS cm_add_disconnect_req_to_list(struct cnx_mgr *cm_ctx,
					 struct cm_disconnect_req *req);

/**
 * cm_disconnect_start_req() - Disconnect start req from the requester
 * @vdev: vdev on which connect is received
 * @req: disconnection req provided
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_start_req(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_disconnect_req *req);

/**
 * cm_disconnect_start_req_sync() - disconnect request with wait till
 * completed
 * @vdev: vdev pointer
 * @req: disconnect req
 *
 * Context: Only call for north bound disconnect req, if wait till complete
 * is required, e.g. during vdev delete. Do not call from scheduler context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_disconnect_start_req_sync(struct wlan_objmgr_vdev *vdev,
					struct wlan_cm_disconnect_req *req);

/**
 * cm_bss_peer_delete_req() - Connection manager bss peer delete
 * request
 * @vdev: VDEV object
 * @peer_mac: Peer mac address
 *
 * This function is called on peer delete indication and sends peer delete
 * request to mlme.
 *
 * Context: Any context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_bss_peer_delete_req(struct wlan_objmgr_vdev *vdev,
				  struct qdf_mac_addr *peer_mac);

/**
 * cm_vdev_down_req() - Connection manager req to send vdev down to FW
 * @vdev: VDEV object
 * @status: status
 *
 * This function is called when peer delete response is received, to send
 * vdev down request to mlme
 *
 * Context: Any context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_vdev_down_req(struct wlan_objmgr_vdev *vdev, uint32_t status);

/**
 * cm_disconnect_rsp() - Connection manager api to post connect event
 * @vdev: VDEV object
 * @resp: Disconnect response
 *
 * This function is called when disconnect response is received, to deliver
 * disconnect event to SM
 *
 * Context: Any context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_disconnect_rsp(struct wlan_objmgr_vdev *vdev,
			     struct wlan_cm_discon_rsp *resp);

/**
 * cm_initiate_internal_disconnect() - Initiate internal disconnect to cleanup
 * a active connect in case of back to back request
 * @cm_ctx: connection manager context
 *
 * Context: Can be called from any context. Hold the SM lock while calling this
 * api.
 *
 * Return: void
 */
void cm_initiate_internal_disconnect(struct cnx_mgr *cm_ctx);

/**
 * cm_send_disconnect_resp() - Initiate disconnect resp for the cm_id
 * @cm_ctx: connection manager context
 * @cm_id: cm id to send disconnect resp for
 *
 * Context: Can be called from any context. Hold the SM lock while calling this
 * api.
 *
 * Return: void
 */
void cm_send_disconnect_resp(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id);

/**
 * cm_disconnect_continue_after_rso_stop() - Continue disconnect after RSO stop
 * @vdev: Objmgr vdev
 * @req: pointer to cm vdev disconnect req
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_disconnect_continue_after_rso_stop(struct wlan_objmgr_vdev *vdev,
				      struct wlan_cm_vdev_discon_req *req);

/**
 * cm_handle_rso_stop_rsp() - Handle RSO stop response
 * @vdev: Objmgr vdev
 * @req: pointer to cm vdev disconnect req
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_handle_rso_stop_rsp(struct wlan_objmgr_vdev *vdev,
		       struct wlan_cm_vdev_discon_req *req);

/*************** UTIL APIs ****************/

/**
 * cm_ser_get_blocking_cmd() - check if serialization command needs to be
 * blocking
 *
 * Return: bool
 */
#ifdef CONN_MGR_ADV_FEATURE
static inline bool cm_ser_get_blocking_cmd(void)
{
	return true;
}
#else
static inline bool cm_ser_get_blocking_cmd(void)
{
	return false;
}
#endif

/**
 * cm_get_cm_id() - Get unique cm id for connect/disconnect request
 * @cm_ctx: connection manager context
 * @source: source of the request (can be connect or disconnect request)
 *
 * Return: cm id
 */
wlan_cm_id cm_get_cm_id(struct cnx_mgr *cm_ctx, enum wlan_cm_source source);

struct cnx_mgr *cm_get_cm_ctx_fl(struct wlan_objmgr_vdev *vdev,
				 const char *func, uint32_t line);

/**
 * cm_get_cm_ctx() - Get connection manager context from vdev
 * @vdev: vdev object pointer
 *
 * Return: pointer to connection manager context
 */
#define cm_get_cm_ctx(vdev) \
	cm_get_cm_ctx_fl(vdev, __func__, __LINE__)

cm_ext_t *cm_get_ext_hdl_fl(struct wlan_objmgr_vdev *vdev,
			    const char *func, uint32_t line);

/**
 * cm_get_ext_hdl() - Get connection manager ext context from vdev
 * @vdev: vdev object pointer
 *
 * Return: pointer to connection manager ext context
 */
#define cm_get_ext_hdl(vdev) \
	cm_get_ext_hdl_fl(vdev, __func__, __LINE__)

/**
 * cm_reset_active_cm_id() - Reset active cm_id from cm context, if its same as
 * passed cm_id
 * @vdev: vdev object pointer
 * @cm_id: cmid to match
 *
 * Return: void
 */
void cm_reset_active_cm_id(struct wlan_objmgr_vdev *vdev, wlan_cm_id cm_id);

#ifdef CRYPTO_SET_KEY_CONVERGED
/**
 * cm_set_key() - set wep or fils key on connection completion
 * @cm_ctx: connection manager context
 * @unicast: if key is unicast
 * @key_idx: Key index
 * @bssid: bssid of the connected AP
 *
 * Return: void
 */
QDF_STATUS cm_set_key(struct cnx_mgr *cm_ctx, bool unicast,
		      uint8_t key_idx, struct qdf_mac_addr *bssid);
#endif

#ifdef CONN_MGR_ADV_FEATURE
/**
 * cm_store_wep_key() - store wep keys in crypto on connect active
 * @cm_ctx: connection manager context
 * @crypto: connection crypto info
 * @cm_id: cm_id of the connection
 *
 * Return: void
 */
void cm_store_wep_key(struct cnx_mgr *cm_ctx,
		      struct wlan_cm_connect_crypto_info *crypto,
		      wlan_cm_id cm_id);

/**
 * cm_inform_dlm_connect_complete() - inform bsl about connect complete
 * @vdev: vdev
 * @resp: connect resp
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_inform_dlm_connect_complete(struct wlan_objmgr_vdev *vdev,
			       struct wlan_cm_connect_resp *resp);

static inline QDF_STATUS
cm_peer_create_on_bss_select_ind_resp(struct cnx_mgr *cm_ctx,
				      wlan_cm_id *cm_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS cm_bss_select_ind_rsp(struct wlan_objmgr_vdev *vdev,
				 QDF_STATUS status)
{
	return QDF_STATUS_SUCCESS;
}
#else
static inline void cm_store_wep_key(struct cnx_mgr *cm_ctx,
				    struct wlan_cm_connect_crypto_info *crypto,
				    wlan_cm_id cm_id)
{}

static inline QDF_STATUS
cm_inform_dlm_connect_complete(struct wlan_objmgr_vdev *vdev,
			       struct wlan_cm_connect_resp *resp)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * cm_peer_create_on_bss_select_ind_resp() - Called to create peer
 * if bss select inidication's resp was success
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 *
 * Return: QDF status
 */
QDF_STATUS
cm_peer_create_on_bss_select_ind_resp(struct cnx_mgr *cm_ctx,
				      wlan_cm_id *cm_id);

/**
 * cm_bss_select_ind_rsp() - Connection manager resp for bss
 * select indication
 * @vdev: vdev pointer
 * @status: Status
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_bss_select_ind_rsp(struct wlan_objmgr_vdev *vdev,
				 QDF_STATUS status);
#endif

#ifdef WLAN_FEATURE_FILS_SK
/**
 * cm_store_fils_key() - store fils keys in crypto on connection complete
 * @cm_ctx: connection manager context
 * @unicast: if key is unicast
 * @key_id: Key index
 * @key_length: key length
 * @key: key data
 * @bssid: bssid of the connected AP
 * @cm_id: cm_id of the connection
 *
 * Return: void
 */
void cm_store_fils_key(struct cnx_mgr *cm_ctx, bool unicast,
		       uint8_t key_id, uint16_t key_length,
		       uint8_t *key, struct qdf_mac_addr *bssid,
		       wlan_cm_id cm_id);
#endif

/**
 * cm_check_cmid_match_list_head() - check if list head command matches the
 * given cm_id
 * @cm_ctx: connection manager context
 * @cm_id: cm id of connect/disconnect req
 *
 * Check if front req command matches the given
 * cm_id, this can be used to check if the latest (head) is same we are
 * trying to processing
 *
 * Return: true if match else false
 */
bool cm_check_cmid_match_list_head(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id);

/**
 * cm_check_scanid_match_list_head() - check if list head command matches the
 * given scan_id
 * @cm_ctx: connection manager context
 * @scan_id: scan_id of connect req
 *
 * Check if front req command is connect command and matches the given
 * scan_id, this can be used to check if the latest (head) is same we are
 * trying to processing
 *
 * Return: true if match else false
 */
bool cm_check_scanid_match_list_head(struct cnx_mgr *cm_ctx,
				     wlan_scan_id *scan_id);

/**
 * cm_free_connect_req_mem() - free connect req internal memory, to be called
 * before cm_req is freed
 * @connect_req: connect req
 *
 * Return: void
 */
void cm_free_connect_req_mem(struct cm_connect_req *connect_req);

/**
 * cm_delete_req_from_list() - Delete the request matching cm id
 * @cm_ctx: connection manager context
 * @cm_id: cm id of connect/disconnect req
 *
 * Context: Can be called from any context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_delete_req_from_list(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id);

/**
 * cm_fill_bss_info_in_connect_rsp_by_cm_id() - fill bss info for the cm id
 * @cm_ctx: connection manager context
 * @cm_id: cm id of connect/disconnect req
 * @resp: resp to copy bss info like ssid/bssid and freq
 *
 * Fill the SSID form the connect req.
 * Fill freq and bssid from current candidate if available (i.e the connection
 * has tried to connect to a candidate), else get the bssid from req bssid or
 * bssid hint which ever is present.
 *
 * Return: Success if entry was found else failure
 */
QDF_STATUS
cm_fill_bss_info_in_connect_rsp_by_cm_id(struct cnx_mgr *cm_ctx,
					 wlan_cm_id cm_id,
					 struct wlan_cm_connect_resp *resp);

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
bool cm_is_cm_id_current_candidate_single_pmk(struct cnx_mgr *cm_ctx,
					      wlan_cm_id cm_id);
#else
static inline
bool cm_is_cm_id_current_candidate_single_pmk(struct cnx_mgr *cm_ctx,
					      wlan_cm_id cm_id)
{
	return false;
}
#endif

/**
 * cm_remove_cmd_from_serialization() - Remove requests matching cm id
 * from serialization.
 * @cm_ctx: connection manager context
 * @cm_id: cmd id to remove from serialization
 *
 * Context: Can be called from any context.
 *
 * Return: void
 */
void cm_remove_cmd_from_serialization(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id);

/**
 * cm_flush_pending_request() - Flush all pending requests matching flush prefix
 * @cm_ctx: connection manager context
 * @prefix: prefix for the type of command to flush
 * @only_failed_req: flush only the failed pending req
 *
 * Context: Can be called from any context.
 *
 * Return: void
 */
void cm_flush_pending_request(struct cnx_mgr *cm_ctx, uint32_t prefix,
			      bool only_failed_req);

/**
 * cm_remove_cmd() - Remove cmd from req list and serialization
 * @cm_ctx: connection manager context
 * @cm_id_to_remove: cm id of connect/disconnect/roam req
 *
 * Return: void
 */
void cm_remove_cmd(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id_to_remove);

/**
 * cm_add_req_to_list_and_indicate_osif() - Add the request to request list in
 * cm ctx and indicate same to osif
 * @cm_ctx: connection manager context
 * @cm_req: cm request
 * @source: source of request
 *
 * Context: Can be called from any context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_add_req_to_list_and_indicate_osif(struct cnx_mgr *cm_ctx,
						struct cm_req *cm_req,
						enum wlan_cm_source source);

struct cm_req *cm_get_req_by_cm_id_fl(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id,
				      const char *func, uint32_t line);

/**
 * cm_get_req_by_cm_id() - Get cm req matching the cm id
 * @cm_ctx: connection manager context
 * @cm_id: cm id of connect/disconnect req
 *
 * Context: Can be called from any context and to be used only after posting a
 * msg to SM (ie holding the SM lock) to avoid use after free. also returned req
 * should only be used till SM lock is hold.
 *
 * Return: cm req from the req list whose cm id matches the argument
 */
#define cm_get_req_by_cm_id(cm_ctx, cm_id) \
	cm_get_req_by_cm_id_fl(cm_ctx, cm_id, __func__, __LINE__)

/**
 * cm_vdev_scan_cancel() - cancel all scans for vdev
 * @pdev: pdev pointer
 * @vdev: vdev for which scan to be canceled
 *
 * Return: void
 */
void cm_vdev_scan_cancel(struct wlan_objmgr_pdev *pdev,
			 struct wlan_objmgr_vdev *vdev);

/**
 * cm_fill_disconnect_resp_from_cm_id() - Fill disconnect response
 * @cm_ctx: connection manager context
 * @cm_id: cm id of connect/disconnect req
 * @resp: Disconnect response which needs to filled
 *
 * This function is called to fill disconnect response from cm id
 *
 * Context: Any Context.
 *
 * Return: Success if disconnect
 */
QDF_STATUS
cm_fill_disconnect_resp_from_cm_id(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id,
				   struct wlan_cm_discon_rsp *resp);

/**
 * cm_inform_bcn_probe() - update scan db with beacon or probe resp
 * @cm_ctx: connection manager context
 * @bcn_probe: beacon or probe resp received during connect
 * @len: beacon or probe resp length
 * @freq: scan frequency in MHz
 * @rssi: rssi of the beacon or probe resp
 * @cm_id: cm id of connect/disconnect req
 *
 * update scan db, so that kernel and driver do not age out
 * the connected AP entry.
 *
 * Context: Any Context.
 *
 * Return: void
 */
void cm_inform_bcn_probe(struct cnx_mgr *cm_ctx, uint8_t *bcn_probe,
			 uint32_t len, qdf_freq_t freq, int32_t rssi,
			 wlan_cm_id cm_id);

/**
 * cm_set_max_connect_attempts() - Set max connect attempts
 * @vdev: vdev pointer
 * @max_connect_attempts: max connect attempts to be set.
 *
 * Set max connect attempts. Max value is limited to CM_MAX_CONNECT_ATTEMPTS.
 *
 * Return: void
 */
void cm_set_max_connect_attempts(struct wlan_objmgr_vdev *vdev,
				 uint8_t max_connect_attempts);

/**
 * cm_trigger_panic_on_cmd_timeout() - trigger panic on active command timeout
 * @vdev: vdev pointer
 *
 * Return: void
 */
void cm_trigger_panic_on_cmd_timeout(struct wlan_objmgr_vdev *vdev);

/**
 * cm_set_max_connect_timeout() - Set max connect timeout
 * @vdev: vdev pointer
 * @max_connect_timeout: max connect timeout to be set.
 *
 * Set max connect timeout.
 *
 * Return: void
 */
void cm_set_max_connect_timeout(struct wlan_objmgr_vdev *vdev,
				uint32_t max_connect_timeout);

/**
 * cm_is_vdev_connecting() - check if vdev is in conneting state
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_connecting(struct wlan_objmgr_vdev *vdev);

/**
 * cm_is_vdev_connected() - check if vdev is in conneted state
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_connected(struct wlan_objmgr_vdev *vdev);

/**
 * cm_is_vdev_active() - check if vdev is in active state ie conneted or roaming
 * state
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_active(struct wlan_objmgr_vdev *vdev);

/**
 * cm_is_vdev_disconnecting() - check if vdev is in disconnecting state
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_disconnecting(struct wlan_objmgr_vdev *vdev);

/**
 * cm_is_vdev_disconnected() - check if vdev is disconnected/init state
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_disconnected(struct wlan_objmgr_vdev *vdev);

/**
 * cm_is_vdev_roaming() - check if vdev is in roaming state
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_roaming(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * cm_is_vdev_roam_started() - check if vdev is in roaming state and
 * roam started sub stated
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_roam_started(struct wlan_objmgr_vdev *vdev);

/**
 * cm_is_vdev_roam_sync_inprogress() - check if vdev is in roaming state
 * and roam sync substate
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_roam_sync_inprogress(struct wlan_objmgr_vdev *vdev);
#endif

#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * cm_is_vdev_roam_preauth_state() - check if vdev is in roaming state and
 * preauth is in progress
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_roam_preauth_state(struct wlan_objmgr_vdev *vdev);

/**
 * cm_is_vdev_roam_reassoc_state() - check if vdev is in roaming state
 * and reassoc is in progress
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool cm_is_vdev_roam_reassoc_state(struct wlan_objmgr_vdev *vdev);
#endif

/**
 * cm_get_active_req_type() - CM active req type
 * @vdev: vdev pointer
 *
 * Return: CM active req type
 */
enum wlan_cm_active_request_type
cm_get_active_req_type(struct wlan_objmgr_vdev *vdev);

/**
 * cm_get_active_connect_req() - Get copy of active connect request
 * @vdev: vdev pointer
 * @req: pointer to the copy of the active connect request
 * *
 * Context: Should be called only in the context of the
 * cm request activation
 *
 * Return: true and connect req if any request is active
 */
bool cm_get_active_connect_req(struct wlan_objmgr_vdev *vdev,
			       struct wlan_cm_vdev_connect_req *req);

/**
 * cm_get_active_disconnect_req() - Get copy of active disconnect request
 * @vdev: vdev pointer
 * @req: pointer to the copy of the active disconnect request
 * *
 * Context: Should be called only in the context of the
 * cm request activation
 *
 * Return: true and disconnect req if any request is active
 */
bool cm_get_active_disconnect_req(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_vdev_discon_req *req);

/**
 * cm_connect_handle_event_post_fail() - initiate connect failure if msg posting
 * to SM fails
 * @cm_ctx: connection manager context
 * @cm_id: cm_id for connect req for which post fails
 *
 * Context: Can be called from any context and to be used only after posting a
 * msg to SM fails from external event e.g. peer create resp,
 * HW mode change resp  serialization cb.
 *
 * Return: QDF_STATUS
 */
void
cm_connect_handle_event_post_fail(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id);

/**
 * cm_get_req_by_scan_id() - Get cm req matching the scan id
 * @cm_ctx: connection manager context
 * @scan_id: scan id of scan req
 *
 * Context: Can be called from any context and to be used only after posting a
 * msg to SM (ie holding the SM lock) to avoid use after free. also returned req
 * should only be used till SM lock is hold.
 *
 * Return: cm req from the req list whose scan id matches the argument
 */
struct cm_req *cm_get_req_by_scan_id(struct cnx_mgr *cm_ctx,
				     wlan_scan_id scan_id);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * cm_connect_resp_fill_mld_addr_from_candidate() - API to fill MLD
 * address in connect resp from scan entry.
 * @vdev: VDEV objmgr pointer.
 * @entry: Scan entry.
 * @resp: connect response pointer.
 *
 * If the MLO VDEV flag is set, get the MLD address from the scan
 * entry and fill in MLD address field in @resp.
 *
 * Return: void
 */
void
cm_connect_resp_fill_mld_addr_from_candidate(struct wlan_objmgr_vdev *vdev,
					     struct scan_cache_entry *entry,
					     struct wlan_cm_connect_resp *resp);
/**
 * cm_connect_resp_fill_mld_addr_from_cm_id() - API to fill MLD address
 * in connect resp from connect request ID.
 * @vdev: VDEV objmgr pointer.
 * @cm_id: connect request ID.
 * @rsp: connect resp pointer.
 *
 * The API gets scan entry from the connect request using the connect request
 * ID and fills MLD address from the scan entry into the connect response.
 *
 * Return: void
 */
void
cm_connect_resp_fill_mld_addr_from_cm_id(struct wlan_objmgr_vdev *vdev,
					 wlan_cm_id cm_id,
					 struct wlan_cm_connect_resp *rsp);

static inline void
cm_connect_rsp_get_mld_addr_or_bssid(struct wlan_cm_connect_resp *resp,
				     struct qdf_mac_addr *bssid)
{
	if (!qdf_is_macaddr_zero(&resp->mld_addr))
		qdf_copy_macaddr(bssid, &resp->mld_addr);
	else
		qdf_copy_macaddr(bssid, &resp->bssid);
}
#else
static inline void
cm_connect_resp_fill_mld_addr_from_candidate(struct wlan_objmgr_vdev *vdev,
					     struct scan_cache_entry *entry,
					     struct wlan_cm_connect_resp *resp)
{
}

static inline void
cm_connect_resp_fill_mld_addr_from_cm_id(struct wlan_objmgr_vdev *vdev,
					 wlan_cm_id cm_id,
					 struct wlan_cm_connect_resp *rsp)
{
}

static inline void
cm_connect_rsp_get_mld_addr_or_bssid(struct wlan_cm_connect_resp *resp,
				     struct qdf_mac_addr *bssid)
{
	qdf_copy_macaddr(bssid, &resp->bssid);
}
#endif

/**
 * cm_get_cm_id_by_scan_id() - Get cm id by matching the scan id
 * @cm_ctx: connection manager context
 * @scan_id: scan id of scan req
 *
 * Context: Can be called from any context and used to get cm_id
 * from scan id when SM lock is not held
 *
 * Return: cm id from the req list whose scan id matches the argument
 */
wlan_cm_id cm_get_cm_id_by_scan_id(struct cnx_mgr *cm_ctx,
				   wlan_scan_id scan_id);

/**
 * cm_update_scan_mlme_on_disconnect() - update the scan mlme info
 * on disconnect completion
 * @vdev: Object manager vdev
 * @req: Disconnect request
 *
 * Return: void
 */
void
cm_update_scan_mlme_on_disconnect(struct wlan_objmgr_vdev *vdev,
				  struct cm_disconnect_req *req);

/**
 * cm_calculate_scores() - Score the candidates obtained from scan
 * manager after filtering
 * @cm_ctx: Connection manager context
 * @pdev: Object manager pdev
 * @filter: Scan filter params
 * @list: List of candidates to be scored
 *
 * Return: void
 */
void cm_calculate_scores(struct cnx_mgr *cm_ctx,
			 struct wlan_objmgr_pdev *pdev,
			 struct scan_filter *filter, qdf_list_t *list);

/**
 * cm_req_lock_acquire() - Acquire connection manager request lock
 * @cm_ctx: Connection manager context
 *
 * Return: void
 */
void cm_req_lock_acquire(struct cnx_mgr *cm_ctx);

/**
 * cm_req_lock_release() - Release connection manager request lock
 * @cm_ctx: Connection manager context
 *
 * Return: void
 */
void cm_req_lock_release(struct cnx_mgr *cm_ctx);

#ifdef SM_ENG_HIST_ENABLE
/**
 * cm_req_history_add() - Save request history
 * @cm_ctx: Connection manager context
 * @cm_req: Connection manager request
 *
 * Return: void
 */
void cm_req_history_add(struct cnx_mgr *cm_ctx,
			struct cm_req *cm_req);
/**
 * cm_req_history_del() - Update history on request deletion
 * @cm_ctx: Connection manager context
 * @cm_req: Connection manager request
 * @del_type: Context in which the request is deleted
 *
 * Return: void
 */
void cm_req_history_del(struct cnx_mgr *cm_ctx,
			struct cm_req *cm_req,
			enum cm_req_del_type del_type);

/**
 * cm_req_history_init() - Initialize the history data struct
 * @cm_ctx: Connection manager context
 *
 * Return: void
 */
void cm_req_history_init(struct cnx_mgr *cm_ctx);

/**
 * cm_req_history_deinit() - Deinitialize the history data struct
 * @cm_ctx: Connection manager context
 *
 * Return: void
 */
void cm_req_history_deinit(struct cnx_mgr *cm_ctx);

/**
 * cm_req_history_print() - Print the history data struct
 * @cm_ctx: Connection manager context
 *
 * Return: void
 */
void cm_req_history_print(struct cnx_mgr *cm_ctx);
extern struct wlan_sm_state_info cm_sm_info[];
#else
static inline
void cm_req_history_add(struct cnx_mgr *cm_ctx,
			struct cm_req *cm_req)
{}

static inline
void cm_req_history_del(struct cnx_mgr *cm_ctx,
			struct cm_req *cm_req,
			enum cm_req_del_type del_type)
{}

static inline void cm_req_history_init(struct cnx_mgr *cm_ctx)
{}

static inline void cm_req_history_deinit(struct cnx_mgr *cm_ctx)
{}

static inline void cm_req_history_print(struct cnx_mgr *cm_ctx)
{}
#endif

#ifdef WLAN_CM_USE_SPINLOCK
/**
 * cm_activate_cmd_req_flush_cb() - Callback when the scheduler msg is flushed
 * @msg: scheduler message
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_activate_cmd_req_flush_cb(struct scheduler_msg *msg);
#endif

#ifndef CONN_MGR_ADV_FEATURE
/**
 * cm_set_candidate_advance_filter_cb() - Set CM candidate advance
 * filter cb
 * @vdev: Objmgr vdev
 * @filter_fun: CM candidate advance filter cb
 *
 * Return: void
 */
void cm_set_candidate_advance_filter_cb(
		struct wlan_objmgr_vdev *vdev,
		void (*filter_fun)(struct wlan_objmgr_vdev *vdev,
				   struct scan_filter *filter));

/**
 * cm_set_candidate_custom_sort_cb() - Set CM candidate custom sort cb
 * @vdev: Objmgr vdev
 * @sort_fun: CM candidate custom sort cb
 *
 * Return: void
 */
void cm_set_candidate_custom_sort_cb(
		struct wlan_objmgr_vdev *vdev,
		void (*sort_fun)(struct wlan_objmgr_vdev *vdev,
				 qdf_list_t *list));

#endif

/**
 * cm_is_connect_req_reassoc() - Is connect req for reassoc
 * @req: connect req
 *
 * Return: void
 */
bool cm_is_connect_req_reassoc(struct wlan_cm_connect_req *req);

#ifdef CONN_MGR_ADV_FEATURE
/**
 * cm_free_connect_rsp_ies() - Function to free all connection IEs.
 * @connect_rsp: pointer to connect rsp
 *
 * Function to free up all the IE in connect response structure.
 *
 * Return: void
 */
void cm_free_connect_rsp_ies(struct wlan_cm_connect_resp *connect_rsp);

/**
 * cm_store_first_candidate_rsp() - store the connection failure response
 * @cm_ctx: connection manager context
 * @cm_id: cm_id for connect response to be filled
 * @resp: first connect failure response
 *
 * This API would be called when candidate fails to connect. It will cache the
 * first connect failure response in connect req structure.
 *
 * Return: void
 */
void cm_store_first_candidate_rsp(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id,
				  struct wlan_cm_connect_resp *resp);

/**
 * cm_get_first_candidate_rsp() - fetch first candidate response
 * @cm_ctx: connection manager context
 * @cm_id: cm_id for connect response to be filled
 * @first_candid_rsp: first connect failure response
 *
 * This API would be called when last candidate is failed to connect. It will
 * fetch the first candidate failure response which was cached in connect
 * request structure.
 *
 * Return: QDF_STATUS_SUCCESS when rsp is fetch successfully
 */
QDF_STATUS
cm_get_first_candidate_rsp(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id,
			   struct wlan_cm_connect_resp *first_candid_rsp);

/**
 * cm_store_n_send_failed_candidate() - stored failed connect response and sent
 * it to osif.
 * @cm_ctx: connection manager context
 * @cm_id: connection manager id
 *
 * This API will stored failed connect response in connect request structure
 * and sent it to osif layer.
 *
 * Return: void
 */
void cm_store_n_send_failed_candidate(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id);
#else
static inline
void cm_free_connect_rsp_ies(struct wlan_cm_connect_resp *connect_rsp)
{
}

static inline
void cm_store_first_candidate_rsp(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id,
				  struct wlan_cm_connect_resp *resp)
{
}

static inline
void cm_store_n_send_failed_candidate(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id)
{
}
#endif /* CONN_MGR_ADV_FEATURE */
#endif /* __WLAN_CM_MAIN_API_H__ */
