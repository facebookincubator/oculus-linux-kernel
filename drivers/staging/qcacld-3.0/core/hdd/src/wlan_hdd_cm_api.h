/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_cm_api.h
 *
 * WLAN host device driver connect/disconnect functions declaration
 */

#ifndef __WLAN_HDD_CM_API_H
#define __WLAN_HDD_CM_API_H

#include <net/cfg80211.h>
#include "wlan_cm_public_struct.h"
#include "osif_cm_util.h"
#include "wlan_cm_roam_ucfg_api.h"

/**
 * wlan_hdd_cm_connect() - cfg80211 connect api
 * @wiphy: Pointer to wiphy
 * @ndev: Pointer to network device
 * @req: Pointer to cfg80211 connect request
 *
 * This function is used to issue connect request to connection manager
 *
 * Context: Any context.
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cm_connect(struct wiphy *wiphy,
			struct net_device *ndev,
			struct cfg80211_connect_params *req);

/**
 * wlan_hdd_cm_issue_disconnect() - initiate disconnect from osif
 * @link_info: Link info pointer in HDD adapter
 * @reason: Disconnect reason code
 * @sync: true if wait for disconnect to complete is required. for the
 *        supplicant initiated disconnect or during vdev delete/change interface
 *        sync should be true.
 *
 * This function is used to issue disconnect request to connection manager
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_hdd_cm_issue_disconnect(struct wlan_hdd_link_info *link_info,
			     enum wlan_reason_code reason, bool sync);

/**
 * wlan_hdd_cm_disconnect() - cfg80211 disconnect api
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @reason: Disconnect reason code
 *
 * This function is used to issue disconnect request to connection manager
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cm_disconnect(struct wiphy *wiphy,
			   struct net_device *dev, u16 reason);

QDF_STATUS hdd_cm_disconnect_complete(struct wlan_objmgr_vdev *vdev,
				      struct wlan_cm_discon_rsp *rsp,
				      enum osif_cb_type type);

QDF_STATUS hdd_cm_netif_queue_control(struct wlan_objmgr_vdev *vdev,
				      enum netif_action_type action,
				      enum netif_reason_type reason);

QDF_STATUS hdd_cm_connect_complete(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_connect_resp *rsp,
				   enum osif_cb_type type);

/**
 * hdd_cm_send_vdev_keys() - send vdev keys
 * @vdev: Pointer to vdev
 * @key_index: key index value
 * @pairwise: pairwise boolean value
 * @cipher_type: cipher type enum value
 *
 * This function is used to send vdev keys
 *
 * Context: Any context.
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_send_vdev_keys(struct wlan_objmgr_vdev *vdev,
				 u8 key_index, bool pairwise,
				 enum wlan_crypto_cipher_type cipher_type);

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
/**
 * hdd_cm_get_vendor_handoff_params() - to get vendor handoff params from fw
 * @psoc: Pointer to psoc object
 * @vendor_handoff_context: Pointer to vendor handoff event rsp
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
hdd_cm_get_vendor_handoff_params(struct wlan_objmgr_psoc *psoc,
				 void *vendor_handoff_context);

/**
 * hdd_cm_get_handoff_param() - send get vendor handoff param request to fw
 * @psoc: psoc common object
 * @vdev_id: vdev id
 * @param_id: param id from enum vendor_control_roam_param
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_get_handoff_param(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    enum vendor_control_roam_param param_id);
#endif

/**
 * hdd_cm_napi_serialize_control() - NAPI serialize hdd cb
 * @action: serialize or de-serialize NAPI activities
 *
 * This function is for napi serialize
 *
 * Return: qdf status
 */
QDF_STATUS hdd_cm_napi_serialize_control(bool action);

#ifdef WLAN_BOOST_CPU_FREQ_IN_ROAM
/**
 * hdd_cm_perfd_set_cpufreq() - API to set CPU min freq
 * @action: set or reset the CPU freq
 *
 * This function sets/resets the CPU min frequency
 * by sending netlink msg to cnss-daemon, which will
 * communicate to perf daemon to set/reset CPU freq.
 *
 * Return: qdf status
 */
