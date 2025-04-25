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

#if !defined(WLAN_HDD_MAIN_H)
#define WLAN_HDD_MAIN_H
/**
 * DOC: wlan_hdd_main.h
 *
 * Linux HDD Adapter Type
 */

/*
 * The following terms were in use in prior versions of the driver but
 * have now been replaced with terms that are aligned with the Linux
 * Coding style. Macros are defined to hopefully prevent new instances
 * from being introduced, primarily by code propagation.
 */
#define pHddCtx
#define pAdapter
#define pHostapdAdapter
#define pHddApCtx
#define pHddStaCtx
#define pHostapdState
#define pRoamInfo
#define pScanInfo
#define pBeaconIes

/*
 * Include files
 */

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/cfg80211.h>
#include <linux/ieee80211.h>
#include <qdf_delayed_work.h>
#include <qdf_list.h>
#include <qdf_types.h>
#include "sir_mac_prot_def.h"
#include "csr_api.h"
#include "wlan_dsc.h"
#include <wlan_hdd_assoc.h>
#include <wlan_hdd_wmm.h>
#include <wlan_hdd_cfg.h>
#include <linux/spinlock.h>
#include <ani_system_defs.h>
#if defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif
#ifdef WLAN_FEATURE_TSF_PTP
#include <linux/ptp_classify.h>
#include <linux/ptp_clock_kernel.h>
#endif
#include <wlan_hdd_ftm.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_tsf.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_debugfs.h"
#include <qdf_defer.h>
#include "sap_api.h"
#include "cdp_txrx_flow_ctrl_legacy.h"
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_misc.h>
#include "wlan_hdd_nan_datapath.h"
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include "wlan_pmo_ucfg_api.h"
#ifdef WIFI_POS_CONVERGED
#include "os_if_wifi_pos.h"
#include "wifi_pos_api.h"
#else
#include "wlan_hdd_oemdata.h"
#endif
#include "wlan_hdd_he.h"

#include <net/neighbour.h>
#include <net/netevent.h>
#include "wlan_hdd_twt.h"
#include "wma_sar_public_structs.h"
#include "wlan_mlme_ucfg_api.h"
#include "pld_common.h"
#include "wlan_cm_roam_public_struct.h"

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
#include "qdf_periodic_work.h"
#endif

#if defined(CLD_PM_QOS) || defined(FEATURE_RUNTIME_PM)
#include <linux/pm_qos.h>
#endif

#include "wlan_hdd_sta_info.h"
#include <wlan_hdd_cm_api.h>
#include "wlan_hdd_mlo.h"
#include "wlan_osif_features.h"
#include "wlan_dp_public_struct.h"

/*
 * Preprocessor definitions and constants
 */

/* Milli seconds to delay SSR thread when an packet is getting processed */
#define SSR_WAIT_SLEEP_TIME 200
/* MAX iteration count to wait for dp tx to complete */
#define MAX_SSR_WAIT_ITERATIONS 100
#define MAX_SSR_PROTECT_LOG (16)

#define HDD_MAX_OEM_DATA_LEN 1024
#define HDD_MAX_FILE_NAME_LEN 64
#ifdef FEATURE_WLAN_APF
/**
 * struct hdd_apf_context - hdd Context for apf
 * @magic: magic number
 * @qdf_apf_event: Completion variable for APF get operations
 * @capability_response: capabilities response received from fw
 * @apf_enabled: True: APF Interpreter enabled, False: Disabled
 * @cmd_in_progress: Flag that indicates an APF command is in progress
 * @buf: Buffer to accumulate read memory chunks
 * @buf_len: Length of the read memory requested
 * @offset: APF work memory offset to fetch from
 * @lock: APF Context lock
 */
struct hdd_apf_context {
	unsigned int magic;
	qdf_event_t qdf_apf_event;
	bool apf_enabled;
	bool cmd_in_progress;
	uint8_t *buf;
	uint32_t buf_len;
	uint32_t offset;
	qdf_spinlock_t lock;
};
#endif /* FEATURE_WLAN_APF */

#ifdef TX_MULTIQ_PER_AC
#define TX_GET_QUEUE_IDX(ac, off) (((ac) * TX_QUEUES_PER_AC) + (off))
#define TX_QUEUES_PER_AC 4
#else
#define TX_GET_QUEUE_IDX(ac, off) (ac)
#define TX_QUEUES_PER_AC 1
#endif

/** Number of Tx Queues */
#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || \
	defined(QCA_HL_NETDEV_FLOW_CONTROL) || \
	defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/* Only one HI_PRIO queue */
#define NUM_TX_QUEUES (4 * TX_QUEUES_PER_AC + 1)
#else
#define NUM_TX_QUEUES (4 * TX_QUEUES_PER_AC)
#endif

#define NUM_RX_QUEUES 5

/*
 * Number of DPTRACE records to dump when a cfg80211 disconnect with reason
 * WLAN_REASON_DEAUTH_LEAVING DEAUTH is received from user-space.
 */
#define WLAN_DEAUTH_DPTRACE_DUMP_COUNT 100

/* HDD_IS_RATE_LIMIT_REQ: Macro helper to implement rate limiting
 * @flag: The flag to determine if limiting is required or not
 * @rate: The number of seconds within which if multiple commands come, the
 *	  flag will be set to true
 *
 * If the function in which this macro is used is called multiple times within
 * "rate" number of seconds, the "flag" will be set to true which can be used
 * to reject/take appropriate action.
 */
#define HDD_IS_RATE_LIMIT_REQ(flag, rate)\
	do {\
		static ulong __last_ticks;\
		ulong __ticks = jiffies;\
		flag = false; \
		if (!time_after(__ticks,\
		    __last_ticks + rate * HZ)) {\
			flag = true; \
		} \
		else { \
			__last_ticks = __ticks;\
		} \
	} while (0)

/*
 * API in_compat_syscall() is introduced in 4.6 kernel to check whether we're
 * in a compat syscall or not. It is a new way to query the syscall type, which
 * works properly on all architectures.
 *
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
static inline bool in_compat_syscall(void) { return is_compat_task(); }
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)) || \
	defined(CFG80211_REMOVE_IEEE80211_BACKPORT)
#define HDD_NL80211_BAND_2GHZ   NL80211_BAND_2GHZ
#define HDD_NL80211_BAND_5GHZ   NL80211_BAND_5GHZ
#define HDD_NUM_NL80211_BANDS   NUM_NL80211_BANDS
#else
#define HDD_NL80211_BAND_2GHZ   IEEE80211_BAND_2GHZ
#define HDD_NL80211_BAND_5GHZ   IEEE80211_BAND_5GHZ
#define HDD_NUM_NL80211_BANDS   ((enum nl80211_band)IEEE80211_NUM_BANDS)
#endif

#if defined(CONFIG_BAND_6GHZ) && (defined(CFG80211_6GHZ_BAND_SUPPORTED) || \
	(KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE))
#define HDD_NL80211_BAND_6GHZ   NL80211_BAND_6GHZ
#endif

#define TSF_GPIO_PIN_INVALID 255

/** Length of the TX queue for the netdev */
#define HDD_NETDEV_TX_QUEUE_LEN (3000)

/** Hdd Tx Time out value */
#define HDD_TX_TIMEOUT          msecs_to_jiffies(5000)

#define HDD_TX_STALL_THRESHOLD 4

/** Hdd Default MTU */
#define HDD_DEFAULT_MTU         (1500)

#ifdef QCA_CONFIG_SMP
#define NUM_CPUS NR_CPUS
#else
#define NUM_CPUS 1
#endif

#define ACS_COMPLETE_TIMEOUT 3000

#define HDD_PSOC_IDLE_SHUTDOWN_SUSPEND_DELAY (1000)
/**
 * enum hdd_adapter_flags - event bitmap flags registered net device
 * @NET_DEVICE_REGISTERED: Adapter is registered with the kernel
 * @WMM_INIT_DONE: Adapter is initialized
 * @DEVICE_IFACE_OPENED: Adapter has been "opened" via the kernel
 * @WDEV_ONLY_REGISTERED: Only WDEV is registered
 */
enum hdd_adapter_flags {
	NET_DEVICE_REGISTERED,
	WMM_INIT_DONE,
	DEVICE_IFACE_OPENED,
	WDEV_ONLY_REGISTERED,
};

/**
 * enum hdd_link_flags - Event bitmap flags specific to per link
 * @SME_SESSION_OPENED: Firmware vdev has been created
 * @SOFTAP_BSS_STARTED: Software Access Point (SAP) is running
 * @SOFTAP_INIT_DONE: Software Access Point (SAP) is initialized
 * @VENDOR_ACS_RESPONSE_PENDING: Waiting for event for vendor acs
 */
enum hdd_link_flags {
	SME_SESSION_OPENED,
	SOFTAP_BSS_STARTED,
	SOFTAP_INIT_DONE,
	VENDOR_ACS_RESPONSE_PENDING,
};

/**
 * enum hdd_nb_cmd_id - North bound command IDs received during SSR
 * @NO_COMMAND: No NB command received during SSR
 * @INTERFACE_DOWN: Received interface down during SSR
 */
enum hdd_nb_cmd_id {
	NO_COMMAND,
	INTERFACE_DOWN
};

#define WLAN_WAIT_TIME_STATS       800
#define WLAN_WAIT_TIME_LINK_STATUS 800

/** Maximum time(ms) to wait for mc thread suspend **/
#define WLAN_WAIT_TIME_MCTHREAD_SUSPEND  1200

/** Maximum time(ms) to wait for target to be ready for suspend **/
#define WLAN_WAIT_TIME_READY_TO_SUSPEND  2000

/* Scan Req Timeout */
#define WLAN_WAIT_TIME_SCAN_REQ 100

#define WLAN_WAIT_TIME_APF     1000

#define WLAN_WAIT_TIME_FW_ROAM_STATS 1000

#define WLAN_WAIT_TIME_ANTENNA_ISOLATION 8000

/* Maximum time(ms) to wait for RSO CMD status event */
#define WAIT_TIME_RSO_CMD_STATUS 2000

/* rcpi request timeout in milli seconds */
#define WLAN_WAIT_TIME_RCPI 500

#define WLAN_WAIT_PEER_CLEANUP 5000

#define MAX_CFG_STRING_LEN  255

/* Maximum time(ms) to wait for external acs response */
#define WLAN_VENDOR_ACS_WAIT_TIME 1000

/* Maximum time(ms) to wait for monitor mode vdev up event completion*/
#define WLAN_MONITOR_MODE_VDEV_UP_EVT      SME_CMD_VDEV_START_BSS_TIMEOUT

/* Mac Address string length */
#define MAC_ADDRESS_STR_LEN 18  /* Including null terminator */
/* Max and min IEs length in bytes */
#define MAX_GENIE_LEN (512)
#define MIN_GENIE_LEN (2)

#define WPS_OUI_TYPE   "\x00\x50\xf2\x04"
#define WPS_OUI_TYPE_SIZE  4

#define P2P_OUI_TYPE   "\x50\x6f\x9a\x09"
#define P2P_OUI_TYPE_SIZE  4

#define OSEN_OUI_TYPE   "\x50\x6f\x9a\x12"
#define OSEN_OUI_TYPE_SIZE  4

#ifdef WLAN_FEATURE_WFD
#define WFD_OUI_TYPE   "\x50\x6f\x9a\x0a"
#define WFD_OUI_TYPE_SIZE  4
#endif

#define MBO_OUI_TYPE   "\x50\x6f\x9a\x16"
#define MBO_OUI_TYPE_SIZE  4

#define QCN_OUI_TYPE   "\x8c\xfd\xf0\x01"
#define QCN_OUI_TYPE_SIZE  4

#define wlan_hdd_get_wps_ie_ptr(ie, ie_len) \
	wlan_get_vendor_ie_ptr_from_oui(WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE, \
	ie, ie_len)

#define hdd_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_HDD, params)
#define hdd_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_HDD, params)
#define hdd_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_HDD, params)
#define hdd_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_HDD, params)
#define hdd_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_HDD, params)

#define hdd_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_HDD, params)

#define hdd_alert_rl(params...) QDF_TRACE_FATAL_RL(QDF_MODULE_ID_HDD, params)
#define hdd_err_rl(params...) QDF_TRACE_ERROR_RL(QDF_MODULE_ID_HDD, params)
#define hdd_warn_rl(params...) QDF_TRACE_WARN_RL(QDF_MODULE_ID_HDD, params)
#define hdd_info_rl(params...) QDF_TRACE_INFO_RL(QDF_MODULE_ID_HDD, params)
#define hdd_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD, params)

#define hdd_enter() QDF_TRACE_ENTER(QDF_MODULE_ID_HDD, "enter")
#define hdd_enter_dev(dev) \
	QDF_TRACE_ENTER(QDF_MODULE_ID_HDD, "enter(%s)", (dev)->name)
#define hdd_exit() QDF_TRACE_EXIT(QDF_MODULE_ID_HDD, "exit")

#define WLAN_HDD_GET_PRIV_PTR(__dev__) \
		(struct hdd_adapter *)(netdev_priv((__dev__)))

#define MAX_NO_OF_2_4_CHANNELS 14

#define WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET 24

#define WLAN_HDD_IS_SOCIAL_CHANNEL(center_freq)	\
	(((center_freq) == 2412) || ((center_freq) == 2437) || \
	((center_freq) == 2462))

#define WLAN_HDD_QOS_ACTION_FRAME 1
#define WLAN_HDD_QOS_MAP_CONFIGURE 4
#define HDD_SAP_WAKE_LOCK_DURATION WAKELOCK_DURATION_RECOMMENDED

/* SAP client disconnect wake lock duration in milli seconds */
#define HDD_SAP_CLIENT_DISCONNECT_WAKE_LOCK_DURATION \
	WAKELOCK_DURATION_RECOMMENDED

#define HDD_CFG_REQUEST_FIRMWARE_RETRIES (3)
#define HDD_CFG_REQUEST_FIRMWARE_DELAY (20)

#define MAX_USER_COMMAND_SIZE 4096
#define DNS_DOMAIN_NAME_MAX_LEN 255
#define ICMPv6_ADDR_LEN 16


#define HDD_MIN_TX_POWER (-100) /* minimum tx power */
#define HDD_MAX_TX_POWER (+100) /* maximum tx power */

/* If IPA UC data path is enabled, target should reserve extra tx descriptors
 * for IPA data path.
 * Then host data path should allow less TX packet pumping in case
 * IPA data path enabled
 */
#define WLAN_TFC_IPAUC_TX_DESC_RESERVE   100

/*
 * NET_NAME_UNKNOWN is only introduced after Kernel 3.17, to have a macro
 * here if the Kernel version is less than 3.17 to avoid the interleave
 * conditional compilation.
 */
#if !((LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) ||\
	defined(WITH_BACKPORTS))
#define NET_NAME_UNKNOWN	0
#endif

#define PRE_CAC_SSID "pre_cac_ssid"

#define SCAN_REJECT_THRESHOLD_TIME 300000 /* Time is in msec, equal to 5 mins */
#define SCAN_REJECT_THRESHOLD 15

/* Default Psoc id */
#define DEFAULT_PSOC_ID 1

/* wait time for nud stats in milliseconds */
#define WLAN_WAIT_TIME_NUD_STATS 800
/* nud stats skb max length */
#define WLAN_NUD_STATS_LEN 800
/* ARP packet type for NUD debug stats */
#define WLAN_NUD_STATS_ARP_PKT_TYPE 1
/* Assigned size of driver memory dump is 4096 bytes */
#define DRIVER_MEM_DUMP_SIZE    4096

/* MAX OS Q block time value in msec
 * Prevent from permanent stall, resume OS Q if timer expired
 */
#define WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME 1000
#define WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME 100
#define WLAN_HDD_TX_FLOW_CONTROL_MAX_24BAND_CH   14

#ifndef NUM_TX_RX_HISTOGRAM
#define NUM_TX_RX_HISTOGRAM 128
#endif

#define NUM_TX_RX_HISTOGRAM_MASK (NUM_TX_RX_HISTOGRAM - 1)

#define HDD_NOISE_FLOOR_DBM (-96)

#define INTF_MACADDR_MASK       0x7

/**
 * typedef wlan_net_dev_ref_dbgid - Debug IDs to detect net device reference
 *                                  leaks.
 * NOTE: New values added to the enum must also be reflected in function
 * net_dev_ref_debug_string_from_id()
 */
typedef enum {
	NET_DEV_HOLD_ID_RESERVED = 0,
	NET_DEV_HOLD_GET_STA_CONNECTION_IN_PROGRESS = 1,
	NET_DEV_HOLD_CHECK_DFS_CHANNEL_FOR_ADAPTER = 2,
	NET_DEV_HOLD_GET_SAP_OPERATING_BAND = 3,
	NET_DEV_HOLD_RECOVERY_NOTIFIER_CALL = 4,
	NET_DEV_HOLD_IS_ANY_STA_CONNECTING = 5,
	NET_DEV_HOLD_SAP_DESTROY_CTX_ALL = 6,
	NET_DEV_HOLD_DRV_CMD_MAX_TX_POWER = 7,
	NET_DEV_HOLD_IPA_SET_TX_FLOW_INFO = 8,
	NET_DEV_HOLD_SET_RPS_CPU_MASK = 9,
	NET_DEV_HOLD_DFS_INDICATE_RADAR = 10,
	NET_DEV_HOLD_MAX_STA_INTERFACE_UP_COUNT_REACHED = 11,
	NET_DEV_HOLD_IS_CHAN_SWITCH_IN_PROGRESS = 12,
	NET_DEV_HOLD_STA_DESTROY_CTX_ALL = 13,
	NET_DEV_HOLD_CHECK_FOR_EXISTING_MACADDR = 14,
	NET_DEV_HOLD_DEINIT_ALL_ADAPTERS = 15,
	NET_DEV_HOLD_STOP_ALL_ADAPTERS = 16,
	NET_DEV_HOLD_RESET_ALL_ADAPTERS = 17,
	NET_DEV_HOLD_IS_ANY_INTERFACE_OPEN = 18,
	NET_DEV_HOLD_START_ALL_ADAPTERS = 19,
	NET_DEV_HOLD_GET_ADAPTER_BY_RAND_MACADDR = 20,
	NET_DEV_HOLD_GET_ADAPTER_BY_MACADDR = 21,
	NET_DEV_HOLD_GET_ADAPTER_BY_VDEV = 22,
	NET_DEV_HOLD_ADAPTER_GET_BY_REFERENCE = 23,
	NET_DEV_HOLD_GET_ADAPTER_BY_IFACE_NAME = 24,
	NET_DEV_HOLD_GET_ADAPTER = 25,
	NET_DEV_HOLD_GET_OPERATING_CHAN_FREQ = 26,
	NET_DEV_HOLD_UNREGISTER_WEXT_ALL_ADAPTERS = 27,
	NET_DEV_HOLD_ABORT_MAC_SCAN_ALL_ADAPTERS = 28,
	NET_DEV_HOLD_ABORT_SCHED_SCAN_ALL_ADAPTERS = 29,
	NET_DEV_HOLD_GET_FIRST_VALID_ADAPTER = 30,
	NET_DEV_HOLD_CLEAR_RPS_CPU_MASK = 31,
	NET_DEV_HOLD_BUS_BW_WORK_HANDLER = 32,
	NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY_COMPACT = 33,
	NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY = 34,
	NET_DEV_HOLD_CLEAR_NETIF_QUEUE_HISTORY = 35,
	NET_DEV_HOLD_UNSAFE_CHANNEL_RESTART_SAP = 36,
	NET_DEV_HOLD_INDICATE_MGMT_FRAME = 37,
	NET_DEV_HOLD_STATE_INFO_DUMP = 38,
	NET_DEV_HOLD_DISABLE_ROAMING = 39,
	NET_DEV_HOLD_ENABLE_ROAMING = 40,
	NET_DEV_HOLD_AUTO_SHUTDOWN_ENABLE = 41,
	NET_DEV_HOLD_GET_CON_SAP_ADAPTER = 42,
	NET_DEV_HOLD_IS_ANY_ADAPTER_CONNECTED = 43,
	NET_DEV_HOLD_IS_ROAMING_IN_PROGRESS = 44,
	NET_DEV_HOLD_DEL_P2P_INTERFACE = 45,
	NET_DEV_HOLD_IS_NDP_ALLOWED = 46,
	NET_DEV_HOLD_NDI_OPEN = 47,
	NET_DEV_HOLD_SEND_OEM_REG_RSP_NLINK_MSG = 48,
	NET_DEV_HOLD_PERIODIC_STA_STATS_DISPLAY = 49,
	NET_DEV_HOLD_SUSPEND_WLAN = 50,
	NET_DEV_HOLD_RESUME_WLAN = 51,
	NET_DEV_HOLD_SSR_RESTART_SAP = 52,
	NET_DEV_HOLD_SEND_DEFAULT_SCAN_IES = 53,
	NET_DEV_HOLD_CFG80211_SUSPEND_WLAN = 54,
	NET_DEV_HOLD_COUNTRY_CHANGE_UPDATE_STA = 55,
	NET_DEV_HOLD_COUNTRY_CHANGE_UPDATE_SAP = 56,
	NET_DEV_HOLD_CACHE_STATION_STATS_CB = 57,
	NET_DEV_HOLD_DISPLAY_TXRX_STATS = 58,
	NET_DEV_HOLD_BUS_BW_MGR = 59,
	NET_DEV_HOLD_START_PRE_CAC_TRANS = 60,
	NET_DEV_HOLD_IS_ANY_STA_CONNECTED = 61,
	NET_DEV_HOLD_GET_ADAPTER_BY_BSSID = 62,
	NET_DEV_HOLD_ALLOW_NEW_INTF = 63,

	/* Keep it at the end */
	NET_DEV_HOLD_ID_MAX
} wlan_net_dev_ref_dbgid;

struct hdd_tx_rx_stats {
	struct {
		/* start_xmit stats */
		__u32    tx_classified_ac[WLAN_MAX_AC];
		__u32    tx_dropped_ac[WLAN_MAX_AC];
#ifdef TX_MULTIQ_PER_AC
		/* Neither valid socket nor skb->hash */
		uint32_t inv_sk_and_skb_hash;
		/* skb->hash already calculated */
		uint32_t qselect_existing_skb_hash;
		/* valid tx queue id in socket */
		uint32_t qselect_sk_tx_map;
		/* skb->hash calculated in select queue */
		uint32_t qselect_skb_hash_calc;
#endif
	} per_cpu[NUM_CPUS];

	/* txflow stats */
	bool     is_txflow_paused;
	__u32    txflow_pause_cnt;
	__u32    txflow_unpause_cnt;
	__u32    txflow_timer_cnt;

};

/**
 * struct hdd_pmf_stats - Protected Management Frame statistics
 * @num_unprot_deauth_rx: Number of unprotected deauth frames received
 * @num_unprot_disassoc_rx: Number of unprotected disassoc frames received
 */
struct hdd_pmf_stats {
	uint8_t num_unprot_deauth_rx;
	uint8_t num_unprot_disassoc_rx;
};

/**
 * struct hdd_peer_stats - Peer stats at HDD level
 * @rx_count: RX count
 * @rx_bytes: RX bytes
 * @fcs_count: FCS err count
 */
struct hdd_peer_stats {
	uint32_t rx_count;
	uint64_t rx_bytes;
	uint32_t fcs_count;
};

#define HDD_MAX_PER_PEER_RATES 16
#if defined(WLAN_FEATURE_11BE_MLO)
/**
 * struct wlan_hdd_station_stats_info - Station stats info
 * @signal: Signal strength of last received PPDU
 * @signal_avg: Average signal strength
 * @chain_signal_avg: Per-chain signal strength average
 * @rxrate: Last unicast data frame rx rate
 * @txrate: Current unicasr tx rate
 * @rx_bytes: Total received bytes (MPDU length)
 * @tx_bytes: Total transmitted bytes (MPDU length)
 * @rx_packets: Total received packets (MSDUs and MMPDUs)
 * @tx_packets: Total transmitted packets (MSDUs and MMPDUs)
 * @tx_retries: Cumulative retry count (MPDU)
 * @tx_failed: Number of failed transmissions (MPDUs)
 * @rx_mpdu_count: Number of MPDUs received from this station
 * @fcs_err_count: Number of MPDUs received from this station with an FCS error
 */
struct wlan_hdd_station_stats_info {
	int8_t signal;
	int8_t signal_avg;
	int8_t chain_signal_avg[IEEE80211_MAX_CHAINS];
	struct rate_info txrate;
	struct rate_info rxrate;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	uint32_t rx_packets;
	uint32_t tx_packets;
	uint32_t tx_retries;
	uint32_t tx_failed;
	uint32_t rx_mpdu_count;
	uint32_t fcs_err_count;
};

