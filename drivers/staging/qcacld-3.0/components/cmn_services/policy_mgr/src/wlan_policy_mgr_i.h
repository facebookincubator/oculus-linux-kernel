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

#ifndef WLAN_POLICY_MGR_I_H
#define WLAN_POLICY_MGR_I_H

#include "wlan_policy_mgr_api.h"
#include "qdf_event.h"
#include "qdf_mc_timer.h"
#include "qdf_lock.h"
#include "qdf_defer.h"
#include "wlan_reg_services_api.h"
#include "cds_ieee80211_common_i.h"
#include "qdf_delayed_work.h"
#define DBS_OPPORTUNISTIC_TIME   5

#define POLICY_MGR_SER_CMD_TIMEOUT 4000

#ifdef QCA_WIFI_3_0_EMU
#define CONNECTION_UPDATE_TIMEOUT (POLICY_MGR_SER_CMD_TIMEOUT + 3000)
#else
#define CONNECTION_UPDATE_TIMEOUT (POLICY_MGR_SER_CMD_TIMEOUT + 2000)
#endif

#define PM_24_GHZ_CH_FREQ_6   (2437)
#define PM_5_GHZ_CH_FREQ_36   (5180)
#define CHANNEL_SWITCH_COMPLETE_TIMEOUT   (2000)
#define MAX_NOA_TIME (3000)

/* Defer SAP force SCC check by 2000ms due to another SAP/GO start AP in
 * progress
 */
#define SAP_CONC_CHECK_DEFER_TIMEOUT_MS (2000)

/*
 * Policy Mgr hardware mode list bit-mask definitions.
 * Bits 4:0, 31:29 are unused.
 *
 * The below definitions are added corresponding to WMI DBS HW mode
 * list to make it independent of firmware changes for WMI definitions.
 * Currently these definitions have dependency with BIT positions of
 * the existing WMI macros. Thus, if the BIT positions are changed for
 * WMI macros, then these macros' BIT definitions are also need to be
 * changed.
 */
#define POLICY_MGR_HW_MODE_EMLSR_MODE_BITPOS       (32)
#define POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_BITPOS  (28)
#define POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_BITPOS  (24)
#define POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_BITPOS  (20)
#define POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_BITPOS  (16)
#define POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_BITPOS   (12)
#define POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_BITPOS   (8)
#define POLICY_MGR_HW_MODE_DBS_MODE_BITPOS         (7)
#define POLICY_MGR_HW_MODE_AGILE_DFS_MODE_BITPOS   (6)
#define POLICY_MGR_HW_MODE_SBS_MODE_BITPOS         (5)
#define POLICY_MGR_HW_MODE_MAC0_BAND_BITPOS        (3)
#define POLICY_MGR_HW_MODE_ID_BITPOS               (0)

#define POLICY_MGR_HW_MODE_EMLSR_MODE_MASK         \
	(0x1 << POLICY_MGR_HW_MODE_EMLSR_MODE_BITPOS)
#define POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_MASK    \
	(0xf << POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_MASK    \
	(0xf << POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_MASK    \
	(0xf << POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_MASK    \
	(0xf << POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_MASK     \
	(0xf << POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_BITPOS)
#define POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_MASK     \
	(0xf << POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_BITPOS)
#define POLICY_MGR_HW_MODE_DBS_MODE_MASK           \
	(0x1 << POLICY_MGR_HW_MODE_DBS_MODE_BITPOS)
#define POLICY_MGR_HW_MODE_AGILE_DFS_MODE_MASK     \
	(0x1 << POLICY_MGR_HW_MODE_AGILE_DFS_MODE_BITPOS)
#define POLICY_MGR_HW_MODE_SBS_MODE_MASK           \
	(0x1 << POLICY_MGR_HW_MODE_SBS_MODE_BITPOS)
#define POLICY_MGR_HW_MODE_MAC0_BAND_MASK           \
			(0x3 << POLICY_MGR_HW_MODE_MAC0_BAND_BITPOS)
#define POLICY_MGR_HW_MODE_ID_MASK           \
			(0x7 << POLICY_MGR_HW_MODE_ID_BITPOS)

#define POLICY_MGR_HW_MODE_EMLSR_MODE_SET(hw_mode, tmp, value) \
	QDF_SET_BITS64(hw_mode, tmp, POLICY_MGR_HW_MODE_EMLSR_MODE_BITPOS,\
	1, value)
#define POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_BITPOS,\
	4, value)
#define POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_BITPOS,\
	4, value)
#define POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_BITPOS,\
	4, value)
#define POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_BITPOS,\
	4, value)
#define POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_SET(hw_mode, value)  \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_BITPOS,\
	4, value)
#define POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_SET(hw_mode, value)  \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_BITPOS,\
	4, value)
#define POLICY_MGR_HW_MODE_DBS_MODE_SET(hw_mode, value)        \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_DBS_MODE_BITPOS,\
	1, value)
#define POLICY_MGR_HW_MODE_AGILE_DFS_SET(hw_mode, value)       \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_AGILE_DFS_MODE_BITPOS,\
	1, value)
#define POLICY_MGR_HW_MODE_SBS_MODE_SET(hw_mode, value)        \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_SBS_MODE_BITPOS,\
	1, value)
#define POLICY_MGR_HW_MODE_MAC0_BAND_SET(hw_mode, value)        \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_MAC0_BAND_BITPOS,\
	2, value)
#define POLICY_MGR_HW_MODE_ID_SET(hw_mode, value)        \
	WMI_SET_BITS(hw_mode, POLICY_MGR_HW_MODE_ID_BITPOS,\
	3, value)

#define POLICY_MGR_HW_MODE_EMLSR_MODE_GET(hw_mode)                     \
		QDF_GET_BITS64(hw_mode, POLICY_MGR_HW_MODE_EMLSR_MODE_BITPOS,\
		1)
