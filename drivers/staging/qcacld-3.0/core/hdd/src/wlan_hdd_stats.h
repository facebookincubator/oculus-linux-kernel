/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_stats.h
 *
 * WLAN Host Device Driver statistics related implementation
 *
 */

#if !defined(WLAN_HDD_STATS_H)
#define WLAN_HDD_STATS_H

#include "wlan_hdd_main.h"
#ifdef WLAN_FEATURE_11BE_MLO
#include "wlan_mlo_mgr_cmn.h"
#endif
#include <wlan_cp_stats_chipset_stats.h>

#define INVALID_MCS_IDX 255

#define DATA_RATE_11AC_MCS_MASK    0x03

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
/* LL stats get request time out value */
#define WLAN_WAIT_TIME_LL_STATS 3300
#else
#define WLAN_WAIT_TIME_LL_STATS 800
#endif

#define WLAN_HDD_TGT_NOISE_FLOOR_DBM     (-128)

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
extern const struct nla_policy qca_wlan_vendor_ll_ext_policy[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR */
extern const struct nla_policy qca_wlan_vendor_ll_clr_policy[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET */
extern const struct nla_policy qca_wlan_vendor_ll_set_policy[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET */
extern const struct nla_policy qca_wlan_vendor_ll_get_policy[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX + 1];

#define FEATURE_LL_STATS_VENDOR_COMMANDS                                \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR,          \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		WIPHY_VENDOR_CMD_NEED_NETDEV |                          \
		WIPHY_VENDOR_CMD_NEED_RUNNING,                          \
	.doit = wlan_hdd_cfg80211_ll_stats_clear,                       \
	vendor_command_policy(qca_wlan_vendor_ll_clr_policy,            \
			      QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX)    \
},                                                                      \
									\
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET,          \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |                         \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,                         \
	.doit = wlan_hdd_cfg80211_ll_stats_set,                         \
	vendor_command_policy(qca_wlan_vendor_ll_set_policy,            \
			      QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX)    \
},                                                                      \
                                                                        \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET,          \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |                         \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,                         \
	.doit = wlan_hdd_cfg80211_ll_stats_get,                         \
	vendor_command_policy(qca_wlan_vendor_ll_get_policy,            \
			      QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX)    \
},
#else
#define FEATURE_LL_STATS_VENDOR_COMMANDS
#endif

/**
 * struct index_vht_data_rate_type - vht data rate type
 * @beacon_rate_index: Beacon rate index
 * @supported_VHT80_rate: VHT80 rate
 * @supported_VHT40_rate: VHT40 rate
 * @supported_VHT20_rate: VHT20 rate
 */
struct index_vht_data_rate_type {
	uint8_t beacon_rate_index;
	uint16_t supported_VHT80_rate[2];
	uint16_t supported_VHT40_rate[2];
	uint16_t supported_VHT20_rate[2];
};

/**
 * enum data_rate_11ac_max_mcs - possible VHT max MCS values
 * @DATA_RATE_11AC_MAX_MCS_7: MCS7 rate
 * @DATA_RATE_11AC_MAX_MCS_8: MCS8 rate
 * @DATA_RATE_11AC_MAX_MCS_9: MCS9 rate
 * @DATA_RATE_11AC_MAX_MCS_NA: Not applicable
 */
enum data_rate_11ac_max_mcs {
	DATA_RATE_11AC_MAX_MCS_7,
	DATA_RATE_11AC_MAX_MCS_8,
	DATA_RATE_11AC_MAX_MCS_9,
	DATA_RATE_11AC_MAX_MCS_NA
};

/**
 * struct index_data_rate_type - non vht data rate type
 * @beacon_rate_index: Beacon rate index
 * @supported_rate: Supported rate table
 */
struct index_data_rate_type {
	uint8_t beacon_rate_index;
	uint16_t supported_rate[4];
};

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

/**
 * wlan_hdd_cfg80211_ll_stats_set() - set link layer stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
int wlan_hdd_cfg80211_ll_stats_set(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len);

/**
 * wlan_hdd_cfg80211_ll_stats_get() - get link layer stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
int wlan_hdd_cfg80211_ll_stats_get(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len);


/**
 * wlan_hdd_cfg80211_ll_stats_clear() - clear link layer stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
int wlan_hdd_cfg80211_ll_stats_clear(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len);

void wlan_hdd_clear_link_layer_stats(struct hdd_adapter *adapter);

static inline bool hdd_link_layer_stats_supported(void)
{
	return true;
}

/**
 * wlan_hdd_cfg80211_ll_stats_ext_set_param() - config monitor parameters
 * @wiphy: wiphy handle
 * @wdev: wdev handle
 * @data: user layer input
 * @data_len: length of user layer input
 *
 * return: 0 success, einval failure
 */