/**
 * struct wlan_hdd_mlo_iface_stats_info - mlo iface stats info
 * @link_id: mlo link_id
 * @freq: frequency of the mlo link
 * @radio_id: radio id of the mlo link
 */
struct wlan_hdd_mlo_iface_stats_info {
	uint8_t link_id;
	uint32_t freq;
	uint32_t radio_id;
};

/**
 * struct wlan_hdd_peer_info - hdd per peer info
 * @type: peer type (AP, TDLS, GO etc.)
 * @peer_mac: peer mac address
 * @capabilities: peer WIFI_CAPABILITY_XXX
 * @power_saving: peer power saving mode
 * @num_rate: number of rates
 * @rate_stats: per rate statistics, num entries = HDD_MAX_PER_PEER_RATES
 * @stats_cached: whether peer stats cached into link_info struct
 * @link_id: IEEE link id for the link
 */
struct wlan_hdd_peer_info {
	enum wmi_peer_type type;
	struct qdf_mac_addr peer_mac;
	uint32_t capabilities;
	union {
		uint32_t power_saving;
		uint32_t num_rate;
	};
	struct wifi_rate_stat rate_stats[HDD_MAX_PER_PEER_RATES];
	bool stats_cached;
	uint32_t link_id;
};
#endif

#define MAX_SUBTYPES_TRACKED	4

struct hdd_stats {
	tCsrSummaryStatsInfo summary_stat;
	tCsrGlobalClassAStatsInfo class_a_stat;
	tCsrGlobalClassDStatsInfo class_d_stat;
	struct csr_per_chain_rssi_stats_info  per_chain_rssi_stats;
	struct hdd_tx_rx_stats tx_rx_stats;
	struct hdd_peer_stats peer_stats;
	struct hdd_pmf_stats hdd_pmf_stats;
	struct pmf_bcn_protect_stats bcn_protect_stats;
};

/**
 * struct hdd_roaming_info - HDD Internal Roaming Information
 * @bssid: BSSID to which we are connected
 * @peer_mac: Peer MAC address for IBSS connection
 * @roam_id: Unique identifier for a roaming instance
 * @roam_status: Current roam command status
 */
struct hdd_roaming_info {
	tSirMacAddr bssid;
	tSirMacAddr peer_mac;
	uint32_t roam_id;
	eRoamCmdStatus roam_status;
};

#ifdef FEATURE_WLAN_WAPI
/* Define WAPI macros for Length, BKID count etc*/
#define MAX_NUM_AKM_SUITES    16

/** WAPI AUTH mode definition */
enum wapi_auth_mode {
	WAPI_AUTH_MODE_OPEN = 0,
	WAPI_AUTH_MODE_PSK = 1,
	WAPI_AUTH_MODE_CERT
} __packed;

#define WPA_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))
#define WPA_GET_BE24(a) ((u32) ((a[0] << 16) | (a[1] << 8) | a[2]))
#define WAPI_PSK_AKM_SUITE  0x02721400
#define WAPI_CERT_AKM_SUITE 0x01721400

/**
 * struct hdd_wapi_info - WAPI Information structure definition
 * @wapi_mode: Is WAPI enabled on this adapter?
 * @is_wapi_sta: Is the STA associated with WAPI?
 * @wapi_auth_mode: WAPI authentication mode used by this adapter
 */
struct hdd_wapi_info {
	bool wapi_mode;
	bool is_wapi_sta;
	enum wapi_auth_mode wapi_auth_mode;
};
#endif /* FEATURE_WLAN_WAPI */

struct hdd_beacon_data {
	u8 *head;
	u8 *tail;
	u8 *proberesp_ies;
	u8 *assocresp_ies;
	int head_len;
	int tail_len;
	int proberesp_ies_len;
	int assocresp_ies_len;
	int dtim_period;
};

/**
 * struct hdd_mon_set_ch_info - Holds monitor mode channel switch params
 * @freq: Channel frequency.
 * @cb_mode: Channel bonding
 * @channel_width: Channel width 0/1/2 for 20/40/80MHz respectively.
 * @phy_mode: PHY mode
 */
struct hdd_mon_set_ch_info {
	uint32_t freq;
	uint8_t cb_mode;
	uint32_t channel_width;
	eCsrPhyMode phy_mode;
};

/**
 * struct hdd_station_ctx -- STA-specific information
 * @roam_profile: current roaming profile
 * @conn_info: current connection information
 * @cache_conn_info: prev connection info
 * @reg_phymode: reg phymode
 * @ch_info: monitor mode channel information
 * @ap_supports_immediate_power_save: Does the current AP allow our STA
 *    to immediately go into power save?
 * @user_cfg_chn_width: max channel bandwidth set by user space
 */
struct hdd_station_ctx {
	uint32_t reg_phymode;
	struct csr_roam_profile roam_profile;
	struct hdd_connection_info conn_info;
	struct hdd_connection_info cache_conn_info;
	struct hdd_mon_set_ch_info ch_info;
	bool ap_supports_immediate_power_save;
	uint8_t user_cfg_chn_width;
};

/**
 * enum bss_state - current state of the BSS
 * @BSS_STOP: BSS is stopped
 * @BSS_START: BSS is started
 */
enum bss_state {
	BSS_STOP,
	BSS_START,
};

/**
 * struct hdd_hostapd_state - hostapd-related state information
 * @bss_state: Current state of the BSS
 * @qdf_event: Event to synchronize actions between hostapd thread and
 *    internal callback threads
 * @qdf_stop_bss_event: Event to synchronize Stop BSS. When Stop BSS
 *    is issued userspace thread can wait on this event. The event will
 *    be set when the Stop BSS processing in UMAC has completed.
 * @qdf_sta_disassoc_event: Event to synchronize STA Disassociation.
 *    When a STA is disassociated userspace thread can wait on this
 *    event. The event will be set when the STA Disassociation
 *    processing in UMAC has completed.
 * @qdf_sta_eap_frm_done_event: Event to synchronize P2P GO disassoc
 *    frame and EAP frame.
 * @qdf_status: Used to communicate state from other threads to the
 *    userspace thread.
 */
struct hdd_hostapd_state {
	enum bss_state bss_state;
	qdf_event_t qdf_event;
	qdf_event_t qdf_stop_bss_event;
	qdf_event_t qdf_sta_disassoc_event;
	qdf_event_t qdf_sta_eap_frm_done_event;
	QDF_STATUS qdf_status;
};

/**
 * enum bss_stop_reason - reasons why a BSS is stopped.
 * @BSS_STOP_REASON_INVALID: no reason specified explicitly.
 * @BSS_STOP_DUE_TO_MCC_SCC_SWITCH: BSS stopped due to host
 *  driver is trying to switch AP role to a different channel
 *  to maintain SCC mode with the STA role on the same card.
 *  this usually happens when STA is connected to an external
 *  AP that runs on a different channel
 * @BSS_STOP_DUE_TO_VENDOR_CONFIG_CHAN: BSS stopped due to
 *  vendor subcmd set sap config channel
 */
enum bss_stop_reason {
	BSS_STOP_REASON_INVALID = 0,
	BSS_STOP_DUE_TO_MCC_SCC_SWITCH = 1,
	BSS_STOP_DUE_TO_VENDOR_CONFIG_CHAN = 2,
};

/**
 * struct hdd_rate_info - rate_info in HDD
 * @rate: tx/rx rate (kbps)
 * @mode: 0->11abg legacy, 1->HT, 2->VHT (refer to sir_sme_phy_mode)
 * @nss: number of streams
 * @mcs: mcs index for HT/VHT mode
 * @rate_flags: rate flags for last tx/rx
 *
 * rate info in HDD
 */
struct hdd_rate_info {
	uint32_t rate;
	uint8_t mode;
	uint8_t nss;
	uint8_t mcs;
	enum tx_rate_info rate_flags;
};

enum hdd_work_status {
	HDD_WORK_UNINITIALIZED,
	HDD_WORK_INITIALIZED,
};

/**
 * struct hdd_fw_txrx_stats - fw txrx status in HDD
 *                            (refer to station_info struct in Kernel)
 * @tx_packets: packets transmitted to this station
 * @tx_bytes: bytes transmitted to this station
 * @rx_packets: packets received from this station
 * @rx_bytes: bytes received from this station
 * @tx_retries: cumulative retry counts
 * @tx_failed: the number of failed frames
 * @tx_succeed: the number of succeed frames
 * @rssi: The signal strength (dbm)
 * @tx_rate: last used tx rate info
 * @rx_rate: last used rx rate info
 *
 * fw txrx status in HDD
 */
struct hdd_fw_txrx_stats {
	uint32_t tx_packets;
	uint64_t tx_bytes;
	uint32_t rx_packets;
	uint64_t rx_bytes;
	uint32_t tx_retries;
	uint32_t tx_failed;
	uint32_t tx_succeed;
	int8_t rssi;
	struct hdd_rate_info tx_rate;
	struct hdd_rate_info rx_rate;
};

/**
 * struct hdd_ap_ctx - SAP/P2PGO specific information
 * @hostapd_state: state control information
 * @dfs_cac_block_tx: Is data tramsmission blocked due to DFS CAC?
 * @ap_active: Are any stations active?
 * @disable_intrabss_fwd: Prevent forwarding between stations
 * @broadcast_sta_id: Station ID assigned after BSS starts
 * @privacy: The privacy bits of configuration
 * @encryption_type: The encryption being used
 * @group_key: Group Encryption Key
 * @wep_key: WEP key array
 * @wep_def_key_idx: WEP default key index
 * @sap_context: Pointer to context maintained by SAP (opaque to HDD)
 * @sap_config: SAP configuration
 * @operating_chan_freq: channel upon which the SAP is operating
 * @beacon: Beacon information
 * @vendor_acs_timer: Timer for ACS
 * @vendor_acs_timer_initialized: Is @vendor_acs_timer initialized?
 * @bss_stop_reason: Reason why the BSS was stopped
 * @acs_in_progress: In progress acs flag for an adapter
 * @ch_switch_in_progress: channel change in progress or not
 * @client_count: client count per dot11_mode
 * @country_ie_updated: country ie is updated or not by hdd hostapd
 * @during_auth_offload: auth mgmt frame is offloading to hostapd
 * @reg_punc_bitmap: puncturing bitmap
 */
struct hdd_ap_ctx {
	struct hdd_hostapd_state hostapd_state;
	bool dfs_cac_block_tx;
	bool ap_active;
	bool disable_intrabss_fwd;
	uint8_t broadcast_sta_id;
	uint8_t privacy;
	eCsrEncryptionType encryption_type;
	uint8_t wep_def_key_idx;
	struct sap_context *sap_context;
	struct sap_config sap_config;
	uint32_t operating_chan_freq;
	struct hdd_beacon_data *beacon;
	qdf_mc_timer_t vendor_acs_timer;
	bool vendor_acs_timer_initialized;
	enum bss_stop_reason bss_stop_reason;
	qdf_atomic_t acs_in_progress;
	qdf_atomic_t ch_switch_in_progress;
	uint16_t client_count[QCA_WLAN_802_11_MODE_INVALID];
	bool country_ie_updated;
	bool during_auth_offload;
#ifdef WLAN_FEATURE_11BE
	uint16_t reg_punc_bitmap;
#endif
};

/**
 * struct hdd_scan_info - Per-adapter scan information
 * @scan_add_ie: Additional IE for scan
 * @default_scan_ies: Default scan IEs
 * @default_scan_ies_len: Length of @default_scan_ies
 * @scan_mode: Scan mode
 */
struct hdd_scan_info {
	tSirAddie scan_add_ie;
	uint8_t *default_scan_ies;
	uint16_t default_scan_ies_len;
	tSirScanType scan_mode;
};

#define WLAN_HDD_MAX_MC_ADDR_LIST CFG_TGT_MAX_MULTICAST_FILTER_ENTRIES

struct hdd_multicast_addr_list {
	uint8_t mc_cnt;
	uint8_t addr[WLAN_HDD_MAX_MC_ADDR_LIST][ETH_ALEN];
};

#define WLAN_HDD_MAX_HISTORY_ENTRY 25

/**
 * struct hdd_netif_queue_stats - netif queue operation statistics
 * @pause_count: pause counter
 * @unpause_count: unpause counter
 * @total_pause_time: amount of time in paused state
 */
struct hdd_netif_queue_stats {
	u32 pause_count;
	u32 unpause_count;
	qdf_time_t total_pause_time;
};

/**
 * struct hdd_netif_queue_history - netif queue operation history
 * @time: timestamp
 * @netif_action: action type
 * @netif_reason: reason type
 * @pause_map: pause map
 * @tx_q_state: state of the netdev TX queues
 */
struct hdd_netif_queue_history {
	qdf_time_t time;
	uint16_t netif_action;
	uint16_t netif_reason;
	uint32_t pause_map;
	unsigned long tx_q_state[NUM_TX_QUEUES];
};

/**
 * struct hdd_chan_change_params - channel related information
 * @chan_freq: operating channel frequency
 * @chan_params: channel parameters
 */
struct hdd_chan_change_params {
	uint32_t chan_freq;
	struct ch_params chan_params;
};

/**
 * struct hdd_runtime_pm_context - context to prevent/allow runtime pm
 * @dfs: dfs context to prevent/allow runtime pm
 * @connect: connect context to prevent/allow runtime pm
 * @user: user context to prevent/allow runtime pm
 * @is_user_wakelock_acquired: boolean to check if user wakelock status
 * @monitor_mode: monitor mode context to prevent/allow runtime pm
 * @wow_unit_test: wow unit test mode context to prevent/allow runtime pm
 * @system_suspend: system suspend context to prevent/allow runtime pm
 * @dyn_mac_addr_update: update mac addr context to prevent/allow runtime pm
 * @vdev_destroy: vdev destroy context to prevent/allow runtime pm
 * @oem_data_cmd: OEM data context to prevent/allow runtime pm
 *
 * Runtime PM control for underlying activities
 */
struct hdd_runtime_pm_context {
	qdf_runtime_lock_t dfs;
	qdf_runtime_lock_t connect;
	qdf_runtime_lock_t user;
	bool is_user_wakelock_acquired;
	qdf_runtime_lock_t monitor_mode;
	qdf_runtime_lock_t wow_unit_test;
	qdf_runtime_lock_t system_suspend;
	qdf_runtime_lock_t dyn_mac_addr_update;
	qdf_runtime_lock_t vdev_destroy;
	qdf_runtime_lock_t oem_data_cmd;
};

/*
 * WLAN_HDD_ADAPTER_MAGIC is a magic number used to identify net devices
 * belonging to this driver from net devices belonging to other devices.
 * Therefore, the magic number must be unique relative to the numbers for
 * other drivers in the system. If WLAN_HDD_ADAPTER_MAGIC is already defined
 * (e.g. by compiler argument), then use that. If it's not already defined,
 * then use the first 4 characters of MULTI_IF_NAME to construct the magic
 * number. If MULTI_IF_NAME is not defined, then use a default magic number.
 */
#ifndef WLAN_HDD_ADAPTER_MAGIC
#ifdef MULTI_IF_NAME
#define WLAN_HDD_ADAPTER_MAGIC                                          \
	(MULTI_IF_NAME[0] == 0 ? 0x574c414e :                           \
	(MULTI_IF_NAME[1] == 0 ? (MULTI_IF_NAME[0] << 24) :             \
	(MULTI_IF_NAME[2] == 0 ? (MULTI_IF_NAME[0] << 24) |             \
		(MULTI_IF_NAME[1] << 16) :                              \
	(MULTI_IF_NAME[0] << 24) | (MULTI_IF_NAME[1] << 16) |           \
	(MULTI_IF_NAME[2] << 8) | MULTI_IF_NAME[3])))
#else
#define WLAN_HDD_ADAPTER_MAGIC 0x574c414e       /* ASCII "WLAN" */
#endif
#endif

/**
 * struct rcpi_info - rcpi info
 * @rcpi: computed value in dB
 * @mac_addr: peer mac addr for which rcpi is computed
 */
struct rcpi_info {
	int32_t rcpi;
	struct qdf_mac_addr mac_addr;
};

struct hdd_context;

#ifdef MULTI_CLIENT_LL_SUPPORT
/* Max host clients which can request the FW arbiter with the latency level */
#define WLM_MAX_HOST_CLIENT 5

/**
 * struct wlm_multi_client_info_table - To store multi client id information
 * @client_id: host id for a client
 * @port_id: client id coming from upper layer
 * @in_use: set true for a client when host receives vendor cmd for that client
 */
struct wlm_multi_client_info_table {
	uint32_t client_id;
	uint32_t port_id;
	bool in_use;
};
#endif

/**
 * enum udp_qos_upgrade - Enumeration of the various User priority (UP) types
 *			  UDP QoS upgrade request
 * @UDP_QOS_UPGRADE_NONE: Do not upgrade UDP QoS AC
 * @UDP_QOS_UPGRADE_BK_BE: Upgrade UDP QoS for BK/BE only
 * @UDP_QOS_UPGRADE_ALL: Upgrade UDP QoS for all packets
 * @UDP_QOS_UPGRADE_MAX: Max enum limit, not to add new beyond this
 */
enum udp_qos_upgrade {
	UDP_QOS_UPGRADE_NONE,
	UDP_QOS_UPGRADE_BK_BE,
	UDP_QOS_UPGRADE_ALL,
	UDP_QOS_UPGRADE_MAX
};

#define WLAN_HDD_DEFLINK_IDX	0

/**
 * struct wlan_hdd_link_info - Data structure to store the link specific info
 * @adapter: Reverse pointer to HDD adapter
 * @vdev_id: Unique value to identify VDEV. Equal to WLAN_UMAC_VDEV_ID_MAX
 *           for invalid VDEVs.
 * @vdev_lock: Lock to protect VDEV pointer access.
 * @vdev: Pointer to VDEV objmgr.
 * @vdev_destroy_event: vdev_destroy_event is moved from the qdf_event
 *                      to linux event consciously, Lets take example
 *                      when sap interface is waiting on the
 *                      session_close event and then there is a SSR
 *                      the wait event is completed the interface down
 *                      is returned and the next command to the driver
 *                      will be hdd_hostapd_uinit-->
 *                      hdd_deinit_ap_mode-->
 *                      hdd_hostapd_deinit_sap_session where in the
 *                      sap_ctx would be freed.  During the SSR if the
 *                      same sap context is used it would result in
 *                      null pointer de-reference.
 * @link_addr: Link MAC address
 * @session: union of @ap and @station specific structs
 * @session.station: station mode information
 * @session.ap: ap mode specific information
 * @acs_complete_event: acs complete event
 * @rssi: The signal strength (dBm)
 * @snr: SNR measured from @rssi
 * @rssi_on_disconnect: Rssi at disconnection time in STA mode
 * @rssi_send: Notify RSSI over lpass
 * @is_mlo_vdev_active: is the mlo vdev currently active
 * @estimated_linkspeed: estimated link speed
 * @hdd_stats: HDD statistics
 * @big_data_stats: Big data stats
 * @ll_iface_stats: Link Layer interface stats
 * @hdd_sinfo: hdd vdev station stats that will be sent to userspace
 * @mlo_peer_info: mlo peer stats info
 * @mscs_prev_tx_vo_pkts: count of prev VO AC packets transmitted
 * @mscs_counter: Counter on MSCS action frames sent
 * @link_flags: a bitmap of hdd_link_flags
 * @chan_change_notify_work: Channel change notify work
 */
struct wlan_hdd_link_info {
	struct hdd_adapter *adapter;
	uint8_t vdev_id;
	qdf_spinlock_t vdev_lock;
	struct wlan_objmgr_vdev *vdev;
	struct completion vdev_destroy_event;
	struct qdf_mac_addr link_addr;

	union {
		struct hdd_station_ctx station;
		struct hdd_ap_ctx ap;
	} session;

	qdf_event_t acs_complete_event;

	int8_t rssi;
	uint8_t snr;
	int32_t rssi_on_disconnect;
#ifdef WLAN_FEATURE_LPSS
	bool rssi_send;
#endif
	bool is_mlo_vdev_active;
	uint32_t estimated_linkspeed;
	struct hdd_stats hdd_stats;
#ifdef WLAN_FEATURE_BIG_DATA_STATS
	struct big_data_stats_event big_data_stats;
#endif
#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
	struct wifi_interface_stats ll_iface_stats;
	struct wlan_hdd_station_stats_info hdd_sinfo;
	struct wlan_hdd_peer_info mlo_peer_info;
#endif

#ifdef WLAN_FEATURE_MSCS
	unsigned long mscs_prev_tx_vo_pkts;
	uint32_t mscs_counter;
#endif /* WLAN_FEATURE_MSCS */

	unsigned long link_flags;
	qdf_work_t chan_change_notify_work;
};

/**
 * struct wlan_hdd_tx_power - Structure to store connection tx power info
 * @tx_pwr: connection tx power sent by firmware
 * @tx_pwr_cached_timestamp: timestamp when tx_pwr is cached into adapter
 */
struct wlan_hdd_tx_power {
	int tx_pwr;
	uint32_t tx_pwr_cached_timestamp;
};

/**
 * struct hdd_adapter - hdd vdev/net_device context
 * @magic: Magic cookie for adapter sanity verification.  Note that this
 *         needs to be at the beginning of the private data structure so
 *         that it will exist at the beginning of dev->priv and hence
 *         will always be in mapped memory
 * @node: list node for membership in the adapter list
 * @hdd_ctx:
 * @dev: Handle to the network device
 * @device_mode:
 * @ipv4_notifier_work: IPv4 notifier callback for handling ARP offload on
 *                      change in IP
 * @ipv6_notifier_work: IPv6 notifier callback for handling NS offload on
 *                      change in IP
 * @wdev: TODO Move this to sta Ctx
 * @ops: ops checks if Opportunistic Power Save is Enable or Not
 * @ctw: stores CT Window value once we receive Opps command from
 *       wpa_supplicant then using CT Window value we need to Enable
 *       Opportunistic Power Save
 * @allow_power_save: STA/CLI powersave enable/disable from userspace
 * @mac_addr: Current MAC Address for the adapter
 * @mld_addr: MLD address for adapter
 * @event_flags: a bitmap of hdd_adapter_flags
 * @curr_link_info_map: Current mapping of link info in adapter array
 * @active_links: a bitmap of active links in @link_info array
 * @num_links_on_create: No of active links set on initial hdd_open_adapter().
 * @is_ll_stats_req_pending: atomic variable to check active stats req
 * @sta_stats_cached_timestamp: last updated stats timestamp
 * @qdf_monitor_mode_vdev_up_event: QDF event for monitor mode vdev up
 * @disconnect_comp_var: completion variable for disconnect callback
 * @linkup_event_var: completion variable for Linkup Event
 * @is_link_up_service_needed: Track whether the linkup handling is needed
 * @hdd_wmm_status: WMM Status
 * @sta_info:
 * @cache_sta_info:
 * @sta_info_list:
 * @cache_sta_info_list:
 * @cache_sta_count: number of currently cached stations
 * @wapi_info:
 * @sap_stop_bss_work:
 * @tsf: structure containing tsf related information
 * @mc_addr_list: multicast address list
 * @mc_list_lock: spin lock for multicast list
 * @addr_filter_pattern:
 * @scan_info:
 * @psb_changed: Flag to ensure PSB is configured through framework
 * @configured_psb: UAPSD psb value configured through framework
 * @scan_block_work:
 * @blocked_scan_request_q:
 * @blocked_scan_request_q_lock:
 * @tx_flow_control_timer:
 * @tx_flow_timer_initialized:
 * @tx_flow_low_watermark:
 * @tx_flow_hi_watermark_offset:
 * @dscp_to_up_map: DSCP to UP QoS Mapping
 * @is_link_layer_stats_set:
 * @ll_stats_failure_count:
 * @link_status:
 * @upgrade_udp_qos_threshold: The threshold for user priority upgrade for
 *			       any UDP packet.
 * @udp_qos_upgrade_type: UDP QoS packet upgrade request type
 * @temperature: variable for temperature in Celsius
 * @ocb_mac_address: MAC addresses used for OCB interfaces
 * @ocb_mac_addr_count:
 * @pause_map: BITMAP indicating pause reason
 * @subqueue_pause_map:
 * @pause_map_lock:
 * @start_time:
 * @last_time:
 * @total_pause_time:
 * @total_unpause_time:
 * @history_index:
 * @queue_oper_history:
 * @queue_oper_stats:
 * @debugfs_phy: debugfs entry
 * @lfr_fw_status:
 * @active_ac:
 * @mon_chan_freq:
 * @mon_bandwidth:
 * @latency_level: 0 - normal, 1 - xr, 2 - low, 3 - ultralow
 * @multi_client_ll_support: to check multi client ll support in driver
 * @client_info: To store multi client id information
 * @multi_ll_response_cookie: cookie for multi client ll command
 * @multi_ll_req_in_progress: to check multi client ll request in progress
 * @multi_ll_resp_expected: to decide whether host will wait for multi client
 *                          event or not
 * @monitor_mode_vdev_up_in_progress:
 * @rcpi: rcpi information
 * @send_mode_change:
 * @apf_context:
 * @csr_file:
 * @motion_detection_mode:
 * @motion_det_cfg:
 * @motion_det_in_progress:
 * @motion_det_baseline_value:
 * @last_disconnect_reason: Last disconnected internal reason code
 * as per enum qca_disconnect_reason_codes
 * @connect_req_status: Last disconnected internal status code
 *                          as per enum qca_sta_connect_fail_reason_codes
 * @peer_cleanup_done:
 * @oem_data_in_progress:
 * @cookie:
 * @response_expected:
 * @handle_feature_update: Handle feature update only if it is triggered
 *			   by hdd_netdev_feature_update
 * @tso_csum_feature_enabled: Indicate if TSO and checksum offload features
 *                            are enabled or not
 * @netdev_features_update_work: work for handling the netdev features update
 * for the adapter.
 * @netdev_features_update_work_status: status for netdev_features_update_work
 * @net_dev_hold_ref_count:
 * @delete_in_progress: Flag to indicate that the adapter delete is in
 * progress, and any operation using rtnl lock inside
 * the driver can be avoided/skipped.
 * @is_virtual_iface: Indicates that netdev is called from virtual interface
 * @mon_adapter: hdd_adapter of monitor mode.
 * @mlo_adapter_info:
 * @set_mac_addr_req_ctx: Set MAC address command request context
 * @delta_qtime: delta between host qtime and monotonic time
 * @traffic_end_ind_en: traffic end indication feature enable/disable
 * @is_dbam_configured:
 * @user_phy_mode: phy mode is set per vdev
 * @deflink: Default link pointing to the 0th index of the linkinfo array
 * @link_info: Data structure to hold link specific information
 * @tx_power: Structure to hold connection tx Power info
 * @tx_latency_cfg: configuration for per-link transmit latency statistics
 * @link_state_cached_timestamp: link state cached timestamp
 */
