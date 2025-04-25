/*
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
 * DOC: contains MLO manager public file containing roaming functionality
 */
#ifndef _WLAN_MLO_MGR_ROAM_H_
#define _WLAN_MLO_MGR_ROAM_H_

#include <wlan_mlo_mgr_cmn.h>
#include <wlan_mlo_mgr_public_structs.h>
#include <wlan_cm_roam_public_struct.h>
#include <../../core/src/wlan_cm_roam_i.h>

#ifdef WLAN_FEATURE_11BE_MLO

/**
 * mlo_fw_roam_sync_req - Handler for roam sync event handling
 *
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @event: event ptr
 * @event_data_len: event data len
 *
 * This api will be called from target_if layer to mlo mgr,
 * handles mlo roaming and posts roam sync propagation to
 * connection manager state machine.
 *
 * Return: qdf status
 */
QDF_STATUS mlo_fw_roam_sync_req(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, void *event,
				uint32_t event_data_len);

/**
 * mlo_fw_ho_fail_req - Handler for HO fail event handling
 *
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @bssid: bssid mac addr
 *
 * This api will be called from target_if layer to mlo mgr,
 * handles mlo ho fail req and posts to connection manager
 * state machine.
 *
 * Return: void
 */
void
mlo_fw_ho_fail_req(struct wlan_objmgr_psoc *psoc,
		   uint8_t vdev_id, struct qdf_mac_addr bssid);

/**
 * mlo_get_sta_link_mac_addr - get sta link mac addr
 *
 * @vdev_id: vdev id
 * @sync_ind: roam sync ind pointer
 * @link_mac_addr: link mac addr pointer
 *
 * This api will be called to get the link specific mac address.
 *
 * Return: qdf status
 */
QDF_STATUS
mlo_get_sta_link_mac_addr(uint8_t vdev_id,
			  struct roam_offload_synch_ind *sync_ind,
			  struct qdf_mac_addr *link_mac_addr);

/**
 * mlo_roam_get_chan_freq - get channel frequency
 *
 * @vdev_id: vdev id
 * @sync_ind: roam sync ind pointer
 *
 * This api will be called to get the link channel frequency.
 *
 * Return: channel frequency
 */
uint32_t
mlo_roam_get_chan_freq(uint8_t vdev_id,
		       struct roam_offload_synch_ind *sync_ind);

/**
 * mlo_roam_get_link_freq_from_mac_addr - get given link frequency
 * @sync_ind: roam sync ind pointer
 * @link_mac_addr: Link mac address
 *
 * This api will be called to get the link frequency.
 *
 * Return: channel frequency
 */
uint32_t
mlo_roam_get_link_freq_from_mac_addr(struct roam_offload_synch_ind *sync_ind,
				     uint8_t *link_mac_addr);

/**
 * mlo_roam_get_link_id_from_mac_addr - get link id of given link addr
 * @sync_ind: roam sync ind pointer
 * @link_mac_addr: Link mac address
 * @link_id: Buffer to fill link corresponds to link mac address
 *
 * This api will be called to get the link id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_roam_get_link_id_from_mac_addr(struct roam_offload_synch_ind *sync_ind,
				   uint8_t *link_mac_addr, uint32_t *link_id);
/**
 * mlo_roam_get_link_id - get link id
 *
 * @vdev_id: vdev id
 * @sync_ind: roam sync ind pointer
 *
 * This api will be called to get the link id information.
 *
 * Return: link id
 */
uint32_t
mlo_roam_get_link_id(uint8_t vdev_id,
		     struct roam_offload_synch_ind *sync_ind);

/**
 * is_multi_link_roam - check if MLO roaming
 *
 * @sync_ind: roam sync ind pointer
 *
 * This api will be called to check if MLO roaming.
 *
 * Return: true/false
 */
bool
is_multi_link_roam(struct roam_offload_synch_ind *sync_ind);

/**
 * mlo_roam_get_num_of_setup_links - get number of setup links
 * @sync_ind: roam sync ind pointer
 *
 * This api will be called to get number of setup links after roaming
 *
 * Return: true/false
 */
uint8_t
mlo_roam_get_num_of_setup_links(struct roam_offload_synch_ind *sync_ind);

/**
 * mlo_enable_rso - Enable rso on assoc vdev
 *
 * @pdev: pdev pointer
 * @vdev: assoc vdev pointer
 * @rsp: cm connect rsp
 *
 * This api will be called to enable RSO for MLO connection.
 *
 * Return: qdf_status success or fail
 */
QDF_STATUS mlo_enable_rso(struct wlan_objmgr_pdev *pdev,
			  struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *rsp);

