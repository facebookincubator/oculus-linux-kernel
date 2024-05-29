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

#ifndef __WLAN_POLICY_MGR_PUBLIC_STRUCT_H
#define __WLAN_POLICY_MGR_PUBLIC_STRUCT_H

/**
 * DOC: wlan_policy_mgr_public_struct.h
 *
 * Concurrenct Connection Management entity
 */

/* Include files */
#include <wmi_unified_api.h>

/*
 *  Some max value greater than the max length of the channel list
 */
#define MAX_WEIGHT_OF_PCL_CHANNELS 255
/*
 *  Some fixed weight difference between the groups
 */
#define PCL_GROUPS_WEIGHT_DIFFERENCE 20

/*
 * Currently max, only 3 groups are possible as per 'enum policy_mgr_pcl_type'.
 * i.e., in a PCL only 3 groups of channels can be present
 * e.g., SCC channel on 2.4 Ghz, SCC channel on 5 Ghz & 5 Ghz channels.
 * Group 1 has highest priority, group 2 has the next higher priority
 * and so on.
 */
#define WEIGHT_OF_GROUP1_PCL_CHANNELS MAX_WEIGHT_OF_PCL_CHANNELS
#define WEIGHT_OF_GROUP2_PCL_CHANNELS \
	(WEIGHT_OF_GROUP1_PCL_CHANNELS - PCL_GROUPS_WEIGHT_DIFFERENCE)
#define WEIGHT_OF_GROUP3_PCL_CHANNELS \
	(WEIGHT_OF_GROUP2_PCL_CHANNELS - PCL_GROUPS_WEIGHT_DIFFERENCE)
#define WEIGHT_OF_GROUP4_PCL_CHANNELS \
	(WEIGHT_OF_GROUP3_PCL_CHANNELS - PCL_GROUPS_WEIGHT_DIFFERENCE)

#define WEIGHT_OF_NON_PCL_CHANNELS 1
#define WEIGHT_OF_DISALLOWED_CHANNELS 0

#define MAX_MAC 2

#ifdef FEATURE_FOURTH_CONNECTION
#define MAX_NUMBER_OF_CONC_CONNECTIONS 4
#else
#define MAX_NUMBER_OF_CONC_CONNECTIONS 3
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/* Max MLO VDEVS - 1 as all vdevs cannot be disabled */
#define MAX_NUMBER_OF_DISABLE_LINK WLAN_UMAC_MLO_MAX_VDEVS - 1
#endif

/* BIT 0 for low latency and BIT 1 for HIGH TPUT */
#define PM_VDEV_TRAFFIC_LOW_LATENCY 0x1
#define PM_VDEV_TRAFFIC_HIGH_TPUT 0x2

/* Policy manager default request id */
#define POLICY_MGR_DEF_REQ_ID 0

typedef int (*send_mode_change_event_cb)(void);

/**
 * enum sap_csa_reason_code - SAP channel switch reason code
 * @CSA_REASON_UNKNOWN: Unknown reason
 * @CSA_REASON_STA_CONNECT_DFS_TO_NON_DFS: STA connection from DFS to NON DFS.
 * @CSA_REASON_USER_INITIATED: User initiated form north bound.
 * @CSA_REASON_PEER_ACTION_FRAME: Action frame received on sta iface.
 * @CSA_REASON_PRE_CAC_SUCCESS: Pre CAC success.
 * @CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL: concurrent sta changed channel.
 * @CSA_REASON_UNSAFE_CHANNEL: Unsafe channel.
 * @CSA_REASON_LTE_COEX: LTE coex.
 * @CSA_REASON_CONCURRENT_NAN_EVENT: NAN concurrency.
 * @CSA_REASON_BAND_RESTRICTED: band disabled or re-enabled
 * @CSA_REASON_DCS: DCS
 * @CSA_REASON_CHAN_DISABLED: channel is disabled
 * @CSA_REASON_CHAN_PASSIVE: channel is passive
 * @CSA_REASON_GO_BSS_STARTED: P2P go started
 * @CSA_REASON_SAP_ACS: 2.4 GHz preferred SAP ACS starting
 * @CSA_REASON_SAP_FIX_CH_CONC_WITH_GO: SAP fix channel start
 *  and move GO to other band
 */
enum sap_csa_reason_code {
	CSA_REASON_UNKNOWN,
	CSA_REASON_STA_CONNECT_DFS_TO_NON_DFS,
	CSA_REASON_USER_INITIATED,
	CSA_REASON_PEER_ACTION_FRAME,
	CSA_REASON_PRE_CAC_SUCCESS,
	CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL,
	CSA_REASON_UNSAFE_CHANNEL,
	CSA_REASON_LTE_COEX,
	CSA_REASON_CONCURRENT_NAN_EVENT,
	CSA_REASON_BAND_RESTRICTED,
	CSA_REASON_DCS,
	CSA_REASON_CHAN_DISABLED,
	CSA_REASON_CHAN_PASSIVE,
	CSA_REASON_GO_BSS_STARTED,
	CSA_REASON_SAP_ACS,
	CSA_REASON_SAP_FIX_CH_CONC_WITH_GO
};

/**
 * enum hw_mode_ss_config - Possible spatial stream configuration
 * @HW_MODE_SS_0x0: Unused Tx and Rx of MAC
 * @HW_MODE_SS_1x1: 1 Tx SS and 1 Rx SS
 * @HW_MODE_SS_2x2: 2 Tx SS and 2 Rx SS
 * @HW_MODE_SS_3x3: 3 Tx SS and 3 Rx SS
 * @HW_MODE_SS_4x4: 4 Tx SS and 4 Rx SS
 *
 * Note: Right now only 1x1 and 2x2 are being supported. Other modes should
 * be added when supported. Asymmetric configuration like 1x2, 2x1 are also
 * not supported now. But, they are still valid. Right now, Tx/Rx SS support is
 * 4 bits long. So, we can go upto 15x15
 */
enum hw_mode_ss_config {
	HW_MODE_SS_0x0,
	HW_MODE_SS_1x1,
	HW_MODE_SS_2x2,
	HW_MODE_SS_3x3,
	HW_MODE_SS_4x4,
};

/**
 * enum hw_mode_dbs_capab - DBS HW mode capability
 * @HW_MODE_DBS_NONE: Non DBS capable
 * @HW_MODE_DBS: DBS capable
 */
enum hw_mode_dbs_capab {
	HW_MODE_DBS_NONE,
	HW_MODE_DBS,
};

/**
 * enum hw_mode_agile_dfs_capab - Agile DFS HW mode capability
 * @HW_MODE_AGILE_DFS_NONE: Non Agile DFS capable
 * @HW_MODE_AGILE_DFS: Agile DFS capable
 */
enum hw_mode_agile_dfs_capab {
	HW_MODE_AGILE_DFS_NONE,
	HW_MODE_AGILE_DFS,
};

/**
 * enum hw_mode_sbs_capab - SBS HW mode capability
 * @HW_MODE_SBS_NONE: Non SBS capable
 * @HW_MODE_SBS: SBS capable
 */
enum hw_mode_sbs_capab {
	HW_MODE_SBS_NONE,
	HW_MODE_SBS,
};

/**
 * enum hw_mode_emlsr_capab - EMLSR HW mode capability
 * @HW_MODE_EMLSR_NONE: Non EMLSR capable
 * @HW_MODE_EMLSR: EMLSR capable
 */
enum hw_mode_emlsr_capab {
	HW_MODE_EMLSR_NONE,
	HW_MODE_EMLSR,
};

/**
 * enum hw_mode_mac_band_cap - mac band capability
 * @HW_MODE_MAC_BAND_NONE: No band requirement.
 * @HW_MODE_MAC_BAND_2G: 2G band supported.
 * @HW_MODE_MAC_BAND_5G: 5G band supported.
 *
 * To add HW_MODE_MAC_BAND_NONE value to help to
 * match the HW DBS mode in hw mode list.
 * Other enum values should match with WMI header:
 * typedef enum {
 *   WLAN_2G_CAPABILITY = 0x1,
 *   WLAN_5G_CAPABILITY = 0x2,
 * } WLAN_BAND_CAPABILITY;
 */
enum hw_mode_mac_band_cap {
	HW_MODE_MAC_BAND_NONE = 0,
	HW_MODE_MAC_BAND_2G = WLAN_2G_CAPABILITY,
	HW_MODE_MAC_BAND_5G = WLAN_5G_CAPABILITY,
};

/**
 * enum force_1x1_type - enum to specify the type of forced 1x1 ini provided.
 * @FORCE_1X1_DISABLED: even if the AP is present in OUI, 1x1 will not be forced
 * @FORCE_1X1_ENABLED_FOR_AS: If antenna sharing supported, then only do 1x1.
 * @FORCE_1X1_ENABLED_FORCED: If AP present in OUI, force 1x1 connection.
 */
enum force_1x1_type {
	FORCE_1X1_DISABLED,
	FORCE_1X1_ENABLED_FOR_AS,
	FORCE_1X1_ENABLED_FORCED,
};

/**
 * enum policy_mgr_pcl_group_id - Identifies the pcl groups to be used
 * @POLICY_MGR_PCL_GROUP_ID1_ID2: Use weights of group1 and group2
 * @POLICY_MGR_PCL_GROUP_ID2_ID3: Use weights of group2 and group3
 * @POLICY_MGR_PCL_GROUP_ID3_ID4: Use weights of group3 and group4
 *
 * Since maximum of three groups are possible, this will indicate which
 * PCL group needs to be used.
 */
enum policy_mgr_pcl_group_id {
	POLICY_MGR_PCL_GROUP_ID1_ID2,
	POLICY_MGR_PCL_GROUP_ID2_ID3,
	POLICY_MGR_PCL_GROUP_ID3_ID4,
};

/**
 * enum policy_mgr_pcl_channel_order - Order in which the PCL is requested
 * @POLICY_MGR_PCL_ORDER_NONE: no order
 * @POLICY_MGR_PCL_ORDER_24G_THEN_5G: 2.4 Ghz channel followed by 5 Ghz channel
 * @POLICY_MGR_PCL_ORDER_5G_THEN_2G: 5 Ghz channel followed by 2.4 Ghz channel
 * @POLICY_MGR_PCL_ORDER_2G: 2G channels
 * @POLICY_MGR_PCL_ORDER_5G: 5G channels
 * @POLICY_MGR_PCL_ORDER_SCC_5G_LOW_5G_LOW: 5G Low SCC frequency followed by
 * 5G low band i.e 5G freq < sbs cutoff freq
 * @POLICY_MGR_PCL_ORDER_SCC_5G_HIGH_5G_HIGH: 5G High SCC frequency followed by
 * 5G High band i.e 5G freq > sbs cutoff freq
 *
 * Order in which the PCL is requested
 */
enum policy_mgr_pcl_channel_order {
	POLICY_MGR_PCL_ORDER_NONE,
	POLICY_MGR_PCL_ORDER_24G_THEN_5G,
	POLICY_MGR_PCL_ORDER_5G_THEN_2G,
	POLICY_MGR_PCL_ORDER_2G,
	POLICY_MGR_PCL_ORDER_5G,
	POLICY_MGR_PCL_ORDER_SCC_5G_LOW_5G_LOW,
	POLICY_MGR_PCL_ORDER_SCC_5G_HIGH_5G_HIGH,
};

/**
 * enum policy_mgr_pcl_band_priority - Band priority between 5G and 6G channel
 * @POLICY_MGR_PCL_BAND_5G_THEN_6G: 5 Ghz channel followed by 6 Ghz channel
 * @POLICY_MGR_PCL_BAND_6G_THEN_5G: 6 Ghz channel followed by 5 Ghz channel
 *
 *  Band priority between 5G and 6G
 */
enum policy_mgr_pcl_band_priority {
	POLICY_MGR_PCL_BAND_5G_THEN_6G = 0,
	POLICY_MGR_PCL_BAND_6G_THEN_5G,
};

