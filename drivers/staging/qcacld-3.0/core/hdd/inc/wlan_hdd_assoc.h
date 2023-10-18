/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(WLAN_HDD_ASSOC_H__)
#define WLAN_HDD_ASSOC_H__

/**
 * DOC: wlan_hdd_assoc.h
 *
 */

/* Include files */
#include <sme_api.h>
#include <wlan_defs.h>
#include "cdp_txrx_peer_ops.h"
#include <net/cfg80211.h>
#include <linux/ieee80211.h>

#define HDD_TIME_STRING_LEN 24

/* Timeout (in ms) for Link to Up before Registering Station */
#define ASSOC_LINKUP_TIMEOUT 60

#define INVALID_PEER_IDX -1

/**
 * enum eConnectionState - connection state values at HDD
 * @eConnectionState_NotConnected: Not associated in Infra
 * @eConnectionState_NdiDisconnected: NDI in disconnected state - no peers
 * @eConnectionState_NdiConnected: NDI in connected state - at least one peer
 */
typedef enum {
	eConnectionState_NotConnected,
	eConnectionState_NdiDisconnected,
	eConnectionState_NdiConnected,
} eConnectionState;

/**
 * enum peer_status - Peer status
 * @ePeerConnected: peer connected
 * @ePeerDisconnected: peer disconnected
 */
enum peer_status {
	ePeerConnected = 1,
	ePeerDisconnected
};

/**
 * struct hdd_conn_flag - connection flags
 * @ht_present: ht element present or not
 * @vht_present: vht element present or not
 * @hs20_present: hs20 element present or not
 * @ht_op_present: ht operation present or not
 * @vht_op_present: vht operation present or not
 * @reserved: reserved spare bits
 */
struct hdd_conn_flag {
	uint8_t ht_present:1;
	uint8_t vht_present:1;
	uint8_t hs20_present:1;
	uint8_t ht_op_present:1;
	uint8_t vht_op_present:1;
	uint8_t reserved:3;
};

/*defines for tx_BF_cap_info */
#define TX_BF_CAP_INFO_TX_BF			0x00000001
#define TX_BF_CAP_INFO_RX_STAG_RED_SOUNDING	0x00000002
#define TX_BF_CAP_INFO_TX_STAG_RED_SOUNDING	0x00000004
#define TX_BF_CAP_INFO_RX_ZFL			0x00000008
#define TX_BF_CAP_INFO_TX_ZFL			0x00000010
#define TX_BF_CAP_INFO_IMP_TX_BF		0x00000020
#define TX_BF_CAP_INFO_CALIBRATION		0x000000c0
#define TX_BF_CAP_INFO_CALIBRATION_SHIFT	6
#define TX_BF_CAP_INFO_EXP_CSIT_BF		0x00000100
#define TX_BF_CAP_INFO_EXP_UNCOMP_STEER_MAT	0x00000200
#define TX_BF_CAP_INFO_EXP_BF_CSI_FB		0x00001c00
#define TX_BF_CAP_INFO_EXP_BF_CSI_FB_SHIFT	10
#define TX_BF_CAP_INFO_EXP_UNCMP_STEER_MAT	0x0000e000
#define TX_BF_CAP_INFO_EXP_UNCMP_STEER_MAT_SHIFT 13
#define TX_BF_CAP_INFO_EXP_CMP_STEER_MAT_FB	0x00070000
#define TX_BF_CAP_INFO_EXP_CMP_STEER_MAT_FB_SHIFT 16
#define TX_BF_CAP_INFO_CSI_NUM_BF_ANT		0x00180000
#define TX_BF_CAP_INFO_CSI_NUM_BF_ANT_SHIFT	18
#define TX_BF_CAP_INFO_UNCOMP_STEER_MAT_BF_ANT	0x00600000
#define TX_BF_CAP_INFO_UNCOMP_STEER_MAT_BF_ANT_SHIFT 20
#define TX_BF_CAP_INFO_COMP_STEER_MAT_BF_ANT	0x01800000
#define TX_BF_CAP_INFO_COMP_STEER_MAT_BF_ANT_SHIFT 22
#define TX_BF_CAP_INFO_RSVD			0xfe000000

