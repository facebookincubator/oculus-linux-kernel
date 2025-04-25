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

#if !defined(HDD_CONFIG_H__)
#define HDD_CONFIG_H__

/**
 * DOC: wlan_hdd_config.h
 *
 * WLAN Adapter Configuration functions
 */

/* $HEADER$ */

/* Include files */
#include <wlan_hdd_includes.h>
#include <wlan_hdd_wmm.h>
#include <qdf_types.h>
#include <csr_api.h>
#include <sap_api.h>
#include <sir_mac_prot_def.h>
#include "osapi_linux.h"
#include <wmi_unified.h>
#include "wlan_pmo_hw_filter_public_struct.h"
#include "wlan_action_oui_public_struct.h"
#include "hdd_config.h"

struct hdd_context;

#define CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST_LEN 30

#define FW_MODULE_LOG_LEVEL_STRING_LENGTH  (512)
#define TX_SCHED_WRR_PARAMS_NUM            (5)

/* Defines for all of the things we read from the configuration (registry). */

#ifdef CONFIG_DP_TRACE
/* Max length of gDptraceConfig string. e.g.- "1, 6, 1, 62" */
#define DP_TRACE_CONFIG_STRING_LENGTH		(20)

/* At max 4 DP Trace config parameters are allowed. Refer - gDptraceConfig */
#define DP_TRACE_CONFIG_NUM_PARAMS		(4)

/*
 * Default value of live mode in case it cannot be determined from cfg string
 * gDptraceConfig
 */
#define DP_TRACE_CONFIG_DEFAULT_LIVE_MODE	(1)

/*
 * Default value of thresh (packets/second) beyond which DP Trace is disabled.
 * Use this default in case the value cannot be determined from cfg string
 * gDptraceConfig
 */
#define DP_TRACE_CONFIG_DEFAULT_THRESH		(6)

/*
 * Number of intervals of BW timer to wait before enabling/disabling DP Trace.
 * Since throughput threshold to disable live logging for DP Trace is very low,
 * we calculate throughput based on # packets received in a second.
 * For example assuming bandwidth timer interval is 100ms, and if more than 6
 * prints are received in 10 * 100 ms interval, we want to disable DP Trace
 * live logging. DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT is the default
 * value, to be used in case the real value cannot be derived from
 * bw timer interval
 */
#define DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT (10)

/* Default proto bitmap in case its missing in gDptraceConfig string */
#define DP_TRACE_CONFIG_DEFAULT_BITMAP \
			(QDF_NBUF_PKT_TRAC_TYPE_EAPOL |\
			QDF_NBUF_PKT_TRAC_TYPE_DHCP |\
			QDF_NBUF_PKT_TRAC_TYPE_MGMT_ACTION |\
			QDF_NBUF_PKT_TRAC_TYPE_ARP |\
			QDF_NBUF_PKT_TRAC_TYPE_ICMP |\
			QDF_NBUF_PKT_TRAC_TYPE_ICMPv6)\

/* Default verbosity, in case its missing in gDptraceConfig string*/
#define DP_TRACE_CONFIG_DEFAULT_VERBOSTY QDF_DP_TRACE_VERBOSITY_LOW

#endif

/*
 * Type declarations
 */

struct hdd_config {
	/* Config parameters */
	enum hdd_dot11_mode dot11Mode;

#ifdef FEATURE_WLAN_DYNAMIC_CVM
	/* Bitmap for operating voltage corner mode */
	uint32_t vc_mode_cfg_bitmap;
#endif
#ifdef ENABLE_MTRACE_LOG
	bool enable_mtrace;
#endif
	bool advertise_concurrent_operation;
#ifdef DHCP_SERVER_OFFLOAD
	struct dhcp_server dhcp_server_ip;
#endif /* DHCP_SERVER_OFFLOAD */
	bool apf_enabled;
	uint16_t sap_tx_leakage_threshold;
	bool sap_internal_restart;
	bool is_11k_offload_supported;
	bool is_unit_test_framework_enabled;
	bool disable_channel;