int wlan_hdd_cfg80211_ll_stats_ext_set_param(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len);
/**
 * hdd_get_interface_info() - get interface info
 * @link_info: Link info pointer in HDD adapter
 * @info: Pointer to interface info
 *
 * Return: bool
 */
bool hdd_get_interface_info(struct wlan_hdd_link_info *link_info,
			    struct wifi_interface_info *info);

/**
 * wlan_hdd_ll_stats_get() - Get Link Layer statistics from FW
 * @link_info: Link info pointer in HDD adapter
 * @req_id: request id
 * @req_mask: bitmask used by FW for the request
 *
 * Return: 0 on success and error code otherwise
 */
int wlan_hdd_ll_stats_get(struct wlan_hdd_link_info *link_info,
			  uint32_t req_id, uint32_t req_mask);

/**
 * wlan_hdd_cfg80211_link_layer_stats_callback() - This function is called
 * @hdd_handle: Handle to HDD context
 * @indication_type: Indication type
 * @results: Pointer to results
 * @cookie: Callback context
 *
 * After receiving Link Layer indications from FW.This callback converts the
 * firmware data to the NL data and send the same to the kernel/upper layers.
 *
 * Return: None
 */
void wlan_hdd_cfg80211_link_layer_stats_callback(hdd_handle_t hdd_handle,
						 int indication_type,
						 tSirLLStatsResults *results,
						 void *cookie);

/**
 * wlan_hdd_cfg80211_link_layer_stats_ext_callback() - Callback for LL ext
 * @ctx: HDD context
 * @rsp: msg from FW
 *
 * This function is an extension of
 * wlan_hdd_cfg80211_link_layer_stats_callback. It converts
 * monitoring parameters offloaded to NL data and send the same to the
 * kernel/upper layers.
 *
 * Return: None.
 */
void wlan_hdd_cfg80211_link_layer_stats_ext_callback(hdd_handle_t ctx,
						     tSirLLStatsResults *rsp);

/**
 * hdd_lost_link_info_cb() - callback function to get lost link information
 * @hdd_handle: Opaque handle for the HDD context
 * @lost_link_info: lost link information
 *
 * Return: none
 */
void hdd_lost_link_info_cb(hdd_handle_t hdd_handle,
			   struct sir_lost_link_info *lost_link_info);

#else /* WLAN_FEATURE_LINK_LAYER_STATS */

static inline bool hdd_link_layer_stats_supported(void)
{
	return false;
}

static inline int
wlan_hdd_cfg80211_ll_stats_ext_set_param(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	return -EINVAL;
}

static inline int
wlan_hdd_ll_stats_get(struct wlan_hdd_link_info *link_info,
		      uint32_t req_id, uint32_t req_mask)
{
	return -EINVAL;
}

static inline void
wlan_hdd_clear_link_layer_stats(struct hdd_adapter *adapter)
{
}

static inline void
wlan_hdd_cfg80211_link_layer_stats_callback(hdd_handle_t hdd_handle,
					    int indication_type,
					    tSirLLStatsResults *results,
					    void *cookie)
{
}

static inline void
wlan_hdd_cfg80211_link_layer_stats_ext_callback(hdd_handle_t ctx,
						tSirLLStatsResults *rsp)
{
}

static inline void
hdd_lost_link_info_cb(hdd_handle_t hdd_handle,
		      struct sir_lost_link_info *lost_link_info)
{
}

#endif /* End of WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef WLAN_FEATURE_STATS_EXT
/**
 * wlan_hdd_cfg80211_stats_ext_request() - ext stats request
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
int wlan_hdd_cfg80211_stats_ext_request(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len);

#endif /* End of WLAN_FEATURE_STATS_EXT */

