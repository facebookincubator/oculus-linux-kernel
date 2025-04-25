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
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_MLME_GENERIC_H
#define __CFG_MLME_GENERIC_H

#define CFG_PMF_SA_QUERY_MAX_RETRIES_TYPE	CFG_INI_UINT
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL_TYPE	CFG_INI_UINT

/**
 * enum monitor_mode_concurrency - Monitor mode concurrency
 * @MONITOR_MODE_CONC_NO_SUPPORT: No concurrency supported with monitor mode
 * @MONITOR_MODE_CONC_STA_SCAN_MON: STA + monitor mode concurrency is supported
 * @MONITOR_MODE_CONC_AFTER_LAST: last value in enum
 * @MONITOR_MODE_CONC_MAX: max value supported
 */
enum monitor_mode_concurrency {
	MONITOR_MODE_CONC_NO_SUPPORT,
	MONITOR_MODE_CONC_STA_SCAN_MON,
	MONITOR_MODE_CONC_AFTER_LAST,
	MONITOR_MODE_CONC_MAX = MONITOR_MODE_CONC_AFTER_LAST - 1,
};

/**
 * enum wlan_wds_mode - wds mode
 * @WLAN_WDS_MODE_DISABLED: WDS is disabled
 * @WLAN_WDS_MODE_REPEATER: WDS repeater mode
 * @WLAN_WDS_MODE_LAST: last value in enum
 * @WLAN_WDS_MODE_MAX: max value supported
 * This is used for 'type' values in wds_mode
 */
enum wlan_wds_mode {
	WLAN_WDS_MODE_DISABLED  =  0,
	WLAN_WDS_MODE_REPEATER  =  1,
	/* keep this last */
	WLAN_WDS_MODE_LAST,
	WLAN_WDS_MODE_MAX = WLAN_WDS_MODE_LAST - 1,
};

/**
 * enum wlan_eht_mode - EHT mode of operation
 * @WLAN_EHT_MODE_DISABLED: EHT is disabled
 * @WLAN_EHT_MODE_SLO: Single-link operation mode
 * @WLAN_EHT_MODE_MLSR: Multi-link Single-Radio mode
 * @WLAN_EHT_MODE_MLMR: Multi-link Multi-Radio mode
 * @WLAN_EHT_MODE_EMLSR: Enhanced Multi-link Single-Radio mode
 * @WLAN_EHT_MODE_LAST: last value in enum
 * @WLAN_EHT_MODE_MAX: max value supported
 *
 * This is used for 'type' values in eht_mode
 */
enum wlan_eht_mode {
	WLAN_EHT_MODE_DISABLED  = 0,
	WLAN_EHT_MODE_SLO       = 1,
	WLAN_EHT_MODE_MLSR      = 2,
	WLAN_EHT_MODE_MLMR      = 3,
	WLAN_EHT_MODE_EMLSR     = 4,
	/* keep this last */
	WLAN_EHT_MODE_LAST,
	WLAN_EHT_MODE_MAX = WLAN_EHT_MODE_LAST - 1,
};

/**
 * enum wlan_emlsr_action_mode - EMLSR action mode
 * @WLAN_EMLSR_MODE_DISABLED: EMLSR is disabled
 * @WLAN_EMLSR_MODE_ENTER: Enter EMLSR operation mode
 * @WLAN_EMLSR_MODE_EXIT: Exit EMLSR operation mode
 * @WLAN_EMLSR_MODE_LAST: last value in enum
 * @WLAN_EMLSR_MODE_MAX: max value supported
 *
 * This is used for 'type' values in emlsr_mode
 */
enum wlan_emlsr_action_mode {
	WLAN_EMLSR_MODE_DISABLED = 0,
	WLAN_EMLSR_MODE_ENTER    = 1,
	WLAN_EMLSR_MODE_EXIT     = 2,
	/* keep this last */
	WLAN_EMLSR_MODE_LAST,
	WLAN_EMLSR_MODE_MAX = WLAN_EMLSR_MODE_LAST - 1,
};

/**
 * enum wlan_t2lm_negotiation_support - TID-to-link mapping negotiation support
 * @WLAN_T2LM_DISABLE: T2LM support is disabled
 * @WLAN_T2LM_SAME_LINK_SET: Mapping of all TIDs to the same link set, both DL
 * and UL
 * @WLAN_T2LM_RESERVED: This value is Reserved
 * @WLAN_T2LM_SAME_DIFF_LINK_SET: Mapping of each TID to the same or different
 * link set
 * @WLAN_T2LM_SUPPORT_LAST: last value in enum
 * @WLAN_T2LM_SUPPORT_MAX: max value supported
 *
 * This is used for 'type' values in T2LM support
 */