/**
 * mlo_roam_copy_partner_info - copy partner link info to connect response
 *
 * @partner_info: Destination buffer to fill partner info from roam sync ind
 * @sync_ind: roam sync ind pointer
 * @skip_vdev_id: Skip to copy the link info corresponds to this vdev_id
 * @fill_all_links: Fill all the links for connect response to userspace
 *
 * This api will be called to copy partner link info to connect response.
 *
 * Return: none
 */
void mlo_roam_copy_partner_info(struct mlo_partner_info *partner_info,
				struct roam_offload_synch_ind *sync_ind,
				uint8_t skip_vdev_id, bool fill_all_links);

/**
 * mlo_roam_init_cu_bpcc() - init cu bpcc per roam sync data
 * @vdev: vdev object
 * @sync_ind: roam sync ind pointer
 *
 * This api will be called to init cu bpcc from connect response.
 *
 * Return: none
 */
void mlo_roam_init_cu_bpcc(struct wlan_objmgr_vdev *vdev,
			   struct roam_offload_synch_ind *sync_ind);

/**
 * mlo_roam_update_connected_links - update connected links bitmap after roaming
 *
 * @vdev: vdev pointer
 * @connect_rsp: connect resp structure pointer
 *
 * This api will be called to copy partner link info to connect response.
 *
 * Return: none
 */
void mlo_roam_update_connected_links(struct wlan_objmgr_vdev *vdev,
				     struct wlan_cm_connect_resp *connect_rsp);

/**
 * mlo_set_single_link_ml_roaming - set single link mlo roaming
 *
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @is_single_link_ml_roaming: boolean flag
 *
 * This api will be called to set single link mlo roaming flag.
 *
 * Return: none
 */
void
mlo_set_single_link_ml_roaming(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id,
			       bool is_single_link_ml_roaming);

/**
 * mlo_get_single_link_ml_roaming - check if single link mlo roaming
 *
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 *
 * This api will be called to check if single link mlo roaming is true or false.
 *
 * Return: boolean value
 */
bool
mlo_get_single_link_ml_roaming(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id);

/**
 * mlo_roam_get_bssid_chan_for_link - get link mac addr and channel info
 *
 * @vdev_id: vdev id
 * @sync_ind: roam sync ind pointer
 * @bssid: link mac addr pointer
 * @chan: link wmi channel pointer
 *
 * This api will be called to get link mac addr and channel info.
 *
 * Return: qdf status
 */
QDF_STATUS
mlo_roam_get_bssid_chan_for_link(uint8_t vdev_id,
				 struct roam_offload_synch_ind *sync_ind,
				 struct qdf_mac_addr *bssid,
				 wmi_channel *chan);

/**
 * mlo_get_link_mac_addr_from_reassoc_rsp - get link mac addr from reassoc rsp
 * @vdev: vdev pointer
 * @link_mac_addr: link mac address
 *
 * This api will be called to get link mac addr from stored reassoc rsp
 * after roaming and vdev id.
 *
 * Return: qdf status
 */
QDF_STATUS
mlo_get_link_mac_addr_from_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
				       struct qdf_mac_addr *link_mac_addr);

/**
 * mlo_roam_copy_reassoc_rsp - Copy cm vdev join rsp
 *
 * @vdev: vdev pointer
 * @reassoc_rsp: cm vdev reassoc rsp pointer
 *
 * This api will be called to copy cm vdev reassoc rsp which will
 * be used to later bring up link vdev/s.
 *
 * Return: qdf_status success or fail
 */
QDF_STATUS
mlo_roam_copy_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *reassoc_rsp);

/**
 * mlo_roam_link_connect_notify - Send connect req
 * on link
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 *
 * This api will be called to send connect req for link vdev.
 *
 * Return: qdf_status success or fail
 */
QDF_STATUS
mlo_roam_link_connect_notify(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id);

/**
 * mlo_roam_is_auth_status_connected - api to check roam auth status
 * on link
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 *
 * This api will be called to check if roam auth status is connected
 *
 * Return: boolean true or false
 */
bool
mlo_roam_is_auth_status_connected(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id);

/**
 * mlo_roam_connect_complete - roam connect complete api
 * @vdev: vdev pointer
 *
 * This api will be called after connect complete for roam 1x case.
 *
 * Return: none
 */
