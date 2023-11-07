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
 *  DOC: wlan_hdd_main.c
 *
 *  WLAN Host Device Driver implementation
 *
 */

/* Include Files */
#include <wbuff.h>
#include "cfg_ucfg_api.h"
#include <wlan_hdd_includes.h>
#include <cds_api.h>
#include <cds_sched.h>
#include <linux/cpu.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <wlan_hdd_tx_rx.h>
#include <wni_api.h>
#include <wlan_hdd_cfg.h>
#include <wlan_ptt_sock_svc.h>
#include <dbglog_host.h>
#include <wlan_logging_sock_svc.h>
#include <wlan_roam_debug.h>
#include <wlan_hdd_connectivity_logging.h>
#include "osif_sync.h"
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_wext.h>
#include "wlan_hdd_trace.h"
#include "wlan_hdd_ioctl.h"
#include "wlan_hdd_ftm.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_stats.h"
#include "wlan_hdd_scan.h"
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_osif_priv.h"
#include <wlan_osif_request_manager.h>
#ifdef CONFIG_LEAK_DETECTION
#include "qdf_debug_domain.h"
#endif
#include "qdf_delayed_work.h"
#include "qdf_periodic_work.h"
#include "qdf_str.h"
#include "qdf_talloc.h"
#include "qdf_trace.h"
#include "qdf_types.h"
#include "qdf_net_if.h"
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_misc.h>
#include <cdp_txrx_stats.h>
#include "cdp_txrx_flow_ctrl_legacy.h"
#include "qdf_ssr_driver_dump.h"

#include <net/addrconf.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <linux/inetdevice.h>
#include <net/addrconf.h>
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_ext_scan.h"
#include "wlan_hdd_p2p.h"
#include <linux/rtnetlink.h>
#include "sap_api.h"
#include <linux/semaphore.h>
#include <linux/ctype.h>
#include <linux/compat.h>
#include <linux/ethtool.h>
#include <linux/suspend.h>

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
#include "qdf_periodic_work.h"
#endif

#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_green_ap.h>
#include "qwlan_version.h"
#include "wma_types.h"
#include "wlan_hdd_tdls.h"
#ifdef FEATURE_WLAN_CH_AVOID
#include "cds_regdomain.h"
#endif /* FEATURE_WLAN_CH_AVOID */
#include "cdp_txrx_flow_ctrl_v2.h"
#include "pld_common.h"
#include "wlan_hdd_ocb.h"
#include "wlan_hdd_nan.h"
#include "wlan_hdd_debugfs.h"
#include "wlan_hdd_debugfs_csr.h"
#include "wlan_hdd_driver_ops.h"
#include "epping_main.h"
#include "wlan_hdd_data_stall_detection.h"
#include "wlan_hdd_mpta_helper.h"

#include <wlan_hdd_ipa.h>
#include "hif.h"
#include "wma.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_hdd_tsf.h"
#include "bmi.h"
#include <wlan_hdd_regulatory.h>
#include "wlan_hdd_lpass.h"
#include "wlan_nan_api.h"
#include <wlan_hdd_napi.h>
#include "wlan_hdd_disa.h"
#include <dispatcher_init_deinit.h>
#include "wlan_hdd_object_manager.h"
#include "cds_utils.h"
#include <cdp_txrx_handle.h>
#include <qca_vendor.h>
#include "wlan_pmo_ucfg_api.h"
#include "sir_api.h"
#include "os_if_wifi_pos.h"
#include "wifi_pos_api.h"
#include "wlan_hdd_oemdata.h"
#include "wlan_hdd_he.h"
#include "os_if_nan.h"
#include "nan_public_structs.h"
#include "nan_ucfg_api.h"
#include "wlan_reg_ucfg_api.h"
#include "wlan_dfs_ucfg_api.h"
#include "wlan_hdd_rx_monitor.h"
#include "sme_power_save_api.h"
#include "enet.h"
#include <cdp_txrx_cmn_struct.h>
#include "wlan_hdd_sysfs.h"
#include "wlan_disa_ucfg_api.h"
#include "wlan_disa_obj_mgmt_api.h"
#include "wlan_action_oui_ucfg_api.h"
#include "wlan_ipa_ucfg_api.h"
#include <target_if.h>
#include "wlan_hdd_nud_tracking.h"
#include "wlan_hdd_apf.h"
#include "wlan_hdd_twt.h"
#include "qc_sap_ioctl.h"
#include "wlan_mlme_main.h"
#include "wlan_p2p_cfg_api.h"
#include "wlan_cfg80211_p2p.h"
#include "wlan_cfg80211_interop_issues_ap.h"
#include "wlan_tdls_cfg_api.h"
#include <wlan_hdd_rssi_monitor.h>
#include "wlan_mlme_ucfg_api.h"
#include "wlan_mlme_twt_ucfg_api.h"
#include "wlan_fwol_ucfg_api.h"
#include "wlan_policy_mgr_ucfg.h"
#include "qdf_func_tracker.h"
#include "pld_common.h"
#include "wlan_hdd_pre_cac.h"

#include "sme_api.h"

#ifdef CNSS_GENL
#ifdef CONFIG_CNSS_OUT_OF_TREE
#include "cnss_nl.h"
#else
#include <net/cnss_nl.h>
#endif
#endif
#include "wlan_reg_ucfg_api.h"
#include "wlan_ocb_ucfg_api.h"
#include <wlan_hdd_spectralscan.h>
#include "wlan_green_ap_ucfg_api.h"
#include <wlan_p2p_ucfg_api.h>
#include <wlan_interop_issues_ap_ucfg_api.h>
#include <target_type.h>
#include <wlan_hdd_debugfs_coex.h>
#include <wlan_hdd_debugfs_config.h>
#include "wlan_dlm_ucfg_api.h"
#include "ftm_time_sync_ucfg_api.h"
#include "wlan_pre_cac_ucfg_api.h"
#include "ol_txrx.h"
#include "wlan_hdd_sta_info.h"
#include "mac_init_api.h"
#include "wlan_pkt_capture_ucfg_api.h"
#include <wlan_hdd_sar_limits.h>
#include "cfg_nan_api.h"
#include "wlan_hdd_btc_chain_mode.h"
#include <wlan_hdd_dcs.h>
#include "wlan_hdd_debugfs_unit_test.h"
#include "wlan_hdd_debugfs_mibstat.h"
#include <wlan_hdd_hang_event.h>
#include "wlan_global_lmac_if_api.h"
#include "wlan_coex_ucfg_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_cm_roam_ucfg_api.h"
#include <cdp_txrx_ctrl.h>
#include "qdf_lock.h"
#include "wlan_hdd_thermal.h"
#include "osif_cm_util.h"
#include "wlan_hdd_gpio_wakeup.h"
#include "wlan_hdd_bootup_marker.h"
#include "wlan_dp_ucfg_api.h"
#include "wlan_hdd_medium_assess.h"
#include "wlan_hdd_eht.h"
#include <linux/bitfield.h>
#include "wlan_hdd_mlo.h"
#include <wlan_hdd_son.h>
#ifdef WLAN_FEATURE_11BE_MLO
#include <wlan_mlo_mgr_ap.h>
#endif
#include "wlan_osif_features.h"
#include "wlan_vdev_mgr_ucfg_api.h"
#include <wlan_objmgr_psoc_obj_i.h>
#include <wlan_objmgr_vdev_obj_i.h>
#include "wifi_pos_ucfg_api.h"
#include "osif_vdev_mgr_util.h"
#include <son_ucfg_api.h>
#include "osif_twt_util.h"
#include "wlan_twt_ucfg_ext_api.h"
#include "wlan_hdd_mcc_quota.h"
#include "osif_pre_cac.h"
#include "wlan_hdd_pre_cac.h"
#include "wlan_osif_features.h"
#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
#include <net/pkt_cls.h>
#endif
#include "wlan_dp_public_struct.h"
#include "os_if_dp.h"
#include <wlan_dp_ucfg_api.h>
#include "wlan_psoc_mlme_ucfg_api.h"
#include "os_if_qmi.h"
#include "wlan_qmi_ucfg_api.h"

#ifdef MULTI_CLIENT_LL_SUPPORT
#define WLAM_WLM_HOST_DRIVER_PORT_ID 0xFFFFFF
#endif

#ifdef MODULE
#ifdef WLAN_WEAR_CHIPSET
#define WLAN_MODULE_NAME  "wlan"
#else
#define WLAN_MODULE_NAME  module_name(THIS_MODULE)
#endif
#else
#define WLAN_MODULE_NAME  "wlan"
#endif

#ifdef TIMER_MANAGER
#define TIMER_MANAGER_STR " +TIMER_MANAGER"
#else
#define TIMER_MANAGER_STR ""
#endif

#ifdef MEMORY_DEBUG
#define MEMORY_DEBUG_STR " +MEMORY_DEBUG"
#else
#define MEMORY_DEBUG_STR ""
#endif

#ifdef PANIC_ON_BUG
#define PANIC_ON_BUG_STR " +PANIC_ON_BUG"
#else
#define PANIC_ON_BUG_STR ""
#endif

/* PCIe gen speed change idle shutdown timer 100 milliseconds */
#define HDD_PCIE_GEN_SPEED_CHANGE_TIMEOUT_MS (100)

#define MAX_NET_DEV_REF_LEAK_ITERATIONS 10
#define NET_DEV_REF_LEAK_ITERATION_SLEEP_TIME_MS 10

#ifdef FEATURE_TSO
#define TSO_FEATURE_FLAGS (NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_SG)
#else
#define TSO_FEATURE_FLAGS 0
#endif

int wlan_start_ret_val;
static DECLARE_COMPLETION(wlan_start_comp);
static qdf_atomic_t wlan_hdd_state_fops_ref;
#ifndef MODULE
static struct gwlan_loader *wlan_loader;
static ssize_t wlan_boot_cb(struct kobject *kobj,
			    struct kobj_attribute *attr,
			    const char *buf, size_t count);
struct gwlan_loader {
	bool loaded_state;
	struct kobject *boot_wlan_obj;
	struct attribute_group *attr_group;
};

static struct kobj_attribute wlan_boot_attribute =
	__ATTR(boot_wlan, 0220, NULL, wlan_boot_cb);

static struct attribute *attrs[] = {
	&wlan_boot_attribute.attr,
	NULL,
};
#define MODULE_INITIALIZED 1

#ifdef MULTI_IF_NAME
#define WLAN_LOADER_NAME "boot_" MULTI_IF_NAME
#else
#define WLAN_LOADER_NAME "boot_wlan"
#endif
#endif

/* the Android framework expects this param even though we don't use it */
#define BUF_LEN 20
static char fwpath_buffer[BUF_LEN];
static struct kparam_string fwpath = {
	.string = fwpath_buffer,
	.maxlen = BUF_LEN,
};

char *country_code;
#ifdef FEATURE_WLAN_RESIDENT_DRIVER
EXPORT_SYMBOL(country_code);
#endif
static int enable_11d = -1;
static int enable_dfs_chan_scan = -1;
static bool is_mode_change_psoc_idle_shutdown;

#define WLAN_NLINK_CESIUM 30

static qdf_wake_lock_t wlan_wake_lock;

/* The valid PCIe gen speeds are 1, 2, 3 */
#define HDD_INVALID_MIN_PCIE_GEN_SPEED (0)
#define HDD_INVALID_MAX_PCIE_GEN_SPEED (4)

#define WOW_MAX_FILTER_LISTS 1
#define WOW_MAX_FILTERS_PER_LIST 4
#define WOW_MIN_PATTERN_SIZE 6
#define WOW_MAX_PATTERN_SIZE 64
#define MGMT_DEFAULT_DATA_RATE_6GHZ 0x400 /* This maps to 8.6Mbps data rate */

#define IS_IDLE_STOP (!cds_is_driver_unloading() && \
		      !cds_is_driver_recovering() && !cds_is_driver_loading())

#define HDD_FW_VER_MAJOR_SPID(tgt_fw_ver)     ((tgt_fw_ver & 0xf0000000) >> 28)
#define HDD_FW_VER_MINOR_SPID(tgt_fw_ver)     ((tgt_fw_ver & 0xf000000) >> 24)
#define HDD_FW_VER_SIID(tgt_fw_ver)           ((tgt_fw_ver & 0xf00000) >> 20)
#define HDD_FW_VER_CRM_ID(tgt_fw_ver)         (tgt_fw_ver & 0x7fff)
#define HDD_FW_VER_SUB_ID(tgt_fw_ver_ext) \
(((tgt_fw_ver_ext & 0x1c00) >> 6) | ((tgt_fw_ver_ext & 0xf0000000) >> 28))
#define HDD_FW_VER_REL_ID(tgt_fw_ver_ext) \
((tgt_fw_ver_ext &  0xf800000) >> 23)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
static const struct wiphy_wowlan_support wowlan_support_reg_init = {
	.flags = WIPHY_WOWLAN_ANY |
		 WIPHY_WOWLAN_MAGIC_PKT |
		 WIPHY_WOWLAN_DISCONNECT |
		 WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
		 WIPHY_WOWLAN_GTK_REKEY_FAILURE |
		 WIPHY_WOWLAN_EAP_IDENTITY_REQ |
		 WIPHY_WOWLAN_4WAY_HANDSHAKE |
		 WIPHY_WOWLAN_RFKILL_RELEASE,
	.n_patterns = WOW_MAX_FILTER_LISTS * WOW_MAX_FILTERS_PER_LIST,
	.pattern_min_len = WOW_MIN_PATTERN_SIZE,
	.pattern_max_len = WOW_MAX_PATTERN_SIZE,
};
#endif

static const struct category_info cinfo[MAX_SUPPORTED_CATEGORY] = {
	[QDF_MODULE_ID_TLSHIM] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_WMI] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_HTT] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_HDD] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SME] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_PE] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_WMA] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SYS] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_QDF] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SAP] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_HDD_SOFTAP] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_HDD_DATA] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_HDD_SAP_DATA] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_HIF] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_HTC] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_TXRX] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_HAL] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_QDF_DEVICE] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CFG] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_BMI] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_EPPING] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_QVIT] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_DP] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_TX_CAPTURE] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_INIT] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_STATS] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_HTT] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_PEER] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_HTT_TX_STATS] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_REO] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_VDEV] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_CDP] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_UMAC_RESET] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_DP_SAWF] = {QDF_DATA_PATH_TRACE_LEVEL},
	[QDF_MODULE_ID_SOC] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_OS_IF] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_TARGET_IF] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SCHEDULER] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_MGMT_TXRX] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_PMO] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SCAN] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_POLICY_MGR] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_P2P] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_TDLS] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_REGULATORY] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SERIALIZATION] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_DFS] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_OBJ_MGR] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_ROAM_DEBUG] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_GREEN_AP] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_OCB] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_IPA] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_ACTION_OUI] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CONFIG] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_MLME] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_TARGET] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CRYPTO] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_FWOL] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SM_ENGINE] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CMN_MLME] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_NAN] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CP_STATS] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_DCS] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_INTEROP_ISSUES_AP] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_DENYLIST_MGR] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_DIRECT_BUF_RX] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SPECTRAL] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_WIFIPOS] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_PKT_CAPTURE] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_FTM_TIME_SYNC] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CFR] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_IFMGR] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_GPIO] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_T2LM] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_MLO] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SON] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_TWT] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_WLAN_PRE_CAC] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_COAP] = {QDF_TRACE_LEVEL_ALL},
};

struct notifier_block hdd_netdev_notifier;

struct sock *cesium_nl_srv_sock;
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
static void wlan_hdd_auto_shutdown_cb(void);
#endif

static void hdd_dp_register_callbacks(struct hdd_context *hdd_ctx);

bool hdd_adapter_is_ap(struct hdd_adapter *adapter)
{
	if (!adapter) {
		hdd_err("null adapter");
		return false;
	}

	return adapter->device_mode == QDF_SAP_MODE ||
		adapter->device_mode == QDF_P2P_GO_MODE;
}

QDF_STATUS hdd_common_roam_callback(struct wlan_objmgr_psoc *psoc,
				    uint8_t session_id,
				    struct csr_roam_info *roam_info,
				    eRoamCmdStatus roam_status,
				    eCsrRoamResult roam_result)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	adapter = wlan_hdd_get_adapter_from_vdev(psoc, session_id);
	if (!adapter)
		return QDF_STATUS_E_INVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx)
		return QDF_STATUS_E_INVAL;

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_NDI_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
		status = hdd_sme_roam_callback(adapter, roam_info,
					       roam_status, roam_result);
		break;
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
		status = wlansap_roam_callback(adapter->session.ap.sap_context,
					       roam_info, roam_status,
					       roam_result);
		break;
	default:
		hdd_err("Wrong device mode");
		break;
	}

	return status;
}

void hdd_start_complete(int ret)
{
	wlan_start_ret_val = ret;
	complete_all(&wlan_start_comp);
}

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
void wlan_hdd_mod_fc_timer(struct hdd_adapter *adapter,
			   enum netif_action_type action)
{
	if (!adapter->tx_flow_timer_initialized)
		return;

	if (action == WLAN_WAKE_NON_PRIORITY_QUEUE) {
		qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		adapter->hdd_stats.tx_rx_stats.is_txflow_paused = false;
		adapter->hdd_stats.tx_rx_stats.txflow_unpause_cnt++;
	} else if (action == WLAN_STOP_NON_PRIORITY_QUEUE) {
		QDF_STATUS status =
		qdf_mc_timer_start(&adapter->tx_flow_control_timer,
				   WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("Failed to start tx_flow_control_timer");
		else
			adapter->
			hdd_stats.tx_rx_stats.txflow_timer_cnt++;

		adapter->hdd_stats.tx_rx_stats.txflow_pause_cnt++;
		adapter->hdd_stats.tx_rx_stats.is_txflow_paused = true;
	}
}
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

/**
 * wlan_hdd_txrx_pause_cb() - pause callback from txrx layer
 * @vdev_id: vdev_id
 * @action: action type
 * @reason: reason type
 *
 * Return: none
 */
void wlan_hdd_txrx_pause_cb(uint8_t vdev_id,
		enum netif_action_type action, enum netif_reason_type reason)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct hdd_adapter *adapter;

	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	wlan_hdd_mod_fc_timer(adapter, action);
	wlan_hdd_netif_queue_control(adapter, action, reason);
}

/*
 * Store WLAN driver version and timestamp info in global variables such that
 * crash debugger can extract them from driver debug symbol and crashdump for
 * post processing
 */
#ifdef BUILD_TAG
uint8_t g_wlan_driver_version[] = QWLAN_VERSIONSTR TIMER_MANAGER_STR MEMORY_DEBUG_STR PANIC_ON_BUG_STR "; " BUILD_TAG;
#else
uint8_t g_wlan_driver_version[] = QWLAN_VERSIONSTR TIMER_MANAGER_STR MEMORY_DEBUG_STR PANIC_ON_BUG_STR;
#endif

int hdd_validate_channel_and_bandwidth(struct hdd_adapter *adapter,
				       qdf_freq_t chan_freq,
				       enum phy_ch_width chan_bw)
{
	struct ch_params ch_params = {0};
	struct hdd_context *hdd_ctx;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("hdd context is NULL");
		return -EINVAL;
	}

	if (reg_is_chan_enum_invalid(
			wlan_reg_get_chan_enum_for_freq(chan_freq))) {
		hdd_err("Channel freq %d not in driver's valid channel list", chan_freq);
		return -EOPNOTSUPP;
	}

	if ((!WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq)) &&
	    (!WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq)) &&
	    (!WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq))) {
		hdd_err("CH %d is not in 2.4GHz or 5GHz or 6GHz", chan_freq);
		return -EINVAL;
	}
	ch_params.ch_width = CH_WIDTH_MAX;
	wlan_reg_set_channel_params_for_pwrmode(hdd_ctx->pdev, chan_freq,
						0, &ch_params,
						REG_CURRENT_PWR_MODE);
	if (ch_params.ch_width == CH_WIDTH_MAX) {
		hdd_err("failed to get max bandwdith for %d", chan_freq);
		return -EINVAL;
	}
	if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq)) {
		if (chan_bw == CH_WIDTH_80MHZ) {
			hdd_err("BW80 not possible in 2.4GHz band");
			return -EINVAL;
		}
		if ((chan_bw != CH_WIDTH_20MHZ) &&
		    (chan_freq == wlan_reg_ch_to_freq(CHAN_ENUM_2484)) &&
		    (chan_bw != CH_WIDTH_MAX) &&
		    (ch_params.ch_width == CH_WIDTH_20MHZ)) {
			hdd_err("Only BW20 possible on channel freq 2484");
			return -EINVAL;
		}
	}

	if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq)) {
		if ((chan_bw != CH_WIDTH_20MHZ) &&
		    (chan_freq == wlan_reg_ch_to_freq(CHAN_ENUM_5825)) &&
		    (chan_bw != CH_WIDTH_MAX) &&
		    (ch_params.ch_width == CH_WIDTH_20MHZ)) {
			hdd_err("Only BW20 possible on channel freq 5825");
			return -EINVAL;
		}
	}

	return 0;
}

uint32_t hdd_get_adapter_home_channel(struct hdd_adapter *adapter)
{
	uint32_t home_chan_freq = 0;
	struct hdd_context *hdd_ctx;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("hdd context is NULL");
		return 0;
	}

	if ((adapter->device_mode == QDF_SAP_MODE ||
	     adapter->device_mode == QDF_P2P_GO_MODE) &&
	    test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		home_chan_freq = adapter->session.ap.operating_chan_freq;
	} else if ((adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE) &&
		   hdd_cm_is_vdev_associated(adapter)) {
		home_chan_freq = adapter->session.station.conn_info.chan_freq;
	}

	return home_chan_freq;
}

enum phy_ch_width hdd_get_adapter_width(struct hdd_adapter *adapter)
{
	enum phy_ch_width width = CH_WIDTH_20MHZ;
	struct hdd_context *hdd_ctx;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("hdd context is NULL");
		return 0;
	}

	if ((adapter->device_mode == QDF_SAP_MODE ||
	     adapter->device_mode == QDF_P2P_GO_MODE) &&
	    test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		width = adapter->session.ap.sap_config.ch_params.ch_width;
	} else if ((adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE) &&
		   hdd_cm_is_vdev_associated(adapter)) {
		width = adapter->session.station.conn_info.ch_width;
	}
	return width;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
static inline struct net_device *hdd_net_dev_from_notifier(void *context)
{
	struct netdev_notifier_info *info = context;

	return info->dev;
}
#else
static inline struct net_device *hdd_net_dev_from_notifier(void *context)
{
	return context;
}
#endif

static int __hdd_netdev_notifier_call(struct net_device *net_dev,
				      unsigned long state)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter_dev(net_dev);

	if (!net_dev->ieee80211_ptr) {
		hdd_debug("ieee80211_ptr is null");
		return NOTIFY_DONE;
	}

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return NOTIFY_DONE;

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_debug("Driver module is closed");
		return NOTIFY_DONE;
	}

	/* Make sure that this callback corresponds to our device. */
	adapter = hdd_get_adapter_by_iface_name(hdd_ctx, net_dev->name);
	if (!adapter) {
		hdd_debug("failed to look up adapter for '%s'", net_dev->name);
		return NOTIFY_DONE;
	}

	if (adapter != WLAN_HDD_GET_PRIV_PTR(net_dev)) {
		hdd_err("HDD adapter mismatch!");
		return NOTIFY_DONE;
	}

	if (cds_is_driver_recovering()) {
		hdd_debug("Driver is recovering");
		return NOTIFY_DONE;
	}

	if (cds_is_driver_in_bad_state()) {
		hdd_debug("Driver is in failed recovery state");
		return NOTIFY_DONE;
	}

	hdd_debug("%s New Net Device State = %lu, flags 0x%x",
		  net_dev->name, state, net_dev->flags);

	switch (state) {
	case NETDEV_REGISTER:
		break;

	case NETDEV_UNREGISTER:
		break;

	case NETDEV_UP:
		sme_ch_avoid_update_req(hdd_ctx->mac_handle);
		break;

	case NETDEV_DOWN:
		break;

	case NETDEV_CHANGE:
		if (adapter->is_link_up_service_needed)
			complete(&adapter->linkup_event_var);
		break;

	case NETDEV_GOING_DOWN:
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_SCAN_ID);
		if (!vdev)
			break;
		if (ucfg_scan_get_vdev_status(vdev) !=
				SCAN_NOT_IN_PROGRESS) {
			wlan_abort_scan(hdd_ctx->pdev, INVAL_PDEV_ID,
					adapter->vdev_id, INVALID_SCAN_ID,
					true);
		}
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_SCAN_ID);
		cds_flush_work(&adapter->scan_block_work);
		/* Need to clean up blocked scan request */
		wlan_hdd_cfg80211_scan_block(adapter);
		hdd_debug("Scan is not Pending from user");
		/*
		 * After NETDEV_GOING_DOWN, kernel calls hdd_stop.Irrespective
		 * of return status of hdd_stop call, kernel resets the IFF_UP
		 * flag after which driver does not send the cfg80211_scan_done.
		 * Ensure to cleanup the scan queue in NETDEV_GOING_DOWN
		 */
		wlan_cfg80211_cleanup_scan_queue(hdd_ctx->pdev, net_dev);
		break;
	case NETDEV_FEAT_CHANGE:
		hdd_debug("vdev %d netdev Feature 0x%llx\n",
			  adapter->vdev_id, net_dev->features);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int hdd_netdev_notifier_bridge_intf(struct net_device *net_dev,
					   unsigned long state)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER;
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;

	hdd_enter_dev(net_dev);

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (wlan_hdd_validate_context(hdd_ctx))
		return NOTIFY_DONE;

	hdd_debug("%s New Net Device State = %lu, flags 0x%x bridge mac address: "QDF_MAC_ADDR_FMT,
		  net_dev->name, state, net_dev->flags, QDF_MAC_ADDR_REF(net_dev->dev_addr));

	if (!qdf_mem_cmp(hdd_ctx->bridgeaddr, net_dev->dev_addr,
			 QDF_MAC_ADDR_SIZE))
		return NOTIFY_DONE;

	switch (state) {
	case NETDEV_REGISTER:
	case NETDEV_CHANGEADDR:
		/* Update FW WoW pattern with new MAC address */
		qdf_mem_copy(hdd_ctx->bridgeaddr, net_dev->dev_addr,
			     QDF_MAC_ADDR_SIZE);

		hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
						   dbgid) {
			if (adapter->device_mode != QDF_SAP_MODE)
				goto loop_next;

			if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
				goto loop_next;

			status = wlan_objmgr_vdev_try_get_ref(adapter->vdev,
							      WLAN_HDD_ID_OBJ_MGR);

			if (QDF_IS_STATUS_ERROR(status))
				goto loop_next;

			ucfg_pmo_set_vdev_bridge_addr(adapter->vdev,
				(struct qdf_mac_addr *)hdd_ctx->bridgeaddr);
			ucfg_pmo_del_wow_pattern(adapter->vdev);
			ucfg_pmo_register_wow_default_patterns(adapter->vdev);

			wlan_objmgr_vdev_release_ref(adapter->vdev,
						     WLAN_HDD_ID_OBJ_MGR);
loop_next:
			hdd_adapter_dev_put_debug(adapter, dbgid);
		}
		break;

	case NETDEV_UNREGISTER:
		qdf_zero_macaddr((struct qdf_mac_addr *)hdd_ctx->bridgeaddr);
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

/**
 * hdd_netdev_notifier_call() - netdev notifier callback function
 * @nb: pointer to notifier block
 * @state: state
 * @context: notifier callback context pointer
 *
 * Return: 0 on success, error number otherwise.
 */
static int hdd_netdev_notifier_call(struct notifier_block *nb,
					unsigned long state,
					void *context)
{
	struct net_device *net_dev = hdd_net_dev_from_notifier(context);
	struct osif_vdev_sync *vdev_sync;
	int errno;

	if (net_dev->priv_flags & IFF_EBRIDGE) {
		errno = hdd_netdev_notifier_bridge_intf(net_dev, state);
		if (errno)
			return NOTIFY_DONE;
	}

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return NOTIFY_DONE;

	errno = __hdd_netdev_notifier_call(net_dev, state);

	osif_vdev_sync_op_stop(vdev_sync);

	return NOTIFY_DONE;
}

struct notifier_block hdd_netdev_notifier = {
	.notifier_call = hdd_netdev_notifier_call,
};

/* variable to hold the insmod parameters */
int con_mode;
#ifdef FEATURE_WLAN_RESIDENT_DRIVER
EXPORT_SYMBOL(con_mode);
#endif

int con_mode_ftm;
#ifdef FEATURE_WLAN_RESIDENT_DRIVER
EXPORT_SYMBOL(con_mode_ftm);
#endif
int con_mode_epping;

static int pcie_gen_speed;

/* Variable to hold connection mode including module parameter con_mode */
static int curr_con_mode;

#if defined(WLAN_FEATURE_11BE) && defined(CFG80211_11BE_BASIC)
static enum phy_ch_width hdd_get_eht_phy_ch_width_from_target(void)
{
	uint32_t max_fw_bw = sme_get_eht_ch_width();

	if (max_fw_bw == WNI_CFG_EHT_CHANNEL_WIDTH_320MHZ)
		return CH_WIDTH_320MHZ;
	else if (max_fw_bw == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		return CH_WIDTH_160MHZ;
	else
		return CH_WIDTH_80MHZ;
}

static bool hdd_is_target_eht_phy_ch_width_supported(enum phy_ch_width width)
{
	enum phy_ch_width max_fw_bw = hdd_get_eht_phy_ch_width_from_target();

	if (width <= max_fw_bw)
		return true;

	hdd_err("FW does not support this BW %d max BW supported %d",
		width, max_fw_bw);
	return false;
}

static bool hdd_is_target_eht_160mhz_capable(void)
{
	return hdd_is_target_eht_phy_ch_width_supported(CH_WIDTH_160MHZ);
}

static enum phy_ch_width
wlan_hdd_map_nl_chan_width(enum nl80211_chan_width width)
{
	if (width == NL80211_CHAN_WIDTH_320) {
		return hdd_get_eht_phy_ch_width_from_target();
	} else {
		hdd_err("Invalid channel width %d, setting to default", width);
		return CH_WIDTH_INVALID;
	}
}

#else /* !WLAN_FEATURE_11BE */
static inline bool
hdd_is_target_eht_phy_ch_width_supported(enum phy_ch_width width)
{
	return true;
}

static inline bool hdd_is_target_eht_160mhz_capable(void)
{
	return false;
}

static enum phy_ch_width
wlan_hdd_map_nl_chan_width(enum nl80211_chan_width width)
{
	hdd_err("Invalid channel width %d, setting to default", width);
	return CH_WIDTH_INVALID;
}
#endif /* WLAN_FEATURE_11BE */

/**
 * hdd_map_nl_chan_width() - Map NL channel width to internal representation
 * @ch_width: NL channel width
 *
 * Converts the NL channel width to the driver's internal representation
 *
 * Return: Converted channel width. In case of non matching NL channel width,
 * CH_WIDTH_MAX will be returned.
 */
enum phy_ch_width hdd_map_nl_chan_width(enum nl80211_chan_width ch_width)
{
	uint8_t fw_ch_bw;

	fw_ch_bw = wma_get_vht_ch_width();
	switch (ch_width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		return CH_WIDTH_20MHZ;
	case NL80211_CHAN_WIDTH_40:
		return CH_WIDTH_40MHZ;
	case NL80211_CHAN_WIDTH_80:
		return CH_WIDTH_80MHZ;
	case NL80211_CHAN_WIDTH_80P80:
		if (fw_ch_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
			return CH_WIDTH_80P80MHZ;
		else if (fw_ch_bw == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
			return CH_WIDTH_160MHZ;
		else
			return CH_WIDTH_80MHZ;
	case NL80211_CHAN_WIDTH_160:
		if (hdd_is_target_eht_160mhz_capable())
			return CH_WIDTH_160MHZ;

		if (fw_ch_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
			return CH_WIDTH_160MHZ;
		else
			return CH_WIDTH_80MHZ;
	case NL80211_CHAN_WIDTH_5:
		return CH_WIDTH_5MHZ;
	case NL80211_CHAN_WIDTH_10:
		return CH_WIDTH_10MHZ;
	default:
		return wlan_hdd_map_nl_chan_width(ch_width);
	}
}

#if defined(WLAN_FEATURE_NAN) && \
	   (KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE)
/**
 * wlan_hdd_convert_nan_type() - Convert nl type to qdf type
 * @nl_type: NL80211 interface type
 * @out_qdf_type: QDF type for the given nl_type
 *
 * Convert nl type to QDF type
 *
 * Return: QDF_STATUS_SUCCESS if converted, failure otherwise.
 */
static QDF_STATUS wlan_hdd_convert_nan_type(enum nl80211_iftype nl_type,
					    enum QDF_OPMODE *out_qdf_type)
{
	if (nl_type == NL80211_IFTYPE_NAN) {
		*out_qdf_type = QDF_NAN_DISC_MODE;
		return QDF_STATUS_SUCCESS;
	}
	return QDF_STATUS_E_INVAL;
}

/**
 * wlan_hdd_set_nan_if_type() - Set the NAN iftype
 * @adapter: pointer to HDD adapter
 *
 * Set the NL80211_IFTYPE_NAN to wdev iftype.
 *
 * Return: None
 */
static void wlan_hdd_set_nan_if_type(struct hdd_adapter *adapter)
{
	adapter->wdev.iftype = NL80211_IFTYPE_NAN;
}

static bool wlan_hdd_is_vdev_creation_allowed(struct wlan_objmgr_psoc *psoc)
{
	return ucfg_nan_is_vdev_creation_allowed(psoc);
}
#else
static QDF_STATUS wlan_hdd_convert_nan_type(enum nl80211_iftype nl_type,
					    enum QDF_OPMODE *out_qdf_type)
{
	return QDF_STATUS_E_INVAL;
}

static void wlan_hdd_set_nan_if_type(struct hdd_adapter *adapter)
{
}

static bool wlan_hdd_is_vdev_creation_allowed(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

QDF_STATUS hdd_nl_to_qdf_iface_type(enum nl80211_iftype nl_type,
				    enum QDF_OPMODE *out_qdf_type)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	switch (nl_type) {
	case NL80211_IFTYPE_AP:
		*out_qdf_type = QDF_SAP_MODE;
		break;
	case NL80211_IFTYPE_MONITOR:
		*out_qdf_type = QDF_MONITOR_MODE;
		break;
	case NL80211_IFTYPE_OCB:
		*out_qdf_type = QDF_OCB_MODE;
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
		*out_qdf_type = QDF_P2P_CLIENT_MODE;
		break;
	case NL80211_IFTYPE_P2P_DEVICE:
		*out_qdf_type = QDF_P2P_DEVICE_MODE;
		break;
	case NL80211_IFTYPE_P2P_GO:
		*out_qdf_type = QDF_P2P_GO_MODE;
		break;
	case NL80211_IFTYPE_STATION:
		*out_qdf_type = QDF_STA_MODE;
		break;
	case NL80211_IFTYPE_WDS:
		*out_qdf_type = QDF_WDS_MODE;
		break;
	default:
		status = wlan_hdd_convert_nan_type(nl_type, out_qdf_type);
		if (QDF_IS_STATUS_SUCCESS(status))
			break;
		hdd_err("Invalid nl80211 interface type %d", nl_type);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

uint8_t wlan_hdd_find_opclass(mac_handle_t mac_handle, uint8_t channel,
			      uint8_t bw_offset)
{
	uint8_t opclass = 0;

	sme_get_opclass(mac_handle, channel, bw_offset, &opclass);
	return opclass;
}

/**
 * hdd_qdf_trace_enable() - configure initial QDF Trace enable
 * @module_id:	Module whose trace level is being configured
 * @bitmask:	Bitmask of log levels to be enabled
 *
 * Called immediately after the cfg.ini is read in order to configure
 * the desired trace levels.
 *
 * Return: None
 */
int hdd_qdf_trace_enable(QDF_MODULE_ID module_id, uint32_t bitmask)
{
	QDF_TRACE_LEVEL level;
	int qdf_print_idx = -1;
	int status = -1;
	/*
	 * if the bitmask is the default value, then a bitmask was not
	 * specified in cfg.ini, so leave the logging level alone (it
	 * will remain at the "compiled in" default value)
	 */
	if (CFG_QDF_TRACE_ENABLE_DEFAULT == bitmask)
		return 0;

	qdf_print_idx = qdf_get_pidx();

	/* a mask was specified.  start by disabling all logging */
	status = qdf_print_set_category_verbose(qdf_print_idx, module_id,
					QDF_TRACE_LEVEL_NONE, 0);

	if (QDF_STATUS_SUCCESS != status)
		return -EINVAL;
	/* now cycle through the bitmask until all "set" bits are serviced */
	level = QDF_TRACE_LEVEL_NONE;
	while (0 != bitmask) {
		if (bitmask & 1) {
			status = qdf_print_set_category_verbose(qdf_print_idx,
							module_id, level, 1);
			if (QDF_STATUS_SUCCESS != status)
				return -EINVAL;
		}

		level++;
		bitmask >>= 1;
	}
	return 0;
}

int __wlan_hdd_validate_context(struct hdd_context *hdd_ctx, const char *func)
{
	if (!hdd_ctx) {
		hdd_err("HDD context is null (via %s)", func);
		return -ENODEV;
	}

	if (!hdd_ctx->config) {
		hdd_err("HDD config is null (via %s)", func);
		return -ENODEV;
	}

	if (cds_is_driver_recovering()) {
		hdd_debug("Recovery in progress (via %s); state:0x%x",
			  func, cds_get_driver_state());
		return -EAGAIN;
	}

	if (cds_is_load_or_unload_in_progress()) {
		hdd_debug("Load/unload in progress (via %s); state:0x%x",
			  func, cds_get_driver_state());
		return -EAGAIN;
	}

	if (cds_is_driver_in_bad_state()) {
		hdd_debug("Driver in bad state (via %s); state:0x%x",
			  func, cds_get_driver_state());
		return -EAGAIN;
	}

	if (cds_is_fw_down()) {
		hdd_debug("FW is down (via %s); state:0x%x",
			  func, cds_get_driver_state());
		return -EAGAIN;
	}

	if (hdd_ctx->is_wlan_disabled) {
		hdd_debug("WLAN is disabled by user space");
		return -EAGAIN;
	}

	return 0;
}

int __hdd_validate_adapter(struct hdd_adapter *adapter, const char *func)
{
	if (!adapter) {
		hdd_err("adapter is null (via %s)", func);
		return -EINVAL;
	}

	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err("bad adapter magic (via %s)", func);
		return -EINVAL;
	}

	if (!adapter->dev) {
		hdd_err("adapter net_device is null (via %s)", func);
		return -EINVAL;
	}

	if (!(adapter->dev->flags & IFF_UP)) {
		hdd_debug_rl("adapter '%s' is not up (via %s)",
			     adapter->dev->name, func);
		return -EAGAIN;
	}

	return __wlan_hdd_validate_vdev_id(adapter->vdev_id, func);
}

int __wlan_hdd_validate_vdev_id(uint8_t vdev_id, const char *func)
{
	if (vdev_id == WLAN_UMAC_VDEV_ID_MAX) {
		hdd_debug_rl("adapter is not up (via %s)", func);
		return -EINVAL;
	}

	if (vdev_id >= WLAN_MAX_VDEVS) {
		hdd_err("bad vdev Id:%u (via %s)", vdev_id, func);
		return -EINVAL;
	}

	return 0;
}

QDF_STATUS __wlan_hdd_validate_mac_address(struct qdf_mac_addr *mac_addr,
					   const char *func)
{
	if (!mac_addr) {
		hdd_err("Received NULL mac address (via %s)", func);
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_is_macaddr_zero(mac_addr)) {
		hdd_err("MAC is all zero (via %s)", func);
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_is_macaddr_broadcast(mac_addr)) {
		hdd_err("MAC is Broadcast (via %s)", func);
		return QDF_STATUS_E_INVAL;
	}

	if (QDF_NET_IS_MAC_MULTICAST(mac_addr->bytes)) {
		hdd_err("MAC is Multicast (via %s)", func);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_hdd_validate_modules_state() - Check modules status
 * @hdd_ctx: HDD context pointer
 *
 * Check's the driver module's state and returns true if the
 * modules are enabled returns false if modules are closed.
 *
 * Return: True if modules are enabled or false.
 */
bool wlan_hdd_validate_modules_state(struct hdd_context *hdd_ctx)
{
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_info("Modules not enabled, Present status: %d",
			 hdd_ctx->driver_status);
		return false;
	}

	return true;
}

#ifdef FEATURE_RUNTIME_PM
/**
 * hdd_runtime_suspend_context_init() - API to initialize HDD Runtime Contexts
 * @hdd_ctx: HDD context
 *
 * Return: None
 */
static void hdd_runtime_suspend_context_init(struct hdd_context *hdd_ctx)
{
	struct hdd_runtime_pm_context *ctx = &hdd_ctx->runtime_context;

	qdf_runtime_lock_init(&ctx->dfs);
	qdf_runtime_lock_init(&ctx->connect);
	qdf_runtime_lock_init(&ctx->user);
	qdf_runtime_lock_init(&ctx->monitor_mode);
	qdf_runtime_lock_init(&ctx->wow_unit_test);
	qdf_runtime_lock_init(&ctx->system_suspend);
	qdf_runtime_lock_init(&ctx->dyn_mac_addr_update);
	qdf_runtime_lock_init(&ctx->vdev_destroy);

	qdf_rtpm_register(QDF_RTPM_ID_WIPHY_SUSPEND, NULL);
	qdf_rtpm_register(QDF_RTPM_ID_PM_QOS_NOTIFY, NULL);

	ctx->is_user_wakelock_acquired = false;

	wlan_scan_runtime_pm_init(hdd_ctx->pdev);
}

/**
 * hdd_runtime_suspend_context_deinit() - API to deinit HDD runtime context
 * @hdd_ctx: HDD Context
 *
 * Return: None
 */
static void hdd_runtime_suspend_context_deinit(struct hdd_context *hdd_ctx)
{
	struct hdd_runtime_pm_context *ctx = &hdd_ctx->runtime_context;

	if (ctx->is_user_wakelock_acquired)
		qdf_runtime_pm_allow_suspend(&ctx->user);

	qdf_runtime_lock_deinit(&ctx->dyn_mac_addr_update);
	qdf_runtime_lock_deinit(&ctx->wow_unit_test);
	qdf_runtime_lock_deinit(&ctx->monitor_mode);
	qdf_runtime_lock_deinit(&ctx->user);
	qdf_runtime_lock_deinit(&ctx->connect);
	qdf_runtime_lock_deinit(&ctx->dfs);
	qdf_runtime_lock_deinit(&ctx->system_suspend);
	qdf_runtime_lock_deinit(&ctx->vdev_destroy);

	qdf_rtpm_deregister(QDF_RTPM_ID_WIPHY_SUSPEND);
	qdf_rtpm_deregister(QDF_RTPM_ID_PM_QOS_NOTIFY);

	wlan_scan_runtime_pm_deinit(hdd_ctx->pdev);
}

#else /* FEATURE_RUNTIME_PM */
static void hdd_runtime_suspend_context_init(struct hdd_context *hdd_ctx) {}
static void hdd_runtime_suspend_context_deinit(struct hdd_context *hdd_ctx) {}
#endif /* FEATURE_RUNTIME_PM */

void hdd_update_macaddr(struct hdd_context *hdd_ctx,
			struct qdf_mac_addr hw_macaddr, bool generate_mac_auto)
{
	int8_t i;
	uint8_t macaddr_b3, tmp_br3;

	/*
	 * If "generate_mac_auto" is true, it indicates that all the
	 * addresses are derived addresses, else the first addresses
	 * is not derived address (It is provided by fw).
	 */
	if (!generate_mac_auto) {
		qdf_mem_copy(hdd_ctx->provisioned_mac_addr[0].bytes,
			     hw_macaddr.bytes, QDF_MAC_ADDR_SIZE);
		hdd_ctx->num_provisioned_addr++;
		hdd_debug("hdd_ctx->provisioned_mac_addr[0]: "
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(hdd_ctx->
					provisioned_mac_addr[0].bytes));
	} else {
		qdf_mem_copy(hdd_ctx->derived_mac_addr[0].bytes,
			     hw_macaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		hdd_ctx->num_derived_addr++;
		hdd_debug("hdd_ctx->derived_mac_addr[0]: "
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(hdd_ctx->derived_mac_addr[0].bytes));
	}
	for (i = hdd_ctx->num_derived_addr; i < (QDF_MAX_CONCURRENCY_PERSONA -
						hdd_ctx->num_provisioned_addr);
			i++) {
		qdf_mem_copy(hdd_ctx->derived_mac_addr[i].bytes,
			     hw_macaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		macaddr_b3 = hdd_ctx->derived_mac_addr[i].bytes[3];
		tmp_br3 = ((macaddr_b3 >> 4 & INTF_MACADDR_MASK) + i) &
			  INTF_MACADDR_MASK;
		macaddr_b3 += tmp_br3;

		/* XOR-ing bit-24 of the mac address. This will give enough
		 * mac address range before collision
		 */
		macaddr_b3 ^= (1 << 7);

		/* Set locally administered bit */
		hdd_ctx->derived_mac_addr[i].bytes[0] |= 0x02;
		hdd_ctx->derived_mac_addr[i].bytes[3] = macaddr_b3;
		hdd_debug("hdd_ctx->derived_mac_addr[%d]: "
			QDF_MAC_ADDR_FMT, i,
			QDF_MAC_ADDR_REF(hdd_ctx->derived_mac_addr[i].bytes));
		hdd_ctx->num_derived_addr++;
	}
}

#ifdef FEATURE_WLAN_TDLS
static int hdd_update_tdls_config(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	struct tdls_start_params tdls_cfg;
	QDF_STATUS status;
	struct wlan_mlme_nss_chains vdev_ini_cfg;

	/* Populate the nss chain params from ini for this vdev type */
	sme_populate_nss_chain_params(hdd_ctx->mac_handle, &vdev_ini_cfg,
				      QDF_TDLS_MODE,
				      hdd_ctx->num_rf_chains);

	cfg_tdls_set_vdev_nss_2g(hdd_ctx->psoc,
				 vdev_ini_cfg.rx_nss[NSS_CHAINS_BAND_2GHZ]);
	cfg_tdls_set_vdev_nss_5g(hdd_ctx->psoc,
				 vdev_ini_cfg.rx_nss[NSS_CHAINS_BAND_5GHZ]);
	hdd_init_tdls_config(&tdls_cfg);
	tdls_cfg.tdls_del_all_peers = eWNI_SME_DEL_ALL_TDLS_PEERS;
	tdls_cfg.tdls_update_dp_vdev_flags = CDP_UPDATE_TDLS_FLAGS;
	tdls_cfg.tdls_event_cb = wlan_cfg80211_tdls_event_callback;
	tdls_cfg.tdls_evt_cb_data = psoc;
	tdls_cfg.tdls_peer_context = hdd_ctx;
	tdls_cfg.tdls_reg_peer = hdd_tdls_register_peer;
	tdls_cfg.tdls_wmm_cb = hdd_wmm_is_acm_allowed;
	tdls_cfg.tdls_wmm_cb_data = psoc;
	tdls_cfg.tdls_rx_cb = wlan_cfg80211_tdls_rx_callback;
	tdls_cfg.tdls_rx_cb_data = psoc;
	tdls_cfg.tdls_dp_vdev_update = hdd_update_dp_vdev_flags;
	tdls_cfg.tdls_osif_init_cb = wlan_cfg80211_tdls_osif_priv_init;
	tdls_cfg.tdls_osif_deinit_cb = wlan_cfg80211_tdls_osif_priv_deinit;
	tdls_cfg.tdls_osif_update_cb.tdls_osif_conn_update =
					hdd_check_and_set_tdls_conn_params;
	tdls_cfg.tdls_osif_update_cb.tdls_osif_disconn_update =
					hdd_check_and_set_tdls_disconn_params;

	status = ucfg_tdls_update_config(psoc, &tdls_cfg);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("failed pmo psoc configuration");
		return -EINVAL;
	}

	hdd_ctx->tdls_umac_comp_active = true;
	/* enable napier specific tdls data path */
	hdd_ctx->tdls_nap_active = true;

	return 0;
}
#else
static int hdd_update_tdls_config(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

void hdd_indicate_active_ndp_cnt(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, uint8_t cnt)
{
	struct hdd_adapter *adapter = NULL;

	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (adapter && cfg_nan_is_roam_config_disabled(psoc)) {
		hdd_debug("vdev_id:%d%s active ndp sessions present", vdev_id,
			  cnt ? "" : " no more");
		if (!cnt)
			wlan_hdd_enable_roaming(adapter, RSO_NDP_CON_ON_NDI);
		else
			wlan_hdd_disable_roaming(adapter, RSO_NDP_CON_ON_NDI);
	}
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void hdd_update_roam_offload(struct hdd_context *hdd_ctx,
				    struct wma_tgt_services *cfg)
{
	bool roam_offload_enable;

	ucfg_mlme_get_roaming_offload(hdd_ctx->psoc, &roam_offload_enable);
	ucfg_mlme_set_roaming_offload(hdd_ctx->psoc,
				      roam_offload_enable &
				      cfg->en_roam_offload);
}
#else
static inline void hdd_update_roam_offload(struct hdd_context *hdd_ctx,
					   struct wma_tgt_services *cfg)
{
}
#endif

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
static void
hdd_club_ll_stats_in_get_sta_cfg_update(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
	config->sta_stats_cache_expiry_time =
			cfg_get(psoc, CFG_STA_STATS_CACHE_EXPIRY);
}

static void
hdd_update_feature_cfg_club_get_sta_in_ll_stats_req(
					struct hdd_context *hdd_ctx,
					struct wma_tgt_services *cfg)
{
	hdd_ctx->is_get_station_clubbed_in_ll_stats_req =
				cfg->is_get_station_clubbed_in_ll_stats_req &&
				cfg_get(hdd_ctx->psoc,
					CFG_CLUB_LL_STA_AND_GET_STATION);
}

static void
hdd_init_get_sta_in_ll_stats_config(struct hdd_adapter *adapter)
{
	adapter->hdd_stats.sta_stats_cached_timestamp = 0;
}
#else
static void
hdd_club_ll_stats_in_get_sta_cfg_update(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
}

static void
hdd_update_feature_cfg_club_get_sta_in_ll_stats_req(
					struct hdd_context *hdd_ctx,
					struct wma_tgt_services *cfg)
{
}

static void
hdd_init_get_sta_in_ll_stats_config(struct hdd_adapter *adapter)
{
}
#endif /* FEATURE_CLUB_LL_STATS_AND_GET_STATION */

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
static void
hdd_intersect_igmp_offload_setting(struct wlan_objmgr_psoc *psoc,
				   struct wma_tgt_services *cfg)
{
	bool igmp_offload_enable;

	igmp_offload_enable =
		ucfg_pmo_is_igmp_offload_enabled(psoc);
	ucfg_pmo_set_igmp_offload_enabled(psoc,
					  igmp_offload_enable &
					  cfg->igmp_offload_enable);
	hdd_info("fw cap to handle igmp %d igmp_offload_enable ini %d",
		 cfg->igmp_offload_enable, igmp_offload_enable);
}
#else
static inline void
hdd_intersect_igmp_offload_setting(struct wlan_objmgr_psoc *psoc,
				   struct wma_tgt_services *cfg)
{}
#endif

#ifdef FEATURE_WLAN_TDLS
static void hdd_update_fw_tdls_wideband_capability(struct hdd_context *hdd_ctx,
						   struct wma_tgt_services *cfg)
{
	ucfg_tdls_update_fw_wideband_capability(hdd_ctx->psoc,
						cfg->en_tdls_wideband_support);
}

#ifdef WLAN_FEATURE_11AX
static void hdd_update_fw_tdls_11ax_capability(struct hdd_context *hdd_ctx,
					       struct wma_tgt_services *cfg)
{
	ucfg_tdls_update_fw_11ax_capability(hdd_ctx->psoc,
					    cfg->en_tdls_11ax_support);
}

static void hdd_update_fw_tdls_6g_capability(struct hdd_context *hdd_ctx,
					     struct wma_tgt_services *cfg)
{
	ucfg_update_fw_tdls_6g_capability(hdd_ctx->psoc,
					  cfg->en_tdls_6g_support);
}

#else
static inline
void hdd_update_fw_tdls_11ax_capability(struct hdd_context *hdd_ctx,
					struct wma_tgt_services *cfg)
{}
static inline
void hdd_update_fw_tdls_6g_capability(struct hdd_context *hdd_ctx,
				      struct wma_tgt_services *cfg)
{}
#endif
#else
static inline
void hdd_update_fw_tdls_11ax_capability(struct hdd_context *hdd_ctx,
					struct wma_tgt_services *cfg)
{}

static inline
void hdd_update_fw_tdls_6g_capability(struct hdd_context *hdd_ctx,
				      struct wma_tgt_services *cfg)
{}

static inline
void hdd_update_fw_tdls_wideband_capability(struct hdd_context *hdd_ctx,
					    struct wma_tgt_services *cfg)
{}
#endif

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
static inline void
hdd_set_dynamic_macaddr_update_capability(struct hdd_context *hdd_ctx,
					  struct wma_tgt_services *cfg)
{
	hdd_ctx->is_vdev_macaddr_dynamic_update_supported =
				cfg->dynamic_vdev_macaddr_support &&
				cfg_get(hdd_ctx->psoc,
					CFG_DYNAMIC_MAC_ADDR_UPDATE_SUPPORTED);
}
#else
static inline void
hdd_set_dynamic_macaddr_update_capability(struct hdd_context *hdd_ctx,
					  struct wma_tgt_services *cfg)
{
}
#endif

#ifdef WLAN_FEATURE_11BE
static bool hdd_dot11Mode_support_11be(enum hdd_dot11_mode dot11Mode)
{
	if (dot11Mode != eHDD_DOT11_MODE_AUTO &&
	    dot11Mode < eHDD_DOT11_MODE_11be)
		return false;

	return true;
}
#else
static bool hdd_dot11Mode_support_11be(enum hdd_dot11_mode dot11Mode)
{
	return false;
}
#endif


static void hdd_update_tgt_services(struct hdd_context *hdd_ctx,
				    struct wma_tgt_services *cfg)
{
	struct hdd_config *config = hdd_ctx->config;
	bool arp_offload_enable;
	bool mawc_enabled;
#ifdef FEATURE_WLAN_TDLS
	bool tdls_support;
	bool tdls_off_channel;
	bool tdls_buffer_sta;
	uint32_t tdls_uapsd_mask;
#endif
	bool get_peer_info_enable;

	/* Set up UAPSD */
	ucfg_mlme_set_sap_uapsd_flag(hdd_ctx->psoc, cfg->uapsd);

	/* 11AX mode support */
	if ((config->dot11Mode == eHDD_DOT11_MODE_11ax ||
	     config->dot11Mode == eHDD_DOT11_MODE_11ax_ONLY) && !cfg->en_11ax)
		config->dot11Mode = eHDD_DOT11_MODE_11ac;

	/* 11AC mode support */
	if ((config->dot11Mode == eHDD_DOT11_MODE_11ac ||
	     config->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY) && !cfg->en_11ac)
		config->dot11Mode = eHDD_DOT11_MODE_AUTO;
	/* 11BE mode support */
	if (!hdd_dot11Mode_support_11be(config->dot11Mode) &&
	    cfg->en_11be) {
		hdd_debug("dot11Mode %d override target en_11be to false",
			  config->dot11Mode);
		cfg->en_11be = false;
	}

	/* ARP offload: override user setting if invalid  */
	arp_offload_enable =
			ucfg_pmo_is_arp_offload_enabled(hdd_ctx->psoc);
	ucfg_pmo_set_arp_offload_enabled(hdd_ctx->psoc,
					 arp_offload_enable & cfg->arp_offload);

	/* Intersect igmp offload ini configuration and fw cap*/
	hdd_intersect_igmp_offload_setting(hdd_ctx->psoc, cfg);

#ifdef FEATURE_WLAN_SCAN_PNO
	/* PNO offload */
	hdd_debug("PNO Capability in f/w = %d", cfg->pno_offload);
	if (cfg->pno_offload)
		ucfg_scan_set_pno_offload(hdd_ctx->psoc, true);
#endif
#ifdef FEATURE_WLAN_TDLS
	cfg_tdls_get_support_enable(hdd_ctx->psoc, &tdls_support);
	cfg_tdls_set_support_enable(hdd_ctx->psoc,
				    tdls_support & cfg->en_tdls);

	cfg_tdls_get_off_channel_enable(hdd_ctx->psoc, &tdls_off_channel);
	cfg_tdls_set_off_channel_enable(hdd_ctx->psoc,
					tdls_off_channel &&
					cfg->en_tdls_offchan);

	cfg_tdls_get_buffer_sta_enable(hdd_ctx->psoc, &tdls_buffer_sta);
	cfg_tdls_set_buffer_sta_enable(hdd_ctx->psoc,
				       tdls_buffer_sta &&
				       cfg->en_tdls_uapsd_buf_sta);

	cfg_tdls_get_uapsd_mask(hdd_ctx->psoc, &tdls_uapsd_mask);
	if (tdls_uapsd_mask && cfg->en_tdls_uapsd_sleep_sta)
		cfg_tdls_set_sleep_sta_enable(hdd_ctx->psoc, true);
	else
		cfg_tdls_set_sleep_sta_enable(hdd_ctx->psoc, false);
#endif
	hdd_update_roam_offload(hdd_ctx, cfg);

	if (ucfg_mlme_get_sap_get_peer_info(
		hdd_ctx->psoc, &get_peer_info_enable) == QDF_STATUS_SUCCESS) {
		get_peer_info_enable &= cfg->get_peer_info_enabled;
		ucfg_mlme_set_sap_get_peer_info(hdd_ctx->psoc,
						get_peer_info_enable);
	}

	ucfg_mlme_is_mawc_enabled(hdd_ctx->psoc, &mawc_enabled);
	ucfg_mlme_set_mawc_enabled(hdd_ctx->psoc,
				   mawc_enabled & cfg->is_fw_mawc_capable);
	hdd_update_tdls_config(hdd_ctx);
	sme_update_tgt_services(hdd_ctx->mac_handle, cfg);
	hdd_ctx->roam_ch_from_fw_supported = cfg->is_roam_scan_ch_to_host;
	hdd_ctx->ll_stats_per_chan_rx_tx_time =
					cfg->ll_stats_per_chan_rx_tx_time;

	hdd_update_feature_cfg_club_get_sta_in_ll_stats_req(hdd_ctx, cfg);
	hdd_ctx->is_therm_cmd_supp =
				cfg->is_fw_therm_throt_supp &&
				cfg_get(hdd_ctx->psoc,
					CFG_THERMAL_MITIGATION_ENABLE);
	hdd_update_fw_tdls_11ax_capability(hdd_ctx, cfg);
	hdd_set_dynamic_macaddr_update_capability(hdd_ctx, cfg);
	hdd_update_fw_tdls_6g_capability(hdd_ctx, cfg);
	hdd_update_fw_tdls_wideband_capability(hdd_ctx, cfg);
	ucfg_psoc_mlme_set_11be_capab(hdd_ctx->psoc, cfg->en_11be);
}

/**
 * hdd_update_vdev_nss() - sets the vdev nss
 * @hdd_ctx: HDD context
 *
 * Sets the Nss per vdev type based on INI
 *
 * Return: None
 */
static void hdd_update_vdev_nss(struct hdd_context *hdd_ctx)
{
	uint8_t max_supp_nss = 1;
	mac_handle_t mac_handle;
	QDF_STATUS status;
	bool bval;

	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &bval);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get vht_enable2x2");

	if (bval && !cds_is_sub_20_mhz_enabled())
		max_supp_nss = 2;

	hdd_debug("max nss %d", max_supp_nss);

	mac_handle = hdd_ctx->mac_handle;
	sme_update_vdev_type_nss(mac_handle, max_supp_nss,
				 NSS_CHAINS_BAND_2GHZ);

	sme_update_vdev_type_nss(mac_handle, max_supp_nss,
				 NSS_CHAINS_BAND_5GHZ);
}

/**
 * hdd_update_2g_wiphy_vhtcap() - Updates 2G wiphy vhtcap fields
 * @hdd_ctx: HDD context
 *
 * Updates 2G wiphy vhtcap fields
 *
 * Return: None
 */
static void hdd_update_2g_wiphy_vhtcap(struct hdd_context *hdd_ctx)
{
	struct ieee80211_supported_band *band_2g =
		hdd_ctx->wiphy->bands[NL80211_BAND_2GHZ];
	uint32_t value;
	bool is_vht_24ghz;

	if (!band_2g) {
		hdd_debug("2GHz band disabled, skipping capability population");
		return;
	}

	ucfg_mlme_get_vht_for_24ghz(hdd_ctx->psoc, &is_vht_24ghz);

	if (is_vht_24ghz) {
		ucfg_mlme_cfg_get_vht_tx_mcs_map(hdd_ctx->psoc, &value);
		band_2g->vht_cap.vht_mcs.tx_mcs_map = value;
	}
}

/**
 * hdd_update_5g_wiphy_vhtcap() - Updates 5G wiphy vhtcap fields
 * @hdd_ctx: HDD context
 *
 * Updates 5G wiphy vhtcap fields
 *
 * Return: None
 */
static void hdd_update_5g_wiphy_vhtcap(struct hdd_context *hdd_ctx)
{
	struct ieee80211_supported_band *band_5g =
		hdd_ctx->wiphy->bands[NL80211_BAND_5GHZ];
	QDF_STATUS status;
	uint8_t value = 0, value1 = 0;
	uint32_t value2;

	if (!band_5g) {
		hdd_debug("5GHz band disabled, skipping capability population");
		return;
	}

	status = ucfg_mlme_cfg_get_vht_tx_bfee_ant_supp(hdd_ctx->psoc,
							&value);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get tx_bfee_ant_supp");

	band_5g->vht_cap.cap |=
			(value << IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT);

	value1 = NUM_OF_SOUNDING_DIMENSIONS;
	band_5g->vht_cap.cap |=
		(value1 << IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT);

	hdd_debug("Updated wiphy vhtcap:0x%x, CSNAntSupp:%d, NumSoundDim:%d",
		  band_5g->vht_cap.cap, value, value1);

	ucfg_mlme_cfg_get_vht_rx_mcs_map(hdd_ctx->psoc, &value2);
	band_5g->vht_cap.vht_mcs.rx_mcs_map = value2;

	ucfg_mlme_cfg_get_vht_tx_mcs_map(hdd_ctx->psoc, &value2);
	band_5g->vht_cap.vht_mcs.tx_mcs_map = value2;
}

/**
 * hdd_update_wiphy_vhtcap() - Updates wiphy vhtcap fields
 * @hdd_ctx: HDD context
 *
 * Updates wiphy vhtcap fields
 *
 * Return: None
 */
static void hdd_update_wiphy_vhtcap(struct hdd_context *hdd_ctx)
{
	hdd_update_2g_wiphy_vhtcap(hdd_ctx);
	hdd_update_5g_wiphy_vhtcap(hdd_ctx);
}

static void hdd_update_tgt_ht_cap(struct hdd_context *hdd_ctx,
				  struct wma_tgt_ht_cap *cfg)
{
	QDF_STATUS status;
	qdf_size_t value_len;
	uint32_t value;
	uint8_t mpdu_density;
	struct mlme_ht_capabilities_info ht_cap_info;
	uint8_t mcs_set[SIZE_OF_SUPPORTED_MCS_SET];
	bool b_enable1x1;

	/* get the MPDU density */
	status = ucfg_mlme_get_ht_mpdu_density(hdd_ctx->psoc, &mpdu_density);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get HT MPDU Density");
		return;
	}

	/*
	 * MPDU density:
	 * override user's setting if value is larger
	 * than the one supported by target,
	 * if target value is 0, then follow user's setting.
	 */
	if (cfg->mpdu_density && mpdu_density > cfg->mpdu_density) {
		status = ucfg_mlme_set_ht_mpdu_density(hdd_ctx->psoc,
						       cfg->mpdu_density);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("could not set HT capability to CCM");
	}

	/* get the HT capability info */
	status = ucfg_mlme_get_ht_cap_info(hdd_ctx->psoc, &ht_cap_info);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("could not get HT capability info");
		return;
	}

	/* check and update RX STBC */
	if (ht_cap_info.rx_stbc && !cfg->ht_rx_stbc)
		ht_cap_info.rx_stbc = cfg->ht_rx_stbc;

	/* Set the LDPC capability */
	if (ht_cap_info.adv_coding_cap && !cfg->ht_rx_ldpc)
		ht_cap_info.adv_coding_cap = cfg->ht_rx_ldpc;

	if (ht_cap_info.short_gi_20_mhz && !cfg->ht_sgi_20)
		ht_cap_info.short_gi_20_mhz = cfg->ht_sgi_20;

	if (ht_cap_info.short_gi_40_mhz && !cfg->ht_sgi_40)
		ht_cap_info.short_gi_40_mhz = cfg->ht_sgi_40;

	hdd_ctx->num_rf_chains = cfg->num_rf_chains;
	hdd_ctx->ht_tx_stbc_supported = cfg->ht_tx_stbc;

	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &b_enable1x1);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get vht_enable2x2");

	b_enable1x1 = b_enable1x1 && (cfg->num_rf_chains == 2);

	status = ucfg_mlme_set_vht_enable2x2(hdd_ctx->psoc, b_enable1x1);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to set vht_enable2x2");

	if (!b_enable1x1)
		ht_cap_info.tx_stbc = 0;

	if (!(cfg->ht_tx_stbc && b_enable1x1))
		ht_cap_info.tx_stbc = 0;

	status = ucfg_mlme_set_ht_cap_info(hdd_ctx->psoc, ht_cap_info);
	if (status != QDF_STATUS_SUCCESS)
		hdd_err("could not set HT capability to CCM");
#define WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES 0xff
	value_len = SIZE_OF_SUPPORTED_MCS_SET;
	if (ucfg_mlme_get_supported_mcs_set(
				hdd_ctx->psoc, mcs_set,
				&value_len) == QDF_STATUS_SUCCESS) {
		hdd_debug("Read MCS rate set");
		if (cfg->num_rf_chains > SIZE_OF_SUPPORTED_MCS_SET)
			cfg->num_rf_chains = SIZE_OF_SUPPORTED_MCS_SET;

		if (b_enable1x1) {
			for (value = 0; value < cfg->num_rf_chains; value++)
				mcs_set[value] =
					WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES;

			status = ucfg_mlme_set_supported_mcs_set(
					hdd_ctx->psoc,
					mcs_set,
					(qdf_size_t)SIZE_OF_SUPPORTED_MCS_SET);
			if (QDF_IS_STATUS_ERROR(status))
				hdd_err("could not set MCS SET to CCM");
		}
	}
#undef WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES
}

static void hdd_update_tgt_vht_cap(struct hdd_context *hdd_ctx,
				   struct wma_tgt_vht_cap *cfg)
{
	QDF_STATUS status;
	struct wiphy *wiphy = hdd_ctx->wiphy;
	struct ieee80211_supported_band *band_5g =
		wiphy->bands[HDD_NL80211_BAND_5GHZ];
	uint32_t ch_width;
	struct wma_caps_per_phy caps_per_phy = {0};
	bool vht_enable_2x2;
	uint32_t tx_highest_data_rate;
	uint32_t rx_highest_data_rate;

	if (!band_5g) {
		hdd_debug("5GHz band disabled, skipping capability population");
		return;
	}

	status = ucfg_mlme_update_vht_cap(hdd_ctx->psoc, cfg);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("could not update vht capabilities");

	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &vht_enable_2x2);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get vht_enable2x2");

	if (vht_enable_2x2) {
		tx_highest_data_rate =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_2_2;
		rx_highest_data_rate =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_2_2;
	} else {
		tx_highest_data_rate =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
		rx_highest_data_rate =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
	}

	status = ucfg_mlme_cfg_set_vht_rx_supp_data_rate(hdd_ctx->psoc,
							 rx_highest_data_rate);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to set rx_supp_data_rate");

	status = ucfg_mlme_cfg_set_vht_tx_supp_data_rate(hdd_ctx->psoc,
							 tx_highest_data_rate);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to set tx_supp_data_rate");

	/* Update the real highest data rate to wiphy */
	if (cfg->vht_short_gi_80 & WMI_VHT_CAP_SGI_80MHZ) {
		if (vht_enable_2x2) {
			tx_highest_data_rate =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_2_2_SGI80;
			rx_highest_data_rate =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_2_2_SGI80;
		} else {
			tx_highest_data_rate =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1_SGI80;
			rx_highest_data_rate =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1_SGI80;
		}
	}

	if (WMI_VHT_CAP_MAX_MPDU_LEN_11454 == cfg->vht_max_mpdu)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454;
	else if (WMI_VHT_CAP_MAX_MPDU_LEN_7935 == cfg->vht_max_mpdu)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_7991;
	else
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_3895;


	if (cfg->supp_chan_width & (1 << eHT_CHANNEL_WIDTH_80P80MHZ)) {
		band_5g->vht_cap.cap |=
			IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;
		ch_width = VHT_CAP_160_AND_80P80_SUPP;
	} else if (cfg->supp_chan_width & (1 << eHT_CHANNEL_WIDTH_160MHZ)) {
		band_5g->vht_cap.cap |=
			IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ;
		ch_width = VHT_CAP_160_SUPP;
	} else {
		ch_width = VHT_CAP_NO_160M_SUPP;
	}

	status = ucfg_mlme_set_vht_ch_width(hdd_ctx->psoc, ch_width);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("could not set the channel width");
	else
		hdd_debug("supported channel width %d", ch_width);

	if (cfg->vht_rx_ldpc & WMI_VHT_CAP_RX_LDPC) {
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_RXLDPC;
		hdd_debug("VHT RxLDPC capability is set");
	} else {
		/*
		 * Get the RX LDPC capability for the NON DBS
		 * hardware mode for 5G band
		 */
		status = wma_get_caps_for_phyidx_hwmode(&caps_per_phy,
					HW_MODE_DBS_NONE, CDS_BAND_5GHZ);
		if ((QDF_IS_STATUS_SUCCESS(status)) &&
			(caps_per_phy.vht_5g & WMI_VHT_CAP_RX_LDPC)) {
			band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_RXLDPC;
			hdd_debug("VHT RX LDPC capability is set");
		}
	}

	if (cfg->vht_short_gi_80 & WMI_VHT_CAP_SGI_80MHZ)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_SHORT_GI_80;
	if (cfg->vht_short_gi_160 & WMI_VHT_CAP_SGI_160MHZ)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_SHORT_GI_160;

	if (vht_enable_2x2 && (cfg->vht_tx_stbc & WMI_VHT_CAP_TX_STBC))
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_TXSTBC;

	if (cfg->vht_rx_stbc & WMI_VHT_CAP_RX_STBC_1SS)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_RXSTBC_1;
	if (vht_enable_2x2 && (cfg->vht_rx_stbc & WMI_VHT_CAP_RX_STBC_2SS))
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_RXSTBC_2;
	if (cfg->vht_rx_stbc & WMI_VHT_CAP_RX_STBC_3SS)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_RXSTBC_3;

	band_5g->vht_cap.cap |=
		(cfg->vht_max_ampdu_len_exp <<
		 IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT);

	if (cfg->vht_su_bformer & WMI_VHT_CAP_SU_BFORMER)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE;
	if (cfg->vht_su_bformee & WMI_VHT_CAP_SU_BFORMEE)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE;
	if (cfg->vht_mu_bformer & WMI_VHT_CAP_MU_BFORMER)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE;
	if (cfg->vht_mu_bformee & WMI_VHT_CAP_MU_BFORMEE)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE;

	if (cfg->vht_txop_ps & WMI_VHT_CAP_TXOP_PS)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_VHT_TXOP_PS;

	band_5g->vht_cap.vht_mcs.rx_highest = cpu_to_le16(rx_highest_data_rate);
	band_5g->vht_cap.vht_mcs.tx_highest = cpu_to_le16(tx_highest_data_rate);
}

/**
 * hdd_generate_macaddr_auto() - Auto-generate mac address
 * @hdd_ctx: Pointer to the HDD context
 *
 * Auto-generate mac address using device serial number.
 * Keep the first 3 bytes of OUI as before and replace
 * the last 3 bytes with the lower 3 bytes of serial number.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int hdd_generate_macaddr_auto(struct hdd_context *hdd_ctx)
{
	unsigned int serialno = 0;
	struct qdf_mac_addr mac_addr = {
		{0x00, 0x0A, 0xF5, 0x00, 0x00, 0x00}
	};

	serialno = pld_socinfo_get_serial_number(hdd_ctx->parent_dev);
	if (serialno == 0)
		return -EINVAL;

	serialno &= 0x00ffffff;

	mac_addr.bytes[3] = (serialno >> 16) & 0xff;
	mac_addr.bytes[4] = (serialno >> 8) & 0xff;
	mac_addr.bytes[5] = serialno & 0xff;

	hdd_update_macaddr(hdd_ctx, mac_addr, true);
	return 0;
}

static void hdd_sar_target_config(struct hdd_context *hdd_ctx,
				  struct wma_tgt_cfg *cfg)
{
	hdd_ctx->sar_version = cfg->sar_version;
}

static void hdd_update_vhtcap_2g(struct hdd_context *hdd_ctx)
{
	uint64_t chip_mode = 0;
	QDF_STATUS status;
	bool b2g_vht_cfg = false;
	bool b2g_vht_target = false;
	struct wma_caps_per_phy caps_per_phy = {0};
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(hdd_ctx->psoc);
	if (!wmi_handle) {
		hdd_err("wmi handle is NULL");
		return;
	}

	status = ucfg_mlme_get_vht_for_24ghz(hdd_ctx->psoc, &b2g_vht_cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get 2g vht mode");
		return;
	}
	if (wmi_service_enabled(wmi_handle, wmi_service_ext_msg)) {
		status = wma_get_caps_for_phyidx_hwmode(&caps_per_phy,
							HW_MODE_DBS_NONE,
							CDS_BAND_2GHZ);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to get phy caps");
			return;
		}
		if (caps_per_phy.vht_2g)
			b2g_vht_target = true;
	} else {
		status = wlan_reg_get_chip_mode(hdd_ctx->pdev, &chip_mode);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to get chip mode");
			return;
		}
		b2g_vht_target =
		(chip_mode & HOST_REGDMN_MODE_11AC_VHT20_2G) ?
		true : false;
	}

	b2g_vht_cfg = b2g_vht_cfg && b2g_vht_target;
	hdd_debug("vht 2g target: %d, cfg: %d", b2g_vht_target, b2g_vht_cfg);
	status = ucfg_mlme_set_vht_for_24ghz(hdd_ctx->psoc, b2g_vht_cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to update 2g vht mode");
		return;
	}
}

static void hdd_extract_fw_version_info(struct hdd_context *hdd_ctx)
{
	hdd_ctx->fw_version_info.major_spid =
			HDD_FW_VER_MAJOR_SPID(hdd_ctx->target_fw_version);
	hdd_ctx->fw_version_info.minor_spid =
			HDD_FW_VER_MINOR_SPID(hdd_ctx->target_fw_version);
	hdd_ctx->fw_version_info.siid =
			HDD_FW_VER_SIID(hdd_ctx->target_fw_version);
	hdd_ctx->fw_version_info.crmid =
			HDD_FW_VER_CRM_ID(hdd_ctx->target_fw_version);
	hdd_ctx->fw_version_info.sub_id =
			HDD_FW_VER_SUB_ID(hdd_ctx->target_fw_vers_ext);
	hdd_ctx->fw_version_info.rel_id =
			HDD_FW_VER_REL_ID(hdd_ctx->target_fw_vers_ext);
}

#if defined(WLAN_FEATURE_11AX) && \
	(defined(CFG80211_SBAND_IFTYPE_DATA_BACKPORT) || \
	 (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)))

#if defined(CONFIG_BAND_6GHZ) && (defined(IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START))
static void hdd_update_wiphy_he_6ghz_capa(struct hdd_context *hdd_ctx)
{
	uint16_t he_6ghz_capa = 0;
	uint8_t min_mpdu_start_spacing;
	uint8_t max_ampdu_len_exp;
	uint8_t max_mpdu_len;
	uint8_t sm_pow_save;

	ucfg_mlme_get_ht_mpdu_density(hdd_ctx->psoc, &min_mpdu_start_spacing);
	he_6ghz_capa |= FIELD_PREP(IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START,
				   min_mpdu_start_spacing);

	ucfg_mlme_cfg_get_vht_ampdu_len_exp(hdd_ctx->psoc, &max_ampdu_len_exp);
	he_6ghz_capa |= FIELD_PREP(IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP,
				   max_ampdu_len_exp);

	ucfg_mlme_cfg_get_vht_max_mpdu_len(hdd_ctx->psoc, &max_mpdu_len);
	he_6ghz_capa |= FIELD_PREP(IEEE80211_HE_6GHZ_CAP_MAX_MPDU_LEN,
				   max_mpdu_len);

	ucfg_mlme_cfg_get_ht_smps(hdd_ctx->psoc, &sm_pow_save);
	he_6ghz_capa |= FIELD_PREP(IEEE80211_HE_6GHZ_CAP_SM_PS, sm_pow_save);

	he_6ghz_capa |= IEEE80211_HE_6GHZ_CAP_RX_ANTPAT_CONS;
	he_6ghz_capa |= IEEE80211_HE_6GHZ_CAP_TX_ANTPAT_CONS;

	hdd_ctx->iftype_data_6g->he_6ghz_capa.capa = he_6ghz_capa;
}
#else
static inline void hdd_update_wiphy_he_6ghz_capa(struct hdd_context *hdd_ctx)
{
}
#endif

#if defined(CONFIG_BAND_6GHZ) && (defined(CFG80211_6GHZ_BAND_SUPPORTED) || \
      (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE))
static void
hdd_update_wiphy_he_caps_6ghz(struct hdd_context *hdd_ctx,
			      tDot11fIEhe_cap *he_cap_cfg)
{
	struct ieee80211_supported_band *band_6g =
		   hdd_ctx->wiphy->bands[HDD_NL80211_BAND_6GHZ];
	uint8_t *phy_info =
		    hdd_ctx->iftype_data_6g->he_cap.he_cap_elem.phy_cap_info;
	uint8_t *mac_info_6g =
		hdd_ctx->iftype_data_6g->he_cap.he_cap_elem.mac_cap_info;
	uint8_t max_fw_bw = sme_get_vht_ch_width();

	if (!band_6g || !phy_info) {
		hdd_debug("6ghz not supported in wiphy");
		return;
	}

	hdd_ctx->iftype_data_6g->types_mask =
		(BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP));
	hdd_ctx->iftype_data_6g->he_cap.has_he = true;
	band_6g->n_iftype_data = 1;

	if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
		phy_info[0] |=
		      IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G;
	if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		phy_info[0] |=
			IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G;
	if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
		phy_info[0] |=
		     IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G;

	if (he_cap_cfg->twt_request)
		mac_info_6g[0] |= IEEE80211_HE_MAC_CAP0_TWT_REQ;

	if (he_cap_cfg->twt_responder)
		mac_info_6g[0] |= IEEE80211_HE_MAC_CAP0_TWT_RES;

	hdd_update_wiphy_he_6ghz_capa(hdd_ctx);

	band_6g->iftype_data = hdd_ctx->iftype_data_6g;
}
#else
static inline void
hdd_update_wiphy_he_caps_6ghz(struct hdd_context *hdd_ctx,
			      tDot11fIEhe_cap *he_cap_cfg)
{
}
#endif

static void hdd_update_wiphy_he_cap(struct hdd_context *hdd_ctx)
{
	tDot11fIEhe_cap he_cap_cfg;
	struct ieee80211_supported_band *band_2g =
			hdd_ctx->wiphy->bands[HDD_NL80211_BAND_2GHZ];
	struct ieee80211_supported_band *band_5g =
			hdd_ctx->wiphy->bands[HDD_NL80211_BAND_5GHZ];
	QDF_STATUS status;
	uint8_t *phy_info_5g =
		    hdd_ctx->iftype_data_5g->he_cap.he_cap_elem.phy_cap_info;
	uint8_t max_fw_bw = sme_get_vht_ch_width();
	uint32_t channel_bonding_mode_2g;
	uint8_t *phy_info_2g =
		    hdd_ctx->iftype_data_2g->he_cap.he_cap_elem.phy_cap_info;
	uint8_t *mac_info_2g =
		hdd_ctx->iftype_data_2g->he_cap.he_cap_elem.mac_cap_info;
	uint8_t *mac_info_5g =
		hdd_ctx->iftype_data_5g->he_cap.he_cap_elem.mac_cap_info;

	status = ucfg_mlme_cfg_get_he_caps(hdd_ctx->psoc, &he_cap_cfg);

	if (QDF_IS_STATUS_ERROR(status))
		return;

	if (band_2g) {
		hdd_ctx->iftype_data_2g->types_mask =
			(BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP));
		hdd_ctx->iftype_data_2g->he_cap.has_he = he_cap_cfg.present;
		band_2g->n_iftype_data = 1;
		band_2g->iftype_data = hdd_ctx->iftype_data_2g;

		ucfg_mlme_get_channel_bonding_24ghz(hdd_ctx->psoc,
						    &channel_bonding_mode_2g);
		if (channel_bonding_mode_2g)
			phy_info_2g[0] |=
			    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G;

		if (he_cap_cfg.twt_request)
			mac_info_2g[0] |= IEEE80211_HE_MAC_CAP0_TWT_REQ;

		if (he_cap_cfg.twt_responder)
			mac_info_2g[0] |= IEEE80211_HE_MAC_CAP0_TWT_RES;
	}
	if (band_5g) {
		hdd_ctx->iftype_data_5g->types_mask =
			(BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP));
		hdd_ctx->iftype_data_5g->he_cap.has_he = he_cap_cfg.present;
		band_5g->n_iftype_data = 1;
		band_5g->iftype_data = hdd_ctx->iftype_data_5g;
		if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
			phy_info_5g[0] |=
				IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G;
		if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
			phy_info_5g[0] |=
				IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G;
		if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
			phy_info_5g[0] |=
			     IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G;

		if (he_cap_cfg.twt_request)
			mac_info_5g[0] |= IEEE80211_HE_MAC_CAP0_TWT_REQ;

		if (he_cap_cfg.twt_responder)
			mac_info_5g[0] |= IEEE80211_HE_MAC_CAP0_TWT_RES;
	}

	hdd_update_wiphy_he_caps_6ghz(hdd_ctx, &he_cap_cfg);
}
#else
static void hdd_update_wiphy_he_cap(struct hdd_context *hdd_ctx)
{
}
#endif

static void hdd_component_cfg_chan_to_freq(struct wlan_objmgr_pdev *pdev)
{
	ucfg_mlme_cfg_chan_to_freq(pdev);
}

static uint32_t hdd_update_band_cap_from_dot11mode(
		struct hdd_context *hdd_ctx, uint32_t band_capability)
{
	if (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_AUTO)
		return band_capability;

	if (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11b ||
	    hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11g ||
	    hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11g_ONLY ||
	    hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11b_ONLY)
		band_capability = (band_capability & (~BIT(REG_BAND_5G)));

	if (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11a)
		band_capability = (band_capability & (~BIT(REG_BAND_2G)));

	if (hdd_ctx->config->dot11Mode != eHDD_DOT11_MODE_11ax_ONLY &&
	    hdd_ctx->config->dot11Mode != eHDD_DOT11_MODE_11ax)
		band_capability = (band_capability & (~BIT(REG_BAND_6G)));

	qdf_debug("Update band capability %x", band_capability);
	return band_capability;
}

#ifdef FEATURE_WPSS_THERMAL_MITIGATION
static inline
void hdd_update_multi_client_thermal_support(struct hdd_context *hdd_ctx)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(hdd_ctx->psoc);
	if (!wmi_handle)
		return;

	hdd_ctx->multi_client_thermal_mitigation =
		wmi_service_enabled(wmi_handle,
				    wmi_service_thermal_multi_client_support);
}
#else
static inline
void hdd_update_multi_client_thermal_support(struct hdd_context *hdd_ctx)
{
}
#endif

int hdd_update_tgt_cfg(hdd_handle_t hdd_handle, struct wma_tgt_cfg *cfg)
{
	int ret;
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	uint32_t temp_band_cap, band_capability;
	struct cds_config_info *cds_cfg = cds_get_ini_config();
	uint8_t antenna_mode;
	uint8_t sub_20_chan_width;
	QDF_STATUS status;
	mac_handle_t mac_handle;
	bool bval = false;
	uint8_t value = 0;
	uint32_t fine_time_meas_cap = 0;
	enum nss_chains_band_info band;
	bool enable_dynamic_cfg;

	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return -EINVAL;
	}
	ret = hdd_objmgr_create_and_store_pdev(hdd_ctx);
	if (ret) {
		QDF_DEBUG_PANIC("Failed to create pdev; errno:%d", ret);
		return -EINVAL;
	}

	hdd_debug("New pdev has been created with pdev_id = %u",
		  hdd_ctx->pdev->pdev_objmgr.wlan_pdev_id);

	status = dispatcher_pdev_open(hdd_ctx->pdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_DEBUG_PANIC("dispatcher pdev open failed; status:%d",
				status);
		ret = qdf_status_to_os_return(status);
		goto exit;
	}

	status = hdd_component_pdev_open(hdd_ctx->pdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		QDF_DEBUG_PANIC("hdd component pdev open failed; status:%d",
				status);
		ret = qdf_status_to_os_return(status);
		goto dispatcher_close;
	}
	/*
	 * For 6GHz support this api is added to convert mlme cfgs
	 * channel numbers to frequency
	 */
	hdd_component_cfg_chan_to_freq(hdd_ctx->pdev);

	hdd_objmgr_update_tgt_max_vdev_psoc(hdd_ctx, cfg->max_intf_count);

	ucfg_ipa_set_dp_handle(hdd_ctx->psoc,
			       cds_get_context(QDF_MODULE_ID_SOC));
	ucfg_ipa_set_pdev_id(hdd_ctx->psoc, OL_TXRX_PDEV_ID);

	status = ucfg_mlme_get_sub_20_chan_width(hdd_ctx->psoc,
						 &sub_20_chan_width);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get sub_20_chan_width config");
		ret = qdf_status_to_os_return(status);
		goto pdev_close;
	}

	if (cds_cfg) {
		if (sub_20_chan_width !=
		    WLAN_SUB_20_CH_WIDTH_NONE && !cfg->sub_20_support) {
			hdd_err("User requested sub 20 MHz channel width but unsupported by FW.");
			cds_cfg->sub_20_channel_width =
				WLAN_SUB_20_CH_WIDTH_NONE;
		} else {
			cds_cfg->sub_20_channel_width = sub_20_chan_width;
		}
	}

	status = ucfg_mlme_get_band_capability(hdd_ctx->psoc, &band_capability);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get MLME band capability");
		ret = qdf_status_to_os_return(status);
		goto pdev_close;
	}

	band_capability =
		hdd_update_band_cap_from_dot11mode(hdd_ctx, band_capability);

	/* first store the INI band capability */
	temp_band_cap = band_capability;

	band_capability = cfg->band_cap;
	hdd_ctx->is_fils_roaming_supported =
			cfg->services.is_fils_roaming_supported;

	hdd_ctx->config->is_11k_offload_supported =
			cfg->services.is_11k_offload_supported;

	/*
	 * merge the target band capability with INI setting if the merge has
	 * at least 1 band enabled
	 */
	temp_band_cap &= band_capability;
	if (!temp_band_cap)
		hdd_warn("ini BandCapability not supported by the target");
	else
		band_capability = temp_band_cap;

	status = ucfg_mlme_set_band_capability(hdd_ctx->psoc, band_capability);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to set MLME Band Capability");
		ret = qdf_status_to_os_return(status);
		goto pdev_close;
	}

	hdd_ctx->curr_band = band_capability;
	hdd_ctx->psoc->soc_nif.user_config.band_capability = hdd_ctx->curr_band;

	status = wlan_hdd_update_wiphy_supported_band(hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to update wiphy band info");
		goto pdev_close;
	}

	status = ucfg_reg_set_band(hdd_ctx->pdev, band_capability);
	if (QDF_IS_STATUS_ERROR(status))
		/*
		 * Continue, Do not close the pdev from here as if host fails
		 * to update band information if cc_list event is not received
		 * by this time, then also driver load should happen.
		 */
		hdd_err("Failed to update regulatory band info");

	if (!cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		hdd_ctx->reg.reg_domain = cfg->reg_domain;
		hdd_ctx->reg.eeprom_rd_ext = cfg->eeprom_rd_ext;
	}

	/* This can be extended to other configurations like ht, vht cap... */
	status = wlan_hdd_validate_mac_address(&cfg->hw_macaddr);
	if (QDF_IS_STATUS_SUCCESS(status))
		qdf_mem_copy(&hdd_ctx->hw_macaddr, &cfg->hw_macaddr,
			     QDF_MAC_ADDR_SIZE);

	hdd_ctx->target_fw_version = cfg->target_fw_version;
	hdd_ctx->target_fw_vers_ext = cfg->target_fw_vers_ext;
	hdd_extract_fw_version_info(hdd_ctx);

	hdd_ctx->hw_bd_id = cfg->hw_bd_id;
	qdf_mem_copy(&hdd_ctx->hw_bd_info, &cfg->hw_bd_info,
		     sizeof(cfg->hw_bd_info));

	if (cfg->max_intf_count > WLAN_MAX_VDEVS) {
		hdd_err("fw max vdevs (%u) > host max vdevs (%u); using %u",
			cfg->max_intf_count, WLAN_MAX_VDEVS, WLAN_MAX_VDEVS);
		hdd_ctx->max_intf_count = WLAN_MAX_VDEVS;
	} else {
		hdd_ctx->max_intf_count = cfg->max_intf_count;
	}

	hdd_sar_target_config(hdd_ctx, cfg);
	hdd_lpass_target_config(hdd_ctx, cfg);

	hdd_ctx->ap_arpns_support = cfg->ap_arpns_support;

	hdd_update_tgt_services(hdd_ctx, &cfg->services);

	hdd_update_tgt_ht_cap(hdd_ctx, &cfg->ht_cap);

	sme_update_bfer_caps_as_per_nss_chains(hdd_ctx->mac_handle, cfg);

	hdd_update_tgt_vht_cap(hdd_ctx, &cfg->vht_cap);
	if (cfg->services.en_11ax  &&
	    (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_AUTO ||
	     hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11ax ||
	     hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11ax_ONLY)) {
		hdd_debug("11AX: 11ax is enabled - update HDD config");
		hdd_update_tgt_he_cap(hdd_ctx, cfg);
		hdd_update_wiphy_he_cap(hdd_ctx);
	}
	hdd_update_tgt_twt_cap(hdd_ctx, cfg);
	hdd_update_tgt_eht_cap(hdd_ctx, cfg);
	hdd_update_wiphy_eht_cap(hdd_ctx);

	for (band = NSS_CHAINS_BAND_2GHZ; band < NSS_CHAINS_BAND_MAX; band++) {
		sme_modify_nss_chains_tgt_cfg(hdd_ctx->mac_handle,
					      QDF_STA_MODE, band);
		sme_modify_nss_chains_tgt_cfg(hdd_ctx->mac_handle,
					      QDF_SAP_MODE, band);
		sme_modify_nss_chains_tgt_cfg(hdd_ctx->mac_handle,
					      QDF_TDLS_MODE, band);
		sme_modify_nss_chains_tgt_cfg(hdd_ctx->mac_handle,
					      QDF_P2P_DEVICE_MODE,
					      band);
		sme_modify_nss_chains_tgt_cfg(hdd_ctx->mac_handle,
					      QDF_OCB_MODE, band);
		sme_modify_nss_chains_tgt_cfg(hdd_ctx->mac_handle,
					      QDF_TDLS_MODE, band);
	}

	hdd_update_vdev_nss(hdd_ctx);

	status =
	  ucfg_mlme_get_enable_dynamic_nss_chains_cfg(hdd_ctx->psoc,
						      &enable_dynamic_cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("unable to get enable dynamic config");
		hdd_ctx->dynamic_nss_chains_support = false;
	} else {
		hdd_ctx->dynamic_nss_chains_support =
					cfg->dynamic_nss_chains_support &
					enable_dynamic_cfg;
		hdd_debug("Dynamic nss chain support FW %d driver %d",
			   cfg->dynamic_nss_chains_support, enable_dynamic_cfg);
	}

	ucfg_mlme_get_fine_time_meas_cap(hdd_ctx->psoc, &fine_time_meas_cap);
	fine_time_meas_cap &= cfg->fine_time_measurement_cap;
	status = ucfg_mlme_set_fine_time_meas_cap(hdd_ctx->psoc,
						  fine_time_meas_cap);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to set fine_time_meas_cap, 0x%x, ox%x",
			fine_time_meas_cap, cfg->fine_time_measurement_cap);
		ucfg_mlme_get_fine_time_meas_cap(hdd_ctx->psoc,
						 &fine_time_meas_cap);
	}

	hdd_ctx->fine_time_meas_cap_target = cfg->fine_time_measurement_cap;
	hdd_debug("fine_time_meas_cap: 0x%x", fine_time_meas_cap);

	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &bval);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get vht_enable2x2");

	antenna_mode = (bval == 0x01) ?
			HDD_ANTENNA_MODE_2X2 : HDD_ANTENNA_MODE_1X1;
	hdd_update_smps_antenna_mode(hdd_ctx, antenna_mode);
	hdd_debug("Init current antenna mode: %d",
		  hdd_ctx->current_antenna_mode);

	hdd_ctx->rcpi_enabled = cfg->rcpi_enabled;

	status = ucfg_mlme_cfg_get_vht_tx_bfee_ant_supp(hdd_ctx->psoc,
							&value);
	if (QDF_IS_STATUS_ERROR(status)) {
		status = false;
		hdd_err("set tx_bfee_ant_supp failed");
	}

	status = ucfg_mlme_set_restricted_80p80_bw_supp(hdd_ctx->psoc,
							cfg->restricted_80p80_bw_supp);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to set MLME restircted 80p80 BW support");

	if ((value > MLME_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF) &&
	    !cfg->tx_bfee_8ss_enabled) {
		status = ucfg_mlme_cfg_set_vht_tx_bfee_ant_supp(hdd_ctx->psoc,
				MLME_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF);
		if (QDF_IS_STATUS_ERROR(status)) {
			status = false;
			hdd_err("set tx_bfee_ant_supp failed");
		}
	}

	mac_handle = hdd_ctx->mac_handle;

	hdd_debug("txBFCsnValue %d", value);

	/*
	 * Update txBFCsnValue and NumSoundingDim values to vhtcap in wiphy
	 */
	hdd_update_wiphy_vhtcap(hdd_ctx);

	hdd_update_vhtcap_2g(hdd_ctx);

	hdd_ctx->wmi_max_len = cfg->wmi_max_len;

	wlan_config_sched_scan_plans_to_wiphy(hdd_ctx->wiphy, hdd_ctx->psoc);
	/*
	 * This needs to be done after HDD pdev is created and stored since
	 * it will access the HDD pdev object lock.
	 */
	hdd_runtime_suspend_context_init(hdd_ctx);

	/* Configure NAN datapath features */
	hdd_nan_datapath_target_config(hdd_ctx, cfg);
	ucfg_nan_set_tgt_caps(hdd_ctx->psoc, &cfg->nan_caps);
	hdd_ctx->dfs_cac_offload = cfg->dfs_cac_offload;
	hdd_ctx->lte_coex_ant_share = cfg->services.lte_coex_ant_share;
	hdd_ctx->obss_scan_offload = cfg->services.obss_scan_offload;
	ucfg_scan_set_obss_scan_offload(hdd_ctx->psoc,
					hdd_ctx->obss_scan_offload);
	status = ucfg_mlme_set_obss_detection_offload_enabled(
			hdd_ctx->psoc, cfg->obss_detection_offloaded);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Couldn't pass WNI_CFG_OBSS_DETECTION_OFFLOAD to CFG");

	status = ucfg_mlme_set_obss_color_collision_offload_enabled(
			hdd_ctx->psoc, cfg->obss_color_collision_offloaded);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to set WNI_CFG_OBSS_COLOR_COLLISION_OFFLOAD");

	if (!cfg->obss_color_collision_offloaded) {
		status = ucfg_mlme_set_bss_color_collision_det_sta(
				hdd_ctx->psoc,
				cfg->obss_color_collision_offloaded);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("Failed to set CFG_BSS_CLR_COLLISION_DET_STA");
	}

	hdd_update_score_config(hdd_ctx);
	hdd_update_multi_client_thermal_support(hdd_ctx);

	ucfg_psoc_mlme_set_11be_capab(hdd_ctx->psoc, cfg->services.en_11be);
	return 0;

dispatcher_close:
	dispatcher_pdev_close(hdd_ctx->pdev);
pdev_close:
	hdd_component_pdev_close(hdd_ctx->pdev);
exit:
	hdd_objmgr_release_and_destroy_pdev(hdd_ctx);

	return ret;
}

bool hdd_dfs_indicate_radar(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	struct hdd_ap_ctx *ap_ctx;
	bool dfs_disable_channel_switch = false;

	if (!hdd_ctx) {
		hdd_info("Couldn't get hdd_ctx");
		return true;
	}

	ucfg_mlme_get_dfs_disable_channel_switch(hdd_ctx->psoc,
						 &dfs_disable_channel_switch);
	if (dfs_disable_channel_switch) {
		hdd_info("skip tx block hdd_ctx=%pK, disableDFSChSwitch=%d",
			 hdd_ctx, dfs_disable_channel_switch);
		return true;
	}

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_DFS_INDICATE_RADAR) {
		ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

		if ((QDF_SAP_MODE == adapter->device_mode ||
		    QDF_P2P_GO_MODE == adapter->device_mode) &&
		    (wlan_reg_is_passive_or_disable_for_pwrmode(hdd_ctx->pdev,
		     ap_ctx->operating_chan_freq, REG_CURRENT_PWR_MODE))) {
			WLAN_HDD_GET_AP_CTX_PTR(adapter)->dfs_cac_block_tx =
				true;
			hdd_info("tx blocked for vdev: %d",
				adapter->vdev_id);
			if (adapter->vdev_id != WLAN_UMAC_VDEV_ID_MAX)
				cdp_fc_vdev_flush(
					cds_get_context(QDF_MODULE_ID_SOC),
					adapter->vdev_id);
		}
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_DFS_INDICATE_RADAR);
	}

	return true;
}

bool hdd_is_valid_mac_address(const uint8_t *mac_addr)
{
	int xdigit = 0;
	int separator = 0;

	while (*mac_addr) {
		if (isxdigit(*mac_addr)) {
			xdigit++;
		} else if (':' == *mac_addr) {
			if (0 == xdigit || ((xdigit / 2) - 1) != separator)
				break;

			++separator;
		} else {
			/* Invalid MAC found */
			return false;
		}
		++mac_addr;
	}
	return xdigit == 12 && (separator == 5 || separator == 0);
}

/**
 * hdd_mon_mode_ether_setup() - Update monitor mode struct net_device.
 * @dev: Handle to struct net_device to be updated.
 *
 * Return: None
 */
static void hdd_mon_mode_ether_setup(struct net_device *dev)
{
	dev->header_ops         = NULL;
	dev->type               = ARPHRD_IEEE80211_RADIOTAP;
	dev->hard_header_len    = ETH_HLEN;
	dev->mtu                = ETH_DATA_LEN;
	dev->addr_len           = ETH_ALEN;
	dev->tx_queue_len       = 1000; /* Ethernet wants good queues */
	dev->flags              = IFF_BROADCAST|IFF_MULTICAST;
	dev->priv_flags        |= IFF_TX_SKB_SHARING;

	memset(dev->broadcast, 0xFF, ETH_ALEN);
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * hdd_mon_turn_off_ps_and_wow() - Update monitor mode struct net_device.
 * @hdd_ctx: Pointer to HDD context.
 *
 * Return: None
 */
static void hdd_mon_turn_off_ps_and_wow(struct hdd_context *hdd_ctx)
{
	ucfg_pmo_set_power_save_mode(hdd_ctx->psoc,
				     PMO_PS_ADVANCED_POWER_SAVE_DISABLE);
	ucfg_pmo_set_wow_enable(hdd_ctx->psoc, PMO_WOW_DISABLE_BOTH);
}

/**
 * __hdd_mon_open() - HDD Open function
 * @dev: Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int __hdd_mon_open(struct net_device *dev)
{
	int ret;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct bbm_params param = {0};

	hdd_enter_dev(dev);

	if (test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_debug_rl("Monitor interface is already up");
		return 0;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	hdd_mon_mode_ether_setup(dev);

	if (con_mode == QDF_GLOBAL_MONITOR_MODE ||
	    ucfg_mlme_is_sta_mon_conc_supported(hdd_ctx->psoc)) {
		ret = hdd_trigger_psoc_idle_restart(hdd_ctx);
		if (ret) {
			hdd_err("Failed to start WLAN modules return");
			return ret;
		}
		hdd_err("hdd_wlan_start_modules() successful !");

		if ((!test_bit(SME_SESSION_OPENED, &adapter->event_flags)) ||
		    (policy_mgr_is_sta_mon_concurrency(hdd_ctx->psoc))) {
			ret = hdd_start_adapter(adapter);
			if (ret) {
				hdd_err("Failed to start adapter :%d",
						adapter->device_mode);
				return ret;
			}
			hdd_err("hdd_start_adapters() successful !");
		}
		hdd_mon_turn_off_ps_and_wow(hdd_ctx);
		set_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);
	}

	if (con_mode != QDF_GLOBAL_MONITOR_MODE &&
	    ucfg_mlme_is_sta_mon_conc_supported(hdd_ctx->psoc)) {
		hdd_info("Acquire wakelock for STA + monitor mode");
		qdf_wake_lock_acquire(&hdd_ctx->monitor_mode_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_MONITOR_MODE);
		qdf_runtime_pm_prevent_suspend(
			&hdd_ctx->runtime_context.monitor_mode);
	}

	ret = hdd_set_mon_rx_cb(dev);

	if (!ret)
		ret = hdd_enable_monitor_mode(dev);

	if (!ret) {
		param.policy = BBM_DRIVER_MODE_POLICY;
		param.policy_info.driver_mode = QDF_GLOBAL_MONITOR_MODE;
		ucfg_dp_bbm_apply_independent_policy(hdd_ctx->psoc, &param);
		ucfg_dp_set_current_throughput_level(hdd_ctx->psoc,
						     PLD_BUS_WIDTH_VERY_HIGH);
	}

	return ret;
}

/**
 * hdd_mon_open() - Wrapper function for __hdd_mon_open to protect it from SSR
 * @net_dev: Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int hdd_mon_open(struct net_device *net_dev)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_trans_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_mon_open(net_dev);

	osif_vdev_sync_trans_stop(vdev_sync);

	return errno;
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE
/**
 * __hdd_pktcapture_open() - HDD Open function
 * @dev: Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int __hdd_pktcapture_open(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_adapter *sta_adapter;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	int ret;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	sta_adapter = hdd_get_adapter(hdd_ctx, QDF_STA_MODE);
	if (!sta_adapter) {
		hdd_err("No station interface found");
		return -EINVAL;
	}

	vdev = hdd_objmgr_get_vdev_by_user(sta_adapter, WLAN_OSIF_ID);
	if (!vdev)
		return -EINVAL;

	hdd_mon_mode_ether_setup(dev);

	status = ucfg_dp_register_pkt_capture_callbacks(vdev);
	ret = qdf_status_to_os_return(status);
	if (ret) {
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
		return ret;
	}

	adapter->vdev = vdev;
	set_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);
	sta_adapter->mon_adapter = adapter;

	return ret;
}

/**
 * hdd_pktcapture_open() - Wrapper function for hdd_pktcapture_open to
 * protect it from SSR
 * @net_dev: Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int hdd_pktcapture_open(struct net_device *net_dev)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_trans_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_pktcapture_open(net_dev);

	osif_vdev_sync_trans_stop(vdev_sync);

	return errno;
}

/**
 * hdd_unmap_monitor_interface_vdev() - unmap monitor interface vdev and
 * deregister packet capture callbacks
 * @sta_adapter: station adapter
 *
 * Return: void
 */
static void
hdd_unmap_monitor_interface_vdev(struct hdd_adapter *sta_adapter)
{
	struct hdd_adapter *mon_adapter = sta_adapter->mon_adapter;

	if (mon_adapter && hdd_is_interface_up(mon_adapter)) {
		ucfg_pkt_capture_deregister_callbacks(mon_adapter->vdev);
		hdd_objmgr_put_vdev_by_user(mon_adapter->vdev,
					    WLAN_OSIF_ID);
		mon_adapter->vdev = NULL;
		hdd_reset_monitor_interface(sta_adapter);
	}
}

/**
 * hdd_map_monitor_interface_vdev() - Map monitor interface vdev and
 * register packet capture callbacks
 * @sta_adapter: Station adapter
 *
 * Return: None
 */
static void hdd_map_monitor_interface_vdev(struct hdd_adapter *sta_adapter)
{
	struct hdd_adapter *mon_adapter;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	int ret;

	mon_adapter = hdd_get_adapter(sta_adapter->hdd_ctx, QDF_MONITOR_MODE);
	if (!mon_adapter) {
		hdd_debug("No monitor interface found");
		return;
	}

	if (!mon_adapter || !hdd_is_interface_up(mon_adapter)) {
		hdd_debug("Monitor interface is not up\n");
		return;
	}

	if (!wlan_hdd_is_session_type_monitor(mon_adapter->device_mode))
		return;

	vdev = hdd_objmgr_get_vdev_by_user(sta_adapter, WLAN_OSIF_ID);
	if (!vdev)
		return;

	status = ucfg_dp_register_pkt_capture_callbacks(vdev);
	ret = qdf_status_to_os_return(status);
	if (ret) {
		hdd_err("Failed registering packet capture callbacks");
		hdd_objmgr_put_vdev_by_user(vdev,
					    WLAN_OSIF_ID);
		return;
	}

	mon_adapter->vdev = vdev;
	sta_adapter->mon_adapter = mon_adapter;
}

void hdd_reset_monitor_interface(struct hdd_adapter *sta_adapter)
{
	sta_adapter->mon_adapter = NULL;
}

struct hdd_adapter *
hdd_is_pkt_capture_mon_enable(struct hdd_adapter *sta_adapter)
{
	return sta_adapter->mon_adapter;
}
#else
static inline void
hdd_unmap_monitor_interface_vdev(struct hdd_adapter *sta_adapter)
{
}

static inline void
hdd_map_monitor_interface_vdev(struct hdd_adapter *sta_adapter)
{
}
#endif

static QDF_STATUS
wlan_hdd_update_dbs_scan_and_fw_mode_config(void)
{
	struct policy_mgr_dual_mac_config cfg = {0};
	QDF_STATUS status;
	uint32_t chnl_sel_logic_conc = 0;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	uint8_t dual_mac_feature = DISABLE_DBS_CXN_AND_SCAN;

	if (!hdd_ctx)
		return QDF_STATUS_E_FAILURE;

	/*
	 * ROME platform doesn't support any DBS related commands in FW,
	 * so if driver sends wmi command with dual_mac_config with all set to
	 * 0 then FW wouldn't respond back and driver would timeout on waiting
	 * for response. Check if FW supports DBS to eliminate ROME vs
	 * NON-ROME platform.
	 */
	if (!policy_mgr_find_if_fw_supports_dbs(hdd_ctx->psoc))
		return QDF_STATUS_SUCCESS;

	if (hdd_ctx->is_dual_mac_cfg_updated) {
		hdd_debug("dual mac config has already been updated, skip");
		return QDF_STATUS_SUCCESS;
	}

	cfg.scan_config = 0;
	cfg.fw_mode_config = 0;
	cfg.set_dual_mac_cb = policy_mgr_soc_set_dual_mac_cfg_cb;
	if (policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc)) {
		status =
		ucfg_policy_mgr_get_chnl_select_plcy(hdd_ctx->psoc,
						     &chnl_sel_logic_conc);
		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("can't get chnl sel policy, use def");
			return status;
		}
	}
	status =
	ucfg_policy_mgr_get_dual_mac_feature(hdd_ctx->psoc,
					     &dual_mac_feature);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("ucfg_policy_mgr_get_dual_mac_feature failed, use def");
		return status;
	}

	if (dual_mac_feature != DISABLE_DBS_CXN_AND_SCAN) {
		status = policy_mgr_get_updated_scan_and_fw_mode_config(
				hdd_ctx->psoc, &cfg.scan_config,
				&cfg.fw_mode_config,
				dual_mac_feature,
				chnl_sel_logic_conc);

		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("wma_get_updated_scan_and_fw_mode_config failed %d",
				status);
			return status;
		}
	}

	hdd_debug("send scan_cfg: 0x%x fw_mode_cfg: 0x%x to fw",
		cfg.scan_config, cfg.fw_mode_config);

	status = sme_soc_set_dual_mac_config(cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("sme_soc_set_dual_mac_config failed %d", status);
		return status;
	}
	hdd_ctx->is_dual_mac_cfg_updated = true;

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_max_sta_interface_up_count_reached() - check sta/p2p_cli vdev count
 * @adapter: HDD adapter
 *
 * Return: true if vdev limit reached
 */
static bool hdd_max_sta_interface_up_count_reached(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_adapter *temp_adapter = NULL, *next_adapter = NULL;
	uint8_t intf_count = 0;
	wlan_net_dev_ref_dbgid dbgid =
				NET_DEV_HOLD_MAX_STA_INTERFACE_UP_COUNT_REACHED;

	if (0 == CFG_TGT_DEFAULT_MAX_STA_VDEVS)
		return false;

	/*
	 * Check for max no of supported STA/P2PCLI VDEVs before
	 * creating another one.
	 */
	hdd_for_each_adapter_dev_held_safe(hdd_ctx, temp_adapter,
					   next_adapter, dbgid) {
		if ((temp_adapter != adapter) &&
		    (temp_adapter->dev->flags & IFF_UP) &&
		    ((temp_adapter->device_mode == QDF_STA_MODE) ||
		     (temp_adapter->device_mode == QDF_P2P_CLIENT_MODE)))
			intf_count++;

		hdd_adapter_dev_put_debug(temp_adapter, dbgid);
	}

	if (intf_count >= CFG_TGT_DEFAULT_MAX_STA_VDEVS) {
		hdd_err("Max limit reached sta vdev-current %d max %d",
			intf_count, CFG_TGT_DEFAULT_MAX_STA_VDEVS);
		return true;
	}
	return false;
}

#if defined(WLAN_FEATURE_11BE_MLO) && \
(defined(CFG80211_IFTYPE_MLO_LINK_SUPPORT) || \
defined(CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT))
static int hdd_start_link_adapter(struct hdd_adapter *sta_adapter)
{
	int i, ret = 0;
	struct hdd_mlo_adapter_info *mlo_adapter_info;
	struct hdd_adapter *link_adapter;

	hdd_enter_dev(sta_adapter->dev);
	mlo_adapter_info = &sta_adapter->mlo_adapter_info;

	for (i = 0; i < WLAN_MAX_MLD; i++) {
		link_adapter = mlo_adapter_info->link_adapter[i];
		if (!link_adapter)
			continue;
		if (link_adapter->mlo_adapter_info.associate_with_ml_adapter) {
			/* TODO have proper references here */
			qdf_spin_lock_bh(&link_adapter->vdev_lock);
			link_adapter->vdev = sta_adapter->vdev;
			link_adapter->vdev_id = sta_adapter->vdev_id;
			qdf_spin_unlock_bh(&link_adapter->vdev_lock);
			continue;
		}
		ret = hdd_start_station_adapter(link_adapter);
	}

	hdd_exit();
	return ret;
}

static int hdd_stop_link_adapter(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *sta_adapter)
{
	int i, ret = 0;
	struct hdd_mlo_adapter_info *mlo_adapter_info;
	struct hdd_adapter *link_adapter;

	hdd_enter_dev(sta_adapter->dev);
	hdd_debug("Stop adapter for link mode : %s(%d)",
		  qdf_opmode_str(sta_adapter->device_mode),
		  sta_adapter->vdev_id);

	mlo_adapter_info = &sta_adapter->mlo_adapter_info;
	for (i = 0; i < WLAN_MAX_MLD; i++) {
		link_adapter = mlo_adapter_info->link_adapter[i];
		if (!link_adapter)
			continue;

		if (link_adapter->mlo_adapter_info.associate_with_ml_adapter) {
			/* TODO have proper references here */
			qdf_spin_lock_bh(&link_adapter->vdev_lock);
			link_adapter->vdev = NULL;
			link_adapter->vdev_id = 0xff;
			qdf_spin_unlock_bh(&link_adapter->vdev_lock);
			continue;
		}
		ret = hdd_stop_adapter_ext(hdd_ctx, link_adapter);
	}

	hdd_exit();
	return ret;
}
#else
static int hdd_start_link_adapter(struct hdd_adapter *link_adapter)
{
	return 0;
}

static int hdd_stop_link_adapter(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *link_adapter)
{
	return 0;
}
#endif

/**
 * hdd_start_adapter() - Wrapper function for device specific adapter
 * @adapter: pointer to HDD adapter
 *
 * This function is called to start the device specific adapter for
 * the mode passed in the adapter's device_mode.
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_start_adapter(struct hdd_adapter *adapter)
{

	int ret;
	enum QDF_OPMODE device_mode = adapter->device_mode;

	hdd_enter_dev(adapter->dev);

	switch (device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
		if (hdd_max_sta_interface_up_count_reached(adapter))
			goto err_start_adapter;

		fallthrough;
	case QDF_P2P_DEVICE_MODE:
	case QDF_OCB_MODE:
	case QDF_MONITOR_MODE:
	case QDF_NAN_DISC_MODE:
		ret = hdd_start_station_adapter(adapter);
		if (ret)
			goto err_start_adapter;

		if (device_mode == QDF_STA_MODE) {
			ret = hdd_start_link_adapter(adapter);
			if (ret)
				hdd_err("Failed to start link adapter:%d", ret);
		}
		break;
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
		ret = hdd_start_ap_adapter(adapter);
		if (ret)
			goto err_start_adapter;
		break;
	case QDF_FTM_MODE:
		/* vdevs are dynamically managed by firmware in FTM */
		hdd_register_wext(adapter->dev);
		goto exit_with_success;
	default:
		hdd_err("Invalid session type %d", device_mode);
		QDF_ASSERT(0);
		goto err_start_adapter;
	}

	if (hdd_set_fw_params(adapter))
		hdd_err("Failed to set the FW params for the adapter!");

	if (adapter->vdev_id != WLAN_UMAC_VDEV_ID_MAX) {
		ret = wlan_hdd_cfg80211_register_frames(adapter);
		if (ret < 0) {
			hdd_err("Failed to register frames - ret %d", ret);
			goto err_start_adapter;
		}
	}

	wlan_hdd_update_dbs_scan_and_fw_mode_config();

exit_with_success:
	hdd_create_adapter_sysfs_files(adapter);

	hdd_exit();

	return 0;

err_start_adapter:
	return -EINVAL;
}

void hdd_update_hw_sw_info(struct hdd_context *hdd_ctx)
{
	void *hif_sc;
	size_t target_hw_name_len;
	const char *target_hw_name;
	uint8_t *buf;
	uint32_t buf_len;

	hif_sc = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_sc)
		return;

	hif_get_hw_info(hif_sc, &hdd_ctx->target_hw_version,
			&hdd_ctx->target_hw_revision,
			&target_hw_name);

	qdf_mem_zero(hdd_ctx->target_hw_name, MAX_TGT_HW_NAME_LEN);

	target_hw_name_len = strlen(target_hw_name) + 1;

	if (target_hw_name_len <= MAX_TGT_HW_NAME_LEN) {
		qdf_mem_copy(hdd_ctx->target_hw_name, target_hw_name,
			     target_hw_name_len);
	} else {
		hdd_err("target_hw_name_len is greater than MAX_TGT_HW_NAME_LEN");
		return;
	}

	hdd_debug("target_hw_name = %s", hdd_ctx->target_hw_name);

	buf = qdf_mem_malloc(WE_MAX_STR_LEN);
	if (buf) {
		buf_len = hdd_wlan_get_version(hdd_ctx, WE_MAX_STR_LEN, buf);
		hdd_nofl_debug("%s", buf);
		qdf_mem_free(buf);
	}
}

/**
 * hdd_update_cds_ac_specs_params() - update cds ac_specs params
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static void
hdd_update_cds_ac_specs_params(struct hdd_context *hdd_ctx)
{
	uint8_t tx_sched_wrr_param[TX_SCHED_WRR_PARAMS_NUM] = {0};
	qdf_size_t out_size = 0;
	int i;
	struct cds_context *cds_ctx;

	if (!hdd_ctx)
		return;

	if (!hdd_ctx->config) {
		/* Do nothing if hdd_ctx is invalid */
		hdd_err("Warning: hdd_ctx->cfg_ini is NULL");
		return;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx)
		return;

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		switch (i) {
		case QCA_WLAN_AC_BE:
			qdf_uint8_array_parse(
				cfg_get(hdd_ctx->psoc,
					CFG_DP_ENABLE_TX_SCHED_WRR_BE),
				tx_sched_wrr_param,
				sizeof(tx_sched_wrr_param),
				&out_size);
			break;
		case QCA_WLAN_AC_BK:
			qdf_uint8_array_parse(
				cfg_get(hdd_ctx->psoc,
					CFG_DP_ENABLE_TX_SCHED_WRR_BK),
				tx_sched_wrr_param,
				sizeof(tx_sched_wrr_param),
				&out_size);
			break;
		case QCA_WLAN_AC_VI:
			qdf_uint8_array_parse(
				cfg_get(hdd_ctx->psoc,
					CFG_DP_ENABLE_TX_SCHED_WRR_VI),
				tx_sched_wrr_param,
				sizeof(tx_sched_wrr_param),
				&out_size);
			break;
		case QCA_WLAN_AC_VO:
			qdf_uint8_array_parse(
				cfg_get(hdd_ctx->psoc,
					CFG_DP_ENABLE_TX_SCHED_WRR_VO),
				tx_sched_wrr_param,
				sizeof(tx_sched_wrr_param),
				&out_size);
			break;
		default:
			break;
		}

		if (out_size == TX_SCHED_WRR_PARAMS_NUM) {
			cds_ctx->ac_specs[i].wrr_skip_weight =
						tx_sched_wrr_param[0];
			cds_ctx->ac_specs[i].credit_threshold =
						tx_sched_wrr_param[1];
			cds_ctx->ac_specs[i].send_limit =
						tx_sched_wrr_param[2];
			cds_ctx->ac_specs[i].credit_reserve =
						tx_sched_wrr_param[3];
			cds_ctx->ac_specs[i].discard_weight =
						tx_sched_wrr_param[4];
		}

		out_size = 0;
	}
}

uint32_t hdd_wlan_get_version(struct hdd_context *hdd_ctx,
			      const size_t version_len, uint8_t *version)
{
	uint32_t size;
	uint8_t reg_major = 0, reg_minor = 0, bdf_major = 0, bdf_minor = 0;
	struct target_psoc_info *tgt_hdl;

	if (!hdd_ctx) {
		hdd_err("Invalid context, HDD context is null");
		return 0;
	}

	if (!version || version_len == 0) {
		hdd_err("Invalid buffer pointr or buffer len\n");
		return 0;
	}
	tgt_hdl = wlan_psoc_get_tgt_if_handle(hdd_ctx->psoc);
	if (tgt_hdl)
		target_psoc_get_version_info(tgt_hdl, &reg_major, &reg_minor,
					     &bdf_major, &bdf_minor);

	size = scnprintf(version, version_len,
			 "Host SW:%s, FW:%d.%d.%d.%d.%d.%d, HW:%s, Board ver: %x Ref design id: %x, Customer id: %x, Project id: %x, Board Data Rev: %x, REG DB: %u:%u, BDF REG DB: %u:%u",
			 QWLAN_VERSIONSTR,
			 hdd_ctx->fw_version_info.major_spid,
			 hdd_ctx->fw_version_info.minor_spid,
			 hdd_ctx->fw_version_info.siid,
			 hdd_ctx->fw_version_info.rel_id,
			 hdd_ctx->fw_version_info.crmid,
			 hdd_ctx->fw_version_info.sub_id,
			 hdd_ctx->target_hw_name,
			 hdd_ctx->hw_bd_info.bdf_version,
			 hdd_ctx->hw_bd_info.ref_design_id,
			 hdd_ctx->hw_bd_info.customer_id,
			 hdd_ctx->hw_bd_info.project_id,
			 hdd_ctx->hw_bd_info.board_data_rev,
			 reg_major, reg_minor, bdf_major, bdf_minor);

	return size;
}

int hdd_set_11ax_rate(struct hdd_adapter *adapter, int set_value,
		      struct sap_config *sap_config)
{
	uint8_t preamble = 0, nss = 0, rix = 0;
	int ret;
	mac_handle_t mac_handle = adapter->hdd_ctx->mac_handle;

	if (!sap_config) {
		if (!sme_is_feature_supported_by_fw(DOT11AX)) {
			hdd_err("Target does not support 11ax");
			return -EIO;
		}
	} else if (sap_config->SapHw_mode != eCSR_DOT11_MODE_11ax &&
		   sap_config->SapHw_mode != eCSR_DOT11_MODE_11ax_ONLY) {
		hdd_err("Invalid hw mode, SAP hw_mode= 0x%x, ch_freq = %d",
			sap_config->SapHw_mode, sap_config->chan_freq);
		return -EIO;
	}

	if (set_value != 0xffff) {
		rix = RC_2_RATE_IDX_11AX(set_value);
		preamble = WMI_RATE_PREAMBLE_HE;
		nss = HT_RC_2_STREAMS_11AX(set_value);

		set_value = hdd_assemble_rate_code(preamble, nss, rix);
	} else {
		ret = sme_set_auto_rate_he_ltf(mac_handle, adapter->vdev_id,
					       QCA_WLAN_HE_LTF_AUTO);
	}

	hdd_info("SET_11AX_RATE val %d rix %d preamble %x nss %d",
		 set_value, rix, preamble, nss);

	ret = wma_cli_set_command(adapter->vdev_id,
				  wmi_vdev_param_fixed_rate,
				  set_value, VDEV_CMD);

	return ret;
}

int hdd_assemble_rate_code(uint8_t preamble, uint8_t nss, uint8_t rate)
{
	int set_value;

	if (sme_is_feature_supported_by_fw(DOT11AX))
		set_value = WMI_ASSEMBLE_RATECODE_V1(rate, nss, preamble);
	else
		set_value = (preamble << 6) | (nss << 4) | rate;

	return set_value;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
static enum policy_mgr_con_mode wlan_hdd_get_mode_for_non_connected_vdev(
			struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("Adapter is NULL");
		return PM_MAX_NUM_OF_MODE;
	}

	return	policy_mgr_convert_device_mode_to_qdf_type(
			adapter->device_mode);
}

/**
 * hdd_is_chan_switch_in_progress() - Check if any adapter has channel switch in
 * progress
 *
 * Return: true, if any adapter has channel switch in
 * progress else false
 */
static bool hdd_is_chan_switch_in_progress(void)
{
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	wlan_net_dev_ref_dbgid dbgid =
				NET_DEV_HOLD_IS_CHAN_SWITCH_IN_PROGRESS;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if ((adapter->device_mode == QDF_SAP_MODE ||
		     adapter->device_mode == QDF_P2P_GO_MODE) &&
		    qdf_atomic_read(&adapter->ch_switch_in_progress)) {
			hdd_debug("channel switch progress for vdev_id %d",
				  adapter->vdev_id);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return true;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return false;
}

/**
 * hdd_is_cac_in_progress() - Check if any SAP connection is performing
 * CAC on DFS channel
 *
 * Return: true, if any of existing SAP is performing CAC
 * or else false
 */
static bool hdd_is_cac_in_progress(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return false;

	return (hdd_ctx->dev_dfs_cac_status == DFS_CAC_IN_PROGRESS);
}

static void hdd_register_policy_manager_callback(
			struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_hdd_cbacks hdd_cbacks;

	qdf_mem_zero(&hdd_cbacks, sizeof(hdd_cbacks));
	hdd_cbacks.sap_restart_chan_switch_cb =
		hdd_sap_restart_chan_switch_cb;
	hdd_cbacks.wlan_hdd_get_channel_for_sap_restart =
		wlan_hdd_get_channel_for_sap_restart;
	hdd_cbacks.get_mode_for_non_connected_vdev =
		wlan_hdd_get_mode_for_non_connected_vdev;
	hdd_cbacks.hdd_get_device_mode = hdd_get_device_mode;
	hdd_cbacks.hdd_is_chan_switch_in_progress =
				hdd_is_chan_switch_in_progress;
	hdd_cbacks.hdd_is_cac_in_progress =
				hdd_is_cac_in_progress;
	hdd_cbacks.wlan_hdd_set_sap_csa_reason =
				wlan_hdd_set_sap_csa_reason;
	hdd_cbacks.hdd_get_ap_6ghz_capable = hdd_get_ap_6ghz_capable;
	hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt =
				hdd_indicate_active_ndp_cnt;
	hdd_cbacks.wlan_get_ap_prefer_conc_ch_params =
			wlan_get_ap_prefer_conc_ch_params;
	hdd_cbacks.wlan_get_sap_acs_band =
			wlan_get_sap_acs_band;
	hdd_cbacks.wlan_check_cc_intf_cb = wlan_hdd_check_cc_intf_cb;

	if (QDF_STATUS_SUCCESS !=
	    policy_mgr_register_hdd_cb(psoc, &hdd_cbacks)) {
		hdd_err("HDD callback registration with policy manager failed");
	}
}
#else
static void hdd_register_policy_manager_callback(
			struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef WLAN_SUPPORT_GAP_LL_PS_MODE
static void hdd_register_green_ap_callback(struct wlan_objmgr_pdev *pdev)
{
	struct green_ap_hdd_callback hdd_cback;
	qdf_mem_zero(&hdd_cback, sizeof(hdd_cback));

	hdd_cback.send_event = wlan_hdd_send_green_ap_ll_ps_event;

	if (QDF_STATUS_SUCCESS !=
			green_ap_register_hdd_callback(pdev, &hdd_cback)) {
		hdd_err("HDD callback registration for Green AP failed");
	}
}
#else
static inline void hdd_register_green_ap_callback(struct wlan_objmgr_pdev *pdev)
{
}
#endif

#ifdef WLAN_FEATURE_NAN
#ifdef WLAN_FEATURE_SR
static void hdd_register_sr_concurrency_cb(struct nan_callbacks *cb_obj)
{
	cb_obj->nan_sr_concurrency_update = hdd_nan_sr_concurrency_update;
}
#else
static void hdd_register_sr_concurrency_cb(struct nan_callbacks *cb_obj)
{}
#endif
static void hdd_nan_register_callbacks(struct hdd_context *hdd_ctx)
{
	struct nan_callbacks cb_obj = {0};

	cb_obj.ndi_open = hdd_ndi_open;
	cb_obj.ndi_close = hdd_ndi_close;
	cb_obj.ndi_set_mode = hdd_ndi_set_mode;
	cb_obj.ndi_start = hdd_ndi_start;
	cb_obj.ndi_delete = hdd_ndi_delete;
	cb_obj.drv_ndi_create_rsp_handler = hdd_ndi_drv_ndi_create_rsp_handler;
	cb_obj.drv_ndi_delete_rsp_handler = hdd_ndi_drv_ndi_delete_rsp_handler;

	cb_obj.new_peer_ind = hdd_ndp_new_peer_handler;
	cb_obj.peer_departed_ind = hdd_ndp_peer_departed_handler;

	cb_obj.nan_concurrency_update = hdd_nan_concurrency_update;
	hdd_register_sr_concurrency_cb(&cb_obj);

	os_if_nan_register_hdd_callbacks(hdd_ctx->psoc, &cb_obj);
}
#else
static inline void hdd_nan_register_callbacks(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef CONFIG_LEAK_DETECTION
/**
 * hdd_check_for_leaks() - Perform runtime memory leak checks
 * @hdd_ctx: the global HDD context
 * @is_ssr: true if SSR is in progress
 *
 * This API triggers runtime memory leak detection. This feature enforces the
 * policy that any memory allocated at runtime must also be released at runtime.
 *
 * Allocating memory at runtime and releasing it at unload is effectively a
 * memory leak for configurations which never unload (e.g. LONU, statically
 * compiled driver). Such memory leaks are NOT false positives, and must be
 * fixed.
 *
 * Return: None
 */
static void hdd_check_for_leaks(struct hdd_context *hdd_ctx, bool is_ssr)
{
	/* DO NOT REMOVE these checks; for false positives, read above first */

	wlan_objmgr_psoc_check_for_leaks(hdd_ctx->psoc);

	/* many adapter resources are not freed by design during SSR */
	if (is_ssr)
		return;

	qdf_wake_lock_check_for_leaks();
	qdf_delayed_work_check_for_leaks();
	qdf_mc_timer_check_for_leaks();
	qdf_nbuf_map_check_for_leaks();
	qdf_periodic_work_check_for_leaks();
	qdf_mem_check_for_leaks();
}

#define hdd_debug_domain_set(domain) qdf_debug_domain_set(domain)
#else
static void hdd_check_for_objmgr_peer_leaks(struct wlan_objmgr_psoc *psoc)
{
	uint32_t vdev_id;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;

	/* get module id which cause the leak and release ref */
	wlan_objmgr_for_each_psoc_vdev(psoc, vdev_id, vdev) {
		wlan_vdev_obj_lock(vdev);
		wlan_objmgr_for_each_vdev_peer(vdev, peer) {
			qdf_atomic_t *ref_id_dbg;
			int ref_id;
			int32_t refs;

			ref_id_dbg = vdev->vdev_objmgr.ref_id_dbg;
			wlan_objmgr_for_each_refs(ref_id_dbg, ref_id, refs)
				wlan_objmgr_peer_release_ref(peer, ref_id);
		}
		wlan_vdev_obj_unlock(vdev);
	}
}

static void hdd_check_for_objmgr_leaks(struct hdd_context *hdd_ctx)
{
	uint32_t vdev_id, pdev_id;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	/*
	 * leak detection is disabled, force release the references for the wlan
	 * to recover cleanly.
	 */
	psoc = hdd_ctx->psoc;
	if (!psoc)
		return;

	wlan_psoc_obj_lock(psoc);

	hdd_check_for_objmgr_peer_leaks(psoc);

	wlan_objmgr_for_each_psoc_vdev(psoc, vdev_id, vdev) {
		qdf_atomic_t *ref_id_dbg;
		int ref_id;
		int32_t refs;

		ref_id_dbg = vdev->vdev_objmgr.ref_id_dbg;
		wlan_objmgr_for_each_refs(ref_id_dbg, ref_id, refs) {
			wlan_objmgr_vdev_release_ref(vdev, ref_id);
		}
	}

	wlan_objmgr_for_each_psoc_pdev(psoc, pdev_id, pdev) {
		qdf_atomic_t *ref_id_dbg;
		int ref_id;
		int32_t refs;

		ref_id_dbg = pdev->pdev_objmgr.ref_id_dbg;
		wlan_objmgr_for_each_refs(ref_id_dbg, ref_id, refs)
			wlan_objmgr_pdev_release_ref(pdev, ref_id);
	}
	wlan_psoc_obj_unlock(psoc);
}

static void hdd_check_for_leaks(struct hdd_context *hdd_ctx, bool is_ssr)
{
	hdd_check_for_objmgr_leaks(hdd_ctx);
}
#define hdd_debug_domain_set(domain)
#endif /* CONFIG_LEAK_DETECTION */

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
/**
 * hdd_skip_acs_scan_timer_handler() - skip ACS scan timer timeout handler
 * @data: pointer to struct hdd_context
 *
 * This function will reset acs_scan_status to eSAP_DO_NEW_ACS_SCAN.
 * Then new ACS request will do a fresh scan without reusing the cached
 * scan information.
 *
 * Return: void
 */
static void hdd_skip_acs_scan_timer_handler(void *data)
{
	struct hdd_context *hdd_ctx = data;
	mac_handle_t mac_handle;

	hdd_debug("ACS Scan result expired. Reset ACS scan skip");
	hdd_ctx->skip_acs_scan_status = eSAP_DO_NEW_ACS_SCAN;
	qdf_spin_lock(&hdd_ctx->acs_skip_lock);
	qdf_mem_free(hdd_ctx->last_acs_freq_list);
	hdd_ctx->last_acs_freq_list = NULL;
	hdd_ctx->num_of_channels = 0;
	qdf_spin_unlock(&hdd_ctx->acs_skip_lock);

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle)
		return;
}

static void hdd_skip_acs_scan_timer_init(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	status = qdf_mc_timer_init(&hdd_ctx->skip_acs_scan_timer,
				   QDF_TIMER_TYPE_SW,
				   hdd_skip_acs_scan_timer_handler,
				   hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to init ACS Skip timer");
	qdf_spinlock_create(&hdd_ctx->acs_skip_lock);
}

static void hdd_skip_acs_scan_timer_deinit(struct hdd_context *hdd_ctx)
{
	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&hdd_ctx->skip_acs_scan_timer)) {
		qdf_mc_timer_stop(&hdd_ctx->skip_acs_scan_timer);
	}

	if (!QDF_IS_STATUS_SUCCESS
		    (qdf_mc_timer_destroy(&hdd_ctx->skip_acs_scan_timer))) {
		hdd_err("Cannot deallocate ACS Skip timer");
	}
	qdf_spin_lock(&hdd_ctx->acs_skip_lock);
	qdf_mem_free(hdd_ctx->last_acs_freq_list);
	hdd_ctx->last_acs_freq_list = NULL;
	hdd_ctx->num_of_channels = 0;
	qdf_spin_unlock(&hdd_ctx->acs_skip_lock);
}
#else
static void hdd_skip_acs_scan_timer_init(struct hdd_context *hdd_ctx) {}
static void hdd_skip_acs_scan_timer_deinit(struct hdd_context *hdd_ctx) {}
#endif

/**
 * hdd_update_country_code - Update country code
 * @hdd_ctx: HDD context
 *
 * Update country code based on module parameter country_code
 *
 * Return: 0 on success and errno on failure
 */
int hdd_update_country_code(struct hdd_context *hdd_ctx)
{
	if (!country_code ||
	    !ucfg_reg_is_user_country_set_allowed(hdd_ctx->psoc))
		return 0;

	return hdd_reg_set_country(hdd_ctx, country_code);
}

#ifdef WLAN_NS_OFFLOAD
/**
 * hdd_wlan_unregister_ip6_notifier() - unregister IPv6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Unregister for IPv6 address change notifications.
 *
 * Return: None
 */
static void hdd_wlan_unregister_ip6_notifier(struct hdd_context *hdd_ctx)
{
	unregister_inet6addr_notifier(&hdd_ctx->ipv6_notifier);
}

/**
 * hdd_wlan_register_ip6_notifier() - register IPv6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Register for IPv6 address change notifications.
 *
 * Return: 0 on success and errno on failure.
 */
static int hdd_wlan_register_ip6_notifier(struct hdd_context *hdd_ctx)
{
	int ret;

	hdd_ctx->ipv6_notifier.notifier_call = wlan_hdd_ipv6_changed;
	ret = register_inet6addr_notifier(&hdd_ctx->ipv6_notifier);
	if (ret) {
		hdd_err("Failed to register IPv6 notifier: %d", ret);
		goto out;
	}

	hdd_debug("Registered IPv6 notifier");
out:
	return ret;
}
#else
/**
 * hdd_wlan_unregister_ip6_notifier() - unregister IPv6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Unregister for IPv6 address change notifications.
 *
 * Return: None
 */
static void hdd_wlan_unregister_ip6_notifier(struct hdd_context *hdd_ctx)
{
}

/**
 * hdd_wlan_register_ip6_notifier() - register IPv6 change notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Register for IPv6 address change notifications.
 *
 * Return: None
 */
static int hdd_wlan_register_ip6_notifier(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

#ifdef FEATURE_RUNTIME_PM
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
static int hdd_pm_qos_add_notifier(struct hdd_context *hdd_ctx)
{
	return dev_pm_qos_add_notifier(hdd_ctx->parent_dev,
				       &hdd_ctx->pm_qos_notifier,
				       DEV_PM_QOS_RESUME_LATENCY);
}

static int hdd_pm_qos_remove_notifier(struct hdd_context *hdd_ctx)
{
	return dev_pm_qos_remove_notifier(hdd_ctx->parent_dev,
					  &hdd_ctx->pm_qos_notifier,
					  DEV_PM_QOS_RESUME_LATENCY);
}
#else
static int hdd_pm_qos_add_notifier(struct hdd_context *hdd_ctx)
{
	return pm_qos_add_notifier(PM_QOS_CPU_DMA_LATENCY,
				   &hdd_ctx->pm_qos_notifier);
}

static int hdd_pm_qos_remove_notifier(struct hdd_context *hdd_ctx)
{
	return pm_qos_remove_notifier(PM_QOS_CPU_DMA_LATENCY,
				      &hdd_ctx->pm_qos_notifier);
}
#endif

/**
 * hdd_wlan_register_pm_qos_notifier() - register PM QOS notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Register for PM QOS change notifications.
 *
 * Return: None
 */
static int hdd_wlan_register_pm_qos_notifier(struct hdd_context *hdd_ctx)
{
	int ret;

	qdf_spinlock_create(&hdd_ctx->pm_qos_lock);

	/* if gRuntimePM is 1 then feature is enabled without CXPC */
	if (hdd_ctx->config->runtime_pm != hdd_runtime_pm_dynamic) {
		hdd_debug("Dynamic Runtime PM disabled");
		return 0;
	}

	hdd_ctx->pm_qos_notifier.notifier_call = wlan_hdd_pm_qos_notify;
	ret = hdd_pm_qos_add_notifier(hdd_ctx);
	if (ret)
		hdd_err("Failed to register PM_QOS notifier: %d", ret);
	else
		hdd_debug("PM QOS Notifier registered");

	return ret;
}

/**
 * hdd_wlan_unregister_pm_qos_notifier() - unregister PM QOS notifier
 * @hdd_ctx: Pointer to hdd context
 *
 * Unregister for PM QOS change notifications.
 *
 * Return: None
 */
static void hdd_wlan_unregister_pm_qos_notifier(struct hdd_context *hdd_ctx)
{
	int ret;

	if (hdd_ctx->config->runtime_pm != hdd_runtime_pm_dynamic) {
		hdd_debug("Dynamic Runtime PM disabled");
		qdf_spinlock_destroy(&hdd_ctx->pm_qos_lock);
		return;
	}

	ret = hdd_pm_qos_remove_notifier(hdd_ctx);
	if (ret)
		hdd_warn("Failed to remove qos notifier, err = %d\n", ret);

	qdf_spin_lock_irqsave(&hdd_ctx->pm_qos_lock);

	if (hdd_ctx->runtime_pm_prevented) {
		hif_rtpm_put(HIF_RTPM_PUT_NOIDLE, HIF_RTPM_ID_PM_QOS_NOTIFY);
		hdd_ctx->runtime_pm_prevented = false;
	}

	qdf_spin_unlock_irqrestore(&hdd_ctx->pm_qos_lock);

	qdf_spinlock_destroy(&hdd_ctx->pm_qos_lock);
}
#else
static int hdd_wlan_register_pm_qos_notifier(struct hdd_context *hdd_ctx)
{
	return 0;
}

static void hdd_wlan_unregister_pm_qos_notifier(struct hdd_context *hdd_ctx)
{
}
#endif

/**
 * hdd_enable_power_management() - API to Enable Power Management
 * @hdd_ctx: HDD context
 *
 * API invokes Bus Interface Layer power management functionality
 *
 * Return: None
 */
static void hdd_enable_power_management(struct hdd_context *hdd_ctx)
{
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);

	if (!hif_ctx)
		return;

	hif_enable_power_management(hif_ctx, cds_is_packet_log_enabled());
	hdd_wlan_register_pm_qos_notifier(hdd_ctx);
}

/**
 * hdd_disable_power_management() - API to disable Power Management
 * @hdd_ctx: HDD context
 *
 * API disable Bus Interface Layer Power management functionality
 *
 * Return: None
 */
static void hdd_disable_power_management(struct hdd_context *hdd_ctx)
{
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);

	if (!hif_ctx)
		return;

	hdd_wlan_unregister_pm_qos_notifier(hdd_ctx);
	hif_disable_power_management(hif_ctx);
}

/**
 * hdd_register_notifiers - Register netdev notifiers.
 * @hdd_ctx: HDD context
 *
 * Register netdev notifiers like IPv4 and IPv6.
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_register_notifiers(struct hdd_context *hdd_ctx)
{
	int ret;

	ret = hdd_wlan_register_ip6_notifier(hdd_ctx);
	if (ret)
		goto out;

	hdd_ctx->ipv4_notifier.notifier_call = wlan_hdd_ipv4_changed;
	ret = register_inetaddr_notifier(&hdd_ctx->ipv4_notifier);
	if (ret) {
		hdd_err("Failed to register IPv4 notifier: %d", ret);
		goto unregister_ip6_notifier;
	}

	ret = osif_dp_nud_register_netevent_notifier(hdd_ctx->psoc);
	if (ret) {
		hdd_err("Failed to register netevent notifier: %d",
			ret);
		goto unregister_inetaddr_notifier;
	}

	return 0;

unregister_inetaddr_notifier:
	unregister_inetaddr_notifier(&hdd_ctx->ipv4_notifier);
unregister_ip6_notifier:
	hdd_wlan_unregister_ip6_notifier(hdd_ctx);
out:
	return ret;
}

#ifdef WLAN_FEATURE_WMI_SEND_RECV_QMI
static inline
void hdd_set_qmi_stats_enabled(struct hdd_context *hdd_ctx)
{
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(hdd_ctx->psoc);

	if (!wmi_handle) {
		hdd_err("could not get wmi handle");
		return;
	}

	wmi_set_qmi_stats(wmi_handle, hdd_ctx->config->is_qmi_stats_enabled);
}
#else
static inline
void hdd_set_qmi_stats_enabled(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef CONFIG_FW_LOGS_BASED_ON_INI
/**
 * hdd_set_fw_log_params() - Set log parameters to FW
 * @hdd_ctx: HDD Context
 * @vdev_id: vdev_id
 *
 * This function set the FW Debug log level based on the INI.
 *
 * Return: None
 */
static void hdd_set_fw_log_params(struct hdd_context *hdd_ctx,
				  uint8_t vdev_id)
{
	QDF_STATUS status;
	uint16_t enable_fw_log_level, enable_fw_log_type;
	int ret;

	if (!hdd_ctx->config->enable_fw_log) {
		hdd_debug("enable_fw_log not enabled in INI");
		return;
	}

	/* Enable FW logs based on INI configuration */
	status = ucfg_fwol_get_enable_fw_log_type(hdd_ctx->psoc,
						  &enable_fw_log_type);
	if (QDF_IS_STATUS_ERROR(status))
		return;
	ret = sme_cli_set_command(vdev_id, WMI_DBGLOG_TYPE,
				  enable_fw_log_type, DBG_CMD);
	if (ret != 0)
		hdd_err("Failed to enable FW log type ret %d", ret);

	status = ucfg_fwol_get_enable_fw_log_level(hdd_ctx->psoc,
						   &enable_fw_log_level);
	if (QDF_IS_STATUS_ERROR(status))
		return;
	ret = sme_cli_set_command(vdev_id, WMI_DBGLOG_LOG_LEVEL,
				  enable_fw_log_level, DBG_CMD);
	if (ret != 0)
		hdd_err("Failed to enable FW log level ret %d", ret);

	sme_enable_fw_module_log_level(hdd_ctx->mac_handle, vdev_id);
}
#else
static void hdd_set_fw_log_params(struct hdd_context *hdd_ctx, uint8_t vdev_id)
{
}

#endif

/**
 * hdd_features_deinit() - Deinit features
 * @hdd_ctx:	HDD context
 *
 * De-Initialize features and their feature context.
 *
 * Return: none.
 */
static void hdd_features_deinit(struct hdd_context *hdd_ctx)
{
	wlan_hdd_gpio_wakeup_deinit(hdd_ctx);
	wlan_hdd_twt_deinit(hdd_ctx);
	wlan_hdd_deinit_chan_info(hdd_ctx);
	wlan_hdd_tsf_deinit(hdd_ctx);
	if (cds_is_packet_log_enabled())
		hdd_pktlog_enable_disable(hdd_ctx, false, 0, 0);
}

/**
 * hdd_deconfigure_cds() -De-Configure cds
 * @hdd_ctx:	HDD context
 *
 * Deconfigure Cds modules before WLAN firmware is down.
 *
 * Return: 0 on success and errno on failure.
 */
static int hdd_deconfigure_cds(struct hdd_context *hdd_ctx)
{
	QDF_STATUS qdf_status;
	int ret = 0;

	hdd_enter();

	wlan_hdd_hang_event_notifier_unregister();
	/* De-init features */
	hdd_features_deinit(hdd_ctx);

	qdf_status = policy_mgr_deregister_mode_change_cb(hdd_ctx->psoc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		hdd_debug("Failed to deregister mode change cb with Policy Manager");

	qdf_status = cds_disable(hdd_ctx->psoc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Failed to Disable the CDS Modules! :%d",
			qdf_status);
		ret = -EINVAL;
	}

	if (ucfg_ipa_uc_ol_deinit(hdd_ctx->pdev) != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to disconnect pipes");
		ret = -EINVAL;
	}

	hdd_exit();
	return ret;
}

/**
 * hdd_qmi_register_callbacks() - Register QMI callbacks
 * @hdd_ctx: HDD context
 *
 * Return: None
 */
static inline void hdd_qmi_register_callbacks(struct hdd_context *hdd_ctx)
{
	struct wlan_qmi_psoc_callbacks cb_obj;

	os_if_qmi_register_callbacks(hdd_ctx->psoc, &cb_obj);
}

static int hdd_set_pcie_params(struct hdd_context *hdd_ctx)
{
	int vdev_id = 0;
	int param_id = WMI_PDEV_PARAM_PCIE_CONFIG;
	bool value;
	QDF_STATUS status;
	int vpdev = PDEV_CMD;
	int ret;

	status = ucfg_fwol_get_pcie_config(hdd_ctx->psoc, &value);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	ret = sme_cli_set_command(vdev_id, param_id, (int)value, vpdev);
	if (ret)
		hdd_err("WMI_PDEV_PARAM_PCIE_CONFIG failed %d", ret);

	return ret;
}

int hdd_wlan_start_modules(struct hdd_context *hdd_ctx, bool reinit)
{
	int ret = 0;
	qdf_device_t qdf_dev;
	QDF_STATUS status;
	bool unint = false;
	void *hif_ctx;
	struct target_psoc_info *tgt_hdl;
	unsigned long thermal_state = 0;
	bool is_sched_disabled = false;

	hdd_enter();
	qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_dev) {
		hdd_exit();
		return -EINVAL;
	}

	hdd_psoc_idle_timer_stop(hdd_ctx);

	if (hdd_ctx->driver_status == DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver modules already Enabled");
		hdd_exit();
		return 0;
	}

	cds_set_driver_state_module_stop(false);

	switch (hdd_ctx->driver_status) {
	case DRIVER_MODULES_UNINITIALIZED:
		hdd_nofl_debug("Wlan transitioning (UNINITIALIZED -> CLOSED)");
		unint = true;
		fallthrough;
	case DRIVER_MODULES_CLOSED:
		hdd_nofl_debug("Wlan transitioning (CLOSED -> ENABLED)");

		hdd_debug_domain_set(QDF_DEBUG_DOMAIN_ACTIVE);

		if (!reinit && !unint) {
			ret = pld_power_on(qdf_dev->dev);
			if (ret) {
				hdd_err("Failed to power up device; errno:%d",
					ret);
				goto release_lock;
			}
		}

		hdd_init_adapter_ops_wq(hdd_ctx);
		pld_set_fw_log_mode(hdd_ctx->parent_dev,
				    hdd_ctx->config->enable_fw_log);
		ret = hdd_hif_open(qdf_dev->dev, qdf_dev->drv_hdl, qdf_dev->bid,
				   qdf_dev->bus_type,
				   (reinit == true) ?  HIF_ENABLE_TYPE_REINIT :
				   HIF_ENABLE_TYPE_PROBE);
		if (ret) {
			hdd_err("Failed to open hif; errno: %d", ret);
			goto power_down;
		}

		hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
		if (!hif_ctx) {
			ret = -EINVAL;
			goto power_down;
		}

		status = ol_cds_init(qdf_dev, hif_ctx);
		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("No Memory to Create BMI Context; status: %d",
				status);
			ret = qdf_status_to_os_return(status);
			goto hif_close;
		}

		if (hdd_get_conparam() == QDF_GLOBAL_EPPING_MODE) {
			status = epping_open();
			if (status) {
				hdd_err("Failed to open in epping mode: %d",
					status);
				ret = -EINVAL;
				goto cds_free;
			}

			status = epping_enable(qdf_dev->dev, false);
			if (status) {
				hdd_err("Failed to enable in epping mode : %d",
					status);
				epping_close();
				goto cds_free;
			}

			hdd_info("epping mode enabled");
			break;
		}

		if (pld_is_ipa_offload_disabled(qdf_dev->dev))
			ucfg_ipa_set_pld_enable(false);

		ucfg_ipa_component_config_update(hdd_ctx->psoc);

		hdd_update_cds_ac_specs_params(hdd_ctx);

		hdd_dp_register_callbacks(hdd_ctx);

		hdd_qmi_register_callbacks(hdd_ctx);

		status = hdd_component_psoc_open(hdd_ctx->psoc);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to Open legacy components; status: %d",
				status);
			ret = qdf_status_to_os_return(status);
			goto ipa_component_free;
		}

		ret = hdd_update_config(hdd_ctx);
		if (ret) {
			hdd_err("Failed to update configuration; errno: %d",
				ret);
			goto ipa_component_free;
		}

		status = wbuff_module_init();
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("WBUFF init unsuccessful; status: %d", status);

		status = cds_open(hdd_ctx->psoc);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to Open CDS; status: %d", status);
			ret = qdf_status_to_os_return(status);
			goto psoc_close;
		}

		hdd_set_qmi_stats_enabled(hdd_ctx);

		hdd_ctx->mac_handle = cds_get_context(QDF_MODULE_ID_SME);

		ucfg_dp_set_rx_thread_affinity(hdd_ctx->psoc);

		/* initialize components configurations after psoc open */
		ret = hdd_update_components_config(hdd_ctx);
		if (ret) {
			hdd_err("Failed to update component configs; errno: %d",
				ret);
			goto close;
		}

		/* Override PS params for monitor mode */
		if (hdd_get_conparam() == QDF_GLOBAL_MONITOR_MODE)
			hdd_override_all_ps(hdd_ctx);

		status = cds_dp_open(hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("Failed to Open cds post open; status: %d",
				status);
			ret = qdf_status_to_os_return(status);
			goto close;
		}
		/* Set IRQ affinity for WLAN DP and CE IRQS */
		hif_config_irq_set_perf_affinity_hint(hif_ctx);

		ret = hdd_register_cb(hdd_ctx);
		if (ret) {
			hdd_err("Failed to register HDD callbacks!");
			goto cds_txrx_free;
		}

		ret = hdd_register_notifiers(hdd_ctx);
		if (ret)
			goto deregister_cb;

		/*
		 * NAN compoenet requires certain operations like, open adapter,
		 * close adapter, etc. to be initiated by HDD, for those
		 * register HDD callbacks with UMAC's NAN componenet.
		 */
		hdd_nan_register_callbacks(hdd_ctx);

		hdd_son_register_callbacks(hdd_ctx);

		hdd_sr_register_callbacks(hdd_ctx);

		wlan_hdd_register_btc_chain_mode_handler(hdd_ctx->psoc);

		status = cds_pre_enable();
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("Failed to pre-enable CDS; status: %d", status);
			ret = qdf_status_to_os_return(status);
			goto unregister_notifiers;
		}

		hdd_register_policy_manager_callback(
			hdd_ctx->psoc);

		/*
		 * Call this function before hdd_enable_power_management. Since
		 * it is required to trigger WMI_PDEV_DMA_RING_CFG_REQ_CMDID
		 * to FW when power save isn't enable.
		 */
		hdd_spectral_register_to_dbr(hdd_ctx);

		hdd_create_sysfs_files(hdd_ctx);
		hdd_update_hw_sw_info(hdd_ctx);

		if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
			hdd_enable_power_management(hdd_ctx);
			hdd_err("in ftm mode, no need to configure cds modules");
			hdd_info("Enable FW log in ftm mode");
			/*
			 * Since vdev is not created for FTM mode,
			 * in FW use vdev_id = 0.
			 */
			hdd_set_fw_log_params(hdd_ctx, 0);
			ret = hdd_set_pcie_params(hdd_ctx);
			if (ret)
				return ret;
			ret = -EINVAL;
			break;
		}

		ret = hdd_configure_cds(hdd_ctx);
		if (ret) {
			hdd_err("Failed to Enable cds modules; errno: %d", ret);
			goto sched_disable;
		}

		if (hdd_get_conparam() == QDF_GLOBAL_MISSION_MODE) {
			status = ucfg_dp_direct_link_init(hdd_ctx->psoc);
			if (QDF_IS_STATUS_ERROR(status)) {
				cds_err("Failed to initialize Direct Link datapath");
				ret = -EINVAL;
				goto deconfigure_cds;
			}
		}

		hdd_enable_power_management(hdd_ctx);

		hdd_skip_acs_scan_timer_init(hdd_ctx);

		hdd_set_hif_init_phase(hif_ctx, false);
		hdd_hif_set_enable_detection(hif_ctx, true);

		wlan_hdd_start_connectivity_logging(hdd_ctx);

		break;

	default:
		QDF_DEBUG_PANIC("Unknown driver state:%d",
				hdd_ctx->driver_status);
		ret = -EINVAL;
		goto release_lock;
	}

	hdd_ctx->driver_status = DRIVER_MODULES_ENABLED;
	hdd_nofl_debug("Wlan transitioned (now ENABLED)");

	ucfg_ipa_reg_is_driver_unloading_cb(hdd_ctx->pdev,
					    cds_is_driver_unloading);
	ucfg_ipa_reg_sap_xmit_cb(hdd_ctx->pdev,
				 hdd_softap_ipa_start_xmit);
	ucfg_ipa_reg_send_to_nw_cb(hdd_ctx->pdev,
				   hdd_ipa_send_nbuf_to_network);
	ucfg_dp_reg_ipa_rsp_ind(hdd_ctx->pdev);

	if (!pld_get_thermal_state(hdd_ctx->parent_dev, &thermal_state,
				   THERMAL_MONITOR_APPS)) {
		if (thermal_state > QCA_WLAN_VENDOR_THERMAL_LEVEL_NONE)
			hdd_send_thermal_mitigation_val(hdd_ctx,
							thermal_state,
							THERMAL_MONITOR_APPS);
	}

	if (!pld_get_thermal_state(hdd_ctx->parent_dev, &thermal_state,
				   THERMAL_MONITOR_WPSS)) {
		if (thermal_state > QCA_WLAN_VENDOR_THERMAL_LEVEL_NONE)
			hdd_send_thermal_mitigation_val(hdd_ctx, thermal_state,
							THERMAL_MONITOR_WPSS);
	}

	hdd_exit();

	return 0;
/*
 * Disable scheduler 1st so that scheduler thread doesn't send messages to fw
 * in parallel to the cleanup
 */
deconfigure_cds:
	is_sched_disabled = !dispatcher_disable();
	hdd_deconfigure_cds(hdd_ctx);
sched_disable:
	if (!is_sched_disabled)
		dispatcher_disable();
	hdd_destroy_sysfs_files();
	cds_post_disable();
unregister_notifiers:
	hdd_unregister_notifiers(hdd_ctx);

deregister_cb:
	hdd_deregister_cb(hdd_ctx);

cds_txrx_free:

	tgt_hdl = wlan_psoc_get_tgt_if_handle(hdd_ctx->psoc);

	if (tgt_hdl && target_psoc_get_wmi_ready(tgt_hdl))
		hdd_runtime_suspend_context_deinit(hdd_ctx);

	if (hdd_ctx->pdev) {
		dispatcher_pdev_close(hdd_ctx->pdev);
		hdd_objmgr_release_and_destroy_pdev(hdd_ctx);
	}

	cds_dp_close(hdd_ctx->psoc);

close:
	hdd_ctx->driver_status = DRIVER_MODULES_CLOSED;
	hdd_info("Wlan transition aborted (now CLOSED)");

	cds_close(hdd_ctx->psoc);

psoc_close:
	hdd_component_psoc_close(hdd_ctx->psoc);
	wlan_global_lmac_if_close(hdd_ctx->psoc);
	cds_deinit_ini_config();

ipa_component_free:
	ucfg_ipa_component_config_free();

cds_free:
	ol_cds_free();

hif_close:
	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	hdd_hif_close(hdd_ctx, hif_ctx);
power_down:
	hdd_deinit_adapter_ops_wq(hdd_ctx);
	if (!reinit && !unint)
		pld_power_off(qdf_dev->dev);
release_lock:
	cds_shutdown_notifier_purge();
	hdd_check_for_leaks(hdd_ctx, reinit);
	hdd_debug_domain_set(QDF_DEBUG_DOMAIN_INIT);
	cds_set_driver_state_module_stop(true);

	hdd_exit();

	return ret;
}

#ifdef WIFI_POS_CONVERGED
static int hdd_activate_wifi_pos(struct hdd_context *hdd_ctx)
{
	int ret = os_if_wifi_pos_register_nl();

	if (ret)
		hdd_err("os_if_wifi_pos_register_nl failed");

	return ret;
}

static int hdd_deactivate_wifi_pos(void)
{
	int ret = os_if_wifi_pos_deregister_nl();

	if (ret)
		hdd_err("os_if_wifi_pos_deregister_nl failed");

	return ret;
}

/**
 * hdd_populate_wifi_pos_cfg - populates wifi_pos parameters
 * @hdd_ctx: hdd context
 *
 * Return: status of operation
 */
static void hdd_populate_wifi_pos_cfg(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	uint16_t neighbor_scan_max_chan_time;
	uint16_t neighbor_scan_min_chan_time;

	wifi_pos_set_oem_target_type(psoc, hdd_ctx->target_type);
	wifi_pos_set_oem_fw_version(psoc, hdd_ctx->target_fw_version);
	wifi_pos_set_drv_ver_major(psoc, QWLAN_VERSION_MAJOR);
	wifi_pos_set_drv_ver_minor(psoc, QWLAN_VERSION_MINOR);
	wifi_pos_set_drv_ver_patch(psoc, QWLAN_VERSION_PATCH);
	wifi_pos_set_drv_ver_build(psoc, QWLAN_VERSION_BUILD);
	ucfg_mlme_get_neighbor_scan_max_chan_time(psoc,
						  &neighbor_scan_max_chan_time);
	ucfg_mlme_get_neighbor_scan_min_chan_time(psoc,
						  &neighbor_scan_min_chan_time);
	wifi_pos_set_dwell_time_min(psoc, neighbor_scan_min_chan_time);
	wifi_pos_set_dwell_time_max(psoc, neighbor_scan_max_chan_time);
}
#else
static int hdd_activate_wifi_pos(struct hdd_context *hdd_ctx)
{
	return oem_activate_service(hdd_ctx);
}

static int hdd_deactivate_wifi_pos(void)
{
	return oem_deactivate_service();
}

static void hdd_populate_wifi_pos_cfg(struct hdd_context *hdd_ctx)
{
}
#endif

/**
 * __hdd_open() - HDD Open function
 * @dev:	Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int __hdd_open(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;

	hdd_enter_dev(dev);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_OPEN_REQUEST,
		   adapter->vdev_id, adapter->device_mode);

	/* Nothing to be done if device is unloading */
	if (cds_is_driver_unloading()) {
		hdd_err("Driver is unloading can not open the hdd");
		return -EBUSY;
	}

	if (cds_is_driver_recovering()) {
		hdd_err("WLAN is currently recovering; Please try again.");
		return -EBUSY;
	}

	/*
	 * This scenario can be hit in cases where in the wlan driver after
	 * registering the netdevices and there is a failure in driver
	 * initialization. So return error gracefully because the netdevices
	 * will be de-registered as part of the load failure.
	 */

	if (!cds_is_driver_loaded()) {
		hdd_err("Failed to start the wlan driver!!");
		return -EIO;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret) {
		hdd_err("Can't start WLAN module, WiFi Disabled");
		return ret;
	}

	ret = hdd_trigger_psoc_idle_restart(hdd_ctx);
	if (ret) {
		hdd_err("Failed to start WLAN modules return");
		return ret;
	}

	if (!test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		ret = hdd_start_adapter(adapter);
		if (ret) {
			hdd_err("Failed to start adapter :%d",
				adapter->device_mode);
			return ret;
		}
	}

	set_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);
	if (hdd_cm_is_vdev_associated(adapter)) {
		hdd_debug("Enabling Tx Queues");
		/* Enable TX queues only when we are connected */
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_START_ALL_NETIF_QUEUE,
					     WLAN_CONTROL_PATH);
	}

	/* Enable carrier and transmit queues for NDI */
	if (WLAN_HDD_IS_NDI(adapter)) {
		hdd_debug("Enabling Tx Queues");
		wlan_hdd_netif_queue_control(adapter,
			WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
			WLAN_CONTROL_PATH);
	}

	hdd_populate_wifi_pos_cfg(hdd_ctx);
	hdd_lpass_notify_start(hdd_ctx, adapter);

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
				      PACKET_CAPTURE_MODE_DISABLE)
		hdd_map_monitor_interface_vdev(adapter);

	return 0;
}

/**
 * hdd_open() - Wrapper function for __hdd_open to protect it from SSR
 * @net_dev: Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int hdd_open(struct net_device *net_dev)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_trans_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_open(net_dev);
	if (!errno)
		osif_vdev_cache_command(vdev_sync, NO_COMMAND);

	osif_vdev_sync_trans_stop(vdev_sync);

	return errno;
}

int hdd_stop_no_trans(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;
	mac_handle_t mac_handle;

	hdd_enter_dev(dev);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_STOP_REQUEST,
		   adapter->vdev_id, adapter->device_mode);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	/* Nothing to be done if the interface is not opened */
	if (false == test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_err("NETDEV Interface is not OPENED");
		return -ENODEV;
	}

	mac_handle = hdd_ctx->mac_handle;

	if (!wlan_hdd_is_session_type_monitor(adapter->device_mode) &&
	    adapter->device_mode != QDF_FTM_MODE) {
		hdd_debug("Disabling Auto Power save timer");
		sme_ps_disable_auto_ps_timer(
			mac_handle,
			adapter->vdev_id);
	}

	/*
	 * Disable TX on the interface, after this hard_start_xmit() will not
	 * be called on that interface
	 */
	hdd_debug("Disabling queues, adapter device mode: %s(%d)",
		  qdf_opmode_str(adapter->device_mode), adapter->device_mode);

	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	if (adapter->device_mode == QDF_STA_MODE)
		hdd_lpass_notify_stop(hdd_ctx);

	/*
	 * NAN data interface is different in some sense. The traffic on NDI is
	 * bursty in nature and depends on the need to transfer. The service
	 * layer may down the interface after the usage and up again when
	 * required. In some sense, the NDI is expected to be available
	 * (like SAP) iface until NDI delete request is issued by the service
	 * layer. Skip BSS termination and adapter deletion for NAN Data
	 * interface (NDI).
	 */
	if (WLAN_HDD_IS_NDI(adapter))
		goto reset_iface_opened;

	/*
	 * The interface is marked as down for outside world (aka kernel)
	 * But the driver is pretty much alive inside. The driver needs to
	 * tear down the existing connection on the netdev (session)
	 * cleanup the data pipes and wait until the control plane is stabilized
	 * for this interface. The call also needs to wait until the above
	 * mentioned actions are completed before returning to the caller.
	 * Notice that hdd_stop_adapter is requested not to close the session
	 * That is intentional to be able to scan if it is a STA/P2P interface
	 */
	hdd_stop_adapter(hdd_ctx, adapter);

	/* DeInit the adapter. This ensures datapath cleanup as well */
	hdd_deinit_adapter(hdd_ctx, adapter, true);

reset_iface_opened:
	/* Make sure the interface is marked as closed */
	clear_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);
	if (!hdd_is_any_interface_open(hdd_ctx))
		hdd_psoc_idle_timer_start(hdd_ctx);
	hdd_exit();

	return 0;
}

/**
 * hdd_stop() - Wrapper function for __hdd_stop to protect it from SSR
 * @net_dev: pointer to net_device structure
 *
 * This is called in response to ifconfig down
 *
 * Return: 0 for success and error number for failure
 */
static int hdd_stop(struct net_device *net_dev)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_trans_start(net_dev, &vdev_sync);
	if (errno) {
		if (vdev_sync)
			osif_vdev_cache_command(vdev_sync, INTERFACE_DOWN);
		return errno;
	}

	errno = hdd_stop_no_trans(net_dev);

	osif_vdev_sync_trans_stop(vdev_sync);

	return errno;
}

/**
 * hdd_uninit() - HDD uninit function
 * @dev: Pointer to net_device structure
 *
 * This is called during the netdev unregister to uninitialize all data
 * associated with the device
 *
 * This function must be protected by a transition
 *
 * Return: None
 */
static void hdd_uninit(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err("Invalid magic");
		goto exit;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("NULL hdd_ctx");
		goto exit;
	}

	if (dev != adapter->dev)
		hdd_err("Invalid device reference");

	hdd_deinit_adapter(hdd_ctx, adapter, true);

	/* after uninit our adapter structure will no longer be valid */
	adapter->magic = 0;

exit:
	hdd_exit();
}

static int hdd_open_cesium_nl_sock(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
	struct netlink_kernel_cfg cfg = {
		.groups = WLAN_NLINK_MCAST_GRP_ID,
		.input = NULL
	};
#endif
	int ret = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
	cesium_nl_srv_sock = netlink_kernel_create(&init_net, WLAN_NLINK_CESIUM,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0))
						   THIS_MODULE,
#endif
						   &cfg);
#else
	cesium_nl_srv_sock = netlink_kernel_create(&init_net, WLAN_NLINK_CESIUM,
						   WLAN_NLINK_MCAST_GRP_ID,
						   NULL, NULL, THIS_MODULE);
#endif

	if (!cesium_nl_srv_sock) {
		hdd_err("NLINK:  cesium netlink_kernel_create failed");
		ret = -ECONNREFUSED;
	}

	return ret;
}

static void hdd_close_cesium_nl_sock(void)
{
	if (cesium_nl_srv_sock) {
		netlink_kernel_release(cesium_nl_srv_sock);
		cesium_nl_srv_sock = NULL;
	}
}

void hdd_update_dynamic_mac(struct hdd_context *hdd_ctx,
			    struct qdf_mac_addr *curr_mac_addr,
			    struct qdf_mac_addr *new_mac_addr)
{
	uint8_t i;

	hdd_enter();

	for (i = 0; i < QDF_MAX_CONCURRENCY_PERSONA; i++) {
		if (!qdf_mem_cmp(
			curr_mac_addr->bytes,
			&hdd_ctx->dynamic_mac_list[i].dynamic_mac.bytes[0],
				 sizeof(struct qdf_mac_addr))) {
			qdf_mem_copy(&hdd_ctx->dynamic_mac_list[i].dynamic_mac,
				     new_mac_addr->bytes,
				     sizeof(struct qdf_mac_addr));
			break;
		}
	}

	hdd_exit();
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
void
hdd_set_mld_address(struct hdd_adapter *adapter,
		    const struct qdf_mac_addr *mac_addr)
{
	int i;
	bool eht_capab;
	struct hdd_adapter *link_adapter;
	struct hdd_mlo_adapter_info *mlo_adapter_info;

	ucfg_psoc_mlme_get_11be_capab(adapter->hdd_ctx->psoc, &eht_capab);
	if (adapter->mlo_adapter_info.is_ml_adapter && eht_capab) {
		mlo_adapter_info = &adapter->mlo_adapter_info;
		for (i = 0; i < WLAN_MAX_MLD; i++) {
			link_adapter = mlo_adapter_info->link_adapter[i];
			if (link_adapter)
				qdf_copy_macaddr(&link_adapter->mld_addr,
						 mac_addr);
		}
		qdf_copy_macaddr(&adapter->mld_addr, mac_addr);
	}
}

/**
 * hdd_get_netdev_by_vdev_mac() - Get Netdev based on MAC
 * @mac_addr: Vdev MAC address
 *
 * Get netdev from adapter based upon Vdev MAC address.
 *
 * Return: netdev pointer.
 */
static qdf_netdev_t
hdd_get_netdev_by_vdev_mac(struct qdf_mac_addr *mac_addr)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct hdd_adapter *ml_adapter;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return NULL;
	}

	adapter = hdd_get_adapter_by_macaddr(hdd_ctx, mac_addr->bytes);
	if (!adapter) {
		hdd_err("Adapter not foud for MAC " QDF_MAC_ADDR_FMT "",
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		return NULL;
	}

	if (adapter->mlo_adapter_info.is_link_adapter &&
	    adapter->mlo_adapter_info.associate_with_ml_adapter) {
		ml_adapter = adapter->mlo_adapter_info.ml_adapter;
		adapter =  ml_adapter;
	}

	return adapter->dev;
}
#else
static qdf_netdev_t
hdd_get_netdev_by_vdev_mac(struct qdf_mac_addr *mac_addr)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return NULL;
	}

	adapter = hdd_get_adapter_by_macaddr(hdd_ctx, mac_addr->bytes);
	if (!adapter) {
		hdd_err("Adapter not foud for MAC " QDF_MAC_ADDR_FMT "",
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		return NULL;
	}

	return adapter->dev;
}
#endif

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
static void hdd_update_set_mac_addr_req_ctx(struct hdd_adapter *adapter,
					    void *req_ctx)
{
	adapter->set_mac_addr_req_ctx = req_ctx;
	if (adapter->mlo_adapter_info.associate_with_ml_adapter)
		adapter->mlo_adapter_info.ml_adapter->set_mac_addr_req_ctx =
									req_ctx;
}
#else
static void hdd_update_set_mac_addr_req_ctx(struct hdd_adapter *adapter,
					    void *req_ctx)
{
	adapter->set_mac_addr_req_ctx = req_ctx;
}
#endif

/**
 * hdd_is_dynamic_set_mac_addr_supported() - API to check dynamic MAC address
 *				             update is supported or not
 * @hdd_ctx: Pointer to the HDD context
 *
 * Return: true or false
 */
static inline bool
hdd_is_dynamic_set_mac_addr_supported(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->is_vdev_macaddr_dynamic_update_supported;
}

bool hdd_is_dynamic_set_mac_addr_allowed(struct hdd_adapter *adapter)
{
	if (!adapter->vdev) {
		hdd_err("VDEV is NULL");
		return false;
	}

	if (!hdd_is_dynamic_set_mac_addr_supported(adapter->hdd_ctx)) {
		hdd_info_rl("On iface up, set mac address change isn't supported");
		return false;
	}

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
		if (!cm_is_vdev_disconnected(adapter->vdev)) {
			hdd_info_rl("VDEV is not in disconnected state, set mac address isn't supported");
			return false;
		}
	fallthrough;
	case QDF_P2P_DEVICE_MODE:
		return true;
	case QDF_SAP_MODE:
		if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			hdd_info_rl("SAP is in up state, set mac address isn't supported");
			return false;
		} else {
			return true;
		}
	default:
		hdd_info_rl("Dynamic set mac address isn't supported for opmode:%d",
			adapter->device_mode);
		return false;
	}
}

int hdd_dynamic_mac_address_set(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter,
				struct qdf_mac_addr mac_addr)
{
	uint32_t *fw_resp_status;
	void *cookie;
	struct osif_request *request;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*fw_resp_status),
		.timeout_ms = WLAN_SET_MAC_ADDR_TIMEOUT
	};
	int ret;
	QDF_STATUS qdf_ret_status;
	struct qdf_mac_addr mld_addr;
	bool update_self_peer, update_mld_addr;

	qdf_ret_status = ucfg_vdev_mgr_cdp_vdev_detach(adapter->vdev);
	if (QDF_IS_STATUS_ERROR(qdf_ret_status)) {
		hdd_err("Failed to detach CDP vdev. Status:%d", qdf_ret_status);
		return qdf_status_to_os_return(qdf_ret_status);
	}

	request = osif_request_alloc(&params);
	if (!request) {
		ret = -ENOMEM;
		goto status_ret;
	}

	if (hdd_adapter_is_link_adapter(adapter)) {
		update_self_peer =
			hdd_adapter_is_associated_with_ml_adapter(adapter);
		update_mld_addr = true;
	} else if (hdd_adapter_is_sl_ml_adapter(adapter)) {
		update_mld_addr = true;
		if (adapter->device_mode == QDF_SAP_MODE)
			update_self_peer = false;
		else
			update_self_peer = true;
	} else {
		update_self_peer = true;
		update_mld_addr = false;
	}
	/* Host should hold a wake lock until the FW event response is received
	 * the WMI event would not be a wake up event.
	 */
	qdf_runtime_pm_prevent_suspend(
			&hdd_ctx->runtime_context.dyn_mac_addr_update);
	hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DYN_MAC_ADDR_UPDATE);

	cookie = osif_request_cookie(request);
	hdd_update_set_mac_addr_req_ctx(adapter, cookie);

	qdf_mem_copy(&mld_addr, adapter->mld_addr.bytes, sizeof(mld_addr));
	qdf_ret_status = sme_send_set_mac_addr(mac_addr, mld_addr,
					       adapter->vdev, update_mld_addr);
	ret = qdf_status_to_os_return(qdf_ret_status);
	if (QDF_STATUS_SUCCESS != qdf_ret_status) {
		hdd_nofl_err("Failed to send set MAC address command. Status:%d",
			     qdf_ret_status);
		osif_request_put(request);
		goto status_ret;
	} else {
		ret = osif_request_wait_for_response(request);
		if (ret) {
			hdd_err("Set MAC address response timed out");
		} else {
			fw_resp_status = (uint32_t *)osif_request_priv(request);
			if (*fw_resp_status) {
				hdd_err("Set MAC address failed in FW. Status: %d",
					*fw_resp_status);
				ret = -EAGAIN;
			}
		}
	}

	osif_request_put(request);

	qdf_ret_status = sme_update_vdev_mac_addr(
			   hdd_ctx->psoc, mac_addr, adapter->vdev,
			   update_self_peer, update_mld_addr,
			   ret);

	if (QDF_IS_STATUS_ERROR(qdf_ret_status))
		ret = qdf_status_to_os_return(qdf_ret_status);

status_ret:
	qdf_ret_status = ucfg_vdev_mgr_cdp_vdev_attach(adapter->vdev);
	if (QDF_IS_STATUS_ERROR(qdf_ret_status)) {
		hdd_allow_suspend(
			WIFI_POWER_EVENT_WAKELOCK_DYN_MAC_ADDR_UPDATE);
		qdf_runtime_pm_allow_suspend(
				&hdd_ctx->runtime_context.dyn_mac_addr_update);
		hdd_err("Failed to attach CDP vdev. status:%d", qdf_ret_status);
		return qdf_status_to_os_return(qdf_ret_status);
	}
	sme_vdev_set_data_tx_callback(adapter->vdev);

	/* Update FW WoW pattern with new MAC address */
	ucfg_pmo_del_wow_pattern(adapter->vdev);
	ucfg_pmo_register_wow_default_patterns(adapter->vdev);

	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DYN_MAC_ADDR_UPDATE);
	qdf_runtime_pm_allow_suspend(
			&hdd_ctx->runtime_context.dyn_mac_addr_update);

	return ret;
}

static void hdd_set_mac_addr_event_cb(uint8_t vdev_id, uint8_t status)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct osif_request *req;
	uint32_t *fw_response_status;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("No adapter found for VDEV ID:%d", vdev_id);
		return;
	}

	req = osif_request_get(adapter->set_mac_addr_req_ctx);
	if (!req) {
		osif_err("Obsolete request for VDEV ID:%d", vdev_id);
		return;
	}

	fw_response_status = (uint32_t *)osif_request_priv(req);
	*fw_response_status = status;

	osif_request_complete(req);
	osif_request_put(req);
}
#else
static inline bool
hdd_is_dynamic_set_mac_addr_supported(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

/**
 * __hdd_set_mac_address() - set the user specified mac address
 * @dev:	Pointer to the net device.
 * @addr:	Pointer to the sockaddr.
 *
 * This function sets the user specified mac address using
 * the command ifconfig wlanX hw ether <mac address>.
 *
 * Return: 0 for success, non zero for failure
 */
static int __hdd_set_mac_address(struct net_device *dev, void *addr)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_adapter *adapter_temp;
	struct hdd_context *hdd_ctx;
	struct sockaddr *psta_mac_addr = addr;
	QDF_STATUS qdf_ret_status = QDF_STATUS_SUCCESS;
	int ret;
	struct qdf_mac_addr mac_addr;
	bool net_if_running = netif_running(dev);

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	if (net_if_running) {
		if (!hdd_is_dynamic_set_mac_addr_allowed(adapter))
			return -ENOTSUPP;
	}

	qdf_mem_copy(&mac_addr, psta_mac_addr->sa_data, sizeof(mac_addr));
	adapter_temp = hdd_get_adapter_by_macaddr(hdd_ctx, mac_addr.bytes);
	if (adapter_temp) {
		if (!qdf_str_cmp(adapter_temp->dev->name, dev->name))
			return 0;
		hdd_err("%s adapter exist with same address " QDF_MAC_ADDR_FMT,
			adapter_temp->dev->name,
			QDF_MAC_ADDR_REF(mac_addr.bytes));
		return -EINVAL;
	}
	qdf_ret_status = wlan_hdd_validate_mac_address(&mac_addr);
	if (QDF_IS_STATUS_ERROR(qdf_ret_status))
		return -EINVAL;

	hdd_nofl_debug("Changing MAC to "
		       QDF_MAC_ADDR_FMT " of the interface %s ",
		       QDF_MAC_ADDR_REF(mac_addr.bytes), dev->name);

	if (net_if_running && adapter->vdev) {
		ret = hdd_update_vdev_mac_address(hdd_ctx, adapter, mac_addr);
		if (ret)
			return ret;
	}

	hdd_set_mld_address(adapter, &mac_addr);

	hdd_update_dynamic_mac(hdd_ctx, &adapter->mac_addr, &mac_addr);
	ucfg_dp_update_inf_mac(hdd_ctx->psoc, &adapter->mac_addr, &mac_addr);
	memcpy(&adapter->mac_addr, psta_mac_addr->sa_data, ETH_ALEN);
	qdf_net_update_net_device_dev_addr(dev, psta_mac_addr->sa_data,
					   ETH_ALEN);

	hdd_exit();
	return ret;
}

/**
 * hdd_set_mac_address() - Wrapper function to protect __hdd_set_mac_address()
 *	function from SSR
 * @net_dev: pointer to net_device structure
 * @addr: Pointer to the sockaddr
 *
 * This function sets the user specified mac address using
 * the command ifconfig wlanX hw ether <mac address>.
 *
 * Return: 0 for success.
 */
static int hdd_set_mac_address(struct net_device *net_dev, void *addr)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_set_mac_address(net_dev, addr);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static uint8_t *wlan_hdd_get_derived_intf_addr(struct hdd_context *hdd_ctx)
{
	int i, j;

	i = qdf_ffz(hdd_ctx->derived_intf_addr_mask);
	if (i < 0 || i >= hdd_ctx->num_derived_addr)
		return NULL;
	qdf_atomic_set_bit(i, &hdd_ctx->derived_intf_addr_mask);
	hdd_nofl_debug("Assigning MAC from derived list "QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(hdd_ctx->derived_mac_addr[i].bytes));

	/* Copy the mac in dynamic mac list at first free position */
	for (j = 0; j < QDF_MAX_CONCURRENCY_PERSONA; j++) {
		if (qdf_is_macaddr_zero(&hdd_ctx->
					dynamic_mac_list[j].dynamic_mac))
			break;
	}
	if (j == QDF_MAX_CONCURRENCY_PERSONA) {
		hdd_err("Max interfaces are up");
		return NULL;
	}

	qdf_mem_copy(&hdd_ctx->dynamic_mac_list[j].dynamic_mac.bytes,
		     &hdd_ctx->derived_mac_addr[i].bytes,
		     sizeof(struct qdf_mac_addr));
	hdd_ctx->dynamic_mac_list[j].is_provisioned_mac = false;
	hdd_ctx->dynamic_mac_list[j].bit_position = i;

	return hdd_ctx->derived_mac_addr[i].bytes;
}

static uint8_t *wlan_hdd_get_provisioned_intf_addr(struct hdd_context *hdd_ctx)
{
	int i, j;

	i = qdf_ffz(hdd_ctx->provisioned_intf_addr_mask);
	if (i < 0 || i >= hdd_ctx->num_provisioned_addr)
		return NULL;
	qdf_atomic_set_bit(i, &hdd_ctx->provisioned_intf_addr_mask);
	hdd_debug("Assigning MAC from provisioned list "QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(hdd_ctx->provisioned_mac_addr[i].bytes));

	/* Copy the mac in dynamic mac list at first free position */
	for (j = 0; j < QDF_MAX_CONCURRENCY_PERSONA; j++) {
		if (qdf_is_macaddr_zero(&hdd_ctx->
					dynamic_mac_list[j].dynamic_mac))
			break;
	}
	if (j == QDF_MAX_CONCURRENCY_PERSONA) {
		hdd_err("Max interfaces are up");
		return NULL;
	}

	qdf_mem_copy(&hdd_ctx->dynamic_mac_list[j].dynamic_mac.bytes,
		     &hdd_ctx->provisioned_mac_addr[i].bytes,
		     sizeof(struct qdf_mac_addr));
	hdd_ctx->dynamic_mac_list[j].is_provisioned_mac = true;
	hdd_ctx->dynamic_mac_list[j].bit_position = i;
	return hdd_ctx->provisioned_mac_addr[i].bytes;
}

uint8_t *wlan_hdd_get_intf_addr(struct hdd_context *hdd_ctx,
				enum QDF_OPMODE interface_type)
{
	uint8_t *mac_addr = NULL;

	if (qdf_atomic_test_bit(interface_type,
				(unsigned long *)
				(&hdd_ctx->config->provisioned_intf_pool)))
		mac_addr = wlan_hdd_get_provisioned_intf_addr(hdd_ctx);

	if ((!mac_addr) &&
	    (qdf_atomic_test_bit(interface_type,
				 (unsigned long *)
				 (&hdd_ctx->config->derived_intf_pool))))
		mac_addr = wlan_hdd_get_derived_intf_addr(hdd_ctx);

	if (!mac_addr)
		hdd_err("MAC is not available in both the lists");
	return mac_addr;
}

void wlan_hdd_release_intf_addr(struct hdd_context *hdd_ctx,
				uint8_t *releaseAddr)
{
	int i;
	int mac_pos_in_mask;

	for (i = 0; i < QDF_MAX_CONCURRENCY_PERSONA; i++) {
		if (!memcmp(releaseAddr,
		    hdd_ctx->dynamic_mac_list[i].dynamic_mac.bytes,
		    QDF_MAC_ADDR_SIZE)) {
			mac_pos_in_mask =
				hdd_ctx->dynamic_mac_list[i].bit_position;
			if (hdd_ctx->dynamic_mac_list[i].is_provisioned_mac) {
				qdf_atomic_clear_bit(
						mac_pos_in_mask,
						&hdd_ctx->
						   provisioned_intf_addr_mask);
				hdd_debug("Releasing MAC from provisioned list");
				hdd_debug(
					  QDF_MAC_ADDR_FMT,
					  QDF_MAC_ADDR_REF(releaseAddr));
			} else {
				qdf_atomic_clear_bit(
						mac_pos_in_mask, &hdd_ctx->
						derived_intf_addr_mask);
				hdd_debug("Releasing MAC from derived list");
				hdd_debug(QDF_MAC_ADDR_FMT,
					  QDF_MAC_ADDR_REF(releaseAddr));
			}
			qdf_zero_macaddr(&hdd_ctx->
					    dynamic_mac_list[i].dynamic_mac);
			hdd_ctx->dynamic_mac_list[i].is_provisioned_mac =
									false;
			hdd_ctx->dynamic_mac_list[i].bit_position = 0;
			break;
		}

	}
	if (i == QDF_MAX_CONCURRENCY_PERSONA)
		hdd_debug("Releasing non existing MAC " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(releaseAddr));
}

/**
 * hdd_set_derived_multicast_list(): Add derived peer multicast address list in
 *                                   multicast list request to the FW
 * @psoc: Pointer to psoc
 * @adapter: Pointer to hdd adapter
 * @mc_list_request: Multicast list request to the FW
 * @mc_count: number of multicast addresses received from the kernel
 *
 * Return: None
 */
static void
hdd_set_derived_multicast_list(struct wlan_objmgr_psoc *psoc,
			       struct hdd_adapter *adapter,
			       struct pmo_mc_addr_list_params *mc_list_request,
			       int *mc_count)
{
	int i = 0, j = 0, list_count = *mc_count;
	struct qdf_mac_addr *peer_mc_addr_list = NULL;
	uint8_t  driver_mc_cnt = 0;
	uint32_t max_ndp_sessions = 0;

	cfg_nan_get_ndp_max_sessions(psoc, &max_ndp_sessions);

	ucfg_nan_get_peer_mc_list(adapter->vdev, &peer_mc_addr_list);

	for (j = 0; j < max_ndp_sessions; j++) {
		for (i = 0; i < list_count; i++) {
			if (qdf_is_macaddr_zero(&peer_mc_addr_list[j]) ||
			    qdf_is_macaddr_equal(&mc_list_request->mc_addr[i],
						 &peer_mc_addr_list[j]))
				break;
		}
		if (i == list_count) {
			qdf_mem_copy(
			   &(mc_list_request->mc_addr[list_count +
						driver_mc_cnt].bytes),
			   peer_mc_addr_list[j].bytes, ETH_ALEN);
			hdd_debug("mlist[%d] = " QDF_MAC_ADDR_FMT,
				  list_count + driver_mc_cnt,
				  QDF_MAC_ADDR_REF(
					mc_list_request->mc_addr[list_count +
					driver_mc_cnt].bytes));
			driver_mc_cnt++;
		}
	}
	*mc_count += driver_mc_cnt;
}

/**
 * __hdd_set_multicast_list() - set the multicast address list
 * @dev:	Pointer to the WLAN device.
 *
 * This function sets the multicast address list.
 *
 * Return: None
 */
static void __hdd_set_multicast_list(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int i = 0, errno;
	struct netdev_hw_addr *ha;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct pmo_mc_addr_list_params *mc_list_request = NULL;
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	int mc_count = 0;

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_err_rl("Device is system suspended");
		return;
	}

	hdd_enter_dev(dev);
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam())
		return;

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return;

	errno = hdd_validate_adapter(adapter);
	if (errno)
		return;

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_debug("Driver module is closed");
		return;
	}

	mc_list_request = qdf_mem_malloc(sizeof(*mc_list_request));
	if (!mc_list_request)
		return;

	/* Delete already configured multicast address list */
	if (adapter->mc_addr_list.mc_cnt > 0)
		hdd_disable_and_flush_mc_addr_list(adapter,
			pmo_mc_list_change_notify);

	if (dev->flags & IFF_ALLMULTI) {
		hdd_debug("allow all multicast frames");
		hdd_disable_and_flush_mc_addr_list(adapter,
			pmo_mc_list_change_notify);
	} else {
		mc_count = netdev_mc_count(dev);
		if (mc_count > ucfg_pmo_max_mc_addr_supported(psoc)) {
			hdd_debug("Exceeded max MC filter addresses (%d). Allowing all MC frames by disabling MC address filtering",
				  ucfg_pmo_max_mc_addr_supported(psoc));
			hdd_disable_and_flush_mc_addr_list(adapter,
				pmo_mc_list_change_notify);
			adapter->mc_addr_list.mc_cnt = 0;
			goto free_req;
		}
		netdev_for_each_mc_addr(ha, dev) {
			if (i == mc_count)
				break;
			memset(&(mc_list_request->mc_addr[i].bytes),
				0, ETH_ALEN);
			memcpy(&(mc_list_request->mc_addr[i].bytes),
				ha->addr, ETH_ALEN);
			hdd_debug("mlist[%d] = "QDF_MAC_ADDR_FMT, i,
				  QDF_MAC_ADDR_REF(mc_list_request->mc_addr[i].bytes));
			i++;
		}

		if (adapter->device_mode == QDF_NDI_MODE)
			hdd_set_derived_multicast_list(psoc, adapter,
						       mc_list_request,
						       &mc_count);
	}

	adapter->mc_addr_list.mc_cnt = mc_count;
	mc_list_request->psoc = psoc;
	mc_list_request->vdev_id = adapter->vdev_id;
	mc_list_request->count = mc_count;

	errno = hdd_cache_mc_addr_list(mc_list_request);
	if (errno) {
		hdd_debug("Failed to cache MC address list for vdev %u; errno:%d",
			  adapter->vdev_id, errno);
		goto free_req;
	}

	hdd_enable_mc_addr_filtering(adapter, pmo_mc_list_change_notify);

free_req:
	qdf_mem_free(mc_list_request);
}

/**
 * hdd_set_multicast_list() - SSR wrapper function for __hdd_set_multicast_list
 * @net_dev: pointer to net_device
 *
 * Return: none
 */
static void hdd_set_multicast_list(struct net_device *net_dev)
{
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync))
		return;

	__hdd_set_multicast_list(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);
}

#ifdef WLAN_FEATURE_TSF_PTP
static const struct ethtool_ops wlan_ethtool_ops = {
	.get_ts_info = wlan_get_ts_info,
};
#endif

/**
 * __hdd_fix_features - Adjust the feature flags needed to be updated
 * @net_dev: Handle to net_device
 * @features: Currently enabled feature flags
 *
 * Return: Adjusted feature flags on success, old feature on failure
 */
static netdev_features_t __hdd_fix_features(struct net_device *net_dev,
					    netdev_features_t features)
{
	netdev_features_t feature_change_req = features;
	netdev_features_t feature_tso_csum;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);

	if (!adapter->handle_feature_update) {
		hdd_debug("Not triggered by hdd_netdev_update_features");
		return features;
	}

	feature_tso_csum = hdd_get_tso_csum_feature_flags();
	if (hdd_is_legacy_connection(adapter)) {
		/* Disable checksum and TSO */
		feature_change_req &= ~feature_tso_csum;
		adapter->tso_csum_feature_enabled = 0;
	} else {
		/* Enable checksum and TSO */
		feature_change_req |= feature_tso_csum;
		adapter->tso_csum_feature_enabled = 1;
	}
	hdd_debug("vdev mode %d current features 0x%llx, requesting feature change 0x%llx",
		  adapter->device_mode, net_dev->features,
		  feature_change_req);

	return feature_change_req;
}

/**
 * hdd_fix_features() - Wrapper for __hdd_fix_features to protect it from SSR
 * @net_dev: Pointer to net_device structure
 * @features: Updated features set
 *
 * Adjusts the feature request, do not update the device yet.
 *
 * Return: updated feature for success, incoming feature as is on failure
 */
static netdev_features_t hdd_fix_features(struct net_device *net_dev,
					  netdev_features_t features)
{
	int errno;
	int changed_features = features;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return features;

	changed_features = __hdd_fix_features(net_dev, features);

	osif_vdev_sync_op_stop(vdev_sync);

	return changed_features;
}
/**
 * __hdd_set_features - Notify device about change in features
 * @net_dev: Handle to net_device
 * @features: Existing + requested feature after resolving the dependency
 *
 * Return: 0 on success, non zero error on failure
 */
static int __hdd_set_features(struct net_device *net_dev,
			      netdev_features_t features)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!adapter->handle_feature_update) {
		hdd_debug("Not triggered by hdd_netdev_update_features");
		return 0;
	}

	if (!soc)
		return 0;

	hdd_debug("vdev mode %d vdev_id %d current features 0x%llx, changed features 0x%llx",
		  adapter->device_mode, adapter->vdev_id, net_dev->features,
		  features);

	return 0;
}

/**
 * hdd_set_features() - Wrapper for __hdd_set_features to protect it from SSR
 * @net_dev: Pointer to net_device structure
 * @features: Updated features set
 *
 * Is called to update device configurations for changed features.
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_set_features(struct net_device *net_dev,
			    netdev_features_t features)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno) {
		/*
		 * Only invoke from netdev_feature_update_work expected,
		 * which is from CLD inside.
		 * Ignore others from upper stack during loading phase,
		 * and return success to avoid failure print from kernel.
		 */
		hdd_debug("VDEV in transition, ignore set_features");
		return 0;
	}

	errno = __hdd_set_features(net_dev, features);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

#define HDD_NETDEV_FEATURES_UPDATE_MAX_WAIT_COUNT	10
#define HDD_NETDEV_FEATURES_UPDATE_WAIT_INTERVAL_MS	20

void hdd_netdev_update_features(struct hdd_adapter *adapter)
{
	struct net_device *net_dev = adapter->dev;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);
	bool request_feature_update = false;
	int wait_count = HDD_NETDEV_FEATURES_UPDATE_MAX_WAIT_COUNT;

	if (!soc)
		return;

	if (!cdp_cfg_get(soc, cfg_dp_disable_legacy_mode_csum_offload))
		return;

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
		if (cdp_cfg_get(soc, cfg_dp_enable_ip_tcp_udp_checksum_offload))
			request_feature_update = true;
		break;
	default:
		break;
	}

	if (request_feature_update) {
		hdd_debug("Update net_dev features for device mode %d",
			  adapter->device_mode);
		while (!adapter->delete_in_progress) {
			if (rtnl_trylock()) {
				adapter->handle_feature_update = true;
				netdev_update_features(net_dev);
				adapter->handle_feature_update = false;
				rtnl_unlock();
				break;
			}

			if (wait_count--) {
				qdf_sleep(
				HDD_NETDEV_FEATURES_UPDATE_WAIT_INTERVAL_MS);
			} else {
				/*
				 * We have failed to updated the netdev
				 * features for very long, so enable the queues
				 * now. The impact of not being able to update
				 * the netdev feature is lower TPUT when
				 * switching from legacy to non-legacy mode.
				 */
				hdd_err("Failed to update netdev features for device mode %d",
					adapter->device_mode);
				break;
			}
		}
	}
}

static const struct net_device_ops wlan_drv_ops = {
	.ndo_open = hdd_open,
	.ndo_stop = hdd_stop,
	.ndo_uninit = hdd_uninit,
	.ndo_start_xmit = hdd_hard_start_xmit,
	.ndo_fix_features = hdd_fix_features,
	.ndo_set_features = hdd_set_features,
	.ndo_tx_timeout = hdd_tx_timeout,
	.ndo_get_stats = hdd_get_stats,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
	.ndo_do_ioctl = hdd_ioctl,
#endif
	.ndo_set_mac_address = hdd_set_mac_address,
	.ndo_select_queue = hdd_select_queue,
	.ndo_set_rx_mode = hdd_set_multicast_list,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	.ndo_siocdevprivate = hdd_dev_private_ioctl,
#endif
};

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/* Monitor mode net_device_ops, does not Tx and most of operations. */
static const struct net_device_ops wlan_mon_drv_ops = {
	.ndo_open = hdd_mon_open,
	.ndo_stop = hdd_stop,
	.ndo_get_stats = hdd_get_stats,
};

/**
 * hdd_set_mon_ops() - update net_device ops for monitor mode
 * @dev: Handle to struct net_device to be updated.
 * Return: None
 */
static void hdd_set_mon_ops(struct net_device *dev)
{
	dev->netdev_ops = &wlan_mon_drv_ops;
}

#ifdef WLAN_FEATURE_TSF_PTP
void hdd_set_station_ops(struct net_device *dev)
{
	if (cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE) {
		hdd_set_mon_ops(dev);
	} else {
		dev->netdev_ops = &wlan_drv_ops;
		dev->ethtool_ops = &wlan_ethtool_ops;
	}
}
#else
void hdd_set_station_ops(struct net_device *dev)
{
	if (cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE)
		hdd_set_mon_ops(dev);
	else
		dev->netdev_ops = &wlan_drv_ops;
}

#endif
#else
#ifdef WLAN_FEATURE_TSF_PTP
void hdd_set_station_ops(struct net_device *dev)
{
	dev->netdev_ops = &wlan_drv_ops;
	dev->ethtool_ops = &wlan_ethtool_ops;
}
#else
void hdd_set_station_ops(struct net_device *dev)
{
	dev->netdev_ops = &wlan_drv_ops;
}
#endif
static void hdd_set_mon_ops(struct net_device *dev)
{
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE
/* Packet Capture mode net_device_ops, does not Tx and most of operations. */
static const struct net_device_ops wlan_pktcapture_drv_ops = {
	.ndo_open = hdd_pktcapture_open,
	.ndo_stop = hdd_stop,
	.ndo_get_stats = hdd_get_stats,
};

static void hdd_set_pktcapture_ops(struct net_device *dev)
{
	dev->netdev_ops = &wlan_pktcapture_drv_ops;
}
#else
static void hdd_set_pktcapture_ops(struct net_device *dev)
{
}
#endif

#ifdef MULTI_CLIENT_LL_SUPPORT
/**
 * hdd_set_multi_client_ll_support() - set multi client ll support flag in
 * allocated station hdd adapter
 * @adapter: pointer to hdd adapter
 *
 * Return: none
 */
static void hdd_set_multi_client_ll_support(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	bool multi_client_ll_ini_support, multi_client_ll_caps;

	ucfg_mlme_cfg_get_multi_client_ll_ini_support(hdd_ctx->psoc,
						&multi_client_ll_ini_support);
	multi_client_ll_caps =
		ucfg_mlme_get_wlm_multi_client_ll_caps(hdd_ctx->psoc);

	hdd_debug("fw caps: %d, ini: %d", multi_client_ll_caps,
		  multi_client_ll_ini_support);
	if (multi_client_ll_caps && multi_client_ll_ini_support)
		adapter->multi_client_ll_support = true;
}
#else
static inline void
hdd_set_multi_client_ll_support(struct hdd_adapter *adapter)
{
}
#endif

/**
 * hdd_alloc_station_adapter() - allocate the station hdd adapter
 * @hdd_ctx: global hdd context
 * @mac_addr: mac address to assign to the interface
 * @name_assign_type: name assignment type
 * @name: User-visible name of the interface
 * @session_type: interface type to be created
 *
 * hdd adapter pointer would point to the netdev->priv space, this function
 * would retrieve the pointer, and setup the hdd adapter configuration.
 *
 * Return: the pointer to hdd adapter, otherwise NULL
 */
static struct hdd_adapter *
hdd_alloc_station_adapter(struct hdd_context *hdd_ctx, tSirMacAddr mac_addr,
			  unsigned char name_assign_type, const char *name,
			  uint8_t session_type)
{
	struct net_device *dev;
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;
	QDF_STATUS qdf_status;
	uint8_t latency_level;

	/* cfg80211 initialization and registration */
	dev = alloc_netdev_mqs(sizeof(*adapter), name,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) || defined(WITH_BACKPORTS)
			      name_assign_type,
#endif
			      ((cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE ||
			       wlan_hdd_is_session_type_monitor(session_type)) ?
			       hdd_mon_mode_ether_setup : ether_setup),
			      NUM_TX_QUEUES, NUM_RX_QUEUES);

	if (!dev) {
		hdd_err("Failed to allocate new net_device '%s'", name);
		return NULL;
	}

	adapter = netdev_priv(dev);

	qdf_mem_zero(adapter, sizeof(*adapter));
	sta_ctx = &adapter->session.station;
	qdf_mem_zero(sta_ctx->conn_info.peer_macaddr,
		     sizeof(sta_ctx->conn_info.peer_macaddr));
	adapter->dev = dev;
	adapter->hdd_ctx = hdd_ctx;
	adapter->magic = WLAN_HDD_ADAPTER_MAGIC;
	adapter->vdev_id = WLAN_UMAC_VDEV_ID_MAX;

	qdf_status = hdd_monitor_mode_qdf_create_event(adapter, session_type);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		hdd_err_rl("create monitor mode vdve up event failed");
		goto free_net_dev;
	}

	init_completion(&adapter->vdev_destroy_event);

	hdd_update_dynamic_tsf_sync(adapter);
	adapter->is_link_up_service_needed = false;
	adapter->send_mode_change = true;

	/* Cache station count initialize to zero */
	qdf_atomic_init(&adapter->cache_sta_count);

	/* Init the net_device structure */
	strlcpy(dev->name, name, IFNAMSIZ);

	qdf_net_update_net_device_dev_addr(dev, mac_addr, sizeof(tSirMacAddr));
	qdf_mem_copy(adapter->mac_addr.bytes, mac_addr, sizeof(tSirMacAddr));
	dev->watchdog_timeo = HDD_TX_TIMEOUT;

	if (wlan_hdd_is_session_type_monitor(session_type)) {
		if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE)
			hdd_set_pktcapture_ops(adapter->dev);
		if (ucfg_mlme_is_sta_mon_conc_supported(hdd_ctx->psoc))
			hdd_set_mon_ops(adapter->dev);
	} else {
		hdd_set_station_ops(adapter->dev);
	}

	hdd_dev_setup_destructor(dev);
	dev->ieee80211_ptr = &adapter->wdev;
	dev->tx_queue_len = HDD_NETDEV_TX_QUEUE_LEN;
	adapter->wdev.wiphy = hdd_ctx->wiphy;
	adapter->wdev.netdev = dev;
	qdf_status = ucfg_mlme_cfg_get_wlm_level(hdd_ctx->psoc, &latency_level);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		hdd_debug("Can't get latency level");
		latency_level =
			QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_NORMAL;
	}
	adapter->latency_level = latency_level;
	hdd_set_multi_client_ll_support(adapter);

	/* set dev's parent to underlying device */
	SET_NETDEV_DEV(dev, hdd_ctx->parent_dev);
	spin_lock_init(&adapter->pause_map_lock);
	adapter->start_time = qdf_system_ticks();
	adapter->last_time = adapter->start_time;

	qdf_atomic_init(&adapter->hdd_stats.is_ll_stats_req_pending);
	hdd_init_get_sta_in_ll_stats_config(adapter);

	return adapter;

free_net_dev:
	free_netdev(adapter->dev);

	return NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
static int
hdd_register_netdevice(struct hdd_adapter *adapter, struct net_device *dev,
		       struct hdd_adapter_create_param *params)
{
	int ret;

	if (params->is_add_virtual_iface)
		ret = wlan_cfg80211_register_netdevice(dev);
	else
		ret = register_netdevice(dev);

	return ret;
}
#else
static int
hdd_register_netdevice(struct hdd_adapter *adapter, struct net_device *dev,
		       struct hdd_adapter_create_param *params)
{
	return register_netdevice(dev);
}
#endif

static QDF_STATUS
hdd_register_interface(struct hdd_adapter *adapter, bool rtnl_held,
		       struct hdd_adapter_create_param *params)
{
	struct net_device *dev = adapter->dev;
	int ret;

	hdd_enter();

	if (rtnl_held) {
		if (strnchr(dev->name, IFNAMSIZ - 1, '%')) {

			ret = dev_alloc_name(dev, dev->name);
			if (ret < 0) {
				hdd_err(
				    "unable to get dev name: %s, err = 0x%x",
				    dev->name, ret);
				return QDF_STATUS_E_FAILURE;
			}
		}

		ret = hdd_register_netdevice(adapter, dev, params);
		if (ret) {
			hdd_err("register_netdevice(%s) failed, err = 0x%x",
				dev->name, ret);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		ret = register_netdev(dev);
		if (ret) {
			hdd_err("register_netdev(%s) failed, err = 0x%x",
				dev->name, ret);
			return QDF_STATUS_E_FAILURE;
		}
	}
	set_bit(NET_DEVICE_REGISTERED, &adapter->event_flags);

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_sme_close_session_callback(uint8_t vdev_id)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return QDF_STATUS_E_FAILURE;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("NULL adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("Invalid magic");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	clear_bit(SME_SESSION_OPENED, &adapter->event_flags);
	qdf_spin_lock_bh(&adapter->vdev_lock);
	adapter->vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	qdf_spin_unlock_bh(&adapter->vdev_lock);

	/*
	 * We can be blocked while waiting for scheduled work to be
	 * flushed, and the adapter structure can potentially be freed, in
	 * which case the magic will have been reset.  So make sure the
	 * magic is still good, and hence the adapter structure is still
	 * valid, before signaling completion
	 */
	if (WLAN_HDD_ADAPTER_MAGIC == adapter->magic)
		complete(&adapter->vdev_destroy_event);

	return QDF_STATUS_SUCCESS;
}

int hdd_vdev_ready(struct wlan_objmgr_vdev *vdev,
		   struct qdf_mac_addr *bridgeaddr)
{
	QDF_STATUS status;

	status = pmo_vdev_ready(vdev, bridgeaddr);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	status = ucfg_reg_11d_vdev_created_update(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	if (wma_capability_enhanced_mcast_filter())
		status = ucfg_pmo_enhanced_mc_filter_enable(vdev);
	else
		status = ucfg_pmo_enhanced_mc_filter_disable(vdev);

	return qdf_status_to_os_return(status);
}

/**
 * hdd_check_wait_for_hw_mode_completion - Check hw mode in progress
 * @hdd_ctx: hdd context
 *
 * Check and wait for hw mode response if any hw mode change is
 * in progress. Vdev delete will purge the serialization queue
 * for the vdev. It will cause issues when the fw event coming
 * up later and no active hw mode change req ser command in queue.
 *
 * Return void
 */
static void hdd_check_wait_for_hw_mode_completion(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	if (!wlan_hdd_validate_context(hdd_ctx) &&
	    policy_mgr_is_hw_mode_change_in_progress(
		hdd_ctx->psoc)) {
		status = policy_mgr_wait_for_connection_update(
			hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_nofl_debug("qdf wait for hw mode event failed!!");
		}
	}
}

static void hdd_stop_last_active_connection(struct hdd_context *hdd_ctx,
					    struct wlan_objmgr_vdev *vdev)
{
	enum policy_mgr_con_mode mode;
	struct wlan_objmgr_psoc *psoc;

	/* If this is the last active connection check
	 * and stop the opportunistic timer.
	 */
	psoc = wlan_vdev_get_psoc(vdev);
	mode = policy_mgr_convert_device_mode_to_qdf_type(
					wlan_vdev_mlme_get_opmode(vdev));
	if ((policy_mgr_get_connection_count(psoc) == 1 &&
	     policy_mgr_mode_specific_connection_count(psoc,
						       mode, NULL) == 1) ||
	     (!policy_mgr_get_connection_count(psoc) &&
	     !hdd_is_any_sta_connecting(hdd_ctx))) {
		policy_mgr_check_and_stop_opportunistic_timer(
						psoc,
						wlan_vdev_get_id(vdev));
	}
}

static int hdd_vdev_destroy_event_wait(struct hdd_context *hdd_ctx,
				       struct wlan_objmgr_vdev *vdev)
{
	long rc;
	QDF_STATUS status;
	uint8_t vdev_id;
	struct hdd_adapter *adapter;

	vdev_id = wlan_vdev_get_id(vdev);
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	/* close sme session (destroy vdev in firmware via legacy API) */
	INIT_COMPLETION(adapter->vdev_destroy_event);
	status = sme_vdev_delete(hdd_ctx->mac_handle, vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("vdev %d: failed to delete with status:%d",
			vdev_id, status);
		return -EAGAIN;
	}

	/* block on a completion variable until sme session is closed */
	rc = wait_for_completion_timeout(
			&adapter->vdev_destroy_event,
			msecs_to_jiffies(SME_CMD_VDEV_CREATE_DELETE_TIMEOUT));
	if (!rc) {
		hdd_err("vdev %d: timed out waiting for delete", vdev_id);
		clear_bit(SME_SESSION_OPENED, &adapter->event_flags);
		sme_cleanup_session(hdd_ctx->mac_handle, vdev_id);
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
			       WLAN_LOG_INDICATOR_HOST_DRIVER,
			       WLAN_LOG_REASON_VDEV_DELETE_RSP_TIMED_OUT,
			       true, true);
		return -EINVAL;
	}

	hdd_nofl_info("vdev %d destroyed successfully", vdev_id);
	return 0;
}

int hdd_vdev_destroy(struct hdd_adapter *adapter)
{
	int ret;
	uint8_t vdev_id;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;

	vdev_id = adapter->vdev_id;
	hdd_nofl_debug("destroying vdev %d", vdev_id);

	/* vdev created sanity check */
	if (!test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		hdd_nofl_debug("vdev %u does not exist", vdev_id);
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	if (!vdev)
		return -EINVAL;

	hdd_stop_last_active_connection(hdd_ctx, vdev);
	hdd_check_wait_for_hw_mode_completion(hdd_ctx);
	ucfg_pmo_del_wow_pattern(vdev);
	status = ucfg_reg_11d_vdev_delete_update(vdev);
	ucfg_scan_vdev_set_disable(vdev, REASON_VDEV_DOWN);
	wlan_hdd_scan_abort(adapter);
	wlan_cfg80211_cleanup_scan_queue(hdd_ctx->pdev, adapter->dev);
	ucfg_son_disable_cbs(vdev);
	/* Disable serialization for vdev before sending vdev delete */
	wlan_ser_vdev_queue_disable(vdev);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);

	qdf_spin_lock_bh(&adapter->vdev_lock);
	hdd_mlo_t2lm_unregister_callback(adapter->vdev);
	adapter->vdev = NULL;
	qdf_spin_unlock_bh(&adapter->vdev_lock);
	osif_cm_osif_priv_deinit(vdev);

	/* Release the hdd reference */
	wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);

	/* Get runtime lock to prevent runtime suspend */
	qdf_runtime_pm_prevent_suspend(&hdd_ctx->runtime_context.vdev_destroy);

	ret = hdd_vdev_destroy_event_wait(hdd_ctx, vdev);

	qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.vdev_destroy);
	return ret;
}

void
hdd_store_nss_chains_cfg_in_vdev(struct hdd_adapter *adapter)
{
	struct wlan_mlme_nss_chains vdev_ini_cfg;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wlan_objmgr_vdev *vdev;

	/* Populate the nss chain params from ini for this vdev type */
	sme_populate_nss_chain_params(hdd_ctx->mac_handle, &vdev_ini_cfg,
				      adapter->device_mode,
				      hdd_ctx->num_rf_chains);

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	/* Store the nss chain config into the vdev */
	if (vdev) {
		sme_store_nss_chains_cfg_in_vdev(vdev, &vdev_ini_cfg);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
	} else {
		hdd_err("Vdev is NULL");
	}
}

bool hdd_is_vdev_in_conn_state(struct hdd_adapter *adapter)
{
	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
		return hdd_cm_is_vdev_associated(adapter);
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
		return (test_bit(SOFTAP_BSS_STARTED,
				 &adapter->event_flags));
	default:
		hdd_err("Device mode %d invalid", adapter->device_mode);
		return 0;
	}

	return 0;
}

static void hdd_store_vdev_info(struct hdd_adapter *adapter,
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_osif_priv *osif_priv;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (osif_priv) {
		osif_priv->wdev = adapter->dev->ieee80211_ptr;
		osif_priv->legacy_osif_priv = adapter;
	}

	qdf_spin_lock_bh(&adapter->vdev_lock);
	adapter->vdev_id = wlan_vdev_get_id(vdev);
	adapter->vdev = vdev;
	qdf_spin_unlock_bh(&adapter->vdev_lock);
}

static void hdd_vdev_set_ht_vht_ies(mac_handle_t mac_handle,
				    struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	bool bval = false;

	psoc = wlan_vdev_get_psoc(vdev);
	status = ucfg_mlme_get_vht_enable2x2(psoc, &bval);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("unable to get vht_enable2x2");

	sme_set_pdev_ht_vht_ies(mac_handle, bval);
	sme_set_vdev_ies_per_band(mac_handle, wlan_vdev_get_id(vdev),
				  wlan_vdev_mlme_get_opmode(vdev));
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
static void
hdd_populate_vdev_create_params(struct hdd_adapter *adapter,
				struct wlan_vdev_create_params *vdev_params)
{
	struct hdd_mlo_adapter_info *mlo_adapter_info;
	struct hdd_adapter *link_adapter;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	bool eht_capab;

	hdd_enter_dev(adapter->dev);
	mlo_adapter_info = &adapter->mlo_adapter_info;

	ucfg_psoc_mlme_get_11be_capab(hdd_ctx->psoc, &eht_capab);
	if (mlo_adapter_info->is_ml_adapter && eht_capab &&
	    adapter->device_mode == QDF_STA_MODE) {
		link_adapter = hdd_get_assoc_link_adapter(adapter);
		if (link_adapter) {
			qdf_mem_copy(vdev_params->macaddr,
				     link_adapter->mac_addr.bytes,
				     QDF_MAC_ADDR_SIZE);
		}
	} else {
		qdf_mem_copy(vdev_params->macaddr, adapter->mac_addr.bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	vdev_params->opmode = adapter->device_mode;

	if (eht_capab) {
		qdf_mem_copy(vdev_params->mldaddr, adapter->mld_addr.bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	vdev_params->size_vdev_priv = sizeof(struct vdev_osif_priv);
	hdd_exit();
}
#else
static void
hdd_populate_vdev_create_params(struct hdd_adapter *adapter,
				struct wlan_vdev_create_params *vdev_params)
{
	vdev_params->opmode = adapter->device_mode;
	qdf_mem_copy(vdev_params->macaddr,
		     adapter->mac_addr.bytes,
		     QDF_NET_MAC_ADDR_MAX_LEN);
	vdev_params->size_vdev_priv = sizeof(struct vdev_osif_priv);
}
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_EXTERNAL_AUTH_MLO_SUPPORT)
static void
hdd_set_vdev_mlo_external_sae_auth_conversion(struct wlan_objmgr_vdev *vdev,
					      enum QDF_OPMODE mode)
{
	if (mode == QDF_STA_MODE || mode == QDF_SAP_MODE)
		wlan_vdev_set_mlo_external_sae_auth_conversion(vdev, true);
}
#else
static inline void
hdd_set_vdev_mlo_external_sae_auth_conversion(struct wlan_objmgr_vdev *vdev,
					      enum QDF_OPMODE mode)
{
}
#endif

int hdd_vdev_create(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	int errno = 0;
	bool bval;
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_vdev_create_params vdev_params = {0};
	bool target_bigtk_support = false;
	uint16_t max_peer_count;

	hdd_nofl_debug("creating new vdev");

	/* do vdev create via objmgr */
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = sme_check_for_duplicate_session(hdd_ctx->mac_handle,
						 adapter->mac_addr.bytes);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Duplicate session is existing with same mac address");
		errno = qdf_status_to_os_return(status);
		return errno;
	}

	hdd_populate_vdev_create_params(adapter, &vdev_params);

	vdev = sme_vdev_create(hdd_ctx->mac_handle, &vdev_params);
	if (!vdev) {
		hdd_err("failed to create vdev");
		return -EINVAL;
	}

	if (wlan_objmgr_vdev_try_get_ref(vdev, WLAN_HDD_ID_OBJ_MGR) !=
	    QDF_STATUS_SUCCESS) {
		errno = QDF_STATUS_E_INVAL;
		sme_vdev_delete(hdd_ctx->mac_handle, vdev);
		return -EINVAL;
	}

	hdd_store_vdev_info(adapter, vdev);
	osif_cm_osif_priv_init(vdev);

	if (hdd_adapter_is_ml_adapter(adapter))
		hdd_mlo_t2lm_register_callback(vdev);

	set_bit(SME_SESSION_OPENED, &adapter->event_flags);
	status = sme_vdev_post_vdev_create_setup(hdd_ctx->mac_handle, vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to setup the vdev");
		errno = qdf_status_to_os_return(status);
		goto hdd_vdev_destroy_procedure;
	}

	/* firmware ready for component communication, raise vdev_ready event */
	errno = hdd_vdev_ready(vdev,
			       (struct qdf_mac_addr *)hdd_ctx->bridgeaddr);
	if (errno) {
		hdd_err("failed to dispatch vdev ready event: %d", errno);
		goto hdd_vdev_destroy_procedure;
	}

	if (adapter->device_mode == QDF_STA_MODE) {
		bval = false;
		status = ucfg_mlme_get_rtt_mac_randomization(hdd_ctx->psoc,
							     &bval);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("unable to get RTT MAC randomization value");

		hdd_debug("setting RTT mac randomization param: %d", bval);
		errno = sme_cli_set_command(adapter->vdev_id,
			wmi_vdev_param_enable_disable_rtt_initiator_random_mac,
			bval,
			VDEV_CMD);
		if (0 != errno)
			hdd_err("RTT mac randomization param set failed %d",
				errno);
	}

	if (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
		if (!vdev)
			goto hdd_vdev_destroy_procedure;

		/* Max peer can be tdls peers + self peer + bss peer */
		max_peer_count = cfg_tdls_get_max_peer_count(hdd_ctx->psoc);
		max_peer_count += 2;
		wlan_vdev_set_max_peer_count(vdev, max_peer_count);

		ucfg_mlme_get_bigtk_support(hdd_ctx->psoc, &target_bigtk_support);
		if (target_bigtk_support)
			mlme_set_bigtk_support(vdev, true);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
	}

	if (QDF_NAN_DISC_MODE == adapter->device_mode) {
		sme_cli_set_command(
		adapter->vdev_id,
		wmi_vdev_param_allow_nan_initial_discovery_of_mp0_cluster,
		cfg_nan_get_support_mp0_discovery(hdd_ctx->psoc),
		VDEV_CMD);
	}
	hdd_store_nss_chains_cfg_in_vdev(adapter);

	hdd_set_vdev_mlo_external_sae_auth_conversion(vdev,
						      adapter->device_mode);

	/* Configure vdev params */
	ucfg_fwol_configure_vdev_params(hdd_ctx->psoc, hdd_ctx->pdev,
					adapter->device_mode, adapter->vdev_id);

	hdd_nofl_debug("vdev %d created successfully", adapter->vdev_id);

	return errno;

hdd_vdev_destroy_procedure:
	QDF_BUG(!hdd_vdev_destroy(adapter));

	return errno;
}

#define MAX_VDEV_RTT_PARAMS 2
/* params being sent:
 * wmi_vdev_param_enable_disable_rtt_responder_role
 * wmi_vdev_param_enable_disable_rtt_initiator_role
 */
QDF_STATUS hdd_init_station_mode(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx = &adapter->session.station;
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;
	int ret_val;
	mac_handle_t mac_handle;
	uint8_t enable_sifs_burst = 0;
	uint32_t fine_time_meas_cap = 0, roam_triggers;
	struct wlan_objmgr_vdev *vdev;
	struct dev_set_param vdevsetparam[MAX_VDEV_RTT_PARAMS] = {};
	uint8_t index = 0;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	mac_handle = hdd_ctx->mac_handle;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_INIT_DEINIT_ID);
	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	hdd_vdev_set_ht_vht_ies(mac_handle, vdev);
	hdd_roam_profile_init(adapter);
	hdd_register_wext(adapter->dev);

	/* Set the default operation channel freq*/
	sta_ctx->conn_info.chan_freq = hdd_ctx->config->operating_chan_freq;

	/* Make the default Auth Type as OPEN */
	sta_ctx->conn_info.auth_type = eCSR_AUTH_TYPE_OPEN_SYSTEM;

	status = ucfg_dp_init_txrx(vdev);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("ucfg_dp_init_txrx() failed, status code %d", status);
		goto error_init_txrx;
	}

	set_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);

	status = hdd_wmm_adapter_init(adapter);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("hdd_wmm_adapter_init() failed, status code %08d [x%08x]",
			status, status);
		goto error_wmm_init;
	}

	set_bit(WMM_INIT_DONE, &adapter->event_flags);

	status = ucfg_get_enable_sifs_burst(hdd_ctx->psoc, &enable_sifs_burst);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to get sifs burst value, use default");
	ret_val = sme_cli_set_command(adapter->vdev_id,
				      wmi_pdev_param_burst_enable,
				      enable_sifs_burst,
				      PDEV_CMD);
	if (ret_val) {
		hdd_err("wmi_pdev_param_burst_enable set failed: %d", ret_val);
		status = QDF_STATUS_E_FAILURE;
		goto error_wmm_init;
	}

	hdd_set_netdev_flags(adapter);

	/* rcpi info initialization */
	qdf_mem_zero(&adapter->rcpi, sizeof(adapter->rcpi));

	if (adapter->device_mode == QDF_STA_MODE) {
		roam_triggers = ucfg_mlme_get_roaming_triggers(hdd_ctx->psoc);
		mlme_set_roam_trigger_bitmap(hdd_ctx->psoc, adapter->vdev_id,
					     roam_triggers);
		ucfg_mlme_get_fine_time_meas_cap(hdd_ctx->psoc,
						 &fine_time_meas_cap);
		status = mlme_check_index_setparam(vdevsetparam,
			wmi_vdev_param_enable_disable_rtt_responder_role,
			(fine_time_meas_cap & WMI_FW_STA_RTT_RESPR), index++,
			MAX_VDEV_RTT_PARAMS);
		if (QDF_IS_STATUS_ERROR(status))
			goto error_wmm_init;

		status = mlme_check_index_setparam(vdevsetparam,
			wmi_vdev_param_enable_disable_rtt_initiator_role,
			(fine_time_meas_cap & WMI_FW_STA_RTT_INITR), index++,
			MAX_VDEV_RTT_PARAMS);
		if (QDF_IS_STATUS_ERROR(status))
			goto error_wmm_init;

		status = sme_send_multi_pdev_vdev_set_params(MLME_VDEV_SETPARAM,
							     adapter->vdev_id,
							     vdevsetparam,
							     index);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("failed to set RTT_RESPONDER,INITIATOR params:%d", status);
			goto error_wmm_init;
		}
	}

	hdd_objmgr_put_vdev_by_user(vdev, WLAN_INIT_DEINIT_ID);
	return QDF_STATUS_SUCCESS;

error_wmm_init:
	clear_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);
	ucfg_dp_deinit_txrx(vdev);
error_init_txrx:
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_INIT_DEINIT_ID);
	hdd_unregister_wext(adapter->dev);
	QDF_BUG(!hdd_vdev_destroy(adapter));

	return status;
}

static char *net_dev_ref_debug_string_from_id(wlan_net_dev_ref_dbgid dbgid)
{
	static const char *strings[] = {
		"NET_DEV_HOLD_ID_RESERVED",
		"NET_DEV_HOLD_GET_STA_CONNECTION_IN_PROGRESS",
		"NET_DEV_HOLD_CHECK_DFS_CHANNEL_FOR_ADAPTER",
		"NET_DEV_HOLD_GET_SAP_OPERATING_BAND",
		"NET_DEV_HOLD_RECOVERY_NOTIFIER_CALL",
		"NET_DEV_HOLD_IS_ANY_STA_CONNECTING",
		"NET_DEV_HOLD_SAP_DESTROY_CTX_ALL",
		"NET_DEV_HOLD_DRV_CMD_MAX_TX_POWER",
		"NET_DEV_HOLD_IPA_SET_TX_FLOW_INFO",
		"NET_DEV_HOLD_SET_RPS_CPU_MASK",
		"NET_DEV_HOLD_DFS_INDICATE_RADAR",
		"NET_DEV_HOLD_MAX_STA_INTERFACE_UP_COUNT_REACHED",
		"NET_DEV_HOLD_IS_CHAN_SWITCH_IN_PROGRESS",
		"NET_DEV_HOLD_STA_DESTROY_CTX_ALL",
		"NET_DEV_HOLD_CHECK_FOR_EXISTING_MACADDR",
		"NET_DEV_HOLD_DEINIT_ALL_ADAPTERS",
		"NET_DEV_HOLD_STOP_ALL_ADAPTERS",
		"NET_DEV_HOLD_RESET_ALL_ADAPTERS",
		"NET_DEV_HOLD_IS_ANY_INTERFACE_OPEN",
		"NET_DEV_HOLD_START_ALL_ADAPTERS",
		"NET_DEV_HOLD_GET_ADAPTER_BY_RAND_MACADDR",
		"NET_DEV_HOLD_GET_ADAPTER_BY_MACADDR",
		"NET_DEV_HOLD_GET_ADAPTER_BY_VDEV",
		"NET_DEV_HOLD_ADAPTER_GET_BY_REFERENCE",
		"NET_DEV_HOLD_GET_ADAPTER_BY_IFACE_NAME",
		"NET_DEV_HOLD_GET_ADAPTER",
		"NET_DEV_HOLD_GET_OPERATING_CHAN_FREQ",
		"NET_DEV_HOLD_UNREGISTER_WEXT_ALL_ADAPTERS",
		"NET_DEV_HOLD_ABORT_MAC_SCAN_ALL_ADAPTERS",
		"NET_DEV_HOLD_ABORT_SCHED_SCAN_ALL_ADAPTERS",
		"NET_DEV_HOLD_GET_FIRST_VALID_ADAPTER",
		"NET_DEV_HOLD_CLEAR_RPS_CPU_MASK",
		"NET_DEV_HOLD_BUS_BW_WORK_HANDLER",
		"NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY_COMPACT",
		"NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY",
		"NET_DEV_HOLD_CLEAR_NETIF_QUEUE_HISTORY",
		"NET_DEV_HOLD_UNSAFE_CHANNEL_RESTART_SAP",
		"NET_DEV_HOLD_INDICATE_MGMT_FRAME",
		"NET_DEV_HOLD_STATE_INFO_DUMP",
		"NET_DEV_HOLD_DISABLE_ROAMING",
		"NET_DEV_HOLD_ENABLE_ROAMING",
		"NET_DEV_HOLD_AUTO_SHUTDOWN_ENABLE",
		"NET_DEV_HOLD_GET_CON_SAP_ADAPTER",
		"NET_DEV_HOLD_IS_ANY_ADAPTER_CONNECTED",
		"NET_DEV_HOLD_IS_ROAMING_IN_PROGRESS",
		"NET_DEV_HOLD_DEL_P2P_INTERFACE",
		"NET_DEV_HOLD_IS_NDP_ALLOWED",
		"NET_DEV_HOLD_NDI_OPEN",
		"NET_DEV_HOLD_SEND_OEM_REG_RSP_NLINK_MSG",
		"NET_DEV_HOLD_PERIODIC_STA_STATS_DISPLAY",
		"NET_DEV_HOLD_SUSPEND_WLAN",
		"NET_DEV_HOLD_RESUME_WLAN",
		"NET_DEV_HOLD_SSR_RESTART_SAP",
		"NET_DEV_HOLD_SEND_DEFAULT_SCAN_IES",
		"NET_DEV_HOLD_CFG80211_SUSPEND_WLAN",
		"NET_DEV_HOLD_COUNTRY_CHANGE_UPDATE_STA",
		"NET_DEV_HOLD_COUNTRY_CHANGE_UPDATE_SAP",
		"NET_DEV_HOLD_CACHE_STATION_STATS_CB",
		"NET_DEV_HOLD_DISPLAY_TXRX_STATS",
		"NET_DEV_HOLD_START_PRE_CAC_TRANS",
		"NET_DEV_HOLD_IS_ANY_STA_CONNECTED",
		"NET_DEV_HOLD_GET_ADAPTER_BY_BSSID",
		"NET_DEV_HOLD_ID_MAX"};
	int32_t num_dbg_strings = QDF_ARRAY_SIZE(strings);

	if (dbgid >= num_dbg_strings) {
		char *ret = "";

		hdd_err("Debug string not found for debug id %d", dbgid);
		return ret;
	}

	return (char *)strings[dbgid];
}

void hdd_check_for_net_dev_ref_leak(struct hdd_adapter *adapter)
{
	int i, id;

	for (id = 0; id < NET_DEV_HOLD_ID_MAX; id++) {
		for (i = 0; i < MAX_NET_DEV_REF_LEAK_ITERATIONS; i++) {
			if (!qdf_atomic_read(
				&adapter->net_dev_hold_ref_count[id]))
				break;
			hdd_info("net_dev held for debug id %s",
				 net_dev_ref_debug_string_from_id(id));
			qdf_sleep(NET_DEV_REF_LEAK_ITERATION_SLEEP_TIME_MS);
		}
		if (i == MAX_NET_DEV_REF_LEAK_ITERATIONS) {
			hdd_err("net_dev hold reference leak detected for debug id: %s",
				net_dev_ref_debug_string_from_id(id));
			QDF_BUG(0);
		}
	}
}

/**
 * hdd_deinit_station_mode() - De-initialize the station adapter
 * @hdd_ctx: global hdd context
 * @adapter: HDD adapter
 * @rtnl_held: Used to indicate whether or not the caller is holding
 *             the kernel rtnl_mutex
 *
 * This function De-initializes the STA/P2P/OCB adapter.
 *
 * Return: None.
 */
static void hdd_deinit_station_mode(struct hdd_context *hdd_ctx,
				       struct hdd_adapter *adapter,
				       bool rtnl_held)
{
	hdd_enter_dev(adapter->dev);

	if (test_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags)) {
		clear_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);
	}

	if (test_bit(WMM_INIT_DONE, &adapter->event_flags)) {
		hdd_wmm_adapter_close(adapter);
		clear_bit(WMM_INIT_DONE, &adapter->event_flags);
	}


	hdd_exit();
}

void hdd_deinit_adapter(struct hdd_context *hdd_ctx,
			struct hdd_adapter *adapter,
			bool rtnl_held)
{
	hdd_enter();

	hdd_wext_unregister(adapter->dev, rtnl_held);

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_MONITOR_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_NDI_MODE:
	case QDF_NAN_DISC_MODE:
	{
		hdd_deinit_station_mode(hdd_ctx, adapter, rtnl_held);
		break;
	}

	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
	{
		hdd_deinit_ap_mode(hdd_ctx, adapter, rtnl_held);
		break;
	}

	default:
		break;
	}

	if (adapter->scan_info.default_scan_ies) {
		qdf_mem_free(adapter->scan_info.default_scan_ies);
		adapter->scan_info.default_scan_ies = NULL;
		adapter->scan_info.default_scan_ies_len = 0;
	}

	hdd_exit();
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)) && \
     defined(WLAN_FEATURE_11AX)
/**
 * hdd_cleanup_he_operation_info() - cleanup he operation info
 * @adapter: Adapter structure
 *
 * This function destroys he operation information
 *
 * Return: none
 */
static void hdd_cleanup_he_operation_info(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *hdd_sta_ctx =
					WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	if (!hdd_sta_ctx) {
		hdd_err("sta ctx in NULL");
		return;
	}
	hdd_debug("cleanup he operation info");

	if (hdd_sta_ctx->cache_conn_info.he_operation) {
		qdf_mem_free(hdd_sta_ctx->cache_conn_info.he_operation);
		hdd_sta_ctx->cache_conn_info.he_operation = NULL;
	}
}
#else
static inline void hdd_cleanup_he_operation_info(struct hdd_adapter *adapter)
{
}
#endif

/**
 * hdd_cleanup_prev_ap_bcn_ie() - cleanup previous ap beacon ie
 * @adapter: Adapter structure
 *
 * This function destroys previous ap beacon information
 *
 * Return: none
 */
static void hdd_cleanup_prev_ap_bcn_ie(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *hdd_sta_ctx =
					WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct element_info *bcn_ie;

	if (!hdd_sta_ctx) {
		hdd_err("sta ctx in NULL");
		return;
	}
	hdd_debug("cleanup previous ap bcn ie");

	bcn_ie = &hdd_sta_ctx->conn_info.prev_ap_bcn_ie;

	if (bcn_ie->ptr) {
		qdf_mem_free(bcn_ie->ptr);
		bcn_ie->ptr = NULL;
		bcn_ie->len = 0;
	}
}

void hdd_cleanup_conn_info(struct hdd_adapter *adapter)
{
	hdd_cleanup_he_operation_info(adapter);
	hdd_cleanup_prev_ap_bcn_ie(adapter);
}

/**
 * hdd_sta_destroy_ctx_all() - cleanup all station contexts
 * @hdd_ctx: Global HDD context
 *
 * This function destroys all the station contexts
 *
 * Return: none
 */
static void hdd_sta_destroy_ctx_all(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_STA_DESTROY_CTX_ALL) {
		if (adapter->device_mode == QDF_STA_MODE)
			hdd_cleanup_conn_info(adapter);
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_STA_DESTROY_CTX_ALL);
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
static void
hdd_unregister_netdevice(struct hdd_adapter *adapter, struct net_device *dev)
{
	if (adapter->is_virtual_iface) {
		wlan_cfg80211_unregister_netdevice(dev);
		adapter->is_virtual_iface = false;
	} else {
		unregister_netdevice(dev);
	}
}
#else
static void
hdd_unregister_netdevice(struct hdd_adapter *adapter, struct net_device *dev)
{
	unregister_netdevice(dev);
}
#endif

static void hdd_cleanup_adapter(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter,
				bool rtnl_held)
{
	struct net_device *dev = NULL;

	if (adapter)
		dev = adapter->dev;
	else {
		hdd_err("adapter is Null");
		return;
	}

	hdd_apf_context_destroy(adapter);
	qdf_spinlock_destroy(&adapter->vdev_lock);
	hdd_sta_info_deinit(&adapter->sta_info_list);
	hdd_sta_info_deinit(&adapter->cache_sta_info_list);

	wlan_hdd_debugfs_csr_deinit(adapter);

	hdd_debugfs_exit(adapter);

	/*
	 * The adapter is marked as closed. When hdd_wlan_exit() call returns,
	 * the driver is almost closed and cannot handle either control
	 * messages or data. However, unregister_netdevice() call above will
	 * eventually invoke hdd_stop(ndo_close) driver callback, which attempts
	 * to close the active connections(basically excites control path) which
	 * is not right. Setting this flag helps hdd_stop() to recognize that
	 * the interface is closed and restricts any operations on that
	 */
	clear_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);

	if (test_bit(NET_DEVICE_REGISTERED, &adapter->event_flags)) {
		if (rtnl_held)
			hdd_unregister_netdevice(adapter, dev);
		else
			unregister_netdev(dev);
		/*
		 * Note that the adapter is no longer valid at this point
		 * since the memory has been reclaimed
		 */
	}
}

static QDF_STATUS hdd_check_for_existing_macaddr(struct hdd_context *hdd_ctx,
						 tSirMacAddr mac_addr)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_CHECK_FOR_EXISTING_MACADDR;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (!qdf_mem_cmp(adapter->mac_addr.bytes,
				 mac_addr, sizeof(tSirMacAddr))) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return QDF_STATUS_E_FAILURE;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_configure_chain_mask() - programs chain mask to firmware
 * @adapter: HDD adapter
 *
 * Return: 0 on success or errno on failure
 */
static int hdd_configure_chain_mask(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	status = ucfg_mlme_configure_chain_mask(hdd_ctx->psoc,
						adapter->vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	return 0;

error:
	hdd_debug("WMI PDEV set param failed");
	return -EINVAL;
}

/**
 * hdd_send_coex_config_params() - Send coex config params to FW
 * @hdd_ctx: HDD context
 * @adapter: Primary adapter context
 *
 * This function is used to send all coex config related params to FW
 *
 * Return: 0 on success and -EINVAL on failure
 */
static int hdd_send_coex_config_params(struct hdd_context *hdd_ctx,
				       struct hdd_adapter *adapter)
{
	struct coex_config_params coex_cfg_params = {0};
	struct wlan_fwol_coex_config config = {0};
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	enum coex_btc_chain_mode btc_chain_mode;
	QDF_STATUS status;

	if (!adapter) {
		hdd_err("adapter is invalid");
		goto err;
	}

	if (!psoc) {
		hdd_err("HDD psoc is invalid");
		goto err;
	}

	status = ucfg_fwol_get_coex_config_params(psoc, &config);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to get coex config params");
		goto err;
	}

	coex_cfg_params.vdev_id = adapter->vdev_id;
	coex_cfg_params.config_type = WMI_COEX_CONFIG_TX_POWER;
	coex_cfg_params.config_arg1 = config.max_tx_power_for_btc;

	wma_nofl_debug("TXP[W][send_coex_cfg]: %d",
		       config.max_tx_power_for_btc);

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex Tx power");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_HANDOVER_RSSI;
	coex_cfg_params.config_arg1 = config.wlan_low_rssi_threshold;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex handover RSSI");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_BTC_MODE;

	/* Modify BTC_MODE according to BTC_CHAIN_MODE */
	status = ucfg_coex_psoc_get_btc_chain_mode(psoc, &btc_chain_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get btc chain mode");
		btc_chain_mode = WLAN_COEX_BTC_CHAIN_MODE_UNSETTLED;
	}

	if (btc_chain_mode <= WLAN_COEX_BTC_CHAIN_MODE_HYBRID)
		coex_cfg_params.config_arg1 = btc_chain_mode;
	else
		coex_cfg_params.config_arg1 = config.btc_mode;

	hdd_debug("Configured BTC mode is %d, BTC chain mode is 0x%x, set BTC mode to %d",
		  config.btc_mode, btc_chain_mode,
		  coex_cfg_params.config_arg1);
	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BTC mode");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_ANTENNA_ISOLATION;
	coex_cfg_params.config_arg1 = config.antenna_isolation;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex antenna isolation");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_BT_LOW_RSSI_THRESHOLD;
	coex_cfg_params.config_arg1 = config.bt_low_rssi_threshold;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BT low RSSI threshold");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_BT_INTERFERENCE_LEVEL;
	coex_cfg_params.config_arg1 = config.bt_interference_low_ll;
	coex_cfg_params.config_arg2 = config.bt_interference_low_ul;
	coex_cfg_params.config_arg3 = config.bt_interference_medium_ll;
	coex_cfg_params.config_arg4 = config.bt_interference_medium_ul;
	coex_cfg_params.config_arg5 = config.bt_interference_high_ll;
	coex_cfg_params.config_arg6 = config.bt_interference_high_ul;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BT interference level");
		goto err;
	}

	if (wlan_hdd_mpta_helper_enable(&coex_cfg_params, &config))
		goto err;

	coex_cfg_params.config_type =
				WMI_COEX_CONFIG_BT_SCO_ALLOW_WLAN_2G_SCAN;
	coex_cfg_params.config_arg1 = config.bt_sco_allow_wlan_2g_scan;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BT sco allow wlan 2g scan");
		goto err;
	}

	coex_cfg_params.config_type =
				WMI_COEX_CONFIG_LE_SCAN_POLICY;
	coex_cfg_params.config_arg1 = config.ble_scan_coex_policy;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BLE scan policy");
		goto err;
	}

#ifdef FEATURE_COEX_TPUT_SHAPING_CONFIG
	coex_cfg_params.config_type =
				WMI_COEX_CONFIG_ENABLE_TPUT_SHAPING;
	coex_cfg_params.config_arg1 = config.coex_tput_shaping_enable;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex traffic shaping value %d",
			coex_cfg_params.config_arg1);
		goto err;
	}
#endif
	return 0;
err:
	return -EINVAL;
}

#define MAX_PDEV_SET_FW_PARAMS 7
/* params being sent:
 * 1.wmi_pdev_param_dtim_synth
 * 2.wmi_pdev_param_1ch_dtim_optimized_chain_selection
 * 3.wmi_pdev_param_tx_sch_delay
 * 4.wmi_pdev_param_en_update_scram_seed
 * 5.wmi_pdev_param_secondary_retry_enable
 * 6.wmi_pdev_param_set_sap_xlna_bypass
 * 7.wmi_pdev_param_set_dfs_chan_ageout_time
 */

/**
 * hdd_set_fw_params() - Set parameters to firmware
 * @adapter: HDD adapter
 *
 * This function Sets various parameters to fw once the
 * adapter is started.
 *
 * Return: 0 on success or errno on failure
 */
int hdd_set_fw_params(struct hdd_adapter *adapter)
{
	int ret;
	uint16_t upper_brssi_thresh, lower_brssi_thresh, rts_profile;
	bool enable_dtim_1chrx;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx;
	bool is_lprx_enabled;
	bool bval = false;
	uint8_t enable_tx_sch_delay, dfs_chan_ageout_time;
	uint32_t dtim_sel_diversity, enable_secondary_rate;
	bool sap_xlna_bypass;
	bool enable_ofdm_scrambler_seed = false;
	struct dev_set_param setparam[MAX_PDEV_SET_FW_PARAMS] = { };
	uint8_t index = 0;

	hdd_enter_dev(adapter->dev);

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return -EINVAL;

	if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_debug("FTM Mode is active; nothing to do");
		return 0;
	}

	/* The ini gEnableLPRx is deprecated. By default, the ini
	 * is enabled. So, making the variable is_lprx_enabled true.
	 */
	is_lprx_enabled = true;

	ret = mlme_check_index_setparam(setparam, wmi_pdev_param_dtim_synth,
					is_lprx_enabled, index++,
					MAX_PDEV_SET_FW_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret))
		goto error;

	ucfg_mlme_get_dtim_selection_diversity(hdd_ctx->psoc,
					       &dtim_sel_diversity);
	ret = mlme_check_index_setparam(
			setparam,
			wmi_pdev_param_1ch_dtim_optimized_chain_selection,
			dtim_sel_diversity, index++, MAX_PDEV_SET_FW_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret))
		goto error;

	if (QDF_IS_STATUS_SUCCESS(ucfg_fwol_get_enable_tx_sch_delay(
				  hdd_ctx->psoc, &enable_tx_sch_delay))) {
		ret = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_tx_sch_delay,
					      enable_tx_sch_delay, index++,
					      MAX_PDEV_SET_FW_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret))
			goto error;
	}

	if (QDF_IS_STATUS_SUCCESS(ucfg_fwol_get_ofdm_scrambler_seed(
				hdd_ctx->psoc, &enable_ofdm_scrambler_seed))) {
		ret = mlme_check_index_setparam(
					setparam,
					wmi_pdev_param_en_update_scram_seed,
					enable_ofdm_scrambler_seed, index++,
					MAX_PDEV_SET_FW_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret))
			goto error;
	}

	if (QDF_IS_STATUS_SUCCESS(ucfg_fwol_get_enable_secondary_rate(
				  hdd_ctx->psoc, &enable_secondary_rate))) {
		ret = mlme_check_index_setparam(
					setparam,
					wmi_pdev_param_secondary_retry_enable,
					enable_secondary_rate, index++,
					MAX_PDEV_SET_FW_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret))
			goto error;
	}
	if (QDF_IS_STATUS_SUCCESS(ucfg_fwol_get_sap_xlna_bypass(
				  hdd_ctx->psoc, &sap_xlna_bypass))) {
		ret = mlme_check_index_setparam(
					setparam,
					wmi_pdev_param_set_sap_xlna_bypass,
					sap_xlna_bypass, index++,
					MAX_PDEV_SET_FW_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret))
			goto error;
	}
	wlan_mlme_get_dfs_chan_ageout_time(hdd_ctx->psoc,
					   &dfs_chan_ageout_time);
	ret = mlme_check_index_setparam(
				      setparam,
				      wmi_pdev_param_set_dfs_chan_ageout_time,
				      dfs_chan_ageout_time, index++,
				      MAX_PDEV_SET_FW_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret))
		goto error;

	ret = sme_send_multi_pdev_vdev_set_params(MLME_PDEV_SETPARAM,
						  WMI_PDEV_ID_SOC, setparam,
						  index);
	if (QDF_IS_STATUS_ERROR(ret)) {
		goto error;
	}
	if (adapter->device_mode == QDF_STA_MODE) {
		status = ucfg_get_upper_brssi_thresh(hdd_ctx->psoc,
						     &upper_brssi_thresh);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;

		sme_set_smps_cfg(adapter->vdev_id,
				 HDD_STA_SMPS_PARAM_UPPER_BRSSI_THRESH,
				 upper_brssi_thresh);

		status = ucfg_get_lower_brssi_thresh(hdd_ctx->psoc,
						     &lower_brssi_thresh);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;

		sme_set_smps_cfg(adapter->vdev_id,
				 HDD_STA_SMPS_PARAM_LOWER_BRSSI_THRESH,
				 lower_brssi_thresh);

		status = ucfg_get_enable_dtim_1chrx(hdd_ctx->psoc,
						    &enable_dtim_1chrx);
		if (QDF_IS_STATUS_ERROR(status))
			return -EINVAL;

		sme_set_smps_cfg(adapter->vdev_id,
				 HDD_STA_SMPS_PARAM_DTIM_1CHRX_ENABLE,
				 enable_dtim_1chrx);
	}

	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &bval);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get vht_enable2x2");

	if (bval) {
		hdd_debug("configuring 2x2 mode fw params");

		ret = sme_set_cck_tx_fir_override(hdd_ctx->mac_handle,
						  adapter->vdev_id);
		if (ret) {
			hdd_err("wmi_pdev_param_enable_cck_tfir_override set failed %d",
				ret);
			goto error;
		}

		hdd_configure_chain_mask(adapter);
	} else {
#define HDD_DTIM_1CHAIN_RX_ID 0x5
#define HDD_SMPS_PARAM_VALUE_S 29
		hdd_debug("configuring 1x1 mode fw params");

		/*
		 * Disable DTIM 1 chain Rx when in 1x1,
		 * we are passing two value
		 * as param_id << 29 | param_value.
		 * Below param_value = 0(disable)
		 */
		ret = sme_cli_set_command(adapter->vdev_id,
					  WMI_STA_SMPS_PARAM_CMDID,
					  HDD_DTIM_1CHAIN_RX_ID <<
					  HDD_SMPS_PARAM_VALUE_S,
					  VDEV_CMD);
		if (ret) {
			hdd_err("DTIM 1 chain set failed %d", ret);
			goto error;
		}

#undef HDD_DTIM_1CHAIN_RX_ID
#undef HDD_SMPS_PARAM_VALUE_S

		hdd_configure_chain_mask(adapter);
	}

	ret = sme_set_enable_mem_deep_sleep(hdd_ctx->mac_handle,
					    adapter->vdev_id);
	if (ret) {
		hdd_err("wmi_pdev_param_hyst_en set failed %d", ret);
		goto error;
	}

	status = ucfg_fwol_get_rts_profile(hdd_ctx->psoc, &rts_profile);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	ret = sme_cli_set_command(adapter->vdev_id,
				  wmi_vdev_param_enable_rtscts,
				  rts_profile,
				  VDEV_CMD);
	if (ret) {
		hdd_err("FAILED TO SET RTSCTS Profile ret:%d", ret);
		goto error;
	}

	if (!hdd_ctx->is_fw_dbg_log_levels_configured) {
		hdd_set_fw_log_params(hdd_ctx, adapter->vdev_id);
		hdd_ctx->is_fw_dbg_log_levels_configured = true;
	}

	ret = hdd_send_coex_config_params(hdd_ctx, adapter);
	if (ret) {
		hdd_warn("Error initializing coex config params");
		goto error;
	}

	hdd_exit();

	return 0;

error:
	return -EINVAL;
}

/**
 * hdd_init_completion() - Initialize Completion Variables
 * @adapter: HDD adapter
 *
 * This function Initialize the completion variables for
 * a particular adapter
 *
 * Return: None
 */
static void hdd_init_completion(struct hdd_adapter *adapter)
{
	init_completion(&adapter->disconnect_comp_var);
	init_completion(&adapter->linkup_event_var);
	init_completion(&adapter->sta_authorized_event);
	init_completion(&adapter->lfr_fw_status.disable_lfr_event);
}

static void hdd_reset_locally_admin_bit(struct hdd_context *hdd_ctx,
					tSirMacAddr mac_addr)
{
	int i;
	/*
	 * Reset locally administered bit for dynamic_mac_list
	 * also as while releasing the MAC address for any
	 * interface mac will be compared with dynamic mac list
	 */
	for (i = 0; i < QDF_MAX_CONCURRENCY_PERSONA; i++) {
		if (!qdf_mem_cmp(
			mac_addr,
			 &hdd_ctx->
				dynamic_mac_list[i].dynamic_mac.bytes[0],
				sizeof(struct qdf_mac_addr))) {
			WLAN_HDD_RESET_LOCALLY_ADMINISTERED_BIT(
				hdd_ctx->
					dynamic_mac_list[i].dynamic_mac.bytes);
			break;
		}
	}
	/*
	 * Reset locally administered bit if the device mode is
	 * STA
	 */
	WLAN_HDD_RESET_LOCALLY_ADMINISTERED_BIT(mac_addr);
	hdd_debug("locally administered bit reset in sta mode: "
		 QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(mac_addr));
}

static void wlan_hdd_cfg80211_scan_block_cb(struct work_struct *work)
{
	struct hdd_adapter *adapter =
		container_of(work, struct hdd_adapter, scan_block_work);
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(adapter->dev, &vdev_sync))
		return;

	wlan_hdd_cfg80211_scan_block(adapter);

	osif_vdev_sync_op_stop(vdev_sync);
}

#ifdef WLAN_FEATURE_11BE_MLO
static inline void
wlan_hdd_set_ml_cap_for_sap_intf(struct hdd_adapter_create_param *create_params,
				 enum QDF_OPMODE mode)
{
	if (mode != QDF_SAP_MODE)
		return;

	create_params->is_single_link = true;
	create_params->is_ml_adapter = true;
}
#else
static inline void
wlan_hdd_set_ml_cap_for_sap_intf(struct hdd_adapter_create_param *create_params,
				 enum QDF_OPMODE mode)
{
}
#endif

/**
 * hdd_open_adapter() - open and setup the hdd adapter
 * @hdd_ctx: global hdd context
 * @session_type: type of the interface to be created
 * @iface_name: User-visible name of the interface
 * @mac_addr: MAC address to assign to the interface
 * @name_assign_type: the name of assign type of the netdev
 * @rtnl_held: the rtnl lock hold flag
 * @params: adapter create params
 *
 * This function open and setup the hdd adapter according to the device
 * type request, assign the name, the mac address assigned, and then prepared
 * the hdd related parameters, queue, lock and ready to start.
 *
 * Return: the pointer of hdd adapter, otherwise NULL.
 */
struct hdd_adapter *hdd_open_adapter(struct hdd_context *hdd_ctx,
				     uint8_t session_type,
				     const char *iface_name,
				     tSirMacAddr mac_addr,
				     unsigned char name_assign_type,
				     bool rtnl_held,
				     struct hdd_adapter_create_param *params)
{
	struct net_device *ndev = NULL;
	struct hdd_adapter *adapter = NULL, *sta_adapter = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t i;
	bool eht_capab = 0;

	status = wlan_hdd_validate_mac_address((struct qdf_mac_addr *)mac_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		/* Not received valid mac_addr */
		hdd_err("Unable to add virtual intf: Not able to get valid mac address");
		return NULL;
	}

	status = hdd_check_for_existing_macaddr(hdd_ctx, mac_addr);
	if (QDF_STATUS_E_FAILURE == status) {
		hdd_err("Duplicate MAC addr: " QDF_MAC_ADDR_FMT
				" already exists",
				QDF_MAC_ADDR_REF(mac_addr));
		return NULL;
	}

	if (params->only_wdev_register) {
		sta_adapter = hdd_get_ml_adapter(hdd_ctx);
		if (!sta_adapter) {
			hdd_err("not able to find the sta adapter");
			return NULL;
		}
	}

	switch (session_type) {
	case QDF_STA_MODE:
		if (!hdd_ctx->config->mac_provision) {
			hdd_reset_locally_admin_bit(hdd_ctx, mac_addr);
			/*
			 * After resetting locally administered bit
			 * again check if the new mac address is already
			 * exists.
			 */
			status = hdd_check_for_existing_macaddr(hdd_ctx,
								mac_addr);
			if (QDF_STATUS_E_FAILURE == status) {
				hdd_err("Duplicate MAC addr: " QDF_MAC_ADDR_FMT
					" already exists",
					QDF_MAC_ADDR_REF(mac_addr));
				return NULL;
			}
		}

		fallthrough;
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_OCB_MODE:
	case QDF_NDI_MODE:
	case QDF_MONITOR_MODE:
	case QDF_NAN_DISC_MODE:
		adapter = hdd_alloc_station_adapter(hdd_ctx, mac_addr,
						    name_assign_type,
						    iface_name, session_type);

		if (!adapter) {
			hdd_err("failed to allocate adapter for session %d",
					session_type);
			return NULL;
		}

		ndev = adapter->dev;

		status = ucfg_dp_create_intf(hdd_ctx->psoc, &adapter->mac_addr,
					     (qdf_netdev_t)adapter->dev);
		if (QDF_IS_STATUS_ERROR(status))
			goto err_free_netdev;

		if (QDF_P2P_CLIENT_MODE == session_type)
			adapter->wdev.iftype = NL80211_IFTYPE_P2P_CLIENT;
		else if (QDF_P2P_DEVICE_MODE == session_type)
			adapter->wdev.iftype = NL80211_IFTYPE_P2P_DEVICE;
		else if (QDF_MONITOR_MODE == session_type)
			adapter->wdev.iftype = NL80211_IFTYPE_MONITOR;
		else if (QDF_NAN_DISC_MODE == session_type)
			wlan_hdd_set_nan_if_type(adapter);
		else
			adapter->wdev.iftype = NL80211_IFTYPE_STATION;

		adapter->device_mode = session_type;


		/*
		 * Workqueue which gets scheduled in IPv4 notification
		 * callback
		 */
		INIT_WORK(&adapter->ipv4_notifier_work,
			  hdd_ipv4_notifier_work_queue);

#ifdef WLAN_NS_OFFLOAD
		/*
		 * Workqueue which gets scheduled in IPv6
		 * notification callback.
		 */
		INIT_WORK(&adapter->ipv6_notifier_work,
			  hdd_ipv6_notifier_work_queue);
#endif
		if (params->only_wdev_register) {
			hdd_register_wdev(sta_adapter, adapter, params);
		} else {
			status = hdd_register_interface(adapter, rtnl_held,
							params);
			if (QDF_STATUS_SUCCESS != status)
				goto err_destroy_dp_intf;
			/* Stop the Interface TX queue. */
			hdd_debug("Disabling queues");
			wlan_hdd_netif_queue_control(adapter,
					WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);
		}
		break;
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
		adapter = hdd_wlan_create_ap_dev(hdd_ctx, mac_addr,
						 name_assign_type,
						 (uint8_t *) iface_name);
		if (!adapter) {
			hdd_err("failed to allocate adapter for session %d",
					  session_type);
			return NULL;
		}

		ndev = adapter->dev;

		status = ucfg_dp_create_intf(hdd_ctx->psoc, &adapter->mac_addr,
					     (qdf_netdev_t)adapter->dev);
		if (QDF_IS_STATUS_ERROR(status))
			goto err_free_netdev;

		adapter->wdev.iftype =
			(session_type ==
			 QDF_SAP_MODE) ? NL80211_IFTYPE_AP :
			NL80211_IFTYPE_P2P_GO;
		adapter->device_mode = session_type;

		status = hdd_register_interface(adapter, rtnl_held, params);
		if (QDF_STATUS_SUCCESS != status)
			goto err_destroy_dp_intf;

		hdd_debug("Disabling queues");
		wlan_hdd_netif_queue_control(adapter,
					WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);

		/*
		 * Workqueue which gets scheduled in IPv4 notification
		 * callback
		 */
		INIT_WORK(&adapter->ipv4_notifier_work,
			  hdd_ipv4_notifier_work_queue);

#ifdef WLAN_NS_OFFLOAD
		/*
		 * Workqueue which gets scheduled in IPv6
		 * notification callback.
		 */
		INIT_WORK(&adapter->ipv6_notifier_work,
			  hdd_ipv6_notifier_work_queue);
#endif
		wlan_hdd_set_ml_cap_for_sap_intf(params, session_type);

		break;
	case QDF_FTM_MODE:
		adapter = hdd_alloc_station_adapter(hdd_ctx, mac_addr,
						    name_assign_type,
						    iface_name, session_type);
		if (!adapter) {
			hdd_err("Failed to allocate adapter for FTM mode");
			return NULL;
		}

		ndev = adapter->dev;

		status = ucfg_dp_create_intf(hdd_ctx->psoc, &adapter->mac_addr,
					     (qdf_netdev_t)adapter->dev);
		if (QDF_IS_STATUS_ERROR(status))
			goto err_free_netdev;

		adapter->wdev.iftype = NL80211_IFTYPE_STATION;
		adapter->device_mode = session_type;
		status = hdd_register_interface(adapter, rtnl_held, params);
		if (QDF_STATUS_SUCCESS != status)
			goto err_destroy_dp_intf;

		/* Stop the Interface TX queue. */
		hdd_debug("Disabling queues");
		wlan_hdd_netif_queue_control(adapter,
					WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);

		break;
	default:
		hdd_err("Invalid session type %d", session_type);
		QDF_ASSERT(0);
		return NULL;
	}

	ucfg_psoc_mlme_get_11be_capab(hdd_ctx->psoc, &eht_capab);
	if (params->is_ml_adapter && eht_capab) {
		hdd_adapter_set_ml_adapter(adapter);
		qdf_copy_macaddr(&adapter->mld_addr, &adapter->mac_addr);
		if (params->is_single_link)
			hdd_adapter_set_sl_ml_adapter(adapter);
	}

	status = hdd_adapter_feature_update_work_init(adapter);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_cleanup_adapter;

	adapter->upgrade_udp_qos_threshold = QCA_WLAN_AC_BK;
	qdf_spinlock_create(&adapter->vdev_lock);

	hdd_init_completion(adapter);
	INIT_WORK(&adapter->scan_block_work, wlan_hdd_cfg80211_scan_block_cb);
	INIT_WORK(&adapter->sap_stop_bss_work,
		  hdd_stop_sap_due_to_invalid_channel);
	qdf_list_create(&adapter->blocked_scan_request_q, WLAN_MAX_SCAN_COUNT);
	qdf_mutex_create(&adapter->blocked_scan_request_q_lock);
	qdf_event_create(&adapter->acs_complete_event);
	qdf_event_create(&adapter->peer_cleanup_done);
	hdd_sta_info_init(&adapter->sta_info_list);
	hdd_sta_info_init(&adapter->cache_sta_info_list);

	for (i = 0; i < NET_DEV_HOLD_ID_MAX; i++)
		qdf_atomic_init(
			&adapter->net_dev_hold_ref_count[NET_DEV_HOLD_ID_MAX]);

	/* Add it to the hdd's session list. */
	status = hdd_add_adapter_back(hdd_ctx, adapter);
	if (QDF_STATUS_SUCCESS != status)
		goto err_destroy_adapter_features_update_work;

	hdd_apf_context_init(adapter);

	policy_mgr_set_concurrency_mode(hdd_ctx->psoc, session_type);

	hdd_check_and_restart_sap_with_non_dfs_acs();

	if (QDF_STATUS_SUCCESS != hdd_debugfs_init(adapter))
		hdd_err("debugfs: Interface %s init failed",
			netdev_name(adapter->dev));

	hdd_debug("%s interface created. iftype: %d", netdev_name(adapter->dev),
		  session_type);

	if (adapter->device_mode == QDF_STA_MODE)
		wlan_hdd_debugfs_csr_init(adapter);

	return adapter;

err_destroy_adapter_features_update_work:
	hdd_adapter_feature_update_work_deinit(adapter);

err_cleanup_adapter:
	if (adapter) {
		hdd_cleanup_adapter(hdd_ctx, adapter, rtnl_held);
		adapter = NULL;
	}

err_destroy_dp_intf:
	ucfg_dp_destroy_intf(hdd_ctx->psoc, &adapter->mac_addr);

err_free_netdev:
	if (ndev)
		free_netdev(ndev);

	return NULL;
}

static void __hdd_close_adapter(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter,
				bool rtnl_held)
{
	struct qdf_mac_addr adapter_mac;

	qdf_copy_macaddr(&adapter_mac, &adapter->mac_addr);
	if (adapter->device_mode == QDF_STA_MODE)
		hdd_cleanup_conn_info(adapter);
	qdf_list_destroy(&adapter->blocked_scan_request_q);
	qdf_mutex_destroy(&adapter->blocked_scan_request_q_lock);
	policy_mgr_clear_concurrency_mode(hdd_ctx->psoc, adapter->device_mode);
	qdf_event_destroy(&adapter->acs_complete_event);
	qdf_event_destroy(&adapter->peer_cleanup_done);
	hdd_adapter_feature_update_work_deinit(adapter);
	hdd_cleanup_adapter(hdd_ctx, adapter, rtnl_held);
	ucfg_dp_destroy_intf(hdd_ctx->psoc, &adapter_mac);
}

void hdd_close_adapter(struct hdd_context *hdd_ctx,
		       struct hdd_adapter *adapter,
		       bool rtnl_held)
{
	/*
	 * Stop the global bus bandwidth timer while touching the adapter list
	 * to avoid bad memory access by the timer handler.
	 */
	ucfg_dp_bus_bw_compute_timer_stop(hdd_ctx->psoc);

	hdd_check_for_net_dev_ref_leak(adapter);
	hdd_remove_adapter(hdd_ctx, adapter);
	__hdd_close_adapter(hdd_ctx, adapter, rtnl_held);

	/* conditionally restart the bw timer */
	ucfg_dp_bus_bw_compute_timer_try_start(hdd_ctx->psoc);
}

void hdd_close_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held)
{
	struct hdd_adapter *adapter;
	struct osif_vdev_sync *vdev_sync;
	QDF_STATUS qdf_status;

	hdd_enter();

	while (QDF_IS_STATUS_SUCCESS(hdd_get_front_adapter(
							hdd_ctx, &adapter))) {
		/* If MLO is enabled unregister the link wdev's */
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_SAP_MODE) {
			qdf_status = hdd_wlan_unregister_mlo_interfaces(adapter,
								     rtnl_held);
			if (QDF_IS_STATUS_ERROR(qdf_status))
				continue;
		}

		hdd_check_for_net_dev_ref_leak(adapter);
		hdd_remove_front_adapter(hdd_ctx, &adapter);
		vdev_sync = osif_vdev_sync_unregister(adapter->dev);
		if (vdev_sync)
			osif_vdev_sync_wait_for_ops(vdev_sync);

		wlan_hdd_release_intf_addr(hdd_ctx, adapter->mac_addr.bytes);
		__hdd_close_adapter(hdd_ctx, adapter, rtnl_held);

		if (vdev_sync)
			osif_vdev_sync_destroy(vdev_sync);
	}

	hdd_exit();
}

void wlan_hdd_reset_prob_rspies(struct hdd_adapter *adapter)
{
	struct qdf_mac_addr *bssid = NULL;
	tSirUpdateIE update_ie;
	mac_handle_t mac_handle;

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	{
		struct hdd_station_ctx *sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		bssid = &sta_ctx->conn_info.bssid;
		break;
	}
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
	{
		bssid = &adapter->mac_addr;
		break;
	}
	case QDF_FTM_MODE:
	case QDF_P2P_DEVICE_MODE:
	default:
		/*
		 * wlan_hdd_reset_prob_rspies should not have been called
		 * for these kind of devices
		 */
		hdd_err("Unexpected request for the current device type %d",
		       adapter->device_mode);
		return;
	}

	qdf_copy_macaddr(&update_ie.bssid, bssid);
	update_ie.vdev_id = adapter->vdev_id;
	update_ie.ieBufferlength = 0;
	update_ie.pAdditionIEBuffer = NULL;
	update_ie.append = true;
	update_ie.notify = false;
	mac_handle = hdd_adapter_get_mac_handle(adapter);
	if (sme_update_add_ie(mac_handle,
			      &update_ie,
			      eUPDATE_IE_PROBE_RESP) == QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass on PROBE_RSP_BCN data to PE");
	}
}

/**
 * hdd_ipa_ap_disconnect_evt() - Indicate wlan ipa ap disconnect event
 * @hdd_ctx: hdd context
 * @adapter: hdd adapter
 *
 * Return: None
 */
static inline
void hdd_ipa_ap_disconnect_evt(struct hdd_context *hdd_ctx,
			       struct hdd_adapter *adapter)
{
	if (ucfg_ipa_is_enabled()) {
		ucfg_ipa_uc_disconnect_ap(hdd_ctx->pdev,
					  adapter->dev);
		ucfg_ipa_cleanup_dev_iface(hdd_ctx->pdev,
					   adapter->dev);
	}
}

#ifdef WLAN_FEATURE_NAN
/**
 * hdd_ndp_state_cleanup() - API to set NDP state to Disconnected
 * @psoc: pointer to psoc object
 * @ndi_vdev_id: vdev_id of the NDI
 *
 * Return: None
 */
static void
hdd_ndp_state_cleanup(struct wlan_objmgr_psoc *psoc, uint8_t ndi_vdev_id)
{
	struct wlan_objmgr_vdev *ndi_vdev;

	ndi_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, ndi_vdev_id,
							WLAN_NAN_ID);
	if (!ndi_vdev) {
		hdd_err("Cannot obtain NDI vdev object!");
		return;
	}

	ucfg_nan_set_ndi_state(ndi_vdev, NAN_DATA_DISCONNECTED_STATE);

	wlan_objmgr_vdev_release_ref(ndi_vdev, WLAN_NAN_ID);
}

/**
 * hdd_ndp_peer_cleanup() - This API will delete NDP peer if exist and modifies
 * the NDP state.
 * @hdd_ctx: hdd context
 * @adapter: hdd adapter
 *
 * Return: None
 */
static void
hdd_ndp_peer_cleanup(struct hdd_context *hdd_ctx, struct hdd_adapter *adapter)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	/* Check if there is any peer present on the adapter */
	if (!hdd_any_valid_peer_present(adapter)) {
		/*
		 * No peers are connected to the NDI. So, set the NDI state to
		 * DISCONNECTED. If there are any peers, ucfg_nan_disable_ndi()
		 * would take care of cleanup all the peers and setting the
		 * state to DISCONNECTED.
		 */
		hdd_ndp_state_cleanup(hdd_ctx->psoc, adapter->vdev_id);
		return;
	}

	if (adapter->device_mode == QDF_NDI_MODE)
		qdf_status = ucfg_nan_disable_ndi(hdd_ctx->psoc,
						  adapter->vdev_id);

	if (QDF_IS_STATUS_ERROR(qdf_status))
		return;

	qdf_status = qdf_wait_for_event_completion(&adapter->peer_cleanup_done,
						   WLAN_WAIT_PEER_CLEANUP);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		hdd_debug("peer_cleanup_done wait fail");
}
#else
static inline void
hdd_ndp_state_cleanup(struct wlan_objmgr_psoc *psoc, uint8_t ndi_vdev_id)
{
}

static inline void
hdd_ndp_peer_cleanup(struct hdd_context *hdd_ctx, struct hdd_adapter *adapter)
{
}
#endif /* WLAN_FEATURE_NAN */

#ifdef FUNC_CALL_MAP

/**
 * hdd_dump_func_call_map() - Dump the function call map
 *
 * Return: None
 */

static void hdd_dump_func_call_map(void)
{
	char *cc_buf;

	cc_buf = qdf_mem_malloc(QDF_FUNCTION_CALL_MAP_BUF_LEN);
	/*
	 * These logs are required as these indicates the start and end of the
	 * dump for the auto script to parse
	 */
	hdd_info("Function call map dump start");
	qdf_get_func_call_map(cc_buf);
	qdf_trace_hex_dump(QDF_MODULE_ID_HDD,
		QDF_TRACE_LEVEL_DEBUG, cc_buf, QDF_FUNCTION_CALL_MAP_BUF_LEN);
	hdd_info("Function call map dump end");
	qdf_mem_free(cc_buf);
}
#else
static inline void hdd_dump_func_call_map(void)
{
}
#endif

QDF_STATUS hdd_stop_adapter_ext(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hdd_station_ctx *sta_ctx;
	struct sap_context *sap_ctx;
	union iwreq_data wrqu;
	tSirUpdateIE update_ie;
	unsigned long rc;
	struct sap_config *sap_config;
	mac_handle_t mac_handle;
	struct wlan_objmgr_vdev *vdev;
	enum wlan_reason_code reason = REASON_IFACE_DOWN;

	hdd_enter();
	hdd_destroy_adapter_sysfs_files(adapter);

	if (adapter->device_mode == QDF_STA_MODE &&
	    hdd_is_pkt_capture_mon_enable(adapter) &&
	    ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
		hdd_unmap_monitor_interface_vdev(adapter);
	}

	if (adapter->vdev_id != WLAN_UMAC_VDEV_ID_MAX)
		wlan_hdd_cfg80211_deregister_frames(adapter);

	hdd_stop_tsf_sync(adapter);
	cds_flush_work(&adapter->scan_block_work);
	wlan_hdd_cfg80211_scan_block(adapter);
	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	mac_handle = hdd_ctx->mac_handle;

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_NDI_MODE:
	case QDF_NAN_DISC_MODE:
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

		if (adapter->device_mode == QDF_NDI_MODE ||
		    ((adapter->device_mode == QDF_STA_MODE ||
		      adapter->device_mode == QDF_P2P_CLIENT_MODE) &&
		      !hdd_cm_is_disconnected(adapter))
		    ) {
			INIT_COMPLETION(adapter->disconnect_comp_var);
			if (cds_is_driver_recovering())
				reason = REASON_DEVICE_RECOVERY;

			/* For NDI do not use roam_profile */
			if (adapter->device_mode == QDF_NDI_MODE) {
				hdd_ndp_peer_cleanup(hdd_ctx, adapter);
				status = sme_roam_ndi_stop(mac_handle,
							   adapter->vdev_id);
				if (QDF_IS_STATUS_SUCCESS(status)) {
					rc = wait_for_completion_timeout(
						&adapter->disconnect_comp_var,
						msecs_to_jiffies
						 (SME_CMD_STOP_BSS_TIMEOUT));
					if (!rc)
						hdd_warn("disconn_comp_var wait fail");
					hdd_cleanup_ndi(hdd_ctx, adapter);
				}
			} else if (adapter->device_mode == QDF_STA_MODE ||
				   adapter->device_mode == QDF_P2P_CLIENT_MODE) {
				/*
				 * On vdev delete wait for disconnect to
				 * complete. i.e use sync API, so that the
				 * vdev ref of MLME are cleaned and disconnect
				 * complete before vdev is moved to logically
				 * deleted.
				 */
				status = wlan_hdd_cm_issue_disconnect(adapter,
								      reason,
								      true);
				if (QDF_IS_STATUS_ERROR(status) &&
				    ucfg_ipa_is_enabled()) {
					hdd_err("STA disconnect failed");
					ucfg_ipa_uc_cleanup_sta(hdd_ctx->pdev,
								adapter->dev);
				}
			}

			memset(&wrqu, '\0', sizeof(wrqu));
			wrqu.ap_addr.sa_family = ARPHRD_ETHER;
			memset(wrqu.ap_addr.sa_data, '\0', ETH_ALEN);
			hdd_wext_send_event(adapter->dev, SIOCGIWAP, &wrqu,
					    NULL);
		}

		if ((adapter->device_mode == QDF_NAN_DISC_MODE ||
		     (adapter->device_mode == QDF_STA_MODE &&
		      !ucfg_nan_is_vdev_creation_allowed(hdd_ctx->psoc))) &&
		    ucfg_is_nan_conc_control_supported(hdd_ctx->psoc) &&
		    ucfg_is_nan_disc_active(hdd_ctx->psoc))
			ucfg_disable_nan_discovery(hdd_ctx->psoc, NULL, 0);

		wlan_hdd_scan_abort(adapter);
		wlan_hdd_cleanup_actionframe(adapter);
		wlan_hdd_cleanup_remain_on_channel_ctx(adapter);
		status = wlan_hdd_flush_pmksa_cache(adapter);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_debug("Cannot flush PMKIDCache");

		hdd_deregister_hl_netdev_fc_timer(adapter);

		hdd_deregister_tx_flow_control(adapter);

#ifdef WLAN_OPEN_SOURCE
		cancel_work_sync(&adapter->ipv4_notifier_work);
#ifdef WLAN_NS_OFFLOAD
		cancel_work_sync(&adapter->ipv6_notifier_work);
#endif
#endif

		if (adapter->device_mode == QDF_STA_MODE) {
			struct wlan_objmgr_vdev *vdev;

			vdev = hdd_objmgr_get_vdev_by_user(adapter,
							   WLAN_OSIF_SCAN_ID);
			if (vdev) {
				wlan_cfg80211_sched_scan_stop(vdev);
				hdd_objmgr_put_vdev_by_user(vdev,
							    WLAN_OSIF_SCAN_ID);
			}

			ucfg_ipa_flush_pending_vdev_events(hdd_ctx->pdev,
							   adapter->vdev_id);
		}
		hdd_vdev_destroy(adapter);
		break;

	case QDF_MONITOR_MODE:
		if (wlan_hdd_is_session_type_monitor(adapter->device_mode) &&
		    adapter->vdev &&
		    ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
			struct hdd_adapter *sta_adapter;

			ucfg_pkt_capture_deregister_callbacks(adapter->vdev);
			hdd_objmgr_put_vdev_by_user(adapter->vdev,
						    WLAN_OSIF_ID);
			adapter->vdev = NULL;

			sta_adapter = hdd_get_adapter(hdd_ctx, QDF_STA_MODE);
			if (!sta_adapter) {
				hdd_err("No station interface found");
				return -EINVAL;
			}
			hdd_reset_monitor_interface(sta_adapter);
		}

		if (wlan_hdd_is_session_type_monitor(QDF_MONITOR_MODE) &&
		    ucfg_mlme_is_sta_mon_conc_supported(hdd_ctx->psoc)) {
			hdd_info("Release wakelock for STA + monitor mode!");
			qdf_runtime_pm_allow_suspend(
				&hdd_ctx->runtime_context.monitor_mode);
			qdf_wake_lock_release(&hdd_ctx->monitor_mode_wakelock,
				WIFI_POWER_EVENT_WAKELOCK_MONITOR_MODE);
		}
		wlan_hdd_scan_abort(adapter);
		hdd_deregister_hl_netdev_fc_timer(adapter);
		hdd_deregister_tx_flow_control(adapter);
		status = hdd_disable_monitor_mode();
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err_rl("datapath reset failed for montior mode");
		hdd_set_idle_ps_config(hdd_ctx, true);
		status = hdd_monitor_mode_vdev_status(adapter);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err_rl("stop failed montior mode");
		sme_delete_mon_session(mac_handle, adapter->vdev_id);
		hdd_vdev_destroy(adapter);
		break;

	case QDF_SAP_MODE:
		wlan_hdd_scan_abort(adapter);
		hdd_abort_ongoing_sta_connection(hdd_ctx);
		/* Diassociate with all the peers before stop ap post */
		if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			if (wlan_hdd_del_station(adapter, NULL))
				hdd_sap_indicate_disconnect_for_sta(adapter);
		}
		status = wlan_hdd_flush_pmksa_cache(adapter);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_debug("Cannot flush PMKIDCache");

		sap_config = &adapter->session.ap.sap_config;
		wlansap_reset_sap_config_add_ie(sap_config, eUPDATE_IE_ALL);

		ucfg_ipa_flush(hdd_ctx->pdev);

		if (!ucfg_pre_cac_adapter_is_active(adapter->vdev)) {
			ucfg_pre_cac_stop(hdd_ctx->psoc);
			hdd_close_pre_cac_adapter(hdd_ctx);
		} else {
			if (ucfg_pre_cac_set_status(adapter->vdev, false))
				hdd_err("Failed to set is_pre_cac_on to false");
		}

		fallthrough;

	case QDF_P2P_GO_MODE:
		sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
		wlansap_cleanup_cac_timer(sap_ctx);

		cds_flush_work(&adapter->sap_stop_bss_work);
		if (qdf_atomic_read(&adapter->session.ap.acs_in_progress)) {
			hdd_info("ACS in progress, wait for complete");
			qdf_wait_for_event_completion(
				&adapter->acs_complete_event,
				ACS_COMPLETE_TIMEOUT);
		}

		if (adapter->device_mode == QDF_P2P_GO_MODE) {
			wlan_hdd_cleanup_remain_on_channel_ctx(adapter);
			hdd_abort_ongoing_sta_connection(hdd_ctx);
		}

		hdd_deregister_hl_netdev_fc_timer(adapter);
		hdd_deregister_tx_flow_control(adapter);
		hdd_destroy_acs_timer(adapter);

		mutex_lock(&hdd_ctx->sap_lock);
		if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			struct hdd_hostapd_state *hostapd_state =
				WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

			qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
			status = wlansap_stop_bss(
					WLAN_HDD_GET_SAP_CTX_PTR(adapter));

			if (QDF_IS_STATUS_SUCCESS(status)) {
				status = qdf_wait_single_event(
					&hostapd_state->qdf_stop_bss_event,
					SME_CMD_STOP_BSS_TIMEOUT);
				if (QDF_IS_STATUS_ERROR(status)) {
					hdd_err("failure waiting for wlansap_stop_bss %d",
						status);
					hdd_ipa_ap_disconnect_evt(hdd_ctx,
								  adapter);
				}
			} else {
				hdd_err("failure in wlansap_stop_bss");
			}

			clear_bit(SOFTAP_BSS_STARTED, &adapter->event_flags);
			policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
						adapter->device_mode,
						adapter->vdev_id);
			hdd_green_ap_start_state_mc(hdd_ctx,
						    adapter->device_mode,
						    false);

			qdf_copy_macaddr(&update_ie.bssid,
					 &adapter->mac_addr);
			update_ie.vdev_id = adapter->vdev_id;
			update_ie.ieBufferlength = 0;
			update_ie.pAdditionIEBuffer = NULL;
			update_ie.append = false;
			update_ie.notify = false;

			/* Probe bcn reset */
			status = sme_update_add_ie(mac_handle, &update_ie,
						   eUPDATE_IE_PROBE_BCN);
			if (status == QDF_STATUS_E_FAILURE)
				hdd_err("Could not pass PROBE_RSP_BCN to PE");

			/* Assoc resp reset */
			status = sme_update_add_ie(mac_handle, &update_ie,
						   eUPDATE_IE_ASSOC_RESP);
			if (status == QDF_STATUS_E_FAILURE)
				hdd_err("Could not pass ASSOC_RSP to PE");

			/* Reset WNI_CFG_PROBE_RSP Flags */
			wlan_hdd_reset_prob_rspies(adapter);
		}

		/*
		 * Note to restart sap after SSR driver needs below information
		 * and is not cleared/freed on purpose in case of SAP SSR
		 */
		if (!cds_is_driver_recovering()) {
			clear_bit(SOFTAP_INIT_DONE, &adapter->event_flags);
			qdf_mem_free(adapter->session.ap.beacon);
			adapter->session.ap.beacon = NULL;
		}

		/* Clear all the cached sta info */
		hdd_clear_cached_sta_info(adapter);

		/*
		 * If Do_Not_Break_Stream was enabled clear avoid channel list.
		 */
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
		if (vdev) {
			if (policy_mgr_is_dnsc_set(vdev))
				wlan_hdd_send_avoid_freq_for_dnbs(hdd_ctx, 0);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
		}

#ifdef WLAN_OPEN_SOURCE
		cancel_work_sync(&adapter->ipv4_notifier_work);
#ifdef WLAN_NS_OFFLOAD
		cancel_work_sync(&adapter->ipv6_notifier_work);
#endif
#endif
		sap_release_vdev_ref(WLAN_HDD_GET_SAP_CTX_PTR(adapter));

		if (adapter->device_mode == QDF_SAP_MODE) {
			ucfg_ipa_flush_pending_vdev_events(hdd_ctx->pdev,
							   adapter->vdev_id);
		}

		hdd_vdev_destroy(adapter);

		mutex_unlock(&hdd_ctx->sap_lock);
		break;
	case QDF_OCB_MODE:
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		cdp_clear_peer(cds_get_context(QDF_MODULE_ID_SOC),
			       OL_TXRX_PDEV_ID,
			       sta_ctx->conn_info.peer_macaddr[0]);
		hdd_deregister_hl_netdev_fc_timer(adapter);
		hdd_deregister_tx_flow_control(adapter);
		hdd_vdev_destroy(adapter);
		break;
	default:
		break;
	}

	/* This function should be invoked at the end of this api*/
	hdd_dump_func_call_map();
	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_stop_adapter(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter)
{
	QDF_STATUS status;

	if (adapter->device_mode == QDF_STA_MODE)
		status = hdd_stop_link_adapter(hdd_ctx, adapter);

	status = hdd_stop_adapter_ext(hdd_ctx, adapter);

	return status;
}

/**
 * hdd_deinit_all_adapters - deinit all adapters
 * @hdd_ctx:   HDD context
 * @rtnl_held: True if RTNL lock held
 *
 */
void  hdd_deinit_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;

	hdd_enter();

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_DEINIT_ALL_ADAPTERS) {
		hdd_deinit_adapter(hdd_ctx, adapter, rtnl_held);
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_DEINIT_ALL_ADAPTERS);
	}

	hdd_exit();
}

QDF_STATUS hdd_stop_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;

	hdd_enter();

	ucfg_pre_cac_stop(hdd_ctx->psoc);
	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_STOP_ALL_ADAPTERS) {
		hdd_stop_adapter(hdd_ctx, adapter);
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_STOP_ALL_ADAPTERS);
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

void hdd_set_netdev_flags(struct hdd_adapter *adapter)
{
	bool enable_csum = false;
	bool enable_lro;
	enum QDF_OPMODE device_mode;
	struct hdd_context *hdd_ctx;
	ol_txrx_soc_handle soc;
	uint64_t temp;

	if (!adapter || !adapter->dev) {
		hdd_err("invalid input!");
		return;
	}
	device_mode = adapter->device_mode;

	hdd_ctx = adapter->hdd_ctx;
	soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!soc || !hdd_ctx) {
		hdd_err("invalid SOC or HDD context!");
		return;
	}

	/* Determine device_mode specific configuration */

	enable_lro = !!cdp_cfg_get(soc, cfg_dp_lro_enable);
	enable_csum = !!cdp_cfg_get(soc,
				     cfg_dp_enable_ip_tcp_udp_checksum_offload);
	switch (device_mode) {
	case QDF_P2P_DEVICE_MODE:
	case QDF_P2P_CLIENT_MODE:
		enable_csum = !!cdp_cfg_get(soc,
					    cfg_dp_enable_p2p_ip_tcp_udp_checksum_offload);
		break;
	case QDF_P2P_GO_MODE:
		enable_csum = !!cdp_cfg_get(soc,
					    cfg_dp_enable_p2p_ip_tcp_udp_checksum_offload);
		enable_lro = false;
		break;
	case QDF_SAP_MODE:
		enable_lro = false;
		break;
	case QDF_NDI_MODE:
	case QDF_NAN_DISC_MODE:
		enable_csum = !!cdp_cfg_get(soc,
					    cfg_dp_enable_nan_ip_tcp_udp_checksum_offload);
		break;
	default:
		break;
	}

	/* Set netdev flags */

	/*
	 * In case of USB tethering, LRO is disabled. If SSR happened
	 * during that time, then as part of SSR init, do not enable
	 * the LRO again. Keep the LRO state same as before SSR.
	 */
	if (enable_lro && !(qdf_atomic_read(&hdd_ctx->vendor_disable_lro_flag)))
		adapter->dev->features |= NETIF_F_LRO;

	if (enable_csum)
		adapter->dev->features |=
			(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);

	if (cdp_cfg_get(soc, cfg_dp_tso_enable) && enable_csum) {
		adapter->dev->features |= TSO_FEATURE_FLAGS;
		adapter->tso_csum_feature_enabled = 1;
	}

	if (cdp_cfg_get(soc, cfg_dp_sg_enable))
		adapter->dev->features |= NETIF_F_SG;

	adapter->dev->features |= NETIF_F_RXCSUM;
	temp = (uint64_t)adapter->dev->features;

	hdd_debug("adapter mode %u dev feature 0x%llx", device_mode, temp);
}

static void hdd_reset_scan_operation(struct hdd_context *hdd_ctx,
				     struct hdd_adapter *adapter)
{
	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_NDI_MODE:
		wlan_hdd_scan_abort(adapter);
		wlan_hdd_cleanup_remain_on_channel_ctx(adapter);
		if (adapter->device_mode == QDF_STA_MODE) {
			struct wlan_objmgr_vdev *vdev;

			vdev = hdd_objmgr_get_vdev_by_user(adapter,
							   WLAN_OSIF_SCAN_ID);
			if (vdev) {
				wlan_cfg80211_sched_scan_stop(vdev);
				hdd_objmgr_put_vdev_by_user(vdev,
							    WLAN_OSIF_SCAN_ID);
			}
		}
		break;
	case QDF_P2P_GO_MODE:
		wlan_hdd_cleanup_remain_on_channel_ctx(adapter);
		break;
	case QDF_SAP_MODE:
		qdf_atomic_set(&adapter->session.ap.acs_in_progress, 0);
		break;
	default:
		break;
	}
}

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_adapter_abort_tx_flow() - Abort the tx flow control
 * @adapter: pointer to hdd adapter
 *
 * Resume tx and stop the tx flow control timer if the tx is paused
 * and the flow control timer is running. This function is called by
 * SSR to avoid the inconsistency of tx status before and after SSR.
 *
 * Return: void
 */
static void hdd_adapter_abort_tx_flow(struct hdd_adapter *adapter)
{
	if (adapter->hdd_stats.tx_rx_stats.is_txflow_paused &&
		QDF_TIMER_STATE_RUNNING ==
		qdf_mc_timer_get_current_state(
			&adapter->tx_flow_control_timer)) {
		hdd_tx_resume_timer_expired_handler(adapter);
		qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
	}
}
#else
static void hdd_adapter_abort_tx_flow(struct hdd_adapter *adapter)
{
}
#endif

QDF_STATUS hdd_reset_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	bool value;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	ucfg_mlme_get_sap_internal_restart(hdd_ctx->psoc, &value);


	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_RESET_ALL_ADAPTERS) {
		hdd_info("[SSR] reset adapter with device mode %s(%d)",
			 qdf_opmode_str(adapter->device_mode),
			 adapter->device_mode);


		hdd_adapter_abort_tx_flow(adapter);

		if ((adapter->device_mode == QDF_STA_MODE) ||
		    (adapter->device_mode == QDF_P2P_CLIENT_MODE)) {
			hdd_send_twt_del_all_sessions_to_userspace(adapter);

			/* Stop tdls timers */
			vdev = hdd_objmgr_get_vdev_by_user(adapter,
							   WLAN_OSIF_TDLS_ID);
			if (vdev) {
				hdd_notify_tdls_reset_adapter(vdev);
				hdd_objmgr_put_vdev_by_user(vdev,
							    WLAN_OSIF_TDLS_ID);
			}
		}

		if (value &&
		    adapter->device_mode == QDF_SAP_MODE) {
			hdd_medium_assess_ssr_enable_flag();
			wlan_hdd_netif_queue_control(adapter,
						     WLAN_STOP_ALL_NETIF_QUEUE,
						     WLAN_CONTROL_PATH);
		} else {
			wlan_hdd_netif_queue_control(adapter,
					   WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					   WLAN_CONTROL_PATH);
		}

		/*
		 * Clear fc flag if it was set before SSR to avoid TX queues
		 * permanently stopped after SSR.
		 * Here WLAN_START_ALL_NETIF_QUEUE will actually not start any
		 * queue since it's blocked by reason WLAN_CONTROL_PATH.
		 */
		if (adapter->pause_map & (1 << WLAN_DATA_FLOW_CONTROL))
			wlan_hdd_netif_queue_control(adapter,
						     WLAN_START_ALL_NETIF_QUEUE,
						     WLAN_DATA_FLOW_CONTROL);

		hdd_reset_scan_operation(hdd_ctx, adapter);

		if (test_bit(WMM_INIT_DONE, &adapter->event_flags)) {
			hdd_wmm_adapter_close(adapter);
			clear_bit(WMM_INIT_DONE, &adapter->event_flags);
		}

		hdd_debug("Flush any mgmt references held by peer");
		hdd_stop_adapter(hdd_ctx, adapter);
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_RESET_ALL_ADAPTERS);
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

bool hdd_is_any_interface_open(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IS_ANY_INTERFACE_OPEN;

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_info("FTM mode, don't close the module");
		return true;
	}

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags) ||
		    test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return true;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return false;
}

bool hdd_is_interface_up(struct hdd_adapter *adapter)
{
	if (test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags))
		return true;
	else
		return false;
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
#ifdef WLAN_FEATURE_11BE
static inline bool wlan_hdd_is_mon_channel_bw_valid(enum phy_ch_width ch_width)
{
	if (ch_width > CH_WIDTH_320MHZ ||
	    (!cds_is_sub_20_mhz_enabled() && (ch_width == CH_WIDTH_5MHZ ||
					      ch_width == CH_WIDTH_10MHZ)))
		return false;

	return true;
}
#else
static inline bool wlan_hdd_is_mon_channel_bw_valid(enum phy_ch_width ch_width)
{
	if (ch_width > CH_WIDTH_10MHZ ||
	    (!cds_is_sub_20_mhz_enabled() && (ch_width == CH_WIDTH_5MHZ ||
					      ch_width == CH_WIDTH_10MHZ)))
		return false;

	return true;
}
#endif

int wlan_hdd_set_mon_chan(struct hdd_adapter *adapter, qdf_freq_t freq,
			  uint32_t bandwidth)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct hdd_mon_set_ch_info *ch_info = &sta_ctx->ch_info;
	QDF_STATUS status;
	struct qdf_mac_addr bssid;
	struct channel_change_req *req;
	struct ch_params ch_params;
	enum phy_ch_width max_fw_bw;
	enum phy_ch_width ch_width;
	int ret;

	if ((hdd_get_conparam() != QDF_GLOBAL_MONITOR_MODE) &&
	    (!policy_mgr_is_sta_mon_concurrency(hdd_ctx->psoc))) {
		hdd_err("Not supported, device is not in monitor mode");
		return -EINVAL;
	}

	if (adapter->device_mode != QDF_MONITOR_MODE) {
		hdd_err_rl("Not supported, adapter is not in monitor mode");
		return -EINVAL;
	}

	/* Verify the BW before accepting this request */
	ch_width = bandwidth;

	if (!wlan_hdd_is_mon_channel_bw_valid(ch_width)) {
		hdd_err("invalid BW received %d", ch_width);
		return -EINVAL;
	}

	max_fw_bw = sme_get_vht_ch_width();

	hdd_debug("max fw BW %d ch width %d", max_fw_bw, ch_width);
	if ((ch_width == CH_WIDTH_160MHZ &&
	    max_fw_bw <= WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ) ||
	    (ch_width == CH_WIDTH_80P80MHZ &&
	    max_fw_bw <= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)) {
		hdd_err("FW does not support this BW %d max BW supported %d",
			ch_width, max_fw_bw);
		return -EINVAL;
	}

	if (!hdd_is_target_eht_phy_ch_width_supported(ch_width))
		return -EINVAL;

	ret = hdd_validate_channel_and_bandwidth(adapter, freq, bandwidth);
	if (ret) {
		hdd_err("Invalid CH and BW combo");
		return ret;
	}

	hdd_debug("Set monitor mode frequency %d", freq);
	qdf_mem_copy(bssid.bytes, adapter->mac_addr.bytes,
		     QDF_MAC_ADDR_SIZE);

	ch_params.ch_width = bandwidth;
	wlan_reg_set_channel_params_for_pwrmode(hdd_ctx->pdev, freq, 0,
						&ch_params,
						REG_CURRENT_PWR_MODE);

	if (ch_params.ch_width == CH_WIDTH_INVALID) {
		hdd_err("Invalid capture channel or bandwidth for a country");
		return -EINVAL;
	}
	if (wlan_hdd_change_hw_mode_for_given_chnl(adapter, freq,
						   POLICY_MGR_UPDATE_REASON_SET_OPER_CHAN)) {
		hdd_err("Failed to change hw mode");
		return -EINVAL;
	}

	if (adapter->monitor_mode_vdev_up_in_progress) {
		hdd_err_rl("monitor mode vdev up in progress");
		return -EBUSY;
	}

	status = qdf_event_reset(&adapter->qdf_monitor_mode_vdev_up_event);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("failed to reinit monitor mode vdev up event");
		return qdf_status_to_os_return(status);
	}
	adapter->monitor_mode_vdev_up_in_progress = true;

	qdf_mem_zero(&ch_params, sizeof(struct ch_params));

	req = qdf_mem_malloc(sizeof(struct channel_change_req));
	if (!req)
		return -ENOMEM;
	req->vdev_id = adapter->vdev_id;
	req->target_chan_freq = freq;
	req->ch_width = ch_width;

	ch_params.ch_width = ch_width;
	hdd_select_cbmode(adapter, freq, 0, &ch_params);

	req->sec_ch_offset = ch_params.sec_ch_offset;
	req->center_freq_seg0 = ch_params.center_freq_seg0;
	req->center_freq_seg1 = ch_params.center_freq_seg1;

	sme_fill_channel_change_request(hdd_ctx->mac_handle, req,
					ch_info->phy_mode);
	status = sme_send_channel_change_req(hdd_ctx->mac_handle, req);
	qdf_mem_free(req);
	if (status) {
		hdd_err("Status: %d Failed to set sme_roam Channel for monitor mode",
			status);
		adapter->monitor_mode_vdev_up_in_progress = false;
		return qdf_status_to_os_return(status);
	}

	adapter->mon_chan_freq = freq;
	adapter->mon_bandwidth = bandwidth;

	/* block on a completion variable until vdev up success*/
	status = qdf_wait_for_event_completion(
				       &adapter->qdf_monitor_mode_vdev_up_event,
					WLAN_MONITOR_MODE_VDEV_UP_EVT);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("monitor vdev up event time out vdev id: %d",
			    adapter->vdev_id);
		if (adapter->qdf_monitor_mode_vdev_up_event.force_set)
			/*
			 * SSR/PDR has caused shutdown, which has
			 * forcefully set the event.
			 */
			hdd_err_rl("monitor mode vdev up event forcefully set");
		else if (status == QDF_STATUS_E_TIMEOUT)
			hdd_err("monitor mode vdev up timed out");
		else
			hdd_err_rl("Failed monitor mode vdev up(status-%d)",
				  status);

		adapter->monitor_mode_vdev_up_in_progress = false;
	}

	return qdf_status_to_os_return(status);
}
#endif

#if defined MSM_PLATFORM && (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 19, 0))
/**
 * hdd_stop_p2p_go() - call cfg80211 API to stop P2P GO
 * @adapter: pointer to adapter
 *
 * This function calls cfg80211 API to stop P2P GO
 *
 * Return: None
 */
static void hdd_stop_p2p_go(struct hdd_adapter *adapter)
{
	hdd_debug("[SSR] send stop ap to supplicant");
	cfg80211_ap_stopped(adapter->dev, GFP_KERNEL);
}

static inline void hdd_delete_sta(struct hdd_adapter *adapter)
{
}

#else
static void hdd_stop_p2p_go(struct hdd_adapter *adapter)
{
	hdd_debug("[SSR] send stop iface ap to supplicant");
	cfg80211_stop_iface(adapter->hdd_ctx->wiphy, &adapter->wdev,
			    GFP_KERNEL);
}

/**
 * hdd_delete_sta() - call cfg80211 API to delete STA
 * @adapter: pointer to adapter
 *
 * This function calls cfg80211 API to delete STA
 *
 * Return: None
 */
static void hdd_delete_sta(struct hdd_adapter *adapter)
{
	struct qdf_mac_addr bcast_mac = QDF_MAC_ADDR_BCAST_INIT;

	hdd_debug("[SSR] send restart supplicant");
	/* event supplicant to restart */
	cfg80211_del_sta(adapter->dev,
			 (const u8 *)&bcast_mac.bytes[0],
			 GFP_KERNEL);
}
#endif

QDF_STATUS hdd_start_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	bool value;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_START_ALL_ADAPTERS;
	int ret;

	hdd_enter();

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (!hdd_is_interface_up(adapter) &&
		    adapter->device_mode != QDF_NDI_MODE) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		hdd_debug("[SSR] start adapter with device mode %s(%d)",
			  qdf_opmode_str(adapter->device_mode),
			  adapter->device_mode);

		hdd_wmm_dscp_initial_state(adapter);

		switch (adapter->device_mode) {
		case QDF_STA_MODE:
		case QDF_P2P_CLIENT_MODE:
		case QDF_P2P_DEVICE_MODE:
		case QDF_NAN_DISC_MODE:

			ret = hdd_start_station_adapter(adapter);
			if (ret) {
				hdd_err("[SSR] Failed to start station adapter: %d",
					ret);
				hdd_adapter_dev_put_debug(adapter, dbgid);
				continue;
			}
			if (adapter->device_mode == QDF_STA_MODE) {
				ret = hdd_start_link_adapter(adapter);
				if (ret) {
					hdd_err("[SSR] Failed to start link adapter: %d",
						ret);
					hdd_stop_adapter(hdd_ctx, adapter);
					hdd_adapter_dev_put_debug(adapter,
								  dbgid);
					continue;
				}
			}

			/* Open the gates for HDD to receive Wext commands */
			adapter->is_link_up_service_needed = false;

			if ((adapter->device_mode == QDF_NAN_DISC_MODE ||
			     (adapter->device_mode == QDF_STA_MODE &&
			      !ucfg_nan_is_vdev_creation_allowed(
							hdd_ctx->psoc))) &&
			    cds_is_driver_recovering())
				ucfg_nan_disable_ind_to_userspace(
							hdd_ctx->psoc);

			hdd_register_tx_flow_control(adapter,
					hdd_tx_resume_timer_expired_handler,
					hdd_tx_resume_cb,
					hdd_tx_flow_control_is_pause);

			hdd_register_hl_netdev_fc_timer(
					adapter,
					hdd_tx_resume_timer_expired_handler);

			hdd_lpass_notify_start(hdd_ctx, adapter);
			break;

		case QDF_SAP_MODE:
			ucfg_mlme_get_sap_internal_restart(hdd_ctx->psoc,
							   &value);
			if (value)
				hdd_start_ap_adapter(adapter);

			break;

		case QDF_P2P_GO_MODE:
			hdd_delete_sta(adapter);
			break;
		case QDF_MONITOR_MODE:
			if (wlan_hdd_is_session_type_monitor(
			    adapter->device_mode) &&
			    ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
				struct hdd_adapter *sta_adapter;

				sta_adapter = hdd_get_adapter(hdd_ctx,
							      QDF_STA_MODE);
				if (!sta_adapter) {
					hdd_err("No station interface found");
					return -EINVAL;
				}

				hdd_map_monitor_interface_vdev(sta_adapter);
				break;
			}
			hdd_start_station_adapter(adapter);
			hdd_set_mon_rx_cb(adapter->dev);

			wlan_hdd_set_mon_chan(
					adapter, adapter->mon_chan_freq,
					adapter->mon_bandwidth);
			break;
		case QDF_NDI_MODE:
			hdd_ndi_start(adapter->dev->name, 0);
			break;
		default:
			break;
		}
		/*
		 * Action frame registered in one adapter which will
		 * applicable to all interfaces
		 */
		if (hdd_set_fw_params(adapter))
			hdd_err("Failed to set adapter FW params after SSR!");

		wlan_hdd_cfg80211_register_frames(adapter);
		hdd_create_adapter_sysfs_files(adapter);
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (!hdd_is_interface_up(adapter)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		if (adapter->device_mode == QDF_P2P_GO_MODE)
			hdd_stop_p2p_go(adapter);

		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

void hdd_adapter_dev_hold_debug(struct hdd_adapter *adapter,
				wlan_net_dev_ref_dbgid dbgid)
{
	if (dbgid >= NET_DEV_HOLD_ID_MAX) {
		hdd_err("Invalid debug id: %d", dbgid);
		QDF_BUG(0);
	}
	dev_hold(adapter->dev);
	qdf_atomic_inc(&adapter->net_dev_hold_ref_count[dbgid]);
}

void hdd_adapter_dev_put_debug(struct hdd_adapter *adapter,
			       wlan_net_dev_ref_dbgid dbgid)
{
	if (dbgid >= NET_DEV_HOLD_ID_MAX) {
		hdd_err("Invalid debug id: %d", dbgid);
		QDF_BUG(0);
	}

	if (qdf_atomic_dec_return(
			&adapter->net_dev_hold_ref_count[dbgid]) < 0) {
		hdd_err("dev_put detected without dev_hold for debug id: %s",
			net_dev_ref_debug_string_from_id(dbgid));
		QDF_BUG(0);
	}

	if (adapter->dev) {
		dev_put(adapter->dev);
	} else {
		hdd_err("adapter->dev is NULL");
		QDF_BUG(0);
	}
}

QDF_STATUS hdd_get_front_adapter(struct hdd_context *hdd_ctx,
				 struct hdd_adapter **out_adapter)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	*out_adapter = NULL;

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	status = qdf_list_peek_front(&hdd_ctx->hdd_adapters, &node);
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_adapter = qdf_container_of(node, struct hdd_adapter, node);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_get_next_adapter(struct hdd_context *hdd_ctx,
				struct hdd_adapter *current_adapter,
				struct hdd_adapter **out_adapter)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	*out_adapter = NULL;

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	status = qdf_list_peek_next(&hdd_ctx->hdd_adapters,
				    &current_adapter->node,
				    &node);
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_adapter = qdf_container_of(node, struct hdd_adapter, node);

	return status;
}

QDF_STATUS hdd_get_front_adapter_no_lock(struct hdd_context *hdd_ctx,
					 struct hdd_adapter **out_adapter)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	*out_adapter = NULL;

	status = qdf_list_peek_front(&hdd_ctx->hdd_adapters, &node);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_adapter = qdf_container_of(node, struct hdd_adapter, node);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_get_next_adapter_no_lock(struct hdd_context *hdd_ctx,
					struct hdd_adapter *current_adapter,
					struct hdd_adapter **out_adapter)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	if (!current_adapter)
		return QDF_STATUS_E_INVAL;

	*out_adapter = NULL;

	status = qdf_list_peek_next(&hdd_ctx->hdd_adapters,
				    &current_adapter->node,
				    &node);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_adapter = qdf_container_of(node, struct hdd_adapter, node);

	return status;
}

QDF_STATUS hdd_remove_adapter(struct hdd_context *hdd_ctx,
			      struct hdd_adapter *adapter)
{
	QDF_STATUS status;

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	status = qdf_list_remove_node(&hdd_ctx->hdd_adapters, &adapter->node);
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	return status;
}

QDF_STATUS hdd_remove_front_adapter(struct hdd_context *hdd_ctx,
				    struct hdd_adapter **out_adapter)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	*out_adapter = NULL;

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	status = qdf_list_remove_front(&hdd_ctx->hdd_adapters, &node);
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_adapter = qdf_container_of(node, struct hdd_adapter, node);

	return status;
}

QDF_STATUS hdd_add_adapter_back(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter)
{
	QDF_STATUS status;

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	status = qdf_list_insert_back(&hdd_ctx->hdd_adapters, &adapter->node);
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	return status;
}

QDF_STATUS hdd_add_adapter_front(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *adapter)
{
	QDF_STATUS status;

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	status = qdf_list_insert_front(&hdd_ctx->hdd_adapters, &adapter->node);
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	return status;
}

void hdd_validate_next_adapter(struct hdd_adapter **curr,
			       struct hdd_adapter **next,
			       wlan_net_dev_ref_dbgid dbg_id)
{
	if (!*curr || !*next || *curr != *next)
		return;

	hdd_err("Validation failed");
	hdd_adapter_dev_put_debug(*curr, dbg_id);
	*curr = NULL;
	*next = NULL;
}

QDF_STATUS hdd_adapter_iterate(hdd_adapter_iterate_cb cb, void *context)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *cache[HDD_MAX_ADAPTERS];
	struct hdd_adapter *adapter;
	uint32_t n_cache = 0;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	QDF_STATUS status;
	int i;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (unlikely(!hdd_ctx))
		return QDF_STATUS_E_FAILURE;

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	for (hdd_get_front_adapter_no_lock(hdd_ctx, &adapter); adapter;
	     hdd_get_next_adapter_no_lock(hdd_ctx, adapter, &adapter)) {
		cache[n_cache++] = adapter;
	}
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	for (i = 0; i < n_cache; i++) {
		adapter = hdd_adapter_get_by_reference(hdd_ctx, cache[i]);
		if (!adapter) {
			/*
			 * detected remove while iterating
			 * concurrency failure
			 */
			ret = QDF_STATUS_E_FAILURE;
			continue;
		}

		status = cb(adapter, context);
		hdd_adapter_put(adapter);
		if (status != QDF_STATUS_SUCCESS)
			return status;
	}

	return ret;
}

struct hdd_adapter *hdd_get_adapter_by_rand_macaddr(
	struct hdd_context *hdd_ctx, tSirMacAddr mac_addr)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid =
				NET_DEV_HOLD_GET_ADAPTER_BY_RAND_MACADDR;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if ((adapter->device_mode == QDF_STA_MODE ||
		     adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		     adapter->device_mode == QDF_P2P_DEVICE_MODE) &&
		    ucfg_p2p_check_random_mac(hdd_ctx->psoc,
					      adapter->vdev_id, mac_addr)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

struct hdd_adapter *hdd_get_adapter_by_macaddr(struct hdd_context *hdd_ctx,
					       tSirMacAddr mac_addr)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER_BY_MACADDR;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (!qdf_mem_cmp(adapter->mac_addr.bytes,
				 mac_addr, sizeof(tSirMacAddr))) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}

		if (hdd_adapter_is_sl_ml_adapter(adapter) &&
		    !qdf_mem_cmp(adapter->mld_addr.bytes,
				 mac_addr, sizeof(tSirMacAddr))) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter, dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

struct hdd_adapter *hdd_get_adapter_by_vdev(struct hdd_context *hdd_ctx,
					    uint32_t vdev_id)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER_BY_VDEV;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->vdev_id == vdev_id) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

struct hdd_adapter *hdd_adapter_get_by_reference(struct hdd_context *hdd_ctx,
						 struct hdd_adapter *reference)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_ADAPTER_GET_BY_REFERENCE;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter == reference) {
			dev_hold(adapter->dev);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			break;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return adapter;
}

void hdd_adapter_put(struct hdd_adapter *adapter)
{
	dev_put(adapter->dev);
}

struct hdd_adapter *hdd_get_adapter_by_iface_name(struct hdd_context *hdd_ctx,
					     const char *iface_name)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER_BY_IFACE_NAME;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (!qdf_str_cmp(adapter->dev->name, iface_name)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

struct hdd_adapter *hdd_get_adapter_by_ifindex(struct hdd_context *hdd_ctx,
					       uint32_t if_index)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->dev->ifindex == if_index) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

/**
 * hdd_get_adapter() - to get adapter matching the mode
 * @hdd_ctx: hdd context
 * @mode: adapter mode
 *
 * This routine will return the pointer to adapter matching
 * with the passed mode.
 *
 * Return: pointer to adapter or null
 */
struct hdd_adapter *hdd_get_adapter(struct hdd_context *hdd_ctx,
			enum QDF_OPMODE mode)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->device_mode == mode) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

enum QDF_OPMODE hdd_get_device_mode(uint32_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return QDF_MAX_NO_OF_MODE;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("Invalid HDD adapter");
		return QDF_MAX_NO_OF_MODE;
	}

	return adapter->device_mode;
}

uint32_t hdd_get_operating_chan_freq(struct hdd_context *hdd_ctx,
				     enum QDF_OPMODE mode)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	uint32_t oper_chan_freq = 0;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_OPERATING_CHAN_FREQ;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (mode == adapter->device_mode) {
			oper_chan_freq =
				hdd_get_adapter_home_channel(adapter);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			break;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return oper_chan_freq;
}

static inline QDF_STATUS hdd_unregister_wext_all_adapters(struct hdd_context *
							  hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid =
				NET_DEV_HOLD_UNREGISTER_WEXT_ALL_ADAPTERS;

	hdd_enter();

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE ||
		    adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			hdd_unregister_wext(adapter->dev);
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_abort_mac_scan_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid =
				NET_DEV_HOLD_ABORT_MAC_SCAN_ALL_ADAPTERS;

	hdd_enter();

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE ||
		    adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			wlan_abort_scan(hdd_ctx->pdev, INVAL_PDEV_ID,
					adapter->vdev_id, INVALID_SCAN_ID,
					true);
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_abort_sched_scan_all_adapters() - stops scheduled (PNO) scans for all
 * adapters
 * @hdd_ctx: The HDD context containing the adapters to operate on
 *
 * return: QDF_STATUS_SUCCESS
 */
static QDF_STATUS hdd_abort_sched_scan_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	int err;
	wlan_net_dev_ref_dbgid dbgid =
				NET_DEV_HOLD_ABORT_SCHED_SCAN_ALL_ADAPTERS;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE ||
		    adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			err = wlan_hdd_sched_scan_stop(adapter->dev);
			if (err)
				hdd_err("Unable to stop scheduled scan");
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_unregister_notifiers - Unregister netdev notifiers.
 * @hdd_ctx: HDD context
 *
 * Unregister netdev notifiers like IPv4 and IPv6.
 *
 * Return: None.
 */
void hdd_unregister_notifiers(struct hdd_context *hdd_ctx)
{
	osif_dp_nud_unregister_netevent_notifier(hdd_ctx->psoc);
	hdd_wlan_unregister_ip6_notifier(hdd_ctx);

	unregister_inetaddr_notifier(&hdd_ctx->ipv4_notifier);
}

/**
 * hdd_exit_netlink_services - Exit netlink services
 * @hdd_ctx: HDD context
 *
 * Exit netlink services like cnss_diag, cesium netlink socket, ptt socket and
 * nl service.
 *
 * Return: None.
 */
static void hdd_exit_netlink_services(struct hdd_context *hdd_ctx)
{
	spectral_scan_deactivate_service();
	cnss_diag_deactivate_service();
	hdd_close_cesium_nl_sock();
	ptt_sock_deactivate_svc();
	hdd_deactivate_wifi_pos();

	nl_srv_exit();
}

/**
 * hdd_init_netlink_services- Init netlink services
 * @hdd_ctx: HDD context
 *
 * Init netlink services like cnss_diag, cesium netlink socket, ptt socket and
 * nl service.
 *
 * Return: 0 on success and errno on failure.
 */
static int hdd_init_netlink_services(struct hdd_context *hdd_ctx)
{
	int ret;

	ret = wlan_hdd_nl_init(hdd_ctx);
	if (ret) {
		hdd_err("nl_srv_init failed: %d", ret);
		goto out;
	}
	cds_set_radio_index(hdd_ctx->radio_index);

	ret = hdd_activate_wifi_pos(hdd_ctx);
	if (ret) {
		hdd_err("hdd_activate_wifi_pos failed: %d", ret);
		goto err_nl_srv;
	}

	ptt_sock_activate_svc();

	ret = hdd_open_cesium_nl_sock();
	if (ret)
		hdd_err("hdd_open_cesium_nl_sock failed ret: %d", ret);

	ret = cnss_diag_activate_service();
	if (ret) {
		hdd_err("cnss_diag_activate_service failed: %d", ret);
		goto err_close_cesium;
	}

	spectral_scan_activate_service(hdd_ctx);

	return 0;

err_close_cesium:
	hdd_close_cesium_nl_sock();
	ptt_sock_deactivate_svc();
	hdd_deactivate_wifi_pos();
err_nl_srv:
	nl_srv_exit();
out:
	return ret;
}

#ifdef SHUTDOWN_WLAN_IN_SYSTEM_SUSPEND
static QDF_STATUS
hdd_shutdown_wlan_in_suspend_prepare(struct hdd_context *hdd_ctx)
{
#define SHUTDOWN_IN_SUSPEND_RETRY 10

	int count = 0;

	if (ucfg_pmo_get_suspend_mode(hdd_ctx->psoc) != PMO_SUSPEND_SHUTDOWN) {
		hdd_debug("shutdown in suspend not supported");
		return 0;
	}

	while (hdd_is_any_interface_open(hdd_ctx) &&
	       count < SHUTDOWN_IN_SUSPEND_RETRY) {
		count++;
		hdd_debug_rl("sleep 50ms to wait adapters stopped, #%d", count);
		msleep(50);
	}
	if (count >= SHUTDOWN_IN_SUSPEND_RETRY) {
		hdd_err("some adapters not stopped");
		return -EBUSY;
	}

	hdd_debug("call pld idle shutdown directly");
	return pld_idle_shutdown(hdd_ctx->parent_dev, hdd_psoc_idle_shutdown);
}

static int hdd_pm_notify(struct notifier_block *b,
			 unsigned long event, void *p)
{
	struct hdd_context *hdd_ctx = container_of(b, struct hdd_context,
						   pm_notifier);

	if (wlan_hdd_validate_context(hdd_ctx) != 0)
		return NOTIFY_STOP;

	hdd_debug("got PM event: %lu", event);

	switch (event) {
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
		if (0 != hdd_shutdown_wlan_in_suspend_prepare(hdd_ctx))
			return NOTIFY_STOP;
		break;
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		break;
	}

	return NOTIFY_DONE;
}

static void hdd_pm_notifier_init(struct hdd_context *hdd_ctx)
{
	hdd_ctx->pm_notifier.notifier_call = hdd_pm_notify;
	register_pm_notifier(&hdd_ctx->pm_notifier);
}

static void hdd_pm_notifier_deinit(struct hdd_context *hdd_ctx)
{
	unregister_pm_notifier(&hdd_ctx->pm_notifier);
}
#else
static inline void hdd_pm_notifier_init(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_pm_notifier_deinit(struct hdd_context *hdd_ctx)
{
}
#endif

/**
 * hdd_context_deinit() - Deinitialize HDD context
 * @hdd_ctx:    HDD context.
 *
 * Deinitialize HDD context along with all the feature specific contexts but
 * do not free hdd context itself. Caller of this API is supposed to free
 * HDD context.
 *
 * return: 0 on success and errno on failure.
 */
static int hdd_context_deinit(struct hdd_context *hdd_ctx)
{
	qdf_wake_lock_destroy(&hdd_ctx->monitor_mode_wakelock);

	wlan_hdd_cfg80211_deinit(hdd_ctx->wiphy);

	ucfg_dp_bbm_context_deinit(hdd_ctx->psoc);

	hdd_sap_context_destroy(hdd_ctx);

	hdd_scan_context_destroy(hdd_ctx);

	qdf_list_destroy(&hdd_ctx->hdd_adapters);

	return 0;
}

void hdd_context_destroy(struct hdd_context *hdd_ctx)
{
	wlan_hdd_sar_timers_deinit(hdd_ctx);

	cds_set_context(QDF_MODULE_ID_HDD, NULL);

	hdd_exit_netlink_services(hdd_ctx);

	hdd_context_deinit(hdd_ctx);

	hdd_objmgr_release_and_destroy_psoc(hdd_ctx);

	qdf_mem_free(hdd_ctx->config);
	hdd_ctx->config = NULL;
	cfg_release();

	hdd_pm_notifier_deinit(hdd_ctx);
	qdf_delayed_work_destroy(&hdd_ctx->psoc_idle_timeout_work);
	wiphy_free(hdd_ctx->wiphy);
}

/**
 * wlan_destroy_bug_report_lock() - Destroy bug report lock
 *
 * This function is used to destroy bug report lock
 *
 * Return: None
 */
static void wlan_destroy_bug_report_lock(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		hdd_err("cds context is NULL");
		return;
	}

	qdf_spinlock_destroy(&p_cds_context->bug_report_lock);
}

#ifdef DISABLE_CHANNEL_LIST
static void wlan_hdd_cache_chann_mutex_destroy(struct hdd_context *hdd_ctx)
{
	qdf_mutex_destroy(&hdd_ctx->cache_channel_lock);
}
#else
static void wlan_hdd_cache_chann_mutex_destroy(struct hdd_context *hdd_ctx)
{
}
#endif

void hdd_wlan_exit(struct hdd_context *hdd_ctx)
{
	struct wiphy *wiphy = hdd_ctx->wiphy;

	hdd_enter();

	ucfg_dp_wait_complete_tasks();
	wlan_hdd_destroy_mib_stats_lock();
	hdd_debugfs_ini_config_deinit(hdd_ctx);
	hdd_debugfs_mws_coex_info_deinit(hdd_ctx);
	hdd_psoc_idle_timer_stop(hdd_ctx);
	hdd_regulatory_deinit(hdd_ctx);

	/*
	 * Powersave Offload Case
	 * Disable Idle Power Save Mode
	 */
	hdd_set_idle_ps_config(hdd_ctx, false);
	/* clear the scan queue in all the scenarios */
	wlan_cfg80211_cleanup_scan_queue(hdd_ctx->pdev, NULL);

	if (hdd_ctx->driver_status != DRIVER_MODULES_CLOSED) {
		hdd_unregister_wext_all_adapters(hdd_ctx);
		/*
		 * Cancel any outstanding scan requests.  We are about to close
		 * all of our adapters, but an adapter structure is what SME
		 * passes back to our callback function.  Hence if there
		 * are any outstanding scan requests then there is a
		 * race condition between when the adapter is closed and
		 * when the callback is invoked.  We try to resolve that
		 * race condition here by canceling any outstanding scans
		 * before we close the adapters.
		 * Note that the scans may be cancelled in an asynchronous
		 * manner, so ideally there needs to be some kind of
		 * synchronization.  Rather than introduce a new
		 * synchronization here, we will utilize the fact that we are
		 * about to Request Full Power, and since that is synchronized,
		 * the expectation is that by the time Request Full Power has
		 * completed, all scans will be cancelled
		 */
		hdd_abort_mac_scan_all_adapters(hdd_ctx);
		hdd_abort_sched_scan_all_adapters(hdd_ctx);

		hdd_stop_all_adapters(hdd_ctx);
		hdd_deinit_all_adapters(hdd_ctx, false);
	}

	unregister_netdevice_notifier(&hdd_netdev_notifier);

	qdf_dp_trace_deinit();

	hdd_wlan_stop_modules(hdd_ctx, false);

	hdd_driver_memdump_deinit();

	qdf_nbuf_deinit_replenish_timer();

	if (QDF_GLOBAL_MONITOR_MODE ==  hdd_get_conparam()) {
		hdd_info("Release wakelock for monitor mode!");
		qdf_wake_lock_release(&hdd_ctx->monitor_mode_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_MONITOR_MODE);
	}

	qdf_spinlock_destroy(&hdd_ctx->hdd_adapter_lock);
	qdf_spinlock_destroy(&hdd_ctx->connection_status_lock);
	wlan_hdd_cache_chann_mutex_destroy(hdd_ctx);

	osif_request_manager_deinit();

	hdd_close_all_adapters(hdd_ctx, false);

	wlansap_global_deinit();
	/*
	 * If there is re_init failure wiphy would have already de-registered
	 * check the wiphy status before un-registering again
	 */
	if (wiphy && wiphy->registered) {
		wiphy_unregister(wiphy);
		wlan_hdd_cfg80211_deinit(wiphy);
		hdd_lpass_notify_stop(hdd_ctx);
	}

	hdd_deinit_regulatory_update_event(hdd_ctx);
	hdd_exit_netlink_services(hdd_ctx);
#ifdef FEATURE_WLAN_CH_AVOID
	mutex_destroy(&hdd_ctx->avoid_freq_lock);
#endif

	/* This function should be invoked at the end of this api*/
	hdd_dump_func_call_map();
}

/**
 * hdd_wlan_notify_modem_power_state() - notify FW with modem power status
 * @state: state
 *
 * This function notifies FW with modem power status
 *
 * Return: 0 if successful, error number otherwise
 */
int hdd_wlan_notify_modem_power_state(int state)
{
	int status;
	QDF_STATUS qdf_status;
	struct hdd_context *hdd_ctx;
	mac_handle_t mac_handle;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle)
		return -EINVAL;

	qdf_status = sme_notify_modem_power_state(mac_handle, state);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		hdd_err("Fail to send notification with modem power state %d",
		       state);
		return -EINVAL;
	}
	return 0;
}

/**
 * hdd_post_cds_enable_config() - HDD post cds start config helper
 * @hdd_ctx: Pointer to the HDD
 *
 * Return: None
 */
QDF_STATUS hdd_post_cds_enable_config(struct hdd_context *hdd_ctx)
{
	QDF_STATUS qdf_ret_status;

	/*
	 * Send ready indication to the HDD.  This will kick off the MAC
	 * into a 'running' state and should kick off an initial scan.
	 */
	qdf_ret_status = sme_hdd_ready_ind(hdd_ctx->mac_handle);
	if (!QDF_IS_STATUS_SUCCESS(qdf_ret_status)) {
		hdd_err("sme_hdd_ready_ind() failed with status code %08d [x%08x]",
			qdf_ret_status, qdf_ret_status);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

struct hdd_adapter *hdd_get_first_valid_adapter(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_FIRST_VALID_ADAPTER;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter && adapter->magic == WLAN_HDD_ADAPTER_MAGIC) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return adapter;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NULL;
}

/* wake lock APIs for HDD */
void hdd_prevent_suspend(uint32_t reason)
{
	qdf_wake_lock_acquire(&wlan_wake_lock, reason);
}

void hdd_allow_suspend(uint32_t reason)
{
	qdf_wake_lock_release(&wlan_wake_lock, reason);
}

void hdd_prevent_suspend_timeout(uint32_t timeout, uint32_t reason)
{
	cds_host_diag_log_work(&wlan_wake_lock, timeout, reason);
	qdf_wake_lock_timeout_acquire(&wlan_wake_lock, timeout);
}

/* Initialize channel list in sme based on the country code */
QDF_STATUS hdd_set_sme_chan_list(struct hdd_context *hdd_ctx)
{
	return sme_init_chan_list(hdd_ctx->mac_handle,
				  hdd_ctx->reg.alpha2,
				  hdd_ctx->reg.cc_src);
}

/**
 * hdd_is_5g_supported() - check if hardware supports 5GHz
 * @hdd_ctx:	Pointer to the hdd context
 *
 * HDD function to know if hardware supports 5GHz
 *
 * Return:  true if hardware supports 5GHz
 */
bool hdd_is_5g_supported(struct hdd_context *hdd_ctx)
{
	if (!hdd_ctx)
		return true;

	if (hdd_ctx->curr_band != BAND_2G)
		return true;
	else
		return false;
}

bool hdd_is_2g_supported(struct hdd_context *hdd_ctx)
{
	if (!hdd_ctx)
		return false;

	if (hdd_ctx->curr_band != BAND_5G)
		return true;
	else
		return false;
}

static int hdd_wiphy_init(struct hdd_context *hdd_ctx)
{
	struct wiphy *wiphy;
	int ret_val;
	uint32_t channel_bonding_mode;

	wiphy = hdd_ctx->wiphy;

	/*
	 * The channel information in
	 * wiphy needs to be initialized before wiphy registration
	 */
	ret_val = hdd_regulatory_init(hdd_ctx, wiphy);
	if (ret_val) {
		hdd_err("regulatory init failed");
		return ret_val;
	}

	if (ucfg_pmo_get_suspend_mode(hdd_ctx->psoc) == PMO_SUSPEND_WOW) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
		wiphy->wowlan = &wowlan_support_reg_init;
#else
		wiphy->wowlan.flags = WIPHY_WOWLAN_ANY |
				      WIPHY_WOWLAN_MAGIC_PKT |
				      WIPHY_WOWLAN_DISCONNECT |
				      WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
				      WIPHY_WOWLAN_GTK_REKEY_FAILURE |
				      WIPHY_WOWLAN_EAP_IDENTITY_REQ |
				      WIPHY_WOWLAN_4WAY_HANDSHAKE |
				      WIPHY_WOWLAN_RFKILL_RELEASE;

		wiphy->wowlan.n_patterns = (WOW_MAX_FILTER_LISTS *
				    WOW_MAX_FILTERS_PER_LIST);
		wiphy->wowlan.pattern_min_len = WOW_MIN_PATTERN_SIZE;
		wiphy->wowlan.pattern_max_len = WOW_MAX_PATTERN_SIZE;
#endif
	}

	ucfg_mlme_get_channel_bonding_24ghz(hdd_ctx->psoc,
					    &channel_bonding_mode);
	if (hdd_ctx->obss_scan_offload) {
		hdd_debug("wmi_service_obss_scan supported");
	} else if (channel_bonding_mode) {
		hdd_debug("enable wpa_supp obss_scan");
		wiphy->features |= NL80211_FEATURE_NEED_OBSS_SCAN;
	}

	if (hdd_ctx->num_rf_chains == HDD_ANTENNA_MODE_2X2 &&
	    ucfg_mlme_is_chain_mask_supported(hdd_ctx->psoc)) {
		wiphy->available_antennas_tx = HDD_CHAIN_MODE_2X2;
		wiphy->available_antennas_rx = HDD_CHAIN_MODE_2X2;
	}
	/* registration of wiphy dev with cfg80211 */
	ret_val = wlan_hdd_cfg80211_register(wiphy);
	if (0 > ret_val) {
		hdd_err("wiphy registration failed");
		return ret_val;
	}

	/* Check the kernel version for upstream commit aced43ce780dc5 that
	 * has support for processing user cell_base hints when wiphy is
	 * self managed or check the backport flag for the same.
	 */
#if defined CFG80211_USER_HINT_CELL_BASE_SELF_MANAGED ||	\
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0))
	hdd_send_wiphy_regd_sync_event(hdd_ctx);
#endif

	pld_increment_driver_load_cnt(hdd_ctx->parent_dev);

	return ret_val;
}

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
#ifdef CLD_PM_QOS
#define PLD_REMOVE_PM_QOS(x)
#define PLD_REQUEST_PM_QOS(x, y)
#define HDD_PM_QOS_HIGH_TPUT_LATENCY_US 1

/**
 * hdd_pm_qos_update_cpu_mask() - Prepare CPU mask for PM_qos voting
 * @mask: return variable of cpumask for the TPUT
 * @enable_perf_cluster: Enable PERF cluster or not
 *
 * By default, the function sets CPU mask for silver cluster unless
 * enable_perf_cluster is set as true.
 *
 * Return: none
 */
static inline void hdd_pm_qos_update_cpu_mask(cpumask_t *mask,
					      bool enable_perf_cluster)
{
	cpumask_set_cpu(0, mask);
	cpumask_set_cpu(1, mask);
	cpumask_set_cpu(2, mask);
	cpumask_set_cpu(3, mask);

	if (enable_perf_cluster) {
		cpumask_set_cpu(4, mask);
		cpumask_set_cpu(5, mask);
		cpumask_set_cpu(6, mask);
	}
}

#ifdef MSM_PLATFORM
#define COPY_CPU_MASK(a, b) cpumask_copy(a, b)
#define DUMP_CPU_AFFINE() hdd_info("Set cpu_mask %*pb for affine_cores", \
			  cpumask_pr_args(&hdd_ctx->pm_qos_req.cpus_affine))
#else
#define COPY_CPU_MASK(a, b) /* no-op*/
#define DUMP_CPU_AFFINE() /* no-op*/
#endif

#ifdef CLD_DEV_PM_QOS

static inline void _hdd_pm_qos_update_request(struct hdd_context *hdd_ctx,
					      cpumask_t *pm_qos_cpu_mask,
					      unsigned int latency)
{
	int cpu;
	uint32_t default_latency;

	default_latency = wlan_hdd_get_default_pm_qos_cpu_latency();
	qdf_cpumask_copy(&hdd_ctx->qos_cpu_mask, pm_qos_cpu_mask);

	if (qdf_cpumask_empty(pm_qos_cpu_mask)) {
		for_each_present_cpu(cpu) {
			dev_pm_qos_update_request(
				&hdd_ctx->pm_qos_req[cpu],
				default_latency);
		}
		hdd_debug("Empty mask %*pb: Set latency %u",
			  qdf_cpumask_pr_args(&hdd_ctx->qos_cpu_mask),
			  default_latency);
	} else { /* Set latency to default for CPUs not included in mask */
		qdf_for_each_cpu_not(cpu, &hdd_ctx->qos_cpu_mask) {
			dev_pm_qos_update_request(
				&hdd_ctx->pm_qos_req[cpu],
				default_latency);
		}
		/* Set latency for CPUs included in mask */
		qdf_for_each_cpu(cpu, &hdd_ctx->qos_cpu_mask) {
			dev_pm_qos_update_request(
				&hdd_ctx->pm_qos_req[cpu],
				latency);
		}
		hdd_debug("For qos_cpu_mask %*pb set latency %u",
			  qdf_cpumask_pr_args(&hdd_ctx->qos_cpu_mask),
			  latency);
	}
}

/**
 * hdd_pm_qos_update_request() - API to request for pm_qos
 * @hdd_ctx: handle to hdd context
 * @pm_qos_cpu_mask: cpu_mask to apply
 *
 * Return: none
 */
static void hdd_pm_qos_update_request(struct hdd_context *hdd_ctx,
				      cpumask_t *pm_qos_cpu_mask)
{
	unsigned int latency;

	if (qdf_cpumask_empty(pm_qos_cpu_mask))
		latency = wlan_hdd_get_default_pm_qos_cpu_latency();
	else
		latency = HDD_PM_QOS_HIGH_TPUT_LATENCY_US;

	_hdd_pm_qos_update_request(hdd_ctx, pm_qos_cpu_mask, latency);
}

static inline void hdd_pm_qos_add_request(struct hdd_context *hdd_ctx)
{
	struct device *cpu_dev;
	int cpu;
	uint32_t default_latency = wlan_hdd_get_default_pm_qos_cpu_latency();


	qdf_cpumask_clear(&hdd_ctx->qos_cpu_mask);
	hdd_pm_qos_update_cpu_mask(&hdd_ctx->qos_cpu_mask, false);

	for_each_present_cpu(cpu) {
		cpu_dev = get_cpu_device(cpu);
		dev_pm_qos_add_request(cpu_dev, &hdd_ctx->pm_qos_req[cpu],
				       DEV_PM_QOS_RESUME_LATENCY,
				       default_latency);
		hdd_debug("Set qos_cpu_mask %*pb for affine_cores",
			 cpumask_pr_args(&hdd_ctx->qos_cpu_mask));
	}
}

static inline void hdd_pm_qos_remove_request(struct hdd_context *hdd_ctx)
{
	int cpu;

	for_each_present_cpu(cpu) {
		dev_pm_qos_remove_request(&hdd_ctx->pm_qos_req[cpu]);
		hdd_debug("Remove dev_pm_qos_request for all cpus: %d", cpu);
	}
	qdf_cpumask_clear(&hdd_ctx->qos_cpu_mask);
}

#else /* CLD_DEV_PM_QOS */

#if defined(CONFIG_SMP) && defined(MSM_PLATFORM)
/**
 * hdd_set_default_pm_qos_mask() - Update PM_qos request for AFFINE_CORES
 * @hdd_ctx: handle to hdd context
 *
 * Return: none
 */
static inline void hdd_set_default_pm_qos_mask(struct hdd_context *hdd_ctx)
{
	hdd_ctx->pm_qos_req.type = PM_QOS_REQ_AFFINE_CORES;
	qdf_cpumask_clear(&hdd_ctx->pm_qos_req.cpus_affine);
	hdd_pm_qos_update_cpu_mask(&hdd_ctx->pm_qos_req.cpus_affine, false);
}
#else
static inline void hdd_set_default_pm_qos_mask(struct hdd_context *hdd_ctx)
{
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
/**
 * hdd_pm_qos_update_request() - API to request for pm_qos
 * @hdd_ctx: handle to hdd context
 * @pm_qos_cpu_mask: cpu_mask to apply
 *
 * Return: none
 */
static inline void hdd_pm_qos_update_request(struct hdd_context *hdd_ctx,
					     cpumask_t *pm_qos_cpu_mask)
{
	COPY_CPU_MASK(&hdd_ctx->pm_qos_req.cpus_affine, pm_qos_cpu_mask);

	if (cpumask_empty(pm_qos_cpu_mask))
		cpu_latency_qos_update_request(&hdd_ctx->pm_qos_req,
					       PM_QOS_DEFAULT_VALUE);
	else
		cpu_latency_qos_update_request(&hdd_ctx->pm_qos_req, 1);
}

static inline void hdd_pm_qos_add_request(struct hdd_context *hdd_ctx)
{
	hdd_set_default_pm_qos_mask(hdd_ctx);
	cpu_latency_qos_add_request(&hdd_ctx->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	DUMP_CPU_AFFINE();
}

static inline void hdd_pm_qos_remove_request(struct hdd_context *hdd_ctx)
{
	cpu_latency_qos_remove_request(&hdd_ctx->pm_qos_req);
}
#else
static inline void hdd_pm_qos_update_request(struct hdd_context *hdd_ctx,
					     cpumask_t *pm_qos_cpu_mask)
{
	COPY_CPU_MASK(&hdd_ctx->pm_qos_req.cpus_affine, pm_qos_cpu_mask);

	if (cpumask_empty(pm_qos_cpu_mask))
		pm_qos_update_request(&hdd_ctx->pm_qos_req,
				      PM_QOS_DEFAULT_VALUE);
	else
		pm_qos_update_request(&hdd_ctx->pm_qos_req, 1);
}

static inline void hdd_pm_qos_add_request(struct hdd_context *hdd_ctx)
{
	hdd_set_default_pm_qos_mask(hdd_ctx);
	pm_qos_add_request(&hdd_ctx->pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
	DUMP_CPU_AFFINE();
}

static inline void hdd_pm_qos_remove_request(struct hdd_context *hdd_ctx)
{
	pm_qos_remove_request(&hdd_ctx->pm_qos_req);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0) */
#endif /* CLD_DEV_PM_QOS */

#else /* CLD_PM_QOS */
#define PLD_REMOVE_PM_QOS(x) pld_remove_pm_qos(x)
#define PLD_REQUEST_PM_QOS(x, y) pld_request_pm_qos(x, y)

static inline void hdd_pm_qos_add_request(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_pm_qos_remove_request(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_pm_qos_update_cpu_mask(cpumask_t *mask,
					      bool high_throughput)
{
}

static inline void hdd_pm_qos_update_request(struct hdd_context *hdd_ctx,
					     cpumask_t *pm_qos_cpu_mask)
{
}
#endif /* CLD_PM_QOS */

#if defined(CLD_PM_QOS)
#if defined(CLD_DEV_PM_QOS)
void wlan_hdd_set_pm_qos_request(struct hdd_context *hdd_ctx,
				 bool pm_qos_request)
{
	cpumask_t pm_qos_cpu_mask;

	cpumask_clear(&pm_qos_cpu_mask);
	if (pm_qos_request) {
		hdd_ctx->pm_qos_request = true;
		if (!hdd_ctx->hbw_requested) {
			hdd_pm_qos_update_cpu_mask(&pm_qos_cpu_mask, true);
			hdd_pm_qos_update_request(hdd_ctx, &pm_qos_cpu_mask);
			hdd_ctx->hbw_requested = true;
		}
	} else {
		if (hdd_ctx->hbw_requested) {
			hdd_pm_qos_update_cpu_mask(&pm_qos_cpu_mask, false);
			hdd_pm_qos_update_request(hdd_ctx, &pm_qos_cpu_mask);
			hdd_ctx->hbw_requested = false;
		}
		hdd_ctx->pm_qos_request = false;
	}
}
#else /* CLD_DEV_PM_QOS */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
void wlan_hdd_set_pm_qos_request(struct hdd_context *hdd_ctx,
				 bool pm_qos_request)
{
	if (pm_qos_request) {
		hdd_ctx->pm_qos_request = true;
		if (!hdd_ctx->hbw_requested) {
			cpumask_setall(&hdd_ctx->pm_qos_req.cpus_affine);
			cpu_latency_qos_update_request(
				&hdd_ctx->pm_qos_req,
				DISABLE_KRAIT_IDLE_PS_VAL);
			hdd_ctx->hbw_requested = true;
		}
	} else {
		if (hdd_ctx->hbw_requested) {
			cpumask_clear(&hdd_ctx->pm_qos_req.cpus_affine);
			cpu_latency_qos_update_request(&hdd_ctx->pm_qos_req,
						       PM_QOS_DEFAULT_VALUE);
			hdd_ctx->hbw_requested = false;
		}
		hdd_ctx->pm_qos_request = false;
	}
}
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)) */
void wlan_hdd_set_pm_qos_request(struct hdd_context *hdd_ctx,
				 bool pm_qos_request)
{
	if (pm_qos_request) {
		hdd_ctx->pm_qos_request = true;
		if (!hdd_ctx->hbw_requested) {
			cpumask_setall(&hdd_ctx->pm_qos_req.cpus_affine);
			pm_qos_update_request(&hdd_ctx->pm_qos_req,
					      DISABLE_KRAIT_IDLE_PS_VAL);
			hdd_ctx->hbw_requested = true;
		}
	} else {
		if (hdd_ctx->hbw_requested) {
			cpumask_clear(&hdd_ctx->pm_qos_req.cpus_affine);
			pm_qos_update_request(&hdd_ctx->pm_qos_req,
					      PM_QOS_DEFAULT_VALUE);
			hdd_ctx->hbw_requested = false;
		}
		hdd_ctx->pm_qos_request = false;
	}
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)) */
#endif /* CLD_DEV_PM_QOS*/
#endif /* CLD_PM_QOS */

#define HDD_BW_GET_DIFF(_x, _y) (unsigned long)((ULONG_MAX - (_y)) + (_x) + 1)

#ifdef WLAN_FEATURE_MSCS

static
void hdd_send_mscs_action_frame(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter)
{
	uint64_t mscs_vo_pkt_delta;
	unsigned long tx_vo_pkts = 0;
	unsigned int cpu;
	struct hdd_tx_rx_stats *stats = &adapter->hdd_stats.tx_rx_stats;
	uint32_t bus_bw_compute_interval;

	/*
	 * To disable MSCS feature in driver set mscs_pkt_threshold = 0
	 * in ini file.
	 */
	if (!hdd_ctx->config->mscs_pkt_threshold)
		return;

	for (cpu = 0; cpu < NUM_CPUS; cpu++)
		tx_vo_pkts += stats->per_cpu[cpu].tx_classified_ac[SME_AC_VO];

	if (!adapter->mscs_counter)
		adapter->mscs_prev_tx_vo_pkts = tx_vo_pkts;

	adapter->mscs_counter++;
	bus_bw_compute_interval =
		ucfg_dp_get_bus_bw_compute_interval(hdd_ctx->psoc);
	if (adapter->mscs_counter * bus_bw_compute_interval >=
	    hdd_ctx->config->mscs_voice_interval * 1000) {
		adapter->mscs_counter = 0;
		mscs_vo_pkt_delta =
				HDD_BW_GET_DIFF(tx_vo_pkts,
						adapter->mscs_prev_tx_vo_pkts);
		if (mscs_vo_pkt_delta > hdd_ctx->config->mscs_pkt_threshold &&
		    !mlme_get_is_mscs_req_sent(adapter->vdev))
			sme_send_mscs_action_frame(adapter->vdev_id);
	}
}
#else
static inline
void hdd_send_mscs_action_frame(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter)
{
}
#endif
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

/**
 * wlan_hdd_sta_get_dot11mode() - GET AP client count
 * @context: HDD context
 * @vdev_id: vdev ID
 * @dot11_mode: variable in which mode need to update.
 *
 * Return: true on success else false
 */
static inline
bool wlan_hdd_sta_get_dot11mode(hdd_cb_handle context, uint8_t vdev_id,
				enum qca_wlan_802_11_mode *dot11_mode)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;
	enum csr_cfgdot11mode mode;

	hdd_ctx = hdd_cb_handle_to_context(context);
	if (!hdd_ctx)
		return false;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter)
		return false;

	if (!hdd_cm_is_vdev_associated(adapter))
		return false;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	mode = sta_ctx->conn_info.dot11mode;
	*dot11_mode = hdd_convert_cfgdot11mode_to_80211mode(mode);
	return true;
}

/**
 * wlan_hdd_get_ap_client_count() - GET AP client count
 * @context: HDD context
 * @vdev_id: vdev ID
 * @client_count: variable in which number of client need to update.
 *
 * Return: true on success else false
 */
static inline
bool wlan_hdd_get_ap_client_count(hdd_cb_handle context, uint8_t vdev_id,
				  uint16_t *client_count)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct hdd_ap_ctx *ap_ctx;
	enum qca_wlan_802_11_mode i;

	hdd_ctx = hdd_cb_handle_to_context(context);
	if (!hdd_ctx) {
		hdd_err("hdd ctx is null");
		return false;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return false;
	}

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
	if (!ap_ctx->ap_active)
		return false;
	for (i = QCA_WLAN_802_11_MODE_11B; i < QCA_WLAN_802_11_MODE_INVALID;
	     i++)
		client_count[i] = ap_ctx->client_count[i];
	return true;
}

/**
 * wlan_hdd_sta_ndi_connected() - Check if NDI connected
 * @context: HDD context
 * @vdev_id: vdev ID
 *
 * Return: true if NDI connected else false
 */
static inline
bool wlan_hdd_sta_ndi_connected(hdd_cb_handle context, uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = hdd_cb_handle_to_context(context);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return false;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
	return false;
	}
	if (WLAN_HDD_GET_STATION_CTX_PTR(adapter)->conn_info.conn_state !=
					 eConnectionState_NdiConnected)
		return false;
	return true;
}

/**
 * wlan_hdd_pktlog_enable_disable() - Enable/Disable packet logging
 * @context: HDD context
 * @enable_disable_flag: Flag to enable/disable
 * @user_triggered: triggered through iwpriv
 * @size: buffer size to be used for packetlog
 *
 * Return: 0 on success; error number otherwise
 */
static inline
int wlan_hdd_pktlog_enable_disable(hdd_cb_handle context,
				   bool enable_disable_flag,
				   uint8_t user_triggered, int size)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return -EINVAL;
	}
	return hdd_pktlog_enable_disable(hdd_ctx, enable_disable_flag,
					 user_triggered, size);
}

/**
 * wlan_hdd_is_roaming_in_progress() - Check if roaming is in progress
 * @context: HDD context
 *
 * Return: true if roaming is in progress else false
 */
static inline bool wlan_hdd_is_roaming_in_progress(hdd_cb_handle context)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return false;
	}
	return hdd_is_roaming_in_progress(hdd_ctx);
}

/**
 * hdd_is_ap_active() - Check if AP is active
 * @context: HDD context
 * @vdev_id: Vdev ID
 *
 * Return: true if AP active else false
 */
static inline bool hdd_is_ap_active(hdd_cb_handle context, uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = hdd_cb_handle_to_context(context);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return false;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return false;
	}
	return WLAN_HDD_GET_AP_CTX_PTR(adapter)->ap_active;
}

/**
 * wlan_hdd_napi_apply_throughput_policy() - Apply NAPI policy
 * @context: HDD context
 * @tx_packets: tx_packets
 * @rx_packets: rx_packets
 *
 * Return: 0 on success else error code
 */
static inline
int wlan_hdd_napi_apply_throughput_policy(hdd_cb_handle context,
					  uint64_t tx_packets,
					  uint64_t rx_packets)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);
	int rc = 0;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return 0;
	}
	if (hdd_ctx->config->napi_cpu_affinity_mask)
		rc = hdd_napi_apply_throughput_policy(hdd_ctx, tx_packets,
						      rx_packets);
	return rc;
}

/**
 * hdd_is_link_adapter() - Check if adapter is link adapter
 * @context: HDD context
 * @vdev_id: Vdev ID
 *
 * Return: true if link adapter else false
 */
static inline bool hdd_is_link_adapter(hdd_cb_handle context, uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = hdd_cb_handle_to_context(context);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return false;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return false;
	}
	return hdd_adapter_is_link_adapter(adapter);
}

/**
 * hdd_get_pause_map() - Get pause map value
 * @context: HDD context
 * @vdev_id: Vdev ID
 *
 * Return: pause map value
 */
static inline
uint32_t hdd_get_pause_map(hdd_cb_handle context, uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);
	struct hdd_adapter *adapter;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return 0;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return 0;
	}
	return adapter->pause_map;
}

/**
 * hdd_any_adapter_connected() - Check if any adapter connected.
 * @context: HDD context
 *
 * Return: True if connected else false.
 */
static inline bool hdd_any_adapter_connected(hdd_cb_handle context)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return false;
	}

	return hdd_is_any_adapter_connected(hdd_ctx);
}

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * hdd_pld_request_pm_qos() - Request PLD PM QoS request
 * @context: HDD context
 *
 * Return: None
 */
static inline void hdd_pld_request_pm_qos(hdd_cb_handle context)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	if (!hdd_ctx->hbw_requested) {
		PLD_REQUEST_PM_QOS(hdd_ctx->parent_dev, 1);
		hdd_ctx->hbw_requested = true;
	}
}

/**
 * hdd_pld_remove_pm_qos() - Remove PLD PM QoS request
 * @context: HDD context
 *
 * Return: None
 */
static inline void hdd_pld_remove_pm_qos(hdd_cb_handle context)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	if (hdd_ctx->hbw_requested &&
	    !hdd_ctx->pm_qos_request) {
		PLD_REMOVE_PM_QOS(hdd_ctx->parent_dev);
		hdd_ctx->hbw_requested = false;
	}
}

/**
 * wlan_hdd_pm_qos_update_request() - Update PM QoS request
 * @context: HDD context
 * @pm_qos_cpu_mask: CPU mask
 *
 * Return: None
 */
static inline void
wlan_hdd_pm_qos_update_request(hdd_cb_handle context,
			       cpumask_t *pm_qos_cpu_mask)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	if (!hdd_ctx->pm_qos_request)
		hdd_pm_qos_update_request(hdd_ctx, pm_qos_cpu_mask);
}

/**
 * wlan_hdd_pm_qos_add_request() - Add PM QoS request
 * @context: HDD context
 *
 * Return: None
 */
static inline void wlan_hdd_pm_qos_add_request(hdd_cb_handle context)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	hdd_pm_qos_add_request(hdd_ctx);
}

/**
 * wlan_hdd_pm_qos_remove_request() - remove PM QoS request
 * @context: HDD context
 *
 * Return: None
 */
static inline void wlan_hdd_pm_qos_remove_request(hdd_cb_handle context)
{
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	hdd_pm_qos_remove_request(hdd_ctx);
}

/**
 * wlan_hdd_send_mscs_action_frame() - Send MSCS action frame
 * @context: HDD context
 * @vdev_id: Vdev ID
 *
 * Return: None
 */
static inline void wlan_hdd_send_mscs_action_frame(hdd_cb_handle context,
						   uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = hdd_cb_handle_to_context(context);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return;
	}
	hdd_send_mscs_action_frame(hdd_ctx, adapter);
}

#else
static inline void hdd_pld_remove_pm_qos(hdd_cb_handle context)
{
}

static inline void hdd_pld_request_pm_qos(hdd_cb_handle context)
{
}

static inline void
wlan_hdd_pm_qos_update_request(hdd_cb_handle context,
			       cpumask_t *pm_qos_cpu_mask)
{
}

static inline void wlan_hdd_pm_qos_add_request(hdd_cb_handle context)
{
}

static inline void wlan_hdd_pm_qos_remove_request(hdd_cb_handle context)
{
}

static inline void wlan_hdd_send_mscs_action_frame(hdd_cb_handle context,
						   uint8_t vdev_id)
{
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && \
defined(FEATURE_RX_LINKSPEED_ROAM_TRIGGER)
void wlan_hdd_link_speed_update(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				bool is_link_speed_good)
{
	ucfg_cm_roam_link_speed_update(psoc, vdev_id, is_link_speed_good);
}
#endif

/**
 * hdd_dp_register_callbacks() - Register DP callbacks with HDD
 * @hdd_ctx: HDD context
 *
 * Return: None
 */
static void hdd_dp_register_callbacks(struct hdd_context *hdd_ctx)
{
	struct wlan_dp_psoc_callbacks cb_obj = {0};

	cb_obj.callback_ctx = (hdd_cb_handle)hdd_ctx;
	cb_obj.wlan_dp_sta_get_dot11mode = wlan_hdd_sta_get_dot11mode;
	cb_obj.wlan_dp_get_ap_client_count = wlan_hdd_get_ap_client_count;
	cb_obj.wlan_dp_sta_ndi_connected = wlan_hdd_sta_ndi_connected;
	cb_obj.dp_any_adapter_connected = hdd_any_adapter_connected;
	cb_obj.dp_send_svc_nlink_msg = wlan_hdd_send_svc_nlink_msg;
	cb_obj.dp_pld_remove_pm_qos = hdd_pld_remove_pm_qos;
	cb_obj.dp_pld_request_pm_qos = hdd_pld_request_pm_qos;
	cb_obj.dp_pktlog_enable_disable = wlan_hdd_pktlog_enable_disable;
	cb_obj.dp_pm_qos_update_request = wlan_hdd_pm_qos_update_request;
	cb_obj.dp_pm_qos_add_request = wlan_hdd_pm_qos_add_request;
	cb_obj.dp_pm_qos_remove_request = wlan_hdd_pm_qos_remove_request;
	cb_obj.dp_send_mscs_action_frame = wlan_hdd_send_mscs_action_frame;
	cb_obj.dp_is_roaming_in_progress = wlan_hdd_is_roaming_in_progress;
	cb_obj.wlan_dp_display_tx_multiq_stats =
		wlan_hdd_display_tx_multiq_stats;
	cb_obj.wlan_dp_display_netif_queue_history =
		wlan_hdd_display_netif_queue_history;
	cb_obj.dp_is_ap_active = hdd_is_ap_active;
	cb_obj.dp_napi_apply_throughput_policy =
		wlan_hdd_napi_apply_throughput_policy;
	cb_obj.dp_is_link_adapter = hdd_is_link_adapter;
	cb_obj.dp_nud_failure_work = hdd_nud_failure_work;
	cb_obj.dp_get_pause_map = hdd_get_pause_map;

	cb_obj.dp_get_netdev_by_vdev_mac =
		hdd_get_netdev_by_vdev_mac;
	cb_obj.dp_get_tx_resource = hdd_get_tx_resource;
	cb_obj.dp_get_tx_flow_low_watermark = hdd_get_tx_flow_low_watermark;
	cb_obj.dp_get_tsf_time = hdd_get_tsf_time_cb;
	cb_obj.dp_tsf_timestamp_rx = hdd_tsf_timestamp_rx;
	cb_obj.dp_gro_rx_legacy_get_napi = hdd_legacy_gro_get_napi;
	cb_obj.link_monitoring_cb = wlan_hdd_link_speed_update;

	os_if_dp_register_hdd_callbacks(hdd_ctx->psoc, &cb_obj);
}

/**
 * __hdd_adapter_param_update_work() - Gist of the work to process
 *				       netdev feature update.
 * @adapter: pointer to adapter structure
 *
 * This function assumes that the adapter pointer is always valid.
 * So the caller should always validate adapter pointer before calling
 * this function
 *
 * Returns: None
 */
static inline void
__hdd_adapter_param_update_work(struct hdd_adapter *adapter)
{
	/**
	 * This check is needed in case the work got scheduled after the
	 * interface got disconnected.
	 * Netdev features update is to be done only after the connection,
	 * since the connection mode plays an important role in identifying
	 * the features that are to be updated.
	 * So in case of interface disconnect skip feature update.
	 */
	if (!hdd_cm_is_vdev_associated(adapter))
		return;

	hdd_netdev_update_features(adapter);
}

/**
 * hdd_adapter_param_update_work() - work to process the netdev features
 *				     update.
 * @arg: private data passed to work
 *
 * Returns: None
 */
static void hdd_adapter_param_update_work(void *arg)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct hdd_adapter *adapter = arg;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	if (!hdd_ctx)
		return;

	hdd_adapter_ops_record_event(hdd_ctx,
				     WLAN_HDD_ADAPTER_OPS_WORK_SCHED,
				     WLAN_INVALID_VDEV_ID);

	if (hdd_validate_adapter(adapter))
		return;

	errno = osif_vdev_sync_op_start(adapter->dev, &vdev_sync);
	if (errno)
		return;

	__hdd_adapter_param_update_work(adapter);

	osif_vdev_sync_op_stop(vdev_sync);
}

QDF_STATUS hdd_init_adapter_ops_wq(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	hdd_ctx->adapter_ops_wq =
		qdf_alloc_high_prior_ordered_workqueue("hdd_adapter_ops_wq");
	if (!hdd_ctx->adapter_ops_wq)
		return QDF_STATUS_E_NOMEM;

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

void hdd_deinit_adapter_ops_wq(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	qdf_flush_workqueue(0, hdd_ctx->adapter_ops_wq);
	qdf_destroy_workqueue(0, hdd_ctx->adapter_ops_wq);

	hdd_exit();
}

QDF_STATUS hdd_adapter_feature_update_work_init(struct hdd_adapter *adapter)
{
	QDF_STATUS status;

	hdd_enter();

	status = qdf_create_work(0, &adapter->netdev_features_update_work,
				 hdd_adapter_param_update_work, adapter);
	adapter->netdev_features_update_work_status = HDD_WORK_INITIALIZED;

	hdd_exit();

	return status;
}

void hdd_adapter_feature_update_work_deinit(struct hdd_adapter *adapter)
{
	hdd_enter();

	if (adapter->netdev_features_update_work_status !=
	    HDD_WORK_INITIALIZED) {
		hdd_debug("work not yet init");
		return;
	}
	qdf_cancel_work(&adapter->netdev_features_update_work);
	qdf_flush_work(&adapter->netdev_features_update_work);
	adapter->netdev_features_update_work_status = HDD_WORK_UNINITIALIZED;

	hdd_exit();
}

#define HDD_DUMP_STAT_HELP(STAT_ID) \
	hdd_nofl_debug("%u -- %s", STAT_ID, (# STAT_ID))
/**
 * hdd_display_stats_help() - print statistics help
 *
 * Return: none
 */
static void hdd_display_stats_help(void)
{
	hdd_nofl_debug("iwpriv wlan0 dumpStats [option] - dump statistics");
	hdd_nofl_debug("iwpriv wlan0 clearStats [option] - clear statistics");
	hdd_nofl_debug("options:");
	HDD_DUMP_STAT_HELP(CDP_TXRX_PATH_STATS);
	HDD_DUMP_STAT_HELP(CDP_TXRX_HIST_STATS);
	HDD_DUMP_STAT_HELP(CDP_TXRX_TSO_STATS);
	HDD_DUMP_STAT_HELP(CDP_HDD_NETIF_OPER_HISTORY);
	HDD_DUMP_STAT_HELP(CDP_DUMP_TX_FLOW_POOL_INFO);
	HDD_DUMP_STAT_HELP(CDP_TXRX_DESC_STATS);
	HDD_DUMP_STAT_HELP(CDP_HIF_STATS);
	HDD_DUMP_STAT_HELP(CDP_NAPI_STATS);
	HDD_DUMP_STAT_HELP(CDP_DP_NAPI_STATS);
	HDD_DUMP_STAT_HELP(CDP_DP_RX_THREAD_STATS);
}

int hdd_wlan_dump_stats(struct hdd_adapter *adapter, int stats_id)
{
	int ret = 0;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	hdd_debug("stats_id %d", stats_id);

	switch (stats_id) {
	case CDP_TXRX_HIST_STATS:
		ucfg_wlan_dp_display_tx_rx_histogram(hdd_ctx->psoc);
		break;
	case CDP_HDD_NETIF_OPER_HISTORY:
		wlan_hdd_display_adapter_netif_queue_history(adapter);
		break;
	case CDP_HIF_STATS:
		hdd_display_hif_stats();
		break;
	case CDP_NAPI_STATS:
		if (hdd_display_napi_stats()) {
			hdd_err("error displaying napi stats");
			ret = -EFAULT;
		}
		break;
	case CDP_DP_RX_THREAD_STATS:
		ucfg_dp_txrx_ext_dump_stats(cds_get_context(QDF_MODULE_ID_SOC),
					    CDP_DP_RX_THREAD_STATS);
		break;
	case CDP_DISCONNECT_STATS:
		sme_display_disconnect_stats(hdd_ctx->mac_handle,
					     adapter->vdev_id);
		break;
	default:
		status = cdp_display_stats(cds_get_context(QDF_MODULE_ID_SOC),
					   stats_id,
					   QDF_STATS_VERBOSITY_LEVEL_HIGH);
		if (status == QDF_STATUS_E_INVAL) {
			hdd_display_stats_help();
			ret = -EINVAL;
		}
		break;
	}
	return ret;
}

int hdd_wlan_clear_stats(struct hdd_adapter *adapter, int stats_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	hdd_debug("stats_id %d", stats_id);

	switch (stats_id) {
	case CDP_HDD_STATS:
		ucfg_dp_clear_net_dev_stats(adapter->dev);
		memset(&adapter->hdd_stats, 0, sizeof(adapter->hdd_stats));
		break;
	case CDP_TXRX_HIST_STATS:
		ucfg_wlan_dp_clear_tx_rx_histogram(adapter->hdd_ctx->psoc);
		break;
	case CDP_HDD_NETIF_OPER_HISTORY:
		wlan_hdd_clear_netif_queue_history(adapter->hdd_ctx);
		break;
	case CDP_HIF_STATS:
		hdd_clear_hif_stats();
		break;
	case CDP_NAPI_STATS:
		hdd_clear_napi_stats();
		break;
	default:
		status = cdp_clear_stats(cds_get_context(QDF_MODULE_ID_SOC),
					 OL_TXRX_PDEV_ID,
					 stats_id);
		if (status != QDF_STATUS_SUCCESS)
			hdd_debug("Failed to dump stats for stats_id: %d",
				  stats_id);
		break;
	}

	return qdf_status_to_os_return(status);
}

/* length of the netif queue log needed per adapter */
#define ADAP_NETIFQ_LOG_LEN ((20 * WLAN_REASON_TYPE_MAX) + 50)

/**
 * hdd_display_netif_queue_history_compact() - display compact netifq history
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
static void
hdd_display_netif_queue_history_compact(struct hdd_context *hdd_ctx)
{
	int adapter_num = 0;
	int i;
	int bytes_written;
	u32 tbytes;
	qdf_time_t total, pause, unpause, curr_time, delta;
	char temp_str[20 * WLAN_REASON_TYPE_MAX];
	char *comb_log_str;
	uint32_t comb_log_str_size;
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid =
			NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY_COMPACT;

	comb_log_str_size = (ADAP_NETIFQ_LOG_LEN * WLAN_MAX_VDEVS) + 1;
	comb_log_str = qdf_mem_malloc(comb_log_str_size);
	if (!comb_log_str)
		return;

	bytes_written = 0;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		curr_time = qdf_system_ticks();
		total = curr_time - adapter->start_time;
		delta = curr_time - adapter->last_time;

		if (adapter->pause_map) {
			pause = adapter->total_pause_time + delta;
			unpause = adapter->total_unpause_time;
		} else {
			unpause = adapter->total_unpause_time + delta;
			pause = adapter->total_pause_time;
		}

		tbytes = 0;
		qdf_mem_zero(temp_str, sizeof(temp_str));
		for (i = WLAN_CONTROL_PATH; i < WLAN_REASON_TYPE_MAX; i++) {
			if (adapter->queue_oper_stats[i].pause_count == 0)
				continue;
			tbytes +=
				snprintf(
					&temp_str[tbytes],
					(tbytes >= sizeof(temp_str) ?
					0 : sizeof(temp_str) - tbytes),
					"%d(%d,%d) ",
					i,
					adapter->queue_oper_stats[i].
								pause_count,
					adapter->queue_oper_stats[i].
								unpause_count);
		}
		if (tbytes >= sizeof(temp_str))
			hdd_warn("log truncated");

		bytes_written += snprintf(&comb_log_str[bytes_written],
			bytes_written >= comb_log_str_size ? 0 :
					comb_log_str_size - bytes_written,
			"[%d %d] (%d) %u/%ums %s|",
			adapter->vdev_id, adapter->device_mode,
			adapter->pause_map,
			qdf_system_ticks_to_msecs(pause),
			qdf_system_ticks_to_msecs(total),
			temp_str);

		adapter_num++;
		/* dev_put has to be done here */
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	/* using QDF_TRACE to avoid printing function name */
	QDF_TRACE(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_INFO_LOW,
		  "STATS |%s", comb_log_str);

	if (bytes_written >= comb_log_str_size)
		hdd_warn("log string truncated");

	qdf_mem_free(comb_log_str);
}

/* Max size of a single netdev tx queue state string. e.g. "1: 0x1" */
#define HDD_NETDEV_TX_Q_STATE_STRLEN 15
/**
 * wlan_hdd_display_adapter_netif_queue_stats() - display adapter based
 * netif queue stats
 * @adapter: hdd adapter
 *
 * Return: none
 */
static void
wlan_hdd_display_adapter_netif_queue_stats(struct hdd_adapter *adapter)
{
	int i;
	qdf_time_t total, pause, unpause, curr_time, delta;
	struct hdd_netif_queue_history *q_hist_ptr;
	char q_status_buf[NUM_TX_QUEUES * HDD_NETDEV_TX_Q_STATE_STRLEN] = {0};

	hdd_nofl_debug("Netif queue operation statistics:");
	hdd_nofl_debug("vdev_id %d device mode %d",
		       adapter->vdev_id, adapter->device_mode);
	hdd_nofl_debug("Current pause_map %x", adapter->pause_map);
	curr_time = qdf_system_ticks();
	total = curr_time - adapter->start_time;
	delta = curr_time - adapter->last_time;
	if (adapter->pause_map) {
		pause = adapter->total_pause_time + delta;
		unpause = adapter->total_unpause_time;
	} else {
		unpause = adapter->total_unpause_time + delta;
		pause = adapter->total_pause_time;
	}
	hdd_nofl_debug("Total: %ums Pause: %ums Unpause: %ums",
		       qdf_system_ticks_to_msecs(total),
		       qdf_system_ticks_to_msecs(pause),
		       qdf_system_ticks_to_msecs(unpause));
	hdd_nofl_debug("reason_type: pause_cnt: unpause_cnt: pause_time");

	for (i = WLAN_CONTROL_PATH; i < WLAN_REASON_TYPE_MAX; i++) {
		qdf_time_t pause_delta = 0;

		if (adapter->pause_map & (1 << i))
			pause_delta = delta;

		/* using hdd_log to avoid printing function name */
		hdd_nofl_debug("%s: %d: %d: %ums",
			       hdd_reason_type_to_string(i),
			       adapter->queue_oper_stats[i].pause_count,
			       adapter->queue_oper_stats[i].
			       unpause_count,
			       qdf_system_ticks_to_msecs(
			       adapter->queue_oper_stats[i].
			       total_pause_time + pause_delta));
	}

	hdd_nofl_debug("Netif queue operation history: Total entries: %d current index %d(-1) time %u",
		       WLAN_HDD_MAX_HISTORY_ENTRY,
		       adapter->history_index,
		       qdf_system_ticks_to_msecs(qdf_system_ticks()));

	hdd_nofl_debug("%2s%20s%50s%30s%10s  %s",
		       "#", "time(ms)", "action_type", "reason_type",
		       "pause_map", "netdev-queue-status");

	for (i = 0; i < WLAN_HDD_MAX_HISTORY_ENTRY; i++) {
		/* using hdd_log to avoid printing function name */
		if (adapter->queue_oper_history[i].time == 0)
			continue;
		q_hist_ptr = &adapter->queue_oper_history[i];
		wlan_hdd_dump_queue_history_state(q_hist_ptr,
						  q_status_buf,
						  sizeof(q_status_buf));
		hdd_nofl_debug("%2d%20u%50s%30s%10x  %s",
			       i, qdf_system_ticks_to_msecs(
				adapter->queue_oper_history[i].time),
				   hdd_action_type_to_string(
				adapter->queue_oper_history[i].
					netif_action),
				   hdd_reason_type_to_string(
				adapter->queue_oper_history[i].
					netif_reason),
				   adapter->queue_oper_history[i].pause_map,
				   q_status_buf);
	}
}

void
wlan_hdd_display_adapter_netif_queue_history(struct hdd_adapter *adapter)
{
	wlan_hdd_display_adapter_netif_queue_stats(adapter);
}

/**
 * wlan_hdd_display_netif_queue_history() - display netif queue history
 * @context: hdd context
 * @verb_lvl: verbosity level
 *
 * Return: none
 */
void
wlan_hdd_display_netif_queue_history(hdd_cb_handle context,
				     enum qdf_stats_verbosity_level verb_lvl)
{
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	struct hdd_context *hdd_ctx = hdd_cb_handle_to_context(context);
	wlan_net_dev_ref_dbgid dbgid =
				NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	if (verb_lvl == QDF_STATS_VERBOSITY_LEVEL_LOW) {
		hdd_display_netif_queue_history_compact(hdd_ctx);
		return;
	}

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->vdev_id == CDP_INVALID_VDEV_ID) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}
		wlan_hdd_display_adapter_netif_queue_stats(adapter);
		/* dev_put has to be done here */
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

/**
 * wlan_hdd_clear_netif_queue_history() - clear netif queue operation history
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void wlan_hdd_clear_netif_queue_history(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_CLEAR_NETIF_QUEUE_HISTORY;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		qdf_mem_zero(adapter->queue_oper_stats,
					sizeof(adapter->queue_oper_stats));
		qdf_mem_zero(adapter->queue_oper_history,
					sizeof(adapter->queue_oper_history));
		adapter->history_index = 0;
		adapter->start_time = adapter->last_time = qdf_system_ticks();
		adapter->total_pause_time = 0;
		adapter->total_unpause_time = 0;
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
/**
 * hdd_init_offloaded_packets_ctx() - Initialize offload packets context
 * @hdd_ctx: hdd global context
 *
 * Return: none
 */
static void hdd_init_offloaded_packets_ctx(struct hdd_context *hdd_ctx)
{
	uint8_t i;

	mutex_init(&hdd_ctx->op_ctx.op_lock);
	for (i = 0; i < MAXNUM_PERIODIC_TX_PTRNS; i++) {
		hdd_ctx->op_ctx.op_table[i].request_id = MAX_REQUEST_ID;
		hdd_ctx->op_ctx.op_table[i].pattern_id = i;
	}
}
#else
static void hdd_init_offloaded_packets_ctx(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef WLAN_FEATURE_WOW_PULSE
/**
 * wlan_hdd_set_wow_pulse() - call SME to send wmi cmd of wow pulse
 * @hdd_ctx: struct hdd_context structure pointer
 * @enable: enable or disable this behaviour
 *
 * Return: int
 */
static int wlan_hdd_set_wow_pulse(struct hdd_context *hdd_ctx, bool enable)
{
	struct wow_pulse_mode wow_pulse_set_info;
	QDF_STATUS status;

	hdd_debug("wow pulse enable flag is %d", enable);

	if (!ucfg_pmo_is_wow_pulse_enabled(hdd_ctx->psoc))
		return 0;

	/* prepare the request to send to SME */
	if (enable == true) {
		wow_pulse_set_info.wow_pulse_enable = true;
		wow_pulse_set_info.wow_pulse_pin =
			ucfg_pmo_get_wow_pulse_pin(hdd_ctx->psoc);

		wow_pulse_set_info.wow_pulse_interval_high =
		    ucfg_pmo_get_wow_pulse_interval_high(hdd_ctx->psoc);

		wow_pulse_set_info.wow_pulse_interval_low =
		    ucfg_pmo_get_wow_pulse_interval_low(hdd_ctx->psoc);

		wow_pulse_set_info.wow_pulse_repeat_count =
		    ucfg_pmo_get_wow_pulse_repeat_count(hdd_ctx->psoc);

		wow_pulse_set_info.wow_pulse_init_state =
		    ucfg_pmo_get_wow_pulse_init_state(hdd_ctx->psoc);
	} else {
		wow_pulse_set_info.wow_pulse_enable = false;
		wow_pulse_set_info.wow_pulse_pin = 0;
		wow_pulse_set_info.wow_pulse_interval_low = 0;
		wow_pulse_set_info.wow_pulse_interval_high = 0;
		wow_pulse_set_info.wow_pulse_repeat_count = 0;
		wow_pulse_set_info.wow_pulse_init_state = 0;
	}
	hdd_debug("enable %d pin %d low %d high %d count %d init %d",
		  wow_pulse_set_info.wow_pulse_enable,
		  wow_pulse_set_info.wow_pulse_pin,
		  wow_pulse_set_info.wow_pulse_interval_low,
		  wow_pulse_set_info.wow_pulse_interval_high,
		  wow_pulse_set_info.wow_pulse_repeat_count,
		  wow_pulse_set_info.wow_pulse_init_state);

	status = sme_set_wow_pulse(&wow_pulse_set_info);
	if (QDF_STATUS_E_FAILURE == status) {
		hdd_debug("sme_set_wow_pulse failure!");
		return -EIO;
	}
	hdd_debug("sme_set_wow_pulse success!");
	return 0;
}
#else
static inline int wlan_hdd_set_wow_pulse(struct hdd_context *hdd_ctx, bool enable)
{
	return 0;
}
#endif

#ifdef WLAN_FEATURE_FASTPATH

/**
 * hdd_enable_fastpath() - Enable fastpath if enabled in config INI
 * @hdd_ctx: hdd context
 * @context: lower layer context
 *
 * Return: none
 */
void hdd_enable_fastpath(struct hdd_context *hdd_ctx,
			 void *context)
{
	if (cfg_get(hdd_ctx->psoc, CFG_DP_ENABLE_FASTPATH))
		hif_enable_fastpath(context);
}
#endif

#if defined(FEATURE_WLAN_CH_AVOID)
/**
 * hdd_set_thermal_level_cb() - set thermal level callback function
 * @hdd_handle:	opaque handle for the hdd context
 * @level:	thermal level
 *
 * Change IPA data path to SW path when the thermal throttle level greater
 * than 0, and restore the original data path when throttle level is 0
 *
 * Return: none
 */
static void hdd_set_thermal_level_cb(hdd_handle_t hdd_handle, u_int8_t level)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);

	/* Change IPA to SW path when throttle level greater than 0 */
	if (level > THROTTLE_LEVEL_0)
		ucfg_ipa_send_mcc_scc_msg(hdd_ctx->pdev, true);
	else
		/* restore original concurrency mode */
		ucfg_ipa_send_mcc_scc_msg(hdd_ctx->pdev, hdd_ctx->mcc_mode);
}
#else
/**
 * hdd_set_thermal_level_cb() - set thermal level callback function
 * @hdd_handle:	opaque handle for the hdd context
 * @level:	thermal level
 *
 * Change IPA data path to SW path when the thermal throttle level greater
 * than 0, and restore the original data path when throttle level is 0
 *
 * Return: none
 */
static void hdd_set_thermal_level_cb(hdd_handle_t hdd_handle, u_int8_t level)
{
}
#endif

/**
 * hdd_switch_sap_channel() - Move SAP to the given channel
 * @adapter: AP adapter
 * @channel: Channel
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Moves the SAP interface by invoking the function which
 * executes the callback to perform channel switch using (E)CSA.
 *
 * Return: None
 */
void hdd_switch_sap_channel(struct hdd_adapter *adapter, uint8_t channel,
			    bool forced)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct hdd_context *hdd_ctx;
	mac_handle_t mac_handle;

	if (!adapter) {
		hdd_err("invalid adapter");
		return;
	}

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

	mac_handle = hdd_adapter_get_mac_handle(adapter);
	if (!mac_handle) {
		hdd_err("invalid MAC handle");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	hdd_debug("chan:%d width:%d",
		channel, hdd_ap_ctx->sap_config.ch_width_orig);

	policy_mgr_change_sap_channel_with_csa(
		hdd_ctx->psoc, adapter->vdev_id,
		wlan_reg_legacy_chan_to_freq(hdd_ctx->pdev, channel),
		hdd_ap_ctx->sap_config.ch_width_orig, forced);
}

QDF_STATUS hdd_switch_sap_chan_freq(struct hdd_adapter *adapter,
				    qdf_freq_t chan_freq, bool forced)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct hdd_context *hdd_ctx;

	if (hdd_validate_adapter(adapter))
		return QDF_STATUS_E_INVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if(wlan_hdd_validate_context(hdd_ctx))
		return QDF_STATUS_E_INVAL;

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

	hdd_debug("chan freq:%d width:%d",
		  chan_freq, hdd_ap_ctx->sap_config.ch_width_orig);

	return policy_mgr_change_sap_channel_with_csa(hdd_ctx->psoc,
						      adapter->vdev_id,
						      chan_freq,
						      hdd_ap_ctx->sap_config.ch_width_orig,
						      forced);
}

int hdd_update_acs_timer_reason(struct hdd_adapter *adapter, uint8_t reason)
{
	struct hdd_external_acs_timer_context *timer_context;
	int status;
	QDF_STATUS qdf_status;

	set_bit(VENDOR_ACS_RESPONSE_PENDING, &adapter->event_flags);

	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&adapter->session.
					ap.vendor_acs_timer)) {
		qdf_mc_timer_stop(&adapter->session.ap.vendor_acs_timer);
	}
	timer_context = (struct hdd_external_acs_timer_context *)
			adapter->session.ap.vendor_acs_timer.user_data;
	timer_context->reason = reason;
	qdf_status =
		qdf_mc_timer_start(&adapter->session.ap.vendor_acs_timer,
				   WLAN_VENDOR_ACS_WAIT_TIME);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		hdd_err("failed to start external acs timer");
		return -ENOSPC;
	}
	/* Update config to application */
	status = hdd_cfg80211_update_acs_config(adapter, reason);
	hdd_info("Updated ACS config to nl with reason %d", reason);

	return status;
}

#ifdef FEATURE_WLAN_CH_AVOID_EXT
uint32_t wlan_hdd_get_restriction_mask(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->restriction_mask;
}

void wlan_hdd_set_restriction_mask(struct hdd_context *hdd_ctx)
{
	hdd_ctx->restriction_mask =
		hdd_ctx->coex_avoid_freq_list.restriction_mask;
}
#else
uint32_t wlan_hdd_get_restriction_mask(struct hdd_context *hdd_ctx)
{
	return -EINVAL;
}

void wlan_hdd_set_restriction_mask(struct hdd_context *hdd_ctx)
{
}
#endif

#if defined(FEATURE_WLAN_CH_AVOID)
/**
 * hdd_store_sap_restart_channel() - store sap restart channel
 * @restart_chan: restart channel
 * @restart_chan_store: pointer to restart channel store
 *
 * The function will store new sap restart channel.
 *
 * Return - none
 */
static void
hdd_store_sap_restart_channel(qdf_freq_t restart_chan, qdf_freq_t *restart_chan_store)
{
	uint8_t i;

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (*(restart_chan_store + i) == restart_chan)
			return;

		if (*(restart_chan_store + i))
			continue;

		*(restart_chan_store + i) = restart_chan;
		return;
	}
}

/**
 * hdd_unsafe_channel_restart_sap() - restart sap if sap is on unsafe channel
 * @hdd_ctxt: hdd context pointer
 *
 * hdd_unsafe_channel_restart_sap check all unsafe channel list
 * and if ACS is enabled, driver will ask userspace to restart the
 * sap. User space on LTE coex indication restart driver.
 *
 * Return - none
 */
void hdd_unsafe_channel_restart_sap(struct hdd_context *hdd_ctxt)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	uint32_t i;
	bool found = false;
	qdf_freq_t restart_chan_store[SAP_MAX_NUM_SESSION] = {0};
	uint8_t scc_on_lte_coex = 0;
	uint32_t restart_freq, ap_chan_freq;
	bool value;
	QDF_STATUS status;
	bool is_acs_support_for_dfs_ltecoex = cfg_default(CFG_USER_ACS_DFS_LTE);
	bool is_vendor_acs_support =
		cfg_default(CFG_USER_AUTO_CHANNEL_SELECTION);
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_UNSAFE_CHANNEL_RESTART_SAP;

	hdd_for_each_adapter_dev_held_safe(hdd_ctxt, adapter, next_adapter,
					   dbgid) {
		if (!(adapter->device_mode == QDF_SAP_MODE &&
		    adapter->session.ap.sap_config.acs_cfg.acs_mode)) {
			hdd_debug_rl("skip device mode:%d acs:%d",
				     adapter->device_mode,
			adapter->session.ap.sap_config.acs_cfg.acs_mode);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		ap_chan_freq = adapter->session.ap.operating_chan_freq;

		found = false;
		status =
		ucfg_policy_mgr_get_sta_sap_scc_lte_coex_chnl(hdd_ctxt->psoc,
							      &scc_on_lte_coex);
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("can't get scc on lte coex chnl, use def");
		/*
		 * If STA+SAP is doing SCC & g_sta_sap_scc_on_lte_coex_chan
		 * is set, no need to move SAP.
		 */
		if ((policy_mgr_is_sta_sap_scc(
		    hdd_ctxt->psoc,
		    adapter->session.ap.operating_chan_freq) &&
		    scc_on_lte_coex) ||
		    policy_mgr_nan_sap_scc_on_unsafe_ch_chk(
		    hdd_ctxt->psoc,
		    adapter->session.ap.operating_chan_freq)) {
			hdd_debug("SAP allowed in unsafe SCC channel");
		} else {
			for (i = 0; i < hdd_ctxt->unsafe_channel_count; i++) {
				if (ap_chan_freq ==
				    hdd_ctxt->unsafe_channel_list[i]) {
					found = true;
					hdd_debug("op ch freq:%d is unsafe",
						  ap_chan_freq);
					break;
				}
			}
		}
		if (!found) {
			hdd_store_sap_restart_channel(
				ap_chan_freq,
				restart_chan_store);
			hdd_debug("ch freq:%d is safe. no need to change channel",
				  ap_chan_freq);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		status = ucfg_mlme_get_acs_support_for_dfs_ltecoex(
					hdd_ctxt->psoc,
					&is_acs_support_for_dfs_ltecoex);
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("get_acs_support_for_dfs_ltecoex failed,set def");

		status = ucfg_mlme_get_vendor_acs_support(
					hdd_ctxt->psoc,
					&is_vendor_acs_support);
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("get_vendor_acs_support failed, set default");

		if (is_vendor_acs_support && is_acs_support_for_dfs_ltecoex) {
			hdd_update_acs_timer_reason(adapter,
				QCA_WLAN_VENDOR_ACS_SELECT_REASON_LTE_COEX);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		restart_freq = 0;
		for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
			if (!restart_chan_store[i])
				continue;

			if (policy_mgr_is_force_scc(hdd_ctxt->psoc) &&
			    WLAN_REG_IS_SAME_BAND_FREQS(
					restart_chan_store[i],
					ap_chan_freq)) {
				restart_freq = restart_chan_store[i];
				break;
			}
		}
		if (!restart_freq) {
			restart_freq =
				wlansap_get_safe_channel_from_pcl_and_acs_range(
					WLAN_HDD_GET_SAP_CTX_PTR(adapter));
		}
		if (!restart_freq) {
			wlan_hdd_set_sap_csa_reason(hdd_ctxt->psoc,
						    adapter->vdev_id,
						    CSA_REASON_UNSAFE_CHANNEL);
			hdd_err("Unable to find safe chan, Stop the SAP if restriction mask is set else set txpower");
			hdd_stop_sap_set_tx_power(hdd_ctxt->psoc, adapter);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}
		/*
		 * SAP restart due to unsafe channel. While
		 * restarting the SAP, make sure to clear
		 * acs_channel, channel to reset to
		 * 0. Otherwise these settings will override
		 * the ACS while restart.
		 */
		hdd_ctxt->acs_policy.acs_chan_freq =
					AUTO_CHANNEL_SELECT;
		ucfg_mlme_get_sap_internal_restart(hdd_ctxt->psoc,
						   &value);
		if (value) {
			wlan_hdd_set_sap_csa_reason(hdd_ctxt->psoc,
						    adapter->vdev_id,
						    CSA_REASON_UNSAFE_CHANNEL);
			status = hdd_switch_sap_chan_freq(adapter,
							  restart_freq,
							  true);
			if (QDF_IS_STATUS_SUCCESS(status)) {
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return;
			} else {
				hdd_debug("CSA failed, check next SAP");
			}
		} else {
			hdd_debug("sending coex indication");
			wlan_hdd_send_svc_nlink_msg(
					hdd_ctxt->radio_index,
					WLAN_SVC_LTE_COEX_IND, NULL, 0);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return;
		}
		/* dev_put has to be done here */
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

/**
 * hdd_init_channel_avoidance() - Initialize channel avoidance
 * @hdd_ctx:	HDD global context
 *
 * Initialize the channel avoidance logic by retrieving the unsafe
 * channel list from the platform driver and plumbing the data
 * down to the lower layers.  Then subscribe to subsequent channel
 * avoidance events.
 *
 * Return: None
 */
static void hdd_init_channel_avoidance(struct hdd_context *hdd_ctx)
{
	uint16_t unsafe_channel_count;
	int index;
	qdf_freq_t *unsafe_freq_list;

	pld_get_wlan_unsafe_channel(hdd_ctx->parent_dev,
				    hdd_ctx->unsafe_channel_list,
				     &(hdd_ctx->unsafe_channel_count),
				     sizeof(uint16_t) * NUM_CHANNELS);

	hdd_debug("num of unsafe channels is %d",
	       hdd_ctx->unsafe_channel_count);

	unsafe_channel_count = QDF_MIN((uint16_t)hdd_ctx->unsafe_channel_count,
				       (uint16_t)NUM_CHANNELS);

	if (!unsafe_channel_count)
		return;

	unsafe_freq_list = qdf_mem_malloc(
			unsafe_channel_count * sizeof(*unsafe_freq_list));

	if (!unsafe_freq_list)
		return;

	for (index = 0; index < unsafe_channel_count; index++) {
		hdd_debug("channel frequency %d is not safe",
			  hdd_ctx->unsafe_channel_list[index]);
		unsafe_freq_list[index] =
			(qdf_freq_t)hdd_ctx->unsafe_channel_list[index];
	}

	ucfg_policy_mgr_init_chan_avoidance(
		hdd_ctx->psoc,
		unsafe_freq_list,
		unsafe_channel_count);

	qdf_mem_free(unsafe_freq_list);
}

static void hdd_lte_coex_restart_sap(struct hdd_adapter *adapter,
				     struct hdd_context *hdd_ctx)
{
	uint8_t restart_chan;
	uint32_t restart_freq;

	restart_freq = wlansap_get_safe_channel_from_pcl_and_acs_range(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter));

	restart_chan = wlan_reg_freq_to_chan(hdd_ctx->pdev,
					     restart_freq);

	if (!restart_chan) {
		hdd_alert("fail to restart SAP");
		return;
	}

	/* SAP restart due to unsafe channel. While restarting
	 * the SAP, make sure to clear acs_channel, channel to
	 * reset to 0. Otherwise these settings will override
	 * the ACS while restart.
	 */
	hdd_ctx->acs_policy.acs_chan_freq = AUTO_CHANNEL_SELECT;

	hdd_debug("sending coex indication");

	wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
				    WLAN_SVC_LTE_COEX_IND, NULL, 0);
	wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc, adapter->vdev_id,
				    CSA_REASON_LTE_COEX);
	hdd_switch_sap_channel(adapter, restart_chan, true);
}

int hdd_clone_local_unsafe_chan(struct hdd_context *hdd_ctx,
	uint16_t **local_unsafe_list, uint16_t *local_unsafe_list_count)
{
	uint32_t size;
	uint16_t *unsafe_list;
	uint16_t chan_count;

	if (!hdd_ctx || !local_unsafe_list_count || !local_unsafe_list_count)
		return -EINVAL;

	chan_count = QDF_MIN(hdd_ctx->unsafe_channel_count,
			     NUM_CHANNELS);
	if (chan_count) {
		size = chan_count * sizeof(hdd_ctx->unsafe_channel_list[0]);
		unsafe_list = qdf_mem_malloc(size);
		if (!unsafe_list)
			return -ENOMEM;
		qdf_mem_copy(unsafe_list, hdd_ctx->unsafe_channel_list, size);
	} else {
		unsafe_list = NULL;
	}

	*local_unsafe_list = unsafe_list;
	*local_unsafe_list_count = chan_count;

	return 0;
}

bool hdd_local_unsafe_channel_updated(
	struct hdd_context *hdd_ctx, uint16_t *local_unsafe_list,
	uint16_t local_unsafe_list_count, uint32_t restriction_mask)
{
	int i, j;

	if (local_unsafe_list_count != hdd_ctx->unsafe_channel_count)
		return true;
	if (local_unsafe_list_count == 0)
		return false;
	for (i = 0; i < local_unsafe_list_count; i++) {
		for (j = 0; j < local_unsafe_list_count; j++)
			if (local_unsafe_list[i] ==
			    hdd_ctx->unsafe_channel_list[j])
				break;
		if (j >= local_unsafe_list_count)
			break;
	}

	if (ucfg_mlme_get_coex_unsafe_chan_nb_user_prefer(hdd_ctx->psoc)) {
		/* Return false if current channel list is same as previous
		 * and restriction mask is not altered
		 */
		if (i >= local_unsafe_list_count &&
		    (restriction_mask ==
		     wlan_hdd_get_restriction_mask(hdd_ctx))) {
			hdd_info("unsafe chan list same");
			return false;
		}
	} else if (i >= local_unsafe_list_count) {
		hdd_info("unsafe chan list same");
		return false;
	}

	return true;
}
#else
static void hdd_init_channel_avoidance(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_lte_coex_restart_sap(struct hdd_adapter *adapter,
					    struct hdd_context *hdd_ctx)
{
	hdd_debug("Channel avoidance is not enabled; Abort SAP restart");
}
#endif /* defined(FEATURE_WLAN_CH_AVOID) */

struct hdd_adapter *
wlan_hdd_get_adapter_from_objmgr(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev) {
		hdd_err("null vdev object");
		return NULL;
	}

	if (vdev->vdev_nif.osdev)
		return vdev->vdev_nif.osdev->legacy_osif_priv;

	return NULL;
}

/**
 * hdd_indicate_mgmt_frame() - Wrapper to indicate management frame to
 * user space
 * @frame_ind: Management frame data to be informed.
 *
 * This function is used to indicate management frame to
 * user space
 *
 * Return: None
 *
 */
void hdd_indicate_mgmt_frame(tSirSmeMgmtFrameInd *frame_ind)
{
	struct hdd_context *hdd_ctx = NULL;
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	int i, num_adapters;
	uint8_t vdev_id[WLAN_MAX_VDEVS + WLAN_MAX_ML_VDEVS];
	struct ieee80211_mgmt *mgmt =
		(struct ieee80211_mgmt *)frame_ind->frameBuf;
	struct wlan_objmgr_vdev *vdev;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_INDICATE_MGMT_FRAME;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (frame_ind->frame_len < ieee80211_hdrlen(mgmt->frame_control)) {
		hdd_err(" Invalid frame length");
		return;
	}

	if (SME_SESSION_ID_ANY == frame_ind->sessionId) {
		for (i = 0; i < WLAN_MAX_VDEVS; i++) {
			adapter =
				hdd_get_adapter_by_vdev(hdd_ctx, i);
			if (adapter)
				break;
		}
	} else if (SME_SESSION_ID_BROADCAST == frame_ind->sessionId) {
		num_adapters = 0;
		hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter,
						   next_adapter, dbgid) {
			vdev_id[num_adapters] = adapter->vdev_id;
			num_adapters++;
			/* dev_put has to be done here */
			hdd_adapter_dev_put_debug(adapter, dbgid);
		}

		adapter = NULL;

		for (i = 0; i < num_adapters; i++) {
			vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
							hdd_ctx->psoc,
							vdev_id[i],
							WLAN_OSIF_ID);

			if (!vdev)
				continue;

			adapter = wlan_hdd_get_adapter_from_objmgr(vdev);

			if (!adapter) {
				wlan_objmgr_vdev_release_ref(vdev,
							     WLAN_OSIF_ID);
				continue;
			}

			hdd_indicate_mgmt_frame_to_user(adapter,
							frame_ind->frame_len,
							frame_ind->frameBuf,
							frame_ind->frameType,
							frame_ind->rx_freq,
							frame_ind->rxRssi,
							frame_ind->rx_flags);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_ID);
		}

		adapter = NULL;
	} else {
		adapter = hdd_get_adapter_by_vdev(hdd_ctx,
						  frame_ind->sessionId);
	}

	if ((adapter) &&
		(WLAN_HDD_ADAPTER_MAGIC == adapter->magic))
		hdd_indicate_mgmt_frame_to_user(adapter,
						frame_ind->frame_len,
						frame_ind->frameBuf,
						frame_ind->frameType,
						frame_ind->rx_freq,
						frame_ind->rxRssi,
						frame_ind->rx_flags);
}

void hdd_acs_response_timeout_handler(void *context)
{
	struct hdd_external_acs_timer_context *timer_context =
			(struct hdd_external_acs_timer_context *)context;
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	uint8_t reason;

	hdd_enter();
	if (!timer_context) {
		hdd_err("invalid timer context");
		return;
	}
	adapter = timer_context->adapter;
	reason = timer_context->reason;


	if ((!adapter) ||
	    (adapter->magic != WLAN_HDD_ADAPTER_MAGIC)) {
		hdd_err("invalid adapter or adapter has invalid magic");
		return;
	}
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (test_bit(VENDOR_ACS_RESPONSE_PENDING, &adapter->event_flags))
		clear_bit(VENDOR_ACS_RESPONSE_PENDING, &adapter->event_flags);
	else
		return;

	hdd_err("ACS timeout happened for %s reason %d",
				adapter->dev->name, reason);

	switch (reason) {
	/* SAP init case */
	case QCA_WLAN_VENDOR_ACS_SELECT_REASON_INIT:
		wlan_sap_set_vendor_acs(WLAN_HDD_GET_SAP_CTX_PTR(adapter),
					false);
		wlan_hdd_cfg80211_start_acs(adapter);
		break;
	/* DFS detected on current channel */
	case QCA_WLAN_VENDOR_ACS_SELECT_REASON_DFS:
		wlan_sap_update_next_channel(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter), 0, 0);
		sme_update_new_channel_event(hdd_ctx->mac_handle,
					     adapter->vdev_id);
		break;
	/* LTE coex event on current channel */
	case QCA_WLAN_VENDOR_ACS_SELECT_REASON_LTE_COEX:
		hdd_lte_coex_restart_sap(adapter, hdd_ctx);
		break;
	default:
		hdd_info("invalid reason for timer invoke");

	}
}

/**
 * hdd_override_ini_config - Override INI config
 * @hdd_ctx: HDD context
 *
 * Override INI config based on module parameter.
 *
 * Return: None
 */
static void hdd_override_ini_config(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	if (0 == enable_dfs_chan_scan || 1 == enable_dfs_chan_scan) {
		ucfg_scan_cfg_set_dfs_chan_scan_allowed(hdd_ctx->psoc,
							enable_dfs_chan_scan);
		hdd_debug("Module enable_dfs_chan_scan set to %d",
			   enable_dfs_chan_scan);
	}
	if (0 == enable_11d || 1 == enable_11d) {
		status = ucfg_mlme_set_11d_enabled(hdd_ctx->psoc, enable_11d);
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("Failed to set 11d_enable flag");
	}
}

#ifdef ENABLE_MTRACE_LOG
static void hdd_set_mtrace_for_each(struct hdd_context *hdd_ctx)
{
	uint8_t module_id = 0;
	int qdf_print_idx = -1;

	qdf_print_idx = qdf_get_pidx();
	for (module_id = 0; module_id < QDF_MODULE_ID_MAX; module_id++)
		qdf_print_set_category_verbose(
					qdf_print_idx,
					module_id, QDF_TRACE_LEVEL_TRACE,
					hdd_ctx->config->enable_mtrace);
}
#else
static void hdd_set_mtrace_for_each(struct hdd_context *hdd_ctx)
{
}

#endif

/**
 * hdd_log_level_to_bitmask() - user space log level to host log bitmask
 * @user_log_level: user space log level
 *
 * Convert log level from user space to host log level bitmask.
 *
 * Return: Bitmask of log levels to be enabled
 */
static uint32_t hdd_log_level_to_bitmask(enum host_log_level user_log_level)
{
	QDF_TRACE_LEVEL host_trace_level;
	uint32_t bitmask;

	switch (user_log_level) {
	case HOST_LOG_LEVEL_NONE:
		host_trace_level = QDF_TRACE_LEVEL_NONE;
		break;
	case HOST_LOG_LEVEL_FATAL:
		host_trace_level = QDF_TRACE_LEVEL_FATAL;
		break;
	case HOST_LOG_LEVEL_ERROR:
		host_trace_level = QDF_TRACE_LEVEL_ERROR;
		break;
	case HOST_LOG_LEVEL_WARN:
		host_trace_level = QDF_TRACE_LEVEL_WARN;
		break;
	case HOST_LOG_LEVEL_INFO:
		host_trace_level = QDF_TRACE_LEVEL_INFO_LOW;
		break;
	case HOST_LOG_LEVEL_DEBUG:
		host_trace_level = QDF_TRACE_LEVEL_DEBUG;
		break;
	case HOST_LOG_LEVEL_TRACE:
		host_trace_level = QDF_TRACE_LEVEL_TRACE;
		break;
	default:
		host_trace_level = QDF_TRACE_LEVEL_TRACE;
		break;
	}

	bitmask = (1 << (host_trace_level + 1)) - 1;

	return bitmask;
}

/**
 * hdd_set_trace_level_for_each - Set trace level for each INI config
 * @hdd_ctx: HDD context
 *
 * Set trace level for each module based on INI config.
 *
 * Return: None
 */
static void hdd_set_trace_level_for_each(struct hdd_context *hdd_ctx)
{
	uint8_t host_module_log[QDF_MODULE_ID_MAX * 2];
	qdf_size_t host_module_log_num = 0;
	QDF_MODULE_ID module_id;
	uint32_t bitmask;
	uint32_t i;

	qdf_uint8_array_parse(cfg_get(hdd_ctx->psoc,
				      CFG_ENABLE_HOST_MODULE_LOG_LEVEL),
			      host_module_log,
			      QDF_MODULE_ID_MAX * 2,
			      &host_module_log_num);

	for (i = 0; i + 1 < host_module_log_num; i += 2) {
		module_id = host_module_log[i];
		bitmask = hdd_log_level_to_bitmask(host_module_log[i + 1]);
		if (module_id < QDF_MODULE_ID_MAX &&
		    module_id >= QDF_MODULE_ID_MIN)
			hdd_qdf_trace_enable(module_id, bitmask);
	}

	hdd_set_mtrace_for_each(hdd_ctx);
}

/**
 * hdd_context_init() - Initialize HDD context
 * @hdd_ctx:	HDD context.
 *
 * Initialize HDD context along with all the feature specific contexts.
 *
 * return: 0 on success and errno on failure.
 */
static int hdd_context_init(struct hdd_context *hdd_ctx)
{
	int ret;

	hdd_ctx->ioctl_scan_mode = eSIR_ACTIVE_SCAN;
	hdd_ctx->max_intf_count = WLAN_MAX_VDEVS;

	init_completion(&hdd_ctx->mc_sus_event_var);
	init_completion(&hdd_ctx->ready_to_suspend);

	qdf_spinlock_create(&hdd_ctx->connection_status_lock);
	qdf_spinlock_create(&hdd_ctx->hdd_adapter_lock);

	qdf_list_create(&hdd_ctx->hdd_adapters, 0);

	ret = hdd_scan_context_init(hdd_ctx);
	if (ret)
		goto list_destroy;

	ret = hdd_sap_context_init(hdd_ctx);
	if (ret)
		goto scan_destroy;

	ret = ucfg_dp_bbm_context_init(hdd_ctx->psoc);
	if (ret)
		goto sap_destroy;

	wlan_hdd_cfg80211_extscan_init(hdd_ctx);

	hdd_init_offloaded_packets_ctx(hdd_ctx);

	ret = wlan_hdd_cfg80211_init(hdd_ctx->parent_dev, hdd_ctx->wiphy,
				     hdd_ctx->config);
	if (ret)
		goto bbm_destroy;

	qdf_wake_lock_create(&hdd_ctx->monitor_mode_wakelock,
			     "monitor_mode_wakelock");

	return 0;

bbm_destroy:
	ucfg_dp_bbm_context_deinit(hdd_ctx->psoc);

sap_destroy:
	hdd_sap_context_destroy(hdd_ctx);

scan_destroy:
	hdd_scan_context_destroy(hdd_ctx);
list_destroy:
	qdf_list_destroy(&hdd_ctx->hdd_adapters);

	return ret;
}

void hdd_psoc_idle_timer_start(struct hdd_context *hdd_ctx)
{
	uint32_t timeout_ms = hdd_ctx->config->iface_change_wait_time;
	uint32_t suspend_timeout_ms;
	enum wake_lock_reason reason =
		WIFI_POWER_EVENT_WAKELOCK_IFACE_CHANGE_TIMER;

	if (!timeout_ms) {
		hdd_info("psoc idle timer is disabled");
		return;
	}

	hdd_debug("Starting psoc idle timer");

	/* If PCIe gen speed change is requested, reduce idle shutdown
	 * timeout to 100 ms
	 */
	if (hdd_ctx->current_pcie_gen_speed) {
		timeout_ms = HDD_PCIE_GEN_SPEED_CHANGE_TIMEOUT_MS;
		hdd_info("pcie gen speed change requested");
	}

	qdf_delayed_work_start(&hdd_ctx->psoc_idle_timeout_work, timeout_ms);
	suspend_timeout_ms = timeout_ms + HDD_PSOC_IDLE_SHUTDOWN_SUSPEND_DELAY;
	hdd_prevent_suspend_timeout(suspend_timeout_ms, reason);
}

void hdd_psoc_idle_timer_stop(struct hdd_context *hdd_ctx)
{
	qdf_delayed_work_stop_sync(&hdd_ctx->psoc_idle_timeout_work);
	hdd_debug("Stopped psoc idle timer");
}


/**
 * __hdd_psoc_idle_shutdown() - perform an idle shutdown on the given psoc
 * @hdd_ctx: the hdd context which should be shutdown
 *
 * When no interfaces are "up" on a psoc, an idle shutdown timer is started.
 * If no interfaces are brought up before the timer expires, we do an
 * "idle shutdown," cutting power to the physical SoC to save power. This is
 * done completely transparently from the perspective of userspace.
 *
 * Return: None
 */
static int __hdd_psoc_idle_shutdown(struct hdd_context *hdd_ctx)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	hdd_enter();

	errno = osif_psoc_sync_trans_start(hdd_ctx->parent_dev, &psoc_sync);
	if (errno) {
		hdd_info("psoc busy, abort idle shutdown; errno:%d", errno);
		errno = -EAGAIN;
		goto exit;
	}

	osif_psoc_sync_wait_for_ops(psoc_sync);
	/*
	 * This is to handle scenario in which platform driver triggers
	 * idle_shutdown if Deep Sleep/Hibernate entry notification is
	 * received from modem subsystem in wearable devices
	 */
	if (hdd_is_any_interface_open(hdd_ctx)) {
		hdd_err_rl("all interfaces are not down, ignore idle shutdown");
		errno = -EAGAIN;
	} else {
		errno = hdd_wlan_stop_modules(hdd_ctx, false);
	}

	osif_psoc_sync_trans_stop(psoc_sync);

exit:
	hdd_exit();
	return errno;
}

static int __hdd_mode_change_psoc_idle_shutdown(struct hdd_context *hdd_ctx)
{
	is_mode_change_psoc_idle_shutdown = false;
	return hdd_wlan_stop_modules(hdd_ctx, true);
}

int hdd_psoc_idle_shutdown(struct device *dev)
{
	int ret;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return -EINVAL;

	if (is_mode_change_psoc_idle_shutdown)
		ret = __hdd_mode_change_psoc_idle_shutdown(hdd_ctx);
	else {
		ret =  __hdd_psoc_idle_shutdown(hdd_ctx);
	}

	return ret;
}

static int __hdd_psoc_idle_restart(struct hdd_context *hdd_ctx)
{
	int ret;

	ret = hdd_soc_idle_restart_lock(hdd_ctx->parent_dev);
	if (ret)
		return ret;

	ret = hdd_wlan_start_modules(hdd_ctx, false);

	hdd_soc_idle_restart_unlock();

	return ret;
}

int hdd_psoc_idle_restart(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return -EINVAL;

	return __hdd_psoc_idle_restart(hdd_ctx);
}

int hdd_trigger_psoc_idle_restart(struct hdd_context *hdd_ctx)
{
	int ret;

	QDF_BUG(rtnl_is_locked());

	hdd_psoc_idle_timer_stop(hdd_ctx);
	if (hdd_ctx->driver_status == DRIVER_MODULES_ENABLED) {
		hdd_nofl_debug("Driver modules already Enabled");
		return 0;
	}

	ret = hdd_soc_idle_restart_lock(hdd_ctx->parent_dev);
	if (ret)
		return ret;

	if (hdd_ctx->current_pcie_gen_speed) {
		hdd_info("request pcie gen speed change to %d",
			 hdd_ctx->current_pcie_gen_speed);

		/* call pld api for pcie gen speed change */
		ret  = pld_set_pcie_gen_speed(hdd_ctx->parent_dev,
					      hdd_ctx->current_pcie_gen_speed);
		if (ret)
			hdd_err_rl("failed to set pcie gen speed");

		hdd_ctx->current_pcie_gen_speed = 0;
	}

	qdf_event_reset(&hdd_ctx->regulatory_update_event);
	qdf_mutex_acquire(&hdd_ctx->regulatory_status_lock);
	hdd_ctx->is_regulatory_update_in_progress = true;
	qdf_mutex_release(&hdd_ctx->regulatory_status_lock);

	ret = pld_idle_restart(hdd_ctx->parent_dev, hdd_psoc_idle_restart);
	hdd_soc_idle_restart_unlock();

	return ret;
}

/**
 * hdd_psoc_idle_timeout_callback() - Handler for psoc idle timeout
 * @priv: pointer to hdd context
 *
 * Return: None
 */
static void hdd_psoc_idle_timeout_callback(void *priv)
{
	int ret;
	struct hdd_context *hdd_ctx = priv;
	void *hif_ctx;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (hif_ctx) {
		/*
		 * Trigger runtime sync resume before psoc_idle_shutdown
		 * such that resume can happen successfully
		 */
		qdf_rtpm_sync_resume();
	}

	hdd_info("Psoc idle timeout elapsed; starting psoc shutdown");

	ret = pld_idle_shutdown(hdd_ctx->parent_dev, hdd_psoc_idle_shutdown);
	if (-EAGAIN == ret || hdd_ctx->is_wiphy_suspended) {
		hdd_debug("System suspend in progress. Restart idle shutdown timer");
		hdd_psoc_idle_timer_start(hdd_ctx);
	}

	/* Clear the recovery flag for PCIe discrete soc after idle shutdown*/
	if (PLD_BUS_TYPE_PCIE == pld_get_bus_type(hdd_ctx->parent_dev) &&
	    -EBUSY != ret)
		cds_set_recovery_in_progress(false);
}

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
static void hdd_set_wlan_logging(struct hdd_context *hdd_ctx)
{
	wlan_set_console_log_levels(hdd_ctx->config->wlan_console_log_levels);
	wlan_logging_set_active(hdd_ctx->config->wlan_logging_enable);
}
#else
static void hdd_set_wlan_logging(struct hdd_context *hdd_ctx)
{ }
#endif

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
static void hdd_init_wlan_logging_params(struct hdd_config *config,
					 struct wlan_objmgr_psoc *psoc)
{
	config->wlan_logging_enable = cfg_get(psoc, CFG_WLAN_LOGGING_SUPPORT);

	config->wlan_console_log_levels =
			cfg_get(psoc, CFG_WLAN_LOGGING_CONSOLE_SUPPORT);
	config->host_log_custom_nl_proto =
		cfg_get(psoc, CFG_HOST_LOG_CUSTOM_NETLINK_PROTO);
}
#else
static void hdd_init_wlan_logging_params(struct hdd_config *config,
					 struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
static void hdd_init_wlan_auto_shutdown(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
	config->wlan_auto_shutdown = cfg_get(psoc, CFG_WLAN_AUTO_SHUTDOWN);
}
#else
static void hdd_init_wlan_auto_shutdown(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifndef REMOVE_PKT_LOG
static void hdd_init_packet_log(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)
{
	config->enable_packet_log = cfg_get(psoc, CFG_ENABLE_PACKET_LOG);
}
#else
static void hdd_init_packet_log(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef ENABLE_MTRACE_LOG
static void hdd_init_mtrace_log(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)
{
	config->enable_mtrace = cfg_get(psoc, CFG_ENABLE_MTRACE);
}
#else
static void hdd_init_mtrace_log(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef FEATURE_RUNTIME_PM
static void hdd_init_runtime_pm(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)
{
	config->runtime_pm = cfg_get(psoc, CFG_ENABLE_RUNTIME_PM);
}

bool hdd_is_runtime_pm_enabled(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->config->runtime_pm != hdd_runtime_pm_disabled;
}
#else
static void hdd_init_runtime_pm(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)

{
}

bool hdd_is_runtime_pm_enabled(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_WMI_SEND_RECV_QMI
static void hdd_init_qmi_stats(struct hdd_config *config,
			       struct wlan_objmgr_psoc *psoc)
{
	config->is_qmi_stats_enabled = cfg_get(psoc, CFG_ENABLE_QMI_STATS);
}
#else
static void hdd_init_qmi_stats(struct hdd_config *config,
			       struct wlan_objmgr_psoc *psoc)

{
}
#endif

#ifdef FEATURE_WLAN_DYNAMIC_CVM
static void hdd_init_vc_mode_cfg_bitmap(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
	config->vc_mode_cfg_bitmap = cfg_get(psoc, CFG_VC_MODE_BITMAP);
}
#else
static void hdd_init_vc_mode_cfg_bitmap(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef DHCP_SERVER_OFFLOAD
static void
hdd_init_dhcp_server_ip(struct hdd_context *hdd_ctx)
{
	uint8_t num_entries;

	hdd_ctx->config->dhcp_server_ip.is_dhcp_server_ip_valid = true;
	hdd_string_to_u8_array(cfg_get(hdd_ctx->psoc, CFG_DHCP_SERVER_IP_NAME),
			       hdd_ctx->config->dhcp_server_ip.dhcp_server_ip,
			       &num_entries, IPADDR_NUM_ENTRIES);

	if (num_entries != IPADDR_NUM_ENTRIES) {
		hdd_err("Incorrect IP address (%s) assigned for DHCP server!",
			cfg_get(hdd_ctx->psoc, CFG_DHCP_SERVER_IP_NAME));
		hdd_config->dhcp_server_ip.is_dhcp_server_ip_valid = false;
	}
}
#else
static void
hdd_init_dhcp_server_ip(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef SAR_SAFETY_FEATURE
static void hdd_sar_cfg_update(struct hdd_config *config,
			       struct wlan_objmgr_psoc *psoc)
{
	config->sar_safety_timeout = cfg_get(psoc, CFG_SAR_SAFETY_TIMEOUT);
	config->sar_safety_unsolicited_timeout =
			cfg_get(psoc, CFG_SAR_SAFETY_UNSOLICITED_TIMEOUT);
	config->sar_safety_req_resp_timeout =
				cfg_get(psoc, CFG_SAR_SAFETY_REQ_RESP_TIMEOUT);
	config->sar_safety_req_resp_retry =
				cfg_get(psoc, CFG_SAR_SAFETY_REQ_RESP_RETRIES);
	config->sar_safety_index = cfg_get(psoc, CFG_SAR_SAFETY_INDEX);
	config->sar_safety_sleep_index =
				cfg_get(psoc, CFG_SAR_SAFETY_SLEEP_INDEX);
	config->enable_sar_safety =
				cfg_get(psoc, CFG_ENABLE_SAR_SAFETY_FEATURE);
	config->config_sar_safety_sleep_index =
			cfg_get(psoc, CFG_CONFIG_SAR_SAFETY_SLEEP_MODE_INDEX);
}
#else
static void hdd_sar_cfg_update(struct hdd_config *config,
			       struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef FEATURE_SET
/**
 * hdd_get_wifi_features_cfg_update() - Initialize get wifi features cfg
 * @config: Pointer to HDD config
 * @psoc: psoc pointer
 *
 * Return: None
 */
static void hdd_get_wifi_features_cfg_update(struct hdd_config *config,
					     struct wlan_objmgr_psoc *psoc)
{
	config->get_wifi_features = cfg_get(psoc, CFG_GET_WIFI_FEATURES);
}
#else
static void hdd_get_wifi_features_cfg_update(struct hdd_config *config,
					     struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef FEATURE_RUNTIME_PM
/**
 * hdd_init_cpu_cxpc_threshold_cfg() - Initialize cpu cxpc threshold cfg
 * @config: Pointer to HDD config
 * @psoc: psoc pointer
 *
 * Return: None
 */
static void hdd_init_cpu_cxpc_threshold_cfg(struct hdd_config *config,
					    struct wlan_objmgr_psoc *psoc)
{
	config->cpu_cxpc_threshold = cfg_get(psoc, CFG_CPU_CXPC_THRESHOLD);
}
#else
static void hdd_init_cpu_cxpc_threshold_cfg(struct hdd_config *config,
					    struct wlan_objmgr_psoc *psoc)
{
}
#endif

/**
 * hdd_cfg_params_init() - Initialize hdd params in hdd_config structure
 * @hdd_ctx: Pointer to HDD context
 *
 * Return: None
 */
static void hdd_cfg_params_init(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	struct hdd_config *config = hdd_ctx->config;
	if (!psoc) {
		hdd_err("Invalid psoc");
		return;
	}

	if (!config) {
		hdd_err("Invalid hdd config");
		return;
	}

	config->dot11Mode = cfg_get(psoc, CFG_HDD_DOT11_MODE);
	config->bug_on_reinit_failure = cfg_get(psoc,
						CFG_BUG_ON_REINIT_FAILURE);

	config->is_ramdump_enabled = cfg_get(psoc,
					     CFG_ENABLE_RAMDUMP_COLLECTION);

	config->iface_change_wait_time = cfg_get(psoc,
						 CFG_INTERFACE_CHANGE_WAIT);

	config->multicast_host_fw_msgs = cfg_get(psoc,
						 CFG_MULTICAST_HOST_FW_MSGS);

	config->private_wext_control = cfg_get(psoc, CFG_PRIVATE_WEXT_CONTROL);
	config->enablefwprint = cfg_get(psoc, CFG_ENABLE_FW_UART_PRINT);
	config->enable_fw_log = cfg_get(psoc, CFG_ENABLE_FW_LOG);
	config->operating_chan_freq = cfg_get(psoc, CFG_OPERATING_FREQUENCY);
	config->num_vdevs = cfg_get(psoc, CFG_NUM_VDEV_ENABLE);
	qdf_str_lcopy(config->enable_concurrent_sta,
		      cfg_get(psoc, CFG_ENABLE_CONCURRENT_STA),
		      CFG_CONCURRENT_IFACE_MAX_LEN);
	qdf_str_lcopy(config->dbs_scan_selection,
		      cfg_get(psoc, CFG_DBS_SCAN_SELECTION),
		      CFG_DBS_SCAN_PARAM_LENGTH);
	config->inform_bss_rssi_raw = cfg_get(psoc, CFG_INFORM_BSS_RSSI_RAW);
	config->mac_provision = cfg_get(psoc, CFG_ENABLE_MAC_PROVISION);
	config->provisioned_intf_pool =
				cfg_get(psoc, CFG_PROVISION_INTERFACE_POOL);
	config->derived_intf_pool = cfg_get(psoc, CFG_DERIVED_INTERFACE_POOL);
	config->advertise_concurrent_operation =
				cfg_get(psoc,
					CFG_ADVERTISE_CONCURRENT_OPERATION);
	config->is_unit_test_framework_enabled =
			cfg_get(psoc, CFG_ENABLE_UNIT_TEST_FRAMEWORK);
	config->disable_channel = cfg_get(psoc, CFG_ENABLE_DISABLE_CHANNEL);
	config->enable_sar_conversion = cfg_get(psoc, CFG_SAR_CONVERSION);
	config->nb_commands_interval =
				cfg_get(psoc, CFG_NB_COMMANDS_RATE_LIMIT);

	hdd_init_vc_mode_cfg_bitmap(config, psoc);
	hdd_init_runtime_pm(config, psoc);
	hdd_init_wlan_auto_shutdown(config, psoc);
	hdd_init_wlan_logging_params(config, psoc);
	hdd_init_packet_log(config, psoc);
	hdd_init_mtrace_log(config, psoc);
	hdd_init_dhcp_server_ip(hdd_ctx);
	hdd_dp_cfg_update(psoc, hdd_ctx);
	hdd_sar_cfg_update(config, psoc);
	hdd_init_qmi_stats(config, psoc);
	hdd_club_ll_stats_in_get_sta_cfg_update(config, psoc);
	config->read_mac_addr_from_mac_file =
			cfg_get(psoc, CFG_READ_MAC_ADDR_FROM_MAC_FILE);

	hdd_get_wifi_features_cfg_update(config, psoc);
	hdd_init_cpu_cxpc_threshold_cfg(config, psoc);
}

#ifdef CONNECTION_ROAMING_CFG
static QDF_STATUS hdd_cfg_parse_connection_roaming_cfg(void)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	bool is_valid;

	is_valid = cfg_valid_ini_check(WLAN_CONNECTION_ROAMING_INI_FILE);

	if (is_valid)
		status = cfg_parse(WLAN_CONNECTION_ROAMING_INI_FILE);
	if (QDF_IS_STATUS_ERROR(status)) {
		is_valid = cfg_valid_ini_check(WLAN_CONNECTION_ROAMING_BACKUP_INI_FILE);
		if (is_valid)
			status = cfg_parse(WLAN_CONNECTION_ROAMING_BACKUP_INI_FILE);
	}
	return status;
}
#else
static inline QDF_STATUS hdd_cfg_parse_connection_roaming_cfg(void)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

struct hdd_context *hdd_context_create(struct device *dev)
{
	QDF_STATUS status;
	int ret = 0;
	struct hdd_context *hdd_ctx;

	hdd_enter();

	hdd_ctx = hdd_cfg80211_wiphy_alloc();
	if (!hdd_ctx) {
		ret = -ENOMEM;
		goto err_out;
	}

	status = qdf_delayed_work_create(&hdd_ctx->psoc_idle_timeout_work,
					 hdd_psoc_idle_timeout_callback,
					 hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		ret = qdf_status_to_os_return(status);
		goto wiphy_dealloc;
	}

	hdd_pm_notifier_init(hdd_ctx);

	hdd_ctx->parent_dev = dev;
	hdd_ctx->last_scan_reject_vdev_id = WLAN_UMAC_VDEV_ID_MAX;

	hdd_ctx->config = qdf_mem_malloc(sizeof(struct hdd_config));
	if (!hdd_ctx->config) {
		ret = -ENOMEM;
		goto err_free_hdd_context;
	}

	status = cfg_parse(WLAN_INI_FILE);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to parse cfg %s; status:%d\n",
			WLAN_INI_FILE, status);
		/* Assert if failed to parse at least one INI parameter */
		QDF_BUG(status != QDF_STATUS_E_INVAL);
		ret = qdf_status_to_os_return(status);
		goto err_free_config;
	}

	status = hdd_cfg_parse_connection_roaming_cfg();

	ret = hdd_objmgr_create_and_store_psoc(hdd_ctx, DEFAULT_PSOC_ID);
	if (ret) {
		QDF_DEBUG_PANIC("Psoc creation fails!");
		goto err_release_store;
	}

	if (QDF_IS_STATUS_SUCCESS(status))
		ucfg_mlme_set_connection_roaming_ini_present(hdd_ctx->psoc,
							     true);

	hdd_cfg_params_init(hdd_ctx);

	/* apply multiplier config, if not already set via module parameter */
	if (qdf_timer_get_multiplier() == 1)
		qdf_timer_set_multiplier(cfg_get(hdd_ctx->psoc,
						 CFG_TIMER_MULTIPLIER));
	hdd_debug("set timer multiplier: %u", qdf_timer_get_multiplier());

	cds_set_fatal_event(cfg_get(hdd_ctx->psoc,
				    CFG_ENABLE_FATAL_EVENT_TRIGGER));

	hdd_override_ini_config(hdd_ctx);

	ret = hdd_context_init(hdd_ctx);

	if (ret)
		goto err_hdd_objmgr_destroy;

	if (hdd_get_conparam() == QDF_GLOBAL_EPPING_MODE)
		goto skip_multicast_logging;

	cds_set_multicast_logging(hdd_ctx->config->multicast_host_fw_msgs);
	ret = hdd_init_netlink_services(hdd_ctx);
	if (ret)
		goto err_deinit_hdd_context;

	hdd_set_wlan_logging(hdd_ctx);
	qdf_atomic_init(&hdd_ctx->adapter_ops_history.index);

skip_multicast_logging:
	hdd_set_trace_level_for_each(hdd_ctx);

	cds_set_context(QDF_MODULE_ID_HDD, hdd_ctx);

	wlan_hdd_sar_timers_init(hdd_ctx);

	hdd_exit();

	return hdd_ctx;

err_deinit_hdd_context:
	hdd_context_deinit(hdd_ctx);

err_hdd_objmgr_destroy:
	hdd_objmgr_release_and_destroy_psoc(hdd_ctx);

err_release_store:
	cfg_release();

err_free_config:
	qdf_mem_free(hdd_ctx->config);

err_free_hdd_context:
	qdf_delayed_work_destroy(&hdd_ctx->psoc_idle_timeout_work);

wiphy_dealloc:
	wiphy_free(hdd_ctx->wiphy);

err_out:
	return ERR_PTR(ret);
}

#ifdef MULTI_CLIENT_LL_SUPPORT
/**
 * wlan_hdd_init_multi_client_info_table()- Initialize the multi client info
 * table
 * @adapter: hdd adapter
 *
 * Return: none
 */
static void
wlan_hdd_init_multi_client_info_table(struct hdd_adapter *adapter)
{
	uint8_t i;

	for (i = 0; i < WLM_MAX_HOST_CLIENT; i++) {
		adapter->client_info[i].client_id = i;
		adapter->client_info[i].port_id = 0;
		adapter->client_info[i].in_use = false;
	}
}

void wlan_hdd_deinit_multi_client_info_table(struct hdd_adapter *adapter)
{
	uint8_t i;

	hdd_debug("deinitializing the client info table");
	/* de-initialize the table for host driver client */
	for (i = 0; i < WLM_MAX_HOST_CLIENT; i++) {
		if (adapter->client_info[i].in_use) {
			adapter->client_info[i].port_id = 0;
			adapter->client_info[i].client_id = i;
			adapter->client_info[i].in_use = false;
		}
	}
}

/**
 * wlan_hdd_get_host_driver_port_id()- get host driver port id
 * @port_id: argument to be filled
 *
 * Return: none
 */
static void wlan_hdd_get_host_driver_port_id(uint32_t *port_id)
{
	*port_id = WLAM_WLM_HOST_DRIVER_PORT_ID;
}

#else
static inline void
wlan_hdd_init_multi_client_info_table(struct hdd_adapter *adapter)
{
}

static inline void wlan_hdd_get_host_driver_port_id(uint32_t *port_id)
{
}
#endif

static void
hdd_adapter_set_wlm_client_latency_level(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	bool reset;
	uint32_t port_id;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (!hdd_ctx)
		return;

	status = ucfg_mlme_cfg_get_wlm_reset(hdd_ctx->psoc, &reset);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get the wlm reset flag");
		reset = false;
	}

	if (reset)
		goto out;

	if (hdd_get_multi_client_ll_support(adapter)) {
		wlan_hdd_get_host_driver_port_id(&port_id);
		status = wlan_hdd_set_wlm_client_latency_level(
						adapter, port_id,
						adapter->latency_level);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_warn("Fail to set latency level:%u", status);
	} else {
		status = sme_set_wlm_latency_level(hdd_ctx->mac_handle,
						   adapter->vdev_id,
						   adapter->latency_level,
						   0, false);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_warn("set wlm mode failed, %u", status);
	}
out:
	hdd_debug("wlm initial mode %u", adapter->latency_level);
}

/**
 * hdd_start_station_adapter()- Start the Station Adapter
 * @adapter: HDD adapter
 *
 * This function initializes the adapter for the station mode.
 *
 * Return: 0 on success or errno on failure.
 */
int hdd_start_station_adapter(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	int ret;

	hdd_enter_dev(adapter->dev);
	if (test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		hdd_err("session is already opened, %d",
			adapter->vdev_id);
		return qdf_status_to_os_return(QDF_STATUS_SUCCESS);
	}

	ret = hdd_vdev_create(adapter);
	if (ret) {
		hdd_err("failed to create vdev: %d", ret);
		return ret;
	}
	status = hdd_init_station_mode(adapter);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Error Initializing station mode: %d", status);
		return qdf_status_to_os_return(status);
	}

	hdd_register_tx_flow_control(adapter,
		hdd_tx_resume_timer_expired_handler,
		hdd_tx_resume_cb,
		hdd_tx_flow_control_is_pause);

	hdd_register_hl_netdev_fc_timer(adapter,
					hdd_tx_resume_timer_expired_handler);

	if (hdd_get_multi_client_ll_support(adapter))
		wlan_hdd_init_multi_client_info_table(adapter);

	hdd_adapter_set_wlm_client_latency_level(adapter);

	hdd_exit();

	return 0;
}

#define MAX_VDEV_AP_RTT_PARAMS 2
/**
 * hdd_start_ap_adapter()- Start AP Adapter
 * @adapter: HDD adapter
 *
 * This function initializes the adapter for the AP mode.
 *
 * Return: 0 on success errno on failure.
 */
int hdd_start_ap_adapter(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	bool is_ssr = false;
	int ret;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	uint32_t fine_time_meas_cap = 0;
	struct dev_set_param setparam[MAX_VDEV_AP_RTT_PARAMS] = {};
	uint8_t index = 0;

	hdd_enter();

	if (test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		hdd_err("session is already opened, %d",
			adapter->vdev_id);
		return qdf_status_to_os_return(QDF_STATUS_SUCCESS);
	}
	/*
	 * In SSR case no need to create new sap context.
	 * Otherwise create sap context first and then create
	 * vdev as while creating the vdev, driver needs to
	 * register SAP callback and that callback uses sap context
	 */
	if (adapter->session.ap.sap_context) {
		is_ssr = true;
	} else if (!hdd_sap_create_ctx(adapter)) {
		hdd_err("sap creation failed");
		return qdf_status_to_os_return(QDF_STATUS_E_FAILURE);
	}

	ret = hdd_vdev_create(adapter);
	if (ret) {
		hdd_err("failed to create vdev, status:%d", ret);
		goto sap_destroy_ctx;
	}

	status = sap_acquire_vdev_ref(hdd_ctx->psoc,
				      WLAN_HDD_GET_SAP_CTX_PTR(adapter),
				      adapter->vdev_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Failed to get vdev ref for sap for session_id: %u",
			adapter->vdev_id);
		ret = qdf_status_to_os_return(status);
		goto sap_vdev_destroy;
	}

	if (adapter->device_mode == QDF_SAP_MODE) {
		ucfg_mlme_get_fine_time_meas_cap(hdd_ctx->psoc,
						 &fine_time_meas_cap);
		ret = mlme_check_index_setparam(
			setparam,
			wmi_vdev_param_enable_disable_rtt_responder_role,
			(fine_time_meas_cap & WMI_FW_STA_RTT_RESPR), index++,
			MAX_VDEV_AP_RTT_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret)) {
			hdd_err("failed at wmi_vdev_param_enable_disable_rtt_responder_role");
			goto sap_release_ref;
		}
		ret = mlme_check_index_setparam(
			setparam,
			wmi_vdev_param_enable_disable_rtt_initiator_role,
			(fine_time_meas_cap & WMI_FW_STA_RTT_INITR), index++,
			MAX_VDEV_AP_RTT_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret)) {
			hdd_err("failed at wmi_vdev_param_enable_disable_rtt_initiator_role");
			goto sap_release_ref;
		}
		ret = sme_send_multi_pdev_vdev_set_params(MLME_VDEV_SETPARAM,
							  adapter->vdev_id,
							  setparam,
							  index);
		if (QDF_IS_STATUS_ERROR(ret))
			hdd_err("failed to send vdev RTT set params");
	}

	status = hdd_init_ap_mode(adapter, is_ssr);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Error Initializing the AP mode: %d", status);
		ret = qdf_status_to_os_return(status);
		goto sap_release_ref;
	}

	hdd_register_tx_flow_control(adapter,
		hdd_softap_tx_resume_timer_expired_handler,
		hdd_softap_tx_resume_cb,
		hdd_tx_flow_control_is_pause);

	hdd_register_hl_netdev_fc_timer(adapter,
					hdd_tx_resume_timer_expired_handler);

	hdd_exit();
	return 0;

sap_release_ref:
	sap_release_vdev_ref(WLAN_HDD_GET_SAP_CTX_PTR(adapter));
sap_vdev_destroy:
	hdd_vdev_destroy(adapter);
sap_destroy_ctx:
	hdd_sap_destroy_ctx(adapter);
	return ret;
}

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/**
 * hdd_txrx_populate_cds_config() - Populate txrx cds configuration
 * @cds_cfg: CDS Configuration
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static inline void hdd_txrx_populate_cds_config(struct cds_config_info
						*cds_cfg,
						struct hdd_context *hdd_ctx)
{
	cds_cfg->tx_flow_stop_queue_th =
		cfg_get(hdd_ctx->psoc, CFG_DP_TX_FLOW_STOP_QUEUE_TH);
	cds_cfg->tx_flow_start_queue_offset =
		cfg_get(hdd_ctx->psoc, CFG_DP_TX_FLOW_START_QUEUE_OFFSET);
	/* configuration for DP RX Threads */
	cds_cfg->enable_dp_rx_threads =
		ucfg_dp_is_rx_threads_enabled(hdd_ctx->psoc);
}
#else
static inline void hdd_txrx_populate_cds_config(struct cds_config_info
						*cds_cfg,
						struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef FEATURE_SET
#ifdef WLAN_FEATURE_11BE
/**
 * hdd_is_cfg_dot11_mode_11be() - Check if dot11 mode is 11 be
 * @dot11_mode: Input dot11_mode which needs to be checked
 *
 * Return: True, ifinput dot11_mode is 11be dot11 mode else return false
 */
static bool hdd_is_cfg_dot11_mode_11be(enum hdd_dot11_mode dot11_mode)
{
	return (dot11_mode == eHDD_DOT11_MODE_11be ||
		dot11_mode == eHDD_DOT11_MODE_11be_ONLY);
}

/**
 * hdd_is_11be_supported() - Check if 11be is supported or not
 *
 * Return: True, if 11be is supported else return false
 */
static bool hdd_is_11be_supported(void)
{
	return true;
}
#else

static bool hdd_is_cfg_dot11_mode_11be(enum hdd_dot11_mode dot11_mode)
{
	return false;
}

static bool hdd_is_11be_supported(void)
{
	return false;
}
#endif

/**
 * hdd_get_wifi_standard() - Get wifi standard
 * @hdd_ctx: hdd context pointer
 * @band_capability: band capability bitmap
 *
 * Return: WMI_HOST_WIFI_STANDARD
 */
static WMI_HOST_WIFI_STANDARD hdd_get_wifi_standard(struct hdd_context *hdd_ctx,
						    uint32_t band_capability)
{
	WMI_HOST_WIFI_STANDARD wifi_standard = WMI_HOST_WIFI_STANDARD_4;

	if (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_AUTO) {
		if (hdd_is_11be_supported())
			wifi_standard = WMI_HOST_WIFI_STANDARD_7;
		else if (band_capability & BIT(REG_BAND_6G))
			wifi_standard = WMI_HOST_WIFI_STANDARD_6E;
		else
			wifi_standard = WMI_HOST_WIFI_STANDARD_6;
	} else if (hdd_is_cfg_dot11_mode_11be(hdd_ctx->config->dot11Mode)) {
		wifi_standard = WMI_HOST_WIFI_STANDARD_7;
	} else if (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11ax ||
		   (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11ax_ONLY)) {
		if (band_capability & BIT(REG_BAND_6G))
			wifi_standard = WMI_HOST_WIFI_STANDARD_6E;
		else
			wifi_standard = WMI_HOST_WIFI_STANDARD_6;
	} else if ((hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11ac) ||
		   (hdd_ctx->config->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY)) {
		wifi_standard = WMI_HOST_WIFI_STANDARD_5;
	}

	return wifi_standard;
}

/**
 * hdd_populate_feature_set_cds_config() - Populate cds feature set config
 * @cds_cfg: cds config where feature set info needs to be updated
 * @hdd_ctx: hdd context pointer
 *
 * Return: None
 */
static void hdd_populate_feature_set_cds_config(struct cds_config_info *cds_cfg,
						struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc;
	uint32_t band_capability;
	QDF_STATUS status;

	if (!hdd_ctx)
		return;

	psoc = hdd_ctx->psoc;

	cds_cfg->get_wifi_features = hdd_ctx->config->get_wifi_features;

	status = ucfg_mlme_get_band_capability(hdd_ctx->psoc, &band_capability);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get MLME band capability");

	band_capability =
		hdd_update_band_cap_from_dot11mode(hdd_ctx, band_capability);

	cds_cfg->cds_feature_set.wifi_standard =
				hdd_get_wifi_standard(hdd_ctx,
						      band_capability);

	cds_cfg->cds_feature_set.sap_5g_supported =
					band_capability & BIT(REG_BAND_5G);

	cds_cfg->cds_feature_set.sap_6g_supported =
					band_capability & BIT(REG_BAND_6G);
	cds_cfg->cds_feature_set.band_capability = band_capability;
}
#else
static inline void
hdd_populate_feature_set_cds_config(struct cds_config_info *cds_cfg,
				    struct hdd_context *hdd_ctx)
{
}
#endif

/**
 * hdd_update_cds_config() - API to update cds configuration parameters
 * @hdd_ctx: HDD Context
 *
 * Return: 0 for Success, errno on failure
 */
static int hdd_update_cds_config(struct hdd_context *hdd_ctx)
{
	struct cds_config_info *cds_cfg;
	int value;
	uint8_t band_capability;
	uint32_t band_bitmap;
	uint8_t ito_repeat_count;
	bool crash_inject;
	bool self_recovery;
	bool fw_timeout_crash;
	QDF_STATUS status;

	cds_cfg = qdf_mem_malloc(sizeof(*cds_cfg));
	if (!cds_cfg)
		return -ENOMEM;

	cds_cfg->driver_type = QDF_DRIVER_TYPE_PRODUCTION;
	ucfg_mlme_get_sap_max_modulated_dtim(hdd_ctx->psoc,
					     &cds_cfg->sta_maxlimod_dtim);

	ucfg_mlme_get_max_modulated_dtim_ms(hdd_ctx->psoc,
					    &cds_cfg->sta_maxlimod_dtim_ms);

	status = ucfg_mlme_get_crash_inject(hdd_ctx->psoc, &crash_inject);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get crash inject ini config");
		goto exit;
	}

	status = ucfg_mlme_get_self_recovery(hdd_ctx->psoc, &self_recovery);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get self recovery ini config");
		goto exit;
	}

	status = ucfg_mlme_get_fw_timeout_crash(hdd_ctx->psoc,
						&fw_timeout_crash);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get fw timeout crash ini config");
		goto exit;
	}

	status = ucfg_mlme_get_ito_repeat_count(hdd_ctx->psoc,
						&ito_repeat_count);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get ITO repeat count ini config");
		goto exit;
	}

	cds_cfg->force_target_assert_enabled = crash_inject;

	ucfg_mlme_get_sap_max_offload_peers(hdd_ctx->psoc, &value);
	cds_cfg->ap_maxoffload_peers = value;
	ucfg_mlme_get_sap_max_offload_reorder_buffs(hdd_ctx->psoc,
						    &value);
	cds_cfg->ap_maxoffload_reorderbuffs = value;

	cds_cfg->reorder_offload = DP_REORDER_OFFLOAD_SUPPORT;

	/* IPA micro controller data path offload resource config item */
	cds_cfg->uc_offload_enabled = ucfg_ipa_uc_is_enabled();

	cds_cfg->enable_rxthread =
		ucfg_dp_is_rx_common_thread_enabled(hdd_ctx->psoc);
	ucfg_mlme_get_sap_max_peers(hdd_ctx->psoc, &value);
	cds_cfg->max_station = value;
	cds_cfg->sub_20_channel_width = WLAN_SUB_20_CH_WIDTH_NONE;
	cds_cfg->max_msdus_per_rxinorderind =
		cfg_get(hdd_ctx->psoc, CFG_DP_MAX_MSDUS_PER_RXIND);
	cds_cfg->self_recovery_enabled = self_recovery;
	cds_cfg->fw_timeout_crash = fw_timeout_crash;

	cds_cfg->ito_repeat_count = ito_repeat_count;

	status = ucfg_mlme_get_band_capability(hdd_ctx->psoc, &band_bitmap);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit;

	band_capability = wlan_reg_band_bitmap_to_band_info(band_bitmap);
	cds_cfg->bandcapability = band_capability;
	cds_cfg->num_vdevs = hdd_ctx->config->num_vdevs;
	cds_cfg->enable_tx_compl_tsf64 =
		hdd_tsf_is_tsf64_tx_set(hdd_ctx);
	hdd_txrx_populate_cds_config(cds_cfg, hdd_ctx);
	hdd_lpass_populate_cds_config(cds_cfg, hdd_ctx);
	hdd_populate_feature_set_cds_config(cds_cfg, hdd_ctx);
	cds_init_ini_config(cds_cfg);
	return 0;

exit:
	qdf_mem_free(cds_cfg);
	return -EINVAL;
}

/**
 * hdd_update_user_config() - API to update user configuration
 * parameters to obj mgr which are used by multiple components
 * @hdd_ctx: HDD Context
 *
 * Return: 0 for Success, errno on failure
 */
static int hdd_update_user_config(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc_user_config *user_config;
	uint8_t band_capability;
	uint32_t band_bitmap;
	QDF_STATUS status;
	bool value = false;

	status = ucfg_mlme_get_band_capability(hdd_ctx->psoc, &band_bitmap);
	if (QDF_IS_STATUS_ERROR(status))
		return -EIO;

	user_config = qdf_mem_malloc(sizeof(*user_config));
	if (!user_config)
		return -ENOMEM;

	user_config->dot11_mode = hdd_ctx->config->dot11Mode;
	status = ucfg_mlme_is_11d_enabled(hdd_ctx->psoc, &value);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Invalid 11d_enable flag");
	user_config->is_11d_support_enabled = value;

	value = false;
	status = ucfg_mlme_is_11h_enabled(hdd_ctx->psoc, &value);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Invalid 11h_enable flag");
	user_config->is_11h_support_enabled = value;
	band_capability = wlan_reg_band_bitmap_to_band_info(band_bitmap);
	user_config->band_capability = band_capability;
	wlan_objmgr_psoc_set_user_config(hdd_ctx->psoc, user_config);

	qdf_mem_free(user_config);
	return 0;
}

/**
 * hdd_init_thermal_info - Initialize thermal level
 * @hdd_ctx:	HDD context
 *
 * Initialize thermal level at SME layer and set the thermal level callback
 * which would be called when a configured thermal threshold is hit.
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_init_thermal_info(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	mac_handle_t mac_handle = hdd_ctx->mac_handle;

	status = sme_init_thermal_info(mac_handle);

	if (!QDF_IS_STATUS_SUCCESS(status))
		return qdf_status_to_os_return(status);

	sme_add_set_thermal_level_callback(mac_handle,
					   hdd_set_thermal_level_cb);

	return 0;

}

#if defined(CONFIG_HDD_INIT_WITH_RTNL_LOCK)
/**
 * hdd_hold_rtnl_lock - Hold RTNL lock
 *
 * Hold RTNL lock
 *
 * Return: True if held and false otherwise
 */
static inline bool hdd_hold_rtnl_lock(void)
{
	rtnl_lock();
	return true;
}

/**
 * hdd_release_rtnl_lock - Release RTNL lock
 *
 * Release RTNL lock
 *
 * Return: None
 */
static inline void hdd_release_rtnl_lock(void)
{
	rtnl_unlock();
}
#else
static inline bool hdd_hold_rtnl_lock(void) { return false; }
static inline void hdd_release_rtnl_lock(void) { }
#endif

#if !defined(REMOVE_PKT_LOG)

/* MAX iwpriv command support */
#define PKTLOG_SET_BUFF_SIZE	3
#define PKTLOG_CLEAR_BUFF	4
/* Set Maximum pktlog file size to 64MB */
#define MAX_PKTLOG_SIZE		64

/**
 * hdd_pktlog_set_buff_size() - set pktlog buffer size
 * @hdd_ctx: hdd context
 * @set_value2: pktlog buffer size value
 *
 *
 * Return: 0 for success or error.
 */
static int hdd_pktlog_set_buff_size(struct hdd_context *hdd_ctx, int set_value2)
{
	struct sir_wifi_start_log start_log = { 0 };
	QDF_STATUS status;

	start_log.ring_id = RING_ID_PER_PACKET_STATS;
	start_log.verbose_level = WLAN_LOG_LEVEL_OFF;
	start_log.ini_triggered = cds_is_packet_log_enabled();
	start_log.user_triggered = 1;
	start_log.size = set_value2;
	start_log.is_pktlog_buff_clear = false;

	status = sme_wifi_start_logger(hdd_ctx->mac_handle, start_log);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_wifi_start_logger failed(err=%d)", status);
		hdd_exit();
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_pktlog_clear_buff() - clear pktlog buffer
 * @hdd_ctx: hdd context
 *
 * Return: 0 for success or error.
 */
static int hdd_pktlog_clear_buff(struct hdd_context *hdd_ctx)
{
	struct sir_wifi_start_log start_log;
	QDF_STATUS status;

	start_log.ring_id = RING_ID_PER_PACKET_STATS;
	start_log.verbose_level = WLAN_LOG_LEVEL_OFF;
	start_log.ini_triggered = cds_is_packet_log_enabled();
	start_log.user_triggered = 1;
	start_log.size = 0;
	start_log.is_pktlog_buff_clear = true;

	status = sme_wifi_start_logger(hdd_ctx->mac_handle, start_log);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_wifi_start_logger failed(err=%d)", status);
		hdd_exit();
		return -EINVAL;
	}

	return 0;
}


/**
 * hdd_process_pktlog_command() - process pktlog command
 * @hdd_ctx: hdd context
 * @set_value: value set by user
 * @set_value2: pktlog buffer size value
 *
 * This function process pktlog command.
 * set_value2 only matters when set_value is 3 (set buff size)
 * otherwise we ignore it.
 *
 * Return: 0 for success or error.
 */
int hdd_process_pktlog_command(struct hdd_context *hdd_ctx, uint32_t set_value,
			       int set_value2)
{
	int ret;
	bool enable;
	uint8_t user_triggered = 0;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	hdd_debug("set pktlog %d, set size %d", set_value, set_value2);

	if (set_value > PKTLOG_CLEAR_BUFF) {
		hdd_err("invalid pktlog value %d", set_value);
		return -EINVAL;
	}

	if (set_value == PKTLOG_SET_BUFF_SIZE) {
		if (set_value2 <= 0) {
			hdd_err("invalid pktlog size %d", set_value2);
			return -EINVAL;
		} else if (set_value2 > MAX_PKTLOG_SIZE) {
			hdd_err_rl("Pktlog size is large. max value is %uMB.",
				   MAX_PKTLOG_SIZE);
			return -EINVAL;
		}
		return hdd_pktlog_set_buff_size(hdd_ctx, set_value2);
	} else if (set_value == PKTLOG_CLEAR_BUFF) {
		return hdd_pktlog_clear_buff(hdd_ctx);
	}

	/*
	 * set_value = 0 then disable packetlog
	 * set_value = 1 enable packetlog forcefully
	 * set_value = 2 then disable packetlog if disabled through ini or
	 *                     enable packetlog with AUTO type.
	 */
	enable = ((set_value > 0) && cds_is_packet_log_enabled()) ?
			 true : false;

	if (1 == set_value) {
		enable = true;
		user_triggered = 1;
	}

	return hdd_pktlog_enable_disable(hdd_ctx, enable, user_triggered, 0);
}

/**
 * hdd_pktlog_enable_disable() - Enable/Disable packet logging
 * @hdd_ctx: HDD context
 * @enable_disable_flag: Flag to enable/disable
 * @user_triggered: triggered through iwpriv
 * @size: buffer size to be used for packetlog
 *
 * Return: 0 on success; error number otherwise
 */
int hdd_pktlog_enable_disable(struct hdd_context *hdd_ctx,
			      bool enable_disable_flag,
			      uint8_t user_triggered, int size)
{
	struct sir_wifi_start_log start_log;
	QDF_STATUS status;

	if (hdd_ctx->is_pktlog_enabled && enable_disable_flag)
		return 0;

	if ((!hdd_ctx->is_pktlog_enabled) && (!enable_disable_flag))
		return 0;

	start_log.ring_id = RING_ID_PER_PACKET_STATS;
	start_log.verbose_level =
		enable_disable_flag ?
			WLAN_LOG_LEVEL_ACTIVE : WLAN_LOG_LEVEL_OFF;
	start_log.ini_triggered = cds_is_packet_log_enabled();
	start_log.user_triggered = user_triggered;
	start_log.size = size;
	start_log.is_pktlog_buff_clear = false;
	/*
	 * Use "is_iwpriv_command" flag to distinguish iwpriv command from other
	 * commands. Host uses this flag to decide whether to send pktlog
	 * disable command to fw without sending pktlog enable command
	 * previously. For eg, If vendor sends pktlog disable command without
	 * sending pktlog enable command, then host discards the packet
	 * but for iwpriv command, host will send it to fw.
	 */
	start_log.is_iwpriv_command = 1;

	status = sme_wifi_start_logger(hdd_ctx->mac_handle, start_log);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_wifi_start_logger failed(err=%d)", status);
		hdd_exit();
		return -EINVAL;
	}

	hdd_ctx->is_pktlog_enabled = enable_disable_flag;

	return 0;
}
#endif /* REMOVE_PKT_LOG */

void hdd_free_mac_address_lists(struct hdd_context *hdd_ctx)
{
	hdd_debug("Resetting MAC address lists");
	qdf_mem_zero(hdd_ctx->provisioned_mac_addr,
		    sizeof(hdd_ctx->provisioned_mac_addr));
	qdf_mem_zero(hdd_ctx->derived_mac_addr,
		    sizeof(hdd_ctx->derived_mac_addr));
	hdd_ctx->num_provisioned_addr = 0;
	hdd_ctx->num_derived_addr = 0;
	hdd_ctx->provisioned_intf_addr_mask = 0;
	hdd_ctx->derived_intf_addr_mask = 0;
}

/**
 * hdd_get_platform_wlan_mac_buff() - API to query platform driver
 *                                    for MAC address
 * @dev: Device Pointer
 * @num: Number of Valid Mac address
 *
 * Return: Pointer to MAC address buffer
 */
static uint8_t *hdd_get_platform_wlan_mac_buff(struct device *dev,
					       uint32_t *num)
{
	return pld_get_wlan_mac_address(dev, num);
}

/**
 * hdd_get_platform_wlan_derived_mac_buff() - API to query platform driver
 *                                    for derived MAC address
 * @dev: Device Pointer
 * @num: Number of Valid Mac address
 *
 * Return: Pointer to MAC address buffer
 */
static uint8_t *hdd_get_platform_wlan_derived_mac_buff(struct device *dev,
						       uint32_t *num)
{
	return pld_get_wlan_derived_mac_address(dev, num);
}

/**
 * hdd_populate_random_mac_addr() - API to populate random mac addresses
 * @hdd_ctx: HDD Context
 * @num: Number of random mac addresses needed
 *
 * Generate random addresses using bit manipulation on the base mac address
 *
 * Return: None
 */
void hdd_populate_random_mac_addr(struct hdd_context *hdd_ctx, uint32_t num)
{
	uint32_t idx = hdd_ctx->num_derived_addr;
	uint32_t iter;
	uint8_t *buf = NULL;
	uint8_t macaddr_b3, tmp_br3;
	/*
	 * Consider first provisioned mac address as source address to derive
	 * remaining addresses
	 */

	uint8_t *src = hdd_ctx->provisioned_mac_addr[0].bytes;

	for (iter = 0; iter < num; ++iter, ++idx) {
		buf = hdd_ctx->derived_mac_addr[idx].bytes;
		qdf_mem_copy(buf, src, QDF_MAC_ADDR_SIZE);
		macaddr_b3 = buf[3];
		tmp_br3 = ((macaddr_b3 >> 4 & INTF_MACADDR_MASK) + idx) &
			INTF_MACADDR_MASK;
		macaddr_b3 += tmp_br3;
		macaddr_b3 ^= (1 << INTF_MACADDR_MASK);
		buf[0] |= 0x02;
		buf[3] = macaddr_b3;
		hdd_debug(QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(buf));
		hdd_ctx->num_derived_addr++;
	}
}

/**
 * hdd_platform_wlan_mac() - API to get mac addresses from platform driver
 * @hdd_ctx: HDD Context
 *
 * API to get mac addresses from platform driver and update the driver
 * structures and configure FW with the base mac address.
 * Return: int
 */
static int hdd_platform_wlan_mac(struct hdd_context *hdd_ctx)
{
	uint32_t no_of_mac_addr, iter;
	uint32_t max_mac_addr = QDF_MAX_CONCURRENCY_PERSONA;
	uint32_t mac_addr_size = QDF_MAC_ADDR_SIZE;
	uint8_t *addr, *buf;
	struct device *dev = hdd_ctx->parent_dev;
	tSirMacAddr mac_addr;
	QDF_STATUS status;

	addr = hdd_get_platform_wlan_mac_buff(dev, &no_of_mac_addr);

	if (no_of_mac_addr == 0 || !addr) {
		hdd_debug("No mac configured from platform driver");
		return -EINVAL;
	}

	hdd_free_mac_address_lists(hdd_ctx);

	if (no_of_mac_addr > max_mac_addr)
		no_of_mac_addr = max_mac_addr;

	qdf_mem_copy(&mac_addr, addr, mac_addr_size);

	for (iter = 0; iter < no_of_mac_addr; ++iter, addr += mac_addr_size) {
		buf = hdd_ctx->provisioned_mac_addr[iter].bytes;
		qdf_mem_copy(buf, addr, QDF_MAC_ADDR_SIZE);
		hdd_info("provisioned MAC Addr [%d] "QDF_MAC_ADDR_FMT, iter,
			 QDF_MAC_ADDR_REF(buf));
	}

	hdd_ctx->num_provisioned_addr = no_of_mac_addr;

	if (hdd_ctx->config->mac_provision) {
		addr = hdd_get_platform_wlan_derived_mac_buff(dev,
							      &no_of_mac_addr);

		if (no_of_mac_addr == 0 || !addr)
			hdd_warn("No derived address from platform driver");
		else if (no_of_mac_addr >
			 (max_mac_addr - hdd_ctx->num_provisioned_addr))
			no_of_mac_addr = (max_mac_addr -
					  hdd_ctx->num_provisioned_addr);

		for (iter = 0; iter < no_of_mac_addr; ++iter,
		     addr += mac_addr_size) {
			buf = hdd_ctx->derived_mac_addr[iter].bytes;
			qdf_mem_copy(buf, addr, QDF_MAC_ADDR_SIZE);
			hdd_debug("derived MAC Addr [%d] "QDF_MAC_ADDR_FMT, iter,
				  QDF_MAC_ADDR_REF(buf));
		}
		hdd_ctx->num_derived_addr = no_of_mac_addr;
	}

	no_of_mac_addr = hdd_ctx->num_provisioned_addr +
					 hdd_ctx->num_derived_addr;
	if (no_of_mac_addr < max_mac_addr)
		hdd_populate_random_mac_addr(hdd_ctx, max_mac_addr -
					     no_of_mac_addr);

	status = sme_set_custom_mac_addr(mac_addr);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return -EAGAIN;

	return 0;
}

/**
 * hdd_update_mac_addr_to_fw() - API to update wlan mac addresses to FW
 * @hdd_ctx: HDD Context
 *
 * Update MAC address to FW. If MAC address passed by FW is invalid, host
 * will generate its own MAC and update it to FW.
 *
 * Return: 0 for success
 *         Non-zero error code for failure
 */
static int hdd_update_mac_addr_to_fw(struct hdd_context *hdd_ctx)
{
	tSirMacAddr custom_mac_addr;
	QDF_STATUS status;

	if (hdd_ctx->num_provisioned_addr)
		qdf_mem_copy(&custom_mac_addr,
			     &hdd_ctx->provisioned_mac_addr[0].bytes[0],
			     sizeof(tSirMacAddr));
	else
		qdf_mem_copy(&custom_mac_addr,
			     &hdd_ctx->derived_mac_addr[0].bytes[0],
			     sizeof(tSirMacAddr));
	status = sme_set_custom_mac_addr(custom_mac_addr);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return -EAGAIN;
	return 0;
}

/**
 * hdd_initialize_mac_address() - API to get wlan mac addresses
 * @hdd_ctx: HDD Context
 *
 * Get MAC addresses from platform driver or wlan_mac.bin. If platform driver
 * is provisioned with mac addresses, driver uses it, else it will use
 * wlan_mac.bin to update HW MAC addresses.
 *
 * Return: None
 */
static int hdd_initialize_mac_address(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	int ret;

	ret = hdd_platform_wlan_mac(hdd_ctx);
	if (!ret) {
		hdd_info("using MAC address from platform driver");
		return ret;
	} else if (hdd_ctx->config->mac_provision) {
		hdd_err("getting MAC address from platform driver failed");
		return ret;
	}

	status = hdd_update_mac_config(hdd_ctx);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		hdd_info("using MAC address from wlan_mac.bin");
		return 0;
	}

	hdd_info("using default MAC address");

	/* Use fw provided MAC */
	if (!qdf_is_macaddr_zero(&hdd_ctx->hw_macaddr)) {
		hdd_update_macaddr(hdd_ctx, hdd_ctx->hw_macaddr, false);
		return 0;
	} else if (hdd_generate_macaddr_auto(hdd_ctx) != 0) {
		struct qdf_mac_addr mac_addr;

		hdd_err("MAC failure from device serial no.");
		qdf_get_random_bytes(&mac_addr, sizeof(mac_addr));
		/*
		 * Reset multicast bit (bit-0) and set
		 * locally-administered bit
		 */
		mac_addr.bytes[0] = 0x2;
		hdd_update_macaddr(hdd_ctx, mac_addr, true);
	}

	ret = hdd_update_mac_addr_to_fw(hdd_ctx);
	if (ret)
		hdd_err("MAC address out-of-sync, ret:%d", ret);
	return ret;
}

#define MAX_PDEV_PRE_ENABLE_PARAMS 7
/* params being sent:
 * wmi_pdev_param_tx_chain_mask_1ss
 * wmi_pdev_param_mgmt_retry_limit
 * wmi_pdev_param_default_6ghz_rate
 * wmi_pdev_param_pdev_stats_tx_xretry_ext
 * wmi_pdev_param_smart_chainmask_scheme
 * wmi_pdev_param_alternative_chainmask_scheme
 * wmi_pdev_param_ani_enable
 */

/**
 * hdd_pre_enable_configure() - Configurations prior to cds_enable
 * @hdd_ctx:	HDD context
 *
 * Pre configurations to be done at lower layer before calling cds enable.
 *
 * Return: 0 on success and errno on failure.
 */
static int hdd_pre_enable_configure(struct hdd_context *hdd_ctx)
{
	int ret;
	uint8_t val = 0;
	uint8_t max_retry = 0;
	bool enable_he_mcs0_for_6ghz_mgmt = false;
	uint32_t tx_retry_multiplier;
	QDF_STATUS status;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct dev_set_param setparam[MAX_PDEV_PRE_ENABLE_PARAMS] = {};
	bool check_value;
	uint8_t index = 0;

	cdp_register_pause_cb(soc, wlan_hdd_txrx_pause_cb);
	/* Register HL netdev flow control callback */
	cdp_hl_fc_register(soc, OL_TXRX_PDEV_ID, wlan_hdd_txrx_pause_cb);
	/* Register rx mic error indication handler */
	ucfg_dp_register_rx_mic_error_ind_handler(soc);

	/*
	 * Note that the cds_pre_enable() sequence triggers the cfg download.
	 * The cfg download must occur before we update the SME config
	 * since the SME config operation must access the cfg database
	 */
	status = hdd_set_sme_config(hdd_ctx);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Failed hdd_set_sme_config: %d", status);
		ret = qdf_status_to_os_return(status);
		goto out;
	}

	status = hdd_set_policy_mgr_user_cfg(hdd_ctx);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_alert("Failed hdd_set_policy_mgr_user_cfg: %d", status);
		ret = qdf_status_to_os_return(status);
		goto out;
	}

	status = ucfg_mlme_get_tx_chainmask_1ss(hdd_ctx->psoc, &val);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Get tx_chainmask_1ss from mlme failed");
		ret = qdf_status_to_os_return(status);
		goto out;
	}
	ret = mlme_check_index_setparam(setparam,
					wmi_pdev_param_tx_chain_mask_1ss,
					val, index++,
					MAX_PDEV_PRE_ENABLE_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed at wmi_pdev_param_tx_chain_mask_1ss");
		goto out;

	}

	wlan_mlme_get_mgmt_max_retry(hdd_ctx->psoc, &max_retry);
	ret = mlme_check_index_setparam(setparam,
					wmi_pdev_param_mgmt_retry_limit,
					max_retry, index++,
					MAX_PDEV_PRE_ENABLE_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed at wmi_pdev_param_mgmt_retry_limit");
		goto out;
	}

	wlan_mlme_get_mgmt_6ghz_rate_support(hdd_ctx->psoc,
					     &enable_he_mcs0_for_6ghz_mgmt);
	if (enable_he_mcs0_for_6ghz_mgmt) {
		hdd_debug("HE rates for 6GHz mgmt frames are supported");
		ret = mlme_check_index_setparam(
					setparam,
					wmi_pdev_param_default_6ghz_rate,
					MGMT_DEFAULT_DATA_RATE_6GHZ, index++,
					MAX_PDEV_PRE_ENABLE_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret)) {
			hdd_err("wmi_pdev_param_default_6ghz_rate failed %d",
				ret);
			goto out;
		}
	}

	wlan_mlme_get_tx_retry_multiplier(hdd_ctx->psoc,
					  &tx_retry_multiplier);
	ret = mlme_check_index_setparam(setparam,
					wmi_pdev_param_pdev_stats_tx_xretry_ext,
					tx_retry_multiplier, index++,
					MAX_PDEV_PRE_ENABLE_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed at wmi_pdev_param_pdev_stats_tx_xretry_ext");
		goto out;
	}

	ret = ucfg_get_smart_chainmask_enabled(hdd_ctx->psoc,
					      &check_value);
	if (QDF_IS_STATUS_SUCCESS(ret)) {
		ret = mlme_check_index_setparam(
					 setparam,
					 wmi_pdev_param_smart_chainmask_scheme,
					 (int)check_value, index++,
					 MAX_PDEV_PRE_ENABLE_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret)) {
			hdd_err("failed to set wmi_pdev_param_smart_chainmask_scheme");
			goto out;
		}
	}

	ret = ucfg_get_alternative_chainmask_enabled(hdd_ctx->psoc,
						    &check_value);
	if (QDF_IS_STATUS_SUCCESS(ret)) {
		ret = mlme_check_index_setparam(
				setparam,
				wmi_pdev_param_alternative_chainmask_scheme,
				(int)check_value, index++,
				MAX_PDEV_PRE_ENABLE_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret)) {
			hdd_err("failed to set wmi_pdev_param_alternative_chainmask_scheme");
			goto out;
		}
	}

	ret = ucfg_fwol_get_ani_enabled(hdd_ctx->psoc, &check_value);
	if (QDF_IS_STATUS_SUCCESS(ret)) {
		ret = mlme_check_index_setparam(setparam,
						wmi_pdev_param_ani_enable,
						(int)check_value, index++,
						MAX_PDEV_PRE_ENABLE_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret)) {
			hdd_err("failed to set wmi_pdev_param_ani_enable");
			goto out;
		}
	}

	ret = sme_send_multi_pdev_vdev_set_params(MLME_PDEV_SETPARAM,
						  WMI_PDEV_ID_SOC, setparam,
						  index);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed to send pdev set params");
		goto out;
	}

	ret = hdd_set_pcie_params(hdd_ctx);
	if (ret)
		goto out;

	/* Configure global firmware params */
	ret = ucfg_fwol_configure_global_params(hdd_ctx->psoc, hdd_ctx->pdev);
	if (ret)
		goto out;

	status = hdd_set_sme_chan_list(hdd_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to init channel list: %d", status);
		ret = qdf_status_to_os_return(status);
		goto out;
	}

	if (!hdd_update_config_cfg(hdd_ctx)) {
		hdd_err("config update failed");
		ret = -EINVAL;
		goto out;
	}
	hdd_init_channel_avoidance(hdd_ctx);

out:
	return ret;
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
/**
 * wlan_hdd_p2p_lo_event_callback - P2P listen offload stop event handler
 * @context: context registered with sme_register_p2p_lo_event(). HDD
 *   always registers a hdd context pointer
 * @evt:event structure pointer
 *
 * This is the p2p listen offload stop event handler, it sends vendor
 * event back to supplicant to notify the stop reason.
 *
 * Return: None
 */
static void wlan_hdd_p2p_lo_event_callback(void *context,
					   struct sir_p2p_lo_event *evt)
{
	struct hdd_context *hdd_ctx = context;
	struct sk_buff *vendor_event;
	struct hdd_adapter *adapter;
	enum qca_nl80211_vendor_subcmds_index index =
		QCA_NL80211_VENDOR_SUBCMD_P2P_LO_EVENT_INDEX;

	hdd_enter();

	if (!hdd_ctx) {
		hdd_err("Invalid HDD context pointer");
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, evt->vdev_id);
	if (!adapter) {
		hdd_err("Cannot find adapter by vdev_id = %d",
				evt->vdev_id);
		return;
	}

	vendor_event =
		wlan_cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
						 &adapter->wdev,
						 sizeof(uint32_t) +
						 NLMSG_HDRLEN,
						 index, GFP_KERNEL);
	if (!vendor_event) {
		hdd_err("wlan_cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_STOP_REASON,
			evt->reason_code)) {
		hdd_err("nla put failed");
		wlan_cfg80211_vendor_free_skb(vendor_event);
		return;
	}

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	hdd_debug("Sent P2P_LISTEN_OFFLOAD_STOP event for vdev_id = %d",
			evt->vdev_id);
}
#else
static void wlan_hdd_p2p_lo_event_callback(void *context,
					   struct sir_p2p_lo_event *evt)
{
}
#endif

#ifdef FEATURE_WLAN_DYNAMIC_CVM
static inline int hdd_set_vc_mode_config(struct hdd_context *hdd_ctx)
{
	return sme_set_vc_mode_config(hdd_ctx->config->vc_mode_cfg_bitmap);
}
#else
static inline int hdd_set_vc_mode_config(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * hdd_adaptive_dwelltime_init() - initialization for adaptive dwell time config
 * @hdd_ctx: HDD context
 *
 * This function sends the adaptive dwell time config configuration to the
 * firmware via WMA
 *
 * Return: 0 - success, < 0 - failure
 */
static int hdd_adaptive_dwelltime_init(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	struct adaptive_dwelltime_params dwelltime_params;

	status = ucfg_fwol_get_all_adaptive_dwelltime_params(hdd_ctx->psoc,
							     &dwelltime_params);
	status = ucfg_fwol_set_adaptive_dwelltime_config(&dwelltime_params);

	hdd_debug("Sending Adaptive Dwelltime Configuration to fw");
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Failed to send Adaptive Dwelltime configuration!");
		return -EAGAIN;
	}
	return 0;
}

int hdd_dbs_scan_selection_init(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	struct wmi_dbs_scan_sel_params dbs_scan_params;
	uint32_t i = 0;
	uint8_t count = 0, numentries = 0;
	uint8_t dual_mac_feature;
	uint8_t dbs_scan_config[CDS_DBS_SCAN_PARAM_PER_CLIENT
				* CDS_DBS_SCAN_CLIENTS_MAX];

	status = ucfg_policy_mgr_get_dual_mac_feature(hdd_ctx->psoc,
						      &dual_mac_feature);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("can't get dual mac feature flag");
		return -EINVAL;
	}
	/* check if DBS is enabled or supported */
	if ((dual_mac_feature == DISABLE_DBS_CXN_AND_SCAN) ||
	    (dual_mac_feature == ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN))
		return -EINVAL;

	hdd_string_to_u8_array(hdd_ctx->config->dbs_scan_selection,
			       dbs_scan_config, &numentries,
			       (CDS_DBS_SCAN_PARAM_PER_CLIENT
				* CDS_DBS_SCAN_CLIENTS_MAX));

	if (!numentries) {
		hdd_debug("Do not send scan_selection_config");
		return 0;
	}

	/* hdd_set_fw_log_params */
	dbs_scan_params.num_clients = 0;
	while (count < (numentries - 2)) {
		dbs_scan_params.module_id[i] = dbs_scan_config[count];
		dbs_scan_params.num_dbs_scans[i] = dbs_scan_config[count + 1];
		dbs_scan_params.num_non_dbs_scans[i] =
			dbs_scan_config[count + 2];
		dbs_scan_params.num_clients++;
		hdd_debug("module:%d NDS:%d NNDS:%d",
			  dbs_scan_params.module_id[i],
			  dbs_scan_params.num_dbs_scans[i],
			  dbs_scan_params.num_non_dbs_scans[i]);
		count += CDS_DBS_SCAN_PARAM_PER_CLIENT;
		i++;
	}

	dbs_scan_params.pdev_id = 0;

	hdd_debug("clients:%d pdev:%d",
		  dbs_scan_params.num_clients, dbs_scan_params.pdev_id);

	status = sme_set_dbs_scan_selection_config(hdd_ctx->mac_handle,
						   &dbs_scan_params);
	hdd_debug("Sending DBS Scan Selection Configuration to fw");
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Failed to send DBS Scan selection configuration!");
		return -EAGAIN;
	}
	return 0;
}

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
/**
 * hdd_set_auto_shutdown_cb() - Set auto shutdown callback
 * @hdd_ctx:	HDD context
 *
 * Set auto shutdown callback to get indications from firmware to indicate
 * userspace to shutdown WLAN after a configured amount of inactivity.
 *
 * Return: 0 on success and errno on failure.
 */
static int hdd_set_auto_shutdown_cb(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	if (!hdd_ctx->config->wlan_auto_shutdown)
		return 0;

	status = sme_set_auto_shutdown_cb(hdd_ctx->mac_handle,
					  wlan_hdd_auto_shutdown_cb);
	if (status != QDF_STATUS_SUCCESS)
		hdd_err("Auto shutdown feature could not be enabled: %d",
			status);

	return qdf_status_to_os_return(status);
}
#else
static int hdd_set_auto_shutdown_cb(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

#ifdef MWS_COEX
#define MAX_PDEV_MWSCOEX_PARAMS 4
/* params being sent:
 * wmi_pdev_param_mwscoex_4g_allow_quick_ftdm
 * wmi_pdev_param_mwscoex_set_5gnr_pwr_limit
 * wmi_pdev_param_mwscoex_pcc_chavd_delay
 * wmi_pdev_param_mwscoex_scc_chavd_delay
 */

/**
 * hdd_init_mws_coex() - Initialize MWS coex configurations
 * @hdd_ctx:   HDD context
 *
 * This function sends MWS-COEX 4G quick FTDM and
 * MWS-COEX 5G-NR power limit to FW
 *
 * Return: 0 on success and errno on failure.
 */
static int hdd_init_mws_coex(struct hdd_context *hdd_ctx)
{
	int ret = 0;
	uint32_t mws_coex_4g_quick_tdm = 0, mws_coex_5g_nr_pwr_limit = 0;
	uint32_t mws_coex_pcc_channel_avoid_delay = 0;
	uint32_t mws_coex_scc_channel_avoid_delay = 0;
	struct dev_set_param setparam[MAX_PDEV_MWSCOEX_PARAMS] = {};
	uint8_t index = 0;

	ucfg_mlme_get_mws_coex_4g_quick_tdm(hdd_ctx->psoc,
					    &mws_coex_4g_quick_tdm);
	ret = mlme_check_index_setparam(
				setparam,
				wmi_pdev_param_mwscoex_4g_allow_quick_ftdm,
				mws_coex_4g_quick_tdm, index++,
				MAX_PDEV_MWSCOEX_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed at wmi_pdev_param_mwscoex_4g_allow_quick_ftdm");
		goto error;
	}

	ucfg_mlme_get_mws_coex_5g_nr_pwr_limit(hdd_ctx->psoc,
					       &mws_coex_5g_nr_pwr_limit);
	ret = mlme_check_index_setparam(
				      setparam,
				      wmi_pdev_param_mwscoex_set_5gnr_pwr_limit,
				      mws_coex_5g_nr_pwr_limit, index++,
				      MAX_PDEV_MWSCOEX_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed at wmi_pdev_param_mwscoex_set_5gnr_pwr_limit");
		goto error;
	}

	ucfg_mlme_get_mws_coex_pcc_channel_avoid_delay(
					hdd_ctx->psoc,
					&mws_coex_pcc_channel_avoid_delay);
	ret = mlme_check_index_setparam(setparam,
					wmi_pdev_param_mwscoex_pcc_chavd_delay,
					mws_coex_pcc_channel_avoid_delay,
					index++, MAX_PDEV_MWSCOEX_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed at wmi_pdev_param_mwscoex_pcc_chavd_delay");
		goto error;
	}

	ucfg_mlme_get_mws_coex_scc_channel_avoid_delay(
					hdd_ctx->psoc,
					&mws_coex_scc_channel_avoid_delay);
	ret = mlme_check_index_setparam(setparam,
					wmi_pdev_param_mwscoex_scc_chavd_delay,
					mws_coex_scc_channel_avoid_delay,
					index++, MAX_PDEV_MWSCOEX_PARAMS);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("failed at wmi_pdev_param_mwscoex_scc_chavd_delay");
		goto error;
	}
	ret = sme_send_multi_pdev_vdev_set_params(MLME_PDEV_SETPARAM,
						  WMI_PDEV_ID_SOC, setparam,
						  index);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("failed to send pdev MWSCOEX set params");
error:
	return ret;
}
#else
static int hdd_init_mws_coex(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

#ifdef THERMAL_STATS_SUPPORT
static void hdd_thermal_stats_cmd_init(struct hdd_context *hdd_ctx)
{
	hdd_send_get_thermal_stats_cmd(hdd_ctx, thermal_stats_init, NULL, NULL);
}
#else
static void hdd_thermal_stats_cmd_init(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef WLAN_FEATURE_CAL_FAILURE_TRIGGER
/**
 * hdd_cal_fail_send_event()- send calibration failure information
 * @cal_type: calibration type
 * @reason: reason for calibration failure
 *
 * This Function sends calibration failure diag event
 *
 * Return: void.
 */
static void hdd_cal_fail_send_event(uint8_t cal_type, uint8_t reason)
{
	/*
	 * For now we are going with the print. Once CST APK has support to
	 * read the diag events then we will add the diag event here.
	 */
	hdd_debug("Received cal failure event with cal_type:%x reason:%x",
		  cal_type, reason);
}
#else
static inline void hdd_cal_fail_send_event(uint8_t cal_type, uint8_t reason)
{
}
#endif

/**
 * hdd_features_init() - Init features
 * @hdd_ctx:	HDD context
 *
 * Initialize features and their feature context after WLAN firmware is up.
 *
 * Return: 0 on success and errno on failure.
 */
static int hdd_features_init(struct hdd_context *hdd_ctx)
{
	struct tx_power_limit hddtxlimit;
	QDF_STATUS status;
	int ret;
	mac_handle_t mac_handle;
	bool b_cts2self, is_imps_enabled;
	bool rf_test_mode;
	bool std_6ghz_conn_policy;
	uint32_t fw_data_stall_evt;
	bool disable_vlp_sta_conn_sp_ap;

	hdd_enter();

	ret = hdd_init_mws_coex(hdd_ctx);
	if (ret)
		hdd_warn("Error initializing mws-coex");

	/* FW capabilities received, Set the Dot11 mode */
	mac_handle = hdd_ctx->mac_handle;
	sme_setdef_dot11mode(mac_handle);

	ucfg_mlme_is_imps_enabled(hdd_ctx->psoc, &is_imps_enabled);
	hdd_set_idle_ps_config(hdd_ctx, is_imps_enabled);

	fw_data_stall_evt = ucfg_dp_fw_data_stall_evt_enabled();

	/* Send Enable/Disable data stall detection cmd to FW */
	sme_cli_set_command(0, wmi_pdev_param_data_stall_detect_enable,
			    fw_data_stall_evt, PDEV_CMD);

	ucfg_mlme_get_go_cts2self_for_sta(hdd_ctx->psoc, &b_cts2self);
	if (b_cts2self)
		sme_set_cts2self_for_p2p_go(mac_handle);

	if (hdd_set_vc_mode_config(hdd_ctx))
		hdd_warn("Error in setting Voltage Corner mode config to FW");

	if (ucfg_dp_rx_ol_init(hdd_ctx->psoc, hdd_ctx->is_wifi3_0_target))
		hdd_err("Unable to initialize Rx LRO/GRO in fw");

	if (hdd_adaptive_dwelltime_init(hdd_ctx))
		hdd_err("Unable to send adaptive dwelltime setting to FW");

	if (hdd_dbs_scan_selection_init(hdd_ctx))
		hdd_err("Unable to send DBS scan selection setting to FW");

	ret = hdd_init_thermal_info(hdd_ctx);
	if (ret) {
		hdd_err("Error while initializing thermal information");
		return ret;
	}

	/**
	 * In case of SSR/PDR, if pktlog was enabled manually before
	 * SSR/PDR, then enable it again automatically after Wlan
	 * device up.
	 * During SSR/PDR, pktlog will be disabled as part of
	 * hdd_features_deinit if pktlog is enabled in ini.
	 * Re-enable pktlog in SSR case, if pktlog is enabled in ini.
	 */
	if (hdd_get_conparam() != QDF_GLOBAL_MONITOR_MODE &&
	    (cds_is_packet_log_enabled() ||
	    (cds_is_driver_recovering() && hdd_ctx->is_pktlog_enabled)))
		hdd_pktlog_enable_disable(hdd_ctx, true, 0, 0);

	hddtxlimit.txPower2g = ucfg_get_tx_power(hdd_ctx->psoc, BAND_2G);
	hddtxlimit.txPower5g = ucfg_get_tx_power(hdd_ctx->psoc, BAND_5G);
	status = sme_txpower_limit(mac_handle, &hddtxlimit);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Error setting txlimit in sme: %d", status);

	wlan_hdd_tsf_init(hdd_ctx);

	status = sme_enable_disable_chanavoidind_event(mac_handle, 0);
	if (QDF_IS_STATUS_ERROR(status) && (status != QDF_STATUS_E_NOSUPPORT)) {
		hdd_err("Failed to disable Chan Avoidance Indication");
		return -EINVAL;
	}

	/* register P2P Listen Offload event callback */
	if (wma_is_p2p_lo_capable())
		sme_register_p2p_lo_event(mac_handle, hdd_ctx,
					  wlan_hdd_p2p_lo_event_callback);
	wlan_hdd_register_mcc_quota_event_callback(hdd_ctx);
	ret = hdd_set_auto_shutdown_cb(hdd_ctx);

	if (ret)
		return -EINVAL;

	wlan_hdd_init_chan_info(hdd_ctx);
	wlan_hdd_twt_init(hdd_ctx);
	wlan_hdd_gpio_wakeup_init(hdd_ctx);

	status = ucfg_mlme_is_rf_test_mode_enabled(hdd_ctx->psoc,
						   &rf_test_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Get rf test mode failed");
		return QDF_STATUS_E_FAILURE;
	}

	if (rf_test_mode) {
		wlan_cm_set_check_6ghz_security(hdd_ctx->psoc, false);
		wlan_cm_set_6ghz_key_mgmt_mask(hdd_ctx->psoc,
					       DEFAULT_KEYMGMT_6G_MASK);
	}

	status = ucfg_mlme_is_standard_6ghz_conn_policy_enabled(hdd_ctx->psoc,
							&std_6ghz_conn_policy);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Get 6ghz standard connection policy failed");
		return QDF_STATUS_E_FAILURE;
	}
	if (std_6ghz_conn_policy)
		wlan_cm_set_standard_6ghz_conn_policy(hdd_ctx->psoc, true);

	status = ucfg_mlme_is_disable_vlp_sta_conn_to_sp_ap_enabled(
						hdd_ctx->psoc,
						&disable_vlp_sta_conn_sp_ap);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Get disable vlp sta conn to sp flag failed");
		return QDF_STATUS_E_FAILURE;
	}

	if (disable_vlp_sta_conn_sp_ap)
		wlan_cm_set_disable_vlp_sta_conn_to_sp_ap(hdd_ctx->psoc, true);

	hdd_thermal_stats_cmd_init(hdd_ctx);
	sme_set_cal_failure_event_cb(hdd_ctx->mac_handle,
				     hdd_cal_fail_send_event);

	hdd_exit();
	return 0;
}

/**
 * hdd_register_bcn_cb() - register scan beacon callback
 * @hdd_ctx: Pointer to the HDD context
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS hdd_register_bcn_cb(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	status = ucfg_scan_register_bcn_cb(hdd_ctx->psoc,
		wlan_cfg80211_inform_bss_frame,
		SCAN_CB_TYPE_INFORM_BCN);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("failed to register SCAN_CB_TYPE_INFORM_BCN with status code %08d [x%08x]",
			status, status);
		return status;
	}

	status = ucfg_scan_register_bcn_cb(hdd_ctx->psoc,
		wlan_cfg80211_unlink_bss_list,
		SCAN_CB_TYPE_UNLINK_BSS);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("failed to refister SCAN_CB_TYPE_FLUSH_BSS with status code %08d [x%08x]",
			status, status);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_v2_flow_pool_map() - Flow pool create callback when vdev is active
 * @vdev_id: vdev_id, corresponds to flow_pool
 *
 * Return: none.
 */
static void hdd_v2_flow_pool_map(int vdev_id)
{
	QDF_STATUS status;

	status = cdp_flow_pool_map(cds_get_context(QDF_MODULE_ID_SOC),
				   OL_TXRX_PDEV_ID,
				   vdev_id);
	/*
	 * For Adrastea flow control v2 is based on FW MAP events,
	 * so this above callback is not implemented.
	 * Hence this is not actual failure. Dont return failure
	 */
	if ((status != QDF_STATUS_SUCCESS) &&
	    (status != QDF_STATUS_E_INVAL)) {
		hdd_err("vdev_id: %d, failed to create flow pool status %d",
			vdev_id, status);
	}
}

/**
 * hdd_v2_flow_pool_unmap() - Flow pool create callback when vdev is not active
 * @vdev_id: vdev_id, corresponds to flow_pool
 *
 * Return: none.
 */
static void hdd_v2_flow_pool_unmap(int vdev_id)
{
	cdp_flow_pool_unmap(cds_get_context(QDF_MODULE_ID_SOC),
			    OL_TXRX_PDEV_ID, vdev_id);
}

static void hdd_hastings_bt_war_initialize(struct hdd_context *hdd_ctx)
{
	if (hdd_ctx->config->iface_change_wait_time)
		hdd_hastings_bt_war_disable_fw(hdd_ctx);
	else
		hdd_hastings_bt_war_enable_fw(hdd_ctx);
}

#define MAX_PDEV_CFG_CDS_PARAMS 8
/* params being sent:
 * wmi_pdev_param_set_iot_pattern
 * wmi_pdev_param_max_mpdus_in_ampdu
 * wmi_pdev_param_enable_rts_sifs_bursting
 * wmi_pdev_param_peer_stats_info_enable
 * wmi_pdev_param_abg_mode_tx_chain_num
 * wmi_pdev_param_gcmp_support_enable
 * wmi_pdev_auto_detect_power_failure
 * wmi_pdev_param_fast_pwr_transition
 */

/**
 * hdd_configure_cds() - Configure cds modules
 * @hdd_ctx:	HDD context
 *
 * Enable Cds modules after WLAN firmware is up.
 *
 * Return: 0 on success and errno on failure.
 */
int hdd_configure_cds(struct hdd_context *hdd_ctx)
{
	int ret;
	QDF_STATUS status;
	int set_value;
	mac_handle_t mac_handle;
	bool enable_rts_sifsbursting;
	uint8_t enable_phy_reg_retention;
	uint8_t max_mpdus_inampdu, is_force_1x1 = 0;
	uint32_t num_abg_tx_chains = 0;
	uint16_t num_11b_tx_chains = 0;
	uint16_t num_11ag_tx_chains = 0;
	struct policy_mgr_dp_cbacks dp_cbs = {0};
	bool value;
	enum pmo_auto_pwr_detect_failure_mode auto_power_fail_mode;
	bool bval = false;
	uint8_t max_index = MAX_PDEV_CFG_CDS_PARAMS;
	struct dev_set_param setparam[MAX_PDEV_CFG_CDS_PARAMS] = {};
	uint8_t index = 0;
	uint8_t next_index = 0;
	mac_handle = hdd_ctx->mac_handle;

	status = ucfg_policy_mgr_get_force_1x1(hdd_ctx->psoc, &is_force_1x1);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to get force 1x1 value");
		goto out;
	}
	if (is_force_1x1) {
		status = mlme_check_index_setparam(
						setparam,
						wmi_pdev_param_set_iot_pattern,
						1, index++,
						max_index);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("failed at wmi_pdev_param_set_iot_pattern");
			goto out;
		}
	}
	/* set chip power save failure detected callback */
	sme_set_chip_pwr_save_fail_cb(mac_handle,
				      hdd_chip_pwr_save_fail_detected_cb);

	status = ucfg_get_max_mpdus_inampdu(hdd_ctx->psoc,
					    &max_mpdus_inampdu);
	if (status) {
		hdd_err("Failed to get max mpdus in ampdu value");
		goto out;
	}

	if (max_mpdus_inampdu) {
		set_value = max_mpdus_inampdu;
		status = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_max_mpdus_in_ampdu,
					      set_value, index++,
					      max_index);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("failed at  wmi_pdev_param_max_mpdus_in_ampdu");
			goto out;
		}
	}

	status = ucfg_get_enable_rts_sifsbursting(hdd_ctx->psoc,
						  &enable_rts_sifsbursting);
	if (status) {
		hdd_err("Failed to get rts sifs bursting value");
		goto out;
	}

	if (enable_rts_sifsbursting) {
		set_value = enable_rts_sifsbursting;
		status = mlme_check_index_setparam(
					setparam,
					wmi_pdev_param_enable_rts_sifs_bursting,
					set_value, index++,
					max_index);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed at wmi_pdev_param_enable_rts_sifs_bursting");
			goto out;
		}
	}

	ucfg_mlme_get_sap_get_peer_info(hdd_ctx->psoc, &value);
	if (value) {
		set_value = value;
		status = mlme_check_index_setparam(
					setparam,
					wmi_pdev_param_peer_stats_info_enable,
					set_value, index++,
					max_index);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed at wmi_pdev_param_peer_stats_info_enable");
			goto out;
		}
	}

	status = ucfg_mlme_get_num_11b_tx_chains(hdd_ctx->psoc,
						 &num_11b_tx_chains);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to get num_11b_tx_chains");
		goto out;
	}

	status = ucfg_mlme_get_num_11ag_tx_chains(hdd_ctx->psoc,
						  &num_11ag_tx_chains);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to get num_11ag_tx_chains");
		goto out;
	}

	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &bval);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get vht_enable2x2");

	if (!bval) {
		if (num_11b_tx_chains > 1)
			num_11b_tx_chains = 1;
		if (num_11ag_tx_chains > 1)
			num_11ag_tx_chains = 1;
	}
	WMI_PDEV_PARAM_SET_11B_TX_CHAIN_NUM(num_abg_tx_chains,
					    num_11b_tx_chains);
	WMI_PDEV_PARAM_SET_11AG_TX_CHAIN_NUM(num_abg_tx_chains,
					     num_11ag_tx_chains);
	status = mlme_check_index_setparam(setparam,
					   wmi_pdev_param_abg_mode_tx_chain_num,
					   num_abg_tx_chains, index++,
					   max_index);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed at wmi_pdev_param_abg_mode_tx_chain_num");
		goto out;
	}
	/* Send some pdev params to maintain legacy order of pdev set params
	 * at hdd_pre_enable_configure
	 */
	status = sme_send_multi_pdev_vdev_set_params(MLME_PDEV_SETPARAM,
						     WMI_PDEV_ID_SOC, setparam,
						     index);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send 1st set of pdev params");
		goto out;
	}
	if (!ucfg_reg_is_regdb_offloaded(hdd_ctx->psoc))
		ucfg_reg_program_default_cc(hdd_ctx->pdev,
					    hdd_ctx->reg.reg_domain);

	ret = hdd_pre_enable_configure(hdd_ctx);
	if (ret) {
		hdd_err("Failed to pre-configure cds");
		goto out;
	}

	/* Always get latest IPA resources allocated from cds_open and configure
	 * IPA module before configuring them to FW. Sequence required as crash
	 * observed otherwise.
	 */

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		ipa_disable_register_cb();
	} else {
		status = ipa_register_is_ipa_ready(hdd_ctx->pdev);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("ipa_register_is_ipa_ready failed");
			goto out;
		}
	}

	/*
	 * Start CDS which starts up the SME/MAC/HAL modules and everything
	 * else
	 */
	status = cds_enable(hdd_ctx->psoc);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("cds_enable failed");
		goto out;
	}

	status = hdd_post_cds_enable_config(hdd_ctx);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("hdd_post_cds_enable_config failed");
		goto cds_disable;
	}
	status = hdd_register_bcn_cb(hdd_ctx);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("hdd_register_bcn_cb failed");
		goto cds_disable;
	}

	ret = hdd_features_init(hdd_ctx);
	if (ret)
		goto cds_disable;

	/*
	 * Donot disable rx offload on concurrency for lithium and
	 * beryllium based targets
	 */
	if (!hdd_ctx->is_wifi3_0_target)
		if (ucfg_dp_is_ol_enabled(hdd_ctx->psoc))
			dp_cbs.hdd_disable_rx_ol_in_concurrency =
					hdd_disable_rx_ol_in_concurrency;
	dp_cbs.hdd_set_rx_mode_rps_cb = ucfg_dp_set_rx_mode_rps;
	dp_cbs.hdd_ipa_set_mcc_mode_cb = hdd_ipa_set_mcc_mode;
	dp_cbs.hdd_v2_flow_pool_map = hdd_v2_flow_pool_map;
	dp_cbs.hdd_v2_flow_pool_unmap = hdd_v2_flow_pool_unmap;
	status = policy_mgr_register_dp_cb(hdd_ctx->psoc, &dp_cbs);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_debug("Failed to register DP cb with Policy Manager");
		goto cds_disable;
	}
	status = policy_mgr_register_mode_change_cb(hdd_ctx->psoc,
					       wlan_hdd_send_mode_change_event);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_debug("Failed to register mode change cb with Policy Manager");
		goto cds_disable;
	}

	if (hdd_green_ap_enable_egap(hdd_ctx))
		hdd_debug("enhance green ap is not enabled");

	hdd_register_green_ap_callback(hdd_ctx->pdev);

	if (0 != wlan_hdd_set_wow_pulse(hdd_ctx, true))
		hdd_debug("Failed to set wow pulse");

	max_index = max_index - index;
	status = mlme_check_index_setparam(
				      setparam + index,
				      wmi_pdev_param_gcmp_support_enable,
				      ucfg_fwol_get_gcmp_enable(hdd_ctx->psoc),
				      next_index++, max_index);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed at wmi_pdev_param_gcmp_support_enable");
		goto out;
	}

	 auto_power_fail_mode =
		ucfg_pmo_get_auto_power_fail_mode(hdd_ctx->psoc);
	status = mlme_check_index_setparam(
				      setparam + index,
				      wmi_pdev_auto_detect_power_failure,
				      auto_power_fail_mode,
				      next_index++, max_index);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed at wmi_pdev_auto_detect_power_failure");
		goto out;
	}

	status = ucfg_get_enable_phy_reg_retention(hdd_ctx->psoc,
						   &enable_phy_reg_retention);

	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	if (enable_phy_reg_retention) {
		status = mlme_check_index_setparam(
					setparam + index,
					wmi_pdev_param_fast_pwr_transition,
					enable_phy_reg_retention,
					next_index++, max_index);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("failed at wmi_pdev_param_fast_pwr_transition");
			goto out;
		}
	}
	/*Send remaining pdev setparams from array*/
	status = sme_send_multi_pdev_vdev_set_params(MLME_PDEV_SETPARAM,
						     WMI_PDEV_ID_SOC,
						     setparam + index,
						     next_index);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to send 2nd set of pdev set params");
		goto out;
	}

	hdd_hastings_bt_war_initialize(hdd_ctx);

	wlan_hdd_hang_event_notifier_register(hdd_ctx);
	return 0;

cds_disable:
	cds_disable(hdd_ctx->psoc);

out:
	return -EINVAL;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
static void hdd_deregister_policy_manager_callback(
			struct wlan_objmgr_psoc *psoc)
{
	if (QDF_STATUS_SUCCESS !=
	    policy_mgr_deregister_hdd_cb(psoc)) {
		hdd_err("HDD callback deregister with policy manager failed");
	}
}
#else
static void hdd_deregister_policy_manager_callback(
			struct wlan_objmgr_psoc *psoc)
{
}
#endif

int hdd_wlan_stop_modules(struct hdd_context *hdd_ctx, bool ftm_mode)
{
	void *hif_ctx;
	qdf_device_t qdf_ctx;
	QDF_STATUS qdf_status;
	bool is_recovery_stop = cds_is_driver_recovering();
	int ret = 0;
	int debugfs_threads;
	struct target_psoc_info *tgt_hdl;
	struct bbm_params param = {0};

	hdd_enter();
	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_ctx)
		return -EINVAL;

	cds_set_driver_state_module_stop(true);

	debugfs_threads = hdd_return_debugfs_threads_count();

	if (debugfs_threads > 0 || hdd_ctx->is_wiphy_suspended) {
		hdd_warn("Debugfs threads %d, wiphy suspend %d",
			 debugfs_threads, hdd_ctx->is_wiphy_suspended);

		if (IS_IDLE_STOP && !ftm_mode) {
			hdd_psoc_idle_timer_start(hdd_ctx);
			cds_set_driver_state_module_stop(false);

			ucfg_dp_bus_bw_compute_timer_stop(hdd_ctx->psoc);
			return -EAGAIN;
		}
	}

	ucfg_dp_bus_bw_compute_timer_stop(hdd_ctx->psoc);
	hdd_deregister_policy_manager_callback(hdd_ctx->psoc);

	/* free user wowl patterns */
	hdd_free_user_wowl_ptrns();

	switch (hdd_ctx->driver_status) {
	case DRIVER_MODULES_UNINITIALIZED:
		hdd_debug("Modules not initialized just return");
		goto done;
	case DRIVER_MODULES_CLOSED:
		hdd_debug("Modules already closed");
		goto done;
	case DRIVER_MODULES_ENABLED:
		hdd_debug("Wlan transitioning (CLOSED <- ENABLED)");

		if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
			hdd_disable_power_management(hdd_ctx);
			break;
		}

		if (hdd_get_conparam() == QDF_GLOBAL_EPPING_MODE)
			break;

		hdd_skip_acs_scan_timer_deinit(hdd_ctx);

		hdd_disable_power_management(hdd_ctx);

		if (hdd_get_conparam() == QDF_GLOBAL_MISSION_MODE)
			ucfg_dp_direct_link_deinit(hdd_ctx->psoc,
						   is_recovery_stop);

		if (hdd_deconfigure_cds(hdd_ctx)) {
			hdd_err("Failed to de-configure CDS");
			QDF_ASSERT(0);
			ret = -EINVAL;
		}
		hdd_debug("successfully Disabled the CDS modules!");

		break;
	default:
		QDF_DEBUG_PANIC("Unknown driver state:%d",
				hdd_ctx->driver_status);
		ret = -EINVAL;
		goto done;
	}

	hdd_destroy_sysfs_files();
	hdd_debug("Closing CDS modules!");

	if (hdd_get_conparam() != QDF_GLOBAL_EPPING_MODE) {
		qdf_status = cds_post_disable();
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("Failed to process post CDS disable! :%d",
				qdf_status);
			ret = -EINVAL;
			QDF_ASSERT(0);
		}

		hdd_unregister_notifiers(hdd_ctx);
		/* De-register the SME callbacks */
		hdd_deregister_cb(hdd_ctx);

		hdd_runtime_suspend_context_deinit(hdd_ctx);

		qdf_status = cds_dp_close(hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_warn("Failed to stop CDS DP: %d", qdf_status);
			ret = -EINVAL;
			QDF_ASSERT(0);
		}

		hdd_component_pdev_close(hdd_ctx->pdev);
		dispatcher_pdev_close(hdd_ctx->pdev);
		ret = hdd_objmgr_release_and_destroy_pdev(hdd_ctx);
		if (ret) {
			hdd_err("Failed to destroy pdev; errno:%d", ret);
			QDF_ASSERT(0);
		}

		qdf_status = cds_close(hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_warn("Failed to stop CDS: %d", qdf_status);
			ret = -EINVAL;
			QDF_ASSERT(0);
		}

		qdf_status = wbuff_module_deinit();
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_err("WBUFF de-init unsuccessful; status: %d",
				qdf_status);

		hdd_component_psoc_close(hdd_ctx->psoc);
		/* pdev close and destroy use tx rx ops so call this here */
		wlan_global_lmac_if_close(hdd_ctx->psoc);
	}

	/*
	 * Reset total mac phy during module stop such that during
	 * next module start same psoc is used to populate new service
	 * ready data
	 */
	tgt_hdl = wlan_psoc_get_tgt_if_handle(hdd_ctx->psoc);
	if (tgt_hdl)
		target_psoc_set_total_mac_phy_cnt(tgt_hdl, 0);


	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx)
		ret = -EINVAL;

	if (hdd_get_conparam() == QDF_GLOBAL_EPPING_MODE) {
		epping_disable();
		epping_close();
	}

	wlan_connectivity_logging_stop();

	ucfg_ipa_component_config_free();
	hdd_hif_close(hdd_ctx, hif_ctx);

	ol_cds_free();

	if (IS_IDLE_STOP) {
		ret = pld_power_off(qdf_ctx->dev);
		if (ret)
			hdd_err("Failed to power down device; errno:%d", ret);
	}

	/* Free the cache channels of the command SET_DISABLE_CHANNEL_LIST */
	wlan_hdd_free_cache_channels(hdd_ctx);
	hdd_driver_mem_cleanup();

	/* Free the resources allocated while storing SAR config. These needs
	 * to be freed only in the case when it is not SSR. As in the case of
	 * SSR, the values needs to be intact so that it can be restored during
	 * reinit path.
	 */
	if (!is_recovery_stop)
		wlan_hdd_free_sar_config(hdd_ctx);

	hdd_sap_destroy_ctx_all(hdd_ctx, is_recovery_stop);
	hdd_sta_destroy_ctx_all(hdd_ctx);

	/*
	 * Reset the driver mode specific bus bw level
	 */
	param.policy = BBM_DRIVER_MODE_POLICY;
	param.policy_info.driver_mode = QDF_GLOBAL_MAX_MODE;
	ucfg_dp_bbm_apply_independent_policy(hdd_ctx->psoc, &param);

	hdd_deinit_adapter_ops_wq(hdd_ctx);
	hdd_check_for_leaks(hdd_ctx, is_recovery_stop);
	hdd_debug_domain_set(QDF_DEBUG_DOMAIN_INIT);

	/* Restore PS params for monitor mode */
	if (hdd_get_conparam() == QDF_GLOBAL_MONITOR_MODE)
		hdd_restore_all_ps(hdd_ctx);

	/* Once the firmware sequence is completed reset this flag */
	hdd_ctx->imps_enabled = false;
	hdd_ctx->is_dual_mac_cfg_updated = false;
	hdd_ctx->driver_status = DRIVER_MODULES_CLOSED;
	hdd_ctx->is_fw_dbg_log_levels_configured = false;
	hdd_debug("Wlan transitioned (now CLOSED)");

done:
	hdd_exit();

	return ret;
}

#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
/**
 * hdd_state_info_dump() - prints state information of hdd layer
 * @buf_ptr: buffer pointer
 * @size: size of buffer to be filled
 *
 * This function is used to dump state information of hdd layer
 *
 * Return: None
 */
static void hdd_state_info_dump(char **buf_ptr, uint16_t *size)
{
	struct hdd_context *hdd_ctx;
	struct hdd_station_ctx *hdd_sta_ctx;
	struct hdd_adapter *adapter, *next_adapter = NULL;
	uint16_t len = 0;
	char *buf = *buf_ptr;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_STATE_INFO_DUMP;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	hdd_debug("size of buffer: %d", *size);

	len += scnprintf(buf + len, *size - len,
		"\n is_wiphy_suspended %d", hdd_ctx->is_wiphy_suspended);
	len += scnprintf(buf + len, *size - len,
		"\n is_scheduler_suspended %d",
		hdd_ctx->is_scheduler_suspended);

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		len += scnprintf(buf + len, *size - len,
				"\n device name: %s", adapter->dev->name);
		len += scnprintf(buf + len, *size - len,
				"\n device_mode: %d", adapter->device_mode);
		switch (adapter->device_mode) {
		case QDF_STA_MODE:
		case QDF_P2P_CLIENT_MODE:
			hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			len += scnprintf(buf + len, *size - len,
				"\n conn_state: %d",
				hdd_sta_ctx->conn_info.conn_state);
			break;

		default:
			break;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	*size -= len;
	*buf_ptr += len;
}

/**
 * hdd_register_debug_callback() - registration function for hdd layer
 * to print hdd state information
 *
 * Return: None
 */
static void hdd_register_debug_callback(void)
{
	qdf_register_debug_callback(QDF_MODULE_ID_HDD, &hdd_state_info_dump);
}
#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static void hdd_register_debug_callback(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */

/*
 * wlan_init_bug_report_lock() - Initialize bug report lock
 *
 * This function is used to create bug report lock
 *
 * Return: None
 */
static void wlan_init_bug_report_lock(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		hdd_err("cds context is NULL");
		return;
	}

	qdf_spinlock_create(&p_cds_context->bug_report_lock);
}

#ifdef DISABLE_CHANNEL_LIST
static QDF_STATUS wlan_hdd_cache_chann_mutex_create(struct hdd_context *hdd_ctx)
{
	return qdf_mutex_create(&hdd_ctx->cache_channel_lock);
}
#else
static QDF_STATUS wlan_hdd_cache_chann_mutex_create(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS hdd_open_adapter_no_trans(struct hdd_context *hdd_ctx,
				     enum QDF_OPMODE op_mode,
				     const char *iface_name,
				     uint8_t *mac_addr_bytes,
				     struct hdd_adapter_create_param *params)
{
	struct osif_vdev_sync *vdev_sync;
	struct hdd_adapter *adapter;
	QDF_STATUS status;
	int errno;

	QDF_BUG(rtnl_is_locked());

	errno = osif_vdev_sync_create(hdd_ctx->parent_dev, &vdev_sync);
	if (errno)
		return qdf_status_from_os_return(errno);

	adapter = hdd_open_adapter(hdd_ctx, op_mode, iface_name,
				   mac_addr_bytes, NET_NAME_UNKNOWN, true,
				   params);
	if (!adapter) {
		status = QDF_STATUS_E_INVAL;
		goto destroy_sync;
	}

	osif_vdev_sync_register(adapter->dev, vdev_sync);

	return QDF_STATUS_SUCCESS;

destroy_sync:
	osif_vdev_sync_destroy(vdev_sync);

	return status;
}

#ifdef WLAN_OPEN_P2P_INTERFACE
/**
 * hdd_open_p2p_interface - Open P2P interface
 * @hdd_ctx: HDD context
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS hdd_open_p2p_interface(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	bool p2p_dev_addr_admin;
	bool is_p2p_locally_administered = false;
	struct hdd_adapter_create_param params = {0};

	cfg_p2p_get_device_addr_admin(hdd_ctx->psoc, &p2p_dev_addr_admin);

	if (p2p_dev_addr_admin) {
		if (hdd_ctx->num_provisioned_addr &&
		    !(hdd_ctx->provisioned_mac_addr[0].bytes[0] & 0x02)) {
			hdd_ctx->p2p_device_address =
					hdd_ctx->provisioned_mac_addr[0];

			/*
			 * Generate the P2P Device Address.  This consists of
			 * the device's primary MAC address with the locally
			 * administered bit set.
			 */

			hdd_ctx->p2p_device_address.bytes[0] |= 0x02;
			is_p2p_locally_administered = true;
		} else if (!(hdd_ctx->derived_mac_addr[0].bytes[0] & 0x02)) {
			hdd_ctx->p2p_device_address =
						hdd_ctx->derived_mac_addr[0];
			/*
			 * Generate the P2P Device Address.  This consists of
			 * the device's primary MAC address with the locally
			 * administered bit set.
			 */
			hdd_ctx->p2p_device_address.bytes[0] |= 0x02;
			is_p2p_locally_administered = true;
		}
	}
	if (!is_p2p_locally_administered) {
		uint8_t *p2p_dev_addr;

		p2p_dev_addr = wlan_hdd_get_intf_addr(hdd_ctx,
						      QDF_P2P_DEVICE_MODE);
		if (!p2p_dev_addr) {
			hdd_err("Failed to get MAC address for new p2p device");
			return QDF_STATUS_E_INVAL;
		}

		qdf_mem_copy(hdd_ctx->p2p_device_address.bytes,
			     p2p_dev_addr, QDF_MAC_ADDR_SIZE);
	}

	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_P2P_DEVICE_MODE,
					   "p2p%d",
					   hdd_ctx->p2p_device_address.bytes,
					   &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		if (!is_p2p_locally_administered)
			wlan_hdd_release_intf_addr(hdd_ctx,
					hdd_ctx->p2p_device_address.bytes);
		hdd_err("Failed to open p2p interface");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS hdd_open_p2p_interface(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS hdd_open_ocb_interface(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	uint8_t *mac_addr;
	struct hdd_adapter_create_param params = {0};

	mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_OCB_MODE);
	if (!mac_addr)
		return QDF_STATUS_E_INVAL;

	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_OCB_MODE,
					   "wlanocb%d", mac_addr,
					   &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
		hdd_err("Failed to open 802.11p interface");
	}

	return status;
}

static QDF_STATUS hdd_open_concurrent_interface(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	const char *iface_name;
	uint8_t *mac_addr;
	struct hdd_adapter_create_param params = {0};

	if (qdf_str_eq(hdd_ctx->config->enable_concurrent_sta, ""))
		return QDF_STATUS_SUCCESS;

	iface_name = hdd_ctx->config->enable_concurrent_sta;
	mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_STA_MODE);
	if (!mac_addr)
		return QDF_STATUS_E_INVAL;

	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_STA_MODE,
					   iface_name, mac_addr,
					   &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
		hdd_err("Failed to open concurrent station interface");
	}

	return status;
}

static QDF_STATUS
hdd_open_adapters_for_mission_mode(struct hdd_context *hdd_ctx)
{
	enum dot11p_mode dot11p_mode;
	QDF_STATUS status;
	uint8_t *mac_addr;
	struct hdd_adapter_create_param params = {0};
	bool eht_capab = 0;

	ucfg_mlme_get_dot11p_mode(hdd_ctx->psoc, &dot11p_mode);
	ucfg_psoc_mlme_get_11be_capab(hdd_ctx->psoc, &eht_capab);

	/* Create only 802.11p interface? */
	if (dot11p_mode == CFG_11P_STANDALONE)
		return hdd_open_ocb_interface(hdd_ctx);

	mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_STA_MODE);
	if (!mac_addr)
		return QDF_STATUS_E_INVAL;

	if (eht_capab)
		params.is_ml_adapter = true;
	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_STA_MODE,
					   "wlan%d", mac_addr,
					   &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
		return status;
	}

	/* opening concurrent STA is best effort, continue on error */
	hdd_open_concurrent_interface(hdd_ctx);

	status = hdd_open_p2p_interface(hdd_ctx);
	if (status)
		goto err_close_adapters;

	/*
	 * Create separate interface (wifi-aware0) for NAN. All NAN commands
	 * should go on this new interface.
	 */
	if (wlan_hdd_is_vdev_creation_allowed(hdd_ctx->psoc)) {
		qdf_mem_zero(&params, sizeof(params));
		mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_NAN_DISC_MODE);
		if (!mac_addr)
			goto err_close_adapters;

		status = hdd_open_adapter_no_trans(hdd_ctx, QDF_NAN_DISC_MODE,
						   "wifi-aware%d", mac_addr,
						   &params);
		if (status) {
			wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
			goto err_close_adapters;
		}
	}
	/* Open 802.11p Interface */
	if (dot11p_mode == CFG_11P_CONCURRENT) {
		status = hdd_open_ocb_interface(hdd_ctx);
		if (QDF_IS_STATUS_ERROR(status))
			goto err_close_adapters;
	}

	if (eht_capab)
		hdd_wlan_register_mlo_interfaces(hdd_ctx);

	return QDF_STATUS_SUCCESS;

err_close_adapters:
	hdd_close_all_adapters(hdd_ctx, true);

	return status;
}

static QDF_STATUS hdd_open_adapters_for_ftm_mode(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	uint8_t *mac_addr;
	struct hdd_adapter_create_param params = {0};

	mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_FTM_MODE);
	if (!mac_addr)
		return QDF_STATUS_E_INVAL;

	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_FTM_MODE,
					   "wlan%d", mac_addr,
					   &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
hdd_open_adapters_for_monitor_mode(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	uint8_t *mac_addr;
	struct hdd_adapter_create_param params = {0};

	mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_MONITOR_MODE);
	if (!mac_addr)
		return QDF_STATUS_E_INVAL;

	status = hdd_open_adapter_no_trans(hdd_ctx, QDF_MONITOR_MODE,
					   "wlan%d", mac_addr,
					   &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS hdd_open_adapters_for_epping_mode(struct hdd_context *hdd_ctx)
{
	epping_enable_adapter();
	return QDF_STATUS_SUCCESS;
}

typedef QDF_STATUS (*hdd_open_mode_handler)(struct hdd_context *hdd_ctx);

static const hdd_open_mode_handler
hdd_open_mode_handlers[QDF_GLOBAL_MAX_MODE] = {
	[QDF_GLOBAL_MISSION_MODE] = hdd_open_adapters_for_mission_mode,
	[QDF_GLOBAL_FTM_MODE] = hdd_open_adapters_for_ftm_mode,
	[QDF_GLOBAL_MONITOR_MODE] = hdd_open_adapters_for_monitor_mode,
	[QDF_GLOBAL_EPPING_MODE] = hdd_open_adapters_for_epping_mode,
};

static QDF_STATUS hdd_open_adapters_for_mode(struct hdd_context *hdd_ctx,
					     enum QDF_GLOBAL_MODE driver_mode)
{
	QDF_STATUS status;

	if (driver_mode < 0 ||
	    driver_mode >= QDF_GLOBAL_MAX_MODE ||
	    !hdd_open_mode_handlers[driver_mode]) {
		hdd_err("Driver mode %d not supported", driver_mode);
		return -ENOTSUPP;
	}

	hdd_hold_rtnl_lock();
	status = hdd_open_mode_handlers[driver_mode](hdd_ctx);
	hdd_release_rtnl_lock();

	return status;
}

int hdd_wlan_startup(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	int errno;
	bool is_imps_enabled;

	hdd_enter();

	qdf_nbuf_init_replenish_timer();

	status = wlan_hdd_cache_chann_mutex_create(hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

#ifdef FEATURE_WLAN_CH_AVOID
	mutex_init(&hdd_ctx->avoid_freq_lock);
#endif

	osif_request_manager_init();
	hdd_driver_memdump_init();

	errno = hdd_init_regulatory_update_event(hdd_ctx);
	if (errno) {
		hdd_err("Failed to initialize regulatory update event; errno:%d",
			errno);
		goto memdump_deinit;
	}

	errno = hdd_wlan_start_modules(hdd_ctx, false);
	if (errno) {
		hdd_err("Failed to start modules; errno:%d", errno);
		goto memdump_deinit;
	}

	if (hdd_get_conparam() == QDF_GLOBAL_EPPING_MODE)
		return 0;

	wlan_hdd_update_wiphy(hdd_ctx);

	hdd_ctx->mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!hdd_ctx->mac_handle)
		goto stop_modules;

	errno = hdd_wiphy_init(hdd_ctx);
	if (errno) {
		hdd_err("Failed to initialize wiphy; errno:%d", errno);
		goto stop_modules;
	}

	errno = hdd_initialize_mac_address(hdd_ctx);
	if (errno) {
		hdd_err("MAC initializtion failed: %d", errno);
		goto unregister_wiphy;
	}

	errno = register_netdevice_notifier(&hdd_netdev_notifier);
	if (errno) {
		hdd_err("register_netdevice_notifier failed; errno:%d", errno);
		goto unregister_wiphy;
	}

	wlan_hdd_update_11n_mode(hdd_ctx);

	hdd_lpass_notify_wlan_version(hdd_ctx);

	status = wlansap_global_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto unregister_notifiers;

	ucfg_mlme_is_imps_enabled(hdd_ctx->psoc, &is_imps_enabled);
	hdd_set_idle_ps_config(hdd_ctx, is_imps_enabled);
	hdd_debugfs_mws_coex_info_init(hdd_ctx);
	hdd_debugfs_ini_config_init(hdd_ctx);
	wlan_hdd_debugfs_unit_test_host_create(hdd_ctx);
	wlan_hdd_create_mib_stats_lock();
	wlan_cfg80211_init_interop_issues_ap(hdd_ctx->pdev);

	hdd_exit();

	return 0;

unregister_notifiers:
	unregister_netdevice_notifier(&hdd_netdev_notifier);

unregister_wiphy:
	qdf_dp_trace_deinit();
	wiphy_unregister(hdd_ctx->wiphy);

stop_modules:
	hdd_wlan_stop_modules(hdd_ctx, false);

memdump_deinit:
	hdd_driver_memdump_deinit();
	osif_request_manager_deinit();
	qdf_nbuf_deinit_replenish_timer();

	if (cds_is_fw_down())
		hdd_err("Not setting the complete event as fw is down");
	else
		hdd_start_complete(errno);

	hdd_exit();

	return errno;
}

QDF_STATUS hdd_psoc_create_vdevs(struct hdd_context *hdd_ctx)
{
	enum QDF_GLOBAL_MODE driver_mode = hdd_get_conparam();
	QDF_STATUS status;

	status = hdd_open_adapters_for_mode(hdd_ctx, driver_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to create vdevs; status:%d", status);
		return status;
	}

	ucfg_dp_try_set_rps_cpu_mask(hdd_ctx->psoc);

	if (driver_mode != QDF_GLOBAL_FTM_MODE &&
	    driver_mode != QDF_GLOBAL_EPPING_MODE)
		hdd_psoc_idle_timer_start(hdd_ctx);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_wlan_update_target_info() - update target type info
 * @hdd_ctx: HDD context
 * @context: hif context
 *
 * Update target info received from firmware in hdd context
 * Return:None
 */

void hdd_wlan_update_target_info(struct hdd_context *hdd_ctx, void *context)
{
	struct hif_target_info *tgt_info = hif_get_target_info_handle(context);

	if (!tgt_info) {
		hdd_err("Target info is Null");
		return;
	}

	hdd_ctx->target_type = tgt_info->target_type;
}

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
QDF_STATUS hdd_md_host_evt_cb(void *ctx, struct sir_md_evt *event)
{
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx;
	struct sme_motion_det_en motion_det;

	if (!ctx || !event)
		return QDF_STATUS_E_INVAL;

	hdd_ctx = (struct hdd_context *)ctx;
	if (wlan_hdd_validate_context(hdd_ctx))
		return QDF_STATUS_E_INVAL;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, event->vdev_id);
	if (!adapter || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		hdd_err("Invalid adapter or adapter has invalid magic");
		return QDF_STATUS_E_INVAL;
	}

	/* When motion is detected, reset the motion_det_in_progress flag */
	if (event->status)
		adapter->motion_det_in_progress = false;

	hdd_debug("Motion Detection CB vdev_id=%u, status=%u",
		  event->vdev_id, event->status);

	if (adapter->motion_detection_mode) {
		motion_det.vdev_id = event->vdev_id;
		motion_det.enable = 1;
		hdd_debug("Motion Detect CB -> Enable Motion Detection again");
		sme_motion_det_enable(hdd_ctx->mac_handle, &motion_det);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_md_bl_evt_cb - Callback for Motion Detection Baseline Event
 * @ctx: HDD context
 * @event: motion detect baseline event
 *
 * Callback for Motion Detection Baseline completion
 *
 * Return: QDF_STATUS QDF_STATUS_SUCCESS on Success and
 * QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS hdd_md_bl_evt_cb(void *ctx, struct sir_md_bl_evt *event)
{
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx;

	if (!ctx || !event)
		return QDF_STATUS_E_INVAL;

	hdd_ctx = (struct hdd_context *)ctx;
	if (wlan_hdd_validate_context(hdd_ctx))
		return QDF_STATUS_E_INVAL;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, event->vdev_id);
	if (!adapter || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		hdd_err("Invalid adapter or adapter has invalid magic");
		return QDF_STATUS_E_INVAL;
	}

	hdd_debug("Motion Detection Baseline CB vdev id=%u, baseline val = %d",
		  event->vdev_id, event->bl_baseline_value);

	adapter->motion_det_baseline_value = event->bl_baseline_value;

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_MOTION_DETECTION */

/**
 * hdd_ssr_on_pagefault_cb - Callback to trigger SSR because
 * of host wake up by firmware with reason pagefault
 *
 * Return: None
 */
static void hdd_ssr_on_pagefault_cb(void)
{
	uint32_t ssr_frequency_on_pagefault;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	qdf_time_t curr_time;

	hdd_enter();

	if (!hdd_ctx)
		return;

	ssr_frequency_on_pagefault =
		ucfg_pmo_get_ssr_frequency_on_pagefault(hdd_ctx->psoc);

	curr_time = qdf_get_time_of_the_day_ms();

	if (!hdd_ctx->last_pagefault_ssr_time ||
	    (curr_time - hdd_ctx->last_pagefault_ssr_time) >=
					ssr_frequency_on_pagefault) {
		hdd_info("curr_time %lu last_pagefault_ssr_time %lu ssr_frequency %d",
			 curr_time, hdd_ctx->last_pagefault_ssr_time,
			 ssr_frequency_on_pagefault);
		hdd_ctx->last_pagefault_ssr_time = curr_time;
		cds_trigger_recovery(QDF_HOST_WAKEUP_REASON_PAGEFAULT);
	}
}

/**
 * hdd_register_cb - Register HDD callbacks.
 * @hdd_ctx: HDD context
 *
 * Register the HDD callbacks to CDS/SME.
 *
 * Return: 0 for success or Error code for failure
 */
int hdd_register_cb(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	int ret = 0;
	mac_handle_t mac_handle;

	hdd_enter();
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("in ftm mode, no need to register callbacks");
		return ret;
	}

	mac_handle = hdd_ctx->mac_handle;

	sme_register_oem_data_rsp_callback(mac_handle,
					   hdd_send_oem_data_rsp_msg);

	sme_register_mgmt_frame_ind_callback(mac_handle,
					     hdd_indicate_mgmt_frame);
	sme_set_tsfcb(mac_handle, hdd_get_tsf_cb, hdd_ctx);
	sme_stats_ext_register_callback(mac_handle,
					wlan_hdd_cfg80211_stats_ext_callback);

	sme_ext_scan_register_callback(mac_handle,
					wlan_hdd_cfg80211_extscan_callback);
	sme_stats_ext2_register_callback(mac_handle,
					wlan_hdd_cfg80211_stats_ext2_callback);

	sme_roam_events_register_callback(mac_handle,
					wlan_hdd_cfg80211_roam_events_callback);

	sme_multi_client_ll_rsp_register_callback(mac_handle,
					hdd_latency_level_event_handler_cb);

	sme_set_rssi_threshold_breached_cb(mac_handle,
					   hdd_rssi_threshold_breached);

	sme_set_link_layer_stats_ind_cb(mac_handle,
				wlan_hdd_cfg80211_link_layer_stats_callback);

	sme_rso_cmd_status_cb(mac_handle, wlan_hdd_rso_cmd_status_cb);

	sme_set_link_layer_ext_cb(mac_handle,
			wlan_hdd_cfg80211_link_layer_stats_ext_callback);
	sme_update_hidden_ssid_status_cb(mac_handle,
					 hdd_hidden_ssid_enable_roaming);

	status = sme_set_lost_link_info_cb(mac_handle,
					   hdd_lost_link_info_cb);

	wlan_hdd_register_cp_stats_cb(hdd_ctx);
	hdd_dcs_register_cb(hdd_ctx);

	hdd_thermal_register_callbacks(hdd_ctx);
	/* print error and not block the startup process */
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("set lost link info callback failed");

	ret = hdd_register_data_stall_detect_cb();
	if (ret) {
		hdd_err("Register data stall detect detect callback failed.");
		return ret;
	}

	wlan_hdd_dcc_register_for_dcc_stats_event(hdd_ctx);

	sme_register_set_connection_info_cb(mac_handle,
					    hdd_set_connection_in_progress,
					    hdd_is_connection_in_progress);

	status = sme_set_bt_activity_info_cb(mac_handle,
					     hdd_bt_activity_cb);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("set bt activity info callback failed");

	status = sme_register_tx_queue_cb(mac_handle,
					  hdd_tx_queue_cb);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Register tx queue callback failed");

#ifdef WLAN_FEATURE_MOTION_DETECTION
	sme_set_md_host_evt_cb(mac_handle, hdd_md_host_evt_cb, (void *)hdd_ctx);
	sme_set_md_bl_evt_cb(mac_handle, hdd_md_bl_evt_cb, (void *)hdd_ctx);
#endif /* WLAN_FEATURE_MOTION_DETECTION */

	mac_register_session_open_close_cb(hdd_ctx->mac_handle,
					   hdd_sme_close_session_callback,
					   hdd_common_roam_callback);

	sme_set_roam_scan_ch_event_cb(mac_handle, hdd_get_roam_scan_ch_cb);
	status = sme_set_monitor_mode_cb(mac_handle,
					 hdd_sme_monitor_mode_callback);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err_rl("Register monitor mode callback failed");

	status = sme_set_beacon_latency_event_cb(mac_handle,
						 hdd_beacon_latency_event_cb);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err_rl("Register beacon latency event callback failed");

	sme_async_oem_event_init(mac_handle,
				 hdd_oem_event_async_cb);

	sme_register_ssr_on_pagefault_cb(mac_handle, hdd_ssr_on_pagefault_cb);

	hdd_exit();

	return ret;
}

/**
 * hdd_deregister_cb() - De-Register HDD callbacks.
 * @hdd_ctx: HDD context
 *
 * De-Register the HDD callbacks to CDS/SME.
 *
 * Return: void
 */
void hdd_deregister_cb(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	int ret;
	mac_handle_t mac_handle;

	hdd_enter();
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("in ftm mode, no need to deregister callbacks");
		return;
	}

	mac_handle = hdd_ctx->mac_handle;

	sme_deregister_ssr_on_pagefault_cb(mac_handle);

	sme_async_oem_event_deinit(mac_handle);

	sme_deregister_tx_queue_cb(mac_handle);

	sme_reset_link_layer_stats_ind_cb(mac_handle);
	sme_reset_rssi_threshold_breached_cb(mac_handle);

	sme_stats_ext_deregister_callback(mac_handle);

	status = sme_reset_tsfcb(mac_handle);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to de-register tsfcb the callback:%d",
			status);

	ret = hdd_deregister_data_stall_detect_cb();
	if (ret)
		hdd_err("Failed to de-register data stall detect event callback");
	hdd_thermal_unregister_callbacks(hdd_ctx);
	sme_deregister_oem_data_rsp_callback(mac_handle);
	sme_roam_events_deregister_callback(mac_handle);
	sme_multi_client_ll_rsp_deregister_callback(mac_handle);

	hdd_exit();
}

/**
 * hdd_softap_sta_deauth() - handle deauth req from HDD
 * @adapter: Pointer to the HDD adapter
 * @param: Params to the operation
 *
 * This to take counter measure to handle deauth req from HDD
 *
 * Return: None
 */
QDF_STATUS hdd_softap_sta_deauth(struct hdd_adapter *adapter,
				 struct csr_del_sta_params *param)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAULT;
	struct hdd_context *hdd_ctx;
	bool is_sap_bcast_deauth_enabled = false;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	ucfg_mlme_get_sap_bcast_deauth_enabled(hdd_ctx->psoc,
					       &is_sap_bcast_deauth_enabled);

	hdd_enter();

	hdd_debug("sap_bcast_deauth_enabled %d", is_sap_bcast_deauth_enabled);
	/* Ignore request to deauth bcmc station */
	if (!is_sap_bcast_deauth_enabled)
		if (param->peerMacAddr.bytes[0] & 0x1)
			return qdf_status;

	qdf_status =
		wlansap_deauth_sta(WLAN_HDD_GET_SAP_CTX_PTR(adapter),
				   param);

	hdd_exit();
	return qdf_status;
}

/**
 * hdd_softap_sta_disassoc() - take counter measure to handle deauth req from HDD
 * @adapter: Pointer to the HDD
 * @param: pointer to station deletion parameters
 *
 * This to take counter measure to handle deauth req from HDD
 *
 * Return: None
 */
void hdd_softap_sta_disassoc(struct hdd_adapter *adapter,
			     struct csr_del_sta_params *param)
{
	hdd_enter();

	/* Ignore request to disassoc bcmc station */
	if (param->peerMacAddr.bytes[0] & 0x1)
		return;

	wlansap_disassoc_sta(WLAN_HDD_GET_SAP_CTX_PTR(adapter),
			     param);
}

void
wlan_hdd_disable_roaming(struct hdd_adapter *cur_adapter,
			 enum wlan_cm_rso_control_requestor rso_op_requestor)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(cur_adapter);
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	struct hdd_station_ctx *sta_ctx;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_DISABLE_ROAMING;

	if (!policy_mgr_is_sta_active_connection_exists(hdd_ctx->psoc))
		return;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

		if (cur_adapter->vdev_id != adapter->vdev_id &&
		    adapter->device_mode == QDF_STA_MODE &&
		    hdd_cm_is_vdev_associated(adapter)) {
			hdd_debug("%d Disable roaming", adapter->vdev_id);
			sme_stop_roaming(hdd_ctx->mac_handle,
					 adapter->vdev_id,
					 REASON_DRIVER_DISABLED,
					 rso_op_requestor);
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

void
wlan_hdd_enable_roaming(struct hdd_adapter *cur_adapter,
			enum wlan_cm_rso_control_requestor rso_op_requestor)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(cur_adapter);
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	struct hdd_station_ctx *sta_ctx;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_ENABLE_ROAMING;

	if (!policy_mgr_is_sta_active_connection_exists(hdd_ctx->psoc))
		return;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

		if (cur_adapter->vdev_id != adapter->vdev_id &&
		    adapter->device_mode == QDF_STA_MODE &&
		    hdd_cm_is_vdev_associated(adapter)) {
			hdd_debug("%d Enable roaming", adapter->vdev_id);
			sme_start_roaming(hdd_ctx->mac_handle,
					  adapter->vdev_id,
					  REASON_DRIVER_ENABLED,
					  rso_op_requestor);
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

/**
 * nl_srv_bcast_svc() - Wrapper function to send bcast msgs to SVC mcast group
 * @skb: sk buffer pointer
 *
 * Sends the bcast message to SVC multicast group with generic nl socket
 * if CNSS_GENL is enabled. Else, use the legacy netlink socket to send.
 *
 * Return: None
 */
static void nl_srv_bcast_svc(struct sk_buff *skb)
{
#ifdef CNSS_GENL
	nl_srv_bcast(skb, CLD80211_MCGRP_SVC_MSGS, WLAN_NL_MSG_SVC);
#else
	nl_srv_bcast(skb);
#endif
}

void wlan_hdd_send_svc_nlink_msg(int radio, int type, void *data, int len)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	tAniMsgHdr *ani_hdr;
	void *nl_data = NULL;
	int flags = GFP_KERNEL;
	struct radio_index_tlv *radio_info;
	int tlv_len;

	if (in_interrupt() || irqs_disabled() || in_atomic())
		flags = GFP_ATOMIC;

	skb = alloc_skb(NLMSG_SPACE(WLAN_NL_MAX_PAYLOAD), flags);

	if (!skb)
		return;

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;     /* from kernel */
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_SVC;

	ani_hdr = NLMSG_DATA(nlh);
	ani_hdr->type = type;

	switch (type) {
	case WLAN_SVC_FW_CRASHED_IND:
	case WLAN_SVC_FW_SHUTDOWN_IND:
	case WLAN_SVC_LTE_COEX_IND:
	case WLAN_SVC_WLAN_AUTO_SHUTDOWN_IND:
	case WLAN_SVC_WLAN_AUTO_SHUTDOWN_CANCEL_IND:
		ani_hdr->length = 0;
		nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr)));
		break;
	case WLAN_SVC_WLAN_STATUS_IND:
	case WLAN_SVC_WLAN_VERSION_IND:
	case WLAN_SVC_DFS_CAC_START_IND:
	case WLAN_SVC_DFS_CAC_END_IND:
	case WLAN_SVC_DFS_RADAR_DETECT_IND:
	case WLAN_SVC_DFS_ALL_CHANNEL_UNAVAIL_IND:
	case WLAN_SVC_WLAN_TP_IND:
	case WLAN_SVC_WLAN_TP_TX_IND:
	case WLAN_SVC_RPS_ENABLE_IND:
	case WLAN_SVC_CORE_MINFREQ:
		ani_hdr->length = len;
		nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + len));
		nl_data = (char *)ani_hdr + sizeof(tAniMsgHdr);
		memcpy(nl_data, data, len);
		break;

	default:
		hdd_err("WLAN SVC: Attempt to send unknown nlink message %d",
		       type);
		kfree_skb(skb);
		return;
	}

	/*
	 * Add radio index at the end of the svc event in TLV format
	 * to maintain the backward compatibility with userspace
	 * applications.
	 */

	tlv_len = 0;

	if ((sizeof(*ani_hdr) + len + sizeof(struct radio_index_tlv))
		< WLAN_NL_MAX_PAYLOAD) {
		radio_info  = (struct radio_index_tlv *)((char *) ani_hdr +
		sizeof(*ani_hdr) + len);
		radio_info->type = (unsigned short) WLAN_SVC_WLAN_RADIO_INDEX;
		radio_info->length = (unsigned short) sizeof(radio_info->radio);
		radio_info->radio = radio;
		tlv_len = sizeof(*radio_info);
		hdd_debug("Added radio index tlv - radio index %d",
			  radio_info->radio);
	}

	nlh->nlmsg_len += tlv_len;
	skb_put(skb, NLMSG_SPACE(sizeof(tAniMsgHdr) + len + tlv_len));

	nl_srv_bcast_svc(skb);
}

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
void wlan_hdd_auto_shutdown_cb(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return;

	hdd_debug("Wlan Idle. Sending Shutdown event..");
	wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
			WLAN_SVC_WLAN_AUTO_SHUTDOWN_IND, NULL, 0);
}

void wlan_hdd_auto_shutdown_enable(struct hdd_context *hdd_ctx, bool enable)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	bool ap_connected = false, sta_connected = false;
	mac_handle_t mac_handle;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_AUTO_SHUTDOWN_ENABLE;

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle)
		return;

	if (hdd_ctx->config->wlan_auto_shutdown == 0)
		return;

	if (enable == false) {
		if (sme_set_auto_shutdown_timer(mac_handle, 0) !=
							QDF_STATUS_SUCCESS) {
			hdd_err("Failed to stop wlan auto shutdown timer");
		}
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
			WLAN_SVC_WLAN_AUTO_SHUTDOWN_CANCEL_IND, NULL, 0);
		return;
	}

	/* To enable shutdown timer check conncurrency */
	if (policy_mgr_concurrent_open_sessions_running(hdd_ctx->psoc)) {
		hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter,
						   next_adapter, dbgid) {
			if (adapter->device_mode == QDF_STA_MODE) {
				if (hdd_cm_is_vdev_associated(adapter)) {
					sta_connected = true;
					hdd_adapter_dev_put_debug(adapter,
								  dbgid);
					if (next_adapter)
						hdd_adapter_dev_put_debug(
								next_adapter,
								dbgid);
					break;
				}
			}

			if (adapter->device_mode == QDF_SAP_MODE) {
				if (WLAN_HDD_GET_AP_CTX_PTR(adapter)->
				    ap_active == true) {
					ap_connected = true;
					hdd_adapter_dev_put_debug(adapter,
								  dbgid);
					if (next_adapter)
						hdd_adapter_dev_put_debug(
								next_adapter,
								dbgid);
					break;
				}
			}
			hdd_adapter_dev_put_debug(adapter, dbgid);
		}
	}

	if (ap_connected == true || sta_connected == true) {
		hdd_debug("CC Session active. Shutdown timer not enabled");
		return;
	}

	if (sme_set_auto_shutdown_timer(mac_handle,
					hdd_ctx->config->wlan_auto_shutdown)
	    != QDF_STATUS_SUCCESS)
		hdd_err("Failed to start wlan auto shutdown timer");
	else
		hdd_info("Auto Shutdown timer for %d seconds enabled",
			 hdd_ctx->config->wlan_auto_shutdown);
}
#endif

struct hdd_adapter *
hdd_get_con_sap_adapter(struct hdd_adapter *this_sap_adapter,
			bool check_start_bss)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(this_sap_adapter);
	struct hdd_adapter *adapter, *con_sap_adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_CON_SAP_ADAPTER;

	con_sap_adapter = NULL;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter && ((adapter->device_mode == QDF_SAP_MODE) ||
				(adapter->device_mode == QDF_P2P_GO_MODE)) &&
						adapter != this_sap_adapter) {
			if (check_start_bss) {
				if (test_bit(SOFTAP_BSS_STARTED,
						&adapter->event_flags)) {
					con_sap_adapter = adapter;
					hdd_adapter_dev_put_debug(adapter,
								  dbgid);
					if (next_adapter)
						hdd_adapter_dev_put_debug(
								next_adapter,
								dbgid);
					break;
				}
			} else {
				con_sap_adapter = adapter;
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				break;
			}
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return con_sap_adapter;
}

static inline bool hdd_adapter_is_sta(struct hdd_adapter *adapter)
{
	return adapter->device_mode == QDF_STA_MODE ||
		adapter->device_mode == QDF_P2P_CLIENT_MODE;
}

bool hdd_is_any_adapter_connected(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IS_ANY_ADAPTER_CONNECTED;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (hdd_adapter_is_sta(adapter) &&
		    hdd_cm_is_vdev_associated(adapter)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return true;
		}

		if (hdd_adapter_is_ap(adapter) &&
		    WLAN_HDD_GET_AP_CTX_PTR(adapter)->ap_active) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return true;
		}

		if (adapter->device_mode == QDF_NDI_MODE &&
		    WLAN_HDD_GET_STATION_CTX_PTR(adapter)->
		    conn_info.conn_state == eConnectionState_NdiConnected) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return true;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return false;
}

#if defined MSM_PLATFORM && (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 19, 0))
/**
 * hdd_inform_stop_sap() - call cfg80211 API to stop SAP
 * @adapter: pointer to adapter
 *
 * This function calls cfg80211 API to stop SAP
 *
 * Return: None
 */
static void hdd_inform_stop_sap(struct hdd_adapter *adapter)
{
	hdd_debug("SAP stopped due to invalid channel vdev id %d",
		  wlan_vdev_get_id(adapter->vdev));
	cfg80211_ap_stopped(adapter->dev, GFP_KERNEL);
}

#else
static void hdd_inform_stop_sap(struct hdd_adapter *adapter)
{
	hdd_debug("SAP stopped due to invalid channel vdev id %d",
		  wlan_vdev_get_id(adapter->vdev));
	cfg80211_stop_iface(adapter->hdd_ctx->wiphy, &adapter->wdev,
			    GFP_KERNEL);
}
#endif

/**
 * wlan_hdd_stop_sap() - This function stops bss of SAP.
 * @ap_adapter: SAP adapter
 *
 * This function will process the stopping of sap adapter.
 *
 * Return: None
 */
void wlan_hdd_stop_sap(struct hdd_adapter *ap_adapter)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct hdd_hostapd_state *hostapd_state;
	QDF_STATUS qdf_status;
	struct hdd_context *hdd_ctx;

	if (!ap_adapter) {
		hdd_err("ap_adapter is NULL here");
		return;
	}

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	mutex_lock(&hdd_ctx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		wlan_hdd_del_station(ap_adapter, NULL);
		hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
		hdd_debug("Now doing SAP STOPBSS");
		qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
		if (QDF_STATUS_SUCCESS == wlansap_stop_bss(hdd_ap_ctx->
							sap_context)) {
			qdf_status = qdf_wait_single_event(&hostapd_state->
					qdf_stop_bss_event,
					SME_CMD_STOP_BSS_TIMEOUT);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				mutex_unlock(&hdd_ctx->sap_lock);
				hdd_err("SAP Stop Failed");
				return;
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
						ap_adapter->device_mode,
						ap_adapter->vdev_id);
		hdd_green_ap_start_state_mc(hdd_ctx, ap_adapter->device_mode,
					    false);
		hdd_inform_stop_sap(ap_adapter);
		hdd_debug("SAP Stop Success");
	} else {
		hdd_err("Can't stop ap because its not started");
	}
	mutex_unlock(&hdd_ctx->sap_lock);
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
/**
 * wlan_hdd_mlo_sap_reinit() - handle mlo scenario for ssr
 * @hdd_ctx: Pointer to hdd context
 * @config: Pointer to sap config
 * @adapter: Pointer to hostapd adapter
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_hdd_mlo_sap_reinit(struct hdd_context *hdd_ctx,
					  struct sap_config *config,
					  struct hdd_adapter *adapter)
{
	if (config->mlo_sap) {
		if (!mlo_ap_vdev_attach(adapter->vdev, config->link_id,
					config->num_link)) {
			hdd_err("SAP mlo mgr attach fail");
			return QDF_STATUS_E_INVAL;
		}
	}

	if (!policy_mgr_is_mlo_sap_concurrency_allowed(
		hdd_ctx->psoc, wlan_vdev_mlme_is_mlo_ap(adapter->vdev))) {
		hdd_err("MLO SAP concurrency check fails");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS wlan_hdd_mlo_sap_reinit(struct hdd_context *hdd_ctx,
					  struct sap_config *config,
					  struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}
#endif
/**
 * wlan_hdd_start_sap() - this function starts bss of SAP.
 * @ap_adapter: SAP adapter
 * @reinit: true if this is a re-init, otherwise initial int
 *
 * This function will process the starting of sap adapter.
 *
 * Return: None
 */
void wlan_hdd_start_sap(struct hdd_adapter *ap_adapter, bool reinit)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct hdd_hostapd_state *hostapd_state;
	QDF_STATUS qdf_status;
	struct hdd_context *hdd_ctx;
	struct sap_config *sap_config;

	if (!ap_adapter) {
		hdd_err("ap_adapter is NULL here");
		return;
	}

	if (QDF_SAP_MODE != ap_adapter->device_mode) {
		hdd_err("SoftAp role has not been enabled");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
	sap_config = &ap_adapter->session.ap.sap_config;

	mutex_lock(&hdd_ctx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags))
		goto end;

	if (0 != wlan_hdd_cfg80211_update_apies(ap_adapter)) {
		hdd_err("SAP Not able to set AP IEs");
		goto end;
	}
	wlan_reg_set_channel_params_for_pwrmode(
				hdd_ctx->pdev,
				hdd_ap_ctx->sap_config.chan_freq,
				0, &hdd_ap_ctx->sap_config.ch_params,
				REG_CURRENT_PWR_MODE);
	if (QDF_IS_STATUS_ERROR(wlan_hdd_mlo_sap_reinit(hdd_ctx, sap_config,
							ap_adapter))) {
		hdd_err("SAP Not able to do mlo attach");
		goto end;
	}

	qdf_event_reset(&hostapd_state->qdf_event);
	if (wlansap_start_bss(hdd_ap_ctx->sap_context, hdd_hostapd_sap_event_cb,
			      &hdd_ap_ctx->sap_config,
			      ap_adapter->dev)
			      != QDF_STATUS_SUCCESS)
		goto end;

	hdd_debug("Waiting for SAP to start");
	qdf_status = qdf_wait_single_event(&hostapd_state->qdf_event,
					SME_CMD_START_BSS_TIMEOUT);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("SAP Start failed");
		goto end;
	}
	hdd_info("SAP Start Success");

	if (reinit)
		hdd_medium_assess_init();
	wlansap_reset_sap_config_add_ie(sap_config, eUPDATE_IE_ALL);
	set_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
	if (hostapd_state->bss_state == BSS_START) {
		policy_mgr_incr_active_session(hdd_ctx->psoc,
					ap_adapter->device_mode,
					ap_adapter->vdev_id);
		hdd_green_ap_start_state_mc(hdd_ctx, ap_adapter->device_mode,
					    true);
	}
	mutex_unlock(&hdd_ctx->sap_lock);

	return;
end:
	wlan_hdd_mlo_reset(ap_adapter);
	wlansap_reset_sap_config_add_ie(sap_config, eUPDATE_IE_ALL);
	mutex_unlock(&hdd_ctx->sap_lock);
	/* SAP context and beacon cleanup will happen during driver unload
	 * in hdd_stop_adapter
	 */
	hdd_err("SAP restart after SSR failed! Reload WLAN and try SAP again");
	/* Free the beacon memory in case of failure in the sap restart */
	if (ap_adapter->session.ap.beacon) {
		qdf_mem_free(ap_adapter->session.ap.beacon);
		ap_adapter->session.ap.beacon = NULL;
	}
}

#ifdef QCA_CONFIG_SMP
/**
 * wlan_hdd_get_cpu() - get cpu_index
 *
 * Return: cpu_index
 */
int wlan_hdd_get_cpu(void)
{
	int cpu_index = get_cpu();

	put_cpu();
	return cpu_index;
}
#endif

/**
 * hdd_get_fwpath() - get framework path
 *
 * This function is used to get the string written by
 * userspace to start the wlan driver
 *
 * Return: string
 */
const char *hdd_get_fwpath(void)
{
	return fwpath.string;
}

static inline int hdd_state_query_cb(void)
{
	return !!wlan_hdd_validate_context(cds_get_context(QDF_MODULE_ID_HDD));
}

static int __hdd_op_protect_cb(void **out_sync, const char *func)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return -EAGAIN;

	return __osif_psoc_sync_op_start(hdd_ctx->parent_dev,
					 (struct osif_psoc_sync **)out_sync,
					 func);
}

static void __hdd_op_unprotect_cb(void *sync, const char *func)
{
	__osif_psoc_sync_op_stop(sync, func);
}

/**
 * hdd_init() - Initialize Driver
 *
 * This function initializes CDS global context with the help of cds_init. This
 * has to be the first function called after probe to get a valid global
 * context.
 *
 * Return: 0 for success, errno on failure
 */
int hdd_init(void)
{
	QDF_STATUS status;

	status = cds_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to allocate CDS context");
		return -ENOMEM;
	}

	qdf_op_callbacks_register(__hdd_op_protect_cb, __hdd_op_unprotect_cb);

	wlan_init_bug_report_lock();

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
	wlan_logging_sock_init_svc();
#endif

	hdd_trace_init();
	hdd_register_debug_callback();
	wlan_roam_debug_init();

	return 0;
}

/**
 * hdd_deinit() - Deinitialize Driver
 *
 * This function frees CDS global context with the help of cds_deinit. This
 * has to be the last function call in remove callback to free the global
 * context.
 */
void hdd_deinit(void)
{
	wlan_roam_debug_deinit();

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
	wlan_logging_sock_deinit_svc();
#endif

	wlan_destroy_bug_report_lock();
	qdf_op_callbacks_register(NULL, NULL);
	cds_deinit();
}

#ifdef QCA_WIFI_EMULATION
#define HDD_WLAN_START_WAIT_TIME ((CDS_WMA_TIMEOUT + 5000) * 100)
#else
#define HDD_WLAN_START_WAIT_TIME (CDS_WMA_TIMEOUT + 5000)
#endif

void hdd_init_start_completion(void)
{
	INIT_COMPLETION(wlan_start_comp);
}

#ifdef WLAN_CTRL_NAME
static unsigned int dev_num = 1;
static struct cdev wlan_hdd_state_cdev;
static struct class *class;
static dev_t device;

static void hdd_set_adapter_wlm_def_level(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_GET_ADAPTER;
	int ret;
	QDF_STATUS qdf_status;
	uint8_t latency_level;
	bool reset;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam())
		return;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		qdf_status = ucfg_mlme_cfg_get_wlm_level(hdd_ctx->psoc,
							 &latency_level);
		if (QDF_IS_STATUS_ERROR(qdf_status))
			adapter->latency_level =
			       QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_NORMAL;
		else
			adapter->latency_level = latency_level;
		qdf_status = ucfg_mlme_cfg_get_wlm_reset(hdd_ctx->psoc, &reset);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			hdd_err("could not get the wlm reset flag");
			reset = false;
		}

		if (hdd_get_multi_client_ll_support(adapter) && !reset)
			wlan_hdd_deinit_multi_client_info_table(adapter);

		adapter->upgrade_udp_qos_threshold = QCA_WLAN_AC_BK;
		hdd_debug("UDP packets qos reset to: %d",
			  adapter->upgrade_udp_qos_threshold);
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

static int wlan_hdd_state_ctrl_param_open(struct inode *inode,
					  struct file *file)
{
	qdf_atomic_inc(&wlan_hdd_state_fops_ref);

	return 0;
}

static void __hdd_inform_wifi_off(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return;

	ucfg_dlm_wifi_off(hdd_ctx->pdev);
}

static void hdd_inform_wifi_off(void)
{
	int ret;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct osif_psoc_sync *psoc_sync;

	if (!hdd_ctx)
		return;

	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return;

	hdd_set_adapter_wlm_def_level(hdd_ctx);
	__hdd_inform_wifi_off();

	osif_psoc_sync_op_stop(psoc_sync);
}

#if defined CFG80211_USER_HINT_CELL_BASE_SELF_MANAGED || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0))
static void hdd_inform_wifi_on(void)
{
	int ret;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct osif_psoc_sync *psoc_sync;

	hdd_nofl_debug("inform regdomain for wifi on");
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return;
	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return;
	if (!hdd_ctx->wiphy)
		return;
	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return;
	if (hdd_ctx->wiphy->registered)
		hdd_send_wiphy_regd_sync_event(hdd_ctx);

	osif_psoc_sync_op_stop(psoc_sync);
}
#else
static void hdd_inform_wifi_on(void)
{
}
#endif

static int hdd_validate_wlan_string(const char __user *user_buf)
{
	char buf[15];
	int i;
	static const char * const wlan_str[] = {
		[WLAN_OFF_STR] = "OFF",
		[WLAN_ON_STR] = "ON",
		[WLAN_ENABLE_STR] = "ENABLE",
		[WLAN_DISABLE_STR] = "DISABLE",
		[WLAN_WAIT_FOR_READY_STR] = "WAIT_FOR_READY",
		[WLAN_FORCE_DISABLE_STR] = "FORCE_DISABLE"
	};

	if (copy_from_user(buf, user_buf, sizeof(buf))) {
		pr_err("Failed to read buffer\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(wlan_str); i++) {
		if (qdf_str_ncmp(buf, wlan_str[i], strlen(wlan_str[i])) == 0)
			return i;
	}

	return -EINVAL;
}

#ifdef FEATURE_CNSS_HW_SECURE_DISABLE
#define WIFI_DISABLE_SLEEP (10)
#define WIFI_DISABLE_MAX_RETRY_ATTEMPTS (10)
static bool g_soft_unload;

bool hdd_get_wlan_driver_status(void)
{
	return g_soft_unload;
}

static int hdd_wlan_soft_driver_load(void)
{
	if (!cds_is_driver_loaded()) {
		hdd_debug("\nEnabling CNSS WLAN HW");
		pld_wlan_hw_enable();
		return 0;
	}

	if (!g_soft_unload) {
		hdd_debug_rl("Enabling WiFi\n");
		return -EINVAL;
	}

	hdd_driver_load();
	g_soft_unload = false;
	return 0;
}

static void hdd_wlan_soft_driver_unload(void)
{
	if (g_soft_unload) {
		hdd_debug_rl("WiFi is already disabled");
		return;
	}
	hdd_debug("Initiating soft driver unload\n");
	g_soft_unload = true;
	hdd_driver_unload();
}

static int hdd_wlan_idle_shutdown(struct hdd_context *hdd_ctx)
{
	int ret;
	int retries = 0;
	void *hif_ctx;

	if (!hdd_ctx) {
		hdd_err_rl("hdd_ctx is Null");
		return -EINVAL;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);

	hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_IDLE_SHUTDOWN);

	while (retries < WIFI_DISABLE_MAX_RETRY_ATTEMPTS) {
		if (hif_ctx) {
			/*
			 * Trigger runtime sync resume before psoc_idle_shutdown
			 * such that resume can happen successfully
			 */
			qdf_rtpm_sync_resume();
		}
		ret = pld_idle_shutdown(hdd_ctx->parent_dev,
					hdd_psoc_idle_shutdown);

		if (-EAGAIN == ret || hdd_ctx->is_wiphy_suspended) {
			hdd_debug("System suspend in progress.Retries done:%d",
				  retries);
			msleep(WIFI_DISABLE_SLEEP);
			retries++;
			continue;
		}
		break;
	}
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_IDLE_SHUTDOWN);

	if (retries > WIFI_DISABLE_MAX_RETRY_ATTEMPTS) {
		hdd_debug("Max retries reached");
		return -EINVAL;
	}
	hdd_debug_rl("WiFi is disabled");

	return 0;
}

static int hdd_disable_wifi(struct hdd_context *hdd_ctx)
{
	int ret;

	if (hdd_ctx->is_wlan_disabled) {
		hdd_err_rl("Wifi is already disabled");
		return 0;
	}

	hdd_debug("Initiating WLAN idle shutdown");
	if (hdd_is_any_interface_open(hdd_ctx)) {
		hdd_err("Interfaces still open, cannot process wifi disable");
		return -EAGAIN;
	}

	hdd_ctx->is_wlan_disabled = true;

	ret = hdd_wlan_idle_shutdown(hdd_ctx);
	if (ret)
		hdd_ctx->is_wlan_disabled = false;

	return ret;
}
#else
static int hdd_wlan_soft_driver_load(void)
{
	return -EINVAL;
}

static void hdd_wlan_soft_driver_unload(void)
{
}

static int hdd_disable_wifi(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif /* FEATURE_CNSS_HW_SECURE_DISABLE */

static ssize_t wlan_hdd_state_ctrl_param_write(struct file *filp,
						const char __user *user_buf,
						size_t count,
						loff_t *f_pos)
{
	int id, ret;
	unsigned long rc;
	struct hdd_context *hdd_ctx;
	bool is_wait_for_ready = false;
	bool is_wlan_force_disabled;

	hdd_enter();

	id = hdd_validate_wlan_string(user_buf);

	switch (id) {
	case WLAN_OFF_STR:
		hdd_info("Wifi turning off from UI\n");
		hdd_inform_wifi_off();
		goto exit;
	case WLAN_ON_STR:
		hdd_info("Wifi Turning On from UI\n");
		break;
	case WLAN_WAIT_FOR_READY_STR:
		is_wait_for_ready = true;
		hdd_info("Wifi wait for ready from UI\n");
		break;
	case WLAN_ENABLE_STR:
		hdd_nofl_debug("Received WiFi enable from framework\n");
		if (!hdd_wlan_soft_driver_load())
			goto exit;
		pr_info("Enabling WiFi\n");
		break;
	case WLAN_DISABLE_STR:
		hdd_nofl_debug("Received WiFi disable from framework\n");
		if (!cds_is_driver_loaded())
			goto exit;

		is_wlan_force_disabled = hdd_get_wlan_driver_status();
		if (is_wlan_force_disabled)
			goto exit;
		pr_info("Disabling WiFi\n");
		break;
	case WLAN_FORCE_DISABLE_STR:
		hdd_nofl_debug("Received Force WiFi disable from framework\n");
		if (!cds_is_driver_loaded())
			goto exit;

		hdd_wlan_soft_driver_unload();
		goto exit;
	default:
		hdd_err_rl("Invalid value received from framework");
		return -EINVAL;
	}

	hdd_info("is_driver_loaded %d is_driver_recovering %d",
		 cds_is_driver_loaded(), cds_is_driver_recovering());

	if (!cds_is_driver_loaded() || cds_is_driver_recovering()) {
		rc = wait_for_completion_timeout(&wlan_start_comp,
				msecs_to_jiffies(HDD_WLAN_START_WAIT_TIME));
		if (!rc) {
			hdd_err("Driver Loading Timed-out!!");
			ret = -EINVAL;
			return ret;
		}
	}

	if (is_wait_for_ready)
		return count;
	/*
	 * Flush idle shutdown work for cases to synchronize the wifi on
	 * during the idle shutdown.
	 */
	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (hdd_ctx)
		hdd_psoc_idle_timer_stop(hdd_ctx);

	if (id == WLAN_DISABLE_STR) {
		if (!hdd_ctx) {
			hdd_err_rl("hdd_ctx is Null");
			goto exit;
		}

		ret = hdd_disable_wifi(hdd_ctx);
		if (ret)
			return ret;
	}

	if (id == WLAN_ENABLE_STR) {
		if (!hdd_ctx) {
			hdd_err_rl("hdd_ctx is Null");
			goto exit;
		}

		if (!hdd_ctx->is_wlan_disabled) {
			hdd_err_rl("WiFi is already enabled");
			goto exit;
		}
		hdd_ctx->is_wlan_disabled = false;
	}

	if (id == WLAN_ON_STR)
		hdd_inform_wifi_on();
exit:
	hdd_exit();
	return count;
}

/**
 * wlan_hdd_state_ctrl_param_release() -  Release callback for /dev/wlan.
 *
 * @inode: struct inode pinter.
 * @file: struct file pointer.
 *
 * Release callback that would be invoked when the file operations has
 * completed fully. This is implemented to provide a reference count mechanism
 * via which the driver can wait till all possible usage of the /dev/wlan
 * file is completed.
 *
 * Return: Success
 */
static int wlan_hdd_state_ctrl_param_release(struct inode *inode,
					     struct file *file)
{
	qdf_atomic_dec(&wlan_hdd_state_fops_ref);

	return 0;
}

const struct file_operations wlan_hdd_state_fops = {
	.owner = THIS_MODULE,
	.open = wlan_hdd_state_ctrl_param_open,
	.write = wlan_hdd_state_ctrl_param_write,
	.release = wlan_hdd_state_ctrl_param_release,
};

static int  wlan_hdd_state_ctrl_param_create(void)
{
	unsigned int wlan_hdd_state_major = 0;
	int ret;
	struct device *dev;

	init_completion(&wlan_start_comp);
	qdf_atomic_init(&wlan_hdd_state_fops_ref);

	device = MKDEV(wlan_hdd_state_major, 0);

	ret = alloc_chrdev_region(&device, 0, dev_num, "qcwlanstate");
	if (ret) {
		pr_err("Failed to register qcwlanstate");
		goto dev_alloc_err;
	}
	wlan_hdd_state_major = MAJOR(device);

	class = class_create(THIS_MODULE, WLAN_CTRL_NAME);
	if (IS_ERR(class)) {
		pr_err("wlan_hdd_state class_create error");
		goto class_err;
	}

	dev = device_create(class, NULL, device, NULL, WLAN_CTRL_NAME);
	if (IS_ERR(dev)) {
		pr_err("wlan_hdd_statedevice_create error");
		goto err_class_destroy;
	}

	cdev_init(&wlan_hdd_state_cdev, &wlan_hdd_state_fops);

	wlan_hdd_state_cdev.owner = THIS_MODULE;

	ret = cdev_add(&wlan_hdd_state_cdev, device, dev_num);
	if (ret) {
		pr_err("Failed to add cdev error");
		goto cdev_add_err;
	}

	pr_info("wlan_hdd_state %s major(%d) initialized",
		WLAN_CTRL_NAME, wlan_hdd_state_major);

	return 0;

cdev_add_err:
	device_destroy(class, device);
err_class_destroy:
	class_destroy(class);
class_err:
	unregister_chrdev_region(device, dev_num);
dev_alloc_err:
	return -ENODEV;
}

/*
 * When multiple instances of the driver are loaded in parallel, only
 * one can create and own the state ctrl param. An instance of the
 * driver that creates the state ctrl param will wait for
 * HDD_WLAN_START_WAIT_TIME to be probed. If it is probed, then that
 * instance of the driver will stay loaded and no other instances of
 * the driver can load. But if it is not probed, then that instance of
 * the driver will destroy the state ctrl param and exit, and another
 * instance of the driver can then create the state ctrl param.
 */

/* max number of instances we expect (arbitrary) */
#define WLAN_DRIVER_MAX_INSTANCES 5

/* max amount of time an instance has to wait for all instances */
#define CTRL_PARAM_WAIT (WLAN_DRIVER_MAX_INSTANCES * HDD_WLAN_START_WAIT_TIME)

/* amount of time we sleep for each retry (arbitrary) */
#define CTRL_PARAM_SLEEP 100

static void wlan_hdd_state_ctrl_param_destroy(void)
{
	cdev_del(&wlan_hdd_state_cdev);
	device_destroy(class, device);
	class_destroy(class);
	unregister_chrdev_region(device, dev_num);

	pr_info("Device node unregistered");
}

#else /* WLAN_CTRL_NAME */

static int wlan_hdd_state_ctrl_param_create(void)
{
	return 0;
}

static void wlan_hdd_state_ctrl_param_destroy(void)
{
}

#endif /* WLAN_CTRL_NAME */

/**
 * hdd_send_scan_done_complete_cb() - API to send scan done indication to upper
 * layer
 * @vdev_id: vdev id
 *
 * Return: none
 */
static void hdd_send_scan_done_complete_cb(uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct sk_buff *vendor_event;
	uint32_t len;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("No adapter found for vdev id:%d", vdev_id);
		return;
	}

	len = NLMSG_HDRLEN;
	vendor_event =
		wlan_cfg80211_vendor_event_alloc(
			hdd_ctx->wiphy, &adapter->wdev, len,
			QCA_NL80211_VENDOR_SUBCMD_CONNECTED_CHANNEL_STATS_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		hdd_err("wlan_cfg80211_vendor_event_alloc failed");
		return;
	}

	hdd_debug("sending scan done ind to upper layer for vdev_id:%d",
		  vdev_id);
	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

struct osif_vdev_mgr_ops osif_vdev_mgrlegacy_ops = {
#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
	.osif_vdev_mgr_set_mac_addr_response = hdd_set_mac_addr_event_cb,
#endif
	.osif_vdev_mgr_send_scan_done_complete_cb =
					hdd_send_scan_done_complete_cb,

};

static QDF_STATUS hdd_vdev_mgr_register_cb(void)
{
	osif_vdev_mgr_set_legacy_cb(&osif_vdev_mgrlegacy_ops);
	return osif_vdev_mgr_register_cb();
}

static void hdd_vdev_mgr_unregister_cb(void)
{
	osif_vdev_mgr_reset_legacy_cb();
}

/**
 * hdd_component_cb_init() - Initialize component callbacks
 *
 * This function initializes hdd callbacks to different
 * components
 *
 * Context: Any context.
 * Return: QDF_STATUS
 */
static QDF_STATUS hdd_component_cb_init(void)
{
	QDF_STATUS status;

	status = hdd_cm_register_cb();
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = hdd_vdev_mgr_register_cb();
	if (QDF_IS_STATUS_ERROR(status))
		goto cm_unregister_cb;

	status = osif_twt_register_cb();
	if (QDF_IS_STATUS_ERROR(status))
		goto hdd_vdev_mgr_unregister_cb;

	status = hdd_pre_cac_register_cb();
	if (QDF_IS_STATUS_ERROR(status))
		goto hdd_vdev_mgr_unregister_cb;

	return QDF_STATUS_SUCCESS;

hdd_vdev_mgr_unregister_cb:
	hdd_vdev_mgr_unregister_cb();
cm_unregister_cb:
	hdd_cm_unregister_cb();
	return status;
}

/**
 * hdd_component_cb_deinit() - De-initialize component callbacks
 *
 * This function de-initializes hdd callbacks with different components
 *
 * Context: Any context.
 * Return: None`
 */
static void hdd_component_cb_deinit(void)
{
	hdd_pre_cac_unregister_cb();
	hdd_vdev_mgr_unregister_cb();
	hdd_cm_unregister_cb();
}

/**
 * hdd_component_init() - Initialize all components
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS hdd_component_init(void)
{
	QDF_STATUS status;

	/* initialize converged components */

	status = ucfg_mlme_global_init();
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = dispatcher_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto mlme_global_deinit;

	status = target_if_init(wma_get_psoc_from_scn_handle);
	if (QDF_IS_STATUS_ERROR(status))
		goto dispatcher_deinit;

	/* initialize non-converged components */
	status = ucfg_mlme_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto target_if_deinit;

	status = ucfg_fwol_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto mlme_deinit;

	status = disa_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto fwol_deinit;

	status = pmo_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto disa_deinit;

	status = ucfg_ocb_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto pmo_deinit;

	status = ipa_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto ocb_deinit;

	status = ucfg_action_oui_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto ipa_deinit;

	status = nan_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto action_oui_deinit;

	status = ucfg_p2p_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto nan_deinit;

	status = ucfg_interop_issues_ap_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto p2p_deinit;

	status = policy_mgr_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto interop_issues_ap_deinit;

	status = ucfg_tdls_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto policy_deinit;

	status = ucfg_dlm_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto tdls_deinit;

	status = ucfg_pkt_capture_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto dlm_deinit;

	status = ucfg_ftm_time_sync_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto pkt_capture_deinit;

	status = ucfg_pre_cac_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto pre_cac_deinit;

	status = ucfg_dp_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto pre_cac_deinit;

	status = ucfg_qmi_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto dp_deinit;

	return QDF_STATUS_SUCCESS;

dp_deinit:
	ucfg_dp_deinit();
pre_cac_deinit:
	ucfg_pre_cac_deinit();
pkt_capture_deinit:
	ucfg_pkt_capture_deinit();
dlm_deinit:
	ucfg_dlm_deinit();
tdls_deinit:
	ucfg_tdls_deinit();
policy_deinit:
	policy_mgr_deinit();
interop_issues_ap_deinit:
	ucfg_interop_issues_ap_deinit();
p2p_deinit:
	ucfg_p2p_deinit();
nan_deinit:
	nan_deinit();
action_oui_deinit:
	ucfg_action_oui_deinit();
ipa_deinit:
	ipa_deinit();
ocb_deinit:
	ucfg_ocb_deinit();
pmo_deinit:
	pmo_deinit();
disa_deinit:
	disa_deinit();
fwol_deinit:
	ucfg_fwol_deinit();
mlme_deinit:
	ucfg_mlme_deinit();
target_if_deinit:
	target_if_deinit();
dispatcher_deinit:
	dispatcher_deinit();
mlme_global_deinit:
	ucfg_mlme_global_deinit();

	return status;
}

/**
 * hdd_component_deinit() - Deinitialize all components
 *
 * Return: None
 */
static void hdd_component_deinit(void)
{
	/* deinitialize non-converged components */
	ucfg_qmi_deinit();
	ucfg_dp_deinit();
	ucfg_pre_cac_deinit();
	ucfg_ftm_time_sync_deinit();
	ucfg_pkt_capture_deinit();
	ucfg_dlm_deinit();
	ucfg_tdls_deinit();
	policy_mgr_deinit();
	ucfg_interop_issues_ap_deinit();
	ucfg_p2p_deinit();
	nan_deinit();
	ucfg_action_oui_deinit();
	ipa_deinit();
	ucfg_ocb_deinit();
	pmo_deinit();
	disa_deinit();
	ucfg_fwol_deinit();
	ucfg_mlme_deinit();

	/* deinitialize converged components */
	target_if_deinit();
	dispatcher_deinit();
	ucfg_mlme_global_deinit();
}

QDF_STATUS hdd_component_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	status = ucfg_mlme_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = ucfg_dlm_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_dlm;

	status = ucfg_fwol_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_fwol;

	status = ucfg_pmo_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_pmo;

	status = ucfg_policy_mgr_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_plcy_mgr;

	status = ucfg_p2p_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_p2p;

	status = ucfg_tdls_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_tdls;

	status = ucfg_nan_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_nan;

	status = ucfg_twt_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_twt;

	status = ucfg_wifi_pos_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_wifi_pos;

	status = ucfg_dp_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_dp;

	return status;

err_dp:
	ucfg_wifi_pos_psoc_close(psoc);
err_wifi_pos:
	ucfg_twt_psoc_close(psoc);
err_twt:
	ucfg_nan_psoc_close(psoc);
err_nan:
	ucfg_tdls_psoc_close(psoc);
err_tdls:
	ucfg_p2p_psoc_close(psoc);
err_p2p:
	ucfg_policy_mgr_psoc_close(psoc);
err_plcy_mgr:
	ucfg_pmo_psoc_close(psoc);
err_pmo:
	ucfg_fwol_psoc_close(psoc);
err_fwol:
	ucfg_dlm_psoc_close(psoc);
err_dlm:
	ucfg_mlme_psoc_close(psoc);

	return status;
}

void hdd_component_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	ucfg_dp_psoc_close(psoc);
	ucfg_wifi_pos_psoc_close(psoc);
	ucfg_twt_psoc_close(psoc);
	ucfg_nan_psoc_close(psoc);
	ucfg_tdls_psoc_close(psoc);
	ucfg_p2p_psoc_close(psoc);
	ucfg_policy_mgr_psoc_close(psoc);
	ucfg_pmo_psoc_close(psoc);
	ucfg_fwol_psoc_close(psoc);
	ucfg_dlm_psoc_close(psoc);
	ucfg_mlme_psoc_close(psoc);
}

void hdd_component_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	ocb_psoc_enable(psoc);
	disa_psoc_enable(psoc);
	nan_psoc_enable(psoc);
	p2p_psoc_enable(psoc);
	ucfg_interop_issues_ap_psoc_enable(psoc);
	policy_mgr_psoc_enable(psoc);
	ucfg_tdls_psoc_enable(psoc);
	ucfg_fwol_psoc_enable(psoc);
	ucfg_action_oui_psoc_enable(psoc);
}

void hdd_component_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	ucfg_action_oui_psoc_disable(psoc);
	ucfg_fwol_psoc_disable(psoc);
	ucfg_tdls_psoc_disable(psoc);
	policy_mgr_psoc_disable(psoc);
	ucfg_interop_issues_ap_psoc_disable(psoc);
	p2p_psoc_disable(psoc);
	nan_psoc_disable(psoc);
	disa_psoc_disable(psoc);
	ocb_psoc_disable(psoc);
}

QDF_STATUS hdd_component_pdev_open(struct wlan_objmgr_pdev *pdev)
{
	return ucfg_mlme_pdev_open(pdev);
}

void hdd_component_pdev_close(struct wlan_objmgr_pdev *pdev)
{
	ucfg_mlme_pdev_close(pdev);
}

static QDF_STATUS hdd_qdf_print_init(void)
{
	QDF_STATUS status;
	int qdf_print_idx;

	status = qdf_print_setup();
	if (QDF_IS_STATUS_ERROR(status)) {
		pr_err("Failed qdf_print_setup; status:%u\n", status);
		return status;
	}

	qdf_print_idx = qdf_print_ctrl_register(cinfo, NULL, NULL, "MCL_WLAN");
	if (qdf_print_idx < 0) {
		pr_err("Failed to register for qdf_print_ctrl\n");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_set_pidx(qdf_print_idx);

	return QDF_STATUS_SUCCESS;
}

static void hdd_qdf_print_deinit(void)
{
	int qdf_pidx = qdf_get_pidx();

	qdf_set_pidx(-1);
	qdf_print_ctrl_cleanup(qdf_pidx);

	/* currently, no qdf print 'un-setup'*/
}

static QDF_STATUS hdd_qdf_init(void)
{
	QDF_STATUS status;

	status = hdd_qdf_print_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto exit;

	status = qdf_debugfs_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to init debugfs; status:%u", status);
		goto print_deinit;
	}

	qdf_lock_stats_init();
	qdf_mem_init();
	qdf_delayed_work_feature_init();
	qdf_periodic_work_feature_init();
	qdf_wake_lock_feature_init();
	qdf_mc_timer_manager_init();
	qdf_event_list_init();

	status = qdf_talloc_feature_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to init talloc; status:%u", status);
		goto event_deinit;
	}

	status = qdf_cpuhp_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to init cpuhp; status:%u", status);
		goto talloc_deinit;
	}

	status = qdf_trace_spin_lock_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to init spinlock; status:%u", status);
		goto cpuhp_deinit;
	}

	qdf_trace_init();
	qdf_minidump_init();
	qdf_ssr_driver_dump_init();
	qdf_register_debugcb_init();

	return QDF_STATUS_SUCCESS;

cpuhp_deinit:
	qdf_cpuhp_deinit();
talloc_deinit:
	qdf_talloc_feature_deinit();
event_deinit:
	qdf_event_list_destroy();
	qdf_mc_timer_manager_exit();
	qdf_wake_lock_feature_deinit();
	qdf_periodic_work_feature_deinit();
	qdf_delayed_work_feature_deinit();
	qdf_mem_exit();
	qdf_lock_stats_deinit();
	qdf_debugfs_exit();
print_deinit:
	hdd_qdf_print_deinit();

exit:
	return status;
}

static void hdd_qdf_deinit(void)
{
	/* currently, no debugcb deinit */
	qdf_ssr_driver_dump_deinit();
	qdf_minidump_deinit();
	qdf_trace_deinit();

	/* currently, no trace spinlock deinit */

	qdf_cpuhp_deinit();
	qdf_talloc_feature_deinit();
	qdf_event_list_destroy();
	qdf_mc_timer_manager_exit();
	qdf_wake_lock_feature_deinit();
	qdf_periodic_work_feature_deinit();
	qdf_delayed_work_feature_deinit();
	qdf_mem_exit();
	qdf_lock_stats_deinit();
	qdf_debugfs_exit();
	hdd_qdf_print_deinit();
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
static bool is_monitor_mode_supported(void)
{
	return true;
}
#else
static bool is_monitor_mode_supported(void)
{
	pr_err("Monitor mode not supported!");
	return false;
}
#endif

#ifdef WLAN_FEATURE_EPPING
static bool is_epping_mode_supported(void)
{
	return true;
}
#else
static bool is_epping_mode_supported(void)
{
	pr_err("Epping mode not supported!");
	return false;
}
#endif

#ifdef QCA_WIFI_FTM
static bool is_ftm_mode_supported(void)
{
	return true;
}
#else
static bool is_ftm_mode_supported(void)
{
	pr_err("FTM mode not supported!");
	return false;
}
#endif

/**
 * is_con_mode_valid() - check con mode is valid or not
 * @mode: global con mode
 *
 * Return: TRUE on success FALSE on failure
 */
static bool is_con_mode_valid(enum QDF_GLOBAL_MODE mode)
{
	switch (mode) {
	case QDF_GLOBAL_MONITOR_MODE:
		return is_monitor_mode_supported();
	case QDF_GLOBAL_EPPING_MODE:
		return is_epping_mode_supported();
	case QDF_GLOBAL_FTM_MODE:
		return is_ftm_mode_supported();
	case QDF_GLOBAL_MISSION_MODE:
		return true;
	default:
		return false;
	}
}

static void hdd_stop_present_mode(struct hdd_context *hdd_ctx,
				  enum QDF_GLOBAL_MODE curr_mode)
{
	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED)
		return;

	switch (curr_mode) {
	case QDF_GLOBAL_MONITOR_MODE:
		hdd_info("Release wakelock for monitor mode!");
		qdf_wake_lock_release(&hdd_ctx->monitor_mode_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_MONITOR_MODE);
		fallthrough;
	case QDF_GLOBAL_MISSION_MODE:
	case QDF_GLOBAL_FTM_MODE:
		hdd_abort_mac_scan_all_adapters(hdd_ctx);
		wlan_cfg80211_cleanup_scan_queue(hdd_ctx->pdev, NULL);
		hdd_stop_all_adapters(hdd_ctx);
		hdd_deinit_all_adapters(hdd_ctx, false);

		break;
	default:
		break;
	}
}

static void hdd_cleanup_present_mode(struct hdd_context *hdd_ctx,
				    enum QDF_GLOBAL_MODE curr_mode)
{
	switch (curr_mode) {
	case QDF_GLOBAL_MISSION_MODE:
	case QDF_GLOBAL_MONITOR_MODE:
	case QDF_GLOBAL_FTM_MODE:
		hdd_close_all_adapters(hdd_ctx, false);
		break;
	case QDF_GLOBAL_EPPING_MODE:
		epping_disable();
		epping_close();
		break;
	default:
		return;
	}
}

static int
hdd_parse_driver_mode(const char *mode_str, enum QDF_GLOBAL_MODE *out_mode)
{
	QDF_STATUS status;
	uint32_t mode;

	*out_mode = QDF_GLOBAL_MAX_MODE;

	status = qdf_uint32_parse(mode_str, &mode);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	if (mode >= QDF_GLOBAL_MAX_MODE)
		return -ERANGE;

	*out_mode = (enum QDF_GLOBAL_MODE)mode;

	return 0;
}

static int hdd_mode_change_psoc_idle_shutdown(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return -EINVAL;

	return hdd_wlan_stop_modules(hdd_ctx, true);
}

static int hdd_mode_change_psoc_idle_restart(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	int ret;

	if (!hdd_ctx)
		return -EINVAL;
	ret = hdd_soc_idle_restart_lock(dev);
	if (ret)
		return ret;
	ret = hdd_wlan_start_modules(hdd_ctx, false);
	hdd_soc_idle_restart_unlock();

	return ret;
}

/**
 * __hdd_driver_mode_change() - Handles a driver mode change
 * @hdd_ctx: Pointer to the global HDD context
 * @next_mode: the driver mode to transition to
 *
 * This function is invoked when user updates con_mode using sys entry,
 * to initialize and bring-up driver in that specific mode.
 *
 * Return: Errno
 */
static int __hdd_driver_mode_change(struct hdd_context *hdd_ctx,
				    enum QDF_GLOBAL_MODE next_mode)
{
	enum QDF_GLOBAL_MODE curr_mode;
	int errno;
	struct bbm_params param = {0};

	hdd_info("Driver mode changing to %d", next_mode);

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	if (!is_con_mode_valid(next_mode)) {
		hdd_err_rl("Requested driver mode is invalid");
		return -EINVAL;
	}

	curr_mode = hdd_get_conparam();
	if (curr_mode == next_mode) {
		hdd_err_rl("Driver is already in the requested mode");
		return 0;
	}

	hdd_psoc_idle_timer_stop(hdd_ctx);

	/* ensure adapters are stopped */
	hdd_stop_present_mode(hdd_ctx, curr_mode);

	if (DRIVER_MODULES_CLOSED != hdd_ctx->driver_status) {
		is_mode_change_psoc_idle_shutdown = true;
		errno = pld_idle_shutdown(hdd_ctx->parent_dev,
					  hdd_mode_change_psoc_idle_shutdown);
		if (errno) {
			is_mode_change_psoc_idle_shutdown = false;
			hdd_err("Stop wlan modules failed");
			return errno;
		}
	}

	/* Cleanup present mode before switching to new mode */
	hdd_cleanup_present_mode(hdd_ctx, curr_mode);

	hdd_set_conparam(next_mode);
	pld_set_mode(next_mode);

	qdf_event_reset(&hdd_ctx->regulatory_update_event);
	qdf_mutex_acquire(&hdd_ctx->regulatory_status_lock);
	hdd_ctx->is_regulatory_update_in_progress = true;
	qdf_mutex_release(&hdd_ctx->regulatory_status_lock);

	errno = pld_idle_restart(hdd_ctx->parent_dev,
				 hdd_mode_change_psoc_idle_restart);
	if (errno) {
		hdd_err("Start wlan modules failed: %d", errno);
		return errno;
	}

	errno = hdd_open_adapters_for_mode(hdd_ctx, next_mode);
	if (errno) {
		hdd_err("Failed to open adapters");
		return errno;
	}

	if (next_mode == QDF_GLOBAL_MONITOR_MODE) {
		struct hdd_adapter *adapter =
			hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);

		QDF_BUG(adapter);
		if (!adapter) {
			hdd_err("Failed to get monitor adapter");
			return -EINVAL;
		}

		errno = hdd_start_adapter(adapter);
		if (errno) {
			hdd_err("Failed to start monitor adapter");
			return errno;
		}

		hdd_info("Acquire wakelock for monitor mode");
		qdf_wake_lock_acquire(&hdd_ctx->monitor_mode_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_MONITOR_MODE);
	}

	/* con_mode is a global module parameter */
	con_mode = next_mode;
	hdd_info("Driver mode successfully changed to %d", next_mode);

	param.policy = BBM_DRIVER_MODE_POLICY;
	param.policy_info.driver_mode = con_mode;
	ucfg_dp_bbm_apply_independent_policy(hdd_ctx->psoc, &param);

	return 0;
}

static int hdd_driver_mode_change(enum QDF_GLOBAL_MODE mode)
{
	struct osif_driver_sync *driver_sync;
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;
	int errno;

	hdd_enter();

	status = osif_driver_sync_trans_start_wait(&driver_sync);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to start 'mode change'; status:%u", status);
		errno = qdf_status_to_os_return(status);
		goto exit;
	}

	osif_driver_sync_wait_for_ops(driver_sync);

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		goto trans_stop;

	errno = __hdd_driver_mode_change(hdd_ctx, mode);

trans_stop:
	osif_driver_sync_trans_stop(driver_sync);

exit:
	hdd_exit();

	return errno;
}

static int hdd_set_con_mode(enum QDF_GLOBAL_MODE mode)
{
	con_mode = mode;

	return 0;
}

static int (*hdd_set_con_mode_cb)(enum QDF_GLOBAL_MODE mode) = hdd_set_con_mode;

static void hdd_driver_mode_change_register(void)
{
	hdd_set_con_mode_cb = hdd_driver_mode_change;
}

static void hdd_driver_mode_change_unregister(void)
{
	hdd_set_con_mode_cb = hdd_set_con_mode;
}

static int con_mode_handler(const char *kmessage, const struct kernel_param *kp)
{
	enum QDF_GLOBAL_MODE mode;
	int errno;

	errno = hdd_parse_driver_mode(kmessage, &mode);
	if (errno) {
		hdd_err_rl("Failed to parse driver mode '%s'", kmessage);
		return errno;
	}

	return hdd_set_con_mode_cb(mode);
}

/*
 * If the wlan_hdd_register_driver will return an error
 * if the wlan driver tries to register with the
 * platform driver before cnss_probe is completed.
 * Depending on the error code, the wlan driver waits
 * and retries to register.
 */

/* Max number of retries (arbitrary)*/
#define HDD_MAX_PLD_REGISTER_RETRY (50)

/* Max amount of time we sleep before each retry */
#define HDD_PLD_REGISTER_FAIL_SLEEP_DURATION (100)

static int hdd_register_driver_retry(void)
{
	int count = 0;
	int errno;

	while (true) {
		errno = wlan_hdd_register_driver();
		if (errno != -EAGAIN)
			return errno;
		hdd_nofl_info("Retry Platform Driver Registration; errno:%d count:%d",
			      errno, count);
		if (++count == HDD_MAX_PLD_REGISTER_RETRY)
			return errno;
		msleep(HDD_PLD_REGISTER_FAIL_SLEEP_DURATION);
		continue;
	}

	return errno;
}

/**
 * hdd_create_wifi_feature_interface() - Create wifi feature interface
 *
 * Return: none
 */
static void hdd_create_wifi_feature_interface(void)
{
	hdd_sysfs_create_wifi_root_obj();
	hdd_create_wifi_feature_interface_sysfs_file();
}

int hdd_driver_load(void)
{
	struct osif_driver_sync *driver_sync;
	QDF_STATUS status;
	int errno;
	bool soft_load;

	pr_err("%s: Loading driver v%s\n", WLAN_MODULE_NAME,
	       g_wlan_driver_version);
	hdd_place_marker(NULL, "START LOADING", NULL);

	status = hdd_qdf_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		errno = qdf_status_to_os_return(status);
		goto exit;
	}

	osif_sync_init();

	status = osif_driver_sync_create_and_trans(&driver_sync);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to init driver sync; status:%u", status);
		errno = qdf_status_to_os_return(status);
		goto sync_deinit;
	}

	errno = hdd_init();
	if (errno) {
		hdd_err("Failed to init HDD; errno:%d", errno);
		goto trans_stop;
	}

	status = hdd_component_cb_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to init component cb; status:%u", status);
		errno = qdf_status_to_os_return(status);
		goto hdd_deinit;
	}

	status = hdd_component_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to init components; status:%u", status);
		errno = qdf_status_to_os_return(status);
		goto comp_cb_deinit;
	}

	status = qdf_wake_lock_create(&wlan_wake_lock, "wlan");
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to create wake lock; status:%u", status);
		errno = qdf_status_to_os_return(status);
		goto comp_deinit;
	}

	hdd_set_conparam(con_mode);

	errno = pld_init();
	if (errno) {
		hdd_err("Failed to init PLD; errno:%d", errno);
		goto wakelock_destroy;
	}

	/* driver mode pass to cnss2 platform driver*/
	errno = pld_set_mode(con_mode);
	if (errno)
		hdd_err("Failed to set mode in PLD; errno:%d", errno);

	hdd_driver_mode_change_register();

	osif_driver_sync_register(driver_sync);
	osif_driver_sync_trans_stop(driver_sync);

	/* psoc probe can happen in registration; do after 'load' transition */
	errno = hdd_register_driver_retry();
	if (errno) {
		hdd_err("Failed to register driver; errno:%d", errno);
		goto pld_deinit;
	}

	/* If a soft unload of driver is done, we don't call
	 * wlan_hdd_state_ctrl_param_destroy() to maintain sync
	 * with userspace. In Symmetry, during soft load, avoid
	 * calling wlan_hdd_state_ctrl_param_create().
	 */
	soft_load = hdd_get_wlan_driver_status();
	if (soft_load)
		goto out;

	errno = wlan_hdd_state_ctrl_param_create();
	if (errno) {
		hdd_err("Failed to create ctrl param; errno:%d", errno);
		goto unregister_driver;
	}
	hdd_create_wifi_feature_interface();
out:
	hdd_debug("%s: driver loaded", WLAN_MODULE_NAME);
	hdd_place_marker(NULL, "DRIVER LOADED", NULL);

	return 0;

unregister_driver:
	wlan_hdd_unregister_driver();
pld_deinit:
	status = osif_driver_sync_trans_start(&driver_sync);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));

	osif_driver_sync_unregister();
	osif_driver_sync_wait_for_ops(driver_sync);

	hdd_driver_mode_change_unregister();
	pld_deinit();

	hdd_start_complete(errno);
	/* Wait for any ref taken on /dev/wlan to be released */
	while (qdf_atomic_read(&wlan_hdd_state_fops_ref))
		;
wakelock_destroy:
	qdf_wake_lock_destroy(&wlan_wake_lock);
comp_deinit:
	hdd_component_deinit();
comp_cb_deinit:
	hdd_component_cb_deinit();
hdd_deinit:
	hdd_deinit();
trans_stop:
	osif_driver_sync_trans_stop(driver_sync);
	osif_driver_sync_destroy(driver_sync);
sync_deinit:
	osif_sync_deinit();
	hdd_qdf_deinit();

exit:
	return errno;
}

#ifdef FEATURE_WLAN_RESIDENT_DRIVER
EXPORT_SYMBOL(hdd_driver_load);
#endif

/**
 * hdd_distroy_wifi_feature_interface() - Distroy wifi feature interface
 *
 * Return: none
 */
static void hdd_distroy_wifi_feature_interface(void)
{
	hdd_destroy_wifi_feature_interface_sysfs_file();
	hdd_sysfs_destroy_wifi_root_obj();
}

void hdd_driver_unload(void)
{
	struct osif_driver_sync *driver_sync;
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;
	void *hif_ctx;
	bool soft_unload;

	soft_unload = hdd_get_wlan_driver_status();
	if (soft_unload) {
		pr_info("%s: Soft Unloading driver v%s\n", WLAN_MODULE_NAME,
			QWLAN_VERSIONSTR);
	} else {
		pr_info("%s: Hard Unloading driver v%s\n", WLAN_MODULE_NAME,
			QWLAN_VERSIONSTR);
	}

	hdd_place_marker(NULL, "START UNLOADING", NULL);

	/*
	 * Wait for any trans to complete and then start the driver trans
	 * for the unload. This will ensure that the driver trans proceeds only
	 * after all trans have been completed. As a part of this trans, set
	 * the driver load/unload flag to further ensure that any upcoming
	 * trans are rejected via wlan_hdd_validate_context.
	 */
	status = osif_driver_sync_trans_start_wait(&driver_sync);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to unload wlan; status:%u", status);
		hdd_place_marker(NULL, "UNLOAD FAILURE", NULL);
		return;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (hif_ctx) {
		/*
		 * Trigger runtime sync resume before setting unload in progress
		 * such that resume can happen successfully
		 */
		qdf_rtpm_sync_resume();
	}

	cds_set_driver_loaded(false);
	cds_set_unload_in_progress(true);

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (hdd_ctx) {
		hdd_psoc_idle_timer_stop(hdd_ctx);
		/*
		 * Runtime PM sync resume may have started the bus bandwidth
		 * periodic work hence stop it.
		 */
		ucfg_dp_bus_bw_compute_timer_stop(hdd_ctx->psoc);
	}

	/*
	 * Stop the trans before calling unregister_driver as that involves a
	 * call to pld_remove which in itself is a psoc transaction
	 */
	osif_driver_sync_trans_stop(driver_sync);

	hdd_distroy_wifi_feature_interface();
	if (!soft_unload)
		wlan_hdd_state_ctrl_param_destroy();

	/* trigger SoC remove */
	wlan_hdd_unregister_driver();

	status = osif_driver_sync_trans_start_wait(&driver_sync);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to unload wlan; status:%u", status);
		hdd_place_marker(NULL, "UNLOAD FAILURE", NULL);
		return;
	}

	osif_driver_sync_unregister();
	osif_driver_sync_wait_for_ops(driver_sync);

	hdd_driver_mode_change_unregister();
	pld_deinit();
	hdd_set_conparam(0);
	qdf_wake_lock_destroy(&wlan_wake_lock);
	hdd_component_deinit();
	hdd_component_cb_deinit();
	hdd_deinit();

	osif_driver_sync_trans_stop(driver_sync);
	osif_driver_sync_destroy(driver_sync);

	osif_sync_deinit();

	hdd_qdf_deinit();
	hdd_place_marker(NULL, "UNLOAD DONE", NULL);
}

#ifdef FEATURE_WLAN_RESIDENT_DRIVER
EXPORT_SYMBOL(hdd_driver_unload);
#endif

#ifndef MODULE
/**
 * wlan_boot_cb() - Wlan boot callback
 * @kobj:      object whose directory we're creating the link in.
 * @attr:      attribute the user is interacting with
 * @buf:       the buffer containing the user data
 * @count:     number of bytes in the buffer
 *
 * This callback is invoked when the fs is ready to start the
 * wlan driver initialization.
 *
 * Return: 'count' on success or a negative error code in case of failure
 */
static ssize_t wlan_boot_cb(struct kobject *kobj,
			    struct kobj_attribute *attr,
			    const char *buf,
			    size_t count)
{

	if (wlan_loader->loaded_state) {
		hdd_err("wlan driver already initialized");
		return -EALREADY;
	}

	if (hdd_driver_load())
		return -EIO;

	wlan_loader->loaded_state = MODULE_INITIALIZED;

	return count;
}

/**
 * hdd_sysfs_cleanup() - cleanup sysfs
 *
 * Return: None
 *
 */
static void hdd_sysfs_cleanup(void)
{
	/* remove from group */
	if (wlan_loader->boot_wlan_obj && wlan_loader->attr_group)
		sysfs_remove_group(wlan_loader->boot_wlan_obj,
				   wlan_loader->attr_group);

	/* unlink the object from parent */
	kobject_del(wlan_loader->boot_wlan_obj);

	/* free the object */
	kobject_put(wlan_loader->boot_wlan_obj);

	kfree(wlan_loader->attr_group);
	kfree(wlan_loader);

	wlan_loader = NULL;
}

/**
 * wlan_init_sysfs() - Creates the sysfs to be invoked when the fs is
 * ready
 *
 * This is creates the syfs entry boot_wlan. Which shall be invoked
 * when the filesystem is ready.
 *
 * QDF API cannot be used here since this function is called even before
 * initializing WLAN driver.
 *
 * Return: 0 for success, errno on failure
 */
static int wlan_init_sysfs(void)
{
	int ret = -ENOMEM;

	wlan_loader = kzalloc(sizeof(*wlan_loader), GFP_KERNEL);
	if (!wlan_loader)
		return -ENOMEM;

	wlan_loader->boot_wlan_obj = NULL;
	wlan_loader->attr_group = kzalloc(sizeof(*(wlan_loader->attr_group)),
					  GFP_KERNEL);
	if (!wlan_loader->attr_group)
		goto error_return;

	wlan_loader->loaded_state = 0;
	wlan_loader->attr_group->attrs = attrs;

	wlan_loader->boot_wlan_obj = kobject_create_and_add(WLAN_LOADER_NAME,
							    kernel_kobj);
	if (!wlan_loader->boot_wlan_obj) {
		hdd_err("sysfs create and add failed");
		goto error_return;
	}

	ret = sysfs_create_group(wlan_loader->boot_wlan_obj,
				 wlan_loader->attr_group);
	if (ret) {
		hdd_err("sysfs create group failed; errno:%d", ret);
		goto error_return;
	}

	return 0;

error_return:
	hdd_sysfs_cleanup();

	return ret;
}

/**
 * wlan_deinit_sysfs() - Removes the sysfs created to initialize the wlan
 *
 * Return: 0 on success or errno on failure
 */
static int wlan_deinit_sysfs(void)
{
	if (!wlan_loader) {
		hdd_err("wlan_loader is null");
		return -EINVAL;
	}

	hdd_sysfs_cleanup();
	return 0;
}

#endif /* MODULE */

#ifdef MODULE
/**
 * hdd_module_init() - Module init helper
 *
 * Module init helper function used by both module and static driver.
 *
 * Return: 0 for success, errno on failure
 */
#ifdef FEATURE_WLAN_RESIDENT_DRIVER
static int hdd_module_init(void)
{
	return 0;
}
#else
static int hdd_module_init(void)
{
	if (hdd_driver_load())
		return -EINVAL;

	return 0;
}
#endif
#else
static int __init hdd_module_init(void)
{
	int ret = -EINVAL;

	ret = wlan_init_sysfs();
	if (ret)
		hdd_err("Failed to create sysfs entry");

	return ret;
}
#endif


#ifdef MODULE
/**
 * hdd_module_exit() - Exit function
 *
 * This is the driver exit point (invoked when module is unloaded using rmmod)
 *
 * Return: None
 */
#ifdef FEATURE_WLAN_RESIDENT_DRIVER
static void __exit hdd_module_exit(void)
{
}
#else
static void __exit hdd_module_exit(void)
{
	hdd_driver_unload();
}
#endif
#else
static void __exit hdd_module_exit(void)
{
	hdd_driver_unload();
	wlan_deinit_sysfs();
}
#endif

static int fwpath_changed_handler(const char *kmessage,
				  const struct kernel_param *kp)
{
	return param_set_copystring(kmessage, kp);
}

static int con_mode_handler_ftm(const char *kmessage,
				const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(kmessage, kp);

	if (cds_is_driver_loaded() || cds_is_load_or_unload_in_progress()) {
		pr_err("Driver already loaded or load/unload in progress");
		return -ENOTSUPP;
	}

	if (con_mode_ftm != QDF_GLOBAL_FTM_MODE) {
		pr_err("Only FTM mode supported!");
		return -ENOTSUPP;
	}

	hdd_set_conparam(con_mode_ftm);
	con_mode = con_mode_ftm;

	return ret;
}

#ifdef WLAN_FEATURE_EPPING
static int con_mode_handler_epping(const char *kmessage,
				   const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(kmessage, kp);

	if (con_mode_epping != QDF_GLOBAL_EPPING_MODE) {
		pr_err("Only EPPING mode supported!");
		return -ENOTSUPP;
	}

	hdd_set_conparam(con_mode_epping);
	con_mode = con_mode_epping;

	return ret;
}
#endif

/**
 * hdd_get_conparam() - driver exit point
 *
 * This is the driver exit point (invoked when module is unloaded using rmmod)
 *
 * Return: enum QDF_GLOBAL_MODE
 */
enum QDF_GLOBAL_MODE hdd_get_conparam(void)
{
	return (enum QDF_GLOBAL_MODE) curr_con_mode;
}

void hdd_set_conparam(int32_t con_param)
{
	curr_con_mode = con_param;
}

/**
 * hdd_svc_fw_crashed_ind() - API to send FW CRASHED IND to Userspace
 *
 * Return: void
 */
static void hdd_svc_fw_crashed_ind(void)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	hdd_ctx ? wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					      WLAN_SVC_FW_CRASHED_IND,
					      NULL, 0) : 0;
}

/**
 * hdd_update_ol_config - API to update ol configuration parameters
 * @hdd_ctx: HDD context
 *
 * Return: void
 */
static void hdd_update_ol_config(struct hdd_context *hdd_ctx)
{
	struct ol_config_info cfg = {0};
	struct ol_context *ol_ctx = cds_get_context(QDF_MODULE_ID_BMI);
	bool self_recovery = false;
	QDF_STATUS status;

	if (!ol_ctx)
		return;

	status = ucfg_mlme_get_self_recovery(hdd_ctx->psoc, &self_recovery);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get self recovery ini config");

	cfg.enable_self_recovery = self_recovery;
	cfg.enable_uart_print = hdd_ctx->config->enablefwprint;
	cfg.enable_fw_log = hdd_ctx->config->enable_fw_log;
	cfg.enable_ramdump_collection = hdd_ctx->config->is_ramdump_enabled;
	cfg.enable_lpass_support = hdd_lpass_is_supported(hdd_ctx);

	ol_init_ini_config(ol_ctx, &cfg);
	ol_set_fw_crashed_cb(ol_ctx, hdd_svc_fw_crashed_ind);
}

#ifdef FEATURE_RUNTIME_PM
/**
 * hdd_populate_runtime_cfg() - populate runtime configuration
 * @hdd_ctx: hdd context
 * @cfg: pointer to the configuration memory being populated
 *
 * Return: void
 */
static void hdd_populate_runtime_cfg(struct hdd_context *hdd_ctx,
				     struct hif_config_info *cfg)
{
	cfg->enable_runtime_pm = hdd_ctx->config->runtime_pm;
	cfg->runtime_pm_delay =
		ucfg_pmo_get_runtime_pm_delay(hdd_ctx->psoc);
}
#else
static void hdd_populate_runtime_cfg(struct hdd_context *hdd_ctx,
				     struct hif_config_info *cfg)
{
}
#endif

/**
 * hdd_update_hif_config - API to update HIF configuration parameters
 * @hdd_ctx: HDD Context
 *
 * Return: void
 */
static void hdd_update_hif_config(struct hdd_context *hdd_ctx)
{
	struct hif_opaque_softc *scn = cds_get_context(QDF_MODULE_ID_HIF);
	struct hif_config_info cfg = {0};
	bool prevent_link_down = false;
	bool self_recovery = false;
	QDF_STATUS status;

	if (!scn)
		return;

	status = ucfg_mlme_get_prevent_link_down(hdd_ctx->psoc,
						 &prevent_link_down);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get prevent_link_down config");

	status = ucfg_mlme_get_self_recovery(hdd_ctx->psoc, &self_recovery);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get self recovery ini config");

	cfg.enable_self_recovery = self_recovery;
	hdd_populate_runtime_cfg(hdd_ctx, &cfg);
	cfg.rx_softirq_max_yield_duration_ns =
		ucfg_dp_get_rx_softirq_yield_duration(hdd_ctx->psoc);

	hif_init_ini_config(scn, &cfg);

	if (prevent_link_down)
		hif_vote_link_up(scn);
}

/**
 * hdd_update_dp_config() - Propagate config parameters to Lithium
 *                          datapath
 * @hdd_ctx: HDD Context
 *
 * Return: 0 for success/errno for failure
 */
static int hdd_update_dp_config(struct hdd_context *hdd_ctx)
{
	struct wlan_dp_user_config dp_cfg;
	QDF_STATUS status;

	dp_cfg.ipa_enable = ucfg_ipa_is_enabled();
	dp_cfg.arp_connectivity_map = CONNECTIVITY_CHECK_SET_ARP;

	status = ucfg_dp_update_config(hdd_ctx->psoc, &dp_cfg);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("failed DP PSOC configuration update");
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_update_config() - Initialize driver per module ini parameters
 * @hdd_ctx: HDD Context
 *
 * API is used to initialize all driver per module configuration parameters
 * Return: 0 for success, errno for failure
 */
int hdd_update_config(struct hdd_context *hdd_ctx)
{
	int ret;

	if (ucfg_pmo_is_ns_offloaded(hdd_ctx->psoc))
		hdd_ctx->ns_offload_enable = true;

	hdd_update_ol_config(hdd_ctx);
	hdd_update_hif_config(hdd_ctx);
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam())
		ret = hdd_update_cds_config_ftm(hdd_ctx);
	else
		ret = hdd_update_cds_config(hdd_ctx);
	ret = hdd_update_user_config(hdd_ctx);

	hdd_update_regdb_offload_config(hdd_ctx);

	return ret;
}

/**
 * hdd_update_pmo_config - API to update pmo configuration parameters
 * @hdd_ctx: HDD context
 *
 * Return: void
 */
static int hdd_update_pmo_config(struct hdd_context *hdd_ctx)
{
	struct pmo_psoc_cfg psoc_cfg = {0};
	QDF_STATUS status;
	enum pmo_wow_enable_type wow_enable;

	ucfg_pmo_get_psoc_config(hdd_ctx->psoc, &psoc_cfg);

	/*
	 * Value of hdd_ctx->wowEnable can be,
	 * 0 - Disable both magic pattern match and pattern byte match.
	 * 1 - Enable magic pattern match on all interfaces.
	 * 2 - Enable pattern byte match on all interfaces.
	 * 3 - Enable both magic patter and pattern byte match on
	 *     all interfaces.
	 */
	wow_enable = ucfg_pmo_get_wow_enable(hdd_ctx->psoc);
	psoc_cfg.magic_ptrn_enable = (wow_enable & 0x01) ? true : false;
	psoc_cfg.ptrn_match_enable_all_vdev =
				(wow_enable & 0x02) ? true : false;
	psoc_cfg.ap_arpns_support = hdd_ctx->ap_arpns_support;
	psoc_cfg.d0_wow_supported = wma_d0_wow_is_supported();
	ucfg_mlme_get_sap_max_modulated_dtim(hdd_ctx->psoc,
					     &psoc_cfg.sta_max_li_mod_dtim);

	hdd_lpass_populate_pmo_config(&psoc_cfg, hdd_ctx);

	status = ucfg_pmo_update_psoc_config(hdd_ctx->psoc, &psoc_cfg);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("failed pmo psoc configuration; status:%d", status);

	return qdf_status_to_os_return(status);
}

void hdd_update_ie_allowlist_attr(struct probe_req_allowlist_attr *ie_allowlist,
				  struct hdd_context *hdd_ctx)
{
	struct wlan_fwol_ie_allowlist allowlist = {0};
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	QDF_STATUS status;
	bool is_ie_allowlist_enable = false;
	uint8_t i = 0;

	status = ucfg_fwol_get_ie_allowlist(psoc, &is_ie_allowlist_enable);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to get IE allowlist param");
		return;
	}

	ie_allowlist->allow_list = is_ie_allowlist_enable;
	if (!ie_allowlist->allow_list)
		return;

	status = ucfg_fwol_get_all_allowlist_params(psoc, &allowlist);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to get all allowlist params");
		return;
	}

	ie_allowlist->ie_bitmap[0] = allowlist.ie_bitmap_0;
	ie_allowlist->ie_bitmap[1] = allowlist.ie_bitmap_1;
	ie_allowlist->ie_bitmap[2] = allowlist.ie_bitmap_2;
	ie_allowlist->ie_bitmap[3] = allowlist.ie_bitmap_3;
	ie_allowlist->ie_bitmap[4] = allowlist.ie_bitmap_4;
	ie_allowlist->ie_bitmap[5] = allowlist.ie_bitmap_5;
	ie_allowlist->ie_bitmap[6] = allowlist.ie_bitmap_6;
	ie_allowlist->ie_bitmap[7] = allowlist.ie_bitmap_7;

	ie_allowlist->num_vendor_oui = allowlist.no_of_probe_req_ouis;
	for (i = 0; i < ie_allowlist->num_vendor_oui; i++)
		ie_allowlist->voui[i] = allowlist.probe_req_voui[i];
}

QDF_STATUS hdd_update_score_config(struct hdd_context *hdd_ctx)
{
	struct hdd_config *cfg = hdd_ctx->config;
	eCsrPhyMode phy_mode = hdd_cfg_xlate_to_csr_phy_mode(cfg->dot11Mode);

	sme_update_score_config(hdd_ctx->mac_handle, phy_mode,
				hdd_ctx->num_rf_chains);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_update_dfs_config() - API to update dfs configuration parameters.
 * @hdd_ctx: HDD context
 *
 * Return: 0 if success else err
 */
static int hdd_update_dfs_config(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	struct dfs_user_config dfs_cfg = {0};
	QDF_STATUS status;

	ucfg_mlme_get_dfs_filter_offload(hdd_ctx->psoc,
					 &dfs_cfg.dfs_is_phyerr_filter_offload);
	status = ucfg_dfs_update_config(psoc, &dfs_cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed dfs psoc configuration");
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_update_scan_config - API to update scan configuration parameters
 * @hdd_ctx: HDD context
 *
 * Return: 0 if success else err
 */
int hdd_update_scan_config(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	struct scan_user_cfg scan_cfg;
	QDF_STATUS status;
	uint32_t mcast_mcc_rest_time = 0;

	qdf_mem_zero(&scan_cfg, sizeof(scan_cfg));
	status = ucfg_mlme_get_sta_miracast_mcc_rest_time(hdd_ctx->psoc,
							  &mcast_mcc_rest_time);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("ucfg_mlme_get_sta_miracast_mcc_rest_time, use def");
		return -EIO;
	}
	scan_cfg.sta_miracast_mcc_rest_time = mcast_mcc_rest_time;
	hdd_update_ie_allowlist_attr(&scan_cfg.ie_allowlist, hdd_ctx);

	status = ucfg_scan_update_user_config(psoc, &scan_cfg);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("failed pmo psoc configuration");
		return -EINVAL;
	}

	return 0;
}

int hdd_update_components_config(struct hdd_context *hdd_ctx)
{
	int ret;

	ret = hdd_update_pmo_config(hdd_ctx);
	if (ret)
		return ret;

	ret = hdd_update_scan_config(hdd_ctx);
	if (ret)
		return ret;

	ret = hdd_update_tdls_config(hdd_ctx);
	if (ret)
		return ret;

	ret = hdd_update_dp_config(hdd_ctx);
	if (ret)
		return ret;

	ret = hdd_update_dfs_config(hdd_ctx);
	if (ret)
		return ret;

	ret = hdd_update_regulatory_config(hdd_ctx);
	if (ret)
		return ret;

	return ret;
}

/**
 * wlan_hdd_get_dfs_mode() - get ACS DFS mode
 * @mode : cfg80211 DFS mode
 *
 * Return: return SAP ACS DFS mode else return ACS_DFS_MODE_NONE
 */
enum  sap_acs_dfs_mode wlan_hdd_get_dfs_mode(enum dfs_mode mode)
{
	switch (mode) {
	case DFS_MODE_ENABLE:
		return ACS_DFS_MODE_ENABLE;
	case DFS_MODE_DISABLE:
		return ACS_DFS_MODE_DISABLE;
	case DFS_MODE_DEPRIORITIZE:
		return ACS_DFS_MODE_DEPRIORITIZE;
	default:
		hdd_debug("ACS dfs mode is NONE");
		return ACS_DFS_MODE_NONE;
	}
}

/**
 * hdd_enable_disable_ca_event() - enable/disable channel avoidance event
 * @hdd_ctx: pointer to hdd context
 * @set_value: enable/disable
 *
 * When Host sends vendor command enable, FW will send *ONE* CA ind to
 * Host(even though it is duplicate). When Host send vendor command
 * disable,FW doesn't perform any action. Whenever any change in
 * CA *and* WLAN is in SAP/P2P-GO mode, FW sends CA ind to host.
 *
 * return - 0 on success, appropriate error values on failure.
 */
int hdd_enable_disable_ca_event(struct hdd_context *hdd_ctx, uint8_t set_value)
{
	QDF_STATUS status;

	if (0 != wlan_hdd_validate_context(hdd_ctx))
		return -EAGAIN;

	status = sme_enable_disable_chanavoidind_event(hdd_ctx->mac_handle,
						       set_value);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Failed to send chan avoid command to SME");
		return -EINVAL;
	}
	return 0;
}

bool hdd_is_roaming_in_progress(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	uint8_t vdev_id;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IS_ROAMING_IN_PROGRESS;

	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return false;
	}

	if (!policy_mgr_is_sta_active_connection_exists(hdd_ctx->psoc))
		return false;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		vdev_id = adapter->vdev_id;
		if (adapter->device_mode == QDF_STA_MODE &&
		    test_bit(SME_SESSION_OPENED, &adapter->event_flags) &&
		    sme_roaming_in_progress(hdd_ctx->mac_handle, vdev_id)) {
			hdd_debug("Roaming is in progress on:vdev_id:%d",
				  adapter->vdev_id);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return true;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return false;
}

/**
 * struct hdd_is_connection_in_progress_priv - adapter connection info
 * @out_vdev_id: id of vdev where connection is occurring
 * @out_reason: scan reject reason
 * @connection_in_progress: true if connection is in progress
 */
struct hdd_is_connection_in_progress_priv {
	uint8_t out_vdev_id;
	enum scan_reject_states out_reason;
	bool connection_in_progress;
};

/**
 * hdd_is_connection_in_progress_iterator() - Check adapter connection based
 * on device mode
 * @adapter: current adapter of interest
 * @ctx: user context supplied
 *
 * Check if connection is in progress for the current adapter according to the
 * device mode
 *
 * Return:
 * * QDF_STATUS_SUCCESS if iteration should continue
 * * QDF_STATUS_E_ABORTED if iteration should be aborted
 */
static QDF_STATUS hdd_is_connection_in_progress_iterator(
					struct hdd_adapter *adapter,
					void *ctx)
{
	struct hdd_station_ctx *hdd_sta_ctx;
	uint8_t *sta_mac;
	struct hdd_context *hdd_ctx;
	mac_handle_t mac_handle;
	struct hdd_station_info *sta_info, *tmp = NULL;
	struct hdd_is_connection_in_progress_priv *context = ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return QDF_STATUS_E_ABORTED;

	mac_handle = hdd_ctx->mac_handle;
	if (!test_bit(SME_SESSION_OPENED, &adapter->event_flags) &&
	    (adapter->device_mode == QDF_STA_MODE ||
	     adapter->device_mode == QDF_P2P_CLIENT_MODE ||
	     adapter->device_mode == QDF_P2P_DEVICE_MODE ||
	     adapter->device_mode == QDF_P2P_GO_MODE ||
	     adapter->device_mode == QDF_SAP_MODE))
		return QDF_STATUS_SUCCESS;

	if (((QDF_STA_MODE == adapter->device_mode)
		|| (QDF_P2P_CLIENT_MODE == adapter->device_mode)
		|| (QDF_P2P_DEVICE_MODE == adapter->device_mode))
		&& hdd_cm_is_connecting(adapter)) {
		hdd_debug("%pK(%d) mode %d Connection is in progress",
			  WLAN_HDD_GET_STATION_CTX_PTR(adapter),
			  adapter->vdev_id, adapter->device_mode);

		context->out_vdev_id = adapter->vdev_id;
		context->out_reason = CONNECTION_IN_PROGRESS;
		context->connection_in_progress = true;

		return QDF_STATUS_E_ABORTED;
	}

	if ((QDF_STA_MODE == adapter->device_mode) &&
	     sme_roaming_in_progress(mac_handle, adapter->vdev_id)) {
		hdd_debug("%pK(%d) mode %d Reassociation in progress",
			  WLAN_HDD_GET_STATION_CTX_PTR(adapter),
			  adapter->vdev_id, adapter->device_mode);

		context->out_vdev_id = adapter->vdev_id;
		context->out_reason = REASSOC_IN_PROGRESS;
		context->connection_in_progress = true;
		return QDF_STATUS_E_ABORTED;
	}

	if ((QDF_STA_MODE == adapter->device_mode) ||
		(QDF_P2P_CLIENT_MODE == adapter->device_mode) ||
		(QDF_P2P_DEVICE_MODE == adapter->device_mode)) {
		hdd_sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if (hdd_cm_is_vdev_associated(adapter)
		    && sme_is_sta_key_exchange_in_progress(
		    mac_handle, adapter->vdev_id)) {
			sta_mac = (uint8_t *)&(adapter->mac_addr.bytes[0]);
			hdd_debug("client " QDF_MAC_ADDR_FMT
				  " is in middle of WPS/EAPOL exchange.",
				  QDF_MAC_ADDR_REF(sta_mac));

			context->out_vdev_id = adapter->vdev_id;
			context->out_reason = EAPOL_IN_PROGRESS;
			context->connection_in_progress = true;

			return QDF_STATUS_E_ABORTED;
		}
	} else if ((QDF_SAP_MODE == adapter->device_mode) ||
			(QDF_P2P_GO_MODE == adapter->device_mode)) {
		hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				     STA_INFO_CONNECTION_IN_PROGRESS_ITERATOR) {
			if (sta_info->peer_state !=
				OL_TXRX_PEER_STATE_CONN) {
				hdd_put_sta_info_ref(
				     &adapter->sta_info_list, &sta_info, true,
				     STA_INFO_CONNECTION_IN_PROGRESS_ITERATOR);
				continue;
			}

			sta_mac = sta_info->sta_mac.bytes;
			hdd_debug("client " QDF_MAC_ADDR_FMT
				  " of SAP/GO is in middle of WPS/EAPOL exchange",
				  QDF_MAC_ADDR_REF(sta_mac));

			context->out_vdev_id = adapter->vdev_id;
			context->out_reason = SAP_EAPOL_IN_PROGRESS;
			context->connection_in_progress = true;

			hdd_put_sta_info_ref(
				&adapter->sta_info_list, &sta_info, true,
				STA_INFO_CONNECTION_IN_PROGRESS_ITERATOR);
			if (tmp)
				hdd_put_sta_info_ref(
				    &adapter->sta_info_list, &tmp, true,
				    STA_INFO_CONNECTION_IN_PROGRESS_ITERATOR);

			return QDF_STATUS_E_ABORTED;
		}
		if (hdd_ctx->connection_in_progress) {
			hdd_debug("AP/GO: vdev %d connection is in progress",
				  adapter->vdev_id);
			context->out_reason = SAP_CONNECTION_IN_PROGRESS;
			context->out_vdev_id = adapter->vdev_id;
			context->connection_in_progress = true;

			return QDF_STATUS_E_ABORTED;
		}
	}

	if (ucfg_nan_is_enable_disable_in_progress(hdd_ctx->psoc)) {
		context->out_reason = NAN_ENABLE_DISABLE_IN_PROGRESS;
		context->out_vdev_id = NAN_PSEUDO_VDEV_ID;
		context->connection_in_progress = true;

		return QDF_STATUS_E_ABORTED;
	}

	return QDF_STATUS_SUCCESS;
}

bool hdd_is_connection_in_progress(uint8_t *out_vdev_id,
				   enum scan_reject_states *out_reason)
{
	struct hdd_is_connection_in_progress_priv hdd_conn;
	hdd_adapter_iterate_cb cb;

	hdd_conn.out_vdev_id = 0;
	hdd_conn.out_reason = SCAN_REJECT_DEFAULT;
	hdd_conn.connection_in_progress = false;

	cb = hdd_is_connection_in_progress_iterator;

	hdd_adapter_iterate(cb, &hdd_conn);

	if (hdd_conn.connection_in_progress && out_vdev_id && out_reason) {
		*out_vdev_id = hdd_conn.out_vdev_id;
		*out_reason = hdd_conn.out_reason;
	}

	return hdd_conn.connection_in_progress;
}

/**
 * hdd_restart_sap() - to restart SAP in driver internally
 * @ap_adapter: Pointer to SAP struct hdd_adapter structure
 *
 * Return: None
 */
void hdd_restart_sap(struct hdd_adapter *ap_adapter)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct hdd_hostapd_state *hostapd_state;
	QDF_STATUS qdf_status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	struct sap_config *sap_config;
	void *sap_ctx;

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	sap_config = &hdd_ap_ctx->sap_config;
	sap_ctx = hdd_ap_ctx->sap_context;

	mutex_lock(&hdd_ctx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		wlan_hdd_del_station(ap_adapter, NULL);
		hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
		qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
		if (QDF_STATUS_SUCCESS == wlansap_stop_bss(sap_ctx)) {
			qdf_status = qdf_wait_single_event(
					&hostapd_state->qdf_stop_bss_event,
					SME_CMD_STOP_BSS_TIMEOUT);

			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hdd_err("SAP Stop Failed");
				goto end;
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
			ap_adapter->device_mode, ap_adapter->vdev_id);
		hdd_green_ap_start_state_mc(hdd_ctx, ap_adapter->device_mode,
					    false);
		hdd_err("SAP Stop Success");

		if (0 != wlan_hdd_cfg80211_update_apies(ap_adapter)) {
			hdd_err("SAP Not able to set AP IEs");
			goto end;
		}

		qdf_status = wlan_hdd_mlo_sap_reinit(hdd_ctx, sap_config,
						     ap_adapter);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			hdd_err("SAP Not able to do mlo attach");
			goto deinit_mlo;
		}

		qdf_event_reset(&hostapd_state->qdf_event);
		if (wlansap_start_bss(sap_ctx, hdd_hostapd_sap_event_cb,
				      sap_config,
				      ap_adapter->dev) != QDF_STATUS_SUCCESS) {
			hdd_err("SAP Start Bss fail");
			goto deinit_mlo;
		}

		hdd_info("Waiting for SAP to start");
		qdf_status =
			qdf_wait_single_event(&hostapd_state->qdf_event,
					SME_CMD_START_BSS_TIMEOUT);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("SAP Start failed");
			goto deinit_mlo;
		}
		wlansap_reset_sap_config_add_ie(sap_config,
						eUPDATE_IE_ALL);
		hdd_err("SAP Start Success");
		hdd_medium_assess_init();
		set_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		if (hostapd_state->bss_state == BSS_START) {
			policy_mgr_incr_active_session(hdd_ctx->psoc,
						ap_adapter->device_mode,
						ap_adapter->vdev_id);
			hdd_green_ap_start_state_mc(hdd_ctx,
						    ap_adapter->device_mode,
						    true);
		}
	}
	mutex_unlock(&hdd_ctx->sap_lock);
	return;

deinit_mlo:
	wlan_hdd_mlo_reset(ap_adapter);
end:
	wlansap_reset_sap_config_add_ie(sap_config, eUPDATE_IE_ALL);
	mutex_unlock(&hdd_ctx->sap_lock);
}

/**
 * hdd_check_and_restart_sap_with_non_dfs_acs() - Restart SAP
 * with non dfs acs
 *
 * Restarts SAP in non-DFS ACS mode when STA-AP mode DFS is not supported
 *
 * Return: None
 */
void hdd_check_and_restart_sap_with_non_dfs_acs(void)
{
	struct hdd_adapter *ap_adapter;
	struct hdd_context *hdd_ctx;
	struct cds_context *cds_ctx;
	uint8_t restart_chan;
	uint32_t restart_freq;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx)
		return;

	if (policy_mgr_get_concurrency_mode(hdd_ctx->psoc)
		!= (QDF_STA_MASK | QDF_SAP_MASK)) {
		hdd_debug("Concurrency mode is not SAP");
		return;
	}

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (ap_adapter &&
	    test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags) &&
	    wlan_reg_is_dfs_for_freq(
			hdd_ctx->pdev,
			ap_adapter->session.ap.operating_chan_freq)) {
		if (policy_mgr_get_dfs_master_dynamic_enabled(
				hdd_ctx->psoc, ap_adapter->vdev_id))
			return;
		hdd_warn("STA-AP Mode DFS not supported, Switch SAP channel to Non DFS");

		restart_freq =
			wlansap_get_safe_channel_from_pcl_and_acs_range(
				WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter));

		restart_chan = wlan_reg_freq_to_chan(hdd_ctx->pdev,
						     restart_freq);
		if (!restart_chan ||
		    wlan_reg_is_dfs_for_freq(hdd_ctx->pdev, restart_freq))
			restart_chan = SAP_DEFAULT_5GHZ_CHANNEL;
		wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc, ap_adapter->vdev_id,
					CSA_REASON_STA_CONNECT_DFS_TO_NON_DFS);
		hdd_switch_sap_channel(ap_adapter, restart_chan, true);
	}
}

/**
 * hdd_set_connection_in_progress() - to set the connection in
 * progress flag
 * @value: value to set
 *
 * This function will set the passed value to connection in progress flag.
 * If value is previously being set to true then no need to set it again.
 *
 * Return: true if value is being set correctly and false otherwise.
 */
bool hdd_set_connection_in_progress(bool value)
{
	bool status = true;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return false;

	qdf_spin_lock(&hdd_ctx->connection_status_lock);
	/*
	 * if the value is set to true previously and if someone is
	 * trying to make it true again then it could be some race
	 * condition being triggered. Avoid this situation by returning
	 * false
	 */
	if (hdd_ctx->connection_in_progress && value)
		status = false;
	else
		hdd_ctx->connection_in_progress = value;
	qdf_spin_unlock(&hdd_ctx->connection_status_lock);
	return status;
}

int wlan_hdd_send_mcc_vdev_quota(struct hdd_adapter *adapter, int set_value)
{
	if (!adapter) {
		hdd_err("Invalid adapter");
		return -EINVAL;
	}
	hdd_info("send mcc vdev quota to fw: %d", set_value);
	sme_cli_set_command(adapter->vdev_id,
			    WMA_VDEV_MCC_SET_TIME_QUOTA,
			    set_value, VDEV_CMD);
	return 0;

}

int wlan_hdd_send_mcc_latency(struct hdd_adapter *adapter, int set_value)
{
	if (!adapter) {
		hdd_err("Invalid adapter");
		return -EINVAL;
	}

	hdd_info("Send MCC latency WMA: %d", set_value);
	sme_cli_set_command(adapter->vdev_id,
			    WMA_VDEV_MCC_SET_TIME_LATENCY,
			    set_value, VDEV_CMD);
	return 0;
}

struct hdd_adapter *wlan_hdd_get_adapter_from_vdev(struct wlan_objmgr_psoc
					      *psoc, uint8_t vdev_id)
{
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	/*
	 * Currently PSOC is not being used. But this logic will
	 * change once we have the converged implementation of
	 * HDD context per PSOC in place. This would break if
	 * multiple vdev objects reuse the vdev id.
	 */
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter)
		hdd_err("Get adapter by vdev id failed");

	return adapter;
}

int hdd_get_rssi_snr_by_bssid(struct hdd_adapter *adapter, const uint8_t *bssid,
			      int8_t *rssi, int8_t *snr)
{
	QDF_STATUS status;
	mac_handle_t mac_handle;

	mac_handle = hdd_adapter_get_mac_handle(adapter);
	status = sme_get_rssi_snr_by_bssid(mac_handle, bssid, rssi, snr);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_debug("sme_get_rssi_snr_by_bssid failed");
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_reset_limit_off_chan() - reset limit off-channel command parameters
 * @adapter: HDD adapter
 *
 * Return: 0 on success and non zero value on failure
 */
int hdd_reset_limit_off_chan(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	int ret;
	QDF_STATUS status;
	uint8_t sys_pref = 0;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret < 0)
		return ret;

	ucfg_policy_mgr_get_sys_pref(hdd_ctx->psoc,
				     &sys_pref);
	/* set the system preferece to default */
	policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc, sys_pref);

	/* clear the bitmap */
	adapter->active_ac = 0;

	hdd_debug("reset ac_bitmap for session %hu active_ac %0x",
		  adapter->vdev_id, adapter->active_ac);

	status = sme_send_limit_off_channel_params(hdd_ctx->mac_handle,
						   adapter->vdev_id,
						   false, 0, 0, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("failed to reset limit off chan params");
		ret = -EINVAL;
	}

	return ret;
}

void hdd_hidden_ssid_enable_roaming(hdd_handle_t hdd_handle, uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	struct hdd_adapter *adapter;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);

	if (!adapter) {
		hdd_err("Adapter is null");
		return;
	}
	/* enable roaming on all adapters once hdd get hidden ssid rsp */
	wlan_hdd_enable_roaming(adapter, RSO_START_BSS);
}

#ifdef WLAN_FEATURE_PKT_CAPTURE
bool wlan_hdd_is_mon_concurrency(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return -EINVAL;

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
		if (policy_mgr_get_concurrency_mode(hdd_ctx->psoc) ==
		    (QDF_STA_MASK | QDF_MONITOR_MASK)) {
			hdd_err("STA + MON mode is UP");
			return true;
		}
	}
	return false;
}

void wlan_hdd_del_monitor(struct hdd_context *hdd_ctx,
			  struct hdd_adapter *adapter, bool rtnl_held)
{
	wlan_hdd_release_intf_addr(hdd_ctx, adapter->mac_addr.bytes);
	hdd_stop_adapter(hdd_ctx, adapter);
	hdd_close_adapter(hdd_ctx, adapter, true);

	hdd_open_p2p_interface(hdd_ctx);
}

void
wlan_hdd_del_p2p_interface(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	struct osif_vdev_sync *vdev_sync;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_DEL_P2P_INTERFACE) {
		if (adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			vdev_sync = osif_vdev_sync_unregister(adapter->dev);
			if (vdev_sync)
				osif_vdev_sync_wait_for_ops(vdev_sync);

			hdd_adapter_dev_put_debug(
				adapter, NET_DEV_HOLD_DEL_P2P_INTERFACE);

			hdd_clean_up_interface(hdd_ctx, adapter);

			if (vdev_sync)
				osif_vdev_sync_destroy(vdev_sync);
		} else
			hdd_adapter_dev_put_debug(
				adapter, NET_DEV_HOLD_DEL_P2P_INTERFACE);
	}
}

#endif /* WLAN_FEATURE_PKT_CAPTURE */

bool wlan_hdd_is_session_type_monitor(uint8_t session_type)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return false;
	}

	if (cds_get_conparam() != QDF_GLOBAL_MONITOR_MODE &&
	    session_type == QDF_MONITOR_MODE)
		return true;
	else
		return false;
}

int
wlan_hdd_add_monitor_check(struct hdd_context *hdd_ctx,
			   struct hdd_adapter **adapter,
			   const char *name, bool rtnl_held,
			   unsigned char name_assign_type)
{
	struct hdd_adapter *sta_adapter;
	struct hdd_adapter *mon_adapter;
	uint8_t num_open_session = 0;
	QDF_STATUS status;
	struct hdd_adapter_create_param params = {0};

	sta_adapter = hdd_get_adapter(hdd_ctx, QDF_STA_MODE);
	if (!sta_adapter) {
		hdd_err("No station adapter");
		return -EINVAL;
	}

	status = policy_mgr_check_mon_concurrency(hdd_ctx->psoc);

	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	if (hdd_is_connection_in_progress(NULL, NULL)) {
		hdd_err("cannot add monitor mode, Connection in progress");
		return -EINVAL;
	}

	if (ucfg_mlme_is_sta_mon_conc_supported(hdd_ctx->psoc)) {
		num_open_session = policy_mgr_mode_specific_connection_count(
						hdd_ctx->psoc,
						PM_STA_MODE,
						NULL);

		if (num_open_session == 1) {
			hdd_ctx->disconnect_for_sta_mon_conc = true;
			/* Try disconnecting if already in connected state */
			wlan_hdd_cm_issue_disconnect(sta_adapter,
						     REASON_UNSPEC_FAILURE,
						     true);
		}
	}

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE)
		wlan_hdd_del_p2p_interface(hdd_ctx);

	params.is_add_virtual_iface = 1;

	mon_adapter = hdd_open_adapter(hdd_ctx, QDF_MONITOR_MODE, name,
				       wlan_hdd_get_intf_addr(
				       hdd_ctx,
				       QDF_MONITOR_MODE),
				       name_assign_type, rtnl_held, &params);
	if (!mon_adapter) {
		hdd_err("hdd_open_adapter failed");
		if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE)
			hdd_open_p2p_interface(hdd_ctx);
		return -EINVAL;
	}

	if (mon_adapter)
		hdd_set_idle_ps_config(hdd_ctx, false);

	*adapter = mon_adapter;
	return 0;
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT

void hdd_sme_monitor_mode_callback(uint8_t vdev_id)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err_rl("NULL adapter");
		return;
	}

	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err_rl("Invalid magic");
		return;
	}

	if (adapter->magic == WLAN_HDD_ADAPTER_MAGIC)
		qdf_event_set(&adapter->qdf_monitor_mode_vdev_up_event);

	hdd_debug("monitor mode vdev up completed");
	adapter->monitor_mode_vdev_up_in_progress = false;
}

QDF_STATUS hdd_monitor_mode_qdf_create_event(struct hdd_adapter *adapter,
					     uint8_t session_type)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (session_type == QDF_MONITOR_MODE) {
		qdf_status = qdf_event_create(
				&adapter->qdf_monitor_mode_vdev_up_event);
	}
	return qdf_status;
}

QDF_STATUS hdd_monitor_mode_vdev_status(struct hdd_adapter *adapter)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!adapter->monitor_mode_vdev_up_in_progress)
		return status;

	/* block on a completion variable until vdev up success*/
	status = qdf_wait_for_event_completion(
				       &adapter->qdf_monitor_mode_vdev_up_event,
					WLAN_MONITOR_MODE_VDEV_UP_EVT);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("monitor mode vdev up event time out vdev id: %d",
			   adapter->vdev_id);
		if (adapter->qdf_monitor_mode_vdev_up_event.force_set)
			/*
			 * SSR/PDR has caused shutdown, which has
			 * forcefully set the event.
			 */
			hdd_err_rl("monitor mode vdev up event forcefully set");
		else if (status == QDF_STATUS_E_TIMEOUT)
			hdd_err_rl("mode vdev up event timed out");
		else
			hdd_err_rl("Failed to wait for monitor vdev up(status-%d)",
				   status);

		adapter->monitor_mode_vdev_up_in_progress = false;
		return status;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
void hdd_beacon_latency_event_cb(uint32_t latency_level)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	if (latency_level ==
		QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_ULTRALOW)
		wlan_hdd_set_pm_qos_request(hdd_ctx, true);
	else
		wlan_hdd_set_pm_qos_request(hdd_ctx, false);
}
#endif

#ifdef CONFIG_WLAN_DEBUG_CRASH_INJECT
int hdd_crash_inject(struct hdd_adapter *adapter, uint32_t v1, uint32_t v2)
{
	struct hdd_context *hdd_ctx;
	int ret;
	bool crash_inject;
	QDF_STATUS status;

	hdd_debug("v1: %d v2: %d", v1, v2);
	pr_err("SSR is triggered by CRASH_INJECT: %d %d\n",
	       v1, v2);
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	status = ucfg_mlme_get_crash_inject(hdd_ctx->psoc, &crash_inject);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get crash inject ini config");
		return 0;
	}

	if (!crash_inject) {
		hdd_err("Crash Inject ini disabled, Ignore Crash Inject");
		return 0;
	}

	if (v1 == 3) {
		cds_trigger_recovery(QDF_REASON_UNSPECIFIED);
		return 0;
	}
	ret = wma_cli_set2_command(adapter->vdev_id,
				   GEN_PARAM_CRASH_INJECT,
				   v1, v2, GEN_CMD);
	return ret;
}
#endif

static const struct hdd_chwidth_info chwidth_info[] = {
	[NL80211_CHAN_WIDTH_20_NOHT] = {
		.ch_bw = HW_MODE_20_MHZ,
		.ch_bw_str = "20MHz",
		.phy_chwidth = CH_WIDTH_20MHZ,
	},
	[NL80211_CHAN_WIDTH_20] = {
		.sir_chwidth_valid = true,
		.sir_chwidth = eHT_CHANNEL_WIDTH_20MHZ,
		.ch_bw = HW_MODE_20_MHZ,
		.ch_bw_str = "20MHz",
		.phy_chwidth = CH_WIDTH_20MHZ,
		.bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE,
	},
	[NL80211_CHAN_WIDTH_40] = {
		.sir_chwidth_valid = true,
		.sir_chwidth = eHT_CHANNEL_WIDTH_40MHZ,
		.ch_bw = HW_MODE_40_MHZ,
		.ch_bw_str = "40MHz",
		.phy_chwidth = CH_WIDTH_40MHZ,
		.bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE,
	},
	[NL80211_CHAN_WIDTH_80] = {
		.sir_chwidth_valid = true,
		.sir_chwidth = eHT_CHANNEL_WIDTH_80MHZ,
		.ch_bw = HW_MODE_80_MHZ,
		.ch_bw_str = "80MHz",
		.phy_chwidth = CH_WIDTH_80MHZ,
		.bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE,
	},
	[NL80211_CHAN_WIDTH_80P80] = {
		.sir_chwidth_valid = true,
		.sir_chwidth = eHT_CHANNEL_WIDTH_80P80MHZ,
		.ch_bw = HW_MODE_80_PLUS_80_MHZ,
		.ch_bw_str = "(80 + 80)MHz",
		.phy_chwidth = CH_WIDTH_80P80MHZ,
		.bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE,
	},
	[NL80211_CHAN_WIDTH_160] = {
		.sir_chwidth_valid = true,
		.sir_chwidth = eHT_CHANNEL_WIDTH_160MHZ,
		.ch_bw = HW_MODE_160_MHZ,
		.ch_bw_str = "160MHz",
		.phy_chwidth = CH_WIDTH_160MHZ,
		.bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE,
	},
	[NL80211_CHAN_WIDTH_5] = {
		.ch_bw = HW_MODE_5_MHZ,
		.ch_bw_str = "5MHz",
		.phy_chwidth = CH_WIDTH_5MHZ,
	},
	[NL80211_CHAN_WIDTH_10] = {
		.ch_bw = HW_MODE_10_MHZ,
		.ch_bw_str = "10MHz",
		.phy_chwidth = CH_WIDTH_10MHZ,
	},
#if defined(WLAN_FEATURE_11BE) && defined(CFG80211_11BE_BASIC)
	[NL80211_CHAN_WIDTH_320] = {
		.sir_chwidth_valid = true,
		.sir_chwidth = eHT_CHANNEL_WIDTH_320MHZ,
		.ch_bw = HW_MODE_320_MHZ,
		.ch_bw_str = "320MHz",
		.phy_chwidth = CH_WIDTH_320MHZ,
		.bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE,
	},
#endif
};

enum eSirMacHTChannelWidth
hdd_nl80211_chwidth_to_chwidth(uint8_t nl80211_chwidth)
{
	if (nl80211_chwidth >= ARRAY_SIZE(chwidth_info) ||
	    !chwidth_info[nl80211_chwidth].sir_chwidth_valid) {
		hdd_err("Unsupported channel width %d", nl80211_chwidth);
		return -EINVAL;
	}

	return chwidth_info[nl80211_chwidth].sir_chwidth;
}

uint8_t hdd_chwidth_to_nl80211_chwidth(enum eSirMacHTChannelWidth chwidth)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(chwidth_info); i++) {
		if (chwidth_info[i].sir_chwidth_valid &&
		    chwidth_info[i].sir_chwidth == chwidth)
			return i;
	}

	hdd_err("Unsupported channel width %d", chwidth);
	return 0xFF;
}

enum hw_mode_bandwidth wlan_hdd_get_channel_bw(enum nl80211_chan_width width)
{
	if (width >= ARRAY_SIZE(chwidth_info)) {
		hdd_err("Invalid width: %d, using default 20MHz", width);
		return HW_MODE_20_MHZ;
	}

	return chwidth_info[width].ch_bw;
}

uint8_t *hdd_ch_width_str(enum phy_ch_width ch_width)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(chwidth_info); i++) {
		if (chwidth_info[i].phy_chwidth == ch_width)
			return chwidth_info[i].ch_bw_str;
	}

	return "UNKNOWN";
}

int hdd_we_set_ch_width(struct hdd_adapter *adapter, int ch_width)
{
	int i;

	/* updating channel bonding only on 5Ghz */
	hdd_debug("wmi_vdev_param_chwidth val %d", ch_width);

	for (i = 0; i < ARRAY_SIZE(chwidth_info); i++) {
		if (chwidth_info[i].sir_chwidth_valid &&
		    chwidth_info[i].sir_chwidth == ch_width)
			return hdd_update_channel_width(
					adapter, ch_width,
					chwidth_info[i].bonding_mode);
	}

	hdd_err("Invalid ch_width %d", ch_width);
	return -EINVAL;
}

/* Register the module init/exit functions */
module_init(hdd_module_init);
module_exit(hdd_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Qualcomm Atheros, Inc.");
MODULE_DESCRIPTION("WLAN HOST DEVICE DRIVER");

const struct kernel_param_ops con_mode_ops = {
	.set = con_mode_handler,
	.get = param_get_int,
};

#ifdef FEATURE_WLAN_RESIDENT_DRIVER
EXPORT_SYMBOL(con_mode_ops);
#endif

const struct kernel_param_ops con_mode_ftm_ops = {
	.set = con_mode_handler_ftm,
	.get = param_get_int,
};

#ifdef FEATURE_WLAN_RESIDENT_DRIVER
EXPORT_SYMBOL(con_mode_ftm_ops);
#endif

#ifdef WLAN_FEATURE_EPPING
static const struct kernel_param_ops con_mode_epping_ops = {
	.set = con_mode_handler_epping,
	.get = param_get_int,
};
#endif

static const struct kernel_param_ops fwpath_ops = {
	.set = fwpath_changed_handler,
	.get = param_get_string,
};

static int __pcie_set_gen_speed_handler(void)
{
	int ret;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	hdd_info_rl("Received PCIe gen speed %d", pcie_gen_speed);
	if (pcie_gen_speed <= HDD_INVALID_MIN_PCIE_GEN_SPEED ||
	    pcie_gen_speed >= HDD_INVALID_MAX_PCIE_GEN_SPEED) {
		hdd_err_rl("invalid pcie gen speed %d", pcie_gen_speed);
		return -EINVAL;
	}

	hdd_ctx->current_pcie_gen_speed = pcie_gen_speed;

	return 0;
}

static int pcie_set_gen_speed_handler(const char *kmessage,
				      const struct kernel_param *kp)
{
	struct osif_driver_sync *driver_sync;
	int ret;

	ret = osif_driver_sync_op_start(&driver_sync);
	if (ret)
		return ret;

	ret = param_set_int(kmessage, kp);
	if (ret) {
		hdd_err_rl("param set int failed %d", ret);
		goto out;
	}

	ret = __pcie_set_gen_speed_handler();

out:
	osif_driver_sync_op_stop(driver_sync);

	return ret;
}

void hdd_wait_for_dp_tx(void)
{
	int count = MAX_SSR_WAIT_ITERATIONS;
	int r;

	hdd_enter();

	while (count) {
		r = atomic_read(&dp_protect_entry_count);

		if (!r)
			break;

		if (--count) {
			hdd_err_rl("Waiting for Packet tx to complete: %d",
				   count);
			msleep(SSR_WAIT_SLEEP_TIME);
		}
	}

	if (!count)
		hdd_err("Timed-out waiting for packet tx");

	hdd_exit();
}

static const struct kernel_param_ops pcie_gen_speed_ops = {
	.set = pcie_set_gen_speed_handler,
	.get = param_get_int,
};

module_param_cb(con_mode, &con_mode_ops, &con_mode,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param_cb(con_mode_ftm, &con_mode_ftm_ops, &con_mode_ftm,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param_cb(pcie_gen_speed, &pcie_gen_speed_ops, &pcie_gen_speed,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

#ifdef WLAN_FEATURE_EPPING
module_param_cb(con_mode_epping, &con_mode_epping_ops,
		&con_mode_epping, 0644);
#endif

module_param_cb(fwpath, &fwpath_ops, &fwpath,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param(enable_dfs_chan_scan, int, S_IRUSR | S_IRGRP | S_IROTH);

module_param(enable_11d, int, S_IRUSR | S_IRGRP | S_IROTH);

module_param(country_code, charp, S_IRUSR | S_IRGRP | S_IROTH);

static int timer_multiplier_get_handler(char *buffer,
					const struct kernel_param *kp)
{
	return scnprintf(buffer, PAGE_SIZE, "%u", qdf_timer_get_multiplier());
}

static int timer_multiplier_set_handler(const char *kmessage,
					const struct kernel_param *kp)
{
	QDF_STATUS status;
	uint32_t scalar;

	status = qdf_uint32_parse(kmessage, &scalar);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	if (!cfg_in_range(CFG_TIMER_MULTIPLIER, scalar))
		return -ERANGE;

	qdf_timer_set_multiplier(scalar);

	return 0;
}

static const struct kernel_param_ops timer_multiplier_ops = {
	.get = timer_multiplier_get_handler,
	.set = timer_multiplier_set_handler,
};

module_param_cb(timer_multiplier, &timer_multiplier_ops, NULL, 0644);