enum wlan_t2lm_negotiation_support {
	WLAN_T2LM_DISABLE            = 0,
	WLAN_T2LM_SAME_LINK_SET      = 1,
	WLAN_T2LM_RESERVED           = 2,
	WLAN_T2LM_SAME_DIFF_LINK_SET = 3,
	/* keep this last */
	WLAN_T2LM_SUPPORT_LAST,
	WLAN_T2LM_SUPPORT_MAX = WLAN_T2LM_SUPPORT_LAST - 1,
};

/**
 * enum debug_packet_log_type - Debug packet log type
 * @DEBUG_PKTLOG_TYPE_NONE: Debug packet log is disabled
 * @DEBUG_PKTLOG_TYPE_MGMT: Management frames logging is enabled.
 * @DEBUG_PKTLOG_TYPE_EAPOL: EAPOL packets logging is enabled.
 * @DEBUG_PKTLOG_TYPE_DHCP: DHCP packets logging is enabled.
 * @DEBUG_PKTLOG_TYPE_ACTION: Action frames logging is enabled.
 * @DEBUG_PKTLOG_TYPE_ARP: ARP packets logging is enabled.
 */
enum debug_packet_log_type {
	DEBUG_PKTLOG_TYPE_NONE   = 0x0,
	DEBUG_PKTLOG_TYPE_MGMT   = 0x1,
	DEBUG_PKTLOG_TYPE_EAPOL  = 0x2,
	DEBUG_PKTLOG_TYPE_DHCP   = 0x4,
	DEBUG_PKTLOG_TYPE_ACTION = 0x8,
	DEBUG_PKTLOG_TYPE_ARP    = 0x10,
};

/**
 * enum t2lm_negotiation_support: t2lm negotiation supported
 * @T2LM_NEGOTIATION_DISABLED: T2LM is disabled
 * @T2LM_NEGOTIATION_ALL_TIDS_TO_SUBSET_OF_LINKS: supports the mapping
 * of all TIDs to the same link set both DL and UL.
 * @T2LM_NEGOTIATION_RESERVED:
 * this mapping value is reserved.
 * @T2LM_NEGOTIATION_DISJOINT_MAPPING: supports the mapping of
 * each TID to the same or different link set.
 * @T2LM_NEGOTIATION_LAST: last value in enum
 * @T2LM_NEGOTIATION_MAX: max value supported
 */
enum t2lm_negotiation_support {
	T2LM_NEGOTIATION_DISABLED = 0,
	T2LM_NEGOTIATION_ALL_TIDS_TO_SUBSET_OF_LINKS = 1,
	T2LM_NEGOTIATION_RESERVED = 2,
	T2LM_NEGOTIATION_DISJOINT_MAPPING  = 3,
	T2LM_NEGOTIATION_LAST,
	/* keep this last */
	T2LM_NEGOTIATION_MAX = T2LM_NEGOTIATION_LAST - 1,
};

/**
 * enum wlan_epcs_capability - EPCS capability
 * @WLAN_EPCS_CAP_DISABLED: EPCS capability disable
 * @WLAN_EPCS_CAP_ENABLE: EPCS capability enable
 * @WLAN_EPCS_CAP_LAST: last value in enum
 * @WLAN_EPCS_CAP_MAX: max value supported
 */
enum wlan_epcs_capability {
	WLAN_EPCS_CAP_DISABLED  =  0,
	WLAN_EPCS_CAP_ENABLE  =  1,
	/* keep this last */
	WLAN_EPCS_CAP_LAST,
	WLAN_EPCS_CAP_MAX = WLAN_EPCS_CAP_LAST - 1,
};

/**
 * enum wlan_epcs_frame - EPCS frame type
 * @WLAN_EPCS_FRAME_TEARDOWN: EPCS teardown frame
 * @WLAN_EPCS_FRAME_REQUEST: EPCS request frame
 * @WLAN_EPCS_FRAME_LAST: last value in enum
 * @WLAN_EPCS_FRAME_MAX: max value supported
 */
