/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_cfg80211.h
 *
 * WLAN host device driver cfg80211 functions declaration
 */

#if !defined(HDD_CFG80211_H__)
#define HDD_CFG80211_H__

#include <wlan_cfg80211_scan.h>
#include <wlan_cfg80211.h>
#include <wlan_cfg80211_tdls.h>
#include <qca_vendor.h>
#include <wlan_cfg80211_spectral.h>

struct hdd_context;

#ifdef WLAN_FEATURE_11BE_MLO
#define EHT_OPMODE_SUPPORTED 2
#else
#define EHT_OPMODE_SUPPORTED 1
#endif

/* QCA_NL80211_VENDOR_SUBCMD_ROAM policy */
extern const struct nla_policy wlan_hdd_set_roam_param_policy[
			QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_INFO policy */
extern const struct nla_policy qca_wlan_vendor_get_wifi_info_policy[
			QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION policy */
extern const struct nla_policy wlan_hdd_wifi_config_policy[
			QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_START policy */
extern const struct nla_policy qca_wlan_vendor_wifi_logger_start_policy[
			QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_ND_OFFLOAD policy */
extern const struct nla_policy ns_offload_set_policy[
			QCA_WLAN_VENDOR_ATTR_ND_OFFLOAD_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_GET_PREFERRED_FREQ_LIST policy */
extern const struct nla_policy get_preferred_freq_list_policy[
			QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_SET_PROBABLE_OPER_CHANNEL policy */
extern const struct nla_policy set_probable_oper_channel_policy[
			QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_NO_DFS_FLAG policy */
extern const struct nla_policy wlan_hdd_set_no_dfs_flag_config_policy[
			QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_GET_RING_DATA policy */
extern const struct nla_policy qca_wlan_vendor_wifi_logger_get_ring_data_policy[
			QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_OFFLOADED_PACKETS policy */
extern const struct nla_policy offloaded_packet_policy[
			QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_SETBAND policy */
extern const struct nla_policy setband_policy[QCA_WLAN_VENDOR_ATTR_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_ACS_POLICY policy */
extern const struct nla_policy wlan_hdd_set_acs_dfs_config_policy[
			QCA_WLAN_VENDOR_ATTR_ACS_DFS_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_STA_CONNECT_ROAM_POLICY policy */
extern const struct nla_policy wlan_hdd_set_sta_roam_config_policy[
			QCA_WLAN_VENDOR_ATTR_STA_CONNECT_ROAM_POLICY_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_WISA  policy */
extern const struct nla_policy wlan_hdd_wisa_cmd_policy[
			QCA_WLAN_VENDOR_ATTR_WISA_MAX + 1];

/* value for initial part of frames and number of bytes to be compared */
#define GAS_INITIAL_REQ "\x04\x0a"
#define GAS_INITIAL_REQ_SIZE 2

#define GAS_INITIAL_RSP "\x04\x0b"
#define GAS_INITIAL_RSP_SIZE 2

#define GAS_COMEBACK_REQ "\x04\x0c"
#define GAS_COMEBACK_REQ_SIZE 2

#define GAS_COMEBACK_RSP "\x04\x0d"
#define GAS_COMEBACK_RSP_SIZE 2

#define P2P_PUBLIC_ACTION_FRAME "\x04\x09\x50\x6f\x9a\x09"
#define P2P_PUBLIC_ACTION_FRAME_SIZE 6

#define P2P_ACTION_FRAME "\x7f\x50\x6f\x9a\x09"
#define P2P_ACTION_FRAME_SIZE 5

#define SA_QUERY_FRAME_REQ "\x08\x00"
#define SA_QUERY_FRAME_REQ_SIZE 2

#define SA_QUERY_FRAME_RSP "\x08\x01"
#define SA_QUERY_FRAME_RSP_SIZE 2

#define WNM_BSS_ACTION_FRAME "\x0a\x07"
#define WNM_BSS_ACTION_FRAME_SIZE 2

#define WNM_NOTIFICATION_FRAME "\x0a\x1a"
#define WNM_NOTIFICATION_FRAME_SIZE 2

#define WPA_OUI_TYPE   "\x00\x50\xf2\x01"
#define DENYLIST_OUI_TYPE   "\x00\x50\x00\x00"
#define ALLOWLIST_OUI_TYPE   "\x00\x50\x00\x01"
#define WPA_OUI_TYPE_SIZE  4
#define WMM_OUI_TYPE   "\x00\x50\xf2\x02\x01"
#define WMM_OUI_TYPE_SIZE  5

#define VENDOR1_AP_OUI_TYPE "\x00\xE0\x4C"
#define VENDOR1_AP_OUI_TYPE_SIZE 3

#define BASIC_RATE_MASK   0x80
#define RATE_MASK         0x7f

#ifndef NL80211_AUTHTYPE_FILS_SK
#define NL80211_AUTHTYPE_FILS_SK 5
#endif
#ifndef NL80211_AUTHTYPE_FILS_SK_PFS
#define NL80211_AUTHTYPE_FILS_SK_PFS 6
#endif
#ifndef NL80211_AUTHTYPE_FILS_PK
#define NL80211_AUTHTYPE_FILS_PK 7
#endif
#ifndef WLAN_AKM_SUITE_FILS_SHA256
#define WLAN_AKM_SUITE_FILS_SHA256 0x000FAC0E
#endif
#ifndef WLAN_AKM_SUITE_FILS_SHA384
#define WLAN_AKM_SUITE_FILS_SHA384 0x000FAC0F
#endif
#ifndef WLAN_AKM_SUITE_FT_FILS_SHA256
#define WLAN_AKM_SUITE_FT_FILS_SHA256 0x000FAC10
#endif
#ifndef WLAN_AKM_SUITE_FT_FILS_SHA384
#define WLAN_AKM_SUITE_FT_FILS_SHA384 0x000FAC11
#endif
#ifndef WLAN_AKM_SUITE_DPP_RSN
#define WLAN_AKM_SUITE_DPP_RSN 0x506f9a02
#endif

#ifndef WLAN_AKM_SUITE_OWE
#define WLAN_AKM_SUITE_OWE 0x000FAC12
#endif

#ifndef WLAN_AKM_SUITE_EAP_SHA256
#define WLAN_AKM_SUITE_EAP_SHA256 0x000FAC0B
#endif

#ifndef WLAN_AKM_SUITE_EAP_SHA384
#define WLAN_AKM_SUITE_EAP_SHA384 0x000FAC0C
#endif

#ifndef WLAN_AKM_SUITE_SAE
#define WLAN_AKM_SUITE_SAE 0x000FAC08
#endif

#ifndef WLAN_AKM_SUITE_FT_SAE
#define WLAN_AKM_SUITE_FT_SAE 0x000FAC09
#endif

#ifndef WLAN_AKM_SUITE_FT_EAP_SHA_384
#define WLAN_AKM_SUITE_FT_EAP_SHA_384 0x000FAC0D
#endif

#ifndef WLAN_AKM_SUITE_SAE_EXT_KEY
#define WLAN_AKM_SUITE_SAE_EXT_KEY 0x000FAC18
#endif

#ifndef WLAN_AKM_SUITE_FT_SAE_EXT_KEY
#define WLAN_AKM_SUITE_FT_SAE_EXT_KEY 0x000FAC19
#endif

#ifdef FEATURE_WLAN_TDLS
#define WLAN_IS_TDLS_SETUP_ACTION(action) \
	((TDLS_SETUP_REQUEST <= action) && \
	(TDLS_SETUP_CONFIRM >= action))
#if !defined(TDLS_MGMT_VERSION2)
#define TDLS_MGMT_VERSION2 0
#endif

#endif

#define HDD_SET_BIT(__param, __val)    ((__param) |= (1 << (__val)))

#define MAX_SCAN_SSID 10

#define IS_CHANNEL_VALID(channel) ((channel >= 0 && channel < 15) \
			|| (channel >= 36 && channel <= 184))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)) \
	|| defined(BACKPORTED_CHANNEL_SWITCH_PRESENT)
#define CHANNEL_SWITCH_SUPPORTED
#endif

#if defined(CFG80211_DEL_STA_V2) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) || defined(WITH_BACKPORTS)
#define USE_CFG80211_DEL_STA_V2
#endif

/**
 * typedef eDFS_CAC_STATUS - CAC status
 *
 * @DFS_CAC_NEVER_DONE: CAC never done
 * @DFS_CAC_IN_PROGRESS: CAC is in progress
 * @DFS_CAC_ALREADY_DONE: CAC already done
 */
typedef enum {
	DFS_CAC_NEVER_DONE,
	DFS_CAC_IN_PROGRESS,
	DFS_CAC_ALREADY_DONE,
} eDFS_CAC_STATUS;

#define MAX_REQUEST_ID			0xFFFFFFFF

/* Feature defines */
#define WIFI_FEATURE_INFRA              0x0001  /* Basic infrastructure mode */
#define WIFI_FEATURE_INFRA_5G           0x0002  /* Support for 5 GHz Band */
#define WIFI_FEATURE_HOTSPOT            0x0004  /* Support for GAS/ANQP */
#define WIFI_FEATURE_P2P                0x0008  /* Wifi-Direct */
#define WIFI_FEATURE_SOFT_AP            0x0010  /* Soft AP */
#define WIFI_FEATURE_EXTSCAN            0x0020  /* Extended Scan APIs */
#define WIFI_FEATURE_NAN                0x0040  /* Neighbor Awareness
						 * Networking
						 */
#define WIFI_FEATURE_D2D_RTT		0x0080  /* Device-to-device RTT */
#define WIFI_FEATURE_D2AP_RTT           0x0100  /* Device-to-AP RTT */
#define WIFI_FEATURE_BATCH_SCAN         0x0200  /* Batched Scan (legacy) */
#define WIFI_FEATURE_PNO                0x0400  /* Preferred network offload */
#define WIFI_FEATURE_ADDITIONAL_STA     0x0800  /* Support for two STAs */
#define WIFI_FEATURE_TDLS               0x1000  /* Tunnel directed link
						 * setup
						 */
#define WIFI_FEATURE_TDLS_OFFCHANNEL	0x2000  /* Support for TDLS off
						 * channel
						 */
#define WIFI_FEATURE_EPR                0x4000  /* Enhanced power reporting */
#define WIFI_FEATURE_AP_STA             0x8000  /* Support for AP STA
						 * Concurrency
						 */
#define WIFI_FEATURE_LINK_LAYER_STATS   0x10000  /* Link layer stats */
#define WIFI_FEATURE_LOGGER             0x20000  /* WiFi Logger */
#define WIFI_FEATURE_HAL_EPNO           0x40000  /* WiFi PNO enhanced */
#define WIFI_FEATURE_RSSI_MONITOR       0x80000  /* RSSI Monitor */
#define WIFI_FEATURE_MKEEP_ALIVE        0x100000  /* WiFi mkeep_alive */
#define WIFI_FEATURE_CONFIG_NDO         0x200000  /* ND offload configure */
#define WIFI_FEATURE_TX_TRANSMIT_POWER  0x400000  /* Tx transmit power levels */
#define WIFI_FEATURE_CONTROL_ROAMING    0x800000  /* Enable/Disable roaming */
#define WIFI_FEATURE_IE_ALLOWLIST       0x1000000 /* Support Probe IE allow
						   * listing
						   */
#define WIFI_FEATURE_SCAN_RAND          0x2000000 /* Support MAC & Probe Sequence Number randomization */
#define WIFI_FEATURE_SET_LATENCY_MODE   0x40000000 /* Set latency mode */
/* Support changing MAC address without iface reset(down and up) */
#define WIFI_FEATURE_DYNAMIC_SET_MAC    0x10000000

/* Support Tx Power Limit setting */
#define WIFI_FEATURE_SET_TX_POWER_LIMIT 0x4000000

/* Add more features here */
#define WIFI_TDLS_SUPPORT			BIT(0)
#define WIFI_TDLS_EXTERNAL_CONTROL_SUPPORT	BIT(1)
#define WIFI_TDLS_OFFCHANNEL_SUPPORT		BIT(2)

#define CFG_NON_AGG_RETRY_MAX                  (64)
#define CFG_AGG_RETRY_MAX                      (64)
#define CFG_CTRL_RETRY_MAX                     (31)
#define CFG_PROPAGATION_DELAY_MAX              (63)
#define CFG_PROPAGATION_DELAY_BASE             (64)
#define CFG_AGG_RETRY_MIN                      (5)
#define CFG_NON_AGG_RETRY_MIN                  (5)

#define CFG_NO_SUPPORT_UL_MUMIMO		(0)
#define CFG_FULL_BW_SUPPORT_UL_MUMIMO		(1)
#define CFG_PARTIAL_BW_SUPPORT_UL_MUMIMO	(2)
#define CFG_FULL_PARTIAL_BW_SUPPORT_UL_MUMIMO	(3)

#define PCL_CHANNEL_SUPPORT_GO			BIT(0)
#define PCL_CHANNEL_SUPPORT_CLI			BIT(1)
#define PCL_CHANNEL_EXCLUDE_IN_GO_NEG		BIT(3)

#define CONNECTIVITY_CHECK_SET_ARP \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ARP
#define CONNECTIVITY_CHECK_SET_DNS \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_DNS
#define CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE
#define CONNECTIVITY_CHECK_SET_ICMPV4 \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ICMPV4
#define CONNECTIVITY_CHECK_SET_ICMPV6 \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ICMPV6
#define CONNECTIVITY_CHECK_SET_TCP_SYN \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_SYN
#define CONNECTIVITY_CHECK_SET_TCP_SYN_ACK \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_SYN_ACK
#define CONNECTIVITY_CHECK_SET_TCP_ACK \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_ACK

extern const struct nla_policy
wlan_hdd_wifi_test_config_policy[
	QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_MAX + 1];

#define RSNXE_DEFAULT 0
#define RSNXE_OVERRIDE_1 1
#define RSNXE_OVERRIDE_2 2
#define CSA_DEFAULT 0
#define CSA_IGNORE 1
#define SA_QUERY_TIMEOUT_DEFAULT 0
#define SA_QUERY_TIMEOUT_IGNORE 1
#define FILS_DISCV_FRAMES_DISABLE 0
#define FILS_DISCV_FRAMES_ENABLE 1
#define H2E_RSNXE_DEFAULT 0
#define H2E_RSNXE_IGNORE 1

#define FEATURE_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION                    \
{                                                                        \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                         \
	.info.subcmd =                                                   \
		QCA_NL80211_VENDOR_SUBCMD_WIFI_TEST_CONFIGURATION,       \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                            \
		WIPHY_VENDOR_CMD_NEED_NETDEV |                           \
		WIPHY_VENDOR_CMD_NEED_RUNNING,                           \
	.doit = wlan_hdd_cfg80211_set_wifi_test_config,                  \
	vendor_command_policy(wlan_hdd_wifi_test_config_policy,          \
			      QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_MAX) \
},

extern const struct nla_policy
	qca_wlan_vendor_set_nud_stats_policy
	[QCA_ATTR_NUD_STATS_SET_MAX + 1];

#define FEATURE_VENDOR_SUBCMD_NUD_STATS_SET				    \
{									    \
		.info.vendor_id = QCA_NL80211_VENDOR_ID,		    \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NUD_STATS_SET,     \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |			    \
			WIPHY_VENDOR_CMD_NEED_NETDEV |			    \
			WIPHY_VENDOR_CMD_NEED_RUNNING,			    \
		.doit = wlan_hdd_cfg80211_set_nud_stats,		    \
		vendor_command_policy(qca_wlan_vendor_set_nud_stats_policy, \
				      QCA_ATTR_NUD_STATS_SET_MAX)	    \
},

extern const struct nla_policy
	qca_wlan_vendor_set_trace_level_policy
	[QCA_WLAN_VENDOR_ATTR_SET_TRACE_LEVEL_MAX + 1];

#define FEATURE_VENDOR_SUBCMD_SET_TRACE_LEVEL				\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_TRACE_LEVEL,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				\
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				\
	.doit = wlan_hdd_cfg80211_set_trace_level,			\
	vendor_command_policy(qca_wlan_vendor_set_trace_level_policy,	\
			      QCA_WLAN_VENDOR_ATTR_SET_TRACE_LEVEL_MAX)	\
},

/**
 * hdd_cfg80211_wiphy_alloc() - Allocate wiphy
 *
 * Allocate wiphy and hdd context.
 *
 * Return: hdd context on success and NULL on failure.
 */
struct hdd_context *hdd_cfg80211_wiphy_alloc(void);

int wlan_hdd_cfg80211_scan(struct wiphy *wiphy,
			   struct cfg80211_scan_request *request);

int wlan_hdd_cfg80211_init(struct device *dev,
			   struct wiphy *wiphy, struct hdd_config *config);

void wlan_hdd_cfg80211_deinit(struct wiphy *wiphy);

void wlan_hdd_update_wiphy(struct hdd_context *hdd_ctx);

void wlan_hdd_update_11n_mode(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_update_wiphy_supported_band() - Updates wiphy band info when
 * receive FW ready event
 * @hdd_ctx: HDD context
 *
 * Updates wiphy band info
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_hdd_update_wiphy_supported_band(struct hdd_context *hdd_ctx);

int wlan_hdd_cfg80211_register(struct wiphy *wiphy);

/**
 * wlan_hdd_cfg80211_register_frames() - register frame types and callbacks
 * with the PE.
 * @adapter: pointer to adapter
 *
 * This function is used by HDD to register frame types which are interested
 * by supplicant, callbacks for rx frame indication and ack.
 *
 * Return: 0 on success and non zero value on failure
 */
int wlan_hdd_cfg80211_register_frames(struct hdd_adapter *adapter);

void wlan_hdd_cfg80211_deregister_frames(struct hdd_adapter *adapter);

void hdd_reg_notifier(struct wiphy *wiphy,
				 struct regulatory_request *request);

QDF_STATUS wlan_hdd_validate_operation_channel(struct hdd_adapter *adapter,
					       uint32_t ch_freq);

/**
 * hdd_select_cbmode() - select channel bonding mode
 * @adapter: Pointer to adapter
 * @oper_freq: Operating frequency (MHz)
 * @sec_ch_2g_freq: secondary channel freq
 * @ch_params: channel info struct to populate
 *
 * Return: none
 */
void hdd_select_cbmode(struct hdd_adapter *adapter, qdf_freq_t oper_freq,
		       qdf_freq_t sec_ch_2g_freq, struct ch_params *ch_params);

/**
 * wlan_hdd_is_ap_supports_immediate_power_save() - to find certain vendor APs
 *				which do not support immediate power-save.
 * @ies: beacon IE of the AP which STA is connecting/connected to
 * @length: beacon IE length only
 *
 * This API takes the IE of connected/connecting AP and determines that
 * whether it has specific vendor OUI. If it finds then it will return false to
 * notify that AP doesn't support immediate power-save.
 *
 * Return: true or false based on findings
 */
bool wlan_hdd_is_ap_supports_immediate_power_save(uint8_t *ies, int length);

/**
 * wlan_hdd_del_station() - delete station wrapper
 * @adapter: pointer to the hdd adapter
 * @mac: pointer to mac addr
 *
 * Return: Errno
 */
int wlan_hdd_del_station(struct hdd_adapter *adapter, const uint8_t *mac);

#if defined(USE_CFG80211_DEL_STA_V2)
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct station_del_parameters *param);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  const uint8_t *mac);
#else
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  uint8_t *mac);
#endif
#endif /* USE_CFG80211_DEL_STA_V2 */

int wlan_hdd_send_avoid_freq_event(struct hdd_context *hdd_ctx,
				   struct ch_avoid_ind_type *avoid_freq_list);

/**
 * wlan_hdd_send_hang_reason_event() - Send hang reason to the userspace
 * @hdd_ctx: Pointer to hdd context
 * @reason: cds recovery reason
 * @data: Hang Data
 * @data_len: Hang Data len
 *
 * Return: 0 on success or failure reason
 */
int wlan_hdd_send_hang_reason_event(struct hdd_context *hdd_ctx,
				    uint32_t reason, uint8_t *data,
				    size_t data_len);

int wlan_hdd_send_avoid_freq_for_dnbs(struct hdd_context *hdd_ctx,
				      qdf_freq_t op_freq);

/**
 * wlan_hdd_rso_cmd_status_cb() - HDD callback to read RSO command status
 * @hdd_handle: opaque handle for the hdd context
 * @rso_status: rso command status
 *
 * This callback function is invoked by firmware to update
 * the RSO(ROAM SCAN OFFLOAD) command status.
 *
 * Return: None
 */
void wlan_hdd_rso_cmd_status_cb(hdd_handle_t hdd_handle,
				struct rso_cmd_status *rso_status);

void wlan_hdd_cfg80211_acs_ch_select_evt(struct hdd_adapter *adapter);

#ifdef WLAN_CFR_ENABLE
/*
 * hdd_cfr_data_send_nl_event() - send cfr data through nl event
 * @vdev_id: vdev id
 * @pid: process pid to which send data event unicast way
 * @data: pointer to the cfr data
 * @data_len: length of data
 *
 * Return: void
 */
void hdd_cfr_data_send_nl_event(uint8_t vdev_id, uint32_t pid,
				const void *data, uint32_t data_len);

#define FEATURE_CFR_DATA_VENDOR_EVENTS                                  \
[QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG_INDEX] = {              \
        .vendor_id = QCA_NL80211_VENDOR_ID,                             \
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG,       \
},
#else
#define FEATURE_CFR_DATA_VENDOR_EVENTS
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * hdd_send_roam_scan_ch_list_event() - roam scan ch list event to user space
 * @hdd_ctx: HDD context
 * @vdev_id: vdev id
 * @buf_len: length of frequency list
 * @buf: pointer to buffer of frequency list
 *
 * Return: None
 */
void hdd_send_roam_scan_ch_list_event(struct hdd_context *hdd_ctx,
				      uint8_t vdev_id, uint16_t buf_len,
				      uint8_t *buf);
#else
static inline
void hdd_send_roam_scan_ch_list_event(struct hdd_context *hdd_ctx,
				      uint8_t vdev_id, uint16_t buf_len,
				      uint8_t *buf)
{
}
#endif

int wlan_hdd_cfg80211_update_apies(struct hdd_adapter *adapter);

int wlan_hdd_sap_cfg_dfs_override(struct hdd_adapter *adapter);

int wlan_hdd_enable_dfs_chan_scan(struct hdd_context *hdd_ctx,
				  bool enable_dfs_channels);

/**
 * wlan_hdd_cfg80211_update_band() - Update band of operation
 * @hdd_ctx: The global HDD context
 * @wiphy: The wiphy being configured
 * @new_band: The new bad of operation
 *
 * This function is called from the supplicant through a
 * private ioctl to change the band value
 *
 * Return: 0 on success, else a negative errno if the operation could
 *         not be completed
 */
int wlan_hdd_cfg80211_update_band(struct hdd_context *hdd_ctx,
				  struct wiphy *wiphy,
				  enum band_info new_band);

/**
 * wlan_hdd_change_hw_mode_for_given_chnl() - change HW mode for given channel
 * @adapter: pointer to adapter
 * @chan_freq: given channel frequency
 * @reason: reason for HW mode change is needed
 *
 * This API decides and sets hardware mode to DBS based on given channel.
 * For example, some of the platforms require DBS hardware mode to operate
 * in 2.4G channel
 *
 * Return: 0 for success and non-zero for failure
 */
int wlan_hdd_change_hw_mode_for_given_chnl(struct hdd_adapter *adapter,
					   uint32_t chan_freq,
					   enum policy_mgr_conn_update_reason reason);

/**
 * enum hdd_rate_info_bw: an HDD internal rate bandwidth representation
 * @HDD_RATE_BW_5: 5MHz
 * @HDD_RATE_BW_10: 10MHz
 * @HDD_RATE_BW_20: 20MHz
 * @HDD_RATE_BW_40: 40MHz
 * @HDD_RATE_BW_80: 80MHz
 * @HDD_RATE_BW_160: 160 MHz
 * @HDD_RATE_BW_320: 320 MHz
 */
enum hdd_rate_info_bw {
	HDD_RATE_BW_5,
	HDD_RATE_BW_10,
	HDD_RATE_BW_20,
	HDD_RATE_BW_40,
	HDD_RATE_BW_80,
	HDD_RATE_BW_160,
	HDD_RATE_BW_320,
};

/**
 * enum hdd_chain_mode : Representation of Number of chains available.
 * @HDD_CHAIN_MODE_1X1: Chain mask Not Configurable as only one chain available
 * @HDD_CHAIN_MODE_2X2: Chain mask configurable as both chains available
 */
enum hdd_chain_mode {
	HDD_CHAIN_MODE_1X1 = 1,
	HDD_CHAIN_MODE_2X2 = 3,
};

/**
 * enum hdd_ba_mode: Representation of Number to configure BA mode
 * @HDD_BA_MODE_AUTO: Auto mode
 * @HDD_BA_MODE_MANUAL: Manual mode
 * @HDD_BA_MODE_64: For buffer size 64
 * @HDD_BA_MODE_256: For buffer size 256
 * @HDD_BA_MODE_128: placeholder, not valid
 * @HDD_BA_MODE_512: For buffer size 512
 * @HDD_BA_MODE_1024: For buffer size 1024
 */
enum hdd_ba_mode {
	HDD_BA_MODE_AUTO,
	HDD_BA_MODE_MANUAL,
	HDD_BA_MODE_64,
	HDD_BA_MODE_256,
	HDD_BA_MODE_128,
	HDD_BA_MODE_512,
	HDD_BA_MODE_1024,
};

/**
 * hdd_set_rate_bw(): Set the bandwidth for the given rate_info
 * @info: The rate info for which the bandwidth should be set
 * @hdd_bw: HDD representation of a rate info bandwidth
 */
void hdd_set_rate_bw(struct rate_info *info, enum hdd_rate_info_bw hdd_bw);

/*
 * hdd_get_sap_operating_band_by_adapter: Get current adapter operating channel
 * for sap.
 * @adapter: Pointer to adapter
 *
 * Return : Corresponding band for SAP operating channel
 */

uint8_t hdd_get_sap_operating_band_by_adapter(struct hdd_adapter *adapter);

/*
 * hdd_get_sap_operating_band:  Get current operating channel
 * for sap.
 * @hdd_ctx: hdd context
 *
 * Return : Corresponding band for SAP operating channel
 */
uint8_t hdd_get_sap_operating_band(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_merge_avoid_freqs(): Merge two tHddAvoidFreqList
 * @destFreqList: Destination list in which merged frequency
 * list will be available.
 * @srcFreqList: Source frequency list.
 *
 * Merges two avoid_frequency lists
 */
int wlan_hdd_merge_avoid_freqs(struct ch_avoid_ind_type *destFreqList,
		struct ch_avoid_ind_type *srcFreqList);


/**
 * hdd_bt_activity_cb() - callback function to receive bt activity
 * @hdd_handle: Opaque handle to the HDD context
 * @bt_activity: specifies the kind of bt activity
 *
 * Return: none
 */
void hdd_bt_activity_cb(hdd_handle_t hdd_handle, uint32_t bt_activity);

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/**
 * wlan_hdd_save_gtk_offload_params() - Save gtk offload parameters in STA
 *                                      context for offload operations.
 * @adapter: Adapter context
 * @kck_ptr: KCK buffer pointer
 * @kck_len: KCK length
 * @kek_ptr: KEK buffer pointer
 * @kek_len: KEK length
 * @replay_ctr: Pointer to 64 bit long replay counter
 * @big_endian: true if replay_ctr is in big endian format
 *
 * Return: None
 */
void wlan_hdd_save_gtk_offload_params(struct hdd_adapter *adapter,
				      uint8_t *kck_ptr, uint8_t  kck_len,
				      uint8_t *kek_ptr, uint32_t kek_len,
				      uint8_t *replay_ctr, bool big_endian);
#else
void wlan_hdd_save_gtk_offload_params(struct hdd_adapter *adapter,
				      uint8_t *kck_ptr, uint8_t kck_len,
				      uint8_t *kek_ptr, uint32_t kek_len,
				      uint8_t *replay_ctr, bool big_endian)
{}
#endif


/**
 * wlan_hdd_flush_pmksa_cache() - flush pmksa cache for adapter
 * @adapter: Adapter context
 *
 * Return: qdf status
 */
QDF_STATUS wlan_hdd_flush_pmksa_cache(struct hdd_adapter *adapter);

/*
 * wlan_hdd_send_mode_change_event() - API to send hw mode change event to
 * userspace
 *
 * Return : 0 on success and errno on failure
 */
int wlan_hdd_send_mode_change_event(void);

/**
 * wlan_hdd_restore_channels() - Restore the channels which were cached
 * and disabled in wlan_hdd_disable_channels api.
 * @hdd_ctx: Pointer to the HDD context
 *
 * Return: 0 on success, Error code on failure
 */
int wlan_hdd_restore_channels(struct hdd_context *hdd_ctx);

/*
 * wlan_hdd_send_sta_authorized_event: Function to send station authorized
 * event to user space in case of SAP
 * @adapter: Pointer to the adapter
 * @hdd_ctx: HDD Context
 * @mac_addr: MAC address of the STA for which the Authorized event needs to
 * be sent
 * This api is used to send station authorized event to user space
 */
QDF_STATUS wlan_hdd_send_sta_authorized_event(
					struct hdd_adapter *adapter,
					struct hdd_context *hdd_ctx,
					const struct qdf_mac_addr *mac_addr);

/**
 * hdd_set_dynamic_antenna_mode() - set dynamic antenna mode
 * @adapter: Pointer to network adapter
 * @num_rx_chains: number of chains to be used for receiving data
 * @num_tx_chains: number of chains to be used for transmitting data
 *
 * This function will set dynamic antenna mode
 *
 * Return: 0 for success
 */
int hdd_set_dynamic_antenna_mode(struct hdd_adapter *adapter,
				 uint8_t num_rx_chains,
				 uint8_t num_tx_chains);

#ifdef MULTI_CLIENT_LL_SUPPORT
/**
 * hdd_get_multi_client_ll_support() - get multi client ll support flag
 * @adapter: hdd adapter
 *
 * Return: none
 */
bool hdd_get_multi_client_ll_support(struct hdd_adapter *adapter);

/**
 * wlan_hdd_set_wlm_client_latency_level() - Set latency level to FW
 * @adapter: pointer to network adapter
 * @port_id: port id for which host sends latency level to FW
 * @latency_level: level to be set in fw
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_hdd_set_wlm_client_latency_level(struct hdd_adapter *adapter,
						 uint32_t port_id,
						 uint16_t latency_level);

/**
 * wlan_hdd_set_wlm_latency_level() - Set latency level to FW
 * @adapter: pointer to network adapter
 * @latency_level: level to be set in fw
 * @client_id_bitmap: client id bitmap
 * @force_reset: flag to reset latency level in fw
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_hdd_set_wlm_latency_level(struct hdd_adapter *adapter,
					  uint16_t latency_level,
					  uint32_t client_id_bitmap,
					  bool force_reset);

/**
 * wlan_hdd_get_set_client_info_id() - to update client info table
 * @adapter: pointer to network adapter
 * @port_id: port id for which host receives set latency level vendor command
 * @client_id: client id for a given port id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_hdd_get_set_client_info_id(struct hdd_adapter *adapter,
					   uint32_t port_id,
					   uint32_t *client_id);

/**
 * wlan_hdd_get_client_id_bitmap() - to calculate client id bitmap
 * @adapter: pointer to network adapter
 *
 * Return: client id bitmap
 */
uint8_t wlan_hdd_get_client_id_bitmap(struct hdd_adapter *adapter);

/**
 * hdd_latency_level_event_handler_cb() - Function to be invoked for low latency
 * event
 * @event_data: event data
 * @vdev_id: vdev id
 *
 * Return: none
 */
void
hdd_latency_level_event_handler_cb(const struct latency_level_data *event_data,
				   uint8_t vdev_id);
#else
static inline
QDF_STATUS wlan_hdd_set_wlm_client_latency_level(struct hdd_adapter *adapter,
						 uint32_t port_id,
						 uint16_t latency_level)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
QDF_STATUS wlan_hdd_set_wlm_latency_level(struct hdd_adapter *adapter,
					  uint16_t latency_level,
					  uint32_t client_id_bitmap,
					  bool force_reset)
{
	return QDF_STATUS_E_FAILURE;
}

static inline uint8_t wlan_hdd_get_client_id_bitmap(struct hdd_adapter *adapter)
{
	return 0;
}

static inline
QDF_STATUS wlan_hdd_get_set_client_info_id(struct hdd_adapter *adapter,
					   uint32_t port_id,
					   uint32_t *client_id)
{
	return QDF_STATUS_E_FAILURE;
}

static inline bool hdd_get_multi_client_ll_support(struct hdd_adapter *adapter)
{
	return false;
}

static inline void
hdd_latency_level_event_handler_cb(const void *event_data,
				   uint8_t vdev_id)
{
}
#endif

/**
 * hdd_convert_cfgdot11mode_to_80211mode() - Function to convert cfg dot11 mode
 *  to 80211 mode
 * @mode: cfg dot11 mode
 *
 * Return: 80211 mode
 */
enum qca_wlan_802_11_mode
hdd_convert_cfgdot11mode_to_80211mode(enum csr_cfgdot11mode mode);

/**
 * hdd_send_update_owe_info_event - Send update OWE info event
 * @adapter: Pointer to adapter
 * @sta_addr: MAC address of peer STA
 * @owe_ie: OWE IE
 * @owe_ie_len: Length of OWE IE
 *
 * Send update OWE info event to hostapd
 *
 * Return: none
 */
#if defined(CFG80211_EXTERNAL_DH_UPDATE_SUPPORT) || \
(LINUX_VERSION_CODE > KERNEL_VERSION(5, 2, 0))
void hdd_send_update_owe_info_event(struct hdd_adapter *adapter,
				    uint8_t sta_addr[],
				    uint8_t *owe_ie,
				    uint32_t owe_ie_len);
#else
static inline void hdd_send_update_owe_info_event(struct hdd_adapter *adapter,
						  uint8_t sta_addr[],
						  uint8_t *owe_ie,
						  uint32_t owe_ie_len)
{
}
#endif

/**
 * hdd_set_phy_mode() - set phy mode
 * @adapter: Handle to hdd_adapter
 * @vendor_phy_mode: phy mode to set
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_phy_mode(struct hdd_adapter *adapter,
		     enum qca_wlan_vendor_phy_mode vendor_phy_mode);

/**
 * hdd_set_mac_chan_width() - set channel width
 * @adapter: Handle to hdd_adapter
 * @chwidth: given channel width
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_mac_chan_width(struct hdd_adapter *adapter,
			   enum eSirMacHTChannelWidth chwidth);

/**
 * hdd_is_legacy_connection() - Is adapter connection is legacy
 * @adapter: Handle to hdd_adapter
 *
 * Return: true if connection mode is legacy, false otherwise.
 */
bool hdd_is_legacy_connection(struct hdd_adapter *adapter);

struct hdd_hostapd_state;

/**
 * hdd_softap_deauth_all_sta() - Deauth all sta in the sta list
 * @adapter: pointer to adapter structure
 * @hapd_state: pointer to hostapd state structure
 * @param: pointer to del sta params
 *
 * Return: QDF_STATUS on success, corresponding QDF failure status on failure
 */
QDF_STATUS hdd_softap_deauth_all_sta(struct hdd_adapter *adapter,
				     struct hdd_hostapd_state *hapd_state,
				     struct csr_del_sta_params *param);

/**
 * wlan_hdd_cfg80211_rx_control_port() - notification about a received control
 * port frame
 *
 * @dev: net device pointer
 * @ta_addr: transmitter address
 * @skb: skbuf with the control port frame
 * @unencrypted: Whether the frame is unencrypted
 *
 * Wrapper function for call to kernel function cfg80211_rx_control_port()
 *
 * Return: none
 */
bool wlan_hdd_cfg80211_rx_control_port(struct net_device *dev,
				       u8 *ta_addr,
				       struct sk_buff *skb,
				       bool unencrypted);

#ifdef WLAN_FEATURE_DBAM_CONFIG
/**
 * hdd_send_dbam_config() - send DBAM config
 * @adapter: hdd adapter
 * @dbam_mode: dbam mode configuration
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_send_dbam_config(struct hdd_adapter *adapter,
			 enum coex_dbam_config_mode dbam_mode);
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_hdd_send_key_vdev() - api to send vdev keys
 * @vdev: vdev pointer
 * @key_index: key index value
 * @pairwise: pairwise keys
 * @cipher_type: cipher type value
 *
 * Api to send vdev keys for mlo link
 *
 * Return: none
 */
QDF_STATUS wlan_hdd_send_key_vdev(struct wlan_objmgr_vdev *vdev,
				  u8 key_index, bool pairwise,
				  enum wlan_crypto_cipher_type cipher_type);

/**
 * wlan_hdd_mlo_copy_partner_addr_from_mlie  - Copy the Partner link mac
 * address from the ML IE
 * @vdev: vdev pointer
 * @partner_mac: pointer to the mac address to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_hdd_mlo_copy_partner_addr_from_mlie(struct wlan_objmgr_vdev *vdev,
					 struct qdf_mac_addr *partner_mac);
#else
static inline
QDF_STATUS wlan_hdd_send_key_vdev(struct wlan_objmgr_vdev *vdev,
				  u8 key_index, bool pairwise,
				  enum wlan_crypto_cipher_type cipher_type)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_hdd_mlo_copy_partner_addr_from_mlie(struct wlan_objmgr_vdev *vdev,
					 struct qdf_mac_addr *partner_mac)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_FEATURE_11BE_MLO */

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_TID_LINK_MAP_SUPPORT)
/**
 * hdd_mlo_dev_t2lm_notify_link_update() - Send update T2LM info event
 * @vdev: Pointer to vdev
 * @t2lm: T2LM info
 *
 * Send update T2LM info event to userspace
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_mlo_dev_t2lm_notify_link_update(struct wlan_objmgr_vdev *vdev,
					       struct wlan_t2lm_info *t2lm);
#else
static inline
QDF_STATUS hdd_mlo_dev_t2lm_notify_link_update(struct wlan_objmgr_vdev *vdev,
					       struct wlan_t2lm_info *t2lm)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && \
	defined(CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT)
/**
 * wlan_hdd_ml_sap_get_peer  - Get ML SAP peer
 * @vdev: vdev pointer
 * @peer_mld: Peer MLD address
 *
 * Return: Peer object
 */
struct wlan_objmgr_peer *
wlan_hdd_ml_sap_get_peer(struct wlan_objmgr_vdev *vdev,
			 const uint8_t *peer_mld);
#else
static inline struct wlan_objmgr_peer *
wlan_hdd_ml_sap_get_peer(struct wlan_objmgr_vdev *vdev,
			 const uint8_t *peer_mld)
{
	return NULL;
}
#endif /* WLAN_FEATURE_11BE_MLO && CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT */
#endif
