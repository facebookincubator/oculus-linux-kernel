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

#ifndef __HDD_CONFIG_H
#define __HDD_CONFIG_H

#include "hdd_sar_safety_config.h"

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif

#define CFG_QDF_TRACE_ENABLE_DEFAULT (0xffff)
#include <wlan_action_oui_public_struct.h>

/**
 * enum hdd_wext_control - knob for wireless extensions
 * @hdd_wext_disabled: interface is completely disabled. An access
 *      control error log will be generated for each attempted use.
 * @hdd_wext_deprecated: interface is available but should not be
 *      used. An access control warning log will be generated for each
 *      use.
 * @hdd_wext_enabled: interface is available without restriction. No
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
 * This ini is used to control access to private wireless extensions
 * ioctls SIOCIWFIRSTPRIV (0x8BE0) thru SIOCIWLASTPRIV (0x8BFF). The
 * functionality provided by some of these ioctls has been superseded
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
#define CFG_PRIVATE_WEXT_CONTROL CFG_INI_UINT( \
			"private_wext_control", \
			hdd_wext_disabled, \
			hdd_wext_enabled, \
			hdd_wext_deprecated, \
			CFG_VALUE_OR_DEFAULT, \
			"Private WEXT Control")

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
#ifdef WLAN_FEATURE_11BE
	eHDD_DOT11_MODE_11be,
	eHDD_DOT11_MODE_11be_ONLY,
#endif
};

/*
 * <ini>
 * gDot11Mode - Phymode of vdev
 * @Min: 0 (auto)
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
 #define CFG_HDD_DOT11_MODE CFG_INI_UINT( \
			"gDot11Mode", \
			eHDD_DOT11_MODE_AUTO, \
			eHDD_DOT11_MODE_11ax, \
			eHDD_DOT11_MODE_11ax, \
			CFG_VALUE_OR_DEFAULT, \
			"dot11 mode")

#ifdef FEATURE_SET
  /*
   * <ini>
   * get_wifi_features  - Get wifi features info from fw
   * @Min: 0
   * @Max: 1
   * @Default: 0
   *
   * This ini is used to enable feature to get wifi supported features from fw
   *
   * Related: None
   *
   * Supported Feature: All
   *
   * Usage: External
   *
   * </ini>
   */
#define CFG_GET_WIFI_FEATURES CFG_INI_BOOL( \
		"get_wifi_features", \
		0, \
		"Get wifi features")
#define CFG_GET_WIFI_FEATURES_ALL CFG(CFG_GET_WIFI_FEATURES)
#else
#define CFG_GET_WIFI_FEATURES_ALL
#endif

#ifdef FEATURE_RUNTIME_PM
/*
 * <ini>
 * cpu_cxpc_threshold - PM QOS threshold
 * @Min: 0
 * @Max: 15000
 * @Default: 10000
 *
 * This ini is used to set PM QOS threshold value
 *
 * Related: None.
 *
 * Supported Feature: ALL
 *
 * Usage: External
 *
 * </ini>
 */
 #define CFG_CPU_CXPC_THRESHOLD CFG_INI_UINT( \
			"cpu_cxpc_threshold", \
			0, \
			15000, \
			10000, \
			CFG_VALUE_OR_DEFAULT, \
			"PM QOS threshold")
#define CFG_CPU_CXPC_THRESHOLD_ALL CFG(CFG_CPU_CXPC_THRESHOLD)
#else
#define CFG_CPU_CXPC_THRESHOLD_ALL
#endif

#ifdef QCA_WIFI_EMULATION
#define CFG_INTERFACE_CHANGE_WAIT_DEFAULT	300000
#else
#ifdef SHUTDOWN_WLAN_IN_SYSTEM_SUSPEND
#define CFG_INTERFACE_CHANGE_WAIT_DEFAULT	10000
#else
#define CFG_INTERFACE_CHANGE_WAIT_DEFAULT	250
#endif
#endif

