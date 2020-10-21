/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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
 * DOC: cds_config.h
 *
 * Defines the configuration Information for various modules. Default values
 * are read from the INI file and saved into cds_config_info which are passed
 * to various modules for the initialization.
 */

#if !defined(__CDS_CONFIG_H)
#define __CDS_CONFIG_H

#include "osdep.h"
#include "cdp_txrx_mob_def.h"
#include "wlan_cmn.h"
#include "wlan_cmn_ieee80211.h"
#include "wlan_pmo_common_public_struct.h"
#include "qca_vendor.h"

/**
 * enum cfg_sub_20_channel_width: ini values for su 20 mhz channel width
 * @WLAN_SUB_20_CH_WIDTH_5: Use 5 mhz channel width
 * @WLAN_SUB_20_CH_WIDTH_10: Use 10 mhz channel width
 */
enum cfg_sub_20_channel_width {
	WLAN_SUB_20_CH_WIDTH_NONE = 0,
	WLAN_SUB_20_CH_WIDTH_5 = 1,
	WLAN_SUB_20_CH_WIDTH_10 = 2,
};

/**
 * struct cds_config_info - Place Holder for cds configuration
 * @max_station: Max station supported
 * @max_bssid: Max Bssid Supported
 * @sta_maxlimod_dtim: station max listen interval
 * @driver_type: Enumeration of Driver Type whether FTM or Mission mode
 * currently rest of bits are not used
 * Indicates whether support is enabled or not
 * @ap_disable_intrabss_fwd: pass intra-bss-fwd info to txrx module
 * @ap_maxoffload_peers: max offload peer
 * @ap_maxoffload_reorderbuffs: max offload reorder buffs
 * @is_ra_ratelimit_enabled: Indicate RA rate limit enabled or not
 * @reorder_offload: is RX re-ordering offloaded to the fw
 * @dfs_pri_multiplier: dfs radar pri multiplier
 * @uc_offload_enabled: IPA Micro controller data path offload enable flag
 * @enable_rxthread: Rx processing in thread from TXRX
 * @tx_flow_stop_queue_th: Threshold to stop queue in percentage
 * @tx_flow_start_queue_offset: Start queue offset in percentage
 * @enable_dp_rx_threads: enable dp rx threads
 * @is_lpass_enabled: Indicate whether LPASS is enabled or not
 * @tx_chain_mask_cck: Tx chain mask enabled or not
 * @sub_20_channel_width: Sub 20 MHz ch width, ini intersected with fw cap
 * @is_fw_timeout: Indicate whether crash host when fw timesout or not
 * @ito_repeat_count: Indicates ito repeated count
 * @force_target_assert_enabled: Indicate whether target assert enabled or not
 * @bandcapability: Configured band by user
 * @rps_enabled: RPS enabled in SAP mode
 * Structure for holding cds ini parameters.
 * @num_vdevs: Configured max number of VDEVs can be supported in the stack.
 */

struct cds_config_info {
	uint16_t max_station;
	uint16_t max_bssid;
	uint8_t sta_maxlimod_dtim;
	enum qdf_driver_type driver_type;
	uint8_t ap_maxoffload_peers;
	uint8_t ap_maxoffload_reorderbuffs;
	uint8_t reorder_offload;
	uint8_t uc_offload_enabled;
	bool enable_rxthread;
	uint32_t tx_flow_stop_queue_th;
	uint32_t tx_flow_start_queue_offset;
	uint8_t enable_dp_rx_threads;
#ifdef WLAN_FEATURE_LPSS
	bool is_lpass_enabled;
#endif
	enum cfg_sub_20_channel_width sub_20_channel_width;
	uint8_t max_msdus_per_rxinorderind;
	bool self_recovery_enabled;
	bool fw_timeout_crash;
	struct ol_tx_sched_wrr_ac_specs_t ac_specs[QCA_WLAN_AC_ALL];
	uint8_t ito_repeat_count;
	bool force_target_assert_enabled;
	uint8_t bandcapability;
	bool rps_enabled;
	uint32_t num_vdevs;
	bool enable_tx_compl_tsf64;
};

#ifdef WLAN_FEATURE_FILS_SK
#define FILS_MAX_KEYNAME_NAI_LENGTH 253
#define FILS_MAX_REALM_LEN 255
#define FILS_MAX_RRK_LENGTH 64
#define FILS_MAX_RIK_LENGTH FILS_MAX_RRK_LENGTH

struct cds_fils_connection_info {
	bool is_fils_connection;
	uint8_t keyname_nai[FILS_MAX_KEYNAME_NAI_LENGTH];
	uint32_t key_nai_length;
	uint16_t sequence_number;
	uint8_t r_rk[FILS_MAX_RRK_LENGTH];
	uint32_t r_rk_length;
	uint8_t realm[FILS_MAX_REALM_LEN];
	uint32_t realm_len;
	uint8_t akm_type;
	uint8_t auth_type;
	uint8_t pmk[MAX_PMK_LEN];
	uint8_t pmk_len;
	uint8_t pmkid[PMKID_LEN];
};
#endif
#endif /* !defined( __CDS_CONFIG_H ) */