/**
 * enum policy_mgr_max_rx_ss - Maximum number of receive spatial streams
 * @POLICY_MGR_RX_NSS_1: Receive Nss = 1
 * @POLICY_MGR_RX_NSS_2: Receive Nss = 2
 * @POLICY_MGR_RX_NSS_3: Receive Nss = 3
 * @POLICY_MGR_RX_NSS_4: Receive Nss = 4
 * @POLICY_MGR_RX_NSS_5: Receive Nss = 5
 * @POLICY_MGR_RX_NSS_6: Receive Nss = 6
 * @POLICY_MGR_RX_NSS_7: Receive Nss = 7
 * @POLICY_MGR_RX_NSS_8: Receive Nss = 8
 * @POLICY_MGR_RX_NSS_MAX: maximum enumeration
 *
 * Indicates the maximum number of spatial streams that the STA can receive
 */
enum policy_mgr_max_rx_ss {
	POLICY_MGR_RX_NSS_1 = 0,
	POLICY_MGR_RX_NSS_2 = 1,
	POLICY_MGR_RX_NSS_3 = 2,
	POLICY_MGR_RX_NSS_4 = 3,
	POLICY_MGR_RX_NSS_5 = 4,
	POLICY_MGR_RX_NSS_6 = 5,
	POLICY_MGR_RX_NSS_7 = 6,
	POLICY_MGR_RX_NSS_8 = 7,
	POLICY_MGR_RX_NSS_MAX,
};

/**
 * enum policy_mgr_chain_mode - Chain Mask tx & rx combination.
 *
 * @POLICY_MGR_ONE_ONE: One for Tx, One for Rx
 * @POLICY_MGR_TWO_TWO: Two for Tx, Two for Rx
 * @POLICY_MGR_MAX_NO_OF_CHAIN_MODE: Max place holder
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum policy_mgr_chain_mode {
	POLICY_MGR_ONE_ONE = 0,
	POLICY_MGR_TWO_TWO,
	POLICY_MGR_MAX_NO_OF_CHAIN_MODE
};

/**
 * enum policy_mgr_conc_priority_mode - t/p, powersave, latency.
 *
 * @PM_THROUGHPUT: t/p is the priority
 * @PM_POWERSAVE: powersave is the priority
 * @PM_LATENCY: latency is the priority
 * @PM_MAX_CONC_PRIORITY_MODE: Max place holder
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum policy_mgr_conc_priority_mode {
	PM_THROUGHPUT = 0,
	PM_POWERSAVE,
	PM_LATENCY,
	PM_MAX_CONC_PRIORITY_MODE
};

/**
 * enum policy_mgr_con_mode - concurrency mode for PCL table
 *
 * @PM_STA_MODE: station mode
 * @PM_SAP_MODE: SAP mode
 * @PM_P2P_CLIENT_MODE: P2P client mode
 * @PM_P2P_GO_MODE: P2P Go mode
 * @PM_NDI_MODE: NDI mode
 * @PM_NAN_DISC_MODE: NAN Discovery mode
 * @PM_MAX_NUM_OF_MODE: max value place holder
 */
enum policy_mgr_con_mode {
	PM_STA_MODE = 0,
	PM_SAP_MODE,
	PM_P2P_CLIENT_MODE,
	PM_P2P_GO_MODE,
	PM_NDI_MODE,
	PM_NAN_DISC_MODE,
	PM_MAX_NUM_OF_MODE
};

/**
 * enum policy_mgr_mac_use - MACs that are used
 * @POLICY_MGR_MAC0: Only MAC0 is used
 * @POLICY_MGR_MAC1: Only MAC1 is used
 * @POLICY_MGR_MAC0_AND_MAC1: Both MAC0 and MAC1 are used
 */
enum policy_mgr_mac_use {
	POLICY_MGR_MAC0 = 1,
	POLICY_MGR_MAC1 = 2,
	POLICY_MGR_MAC0_AND_MAC1 = 3
};

/**
 * enum policy_mgr_pcl_type - Various types of Preferred channel list (PCL).
 *
 * @PM_NONE: No channel preference
 * @PM_24G: 2.4 Ghz channels only
 * @PM_5G: 5 Ghz channels only
 * @PM_SCC_CH: SCC channel only
 * @PM_MCC_CH: MCC channels only
 * @PM_SBS_CH: SBS channels only
 * @PM_SCC_CH_24G: SCC channel & 2.4 Ghz channels
 * @PM_SCC_CH_5G: SCC channel & 5 Ghz channels
 * @PM_24G_SCC_CH: 2.4 Ghz channels & SCC channel
 * @PM_5G_SCC_CH: 5 Ghz channels & SCC channel
 * @PM_SCC_ON_5_CH_5G: 5 Ghz SCC channel & 5 Ghz channels
 * @PM_SCC_ON_5_SCC_ON_24_24G: SCC channel on 5 Ghz, SCC
 *	channel on 2.4 Ghz & 2.4 Ghz channels
 * @PM_SCC_ON_5_SCC_ON_24_5G: SCC channel on 5 Ghz, SCC channel
 *	on 2.4 Ghz & 5 Ghz channels
 * @PM_SCC_ON_5_5G_24G: SCC channel on 5 Ghz, 5 Ghz channels & 2.4 Ghz channels
 * @PM_SCC_ON_5_5G_SCC_ON_24G: SCC channel on 5 Ghz, 5 Ghz channels &
 *	SCC channel on 2.4 Ghz
 * @PM_SCC_ON_24_SCC_ON_5_24G: SCC channel on 2.4 Ghz, SCC
 *	channel on 5 Ghz & 2.4 Ghz channels
 * @PM_SCC_ON_24_SCC_ON_5_5G: SCC channel on 2.4 Ghz, SCC
 *	channel on 5 Ghz & 5 Ghz channels
 * @PM_SCC_ON_24_CH_24G: SCC channel on 2.4 GHz & 2.4 GHz channels
 * @PM_SCC_ON_5_SCC_ON_24: SCC channel on 5 Ghz, SCC channel on
 *	2.4 Ghz
 * @PM_SCC_ON_24_SCC_ON_5: SCC channel on 2.4 Ghz, SCC channel
 *	on 5 Ghz
 * @PM_MCC_CH_24G: MCC channels & 2.4 Ghz channels
 * @PM_MCC_CH_5G:  MCC channels & 5 Ghz channels
 * @PM_24G_MCC_CH: 2.4 Ghz channels & MCC channels
 * @PM_5G_MCC_CH: 5 Ghz channels & MCC channels
 * @PM_SBS_CH_5G: SBS channels & rest of 5 Ghz channels
 * @PM_24G_SCC_CH_SBS_CH: 2.4 Ghz channels, SCC channel & SBS channels
 * @PM_24G_SCC_CH_SBS_CH_5G: 2.4 Ghz channels, SCC channel,
 *      SBS channels & rest of the 5G channels
 * @PM_24G_SBS_CH_MCC_CH: 2.4 Ghz channels, SBS channels & MCC channels
 * @PM_SBS_CH_24G_SCC_CH:
 * @PM_SBS_CH_SCC_CH_24G:
 * @PM_SCC_CH_SBS_CH_24G:
 * @PM_SBS_CH_SCC_CH_5G_24G:
 * @PM_SCC_CH_MCC_CH_SBS_CH_24G:
 * @PM_SBS_CH_2G: SBS channels & 2.4 Ghz channels
 * @PM_SCC_ON_5G_LOW_5G_LOW_PLUS_SHARED_2G: 5 GHz low SCC channel followed by
 * 5 GHz low frequencies, add 2.4 GHz if its shared with 5 GHz low
 * @PM_SCC_ON_5G_HIGH_5G_HIGH_PLUS_SHARED_2G: 5GHZ high SCC channel followed by
 * 5 GHz high frequencies, add 2.4 GHZ if its shared with 5GHz high
 *
 * @PM_MAX_PCL_TYPE: Max place holder
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum policy_mgr_pcl_type {
	PM_NONE = 0,
	PM_24G,
	PM_5G,
	PM_SCC_CH,
	PM_MCC_CH,
	PM_SBS_CH,
	PM_SCC_CH_24G,
	PM_SCC_CH_5G,
	PM_24G_SCC_CH,
	PM_5G_SCC_CH,
	PM_SCC_ON_5_CH_5G,
	PM_SCC_ON_5_SCC_ON_24_24G,
	PM_SCC_ON_5_SCC_ON_24_5G,
	PM_SCC_ON_5_5G_24G,
	PM_SCC_ON_5_5G_SCC_ON_24G,
	PM_SCC_ON_24_SCC_ON_5_24G,
	PM_SCC_ON_24_SCC_ON_5_5G,
	PM_SCC_ON_24_CH_24G,
	PM_SCC_ON_5_SCC_ON_24,
	PM_SCC_ON_24_SCC_ON_5,
	PM_MCC_CH_24G,
	PM_MCC_CH_5G,
	PM_24G_MCC_CH,
	PM_5G_MCC_CH,
	PM_SBS_CH_5G,
	PM_24G_SCC_CH_SBS_CH,
	PM_24G_SCC_CH_SBS_CH_5G,
	PM_24G_SBS_CH_MCC_CH,
	/* New PCL type for DBS-SBS HW */
	PM_SBS_CH_24G_SCC_CH,
	PM_SBS_CH_SCC_CH_24G,
	PM_SCC_CH_SBS_CH_24G,
	PM_SBS_CH_SCC_CH_5G_24G,
	PM_SCC_CH_MCC_CH_SBS_CH_24G,
	PM_SBS_CH_2G,
	PM_SCC_ON_5G_LOW_5G_LOW_PLUS_SHARED_2G,
	PM_SCC_ON_5G_HIGH_5G_HIGH_PLUS_SHARED_2G,

	PM_MAX_PCL_TYPE
};