/**
 * wlan_hdd_cfg80211_connected_chan_stats_req() - get currently connected
 * channel statistics from driver/firmware
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
int wlan_hdd_cfg80211_connected_chan_stats_req(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len);

/**
 * wlan_hdd_cfg80211_get_station() - get station statistics
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * Return: 0 for success, non-zero for failure
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_hdd_cfg80211_get_station(struct wiphy *wiphy,
				  struct net_device *dev, const uint8_t *mac,
				  struct station_info *sinfo);
#else
int wlan_hdd_cfg80211_get_station(struct wiphy *wiphy,
				  struct net_device *dev, uint8_t *mac,
				  struct station_info *sinfo);
#endif

/**
 * wlan_hdd_cfg80211_dump_station() - dump station statistics
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @idx: variable to determine whether to get stats or not
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_dump_station(struct wiphy *wiphy,
				struct net_device *dev,
				int idx, u8 *mac,
				struct station_info *sinfo);

struct net_device_stats *hdd_get_stats(struct net_device *dev);

int wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
				  struct net_device *dev,
				  int idx, struct survey_info *survey);

void hdd_display_hif_stats(void);
void hdd_clear_hif_stats(void);

/**
 * wlan_hdd_cfg80211_stats_ext_callback() - ext stats callback
 * @hdd_handle: Opaque handle to HDD context
 * @data: ext stats payload
 *
 * Return: nothing
 */
void wlan_hdd_cfg80211_stats_ext_callback(hdd_handle_t hdd_handle,
					  struct stats_ext_event *data);

/**
 * wlan_hdd_cfg80211_stats_ext2_callback() - stats_ext2_callback
 * @hdd_handle: opaque handle to the hdd context
 * @pmsg: sir_sme_rx_aggr_hole_ind
 *
 * Return: void
 */
void
wlan_hdd_cfg80211_stats_ext2_callback(hdd_handle_t hdd_handle,
				      struct sir_sme_rx_aggr_hole_ind *pmsg);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_hdd_cfg80211_roam_events_callback() - roam_events_callback
 * @roam_stats: roam events stats
 * @idx: TLV index in roam stats event
 *
 * Return: void
 */
void
wlan_hdd_cfg80211_roam_events_callback(struct roam_stats_event *roam_stats,
				       uint8_t idx);
#endif /* End of WLAN_FEATURE_ROAM_OFFLOAD */

/**
 * wlan_hdd_get_rcpi() - Wrapper to get current RCPI
 * @adapter: adapter upon which the measurement is requested
 * @mac: peer addr for which measurement is requested
 * @rcpi_value: pointer to where the RCPI should be returned
 * @measurement_type: type of rcpi measurement
 *
 * This is a wrapper function for getting RCPI, invoke this function only
 * when rcpi support is enabled in firmware
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_get_rcpi(struct hdd_adapter *adapter, uint8_t *mac,
		      int32_t *rcpi_value,
		      enum rcpi_measurement_type measurement_type);

#ifdef WLAN_FEATURE_MIB_STATS
/**
 * wlan_hdd_get_mib_stats() - Get the mib statistics
 * @adapter: adapter upon which the measurement is requested
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_hdd_get_mib_stats(struct hdd_adapter *adapter);
#endif

/**
 * wlan_hdd_get_rssi() - Get the current RSSI
 * @link_info: Link info pointer in HDD adapter
 * @rssi_value: pointer to where the RSSI should be returned
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_hdd_get_rssi(struct wlan_hdd_link_info *link_info,
			     int8_t *rssi_value);

/**
 * wlan_hdd_get_snr() - Get the current SNR
 * @link_info: Link info pointer in HDD adapter
 * @snr: pointer to where the SNR should be returned
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_hdd_get_snr(struct wlan_hdd_link_info *link_info, int8_t *snr);

/**
 * wlan_hdd_get_linkspeed_for_peermac() - Get link speed for a peer
 * @link_info: Link info pointer in adapter
 * @mac_address: MAC address of the peer
 * @linkspeed: pointer to memory where returned link speed is to be placed
 *
 * This function will send a query to SME for the linkspeed of the
 * given peer, and then wait for the callback to be invoked.
 *
 * Return: 0 if linkspeed data is available, negative errno otherwise
 */
int wlan_hdd_get_linkspeed_for_peermac(struct wlan_hdd_link_info *link_info,
				       struct qdf_mac_addr *mac_address,
				       uint32_t *linkspeed);