struct hdd_adapter {
	uint32_t magic;
	qdf_list_node_t node;

	struct hdd_context *hdd_ctx;

	struct net_device *dev;

	enum QDF_OPMODE device_mode;

	struct work_struct ipv4_notifier_work;
#ifdef WLAN_NS_OFFLOAD
	/* IPv6 notifier callback for handling NS offload on change in IP */
	struct work_struct ipv6_notifier_work;
#endif

	/* TODO Move this to sta Ctx */
	struct wireless_dev wdev;

	uint8_t ops;
	uint32_t ctw;
	bool allow_power_save;

	struct qdf_mac_addr mac_addr;
#ifndef WLAN_HDD_MULTI_VDEV_SINGLE_NDEV
	struct qdf_mac_addr mld_addr;
#endif
	unsigned long event_flags;
	uint8_t curr_link_info_map[WLAN_MAX_ML_BSS_LINKS];
	unsigned long active_links;
	uint8_t num_links_on_create;

	qdf_atomic_t is_ll_stats_req_pending;

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
	uint32_t sta_stats_cached_timestamp;
#endif

#ifdef FEATURE_MONITOR_MODE_SUPPORT
	qdf_event_t qdf_monitor_mode_vdev_up_event;
#endif

	/* TODO: move these to sta ctx. These may not be used in AP */
	struct completion disconnect_comp_var;
	struct completion linkup_event_var;

	bool is_link_up_service_needed;

	struct hdd_wmm_status hdd_wmm_status;

	/* TODO: Will be removed as a part of next phase of clean up */
	struct hdd_station_info sta_info[WLAN_MAX_STA_COUNT];
	struct hdd_station_info cache_sta_info[WLAN_MAX_STA_COUNT];

	/* TODO: _list from name will be removed after clean up */
	struct hdd_sta_info_obj sta_info_list;
	struct hdd_sta_info_obj cache_sta_info_list;
	qdf_atomic_t cache_sta_count;

#ifdef FEATURE_WLAN_WAPI
	struct hdd_wapi_info wapi_info;
#endif

	struct work_struct  sap_stop_bss_work;

#ifdef WLAN_FEATURE_TSF
	struct hdd_vdev_tsf tsf;
#endif
	struct hdd_multicast_addr_list mc_addr_list;
	qdf_spinlock_t mc_list_lock;
	uint8_t addr_filter_pattern;

	struct hdd_scan_info scan_info;

	uint8_t psb_changed;
	uint8_t configured_psb;

	struct work_struct scan_block_work;
	qdf_list_t blocked_scan_request_q;
	qdf_mutex_t blocked_scan_request_q_lock;

#if  defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || \
				defined(QCA_HL_NETDEV_FLOW_CONTROL)
	qdf_mc_timer_t tx_flow_control_timer;
	bool tx_flow_timer_initialized;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL || QCA_HL_NETDEV_FLOW_CONTROL */
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	unsigned int tx_flow_low_watermark;
	unsigned int tx_flow_hi_watermark_offset;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

	enum sme_qos_wmmuptype dscp_to_up_map[WLAN_MAX_DSCP + 1];

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	bool is_link_layer_stats_set;
	uint8_t ll_stats_failure_count;
#endif
	uint8_t link_status;
	uint8_t upgrade_udp_qos_threshold;
	enum udp_qos_upgrade udp_qos_upgrade_type;

	int temperature;

#ifdef WLAN_FEATURE_DSRC
	struct qdf_mac_addr ocb_mac_address[QDF_MAX_CONCURRENCY_PERSONA];
	int ocb_mac_addr_count;
#endif

	uint32_t pause_map;
	uint32_t subqueue_pause_map;
	spinlock_t pause_map_lock;
	qdf_time_t start_time;
	qdf_time_t last_time;
	qdf_time_t total_pause_time;
	qdf_time_t total_unpause_time;
	uint8_t history_index;
	struct hdd_netif_queue_history
		 queue_oper_history[WLAN_HDD_MAX_HISTORY_ENTRY];
	struct hdd_netif_queue_stats queue_oper_stats[WLAN_REASON_TYPE_MAX];

	struct dentry *debugfs_phy;
	struct lfr_firmware_status lfr_fw_status;
	uint8_t active_ac;
	uint32_t mon_chan_freq;
	uint32_t mon_bandwidth;
	uint16_t latency_level;
#ifdef MULTI_CLIENT_LL_SUPPORT
	bool multi_client_ll_support;
	struct wlm_multi_client_info_table client_info[WLM_MAX_HOST_CLIENT];
	void *multi_ll_response_cookie;
	bool multi_ll_req_in_progress;
	bool multi_ll_resp_expected;
#endif
#ifdef FEATURE_MONITOR_MODE_SUPPORT
	bool monitor_mode_vdev_up_in_progress;
#endif

	struct rcpi_info rcpi;
	bool send_mode_change;
#ifdef FEATURE_WLAN_APF
	struct hdd_apf_context apf_context;
#endif /* FEATURE_WLAN_APF */

#ifdef WLAN_DEBUGFS
	struct hdd_debugfs_file_info csr_file[HDD_DEBUGFS_FILE_ID_MAX];
#endif /* WLAN_DEBUGFS */

#ifdef WLAN_FEATURE_MOTION_DETECTION
	bool motion_detection_mode;
	bool motion_det_cfg;
	bool motion_det_in_progress;
	uint32_t motion_det_baseline_value;
#endif /* WLAN_FEATURE_MOTION_DETECTION */
	enum qca_disconnect_reason_codes last_disconnect_reason;
	enum wlan_status_code connect_req_status;
	qdf_event_t peer_cleanup_done;
#ifdef FEATURE_OEM_DATA
	bool oem_data_in_progress;
	void *cookie;
	bool response_expected;
#endif
	bool handle_feature_update;

	bool tso_csum_feature_enabled;

	qdf_work_t netdev_features_update_work;
	enum hdd_work_status netdev_features_update_work_status;
	qdf_atomic_t net_dev_hold_ref_count[NET_DEV_HOLD_ID_MAX];
	bool delete_in_progress;
	bool is_virtual_iface;
#ifdef WLAN_FEATURE_PKT_CAPTURE
	struct hdd_adapter *mon_adapter;
#endif
#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
	struct hdd_mlo_adapter_info mlo_adapter_info;
#endif
#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
	void *set_mac_addr_req_ctx;
#endif
	int64_t delta_qtime;
#ifdef DP_TRAFFIC_END_INDICATION
	bool traffic_end_ind_en;
#endif
#ifdef WLAN_FEATURE_DBAM_CONFIG
	bool is_dbam_configured;
#endif
	enum qca_wlan_vendor_phy_mode user_phy_mode;
	struct wlan_hdd_link_info *deflink;
	struct wlan_hdd_link_info link_info[WLAN_MAX_ML_BSS_LINKS];
	struct wlan_hdd_tx_power tx_power;
#ifdef WLAN_FEATURE_TX_LATENCY_STATS
	struct cdp_tx_latency_config tx_latency_cfg;
#endif
#ifdef WLAN_FEATURE_11BE_MLO
	qdf_time_t link_state_cached_timestamp;
#endif
};

#define WLAN_HDD_GET_STATION_CTX_PTR(link_info) (&(link_info)->session.station)
#define WLAN_HDD_GET_AP_CTX_PTR(link_info) (&(link_info)->session.ap)
#define WLAN_HDD_GET_CTX(adapter) ((adapter)->hdd_ctx)
#define WLAN_HDD_GET_HOSTAP_STATE_PTR(link_info) \
		(&(WLAN_HDD_GET_AP_CTX_PTR((link_info))->hostapd_state))
#define WLAN_HDD_GET_SAP_CTX_PTR(link_info) \
		(WLAN_HDD_GET_AP_CTX_PTR((link_info))->sap_context)

#ifdef WLAN_FEATURE_NAN
#define WLAN_HDD_IS_NDP_ENABLED(hdd_ctx) ((hdd_ctx)->nan_datapath_enabled)
#else
/* WLAN_HDD_GET_NDP_CTX_PTR and WLAN_HDD_GET_NDP_WEXT_STATE_PTR are not defined
 * intentionally so that all references to these must be within NDP code.
 * non-NDP code can call WLAN_HDD_IS_NDP_ENABLED(), and when it is enabled,
 * invoke NDP code to do all work.
 */
#define WLAN_HDD_IS_NDP_ENABLED(hdd_ctx) (false)
#endif

/* Set mac address locally administered bit */
#define WLAN_HDD_RESET_LOCALLY_ADMINISTERED_BIT(macaddr) (macaddr[0] &= 0xFD)

#define HDD_DEFAULT_MCC_P2P_QUOTA    70
#define HDD_RESET_MCC_P2P_QUOTA      50

/*
 * struct hdd_priv_data - driver ioctl private data payload
 * @buf: pointer to command buffer (may be in userspace)
 * @used_len: length of the command/data currently in @buf
 * @total_len: total length of the @buf memory allocation
 */
struct hdd_priv_data {
	uint8_t *buf;
	int used_len;
	int total_len;
};

#define  MAX_MOD_LOGLEVEL 10
struct fw_log_info {
	uint8_t enable;
	uint8_t dl_type;
	uint8_t dl_report;
	uint8_t dl_loglevel;
	uint8_t index;
	uint32_t dl_mod_loglevel[MAX_MOD_LOGLEVEL];

};

/**
 * enum antenna_mode - number of TX/RX chains
 * @HDD_ANTENNA_MODE_INVALID: Invalid mode place holder
 * @HDD_ANTENNA_MODE_1X1: Number of TX/RX chains equals 1
 * @HDD_ANTENNA_MODE_2X2: Number of TX/RX chains equals 2
 * @HDD_ANTENNA_MODE_MAX: Place holder for max mode
 */
enum antenna_mode {
	HDD_ANTENNA_MODE_INVALID,
	HDD_ANTENNA_MODE_1X1,
	HDD_ANTENNA_MODE_2X2,
	HDD_ANTENNA_MODE_MAX
};

/**
 * enum smps_mode - SM power save mode
 * @HDD_SMPS_MODE_STATIC: Static power save
 * @HDD_SMPS_MODE_DYNAMIC: Dynamic power save
 * @HDD_SMPS_MODE_RESERVED: Reserved
 * @HDD_SMPS_MODE_DISABLED: Disable power save
 * @HDD_SMPS_MODE_MAX: Place holder for max mode
 */
enum smps_mode {
	HDD_SMPS_MODE_STATIC,
	HDD_SMPS_MODE_DYNAMIC,
	HDD_SMPS_MODE_RESERVED,
	HDD_SMPS_MODE_DISABLED,
	HDD_SMPS_MODE_MAX
};

#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
/**
 * struct hdd_offloaded_packets - request id to pattern id mapping
 * @request_id: request id
 * @pattern_id: pattern id
 *
 */
struct hdd_offloaded_packets {
	uint32_t request_id;
	uint8_t  pattern_id;
};

/**
 * struct hdd_offloaded_packets_ctx - offloaded packets context
 * @op_table: request id to pattern id table
 * @op_lock: mutex lock
 */
struct hdd_offloaded_packets_ctx {
	struct hdd_offloaded_packets op_table[MAXNUM_PERIODIC_TX_PTRNS];
	struct mutex op_lock;
};
#endif

/**
 * enum driver_modules_status - Driver Modules status
 * @DRIVER_MODULES_UNINITIALIZED: Driver CDS modules uninitialized
 * @DRIVER_MODULES_ENABLED: Driver CDS modules opened
 * @DRIVER_MODULES_CLOSED: Driver CDS modules closed
 */
enum driver_modules_status {
	DRIVER_MODULES_UNINITIALIZED,
	DRIVER_MODULES_ENABLED,
	DRIVER_MODULES_CLOSED
};

/**
 * struct acs_dfs_policy - Define ACS policies
 * @acs_dfs_mode: Dfs mode enabled/disabled.
 * @acs_chan_freq: pre defined channel frequency to avoid ACS.
 */
struct acs_dfs_policy {
	enum dfs_mode acs_dfs_mode;
	uint32_t acs_chan_freq;
};

/**
 * enum suspend_fail_reason - Reasons a WLAN suspend might fail
 * @SUSPEND_FAIL_IPA: IPA in progress
 * @SUSPEND_FAIL_RADAR: radar scan in progress
 * @SUSPEND_FAIL_ROAM: roaming in progress
 * @SUSPEND_FAIL_SCAN: scan in progress
 * @SUSPEND_FAIL_INITIAL_WAKEUP: received initial wakeup from firmware
 * @SUSPEND_FAIL_MAX_COUNT: the number of wakeup reasons, always at the end
 */
enum suspend_fail_reason {
	SUSPEND_FAIL_IPA,
	SUSPEND_FAIL_RADAR,
	SUSPEND_FAIL_ROAM,
	SUSPEND_FAIL_SCAN,
	SUSPEND_FAIL_INITIAL_WAKEUP,
	SUSPEND_FAIL_MAX_COUNT
};

/**
 * struct suspend_resume_stats - counters for suspend/resume events
 * @suspends: number of suspends completed
 * @resumes: number of resumes completed
 * @suspend_fail: counters for failed suspend reasons
 */
struct suspend_resume_stats {
	uint32_t suspends;
	uint32_t resumes;
	uint32_t suspend_fail[SUSPEND_FAIL_MAX_COUNT];
};

/**
 * enum hdd_sta_smps_param - SMPS parameters to configure from hdd
 * @HDD_STA_SMPS_PARAM_UPPER_RSSI_THRESH: RSSI threshold to enter Dynamic SMPS
 * mode from inactive mode
 * @HDD_STA_SMPS_PARAM_STALL_RSSI_THRESH:  RSSI threshold to enter
 * Stalled-D-SMPS mode from D-SMPS mode or to enter D-SMPS mode from
 * Stalled-D-SMPS mode
 * @HDD_STA_SMPS_PARAM_LOWER_RSSI_THRESH:  RSSI threshold to disable SMPS modes
 * @HDD_STA_SMPS_PARAM_UPPER_BRSSI_THRESH: Upper threshold for beacon-RSSI.
 * Used to reduce RX chainmask.
 * @HDD_STA_SMPS_PARAM_LOWER_BRSSI_THRESH:  Lower threshold for beacon-RSSI.
 * Used to increase RX chainmask.
 * @HDD_STA_SMPS_PARAM_DTIM_1CHRX_ENABLE: Enable/Disable DTIM 1chRx feature
 */
enum hdd_sta_smps_param {
	HDD_STA_SMPS_PARAM_UPPER_RSSI_THRESH = 0,
	HDD_STA_SMPS_PARAM_STALL_RSSI_THRESH = 1,
	HDD_STA_SMPS_PARAM_LOWER_RSSI_THRESH = 2,
	HDD_STA_SMPS_PARAM_UPPER_BRSSI_THRESH = 3,
	HDD_STA_SMPS_PARAM_LOWER_BRSSI_THRESH = 4,
	HDD_STA_SMPS_PARAM_DTIM_1CHRX_ENABLE = 5
};

/**
 * enum RX_OFFLOAD - Receive offload modes
 * @CFG_LRO_ENABLED: Large Rx offload
 * @CFG_GRO_ENABLED: Generic Rx Offload
 */
enum RX_OFFLOAD {
	CFG_LRO_ENABLED = 1,
	CFG_GRO_ENABLED,
};

/* One per STA: 1 for BCMC_STA_ID, 1 for each SAP_SELF_STA_ID,
 * 1 for WDS_STAID
 */
#define HDD_MAX_ADAPTERS (WLAN_MAX_STA_COUNT + QDF_MAX_NO_OF_SAP_MODE + 2)

#ifdef DISABLE_CHANNEL_LIST

/**
 * struct hdd_cache_channel_info - Structure of the channel info
 * which needs to be cached
 * @freq: frequency
 * @reg_status: Current regulatory status of the channel
 * Enable
 * Disable
 * DFS
 * Invalid
 * @wiphy_status: Current wiphy status
 */
struct hdd_cache_channel_info {
	qdf_freq_t freq;
	enum channel_state reg_status;
	uint32_t wiphy_status;
};

/**
 * struct hdd_cache_channels - Structure of the channels to be cached
 * @num_channels: Number of channels to be cached
 * @channel_info: Structure of the channel info
 */
struct hdd_cache_channels {
	uint32_t num_channels;
	struct hdd_cache_channel_info *channel_info;
};
#endif

/**
 * struct hdd_dynamic_mac - hdd structure to handle dynamic mac address changes
 * @dynamic_mac: Dynamically configured mac, this contains the mac on which
 * current interface is up
 * @is_provisioned_mac: is this mac from provisioned list
 * @bit_position: holds the bit mask position from where this mac is assigned,
 * if mac is assigned from provisioned this field contains the position from
 * provisioned_intf_addr_mask else contains the position from
 * derived_intf_addr_mask
 */
struct hdd_dynamic_mac {
	struct qdf_mac_addr dynamic_mac;
	bool is_provisioned_mac;
	uint8_t bit_position;
};

/**
 * struct hdd_fw_ver_info - FW version info structure
 * @major_spid: FW version - major spid.
 * @minor_spid: FW version - minor spid
 * @siid:       FW version - siid
 * @sub_id:     FW version - sub id
 * @rel_id:     FW version - release id
 * @crmid:      FW version - crmid
 */

struct hdd_fw_ver_info {
	uint32_t major_spid;
	uint32_t minor_spid;
	uint32_t siid;
	uint32_t sub_id;
	uint32_t rel_id;
	uint32_t crmid;
};

/*
 * The logic for get current index of history is dependent on this
 * value being power of 2.
 */
#define WLAN_HDD_ADAPTER_OPS_HISTORY_MAX 4
QDF_COMPILE_TIME_ASSERT(adapter_ops_history_size,
			(WLAN_HDD_ADAPTER_OPS_HISTORY_MAX &
			 (WLAN_HDD_ADAPTER_OPS_HISTORY_MAX - 1)) == 0);

/**
 * enum hdd_adapter_ops_event - events for adapter ops history
 * @WLAN_HDD_ADAPTER_OPS_WORK_POST: adapter ops work posted
 * @WLAN_HDD_ADAPTER_OPS_WORK_SCHED: adapter ops work scheduled
 */
enum hdd_adapter_ops_event {
	WLAN_HDD_ADAPTER_OPS_WORK_POST,
	WLAN_HDD_ADAPTER_OPS_WORK_SCHED,
};

/**
 * struct hdd_adapter_ops_record - record of adapter ops history
 * @timestamp: time of the occurrence of event
 * @event: event
 * @vdev_id: vdev id corresponding to the event
 */
struct hdd_adapter_ops_record {
	uint64_t timestamp;
	enum hdd_adapter_ops_event event;
	int vdev_id;
};

/**
 * struct hdd_adapter_ops_history - history of adapter ops
 * @index: index to store the next event
 * @entry: array of events
 */
struct hdd_adapter_ops_history {
	qdf_atomic_t index;
	struct hdd_adapter_ops_record entry[WLAN_HDD_ADAPTER_OPS_HISTORY_MAX];
};

/**
 * struct hdd_dual_sta_policy - Concurrent STA policy configuration
 * @dual_sta_policy: Possible values are defined in enum
 * qca_wlan_concurrent_sta_policy_config
 * @primary_vdev_id: specified iface is the primary STA iface, say 0 means
 * vdev 0 is acting as primary interface
 */
struct hdd_dual_sta_policy {
	uint8_t dual_sta_policy;
	uint8_t primary_vdev_id;
};

#ifdef FEATURE_CNSS_HW_SECURE_DISABLE
/**
 * hdd_get_wlan_driver_status() - get status of soft driver unload
 *
 * Return: true if wifi is disabled by soft driver unload, else false
 */
bool hdd_get_wlan_driver_status(void);
#else
static inline bool hdd_get_wlan_driver_status(void)
{
	return false;
}
#endif

/**
 * struct hdd_lpc_info - Local packet capture information
 * @lpc_wk: local packet capture work
 * @lpc_wk_scheduled: flag to indicate if lpc work is scheduled or not
 * @mon_adapter: monitor adapter
 */
struct hdd_lpc_info {
	qdf_work_t lpc_wk;
	bool lpc_wk_scheduled;
	struct hdd_adapter *mon_adapter;
};

/**
 * enum wlan_state_ctrl_str_id - state control param string id
 * @WLAN_OFF_STR: Turn OFF WiFi
 * @WLAN_ON_STR: Turn ON WiFi
 * @WLAN_ENABLE_STR: Enable WiFi
 * @WLAN_DISABLE_STR: Disable Wifi
 * @WLAN_WAIT_FOR_READY_STR: Driver should wait for ongoing recovery
 * @WLAN_FORCE_DISABLE_STR: Disable Wifi by soft driver unload
 */
enum wlan_state_ctrl_str_id {
	WLAN_OFF_STR   = 0,
	WLAN_ON_STR,
	WLAN_ENABLE_STR,
	WLAN_DISABLE_STR,
	WLAN_WAIT_FOR_READY_STR,
	WLAN_FORCE_DISABLE_STR
};

#define MAX_TGT_HW_NAME_LEN 32