/*
 * <ini>
 * gInterfaceChangeWait - Interface change wait
 * @Min: 0,
 * @Max: 500000
 * @Default: 10000 (300000 for emulation)
 *
 * Timer waiting for interface up from the upper layer. If
 * this timer expires all the cds modules shall be closed.
 * Time Units: ms
 *
 * Value 0 can be used to disable idle module stop.
 *
 * Related: None
 *
 * Supported Feature: All
 *
 * </ini>
 */
#define CFG_INTERFACE_CHANGE_WAIT CFG_INI_UINT( \
			"gInterfaceChangeWait", \
			0, \
			500000, \
			CFG_INTERFACE_CHANGE_WAIT_DEFAULT, \
			CFG_VALUE_OR_DEFAULT, \
			"Interface change wait")

#ifdef QCA_WIFI_EMULATION
#define CFG_TIMER_MULTIPLIER_DEFAULT	100
#else
#define CFG_TIMER_MULTIPLIER_DEFAULT	1
#endif

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
 * Related: None
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TIMER_MULTIPLIER CFG_INI_UINT( \
			"gTimerMultiplier", \
			1, \
			0xFFFFFFFF, \
			CFG_TIMER_MULTIPLIER_DEFAULT, \
			CFG_VALUE_OR_DEFAULT, \
			"Timer Multiplier")

#define CFG_BUG_ON_REINIT_FAILURE_DEFAULT 0
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
#define CFG_BUG_ON_REINIT_FAILURE CFG_INI_BOOL( \
		"g_bug_on_reinit_failure", \
		CFG_BUG_ON_REINIT_FAILURE_DEFAULT, \
		"BUG on reinit failure")

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
#define CFG_ENABLE_RAMDUMP_COLLECTION CFG_INI_BOOL( \
			"gEnableDumpCollect", \
			1, \
			"Enable dump collect")

#if defined(MDM_PLATFORM) && !defined(FEATURE_MULTICAST_HOST_FW_MSGS)
#define CFG_MULTICAST_HOST_FW_MSGS_DEFAULT	0
#else
#define CFG_MULTICAST_HOST_FW_MSGS_DEFAULT	1
#endif

/*
 * <ini>
 * gMulticastHostFwMsgs - Multicast host FW messages
 * @Min: 0
 * @Max: 1
 * @Default: 0 for MDM platform and 1 for other
 *
 * </ini>
 */
#define CFG_MULTICAST_HOST_FW_MSGS CFG_INI_UINT( \
			"gMulticastHostFwMsgs", \
			0, \
			1, \
			CFG_MULTICAST_HOST_FW_MSGS_DEFAULT, \
			CFG_VALUE_OR_DEFAULT, \
			"Multicast host FW msgs")

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
/*
 * <ini>
 * wlanLoggingEnable - Wlan logging enable
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * </ini>
 */
#define CFG_WLAN_LOGGING_SUPPORT CFG_INI_BOOL( \
				"wlanLoggingEnable", \
				1, \
				"Wlan logging enable")

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
#define CFG_HOST_LOG_CUSTOM_NETLINK_PROTO CFG_INI_UINT( \
	"host_log_custom_nl_proto", \
	0, \
	32, \
	2, \
	CFG_VALUE_OR_DEFAULT, \
	"host log custom netlink protocol")

/*
 * <ini>
 * wlanConsoleLogLevelsBitmap - Bitmap to enable/disable console log levels
 * @Min: 0x00000000
 * @Max: 0x000003ff
 * @Default: 0x0000001e
 *
 * This INI is used to enable/disable console logs for specific log level.
 *
 * bit-0: Reserved
 * bit-1: QDF_TRACE_LEVEL_FATAL
 * bit-2: QDF_TRACE_LEVEL_ERROR
 * bit-3: QDF_TRACE_LEVEL_WARN
 * bit-4: QDF_TRACE_LEVEL_INFO
 * bit-5: QDF_TRACE_LEVEL_INFO_HIGH
 * bit-6: QDF_TRACE_LEVEL_INFO_MED
 * bit-7: QDF_TRACE_LEVEL_INFO_LOW
 * bit-8: QDF_TRACE_LEVEL_DEBUG
 * bit-9: QDF_TRACE_LEVEL_TRACE
 * bit-10 to bit-31: Reserved
 *
 * </ini>
 */