void mlo_roam_connect_complete(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_roam_free_copied_reassoc_rsp - roam free copied reassoc rsp
 * @vdev: vdev pointer
 *
 * This api will be called to free copied reassoc rsp.
 *
 * Return: none
 */
void mlo_roam_free_copied_reassoc_rsp(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * mlo_mgr_roam_update_ap_link_info() - Update AP links information
 * @vdev: Object Manager vdev
 * @src_link_info: Source link setup information
 * @channel: Channel information
 *
 * Update AP link information for each link of AP MLD
 *
 * Return: None
 */
void mlo_mgr_roam_update_ap_link_info(struct wlan_objmgr_vdev *vdev,
				      struct ml_setup_link_param *src_link_info,
				      struct wlan_channel *channel);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * mlo_mgr_num_roam_links() - Get number of roaming links
 * @vdev: VDEV object manager pointer
 *
 * Returns the num of links device roamed via FW roam sync event,
 * for non-MLO VDEV the number of links is one.
 */
uint8_t mlo_mgr_num_roam_links(struct wlan_objmgr_vdev *vdev);
#else
static inline uint8_t mlo_mgr_num_roam_links(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}
#endif

/**
 * mlo_cm_roam_sync_cb - Callback function from CM to MLO mgr
 *
 * @vdev: vdev pointer
 * @event: event ptr
 * @event_data_len: event data len
 *
 * This api will be called from connection manager to mlo
 * manager to start roam sync request on link vdev's.
 *
 * Return: qdf status
 */
QDF_STATUS
mlo_cm_roam_sync_cb(struct wlan_objmgr_vdev *vdev,
		    void *event, uint32_t event_data_len);
#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */

/**
 * wlan_mlo_roam_abort_on_link - Abort roam on link
 *
 * @psoc: psoc pointer
 * @event: Roam sync indication event pointer
 * @vdev_id: vdev id value
 *
 * Abort roaming on all the links except the vdev id passed.
 * Roam abort on vdev id link would be taken care in legacy path.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlo_roam_abort_on_link(struct wlan_objmgr_psoc *psoc,
			    uint8_t *event, uint8_t vdev_id);

/**
 * mlo_check_if_all_links_up - Check if all links are up
 * @vdev: vdev pointer
 *
 * This api will check if all the requested links are  in CM connected
 * state.
 *
 * Return: bool, true: all links of mld connected
 */
bool
mlo_check_if_all_links_up(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_check_if_all_vdev_up - Check if all vdev are up
 * @vdev: vdev pointer
 *
 * This api will check if all the requested vdev are  in up
 * state.
 *
 * Return: bool, true: all assoc/link vdevs of mld in UP state
 */
bool
mlo_check_if_all_vdev_up(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_roam_set_link_id - set link id post roaming
 *
 * @vdev: vdev pointer
 * @sync_ind: roam sync indication pointer
 *
 * This api will be called to set link id post roaming
 *
 * Return: none
 */
void
mlo_roam_set_link_id(struct wlan_objmgr_vdev *vdev,
		     struct roam_offload_synch_ind *sync_ind);

/**
 * mlo_is_roaming_in_progress - check if roaming is in progress
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 *
 * This api will be called to check if roaming in progress on any
 * of the mlo links.
 *
 * Return: boolean (true or false)
 */
bool
mlo_is_roaming_in_progress(struct wlan_objmgr_psoc *psoc,
			   uint8_t vdev_id);

/**
 * mlo_add_all_link_probe_rsp_to_scan_db - Extract and add all ML link probe
 * rsps to scan db
 * @psoc: psoc pointer
 * @rcvd_frame: Received frame from firmware
 *
 * This api will be called to generate ML probe responses corresponds to each
 * link from the received probe response and add them to scan db
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlo_add_all_link_probe_rsp_to_scan_db(struct wlan_objmgr_psoc *psoc,
			      struct roam_scan_candidate_frame *rcvd_frame);

/**
 * mlo_is_enable_roaming_on_connected_sta_allowed() - whether connected STA is
 * allowed to enable roaming if link vdev disconnects
 * @vdev: vdev object
 *
 * Return: true if connected STA is allowed to enable roaming, false otherwise.
 */
bool
mlo_is_enable_roaming_on_connected_sta_allowed(struct wlan_objmgr_vdev *vdev);

/**
 * mlo_check_is_given_vdevs_on_same_mld() - check if the 2 given vdev's are on
 * same MLD
 * @psoc: PSOC object
 * @vdev_id_1: Current connected station vdev id on which roaming is to be
 * enabled
 * @vdev_id_2: vdev id on which disconnection is happening
 *
 * Return: true if both vdev ids are on same MLD, false otherwise.
 */
bool
mlo_check_is_given_vdevs_on_same_mld(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id_1, uint8_t vdev_id_2);
#else /* WLAN_FEATURE_11BE_MLO */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static inline
QDF_STATUS mlo_fw_roam_sync_req(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, void *event,
				uint32_t event_data_len)
{
	return cm_fw_roam_sync_req(psoc, vdev_id, event, event_data_len);
}
#endif
static inline QDF_STATUS
mlo_get_sta_link_mac_addr(uint8_t vdev_id,
			  struct roam_offload_synch_ind *sync_ind,
			  struct qdf_mac_addr *link_mac_addr)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline uint32_t
mlo_roam_get_chan_freq(uint8_t vdev_id,
		       struct roam_offload_synch_ind *sync_ind)
{
	return 0;
}

static inline uint32_t
mlo_roam_get_link_id(uint8_t vdev_id,
		     struct roam_offload_synch_ind *sync_ind)
{
	return 0;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * mlo_cm_roam_sync_cb() - MLO callback to handle roam synch event
 * for MLO vdev
 * @vdev: Pointer to objmgr vdev
 * @event: Pointer to event
 * @event_data_len: event data length
 */
QDF_STATUS mlo_cm_roam_sync_cb(struct wlan_objmgr_vdev *vdev,
			       void *event, uint32_t event_data_len);
#else
static inline QDF_STATUS
mlo_cm_roam_sync_cb(struct wlan_objmgr_vdev *vdev,
		    void *event, uint32_t event_data_len)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static inline bool
is_multi_link_roam(struct roam_offload_synch_ind *sync_ind)
{
	return false;
}

static inline uint8_t
mlo_roam_get_num_of_setup_links(struct roam_offload_synch_ind *sync_ind)
{
	return 0;
}

static inline
QDF_STATUS mlo_enable_rso(struct wlan_objmgr_pdev *pdev,
			  struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *rsp)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
mlo_roam_copy_partner_info(struct mlo_partner_info *partner_info,
			   struct roam_offload_synch_ind *sync_ind,
			   uint8_t skip_vdev_id, bool fill_all_links)
{}

static inline
void mlo_roam_init_cu_bpcc(struct wlan_objmgr_vdev *vdev,
			   struct roam_offload_synch_ind *sync_ind)
{}

static inline void
mlo_roam_update_connected_links(struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_connect_resp *connect_rsp)
{}

static inline QDF_STATUS
wlan_mlo_roam_abort_on_link(struct wlan_objmgr_psoc *psoc,
			    uint8_t *event, uint8_t vdev_id)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
mlo_set_single_link_ml_roaming(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id,
			       bool is_single_link_ml_roaming)
{}

static inline bool
mlo_get_single_link_ml_roaming(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id)
{
	return false;
}

static inline QDF_STATUS
mlo_roam_get_bssid_chan_for_link(uint8_t vdev_id,
				 struct roam_offload_synch_ind *sync_ind,
				 struct qdf_mac_addr *bssid,
				 wmi_channel *chan)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool
mlo_check_if_all_links_up(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline bool
mlo_check_if_all_vdev_up(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
mlo_roam_set_link_id(struct wlan_objmgr_vdev *vdev,
		     struct roam_offload_synch_ind *sync_ind)
{}

static inline QDF_STATUS
mlo_roam_copy_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_connect_resp *reassoc_rsp)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
mlo_roam_link_connect_notify(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool
mlo_roam_is_auth_status_connected(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id)
{
	return false;
}

static inline void
mlo_roam_connect_complete(struct wlan_objmgr_vdev *vdev)
{}

static inline void
mlo_roam_free_copied_reassoc_rsp(struct wlan_objmgr_vdev *vdev)
{}

static inline QDF_STATUS
mlo_get_link_mac_addr_from_reassoc_rsp(struct wlan_objmgr_vdev *vdev,
				       struct qdf_mac_addr *link_mac_addr)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
void mlo_mgr_roam_update_ap_link_info(struct wlan_objmgr_vdev *vdev,
				      struct ml_setup_link_param *src_info,
				      struct wlan_channel *channel)
{}

static inline uint8_t mlo_mgr_num_roam_links(struct wlan_objmgr_vdev *vdev)
{
	return 1;
}

static inline uint32_t
mlo_roam_get_link_freq_from_mac_addr(struct roam_offload_synch_ind *sync_ind,
				     uint8_t *link_mac_addr)
{
	if (sync_ind)
		return sync_ind->chan_freq;

	return 0;
}

static inline bool
mlo_is_roaming_in_progress(struct wlan_objmgr_psoc *psoc,
			   uint8_t vdev_id)
{
	return false;
}

static inline bool
mlo_is_enable_roaming_on_connected_sta_allowed(struct wlan_objmgr_vdev *vdev)
{
	return true;
}

static inline bool
mlo_check_is_given_vdevs_on_same_mld(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id_1, uint8_t vdev_id_2)
{
	return false;
}

#endif /* WLAN_FEATURE_11BE_MLO */
#endif