/**
 * struct hdd_context - hdd shared driver and psoc/device context
 * @psoc: object manager psoc context
 * @pdev: object manager pdev context
 * @mac_handle: opaque handle to MAC context
 * @wiphy: Linux wiphy
 * @hdd_adapter_lock: lock for @hdd_adapters
 * @hdd_adapters: list of all instantiated adapters
 * @is_therm_cmd_supp: get temperature command enable or disable
 * @fw: pointer to firmware image data
 * @cfg: pointer to configuration data
 * @parent_dev: pointer to parent device
 * @config: Config values read from qcom_cfg.ini file
 * @channels_2ghz: pointer for wiphy 2 GHz channels
 * @channels_5ghz: pointer for wiphy 5 GHz channels
 * @iftype_data_2g: Interface data for 2 GHz band
 * @iftype_data_5g: Interface data for 5 GHz band
 * @iftype_data_6g: Interface data for 6 GHz band
 * @mc_sus_event_var: Completion variable to indicate Mc Thread Suspended
 * @is_scheduler_suspended: true if the MC Thread is suspended
 * @is_ol_rx_thread_suspended: true if the RX Thread is suspended
 * @hdd_wlan_suspended: true if the HDD is suspended
 * @suspended: unused???
 * @is_pktlog_enabled: true if pktlog is enabled, used to start pktlog after
 *                     SSR/PDR if previously enabled
 * @sap_lock: Lock to avoid race condition during start/stop bss
 * @oem_app_registered: OEM App registered or not
 * @oem_pid: OEM App Process ID when registered
 * @concurrency_mode: Concurrency Parameters
 * @no_of_open_sessions: number of open sessions per operating mode
 * @no_of_active_sessions: number of active sessions per operating mode
 * @p2p_device_address: P2P Device MAC Address for the adapter
 * @sap_wake_lock: Soft AP wakelock
 * @is_wiphy_suspended: Flag keeps track of wiphy suspend/resume
 * @ready_to_suspend: completed when ready to suspend
 * @target_type: defining the solution type
 * @target_fw_version: firmware version
 * @target_fw_vers_ext: firmware version extension
 * @fw_version_info: detailed firmware version information
 * @target_hw_version:  the chip/rom version
 * @target_hw_revision: the chip/rom revision
 * @target_hw_name: chip/rom name
 * @reg: regulatory information
 * @unsafe_channel_count: number of unsafe channels
 * @unsafe_channel_list: list of unsafe channels
 * @restriction_mask: channel avoidance restrictions mask
 * @max_intf_count: maximum number of supported interfaces
 * @lpss_support: Is LPSS offload supported
 * @ap_arpns_support: Is AP ARP/NS offload supported
 * @ioctl_scan_mode: scan mode
 * @sta_ap_intf_check_work: workqueue for interface check
 * @dev_dfs_cac_status: DFS CAC status
 * @bt_coex_mode_set: Has BT coex mode been set
 * @skip_acs_scan_timer: timer used to skip ACS scan
 * @skip_acs_scan_status: status of skip ACS scan
 * @last_acs_freq_list: ACS frequency list
 * @num_of_channels: number of channels in @last_acs_freq_list
 * @acs_skip_lock: use to synchronize "skip ACS scan" feature
 * @sap_dfs_wakelock : SAP DFS wakelock
 * @sap_dfs_ref_cnt: SAP DFS reference count
 * @is_extwow_app_type1_param_set: is extwow app type1 param set
 * @is_extwow_app_type2_param_set: is extwow app type2 param set
 * @ext_scan_start_since_boot: Time since boot up to extscan start (in micro
 *                             seconds)
 * @miracast_value: value of driver miracast command
 * @ipv6_notifier: IPv6 notifier callback for handling NS offload on change
 *                 in IP
 * @ns_offload_enable: Is NS offload enabled
 * @ipv4_notifier: IPv4 notifier callback for handling ARP offload on change
 *                 in IP
 * @pm_qos_notifier: Device PM QoS notifier
 * @runtime_pm_prevented: Is PM prevented
 * @pm_qos_lock: Lock for PM QoS data
 * @num_rf_chains: number of rf chains supported by target
 * @ht_tx_stbc_supported: Is HT Tx STBC supported by target
 * @op_ctx: Offloaded packets context
 * @mcc_mode: Is Multi-channel Concurrency enabled
 * @memdump_lock: Lock for memdump data
 * @driver_dump_size: Size of the memdump data buffer
 * @driver_dump_mem: memdump data buffer
 * @connection_in_progress: Is connection in progress
 * @connection_status_lock: Lock for connection status
 * @fine_time_meas_cap_target: place to store FTM capab of target. This
 *                             allows changing of FTM capab at runtime
 *                             and intersecting it with target capab before
 *                             updating.
 * @current_antenna_mode: Current number of TX X RX chains being used
 * @radio_index: the radio index assigned by cnss_logger
 * @hbw_requested: Has high bandwidth been requested
 * @pm_qos_request: Is PM QoS requested
 * @nan_datapath_enabled: Is NAN datapath enabled
 * @driver_status: Present state of driver cds modules
 * @psoc_idle_timeout_work: delayed work for psoc idle shutdown
 * @pm_notifier: PM notifier of hdd modules
 * @acs_policy: ACS DFS policy
 * @wmi_max_len: MTU of the WMI interface
 * @suspend_resume_stats: Suspend/Resume statistics
 * @runtime_context: Runtime PM context
 * @chan_info: scan channel information
 * @chan_info_lock: lock for @chan_info
 * @tdls_source_bitmap: bit map to set/reset TDLS by different sources
 * @tdls_umac_comp_active: Is the TDLS component active
 * @tdls_nap_active: Is napier specific tdls data path enabled
 * @beacon_probe_rsp_cnt_per_scan:
 * @last_scan_reject_vdev_id:
 * @last_scan_reject_reason:
 * @last_scan_reject_timestamp:
 * @scan_reject_cnt:
 * @dfs_cac_offload:
 * @reg_offload:
 * @rcpi_enabled:
 * @coex_avoid_freq_list:
 * @dnbs_avoid_freq_list:
 * @avoid_freq_lock:  Lock to control access to dnbs and coex avoid freq list
 * @tsf: structure containing tsf related information
 * @bt_a2dp_active:
 * @bt_vo_active:
 * @bt_profile_con:
 * @curr_band:
 * @imps_enabled:
 * @user_configured_pkt_filter_rules:
 * @is_fils_roaming_supported:
 * @receive_offload_cb:
 * @vendor_disable_lro_flag:
 * @force_rsne_override:
 * @monitor_mode_wakelock:
 * @lte_coex_ant_share:
 * @obss_scan_offload:
 * @sscan_pid:
 * @track_arp_ip:
 * @hw_bd_id: defining the board related information
 * @hw_bd_info:
 * @twt_state:
 * @twt_disable_comp_evt:
 * @twt_enable_comp_evt:
 * @apf_version:
 * @apf_enabled_v2:
 * @original_channels:
 * @cache_channel_lock:
 * @sar_version:
 * @dynamic_mac_list:
 * @dynamic_nss_chains_support: Per vdev dynamic nss chains update capability
 * @hw_macaddr:
 * @provisioned_mac_addr:
 * @derived_mac_addr:
 * @num_provisioned_addr:
 * @num_derived_addr:
 * @provisioned_intf_addr_mask:
 * @derived_intf_addr_mask:
 * @sar_cmd_params: SAR command params to be configured to the FW
 * @sar_safety_timer:
 * @sar_safety_unsolicited_work:
 * @sar_safety_req_resp_event:
 * @sar_safety_req_resp_event_in_progress:
 * @runtime_resume_start_time_stamp:
 * @runtime_suspend_done_time_stamp:
 * @pm_qos_req:
 * @qos_cpu_mask: voted cpu core mask
 * @pm_qos_req: pm_qos request for all cpu cores
 * @roam_ch_from_fw_supported:
 * @dutycycle_off_percent:
 * @pm_qos_request_flags:
 * @country_change_work: work for updating vdev when country changes
 * @current_pcie_gen_speed: current pcie gen speed
 * @adapter_ops_wq: High priority workqueue for handling adapter operations
 * @adapter_ops_history:
 * @ll_stats_per_chan_rx_tx_time:
 * @is_get_station_clubbed_in_ll_stats_req:
 * @multi_client_thermal_mitigation: Multi client thermal mitigation by fw
 * @is_dual_mac_cfg_updated: indicate whether dual mac cfg has been updated
 * @is_regulatory_update_in_progress:
 * @regulatory_update_event:
 * @regulatory_status_lock:
 * @is_fw_dbg_log_levels_configured:
 * @twt_en_dis_work: work to send twt enable/disable cmd on MCC/SCC concurrency
 * @is_wifi3_0_target:
 * @dump_in_progress: Stores value of dump in progress
 * @dual_sta_policy: Concurrent STA policy configuration
 * @is_therm_stats_in_progress:
 * @is_vdev_macaddr_dynamic_update_supported:
 * @power_type:
 * @is_wlan_disabled: if wlan is disabled by userspace
 * @oem_data:
 * @oem_data_len:
 * @file_name:
 * @dbam_mode:
 * @last_pagefault_ssr_time: Time when last recovery was triggered because of
 * @host wakeup from fw with reason as pagefault
 * @bridgeaddr: Bridge MAC address
 * @is_mlo_per_link_stats_supported: Per link mlo stats is supported or not
 * @num_mlo_peers: Total number of MLO peers
 * @more_peer_data: more mlo peer data in peer stats
 * @lpc_info: Local packet capture info
 */
struct hdd_context {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	mac_handle_t mac_handle;
	struct wiphy *wiphy;
	qdf_spinlock_t hdd_adapter_lock;
	qdf_list_t hdd_adapters;
	bool is_therm_cmd_supp;
	const struct firmware *fw;
	const struct firmware *cfg;
	struct device *parent_dev;
	struct hdd_config *config;

	/* Pointer for wiphy 2G/5G band channels */
	struct ieee80211_channel *channels_2ghz;
	struct ieee80211_channel *channels_5ghz;

#if defined(WLAN_FEATURE_11AX) && \
	(defined(CFG80211_SBAND_IFTYPE_DATA_BACKPORT) || \
	 (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)))
	struct ieee80211_sband_iftype_data *iftype_data_2g;
	struct ieee80211_sband_iftype_data *iftype_data_5g;
#if defined(CONFIG_BAND_6GHZ) && (defined(CFG80211_6GHZ_BAND_SUPPORTED) || \
		(KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE))
	struct ieee80211_sband_iftype_data *iftype_data_6g;
#endif
#endif
	struct completion mc_sus_event_var;
	bool is_scheduler_suspended;

#ifdef WLAN_DP_LEGACY_OL_RX_THREAD
	bool is_ol_rx_thread_suspended;
#endif

	bool hdd_wlan_suspended;
	bool suspended;
	bool is_pktlog_enabled;
	struct mutex sap_lock;

#ifdef FEATURE_OEM_DATA_SUPPORT
	bool oem_app_registered;
	int32_t oem_pid;
#endif

	uint32_t concurrency_mode;

	uint8_t no_of_open_sessions[QDF_MAX_NO_OF_MODE];
	uint8_t no_of_active_sessions[QDF_MAX_NO_OF_MODE];
	struct qdf_mac_addr p2p_device_address;
	qdf_wake_lock_t sap_wake_lock;
	bool is_wiphy_suspended;
	struct completion ready_to_suspend;
	uint32_t target_type;
	uint32_t target_fw_version;
	uint32_t target_fw_vers_ext;
	struct hdd_fw_ver_info fw_version_info;
	uint32_t target_hw_version;
	uint32_t target_hw_revision;
	char target_hw_name[MAX_TGT_HW_NAME_LEN];
	struct regulatory reg;
#ifdef FEATURE_WLAN_CH_AVOID
	uint16_t unsafe_channel_count;
	uint16_t unsafe_channel_list[NUM_CHANNELS];
#endif /* FEATURE_WLAN_CH_AVOID */
#ifdef FEATURE_WLAN_CH_AVOID_EXT
	uint32_t restriction_mask;
#endif

	uint8_t max_intf_count;
#ifdef WLAN_FEATURE_LPSS
	uint8_t lpss_support;
#endif
	uint8_t ap_arpns_support;
	tSirScanType ioctl_scan_mode;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	qdf_work_t sta_ap_intf_check_work;
#endif

	uint8_t dev_dfs_cac_status;

	bool bt_coex_mode_set;
#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	qdf_mc_timer_t skip_acs_scan_timer;
	uint8_t skip_acs_scan_status;
	uint32_t *last_acs_freq_list;
	uint8_t num_of_channels;
	qdf_spinlock_t acs_skip_lock;
#endif

	qdf_wake_lock_t sap_dfs_wakelock;
	atomic_t sap_dfs_ref_cnt;

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	bool is_extwow_app_type1_param_set;
	bool is_extwow_app_type2_param_set;
#endif

	uint64_t ext_scan_start_since_boot;
	uint8_t miracast_value;

#ifdef WLAN_NS_OFFLOAD
	/* IPv6 notifier callback for handling NS offload on change in IP */
	struct notifier_block ipv6_notifier;
#endif
	bool ns_offload_enable;
	/* IPv4 notifier callback for handling ARP offload on change in IP */
	struct notifier_block ipv4_notifier;

#ifdef FEATURE_RUNTIME_PM
	struct notifier_block pm_qos_notifier;
	bool runtime_pm_prevented;
	qdf_spinlock_t pm_qos_lock;
#endif
	uint32_t  num_rf_chains;
	uint8_t   ht_tx_stbc_supported;
#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
	struct hdd_offloaded_packets_ctx op_ctx;
#endif
	bool mcc_mode;
	struct mutex memdump_lock;
	uint16_t driver_dump_size;
	uint8_t *driver_dump_mem;

	bool connection_in_progress;
	qdf_spinlock_t connection_status_lock;

	uint32_t fine_time_meas_cap_target;
	enum antenna_mode current_antenna_mode;

	int radio_index;
	bool hbw_requested;
	bool pm_qos_request;
#ifdef WLAN_FEATURE_NAN
	bool nan_datapath_enabled;
#endif
	enum driver_modules_status driver_status;
	struct qdf_delayed_work psoc_idle_timeout_work;
	struct notifier_block pm_notifier;
	struct acs_dfs_policy acs_policy;
	uint16_t wmi_max_len;
	struct suspend_resume_stats suspend_resume_stats;
	struct hdd_runtime_pm_context runtime_context;
	struct scan_chan_info *chan_info;
	struct mutex chan_info_lock;
	unsigned long tdls_source_bitmap;
	bool tdls_umac_comp_active;
	bool tdls_nap_active;
	uint8_t beacon_probe_rsp_cnt_per_scan;
	uint8_t last_scan_reject_vdev_id;
	enum scan_reject_states last_scan_reject_reason;
	unsigned long last_scan_reject_timestamp;
	uint8_t scan_reject_cnt;
	bool dfs_cac_offload;
	bool reg_offload;
	bool rcpi_enabled;
#ifdef FEATURE_WLAN_CH_AVOID
	struct ch_avoid_ind_type coex_avoid_freq_list;
	struct ch_avoid_ind_type dnbs_avoid_freq_list;
	/* Lock to control access to dnbs and coex avoid freq list */
	struct mutex avoid_freq_lock;
#endif
#ifdef WLAN_FEATURE_TSF
	struct hdd_ctx_tsf tsf;
#endif

	uint8_t bt_a2dp_active:1;
	uint8_t bt_vo_active:1;
	uint8_t bt_profile_con:1;
	enum band_info curr_band;
	bool imps_enabled;
#ifdef WLAN_FEATURE_PACKET_FILTERING
	int user_configured_pkt_filter_rules;
#endif
	bool is_fils_roaming_supported;
	QDF_STATUS (*receive_offload_cb)(struct hdd_adapter *,
					 struct sk_buff *);
	qdf_atomic_t vendor_disable_lro_flag;
	bool force_rsne_override;
	qdf_wake_lock_t monitor_mode_wakelock;
	bool lte_coex_ant_share;
	bool obss_scan_offload;
	int sscan_pid;
	uint32_t track_arp_ip;

	/* defining the board related information */
	uint32_t hw_bd_id;
	struct board_info hw_bd_info;
#ifdef WLAN_SUPPORT_TWT
	enum twt_status twt_state;
	qdf_event_t twt_disable_comp_evt;
	qdf_event_t twt_enable_comp_evt;
#endif
#ifdef FEATURE_WLAN_APF
	uint32_t apf_version;
	bool apf_enabled_v2;
#endif

#ifdef DISABLE_CHANNEL_LIST
	struct hdd_cache_channels *original_channels;
	qdf_mutex_t cache_channel_lock;
#endif
	enum sar_version sar_version;
	struct hdd_dynamic_mac dynamic_mac_list[QDF_MAX_CONCURRENCY_PERSONA];
	bool dynamic_nss_chains_support;
	struct qdf_mac_addr hw_macaddr;
	struct qdf_mac_addr provisioned_mac_addr[QDF_MAX_CONCURRENCY_PERSONA];
	struct qdf_mac_addr derived_mac_addr[QDF_MAX_CONCURRENCY_PERSONA];
	uint32_t num_provisioned_addr;
	uint32_t num_derived_addr;
	unsigned long provisioned_intf_addr_mask;
	unsigned long derived_intf_addr_mask;

	struct sar_limit_cmd_params *sar_cmd_params;
#ifdef SAR_SAFETY_FEATURE
	qdf_mc_timer_t sar_safety_timer;
	struct qdf_delayed_work sar_safety_unsolicited_work;
	qdf_event_t sar_safety_req_resp_event;
	qdf_atomic_t sar_safety_req_resp_event_in_progress;
#endif

	qdf_time_t runtime_resume_start_time_stamp;
	qdf_time_t runtime_suspend_done_time_stamp;
#if defined(CLD_PM_QOS) && defined(CLD_DEV_PM_QOS)
	struct dev_pm_qos_request pm_qos_req[NR_CPUS];
	struct cpumask qos_cpu_mask;
#elif defined(CLD_PM_QOS)
	struct pm_qos_request pm_qos_req;
#endif
	bool roam_ch_from_fw_supported;
#ifdef FW_THERMAL_THROTTLE_SUPPORT
	uint8_t dutycycle_off_percent;
#endif
	uint8_t pm_qos_request_flags;
	qdf_work_t country_change_work;
	int current_pcie_gen_speed;
	qdf_workqueue_t *adapter_ops_wq;
	struct hdd_adapter_ops_history adapter_ops_history;
	bool ll_stats_per_chan_rx_tx_time;
#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
	bool is_get_station_clubbed_in_ll_stats_req;
#endif
#ifdef FEATURE_WPSS_THERMAL_MITIGATION
	bool multi_client_thermal_mitigation;
#endif
	bool is_dual_mac_cfg_updated;
	bool is_regulatory_update_in_progress;
	qdf_event_t regulatory_update_event;
	qdf_mutex_t regulatory_status_lock;
	bool is_fw_dbg_log_levels_configured;
#ifdef WLAN_SUPPORT_TWT
	qdf_work_t twt_en_dis_work;
#endif
	bool is_wifi3_0_target;
	bool dump_in_progress;
	struct hdd_dual_sta_policy dual_sta_policy;
#ifdef THERMAL_STATS_SUPPORT
	bool is_therm_stats_in_progress;
#endif
#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
	bool is_vdev_macaddr_dynamic_update_supported;
#endif
#ifdef CONFIG_WLAN_FREQ_LIST
	uint8_t power_type;
#endif
	bool is_wlan_disabled;

	uint8_t oem_data[HDD_MAX_OEM_DATA_LEN];
	uint8_t oem_data_len;
	uint8_t file_name[HDD_MAX_FILE_NAME_LEN];
#ifdef WLAN_FEATURE_DBAM_CONFIG
	enum coex_dbam_config_mode dbam_mode;
#endif
	qdf_time_t last_pagefault_ssr_time;
	uint8_t bridgeaddr[QDF_MAC_ADDR_SIZE];
#ifdef WLAN_FEATURE_11BE_MLO
	bool is_mlo_per_link_stats_supported;
	uint8_t num_mlo_peers;
	uint32_t more_peer_data;
#endif
#ifdef WLAN_FEATURE_LOCAL_PKT_CAPTURE
	struct hdd_lpc_info lpc_info;
#endif
};

/**
 * struct hdd_vendor_acs_chan_params - vendor acs channel parameters
 * @pcl_count: pcl list count
 * @vendor_pcl_list: pointer to pcl frequency (MHz) list
 * @vendor_weight_list: pointer to pcl weight list
 */
struct hdd_vendor_acs_chan_params {
	uint32_t pcl_count;
	uint32_t *vendor_pcl_list;
	uint8_t *vendor_weight_list;
};

/**
 * struct hdd_external_acs_timer_context - acs timer context
 * @reason: reason for acs trigger
 * @adapter: hdd adapter for acs
 */
struct hdd_external_acs_timer_context {
	int8_t reason;
	struct hdd_adapter *adapter;
};

/**
 * struct hdd_vendor_chan_info - vendor channel info
 * @band: channel operating band
 * @pri_chan_freq: primary channel freq in MHz
 * @ht_sec_chan_freq: secondary channel freq in MHz
 * @vht_seg0_center_chan_freq: segment0 for vht in MHz
 * @vht_seg1_center_chan_freq: vht segment 1 in MHz
 * @chan_width: channel width
 */
struct hdd_vendor_chan_info {
	uint8_t band;
	uint32_t pri_chan_freq;
	uint32_t ht_sec_chan_freq;
	uint32_t vht_seg0_center_chan_freq;
	uint32_t vht_seg1_center_chan_freq;
	uint8_t chan_width;
};

/**
 * struct  hdd_channel_info - standard channel info
 * @freq: Freq in Mhz
 * @flags: channel info flags
 * @flagext: extended channel info flags
 * @ieee_chan_number: channel number
 * @max_reg_power: max tx power according to regulatory
 * @max_radio_power: max radio power
 * @min_radio_power: min radio power
 * @reg_class_id: regulatory class
 * @max_antenna_gain: max antenna gain allowed on channel
 * @vht_center_freq_seg0: vht center freq segment 0
 * @vht_center_freq_seg1: vht center freq segment 1
 */
struct hdd_channel_info {
	u_int16_t freq;
	u_int32_t flags;
	u_int16_t flagext;
	u_int8_t ieee_chan_number;
	int8_t max_reg_power;
	int8_t max_radio_power;
	int8_t min_radio_power;
	u_int8_t reg_class_id;
	u_int8_t max_antenna_gain;
	u_int8_t vht_center_freq_seg0;
	u_int8_t vht_center_freq_seg1;
};

/**
 * struct hdd_chwidth_info - channel width related info
 * @sir_chwidth_valid: If nl_chan_width is valid in Sir
 * @sir_chwidth: enum eSirMacHTChannelWidth
 * @ch_bw: enum hw_mode_bandwidth
 * @ch_bw_str: ch_bw in string format
 * @phy_chwidth: enum phy_ch_width
 * @bonding_mode: WNI_CFG_CHANNEL_BONDING_MODE_DISABLE or
 *		  WNI_CFG_CHANNEL_BONDING_MODE_ENABLE
 */
struct hdd_chwidth_info {
	bool sir_chwidth_valid;
	enum eSirMacHTChannelWidth sir_chwidth;
	enum hw_mode_bandwidth ch_bw;
	char *ch_bw_str;
	enum phy_ch_width phy_chwidth;
	int bonding_mode;
};

/**
 * struct mac_addr_set_priv: Set MAC addr private context
 * @fw_resp_status: F/W response status
 * @pending_rsp_cnt: Pending response count
 */
struct mac_addr_set_priv {
	uint32_t fw_resp_status;
	qdf_atomic_t pending_rsp_cnt;
};

/**
 * struct mlo_ulmu_config: Set ULMU MLO config
 * @vdev_id: vdev id calculated from link info
 * @ulmu: ulmu value from command params
 */
struct mlo_ulmu_config {
	uint8_t vdev_id;
	uint8_t ulmu;
};

/*
 * Function declarations and documentation
 */

/**
 * wlan_hdd_history_get_next_index() - get next index to store the history
 *				       entry
 * @curr_idx: current index
 * @max_entries: max entries in the history
 *
 * Returns: The index at which record is to be stored in history
 */
static inline uint32_t wlan_hdd_history_get_next_index(qdf_atomic_t *curr_idx,
						       uint32_t max_entries)
{
	uint32_t idx = qdf_atomic_inc_return(curr_idx);

	return idx & (max_entries - 1);
}

/**
 * hdd_adapter_ops_record_event() - record an entry in the adapter ops history
 * @hdd_ctx: pointer to hdd context
 * @event: event
 * @vdev_id: vdev id corresponding to event
 *
 * Returns: None
 */
static inline void
hdd_adapter_ops_record_event(struct hdd_context *hdd_ctx,
			     enum hdd_adapter_ops_event event,
			     int vdev_id)
{
	struct hdd_adapter_ops_history *adapter_hist;
	struct hdd_adapter_ops_record *record;
	uint32_t idx;

	adapter_hist = &hdd_ctx->adapter_ops_history;

	idx = wlan_hdd_history_get_next_index(&adapter_hist->index,
					      WLAN_HDD_ADAPTER_OPS_HISTORY_MAX);

	record = &adapter_hist->entry[idx];
	record->event = event;
	record->vdev_id = vdev_id;
	record->timestamp = qdf_get_log_timestamp();
}

/**
 * hdd_validate_channel_and_bandwidth() - Validate the channel-bandwidth combo
 * @adapter: HDD adapter
 * @chan_freq: Channel frequency
 * @chan_bw: Bandwidth
 *
 * Checks if the given bandwidth is valid for the given channel number.
 *
 * Return: 0 for success, non-zero for failure
 */