enum wlan_epcs_frame {
	WLAN_EPCS_FRAME_TEARDOWN  =  0,
	WLAN_EPCS_FRAME_REQUEST  =  1,
	/* keep this last */
	WLAN_EPCS_FRAME_LAST,
	WLAN_EPCS_FRAME_MAX = WLAN_EPCS_FRAME_LAST - 1,
};

/*
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
 */
#define CFG_PMF_SA_QUERY_MAX_RETRIES CFG_PMF_SA_QUERY_MAX_RETRIES_TYPE( \
		"pmfSaQueryMaxRetries", \
		0, \
		20, \
		5, \
		CFG_VALUE_OR_DEFAULT, \
		"PMF SA query retries for SAP")
/*
 * pmfSaQueryRetryInterval - Control PMF SA query retry interval
 * for SAP in ms
 * @Min: 10
 * @Max: 2000
 * @Default: 200
 *
 * This ini to set the PMF SA query retry interval for SAP in ms
 *
 * Related: None.
 *
 * Supported Feature: PMF(11W)
 *
 */
#define CFG_PMF_SA_QUERY_RETRY_INTERVAL CFG_PMF_SA_QUERY_RETRY_INTERVAL_TYPE( \
		"pmfSaQueryRetryInterval", \
		10, \
		2000, \
		200, \
		CFG_VALUE_OR_DEFAULT, \
		"PMF SA query retry interval for SAP")

#ifdef WLAN_FEATURE_11BE
/*
 * oem_eht_mlo_crypto_bitmap - OEM control to allow various EHT connection
 * options using bitmap based on following ENUM (Name of ENUM to be added)
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x0
 *
 * This INI is used to control the driver candidate selection and EHT
 * connection choice based on OEM configuration. The bitmap follows the
 * implementation from this ENUM (name of the ENUM to be added)
 */
#define CFG_OEM_EHT_MLO_CRYPTO_BITMAP CFG_INI_UINT( \
		"oem_eht_mlo_crypto_bitmap", \
		0x0, \
		0xFFFFFFFF, \
		0x0, \
		CFG_VALUE_OR_DEFAULT, \
		"OEM control to allow/disallow crypto to EHT configuration")

#define CFG_OEM_EHT_MLO_CRYPTO_BITMAP_SUPPORTED \
			CFG(CFG_OEM_EHT_MLO_CRYPTO_BITMAP)
#else
#define CFG_OEM_EHT_MLO_CRYPTO_BITMAP_SUPPORTED
#endif

/*
 * <ini>
 * enable_rtt_mac_randomization - Enable/Disable rtt mac randomization
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_RTT_MAC_RANDOMIZATION CFG_INI_BOOL( \
	"enable_rtt_mac_randomization", \
	1, \
	"Enable RTT MAC randomization")

#define CFG_RTT3_ENABLE CFG_BOOL( \
		"rtt3_enabled", \
		1, \
		"RTT3 enable/disable info")

/*
 * <ini>
 * g11hSupportEnabled - Enable 11h support
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set 11h support flag
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_11H_SUPPORT_ENABLED CFG_INI_BOOL( \
		"g11hSupportEnabled", \
		1, \
		"11h Enable Flag")

/*
 * <ini>
 * g11dSupportEnabled - Enable 11d support
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set 11d support flag
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_11D_SUPPORT_ENABLED CFG_INI_BOOL( \
		"g11dSupportEnabled", \
		1, \
		"11d Enable Flag")

/*
 * rf_test_mode_enabled - Enable rf test mode support
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This cfg is used to set rf test mode support flag
 * by default 6 G Hz security check will be enabled
 * with rf test mode as disabled.
 *
 * Related: None
 *
 * Supported Feature: STA
 */
#define CFG_RF_TEST_MODE_SUPP_ENABLED CFG_BOOL( \
		"rf_test_mode_enabled", \
		0, \
		"rf test mode Enable Flag")

#ifdef CONFIG_BAND_6GHZ
/*
 * disable_vlp_sta_conn_to_sp_ap - Disable VLP STA connection to SP AP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This cfg is used to disable connection when AP is operating in 6 GHz
 * SP mode but STA doesn't support SP mode and supports VLP mode.
 *
 * Related: None
 *
 * Supported Feature: STA
 */
#define CFG_DISABLE_VLP_STA_CONN_TO_SP_AP CFG_BOOL( \
		"disable_vlp_sta_conn_to_sp_ap", \
		0, \
		"disable vlp sta conn to sp ap")