/**
 * enum policy_mgr_one_connection_mode - Combination of first connection
 * type, band & spatial stream used.
 *
 * @PM_STA_24_1x1: STA connection using 1x1@2.4 Ghz
 * @PM_STA_24_2x2: STA connection using 2x2@2.4 Ghz
 * @PM_STA_5_1x1: STA connection using 1x1@5 Ghz
 * @PM_STA_5_2x2: STA connection using 2x2@5 Ghz
 * @PM_P2P_CLI_24_1x1: P2P Client connection using 1x1@2.4 Ghz
 * @PM_P2P_CLI_24_2x2: P2P Client connection using 2x2@2.4 Ghz
 * @PM_P2P_CLI_5_1x1: P2P Client connection using 1x1@5 Ghz
 * @PM_P2P_CLI_5_2x2: P2P Client connection using 2x2@5 Ghz
 * @PM_P2P_GO_24_1x1: P2P GO connection using 1x1@2.4 Ghz
 * @PM_P2P_GO_24_2x2: P2P GO connection using 2x2@2.4 Ghz
 * @PM_P2P_GO_5_1x1: P2P GO connection using 1x1@5 Ghz
 * @PM_P2P_GO_5_2x2: P2P GO connection using 2x2@5 Ghz
 * @PM_SAP_24_1x1: SAP connection using 1x1@2.4 Ghz
 * @PM_SAP_24_2x2: SAP connection using 2x2@2.4 Ghz
 * @PM_SAP_5_1x1: SAP connection using 1x1@5 Ghz
 * @PM_SAP_5_2x2: SAP connection using 2x2@5 Ghz
 * @PM_NAN_DISC_24_1x1:  NAN Discovery using 1x1@2.4 Ghz
 * @PM_NAN_DISC_24_2x2:  NAN Discovery using 2x2@2.4 Ghz
 * @PM_NDI_24_1x1:  NAN Datapath using 1x1@2.4 Ghz
 * @PM_NDI_24_2x2:  NAN Datapath using 2x2@2.4 Ghz
 * @PM_NDI_5_1x1:  NAN Datapath using 1x1@5 Ghz
 * @PM_NDI_5_2x2:  NAN Datapath using 2x2@5 Ghz
 * @PM_MAX_ONE_CONNECTION_MODE: Max place holder
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum policy_mgr_one_connection_mode {
	PM_STA_24_1x1 = 0,
	PM_STA_24_2x2,
	PM_STA_5_1x1,
	PM_STA_5_2x2,
	PM_P2P_CLI_24_1x1,
	PM_P2P_CLI_24_2x2,
	PM_P2P_CLI_5_1x1,
	PM_P2P_CLI_5_2x2,
	PM_P2P_GO_24_1x1,
	PM_P2P_GO_24_2x2,
	PM_P2P_GO_5_1x1,
	PM_P2P_GO_5_2x2,
	PM_SAP_24_1x1,
	PM_SAP_24_2x2,
	PM_SAP_5_1x1,
	PM_SAP_5_2x2,
	PM_NAN_DISC_24_1x1,
	PM_NAN_DISC_24_2x2,
	PM_NDI_24_1x1,
	PM_NDI_24_2x2,
	PM_NDI_5_1x1,
	PM_NDI_5_2x2,

	PM_MAX_ONE_CONNECTION_MODE
};

/**
 * enum policy_mgr_two_connection_mode - Combination of first two
 * connections type, concurrency state, band & spatial stream
 * used.
 *
 * @PM_STA_SAP_SCC_24_1x1: STA & SAP connection on SCC using
 *			1x1@2.4 Ghz
 * @PM_STA_SAP_SCC_24_2x2: STA & SAP connection on SCC using
 *			2x2@2.4 Ghz
 * @PM_STA_SAP_MCC_24_1x1: STA & SAP connection on MCC using
 *			1x1@2.4 Ghz
 * @PM_STA_SAP_MCC_24_2x2: STA & SAP connection on MCC using
 *			2x2@2.4 Ghz
 * @PM_STA_SAP_SCC_5_1x1: STA & SAP connection on SCC using
 *			1x1@5 Ghz
 * @PM_STA_SAP_SCC_5_2x2: STA & SAP connection on SCC using
 *			2x2@5 Ghz
 * @PM_STA_SAP_MCC_5_1x1: STA & SAP connection on MCC using
 *			1x1@5 Ghz
 * @PM_STA_SAP_MCC_5_2x2: STA & SAP connection on MCC using
 *			2x2@5 Ghz
 * @PM_STA_SAP_MCC_24_5_1x1:
 * @PM_STA_SAP_MCC_24_5_2x2:
 * @PM_STA_SAP_DBS_1x1: STA & SAP connection on DBS using 1x1
 * @PM_STA_SAP_DBS_2x2: STA & SAP connection on DBS using 2x2
 * @PM_STA_SAP_SBS_5_1x1: STA & SAP connection on 5G SBS using 1x1
 * @PM_STA_SAP_SBS_5_2x2: STA & SAP connection on 5G SBS using 2x2
 * @PM_STA_P2P_GO_SCC_24_1x1: STA & P2P GO connection on SCC
 *			using 1x1@2.4 Ghz
 * @PM_STA_P2P_GO_SCC_24_2x2: STA & P2P GO connection on SCC
 *			using 2x2@2.4 Ghz
 * @PM_STA_P2P_GO_MCC_24_1x1: STA & P2P GO connection on MCC
 *			using 1x1@2.4 Ghz
 * @PM_STA_P2P_GO_MCC_24_2x2: STA & P2P GO connection on MCC
 *			using 2x2@2.4 Ghz
 * @PM_STA_P2P_GO_SCC_5_1x1: STA & P2P GO connection on SCC
 *			using 1x1@5 Ghz
 * @PM_STA_P2P_GO_SCC_5_2x2: STA & P2P GO connection on SCC
 *			using 2x2@5 Ghz
 * @PM_STA_P2P_GO_MCC_5_1x1: STA & P2P GO connection on MCC
 *			using 1x1@5 Ghz
 * @PM_STA_P2P_GO_MCC_5_2x2: STA & P2P GO connection on MCC
 *			using 2x2@5 Ghz
 * @PM_STA_P2P_GO_MCC_24_5_1x1:
 * @PM_STA_P2P_GO_MCC_24_5_2x2:
 * @PM_STA_P2P_GO_DBS_1x1: STA & P2P GO connection on DBS using
 *			1x1
 * @PM_STA_P2P_GO_DBS_2x2: STA & P2P GO connection on DBS using
 *			2x2
 * @PM_STA_P2P_GO_SBS_5_1x1: STA & P2P GO connection on 5G SBS
 *			using 1x1
 * @PM_STA_P2P_GO_SBS_5_2x2: STA & P2P GO connection on 5G SBS
 *			using 2x2
 * @PM_STA_P2P_CLI_SCC_24_1x1: STA & P2P CLI connection on SCC
 *			using 1x1@2.4 Ghz
 * @PM_STA_P2P_CLI_SCC_24_2x2: STA & P2P CLI connection on SCC
 *			using 2x2@2.4 Ghz
 * @PM_STA_P2P_CLI_MCC_24_1x1: STA & P2P CLI connection on MCC
 *			using 1x1@2.4 Ghz
 * @PM_STA_P2P_CLI_MCC_24_2x2: STA & P2P CLI connection on MCC
 *			using 2x2@2.4 Ghz
 * @PM_STA_P2P_CLI_SCC_5_1x1: STA & P2P CLI connection on SCC
 *			using 1x1@5 Ghz
 * @PM_STA_P2P_CLI_SCC_5_2x2: STA & P2P CLI connection on SCC
 *			using 2x2@5 Ghz
 * @PM_STA_P2P_CLI_MCC_5_1x1: STA & P2P CLI connection on MCC
 *			using 1x1@5 Ghz
 * @PM_STA_P2P_CLI_MCC_5_2x2: STA & P2P CLI connection on MCC
 *			using 2x2@5 Ghz
 * @PM_STA_P2P_CLI_MCC_24_5_1x1:
 * @PM_STA_P2P_CLI_MCC_24_5_2x2:
 * @PM_STA_P2P_CLI_DBS_1x1: STA & P2P CLI connection on DBS
 *			using 1x1
 * @PM_STA_P2P_CLI_DBS_2x2: STA & P2P CLI connection on DBS
 *			using 2x2
 * @PM_STA_P2P_CLI_SBS_5_1x1: STA & P2P CLI connection on 5G
 *			SBS using 1x1
 * @PM_STA_P2P_CLI_SBS_5_2x2: STA & P2P CLI connection on 5G
 *			SBS using 2x2
 * @PM_P2P_GO_P2P_CLI_SCC_24_1x1: P2P GO & CLI connection on
 *			SCC using 1x1@2.4 Ghz
 * @PM_P2P_GO_P2P_CLI_SCC_24_2x2: P2P GO & CLI connection on
 *			SCC using 2x2@2.4 Ghz
 * @PM_P2P_GO_P2P_CLI_MCC_24_1x1: P2P GO & CLI connection on
 *			MCC using 1x1@2.4 Ghz
 * @PM_P2P_GO_P2P_CLI_MCC_24_2x2: P2P GO & CLI connection on
 *			MCC using 2x2@2.4 Ghz
 * @PM_P2P_GO_P2P_CLI_SCC_5_1x1: P2P GO & CLI connection on
 *			SCC using 1x1@5 Ghz
 * @PM_P2P_GO_P2P_CLI_SCC_5_2x2: P2P GO & CLI connection on
 *			SCC using 2x2@5 Ghz
 * @PM_P2P_GO_P2P_CLI_MCC_5_1x1: P2P GO & CLI connection on
 *			MCC using 1x1@5 Ghz
 * @PM_P2P_GO_P2P_CLI_MCC_5_2x2: P2P GO & CLI connection on
 *			MCC using 2x2@5 Ghz
 * @PM_P2P_GO_P2P_CLI_MCC_24_5_1x1:
 * @PM_P2P_GO_P2P_CLI_MCC_24_5_2x2:
 * @PM_P2P_GO_P2P_CLI_DBS_1x1: P2P GO & CLI connection on DBS
 *			using 1x1
 * @PM_P2P_GO_P2P_CLI_DBS_2x2: P2P GO & P2P CLI connection
 *			on DBS using 2x2
 * @PM_P2P_GO_P2P_CLI_SBS_5_1x1: P2P GO & P2P CLI connection
 *			on 5G SBS using 1x1
 * @PM_P2P_GO_P2P_CLI_SBS_5_2x2: P2P GO & P2P CLI connection
 *			on 5G SBS using 2x2
 * @PM_P2P_GO_SAP_SCC_24_1x1: P2P GO & SAP connection on
 *			SCC using 1x1@2.4 Ghz
 * @PM_P2P_GO_SAP_SCC_24_2x2: P2P GO & SAP connection on
 *			SCC using 2x2@2.4 Ghz
 * @PM_P2P_GO_SAP_MCC_24_1x1: P2P GO & SAP connection on
 *			MCC using 1x1@2.4 Ghz
 * @PM_P2P_GO_SAP_MCC_24_2x2: P2P GO & SAP connection on
 *			MCC using 2x2@2.4 Ghz
 * @PM_P2P_GO_SAP_SCC_5_1x1: P2P GO & SAP connection on
 *			SCC using 1x1@5 Ghz
 * @PM_P2P_GO_SAP_SCC_5_2x2: P2P GO & SAP connection on
 *			SCC using 2x2@5 Ghz
 * @PM_P2P_GO_SAP_MCC_5_1x1: P2P GO & SAP connection on
 *			MCC using 1x1@5 Ghz
 * @PM_P2P_GO_SAP_MCC_5_2x2: P2P GO & SAP connection on
 *			MCC using 2x2@5 Ghz
 * @PM_P2P_GO_SAP_MCC_24_5_1x1:
 * @PM_P2P_GO_SAP_MCC_24_5_2x2:
 * @PM_P2P_GO_SAP_DBS_1x1: P2P GO & SAP connection on DBS using
 *			1x1
 * @PM_P2P_GO_SAP_DBS_2x2: P2P GO & SAP connection on DBS using
 *			2x2
 * @PM_P2P_GO_SAP_SBS_5_1x1: P2P GO & SAP connection on 5G SBS
 *			using 1x1
 * @PM_P2P_GO_SAP_SBS_5_2x2: P2P GO & SAP connection on 5G SBS
 *			using 2x2
 * @PM_P2P_GO_P2P_GO_SCC_24_1x1: P2P GO & P2P GO SCC on 2.4G using 1x1
 * @PM_P2P_GO_P2P_GO_SCC_24_2x2: P2P GO & P2P GO SCC on 2.4G using 2x2
 * @PM_P2P_GO_P2P_GO_MCC_24_1x1: P2P GO & P2P GO MCC on 2.4G using 1x1
 * @PM_P2P_GO_P2P_GO_MCC_24_2x2: P2P GO & P2P GO MCC on 2.4G using 2x2
 * @PM_P2P_GO_P2P_GO_SCC_5_1x1: P2P GO & P2P GO SCC on 5G using 1x1
 * @PM_P2P_GO_P2P_GO_SCC_5_2x2: P2P GO & P2P GO SCC on 5G using 2x2
 * @PM_P2P_GO_P2P_GO_MCC_5_1x1: P2P GO & P2P GO MCC on 5G using 1x1
 * @PM_P2P_GO_P2P_GO_MCC_5_2x2: P2P GO & P2P GO MCC on 5G using 2x2
 * @PM_P2P_GO_P2P_GO_MCC_24_5_1x1: P2P GO 2.4G & P2P GO 5G dual band MCC
 *                                 using 1x1
 * @PM_P2P_GO_P2P_GO_MCC_24_5_2x2: P2P GO 2.4G & P2P GO 5G dual band MCC
 *                                 using 2x2
 * @PM_P2P_GO_P2P_GO_DBS_1x1: P2P GO & P2P GO on DBS using 1x1
 * @PM_P2P_GO_P2P_GO_DBS_2x2: P2P GO & P2P GO on DBS using 2x2
 * @PM_P2P_GO_P2P_GO_SBS_5_1x1: P2P GO & P2P GO on SBS using 1x1
 * @PM_P2P_GO_P2P_GO_SBS_5_2x2: P2P GO & P2P GO on SBS using 2x2
 * @PM_P2P_CLI_SAP_SCC_24_1x1: CLI & SAP connection on SCC using
 *			1x1@2.4 Ghz
 * @PM_P2P_CLI_SAP_SCC_24_2x2: CLI & SAP connection on SCC using
 *			2x2@2.4 Ghz
 * @PM_P2P_CLI_SAP_MCC_24_1x1: CLI & SAP connection on MCC using
 *			1x1@2.4 Ghz
 * @PM_P2P_CLI_SAP_MCC_24_2x2: CLI & SAP connection on MCC using
 *			2x2@2.4 Ghz
 * @PM_P2P_CLI_SAP_SCC_5_1x1: CLI & SAP connection on SCC using
 *			1x1@5 Ghz
 * @PM_P2P_CLI_SAP_SCC_5_2x2: CLI & SAP connection on SCC using
 *			2x2@5 Ghz
 * @PM_P2P_CLI_SAP_MCC_5_1x1: CLI & SAP connection on MCC using
 *			1x1@5 Ghz
 * @PM_P2P_CLI_SAP_MCC_5_2x2: CLI & SAP connection on MCC using
 *			2x2@5 Ghz
 * @PM_P2P_CLI_SAP_MCC_24_5_1x1: CLI and SAP connecting on MCC
 *			in 2.4 and 5GHz 1x1
 * @PM_P2P_CLI_SAP_MCC_24_5_2x2: CLI and SAP connecting on MCC
 *			in 2.4 and 5GHz 2x2
 * @PM_P2P_CLI_SAP_DBS_1x1: CLI & SAP connection on DBS using 1x1
 * @PM_P2P_CLI_SAP_DBS_2x2: P2P CLI & SAP connection on DBS using
 *			2x2
 * @PM_P2P_CLI_SAP_SBS_5_1x1: P2P CLI & SAP connection on 5G SBS
 *			using 1x1
 * @PM_P2P_CLI_SAP_SBS_5_2x2: P2P CLI & SAP connection on 5G SBS
 *			using 2x2
 * @PM_P2P_CLI_P2P_CLI_SCC_24_1x1: P2P CLI & P2P CLI SCC on 2.4G using 1x1
 * @PM_P2P_CLI_P2P_CLI_SCC_24_2x2: P2P CLI & P2P CLI SCC on 2.4G using 2x2
 * @PM_P2P_CLI_P2P_CLI_MCC_24_1x1: P2P CLI & P2P CLI MCC on 2.4G using 1x1
 * @PM_P2P_CLI_P2P_CLI_MCC_24_2x2: P2P CLI & P2P CLI MCC on 2.4G using 2x2
 * @PM_P2P_CLI_P2P_CLI_SCC_5_1x1: P2P CLI & P2P CLI SCC on 5G using 1x1
 * @PM_P2P_CLI_P2P_CLI_SCC_5_2x2: P2P CLI & P2P CLI SCC on 5G using 2x2
 * @PM_P2P_CLI_P2P_CLI_MCC_5_1x1: P2P CLI & P2P CLI MCC on 5G using 1x1
 * @PM_P2P_CLI_P2P_CLI_MCC_5_2x2: P2P CLI & P2P CLI MCC on 5G using 2x2
 * @PM_P2P_CLI_P2P_CLI_MCC_24_5_1x1: P2P CLI 2.4G & P2P CLI 5G dual band MCC
 *                                 using 1x1
 * @PM_P2P_CLI_P2P_CLI_MCC_24_5_2x2: P2P CLI 2.4G & P2P CLI 5G dual band MCC
 *                                 using 2x2
 * @PM_P2P_CLI_P2P_CLI_DBS_1x1: P2P CLI & P2P CLI on DBS using 1x1
 * @PM_P2P_CLI_P2P_CLI_DBS_2x2: P2P CLI & P2P CLI on DBS using 2x2
 * @PM_P2P_CLI_P2P_CLI_SBS_5_1x1: P2P CLI & P2P CLI on SBS using 1x1
 * @PM_P2P_CLI_P2P_CLI_SBS_5_2x2: P2P CLI & P2P CLI on SBS using 2x2
 * @PM_SAP_SAP_SCC_24_1x1: SAP & SAP connection on
 *			SCC using 1x1@2.4 Ghz
 * @PM_SAP_SAP_SCC_24_2x2: SAP & SAP connection on
 *			SCC using 2x2@2.4 Ghz
 * @PM_SAP_SAP_MCC_24_1x1: SAP & SAP connection on
 *			MCC using 1x1@2.4 Ghz
 * @PM_SAP_SAP_MCC_24_2x2: SAP & SAP connection on
 *			MCC using 2x2@2.4 Ghz
 * @PM_SAP_SAP_SCC_5_1x1: SAP & SAP connection on
 *			SCC using 1x1@5 Ghz
 * @PM_SAP_SAP_SCC_5_2x2: SAP & SAP connection on
 *			SCC using 2x2@5 Ghz
 * @PM_SAP_SAP_MCC_5_1x1: SAP & SAP connection on
 *			MCC using 1x1@5 Ghz
 * @PM_SAP_SAP_MCC_5_2x2: SAP & SAP connection on
 *          MCC using 2x2@5 Ghz
 * @PM_SAP_SAP_MCC_24_5_1x1: SAP & SAP connection on
 *			MCC in 2.4 and 5GHz 1x1
 * @PM_SAP_SAP_MCC_24_5_2x2: SAP & SAP connection on
 *			MCC in 2.4 and 5GHz 2x2
 * @PM_SAP_SAP_DBS_1x1: SAP & SAP connection on DBS using
 *			1x1
 * @PM_SAP_SAP_DBS_2x2: SAP & SAP connection on DBS using 2x2
 * @PM_SAP_SAP_SBS_5_1x1: SAP & SAP connection on 5G SBS using 1x1
 * @PM_SAP_SAP_SBS_5_2x2: SAP & SAP connection on 5G SBS using 2x2
 * @PM_SAP_NAN_DISC_SCC_24_1x1: SAP & NAN connection on
 *			SCC using 1x1@2.4 Ghz
 * @PM_SAP_NAN_DISC_SCC_24_2x2: SAP & NAN connection on
 *			SCC using 2x2@2.4 Ghz
 * @PM_SAP_NAN_DISC_MCC_24_1x1: SAP & NAN connection on
 *			MCC using 1x1@2.4 Ghz
 * @PM_SAP_NAN_DISC_MCC_24_2x2: SAP & NAN connection on
 *			SCC using 2x2@2.4 Ghz
 * @PM_SAP_NAN_DISC_DBS_1x1: SAP & NAN connection on DBS using 1x1
 * @PM_SAP_NAN_DISC_DBS_2x2: SAP & NAN connection on DBS using 2x2
 * @PM_STA_STA_SCC_24_1x1: STA & STA connection on
 *			SCC using 1x1@2.4 Ghz
 * @PM_STA_STA_SCC_24_2x2: STA & STA connection on
 *			SCC using 2x2@2.4 Ghz
 * @PM_STA_STA_MCC_24_1x1: STA & STA connection on
 *			MCC using 1x1@2.4 Ghz
 * @PM_STA_STA_MCC_24_2x2: STA & STA connection on
 *			MCC using 2x2@2.4 Ghz
 * @PM_STA_STA_SCC_5_1x1: STA & STA connection on
 *			SCC using 1x1@5 Ghz
 * @PM_STA_STA_SCC_5_2x2: STA & STA connection on
 *			SCC using 2x2@5 Ghz
 * @PM_STA_STA_MCC_5_1x1: STA & STA connection on
 *			MCC using 1x1@5 Ghz
 * @PM_STA_STA_MCC_5_2x2: STA & STA connection on
 *          MCC using 2x2@5 Ghz
 * @PM_STA_STA_MCC_24_5_1x1: STA & STA connection on
 *			MCC in 2.4 and 5GHz 1x1
 * @PM_STA_STA_MCC_24_5_2x2: STA & STA connection on
 *			MCC in 2.4 and 5GHz 2x2
 * @PM_STA_STA_DBS_1x1: STA & STA connection on DBS using
 *			1x1
 * @PM_STA_STA_DBS_2x2: STA & STA connection on DBS using 2x2
 * @PM_STA_STA_SBS_5_1x1: STA & STA connection on 5G SBS using 1x1
 * @PM_STA_STA_SBS_5_2x2: STA & STA connection on 5G SBS using 2x2
 * @PM_STA_NAN_DISC_SCC_24_1x1: NAN & STA connection on SCC using 1x1 on 2.4 GHz
 * @PM_STA_NAN_DISC_SCC_24_2x2: NAN & STA connection on SCC using 2x2 on 2.4 GHz
 * @PM_STA_NAN_DISC_MCC_24_1x1: NAN & STA connection on MCC using 1x1 on 2.4 GHz
 * @PM_STA_NAN_DISC_MCC_24_2x2: NAN & STA connection on MCC using 2x2 on 2.4 GHz
 * @PM_STA_NAN_DISC_DBS_1x1: NAN & STA connection on DBS using 1x1
 * @PM_STA_NAN_DISC_DBS_2x2: NAN & STA connection on DBS using 2x2
 * @PM_NAN_DISC_NDI_SCC_24_1x1: NAN & NDI connection on SCC using 1x1 on 2.4 GHz
 * @PM_NAN_DISC_NDI_SCC_24_2x2: NAN & NDI connection on SCC using 2x2 on 2.4 GHz
 * @PM_NAN_DISC_NDI_MCC_24_1x1: NAN & NDI connection on MCC using 1x1 on 2.4 GHz
 * @PM_NAN_DISC_NDI_MCC_24_2x2: NAN & NDI connection on MCC using 2x2 on 2.4 GHz
 * @PM_NAN_DISC_NDI_DBS_1x1: NAN & NDI connection on DBS using 1x1
 * @PM_NAN_DISC_NDI_DBS_2x2: NAN & NDI connection on DBS using 2x2
 * @PM_MAX_TWO_CONNECTION_MODE: Max enumeration
 *
 * These are generic IDs that identify the various roles in the
 * software system
 */