int hdd_validate_channel_and_bandwidth(struct hdd_adapter *adapter,
				       qdf_freq_t chan_freq,
				       enum phy_ch_width chan_bw);

/**
 * hdd_get_front_adapter() - Get the first adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_front_adapter(struct hdd_context *hdd_ctx,
				 struct hdd_adapter **out_adapter);

/**
 * hdd_get_next_adapter() - Get the next adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @current_adapter: pointer to the current adapter
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_next_adapter(struct hdd_context *hdd_ctx,
				struct hdd_adapter *current_adapter,
				struct hdd_adapter **out_adapter);

/**
 * hdd_get_front_adapter_no_lock() - Get the first adapter from the adapter list
 * This API does not use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @hdd_ctx: pointer to the HDD context
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_front_adapter_no_lock(struct hdd_context *hdd_ctx,
					 struct hdd_adapter **out_adapter);

/**
 * hdd_get_next_adapter_no_lock() - Get the next adapter from the adapter list
 * This API does not use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @hdd_ctx: pointer to the HDD context
 * @current_adapter: pointer to the current adapter
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_next_adapter_no_lock(struct hdd_context *hdd_ctx,
					struct hdd_adapter *current_adapter,
					struct hdd_adapter **out_adapter);

/**
 * hdd_remove_adapter() - Remove the adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @adapter: pointer to the adapter to be removed
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_remove_adapter(struct hdd_context *hdd_ctx,
			      struct hdd_adapter *adapter);

/**
 * hdd_remove_front_adapter() - Remove the first adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @out_adapter: pointer to the adapter to be removed
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_remove_front_adapter(struct hdd_context *hdd_ctx,
				    struct hdd_adapter **out_adapter);

/**
 * hdd_add_adapter_back() - Add an adapter to the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @adapter: pointer to the adapter to be added
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_add_adapter_back(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter);

/**
 * hdd_add_adapter_front() - Add an adapter to the head of the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @adapter: pointer to the adapter to be added
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_add_adapter_front(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *adapter);

/**
 * typedef hdd_adapter_iterate_cb() - Iteration callback function
 * @link_info: Link info pointer in HDD adapter
 * @context: user context supplied to the iterator
 *
 * This specifies the type of a callback function to supply to
 * hdd_adapter_iterate().
 *
 * Return:
 * * QDF_STATUS_SUCCESS if further iteration should continue
 * * QDF_STATUS_E_ABORTED if further iteration should be aborted
 */
typedef QDF_STATUS
(*hdd_adapter_iterate_cb)(struct wlan_hdd_link_info *link_info, void *context);

/**
 * hdd_adapter_iterate() - Safely iterate over all adapters
 * @cb: callback function to invoke for each adapter
 * @context: user-supplied context to pass to @cb
 *
 * This function will iterate over all of the adapters known to the system in
 * a safe manner, invoking the callback function for each adapter.
 * The callback function will be invoked in the same context/thread as the
 * caller without any additional locks in force.
 * Iteration continues until either the callback has been invoked for all
 * adapters or a callback returns a value of QDF_STATUS_E_ABORTED to indicate
 * that further iteration should cease.
 *
 * Return:
 * * QDF_STATUS_E_ABORTED if any callback function returns that value
 * * QDF_STATUS_E_FAILURE if the callback was not invoked for all adapters due
 * to concurrency (i.e. adapter was deleted while iterating)
 * * QDF_STATUS_SUCCESS if @cb was invoked for each adapter and none returned
 * an error
 */
QDF_STATUS hdd_adapter_iterate(hdd_adapter_iterate_cb cb,
			       void *context);

/**
 * hdd_adapter_dev_hold_debug - Debug API to call dev_hold
 * @adapter: hdd_adapter pointer
 * @dbgid: Debug ID corresponding to API that is requesting the dev_hold
 *
 * Return: none
 */
void hdd_adapter_dev_hold_debug(struct hdd_adapter *adapter,
				wlan_net_dev_ref_dbgid dbgid);

/**
 * hdd_adapter_dev_put_debug - Debug API to call dev_put
 * @adapter: hdd_adapter pointer
 * @dbgid: Debug ID corresponding to API that is requesting the dev_put
 *
 * Return: none
 */
void hdd_adapter_dev_put_debug(struct hdd_adapter *adapter,
			       wlan_net_dev_ref_dbgid dbgid);

/**
 * hdd_validate_next_adapter - API to check for infinite loop
 *                             in the adapter list traversal
 * @curr: current adapter pointer
 * @next: next adapter pointer
 * @dbg_id: Debug ID corresponding to API that is requesting the dev_put
 *
 * Return: None
 */
void hdd_validate_next_adapter(struct hdd_adapter **curr,
			       struct hdd_adapter **next,
			       wlan_net_dev_ref_dbgid dbg_id);

/**
 * __hdd_take_ref_and_fetch_front_adapter_safe - Helper macro to lock, fetch
 * front and next adapters, take ref and unlock.
 * @hdd_ctx: the global HDD context
 * @adapter: an hdd_adapter pointer to use as a cursor
 * @next_adapter: hdd_adapter pointer to next adapter
 * @dbgid: debug ID to detect reference leaks
 */
#define __hdd_take_ref_and_fetch_front_adapter_safe(hdd_ctx, adapter, \
						    next_adapter, dbgid) \
	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock), \
	hdd_get_front_adapter_no_lock(hdd_ctx, &adapter), \
	(adapter) ? hdd_adapter_dev_hold_debug(adapter, dbgid) : (false), \
	hdd_get_next_adapter_no_lock(hdd_ctx, adapter, &next_adapter), \
	(next_adapter) ? hdd_adapter_dev_hold_debug(next_adapter, dbgid) : \
			 (false), \
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock)

/**
 * __hdd_take_ref_and_fetch_next_adapter_safe - Helper macro to lock, fetch next
 * adapter, take ref and unlock.
 * @hdd_ctx: the global HDD context
 * @adapter: hdd_adapter pointer to use as a cursor
 * @next_adapter: hdd_adapter pointer to next adapter
 * @dbgid: debug ID to detect reference leaks
 */
#define __hdd_take_ref_and_fetch_next_adapter_safe(hdd_ctx, adapter, \
						   next_adapter, dbgid) \
	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock), \
	adapter = next_adapter, \
	hdd_get_next_adapter_no_lock(hdd_ctx, adapter, &next_adapter), \
	hdd_validate_next_adapter(&adapter, &next_adapter, dbgid), \
	(next_adapter) ? hdd_adapter_dev_hold_debug(next_adapter, dbgid) : \
			 (false), \
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock)

/**
 * __hdd_is_adapter_valid - Helper macro to return true/false for valid adapter.
 * @_adapter: an hdd_adapter pointer to use as a cursor
 */
#define __hdd_is_adapter_valid(_adapter) !!_adapter

/**
 * hdd_for_each_adapter_dev_held_safe - Adapter iterator with dev_hold called
 *                                      in a delete safe manner
 * @hdd_ctx: the global HDD context
 * @adapter: an hdd_adapter pointer to use as a cursor
 * @next_adapter: hdd_adapter pointer to the next adapter
 * @dbgid: reference count debugging id
 *
 * This iterator will take the reference of the netdev associated with the
 * given adapter so as to prevent it from being removed in other context. It
 * also takes the reference of the next adapter if exist. This avoids infinite
 * loop due to deletion of the adapter list entry inside the loop. Deletion of
 * list entry will make the list entry to point to self. If the control goes
 * inside the loop body then the dev_hold has been invoked.
 *
 *                           ***** NOTE *****
 * Before the end of each iteration, hdd_adapter_dev_put_debug(adapter, dbgid)
 * must be called. Not calling this will keep hold of a reference, thus
 * preventing unregister of the netdevice. If the loop is terminated in
 * between with return/goto/break statements,
 * hdd_adapter_dev_put_debug(next_adapter, dbgid) must be done along with
 * hdd_adapter_dev_put_debug(adapter, dbgid) before termination of the loop.
 *
 * Usage example:
 *  hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter, dbgid) {
 *        <work involving adapter>
 *        <some more work>
 *        hdd_adapter_dev_put_debug(adapter, dbgid)
 *  }
 */
#define hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter, \
					   dbgid) \
	for (__hdd_take_ref_and_fetch_front_adapter_safe(hdd_ctx, adapter, \
							 next_adapter, dbgid); \
	     __hdd_is_adapter_valid(adapter); \
	     __hdd_take_ref_and_fetch_next_adapter_safe(hdd_ctx, adapter, \
							next_adapter, dbgid))

/* Helper MACROS and APIs definition to iterate
 * link info array in HDD adapter.
 */
#define __hdd_adapter_is_active_link(adapter, link_idx) \
			qdf_atomic_test_bit(link_idx, &(adapter)->active_links)

#define hdd_adapter_get_link_info_if_active(adapter, link_idx) \
		__hdd_adapter_is_active_link((adapter), (link_idx)) ? \
			&((adapter)->link_info[(link_idx)]) : NULL

#define __hdd_is_link_info_valid(_link_info) !!_link_info

#define __hdd_adapter_get_first_active_link_info(adapter, link_info) \
	link_info = NULL, \
	hdd_adapter_get_next_active_link_info(adapter, &link_info)


static inline uint8_t
hdd_adapter_get_index_of_link_info(struct wlan_hdd_link_info *link_info)
{
	return (link_info - &link_info->adapter->link_info[0]);
}

static inline void
hdd_adapter_get_next_active_link_info(struct hdd_adapter *adapter,
				      struct wlan_hdd_link_info **link_info)
{
	uint8_t link_idx = WLAN_HDD_DEFLINK_IDX;
	uint8_t link_idx_max;

	if (!link_info || !adapter)
		return;

	/* If @link_info already points to valid link info address, get the
	 * index of that link info and get the next valid link info which is
	 * set to active.
	 * If @link_info points to invalid address, then start the search
	 * for active link info from WLAN_HDD_DEFLINK_IDX index.
	 */
	if (*link_info)
		link_idx = hdd_adapter_get_index_of_link_info(*link_info) + 1;

	*link_info = NULL;
	link_idx_max = QDF_ARRAY_SIZE(adapter->link_info);
	while (link_idx < link_idx_max && !(*link_info)) {
		*link_info = hdd_adapter_get_link_info_if_active(adapter,
								 link_idx);
		link_idx++;
	}
}

/**
 * hdd_adapter_for_each_active_link_info() - Link info iterator which loops
 * through the link info array elements which are set to active.
 * @adapter: HDD adapter to iterate for each active link info pointer.
 * @link_info: Pointer of active link info.
 *
 * The "active_links" bitmap in @adapter says which indices are active
 * in the link info array.
 * The MACRO iterates through all the active link info elements in link info
 * array and ends loop when no more active link info entries are present.
 * The @link_info points next active link info pointer on each iteration or
 * NULL value at the end of the loop.
 *
 * Callers to take reference of adapter if needed.
 */
#define hdd_adapter_for_each_active_link_info(adapter, link_info) \
	for (__hdd_adapter_get_first_active_link_info(adapter, link_info); \
	     __hdd_is_link_info_valid(link_info); \
	     hdd_adapter_get_next_active_link_info(adapter, &link_info))

#define __hdd_adapter_get_firstlink(adapter, __link_info)	\
		(__link_info = adapter ? &((adapter)->link_info[0]) : NULL)

#define __hdd_is_link_info_idx_valid(adapter, __link_info) \
	((__link_info - &(adapter)->link_info[0]) < \
					QDF_ARRAY_SIZE((adapter)->link_info))

#define __hdd_adapter_next_link_info(link_info)	((link_info)++)

/**
 * hdd_adapter_for_each_link_info() - Link info iterator for all
 * link_info fields.
 * @adapter: HDD adapter to iterate each link_info.
 * @link_info: Pointer to each link info element in the array.
 *
 * The function iterates from the start index of link_info array
 * in @adapter till the end of the link_info array.
 *
 * Callers to take reference of adapter if needed.
 */

#define hdd_adapter_for_each_link_info(adapter, link_info) \
	for (__hdd_adapter_get_firstlink(adapter, link_info); \
	     __hdd_is_link_info_valid(link_info) && \
	     __hdd_is_link_info_idx_valid(adapter, link_info); \
	     __hdd_adapter_next_link_info(link_info))

/**
 * wlan_hdd_get_link_info_from_objmgr() - Fetch adapter from objmgr
 * @vdev: the vdev whose corresponding adapter has to be fetched
 *
 * Return: Address of link info pointer in HDD adapter corresponding to VDEV
 */
struct wlan_hdd_link_info *
wlan_hdd_get_link_info_from_objmgr(struct wlan_objmgr_vdev *vdev);

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC) && \
	defined(WLAN_HDD_MULTI_VDEV_SINGLE_NDEV)
/**
 * hdd_adapter_disable_all_links() - Reset the links on stop adapter.
 * @adapter: HDD adapter
 *
 * Resets the MAC address in each link info and resets the link info
 * mapping in adapter array.
 *
 * Return: void
 */
void hdd_adapter_disable_all_links(struct hdd_adapter *adapter);
#else
static inline void hdd_adapter_disable_all_links(struct hdd_adapter *adapter)
{
}
#endif

struct hdd_adapter *hdd_open_adapter(struct hdd_context *hdd_ctx,
				     uint8_t session_type,
				     const char *name, tSirMacAddr mac_addr,
				     unsigned char name_assign_type,
				     bool rtnl_held,
				     struct hdd_adapter_create_param *params);

QDF_STATUS hdd_open_adapter_no_trans(struct hdd_context *hdd_ctx,
				     enum QDF_OPMODE op_mode,
				     const char *iface_name,
				     uint8_t *mac_addr_bytes,
				     struct hdd_adapter_create_param *params);
/**
 * hdd_close_adapter() - remove and free @adapter from the adapter list
 * @hdd_ctx: The Hdd context containing the adapter list
 * @adapter: the adapter to remove and free
 * @rtnl_held: if the caller is already holding the RTNL lock
 *
 * Return: None
 */
void hdd_close_adapter(struct hdd_context *hdd_ctx,
		       struct hdd_adapter *adapter,
		       bool rtnl_held);

/**
 * hdd_close_all_adapters() - remove and free all adapters from the adapter list
 * @hdd_ctx: The Hdd context containing the adapter list
 * @rtnl_held: if the caller is already holding the RTNL lock
 *
 * Return: None
 */
void hdd_close_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held);

QDF_STATUS hdd_stop_all_adapters(struct hdd_context *hdd_ctx);
void hdd_deinit_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held);
QDF_STATUS hdd_reset_all_adapters(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_start_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held);

/**
 * hdd_get_link_info_by_vdev() - Return link info with the given vdev id
 * @hdd_ctx: hdd context.
 * @vdev_id: vdev id for the link info to get.
 *
 * This function is used to get the link info with provided vdev id
 *
 * Return: adapter pointer if found
 *
 */
struct wlan_hdd_link_info *
hdd_get_link_info_by_vdev(struct hdd_context *hdd_ctx, uint32_t vdev_id);

/**
 * hdd_adapter_get_by_reference() - Return adapter with the given reference
 * @hdd_ctx: hdd context
 * @reference: reference for the adapter to get
 *
 * This function is used to get the adapter with provided reference.
 * The adapter reference will be held until being released by calling
 * hdd_adapter_put().
 *
 * Return: adapter pointer if found
 *
 */
struct hdd_adapter *hdd_adapter_get_by_reference(struct hdd_context *hdd_ctx,
						 struct hdd_adapter *reference);

/**
 * hdd_adapter_put() - Release reference to adapter
 * @adapter: adapter reference
 *
 * Release reference to adapter previously acquired via
 * hdd_adapter_get_*() function
 */
void hdd_adapter_put(struct hdd_adapter *adapter);

/**
 * hdd_get_link_info_by_link_addr() - Get the link info pointer where
 * the link address matches.
 * @hdd_ctx: HDD context pointer
 * @link_addr: Link address to search
 *
 * In the given @adapter search for @link_addr in each entry of link_info
 * array, and return the matching link_info pointer.
 *
 * Return: NULL / Valid link info pointer
 */
struct wlan_hdd_link_info *
hdd_get_link_info_by_link_addr(struct hdd_context *hdd_ctx,
			       struct qdf_mac_addr *link_addr);

struct hdd_adapter *hdd_get_adapter_by_macaddr(struct hdd_context *hdd_ctx,
					       tSirMacAddr mac_addr);

/**
 * hdd_get_link_info_home_channel() - return home channel of adapter
 * @link_info: Pointer of link_info in @adapter
 *
 * This function returns operation channel of station/p2p-cli if
 * connected, returns operation channel of sap/p2p-go if started.
 *
 * Return: home channel if connected/started or invalid channel 0
 */
uint32_t hdd_get_link_info_home_channel(struct wlan_hdd_link_info *link_info);

/**
 * hdd_get_link_info_width() - return current bandwidth of adapter
 * @link_info: Pointer of link_info in @adapter
 *
 * This function returns current bandwidth of station/p2p-cli if
 * connected, returns current bandwidth of sap/p2p-go if started.
 *
 * Return: bandwidth if connected/started or invalid bandwidth 0
 */
enum phy_ch_width hdd_get_link_info_width(struct wlan_hdd_link_info *link_info);

/*
 * hdd_get_adapter_by_rand_macaddr() - find Random mac adapter
 * @hdd_ctx: hdd context
 * @mac_addr: random mac addr
 *
 * Find the Adapter based on random mac addr. Adapter's vdev
 * have active random mac list.
 *
 * Return: adapter ptr or null
 */
struct hdd_adapter *
hdd_get_adapter_by_rand_macaddr(struct hdd_context *hdd_ctx,
				tSirMacAddr mac_addr);

/**
 * hdd_adapter_update_mlo_mgr_mac_addr() - Update each link address to MLO mgr.
 * @adapter: HDD adapter
 *
 * Update MLO manager with each link address and corresponding VDEV ID.
 * Only update for ML-STA adapter types.
 *
 * Return: void
 */
void hdd_adapter_update_mlo_mgr_mac_addr(struct hdd_adapter *adapter);

/**
 * hdd_is_vdev_in_conn_state() - Check whether the vdev is in
 * connected/started state.
 * @link_info: Pointer to link_info in adapter
 *
 * This function will give whether the vdev in the adapter is in
 * connected/started state.
 *
 * Return: True/false
 */
bool hdd_is_vdev_in_conn_state(struct wlan_hdd_link_info *link_info);

/**
 * hdd_adapter_deregister_fc() - Deregisters flow control
 * callbacks
 * @adapter: HDD adapter
 *
 * The function call deregisters flow control callbacks
 *
 * Return: void
 */
void hdd_adapter_deregister_fc(struct hdd_adapter *adapter);

#ifdef WLAN_OPEN_SOURCE
/**
 * hdd_cancel_ip_notifier_work() - Cancel scheduled IP
 * notifier deferred work
 * @adapter: HDD adapter
 *
 * The API calls cancel work for IPv4 and IPv6 notifier
 * deferred task
 *
 * Return: void
 */
void hdd_cancel_ip_notifier_work(struct hdd_adapter *adapter);
#else
static inline
void hdd_cancel_ip_notifier_work(struct hdd_adapter *adapter)
{
}
#endif

/**
 * hdd_vdev_create() - Create the vdev in the firmware
 * @link_info: Link info pointer in HDD adapter
 *
 * This function will create the vdev in the firmware
 *
 * Return: 0 when the vdev create is sent to firmware or -EINVAL when
 * there is a failure to send the command.
 */
int hdd_vdev_create(struct wlan_hdd_link_info *link_info);

/**
 * hdd_vdev_destroy() - Destroy the vdev in the firmware
 * @link_info: Link info pointer in HDD adapter
 *
 * This function will destroy the vdev in the firmware
 *
 * Return: 0 when the vdev destroy is sent to firmware
 * or -EINVAL when there is a failure to send the command.
 */
int hdd_vdev_destroy(struct wlan_hdd_link_info *link_info);

/**
 * hdd_vdev_ready() - Configure FW post VDEV create
 * @vdev: VDEV object.
 * @bridgeaddr: Bridge MAC address
 *
 * The function is used send configuration to the FW
 * post VDEV creation.
 * The caller to ensure to hold the VDEV reference
 *
 * Return: 0 on success, negative value on failure.
 */
int hdd_vdev_ready(struct wlan_objmgr_vdev *vdev,
		   struct qdf_mac_addr *bridgeaddr);

/**
 * hdd_init_station_mode() - Initialize STA mode adapter
 * post vdev creation.
 * @link_info: Link info pointer in HDD adapter
 *
 * The function initializes the adapter post vdev
 * create for STA mode type adapters on start
 * adapter.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_init_station_mode(struct wlan_hdd_link_info *link_info);

struct hdd_adapter *hdd_get_adapter(struct hdd_context *hdd_ctx,
			enum QDF_OPMODE mode);

/**
 * hdd_get_device_mode() - Get device mode
 * @vdev_id: vdev id
 *
 * Return: Device mode
 */
enum QDF_OPMODE hdd_get_device_mode(uint32_t vdev_id);

/**
 * hdd_deinit_session() - Cleanup session context in
 * adapter
 * @adapter: HDD adapter
 *
 * The API cleans up session context and scan IEs
 * in link_info and adapter.
 *
 * Return: None
 */
void hdd_deinit_session(struct hdd_adapter *adapter);

void hdd_deinit_adapter(struct hdd_context *hdd_ctx,
			struct hdd_adapter *adapter,
			bool rtnl_held);
QDF_STATUS hdd_stop_adapter(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter);

/**
 * hdd_set_station_ops() - update net_device ops
 * @dev: Handle to struct net_device to be updated.
 * Return: None
 */
void hdd_set_station_ops(struct net_device *dev);

/**
 * wlan_hdd_get_intf_addr() - Get address for the interface
 * @hdd_ctx: Pointer to hdd context
 * @interface_type: type of the interface for which address is queried
 *
 * This function is used to get mac address for every new interface
 *
 * Return: If addr is present then return pointer to MAC address
 *         else NULL
 */

uint8_t *wlan_hdd_get_intf_addr(struct hdd_context *hdd_ctx,
				enum QDF_OPMODE interface_type);
void wlan_hdd_release_intf_addr(struct hdd_context *hdd_ctx,
				uint8_t *releaseAddr);

/**
 * hdd_get_operating_chan_freq() - return operating channel of the device mode
 * @hdd_ctx:	Pointer to the HDD context.
 * @mode:	Device mode for which operating channel is required.
 *              Supported modes:
 *			QDF_STA_MODE,
 *			QDF_P2P_CLIENT_MODE,
 *			QDF_SAP_MODE,
 *			QDF_P2P_GO_MODE.
 *
 * This API returns the operating channel of the requested device mode
 *
 * Return: channel frequency, or
 *         0 if the requested device mode is not found.
 */
uint32_t hdd_get_operating_chan_freq(struct hdd_context *hdd_ctx,
				     enum QDF_OPMODE mode);

void hdd_set_conparam(int32_t con_param);
enum QDF_GLOBAL_MODE hdd_get_conparam(void);

/**
 * wlan_hdd_reset_prob_rspies() - Reset probe response IEs
 * @link_info: Link info pointer in HDD adapter.
 *
 * Reset the probe response IEs for the VDEV pointer by link info.
 *
 * Return: void
 */
void wlan_hdd_reset_prob_rspies(struct wlan_hdd_link_info *link_info);
void hdd_prevent_suspend(uint32_t reason);

/*
 * hdd_get_first_valid_adapter() - Get the first valid adapter from adapter list
 *
 * This function is used to fetch the first valid adapter from the adapter
 * list. If there is no valid adapter then it returns NULL
 *
 * @hdd_ctx: HDD context handler
 *
 * Return: NULL if no valid adapter found in the adapter list
 *
 */
struct hdd_adapter *hdd_get_first_valid_adapter(struct hdd_context *hdd_ctx);

void hdd_allow_suspend(uint32_t reason);
void hdd_prevent_suspend_timeout(uint32_t timeout, uint32_t reason);

/**
 * wlan_hdd_validate_context() - check the HDD context
 * @hdd_ctx: Global HDD context pointer
 *
 * Return: 0 if the context is valid. Error code otherwise
 */
#define wlan_hdd_validate_context(hdd_ctx) \
	__wlan_hdd_validate_context(hdd_ctx, __func__)

int __wlan_hdd_validate_context(struct hdd_context *hdd_ctx, const char *func);