#define CFG_DIS_VLP_STA_CONN_TO_SP_AP	CFG(CFG_DISABLE_VLP_STA_CONN_TO_SP_AP)
#else
#define CFG_DIS_VLP_STA_CONN_TO_SP_AP
#endif

#ifdef CONFIG_BAND_6GHZ
/*
 * standard_6ghz_connection_policy - Enable 6 GHz standard connection policy
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set standard 6 GHz policies where STA will be
 * allowed to scan and connect to any 6 GHz AP.
 *
 * Related: None
 *
 * Supported Feature: STA
 */
#define CFG_6GHZ_STANDARD_CONNECTION_POLICY CFG_INI_BOOL( \
		"standard_6ghz_connection_policy", \
		1, \
		"6ghz standard 6 GHZ connection policy")
#define CFG_6GHZ_STD_CONN_POLICY	CFG(CFG_6GHZ_STANDARD_CONNECTION_POLICY)
#else
#define CFG_6GHZ_STD_CONN_POLICY
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/*
 * emlsr_mode_enable - Enable eMLSR mode support
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This cfg is used to enable eMLSR mode
 * If 0 - MLMR mode (Default mode)
 * If 1 - eMLSR mode
 *
 * Related: None
 *
 * Supported Feature: STA
 */
#define CFG_EMLSR_MODE_ENABLE CFG_BOOL( \
		"emlsr_mode_enable", \
		0, \
		"eMLSR mode enable flag")
#define CFG_EMLSR_MODE_ENABLED	CFG(CFG_EMLSR_MODE_ENABLE)
#else
#define CFG_EMLSR_MODE_ENABLED
#endif

/*
 * <ini>
 * BandCapability - Preferred band (0: 2.4G, 5G, and 6G,
 *				    1: 2.4G only,
 *				    2: 5G only,
 *				    3: Both 2.4G and 5G,
 *				    4: 6G only,
 *				    5: Both 2.4G and 6G,
 *				    6: Both 5G and 6G,
 *				    7: 2.4G, 5G, and 6G)
 * @Min: 0
 * @Max: 7
 * @Default: 7
 *
 * This ini is used to set default band capability
 * (0: Both 2.4G and 5G, 1: 2.4G only, 2: 5G only, 3: Both 2.4G and 5G,
 *  4: 6G only, 5: Both 2.4G and 6G, 6: Both 5G and 6G, 7: 2.4G, 5G, and 6G)
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAND_CAPABILITY CFG_INI_UINT( \
	"BandCapability", \
	0, \
	7, \
	7, \
	CFG_VALUE_OR_DEFAULT, \
	"Band Capability")

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
#if defined(QCA_WIFI_EMULATION) || defined(QCA_WIFI_QCA6290)
#define CFG_PREVENT_LINK_DOWN CFG_INI_BOOL( \
	"gPreventLinkDown", \
	1, \
	"Prevent Bus Link Down")
#else
#define CFG_PREVENT_LINK_DOWN CFG_INI_BOOL( \
	"gPreventLinkDown", \
	0, \
	"Prevent Bus Link Down")
#endif /* QCA_WIFI_EMULATION */

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
#define CFG_SELECT_5GHZ_MARGIN CFG_INI_UINT( \
	"gSelect5GHzMargin", \
	0, \
	60, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Select 5Ghz Margin")

