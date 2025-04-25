/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022,2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: declares nan component os interface APIs
 */

#ifndef _OS_IF_NAN_H_
#define _OS_IF_NAN_H_

#include "qdf_types.h"
#ifdef WLAN_FEATURE_NAN
#include "nan_public_structs.h"
#include "nan_ucfg_api.h"
#include "qca_vendor.h"
#include <wlan_cp_stats_chipset_stats.h>

/* QCA_NL80211_VENDOR_SUBCMD_NAN_EXT policy */
extern const struct nla_policy nan_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_NDP policy */
extern const struct nla_policy vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];

/**
 * struct ndi_find_vdev_filter - find vdev filter object. this can be extended
 * @ifname:           interface name of vdev
 * @found_vdev:       found vdev object matching one or more of above params
 */
struct ndi_find_vdev_filter {
	const char *ifname;
	struct wlan_objmgr_vdev *found_vdev;
};

/**
 * os_if_nan_process_ndp_cmd: os_if api to handle nan request message
 * @psoc: pointer to psoc object
 * @data: request data. contains vendor cmd tlvs
 * @data_len: length of data
 * @is_ndp_allowed: Indicates whether to allow NDP creation.
 *		    NDI creation is always allowed.
 * @wdev: Wireless device structure pointer
 *
 * Return: status of operation
 */
int os_if_nan_process_ndp_cmd(struct wlan_objmgr_psoc *psoc,
			      const void *data, int data_len,
			      bool is_ndp_allowed,
			      struct wireless_dev *wdev);

/**
 * os_if_nan_register_hdd_callbacks: os_if api to register hdd callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: struct pointer containing callbacks
 *
 * Return: status of operation
 */
int os_if_nan_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct nan_callbacks *cb_obj);

/**
 * os_if_nan_register_lim_callbacks: os_if api to register lim callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: struct pointer containing callbacks
 *
 * Return: status of operation
 */
int os_if_nan_register_lim_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct nan_callbacks *cb_obj);

/**
 * os_if_nan_post_ndi_create_rsp: os_if api to pos ndi create rsp to umac nan
 * component
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id of ndi
 * @success: if create was success or failure
 *
 * Return: None
 */
void os_if_nan_post_ndi_create_rsp(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool success);

/**
 * os_if_nan_post_ndi_delete_rsp: os_if api to pos ndi delete rsp to umac nan
 * component
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id of ndi
 * @success: if delete was success or failure
 *
 * Return: None
 */
void os_if_nan_post_ndi_delete_rsp(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool success);

/**
 * os_if_nan_ndi_session_end: os_if api to process ndi session end
 * component
 * @vdev: pointer to vdev deleted
 *
 * Return: None
 */
