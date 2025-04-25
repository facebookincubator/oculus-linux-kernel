/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains MLO manager public file containing link switch functionality
 */
#ifndef _WLAN_MLO_MGR_LINK_SWITCH_H_
#define _WLAN_MLO_MGR_LINK_SWITCH_H_

#include <wlan_mlo_mgr_public_structs.h>
#include <wlan_cm_public_struct.h>

struct wlan_channel;

#define WLAN_MLO_LSWITCH_MAX_HISTORY 5
#ifndef WLAN_MAX_ML_BSS_LINKS
#define WLAN_MAX_ML_BSS_LINKS 1
#endif

/**
 * enum wlan_mlo_link_switch_cnf_reason: Link Switch reject reason
 *
 * @MLO_LINK_SWITCH_CNF_REASON_BSS_PARAMS_CHANGED: Link's BSS params changed
 * @MLO_LINK_SWITCH_CNF_REASON_CONCURRECNY_CONFLICT: Rejected because of
 * Concurrency
 * @MLO_LINK_SWITCH_CNF_REASON_HOST_INTERNAL_ERROR: Host internal error
 * @MLO_LINK_SWITCH_CNF_REASON_MAX: Maximum reason for link switch rejection
 */
enum wlan_mlo_link_switch_cnf_reason {
	MLO_LINK_SWITCH_CNF_REASON_BSS_PARAMS_CHANGED = 1,
	MLO_LINK_SWITCH_CNF_REASON_CONCURRECNY_CONFLICT = 2,
	MLO_LINK_SWITCH_CNF_REASON_HOST_INTERNAL_ERROR = 3,
	MLO_LINK_SWITCH_CNF_REASON_MAX,
};

/**
 * enum wlan_mlo_link_switch_cnf_status: Link Switch Confirmation status
 *
 * @MLO_LINK_SWITCH_CNF_STATUS_ACCEPT: Link switch accepted
 * @MLO_LINK_SWITCH_CNF_STATUS_REJECT: Rejected because link switch cnf reason
 * @MLO_LINK_SWITCH_CNF_STATUS_MAX: Maximum reason for link status
 */
enum wlan_mlo_link_switch_cnf_status {
	MLO_LINK_SWITCH_CNF_STATUS_ACCEPT = 0,
	MLO_LINK_SWITCH_CNF_STATUS_REJECT = 1,
	MLO_LINK_SWITCH_CNF_STATUS_MAX,
};

/**
 * struct wlan_mlo_link_switch_cnf: structure to hold link switch conf info
 *
 * @vdev_id: VDEV ID of link switch link
 * @status: Link Switch Confirmation status
 * @reason: Link Switch Reject reason
 */
struct wlan_mlo_link_switch_cnf {
	uint32_t vdev_id;
	enum wlan_mlo_link_switch_cnf_status status;
	enum wlan_mlo_link_switch_cnf_reason reason;
};

/**
 * enum wlan_mlo_link_switch_reason- Reason for link switch
 *
 * @MLO_LINK_SWITCH_REASON_RSSI_CHANGE: Link switch reason is because of RSSI
 * @MLO_LINK_SWITCH_REASON_LOW_QUALITY: Link switch reason is because of low
 * quality
 * @MLO_LINK_SWITCH_REASON_C2_CHANGE: Link switch reason is because of C2 Metric
 * @MLO_LINK_SWITCH_REASON_HOST_FORCE: Link switch reason is because of host
 * force active/inactive
 * @MLO_LINK_SWITCH_REASON_T2LM: Link switch reason is because of T2LM
 * @MLO_LINK_SWITCH_REASON_MAX: Link switch reason max
 */