/* defines for antenna selection info */
#define ANTENNA_SEL_INFO			0x01
#define ANTENNA_SEL_INFO_EXP_CSI_FB_TX		0x02
#define ANTENNA_SEL_INFO_ANT_ID_FB_TX		0x04
#define ANTENNA_SEL_INFO_EXP_CSI_FB		0x08
#define ANTENNA_SEL_INFO_ANT_ID_FB		0x10
#define ANTENNA_SEL_INFO_RX_AS			0x20
#define ANTENNA_SEL_INFO_TX_SOUNDING_PPDU	0x40
#define ANTENNA_SEL_INFO_RSVD			0x80

/**
 * struct hdd_connection_info - structure to store connection information
 * @conn_state: connection state of the NIC
 * @bssid: BSSID
 * @ssid: SSID Info
 * @peer_macaddr:Peer Mac Address of the IBSS Stations
 * @auth_type: Auth Type
 * @uc_encrypt_type: Unicast Encryption Type
 * @is_authenticated: Remembers authenticated state
 * @dot11mode: dot11mode
 * @proxy_arp_service: proxy arp service
 * @ptk_installed: ptk installed state
 * @gtk_installed: gtk installed state
 * @nss: number of spatial streams negotiated
 * @rate_flags: rate flags for current connection
 * @chan_freq: channel frequency
 * @txrate: txrate structure holds nss & datarate info
 * @rxrate: rx rate info
 * @noise: holds noise information
 * @ht_caps: holds ht capabilities info
 * @vht_caps: holds vht capabilities info
 * @conn_flag: flag conn info params is present or not
 * @hs20vendor_ie: holds passpoint/hs20 info
 * @ht_operation: HT operation info
 * @vht_operation: VHT operation info
 * @he_operation: HE operation info
 * @he_oper_len: length of @he_operation
 * @roam_count: roaming counter
 * @signal: holds rssi info
 * @assoc_status_code: holds assoc fail reason
 * @congestion: holds congestion percentage
 * @last_ssid: holds last ssid
 * @last_auth_type: holds last auth type
 * @auth_time: last authentication established time
 * @connect_time: last association established time
 * @ch_width: channel width of operating channel
 * @max_tx_bitrate: Max tx bitrate supported by the AP
 * to which currently sta is connected.
 * @prev_ap_bcn_ie: ap beacon IE information to which sta is currently connected
 */
struct hdd_connection_info {
	eConnectionState conn_state;
	struct qdf_mac_addr bssid;
	tCsrSSIDInfo ssid;
	struct qdf_mac_addr peer_macaddr[MAX_PEERS];
	enum csr_akm_type auth_type;
	eCsrEncryptionType uc_encrypt_type;
	uint8_t is_authenticated;
	uint32_t dot11mode;
	uint8_t proxy_arp_service;
	bool ptk_installed;
	bool gtk_installed;
	uint8_t nss;
	uint32_t rate_flags;
	uint32_t chan_freq;
	struct rate_info txrate;
	struct rate_info rxrate;
	int8_t noise;
	struct ieee80211_ht_cap ht_caps;
	struct ieee80211_vht_cap vht_caps;
	struct hdd_conn_flag conn_flag;
	tDot11fIEhs20vendor_ie hs20vendor_ie;
	struct ieee80211_ht_operation ht_operation;
	struct ieee80211_vht_operation vht_operation;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)) \
     && defined(WLAN_FEATURE_11AX)
	struct ieee80211_he_operation *he_operation;
	uint32_t he_oper_len;
#endif
	uint32_t roam_count;
	int8_t signal;
	int32_t assoc_status_code;
	tCsrSSIDInfo last_ssid;
	enum csr_akm_type last_auth_type;
	char auth_time[HDD_TIME_STRING_LEN];
	char connect_time[HDD_TIME_STRING_LEN];
	enum phy_ch_width ch_width;
	struct rate_info max_tx_bitrate;
	struct element_info prev_ap_bcn_ie;
};

/* Forward declarations */
struct hdd_adapter;
struct hdd_station_ctx;
struct hdd_context;

/*
 * hdd_is_fils_connection: API to determine if connection is FILS
 * @hdd_ctx: hdd context
 * @adapter: hdd adapter
 *
 * Return: true if fils connection else false
 */