/*
 * <ini>
 * gEnableMemDeepSleep - Sets Memory Deep Sleep on/off.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This option enables/disables memory deep sleep.
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_MEM_DEEP_SLEEP CFG_INI_BOOL( \
	"gEnableMemDeepSleep", \
	1, \
	"Enable Memory Deep Sleep")

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
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_CCK_TX_FIR_OVERRIDE CFG_INI_BOOL( \
	"gEnableCckTxFirOverride", \
	0, \
	"Enable CCK TX FIR Override")

/*
 * <ini>
 *
 * gEnableForceTargetAssert - Enable/disable SSR
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 0 (disabled)
 *
 * This INI item is used to control subsystem restart(SSR) test framework
 * Set it's value to 1 to enable APPS triggered SSR testing
 *
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_CRASH_INJECT CFG_INI_BOOL( \
	"gEnableForceTargetAssert", \
	0, \
	"Enable Crash Inject")

/*
 * <ini>
 *
 * gEnableLpassSupport - Enable/disable LPASS Support
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 1 (disabled) if WLAN_FEATURE_LPSS is defined, 0 otherwise
 *
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#ifdef WLAN_FEATURE_LPSS
#define CFG_ENABLE_LPASS_SUPPORT CFG_INI_BOOL( \
	"gEnableLpassSupport", \
	1, \
	"Enable LPASS Support")
#else
#define CFG_ENABLE_LPASS_SUPPORT CFG_BOOL( \
	"gEnableLpassSupport", \
	0, \
	"Enable LPASS Support")
#endif

/*
 * <ini>
 *
 * gEnableSelfRecovery - Enable/disable Self Recovery
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 0 (disabled)
 *
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SELF_RECOVERY CFG_INI_BOOL( \
	"gEnableSelfRecovery", \
	0, \
	"Enable Self Recovery")

/*
 * <ini>
 *
 * gSapDot11mc - Enable/disable SAP 802.11mc support
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 0 (disabled)
 *
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_DOT11MC CFG_INI_BOOL( \
	"gSapDot11mc", \
	0, \
	"SAP 802.11mc support")

/*
 * <ini>
 *
 * gEnableFatalEvent - Enable/Disable BUG report in case of fatal event
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 1 (enabled)
 *
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_FATAL_EVENT_TRIGGER CFG_INI_BOOL( \
	"gEnableFatalEvent", \
	1, \
	"Enable Fatal Event Trigger")

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
 * Usage: External
 *
 * </ini>
 */
#define CFG_SUB_20_CHANNEL_WIDTH CFG_INI_UINT( \
	"gSub20ChannelWidth", \
	0, \
	2, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Sub 20 Channel Width")

/*
 * <ini>
 * goptimize_chan_avoid_event - Optimize channel avoidance indication
 *				coming from firmware
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OPTIMIZE_CA_EVENT CFG_INI_BOOL( \
	"goptimize_chan_avoid_event", \
	0, \
	"Optimize FW CA Event")

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
 * Usage: External
 *
 * </ini>
 */
#define CFG_CRASH_FW_TIMEOUT CFG_INI_BOOL( \
	"fw_timeout_crash", \
	1, \
	"Enable FW Timeout Crash")

/*
 * <ini>
 * gDroppedPktDisconnectTh - Sets dropped packet threshold in firmware
 * @Min: 0
 * @Max: 65535
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
#define CFG_DROPPED_PKT_DISCONNECT_THRESHOLD CFG_INI_UINT( \
	"gDroppedPktDisconnectTh", \
	0, \
	65535, \
	512, \
	CFG_VALUE_OR_DEFAULT, \
	"Dropped Pkt Disconnect threshold")

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
#define CFG_ITO_REPEAT_COUNT CFG_INI_UINT( \
	"gItoRepeatCount", \
	0, \
	5, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"ITO Repeat Count")

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
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DEAUTH_TO_DISASSOC_MAP CFG_INI_BOOL( \
		"gEnableDeauthToDisassocMap", \
		0, \
		"Enables deauth to disassoc map")

/*
 * <ini>
 * gEnableDebugLog - Enable/Disable the Connection related logs
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0x01
 *
 * This ini is used to enable/disable the connection related logs
 * 0x1  - Enable mgmt pkt logs (except probe req/rsp, beacons).
 * 0x2  - Enable EAPOL pkt logs.
 * 0x4  - Enable DHCP pkt logs.
 * 0x8  - Enable mgmt action frames logs.
 * 0x10 - Enable ARP pkt logs.
 * 0x0  - Disable all the above connection related logs.
 * The default value of 0x01 will enable all the mgmt logs
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DEBUG_PACKET_LOG CFG_INI_UINT( \
				"gEnableDebugLog", \
				0, 0xFF, 0x01, \
				CFG_VALUE_OR_DEFAULT, \
				"Enable debug log")

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
 * Usage: External
 *
 * </ini>
 */
 #define CFG_ENABLE_BEACON_RECEPTION_STATS CFG_INI_BOOL( \
			"enable_beacon_reception_stats", \
			0, \
			"Enable disable beacon reception stats")