enum policy_mgr_two_connection_mode {
	PM_STA_SAP_SCC_24_1x1 = 0,
	PM_STA_SAP_SCC_24_2x2,
	PM_STA_SAP_MCC_24_1x1,
	PM_STA_SAP_MCC_24_2x2,
	PM_STA_SAP_SCC_5_1x1,
	PM_STA_SAP_SCC_5_2x2,
	PM_STA_SAP_MCC_5_1x1,
	PM_STA_SAP_MCC_5_2x2,
	PM_STA_SAP_MCC_24_5_1x1,
	PM_STA_SAP_MCC_24_5_2x2,
	PM_STA_SAP_DBS_1x1,
	PM_STA_SAP_DBS_2x2,
	PM_STA_SAP_SBS_5_1x1,
	PM_STA_SAP_SBS_5_2x2 = PM_STA_SAP_SBS_5_1x1,
	PM_STA_P2P_GO_SCC_24_1x1,
	PM_STA_P2P_GO_SCC_24_2x2,
	PM_STA_P2P_GO_MCC_24_1x1,
	PM_STA_P2P_GO_MCC_24_2x2,
	PM_STA_P2P_GO_SCC_5_1x1,
	PM_STA_P2P_GO_SCC_5_2x2,
	PM_STA_P2P_GO_MCC_5_1x1,
	PM_STA_P2P_GO_MCC_5_2x2,
	PM_STA_P2P_GO_MCC_24_5_1x1,
	PM_STA_P2P_GO_MCC_24_5_2x2,
	PM_STA_P2P_GO_DBS_1x1,
	PM_STA_P2P_GO_DBS_2x2,
	PM_STA_P2P_GO_SBS_5_1x1,
	PM_STA_P2P_GO_SBS_5_2x2 = PM_STA_P2P_GO_SBS_5_1x1,
	PM_STA_P2P_CLI_SCC_24_1x1,
	PM_STA_P2P_CLI_SCC_24_2x2,
	PM_STA_P2P_CLI_MCC_24_1x1,
	PM_STA_P2P_CLI_MCC_24_2x2,
	PM_STA_P2P_CLI_SCC_5_1x1,
	PM_STA_P2P_CLI_SCC_5_2x2,
	PM_STA_P2P_CLI_MCC_5_1x1,
	PM_STA_P2P_CLI_MCC_5_2x2,
	PM_STA_P2P_CLI_MCC_24_5_1x1,
	PM_STA_P2P_CLI_MCC_24_5_2x2,
	PM_STA_P2P_CLI_DBS_1x1,
	PM_STA_P2P_CLI_DBS_2x2,
	PM_STA_P2P_CLI_SBS_5_1x1,
	PM_STA_P2P_CLI_SBS_5_2x2 = PM_STA_P2P_CLI_SBS_5_1x1,
	PM_P2P_GO_P2P_CLI_SCC_24_1x1,
	PM_P2P_GO_P2P_CLI_SCC_24_2x2,
	PM_P2P_GO_P2P_CLI_MCC_24_1x1,
	PM_P2P_GO_P2P_CLI_MCC_24_2x2,
	PM_P2P_GO_P2P_CLI_SCC_5_1x1,
	PM_P2P_GO_P2P_CLI_SCC_5_2x2,
	PM_P2P_GO_P2P_CLI_MCC_5_1x1,
	PM_P2P_GO_P2P_CLI_MCC_5_2x2,
	PM_P2P_GO_P2P_CLI_MCC_24_5_1x1,
	PM_P2P_GO_P2P_CLI_MCC_24_5_2x2,
	PM_P2P_GO_P2P_CLI_DBS_1x1,
	PM_P2P_GO_P2P_CLI_DBS_2x2,
	PM_P2P_GO_P2P_CLI_SBS_5_1x1,
	PM_P2P_GO_P2P_CLI_SBS_5_2x2 = PM_P2P_GO_P2P_CLI_SBS_5_1x1,
	PM_P2P_GO_SAP_SCC_24_1x1,
	PM_P2P_GO_SAP_SCC_24_2x2,
	PM_P2P_GO_SAP_MCC_24_1x1,
	PM_P2P_GO_SAP_MCC_24_2x2,
	PM_P2P_GO_SAP_SCC_5_1x1,
	PM_P2P_GO_SAP_SCC_5_2x2,
	PM_P2P_GO_SAP_MCC_5_1x1,
	PM_P2P_GO_SAP_MCC_5_2x2,
	PM_P2P_GO_SAP_MCC_24_5_1x1,
	PM_P2P_GO_SAP_MCC_24_5_2x2,
	PM_P2P_GO_SAP_DBS_1x1,
	PM_P2P_GO_SAP_DBS_2x2,
	PM_P2P_GO_SAP_SBS_5_1x1,
	PM_P2P_GO_SAP_SBS_5_2x2 = PM_P2P_GO_SAP_SBS_5_1x1,
	PM_P2P_GO_P2P_GO_SCC_24_1x1,
	PM_P2P_GO_P2P_GO_SCC_24_2x2,
	PM_P2P_GO_P2P_GO_MCC_24_1x1,
	PM_P2P_GO_P2P_GO_MCC_24_2x2,
	PM_P2P_GO_P2P_GO_SCC_5_1x1,
	PM_P2P_GO_P2P_GO_SCC_5_2x2,
	PM_P2P_GO_P2P_GO_MCC_5_1x1,
	PM_P2P_GO_P2P_GO_MCC_5_2x2,
	PM_P2P_GO_P2P_GO_MCC_24_5_1x1,
	PM_P2P_GO_P2P_GO_MCC_24_5_2x2,
	PM_P2P_GO_P2P_GO_DBS_1x1,
	PM_P2P_GO_P2P_GO_DBS_2x2,
	PM_P2P_GO_P2P_GO_SBS_5_1x1,
	PM_P2P_GO_P2P_GO_SBS_5_2x2 = PM_P2P_GO_P2P_GO_SBS_5_1x1,
	PM_P2P_CLI_SAP_SCC_24_1x1,
	PM_P2P_CLI_SAP_SCC_24_2x2,
	PM_P2P_CLI_SAP_MCC_24_1x1,
	PM_P2P_CLI_SAP_MCC_24_2x2,
	PM_P2P_CLI_SAP_SCC_5_1x1,
	PM_P2P_CLI_SAP_SCC_5_2x2,
	PM_P2P_CLI_SAP_MCC_5_1x1,
	PM_P2P_CLI_SAP_MCC_5_2x2,
	PM_P2P_CLI_SAP_MCC_24_5_1x1,
	PM_P2P_CLI_SAP_MCC_24_5_2x2,
	PM_P2P_CLI_SAP_DBS_1x1,
	PM_P2P_CLI_SAP_DBS_2x2,
	PM_P2P_CLI_SAP_SBS_5_1x1,
	PM_P2P_CLI_SAP_SBS_5_2x2 = PM_P2P_CLI_SAP_SBS_5_1x1,
	PM_P2P_CLI_P2P_CLI_SCC_24_1x1,
	PM_P2P_CLI_P2P_CLI_SCC_24_2x2,
	PM_P2P_CLI_P2P_CLI_MCC_24_1x1,
	PM_P2P_CLI_P2P_CLI_MCC_24_2x2,
	PM_P2P_CLI_P2P_CLI_SCC_5_1x1,
	PM_P2P_CLI_P2P_CLI_SCC_5_2x2,
	PM_P2P_CLI_P2P_CLI_MCC_5_1x1,
	PM_P2P_CLI_P2P_CLI_MCC_5_2x2,
	PM_P2P_CLI_P2P_CLI_MCC_24_5_1x1,
	PM_P2P_CLI_P2P_CLI_MCC_24_5_2x2,
	PM_P2P_CLI_P2P_CLI_DBS_1x1,
	PM_P2P_CLI_P2P_CLI_DBS_2x2,
	PM_P2P_CLI_P2P_CLI_SBS_5_1x1,
	PM_P2P_CLI_P2P_CLI_SBS_5_2x2 = PM_P2P_CLI_P2P_CLI_SBS_5_1x1,
	PM_SAP_SAP_SCC_24_1x1,
	PM_SAP_SAP_SCC_24_2x2,
	PM_SAP_SAP_MCC_24_1x1,
	PM_SAP_SAP_MCC_24_2x2,
	PM_SAP_SAP_SCC_5_1x1,
	PM_SAP_SAP_SCC_5_2x2,
	PM_SAP_SAP_MCC_5_1x1,
	PM_SAP_SAP_MCC_5_2x2,
	PM_SAP_SAP_MCC_24_5_1x1,
	PM_SAP_SAP_MCC_24_5_2x2,
	PM_SAP_SAP_DBS_1x1,
	PM_SAP_SAP_DBS_2x2,
	PM_SAP_SAP_SBS_5_1x1,
	PM_SAP_SAP_SBS_5_2x2 = PM_SAP_SAP_SBS_5_1x1,
	PM_SAP_NAN_DISC_SCC_24_1x1,
	PM_SAP_NAN_DISC_SCC_24_2x2,
	PM_SAP_NAN_DISC_MCC_24_1x1,
	PM_SAP_NAN_DISC_MCC_24_2x2,
	PM_SAP_NAN_DISC_DBS_1x1,
	PM_SAP_NAN_DISC_DBS_2x2,
	PM_STA_STA_SCC_24_1x1,
	PM_STA_STA_SCC_24_2x2,
	PM_STA_STA_MCC_24_1x1,
	PM_STA_STA_MCC_24_2x2,
	PM_STA_STA_SCC_5_1x1,
	PM_STA_STA_SCC_5_2x2,
	PM_STA_STA_MCC_5_1x1,
	PM_STA_STA_MCC_5_2x2,
	PM_STA_STA_MCC_24_5_1x1,
	PM_STA_STA_MCC_24_5_2x2,
	PM_STA_STA_DBS_1x1,
	PM_STA_STA_DBS_2x2,
	PM_STA_STA_SBS_5_1x1,
	PM_STA_STA_SBS_5_2x2 = PM_STA_STA_SBS_5_1x1,
	PM_STA_NAN_DISC_SCC_24_1x1,
	PM_STA_NAN_DISC_SCC_24_2x2,
	PM_STA_NAN_DISC_MCC_24_1x1,
	PM_STA_NAN_DISC_MCC_24_2x2,
	PM_STA_NAN_DISC_DBS_1x1,
	PM_STA_NAN_DISC_DBS_2x2,
	PM_NAN_DISC_NDI_SCC_24_1x1,
	PM_NAN_DISC_NDI_SCC_24_2x2,
	PM_NAN_DISC_NDI_MCC_24_1x1,
	PM_NAN_DISC_NDI_MCC_24_2x2,
	PM_NAN_DISC_NDI_DBS_1x1,
	PM_NAN_DISC_NDI_DBS_2x2,
	PM_MAX_TWO_CONNECTION_MODE
};

