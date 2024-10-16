/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: os_if_son.h
 *
 * WLAN Host Device Driver file for son (Self Organizing Network)
 * support.
 *
 */
#ifndef _OS_IF_SON_H_
#define _OS_IF_SON_H_

#include <qdf_types.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_reg_ucfg_api.h>
#include <ieee80211_external.h>

#define INVALID_WIDTH 0xFF

/**
 * struct son_callbacks - struct containing callback to non-converged driver
 * @os_if_is_acs_in_progress: whether acs is in progress or not
 * @os_if_set_chan_ext_offset: set chan extend offset
 * @os_if_get_chan_ext_offset: get chan extend offset
 * @os_if_set_bandwidth: set band width
 * @os_if_get_bandwidth: get band width
 * @os_if_set_chan: set chan
 * @os_if_get_sta_count: get station count
 * @os_if_set_country_code: set country code
 * @os_if_set_candidate_freq: set freq to switch after radar detection
 * @os_if_get_candidate_freq: get freq to switch after radar detection
 * @os_if_set_phymode: set phy mode
 * @os_if_get_phymode: get phy mode
 * @os_if_get_rx_nss: Gets number of RX spatial streams
 * @os_if_set_acl_policy: set acl policy
 * @os_if_get_acl_policy: get acl policy
 * @os_if_add_acl_mac: add mac to acl
 * @os_if_del_acl_mac: del mac from acl
 * @os_if_kickout_mac: kickout sta with given mac
 * @os_if_set_chwidth: set chan width
 * @os_if_get_chwidth: get chan width
 * @os_if_get_sta_list: get sta list
 * @os_if_get_sta_space: get sta space
 * @os_if_deauth_sta: Deauths the target peer
 * @os_if_modify_acl: Add/Del target peer in ACL
 * @os_if_get_vdev_by_netdev: Get vdev from net device
 * @os_if_trigger_objmgr_object_creation: Trigger objmgr object creation
 * @os_if_trigger_objmgr_object_deletion: Trigger objmgr object deletion
 * @os_if_start_acs: Trigger ACS
 * @os_if_set_acs_channels: Set channel list for ACS
 * @os_if_get_acs_report: Gets the ACS report
 * @os_if_get_node_info: Gets the datarate info for node
 * @os_if_get_peer_capability: Gets peer capability
 * @os_if_get_peer_max_mcs_idx: Gets peer max MCS index
 * @os_if_get_sta_stats: Get sta stats
 */