bool hdd_is_fils_connection(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter);

/**
 * hdd_conn_set_authenticated() - set authentication state
 * @adapter: pointer to the adapter
 * @auth_state: authentication state
 *
 * This function updates the global HDD station context
 * authentication state. And to start auto powersave timer
 * if ptk installed case and open security case.
 *
 * Return: none
 */
void
hdd_conn_set_authenticated(struct hdd_adapter *adapter, uint8_t auth_state);

/**
 * hdd_conn_set_connection_state() - set connection state
 * @adapter: pointer to the adapter
 * @conn_state: connection state
 *
 * This function updates the global HDD station context connection state.
 *
 * Return: none
 */
void hdd_conn_set_connection_state(struct hdd_adapter *adapter,
				   eConnectionState conn_state);

/**
 * hdd_conn_get_connected_band() - get current connection radio band
 * @adapter: HDD adapter
 *
 * Return: BAND_2G or BAND_5G based on current AP connection
 *      BAND_ALL if not connected
 */
enum band_info hdd_conn_get_connected_band(struct hdd_adapter *adapter);

/**
 * hdd_get_sta_connection_in_progress() - get STA for which connection
 *                                        is in progress
 * @hdd_ctx: hdd context
 *
 * Return: hdd adapter for which connection is in progress
 */
struct hdd_adapter *hdd_get_sta_connection_in_progress(
			struct hdd_context *hdd_ctx);

/**
 * hdd_abort_ongoing_sta_connection() - Disconnect the sta for which the
 * connection is in progress.
 *
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void hdd_abort_ongoing_sta_connection(struct hdd_context *hdd_ctx);

/**
 * hdd_is_any_sta_connected() - check if any sta in connected state
 * @hdd_ctx: hdd context
 *
 * Return: true if any connected sta
 */
bool hdd_is_any_sta_connected(struct hdd_context *hdd_ctx);

/**
 * hdd_get_first_connected_sta_vdev_id() - check if any sta in connected state
 * and exteact the vdev id of connected STA.
 * @hdd_ctx: hdd context
 * @vdev_id: pointer to vdev id
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS hdd_get_first_connected_sta_vdev_id(struct hdd_context *hdd_ctx,
					       uint32_t *vdev_id);

/**
 * hdd_sme_roam_callback() - hdd sme roam callback
 * @context: pointer to adapter context
 * @roam_info: pointer to roam info
 * @roam_status: roam status
 * @roam_result: roam result
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS hdd_sme_roam_callback(void *context,
				 struct csr_roam_info *roam_info,
				 eRoamCmdStatus roam_status,
				 eCsrRoamResult roam_result);

#ifdef FEATURE_WLAN_TDLS
/**
 * hdd_roam_register_tdlssta() - register new TDLS station
 * @adapter: pointer to adapter
 * @peerMac: pointer to peer MAC address
 * @qos: Quality of service
 *
 * Construct the txrx_desc and register the new STA with the Data Plane.
 * This is called as part of ADD_STA in the TDLS setup.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS hdd_roam_register_tdlssta(struct hdd_adapter *adapter,
				     const uint8_t *peerMac, uint8_t qos);
#endif

#ifdef FEATURE_WLAN_ESE
/**
 * hdd_indicate_ese_bcn_report_no_results() - beacon report no scan results
 * @adapter: pointer to adapter
 * @measurementToken: measurement token
 * @flag: flag
 * @numBss: number of bss
 *
 * If the measurement is none and no scan results found,
 * indicate the supplicant about measurement done.
 *
 * Return: none
 */
void
hdd_indicate_ese_bcn_report_no_results(const struct hdd_adapter *adapter,
					    const uint16_t measurementToken,
					    const bool flag,
					    const uint8_t numBss);
#endif /* FEATURE_WLAN_ESE */

/**
 * hdd_change_peer_state() - change peer state
 * @adapter: HDD adapter
 * @peer_mac_addr: Peer MAC address
 * @sta_state: peer state
 *
 * Return: QDF status
 */
QDF_STATUS hdd_change_peer_state(struct hdd_adapter *adapter,
				 uint8_t *peer_mac_addr,
				 enum ol_txrx_peer_state sta_state);