#ifdef FEATURE_FOURTH_CONNECTION
/**
 * enum policy_mgr_three_connection_mode - Combination of first three
 * connections type, concurrency state, band used.
 *
 * @PM_STA_SAP_SCC_24_SAP_5_DBS: STA & SAP connection on 2.4 GHZ, another
 * SAP on 5 GHZ
 * @PM_STA_SAP_SCC_5_SAP_24_DBS: STA & SAP connection on 5 GHZ,
 * another SAP on 2.4 GHZ
 * @PM_24_SCC_MCC_PLUS_5_DBS: ANY 2 link on 2.4 GHZ SCC/MCC mac and one link on
 * 5 GHZ doing DBS
 * @PM_STA_SAP_24_STA_5_DBS: STA & SAP connection on 2.4 GHZ SCC/MCC,
 * another STA on 5 GHZ
 * @PM_5_SCC_MCC_PLUS_24_DBS: ANY 2 link on 5 GHZ SCC/MCC mac and one link on
 * 2.4 GHZ doing DBS
 * @PM_STA_SAP_5_STA_24_DBS: STA & SAP connection on 5 GHZ SCC/MCC,
 * STA on 2.4 GHZ
 * @PM_STA_STA_5_SAP_24_DBS: STA & STA connection on 5 GHZ SCC/MCC,
 * SAP on 2.4 GHZ
 * @PM_NAN_DISC_SAP_SCC_24_NDI_5_DBS: NAN_DISC & SAP connection on 2.4 Ghz SCC,
 * NDI/NDP on 5 G
 * @PM_NAN_DISC_NDI_SCC_24_SAP_5_DBS: NAN_DISC & NDI/NDP connection on 2.4 Ghz
 * SCC, SAP on 5 G
 * @PM_SAP_NDI_SCC_5_NAN_DISC_24_DBS: SAP & NDI/NDP connection on 5 Ghz,
 * NAN_DISC on 24 Ghz
 * @PM_NAN_DISC_STA_24_NDI_5_DBS: STA and NAN Disc on 2.4Ghz and NDI on 5ghz DBS
 * @PM_NAN_DISC_NDI_24_STA_5_DBS: NDI and NAN Disc on 2.4Ghz and STA on 5ghz DBS
 * @PM_STA_NDI_5_NAN_DISC_24_DBS: STA, NDI on 5ghz and NAN Disc on 2.4Ghz DBS
 * @PM_STA_NDI_NAN_DISC_24_SMM: STA, NDI, NAN Disc all on 2.4ghz SMM
 * @PM_NAN_DISC_NDI_24_NDI_5_DBS: NDI and NAN Disc on 2.4Ghz and second NDI in
 * 5ghz DBS
 * @PM_NDI_NDI_5_NAN_DISC_24_DBS: Both NDI on 5ghz and NAN Disc on 2.4Ghz DBS
 * @PM_NDI_NDI_NAN_DISC_24_SMM: Both NDI, NAN Disc on 2.4ghz SMM
 * @PM_SAP_SAP_SCC_24_SAP_5_DBS: Both SAP on 2.4Ghz and another SAP on 5Ghz DBS
 * @PM_SAP_SAP_SCC_5_SAP_24_DBS: Both SAP on 5Ghz and another SAP on 2.4Ghz DBS
 * @PM_STA_STA_5_NAN_DISC_24_DBS: Both STA on 5Ghz and NAN Disc on 2.4Ghz DBS
 * @PM_NAN_DISC_24_STA_STA_5_DBS: NAN Disc on 2.4Ghz and both STA on 5Ghz DBS
 * @PM_STA_STA_24_NAN_DISC_24_SMM: Both STA on 2.4Ghz and NAN Disc on 2.4Ghz SMM
 * @PM_NAN_DISC_24_STA_STA_24_SMM: NAN Disc on 2.4Ghz and both STA on 2.4Ghz SMM
 * @PM_STA_24_STA_5_NAN_DISC_24_SMM: First STA on 2.4Ghz and second STA on 5Ghz
 * and NAN Disc on 2.4Ghz SMM
 * @PM_STA_24_STA_5_NAN_DISC_24_DBS: First STA on 2.4Ghz and second STA on 5Ghz
 * and NAN Disc on 2.4Ghz DBS
 * @PM_STA_5_STA_24_NAN_DISC_24_SMM: First STA on 5Ghz and second STA on 2.4Ghz
 * and NAN Disc on 2.4Ghz SMM
 * @PM_STA_5_STA_24_NAN_DISC_24_DBS: First STA on 5Ghz and second STA on 2.4Ghz
 * and NAN Disc on 2.4Ghz DBS
 * @PM_NAN_DISC_24_STA_5_STA_24_SMM: NAN Disc on 2.4Ghz and first STA on 5Ghz
 * and second STA on 2.4Ghz SMM
 * @PM_NAN_DISC_24_STA_5_STA_24_DBS: NAN Disc on 2.4Ghz and first STA on 5Ghz
 * and second STA on 2.4Ghz DBS
 * @PM_NAN_DISC_24_STA_24_STA_5_SMM: NAN Disc on 2.4Ghz and first STA on 2.4Ghz
 * and second STA on 5Ghz SMM
 * @PM_NAN_DISC_24_STA_24_STA_5_DBS: NAN Disc on 2.4Ghz and first STA on 2.4Ghz
 * and second STA on 5Ghz DBS
 * @PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS: ANY 2 link on 5 GHZ high mac
 * and one link on 5 GHZ low doing SBS
 * @PM_STA_24_SAP_5_HIGH_MCC_STA_5_LOW_SBS : First STA on 2.4 GHZ & SAP on high
 * 5 GHZ MCC on mac 0 and second STA on low 5 GHZ on mac1 doing SBS
 * @PM_STA_24_STA_5_HIGH_MCC_SAP_5_LOW_SBS : First STA on 2.4 GHZ & second STA
 * on high 5 GHZ MCC on mac 0 and SAP on high 5 GHZ on mac1 doing SBS
 * @PM_STA_SAP_5_HIGH_STA_5_LOW_SBS : First STA on high 5 GHZ & SAP on high
 * 5 GHZ SCC/MCC on mac0 and second STA on low 5 GHZ on mac1 doing SBS
 * @PM_STA_5_HIGH_SAP_24_MCC_STA_5_LOW_SBS : First STA on high 5 GHZ & SAP on
 * 2.4 GHZ MCC on mac0 and second STA on low 5 GHZ on mac1 doing SBS
 * @PM_STA_STA_5_HIGH_MCC_SAP_5_LOW_SBS : First STA on high 5 GHZ & Second STA
 * on high 5 GHZ MCC on mac0 and SAP on low 5 GHZ on mac1 doing SBS
 * @PM_STA_24_STA_5_MCC_SAP_5_HIGH_SBS: MLO STA 2+5/6 GHz, SAP on 5/6 GHz
 * high band, the current hw mode is SBS.
 * @PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS: Any 2 link on low 5 GHZ mac
 * and one link on high 5 GHZ freq doing SBS
 * @PM_STA_24_SAP_5_LOW_MCC_STA_5_HIGH_SBS : First STA on 2.4 GHZ & SAP on low
 * 5 GHZ MCC on mac 0 and second STA on high 5 GHZ on mac1 doing SBS
 * @PM_STA_24_STA_5_LOW_MCC_SAP_5_HIGH_SBS : First STA on 2.4 & second STA on
 * low 5G MCC on mac 0 and SAP on high 5g on mac1 doing SBS
 * @PM_STA_SAP_5_LOW_STA_5_HIGH_SBS : First STA on low 5 GHZ & SAP on low 5 GHZ
 * SCC/MCC on mac0 and second STA on high 5 GHZ on mac1 doing SBS
 * @PM_STA_5_LOW_SAP_24_MCC_STA_5_HIGH_SBS : First STA on low 5 GHZ & SAP on
 * 2.4 GHZ MCC on mac0 and second STA on high 5 GHZ on mac1 doing SBS
 * @PM_STA_STA_5_LOW_MCC_SAP_5_HIGH_SBS : First STA on high 5 GHZ & Second STA
 * on high 5 GHZ MCC on mac0 and SAP on low 5 GHZ on mac1 doing SBS
 * @PM_STA_24_STA_5_MCC_SAP_5_LOW_SBS: MLO STA 2+5/6 GHz, SAP on 5/6 GHz
 * low band, the current hw mode is SBS.
 * @PM_24_5_PLUS_5_LOW_N_HIGH_SHARE_SBS:
 * @PM_SAP_24_STA_5_STA_5_LOW_N_HIGH_SHARE_SBS: The 2.4 GHZ SAP creating MCC/SCC
 * with STA of low 5 GHZ or high 5 GHZ (dynamic SBS) on mac 0 and one STA on
 * high 5 GHZ or low 5 GHZ freq respectively on mac 1 doing SBS
 * @PM_STA_24_SAP_5_STA_5_LOW_N_HIGH_SHARE_SBS: The 2.4 GHZ STA creating MCC/SCC
 * with SAP of low 5 GHZ or high 5 GHZ (dynamic SBS) on mac 0 and one STA on
 * high 5 GHZ or low 5 GHZ freq respectively on mac 1 doing SBS
 * @PM_24_5_PLUS_5_LOW_OR_HIGH_SHARE_SBS: (not dynamic SBS) MLO STA 2+5/6 GHz,
 * SAP on 5/6 GHz, and target only support 2.4 GHz shared with 5 GHz low band
 * or 5 GHz high band but not both.
 * @PM_MAX_THREE_CONNECTION_MODE: Maximum enumeration
 */