struct son_callbacks {
	uint32_t (*os_if_is_acs_in_progress)(struct wlan_objmgr_vdev *vdev);
	int (*os_if_set_chan_ext_offset)(
				struct wlan_objmgr_vdev *vdev,
				enum sec20_chan_offset son_chan_ext_offset);
	enum sec20_chan_offset (*os_if_get_chan_ext_offset)(
				struct wlan_objmgr_vdev *vdev);
	int (*os_if_set_bandwidth)(struct wlan_objmgr_vdev *vdev,
				   uint32_t son_bandwidth);
	uint32_t (*os_if_get_bandwidth)(struct wlan_objmgr_vdev *vdev);
	int (*os_if_set_chan)(struct wlan_objmgr_vdev *vdev, int chan,
			      enum wlan_band_id son_band);
	uint32_t (*os_if_get_sta_count)(struct wlan_objmgr_vdev *vdev);
	int (*os_if_set_country_code)(struct wlan_objmgr_vdev *vdev,
				      char *country_code);
	int (*os_if_set_candidate_freq)(struct wlan_objmgr_vdev *vdev,
					qdf_freq_t freq);
	qdf_freq_t (*os_if_get_candidate_freq)(struct wlan_objmgr_vdev *vdev);
	int (*os_if_set_phymode)(struct wlan_objmgr_vdev *vdev,
				 enum ieee80211_phymode mode);
	enum ieee80211_phymode (*os_if_get_phymode)(
					struct wlan_objmgr_vdev *vdev);
	uint8_t (*os_if_get_rx_nss)(struct wlan_objmgr_vdev *vdev);
	QDF_STATUS (*os_if_set_acl_policy)(struct wlan_objmgr_vdev *vdev,
					   ieee80211_acl_cmd son_acl_policy);
	ieee80211_acl_cmd (*os_if_get_acl_policy)(
						struct wlan_objmgr_vdev *vdev);
	int (*os_if_add_acl_mac)(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *acl_mac);
	int (*os_if_del_acl_mac)(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *acl_mac);
	int (*os_if_kickout_mac)(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *acl_mac);
	int (*os_if_set_chwidth)(struct wlan_objmgr_vdev *vdev,
				 enum ieee80211_cwm_width son_chwidth);
	enum ieee80211_cwm_width (*os_if_get_chwidth)(
				struct wlan_objmgr_vdev *vdev);
	void (*os_if_get_sta_list)(struct wlan_objmgr_vdev *vdev,
				   struct ieee80211req_sta_info *req,
				   uint32_t *space);
	uint32_t (*os_if_get_sta_space)(struct wlan_objmgr_vdev *vdev);
	void (*os_if_deauth_sta)(struct wlan_objmgr_vdev *vdev,
				 uint8_t *peer_mac,
				 bool ignore_frame);
	void (*os_if_modify_acl)(struct wlan_objmgr_vdev *vdev,
				 uint8_t *peer_mac,
				 bool allow_auth);
	struct wlan_objmgr_vdev *(*os_if_get_vdev_by_netdev)
				(struct net_device *dev);
	QDF_STATUS (*os_if_trigger_objmgr_object_creation)
				(enum wlan_umac_comp_id id);
	QDF_STATUS (*os_if_trigger_objmgr_object_deletion)
				(enum wlan_umac_comp_id id);
	int (*os_if_start_acs)(struct wlan_objmgr_vdev *vdev, uint8_t enable);
	int (*os_if_set_acs_channels)(struct wlan_objmgr_vdev *vdev,
				      struct ieee80211req_athdbg *req);
	int (*os_if_get_acs_report)(struct wlan_objmgr_vdev *vdev,
				    struct ieee80211_acs_dbg *acs_r);
	QDF_STATUS (*os_if_get_node_info)(struct wlan_objmgr_vdev *vdev,
					  uint8_t *mac_addr,
					  wlan_node_info *nodeinfo);
	QDF_STATUS (*os_if_get_peer_capability)(struct wlan_objmgr_vdev *vdev,
						struct wlan_objmgr_peer *peer,
						wlan_peer_cap *cap);
	uint32_t (*os_if_get_peer_max_mcs_idx)(struct wlan_objmgr_vdev *vdev,
					       struct wlan_objmgr_peer *peer);
	int (*os_if_get_sta_stats)(struct wlan_objmgr_vdev *vdev,
				   uint8_t *mac_addr,
				   struct ieee80211_nodestats *stats);
};

/**
 * enum os_if_son_vendor_cmd_type - Enum to specify get/set command
 * @OS_IF_SON_VENDOR_GET_CMD: Get type command called from wificonfiguration
 *                            vendor command handler
 * @OS_IF_SON_VENDOR_SET_CMD: Set type command called from wificonfiguration
 *                            vendor command handler
 * @OS_IF_SON_VENDOR_MAX_CMD: Max cmd type
 */
enum os_if_son_vendor_cmd_type {
	OS_IF_SON_VENDOR_GET_CMD,
	OS_IF_SON_VENDOR_SET_CMD,
	OS_IF_SON_VENDOR_MAX_CMD,
};

/**
 * struct os_if_son_rx_ops - Contains cb for os_if rx ops used by SON
 * @parse_generic_nl_cmd: Callback for parsing generic nl vendor commands
 */
struct os_if_son_rx_ops {
	int (*parse_generic_nl_cmd)(struct wiphy *wiphy,
				    struct wireless_dev *wdev, void *params,
				    enum os_if_son_vendor_cmd_type type);
};

/**
 * struct wlan_os_if_son_ops - Contains cb for os_if txrx ops used by SON
 * @son_osif_rx_ops: structure to contain rx ops
 */
struct wlan_os_if_son_ops {
	struct os_if_son_rx_ops son_osif_rx_ops;
};

/**
 * wlan_os_if_son_ops_register_cb() - Set son os_if ops cb
 * @handler: son os_if ops cb table
 *
 * Return: void
 */
void
wlan_os_if_son_ops_register_cb(void (*handler)(struct wlan_os_if_son_ops *));

/**
 * os_if_son_register_osif_ops() - Register son os_if ops with os_if
 *
 * Return: void
 */
void os_if_son_register_osif_ops(void);

/**
 * os_if_son_register_lmac_if_ops() - Register son lmac_if rx_ops with lmac
 * @psoc: objmrg psoc handle
 *
 * Register son lmac_if rx_ops with lmac to be called by SON DLKM
 *
 * Return: void
 */