/**
 * wlan_hdd_get_link_speed() - get link speed
 * @link_info: Link info pointer in HDD adapter
 * @link_speed:   pointer to link speed
 *
 * This function fetches per bssid link speed.
 *
 * Return: if associated, link speed shall be returned.
 *         if not associated, link speed of 0 is returned.
 *         On error, error number will be returned.
 */
int wlan_hdd_get_link_speed(struct wlan_hdd_link_info *link_info,
			    uint32_t *link_speed);

/**
 * wlan_hdd_get_sap_go_peer_linkspeed() - Get SAP/GO peer link speed
 * @link_info:   Link info pointer in HDD adapter
 * @link_speed:  Pointer to link speed
 * @command:     Driver command string
 * @command_len: Driver command string length
 *
 * Return: 0 if linkspeed data is available, negative errno otherwise
 */
int wlan_hdd_get_sap_go_peer_linkspeed(struct wlan_hdd_link_info *link_info,
				       uint32_t *link_speed,
				       uint8_t *command,
				       uint8_t command_len);
#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
/**
 * wlan_hdd_get_peer_rx_rate_stats() - STA gets rx rate stats
 * @link_info: Link info pointer in HDD adapter
 *
 * STA gets rx rate stats through using the existed API
 * cdp_host_get_peer_stats. The reason that we make this
 * function is to avoid being disrupted by the flag
 * "get_station_fw_request_needed"
 *
 * Return: void
 */
void wlan_hdd_get_peer_rx_rate_stats(struct wlan_hdd_link_info *link_info);
#else
static inline void
wlan_hdd_get_peer_rx_rate_stats(struct wlan_hdd_link_info *link_info)
{
}
#endif

/**
 * wlan_hdd_get_station_stats() - Get station statistics
 * @link_info: Link info pointer in HDD adapter.
 *
 * Return: status of operation
 */
int wlan_hdd_get_station_stats(struct wlan_hdd_link_info *link_info);

int wlan_hdd_qmi_get_sync_resume(void);
int wlan_hdd_qmi_put_suspend(void);

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/**
 * wlan_hdd_get_big_data_station_stats() - Get big data station statistics
 * @link_info: Link info pointer in HDD adapter
 *
 * Return: status of operation
 */
int wlan_hdd_get_big_data_station_stats(struct wlan_hdd_link_info *link_info);

/**
 * wlan_cfg80211_mc_cp_get_big_data_stats() - API to get big data
 * statistics from firmware
 * @vdev:    Pointer to vdev
 * @errno:   error type in case of failure
 *
 * Return: big data stats buffer on success, Null on failure
 */
struct big_data_stats_event *
wlan_cfg80211_mc_cp_get_big_data_stats(struct wlan_objmgr_vdev *vdev,
				       int *errno);

/**
 * wlan_cfg80211_mc_cp_stats_free_big_data_stats_event() - API to release big
 * data statistics buffer
 * @info:    pointer to object to populate with big data stats
 *
 * Return: None
 */
void wlan_cfg80211_mc_cp_stats_free_big_data_stats_event(
					struct big_data_stats_event *info);
#else
static inline int
wlan_hdd_get_big_data_station_stats(struct wlan_hdd_link_info *link_info)
{
	return 0;
}

static inline struct big_data_stats_event *
wlan_cfg80211_mc_cp_get_big_data_stats(struct wlan_objmgr_vdev *vdev,
				       int *errno)
{
	return 0;
}

static inline
void wlan_cfg80211_mc_cp_stats_free_big_data_stats_event(
					struct big_data_stats_event *info)
{
}
#endif

/**
 * wlan_hdd_get_temperature() - get current device temperature
 * @adapter: device upon which the request was made
 * @temperature: pointer to where the temperature is to be returned
 *
 * Return: 0 if a temperature value (either current or cached) was
 * returned, otherwise a negative errno is returned.
 *
 */
int wlan_hdd_get_temperature(struct hdd_adapter *adapter, int *temperature);

/**
 * hdd_get_max_tx_bitrate() - Get the max tx bitrate of the AP
 * @link_info: pointer to link_info struct in adapter
 *
 * This function gets the MAX supported rate by AP and cache
 * it into connection info structure
 *
 * Return: None
 */
void hdd_get_max_tx_bitrate(struct wlan_hdd_link_info *link_info);

#ifdef TX_MULTIQ_PER_AC
/**
 * wlan_hdd_display_tx_multiq_stats() - display Tx multi queue stats
 * @context: hdd context
 * @netdev: netdev
 *
 * Return: none
 */