#define CFG_WLAN_LOGGING_CONSOLE_SUPPORT CFG_INI_UINT( \
				"wlanConsoleLogLevelsBitmap", \
				0x00000000, \
				0x000003ff, \
				0x0000001e, \
				CFG_VALUE_OR_DEFAULT, \
				"Wlan logging to console")

#define CFG_WLAN_LOGGING_SUPPORT_ALL \
	CFG(CFG_WLAN_LOGGING_SUPPORT) \
	CFG(CFG_WLAN_LOGGING_CONSOLE_SUPPORT) \
	CFG(CFG_HOST_LOG_CUSTOM_NETLINK_PROTO)
#else
#define CFG_WLAN_LOGGING_SUPPORT_ALL
#endif

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
/*
 * <ini>
 * gWlanAutoShutdown - Wlan auto shutdown timer value
 * @Min: 0
 * @Max: 86400
 * @Default: 0
 *
 * This ini specifies the seconds of WLAN inactivity firmware has to wait
 * before indicating WLAN idle event to driver. Upon receiving firmware's
 * WLAN idle indication, driver may indicate similar event to upper layer
 * daemons(SCM, or any other components working to achieve the same purpose),
 * who may choose what to do next, e.g. whether to unload driver module or not.
 * 0 indicates no auto shutdown will take place.
 *
 * </ini>
 */
#define CFG_WLAN_AUTO_SHUTDOWN CFG_INI_UINT( \
			"gWlanAutoShutdown", \
			0, \
			86400, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Wlan auto shutdown")
#define CFG_WLAN_AUTO_SHUTDOWN_ALL \
	CFG(CFG_WLAN_AUTO_SHUTDOWN)
#else
#define CFG_WLAN_AUTO_SHUTDOWN_ALL
#endif

/*
 * <ini>
 * gEnablefwprint - Enable FW uart print
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * </ini>
 */
#define CFG_ENABLE_FW_UART_PRINT CFG_INI_BOOL( \
			"gEnablefwprint", \
			0, \
			"Enable FW uart print")

/*
 * <ini>
 * gEnablefwlog - Enable FW log
 * @Min: 0
 * @Max: 2
 * @Default: 1
 *
 * </ini>
 */
#define CFG_ENABLE_FW_LOG CFG_INI_UINT( \
			"gEnablefwlog", \
			0, \
			2, \
			1, \
			CFG_VALUE_OR_DEFAULT, \
			"Enable FW log")

#ifndef REMOVE_PKT_LOG

#ifdef FEATURE_PKTLOG
#define CFG_ENABLE_PACKET_LOG_DEFAULT	1
#else
#define CFG_ENABLE_PACKET_LOG_DEFAULT	0
#endif

/*
 * <ini>
 * gEnablePacketLog - Enale packet log
 * @Min: 0
 * @Max: 1
 * @Default: 1 if packet log code is enabled, 0 otherwise
 *
 * This option enables/disables packet log collecting.
 *
 * </ini>
 */
#define CFG_ENABLE_PACKET_LOG CFG_INI_BOOL( \
			"gEnablePacketLog", \
			CFG_ENABLE_PACKET_LOG_DEFAULT, \
			"Enable packet log")

#define CFG_ENABLE_PACKET_LOG_ALL \
	CFG(CFG_ENABLE_PACKET_LOG)
#else
#define CFG_ENABLE_PACKET_LOG_ALL
#endif

#ifdef FEATURE_RUNTIME_PM

/**
 * enum hdd_runtime_pm_cfg - Runtime PM (RTPM) configuration options
 * @hdd_runtime_pm_disabled: RTPM and CxPC aware RTPM  disabled
 * @hdd_runtime_pm_static: RTPM enabled, but CxPC aware RTPM disabled
 * @hdd_runtime_pm_dynamic: RTPM and CxPC aware RTPM enabled
 */
enum hdd_runtime_pm_cfg {
	hdd_runtime_pm_disabled = 0,
	hdd_runtime_pm_static = 1,
	hdd_runtime_pm_dynamic = 2,
};