void os_if_son_register_lmac_if_ops(struct wlan_objmgr_psoc *psoc);

/**
 * os_if_son_register_hdd_callbacks() - register son hdd callback
 * @psoc: psoc
 * @cb_obj: pointer to callback
 *
 * Return: void
 */
void os_if_son_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				      struct son_callbacks *cb_obj);

/**
 * os_if_son_get_freq() - get freq
 * @vdev: vdev
 *
 * Return: freq of given vdev
 */
qdf_freq_t os_if_son_get_freq(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_is_acs_in_progress() - whether ACS in progress or not
 * @vdev: vdev
 *
 * Return: true if ACS is in progress
 */
uint32_t os_if_son_is_acs_in_progress(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_is_cac_in_progress() - whether CAC is in progress or not
 * @vdev: vdev
 *
 * Return: true if CAC is in progress
 */
uint32_t os_if_son_is_cac_in_progress(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_set_chan_ext_offset() - set chan extend offset
 * @vdev: vdev
 * @son_chan_ext_offset: son chan extend offset
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_chan_ext_offset(struct wlan_objmgr_vdev *vdev,
				  enum sec20_chan_offset son_chan_ext_offset);

/**
 * os_if_son_get_chan_ext_offset() - get chan extend offset
 * @vdev: vdev
 *
 * Return: enum sec20_chan_offset
 */
enum sec20_chan_offset os_if_son_get_chan_ext_offset(
					struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_set_bandwidth() - set band width
 * @vdev: vdev
 * @son_bandwidth: band width
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_bandwidth(struct wlan_objmgr_vdev *vdev,
			    uint32_t son_bandwidth);

/**
 * os_if_son_get_bandwidth() - get band width
 * @vdev: vdev
 *
 * Return: band width
 */
uint32_t os_if_son_get_bandwidth(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_get_band_info() - get band info
 * @vdev: vdev
 *
 * Return: band info
 */
uint32_t os_if_son_get_band_info(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_get_chan_list() - get a list of chan information
 * @vdev: vdev
 * @ic_chans: chan information array to get
 * @chan_info: pointer to ieee80211_channel_info to get
 * @ic_nchans: number of chan information it gets
 * @flag_160: flag indicating the API to fill the center frequencies of 160MHz.
 * @flag_6ghz: flag indicating the API to include 6 GHz or not
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_get_chan_list(struct wlan_objmgr_vdev *vdev,
			    struct ieee80211_ath_channel *ic_chans,
			    struct ieee80211_channel_info *chan_info,
			    uint8_t *ic_nchans, bool flag_160, bool flag_6ghz);

/**
 * os_if_son_get_sta_count() - get connected STA count
 * @vdev: vdev
 *
 * Return: connected STA count
 */
uint32_t os_if_son_get_sta_count(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_get_bssid() - get bssid of given vdev
 * @vdev: vdev
 * @bssid: pointer to BSSID
 *
 * Return: 0 if BSSID is gotten successfully
 */
int os_if_son_get_bssid(struct wlan_objmgr_vdev *vdev,
			uint8_t bssid[QDF_MAC_ADDR_SIZE]);

/**
 * os_if_son_get_ssid() - get ssid of given vdev
 * @vdev: vdev
 * @ssid: pointer to SSID
 * @ssid_len: ssid length
 *
 * Return: 0 if SSID is gotten successfully
 */
int os_if_son_get_ssid(struct wlan_objmgr_vdev *vdev,
		       char ssid[WLAN_SSID_MAX_LEN + 1],
		       uint8_t *ssid_len);

/**
 * os_if_son_set_chan() - set chan
 * @vdev: vdev
 * @chan: given chan
 * @son_band: given band
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_chan(struct wlan_objmgr_vdev *vdev,
		       int chan, enum wlan_band_id son_band);

/**
 * os_if_son_set_cac_timeout() - set cac timeout
 * @vdev: vdev
 * @cac_timeout: cac timeount to set
 *
 * Return: 0 if cac time out is set successfully
 */
int os_if_son_set_cac_timeout(struct wlan_objmgr_vdev *vdev,
			      int cac_timeout);

/**
 * os_if_son_get_cac_timeout() - get cac timeout
 * @vdev: vdev
 * @cac_timeout: cac timeout to get
 *
 * Return 0 if cac time out is get successfully
 */
int os_if_son_get_cac_timeout(struct wlan_objmgr_vdev *vdev,
			      int *cac_timeout);

/**
 * os_if_son_set_country_code() - set country code
 * @vdev: vdev
 * @country_code: country code to set
 *
 * Return: 0 if country code is set successfully
 */
int os_if_son_set_country_code(struct wlan_objmgr_vdev *vdev,
			       char *country_code);

/**
 * os_if_son_get_country_code() - get country code
 * @vdev: vdev
 * @country_code: country code to get
 *
 * Return: 0 if country code is get successfully
 */
int os_if_son_get_country_code(struct wlan_objmgr_vdev *vdev,
			       char *country_code);

/**
 * os_if_son_set_candidate_freq() - set freq to switch after radar detection
 * @vdev: vdev
 * @freq: freq to switch
 *
 * Return: 0 if candidate freq is set successfully
 */
int os_if_son_set_candidate_freq(struct wlan_objmgr_vdev *vdev,
				 qdf_freq_t freq);

/**
 * os_if_son_get_candidate_freq() - get freq to switch after radar detection
 * @vdev: vdev
 *
 * Return: candidate freq to switch after radar detection
 */
qdf_freq_t os_if_son_get_candidate_freq(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_get_phymode() - get phy mode
 * @vdev: vdev
 *
 * Return: enum ieee80211_phymode
 */
enum ieee80211_phymode os_if_son_get_phymode(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_set_phymode() - set phy mode
 * @vdev: vdev
 * @mode: son phy mode to set
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_phymode(struct wlan_objmgr_vdev *vdev,
			  enum ieee80211_phymode mode);

/**
 * os_if_son_get_phy_stats() - get phy stats
 * @vdev: vdev
 * @phy_stats: phy stats
 *
 * Return: void
 */
void os_if_son_get_phy_stats(struct wlan_objmgr_vdev *vdev,
			     struct ol_ath_radiostats *phy_stats);

/**
 * os_if_son_cbs_init() - cbs init
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_cbs_init(void);

/**
 * os_if_son_cbs_deinit() - cbs deinit
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_cbs_deinit(void);

/**
 * os_if_son_set_cbs() - enable cbs or disable
 * @vdev: vdev
 * @enable: true or false
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_cbs(struct wlan_objmgr_vdev *vdev,
		      bool enable);

/**
 * os_if_son_set_cbs_wait_time() - set cbs wait time
 * @vdev: vdev
 * @val: value
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_cbs_wait_time(struct wlan_objmgr_vdev *vdev,
				uint32_t val);

/**
 * os_if_son_set_cbs_dwell_split_time() - set cbs dwell split time
 * @vdev: vdev
 * @val: value
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_cbs_dwell_split_time(struct wlan_objmgr_vdev *vdev,
				       uint32_t val);

/**
 * os_if_son_get_chan_util() - get chan utilization
 * @vdev: vdev
 *
 * Return: chan utilization (0 - 100)
 */
uint8_t os_if_son_get_chan_util(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_pdev_ops() - Handles PDEV specific SON commands
 * @pdev: pdev
 * @type: SON command to handle
 * @data: Input Data
 * @ret: Output Data
 *
 * Return: QDF_SUCCCESS_SUCCESS in case of success
 */
QDF_STATUS os_if_son_pdev_ops(struct wlan_objmgr_pdev *pdev,
			      enum wlan_mlme_pdev_param type,
			      void *data, void *ret);

/**
 * os_if_son_vdev_ops() - Handles VDEV specific SON commands
 * @vdev: vdev
 * @type: SON command to handle
 * @data: Input Data
 * @ret: Output Data
 *
 * Return: QDF_SUCCCESS_SUCCESS in case of success
 */
QDF_STATUS os_if_son_vdev_ops(struct wlan_objmgr_vdev *vdev,
			      enum wlan_mlme_vdev_param type,
			      void *data, void *ret);

/**
 * os_if_son_peer_ops() - Handles PEER specific SON commands
 * @peer: peer
 * @type: SON command to handle
 * @data: Input Data. Pointer to wlan_mlme_peer_data
 * @ret: Output Data. Pointer to wlan_mlme_peer_data
 *
 * Return: QDF_SUCCCESS_SUCCESS in case of success
 */
QDF_STATUS os_if_son_peer_ops(struct wlan_objmgr_peer *peer,
			      enum wlan_mlme_peer_param type,
			      union wlan_mlme_peer_data *data,
			      union wlan_mlme_peer_data *ret);

/**
 * os_if_son_scan_db_iterate() - get country code
 * @pdev: pdev
 * @handler: scan_iterator
 * @arg: argument to be passed to handler
 *
 * Return: QDF_SUCCCESS_SUCCESS in case of success
 */
QDF_STATUS os_if_son_scan_db_iterate(struct wlan_objmgr_pdev *pdev,
				     scan_iterator_func handler, void *arg);

/**
 * os_if_son_acl_is_probe_wh_set() - Withheld probes for given mac_addr,
 *				     not supported
 * @vdev: vdev
 * @mac_addr: 6-Byte MAC address
 * @probe_rssi: Probe Request RSSI
 *
 * Return: true / false
 */
bool os_if_son_acl_is_probe_wh_set(struct wlan_objmgr_vdev *vdev,
				   const uint8_t *mac_addr,
				   uint8_t probe_rssi);

/**
 * os_if_son_get_rx_streams() - Gets number of RX spatial streams
 * @vdev: target vdev
 *
 * Return: number of spatial stream
 */
uint8_t os_if_son_get_rx_streams(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_cfg80211_reply() - replies to cfg80211
 * @sk_buf: sk_buff to uper layer
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS os_if_son_cfg80211_reply(qdf_nbuf_t sk_buf);

/**
 * os_if_son_vdev_is_wds() - checks if wds capability is supported or not
 * @vdev: Pointer to vdev
 *
 * Return: true if wds is supported
 */
bool os_if_son_vdev_is_wds(struct wlan_objmgr_vdev *vdev);

/*
 * os_if_son_set_acl_policy() - set acl policy
 * @vdev: vdev
 * @son_acl_policy: son acl policy. enum ieee80211_acl_cmd
 *
 * Return: QDF_STATUS
 */
QDF_STATUS os_if_son_set_acl_policy(struct wlan_objmgr_vdev *vdev,
				    ieee80211_acl_cmd son_acl_policy);

/**
 * os_if_son_get_acl_policy() - get acl policy
 * @vdev: vdev
 *
 * Return: acl policy. enum ieee80211_acl_cmd
 */
ieee80211_acl_cmd os_if_son_get_acl_policy(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_add_acl_mac() - add mac to acl
 * @vdev: vdev
 * @acl_mac: mac to add
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_add_acl_mac(struct wlan_objmgr_vdev *vdev,
			  struct qdf_mac_addr *acl_mac);

/**
 * os_if_son_get_sta_space() - get sta space
 * @vdev: target vdev
 *
 * Return: bytes which is needed to fill sta information
 */
uint32_t os_if_son_get_sta_space(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_get_sta_list() - get sta list
 * @vdev: target vdev
 * @si: pointer to ieee80211req_sta_info
 * @space: space left
 *
 * Return: void
 */
void os_if_son_get_sta_list(struct wlan_objmgr_vdev *vdev,
			    struct ieee80211req_sta_info *si, uint32_t *space);

/**
 * os_if_son_del_acl_mac() - del mac from acl
 * @vdev: vdev
 * @acl_mac: mac to del
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_del_acl_mac(struct wlan_objmgr_vdev *vdev,
			  struct qdf_mac_addr *acl_mac);

/**
 * os_if_son_kickout_mac() - kickout sta with given mac
 * @vdev: vdev
 * @mac: sta mac to kickout
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_kickout_mac(struct wlan_objmgr_vdev *vdev,
			  struct qdf_mac_addr *mac);

/**
 * os_if_son_set_chwidth() - set chan width
 * @vdev: vdev
 * @son_chwidth: son chan width
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_set_chwidth(struct wlan_objmgr_vdev *vdev,
			  enum ieee80211_cwm_width son_chwidth);

/**
 * os_if_son_get_chwidth() - get chan width
 * @vdev: vdev
 *
 * Return: son chan width
 */
enum ieee80211_cwm_width os_if_son_get_chwidth(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_son_deauth_peer_sta - Deauths specified STA
 * @vdev: vdev
 * @peer_mac: Target peer MAC address
 * @ignore_frame: True to silently deauth the peer
 *
 * Return: void
 */
void os_if_son_deauth_peer_sta(struct wlan_objmgr_vdev *vdev,
			       uint8_t *peer_mac,
			       bool ignore_frame);

/**
 * os_if_son_modify_acl - Updates ACL with given peer
 * @vdev: vdev
 * @peer_mac: Target peer MAC address
 * @allow_auth: True to allow specified peer to connect
 *
 * Return: void
 */
void os_if_son_modify_acl(struct wlan_objmgr_vdev *vdev,
			  uint8_t *peer_mac,
			  bool allow_auth);

/**
 * os_if_son_deliver_ald_event() - deliver ald events to son
 * @vdev: vdev object
 * @peer: peer object
 * @event: Name of the event
 * @event_data: event data
 *
 * Return: 0 on success
 */
int os_if_son_deliver_ald_event(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_peer *peer,
				enum ieee80211_event_type event,
				void *event_data);
/**
 * os_if_son_get_vdev_by_netdev() - Get vdev from net device
 * @dev: net device struct
 *
 * Return: objmgr vdev on success else NULL
 */
struct wlan_objmgr_vdev *os_if_son_get_vdev_by_netdev(struct net_device *dev);

/**
 * os_if_son_trigger_objmgr_object_deletion() - Trigger objmgr object deletion
 * @id: umac component id
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS os_if_son_trigger_objmgr_object_deletion(enum wlan_umac_comp_id id);

/**
 * os_if_son_trigger_objmgr_object_creation() - Trigger objmgr object creation
 * @id: umac component id
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS os_if_son_trigger_objmgr_object_creation(enum wlan_umac_comp_id id);

/**
 * os_if_son_start_acs() - Triggers ACS on the target vdev
 * @vdev: target vdev
 * @enable: True - to start ACS
 *
 * Return: 0 on success
 */
int os_if_son_start_acs(struct wlan_objmgr_vdev *vdev, uint8_t enable);

/**
 * os_if_son_set_acs_chan() - Set channel list for ACS
 * @vdev: target vdev
 * @req: channel list
 *
 * Return: 0 on success
 */
int os_if_son_set_acs_chan(struct wlan_objmgr_vdev *vdev,
			   struct ieee80211req_athdbg *req);

/**
 * os_if_son_get_acs_report() - Get ACS report
 * @vdev: target vdev
 * @acs_r: ACS report structure
 *
 * Return: 0 on success
 */
int os_if_son_get_acs_report(struct wlan_objmgr_vdev *vdev,
			     struct ieee80211_acs_dbg *acs_r);

/**
 * os_if_son_parse_generic_nl_cmd() - Sends the Generic vendor commands
 *				      to SON.
 * @wiphy: Standard wiphy object
 * @wdev: wireless device
 * @tb: Command type structure pointer
 * @type: Get/Set command
 *
 * This function parses the GENERIC vendor commands received from
 * userspace then sends the extracted data to SON module for further
 * processing along with wiphy, wdev, extected structure - param
 * and command type i.e. GET / SET. Each of the GENERIC commands are
 * interdependent and hence in SON module, they will be further
 * parsed based on type i.e. GET / SET.
 *
 * Return: 0 on success
 */
int os_if_son_parse_generic_nl_cmd(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   struct nlattr **tb,
				   enum os_if_son_vendor_cmd_type type);

/**
 * os_if_son_get_node_datarate_info() - Get datarate info about given mac
 * @vdev: vdev_obj
 * @mac_addr: mac_address to get datarate information
 * @node_info: object to store datarate information
 *
 * Return: void
 */
QDF_STATUS os_if_son_get_node_datarate_info(struct wlan_objmgr_vdev *vdev,
					    uint8_t *mac_addr,
					    wlan_node_info *node_info);

/**
 * os_if_son_get_peer_max_mcs_idx() - Get max mcs index of the peer
 * @vdev: vdev obj
 * @peer: peer obj
 *
 * Return: max mcs index on success / 0 on failure
 */
uint32_t os_if_son_get_peer_max_mcs_idx(struct wlan_objmgr_vdev *vdev,
					struct wlan_objmgr_peer *peer);

/**
 * os_if_son_get_sta_stats() - get connected sta rssi and estimated data rate
 * @vdev: pointer to vdev
 * @mac_addr: connected sta mac addr
 * @stats: pointer to ieee80211_nodestats
 *
 * Return: 0 on success, negative errno on failure
 */
int os_if_son_get_sta_stats(struct wlan_objmgr_vdev *vdev, uint8_t *mac_addr,
			    struct ieee80211_nodestats *stats);
#endif