enum policy_mgr_three_connection_mode {
	PM_STA_SAP_SCC_24_SAP_5_DBS,
	PM_STA_SAP_SCC_5_SAP_24_DBS,
	PM_24_SCC_MCC_PLUS_5_DBS,
	PM_STA_SAP_24_STA_5_DBS = PM_24_SCC_MCC_PLUS_5_DBS,
	PM_5_SCC_MCC_PLUS_24_DBS,
	PM_STA_SAP_5_STA_24_DBS = PM_5_SCC_MCC_PLUS_24_DBS,
	PM_STA_STA_5_SAP_24_DBS = PM_5_SCC_MCC_PLUS_24_DBS,
	PM_NAN_DISC_SAP_SCC_24_NDI_5_DBS,
	PM_NAN_DISC_NDI_SCC_24_SAP_5_DBS,
	PM_SAP_NDI_SCC_5_NAN_DISC_24_DBS,
	PM_NAN_DISC_STA_24_NDI_5_DBS,
	PM_NAN_DISC_NDI_24_STA_5_DBS,
	PM_STA_NDI_5_NAN_DISC_24_DBS,
	PM_STA_NDI_NAN_DISC_24_SMM,
	PM_NAN_DISC_NDI_24_NDI_5_DBS,
	PM_NDI_NDI_5_NAN_DISC_24_DBS,
	PM_NDI_NDI_NAN_DISC_24_SMM,
	PM_SAP_SAP_SCC_24_SAP_5_DBS,
	PM_SAP_SAP_SCC_5_SAP_24_DBS,
	PM_STA_STA_5_NAN_DISC_24_DBS,
	PM_NAN_DISC_24_STA_STA_5_DBS,
	PM_STA_STA_24_NAN_DISC_24_SMM,
	PM_NAN_DISC_24_STA_STA_24_SMM,
	PM_STA_24_STA_5_NAN_DISC_24_SMM,
	PM_STA_24_STA_5_NAN_DISC_24_DBS,
	PM_STA_5_STA_24_NAN_DISC_24_SMM,
	PM_STA_5_STA_24_NAN_DISC_24_DBS,
	PM_NAN_DISC_24_STA_5_STA_24_SMM,
	PM_NAN_DISC_24_STA_5_STA_24_DBS,
	PM_NAN_DISC_24_STA_24_STA_5_SMM,
	PM_NAN_DISC_24_STA_24_STA_5_DBS,
	PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS,
	PM_STA_24_SAP_5_HIGH_MCC_STA_5_LOW_SBS =
		PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS,
	PM_STA_24_STA_5_HIGH_MCC_SAP_5_LOW_SBS =
		PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS,
	PM_STA_SAP_5_HIGH_STA_5_LOW_SBS =
		PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS,
	PM_STA_5_HIGH_SAP_24_MCC_STA_5_LOW_SBS =
		PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS,
	PM_STA_STA_5_HIGH_MCC_SAP_5_LOW_SBS =
		PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS,
	PM_STA_24_STA_5_MCC_SAP_5_HIGH_SBS =
		PM_MCC_SCC_5G_HIGH_PLUS_5_LOW_SBS,
	PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS,
	PM_STA_24_SAP_5_LOW_MCC_STA_5_HIGH_SBS =
		PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS,
	PM_STA_24_STA_5_LOW_MCC_SAP_5_HIGH_SBS =
		PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS,
	PM_STA_SAP_5_LOW_STA_5_HIGH_SBS =
		PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS,
	PM_STA_5_LOW_SAP_24_MCC_STA_5_HIGH_SBS =
		PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS,
	PM_STA_STA_5_LOW_MCC_SAP_5_HIGH_SBS =
		PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS,
	PM_STA_24_STA_5_MCC_SAP_5_LOW_SBS =
		PM_MCC_SCC_5G_LOW_PLUS_5_HIGH_SBS,
	PM_24_5_PLUS_5_LOW_N_HIGH_SHARE_SBS,
	PM_SAP_24_STA_5_STA_5_LOW_N_HIGH_SHARE_SBS =
			PM_24_5_PLUS_5_LOW_N_HIGH_SHARE_SBS,
	PM_STA_24_SAP_5_STA_5_LOW_N_HIGH_SHARE_SBS =
			PM_24_5_PLUS_5_LOW_N_HIGH_SHARE_SBS,
	PM_24_5_PLUS_5_LOW_OR_HIGH_SHARE_SBS,

	PM_MAX_THREE_CONNECTION_MODE,
};
#endif