/*
 * <ini>
 * gRuntimePM - enable runtime suspend
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to enable runtime PM
 *
 * 0: RTPM disabled, so CxPC aware RTPM will be disabled as well
 * 1: RTPM enabled, but CxPC aware RTPM disabled
 * 2: RTPM enabled and CxPC aware RTPM enabled as well
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_RUNTIME_PM CFG_INI_UINT( \
		"gRuntimePM", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"This ini is used to enable runtime_suspend")
#define CFG_ENABLE_RUNTIME_PM_ALL \
	CFG(CFG_ENABLE_RUNTIME_PM)
#else
#define CFG_ENABLE_RUNTIME_PM_ALL
#endif

#ifdef WLAN_FEATURE_WMI_SEND_RECV_QMI
/*
 * <ini>
 * enable_qmi_stats - enable periodic stats over qmi
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable periodic stats over qmi if DUT is
 * in RTPM suspended state to avoid WoW enter/exit for every stats
 * request.
 *
 * 0: Periodic stats over QMI is disabled
 * 1: Periodic stats over QMI is enabled
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_QMI_STATS CFG_INI_UINT( \
		"enable_qmi_stats", \
		0, \
		1, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"This ini is used to enable periodic stats over qmi")
#define CFG_ENABLE_QMI_STATS_ALL \
	CFG(CFG_ENABLE_QMI_STATS)
#else
#define CFG_ENABLE_QMI_STATS_ALL
#endif

/*
 * <ini>
 * gInformBssRssiRaw - Report rssi in cfg80211_inform_bss_frame
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Option to report rssi in cfg80211_inform_bss_frame()
 *
 * Related: None
 *
 * Supported Feature: N/A
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_INFORM_BSS_RSSI_RAW CFG_INI_BOOL( \
		"gInformBssRssiRaw", \
		1, \
		"Option to report rssi in cfg80211_inform_bss_frame")

#ifdef FEATURE_WLAN_DYNAMIC_CVM
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
#define CFG_VC_MODE_BITMAP CFG_INI_INT( \
	"gConfigVCmode", \
	0x00000000, \
	0x0fffffff, \
	0x00000005, \
	CFG_VALUE_OR_DEFAULT, \
	"Bitmap for operating voltage corner mode")

#define CFG_VC_MODE_BITMAP_ALL CFG(CFG_VC_MODE_BITMAP)
#else
#define CFG_VC_MODE_BITMAP_ALL
#endif

/*
 * <ini>
 * def_sta_operating_freq - Default STA operating Freq
 * @Min: 0
 * @Max: 2484
 * @Default: 2412
 *
 * This ini is used to specify the default operating frequency of a STA during
 * initialization.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OPERATING_FREQUENCY CFG_INI_UINT( \
			"def_sta_operating_freq", \
			0, \
			2484, \
			2412, \
			CFG_VALUE_OR_DEFAULT, \
			"Default STA Operating Frequency")
#ifdef DHCP_SERVER_OFFLOAD
#define IPADDR_NUM_ENTRIES     (4)
#define IPADDR_STRING_LENGTH   (16)
#define CFG_DHCP_SERVER_IP_DEFAULT  ""

/*
 * struct wlan_mlme_chainmask - All chainmask related cfg items
 * @dhcpServerIP:     Dhcp server IP address
 * @is_dhcp_server_ip_valid:     is dhcp server valid
 */
struct dhcp_server {
	uint8_t dhcp_server_ip[IPADDR_NUM_ENTRIES];
	bool is_dhcp_server_ip_valid;
}

/*
 * <ini>
 * gDHCPServerIP - Dhcp server Ip name
 * @Default:
 *
 * This ini is used to give the DHCP IP server name
 */
#define CFG_DHCP_SERVER_IP_NAME \
	CFG_INI_STRING("gDHCPServerIP", \
	0, IPADDR_STRING_LENGTH, CFG_DHCP_SERVER_IP_DEFAULT, "DHCP Server IP")
#endif /* DHCP_SERVER_OFFLOAD */

