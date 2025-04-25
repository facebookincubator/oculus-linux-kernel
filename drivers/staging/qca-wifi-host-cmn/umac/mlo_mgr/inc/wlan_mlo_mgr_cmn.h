/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains MLO manager public file containing common functionality
 */
#ifndef _WLAN_MLO_MGR_CMN_H_
#define _WLAN_MLO_MGR_CMN_H_

#include <qdf_types.h>
#include <qdf_trace.h>
#include "wlan_mlo_mgr_public_structs.h"
#include <wlan_mlo_mgr_main.h>

#define mlo_alert(format, args...) \
		QDF_TRACE_FATAL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_err(format, args...) \
		QDF_TRACE_ERROR(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_warn(format, args...) \
		QDF_TRACE_WARN(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_info(format, args...) \
		QDF_TRACE_INFO(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_debug(format, args...) \
		QDF_TRACE_DEBUG(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_alert(format, args...) \
		QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_err(format, args...) \
		QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_warn(format, args...) \
		QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_info(format, args...) \
		QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_debug(format, args...) \
		QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_alert_rl(format, args...) \
		QDF_TRACE_FATAL_RL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_err_rl(format, args...) \
		QDF_TRACE_ERROR_RL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_warn_rl(format, args...) \
		QDF_TRACE_WARN_RL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_info_rl(format, args...) \
		QDF_TRACE_INFO_RL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_debug_rl(format, args...) \
		QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_alert_rl(format, args...) \
		QDF_TRACE_FATAL_RL_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_err_rl(format, args...) \
		QDF_TRACE_ERROR_RL_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_warn_rl(format, args...) \
		QDF_TRACE_WARN_RL_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_info_rl(format, args...) \
		QDF_TRACE_INFO_RL_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#define mlo_nofl_debug_rl(format, args...) \
		QDF_TRACE_DEBUG_RL_NO_FL(QDF_MODULE_ID_MLO, format, ## args)

#if defined(WLAN_FEATURE_11BE_MLO_ENABLE_ENHANCED_TRACE)
#define mlo_etrace_debug(format, args...) \
		QDF_TRACE_DEBUG(QDF_MODULE_ID_MLO, format, ## args)
#define mlo_etrace_err_rl(format, args...) \
		QDF_TRACE_ERROR_RL(QDF_MODULE_ID_MLO, format, ## args)
#else
#define mlo_etrace_debug(format, args...)
#define mlo_etrace_err_rl(format, args...)
#endif

#define MLO_INVALID_LINK_IDX 0xFF
/**
 * mlo_get_link_information() - get partner link information
 * @mld_addr : MLD address
 * @info: partner link information
 *
 * Return: QDF_STATUS
 */
void mlo_get_link_information(struct qdf_mac_addr *mld_addr,
			      struct mlo_link_info *info);
/**
 * is_mlo_all_links_up() - check all the link status in a MLO device
 * @ml_dev: ML device context
 *
 * Return: QDF_STATUS
 */
void is_mlo_all_links_up(struct wlan_mlo_dev_context *ml_dev);

/**
 * mlo_get_vdev_by_link_id() - get vdev by link id
 * @vdev: vdev pointer
 * @link_id: link id
 * @id: debug id
 *
 * Return: vdev object pointer to link id
 */
struct wlan_objmgr_vdev *mlo_get_vdev_by_link_id(
			struct wlan_objmgr_vdev *vdev,
			uint8_t link_id, wlan_objmgr_ref_dbgid id);

/**
 * mlo_release_vdev_ref() - release vdev reference
 * @vdev: vdev pointer
 *
 * Return: void
 */
void mlo_release_vdev_ref(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_reg_mlme_ext_cb() - Function to register mlme extended callbacks
 * @ctx: Pointer to mlo manager global context
 * @ops: Pointer to the struct containing the callbacks
 *
 * Return: QDF_STATUS_SUCCESS on successful registration else failure
 */
QDF_STATUS mlo_reg_mlme_ext_cb(struct mlo_mgr_context *ctx,
			       struct mlo_mlme_ext_ops *ops);

/**
 * mlo_unreg_mlme_ext_cb() - Function to unregister mlme extended callbacks
 * @ctx: Pointer to mlo manager global context
 *
 * Return: QDF_STATUS_SUCCESS on success else failure
 */
QDF_STATUS mlo_unreg_mlme_ext_cb(struct mlo_mgr_context *ctx);

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * wlan_mlo_mgr_register_osif_ext_ops() - Function to register OSIF callbacks
 * @mlo_ctx: Global MLO manager pointer
 * @ops: Pointer to the struct containing OSIF callbacks.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_mgr_register_osif_ext_ops(struct mlo_mgr_context *mlo_ctx,
					      struct mlo_osif_ext_ops *ops);

/**
 * wlan_mlo_mgr_unregister_osif_ext_ops() - Function to unregister OSIF
 * callbacks
 * @mlo_ctx: Global MLO manager pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlo_mgr_unregister_osif_ext_ops(struct mlo_mgr_context *mlo_ctx);
#else
static inline QDF_STATUS
wlan_mlo_mgr_register_osif_ext_ops(struct mlo_mgr_context *mlo_ctx,
				   struct mlo_osif_ext_ops *ops)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlo_mgr_unregister_osif_ext_ops(struct mlo_mgr_context *mlo_ctx)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * mlo_mlme_clone_sta_security() - Clone Security params in partner vdevs
 * @vdev: Object manager vdev
 * @req: wlan_cm_connect_req data object to be passed to callback
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mlme_clone_sta_security(struct wlan_objmgr_vdev *vdev,
				       struct wlan_cm_connect_req *req);

/**
 * mlo_mlme_sta_op_class() - Update partner link op-class from ML-IE
 * @vdev: Object manager vdev
 * @ml_ie: buffer having the ML-IE from supplicant
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mlme_sta_op_class(struct wlan_objmgr_vdev *vdev,
				 uint8_t *ml_ie);

/**
 * mlo_mlme_validate_conn_req() - Validate connect request
 * @vdev: Object manager vdev
 * @ext_data: Data object to be passed to callback
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mlme_validate_conn_req(struct wlan_objmgr_vdev *vdev,
				      void *ext_data);

/**
 * mlo_mlme_create_link_vdev() - Create link vdev for ML STA
 * @vdev: Object manager vdev
 * @ext_data: Data object to be passed to callback
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_mlme_create_link_vdev(struct wlan_objmgr_vdev *vdev,
				     void *ext_data);

/**
 * mlo_mlme_peer_create() - Create mlo peer
 * @vdev: Object manager vdev
 * @ml_peer: MLO peer context
 * @addr: Peer addr
 * @frm_buf: Frame buffer for IE processing
 *
 * Return: void
 */
void mlo_mlme_peer_create(struct wlan_objmgr_vdev *vdev,
			  struct wlan_mlo_peer_context *ml_peer,
			  struct qdf_mac_addr *addr,
			  qdf_nbuf_t frm_buf);

/**
 * mlo_mlme_bridge_peer_create() - Create mlo bridge peer
 * @vdev: Object manager vdev
 * @ml_peer: MLO peer context
 * @addr: Peer addr
 * @frm_buf: Frame buffer for IE processing
 *
 * Return: void
 */
void mlo_mlme_bridge_peer_create(struct wlan_objmgr_vdev *vdev,
				 struct wlan_mlo_peer_context *ml_peer,
				 struct qdf_mac_addr *addr,
				 qdf_nbuf_t frm_buf);

/**
 * mlo_mlme_peer_assoc() - Send ML Peer assoc
 * @peer: Object manager peer
 *
 * Return: void
 */
void mlo_mlme_peer_assoc(struct wlan_objmgr_peer *peer);

/**
 * mlo_mlme_peer_assoc_fail() - Send ML Peer assoc fail
 * @peer: Object manager peer
 *
 * Return: void
 */
void mlo_mlme_peer_assoc_fail(struct wlan_objmgr_peer *peer);

/**
 * mlo_mlme_peer_delete() - Send ML Peer delete
 * @peer: Object manager peer
 *
 * Return: void
 */
void mlo_mlme_peer_delete(struct wlan_objmgr_peer *peer);

/**
 * mlo_mlme_peer_assoc_resp() - Initiate sending of Assoc response
 * @peer: Object manager peer
 *
 * Return: void
 */
void mlo_mlme_peer_assoc_resp(struct wlan_objmgr_peer *peer);

/**
 * mlo_mlme_get_link_assoc_req() - API to get link assoc req buffer
 * @peer: Object manager peer
 * @link_ix: link id of vdev
 *
 * Return: assoc req buffer
 */
qdf_nbuf_t mlo_mlme_get_link_assoc_req(struct wlan_objmgr_peer *peer,
				       uint8_t link_ix);

/**
 * mlo_mlme_peer_deauth() - Initiate deauth on link peer
 * @peer: Object manager peer
 * @is_disassoc: disassoc frame needs to be sent
 *
 * Return: void
 */
void mlo_mlme_peer_deauth(struct wlan_objmgr_peer *peer, uint8_t is_disassoc);

#ifdef UMAC_MLO_AUTH_DEFER
/**
 * mlo_mlme_peer_process_auth() - Process deferred auth request
 * @auth_param: deferred auth params
 *
 * Return: void
 */
void mlo_mlme_peer_process_auth(struct mlpeer_auth_params *auth_param);
#else
static inline void
mlo_mlme_peer_process_auth(struct mlpeer_auth_params *auth_param)
{
}
#endif

/**
 * mlo_mlme_peer_reassoc() - Reassoc mlo peer
 * @vdev: Object manager vdev
 * @ml_peer: MLO peer context
 * @addr: Peer addr
 * @frm_buf: Frame buffer for IE processing
 *
 * Return: void
 */
void mlo_mlme_peer_reassoc(struct wlan_objmgr_vdev *vdev,
			   struct wlan_mlo_peer_context *ml_peer,
			   struct qdf_mac_addr *addr,
			   qdf_nbuf_t frm_buf);

/**
 * mlo_get_link_vdev_ix() - Get index of link VDEV in MLD
 * @mldev: ML device context
 * @vdev: VDEV object
 *
 * Return: link index
 */

uint8_t mlo_get_link_vdev_ix(struct wlan_mlo_dev_context *mldev,
			     struct wlan_objmgr_vdev *vdev);

/**
 * mlo_get_ml_vdev_list() - get mlo vdev list
 * @vdev: vdev pointer
 * @vdev_count: vdev count
 * @wlan_vdev_list: vdev list
 *
 * Caller should release ref of the vdevs in wlan_vdev_list
 * Return: None
 */
void mlo_get_ml_vdev_list(struct wlan_objmgr_vdev *vdev,
			  uint16_t *vdev_count,
			  struct wlan_objmgr_vdev **wlan_vdev_list);

/**
 * mlo_mlme_handle_sta_csa_param() - process saved mlo sta csa param
 * @vdev: vdev pointer
 * @csa_param: saved csa_param
 *
 * Return: None
 */
void mlo_mlme_handle_sta_csa_param(struct wlan_objmgr_vdev *vdev,
				   struct csa_offload_params *csa_param);

#define INVALID_HW_LINK_ID 0xFFFF
#define WLAN_MLO_INVALID_NUM_LINKS             (-1)
#ifdef WLAN_MLO_MULTI_CHIP
#define WLAN_MLO_GROUP_INVALID                 0xFF
#define WLAN_MLO_CHIP_ID_INVALID               0xFF
/**
 * wlan_mlo_get_max_num_links() - Get the maximum number of MLO links
 * possible in the system
 * @grp_id: Id of the required MLO Group
 *
 * Return: Maximum number of MLO links in the system
 */
int8_t wlan_mlo_get_max_num_links(uint8_t grp_id);

/**
 * wlan_mlo_get_num_active_links() - Get the number of active MLO links
 * in the system
 * @grp_id: Id of the required MLO Group
 *
 * Return: Number of active MLO links in the system
 */
int8_t wlan_mlo_get_num_active_links(uint8_t grp_id);

/**
 * wlan_mlo_get_valid_link_bitmap() - Get the bitmap indicating the valid
 * MLO links in the system. If bit position i is set, link with id i is
 * valid.
 * @grp_id: Id of the required MLO Group
 *
 * Return: Valid link bitmap
 */
uint16_t wlan_mlo_get_valid_link_bitmap(uint8_t grp_id);

/**
 * wlan_mlo_get_pdev_hw_link_id() - Get hw_link_id of pdev
 * @pdev: pdev object
 *
 * Return: hw_link_id of the pdev.
 */
uint16_t wlan_mlo_get_pdev_hw_link_id(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_mlo_get_psoc_group_id() - Get MLO group id of psoc
 * @psoc: psoc object
 *
 * Return: MLO group id of the psoc
 */
uint8_t wlan_mlo_get_psoc_group_id(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlo_get_psoc_mlo_chip_id() - Get MLO chip id of psoc
 * @psoc: psoc object
 *
 * Return: MLO group id of the psoc
 */
uint8_t wlan_mlo_get_psoc_mlo_chip_id(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlo_get_psoc_capable() - Get if MLO capable psoc
 * @psoc: Pointer to psoc object
 *
 * Return: True if MLO capable else false
 */
bool wlan_mlo_get_psoc_capable(struct wlan_objmgr_psoc *psoc);

/**
 * struct hw_link_id_iterator: Argument passed in psoc/pdev iterator to
 *                             find pdev from hw_link_id
 * @hw_link_id: HW link id of pdev to find
 * @mlo_grp_id: MLO Group id which it belongs to
 * @dbgid: Module ref id used in iterator
 * @pdev: Pointer to pdev. This will be set inside itertor callback
 *        if hw_link_id match is found.
 */
struct hw_link_id_iterator {
	uint16_t hw_link_id;
	uint8_t mlo_grp_id;
	wlan_objmgr_ref_dbgid dbgid;
	struct wlan_objmgr_pdev *pdev;
};

/**
 * wlan_mlo_get_pdev_by_hw_link_id() - Get pdev object from hw_link_id
 * @hw_link_id: HW link id of the pdev
 * @ml_grp_id: MLO Group id which it belongs to
 * @refdbgid: dbgid of module used for taking reference to pdev object
 *
 * Return: Pointer to pdev object if hw_link_id is valid. Else, NULL
 *         Reference will be held with refdgid if return is non-NULL.
 *         Caller should free this reference.
 */
struct wlan_objmgr_pdev *
wlan_mlo_get_pdev_by_hw_link_id(uint16_t hw_link_id, uint8_t ml_grp_id,
				wlan_objmgr_ref_dbgid refdbgid);

#else
static inline int8_t
wlan_mlo_get_max_num_links(uint8_t grp_id)
{
	return WLAN_MLO_INVALID_NUM_LINKS;
}

static inline int8_t
wlan_mlo_get_num_active_links(uint8_t grp_id)
{
	return WLAN_MLO_INVALID_NUM_LINKS;
}

static inline uint16_t
wlan_mlo_get_valid_link_bitmap(uint8_t grp_id)
{
	return 0;
}

static inline struct wlan_objmgr_pdev *
wlan_mlo_get_pdev_by_hw_link_id(uint16_t hw_link_id, uint8_t ml_grp_id,
				wlan_objmgr_ref_dbgid refdbgid)
{
	return NULL;
}

static inline
uint16_t wlan_mlo_get_pdev_hw_link_id(struct wlan_objmgr_pdev *pdev)
{
	return INVALID_HW_LINK_ID;
}

static inline
uint8_t wlan_mlo_get_psoc_group_id(struct wlan_objmgr_psoc *psoc)
{
	return -EINVAL;
}

static inline
bool wlan_mlo_get_psoc_capable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif/*WLAN_MLO_MULTI_CHIP*/

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * mlo_process_link_set_active_resp() - handler for mlo link set active response
 * @psoc: psoc pointer
 * @event: pointer to mlo link set active response
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_process_link_set_active_resp(struct wlan_objmgr_psoc *psoc,
				 struct mlo_link_set_active_resp *event);

/**
 * mlo_ser_set_link_req() - add mlo link set active cmd to serialization
 * @req: mlo link set active request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlo_ser_set_link_req(struct mlo_link_set_active_req *req);

/**
 * typedef mlo_vdev_ops_handler() - API to have operation on ml vdevs
 * @vdev: vdev object
 * @arg: operation-specific argument
 */
typedef void (*mlo_vdev_ops_handler)(struct wlan_objmgr_vdev *vdev,
				     void *arg);

/**
 * mlo_iterate_ml_vdev_list() - Iterate on ML vdevs of MLD
 * @vdev: vdev object
 * @handler: the handler will be called for each object in ML list
 * @arg: argument to be passed to handler
 * @lock: Need to acquire lock or not
 *
 * Return: none
 */
static inline
void mlo_iterate_ml_vdev_list(struct wlan_objmgr_vdev *vdev,
			      mlo_vdev_ops_handler handler,
			      void *arg, bool lock)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx = NULL;
	uint8_t i = 0;
	QDF_STATUS status;

	if (!vdev)
		return;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx || !(wlan_vdev_mlme_is_mlo_vdev(vdev)))
		return;

	if (lock)
		mlo_dev_lock_acquire(mlo_dev_ctx);

	for (i =  0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!mlo_dev_ctx->wlan_vdev_list[i])
			continue;

		status = wlan_objmgr_vdev_try_get_ref(
					mlo_dev_ctx->wlan_vdev_list[i],
					WLAN_MLO_MGR_ID);
		if (QDF_IS_STATUS_ERROR(status))
			continue;

		if (handler)
			handler(mlo_dev_ctx->wlan_vdev_list[i], arg);

		mlo_release_vdev_ref(mlo_dev_ctx->wlan_vdev_list[i]);
	}

	if (lock)
		mlo_dev_lock_release(mlo_dev_ctx);
}

/**
 * struct mlo_stats_vdev_params - vdev params for MLO stats request
 * @ml_vdev_count: Num of connected mlo vdevs
 * @ml_vdev_id: vdev_ids of ml vdevs
 */
struct mlo_stats_vdev_params {
	uint8_t ml_vdev_count;
	uint8_t ml_vdev_id[WLAN_UMAC_MLO_MAX_VDEVS];
};

/**
 * mlo_get_mlstats_vdev_params() - Get vdev params for MLO stats
 * @psoc: psoc object
 * @vdev_id: vdev id
 * @ml_vdev_info: pointer to mlo_stats_vdev_params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_get_mlstats_vdev_params(struct wlan_objmgr_psoc *psoc,
			    struct mlo_stats_vdev_params *ml_vdev_info,
			    uint8_t vdev_id);

/**
 * typedef get_ml_link_state_cb() - api to handle link state callback
 * @ev: pointer to event parameter of structure
 * @cookie: a cookie for request context
 */
typedef void (*get_ml_link_state_cb)(struct ml_link_state_info_event *ev,
				     void *cookie);
/**
 * wlan_handle_ml_link_state_info_event() - Event handler for ml link state
 * @psoc: psoc handler
 * @event: pointer to event parameter of structure
 */
QDF_STATUS
wlan_handle_ml_link_state_info_event(struct wlan_objmgr_psoc *psoc,
				     struct ml_link_state_info_event *event);
/**
 * mlo_get_link_state_register_resp_cb() - Register link state callback
 * @vdev: vdev handler
 * @req: pointer to request parameter of structure
 */
QDF_STATUS
mlo_get_link_state_register_resp_cb(struct wlan_objmgr_vdev *vdev,
				    struct ml_link_state_cmd_info *req);
/**
 * ml_post_get_link_state_msg() - Post get link state msg
 * @vdev: vdev handler
 */
QDF_STATUS ml_post_get_link_state_msg(struct wlan_objmgr_vdev *vdev);

#endif
#ifdef WLAN_FEATURE_11BE
/**
 * util_add_bw_ind() - Adding bandwidth indiacation element
 * @bw_ind: pointer to bandwidth indication element
 * @ccfs0: EHT Channel Centre Frequency Segment0 information
 * @ccfs1: EHT Channel Centre Frequency Segment1 information
 * @ch_width: channel width
 * @puncture_bitmap: puncturing information
 * @bw_ind_len: pointer to length of bandwidth indication element
 */
QDF_STATUS
util_add_bw_ind(struct wlan_ie_bw_ind *bw_ind, uint8_t ccfs0,
		uint8_t ccfs1, enum phy_ch_width ch_width,
		uint16_t puncture_bitmap, int *bw_ind_len);

/**
 * util_parse_bw_ind() - Parsing of bandwidth indiacation element
 * @bw_ind: pointer to bandwidth indication element
 * @ccfs0: EHT Channel Centre Frequency Segment0 information
 * @ccfs1: EHT Channel Centre Frequency Segment1 information
 * @ch_width: channel width
 * @puncture_bitmap: puncturing information
 */

QDF_STATUS
util_parse_bw_ind(struct wlan_ie_bw_ind *bw_ind, uint8_t *ccfs0,
		  uint8_t *ccfs1, enum phy_ch_width *ch_width,
		  uint16_t *puncture_bitmap);
#endif

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
/**
 * mlo_mlme_ptqm_migrate_timer_cb() - Timer callback for ptqm migration
 * @arg: timer function argument
 *
 * Return: None
 */
void mlo_mlme_ptqm_migrate_timer_cb(void *arg);

/*
 * wlan_mlo_set_ptqm_migration() - API to trigger ptqm migration.
 * @vdev: vdev object
 * @ml_peer: ml peer object
 * @link_migration: flag to indicate if all peers of vdev need migration
 * or individual peer migration
 * @link_id: link id for new ptqm
 * @force_mig: allow migration to vdevs which are disabled to be pumac
 * using primary_umac_skip ini
 *
 * Return: Success if migration is triggered, else failure
 */
QDF_STATUS wlan_mlo_set_ptqm_migration(struct wlan_objmgr_vdev *vdev,
				       struct wlan_mlo_peer_context *ml_peer,
				       bool link_migration,
				       uint32_t link_id,
				       bool force_mig);
#endif

/*
 * wlan_mlo_is_csa_allow() - API to check if CSA allowed for MLO vdev
 * @vdev: vdev object
 * @csa_freq: CSA target freq
 *
 * Return: true if CSA allowed
 */
bool
wlan_mlo_is_csa_allow(struct wlan_objmgr_vdev *vdev, uint16_t csa_freq);
#endif