/**
 * enum policy_mgr_conc_next_action - actions to be taken on old
 * connections.
 *
 * @PM_NOP: No action
 * @PM_DBS: switch to DBS mode
 * @PM_DBS_DOWNGRADE: switch to DBS mode & downgrade to 1x1
 * @PM_DBS_UPGRADE: switch to DBS mode & upgrade to 2x2
 * @PM_SINGLE_MAC: switch to MCC/SCC mode
 * @PM_SINGLE_MAC_UPGRADE: switch to MCC/SCC mode & upgrade to 2x2
 * @PM_SBS: switch to SBS mode
 * @PM_SBS_DOWNGRADE: switch to SBS mode & downgrade to 1x1
 * @PM_DOWNGRADE: downgrade to 1x1
 * @PM_UPGRADE: upgrade to 2x2
 * @PM_DBS1: switch to DBS 1
 * @PM_DBS1_DOWNGRADE: downgrade 2G beaconing entity to 1x1 and switch to DBS1.
 * @PM_DBS2: switch to DBS 2
 * @PM_DBS2_DOWNGRADE: downgrade 5G beaconing entity to 1x1 and switch to DBS2.
 * @PM_UPGRADE_5G: upgrade 5g beaconing entity to 2x2.
 * @PM_UPGRADE_2G: upgrade 2g beaconing entity to 2x2.
 * @PM_MAX_CONC_NEXT_ACTION: Max place holder
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum policy_mgr_conc_next_action {
	PM_NOP = 0,
	PM_DBS,
	PM_DBS_DOWNGRADE,
	PM_DBS_UPGRADE,
	PM_SINGLE_MAC,
	PM_SINGLE_MAC_UPGRADE,
	PM_SBS,
	PM_SBS_DOWNGRADE,
	PM_DOWNGRADE,
	PM_UPGRADE,
	PM_DBS1,
	PM_DBS1_DOWNGRADE,
	PM_DBS2,
	PM_DBS2_DOWNGRADE,
	PM_UPGRADE_5G,
	PM_UPGRADE_2G,

	PM_MAX_CONC_NEXT_ACTION
};

/**
 * enum policy_mgr_band - wifi band.
 *
 * @POLICY_MGR_BAND_24: 2.4 Ghz band
 * @POLICY_MGR_BAND_5: 5 Ghz band
 * @POLICY_MGR_ANY: to specify all band
 * @POLICY_MGR_MAX_BAND: Max place holder
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum policy_mgr_band {
	POLICY_MGR_BAND_24 = 0,
	POLICY_MGR_BAND_5,
	POLICY_MGR_ANY,
	POLICY_MGR_MAX_BAND = POLICY_MGR_ANY,
};

/**
 * enum policy_mgr_conn_update_reason: Reason for conc connection update
 * @POLICY_MGR_UPDATE_REASON_SET_OPER_CHAN: Set probable operating channel
 * @POLICY_MGR_UPDATE_REASON_START_AP: Start AP
 * @POLICY_MGR_UPDATE_REASON_NORMAL_STA: Connection to Normal STA
 * @POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC: Opportunistic HW mode update
 * @POLICY_MGR_UPDATE_REASON_NSS_UPDATE: NSS update
 * @POLICY_MGR_UPDATE_REASON_AFTER_CHANNEL_SWITCH: After Channel switch
 * @POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_STA: Before Channel switch for STA
 * @POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_SAP: Before Channel switch for SAP
 * @POLICY_MGR_UPDATE_REASON_PRE_CAC: Pre-CAC
 * @POLICY_MGR_UPDATE_REASON_PRI_VDEV_CHANGE: In Dual DBS HW, if the vdev based
 *        2x2 preference enabled, the vdev down may cause prioritized active
 *        vdev change, then DBS hw mode may needs to change from one DBS mode
 *        to the other DBS mode. This reason code indicates such condition.
 * @POLICY_MGR_UPDATE_REASON_NAN_DISCOVERY: NAN Discovery related
 * @POLICY_MGR_UPDATE_REASON_NDP_UPDATE: NAN Datapath related update
 * @POLICY_MGR_UPDATE_REASON_LFR2_ROAM: Roaming
 * @POLICY_MGR_UPDATE_REASON_STA_CONNECT: STA/CLI connection to peer
 */
enum policy_mgr_conn_update_reason {
	POLICY_MGR_UPDATE_REASON_SET_OPER_CHAN,
	POLICY_MGR_UPDATE_REASON_START_AP,
	POLICY_MGR_UPDATE_REASON_NORMAL_STA,
	POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC,
	POLICY_MGR_UPDATE_REASON_NSS_UPDATE,
	POLICY_MGR_UPDATE_REASON_AFTER_CHANNEL_SWITCH,
	POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_STA,
	POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_SAP,
	POLICY_MGR_UPDATE_REASON_PRE_CAC,
	POLICY_MGR_UPDATE_REASON_PRI_VDEV_CHANGE,
	POLICY_MGR_UPDATE_REASON_NAN_DISCOVERY,
	POLICY_MGR_UPDATE_REASON_NDP_UPDATE,
	POLICY_MGR_UPDATE_REASON_LFR2_ROAM,
	POLICY_MGR_UPDATE_REASON_STA_CONNECT,
};

/**
 * enum hw_mode_bandwidth - bandwidth of wifi channel.
 * @HW_MODE_BW_NONE: no bandwidth configured
 * @HW_MODE_5_MHZ: 5 Mhz bandwidth
 * @HW_MODE_10_MHZ: 10 Mhz bandwidth
 * @HW_MODE_20_MHZ: 20 Mhz bandwidth
 * @HW_MODE_40_MHZ: 40 Mhz bandwidth
 * @HW_MODE_80_MHZ: 80 Mhz bandwidth
 * @HW_MODE_80_PLUS_80_MHZ: 80 Mhz plus 80 Mhz bandwidth
 * @HW_MODE_160_MHZ: 160 Mhz bandwidth
 * @HW_MODE_320_MHZ: 320 Mhz bandwidth
 * @HW_MODE_MAX_BANDWIDTH: Max place holder
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum hw_mode_bandwidth {
	HW_MODE_BW_NONE,
	HW_MODE_5_MHZ,
	HW_MODE_10_MHZ,
	HW_MODE_20_MHZ,
	HW_MODE_40_MHZ,
	HW_MODE_80_MHZ,
	HW_MODE_80_PLUS_80_MHZ,
	HW_MODE_160_MHZ,
	HW_MODE_320_MHZ,
	HW_MODE_MAX_BANDWIDTH
};

/**
 * enum set_hw_mode_status - Status of set HW mode command
 * @SET_HW_MODE_STATUS_OK: command successful
 * @SET_HW_MODE_STATUS_EINVAL: Requested invalid hw_mode
 * @SET_HW_MODE_STATUS_ECANCELED: HW mode change cancelled
 * @SET_HW_MODE_STATUS_ENOTSUP: HW mode not supported
 * @SET_HW_MODE_STATUS_EHARDWARE: HW mode change prevented by hardware
 * @SET_HW_MODE_STATUS_EPENDING: HW mode change is pending
 * @SET_HW_MODE_STATUS_ECOEX: HW mode change conflict with Coex
 * @SET_HW_MODE_STATUS_ALREADY: Requested hw mode is already applied to FW.
 */
enum set_hw_mode_status {
	SET_HW_MODE_STATUS_OK,
	SET_HW_MODE_STATUS_EINVAL,
	SET_HW_MODE_STATUS_ECANCELED,
	SET_HW_MODE_STATUS_ENOTSUP,
	SET_HW_MODE_STATUS_EHARDWARE,
	SET_HW_MODE_STATUS_EPENDING,
	SET_HW_MODE_STATUS_ECOEX,
	SET_HW_MODE_STATUS_ALREADY,
};

typedef void (*dual_mac_cb)(enum set_hw_mode_status status,
		uint32_t scan_config,
		uint32_t fw_mode_config);
/**
 * enum policy_mgr_hw_mode_change - identify the HW mode switching to.
 *
 * @POLICY_MGR_HW_MODE_NOT_IN_PROGRESS: HW mode change not in progress
 * @POLICY_MGR_SMM_IN_PROGRESS: switching to SMM mode
 * @POLICY_MGR_DBS_IN_PROGRESS: switching to DBS mode
 * @POLICY_MGR_SBS_IN_PROGRESS: switching to SBS mode
 *
 * These are generic IDs that identify the various roles
 * in the software system
 */
enum policy_mgr_hw_mode_change {
	POLICY_MGR_HW_MODE_NOT_IN_PROGRESS = 0,
	POLICY_MGR_SMM_IN_PROGRESS,
	POLICY_MGR_DBS_IN_PROGRESS,
	POLICY_MGR_SBS_IN_PROGRESS
};

/**
 * enum dbs_support - structure to define INI values and their meaning
 *
 * @ENABLE_DBS_CXN_AND_SCAN: Enable DBS support for connection and scan
 * @DISABLE_DBS_CXN_AND_SCAN: Disable DBS support for connection and scan
 * @DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN: disable dbs support for
 * connection but keep dbs support for scan
 * @DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN_WITH_ASYNC_SCAN_OFF: disable dbs support
 * for connection but keep dbs for scan but switch off the async scan
 * @ENABLE_DBS_CXN_AND_ENABLE_SCAN_WITH_ASYNC_SCAN_OFF: enable dbs support for
 * connection and scan but switch off the async scan
 * @ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN: Enable DBS support for connection and
 * disable DBS support for scan
 * @ENABLE_DBS_CXN_AND_DISABLE_SIMULTANEOUS_SCAN: Enable DBS
 * support for connection and disable simultaneous scan from
 * upper layer (DBS scan remains enabled in FW)
 */
enum dbs_support {
	ENABLE_DBS_CXN_AND_SCAN,
	DISABLE_DBS_CXN_AND_SCAN,
	DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN,
	DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN_WITH_ASYNC_SCAN_OFF,
	ENABLE_DBS_CXN_AND_ENABLE_SCAN_WITH_ASYNC_SCAN_OFF,
	ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN,
	ENABLE_DBS_CXN_AND_DISABLE_SIMULTANEOUS_SCAN,
};

/**
 * enum conn_6ghz_flag - structure to define connection 6ghz capable info
 * in policy mgr conn info struct
 *
 * @CONN_6GHZ_FLAG_VALID: The 6ghz flag is valid (has been initialized)
 * @CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED: AP is configured in 6ghz band capable
 *   by user:
 *   a. ACS channel range includes 6ghz.
 *   b. SAP is started in 6ghz fix channel.
 * @CONN_6GHZ_FLAG_SECURITY_ALLOWED: AP has security mode which is permitted in
 *   6ghz band.
 * @CONN_6GHZ_FLAG_NO_LEGACY_CLIENT: AP has no legacy client connected
 */
enum conn_6ghz_flag {
	CONN_6GHZ_FLAG_VALID = 0x0001,
	CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED = 0x0002,
	CONN_6GHZ_FLAG_SECURITY_ALLOWED = 0x0004,
	CONN_6GHZ_FLAG_NO_LEGACY_CLIENT = 0x0008,
};

#ifdef WLAN_FEATURE_AFC_DCS_SKIP_ACS_RANGE
/* To support DCS to 6 Ghz channel when AFC response receive */
#define CONN_6GHZ_CAPABLE (CONN_6GHZ_FLAG_VALID | \
			     CONN_6GHZ_FLAG_SECURITY_ALLOWED | \
			     CONN_6GHZ_FLAG_NO_LEGACY_CLIENT)
#else
#define CONN_6GHZ_CAPABLE (CONN_6GHZ_FLAG_VALID | \
			     CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED | \
			     CONN_6GHZ_FLAG_SECURITY_ALLOWED | \
			     CONN_6GHZ_FLAG_NO_LEGACY_CLIENT)
#endif

/**
 * struct policy_mgr_conc_connection_info - information of all existing
 * connections in the wlan system
 *
 * @mode: connection type
 * @freq: Channel frequency
 * @bw: channel bandwidth used for the connection
 * @mac: The HW mac it is running
 * @chain_mask: The original capability advertised by HW
 * @original_nss: nss negotiated at connection time
 * @vdev_id: vdev id of the connection
 * @in_use: if the table entry is active
 * @ch_flagext: Channel extension flags.
 * @conn_6ghz_flag: connection 6ghz capable flags
 */
struct policy_mgr_conc_connection_info {
	enum policy_mgr_con_mode mode;
	uint32_t      freq;
	enum hw_mode_bandwidth bw;
	uint8_t       mac;
	enum policy_mgr_chain_mode chain_mask;
	uint32_t      original_nss;
	uint32_t      vdev_id;
	bool          in_use;
	uint16_t      ch_flagext;
	enum conn_6ghz_flag conn_6ghz_flag;
};

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * struct policy_mgr_disabled_ml_link_info - information of all existing
 * disabled ml link
 * @in_use: if the table entry is active
 * @mode: connection type
 * @freq: Channel frequency
 * @vdev_id: vdev id of the connection
 */
struct policy_mgr_disabled_ml_link_info {
	bool in_use;
	enum policy_mgr_con_mode mode;
	qdf_freq_t freq;
	uint8_t vdev_id;
};
#endif

/**
 * struct policy_mgr_hw_mode_params - HW mode params
 * @mac0_tx_ss: MAC0 Tx spatial stream
 * @mac0_rx_ss: MAC0 Rx spatial stream
 * @mac1_tx_ss: MAC1 Tx spatial stream
 * @mac1_rx_ss: MAC1 Rx spatial stream
 * @mac0_bw: MAC0 bandwidth
 * @mac1_bw: MAC1 bandwidth
 * @mac0_band_cap: mac0 band (5g/2g) capability
 * @dbs_cap: DBS capabality
 * @agile_dfs_cap: Agile DFS capabality
 * @sbs_cap: SBS capability
 * @emlsr_cap: eMLSR capability
 * @action_type: for dbs mode, the field indicates the "Action type" to be
 * used to switch to the mode. To help the hw mode validation.
 */