	/* HDD converged ini items are listed below this*/
	bool bug_on_reinit_failure;
	bool is_ramdump_enabled;
	uint32_t iface_change_wait_time;
	uint8_t multicast_host_fw_msgs;
	enum hdd_wext_control private_wext_control;
	bool enablefwprint;
	uint8_t enable_fw_log;

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
	/* WLAN Logging */
	bool wlan_logging_enable;
	uint32_t wlan_console_log_levels;
	uint8_t host_log_custom_nl_proto;
#endif /* WLAN_LOGGING_SOCK_SVC_ENABLE */

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	uint32_t wlan_auto_shutdown;
#endif

#ifndef REMOVE_PKT_LOG
	bool enable_packet_log;
#endif

#ifdef WLAN_FEATURE_MSCS
	uint32_t mscs_pkt_threshold;
	uint32_t mscs_voice_interval;
#endif /* WLAN_FEATURE_MSCS */

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	uint32_t tx_flow_low_watermark;
	uint32_t tx_flow_hi_watermark_offset;
	uint32_t tx_flow_max_queue_depth;
	uint32_t tx_lbw_flow_low_watermark;
	uint32_t tx_lbw_flow_hi_watermark_offset;
	uint32_t tx_lbw_flow_max_queue_depth;
	uint32_t tx_hbw_flow_low_watermark;
	uint32_t tx_hbw_flow_hi_watermark_offset;
	uint32_t tx_hbw_flow_max_queue_depth;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
	uint32_t napi_cpu_affinity_mask;
	uint32_t operating_chan_freq;
	uint8_t num_vdevs;
	uint8_t enable_concurrent_sta[CFG_CONCURRENT_IFACE_MAX_LEN];
	uint8_t dbs_scan_selection[CFG_DBS_SCAN_PARAM_LENGTH];
#ifdef FEATURE_RUNTIME_PM
	uint8_t runtime_pm;
#endif
#ifdef WLAN_FEATURE_WMI_SEND_RECV_QMI
	bool is_qmi_stats_enabled;
#endif
	uint8_t inform_bss_rssi_raw;

	bool mac_provision;
	uint32_t provisioned_intf_pool;
	uint32_t derived_intf_pool;
	uint32_t cfg_wmi_credit_cnt;
	uint32_t enable_sar_conversion;
#ifdef WLAN_FEATURE_TSF_PLUS
	uint8_t tsf_ptp_options;
#endif /* WLAN_FEATURE_TSF_PLUS */

#ifdef SAR_SAFETY_FEATURE
	uint32_t sar_safety_timeout;
	uint32_t sar_safety_unsolicited_timeout;
	uint32_t sar_safety_req_resp_timeout;
	uint32_t sar_safety_req_resp_retry;
	uint32_t sar_safety_index;
	uint32_t sar_safety_sleep_index;
	uint8_t enable_sar_safety;
	bool config_sar_safety_sleep_index;
#endif
	uint8_t nb_commands_interval;

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
	uint32_t sta_stats_cache_expiry_time;
#endif
	bool read_mac_addr_from_mac_file;
#ifdef FEATURE_SET
	bool get_wifi_features;
#endif
#ifdef FEATURE_RUNTIME_PM
	uint16_t cpu_cxpc_threshold;
#endif
	bool exclude_selftx_from_cca_busy;
#ifdef WLAN_FEATURE_11BE_MLO
	/* ml link state cache expiry time*/
	qdf_time_t link_state_cache_expiry_time;
#endif
};

/**
 * hdd_to_csr_wmm_mode() - Utility function to convert HDD to CSR WMM mode
 *
 * @mode: hdd WMM user mode
 *
 * Return: CSR WMM mode
 */
enum wmm_user_mode hdd_to_csr_wmm_mode(uint8_t mode);