/*
 * <ini>
 * disable_4way_hs_offload - Enable/Disable 4 way handshake offload to firmware
 * @Min: 0
 * @Max: 0x2
 * @Default: 0x2
 *
 * 0x0 - 4-way HS to be handled in firmware for the AKMs except for SAE and
 * OWE roaming the 4way HS is handled in supplicant by default
 * 0x1 - 4-way HS to be handled in supplicant
 * 0x2 - 4-way HS to be handled in firmware for the AKMs including the SAE
 * Roam except for OWE roaming the 4way HS is handled in supplicant
 *
 * Based on the requirement the Max value can be increased per AKM.
 *
 * Related: None
 *
 * Supported Feature: STA Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_4WAY_HS_OFFLOAD CFG_INI_UINT( \
		"disable_4way_hs_offload", \
		0, \
		0x2, \
		0x2, \
		CFG_VALUE_OR_DEFAULT, \
		"Enable/disable 4 way handshake offload to firmware")

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
#define CFG_MGMT_RETRY_MAX CFG_INI_UINT( \
	"mgmt_retry_max", \
	0, \
	31, \
	15, \
	CFG_VALUE_OR_DEFAULT, \
	"Max retries for mgmt frames")

/*
 * <ini>
 * enable_he_mcs0_for_mgmt_6ghz- if disabled FW will use 6Mbps 11A rate
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * If this ini is disabled firmware will use 6Mbps 11A rate
 *
 * Supported Feature: STA/SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_HE_MCS0_MGMT_6GHZ CFG_INI_BOOL( \
	"enable_he_mcs0_for_mgmt_6ghz", \
	0, \
	"MCS0 rate for 6ghz mgmt frames")

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
 * Supported Feature: STA Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BMISS_SKIP_FULL_SCAN CFG_INI_BOOL("bmiss_skip_full_scan", \
			0, \
			"To decide partial/partial scan followed by full scan")

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
#define CFG_ENABLE_RING_BUFFER CFG_INI_BOOL( \
		"gEnableRingBuffer", \
		1, \
		"To Enable Ring Buffer")

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
#define CFG_DFS_CHAN_AGEOUT_TIME CFG_INI_UINT("dfs_chan_ageout_time", \
			0, 8, 0, CFG_VALUE_OR_DEFAULT, \
			"Set DFS Channel ageout time from host to firmware")

/*
 * <ini>
 * sae_connect_retries - Bit mask to retry Auth and full connection on assoc
 * timeout to same AP and auth retries during roaming
 * @Min: 0x0
 * @Max: 0x53
 * @Default: 0x52
 *
 * This ini is used to set max auth retry in auth phase of roaming and initial
 * connection and max connection retry in case of assoc timeout. MAX Auth
 * retries are capped to 3, connection retries are capped to 2 and roam Auth
 * retry is capped to 1.
 * Default is 0x52 i.e. 1 roam auth retry, 2 auth retry and 2 full connection
 * retry.
 *
 * Bits       Retry Type
 * BIT[0:2]   AUTH retries
 * BIT[3:5]   Connection reties
 * BIT[6:8]   ROAM AUTH retries
 *
 * Some Possible values are as below
 * 0          - NO auth/roam Auth retry and NO full connection retry after
 *              assoc timeout
 * 0x49       - 1 auth/roam auth retry and 1 full connection retry
 * 0x52       - 1 roam auth retry, 2 auth retry and 2 full connection retry
 * 0x1 /0x2   - 0 roam auth retry, 1 or 2 auth retry respectively and NO full
 *              connection retry
 * 0x8 /0x10  - 0 roam auth retry,NO auth retry and 1 or 2 full connection retry
 *              respectively.
 * 0x4A       - 1 roam auth retry,2 auth retry and 1 full connection retry
 * 0x51       - 1 auth/roam auth retry and 2 full connection retry
 *
 * Related: None
 *
 * Supported Feature: STA SAE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAE_CONNECION_RETRIES CFG_INI_UINT("sae_connect_retries", \
				0, 0x53, 0x52, CFG_VALUE_OR_DEFAULT, \
				"Bit mask to retry Auth and full connection on assoc timeout to same AP for SAE connection")

/*
 * <ini>
 *
 * wls_6ghz_capable - WiFi Location Service(WLS) is 6Ghz capable
 * @Min: 0 (WLS 6Ghz non-capable)
 * @Max: 1 (WLS 6Ghz capable)
 * @Default: 0 (WLS 6Ghz non-capable)
 *
 * Related: None
 *
 * Supported Feature: General
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_WLS_6GHZ_CAPABLE CFG_INI_BOOL( \
	"wls_6ghz_capable", \
	0, \
	"WiFi Location Service(WLS) is 6Ghz capable or not")

/*
 * <ini>
 *
 * monitor_mode_conc - Monitor mode concurrency supported
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: None
 *
 * Monitor mode concurrency supported
 * 0 - No concurrency supported
 * 1 - Allow STA scan + Monitor mode concurrency
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MONITOR_MODE_CONCURRENCY CFG_INI_UINT( \
	"monitor_mode_concurrency", \
	MONITOR_MODE_CONC_NO_SUPPORT, \
	MONITOR_MODE_CONC_MAX, \
	MONITOR_MODE_CONC_NO_SUPPORT, \
	CFG_VALUE_OR_DEFAULT, \
	"Monitor mode concurrency supported")

#ifdef FEATURE_WDS
/*
 * <ini>
 *
 * wds_mode - wds mode supported
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: None
 *
 * wds mode supported
 * 0 - wds mode disabled
 * 1 - wds repeater mode
 *
 * Supported Feature: General
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_WDS_MODE CFG_INI_UINT( \
	"wds_mode", \
	WLAN_WDS_MODE_DISABLED, \
	WLAN_WDS_MODE_MAX, \
	WLAN_WDS_MODE_DISABLED, \
	CFG_VALUE_OR_DEFAULT, \
	"wds mode supported")

#define CFG_WDS_MODE_ALL CFG(CFG_WDS_MODE)
#else
#define CFG_WDS_MODE_ALL
#endif

/*
 * <ini>
 * tx_retry_multiplier - TX retry multiplier
 * @Min: 0
 * @Max: 500
 * @Default: 0
 *
 * This ini is used to indicate percentage to max retry limit to fw
 * which can further be used by fw to multiply counter by
 * tx_retry_multiplier percent.
 *
 * Supported Feature: STA/SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_RETRY_MULTIPLIER CFG_INI_UINT( \
	"tx_retry_multiplier", \
	0, \
	500, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"percentage of max retry limit")

/*
 * <ini>
 * mgmt_frame_hw_tx_retry_count - Set hw tx retry count for mgmt action
 * frame
 * @Min: N/A
 * @Max: N/A
 * @Default: N/A
 *
 * Set mgmt action frame hw tx retry count, string format looks like below:
 * frame_hw_tx_retry_count="<frame type>,<retry count>,..."
 * frame type is enum value of mlme_cfg_frame_type.
 * Retry count max value is 127.
 * For example:
 * frame_hw_tx_retry_count="0,64,2,32"
 * The above input string means:
 * For p2p go negotiation request fame, hw retry count 64
 * For p2p provision discovery request, hw retry count 32
 *
 * Related: None.
 *
 * Supported Feature: STA/P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define MGMT_FRM_HW_TX_RETRY_COUNT_STR_LEN  (64)
#define CFG_MGMT_FRAME_HW_TX_RETRY_COUNT CFG_INI_STRING( \
		"mgmt_frame_hw_tx_retry_count", \
		0, \
		MGMT_FRM_HW_TX_RETRY_COUNT_STR_LEN, \
		"", \
		"Set mgmt action frame hw tx retry count")

#if defined(WLAN_FEATURE_SR)
/*
 * <ini>
 * sr_enable_modes - Modes for which SR(Spatial Reuse) feature can be enabled
 * @Min: 0x00
 * @Max: 0xf
 * @Default: 0x1
 *
 * This ini is used to check for which mode SR feature is enabled
 *
 * Bit 0: Enable/Disable SR feature for STA
 * Bit 1: Enable/Disable SR feature for SAP
 * Bit 2: Enable/Disable SR feature for P2P CLI
 * Bit 3: Enable/Disable SR feature for P2P GO
 *
 * Related: None
 *
 * Supported Feature: STA/SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SR_ENABLE_MODES CFG_INI_UINT( \
		"sr_enable_modes",\
		0x0,\
		0xf,\
		0x1,\
		CFG_VALUE_OR_DEFAULT, \
		"To decide for which mode SR feature is enabled")
#define CFG_SR_ENABLE_MODES_ALL CFG(CFG_SR_ENABLE_MODES)
#else
#define CFG_SR_ENABLE_MODES_ALL
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/*
 * t2lm_negotiation_support - T2LM negotiation support by STA
 * @Min: 0
 * @Max: 3
 * @Default: 1
 *
 * This cfg is used to define t2lm negotiation supported value by STA
 * If 0 - t2lm negotiation is not supported
 * If 1 - supports the mapping of all TIDs to the same link set both DL and UL.
 * If 2 - reserved
 * If 3 - supports the mapping of each TID to the same or different link set.
 *
 * Related: None
 *
 * Supported Feature: STA
 */