#define POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_GET(hw_mode)                \
		(((hw_mode) & POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_MASK) >>    \
		POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_GET(hw_mode)                \
		(((hw_mode) & POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_MASK) >>    \
		POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_GET(hw_mode)                \
		(((hw_mode) & POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_MASK) >>    \
		POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_GET(hw_mode)                \
		(((hw_mode) & POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_MASK) >>    \
		POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_BITPOS)
#define POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_GET(hw_mode)                 \
		(((hw_mode) & POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_MASK) >>     \
		POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_BITPOS)
#define POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_GET(hw_mode)                 \
		(((hw_mode) & POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_MASK) >>     \
		POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_BITPOS)
#define POLICY_MGR_HW_MODE_DBS_MODE_GET(hw_mode)                       \
		(((hw_mode) & POLICY_MGR_HW_MODE_DBS_MODE_MASK) >>           \
		POLICY_MGR_HW_MODE_DBS_MODE_BITPOS)
#define POLICY_MGR_HW_MODE_AGILE_DFS_GET(hw_mode)                      \
		(((hw_mode) & POLICY_MGR_HW_MODE_AGILE_DFS_MODE_MASK) >>     \
		POLICY_MGR_HW_MODE_AGILE_DFS_MODE_BITPOS)
#define POLICY_MGR_HW_MODE_SBS_MODE_GET(hw_mode)                       \
		(((hw_mode) & POLICY_MGR_HW_MODE_SBS_MODE_MASK) >>           \
		POLICY_MGR_HW_MODE_SBS_MODE_BITPOS)
#define POLICY_MGR_HW_MODE_MAC0_BAND_GET(hw_mode)                       \
		(((hw_mode) & POLICY_MGR_HW_MODE_MAC0_BAND_MASK) >> \
		POLICY_MGR_HW_MODE_MAC0_BAND_BITPOS)
#define POLICY_MGR_HW_MODE_ID_GET(hw_mode)                       \
		(((hw_mode) & POLICY_MGR_HW_MODE_ID_MASK) >> \
		POLICY_MGR_HW_MODE_ID_BITPOS)

#define POLICY_MGR_DEFAULT_HW_MODE_INDEX 0xFFFF

#define policy_mgr_alert(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_POLICY_MGR, params)
#define policy_mgr_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_POLICY_MGR, params)
#define policy_mgr_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_POLICY_MGR, params)
#define policy_mgr_notice(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_POLICY_MGR, params)
#define policy_mgr_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_POLICY_MGR, params)
#define policy_mgr_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_POLICY_MGR, params)

#define policymgr_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_POLICY_MGR, params)
#define policymgr_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_POLICY_MGR, params)
#define policymgr_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_POLICY_MGR, params)
#define policymgr_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_POLICY_MGR, params)
#define policymgr_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_POLICY_MGR, params)

#define policy_mgr_rl_debug(params...) \
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_POLICY_MGR, params)

#define PM_CONC_CONNECTION_LIST_VALID_INDEX(index) \
		((MAX_NUMBER_OF_CONC_CONNECTIONS > index) && \
			(pm_conc_connection_list[index].in_use))

extern struct policy_mgr_conc_connection_info
	pm_conc_connection_list[MAX_NUMBER_OF_CONC_CONNECTIONS];

#ifdef WLAN_FEATURE_11BE_MLO
extern struct policy_mgr_disabled_ml_link_info
	pm_disabled_ml_links[MAX_NUMBER_OF_DISABLE_LINK];
#endif

extern const enum policy_mgr_pcl_type
	first_connection_pcl_table[PM_MAX_NUM_OF_MODE]
			[PM_MAX_CONC_PRIORITY_MODE];
extern  pm_dbs_pcl_second_connection_table_type
	*second_connection_pcl_dbs_table;

extern enum policy_mgr_pcl_type const
	(*second_connection_pcl_non_dbs_table)[PM_MAX_ONE_CONNECTION_MODE]
			[PM_MAX_NUM_OF_MODE][PM_MAX_CONC_PRIORITY_MODE];
extern pm_dbs_pcl_third_connection_table_type
		*third_connection_pcl_dbs_table;
extern enum policy_mgr_pcl_type const
	(*third_connection_pcl_non_dbs_table)[PM_MAX_TWO_CONNECTION_MODE]
			[PM_MAX_NUM_OF_MODE][PM_MAX_CONC_PRIORITY_MODE];

extern policy_mgr_next_action_two_connection_table_type
		*next_action_two_connection_table;
extern policy_mgr_next_action_three_connection_table_type
		*next_action_three_connection_table;

#ifdef FEATURE_FOURTH_CONNECTION
extern const enum policy_mgr_pcl_type
	fourth_connection_pcl_dbs_sbs_table
	[PM_MAX_THREE_CONNECTION_MODE][PM_MAX_NUM_OF_MODE]
	[PM_MAX_CONC_PRIORITY_MODE];
#endif

extern policy_mgr_next_action_two_connection_table_type
		*next_action_two_connection_2x2_2g_1x1_5g_table;
extern policy_mgr_next_action_three_connection_table_type
		*next_action_three_connection_2x2_2g_1x1_5g_table;

extern enum policy_mgr_conc_next_action
	(*policy_mgr_get_current_pref_hw_mode_ptr)
	(struct wlan_objmgr_psoc *psoc);

/**
 * struct policy_mgr_cfg - all the policy manager owned configs
 * @mcc_to_scc_switch: switch to indicate MCC to SCC config
 * @sys_pref: system's preference while selecting PCLs
 * @max_conc_cxns: Max allowed concurrenct active connections
 * @conc_rule1: concurrency rule1
 * @conc_rule2: concurrency rule2
 * @allow_mcc_go_diff_bi: Allow GO and STA diff beacon interval in MCC
 * @dual_mac_feature: To enable/disable dual mac features
 * @is_force_1x1_enable: Is 1x1 forced for connection
 * @sta_sap_scc_on_dfs_chnl: STA-SAP SCC on DFS channel
 * @sta_sap_scc_on_lte_coex_chnl: STA-SAP SCC on LTE Co-ex channel
 * @sta_sap_scc_on_indoor_channel: Allow STA-SAP scc on indoor only
 * channels
 * @nan_sap_scc_on_lte_coex_chnl: NAN-SAP SCC on LTE Co-ex channel
 * @sap_mandatory_chnl_enable: To enable/disable SAP mandatory channels
 * @mark_indoor_chnl_disable: Mark indoor channel as disable or enable
 * @dbs_selection_plcy: DBS selection policy for concurrency
 * @vdev_priority_list: Priority list for various vdevs
 * @chnl_select_plcy: Channel selection policy
 * @enable_mcc_adaptive_sch: Enable/Disable MCC adaptive scheduler
 * @enable_sta_cxn_5g_band: Enable/Disable STA connection in 5G band
 * @go_force_scc: Enable/Disable P2P GO force SCC
 * @pcl_band_priority: PCL channel order between 5G and 6G.
 * @sbs_enable: To enable/disable SBS
 * @multi_sap_allowed_on_same_band: Enable/Disable multi sap started
 *                                  on same band
 * @sr_in_same_mac_conc: Enable/Disable SR in same MAC concurrency
 * @use_sap_original_bw: Enable/Disable sap original BW as default
 *                       BW when do restart
 * @move_sap_go_1st_on_dfs_sta_csa: Enable/Disable SAP / GO's movement
 *				    to non-DFS channel before STA
 */
struct policy_mgr_cfg {
	uint8_t mcc_to_scc_switch;
	uint8_t sys_pref;
	uint8_t max_conc_cxns;
	uint8_t conc_rule1;
	uint8_t conc_rule2;
	bool enable_mcc_adaptive_sch;
	uint8_t allow_mcc_go_diff_bi;
	uint8_t dual_mac_feature;
	enum force_1x1_type is_force_1x1_enable;
	uint8_t sta_sap_scc_on_dfs_chnl;
	uint8_t sta_sap_scc_on_lte_coex_chnl;
	bool sta_sap_scc_on_indoor_channel;
	uint8_t nan_sap_scc_on_lte_coex_chnl;
	uint8_t sap_mandatory_chnl_enable;
	uint8_t mark_indoor_chnl_disable;
	uint8_t enable_sta_cxn_5g_band;
	uint32_t dbs_selection_plcy;
	uint32_t vdev_priority_list;
	uint32_t chnl_select_plcy;
	uint8_t go_force_scc;
	enum policy_mgr_pcl_band_priority pcl_band_priority;
	bool sbs_enable;
	bool multi_sap_allowed_on_same_band;
#ifdef WLAN_FEATURE_SR
	bool sr_in_same_mac_conc;
#endif
	bool use_sap_original_bw;
	bool move_sap_go_1st_on_dfs_sta_csa;
};

/**
 * struct policy_mgr_psoc_priv_obj - Policy manager private data
 * @psoc: pointer to PSOC object information
 * @pdev: pointer to PDEV object information
 * @connection_update_done_evt: qdf event to synchronize
 *                            connection activities
 * @qdf_conc_list_lock: To protect connection table
 * @dbs_opportunistic_timer: Timer to drop down to Single Mac
 *                         Mode opportunistically
 * @sap_restart_chan_switch_cb: Callback for channel switch
 *                            notification for SAP
 * @hdd_cbacks: callbacks to be registered by HDD for
 *            interaction with Policy Manager
 * @sme_cbacks: callbacks to be registered by SME for
 *            interaction with Policy Manager
 * @wma_cbacks: callbacks to be registered by SME for
 * interaction with Policy Manager
 * @tdls_cbacks: callbacks to be registered by SME for
 * interaction with Policy Manager
 * @cdp_cbacks: callbacks to be registered by SME for
 * interaction with Policy Manager
 * @dp_cbacks: callbacks to be registered by Datapath for
 * interaction with Policy Manager
 * @conc_cbacks: callbacks to be registered by lim for
 * interaction with Policy Manager
 * @sap_mandatory_channels: The user preferred master list on
 *                        which SAP can be brought up. This
 *                        mandatory channel freq list would be as per
 *                        OEMs preference & conforming to the
 *                        regulatory/other considerations
 * @sap_mandatory_channels_len: Length of the SAP mandatory
 *                            channel list
 * @do_sap_unsafe_ch_check: whether need check sap unsafe channel
 * @last_disconn_sta_freq: last disconnected sta channel freq
 * @concurrency_mode: active concurrency combination
 * @no_of_open_sessions: Number of active vdevs
 * @no_of_active_sessions: Number of active connections
 * @sta_ap_intf_check_work: delayed sap restart work
 * @work_fail_count: sta_ap work schedule fail count
 * @nan_sap_conc_work: Info related to nan sap conc work
 * @num_dbs_hw_modes: Number of different HW modes supported
 * @hw_mode: List of HW modes supported
 * @old_hw_mode_index: Old HW mode from hw_mode table
 * @new_hw_mode_index: New HW mode from hw_mode table
 * @dual_mac_cfg: DBS configuration currently used by FW for
 *              scan & connections
 * @radio_comb_num: radio combination number
 * @radio_combinations: radio combination list
 * @hw_mode_change_in_progress: This is to track if HW mode
 *                            change is in progress
 * @enable_mcc_adaptive_scheduler: Enable MCC adaptive scheduler
 *      value from INI
 * @user_cfg:
 * @unsafe_channel_list: LTE coex channel freq avoidance list
 * @unsafe_channel_count: LTE coex channel avoidance list count
 * @sta_ap_intf_check_work_info: Info related to sta_ap_intf_check_work
 * @cur_conc_system_pref:
 * @opportunistic_update_done_evt: qdf event to synchronize host
 *                               & FW HW mode
 * @channel_switch_complete_evt: qdf event for channel switch completion check
 * @mode_change_cb: Mode change callback
 * @cfg: Policy manager config data
 * @valid_ch_freq_list: valid frequencies
 * @valid_ch_freq_list_count: number of valid frequencies
 * @dynamic_mcc_adaptive_sched: disable/enable mcc adaptive scheduler feature
 * @dynamic_dfs_master_disabled: current state of dynamic dfs master
 * @link_in_progress: To track if set link is in progress
 * @set_link_update_done_evt: qdf event to synchronize set link
 * @active_vdev_bitmap: Active vdev id bitmap
 * @inactive_vdev_bitmap: Inactive vdev id bitmap
 * @restriction_mask:
 */
struct policy_mgr_psoc_priv_obj {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	qdf_event_t connection_update_done_evt;
	qdf_mutex_t qdf_conc_list_lock;
	qdf_mc_timer_t dbs_opportunistic_timer;
	struct policy_mgr_hdd_cbacks hdd_cbacks;
	struct policy_mgr_sme_cbacks sme_cbacks;
	struct policy_mgr_wma_cbacks wma_cbacks;
	struct policy_mgr_tdls_cbacks tdls_cbacks;
	struct policy_mgr_cdp_cbacks cdp_cbacks;
	struct policy_mgr_dp_cbacks dp_cbacks;
	struct policy_mgr_conc_cbacks conc_cbacks;
	uint32_t sap_mandatory_channels[NUM_CHANNELS];
	uint32_t sap_mandatory_channels_len;
	qdf_freq_t last_disconn_sta_freq;
	uint32_t concurrency_mode;
	uint8_t no_of_open_sessions[QDF_MAX_NO_OF_MODE];
	uint8_t no_of_active_sessions[QDF_MAX_NO_OF_MODE];
	struct qdf_delayed_work sta_ap_intf_check_work;
	uint8_t work_fail_count;
	qdf_work_t nan_sap_conc_work;
	uint32_t num_dbs_hw_modes;
	struct dbs_hw_mode_info hw_mode;
	uint32_t old_hw_mode_index;
	uint32_t new_hw_mode_index;
	struct dual_mac_config dual_mac_cfg;
	uint32_t radio_comb_num;
	struct radio_combination radio_combinations[MAX_RADIO_COMBINATION];
	uint32_t hw_mode_change_in_progress;
	struct policy_mgr_user_cfg user_cfg;
	uint32_t unsafe_channel_list[NUM_CHANNELS];
	uint16_t unsafe_channel_count;
	struct sta_ap_intf_check_work_ctx *sta_ap_intf_check_work_info;
	uint8_t cur_conc_system_pref;
	qdf_event_t opportunistic_update_done_evt;
	qdf_event_t channel_switch_complete_evt;
	send_mode_change_event_cb mode_change_cb;
	struct policy_mgr_cfg cfg;
	uint32_t valid_ch_freq_list[NUM_CHANNELS];
	uint32_t valid_ch_freq_list_count;
	bool dynamic_mcc_adaptive_sched;
	bool dynamic_dfs_master_disabled;
#ifdef WLAN_FEATURE_11BE_MLO
	qdf_atomic_t link_in_progress;
	qdf_event_t set_link_update_done_evt;
#endif
	uint32_t active_vdev_bitmap;
	uint32_t inactive_vdev_bitmap;
#ifdef FEATURE_WLAN_CH_AVOID_EXT
	uint32_t restriction_mask;
#endif
};

/**
 * struct policy_mgr_mac_ss_bw_info - hw_mode_list PHY/MAC params for each MAC
 * @mac_tx_stream: Max TX stream number supported on MAC
 * @mac_rx_stream: Max RX stream number supported on MAC
 * @mac_bw: Max bandwidth(wmi_channel_width enum type)
 * @mac_band_cap: supported Band bit map(WLAN_2G_CAPABILITY = 0x1,
 *                            WLAN_5G_CAPABILITY = 0x2)
 * @support_6ghz_band: support 6 GHz band
 */
struct policy_mgr_mac_ss_bw_info {
	uint32_t mac_tx_stream;
	uint32_t mac_rx_stream;
	uint32_t mac_bw;
	uint32_t mac_band_cap;
	bool support_6ghz_band;
};

#ifdef WLAN_FEATURE_SR
/**
 * policy_mgr_get_same_mac_conc_sr_status() - Function returns value of INI
 * g_enable_sr_in_same_mac_conc
 *
 * @psoc: Pointer to PSOC
 *
 * Return: Returns True / False
 */
bool policy_mgr_get_same_mac_conc_sr_status(struct wlan_objmgr_psoc *psoc);

#else
static inline
bool policy_mgr_get_same_mac_conc_sr_status(struct wlan_objmgr_psoc *psoc)
{
	return true;
}
#endif

struct policy_mgr_psoc_priv_obj *policy_mgr_get_context(
		struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_updated_scan_config() - Get the updated scan configuration
 * @psoc: psoc handle
 * @scan_config: Pointer containing the updated scan config
 * @dbs_scan: 0 or 1 indicating if DBS scan needs to be enabled/disabled
 * @dbs_plus_agile_scan: 0 or 1 indicating if DBS plus agile scan needs to be
 * enabled/disabled
 * @single_mac_scan_with_dfs: 0 or 1 indicating if single MAC scan with DFS
 * needs to be enabled/disabled
 *
 * Takes the current scan configuration and set the necessary scan config
 * bits to either 0/1 and provides the updated value to the caller who
 * can use this to pass it on to the FW
 *
 * Return: 0 on success
 */
QDF_STATUS policy_mgr_get_updated_scan_config(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *scan_config,
		bool dbs_scan,
		bool dbs_plus_agile_scan,
		bool single_mac_scan_with_dfs);

/**
 * policy_mgr_get_updated_fw_mode_config() - Get the updated fw
 * mode configuration
 * @psoc: psoc handle
 * @fw_mode_config: Pointer containing the updated fw mode config
 * @dbs: 0 or 1 indicating if DBS needs to be enabled/disabled
 * @agile_dfs: 0 or 1 indicating if agile DFS needs to be enabled/disabled
 *
 * Takes the current fw mode configuration and set the necessary fw mode config
 * bits to either 0/1 and provides the updated value to the caller who
 * can use this to pass it on to the FW
 *
 * Return: 0 on success
 */
QDF_STATUS policy_mgr_get_updated_fw_mode_config(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *fw_mode_config,
		bool dbs,
		bool agile_dfs);

/**
 * policy_mgr_is_dual_mac_disabled_in_ini() - Check if dual mac
 * is disabled in INI
 * @psoc: psoc handle
 *
 * Checks if the dual mac feature is disabled in INI
 *
 * Return: true if the dual mac connection is disabled from INI
 */
bool policy_mgr_is_dual_mac_disabled_in_ini(
		struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_find_if_hwlist_has_dbs() - Find if hw list has DBS modes or not
 * @psoc: PSOC object information
 *
 * Find if hw list has DBS modes or not
 *
 * Return: true or false
 */
bool policy_mgr_find_if_hwlist_has_dbs(struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_mcc_to_scc_switch_mode() - MCC to SCC
 * switch mode value in the user config
 * @psoc: PSOC object information
 *
 * MCC to SCC switch mode value in user config
 *
 * Return: MCC to SCC switch mode value
 */
uint32_t policy_mgr_get_mcc_to_scc_switch_mode(
	struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_dbs_config() - Get DBS bit
 * @psoc: psoc handle
 *
 * Gets the DBS bit of fw_mode_config_bits
 *
 * Return: 0 or 1 to indicate the DBS bit
 */
bool policy_mgr_get_dbs_config(struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_agile_dfs_config() - Get Agile DFS bit
 * @psoc: psoc handle
 *
 * Gets the Agile DFS bit of fw_mode_config_bits
 *
 * Return: 0 or 1 to indicate the Agile DFS bit
 */
bool policy_mgr_get_agile_dfs_config(struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_dbs_scan_config() - Get DBS scan bit
 * @psoc: psoc handle
 *
 * Gets the DBS scan bit of concurrent_scan_config_bits
 *
 * Return: 0 or 1 to indicate the DBS scan bit
 */
bool policy_mgr_get_dbs_scan_config(struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_tx_rx_ss_from_config() - Get Tx/Rx spatial
 * stream from HW mode config
 * @mac_ss: Config which indicates the HW mode as per 'hw_mode_ss_config'
 * @tx_ss: Contains the Tx spatial stream
 * @rx_ss: Contains the Rx spatial stream
 *
 * Returns the number of spatial streams of Tx and Rx
 *
 * Return: None
 */
void policy_mgr_get_tx_rx_ss_from_config(enum hw_mode_ss_config mac_ss,
		uint32_t *tx_ss, uint32_t *rx_ss);

/**
 * policy_mgr_get_matching_hw_mode_index() - Get matching HW mode index
 * @psoc: psoc handle
 * @mac0_tx_ss: Number of tx spatial streams of MAC0
 * @mac0_rx_ss: Number of rx spatial streams of MAC0
 * @mac0_bw: Bandwidth of MAC0 of type 'hw_mode_bandwidth'
 * @mac1_tx_ss: Number of tx spatial streams of MAC1
 * @mac1_rx_ss: Number of rx spatial streams of MAC1
 * @mac1_bw: Bandwidth of MAC1 of type 'hw_mode_bandwidth'
 * @mac0_band_cap: mac0 band capability requirement
 *     (0: Don't care, 1: 2.4G, 2: 5G)
 * @dbs: DBS capability of type 'hw_mode_dbs_capab'
 * @dfs: Agile DFS capability of type 'hw_mode_agile_dfs_capab'
 * @sbs: SBS capability of type 'hw_mode_sbs_capab'
 *
 * Fetches the HW mode index corresponding to the HW mode provided.
 * In Genoa two DBS HW modes (2x2 5G + 1x1 2G, 2x2 2G + 1x1 5G),
 * the "ss" number and "bw" value are not enough to specify the expected
 * HW mode. But in both HW mode, the mac0 can support either 5G or 2G.
 * So, the Parameter "mac0_band_cap" will specify the expected band support
 * requirement on mac 0 to find the expected HW mode.
 *
 * Return: Positive hw mode index in case a match is found or a negative
 * value, otherwise
 */
int8_t policy_mgr_get_matching_hw_mode_index(
		struct wlan_objmgr_psoc *psoc,
		uint32_t mac0_tx_ss, uint32_t mac0_rx_ss,
		enum hw_mode_bandwidth mac0_bw,
		uint32_t mac1_tx_ss, uint32_t mac1_rx_ss,
		enum hw_mode_bandwidth mac1_bw,
		enum hw_mode_mac_band_cap mac0_band_cap,
		enum hw_mode_dbs_capab dbs,
		enum hw_mode_agile_dfs_capab dfs,
		enum hw_mode_sbs_capab sbs);

/**
 * policy_mgr_get_hw_mode_idx_from_dbs_hw_list() - Get hw_mode index
 * @psoc: psoc handle
 * @mac0_ss: MAC0 spatial stream configuration
 * @mac0_bw: MAC0 bandwidth configuration
 * @mac1_ss: MAC1 spatial stream configuration
 * @mac1_bw: MAC1 bandwidth configuration
 * @mac0_band_cap: mac0 band capability requirement
 *     (0: Don't care, 1: 2.4G, 2: 5G)
 * @dbs: HW DBS capability
 * @dfs: HW Agile DFS capability
 * @sbs: HW SBS capability
 *
 * Get the HW mode index corresponding to the HW modes spatial stream,
 * bandwidth, DBS, Agile DFS and SBS capability
 *
 * In Genoa two DBS HW modes (2x2 5G + 1x1 2G, 2x2 2G + 1x1 5G),
 * the "ss" number and "bw" value are not enough to specify the expected
 * HW mode. But in both HW mode, the mac0 can support either 5G or 2G.
 * So, the Parameter "mac0_band_cap" will specify the expected band support
 * requirement on mac 0 to find the expected HW mode.
 *
 * Return: Index number if a match is found or -negative value if not found
 */
int8_t policy_mgr_get_hw_mode_idx_from_dbs_hw_list(
		struct wlan_objmgr_psoc *psoc,
		enum hw_mode_ss_config mac0_ss,
		enum hw_mode_bandwidth mac0_bw,
		enum hw_mode_ss_config mac1_ss,
		enum hw_mode_bandwidth mac1_bw,
		enum hw_mode_mac_band_cap mac0_band_cap,
		enum hw_mode_dbs_capab dbs,
		enum hw_mode_agile_dfs_capab dfs,
		enum hw_mode_sbs_capab sbs);

/**
 * policy_mgr_get_old_and_new_hw_index() - Get the old and new HW index
 * @psoc: psoc handle
 * @old_hw_mode_index: Value at this pointer contains the old HW mode index
 * Default value when not configured is POLICY_MGR_DEFAULT_HW_MODE_INDEX
 * @new_hw_mode_index: Value at this pointer contains the new HW mode index
 * Default value when not configured is POLICY_MGR_DEFAULT_HW_MODE_INDEX
 *
 * Get the old and new HW index configured in the driver
 *
 * Return: Failure in case the HW mode indices cannot be fetched and Success
 * otherwise. When no HW mode transition has happened the values of
 * old_hw_mode_index and new_hw_mode_index will be the same.
 */
QDF_STATUS policy_mgr_get_old_and_new_hw_index(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *old_hw_mode_index,
		uint32_t *new_hw_mode_index);

/**
 * policy_mgr_update_conc_list() - Update the concurrent connection list
 * @psoc: PSOC object information
 * @conn_index: Connection index
 * @mode: Mode
 * @freq: channel frequency
 * @bw: Bandwidth
 * @mac: Mac id
 * @chain_mask: Chain mask
 * @original_nss: Original number of spatial streams
 * @vdev_id: vdev id
 * @in_use: Flag to indicate if the index is in use or not
 * @update_conn: Flag to indicate if mode change event should
 *  be sent or not
 * @ch_flagext: channel state flags
 *
 * Updates the index value of the concurrent connection list
 *
 * Return: None
 */
void policy_mgr_update_conc_list(struct wlan_objmgr_psoc *psoc,
		uint32_t conn_index,
		enum policy_mgr_con_mode mode,
		uint32_t freq,
		enum hw_mode_bandwidth bw,
		uint8_t mac,
		enum policy_mgr_chain_mode chain_mask,
		uint32_t original_nss,
		uint32_t vdev_id,
		bool in_use,
		bool update_conn,
		uint16_t ch_flagext);

void policy_mgr_store_and_del_conn_info(struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				bool all_matching_cxn_to_del,
				struct policy_mgr_conc_connection_info *info,
				uint8_t *num_cxn_del);

/**
 * policy_mgr_store_and_del_conn_info_by_vdev_id() - Store and del a
 * connection info by vdev id
 * @psoc: PSOC object information
 * @vdev_id: vdev id whose entry has to be deleted
 * @info: structure array pointer where the connection info will be saved
 * @num_cxn_del: number of connection which are going to be deleted
 *
 * Saves the connection info corresponding to the provided mode
 * and deleted that corresponding entry based on vdev from the
 * connection info structure
 *
 * Return: None
 */
void policy_mgr_store_and_del_conn_info_by_vdev_id(
			struct wlan_objmgr_psoc *psoc,
			uint32_t vdev_id,
			struct policy_mgr_conc_connection_info *info,
			uint8_t *num_cxn_del);

/**
 * policy_mgr_store_and_del_conn_info_by_chan_and_mode() - Store and del a
 * connection info by chan number and conn mode
 * @psoc: PSOC object information
 * @ch_freq: channel frequency value
 * @mode: conn mode
 * @info: structure array pointer where the connection info will be saved
 * @num_cxn_del: number of connection which are going to be deleted
 *
 * Saves and deletes the entries if the active connection entry chan and mode
 * matches the provided chan & mode from the function parameters.
 *
 * Return: None
 */
void policy_mgr_store_and_del_conn_info_by_chan_and_mode(
			struct wlan_objmgr_psoc *psoc,
			uint32_t ch_freq,
			enum policy_mgr_con_mode mode,
			struct policy_mgr_conc_connection_info *info,
			uint8_t *num_cxn_del);

void policy_mgr_restore_deleted_conn_info(struct wlan_objmgr_psoc *psoc,
				struct policy_mgr_conc_connection_info *info,
				uint8_t num_cxn_del);
void policy_mgr_update_hw_mode_conn_info(struct wlan_objmgr_psoc *psoc,
				uint32_t num_vdev_mac_entries,
				struct policy_mgr_vdev_mac_map *vdev_mac_map,
				struct policy_mgr_hw_mode_params hw_mode,
				uint32_t num_mac_freq,
				struct policy_mgr_pdev_mac_freq_map *freq_info);
void policy_mgr_pdev_set_hw_mode_cb(uint32_t status,
				uint32_t cfgd_hw_mode_index,
				uint32_t num_vdev_mac_entries,
				struct policy_mgr_vdev_mac_map *vdev_mac_map,
				uint8_t next_action,
				enum policy_mgr_conn_update_reason reason,
				uint32_t session_id, void *context,
				uint32_t request_id);

#ifdef WLAN_FEATURE_11BE_MLO
void
policy_mgr_dump_disabled_ml_links(struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_link_switch_notifier_cb() - link switch notifier callback
 * @vdev: vdev object
 * @req: link switch request
 * @notify_reason: Reason for notification
 *
 * This API will be registered to mlo link switch, to be invoked before
 * do link switch process.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
policy_mgr_link_switch_notifier_cb(struct wlan_objmgr_vdev *vdev,
				   struct wlan_mlo_link_switch_req *req,
				   enum wlan_mlo_link_switch_notify_reason notify_reason);
#else
static inline void
policy_mgr_dump_disabled_ml_links(struct policy_mgr_psoc_priv_obj *pm_ctx) {}
#endif

/**
 * policy_mgr_dump_current_concurrency() - To dump the current
 * concurrency combination
 * @psoc: psoc handle
 *
 * This routine is called to dump the concurrency info
 *
 * Return: None
 */
void policy_mgr_dump_current_concurrency(struct wlan_objmgr_psoc *psoc);

void pm_dbs_opportunistic_timer_handler(void *data);

/**
 * policy_mgr_get_channel_list() - Get channel list based on PCL and mode
 * @psoc: psoc object
 * @pcl: pcl type
 * @mode: interface mode
 * @pcl_channels: pcl channel list buffer
 * @pcl_weights: pcl weight buffer
 * @pcl_sz: pcl channel list buffer size
 * @len: pcl channel number returned from API
 *
 * Return: QDF_STATUS
 */
QDF_STATUS policy_mgr_get_channel_list(struct wlan_objmgr_psoc *psoc,
				       enum policy_mgr_pcl_type pcl,
				       enum policy_mgr_con_mode mode,
				       uint32_t *pcl_channels,
				       uint8_t *pcl_weights,
				       uint32_t pcl_sz, uint32_t *len);

/**
 * policy_mgr_allow_new_home_channel() - Check for allowed number of
 * home channels
 * @psoc: PSOC Pointer
 * @mode: Connection mode
 * @ch_freq: channel frequency on which new connection is coming up
 * @num_connections: number of current connections
 * @is_dfs_ch: DFS channel or not
 * @ext_flags: extended flags for concurrency check
 *
 * When a new connection is about to come up check if current
 * concurrency combination including the new connection is
 * allowed or not based on the HW capability
 *
 * Return: True/False
 */
bool policy_mgr_allow_new_home_channel(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode,
	uint32_t ch_freq, uint32_t num_connections, bool is_dfs_ch,
	uint32_t ext_flags);

/**
 * policy_mgr_is_5g_channel_allowed() - check if 5g channel is allowed
 * @psoc: PSOC object information
 * @ch_freq: channel frequency which needs to be validated
 * @list: list of existing connections.
 * @mode: mode against which channel needs to be validated
 *
 * This API takes the channel frequency as input and compares with existing
 * connection channels. If existing connection's channel is DFS channel
 * and provided channel is 5G channel then don't allow concurrency to
 * happen as MCC with DFS channel is not yet supported
 *
 * Return: true if 5G channel is allowed, false if not allowed
 *
 */
bool policy_mgr_is_5g_channel_allowed(struct wlan_objmgr_psoc *psoc,
				uint32_t ch_freq, uint32_t *list,
				enum policy_mgr_con_mode mode);

/**
 * policy_mgr_complete_action() - initiates actions needed on
 * current connections once channel has been decided for the new
 * connection
 * @psoc: PSOC object information
 * @new_nss: the new nss value
 * @next_action: next action to happen at policy mgr after
 *		beacon update
 * @reason: Reason for connection update
 * @session_id: Session id
 * @request_id: connection manager req id
 *
 * This function initiates actions
 * needed on current connections once channel has been decided
 * for the new connection. Notifies UMAC & FW as well
 *
 * Return: QDF_STATUS enum
 */
QDF_STATUS policy_mgr_complete_action(struct wlan_objmgr_psoc *psoc,
				uint8_t  new_nss, uint8_t next_action,
				enum policy_mgr_conn_update_reason reason,
				uint32_t session_id, uint32_t request_id);

enum policy_mgr_con_mode policy_mgr_get_mode_by_vdev_id(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id);
QDF_STATUS policy_mgr_init_connection_update(
		struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_get_current_pref_hw_mode_dbs_2x2() - Get the
 * current preferred hw mode
 * @psoc: psoc handle
 *
 * Get the preferred hw mode based on the current connection combinations
 *
 * Return: No change (PM_NOP), MCC (PM_SINGLE_MAC),
 *         DBS (PM_DBS), SBS (PM_SBS)
 */
enum policy_mgr_conc_next_action
		policy_mgr_get_current_pref_hw_mode_dbs_2x2(
		struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_current_pref_hw_mode_dbs_1x1() - Get the
 * current preferred hw mode
 * @psoc: psoc handle
 *
 * Get the preferred hw mode based on the current connection combinations
 *
 * Return: No change (PM_NOP), MCC (PM_SINGLE_MAC_UPGRADE),
 *         DBS (PM_DBS_DOWNGRADE)
 */
enum policy_mgr_conc_next_action
		policy_mgr_get_current_pref_hw_mode_dbs_1x1(
		struct wlan_objmgr_psoc *psoc);

/**
 * policy_mgr_get_current_pref_hw_mode_dual_dbs() - Get the
 * current preferred hw mode
 * @psoc: PSOC object information
 *
 * Get the preferred hw mode based on the current connection combinations
 *
 * Return: No change (PM_NOP), (PM_SINGLE_MAC_UPGRADE),
 *         DBS (PM_DBS1_DOWNGRADE or PM_DBS2_DOWNGRADE)
 */
enum policy_mgr_conc_next_action
		policy_mgr_get_current_pref_hw_mode_dual_dbs(
		struct wlan_objmgr_psoc *psoc);

void
policy_mgr_dump_freq_range_per_mac(struct policy_mgr_freq_range *freq_range,
				   enum policy_mgr_mode hw_mode);

/**
 * policy_mgr_fill_curr_mac_freq_by_hwmode() - Fill Current Mac frequency with
 * the frequency range of the given Hw Mode
 *
 * @pm_ctx: Policy Mgr context
 * @mode_hw: Policy Mgr Hw mode
 *
 * Fill Current Mac frequency with the frequency range of the given Hw Mode
 *
 * Return: None
 */
void
policy_mgr_fill_curr_mac_freq_by_hwmode(struct policy_mgr_psoc_priv_obj *pm_ctx,
					enum policy_mgr_mode mode_hw);

/**
 * policy_mgr_dump_freq_range() - Function to print every frequency range
 * for both MAC 0 and MAC1 for every Hw mode
 *
 * @pm_ctx: Policy Mgr context
 *
 * This function will print every frequency range
 * for both MAC 0 and MAC1 for every Hw mode
 *
 * Return: void
 *
 */
void
policy_mgr_dump_freq_range(struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_dump_sbs_freq_range() - Function to print SBS frequency range
 * for both MAC 0 and MAC1
 *
 * @pm_ctx: Policy Mgr context
 *
 * Return: void
 */
void
policy_mgr_dump_sbs_freq_range(struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_dump_curr_freq_range() - Function to print current frequency range
 * for both MAC 0 and MAC1
 *
 * @pm_ctx: Policy Mgr context
 *
 * This function will print current frequency range
 * for both MAC 0 and MAC1 for every Hw mode
 *
 * Return: void
 *
 */
void
policy_mgr_dump_curr_freq_range(struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_reg_chan_change_callback() - Callback to be
 * invoked by regulatory module when valid channel list changes
 * @psoc: PSOC object information
 * @pdev: PDEV object information
 * @chan_list: New channel list
 * @avoid_freq_ind: LTE coex avoid channel list
 * @arg: Information passed at registration
 *
 * Get updated channel list from regulatory module
 *
 * Return: None
 */
void policy_mgr_reg_chan_change_callback(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev,
		struct regulatory_channel *chan_list,
		struct avoid_freq_ind_data *avoid_freq_ind,
		void *arg);

/**
 * policy_mgr_nss_update() - update nss for AP vdev
 * @psoc: PSOC object information
 * @new_nss: new NSS value
 * @next_action: Next action after nss update
 * @band: update AP vdev on the Band.
 * @reason: action reason
 * @original_vdev_id: original request hwmode change vdev id
 * @request_id: request id
 *
 * The function will update AP vdevs on specific band.
 *  eg. band = POLICY_MGR_ANY will request to update all band (2g and 5g)
 *
 * Return: QDF_STATUS_SUCCESS, update requested successfully.
 */
QDF_STATUS policy_mgr_nss_update(struct wlan_objmgr_psoc *psoc,
		uint8_t  new_nss, uint8_t next_action,
		enum policy_mgr_band band,
		enum policy_mgr_conn_update_reason reason,
		uint32_t original_vdev_id, uint32_t request_id);

/**
 * policy_mgr_is_concurrency_allowed() - Check for allowed
 * concurrency combination
 * @psoc: PSOC object information
 * @mode: new connection mode
 * @ch_freq: channel frequency on which new connection is coming up
 * @bw: Bandwidth requested by the connection (optional)
 * @ext_flags: extended flags for concurrency check (union conc_ext_flag)
 * @pcl: Optional PCL for new connection
 *
 * When a new connection is about to come up check if current
 * concurrency combination including the new connection is
 * allowed or not based on the HW capability, but no need to
 * invoke get_pcl
 *
 * Return: True/False
 */
bool policy_mgr_is_concurrency_allowed(struct wlan_objmgr_psoc *psoc,
				       enum policy_mgr_con_mode mode,
				       uint32_t ch_freq,
				       enum hw_mode_bandwidth bw,
				       uint32_t ext_flags,
				       struct policy_mgr_pcl_list *pcl);

/**
 * policy_mgr_can_2ghz_share_low_high_5ghz_sbs() - if SBS mode is dynamic where
 * 2.4 GHZ can be shared by any of high 5 GHZ or low 5GHZ at a time.
 * @pm_ctx: policy mgr psoc priv object
 *
 * Return: true is sbs is dynamic else false.
 */
bool policy_mgr_can_2ghz_share_low_high_5ghz_sbs(
				struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_sbs_24_shared_with_high_5() - if 2.4 GHZ
 * can be shared by high 5 GHZ
 *
 * @pm_ctx: policy mgr psoc priv object
 *
 * Return: true if 2.4 GHz is shared by high 5 GHZ
 */
bool
policy_mgr_sbs_24_shared_with_high_5(struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_sbs_24_shared_with_low_5() - if 2.4 GHZ
 * can be shared by low 5 GHZ
 *
 * @pm_ctx: policy mgr psoc priv object
 *
 * Return: true if 2.4 GHz is shared by low 5 GHZ
 */
bool
policy_mgr_sbs_24_shared_with_low_5(struct policy_mgr_psoc_priv_obj *pm_ctx);

/**
 * policy_mgr_2_freq_same_mac_in_dbs() - to check provided frequencies are
 * in dbs freq range or not
 *
 * @pm_ctx: policy mgr psoc priv object
 * @freq_1: first frequency
 * @freq_2: second frequency
 *
 * This API is used to check provided frequencies are in dbs freq range or not
 *
 * Return: true/false.
 */
bool
policy_mgr_2_freq_same_mac_in_dbs(struct policy_mgr_psoc_priv_obj *pm_ctx,
				  qdf_freq_t freq_1, qdf_freq_t freq_2);

/**
 * policy_mgr_2_freq_same_mac_in_sbs() - to check provided frequencies are
 * in sbs freq range or not
 *
 * @pm_ctx: policy mgr psoc priv object
 * @freq_1: first frequency
 * @freq_2: second frequency
 *
 * This API is used to check provided frequencies are in sbs freq range or not
 *
 * Return: true/false.
 */
bool policy_mgr_2_freq_same_mac_in_sbs(struct policy_mgr_psoc_priv_obj *pm_ctx,
				       qdf_freq_t freq_1, qdf_freq_t freq_2);

/**
 * policy_mgr_get_connection_for_vdev_id() - provides the
 * particular connection with the requested vdev id
 * @psoc: PSOC object information
 * @vdev_id: vdev id of the connection
 *
 * This function provides the specific connection with the
 * requested vdev id
 *
 * Return: index in the connection table
 */
uint32_t policy_mgr_get_connection_for_vdev_id(struct wlan_objmgr_psoc *psoc,
					       uint32_t vdev_id);

#ifdef FEATURE_WLAN_CH_AVOID_EXT
/**
 * policy_mgr_set_freq_restriction_mask() - fill the restriction_mask
 * in pm_ctx
 *
 * @pm_ctx: policy mgr psoc priv object
 * @freq_list: avoid freq indication carries freq/mask/freq count
 *
 * Return: None
 */
void
policy_mgr_set_freq_restriction_mask(struct policy_mgr_psoc_priv_obj *pm_ctx,
				     struct ch_avoid_ind_type *freq_list);

/**
 * policy_mgr_get_freq_restriction_mask() - get restriction_mask from
 * pm_ctx
 *
 * @pm_ctx: policy mgr psoc priv object
 *
 * Return: Restriction mask
 */
uint32_t
policy_mgr_get_freq_restriction_mask(struct policy_mgr_psoc_priv_obj *pm_ctx);
#else
static inline void
policy_mgr_set_freq_restriction_mask(struct policy_mgr_psoc_priv_obj *pm_ctx,
				     struct ch_avoid_ind_type *freq_list)
{
}
#endif

/**
 * policy_mgr_get_connection_max_channel_width() - Get max channel width
 * among vdevs in use
 * @psoc: PSOC object pointer
 *
 * This function returns max channel width among in-use vdevs
 *
 * Return: enum hw_mode_bandwidth
 */
enum hw_mode_bandwidth
policy_mgr_get_connection_max_channel_width(struct wlan_objmgr_psoc *psoc);

#endif