/**
 * hdd_validate_adapter() - Validate the given adapter
 * @adapter: the adapter to validate
 *
 * This function validates the given adapter, and ensures that it is open.
 *
 * Return: Errno
 */
#define hdd_validate_adapter(adapter) \
	__hdd_validate_adapter(adapter, __func__)

int __hdd_validate_adapter(struct hdd_adapter *adapter, const char *func);

/**
 * wlan_hdd_validate_vdev_id() - ensure the given vdev Id is valid
 * @vdev_id: the vdev Id to validate
 *
 * Return: Errno
 */
#define wlan_hdd_validate_vdev_id(vdev_id) \
	__wlan_hdd_validate_vdev_id(vdev_id, __func__)

int __wlan_hdd_validate_vdev_id(uint8_t vdev_id, const char *func);

/**
 * hdd_is_valid_mac_address() - validate MAC address
 * @mac_addr:	Pointer to the input MAC address
 *
 * This function validates whether the given MAC address is valid or not
 * Expected MAC address is of the format XX:XX:XX:XX:XX:XX
 * where X is the hexa decimal digit character and separated by ':'
 * This algorithm works even if MAC address is not separated by ':'
 *
 * This code checks given input string mac contains exactly 12 hexadecimal
 * digits and a separator colon : appears in the input string only after
 * an even number of hex digits.
 *
 * Return: true for valid and false for invalid
 */
bool hdd_is_valid_mac_address(const uint8_t *mac_addr);