struct policy_mgr_hw_mode_params {
	uint8_t mac0_tx_ss;
	uint8_t mac0_rx_ss;
	uint8_t mac1_tx_ss;
	uint8_t mac1_rx_ss;
	uint8_t mac0_bw;
	uint8_t mac1_bw;
	uint8_t mac0_band_cap;
	uint8_t dbs_cap;
	uint8_t agile_dfs_cap;
	uint8_t sbs_cap;
	uint8_t emlsr_cap;
	enum policy_mgr_conc_next_action action_type;
};

/**
 * struct policy_mgr_dual_mac_config - Dual MAC configuration
 * @scan_config: Scan configuration
 * @fw_mode_config: FW mode configuration
 * @set_dual_mac_cb: Callback function to be executed on response to the command
 */
struct policy_mgr_dual_mac_config {
	uint32_t scan_config;
	uint32_t fw_mode_config;
	dual_mac_cb set_dual_mac_cb;
};

/**
 * struct policy_mgr_hw_mode - Format of set HW mode
 * @hw_mode_index: Index of HW mode to be set
 * @set_hw_mode_cb: HDD set HW mode callback
 * @reason: Reason for HW mode change
 * @session_id: Session id
 * @next_action: next action to happen at policy mgr
 * @action: current hw change action to be done
 * @context: psoc context
 * @request_id: Request id provided by the requester, can be used while
 * calling callback to the requester
 */
struct policy_mgr_hw_mode {
	uint32_t hw_mode_index;
	void *set_hw_mode_cb;
	enum policy_mgr_conn_update_reason reason;
	uint32_t session_id;
	uint8_t next_action;
	enum policy_mgr_conc_next_action action;
	struct wlan_objmgr_psoc *context;
	uint32_t request_id;
};

/**
 * struct policy_mgr_pcl_list - Format of PCL
 * @pcl_list: List of preferred channels
 * @weight_list: Weights of the PCL
 * @pcl_len: Number of channels in the PCL
 */
struct policy_mgr_pcl_list {
	uint32_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
	uint32_t pcl_len;
};

/**
 * struct policy_mgr_pcl_chan_weights - Params to get the valid weighed list
 * @pcl_list: Preferred channel list already sorted in the order of preference
 * @pcl_len: Length of the PCL
 * @saved_chan_list: Valid channel list updated as part of
 * WMA_UPDATE_CHAN_LIST_REQ
 * @saved_num_chan: Length of the valid channel list
 * @weighed_valid_list: Weights of the valid channel list. This will have one
 * to one mapping with valid_chan_list. FW expects channel order and size to be
 * as per the list provided in WMI_SCAN_CHAN_LIST_CMDID.
 * @weight_list: Weights assigned by policy manager
 */
struct policy_mgr_pcl_chan_weights {
	uint32_t pcl_list[NUM_CHANNELS];
	uint32_t pcl_len;
	uint32_t saved_chan_list[NUM_CHANNELS];
	uint32_t saved_num_chan;
	uint8_t weighed_valid_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
};

/**
 * struct policy_mgr_vdev_entry_info - vdev related param to be
 * used by policy manager
 * @type: type
 * @sub_type: sub type
 * @mhz: channel frequency in MHz
 * @chan_width: channel bandwidth
 * @mac_id: the mac on which vdev is on
 * @ch_flagext: Channel extension flags.
 */
struct policy_mgr_vdev_entry_info {
	uint32_t type;
	uint32_t sub_type;
	uint32_t mhz;
	uint32_t chan_width;
	uint32_t mac_id;
	uint16_t ch_flagext;
};

/**
 * struct policy_mgr_freq_range - hw mode freq range for the pdev
 * @low_2ghz_freq: lower 2.4GHz channels
 * @high_2ghz_freq: higher 2.4 GHz channels
 * @low_5ghz_freq: lower 5 GHz channels
 * @high_5ghz_freq: higher 5 GHz channels
 */
struct policy_mgr_freq_range {
	qdf_freq_t low_2ghz_freq;
	qdf_freq_t high_2ghz_freq;
	qdf_freq_t low_5ghz_freq;
	qdf_freq_t high_5ghz_freq;
};

/**
 * enum policy_mgr_mode - enum for host mode
 * @MODE_SMM:               Single mac mode
 * @MODE_DBS:               DBS mode
 * @MODE_SBS:               SBS mode with either high share or low share
 * @MODE_SBS_UPPER_SHARE:   Higher 5Ghz shared with 2.4Ghz
 * @MODE_SBS_LOWER_SHARE:   LOWER 5Ghz shared with 2.4Ghz
 * @MODE_EMLSR:             eMLSR mode
 * @MODE_HW_MAX: MAX
 */
enum policy_mgr_mode {
	MODE_SMM,
	MODE_DBS,
	MODE_SBS,
	MODE_SBS_UPPER_SHARE,
	MODE_SBS_LOWER_SHARE,
	MODE_EMLSR,
	MODE_HW_MAX,
};

/**
 * struct dbs_hw_mode_info - WLAN_DBS_HW_MODES_TLV Format
 * @hw_mode_list: WLAN_DBS_HW_MODE_LIST entries
 * @sbs_lower_band_end_freq: value with which range will be divided
 * @freq_range_caps: Initial capability and range for different modes for both
 *                   pdev
 * @cur_mac_freq_range: Current freq range for both pdev, this can be used to
 *                      reject if 2 home channels on a MAC, depending on opmode
 *                      and current HW mode.
 */
struct dbs_hw_mode_info {
	uint64_t *hw_mode_list;
	qdf_freq_t sbs_lower_band_end_freq;
	struct policy_mgr_freq_range freq_range_caps[MODE_HW_MAX][MAX_MAC];
	struct policy_mgr_freq_range cur_mac_freq_range[MAX_MAC];
};

/**
 * struct dual_mac_config - Dual MAC configurations
 * @prev_scan_config: Previous scan configuration
 * @prev_fw_mode_config: Previous FW mode configuration
 * @cur_scan_config: Current scan configuration
 * @cur_fw_mode_config: Current FW mode configuration
 * @req_scan_config: Requested scan configuration
 * @req_fw_mode_config: Requested FW mode configuration
 */
struct dual_mac_config {
	uint32_t prev_scan_config;
	uint32_t prev_fw_mode_config;
	uint32_t cur_scan_config;
	uint32_t cur_fw_mode_config;
	uint32_t req_scan_config;
	uint32_t req_fw_mode_config;
};

/**
 * enum policy_mgr_pri_id - vdev type priority id
 * @PM_STA_PRI_ID: station vdev type priority id
 * @PM_SAP_PRI_ID: sap vdev type priority id
 * @PM_P2P_GO_PRI_ID: p2p go vdev type priority id
 * @PM_P2P_CLI_PRI_ID: p2p cli vdev type priority id
 * @PM_MAX_PRI_ID: vdev type priority id max value
 */
enum policy_mgr_pri_id {
	PM_STA_PRI_ID = 1,
	PM_SAP_PRI_ID,
	PM_P2P_GO_PRI_ID,
	PM_P2P_CLI_PRI_ID,
	PM_MAX_PRI_ID = 0xF,
};

#define PM_GET_BAND_PREFERRED(_policy_) ((_policy_) & 0x1)
#define PM_GET_VDEV_PRIORITY_ENABLED(_policy_) (((_policy_) & 0x2) >> 1)

/**
 * struct policy_mgr_user_cfg - Policy manager user config variables
 * @enable2x2: 2x2 chain mask user config
 * @sub_20_mhz_enabled: Is 5 or 10 Mhz enabled
 */
struct policy_mgr_user_cfg {
	bool enable2x2;
	bool sub_20_mhz_enabled;
};

/**
 * struct dbs_bw - Max BW supported in DBS mode
 * @mac0_bw: BW of MAC0
 * @mac1_bw: BW of MAC1
 */
struct dbs_bw {
	enum hw_mode_bandwidth mac0_bw;
	enum hw_mode_bandwidth mac1_bw;
};

/**
 * struct dbs_nss - Number of spatial streams in DBS mode
 * @mac0_ss: Number of spatial streams on MAC0
 * @mac1_ss: Number of spatial streams on MAC1
 * @single_mac0_band_cap: Mac0 band capability for single mac hw mode
 */
struct dbs_nss {
	enum hw_mode_ss_config mac0_ss;
	enum hw_mode_ss_config mac1_ss;
	uint32_t single_mac0_band_cap;
};

/*
 * Max radio combination numbers
 */
#define MAX_RADIO_COMBINATION 16

/**
 * struct radio_combination - Radio combination
 * @hw_mode: hw mode type
 * @band_mask: band support type for each mac
 * @antenna: antenna support for each mac
 */
struct radio_combination {
	enum policy_mgr_mode hw_mode;
	uint8_t band_mask[MAX_MAC];
	uint8_t antenna[MAX_MAC];
};

/**
 * struct connection_info - connection information
 * @mac_id: The HW mac it is running
 * @vdev_id: vdev id
 * @channel: channel of the connection
 * @ch_freq: channel freq in Mhz
 */
struct connection_info {
	uint8_t mac_id;
	uint8_t vdev_id;
	uint8_t channel;
	uint32_t ch_freq;
};

/**
 * struct go_plus_go_force_scc - structure to hold p2p go
 * params for forcescc restart
 *
 * @vdev_id: vdev id of first p2p go which needs to do csa
 * @ch_freq: ch freq of curr p2p go
 * @ch_width: ch width of curr p2p go
 */
struct go_plus_go_force_scc {
	uint8_t vdev_id;
	uint32_t ch_freq;
	uint32_t ch_width;
};

/**
 * struct sap_plus_go_force_scc - structure to hold
 * params for forcescc restart in sap plus go
 *
 * @reason: channel change reason code
 * @initiator_vdev_id: the first interface ID to trigger CSA
 * @responder_vdev_id: the second interface ID to follow CSA
 */
struct sap_plus_go_force_scc {
	enum sap_csa_reason_code reason;
	uint8_t initiator_vdev_id;
	uint8_t responder_vdev_id;
};

/**
 * struct sta_ap_intf_check_work_ctx - sta_ap_intf_check_work
 * related info
 * @psoc: pointer to PSOC object information
 * @go_plus_go_force_scc: structure to hold params of
 *			  curr and first p2p go ctx
 * @sap_plus_go_force_scc: sap p2p force SCC ctx
 * @nan_force_scc_in_progress: NAN force scc in progress
 */
struct sta_ap_intf_check_work_ctx {
	struct wlan_objmgr_psoc *psoc;
	struct go_plus_go_force_scc go_plus_go_force_scc;
	struct sap_plus_go_force_scc sap_plus_go_force_scc;
	uint8_t nan_force_scc_in_progress;
};

/**
 * enum indoor_conc_update_type - Indoor concurrency update type
 * @CONNECT: On a new STA connection
 * @DISCONNECT_WITHOUT_CONCURRENCY: On a STA disconnection with no active
 * sessions on the same frequency
 * @DISCONNECT_WITH_CONCURRENCY: On a STA disconnection with an active
 * session on the same frequency
 * @SWITCH_WITH_CONCURRENCY: On a STA roam or CSA to a different channel
 * with a concurrent SAP on previous frequency
 * @SWITCH_WITHOUT_CONCURRENCY: On a STA roam or CSA to a different channel
 * without any concurrent SAP on previous frequency
 */
enum indoor_conc_update_type {
	CONNECT,
	DISCONNECT_WITHOUT_CONCURRENCY,
	DISCONNECT_WITH_CONCURRENCY,
	SWITCH_WITHOUT_CONCURRENCY,
	SWITCH_WITH_CONCURRENCY,
};

#endif /* __WLAN_POLICY_MGR_PUBLIC_STRUCT_H */