void os_if_nan_ndi_session_end(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_nan_set_ndi_state: os_if api set NDI state
 * @vdev: pointer to vdev deleted
 * @state: value to set
 *
 * Return: status of operation
 */
static inline QDF_STATUS os_if_nan_set_ndi_state(struct wlan_objmgr_vdev *vdev,
						 uint32_t state)
{
	return ucfg_nan_set_ndi_state(vdev, state);
}

/**
 * os_if_nan_set_ndp_create_transaction_id: set ndp create transaction id
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
static inline QDF_STATUS os_if_nan_set_ndp_create_transaction_id(
						struct wlan_objmgr_vdev *vdev,
						uint16_t val)
{
	return ucfg_nan_set_ndp_create_transaction_id(vdev, val);
}

/**
 * os_if_nan_set_ndp_delete_transaction_id: set ndp delete transaction id
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
static inline QDF_STATUS os_if_nan_set_ndp_delete_transaction_id(
						struct wlan_objmgr_vdev *vdev,
						uint16_t val)
{
	return ucfg_nan_set_ndp_delete_transaction_id(vdev, val);
}

/**
 * os_if_process_nan_req: os_if api to handle NAN requests attached to the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_NAN_EXT
 * @pdev: pointer to pdev object
 * @vdev_id: NAN vdev id
 * @data: request data. contains vendor cmd tlvs
 * @data_len: length of data
 *
 * Return: status of operation
 */
int os_if_process_nan_req(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			  const void *data, int data_len);
#else

static inline void os_if_nan_post_ndi_create_rsp(struct wlan_objmgr_psoc *psoc,
						 uint8_t vdev_id, bool success)
{
}

static inline void os_if_nan_post_ndi_delete_rsp(struct wlan_objmgr_psoc *psoc,
						 uint8_t vdev_id, bool success)
{
}

#endif /* WLAN_FEATURE_NAN */

#if defined(WLAN_FEATURE_NAN) && defined(WLAN_CHIPSET_STATS)
/**
 * os_if_cstats_log_ndp_initiator_req_evt() - Chipset stats for ndp
 * initiator request
 *
 * @req : pointer to nan_datapath_initiator_req
 *
 * Return : void
 */
void
os_if_cstats_log_ndp_initiator_req_evt(struct nan_datapath_initiator_req *req);

/**
 * os_if_cstats_log_ndp_responder_req_evt() - Chipset stats for ndp
 * responder request
 *
 * @vdev : pointer to vdev object
 * @req : pointer to nan_datapath_responder_req
 *
 * Return : void
 */
void
os_if_cstats_log_ndp_responder_req_evt(struct wlan_objmgr_vdev *vdev,
				       struct nan_datapath_responder_req *req);

/**
 * os_if_cstats_log_ndp_end_req_evt() - Chipset stats for ndp end
 * request event
 *
 * @vdev : pointer to vdev object
 * @rq : pointer to nan_datapath_end_req
 *
 * Return : void
 */
void os_if_cstats_log_ndp_end_req_evt(struct wlan_objmgr_vdev *vdev,
				      struct nan_datapath_end_req *rq);

/**
 * os_if_cstats_log_ndp_initiator_resp_evt() - Chipset stats for ndp
 * initiator request event
 *
 * @vdev : pointer to vdev object
 * @rsp : pointer to nan_datapath_end_req
 *
 * Return : void
 */
void
os_if_cstats_log_ndp_initiator_resp_evt(struct wlan_objmgr_vdev *vdev,
					struct nan_datapath_initiator_rsp *rsp);

/**
 * os_if_cstats_log_ndp_responder_resp_evt() - Chipset stats for ndp
 * responder response event
 *
 * @vdev : pointer to vdev object
 * @rsp : pointer to nan_datapath_responder_rsp
 *
 * Return : void
 */
void
os_if_cstats_log_ndp_responder_resp_evt(struct wlan_objmgr_vdev *vdev,
					struct nan_datapath_responder_rsp *rsp);

/**
 * os_if_cstats_log_ndp_indication_evt() - Chipset stats for ndp
 * indication event
 *
 * @vdev : pointer to vdev object
 * @evt : pointer to nan_datapath_indication_event object
 *
 * Return : void
 */
void
os_if_cstats_log_ndp_indication_evt(struct wlan_objmgr_vdev *vdev,
				    struct nan_datapath_indication_event *evt);

/**
 * os_if_cstats_log_ndp_confirm_evt() - Chipset stats for ndp
 * confirm event
 *
 * @vdev : pointer to vdev object
 * @nc : pointer to nan_datapath_confirm_event
 *
 * Return : void
 */
void os_if_cstats_log_ndp_confirm_evt(struct wlan_objmgr_vdev *vdev,
				      struct nan_datapath_confirm_event *nc);

/**
 * os_if_cstats_log_ndp_end_rsp_evt() - Chipset stats for ndp
 * end response event
 *
 * @vdev : pointer to vdev object
 * @rsp : pointer to nan_datapath_end_rsp_event object
 *
 * Return : void
 */
void os_if_cstats_log_ndp_end_rsp_evt(struct wlan_objmgr_vdev *vdev,
				      struct nan_datapath_end_rsp_event *rsp);

/**
 * os_if_cstats_log_ndp_new_peer_evt() - Chipset stats for ndp
 * new peer event
 *
 * @vdev : pointer to vdev object
 * @peer_ind : pointer to nan_datapath_peer_ind object
 *
 * Return : void
 */
void os_if_cstats_log_ndp_new_peer_evt(struct wlan_objmgr_vdev *vdev,
				       struct nan_datapath_peer_ind *peer_ind);

/**
 * os_if_cstats_log_ndi_delete_resp_evt() - Chipset stats for ndi
 * delete response event
 *
 * @vdev : pointer to vdev object
 * @rsp : pointer to nan_datapath_inf_delete_rsp object
 *
 * Return : void
 */
void
os_if_cstats_log_ndi_delete_resp_evt(struct wlan_objmgr_vdev *vdev,
				     struct nan_datapath_inf_delete_rsp *rsp);

/**
 * os_if_cstats_log_nan_disc_enable_req_evt() - Chipset stats for nan
 * discovery enable request
 *
 * @vdev_id : pointer to vdev object
 * @nan_req : pointer to nan_enable_req object
 *
 * Return : void
 */
void os_if_cstats_log_nan_disc_enable_req_evt(uint8_t vdev_id,
					      struct nan_enable_req *nan_req);

/**
 * os_if_cstats_log_disable_nan_disc_evt() - Chipset stats for nan
 * discovery disable event
 *
 * @pdev : pointer to pdev object
 * @vdev_id : vdev ID
 *
 * Return : void
 */
void os_if_cstats_log_disable_nan_disc_evt(struct wlan_objmgr_pdev *pdev,
					   uint8_t vdev_id);
#else
static inline void
os_if_cstats_log_ndp_initiator_req_evt(struct nan_datapath_initiator_req *req)
{
}

static inline void
os_if_cstats_log_ndp_responder_req_evt(struct wlan_objmgr_vdev *vdev,
				       struct nan_datapath_responder_req *req)
{
}

static inline void
os_if_cstats_log_ndp_end_req_evt(struct wlan_objmgr_vdev *vdev,
				 struct nan_datapath_end_req *rq)
{
}

static inline void
os_if_cstats_log_ndp_initiator_resp_evt(struct wlan_objmgr_vdev *vdev,
					struct nan_datapath_initiator_rsp *rsp)
{
}

static inline void
os_if_cstats_log_ndp_responder_resp_evt(struct wlan_objmgr_vdev *vdev,
					struct nan_datapath_responder_rsp *rsp)
{
}

static inline void
os_if_cstats_log_ndp_indication_evt(struct wlan_objmgr_vdev *vdev,
				    struct nan_datapath_indication_event *event)
{
}

static inline void
os_if_cstats_log_ndp_confirm_evt(struct wlan_objmgr_vdev *vdev,
				 struct nan_datapath_confirm_event *nc)
{
}

static inline void
os_if_cstats_log_ndp_end_rsp_evt(struct wlan_objmgr_vdev *vdev,
				 struct nan_datapath_end_rsp_event *rsp)
{
}

static inline void
os_if_cstats_log_ndp_new_peer_evt(struct wlan_objmgr_vdev *vdev,
				  struct nan_datapath_peer_ind *peer_ind)
{
}

static inline void
os_if_cstats_log_ndi_delete_resp_evt(struct wlan_objmgr_vdev *vdev,
				     struct nan_datapath_inf_delete_rsp *rsp)
{
}

static inline void
os_if_cstats_log_nan_disc_enable_req_evt(uint8_t vdev_id,
					 struct nan_enable_req *nan_req)
{
}

static inline void
os_if_cstats_log_disable_nan_disc_evt(struct wlan_objmgr_pdev *pdev,
				      uint8_t vdev_id)
{
}
#endif /* WLAN_CHIPSET_STATS */
#endif