/**
 * hdd_update_dp_vdev_flags() - update datapath vdev flags
 * @cbk_data: callback data
 * @vdev_id: virtual interface id
 * @vdev_param: vdev parameter
 * @is_link_up: link state up or down
 *
 * Return: QDF status
 */
QDF_STATUS hdd_update_dp_vdev_flags(void *cbk_data,
				    uint8_t vdev_id,
				    uint32_t vdev_param,
				    bool is_link_up);

/**
 * hdd_roam_register_sta() - register station
 * @adapter: pointer to adapter
 * @bssid: bssid of the connection
 * @is_auth_required: is upper layer authenticatoin required
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS hdd_roam_register_sta(struct hdd_adapter *adapter,
				 struct qdf_mac_addr *bssid,
				 bool is_auth_required);

/**
 * hdd_save_peer() - Save peer MAC address in adapter peer table.
 * @sta_ctx: pointer to hdd station context
 * @peer_mac_addr: mac address of new peer
 *
 * This information is passed to iwconfig later. The peer that joined
 * last is passed as information to iwconfig.
 *
 * Return: true if success, false otherwise
 */
bool hdd_save_peer(struct hdd_station_ctx *sta_ctx,
		   struct qdf_mac_addr *peer_mac_addr);

/**
 * hdd_delete_peer() - removes peer from hdd station context peer table
 * @sta_ctx: pointer to hdd station context
 * @peer_mac_addr: mac address of peer to be deleted
 *
 * Return: None
 */
void hdd_delete_peer(struct hdd_station_ctx *sta_ctx,
		     struct qdf_mac_addr *peer_mac_addr);

/**
 * hdd_copy_ht_caps()- copy ht caps info from roam ht caps
 * info to source ht_cap info of type ieee80211_ht_cap.
 * @hdd_ht_cap: pointer to Source ht_cap info of type ieee80211_ht_cap
 * @roam_ht_cap: pointer to roam ht_caps info
 *
 * Return: None
 */

void hdd_copy_ht_caps(struct ieee80211_ht_cap *hdd_ht_cap,
		      tDot11fIEHTCaps *roam_ht_cap);

/**
 * hdd_add_beacon_filter() - add beacon filter
 * @adapter: Pointer to the hdd adapter
 *
 * Return: 0 on success and errno on failure
 */
int hdd_add_beacon_filter(struct hdd_adapter *adapter);

/**
 * hdd_copy_vht_caps()- copy vht caps info from roam vht caps
 * info to source vht_cap info of type ieee80211_vht_cap.
 * @hdd_vht_cap: pointer to Source vht_cap info of type ieee80211_vht_cap
 * @roam_vht_cap: pointer to roam vht_caps info
 *
 * Return: None
 */
void hdd_copy_vht_caps(struct ieee80211_vht_cap *hdd_vht_cap,
		       tDot11fIEVHTCaps *roam_vht_cap);

/**
 * hdd_roam_profile_init() - initialize adapter roam profile
 * @adapter: The HDD adapter being initialized
 *
 * This function initializes the roam profile that is embedded within
 * the adapter.
 *
 * Return: void
 */
void hdd_roam_profile_init(struct hdd_adapter *adapter);

/**
 * hdd_any_valid_peer_present() - Check if any valid peer is present
 * @adapter: The HDD adapter
 *
 * Check if there is any peer present with non-zero mac address other than
 * broadcast address.
 *
 * Return: True if there is any valid peer present
 */
bool hdd_any_valid_peer_present(struct hdd_adapter *adapter);

/**
 * hdd_cm_register_cb() - Sets legacy callbacks to osif
 *
 * API to set legacy callbacks to osif
 * Context: Any context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_cm_register_cb(void);

/**
 * hdd_cm_unregister_cb - Resets legacy callbacks to osif
 *
 * API to reset legacy callbacks to osif
 * Context: Any context.
 *
 * Return: QDF_STATUS
 */
void hdd_cm_unregister_cb(void);

/**
 * hdd_conn_remove_connect_info() - remove connection info
 * @sta_ctx: pointer to global HDD station context
 *
 * Return: none
 */