void wlan_hdd_display_tx_multiq_stats(hdd_cb_handle context,
				      qdf_netdev_t netdev);
#else
static inline
void wlan_hdd_display_tx_multiq_stats(hdd_cb_handle context,
				      qdf_netdev_t netdev)
{
}
#endif

/**
 * hdd_report_max_rate() - Fill the max rate stats in the station info structure
 * to be sent to the userspace.
 * @link_info: pointer to link_info struct in adapter
 * @mac_handle: The mac handle
 * @rate: The station_info tx/rx rate to be filled
 * @signal: signal from station_info
 * @rate_flags: TX/RX rate flags computed from tx/rx rate
 * @mcs_index: The TX/RX mcs index computed from tx/rx rate
 * @fw_rate: The tx/rx rate from fw stats
 * @nss: The TX/RX NSS from fw stats
 *
 * Return: True if fill is successful
 */
bool hdd_report_max_rate(struct wlan_hdd_link_info *link_info,
			 mac_handle_t mac_handle,
			 struct rate_info *rate,
			 int8_t signal,
			 enum tx_rate_info rate_flags,
			 uint8_t mcs_index,
			 uint16_t fw_rate, uint8_t nss);

/**
 * hdd_check_and_update_nss() - Check and update NSS as per DBS capability
 * @hdd_ctx: HDD Context pointer
 * @tx_nss: pointer to variable storing the tx_nss
 * @rx_nss: pointer to variable storing the rx_nss
 *
 * The parameters include the NSS obtained from the FW or static NSS. This NSS
 * could be invalid in the case the current HW mode is DBS where the connection
 * are 1x1. Rectify these NSS values as per the current HW mode.
 *
 * Return: none
 */
void hdd_check_and_update_nss(struct hdd_context *hdd_ctx,
			      uint8_t *tx_nss, uint8_t *rx_nss);
#ifdef QCA_SUPPORT_CP_STATS
/**
 * wlan_hdd_register_cp_stats_cb() - Register hdd stats specific
 * callbacks to the cp stats component
 * @hdd_ctx: hdd context
 *
 * Return: none
 */

void wlan_hdd_register_cp_stats_cb(struct hdd_context *hdd_ctx);
#else
static inline void wlan_hdd_register_cp_stats_cb(struct hdd_context *hdd_ctx) {}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_ROAM_INFO_STATS)
#define FEATURE_ROAM_STATS_COMMANDS	\
	{	\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,	\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ROAM_STATS,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |	\
		 WIPHY_VENDOR_CMD_NEED_NETDEV |	\
		 WIPHY_VENDOR_CMD_NEED_RUNNING,	\
	.doit = wlan_hdd_cfg80211_get_roam_stats,	\
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)	\
	},	\

#define FEATURE_ROAM_STATS_EVENTS	\
	[QCA_NL80211_VENDOR_SUBCMD_ROAM_STATS_INDEX] = {	\
		.vendor_id = QCA_NL80211_VENDOR_ID,	\
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_ROAM_STATS,	\
	},	\

/**
 * wlan_hdd_cfg80211_get_roam_stats() - get roam statstics information
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * Return: 0 on success; error number otherwise.
 */
int wlan_hdd_cfg80211_get_roam_stats(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len);
#else
#define FEATURE_ROAM_STATS_COMMANDS
#define FEATURE_ROAM_STATS_EVENTS
#endif

#ifdef WLAN_FEATURE_TX_LATENCY_STATS
/**
 * hdd_tx_latency_register_cb() - register callback function for transmit
 * latency stats
 * @soc: pointer to soc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_tx_latency_register_cb(void *soc);

/**
 * hdd_tx_latency_restore_config() - restore tx latency stats config for a link
 * @link_info: link specific information
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
hdd_tx_latency_restore_config(struct wlan_hdd_link_info *link_info);

/**
 * wlan_hdd_cfg80211_tx_latency - configure/retrieve per-link transmit latency
 * statistics
 * @wiphy: wiphy handle
 * @wdev: wdev handle
 * @data: user layer input
 * @data_len: length of user layer input
 *
 * return: 0 success, einval failure
 */
int wlan_hdd_cfg80211_tx_latency(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len);

/**
 * hdd_tx_latency_record_ingress_ts() - Record driver ingress timestamp in CB
 * @adapter: pointer to hdd vdev/net_device context
 * @skb: sk buff
 *
 * Return: None
 */