/*
 * <ini>
 * gNumVdevs - max number of VDEVs supported
 *
 * @Min: 0x1
 * @Max: 0x5
 * @Default: CFG_TGT_NUM_VDEV
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NUM_VDEV_ENABLE CFG_INI_UINT( \
		"gNumVdevs", \
		1, \
		5, \
		CFG_TGT_NUM_VDEV, \
		CFG_VALUE_OR_DEFAULT, \
		"Number of VDEVs")

#define CFG_CONCURRENT_IFACE_MAX_LEN 16
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
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_ENABLE_CONCURRENT_STA CFG_INI_STRING( \
		"gEnableConcurrentSTA", \
		0, \
		CFG_CONCURRENT_IFACE_MAX_LEN, \
		"", \
		"Enable Concurrent STA")

#define CFG_DBS_SCAN_PARAM_LENGTH 42
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
#define CFG_DBS_SCAN_SELECTION CFG_INI_STRING( \
			"gdbs_scan_selection", \
			0, \
			CFG_DBS_SCAN_PARAM_LENGTH, \
			"", \
			"DBS Scan Selection")

/*
 * </ini>
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
#define CFG_ENABLE_MAC_PROVISION CFG_INI_BOOL( \
	"enable_mac_provision", \
	0, \
	"enable/disable MAC address provisioning feature")

/*
 * </ini>
 * read_mac_addr_from_mac_file - Use/ignore MAC address from mac cfg file
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used whether to configure MAC address from the cfg file or not
 *
 * Supported Feature: STA/SAP/P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_READ_MAC_ADDR_FROM_MAC_FILE CFG_INI_BOOL( \
	"read_mac_addr_from_mac_file", \
	0, \
	"Use/ignore MAC address from cfg file")

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
#define CFG_PROVISION_INTERFACE_POOL CFG_INI_UINT( \
			"provisioned_intf_pool", \
			0, \
			0xffffffff, \
			0xffffffff, \
			CFG_VALUE_OR_DEFAULT, \
			"It is bit mask value of Interfaces")

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
#define CFG_DERIVED_INTERFACE_POOL CFG_INI_UINT( \
				"derived_intf_pool", \
				0, \
				0xffffffff, \
				0xffffffff, \
				CFG_VALUE_OR_DEFAULT, \
				"It is bit mask value of Interfaces")

#ifdef ENABLE_MTRACE_LOG
/*
 * <ini>
 * enable_mtrace - Enable Mtrace.
 * @Default: 0
 *
 * This ini is used to enable MTRACE logging
 *
 * Related: None.
 *
 * Supported Feature: MTRACE
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_MTRACE CFG_INI_BOOL( \
			"enable_mtrace", \
			false, \
			"Enable MTRACE")
#define CFG_ENABLE_MTRACE_ALL CFG(CFG_ENABLE_MTRACE)
#else
#define CFG_ENABLE_MTRACE_ALL
#endif

/*
 * <ini>
 * gAdvertiseConcurrentOperation - Iface combination advertising
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to control whether driver should indicate to kernel
 * wiphy layer the combination of all its interfaces' supportability.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ADVERTISE_CONCURRENT_OPERATION CFG_INI_BOOL( \
		"gAdvertiseConcurrentOperation", \
		1, \
		"Iface combination advertising")

/*
 * <ini>
 * gEnableUnitTestFramework - Enable/Disable unit test framework
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: None
 *
 * Supported Feature: unit test framework
 *
 * Usage: Internal (only for dev and test team)
 *
 * </ini>
 */
#define CFG_ENABLE_UNIT_TEST_FRAMEWORK CFG_INI_BOOL( \
			"gEnableUnitTestFramework", \
			0, \
			"Enable/Disable unit test framework")