void hdd_conn_remove_connect_info(struct hdd_station_ctx *sta_ctx);

/**
 * hdd_clear_roam_profile_ie() - Clear Roam Profile IEs
 * @adapter: adapter who's IEs are to be cleared
 *
 * Return: None
 */
void hdd_clear_roam_profile_ie(struct hdd_adapter *adapter);

/**
 * hdd_remove_beacon_filter() - remove beacon filter
 * @adapter: Pointer to the hdd adapter
 *
 * Return: 0 on success and errno on failure
 */
int hdd_remove_beacon_filter(struct hdd_adapter *adapter);

/**
 * hdd_copy_ht_operation()- copy HT operation element to
 * hdd station context.
 * @hdd_sta_ctx: pointer to hdd station context
 * @ht_ops: pointer to ht operation
 *
 * Return: None
 */
void hdd_copy_ht_operation(struct hdd_station_ctx *hdd_sta_ctx,
			   tDot11fIEHTInfo *ht_ops);

/**
 * hdd_copy_vht_operation()- copy VHT operations element to
 * hdd station context.
 * @hdd_sta_ctx: pointer to hdd station context
 * @vht_ops: pointer to vht operation
 *
 * Return: None
 */
void hdd_copy_vht_operation(struct hdd_station_ctx *hdd_sta_ctx,
			    tDot11fIEVHTOperation *vht_ops);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)) && \
     defined(WLAN_FEATURE_11AX)
/**
 * hdd_copy_he_operation()- copy HE operations element to
 * hdd station context.
 * @hdd_sta_ctx: pointer to hdd station context
 * @he_operation: pointer to he operation
 *
 * Return: None
 */
void hdd_copy_he_operation(struct hdd_station_ctx *hdd_sta_ctx,
			   tDot11fIEhe_op *he_operation);
#else
static inline void hdd_copy_he_operation(struct hdd_station_ctx *hdd_sta_ctx,
					 tDot11fIEhe_op *he_operation)
{
}
#endif

/**
 * hdd_is_roam_sync_in_progress()- Check if roam offloaded
 * @hdd_ctx: Pointer to hdd context
 * @vdev_id: Vdev id
 *
 * Return: roam sync status if roaming offloaded else false
 */
bool hdd_is_roam_sync_in_progress(struct hdd_context *hdd_ctx, uint8_t vdev_id);

#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * wlan_hdd_ft_set_key_delay() - hdd set key delayed for FT mode
 * @vdev: vdev
 *
 * Return: void
 */
void wlan_hdd_ft_set_key_delay(struct wlan_objmgr_vdev *vdev);
#else
static inline void
wlan_hdd_ft_set_key_delay(struct wlan_objmgr_vdev *vdev)
{
}
#endif

#ifdef FEATURE_WLAN_WAPI
/**
 * hdd_translate_wapi_to_csr_auth_type() - Translate WAPI to CSR auth type
 * @auth_suite: auth suite
 *
 * Return: enum csr_akm_type enumeration
 */
enum csr_akm_type hdd_translate_wapi_to_csr_auth_type(uint8_t auth_suite[4]);

/**
 * hdd_translate_wapi_to_csr_encryption_type() -
 *	Translate WAPI to CSR encryption type
 * @cipher_suite: cipher suite
 *
 * Return: eCsrEncryptionType enumeration
 */
eCsrEncryptionType
hdd_translate_wapi_to_csr_encryption_type(uint8_t cipher_suite[4]);
#else
enum csr_akm_type hdd_translate_wapi_to_csr_auth_type(uint8_t auth_suite[4])
{
	return eCSR_AUTH_TYPE_UNKNOWN;
}

eCsrEncryptionType
hdd_translate_wapi_to_csr_encryption_type(uint8_t cipher_suite[4])
{
	return eCSR_AUTH_TYPE_UNKNOWN;
}
#endif

/**
 * hdd_convert_ch_width_to_cdp_peer_bw() - Convert ch_width to DP format
 * @ch_width: ch_width
 *
 * Return: cdp_peer_bw enumeration
 */
enum cdp_peer_bw
hdd_convert_ch_width_to_cdp_peer_bw(enum phy_ch_width ch_width);

#endif