enum wlan_mlo_link_switch_reason {
	MLO_LINK_SWITCH_REASON_RSSI_CHANGE = 1,
	MLO_LINK_SWITCH_REASON_LOW_QUALITY = 2,
	MLO_LINK_SWITCH_REASON_C2_CHANGE   = 3,
	MLO_LINK_SWITCH_REASON_HOST_FORCE  = 4,
	MLO_LINK_SWITCH_REASON_T2LM        = 5,
	MLO_LINK_SWITCH_REASON_MAX,
};

/*
 * enum mlo_link_switch_req_state - Enum to maintain the current state of
 * link switch request.
 * @MLO_LINK_SWITCH_STATE_IDLE: The last link switch request is inactive
 * @MLO_LINK_SWITCH_STATE_INIT: Link switch is in pre-start state.
 * @MLO_LINK_SWITCH_STATE_DISCONNECT_CURR_LINK: Current link disconnect
 *                                              in progress.
 * @MLO_LINK_SWITCH_STATE_SET_MAC_ADDR: MAC address update in progress
 * @MLO_LINK_SWITCH_STATE_CONNECT_NEW_LINK: New link connect in progress.
 * @MLO_LINK_SWITCH_STATE_COMPLETE_SUCCESS: Link switch completed successfully
 * @MLO_LINK_SWITCH_STATE_ABORT_TRANS: Do not allow any further state
 *                                     transition, only allowed to move to
 *                                     MLO_LINK_SWITCH_STATE_IDLE state.
 */
enum mlo_link_switch_req_state {
	MLO_LINK_SWITCH_STATE_IDLE,
	MLO_LINK_SWITCH_STATE_INIT,
	MLO_LINK_SWITCH_STATE_DISCONNECT_CURR_LINK,
	MLO_LINK_SWITCH_STATE_SET_MAC_ADDR,
	MLO_LINK_SWITCH_STATE_CONNECT_NEW_LINK,
	MLO_LINK_SWITCH_STATE_COMPLETE_SUCCESS,
	MLO_LINK_SWITCH_STATE_ABORT_TRANS,
};

/**
 * struct wlan_mlo_link_switch_req - Data Structure because of link switch
 * request
 * @vdev_id: VDEV Id of the link which is under link switch
 * @curr_ieee_link_id: Current link id of the ML link
 * @new_ieee_link_id: Link id of the link to which going to link switched
 * @peer_mld_addr: Peer MLD address
 * @new_primary_freq: primary frequency of link switch link
 * @new_phymode: Phy mode of link switch link
 * @state: Current state of link switch
 * @reason: Link switch reason
 * @restore_vdev_flag: VDEV Flag to be restored post link switch.
 * @link_switch_ts: Link switch timestamp
 */
struct wlan_mlo_link_switch_req {
	uint8_t vdev_id;
	uint8_t curr_ieee_link_id;
	uint8_t new_ieee_link_id;
	struct qdf_mac_addr peer_mld_addr;
	uint32_t new_primary_freq;
	uint32_t new_phymode;
	enum mlo_link_switch_req_state state;
	enum wlan_mlo_link_switch_reason reason;
	bool restore_vdev_flag;
	qdf_time_t link_switch_ts;
};

/**
 * struct mlo_link_switch_stats - hold information regarding link switch stats
 * @total_num_link_switch: Total number of link switch
 * @req_reason: Reason of link switch received from FW
 * @cnf_reason: Confirm reason sent to FW
 * @req_ts: Link switch timestamp
 * @lswitch_status: structure to hold link switch status
 */
struct mlo_link_switch_stats {
	uint32_t total_num_link_switch;
	struct {
		enum wlan_mlo_link_switch_reason req_reason;
		enum wlan_mlo_link_switch_cnf_reason cnf_reason;
		qdf_time_t req_ts;
	} lswitch_status[WLAN_MLO_LSWITCH_MAX_HISTORY];
};

/**
 * struct mlo_link_switch_context - Link switch data structure.
 * @links_info: Hold information regarding all the links of ml connection
 * @last_req: Last link switch request received from FW
 * @lswitch_stats: History of the link switch stats
 *                 Includes both fail and success stats.
 */