/*
 * <ini>
 * gDisableChannel - Used to disable channels specified
 *
 * @Min: 0
 * @Max: 1
 * Default: 0
 *
 * This ini is used to disable the channels given in the command
 * SET_DISABLE_CHANNEL_LIST and to restore the channels when the
 * command is given with channel list as 0
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DISABLE_CHANNEL  CFG_INI_BOOL( \
			"gDisableChannel", \
			0, \
			"Enable/Disable to disable channels specified")

/*
 * <ini>
 * gEnableSARV1toSARV2 - Used to Enable/Disable SAR version conversion
 *
 * @Min: 0
 * @Max: 1
 * Default: 1
 *
 * If user space is using SARV1 and FW is using SARV2 in BDF in that case
 * this ini is used to enable conversion from user specified SARV1 command
 * to FW expected SARV2 command.
 * If value of this ini is set to 0, SAR version 1 will
 * not be converted to SARV2 and command will be rejected.
 * If value of this ini is set to 1 SAR version 1 will be converted to
 * SARV2 based on FW capability
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAR_CONVERSION  CFG_INI_BOOL( \
			"gEnableSARV1toSARV2", \
			1, \
			"Enable/Disable conversion from SARV1 to SARV2")

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
#define CFG_NB_COMMANDS_RATE_LIMIT CFG_INI_UINT( \
			"nb_commands_interval", \
			0, \
			10, \
			3, \
			CFG_VALUE_OR_DEFAULT, \
			"Rate limiting for nb commands")

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
#define CFG_PERIODIC_STATS_TIMER_INTERVAL  CFG_INI_UINT( \
				"periodic_stats_timer_interval", \
				0, \
				10000, \
				3000, \
				CFG_VALUE_OR_DEFAULT, \
				"Periodic stats timer interval")

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
#define CFG_PERIODIC_STATS_TIMER_DURATION  CFG_INI_UINT( \
			"periodic_stats_timer_duration", \
			0, \
			60000, \
			30000, \
			CFG_VALUE_OR_DEFAULT, \
			"Periodic stats timer duration")

#define CFG_WLAN_STA_PERIODIC_STATS \
	 CFG(CFG_PERIODIC_STATS_TIMER_DURATION) \
	 CFG(CFG_PERIODIC_STATS_TIMER_INTERVAL)
#else
#define CFG_WLAN_STA_PERIODIC_STATS
#endif /* WLAN_FEATURE_PERIODIC_STA_STATS */

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
/*
 * <ini>
 * club_get_sta_in_ll_stats_req - Flag used to club ll_stats and get_station
 *                                requests in the driver
 *
 * @Min: 0
 * @Max: 1
 * Default: 1
 *
 * This ini param is used to enable/disable the feature for clubbing ll stats
 * and get station requests.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CLUB_LL_STA_AND_GET_STATION  CFG_INI_BOOL( \
			"club_get_sta_in_ll_stats_req", \
			1, \
			"Club ll_stats and get station requests")

/*
 * <ini>
 * sta_stats_cache_expiry_time - Expiry time for cached station stats
 *
 * @Min: 0
 * @Max: 5000
 * Default: 400
 *
 * This ini is used as duration in milliseconds for which cached station stats
 * are valid. Driver sends the cached information as response, if it gets the
 * get_station request with in this duration. Otherwise driver sends new
 * request to the firmware to get the updated stats.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_STA_STATS_CACHE_EXPIRY  CFG_INI_UINT( \
			"sta_stats_cache_expiry_time", \
			0, \
			5000, \
			400, \
			CFG_VALUE_OR_DEFAULT, \
			"Station stats cache expiry")

#define CFG_WLAN_CLUB_GET_STA_IN_LL_STA_REQ \
	 CFG(CFG_CLUB_LL_STA_AND_GET_STATION) \
	 CFG(CFG_STA_STATS_CACHE_EXPIRY)
#else
#define CFG_WLAN_CLUB_GET_STA_IN_LL_STA_REQ
#endif /* FEATURE_CLUB_LL_STATS_AND_GET_STATION */

#ifdef WLAN_FEATURE_11BE_MLO
/*
 * <ini>
 * link_state_cache_expiry_time - Expiry time for cached ml link state
 *
 * @Min: 0
 * @Max: 5000
 * Default: 400
 *
 * This ini is used as duration in milliseconds for which cached ml link state
 * are valid. Driver sends the cached information as response, if it gets the
 * ml link state request with in this duration. Otherwise driver sends new
 * request to the firmware to get the updated ml link state.
 *
 * Supported Feature: MLO STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LINK_STATE_CACHE_EXPIRY  CFG_INI_UINT( \
			"link_state_cache_expiry_time", \
			0, \
			5000, \
			400, \
			CFG_VALUE_OR_DEFAULT, \
			"link state cache expiry")

#define CFG_LINK_STATE_CACHE_EXPIRY_ALL \
	CFG(CFG_LINK_STATE_CACHE_EXPIRY)
#else
#define CFG_LINK_STATE_CACHE_EXPIRY_ALL
#endif /* WLAN_FEATURE_11BE_MLO */