QDF_STATUS hdd_cm_perfd_set_cpufreq(bool action);
#else
static inline
QDF_STATUS hdd_cm_perfd_set_cpufreq(bool action)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_FILS_SK
/**
 * hdd_cm_save_gtk() - save gtk api
 * @vdev: Pointer to vdev
 * @rsp: Pointer to connect rsp
 *
 * This function is used to save gtk in legacy mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_save_gtk(struct wlan_objmgr_vdev *vdev,
			   struct wlan_cm_connect_resp *rsp);

/**
 * hdd_cm_set_hlp_data() - api to set hlp data for dhcp
 * @dev: pointer to net device
 * @vdev: Pointer to vdev
 * @rsp: Pointer to connect rsp
 *
 * This function is used to set hlp data for dhcp in legacy mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_set_hlp_data(struct net_device *dev,
			       struct wlan_objmgr_vdev *vdev,
			       struct wlan_cm_connect_resp *rsp);
#endif

#ifdef WLAN_FEATURE_PREAUTH_ENABLE
/**
 * hdd_cm_ft_preauth_complete() - send fast transition event
 * @vdev: Pointer to vdev
 * @rsp: Pointer to preauth rsp
 *
 * This function is used to send fast transition event in legacy mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_ft_preauth_complete(struct wlan_objmgr_vdev *vdev,
				      struct wlan_preauth_rsp *rsp);

#ifdef FEATURE_WLAN_ESE
/**
 * hdd_cm_cckm_preauth_complete() - send cckm preauth indication to
 * the supplicant via wireless custom event
 * @vdev: Pointer to vdev
 * @rsp: Pointer to preauth rsp
 *
 * This function is used to send cckm preauth indication to
 * the supplicant via wireless custom event in legacy mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_cckm_preauth_complete(struct wlan_objmgr_vdev *vdev,
					struct wlan_preauth_rsp *rsp);
#endif
#endif

#ifdef WLAN_FEATURE_MSCS
/**
 * reset_mscs_params() - Reset mscs parameters
 * @link_info: pointer to link_info struct in adapter
 *
 * Reset mscs parameters whils disconnection
 *
 * Return: None
 */
void reset_mscs_params(struct wlan_hdd_link_info *link_info);
#else
static inline
void reset_mscs_params(struct wlan_hdd_link_info *link_info)
{
	return;
}
#endif

/**
 * hdd_handle_disassociation_event() - Handle disassociation event
 * @link_info: Link info pointer in HDD adapter
 * @peer_macaddr: Pointer to peer mac address
 *
 * Return: None
 */
void hdd_handle_disassociation_event(struct wlan_hdd_link_info *link_info,
				     struct qdf_mac_addr *peer_macaddr);

/**
 * __hdd_cm_disconnect_handler_pre_user_update() - Handle disconnect indication
 * before updating to user space
 * @link_info: Link info pointer in HDD adapter
 *
 * Return: None
 */
void
__hdd_cm_disconnect_handler_pre_user_update(struct wlan_hdd_link_info *link_info);

/**
 * __hdd_cm_disconnect_handler_post_user_update() - Handle disconnect indication
 * after updating to user space
 * @link_info: Link info pointer in HDD adapter
 * @vdev: vdev ptr
 * @source: Disconnect source
 *
 * Return: None
 */
void
__hdd_cm_disconnect_handler_post_user_update(struct wlan_hdd_link_info *link_info,
					     struct wlan_objmgr_vdev *vdev,
					     enum wlan_cm_source source);

/**
 * hdd_cm_set_peer_authenticate() - set peer as authenticated
 * @link_info: Link info pointer in HDD adapter
 * @bssid: bssid of the connection
 * @is_auth_required: is upper layer authenticatoin required
 *
 * Return: QDF_STATUS enumeration
 */
void hdd_cm_set_peer_authenticate(struct wlan_hdd_link_info *link_info,
				  struct qdf_mac_addr *bssid,
				  bool is_auth_required);

/**
 * hdd_cm_update_rssi_snr_by_bssid() - update rsi and snr into adapter
 * @link_info: Link info pointer in HDD adapter
 *
 * Return: None
 */
void hdd_cm_update_rssi_snr_by_bssid(struct wlan_hdd_link_info *link_info);

/**
 *  hdd_cm_handle_assoc_event() - Send disassociation indication to oem
 * app
 * @vdev: Pointer to adapter
 * @peer_mac: Pointer to peer mac address
 *
 * Return: None
 */
void hdd_cm_handle_assoc_event(struct wlan_objmgr_vdev *vdev,
			       uint8_t *peer_mac);

/**
 * hdd_cm_netif_queue_enable() - Enable the network queue for a
 *			      particular adapter.
 * @adapter: pointer to the adapter structure
 *
 * This function schedules a work to update the netdev features
 * and enable the network queue if the feature "disable checksum/tso
 * for legacy connections" is enabled via INI. If not, it will
 * retain the existing behavior by just enabling the network queues.
 *
 * Returns: none
 */