static inline void
hdd_tx_latency_record_ingress_ts(struct hdd_adapter *adapter,
				 struct sk_buff *skb)
{
	if (adapter->tx_latency_cfg.enable)
		qdf_nbuf_set_tx_ts(skb);
}

extern const struct nla_policy
	tx_latency_policy[QCA_WLAN_VENDOR_ATTR_TX_LATENCY_MAX + 1];

#define FEATURE_TX_LATENCY_STATS_COMMANDS	\
	{	\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,	\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |	\
		 WIPHY_VENDOR_CMD_NEED_NETDEV |	\
		 WIPHY_VENDOR_CMD_NEED_RUNNING,	\
	.doit = wlan_hdd_cfg80211_tx_latency,	\
	vendor_command_policy(tx_latency_policy,	\
			      QCA_WLAN_VENDOR_ATTR_TX_LATENCY_MAX)	\
	},	\

#define FEATURE_TX_LATENCY_STATS_EVENTS	\
	[QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY_INDEX] = {	\
		.vendor_id = QCA_NL80211_VENDOR_ID,	\
		.subcmd = QCA_NL80211_VENDOR_SUBCMD_TX_LATENCY,	\
	},	\

#else
static inline QDF_STATUS hdd_tx_latency_register_cb(void *soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
hdd_tx_latency_restore_config(struct wlan_hdd_link_info *link_info)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
hdd_tx_latency_record_ingress_ts(struct hdd_adapter *adapter,
				 struct sk_buff *skb)
{
}

#define FEATURE_TX_LATENCY_STATS_COMMANDS
#define FEATURE_TX_LATENCY_STATS_EVENTS
#endif

#ifdef WLAN_CHIPSET_STATS
/**
 * hdd_cstats_send_data_to_userspace() - Send chipsset stats to userspace
 *
 * @buff: Buffer to be sent
 * @len: length of the buffer
 * @type: Chipset stats type
 *
 * Return: 0 on success -ve value on error
 */
int hdd_cstats_send_data_to_userspace(char *buff, unsigned int len,
				      enum cstats_types type);

/**
 * hdd_register_cstats_ops() - Register chipset stats ops
 *
 * Return: void
 */
void hdd_register_cstats_ops(void);

/**
 * hdd_cstats_log_ndi_delete_req_evt() - Chipset stats for ndi delete
 *
 * @vdev: pointer to vdev object
 * @transaction_id: transaction ID
 *
 * Return : void
 */
void hdd_cstats_log_ndi_delete_req_evt(struct wlan_objmgr_vdev *vdev,
				       uint16_t transaction_id);

/**
 * hdd_cstats_log_ndi_create_resp_evt() - Chipset stats for ndi create
 * response
 * @li: pointer link_info object
 * @ndi_rsp: pointer to nan_datapath_inf_create_rsp object
 *
 * Return : void
 */
void
hdd_cstats_log_ndi_create_resp_evt(struct wlan_hdd_link_info *li,
				   struct nan_datapath_inf_create_rsp *ndi_rsp);

/**
 * hdd_cstats_log_ndi_create_req_evt() - Chipset stats for ndi create
 * request
 *
 * @vdev: pointer vdve object
 * @transaction_id : Transaction ID
 *
 * Return : void
 */
void hdd_cstats_log_ndi_create_req_evt(struct wlan_objmgr_vdev *vdev,
				       uint16_t transaction_id);
#else
static inline void hdd_register_cstats_ops(void)
{
}

static inline int
hdd_cstats_send_data_to_userspace(char *buff, unsigned int len,
				  enum cstats_types type)
{
	return 0;
}

static inline void
hdd_cstats_log_ndi_delete_req_evt(struct wlan_objmgr_vdev *vdev,
				  uint16_t transaction_id)
{
}

static inline void
hdd_cstats_log_ndi_create_resp_evt(struct wlan_hdd_link_info *li,
				   struct nan_datapath_inf_create_rsp *ndi_rsp)
{
}

static inline void
hdd_cstats_log_ndi_create_req_evt(struct wlan_objmgr_vdev *vdev,
				  uint16_t transaction_id)
{
}
#endif /* WLAN_CHIPSET_STATS */
#endif /* end #if !defined(WLAN_HDD_STATS_H) */