#define CFG_T2LM_NEGOTIATION_SUPPORT CFG_INI_UINT( \
					"t2lm_negotiation_supported", \
					T2LM_NEGOTIATION_DISABLED, \
					T2LM_NEGOTIATION_DISJOINT_MAPPING, \
					T2LM_NEGOTIATION_ALL_TIDS_TO_SUBSET_OF_LINKS, \
					CFG_VALUE_OR_DEFAULT, \
					"T2LM negotiation supported value")

#define CFG_T2LM_NEGOTIATION_SUPPORTED CFG(CFG_T2LM_NEGOTIATION_SUPPORT)
#else
#define CFG_T2LM_NEGOTIATION_SUPPORTED
#endif

#define CFG_GENERIC_ALL \
	CFG(CFG_ENABLE_DEBUG_PACKET_LOG) \
	CFG(CFG_PMF_SA_QUERY_MAX_RETRIES) \
	CFG(CFG_PMF_SA_QUERY_RETRY_INTERVAL) \
	CFG_OEM_EHT_MLO_CRYPTO_BITMAP_SUPPORTED \
	CFG(CFG_ENABLE_RTT_MAC_RANDOMIZATION) \
	CFG(CFG_RTT3_ENABLE) \
	CFG(CFG_11H_SUPPORT_ENABLED) \
	CFG(CFG_11D_SUPPORT_ENABLED) \
	CFG(CFG_BAND_CAPABILITY) \
	CFG(CFG_PREVENT_LINK_DOWN) \
	CFG(CFG_SELECT_5GHZ_MARGIN) \
	CFG(CFG_ENABLE_MEM_DEEP_SLEEP) \
	CFG(CFG_ENABLE_CCK_TX_FIR_OVERRIDE) \
	CFG(CFG_ENABLE_CRASH_INJECT) \
	CFG(CFG_ENABLE_LPASS_SUPPORT) \
	CFG(CFG_ENABLE_SELF_RECOVERY) \
	CFG(CFG_ENABLE_DEAUTH_TO_DISASSOC_MAP) \
	CFG(CFG_DISABLE_4WAY_HS_OFFLOAD) \
	CFG(CFG_SAP_DOT11MC) \
	CFG(CFG_ENABLE_FATAL_EVENT_TRIGGER) \
	CFG(CFG_SUB_20_CHANNEL_WIDTH) \
	CFG(CFG_OPTIMIZE_CA_EVENT) \
	CFG(CFG_CRASH_FW_TIMEOUT) \
	CFG(CFG_DROPPED_PKT_DISCONNECT_THRESHOLD) \
	CFG(CFG_ITO_REPEAT_COUNT) \
	CFG(CFG_ENABLE_BEACON_RECEPTION_STATS) \
	CFG(CFG_MGMT_RETRY_MAX) \
	CFG(CFG_ENABLE_HE_MCS0_MGMT_6GHZ) \
	CFG(CFG_BMISS_SKIP_FULL_SCAN) \
	CFG(CFG_ENABLE_RING_BUFFER) \
	CFG(CFG_DFS_CHAN_AGEOUT_TIME) \
	CFG(CFG_SAE_CONNECION_RETRIES) \
	CFG(CFG_WLS_6GHZ_CAPABLE) \
	CFG(CFG_MONITOR_MODE_CONCURRENCY) \
	CFG(CFG_RF_TEST_MODE_SUPP_ENABLED) \
	CFG_WDS_MODE_ALL \
	CFG(CFG_TX_RETRY_MULTIPLIER) \
	CFG(CFG_MGMT_FRAME_HW_TX_RETRY_COUNT) \
	CFG_6GHZ_STD_CONN_POLICY \
	CFG_EMLSR_MODE_ENABLED \
	CFG_SR_ENABLE_MODES_ALL \
	CFG_T2LM_NEGOTIATION_SUPPORTED\
	CFG_DIS_VLP_STA_CONN_TO_SP_AP
#endif /* __CFG_MLME_GENERIC_H */