QDF_STATUS hdd_update_mac_config(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_set_sme_config(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_set_policy_mgr_user_cfg(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_set_sme_chan_list(struct hdd_context *hdd_ctx);
bool hdd_update_config_cfg(struct hdd_context *hdd_ctx);
void hdd_cfg_get_global_config(struct hdd_context *hdd_ctx, char *buf,
			       int buflen);

eCsrPhyMode hdd_cfg_xlate_to_csr_phy_mode(enum hdd_dot11_mode dot11Mode);

QDF_STATUS hdd_set_idle_ps_config(struct hdd_context *hdd_ctx, bool val);
void hdd_get_pmkid_modes(struct hdd_context *hdd_ctx,
			 struct pmkid_mode_bits *pmkid_modes);

int hdd_update_tgt_cfg(hdd_handle_t hdd_handle, struct wma_tgt_cfg *cfg);

/**
 * hdd_string_to_u8_array() - used to convert decimal string into u8 array
 * @str: Decimal string
 * @array: Array where converted value is stored
 * @len: Length of the populated array
 * @array_max_len: Maximum length of the array
 *
 * This API is called to convert decimal string (each byte separated by
 * a comma) into an u8 array
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_string_to_u8_array(char *str, uint8_t *array,
				  uint8_t *len, uint16_t array_max_len);

QDF_STATUS hdd_hex_string_to_u16_array(char *str, uint16_t *int_array,
				uint8_t *len, uint8_t int_array_max_len);

void hdd_cfg_print_global_config(struct hdd_context *hdd_ctx);

/**
 * hdd_update_nss() - Update the number of spatial streams supported.
 * @link_info: Link info pointer in HDD adapter
 * @tx_nss: the number of Tx spatial streams to be updated
 * @rx_nss: the number of Rx spatial streams to be updated
 *
 * This function is used to modify the number of spatial streams
 * supported when not in connected state.
 *
 * Return: QDF_STATUS_SUCCESS if nss is correctly updated,
 *              otherwise QDF_STATUS_E_FAILURE would be returned
 */
QDF_STATUS hdd_update_nss(struct wlan_hdd_link_info *link_info,
			  uint8_t tx_nss, uint8_t rx_nss);

/**
 * hdd_get_nss() - Get the number of spatial streams supported by the adapter
 *
 * @adapter: the pointer to adapter
 * @nss: the number of spatial streams supported by the adapter
 *
 * This function is used to get the number of spatial streams supported by
 * the adapter.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_nss(struct hdd_adapter *adapter, uint8_t *nss);

/**
 * hdd_get_num_tx_chains() - Get the number of tx chains supported by the
 * adapter
 * @link_info: Link info pointer in HDD adapter
 * @tx_chains: the number of Tx chains supported by the adapter
 *
 * This function is used to get the number of Tx chains supported by
 * the adapter.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_num_tx_chains(struct wlan_hdd_link_info *link_info,
				 uint8_t *tx_chains);

/**
 * hdd_get_tx_nss() - Get the number of spatial streams supported by the adapter
 * @link_info: Link info pointer in HDD adapter
 * @tx_nss: the number Tx of spatial streams supported by the adapter
 *
 * This function is used to get the number of Tx spatial streams supported by
 * the adapter.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_tx_nss(struct wlan_hdd_link_info *link_info,
			  uint8_t *tx_nss);

/**
 * hdd_get_num_rx_chains() - Get the number of chains supported by the adapter
 * @link_info: Link info pointer in HDD adapter
 * @rx_chains: the number of Rx chains supported by the adapter
 *
 * This function is used to get the number of Rx chains supported by
 * the adapter.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_num_rx_chains(struct wlan_hdd_link_info *link_info,
				 uint8_t *rx_chains);

/**
 * hdd_get_rx_nss() - Get the number of spatial streams supported by the adapter
 * @link_info: Link info pointer in HDD adapter
 * @rx_nss: the number Rx of spatial streams supported by the adapter
 *
 * This function is used to get the number of Rx spatial streams supported by
 * the adapter.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_rx_nss(struct wlan_hdd_link_info *link_info,
			  uint8_t *rx_nss);


/**
 * hdd_dfs_indicate_radar() - Block tx as radar found on the channel
 * @hdd_ctx: HDD context pointer
 *
 * This function is invoked in atomic context when a radar
 * is found on the SAP current operating channel and Data Tx
 * from netif has to be stopped to honor the DFS regulations.
 * Actions: Stop the netif Tx queues,Indicate Radar present
 * in HDD context for future usage.
 *
 * Return: true on success, else false
 */
bool hdd_dfs_indicate_radar(struct hdd_context *hdd_ctx);

/**
 * hdd_restore_all_ps() - Restore all the powersave configuration overwritten
 * by hdd_override_all_ps.
 * @hdd_ctx: Pointer to HDD context.
 *
 * Return: None
 */
void hdd_restore_all_ps(struct hdd_context *hdd_ctx);

/**
 * hdd_override_all_ps() - overrides to disables all the powersave features.
 * @hdd_ctx: Pointer to HDD context.
 * Overrides below powersave ini configurations.
 * gEnableImps=0
 * gEnableBmps=0
 * gRuntimePM=0
 * gWlanAutoShutdown = 0
 * gEnableWoW=0
 *
 * Return: None
 */
void hdd_override_all_ps(struct hdd_context *hdd_ctx);

/**
 * hdd_vendor_mode_to_phymode() - Get eCsrPhyMode according to vendor phy mode
 * @vendor_phy_mode: vendor phy mode
 * @csr_phy_mode: phy mode of eCsrPhyMode
 *
 * Return: 0 on success, negative errno value on error
 */
int hdd_vendor_mode_to_phymode(enum qca_wlan_vendor_phy_mode vendor_phy_mode,
			       eCsrPhyMode *csr_phy_mode);

/**
 * hdd_phymode_to_vendor_mode() - Get vendor phy mode according to CSR phy mode.
 * @csr_phy_mode: phy mode of eCsrPhyMode
 * @vendor_phy_mode: vendor phy mode
 *
 * Return: 0 on success, negative error value on failure
 */
int hdd_phymode_to_vendor_mode(eCsrPhyMode csr_phy_mode,
			       enum qca_wlan_vendor_phy_mode *vendor_phy_mode);

/**
 * hdd_vendor_mode_to_band() - Get band_info according to vendor phy mode
 * @vendor_phy_mode: vendor phy mode
 * @supported_band: supported band bitmap
 * @is_6ghz_supported: whether 6ghz is supported
 *
 * Return: 0 on success, negative errno value on error
 */
int hdd_vendor_mode_to_band(enum qca_wlan_vendor_phy_mode vendor_phy_mode,
			    uint8_t *supported_band, bool is_6ghz_supported);

/**
 * hdd_vendor_mode_to_bonding_mode() - Get channel bonding mode according to
 * vendor phy mode
 * @vendor_phy_mode: vendor phy mode
 * @bonding_mode: channel bonding mode
 *
 * Return: 0 on success, negative errno value on error
 */
int
hdd_vendor_mode_to_bonding_mode(enum qca_wlan_vendor_phy_mode vendor_phy_mode,
				uint32_t *bonding_mode);

/**
 * hdd_phymode_to_dot11_mode() - Mapping phymode to dot11mode
 * @phymode: phy mode
 * @dot11_mode: dot11 mode
 *
 * Return: 0 on success, negative errno value on error
 */
int hdd_phymode_to_dot11_mode(eCsrPhyMode phymode,
			      enum hdd_dot11_mode *dot11_mode);

/**
 * hdd_update_phymode() - update the PHY mode of the adapter
 * @adapter: adapter being modified
 * @phymode: new PHY mode for the adapter
 * @supported_band: supported band bitmap for the adapter
 * @bonding_mode: new channel bonding mode for the adapter
 *
 * This function is called when the adapter is set to a new PHY mode.
 * It takes a holistic look at the desired PHY mode along with the
 * configured capabilities of the driver and the reported capabilities
 * of the hardware in order to correctly configure all PHY-related
 * parameters.
 *
 * Return: 0 on success, negative errno value on error
 */
int hdd_update_phymode(struct hdd_adapter *adapter, eCsrPhyMode phymode,
		       uint8_t supported_band, uint32_t bonding_mode);

/**
 * hdd_get_ldpc() - Get adapter LDPC
 * @adapter: adapter being queried
 * @value: where to store the value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_get_ldpc(struct hdd_adapter *adapter, int *value);

/**
 * hdd_set_ldpc() - Set adapter LDPC
 * @link_info: Link info pointer in adapter
 * @value: new LDPC value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_ldpc(struct wlan_hdd_link_info *link_info, int value);

/**
 * hdd_get_tx_stbc() - Get adapter TX STBC
 * @adapter: adapter being queried
 * @value: where to store the value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_get_tx_stbc(struct hdd_adapter *adapter, int *value);

/**
 * hdd_set_tx_stbc() - Set adapter TX STBC
 * @link_info: Link info pointer in HDD adapter
 * @value: new TX STBC value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_tx_stbc(struct wlan_hdd_link_info *link_info, int value);

/**
 * hdd_get_rx_stbc() - Get adapter RX STBC
 * @adapter: adapter being queried
 * @value: where to store the value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_get_rx_stbc(struct hdd_adapter *adapter, int *value);

/**
 * hdd_set_rx_stbc() - Set adapter RX STBC
 * @link_info: Link info pointer in HDD adapter
 * @value: new RX STBC value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_rx_stbc(struct wlan_hdd_link_info *link_info, int value);

/**
 * hdd_update_channel_width() - Update adapter channel width settings
 * @link_info: Link info in HDD adapter
 * @chwidth: new channel width of enum eSirMacHTChannelWidth
 * @bonding_mode: channel bonding mode of the new channel width
 * @link_id: mlo link id
 * @is_restore: is restore
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_update_channel_width(struct wlan_hdd_link_info *link_info,
			     enum eSirMacHTChannelWidth chwidth,
			     uint32_t bonding_mode, uint8_t link_id,
			     bool is_restore);
#endif /* end #if !defined(HDD_CONFIG_H__) */