struct mlo_link_switch_context {
	struct mlo_link_info links_info[WLAN_MAX_ML_BSS_LINKS];
	struct wlan_mlo_link_switch_req last_req;
	struct mlo_link_switch_stats lswitch_stats[MLO_LINK_SWITCH_CNF_STATUS_MAX];
};

/**
 * mlo_mgr_update_link_info_mac_addr() - MLO mgr update link info mac address
 * @vdev: Object manager vdev
 * @mlo_mac_update: ML link mac addresses update.
 *
 * Update link mac addresses for the ML links
 * Return: none
 */
void
mlo_mgr_update_link_info_mac_addr(struct wlan_objmgr_vdev *vdev,
				  struct wlan_mlo_link_mac_update *mlo_mac_update);

/**
 * mlo_mgr_update_link_info_reset() - Reset link info of ml dev context
 * @psoc: psoc pointer
 * @ml_dev: MLO device context
 *
 * Reset link info of ml links
 * Return: QDF_STATUS
 */
void mlo_mgr_update_link_info_reset(struct wlan_objmgr_psoc *psoc,
				    struct wlan_mlo_dev_context *ml_dev);

/**
 * mlo_mgr_update_ap_link_info() - Update AP links information
 * @vdev: Object Manager vdev
 * @link_id: Link id of the AP MLD link
 * @ap_link_addr: AP link addresses
 * @channel: wlan channel information of the link
 *
 * Update AP link information for each link of AP MLD
 * Return: void
 */
void mlo_mgr_update_ap_link_info(struct wlan_objmgr_vdev *vdev, uint8_t link_id,
				 uint8_t *ap_link_addr,
				 struct wlan_channel channel);

/**
 * mlo_mgr_clear_ap_link_info() - Clear AP link information
 * @vdev: Object Manager vdev
 * @ap_link_addr: AP link addresses
 *
 * Clear AP link info
 * Return: void
 */
void mlo_mgr_clear_ap_link_info(struct wlan_objmgr_vdev *vdev,
				uint8_t *ap_link_addr);

/**
 * mlo_mgr_reset_ap_link_info() - Reset AP links information
 * @vdev: Object Manager vdev
 *
 * Reset AP links information in MLD
 */