/**
 * enum host_log_level - Debug verbose level imposed by user
 * @HOST_LOG_LEVEL_NONE: no trace will be logged.
 * @HOST_LOG_LEVEL_FATAL: fatal error will be logged
 * @HOST_LOG_LEVEL_ERROR: error(include level less than error) will be logged
 * @HOST_LOG_LEVEL_WARN: warning(include level less than warning) will be logged
 * @HOST_LOG_LEVEL_INFO: inform(include level less than inform) will be logged
 * @HOST_LOG_LEVEL_DEBUG: debug(include level less than debug) will be logged
 * @HOST_LOG_LEVEL_TRACE: trace(include level less than trace) will be logged
 * @HOST_LOG_LEVEL_MAX: Max host log level
 */
enum host_log_level {
	HOST_LOG_LEVEL_NONE = 0,
	HOST_LOG_LEVEL_FATAL,
	HOST_LOG_LEVEL_ERROR,
	HOST_LOG_LEVEL_WARN,
	HOST_LOG_LEVEL_INFO,
	HOST_LOG_LEVEL_DEBUG,
	HOST_LOG_LEVEL_TRACE,
	HOST_LOG_LEVEL_MAX,
};

/*
 * <ini>
 * gHostModuleLoglevel - modulized host debug log level
 * @Min: N/A
 * @Max: N/A
 * @Default: N/A
 *
 * This ini is used to set modulized host debug log level.
 * WLAN host module log level input string format looks like below:
 * gHostModuleLoglevel="<host Module ID>,<Log Level>,..."
 * For example:
 * gHostModuleLoglevel=51,1,52,2,53,3,54,4,55,5,56,6
 * The above input string means:
 * For WLAN host module ID 51 enable log level HOST_LOG_LEVEL_FATAL
 * For WLAN host module ID 52 enable log level HOST_LOG_LEVEL_ERROR
 * For WLAN host module ID 53 enable log level HOST_LOG_LEVEL_WARN
 * For WLAN host module ID 54 enable log level HOST_LOG_LEVEL_INFO
 * For WLAN host module ID 55 enable log level HOST_LOG_LEVEL_DEBUG
 * For WLAN host module ID 55 enable log level HOST_LOG_LEVEL_TRACE
 * For valid values of module ids check enum QDF_MODULE_ID and
 * for valid values of log levels check below.
 * HOST_LOG_LEVEL_NONE = 0, No trace will be logged
 * HOST_LOG_LEVEL_FATAL = 1, fatal error log
 * HOST_LOG_LEVEL_ERROR = 2, error(include level less than error) log
 * HOST_LOG_LEVEL_WARN = 3, warning(include level less than warning) log
 * HOST_LOG_LEVEL_INFO = 4, inform(include level less than inform) log
 * HOST_LOG_LEVEL_DEBUG = 5, debug(include level less than debug) log
 * HOST_LOG_LEVEL_TRACE = 6, trace(include level less than trace) log
 *
 * Related: None
 *
 * Supported Feature: Debugging
 *
 * Usage: Internal
 *
 * </ini>
 */

