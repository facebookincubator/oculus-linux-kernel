/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 *
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
#include "osapi_linux.h"
#include <wmi_unified.h>
#include "wlan_pmo_hw_filter_public_struct.h"
#include "wlan_action_oui_public_struct.h"
#include "wlan_hdd_green_ap_cfg.h"

struct hdd_context;

#define FW_MODULE_LOG_LEVEL_STRING_LENGTH  (512)
#define TX_SCHED_WRR_PARAM_STRING_LENGTH   (50)
#define TX_SCHED_WRR_PARAMS_NUM            (5)
#define CFG_ENABLE_RX_THREAD		(1 << 0)
#define CFG_ENABLE_RPS			(1 << 1)
#define CFG_ENABLE_NAPI			(1 << 2)
#define CFG_ENABLE_DYNAMIC_RPS		(1 << 3)

#ifdef DHCP_SERVER_OFFLOAD
#define IPADDR_NUM_ENTRIES     (4)
#define IPADDR_STRING_LENGTH   (16)
#endif

#define CFG_DBS_SCAN_PARAM_LENGTH          (42)

/* Number of items that can be configured */
#define MAX_CFG_INI_ITEMS   1024

#define MAX_PRB_REQ_VENDOR_OUI_INI_LEN 160
#define VENDOR_SPECIFIC_IE_BITMAP 0x20000000

#define CFG_CONCURRENT_IFACE_MAX_LEN 16

/* Defines for all of the things we read from the configuration (registry). */
/*
 * <ini>
 * gEnableConnectedScan - Will enable or disable scan in connected state
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable the scanning in
 * Connected state
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * <ini>
 */

#define CFG_ENABLE_CONNECTED_SCAN_NAME        "gEnableConnectedScan"
#define CFG_ENABLE_CONNECTED_SCAN_MIN         (0)
#define CFG_ENABLE_CONNECTED_SCAN_MAX         (1)
#define CFG_ENABLE_CONNECTED_SCAN_DEFAULT     (1)

/*
 * <ini>
 * mws_coex_pcc_channel_avoid_delay - configures the duration, when WWAN PCC
 * (Primary Component Carrier) conflicts with WLAN channel.
 * @Min: 0x00
 * @Max: 0xFF
 * @Default: 0x3C
 *
 * It is used to set MWS-COEX WWAN PCC channel avoidance delay
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MWS_COEX_PCC_CHANNEL_AVOID_DELAY		"mws_coex_pcc_channel_avoid_delay"
#define CFG_MWS_COEX_PCC_CHANNEL_AVOID_DELAY_DEFAULT	(0x3C)
#define CFG_MWS_COEX_PCC_CHANNEL_AVOID_DELAY_MAX	(0xFF)
#define CFG_MWS_COEX_PCC_CHANNEL_AVOID_DELAY_MIN	(0)

/*
 * <ini>
 * mws_coex_scc_channel_avoid_delay - configures the duration, when WWAN SCC
 * (Secondary Component Carrier) conflicts with WLAN channel.
 * @Min: 0x00
 * @Max: 0xFF
 * @Default: 0x78
 *
 * It is used to set MWS-COEX WWAN SCC channel avoidance delay
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MWS_COEX_SCC_CHANNEL_AVOID_DELAY "mws_coex_scc_channel_avoid_delay"
#define CFG_MWS_COEX_SCC_CHANNEL_AVOID_DELAY_DEFAULT 0x00
#define CFG_MWS_COEX_SCC_CHANNEL_AVOID_DELAY_MAX 0xFF
#define CFG_MWS_COEX_SCC_CHANNEL_AVOID_DELAY_MIN 0x00

#ifdef WLAN_NUD_TRACKING
/*
 * <ini>
 * gEnableNUDTracking - Will enable or disable NUD tracking within driver
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable NUD tracking within driver
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * <ini>
 */

#define CFG_ENABLE_NUD_TRACKING_NAME            "gEnableNUDTracking"
#define CFG_ENABLE_NUD_TRACKING_MIN             (0)
#define CFG_ENABLE_NUD_TRACKING_MAX             (1)
#define CFG_ENABLE_NUD_TRACKING_DEFAULT         (1)
#endif

/*
 * <ini>
 * RTSThreshold - Will provide RTSThreshold
 * @Min: 0
 * @Max: 1048576
 * @Default: 2347
 *
 * This ini is used to set default RTSThreshold
 * If minimum value 0 is selectd then it will use always RTS
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_RTS_THRESHOLD_NAME                 "RTSThreshold"
#define CFG_RTS_THRESHOLD_MIN                  WNI_CFG_RTS_THRESHOLD_STAMIN     /* min is 0, meaning always use RTS. */
#define CFG_RTS_THRESHOLD_MAX                  WNI_CFG_RTS_THRESHOLD_STAMAX     /* max is the max frame size */
#define CFG_RTS_THRESHOLD_DEFAULT              WNI_CFG_RTS_THRESHOLD_STADEF

/*
 * <ini>
 * gFragmentationThreshold - It will set fragmentation threshold
 * @Min: 256
 * @Max: 8000
 * @Default: 8000
 *
 * This ini is used to indicate default fragmentation threshold
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_FRAG_THRESHOLD_NAME                "gFragmentationThreshold"
#define CFG_FRAG_THRESHOLD_MIN                 WNI_CFG_FRAGMENTATION_THRESHOLD_STAMIN
#define CFG_FRAG_THRESHOLD_MAX                 WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX
#define CFG_FRAG_THRESHOLD_DEFAULT             WNI_CFG_FRAGMENTATION_THRESHOLD_STADEF

#define CFG_OPERATING_CHANNEL_NAME             "gOperatingChannel"
#define CFG_OPERATING_CHANNEL_MIN              (0)
#define CFG_OPERATING_CHANNEL_MAX              (14)
#define CFG_OPERATING_CHANNEL_DEFAULT          (1)

/*
 * <ini>
 * gShortSlotTimeEnabled - It will set slot timing slot.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default timing slot.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_SHORT_SLOT_TIME_ENABLED_NAME       "gShortSlotTimeEnabled"
#define CFG_SHORT_SLOT_TIME_ENABLED_MIN        WNI_CFG_SHORT_SLOT_TIME_STAMIN
#define CFG_SHORT_SLOT_TIME_ENABLED_MAX        WNI_CFG_SHORT_SLOT_TIME_STAMAX
#define CFG_SHORT_SLOT_TIME_ENABLED_DEFAULT    WNI_CFG_SHORT_SLOT_TIME_STADEF

#define CFG_11D_SUPPORT_ENABLED_NAME           "g11dSupportEnabled"
#define CFG_11D_SUPPORT_ENABLED_MIN            WNI_CFG_11D_ENABLED_STAMIN
#define CFG_11D_SUPPORT_ENABLED_MAX            WNI_CFG_11D_ENABLED_STAMAX
#define CFG_11D_SUPPORT_ENABLED_DEFAULT        WNI_CFG_11D_ENABLED_STADEF       /* Default is ON */

/*
 * <ini>
 * enable_11d_in_world_mode - enable 11d in world mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini enables 11d in world mode, irrespective of value of
 * g11dSupportEnabled
 *
 * Usage: External
 *
 * </ini>
 */
 #define CFG_ENABLE_11D_IN_WORLD_MODE_NAME "enable_11d_in_world_mode"
 #define CFG_ENABLE_11D_IN_WORLD_MODE_MIN     (0)
 #define CFG_ENABLE_11D_IN_WORLD_MODE_MAX     (1)
 #define CFG_ENABLE_11D_IN_WORLD_MODE_DEFAULT (0)

#define CFG_11H_SUPPORT_ENABLED_NAME           "g11hSupportEnabled"
#define CFG_11H_SUPPORT_ENABLED_MIN            (0)
#define CFG_11H_SUPPORT_ENABLED_MAX            (1)
#define CFG_11H_SUPPORT_ENABLED_DEFAULT        (1)    /* Default is ON */

/* COUNTRY Code Priority */
#define CFG_COUNTRY_CODE_PRIORITY_NAME         "gCountryCodePriority"
#define CFG_COUNTRY_CODE_PRIORITY_MIN          (0)
#define CFG_COUNTRY_CODE_PRIORITY_MAX          (1)
#define CFG_COUNTRY_CODE_PRIORITY_DEFAULT      (0)

#define CFG_HEARTBEAT_THRESH_24_NAME           "gHeartbeat24"
#define CFG_HEARTBEAT_THRESH_24_MIN            WNI_CFG_HEART_BEAT_THRESHOLD_STAMIN
#define CFG_HEARTBEAT_THRESH_24_MAX            WNI_CFG_HEART_BEAT_THRESHOLD_STAMAX
#define CFG_HEARTBEAT_THRESH_24_DEFAULT        WNI_CFG_HEART_BEAT_THRESHOLD_STADEF

/*
 * <ini>
 * gMaxRxAmpduFactor - Provide the maximum ampdu factor.
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set default maxampdu factor
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_MAX_RX_AMPDU_FACTOR_NAME         "gMaxRxAmpduFactor"
#define CFG_MAX_RX_AMPDU_FACTOR_MIN          WNI_CFG_MAX_RX_AMPDU_FACTOR_STAMIN
#define CFG_MAX_RX_AMPDU_FACTOR_MAX          WNI_CFG_MAX_RX_AMPDU_FACTOR_STAMAX
#define CFG_MAX_RX_AMPDU_FACTOR_DEFAULT      WNI_CFG_MAX_RX_AMPDU_FACTOR_STADEF

/* Configuration option for HT MPDU density (Table 8-125 802.11-2012)
 * 0 for no restriction
 * 1 for 1/4 micro sec
 * 2 for 1/2 micro sec
 * 3 for 1 micro sec
 * 4 for 2 micro sec
 * 5 for 4 micro sec
 * 6 for 8 micro sec
 * 7 for 16 micro sec
 */
#define CFG_HT_MPDU_DENSITY_NAME               "ght_mpdu_density"
#define CFG_HT_MPDU_DENSITY_MIN                WNI_CFG_MPDU_DENSITY_STAMIN
#define CFG_HT_MPDU_DENSITY_MAX                WNI_CFG_MPDU_DENSITY_STAMAX
#define CFG_HT_MPDU_DENSITY_DEFAULT            WNI_CFG_MPDU_DENSITY_STADEF

#define CFG_REG_CHANGE_DEF_COUNTRY_NAME          "gRegulatoryChangeCountry"
#define CFG_REG_CHANGE_DEF_COUNTRY_DEFAULT       (0)
#define CFG_REG_CHANGE_DEF_COUNTRY_MIN           (0)
#define CFG_REG_CHANGE_DEF_COUNTRY_MAX           (1)

#define CFG_ADVERTISE_CONCURRENT_OPERATION_NAME    "gAdvertiseConcurrentOperation"
#define CFG_ADVERTISE_CONCURRENT_OPERATION_DEFAULT (1)
#define CFG_ADVERTISE_CONCURRENT_OPERATION_MIN     (0)
#define CFG_ADVERTISE_CONCURRENT_OPERATION_MAX     (1)

enum hdd_dot11_mode {
	eHDD_DOT11_MODE_AUTO = 0,       /* covers all things we support */
	eHDD_DOT11_MODE_abg,    /* 11a/b/g only, no HT, no proprietary */
	eHDD_DOT11_MODE_11b,
	eHDD_DOT11_MODE_11g,
	eHDD_DOT11_MODE_11n,
	eHDD_DOT11_MODE_11g_ONLY,
	eHDD_DOT11_MODE_11n_ONLY,
	eHDD_DOT11_MODE_11b_ONLY,
	eHDD_DOT11_MODE_11ac_ONLY,
	eHDD_DOT11_MODE_11ac,
	eHDD_DOT11_MODE_11a,
	eHDD_DOT11_MODE_11ax_ONLY,
	eHDD_DOT11_MODE_11ax,
};

/*
 * <ini>
 * gChannelBondingMode24GHz - Configures Channel Bonding in 24 GHz
 * @Min: 0
 * @Max: 10
 * @Default: 1
 *
 * This ini is used to set default channel bonding mode 24GHZ
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_CHANNEL_BONDING_MODE_24GHZ_NAME    "gChannelBondingMode24GHz"
#define CFG_CHANNEL_BONDING_MODE_MIN           WNI_CFG_CHANNEL_BONDING_MODE_STAMIN
#define CFG_CHANNEL_BONDING_MODE_MAX           WNI_CFG_CHANNEL_BONDING_MODE_STAMAX
#define CFG_CHANNEL_BONDING_MODE_DEFAULT       WNI_CFG_CHANNEL_BONDING_MODE_STADEF

/*
 * <ini>
 * override_ht20_40_24g - use channel Bonding in 24 GHz from supplicant
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to use channel Bonding in 24 GHz from supplicant if
 * gChannelBondingMode24GHz is set
 *
 * Related: gChannelBondingMode24GHz
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_OVERRIDE_HT40_20_24GHZ_NAME    "override_ht20_40_24g"
#define CFG_OVERRIDE_HT40_20_24GHZ_MIN           0
#define CFG_OVERRIDE_HT40_20_24GHZ_MAX           1
#define CFG_OVERRIDE_HT40_20_24GHZ_DEFAULT       0

/*
 * <ini>
 * gChannelBondingMode5GHz - Configures Channel Bonding in 5 GHz
 * @Min: 0
 * @Max: 10
 * @Default: 0
 *
 * This ini is used to set default channel bonding mode 5GHZ
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_CHANNEL_BONDING_MODE_5GHZ_NAME     "gChannelBondingMode5GHz"
#define CFG_CHANNEL_BONDING_MODE_MIN           WNI_CFG_CHANNEL_BONDING_MODE_STAMIN
#define CFG_CHANNEL_BONDING_MODE_MAX           WNI_CFG_CHANNEL_BONDING_MODE_STAMAX
#define CFG_CHANNEL_BONDING_MODE_DEFAULT       WNI_CFG_CHANNEL_BONDING_MODE_STADEF

/*
 * <ini>
 * gShortGI20Mhz - Short Guard Interval for HT20
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default short interval for HT20
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_SHORT_GI_20MHZ_NAME                "gShortGI20Mhz"
#define CFG_SHORT_GI_20MHZ_MIN                 WNI_CFG_SHORT_GI_20MHZ_STAMIN
#define CFG_SHORT_GI_20MHZ_MAX                 WNI_CFG_SHORT_GI_20MHZ_STAMAX
#define CFG_SHORT_GI_20MHZ_DEFAULT             WNI_CFG_SHORT_GI_20MHZ_STADEF

/*
 * <ini>
 * gScanResultAgeCount - Set scan result age count
 * @Min: 1
 * @Max: 100
 * @Default: 1
 *
 * This ini parameter is the number of times a scan
 * doesn't find it before it is removed from results.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SCAN_RESULT_AGE_COUNT_NAME         "gScanResultAgeCount"
#define CFG_SCAN_RESULT_AGE_COUNT_MIN          (1)
#define CFG_SCAN_RESULT_AGE_COUNT_MAX          (100)
#define CFG_SCAN_RESULT_AGE_COUNT_DEFAULT      (1)

/*
 * <ini>
 * gNeighborScanTimerPeriod - Set neighbor scan timer period
 * @Min: 3
 * @Max: 300
 * @Default: 200
 *
 * This ini is used to set the timer period in secs after
 * which neighbor scan is trigerred.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_NAME             "gNeighborScanTimerPeriod"
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN              (3)
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX              (300)
#define CFG_NEIGHBOR_SCAN_TIMER_PERIOD_DEFAULT          (100)

/*
 * <ini>
 * gRoamRestTimeMin - Set min neighbor scan timer period
 * @Min: 3
 * @Max: 300
 * @Default: 200
 *
 * This is the min rest time after which firmware will check for traffic
 * and if there no traffic it will move to a new channel to scan
 * else it will stay on the home channel till gNeighborScanTimerPeriod time
 * and then will move to a new channel to scan.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NEIGHBOR_SCAN_MIN_TIMER_PERIOD_NAME         "gRoamRestTimeMin"
#define CFG_NEIGHBOR_SCAN_MIN_TIMER_PERIOD_MIN          (3)
#define CFG_NEIGHBOR_SCAN_MIN_TIMER_PERIOD_MAX          (300)
#define CFG_NEIGHBOR_SCAN_MIN_TIMER_PERIOD_DEFAULT      (50)

/*
 * <ini>
 * gOpportunisticThresholdDiff - Set oppurtunistic threshold diff
 * @Min: 0
 * @Max: 127
 * @Default: 0
 *
 * This ini is used to set opportunistic threshold diff.
 * This parameter is the RSSI diff above neighbor lookup
 * threshold, when opportunistic scan should be triggered.
 * MAX value is chosen so that this type of scan can be
 * always enabled by user.
 * MIN value will cause opportunistic scan to be triggered
 * in neighbor lookup RSSI range.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF_NAME            "gOpportunisticThresholdDiff"
#define CFG_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF_MIN             (0)
#define CFG_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF_MAX             (127)
#define CFG_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF_DEFAULT         (0)

/*
 * <ini>
 * gNeighborScanChannelList - Set channels to be scanned
 * by firmware for LFR scan
 * @Default: ""
 *
 * This ini is used to set the channels to be scanned
 * by firmware for LFR scan.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NEIGHBOR_SCAN_CHAN_LIST_NAME                      "gNeighborScanChannelList"
#define CFG_NEIGHBOR_SCAN_CHAN_LIST_DEFAULT                   ""

/*
 * <ini>
 * gNeighborScanChannelMinTime - Set neighbor scan channel min time
 * @Min: 10
 * @Max: 40
 * @Default: 20
 *
 * This ini is used to set the minimum time in secs spent on each
 * channel in LFR scan inside firmware.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_NAME                  "gNeighborScanChannelMinTime"
#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN                   (10)
#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX                   (40)
#define CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_DEFAULT               (20)

/*
 * <ini>
 * gNeighborScanChannelMaxTime - Set neighbor scan channel max time
 * @Min: 3
 * @Max: 300
 * @Default: 40
 *
 * This ini is used to set the maximum time in secs spent on each
 * channel in LFR scan inside firmware.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_NAME                  "gNeighborScanChannelMaxTime"
#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN                   (3)
#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX                   (300)
#define CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_DEFAULT               (40)

/*
 * <ini>
 * gNeighborScanRefreshPeriod - Set neighbor scan refresh period
 * @Min: 1000
 * @Max: 60000
 * @Default: 20000
 *
 * This ini is used by firmware to set scan refresh period
 * in msecs for lfr scan.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_NAME         "gNeighborScanRefreshPeriod"
#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN          (1000)
#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX          (60000)
#define CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_DEFAULT      (20000)

/**
 * <ini>
 * gFullRoamScanPeriod - Set full roam scan refresh period
 * @Min: 0
 * @Max: 600
 * @Default: 0
 *
 * This ini is used by firmware to set full roam scan period in secs.
 * Full roam scan period is the minimum idle period in seconds between two
 * successive full channel roam scans. If this is configured as a non-zero,
 * full roam scan will be triggered for every configured interval.
 * If this configured as 0, full roam scan will not be triggered at all.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_FULL_ROAM_SCAN_REFRESH_PERIOD_NAME         "gFullRoamScanPeriod"
#define CFG_FULL_ROAM_SCAN_REFRESH_PERIOD_MIN          (0)
#define CFG_FULL_ROAM_SCAN_REFRESH_PERIOD_MAX          (600)
#define CFG_FULL_ROAM_SCAN_REFRESH_PERIOD_DEFAULT      (0)

/*
 * <ini>
 * gEmptyScanRefreshPeriod - Set empty scan refresh period
 * @Min: 0
 * @Max: 60000
 * @Default: 0
 *
 * This ini is used by firmware to set scan period in msecs
 * following empty scan results.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EMPTY_SCAN_REFRESH_PERIOD_NAME         "gEmptyScanRefreshPeriod"
#define CFG_EMPTY_SCAN_REFRESH_PERIOD_MIN          (0)
#define CFG_EMPTY_SCAN_REFRESH_PERIOD_MAX          (60000)
#define CFG_EMPTY_SCAN_REFRESH_PERIOD_DEFAULT      (0)

/*
 * <ini>
 * gEnableDFSChnlScan - Enable DFS channel scan
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable DFS channel
 * scan
 */
#define CFG_ENABLE_DFS_CHNL_SCAN_NAME              "gEnableDFSChnlScan"
#define CFG_ENABLE_DFS_CHNL_SCAN_MIN               (0)
#define CFG_ENABLE_DFS_CHNL_SCAN_MAX               (1)
#define CFG_ENABLE_DFS_CHNL_SCAN_DEFAULT           (1)

/*
 * <ini>
 * honour_nl_scan_policy_flags - Whether to honour NL80211 scan policy flags
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This parameter will decide whether to honour scan flags such as
 * NL80211_SCAN_FLAG_HIGH_ACCURACY , NL80211_SCAN_FLAG_LOW_SPAN,
 * NL80211_SCAN_FLAG_LOW_POWER.
 * Acceptable values for this:
 * 0: Config is disabled
 * 1: Config is enabled
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_HONOUR_NL_SCAN_POLICY_FLAGS           "honour_nl_scan_policy_flags"
#define CFG_HONOUR_NL_SCAN_POLICY_FLAGS_MIN       (0)
#define CFG_HONOUR_NL_SCAN_POLICY_FLAGS_MAX       (1)
#define CFG_HONOUR_NL_SCAN_POLICY_FLAGS_DEFAULT   (1)

/*
 * <ini>
 * wake_lock_in_user_scan - use to acquire wake lock during user scan
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to define if wake lock is held used during user scan req
 * This INI is added for a specific OEM on their request, who don’t want to
 * use PNO offload scan (sched scans). This is useful only if PNO scan offload
 * is disabled. If PNO scan is enabled this INI should be disabled and its
 * by default disabled intentionally.
 * This is used to acquire wake lock to handle the case where PNO scan offload
 * is disabled so that wlan is not suspended during scan before connect and
 * thus scan is not aborted in between. In case PNO scan is offloaded, the FW
 * will take care of connect scans and will wake up host when candidate is found
 *
 *
 * Related: Scan
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_WAKE_LOCK_IN_SCAN               "wake_lock_in_user_scan"
#define CFG_ENABLE_WAKE_LOCK_IN_SCAN_MIN           (0)
#define CFG_ENABLE_WAKE_LOCK_IN_SCAN_MAX           (1)
#define CFG_ENABLE_WAKE_LOCK_IN_SCAN_DEFAULT       (0)



/*
 * <ini>
 * pmkidModes - Enable PMKID modes
 * This INI is used to enable PMKID feature options
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMKID_MODES_NAME                       "pmkidModes"
#define CFG_PMKID_MODES_MIN                        (0x0)
#define CFG_PMKID_MODES_MAX                        (0x3)
#define CFG_PMKID_MODES_DEFAULT                    (0x3)
#define CFG_PMKID_MODES_OKC                        (0x1)
#define CFG_PMKID_MODES_PMKSA_CACHING              (0x2)

/*
 * <ini>
 * gEnableDFSPnoChnlScan - enable dfs channels in PNO scan
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable dfs channels in PNO scan request,
 * enabling this ini enables driver to include dfs channels in its
 * PNO scan request
 * Related: NA
 *
 * Supported Feature: DFS, PNO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_NAME              "gEnableDFSPnoChnlScan"
#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_MIN               (0)
#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_MAX               (1)
#define CFG_ENABLE_DFS_PNO_CHNL_SCAN_DEFAULT           (1)

/*
 * <ini>
 * gEnableFirstScan2GOnly - Enable first scan 2G only
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to scan 2G channels only in first scan.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_NAME            "gEnableFirstScan2GOnly"
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_MIN        (0)
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_MAX        (1)
#define CFG_ENABLE_FIRST_SCAN_2G_ONLY_DEFAULT    (0)

/*
 * <ini>
 * gScanAgingTime - Set scan aging time
 * @Min: 0
 * @Max: 200
 * @Default: 30
 *
 * This ini is used to set scan aging timeout value
 * in secs. For example after 30 secs the bss results
 * greater than 30secs age will be flushed.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SCAN_AGING_PARAM_NAME          "gScanAgingTime"
#define CFG_SCAN_AGING_PARAM_MIN           (0)
#define CFG_SCAN_AGING_PARAM_MAX           (200)
#ifdef QCA_WIFI_NAPIER_EMULATION
#define CFG_SCAN_AGING_PARAM_DEFAULT       (90)
#else
#define CFG_SCAN_AGING_PARAM_DEFAULT       (30)
#endif

#ifdef FEATURE_WLAN_SCAN_PNO
/*
 * <ini>
 * gPNOScanSupport - Enable or Disable PNO scan
 * @Min: 1
 * @Max: 0
 * @Default: 1
 *
 * This ini is used to Enable or Disable PNO scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PNO_SCAN_SUPPORT                         "gPNOScanSupport"
#define CFG_PNO_SCAN_SUPPORT_ENABLE                  (1)
#define CFG_PNO_SCAN_SUPPORT_DISABLE                 (0)
#define CFG_PNO_SCAN_SUPPORT_DEFAULT                 (1)

/*
 * <ini>
 * gPNOScanTimerRepeatValue - Set PNO scan timer repeat value
 * @Min: 30
 * @Max: 0
 * @Default: 0xffffffff
 *
 * This ini is used by firmware to set fast scan max cycles
 * equal to gPNOScanTimerRepeatValue. Taking power consumption
 * into account firmware after gPNOScanTimerRepeatValue times
 * fast_scan_period switches to slow_scan_period.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE              "gPNOScanTimerRepeatValue"
#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE_DEFAULT      (30)
#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE_MIN          (0)
#define CFG_PNO_SCAN_TIMER_REPEAT_VALUE_MAX          (0xffffffff)

/*
 * <ini>
 * gPNOSlowScanMultiplier - Set PNO slow scan multiplier
 * @Min: 6
 * @Max: 0
 * @Default: 30
 *
 * This ini is used by firmware to set slow scan period
 * as gPNOSlowScanMultiplier times fast_scan_period.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PNO_SLOW_SCAN_MULTIPLIER                 "gPNOSlowScanMultiplier"
#define CFG_PNO_SLOW_SCAN_MULTIPLIER_DEFAULT         (6)
#define CFG_PNO_SLOW_SCAN_MULTIPLIER_MIN             (0)
#define CFG_PNO_SLOW_SCAN_MULTIPLIER_MAX             (30)
#endif

/*
 * <ini>
 * max_scan_count - Set maximum number of scans
 * @Min: 1
 * @Max: 8
 * @Default: 4
 *
 * This ini is used to set the maximum number of
 * scans that host can queue at firmware.
 * Rome firmware support 8 scan queue size and 4
 * are reserved for internal scan requests like
 * roaming. So host can send 4 scan requests.
 * In iHelium, there is no constraint in number of
 * scan queue size at firmware but the current use
 * cases needs support of maximum of 4 scan request
 * from host.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_SCAN_COUNT_NAME           "max_scan_count"
#define CFG_MAX_SCAN_COUNT_MIN            (1)
#define CFG_MAX_SCAN_COUNT_MAX            (8)
#define CFG_MAX_SCAN_COUNT_DEFAULT        (4)

/*
 * <ini>
 * drop_bcn_on_chan_mismatch - drop the beacon for chan mismatch
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to decide drop the beacon or not if channel received
 * in metadata doesn't match the one in beacon.
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DROP_BCN_ON_CHANNEL_MISMATCH      "drop_bcn_on_chan_mismatch"
#define CFG_DROP_BCN_ON_CHANNEL_MISMATCH_MIN       (0)
#define CFG_DROP_BCN_ON_CHANNEL_MISMATCH_MAX       (1)
#define CFG_DROP_BCN_ON_CHANNEL_MISMATCH_DEFAULT   (1)

/*
 * <ini>
 * gPassiveMaxChannelTime - Set max channel time for passive scan
 * @Min: 0
 * @Max: 10000
 * @Default: 110
 *
 * This ini is used to set maximum channel time in secs spent in
 * passive scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PASSIVE_MAX_CHANNEL_TIME_NAME      "gPassiveMaxChannelTime"
#define CFG_PASSIVE_MAX_CHANNEL_TIME_MIN       (0)
#define CFG_PASSIVE_MAX_CHANNEL_TIME_MAX       (10000)
#define CFG_PASSIVE_MAX_CHANNEL_TIME_DEFAULT   (110)

/*
 * <ini>
 * gPassiveMinChannelTime - Set min channel time for passive scan
 * @Min: 0
 * @Max: 10000
 * @Default: 60
 *
 * This ini is used to set minimum channel time in secs spent in
 * passive scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PASSIVE_MIN_CHANNEL_TIME_NAME      "gPassiveMinChannelTime"
#define CFG_PASSIVE_MIN_CHANNEL_TIME_MIN       (0)
#define CFG_PASSIVE_MIN_CHANNEL_TIME_MAX       (10000)
#define CFG_PASSIVE_MIN_CHANNEL_TIME_DEFAULT   (60)

/*
 * <ini>
 * gActiveMaxChannelTime - Set max channel time for active scan
 * @Min: 0
 * @Max: 10000
 * @Default: 40
 *
 * This ini is used to set maximum channel time in secs spent in
 * active scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTIVE_MAX_CHANNEL_TIME_NAME       "gActiveMaxChannelTime"
#define CFG_ACTIVE_MAX_CHANNEL_TIME_MIN        (0)
#define CFG_ACTIVE_MAX_CHANNEL_TIME_MAX        (10000)
#define CFG_ACTIVE_MAX_CHANNEL_TIME_DEFAULT    (40)

/*
 * <ini>
 * active_max_channel_time_2g - Set max time for active 2G channel scan
 * @Min: 0
 * @Max: 10000
 * @Default: 80
 *
 * This ini is used to set maximum time in msecs spent in active 2G channel scan
 * if it's not zero, in case of zero, CFG_ACTIVE_MAX_CHANNEL_TIME is used for 2G
 * channels also.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTIVE_MAX_2G_CHANNEL_TIME_NAME       "active_max_channel_time_2g"
#define CFG_ACTIVE_MAX_2G_CHANNEL_TIME_MIN        (0)
#define CFG_ACTIVE_MAX_2G_CHANNEL_TIME_MAX        (10000)
#define CFG_ACTIVE_MAX_2G_CHANNEL_TIME_DEFAULT    (80)


/*
 * <ini>
 * gActiveMinChannelTime - Set min channel time for active scan
 * @Min: 0
 * @Max: 10000
 * @Default: 20
 *
 * This ini is used to set minimum channel time in secs spent in
 * active scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTIVE_MIN_CHANNEL_TIME_NAME       "gActiveMinChannelTime"
#define CFG_ACTIVE_MIN_CHANNEL_TIME_MIN        (0)
#define CFG_ACTIVE_MIN_CHANNEL_TIME_MAX        (10000)
#define CFG_ACTIVE_MIN_CHANNEL_TIME_DEFAULT    (20)

/*
 * <ini>
 * gScanNumProbes - Set the number of probes on each channel for active scan
 * @Min: 0
 * @Max: 20
 * @Default: 0
 *
 * This ini is used to set number of probes on each channel for
 * active scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SCAN_NUM_PROBES_NAME       "gScanNumProbes"
#define CFG_SCAN_NUM_PROBES_MIN        (0)
#define CFG_SCAN_NUM_PROBES_MAX        (20)
#define CFG_SCAN_NUM_PROBES_DEFAULT    (0)

/*
 * <ini>
 * gScanProbeRepeatTime - Set the probe repeat time on each channel for active scan
 * @Min: 0
 * @Max: 30
 * @Default: 0
 *
 * This ini is used to set probe repeat time on each channel for
 * active scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SCAN_PROBE_REPEAT_TIME_NAME       "gScanProbeRepeatTime"
#define CFG_SCAN_PROBE_REPEAT_TIME_MIN        (0)
#define CFG_SCAN_PROBE_REPEAT_TIME_MAX        (30)
#define CFG_SCAN_PROBE_REPEAT_TIME_DEFAULT    (0)

#ifdef FEATURE_WLAN_EXTSCAN
/*
 * <ini>
 * gExtScanEnable - Enable external scan
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to control enabling of external scan
 * feature.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTSCAN_ALLOWED_NAME                   "gExtScanEnable"
#define CFG_EXTSCAN_ALLOWED_MIN                    (0)
#define CFG_EXTSCAN_ALLOWED_MAX                    (1)
#define CFG_EXTSCAN_ALLOWED_DEF                    (1)

/*
 * <ini>
 * gExtScanPassiveMaxChannelTime - Set max channel time for external
 * passive scan
 * @Min: 0
 * @Max: 500
 * @Default: 110
 *
 * This ini is used to set maximum channel time  in secs spent in
 * external passive scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTSCAN_PASSIVE_MAX_CHANNEL_TIME_NAME      "gExtScanPassiveMaxChannelTime"
#define CFG_EXTSCAN_PASSIVE_MAX_CHANNEL_TIME_MIN       (0)
#define CFG_EXTSCAN_PASSIVE_MAX_CHANNEL_TIME_MAX       (500)
#define CFG_EXTSCAN_PASSIVE_MAX_CHANNEL_TIME_DEFAULT   (110)

/*
 * <ini>
 * gExtScanPassiveMinChannelTime - Set min channel time for external
 * passive scan
 * @Min: 0
 * @Max: 500
 * @Default: 60
 *
 * This ini is used to set minimum channel time in secs spent in
 * external passive scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTSCAN_PASSIVE_MIN_CHANNEL_TIME_NAME      "gExtScanPassiveMinChannelTime"
#define CFG_EXTSCAN_PASSIVE_MIN_CHANNEL_TIME_MIN       (0)
#define CFG_EXTSCAN_PASSIVE_MIN_CHANNEL_TIME_MAX       (500)
#define CFG_EXTSCAN_PASSIVE_MIN_CHANNEL_TIME_DEFAULT   (60)

/*
 * <ini>
 * gExtScanActiveMaxChannelTime - Set min channel time for external
 * active scan
 * @Min: 0
 * @Max: 110
 * @Default: 40
 *
 * This ini is used to set maximum channel time in secs spent in
 * external active scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTSCAN_ACTIVE_MAX_CHANNEL_TIME_NAME       "gExtScanActiveMaxChannelTime"
#define CFG_EXTSCAN_ACTIVE_MAX_CHANNEL_TIME_MIN        (0)
#define CFG_EXTSCAN_ACTIVE_MAX_CHANNEL_TIME_MAX        (110)
#define CFG_EXTSCAN_ACTIVE_MAX_CHANNEL_TIME_DEFAULT    (40)

/*
 * <ini>
 * gExtScanActiveMinChannelTime - Set min channel time for external
 * active scan
 * @Min: 0
 * @Max: 110
 * @Default: 20
 *
 * This ini is used to set minimum channel time in secs spent in
 * external active scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTSCAN_ACTIVE_MIN_CHANNEL_TIME_NAME       "gExtScanActiveMinChannelTime"
#define CFG_EXTSCAN_ACTIVE_MIN_CHANNEL_TIME_MIN        (0)
#define CFG_EXTSCAN_ACTIVE_MIN_CHANNEL_TIME_MAX        (110)
#define CFG_EXTSCAN_ACTIVE_MIN_CHANNEL_TIME_DEFAULT    (20)
#endif

/*
 * <ini>
 * gChPredictionFullScanMs - Set periodic timer for channel
 * prediction
 * @Min: 3000
 * @Max: 0x7fffffff
 * @Default: 60000
 *
 * This ini is used to set the periodic timer upon which
 * a full scan needs to be triggered when PNO channel
 * prediction feature is enabled. This parameter is intended
 * to tweak the internal algortihm for experiments.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_CHANNEL_PREDICTION_FULL_SCAN_MS_NAME      "gChPredictionFullScanMs"
#define CFG_CHANNEL_PREDICTION_FULL_SCAN_MS_MIN       (30000)
#define CFG_CHANNEL_PREDICTION_FULL_SCAN_MS_MAX       (0x7fffffff)
#define CFG_CHANNEL_PREDICTION_FULL_SCAN_MS_DEFAULT   (60000)

/*
 * <ini>
 * gEnableEarlyStopScan - Set early stop scan
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set early stop scan. Early stop
 * scan is a feature for roaming to stop the scans at
 * an early stage as soon as we find a better AP to roam.
 * This would make the roaming happen quickly.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EARLY_STOP_SCAN_ENABLE           "gEnableEarlyStopScan"
#define CFG_EARLY_STOP_SCAN_ENABLE_MIN       (0)
#define CFG_EARLY_STOP_SCAN_ENABLE_MAX       (1)
#define CFG_EARLY_STOP_SCAN_ENABLE_DEFAULT   (0)

/*
 * <ini>
 * gEarlyStopScanMinThreshold - Set early stop scan min
 * threshold
 * @Min: -80
 * @Max: -70
 * @Default: -73
 *
 * This ini is used to set the early stop scan minimum
 * threshold. Early stop scan minimum threshold is the
 * minimum threshold to be considered for stopping the
 * scan. The algorithm starts with a scan on the greedy
 * channel list with the maximum threshold and steps down
 * the threshold by 20% for each further channel. It can
 * step down on each channel but cannot go lower than the
 * minimum threshold.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EARLY_STOP_SCAN_MIN_THRESHOLD           "gEarlyStopScanMinThreshold"
#define CFG_EARLY_STOP_SCAN_MIN_THRESHOLD_MIN       (-80)
#define CFG_EARLY_STOP_SCAN_MIN_THRESHOLD_MAX       (-70)
#define CFG_EARLY_STOP_SCAN_MIN_THRESHOLD_DEFAULT   (-73)

/*
 * <ini>
 * gEarlyStopScanMaxThreshold - Set early stop scan max
 * threshold
 * @Min: -60
 * @Max: -40
 * @Default: -43
 *
 * This ini is used to set the the early stop scan maximum
 * threshold at which the candidate AP should be to be
 * qualified as a potential roam candidate and good enough
 * to stop the roaming scan.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EARLY_STOP_SCAN_MAX_THRESHOLD           "gEarlyStopScanMaxThreshold"
#define CFG_EARLY_STOP_SCAN_MAX_THRESHOLD_MIN       (-60)
#define CFG_EARLY_STOP_SCAN_MAX_THRESHOLD_MAX       (-40)
#define CFG_EARLY_STOP_SCAN_MAX_THRESHOLD_DEFAULT   (-43)

/*
 * <ini>
 * gFirstScanBucketThreshold - Set first scan bucket
 * threshold
 * @Min: -50
 * @Max: -30
 * @Default: -30
 *
 * This ini will configure the first scan bucket
 * threshold to the mentioned value and all the AP's which
 * have RSSI under this threshold will fall under this
 * bucket. This configuration item used to tweak and
 * test the input for internal algorithm.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_FIRST_SCAN_BUCKET_THRESHOLD_NAME      "gFirstScanBucketThreshold"
#define CFG_FIRST_SCAN_BUCKET_THRESHOLD_MIN       (-50)
#define CFG_FIRST_SCAN_BUCKET_THRESHOLD_MAX       (-30)
#define CFG_FIRST_SCAN_BUCKET_THRESHOLD_DEFAULT   (-30)

/*
 * <ini>
 * obss_active_dwelltime - Set obss active dwelltime
 * @Min: 5
 * @Max: 1000
 * @Default: 10
 *
 * This ini is used to set dwell time in secs for active
 * obss scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_NAME    "obss_active_dwelltime"
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_MIN     (5)
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_MAX     (1000)
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_DEFAULT (10)

/*
 * <ini>
 * obss_passive_dwelltime - Set obss passive dwelltime
 * @Min: 10
 * @Max: 1000
 * @Default: 20
 *
 * This ini is used to set dwell time in secs for passive
 * obss scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_NAME   "obss_passive_dwelltime"
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_MIN    (10)
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_MAX    (1000)
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_DEFAULT (20)

/*
 * <ini>
 * obss_width_trigger_interval - Set obss trigger interval
 * @Min: 10
 * @Max: 900
 * @Default: 200
 *
 * This ini is used during an OBSS scan operation,
 * where each channel in the set is scanned at least
 * once per configured trigger interval time.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_NAME "obss_width_trigger_interval"
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_MIN  (10)
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_MAX  (900)
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_DEFAULT (200)

/*
 * <ini>
 * gbug_report_for_scan_results - Enable bug report
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to create bug report in
 * case of nil scan results.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CREATE_BUG_REPORT_FOR_SCAN       "gbug_report_for_scan_results"
#define CFG_CREATE_BUG_REPORT_FOR_SCAN_DISABLE    (0)
#define CFG_CREATE_BUG_REPORT_FOR_SCAN_ENABLE     (1)
#define CFG_CREATE_BUG_REPORT_FOR_SCAN_DEFAULT    (0)

/*
 * <ini>
 * hostscan_adaptive_dwell_mode - Enable adaptive dwell mode
 * during host scan with conneciton
 * @Min: 0
 * @Max: 4
 * @Default: 2
 *
 * This ini will set the algo used in dwell time optimization
 * during host scan with connection.
 * See enum scan_dwelltime_adaptive_mode.
 * Acceptable values for this:
 * 0: Default (Use firmware default mode)
 * 1: Conservative optimization
 * 2: Moderate optimization
 * 3: Aggressive optimization
 * 4: Static
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_NAME        "hostscan_adaptive_dwell_mode"
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_MIN         (0)
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_MAX         (4)
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_DEFAULT     (2)

/*
 * <ini>
 * hostscan_adaptive_dwell_mode_no_conn - Enable adaptive dwell mode
 * during host scan without connection
 * @Min: 0
 * @Max: 4
 * @Default: 4
 *
 * This ini will set the algo used in dwell time optimization
 * during host scan without connection.
 * See enum scan_dwelltime_adaptive_mode.
 * Acceptable values for this:
 * 0: Default (Use firmware default mode)
 * 1: Conservative optimization
 * 2: Moderate optimization
 * 3: Aggressive optimization
 * 4: Static
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_NC_NAME    "hostscan_adaptive_dwell_mode_no_conn"
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_NC_MIN     (0)
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_NC_MAX     (4)
#define CFG_ADAPTIVE_SCAN_DWELL_MODE_NC_DEFAULT (4)

/*
 * <ini>
 * extscan_adaptive_dwell_mode - Enable adaptive dwell mode
 * during ext scan
 * @Min: 0
 * @Max: 4
 * @Default: 1
 *
 * This ini will set the algo used in dwell time optimization
 * during ext scan. see enum scan_dwelltime_adaptive_mode.
 * Acceptable values for this:
 * 0: Default (Use firmware default mode)
 * 1: Conservative optimization
 * 2: Moderate optimization
 * 3: Aggressive optimization
 * 4: Static
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_EXTSCAN_DWELL_MODE_NAME     "extscan_adaptive_dwell_mode"
#define CFG_ADAPTIVE_EXTSCAN_DWELL_MODE_MIN      (0)
#define CFG_ADAPTIVE_EXTSCAN_DWELL_MODE_MAX      (4)
#define CFG_ADAPTIVE_EXTSCAN_DWELL_MODE_DEFAULT  (1)

/*
 * <ini>
 * pnoscan_adaptive_dwell_mode - Enable adaptive dwell mode
 * during pno scan
 * @Min: 0
 * @Max: 4
 * @Default: 1
 *
 * This ini will set the algo used in dwell time optimization
 * during pno scan. see enum scan_dwelltime_adaptive_mode.
 * Acceptable values for this:
 * 0: Default (Use firmware default mode)
 * 1: Conservative optimization
 * 2: Moderate optimization
 * 3: Aggressive optimization
 * 4: Static
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_PNOSCAN_DWELL_MODE_NAME     "pnoscan_adaptive_dwell_mode"
#define CFG_ADAPTIVE_PNOSCAN_DWELL_MODE_MIN      (0)
#define CFG_ADAPTIVE_PNOSCAN_DWELL_MODE_MAX      (4)
#define CFG_ADAPTIVE_PNOSCAN_DWELL_MODE_DEFAULT  (1)

/*
 * <ini>
 * adaptive_dwell_mode_enabled - Enable adaptive dwell mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This parameter will globally disable/enable the adaptive dwell config.
 * Following parameters will set different values of attributes for dwell
 * time optimization thus reducing total scan time.
 * Acceptable values for this:
 * 0: Config is disabled
 * 1: Config is enabled
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_DWELL_MODE_ENABLED_NAME      "adaptive_dwell_mode_enabled"
#define CFG_ADAPTIVE_DWELL_MODE_ENABLED_MIN       (0)
#define CFG_ADAPTIVE_DWELL_MODE_ENABLED_MAX       (1)
#define CFG_ADAPTIVE_DWELL_MODE_ENABLED_DEFAULT   (1)

/*
 * <ini>
 * global_adapt_dwelltime_mode - Set default adaptive mode
 * @Min: 0
 * @Max: 4
 * @Default: 0
 *
 * This parameter will set default adaptive mode, will be used if any of the
 * scan dwell mode is set to default.
 * For uses : see enum scan_dwelltime_adaptive_mode
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GLOBAL_ADAPTIVE_DWELL_MODE_NAME       "global_adapt_dwelltime_mode"
#define CFG_GLOBAL_ADAPTIVE_DWELL_MODE_MIN        (0)
#define CFG_GLOBAL_ADAPTIVE_DWELL_MODE_MAX        (4)
#define CFG_GLOBAL_ADAPTIVE_DWELL_MODE_DEFAULT    (0)

/*
 * <ini>
 * gRssiCatGap - Set Rssi CatGap
 * @Min: 5
 * @Max: 100
 * @Default: 5
 *
 * This ini is used to set default RssiCatGap
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_RSSI_CATEGORY_GAP_NAME             "gRssiCatGap"
#define CFG_RSSI_CATEGORY_GAP_MIN              (5)
#define CFG_RSSI_CATEGORY_GAP_MAX              (100)
#define CFG_RSSI_CATEGORY_GAP_DEFAULT          (5)

/*
 * <ini>
 * gRoamPrefer5GHz - Prefer roaming to 5GHz Bss
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to inform FW to prefer roaming to 5GHz BSS
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_PREFER_5GHZ                  "gRoamPrefer5GHz"
#define CFG_ROAM_PREFER_5GHZ_MIN              (0)
#define CFG_ROAM_PREFER_5GHZ_MAX              (1)
#define CFG_ROAM_PREFER_5GHZ_DEFAULT          (1)

/*
 * <ini>
 * gRoamIntraBand - Prefer roaming within Band
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to inform FW to prefer roaming within band
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_INTRA_BAND                   "gRoamIntraBand"
#define CFG_ROAM_INTRA_BAND_MIN               (0)
#define CFG_ROAM_INTRA_BAND_MAX               (1)
#define CFG_ROAM_INTRA_BAND_DEFAULT           (0)

/*
 * <ini>
 * FastRoamEnabled - Enable fast roaming
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to inform FW to enable fast roaming
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_FEATURE_ENABLED_NAME                        "FastRoamEnabled"
#define CFG_LFR_FEATURE_ENABLED_MIN                         (0)
#define CFG_LFR_FEATURE_ENABLED_MAX                         (1)
#define CFG_LFR_FEATURE_ENABLED_DEFAULT                     (0)

/*
 * <ini>
 * FastTransitionEnabled - Enable fast transition in case of 11r and ese.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to turn ON/OFF the whole neighbor roam, pre-auth, reassoc.
 * With this turned OFF 11r will completely not work. For 11r this flag has to
 * be ON. For ESE fastroam will not work.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_FAST_TRANSITION_ENABLED_NAME                    "FastTransitionEnabled"
#define CFG_FAST_TRANSITION_ENABLED_NAME_MIN                (0)
#define CFG_FAST_TRANSITION_ENABLED_NAME_MAX                (1)
#define CFG_FAST_TRANSITION_ENABLED_NAME_DEFAULT            (1)

/*
 * <ini>
 * RoamRssiDiff - Enable roam based on rssi
 * @Min: 0
 * @Max: 100
 * @Default: 5
 *
 * This INI is used to decide whether to Roam or not based on RSSI. AP1 is the
 * currently associated AP and AP2 is chosen for roaming. The Roaming will
 * happen only if AP2 has better Signal Quality and it has a RSSI better than
 * AP2. RoamRssiDiff is the number of units (typically measured in dB) AP2
 * is better than AP1.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_RSSI_DIFF_NAME                             "RoamRssiDiff"
#define CFG_ROAM_RSSI_DIFF_MIN                              (0)
#define CFG_ROAM_RSSI_DIFF_MAX                              (100)
#define CFG_ROAM_RSSI_DIFF_DEFAULT                          (5)

/*
 * <ini>
 * rssi_abs_thresh - The min RSSI of the candidate AP to consider roam
 * @Min: -96
 * @Max: 0
 * @Default: 0
 *
 * The RSSI value of the candidate AP should be higher than rssi_abs_thresh
 * to roam to the AP. 0 means no absolute minimum RSSI is required.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_RSSI_ABS_THRESHOLD_NAME        "rssi_abs_thresh"
#define CFG_ROAM_RSSI_ABS_THRESHOLD_MIN         (-96)
#define CFG_ROAM_RSSI_ABS_THRESHOLD_MAX         (0)
#define CFG_ROAM_RSSI_ABS_THRESHOLD_DEFAULT     (0)

/*
 * <ini>
 * gRoamScanNProbes - Sets the number of probes to be sent for firmware roaming
 * @Min: 1
 * @Max: 10
 * @Default: 2
 *
 * This INI is used to set the maximum number of probes the firmware can send
 * for firmware internal roaming cases.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_N_PROBES                              "gRoamScanNProbes"
#define CFG_ROAM_SCAN_N_PROBES_MIN                          (1)
#define CFG_ROAM_SCAN_N_PROBES_MAX                          (10)
#define CFG_ROAM_SCAN_N_PROBES_DEFAULT                      (2)

/*
 * <ini>
 * gRoamScanHomeAwayTime - Sets the Home Away Time to firmware
 * @Min: 0
 * @Max: 300
 * @Default: 0
 *
 * Home Away Time should be at least equal to (gNeighborScanChannelMaxTime
 * + (2*RFS)), where RFS is the RF Switching time(3). It is twice RFS
 * to consider the time to go off channel and return to the home channel.
 *
 * Related: gNeighborScanChannelMaxTime
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_HOME_AWAY_TIME                        "gRoamScanHomeAwayTime"
#define CFG_ROAM_SCAN_HOME_AWAY_TIME_MIN                    (0)
#define CFG_ROAM_SCAN_HOME_AWAY_TIME_MAX                    (300)
#define CFG_ROAM_SCAN_HOME_AWAY_TIME_DEFAULT                (0)

/*
 * <ini>
 * gRoamScanOffloadEnabled - Enable Roam Scan Offload
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable Roam Scan Offload in firmware
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED                       "gRoamScanOffloadEnabled"
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED_MIN                   (0)
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED_MAX                   (1)
#define CFG_ROAM_SCAN_OFFLOAD_ENABLED_DEFAULT               (1)

/*
 * <ini>
 * gRoamRescanRssiDiff - Sets RSSI for Scan trigger in firmware
 * @Min: 0
 * @Max: 100
 * @Default: 5
 *
 * This INI is the drop in RSSI value that will trigger a precautionary
 * scan by firmware. Max value is chosen in such a way that this type
 * of scan can be disabled by user.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_RESCAN_RSSI_DIFF_NAME                  "gRoamRescanRssiDiff"
#define CFG_ROAM_RESCAN_RSSI_DIFF_MIN                   (0)
#define CFG_ROAM_RESCAN_RSSI_DIFF_MAX                   (100)
#define CFG_ROAM_RESCAN_RSSI_DIFF_DEFAULT               (5)

/*
 * <ini>
 * gDroppedPktDisconnectTh - Sets dropped packet threshold in firmware
 * @Min: 0
 * @Max: 512
 * @Default: 512
 *
 * This INI is the packet drop threshold will trigger disconnect from remote
 * peer.
 *
 * Related: None
 *
 * Supported Feature: connection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DROPPED_PKT_DISCONNECT_TH_NAME      "gDroppedPktDisconnectTh"
#define CFG_DROPPED_PKT_DISCONNECT_TH_MIN       (0)
#define CFG_DROPPED_PKT_DISCONNECT_TH_MAX       (65535)
#define CFG_DROPPED_PKT_DISCONNECT_TH_DEFAULT   (512)

/*
 * <ini>
 * gForce1x1Exception - force 1x1 when connecting to certain peer
 * @Min: 0
 * @Max: 2
 * @Default: 2
 *
 * This INI when enabled will force 1x1 connection with certain peer.
 * The implementation for this ini would be as follows:-
 * Value 0: Even if the AP is present in OUI, 1x1 will not be forced
 * Value 1: If antenna sharing supported, then only do 1x1.
 * Value 2: If AP present in OUI, force 1x1 connection.

 *
 * Related: None
 *
 * Supported Feature: connection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_FORCE_1X1_NAME      "gForce1x1Exception"
#define CFG_FORCE_1X1_MIN       (0)
#define CFG_FORCE_1X1_MAX       (2)
#define CFG_FORCE_1X1_DEFAULT   (1)

/*
 * <ini>
 * g11bNumTxChains - Number of Tx Chanins in 11b mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * Number of Tx Chanins in 11b mode
 *
 *
 * Related: None
 *
 * Supported Feature: connection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_11B_NUM_TX_CHAIN_NAME      "g11bNumTxChains"
#define CFG_11B_NUM_TX_CHAIN_MIN       (0)
#define CFG_11B_NUM_TX_CHAIN_MAX       (2)
#define CFG_11B_NUM_TX_CHAIN_DEFAULT   (0)

/*
 * <ini>
 * g11agNumTxChains - Number of Tx Chanins in 11ag mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * Number of Tx Chanins in 11ag mode
 *
 *
 * Related: None
 *
 * Supported Feature: connection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_11AG_NUM_TX_CHAIN_NAME      "g11agNumTxChains"
#define CFG_11AG_NUM_TX_CHAIN_MIN       (0)
#define CFG_11AG_NUM_TX_CHAIN_MAX       (2)
#define CFG_11AG_NUM_TX_CHAIN_DEFAULT   (0)

/*
 * <ini>
 * gEnableFastRoamInConcurrency - Enable LFR roaming on STA during concurrency
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable Legacy fast roaming(LFR) on STA link during
 * concurrent sessions.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY          "gEnableFastRoamInConcurrency"
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_MIN      (0)
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_MAX      (1)
#define CFG_ENABLE_FAST_ROAM_IN_CONCURRENCY_DEFAULT  (1)

/*
 * <ini>
 * gSelect5GHzMargin - Sets RSSI preference for 5GHz over 2.4GHz AP.
 * @Min: 0
 * @Max: 60
 * @Default: 0
 *
 * Prefer connecting to 5G AP even if its RSSI is lower by gSelect5GHzMargin
 * dBm than 2.4G AP. This feature requires the dependent cfg.ini
 * "gRoamPrefer5GHz" set to 1
 *
 * Related: gRoamPrefer5GHz
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN                 "gSelect5GHzMargin"
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN_MIN             (0)
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN_MAX             (60)
#define CFG_STRICT_5GHZ_PREF_BY_MARGIN_DEFAULT         (0)

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*
 * <ini>
 * gRoamOffloadEnabled - enable/disable roam offload feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable/disable roam offload feature
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAMING_OFFLOAD_NAME                "gRoamOffloadEnabled"
#define CFG_ROAMING_OFFLOAD_MIN                 (0)
#define CFG_ROAMING_OFFLOAD_MAX                 (1)
#define CFG_ROAMING_OFFLOAD_DEFAULT             (1)

/*
 * <ini>
 * roam_triggers - Bitmap of roaming triggers. Setting this to
 * zero will disable roaming altogether for the STA interface.
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0xFFFF
 *
 * ROAM_TRIGGER_REASON_NONE        BIT 0
 * ROAM_TRIGGER_REASON_PER         BIT 1
 * ROAM_TRIGGER_REASON_BMISS       BIT 2
 * ROAM_TRIGGER_REASON_LOW_RSSI    BIT 3
 * ROAM_TRIGGER_REASON_HIGH_RSSI   BIT 4
 * ROAM_TRIGGER_REASON_PERIODIC    BIT 5
 * ROAM_TRIGGER_REASON_MAWC        BIT 6
 * ROAM_TRIGGER_REASON_DENSE       BIT 7
 * ROAM_TRIGGER_REASON_BACKGROUND  BIT 8
 * ROAM_TRIGGER_REASON_FORCED      BIT 9
 * ROAM_TRIGGER_REASON_BTM         BIT 10
 * ROAM_TRIGGER_REASON_UNIT_TEST   BIT 11
 * ROAM_TRIGGER_REASON_BSS_LOAD    BIT 12
 * ROAM_TRIGGER_REASON_DEAUTH      BIT 13
 * ROAM_TRIGGER_REASON_IDLE        BIT 14
 * ROAM_TRIGGER_REASON_STA_KICKOUT BIT 15
 * ROAM_TRIGGER_EXT_REASON_MAX     BIT 16
 * Bitmap corresponds to the enum roam_trigger_reason
 *
 * Related: none
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ROAM_TRIGGER_BITMAP        "roam_triggers"
#define CFG_ROAM_TRIGGER_BITMAP_MIN       0
#define CFG_ROAM_TRIGGER_BITMAP_MAX       0xFFFFFFFF
#define CFG_ROAM_TRIGGER_BITMAP_DEFAULT   0xFFFF

#endif

/*
 * <ini>
 * gRoamScanHiRssiMaxCount - Sets 5GHz maximum scan count
 * @Min: 0
 * @Max: 10
 * @Default: 3
 *
 * This INI is used to set maximum scan count in 5GHz
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_HI_RSSI_MAXCOUNT_NAME         "gRoamScanHiRssiMaxCount"
#define CFG_ROAM_SCAN_HI_RSSI_MAXCOUNT_MIN          (0)
#define CFG_ROAM_SCAN_HI_RSSI_MAXCOUNT_MAX          (10)
#define CFG_ROAM_SCAN_HI_RSSI_MAXCOUNT_DEFAULT      (3)

/*
 * <ini>
 * gRoamScanHiRssiDelta - Sets RSSI Delta for scan trigger
 * @Min: 0
 * @Max: 16
 * @Default: 10
 *
 * This INI is used to set change in RSSI at which scan is triggered
 * in 5GHz.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_HI_RSSI_DELTA_NAME           "gRoamScanHiRssiDelta"
#define CFG_ROAM_SCAN_HI_RSSI_DELTA_MIN            (0)
#define CFG_ROAM_SCAN_HI_RSSI_DELTA_MAX            (16)
#define CFG_ROAM_SCAN_HI_RSSI_DELTA_DEFAULT        (10)

/*
 * <ini>
 * gRoamScanHiRssiDelay - Sets minimum delay between 5GHz scans
 * @Min: 5000
 * @Max: 0x7fffffff
 * @Default: 15000
 *
 * This INI is used to set the minimum delay between 5GHz scans.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_HI_RSSI_DELAY_NAME            "gRoamScanHiRssiDelay"
#define CFG_ROAM_SCAN_HI_RSSI_DELAY_MIN             (5000)
#define CFG_ROAM_SCAN_HI_RSSI_DELAY_MAX             (0x7fffffff)
#define CFG_ROAM_SCAN_HI_RSSI_DELAY_DEFAULT         (15000)

/*
 * <ini>
 * gRoamScanHiRssiUpperBound - Sets upper bound after which 5GHz scan
 * @Min: -66
 * @Max: 0
 * @Default: -30
 *
 * This INI is used to set the RSSI upper bound above which the 5GHz scan
 * will not be performed.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_HI_RSSI_UB_NAME              "gRoamScanHiRssiUpperBound"
#define CFG_ROAM_SCAN_HI_RSSI_UB_MIN               (-66)
#define CFG_ROAM_SCAN_HI_RSSI_UB_MAX               (0)
#define CFG_ROAM_SCAN_HI_RSSI_UB_DEFAULT           (-30)

#ifdef FEATURE_LFR_SUBNET_DETECTION
/*
 * <ini>
 * gLFRSubnetDetectionEnable - Enable LFR3 subnet detection
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Enable IP subnet detection during legacy fast roming version 3. Legacy fast
 * roaming could roam across IP subnets without host processors' knowledge.
 * This feature enables firmware to wake up the host processor if it
 * successfully determines change in the IP subnet. Change in IP subnet could
 * potentially cause disruption in IP connnectivity if IP address is not
 * refreshed.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_LFR_SUBNET_DETECTION    "gLFRSubnetDetectionEnable"
#define CFG_ENABLE_LFR_SUBNET_MIN          (0)
#define CFG_ENABLE_LFR_SUBNET_MAX          (1)
#define CFG_ENABLE_LFR_SUBNET_DEFAULT      (1)
#endif /* FEATURE_LFR_SUBNET_DETECTION */

/*
 * <ini>
 * groam_dense_rssi_thresh_offset - Sets dense roam RSSI threshold diff
 * @Min: 0
 * @Max: 20
 * @Default: 10
 *
 * This INI is used to set offset value from normal RSSI threshold to dense
 * RSSI threshold Fw will optimize roaming based on new RSSI threshold once
 * it detects dense enviournment.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_DENSE_RSSI_THRE_OFFSET         "groam_dense_rssi_thresh_offset"
#define CFG_ROAM_DENSE_RSSI_THRE_OFFSET_MIN     (0)
#define CFG_ROAM_DENSE_RSSI_THRE_OFFSET_MAX     (20)
#define CFG_ROAM_DENSE_RSSI_THRE_OFFSET_DEFAULT (10)

/*
 * <ini>
 * enable_ftopen - enable/disable FT open feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable/disable FT open feature
*
* Related: None
*
* Supported Feature: Roaming
*
* Usage: External
*
* </ini>
*/
#define CFG_ROAM_FT_OPEN_ENABLE_NAME                "enable_ftopen"
#define CFG_ROAM_FT_OPEN_ENABLE_MIN                 (0)
#define CFG_ROAM_FT_OPEN_ENABLE_MAX                 (1)
#define CFG_ROAM_FT_OPEN_ENABLE_DEFAULT             (1)

/*
 * <ini>
 * groam_dense_min_aps - Sets minimum number of AP for dense roam
 * @Min: 1
 * @Max: 5
 * @Default: 3
 *
 * Minimum number of APs required for dense roam. FW will consider
 * environment as dense once it detects #APs operating is more than
 * groam_dense_min_aps.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_DENSE_MIN_APS         "groam_dense_min_aps"
#define CFG_ROAM_DENSE_MIN_APS_MIN     (1)
#define CFG_ROAM_DENSE_MIN_APS_MAX     (5)
#define CFG_ROAM_DENSE_MIN_APS_DEFAULT (3)

/*
 * <ini>
 * roam_bg_scan_bad_rssi_thresh - RSSI threshold for background roam
 * @Min: -96
 * @Max: 0
 * @Default: -76
 *
 * If the DUT is connected to an AP with weak signal, then the bad RSSI
 * threshold will be used as an opportunity to use the scan results
 * from other scan clients and try to roam if there is a better AP
 * available in the environment.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_BG_SCAN_BAD_RSSI_THRESHOLD_NAME  "roam_bg_scan_bad_rssi_thresh"
#define CFG_ROAM_BG_SCAN_BAD_RSSI_THRESHOLD_MIN     (-96)
#define CFG_ROAM_BG_SCAN_BAD_RSSI_THRESHOLD_MAX     (0)
#define CFG_ROAM_BG_SCAN_BAD_RSSI_THRESHOLD_DEFAULT (-76)

/*
 * <ini>
 * roam_data_rssi_threshold_triggers - Triggers of data rssi threshold for roam
 * @Min: 0
 * @Max: 0xffff
 * @Default: 0
 *
 * If the DUT is connected to an AP with weak signal, during latest
 * rx_data_inactivity_time, if there is no activity or avg of data_rssi is
 * better than roam_data_rssi_threshold(-70dbM), then suppress roaming
 * triggered by roam_data_rssi_threshold_triggers: low RSSI or bg scan.
 * Triggers bitmap definition:
 * ROAM_DATA_RSSI_FLAG_LOW_RSSI   1<<0
 * ROAM_DATA_RSSI_FLAG_BACKGROUND 1<<1
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_DATA_RSSI_THRESHOLD_TRIGGERS_NAME  "roam_data_rssi_threshold_triggers"
#define CFG_ROAM_DATA_RSSI_THRESHOLD_TRIGGERS_MIN     (0)
#define CFG_ROAM_DATA_RSSI_THRESHOLD_TRIGGERS_MAX     (0xffff)
#define CFG_ROAM_DATA_RSSI_THRESHOLD_TRIGGERS_DEFAULT (0x3)

/*
 * <ini>
 * roam_bg_scan_client_bitmap - Bitmap used to identify the scan clients
 * @Min: 0
 * @Max: 0x3FF
 * @Default: 0x
 *
 * This bitmap is used to define the client scans that need to be used
 * by the roaming module to perform a background roaming.
 * Currently supported bit positions are as follows:
 * Bit 0 is reserved in the firmware.
 * WMI_SCAN_CLIENT_NLO - 1
 * WMI_SCAN_CLIENT_EXTSCAN - 2
 * WMI_SCAN_CLIENT_ROAM - 3
 * WMI_SCAN_CLIENT_P2P - 4
 * WMI_SCAN_CLIENT_LPI - 5
 * WMI_SCAN_CLIENT_NAN - 6
 * WMI_SCAN_CLIENT_ANQP - 7
 * WMI_SCAN_CLIENT_OBSS - 8
 * WMI_SCAN_CLIENT_PLM - 9
 * WMI_SCAN_CLIENT_HOST - 10
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_BG_SCAN_CLIENT_BITMAP_NAME     "roam_bg_scan_client_bitmap"
#define CFG_ROAM_BG_SCAN_CLIENT_BITMAP_MIN      (0)
#define CFG_ROAM_BG_SCAN_CLIENT_BITMAP_MAX      (0x7FF)
#define CFG_ROAM_BG_SCAN_CLIENT_BITMAP_DEFAULT  (0x424)

/*
 * <ini>
 * roam_bg_scan_bad_rssi_thresh - Data RSSI threshold for background roam
 * @Min: -96
 * @Max: 0
 * @Default: -70
 *
 * If the DUT is connected to an AP with weak signal, during latest
 * rx_data_inactivity_time, if there is no activity or avg of data_rssi is
 * better than roam_data_rssi_threshold(-70dbM), then suppress roaming
 * triggered by roam_data_rssi_threshold_triggers: low RSSI or bg scan.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_DATA_RSSI_THRESHOLD_NAME  "roam_data_rssi_threshold"
#define CFG_ROAM_DATA_RSSI_THRESHOLD_MIN     (-96)
#define CFG_ROAM_DATA_RSSI_THRESHOLD_MAX     (0)
#define CFG_ROAM_DATA_RSSI_THRESHOLD_DEFAULT (-70)

/*
 * <ini>
 * rx_data_inactivity_time - Duration to check rx data rssi
 * @Min: 0
 * @Max: 100000 ms
 * @Default: 0
 *
 * If the DUT is connected to an AP with weak signal, during latest
 * rx_data_inactivity_time, if there is no activity or avg of data_rssi is
 * better than roam_data_rssi_threshold(-70dbM), then suppress roaming
 * triggered by roam_data_rssi_threshold_triggers: low RSSI or bg scan.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_DATA_INACTIVITY_TIME_NAME  "rx_data_inactivity_time"
#define CFG_RX_DATA_INACTIVITY_TIME_MIN     (0)
#define CFG_RX_DATA_INACTIVITY_TIME_MAX     (100000)
#define CFG_RX_DATA_INACTIVITY_TIME_DEFAULT (2000)

/*
 * <ini>
 * min_delay_btw_roam_scans - Min duration (in sec) allowed btw two
 * consecutive roam scans
 * @Min: 0
 * @Max: 60
 * @Default: 10
 *
 * Roam scan is not allowed if duration between two consecutive
 * roam scans is less than this time.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MIN_DELAY_BTW_ROAM_SCAN_NAME    "min_delay_btw_roam_scans"
#define CFG_MIN_DELAY_BTW_ROAM_SCAN_MIN     (0)
#define CFG_MIN_DELAY_BTW_ROAM_SCAN_MAX     (60)
#define CFG_MIN_DELAY_BTW_ROAM_SCAN_DEFAULT (10)

/*
 * <ini>
 * roam_trigger_reason_bitmask - Contains roam_trigger_reasons
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0x10DA
 *
 * Bitmask containing roam_trigger_reasons for which
 * min_delay_btw_roam_scans constraint should be applied.
 * Currently supported bit positions are as follows:
 * Bit 0 is reserved in the firmware.
 * WMI_ROAM_TRIGGER_REASON_PER - 1
 * WMI_ROAM_TRIGGER_REASON_BMISS - 2
 * WMI_ROAM_TRIGGER_REASON_LOW_RSSI - 3
 * WMI_ROAM_TRIGGER_REASON_HIGH_RSSI - 4
 * WMI_ROAM_TRIGGER_REASON_PERIODIC - 5
 * WMI_ROAM_TRIGGER_REASON_MAWC - 6
 * WMI_ROAM_TRIGGER_REASON_DENSE - 7
 * WMI_ROAM_TRIGGER_REASON_BACKGROUND - 8
 * WMI_ROAM_TRIGGER_REASON_FORCED - 9
 * WMI_ROAM_TRIGGER_REASON_BTM - 10
 * WMI_ROAM_TRIGGER_REASON_UNIT_TEST - 11
 * WMI_ROAM_TRIGGER_REASON_BSS_LOAD - 12
 * WMI_ROAM_TRIGGER_REASON_DEAUTH - 13
 * WMI_ROAM_TRIGGER_REASON_IDLE - 14
 * WMI_ROAM_TRIGGER_REASON_MAX - 15
 *
 * For Ex: 0xDA (PER, LOW_RSSI, HIGH_RSSI, MAWC, DENSE)
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_TRIGGER_REASON_BITMASK_NAME "roam_trigger_reason_bitmask"
#define CFG_ROAM_SCAN_TRIGGER_REASON_BITMASK_MIN     (0)
#define CFG_ROAM_SCAN_TRIGGER_REASON_BITMASK_MAX     (0xFFFFFFFF)
#define CFG_ROAM_SCAN_TRIGGER_REASON_BITMASK_DEFAULT (0x10DA)

/*
 * <ini>
 * roaming_scan_policy - To config roaming scan policy
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to configure roaming scan behavior from HOST
 * 0 : DBS scan
 * 1 : Non-DBS scan
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_SCAN_POLICY_NAME "roaming_scan_policy"
#define CFG_ROAM_SCAN_SCAN_POLICY_MIN     (0)
#define CFG_ROAM_SCAN_SCAN_POLICY_MAX     (1)
#define CFG_ROAM_SCAN_SCAN_POLICY_DEFAULT (0)

/*
 * <ini>
 * roam_bad_rssi_thresh_offset_2g - RSSI threshold offset for 2G to 5G roam
 * @Min: 0
 * @Max: 86
 * @Default: 40
 *
 * If the DUT is connected to an AP with weak signal in 2G band, then the
 * bad RSSI offset for 2g would be used as offset from the bad RSSI
 * threshold configured and then use the resulting rssi for an opportunity
 * to use the scan results from other scan clients and try to roam to
 * 5G Band ONLY if there is a better AP available in the environment.
 *
 * For example if the roam_bg_scan_bad_rssi_thresh is -76 and
 * roam_bad_rssi_thresh_offset_2g is 40 then the difference of -36 would be
 * used as a trigger to roam to a 5G AP if DUT initially connected to a 2G AP
 *
 * Related: roam_bg_scan_bad_rssi_thresh
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_BG_SCAN_BAD_RSSI_OFFSET_2G_NAME "roam_bad_rssi_thresh_offset_2g"
#define CFG_ROAM_BG_SCAN_BAD_RSSI_OFFSET_2G_MIN     (0)
#define CFG_ROAM_BG_SCAN_BAD_RSSI_OFFSET_2G_MAX     (86)
#define CFG_ROAM_BG_SCAN_BAD_RSSI_OFFSET_2G_DEFAULT (40)

/*
 * <ini>
 * ho_delay_for_rx - Delay Hand-off (In msec) by this duration to receive
 * pending rx frames from current BSS
 * @Min: 0
 * @Max: 200
 * @Default: 0
 *
 * For LFR 3.0 roaming scenario, once roam candidate is found, firmware
 * waits for minimum this much duration to receive pending rx frames from
 * current BSS before switching to new channel for handoff to new AP.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_HO_DELAY_FOR_RX_NAME    "ho_delay_for_rx"
#define CFG_ROAM_HO_DELAY_FOR_RX_MIN     (0)
#define CFG_ROAM_HO_DELAY_FOR_RX_MAX     (200)
#define CFG_ROAM_HO_DELAY_FOR_RX_DEFAULT (0)

/*
 * <ini>
 * roam_force_rssi_trigger - To force RSSI trigger
 * irrespective of channel list type
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set roam scan mode
 * WMI_ROAM_SCAN_MODE_RSSI_CHANGE, irrespective of whether
 * channel list type is CHANNEL_LIST_STATIC or not
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_FORCE_RSSI_TRIGGER_NAME  "roam_force_rssi_trigger"
#define CFG_ROAM_FORCE_RSSI_TRIGGER_MIN     (0)
#define CFG_ROAM_FORCE_RSSI_TRIGGER_MAX     (1)
#define CFG_ROAM_FORCE_RSSI_TRIGGER_DEFAULT (1)

/*
 * <ini>
 * roamscan_adaptive_dwell_mode - Sets dwell time adaptive mode
 * @Min: 0
 * @Max: 4
 * @Default: 1
 *
 * This parameter will set the algo used in dwell time optimization during
 * roam scan. see enum scan_dwelltime_adaptive_mode.
 * Acceptable values for this:
 * 0: Default (Use firmware default mode)
 * 1: Conservative optimization
 * 2: Moderate optimization
 * 3: Aggressive optimization
 * 4: Static
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_ROAMSCAN_DWELL_MODE_NAME    "roamscan_adaptive_dwell_mode"
#define CFG_ADAPTIVE_ROAMSCAN_DWELL_MODE_MIN     (0)
#define CFG_ADAPTIVE_ROAMSCAN_DWELL_MODE_MAX     (4)
#define CFG_ADAPTIVE_ROAMSCAN_DWELL_MODE_DEFAULT (1)

/*
 * Timer waiting for interface up from the upper layer. If
 * this timer expires all the cds modules shall be closed.
 * Time Units: ms
 */
#define CFG_INTERFACE_CHANGE_WAIT_NAME    "gInterfaceChangeWait"
#define CFG_INTERFACE_CHANGE_WAIT_MIN     (10)
#define CFG_INTERFACE_CHANGE_WAIT_MAX     (500000)
#define CFG_INTERFACE_CHANGE_WAIT_DEFAULT (10000)

/*
 * <ini>
 * gShortPreamble - Set Short Preamble
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default short Preamble
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_SHORT_PREAMBLE_NAME                "gShortPreamble"
#define CFG_SHORT_PREAMBLE_MIN                 WNI_CFG_SHORT_PREAMBLE_STAMIN
#define CFG_SHORT_PREAMBLE_MAX                 WNI_CFG_SHORT_PREAMBLE_STAMAX
#define CFG_SHORT_PREAMBLE_DEFAULT             WNI_CFG_SHORT_PREAMBLE_STADEF

/*
 * <ini>
 * gIbssBssid - Default IBSS BSSID if BSSID is not provided by supplicant
 * @Min: "000000000000"
 * @Max: "ffffffffffff"
 * @Default: "000AF5040506"
 *
 * This ini is used to set Default IBSS BSSID if BSSID
 * is not provided by supplicant and Coalesing is disabled
 *
 * Related: Only applicable if gCoalesingInIBSS is 0
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_BSSID_NAME                    "gIbssBssid"
#define CFG_IBSS_BSSID_MIN                     "000000000000"
#define CFG_IBSS_BSSID_MAX                     "ffffffffffff"
#define CFG_IBSS_BSSID_DEFAULT                 "000AF5040506"

/*
 * <ini>
 * gAdHocChannel5G - Default 5Ghz IBSS channel if channel is not
 * provided by supplicant.
 * @Min: 36
 * @Max: 165
 * @Default: 44
 *
 * This ini is used to set default 5Ghz IBSS channel
 * if channel is not provided by supplicant and band is 5Ghz
 *
 * Related: None
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_NAME          "gAdHocChannel5G"
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_MIN           (36)
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_MAX           (165)
#define CFG_IBSS_ADHOC_CHANNEL_5GHZ_DEFAULT       (44)

/*
 * <ini>
 * gAdHocChannel24G - Default 2.4Ghz IBSS channel if channel is not
 * provided by supplicant.
 * @Min: 1
 * @Max: 14
 * @Default: 6
 *
 * This ini is used to set default 2.4Ghz IBSS channel
 * if channel is not provided by supplicant and band is 2.4Ghz
 *
 * Related: None
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_NAME         "gAdHocChannel24G"
#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_MIN          (1)
#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_MAX          (14)
#define CFG_IBSS_ADHOC_CHANNEL_24GHZ_DEFAULT      (6)

/*
 * <ini>
 * gCoalesingInIBSS - If IBSS coalesing is enabled.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set IBSS coalesing
 *
 * Related: None
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_COALESING_IN_IBSS_NAME                "gCoalesingInIBSS"
#define CFG_COALESING_IN_IBSS_MIN                 (0)
#define CFG_COALESING_IN_IBSS_MAX                 (1)
#define CFG_COALESING_IN_IBSS_DEFAULT             (0)   /* disabled */

/*
 * <ini>
 * gIbssATIMWinSize - Set IBSS ATIM window size
 * @Min: 0
 * @Max: 50
 * @Default: 0
 *
 * This ini is used to set IBSS ATIM window size
 *
 * Related: None
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_ATIM_WIN_SIZE_NAME                "gIbssATIMWinSize"
#define CFG_IBSS_ATIM_WIN_SIZE_MIN                 (0)
#define CFG_IBSS_ATIM_WIN_SIZE_MAX                 (50)
#define CFG_IBSS_ATIM_WIN_SIZE_DEFAULT             (0)


/*
 * <ini>
 * gIbssIsPowerSaveAllowed - Indicates if IBSS Power Save is
 * supported or not
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to Indicates if IBSS Power Save is
 * supported or not. When not allowed,IBSS station has
 * to stay awake all the time and should never set PM=1
 * in its transmitted frames.
 *
 * Related: valid only when gIbssATIMWinSize is non-zero
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_IS_POWER_SAVE_ALLOWED_NAME        "gIbssIsPowerSaveAllowed"
#define CFG_IBSS_IS_POWER_SAVE_ALLOWED_MIN         (0)
#define CFG_IBSS_IS_POWER_SAVE_ALLOWED_MAX         (1)
#define CFG_IBSS_IS_POWER_SAVE_ALLOWED_DEFAULT     (1)

/*
 * <ini>
 * gIbssIsPowerCollapseAllowed - Indicates if IBSS Power Collapse
 * is allowed
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to indicates if IBSS Power Collapse
 * is allowed
 *
 * Related: None
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_IS_POWER_COLLAPSE_ALLOWED_NAME    "gIbssIsPowerCollapseAllowed"
#define CFG_IBSS_IS_POWER_COLLAPSE_ALLOWED_MIN     (0)
#define CFG_IBSS_IS_POWER_COLLAPSE_ALLOWED_MAX     (1)
#define CFG_IBSS_IS_POWER_COLLAPSE_ALLOWED_DEFAULT (1)

/*
 * <ini>
 * gIbssAwakeOnTxRx - Indicates whether IBSS station
 * can exit power save mode and enter power active
 * state whenever there is a TX/RX activity.
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to ndicates whether IBSS station
 * can exit power save mode and enter power active
 * state whenever there is a TX/RX activity.
 *
 * Related: None
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_AWAKE_ON_TX_RX_NAME               "gIbssAwakeOnTxRx"
#define CFG_IBSS_AWAKE_ON_TX_RX_MIN                (0)
#define CFG_IBSS_AWAKE_ON_TX_RX_MAX                (1)
#define CFG_IBSS_AWAKE_ON_TX_RX_DEFAULT            (0)

/*
 * <ini>
 * gIbssInactivityTime - Indicates the data
 * inactivity time in number of beacon intervals
 * after which IBSS station re-inters power save
 *
 * @Min: 1
 * @Max: 10
 * @Default: 1
 *
 * In IBSS mode if Awake on TX/RX activity is enabled
 * Ibss Inactivity parameter indicates the data
 * inactivity time in number of beacon intervals
 * after which IBSS station re-inters power save
 * by sending Null frame with PM=1
 *
 * Related: Aplicable if gIbssAwakeOnTxRx is enabled
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_INACTIVITY_TIME_NAME              "gIbssInactivityTime"
#define CFG_IBSS_INACTIVITY_TIME_MIN               (1)
#define CFG_IBSS_INACTIVITY_TIME_MAX               (10)
#define CFG_IBSS_INACTIVITY_TIME_DEFAULT           (1)

/*
 * <ini>
 * gIbssTxSpEndInactivityTime - Indicates the time after
 * which TX Service Period is terminated by
 * sending a Qos Null frame with EOSP.
 *
 * @Min: 0
 * @Max: 100
 * @Default: 0
 *
 * In IBSS mode Tx Service Period Inactivity
 * time in msecs indicates the time after
 * which TX Service Period is terminated by
 * sending a Qos Null frame with EOSP.
 * If value is 0, TX SP is terminated with the
 * last buffered packet itself instead of waiting
 * for the inactivity.
 *
 * Related: None
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_TXSP_END_INACTIVITY_NAME          "gIbssTxSpEndInactivityTime"
#define CFG_IBSS_TXSP_END_INACTIVITY_MIN           (0)
#define CFG_IBSS_TXSP_END_INACTIVITY_MAX           (100)
#define CFG_IBSS_TXSP_END_INACTIVITY_DEFAULT       (0)

/*
 * <ini>
 * gIbssPsWarmupTime - PS-supporting device
 * does not enter protocol sleep state during first
 * gIbssPsWarmupTime seconds.
 *
 * @Min: 0
 * @Max: 65535
 * @Default: 0
 *
 * When IBSS network is initialized, PS-supporting device
 * does not enter protocol sleep state during first
 * gIbssPsWarmupTime seconds.
 *
 * Related: valid if gIbssIsPowerSaveAllowed is set
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_PS_WARMUP_TIME_NAME               "gIbssPsWarmupTime"
#define CFG_IBSS_PS_WARMUP_TIME_MIN                (0)
/* Allow unsigned Int Max for now */
#define CFG_IBSS_PS_WARMUP_TIME_MAX                (65535)
#define CFG_IBSS_PS_WARMUP_TIME_DEFAULT            (0)

/*
 * <ini>
 * gIbssPs1RxChainInAtim - IBSS Power Save Enable/Disable 1 RX
 * chain usage during the ATIM window
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * IBSS Power Save Enable/Disable 1 RX
 * chain usage during the ATIM window
 *
 * Related: Depend on gIbssIsPowerSaveAllowed
 *
 * Supported Feature: IBSS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_NAME    "gIbssPs1RxChainInAtim"
#define CFG_IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_MIN     (0)
#define CFG_IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_MAX     (1)
#define CFG_IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_DEFAULT (0)

/*
 * <ini>
 * wlm_latency_enable - WLM latency Enable
 *
 * @min: 0
 * @max: 1
 * @default: 1
 *
 * 0 - disable
 * 1 - enable
 *
 * </ini>
 */
#define CFG_LATENCY_ENABLE_NAME    "wlm_latency_enable"
#define CFG_LATENCY_ENABLE_MIN     (0)
#define CFG_LATENCY_ENABLE_MAX     (1)
#define CFG_LATENCY_ENABLE_DEFAULT (1)

/*
 * <ini>
 * wlm_latency_level - WLM latency level
 * Define 4 latency level to gain latency
 *
 * @min: 0
 * @max: 3
 * @defalut: 0
 *
 * 0 - normal
 * 1 - moderate
 * 2 - low
 * 3 - ultralow
 *
 * </ini>
 */
#define CFG_LATENCY_LEVEL_NAME    "wlm_latency_level"
#define CFG_LATENCY_LEVEL_MIN     (0)
#define CFG_LATENCY_LEVEL_MAX     (3)
#define CFG_LATENCY_LEVEL_DEFAULT (0)

/*
 * <ini>
 * wlm_latency_flags_normal - WLM flags setting for normal level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @defalut: 0x0
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_NORMAL_NAME    "wlm_latency_flags_normal"
#define CFG_LATENCY_FLAGS_NORMAL_MIN     (0x0)
#define CFG_LATENCY_FLAGS_NORMAL_MAX     (0xffffffff)
#define CFG_LATENCY_FLAGS_NORMAL_DEFAULT (0x0)

/*
 * <ini>
 * wlm_latency_flags_moderate - WLM flags setting for moderate level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @defalut: 0x8
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_MODERATE_NAME    "wlm_latency_flags_moderate"
#define CFG_LATENCY_FLAGS_MODERATE_MIN     (0x0)
#define CFG_LATENCY_FLAGS_MODERATE_MAX     (0xffffffff)
#define CFG_LATENCY_FLAGS_MODERATE_DEFAULT (0x8)

/*
 * <ini>
 * wlm_latency_flags_low - WLM flags setting for low level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @defalut: 0xa
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_LOW_NAME    "wlm_latency_flags_low"
#define CFG_LATENCY_FLAGS_LOW_MIN     (0x0)
#define CFG_LATENCY_FLAGS_LOW_MAX     (0xffffffff)
#define CFG_LATENCY_FLAGS_LOW_DEFAULT (0xa)

/*
 * <ini>
 * wlm_latency_flags_ultralow - WLM flags setting for ultralow level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @defalut: 0xc83
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_ULTRALOW_NAME    "wlm_latency_flags_ultralow"
#define CFG_LATENCY_FLAGS_ULTRALOW_MIN     (0x0)
#define CFG_LATENCY_FLAGS_ULTRALOW_MAX     (0xffffffff)
#define CFG_LATENCY_FLAGS_ULTRALOW_DEFAULT (0xc83)

#define CFG_INTF0_MAC_ADDR_NAME                  "Intf0MacAddress"
#define CFG_INTF0_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF0_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF0_MAC_ADDR_DEFAULT               "000AF58989FF"

#define CFG_INTF1_MAC_ADDR_NAME                  "Intf1MacAddress"
#define CFG_INTF1_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF1_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF1_MAC_ADDR_DEFAULT               "000AF58989FE"

#define CFG_INTF2_MAC_ADDR_NAME                  "Intf2MacAddress"
#define CFG_INTF2_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF2_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF2_MAC_ADDR_DEFAULT               "000AF58989FD"

#define CFG_INTF3_MAC_ADDR_NAME                  "Intf3MacAddress"
#define CFG_INTF3_MAC_ADDR_MIN                   "000000000000"
#define CFG_INTF3_MAC_ADDR_MAX                   "ffffffffffff"
#define CFG_INTF3_MAC_ADDR_DEFAULT               "000AF58989FC"

/*
 * <ini>
 * gDot11Mode - SAP phy mode
 * @Min: 0
 * @Max: 12 (11ax)
 * @Default: 12 (11ax)
 *
 * This ini is used to set Phy Mode (auto, b, g, n, etc/) Valid values are
 * 0-12, with 0 = Auto, 12 = 11ax.
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DOT11_MODE_NAME                    "gDot11Mode"
#define CFG_DOT11_MODE_MIN                     eHDD_DOT11_MODE_AUTO
#define CFG_DOT11_MODE_DEFAULT                 eHDD_DOT11_MODE_11ax
#define CFG_DOT11_MODE_MAX                     eHDD_DOT11_MODE_11ax

/*
 * <ini>
 * gEnableApUapsd - Enable/disable UAPSD for SoftAP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to setup setup U-APSD for Acs at association
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_QOS_UAPSD_MODE_NAME             "gEnableApUapsd"
#define CFG_AP_QOS_UAPSD_MODE_MIN              (0)
#define CFG_AP_QOS_UAPSD_MODE_MAX              (1)
#define CFG_AP_QOS_UAPSD_MODE_DEFAULT          (1)

/*
 * <ini>
 * gEnableApRandomBssid - Create ramdom BSSID
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to create a random BSSID in SoftAP mode to meet
 * the Android requirement.
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_ENABLE_RANDOM_BSSID_NAME            "gEnableApRandomBssid"
#define CFG_AP_ENABLE_RANDOM_BSSID_MIN             (0)
#define CFG_AP_ENABLE_RANDOM_BSSID_MAX             (1)
#define CFG_AP_ENABLE_RANDOM_BSSID_DEFAULT         (0)

/*
 * <ini>
 * gEnableApProt - Enable/Disable AP protection
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable AP protection
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_ENABLE_PROTECTION_MODE_NAME            "gEnableApProt"
#define CFG_AP_ENABLE_PROTECTION_MODE_MIN             (0)
#define CFG_AP_ENABLE_PROTECTION_MODE_MAX             (1)
#define CFG_AP_ENABLE_PROTECTION_MODE_DEFAULT         (1)

/*
 * <ini>
 * gApProtection - Set AP protection parameter
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0xBFFF
 *
 * This ini is used to set AP protection parameter
 * Bit map for CFG_AP_PROTECTION_MODE_DEFAULT
 * LOWER byte for associated stations
 * UPPER byte for overlapping stations
 * each byte will have the following info
 * bit15 bit14 bit13     bit12  bit11 bit10    bit9     bit8
 * OBSS  RIFS  LSIG_TXOP NON_GF HT20  FROM_11G FROM_11B FROM_11A
 * bit7  bit6  bit5      bit4   bit3  bit2     bit1     bit0
 * OBSS  RIFS  LSIG_TXOP NON_GF HT_20 FROM_11G FROM_11B FROM_11A
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_PROTECTION_MODE_NAME            "gApProtection"
#define CFG_AP_PROTECTION_MODE_MIN             (0x0)
#define CFG_AP_PROTECTION_MODE_MAX             (0xFFFF)
#define CFG_AP_PROTECTION_MODE_DEFAULT         (0xBFFF)

/*
 * <ini>
 * gEnableApOBSSProt - Enable/Disable AP OBSS protection
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable AP OBSS protection
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_OBSS_PROTECTION_MODE_NAME       "gEnableApOBSSProt"
#define CFG_AP_OBSS_PROTECTION_MODE_MIN        (0)
#define CFG_AP_OBSS_PROTECTION_MODE_MAX        (1)
#define CFG_AP_OBSS_PROTECTION_MODE_DEFAULT    (0)

/*
 * <ini>
 * gDisableIntraBssFwd - Disable intrs BSS Rx packets
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to disbale to forward Intra-BSS Rx packets when
 * ap_isolate=1 in hostapd.conf
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_STA_SECURITY_SEPERATION_NAME    "gDisableIntraBssFwd"
#define CFG_AP_STA_SECURITY_SEPERATION_MIN     (0)
#define CFG_AP_STA_SECURITY_SEPERATION_MAX     (1)
#define CFG_AP_STA_SECURITY_SEPERATION_DEFAULT (0)

/*
 * <ini>
 * gApKeepAlivePeriod - AP keep alive period
 * @Min: 1
 * @Max: 65535
 * @Default: 20
 *
 * This ini is used to set keep alive period of AP
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_KEEP_ALIVE_PERIOD_NAME          "gApKeepAlivePeriod"
#define CFG_AP_KEEP_ALIVE_PERIOD_MIN           WNI_CFG_AP_KEEP_ALIVE_TIMEOUT_STAMIN
#define CFG_AP_KEEP_ALIVE_PERIOD_MAX           WNI_CFG_AP_KEEP_ALIVE_TIMEOUT_STAMAX
#define CFG_AP_KEEP_ALIVE_PERIOD_DEFAULT       WNI_CFG_AP_KEEP_ALIVE_TIMEOUT_STADEF

/*
 * <ini>
 * gApLinkMonitorPeriod - AP keep alive period
 * @Min: 3
 * @Max: 50
 * @Default: 10
 *
 * This ini is used to configure AP link monitor timeout value
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_LINK_MONITOR_PERIOD_NAME          "gApLinkMonitorPeriod"
#define CFG_AP_LINK_MONITOR_PERIOD_MIN           (3)
#define CFG_AP_LINK_MONITOR_PERIOD_MAX           (50)
#define CFG_AP_LINK_MONITOR_PERIOD_DEFAULT       (10)

/*
 * <ini>
 * gBeaconInterval - Beacon interval for SoftAP
 * @Min: 0
 * @Max: 65535
 * @Default: 100
 *
 * This ini is used to set beacon interval for SoftAP
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BEACON_INTERVAL_NAME               "gBeaconInterval"
#define CFG_BEACON_INTERVAL_MIN                WNI_CFG_BEACON_INTERVAL_STAMIN
#define CFG_BEACON_INTERVAL_MAX                WNI_CFG_BEACON_INTERVAL_STAMAX
#define CFG_BEACON_INTERVAL_DEFAULT            WNI_CFG_BEACON_INTERVAL_STADEF

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
#define CFG_WLAN_AUTO_SHUTDOWN              "gWlanAutoShutdown"
#define CFG_WLAN_AUTO_SHUTDOWN_MIN          (0)
#define CFG_WLAN_AUTO_SHUTDOWN_MAX          (86400)   /* Max 1 day timeout */
#define CFG_WLAN_AUTO_SHUTDOWN_DEFAULT      (0)
#endif

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/*
 * <ini>
 * gWlanMccToSccSwitchMode - Control SAP channel.
 * @Min: 0
 * @Max: 5
 * @Default: 0
 *
 * This ini is used to override SAP channel.
 * If gWlanMccToSccSwitchMode = 0: disabled.
 * If gWlanMccToSccSwitchMode = 1: Enable switch.
 * If gWlainMccToSccSwitchMode = 2: Force switch with SAP restart.
 * If gWlainMccToSccSwitchMode = 3: Force switch without SAP restart.
 * If gWlainMccToSccSwitchMode = 4: Switch using
 *		fav channel(s)without SAP restart.
 * If gWlainMccToSccSwitchMode = 5: Force switch without SAP restart.MCC allowed
 *					in exceptional cases.
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_WLAN_MCC_TO_SCC_SWITCH_MODE          "gWlanMccToSccSwitchMode"
#define CFG_WLAN_MCC_TO_SCC_SWITCH_MODE_MIN      (QDF_MCC_TO_SCC_SWITCH_DISABLE)
#define CFG_WLAN_MCC_TO_SCC_SWITCH_MODE_MAX \
		   (QDF_MCC_TO_SCC_SWITCH_MAX - 1)
#define CFG_WLAN_MCC_TO_SCC_SWITCH_MODE_DEFAULT  (QDF_MCC_TO_SCC_SWITCH_DISABLE)
#endif

#define CFG_DISABLE_PACKET_FILTER		"gDisablePacketFilter"
#define CFG_DISABLE_PACKET_FILTER_MIN		(0)
#define CFG_DISABLE_PACKET_FILTER_MAX		(1)
#define CFG_DISABLE_PACKET_FILTER_DEFAULT	(1)

#define CFG_ENABLE_LTE_COEX              "gEnableLTECoex"
#define CFG_ENABLE_LTE_COEX_MIN               (0)
#define CFG_ENABLE_LTE_COEX_MAX               (1)
#define CFG_ENABLE_LTE_COEX_DEFAULT           (0)

#define CFG_GO_KEEP_ALIVE_PERIOD_NAME          "gGoKeepAlivePeriod"
#define CFG_GO_KEEP_ALIVE_PERIOD_MIN           WNI_CFG_GO_KEEP_ALIVE_TIMEOUT_STAMIN
#define CFG_GO_KEEP_ALIVE_PERIOD_MAX           WNI_CFG_GO_KEEP_ALIVE_TIMEOUT_STAMAX
#define CFG_GO_KEEP_ALIVE_PERIOD_DEFAULT       WNI_CFG_GO_KEEP_ALIVE_TIMEOUT_STADEF

/* gGoLinkMonitorPeriod is period where link is idle and where
 * we send NULL frame
 */
#define CFG_GO_LINK_MONITOR_PERIOD_NAME          "gGoLinkMonitorPeriod"
#define CFG_GO_LINK_MONITOR_PERIOD_MIN           (3)
#define CFG_GO_LINK_MONITOR_PERIOD_MAX           (50)
#define CFG_GO_LINK_MONITOR_PERIOD_DEFAULT       (10)

#define CFG_VCC_RSSI_TRIGGER_NAME             "gVccRssiTrigger"
#define CFG_VCC_RSSI_TRIGGER_MIN              (0)
#define CFG_VCC_RSSI_TRIGGER_MAX              (80)
#define CFG_VCC_RSSI_TRIGGER_DEFAULT          (80)

#define CFG_VCC_UL_MAC_LOSS_THRESH_NAME       "gVccUlMacLossThresh"
#define CFG_VCC_UL_MAC_LOSS_THRESH_MIN        (0)
#define CFG_VCC_UL_MAC_LOSS_THRESH_MAX        (9)
#define CFG_VCC_UL_MAC_LOSS_THRESH_DEFAULT    (9)

/*
 * <ini>
 * gPassiveMaxChannelTimeConc - Maximum passive scan time in milliseconds.
 * @Min: 0
 * @Max: 10000
 * @Default: 110
 *
 * This ini is used to set maximum passive scan time in STA+SAP concurrent
 * mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_NAME      "gPassiveMaxChannelTimeConc"
#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MIN       (0)
#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_MAX       (10000)
#define CFG_PASSIVE_MAX_CHANNEL_TIME_CONC_DEFAULT   (110)

/*
 * <ini>
 * gPassiveMinChannelTimeConc - Minimum passive scan time in milliseconds.
 * @Min: 0
 * @Max: 10000
 * @Default: 60
 *
 * This ini is used to set minimum passive scan time in STA+SAP concurrent
 * mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_NAME      "gPassiveMinChannelTimeConc"
#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MIN       (0)
#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_MAX       (10000)
#define CFG_PASSIVE_MIN_CHANNEL_TIME_CONC_DEFAULT   (60)

/*
 * <ini>
 * gActiveMaxChannelTimeConc - Maximum active scan time in milliseconds.
 * @Min: 0
 * @Max: 10000
 * @Default: 40
 *
 * This ini is used to set maximum active scan time in STA+SAP concurrent
 * mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_NAME       "gActiveMaxChannelTimeConc"
#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MIN        (0)
#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_MAX        (10000)
#define CFG_ACTIVE_MAX_CHANNEL_TIME_CONC_DEFAULT    (40)

/*
 * <ini>
 * gActiveMinChannelTimeConc - Minimum active scan time in milliseconds..
 * @Min: 0
 * @Max: 10000
 * @Default: 20
 *
 * This ini is used to set minimum active scan time in STA+SAP concurrent
 * mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_NAME       "gActiveMinChannelTimeConc"
#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MIN        (0)
#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_MAX        (10000)
#define CFG_ACTIVE_MIN_CHANNEL_TIME_CONC_DEFAULT    (20)

/*
 * <ini>
 * gRestTimeConc - Rest time before moving to a new channel to scan.
 * @Min: 0
 * @Max: 10000
 * @Default: 100
 *
 * This ini is used to configure rest time.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_REST_TIME_CONC_NAME                     "gRestTimeConc"
#define CFG_REST_TIME_CONC_MIN                      (0)
#define CFG_REST_TIME_CONC_MAX                      (10000)
#define CFG_REST_TIME_CONC_DEFAULT                  (100)

/*
 * <ini>
 * gMinRestTimeConc - Mininum time spent on home channel before moving to a
 * new channel to scan.
 * @Min: 0
 * @Max: 50
 * @Default: 50
 *
 * This ini is used to configure minimum time spent on home channel before
 * moving to a new channel to scan.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MIN_REST_TIME_NAME                      "gMinRestTimeConc"
#define CFG_MIN_REST_TIME_MIN                       (0)
#define CFG_MIN_REST_TIME_MAX                       (50)
#define CFG_MIN_REST_TIME_DEFAULT                   (50)

/*
 * <ini>
 * gIdleTimeConc - Data inactivity time in msec.
 * @Min: 0
 * @Max: 25
 * @Default: 25
 *
 * This ini is used to configure data inactivity time in msec on bss channel
 * that will be used by scan engine in firmware.
 * For example if this value is 25ms then firmware will check for data
 * inactivity every 25ms till gRestTimeConc is reached.
 * If inactive then scan engine will move from home channel to scan the next
 * frequency.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IDLE_TIME_NAME                          "gIdleTimeConc"
#define CFG_IDLE_TIME_MIN                           (0)
#define CFG_IDLE_TIME_MAX                           (25)
#define CFG_IDLE_TIME_DEFAULT                       (25)

#define CFG_MAX_PS_POLL_NAME                   "gMaxPsPoll"
#define CFG_MAX_PS_POLL_MIN                    WNI_CFG_MAX_PS_POLL_STAMIN
#define CFG_MAX_PS_POLL_MAX                    WNI_CFG_MAX_PS_POLL_STAMAX
#define CFG_MAX_PS_POLL_DEFAULT                WNI_CFG_MAX_PS_POLL_STADEF

#define CFG_MAX_TX_POWER_NAME                   "gTxPowerCap"
#define CFG_MAX_TX_POWER_MIN                    WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMIN
#define CFG_MAX_TX_POWER_MAX                    WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMAX
/* Not to use CFG default because if no registry setting, this is ignored by SME. */
#define CFG_MAX_TX_POWER_DEFAULT                WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMAX

/* This ini controls driver to honor/dishonor power constraint from AP */
#define CFG_TX_POWER_CTRL_NAME                 "gAllowTPCfromAP"
#define CFG_TX_POWER_CTRL_DEFAULT              (1)
#define CFG_TX_POWER_CTRL_MIN                  (0)
#define CFG_TX_POWER_CTRL_MAX                  (1)

/*
 * <ini>
 * gMaxLIModulatedDTIM - Set MaxLIModulate Dtim
 * @Min: 1
 * @Max: 10
 * @Default: 10
 *
 * This ini is used to set default MaxLIModulatedDTIM
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_MAX_LI_MODULATED_DTIM_NAME         "gMaxLIModulatedDTIM"
#define CFG_MAX_LI_MODULATED_DTIM_MIN          (1)
#define CFG_MAX_LI_MODULATED_DTIM_MAX          (10)
#define CFG_MAX_LI_MODULATED_DTIM_DEFAULT      (10)

/*
 * <ini>
 * gFWMccRtsCtsProtection - RTS-CTS protection in MCC.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable use of long duration RTS-CTS protection
 * when SAP goes off channel in MCC mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_FW_MCC_RTS_CTS_PROT_NAME           "gFWMccRtsCtsProtection"
#define CFG_FW_MCC_RTS_CTS_PROT_MIN            (0)
#define CFG_FW_MCC_RTS_CTS_PROT_MAX            (1)
#define CFG_FW_MCC_RTS_CTS_PROT_DEFAULT        (0)

/*
 * <ini>
 * gFWMccBCastProbeResponse - Broadcast Probe Response in MCC.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable use of broadcast probe response to
 * increase the detectability of SAP in MCC mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_FW_MCC_BCAST_PROB_RESP_NAME        "gFWMccBCastProbeResponse"
#define CFG_FW_MCC_BCAST_PROB_RESP_MIN         (0)
#define CFG_FW_MCC_BCAST_PROB_RESP_MAX         (1)
#define CFG_FW_MCC_BCAST_PROB_RESP_DEFAULT     (0)

/*
 * <ini>
 * gDataInactivityTimeout - Data activity timeout for non wow mode.
 * @Min: 1
 * @Max: 255
 * @Default: 200
 *
 * This ini is used to set data inactivity timeout in non wow mode.
 *
 * Supported Feature: inactivity timeout in non wow mode
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_DATA_INACTIVITY_TIMEOUT_NAME       "gDataInactivityTimeout"
#define CFG_DATA_INACTIVITY_TIMEOUT_MIN        (1)
#define CFG_DATA_INACTIVITY_TIMEOUT_MAX        (255)
#define CFG_DATA_INACTIVITY_TIMEOUT_DEFAULT    (200)

/*
 * <ini>
 * g_wow_data_inactivity_timeout - Data activity timeout in wow mode.
 * @Min: 1
 * @Max: 255
 * @Default: 50
 *
 * This ini is used to set data inactivity timeout in wow mode.
 *
 * Supported Feature: inactivity timeout in wow mode
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_WOW_DATA_INACTIVITY_TIMEOUT_NAME     "g_wow_data_inactivity_timeout"
#define CFG_WOW_DATA_INACTIVITY_TIMEOUT_MIN      (1)
#define CFG_WOW_DATA_INACTIVITY_TIMEOUT_MAX      (255)
#define CFG_WOW_DATA_INACTIVITY_TIMEOUT_DEFAULT  (50)

/*
 * <ini>
 * gStaKeepAlivePeriod - Sends NULL frame to AP periodically in
 * seconds to notify STA's existence
 * @Min: 0
 * @Max: 65535
 * @Default: 30
 *
 * This ini is used to send default NULL frame to AP
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_NAME          "gStaKeepAlivePeriod"
#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MIN           (0)
#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MAX           (65535)
#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD_DEFAULT       (30)

/**
 * enum station_keepalive_method - available keepalive methods for stations
 * @HDD_STA_KEEPALIVE_NULL_DATA: null data packet
 * @HDD_STA_KEEPALIVE_GRAT_ARP: gratuitous ARP packet
 * @HDD_STA_KEEPALIVE_COUNT: number of method options available
 */
enum station_keepalive_method {
	HDD_STA_KEEPALIVE_NULL_DATA,
	HDD_STA_KEEPALIVE_GRAT_ARP,
	/* keep at the end */
	HDD_STA_KEEPALIVE_COUNT
};

/*
 * <ini>
 * gStaKeepAliveMethod - Which keepalive method to use
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini determines which keepalive method to use for station interfaces
 *       1) Use null data packets
 *       2) Use gratuitous ARP packets
 *
 * Related: gStaKeepAlivePeriod, gApKeepAlivePeriod, gGoKeepAlivePeriod
 *
 * Supported Feature: STA, Keepalive
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_STA_KEEPALIVE_METHOD_NAME              "gStaKeepAliveMethod"
#define CFG_STA_KEEPALIVE_METHOD_MIN               (HDD_STA_KEEPALIVE_NULL_DATA)
#define CFG_STA_KEEPALIVE_METHOD_MAX               (HDD_STA_KEEPALIVE_COUNT - 1)
#define CFG_STA_KEEPALIVE_METHOD_DEFAULT           (HDD_STA_KEEPALIVE_GRAT_ARP)

/* WMM configuration */
/*
 * <ini>
 * WmmIsEnabled - Enable WMM feature
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to enable/disable WMM.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MODE_NAME                          "WmmIsEnabled"
#define CFG_QOS_WMM_MODE_MIN                           (0)
#define CFG_QOS_WMM_MODE_MAX                           (2) /* HDD_WMM_NO_QOS */
#define CFG_QOS_WMM_MODE_DEFAULT                       (0) /* HDD_WMM_AUTO */

/*
 * <ini>
 * 80211eIsEnabled - Enable 802.11e feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable 802.11e.
 *
 * Related: None.
 *
 * Supported Feature: 802.11e
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_80211E_ENABLED_NAME                   "80211eIsEnabled"
#define CFG_QOS_WMM_80211E_ENABLED_MIN                     (0)
#define CFG_QOS_WMM_80211E_ENABLED_MAX                     (1)
#define CFG_QOS_WMM_80211E_ENABLED_DEFAULT                 (0)

/*
 * <ini>
 * UapsdMask - To setup U-APSD mask for ACs
 * @Min: 0x00
 * @Max: 0xFF
 * @Default: 0x00
 *
 * This ini is used to setup U-APSD mask for ACs.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_MASK_NAME                        "UapsdMask"
#define CFG_QOS_WMM_UAPSD_MASK_MIN                         (0x00)
#define CFG_QOS_WMM_UAPSD_MASK_MAX                         (0xFF)
#define CFG_QOS_WMM_UAPSD_MASK_DEFAULT                     (0x00)

/*
 * <ini>
 * ImplicitQosIsEnabled - Enableimplicit QOS
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable implicit QOS.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_IMPLICIT_SETUP_ENABLED_NAME             "ImplicitQosIsEnabled"
#define CFG_QOS_IMPLICIT_SETUP_ENABLED_MIN              (0)
#define CFG_QOS_IMPLICIT_SETUP_ENABLED_MAX              (1)
#define CFG_QOS_IMPLICIT_SETUP_ENABLED_DEFAULT          (0)

/*
 * <ini>
 * InfraUapsdVoSrvIntv - Set Uapsd service interval for voice
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 20
 *
 * This ini is used to set Uapsd service interval for voice.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_NAME         "InfraUapsdVoSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_DEFAULT       (20)

/*
 * <ini>
 * InfraUapsdVoSuspIntv - Set Uapsd suspension interval for voice
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval for voice.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_NAME         "InfraUapsdVoSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_DEFAULT       (2000)

/*
 * <ini>
 * InfraUapsdViSrvIntv - Set Uapsd service interval for video
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 300
 *
 * This ini is used to set Uapsd service interval for video.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_NAME         "InfraUapsdViSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_DEFAULT       (300)

/*
 * <ini>
 * InfraUapsdViSuspIntv - Set Uapsd suspension interval for video
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval for video
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_NAME         "InfraUapsdViSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_DEFAULT       (2000)

/*
 * <ini>
 * InfraUapsdBeSrvIntv - Set Uapsd service interval for BE
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 300
 *
 * This ini is used to set Uapsd service interval for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_NAME         "InfraUapsdBeSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_DEFAULT       (300)

/*
 * <ini>
 * InfraUapsdBeSuspIntv - Set Uapsd suspension interval for BE
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_NAME         "InfraUapsdBeSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_DEFAULT       (2000)

/*
 * <ini>
 * InfraUapsdBkSrvIntv - Set Uapsd service interval for BK
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 300
 *
 * This ini is used to set Uapsd service interval for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_NAME         "InfraUapsdBkSrvIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_DEFAULT       (300)

/*
 * <ini>
 * InfraUapsdBkSuspIntv - Set Uapsd suspension interval for BK
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_NAME         "InfraUapsdBkSuspIntv"
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MIN           (0)
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MAX           (4294967295UL)
#define CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_DEFAULT       (2000)

/* default TSPEC parameters for AC_VO */
/*
 * <ini>
 * InfraDirAcVo - Set TSPEC direction for VO
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_NAME   "InfraDirAcVo"
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_MIN     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_MAX     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_VO_DEFAULT (3) /*WLAN_QCT_CUST_WMM_TSDIR_BOTH*/

/*
 * <ini>
 * InfraNomMsduSizeAcVo - Set normal MSDU size for VO
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x80D0
 *
 * This ini is used to set normal MSDU size for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_NAME      "InfraNomMsduSizeAcVo"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MIN        (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MAX        (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_DEFAULT    (0x80D0)

/*
 * <ini>
 * InfraMeanDataRateAcVo - Set mean data rate for VO
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x14500
 *
 * This ini is used to set mean data rate for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_NAME    "InfraMeanDataRateAcVo"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MIN      (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MAX      (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_DEFAULT  (0x14500)

/*
 * <ini>
 * InfraMinPhyRateAcVo - Set min PHY rate for VO
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_NAME       "InfraMinPhyRateAcVo"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MIN         (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MAX         (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_DEFAULT     (0x5B8D80)
/*
 * <ini>
 * InfraSbaAcVo - Set surplus bandwidth allowance for VO
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_QOS_WMM_INFRA_SBA_AC_VO_NAME                   "InfraSbaAcVo"
 #define CFG_QOS_WMM_INFRA_SBA_AC_VO_MIN                     (0x2001)
 #define CFG_QOS_WMM_INFRA_SBA_AC_VO_MAX                     (0xFFFF)
 #define CFG_QOS_WMM_INFRA_SBA_AC_VO_DEFAULT                 (0x2001)

 /* default TSPEC parameters for AC_VI */
/*
 * <ini>
 * InfraDirAcVi - Set TSPEC direction for VI
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_NAME   "InfraDirAcVi"
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_MIN     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_MAX     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_VI_DEFAULT (3) /*WLAN_QCT_CUST_WMM_TSDIR_BOTH*/

/*
 * <ini>
 * InfraNomMsduSizeAcVi - Set normal MSDU size for VI
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x85DC
 *
 * This ini is used to set normal MSDU size for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_NAME      "InfraNomMsduSizeAcVi"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MIN        (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MAX        (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_DEFAULT    (0x85DC)

/*
 * <ini>
 * InfraMeanDataRateAcVi - Set mean data rate for VI
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x57E40
 *
 * This ini is used to set mean data rate for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_NAME    "InfraMeanDataRateAcVi"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MIN      (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MAX      (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_DEFAULT  (0x57E40)

/*
 * <ini>
 * iInfraMinPhyRateAcVi - Set min PHY rate for VI
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_NAME        "InfraMinPhyRateAcVi"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MIN          (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MAX          (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_DEFAULT      (0x5B8D80)

/*
 * <ini>
 * InfraSbaAcVi - Set surplus bandwidth allowance for VI
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_QOS_WMM_INFRA_SBA_AC_VI_NAME                   "InfraSbaAcVi"
 #define CFG_QOS_WMM_INFRA_SBA_AC_VI_MIN                     (0x2001)
 #define CFG_QOS_WMM_INFRA_SBA_AC_VI_MAX                     (0xFFFF)
 #define CFG_QOS_WMM_INFRA_SBA_AC_VI_DEFAULT                 (0x2001)

 /* default TSPEC parameters for AC_BE*/
/*
 * <ini>
 * InfraDirAcBe - Set TSPEC direction for BE
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_NAME   "InfraDirAcBe"
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_MIN     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_MAX     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_BE_DEFAULT (3) /*WLAN_QCT_CUST_WMM_TSDIR_BOTH*/

/*
 * <ini>
 * InfraNomMsduSizeAcBe - Set normal MSDU size for BE
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x85DC
 *
 * This ini is used to set normal MSDU size for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_NAME      "InfraNomMsduSizeAcBe"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MIN        (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MAX        (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_DEFAULT    (0x85DC)

/*
 * <ini>
 * InfraMeanDataRateAcBe - Set mean data rate for BE
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x493E0
 *
 * This ini is used to set mean data rate for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_NAME    "InfraMeanDataRateAcBe"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MIN      (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MAX      (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_DEFAULT  (0x493E0)

/*
 * <ini>
 * InfraMinPhyRateAcBe - Set min PHY rate for BE
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_NAME        "InfraMinPhyRateAcBe"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MIN          (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MAX          (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_DEFAULT      (0x5B8D80)

/*
 * <ini>
 * InfraSbaAcBe - Set surplus bandwidth allowance for BE
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_QOS_WMM_INFRA_SBA_AC_BE_NAME                   "InfraSbaAcBe"
 #define CFG_QOS_WMM_INFRA_SBA_AC_BE_MIN                     (0x2001)
 #define CFG_QOS_WMM_INFRA_SBA_AC_BE_MAX                     (0xFFFF)
 #define CFG_QOS_WMM_INFRA_SBA_AC_BE_DEFAULT                 (0x2001)

 /* default TSPEC parameters for AC_Bk*/
/*
 * <ini>
 * InfraDirAcBk - Set TSPEC direction for BK
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_NAME   "InfraDirAcBk"
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_MIN     (0)
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_MAX     (3)
#define CFG_QOS_WMM_INFRA_DIR_AC_BK_DEFAULT (3) /*WLAN_QCT_CUST_WMM_TSDIR_BOTH*/

/*
 * <ini>
 * InfraNomMsduSizeAcBk - Set normal MSDU size for BK
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x85DC
 *
 * This ini is used to set normal MSDU size for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_NAME      "InfraNomMsduSizeAcBk"
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MIN        (0x0)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MAX        (0xFFFF)
#define CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_DEFAULT    (0x85DC)

/*
 * <ini>
 * InfraMeanDataRateAcBk - Set mean data rate for BK
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x493E0
 *
 * This ini is used to set mean data rate for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_NAME    "InfraMeanDataRateAcBk"
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MIN      (0x0)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MAX      (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_DEFAULT  (0x493E0)

/*
 * <ini>
 * InfraMinPhyRateAcBke - Set min PHY rate for BK
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_NAME        "InfraMinPhyRateAcBk"
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MIN          (0x0)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MAX          (0xFFFFFFFF)
#define CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_DEFAULT      (0x5B8D80)

/*
 * <ini>
 * InfraSbaAcBk - Set surplus bandwidth allowance for BK
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_QOS_WMM_INFRA_SBA_AC_BK_NAME                   "InfraSbaAcBk"
 #define CFG_QOS_WMM_INFRA_SBA_AC_BK_MIN                     (0x2001)
 #define CFG_QOS_WMM_INFRA_SBA_AC_BK_MAX                     (0xFFFF)
 #define CFG_QOS_WMM_INFRA_SBA_AC_BK_DEFAULT                 (0x2001)

/*
 * <ini>
 * burstSizeDefinition - Set TS burst size
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set TS burst size
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_BURST_SIZE_DEFN_NAME                "burstSizeDefinition"
#define CFG_QOS_WMM_BURST_SIZE_DEFN_MIN                  (0)
#define CFG_QOS_WMM_BURST_SIZE_DEFN_MAX                  (1)
#define CFG_QOS_WMM_BURST_SIZE_DEFN_DEFAULT              (0)

/*
 * <ini>
 * tsInfoAckPolicy - Set TS ack policy
 * @Min: 0x00
 * @Max: 0x01
 * @Default: 0x00
 *
 * This ini is used to set TS ack policy
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_NAME              "tsInfoAckPolicy"
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_MIN                (0x00)
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_MAX                (0x01)
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY_DEFAULT            (0x00)

/*
 * <ini>
 * SingleTIDRC - Set replay counter for all TID's
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set replay counter for all TID's
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SINGLE_TID_RC_NAME    "SingleTIDRC"
#define CFG_SINGLE_TID_RC_MIN      (0) /* Separate replay counter for all TID */
#define CFG_SINGLE_TID_RC_MAX      (1) /* Single replay counter for all TID */
#define CFG_SINGLE_TID_RC_DEFAULT  (1)

/*
 * <ini>
 * gAddTSWhenACMIsOff - Set ACM value for AC
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set ACM value for AC
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_NAME               "gAddTSWhenACMIsOff"
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MIN                (0)
/* Send AddTs even when ACM is not set for the AC */
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MAX                (1)
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_DEFAULT            (0)

#ifdef FEATURE_WLAN_ESE
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_NAME    "InfraInactivityInterval"
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MIN      (0)
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MAX      (4294967295UL)
#define CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_DEFAULT  (0) /* disabled */

#define CFG_ESE_FEATURE_ENABLED_NAME                       "EseEnabled"
#define CFG_ESE_FEATURE_ENABLED_MIN                         (0)
#define CFG_ESE_FEATURE_ENABLED_MAX                         (1)
#define CFG_ESE_FEATURE_ENABLED_DEFAULT                     (0) /* disabled */
#endif /* FEATURE_WLAN_ESE */

/*
 * <ini>
 * MAWCEnabled - Enable/Disable Motion Aided Wireless Connectivity Global
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 0
 *
 * This ini is used to controls the MAWC feature globally.
 * MAWC is Motion Aided Wireless Connectivity.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: Roaming and PNO/NLO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_MAWC_FEATURE_ENABLED_NAME                   "MAWCEnabled"
#define CFG_LFR_MAWC_FEATURE_ENABLED_MIN                    (0)
#define CFG_LFR_MAWC_FEATURE_ENABLED_MAX                    (1)
#define CFG_LFR_MAWC_FEATURE_ENABLED_DEFAULT                (0)

/*
 * <ini>
 * mawc_roam_enabled - Enable/Disable MAWC during roaming
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 0
 *
 * This ini is used to control MAWC during roaming.
 *
 * Related: MAWCEnabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_ROAM_ENABLED_NAME            "mawc_roam_enabled"
#define CFG_MAWC_ROAM_ENABLED_MIN             (0)
#define CFG_MAWC_ROAM_ENABLED_MAX             (1)
#define CFG_MAWC_ROAM_ENABLED_DEFAULT         (0)

/*
 * <ini>
 * mawc_roam_traffic_threshold - Configure traffic threshold
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 300
 *
 * This ini is used to configure the data traffic load in kBps to
 * register CMC.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_ROAM_TRAFFIC_THRESHOLD_NAME       "mawc_roam_traffic_threshold"
#define CFG_MAWC_ROAM_TRAFFIC_THRESHOLD_MIN        (0)
#define CFG_MAWC_ROAM_TRAFFIC_THRESHOLD_MAX        (0xFFFFFFFF)
#define CFG_MAWC_ROAM_TRAFFIC_THRESHOLD_DEFAULT    (300)

/*
 * <ini>
 * mawc_roam_ap_rssi_threshold - Best AP RSSI threshold
 * @Min: -120
 * @Max: 0
 * @Default: -66
 *
 * This ini is used to specify the RSSI threshold to scan for the AP.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_ROAM_AP_RSSI_THRESHOLD_NAME       "mawc_roam_ap_rssi_threshold"
#define CFG_MAWC_ROAM_AP_RSSI_THRESHOLD_MIN        (-120)
#define CFG_MAWC_ROAM_AP_RSSI_THRESHOLD_MAX        (0)
#define CFG_MAWC_ROAM_AP_RSSI_THRESHOLD_DEFAULT    (-66)

/*
 * <ini>
 * mawc_roam_rssi_high_adjust - Adjust MAWC roam high RSSI
 * @Min: 3
 * @Max: 5
 * @Default: 5
 *
 * This ini is used for high RSSI threshold adjustment in stationary state
 * to suppress the scan.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_ROAM_RSSI_HIGH_ADJUST_NAME        "mawc_roam_rssi_high_adjust"
#define CFG_MAWC_ROAM_RSSI_HIGH_ADJUST_MIN         (3)
#define CFG_MAWC_ROAM_RSSI_HIGH_ADJUST_MAX         (5)
#define CFG_MAWC_ROAM_RSSI_HIGH_ADJUST_DEFAULT     (5)

/*
 * <ini>
 * mawc_roam_rssi_high_adjust - Adjust MAWC roam low RSSI
 * @Min: 3
 * @Max: 5
 * @Default: 5
 *
 * This ini is used for low RSSI threshold adjustment in stationary state
 * to suppress the scan.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_ROAM_RSSI_LOW_ADJUST_NAME        "mawc_roam_rssi_low_adjust"
#define CFG_MAWC_ROAM_RSSI_LOW_ADJUST_MIN         (3)
#define CFG_MAWC_ROAM_RSSI_LOW_ADJUST_MAX         (5)
#define CFG_MAWC_ROAM_RSSI_LOW_ADJUST_DEFAULT     (5)

/*This parameter is used to set Wireless Extended Security Mode.*/
#define CFG_ENABLE_WES_MODE_NAME                            "gWESModeEnabled"
#define CFG_ENABLE_WES_MODE_NAME_MIN                        (0)
#define CFG_ENABLE_WES_MODE_NAME_MAX                        (1)
#define CFG_ENABLE_WES_MODE_NAME_DEFAULT                    (0)

#define CFG_TL_DELAYED_TRGR_FRM_INT_NAME                   "DelayedTriggerFrmInt"
#define CFG_TL_DELAYED_TRGR_FRM_INT_MIN                     1
#define CFG_TL_DELAYED_TRGR_FRM_INT_MAX                     (4294967295UL)
#define CFG_TL_DELAYED_TRGR_FRM_INT_DEFAULT                 3000

/*
 * <ini>
 * gRrmEnable - Enable/Disable RRM
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to controls the capabilities (11 k) included
 * in the capabilities field.
 *
 * Related: None.
 *
 * Supported Feature: 11k
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RRM_ENABLE_NAME                              "gRrmEnable"
#define CFG_RRM_ENABLE_MIN                               (0)
#define CFG_RRM_ENABLE_MAX                               (1)
#define CFG_RRM_ENABLE_DEFAULT                           (0)

/*
 * <ini>
 * gRrmRandnIntvl - Randomization interval
 * @Min: 10
 * @Max: 100
 * @Default: 100
 *
 * This ini is used to set randomization interval which is used to start a timer
 * of a random value within randomization interval. Next RRM Scan request
 * will be issued after the expiry of this random interval.
 *
 * Related: None.
 *
 * Supported Feature: 11k
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_NAME            "gRrmRandnIntvl"
#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_MIN             (10)
#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_MAX             (100)
#define CFG_RRM_MEAS_RANDOMIZATION_INTVL_DEFAULT         (100)

/*
 * <ini>
 * rm_capability - Configure RM enabled capabilities IE
 * @Default: 73,10,91,00,04
 *
 * This ini is used to configure RM enabled capabilities IE.
 * Using this INI, we can set/unset any of the bits in 5 bytes
 * (last 4bytes are reserved). Bit details are updated as per
 * Draft version of 11mc spec. (Draft P802.11REVmc_D4.2)
 *
 * Bitwise details are defined as bit mask in rrm_global.h
 * Comma is used as a separator for each byte.
 *
 * Related: None.
 *
 * Supported Feature: 11k
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RM_CAPABILITY_NAME            "rm_capability"
#define CFG_RM_CAPABILITY_DEFAULT         "73,10,91,00,04"

#define CFG_TELE_BCN_MAX_LI_NAME                     "telescopicBeaconMaxListenInterval"
#define CFG_TELE_BCN_MAX_LI_MIN                      (0)
#define CFG_TELE_BCN_MAX_LI_MAX                      (7)
#define CFG_TELE_BCN_MAX_LI_DEFAULT                  (5)

#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_NAME      "gNeighborLookupThreshold"
#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN       (10)
#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX       (120)
#define CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_DEFAULT   (78)

/*
 * <ini>
 * lookup_threshold_5g_offset - Lookup Threshold offset for 5G band
 * @Min: -120
 * @Max: +120
 * @Default: 0
 *
 * This ini is  used to set the 5G band lookup threshold for roaming.
 * It depends on another INI which is gNeighborLookupThreshold.
 * gNeighborLookupThreshold is a legacy INI item which will be used to
 * set the RSSI lookup threshold for both 2G and 5G bands. If the
 * user wants to setup a different threshold for a 5G band, then user
 * can use this offset value which will be summed up to the value of
 * gNeighborLookupThreshold and used for 5G
 * e.g: gNeighborLookupThreshold = -76dBm
 *      lookup_threshold_5g_offset = 6dBm
 *      Then the 5G band will be configured to -76+6 = -70dBm
 * A default value of Zero to lookup_threshold_5g_offset will keep the
 * thresholds same for both 2G and 5G bands
 *
 * Related: gNeighborLookupThreshold
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_5G_RSSI_THRESHOLD_OFFSET_NAME      "lookup_threshold_5g_offset"
#define CFG_5G_RSSI_THRESHOLD_OFFSET_MIN       (-120)
#define CFG_5G_RSSI_THRESHOLD_OFFSET_MAX       (120)
#define CFG_5G_RSSI_THRESHOLD_OFFSET_DEFAULT   (0)

#define CFG_DELAY_BEFORE_VDEV_STOP_NAME              "gDelayBeforeVdevStop"
#define CFG_DELAY_BEFORE_VDEV_STOP_MIN               (2)
#define CFG_DELAY_BEFORE_VDEV_STOP_MAX               (200)
#define CFG_DELAY_BEFORE_VDEV_STOP_DEFAULT           (20)

#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_NAME           "gMaxNeighborReqTries"
#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MIN            (1)
#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MAX            (4)
#define CFG_11R_NEIGHBOR_REQ_MAX_TRIES_DEFAULT        (3)

#define CFG_ROAM_BMISS_FIRST_BCNT_NAME                  "gRoamBmissFirstBcnt"
#define CFG_ROAM_BMISS_FIRST_BCNT_MIN                   (5)
#define CFG_ROAM_BMISS_FIRST_BCNT_MAX                   (100)
#define CFG_ROAM_BMISS_FIRST_BCNT_DEFAULT               (10)

#define CFG_ROAM_BMISS_FINAL_BCNT_NAME                  "gRoamBmissFinalBcnt"
#define CFG_ROAM_BMISS_FINAL_BCNT_MIN                   (5)
#define CFG_ROAM_BMISS_FINAL_BCNT_MAX                   (100)
#define CFG_ROAM_BMISS_FINAL_BCNT_DEFAULT               (20)

#define CFG_ROAM_BEACON_RSSI_WEIGHT_NAME                "gRoamBeaconRssiWeight"
#define CFG_ROAM_BEACON_RSSI_WEIGHT_MIN                 (0)
#define CFG_ROAM_BEACON_RSSI_WEIGHT_MAX                 (16)
#define CFG_ROAM_BEACON_RSSI_WEIGHT_DEFAULT             (14)

/*
 * <ini>
 * gTelescopicBeaconWakeupEn - Set teles copic beacon wakeup
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default teles copic beacon wakeup
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_TELE_BCN_WAKEUP_EN_NAME            "gTelescopicBeaconWakeupEn"
#define CFG_TELE_BCN_WAKEUP_EN_MIN             (0)
#define CFG_TELE_BCN_WAKEUP_EN_MAX             (1)
#define CFG_TELE_BCN_WAKEUP_EN_DEFAULT         (0)

#define CFG_AP_DATA_AVAIL_POLL_PERIOD_NAME      "gApDataAvailPollInterval"
#define CFG_AP_DATA_AVAIL_POLL_PERIOD_MIN       (WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD_STAMIN)
#define CFG_AP_DATA_AVAIL_POLL_PERIOD_MAX       (WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD_STAMAX)
#define CFG_AP_DATA_AVAIL_POLL_PERIOD_DEFAULT   (WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD_STADEF)

#define CFG_ENABLE_HOST_ARPOFFLOAD_NAME         "hostArpOffload"
#define CFG_ENABLE_HOST_ARPOFFLOAD_MIN          (0)
#define CFG_ENABLE_HOST_ARPOFFLOAD_MAX          (1)
#define CFG_ENABLE_HOST_ARPOFFLOAD_DEFAULT      (1)

#define CFG_ENABLE_HOST_SSDP_NAME              "ssdp"
#define CFG_ENABLE_HOST_SSDP_MIN               (0)
#define CFG_ENABLE_HOST_SSDP_MAX               (1)
#define CFG_ENABLE_HOST_SSDP_DEFAULT           (1)

#define CFG_ENABLE_HOST_NSOFFLOAD_NAME         "hostNSOffload"
#define CFG_ENABLE_HOST_NSOFFLOAD_MIN          (0)
#define CFG_ENABLE_HOST_NSOFFLOAD_MAX          (1)
#define CFG_ENABLE_HOST_NSOFFLOAD_DEFAULT      (1)

/*
 * <ini>
 * gHwFilterMode - configure hardware filter for DTIM mode
 * @Min: 0
 * @Max: 3
 * @Default: 1
 *
 * The hardware filter is only effective in DTIM mode. Use this configuration
 * to blanket drop broadcast/multicast packets at the hardware level, without
 * waking up the firmware
 *
 * Takes a bitmap of frame types to drop
 * @E.g.
 *	# disable feature
 *	gHwFilterMode=0
 *	# drop all broadcast frames, except ARP (default)
 *	gHwFilterMode=1
 *	# drop all multicast frames, except ICMPv6
 *	gHwFilterMode=2
 *	# drop all broadcast and multicast frames, except ARP and ICMPv6
 *	gHwFilterMode=3
 *
 * Related: N/A
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_HW_FILTER_MODE_BITMAP_NAME	"gHwFilterMode"
#define CFG_HW_FILTER_MODE_BITMAP_MIN		(0)
#define CFG_HW_FILTER_MODE_BITMAP_MAX		(3)
#define CFG_HW_FILTER_MODE_BITMAP_DEFAULT	(1)

/*
 * <ini>
 * BandCapability - Preferred band (0: Both,  1: 2.4G only,  2: 5G only)
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to set default band capability
 * (0: Both, 1: 2.4G only, 2: 5G only)
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_BAND_CAPABILITY_NAME          "BandCapability"
#define CFG_BAND_CAPABILITY_MIN           (0)
#define CFG_BAND_CAPABILITY_MAX           (2)
#define CFG_BAND_CAPABILITY_DEFAULT       (0)

#define CFG_ENABLE_BYPASS_11D_NAME                 "gEnableBypass11d"
#define CFG_ENABLE_BYPASS_11D_MIN                  (0)
#define CFG_ENABLE_BYPASS_11D_MAX                  (1)
#define CFG_ENABLE_BYPASS_11D_DEFAULT              (1)

/*
 * <ini>
 * gEnableDumpCollect - It will use for collect the dumps
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set collect default dump
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_RAMDUMP_COLLECTION              "gEnableDumpCollect"
#define CFG_ENABLE_RAMDUMP_COLLECTION_MIN          (0)
#define CFG_ENABLE_RAMDUMP_COLLECTION_MAX          (1)
#define CFG_ENABLE_RAMDUMP_COLLECTION_DEFAULT      (1)

enum hdd_link_speed_rpt_type {
	eHDD_LINK_SPEED_REPORT_ACTUAL = 0,
	eHDD_LINK_SPEED_REPORT_MAX = 1,
	eHDD_LINK_SPEED_REPORT_MAX_SCALED = 2,
};

/*
 * <ini>
 * gVhtChannelWidth - Channel width capability for 11ac
 * @Min: 0
 * @Max: 4
 * @Default: 3
 *
 * This ini is  used to set channel width capability for 11AC.
 * eHT_CHANNEL_WIDTH_20MHZ = 0,
 * eHT_CHANNEL_WIDTH_40MHZ = 1,
 * eHT_CHANNEL_WIDTH_80MHZ = 2,
 * eHT_CHANNEL_WIDTH_160MHZ = 3,
 * eHT_CHANNEL_WIDTH_80P80MHZ = 4,
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_CHANNEL_WIDTH                "gVhtChannelWidth"
#define CFG_VHT_CHANNEL_WIDTH_MIN            (0)
#define CFG_VHT_CHANNEL_WIDTH_MAX            (4)
#define CFG_VHT_CHANNEL_WIDTH_DEFAULT        (2)

/*
 * <ini>
 * gVhtRxMCS - VHT Rx MCS capability for 1x1 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Rx MCS capability for 1x1 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_RX_MCS_8_9               "gVhtRxMCS"
#define CFG_VHT_ENABLE_RX_MCS_8_9_MIN           (0)
#define CFG_VHT_ENABLE_RX_MCS_8_9_MAX           (2)
#define CFG_VHT_ENABLE_RX_MCS_8_9_DEFAULT       (0)

/*
 * <ini>
 * gVhtTxMCS - VHT Tx MCS capability for 1x1 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Tx MCS capability for 1x1 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_TX_MCS_8_9               "gVhtTxMCS"
#define CFG_VHT_ENABLE_TX_MCS_8_9_MIN           (0)
#define CFG_VHT_ENABLE_TX_MCS_8_9_MAX           (2)
#define CFG_VHT_ENABLE_TX_MCS_8_9_DEFAULT       (0)

/*
 * <ini>
 * gVhtRxMCS2x2 - VHT Rx MCS capability for 2x2 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Rx MCS capability for 2x2 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_RX_MCS2x2_8_9               "gVhtRxMCS2x2"
#define CFG_VHT_ENABLE_RX_MCS2x2_8_9_MIN           (0)
#define CFG_VHT_ENABLE_RX_MCS2x2_8_9_MAX           (2)
#define CFG_VHT_ENABLE_RX_MCS2x2_8_9_DEFAULT       (0)

/*
 * <ini>
 * gVhtTxMCS2x2 - VHT Tx MCS capability for 2x2 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Tx MCS capability for 2x2 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_TX_MCS2x2_8_9               "gVhtTxMCS2x2"
#define CFG_VHT_ENABLE_TX_MCS2x2_8_9_MIN           (0)
#define CFG_VHT_ENABLE_TX_MCS2x2_8_9_MAX           (2)
#define CFG_VHT_ENABLE_TX_MCS2x2_8_9_DEFAULT       (0)

/*
 * <ini>
 * enable_vht20_mcs9 - Enables VHT MCS9 in 20M BW operation
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_VHT20_MCS9               "enable_vht20_mcs9"
#define CFG_ENABLE_VHT20_MCS9_MIN           (0)
#define CFG_ENABLE_VHT20_MCS9_MAX           (1)
#define CFG_ENABLE_VHT20_MCS9_DEFAULT       (1)

/*
 * <ini>
 * gEnable2x2 - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini disables/enables 2x2 mode. If this is zero then DUT operates as 1x1
 *
 * 0, Disable
 * 1, Enable
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_2x2_CAP_FEATURE         "gEnable2x2"
#define CFG_VHT_ENABLE_2x2_CAP_FEATURE_MIN     (0)
#define CFG_VHT_ENABLE_2x2_CAP_FEATURE_MAX     (1)
#define CFG_VHT_ENABLE_2x2_CAP_FEATURE_DEFAULT (0)

/*
 * <ini>
 * enable_bt_chain_separation - Enables/disables bt /wlan chainmask assignment
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini disables/enables chainmask setting on 2x2, mainly used for ROME
 * BT/WLAN chainmask assignment.
 *
 * 0, Disable
 * 1, Enable
 *
 * Related: NA
 *
 * Supported Feature: 11n/11ac
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_BT_CHAIN_SEPARATION         "enableBTChainSeparation"
#define CFG_ENABLE_BT_CHAIN_SEPARATION_MIN     (0)
#define CFG_ENABLE_BT_CHAIN_SEPARATION_MAX     (1)
#define CFG_ENABLE_BT_CHAIN_SEPARATION_DEFAULT (0)

/*
 * <ini>
 * disable_high_ht_mcs_2x2 - disable high mcs index for 2nd stream in 2.4G
 * @Min: 0
 * @Max: 8
 * @Default: 0
 *
 * This ini is used to disable high HT MCS index for 2.4G STA connection.
 * It has been introduced to resolve IOT issue with one of the vendor.
 *
 * Note: This INI is not useful with 1x1 setting. If some platform supports
 * only 1x1 then this INI is not useful.
 *
 * 0 - It won't disable any HT MCS index (just like normal HT MCS)
 * 1 - It will disable 15th bit from HT RX MCS set (from 8-15 bits slot)
 * 2 - It will disable 14th & 15th bits from HT RX MCS set
 * 3 - It will disable 13th, 14th, & 15th bits from HT RX MCS set
 * and so on.
 *
 * Related: STA
 *
 * Supported Feature: 11n
 *
 * Usage: External
 */
#define CFG_DISABLE_HIGH_HT_RX_MCS_2x2         "disable_high_ht_mcs_2x2"
#define CFG_DISABLE_HIGH_HT_RX_MCS_2x2_MIN     (0)
#define CFG_DISABLE_HIGH_HT_RX_MCS_2x2_MAX     (8)
#define CFG_DISABLE_HIGH_HT_RX_MCS_2x2_DEFAULT (0)

/*
 * <ini>
 * gStaPrefer80MHzOver160MHz - set Sta perferance to connect in 80HZ/160HZ
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is  used to set Sta perferance to connect in 80HZ/160HZ
 *
 * 0 - Connects in 160MHz 1x1 when AP is 160MHz 2x2
 * 1 - Connects in 80MHz 2x2 when AP is 160MHz 2x2
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_STA_PREFER_80MHZ_OVER_160MHZ         "gStaPrefer80MHzOver160MHz"
#define CFG_STA_PREFER_80MHZ_OVER_160MHZ_MIN     (0)
#define CFG_STA_PREFER_80MHZ_OVER_160MHZ_MAX     (1)
#define CFG_STA_PREFER_80MHZ_OVER_160MHZ_DEFAULT (1)

/*
 * <ini>
 * gEnableMuBformee - Enables/disables multi-user (MU) beam formee capability
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini enables/disables multi-user (MU) beam formee
 * capability
 *
 * Change MU Bformee only when  gTxBFEnable is enabled.
 * When gTxBFEnable and gEnableMuBformee are set, MU beam formee capability is
 * enabled.
 * Related:  gTxBFEnable
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE         "gEnableMuBformee"
#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_MIN     (0)
#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_MAX     (1)
#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE_DEFAULT (0)

#define CFG_VHT_ENABLE_PAID_FEATURE             "gEnablePAID"
#define CFG_VHT_ENABLE_PAID_FEATURE_MIN         (0)
#define CFG_VHT_ENABLE_PAID_FEATURE_MAX         (1)
#define CFG_VHT_ENABLE_PAID_FEATURE_DEFAULT     (0)

#define CFG_VHT_ENABLE_GID_FEATURE              "gEnableGID"
#define CFG_VHT_ENABLE_GID_FEATURE_MIN          (0)
#define CFG_VHT_ENABLE_GID_FEATURE_MAX          (1)
#define CFG_VHT_ENABLE_GID_FEATURE_DEFAULT      (0)

/*
 * <ini>
 * gSetTxChainmask1x1 - Sets Transmit chain mask.
 * @Min: 1
 * @Max: 3
 * @Default: 0
 *
 * This ini Sets Transmit chain mask.
 *
 * If gEnable2x2 is disabled, gSetTxChainmask1x1 and gSetRxChainmask1x1 values
 * are taken into account. If chainmask value exceeds the maximum number of
 * chains supported by target, the max number of chains is used. By default,
 * chain0 is selected for both Tx and Rx.
 * gSetTxChainmask1x1=1 or gSetRxChainmask1x1=1 to select chain0.
 * gSetTxChainmask1x1=2 or gSetRxChainmask1x1=2 to select chain1.
 * gSetTxChainmask1x1=3 or gSetRxChainmask1x1=3 to select both chains.
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_1x1_TX_CHAINMASK         "gSetTxChainmask1x1"
#define CFG_VHT_ENABLE_1x1_TX_CHAINMASK_MIN     (0)
#define CFG_VHT_ENABLE_1x1_TX_CHAINMASK_MAX     (3)
#define CFG_VHT_ENABLE_1x1_TX_CHAINMASK_DEFAULT (0)

/*
 * <ini>
 * gSetRxChainmask1x1 - Sets Receive chain mask.
 * @Min: 1
 * @Max: 3
 * @Default: 0
 *
 * This ini is  used to set Receive chain mask.
 *
 * If gEnable2x2 is disabled, gSetTxChainmask1x1 and gSetRxChainmask1x1 values
 * are taken into account. If chainmask value exceeds the maximum number of
 * chains supported by target, the max number of chains is used. By default,
 * chain0 is selected for both Tx and Rx.
 * gSetTxChainmask1x1=1 or gSetRxChainmask1x1=1 to select chain0.
 * gSetTxChainmask1x1=2 or gSetRxChainmask1x1=2 to select chain1.
 * gSetTxChainmask1x1=3 or gSetRxChainmask1x1=3 to select both chains.
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_VHT_ENABLE_1x1_RX_CHAINMASK         "gSetRxChainmask1x1"
#define CFG_VHT_ENABLE_1x1_RX_CHAINMASK_MIN     (0)
#define CFG_VHT_ENABLE_1x1_RX_CHAINMASK_MAX     (3)
#define CFG_VHT_ENABLE_1x1_RX_CHAINMASK_DEFAULT (0)

/*
 * <ini>
 * gEnableAMPDUPS - Enable the AMPDUPS
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default AMPDUPS
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_AMPDUPS_FEATURE              "gEnableAMPDUPS"
#define CFG_ENABLE_AMPDUPS_FEATURE_MIN          (0)
#define CFG_ENABLE_AMPDUPS_FEATURE_MAX          (1)
#define CFG_ENABLE_AMPDUPS_FEATURE_DEFAULT      (0)

#define CFG_HT_ENABLE_SMPS_CAP_FEATURE          "gEnableHtSMPS"
#define CFG_HT_ENABLE_SMPS_CAP_FEATURE_MIN      (0)
#define CFG_HT_ENABLE_SMPS_CAP_FEATURE_MAX      (1)
#define CFG_HT_ENABLE_SMPS_CAP_FEATURE_DEFAULT  (0)

#define CFG_HT_SMPS_CAP_FEATURE                 "gHtSMPS"
#define CFG_HT_SMPS_CAP_FEATURE_MIN             (0)
#define CFG_HT_SMPS_CAP_FEATURE_MAX             (3)
#define CFG_HT_SMPS_CAP_FEATURE_DEFAULT         (3)

/*
 * <ini>
 * gDisableDFSChSwitch - Disable channel switch if radar is found
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to disable channel switch if radar is found
 * on that channel.
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DISABLE_DFS_CH_SWITCH                 "gDisableDFSChSwitch"
#define CFG_DISABLE_DFS_CH_SWITCH_MIN             (0)
#define CFG_DISABLE_DFS_CH_SWITCH_MAX             (1)
#define CFG_DISABLE_DFS_CH_SWITCH_DEFAULT         (0)

/*
 * <ini>
 * gEnableDFSMasterCap - Enable DFS master capability
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the DFS master capability.
 * Disabling it will cause driver to not advertise the spectrum
 * management capability
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_DFS_MASTER_CAPABILITY               "gEnableDFSMasterCap"
#define CFG_ENABLE_DFS_MASTER_CAPABILITY_MIN           (0)
#define CFG_ENABLE_DFS_MASTER_CAPABILITY_MAX           (1)
#define CFG_ENABLE_DFS_MASTER_CAPABILITY_DEFAULT       (0)

/*
 * <ini>
 * gSapPreferredChanLocation - Restrict channel switches between ondoor and
 * outdoor.
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used for restricting channel switches between Indoor and outdoor
 * channels after radar detection.
 * 0- No preferred channel location
 * 1- Use indoor channels only
 * 2- Use outdoor channels only
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SAP_PREFERRED_CHANNEL_LOCATION          "gSapPreferredChanLocation"
#define CFG_SAP_PREFERRED_CHANNEL_LOCATION_MIN      (0)
#define CFG_SAP_PREFERRED_CHANNEL_LOCATION_MAX      (2)
#define CFG_SAP_PREFERRED_CHANNEL_LOCATION_DEFAULT  (0)

/*
 * <ini>
 * gDisableDfsJapanW53 - Block W53 channels in random channel selection
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to block W53 Japan channel in random channel selection
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DISABLE_DFS_JAPAN_W53                      "gDisableDfsJapanW53"
#define CFG_DISABLE_DFS_JAPAN_W53_MIN                  (0)
#define CFG_DISABLE_DFS_JAPAN_W53_MAX                  (1)
#define CFG_DISABLE_DFS_JAPAN_W53_DEFAULT              (0)

/*
 * <ini>
 * gDisableDfsJapanW53 - Enable dfs phyerror filtering offload in FW
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to to enable dfs phyerror filtering offload to firmware
 * Enabling it will cause basic phy error to be discarding in firmware.
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_DFS_PHYERR_FILTEROFFLOAD_NAME       "dfsPhyerrFilterOffload"
#define CFG_ENABLE_DFS_PHYERR_FILTEROFFLOAD_MIN        (0)
#define CFG_ENABLE_DFS_PHYERR_FILTEROFFLOAD_MAX        (1)
#define CFG_ENABLE_DFS_PHYERR_FILTEROFFLOAD_DEFAULT    (0)

#define CFG_REPORT_MAX_LINK_SPEED                  "gReportMaxLinkSpeed"
#define CFG_REPORT_MAX_LINK_SPEED_MIN              (eHDD_LINK_SPEED_REPORT_ACTUAL)
#define CFG_REPORT_MAX_LINK_SPEED_MAX              (eHDD_LINK_SPEED_REPORT_MAX_SCALED)
#define CFG_REPORT_MAX_LINK_SPEED_DEFAULT          (eHDD_LINK_SPEED_REPORT_ACTUAL)

/*
 * <ini>
 * gLinkSpeedRssiHigh - Report the max possible speed with RSSI scaling
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default eHDD_LINK_SPEED_REPORT
 * Used when eHDD_LINK_SPEED_REPORT_SCALED is selected
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_LINK_SPEED_RSSI_HIGH                   "gLinkSpeedRssiHigh"
#define CFG_LINK_SPEED_RSSI_HIGH_MIN               (-127)
#define CFG_LINK_SPEED_RSSI_HIGH_MAX               (0)
#define CFG_LINK_SPEED_RSSI_HIGH_DEFAULT           (-55)

/*
 * <ini>
 * gLinkSpeedRssiMed - Used when eHDD_LINK_SPEED_REPORT_SCALED is selected
 * @Min: -127
 * @Max: 0
 * @Default: -65
 *
 * This ini is used to set medium rssi link speed
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_LINK_SPEED_RSSI_MID                    "gLinkSpeedRssiMed"
#define CFG_LINK_SPEED_RSSI_MID_MIN                (-127)
#define CFG_LINK_SPEED_RSSI_MID_MAX                (0)
#define CFG_LINK_SPEED_RSSI_MID_DEFAULT            (-65)

/*
 * <ini>
 * gLinkSpeedRssiLow - Used when eHDD_LINK_SPEED_REPORT_SCALED is selected
 * @Min: -127
 * @Max: 0
 * @Default: -80
 *
 * This ini is used to set low rssi link speed
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_LINK_SPEED_RSSI_LOW                    "gLinkSpeedRssiLow"
#define CFG_LINK_SPEED_RSSI_LOW_MIN                (-127)
#define CFG_LINK_SPEED_RSSI_LOW_MAX                (0)
#define CFG_LINK_SPEED_RSSI_LOW_DEFAULT            (-80)

/*
 * <ini>
 * isP2pDeviceAddrAdministrated - Enables to derive the P2P MAC address from
 * the primary MAC address
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable to derive the P2P MAC address from the
 * primary MAC address.
 *
 * Supported Feature: P2P
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_NAME                "isP2pDeviceAddrAdministrated"
#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MIN                 (0)
#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MAX                 (1)
#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_DEFAULT             (1)

/*
 * <ini>
 * gEnableSSR - Enable/Disable SSR
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable System Self Recovery at the times of
 * System crash or fatal errors
 * gEnableSSR = 0 Disabled
 * gEnableSSR = 1 wlan shutdown and re-init happens
 *
 * Related: None
 *
 * Supported Feature: SSR
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SSR                      "gEnableSSR"
#define CFG_ENABLE_SSR_MIN                  (0)
#define CFG_ENABLE_SSR_MAX                  (1)
#define CFG_ENABLE_SSR_DEFAULT              (1)

/**
 * <ini>
 * gEnableDataStallDetection - Enable/Disable Data stall detection
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable data stall detection
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_DATA_STALL_DETECTION           "gEnableDataStallDetection"
#define CFG_ENABLE_DATA_STALL_DETECTION_MIN       (0)
#define CFG_ENABLE_DATA_STALL_DETECTION_MAX       (1)
#define CFG_ENABLE_DATA_STALL_DETECTION_DEFAULT   (1)

/*
 * <ini>
 * gEnableOverLapCh - Enables Overlap Channel. If set, allow overlapping
 *                    channels to be selected for the SoftAP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set Overlap Channel
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_OVERLAP_CH               "gEnableOverLapCh"
#define CFG_ENABLE_OVERLAP_CH_MIN           (0)
#define CFG_ENABLE_OVERLAP_CH_MAX           (1)
#define CFG_ENABLE_OVERLAP_CH_DEFAULT       (0)

/*
 * <ini>
 * gEnable5gEBT - Enables/disables 5G early beacon termination. When enabled
 *                 terminate the reception of beacon if the TIM element is
 *                 clear for the power saving
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default 5G early beacon termination
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_PPS_ENABLE_5G_EBT                 "gEnable5gEBT"
#define CFG_PPS_ENABLE_5G_EBT_FEATURE_MIN     (0)
#define CFG_PPS_ENABLE_5G_EBT_FEATURE_MAX     (1)
#define CFG_PPS_ENABLE_5G_EBT_FEATURE_DEFAULT (1)

#define CFG_ENABLE_MEMORY_DEEP_SLEEP          "gEnableMemDeepSleep"
#define CFG_ENABLE_MEMORY_DEEP_SLEEP_MIN      (0)
#define CFG_ENABLE_MEMORY_DEEP_SLEEP_MAX      (1)
#define CFG_ENABLE_MEMORY_DEEP_SLEEP_DEFAULT  (1)

/*
 * <ini>
 *
 * gEnableCckTxFirOverride - Enable/disable CCK TxFIR Override
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 0 (disabled)
 *
 * When operating in an 802.11b mode, this configuration item forces a 2x2 radio
 * configuration into 1x for Tx and 2x for Rx (ie 1x2) for regulatory compliance
 * reasons.
 *
 * Related: enable2x2
 *
 * Supported Feature: 802.11b, 2x2
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_CCK_TX_FIR_OVERRIDE_NAME     "gEnableCckTxFirOverride"
#define CFG_ENABLE_CCK_TX_FIR_OVERRIDE_MIN      (0)
#define CFG_ENABLE_CCK_TX_FIR_OVERRIDE_MAX      (1)
#define CFG_ENABLE_CCK_TX_FIR_OVERRIDE_DEFAULT  (0)

#ifndef REMOVE_PKT_LOG
#define CFG_ENABLE_PACKET_LOG            "gEnablePacketLog"
#define CFG_ENABLE_PACKET_LOG_MIN        (0)
#define CFG_ENABLE_PACKET_LOG_MAX        (1)
#ifdef FEATURE_PKTLOG
#define CFG_ENABLE_PACKET_LOG_DEFAULT    (1)
#else
#define CFG_ENABLE_PACKET_LOG_DEFAULT    (0)
#endif

#define CFG_PACKET_LOG_BUFFER_SIZE_NAME    "PktlogBufSize"
#define CFG_PACKET_LOG_BUFFER_SIZE_DEFAULT (10)
#define CFG_PACKET_LOG_BUFFER_SIZE_MIN     (1)
#define CFG_PACKET_LOG_BUFFER_SIZE_MAX     (10)
#endif


/* gFwDebugLogType takes values from enum dbglog_process_t,
 * make default value as DBGLOG_PROCESS_NET_RAW to give the
 * logs to net link since cnss_diag service is started at boot
 * time by default.
 */
#define CFG_ENABLE_FW_LOG_TYPE            "gFwDebugLogType"
#define CFG_ENABLE_FW_LOG_TYPE_MIN        (0)
#define CFG_ENABLE_FW_LOG_TYPE_MAX        (255)
#define CFG_ENABLE_FW_LOG_TYPE_DEFAULT    (3)

/* gFwDebugLogLevel takes values from enum DBGLOG_LOG_LVL,
 * make default value as DBGLOG_WARN to enable error and
 * warning logs by default.
 */
#define CFG_ENABLE_FW_DEBUG_LOG_LEVEL          "gFwDebugLogLevel"
#define CFG_ENABLE_FW_DEBUG_LOG_LEVEL_MIN      (0)
#define CFG_ENABLE_FW_DEBUG_LOG_LEVEL_MAX      (255)
#define CFG_ENABLE_FW_DEBUG_LOG_LEVEL_DEFAULT  (3)

/* For valid values of log levels check enum DBGLOG_LOG_LVL and
 * for valid values of module ids check enum WLAN_MODULE_ID.
 */
#define CFG_ENABLE_FW_MODULE_LOG_LEVEL    "gFwDebugModuleLoglevel"
#define CFG_ENABLE_FW_MODULE_LOG_DEFAULT  "2,1,3,1,5,1,9,1,13,1,14,1,18,1,19,1,26,1,28,1,29,1,31,1,36,1,38,1,46,1,47,1,50,1,52,1,53,1,56,1,60,1,61,1,4,1"

/*
 * <ini>
 * gEnableConcurrentSTA - This will control the creation of concurrent STA
 * interface
 * @Default: NULL
 *
 * This ini is used for providing control to create a concurrent STA session
 * along with the creation of wlan0 and p2p0. The name of the interface is
 * specified as the parameter
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_CONCURRENT_STA           "gEnableConcurrentSTA"
#define CFG_ENABLE_CONCURRENT_STA_DEFAULT   ""

/*
 * <ini>
 * gEnableRTSProfiles - It will use configuring different RTS profiles
 * @Min: 0
 * @Max: 66
 * @Default: 33
 *
 * This ini used for configuring different RTS profiles
 * to firmware.
 * Following are the valid values for the rts profile:
 * RTSCTS_DISABLED				0
 * NOT_ALLOWED					1
 * NOT_ALLOWED					2
 * RTSCTS_DISABLED				16
 * RTSCTS_ENABLED_4_SECOND_RATESERIES		17
 * CTS2SELF_ENABLED_4_SECOND_RATESERIES		18
 * RTSCTS_DISABLED				32
 * RTSCTS_ENABLED_4_SWRETRIES			33
 * CTS2SELF_ENABLED_4_SWRETRIES			34
 * NOT_ALLOWED					48
 * NOT_ALLOWED					49
 * NOT_ALLOWED					50
 * RTSCTS_DISABLED				64
 * RTSCTS_ENABLED_4_ALL_RATESERIES		65
 * CTS2SELF_ENABLED_4_ALL_RATESERIES		66
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_FW_RTS_PROFILE              "gEnableRTSProfiles"
#define CFG_ENABLE_FW_RTS_PROFILE_MIN          (0)
#define CFG_ENABLE_FW_RTS_PROFILE_MAX          (66)
#define CFG_ENABLE_FW_RTS_PROFILE_DEFAULT      (33)


/*
 * QDF Trace Enable Control
 * Notes:
 *  the MIN/MAX/DEFAULT values apply for all modules
 *  the DEFAULT value is outside the valid range.  if the DEFAULT
 *    value is not overridden, then no change will be made to the
 *    "built in" default values compiled into the code
 *  values are a bitmap indicating which log levels are to enabled
 *    (must match order of qdf_trace_level enumerations)
 *    00000001  FATAL
 *    00000010  ERROR
 *    00000100  WARN
 *    00001000  INFO
 *    00010000  INFO HIGH
 *    00100000  INFO MED
 *    01000000  INFO LOW
 *    10000000  DEBUG
 *
 *  hence a value of 0xFF would set all bits (enable all logs)
 */

#define CFG_QDF_TRACE_ENABLE_WDI_NAME     "qdf_trace_enable_wdi"
#define CFG_QDF_TRACE_ENABLE_HDD_NAME     "qdf_trace_enable_hdd"
#define CFG_QDF_TRACE_ENABLE_SME_NAME     "qdf_trace_enable_sme"
#define CFG_QDF_TRACE_ENABLE_PE_NAME      "qdf_trace_enable_pe"
#define CFG_QDF_TRACE_ENABLE_PMC_NAME     "qdf_trace_enable_pmc"
#define CFG_QDF_TRACE_ENABLE_WMA_NAME     "qdf_trace_enable_wma"
#define CFG_QDF_TRACE_ENABLE_SYS_NAME     "qdf_trace_enable_sys"
#define CFG_QDF_TRACE_ENABLE_QDF_NAME     "qdf_trace_enable_qdf"
#define CFG_QDF_TRACE_ENABLE_SAP_NAME     "qdf_trace_enable_sap"
#define CFG_QDF_TRACE_ENABLE_HDD_SAP_NAME "qdf_trace_enable_hdd_sap"
#define CFG_QDF_TRACE_ENABLE_BMI_NAME     "qdf_trace_enable_bmi"
#define CFG_QDF_TRACE_ENABLE_CFG_NAME     "qdf_trace_enable_cfg"
#define CFG_QDF_TRACE_ENABLE_EPPING       "qdf_trace_enable_epping"
#define CFG_QDF_TRACE_ENABLE_QDF_DEVICES  "qdf_trace_enable_qdf_devices"
#define CFG_QDF_TRACE_ENABLE_TXRX_NAME    "qdf_trace_enable_txrx"
#define CFG_QDF_TRACE_ENABLE_DP_NAME      "qdf_trace_enable_dp"
#define CFG_QDF_TRACE_ENABLE_HTC_NAME     "qdf_trace_enable_htc"
#define CFG_QDF_TRACE_ENABLE_HIF_NAME     "qdf_trace_enable_hif"
#define CFG_CDR_TRACE_ENABLE_HDD_SAP_DATA_NAME   "qdf_trace_enable_hdd_sap_data"
#define CFG_QDF_TRACE_ENABLE_HDD_DATA_NAME       "qdf_trace_enable_hdd_data"
#define CFG_QDF_TRACE_ENABLE_WIFI_POS     "qdf_trace_enable_wifi_pos"
#define CFG_QDF_TRACE_ENABLE_NAN          "qdf_trace_enable_nan"
#define CFG_QDF_TRACE_ENABLE_REGULATORY   "qdf_trace_enable_regulatory"
#define CFG_QDF_TRACE_ENABLE_CP_STATS     "qdf_trace_enable_cp_stats"

#define CFG_QDF_TRACE_ENABLE_MIN          (0)
#define CFG_QDF_TRACE_ENABLE_MAX          (0xff)
#define CFG_QDF_TRACE_ENABLE_DEFAULT      (0xffff)
/* disable debug logs for DP by default */
#define CFG_QDF_TRACE_ENABLE_DP_DEFAULT   (0x7f)

#ifdef ENABLE_MTRACE_LOG
/*
 * Enable MTRACE for all modules
 */
#define CFG_ENABLE_MTRACE            "enable_mtrace"
#define CFG_ENABLE_MTRACE_MIN        (0)
#define CFG_ENABLE_MTRACE_MAX        (1)
#define CFG_ENABLE_MTRACE_DEFAULT    (0)
#endif

#define HDD_MCASTBCASTFILTER_FILTER_NONE                       0x00
#define HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST              0x01
#define HDD_MCASTBCASTFILTER_FILTER_ALL_BROADCAST              0x02
#define HDD_MCASTBCASTFILTER_FILTER_ALL_MULTICAST_BROADCAST    0x03
#define HDD_MULTICAST_FILTER_LIST                              0x04
#define HDD_MULTICAST_FILTER_LIST_CLEAR                        0x05

/*
 * Enable Dynamic DTIM
 * Options
 * 0 -Disable DynamicDTIM
 * 1 to 5 - SLM will switch to DTIM specified here when host suspends and
 *          switch DTIM1 when host resumes
 */
#define CFG_ENABLE_DYNAMIC_DTIM_NAME            "gEnableDynamicDTIM"
#define CFG_ENABLE_DYNAMIC_DTIM_MIN        (0)
#define CFG_ENABLE_DYNAMIC_DTIM_MAX        (9)
#define CFG_ENABLE_DYNAMIC_DTIM_DEFAULT    (0)

/*
 * <ini>
 * gConfigVCmodeBitmap - Bitmap for operating voltage corner mode
 * @Min: 0x00000000
 * @Max: 0x0fffffff
 * @Default: 0x0000000a
 * This ini is used to set operating voltage corner mode for differenet
 * phymode and bw configurations. Every 2 bits till BIT27 are dedicated
 * for a specific configuration. Bit values decide the type of voltage
 * corner mode. All the details below -
 *
 * Configure operating voltage corner mode based on phymode and bw.
 * bit 0-1 -   operating voltage corner mode for 11a/b.
 * bit 2-3 -   operating voltage corner mode for 11g.
 * bit 4-5 -   operating voltage corner mode for 11n, 20MHz, 1x1.
 * bit 6-7 -   operating voltage corner mode for 11n, 20MHz, 2x2.
 * bit 8-9 -   operating voltage corner mode for 11n, 40MHz, 1x1.
 * bit 10-11 - operating voltage corner mode for 11n, 40MHz, 2x2.
 * bit 12-13 - operating voltage corner mode for 11ac, 20MHz, 1x1.
 * bit 14-15 - operating voltage corner mode for 11ac, 20MHz, 2x2.
 * bit 16-17 - operating voltage corner mode for 11ac, 40MHz, 1x1.
 * bit 18-19 - operating voltage corner mode for 11ac, 40MHz, 2x2.
 * bit 20-21 - operating voltage corner mode for 11ac, 80MHz, 1x1.
 * bit 22-23 - operating voltage corner mode for 11ac, 80MHz, 2x2.
 * bit 24-25 - operating voltage corner mode for 11ac, 160MHz, 1x1.
 * bit 26-27 - operating voltage corner mode for 11ac, 160MHz, 2x2.
 * ---------------------------------------------
 * 00 - Static voltage corner SVS
 * 01 - static voltage corner LOW SVS
 * 10 - Dynamic voltage corner selection based on TPUT
 * 11 - Dynamic voltage corner selection based on TPUT and Tx Flush counters

 * Related: None
 *
 * Supported Feature: None
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_VC_MODE_BITMAP                  "gConfigVCmode"
#define CFG_VC_MODE_BITMAP_MIN              (0x00000000)
#define CFG_VC_MODE_BITMAP_MAX              (0x0fffffff)
#define CFG_VC_MODE_BITMAP_DEFAULT          (0x00000005)

/*
 * <ini>
 * gEnableSAPManadatoryChanList - Enable SAP Mandatory channel list
 * Options.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the SAP manadatory chan list
 * 0 - Disable SAP mandatory chan list
 * 1 - Enable SAP mandatory chan list
 *
 * Supported Feature: SAP
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_SAP_MANDATORY_CHAN_LIST       "gEnableSAPManadatoryChanList"
#define CFG_ENABLE_SAP_MANDATORY_CHAN_LIST_MIN   (0)
#define CFG_ENABLE_SAP_MANDATORY_CHAN_LIST_MAX   (1)
#define CFG_ENABLE_SAP_MANDATORY_CHAN_LIST_DEFAULT (0)

/*
 * Skip DFS Channel in case of P2P Search
 * Options
 * 0 - Don't Skip DFS Channel in case of P2P Search
 * 1 - Skip DFS Channel in case of P2P Search
 */
/*
 * <ini>
 * gSkipDfsChannelInP2pSearch - Skip DFS Channel in case of P2P Search
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to to disable(skip) dfs channel in p2p search.
 * Related: NA.
 *
 * Supported Feature: DFS P2P
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_NAME       "gSkipDfsChannelInP2pSearch"
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_MIN        (0)
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_MAX        (1)
#define CFG_ENABLE_SKIP_DFS_IN_P2P_SEARCH_DEFAULT    (1)
/*
 * <ini>
 * gIgnoreDynamicDtimInP2pMode - Ignore Dynamic Dtim in case of P2P
 * Options.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to decide if Dynamic Dtim needs to be consider or
 * not in case of P2P.
 * 0 - Consider Dynamic Dtim incase of P2P
 * 1 - Ignore Dynamic Dtim incase of P2P
 *
 * Supported Feature: P2P
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_NAME       "gIgnoreDynamicDtimInP2pMode"
#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_MIN        (0)
#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_MAX        (1)
#define CFG_IGNORE_DYNAMIC_DTIM_IN_P2P_MODE_DEFAULT    (0)

/*
 * <ini>
 * gShortGI40Mhz - It will check gShortGI20Mhz and
 * gShortGI40Mhz from session entry
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default gShortGI40Mhz
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_SHORT_GI_40MHZ_NAME                "gShortGI40Mhz"
#define CFG_SHORT_GI_40MHZ_MIN                 0
#define CFG_SHORT_GI_40MHZ_MAX                 1
#define CFG_SHORT_GI_40MHZ_DEFAULT             1

/*
 * <ini>
 * gEnableMCCMode - Enable/Disable MCC feature.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable MCC feature.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_MCC_ENABLED_NAME             "gEnableMCCMode"
#define CFG_ENABLE_MCC_ENABLED_MIN              (0)
#define CFG_ENABLE_MCC_ENABLED_MAX              (1)
#define CFG_ENABLE_MCC_ENABLED_DEFAULT          (1)

/*
 * <ini>
 * gAllowMCCGODiffBI - Allow GO in MCC mode to accept different beacon interval
 * than STA's.
 * @Min: 0
 * @Max: 4
 * @Default: 4
 *
 * This ini is used to allow GO in MCC mode to accept different beacon interval
 * than STA's.
 * Added for Wi-Fi Cert. 5.1.12
 * If gAllowMCCGODiffBI = 1
 *	Set to 1 for WFA certification. GO Beacon interval is not changed.
 *	MCC GO doesn't work well in optimized way. In worst scenario, it may
 *	invite STA disconnection.
 * If gAllowMCCGODiffBI = 2
 *	If set to 2 workaround 1 disassoc all the clients and update beacon
 *	Interval.
 * If gAllowMCCGODiffBI = 3
 *	If set to 3 tear down the P2P link in auto/Non-autonomous -GO case.
 * If gAllowMCCGODiffBI = 4
 *	If set to 4 don't disconnect the P2P client in autonomous/Non-auto-
 *	nomous -GO case update the BI dynamically
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ALLOW_MCC_GO_DIFF_BI_NAME           "gAllowMCCGODiffBI"
#define CFG_ALLOW_MCC_GO_DIFF_BI_MIN            (0)
#define CFG_ALLOW_MCC_GO_DIFF_BI_MAX            (4)
#define CFG_ALLOW_MCC_GO_DIFF_BI_DEFAULT        (4)

#if defined(CONFIG_HL_SUPPORT) && defined(QCA_BAD_PEER_TX_FLOW_CL)
/*
 * Enable/Disable Bad Peer TX CTL feature
 * Default: Enable
 */
#define CFG_BAD_PEER_TX_CTL_ENABLE_NAME		"gBadPeerTxCtlEnable"
#define CFG_BAD_PEER_TX_CTL_ENABLE_MIN         (0)
#define CFG_BAD_PEER_TX_CTL_ENABLE_MAX         (1)
#define CFG_BAD_PEER_TX_CTL_ENABLE_DEFAULT     (1)

#define CFG_BAD_PEER_TX_CTL_PERIOD_NAME		"gBadPeerTxCtlPeriod"
#define CFG_BAD_PEER_TX_CTL_PERIOD_MIN         (10)
#define CFG_BAD_PEER_TX_CTL_PERIOD_MAX         (10000)
#define CFG_BAD_PEER_TX_CTL_PERIOD_DEFAULT     (50)

#define CFG_BAD_PEER_TX_CTL_TXQ_LIMIT_NAME	"gBadPeerTxCtlTxqLimit"
#define CFG_BAD_PEER_TX_CTL_TXQ_LIMIT_MIN      (1)
#define CFG_BAD_PEER_TX_CTL_TXQ_LIMIT_MAX      (5000)
#define CFG_BAD_PEER_TX_CTL_TXQ_LIMIT_DEFAULT  (100)

#define CFG_BAD_PEER_TX_CTL_TGT_BACKOFF_T_NAME	"gBadPeerTxCtlTgtBackoffTime"
#define CFG_BAD_PEER_TX_CTL_TGT_BACKOFF_T_MIN     (1)
#define CFG_BAD_PEER_TX_CTL_TGT_BACKOFF_T_MAX     (5000)
#define CFG_BAD_PEER_TX_CTL_TGT_BACKOFF_T_DEFAULT (20)

#define CFG_BAD_PEER_TX_CTL_TGT_REPORT_PRD_NAME	"gBadPeerTxCtlTgtReportPeriod"
#define CFG_BAD_PEER_TX_CTL_TGT_REPORT_PRD_MIN     (1)
#define CFG_BAD_PEER_TX_CTL_TGT_REPORT_PRD_MAX     (5000)
#define CFG_BAD_PEER_TX_CTL_TGT_REPORT_PRD_DEFAULT (500)

#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEB_NAME	"gBadPeerTxCtlCondLevelIeeeB"
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEB_MIN     (1)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEB_MAX     (2)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEB_DEFAULT (2)

#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEB_NAME	"gBadPeerTxCtlDeltaLevelIeeeB"
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEB_MIN     (1)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEB_MAX     (11)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEB_DEFAULT (2)

#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEB_NAME	"gBadPeerTxCtlPctLevelIeeeB"
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEB_MIN        (1)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEB_MAX        (8)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEB_DEFAULT    (1)

#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEB_NAME	"gBadPeerTxCtlTputLevelIeeeB"
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEB_MIN       (1)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEB_MAX       (11)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEB_DEFAULT   (2)

#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEB_NAME	"gBadPeerTxCtlTxLimitLevelIeeeB"
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEB_MIN      (0)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEB_MAX      (50)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEB_DEFAULT  (3)

#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAG_NAME	"gBadPeerTxCtlCondLevelIeeeAG"
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAG_MIN         (1)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAG_MAX         (2)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAG_DEFAULT     (2)

#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAG_NAME	"gBadPeerTxCtlDeltaLevelIeeeAG"
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAG_MIN        (6)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAG_MAX        (54)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAG_DEFAULT    (6)

#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAG_NAME	"gBadPeerTxCtlPctLevelIeeeAG"
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAG_MIN          (1)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAG_MAX          (8)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAG_DEFAULT      (1)

#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAG_NAME	"gBadPeerTxCtlTputLevelIeeeAG"
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAG_MIN         (6)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAG_MAX         (54)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAG_DEFAULT     (6)

#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAG_NAME	"gBadPeerTxCtlTxLimitLevelIeeeAG"
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAG_MIN     (0)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAG_MAX     (50)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAG_DEFAULT (3)

#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEN_NAME	"gBadPeerTxCtlCondLevelIeeeN"
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEN_MIN          (1)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEN_MAX          (2)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEN_DEFAULT      (2)

#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEN_NAME	"gBadPeerTxCtlDeltaLevelIeeeN"
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEN_MIN         (6)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEN_MAX         (72)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEN_DEFAULT     (6)

#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEN_NAME	"gBadPeerTxCtlPctLevelIeeeN"
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEN_MIN           (1)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEN_MAX           (8)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEN_DEFAULT       (1)

#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEN_NAME	"gBadPeerTxCtlTputLevelIeeeN"
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEN_MIN          (6)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEN_MAX          (72)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEN_DEFAULT      (15)

#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEN_NAME	"gBadPeerTxCtlTxLimitLevelIeeeN"
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEN_MIN      (0)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEN_MAX      (50)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEN_DEFAULT  (3)

#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAC_NAME	"gBadPeerTxCtlCondLevelIeeeAC"
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAC_MIN         (1)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAC_MAX         (2)
#define CFG_BAD_PEER_TX_CTL_COND_LEVEL_IEEEAC_DEFAULT     (2)

#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAC_NAME	"gBadPeerTxCtlDeltaLevelIeeeAC"
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAC_MIN        (6)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAC_MAX        (433)
#define CFG_BAD_PEER_TX_CTL_DELTA_LEVEL_IEEEAC_DEFAULT    (6)

#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAC_NAME	"gBadPeerTxCtlPctLevelIeeeAC"
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAC_MIN          (1)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAC_MAX          (8)
#define CFG_BAD_PEER_TX_CTL_PCT_LEVEL_IEEEAC_DEFAULT      (1)

#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAC_NAME	"gBadPeerTxCtlTputLevelIeeeAC"
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAC_MIN         (6)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAC_MAX         (433)
#define CFG_BAD_PEER_TX_CTL_TPUT_LEVEL_IEEEAC_DEFAULT     (15)

#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAC_NAME    "gBadPeerTxCtlTxLimitLevelIeeeAC"
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAC_MIN     (0)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAC_MAX     (50)
#define CFG_BAD_PEER_TX_CTL_TX_LIMIT_LEVEL_IEEEAC_DEFAULT (3)
#endif


/*
 * Enable/Disable Thermal Mitigation feature
 * Default: Enable
 */
#define CFG_THERMAL_MIGRATION_ENABLE_NAME      "gThermalMitigationEnable"
#define CFG_THERMAL_MIGRATION_ENABLE_MIN       (0)
#define CFG_THERMAL_MIGRATION_ENABLE_MAX       (1)
#define CFG_THERMAL_MIGRATION_ENABLE_DEFAULT   (0)

#define CFG_THROTTLE_PERIOD_NAME               "gThrottlePeriod"
#define CFG_THROTTLE_PERIOD_MIN                (10)
#define CFG_THROTTLE_PERIOD_MAX                (10000)
#define CFG_THROTTLE_PERIOD_DEFAULT            (4000)

/*
 * Configure Throttle Period Different Level Duty Cycle in percentage
 * When temperature measured is greater than threshold at particular level,
 * then throtling level will get increased by one level and
 * will reduce TX duty by the given percentage
 */
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL0_NAME    "gThrottleDutyCycleLevel0"
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL0_MIN     (0)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL0_MAX     (0)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL0_DEFAULT (0)

#define CFG_THROTTLE_DUTY_CYCLE_LEVEL1_NAME    "gThrottleDutyCycleLevel1"
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL1_MIN     (0)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL1_MAX     (100)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL1_DEFAULT (50)

#define CFG_THROTTLE_DUTY_CYCLE_LEVEL2_NAME    "gThrottleDutyCycleLevel2"
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL2_MIN     (0)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL2_MAX     (100)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL2_DEFAULT (75)

#define CFG_THROTTLE_DUTY_CYCLE_LEVEL3_NAME    "gThrottleDutyCycleLevel3"
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL3_MIN     (0)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL3_MAX     (100)
#define CFG_THROTTLE_DUTY_CYCLE_LEVEL3_DEFAULT (94)

#define CFG_THERMAL_TEMP_MIN_LEVEL0_NAME      "gThermalTempMinLevel0"
#define CFG_THERMAL_TEMP_MIN_LEVEL0_MIN       (0)
#define CFG_THERMAL_TEMP_MIN_LEVEL0_MAX       (1000)
#define CFG_THERMAL_TEMP_MIN_LEVEL0_DEFAULT   (0)

#define CFG_THERMAL_TEMP_MAX_LEVEL0_NAME      "gThermalTempMaxLevel0"
#define CFG_THERMAL_TEMP_MAX_LEVEL0_MIN       (0)
#define CFG_THERMAL_TEMP_MAX_LEVEL0_MAX       (1000)
#define CFG_THERMAL_TEMP_MAX_LEVEL0_DEFAULT   (90)

#define CFG_THERMAL_TEMP_MIN_LEVEL1_NAME      "gThermalTempMinLevel1"
#define CFG_THERMAL_TEMP_MIN_LEVEL1_MIN       (0)
#define CFG_THERMAL_TEMP_MIN_LEVEL1_MAX       (1000)
#define CFG_THERMAL_TEMP_MIN_LEVEL1_DEFAULT   (70)

#define CFG_THERMAL_TEMP_MAX_LEVEL1_NAME      "gThermalTempMaxLevel1"
#define CFG_THERMAL_TEMP_MAX_LEVEL1_MIN       (0)
#define CFG_THERMAL_TEMP_MAX_LEVEL1_MAX       (1000)
#define CFG_THERMAL_TEMP_MAX_LEVEL1_DEFAULT   (110)

#define CFG_THERMAL_TEMP_MIN_LEVEL2_NAME      "gThermalTempMinLevel2"
#define CFG_THERMAL_TEMP_MIN_LEVEL2_MIN       (0)
#define CFG_THERMAL_TEMP_MIN_LEVEL2_MAX       (1000)
#define CFG_THERMAL_TEMP_MIN_LEVEL2_DEFAULT   (90)

#define CFG_THERMAL_TEMP_MAX_LEVEL2_NAME      "gThermalTempMaxLevel2"
#define CFG_THERMAL_TEMP_MAX_LEVEL2_MIN       (0)
#define CFG_THERMAL_TEMP_MAX_LEVEL2_MAX       (1000)
#define CFG_THERMAL_TEMP_MAX_LEVEL2_DEFAULT   (125)

#define CFG_THERMAL_TEMP_MIN_LEVEL3_NAME      "gThermalTempMinLevel3"
#define CFG_THERMAL_TEMP_MIN_LEVEL3_MIN       (0)
#define CFG_THERMAL_TEMP_MIN_LEVEL3_MAX       (1000)
#define CFG_THERMAL_TEMP_MIN_LEVEL3_DEFAULT   (110)

#define CFG_THERMAL_TEMP_MAX_LEVEL3_NAME      "gThermalTempMaxLevel3"
#define CFG_THERMAL_TEMP_MAX_LEVEL3_MIN       (0)
#define CFG_THERMAL_TEMP_MAX_LEVEL3_MAX       (1000)
#define CFG_THERMAL_TEMP_MAX_LEVEL3_DEFAULT   (0)

/*
 * Enable/Disable Modulated DTIM feature
 * Default: Disable
 */
#define CFG_ENABLE_MODULATED_DTIM_NAME       "gEnableModulatedDTIM"
#define CFG_ENABLE_MODULATED_DTIM_MIN        (0)
#define CFG_ENABLE_MODULATED_DTIM_MAX        (5)
#define CFG_ENABLE_MODULATED_DTIM_DEFAULT    (0)

/*
 * <ini>
 * gMCAddrListEnable - Enable/Disable Multicast MAC Address List feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default MAC Address
 * Default: Enable
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_MC_ADDR_LIST_ENABLE_NAME          "gMCAddrListEnable"
#define CFG_MC_ADDR_LIST_ENABLE_MIN           (0)
#define CFG_MC_ADDR_LIST_ENABLE_MAX           (1)
#define CFG_MC_ADDR_LIST_ENABLE_DEFAULT       (1)

/*
 * <ini>
 * gEnableRXSTBC - Enables/disables Rx STBC capability in STA mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default Rx STBC capability
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_RX_STBC                       "gEnableRXSTBC"
#define CFG_ENABLE_RX_STBC_MIN                   (0)
#define CFG_ENABLE_RX_STBC_MAX                   (1)
#define CFG_ENABLE_RX_STBC_DEFAULT               (1)

/*
 * <ini>
 * gEnableTXSTBC - Enables/disables Tx STBC capability in STA mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default Tx STBC capability
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_TX_STBC                       "gEnableTXSTBC"
#define CFG_ENABLE_TX_STBC_MIN                   (0)
#define CFG_ENABLE_TX_STBC_MAX                   (1)
#define CFG_ENABLE_TX_STBC_DEFAULT               (0)

/*
 * <ini>
 * gMaxHTMCSForTxData - max HT mcs for TX
 * @Min: 0
 * @Max: 383
 * @Default: 0
 *
 * This ini is used to configure the max HT mcs
 * for tx data.
 *
 * Usage: External
 *
 * bits 0-15:  max HT mcs
 * bits 16-31: zero to disable, otherwise enable.
 *
 * </ini>
 */
#define CFG_MAX_HT_MCS_FOR_TX_DATA          "gMaxHTMCSForTxData"
#define CFG_MAX_HT_MCS_FOR_TX_DATA_MIN      (WNI_CFG_MAX_HT_MCS_TX_DATA_STAMIN)
#define CFG_MAX_HT_MCS_FOR_TX_DATA_MAX      (WNI_CFG_MAX_HT_MCS_TX_DATA_STAMAX)
#define CFG_MAX_HT_MCS_FOR_TX_DATA_DEFAULT  (WNI_CFG_MAX_HT_MCS_TX_DATA_STADEF)

/*
 * <ini>
 * gSapGetPeerInfo - Enable/Disable remote peer info query support
 * @Min: 0 - Disable remote peer info query support
 * @Max: 1 - Enable remote peer info query support
 * @Default: 1
 *
 * This ini is used to enable/disable remote peer info query support
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_GET_PEER_INFO                      "gSapGetPeerInfo"
#define CFG_SAP_GET_PEER_INFO_MIN                   (0)
#define CFG_SAP_GET_PEER_INFO_MAX                   (1)
#define CFG_SAP_GET_PEER_INFO_DEFAULT               (1)

/*
 * <ini>
 * gDisableABGRateForTxData - disable abg rate for tx data
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to disable abg rate for tx data.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_ABG_RATE_FOR_TX_DATA        "gDisableABGRateForTxData"
#define CFG_DISABLE_ABG_RATE_FOR_TX_DATA_MIN \
	(WNI_CFG_DISABLE_ABG_RATE_FOR_TX_DATA_STAMIN)
#define CFG_DISABLE_ABG_RATE_FOR_TX_DATA_MAX \
	(WNI_CFG_DISABLE_ABG_RATE_FOR_TX_DATA_STAMAX)
#define CFG_DISABLE_ABG_RATE_FOR_TX_DATA_DEFAULT \
	(WNI_CFG_DISABLE_ABG_RATE_FOR_TX_DATA_STADEF)

/*
 * <ini>
 * gRateForTxMgmt - rate for tx mgmt frame
 * @Min: 0x0
 * @Max: 0xFF
 * @Default: 0xFF
 *
 * This ini is used to configure the rate for tx
 * mgmt frame. Default 0xFF means disable.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RATE_FOR_TX_MGMT                  "gRateForTxMgmt"
#define CFG_RATE_FOR_TX_MGMT_MIN              (WNI_CFG_RATE_FOR_TX_MGMT_STAMIN)
#define CFG_RATE_FOR_TX_MGMT_MAX              (WNI_CFG_RATE_FOR_TX_MGMT_STAMAX)
#define CFG_RATE_FOR_TX_MGMT_DEFAULT          (WNI_CFG_RATE_FOR_TX_MGMT_STADEF)

/*
 * <ini>
 * gRateForTxMgmt2G - rate for tx mgmt frame on 2G
 * @Min: 0x0
 * @Max: 0xFF
 * @Default: 0xFF
 *
 * This ini is used to configure the rate for tx
 * mgmt frame on 2G Band. Default 0xFF means disable.
 * It has higher priority and will overwrite gRateForTxMgmt
 * setting.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RATE_FOR_TX_MGMT_2G            "gRateForTxMgmt2G"
#define CFG_RATE_FOR_TX_MGMT_2G_MIN        (WNI_CFG_RATE_FOR_TX_MGMT_2G_STAMIN)
#define CFG_RATE_FOR_TX_MGMT_2G_MAX        (WNI_CFG_RATE_FOR_TX_MGMT_2G_STAMAX)
#define CFG_RATE_FOR_TX_MGMT_2G_DEFAULT    (WNI_CFG_RATE_FOR_TX_MGMT_2G_STADEF)

/*
 * <ini>
 * gRateForTxMgmt5G - rate for tx mgmt frame on 5G
 * @Min: 0x0
 * @Max: 0xFF
 * @Default: 0xFF
 *
 * This ini is used to configure the rate for tx
 * mgmt frame on 5G Band. Default 0xFF means disable.
 * It has higher priority and will overwrite gRateForTxMgmt
 * setting.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RATE_FOR_TX_MGMT_5G            "gRateForTxMgmt5G"
#define CFG_RATE_FOR_TX_MGMT_5G_MIN        (WNI_CFG_RATE_FOR_TX_MGMT_5G_STAMIN)
#define CFG_RATE_FOR_TX_MGMT_5G_MAX        (WNI_CFG_RATE_FOR_TX_MGMT_5G_STAMAX)
#define CFG_RATE_FOR_TX_MGMT_5G_DEFAULT    (WNI_CFG_RATE_FOR_TX_MGMT_5G_STADEF)

/*
 * <ini>
 * gPreventLinkDown - Enable to prevent bus link from going down
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Enable to prevent bus link from going down. Useful for platforms that do not
 * (yet) support link down suspend cases.
 *
 * Related: N/A
 *
 * Supported Feature: Suspend/Resume
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PREVENT_LINK_DOWN_NAME		"gPreventLinkDown"
#define CFG_PREVENT_LINK_DOWN_MIN		(0)
#define CFG_PREVENT_LINK_DOWN_MAX		(1)
#if defined(QCA_WIFI_NAPIER_EMULATION) || defined(QCA_WIFI_QCA6290)
#define CFG_PREVENT_LINK_DOWN_DEFAULT		(1)
#else
#define CFG_PREVENT_LINK_DOWN_DEFAULT		(0)
#endif /* QCA_WIFI_NAPIER_EMULATION */

#ifdef FEATURE_WLAN_TDLS
/*
 * <ini>
 * gEnableTDLSSupport - Enable support for TDLS.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable TDLS support.
 *
 * Related: None.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_SUPPORT_ENABLE                     "gEnableTDLSSupport"
#define CFG_TDLS_SUPPORT_ENABLE_MIN                 (0)
#define CFG_TDLS_SUPPORT_ENABLE_MAX                 (1)
#define CFG_TDLS_SUPPORT_ENABLE_DEFAULT             (0)

/*
 * <ini>
 * gEnableTDLSImplicitTrigger - Enable Implicit TDLS.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable implicit TDLS.
 * CLD driver initiates TDLS Discovery towards a peer whenever TDLS Setup
 * criteria (throughput and RSSI thresholds) is met and then it tears down
 * TDLS when teardown criteria (idle packet count and RSSI) is met.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_IMPLICIT_TRIGGER                   "gEnableTDLSImplicitTrigger"
#define CFG_TDLS_IMPLICIT_TRIGGER_MIN               (0)
#define CFG_TDLS_IMPLICIT_TRIGGER_MAX               (1)
#define CFG_TDLS_IMPLICIT_TRIGGER_DEFAULT           (0)

/*
 * <ini>
 * gTDLSTxStatsPeriod - TDLS TX statistics time period.
 * @Min: 1000
 * @Max: 4294967295
 * @Default: 2000
 *
 * This ini is used to configure the time period (in ms) to evaluate whether
 * the number of Tx/Rx packets exceeds TDLSTxPacketThreshold and triggers a
 * TDLS Discovery request.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_TX_STATS_PERIOD                    "gTDLSTxStatsPeriod"
#define CFG_TDLS_TX_STATS_PERIOD_MIN                (1000)
#define CFG_TDLS_TX_STATS_PERIOD_MAX                (4294967295UL)
#define CFG_TDLS_TX_STATS_PERIOD_DEFAULT            (2000)

/*
 * <ini>
 * gTDLSTxPacketThreshold - Tx/Rx Packet threshold for initiating TDLS.
 * @Min: 0
 * @Max: 4294967295
 * @Default: 40
 *
 * This ini is used to configure the number of Tx/Rx packets during the
 * period of gTDLSTxStatsPeriod when exceeded, a TDLS Discovery request
 * is triggered.
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_TX_PACKET_THRESHOLD                "gTDLSTxPacketThreshold"
#define CFG_TDLS_TX_PACKET_THRESHOLD_MIN            (0)
#define CFG_TDLS_TX_PACKET_THRESHOLD_MAX            (4294967295UL)
#define CFG_TDLS_TX_PACKET_THRESHOLD_DEFAULT        (40)

/*
 * <ini>
 * gTDLSMaxDiscoveryAttempt - Attempts for sending TDLS discovery requests.
 * @Min: 1
 * @Max: 100
 * @Default: 5
 *
 * This ini is used to configure the number of failures of discover request,
 * when exceeded, the peer is assumed to be not TDLS capable and no further
 * TDLS Discovery request is made.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT              "gTDLSMaxDiscoveryAttempt"
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MIN          (1)
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT_MAX          (100)
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT_DEFAULT      (5)

/*
 * <ini>
 * gTDLSIdleTimeout - Duration within which number of TX / RX frames meet the
 * criteria for TDLS teardown.
 * @Min: 500
 * @Max: 40000
 * @Default: 5000
 *
 * This ini is used to configure the time period (in ms) to evaluate whether
 * the number of Tx/Rx packets exceeds gTDLSIdlePacketThreshold and thus meets
 * criteria for TDLS teardown.
 * Teardown notification interval (gTDLSIdleTimeout) should be multiple of
 * setup notification (gTDLSTxStatsPeriod) interval.
 * e.g.
 *      if setup notification (gTDLSTxStatsPeriod) interval = 500, then
 *      teardown notification (gTDLSIdleTimeout) interval should be 1000,
 *      1500, 2000, 2500...
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_IDLE_TIMEOUT                       "gTDLSIdleTimeout"
#define CFG_TDLS_IDLE_TIMEOUT_MIN                   (500)
#define CFG_TDLS_IDLE_TIMEOUT_MAX                   (40000)
#define CFG_TDLS_IDLE_TIMEOUT_DEFAULT               (5000)


/*
 * <ini>
 * gTDLSIdlePacketThreshold - Number of idle packet.
 * @Min: 0
 * @Max: 40000
 * @Default: 3
 *
 * This ini is used to configure the number of Tx/Rx packet, below which
 * within last gTDLSTxStatsPeriod period is considered as idle condition.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_IDLE_PACKET_THRESHOLD              "gTDLSIdlePacketThreshold"
#define CFG_TDLS_IDLE_PACKET_THRESHOLD_MIN          (0)
#define CFG_TDLS_IDLE_PACKET_THRESHOLD_MAX          (40000)
#define CFG_TDLS_IDLE_PACKET_THRESHOLD_DEFAULT      (3)

/*
 * <ini>
 * gTDLSRSSITriggerThreshold - RSSI threshold for TDLS connection.
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * This ini is used to configure the absolute value (in dB) of the peer RSSI,
 * below which a TDLS setup request is triggered.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD             "gTDLSRSSITriggerThreshold"
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MIN         (-120)
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD_MAX         (0)
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD_DEFAULT     (-75)

/*
 * <ini>
 * gTDLSRSSITeardownThreshold - RSSI threshold for TDLS teardown.
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * This ini is used to configure the absolute value (in dB) of the peer RSSI,
 * when exceed, a TDLS teardown is triggered.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD            "gTDLSRSSITeardownThreshold"
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MIN        (-120)
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_MAX        (0)
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD_DEFAULT    (-75)

/*
 * <ini>
 * gTDLSRSSIDelta - Delta value for the peer RSSI that can trigger teardown.
 * @Min: -30
 * @Max: 0
 * @Default: -20
 *
 * This ini is used to .
 * This ini is used to configure delta for peer RSSI such that if Peer RSSI
 * is less than AP RSSI plus delta will trigger a teardown.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_RSSI_DELTA                         "gTDLSRSSIDelta"
#define CFG_TDLS_RSSI_DELTA_MIN                     (-30)
#define CFG_TDLS_RSSI_DELTA_MAX                     (0)
#define CFG_TDLS_RSSI_DELTA_DEFAULT                 (-20)

/*
 * <ini>
 * gTDLSUapsdMask - ACs to setup U-APSD for TDLS Sta.
 * @Min: 0
 * @Max: 0x0F
 * @Default: 0x0F
 *
 * This ini is used to configure the ACs for which mask needs to be enabled.
 * 0x1: Background	0x2: Best effort
 * 0x4: Video		0x8:Voice
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_QOS_WMM_UAPSD_MASK_NAME            "gTDLSUapsdMask"
#define CFG_TDLS_QOS_WMM_UAPSD_MASK_MIN             (0)
#define CFG_TDLS_QOS_WMM_UAPSD_MASK_MAX             (0x0F)
#define CFG_TDLS_QOS_WMM_UAPSD_MASK_DEFAULT         (0x0F)

/*
 * <ini>
 * gEnableTDLSBufferSta - Controls the TDLS buffer.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to control the TDLS buffer.
 * Buffer STA is not enabled in CLD 2.0 yet.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE          "gEnableTDLSBufferSta"
#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_MIN      (0)
#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_MAX      (1)
#define CFG_TDLS_BUFFER_STA_SUPPORT_ENABLE_DEFAULT  (1)

/*
 * <ini>
 * gTDLSPuapsdInactivityTime - Peer UAPSD Inactivity time.
 * @Min: 0
 * @Max: 10
 * @Default: 0
 *
 * This ini is used to configure peer uapsd inactivity time.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PUAPSD_INACTIVITY_TIME             "gTDLSPuapsdInactivityTime"
#define CFG_TDLS_PUAPSD_INACTIVITY_TIME_MIN         (0)
#define CFG_TDLS_PUAPSD_INACTIVITY_TIME_MAX         (10)
#define CFG_TDLS_PUAPSD_INACTIVITY_TIME_DEFAULT     (0)

/*
 * <ini>
 * gTDLSPuapsdRxFrameThreshold - Peer UAPSD Rx frame threshold.
 * @Min: 10
 * @Max: 20
 * @Default: 10
 *
 * This ini is used to configure maximum Rx frame during SP.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD          "gTDLSPuapsdRxFrameThreshold"
#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_MIN      (10)
#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_MAX      (20)
#define CFG_TDLS_PUAPSD_RX_FRAME_THRESHOLD_DEFAULT  (10)

/*
 * <ini>
 * gTDLSPuapsdPTIWindow - This ini is used to configure peer traffic indication
 * window.
 * @Min: 1
 * @Max: 5
 * @Default: 2
 *
 * This ini is used to configure buffering time in number of beacon intervals.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_IND_WINDOW          "gTDLSPuapsdPTIWindow"
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_IND_WINDOW_MIN      (1)
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_IND_WINDOW_MAX      (5)
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_IND_WINDOW_DEFAULT  (2)

/*
 * <ini>
 * gTDLSPuapsdPTRTimeout - Peer Traffic Response timer duration in ms.
 * @Min: 0
 * @Max: 10000
 * @Default: 5000
 *
 * This ini is used to configure the peer traffic response timer duration
 * in ms.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_RSP_TIMEOUT         "gTDLSPuapsdPTRTimeout"
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_RSP_TIMEOUT_MIN     (0)
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_RSP_TIMEOUT_MAX     (10000)
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_RSP_TIMEOUT_DEFAULT (5000)

/*
 * <ini>
 * gTDLSExternalControl - Enable external TDLS control.
 * @Min: 0
 * @Max: 2
 * @Default: 1
 *
 * This ini is used to enable/disable external TDLS control.
 * TDLS external control works with TDLS implicit trigger. TDLS external
 * control allows a user to add a MAC address of potential TDLS peers so
 * that the CLD driver can initiate implicit TDLS setup to only those peers
 * when criteria for TDLS setup (throughput and RSSI threshold) is met.
 * There are two flavors of external control supported. If control default
 * is set 1 it means strict external control where only for configured
 * tdls peer mac address tdls link will be established. If control default
 * is set 2 it means liberal tdls external control needed. It means
 * tdls link will be established with configured peer mac address as well
 * as any other peer which supports tdls.
 *
 * Related: gEnableTDLSSupport, gEnableTDLSImplicitTrigger.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_EXTERNAL_CONTROL                   "gTDLSExternalControl"
#define CFG_TDLS_EXTERNAL_CONTROL_MIN               (0)
#define CFG_TDLS_EXTERNAL_CONTROL_MAX               (2)
#define CFG_TDLS_EXTERNAL_CONTROL_DEFAULT           (1)

/*
 * This INI item is used to control subsystem restart(SSR) test framework
 * Set it's value to 1 to enable APPS trigerred SSR testing
 */
#define CFG_ENABLE_CRASH_INJECT         "gEnableForceTargetAssert"
#define CFG_ENABLE_CRASH_INJECT_MIN     (0)
#define CFG_ENABLE_CRASH_INJECT_MAX     (1)
#define CFG_ENABLE_CRASH_INJECT_DEFAULT (0)

/*
 * <ini>
 * gEnableTDLSOffChannel - Enables off-channel support for TDLS link.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable off-channel support for TDLS link.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE          "gEnableTDLSOffChannel"
#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_MIN      (0)
#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_MAX      (1)
#define CFG_TDLS_OFF_CHANNEL_SUPPORT_ENABLE_DEFAULT  (0)

/*
 * <ini>
 * gEnableTDLSWmmMode - Enables WMM support over TDLS link.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable WMM support over TDLS link.
 * This is required to be set to 1 for any TDLS and uAPSD functionality.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_WMM_MODE_ENABLE                     "gEnableTDLSWmmMode"
#define CFG_TDLS_WMM_MODE_ENABLE_MIN                 (0)
#define CFG_TDLS_WMM_MODE_ENABLE_MAX                 (1)
#define CFG_TDLS_WMM_MODE_ENABLE_DEFAULT             (1)

/*
 * <ini>
 * gTDLSPrefOffChanNum - Preferred TDLS channel number when off-channel support
 * is enabled.
 * @Min: 1
 * @Max: 165
 * @Default: 36
 *
 * This ini is used to configure preferred TDLS channel number when off-channel
 * support is enabled.
 *
 * Related: gEnableTDLSSupport, gEnableTDLSOffChannel.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_NUM          "gTDLSPrefOffChanNum"
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_NUM_MIN      (1)
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_NUM_MAX      (165)
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_NUM_DEFAULT  (36)

/*
 * <ini>
 * gTDLSPrefOffChanBandwidth - Preferred TDLS channel bandwidth when
 * off-channel support is enabled.
 * @Min: 0x01
 * @Max: 0x0F
 * @Default: 0x07
 *
 * This ini is used to configure preferred TDLS channel bandwidth when
 * off-channel support is enabled.
 * 0x1: 20 MHz	0x2: 40 MHz	0x4: 80 MHz	0x8: 160 MHz
 * When more than one bits are set then firmware starts from the highest and
 * selects one based on capability of peer.
 *
 * Related: gEnableTDLSSupport, gEnableTDLSOffChannel.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_BW           "gTDLSPrefOffChanBandwidth"
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_BW_MIN      (0x01)
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_BW_MAX      (0x0F)
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_BW_DEFAULT  (0x07)

/*
 * <ini>
 * gEnableTDLSScan - Allow scan and maintain TDLS link.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable TDLS scan.
 *  0: If peer is not buffer STA capable and device is not sleep STA
 *     capable, then teardown TDLS link when scan is initiated. If peer
 *     is buffer STA and we can be sleep STA then TDLS link is maintained
 *     during scan.
 *  1: Maintain TDLS link and allow scan even if peer is not buffer STA
 *     capable and device is not sleep STA capable. There will be loss of
 *     Rx pkts since peer would not know when device moves away from tdls
 *     channel. Tx on TDLS link would stop when device moves away from tdls
 *     channel.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_SCAN_ENABLE                       "gEnableTDLSScan"
#define CFG_TDLS_SCAN_ENABLE_MIN                   (0)
#define CFG_TDLS_SCAN_ENABLE_MAX                   (1)
#define CFG_TDLS_SCAN_ENABLE_DEFAULT               (1)

/*
 * <ini>
 * gTDLSPeerKickoutThreshold - TDLS peer kickout threshold to firmware.
 * @Min: 10
 * @Max: 5000
 * @Default: 96
 *
 * This ini is used to configure TDLS peer kickout threshold to firmware.
 *     Firmware will use this value to determine, when to send TDLS
 *     peer kick out event to host.
 *     E.g.
 *        if peer kick out threshold is 10, then firmware will wait for 10
 *        consecutive packet failures and then send TDLS kickout
 *        notification to host driver
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PEER_KICKOUT_THRESHOLD            "gTDLSPeerKickoutThreshold"
#define CFG_TDLS_PEER_KICKOUT_THRESHOLD_MIN        (10)
#define CFG_TDLS_PEER_KICKOUT_THRESHOLD_MAX        (5000)
#define CFG_TDLS_PEER_KICKOUT_THRESHOLD_DEFAULT    (96)

/*
 * <ini>
 * gTDLSDiscoveryWakeTimeout - TDLS discovery WAKE timeout in ms.
 * @Min: 10
 * @Max: 5000
 * @Default: 96
 *
 * DUT will wake until this timeout to receive TDLS discovery response
 * from peer. If tdls_discovery_wake_timeout is 0x0, the DUT will
 * choose autonomously what wake timeout value to use.
 *
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TDLS_DISCOVERY_WAKE_TIMEOUT            "gTDLSDiscoveryWakeTimeout"
#define CFG_TDLS_DISCOVERY_WAKE_TIMEOUT_MIN        (0)
#define CFG_TDLS_DISCOVERY_WAKE_TIMEOUT_MAX        (2000)
#define CFG_TDLS_DISCOVERY_WAKE_TIMEOUT_DEFAULT    (200)

#endif

/*
 * <ini>
 * gTDLSEnableDeferTime - Timer to defer for enabling TDLS on P2P listen.
 * @Min: 500
 * @Max: 6000
 * @Default: 2000
 *
 * This ini is used to set the timer to defer for enabling TDLS on P2P
 * listen (value in milliseconds).
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_ENABLE_DEFER_TIMER                "gTDLSEnableDeferTime"
#define CFG_TDLS_ENABLE_DEFER_TIMER_MIN            (500)
#define CFG_TDLS_ENABLE_DEFER_TIMER_MAX            (6000)
#define CFG_TDLS_ENABLE_DEFER_TIMER_DEFAULT        (2000)

/*
 * <ini>
 * gTxLdpcEnable - Config Param to enable Tx LDPC capability
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to enable/disable Tx LDPC capability
 * 0 - disable
 * 1 - HT LDPC enable
 * 2 - VHT LDPC enable
 * 3 - HT & VHT LDPC enable
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Concurrency/Standalone
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TX_LDPC_ENABLE_FEATURE         "gTxLdpcEnable"
#define CFG_TX_LDPC_ENABLE_FEATURE_MIN     (0)
#define CFG_TX_LDPC_ENABLE_FEATURE_MAX     (3)
#define CFG_TX_LDPC_ENABLE_FEATURE_DEFAULT (3)

/*
 * <ini>
 * gEnableRXLDPC - Config Param to enable Rx LDPC capability
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable Rx LDPC capability
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Concurrency/Standalone
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_RX_LDPC                       "gEnableRXLDPC"
#define CFG_ENABLE_RX_LDPC_MIN                   (0)
#define CFG_ENABLE_RX_LDPC_MAX                   (1)
#define CFG_ENABLE_RX_LDPC_DEFAULT               (0)

/*
 * <ini>
 * gEnableMCCAdaptiveScheduler - MCC Adaptive Scheduler feature.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable MCC Adaptive Scheduler feature.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_NAME             "gEnableMCCAdaptiveScheduler"
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_MIN              (0)
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_MAX              (1)
#define CFG_ENABLE_MCC_ADATIVE_SCHEDULER_ENABLED_DEFAULT          (1)

#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE         "gTxBFEnable"
#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_MIN     (WNI_CFG_VHT_SU_BEAMFORMEE_CAP_STAMIN)
#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_MAX     (WNI_CFG_VHT_SU_BEAMFORMEE_CAP_STAMAX)
#define CFG_VHT_SU_BEAMFORMEE_CAP_FEATURE_DEFAULT (WNI_CFG_VHT_SU_BEAMFORMEE_CAP_STADEF)

/*
 * <ini>
 * enable_subfee_vendor_vhtie - ini to enable/disable SU Bformee in vendor VHTIE
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable SU Bformee in vendor vht ie if gTxBFEnable
 * is enabled. if gTxBFEnable is 0 this will not have any effect.
 *
 * Related: gTxBFEnable.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SUBFEE_IN_VENDOR_VHTIE_NAME    "enable_subfee_vendor_vhtie"
#define CFG_ENABLE_SUBFEE_IN_VENDOR_VHTIE_MIN     (0)
#define CFG_ENABLE_SUBFEE_IN_VENDOR_VHTIE_MAX     (1)
#define CFG_ENABLE_SUBFEE_IN_VENDOR_VHTIE_DEFAULT (1)

/*
 * Enable / Disable Tx beamformee in SAP mode
 * Default: Disable
 */
#define CFG_VHT_ENABLE_TXBF_SAP_MODE         "gEnableTxBFeeSAP"
#define CFG_VHT_ENABLE_TXBF_SAP_MODE_MIN     (0)
#define CFG_VHT_ENABLE_TXBF_SAP_MODE_MAX     (1)
#define CFG_VHT_ENABLE_TXBF_SAP_MODE_DEFAULT (0)

#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED         "gTxBFCsnValue"
#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_MIN     (WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_STAMIN)
#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_MAX     (WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_STAMAX - 1)
#define CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_DEFAULT (WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_STADEF - 1)

#define CFG_VHT_ENABLE_TXBF_IN_20MHZ               "gEnableTxBFin20MHz"
#define CFG_VHT_ENABLE_TXBF_IN_20MHZ_MIN           (0)
#define CFG_VHT_ENABLE_TXBF_IN_20MHZ_MAX           (1)
#define CFG_VHT_ENABLE_TXBF_IN_20MHZ_DEFAULT       (0)

#define CFG_VHT_ENABLE_TX_SU_BEAM_FORMER         "gEnableTxSUBeamformer"
#define CFG_VHT_ENABLE_TX_SU_BEAM_FORMER_MIN     (0)
#define CFG_VHT_ENABLE_TX_SU_BEAM_FORMER_MAX     (1)
#define CFG_VHT_ENABLE_TX_SU_BEAM_FORMER_DEFAULT (0)

/* Enable debug for remain on channel issues */
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_NAME    "gDebugP2pRemainOnChannel"
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_DEFAULT (0)
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_MIN     (0)
#define CFG_DEBUG_P2P_REMAIN_ON_CHANNEL_MAX     (1)

/*
 * SAP ALLOW All Channels
 */
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_NAME          "gSapAllowAllChannel"
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_MIN           (0)
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_MAX           (1)
#define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM_DEFAULT       (0)

#define CFG_DISABLE_LDPC_WITH_TXBF_AP             "gDisableLDPCWithTxbfAP"
#define CFG_DISABLE_LDPC_WITH_TXBF_AP_MIN         (0)
#define CFG_DISABLE_LDPC_WITH_TXBF_AP_MAX         (1)
#define CFG_DISABLE_LDPC_WITH_TXBF_AP_DEFAULT     (0)

/* Parameter to control VHT support in 2.4 GHz band */
#define CFG_ENABLE_VHT_FOR_24GHZ_NAME             "gEnableVhtFor24GHzBand"
#define CFG_ENABLE_VHT_FOR_24GHZ_MIN              (0)
#define CFG_ENABLE_VHT_FOR_24GHZ_MAX              (1)
#define CFG_ENABLE_VHT_FOR_24GHZ_DEFAULT          (0)

/*
 * gEnableVendorVhtFor24GHzBand:Parameter to control VHT support
 * based on vendor ie in 2.4 GHz band
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This parameter will enable SAP to read VHT capability in vendor ie in Assoc
 * Req and send VHT caps in Resp to establish connection in VHT Mode.
 * Supported Feature: SAP
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_VENDOR_VHT_FOR_24GHZ_NAME      "gEnableVendorVhtFor24GHzBand"
#define CFG_ENABLE_VENDOR_VHT_FOR_24GHZ_MIN       (0)
#define CFG_ENABLE_VENDOR_VHT_FOR_24GHZ_MAX       (1)
#define CFG_ENABLE_VENDOR_VHT_FOR_24GHZ_DEFAULT   (1)

/*
 * <ini>
 * gEnableSNRMonitoring - Enables SNR Monitoring
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default snr monitor
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_SNR_MONITORING_NAME              "gEnableSNRMonitoring"
#define CFG_ENABLE_SNR_MONITORING_MIN               (0)
#define CFG_ENABLE_SNR_MONITORING_MAX               (1)
#define CFG_ENABLE_SNR_MONITORING_DEFAULT           (0)

/*
 * <ini>
 * gEnableIpTcpUdpChecksumOffload - It enables IP, TCP and UDP checksum
 * offload in hardware
 * @Min: 0
 * @Max: 1
 * @Default: DEF
 *
 * This ini is used to enable IP, TCP and UDP checksum offload in hardware
 * and also advertise same to network stack
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_IP_TCP_UDP_CHKSUM_OFFLOAD            "gEnableIpTcpUdpChecksumOffload"
#define CFG_ENABLE_IP_TCP_UDP_CHKSUM_OFFLOAD_DISABLE    (0)
#define CFG_ENABLE_IP_TCP_UDP_CHKSUM_OFFLOAD_ENABLE     (1)
#define CFG_ENABLE_IP_TCP_UDP_CHKSUM_OFFLOAD_DEFAULT    (CFG_ENABLE_IP_TCP_UDP_CHKSUM_OFFLOAD_ENABLE)

#ifdef WLAN_FEATURE_FASTPATH

/*
 * <ini>
 * gEnableFastPath - Control to enable fastpath feature
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable fastpath feature
 *
 * Supported Feature: Wlan Fastpath Feature
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_FASTPATH                      "gEnableFastPath"
#define CFG_ENABLE_FASTPATH_MIN                  (0)
#define CFG_ENABLE_FASTPATH_MAX                  (1)
#define CFG_ENABLE_FASTPATH_DEFAULT              (CFG_ENABLE_FASTPATH_MIN)
#endif /* WLAN_FEATURE_FASTPATH */

/*
 * IPA Offload configuration - Each bit enables a feature
 * bit0 - IPA Enable
 * bit1 - IPA Pre filter enable
 * bit2 - IPv6 enable
 * bit3 - IPA Resource Manager (RM) enable
 * bit4 - IPA Clock scaling enable
 */
#define CFG_IPA_OFFLOAD_CONFIG_NAME              "gIPAConfig"
#define CFG_IPA_OFFLOAD_CONFIG_MIN               (0)
#define CFG_IPA_OFFLOAD_CONFIG_MAX               (0xFFFFFFFF)
#define CFG_IPA_OFFLOAD_CONFIG_DEFAULT           (CFG_IPA_OFFLOAD_CONFIG_MIN)

/*
 * IPA DESC SIZE
 */
#define CFG_IPA_DESC_SIZE_NAME                   "gIPADescSize"
#define CFG_IPA_DESC_SIZE_MIN                    (800)
#define CFG_IPA_DESC_SIZE_MAX                    (8000)
#define CFG_IPA_DESC_SIZE_DEFAULT                (800)

#define CFG_IPA_HIGH_BANDWIDTH_MBPS              "gIPAHighBandwidthMbps"
#define CFG_IPA_HIGH_BANDWIDTH_MBPS_MIN          (200)
#define CFG_IPA_HIGH_BANDWIDTH_MBPS_MAX          (1000)
#define CFG_IPA_HIGH_BANDWIDTH_MBPS_DEFAULT      (400)

#define CFG_IPA_MEDIUM_BANDWIDTH_MBPS            "gIPAMediumBandwidthMbps"
#define CFG_IPA_MEDIUM_BANDWIDTH_MBPS_MIN        (100)
#define CFG_IPA_MEDIUM_BANDWIDTH_MBPS_MAX        (400)
#define CFG_IPA_MEDIUM_BANDWIDTH_MBPS_DEFAULT    (200)

#define CFG_IPA_LOW_BANDWIDTH_MBPS               "gIPALowBandwidthMbps"
#define CFG_IPA_LOW_BANDWIDTH_MBPS_MIN           (0)
#define CFG_IPA_LOW_BANDWIDTH_MBPS_MAX           (100)
#define CFG_IPA_LOW_BANDWIDTH_MBPS_DEFAULT       (100)

#define CFG_IPA_FORCE_VOTING_ENABLE              "gIPAForceVotingEnable"
#define CFG_IPA_FORCE_VOTING_ENABLE_MIN          (0)
#define CFG_IPA_FORCE_VOTING_ENABLE_MAX          (1)
#define CFG_IPA_FORCE_VOTING_ENABLE_DEFAULT      (0)

/*
 * Firmware uart print
 */
#define CFG_ENABLE_FW_UART_PRINT_NAME             "gEnablefwprint"
#define CFG_ENABLE_FW_UART_PRINT_DISABLE          (0)
#define CFG_ENABLE_FW_UART_PRINT_ENABLE           (1)
#define CFG_ENABLE_FW_UART_PRINT_DEFAULT          (CFG_ENABLE_FW_UART_PRINT_DISABLE)

/*
 * Firmware log
 */
#define CFG_ENABLE_FW_LOG_NAME                   "gEnablefwlog"
#define CFG_ENABLE_FW_LOG_DISABLE                (0)
#define CFG_ENABLE_FW_LOG_WMI                    (1)
#define CFG_ENABLE_FW_LOG_DIAG                   (2)
#define CFG_ENABLE_FW_LOG_MIN                    (CFG_ENABLE_FW_LOG_DISABLE)
#define CFG_ENABLE_FW_LOG_MAX                    (CFG_ENABLE_FW_LOG_DIAG)
#define CFG_ENABLE_FW_LOG_DEFAULT                (CFG_ENABLE_FW_LOG_WMI)

/* Macro to handle maximum receive AMPDU size configuration */
#define CFG_VHT_AMPDU_LEN_EXPONENT_NAME                "gVhtAmpduLenExponent"
#define CFG_VHT_AMPDU_LEN_EXPONENT_MIN                 (0)
#define CFG_VHT_AMPDU_LEN_EXPONENT_MAX                 (7)
#define CFG_VHT_AMPDU_LEN_EXPONENT_DEFAULT             (3)

#define CFG_VHT_MPDU_LEN_NAME                          "gVhtMpduLen"
#define CFG_VHT_MPDU_LEN_MIN                           (0)
#define CFG_VHT_MPDU_LEN_MAX                           (2)
#define CFG_VHT_MPDU_LEN_DEFAULT                       (0)

#define CFG_SAP_MAX_NO_PEERS                       "gSoftApMaxPeers"
#define CFG_SAP_MAX_NO_PEERS_MIN                   (1)
#define CFG_SAP_MAX_NO_PEERS_MAX                   (SIR_SAP_MAX_NUM_PEERS)
#define CFG_SAP_MAX_NO_PEERS_DEFAULT               (SIR_SAP_MAX_NUM_PEERS)

/*
 * <ini>
 * gEnableDebugLog - Enable/Disable the Connection related logs
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0x0F
 *
 * This ini is used to enable/disable the connection related logs
 * 0x1 - Enable mgmt pkt logs (excpet probe req/rsp, beacons).
 * 0x2 - Enable EAPOL pkt logs.
 * 0x4 - Enable DHCP pkt logs.
 * 0x8 - Enable mgmt action frames logs.
 * 0x0 - Disable all the above connection related logs.
 * The default value of 0x0F will enable all the above logs
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_DEBUG_CONNECT_ISSUE             "gEnableDebugLog"
#define CFG_ENABLE_DEBUG_CONNECT_ISSUE_MIN         (0)
#define CFG_ENABLE_DEBUG_CONNECT_ISSUE_MAX         (0xFF)
#define CFG_ENABLE_DEBUG_CONNECT_ISSUE_DEFAULT     (0x0F)

/* SAR Thermal limit values for 2g and 5g */

#define CFG_SET_TXPOWER_LIMIT2G_NAME               "TxPower2g"
#define CFG_SET_TXPOWER_LIMIT2G_MIN                (0)
#define CFG_SET_TXPOWER_LIMIT2G_MAX                (30)
#define CFG_SET_TXPOWER_LIMIT2G_DEFAULT            (30)

#define CFG_SET_TXPOWER_LIMIT5G_NAME               "TxPower5g"
#define CFG_SET_TXPOWER_LIMIT5G_MIN                (0)
#define CFG_SET_TXPOWER_LIMIT5G_MAX                (30)
#define CFG_SET_TXPOWER_LIMIT5G_DEFAULT            (30)

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL

/*
 * <ini>
 * TxFlowLowWaterMark - Low watermark for pausing network queues
 *
 * @Min: 0
 * @Max: 1000
 * @Default: 300
 *
 * This ini specifies the low watermark of data packets transmitted
 * before pausing netif queues in tx flow path. It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowHighWaterMarkOffset, TxFlowMaxQueueDepth,
 *          TxLbwFlowLowWaterMark, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_FLOW_LWM                         "TxFlowLowWaterMark"
#define CFG_LL_TX_FLOW_LWM_MIN                     (0)
#define CFG_LL_TX_FLOW_LWM_MAX                     (1000)
#define CFG_LL_TX_FLOW_LWM_DEFAULT                 (300)

/*
 * <ini>
 * TxFlowHighWaterMarkOffset - High Watermark offset to unpause Netif queues
 * @Min: 0
 * @Max: 300
 * @Default: 94
 *
 * This ini specifies the offset to upause the netif queues
 * when they are paused due to insufficient descriptors as guided by
 * ini TxFlowLowWaterMark. It is only applicable where legacy flow control
 * is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowMaxQueueDepth,
 *          TxLbwFlowLowWaterMark, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_FLOW_HWM_OFFSET                  "TxFlowHighWaterMarkOffset"
#define CFG_LL_TX_FLOW_HWM_OFFSET_MIN              (0)
#define CFG_LL_TX_FLOW_HWM_OFFSET_MAX              (300)
#define CFG_LL_TX_FLOW_HWM_OFFSET_DEFAULT          (94)

/*
 * <ini>
 * TxFlowMaxQueueDepth - Max pause queue depth.
 *
 * @Min: 400
 * @Max: 3500
 * @Default: 1500
 *
 * This ini specifies the max queue pause depth.It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxLbwFlowLowWaterMark, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_FLOW_MAX_Q_DEPTH                 "TxFlowMaxQueueDepth"
#define CFG_LL_TX_FLOW_MAX_Q_DEPTH_MIN             (400)
#define CFG_LL_TX_FLOW_MAX_Q_DEPTH_MAX             (3500)
#define CFG_LL_TX_FLOW_MAX_Q_DEPTH_DEFAULT         (1500)

/*
 * <ini>
 * TxLbwFlowLowWaterMark - Low watermark for pausing network queues
 *                         in low bandwidth band
 * @Min: 0
 * @Max: 1000
 * @Default: 450
 *
 * This ini specifies the low watermark of data packets transmitted
 * before pausing netif queues in tx flow path in low bandwidth band.
 * It is only applicable where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowHighWaterMarkOffset,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_LBW_FLOW_LWM                     "TxLbwFlowLowWaterMark"
#define CFG_LL_TX_LBW_FLOW_LWM_MIN                 (0)
#define CFG_LL_TX_LBW_FLOW_LWM_MAX                 (1000)
#define CFG_LL_TX_LBW_FLOW_LWM_DEFAULT             (450)

/*
 * <ini>
 * TxLbwFlowHighWaterMarkOffset - High Watermark offset to unpause Netif queues
 *                                in low bandwidth band.
 * @Min: 0
 * @Max: 300
 * @Default: 50
 *
 * This ini specifies the offset to upause the netif queues
 * when they are paused due to insufficient descriptors as guided by
 * ini TxLbwFlowLowWaterMark in low bandwidth band. It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowMaxQueueDepth, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_LBW_FLOW_HWM_OFFSET              "TxLbwFlowHighWaterMarkOffset"
#define CFG_LL_TX_LBW_FLOW_HWM_OFFSET_MIN          (0)
#define CFG_LL_TX_LBW_FLOW_HWM_OFFSET_MAX          (300)
#define CFG_LL_TX_LBW_FLOW_HWM_OFFSET_DEFAULT      (50)

/*
 * <ini>
 * TxLbwFlowMaxQueueDepth - Max pause queue depth in low bandwidth band
 *
 * @Min: 400
 * @Max: 3500
 * @Default: 750
 *
 * This ini specifies the max queue pause depth in low bandwidth band.
 * It is only applicable where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxHbwFlowLowWaterMark,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_LBW_FLOW_MAX_Q_DEPTH             "TxLbwFlowMaxQueueDepth"
#define CFG_LL_TX_LBW_FLOW_MAX_Q_DEPTH_MIN         (400)
#define CFG_LL_TX_LBW_FLOW_MAX_Q_DEPTH_MAX         (3500)
#define CFG_LL_TX_LBW_FLOW_MAX_Q_DEPTH_DEFAULT     (750)

/*
 * <ini>
 * TxHbwFlowLowWaterMark - Low watermark for pausing network queues
 *                         in high bandwidth band
 * @Min: 0
 * @Max: 1000
 * @Default: 406
 *
 * This ini specifies the threshold of data packets transmitted
 * before pausing netif queues.It is only applicable where
 * legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxLbwFlowMaxQueueDepth,
 *          TxHbwFlowHighWaterMarkOffset, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_HBW_FLOW_LWM                     "TxHbwFlowLowWaterMark"
#define CFG_LL_TX_HBW_FLOW_LWM_MIN                 (0)
#define CFG_LL_TX_HBW_FLOW_LWM_MAX                 (1000)
#define CFG_LL_TX_HBW_FLOW_LWM_DEFAULT             (406)

/*
 * <ini>
 * TxHbwFlowHighWaterMarkOffset - High Watermark offset to unpause Netif queues
 *                                in high bandwidth band.
 * @Min: 0
 * @Max: 300
 * @Default: 94
 *
 * This ini specifies the offset to upause the netif queues
 * when they are paused due to insufficient descriptors as guided by
 * ini TxHbwFlowLowWaterMark in high bandwidth band. It is only applicable
 * where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxLbwFlowMaxQueueDepth,
 *          TxHbwFlowLowWaterMark, TxHbwFlowMaxQueueDepth
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_HBW_FLOW_HWM_OFFSET              "TxHbwFlowHighWaterMarkOffset"
#define CFG_LL_TX_HBW_FLOW_HWM_OFFSET_MIN          (0)
#define CFG_LL_TX_HBW_FLOW_HWM_OFFSET_MAX          (300)
#define CFG_LL_TX_HBW_FLOW_HWM_OFFSET_DEFAULT      (94)

/*
 * <ini>
 * TxHbwFlowMaxQueueDepth - Max pause queue depth in high bandwidth band
 * @Min: 4000
 * @Max: 3500
 * @Default: 1500
 *
 * This ini specifies the max queue pause depth in high bandwidth band.
 * It is only applicable where legacy flow control is used i.e.for Rome.
 *
 * Related: TxFlowLowWaterMark, TxFlowHighWaterMarkOffset,
 *          TxFlowMaxQueueDepth, TxLbwFlowLowWaterMark,
 *          TxLbwFlowHighWaterMarkOffset, TxLbwFlowMaxQueueDepth,
 *          TxHbwFlowLowWaterMark, TxHbwFlowHighWaterMarkOffset
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_HBW_FLOW_MAX_Q_DEPTH             "TxHbwFlowMaxQueueDepth"
#define CFG_LL_TX_HBW_FLOW_MAX_Q_DEPTH_MIN         (400)
#define CFG_LL_TX_HBW_FLOW_MAX_Q_DEPTH_MAX         (3500)
#define CFG_LL_TX_HBW_FLOW_MAX_Q_DEPTH_DEFAULT     (1500)
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */


/*
 * <ini>
 * TxFlowStopQueueThreshold - Stop queue Threshold to pause
 *                            Netif queues when it reaches
 * @Min: 0
 * @Max: 50
 * @Default: 15
 *
 * This ini specifies the threshold of data packets transmitted
 * before pausing netif queues.
 *
 * Related: TxFlowStartQueueOffset
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_FLOW_STOP_QUEUE_TH               "TxFlowStopQueueThreshold"
#define CFG_LL_TX_FLOW_STOP_QUEUE_TH_DEFAULT       (15)
#define CFG_LL_TX_FLOW_STOP_QUEUE_TH_MIN           (0)
#define CFG_LL_TX_FLOW_STOP_QUEUE_TH_MAX           (50)

/*
 * <ini>
 * TxFlowStartQueueOffset - Start queue offset to unpause
 *                          Netif queues
 * @Min: 0
 * @Max: 30
 * @Default: 11
 *
 * This ini specifies the offset to upause the netif queues
 * when they are paused due to insufficient descriptors as guided by
 * ini TxFlowStopQueueThreshold.
 *
 * Related: TxFlowStopQueueThreshold
 *
 * Supported Feature: Dynamic Flow Control
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LL_TX_FLOW_START_QUEUE_OFFSET          "TxFlowStartQueueOffset"
#define CFG_LL_TX_FLOW_START_QUEUE_OFFSET_DEFAULT  (10)
#define CFG_LL_TX_FLOW_START_QUEUE_OFFSET_MIN      (0)
#define CFG_LL_TX_FLOW_START_QUEUE_OFFSET_MAX      (30)


#define CFG_SAP_MAX_OFFLOAD_PEERS                  "gMaxOffloadPeers"
#define CFG_SAP_MAX_OFFLOAD_PEERS_MIN              (2)
#define CFG_SAP_MAX_OFFLOAD_PEERS_MAX              (5)
#define CFG_SAP_MAX_OFFLOAD_PEERS_DEFAULT          (2)

#define CFG_SAP_MAX_OFFLOAD_REORDER_BUFFS          "gMaxOffloadReorderBuffs"
#define CFG_SAP_MAX_OFFLOAD_REORDER_BUFFS_MIN      (0)
#define CFG_SAP_MAX_OFFLOAD_REORDER_BUFFS_MAX      (3)
#define CFG_SAP_MAX_OFFLOAD_REORDER_BUFFS_DEFAULT  (2)

#ifdef FEATURE_WLAN_RA_FILTERING
#define CFG_RA_FILTER_ENABLE_NAME                  "gRAFilterEnable"
#define CFG_RA_FILTER_ENABLE_MIN                   (0)
#define CFG_RA_FILTER_ENABLE_MAX                   (1)
#define CFG_RA_FILTER_ENABLE_DEFAULT               (1)

#define CFG_RA_RATE_LIMIT_INTERVAL_NAME            "gRArateLimitInterval"
#define CFG_RA_RATE_LIMIT_INTERVAL_MIN             (60)
#define CFG_RA_RATE_LIMIT_INTERVAL_MAX             (3600)
#define CFG_RA_RATE_LIMIT_INTERVAL_DEFAULT         (60) /*60 SEC */
#endif

/*
 * <ini>
 * gIgnorePeerErpInfo - Used for ignore peer infrormation
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to ignore default peer info
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_IGNORE_PEER_ERP_INFO_NAME      "gIgnorePeerErpInfo"
#define CFG_IGNORE_PEER_ERP_INFO_MIN       (0)
#define CFG_IGNORE_PEER_ERP_INFO_MAX       (1)
#define CFG_IGNORE_PEER_ERP_INFO_DEFAULT   (0)

/*
 * <ini>
 * gEnableMemoryDebug - Enables the memory debug
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable default memory debug
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#ifdef MEMORY_DEBUG
#define CFG_ENABLE_MEMORY_DEBUG_NAME             "gEnableMemoryDebug"
#define CFG_ENABLE_MEMORY_DEBUG_MIN              (0)
#define CFG_ENABLE_MEMORY_DEBUG_MAX              (1)
#define CFG_ENABLE_MEMORY_DEBUG_DEFAULT          (1)
#endif

/*
 * <ini>
 * g_auto_detect_power_failure_mode - auto detect power save failure mode
 * @Min: PMO_FW_TO_CRASH_ON_PWR_FAILURE
 * @Max: PMO_AUTO_PWR_FAILURE_DETECT_DISABLE
 * @Default: PMO_FW_TO_CRASH_ON_PWR_FAILURE
 *
 * This ini specifies the behavior of FW in case of
 * CHIP_POWER_SAVE_FAIL_DETECTED event
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_AUTO_DETECT_POWER_FAIL_MODE_NAME    "g_auto_detect_power_failure_mode"
#define CFG_AUTO_DETECT_POWER_FAIL_MODE_DEFAULT (PMO_FW_TO_CRASH_ON_PWR_FAILURE)
#define CFG_AUTO_DETECT_POWER_FAIL_MODE_MIN     (PMO_FW_TO_CRASH_ON_PWR_FAILURE)
#define CFG_AUTO_DETECT_POWER_FAIL_MODE_MAX \
					(PMO_AUTO_PWR_FAILURE_DETECT_DISABLE)
/*
 * <ini>
 * gMaxAmsduNum - Max number of MSDU's in aggregate
 * @Min: 0
 * @Max: 3
 * @Default: 1
 * gMaxAmsduNum is the number of MSDU's transmitted in the 11n aggregate
 * frame. Setting it to a value larger than 1 enables transmit aggregation.
 * It is a PHY parameter that applies to all vdev's in firmware.
 *
 * Supported Feature: 11n aggregation
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MAX_AMSDU_NUM_NAME                "gMaxAmsduNum"
#define CFG_MAX_AMSDU_NUM_MIN                 (0)
#define CFG_MAX_AMSDU_NUM_MAX                 (3)
#define CFG_MAX_AMSDU_NUM_DEFAULT             (1)

/*
 * <ini>
 * gInitialDwellTime - Used to set initial dwell time
 * @Min: 0
 * @Max: 0
 * @Default: 100
 *
 * This ini is used to set default initial dwell time
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_INITIAL_DWELL_TIME_NAME            "gInitialDwellTime"
#define CFG_INITIAL_DWELL_TIME_DEFAULT         (0)
#define CFG_INITIAL_DWELL_TIME_MIN             (0)
#define CFG_INITIAL_DWELL_TIME_MAX             (100)

/*
 * <ini>
 * gInitialScanNoDFSChnl - WLAN skips scanning the DFS channels
 * @Min: 0
 * @Max: 0
 * @Default: 1
 *
 * This ini is used to set for the first scan after driver
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_INITIAL_SCAN_NO_DFS_CHNL_NAME         "gInitialScanNoDFSChnl"
#define CFG_INITIAL_SCAN_NO_DFS_CHNL_DEFAULT      (0)
#define CFG_INITIAL_SCAN_NO_DFS_CHNL_MIN          (0)
#define CFG_INITIAL_SCAN_NO_DFS_CHNL_MAX          (1)

/*
 * <ini>
 * gAllowDFSChannelRoam - Allow dfs channel in roam
 * @Min: 0
 * @Max: 1
 * @Default: 2
 *
 * This ini is used to set default dfs channel
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ROAMING_DFS_CHANNEL_NAME               "gAllowDFSChannelRoam"
#define CFG_ROAMING_DFS_CHANNEL_DISABLED           (0)
#define CFG_ROAMING_DFS_CHANNEL_ENABLED_NORMAL     (1)
#define CFG_ROAMING_DFS_CHANNEL_ENABLED_ACTIVE     (2)
#define CFG_ROAMING_DFS_CHANNEL_MIN                (CFG_ROAMING_DFS_CHANNEL_DISABLED)
#define CFG_ROAMING_DFS_CHANNEL_MAX                (CFG_ROAMING_DFS_CHANNEL_ENABLED_ACTIVE)
#define CFG_ROAMING_DFS_CHANNEL_DEFAULT            (CFG_ROAMING_DFS_CHANNEL_DISABLED)

#ifdef MSM_PLATFORM
#define CFG_BUS_BANDWIDTH_HIGH_THRESHOLD           "gBusBandwidthHighThreshold"
#define CFG_BUS_BANDWIDTH_HIGH_THRESHOLD_DEFAULT   (2000)
#define CFG_BUS_BANDWIDTH_HIGH_THRESHOLD_MIN       (0)
#define CFG_BUS_BANDWIDTH_HIGH_THRESHOLD_MAX       (4294967295UL)

#define CFG_BUS_BANDWIDTH_MEDIUM_THRESHOLD         "gBusBandwidthMediumThreshold"
#define CFG_BUS_BANDWIDTH_MEDIUM_THRESHOLD_DEFAULT (500)
#define CFG_BUS_BANDWIDTH_MEDIUM_THRESHOLD_MIN     (0)
#define CFG_BUS_BANDWIDTH_MEDIUM_THRESHOLD_MAX     (4294967295UL)

#define CFG_BUS_BANDWIDTH_LOW_THRESHOLD            "gBusBandwidthLowThreshold"
#define CFG_BUS_BANDWIDTH_LOW_THRESHOLD_DEFAULT    (150)
#define CFG_BUS_BANDWIDTH_LOW_THRESHOLD_MIN        (0)
#define CFG_BUS_BANDWIDTH_LOW_THRESHOLD_MAX        (4294967295UL)

#define CFG_BUS_BANDWIDTH_COMPUTE_INTERVAL         "gBusBandwidthComputeInterval"
#define CFG_BUS_BANDWIDTH_COMPUTE_INTERVAL_DEFAULT (100)
#define CFG_BUS_BANDWIDTH_COMPUTE_INTERVAL_MIN     (0)
#define CFG_BUS_BANDWIDTH_COMPUTE_INTERVAL_MAX     (10000)

/*
 * <ini>
 * gEnableTcpLimitOutput - Control to enable TCP limit output byte
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable dynamic configuration of TCP limit output bytes
 * tcp_limit_output_bytes param. Enabling this will let driver post message to
 * cnss-daemon, accordingly cnss-daemon will modify the tcp_limit_output_bytes.
 *
 * Supported Feature: Tcp limit output bytes
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_TCP_LIMIT_OUTPUT                      "gTcpLimitOutputEnable"
#define CFG_ENABLE_TCP_LIMIT_OUTPUT_DEFAULT              (1)
#define CFG_ENABLE_TCP_LIMIT_OUTPUT_MIN                  (0)
#define CFG_ENABLE_TCP_LIMIT_OUTPUT_MAX                  (1)

/*
 * <ini>
 * gTcpAdvWinScaleEnable - Control to enable  TCP adv window scaling
 * @Min: -0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable dynamic configuration of TCP adv window scaling system parameter.
 *
 * Supported Feature: Tcp Advance Window Scaling
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_TCP_ADV_WIN_SCALE                      "gTcpAdvWinScaleEnable"
#define CFG_ENABLE_TCP_ADV_WIN_SCALE_DEFAULT              (1)
#define CFG_ENABLE_TCP_ADV_WIN_SCALE_MIN                  (0)
#define CFG_ENABLE_TCP_ADV_WIN_SCALE_MAX                  (1)

/*
 * <ini>
 * gTcpDelAckEnable - Control to enable Dynamic Configuration of Tcp Delayed Ack
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable Dynamic Configuration of Tcp Delayed Ack
 *
 * Related: gTcpDelAckThresholdHigh, gTcpDelAckThresholdLow,
 *          gTcpDelAckTimerCount
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_TCP_DELACK                      "gTcpDelAckEnable"
#define CFG_ENABLE_TCP_DELACK_DEFAULT              (1)
#define CFG_ENABLE_TCP_DELACK_MIN                  (0)
#define CFG_ENABLE_TCP_DELACK_MAX                  (1)

/*
 * <ini>
 * enable_tcp_param_update - Configure TCP param through Wi-Fi HAL
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to configure TCP param through Wi-Fi HAL
 *
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_TCP_PARAM_UPDATE                      "enable_tcp_param_update"
#define CFG_ENABLE_TCP_PARAM_UPDATE_DEFAULT              (0)
#define CFG_ENABLE_TCP_PARAM_UPDATE_MIN                  (0)
#define CFG_ENABLE_TCP_PARAM_UPDATE_MAX                  (1)


/*
 * <ini>
 * gTcpDelAckThresholdHigh - High Threshold inorder to trigger TCP Del Ack
 *                                          indication
 * @Min: 0
 * @Max: 16000
 * @Default: 500
 *
 * This ini is used to mention the High Threshold inorder to trigger TCP Del Ack
 * indication i.e the threshold of packets received over a period of 100 ms.
 * i.e to have a low RX throughput requirement
 * Related: gTcpDelAckEnable, gTcpDelAckThresholdLow, gTcpDelAckTimerCount
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TCP_DELACK_THRESHOLD_HIGH              "gTcpDelAckThresholdHigh"
#define CFG_TCP_DELACK_THRESHOLD_HIGH_DEFAULT      (500)
#define CFG_TCP_DELACK_THRESHOLD_HIGH_MIN          (0)
#define CFG_TCP_DELACK_THRESHOLD_HIGH_MAX          (16000)

/*
 * <ini>
 * gTcpDelAckThresholdLow - Low Threshold inorder to trigger TCP Del Ack
 *                                          indication
 * @Min: 0
 * @Max: 10000
 * @Default: 1000
 *
 * This ini is used to mention the Low Threshold inorder to trigger TCP Del Ack
 * indication i.e the threshold of packets received over a period of 100 ms.
 * i.e to have a low RX throughput requirement
 *
 * Related: gTcpDelAckEnable, gTcpDelAckThresholdHigh, gTcpDelAckTimerCount
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TCP_DELACK_THRESHOLD_LOW               "gTcpDelAckThresholdLow"
#define CFG_TCP_DELACK_THRESHOLD_LOW_DEFAULT       (1000)
#define CFG_TCP_DELACK_THRESHOLD_LOW_MIN           (0)
#define CFG_TCP_DELACK_THRESHOLD_LOW_MAX           (10000)

/*
 * <ini>
 * gTcpDelAckTimerCount - Del Ack Timer Count  inorder to trigger TCP Del Ack
 *                                      indication
 * @Min: 1
 * @Max: 1000
 * @Default: 30
 *
 * This ini is used to mention the Del Ack Timer Count inorder to
 * trigger TCP Del Ack indication i.e number of 100 ms periods
 *
 * Related: gTcpDelAckEnable, gTcpDelAckThresholdHigh, gTcpDelAckThresholdLow
 *
 * Supported Feature: Tcp Delayed Ack
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TCP_DELACK_TIMER_COUNT                 "gTcpDelAckTimerCount"
#define CFG_TCP_DELACK_TIMER_COUNT_DEFAULT         (30)
#define CFG_TCP_DELACK_TIMER_COUNT_MIN             (1)
#define CFG_TCP_DELACK_TIMER_COUNT_MAX             (1000)


/*
 * <ini>
 * gTcpTxHighTputThreshold - High Threshold inorder to trigger High
 *                                          Tx Throughput requirement.
 * @Min: 0
 * @Max: 16000
 * @Default: 500
 *
 * This ini specifies the threshold of packets transmitted
 * over a period of 100 ms beyond which TCP can be considered to have a high
 * TX throughput requirement. The driver uses this condition to tweak TCP TX
 * specific parameters (via cnss-daemon)
 *
 * Supported Feature: To tweak TCP TX n/w parameters
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TCP_TX_HIGH_TPUT_THRESHOLD_NAME         "gTcpTxHighTputThreshold"
#define CFG_TCP_TX_HIGH_TPUT_THRESHOLD_DEFAULT      (500)
#define CFG_TCP_TX_HIGH_TPUT_THRESHOLD_MIN          (0)
#define CFG_TCP_TX_HIGH_TPUT_THRESHOLD_MAX          (16000)

/*
 * <ini>
 * periodic_stats_display_time - time(seconds) after which stats will be printed
 * @Min: 0
 * @Max: 256
 * @Default: 10
 *
 * This values specifies the recurring time period after which stats will be
 * printed in wlan driver logs.
 *
 * Usage: Internal / External
 *
 * </ini>
 */
#define CFG_PERIODIC_STATS_DISPLAY_TIME_NAME       "periodic_stats_display_time"
#define CFG_PERIODIC_STATS_DISPLAY_TIME_DEFAULT    (10)
#define CFG_PERIODIC_STATS_DISPLAY_TIME_MIN        (0)
#define CFG_PERIODIC_STATS_DISPLAY_TIME_MAX        (256)

#endif /* MSM_PLATFORM */

#ifdef WLAN_FEATURE_11W
/*
 * <ini>
 * pmfSaQueryMaxRetries - Control PMF SA query retries for SAP
 * @Min: 0
 * @Max: 20
 * @Default: 5
 *
 * This ini to set the number of PMF SA query retries for SAP
 *
 * Related: None.
 *
 * Supported Feature: PMF(11W)
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PMF_SA_QUERY_MAX_RETRIES_NAME          "pmfSaQueryMaxRetries"
#define CFG_PMF_SA_QUERY_MAX_RETRIES_DEFAULT       (5)
#define CFG_PMF_SA_QUERY_MAX_RETRIES_MIN           (0)
#define CFG_PMF_SA_QUERY_MAX_RETRIES_MAX           (20)

/*
 * <ini>
 * pmfSaQueryRetryInterval - Control PMF SA query retry interval
 * for SAP in ms
 * @Min: 0
 * @Max: 2000
 * @Default: 200
 *
 * This ini to set the PMF SA query retry interval for SAP in ms
 *
 * Related: None.
 *
 * Supported Feature: PMF(11W)
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_NAME       "pmfSaQueryRetryInterval"
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_DEFAULT    (200)
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_MIN        (0)
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_MAX        (2000)
#endif

/*
 * <ini>
 * gMaxConcurrentActiveSessions - Maximum number of concurrent connections.
 * @Min: 1
 * @Max: 4
 * @Default: 3
 *
 * This ini is used to configure the maximum number of concurrent connections.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAX_CONCURRENT_CONNECTIONS_NAME        "gMaxConcurrentActiveSessions"
#define CFG_MAX_CONCURRENT_CONNECTIONS_DEFAULT     (3)
#define CFG_MAX_CONCURRENT_CONNECTIONS_MIN         (1)
#define CFG_MAX_CONCURRENT_CONNECTIONS_MAX         (4)

/*
 * <ini>
 * gIgnoreCAC - Used to ignore CAC
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default CAC
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_IGNORE_CAC_NAME                        "gIgnoreCAC"
#define CFG_IGNORE_CAC_MIN                         (0)
#define CFG_IGNORE_CAC_MAX                         (1)
#define CFG_IGNORE_CAC_DEFAULT                     (0)

#define CFG_DFS_RADAR_PRI_MULTIPLIER_NAME          "gDFSradarMappingPriMultiplier"
#define CFG_DFS_RADAR_PRI_MULTIPLIER_DEFAULT       (4)
#define CFG_DFS_RADAR_PRI_MULTIPLIER_MIN           (0)
#define CFG_DFS_RADAR_PRI_MULTIPLIER_MAX           (10)

/*
 * <ini>
 * gReorderOffloadSupported - Packet reordering offload to firmware
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default Packet reordering
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_REORDER_OFFLOAD_SUPPORT_NAME    "gReorderOffloadSupported"
#define CFG_REORDER_OFFLOAD_SUPPORT_MIN     (0)
#define CFG_REORDER_OFFLOAD_SUPPORT_MAX     (1)
#define CFG_REORDER_OFFLOAD_SUPPORT_DEFAULT (1)

/* IpaUcTxBufCount should be power of 2 */
#define CFG_IPA_UC_TX_BUF_COUNT_NAME               "IpaUcTxBufCount"
#define CFG_IPA_UC_TX_BUF_COUNT_MIN                (0)
#define CFG_IPA_UC_TX_BUF_COUNT_MAX                (2048)
#define CFG_IPA_UC_TX_BUF_COUNT_DEFAULT            (512)

#define CFG_IPA_UC_TX_BUF_SIZE_NAME                "IpaUcTxBufSize"
#define CFG_IPA_UC_TX_BUF_SIZE_MIN                (0)
#define CFG_IPA_UC_TX_BUF_SIZE_MAX                (4096)
#define CFG_IPA_UC_TX_BUF_SIZE_DEFAULT            (2048)

/* IpaUcRxIndRingCount should be power of 2 */
#define CFG_IPA_UC_RX_IND_RING_COUNT_NAME          "IpaUcRxIndRingCount"
#define CFG_IPA_UC_RX_IND_RING_COUNT_MIN           (0)
#define CFG_IPA_UC_RX_IND_RING_COUNT_MAX           (2048)
#define CFG_IPA_UC_RX_IND_RING_COUNT_DEFAULT       (1024)

#define CFG_IPA_UC_TX_PARTITION_BASE_NAME          "IpaUcTxPartitionBase"
#define CFG_IPA_UC_TX_PARTITION_BASE_MIN           (0)
#define CFG_IPA_UC_TX_PARTITION_BASE_MAX           (9000)
#define CFG_IPA_UC_TX_PARTITION_BASE_DEFAULT       (3000)

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
/* Enable WLAN Logging to app space */
#define CFG_WLAN_LOGGING_SUPPORT_NAME               "wlanLoggingEnable"
#define CFG_WLAN_LOGGING_SUPPORT_ENABLE             (1)
#define CFG_WLAN_LOGGING_SUPPORT_DISABLE            (0)
#define CFG_WLAN_LOGGING_SUPPORT_DEFAULT            (1)

/* Enable forwarding the driver logs to kmsg console */
#define CFG_WLAN_LOGGING_CONSOLE_SUPPORT_NAME    "wlanLoggingToConsole"
#define CFG_WLAN_LOGGING_CONSOLE_SUPPORT_ENABLE  (1)
#define CFG_WLAN_LOGGING_CONSOLE_SUPPORT_DISABLE (0)
#define CFG_WLAN_LOGGING_CONSOLE_SUPPORT_DEFAULT (1)

/*
 * <ini>
 * host_log_custom_nl_proto - Host log netlink protocol
 * @Min: 0
 * @Max: 32
 * @Default: 2
 *
 * This ini is used to set host log netlink protocol. The default
 * value is 2 (NETLINK_USERSOCK), customer should avoid selecting the
 * netlink protocol that already used on their platform by other
 * applications or services. By choosing the non-default value(2),
 * Customer need to change the netlink protocol of application receive
 * tool(cnss_diag) accordingly. Available values could be:
 *
 * host_log_custom_nl_proto = 0 -	NETLINK_ROUTE, Routing/device hook
 * host_log_custom_nl_proto = 1 -	NETLINK_UNUSED, Unused number
 * host_log_custom_nl_proto = 2 -	NETLINK_USERSOCK, Reserved for user
 *					mode socket protocols
 * host_log_custom_nl_proto = 3 -	NETLINK_FIREWALL, Unused number,
 *					formerly ip_queue
 * host_log_custom_nl_proto = 4 -	NETLINK_SOCK_DIAG, socket monitoring
 * host_log_custom_nl_proto = 5 -	NETLINK_NFLOG, netfilter/iptables ULOG
 * host_log_custom_nl_proto = 6 -	NETLINK_XFRM, ipsec
 * host_log_custom_nl_proto = 7 -	NETLINK_SELINUX, SELinux event
 *					notifications
 * host_log_custom_nl_proto = 8 -	NETLINK_ISCSI, Open-iSCSI
 * host_log_custom_nl_proto = 9 -	NETLINK_AUDIT, auditing
 * host_log_custom_nl_proto = 10 -	NETLINK_FIB_LOOKUP
 * host_log_custom_nl_proto = 11 -	NETLINK_CONNECTOR
 * host_log_custom_nl_proto = 12 -	NETLINK_NETFILTER, netfilter subsystem
 * host_log_custom_nl_proto = 13 -	NETLINK_IP6_FW
 * host_log_custom_nl_proto = 14 -	NETLINK_DNRTMSG, DECnet routing messages
 * host_log_custom_nl_proto = 15 -	NETLINK_KOBJECT_UEVENT, Kernel
 *					messages to userspace
 * host_log_custom_nl_proto = 16 -	NETLINK_GENERIC, leave room for
 *					NETLINK_DM (DM Events)
 * host_log_custom_nl_proto = 18 -	NETLINK_SCSITRANSPORT, SCSI Transports
 * host_log_custom_nl_proto = 19 -	NETLINK_ECRYPTFS
 * host_log_custom_nl_proto = 20 -	NETLINK_RDMA
 * host_log_custom_nl_proto = 21 -	NETLINK_CRYPTO, Crypto layer
 * host_log_custom_nl_proto = 22 -	NETLINK_SMC, SMC monitoring
 *
 * The max value is: MAX_LINKS which is 32
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_HOST_LOG_CUSTOM_NETLINK_PROTO    "host_log_custom_nl_proto"
#define CFG_HOST_LOG_CUSTOM_NETLINK_PROTO_MIN           (0)
#define CFG_HOST_LOG_CUSTOM_NETLINK_PROTO_MAX           (32)
#define CFG_HOST_LOG_CUSTOM_NETLINK_PROTO_DEFAULT       (2)
#endif /* WLAN_LOGGING_SOCK_SVC_ENABLE */

#ifdef WLAN_FEATURE_LPSS
#define CFG_ENABLE_LPASS_SUPPORT                          "gEnableLpassSupport"
#define CFG_ENABLE_LPASS_SUPPORT_DEFAULT                  (0)
#define CFG_ENABLE_LPASS_SUPPORT_MIN                      (0)
#define CFG_ENABLE_LPASS_SUPPORT_MAX                      (1)
#endif

/*
 * NaN feature support configuration
 * gEnableNanSupport = 0 means NaN is not supported
 * gEnableNanSupport = 1 means NaN is supported
 */
#ifdef WLAN_FEATURE_NAN
#define CFG_ENABLE_NAN_SUPPORT                          "gEnableNanSupport"
#define CFG_ENABLE_NAN_SUPPORT_DEFAULT                  (0)
#define CFG_ENABLE_NAN_SUPPORT_MIN                      (0)
#define CFG_ENABLE_NAN_SUPPORT_MAX                      (1)

/*
 * <ini>
 * nan_separate_iface_support - Separate iface creation for NAN
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Value is 1 when Host HDD supports separate iface creation for NAN
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_SEPARATE_IFACE_SUPP                "nan_separate_iface_support"
#define CFG_NAN_SEPARATE_IFACE_SUPP_DEFAULT        (1)
#define CFG_NAN_SEPARATE_IFACE_SUPP_MIN            (0)
#define CFG_NAN_SEPARATE_IFACE_SUPP_MAX            (1)

/*
 * <ini>
 * gNdpKeepAlivePeriod - To configure duration of how many seconds
 * to wait to kickout peer if peer is not reachable.
 *
 * @Min: 10
 * @Max: 30
 * @Default: 20
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NDP_KEEP_ALIVE_PERIOD                  "gNdpKeepAlivePeriod"
#define CFG_NDP_KEEP_ALIVE_PERIOD_MIN              (10)
#define CFG_NDP_KEEP_ALIVE_PERIOD_MAX              (30)
#define CFG_NDP_KEEP_ALIVE_PERIOD_DEFAULT          (20)

/*
 * <ini>
 * nan_feature_config - Bitmap to enable/disable a particular NAN/NDP feature
 *
 * @Min: 0
 * @Max: 0xFFFF
 * @Default: 0x1
 *
 * This parameter helps to enable/disable a particular feature config by setting
 * corresponding bit and send to firmware through the VDEV param
 * WMI_VDEV_PARAM_ENABLE_DISABLE_NAN_CONFIG_FEATURES
 * Acceptable values for this:
 * BIT(0): Allow DW configuration from framework in sync role.
 *	   If this is not set, firmware shall follow the spec/default behavior.
 * BIT(1) to BIT(31): Reserved
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_FEATURE_CONFIG                  "nan_feature_config"
#define CFG_NAN_FEATURE_CONFIG_MIN              (0)
#define CFG_NAN_FEATURE_CONFIG_MAX              (0xFFFF)
#define CFG_NAN_FEATURE_CONFIG_DEFAULT          (1)

/*
 * <ini>
 * gSupportMp0Discovery - To support discovery of NAN cluster with
 * Master Preference (MP) as 0 when a new device is enabling NAN.
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SUPPORT_MP0_DISCOVERY                  "gSupportMp0Discovery"
#define CFG_SUPPORT_MP0_DISCOVERY_MIN              (0)
#define CFG_SUPPORT_MP0_DISCOVERY_MAX              (1)
#define CFG_SUPPORT_MP0_DISCOVERY_DEFAULT          (1)
#endif

#define CFG_ENABLE_SELF_RECOVERY                   "gEnableSelfRecovery"
#define CFG_ENABLE_SELF_RECOVERY_MIN               (0)
#define CFG_ENABLE_SELF_RECOVERY_MAX               (1)
#define CFG_ENABLE_SELF_RECOVERY_DEFAULT           (0)

#define CFG_ENABLE_SAP_SUSPEND                     "gEnableSapSuspend"
#define CFG_ENABLE_SAP_SUSPEND_MIN                 (0)
#define CFG_ENABLE_SAP_SUSPEND_MAX                 (1)
#define CFG_ENABLE_SAP_SUSPEND_DEFAULT             (1)

/*
 * <ini>
 * gEnableDeauthToDisassocMap - Enables deauth to disassoc map
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default  disassoc map
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_DEAUTH_TO_DISASSOC_MAP_NAME    "gEnableDeauthToDisassocMap"
#define CFG_ENABLE_DEAUTH_TO_DISASSOC_MAP_MIN     (0)
#define CFG_ENABLE_DEAUTH_TO_DISASSOC_MAP_MAX     (1)
#define CFG_ENABLE_DEAUTH_TO_DISASSOC_MAP_DEFAULT (0)

#ifdef DHCP_SERVER_OFFLOAD
#define CFG_DHCP_SERVER_OFFLOAD_SUPPORT_NAME      "gDHCPServerOffloadEnable"
#define CFG_DHCP_SERVER_OFFLOAD_SUPPORT_MIN       (0)
#define CFG_DHCP_SERVER_OFFLOAD_SUPPORT_MAX       (1)
#define CFG_DHCP_SERVER_OFFLOAD_SUPPORT_DEFAULT   (CFG_DHCP_SERVER_OFFLOAD_SUPPORT_MIN)

#define CFG_DHCP_SERVER_OFFLOAD_NUM_CLIENT_NAME     "gDHCPMaxNumClients"
#define CFG_DHCP_SERVER_OFFLOAD_NUM_CLIENT_MIN      (1)
#define CFG_DHCP_SERVER_OFFLOAD_NUM_CLIENT_MAX      (8)
#define CFG_DHCP_SERVER_OFFLOAD_NUM_CLIENT_DEFAULT  (CFG_DHCP_SERVER_OFFLOAD_NUM_CLIENT_MAX)

#define CFG_DHCP_SERVER_IP_NAME     "gDHCPServerIP"
#define CFG_DHCP_SERVER_IP_DEFAULT  ""
#endif /* DHCP_SERVER_OFFLOAD */

/*
 * <ini>
 * gSendDeauthBeforeCon - It will send deauth before connection
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default DeauthBeforeCon
 * If last disconnection was due to HB failure and we reconnect
 * to same AP next time, send Deauth before starting connection
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION                  "gSendDeauthBeforeCon"
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_MIN              (0)
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_MAX              (1)
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION_DEFAULT          (0)

/*
 * <ini>
 * gEnableCustomConcRule1 - Enable custom concurrency rule1.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable custom concurrency rule1.
 * If SAP comes up first and STA comes up later then SAP needs to follow STA's
 * channel.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_CUSTOM_CONC_RULE1_NAME         "gEnableCustomConcRule1"
#define CFG_ENABLE_CUSTOM_CONC_RULE1_NAME_MIN     (0)
#define CFG_ENABLE_CUSTOM_CONC_RULE1_NAME_MAX     (1)
#define CFG_ENABLE_CUSTOM_CONC_RULE1_NAME_DEFAULT (0)

/*
 * <ini>
 * gEnableCustomConcRule2 - Enable custom concurrency rule2.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable custom concurrency rule2.
 * If P2PGO comes up first and STA comes up later then P2PGO need to follow
 * STA's channel in 5Ghz. In following if condition we are just adding sanity
 * check to make sure that by this time P2PGO's channel is same as STA's
 * channel.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_CUSTOM_CONC_RULE2_NAME         "gEnableCustomConcRule2"
#define CFG_ENABLE_CUSTOM_CONC_RULE2_NAME_MIN     (0)
#define CFG_ENABLE_CUSTOM_CONC_RULE2_NAME_MAX     (1)
#define CFG_ENABLE_CUSTOM_CONC_RULE2_NAME_DEFAULT (0)

#define CFG_ENABLE_STA_CONNECTION_IN_5GHZ         "gEnableStaConnectionIn5Ghz"
#define CFG_ENABLE_STA_CONNECTION_IN_5GHZ_MIN     (0)
#define CFG_ENABLE_STA_CONNECTION_IN_5GHZ_MAX     (1)
#define CFG_ENABLE_STA_CONNECTION_IN_5GHZ_DEFAULT (1)

#define CFG_ENABLE_MAC_ADDR_SPOOFING                "gEnableMacAddrSpoof"
#define CFG_ENABLE_MAC_ADDR_SPOOFING_MIN            (0)
#define CFG_ENABLE_MAC_ADDR_SPOOFING_MAX            (1)
#define CFG_ENABLE_MAC_ADDR_SPOOFING_DEFAULT        (1)

/*
 * <ini>
 * gP2PListenDeferInterval - Defer Remain on channel for some duration
 * @Min: 100
 * @Max: 200
 * @Default: 100
 *
 * This ini is used to defer back to back RoC request when sta is
 * connected.
 * If back to back listen received when sta is connected then fw is
 * not getting enough time to spend on home channel so it leading to
 * heartbeat failure.
 *
 * Supported Feature: P2P
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_P2P_LISTEN_DEFER_INTERVAL_NAME        "gP2PListenDeferInterval"
#define CFG_P2P_LISTEN_DEFER_INTERVAL_MIN         (100)
#define CFG_P2P_LISTEN_DEFER_INTERVAL_MAX         (200)
#define CFG_P2P_LISTEN_DEFER_INTERVAL_DEFAULT     (100)

/*
 * <ini>
 * gStaMiracastMccRestTimeVal - Rest time when Miracast is running.
 * @Min: 100
 * @Max: 500
 * @Default: 400
 *
 * This ini is used to set rest time for home channel for Miracast before
 * going for scan.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_STA_MIRACAST_MCC_REST_TIME_VAL          "gStaMiracastMccRestTimeVal"
#define CFG_STA_MIRACAST_MCC_REST_TIME_VAL_MIN     (100)
#define CFG_STA_MIRACAST_MCC_REST_TIME_VAL_MAX     (500)
#define CFG_STA_MIRACAST_MCC_REST_TIME_VAL_DEFAULT (400)

/*
 * <ini>
 * sta_scan_burst_duration - Burst duration in case of split scan.
 * @Min: 0
 * @Max: 180
 * @Default: 0
 *
 * This ini is used to set burst duration of scan only when STA is active.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_STA_SCAN_BURST_DURATION_VAL                 "sta_scan_burst_duration"
#define CFG_STA_SCAN_BURST_DURATION_VAL_MIN             (0)
#define CFG_STA_SCAN_BURST_DURATION_VAL_MAX             (180)
#define CFG_STA_SCAN_BURST_DURATION_VAL_DEFAULT         (0)

/*
 * <ini>
 * p2p_scan_burst_duration - Burst duration in case of split scan for p2p scan.
 * @Min: 0
 * @Max: 180
 * @Default: 0
 *
 * This ini is used to set burst duration of scan for p2p scan requests.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_P2P_SCAN_BURST_DURATION_VAL                 "p2p_scan_burst_duration"
#define CFG_P2P_SCAN_BURST_DURATION_VAL_MIN             (0)
#define CFG_P2P_SCAN_BURST_DURATION_VAL_MAX             (180)
#define CFG_P2P_SCAN_BURST_DURATION_VAL_DEFAULT         (0)

/*
 * <ini>
 * go_scan_burst_duration - Burst duration in case of split scan when GO is
 * active.
 * @Min: 0
 * @Max: 180
 * @Default: 0
 *
 * This ini is used to set burst duration of scan when GO is active.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GO_SCAN_BURST_DURATION_VAL                 "go_scan_burst_duration"
#define CFG_GO_SCAN_BURST_DURATION_VAL_MIN             (0)
#define CFG_GO_SCAN_BURST_DURATION_VAL_MAX             (180)
#define CFG_GO_SCAN_BURST_DURATION_VAL_DEFAULT         (0)

/*
 * <ini>
 * ap_scan_burst_duration - Burst duration in case of split scan when ap
 * is active.
 * @Min: 0
 * @Max: 32
 * @Default: 0
 *
 * This ini is used to set burst duration of scan when SAP is active.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_AP_SCAN_BURST_DURATION_VAL                 "ap_scan_burst_duration"
#define CFG_AP_SCAN_BURST_DURATION_VAL_MIN             (0)
#define CFG_AP_SCAN_BURST_DURATION_VAL_MAX             (32)
#define CFG_AP_SCAN_BURST_DURATION_VAL_DEFAULT         (0)

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/*
 * <ini>
 * gSapChannelAvoidance - SAP MCC channel avoidance.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to sets sap mcc channel avoidance.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SAP_MCC_CHANNEL_AVOIDANCE_NAME         "gSapChannelAvoidance"
#define CFG_SAP_MCC_CHANNEL_AVOIDANCE_MIN          (0)
#define CFG_SAP_MCC_CHANNEL_AVOIDANCE_MAX          (1)
#define CFG_SAP_MCC_CHANNEL_AVOIDANCE_DEFAULT      (0)
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/*
 * <ini>
 * gSAP11ACOverride - Override bw to 11ac for SAP in driver even if supplicant
 *                    or hostapd configures HT.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable 11AC override for SAP.
 * Android UI does not provide advanced configuration options
 * for SoftAP for Android O and below.
 * Default override disabled for android. Can be enabled from
 * ini for Android O and below.
 *
 *
 * Supported Feature: SAP
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SAP_11AC_OVERRIDE_NAME             "gSAP11ACOverride"
#define CFG_SAP_11AC_OVERRIDE_MIN              (0)
#define CFG_SAP_11AC_OVERRIDE_MAX              (1)
#define CFG_SAP_11AC_OVERRIDE_DEFAULT          (0)

/*
 * <ini>
 * gGO11ACOverride - Override bw to 11ac for P2P GO
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable 11AC override for GO.
 * P2P GO also follows start_bss and since P2P GO could not be
 * configured to setup VHT channel width in wpa_supplicant, driver
 * can override 11AC.
 *
 *
 * Supported Feature: P2P
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_GO_11AC_OVERRIDE_NAME             "gGO11ACOverride"
#define CFG_GO_11AC_OVERRIDE_MIN              (0)
#define CFG_GO_11AC_OVERRIDE_MAX              (1)
#define CFG_GO_11AC_OVERRIDE_DEFAULT          (1)

#define CFG_SAP_DOT11MC               "gSapDot11mc"
#define CFG_SAP_DOT11MC_MIN           (0)
#define CFG_SAP_DOT11MC_MAX           (1)
#define CFG_SAP_DOT11MC_DEFAULT       (0)

#define CFG_ENABLE_NON_DFS_CHAN_ON_RADAR           "gPreferNonDfsChanOnRadar"
#define CFG_ENABLE_NON_DFS_CHAN_ON_RADAR_MIN       (0)
#define CFG_ENABLE_NON_DFS_CHAN_ON_RADAR_MAX       (1)
#define CFG_ENABLE_NON_DFS_CHAN_ON_RADAR_DEFAULT   (0)

#define CFG_MULTICAST_HOST_FW_MSGS          "gMulticastHostFwMsgs"
#define CFG_MULTICAST_HOST_FW_MSGS_MIN      (0)
#define CFG_MULTICAST_HOST_FW_MSGS_MAX      (1)
#if defined(MDM_PLATFORM) && !defined(FEATURE_MULTICAST_HOST_FW_MSGS)
#define CFG_MULTICAST_HOST_FW_MSGS_DEFAULT  (0)
#else
#define CFG_MULTICAST_HOST_FW_MSGS_DEFAULT  (1)
#endif


/*
 * <ini>
 * gSystemPref - Configure wlan system preference for PCL.
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to configure wlan system preference option to help
 * policy manager decide on Preferred Channel List for a new connection.
 * For possible values refer to enum hdd_conc_priority_mode
 *
 * Related: None.
 *
 * Supported Feature: DBS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_CONC_SYSTEM_PREF               "gSystemPref"
#define CFG_CONC_SYSTEM_PREF_MIN           (0)
#define CFG_CONC_SYSTEM_PREF_MAX           (2)
#define CFG_CONC_SYSTEM_PREF_DEFAULT       (0)

/*
 * <ini>
 * TSOEnable - Control to enable tso feature
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable TSO feature
 *
 * Supported Feature: TSO Feature
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TSO_ENABLED_NAME           "TSOEnable"
#define CFG_TSO_ENABLED_MIN            (0)
#define CFG_TSO_ENABLED_MAX            (1)
#define CFG_TSO_ENABLED_DEFAULT        (0)

/*
 * <ini>
 * LROEnable - Control to enable lro feature
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable LRO feature
 *
 * Supported Feature: LRO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LRO_ENABLED_NAME           "LROEnable"
#define CFG_LRO_ENABLED_MIN            (0)
#define CFG_LRO_ENABLED_MAX            (1)
#define CFG_LRO_ENABLED_DEFAULT        (0)

/*
 * <ini>
 * GROEnable - Control to enable gro feature
 *
 * @Disable: 0
 * @Enable: 1
 * @Default: 0
 *
 * This ini is used to enable GRO feature
 *
 * Supported Feature: GRO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_GRO_ENABLED_NAME           "GROEnable"
#define CFG_GRO_ENABLED_MIN            (0)
#define CFG_GRO_ENABLED_MAX            (1)
#define CFG_GRO_ENABLED_DEFAULT        (0)

/*
 * Enable Rx traffic flow steering to enable Rx interrupts on multiple CEs based
 * on the flows. Different CEs<==>different IRQs<==>probably different CPUs.
 * Parallel Rx paths.
 * 1 - enable  0 - disable
 */
#define CFG_FLOW_STEERING_ENABLED_NAME        "gEnableFlowSteering"
#define CFG_FLOW_STEERING_ENABLED_MIN         (0)
#define CFG_FLOW_STEERING_ENABLED_MAX         (1)
#define CFG_FLOW_STEERING_ENABLED_DEFAULT     (0)

/*
 * Max number of MSDUs per HTT RX IN ORDER INDICATION msg.
 * Note that this has a direct impact on the size of source CE rings.
 * It is possible to go below 8, but would require testing; so we are
 * restricting the lower limit to 8 artificially
 *
 * It is recommended that this value is a POWER OF 2.
 *
 * Values lower than 8 are for experimental purposes only.
 */
#define CFG_MAX_MSDUS_PER_RXIND_NAME          "maxMSDUsPerRxInd"
#define CFG_MAX_MSDUS_PER_RXIND_MIN           (4)
#define CFG_MAX_MSDUS_PER_RXIND_MAX           (32)
#define CFG_MAX_MSDUS_PER_RXIND_DEFAULT       (32)
/*
 * In static display use case when APPS is in stand alone power save mode enable
 * active offload mode which helps FW to filter out MC/BC data packets to avoid
 * APPS wake up and save more power.
 *
 * By default enable active mode offload as it helps to save more power in
 * static display usecase(APPS stand alone power collapse).
 *
 * If active mode offload(gActiveModeOffload=1) is enabled then all applicable
 * data offload/filtering is enabled immediately in FW once config is available
 * in WLAN driver and FW caches this configuration across suspend/resume
 *
 * If active mode offload is disabled(gActiveModeOffload=0) then all applicable
 * data offload/filtering is enabled during cfg80211 suspend and disabled
 * during cfg80211 resume
 *
 * Active mode offload feature is bydefault enabled for all targets
 */

#define CFG_ACTIVE_MODE_OFFLOAD            "gActiveModeOffload"
#define CFG_ACTIVE_MODE_OFFLOAD_MIN        (0)
#define CFG_ACTIVE_MODE_OFFLOAD_MAX        (1)
#define CFG_ACTIVE_MODE_OFFLOAD_DEFAULT    (1)

/*
 * 0: Disable APF packet filter
 * 1: Enable APF packet filter
 */
#define CFG_APF_PACKET_FILTER_OFFLOAD           "gBpfFilterEnable"
#define CFG_APF_PACKET_FILTER_OFFLOAD_MIN       (0)
#define CFG_APF_PACKET_FILTER_OFFLOAD_MAX       (1)
#define CFG_APF_PACKET_FILTER_OFFLOAD_DEFAULT   (1)

/*
 * <ini>
 * gCckChainMaskEnable - Used to enable/disable Cck ChainMask
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default Cck ChainMask
 * 0: disable the cck tx chain mask (default)
 * 1: enable the cck tx chain mask
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_TX_CHAIN_MASK_CCK          "gCckChainMaskEnable"
#define CFG_TX_CHAIN_MASK_CCK_MIN      (0)
#define CFG_TX_CHAIN_MASK_CCK_MAX      (1)
#define CFG_TX_CHAIN_MASK_CCK_DEFAULT  (0)
/*
 * <ini>
 * gTxChainMask1ss - Enables/disables tx chain Mask1ss
 * @Min: 0
 * @Max: 3
 * @Default: 1
 *
 * This ini is used to set default tx chain Mask1ss
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_TX_CHAIN_MASK_1SS       "gTxChainMask1ss"
#define CFG_TX_CHAIN_MASK_1SS_MIN      (0)
#define CFG_TX_CHAIN_MASK_1SS_MAX      (3)
#define CFG_TX_CHAIN_MASK_1SS_DEFAULT  (1)

/*
 * <ini>
 * gEnableSmartChainmask - Enable Smart Chainmask
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the Smart Chainmask feature via
 * the WMI_PDEV_PARAM_SMART_CHAINMASK_SCHEME firmware parameter.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_SMART_CHAINMASK_NAME    "gEnableSmartChainmask"
#define CFG_ENABLE_SMART_CHAINMASK_MIN     (0)
#define CFG_ENABLE_SMART_CHAINMASK_MAX     (1)
#define CFG_ENABLE_SMART_CHAINMASK_DEFAULT (0)

/*
 * <ini>
 * gEnableAlternativeChainmask - Enable Co-Ex Alternative Chainmask
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the Co-ex Alternative Chainmask
 * feature via the WMI_PDEV_PARAM_ALTERNATIVE_CHAINMASK_SCHEME
 * firmware parameter.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_COEX_ALT_CHAINMASK_NAME    "gEnableAlternativeChainmask"
#define CFG_ENABLE_COEX_ALT_CHAINMASK_MIN     (0)
#define CFG_ENABLE_COEX_ALT_CHAINMASK_MAX     (1)
#define CFG_ENABLE_COEX_ALT_CHAINMASK_DEFAULT (0)

/*
 * set the self gen power value from
 * 0 to 0xffff
 */
#define CFG_SELF_GEN_FRM_PWR        "gSelfGenFrmPwr"
#define CFG_SELF_GEN_FRM_PWR_MIN      (0)
#define CFG_SELF_GEN_FRM_PWR_MAX      (0xffff)
#define CFG_SELF_GEN_FRM_PWR_DEFAULT  (0)

/*
 * <ini>
 * gTxAggregationSize - Gives an option to configure Tx aggregation size
 * in no of MPDUs
 * @Min: 0
 * @Max: 64
 * @Default: 64
 *
 * gTxAggregationSize gives an option to configure Tx aggregation size
 * in no of MPDUs.This can be useful in debugging throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_TX_AGGREGATION_SIZE      "gTxAggregationSize"
#define CFG_TX_AGGREGATION_SIZE_MIN      (0)
#define CFG_TX_AGGREGATION_SIZE_MAX      (64)
#define CFG_TX_AGGREGATION_SIZE_DEFAULT  (64)

/*
 * <ini>
 * gTxAggregationSizeBE - To configure Tx aggregation size for BE queue
 * in no of MPDUs
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggregationSizeBE gives an option to configure Tx aggregation size
 * for BE queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGREGATION_SIZEBE      "gTxAggregationSizeBE"
#define CFG_TX_AGGREGATION_SIZEBE_MIN      (0)
#define CFG_TX_AGGREGATION_SIZEBE_MAX      (64)
#define CFG_TX_AGGREGATION_SIZEBE_DEFAULT  (0)

/*
 * <ini>
 * gTxAggregationSizeBK - To configure Tx aggregation size for BK queue
 * in no of MPDUs
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggregationSizeBK gives an option to configure Tx aggregation size
 * for BK queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGREGATION_SIZEBK      "gTxAggregationSizeBK"
#define CFG_TX_AGGREGATION_SIZEBK_MIN      (0)
#define CFG_TX_AGGREGATION_SIZEBK_MAX      (64)
#define CFG_TX_AGGREGATION_SIZEBK_DEFAULT  (0)

/*
 * <ini>
 * gTxAggregationSizeVI - To configure Tx aggregation size for VI queue
 * in no of MPDUs
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggregationSizeVI gives an option to configure Tx aggregation size
 * for VI queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGREGATION_SIZEVI      "gTxAggregationSizeVI"
#define CFG_TX_AGGREGATION_SIZEVI_MIN      (0)
#define CFG_TX_AGGREGATION_SIZEVI_MAX      (64)
#define CFG_TX_AGGREGATION_SIZEVI_DEFAULT  (0)

/*
 * <ini>
 * gTxAggregationSizeVO - To configure Tx aggregation size for VO queue
 * in no of MPDUs
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggregationSizeVO gives an option to configure Tx aggregation size
 * for BE queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGREGATION_SIZEVO      "gTxAggregationSizeVO"
#define CFG_TX_AGGREGATION_SIZEVO_MIN      (0)
#define CFG_TX_AGGREGATION_SIZEVO_MAX      (64)
#define CFG_TX_AGGREGATION_SIZEVO_DEFAULT  (0)

/*
 * <ini>
 * gRxAggregationSize - Gives an option to configure Rx aggregation size
 * in no of MPDUs
 * @Min: 1
 * @Max: 64
 * @Default: 64
 *
 * gRxAggregationSize gives an option to configure Rx aggregation size
 * in no of MPDUs. This can be useful in debugging throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_RX_AGGREGATION_SIZE      "gRxAggregationSize"
#define CFG_RX_AGGREGATION_SIZE_MIN      (1)
#define CFG_RX_AGGREGATION_SIZE_MAX      (64)
#define CFG_RX_AGGREGATION_SIZE_DEFAULT  (64)

/*
 * <ini>
 * gTxAggSwRetryBE - Configure Tx aggregation sw retry for BE
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryBE gives an option to configure Tx aggregation sw
 * retry for BE. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGR_SW_RETRY_BE      "gTxAggSwRetryBE"
#define CFG_TX_AGGR_SW_RETRY_BE_MIN      (0)
#define CFG_TX_AGGR_SW_RETRY_BE_MAX      (64)
#define CFG_TX_AGGR_SW_RETRY_BE_DEFAULT  (0)

/*
 * <ini>
 * gTxAggSwRetryBK - Configure Tx aggregation sw retry for BK
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryBK gives an option to configure Tx aggregation sw
 * retry for BK. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGR_SW_RETRY_BK      "gTxAggSwRetryBK"
#define CFG_TX_AGGR_SW_RETRY_BK_MIN      (0)
#define CFG_TX_AGGR_SW_RETRY_BK_MAX      (64)
#define CFG_TX_AGGR_SW_RETRY_BK_DEFAULT  (0)

/*
 * <ini>
 * gTxAggSwRetryVI - Configure Tx aggregation sw retry for VI
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryVI gives an option to configure Tx aggregation sw
 * retry for VI. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGR_SW_RETRY_VI      "gTxAggSwRetryVI"
#define CFG_TX_AGGR_SW_RETRY_VI_MIN      (0)
#define CFG_TX_AGGR_SW_RETRY_VI_MAX      (64)
#define CFG_TX_AGGR_SW_RETRY_VI_DEFAULT  (0)

/*
 * <ini>
 * gTxAggSwRetryVO - Configure Tx aggregation sw retry for VO
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryVO gives an option to configure Tx aggregation sw
 * retry for VO. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_AGGR_SW_RETRY_VO      "gTxAggSwRetryVO"
#define CFG_TX_AGGR_SW_RETRY_VO_MIN      (0)
#define CFG_TX_AGGR_SW_RETRY_VO_MAX      (64)
#define CFG_TX_AGGR_SW_RETRY_VO_DEFAULT  (0)

/*
 * <ini>
 * gTxAggSwRetry - Configure Tx aggregation sw retry
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetry gives an option to configure Tx aggregation sw
 * retry. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_TX_AGGR_SW_RETRY      "gTxAggSwRetry"
#define CFG_TX_AGGR_SW_RETRY_MIN      (0)
#define CFG_TX_AGGR_SW_RETRY_MAX      (64)
#define CFG_TX_AGGR_SW_RETRY_DEFAULT  (0)

/*
 * <ini>
 * gTxNonAggSwRetryBE - Configure Tx non aggregation sw retry for BE
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryBE gives an option to configure Tx non aggregation sw
 * retry for BE. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_NON_AGGR_SW_RETRY_BE      "gTxNonAggSwRetryBE"
#define CFG_TX_NON_AGGR_SW_RETRY_BE_MIN      (0)
#define CFG_TX_NON_AGGR_SW_RETRY_BE_MAX      (64)
#define CFG_TX_NON_AGGR_SW_RETRY_BE_DEFAULT  (0)

/*
 * <ini>
 * gTxNonAggSwRetryBK - Configure Tx non aggregation sw retry for BK
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryBK gives an option to configure Tx non aggregation sw
 * retry for BK. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_NON_AGGR_SW_RETRY_BK      "gTxNonAggSwRetryBK"
#define CFG_TX_NON_AGGR_SW_RETRY_BK_MIN      (0)
#define CFG_TX_NON_AGGR_SW_RETRY_BK_MAX      (64)
#define CFG_TX_NON_AGGR_SW_RETRY_BK_DEFAULT  (0)

/*
 * <ini>
 * gTxNonAggSwRetryVI - Configure Tx non aggregation sw retry for VI
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryVI gives an option to configure Tx non aggregation sw
 * retry for VI. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_NON_AGGR_SW_RETRY_VI      "gTxNonAggSwRetryVI"
#define CFG_TX_NON_AGGR_SW_RETRY_VI_MIN      (0)
#define CFG_TX_NON_AGGR_SW_RETRY_VI_MAX      (64)
#define CFG_TX_NON_AGGR_SW_RETRY_VI_DEFAULT  (0)

/*
 * <ini>
 * gTxNonAggSwRetryVO - Configure Tx non aggregation sw retry for VO
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryVO gives an option to configure Tx non aggregation sw
 * retry for VO. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_TX_NON_AGGR_SW_RETRY_VO      "gTxNonAggSwRetryVO"
#define CFG_TX_NON_AGGR_SW_RETRY_VO_MIN      (0)
#define CFG_TX_NON_AGGR_SW_RETRY_VO_MAX      (64)
#define CFG_TX_NON_AGGR_SW_RETRY_VO_DEFAULT  (0)

/*
 * <ini>
 * gTxNonAggSwRetry - Configure Tx non aggregation sw retry
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetry gives an option to configure Tx non aggregation sw
 * retry. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_TX_NON_AGGR_SW_RETRY      "gTxNonAggSwRetry"
#define CFG_TX_NON_AGGR_SW_RETRY_MIN      (0)
#define CFG_TX_NON_AGGR_SW_RETRY_MAX      (64)
#define CFG_TX_NON_AGGR_SW_RETRY_DEFAULT  (0)

/*
 * fine timing measurement capability information
 *
 * <----- fine_time_meas_cap (in bits) ----->
 *+----------+-----+-----+------+------+-------+-------+-----+-----+
 *|   8-31   |  7  |  6  |   5  |   4  |   3   |   2   |  1  |  0  |
 *+----------+-----+-----+------+------+-------+-------+-----+-----+
 *| reserved | SAP | SAP |P2P-GO|P2P-GO|P2P-CLI|P2P-CLI| STA | STA |
 *|          |resp |init |resp  |init  |resp   |init   |resp |init |
 *+----------+-----+-----+------+------+-------+-------+-----+-----+
 *
 * resp - responder role; init- initiator role
 *
 * CFG_FINE_TIME_MEAS_CAPABILITY_MAX computed based on the table
 * +-----------------+-----------------+-----------+
 * |  Device Role    |   Initiator     | Responder |
 * +-----------------+-----------------+-----------+
 * |   Station       |       Y         |     N     |
 * |   P2P-CLI       |       Y         |     Y     |
 * |   P2P-GO        |       Y         |     Y     |
 * |   SAP           |       N         |     Y     |
 * +-----------------+-----------------+-----------+
 */
#define CFG_FINE_TIME_MEAS_CAPABILITY              "gfine_time_meas_cap"
#define CFG_FINE_TIME_MEAS_CAPABILITY_MIN          (0x0000)
#define CFG_FINE_TIME_MEAS_CAPABILITY_MAX          (0x00BD)
#define CFG_FINE_TIME_MEAS_CAPABILITY_DEFAULT      (0x000D)

/**
 * enum dot11p_mode - The 802.11p mode of operation
 * @WLAN_HDD_11P_DISABLED:   802.11p mode is disabled
 * @WLAN_HDD_11P_STANDALONE: 802.11p-only operation
 * @WLAN_HDD_11P_CONCURRENT: 802.11p and WLAN operate concurrently
 */
enum dot11p_mode {
	WLAN_HDD_11P_DISABLED = 0,
	WLAN_HDD_11P_STANDALONE,
	WLAN_HDD_11P_CONCURRENT,
};

#define CFG_DOT11P_MODE_NAME             "gDot11PMode"
#define CFG_DOT11P_MODE_DEFAULT          (WLAN_HDD_11P_DISABLED)
#define CFG_DOT11P_MODE_MIN              (WLAN_HDD_11P_DISABLED)
#define CFG_DOT11P_MODE_MAX              (WLAN_HDD_11P_CONCURRENT)

/*
 * <ini>
 * etsi13_srd_chan_in_master_mode - Enable/disable ETSI SRD channels in
 * master mode PCL and ACS functionality
 * @Min: 0
 * @Max: 0xFF
 * @Default: 6
 *
 * etsi_srd_chan_in_master_mode is to enable/disable ETSI SRD channels in
 * master mode PCL and ACS functionality
 * Bit map for enabling the SRD mode in various modes are as follows:-
 * BIT 0:- Enable/Disable SRD channels for SAP.
 * BIT 1:- Enable/Disable SRD channels for P2P-GO.
 * BIT 2:- Enable/Disable SRD channels for NAN.
 * Rest of the bits are currently reserved for future SRD channel support for
 * other vdevs.
 *
 * Related: None
 *
 * Supported Feature: SAP/P2P-GO
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ETSI13_SRD_CHAN_IN_MASTER_MODE    "etsi13_srd_chan_in_master_mode"
#define CFG_ETSI13_SRD_CHAN_IN_MASTER_MODE_DEF (6)
#define CFG_ETSI13_SRD_CHAN_IN_MASTER_MODE_MIN (0)
#define CFG_ETSI13_SRD_CHAN_IN_MASTER_MODE_MAX (0xFF)

/*
 * <ini>
 * num_tx_chains_2g - Config Param to change number of tx
 * chains per vdev for 2.4ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num tx chains for 2.4ghz connection to 1 each
 * 0x02492492 - change all vdev's num tx chains for 2.4ghz connection to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_2G           "num_tx_chains_2g"
#define CFG_NUM_TX_CHAINS_2G_MIN       0x01249249
#define CFG_NUM_TX_CHAINS_2G_MAX       0x02492492
#define CFG_NUM_TX_CHAINS_2G_DEF       0x02492492

/*
 * <ini>
 * num_tx_chains_5g - Config Param to change number of tx
 * chains per vdev for 5 ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249- change all vdev's tx num chains for 5ghz connection to 1 each
 * 0x02492492 - change all vdev's tx num chains for 5ghz connection to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_5G           "num_tx_chains_5g"
#define CFG_NUM_TX_CHAINS_5G_MIN       0x01249249
#define CFG_NUM_TX_CHAINS_5G_MAX       0x02492492
#define CFG_NUM_TX_CHAINS_5G_DEF       0x02492492

/*
 * <ini>
 * num_rx_chains_2g - Config Param to change number of rx
 * chains per vdev for 2.4 ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's rx num chains for 2.4ghz connections to 1 each
 * 0x02492492 - change all vdev's rx num chains for 2.4ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_RX_CHAINS_2G           "num_rx_chains_2g"
#define CFG_NUM_RX_CHAINS_2G_MIN       0x01249249
#define CFG_NUM_RX_CHAINS_2G_MAX       0x02492492
#define CFG_NUM_RX_CHAINS_2G_DEF       0x02492492

/*
 * <ini>
 * num_rx_chains_5g - Config Param to change number of rx
 * chains per vdev for 5 ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's rx num chains for 5ghz connections to 1 each
 * 0x02492492 - change all vdev's rx num chains for 5ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_RX_CHAINS_5G           "num_rx_chains_5g"
#define CFG_NUM_RX_CHAINS_5G_MIN       0x01249249
#define CFG_NUM_RX_CHAINS_5G_MAX       0x02492492
#define CFG_NUM_RX_CHAINS_5G_DEF       0x02492492

/*
 * <ini>
 * tx_nss_2g - Config Param to change tx nss
 * per vdev for 2.4ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of tx spatial streams for eg:-
 * 0x01249249 - change all vdev's tx nss for 2.4ghz connections to 1 each
 * 0x02492492 - change all vdev's tx nss for 2.4ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_NSS_2G           "tx_nss_2g"
#define CFG_TX_NSS_2G_MIN       0x01249249
#define CFG_TX_NSS_2G_MAX       0x02492492
#define CFG_TX_NSS_2G_DEF       0x02492492

/*
 * <ini>
 * tx_nss_5g - Config Param to change tx nss
 * per vdev for 5ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of tx spatial streams for eg:-
 * 0x01249249 - change all vdev's tx nss for 5ghz connections to 1 each
 * 0x02492492 - change all vdev's tx nss for 5ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_NSS_5G           "tx_nss_5g"
#define CFG_TX_NSS_5G_MIN       0x01249249
#define CFG_TX_NSS_5G_MAX       0x02492492
#define CFG_TX_NSS_5G_DEF       0x02492492

/*
 * <ini>
 * rx_nss_2g - Config Param to change rx nss
 * per vdev for 2.4ghz frequency connections
 *
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of rx spatial streams for eg:-
 * 0x01249249 - change all vdev's rx nss for 2.4ghz connections to 1 each
 * 0x02492492 - change all vdev's rx nss for 2.4ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_NSS_2G           "rx_nss_2g"
#define CFG_RX_NSS_2G_MIN       0x01249249
#define CFG_RX_NSS_2G_MAX       0x02492492
#define CFG_RX_NSS_2G_DEF       0x02492492

/*
 * <ini>
 * rx_nss_5g - Config Param to change rx nss
 * per vdev for 5ghz frequency connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of rx spatial streams for eg:-
 * 0x01249249 - change all vdev's rx nss for 5ghz connections to 1 each
 * 0x02492492 - change all vdev's rx nss for 5ghz connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_NSS_5G           "rx_nss_5g"
#define CFG_RX_NSS_5G_MIN       0x01249249
#define CFG_RX_NSS_5G_MAX       0x02492492
#define CFG_RX_NSS_5G_DEF       0x02492492

/*
 * <ini>
 * num_tx_chains_11b - Config Param to change number of tx
 * chains per vdev for 2.4ghz 11b mode connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num chains for 11b connections to 1 each
 * 0x02492492 - change all vdev's num chains for 11b connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_11b          "num_tx_chains_11b"
#define CFG_NUM_TX_CHAINS_11b_MIN       0x01249249
#define CFG_NUM_TX_CHAINS_11b_MAX       0x02492492
#define CFG_NUM_TX_CHAINS_11b_DEF       0x02492492

/*
 * <ini>
 * num_tx_chains_11g - Config Param to change number of tx
 * chains per vdev for 2.4ghz 11g mode connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num chains for 11g connections to 1 each
 * 0x02492492 - change all vdev's num chains for 11g connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_11g          "num_tx_chains_11g"
#define CFG_NUM_TX_CHAINS_11g_MIN       0x01249249
#define CFG_NUM_TX_CHAINS_11g_MAX       0x02492492
#define CFG_NUM_TX_CHAINS_11g_DEF       0x02492492

/*
 * <ini>
 * num_tx_chains_11a - Config Param to change number of tx
 * chains per vdev for 5ghz 11a mode connections
 * @Min: 0x01249249
 * @Max: 0x02492492
 * @Default: 0x02492492
 *
 * This ini is used to change the num of chains for eg:-
 * 0x01249249 - change all vdev's num chains for 11a connections to 1 each
 * 0x02492492 - change all vdev's num chains for 11a connections to 2 each
 * Bits          VDEV Type
 * BIT[0:2]        STA
 * BIT[3:5]        SAP
 * BIT[6:8]        P2P GO
 * BIT[9:11]       P2P Client
 * BIT[12:14]      TDLS
 * BIT[15:17]      IBSS
 * BIT[18:20]      P2P device
 * BIT[21:23]      OCB
 * BIT[24:26]      NAN
 * BIT[27:31]      Reserved
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_TX_CHAINS_11a          "num_tx_chains_11a"
#define CFG_NUM_TX_CHAINS_11a_MIN       0x01249249
#define CFG_NUM_TX_CHAINS_11a_MAX       0x02492492
#define CFG_NUM_TX_CHAINS_11a_DEF       0x02492492

/*
 * <ini>
 * disable_tx_mrc_2g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_TX_MRC_2G             "disable_tx_mrc_2g"
#define CFG_DISABLE_TX_MRC_2G_MIN         0
#define CFG_DISABLE_TX_MRC_2G_MAX         1
#define CFG_DISABLE_TX_MRC_2G_DEF         0

/*
 * <ini>
 * disable_rx_mrc_2g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_RX_MRC_2G            "disable_rx_mrc_2g"
#define CFG_DISABLE_RX_MRC_2G_MIN        0
#define CFG_DISABLE_RX_MRC_2G_MAX        1
#define CFG_DISABLE_RX_MRC_2G_DEF        0

/*
 * <ini>
 * disable_tx_mrc_5g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_TX_MRC_5G           "disable_tx_mrc_5g"
#define CFG_DISABLE_TX_MRC_5G_MIN        0
#define CFG_DISABLE_TX_MRC_5G_MAX        1
#define CFG_DISABLE_TX_MRC_5G_DEF        0

/*
 * <ini>
 * enable_dynamic_nss_chain_config - Enable/Disable dynamic nss and chain config
 * to FW.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Related: STA/SAP/P2P/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DYNAMIC_NSS_CHAIN_CFG       "enable_dynamic_nss_chain_config"
#define CFG_ENABLE_DYNAMIC_NSS_CHAIN_CFG_MIN   0
#define CFG_ENABLE_DYNAMIC_NSS_CHAIN_CFG_MAX   1
#define CFG_ENABLE_DYNAMIC_NSS_CHAIN_CFG_DEF   1

/*
 * <ini>
 * disable_rx_mrc_5g - Config Param to disable 2 chains in 1x1 nss mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Dynamic chainmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_RX_MRC_5G           "disable_rx_mrc_5g"
#define CFG_DISABLE_RX_MRC_5G_MIN        0
#define CFG_DISABLE_RX_MRC_5G_MAX        1
#define CFG_DISABLE_RX_MRC_5G_DEF        0

/*
 * <ini>
 * gEnable_go_cts2self_for_sta - Indicate firmware to stop NOA and
 * start using cts2self
 * @Min: 1
 * @Max: 1
 * @Default: 0
 *
 * When gEnable_go_cts2self_for_sta is
 * enabled  then if a legacy client connects to P2P GO,
 * Host will send a WMI VDEV command to FW to stop using NOA for P2P GO
 * and start using CTS2SELF.
 *
 *
 * Supported Feature: P2P
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_GO_CTS2SELF_FOR_STA   "gEnable_go_cts2self_for_sta"
#define CFG_ENABLE_GO_CTS2SELF_FOR_STA_DEFAULT  (0)
#define CFG_ENABLE_GO_CTS2SELF_FOR_STA_MIN      (0)
#define CFG_ENABLE_GO_CTS2SELF_FOR_STA_MAX      (1)

#define CFG_CE_CLASSIFY_ENABLE_NAME	"gCEClassifyEnable"
#define CFG_CE_CLASSIFY_ENABLE_MIN	(0)
#define CFG_CE_CLASSIFY_ENABLE_MAX	(1)
#define CFG_CE_CLASSIFY_ENABLE_DEFAULT	(1)

/*
 *
 * <ini>
 * gDualMacFeatureDisable - Disable Dual MAC feature.
 * @Min: 0
 * @Max: 4
 * @Default: 0
 *
 * This ini is used to enable/disable dual MAC feature.
 * 0 - enable DBS
 * 1 - disable DBS
 * 2 - disable DBS for connection but keep DBS for scan
 * 3 - disable DBS for connection but keep DBS scan with async
 * scan policy disabled
 * 4 - enable DBS for connection as well as for scan with async
 * scan policy disabled
 * 5 - enable DBS for connection but disable DBS for scan.
 * 6 - enable DBS for connection but disable simultaneous scan
 * from upper layer (DBS scan remains enabled in FW).
 *
 * Note: INI item value should match 'enum dbs_support'
 *
 * Related: None.
 *
 * Supported Feature: DBS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DUAL_MAC_FEATURE_DISABLE              "gDualMacFeatureDisable"
#define CFG_DUAL_MAC_FEATURE_DISABLE_MIN          (0)
#define CFG_DUAL_MAC_FEATURE_DISABLE_MAX          (6)
#define CFG_DUAL_MAC_FEATURE_DISABLE_DEFAULT      (0)

/*
 * <ini>
 * gdbs_scan_selection - DBS Scan Selection.
 * @Default: ""
 *
 * This ini is used to enable DBS scan selection.
 * Example
 * @Value: "5,2,2,16,2,2"
 * 1st argument is module_id, 2nd argument is number of DBS scan,
 * 3rd argument is number of non-DBS scan,
 * and other arguments follows.
 * 5,2,2,16,2,2 means:
 * 5 is module id, 2 is num of DBS scan, 2 is num of non-DBS scan.
 * 16 is module id, 2 is num of DBS scan, 2 is num of non-DBS scan.
 *
 * Related: None.
 *
 * Supported Feature: DBS Scan
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DBS_SCAN_SELECTION_NAME          "gdbs_scan_selection"
#define CFG_DBS_SCAN_SELECTION_DEFAULT       ""

/*
 * <ini>
 * g_sta_sap_scc_on_dfs_chan - Allow STA+SAP SCC on DFS channel with master
 * mode support disabled.
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to allow STA+SAP SCC on DFS channel with master mode
 * support disabled, the value is defined by enum PM_AP_DFS_MASTER_MODE.
 * 0 - Disallow STA+SAP SCC on DFS channel
 * 1 - Allow STA+SAP SCC on DFS channel with master mode disabled
 * 2 - enhance "1" with below requirement
 *	 a. Allow single SAP (GO) start on DFS channel.
 *	 b. Allow CAC process on DFS channel in single SAP (GO) mode
 *	 c. Allow DFS radar event process in single SAP (GO) mode
 *	 d. Disallow CAC and radar event process in SAP (GO) + STA mode.
 * Related: None.
 *
 * Supported Feature: Non-DBS, DBS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_STA_SAP_SCC_ON_DFS_CHAN              "g_sta_sap_scc_on_dfs_chan"
#define CFG_STA_SAP_SCC_ON_DFS_CHAN_MIN          (0)
#define CFG_STA_SAP_SCC_ON_DFS_CHAN_MAX          (1)
#define CFG_STA_SAP_SCC_ON_DFS_CHAN_DEFAULT      (0)

/*
 * <ini>
 * g_sta_sap_scc_on_lte_coex_chan - Allow STA+SAP SCC on LTE coex channel
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to allow STA+SAP SCC on LTE coex channel
 * 0 - Disallow STA+SAP SCC on LTE coex channel
 * 1 - Allow STA+SAP SCC on LTE coex channel
 *
 * Related: None.
 *
 * Supported Feature: Non-DBS, DBS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_STA_SAP_SCC_ON_LTE_COEX_CHAN              "g_sta_sap_scc_on_lte_coex_chan"
#define CFG_STA_SAP_SCC_ON_LTE_COEX_CHAN_MIN          (0)
#define CFG_STA_SAP_SCC_ON_LTE_COEX_CHAN_MAX          (1)
#define CFG_STA_SAP_SCC_ON_LTE_COEX_CHAN_DEFAULT      (1)

/*
 * <ini>
 * g_enable_go_force_scc - Enable/Disable force SCC on P2P GO
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini and along with "gWlanMccToSccSwitchMode" is used to enable
 * force SCC on P2P GO interface.
 *
 * Supported Feature: P2P GO
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_P2P_GO_ENABLE_FORCE_SCC              "g_enable_go_force_scc"
#define CFG_P2P_GO_ENABLE_FORCE_SCC_MIN          (0)
#define CFG_P2P_GO_ENABLE_FORCE_SCC_MAX          (1)
#define CFG_P2P_GO_ENABLE_FORCE_SCC_DEFAULT      (0)

/*
 * gPNOChannelPrediction will allow user to enable/disable the
 * PNO channel prediction feature.
 * In current PNO implementation, scan is always done until all configured
 * channels are scanned. If we can determine DUT is stationary based on
 * scanning a subset of channels, we may cancel the remaining channels.
 * Hence, we can save additional power consumption.
 */
#define CFG_PNO_CHANNEL_PREDICTION_NAME      "gPNOChannelPrediction"
#define CFG_PNO_CHANNEL_PREDICTION_MIN       (0)
#define CFG_PNO_CHANNEL_PREDICTION_MAX       (1)
#define CFG_PNO_CHANNEL_PREDICTION_DEFAULT   (0)
/*
 * The top K number of channels are used for tanimoto distance
 * calculation. These are the top channels on which the probability
 * of finding the AP's is extremely high. This number is intended
 * for tweaking the internal algorithm for experiments. This should
 * not be changed externally.
 */
#define CFG_TOP_K_NUM_OF_CHANNELS_NAME      "gTopKNumOfChannels"
#define CFG_TOP_K_NUM_OF_CHANNELS_MIN       (1)
#define CFG_TOP_K_NUM_OF_CHANNELS_MAX       (5)
#define CFG_TOP_K_NUM_OF_CHANNELS_DEFAULT   (3)
/*
 * This is the threshold value to determine that the STA is
 * stationary. If the tanimoto distance is less than this
 * value, then the device is considered to be stationary.
 * This parameter is intended to tweak the internal algorithm
 * for experiments. This should not be changed externally.
 */
#define CFG_STATIONARY_THRESHOLD_NAME      "gStationaryThreshold"
#define CFG_STATIONARY_THRESHOLD_MIN       (0)
#define CFG_STATIONARY_THRESHOLD_MAX       (100)
#define CFG_STATIONARY_THRESHOLD_DEFAULT   (10)

/* Option to report rssi in cfg80211_inform_bss_frame()
 * 0 = use rssi value based on noise floor = -96 dBm
 * 1 = use rssi value based on actual noise floor in hardware
 */
#define CFG_INFORM_BSS_RSSI_RAW_NAME               "gInformBssRssiRaw"
#define CFG_INFORM_BSS_RSSI_RAW_MIN                (0)
#define CFG_INFORM_BSS_RSSI_RAW_MAX                (1)
#define CFG_INFORM_BSS_RSSI_RAW_DEFAULT            (1)

/* GPIO pin to toggle when capture tsf */
#define CFG_SET_TSF_GPIO_PIN_NAME                  "gtsf_gpio_pin"
#define CFG_SET_TSF_GPIO_PIN_MIN                   (0)
#define CFG_SET_TSF_GPIO_PIN_MAX                   (254)
#define TSF_GPIO_PIN_INVALID                       (255)
#define CFG_SET_TSF_GPIO_PIN_DEFAULT               (TSF_GPIO_PIN_INVALID)

#ifdef WLAN_FEATURE_TSF_PLUS
/* PTP options */
#define CFG_SET_TSF_PTP_OPT_NAME                  "gtsf_ptp_options"
#define CFG_SET_TSF_PTP_OPT_MIN                   (0)
#define CFG_SET_TSF_PTP_OPT_MAX                   (0xff)
#define CFG_SET_TSF_PTP_OPT_RX                    (0x1)
#define CFG_SET_TSF_PTP_OPT_TX                    (0x2)
#define CFG_SET_TSF_PTP_OPT_RAW                   (0x4)
#define CFG_SET_TSF_DBG_FS                        (0x8)
#define CFG_SET_TSF_PTP_OPT_TSF64_TX            (0x10)
#define CFG_SET_TSF_PTP_OPT_DEFAULT               (0xf)
#endif

/*
 * <ini>
 * gThreeWayCoexConfigLegacyEnable - Enable coex config legacy feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable or disable three way coex config legacy feature.
 * This feature is designed only for non-mobile solution.
 * When the feature is disabled, Firmware use the default configuration to
 * set the coex priority of three antenna(WLAN, BT, ZIGBEE).
 * when enable this feature, customer can use the vendor command to set antenna
 * coex priority dynamically.
 *
 * Supported Feature: three way coex config
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_TW_COEX_LEGACY_NAME  "gThreeWayCoexConfigLegacyEnable"
#define CFG_ENABLE_TW_COEX_LEGACY_MIN     (0)
#define CFG_ENABLE_TW_COEX_LEGACY_MAX     (1)
#define CFG_ENABLE_TW_COEX_LEGACY_DEFAULT (0)

/*
 * Dense traffic threshold
 * traffic threshold required for dense roam scan
 * Measured in kbps
 */
#define CFG_ROAM_DENSE_TRAFFIC_THRESHOLD         "gtraffic_threshold"
#define CFG_ROAM_DENSE_TRAFFIC_THRESHOLD_MIN     (0)
#define CFG_ROAM_DENSE_TRAFFIC_THRESHOLD_MAX     (0xffffffff)
#define CFG_ROAM_DENSE_TRAFFIC_THRESHOLD_DEFAULT (400)

/*
 * <ini>
 * gvendor_acs_support - vendor based channel selection manager
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Enabling this parameter will force driver to use user application based
 * channel selection algo instead of driver based auto channel selection
 * logic.
 *
 * Supported Feature: ACS
 *
 * Usage: External/Internal
 *
 * </ini>
 */
#define CFG_USER_AUTO_CHANNEL_SELECTION       "gvendor_acs_support"
#define CFG_USER_AUTO_CHANNEL_SELECTION_DISABLE   (0)
#define CFG_USER_AUTO_CHANNEL_SELECTION_ENABLE    (1)
#define CFG_USER_AUTO_CHANNEL_SELECTION_DEFAULT   (0)

/*
 * <ini>
 * gacs_support_for_dfs_lte_coex - acs support for lte coex and dfs event
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Enabling this parameter will force driver to use user application based
 * channel selection algo for channel selection in case of dfs and lte
 * coex event.
 *
 * Supported Feature: ACS
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_USER_ACS_DFS_LTE           "gacs_support_for_dfs_lte_coex"
#define CFG_USER_ACS_DFS_LTE_DISABLE   (0)
#define CFG_USER_ACS_DFS_LTE_ENABLE    (1)
#define CFG_USER_ACS_DFS_LTE_DEFAULT   (0)

/*
 * Enabling gignore_peer_ht_opmode will enable 11g
 * protection only when there is a 11g AP in vicinity.
 */
#define CFG_IGNORE_PEER_HT_MODE_NAME       "gignore_peer_ht_opmode"
#define CFG_IGNORE_PEER_HT_MODE_MIN        (0)
#define CFG_IGNORE_PEER_HT_MODE_MAX        (1)
#define CFG_IGNORE_PEER_HT_MODE_DEFAULT    (0)

#ifdef WLAN_FEATURE_NAN_DATAPATH
/*
 * Enable NaN data path feature. NaN data path enables
 * NaN supported devices to exchange data over traditional
 * TCP/UDP network stack.
 */
#define CFG_ENABLE_NAN_DATAPATH_NAME    "genable_nan_datapath"
#define CFG_ENABLE_NAN_DATAPATH_MIN     (0)
#define CFG_ENABLE_NAN_DATAPATH_MAX     (1)
#define CFG_ENABLE_NAN_DATAPATH_DEFAULT (0)

/*
 * NAN channel on which NAN data interface to start
 */
#define CFG_ENABLE_NAN_NDI_CHANNEL_NAME    "gnan_datapath_ndi_channel"
#define CFG_ENABLE_NAN_NDI_CHANNEL_MIN     (6)
#define CFG_ENABLE_NAN_NDI_CHANNEL_MAX     (149)
#define CFG_ENABLE_NAN_NDI_CHANNEL_DEFAULT (6)
#endif

/*
 * Enable/Disable to initiate BUG report in case of fatal event
 * Default: Enable
 */
#define CFG_ENABLE_FATAL_EVENT_TRIGGER                 "gEnableFatalEvent"
#define CFG_ENABLE_FATAL_EVENT_TRIGGER_MIN             (0)
#define CFG_ENABLE_FATAL_EVENT_TRIGGER_MAX             (1)
#define CFG_ENABLE_FATAL_EVENT_TRIGGER_DEFAULT         (1)

/*
 * <ini>
 * gEnableEdcaParams - Enable edca parameter
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used if gEnableEdcaParams is set to 1, params gEdcaVoCwmin,
 * gEdcaViCwmin, gEdcaBkCwmin, gEdcaBeCwmin, gEdcaVoCwmax,
 * gEdcaViCwmax, gEdcaBkCwmax, gEdcaBeCwmax, gEdcaVoAifs,
 * gEdcaViAifs, gEdcaBkAifs and gEdcaBeAifs values are used
 * to overwrite the values received from AP
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ENABLE_EDCA_INI_NAME       "gEnableEdcaParams"
#define CFG_ENABLE_EDCA_INI_MIN        (0)
#define CFG_ENABLE_EDCA_INI_MAX        (1)
#define CFG_ENABLE_EDCA_INI_DEFAULT    (0)

/*
 * <ini>
 * gEdcaVoCwmin - Set Cwmin value for EDCA_AC_VO
 * @Min: 0
 * @Max: 0x15
 * @Default: 2
 *
 * This ini is used to set default Cwmin value for EDCA_AC_VO
 * Cwmin value for EDCA_AC_VO. CWVomin = 2^gEdcaVoCwmin -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin etc
 * are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_VO_CWMIN_VALUE_NAME      "gEdcaVoCwmin"
#define CFG_EDCA_VO_CWMIN_VALUE_MIN       (0x0)
#define CFG_EDCA_VO_CWMIN_VALUE_MAX       (15)
#define CFG_EDCA_VO_CWMIN_VALUE_DEFAULT   (2)

/*
 * <ini>
 * gEdcaViCwmin - Set Cwmin value for EDCA_AC_VI
 * @Min: 0x0
 * @Max: 15
 * @Default: 3
 *
 * This ini is used to set default value for EDCA_AC_VI
 * Cwmin value for EDCA_AC_VI. CWVimin = 2^gEdcaViCwmin -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_VI_CWMIN_VALUE_NAME      "gEdcaViCwmin"
#define CFG_EDCA_VI_CWMIN_VALUE_MIN       (0x0)
#define CFG_EDCA_VI_CWMIN_VALUE_MAX       (15)
#define CFG_EDCA_VI_CWMIN_VALUE_DEFAULT   (3)

/*
 * <ini>
 * gEdcaBkCwmin - Set Cwmin value for EDCA_AC_BK
 * @Min: 0x0
 * @Max: 15
 * @Default: 4
 *
 * This ini is used to set default Cwmin value for EDCA_AC_BK
 * Cwmin value for EDCA_AC_BK. CWBkmin = 2^gEdcaBkCwmin -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 * </ini>
 */

#define CFG_EDCA_BK_CWMIN_VALUE_NAME      "gEdcaBkCwmin"
#define CFG_EDCA_BK_CWMIN_VALUE_MIN       (0x0)
#define CFG_EDCA_BK_CWMIN_VALUE_MAX       (15)
#define CFG_EDCA_BK_CWMIN_VALUE_DEFAULT   (4)

/*
 * <ini>
 * gEdcaBeCwmin - Set Cwmin value for EDCA_AC_BE
 * @Min: 0x0
 * @Max: 15
 * @Default: 4
 *
 * This ini is used to set default Cwmin value for EDCA_AC_BE
 * Cwmin value for EDCA_AC_BE. CWBemin = 2^gEdcaBeCwmin
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_BE_CWMIN_VALUE_NAME      "gEdcaBeCwmin"
#define CFG_EDCA_BE_CWMIN_VALUE_MIN       (0x0)
#define CFG_EDCA_BE_CWMIN_VALUE_MAX       (15)
#define CFG_EDCA_BE_CWMIN_VALUE_DEFAULT   (4)

/*
 * <ini>
 * gEdcaVoCwmax - Set Cwmax value for EDCA_AC_VO
 * @Min: 0
 * @Max: 15
 * @Default: 3
 *
 * This ini is used to set default Cwmax value for EDCA_AC_VO
 * Cwmax value for EDCA_AC_VO. CWVomax = 2^gEdcaVoCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_VO_CWMAX_VALUE_NAME      "gEdcaVoCwmax"
#define CFG_EDCA_VO_CWMAX_VALUE_MIN       (0)
#define CFG_EDCA_VO_CWMAX_VALUE_MAX       (15)
#define CFG_EDCA_VO_CWMAX_VALUE_DEFAULT   (3)

/*
 * <ini>
 * gEdcaViCwmax - Set Cwmax value for EDCA_AC_VI
 * @Min: 0
 * @Max: 15
 * @Default: 4
 *
 * This ini is used to set default Cwmax value for EDCA_AC_VI
 * Cwmax value for EDCA_AC_VI. CWVimax = 2^gEdcaViCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_VI_CWMAX_VALUE_NAME      "gEdcaViCwmax"
#define CFG_EDCA_VI_CWMAX_VALUE_MIN       (0)
#define CFG_EDCA_VI_CWMAX_VALUE_MAX       (15)
#define CFG_EDCA_VI_CWMAX_VALUE_DEFAULT   (4)

/*
 * <ini>
 * gEdcaBkCwmax - Set Cwmax value for EDCA_AC_BK
 * @Min: 0
 * @Max: 15
 * @Default: 10
 *
 * This ini is used to set default Cwmax value for EDCA_AC_BK
 * Cwmax value for EDCA_AC_BK. CWBkmax = 2^gEdcaBkCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_BK_CWMAX_VALUE_NAME      "gEdcaBkCwmax"
#define CFG_EDCA_BK_CWMAX_VALUE_MIN       (0)
#define CFG_EDCA_BK_CWMAX_VALUE_MAX       (15)
#define CFG_EDCA_BK_CWMAX_VALUE_DEFAULT   (10)

/*
 * <ini>
 * gEdcaBeCwmax - Set Cwmax value for EDCA_AC_BE
 * @Min: 0
 * @Max: 15
 * @Default: 10
 *
 * This ini is used to set default Cwmax value for EDCA_AC_BE
 * Cwmax value for EDCA_AC_BE. CWBemax = 2^gEdcaBeCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_BE_CWMAX_VALUE_NAME      "gEdcaBeCwmax"
#define CFG_EDCA_BE_CWMAX_VALUE_MIN       (0)
#define CFG_EDCA_BE_CWMAX_VALUE_MAX       (15)
#define CFG_EDCA_BE_CWMAX_VALUE_DEFAULT   (10)

/*
 * <ini>
 * gEdcaVoAifs - Set Aifs value for EDCA_AC_VO
 * @Min: 0
 * @Max: 15
 * @Default: 2
 *
 * This ini is used to set default Aifs value for EDCA_AC_VO
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_VO_AIFS_VALUE_NAME       "gEdcaVoAifs"
#define CFG_EDCA_VO_AIFS_VALUE_MIN        (0)
#define CFG_EDCA_VO_AIFS_VALUE_MAX        (15)
#define CFG_EDCA_VO_AIFS_VALUE_DEFAULT    (2)

/*
 * <ini>
 * gEdcaViAifs - Set Aifs value for EDCA_AC_VI
 * @Min: 0
 * @Max: 15
 * @Default: 2
 *
 * This ini is used to set default Aifs value for EDCA_AC_VI
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_VI_AIFS_VALUE_NAME       "gEdcaViAifs"
#define CFG_EDCA_VI_AIFS_VALUE_MIN        (0)
#define CFG_EDCA_VI_AIFS_VALUE_MAX        (15)
#define CFG_EDCA_VI_AIFS_VALUE_DEFAULT    (2)

/*
 * <ini>
 * gEdcaBkAifs - Set Aifs value for EDCA_AC_BK
 * @Min: 0
 * @Max: 15
 * @Default: 7
 *
 * This ini is used to set default Aifs value for EDCA_AC_BK
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_BK_AIFS_VALUE_NAME       "gEdcaBkAifs"
#define CFG_EDCA_BK_AIFS_VALUE_MIN        (0)
#define CFG_EDCA_BK_AIFS_VALUE_MAX        (15)
#define CFG_EDCA_BK_AIFS_VALUE_DEFAULT    (7)

/*
 * <ini>
 * gEdcaBeAifs - Set Aifs value for EDCA_AC_BE
 * @Min: 0
 * @Max: 15
 * @Default: 3
 *
 * This ini is used to set default Aifs value for EDCA_AC_BE
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_BE_AIFS_VALUE_NAME       "gEdcaBeAifs"
#define CFG_EDCA_BE_AIFS_VALUE_MIN        (0)
#define CFG_EDCA_BE_AIFS_VALUE_MAX        (15)
#define CFG_EDCA_BE_AIFS_VALUE_DEFAULT    (3)

/*
 * <ini>
 * gEnableTxSchedWrrVO - Set TX sched parameters for VO
 * @Default:
 *
 * This key is mapping to VO defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for VO.
 * e.g., gEnableTxSchedWrrVO = 10, 9, 8, 1, 8
 */
#define CFG_ENABLE_TX_SCHED_WRR_VO_NAME     "gEnableTxSchedWrrVO"
#define CFG_ENABLE_TX_SCHED_WRR_VO_DEFAULT  ""

/*
 * <ini>
 * gEnableTxSchedWrrVI - Set TX sched parameters for VI
 * @Default:
 *
 * This key is mapping to VI defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for VI.
 * e.g., gEnableTxSchedWrrVI = 10, 9, 8, 1, 8
 */
#define CFG_ENABLE_TX_SCHED_WRR_VI_NAME     "gEnableTxSchedWrrVI"
#define CFG_ENABLE_TX_SCHED_WRR_VI_DEFAULT  ""

/*
 * <ini>
 * gEnableTxSchedWrrBE - Set TX sched parameters for BE
 * @Default:
 *
 * This key is mapping to BE defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for BE.
 * e.g., gEnableTxSchedWrrBE = 10, 9, 8, 1, 8
 */
#define CFG_ENABLE_TX_SCHED_WRR_BE_NAME     "gEnableTxSchedWrrBE"
#define CFG_ENABLE_TX_SCHED_WRR_BE_DEFAULT  ""

/*
 * <ini>
 * gEnableTxSchedWrrBK - Set TX sched parameters for BK
 * @Default:
 *
 * This key is mapping to BK defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for BK.
 * e.g., gEnableTxSchedWrrBK = 10, 9, 8, 1, 8
 */
#define CFG_ENABLE_TX_SCHED_WRR_BK_NAME     "gEnableTxSchedWrrBK"
#define CFG_ENABLE_TX_SCHED_WRR_BK_DEFAULT  ""

#ifdef CONFIG_DP_TRACE
/*
 * Enable/disable DPTRACE
 * Enabling this might have performace impact.
 */
#define CFG_ENABLE_DP_TRACE		"enable_dp_trace"
#define CFG_ENABLE_DP_TRACE_MIN		(0)
#define CFG_ENABLE_DP_TRACE_MAX	(1)
#define CFG_ENABLE_DP_TRACE_DEFAULT	(1)

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
/*
 * Config DPTRACE
 * The sequence of params is important. If some param is missing, defaults are
 * considered.
 * Param 1: Enable/Disable DP Trace live mode (uint8_t)
 * Param 2: DP Trace live mode high bandwidth thresh.(uint8_t)
 *         (packets/second) beyond which DP Trace is disabled. Decimal Val.
 *          MGMT, DHCP, EAPOL, ARP pkts are not counted. ICMP and Data are.
 * Param 3: Default Verbosity (0-4)
 * Param 4: Proto Bitmap (uint8_t). Decimal Value.
 *          (decimal 62 = 0x3e)
 * e.g., to disable live mode, use the following param in the ini file.
 * gDptraceConfig = 0
 * e.g., to enable dptrace live mode and set the thresh as 6,
 * use the following param in the ini file.
 * gDptraceConfig = 1, 6
 */
#define CFG_ENABLE_DP_TRACE_CONFIG		"gDptraceConfig"
#define CFG_ENABLE_DP_TRACE_CONFIG_DEFAULT	"1, 6, 2, 126"

/*
 * <ini>
 * dp_proto_event_bitmap - Control for which protocol packet diag event should
 *  be sent to user space.
 * @Min: 0
 * @Max: 0x17
 * @Default: 0x6
 *
 * This ini is used to control for which protocol packet diag event should be
 * sent to user space.
 *
 * QDF_NBUF_PKT_TRAC_TYPE_DNS       0x01
 * QDF_NBUF_PKT_TRAC_TYPE_EAPOL     0x02
 * QDF_NBUF_PKT_TRAC_TYPE_DHCP      0x04
 * QDF_NBUF_PKT_TRAC_TYPE_ARP       0x10
 *
 * Related: None
 *
 * Supported Feature: STA, SAP
 *
 * Usage: Internal
 *
 * <ini>
 */
#define CFG_DP_PROTO_EVENT_BITMAP		"dp_proto_event_bitmap"
#define CFG_DP_PROTO_EVENT_BITMAP_MIN		(0x0)
#define CFG_DP_PROTO_EVENT_BITMAP_MAX		(0x17)
#define CFG_DP_PROTO_EVENT_BITMAP_DEFAULT	(0x17)
#endif

/*
 * This parameter will set the weight to calculate the average low pass
 * filter for channel congestion.
 * Acceptable values for this: 0-100 (In %)
 */
#define CFG_ADAPT_DWELL_LPF_WEIGHT_NAME       "adapt_dwell_lpf_weight"
#define CFG_ADAPT_DWELL_LPF_WEIGHT_MIN        (0)
#define CFG_ADAPT_DWELL_LPF_WEIGHT_MAX        (100)
#define CFG_ADAPT_DWELL_LPF_WEIGHT_DEFAULT    (80)

/*
 * This parameter will set interval to monitor wifi activity
 * in passive scan in msec.
 * Acceptable values for this: 0-25
 */
#define CFG_ADAPT_DWELL_PASMON_INTVAL_NAME     "adapt_dwell_passive_mon_intval"
#define CFG_ADAPT_DWELL_PASMON_INTVAL_MIN      (0)
#define CFG_ADAPT_DWELL_PASMON_INTVAL_MAX      (25)
#define CFG_ADAPT_DWELL_PASMON_INTVAL_DEFAULT  (10)

/*
 * This parameter will set % of wifi activity used in passive scan 0-100.
 * Acceptable values for this: 0-100 (in %)
 */
#define CFG_ADAPT_DWELL_WIFI_THRESH_NAME       "adapt_dwell_wifi_act_threshold"
#define CFG_ADAPT_DWELL_WIFI_THRESH_MIN        (0)
#define CFG_ADAPT_DWELL_WIFI_THRESH_MAX        (100)
#define CFG_ADAPT_DWELL_WIFI_THRESH_DEFAULT    (10)

/*
 * <ini>
 * g_bug_on_reinit_failure  - Enable/Disable bug on reinit
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to debug ssr reinit failure issues by raising vos bug so
 * dumps can be collected.
 * g_bug_on_reinit_failure = 0 wlan driver will only recover after driver
 * unload and load
 * g_bug_on_reinit_failure = 1 raise vos bug to collect dumps
 *
 * Related: gEnableSSR
 *
 * Supported Feature: SSR
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BUG_ON_REINIT_FAILURE_NAME     "g_bug_on_reinit_failure"
#define CFG_BUG_ON_REINIT_FAILURE_MIN      (0)
#define CFG_BUG_ON_REINIT_FAILURE_MAX      (1)
#define CFG_BUG_ON_REINIT_FAILURE_DEFAULT  (0)

/*
 * <ini>
 * gSub20ChannelWidth - Control sub 20 channel width (5/10 Mhz)
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to set the sub 20 channel width.
 * gSub20ChannelWidth=0: indicates do not use Sub 20 MHz bandwidth
 * gSub20ChannelWidth=1: Bring up SAP/STA in 5 MHz bandwidth
 * gSub20ChannelWidth=2: Bring up SAP/STA in 10 MHz bandwidth
 *
 * Related: None
 *
 * Supported Feature: 5/10 Mhz channel width support
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SUB_20_CHANNEL_WIDTH_NAME              "gSub20ChannelWidth"
#define CFG_SUB_20_CHANNEL_WIDTH_MIN               (WLAN_SUB_20_CH_WIDTH_NONE)
#define CFG_SUB_20_CHANNEL_WIDTH_MAX               (WLAN_SUB_20_CH_WIDTH_10)
#define CFG_SUB_20_CHANNEL_WIDTH_DEFAULT           (WLAN_SUB_20_CH_WIDTH_NONE)

#define CFG_TGT_GTX_USR_CFG_NAME    "tgt_gtx_usr_cfg"
#define CFG_TGT_GTX_USR_CFG_MIN     (0)
#define CFG_TGT_GTX_USR_CFG_MAX     (32)
#define CFG_TGT_GTX_USR_CFG_DEFAULT (32)

#define CFG_SAP_INTERNAL_RESTART_NAME    "gEnableSapInternalRestart"
#define CFG_SAP_INTERNAL_RESTART_MIN     (0)
#define CFG_SAP_INTERNAL_RESTART_MAX     (1)
#define CFG_SAP_INTERNAL_RESTART_DEFAULT (1)

/*
 * <ini>
 * restart_beaconing_on_chan_avoid_event - control the beaconing entity to move
 * away from active LTE channels
 * @Min: 0
 * @Max: 2
 * @Default: 1
 *
 * This ini is used to control the beaconing entity (SAP/GO) to move away from
 * active LTE channels when channel avoidance event is received
 * restart_beaconing_on_chan_avoid_event=0: Don't allow beaconing entity move
 * from active LTE channels
 * restart_beaconing_on_chan_avoid_event=1: Allow beaconing entity move from
 * active LTE channels
 * restart_beaconing_on_chan_avoid_event=2: Allow beaconing entity move from
 * 2.4G active LTE channels only
 *
 * Related: None
 *
 * Supported Feature: channel avoidance
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RESTART_BEACONING_ON_CH_AVOID_NAME    "restart_beaconing_on_chan_avoid_event"
#define CFG_RESTART_BEACONING_ON_CH_AVOID_MIN     (CH_AVOID_RULE_DO_NOT_RESTART)
#define CFG_RESTART_BEACONING_ON_CH_AVOID_MAX     (CH_AVOID_RULE_RESTART_24G_ONLY)
#define CFG_RESTART_BEACONING_ON_CH_AVOID_DEFAULT (CH_AVOID_RULE_RESTART)
/*
 * This parameter will avoid updating ap_sta_inactivity from hostapd.conf
 * file. If a station does not send anything in ap_max_inactivity seconds, an
 * empty data frame is sent to it in order to verify whether it is
 * still in range. If this frame is not ACKed, the station will be
 * disassociated and then deauthenticated. This feature is used to
 * clear station table of old entries when the STAs move out of the
 * range.
 * Default : Disable
 */
#define CFG_SAP_MAX_INACTIVITY_OVERRIDE_NAME     "gSapMaxInactivityOverride"
#define CFG_SAP_MAX_INACTIVITY_OVERRIDE_MIN      (0)
#define CFG_SAP_MAX_INACTIVITY_OVERRIDE_MAX      (1)
#define CFG_SAP_MAX_INACTIVITY_OVERRIDE_DEFAULT  (0)

/*
 * <ini>
 * rx_mode - Control to decide rx mode
 *
 * @Min: 0
 * @Max: (CFG_ENABLE_RX_THREAD | CFG_ENABLE_RPS | CFG_ENABLE_NAPI | \
 *        CFG_ENABLE_DYNAMIC_RPS)
 * @Default: MDM_PLATFORM   -  0
 *           HELIUMPLUS     -  CFG_ENABLE_NAPI
 *           Other cases    -  (CFG_ENABLE_RX_THREAD | CFG_ENABLE_NAPI)
 *
 * This ini is used to decide mode for the rx path
 *
 * Supported Feature: NAPI
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_RX_MODE_NAME     "rx_mode"
#define CFG_RX_MODE_MIN      (0)
#define CFG_RX_MODE_MAX      (CFG_ENABLE_RX_THREAD | CFG_ENABLE_RPS | \
				 CFG_ENABLE_NAPI | CFG_ENABLE_DYNAMIC_RPS)
#ifdef MDM_PLATFORM
#define CFG_RX_MODE_DEFAULT  (0)
#elif defined(HELIUMPLUS)
#define CFG_RX_MODE_DEFAULT  CFG_ENABLE_NAPI
#else
#define CFG_RX_MODE_DEFAULT  (CFG_ENABLE_RX_THREAD | CFG_ENABLE_NAPI)
#endif

/*
 * <ini>
 * ce_service_max_yield_time - Control to set ce service max yield time (in us)
 *
 * @Min: 500
 * @Max: 10000
 * @Default: 10000
 *
 * This ini is used to set ce service max yield time (in us)
 *
 * Supported Feature: NAPI
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_CE_SERVICE_MAX_YIELD_TIME_NAME     "ce_service_max_yield_time"
#define CFG_CE_SERVICE_MAX_YIELD_TIME_MIN      (500)
#define CFG_CE_SERVICE_MAX_YIELD_TIME_MAX      (10000)
#define CFG_CE_SERVICE_MAX_YIELD_TIME_DEFAULT  (10000)

/*
 * <ini>
 * ce_service_max_rx_ind_flush - Control to set ce service max rx ind flush
 *
 * @Min: 1
 * @Max: 32
 * @Default: 1
 *
 * This ini is used to set ce service max rx ind flush
 *
 * Supported Feature: NAPI
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_CE_SERVICE_MAX_RX_IND_FLUSH_NAME     "ce_service_max_rx_ind_flush"
#define CFG_CE_SERVICE_MAX_RX_IND_FLUSH_MIN      (1)
#define CFG_CE_SERVICE_MAX_RX_IND_FLUSH_MAX      (32)
#define CFG_CE_SERVICE_MAX_RX_IND_FLUSH_DEFAULT  (32)

/*
 * <ini>
 * NAPI_CPU_AFFINITY_MASK - CPU mask to affine NAPIs
 *
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0
 *
 * This ini is used to set NAPI IRQ CPU affinity
 *
 * Supported Feature: NAPI
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_NAPI_CE_CPU_MASK_NAME	"NAPI_CPU_AFFINITY_MASK"
#define CFG_NAPI_CE_CPU_MASK_MIN	(0)
#define CFG_NAPI_CE_CPU_MASK_MAX	(0xFF)
#define CFG_NAPI_CE_CPU_MASK_DEFAULT	(0)

/*
 * <ini>
 * RX_THREAD_CPU_AFFINITY_MASK - CPU mask to affine Rx_thread
 *
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0
 *
 * This ini is used to set Rx_thread CPU affinity
 *
 * Supported Feature: Rx_thread
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_RX_THREAD_CPU_MASK_NAME	"RX_THREAD_CPU_AFFINITY_MASK"
#define CFG_RX_THREAD_CPU_MASK_MIN	(0)
#define CFG_RX_THREAD_CPU_MASK_MAX	(0xFF)
#ifdef RX_PERFORMANCE
#define CFG_RX_THREAD_CPU_MASK_DEFAULT	(0xFE)
#else
#define CFG_RX_THREAD_CPU_MASK_DEFAULT	(0)
#endif

/*
 * <ini>
 * RX_THREAD_UL_CPU_AFFINITY_MASK - CPU mask to affine Rx_thread
 *
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0x0
 *
 * This ini is used to set Rx_thread CPU affinity for uplink traffic
 *
 * Supported Feature: Rx_thread
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_RX_THREAD_UL_CPU_MASK_NAME    "RX_THREAD_UL_CPU_AFFINITY_MASK"
#define CFG_RX_THREAD_UL_CPU_MASK_MIN     (0)
#define CFG_RX_THREAD_UL_CPU_MASK_MAX     (0xFF)
#define CFG_RX_THREAD_UL_CPU_MASK_DEFAULT (0)

/* List of RPS CPU maps for different rx queues registered by WLAN driver
 * Ref - Kernel/Documentation/networking/scaling.txt
 * RPS CPU map for a particular RX queue, selects CPU(s) for bottom half
 * processing of RX packets. For example, for a system with 4 CPUs,
 * 0xe: Use CPU1 - CPU3 and donot use CPU0.
 * 0x0: RPS is disabled, packets are processed on the interrupting CPU.
.*
 * WLAN driver registers NUM_TX_QUEUES queues for tx and rx each during
 * alloc_netdev_mq. Hence, we need to have a cpu mask for each of the rx queues.
 *
 * For example, if the NUM_TX_QUEUES is 4, a sample WLAN ini entry may look like
 * rpsRxQueueCpuMapList=a b c d
 * For a 4 CPU system (CPU0 - CPU3), this implies:
 * 0xa - (1010) use CPU1, CPU3 for rx queue 0
 * 0xb - (1011) use CPU0, CPU1 and CPU3 for rx queue 1
 * 0xc - (1100) use CPU2, CPU3 for rx queue 2
 * 0xd - (1101) use CPU0, CPU2 and CPU3 for rx queue 3

 * In practice, we may want to avoid the cores which are heavily loaded.
 */

/* Name of the ini file entry to specify RPS map for different RX queus */
#define CFG_RPS_RX_QUEUE_CPU_MAP_LIST_NAME         "rpsRxQueueCpuMapList"

/* Default value of rpsRxQueueCpuMapList. Different platforms may have
 * different configurations for NUM_TX_QUEUES and # of cpus, and will need to
 * configure an appropriate value via ini file. Setting default value to 'e' to
 * avoid use of CPU0 (since its heavily used by other system processes) by rx
 * queue 0, which is currently being used for rx packet processing.
 */
#define CFG_RPS_RX_QUEUE_CPU_MAP_LIST_DEFAULT      "e"

/* Maximum length of string used to hold a list of cpu maps for various rx
 * queues. Considering a 16 core system with 5 rx queues, a RPS CPU map
 * list may look like -
 * rpsRxQueueCpuMapList = ffff ffff ffff ffff ffff
 * (all 5 rx queues can be processed on all 16 cores)
 * max string len = 24 + 1(for '\0'). Considering 30 to be on safe side.
 */
#define CFG_RPS_RX_QUEUE_CPU_MAP_LIST_LEN 30

#ifdef WLAN_FEATURE_WOW_PULSE
/*
 * Enable/Disable  WOW PULSE feature
 * Set the wakeup pulse which FW use to wake up HOST
 * Default : Disable
 */
#define CFG_WOW_PULSE_SUPPORT_NAME     "gwow_pulse_support"
#define CFG_WOW_PULSE_SUPPORT_MIN      (0)
#define CFG_WOW_PULSE_SUPPORT_MAX      (1)
#define CFG_WOW_PULSE_SUPPORT_DEFAULT  (CFG_WOW_PULSE_SUPPORT_MIN)

/*
 * GPIO PIN for Pulse
 * Which PIN to send the Pulse
 */
#define CFG_WOW_PULSE_PIN_NAME         "gwow_pulse_pin"
#define CFG_WOW_PULSE_PIN_MIN          (CFG_SET_TSF_GPIO_PIN_MIN)
#define CFG_WOW_PULSE_PIN_MAX          (CFG_SET_TSF_GPIO_PIN_MAX)
#define CFG_WOW_PULSE_PIN_DEFAULT      (35)

/*
 * Pulse interval low
 * The interval of low level in the pulse
 * The value which defined by customer should between 160 and 480
 */
#define CFG_WOW_PULSE_INTERVAL_LOW_NAME     "gwow_pulse_interval_low"
#define CFG_WOW_PULSE_INTERVAL_LOW_MIN      (160)
#define CFG_WOW_PULSE_INTERVAL_LOW_MAX      (480)
#define CFG_WOW_PULSE_INTERVAL_LOW_DEFAULT  (180)

/*
 * Pulse interval high
 * The interval of high level in the pulse
 * The value which defined by customer should between 20 and 40
 */
#define CFG_WOW_PULSE_INTERVAL_HIGH_NAME    "gwow_pulse_interval_high"
#define CFG_WOW_PULSE_INTERVAL_HIGH_MIN     (20)
#define CFG_WOW_PULSE_INTERVAL_HIGH_MAX     (40)
#define CFG_WOW_PULSE_INTERVAL_HIGH_DEFAULT (20)
#endif

/*
 * Support to start sap in indoor channel
 * Customer can config this item to enable/disable sap in indoor channel
 * Default: Disable
 */
#define CFG_INDOOR_CHANNEL_SUPPORT_NAME     "gindoor_channel_support"
#define CFG_INDOOR_CHANNEL_SUPPORT_MIN      (0)
#define CFG_INDOOR_CHANNEL_SUPPORT_MAX      (1)
#define CFG_INDOOR_CHANNEL_SUPPORT_DEFAULT  (0)

/*
 * <ini>
 * g_mark_sap_indoor_as_disable - Enable/Disable Indoor channel
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to mark the Indoor channel as
 * disable when SAP start and revert it on SAP stop,
 * so SAP will not turn on indoor channel and
 * sta will not scan/associate and roam on indoor
 * channels.
 *
 * Related: If g_mark_sap_indoor_as_disable set, turn the
 * indoor channels to disable and update Wiphy & fw.
 *
 * Supported Feature: SAP/STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MARK_INDOOR_AS_DISABLE_NAME     "g_mark_sap_indoor_as_disable"
#define CFG_MARK_INDOOR_AS_DISABLE_MIN      (0)
#define CFG_MARK_INDOOR_AS_DISABLE_MAX      (1)
#define CFG_MARK_INDOOR_AS_DISABLE_DEFAULT  (0)

/*
 * Force softap to 11n, when gSapForce11NFor11AC is set to 1 from ini
 * despite of hostapd.conf request for 11ac
 */
#define CFG_SAP_FORCE_11N_FOR_11AC_NAME    "gSapForce11NFor11AC"
#define CFG_SAP_FORCE_11N_FOR_11AC_MIN     (0)
#define CFG_SAP_FORCE_11N_FOR_11AC_MAX     (1)
#define CFG_SAP_FORCE_11N_FOR_11AC_DEFAULT (0)

#define CFG_GO_FORCE_11N_FOR_11AC_NAME    "gGoForce11NFor11AC"
#define CFG_GO_FORCE_11N_FOR_11AC_MIN     (0)
#define CFG_GO_FORCE_11N_FOR_11AC_MAX     (1)
#define CFG_GO_FORCE_11N_FOR_11AC_DEFAULT (0)

/*
 * sap tx leakage threshold
 * customer can set this value from 100 to 1000 which means
 * sap tx leakage threshold is -10db to -100db
 */
#define CFG_SAP_TX_LEAKAGE_THRESHOLD_NAME    "gsap_tx_leakage_threshold"
#define CFG_SAP_TX_LEAKAGE_THRESHOLD_MIN     (100)
#define CFG_SAP_TX_LEAKAGE_THRESHOLD_MAX     (1000)
#define CFG_SAP_TX_LEAKAGE_THRESHOLD_DEFAULT (310)


/*
 * Enable filtering of replayed multicast packets
 * In a typical infrastructure setup, it is quite normal to receive
 * replayed multicast packets. These packets may cause more harm than
 * help if not handled properly. Providing a configuration option
 * to enable filtering of such packets
 */
#define CFG_FILTER_MULTICAST_REPLAY_NAME    "enable_multicast_replay_filter"
#define CFG_FILTER_MULTICAST_REPLAY_MIN      (0)
#define CFG_FILTER_MULTICAST_REPLAY_MAX      (1)
#define CFG_FILTER_MULTICAST_REPLAY_DEFAULT  (1)

/* Optimize channel avoidance indication coming from firmware */
#define CFG_OPTIMIZE_CA_EVENT_NAME       "goptimize_chan_avoid_event"
#define CFG_OPTIMIZE_CA_EVENT_DISABLE    (0)
#define CFG_OPTIMIZE_CA_EVENT_ENABLE     (1)
#define CFG_OPTIMIZE_CA_EVENT_DEFAULT    (0)

/*
 * <ini>
 * fw_timeout_crash - Enable/Disable BUG ON
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to Trigger host crash when firmware fails to send the
 * response to host
 * fw_timeout_crash = 0 Disabled
 * fw_timeout_crash = 1 Trigger host crash
 *
 * Related: None
 *
 * Supported Feature: SSR
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_CRASH_FW_TIMEOUT_NAME       "fw_timeout_crash"
#define CFG_CRASH_FW_TIMEOUT_DISABLE    (0)
#define CFG_CRASH_FW_TIMEOUT_ENABLE     (1)
#define CFG_CRASH_FW_TIMEOUT_DEFAULT    (1)

/*
 * <ini>
 * rx_wakelock_timeout - Amount of time to hold wakelock for RX unicast packets
 * @Min: 0
 * @Max: 100
 * @Default: 50
 *
 * This ini item configures the amount of time, in milliseconds, that the driver
 * should prevent system power collapse after receiving an RX unicast packet.
 * A conigured value of 0 disables the RX Wakelock feature completely.
 *
 * Related: None.
 *
 * Supported Feature: RX Wakelock
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RX_WAKELOCK_TIMEOUT_NAME     "rx_wakelock_timeout"
#define CFG_RX_WAKELOCK_TIMEOUT_DEFAULT  (50)
#define CFG_RX_WAKELOCK_TIMEOUT_MIN      (0)
#define CFG_RX_WAKELOCK_TIMEOUT_MAX      (100)

/*
 * <ini>
 * enable_5g_band_pref - Enable preference for 5G from INI.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 * This ini is used to enable 5G preference parameters.
 *
 * Related: 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_5G_BAND_PREF_NAME             "enable_5g_band_pref"
#define CFG_ENABLE_5G_BAND_PREF_MIN              (0)
#define CFG_ENABLE_5G_BAND_PREF_MAX              (1)
#define CFG_ENABLE_5G_BAND_PREF_DEFAULT          (0)

/*
 * <ini>
 * 5g_rssi_boost_threshold - A_band_boost_threshold above which 5 GHz is favored.
 * @Min: -70
 * @Max: -55
 * @Default: -60
 * This ini is used to set threshold for 5GHz band preference.
 *
 * Related: 5g_rssi_boost_factor, 5g_max_rssi_boost
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_5G_RSSI_BOOST_THRESHOLD_NAME         "5g_rssi_boost_threshold"
#define CFG_5G_RSSI_BOOST_THRESHOLD_MIN          (-70)
#define CFG_5G_RSSI_BOOST_THRESHOLD_MAX          (-55)
#define CFG_5G_RSSI_BOOST_THRESHOLD_DEFAULT      (-60)

/*
 * <ini>
 * 5g_rssi_boost_factor - Factor by which 5GHz RSSI is boosted.
 * @Min: 0
 * @Max: 2
 * @Default: 1
 * This ini is used to set the 5Ghz boost factor.
 *
 * Related: 5g_rssi_boost_threshold, 5g_max_rssi_boost
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_5G_RSSI_BOOST_FACTOR_NAME            "5g_rssi_boost_factor"
#define CFG_5G_RSSI_BOOST_FACTOR_MIN             (0)
#define CFG_5G_RSSI_BOOST_FACTOR_MAX             (2)
#define CFG_5G_RSSI_BOOST_FACTOR_DEFAULT         (1)

/*
 * <ini>
 * 5g_max_rssi_boost - Maximum boost that can be applied to 5GHz RSSI.
 * @Min: 0
 * @Max: 20
 * @Default: 10
 * This ini is used to set maximum boost which can be given to a 5Ghz network.
 *
 * Related: 5g_rssi_boost_threshold, 5g_rssi_boost_factor
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_5G_MAX_RSSI_BOOST_NAME               "5g_max_rssi_boost"
#define CFG_5G_MAX_RSSI_BOOST_MIN                (0)
#define CFG_5G_MAX_RSSI_BOOST_MAX                (20)
#define CFG_5G_MAX_RSSI_BOOST_DEFAULT            (10)

/*
 * <ini>
 * 5g_rssi_penalize_threshold - A_band_penalize_threshold above which
 * 5 GHz is not favored.
 * @Min: -80
 * @Max: -65
 * @Default: -70
 * This ini is used to set threshold for 5GHz band preference.
 *
 * Related: 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 * 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_5G_RSSI_PENALIZE_THRESHOLD_NAME      "5g_rssi_penalize_threshold"
#define CFG_5G_RSSI_PENALIZE_THRESHOLD_MIN       (-80)
#define CFG_5G_RSSI_PENALIZE_THRESHOLD_MAX       (-65)
#define CFG_5G_RSSI_PENALIZE_THRESHOLD_DEFAULT   (-70)

/*
 * <ini>
 * 5g_rssi_penalize_factor - Factor by which 5GHz RSSI is penalizeed.
 * @Min: 0
 * @Max: 2
 * @Default: 1
 * This ini is used to set the 5Ghz penalize factor.
 *
 * Related: 5g_rssi_penalize_threshold, 5g_max_rssi_penalize
 * 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_5G_RSSI_PENALIZE_FACTOR_NAME         "5g_rssi_penalize_factor"
#define CFG_5G_RSSI_PENALIZE_FACTOR_MIN          (0)
#define CFG_5G_RSSI_PENALIZE_FACTOR_MAX          (2)
#define CFG_5G_RSSI_PENALIZE_FACTOR_DEFAULT      (1)

/*
 * <ini>
 * 5g_max_rssi_penalize - Maximum penalty that can be applied to 5GHz RSSI.
 * @Min: 0
 * @Max: 20
 * @Default: 10
 * This ini is used to set maximum penalty which can be given to a 5Ghz network.
 *
 * Related: 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor
 * 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_5G_MAX_RSSI_PENALIZE_NAME            "5g_max_rssi_penalize"
#define CFG_5G_MAX_RSSI_PENALIZE_MIN             (0)
#define CFG_5G_MAX_RSSI_PENALIZE_MAX             (20)
#define CFG_5G_MAX_RSSI_PENALIZE_DEFAULT         (10)

/*
 * <ini>
 * g_max_sched_scan_plan_int - pno sched max scan plan interval.
 * @Min: 1
 * @Max: 7200
 * @Default: 3600
 *
 * This ini is used to set max sched scan plan interval for pno scan
 * (value in seconds).
 *
 * Related: gPNOScanSupport
 *
 * Supported Feature: PNO scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_SCHED_SCAN_PLAN_INT_NAME       "g_max_sched_scan_plan_int"
#define CFG_MAX_SCHED_SCAN_PLAN_INT_MIN        (1)
#define CFG_MAX_SCHED_SCAN_PLAN_INT_MAX        (7200)
#define CFG_MAX_SCHED_SCAN_PLAN_INT_DEFAULT    (3600)

/*
 * <ini>
 * g_max_sched_scan_plan_iterations - pno sched max scan plan iterations.
 * @Min: 1
 * @Max: 100
 * @Default: 10
 *
 * This ini is used to set max sched scan plan iterations for pno scan
 * (value in seconds).
 *
 * Related: gPNOScanSupport
 *
 * Supported Feature: PNO scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_SCHED_SCAN_PLAN_ITRNS_NAME   "g_max_sched_scan_plan_iterations"
#define CFG_MAX_SCHED_SCAN_PLAN_ITRNS_MIN     (1)
#define CFG_MAX_SCHED_SCAN_PLAN_ITRNS_MAX     (100)
#define CFG_MAX_SCHED_SCAN_PLAN_ITRNS_DEFAULT (10)

/**
 * enum hdd_wext_control - knob for wireless extensions
 * @hdd_wext_disabled - interface is completely disabled. An access
 *      control error log will be generated for each attempted use.
 * @hdd_wext_deprecated - interface is available but should not be
 *      used. An access control warning log will be generated for each
 *      use.
 * @hdd_wext_enabled - interface is available without restriction. No
 *      access control logs will be generated.
 *
 * enum hdd_wext_control is used to enable coarse grained control on
 * wireless extensions ioctls. This control is used by configuration
 * item private_wext_control.
 *
 */
enum hdd_wext_control {
	hdd_wext_disabled = 0,
	hdd_wext_deprecated = 1,
	hdd_wext_enabled = 2,
};

/*
 * <ini>
 * private_wext_control - Private wireless extensions control
 * @Min: 0
 * @Max: 2
 * @Default: 1
 *
 * Values are per enum hdd_wext_control.
 *
 * This ini is used to control access to private wireless extensions
 * ioctls SIOCIWFIRSTPRIV (0x8BE0) thru SIOCIWLASTPRIV (0x8BFF). The
 * functionality provided by some of these ioctls has been superceeded
 * by cfg80211 (either standard commands or vendor commands), but many
 * of the private ioctls do not have a cfg80211-based equivalent, so
 * by default support for these ioctls is deprecated.
 *
 * Related: None
 *
 * Supported Feature: All
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRIVATE_WEXT_CONTROL_NAME           "private_wext_control"
#define CFG_PRIVATE_WEXT_CONTROL_MIN            (hdd_wext_disabled)
#define CFG_PRIVATE_WEXT_CONTROL_MAX            (hdd_wext_enabled)
#define CFG_PRIVATE_WEXT_CONTROL_DEFAULT        (hdd_wext_deprecated)

/*
 * <ini>
 * gper_roam_mon_time - Minimum time required in seconds to
 * be considered as valid scenario for PER based roam
 * @Min: 5
 * @Max: 25
 * @Default: 25
 *
 * This ini is used to define minimum time in seconds for which DUT has
 * collected the PER stats before it can consider the stats hysteresis to be
 * valid for PER based scan.
 * DUT collects following information during this period:
 *     1. % of packets below gper_roam_low_rate_th
 *     2. # packets above gper_roam_high_rate_th
 * if DUT gets (1) greater than gper_roam_th_percent and (2) is zero during
 * this period, it triggers PER based roam scan.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PER_ROAM_MONITOR_TIME                "gper_roam_mon_time"
#define CFG_PER_ROAM_MONITOR_TIME_MIN            (5)
#define CFG_PER_ROAM_MONITOR_TIME_MAX            (25)
#define CFG_PER_ROAM_MONTIOR_TIME_DEFAULT        (25)

/*
 * <ini>
 * gPowerUsage - Preferred Power Usage
 * @Min: Min
 * @Max: Max
 * @Default: Mod
 *
 * This ini is used to set the preferred power usage
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_POWER_USAGE_NAME                   "gPowerUsage"
#define CFG_POWER_USAGE_MIN                    "Min"
#define CFG_POWER_USAGE_MAX                    "Max"
#define CFG_POWER_USAGE_DEFAULT                "Mod"


/*
 * <ini>
 * gper_min_rssi_threshold_for_roam -  Minimum roamable AP RSSI for
 * candidate selection for PER based roam
 * @Min: 0
 * @Max: 96
 * @Default: 83
 *
 * Minimum roamable AP RSSI for candidate selection for PER based roam
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PER_ROAM_MIN_CANDIDATE_RSSI           "gper_min_rssi_threshold_for_roam"
#define CFG_PER_ROAM_MIN_CANDIDATE_RSSI_MIN       (0)
#define CFG_PER_ROAM_MIN_CANDIDATE_RSSI_MAX       (96)
#define CFG_PER_ROAM_MIN_CANDIDATE_RSSI_DEFAULT   (83)


/*
 * <ini>
 * gEnableImps - Enable/Disable IMPS
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/Disable IMPS(IdleModePowerSave) Mode
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_IMPS_NAME                   "gEnableImps"
#define CFG_ENABLE_IMPS_MIN                    (0)
#define CFG_ENABLE_IMPS_MAX                    (1)
#define CFG_ENABLE_IMPS_DEFAULT                (1)

/*
 * <ini>
 * gEnableBmps - Enable/Disable BMPS
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/Disable BMPS(BeaconModePowerSave) Mode
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_PS_NAME                     "gEnableBmps"
#define CFG_ENABLE_PS_MIN                      (0)
#define CFG_ENABLE_PS_MAX                      (1)
#define CFG_ENABLE_PS_DEFAULT                  (1)

/*
 * <ini>
 * gAutoBmpsTimerValue - Set Auto BMPS Timer value
 * @Min: 0
 * @Max: 1000
 * @Default: 600
 *
 * This ini is used to set Auto BMPS Timer value in seconds
 *
 * Related: gEnableBmps
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_AUTO_PS_ENABLE_TIMER_NAME          "gAutoBmpsTimerValue"
#define CFG_AUTO_PS_ENABLE_TIMER_MIN           (0)
#define CFG_AUTO_PS_ENABLE_TIMER_MAX           (1000)
#define CFG_AUTO_PS_ENABLE_TIMER_DEFAULT       (600)

#ifdef WLAN_ICMP_DISABLE_PS
/*
 * <ini>
 * gIcmpDisablePsValue - Set ICMP packet disable power save value
 * @Min:     0
 * @Max:     10000
 * @Default: 5000
 *
 * This ini is used to set ICMP packet disable power save value in
 * millisecond.
 *
 * Related: gEnableBmps
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ICMP_DISABLE_PS_NAME               "gIcmpDisablePsValue"
#define CFG_ICMP_DISABLE_PS_MIN                (0)
#define CFG_ICMP_DISABLE_PS_MAX                (10000)
#define CFG_ICMP_DISABLE_PS_DEFAULT            (5000)
#endif

/*
 * <ini>
 * gBmpsMinListenInterval - Set BMPS Minimum Listen Interval
 * @Min: 1
 * @Max: 65535
 * @Default: 1
 *
 * This ini is used to set BMPS Minimum Listen Interval. If gPowerUsage
 * is set "Min", this INI need to be set.
 *
 * Related: gEnableBmps, gPowerUsage
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BMPS_MINIMUM_LI_NAME               "gBmpsMinListenInterval"
#define CFG_BMPS_MINIMUM_LI_MIN                (1)
#define CFG_BMPS_MINIMUM_LI_MAX                (65535)
#define CFG_BMPS_MINIMUM_LI_DEFAULT            (1)

/*
 * <ini>
 * gBmpsMaxListenInterval - Set BMPS Maximum Listen Interval
 * @Min: 1
 * @Max: 65535
 * @Default: 1
 *
 * This ini is used to set BMPS Maximum Listen Interval. If gPowerUsage
 * is set "Max", this INI need to be set.
 *
 * Related: gEnableBmps, gPowerUsage
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BMPS_MAXIMUM_LI_NAME               "gBmpsMaxListenInterval"
#define CFG_BMPS_MAXIMUM_LI_MIN                (1)
#define CFG_BMPS_MAXIMUM_LI_MAX                (65535)
#define CFG_BMPS_MAXIMUM_LI_DEFAULT            (1)

#ifdef FEATURE_RUNTIME_PM
/*
 * <ini>
 * gRuntimePM - enable runtime suspend
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable runtime_suspend
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_RUNTIME_PM                  "gRuntimePM"
#define CFG_ENABLE_RUNTIME_PM_MIN              (0)
#define CFG_ENABLE_RUNTIME_PM_MAX              (1)
#define CFG_ENABLE_RUNTIME_PM_DEFAULT          (0)

/*
 * <ini>
 * gRuntimePMDelay - Set runtime pm's inactivity timer
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set runtime pm's inactivity timer value.
 * the wlan driver will wait for this number of milliseconds of
 * inactivity before performing a runtime suspend.
 *
 * Related: gRuntimePM
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RUNTIME_PM_DELAY_NAME               "gRuntimePMDelay"
#define CFG_RUNTIME_PM_DELAY_MIN                (100)
#define CFG_RUNTIME_PM_DELAY_MAX                (10000)
#define CFG_RUNTIME_PM_DELAY_DEFAULT            (500)
#endif

/*
 * <ini>
 * gEnablePowerSaveOffload - Enable Power Save Offload
 * @Min: 0
 * @Max: 5
 * @Default: 0
 *
 * This ini is used to set Power Save Offload configuration:
 * Current values of gEnablePowerSaveOffload:
 * 0 -> Power save offload is disabled
 * 1 -> Legacy Power save enabled + Deep sleep Disabled
 * 2 -> QPower enabled + Deep sleep Disabled
 * 3 -> Legacy Power save enabled + Deep sleep Enabled
 * 4 -> QPower enabled + Deep sleep Enabled
 * 5 -> Duty cycling QPower enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_POWERSAVE_OFFLOAD_NAME                "gEnablePowerSaveOffload"
#define CFG_POWERSAVE_OFFLOAD_MIN                 (0)
#define CFG_POWERSAVE_OFFLOAD_MAX                 (PS_DUTY_CYCLING_QPOWER)
#define CFG_POWERSAVE_OFFLOAD_DEFAULT             (CFG_POWERSAVE_OFFLOAD_MIN)

/*
 * <ini>
 * gEnableWoW - Enable/Disable WoW
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to enable/disable WoW. Configurations are as follows:
 * 0 - Disable both magic pattern match and pattern byte match.
 * 1 - Enable magic pattern match on all interfaces.
 * 2 - Enable pattern byte match on all interfaces.
 * 3 - Enable both magic patter and pattern byte match on all interfaces.
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_WOW_STATUS_NAME                    "gEnableWoW"
#define CFG_WOW_ENABLE_MIN                     (0)
#define CFG_WOW_ENABLE_MAX                     (3)
#define CFG_WOW_STATUS_DEFAULT                 (3)

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
/*
 * <ini>
 * gExtWoWgotoSuspend - Enable/Disable Extended WoW
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable Extended WoW.
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_GO_TO_SUSPEND               "gExtWoWgotoSuspend"
#define CFG_EXTWOW_GO_TO_SUSPEND_MIN           (0)
#define CFG_EXTWOW_GO_TO_SUSPEND_MAX           (1)
#define CFG_EXTWOW_GO_TO_SUSPEND_DEFAULT       (1)

/*
 * <ini>
 * gExtWowApp1WakeupPinNumber - Set wakeup1 PIN number
 * @Min: 0
 * @Max: 255
 * @Default: 12
 *
 * This ini is used to set EXT WOW APP1 wakeup PIN number
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_APP1_WAKE_PIN_NUMBER            "gExtWowApp1WakeupPinNumber"
#define CFG_EXTWOW_APP1_WAKE_PIN_NUMBER_MIN        (0)
#define CFG_EXTWOW_APP1_WAKE_PIN_NUMBER_MAX        (255)
#define CFG_EXTWOW_APP1_WAKE_PIN_NUMBER_DEFAULT    (12)

/*
 * <ini>
 * gExtWowApp2WakeupPinNumber - Set wakeup2 PIN number
 * @Min: 0
 * @Max: 255
 * @Default: 16
 *
 * This ini is used to set EXT WOW APP2 wakeup PIN number
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_APP2_WAKE_PIN_NUMBER            "gExtWowApp2WakeupPinNumber"
#define CFG_EXTWOW_APP2_WAKE_PIN_NUMBER_MIN        (0)
#define CFG_EXTWOW_APP2_WAKE_PIN_NUMBER_MAX        (255)
#define CFG_EXTWOW_APP2_WAKE_PIN_NUMBER_DEFAULT    (16)

/*
 * <ini>
 * gExtWoWApp2KAInitPingInterval - Set Keep Alive Init Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 240
 *
 * This ini is used to set Keep Alive Init Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_INIT_PING_INTERVAL           "gExtWoWApp2KAInitPingInterval"
#define CFG_EXTWOW_KA_INIT_PING_INTERVAL_MIN       (0)
#define CFG_EXTWOW_KA_INIT_PING_INTERVAL_MAX       (0xffffffff)
#define CFG_EXTWOW_KA_INIT_PING_INTERVAL_DEFAULT   (240)

/*
 * <ini>
 * gExtWoWApp2KAMinPingInterval - Set Keep Alive Minimum Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 240
 *
 * This ini is used to set Keep Alive Minimum Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_MIN_PING_INTERVAL            "gExtWoWApp2KAMinPingInterval"
#define CFG_EXTWOW_KA_MIN_PING_INTERVAL_MIN        (0)
#define CFG_EXTWOW_KA_MIN_PING_INTERVAL_MAX        (0xffffffff)
#define CFG_EXTWOW_KA_MIN_PING_INTERVAL_DEFAULT    (240)

/*
 * <ini>
 * gExtWoWApp2KAMaxPingInterval - Set Keep Alive Maximum Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 1280
 *
 * This ini is used to set Keep Alive Maximum Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_MAX_PING_INTERVAL            "gExtWoWApp2KAMaxPingInterval"
#define CFG_EXTWOW_KA_MAX_PING_INTERVAL_MIN        (0)
#define CFG_EXTWOW_KA_MAX_PING_INTERVAL_MAX        (0xffffffff)
#define CFG_EXTWOW_KA_MAX_PING_INTERVAL_DEFAULT    (1280)

/*
 * <ini>
 * gExtWoWApp2KAIncPingInterval - Set Keep Alive increment of Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 4
 *
 * This ini is used to set Keep Alive increment of Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_INC_PING_INTERVAL            "gExtWoWApp2KAIncPingInterval"
#define CFG_EXTWOW_KA_INC_PING_INTERVAL_MIN        (0)
#define CFG_EXTWOW_KA_INC_PING_INTERVAL_MAX        (0xffffffff)
#define CFG_EXTWOW_KA_INC_PING_INTERVAL_DEFAULT    (4)

/*
 * <ini>
 * gExtWoWApp2KAIncPingInterval - Set TCP source port
 * @Min: 0
 * @Max: 65535
 * @Default: 5000
 *
 * This ini is used to set TCP source port when EXT WOW is enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_SRC_PORT                    "gExtWoWApp2TcpSrcPort"
#define CFG_EXTWOW_TCP_SRC_PORT_MIN                (0)
#define CFG_EXTWOW_TCP_SRC_PORT_MAX                (65535)
#define CFG_EXTWOW_TCP_SRC_PORT_DEFAULT            (5000)

/*
 * <ini>
 * gExtWoWApp2TcpDstPort - Set TCP Destination port
 * @Min: 0
 * @Max: 65535
 * @Default: 5001
 *
 * This ini is used to set TCP Destination port when EXT WOW is enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_DST_PORT                    "gExtWoWApp2TcpDstPort"
#define CFG_EXTWOW_TCP_DST_PORT_MIN                (0)
#define CFG_EXTWOW_TCP_DST_PORT_MAX                (65535)
#define CFG_EXTWOW_TCP_DST_PORT_DEFAULT            (5001)

/*
 * <ini>
 * gExtWoWApp2TcpTxTimeout - Set TCP tx timeout
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 200
 *
 * This ini is used to set TCP Tx timeout when EXT WOW is enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_TX_TIMEOUT                  "gExtWoWApp2TcpTxTimeout"
#define CFG_EXTWOW_TCP_TX_TIMEOUT_MIN              (0)
#define CFG_EXTWOW_TCP_TX_TIMEOUT_MAX              (0xffffffff)
#define CFG_EXTWOW_TCP_TX_TIMEOUT_DEFAULT          (200)

/*
 * <ini>
 * gExtWoWApp2TcpRxTimeout - Set TCP rx timeout
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 200
 *
 * This ini is used to set TCP Rx timeout when EXT WOW is enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_RX_TIMEOUT                  "gExtWoWApp2TcpRxTimeout"
#define CFG_EXTWOW_TCP_RX_TIMEOUT_MIN              (0)
#define CFG_EXTWOW_TCP_RX_TIMEOUT_MAX              (0xffffffff)
#define CFG_EXTWOW_TCP_RX_TIMEOUT_DEFAULT          (200)
#endif

/*
 * <ini>
 * gper_roam_enabled - To enabled/disable PER based roaming in FW
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini is used to enable/disable Packet error based roaming, enabling this
 * will cause DUT to monitor Tx and Rx traffic and roam to a better candidate
 * if current is not good enough.
 *
 * Values supported:
 * 0: disabled
 * 1: enabled for Rx traffic
 * 2: enabled for Tx traffic
 * 3: enabled for Tx and Rx traffic
 *
 * Related: gper_roam_high_rate_th, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PER_ROAM_ENABLE_NAME           "gper_roam_enabled"
#define CFG_PER_ROAM_ENABLE_MIN            (0)
#define CFG_PER_ROAM_ENABLE_MAX            (3)
#define CFG_PER_ROAM_ENABLE_DEFAULT        (3)

/*
 * <ini>
 * gper_roam_high_rate_th - Rate at which PER based roam will stop
 * @Min: 1 Mbps
 * @Max: 0xffffffff
 * @Default: 40 Mbps
 *
 * This ini is used to define the data rate in mbps*10 at which FW will stop
 * monitoring the traffic for PER based roam.
 *
 * Related: gper_roam_enabled, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PER_ROAM_CONFIG_HIGH_RATE_TH_NAME    "gper_roam_high_rate_th"
#define CFG_PER_ROAM_CONFIG_HIGH_RATE_TH_MIN     (10)
#define CFG_PER_ROAM_CONFIG_HIGH_RATE_TH_MAX     (0xffffffff)
#define CFG_PER_ROAM_CONFIG_HIGH_RATE_TH_DEFAULT (400)

/*
 * <ini>
 * gper_roam_low_rate_th - Rate at which FW starts considering traffic for PER
 * based roam.
 *
 * @Min: 1 Mbps
 * @Max: 0xffffffff
 * @Default: 20 Mbps
 *
 * This ini is used to define the rate in mbps*10 at which FW starts considering
 * traffic for PER based roam, if gper_roam_th_percent of data is below this
 * rate, FW will issue a roam scan.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PER_ROAM_CONFIG_LOW_RATE_TH_NAME    "gper_roam_low_rate_th"
#define CFG_PER_ROAM_CONFIG_LOW_RATE_TH_MIN     (10)
#define CFG_PER_ROAM_CONFIG_LOW_RATE_TH_MAX     (0xffffffff)
#define CFG_PER_ROAM_CONFIG_LOW_RATE_TH_DEFAULT (200)

/*
 * <ini>
 * gper_roam_th_percent - Percentage at which FW will issue a roam scan if
 * traffic is below gper_roam_low_rate_th rate.
 *
 * @Min: 10%
 * @Max: 100%
 * @Default: 60%
 *
 * This ini is used to define the percentage at which FW will issue a roam scan
 * if traffic is below gper_roam_low_rate_th rate.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th,
 *          gper_roam_high_rate_th, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PER_ROAM_CONFIG_RATE_TH_PERCENT_NAME      "gper_roam_th_percent"
#define CFG_PER_ROAM_CONFIG_RATE_TH_PERCENT_MIN       (10)
#define CFG_PER_ROAM_CONFIG_RATE_TH_PERCENT_MAX       (100)
#define CFG_PER_ROAM_CONFIG_RATE_TH_PERCENT_DEFAULT   (60)

/*
 * <ini>
 * gper_roam_rest_time - Time for which FW will wait once it issues a
 * roam scan.
 *
 * @Min: 10 seconds
 * @Max: 3600 seconds
 * @Default: 300 seconds
 *
 * This ini is used to define the time for which FW will wait once it issues a
 * PER based roam scan.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th,
 *          gper_roam_high_rate_th, gper_roam_th_percent
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PER_ROAM_REST_TIME_NAME     "gper_roam_rest_time"
#define CFG_PER_ROAM_REST_TIME_MIN      (10)
#define CFG_PER_ROAM_REST_TIME_MAX      (3600)
#define CFG_PER_ROAM_REST_TIME_DEFAULT  (300)

/*
 * <ini>
 * g_is_fils_enabled - Enable/Disable FILS support in driver
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable FILS support in driver
 * Driver will update config to supplicant based on this config.
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IS_FILS_ENABLED_NAME    "g_is_fils_enabled"
#define CFG_IS_FILS_ENABLED_DEFAULT (1)
#define CFG_IS_FILS_ENABLED_MIN     (0)
#define CFG_IS_FILS_ENABLED_MAX     (1)

/*
 * <ini>
 * g_enable_bcast_probe_rsp - Enable Broadcast probe response.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable broadcast probe response.
 * If this is disabled then OCE ini oce_sta_enable will also be
 * disabled and OCE IE will not be sent in frames.
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_BCAST_PROBE_RESP_NAME    "g_enable_bcast_probe_rsp"
#define CFG_ENABLE_BCAST_PROBE_RESP_MIN     (0)
#define CFG_ENABLE_BCAST_PROBE_RESP_MAX     (1)
#define CFG_ENABLE_BCAST_PROBE_RESP_DEFAULT (1)

/**
 * arp_ac_category - ARP access category
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * Firmware by default categorizes ARP packets with VOICE TID.
 * This ini shall be used to override the default configuration.
 * Access category enums are referenced in ieee80211_common.h
 * WME_AC_BE = 0 (Best effort)
 * WME_AC_BK = 1 (Background)
 * WME_AC_VI = 2 (Video)
 * WME_AC_VO = 3 (Voice)
 *
 * Related: none
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ARP_AC_CATEGORY                "arp_ac_category"
#define CFG_ARP_AC_CATEGORY_MIN            (0)
#define CFG_ARP_AC_CATEGORY_MAX            (3)
#define CFG_ARP_AC_CATEGORY_DEFAULT        (3)

/*
 * <ini>
 * gActiveUcBpfMode - Control UC active APF mode
 * @Min: 0 (disabled)
 * @Max: 2 (adaptive)
 * @Default: 0 (disabled)
 *
 * This config item controls UC APF in active mode. There are 3 modes:
 *	0) disabled - APF is disabled in active mode
 *	1) enabled - APF is enabled for all packets in active mode
 *	2) adaptive - APF is enabled for packets up to some throughput threshold
 *
 * Related: gActiveMcBcBpfMode
 *
 * Supported Feature: Active Mode APF
 *
 * Usage: Internal/External
 * </ini>
 */
#define CFG_ACTIVE_UC_APF_MODE_NAME    "gActiveUcBpfMode"
#define CFG_ACTIVE_UC_APF_MODE_MIN     (ACTIVE_APF_DISABLED)
#define CFG_ACTIVE_UC_APF_MODE_MAX     (ACTIVE_APF_MODE_COUNT - 1)
#define CFG_ACTIVE_UC_APF_MODE_DEFAULT (ACTIVE_APF_DISABLED)

/*
 * <ini>
 * gActiveMcBcBpfMode - Control MC/BC active APF mode
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 0 (disabled)
 *
 * This config item controls MC/BC APF in active mode. There are 3 modes:
 *	0) disabled - APF is disabled in active mode
 *	1) enabled - APF is enabled for all packets in active mode
 *	2) adaptive - APF is enabled for packets up to some throughput threshold
 *
 * Related: gActiveUcBpfMode
 *
 * Supported Feature: Active Mode APF
 *
 * Usage: Internal/External
 * </ini>
 */
#define CFG_ACTIVE_MC_BC_APF_MODE_NAME    "gActiveMcBcBpfMode"
#define CFG_ACTIVE_MC_BC_APF_MODE_MIN     (ACTIVE_APF_DISABLED)
#define CFG_ACTIVE_MC_BC_APF_MODE_MAX     (ACTIVE_APF_ENABLED)
#define CFG_ACTIVE_MC_BC_APF_MODE_DEFAULT (ACTIVE_APF_DISABLED)

/*
 * <ini>
 * acs_with_more_param- Enable acs calculation with more param.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable acs calculation with more param.
 *
 * Related: NA
 *
 * Supported Feature: ACS
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_ACS_WITH_MORE_PARAM_NAME    "acs_with_more_param"
#define CFG_ACS_WITH_MORE_PARAM_MIN     (0)
#define CFG_ACS_WITH_MORE_PARAM_MAX     (1)
#define CFG_ACS_WITH_MORE_PARAM_DEFAULT (0)

/*
 * <ini>
 * AutoChannelSelectWeight - ACS channel weight
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0x000000FF
 *
 * This ini is used to adjust weight of factors in
 * acs algorithm.
 *
 * Supported Feature: ACS
 *
 * Usage: Internal/External
 *
 * bits 0-3:   rssi weight
 * bits 4-7:   bss count weight
 * bits 8-11:  noise floor weight
 * bits 12-15: channel free weight
 * bits 16-19: tx power range weight
 * bits 20-23: tx power throughput weight
 * bits 24-31: reserved
 *
 * </ini>
 */
#define CFG_AUTO_CHANNEL_SELECT_WEIGHT            "AutoChannelSelectWeight"
#define CFG_AUTO_CHANNEL_SELECT_WEIGHT_MIN        (0)
#define CFG_AUTO_CHANNEL_SELECT_WEIGHT_MAX        (0xFFFFFFFF)
#define CFG_AUTO_CHANNEL_SELECT_WEIGHT_DEFAULT    (0x000000FF)

#ifdef WLAN_FEATURE_11AX
/* 11AX related INI configuration */
/*
 * <ini>
 * he_dynamic_frag_support - configure dynamic fragmentation
 * @Min: 0
 * @Max: 3
 * @Default: 1
 *
 * This ini is used to configure dynamic fragmentation.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_HE_DYNAMIC_FRAGMENTATION_NAME    "he_dynamic_frag_support"
#define CFG_HE_DYNAMIC_FRAGMENTATION_MIN     (0)
#define CFG_HE_DYNAMIC_FRAGMENTATION_MAX     (3)
#define CFG_HE_DYNAMIC_FRAGMENTATION_DEFAULT (0)

/*
 * <ini>
 * enable_ul_mimo- Enable UL MIMO.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable or disable UL MIMO.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_UL_MIMO_NAME    "enable_ul_mimo"
#define CFG_ENABLE_UL_MIMO_MIN     (0)
#define CFG_ENABLE_UL_MIMO_MAX     (1)
#define CFG_ENABLE_UL_MIMO_DEFAULT (0)

/*
 * <ini>
 * enable_ul_ofdma- Enable UL OFDMA.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable or disable UL OFDMA.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_UL_OFDMA_NAME    "enable_ul_ofdma"
#define CFG_ENABLE_UL_OFDMA_MIN     (0)
#define CFG_ENABLE_UL_OFDMA_MAX     (1)
#define CFG_ENABLE_UL_OFDMA_DEFAULT (0)

/*
 * <ini>
 * he_sta_obsspd- 11AX HE OBSS PD bit field
 * @Min: 0
 * @Max: uin32_t max
 * @Default: 0x15b8c2ae
 *
 * 4 Byte value with each byte representing a signed value for following params:
 * Param                   Bit position    Default
 * OBSS_PD min (primary)   7:0             -82 (0xae)
 * OBSS_PD max (primary)   15:8            -62 (0xc2)
 * Secondary channel Ed    23:16           -72 (0xb8)
 * TX_PWR(ref)             31:24           21  (0x15)
 * This bit field value is directly applied to FW
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HE_STA_OBSSPD_NAME    "he_sta_obsspd"
#define CFG_HE_STA_OBSSPD_MIN     (0)
#define CFG_HE_STA_OBSSPD_MAX     (0xffffffff)
#define CFG_HE_STA_OBSSPD_DEFAULT (0x15b8c2ae)

#endif /* WLAN_FEATURE_11AX */
#ifdef WLAN_SUPPORT_TWT
/*
 * <ini>
 * enable_twt - Enable Target Wake Time support.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable TWT support.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_TWT_NAME    "enable_twt"
#define CFG_ENABLE_TWT_MIN     (0)
#define CFG_ENABLE_TWT_MAX     (1)
#define CFG_ENABLE_TWT_DEFAULT (1)

/*
 * <ini>
 * twt_congestion_timeout - Target wake time congestion timeout.
 * @Min: 0
 * @Max: 10000
 * @Default: 100
 *
 * STA uses this timer to continuously monitor channel congestion levels to
 * decide whether to start or stop TWT. This ini is used to configure the
 * target wake time congestion timeout value in the units of milliseconds.
 * A value of Zero indicates that this is a host triggered TWT and all the
 * necessary configuration for TWT will be directed from the host.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TWT_CONGESTION_TIMEOUT_NAME    "twt_congestion_timeout"
#define CFG_TWT_CONGESTION_TIMEOUT_MIN     (0)
#define CFG_TWT_CONGESTION_TIMEOUT_MAX     (10000)
#define CFG_TWT_CONGESTION_TIMEOUT_DEFAULT (100)

#endif /* WLAN_SUPPORT_TWT */

/*
 * <ini>
 * gEnableANI - Enable Adaptive Noise Immunity
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable Adaptive Noise Immunity.
 *
 * Related: None
 *
 * Supported Feature: ANI
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_ANI_NAME              "gEnableANI"
#define CFG_ENABLE_ANI_MIN               (0)
#define CFG_ENABLE_ANI_MAX               (1)
#define CFG_ENABLE_ANI_DEFAULT           (1)

/*
 * <ini>
 * g_qcn_ie_support - QCN IE Support
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 1 (enabled)
 *
 * This config item is used to support QCN IE in probe/assoc/reassoc request
 * for STA mode. QCN IE support is not added for SAP mode.
 *
 * Related: N/A
 *
 * Supported Feature: N/A
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QCN_IE_SUPPORT_NAME    "g_qcn_ie_support"
#define CFG_QCN_IE_SUPPORT_MIN      0
#define CFG_QCN_IE_SUPPORT_MAX      1
#define CFG_QCN_IE_SUPPORT_DEFAULT  1

/*
 * <ini>
 * gTimerMultiplier - Scale QDF timers by this value
 * @Min: 1
 * @Max: 0xFFFFFFFF
 * @Default: 1 (100 for emulation)
 *
 * To assist in debugging emulation setups, scale QDF timers by this factor.
 *
 * @E.g.
 *	# QDF timers expire in real time
 *	gTimerMultiplier=1
 *	# QDF timers expire after 100 times real time
 *	gTimerMultiplier=100
 *
 * Related: N/A
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TIMER_MULTIPLIER_NAME	"gTimerMultiplier"
#define CFG_TIMER_MULTIPLIER_MIN	(1)
#define CFG_TIMER_MULTIPLIER_MAX	(0xFFFFFFFF)
#ifdef QCA_WIFI_NAPIER_EMULATION
#define CFG_TIMER_MULTIPLIER_DEFAULT	(100)
#else
#define CFG_TIMER_MULTIPLIER_DEFAULT	(1)
#endif

/* Begin of probe request IE whitelisting feature ini params */
/*
 * <ini>
 * g_enable_probereq_whitelist_ies - Enable IE white listing
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable probe request IE white listing feature.
 * Values 0 and 1 are used to disable and enable respectively, by default this
 * feature is disabled.
 *
 * Related: None
 *
 * Supported Feature: Probe request IE whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_WHITELIST_NAME    "g_enable_probereq_whitelist_ies"
#define CFG_PRB_REQ_IE_WHITELIST_MIN     (0)
#define CFG_PRB_REQ_IE_WHITELIST_MAX     (1)
#define CFG_PRB_REQ_IE_WHITELIST_DEFAULT (0)

/*
 * For IE white listing in Probe Req, following ini parameters from
 * g_probe_req_ie_bitmap_0 to g_probe_req_ie_bitmap_7 are used. User needs to
 * input this values in hexa decimal format, when bit is set in bitmap,
 * corresponding IE needs to be included in probe request.
 *
 * Example:
 * ========
 * If IE 221 needs to be in the probe request, set the corresponding bit
 * as follows:
 * a= IE/32 = 221/32 = 6 = g_probe_req_ie_bitmap_6
 * b = IE modulo 32 = 29,
 * means set the bth bit in g_probe_req_ie_bitmap_a,
 * therefore set 29th bit in g_probe_req_ie_bitmap_6,
 * as a result, g_probe_req_ie_bitmap_6=20000000
 *
 * Note: For IE 221, its mandatory to set the gProbeReqOUIs.
 */

/*
 * <ini>
 * g_probe_req_ie_bitmap_0 - Used to set the bitmap of IEs from 0 to 31
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 0 to 31 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP0_NAME    "g_probe_req_ie_bitmap_0"
#define CFG_PRB_REQ_IE_BIT_MAP0_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP0_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP0_DEFAULT (0x00000000)

/*
 * <ini>
 * g_probe_req_ie_bitmap_1 - Used to set the bitmap of IEs from 32 to 63
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 32 to 63 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP1_NAME    "g_probe_req_ie_bitmap_1"
#define CFG_PRB_REQ_IE_BIT_MAP1_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP1_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP1_DEFAULT (0x00000000)

/*
 * <ini>
 * g_probe_req_ie_bitmap_2 - Used to set the bitmap of IEs from 64 to 95
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 64 to 95 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP2_NAME    "g_probe_req_ie_bitmap_2"
#define CFG_PRB_REQ_IE_BIT_MAP2_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP2_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP2_DEFAULT (0x00000000)

/*
 * <ini>
 * g_probe_req_ie_bitmap_3 - Used to set the bitmap of IEs from 96 to 127
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 96 to 127 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP3_NAME    "g_probe_req_ie_bitmap_3"
#define CFG_PRB_REQ_IE_BIT_MAP3_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP3_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP3_DEFAULT (0x00000000)

/*
 * <ini>
 * g_probe_req_ie_bitmap_4 - Used to set the bitmap of IEs from 128 to 159
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 128 to 159 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP4_NAME    "g_probe_req_ie_bitmap_4"
#define CFG_PRB_REQ_IE_BIT_MAP4_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP4_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP4_DEFAULT (0x00000000)

/*
 * <ini>
 * g_probe_req_ie_bitmap_5 - Used to set the bitmap of IEs from 160 to 191
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 160 to 191 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP5_NAME    "g_probe_req_ie_bitmap_5"
#define CFG_PRB_REQ_IE_BIT_MAP5_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP5_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP5_DEFAULT (0x00000000)

/*
 * <ini>
 * g_probe_req_ie_bitmap_6 - Used to set the bitmap of IEs from 192 to 223
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 192 to 223 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP6_NAME    "g_probe_req_ie_bitmap_6"
#define CFG_PRB_REQ_IE_BIT_MAP6_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP6_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP6_DEFAULT (0x00000000)

/*
 * <ini>
 * g_probe_req_ie_bitmap_7 - Used to set the bitmap of IEs from 224 to 255
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 224 to 255 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PRB_REQ_IE_BIT_MAP7_NAME    "g_probe_req_ie_bitmap_7"
#define CFG_PRB_REQ_IE_BIT_MAP7_MIN     (0x00000000)
#define CFG_PRB_REQ_IE_BIT_MAP7_MAX     (0xFFFFFFFF)
#define CFG_PRB_REQ_IE_BIT_MAP7_DEFAULT (0x00000000)

/*
 * For vendor specific IE, Probe Req OUI types and sub types which are
 * to be white listed are specified in gProbeReqOUIs in the following
 * example format - gProbeReqOUIs=AABBCCDD EEFF1122
 */

/*
 * <ini>
 * gProbeReqOUIs - Used to specify vendor specific OUIs
 * @Default: Empty string
 *
 * This ini is used to include the specified OUIs in vendor specific IE
 * of probe request.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies and
 * vendor specific IE should be set in g_probe_req_ie_bitmap_6.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_OUI_NAME    "gProbeReqOUIs"
#define CFG_PROBE_REQ_OUI_DEFAULT ""
/* End of probe request IE whitelisting feature ini params */

/*
 * <ini>
 * g_sap_chanswitch_beacon_cnt - channel switch beacon count
 * @Min: 1
 * @Max: 10
 * @Default: 10
 *
 * This ini is used to configure channel switch beacon count
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_CH_SWITCH_BEACON_CNT         "g_sap_chanswitch_beacon_cnt"
#define CFG_SAP_CH_SWITCH_BEACON_CNT_MIN     (1)
#define CFG_SAP_CH_SWITCH_BEACON_CNT_MAX     (10)
#define CFG_SAP_CH_SWITCH_BEACON_CNT_DEFAULT (10)

/*
 * <ini>
 * g_sap_chanswitch_mode - channel switch mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to configure channel switch mode
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_CH_SWITCH_MODE         "g_sap_chanswitch_mode"
#define CFG_SAP_CH_SWITCH_MODE_MIN     (0)
#define CFG_SAP_CH_SWITCH_MODE_MAX     (1)
#define CFG_SAP_CH_SWITCH_MODE_DEFAULT (1)

/*
 * <ini>
 * g_fils_max_chan_guard_time - Set maximum channel guard time(ms)
 * @Min: 0
 * @Max: 10
 * @Default: 0
 *
 * This ini is used to set maximum channel guard time in milli seconds
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_FILS_MAX_CHAN_GUARD_TIME_NAME    "g_fils_max_chan_guard_time"
#define CFG_FILS_MAX_CHAN_GUARD_TIME_MIN     (0)
#define CFG_FILS_MAX_CHAN_GUARD_TIME_MAX     (10)
#define CFG_FILS_MAX_CHAN_GUARD_TIME_DEFAULT (0)

/**
 * gSetRTSForSIFSBursting - set rts for sifs bursting
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini set rts for sifs bursting
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_RTS_FOR_SIFS_BURSTING           "gSetRTSForSIFSBursting"
#define CFG_SET_RTS_FOR_SIFS_BURSTING_MIN       (0)
#define CFG_SET_RTS_FOR_SIFS_BURSTING_MAX       (1)
#define CFG_SET_RTS_FOR_SIFS_BURSTING_DEFAULT   (0)

/**
 * <ini>
 * gMaxMPDUsInAMPDU - max mpdus in ampdu
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * This ini configure max mpdus in ampdu
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_MPDUS_IN_AMPDU                  "gMaxMPDUsInAMPDU"
#define CFG_MAX_MPDUS_IN_AMPDU_MIN              (0)
#define CFG_MAX_MPDUS_IN_AMPDU_MAX              (64)
#define CFG_MAX_MPDUS_IN_AMPDU_DEFAULT          (0)

/*
 * <ini>
 * gScanBackoffMultiplier - For NLO/PNO, multiply fast scan period by this every
 *	max cycles
 * @Min: 0
 * @Max: 255
 * @Default: 0
 *
 * For Network Listen Offload and Perfered Network Offload, multiply the fast
 * scan period by this value after max cycles have occurred. Setting this to 0
 * disables the feature.
 *
 * @E.g.
 *	# Disable scan backoff multiplier
 *	gScanBackoffMultiplier=0
 *	# Effectively the same
 *	gScanBackoffMultiplier=1
 *	# Double the scan period after each max cycles have occurred
 *	gScanBackoffMultiplier=2
 *
 * Related: NLO, PNO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SCAN_BACKOFF_MULTIPLIER_NAME	"gScanBackoffMultiplier"
#define CFG_SCAN_BACKOFF_MULTIPLIER_MIN		(0)
#define CFG_SCAN_BACKOFF_MULTIPLIER_MAX		(255)
#define CFG_SCAN_BACKOFF_MULTIPLIER_DEFAULT	(0)

/*
 * <ini>
 * mawc_nlo_enabled - For NLO/PNO, enable MAWC based scan
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Enable/Disable the Motion Aided Wireless Connectivity
 * based NLO using this parameter
 *
 * Related: NLO, PNO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_NLO_ENABLED_NAME	"mawc_nlo_enabled"
#define CFG_MAWC_NLO_ENABLED_MIN	(0)
#define CFG_MAWC_NLO_ENABLED_MAX	(1)
#define CFG_MAWC_NLO_ENABLED_DEFAULT	(1)

/*
 * <ini>
 * mawc_nlo_exp_backoff_ratio - Exponential back off ratio
 * @Min: 0
 * @Max: 300
 * @Default: 3
 *
 * Configure the exponential back off ratio using this
 * parameter for MAWC based NLO
 * ratio of exponential backoff, next = current + current*ratio/100
 *
 * Related: NLO, PNO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_NLO_EXP_BACKOFF_RATIO_NAME     "mawc_nlo_exp_backoff_ratio"
#define CFG_MAWC_NLO_EXP_BACKOFF_RATIO_MIN      (0)
#define CFG_MAWC_NLO_EXP_BACKOFF_RATIO_MAX      (300)
#define CFG_MAWC_NLO_EXP_BACKOFF_RATIO_DEFAULT  (3)

/*
 * <ini>
 * mawc_nlo_init_scan_interval - Initial Scan Interval
 * @Min: 1000
 * @Max: 0xFFFFFFFF
 * @Default: 10000
 *
 * Configure the initial scan interval  using this
 * parameter for MAWC based NLO (Units in Milliseconds)
 *
 * Related: NLO, PNO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_NLO_INIT_SCAN_INTERVAL_NAME     "mawc_nlo_init_scan_interval"
#define CFG_MAWC_NLO_INIT_SCAN_INTERVAL_MIN      (1000)
#define CFG_MAWC_NLO_INIT_SCAN_INTERVAL_MAX      (0xFFFFFFFF)
#define CFG_MAWC_NLO_INIT_SCAN_INTERVAL_DEFAULT  (10000)

/*
 * <ini>
 * mawc_nlo_max_scan_interval - Maximum Scan Interval
 * @Min: 1000
 * @Max: 0xFFFFFFFF
 * @Default: 60000
 *
 * Configure the maximum scan interval  using this
 * parameter for MAWC based NLO (Units in Milliseconds)
 *
 * Related: NLO, PNO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAWC_NLO_MAX_SCAN_INTERVAL_NAME     "mawc_nlo_max_scan_interval"
#define CFG_MAWC_NLO_MAX_SCAN_INTERVAL_MIN      (1000)
#define CFG_MAWC_NLO_MAX_SCAN_INTERVAL_MAX      (0xFFFFFFFF)
#define CFG_MAWC_NLO_MAX_SCAN_INTERVAL_DEFAULT  (60000)


/*
 * enum hdd_external_acs_policy - External ACS policy
 * @HDD_EXTERNAL_ACS_PCL_PREFERRED -Preferable for ACS to select a
 *	channel with non-zero pcl weight.
 * @HDD_EXTERNAL_ACS_PCL_MANDATORY -Mandatory for ACS to select a
 *	channel with non-zero pcl weight.
 *
 * enum hdd_external_acs_policy is used to select the ACS policy.
 *
 */
enum hdd_external_acs_policy {
	HDD_EXTERNAL_ACS_PCL_PREFERRED = 0,
	HDD_EXTERNAL_ACS_PCL_MANDATORY = 1,
};

/*
 * <ini>
 * external_acs_policy - External ACS policy control
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Values are per enum hdd_external_acs_policy.
 *
 * This ini is used to control the external ACS policy.
 *
 * Related: None
 *
 * Supported Feature: ACS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EXTERNAL_ACS_POLICY         "acs_policy"
#define CFG_EXTERNAL_ACS_POLICY_MIN     (HDD_EXTERNAL_ACS_PCL_PREFERRED)
#define CFG_EXTERNAL_ACS_POLICY_MAX     (HDD_EXTERNAL_ACS_PCL_MANDATORY)
#define CFG_EXTERNAL_ACS_POLICY_DEFAULT (HDD_EXTERNAL_ACS_PCL_PREFERRED)

/*
 * gSapMaxMCSForTxData - sap 11n max mcs
 * @Min: 0
 * @Max: 383
 * @Default: 0
 *
 * This ini configure SAP 11n max mcs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_MAX_MCS_FOR_TX_DATA         "gSapMaxMCSForTxData"
#define CFG_SAP_MAX_MCS_FOR_TX_DATA_MIN     (0)
#define CFG_SAP_MAX_MCS_FOR_TX_DATA_MAX     (383)
#define CFG_SAP_MAX_MCS_FOR_TX_DATA_DEFAULT (0)

/*
 * <ini>
 * gEnableTxOrphan- Enable/Disable orphaning of Tx packets
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable orphaning of Tx packets.
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_ORPHAN_ENABLE_NAME    "gEnableTxOrphan"
#define CFG_TX_ORPHAN_ENABLE_DEFAULT (0)
#define CFG_TX_ORPHAN_ENABLE_MIN     (0)
#define CFG_TX_ORPHAN_ENABLE_MAX     (1)

/*
 * <ini>
 * gItoRepeatCount - sets ito repeated count
 * @Min: 0
 * @Max: 5
 * @Default: 0
 *
 * This ini sets the ito count in FW
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ITO_REPEAT_COUNT_NAME "gItoRepeatCount"
#define CFG_ITO_REPEAT_COUNT_MIN        (0)
#define CFG_ITO_REPEAT_COUNT_MAX        (5)
#define CFG_ITO_REPEAT_COUNT_DEFAULT    (0)

/*
 * <ini>
 * oce_sta_enable - Enable/disable oce feature for STA
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable oce feature for STA
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_ENABLE_STA_NAME    "oce_sta_enable"
#define CFG_OCE_ENABLE_STA_MIN     (0)
#define CFG_OCE_ENABLE_STA_MAX     (1)
#define CFG_OCE_ENABLE_STA_DEFAULT (1)

/*
 * <ini>
 * oce_sap_enable - Enable/disable oce feature for SAP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable oce feature for SAP
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_ENABLE_SAP_NAME    "oce_sap_enable"
#define CFG_OCE_ENABLE_SAP_MIN     (0)
#define CFG_OCE_ENABLE_SAP_MAX     (1)
#define CFG_OCE_ENABLE_SAP_DEFAULT (1)

/*
 * <ini>
 * groam_disallow_duration -disallow duration before roaming
 * @Min: 0
 * @Max: 3600
 * @Default: 30
 *
 * This ini is used to configure how long LCA[Last Connected AP] AP will
 * be disallowed before it can be a roaming candidate again, in units of
 * seconds.
 *
 * Related: LFR
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ROAM_DISALLOW_DURATION_NAME    "groam_disallow_duration"
#define CFG_ROAM_DISALLOW_DURATION_MIN     (0)
#define CFG_ROAM_DISALLOW_DURATION_MAX     (3600)
#define CFG_ROAM_DISALLOW_DURATION_DEFAULT (30)

/*
 * <ini>
 * grssi_channel_penalization - RSSI penalization
 * @Min: 0
 * @Max: 15
 * @Default: 5
 *
 * This ini is used to configure RSSI that will be penalized if candidate(s)
 * are found to be in the same channel as disallowed AP's, in units of db.
 *
 * Related: LFR
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ROAM_RSSI_CHANNEL_PENALIZATION_NAME    "grssi_channel_penalization"
#define CFG_ROAM_RSSI_CHANNEL_PENALIZATION_MIN     (0)
#define CFG_ROAM_RSSI_CHANNEL_PENALIZATION_MAX     (15)
#define CFG_ROAM_RSSI_CHANNEL_PENALIZATION_DEFAULT (5)

/*
 * <ini>
 * groam_num_disallowed_aps - Max number of AP's to maintain in LCA list
 * @Min: 0
 * @Max: 8
 * @Default: 3
 *
 * This ini is used to set the maximum number of AP's to be maintained
 * in LCA [Last Connected AP] list.
 *
 * Related: LFR
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ROAM_NUM_DISALLOWED_APS_NAME    "groam_num_disallowed_aps"
#define CFG_ROAM_NUM_DISALLOWED_APS_MIN     (0)
#define CFG_ROAM_NUM_DISALLOWED_APS_MAX     (8)
#define CFG_ROAM_NUM_DISALLOWED_APS_DEFAULT (3)


/*
 * <ini>
 * gEnableNDIMacRandomization - When enabled this will randomize NDI Mac
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * When enabled this will randomize NDI Mac
 *
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RANDOMIZE_NDI_MAC_NAME      "gEnableNDIMacRandomization"
#define CFG_RANDOMIZE_NDI_MAC_MIN       (0)
#define CFG_RANDOMIZE_NDI_MAC_MAX       (1)
#define CFG_RANDOMIZE_NDI_MAC_DEFAULT   (1)

/*
 * <ini>
 * gEnableLPRx - Enable/Disable LPRx
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini Enables or disables the LPRx in FW
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_LPRx_NAME       "gEnableLPRx"
#define CFG_LPRx_MIN         (0)
#define CFG_LPRx_MAX         (1)
#define CFG_LPRx_DEFAULT     (1)

/*
 * <ini>
 * gUpperBrssiThresh - Sets Upper threshold for beacon RSSI
 * @Min: 36
 * @Max: 66
 * @Default: 46
 *
 * This ini sets Upper beacon threshold for beacon RSSI in FW
 * Used to reduced RX chainmask in FW, once this threshold is
 * reached FW will switch to 1X1 (Single chain).
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_UPPER_BRSSI_THRESH_NAME             "gUpperBrssiThresh"
#define CFG_UPPER_BRSSI_THRESH_MIN              (36)
#define CFG_UPPER_BRSSI_THRESH_MAX              (66)
#define CFG_UPPER_BRSSI_THRESH_DEFAULT          (46)

/*
 * <ini>
 * gLowerrBrssiThresh - Sets Lower threshold for beacon RSSI
 * @Min: 6
 * @Max: 36
 * @Default: 26
 *
 * This ini sets Lower beacon threshold for beacon RSSI in FW
 * Used to increase RX chainmask in FW, once this threshold is
 * reached FW will switch to 2X2 chain.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_LOWER_BRSSI_THRESH_NAME     "gLowerBrssiThresh"
#define CFG_LOWER_BRSSI_THRESH_MIN      (6)
#define CFG_LOWER_BRSSI_THRESH_MAX      (36)
#define CFG_LOWER_BRSSI_THRESH_DEFAULT  (26)

/*
 * <ini>
 * gDtim1ChRxEnable - Enable/Disable DTIM 1Chrx feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini Enables or Disables DTIM 1CHRX feature in FW
 * If this flag is set FW enables shutting off one chain
 * while going to power save.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_DTIM_1CHRX_ENABLE_NAME      "gDtim1ChRxEnable"
#define CFG_DTIM_1CHRX_ENABLE_MIN       (0)
#define CFG_DTIM_1CHRX_ENABLE_MAX       (1)
#define CFG_DTIM_1CHRX_ENABLE_DEFAULT   (1)

/*
 * <ini>
 * scan_11d_interval - 11d scan interval in ms
 * @Min: 1 sec
 * @Max: 10 hr
 * @Default: 1 hr
 *
 * This ini sets the 11d scan interval in FW
 *
 *
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SCAN_11D_INTERVAL_NAME      "scan_11d_interval"
#define CFG_SCAN_11D_INTERVAL_DEFAULT   (3600000)
#define CFG_SCAN_11D_INTERVAL_MIN       (1000)
#define CFG_SCAN_11D_INTERVAL_MAX       (36000000)
/*
 * <ini>
 * is_bssid_hint_priority - Set priority for connection with bssid_hint
 * BSSID.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to give priority to BSS for connection which comes
 * as part of bssid_hint
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_IS_BSSID_HINT_PRIORITY_NAME    "is_bssid_hint_priority"
#define CFG_IS_BSSID_HINT_PRIORITY_DEFAULT (0)
#define CFG_IS_BSSID_HINT_PRIORITY_MIN     (0)
#define CFG_IS_BSSID_HINT_PRIORITY_MAX     (1)

/*
 * <ini>
 * rssi_weightage - RSSI Weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 20
 *
 * This ini is used to increase/decrease RSSI weightage in best candidate
 * selection. AP with better RSSI will get more weightage.
 *
 * Related: None
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RSSI_WEIGHTAGE_NAME    "rssi_weightage"
#define CFG_RSSI_WEIGHTAGE_DEFAULT (20)
#define CFG_RSSI_WEIGHTAGE_MIN     (0)
#define CFG_RSSI_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * ht_caps_weightage - HT caps weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 2
 *
 * This ini is used to increase/decrease HT caps weightage in best candidate
 * selection. If AP supports HT caps, AP will get additional Weightage with
 * this param. Weightage will be given only if dot11mode is HT capable.
 *
 * Related: None
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HT_CAPABILITY_WEIGHTAGE_NAME   "ht_caps_weightage"
#define CFG_HT_CAPABILITY_WEIGHTAGE_DEFAULT (2)
#define CFG_HT_CAPABILITY_WEIGHTAGE_MIN     (0)
#define CFG_HT_CAPABILITY_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * vht_caps_weightage - VHT caps Weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 1
 *
 * This ini is used to increase/decrease VHT caps weightage in best candidate
 * selection. If AP supports VHT caps, AP will get additional weightage with
 * this param. Weightage will be given only if dot11mode is VHT capable.
 *
 * Related: None
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_CAPABILITY_WEIGHTAGE_NAME    "vht_caps_weightage"
#define CFG_VHT_CAPABILITY_WEIGHTAGE_DEFAULT (1)
#define CFG_VHT_CAPABILITY_WEIGHTAGE_MIN     (0)
#define CFG_VHT_CAPABILITY_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * he_caps_weightage - HE caps Weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 1
 *
 * This ini is used to increase/decrease HE caps weightage in best candidate
 * selection. If AP supports HE caps, AP will get additional weightage with
 * this param. Weightage will be given only if dot11mode is HE capable.
 *
 * Related: None
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HE_CAPABILITY_WEIGHTAGE_NAME    "he_caps_weightage"
#define CFG_HE_CAPABILITY_WEIGHTAGE_DEFAULT (2)
#define CFG_HE_CAPABILITY_WEIGHTAGE_MIN     (0)
#define CFG_HE_CAPABILITY_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * chan_width_weightage - Channel Width Weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 17
 *
 * This ini is used to increase/decrease Channel Width weightage in best
 * candidate selection. AP with Higher channel width will get higher weightage
 *
 * Related: bandwidth_weight_per_index
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CHAN_WIDTH_WEIGHTAGE_NAME    "chan_width_weightage"
#define CFG_CHAN_WIDTH_WEIGHTAGE_DEFAULT (17)
#define CFG_CHAN_WIDTH_WEIGHTAGE_MIN     (0)
#define CFG_CHAN_WIDTH_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * chan_band_weightage - Channel Band perferance to 5GHZ to
 * calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 2
 *
 * This ini is used to increase/decrease Channel Band Preference weightage
 * in best candidate selection. 5GHZ AP get this additional boost compare to
 * 2GHZ AP before   rssi_pref_5g_rssi_thresh and 2.4Ghz get weightage after
 * rssi_pref_5g_rssi_thresh.
 *
 * Related: rssi_pref_5g_rssi_thresh, band_weight_per_index
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 * gEnableFastPwrTransition - Configuration for fast power transition
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini supported values:
 * 0x0: Phy register retention disabled (Higher timeline, Good for power)
 * 0x1: Phy register retention statically enabled
 * 0x2: Phy register retention enabled/disabled dynamically
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_PHY_REG  "gEnableFastPwrTransition"
#define CFG_PHY_REG_DEFAULT (0x0)
#define CFG_PHY_REG_MIN     (0x0)
#define CFG_PHY_REG_MAX     (0x2)

/*
 * <ini>
 * sae_enabled - Enable/Disable SAE support in driver
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable SAE support in driver
 * Driver will update config to supplicant based on this config.
 *
 * Related: None
 *
 * Supported Feature: SAE
 * Usage: External
 *
 * </ini>
 */

#define CFG_IS_SAE_ENABLED_NAME    "sae_enabled"
#define CFG_IS_SAE_ENABLED_DEFAULT (1)
#define CFG_IS_SAE_ENABLED_MIN     (0)
#define CFG_IS_SAE_ENABLED_MAX     (1)

/*
 * <ini>
 * enable_sae_for_sap - Enable/Disable SAE support in driver for SAP
 * channel&bandwidth in the mission mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable SAE support in driver for SAP mode
 * Driver will process/drop the SAE authentication frames based on this config.
 *
 * Related: None
 *
 * Supported Feature: SAE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IS_SAP_SAE_ENABLED_NAME    "enable_sae_for_sap"
#define CFG_IS_SAP_SAE_ENABLED_DEFAULT (1)
#define CFG_IS_SAP_SAE_ENABLED_MIN     (0)
#define CFG_IS_SAP_SAE_ENABLED_MAX     (1)

/*
 * Type declarations
 */
#define CFG_CHAN_BAND_WEIGHTAGE_NAME    "chan_band_weightage"
#define CFG_CHAN_BAND_WEIGHTAGE_DEFAULT (2)
#define CFG_CHAN_BAND_WEIGHTAGE_MIN     (0)
#define CFG_CHAN_BAND_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * nss_weightage - NSS Weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 16
 *
 * This ini is used to increase/decrease NSS weightage in best candidate
 * selection. If there are two AP, one AP supports 2x2 and another one supports
 * 1x1 and station supports 2X2, first A will get this additional weightage
 * depending on self-capability.
 *
 * Related: nss_weight_per_index
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NSS_WEIGHTAGE_NAME    "nss_weightage"
#define CFG_NSS_WEIGHTAGE_DEFAULT (16)
#define CFG_NSS_WEIGHTAGE_MIN     (0)
#define CFG_NSS_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * beamforming_cap_weightage - Beam Forming Weightage to
 *                             calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 2
 *
 * This ini is used to increase/decrease Beam forming Weightage if some AP
 * support Beam forming or not. If AP supports Beam forming, that AP will get
 * additional boost of this weightage.
 *
 * Related: None
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BEAMFORMING_CAP_WEIGHTAGE_NAME "beamforming_cap_weightage"
#define CFG_BEAMFORMING_CAP_WEIGHTAGE_DEFAULT (2)
#define CFG_BEAMFORMING_CAP_WEIGHTAGE_MIN     (0)
#define CFG_BEAMFORMING_CAP_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * pcl_weightage - PCL Weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 10
 *
 * This ini is used to increase/decrease PCL weightage in best candidate
 * selection. If some APs are in PCL list, those AP will get addition
 * weightage.
 *
 * Related: None
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PCL_WEIGHT_WEIGHTAGE_NAME "pcl_weightage"
#define CFG_PCL_WEIGHT_DEFAULT        (10)
#define CFG_PCL_WEIGHT_MIN            (0)
#define CFG_PCL_WEIGHT_MAX            (100)

/*
 * <ini>
 * channel_congestion_weightage - channel Congestion Weightage to
 * calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 5
 *
 * This ini is used to increase/decrease channel congestion weightage in
 * candidate selection. Congestion is measured with the help of ESP/QBSS load.
 *
 * Related: num_esp_qbss_slots
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CHANNEL_CONGESTION_WEIGHTAGE_NAME "channel_congestion_weightage"
#define CFG_CHANNEL_CONGESTION_WEIGHTAGE_DEFAULT (5)
#define CFG_CHANNEL_CONGESTION_WEIGHTAGE_MIN     (0)
#define CFG_CHANNEL_CONGESTION_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * oce_wan_weightage - OCE WAN DL capacity Weightage to calculate best candidate
 * @Min: 0
 * @Max: 100
 * @Default: 2
 *
 * This ini is used to increase/decrease OCE WAN caps weightage in best
 * candidate selection. If AP have OCE WAN information, give weightage depending
 * on the downaload available capacity.
 *
 * Related: num_oce_wan_slots
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_WAN_WEIGHTAGE_NAME    "oce_wan_weightage"
#define CFG_OCE_WAN_WEIGHTAGE_DEFAULT (2)
#define CFG_OCE_WAN_WEIGHTAGE_MIN     (0)
#define CFG_OCE_WAN_WEIGHTAGE_MAX     (100)

/*
 * <ini>
 * vendor_roam_score_algorithm - Algorithm to calculate AP score
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * By default the value is 0 and default roam algorithm will be used.
 * When the value is 1, the V2 roaming algorithm will be used:
 * For this V2 algo, AP score calculation is based on below equation:
 * AP Score = (RSSIfactor * rssiweight(0.65)) + (CUfactor *cuweight(0.35))
 *
 * Related: None
 *
 * Supported Feature: roam score algorithm
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VENDOR_ROAM_SCORE_ALGORITHM_NAME  "vendor_roam_score_algorithm"
#define CFG_VENDOR_ROAM_SCORE_ALGORITHM_MIN        0
#define CFG_VENDOR_ROAM_SCORE_ALGORITHM_MAX        1
#define CFG_VENDOR_ROAM_SCORE_ALGORITHM_DEFAULT    0

/*
 * <ini>
 * best_rssi_threshold - Best Rssi for score calculation
 * @Min: 0
 * @Max: 96
 * @Default: 55
 *
 * This ini tells limit for best RSSI. RSSI better than this limit are
 * considered as best RSSI. The best RSSI is given full rssi_weightage.
 *
 * Related: rssi_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BEST_RSSI_THRESHOLD_NAME         "best_rssi_threshold"
#define CFG_BEST_RSSI_THRESHOLD_DEFAULT      (55)
#define CFG_BEST_RSSI_THRESHOLD_MIN          (0)
#define CFG_BEST_RSSI_THRESHOLD_MAX          (96)

/*
 * <ini>
 * good_rssi_threshold - Good Rssi for score calculation
 * @Min: 0
 * @Max: 96
 * @Default: 70
 *
 * This ini tells limit for good RSSI. RSSI better than this limit and less
 * than best_rssi_threshold is considered as good RSSI.
 *
 * Related: rssi_weightage, best_rssi_threshold
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GOOD_RSSI_THRESHOLD_NAME          "good_rssi_threshold"
#define CFG_GOOD_RSSI_THRESHOLD_DEFAULT       (70)
#define CFG_GOOD_RSSI_THRESHOLD_MIN           (0)
#define CFG_GOOD_RSSI_THRESHOLD_MAX           (96)

/*
 * <ini>
 * bad_rssi_threshold - Bad Rssi for score calculation
 * @Min: 0
 * @Max: 96
 * @Default: 80
 *
 * This ini tells limit for Bad RSSI. RSSI greater then bad_rssi_threshold
 * is considered as bad RSSI.
 *
 * Related: rssi_weightage, good_rssi_threshold
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAD_RSSI_THRESHOLD_NAME            "bad_rssi_threshold"
#define CFG_BAD_RSSI_THRESHOLD_DEFAULT         (80)
#define CFG_BAD_RSSI_THRESHOLD_MIN             (0)
#define CFG_BAD_RSSI_THRESHOLD_MAX             (96)

/*
 * <ini>
 * good_rssi_pcnt - Percent Score to Good RSSI out of total RSSI score.
 * @Min: 0
 * @Max: 100
 * @Default: 80
 *
 * This ini tells about how much percent should be given to good RSSI(RSSI
 * between best_rssi_threshold and good_rssi_threshold) out of RSSI weightage.
 *
 * Related: rssi_weightage, best_rssi_threshold, good_rssi_threshold
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GOOD_RSSI_PCNT_NAME            "good_rssi_pcnt"
#define CFG_GOOD_RSSI_PCNT_DEFAULT         (80)
#define CFG_GOOD_RSSI_PCNT_MIN             (0)
#define CFG_GOOD_RSSI_PCNT_MAX             (100)

/*
 * <ini>
 * bad_rssi_pcnt - Percent Score to BAD RSSI out of total RSSI score.
 * @Min: 0
 * @Max: 100
 * @Default: 25
 *
 * This ini tells about how much percent should be given to bad RSSI (RSSI
 * between good_rssi_threshold and bad_rssi_threshold) out of RSSI weightage.
 *
 * Related: rssi_weightage, good_rssi_threshold, bad_rssi_threshold
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAD_RSSI_PCNT_NAME            "bad_rssi_pcnt"
#define CFG_BAD_RSSI_PCNT_DEFAULT         (25)
#define CFG_BAD_RSSI_PCNT_MIN             (0)
#define CFG_BAD_RSSI_PCNT_MAX             (100)

/*
 * <ini>
 * good_rssi_bucket_size - Bucket size between best and good RSSI to score.
 * @Min: 1
 * @Max: 10
 * @Default: 5
 *
 * This ini tells about bucket size for scoring between best and good RSSI.
 * Below Best RSSI, 100% score will be given. Between best and good RSSI, RSSI
 * is divided in buckets and score will be assigned bucket wise starting from
 * good_rssi_pcnt.
 *
 * Related: rssi_weightage, good_rssi_pcnt
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GOOD_RSSI_BUCKET_SIZE_NAME            "good_rssi_bucket_size"
#define CFG_GOOD_RSSI_BUCKET_SIZE_DEFAULT         (5)
#define CFG_GOOD_RSSI_BUCKET_SIZE_MIN             (1)
#define CFG_GOOD_RSSI_BUCKET_SIZE_MAX             (10)

/*
 * <ini>
 * bad_rssi_bucket_size - Bucket size between good and bad RSSI to score.
 * @Min: 1
 * @Max: 10
 * @Default: 5
 *
 * This ini tells about bucket size for scoring between good and bad RSSI.
 * Between good and bad RSSI, RSSI is divided in buckets and score will be
 * assigned bucket wise starting from bad_rssi_pcnt.
 *
 * Related: rssi_weightage, bad_rssi_pcnt
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAD_RSSI_BUCKET_SIZE_NAME            "bad_rssi_bucket_size"
#define CFG_BAD_RSSI_BUCKET_SIZE_DEFAULT         (5)
#define CFG_BAD_RSSI_BUCKET_SIZE_MIN             (1)
#define CFG_BAD_RSSI_BUCKET_SIZE_MAX             (10)

/*
 * <ini>
 * rssi_pref_5g_rssi_thresh - A RSSI threshold above which 5 GHz is not favored
 * @Min: 0
 * @Max: 96
 * @Default: 76
 *
 * 5G AP are given chan_band_weightage. This ini tells about RSSI threshold
 * above which 5GHZ is not favored.
 *
 * Related: chan_band_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RSSI_PERF_5G_THRESHOLD_NAME    "rssi_pref_5g_rssi_thresh"
#define CFG_RSSI_PERF_5G_THRESHOLD_DEFAULT (76)
#define CFG_RSSI_PERF_5G_THRESHOLD_MIN     (0)
#define CFG_RSSI_PERF_5G_THRESHOLD_MAX     (96)

/*
 * <ini>
 * bandwidth_weight_per_index - percentage as per bandwidth
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x6432190C
 *
 * This INI give percentage value of chan_width_weightage to be used as per
 * peer bandwidth. Self BW is also considered while calculating score. Eg if
 * self BW is 20 MHZ 10% will be given for all AP irrespective of the AP
 * capability.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): 20 MHz - Def 12%
 *     1 Index (BITS 8-15): 40 MHz - Def 25%
 *     2 Index (BITS 16-23): 80 MHz - Def 50%
 *     3 Index (BITS 24-31): 160 MHz - Def 100%
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: chan_width_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAND_WIDTH_WEIGHT_PER_INDEX_NAME       "bandwidth_weight_per_index"
#define CFG_BAND_WIDTH_WEIGHT_PER_INDEX_DEFAULT    (0x6432190C)
#define CFG_BAND_WIDTH_WEIGHT_PER_INDEX_MIN        (0x00000000)
#define CFG_BAND_WIDTH_WEIGHT_PER_INDEX_MAX        (0x64646464)

/*
 * <ini>
 * nss_weight_per_index - percentage as per NSS
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x6432190C
 *
 * This INI give percentage value of nss_weightage to be used as per peer NSS.
 * Self NSS capability is also considered. Eg if self NSS is 1x1 10% will be
 * given for all AP irrespective of the AP capability.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): 1X1- Def 12%
 *     1 Index (BITS 8-15): 2X2- Def 25%
 *     2 Index (BITS 16-23): 3X3- Def 50%
 *     3 Index (BITS 24-31): 4X4- Def 100%
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: nss_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NSS_WEIGHT_PER_INDEX_NAME       "nss_weight_per_index"
#define CFG_NSS_WEIGHT_PER_INDEX_DEFAULT    (0x6432190C)
#define CFG_NSS_WEIGHT_PER_INDEX_MIN        (0x00000000)
#define CFG_NSS_WEIGHT_PER_INDEX_MAX        (0x64646464)

/*
 * <ini>
 * band_weight_per_index - percentage as per band
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x0000644B
 *
 * This INI give percentage value of chan_band_weightage to be used as per band.
 * If RSSI is greater than rssi_pref_5g_rssi_thresh preference is given for 5Ghz
 * else, it's given for 2.4Ghz.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): 2.4GHz - Def 10%
 *     1 Index (BITS 8-15): 5GHz - Def 20%
 *     2 Index (BITS 16-23): Reserved
 *     3 Index (BITS 24-31): Reserved
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: chan_band_weightage, rssi_pref_5g_rssi_thresh
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAND_WEIGHT_PER_INDEX_NAME      "band_weight_per_index"
#define CFG_BAND_WEIGHT_PER_INDEX_DEFAULT   (0x0000644B)
#define CFG_BAND_WEIGHT_PER_INDEX_MIN       (0x00000000)
#define CFG_BAND_WEIGHT_PER_INDEX_MAX       (0x64646464)

/*
 * <ini>
 * num_esp_qbss_slots - number of slots in which the esp/qbss load will
 * be divided
 *
 * @Min: 1
 * @Max: 15
 * @Default: 4
 *
 * Number of slots in which the esp/qbss load will be divided. Max 15. index 0
 * is used for 'not_present. Num_slot will equally divide 100. e.g, if
 * num_slot = 4 slot 1 = 0-25% load, slot 2 = 26-50% load, slot 3 = 51-75% load,
 * slot 4 = 76-100% load. Remaining unused index can be 0.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ESP_QBSS_SLOTS_NAME      "num_esp_qbss_slots"
#define CFG_ESP_QBSS_SLOTS_DEFAULT   (4)
#define CFG_ESP_QBSS_SLOTS_MIN       (1)
#define CFG_ESP_QBSS_SLOTS_MAX       (15)

/*
 * <ini>
 * esp_qbss_score_idx3_to_0 - percentage for  esp/qbss load for slots 0-3
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x19326432
 *
 * This INI give percentage value of channel_congestion_weightage to be used as
 * index in which the load value falls. Index 0 is for percentage when ESP/QBSS
 * is not present.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): when ESP/QBSS is not present
 *     1 Index (BITS 8-15): SLOT_1
 *     2 Index (BITS 16-23): SLOT_2
 *     3 Index (BITS 24-31): SLOT_3
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: channel_congestion_weightage, num_esp_qbss_slots
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ESP_QBSS_SCORE_IDX3_TO_0_NAME      "esp_qbss_score_idx3_to_0"
#define CFG_ESP_QBSS_SCORE_IDX3_TO_0_DEFAULT   (0x19326432)
#define CFG_ESP_QBSS_SCORE_IDX3_TO_0_MIN       (0x00000000)
#define CFG_ESP_QBSS_SCORE_IDX3_TO_0_MAX       (0x64646464)

/*
 * <ini>
 * esp_qbss_score_idx7_to_4 - percentage for  esp/qbss load for slots 4-7
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x0000000A
 *
 * This INI give percentage value of channel_congestion_weightage to be used as
 * index in which the load value falls. Used only if num_esp_qbss_slots is
 * greater than 3.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): SLOT_4
 *     1 Index (BITS 8-15): SLOT_5
 *     2 Index (BITS 16-23): SLOT_6
 *     3 Index (BITS 24-31): SLOT_7
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: channel_congestion_weightage, num_esp_qbss_slots
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ESP_QBSS_SCORE_IDX7_TO_4_NAME      "esp_qbss_score_idx7_to_4"
#define CFG_ESP_QBSS_SCORE_IDX7_TO_4_DEFAULT   (0x0000000A)
#define CFG_ESP_QBSS_SCORE_IDX7_TO_4_MIN       (0x00000000)
#define CFG_ESP_QBSS_SCORE_IDX7_TO_4_MAX       (0x64646464)


/*
 * <ini>
 * esp_qbss_score_idx11_to_8 - percentage for  esp/qbss load for slots 8-11
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x00000000
 *
 * This INI give percentage value of channel_congestion_weightage to be used as
 * index in which the load value falls. Used only if num_esp_qbss_slots is
 * greater than 7.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): SLOT_8
 *     1 Index (BITS 8-15): SLOT_9
 *     2 Index (BITS 16-23): SLOT_10
 *     3 Index (BITS 24-31): SLOT_11
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: channel_congestion_weightage, num_esp_qbss_slots
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ESP_QBSS_SCORE_IDX11_TO_8_NAME      "esp_qbss_score_idx11_to_8"
#define CFG_ESP_QBSS_SCORE_IDX11_TO_8_DEFAULT   (0x00000000)
#define CFG_ESP_QBSS_SCORE_IDX11_TO_8_MIN       (0x00000000)
#define CFG_ESP_QBSS_SCORE_IDX11_TO_8_MAX       (0x64646464)


/*
 * <ini>
 * esp_qbss_score_idx15_to_12 - percentage for  esp/qbss load for slots 12-15
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x00000000
 *
 * This INI give percentage value of channel_congestion_weightage to be used as
 * index in which the load value falls. Used only if num_esp_qbss_slots is
 * greater than 11.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): SLOT_12
 *     1 Index (BITS 8-15): SLOT_13
 *     2 Index (BITS 16-23): SLOT_14
 *     3 Index (BITS 24-31): SLOT_15
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: channel_congestion_weightage, num_esp_qbss_slots
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ESP_QBSS_SCORE_IDX15_TO_12_NAME      "esp_qbss_score_idx15_to_12"
#define CFG_ESP_QBSS_SCORE_IDX15_TO_12_DEFAULT   (0x00000000)
#define CFG_ESP_QBSS_SCORE_IDX15_TO_12_MIN       (0x00000000)
#define CFG_ESP_QBSS_SCORE_IDX15_TO_12_MAX       (0x64646464)

/*
 * <ini>
 * num_oce_wan_slots - number of slots in which the oce wan metrics will
 * be divided
 *
 * @Min: 1
 * @Max: 15
 * @Default: 8
 *
 * Number of slots in which the oce wan metrics will be divided. Max 15. index 0
 * is used for not_present. Num_slot will equally divide 100. e.g, if
 * num_slot = 4 slot 1 = 0-3 DL CAP, slot 2 = 4-7 DL CAP, slot 3 = 8-11 DL CAP,
 * slot 4 = 12-15 DL CAP. Remaining unused index can be 0.
 *
 * Related: oce_wan_weightage
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_WAN_SLOTS_NAME      "num_oce_wan_slots"
#define CFG_OCE_WAN_SLOTS_DEFAULT   (15)
#define CFG_OCE_WAN_SLOTS_MIN       (1)
#define CFG_OCE_WAN_SLOTS_MAX       (15)

/*
 * <ini>
 * oce_wan_score_idx3_to_0 - percentage for OCE WAN metrics score for slots 0-3
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x00000032
 *
 * This INI give percentage value of OCE WAN metrics DL CAP, to be used as
 * index in which the DL CAP value falls. Index 0 is for percentage when
 * OCE WAN metrics DL CAP is not present.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): when OCE WAN metrics DL CAP is not present
 *     1 Index (BITS 8-15): SLOT_1
 *     2 Index (BITS 16-23): SLOT_2
 *     3 Index (BITS 24-31): SLOT_3
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: num_oce_wan_slots, oce_wan_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_WAN_SCORE_IDX3_TO_0_NAME      "oce_wan_score_idx3_to_0"
#define CFG_OCE_WAN_SCORE_IDX3_TO_0_DEFAULT   (0x00000032)
#define CFG_OCE_WAN_SCORE_IDX3_TO_0_MIN       (0x00000000)
#define CFG_OCE_WAN_SCORE_IDX3_TO_0_MAX       (0x64646464)

/*
 * <ini>
 * oce_wan_score_idx7_to_4 - percentage for OCE WAN metrics score for slots 4-7
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x00000000
 *
 * This INI give percentage value of OCE WAN metrics DL CAP, to be used as
 * index in which the DL CAP value falls. Used only if num_oce_wan_slots is
 * greater than 3.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): SLOT_4
 *     1 Index (BITS 8-15): SLOT_5
 *     2 Index (BITS 16-23): SLOT_6
 *     3 Index (BITS 24-31): SLOT_7
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: num_oce_wan_slots, oce_wan_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_WAN_SCORE_IDX7_TO_4_NAME      "oce_wan_score_idx7_to_4"
#define CFG_OCE_WAN_SCORE_IDX7_TO_4_DEFAULT   (0x00000000)
#define CFG_OCE_WAN_SCORE_IDX7_TO_4_MIN       (0x00000000)
#define CFG_OCE_WAN_SCORE_IDX7_TO_4_MAX       (0x64646464)

/*
 * <ini>
 * oce_wan_score_idx11_to_8 - percentage for OCE WAN metrics score for slot 8-11
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x06030000
 *
 * This INI give percentage value of OCE WAN metrics DL CAP, to be used as
 * index in which the DL CAP value falls. Used only if num_oce_wan_slots is
 * greater than 7.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): SLOT_8
 *     1 Index (BITS 8-15): SLOT_9
 *     2 Index (BITS 16-23): SLOT_10
 *     3 Index (BITS 24-31): SLOT_11
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: num_oce_wan_slots, oce_wan_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_WAN_SCORE_IDX11_TO_8_NAME      "oce_wan_score_idx11_to_8"
#define CFG_OCE_WAN_SCORE_IDX11_TO_8_DEFAULT   (0x06030000)
#define CFG_OCE_WAN_SCORE_IDX11_TO_8_MIN       (0x00000000)
#define CFG_OCE_WAN_SCORE_IDX11_TO_8_MAX       (0x64646464)

/*
 * <ini>
 * oce_wan_score_idx15_to_12 - % for OCE WAN metrics score for slot 12-15
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x6432190C
 *
 * This INI give percentage value of OCE WAN metrics DL CAP, to be used as
 * index in which the DL CAP value falls. Used only if num_oce_wan_slots is
 * greater than 11.
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): SLOT_12
 *     1 Index (BITS 8-15): SLOT_13
 *     2 Index (BITS 16-23): SLOT_14
 *     3 Index (BITS 24-31): SLOT_15
 * These percentage values are stored in HEX. For any index max value, can be 64
 *
 * Related: num_oce_wan_slots, oce_wan_weightage
 *
 * Supported Feature: STA Candidate selection
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_WAN_SCORE_IDX15_TO_12_NAME      "oce_wan_score_idx15_to_12"
#define CFG_OCE_WAN_SCORE_IDX15_TO_12_DEFAULT   (0x6432190C)
#define CFG_OCE_WAN_SCORE_IDX15_TO_12_MIN       (0x00000000)
#define CFG_OCE_WAN_SCORE_IDX15_TO_12_MAX       (0x64646464)

/*
 * <ini>
 * enable_scoring_for_roam - enable/disable scoring logic in FW for candidate
 * selection during roaming
 *
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable scoring logic in FW for candidate
 * selection during roaming.
 *
 * Supported Feature: STA Candidate selection by FW during roaming based on
 * scoring logic.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SCORING_FOR_ROAM_NAME     "enable_scoring_for_roam"
#define CFG_ENABLE_SCORING_FOR_ROAM_DEFAULT  (1)
#define CFG_ENABLE_SCORING_FOR_ROAM_MIN      (0)
#define CFG_ENABLE_SCORING_FOR_ROAM_MAX      (1)

/*
 * <ini>
 * force_rsne_override - force rsnie override from user
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable test mode to force rsne override used in
 * security enhancement test cases to pass the RSNIE sent by user in
 * assoc request.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: internal
 *
 * </ini>
 */
#define CFG_FORCE_RSNE_OVERRIDE_NAME    "force_rsne_override"
#define CFG_FORCE_RSNE_OVERRIDE_MIN     (0)
#define CFG_FORCE_RSNE_OVERRIDE_MAX     (1)
#define CFG_FORCE_RSNE_OVERRIDE_DEFAULT (0)

/*
 * <ini>
 * gChanSwitchHostapdRateEnabled - Enable/disable hostapd rate when doing SAP
 * channel switch
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set supported rates calculated from hostapd.conf file
 * or not when doing SAP channel switch. It must set it to 0 when cross-band
 * channel switch happens such as from 2G to 5G or 5G to 2G.
 *
 * Related: When doing SAP channel switch, if gChanSwitchHostapdRateEnabled is
 * set to 1, supported rates will be calculated from hostapd.conf file,
 * if gChanSwitchHostapdRateEnabled is set to 0, supported rates will be
 * calculated from driver default rates.
 *
 * Supported Feature: SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CHAN_SWITCH_HOSTAPD_RATE_ENABLED_NAME \
	"gChanSwitchHostapdRateEnabled"
#define CFG_CHAN_SWITCH_HOSTAPD_RATE_ENABLED_MIN     (0)
#define CFG_CHAN_SWITCH_HOSTAPD_RATE_ENABLED_MAX     (1)
#define CFG_CHAN_SWITCH_HOSTAPD_RATE_ENABLED_DEFAULT (0)

/*
 * <ini>
 * g_mbo_candidate_rssi_thres - Candidate AP's minimum RSSI to accept
 * @Min: -120
 * @Max: 0
 * @Default: -72
 *
 * This ini specifies the minimum RSSI value a candidate should have to accept
 * it as a target for transition.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CANDIDATE_RSSI_THRESHOLD_NAME   "g_mbo_candidate_rssi_thres"
#define CFG_CANDIDATE_RSSI_THRESHOLD_DEFAULT    (-72)
#define CFG_CANDIDATE_RSSI_THRESHOLD_MIN        (-120)
#define CFG_CANDIDATE_RSSI_THRESHOLD_MAX        (0)

/*
 * <ini>
 * g_mbo_current_rssi_thres - Connected AP's RSSI threshold to consider a
 * transition
 * @Min: -120
 * @Max: 0
 * @Default: -65
 *
 * This ini is used to configure connected AP's RSSI threshold value to consider
 * a transition.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CURRENT_RSSI_THRESHOLD_NAME     "g_mbo_current_rssi_thres"
#define CFG_CURRENT_RSSI_THRESHOLD_DEFAULT      (-65)
#define CFG_CURRENT_RSSI_THRESHOLD_MIN          (-120)
#define CFG_CURRENT_RSSI_THRESHOLD_MAX          (0)

/*
 * <ini>
 * g_mbo_current_rssi_mcc_thres - connected AP's RSSI threshold value to prefer
 * against a MCC
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * This ini is used to configure connected AP's minimum RSSI threshold that is
 * preferred against a MCC case, if the candidate can cause MCC.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CUR_RSSI_MCC_THRESHOLD_NAME       "g_mbo_current_rssi_mcc_thres"
#define CFG_MBO_CUR_RSSI_MCC_THRESHOLD_DEFAULT    (-75)
#define CFG_MBO_CUR_RSSI_MCC_THRESHOLD_MIN        (-120)
#define CFG_MBO_CUR_RSSI_MCC_THRESHOLD_MAX        (0)

/*
 * <ini>
 * g_mbo_candidate_rssi_btc_thres -  Candidate AP's minimum RSSI threshold to
 * prefer it even in case of BT coex
 * @Min: -120
 * @Max: 0
 * @Default: -70
 *
 * This ini is used to configure candidate AP's minimum RSSI threshold to prefer
 * it for transition even in case of BT coex.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CAND_RSSI_BTC_THRESHOLD_NAME    "g_mbo_candidate_rssi_btc_thres"
#define CFG_MBO_CAND_RSSI_BTC_THRESHOLD_DEFAULT (-70)
#define CFG_MBO_CAND_RSSI_BTC_THRESHOLD_MIN     (-120)
#define CFG_MBO_CAND_RSSI_BTC_THRESHOLD_MAX     (0)

/*
 * <ini>
 * g_enable_packet_filter_bitmap - Enable Packet filters before going into
 * suspend mode
 * @Min: 0
 * @Max: 63
 * @Default: 0
 * Below is the Detailed bit map of the Filters
 * bit-0 : IPv6 multicast
 * bit-1 : IPv4 multicast
 * bit-2 : IPv4 broadcast
 * bit-3 : XID - Exchange station Identification packet, solicits the
 * identification of the receiving station
 * bit-4 : STP - Spanning Tree Protocol, builds logical loop free topology
 * bit-5 : DTP/LLC/CDP
 * DTP - Dynamic Trunking Protocol is used by Ciscoswitches to negotiate
 * whether an interconnection between two switches should be put into access or
 * trunk mode
 * LLC - Logical link control, used for multiplexing, flow & error control
 * CDP - Cisco Discovery Protocol packet contains information about the cisco
 * devices in the network
 *
 * This ini support to enable above mentioned packet filters
 * when target goes to suspend mode, clear those when resume
 *
 * Related: None
 *
 * Supported Feature: PACKET FILTERING
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_PACKET_FILTERS_NAME     "g_enable_packet_filter_bitmap"
#define CFG_ENABLE_PACKET_FILTERS_DEFAULT  (0)
#define CFG_ENABLE_PACKET_FILTERS_MIN      (0)
#define CFG_ENABLE_PACKET_FILTERS_MAX      (63)


/*
 * <ini>
 * gDfsBeaconTxEnhanced - beacon tx enhanced
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enhance dfs beacon tx
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DFS_BEACON_TX_ENHANCED         "gDfsBeaconTxEnhanced"
#define CFG_DFS_BEACON_TX_ENHANCED_MIN     (0)
#define CFG_DFS_BEACON_TX_ENHANCED_MAX     (1)
#define CFG_DFS_BEACON_TX_ENHANCED_DEFAULT (0)

/*
 * gReducedBeaconInterval - beacon interval reduced
 * @Min: 0
 * @Max: 100
 * @Default: 0
 *
 * This ini is used to reduce beacon interval before channel
 * switch (when val great than 0, or the feature is disabled).
 * It would reduce the downtime on the STA side which is
 * waiting for beacons from the AP to resume back transmission.
 * Switch back the beacon_interval to its original value after
 * channel switch based on the timeout.
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_REDUCED_BEACON_INTERVAL         "gReducedBeaconInterval"
#define CFG_REDUCED_BEACON_INTERVAL_MIN     (0)
#define CFG_REDUCED_BEACON_INTERVAL_MAX     (100)
#define CFG_REDUCED_BEACON_INTERVAL_DEFAULT (0)

/*
 * oce_enable_rssi_assoc_reject - Enable/disable rssi based assoc rejection
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable rssi based assoc rejection. If this is
 * disabled then OCE ini oce_sta_enable will also be disabled and OCE IE will
 * not be sent in frames.
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_ENABLE_RSSI_BASED_ASSOC_REJECT_NAME \
	"oce_enable_rssi_assoc_reject"
#define CFG_OCE_ENABLE_RSSI_BASED_ASSOC_REJECT_MIN     (0)
#define CFG_OCE_ENABLE_RSSI_BASED_ASSOC_REJECT_MAX     (1)
#define CFG_OCE_ENABLE_RSSI_BASED_ASSOC_REJECT_DEFAULT (1)

/*
 * <ini>
 * oce_enable_probe_req_rate - Set probe request rate
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set probe request rate to 5.5Mbps as per OCE requirement
 * in 2.4G band
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_PROBE_REQ_RATE_NAME    "oce_enable_probe_req_rate"
#define CFG_OCE_PROBE_REQ_RATE_MIN     (0)
#define CFG_OCE_PROBE_REQ_RATE_MAX     (1)
#define CFG_OCE_PROBE_REQ_RATE_DEFAULT (1)

/*
 * <ini>
 * oce_enable_probe_resp_rate - Set probe response rate
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set probe response rate to 5.5Mbps as per OCE requirement
 * in 2.4G band
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_PROBE_RSP_RATE_NAME    "oce_enable_probe_resp_rate"
#define CFG_OCE_PROBE_RSP_RATE_MIN     (0)
#define CFG_OCE_PROBE_RSP_RATE_MAX     (1)
#define CFG_OCE_PROBE_RSP_RATE_DEFAULT (0)

/*
 * <ini>
 * oce_enable_beacon_rate - Set beacon rate
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set beacon rate to 5.5Mbps as per OCE requirement in
 * 2.4G band
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_BEACON_RATE_NAME    "oce_enable_beacon_rate"
#define CFG_OCE_BEACON_RATE_MIN     (0)
#define CFG_OCE_BEACON_RATE_MAX     (1)
#define CFG_OCE_BEACON_RATE_DEFAULT (0)

/*
 * <ini>
 * oce_enable_probe_req_deferral - Enable/disable probe request deferral
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable probe request deferral as per OCE spec
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_PROBE_REQ_DEFERRAL_NAME    "oce_enable_probe_req_deferral"
#define CFG_ENABLE_PROBE_REQ_DEFERRAL_MIN     (0)
#define CFG_ENABLE_PROBE_REQ_DEFERRAL_MAX     (1)
#define CFG_ENABLE_PROBE_REQ_DEFERRAL_DEFAULT (1)

/*
 * <ini>
 * oce_enable_fils_discovery_sap - Enable/disable fils discovery in sap mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable fils discovery in sap mode
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_FILS_DISCOVERY_SAP_NAME    "oce_enable_fils_discovery_sap"
#define CFG_ENABLE_FILS_DISCOVERY_SAP_MIN     (0)
#define CFG_ENABLE_FILS_DISCOVERY_SAP_MAX     (1)
#define CFG_ENABLE_FILS_DISCOVERY_SAP_DEFAULT (1)

/*
 * <ini>
 * enable_esp_for_roam - Enable/disable esp feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable ESP(Estimated service parameters) IE
 * parsing and decides whether firmware will include this in its scoring algo.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_ESP_FEATURE_NAME    "enable_esp_for_roam"
#define CFG_ENABLE_ESP_FEATURE_MIN     (0)
#define CFG_ENABLE_ESP_FEATURE_MAX     (1)
#define CFG_ENABLE_ESP_FEATURE_DEFAULT (1)

/*
 * <ini>
 * tx_chain_mask_2g - tx chain mask for 2g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set tx chain mask for 2g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * tx_chain_mask_2g=0 : don't care
 * tx_chain_mask_2g=1 : for 2g tx use chain 0
 * tx_chain_mask_2g=2 : for 2g tx use chain 1
 * tx_chain_mask_2g=3 : for 2g tx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_CHAIN_MASK_2G_NAME    "tx_chain_mask_2g"
#define CFG_TX_CHAIN_MASK_2G_MIN     (0)
#define CFG_TX_CHAIN_MASK_2G_MAX     (3)
#define CFG_TX_CHAIN_MASK_2G_DEFAULT (0)


/*
 * <ini>
 * tx_chain_mask_5g - tx chain mask for 5g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set tx chain mask for 5g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * tx_chain_mask_5g=0 : don't care
 * tx_chain_mask_5g=1 : for 5g tx use chain 0
 * tx_chain_mask_5g=2 : for 5g tx use chain 1
 * tx_chain_mask_5g=3 : for 5g tx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_CHAIN_MASK_5G_NAME    "tx_chain_mask_5g"
#define CFG_TX_CHAIN_MASK_5G_MIN     (0)
#define CFG_TX_CHAIN_MASK_5G_MAX     (3)
#define CFG_TX_CHAIN_MASK_5G_DEFAULT (0)


/*
 * <ini>
 * rx_chain_mask_2g - rx chain mask for 2g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set rx chain mask for 2g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * rx_chain_mask_2g=0 : don't care
 * rx_chain_mask_2g=1 : for 2g rx use chain 0
 * rx_chain_mask_2g=2 : for 2g rx use chain 1
 * rx_chain_mask_2g=3 : for 2g rx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_CHAIN_MASK_2G_NAME    "rx_chain_mask_2g"
#define CFG_RX_CHAIN_MASK_2G_MIN     (0)
#define CFG_RX_CHAIN_MASK_2G_MAX     (3)
#define CFG_RX_CHAIN_MASK_2G_DEFAULT (0)


/*
 * <ini>
 * rx_chain_mask_5g - rx chain mask for 5g
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini will set rx chain mask for 5g. To use the ini, make sure:
 * gSetTxChainmask1x1/gSetRxChainmask1x1 = 0,
 * gDualMacFeatureDisable = 1
 * gEnable2x2 = 0
 *
 * rx_chain_mask_5g=0 : don't care
 * rx_chain_mask_5g=1 : for 5g rx use chain 0
 * rx_chain_mask_5g=2 : for 5g rx use chain 1
 * rx_chain_mask_5g=3 : for 5g rx can use either chain
 *
 * Related: None
 *
 * Supported Feature: All profiles
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_CHAIN_MASK_5G_NAME    "rx_chain_mask_5g"
#define CFG_RX_CHAIN_MASK_5G_MIN     (0)
#define CFG_RX_CHAIN_MASK_5G_MAX     (3)
#define CFG_RX_CHAIN_MASK_5G_DEFAULT (0)

/*
 * <ini>
 * btm_offload_config - Configure BTM
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to configure BTM
 *
 * Bit 0: Enable/Disable the BTM offload. Set this to 1 will
 * enable and 0 will disable BTM offload.
 *
 * BIT 2, 1: Action on non matching candidate with cache. If a BTM request
 * is received from AP then the candidate AP's may/may-not be present in
 * the firmware scan cache . Based on below config firmware will decide
 * whether to forward BTM frame to host or consume with firmware and proceed
 * with Roaming to candidate AP.
 * 00 scan and consume
 * 01 no scan and forward to host
 * 10, 11 reserved
 *
 * BIT 5, 4, 3: Roaming handoff decisions on multiple candidates match
 * 000 match if exact BSSIDs are found
 * 001 match if at least one top priority BSSID only
 * 010, 011, 100, 101, 110, 111 reserved
 *
 * BIT 6: Set this to 1 will send BTM query frame and 0 not sent.
 *
 * BIT 7: Roam to BTM candidates based on the roam score instead of BTM
 * preferred value
 *
 * BIT 8: If AP does not support Neighbor report response, STA should
 * request BTM query to get BTM request and check neighbor report exists
 * or not. If Neighbor report exists, STA can use this information to update
 * cached channel information
 *
 * BIT 9: When ever roaming is triggered after a successful roam scan a BTM
 * query is sends to current connected AP which is 11v capable including the
 * preferred candidate list obtained as part of roam scan with preference filled
 * based on our internal scoring logic.
 *
 * BIT 10-31: Reserved
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_ENABLE_NAME      "btm_offload_config"
#define CFG_BTM_ENABLE_MIN       (0x00000000)
#define CFG_BTM_ENABLE_MAX       (0xffffffff)
#define CFG_BTM_ENABLE_DEFAULT   (0x00000001)

/*
 * <ini>
 * btm_solicited_timeout - timeout value for waiting BTM request
 * @Min: 1
 * @Max: 10000
 * @Default: 100
 *
 * This ini is used to configure timeout value for waiting BTM request.
 * Unit: millionsecond
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_SOLICITED_TIMEOUT           "btm_solicited_timeout"
#define CFG_BTM_SOLICITED_TIMEOUT_MIN       (1)
#define CFG_BTM_SOLICITED_TIMEOUT_MAX       (10000)
#define CFG_BTM_SOLICITED_TIMEOUT_DEFAULT   (100)

/*
 * <ini>
 * btm_max_attempt_cnt - Maximum attempt for sending BTM query to ESS
 * @Min: 1
 * @Max: 0xFFFFFFFF
 * @Default: 3
 *
 * This ini is used to configure maximum attempt for sending BTM query to ESS.
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_MAX_ATTEMPT_CNT           "btm_max_attempt_cnt"
#define CFG_BTM_MAX_ATTEMPT_CNT_MIN       (0x00000001)
#define CFG_BTM_MAX_ATTEMPT_CNT_MAX       (0xFFFFFFFF)
#define CFG_BTM_MAX_ATTEMPT_CNT_DEFAULT   (0x00000003)

/*
 * <ini>
 * sticky_time - Stick time after roaming to new AP by BTM
 * @Min: 0
 * @Max: 0x0000FFFF
 * @Default: 300
 *
 * This ini is used to configure Stick time after roaming to new AP by BTM.
 * Unit: seconds
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_STICKY_TIME           "btm_sticky_time"
#define CFG_BTM_STICKY_TIME_MIN       (0x00000000)
#define CFG_BTM_STICKY_TIME_MAX       (0x0000FFFF)
#define CFG_BTM_STICKY_TIME_DEFAULT   (300)

/*
 * <ini>
 * btm_query_bitmask - To send BTM query with candidate list on various roam
 * scans reasons
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0x8
 *
 * This new ini is introduced to configure the bitmask for various roam scan
 * reasons. Fw sends "BTM query with preferred candidate list" only for those
 * roam scans which are enable through this bitmask.

 * For Example:
 * Bitmask : 0x8 (LOW_RSSI) refer enum WMI_ROAM_TRIGGER_REASON_ID
 * Bitmask : 0xDA (PER, LOW_RSSI, HIGH_RSSI, MAWC, DENSE)
 * refer enum WMI_ROAM_TRIGGER_REASON_ID
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_QUERY_BITMASK_NAME    "btm_query_bitmask"
#define CFG_BTM_QUERY_BITMASK_MIN     (0)
#define CFG_BTM_QUERY_BITMASK_MAX     (0xFFFFFFFF)
#define CFG_BTM_QUERY_BITMASK_DEFAULT (0x8)

/*
 * <ini>
 * gcmp_enabled - ini to enable/disable GCMP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Currently Firmware update the sequence number for each TID with 2^3
 * because of security issues. But with this PN mechanism, throughput drop
 * is observed. With this ini FW takes the decision to trade off between
 * security and throughput
 *
 * Supported Feature: STA/SAP/P2P
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_GCMP_NAME    "gcmp_enabled"
#define CFG_ENABLE_GCMP_MIN     (0)
#define CFG_ENABLE_GCMP_MAX     (1)
#define CFG_ENABLE_GCMP_DEFAULT (1)

/*
 * <ini>
 * 11k_offload_enable_bitmask - Bitmask to enable 11k offload to FW
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set which of the 11k features is offloaded to FW
 * Currently Neighbor Report Request is supported for offload and is enabled
 * by default
 * B0: Offload 11k neighbor report requests
 * B1-B31: Reserved
 *
 * Related : None
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_OFFLOAD_11K_ENABLE_BITMASK_NAME    "11k_offload_enable_bitmask"
#define CFG_OFFLOAD_11K_ENABLE_BITMASK_MIN     (0)
#define CFG_OFFLOAD_11K_ENABLE_BITMASK_MAX     (1)
#define CFG_OFFLOAD_11K_ENABLE_BITMASK_DEFAULT (1)

#define OFFLOAD_11K_BITMASK_NEIGHBOR_REPORT_REQUEST  0x1
/*
 * <ini>
 * nr_offload_params_bitmask - bitmask to specify which of the
 * neighbor report offload params are valid in the ini
 * frame
 * @Min: 0
 * @Max: 63
 * @Default: 63
 *
 * This ini specifies which of the neighbor report offload params are valid
 * and should be considered by the FW. The bitmask is as follows
 * B0: nr_offload_time_offset
 * B1: nr_offload_low_rssi_offset
 * B2: nr_offload_bmiss_count_trigger
 * B3: nr_offload_per_threshold_offset
 * B4: nr_offload_cache_timeout
 * B5: nr_offload_max_req_cap
 * B6-B7: Reserved
 *
 * Related : 11k_offload_enable_bitmask
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_OFFLOAD_NEIGHBOR_REPORT_PARAMS_BITMASK_NAME \
	"nr_offload_params_bitmask"
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PARAMS_BITMASK_MIN      (0)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PARAMS_BITMASK_MAX      (63)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PARAMS_BITMASK_DEFAULT  (63)

/*
 * <ini>
 * nr_offload_time_offset - time interval in seconds after the
 * neighbor report offload command to send the first neighbor report request
 * frame
 * @Min: 0
 * @Max: 3600
 * @Default: 30
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_TIME_OFFSET_NAME \
	"nr_offload_time_offset"
#define CFG_OFFLOAD_NEIGHBOR_REPORT_TIME_OFFSET_MIN      (0)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_TIME_OFFSET_MAX      (3600)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_TIME_OFFSET_DEFAULT  (30)

/*
 * <ini>
 * nr_offload_low_rssi_offset - offset from the roam RSSI threshold
 * to trigger the neighbor report request frame (in dBm)
 * @Min: 4
 * @Max: 10
 * @Default: 4
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_LOW_RSSI_OFFSET_NAME \
	"nr_offload_low_rssi_offset"
#define CFG_OFFLOAD_NEIGHBOR_REPORT_LOW_RSSI_OFFSET_MIN     (4)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_LOW_RSSI_OFFSET_MAX     (10)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_LOW_RSSI_OFFSET_DEFAULT (4)

/*
 * <ini>
 * nr_offload_bmiss_count_trigger - Number of beacon miss events to
 * trigger a neighbor report request frame
 * @Min: 1
 * @Max: 5
 * @Default: 1
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_BMISS_COUNT_TRIGGER_NAME \
	"nr_offload_bmiss_count_trigger"
#define CFG_OFFLOAD_NEIGHBOR_REPORT_BMISS_COUNT_TRIGGER_MIN     (1)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_BMISS_COUNT_TRIGGER_MAX     (5)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_BMISS_COUNT_TRIGGER_DEFAULT (1)

/*
 * <ini>
 * nr_offload_per_threshold_offset - offset from PER threshold to
 * trigger a neighbor report request frame (in %)
 * @Min: 5
 * @Max: 20
 * @Default: 5
 *
 * This ini is used to set the neighbor report offload parameter:
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PER_THRESHOLD_OFFSET_NAME \
	"nr_offload_per_threshold_offset"
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PER_THRESHOLD_OFFSET_MIN     (5)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PER_THRESHOLD_OFFSET_MAX     (20)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PER_THRESHOLD_OFFSET_DEFAULT (5)

/*
 * <ini>
 * nr_offload_cache_timeout - time in seconds after which the
 * neighbor report cache is marked as timed out and any of the triggers would
 * cause a neighbor report request frame to be sent.
 * @Min: 5
 * @Max: 86400
 * @Default: 1200
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_CACHE_TIMEOUT_NAME \
	"nr_offload_cache_timeout"
#define CFG_OFFLOAD_NEIGHBOR_REPORT_CACHE_TIMEOUT_MIN     (5)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_CACHE_TIMEOUT_MAX     (86400)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_CACHE_TIMEOUT_DEFAULT (1200)

/*
 * <ini>
 * nr_offload_max_req_cap - Max number of neighbor
 * report requests that can be sent to a connected peer in the current session.
 * This counter is reset once a successful roam happens or at cache timeout
 * @Min: 3
 * @Max: 300
 * @Default: 3
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_MAX_REQ_CAP_NAME \
	"nr_offload_max_req_cap"
#define CFG_OFFLOAD_NEIGHBOR_REPORT_MAX_REQ_CAP_MIN     (3)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_MAX_REQ_CAP_MAX     (300)
#define CFG_OFFLOAD_NEIGHBOR_REPORT_MAX_REQ_CAP_DEFAULT (3)

/*
 * <ini>
 * wmi_wq_watchdog - Sets timeout period for wmi watchdog bite
 * @Min: 0
 * @Max: 30
 * @Default: 20
 *
 * This ini is used to set timeout period for wmi watchdog bite. If it is
 * 0 then wmi watchdog bite is disabled.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_WMI_WQ_WATCHDOG          "wmi_wq_watchdog"
#define CFG_WMI_WQ_WATCHDOG_MIN      (0)
#define CFG_WMI_WQ_WATCHDOG_MAX      (30) /* 30s */
#define CFG_WMI_WQ_WATCHDOG_DEFAULT  (20) /* 20s */

/*
 * <ini>
 * gEnableDTIMSelectionDiversity - Enable/Disable chain
 * selection optimization for one chain dtim
 * @Min: 0
 * @Max: 30
 * @Default: 5
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DTIM_SELECTION_DIVERSITY_NAME    "gEnableDTIMSelectionDiversity"
#define CFG_DTIM_SELECTION_DIVERSITY_MIN     (0)
#define CFG_DTIM_SELECTION_DIVERSITY_MAX     (30)
#define CFG_DTIM_SELECTION_DIVERSITY_DEFAULT (5)

/*
 * <ini>
 * channel_select_logic_conc - Set channel selection logic
 * for different concurrency combinations to DBS or inter band
 * MCC. Default is DBS for STA+STA and STA+P2P.
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * 0 - inter-band MCC
 * 1 - DBS
 *
 * BIT 0: STA+STA
 * BIT 1: STA+P2P
 * BIT 2-31: Reserved
 *
 * Supported Feature: STA+STA, STA+P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CHANNEL_SELECT_LOGIC_CONC_NAME    "channel_select_logic_conc"
#define CFG_CHANNEL_SELECT_LOGIC_CONC_MIN     (0x00000000)
#define CFG_CHANNEL_SELECT_LOGIC_CONC_MAX     (0xFFFFFFFF)
#define CFG_CHANNEL_SELECT_LOGIC_CONC_DEFAULT (0x00000003)

/*
 * <ini>
 * gTxSchDelay - Enable/Disable Tx sch delay
 * @Min: 0
 * @Max: 5
 * @Default: 0
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_TX_SCH_DELAY_NAME          "gTxSchDelay"
#define CFG_TX_SCH_DELAY_MIN           (0)
#define CFG_TX_SCH_DELAY_MAX           (5)
#define CFG_TX_SCH_DELAY_DEFAULT       (0)

/*
 * Start of action oui inis
 *
 * To enable action oui feature, set gEnableActionOUI
 *
 * Each action oui is expected in the following format:
 * <Extension 1> <Extension 2> ..... <Extension 10> (maximum 10)
 *
 * whereas, each Extension is separated by space and have the following format:
 * <Token1> <Token2> <Token3> <Token4> <Token5> <Token6> <Token7> <Token8>
 * where each Token is a string of hexa-decimal digits and
 * following are the details about each token
 *
 * Token1 = OUI
 * Token2 = Data_Length
 * Token3 = Data
 * Token4 = Data_Mask
 * Token5 = Info_Presence_Bit
 * Token6 = MAC_Address
 * Token7 = Mac_Address Mask
 * Token8 = Capability
 *
 * <OUI> is mandatory and it can be either 3 or 5 bytes means 6 or 10
 * hexa-decimal characters
 * If the OUI and Data checks needs to be ignored, the oui FFFFFF
 * needs to be provided as OUI and bit 0 of Info_Presence_Bit should
 * be set to 0.
 *
 * <Data_Length> is mandatory field and should give length of
 * the <Data> if present else zero
 *
 * Presence of <Data> is controlled by <Data_Length>, if <Data_Length> is 0,
 * then <Data> is not expected else Data of the size Data Length bytes are
 * expected which means the length of Data string is 2 * Data Length,
 * since every byte constitutes two hexa-decimal characters.
 *
 * <Data_Mask> is mandatory if <Data> is present and length of the
 * Data mask string depends on the <Data Length>
 * If <Data Length> is 06, then length of Data Mask string is
 * 2 characters (represents 1 byte)
 * data_mask_length = ((Data_Length - (Data_Length % 8)) / 8) +
 *                    ((Data_Length % 8) ? 1 : 0)
 * and <Data_Mask> has to be constructed from left to right.
 *
 * Presence of <Mac_Address> and <Capability> is
 * controlled by <Info_Presence_Bit> which is mandatory
 * <Info_Presence_Bit> will give the information for
 *   OUI – bit 0 Should be set to 1
 *               Setting to 0 will ignore OUI and data check
 *   Mac Address present – bit 1
 *   NSS – bit 2
 *   HT check – bit 3
 *   VHT check – bit 4
 *   Band info – bit 5
 *   reserved – bit 6 (should always be zero)
 *   reserved – bit 7 (should always be zero)
 * and should be constructed from right to left (b7b6b5b4b3b2b1b0)
 *
 * <Mac_Address_Mask> for <Mac_Address> should be constructed from left to right
 *
 * <Capability> is 1 byte long and it contains the below info
 *   NSS – 4 bits starting from LSB (b0 – b3)
 *   HT enabled – bit 4
 *   VHT enabled – bit 5
 *   2G band – bit 6
 *   5G band – bit 7
 * and should be constructed from right to left (b7b6b5b4b3b2b1b0)
 * <Capability> is present if atleast one of the bit is set
 * from b2 - b6 in <Info_Presence_Bit>
 *
 * Example 1:
 *
 * OUI is 00-10-18, data length is 05 (hex form), data is 02-11-04-5C-DE and
 * need to consider first 3 bytes and last byte of data for comparision
 * mac-addr EE-1A-59-FE-FD-AF is present and first 3 bytes and last byte of
 * mac address should be considered for comparision
 * capability is not present
 * then action OUI for gActionOUIITOExtension is as follows:
 *
 * gActionOUIITOExtension=001018 05 0211045CDE E8 03 EE1A59FEFDAF E4
 *
 * data mask calculation in above example:
 * Data[0] = 02 ---- d0 = 1
 * Data[1] = 11 ---- d1 = 1
 * Data[2] = 04 ---- d2 = 1
 * Data[3] = 5C ---- d3 = 0
 * Data[4] = DE ---- d4 = 1
 * data_mask = d0d1d2d3d4 + append with zeros to complete 8-bit = 11101000 = E8
 *
 * mac mask calculation in above example:
 * mac_addr[0] = EE ---- m0 = 1
 * mac_addr[1] = 1A ---- m1 = 1
 * mac_addr[2] = 59 ---- m2 = 1
 * mac_addr[3] = FE ---- m3 = 0
 * mac_addr[4] = FD ---- m4 = 0
 * mac_addr[5] = AF ---- m5 = 1
 * mac_mask = m0m1m2m3m4m5 + append with zeros to complete 8-bit = 11100100 = E4
 *
 * Example 2:
 *
 * OUI is 00-10-18, data length is 00 and no Mac Address and capability
 *
 * gActionOUIITOExtension=001018 00 01
 *
 */

/*
 * <ini>
 * gEnableActionOUI - Enable/Disable action oui feature
 * @Min: 0 (disable)
 * @Max: 1 (enable)
 * @Default: 1 (enable)
 *
 * This ini is used to enable the action oui feature to control
 * mode of connection, connected AP's in-activity time, Tx rate etc.,
 *
 * Related: If gEnableActionOUI is set, then at least one of the following inis
 * must be set with the proper action oui extensions:
 * gActionOUIConnect1x1, gActionOUIITOExtension, gActionOUICCKM1X1
 *
 * Supported Feature: action ouis
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_ACTION_OUI         "gEnableActionOUI"
#define CFG_ENABLE_ACTION_OUI_MIN     (0)
#define CFG_ENABLE_ACTION_OUI_MAX     (1)
#define CFG_ENABLE_ACTION_OUI_DEFAULT (1)

/*
 * <ini>
 * gActionOUIConnect1x1 - Used to specify action OUIs for 1x1 connection
 * @Default: 000C43 00 25 C2 001018 06 02FFF02C0000 BC 25 42 001018 06 02FF040C0000 BC 25 42 00037F 00 35 6C 001018 06 02FF009C0000 BC 25 48
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1 : 000C43
 *   OUI data Len : 00
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: C2 - NSS == 2 && Band == 2G || Band == 5G
 * OUI 2 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF02C0000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: 42 - NSS == 2 && Band == 2G
 * OUI 3 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FF040C0000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: 42 - NSS == 2 && Band == 2G
 * OUI 4 : 00037F
 *   OUI data Len : 00
 *   Info Mask : 35 - Check for NSS, VHT Caps and Band
 *   Capabilities: 6C - (NSS == 3 or 4) && VHT Caps Preset && Band == 2G
 * OUI 5 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FF009C0000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 25 - Check for NSS and Band
 *   Capabilities: 48 - NSS == 4 && Band == 2G
 * This ini is used to specify the AP OUIs with which only 1x1 connection
 * is allowed.
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_CONNECT_1X1_NAME    "gActionOUIConnect1x1"
#define CFG_ACTION_OUI_CONNECT_1X1_DEFAULT "000C43 00 25 C2 001018 06 02FFF02C0000 BC 25 42 001018 06 02FF040C0000 BC 25 42 00037F 00 35 6C 001018 06 02FF009C0000 BC 25 48"

/*
 * <ini>
 * gActionOUIITOExtension - Used to extend in-activity time for specified APs
 * @Default: 00037F 06 01010000FF7F FC 01 000AEB 02 0100 C0 01 000B86 03 010408 E0 01
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1: 00037F
 *   OUI data Len: 06
 *   OUI Data: 01010000FF7F
 *   OUI data Mask: FC - 11111100
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 2: 000AEB
 *   OUI data Len: 02
 *   OUI Data: 0100
 *   OUI data Mask: C0 - 11000000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * OUI 3: 000B86
 *   OUI data Len: 03
 *   OUI Data: 010408
 *   OUI data Mask: E0 - 11100000
 *   Info Mask : 01 - only OUI present in Info mask
 *
 * This ini is used to specify AP OUIs using which station's in-activity time
 * can be extended with the respective APs
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_ITO_EXTENSION_NAME    "gActionOUIITOExtension"
#define CFG_ACTION_OUI_ITO_EXTENSION_DEFAULT "00037F 06 01010000FF7F FC 01 000AEB 02 0100 C0 01 000B86 03 010408 E0 01"

/*
 * <ini>
 * gActionOUICCKM1X1 - Used to specify action OUIs to control station's TX rates
 *
 * This ini is used to specify AP OUIs for which station's CCKM TX rates
 * should be 1x1 only.
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_CCKM_1X1_NAME    "gActionOUICCKM1X1"
#define CFG_ACTION_OUI_CCKM_1X1_DEFAULT ""

/*
 * <ini>
 * gActionOUIITOAlternate - Used to specify action OUIs to have alternate ITO in
 * weak RSSI state
 *
 * This ini is used to specify AP OUIs for which the stations will have
 * alternate ITOs for the case when the RSSI is weak.
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
 #define CFG_ACTION_OUI_ITO_ALTERNATE_NAME    "gActionOUIITOAlternate"
 #define CFG_ACTION_OUI_ITO_ALTERNATE_DEFAULT "001018 06 0202001c0000 FC 01"

/*
 * <ini>
 * gActionOUISwitchTo11nMode - Used to specify action OUIs for switching to 11n
 *
 * This ini is used to specify which AP for which the connection has to be
 * made in 2x2 mode with HT capabilities only and not VHT.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1 : 00904C
 *   OUI data Len : 03
 *   OUI Data : 0418BF
 *   OUI data Mask: E0 - 11100000
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * Related: None
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_SWITCH_TO_11N_MODE_NAME    "gActionOUISwitchTo11nMode"
#define CFG_ACTION_OUI_SWITCH_TO_11N_MODE_DEFAULT "00904C 05 0418BF0CB2 F8 21 40"

/*
 * <ini>
 * gActionOUIConnect1x1with1TxRxChain - Used to specify action OUIs for
 *					 1x1 connection with one Tx/Rx Chain
 * @Default:
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs: (All values in Hex)
 * OUI 1 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF0040000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * OUI 2 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF0050000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * OUI 3 : 001018
 *   OUI data Len : 06
 *   OUI Data : 02FFF4050000
 *   OUI data Mask: BC - 10111100
 *   Info Mask : 21 - Check for Band
 *   Capabilities: 40 - Band == 2G
 *
 * This ini is used to specify the AP OUIs with which only 1x1 connection
 * with one Tx/Rx Chain is allowed.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN_NAME    "gActionOUIConnect1x1with1TxRxChain"
#define CFG_ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN_DEFAULT "001018 06 02FFF0040000 BC 21 40 001018 06 02FFF0050000 BC 21 40 001018 06 02FFF4050000 BC 21 40"

/*
 * <ini>
 * gActionOUIDisableAggressiveTX - Used to specify action OUIs to disable
 * Aggressive TX feature when operating in softap.
 *
 * @Default:
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * Default OUIs:
 *
 * OUI 1 : FFFFFF
 *   OUI data Len : 00
 *   OUI Data: No data
 *   OUI data Mask: No data mask
 *   Info Mask:  2A - Check for mac-addr, HT capability and Band
 *   Mac-addr: F8:59:71:00:00:00 - first 3 bytes
 *   Mac-mask: E0 - Match only first 3 bytes of peer mac-addr
 *   Capabilities: 50 – HT should be enabled, and band should be 2.4GHz
 *
 * OUI 2 : FFFFFF
 *   OUI data Len : 00
 *   OUI Data: No data
 *   OUI data Mask: No data mask
 *   Info Mask:  2A - Check for mac-addr, HT capability and Band
 *   Mac-addr: 14:AB:C5:00:00:00 - first 3 bytes
 *   Mac-mask: E0 - Match only first 3 bytes of peer mac-addr
 *   Capabilities: 50 – HT should be enabled, and band should be 2.4GHz
 *
 * When operating in Softap mode, this ini is used to specify
 * STA (peer) OUIs/mac-addr for which aggressive tx is disabled after
 * association is successful.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_DISABLE_AGGRESSIVE_TX_NAME "gActionOUIDisableAggressiveTX"
#define CFG_ACTION_OUI_DISABLE_AGGRESSIVE_TX_DEFAULT "FFFFFF 00 2A F85971000000 E0 50 FFFFFF 00 2A 14ABC5000000 E0 50"

/*
 * <ini>
 * gActionOUIDisableAggressiveEDCA - Used to specify action OUIs to control
 * EDCA configuration when join the candidate AP
 *
 * @Default: NULL
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 *
 * This ini is used to specify AP OUIs. The station's EDCA should follow the
 * APs' when connecting to those AP, even if the gEnableEdcaParams is set.
 * For example, it follows the AP's EDCA whose OUI is 0050F2 with the
 * following setting:
 *     gActionOUIDisableAggressiveEDCA=0050F2 00 01
 *          Explain: 0050F2: OUI
 *                   00: data length is 0
 *                   01: info mask, only OUI present in Info mask
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableEdcaParams, gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_DISABLE_AGGRESSIVE_EDCA "gActionOUIDisableAggressiveEDCA"
#define CFG_ACTION_OUI_DISABLE_AGGRESSIVE_EDCA_DEFAULT ""

/*
 * <ini>
 * gActionOUIReconnAssocTimeout - Used to specify action OUIs to
 * reconnect to same BSSID when wait for association response timeout
 *
 * This ini is used to specify AP OUIs. Some of AP doesn't response our
 * first association request, but it would response our second association
 * request. Add such OUI configuration INI to apply reconnect logic when
 * association timeout happends with such AP.
 * For default:
 *     gActionOUIReconnAssocTimeout=00E04C 00 01
 *          Explain: 00E04C: OUI
 *                   00: data length is 0
 *                   01: infio mask, only OUI present in Info mask
 * Note: User should strictly add new action OUIs at the end of this
 * default value.
 * Refer to gEnableActionOUI for more detail about the format.
 *
 * Related: gEnableActionOUI
 *
 * Supported Feature: Action OUIs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACTION_OUI_RECONN_ASSOCTIMEOUT \
	"gActionOUIReconnAssocTimeout"
#define CFG_ACTION_OUI_RECONN_ASSOCTIMEOUT_DEFAULT "00E04C 00 01"
/* End of action oui inis */

/*
 * <ini>
 * gEnableUnitTestFramework - Enable/Disable unit test framework
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Usage: Internal (only for dev and test team)
 *
 * </ini>
 */
#define CFG_ENABLE_UNIT_TEST_FRAMEWORK_NAME    "gEnableUnitTestFramework"
#define CFG_ENABLE_UNIT_TEST_FRAMEWORK_MIN     (0)
#define CFG_ENABLE_UNIT_TEST_FRAMEWORK_MAX     (1)
#define CFG_ENABLE_UINT_TEST_FRAMEWORK_DEFAULT (0)

/*
 * <ini>
 * enable_rtt_mac_randomization - Enable/Disable rtt mac randomization
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_RTT_MAC_RANDOMIZATION_NAME    "enable_rtt_mac_randomization"
#define CFG_ENABLE_RTT_MAC_RANDOMIZATION_MIN     (0)
#define CFG_ENABLE_RTT_MAC_RANDOMIZATION_MAX     (1)
#define CFG_ENABLE_RTT_MAC_RANDOMIZATION_DEFAULT (0)

/*
 * <ini>
 * gEnableSecondaryRate - Enable/Disable Secondary Retry Rate feature subset
 *
 * @Min: 0x0
 * @Max: 0x3F
 * @Default: 0x17
 *
 * It is a 32 bit value such that the various bits represent as below -
 * Bit-0 : is Enable/Disable Control for "PPDU Secondary Retry Support"
 * Bit-1 : is Enable/Disable Control for "RTS Black/White-listing Support"
 * Bit-2 : is Enable/Disable Control for "Higher MCS retry restriction
 *         on XRETRY failures"
 * Bit 3-5 : is "Xretry threshold" to use
 * Bit 3~31 : reserved for future use.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SECONDARY_RATE_NAME          "gEnableSecondaryRate"
#define CFG_ENABLE_SECONDARY_RATE_MIN           (0)
#define CFG_ENABLE_SECONDARY_RATE_MAX           (0x3F)
#define CFG_ENABLE_SECONDARY_RATE_DEFAULT       (0x17)

#ifdef MWS_COEX
/*
 * <ini>
 * gMwsCoex4gQuickTdm - Bitmap to control MWS-COEX 4G quick FTDM policy
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * It is a 32 bit value such that the various bits represent as below:
 * Bit-0 : 0 - Don't allow quick FTDM policy (Default)
 *        1 - Allow quick FTDM policy
 * Bit 1-31 : reserved for future use
 *
 * It is used to enable or disable MWS-COEX 4G (LTE) Quick FTDM
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_MWS_COEX_4G_QUICK_FTDM_NAME      "gMwsCoex4gQuickTdm"
#define CFG_MWS_COEX_4G_QUICK_FTDM_MIN       (0x00000000)
#define CFG_MWS_COEX_4G_QUICK_FTDM_MAX       (0xFFFFFFFF)
#define CFG_MWS_COEX_4G_QUICK_FTDM_DEFAULT   (0x00000000)

/*
 * <ini>
 * gMwsCoex5gnrPwrLimit - Bitmap to set MWS-COEX 5G-NR power limit
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * It is a 32 bit value such that the various bits represent as below:
 * Bit-0 : Don't apply user specific power limit,
 *        use internal power limit (Default)
 * Bit 1-2 : Invalid value (Ignored)
 * Bit 3-21 : Apply the specified value as the external power limit, in dBm
 * Bit 22-31 : Invalid value (Ignored)
 *
 * It is used to set MWS-COEX 5G-NR power limit
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_MWS_COEX_5G_NR_PWR_LIMIT_NAME      "gMwsCoex5gnrPwrLimit"
#define CFG_MWS_COEX_5G_NR_PWR_LIMIT_MIN       (0x00000000)
#define CFG_MWS_COEX_5G_NR_PWR_LIMIT_MAX       (0xFFFFFFFF)
#define CFG_MWS_COEX_5G_NR_PWR_LIMIT_DEFAULT   (0x00000000)
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*
 * <ini>
 * roam_preauth_retry_count
 *
 * @Min: 1
 * @Max: 10
 * @Default: 5
 *
 * The maximum number of software retries for preauth or
 * reassoc made before picking up the next candidate for
 * connection during roaming.
 *
 * Related: N/A
 *
 * Supported Features: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ROAM_PREAUTH_RETRY_COUNT_NAME    "roam_preauth_retry_count"
#define CFG_ROAM_PREAUTH_RETRY_COUNT_MIN     (1)
#define CFG_ROAM_PREAUTH_RETRY_COUNT_MAX     (10)
#define CFG_ROAM_PREAUTH_RETRY_COUNT_DEFAULT (5)

/*
 * <ini>
 * roam_preauth_no_ack_timeout
 *
 * @Min: 5
 * @Max: 50
 * @Default: 5
 *
 * Time to wait (in ms) after sending an preauth or reassoc
 * request which didn’t have an ack, before considering
 * it as a failure and making another software retry.
 *
 * Related: N/A
 *
 * Supported Features: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ROAM_PREAUTH_NO_ACK_TIMEOUT_NAME    "roam_preauth_no_ack_timeout"
#define CFG_ROAM_PREAUTH_NO_ACK_TIMEOUT_MIN     (5)
#define CFG_ROAM_PREAUTH_NO_ACK_TIMEOUT_MAX     (50)
#define CFG_ROAM_PREAUTH_NO_ACK_TIMEOUT_DEFAULT (5)
#endif
/*
 * <ini>
 * gSetBTCMode - Config BTC mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * 0 - TDD
 * 1 - FDD
 * 2 - Hybrid
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BTC_MODE_NAME     "gSetBTCMode"
#define CFG_SET_BTC_MODE_MIN      (0)
#define CFG_SET_BTC_MODE_MAX      (2)
#define CFG_SET_BTC_MODE_DEFAULT  (0)

/*
 * <ini>
 * gSetAntennaIsolation - Set Antenna Isolation
 * @Min: 0
 * @Max: 255
 * @Default: 25
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_ANTENNA_ISOLATION_NAME     "gSetAntennaIsolation"
#define CFG_SET_ANTENNA_ISOLATION_MIN      (0)
#define CFG_SET_ANTENNA_ISOLATION_MAX      (255)
#define CFG_SET_ANTENNA_ISOLATION_DEFAULT  (25)

/*
 * <ini>
 * gSetMaxTxPowerForBTC - Set Max WLAN Tx power in COEX scenario
 * @Min: 0
 * @Max: 100
 * @Default: 100
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_MAX_TX_POWER_FOR_BTC_NAME     "gSetMaxTxPowerForBTC"
#define CFG_SET_MAX_TX_POWER_FOR_BTC_MIN      (0)
#define CFG_SET_MAX_TX_POWER_FOR_BTC_MAX      (100)
#define CFG_SET_MAX_TX_POWER_FOR_BTC_DEFAULT  (100)

/*
 * <ini>
 * gSetWlanLowRssiThreshold - Set WLAN low RSSI threshold for BTC mode switching
 * @Min: -100
 * @Max: 0
 * @Default: -80
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_WLAN_LOW_RSSI_THRESHOLD_NAME     "gSetWlanLowRssiThreshold"
#define CFG_SET_WLAN_LOW_RSSI_THRESHOLD_MIN      (-100)
#define CFG_SET_WLAN_LOW_RSSI_THRESHOLD_MAX      (0)
#define CFG_SET_WLAN_LOW_RSSI_THRESHOLD_DEFAULT  (-80)

/*
 * <ini>
 * gSetBtLowRssiThreshold - Set BT low RSSI threshold for BTC mode switching
 * @Min: -100
 * @Max: 0
 * @Default: -80
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BT_LOW_RSSI_THRESHOLD_NAME     "gSetBtLowRssiThreshold"
#define CFG_SET_BT_LOW_RSSI_THRESHOLD_MIN      (-100)
#define CFG_SET_BT_LOW_RSSI_THRESHOLD_MAX      (0)
#define CFG_SET_BT_LOW_RSSI_THRESHOLD_DEFAULT  (-80)

/*
 * <ini>
 * gSetBtInterferenceLowLL - Set lower limit of low level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -25
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BT_INTERFERENCE_LOW_LL_NAME     "gSetBtInterferenceLowLL"
#define CFG_SET_BT_INTERFERENCE_LOW_LL_MIN      (-100)
#define CFG_SET_BT_INTERFERENCE_LOW_LL_MAX      (100)
#define CFG_SET_BT_INTERFERENCE_LOW_LL_DEFAULT  (-25)

/*
 * <ini>
 * gSetBtInterferenceLowUL - Set upper limit of low level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -21
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BT_INTERFERENCE_LOW_UL_NAME     "gSetBtInterferenceLowUL"
#define CFG_SET_BT_INTERFERENCE_LOW_UL_MIN      (-100)
#define CFG_SET_BT_INTERFERENCE_LOW_UL_MAX      (100)
#define CFG_SET_BT_INTERFERENCE_LOW_UL_DEFAULT  (-21)

/*
 * <ini>
 * gSetBtInterferenceMediumLL - Set lower limit of medium level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -20
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BT_INTERFERENCE_MEDIUM_LL_NAME     "gSetBtInterferenceMediumLL"
#define CFG_SET_BT_INTERFERENCE_MEDIUM_LL_MIN      (-100)
#define CFG_SET_BT_INTERFERENCE_MEDIUM_LL_MAX      (100)
#define CFG_SET_BT_INTERFERENCE_MEDIUM_LL_DEFAULT  (-20)

/*
 * <ini>
 * gSetBtInterferenceMediumUL - Set upper limit of medium level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -16
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BT_INTERFERENCE_MEDIUM_UL_NAME     "gSetBtInterferenceMediumUL"
#define CFG_SET_BT_INTERFERENCE_MEDIUM_UL_MIN      (-100)
#define CFG_SET_BT_INTERFERENCE_MEDIUM_UL_MAX      (100)
#define CFG_SET_BT_INTERFERENCE_MEDIUM_UL_DEFAULT  (-16)

/*
 * <ini>
 * gSetBtInterferenceHighLL - Set lower limit of high level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -15
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BT_INTERFERENCE_HIGH_LL_NAME     "gSetBtInterferenceHighLL"
#define CFG_SET_BT_INTERFERENCE_HIGH_LL_MIN      (-100)
#define CFG_SET_BT_INTERFERENCE_HIGH_LL_MAX      (100)
#define CFG_SET_BT_INTERFERENCE_HIGH_LL_DEFAULT  (-15)

/*
 * <ini>
 * gSetBtInterferenceHighUL - Set upper limit of high level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -11
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_BT_INTERFERENCE_HIGH_UL_NAME     "gSetBtInterferenceHighUL"
#define CFG_SET_BT_INTERFERENCE_HIGH_UL_MIN      (-100)
#define CFG_SET_BT_INTERFERENCE_HIGH_UL_MAX      (100)
#define CFG_SET_BT_INTERFERENCE_HIGH_UL_DEFAULT  (-11)

#ifdef FEATURE_MPTA_HELPER
/*
 * <ini>
 * gMPTAHelperEnable - Config MPTA helper enable
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_MPTA_HELPER_ENABLE_NAME     "gMPTAHelperEnable"
#define CFG_SET_MPTA_HELPER_ENABLE_MIN      (0)
#define CFG_SET_MPTA_HELPER_ENABLE_MAX      (1)
#define CFG_SET_MPTA_HELPER_ENABLE_DEFAULT  (0)
#endif

/*
 * <ini>
 * enable_mac_provision - Enable/disable MAC address provisioning feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable MAC address provisioning feature
 *
 * Supported Feature: STA/SAP/P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_MAC_PROVISION_NAME    "enable_mac_provision"
#define CFG_ENABLE_MAC_PROVISION_MIN     (0)
#define CFG_ENABLE_MAC_PROVISION_MAX     (1)
#define CFG_ENABLE_MAC_PROVISION_DEFAULT (0)

/*
 * <ini>
 * provisioned_intf_pool - It is bit mask value of Interfaces
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 0xffffffff
 *
 * This ini will contain the bitmask of all the interfaces
 * which can use addresses from provisioned list. Using enum QDF_OPMODE
 * for deciding the bit positions corresponding to each interface.
 * Bit 0 : QDF_STA_MODE
 * Bit 1 : QDF_SAP_MODE
 * Bit 2 : QDF_P2P_CLIENT_MODE
 * Bit 3 : QDF_P2P_GO_MODE
 * Bit 4 : QDF_FTM_MODE
 * Bit 5 : QDF_IBSS_MODE
 * Bit 6 : QDF_MONITOR_MODE
 * Bit 7 : QDF_P2P_DEVICE_MODE
 * Bit 8 : QDF_OCB_MODE
 * Bit 9 : QDF_EPPING_MODE
 * Bit 10 : QDF_QVIT_MODE
 * Bit 11 : QDF_NDI_MODE
 * Bit 12 : QDF_MAX_NO_OF_MODE
 * For example :
 * If Bit 0 represents STA
 * Bit 1 represents SAP
 * Bit 2 represents P2PGO
 * If only STA and SAP can use addresses from provisioned list then the value
 * of ini should be 3 (00000011) as first and second bit should be set.
 * If only STA and P2PGO can use addresses from provisioned list then the value
 * of ini should be 5 (00000101) as first and third bit should be set.
 * Similarly, for only SAP and P2PGO ini should be 6 (00000110)
 *
 * Supported Feature: STA/SAP/P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PROVISION_INTERFACE_POOL_NAME    "provisioned_intf_pool"
#define CFG_PROVISION_INTERFACE_POOL_MIN     (0)
#define CFG_PROVISION_INTERFACE_POOL_MAX     (0xffffffff)
#define CFG_PROVISION_INTERFACE_POOL_DEFAULT (0xffffffff)
/*
 * <ini>
 * deriveded_intf_pool - It is bit mask value of Interfaces
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 0xffffffff
 *
 * This ini will contain the bitmask of all the interfaces
 * which can use addresses from derived list
 *
 *
 * Supported Feature: STA/SAP/P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DERIVED_INTERFACE_POOL_NAME    "derived_intf_pool"
#define CFG_DERIVED_INTERFACE_POOL_MIN     (0)
#define CFG_DERIVED_INTERFACE_POOL_MAX     (0xffffffff)
#define CFG_DERIVED_INTERFACE_POOL_DEFAULT (0xffffffff)

/*
 * <ini>
 * gEnablePeerUnmapConfSupport - Set PEER UNMAP confirmation support
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable or disable peer unmap confirmation support
 * in Host. Host sends this support to FW only if FW support is enabled.
 *
 *
 * Supported Feature: STA/SAP/P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_PEER_UNMAP_CONF_NAME    "gEnablePeerUnmapConfSupport"
#define CFG_ENABLE_PEER_UNMAP_CONF_MIN     (0)
#define CFG_ENABLE_PEER_UNMAP_CONF_MAX     (1)
#define CFG_ENABLE_PEER_UNMAP_CONF_DEFAULT (0)

/*
 * <ini>
 * roam_score_delta - Percentage increment in roam score value
 * that is expected from a roaming candidate AP.
 * @Min: 0
 * @Max: 100
 * @Default: 0
 *
 * This ini is used to provide the percentage increment value over roam
 * score for the candidate APs so that they can be preferred over current
 * AP for roaming.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCORE_DELTA           "roam_score_delta"

#define CFG_ROAM_SCORE_DELTA_DEFAULT    0
#define CFG_ROAM_SCORE_DELTA_MIN        0
#define CFG_ROAM_SCORE_DELTA_MAX        100

/*
 * <ini>
 * min_roam_score_delta - Difference of roam score values between connected
 * AP and roam candidate AP.
 * @Min: 0
 * @Max: 10000
 * @Default: 0
 *
 * This ini is used during CU and low rssi based roam triggers, consider
 * AP as roam candidate only if its roam score is better than connected
 * AP score by at least min_roam_score_delta.
 * If user configured "roam_score_delta" and "min_roam_score_delta" both,
 * then firmware selects roam candidate AP by considering values of both
 * INIs.
 * Example: If DUT is connected with AP1 and roam candidate AP2 has roam
 * score greater than roam_score_delta and min_roam_score_delta then only
 * firmware will trigger roaming to AP2.
 *
 * Related: roam_score_delta
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CAND_MIN_ROAM_SCORE_DELTA "min_roam_score_delta"
#define CFG_CAND_MIN_ROAM_SCORE_DELTA_DEFAULT 0
#define CFG_CAND_MIN_ROAM_SCORE_DELTA_MAX 10000
#define CFG_CAND_MIN_ROAM_SCORE_DELTA_MIN 0

/*
 * <ini>
 * roam_score_delta_bitmap - bitmap to enable roam triggers on
 * which roam score delta is to be applied during roam candidate
 * selection
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 0xffffffff
 *
 * Bitmap value of the following roam triggers:
 * ROAM_TRIGGER_REASON_NONE       - B0,
 * ROAM_TRIGGER_REASON_PER        - B1,
 * ROAM_TRIGGER_REASON_BMISS      - B2,
 * ROAM_TRIGGER_REASON_LOW_RSSI   - B3,
 * ROAM_TRIGGER_REASON_HIGH_RSSI  - B4,
 * ROAM_TRIGGER_REASON_PERIODIC   - B5,
 * ROAM_TRIGGER_REASON_MAWC       - B6,
 * ROAM_TRIGGER_REASON_DENSE      - B7,
 * ROAM_TRIGGER_REASON_BACKGROUND - B8,
 * ROAM_TRIGGER_REASON_FORCED     - B9,
 * ROAM_TRIGGER_REASON_BTM        - B10,
 * ROAM_TRIGGER_REASON_UNIT_TEST  - B11,
 * ROAM_TRIGGER_REASON_BSS_LOAD   - B12
 *
 * When the bit corresponding to a particular roam trigger reason
 * is set, the value of "roam_score_delta" is expected over the
 * roam score of the current connected AP, for that triggered roam
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_TRIGGER_DELTA_BITMAP           "roam_score_delta_bitmap"

#define CFG_ROAM_TRIGGER_DELTA_BITMAP_DEFAULT    0xffffffff
#define CFG_ROAM_TRIGGER_DELTA_BITMAP_MIN        0
#define CFG_ROAM_TRIGGER_DELTA_BITMAP_MAX        0xffffffff

/*
 * <ini>
 * prefer_btm_query - Prefer btm query over 11k neighbor report
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable the STA to send BTM query instead of
 * 11k neighbor report.
 * Enabling this flag also will Set the bit 8 of btm_offload_config
 * which will be sent to firmware
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 </ini>
 */
#define CFG_PREFER_BTM_QUERY		"prefer_btm_query"

#define CFG_PREFER_BTM_QUERY_DEFAULT    1
#define CFG_PREFER_BTM_QUERY_MIN        0
#define CFG_PREFER_BTM_QUERY_MAX        1

#define BTM_OFFLOAD_CONFIG_BIT_8    8
#define BTM_OFFLOAD_CONFIG_BIT_7    7

/*
 * <ini>
 * prefer_roam_score_for_candidate_selection - choose to sort the candidates on
 * roam score or prefered AP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable the the firmware to sort the candidates
 * based on the roam score rather than selecting preferred APs.
 * Enabling this flag also will Set the bit 7 of btm_offload_config
 * which will be sent to firmware
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_BTM_ABRIDGE  "prefer_roam_score_for_candidate_selection"

#define CFG_ENABLE_BTM_ABRIDGE_DEFAULT    1
#define CFG_ENABLE_BTM_ABRIDGE_MIN        0
#define CFG_ENABLE_BTM_ABRIDGE_MAX        1

/*
 * <ini>
 * roam_candidate_validity_timer - roam cache entries validity timer
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 0xffffffff
 *
 * This value is the timeout values for the cached roam candidate
 * entries in firmware. If this value is 0, then that entry is not
 * valid
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_VALIDITY_TIMER    "roam_candidate_validity_timer"

#define CFG_BTM_VALIDITY_TIMER_DEFAULT    0xffffffff
#define CFG_BTM_VALIDITY_TIMER_MIN        0
#define CFG_BTM_VALIDITY_TIMER_MAX        0xffffffff

 /*
 * <ini>
 * enable_beacon_reception_stats - Enable disable beacon reception stats
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the beacon reception stats collected per
 * vdev and then sent to the driver to be displayed in sysfs
 *
 * Related: None
 *
 * Supported Feature: Stats
 *
 * </ini>
 */
#define CFG_ENABLE_BEACON_RECEPTION_STATS_NAME   "enable_beacon_reception_stats"
#define CFG_ENABLE_BEACON_RECEPTION_STATS_MIN     0
#define CFG_ENABLE_BEACON_RECEPTION_STATS_MAX     1
#define CFG_ENABLE_BEACON_RECEPTION_STATS_DEFAULT 0

/*
 * <ini>
 * mgmt_retry_max - Maximum Retries for mgmt frames
 * @Min: 0
 * @Max: 31
 * @Default: 15
 *
 * This ini is used to set maximum retries for mgmt frames
 *
 * Supported Feature: STA/SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MGMT_RETRY_MAX         "mgmt_retry_max"
#define CFG_MGMT_RETRY_MAX_MIN     0
#define CFG_MGMT_RETRY_MAX_MAX     31
#define CFG_MGMT_RETRY_MAX_DEFAULT 15

/*
 * <ini>
 * btm_disassoc_timer_threshold - Disassociation timer threshold to wait
 * after which the full scan for roaming can be started after the AP has sent
 * the disassoc imminent
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 0
 *
 * When AP sends, BTM request with disassoc imminent bit set, the STA should
 * roam to a new AP within the disassc timeout provided by the ap. If the Roam
 * scan period is less than the disassoc timeout value, then instead of
 * triggering the roam scan immediately, STA can wait for this
 * btm_disassoc_timer_threshold and then start roaming.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_DISASSOC_TIMER_THRESHOLD "btm_disassoc_timer_threshold"

#define CFG_BTM_DISASSOC_TIMER_THRESHOLD_DEFAULT    0
#define CFG_BTM_DISASSOC_TIMER_THRESHOLD_MIN        0
#define CFG_BTM_DISASSOC_TIMER_THRESHOLD_MAX        0xffffffff

/*
 * <ini>
 * enable_bss_load_roam_trigger - enable/disable bss load based roam trigger
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini when enabled, allows the firmware to roam when bss load outpaces
 * the configured bss load threshold. When this ini is disabled, firmware
 * doesn't consider bss load values to trigger roam.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM  "enable_bss_load_roam_trigger"

#define CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM_DEFAULT    0
#define CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM_MIN        0
#define CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM_MAX        1

/*
 * <ini>
 * bss_load_threshold - bss load above which the STA should trigger roaming
 * @Min: 0
 * @Max: 100
 * @Default: 70
 *
 * When the bss laod value that is sampled exceeds this threshold, firmware
 * will trigger roaming if bss load trigger is enabled.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BSS_LOAD_THRESHOLD "bss_load_threshold"

#define CFG_BSS_LOAD_THRESHOLD_DEFAULT    70
#define CFG_BSS_LOAD_THRESHOLD_MIN        0
#define CFG_BSS_LOAD_THRESHOLD_MAX        100

 /*
 * <ini>
 * bss_load_sample_time - Time in milliseconds for which the bss load values
 * obtained from the beacons is sampled.
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 10000
 *
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define  CFG_BSS_LOAD_SAMPLE_TIME    "bss_load_sample_time"

#define CFG_BSS_LOAD_SAMPLE_TIME_DEFAULT    10000
#define CFG_BSS_LOAD_SAMPLE_TIME_MIN        0
#define CFG_BSS_LOAD_SAMPLE_TIME_MAX        0xffffffff

/*
 * gEnableRTTsupport
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 1 - Enabled
 *
 * The param is used to enable/disable support for RTT
 */
#define CFG_ENABLE_RTT_SUPPORT            "gEnableRTTSupport"
#define CFG_ENABLE_RTT_SUPPORT_DEFAULT    (1)
#define CFG_ENABLE_RTT_SUPPORT_MIN        (0)
#define CFG_ENABLE_RTT_SUPPORT_MAX        (1)

/*
 * <ini>
 * ignore_fw_reg_offload_ind - If set, Ignore the FW offload indication
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to ignore regdb offload indication from FW and
 * regulatory will be treated as non offload.
 * There is a case where FW is sending the offload indication in
 * service ready event but not sending the cc list event
 * WMI_REG_CHAN_LIST_CC_EVENTID and because of this driver is not
 * able to populate the channel list. To address this issue, this ini
 * is added. If this ini is enabled, regulatory will always be treated as
 * non offload solution.
 *
 * This ini should only be enabled to circumvent the above mentioned firmware
 * bug.
 *
 * Related: None
 *
 * Supported Feature: STA/AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IGNORE_FW_REG_OFFLOAD_IND            "ignore_fw_reg_offload_ind"
#define CFG_IGNORE_FW_REG_OFFLOAD_IND_DEFAULT    (0)
#define CFG_IGNORE_FW_REG_OFFLOAD_IND_MIN        (0)
#define CFG_IGNORE_FW_REG_OFFLOAD_IND_MAX        (1)

/*
 * <ini>
 * ndp_inactivity_timeout - To configure duration of how many seconds
 * without TX/RX data traffic, NDI vdev can kickout the connected
 * peer(i.e. NDP Termination).
 *
 * @Min: 0
 * @Max: 180
 * @Default: 60
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_NDP_INACTIVITY_TIMEOUT         "ndp_inactivity_timeout"
#define CFG_NAN_NDP_INACTIVITY_TIMEOUT_DEFAULT (60)
#define CFG_NAN_NDP_INACTIVITY_TIMEOUT_MAX     (1800)
#define CFG_NAN_NDP_INACTIVITY_TIMEOUT_MIN     (0)

/*
 * <ini>
 * enable_disconnect_roam_offload - Enable/Disable emergency roaming during
 * deauth/disassoc
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 1
 *
 * When this ini is enabled firmware will trigger roam scan and roam to a new ap
 * if candidate is found and it will not send the deauth/disassoc frame to
 * the host driver.
 * If roaming fails after this deauth, then firmware will send
 * WMI_ROAM_REASON_DEAUTH event to the host. If roaming is successful, driver
 * follows the normal roam synch event path.
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_ENABLE_DISCONNECT_ROAM      "enable_disconnect_roam_offload"
#define CFG_LFR_ENABLE_DISCONNECT_ROAM_MIN     (false)
#define CFG_LFR_ENABLE_DISCONNECT_ROAM_MAX     (true)
#define CFG_LFR_ENABLE_DISCONNECT_ROAM_DEFAULT (true)

/*
 * <ini>
 * enable_idle_roam - Enable/Disable idle roaming
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 0
 *
 * When this ini is enabled firmware will trigger roam scan and roam to a new
 * ap if current connected AP rssi falls below the threshold. To consider the
 * connection as idle, the following conditions should be met if this ini
 * "enable_idle_roam" is enabled:
 * 1. User space sends "SET SUSPENDMODE" command with value 0.
 * 2. No TX/RX data for idle time configured via ini "idle_roam_inactive_time".
 * 3. Connected AP rssi change doesn't exceed a specific delta value.
 * (configured via ini idle_roam_rssi_delta)
 * 4. Connected AP rssi falls below minimum rssi (configured via ini
 * "idle_roam_min_rssi").
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_ENABLE_IDLE_ROAM    "enable_idle_roam"
#define CFG_LFR_ENABLE_IDLE_ROAM_MIN     (false)
#define CFG_LFR_ENABLE_IDLE_ROAM_MAX     (true)
#define CFG_LFR_ENABLE_IDLE_ROAM_DEFAULT (false)

/*
 * <ini>
 * idle_roam_rssi_delta - This threshold is the criteria to decide whether DUT
 * is idle or moving. If rssi delta is more than configured thresold then its
 * considered as not idle. RSSI delta is entered in dBm. Idle roaming can be
 * triggered if the connected AP rssi change exceeds or falls below the
 * rssi delta and if other criteria of ini "enable_idle_roam" is met
 * @Min: 0
 * @Max: 50
 * @Default: 3
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_RSSI_DELTA   "idle_roam_rssi_delta"
#define CFG_LFR_IDLE_ROAM_RSSI_DELTA_MIN      (0)
#define CFG_LFR_IDLE_ROAM_RSSI_DELTA_MAX      (50)
#define CFG_LFR_IDLE_ROAM_RSSI_DELTA_DEFAULT  (3)

/*
 * <ini>
 * idle_roam_inactive_time - Time duration in millseconds for which the
 * connection is idle.
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 10000
 *
 * This ini is used to configure the time in seconds for which the connection
 * candidate is idle and after which idle roam scan can be triggered if
 * other criteria of ini "enable_idle_roam" is met.
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_INACTIVE_TIME   "idle_roam_inactive_time"
#define CFG_LFR_IDLE_ROAM_INACTIVE_TIME_MIN        (0)
#define CFG_LFR_IDLE_ROAM_INACTIVE_TIME_MAX      (0xFFFFFFFF)
#define CFG_LFR_IDLE_ROAM_INACTIVE_TIME_DEFAULT  (10000)

/*
 * <ini>
 * idle_data_packet_count - No of tx/rx packets above which the connection is
 * not idle.
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 10
 *
 * This ini is used to configure the acceptable number of tx/rx packets below
 * which the connection is idle. Ex: If idle_data_packet_count is 10
 * and if the tx/rx packet count is less than 10, the connection is
 * idle. If there are more than 10 packets, the connection is active one.
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_PACKET_COUNT       "idle_data_packet_count"
#define CFG_LFR_IDLE_ROAM_PACKET_COUNT_MIN            (0)
#define CFG_LFR_IDLE_ROAM_PACKET_COUNT_MAX        (0xFFFFFFFF)
#define CFG_LFR_IDLE_ROAM_PACKET_COUNT_DEFAULT        (10)

/*
 * <ini>
 * idle_roam_min_rssi - Minimum RSSI of connected AP, below which
 * idle roam scan can be triggered if other criteria of ini "enable_idle_roam"
 * is met.
 * @Min: -96
 * @Max: 0
 * @Default: -65
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_MIN_RSSI    "idle_roam_min_rssi"
#define CFG_LFR_IDLE_ROAM_MIN_RSSI_MIN      (-96)
#define CFG_LFR_IDLE_ROAM_MIN_RSSI_MAX       (0)
#define CFG_LFR_IDLE_ROAM_MIN_RSSI_DEFAULT  (-65)

/*
 * <ini>
 * idle_roam_band - Band on which idle roam scan will be
 * enabled
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * Value 0 - Allow idle roam on both bands
 * Value 1 - Allow idle roam only on 2G band
 * Value 2 - Allow idle roam only on 5G band
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_BAND      "idle_roam_band"
#define CFG_LFR_IDLE_ROAM_BAND_MIN      (0)
#define CFG_LFR_IDLE_ROAM_BAND_MAX      (2)
#define CFG_LFR_IDLE_ROAM_BAND_DEFAULT  (0)

#ifdef WLAN_ADAPTIVE_11R
/*
 * <ini>
 * adaptive_11r - Enable/disable adaptive 11r feature.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Adaptive 11r feature enables the AP to support FT-AKM without
 * configuring the FT-AKM in the network. The AP will advertise non-FT akm
 * with a vendor specific IE having Adaptive 11r bit set to 1 in the IE data.
 * The AP also advertises the MDE in beacon/probe response.
 *
 * STA should check the adaptive 11r capability if the AP advertises MDE in
 * beacon/probe and adaptive 11r capability in vendor specific IE.  If adaptive
 * 11r capability is found, STA can advertise the FT equivalent of the non-FT
 * AKM and connect with 11r protocol.
 *
 * Related: None.
 *
 * Supported Feature: Fast BSS Transition
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_11R        "enable_adaptive_11r"
#define CFG_ADAPTIVE_11R_MIN      (false)
#define CFG_ADAPTIVE_11R_MAX      (true)
#define CFG_ADAPTIVE_11R_DEFAULT  (false)
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/*
 * <ini>
 * sae_single_pmk_feature_enabled - Enable/disable sae single pmk feature.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This INI is to enable/disable SAE Roaming with same PMK/PMKID feature support
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_SAE_SINGLE_PMK          "sae_single_pmk_feature_enabled"
#define CFG_SAE_SINGLE_PMK_MIN      (false)
#define CFG_SAE_SINGLE_PMK_MAX      (true)
#define CFG_SAE_SINGLE_PMK_DEFAULT  (false)
#endif

/*
 * <ini>
 * bss_load_trigger_5g_rssi_threshold - Current AP minimum RSSI in dBm below
 * which roaming can be triggered if BSS load exceeds bss_load_threshold.
 * @Min: -120
 * @Max: 0
 * @Default: -70
 *
 * If connected AP is in 5Ghz, then consider bss load roam triggered only if
 * load % > bss_load_threshold && connected AP rssi is worse than
 * bss_load_trigger_5g_rssi_threshold
 *
 * Related: "bss_load_threshold"
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BSS_LOAD_TRIG_5G_RSSI_THRES     "bss_load_trigger_5g_rssi_threshold"
#define CFG_BSS_LOAD_TRIG_5G_RSSI_THRES_MIN            -120
#define CFG_BSS_LOAD_TRIG_5G_RSSI_THRES_MAX              0
#define CFG_BSS_LOAD_TRIG_5G_RSSI_THRES_DEFAULT        -70

/*
 * <ini>
 * bss_load_trigger_2g_rssi_threshold - Current AP minimum RSSI in dBm below
 * which roaming can be triggered if BSS load exceeds bss_load_threshold.
 * @Min: -120
 * @Max: 0
 * @Default: -60
 *
 * If connected AP is in 2Ghz, then consider bss load roam triggered only if
 * load % > bss_load_threshold && connected AP rssi is worse than
 * bss_load_trigger_2g_rssi_threshold.
 *
 * Related: "bss_load_threshold"
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BSS_LOAD_TRIG_2G_RSSI_THRES     "bss_load_trigger_2g_rssi_threshold"
#define CFG_BSS_LOAD_TRIG_2G_RSSI_THRES_MIN             -120
#define CFG_BSS_LOAD_TRIG_2G_RSSI_THRES_MAX               0
#define CFG_BSS_LOAD_TRIG_2G_RSSI_THRES_DEFAULT         -60

/*
 * <ini>
 * roam_scan_inactivity_time - Device inactivity monitoring time in
 * milliseconds for which the device is considered to be inactive with data
 * packets count is less than configured roam_inactive_data_count.
 *
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0
 *
 * The below three ini values are used to control the roam scan after the
 * firmware gets empty roam scan results during periodic roam scans.
 * 1. roam_scan_inactivity_time
 * 2. roam_inactive_data_count
 * 3. roam_scan_period_after_inactivity
 * The first two ini "roam_scan_inactivity_time" and "roam_inactive_data_count"
 * is frames the criteria to detect if the DUT is inactive. If the device is
 * identified to be inactive based on the above two ini, then the value,
 * "roam_scan_period_after_inactivity" will be used as periodic roam scan
 * duration.
 *
 * Related: roam_inactive_data_count
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_INACTIVITY_TIME      "roam_scan_inactivity_time"
#define CFG_ROAM_SCAN_INACTIVITY_TIME_MIN         0
#define CFG_ROAM_SCAN_INACTIVITY_TIME_MAX      0xFFFFFFFF
#define CFG_ROAM_SCAN_INACTIVITY_TIME_DEFAULT     0

/*
 * <ini>
 * roam_inactive_data_count - Maximum allowed data packets count during
 * roam_scan_inactivity_time.
 *
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 10
 *
 * The DUT is said to be inactive only if the data packets count
 * during this  roam_scan_inactivity_time is less than the configured
 * roam_inactive_data_count.
 *
 * Related: roam_scan_inactivity_time
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_INACTIVE_COUNT         "roam_inactive_data_count"
#define CFG_ROAM_INACTIVE_COUNT_MIN             0
#define CFG_ROAM_INACTIVE_COUNT_MAX         0xFFFFFFFF
#define CFG_ROAM_INACTIVE_COUNT_DEFAULT         10

/*
 * <ini>
 * roam_scan_period_after_inactivity - Roam scan duration in ms after device is
 * out of inactivity state.
 *
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 120000
 *
 * If there is empty scan results during roam scan, firmware will move to
 * roam scan inactive state if roam_scan_inactivity and
 * roam_inactive_data_count criteria are met.
 * This ini is used to configure the roam scan duration in ms once the
 * inactivity is finished and roam scan can be started.
 *
 * Related: roam_scan_inactivity_time, roam_inactive_data_count
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_POST_INACTIVITY_ROAM_SCAN_PERIOD "roam_scan_period_after_inactivity"
#define CFG_POST_INACTIVITY_ROAM_SCAN_PERIOD_MIN               0
#define CFG_POST_INACTIVITY_ROAM_SCAN_PERIOD_MAX           0xFFFFFFFF
#define CFG_POST_INACTIVITY_ROAM_SCAN_PERIOD_DEFAULT        120000

/*
 * <ini>
 * candidate_min_rssi_for_disconnect - Candidate AP minimum RSSI in
 * idle roam trigger(in dBm).
 * @Min: -120
 * @Max: 0
 * @Default: -70
 *
 * Minimum RSSI value of the candidate AP to consider it as candidate for
 * roaming when roam trigger is Deauthentication/Disconnection from current
 * AP. This value will be sent to firmware over the WMI_ROAM_AP_PROFILE
 * wmi command in the roam_min_rssi_param_list tlv.
 *
 * Related: enable_idle_roam.
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DISCONNECT_ROAM_TRIGGER_MIN_RSSI "candidate_min_rssi_for_disconnect"
#define CFG_DISCONNECT_ROAM_TRIGGER_MIN_RSSI_MIN        -120
#define CFG_DISCONNECT_ROAM_TRIGGER_MIN_RSSI_MAX         0
#define CFG_DISCONNECT_ROAM_TRIGGER_MIN_RSSI_DEFAULT    -70

/*
 * <ini>
 * candidate_min_rssi_for_beacon_miss - Candidate AP minimum RSSI for beacon
 * miss roam trigger (in dBm)
 * @Min: -120
 * @Max: 0
 * @Default: -70
 *
 * Minimum RSSI value of the candidate AP to consider it as candidate for
 * roaming when roam trigger is disconnection from current AP due to beacon
 * miss. This value will be sent to firmware over the WMI_ROAM_AP_PROFILE
 * wmi command in the roam_min_rssi_param_list tlv.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BMISS_ROAM_MIN_RSSI    "candidate_min_rssi_for_beacon_miss"
#define CFG_BMISS_ROAM_MIN_RSSI_MIN             -120
#define CFG_BMISS_ROAM_MIN_RSSI_MAX               0
#define CFG_BMISS_ROAM_MIN_RSSI_DEFAULT          -70

/*
 * <ini>
 * idle_roam_score_delta - Roam score delta value in percentage for idle roam.
 * @Min: 0
 * @Max: 100
 * @Default: 10
 *
 * This ini is used to configure the minimum change in roam score
 * value of the AP to consider it as candidate for
 * roaming when roam trigger is due to idle state of sta.
 * This value will be sent to firmware over the WMI_ROAM_AP_PROFILE wmi
 * command in the roam_score_delta_param_list tlv.
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IDLE_ROAM_SCORE_DELTA       "idle_roam_score_delta"
#define CFG_IDLE_ROAM_SCORE_DELTA_MIN         0
#define CFG_IDLE_ROAM_SCORE_DELTA_MAX         100
#define CFG_IDLE_ROAM_SCORE_DELTA_DEFAULT     10

/*
 * <ini>
 * btm_roam_score_delta - Roam score delta value in percentage for BTM triggered
 * roaming.
 * @Min: 0
 * @Max: 100
 * @Default: 10
 *
 * This ini is used to configure the minimum change in roam score
 * value of the AP to consider it as candidate when the sta is disconnected
 * from the current AP due to BTM kickout.
 * This value will be sent to firmware over the WMI_ROAM_AP_PROFILE wmi
 * command in the roam_score_delta_param_list tlv.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_ROAM_SCORE_DELTA        "btm_roam_score_delta"
#define CFG_BTM_ROAM_SCORE_DELTA_MIN           0
#define CFG_BTM_ROAM_SCORE_DELTA_MAX           100
#define CFG_BTM_ROAM_SCORE_DELTA_DEFAULT       10

/*
 * <ini>
 * minimum_btm_candidate_score - Consider the AP as roam candidate only if
 * its score is greater than minimum_btm_candidate_score.
 * @Min: 0
 * @Max: 10000
 * @Default: 1800
 *
 * This ini is applicable only for candidate selection during BTM roam trigger.
 * For this roam_score_delta_bitmap bit 10 should be set to 1.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MIN_BTM_CANDIDATE_SCORE       "minimum_btm_candidate_score"
#define CFG_MIN_BTM_CANDIDATE_SCORE_MIN           0
#define CFG_MIN_BTM_CANDIDATE_SCORE_MAX           10000
#define CFG_MIN_BTM_CANDIDATE_SCORE_DEFAULT       1800

/*
 * <ini>
 * enable_pending_list_req - Sets Pending channel List Req.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This option enables/disables SCAN_CHAN_LIST_CMDID channel list command to FW
 * till the current scan is complete.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_PENDING_CHAN_LIST_REQ       "enable_pending_list_req"
#define CFG_ENABLE_PENDING_CHAN_LIST_REQ_MIN           0
#define CFG_ENABLE_PENDING_CHAN_LIST_REQ_MAX           1
#define CFG_ENABLE_PENDING_CHAN_LIST_REQ_DEFAULT       0

/*
 * <ini>
 * gEnableRingBuffer - Enable Ring Buffer for Bug Report
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable Ring Buffer
 *
 * Related: None
 *
 * Supported Feature: STA/SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_RING_BUFFER	   "gEnableRingBuffer"
#define CFG_ENABLE_RING_BUFFER_MIN 0
#define CFG_ENABLE_RING_BUFFER_MAX 1
#define CFG_ENABLE_RING_BUFFER_DEFAULT 1


/*
 * <ini>
 * gNumVdevs - max number of VDEVs supported
 * @Min: 0x1
 * @Max: 0x4
 * @Default: CFG_TGT_NUM_VDEV
 *
 * Usage: External
 * </ini>
 */
#define CFG_NUM_VDEV_ENABLE_NAME      "gNumVdevs"
#define CFG_NUM_VDEV_ENABLE_MIN       (0x1)
#define CFG_NUM_VDEV_ENABLE_MAX       (0x4)
#define CFG_NUM_VDEV_ENABLE_DEFAULT   (CFG_TGT_NUM_VDEV)

#ifdef SAR_SAFETY_FEATURE
/*
 * <ini>
 * gSarSafetyTimeout - Specify SAR safety timeout value in milliseconds
 *
 * @Min: 120000
 * @Max: 600000
 * Default: 300000
 *
 * This ini is used to define SAR safety timeout value in milliseconds.
 * This timer is started when the QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS
 * is received first time.
 * SAR safety timer will wait for the gSarSafetyTimeout for
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and if
 * SAR safety timer timeouts host will configure the gSarSafetyIndex
 * to the FW.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAR_SAFETY_TIMEOUT          "gSarSafetyTimeout"
#define CFG_SAR_SAFETY_TIMEOUT_MIN      (120000)
#define CFG_SAR_SAFETY_TIMEOUT_MAX      (600000)
#define CFG_SAR_SAFETY_TIMEOUT_DEFAULT  (300000)
/*
 * <ini>
 * gSarSafetyUnsolicitedTimeout - Specify SAR safety unsolicited timeout value
 * in milliseconds
 *
 * @Min: 5000
 * @Max: 30000
 * Default: 15000
 *
 * This ini is used to define SAR safety unsolicited timeout value in
 * milliseconds. This timer is started on first data tx.
 * SAR unsolicited timer will wait for the
 * gSarSafetyUnsolicitedTimeout for QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS
 * vendor command and if SAR unsolicited timer timeouts host will indicate
 * user space with QCA_NL80211_VENDOR_SUBCMD_REQUEST_SAR_LIMITS_EVENT to send
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAR_SAFETY_UNSOLICITED_TIMEOUT        "gSarSafetyUnsolicitedTimeout"
#define CFG_SAR_SAFETY_UNSOLICITED_TIMEOUT_MIN     (5000)
#define CFG_SAR_SAFETY_UNSOLICITED_TIMEOUT_MAX     (30000)
#define CFG_SAR_SAFETY_UNSOLICITED_TIMEOUT_DEFAULT (15000)

/*
 * <ini>
 * gSarSafetyReqRespTimeout - Specify SAR safety request response timeout value
 * in milliseconds
 *
 * @Min: 500
 * @Max: 3000
 * Default: 1000
 *
 * This ini is used to define SAR request-response timeout value
 * in milliseconds. SAR request-response timer will wait for the
 * gSarSafetyReqRespTimeout for QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS
 * vendor command and if SAR request-response timer timeouts host will
 * indicate user space with QCA_NL80211_VENDOR_SUBCMD_REQUEST_SAR_LIMITS_EVENT
 * for gSarSafetyReqRespRetry number of times to send
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and still if host
 * does not get QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command, host
 * will configure the gSarSafetyIndex to the FW.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAR_SAFETY_REQ_RESP_TIMEOUT          "gSarSafetyReqRespTimeout"
#define CFG_SAR_SAFETY_REQ_RESP_TIMEOUT_MIN      (500)
#define CFG_SAR_SAFETY_REQ_RESP_TIMEOUT_MAX      (1000)
#define CFG_SAR_SAFETY_REQ_RESP_TIMEOUT_DEFAULT  (1000)

/*
 * <ini>
 * gSarSafetyReqRespRetry - Specify SAR request response retries value
 *
 * @Min: 1
 * @Max: 10
 * Default: 5
 *
 * This ini is used to define SAR request-response retries value.
 * SAR request-response timer will wait for the gSarReqRespTimeout for
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and if
 * SAR request-response timer timeouts host will indicate user space
 * for gSarSafetyReqRespRetry number of times to send
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and still if
 * host does not get QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor
 * command, host will configure the gSarSafetyIndex to the FW.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAR_SAFETY_REQ_RESP_RETRIES             "gSarSafetyReqRespRetry"
#define CFG_SAR_SAFETY_REQ_RESP_RETRIES_MIN         (1)
#define CFG_SAR_SAFETY_REQ_RESP_RETRIES_MAX         (5)
#define CFG_SAR_SAFETY_REQ_RESP_RETRIES_DEFAULT     (5)

/*
 * <ini>
 * gSarSafetyIndex - Specify SAR safety index
 *
 * @Min: 0
 * @Max: 11
 * Default: 11
 *
 * This ini is used to define SAR safety index, when sar safety timer
 * timeouts or sar request response timer timeouts for gSarSafetyReqRespRetry
 * number of times, host will configure gSarSafetyIndex value to the FW.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAR_SAFETY_INDEX            "gSarSafetyIndex"
#define CFG_SAR_SAFETY_INDEX_MIN        (0)
#define CFG_SAR_SAFETY_INDEX_MAX        (11)
#define CFG_SAR_SAFETY_INDEX_DEFAULT    (11)

/*
 * <ini>
 * gSarSafetySleepIndex - Specify SAR Safety sleep index
 *
 * @Min: 0
 * @Max: 11
 * Default: 11
 *
 * This ini is used to define SAR sleep index, when device goes into the
 * sleep mode, before going into the sleep mode host configures
 * gSarSafetySleepIndex value to the FW.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAR_SAFETY_SLEEP_INDEX             "gSarSafetySleepIndex"
#define CFG_SAR_SAFETY_SLEEP_INDEX_MIN         (0)
#define CFG_SAR_SAFETY_SLEEP_INDEX_MAX         (11)
#define CFG_SAR_SAFETY_SLEEP_INDEX_DEFAULT     (11)

/*
 * <ini>
 * gEnableSarSafety - Enable/Disable SAR safety feature
 *
 * @Min: 0
 * @Max: 1
 * Default: 0
 *
 * This ini is used to enable/disable SAR safety feature
 * Value 1 of this ini enables SAR safety feature and
 * value 0 of this ini disables SAR safety feature
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SAR_SAFETY_FEATURE           "gEnableSarSafety"
#define CFG_ENABLE_SAR_SAFETY_FEATURE_MIN       (0)
#define CFG_ENABLE_SAR_SAFETY_FEATURE_MAX       (1)
#define CFG_ENABLE_SAR_SAFETY_FEATURE_DEFAULT   (0)

/*
 * <ini>
 * gConfigSarSafetySleepIndex - Enable/Disable SAR Safety sleep index
 *
 * @Min: 0
 * @Max: 1
 * Default: 0
 *
 * This Configuration is to decide that before going to
 * sleep mode whether to maintain high RF power
 * (SAR disable) or to configure SAR sleep mode index
 *
 * Value 0 for this ini indicates to maintain high
 * RF power (SAR disable)
 * Value 1 for this ini indicates to configure SAR
 * sleep mode index.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CONFIG_SAR_SAFETY_SLEEP_MODE_INDEX     "gConfigSarSafetySleepIndex"
#define CFG_CONFIG_SAR_SAFETY_SLEEP_MODE_INDEX_MIN       (0)
#define CFG_CONFIG_SAR_SAFETY_SLEEP_MODE_INDEX_MAX       (1)
#define CFG_CONFIG_SAR_SAFETY_SLEEP_MODE_INDEX_DEFAULT   (0)
#endif

#define CFG_PKT_CAPTURE_MODE_MGMT_PKT	BIT(0)
#define CFG_PKT_CAPTURE_MODE_DATA_PKT	BIT(1)

/*
 * <ini>
 * packet_capture_mode - Packet capture mode
 * @Min: 0
 * @Max: 3
 * Default: 0 - Capture no packets
 *
 * This ini is used to decide packet capture mode
 *
 * packet_capture_mode = 0 - Capture no packets
 * packet_capture_mode = 1 - Capture management packets only
 * packet_capture_mode = 2 - Capture data packets only
 * packet_capture_mode = 3 - Capture both data and management packets
 *
 * Supported Feature: packet capture
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PKT_CAPTURE_MODE		"packet_capture_mode"
#define CFG_PKT_CAPTURE_MODE_MIN	0
#define CFG_PKT_CAPTURE_MODE_MAX	(CFG_PKT_CAPTURE_MODE_MGMT_PKT | \
					 CFG_PKT_CAPTURE_MODE_DATA_PKT)
#define CFG_PKT_CAPTURE_MODE_DEFAULT	0

/*
 * <ini>
 * disable_4way_hs_offload - Enable/Disable 4 way handshake offload to firmware
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * 0  4-way HS to be handled in firmware
 * 1  4-way HS to be handled in supplicant
 *
 * Related: None
 *
 * Supported Feature: STA Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_4WAY_HS_OFFLOAD           "disable_4way_hs_offload"
#define CFG_DISABLE_4WAY_HS_OFFLOAD_MIN       (0)
#define CFG_DISABLE_4WAY_HS_OFFLOAD_MAX       (1)
#define CFG_DISABLE_4WAY_HS_OFFLOAD_DEFAULT   (0)

/*
 * <ini>
 * nb_commands_interval - Used to rate limit nb commands from userspace
 *
 * @Min: 0
 * @Max: 10
 * Default: 3
 *
 * This ini is used to specify the duration in which any supp. nb command from
 * userspace will not be processed completely in driver. For ex, the default
 * value of 3 seconds signifies that consecutive commands within that
 * time will not be processed fully.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_NB_COMMANDS_RATE_LIMIT          "nb_commands_interval"
#define CFG_NB_COMMANDS_RATE_LIMIT_MIN      (0)
#define CFG_NB_COMMANDS_RATE_LIMIT_MAX      (10)
#define CFG_NB_COMMANDS_RATE_LIMIT_DEFAULT  (3)

/*
 * <ini>
 * enable_time_sync_ftm - Time Sync FTM feature support
 * @Min: 0
 * @Max: 1
 * @Default: 0 - Disable feature
 *
 * When set to 1 Time Sync FTM feature will be enabled.
 *
 * Supported Feature: Time Sync FTM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_TIME_SYNC_FTM		"enable_time_sync_ftm"
#define CFG_ENABLE_TIME_SYNC_FTM_MIN		(0)
#define CFG_ENABLE_TIME_SYNC_FTM_MAX		(1)
#define CFG_ENABLE_TIME_SYNC_FTM_DEFAULT	(0)

/*
 * <ini>
 * time_sync_ftm_mode- Time Sync FTM feature Mode configuration
 * @Min: 0 - Aggregated Mode
 * @Max: 1 - Burst Mode
 * @Default: 0
 *
 * This ini is applicable only if enable_time_sync_ftm  is set to 1.
 *
 * Supported Feature: Time Sync FTM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TIME_SYNC_FTM_MODE			"time_sync_ftm_mode"
#define CFG_TIME_SYNC_FTM_MODE_MIN		(0)
#define CFG_TIME_SYNC_FTM_MODE_MAX		(1)
#define CFG_TIME_SYNC_FTM_MODE_DEFAULT		(0)

/*
 * <ini>
 * time_sync_ftm_role- Time Sync FTM feature Role configuration
 * @Min: 0 - Slave Role
 * @Max: 1 - Master Role
 * @Default: 0
 *
 * This ini is applicable only if enable_time_sync_ftm is set to 1.
 *
 * Supported Feature: Time Sync FTM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TIME_SYNC_FTM_ROLE			"time_sync_ftm_role"
#define CFG_TIME_SYNC_FTM_ROLE_MIN		(0)
#define CFG_TIME_SYNC_FTM_ROLE_MAX		(1)
#define CFG_TIME_SYNC_FTM_ROLE_DEFAULT		(0)

/*
 * <ini>
 * bmiss_skip_full_scan - To decide whether firmware does channel map based
 * partial scan or partial scan followed by full scan in case no candidate is
 * found in partial scan.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * 0 : Based on the channel map , firmware does scan to find new AP. if AP is
 *     not found then it does a full scan on all valid channels.
 * 1 : Firmware does channel map based partial scan only.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BMISS_SKIP_FULL_SCAN               "bmiss_skip_full_scan"
#define CFG_BMISS_SKIP_FULL_SCAN_MIN           0
#define CFG_BMISS_SKIP_FULL_SCAN_MAX           1
#define CFG_BMISS_SKIP_FULL_SCAN_DEFAULT       0

#ifdef WLAN_FEATURE_PERIODIC_STA_STATS
/*
 * <ini>
 * periodic_stats_timer_interval - Print selective stats on this specified
 *				   interval
 *
 * @Min: 0
 * @Max: 10000
 * Default: 3000
 *
 * This ini is used to specify interval in milliseconds for periodic stats
 * timer. This timer will print selective stats after expiration of each
 * interval. STA starts this periodic timer after initial connection or after
 * roaming is successful. This will be restarted for every
 * periodic_stats_timer_interval till the periodic_stats_timer_duration expires.
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PERIODIC_STATS_TIMER_INTERVAL		"periodic_stats_timer_interval"
#define CFG_PERIODIC_STATS_TIMER_INTERVAL_MIN		(0)
#define CFG_PERIODIC_STATS_TIMER_INTERVAL_MAX		(10000)
#define CFG_PERIODIC_STATS_TIMER_INTERVAL_DEFAULT	(3000)

/*
 * <ini>
 * periodic_stats_timer_duration - Used as duration for which periodic timer
 *				   should run
 *
 * @Min: 0
 * @Max: 60000
 * Default: 30000
 *
 * This ini is used as duration in milliseconds for which periodic stats timer
 * should run. This periodic timer will print selective stats for every
 * periodic_stats_timer_interval until this duration is reached.
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PERIODIC_STATS_TIMER_DURATION		"periodic_stats_timer_duration"
#define CFG_PERIODIC_STATS_TIMER_DURATION_MIN		(0)
#define CFG_PERIODIC_STATS_TIMER_DURATION_MAX		(60000)
#define CFG_PERIODIC_STATS_TIMER_DURATION_DEFAULT	(30000)

#endif /* WLAN_FEATURE_PERIODIC_STA_STATS */

/*
 * <ini>
 * sta_disable_roam - Disable Roam on sta interface
 * @Min: 0 - Roam Enabled on sta interface
 * @Max: 0xffffffff - Roam Disabled on sta interface irrespective
 * of other interface connections
 * @Default: 0x00
 *
 * Disable roaming on STA iface to avoid audio glitches on p2p and ndp if
 * those are in connected state. Each bit for "sta_disable_roam" INI represents
 * an interface for which sta roaming can be disabled.
 *
 * LFR3_STA_ROAM_DISABLE_BY_P2P BIT(0)
 * LFR3_STA_ROAM_DISABLE_BY_NAN BIT(1)
 *
 * Related: None.
 *
 * Supported Feature: ROAM
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_STA_DISABLE_ROAM            "sta_disable_roam"
#define CFG_STA_DISABLE_ROAM_MIN         (0)
#define CFG_STA_DISABLE_ROAM_MAX         (0XFFFFFFFF)
#define CFG_STA_DISABLE_ROAM_DEFAULT     (0X00)

/*
 * <ini>
 * dfs_chan_ageout_time - Set DFS Channel ageout time(in seconds)
 * @Min: 0
 * @Max: 8
 * Default: 0
 *
 * Ageout time is the time upto which DFS channel information such as beacon
 * found is remembered. So that Firmware performs Active scan instead of the
 * Passive to reduce the Dwell time.
 * This ini Parameter used to set ageout timer value from host to FW.
 * If not set, Firmware will disable ageout time.
 *
 * Supported Feature: STA scan in DFS channels
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DFS_CHAN_AGEOUT_TIME		"dfs_chan_ageout_time"
#define CFG_DFS_CHAN_AGEOUT_TIME_MIN		(0)
#define CFG_DFS_CHAN_AGEOUT_TIME_MAX		(8)
#define CFG_DFS_CHAN_AGEOUT_TIME_DEFAULT	(0)

/*
 * Type declarations
 */

struct hdd_config {
	/* Bitmap to track what is explicitly configured */
	DECLARE_BITMAP(bExplicitCfg, MAX_CFG_INI_ITEMS);

	/* Config parameters */
#ifdef WLAN_NUD_TRACKING
	bool enable_nud_tracking;
#endif
	bool enable_connected_scan;
	uint32_t RTSThreshold;
	uint32_t FragmentationThreshold;
	uint8_t OperatingChannel;
	bool ShortSlotTimeEnabled;
	bool Is11dSupportEnabled;
	bool Is11hSupportEnabled;
	bool fSupplicantCountryCodeHasPriority;
	uint32_t HeartbeatThresh24;
	char PowerUsageControl[4];
	bool fIsImpsEnabled;
	bool is_ps_enabled;
	uint32_t auto_bmps_timer_val;
	uint32_t icmp_disable_ps_val;
	uint32_t nBmpsMaxListenInterval;
	uint32_t nBmpsMinListenInterval;
	enum hdd_dot11_mode dot11Mode;
	uint32_t nChannelBondingMode24GHz;
	bool override_ht20_40_24g;
	uint32_t nChannelBondingMode5GHz;
	uint32_t MaxRxAmpduFactor;
	uint32_t ShortGI20MhzEnable;
	uint32_t ScanResultAgeCount;
	uint8_t nRssiCatGap;
	bool fIsShortPreamble;
	struct qdf_mac_addr IbssBssid;
	uint32_t AdHocChannel5G;
	uint32_t AdHocChannel24G;

	bool apUapsdEnabled;
	bool apRandomBssidEnabled;
	bool apProtEnabled;
	uint16_t apProtection;
	bool apOBSSProtEnabled;
	bool apDisableIntraBssFwd;
	uint8_t enableLTECoex;
	uint32_t apKeepAlivePeriod;
	uint32_t goKeepAlivePeriod;
	enum station_keepalive_method sta_keepalive_method;
	uint32_t apLinkMonitorPeriod;
	uint32_t goLinkMonitorPeriod;
	uint32_t nBeaconInterval;
	uint8_t nTxPowerCap;    /* In dBm */
	bool allow_tpc_from_ap;
	uint8_t disablePacketFilter;
	bool fRrmEnable;
	uint16_t nRrmRandnIntvl;
	/* length includes separator */
	char rm_capability[3 * DOT11F_IE_RRMENABLEDCAP_MAX_LEN];

	/* Bitmap for operating voltage corner mode */
	uint32_t vc_mode_cfg_bitmap;

#ifdef MWS_COEX
	/* Bitmap for MWS-COEX 4G Quick FTDM */
	uint32_t mws_coex_4g_quick_tdm;

	/* Bitmap for MWS-COEX 5G-NR power limit */
	uint32_t mws_coex_5g_nr_pwr_limit;
#endif

	uint16_t nNeighborScanPeriod;
	uint16_t neighbor_scan_min_period;
	uint8_t nNeighborLookupRssiThreshold;
	uint8_t delay_before_vdev_stop;
	uint8_t nOpportunisticThresholdDiff;
	uint8_t nRoamRescanRssiDiff;
	uint8_t neighborScanChanList[WNI_CFG_VALID_CHANNEL_LIST_LEN];
	uint16_t nNeighborScanMinChanTime;
	uint16_t nNeighborScanMaxChanTime;
	uint16_t nMaxNeighborReqTries;
	uint16_t nNeighborResultsRefreshPeriod;
	uint16_t nEmptyScanRefreshPeriod;
	uint8_t nRoamBmissFirstBcnt;
	uint8_t nRoamBmissFinalBcnt;
	uint8_t nRoamBeaconRssiWeight;
	uint32_t nhi_rssi_scan_max_count;
	uint32_t nhi_rssi_scan_rssi_delta;
	uint32_t nhi_rssi_scan_delay;
	int32_t nhi_rssi_scan_rssi_ub;

	/* Additional Handoff params */
	uint16_t nVccRssiTrigger;
	uint32_t nVccUlMacLossThreshold;

	uint32_t nPassiveMinChnTime;    /* in units of milliseconds */
	uint32_t nPassiveMaxChnTime;    /* in units of milliseconds */
	uint32_t nActiveMinChnTime;     /* in units of milliseconds */
	uint32_t nActiveMaxChnTime;     /* in units of milliseconds */
	uint32_t active_dwell_2g;       /* in units of milliseconds */
	uint32_t scan_probe_repeat_time;
	uint32_t scan_num_probes;
	bool drop_bcn_on_chan_mismatch;

	uint32_t nInitialDwellTime;     /* in units of milliseconds */
	bool initial_scan_no_dfs_chnl;

	uint32_t nPassiveMinChnTimeConc;        /* in units of milliseconds */
	uint32_t nPassiveMaxChnTimeConc;        /* in units of milliseconds */
	uint32_t nActiveMinChnTimeConc; /* in units of milliseconds */
	uint32_t nActiveMaxChnTimeConc; /* in units of milliseconds */
	uint32_t nRestTimeConc; /* in units of milliseconds */
	/* In units of milliseconds */
	uint32_t       min_rest_time_conc;
	/* In units of milliseconds */
	uint32_t       idle_time_conc;

	uint8_t nMaxPsPoll;

	uint8_t nRssiFilterPeriod;
	uint8_t fMaxLIModulatedDTIM;

	bool mcc_rts_cts_prot_enable;
	bool mcc_bcast_prob_resp_enable;
	uint8_t nDataInactivityTimeout;
	uint8_t wow_data_inactivity_timeout;

	/* WMM QoS Configuration */
	enum hdd_wmm_user_mode WmmMode;
	bool b80211eIsEnabled;
	uint8_t UapsdMask;      /* what ACs to setup U-APSD for at assoc */
	uint32_t InfraUapsdVoSrvIntv;
	uint32_t InfraUapsdVoSuspIntv;
	uint32_t InfraUapsdViSrvIntv;
	uint32_t InfraUapsdViSuspIntv;
	uint32_t InfraUapsdBeSrvIntv;
	uint32_t InfraUapsdBeSuspIntv;
	uint32_t InfraUapsdBkSrvIntv;
	uint32_t InfraUapsdBkSuspIntv;
	bool isFastRoamIniFeatureEnabled;
	bool MAWCEnabled;
	bool mawc_roam_enabled;
	uint32_t mawc_roam_traffic_threshold;
	int8_t mawc_roam_ap_rssi_threshold;
	uint8_t mawc_roam_rssi_high_adjust;
	uint8_t mawc_roam_rssi_low_adjust;
#ifdef FEATURE_WLAN_ESE
	uint32_t InfraInactivityInterval;
	bool isEseIniFeatureEnabled;
#endif
	bool isFastTransitionEnabled;
	uint8_t RoamRssiDiff;
	int32_t rssi_abs_thresh;
	bool isWESModeEnabled;
	uint32_t pmkid_modes;
	bool isRoamOffloadScanEnabled;
	bool bImplicitQosEnabled;

	/* default TSPEC parameters for AC_VO */
	enum sme_qos_wmm_dir_type InfraDirAcVo;
	uint16_t InfraNomMsduSizeAcVo;
	uint32_t InfraMeanDataRateAcVo;
	uint32_t InfraMinPhyRateAcVo;
	uint16_t InfraSbaAcVo;

	/* default TSPEC parameters for AC_VI */
	enum sme_qos_wmm_dir_type InfraDirAcVi;
	uint16_t InfraNomMsduSizeAcVi;
	uint32_t InfraMeanDataRateAcVi;
	uint32_t InfraMinPhyRateAcVi;
	uint16_t InfraSbaAcVi;

	/* default TSPEC parameters for AC_BE */
	enum sme_qos_wmm_dir_type InfraDirAcBe;
	uint16_t InfraNomMsduSizeAcBe;
	uint32_t InfraMeanDataRateAcBe;
	uint32_t InfraMinPhyRateAcBe;
	uint16_t InfraSbaAcBe;

	/* default TSPEC parameters for AC_BK */
	enum sme_qos_wmm_dir_type InfraDirAcBk;
	uint16_t InfraNomMsduSizeAcBk;
	uint32_t InfraMeanDataRateAcBk;
	uint32_t InfraMinPhyRateAcBk;
	uint16_t InfraSbaAcBk;

	uint32_t DelayedTriggerFrmInt;

	char enableConcurrentSTA[CFG_CONCURRENT_IFACE_MAX_LEN];

	/* Control for Replay counetr. value 1 means
	 * single replay counter for all TID
	 */
	bool bSingleTidRc;
	bool fhostArpOffload;
	enum pmo_hw_filter_mode hw_filter_mode_bitmap;
	bool ssdp;

#ifdef FEATURE_RUNTIME_PM
	bool runtime_pm;
	uint32_t runtime_pm_delay;
#endif

#ifdef FEATURE_WLAN_RA_FILTERING
	bool IsRArateLimitEnabled;
	uint16_t RArateLimitInterval;
#endif
#ifdef FEATURE_WLAN_SCAN_PNO
	bool PnoOffload;
#endif
	bool fhostNSOffload;
	bool burstSizeDefinition;
	uint8_t tsInfoAckPolicy;

	bool AddTSWhenACMIsOff;

	uint32_t infraStaKeepAlivePeriod;
	uint8_t nBandCapability;
	bool teleBcnWakeupEn;

/* QDF Trace Control*/
	uint16_t qdf_trace_enable_wdi;
	uint16_t qdf_trace_enable_hdd;
	uint16_t qdf_trace_enable_sme;
	uint16_t qdf_trace_enable_pe;
	uint16_t qdf_trace_enable_pmc;
	uint16_t qdf_trace_enable_wma;
	uint16_t qdf_trace_enable_sys;
	uint16_t qdf_trace_enable_qdf;
	uint16_t qdf_trace_enable_sap;
	uint16_t qdf_trace_enable_hdd_sap;
	uint16_t qdf_trace_enable_bmi;
	uint16_t qdf_trace_enable_cfg;
	uint16_t qdf_trace_enable_txrx;
	uint16_t qdf_trace_enable_dp;
	uint16_t qdf_trace_enable_htc;
	uint16_t qdf_trace_enable_hif;
	uint16_t qdf_trace_enable_hdd_sap_data;
	uint16_t qdf_trace_enable_hdd_data;
	uint16_t qdf_trace_enable_epping;
	uint16_t qdf_trace_enable_qdf_devices;
	uint16_t qdf_trace_enable_wifi_pos;
	uint16_t qdf_trace_enable_nan;
	uint16_t qdf_trace_enable_regulatory;
	uint16_t qdf_trace_enable_cp_stats;
#ifdef ENABLE_MTRACE_LOG
	bool enable_mtrace;
#endif
	uint16_t nTeleBcnMaxListenInterval;
	uint8_t enableBypass11d;
	uint8_t enableDFSChnlScan;
	bool wake_lock_in_user_scan;
	bool honour_nl_scan_policy_flags;
	uint8_t enable_dfs_pno_chnl_scan;
	uint8_t enableDynamicDTIM;
	uint8_t ShortGI40MhzEnable;
	enum hdd_link_speed_rpt_type reportMaxLinkSpeed;
	int32_t linkSpeedRssiHigh;
	int32_t linkSpeedRssiMid;
	int32_t linkSpeedRssiLow;
	bool nRoamPrefer5GHz;
	bool nRoamIntraBand;
	uint8_t nProbes;
	uint16_t nRoamScanHomeAwayTime;
	uint8_t enableMCC;
	uint8_t allowMCCGODiffBI;
	bool isP2pDeviceAddrAdministrated;
	uint8_t thermalMitigationEnable;
	uint32_t throttlePeriod;
	uint32_t throttle_dutycycle_level0;
	uint32_t throttle_dutycycle_level1;
	uint32_t throttle_dutycycle_level2;
	uint32_t throttle_dutycycle_level3;
#if defined(CONFIG_HL_SUPPORT) && defined(QCA_BAD_PEER_TX_FLOW_CL)
	bool bad_peer_txctl_enable;
	uint32_t bad_peer_txctl_prd;
	uint32_t bad_peer_txctl_txq_lmt;
	uint32_t bad_peer_tgt_backoff;
	uint32_t bad_peer_tgt_report_prd;
	uint32_t bad_peer_cond_ieee80211b;
	uint32_t bad_peer_delta_ieee80211b;
	uint32_t bad_peer_pct_ieee80211b;
	uint32_t bad_peer_tput_ieee80211b;
	uint32_t bad_peer_limit_ieee80211b;
	uint32_t bad_peer_cond_ieee80211ag;
	uint32_t bad_peer_delta_ieee80211ag;
	uint32_t bad_peer_pct_ieee80211ag;
	uint32_t bad_peer_tput_ieee80211ag;
	uint32_t bad_peer_limit_ieee80211ag;
	uint32_t bad_peer_cond_ieee80211n;
	uint32_t bad_peer_delta_ieee80211n;
	uint32_t bad_peer_pct_ieee80211n;
	uint32_t bad_peer_tput_ieee80211n;
	uint32_t bad_peer_limit_ieee80211n;
	uint32_t bad_peer_cond_ieee80211ac;
	uint32_t bad_peer_delta_ieee80211ac;
	uint32_t bad_peer_pct_ieee80211ac;
	uint32_t bad_peer_tput_ieee80211ac;
	uint32_t bad_peer_limit_ieee80211ac;
#endif
	uint8_t vhtChannelWidth;
	uint8_t vhtRxMCS;
	uint8_t vhtTxMCS;
	bool enableTxBF;
	bool enable_subfee_vendor_vhtie;
	bool enable_txbf_sap_mode;
	bool enable_vht20_mcs9;
	uint8_t txBFCsnValue;
	bool enable_su_tx_bformer;
	uint8_t vhtRxMCS2x2;
	uint8_t vhtTxMCS2x2;
	uint8_t disable_high_ht_mcs_2x2;
	bool enable2x2;
	uint8_t txchainmask1x1;
	uint8_t rxchainmask1x1;
	bool enableMuBformee;
	bool enableVhtpAid;
	bool enableVhtGid;
	bool enableTxBFin20MHz;
	uint8_t enableAmpduPs;
	uint8_t enableHtSmps;
	uint8_t htSmps;
	uint8_t enableModulatedDTIM;
	uint32_t fEnableMCAddrList;
	bool enableFirstScan2GOnly;
	bool skipDfsChnlInP2pSearch;
	bool ignoreDynamicDtimInP2pMode;
	bool enableRxSTBC;
	bool enableTxSTBC;
	uint8_t enable_tx_ldpc;
	uint8_t enable_rx_ldpc;
	bool enable5gEBT;
	bool prevent_link_down;
#ifdef FEATURE_WLAN_TDLS
	bool fEnableTDLSSupport;
	bool fEnableTDLSImplicitTrigger;
	uint32_t fTDLSTxStatsPeriod;
	uint32_t fTDLSTxPacketThreshold;
	uint32_t fTDLSMaxDiscoveryAttempt;
	uint32_t tdls_idle_timeout;
	uint32_t fTDLSIdlePacketThreshold;
	int32_t fTDLSRSSITriggerThreshold;
	int32_t fTDLSRSSITeardownThreshold;
	int32_t fTDLSRSSIDelta;
	uint32_t fTDLSUapsdMask;        /* what ACs to setup U-APSD for TDLS */
	uint32_t fEnableTDLSBufferSta;
	uint32_t fEnableTDLSSleepSta;
	uint32_t fTDLSPuapsdInactivityTimer;
	uint32_t fTDLSRxFrameThreshold;
	uint32_t fTDLSPuapsdPTIWindow;
	uint32_t fTDLSPuapsdPTRTimeout;
	uint8_t fTDLSExternalControl;
	uint32_t fEnableTDLSOffChannel;
	uint32_t fEnableTDLSWmmMode;
	uint8_t fTDLSPrefOffChanNum;
	uint8_t fTDLSPrefOffChanBandwidth;
	uint8_t enable_tdls_scan;
	uint32_t tdls_peer_kickout_threshold;
	uint32_t tdls_discovery_wake_timeout;
#endif
	uint8_t scanAgingTimeout;
	uint8_t disableLDPCWithTxbfAP;
	uint8_t enableMCCAdaptiveScheduler;
	bool sapAllowAllChannel;
	bool enableSSR;
	bool enable_data_stall_det;
	bool enableVhtFor24GHzBand;
	bool enable_sap_vendor_vht;
	bool bFastRoamInConIniFeatureEnabled;
	bool fEnableSNRMonitoring;
	/*PNO related parameters */
#ifdef FEATURE_WLAN_SCAN_PNO
	bool configPNOScanSupport;
	uint32_t configPNOScanTimerRepeatValue;
	uint32_t pno_slow_scan_multiplier;
#endif
	uint8_t max_amsdu_num;
	uint8_t nSelect5GHzMargin;
	uint8_t isCoalesingInIBSSAllowed;

	/* IBSS Power Save related parameters */
	uint32_t ibssATIMWinSize;
	uint8_t isIbssPowerSaveAllowed;
	uint8_t isIbssPowerCollapseAllowed;
	uint8_t isIbssAwakeOnTxRx;
	uint32_t ibssInactivityCount;
	uint32_t ibssTxSpEndInactivityTime;
	uint32_t ibssPsWarmupTime;
	uint32_t ibssPs1RxChainInAtimEnable;

	bool enable_ip_tcp_udp_checksum_offload;
	uint8_t enablePowersaveOffload;
	bool enablefwprint;
	uint8_t enable_fw_log;
	uint8_t fVhtAmpduLenExponent;
	uint32_t vhtMpduLen;
	uint32_t IpaConfig;
	bool IpaClkScalingEnable;
	uint32_t IpaDescSize;
	uint32_t IpaHighBandwidthMbps;
	uint32_t IpaMediumBandwidthMbps;
	uint32_t IpaLowBandwidthMbps;
	bool IpaForceVoting;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint32_t WlanMccToSccSwitchMode;
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	uint32_t WlanAutoShutdown;
#endif
	uint8_t wowEnable;
	uint8_t maxNumberOfPeers;
	uint8_t disableDFSChSwitch;
	uint8_t enableDFSMasterCap;
	uint16_t thermalTempMinLevel0;
	uint16_t thermalTempMaxLevel0;
	uint16_t thermalTempMinLevel1;
	uint16_t thermalTempMaxLevel1;
	uint16_t thermalTempMinLevel2;
	uint16_t thermalTempMaxLevel2;
	uint16_t thermalTempMinLevel3;
	uint16_t thermalTempMaxLevel3;
	uint32_t TxPower2g;
	uint32_t TxPower5g;
	uint32_t gEnableDebugLog;
	bool fDfsPhyerrFilterOffload;
	uint8_t gSapPreferredChanLocation;
	uint8_t gDisableDfsJapanW53;
	bool gEnableOverLapCh;
	bool fRegChangeDefCountry;
	bool acs_with_more_param;
	uint32_t auto_channel_select_weight;
	uint16_t max_ht_mcs_txdata;
	bool sap_get_peer_info;
	bool disable_abg_rate_txdata;
	uint8_t rate_for_tx_mgmt;
	uint8_t rate_for_tx_mgmt_2g;
	uint8_t rate_for_tx_mgmt_5g;
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	uint32_t TxFlowLowWaterMark;
	uint32_t TxFlowHighWaterMarkOffset;
	uint32_t TxFlowMaxQueueDepth;
	uint32_t TxLbwFlowLowWaterMark;
	uint32_t TxLbwFlowHighWaterMarkOffset;
	uint32_t TxLbwFlowMaxQueueDepth;
	uint32_t TxHbwFlowLowWaterMark;
	uint32_t TxHbwFlowHighWaterMarkOffset;
	uint32_t TxHbwFlowMaxQueueDepth;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
	uint32_t TxFlowStopQueueThreshold;
	uint32_t TxFlowStartQueueOffset;
	uint8_t apMaxOffloadPeers;
	uint8_t apMaxOffloadReorderBuffs;
	bool advertiseConcurrentOperation;
	bool enableMemDeepSleep;
	bool enable_cck_tx_fir_override;

	uint8_t allowDFSChannelRoam;

	bool debugP2pRemainOnChannel;

#ifndef REMOVE_PKT_LOG
	bool enablePacketLog;
	uint8_t pktlog_buf_size;
#endif

#ifdef MSM_PLATFORM
	uint32_t busBandwidthHighThreshold;
	uint32_t busBandwidthMediumThreshold;
	uint32_t busBandwidthLowThreshold;
	uint32_t busBandwidthComputeInterval;
	uint32_t enable_tcp_delack;
	bool     enable_tcp_limit_output;
	uint32_t enable_tcp_adv_win_scale;
	uint32_t tcpDelackThresholdHigh;
	uint32_t tcpDelackThresholdLow;
	uint32_t tcp_tx_high_tput_thres;
	uint32_t tcp_delack_timer_count;
	bool     enable_tcp_param_update;
	u8  periodic_stats_disp_time;
#endif /* MSM_PLATFORM */

	/* FW debug log parameters */
	uint32_t enableFwLogType;
	uint32_t enableFwLogLevel;
	uint8_t enableFwModuleLogLevel[FW_MODULE_LOG_LEVEL_STRING_LENGTH];

	/* RTS profile parameter */
	uint32_t rts_profile;

#ifdef WLAN_FEATURE_11W
	uint32_t pmfSaQueryMaxRetries;
	uint32_t pmfSaQueryRetryInterval;
#endif

	uint8_t gMaxConcurrentActiveSessions;

	uint8_t ignoreCAC;

	/* Flag to indicate crash inject enabled or not */
	bool crash_inject_enabled;

	bool enable_sap_mandatory_chan_list;

	int32_t dfsRadarPriMultiplier;
	uint8_t reorderOffloadSupport;

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	bool isRoamOffloadEnabled;
	uint32_t roam_triggers;
#endif
	bool bmiss_skip_full_scan;
	uint32_t IpaUcTxBufCount;
	uint32_t IpaUcTxBufSize;
	uint32_t IpaUcRxIndRingCount;
	uint32_t IpaUcTxPartitionBase;
#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
	/* WLAN Logging */
	bool wlan_logging_enable;
	bool wlan_logging_to_console;
	uint8_t host_log_custom_nl_proto;
#endif /* WLAN_LOGGING_SOCK_SVC_ENABLE */

#ifdef WLAN_FEATURE_LPSS
	bool enable_lpass_support;
#endif
#ifdef WLAN_FEATURE_NAN
	bool enable_nan_support;
	bool nan_separate_iface_support;
	uint16_t ndp_keep_alive_period;
	bool support_mp0_discovery;
	uint32_t nan_feature_config;
#endif
	bool enableSelfRecovery;
#ifdef FEATURE_WLAN_FORCE_SAP_SCC
	uint8_t SapSccChanAvoidance;
#endif /* FEATURE_WLAN_FORCE_SAP_SCC */

	bool enable_sap_suspend;

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	uint8_t extWowGotoSuspend;
	uint8_t extWowApp1WakeupPinNumber;
	uint8_t extWowApp2WakeupPinNumber;
	uint32_t extWowApp2KAInitPingInterval;
	uint32_t extWowApp2KAMinPingInterval;
	uint32_t extWowApp2KAMaxPingInterval;
	uint32_t extWowApp2KAIncPingInterval;
	uint16_t extWowApp2TcpSrcPort;
	uint16_t extWowApp2TcpDstPort;
	uint32_t extWowApp2TcpTxTimeout;
	uint32_t extWowApp2TcpRxTimeout;
#endif
	bool gEnableDeauthToDisassocMap;
#ifdef DHCP_SERVER_OFFLOAD
	bool enableDHCPServerOffload;
	uint32_t dhcpMaxNumClients;
	uint8_t dhcpServerIP[IPADDR_STRING_LENGTH];
#endif /* DHCP_SERVER_OFFLOAD */
	bool enable_mac_spoofing;
	uint8_t conc_custom_rule1;
	uint8_t conc_custom_rule2;
	uint8_t is_sta_connection_in_5gz_enabled;
	uint16_t p2p_listen_defer_interval;
	uint32_t sta_miracast_mcc_rest_time_val;
	uint32_t sta_scan_burst_duration;
	uint32_t p2p_scan_burst_duration;
	uint32_t go_scan_burst_duration;
	uint32_t ap_scan_burst_duration;
	bool is_ramdump_enabled;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	bool sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	uint8_t sap_11ac_override;
	uint8_t go_11ac_override;
	uint8_t sap_dot11mc;
	uint8_t prefer_non_dfs_on_radar;
	bool ignore_peer_erp_info;
	uint8_t multicast_host_fw_msgs;
	uint8_t conc_system_pref;
	bool send_deauth_before_con;
	bool tso_enable;
	bool lro_enable;
	bool gro_enable;
	bool flow_steering_enable;
	uint8_t max_msdus_per_rxinorderind;
	bool active_mode_offload;
	bool apf_packet_filter_enable;
	/* parameter for defer timer for enabling TDLS on p2p listen */
	uint16_t tdls_enable_defer_time;
	uint32_t fine_time_meas_cap;
	uint8_t max_scan_count;
#ifdef WLAN_FEATURE_FASTPATH
	bool fastpath_enable;
#endif
	uint8_t dot11p_mode;
	uint8_t etsi13_srd_chan_in_master_mode;
	uint8_t rx_mode;
	uint32_t ce_service_max_yield_time;
	uint8_t ce_service_max_rx_ind_flush;
	uint32_t napi_cpu_affinity_mask;
	/* CPU affinity mask for rx_thread */
	uint32_t rx_thread_ul_affinity_mask;
	uint32_t rx_thread_affinity_mask;
	uint8_t cpu_map_list[CFG_RPS_RX_QUEUE_CPU_MAP_LIST_LEN];
#ifdef FEATURE_WLAN_EXTSCAN
	bool     extscan_enabled;
	uint32_t extscan_passive_max_chn_time;
	uint32_t extscan_passive_min_chn_time;
	uint32_t extscan_active_max_chn_time;
	uint32_t extscan_active_min_chn_time;
#endif
	bool ce_classify_enabled;
	uint32_t dual_mac_feature_disable;
	uint8_t dbs_scan_selection[CFG_DBS_SCAN_PARAM_LENGTH];
	uint32_t sta_sap_scc_on_dfs_chan;
	uint32_t sta_sap_scc_on_lte_coex_chan;
	uint32_t go_force_scc;
	bool     tx_chain_mask_cck;
	uint8_t  tx_chain_mask_1ss;
	bool smart_chainmask_enabled;
	bool alternative_chainmask_enabled;
	uint16_t  self_gen_frm_pwr;
#ifdef FEATURE_WLAN_SCAN_PNO
	bool pno_channel_prediction;
	uint8_t top_k_num_of_channels;
	uint8_t stationary_thresh;
	uint32_t channel_prediction_full_scan;
#endif
	bool early_stop_scan_enable;
	int8_t early_stop_scan_min_threshold;
	int8_t early_stop_scan_max_threshold;
	int8_t first_scan_bucket_threshold;
	uint8_t ht_mpdu_density;
#ifdef FEATURE_LFR_SUBNET_DETECTION
	bool enable_lfr_subnet_detection;
#endif
	uint16_t obss_active_dwelltime;
	uint16_t obss_passive_dwelltime;
	uint16_t obss_width_trigger_interval;
	uint8_t inform_bss_rssi_raw;
#ifdef WLAN_FEATURE_TSF
	uint32_t tsf_gpio_pin;

#ifdef WLAN_FEATURE_TSF_PLUS
	uint8_t tsf_ptp_options;
#endif /* WLAN_FEATURE_TSF_PLUS */
#endif
	bool enable_three_way_coex_config_legacy;
	uint32_t roam_dense_traffic_thresh;
	uint32_t roam_dense_rssi_thresh_offset;
	bool ignore_peer_ht_opmode;
	uint32_t roam_dense_min_aps;
	int8_t roam_bg_scan_bad_rssi_thresh;
	uint8_t roam_bad_rssi_thresh_offset_2g;
	uint32_t roam_data_rssi_threshold_triggers;
	int32_t roam_data_rssi_threshold;
	uint32_t rx_data_inactivity_time;
	uint32_t ho_delay_for_rx;
	uint32_t min_delay_btw_roam_scans;
	uint32_t roam_trigger_reason_bitmask;
	bool roaming_scan_policy;
	uint32_t roam_bg_scan_client_bitmap;
	bool enable_edca_params;
	uint32_t edca_vo_cwmin;
	uint32_t edca_vi_cwmin;
	uint32_t edca_bk_cwmin;
	uint32_t edca_be_cwmin;
	uint32_t edca_vo_cwmax;
	uint32_t edca_vi_cwmax;
	uint32_t edca_bk_cwmax;
	uint32_t edca_be_cwmax;
	uint32_t edca_vo_aifs;
	uint32_t edca_vi_aifs;
	uint32_t edca_bk_aifs;
	uint32_t edca_be_aifs;

	/* Tuning TX sched parameters for VO (skip credit limit credit disc) */
	uint8_t  tx_sched_wrr_vo[TX_SCHED_WRR_PARAM_STRING_LENGTH];
	/* Tuning TX sched parameters for VI (skip credit limit credit disc) */
	uint8_t  tx_sched_wrr_vi[TX_SCHED_WRR_PARAM_STRING_LENGTH];
	/* Tuning TX sched parameters for BE (skip credit limit credit disc) */
	uint8_t  tx_sched_wrr_be[TX_SCHED_WRR_PARAM_STRING_LENGTH];
	/* Tuning TX sched parameters for BK (skip credit limit credit disc) */
	uint8_t  tx_sched_wrr_bk[TX_SCHED_WRR_PARAM_STRING_LENGTH];

	bool enable_fatal_event;
	bool apf_enabled;
#ifdef CONFIG_DP_TRACE
	bool enable_dp_trace;
	uint8_t dp_trace_config[DP_TRACE_CONFIG_STRING_LENGTH];
#endif
	bool adaptive_dwell_mode_enabled;
	enum scan_dwelltime_adaptive_mode scan_adaptive_dwell_mode;
	enum scan_dwelltime_adaptive_mode scan_adaptive_dwell_mode_nc;
	enum scan_dwelltime_adaptive_mode roamscan_adaptive_dwell_mode;
	enum scan_dwelltime_adaptive_mode extscan_adaptive_dwell_mode;
	enum scan_dwelltime_adaptive_mode pnoscan_adaptive_dwell_mode;
	enum scan_dwelltime_adaptive_mode global_adapt_dwelltime_mode;
	uint8_t adapt_dwell_lpf_weight;
	uint8_t adapt_dwell_passive_mon_intval;
	uint8_t adapt_dwell_wifi_act_threshold;
	bool vendor_acs_support;
	bool acs_support_for_dfs_ltecoex;
	bool bug_report_for_no_scan_results;
	bool bug_on_reinit_failure;
#ifdef WLAN_FEATURE_NAN_DATAPATH
	bool enable_nan_datapath;
	uint8_t nan_datapath_ndi_channel;
#endif
	uint32_t iface_change_wait_time;
	/* parameter to control GTX */
	uint32_t tgt_gtx_usr_cfg;
	enum cfg_sub_20_channel_width enable_sub_20_channel_width;
	bool indoor_channel_support;
	/* control marking indoor channel passive to disable */
	bool force_ssc_disable_indoor_channel;
	/* parameter to force sap into 11n */
	bool sap_force_11n_for_11ac;
	bool go_force_11n_for_11ac;
	uint16_t sap_tx_leakage_threshold;
	bool multicast_replay_filter;
	bool goptimize_chan_avoid_event;
	bool enable_go_cts2self_for_sta;
	uint32_t tx_aggregation_size;
	uint32_t tx_aggregation_size_be;
	uint32_t tx_aggregation_size_bk;
	uint32_t tx_aggregation_size_vi;
	uint32_t tx_aggregation_size_vo;
	uint32_t rx_aggregation_size;
	uint32_t tx_aggr_sw_retry_threshold_be;
	uint32_t tx_aggr_sw_retry_threshold_bk;
	uint32_t tx_aggr_sw_retry_threshold_vi;
	uint32_t tx_aggr_sw_retry_threshold_vo;
	uint32_t tx_aggr_sw_retry_threshold;
	uint32_t tx_non_aggr_sw_retry_threshold_be;
	uint32_t tx_non_aggr_sw_retry_threshold_bk;
	uint32_t tx_non_aggr_sw_retry_threshold_vi;
	uint32_t tx_non_aggr_sw_retry_threshold_vo;
	uint32_t tx_non_aggr_sw_retry_threshold;
	bool sta_prefer_80MHz_over_160MHz;
	uint8_t sap_max_inactivity_override;
	bool fw_timeout_crash;
	/* beacon count before channel switch */
	uint8_t sap_chanswitch_beacon_cnt;
	uint8_t sap_chanswitch_mode;
	uint32_t rx_wakelock_timeout;
	uint32_t max_sched_scan_plan_interval;
	uint32_t max_sched_scan_plan_iterations;
#ifdef WLAN_FEATURE_WOW_PULSE
	bool wow_pulse_support;
	uint8_t wow_pulse_pin;
	uint16_t wow_pulse_interval_high;
	uint16_t wow_pulse_interval_low;
#endif
	enum hdd_wext_control private_wext_control;
	bool sap_internal_restart;
	enum restart_beaconing_on_ch_avoid_rule
		restart_beaconing_on_chan_avoid_event;
	uint8_t is_per_roam_enabled;
	uint32_t per_roam_high_rate_threshold;
	uint32_t per_roam_low_rate_threshold;
	uint32_t per_roam_th_percent;
	uint32_t per_roam_rest_time;
	uint32_t per_roam_mon_time;
	uint32_t min_candidate_rssi;
	enum active_apf_mode active_uc_apf_mode;
	enum active_apf_mode active_mc_bc_apf_mode;
	bool enable_bcast_probe_rsp;
	uint8_t he_dynamic_frag_support;
#ifdef WLAN_FEATURE_11AX
	bool enable_ul_mimo;
	bool enable_ul_ofdma;
	uint32_t he_sta_obsspd;
#endif
#ifdef WLAN_SUPPORT_TWT
	bool enable_twt;
	uint32_t twt_congestion_timeout;
#endif
	uint32_t arp_ac_category;
	bool ani_enabled;
	bool qcn_ie_support;
	bool tx_orphan_enable;

	bool probe_req_ie_whitelist;
	/* probe request bit map ies */
	uint32_t probe_req_ie_bitmap_0;
	uint32_t probe_req_ie_bitmap_1;
	uint32_t probe_req_ie_bitmap_2;
	uint32_t probe_req_ie_bitmap_3;
	uint32_t probe_req_ie_bitmap_4;
	uint32_t probe_req_ie_bitmap_5;
	uint32_t probe_req_ie_bitmap_6;
	uint32_t probe_req_ie_bitmap_7;
	/* Probe Request multiple vendor OUIs */
	uint8_t probe_req_ouis[MAX_PRB_REQ_VENDOR_OUI_INI_LEN];
	uint32_t no_of_probe_req_ouis;
	uint32_t probe_req_voui[MAX_PROBE_REQ_OUIS];

	uint32_t timer_multiplier;
	uint8_t fils_max_chan_guard_time;
	uint8_t scan_backoff_multiplier;
	bool mawc_nlo_enabled;
	uint32_t mawc_nlo_exp_backoff_ratio;
	uint32_t mawc_nlo_init_scan_interval;
	uint32_t mawc_nlo_max_scan_interval;
	enum hdd_external_acs_policy external_acs_policy;
	/* threshold of packet drops at which FW initiates disconnect */
	uint16_t pkt_err_disconn_th;
	enum force_1x1_type is_force_1x1_enable;
	uint8_t enable_rts_sifsbursting;
	uint8_t max_mpdus_inampdu;
	uint16_t sap_max_mcs_txdata;
	enum pmo_auto_pwr_detect_failure_mode auto_pwr_save_fail_mode;
	uint16_t num_11b_tx_chains;
	uint16_t num_11ag_tx_chains;
	uint8_t ito_repeat_count;
	/* LCA(Last connected AP) disallow configs */
	uint32_t disallow_duration;
	uint32_t rssi_channel_penalization;
	uint32_t num_disallowed_aps;
	bool oce_sta_enabled;
	bool oce_sap_enabled;
	bool enable_11d_in_world_mode;
	/* 5G preference parameters for boosting RSSI */
	bool                        enable_5g_band_pref;
	int8_t                      rssi_boost_threshold_5g;
	uint8_t                     rssi_boost_factor_5g;
	uint8_t                     max_rssi_boost_5g;
	/* 5G preference parameters for dropping RSSI*/
	int8_t                      rssi_penalize_threshold_5g;
	uint8_t                     rssi_penalize_factor_5g;
	uint8_t                     max_rssi_penalize_5g;
	bool enable_lprx;
	uint8_t upper_brssi_thresh;
	uint8_t lower_brssi_thresh;
	bool enable_dtim_1chrx;
	int8_t rssi_thresh_offset_5g;
	bool is_ndi_mac_randomized;
	uint32_t scan_11d_interval;
	bool chan_switch_hostapd_rate_enabled;
	bool is_bssid_hint_priority;
	uint8_t rssi_weightage;
	uint8_t ht_caps_weightage;
	uint8_t vht_caps_weightage;
	uint8_t he_caps_weightage;
	uint8_t chan_width_weightage;
	uint8_t chan_band_weightage;
	uint8_t nss_weightage;
	uint8_t beamforming_cap_weightage;
	uint8_t pcl_weightage;
	uint8_t channel_congestion_weightage;
	uint8_t oce_wan_weightage;
	uint32_t bandwidth_weight_per_index;
	uint32_t nss_weight_per_index;
	uint32_t band_weight_per_index;
	uint32_t best_rssi_threshold;
	uint32_t good_rssi_threshold;
	uint32_t bad_rssi_threshold;
	uint32_t good_rssi_pcnt;
	uint32_t bad_rssi_pcnt;
	uint32_t good_rssi_bucket_size;
	uint32_t bad_rssi_bucket_size;
	uint32_t rssi_pref_5g_rssi_thresh;
	uint8_t num_esp_qbss_slots;
	uint32_t esp_qbss_score_slots3_to_0;
	uint32_t esp_qbss_score_slots7_to_4;
	uint32_t esp_qbss_score_slots11_to_8;
	uint32_t esp_qbss_score_slots15_to_12;
	uint8_t num_oce_wan_slots;
	uint32_t oce_wan_score_slots3_to_0;
	uint32_t oce_wan_score_slots7_to_4;
	uint32_t oce_wan_score_slots11_to_8;
	uint32_t oce_wan_score_slots15_to_12;
	bool enable_scoring_for_roam;
	bool force_rsne_override;
	bool is_fils_enabled;
	uint16_t wlm_latency_enable;
	uint16_t wlm_latency_level;
	uint32_t wlm_latency_flags_normal;
	uint32_t wlm_latency_flags_moderate;
	uint32_t wlm_latency_flags_low;
	uint32_t wlm_latency_flags_ultralow;
	/* mbo related thresholds */
	int8_t mbo_candidate_rssi_thres;
	int8_t mbo_current_rssi_thres;
	int8_t mbo_current_rssi_mcc_thres;
	int8_t mbo_candidate_rssi_btc_thres;
	uint8_t packet_filters_bitmap;
	uint8_t enable_phy_reg_retention;
	uint8_t dfs_beacon_tx_enhanced;
	uint16_t reduced_beacon_interval;
	bool rssi_assoc_reject_enabled;
	bool oce_probe_req_rate_enabled;
	bool oce_probe_resp_rate_enabled;
	bool oce_beacon_rate_enabled;
	bool probe_req_deferral_enabled;
	bool fils_discovery_sap_enabled;
	bool esp_for_roam_enabled;
	uint8_t tx_chain_mask_2g;
	uint8_t rx_chain_mask_2g;
	uint8_t tx_chain_mask_5g;
	uint8_t rx_chain_mask_5g;
	uint32_t btm_offload_config;
#ifdef WLAN_FEATURE_SAE
	bool is_sae_enabled;
	bool sap_sae_enabled;
#endif
	uint32_t btm_solicited_timeout;
	uint32_t btm_max_attempt_cnt;
	uint32_t btm_sticky_time;
	uint32_t btm_query_bitmask;
	uint32_t btm_trig_min_candidate_score;
	bool gcmp_enabled;
	bool is_11k_offload_supported;
	uint32_t offload_11k_enable_bitmask;
	uint32_t neighbor_report_offload_params_bitmask;
	uint32_t neighbor_report_offload_time_offset;
	uint32_t neighbor_report_offload_low_rssi_offset;
	uint32_t neighbor_report_offload_bmiss_count_trigger;
	uint32_t neighbor_report_offload_per_threshold_offset;
	uint32_t neighbor_report_offload_cache_timeout;
	uint32_t neighbor_report_offload_max_req_cap;
	bool action_oui_enable;
	uint8_t action_oui_str[ACTION_OUI_MAXIMUM_ID][ACTION_OUI_MAX_STR_LEN];
	uint16_t wmi_wq_watchdog_timeout;
	bool enable_dtim_selection_diversity;
	uint32_t channel_select_logic_conc;
	bool enable_bt_chain_separation;
	uint8_t enable_tx_sch_delay;
	uint32_t enable_secondary_rate;
	HDD_GREEN_AP_CFG_FIELDS
	bool is_unit_test_framework_enabled;
	bool enable_ftopen;
	bool enable_rtt_mac_randomization;
	bool roam_force_rssi_trigger;
	uint8_t set_btc_mode;
	uint8_t set_antenna_isolation;
	uint8_t set_max_tx_power_for_btc;
	int16_t set_wlan_low_rssi_threshold;
	int16_t set_bt_low_rssi_threshold;
	int16_t set_bt_interference_low_ll;
	int16_t set_bt_interference_low_ul;
	int16_t set_bt_interference_medium_ll;
	int16_t set_bt_interference_medium_ul;
	int16_t set_bt_interference_high_ll;
	int16_t set_bt_interference_high_ul;
#ifdef FEATURE_MPTA_HELPER
	bool    set_mpta_helper_enable;
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	uint32_t roam_preauth_retry_count;
	uint32_t roam_preauth_no_ack_timeout;
	bool enable_disconnect_roam_offload;
	bool enable_idle_roam;
	uint32_t idle_roam_rssi_delta;
	uint32_t idle_roam_inactive_time;
	uint32_t idle_data_packet_count;
	uint32_t idle_roam_band;
	int32_t idle_roam_min_rssi;
#endif
	uint32_t num_tx_chains_2g;
	uint32_t num_tx_chains_5g;
	uint32_t num_rx_chains_2g;
	uint32_t num_rx_chains_5g;
	uint32_t tx_nss_2g;
	uint32_t tx_nss_5g;
	uint32_t rx_nss_2g;
	uint32_t rx_nss_5g;
	uint32_t num_tx_chains_11b;
	uint32_t num_tx_chains_11g;
	uint32_t num_tx_chains_11a;
	bool disable_tx_mrc_2g;
	bool disable_rx_mrc_2g;
	bool disable_tx_mrc_5g;
	bool disable_rx_mrc_5g;
	bool enable_dynamic_nss_chains_cfg;
	bool mac_provision;
	uint32_t provisioned_intf_pool;
	uint32_t derived_intf_pool;
	bool enable_peer_unmap_conf_support;
	uint8_t enable_rtt_support;

	uint32_t roam_score_delta;
	uint32_t min_roam_score_delta;
	uint32_t roam_score_delta_bitmap;
	bool prefer_btm_query;
	bool btm_abridge_config;
	uint32_t btm_validity_timer;
	uint32_t btm_disassoc_timer_threshold;
	bool enable_bss_load_roam_trigger;
	uint32_t bss_load_threshold;
	uint32_t bss_load_sample_time;
	int32_t bss_load_trigger_rssi_threshold_5ghz;
	int32_t bss_load_trigger_rssi_threshold_24ghz;
	uint32_t roam_scan_inactivity_time;
	uint32_t roam_inactive_data_packet_count;
	uint32_t roam_scan_period_after_inactivity;
	int32_t disconnect_roam_min_rssi;
	int32_t bmiss_roam_min_rssi;
	uint32_t btm_roam_score_delta;
	uint32_t idle_roam_score_delta;

	bool enable_beacon_reception_stats;

	bool ignore_fw_reg_offload_ind;
	bool enable_pending_list_req;
	uint32_t roam_full_scan_period;
	uint16_t ndp_inactivity_timeout;
	uint8_t mgmt_retry_max;
	uint8_t enable_ring_buffer;
#ifdef WLAN_ADAPTIVE_11R
	bool enable_adaptive_11r;
#endif
#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
	bool sae_single_pmk_feature_enabled;
#endif
	uint32_t num_vdevs;
	bool ShortGI80MhzEnable;
	bool ShortGI160MhzEnable;
	uint32_t vendor_roam_score_algorithm;
	uint32_t dp_proto_event_bitmap;
	uint8_t nb_commands_interval;

#ifdef SAR_SAFETY_FEATURE
	uint32_t sar_safety_timeout;
	uint32_t sar_safety_unsolicited_timeout;
	uint32_t sar_safety_req_resp_timeout;
	uint32_t sar_safety_req_resp_retry;
	uint32_t sar_safety_index;
	uint32_t sar_safety_sleep_index;
	bool enable_sar_safety;
	bool config_sar_safety_sleep_index;
#endif
	uint32_t pkt_capture_mode;

	uint32_t mws_coex_scc_channel_avoid_delay;
	uint32_t mws_coex_pcc_channel_avoid_delay;

	bool disable_4way_hs_offload;
#ifdef FEATURE_WLAN_TIME_SYNC_FTM
	bool time_sync_ftm_enable;
	bool time_sync_ftm_mode;
	bool time_sync_ftm_role;
#endif
	uint32_t sta_disable_roam;

#ifdef WLAN_FEATURE_PERIODIC_STA_STATS
	/* Periodicity of logging */
	uint32_t periodic_stats_timer_interval;
	/* Duration for which periodic logging should be done */
	uint32_t periodic_stats_timer_duration;
#endif /* WLAN_FEATURE_PERIODIC_STA_STATS */
	uint8_t dfs_chan_ageout_time;
};

#define VAR_OFFSET(_Struct, _Var) (offsetof(_Struct, _Var))
#define VAR_SIZE(_Struct, _Var) (sizeof(((_Struct *)0)->_Var))

#define VAR_FLAGS_NONE         (0)

/* bit 0 is Required or Optional */
#define VAR_FLAGS_REQUIRED     (1 << 0)
#define VAR_FLAGS_OPTIONAL     (0 << 0)

/*
 * bit 1 tells if range checking is required.
 * If less than MIN, assume MIN.
 * If greater than MAX, assume MAX.
 */
#define VAR_FLAGS_RANGE_CHECK  (1 << 1)
#define VAR_FLAGS_RANGE_CHECK_ASSUME_MINMAX (VAR_FLAGS_RANGE_CHECK)

/*
 * bit 2 is range checking that assumes the DEFAULT value
 * If less than MIN, assume DEFAULT,
 * If greater than MAX, assume DEFAULT.
 */
#define VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT (1 << 2)

/*
 * Bit 3 indicates that the config item can be modified dynamicially
 * on a running system
 */
#define VAR_FLAGS_DYNAMIC_CFG (1 << 3)

enum wlan_parameter_type {
	WLAN_PARAM_Integer,
	WLAN_PARAM_SignedInteger,
	WLAN_PARAM_HexInteger,
	WLAN_PARAM_String,
	WLAN_PARAM_MacAddr,
};

#define REG_VARIABLE(_Name, _Type,  _Struct, _VarName,		\
		      _Flags, _Default, _Min, _Max)		\
	{							\
		(_Name),					\
		(_Type),					\
		(_Flags),					\
		VAR_OFFSET(_Struct, _VarName),			\
		VAR_SIZE(_Struct, _VarName),			\
		(_Default),					\
		(_Min),						\
		(_Max),						\
		NULL,						\
		0						\
	}

#define REG_DYNAMIC_VARIABLE(_Name, _Type,  _Struct, _VarName,	\
			      _Flags, _Default, _Min, _Max,	\
			      _CBFunc, _CBParam)		\
	{							\
		(_Name),					\
		(_Type),					\
		(VAR_FLAGS_DYNAMIC_CFG | (_Flags)),		\
		VAR_OFFSET(_Struct, _VarName),			\
		VAR_SIZE(_Struct, _VarName),			\
		(_Default),					\
		(_Min),						\
		(_Max),						\
		(_CBFunc),					\
		(_CBParam)					\
	}

#define REG_VARIABLE_STRING(_Name, _Type,  _Struct, _VarName,	\
			     _Flags, _Default)			\
	{							\
		(_Name),					\
		(_Type),					\
		(_Flags),					\
		VAR_OFFSET(_Struct, _VarName),			\
		VAR_SIZE(_Struct, _VarName),			\
		(unsigned long)(_Default),			\
		0,						\
		0,						\
		NULL,						\
		0						\
	}

struct reg_table_entry {
	char *RegName;          /* variable name in the qcom_cfg.ini file */
	enum wlan_parameter_type RegType;    /* variable type in hdd_config struct */
	unsigned long Flags;    /* Specify optional parms and if RangeCheck is performed */
	unsigned short VarOffset;       /* offset to field from the base address of the structure */
	unsigned short VarSize; /* size (in bytes) of the field */
	unsigned long VarDefault;       /* default value to use */
	unsigned long VarMin;   /* minimum value, for range checking */
	unsigned long VarMax;   /* maximum value, for range checking */
	/* Dynamic modification notifier */
	void (*pfnDynamicnotify)(struct hdd_context *hdd_ctx,
				 unsigned long notifyId);
	unsigned long notifyId; /* Dynamic modification identifier */
};

/**
 * hdd_to_csr_wmm_mode() - Utility function to convert HDD to CSR WMM mode
 *
 * @enum hdd_wmm_user_mode - hdd WMM user mode
 *
 * Return: CSR WMM mode
 */
eCsrRoamWmmUserModeType hdd_to_csr_wmm_mode(enum hdd_wmm_user_mode mode);

/* Function declarations and documenation */
QDF_STATUS hdd_parse_config_ini(struct hdd_context *hdd_ctx);

/**
 * hdd_validate_prb_req_ie_bitmap - validates user input for ie bit map
 * @hdd_ctx: the pointer to hdd context
 *
 * This function checks whether user has entered valid probe request
 * ie bitmap and also verifies vendor ouis if vendor specific ie is set
 *
 * Return: status of verification
 *         true - valid input
 *         false - invalid input
 */
bool hdd_validate_prb_req_ie_bitmap(struct hdd_context *hdd_ctx);

/**
 * hdd_parse_probe_req_ouis - form ouis from ini gProbeReqOUIs
 * @hdd_ctx: the pointer to hdd context
 *
 * This function parses the ini string gProbeReqOUIs which needs be to in the
 * following format:
 * "<8 characters of [0-9] or [A-F]>space<8 characters from [0-9] etc.,"
 * example: "AABBCCDD 1122EEFF"
 * and the logic counts the number of OUIS and allocates the memory
 * for every valid OUI and is stored in struct hdd_context
 *
 * Return: status of parsing
 *         0 - success
 *         negative value - failure
 */
int hdd_parse_probe_req_ouis(struct hdd_context *hdd_ctx);

QDF_STATUS hdd_update_mac_config(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_set_sme_config(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_set_policy_mgr_user_cfg(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_set_sme_chan_list(struct hdd_context *hdd_ctx);
bool hdd_update_config_cfg(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_cfg_get_global_config(struct hdd_context *hdd_ctx, char *pBuf,
				     int buflen);

eCsrPhyMode hdd_cfg_xlate_to_csr_phy_mode(enum hdd_dot11_mode dot11Mode);
QDF_STATUS hdd_execute_global_config_command(struct hdd_context *hdd_ctx,
					     char *command);

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

void hdd_cfg_print(struct hdd_context *hdd_ctx);

QDF_STATUS hdd_update_nss(struct hdd_adapter *adapter, uint8_t nss);

/**
 * hdd_dfs_indicate_radar() - Block tx as radar found on the channel
 * @hdd_ctxt: HDD context pointer
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

#endif