void mlo_mgr_reset_ap_link_info(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_mgr_update_ap_channel_info() - Update AP channel information
 * @vdev: Object Manager vdev
 * @link_id: Link id of the AP MLD link
 * @ap_link_addr: AP link addresses
 * @channel: wlan channel information of the link
 *
 * Update AP channel information for each link of AP MLD
 * Return: void
 */
void mlo_mgr_update_ap_channel_info(struct wlan_objmgr_vdev *vdev,
				    uint8_t link_id,
				    uint8_t *ap_link_addr,
				    struct wlan_channel channel);

/**
 * mlo_mgr_get_ap_link() - Assoc mlo link info from link id
 * @vdev: Object Manager vdev
 *
 * Get Assoc link info.
 *
 * Return: Pointer of link info
 */
struct mlo_link_info *mlo_mgr_get_ap_link(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * mlo_mgr_get_ap_link_by_link_id() - Get mlo link info from link id
 * @mlo_dev_ctx: mlo context
 * @link_id: Link id of the AP MLD link
 *
 * Search for the @link_id in the array in link_ctx in mlo_dev_ctx.
 * Returns the pointer of mlo_link_info element matching the @link_id,
 * or else NULL.
 *
 * Return: Pointer of link info
 */
struct mlo_link_info*
mlo_mgr_get_ap_link_by_link_id(struct wlan_mlo_dev_context *mlo_dev_ctx,
			       int link_id);

/**
 * mlo_mgr_update_csa_link_info - update mlo sta csa params
 * @pdev: pdev object manager
 * @mlo_dev_ctx: mlo dev ctx
 * @csa_param: csa parameters to be updated
 * @link_id: link id
 * Return : true if csa parameters are updated
 */
bool mlo_mgr_update_csa_link_info(struct wlan_objmgr_pdev *pdev,
				  struct wlan_mlo_dev_context *mlo_dev_ctx,
				  struct csa_offload_params *csa_param,
				  uint8_t link_id);

/**
 * mlo_mgr_osif_update_connect_info() - Update connection info to OSIF
 * layer on successful connection complete.
 * @vdev: VDEV object manager.
 * @link_id: IEEE protocol link id.
 *
 * The API will call OSIF connection update callback to update IEEE link id
 * as part of connection to MLO capable BSS. This is specifically needed to
 * make OSIF aware of all the links part of connection even about the links
 * for which VDEV doesn't exist.
 *
 * Return: void
 */
void mlo_mgr_osif_update_connect_info(struct wlan_objmgr_vdev *vdev,
				      int32_t link_id);

/**
 * mlo_mgr_link_switch_disconnect_done() - Notify MLO manager on link switch
 * disconnect complete.
 * @vdev: VDEV object manager
 * @status: Status of disconnect
 * @is_link_switch_resp: Set to true is disconnect response is for link switch
 * disconnect request else false.
 *
 * The API to decide on next sequence of tasks based on status on disconnect
 * request send as part of link switch. If the status is error, then abort
 * link switch or else continue.
 *
 * If API is called with @is_link_switch_resp argument as false, then some
 * other thread initiated disconnect, in this scenario change the state of
 * link switch to abort further state transition and return, in actual link
 * switch flow check this state to abort link switch.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mgr_link_switch_disconnect_done(struct wlan_objmgr_vdev *vdev,
					       QDF_STATUS status,
					       bool is_link_switch_resp);

/**
 * mlo_mgr_link_switch_set_mac_addr_resp() - Handle response of set MAC addr
 * for VDEV under going link switch.
 * @vdev: VDEV object manager
 * @resp_status: Status of MAC address set request.
 *
 * The function will handle the response for set MAC address request sent to FW
 * as part of link switch. If the response is error, then abort the link switch
 * and send the appropirate status to FW
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mgr_link_switch_set_mac_addr_resp(struct wlan_objmgr_vdev *vdev,
						 uint8_t resp_status);

/**
 * mlo_mgr_link_switch_start_connect() - Start link switch connect on new link
 * @vdev: VDEV pointer.
 *
 * Call the API to initiate connection for link switch post successful set mac
 * address on @vdev.
 *
 * Return:QDF_STATUS
 */
QDF_STATUS mlo_mgr_link_switch_start_connect(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_mgr_link_switch_connect_done() - Link switch connect done indication.
 * @vdev: VDEV object manager
 * @status: Status of connect request.
 *
 * The callback from connection manager with connect response.
 * If the response is failure, don't change the state of link switch.
 * If the response if success, set link switch state to
 * MLO_LINK_SWITCH_STATE_COMPLETE_SUCCESS.
 * Finally call remove link switch cmd from serialization.
 *
 * Return: void
 */
void mlo_mgr_link_switch_connect_done(struct wlan_objmgr_vdev *vdev,
				      QDF_STATUS status);

/**
 * mlo_mgr_link_switch_init_state() - Set the current state of link switch
 * to init state.
 * @mlo_dev_ctx: MLO dev context
 *
 * Sets the current state of link switch to MLO_LINK_SWITCH_STATE_IDLE with
 * MLO dev context lock held.
 *
 * Return: void
 */
void mlo_mgr_link_switch_init_state(struct wlan_mlo_dev_context *mlo_dev_ctx);

/**
 * mlo_mgr_link_switch_trans_next_state() - Transition to next state based
 * on current state.
 * @mlo_dev_ctx: MLO dev context
 *
 * Move to next state in link switch process based on current state with
 * MLO dev context lock held.
 *
 * Return: void
 */
QDF_STATUS
mlo_mgr_link_switch_trans_next_state(struct wlan_mlo_dev_context *mlo_dev_ctx);

/**
 * mlo_mgr_link_switch_trans_abort_state() - Transition to abort trans state.
 * @mlo_dev_ctx: ML dev context pointer of VDEV
 *
 * Transition the current link switch state to MLO_LINK_SWITCH_STATE_ABORT_TRANS
 * state, no further state transitions are allowed in the ongoing link switch
 * request.
 *
 * Return: void
 */
void
mlo_mgr_link_switch_trans_abort_state(struct wlan_mlo_dev_context *mlo_dev_ctx);

/**
 * mlo_mgr_link_switch_get_curr_state() - Get the current state of link switch.
 * @mlo_dev_ctx: MLO dev context.
 *
 * Get the current state of link switch with MLO dev context lock held.
 *
 * Return: void
 */
enum mlo_link_switch_req_state
mlo_mgr_link_switch_get_curr_state(struct wlan_mlo_dev_context *mlo_dev_ctx);

/**
 * mlo_mgr_is_link_switch_in_progress() - Check in link ctx in MLO dev context
 * if the last received link switch is in progress.
 * @vdev: VDEV object manager
 *
 * The API is to be called for VDEV which has MLO dev context and link context
 * initialized. Returns the value of 'is_in_progress' flag in last received
 * link switch request.
 *
 * Return: bool
 */
bool mlo_mgr_is_link_switch_in_progress(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_mgr_link_switch_notification() - Notify MLO manager on start
 * of link switch
 * @vdev: VDEV object manager
 * @lswitch_req: Link switch request params from FW
 * @notify_reason: Reason for link switch notification
 *
 * The link switch notifier callback to MLO manager invoked before starting
 * link switch disconnect
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mgr_link_switch_notification(struct wlan_objmgr_vdev *vdev,
					    struct wlan_mlo_link_switch_req *lswitch_req,
					    enum wlan_mlo_link_switch_notify_reason notify_reason);

/**
 * mlo_mgr_is_link_switch_on_assoc_vdev() - API to query whether link switch
 * is on-going on assoc VDEV.
 * @vdev: VDEV object manager
 *
 * Return: bool
 */
bool mlo_mgr_is_link_switch_on_assoc_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_mgr_link_switch_get_assoc_vdev() - Get current link switch VDEV
 * pointer if it is assoc VDEV.
 * @vdev: VDEV object manager.
 *
 * If the current link switch VDEV is assoc VDEV, fetch the pointer of that VDEV
 *
 * Return: VDEV object manager pointer
 */
struct wlan_objmgr_vdev *
mlo_mgr_link_switch_get_assoc_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_mgr_ser_link_switch_cmd() - The API will serialize link switch
 * command in serialization queue.
 * @vdev: VDEV objmgr pointer
 * @req: Link switch request parameters
 *
 * On receiving link switch request with valid parameters from FW, this
 * API will serialize the link switch command to procced for link switch
 * on @vdev once the command comes to active queue.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mgr_ser_link_switch_cmd(struct wlan_objmgr_vdev *vdev,
				       struct wlan_mlo_link_switch_req *req);

/**
 * mlo_mgr_remove_link_switch_cmd() - The API will remove the link switch
 * command from active serialization queue.
 * @vdev: VDEV object manager
 *
 * Once link switch process on @vdev is completed either in success of failure
 * case, the API removes the link switch command from serialization queue.
 *
 * Return: void
 */
void mlo_mgr_remove_link_switch_cmd(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_mgr_link_switch_notify() - API to notify registered link switch notify
 * callbacks.
 * @vdev: VDEV object manager
 * @req: Link switch request params from FW.
 *
 * The API calls all the registered link switch notifiers with appropriate
 * reason for notifications. Callback handlers to take necessary action based
 * on the reason.
 * If any callback returns error API will return error or else success.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
mlo_mgr_link_switch_notify(struct wlan_objmgr_vdev *vdev,
			   struct wlan_mlo_link_switch_req *req);

/**
 * mlo_mgr_link_switch_validate_request() - Validate link switch request
 * received from FW.
 * @vdev: VDEV object manager
 * @req: Request params from FW
 *
 * The API performs initial validation of link switch params received from FW
 * before serializing the link switch cmd. If any of the params is invalid or
 * the current status of MLO manager can't allow link switch, the API returns
 * failure and link switch has to be terminated.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_mgr_link_switch_validate_request(struct wlan_objmgr_vdev *vdev,
				     struct wlan_mlo_link_switch_req *req);

/**
 * mlo_mgr_link_switch_request_params() - Link switch request params from FW.
 * @psoc: PSOC object manager
 * @evt_params: Link switch params received from FW.
 *
 * The @params contain link switch request parameters received from FW as
 * an indication to host to trigger link switch sequence on the specified
 * VDEV. If the @params are not valid link switch will be terminated.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mgr_link_switch_request_params(struct wlan_objmgr_psoc *psoc,
					      void *evt_params);
/**
 * mlo_mgr_link_state_switch_info_handler() - Handle Link State change related
 * information and generate corresponding connectivity logging event
 * @psoc: Pointer to PSOC object
 * @info: Source info to be sent for the logging event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_mgr_link_state_switch_info_handler(struct wlan_objmgr_psoc *psoc,
				       struct mlo_link_switch_state_info *info);

/**
 * mlo_mgr_link_switch_complete() - Link switch complete notification to FW
 * @vdev: VDV object manager
 *
 * Notify the status of link switch to FW once the link switch sequence is
 * completed.
 *
 * Return: QDF_STATUS;
 */
QDF_STATUS mlo_mgr_link_switch_complete(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_mgr_link_switch_send_cnf_cmd() - Send status of link switch request to FW
 * @psoc: PSOC object manager
 * @cnf_params: Link switch confirm params to send to FW
 *
 * The API sends the link switch confirm params received to FW.
 * Returns error incase it failed to notify FW.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_mgr_link_switch_send_cnf_cmd(struct wlan_objmgr_psoc *psoc,
				 struct wlan_mlo_link_switch_cnf *cnf_params);

/**
 * mlo_mgr_link_switch_defer_disconnect_req() - Defer disconnect request from
 * source other than link switch
 * @vdev: VDEV object manager
 * @source: Disconnect requestor
 * @reason: Reason for disconnect
 *
 * If link switch is in progress for @vdev, then queue to disconnect request
 * received in the MLO dev context and move link switch state to abort and
 * on completion of link switch schedule pending disconnect requests.
 *
 * If link switch is not in progress or already another disconnect in queued in
 * MLO dev context then reject the disconnect defer request.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
mlo_mgr_link_switch_defer_disconnect_req(struct wlan_objmgr_vdev *vdev,
					 enum wlan_cm_source source,
					 enum wlan_reason_code reason);

/**
 * mlo_mgr_link_switch_init() - API to initialize link switch
 * @psoc: PSOC object manager
 * @ml_dev: MLO dev context
 *
 * Initializes the MLO link context in @ml_dev and allocates various
 * buffers needed.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mgr_link_switch_init(struct wlan_objmgr_psoc *psoc,
				    struct wlan_mlo_dev_context *ml_dev);

/**
 * mlo_mgr_link_switch_deinit() - API to de-initialize link switch
 * @ml_dev: MLO dev context
 *
 * De-initialize the MLO link context in @ml_dev on and frees memory
 * allocated as part of initialization.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mgr_link_switch_deinit(struct wlan_mlo_dev_context *ml_dev);

static inline bool
mlo_mgr_is_link_switch_supported(struct wlan_objmgr_vdev *vdev)
{
	return true;
}

#else
static inline struct mlo_link_info
*mlo_mgr_get_ap_link_by_link_id(struct wlan_mlo_dev_context *mlo_dev_ctx,
				int link_id)
{
	return NULL;
}

static inline bool
mlo_mgr_is_link_switch_supported(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
mlo_mgr_osif_update_connect_info(struct wlan_objmgr_vdev *vdev, int32_t link_id)
{
}

static inline QDF_STATUS
mlo_mgr_link_switch_disconnect_done(struct wlan_objmgr_vdev *vdev,
				    QDF_STATUS status,
				    bool is_link_switch_resp)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
mlo_mgr_link_switch_set_mac_addr_resp(struct wlan_objmgr_vdev *vdev,
				      uint8_t resp_status)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_mgr_link_switch_start_connect(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
mlo_mgr_link_switch_connect_done(struct wlan_objmgr_vdev *vdev,
				 QDF_STATUS status)
{
}

static inline QDF_STATUS
mlo_mgr_link_switch_deinit(struct wlan_mlo_dev_context *ml_dev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
mlo_mgr_link_switch_init(struct wlan_objmgr_psoc *psoc,
			 struct wlan_mlo_dev_context *ml_dev)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
mlo_mgr_link_switch_init_state(struct wlan_mlo_dev_context *mlo_dev_ctx)
{
}

static inline QDF_STATUS
mlo_mgr_link_switch_trans_next_state(struct wlan_mlo_dev_context *mlo_dev_ctx)
{
	return QDF_STATUS_E_INVAL;
}

static inline void
mlo_mgr_link_switch_trans_abort_state(struct wlan_mlo_dev_context *mlo_dev_ctx)
{
}

static inline enum mlo_link_switch_req_state
mlo_mgr_link_switch_get_curr_state(struct wlan_mlo_dev_context *mlo_dev_ctx)
{
	return MLO_LINK_SWITCH_STATE_IDLE;
}

static inline bool
mlo_mgr_is_link_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline QDF_STATUS
mlo_mgr_link_switch_notification(struct wlan_objmgr_vdev *vdev,
				 struct wlan_mlo_link_switch_req *lswitch_req,
				 enum wlan_mlo_link_switch_notify_reason notify_reason)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool
mlo_mgr_is_link_switch_on_assoc_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline struct wlan_objmgr_vdev *
mlo_mgr_link_switch_get_assoc_vdev(struct wlan_objmgr_vdev *vdev)
{
	return NULL;
}

static inline QDF_STATUS
mlo_mgr_ser_link_switch_cmd(struct wlan_objmgr_vdev *vdev,
			    struct wlan_mlo_link_switch_req *req)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
mlo_mgr_remove_link_switch_cmd(struct wlan_objmgr_vdev *vdev)
{
}

static inline QDF_STATUS
mlo_mgr_link_switch_notify(struct wlan_objmgr_vdev *vdev,
			   struct wlan_mlo_link_switch_req *req)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_mgr_link_switch_validate_request(struct wlan_objmgr_vdev *vdev,
				     struct wlan_mlo_link_switch_req *req)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_mgr_link_switch_request_params(struct wlan_objmgr_psoc *psoc,
				   void *evt_params)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_mgr_link_switch_complete(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_mgr_link_switch_send_cnf_cmd(struct wlan_objmgr_psoc *psoc,
				 struct wlan_mlo_link_switch_cnf *cnf_params)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_mgr_link_switch_defer_disconnect_req(struct wlan_objmgr_vdev *vdev,
					 enum wlan_cm_source source,
					 enum wlan_reason_code reason)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool
mlo_mgr_update_csa_link_info(struct wlan_mlo_dev_context *mlo_dev_ctx,
			     struct csa_offload_params *csa_param,
			     uint8_t link_id)
{
	return false;
}
#endif
#endif