#define HOST_MODULE_LOG_LEVEL_STRING_MAX_LENGTH  (QDF_MODULE_ID_MAX * 6)
#define CFG_ENABLE_HOST_MODULE_LOG_LEVEL CFG_INI_STRING( \
	"gHostModuleLoglevel", \
	0, \
	HOST_MODULE_LOG_LEVEL_STRING_MAX_LENGTH, \
	"", \
	"Set modulized host debug log level")

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
/*
 * <ini>
 * dynamic_mac_addr_update_supported - Flag to configure dynamic MAC address
 *                                     support in the driver
 *
 * @Min: 0
 * @Max: 1
 * Default: 1
 *
 * This ini param is used to enable/disable the dynamic MAC address support
 * in the driver.
 *
 * Supported Feature: STA/SAP/P2P_Device
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DYNAMIC_MAC_ADDR_UPDATE_SUPPORTED CFG_INI_BOOL( \
			"dynamic_mac_addr_update_supported", \
			1, \
			"Dynamic MAC address update support")
#define CFG_DYNAMIC_MAC_ADDR_UPDATE_SUPPORTED_ALL \
	CFG(CFG_DYNAMIC_MAC_ADDR_UPDATE_SUPPORTED)
#else
#define CFG_DYNAMIC_MAC_ADDR_UPDATE_SUPPORTED_ALL
#endif

/*
 * <ini>
 * exclude_selftx_from_cca_busy_time - Exclude self tx time from cca busy time
 * @Default: false
 *
 * This ini is used to exclude self tx time from cca busy time.
 *
 * false: Don't exclude self tx time from cca busy time.
 * true: Deduct tx time from cca busy time.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXCLUDE_SELFTX_FROM_CCA_BUSY_TIME CFG_INI_BOOL( \
	"exclude_selftx_from_cca_busy_time", \
	false, \
	"This ini is used to exclude self tx time from CCA busy time")

#define CFG_HDD_ALL \
	CFG_DYNAMIC_MAC_ADDR_UPDATE_SUPPORTED_ALL \
	CFG_ENABLE_PACKET_LOG_ALL \
	CFG_ENABLE_RUNTIME_PM_ALL \
	CFG_ENABLE_QMI_STATS_ALL \
	CFG_VC_MODE_BITMAP_ALL \
	CFG_WLAN_AUTO_SHUTDOWN_ALL \
	CFG_WLAN_CLUB_GET_STA_IN_LL_STA_REQ \
	CFG_WLAN_LOGGING_SUPPORT_ALL \
	CFG_WLAN_STA_PERIODIC_STATS \
	CFG(CFG_ADVERTISE_CONCURRENT_OPERATION) \
	CFG(CFG_BUG_ON_REINIT_FAILURE) \
	CFG(CFG_DBS_SCAN_SELECTION) \
	CFG(CFG_DERIVED_INTERFACE_POOL) \
	CFG(CFG_ENABLE_CONCURRENT_STA) \
	CFG(CFG_ENABLE_FW_LOG) \
	CFG(CFG_ENABLE_FW_UART_PRINT) \
	CFG(CFG_ENABLE_MAC_PROVISION) \
	CFG_ENABLE_MTRACE_ALL \
	CFG(CFG_ENABLE_RAMDUMP_COLLECTION) \
	CFG(CFG_ENABLE_UNIT_TEST_FRAMEWORK) \
	CFG(CFG_INTERFACE_CHANGE_WAIT) \
	CFG(CFG_INFORM_BSS_RSSI_RAW) \
	CFG(CFG_MULTICAST_HOST_FW_MSGS) \
	CFG(CFG_NUM_VDEV_ENABLE) \
	CFG(CFG_OPERATING_FREQUENCY) \
	CFG(CFG_PRIVATE_WEXT_CONTROL) \
	CFG(CFG_PROVISION_INTERFACE_POOL) \
	CFG(CFG_TIMER_MULTIPLIER) \
	CFG(CFG_NB_COMMANDS_RATE_LIMIT) \
	CFG(CFG_HDD_DOT11_MODE) \
	CFG(CFG_ENABLE_DISABLE_CHANNEL) \
	CFG(CFG_READ_MAC_ADDR_FROM_MAC_FILE) \
	CFG(CFG_SAR_CONVERSION) \
	CFG(CFG_ENABLE_HOST_MODULE_LOG_LEVEL) \
	SAR_SAFETY_FEATURE_ALL \
	CFG_GET_WIFI_FEATURES_ALL \
	CFG_CPU_CXPC_THRESHOLD_ALL \
	CFG(CFG_EXCLUDE_SELFTX_FROM_CCA_BUSY_TIME) \
	CFG_LINK_STATE_CACHE_EXPIRY_ALL
#endif