bool wlan_hdd_validate_modules_state(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_validate_mac_address() - Function to validate mac address
 * @mac_addr: input mac address
 *
 * Return QDF_STATUS
 */
#define wlan_hdd_validate_mac_address(mac_addr) \
	__wlan_hdd_validate_mac_address(mac_addr, __func__)

QDF_STATUS __wlan_hdd_validate_mac_address(struct qdf_mac_addr *mac_addr,
					   const char *func);

/**
 * hdd_is_any_adapter_connected() - Check if any adapter is in connected state
 * @hdd_ctx: the global hdd context
 *
 * Returns: true, if any of the adapters is in connected state,
 *	    false, if none of the adapters is in connected state.
 */
bool hdd_is_any_adapter_connected(struct hdd_context *hdd_ctx);

/**
 * hdd_init_adapter_ops_wq() - Init global workqueue for adapter operations.
 * @hdd_ctx: pointer to HDD context
 *
 * Return: QDF_STATUS_SUCCESS if workqueue is allocated,
 *	   QDF_STATUS_E_NOMEM if workqueue aloocation fails.
 */
QDF_STATUS hdd_init_adapter_ops_wq(struct hdd_context *hdd_ctx);

/**
 * hdd_deinit_adapter_ops_wq() - Deinit global workqueue for adapter operations.
 * @hdd_ctx: pointer to HDD context
 *
 * Return: None
 */
void hdd_deinit_adapter_ops_wq(struct hdd_context *hdd_ctx);

/**
 * hdd_adapter_feature_update_work_init() - Init per adapter work for netdev
 *					    feature update
 * @adapter: pointer to adapter structure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_adapter_feature_update_work_init(struct hdd_adapter *adapter);

/**
 * hdd_adapter_feature_update_work_deinit() - Deinit per adapter work for
 *					      netdev feature update
 * @adapter: pointer to adapter structure
 *
 * Return: QDF_STATUS
 */
void hdd_adapter_feature_update_work_deinit(struct hdd_adapter *adapter);

int hdd_qdf_trace_enable(QDF_MODULE_ID module_id, uint32_t bitmask);

int hdd_init(void);
void hdd_deinit(void);

/**
 * hdd_wlan_startup() - HDD init function
 * @hdd_ctx: the HDD context corresponding to the psoc to startup
 *
 * Return: Errno
 */
int hdd_wlan_startup(struct hdd_context *hdd_ctx);

/**
 * hdd_wlan_exit() - HDD WLAN exit function
 * @hdd_ctx: pointer to the HDD Context
 *
 * Return: None
 */
void hdd_wlan_exit(struct hdd_context *hdd_ctx);

/**
 * hdd_psoc_create_vdevs() - create the default vdevs for a psoc
 * @hdd_ctx: the HDD context for the psoc to operate against
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_psoc_create_vdevs(struct hdd_context *hdd_ctx);

/*
 * hdd_context_create() - Allocate and inialize HDD context.
 * @dev: Device Pointer to the underlying device
 *
 * Allocate and initialize HDD context. HDD context is allocated as part of
 * wiphy allocation and then context is initialized.
 *
 * Return: HDD context on success and ERR_PTR on failure
 */
struct hdd_context *hdd_context_create(struct device *dev);

/**
 * hdd_context_destroy() - Destroy HDD context
 * @hdd_ctx: HDD context to be destroyed.
 *
 * Free config and HDD context as well as destroy all the resources.
 *
 * Return: None
 */
void hdd_context_destroy(struct hdd_context *hdd_ctx);

int hdd_wlan_notify_modem_power_state(int state);

void wlan_hdd_send_svc_nlink_msg(int radio, int type, void *data, int len);
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
void wlan_hdd_auto_shutdown_enable(struct hdd_context *hdd_ctx, bool enable);
#else
static inline void
wlan_hdd_auto_shutdown_enable(struct hdd_context *hdd_ctx, bool enable)
{
}
#endif

struct hdd_adapter *
hdd_get_con_sap_adapter(struct hdd_adapter *this_sap_adapter,
			bool check_start_bss);

bool hdd_is_5g_supported(struct hdd_context *hdd_ctx);

/**
 * hdd_is_2g_supported() - check if 2GHz channels are supported
 * @hdd_ctx:	Pointer to the hdd context
 *
 * HDD function to know if 2GHz channels are supported
 *
 * Return:  true if 2GHz channels are supported
 */
bool hdd_is_2g_supported(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_scan_abort() - abort ongoing scan
 * @link_info: Link info pointer in HDD adapter
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_hdd_scan_abort(struct wlan_hdd_link_info *link_info);

/**
 * hdd_indicate_active_ndp_cnt() - Callback to indicate active ndp count to hdd
 * if ndp connection is on NDI established
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id
 * @cnt: number of active ndp sessions
 *
 * This HDD callback registered with policy manager to indicates number of active
 * ndp sessions to hdd.
 *
 * Return:  none
 */
void hdd_indicate_active_ndp_cnt(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, uint8_t cnt);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static inline bool roaming_offload_enabled(struct hdd_context *hdd_ctx)
{
	bool is_roam_offload;

	ucfg_mlme_get_roaming_offload(hdd_ctx->psoc, &is_roam_offload);

	return is_roam_offload;
}
#else
static inline bool roaming_offload_enabled(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_HOST_ROAM
static inline bool hdd_driver_roaming_supported(struct hdd_context *hdd_ctx)
{
	bool lfr_enabled;

	ucfg_mlme_is_lfr_enabled(hdd_ctx->psoc, &lfr_enabled);

	return lfr_enabled;
}
#else
static inline bool hdd_driver_roaming_supported(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

static inline bool hdd_roaming_supported(struct hdd_context *hdd_ctx)
{
	bool val;

	val = hdd_driver_roaming_supported(hdd_ctx) ||
		roaming_offload_enabled(hdd_ctx);

	return val;
}

#ifdef WLAN_NS_OFFLOAD
static inline void
hdd_adapter_flush_ipv6_notifier_work(struct hdd_adapter *adapter)
{
	flush_work(&adapter->ipv6_notifier_work);
}
#else
static inline void
hdd_adapter_flush_ipv6_notifier_work(struct hdd_adapter *adapter)
{
}
#endif

#ifdef CFG80211_SCAN_RANDOM_MAC_ADDR
static inline bool hdd_scan_random_mac_addr_supported(void)
{
	return true;
}
#else
static inline bool hdd_scan_random_mac_addr_supported(void)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
static inline bool hdd_dynamic_mac_addr_supported(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->is_vdev_macaddr_dynamic_update_supported;
}
#else
static inline bool hdd_dynamic_mac_addr_supported(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

/**
 * hdd_adapter_get_link_info_ptr() - To get the pointer of link_info
 * in adapter.
 * @adapter: HDD adapter
 * @link_idx: Index of link_info in @adapter.
 *
 * The API returns link_info in @adapter pointed at @link_idx in the array.
 *
 * Return: Pointer to wlan_hdd_link_info or NULL.
 */
static inline struct wlan_hdd_link_info *
hdd_adapter_get_link_info_ptr(struct hdd_adapter *adapter, uint8_t link_idx)
{
	if (!adapter || (link_idx >= QDF_ARRAY_SIZE(adapter->link_info)))
		return NULL;

	return &adapter->link_info[link_idx];
}

/**
 * hdd_start_vendor_acs(): Start vendor ACS procedure
 * @adapter: pointer to SAP adapter struct
 *
 * This function sends the ACS config to the ACS daemon and
 * starts the vendor ACS timer to wait for the next command.
 *
 * Return: Status of vendor ACS procedure
 */
int hdd_start_vendor_acs(struct hdd_adapter *adapter);

/**
 * hdd_acs_response_timeout_handler() - timeout handler for acs_timer
 * @context: timeout handler context
 *
 * Return: None
 */
void hdd_acs_response_timeout_handler(void *context);

/**
 * wlan_hdd_cfg80211_start_acs(): Start ACS Procedure for SAP
 * @link_info: Link info pointer in HDD adapter
 *
 * This function starts the ACS procedure if there are no
 * constraints like MBSSID DFS restrictions.
 *
 * Return: Status of ACS Start procedure
 */
int wlan_hdd_cfg80211_start_acs(struct wlan_hdd_link_info *link_info);

/**
 * wlan_hdd_trim_acs_channel_list() - Trims ACS channel list with
 * intersection of PCL
 * @pcl: preferred channel list
 * @pcl_count: Preferred channel list count
 * @org_freq_list: ACS channel list from user space
 * @org_ch_list_count: ACS channel count from user space
 *
 * Return: None
 */
void wlan_hdd_trim_acs_channel_list(uint32_t *pcl, uint8_t pcl_count,
				    uint32_t *org_freq_list,
				    uint8_t *org_ch_list_count);

/**
 * wlan_hdd_handle_zero_acs_list() - Handle worst case of ACS channel
 * trimmed to zero
 * @hdd_ctx: struct hdd_context
 * @acs_freq_list: Calculated ACS channel list
 * @acs_ch_list_count: Calculated ACS channel count
 * @org_freq_list: ACS channel list from user space
 * @org_ch_list_count: ACS channel count from user space
 *
 * When all channels in the ACS freq list is filtered out by
 * wlan_hdd_trim_acs_channel_list(), the hostapd start will fail.
 * This happens when PCL is PM_24G_SCC_CH_SBS_CH, and SAP ACS range
 * includes 5 GHz channel list. One example is STA active on 6 GHz
 * chan. Hostapd start SAP on 5 GHz ACS range. The intersection of PCL
 * and ACS range is zero.  Instead of ACS failure, this API selects
 * one channel from ACS range and report to Hostapd. When hostapd do
 * start_ap, the driver will force SCC to 6 GHz or move SAP to 2 GHz
 * based on SAP's configuration.
 *
 * Return: None
 */
void wlan_hdd_handle_zero_acs_list(struct hdd_context *hdd_ctx,
				   uint32_t *acs_freq_list,
				   uint8_t *acs_ch_list_count,
				   uint32_t *org_freq_list,
				   uint8_t org_ch_list_count);

/**
 * hdd_cfg80211_update_acs_config() - update acs config to application
 * @adapter: hdd adapter
 * @reason: channel change reason
 *
 * Return: 0 for success else error code
 */
int hdd_cfg80211_update_acs_config(struct hdd_adapter *adapter,
				   uint8_t reason);

/**
 * hdd_update_acs_timer_reason() - update acs timer start reason
 * @adapter: hdd adapter
 * @reason: channel change reason
 *
 * Return: 0 for success
 */
int hdd_update_acs_timer_reason(struct hdd_adapter *adapter, uint8_t reason);

/**
 * hdd_switch_sap_channel() - Move SAP to the given channel
 * @link_info: Pointer of link_info in adapter
 * @channel: Channel
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Moves the SAP interface by invoking the function which
 * executes the callback to perform channel switch using (E)CSA.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_switch_sap_channel(struct wlan_hdd_link_info *link_info,
				  uint8_t channel, bool forced);

/**
 * hdd_switch_sap_chan_freq() - Move SAP to the given channel
 * @adapter: AP adapter
 * @chan_freq: Channel frequency
 * @ch_width: channel bandwidth
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Moves the SAP interface by invoking the function which
 * executes the callback to perform channel switch using (E)CSA.
 *
 * Return: QDF_STATUS_SUCCESS if successfully
 */
QDF_STATUS hdd_switch_sap_chan_freq(struct hdd_adapter *adapter,
				    qdf_freq_t chan_freq,
				    enum phy_ch_width ch_width,
				    bool forced);

#if defined(FEATURE_WLAN_CH_AVOID)
QDF_STATUS hdd_unsafe_channel_restart_sap(struct hdd_context *hdd_ctx);

void hdd_ch_avoid_ind(struct hdd_context *hdd_ctxt,
		      struct unsafe_ch_list *unsafe_chan_list,
		      struct ch_avoid_ind_type *avoid_freq_list);
#else
static inline
QDF_STATUS hdd_unsafe_channel_restart_sap(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void hdd_ch_avoid_ind(struct hdd_context *hdd_ctxt,
		      struct unsafe_ch_list *unsafe_chan_list,
		      struct ch_avoid_ind_type *avoid_freq_list)
{
}
#endif

/**
 * hdd_free_mac_address_lists() - Free both the MAC address lists
 * @hdd_ctx: HDD context
 *
 * This API clears/memset provisioned address list and
 * derived address list
 *
 */
void hdd_free_mac_address_lists(struct hdd_context *hdd_ctx);

/**
 * hdd_update_macaddr() - update mac address
 * @hdd_ctx:	hdd contxt
 * @hw_macaddr:	mac address
 * @generate_mac_auto: Indicates whether the first address is
 * provisioned address or derived address.
 *
 * Mac address for multiple virtual interface is found as following
 * i) The mac address of the first interface is just the actual hw mac address.
 * ii) MSM 3 or 4 bits of byte5 of the actual mac address are used to
 *     define the mac address for the remaining interfaces and locally
 *     admistered bit is set. INTF_MACADDR_MASK is based on the number of
 *     supported virtual interfaces, right now this is 0x07 (meaning 8
 *     interface).
 *     Byte[3] of second interface will be hw_macaddr[3](bit5..7) + 1,
 *     for third interface it will be hw_macaddr[3](bit5..7) + 2, etc.
 *
 * Return: None
 */
void hdd_update_macaddr(struct hdd_context *hdd_ctx,
			struct qdf_mac_addr hw_macaddr, bool generate_mac_auto);

/**
 * hdd_store_nss_chains_cfg_in_vdev() - Store the per vdev ini cfg in vdev_obj
 * @hdd_ctx: HDD context passed from caller
 * @vdev: VDEV passed with caller holding reference.
 *
 * This function will store the per vdev nss params to the particular mlme
 * vdev obj.
 * Caller shall acquire the reference for vdev objmgr and release on return.
 *
 * Return: None
 */
void
hdd_store_nss_chains_cfg_in_vdev(struct hdd_context *hdd_ctx,
				 struct wlan_objmgr_vdev *vdev);

/**
 * wlan_hdd_set_roaming_state() - Enable or disable roaming
 * on all STAs except the input one
 * @cur_link_info: Current link info pointer in HDD adapter
 * @rso_op_requestor: roam disable requestor
 * @enab_roam: Set to true to enable roaming or else set false
 *
 * This function loops through all adapters and enables or
 * disables roaming on each STA mode adapter except the
 * current adapter passed from the caller.
 * If @enab_roam is true, roaming is enabled or else
 * roaming is disabled
 *
 * Return: None
 */
void
wlan_hdd_set_roaming_state(struct wlan_hdd_link_info *cur_link_info,
			   enum wlan_cm_rso_control_requestor rso_op_requestor,
			   bool enab_roam);

QDF_STATUS hdd_post_cds_enable_config(struct hdd_context *hdd_ctx);

QDF_STATUS hdd_abort_mac_scan_all_adapters(struct hdd_context *hdd_ctx);

void wlan_hdd_stop_sap(struct hdd_adapter *ap_adapter);

/**
 * wlan_hdd_start_sap() - this function starts bss of SAP.
 * @link_info: Link info pointer in SAP/GO adapter
 * @reinit: true if this is a re-init, otherwise initial int
 *
 * This function will process the starting of sap adapter.
 *
 * Return: None
 */
void wlan_hdd_start_sap(struct wlan_hdd_link_info *link_info, bool reinit);

/**
 * wlan_hdd_set_sap_beacon_protection() - this function will set beacon
 * protection for SAP.
 * @hdd_ctx: pointer to HDD context
 * @link_info: Link info pointer
 * @beacon: pointer to beacon data structure
 *
 * This function will enable beacon protection and cache the value in vdev
 * priv object.
 *
 * Return: None
 */
void wlan_hdd_set_sap_beacon_protection(struct hdd_context *hdd_ctx,
					struct wlan_hdd_link_info *link_info,
					struct hdd_beacon_data *beacon);
#ifdef QCA_CONFIG_SMP
int wlan_hdd_get_cpu(void);
#else
static inline int wlan_hdd_get_cpu(void)
{
	return 0;
}
#endif

void wlan_hdd_txrx_pause_cb(uint8_t vdev_id,
	enum netif_action_type action, enum netif_reason_type reason);

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
void wlan_hdd_mod_fc_timer(struct hdd_adapter *adapter,
			   enum netif_action_type action);
#else
static inline void wlan_hdd_mod_fc_timer(struct hdd_adapter *adapter,
					 enum netif_action_type action)
{
}
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

/**
 * hdd_wlan_dump_stats() - display dump Stats
 * @adapter: adapter handle
 * @stats_id: stats id from user
 *
 * Return: 0 => success, error code on failure
 */
int hdd_wlan_dump_stats(struct hdd_adapter *adapter, int stats_id);

/**
 * hdd_wlan_clear_stats() - clear Stats
 * @adapter: adapter handle
 * @stats_id: stats id from user
 *
 * Return: 0 => success, error code on failure
 */
int hdd_wlan_clear_stats(struct hdd_adapter *adapter, int stats_id);

/**
 * hdd_cb_handle_to_context() - turn an HDD handle into an HDD context
 * @hdd_handle: HDD handle to be converted
 *
 * Return: HDD context referenced by @hdd_handle
 */
static inline
struct hdd_context *hdd_cb_handle_to_context(hdd_cb_handle hdd_handle)
{
	return (struct hdd_context *)hdd_handle;
}

/**
 * wlan_hdd_display_netif_queue_history() - display netif queue history
 * @context: opaque handle to hdd context
 * @verb_lvl: Verbosity levels for stats
 *
 * Return: none
 */
void
wlan_hdd_display_netif_queue_history(hdd_cb_handle context,
				     enum qdf_stats_verbosity_level verb_lvl);

/**
 * wlan_hdd_display_adapter_netif_queue_history() - display adapter based netif
 * queue history
 * @adapter: hdd adapter
 *
 * Return: none
 */
void
wlan_hdd_display_adapter_netif_queue_history(struct hdd_adapter *adapter);

void wlan_hdd_clear_netif_queue_history(struct hdd_context *hdd_ctx);
const char *hdd_get_fwpath(void);
void hdd_indicate_mgmt_frame(tSirSmeMgmtFrameInd *frame_ind);

/**
 * hdd_get_adapter_by_iface_name() - Return adapter with given interface name
 * @hdd_ctx: hdd context.
 * @iface_name: interface name
 *
 * This function is used to get the adapter with given interface name
 *
 * Return: adapter pointer if found, NULL otherwise
 *
 */
struct hdd_adapter *hdd_get_adapter_by_iface_name(struct hdd_context *hdd_ctx,
						  const char *iface_name);

/**
 * hdd_get_adapter_by_ifindex() - Return adapter associated with an ifndex
 * @hdd_ctx: hdd context.
 * @if_index: netdev interface index
 *
 * This function is used to get the adapter associated with a netdev with the
 * given interface index.
 *
 * Return: adapter pointer if found, NULL otherwise
 *
 */
struct hdd_adapter *hdd_get_adapter_by_ifindex(struct hdd_context *hdd_ctx,
					       uint32_t if_index);

enum phy_ch_width hdd_map_nl_chan_width(enum nl80211_chan_width ch_width);

/**
 * hdd_nl_to_qdf_iface_type() - map nl80211_iftype to QDF_OPMODE
 * @nl_type: the input NL80211 interface type to map
 * @out_qdf_type: the output, equivalent QDF operating mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_nl_to_qdf_iface_type(enum nl80211_iftype nl_type,
				    enum QDF_OPMODE *out_qdf_type);

/**
 * wlan_hdd_find_opclass() - Find operating class for a channel
 * @mac_handle: global MAC handle
 * @channel: channel id
 * @bw_offset: bandwidth offset
 *
 * Function invokes sme api to find the operating class
 *
 * Return: operating class
 */
uint8_t wlan_hdd_find_opclass(mac_handle_t mac_handle, uint8_t channel,
			      uint8_t bw_offset);

int hdd_update_config(struct hdd_context *hdd_ctx);

/**
 * hdd_update_components_config() - Initialize driver per module ini parameters
 * @hdd_ctx: HDD Context
 *
 * API is used to initialize components configuration parameters
 * Return: 0 for success, errno for failure
 */
int hdd_update_components_config(struct hdd_context *hdd_ctx);

/**
 * hdd_chan_change_notify_work_handler() - Function to notify hostapd about
 * channel change
 * @work: work pointer
 *
 * This function is used to notify hostapd about the channel change
 *
 * Return: None
 *
 */
void hdd_chan_change_notify_work_handler(void *work);

int wlan_hdd_set_channel(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_chan_def *chandef,
		enum nl80211_channel_type channel_type);

/**
 * wlan_hdd_cfg80211_start_bss() - start bss
 * @link_info: Link info pointer in hostapd adapter
 * @params: Pointer to start bss beacon parameters
 * @ssid: Pointer ssid
 * @ssid_len: Length of ssid
 * @hidden_ssid: Hidden SSID parameter
 * @check_for_concurrency: Flag to indicate if check for concurrency is needed
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_cfg80211_start_bss(struct wlan_hdd_link_info *link_info,
				struct cfg80211_beacon_data *params,
				const u8 *ssid, size_t ssid_len,
				enum nl80211_hidden_ssid hidden_ssid,
				bool check_for_concurrency);

#if !defined(REMOVE_PKT_LOG)
int hdd_process_pktlog_command(struct hdd_context *hdd_ctx, uint32_t set_value,
			       int set_value2);
int hdd_pktlog_enable_disable(struct hdd_context *hdd_ctx, bool enable,
			      uint8_t user_triggered, int size);

#else
static inline
int hdd_pktlog_enable_disable(struct hdd_context *hdd_ctx, bool enable,
			      uint8_t user_triggered, int size)
{
	return 0;
}

static inline
int hdd_process_pktlog_command(struct hdd_context *hdd_ctx,
			       uint32_t set_value, int set_value2)
{
	return 0;
}
#endif /* REMOVE_PKT_LOG */

#if defined(FEATURE_SG) && !defined(CONFIG_HL_SUPPORT)
/**
 * hdd_set_sg_flags() - enable SG flag in the network device
 * @hdd_ctx: HDD context
 * @wlan_dev: network device structure
 *
 * This function enables the SG feature flag in the
 * given network device.
 *
 * Return: none
 */
static inline void hdd_set_sg_flags(struct hdd_context *hdd_ctx,
				struct net_device *wlan_dev)
{
	hdd_debug("SG Enabled");
	wlan_dev->features |= NETIF_F_SG;
}
#else
static inline void hdd_set_sg_flags(struct hdd_context *hdd_ctx,
				struct net_device *wlan_dev){}
#endif

/**
 * hdd_set_netdev_flags() - set netdev flags for adapter as per ini config
 * @adapter: hdd adapter context
 *
 * This function sets netdev feature flags for the adapter.
 *
 * Return: none
 */
void hdd_set_netdev_flags(struct hdd_adapter *adapter);

#ifdef FEATURE_TSO
/**
 * hdd_get_tso_csum_feature_flags() - Return TSO and csum flags if enabled
 *
 * Return: Enabled feature flags set, 0 on failure
 */
static inline netdev_features_t hdd_get_tso_csum_feature_flags(void)
{
	netdev_features_t netdev_features = 0;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!soc) {
		hdd_err("soc handle is NULL");
		return 0;
	}

	if (cdp_cfg_get(soc, cfg_dp_enable_ip_tcp_udp_checksum_offload)) {
		netdev_features = NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;

		if (cdp_cfg_get(soc, cfg_dp_tso_enable)) {
			/*
			 * Enable TSO only if IP/UDP/TCP TX checksum flag is
			 * enabled.
			 */
			netdev_features |= NETIF_F_TSO | NETIF_F_TSO6 |
					   NETIF_F_SG;
		}
	}
	return netdev_features;
}

/**
 * hdd_set_tso_flags() - enable TSO flags in the network device
 * @hdd_ctx: HDD context
 * @wlan_dev: network device structure
 *
 * This function enables the TSO related feature flags in the
 * given network device.
 *
 * Return: none
 */
static inline void hdd_set_tso_flags(struct hdd_context *hdd_ctx,
	 struct net_device *wlan_dev)
{
	hdd_debug("TSO Enabled");

	wlan_dev->features |= hdd_get_tso_csum_feature_flags();
}
#else
static inline void hdd_set_tso_flags(struct hdd_context *hdd_ctx,
	 struct net_device *wlan_dev)
{
	hdd_set_sg_flags(hdd_ctx, wlan_dev);
}

static inline netdev_features_t hdd_get_tso_csum_feature_flags(void)
{
	return 0;
}
#endif /* FEATURE_TSO */

/**
 * wlan_hdd_get_host_log_nl_proto() - Get host log netlink protocol
 * @hdd_ctx: HDD context
 *
 * This function returns with host log netlink protocol settings
 *
 * Return: none
 */
#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
static inline int wlan_hdd_get_host_log_nl_proto(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->config->host_log_custom_nl_proto;
}
#else
static inline int wlan_hdd_get_host_log_nl_proto(struct hdd_context *hdd_ctx)
{
	return NETLINK_USERSOCK;
}
#endif

#ifdef CONFIG_CNSS_LOGGER
/**
 * wlan_hdd_nl_init() - wrapper function to CNSS_LOGGER case
 * @hdd_ctx:	the hdd context pointer
 *
 * The nl_srv_init() will call to cnss_logger_device_register() and
 * expect to get a radio_index from cnss_logger module and assign to
 * hdd_ctx->radio_index, then to maintain the consistency to original
 * design, adding the radio_index check here, then return the error
 * code if radio_index is not assigned correctly, which means the nl_init
 * from cnss_logger is failed.
 *
 * Return: 0 if successfully, otherwise error code
 */
static inline int wlan_hdd_nl_init(struct hdd_context *hdd_ctx)
{
	int proto;

	proto = wlan_hdd_get_host_log_nl_proto(hdd_ctx);
	hdd_ctx->radio_index = nl_srv_init(hdd_ctx->wiphy, proto);

	/* radio_index is assigned from 0, so only >=0 will be valid index  */
	if (hdd_ctx->radio_index >= 0)
		return 0;
	else
		return -EINVAL;
}
#else
/**
 * wlan_hdd_nl_init() - wrapper function to non CNSS_LOGGER case
 * @hdd_ctx:	the hdd context pointer
 *
 * In case of non CNSS_LOGGER case, the nl_srv_init() will initialize
 * the netlink socket and return the success or not.
 *
 * Return: the return value from  nl_srv_init()
 */
static inline int wlan_hdd_nl_init(struct hdd_context *hdd_ctx)
{
	int proto;

	proto = wlan_hdd_get_host_log_nl_proto(hdd_ctx);
	return nl_srv_init(hdd_ctx->wiphy, proto);
}
#endif

QDF_STATUS hdd_sme_close_session_callback(uint8_t vdev_id);

int hdd_register_cb(struct hdd_context *hdd_ctx);
void hdd_deregister_cb(struct hdd_context *hdd_ctx);

#ifdef WLAN_HDD_MULTI_VDEV_SINGLE_NDEV
static inline struct qdf_mac_addr *
hdd_adapter_get_netdev_mac_addr(struct hdd_adapter *adapter)
{
	return &adapter->mac_addr;
}
#else
static inline struct qdf_mac_addr *
hdd_adapter_get_netdev_mac_addr(struct hdd_adapter *adapter)
{
	if (hdd_adapter_is_ml_adapter(adapter) ||
	    hdd_adapter_is_link_adapter(adapter))
		return &adapter->mld_addr;

	return &adapter->mac_addr;
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC) && \
	defined(WLAN_HDD_MULTI_VDEV_SINGLE_NDEV)
/**
 * hdd_adapter_fill_link_address() - Fill derived
 * link address in adapter
 * @adapter: HDD adapter
 *
 * The API takes MLD address of @adapter and calls link address
 * derive API and fills the derived link address in each link.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_adapter_fill_link_address(struct hdd_adapter *adapter);
#else
static inline
QDF_STATUS hdd_adapter_fill_link_address(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * hdd_adapter_get_link_mac_addr() - Returns the appropriate
 * MAC address pointer in adapter.
 * @link_info: Link info in HDD adapter.
 *
 * If WLAN_HDD_MULTI_VDEV_SINGLE_NDEV flag is enabled, then MAC address pointer
 * returned is based on following conditions:
 *      -if adapter of link info is non-ml:
 *              Return pointer of mac_addr in adapter.
 *      -else if link_addr in @link_info is NULL:
 *              Return pointer of mac_addr in adapter.
 *      -else
 *              Return pointer of link_addr in @link_info.
 *
 * If WLAN_HDD_MULTI_VDEV_SINGLE_NDEV flag is not enabled, then return pointer
 * of mac_addr in adapter.
 *
 * Return: MAC address pointer based on adapter type.
 */
struct qdf_mac_addr *
hdd_adapter_get_link_mac_addr(struct wlan_hdd_link_info *link_info);

/**
 * hdd_adapter_check_duplicate_session() - Check for duplicate
 * session on start adapter.
 * @adapter: HDD adapter
 *
 * The API passes list of addresses contained in @adapter to
 * sme_check_for_duplicate_session() to check the status
 * of existing peer with same MAC address.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_adapter_check_duplicate_session(struct hdd_adapter *adapter);

/**
 * hdd_adapter_reset_station_ctx() - Resets station context with appropriate
 * initial value.
 * @adapter: HDD adapter
 *
 * Return: void
 */
void hdd_adapter_reset_station_ctx(struct hdd_adapter *adapter);

/**
 * hdd_start_station_adapter()- Start the Station Adapter
 * @adapter: HDD adapter
 *
 * This function initializes the adapter for the station mode.
 *
 * Return: 0 on success or errno on failure.
 */
int hdd_start_station_adapter(struct hdd_adapter *adapter);

/**
 * hdd_start_ap_adapter()- Start AP Adapter
 * @adapter: HDD adapter
 * @rtnl_held: True if rtnl lock is taken, otherwise false
 *
 * This function initializes the adapter for the AP mode.
 *
 * Return: 0 on success errno on failure.
 */
int hdd_start_ap_adapter(struct hdd_adapter *adapter, bool rtnl_held);
int hdd_configure_cds(struct hdd_context *hdd_ctx);
int hdd_set_fw_params(struct hdd_adapter *adapter);

#ifdef MULTI_CLIENT_LL_SUPPORT
/**
 * wlan_hdd_deinit_multi_client_info_table() - to deinit multi client info table
 * @adapter: hdd vdev/net_device context
 *
 * Return: none
 */
void wlan_hdd_deinit_multi_client_info_table(struct hdd_adapter *adapter);
#else
static inline void
wlan_hdd_deinit_multi_client_info_table(struct hdd_adapter *adapter)
{}
#endif

/**
 * hdd_wlan_start_modules() - Single driver state machine for starting modules
 * @hdd_ctx: HDD context
 * @reinit: flag to indicate from SSR or normal path
 *
 * This function maintains the driver state machine it will be invoked from
 * startup, reinit and change interface. Depending on the driver state shall
 * perform the opening of the modules.
 *
 * Return: Errno
 */
int hdd_wlan_start_modules(struct hdd_context *hdd_ctx, bool reinit);

/**
 * hdd_wlan_stop_modules - Single driver state machine for stopping modules
 * @hdd_ctx: HDD context
 * @ftm_mode: ftm mode
 *
 * This function maintains the driver state machine it will be invoked from
 * exit, shutdown and con_mode change handler. Depending on the driver state
 * shall perform the stopping/closing of the modules.
 *
 * Return: Errno
 */
int hdd_wlan_stop_modules(struct hdd_context *hdd_ctx, bool ftm_mode);

/**
 * hdd_psoc_idle_timer_start() - start the idle psoc detection timer
 * @hdd_ctx: the hdd context for which the timer should be started
 *
 * Return: None
 */
void hdd_psoc_idle_timer_start(struct hdd_context *hdd_ctx);

/**
 * hdd_psoc_idle_timer_stop() - stop the idle psoc detection timer
 * @hdd_ctx: the hdd context for which the timer should be stopped
 *
 * Return: None
 */
void hdd_psoc_idle_timer_stop(struct hdd_context *hdd_ctx);

/**
 * hdd_trigger_psoc_idle_restart() - trigger restart of a previously shutdown
 *                                   idle psoc, if needed
 * @hdd_ctx: the hdd context which should be restarted
 *
 * This API does nothing if the given psoc is already active.
 *
 * Return: Errno
 */
int hdd_trigger_psoc_idle_restart(struct hdd_context *hdd_ctx);

int hdd_start_adapter(struct hdd_adapter *adapter, bool rtnl_held);
void hdd_populate_random_mac_addr(struct hdd_context *hdd_ctx, uint32_t num);
/**
 * hdd_is_interface_up()- Check if the given interface is up
 * @adapter: interface to check
 *
 * Checks whether the given interface was brought up by userspace.
 *
 * Return: true if interface was opened else false
 */
bool hdd_is_interface_up(struct hdd_adapter *adapter);

#ifdef WLAN_FEATURE_FASTPATH
void hdd_enable_fastpath(struct hdd_context *hdd_ctx,
			 void *context);
#else
static inline void hdd_enable_fastpath(struct hdd_context *hdd_ctx,
				       void *context)
{
}
#endif
void hdd_wlan_update_target_info(struct hdd_context *hdd_ctx, void *context);

enum  sap_acs_dfs_mode wlan_hdd_get_dfs_mode(enum dfs_mode mode);

/**
 * hdd_clone_local_unsafe_chan() - clone hdd ctx unsafe chan list
 * @hdd_ctx: hdd context pointer
 * @local_unsafe_list: copied unsafe chan list array
 * @local_unsafe_list_count: channel number in returned local_unsafe_list
 *
 * The function will allocate memory and make a copy the current unsafe
 * channels from hdd ctx. The caller need to free the local_unsafe_list
 * memory after use.
 *
 * Return: 0 if successfully clone unsafe chan list.
 */
int hdd_clone_local_unsafe_chan(struct hdd_context *hdd_ctx,
	uint16_t **local_unsafe_list, uint16_t *local_unsafe_list_count);

/**
 * hdd_local_unsafe_channel_updated() - check unsafe chan list same or not
 * @hdd_ctx: hdd context pointer
 * @local_unsafe_list: unsafe chan list to be compared with hdd_ctx's list
 * @local_unsafe_list_count: channel number in local_unsafe_list
 * @restriction_mask: restriction mask is to differentiate current channel
 * list different from previous channel list
 *
 * The function checked the input channel is same as current unsafe chan
 * list in hdd_ctx.
 *
 * Return: true if input channel list is same as the list in hdd_ctx
 */
bool hdd_local_unsafe_channel_updated(struct hdd_context *hdd_ctx,
	uint16_t *local_unsafe_list, uint16_t local_unsafe_list_count,
	uint32_t restriction_mask);

int hdd_enable_disable_ca_event(struct hdd_context *hddctx,
				uint8_t set_value);

/**
 * wlan_hdd_undo_acs : Do cleanup of DO_ACS
 * @link_info: Pointer of link_info in adapter
 *
 * This function handle cleanup of what was done in DO_ACS, including free
 * memory.
 *
 * Return: void
 */
void wlan_hdd_undo_acs(struct wlan_hdd_link_info *link_info);

/**
 * wlan_hdd_set_restriction_mask() - set restriction mask for hdd context
 * @hdd_ctx: hdd context pointer
 *
 * Return: None
 */
void wlan_hdd_set_restriction_mask(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_get_restriction_mask() - get restriction mask from hdd context
 * @hdd_ctx: hdd context pointer
 *
 * Return: restriction_mask
 */
uint32_t wlan_hdd_get_restriction_mask(struct hdd_context *hdd_ctx);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
static inline int
hdd_wlan_nla_put_u64(struct sk_buff *skb, int attrtype, u64 value)
{
	return nla_put_u64(skb, attrtype, value);
}
#else
static inline int
hdd_wlan_nla_put_u64(struct sk_buff *skb, int attrtype, u64 value)
{
	return nla_put_u64_64bit(skb, attrtype, value, NL80211_ATTR_PAD);
}
#endif

/**
 * hdd_roam_profile() - Get adapter's roam profile
 * @link_info: Link info pointer in HDD adapter
 *
 * Given an adapter this function returns a pointer to its roam profile.
 *
 * NOTE WELL: Caller is responsible for ensuring this interface is only
 * invoked for STA-type interfaces
 *
 * Return: pointer to the adapter's roam profile
 */
static inline struct csr_roam_profile *
hdd_roam_profile(struct wlan_hdd_link_info *link_info)
{
	struct hdd_station_ctx *sta_ctx;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(link_info);
	return &sta_ctx->roam_profile;
}

/**
 * hdd_is_roaming_in_progress() - check if roaming is in progress
 * @hdd_ctx: Global HDD context
 *
 * Checks if roaming is in progress on any of the adapters
 *
 * Return: true if roaming is in progress else false
 */
bool hdd_is_roaming_in_progress(struct hdd_context *hdd_ctx);

/**
 * hdd_is_connection_in_progress() - check if connection is in progress
 * @out_vdev_id: id of vdev where connection is occurring
 * @out_reason: scan reject reason
 *
 * Go through each adapter and check if connection is in progress.
 * Output parameters @out_vdev_id and @out_reason will only be written
 * when a connection is in progress.
 *
 * Return: true if connection is in progress else false
 */
bool hdd_is_connection_in_progress(uint8_t *out_vdev_id,
				   enum scan_reject_states *out_reason);

/**
 * hdd_restart_sap() - to restart SAP in driver internally
 * @link_info: Link info pointer of SAP adapter
 *
 * Return: None
 */
void hdd_restart_sap(struct wlan_hdd_link_info *link_info);
bool hdd_set_connection_in_progress(bool value);

/**
 * wlan_hdd_init_chan_info() - initialize channel info variables
 * @hdd_ctx: hdd ctx
 *
 * This API initialize channel info variables
 *
 * Return: None
 */
void wlan_hdd_init_chan_info(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_deinit_chan_info() - deinitialize channel info variables
 * @hdd_ctx: hdd ctx
 *
 * This API deinitialize channel info variables
 *
 * Return: None
 */
void wlan_hdd_deinit_chan_info(struct hdd_context *hdd_ctx);

/**
 * hdd_is_any_interface_open() - Check for interface up
 * @hdd_ctx: HDD context
 *
 * Return: true if any interface is open
 */
bool hdd_is_any_interface_open(struct hdd_context *hdd_ctx);

#ifdef WIFI_POS_CONVERGED
/**
 * hdd_send_peer_status_ind_to_app() - wrapper to call legacy or new wifi_pos
 * function to send peer status to a registered application
 * @peer_mac: MAC address of peer
 * @peer_status: ePeerConnected or ePeerDisconnected
 * @peer_timing_meas_cap: 0: RTT/RTT2, 1: RTT3. Default is 0
 * @vdev_id: ID of the underlying vdev
 * @chan_info: operating channel information
 * @dev_mode: dev mode for which indication is sent
 *
 * Return: none
 */
static inline void hdd_send_peer_status_ind_to_app(
					struct qdf_mac_addr *peer_mac,
					uint8_t peer_status,
					uint8_t peer_timing_meas_cap,
					uint8_t vdev_id,
					struct oem_channel_info *chan_info,
					enum QDF_OPMODE dev_mode)
{
	struct wifi_pos_ch_info ch_info;

	if (!chan_info) {
		os_if_wifi_pos_send_peer_status(peer_mac, peer_status,
						peer_timing_meas_cap, vdev_id,
						NULL, dev_mode);
		return;
	}

	/* chan_id is obsoleted by mhz */
	ch_info.chan_id = 0;
	ch_info.mhz = chan_info->mhz;
	ch_info.band_center_freq1 = chan_info->band_center_freq1;
	ch_info.band_center_freq2 = chan_info->band_center_freq2;
	ch_info.info = chan_info->info;
	ch_info.reg_info_1 = chan_info->reg_info_1;
	ch_info.reg_info_2 = chan_info->reg_info_2;
	ch_info.nss = chan_info->nss;
	ch_info.rate_flags = chan_info->rate_flags;
	ch_info.sec_ch_offset = chan_info->sec_ch_offset;
	ch_info.ch_width = chan_info->ch_width;
	os_if_wifi_pos_send_peer_status(peer_mac, peer_status,
					peer_timing_meas_cap, vdev_id,
					&ch_info, dev_mode);
}
#else
static inline void hdd_send_peer_status_ind_to_app(
					struct qdf_mac_addr *peer_mac,
					uint8_t peer_status,
					uint8_t peer_timing_meas_cap,
					uint8_t vdev_id,
					struct oem_channel_info *chan_info,
					enum QDF_OPMODE dev_mode)
{
	hdd_send_peer_status_ind_to_oem_app(peer_mac, peer_status,
			peer_timing_meas_cap, vdev_id, chan_info, dev_mode);
}
#endif /* WIFI_POS_CONVERGENCE */

/**
 * wlan_hdd_send_mcc_vdev_quota()- Send mcc vdev quota value to FW
 * @adapter: Adapter data
 * @sval:    mcc vdev quota value
 *
 * Send mcc vdev quota value value to FW
 *
 * Return: 0 success else failure
 */
int wlan_hdd_send_mcc_vdev_quota(struct hdd_adapter *adapter, int sval);

/**
 * wlan_hdd_send_mcc_latency()- Send MCC latency to FW
 * @adapter: Adapter data
 * @sval:    MCC latency value
 *
 * Send MCC latency value to FW
 *
 * Return: 0 success else failure
 */
int wlan_hdd_send_mcc_latency(struct hdd_adapter *adapter, int sval);

/**
 * wlan_hdd_get_link_info_from_vdev()- Get link info from vdev id
 * and PSOC object data
 * @psoc: Psoc object data
 * @vdev_id: vdev id
 *
 * Get link info from vdev id and PSOC object data
 *
 * Return: link info pointer
 */
struct wlan_hdd_link_info *
wlan_hdd_get_link_info_from_vdev(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id);

/**
 * hdd_unregister_notifiers()- unregister kernel notifiers
 * @hdd_ctx: Hdd Context
 *
 * Unregister netdev notifiers like Netdevice,IPv4 and IPv6.
 *
 */
void hdd_unregister_notifiers(struct hdd_context *hdd_ctx);

/**
 * hdd_dbs_scan_selection_init() - initialization for DBS scan selection config
 * @hdd_ctx: HDD context
 *
 * This function sends the DBS scan selection config configuration to the
 * firmware via WMA
 *
 * Return: 0 - success, < 0 - failure
 */
int hdd_dbs_scan_selection_init(struct hdd_context *hdd_ctx);

/**
 * hdd_update_scan_config - API to update scan configuration parameters
 * @hdd_ctx: HDD context
 *
 * Return: 0 if success else err
 */
int hdd_update_scan_config(struct hdd_context *hdd_ctx);

/**
 * hdd_start_complete()- complete the start event
 * @ret: return value for complete event.
 *
 * complete the startup event and set the return in
 * global variable
 *
 * Return: void
 */

void hdd_start_complete(int ret);

/**
 * hdd_chip_pwr_save_fail_detected_cb() - chip power save failure detected
 * callback
 * @hdd_handle: HDD handle
 * @data: chip power save failure detected data
 *
 * This function reads the chip power save failure detected data and fill in
 * the skb with NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */

void hdd_chip_pwr_save_fail_detected_cb(hdd_handle_t hdd_handle,
				struct chip_pwr_save_fail_detected_params
				*data);

/**
 * hdd_update_ie_allowlist_attr() - Copy probe req ie allowlist attrs from cfg
 * @ie_allowlist: output parameter
 * @hdd_ctx: pointer to hdd context
 *
 * Return: None
 */
void hdd_update_ie_allowlist_attr(struct probe_req_allowlist_attr *ie_allowlist,
				  struct hdd_context *hdd_ctx);

/**
 * hdd_get_rssi_snr_by_bssid() - gets the rssi and snr by bssid from scan cache
 * @mac_handle: MAC handle
 * @bssid: bssid to look for in scan cache
 * @rssi: rssi value found
 * @snr: snr value found
 *
 * Return: QDF_STATUS
 */
int hdd_get_rssi_snr_by_bssid(mac_handle_t mac_handle, const uint8_t *bssid,
			      int8_t *rssi, int8_t *snr);

/**
 * hdd_reset_limit_off_chan() - reset limit off-channel command parameters
 * @adapter: HDD adapter
 *
 * Return: 0 on success and non zero value on failure
 */
int hdd_reset_limit_off_chan(struct hdd_adapter *adapter);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
static inline void hdd_dev_setup_destructor(struct net_device *dev)
{
	dev->destructor = free_netdev;
}
#else
static inline void hdd_dev_setup_destructor(struct net_device *dev)
{
	dev->needs_free_netdev = true;
}
#endif /* KERNEL_VERSION(4, 12, 0) */

/**
 * hdd_update_score_config - API to update candidate scoring related params
 * configuration parameters
 * @hdd_ctx: hdd context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_update_score_config(struct hdd_context *hdd_ctx);

/**
 * hdd_get_stainfo() - get stainfo for the specified peer
 * @astainfo: array of the station info in which the sta info
 * corresponding to mac_addr needs to be searched
 * @mac_addr: mac address of requested peer
 *
 * This function find the stainfo for the peer with mac_addr
 *
 * Return: stainfo if found, NULL if not found
 */
struct hdd_station_info *hdd_get_stainfo(struct hdd_station_info *astainfo,
					 struct qdf_mac_addr mac_addr);

/**
 * hdd_component_psoc_open() - Open the legacy components
 * @psoc: Pointer to psoc object
 *
 * This function opens the legacy components and initializes the
 * component's private objects.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_component_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_psoc_close() - Close the legacy components
 * @psoc: Pointer to psoc object
 *
 * This function closes the legacy components and resets the
 * component's private objects.
 *
 * Return: None
 */
void hdd_component_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_psoc_enable() - Trigger psoc enable for CLD Components
 * @psoc: Pointer to psoc object
 *
 * Return: None
 */
void hdd_component_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_psoc_disable() - Trigger psoc disable for CLD Components
 * @psoc: Pointer to psoc object
 *
 * Return: None
 */
void hdd_component_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_pdev_open() - Trigger pdev open for CLD Components
 * @pdev: Pointer to pdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_component_pdev_open(struct wlan_objmgr_pdev *pdev);

/**
 * hdd_component_pdev_close() - Trigger pdev close for CLD Components
 * @pdev: Pointer to pdev object
 *
 * Return: None
 */
void hdd_component_pdev_close(struct wlan_objmgr_pdev *pdev);

#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
int hdd_driver_memdump_init(void);
void hdd_driver_memdump_deinit(void);

/**
 * hdd_driver_mem_cleanup() - Frees memory allocated for
 * driver dump
 *
 * This function  frees driver dump memory.
 *
 * Return: None
 */
void hdd_driver_mem_cleanup(void);

#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static inline int hdd_driver_memdump_init(void)
{
	return 0;
}
static inline void hdd_driver_memdump_deinit(void)
{
}

static inline void hdd_driver_mem_cleanup(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * wlan_hdd_set_mon_chan() - Set capture channel on the monitor mode interface.
 * @adapter: Handle to adapter
 * @freq: Monitor mode frequency (MHz)
 * @bandwidth: Capture channel bandwidth
 *
 * Return: 0 on success else error code.
 */
int wlan_hdd_set_mon_chan(struct hdd_adapter *adapter, qdf_freq_t freq,
			  uint32_t bandwidth);
#else
static inline
int wlan_hdd_set_mon_chan(struct hdd_adapter *adapter, qdf_freq_t freq,
			  uint32_t bandwidth)
{
	return 0;
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC) && \
	!defined(WLAN_HDD_MULTI_VDEV_SINGLE_NDEV)
/**
 *  hdd_set_mld_address() - Set the MLD address of the adapter
 *  @adapter: Handle to adapter
 *  @mac_addr: MAC address to be copied
 *
 *  The function copies the MAC address sent in @mac_addr to
 *  the adapter's MLD address and the MLD address of each
 *  link adapter mapped of the @adapter.
 *  The mode of operation must be 11be capable and @adapter
 *  has to be ML type.
 *
 *  Return: void
 */
void
hdd_set_mld_address(struct hdd_adapter *adapter,
		    const struct qdf_mac_addr *mac_addr);
#else
static inline void
hdd_set_mld_address(struct hdd_adapter *adapter,
		    const struct qdf_mac_addr *mac_addr)
{
}
#endif

/**
 * hdd_wlan_get_version() - Get version information
 * @hdd_ctx: Global HDD context
 * @version_len: length of the version buffer size
 * @version: the buffer to the version string
 *
 * This function is used to get Wlan Driver, Firmware, Hardware Version
 * & the Board related information.
 *
 * Return: the length of the version string
 */
uint32_t hdd_wlan_get_version(struct hdd_context *hdd_ctx,
			      const size_t version_len, uint8_t *version);
/**
 * hdd_assemble_rate_code() - assemble rate code to be sent to FW
 * @preamble: rate preamble
 * @nss: number of streams
 * @rate: rate index
 *
 * Rate code assembling is different for targets which are 11ax capable.
 * Check for the target support and assemble the rate code accordingly.
 *
 * Return: assembled rate code
 */
int hdd_assemble_rate_code(uint8_t preamble, uint8_t nss, uint8_t rate);

/**
 * hdd_update_country_code - Update country code
 * @hdd_ctx: HDD context
 *
 * Update country code based on module parameter country_code
 *
 * Return: 0 on success and errno on failure
 */
int hdd_update_country_code(struct hdd_context *hdd_ctx);

/**
 * hdd_set_11ax_rate() - set 11ax rate
 * @adapter: adapter being modified
 * @value: new 11ax rate code
 * @sap_config: pointer to SAP config to check HW mode
 *		this will be NULL for call from STA persona
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_11ax_rate(struct hdd_adapter *adapter, int value,
		      struct sap_config *sap_config);

/**
 * hdd_update_hw_sw_info() - API to update the HW/SW information
 * @hdd_ctx: Global HDD context
 *
 * API to update the HW and SW information in the driver
 *
 * Note:
 * All the version/revision information would only be retrieved after
 * firmware download
 *
 * Return: None
 */
void hdd_update_hw_sw_info(struct hdd_context *hdd_ctx);

/**
 * hdd_context_get_mac_handle() - get mac handle from hdd context
 * @hdd_ctx: Global HDD context pointer
 *
 * Retrieves the global MAC handle from the HDD context
 *
 * Return: The global MAC handle (which may be NULL)
 */
static inline
mac_handle_t hdd_context_get_mac_handle(struct hdd_context *hdd_ctx)
{
	return hdd_ctx ? hdd_ctx->mac_handle : NULL;
}

/**
 * hdd_adapter_get_mac_handle() - get mac handle from hdd adapter
 * @adapter: HDD adapter pointer
 *
 * Retrieves the global MAC handle given an HDD adapter
 *
 * Return: The global MAC handle (which may be NULL)
 */
static inline
mac_handle_t hdd_adapter_get_mac_handle(struct hdd_adapter *adapter)
{
	return adapter ?
		hdd_context_get_mac_handle(adapter->hdd_ctx) : NULL;
}

/**
 * hdd_handle_to_context() - turn an HDD handle into an HDD context
 * @hdd_handle: HDD handle to be converted
 *
 * Return: HDD context referenced by @hdd_handle
 */
static inline
struct hdd_context *hdd_handle_to_context(hdd_handle_t hdd_handle)
{
	return (struct hdd_context *)hdd_handle;
}

/**
 * wlan_hdd_free_cache_channels() - Free the cache channels list
 * @hdd_ctx: Pointer to HDD context
 *
 * Return: None
 */
void wlan_hdd_free_cache_channels(struct hdd_context *hdd_ctx);

/**
 * hdd_update_dynamic_mac() - Updates the dynamic MAC list
 * @hdd_ctx: Pointer to HDD context
 * @curr_mac_addr: Current interface mac address
 * @new_mac_addr: New mac address which needs to be updated
 *
 * This function updates newly configured MAC address to the
 * dynamic MAC address list corresponding to the current
 * adapter MAC address
 *
 * Return: None
 */
void hdd_update_dynamic_mac(struct hdd_context *hdd_ctx,
			    struct qdf_mac_addr *curr_mac_addr,
			    struct qdf_mac_addr *new_mac_addr);

#ifdef WLAN_FEATURE_MOTION_DETECTION
/**
 * hdd_md_host_evt_cb - Callback for Motion Detection Event
 * @ctx: HDD context
 * @event: motion detect event
 *
 * Callback for Motion Detection Event. Re-enables Motion
 * Detection again upon event
 *
 * Return: QDF_STATUS QDF_STATUS_SUCCESS on Success and
 * QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS hdd_md_host_evt_cb(void *ctx, struct sir_md_evt *event);

/**
 * hdd_md_bl_evt_cb - Callback for Motion Detection Baseline Event
 * @ctx: HDD context
 * @event: motion detect baseline event
 *
 * Callback for Motion Detection Baseline Event
 *
 * Return: QDF_STATUS QDF_STATUS_SUCCESS on Success and
 * QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS hdd_md_bl_evt_cb(void *ctx, struct sir_md_bl_evt *event);
#endif /* WLAN_FEATURE_MOTION_DETECTION */

/**
 * hdd_hidden_ssid_enable_roaming() - enable roaming after hidden ssid rsp
 * @hdd_handle: Hdd handler
 * @vdev_id: Vdev Id
 *
 * This is a wrapper function to enable roaming after getting hidden
 * ssid rsp
 */
void hdd_hidden_ssid_enable_roaming(hdd_handle_t hdd_handle, uint8_t vdev_id);

/**
 * hdd_psoc_idle_shutdown - perform idle shutdown after interface inactivity
 *                          timeout
 * @dev: pointer to struct device
 *
 * Return: 0 for success non-zero error code for failure
 */
int hdd_psoc_idle_shutdown(struct device *dev);

/**
 * hdd_psoc_idle_restart - perform idle restart after idle shutdown
 * @dev: pointer to struct device
 *
 * Return: 0 for success non-zero error code for failure
 */
int hdd_psoc_idle_restart(struct device *dev);

/**
 * hdd_adapter_is_ap() - whether adapter is ap or not
 * @adapter: adapter to check
 * Return: true if it is AP
 */
bool hdd_adapter_is_ap(struct hdd_adapter *adapter);

/**
 * hdd_common_roam_callback() - common sme roam callback
 * @psoc: Object Manager Psoc
 * @session_id: session id for which callback is called
 * @roam_info: pointer to roam info
 * @roam_status: roam status
 * @roam_result: roam result
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS hdd_common_roam_callback(struct wlan_objmgr_psoc *psoc,
				    uint8_t session_id,
				    struct csr_roam_info *roam_info,
				    eRoamCmdStatus roam_status,
				    eCsrRoamResult roam_result);

#ifdef WLAN_FEATURE_PKT_CAPTURE
/**
 * wlan_hdd_is_mon_concurrency() - check if MONITOR and STA concurrency
 * is UP when packet capture mode is enabled.
 *
 * Return: True - if STA and monitor concurrency is there, else False
 *
 */
bool wlan_hdd_is_mon_concurrency(void);

/**
 * wlan_hdd_del_monitor() - delete monitor interface
 * @hdd_ctx: pointer to hdd context
 * @adapter: adapter to be deleted
 * @rtnl_held: rtnl lock held
 *
 * This function is invoked to delete monitor interface.
 *
 * Return: None
 */
void wlan_hdd_del_monitor(struct hdd_context *hdd_ctx,
			  struct hdd_adapter *adapter, bool rtnl_held);

/**
 * wlan_hdd_del_p2p_interface() - delete p2p interface
 * @hdd_ctx: pointer to hdd context
 *
 * This function is invoked to delete p2p interface.
 *
 * Return: None
 */
void
wlan_hdd_del_p2p_interface(struct hdd_context *hdd_ctx);

/**
 * hdd_reset_monitor_interface() - reset monitor interface flags
 * @sta_adapter: station adapter
 *
 * Return: void
 */
void hdd_reset_monitor_interface(struct hdd_adapter *sta_adapter);

/**
 * hdd_is_pkt_capture_mon_enable() - Is packet capture monitor mode enable
 * @sta_adapter: station adapter
 *
 * Return: status of packet capture monitor adapter
 */
struct hdd_adapter *
hdd_is_pkt_capture_mon_enable(struct hdd_adapter *sta_adapter);
#else
static inline
void wlan_hdd_del_monitor(struct hdd_context *hdd_ctx,
			  struct hdd_adapter *adapter, bool rtnl_held)
{
}

static inline
bool wlan_hdd_is_mon_concurrency(void)
{
	return false;
}

static inline
void wlan_hdd_del_p2p_interface(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_reset_monitor_interface(struct hdd_adapter *sta_adapter)
{
}

static inline int hdd_is_pkt_capture_mon_enable(struct hdd_adapter *adapter)
{
	return 0;
}
#endif /* WLAN_FEATURE_PKT_CAPTURE */
/**
 * wlan_hdd_is_session_type_monitor() - check if session type is MONITOR
 * @session_type: session type
 *
 * Return: True - if session type for adapter is monitor, else False
 *
 */
bool wlan_hdd_is_session_type_monitor(uint8_t session_type);

/**
 * wlan_hdd_add_monitor_check() - check for monitor intf and add if needed
 * @hdd_ctx: pointer to hdd context
 * @adapter: output pointer to hold created monitor adapter
 * @name: name of the interface
 * @rtnl_held: True if RTNL lock is held
 * @name_assign_type: the name of assign type of the netdev
 *
 * Return: 0 - on success
 *         err code - on failure
 */
int wlan_hdd_add_monitor_check(struct hdd_context *hdd_ctx,
			       struct hdd_adapter **adapter,
			       const char *name, bool rtnl_held,
			       unsigned char name_assign_type);

#ifdef CONFIG_WLAN_DEBUG_CRASH_INJECT
/**
 * hdd_crash_inject() - Inject a crash
 * @adapter: Adapter upon which the command was received
 * @v1: first value to inject
 * @v2: second value to inject
 *
 * This function is the handler for the crash inject debug feature.
 * This feature only exists for internal testing and must not be
 * enabled on a production device.
 *
 * Return: 0 on success and errno on failure
 */
int hdd_crash_inject(struct hdd_adapter *adapter, uint32_t v1, uint32_t v2);
#else
static inline
int hdd_crash_inject(struct hdd_adapter *adapter, uint32_t v1, uint32_t v2)
{
	return -ENOTSUPP;
}
#endif

#ifdef FEATURE_MONITOR_MODE_SUPPORT

void hdd_sme_monitor_mode_callback(uint8_t vdev_id);

QDF_STATUS hdd_monitor_mode_vdev_status(struct hdd_adapter *adapter);

QDF_STATUS hdd_monitor_mode_qdf_create_event(struct hdd_adapter *adapter,
					     uint8_t session_type);
#else
static inline void hdd_sme_monitor_mode_callback(uint8_t vdev_id) {}

static inline QDF_STATUS
hdd_monitor_mode_vdev_status(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
hdd_monitor_mode_qdf_create_event(struct hdd_adapter *adapter,
				  uint8_t session_type)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * hdd_cleanup_conn_info() - Cleanup connectin info
 * @link_info: pointer to link_info struct in adapter
 *
 * This function frees the memory allocated for the connection
 * info structure
 *
 * Return: none
 */
void hdd_cleanup_conn_info(struct wlan_hdd_link_info *link_info);

#ifdef FEATURE_WLAN_RESIDENT_DRIVER
extern char *country_code;
extern int con_mode;
extern const struct kernel_param_ops con_mode_ops;
extern int con_mode_ftm;
extern const struct kernel_param_ops con_mode_ftm_ops;
#endif

/**
 * hdd_driver_load() - Perform the driver-level load operation
 *
 * Note: this is used in both static and DLKM driver builds
 *
 * Return: Errno
 */
int hdd_driver_load(void);

/**
 * hdd_driver_unload() - Performs the driver-level unload operation
 *
 * Note: this is used in both static and DLKM driver builds
 *
 * Return: None
 */
void hdd_driver_unload(void);

/**
 * hdd_init_start_completion() - Init the completion variable to wait on ON/OFF
 *
 * Return: None
 */
void hdd_init_start_completion(void);

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
/**
 * hdd_beacon_latency_event_cb() - Callback function to get latency level
 * @latency_level: latency level received from firmware
 *
 * Return: None
 */
void hdd_beacon_latency_event_cb(uint32_t latency_level);
#else
static inline void hdd_beacon_latency_event_cb(uint32_t latency_level)
{
}
#endif

#if defined(CLD_PM_QOS) || defined(FEATURE_RUNTIME_PM)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
/**
 * wlan_hdd_get_default_pm_qos_cpu_latency() - get default PM QOS CPU latency
 *
 * Return: PM QOS CPU latency value
 */
static inline unsigned long wlan_hdd_get_default_pm_qos_cpu_latency(void)
{
	return PM_QOS_CPU_LATENCY_DEFAULT_VALUE;
}
#else
static inline unsigned long wlan_hdd_get_default_pm_qos_cpu_latency(void)
{
	return PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0) */
#endif /* defined(CLD_PM_QOS) || defined(FEATURE_RUNTIME_PM) */

/**
 * hdd_get_wifi_standard() - Get wifi standard
 * @hdd_ctx: hdd context pointer
 * @dot11_mode: hdd dot11 mode
 * @band_capability: band capability bitmap
 *
 * Return: WMI_HOST_WIFI_STANDARD
 */
WMI_HOST_WIFI_STANDARD
hdd_get_wifi_standard(struct hdd_context *hdd_ctx,
		      enum hdd_dot11_mode dot11_mode, uint32_t band_capability);

/**
 * hdd_is_runtime_pm_enabled - if runtime pm enabled
 * @hdd_ctx: hdd context
 *
 * Return: true if runtime pm enabled. false if disabled.
 */
bool hdd_is_runtime_pm_enabled(struct hdd_context *hdd_ctx);

/**
 * hdd_netdev_update_features() - Update the netdev features
 * @adapter: adapter associated with the net_device
 *
 * This func holds the rtnl_lock. Do not call with rtnl_lock held.
 *
 * Return: None
 */
void hdd_netdev_update_features(struct hdd_adapter *adapter);

/**
 * hdd_stop_no_trans() - HDD stop function
 * @dev:	Pointer to net_device structure
 *
 * This is called in response to ifconfig down. Vdev sync transaction
 * should be started before calling this API.
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_stop_no_trans(struct net_device *dev);

#if defined(CLD_PM_QOS)
/**
 * wlan_hdd_set_pm_qos_request() - Function to set pm_qos config in wlm mode
 * @hdd_ctx: HDD context
 * @pm_qos_request: pm_qos_request flag
 *
 * Return: None
 */
void wlan_hdd_set_pm_qos_request(struct hdd_context *hdd_ctx,
				 bool pm_qos_request);
#else
static inline
void wlan_hdd_set_pm_qos_request(struct hdd_context *hdd_ctx,
				 bool pm_qos_request)
{
}
#endif

/**
 * hdd_nl80211_chwidth_to_chwidth - Get sir chan width from nl chan width
 * @nl80211_chwidth: enum nl80211_chan_width
 *
 * Return: enum eSirMacHTChannelWidth or -INVAL for unsupported nl chan width
 */
enum eSirMacHTChannelWidth
hdd_nl80211_chwidth_to_chwidth(uint8_t nl80211_chwidth);

/**
 * hdd_chwidth_to_nl80211_chwidth - Get nl chan width from sir chan width
 * @chwidth: enum eSirMacHTChannelWidth
 *
 * Return: enum nl80211_chan_width or 0xFF for unsupported sir chan width
 */
uint8_t hdd_chwidth_to_nl80211_chwidth(enum eSirMacHTChannelWidth chwidth);

/**
 * hdd_phy_chwidth_to_nl80211_chwidth() - Get nl chan width from phy chan width
 * @chwidth: enum phy_ch_width
 *
 * Return: enum nl80211_chan_width or 0xFF for unsupported phy chan width
 */
uint8_t hdd_phy_chwidth_to_nl80211_chwidth(enum phy_ch_width chwidth);

/**
 * wlan_hdd_get_channel_bw() - get channel bandwidth
 * @width: input channel width in nl80211_chan_width value
 *
 * Return: channel width value defined by driver
 */
enum hw_mode_bandwidth wlan_hdd_get_channel_bw(enum nl80211_chan_width width);

/**
 * hdd_ch_width_str() - Get string for channel width
 * @ch_width: channel width from connect info
 *
 * Return: User readable string for channel width
 */
uint8_t *hdd_ch_width_str(enum phy_ch_width ch_width);

/**
 * hdd_we_set_ch_width - Function to update channel width
 * @link_info: Link info pointer in HDD adapter.
 * @ch_width: enum eSirMacHTChannelWidth
 *
 * Return: 0 for success otherwise failure
 */
int hdd_we_set_ch_width(struct wlan_hdd_link_info *link_info, int ch_width);

/**
 * hdd_stop_adapter_ext: close/delete the vdev session in host/fw.
 * @hdd_ctx: HDD context
 * @adapter: Pointer to hdd_adapter
 *
 * Close/delete the vdev session in host/firmware.
 */
QDF_STATUS hdd_stop_adapter_ext(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter);

/**
 * hdd_check_for_net_dev_ref_leak: check for vdev reference leak in driver
 * @adapter: Pointer to hdd_adapter
 *
 * various function take netdev reference to get protected against netdev
 * getting deleted in parallel, check if all those references are cleanly
 * released.
 */
void hdd_check_for_net_dev_ref_leak(struct hdd_adapter *adapter);

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_HDD_MULTI_VDEV_SINGLE_NDEV)

/**
 * hdd_link_switch_vdev_mac_addr_update() - API to update OSIF/HDD on VDEV
 * mac addr update due to link switch.
 * @ieee_old_link_id: Current  IEEE link ID of VDEV prior to link switch
 * @ieee_new_link_id: New IEEE link ID of VDEV post link switch
 * @vdev_id: VDEV undergoing link switch.
 *
 * Check if both @ieee_old_link_id and @ieee_new_link_id are part of adapter
 * corresponding to @vdev_id. Then take necessary actions to support link switch
 * MAC update and update DP to change link MAC address to new link's address.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
hdd_link_switch_vdev_mac_addr_update(int32_t ieee_old_link_id,
				     int32_t ieee_new_link_id, uint8_t vdev_id);

/**
 * hdd_get_link_info_by_ieee_link_id() - Find link info pointer matching with
 * IEEE link ID.
 * @adapter: HDD adapter
 * @link_id: IEEE link ID to search for.
 *
 * Search the station ctx connection info for matching link ID in @adapter and
 * return the link info pointer on match. The IEEE link ID is updated in station
 * context during MLO connection and reset on disconnection.
 *
 * Return: link info pointer
 */
struct wlan_hdd_link_info *
hdd_get_link_info_by_ieee_link_id(struct hdd_adapter *adapter, int32_t link_id);
#endif

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
/**
 * hdd_dynamic_mac_address_set(): API to set MAC address, when interface
 *                                is up.
 * @link_info: Link info pointer in HDD adapter
 * @mac_addr: MAC address to set
 * @mld_addr: MLD address to set
 * @update_self_peer: Set to true to update self peer's address
 *
 * This API is used to update the current VDEV MAC address.
 *
 * Return: 0 for success. non zero valure for failure.
 */
int hdd_dynamic_mac_address_set(struct wlan_hdd_link_info *link_info,
				struct qdf_mac_addr mac_addr,
				struct qdf_mac_addr mld_addr,
				bool update_self_peer);

/**
 * hdd_is_dynamic_set_mac_addr_allowed() - API to check dynamic MAC address
 *				           update is allowed or not
 * @adapter: Pointer to the adapter structure
 *
 * Return: true or false
 */
bool hdd_is_dynamic_set_mac_addr_allowed(struct hdd_adapter *adapter);

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
/**
 * hdd_update_vdev_mac_address() - Update VDEV MAC address dynamically
 * @adapter: Pointer to HDD adapter
 * @mac_addr: MAC address to be updated
 *
 * API to update VDEV MAC address during interface is in UP state.
 *
 * Return: 0 for Success. Error code for failure
 */
int hdd_update_vdev_mac_address(struct hdd_adapter *adapter,
				struct qdf_mac_addr mac_addr);
#else
static inline int hdd_update_vdev_mac_address(struct hdd_adapter *adapter,
					      struct qdf_mac_addr mac_addr)
{
	struct qdf_mac_addr mld_addr = QDF_MAC_ADDR_ZERO_INIT;

	return hdd_dynamic_mac_address_set(adapter->deflink, mac_addr,
					   mld_addr, true);
}
#endif /* WLAN_FEATURE_11BE_MLO */
#else
static inline int hdd_update_vdev_mac_address(struct hdd_adapter *adapter,
					      struct qdf_mac_addr mac_addr)
{
	return 0;
}

static inline int
hdd_dynamic_mac_address_set(struct wlan_hdd_link_info *link_info,
			    struct qdf_mac_addr mac_addr,
			    struct qdf_mac_addr mld_addr,
			    bool update_self_peer)
{
	return 0;
}

static inline bool
hdd_is_dynamic_set_mac_addr_allowed(struct hdd_adapter *adapter)
{
	return false;
}

#endif /* WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE */

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && \
defined(FEATURE_RX_LINKSPEED_ROAM_TRIGGER)
/**
 * wlan_hdd_link_speed_update() - Update link speed to F/W
 * @psoc: pointer to soc
 * @vdev_id: Vdev ID
 * @is_link_speed_good: true means good link speed,  false means bad link speed
 *
 * Return: None
 */
void wlan_hdd_link_speed_update(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				bool is_link_speed_good);
#else
static inline void wlan_hdd_link_speed_update(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id,
					      bool is_link_speed_good)
{}
#endif

/**
 * hdd_update_multicast_list() - update the multicast list
 * @vdev: pointer to VDEV object
 *
 * Return: none
 */
void hdd_update_multicast_list(struct wlan_objmgr_vdev *vdev);

/**
 * hdd_set_sar_init_index() - Set SAR safety index at init.
 * @hdd_ctx: HDD context
 *
 */
#ifdef SAR_SAFETY_FEATURE
void hdd_set_sar_init_index(struct hdd_context *hdd_ctx);
#else
static inline void hdd_set_sar_init_index(struct hdd_context *hdd_ctx)
{}
#endif
/**
 * hdd_send_coex_traffic_shaping_mode() - Send coex traffic shaping mode
 * to FW
 * @vdev_id: vdev ID
 * @mode: traffic shaping mode
 *
 * This function is used to send coex traffic shaping mode to FW
 *
 * Return: 0 on success and -EINVAL on failure
 */
int hdd_send_coex_traffic_shaping_mode(uint8_t vdev_id, uint8_t mode);

#ifdef WLAN_FEATURE_LOCAL_PKT_CAPTURE
/**
 * wlan_hdd_lpc_handle_concurrency() - Handle local packet capture
 * concurrency scenario
 * @hdd_ctx: hdd_ctx
 * @is_virtual_iface: is virtual interface
 *
 * This function takes care of handling concurrency scenario
 * If STA+Mon present and SAP is coming up, terminate Mon and let SAP come up
 * If STA+Mon present and P2P is coming up, terminate Mon and let P2P come up
 * If STA+Mon present and NAN is coming up, terminate Mon and let NAN come up
 *
 * Return: none
 */
void wlan_hdd_lpc_handle_concurrency(struct hdd_context *hdd_ctx,
				     bool is_virtual_iface);

/**
 * hdd_lpc_is_work_scheduled() - function to return if lpc wq scheduled
 * @hdd_ctx: hdd_ctx
 *
 * Return: true if scheduled; false otherwise
 */
bool hdd_lpc_is_work_scheduled(struct hdd_context *hdd_ctx);

#else
static inline void
wlan_hdd_lpc_handle_concurrency(struct hdd_context *hdd_ctx,
				bool is_virtual_iface)
{}

static inline bool
hdd_lpc_is_work_scheduled(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

#endif /* end #if !defined(WLAN_HDD_MAIN_H) */