void hdd_cm_netif_queue_enable(struct hdd_adapter *adapter);

/**
 * hdd_cm_clear_pmf_stats() - Clear pmf stats
 * @adapter: pointer to the adapter structure
 *
 * Returns: None
 */
void hdd_cm_clear_pmf_stats(struct hdd_adapter *adapter);

/**
 * hdd_cm_save_connect_status() - Save connect status
 * @link_info: Link info pointer in HDD adapter
 * @reason_code: IEE80211 wlan status code
 *
 * Returns: None
 */
void hdd_cm_save_connect_status(struct wlan_hdd_link_info *link_info,
				uint32_t reason_code);

/**
 * hdd_cm_is_vdev_associated() - Checks if vdev is associated or not
 * @link_info: pointer to the link info structure
 *
 * Returns: True if vdev is associated else false
 */
bool hdd_cm_is_vdev_associated(struct wlan_hdd_link_info *link_info);

/**
 * hdd_cm_is_vdev_connected() - Checks if vdev is connected or not
 * @link_info: pointer to the link_info structure
 *
 * Returns: True if vdev is connected else false
 */
bool hdd_cm_is_vdev_connected(struct wlan_hdd_link_info *link_info);

/**
 * hdd_cm_is_connecting() - Function to check connection in progress
 * @link_info: pointer to the link_info structure
 *
 * Return: true if connecting, false otherwise
 */
bool hdd_cm_is_connecting(struct wlan_hdd_link_info *link_info);

/**
 * hdd_cm_is_disconnected() - Function to check if vdev is disconnected or not
 * @link_info: pointer to the link_info structure
 *
 * Return: true if disconnected, false otherwise
 */
bool hdd_cm_is_disconnected(struct wlan_hdd_link_info *link_info);

/**
 * hdd_cm_is_vdev_roaming() - Function to check roaming in progress
 * @link_info: pointer to the link_info structure
 *
 * Return: true if roaming, false otherwise
 */
bool hdd_cm_is_vdev_roaming(struct wlan_hdd_link_info *link_info);

/**
 * hdd_cm_get_scan_ie_params() - to get scan ie params
 * @vdev: Pointer to vdev object
 * @scan_ie: pointer to scan ie element struct
 * @dot11mode_filter: Pointer to dot11mode_filter enum
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
hdd_cm_get_scan_ie_params(struct wlan_objmgr_vdev *vdev,
			  struct element_info *scan_ie,
			  enum dot11_mode_filter *dot11mode_filter);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * hdd_cm_save_connected_links_info() - Update connection info to station
 * context.
 * @self_mac: Self MAC address.
 * @bssid: BSSID of link.
 * @link_id: IEEE link id.
 *
 * It searches for link info pointer matching with @self_mac and updates
 * the BSSID and link ID fields in station context's connection info.
 * This will help to retrieve the link information using IEEE link ID or
 * BSSID thereafter.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_save_connected_links_info(struct qdf_mac_addr *self_mac,
					    struct qdf_mac_addr *bssid,
					    int32_t link_id);

/**
 * hdd_cm_set_ieee_link_id() - Set IEEE link ID in station context conn info.
 * @link_info: Link info pointer in HDD adapter
 * @link_id: IEEE link ID
 *
 * Sets IEEE link ID in connection info of @link_info's station context.
 *
 * Return: void
 */
void
hdd_cm_set_ieee_link_id(struct wlan_hdd_link_info *link_info, uint8_t link_id);

/**
 * hdd_cm_clear_ieee_link_id() - Clear IEEE link ID in station context
 * conn info.
 * @link_info: Link info pointer in HDD adapter
 *
 * Clear IEEE link ID in connection info of @link_info's station context.
 *
 * Return: void
 */
void hdd_cm_clear_ieee_link_id(struct wlan_hdd_link_info *link_info);
#else
static inline void
hdd_cm_set_ieee_link_id(struct wlan_hdd_link_info *link_info, uint8_t link_id)
{
}

static inline void
hdd_cm_clear_ieee_link_id(struct wlan_hdd_link_info *link_info)
{
}

static inline
QDF_STATUS hdd_cm_save_connected_links_info(struct qdf_mac_addr *self_mac,
					    struct qdf_mac_addr *bssid,
					    int32_t link_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_11BE_MLO */
#endif /* __WLAN_HDD_CM_API_H */
