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

/**
 *  DOC: wlan_hdd_main.c
 *
 *  WLAN Host Device Driver implementation
 *
 */

/* Include Files */
#include <wbuff.h>
#include <wlan_hdd_includes.h>
#include <cds_api.h>
#include <cds_sched.h>
#include <linux/cpu.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <wlan_hdd_tx_rx.h>
#include <wni_api.h>
#include <wlan_hdd_cfg.h>
#include <wlan_ptt_sock_svc.h>
#include <dbglog_host.h>
#include <wlan_logging_sock_svc.h>
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_wext.h>
#include "wlan_hdd_trace.h"
#include "wlan_hdd_ioctl.h"
#include "wlan_hdd_ftm.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_stats.h"
#include "wlan_hdd_periodic_sta_stats.h"
#include "wlan_hdd_scan.h"
#include <wlan_osif_request_manager.h>
#ifdef CONFIG_LEAK_DETECTION
#include "qdf_debug_domain.h"
#endif
#include "qdf_str.h"
#include "qdf_trace.h"
#include "qdf_types.h"
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_misc.h>
#include <cdp_txrx_stats.h>
#include "cdp_txrx_flow_ctrl_legacy.h"

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
#ifdef WLAN_FEATURE_ETHTOOL
#include <linux/ethtool.h>
#endif
#ifdef MSM_PLATFORM
#include <soc/qcom/subsystem_restart.h>
#endif
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_green_ap.h>
#include "cfg_api.h"
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
#include "nan_api.h"
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
#include "wlan_mlme_ucfg_api.h"
#include <wlan_hdd_debugfs_coex.h>
#include "qdf_func_tracker.h"

#ifdef CNSS_GENL
#include <net/cnss_nl.h>
#endif
#include "wlan_reg_ucfg_api.h"
#include "wlan_ocb_ucfg_api.h"
#include <wlan_hdd_spectralscan.h>
#include <wlan_p2p_ucfg_api.h>
#include "wlan_osif_priv.h"
#include "wlan_pkt_capture_ucfg_api.h"
#include "ftm_time_sync_ucfg_api.h"
#include <wlan_hdd_hang_event.h>

#ifdef MODULE
#define WLAN_MODULE_NAME  module_name(THIS_MODULE)
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

int wlan_start_ret_val;
static DECLARE_COMPLETION(wlan_start_comp);
static qdf_atomic_t wlan_hdd_state_fops_ref;
static unsigned int dev_num = 1;
static struct cdev wlan_hdd_state_cdev;
static struct class *class;
static dev_t device;
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
#endif

#define HDD_OPS_INACTIVITY_TIMEOUT (120000)
#define MAX_OPS_NAME_STRING_SIZE 20
#define RATE_LIMIT_ERROR_LOG (256)

static qdf_timer_t hdd_drv_ops_inactivity_timer;
static struct task_struct *hdd_drv_ops_task;
static char drv_ops_string[MAX_OPS_NAME_STRING_SIZE];

/* the Android framework expects this param even though we don't use it */
#define BUF_LEN 20
static char fwpath_buffer[BUF_LEN];
static struct kparam_string fwpath = {
	.string = fwpath_buffer,
	.maxlen = BUF_LEN,
};

static char *country_code;
static int enable_11d = -1;
static int enable_dfs_chan_scan = -1;
static bool is_mode_change_psoc_idle_shutdown;

/*
 * spinlock for synchronizing asynchronous request/response
 * (full description of use in wlan_hdd_main.h)
 */
DEFINE_SPINLOCK(hdd_context_lock);
DEFINE_MUTEX(hdd_init_deinit_lock);

#define WLAN_NLINK_CESIUM 30

static qdf_wake_lock_t wlan_wake_lock;

#define WOW_MAX_FILTER_LISTS 1
#define WOW_MAX_FILTERS_PER_LIST 4
#define WOW_MIN_PATTERN_SIZE 6
#define WOW_MAX_PATTERN_SIZE 64

/* max peer can be tdls peers + self peer + bss peer */
#define HDD_MAX_VDEV_PEER_COUNT  (HDD_MAX_NUM_TDLS_STA + 2)
#define IS_IDLE_STOP (!cds_is_driver_unloading() && \
		      !cds_is_driver_recovering() && !cds_is_driver_loading())

#define HDD_FW_VER_MAJOR_SPID(tgt_fw_ver)     ((tgt_fw_ver & 0xf0000000) >> 28)
#define HDD_FW_VER_MINOR_SPID(tgt_fw_ver)     ((tgt_fw_ver & 0xf000000) >> 24)
#define HDD_FW_VER_SIID(tgt_fw_ver)           ((tgt_fw_ver & 0xf00000) >> 20)
#define HDD_FW_VER_CRM_ID(tgt_fw_ver)         (tgt_fw_ver & 0x7fff)
#define HDD_FW_VER_SUB_ID(tgt_fw_ver_ext) \
((tgt_fw_ver_ext & 0xf0000000) >> 28)
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
	[QDF_MODULE_ID_QDF_DEVICE] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CFG] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_BMI] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_EPPING] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_QVIT] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_DP] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SOC] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_OS_IF] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_TARGET_IF] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SCHEDULER] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_MGMT_TXRX] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_PMO] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_SCAN] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_MLME] = {QDF_TRACE_LEVEL_ALL},
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
	[QDF_MODULE_ID_WIFIPOS]  = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_TARGET] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_CP_STATS] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_NAN] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_PKT_CAPTURE] = {QDF_TRACE_LEVEL_ALL},
	[QDF_MODULE_ID_FTM_TIME_SYNC] = {QDF_TRACE_LEVEL_ALL},
};

int limit_off_chan_tbl[HDD_MAX_AC][HDD_MAX_OFF_CHAN_ENTRIES] = {
	{ HDD_AC_BK_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_BK },
	{ HDD_AC_BE_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_BE },
	{ HDD_AC_VI_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_VI },
	{ HDD_AC_VO_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_VO },
};

struct notifier_block hdd_netdev_notifier;

struct sock *cesium_nl_srv_sock;
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
static void wlan_hdd_auto_shutdown_cb(void);
#endif

void hdd_start_complete(int ret)
{
	wlan_start_ret_val = ret;

	complete(&wlan_start_comp);
}

/**
 * hdd_set_rps_cpu_mask - set RPS CPU mask for interfaces
 * @hdd_ctx: pointer to struct hdd_context
 *
 * Return: none
 */
static void hdd_set_rps_cpu_mask(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter)
		hdd_send_rps_ind(adapter);
}

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

	if (!hdd_ctx) {
		hdd_err("hdd ctx is NULL");
		return;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);

	wlan_hdd_netif_queue_control(adapter, action, reason);
}

/*
 * Store WLAN driver version and timestamp info in global variables such that
 * crash debugger can extract them from driver debug symbol and crashdump for
 * post processing
 */
#ifdef BUILD_TAG
uint8_t g_wlan_driver_version[] = QWLAN_VERSIONSTR TIMER_MANAGER_STR MEMORY_DEBUG_STR "; " BUILD_TAG;
#else
uint8_t g_wlan_driver_version[] = QWLAN_VERSIONSTR TIMER_MANAGER_STR MEMORY_DEBUG_STR;
#endif

/**
 * hdd_device_mode_to_string() - return string conversion of device mode
 * @device_mode: device mode
 *
 * This utility function helps log string conversion of device mode.
 *
 * Return: string conversion of device mode, if match found;
 *	   "Unknown" otherwise.
 */
const char *hdd_device_mode_to_string(uint8_t device_mode)
{
	switch (device_mode) {
	CASE_RETURN_STRING(QDF_STA_MODE);
	CASE_RETURN_STRING(QDF_SAP_MODE);
	CASE_RETURN_STRING(QDF_P2P_CLIENT_MODE);
	CASE_RETURN_STRING(QDF_P2P_GO_MODE);
	CASE_RETURN_STRING(QDF_FTM_MODE);
	CASE_RETURN_STRING(QDF_IBSS_MODE);
	CASE_RETURN_STRING(QDF_P2P_DEVICE_MODE);
	CASE_RETURN_STRING(QDF_OCB_MODE);
	CASE_RETURN_STRING(QDF_NDI_MODE);
	default:
		return "Unknown";
	}
}

/**
 * hdd_get_valid_chan() - return current chan list from regulatory.
 * @hdd_ctx: HDD context
 * @chan_list: buf hold returned chan list
 * @chan_num: input buf size and output returned chan num
 *
 * This function helps get current available chan list from regulatory
 * module. It excludes the "disabled" and "invalid" channels.
 *
 * Return: 0 for success.
 */
static int hdd_get_valid_chan(struct hdd_context *hdd_ctx,
			      uint8_t *chan_list,
			      uint32_t *chan_num)
{
	int i = 0, j = 0;
	struct regulatory_channel *cur_chan_list;
	struct wlan_objmgr_pdev *pdev;

	if (!hdd_ctx || !hdd_ctx->pdev || !chan_list || !chan_num)
		return -EINVAL;

	pdev = hdd_ctx->pdev;
	cur_chan_list = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(struct regulatory_channel));
	if (!cur_chan_list)
		return -ENOMEM;

	if (wlan_reg_get_current_chan_list(pdev, cur_chan_list) !=
					   QDF_STATUS_SUCCESS) {
		qdf_mem_free(cur_chan_list);
		return -EINVAL;
	}

	for (i = 0; i < NUM_CHANNELS; i++) {
		uint32_t ch = cur_chan_list[i].chan_num;
		enum channel_state state = wlan_reg_get_channel_state(pdev,
								      ch);

		if (state != CHANNEL_STATE_DISABLE &&
		    state != CHANNEL_STATE_INVALID &&
		    j < *chan_num) {
			chan_list[j] = (uint8_t)ch;
			j++;
		}
	}
	*chan_num = j;
	qdf_mem_free(cur_chan_list);
	return 0;
}

/**
 * hdd_validate_channel_and_bandwidth() - Validate the channel-bandwidth combo
 * @adapter: HDD adapter
 * @chan_number: Channel number
 * @chan_bw: Bandwidth
 *
 * Checks if the given bandwidth is valid for the given channel number.
 *
 * Return: 0 for success, non-zero for failure
 */
int hdd_validate_channel_and_bandwidth(struct hdd_adapter *adapter,
		uint32_t chan_number,
		enum phy_ch_width chan_bw)
{
	uint8_t chan[NUM_CHANNELS];
	uint32_t len = NUM_CHANNELS, i;
	bool found = false;
	mac_handle_t mac_handle;
	int ret;

	mac_handle = hdd_adapter_get_mac_handle(adapter);
	if (!mac_handle) {
		hdd_err("Invalid MAC handle");
		return -EINVAL;
	}

	ret = hdd_get_valid_chan(adapter->hdd_ctx, chan,
				 &len);
	if (ret) {
		hdd_err("error %d in getting valid channel list", ret);
		return ret;
	}

	for (i = 0; i < len; i++) {
		if (chan[i] == chan_number) {
			found = true;
			break;
		}
	}

	if (found == false) {
		hdd_err("Channel not in driver's valid channel list");
		return -EOPNOTSUPP;
	}

	if ((!WLAN_REG_IS_24GHZ_CH(chan_number)) &&
			(!WLAN_REG_IS_5GHZ_CH(chan_number))) {
		hdd_err("CH %d is not in 2.4GHz or 5GHz", chan_number);
		return -EINVAL;
	}

	if (WLAN_REG_IS_24GHZ_CH(chan_number)) {
		if (chan_bw == CH_WIDTH_80MHZ) {
			hdd_err("BW80 not possible in 2.4GHz band");
			return -EINVAL;
		}
		if ((chan_bw != CH_WIDTH_20MHZ) && (chan_number == 14) &&
				(chan_bw != CH_WIDTH_MAX)) {
			hdd_err("Only BW20 possible on channel 14");
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * hdd_wait_for_recovery_completion() - Wait for cds recovery completion
 *
 * Block the unloading of the driver (or) interface up until the
 * cds recovery is completed
 *
 * Return: true for recovery completion else false
 */
static bool hdd_wait_for_recovery_completion(void)
{
	int retry = 0;

	/* Wait for recovery to complete */
	while (cds_is_driver_recovering()) {
		if (retry == HDD_MOD_EXIT_SSR_MAX_RETRIES/2)
			hdd_err("Recovery in progress; wait here!!!");

		msleep(1000);
		if (retry++ == HDD_MOD_EXIT_SSR_MAX_RETRIES) {
			hdd_err("SSR never completed, error");
			/*
			 * Trigger the bug_on in the internal builds, in the
			 * customer builds self-recovery will be enabled
			 * in those cases just return error.
			 */
			if (cds_is_self_recovery_enabled())
				return false;
			QDF_BUG(0);
		}
	}

	hdd_info("Recovery completed successfully!");
	return true;
}


static int __hdd_netdev_notifier_call(struct notifier_block *nb,
				    unsigned long state, void *data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
	struct netdev_notifier_info *dev_notif_info = data;
	struct net_device *dev = dev_notif_info->dev;
#else
	struct net_device *dev = data;
#endif
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter_dev(dev);

	if (!dev->ieee80211_ptr) {
		hdd_debug("ieee80211_ptr is null");
		return NOTIFY_DONE;
	}

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD Context is Null");
		return NOTIFY_DONE;
	}

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_debug("%s: Driver module is closed", __func__);
		return NOTIFY_DONE;
	}

	/* Make sure that this callback corresponds to our device. */
	adapter = hdd_get_adapter_by_iface_name(hdd_ctx, dev->name);
	if (!adapter) {
		hdd_debug("failed to look up adapter for '%s'", dev->name);
		return NOTIFY_DONE;
	}

	if (adapter != WLAN_HDD_GET_PRIV_PTR(dev)) {
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

	hdd_debug("%s New Net Device State = %lu", dev->name, state);

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
		vdev = hdd_objmgr_get_vdev(adapter);
		if (!vdev)
			break;
		if (ucfg_scan_get_vdev_status(vdev) !=
				SCAN_NOT_IN_PROGRESS) {
			wlan_abort_scan(hdd_ctx->pdev, INVAL_PDEV_ID,
					adapter->session_id, INVALID_SCAN_ID,
					true);
		}
		hdd_objmgr_put_vdev(vdev);
		cds_flush_work(&adapter->scan_block_work);
		/* Need to clean up blocked scan request */
		wlan_hdd_cfg80211_scan_block_cb(&adapter->scan_block_work);
		hdd_debug("Scan is not Pending from user");
		/*
		 * After NETDEV_GOING_DOWN, kernel calls hdd_stop.Irrespective
		 * of return status of hdd_stop call, kernel resets the IFF_UP
		 * flag after which driver does not send the cfg80211_scan_done.
		 * Ensure to cleanup the scan queue in NETDEV_GOING_DOWN
		 */
		wlan_cfg80211_cleanup_scan_queue(hdd_ctx->pdev, dev);
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
 * @ndev: ndev pointer
 *
 * Return: 0 on success, error number otherwise.
 */
static int hdd_netdev_notifier_call(struct notifier_block *nb,
					unsigned long state,
					void *ndev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_netdev_notifier_call(nb, state, ndev);
	cds_ssr_unprotect(__func__);

	return ret;
}

struct notifier_block hdd_netdev_notifier = {
	.notifier_call = hdd_netdev_notifier_call,
};

/* variable to hold the insmod parameters */
static int con_mode;

static int con_mode_ftm;
int con_mode_monitor;

/* Variable to hold connection mode including module parameter con_mode */
static int curr_con_mode;

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
		if (fw_ch_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
			return CH_WIDTH_160MHZ;
		else
			return CH_WIDTH_80MHZ;
	case NL80211_CHAN_WIDTH_5:
		return CH_WIDTH_5MHZ;
	case NL80211_CHAN_WIDTH_10:
		return CH_WIDTH_10MHZ;
	default:
		hdd_err("Invalid channel width %d, setting to default",
				ch_width);
		return CH_WIDTH_INVALID;
	}
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

	if (hdd_ctx->start_modules_in_progress) {
		hdd_debug("Start modules in progress (via %s)", func);
		return -EAGAIN;
	}

	if (hdd_ctx->stop_modules_in_progress) {
		hdd_debug("Stop modules in progress (via %s)", func);
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

	if (qdf_atomic_read(&hdd_ctx->con_mode_flag)) {
		hdd_debug("Driver mode change in progress (via %s)", func);
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

	return __wlan_hdd_validate_session_id(adapter->session_id, func);
}

int __wlan_hdd_validate_session_id(uint8_t session_id, const char *func)
{
	if (session_id == CSR_SESSION_ID_INVALID) {
		hdd_debug_rl("adapter is not up (via %s)", func);
		return -EINVAL;
	}

	if (session_id >= CSR_ROAM_SESSION_MAX) {
		hdd_err("bad session Id:%u (via %s)", session_id, func);
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
	mutex_lock(&hdd_ctx->iface_change_lock);
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		mutex_unlock(&hdd_ctx->iface_change_lock);
		hdd_info("Modules not enabled, Present status: %d",
			 hdd_ctx->driver_status);
		return false;
	}
	mutex_unlock(&hdd_ctx->iface_change_lock);
	return true;
}

/**
 * hdd_set_ibss_power_save_params() - update IBSS Power Save params to WMA.
 * @struct hdd_adapter Hdd adapter.
 *
 * This function sets the IBSS power save config parameters to WMA
 * which will send it to firmware if FW supports IBSS power save
 * before vdev start.
 *
 * Return: QDF_STATUS QDF_STATUS_SUCCESS on Success and QDF_STATUS_E_FAILURE
 *         on failure.
 */
QDF_STATUS hdd_set_ibss_power_save_params(struct hdd_adapter *adapter)
{
	int ret;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (hdd_ctx == NULL) {
		hdd_err("HDD context is null");
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_SET_ATIM_WINDOW_SIZE,
				  hdd_ctx->config->ibssATIMWinSize,
				  VDEV_CMD);
	if (0 != ret) {
		hdd_err("atim window set failed %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_SET_POWER_SAVE_ALLOWED,
				  hdd_ctx->config->isIbssPowerSaveAllowed,
				  VDEV_CMD);
	if (0 != ret) {
		hdd_err("power save allow failed %d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_SET_POWER_COLLAPSE_ALLOWED,
				  hdd_ctx->config->
				  isIbssPowerCollapseAllowed, VDEV_CMD);
	if (0 != ret) {
		hdd_err("power collapse allow failed %d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_SET_AWAKE_ON_TX_RX,
				  hdd_ctx->config->isIbssAwakeOnTxRx,
				  VDEV_CMD);
	if (0 != ret) {
		hdd_err("set awake on tx/rx failed %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_SET_INACTIVITY_TIME,
				  hdd_ctx->config->ibssInactivityCount,
				  VDEV_CMD);
	if (0 != ret) {
		hdd_err("set inactivity time failed %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_SET_TXSP_END_INACTIVITY_TIME,
				  hdd_ctx->config->ibssTxSpEndInactivityTime,
				  VDEV_CMD);
	if (0 != ret) {
		hdd_err("set txsp end failed %d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_PS_SET_WARMUP_TIME_SECS,
				  hdd_ctx->config->ibssPsWarmupTime,
				  VDEV_CMD);
	if (0 != ret) {
		hdd_err("set ps warmup failed %d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMA_VDEV_IBSS_PS_SET_1RX_CHAIN_IN_ATIM_WINDOW,
				  hdd_ctx->config->ibssPs1RxChainInAtimEnable,
				  VDEV_CMD);
	if (0 != ret) {
		hdd_err("set 1rx chain atim failed %d",
			ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
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

	qdf_runtime_lock_deinit(&ctx->dfs);
	qdf_runtime_lock_deinit(&ctx->connect);

	wlan_scan_runtime_pm_deinit(hdd_ctx->pdev);
}

#else /* FEATURE_RUNTIME_PM */
static void hdd_runtime_suspend_context_init(struct hdd_context *hdd_ctx) {}
static void hdd_runtime_suspend_context_deinit(struct hdd_context *hdd_ctx) {}
#endif /* FEATURE_RUNTIME_PM */

#define INTF_MACADDR_MASK       0x7

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
			  MAC_ADDRESS_STR,
			  MAC_ADDR_ARRAY(hdd_ctx->
					provisioned_mac_addr[0].bytes));
	} else {
		qdf_mem_copy(hdd_ctx->derived_mac_addr[0].bytes,
			     hw_macaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		hdd_ctx->num_derived_addr++;
		hdd_debug("hdd_ctx->derived_mac_addr[0]: "
			  MAC_ADDRESS_STR,
			  MAC_ADDR_ARRAY(hdd_ctx->derived_mac_addr[0].bytes));
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
			  MAC_ADDRESS_STR, i,
			  MAC_ADDR_ARRAY(hdd_ctx->derived_mac_addr[i].bytes));
		hdd_ctx->num_derived_addr++;
	}
}

static int hdd_update_tdls_config(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	struct tdls_start_params tdls_cfg;
	struct tdls_user_config *config = &tdls_cfg.config;
	struct hdd_config *cfg = hdd_ctx->config;
	QDF_STATUS status;

	config->tdls_tx_states_period = cfg->fTDLSTxStatsPeriod;
	config->tdls_tx_pkt_threshold = cfg->fTDLSTxPacketThreshold;
	config->tdls_rx_pkt_threshold = cfg->fTDLSRxFrameThreshold;
	config->tdls_max_discovery_attempt = cfg->fTDLSMaxDiscoveryAttempt;
	config->tdls_idle_timeout = cfg->tdls_idle_timeout;
	config->tdls_idle_pkt_threshold = cfg->fTDLSIdlePacketThreshold;
	config->tdls_rssi_trigger_threshold = cfg->fTDLSRSSITriggerThreshold;
	config->tdls_rssi_teardown_threshold = cfg->fTDLSRSSITeardownThreshold;
	config->tdls_rssi_delta = cfg->fTDLSRSSIDelta;
	config->tdls_uapsd_mask = cfg->fTDLSUapsdMask;
	config->tdls_uapsd_inactivity_time = cfg->fTDLSPuapsdInactivityTimer;
	config->tdls_uapsd_pti_window = cfg->fTDLSPuapsdPTIWindow;
	config->tdls_uapsd_ptr_timeout = cfg->fTDLSPuapsdPTRTimeout;
	config->tdls_pre_off_chan_num = cfg->fTDLSPrefOffChanNum;
	config->tdls_pre_off_chan_bw = cfg->fTDLSPrefOffChanBandwidth;
	config->tdls_peer_kickout_threshold = cfg->tdls_peer_kickout_threshold;
	config->tdls_discovery_wake_timeout = cfg->tdls_discovery_wake_timeout;
	config->delayed_trig_framint = cfg->DelayedTriggerFrmInt;
	config->tdls_feature_flags = ((cfg->fEnableTDLSOffChannel ?
				     1 << TDLS_FEATURE_OFF_CHANNEL : 0) |
		(cfg->fEnableTDLSWmmMode ? 1 << TDLS_FEATURE_WMM : 0) |
		(cfg->fEnableTDLSBufferSta ? 1 << TDLS_FEATURE_BUFFER_STA : 0) |
		(cfg->fEnableTDLSSleepSta ? 1 << TDLS_FEATURE_SLEEP_STA : 0) |
		(cfg->enable_tdls_scan ? 1 << TDLS_FEATURE_SCAN : 0) |
		(cfg->fEnableTDLSSupport ? 1 << TDLS_FEATURE_ENABLE : 0) |
		(cfg->fEnableTDLSImplicitTrigger ?
		 1 << TDLS_FEAUTRE_IMPLICIT_TRIGGER : 0) |
		(cfg->fTDLSExternalControl & TDLS_STRICT_EXTERNAL_CONTROL ?
		 1 << TDLS_FEATURE_EXTERNAL_CONTROL : 0) |
		(cfg->fTDLSExternalControl & TDLS_LIBERAL_EXTERNAL_CONTROL ?
		 1 << TDLS_FEATURE_LIBERAL_EXTERNAL_CONTROL : 0 ));
	config->tdls_vdev_nss_2g = GET_VDEV_NSS_CHAIN(cfg->rx_nss_2g,
						      QDF_TDLS_MODE);
	config->tdls_vdev_nss_5g = GET_VDEV_NSS_CHAIN(cfg->rx_nss_5g,
						      QDF_TDLS_MODE);

	tdls_cfg.tdls_send_mgmt_req = eWNI_SME_TDLS_SEND_MGMT_REQ;
	tdls_cfg.tdls_add_sta_req = eWNI_SME_TDLS_ADD_STA_REQ;
	tdls_cfg.tdls_del_sta_req = eWNI_SME_TDLS_DEL_STA_REQ;
	tdls_cfg.tdls_update_peer_state = WMA_UPDATE_TDLS_PEER_STATE;
	tdls_cfg.tdls_del_all_peers = eWNI_SME_DEL_ALL_TDLS_PEERS;
	tdls_cfg.tdls_update_dp_vdev_flags = CDP_UPDATE_TDLS_FLAGS;
	tdls_cfg.tdls_event_cb = wlan_cfg80211_tdls_event_callback;
	tdls_cfg.tdls_evt_cb_data = psoc;
	tdls_cfg.tdls_peer_context = hdd_ctx;
	tdls_cfg.tdls_reg_peer = hdd_tdls_register_peer;
	tdls_cfg.tdls_dereg_peer = hdd_tdls_deregister_peer;
	tdls_cfg.tdls_wmm_cb = hdd_wmm_is_acm_allowed;
	tdls_cfg.tdls_wmm_cb_data = psoc;
	tdls_cfg.tdls_rx_cb = wlan_cfg80211_tdls_rx_callback;
	tdls_cfg.tdls_rx_cb_data = psoc;
	tdls_cfg.tdls_dp_vdev_update = hdd_update_dp_vdev_flags;
	tdls_cfg.tdls_osif_init_cb = wlan_cfg80211_tdls_osif_priv_init;
	tdls_cfg.tdls_osif_deinit_cb = wlan_cfg80211_tdls_osif_priv_deinit;

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

static void hdd_update_tgt_services(struct hdd_context *hdd_ctx,
				    struct wma_tgt_services *cfg)
{
	struct hdd_config *config = hdd_ctx->config;

	/* Set up UAPSD */
	config->apUapsdEnabled &= cfg->uapsd;

	/* 11AX mode support */
	if ((config->dot11Mode == eHDD_DOT11_MODE_11ax ||
	     config->dot11Mode == eHDD_DOT11_MODE_11ax_ONLY) && !cfg->en_11ax)
		config->dot11Mode = eHDD_DOT11_MODE_11ac;

	/* 11AC mode support */
	if ((config->dot11Mode == eHDD_DOT11_MODE_11ac ||
	     config->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY) && !cfg->en_11ac)
		config->dot11Mode = eHDD_DOT11_MODE_AUTO;

	/* ARP offload: override user setting if invalid  */
	config->fhostArpOffload &= cfg->arp_offload;

#ifdef FEATURE_WLAN_SCAN_PNO
	/* PNO offload */
	hdd_debug("PNO Capability in f/w = %d", cfg->pno_offload);
	if (cfg->pno_offload)
		config->PnoOffload = true;
#endif
#ifdef FEATURE_WLAN_TDLS
	config->fEnableTDLSSupport &= cfg->en_tdls;
	config->fEnableTDLSOffChannel = config->fEnableTDLSOffChannel &&
						cfg->en_tdls_offchan;
	config->fEnableTDLSBufferSta = config->fEnableTDLSBufferSta &&
						cfg->en_tdls_uapsd_buf_sta;
	if (config->fTDLSUapsdMask && cfg->en_tdls_uapsd_sleep_sta)
		config->fEnableTDLSSleepSta = true;
	else
		config->fEnableTDLSSleepSta = false;
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	config->isRoamOffloadEnabled &= cfg->en_roam_offload;
#endif
	config->sap_get_peer_info &= cfg->get_peer_info_enabled;
	config->MAWCEnabled &= cfg->is_fw_mawc_capable;
	hdd_update_tdls_config(hdd_ctx);
	sme_update_tgt_services(hdd_ctx->mac_handle, cfg);
	hdd_ctx->roam_ch_from_fw_supported = cfg->is_roam_scan_ch_to_host;
	hdd_ctx->ll_stats_per_chan_rx_tx_time =
					cfg->ll_stats_per_chan_rx_tx_time;
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
	struct hdd_config *cfg_ini = hdd_ctx->config;
	uint8_t max_supp_nss = 1;
	mac_handle_t mac_handle;

	if (cfg_ini->enable2x2 && !cds_is_sub_20_mhz_enabled())
		max_supp_nss = 2;
	hdd_debug("max nss %d", max_supp_nss);

	mac_handle = hdd_ctx->mac_handle;
	sme_update_vdev_type_nss(mac_handle, max_supp_nss,
				 cfg_ini->rx_nss_2g, BAND_2G);

	sme_update_vdev_type_nss(mac_handle, max_supp_nss,
				 cfg_ini->rx_nss_5g, BAND_5G);
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
	struct ieee80211_supported_band *band_5g =
		hdd_ctx->wiphy->bands[NL80211_BAND_5GHZ];
	uint32_t val;
	uint32_t value1;

	if (!band_5g) {
		hdd_debug("5GHz band disabled, skipping capability population");
		return;
	}

	val = hdd_ctx->config->txBFCsnValue;
	band_5g->vht_cap.cap |= (val << IEEE80211_VHT_CAP_BEAMFORMEE_STS_SHIFT);

	val = NUM_OF_SOUNDING_DIMENSIONS;
	band_5g->vht_cap.cap |=
		(val << IEEE80211_VHT_CAP_SOUNDING_DIMENSIONS_SHIFT);

	hdd_debug("Updated wiphy vhtcap:0x%x, CSNAntSupp:%d, NumSoundDim:%d",
		  band_5g->vht_cap.cap, hdd_ctx->config->txBFCsnValue, val);

	sme_cfg_get_int(hdd_ctx->mac_handle, WNI_CFG_VHT_RX_MCS_MAP, &value1);
	band_5g->vht_cap.vht_mcs.rx_mcs_map = value1;

	sme_cfg_get_int(hdd_ctx->mac_handle, WNI_CFG_VHT_TX_MCS_MAP, &value1);
	band_5g->vht_cap.vht_mcs.tx_mcs_map = value1;

}

/**
 * hdd_update_hw_dbs_capable() - sets the dbs capability of the device
 * @hdd_ctx: HDD context
 *
 * Sets the DBS capability as per INI and firmware capability
 *
 * Return: None
 */
static void hdd_update_hw_dbs_capable(struct hdd_context *hdd_ctx)
{
	struct hdd_config *cfg_ini = hdd_ctx->config;
	uint8_t hw_dbs_capable = 0;

	if (policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc) &&
		((cfg_ini->dual_mac_feature_disable ==
			ENABLE_DBS_CXN_AND_SCAN) ||
		(cfg_ini->dual_mac_feature_disable ==
			ENABLE_DBS_CXN_AND_ENABLE_SCAN_WITH_ASYNC_SCAN_OFF) ||
		(cfg_ini->dual_mac_feature_disable ==
			ENABLE_DBS_CXN_AND_DISABLE_SIMULTANEOUS_SCAN)))
		hw_dbs_capable = 1;

	sme_update_hw_dbs_capable(hdd_ctx->mac_handle, hw_dbs_capable);
}

static void hdd_update_tgt_ht_cap(struct hdd_context *hdd_ctx,
				  struct wma_tgt_ht_cap *cfg)
{
	QDF_STATUS status;
	uint32_t value, val32;
	uint16_t val16;
	struct hdd_config *pconfig = hdd_ctx->config;
	tSirMacHTCapabilityInfo *phtCapInfo;
	uint8_t mcs_set[SIZE_OF_SUPPORTED_MCS_SET];
	uint8_t enable_tx_stbc;
	mac_handle_t mac_handle;

	/* check and update RX STBC */
	if (pconfig->enableRxSTBC && !cfg->ht_rx_stbc)
		pconfig->enableRxSTBC = cfg->ht_rx_stbc;

	mac_handle = hdd_ctx->mac_handle;

	/* get the MPDU density */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_MPDU_DENSITY, &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get MPDU DENSITY");
		value = 0;
	}

	/*
	 * MPDU density:
	 * override user's setting if value is larger
	 * than the one supported by target
	 */
	if (value > cfg->mpdu_density) {
		status = sme_cfg_set_int(mac_handle, WNI_CFG_MPDU_DENSITY,
					 cfg->mpdu_density);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set MPDU DENSITY to CCM");
	}

	/* get the HT capability info */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_HT_CAP_INFO, &val32);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("could not get HT capability info");
		return;
	}
	val16 = (uint16_t) val32;
	phtCapInfo = (tSirMacHTCapabilityInfo *) &val16;

	/* Set the LDPC capability */
	phtCapInfo->advCodingCap = cfg->ht_rx_ldpc;

	if (pconfig->ShortGI20MhzEnable && !cfg->ht_sgi_20)
		pconfig->ShortGI20MhzEnable = cfg->ht_sgi_20;

	if (pconfig->ShortGI40MhzEnable && !cfg->ht_sgi_40)
		pconfig->ShortGI40MhzEnable = cfg->ht_sgi_40;

	hdd_ctx->num_rf_chains = cfg->num_rf_chains;
	hdd_ctx->ht_tx_stbc_supported = cfg->ht_tx_stbc;

	enable_tx_stbc = pconfig->enableTxSTBC;

	if (pconfig->enable2x2 && (cfg->num_rf_chains == 2)) {
		pconfig->enable2x2 = 1;
	} else {
		pconfig->enable2x2 = 0;
		enable_tx_stbc = 0;
	}

	if (!(cfg->ht_tx_stbc && pconfig->enable2x2))
		enable_tx_stbc = 0;
	phtCapInfo->txSTBC = enable_tx_stbc;

	val32 = val16;
	status = sme_cfg_set_int(mac_handle, WNI_CFG_HT_CAP_INFO, val32);
	if (status != QDF_STATUS_SUCCESS)
		hdd_err("could not set HT capability to CCM");
#define WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES 0xff
	value = SIZE_OF_SUPPORTED_MCS_SET;
	if (sme_cfg_get_str(mac_handle, WNI_CFG_SUPPORTED_MCS_SET, mcs_set,
			    &value) == QDF_STATUS_SUCCESS) {
		hdd_debug("Read MCS rate set");
		if (cfg->num_rf_chains > SIZE_OF_SUPPORTED_MCS_SET)
			cfg->num_rf_chains = SIZE_OF_SUPPORTED_MCS_SET;
		if (pconfig->enable2x2) {
			for (value = 0; value < cfg->num_rf_chains; value++)
				mcs_set[value] =
					WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES;

			status =
				sme_cfg_set_str(mac_handle,
						WNI_CFG_SUPPORTED_MCS_SET,
						mcs_set,
						SIZE_OF_SUPPORTED_MCS_SET);
			if (status == QDF_STATUS_E_FAILURE)
				hdd_err("could not set MCS SET to CCM");
		}
	}
#undef WLAN_HDD_RX_MCS_ALL_NSTREAM_RATES
}

static void hdd_update_tgt_vht_cap(struct hdd_context *hdd_ctx,
				   struct wma_tgt_vht_cap *cfg)
{
	QDF_STATUS status;
	uint32_t value = 0;
	struct hdd_config *pconfig = hdd_ctx->config;
	struct wiphy *wiphy = hdd_ctx->wiphy;
	struct ieee80211_supported_band *band_5g =
		wiphy->bands[HDD_NL80211_BAND_5GHZ];
	uint32_t temp = 0;
	uint32_t ch_width = eHT_CHANNEL_WIDTH_80MHZ;
	uint32_t hw_rx_ldpc_enabled;
	struct wma_caps_per_phy caps_per_phy;
	mac_handle_t mac_handle;
	uint32_t tx_highest_data_rate;
	uint32_t rx_highest_data_rate;

	if (!band_5g) {
		hdd_debug("5GHz band disabled, skipping capability population");
		return;
	}

	mac_handle = hdd_ctx->mac_handle;
	if (pconfig->enable2x2) {
		if (cfg->vht_short_gi_80 & WMI_VHT_CAP_SGI_80MHZ) {
			/* Update 2x2 Highest Short GI data rate */
			tx_highest_data_rate =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_2_2_SGI80;
			rx_highest_data_rate =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_2_2_SGI80;
		} else {
			/* Update 2x2 Rx Highest Long GI data Rate */
			tx_highest_data_rate =
					VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_2_2;
			rx_highest_data_rate =
					VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_2_2;
		}
	} else if (cfg->vht_short_gi_80 & WMI_VHT_CAP_SGI_80MHZ) {
		/* Update 1x1 Highest Short GI data rate */
		tx_highest_data_rate =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1_SGI80;
		rx_highest_data_rate =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1_SGI80;
	} else {
		 /* Update 1x1 Highest Long GI data rate */
		tx_highest_data_rate = VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
		rx_highest_data_rate = VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
	}

	status = sme_cfg_set_int(mac_handle,
				 WNI_CFG_VHT_RX_HIGHEST_SUPPORTED_DATA_RATE,
				 rx_highest_data_rate);

	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to set rx_supp_data_rate");

	status = sme_cfg_set_int(mac_handle,
				 WNI_CFG_VHT_TX_HIGHEST_SUPPORTED_DATA_RATE,
				 tx_highest_data_rate);

	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to set tx_supp_data_rate");


	/* Get the current MPDU length */
	status =
		sme_cfg_get_int(mac_handle, WNI_CFG_VHT_MAX_MPDU_LENGTH,
				&value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get MPDU LENGTH");
		value = 0;
	}

	/*
	 * VHT max MPDU length:
	 * override if user configured value is too high
	 * that the target cannot support
	 */
	if (value > cfg->vht_max_mpdu) {
		status = sme_cfg_set_int(mac_handle,
					 WNI_CFG_VHT_MAX_MPDU_LENGTH,
					 cfg->vht_max_mpdu);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set VHT MAX MPDU LENGTH");
	}

	sme_cfg_get_int(mac_handle, WNI_CFG_VHT_BASIC_MCS_SET, &temp);
	temp = (temp & VHT_MCS_1x1) | pconfig->vhtRxMCS;

	if (pconfig->enable2x2)
		temp = (temp & VHT_MCS_2x2) | (pconfig->vhtRxMCS2x2 << 2);

	if (sme_cfg_set_int(mac_handle, WNI_CFG_VHT_BASIC_MCS_SET, temp) ==
				QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass VHT_BASIC_MCS_SET to CCM");
	}

	sme_cfg_get_int(mac_handle, WNI_CFG_VHT_RX_MCS_MAP, &temp);
	temp = (temp & VHT_MCS_1x1) | pconfig->vhtRxMCS;
	if (pconfig->enable2x2)
		temp = (temp & VHT_MCS_2x2) | (pconfig->vhtRxMCS2x2 << 2);

	if (sme_cfg_set_int(mac_handle, WNI_CFG_VHT_RX_MCS_MAP, temp) ==
			QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass WNI_CFG_VHT_RX_MCS_MAP to CCM");
	}

	sme_cfg_get_int(mac_handle, WNI_CFG_VHT_TX_MCS_MAP, &temp);
	temp = (temp & VHT_MCS_1x1) | pconfig->vhtTxMCS;
	if (pconfig->enable2x2)
		temp = (temp & VHT_MCS_2x2) | (pconfig->vhtTxMCS2x2 << 2);

	hdd_debug("vhtRxMCS2x2 - %x temp - %u enable2x2 %d",
			pconfig->vhtRxMCS2x2, temp, pconfig->enable2x2);

	if (sme_cfg_set_int(mac_handle, WNI_CFG_VHT_TX_MCS_MAP, temp) ==
			QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass WNI_CFG_VHT_TX_MCS_MAP to CCM");
	}
	/* Get the current RX LDPC setting */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_LDPC_CODING_CAP,
				 &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT LDPC CODING CAP");
		value = 0;
	}

	/* Set HW RX LDPC capability */
	hw_rx_ldpc_enabled = !!cfg->vht_rx_ldpc;
	if (hw_rx_ldpc_enabled != value) {
		status = sme_cfg_set_int(mac_handle,
					 WNI_CFG_VHT_LDPC_CODING_CAP,
					 hw_rx_ldpc_enabled);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set VHT LDPC CODING CAP to CCM");
	}

	/* Get current GI 80 value */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_SHORT_GI_80MHZ,
				 &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get SHORT GI 80MHZ");
		value = 0;
	}

	/* set the Guard interval 80MHz */
	if (value && !cfg->vht_short_gi_80) {
		status = sme_cfg_set_int(mac_handle,
					 WNI_CFG_VHT_SHORT_GI_80MHZ,
					 cfg->vht_short_gi_80);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set SHORT GI 80MHZ to CCM");
	}

	/* Get VHT TX STBC cap */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_TXSTBC, &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT TX STBC");
		value = 0;
	}

	/* VHT TX STBC cap */
	if (value && !cfg->vht_tx_stbc) {
		status = sme_cfg_set_int(mac_handle, WNI_CFG_VHT_TXSTBC,
					 cfg->vht_tx_stbc);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set the VHT TX STBC to CCM");
	}

	/* Get VHT RX STBC cap */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_RXSTBC, &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT RX STBC");
		value = 0;
	}

	/* VHT RX STBC cap */
	if (value && !cfg->vht_rx_stbc) {
		status = sme_cfg_set_int(mac_handle, WNI_CFG_VHT_RXSTBC,
					 cfg->vht_rx_stbc);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set the VHT RX STBC to CCM");
	}

	/* Get VHT SU Beamformer cap */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_SU_BEAMFORMER_CAP,
				 &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT SU BEAMFORMER CAP");
		value = 0;
	}

	/* set VHT SU Beamformer cap */
	if (value && !cfg->vht_su_bformer) {
		status = sme_cfg_set_int(mac_handle,
					 WNI_CFG_VHT_SU_BEAMFORMER_CAP,
					 cfg->vht_su_bformer);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set VHT SU BEAMFORMER CAP");
	}

	/* check and update SU BEAMFORMEE capabality */
	if (pconfig->enableTxBF && !cfg->vht_su_bformee)
		pconfig->enableTxBF = cfg->vht_su_bformee;

	status = sme_cfg_set_int(mac_handle,
				 WNI_CFG_VHT_SU_BEAMFORMEE_CAP,
				 pconfig->enableTxBF);

	if (status == QDF_STATUS_E_FAILURE)
		hdd_err("could not set VHT SU BEAMFORMEE CAP");

	/* Get VHT MU Beamformer cap */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_MU_BEAMFORMER_CAP,
				 &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT MU BEAMFORMER CAP");
		value = 0;
	}

	/* set VHT MU Beamformer cap */
	if (value && !cfg->vht_mu_bformer) {
		status = sme_cfg_set_int(mac_handle,
					 WNI_CFG_VHT_MU_BEAMFORMER_CAP,
					 cfg->vht_mu_bformer);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set the VHT MU BEAMFORMER CAP to CCM");
	}

	/* Get VHT MU Beamformee cap */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_MU_BEAMFORMEE_CAP,
				 &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT MU BEAMFORMEE CAP");
		value = 0;
	}

	/* set VHT MU Beamformee cap */
	if (value && !cfg->vht_mu_bformee) {
		status = sme_cfg_set_int(mac_handle,
					 WNI_CFG_VHT_MU_BEAMFORMEE_CAP,
					 cfg->vht_mu_bformee);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set VHT MU BEAMFORMER CAP");
	}

	/* Get VHT MAX AMPDU Len exp */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_AMPDU_LEN_EXPONENT,
				 &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT AMPDU LEN");
		value = 0;
	}

	/*
	 * VHT max AMPDU len exp:
	 * override if user configured value is too high
	 * that the target cannot support.
	 * Even though Rome publish ampdu_len=7, it can
	 * only support 4 because of some h/w bug.
	 */

	if (value > cfg->vht_max_ampdu_len_exp) {
		status = sme_cfg_set_int(mac_handle,
					 WNI_CFG_VHT_AMPDU_LEN_EXPONENT,
					 cfg->vht_max_ampdu_len_exp);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set the VHT AMPDU LEN EXP");
	}

	/* Get VHT TXOP PS CAP */
	status = sme_cfg_get_int(mac_handle, WNI_CFG_VHT_TXOP_PS, &value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get VHT TXOP PS");
		value = 0;
	}

	/* set VHT TXOP PS cap */
	if (value && !cfg->vht_txop_ps) {
		status = sme_cfg_set_int(mac_handle, WNI_CFG_VHT_TXOP_PS,
					 cfg->vht_txop_ps);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set the VHT TXOP PS");
	}

	if (WMI_VHT_CAP_MAX_MPDU_LEN_11454 == cfg->vht_max_mpdu)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454;
	else if (WMI_VHT_CAP_MAX_MPDU_LEN_7935 == cfg->vht_max_mpdu)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_7991;
	else
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_3895;


	if (cfg->supp_chan_width & (1 << eHT_CHANNEL_WIDTH_80P80MHZ)) {
		status = sme_cfg_set_int(mac_handle,
				WNI_CFG_VHT_SUPPORTED_CHAN_WIDTH_SET,
				VHT_CAP_160_AND_80P80_SUPP);
		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set the VHT CAP 160");
		band_5g->vht_cap.cap |=
			IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;
		ch_width = eHT_CHANNEL_WIDTH_80P80MHZ;
	} else if (cfg->supp_chan_width & (1 << eHT_CHANNEL_WIDTH_160MHZ)) {
		status = sme_cfg_set_int(mac_handle,
				WNI_CFG_VHT_SUPPORTED_CHAN_WIDTH_SET,
				VHT_CAP_160_SUPP);
		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("could not set the VHT CAP 160");
		band_5g->vht_cap.cap |=
			IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ;
		ch_width = eHT_CHANNEL_WIDTH_160MHZ;
	}
	pconfig->vhtChannelWidth = QDF_MIN(pconfig->vhtChannelWidth,
			ch_width);
	/* Get the current GI 160 value */
	status = sme_cfg_get_int(mac_handle,
				WNI_CFG_VHT_SHORT_GI_160_AND_80_PLUS_80MHZ,
				&value);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("could not get GI 80 & 160");
		value = 0;
	}
	pconfig->ShortGI160MhzEnable = cfg->vht_short_gi_160;
	/* set the Guard interval 160MHz */
	if (value && !cfg->vht_short_gi_160) {
		status = sme_cfg_set_int(mac_handle,
			WNI_CFG_VHT_SHORT_GI_160_AND_80_PLUS_80MHZ,
			cfg->vht_short_gi_160);

		if (status == QDF_STATUS_E_FAILURE)
			hdd_err("failed to set SHORT GI 160MHZ");
	}

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

	pconfig->ShortGI80MhzEnable = cfg->vht_short_gi_80;
	if (cfg->vht_short_gi_80 & WMI_VHT_CAP_SGI_80MHZ)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_SHORT_GI_80;
	if (cfg->vht_short_gi_160 & WMI_VHT_CAP_SGI_160MHZ)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_SHORT_GI_160;

	if (cfg->vht_tx_stbc & WMI_VHT_CAP_TX_STBC)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_TXSTBC;

	if (cfg->vht_rx_stbc & WMI_VHT_CAP_RX_STBC_1SS)
		band_5g->vht_cap.cap |= IEEE80211_VHT_CAP_RXSTBC_1;
	if (cfg->vht_rx_stbc & WMI_VHT_CAP_RX_STBC_2SS)
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

#ifdef FEATURE_WLAN_APF
/**
 * hdd_update_apf_support() - Update APF supported flag in hdd context
 * @hdd_ctx: Pointer to hdd_ctx
 * @cfg: target configuration
 *
 * Update the APF support flag in HDD Context using INI and target config.
 *
 * Return: None
 */
static void hdd_update_apf_support(struct hdd_context *hdd_ctx,
				   struct wma_tgt_cfg *cfg)
{
	hdd_ctx->apf_supported = (cfg->apf_enabled &&
				  hdd_ctx->config->apf_packet_filter_enable);
}
#else
static void hdd_update_apf_support(struct hdd_context *hdd_ctx,
				   struct wma_tgt_cfg *cfg)
{
}
#endif

/**
 * hdd_update_ra_rate_limit() - Update RA rate limit from target
 *  configuration to cfg_ini in HDD
 * @hdd_ctx: Pointer to hdd_ctx
 * @cfg: target configuration
 *
 * Return: None
 */
#ifdef FEATURE_WLAN_RA_FILTERING
static void hdd_update_ra_rate_limit(struct hdd_context *hdd_ctx,
				     struct wma_tgt_cfg *cfg)
{
	hdd_ctx->config->IsRArateLimitEnabled = cfg->is_ra_rate_limit_enabled;
}
#else
static void hdd_update_ra_rate_limit(struct hdd_context *hdd_ctx,
				     struct wma_tgt_cfg *cfg)
{
}
#endif

static void hdd_sar_target_config(struct hdd_context *hdd_ctx,
				  struct wma_tgt_cfg *cfg)
{
	hdd_ctx->sar_version = cfg->sar_version;
}


static uint8_t hdd_get_nss_chain_shift(enum QDF_OPMODE device_mode)
{
	switch (device_mode) {
	case QDF_STA_MODE:
		return STA_NSS_CHAINS_SHIFT;
	case QDF_SAP_MODE:
		return SAP_NSS_CHAINS_SHIFT;
	case QDF_P2P_GO_MODE:
		return P2P_GO_NSS_CHAINS_SHIFT;
	case QDF_P2P_CLIENT_MODE:
		return P2P_CLI_CHAINS_SHIFT;
	case QDF_IBSS_MODE:
		return IBSS_NSS_CHAINS_SHIFT;
	case QDF_P2P_DEVICE_MODE:
		return P2P_DEV_NSS_CHAINS_SHIFT;
	case QDF_OCB_MODE:
		return OCB_NSS_CHAINS_SHIFT;
	case QDF_TDLS_MODE:
		return TDLS_NSS_CHAINS_SHIFT;

	default:
		hdd_err("Device mode %d invalid", device_mode);
		return STA_NSS_CHAINS_SHIFT;
	}
}

static bool
hdd_is_nss_update_allowed(struct wlan_mlme_chain_cfg chain_cfg,
			  uint8_t rx_nss, uint8_t tx_nss,
			  enum nss_chains_band_info band)
{
	switch (band) {
	case NSS_CHAINS_BAND_2GHZ:
		if (rx_nss > chain_cfg.max_rx_chains_2g)
			return false;
		if (tx_nss > chain_cfg.max_tx_chains_2g)
			return false;
		break;
	case NSS_CHAINS_BAND_5GHZ:
		if (rx_nss > chain_cfg.max_rx_chains_5g)
			return false;
		if (tx_nss > chain_cfg.max_tx_chains_5g)
			return false;
		break;
	default:
		hdd_err("Unknown Band nss change not allowed");
		return false;
	}
	return true;
}

static void hdd_modify_chains_in_hdd_cfg(struct hdd_context *hdd_ctx,
					  uint8_t rx_chains,
					  uint8_t tx_chains,
					  enum QDF_OPMODE vdev_op_mode,
					  enum nss_chains_band_info band)
{
	uint8_t nss_shift;
	uint32_t nss_mask = 0x7;
	uint32_t *rx_chains_ini = NULL;
	uint32_t *tx_chains_ini = NULL;

	if (band == NSS_CHAINS_BAND_2GHZ) {
		rx_chains_ini = &hdd_ctx->config->rx_nss_2g;
		tx_chains_ini = &hdd_ctx->config->tx_nss_2g;
	} else if (band == NSS_CHAINS_BAND_5GHZ) {
		rx_chains_ini = &hdd_ctx->config->rx_nss_5g;
		tx_chains_ini = &hdd_ctx->config->tx_nss_5g;
	} else {
		hdd_err("wrong band passed %d, cannot modify chains", band);
		return;
	}

	nss_shift = hdd_get_nss_chain_shift(vdev_op_mode);

	*rx_chains_ini &= ~(nss_mask << nss_shift);
	*rx_chains_ini |= (rx_chains << nss_shift);
	*tx_chains_ini &= ~(nss_mask << nss_shift);
	*tx_chains_ini |= (tx_chains << nss_shift);

	hdd_debug("rx chains %d tx chains %d changed for vdev mode %d for band %d",
		  rx_chains, tx_chains, vdev_op_mode, band);
}

void
hdd_modify_nss_in_hdd_cfg(struct hdd_context *hdd_ctx,
			  uint8_t rx_nss, uint8_t tx_nss,
			  enum QDF_OPMODE vdev_op_mode,
			  enum nss_chains_band_info band)
{
	uint8_t nss_shift;
	uint32_t nss_mask = 0x7;
	uint32_t *rx_nss_ini = NULL;
	uint32_t *tx_nss_ini = NULL;

	if (band == NSS_CHAINS_BAND_2GHZ) {
		rx_nss_ini = &hdd_ctx->config->rx_nss_2g;
		tx_nss_ini = &hdd_ctx->config->tx_nss_2g;
	} else if (band == NSS_CHAINS_BAND_5GHZ) {
		rx_nss_ini = &hdd_ctx->config->rx_nss_5g;
		tx_nss_ini = &hdd_ctx->config->tx_nss_5g;
	} else
		return;

	if (!hdd_is_nss_update_allowed(hdd_ctx->fw_chain_cfg, rx_nss, tx_nss,
				       band)) {
		hdd_debug("Nss modification failed, fw doesn't support this nss %d",
			  rx_nss);
		return;
	}

	nss_shift = hdd_get_nss_chain_shift(vdev_op_mode);

	*rx_nss_ini &= ~(nss_mask << nss_shift);
	*rx_nss_ini |= (rx_nss << nss_shift);
	*tx_nss_ini &= ~(nss_mask << nss_shift);
	*tx_nss_ini |= (tx_nss << nss_shift);

	hdd_debug("rx nss %d tx nss %d changed for vdev mode %d for band %d",
		  rx_nss, tx_nss, vdev_op_mode, band);
}

static void
hdd_modify_nss_chains_tgt_cfg(struct hdd_context *hdd_ctx,
			      enum QDF_OPMODE vdev_op_mode,
			      enum nss_chains_band_info band)
{
	uint32_t ini_rx_nss;
	uint32_t ini_tx_nss;
	uint8_t max_supported_rx_nss = MAX_VDEV_NSS;
	uint8_t max_supported_tx_nss = MAX_VDEV_NSS;
	uint32_t ini_rx_chains;
	uint32_t ini_tx_chains;
	uint8_t max_supported_rx_chains = MAX_VDEV_CHAINS;
	uint8_t max_supported_tx_chains = MAX_VDEV_CHAINS;
	uint8_t nss_shift = hdd_get_nss_chain_shift(vdev_op_mode);
	struct wlan_mlme_chain_cfg chain_cfg = hdd_ctx->fw_chain_cfg;

	if (band == NSS_CHAINS_BAND_2GHZ) {
		ini_rx_nss = hdd_ctx->config->rx_nss_2g;
		ini_tx_nss = hdd_ctx->config->tx_nss_2g;
		ini_rx_chains = hdd_ctx->config->num_rx_chains_2g;
		ini_tx_chains = hdd_ctx->config->num_tx_chains_2g;
	} else if (band == NSS_CHAINS_BAND_5GHZ) {
		ini_rx_nss = hdd_ctx->config->rx_nss_5g;
		ini_tx_nss = hdd_ctx->config->tx_nss_5g;
		ini_rx_chains = hdd_ctx->config->num_rx_chains_5g;
		ini_tx_chains = hdd_ctx->config->num_tx_chains_5g;
	} else {
		hdd_err("wrong band passed %d, cannot change nss in ini", band);
		return;
	}

	ini_rx_nss = GET_VDEV_NSS_CHAIN(ini_rx_nss, nss_shift);
	ini_tx_nss = GET_VDEV_NSS_CHAIN(ini_tx_nss, nss_shift);

	if (band == NSS_CHAINS_BAND_2GHZ) {
		max_supported_rx_nss = chain_cfg.max_rx_chains_2g;
		max_supported_tx_nss = chain_cfg.max_tx_chains_2g;
	} else if (band == NSS_CHAINS_BAND_5GHZ) {
		max_supported_rx_nss = chain_cfg.max_rx_chains_5g;
		max_supported_tx_nss = chain_cfg.max_tx_chains_5g;
	}

	max_supported_rx_nss = QDF_MIN(ini_rx_nss, max_supported_rx_nss);
	max_supported_tx_nss = QDF_MIN(ini_tx_nss, max_supported_tx_nss);

	ini_rx_chains = GET_VDEV_NSS_CHAIN(ini_rx_chains, nss_shift);
	ini_tx_chains = GET_VDEV_NSS_CHAIN(ini_tx_chains, nss_shift);

	if (band == NSS_CHAINS_BAND_2GHZ) {
		max_supported_rx_chains = chain_cfg.max_rx_chains_2g;
		max_supported_tx_chains = chain_cfg.max_tx_chains_2g;
	} else if (band == NSS_CHAINS_BAND_5GHZ) {
		max_supported_rx_chains = chain_cfg.max_rx_chains_5g;
		max_supported_tx_chains = chain_cfg.max_tx_chains_5g;
	}

	max_supported_rx_chains = QDF_MIN(ini_rx_chains,
					  max_supported_rx_chains);
	max_supported_tx_chains = QDF_MIN(ini_tx_chains,
					  max_supported_tx_chains);

	hdd_modify_nss_in_hdd_cfg(hdd_ctx, max_supported_rx_chains,
				     max_supported_tx_chains, vdev_op_mode,
				     band);
	hdd_modify_chains_in_hdd_cfg(hdd_ctx, max_supported_rx_nss,
				  max_supported_tx_nss, vdev_op_mode, band);
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

int hdd_update_tgt_cfg(hdd_handle_t hdd_handle, struct wma_tgt_cfg *cfg)
{
	int ret;
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	uint8_t temp_band_cap;
	struct cds_config_info *cds_cfg = cds_get_ini_config();
	uint8_t antenna_mode;
	QDF_STATUS status;
	mac_handle_t mac_handle;
	enum nss_chains_band_info band;

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

	hdd_objmgr_update_tgt_max_vdev_psoc(hdd_ctx, cfg->max_intf_count);

	ret = hdd_green_ap_update_config(hdd_ctx);

	ucfg_ipa_set_dp_handle(hdd_ctx->psoc,
			       cds_get_context(QDF_MODULE_ID_SOC));
	ucfg_ipa_set_txrx_handle(hdd_ctx->psoc,
				 cds_get_context(QDF_MODULE_ID_TXRX));
	ucfg_ipa_reg_sap_xmit_cb(hdd_ctx->pdev,
				 hdd_softap_ipa_start_xmit);
	ucfg_ipa_reg_send_to_nw_cb(hdd_ctx->pdev,
				   hdd_ipa_send_nbuf_to_network);

	if (cds_cfg) {
		if (hdd_ctx->config->enable_sub_20_channel_width !=
			WLAN_SUB_20_CH_WIDTH_NONE && !cfg->sub_20_support) {
			hdd_err("User requested sub 20 MHz channel width but unsupported by FW.");
			cds_cfg->sub_20_channel_width =
				WLAN_SUB_20_CH_WIDTH_NONE;
		} else {
			cds_cfg->sub_20_channel_width =
				hdd_ctx->config->enable_sub_20_channel_width;
		}
	}

	/* first store the INI band capability */
	temp_band_cap = hdd_ctx->config->nBandCapability;

	hdd_ctx->config->nBandCapability = cfg->band_cap;
	hdd_ctx->is_fils_roaming_supported =
			cfg->services.is_fils_roaming_supported;

	hdd_ctx->config->is_11k_offload_supported =
			cfg->services.is_11k_offload_supported;

	/*
	 * now overwrite the target band capability with INI
	 * setting if INI setting is a subset
	 */

	if ((hdd_ctx->config->nBandCapability == BAND_ALL) &&
	    (temp_band_cap != BAND_ALL))
		hdd_ctx->config->nBandCapability = temp_band_cap;
	else if ((hdd_ctx->config->nBandCapability != BAND_ALL) &&
		 (temp_band_cap != BAND_ALL) &&
		 (hdd_ctx->config->nBandCapability != temp_band_cap)) {
		hdd_warn("ini BandCapability not supported by the target");
	}

	hdd_ctx->curr_band = hdd_ctx->config->nBandCapability;
	hdd_ctx->psoc->soc_nif.user_config.band_capability = hdd_ctx->curr_band;

	status = ucfg_reg_set_band(hdd_ctx->pdev, hdd_ctx->curr_band);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to update regulatory band info");

	if (!cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		hdd_ctx->reg.reg_domain = cfg->reg_domain;
		hdd_ctx->reg.eeprom_rd_ext = cfg->eeprom_rd_ext;
	}

	/* This can be extended to other configurations like ht, vht cap... */

	if (!qdf_is_macaddr_zero(&cfg->hw_macaddr))
		qdf_mem_copy(&hdd_ctx->hw_macaddr, &cfg->hw_macaddr,
			     QDF_MAC_ADDR_SIZE);
	else
		hdd_info("hw_mac is zero");

	hdd_ctx->target_fw_version = cfg->target_fw_version;
	hdd_ctx->target_fw_vers_ext = cfg->target_fw_vers_ext;
	hdd_extract_fw_version_info(hdd_ctx);

	hdd_ctx->hw_bd_id = cfg->hw_bd_id;
	qdf_mem_copy(&hdd_ctx->hw_bd_info, &cfg->hw_bd_info,
		     sizeof(cfg->hw_bd_info));

	if (cfg->max_intf_count > WLAN_MAX_VDEVS) {
		hdd_err("fw max vdevs (%u) > host max vdevs (%u); using %u",
			cfg->max_intf_count, WLAN_MAX_VDEVS,
			WLAN_MAX_VDEVS);
		hdd_ctx->max_intf_count = WLAN_MAX_VDEVS;
	} else {
		hdd_ctx->max_intf_count = cfg->max_intf_count;
	}

	hdd_sar_target_config(hdd_ctx, cfg);
	hdd_lpass_target_config(hdd_ctx, cfg);

	hdd_ctx->ap_arpns_support = cfg->ap_arpns_support;

	hdd_update_tgt_services(hdd_ctx, &cfg->services);

	hdd_update_tgt_ht_cap(hdd_ctx, &cfg->ht_cap);

	hdd_update_tgt_vht_cap(hdd_ctx, &cfg->vht_cap);
	if (cfg->services.en_11ax) {
		hdd_debug("11AX: 11ax is enabled - update HDD config");
		hdd_update_tgt_he_cap(hdd_ctx, cfg);
	}
	hdd_update_tgt_twt_cap(hdd_ctx, cfg);

	hdd_ctx->fw_chain_cfg = cfg->chain_cfg;

	for (band = NSS_CHAINS_BAND_2GHZ; band < NSS_CHAINS_BAND_MAX; band++) {
		hdd_modify_nss_chains_tgt_cfg(hdd_ctx, QDF_STA_MODE, band);
		hdd_modify_nss_chains_tgt_cfg(hdd_ctx, QDF_SAP_MODE, band);
		hdd_modify_nss_chains_tgt_cfg(hdd_ctx, QDF_TDLS_MODE, band);
		hdd_modify_nss_chains_tgt_cfg(hdd_ctx, QDF_P2P_DEVICE_MODE,
					      band);
		hdd_modify_nss_chains_tgt_cfg(hdd_ctx, QDF_OCB_MODE, band);
		hdd_modify_nss_chains_tgt_cfg(hdd_ctx, QDF_TDLS_MODE, band);
	}

	hdd_update_vdev_nss(hdd_ctx);

	hdd_update_hw_dbs_capable(hdd_ctx);
	hdd_ctx->dynamic_nss_chains_support =
		cfg->dynamic_nss_chains_support &
		hdd_ctx->config->enable_dynamic_nss_chains_cfg;
	hdd_debug("Dynamic nss chains support FW %d driver %d",
		  cfg->dynamic_nss_chains_support,
		  hdd_ctx->config->enable_dynamic_nss_chains_cfg);
	hdd_ctx->nan_seperate_vdev_supported = cfg->nan_seperate_vdev_support;
	hdd_ctx->config->fine_time_meas_cap &= cfg->fine_time_measurement_cap;
	hdd_ctx->fine_time_meas_cap_target = cfg->fine_time_measurement_cap;
	hdd_debug("fine_time_meas_cap: 0x%x",
		  hdd_ctx->config->fine_time_meas_cap);

	antenna_mode = (hdd_ctx->config->enable2x2 == 0x01) ?
			HDD_ANTENNA_MODE_2X2 : HDD_ANTENNA_MODE_1X1;
	hdd_update_smps_antenna_mode(hdd_ctx, antenna_mode);
	hdd_debug("Init current antenna mode: %d",
		  hdd_ctx->current_antenna_mode);

	hdd_ctx->rcpi_enabled = cfg->rcpi_enabled;
	hdd_update_ra_rate_limit(hdd_ctx, cfg);

	if ((hdd_ctx->config->txBFCsnValue >
	     WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF) &&
	    !cfg->tx_bfee_8ss_enabled)
		hdd_ctx->config->txBFCsnValue =
			WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF;

	mac_handle = hdd_ctx->mac_handle;
	status = sme_cfg_set_int(mac_handle,
				 WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED,
				 hdd_ctx->config->txBFCsnValue);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("fw update WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED to CFG fails");

	hdd_update_apf_support(hdd_ctx, cfg);

	hdd_debug("Target APF %d Host APF %d 8ss fw support %d txBFCsnValue %d",
		  cfg->apf_enabled, hdd_ctx->config->apf_packet_filter_enable,
		  cfg->tx_bfee_8ss_enabled, hdd_ctx->config->txBFCsnValue);

	/*
	 * Update txBFCsnValue and NumSoundingDim values to vhtcap in wiphy
	 */
	hdd_update_wiphy_vhtcap(hdd_ctx);

	hdd_ctx->wmi_max_len = cfg->wmi_max_len;

	/*
	 * This needs to be done after HDD pdev is created and stored since
	 * it will access the HDD pdev object lock.
	 */
	hdd_runtime_suspend_context_init(hdd_ctx);

	/* Configure NAN datapath features */
	hdd_nan_datapath_target_config(hdd_ctx, cfg);
	hdd_ctx->dfs_cac_offload = cfg->dfs_cac_offload;
	hdd_ctx->lte_coex_ant_share = cfg->services.lte_coex_ant_share;
	status = sme_cfg_set_int(mac_handle, WNI_CFG_OBSS_DETECTION_OFFLOAD,
				 cfg->obss_detection_offloaded);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Couldn't pass WNI_CFG_OBSS_DETECTION_OFFLOAD to CFG");

	status = sme_cfg_set_int(mac_handle,
				 WNI_CFG_OBSS_COLOR_COLLISION_OFFLOAD,
				 cfg->obss_color_collision_offloaded);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to set WNI_CFG_OBSS_COLOR_COLLISION_OFFLOAD");

	return 0;

exit:
	hdd_objmgr_release_and_destroy_pdev(hdd_ctx);
	return ret;
}

bool hdd_dfs_indicate_radar(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;
	struct hdd_ap_ctx *ap_ctx;

	if (!hdd_ctx) {
		hdd_info("Couldn't get hdd_ctx");
		return true;
	}

	if (hdd_ctx->config->disableDFSChSwitch) {
		hdd_info("skip tx block hdd_ctx=%pK, disableDFSChSwitch=%d",
			 hdd_ctx, hdd_ctx->config->disableDFSChSwitch);
		return true;
	}

	hdd_for_each_adapter(hdd_ctx, adapter) {
		ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

		if ((QDF_SAP_MODE == adapter->device_mode ||
		    QDF_P2P_GO_MODE == adapter->device_mode) &&
		    (wlan_reg_is_passive_or_disable_ch(hdd_ctx->pdev,
		     ap_ctx->operating_channel))) {
			WLAN_HDD_GET_AP_CTX_PTR(adapter)->dfs_cac_block_tx =
				true;
			hdd_info("tx blocked for session: %d",
				adapter->session_id);
			if (adapter->txrx_vdev)
				cdp_fc_vdev_flush(
					cds_get_context(QDF_MODULE_ID_SOC),
					adapter->txrx_vdev);
		}
	}

	return true;
}

/**
 * hdd_is_valid_mac_address() - validate MAC address
 * @pMacAddr:	Pointer to the input MAC address
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
 * Return: 1 for valid and 0 for invalid
 */
bool hdd_is_valid_mac_address(const uint8_t *pMacAddr)
{
	int xdigit = 0;
	int separator = 0;

	while (*pMacAddr) {
		if (isxdigit(*pMacAddr)) {
			xdigit++;
		} else if (':' == *pMacAddr) {
			if (0 == xdigit || ((xdigit / 2) - 1) != separator)
				break;

			++separator;
		} else {
			/* Invalid MAC found */
			return 0;
		}
		++pMacAddr;
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
 * __hdd__mon_open() - HDD Open function
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

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	hdd_mon_mode_ether_setup(dev);

	if (con_mode == QDF_GLOBAL_MONITOR_MODE) {
		ret = hdd_trigger_psoc_idle_restart(hdd_ctx);
		if (ret) {
			hdd_err("Failed to start WLAN modules return");
			return ret;
		}
		hdd_err("hdd_wlan_start_modules() successful !");

		if (!test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
			ret = hdd_start_adapter(adapter);
			if (ret) {
				hdd_err("Failed to start adapter :%d",
						adapter->device_mode);
				return ret;
			}
			hdd_err("hdd_start_adapters() successful !");
		}
		set_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);
	}

	ret = hdd_set_mon_rx_cb(dev);

	if (!ret)
		ret = hdd_enable_monitor_mode(dev);

	return ret;
}

/**
 * hdd_mon_open() - Wrapper function for __hdd_mon_open to protect it from SSR
 * @dev:	Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int hdd_mon_open(struct net_device *dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_mon_open(dev);
	cds_ssr_unprotect(__func__);

	return ret;
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

	adapter->vdev = hdd_objmgr_get_vdev(sta_adapter);
	if (!adapter->vdev) {
		hdd_err("station interface is not up");
		return -EINVAL;
	}

	hdd_mon_mode_ether_setup(dev);

	status = ucfg_pkt_capture_register_callbacks(adapter->vdev,
						     hdd_mon_rx_packet_cbk,
						     adapter);
	ret = qdf_status_to_os_return(status);
	if (ret) {
		hdd_objmgr_put_vdev(adapter->vdev);
		return -EINVAL;
	}
	set_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);

	return ret;
}

/**
 * hdd_pktcapture_open() - Wrapper function for __hdd_pktcapture_open to
 * protect it from SSR
 * @dev: Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int hdd_pktcapture_open(struct net_device *net_dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_pktcapture_open(net_dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_del_monitor_interface() - Delete monitor interface
 * @hdd_ctx: hdd context
 *
 * Return: void
 */
static void hdd_del_monitor_interface(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;

	adapter = hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);
	if (adapter) {
		if (hdd_is_interface_up(adapter)) {
			hdd_stop_adapter(hdd_ctx, adapter);
			hdd_deinit_adapter(hdd_ctx, adapter, true);
		}

		hdd_close_adapter(hdd_ctx, adapter, true);
	}
}

/**
 * hdd_close_monitor_interface() - Close monitor interface
 * @hdd_ctx: hdd context
 *
 * Return: void
 */
static void hdd_close_monitor_interface(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;

	adapter = hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);
	if (adapter)
		wlan_hdd_del_monitor(hdd_ctx, adapter, true);
}
#else
static inline void
hdd_del_monitor_interface(struct hdd_context *hdd_ctx)
{
}

static inline void
hdd_close_monitor_interface(struct hdd_context *hdd_ctx)
{
}
#endif

static QDF_STATUS
wlan_hdd_update_dbs_scan_and_fw_mode_config(void)
{
	struct policy_mgr_dual_mac_config cfg = {0};
	QDF_STATUS status;
	uint32_t channel_select_logic_conc = 0;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return QDF_STATUS_E_FAILURE;
	}


	/*
	 * ROME platform doesn't support any DBS related commands in FW,
	 * so if driver sends wmi command with dual_mac_config with all set to
	 * 0 then FW wouldn't respond back and driver would timeout on waiting
	 * for response. Check if FW supports DBS to eliminate ROME vs
	 * NON-ROME platform.
	 */
	if (!policy_mgr_find_if_fw_supports_dbs(hdd_ctx->psoc))
		return QDF_STATUS_SUCCESS;

	cfg.scan_config = 0;
	cfg.fw_mode_config = 0;
	cfg.set_dual_mac_cb = policy_mgr_soc_set_dual_mac_cfg_cb;

	if (policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc))
		channel_select_logic_conc =
			hdd_ctx->config->channel_select_logic_conc;

	if (hdd_ctx->config->dual_mac_feature_disable !=
	    DISABLE_DBS_CXN_AND_SCAN) {
		status = policy_mgr_get_updated_scan_and_fw_mode_config(
				hdd_ctx->psoc, &cfg.scan_config,
				&cfg.fw_mode_config,
				hdd_ctx->config->dual_mac_feature_disable,
				channel_select_logic_conc);

		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("wma_get_updated_scan_and_fw_mode_config failed %d",
				status);
			return status;
		}
	}

	hdd_debug("send scan_cfg: 0x%x fw_mode_cfg: 0x%x to fw",
		cfg.scan_config, cfg.fw_mode_config);

	status = sme_soc_set_dual_mac_config(cfg);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("sme_soc_set_dual_mac_config failed %d", status);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

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
	hdd_debug("Start_adapter for mode : %d", adapter->device_mode);

	switch (device_mode) {
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_OCB_MODE:
	case QDF_STA_MODE:
	case QDF_MONITOR_MODE:
	case QDF_NAN_DISC_MODE:
		ret = hdd_start_station_adapter(adapter);
		if (ret)
			goto err_start_adapter;

		hdd_nud_ignore_tracking(adapter, false);
		break;
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
		ret = hdd_start_ap_adapter(adapter);
		if (ret)
			goto err_start_adapter;
		break;
	case QDF_IBSS_MODE:
		/*
		 * For IBSS interface is initialized as part of
		 * hdd_init_station_mode()
		 */
		goto exit_with_success;
	case QDF_FTM_MODE:
		/* vdevs are dynamically managed by firmware in FTM */
		goto exit_with_success;
	default:
		hdd_err("Invalid session type %d", device_mode);
		QDF_ASSERT(0);
		goto err_start_adapter;
	}

	if (hdd_set_fw_params(adapter))
		hdd_err("Failed to set the FW params for the adapter!");

	if (adapter->session_id != HDD_SESSION_ID_INVALID) {
		ret = wlan_hdd_cfg80211_register_frames(adapter);
		if (ret < 0) {
			hdd_err("Failed to register frames - ret %d", ret);
			goto err_start_adapter;
		}
	}

	wlan_hdd_update_dbs_scan_and_fw_mode_config();

exit_with_success:
	hdd_exit();

	return 0;

err_start_adapter:
	return -EINVAL;
}

/**
 * hdd_enable_power_management() - API to Enable Power Management
 *
 * API invokes Bus Interface Layer power management functionality
 *
 * Return: None
 */
static void hdd_enable_power_management(void)
{
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);

	if (!hif_ctx) {
		hdd_err("Bus Interface Context is Invalid");
		return;
	}

	hif_enable_power_management(hif_ctx, cds_is_packet_log_enabled());
}

/**
 * hdd_disable_power_management() - API to disable Power Management
 *
 * API disable Bus Interface Layer Power management functionality
 *
 * Return: None
 */
static void hdd_disable_power_management(void)
{
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);

	if (!hif_ctx) {
		hdd_err("Bus Interface Context is Invalid");
		return;
	}

	hif_disable_power_management(hif_ctx);
}

void hdd_update_hw_sw_info(struct hdd_context *hdd_ctx)
{
	void *hif_sc;
	size_t target_hw_name_len;
	const char *target_hw_name;
	uint8_t *buf;
	uint32_t buf_len;

	hif_sc = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_sc) {
		hdd_err("HIF context is NULL");
		return;
	}

	hif_get_hw_info(hif_sc, &hdd_ctx->target_hw_version,
			&hdd_ctx->target_hw_revision,
			&target_hw_name);

	if (hdd_ctx->target_hw_name)
		qdf_mem_free(hdd_ctx->target_hw_name);

	target_hw_name_len = strlen(target_hw_name) + 1;
	hdd_ctx->target_hw_name = qdf_mem_malloc(target_hw_name_len);
	if (hdd_ctx->target_hw_name)
		qdf_mem_copy(hdd_ctx->target_hw_name, target_hw_name,
			     target_hw_name_len);

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
	uint8_t num_entries = 0;
	uint8_t tx_sched_wrr_param[TX_SCHED_WRR_PARAMS_NUM];
	uint8_t *tx_sched_wrr_ac;
	int i;
	struct cds_context *cds_ctx;

	if (NULL == hdd_ctx)
		return;

	if (NULL == hdd_ctx->config) {
		/* Do nothing if hdd_ctx is invalid */
		hdd_err("%s: Warning: hdd_ctx->cfg_ini is NULL", __func__);
		return;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);

	if (!cds_ctx) {
		hdd_err("Invalid CDS Context");
		return;
	}

	for (i = 0; i < OL_TX_NUM_WMM_AC; i++) {
		switch (i) {
		case OL_TX_WMM_AC_BE:
			tx_sched_wrr_ac = hdd_ctx->config->tx_sched_wrr_be;
			break;
		case OL_TX_WMM_AC_BK:
			tx_sched_wrr_ac = hdd_ctx->config->tx_sched_wrr_bk;
			break;
		case OL_TX_WMM_AC_VI:
			tx_sched_wrr_ac = hdd_ctx->config->tx_sched_wrr_vi;
			break;
		case OL_TX_WMM_AC_VO:
			tx_sched_wrr_ac = hdd_ctx->config->tx_sched_wrr_vo;
			break;
		default:
			tx_sched_wrr_ac = NULL;
			break;
		}

		hdd_string_to_u8_array(tx_sched_wrr_ac,
				tx_sched_wrr_param,
				&num_entries,
				sizeof(tx_sched_wrr_param));

		if (num_entries == TX_SCHED_WRR_PARAMS_NUM) {
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

		num_entries = 0;
	}
}

uint32_t hdd_wlan_get_version(struct hdd_context *hdd_ctx,
			      const size_t version_len, uint8_t *version)
{
	uint32_t size;

	if (!hdd_ctx) {
		hdd_err("Invalid context, HDD context is null");
		return 0;
	}

	if (!version || version_len == 0) {
		hdd_err("Invalid buffer pointr or buffer len\n");
		return 0;
	}

	size = scnprintf(version, version_len,
			 "Host SW:%s, FW:%d.%d.%d.%d.%d.%d, HW:%s, Board ver: %x Ref design id: %x, Customer id: %x, Project id: %x, Board Data Rev: %x",
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
			 hdd_ctx->hw_bd_info.board_data_rev);

	return size;
}

#ifdef IPA_OFFLOAD
/**
 * hdd_update_ipa_component_config() - update ipa config
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static void hdd_update_ipa_component_config(struct hdd_context *hdd_ctx)
{
	struct hdd_config *cfg = hdd_ctx->config;
	struct wlan_ipa_config ipa_cfg;

	ipa_cfg.ipa_config = cfg->IpaConfig;
	ipa_cfg.desc_size = cfg->IpaDescSize;
	ipa_cfg.txbuf_count = cfg->IpaUcTxBufCount;
	ipa_cfg.bus_bw_high = cfg->busBandwidthHighThreshold;
	ipa_cfg.bus_bw_medium = cfg->busBandwidthMediumThreshold;
	ipa_cfg.bus_bw_low = cfg->busBandwidthLowThreshold;
	ipa_cfg.ipa_bw_high = cfg->IpaHighBandwidthMbps;
	ipa_cfg.ipa_bw_medium = cfg->IpaMediumBandwidthMbps;
	ipa_cfg.ipa_bw_low = cfg->IpaLowBandwidthMbps;
	ipa_cfg.ipa_force_voting = cfg->IpaForceVoting;

	ucfg_ipa_update_config(&ipa_cfg);
}
#else
static void hdd_update_ipa_component_config(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef FEATURE_WLAN_WAPI
/**
 * hdd_wapi_security_sta_exist() - return wapi security sta exist or not
 *
 * This API returns the wapi security station exist or not
 *
 * Return: true - wapi security station exist
 */
static bool hdd_wapi_security_sta_exist(void)
{
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if ((adapter->device_mode == QDF_STA_MODE) &&
		    adapter->wapi_info.wapi_mode &&
		    (adapter->wapi_info.wapi_auth_mode != WAPI_AUTH_MODE_OPEN))
			return true;
	}
	return false;
}
#else
static bool hdd_wapi_security_sta_exist(void)
{
	return false;
}
#endif

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
	hdd_cbacks.hdd_wapi_security_sta_exist =
		hdd_wapi_security_sta_exist;

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

#ifdef WLAN_FEATURE_NAN_CONVERGENCE
static void hdd_indicate_active_ndp_cnt(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, uint8_t cnt)
{
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (adapter && hdd_ctx && (hdd_ctx->config->sta_disable_roam &
	    LFR3_STA_ROAM_DISABLE_BY_NAN)) {
		hdd_debug("vdev_id:%d ndp active sessions %d", vdev_id, cnt);
		if (!cnt)
			wlan_hdd_enable_roaming(adapter);
		else
			wlan_hdd_disable_roaming(adapter);
	}
}

static void hdd_nan_register_callbacks(struct hdd_context *hdd_ctx)
{
	struct nan_callbacks cb_obj = {0};

	cb_obj.ndi_open = hdd_ndi_open;
	cb_obj.ndi_close = hdd_ndi_close;
	cb_obj.ndi_start = hdd_ndi_start;
	cb_obj.ndi_delete = hdd_ndi_delete;
	cb_obj.drv_ndi_create_rsp_handler = hdd_ndi_drv_ndi_create_rsp_handler;
	cb_obj.drv_ndi_delete_rsp_handler = hdd_ndi_drv_ndi_delete_rsp_handler;

	cb_obj.new_peer_ind = hdd_ndp_new_peer_handler;
	cb_obj.get_peer_idx = hdd_ndp_get_peer_idx;
	cb_obj.peer_departed_ind = hdd_ndp_peer_departed_handler;
	cb_obj.wlan_hdd_indicate_active_ndp_cnt = hdd_indicate_active_ndp_cnt;

	os_if_nan_register_hdd_callbacks(hdd_ctx->psoc, &cb_obj);
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

	wlan_objmgr_psoc_check_for_peer_leaks(hdd_ctx->psoc);
	wlan_objmgr_psoc_check_for_vdev_leaks(hdd_ctx->psoc);
	wlan_objmgr_psoc_check_for_pdev_leaks(hdd_ctx->psoc);

	/* many adapter resources are not freed by design during SSR */
	if (is_ssr)
		return;

	qdf_mc_timer_check_for_leaks();
	qdf_nbuf_map_check_for_leaks();
	qdf_mem_check_for_leaks();
}

#define hdd_debug_domain_set(domain) qdf_debug_domain_set(domain)
#else
static inline void hdd_check_for_leaks(struct hdd_context *hdd_ctx, bool is_ssr)
{ }

#define hdd_debug_domain_set(domain)
#endif /* CONFIG_LEAK_DETECTION */

/**
 * hdd_update_country_code - Update country code
 * @hdd_ctx: HDD context
 *
 * Update country code based on module parameter country_code
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_update_country_code(struct hdd_context *hdd_ctx)
{
	if (!country_code)
		return 0;

	return hdd_reg_set_country(hdd_ctx, country_code);
}

/**
 * wlan_hdd_init_tx_rx_histogram() - init tx/rx histogram stats
 * @hdd_ctx: hdd context
 *
 * Return: 0 for success or error code
 */
static int wlan_hdd_init_tx_rx_histogram(struct hdd_context *hdd_ctx)
{
	hdd_ctx->hdd_txrx_hist = qdf_mem_malloc(
		(sizeof(struct hdd_tx_rx_histogram) * NUM_TX_RX_HISTOGRAM));
	if (hdd_ctx->hdd_txrx_hist == NULL) {
		hdd_err("Failed malloc for hdd_txrx_hist");
		return -ENOMEM;
	}
	return 0;
}

/**
 * wlan_hdd_deinit_tx_rx_histogram() - deinit tx/rx histogram stats
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
static void wlan_hdd_deinit_tx_rx_histogram(struct hdd_context *hdd_ctx)
{
	if (!hdd_ctx || hdd_ctx->hdd_txrx_hist == NULL)
		return;

	qdf_mem_free(hdd_ctx->hdd_txrx_hist);
	hdd_ctx->hdd_txrx_hist = NULL;
}

/**
 * hdd_wlan_start_modules() - Single driver state machine for starting modules
 * @hdd_ctx: HDD context
 * @reinit: flag to indicate from SSR or normal path
 *
 * This function maintains the driver state machine it will be invoked from
 * startup, reinit and change interface. Depending on the driver state shall
 * perform the opening of the modules.
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_wlan_start_modules(struct hdd_context *hdd_ctx, bool reinit)
{
	int ret = 0;
	qdf_device_t qdf_dev;
	QDF_STATUS status;
	bool unint = false;
	void *hif_ctx;
	struct target_psoc_info *tgt_hdl;

	qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_dev) {
		hdd_err("QDF Device Context is Invalid return");
		return -EINVAL;
	}

	hdd_psoc_idle_timer_stop(hdd_ctx);

	mutex_lock(&hdd_ctx->iface_change_lock);
	if (hdd_ctx->driver_status == DRIVER_MODULES_ENABLED) {
		mutex_unlock(&hdd_ctx->iface_change_lock);
		hdd_debug("Driver modules already Enabled");
		hdd_exit();
		return 0;
	}

	hdd_ctx->start_modules_in_progress = true;

	switch (hdd_ctx->driver_status) {
	case DRIVER_MODULES_UNINITIALIZED:
		hdd_nofl_debug("Wlan transitioning (UNINITIALIZED -> CLOSED)");
		unint = true;
		/* Fall through dont add break here */
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
			hdd_err("hif context is null!!");
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

		hdd_update_ipa_component_config(hdd_ctx);

		ret = hdd_update_config(hdd_ctx);
		if (ret) {
			hdd_err("Failed to update configuration; errno: %d",
				ret);
			goto cds_free;
		}

		hdd_update_cds_ac_specs_params(hdd_ctx);

		status = wbuff_module_init();
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("WBUFF init unsuccessful; status: %d", status);

		status = cds_open(hdd_ctx->psoc);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to Open CDS; status: %d", status);
			ret = qdf_status_to_os_return(status);
			goto deinit_config;
		}

		hdd_ctx->mac_handle = cds_get_context(QDF_MODULE_ID_SME);

		if (hdd_ctx->config->rx_thread_affinity_mask)
			cds_set_rx_thread_cpu_mask(
				hdd_ctx->config->rx_thread_affinity_mask);

		if (hdd_ctx->config->rx_thread_ul_affinity_mask)
			cds_set_rx_thread_ul_cpu_mask(
				hdd_ctx->config->rx_thread_ul_affinity_mask);

		/* initialize components configurations after psoc open */
		ret = hdd_update_components_config(hdd_ctx);
		if (ret) {
			hdd_err("Failed to update component configs; errno: %d",
				ret);
			goto close;
		}

		status = cds_dp_open(hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("Failed to Open cds post open; status: %d",
				status);
			ret = qdf_status_to_os_return(status);
			goto close;
		}

		ret = hdd_register_cb(hdd_ctx);
		if (ret) {
			hdd_err("Failed to register HDD callbacks!");
			goto cds_txrx_free;
		}

		/*
		 * NAN compoenet requires certian operations like, open adapter,
		 * close adapter, etc. to be initiated by HDD, for those
		 * register HDD callbacks with UMAC's NAN componenet.
		 */
#ifdef WLAN_FEATURE_NAN_CONVERGENCE
		hdd_nan_register_callbacks(hdd_ctx);
#endif

		status = cds_pre_enable();
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("Failed to pre-enable CDS; status: %d", status);
			ret = qdf_status_to_os_return(status);
			goto deregister_cb;
		}

		hdd_register_policy_manager_callback(
			hdd_ctx->psoc);

		hdd_sysfs_create_driver_root_obj();
		hdd_sysfs_create_version_interface(hdd_ctx->psoc);
		hdd_sysfs_create_powerstats_interface();
		hdd_update_hw_sw_info(hdd_ctx);

		if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
			hdd_err("in ftm mode, no need to configure cds modules");
			ret = -EINVAL;
			break;
		}

		ret = hdd_configure_cds(hdd_ctx);
		if (ret) {
			hdd_err("Failed to Enable cds modules; errno: %d", ret);
			goto destroy_driver_sysfs;
		}

		hdd_enable_power_management();

		wlan_hdd_init_tx_rx_histogram(hdd_ctx);

		break;

	default:
		QDF_DEBUG_PANIC("Unknown driver state:%d",
				hdd_ctx->driver_status);
		ret = -EINVAL;
		goto release_lock;
	}

	hdd_ctx->driver_status = DRIVER_MODULES_ENABLED;
	hdd_nofl_debug("Wlan transitioned (now ENABLED)");

	hdd_ctx->start_modules_in_progress = false;

	mutex_unlock(&hdd_ctx->iface_change_lock);

	hdd_exit();

	return 0;

destroy_driver_sysfs:
	hdd_sysfs_destroy_powerstats_interface();
	hdd_sysfs_destroy_version_interface();
	hdd_sysfs_destroy_driver_root_obj();
	cds_post_disable();

deregister_cb:
	hdd_deregister_cb(hdd_ctx);

cds_txrx_free:
	tgt_hdl = wlan_psoc_get_tgt_if_handle(hdd_ctx->psoc);

	if (tgt_hdl && target_psoc_get_wmi_ready(tgt_hdl)) {
		hdd_runtime_suspend_context_deinit(hdd_ctx);
		dispatcher_pdev_close(hdd_ctx->pdev);
		hdd_objmgr_release_and_destroy_pdev(hdd_ctx);
	}

	cds_dp_close(hdd_ctx->psoc);

close:
	hdd_ctx->driver_status = DRIVER_MODULES_CLOSED;
	hdd_info("Wlan transition aborted (now CLOSED)");

	cds_close(hdd_ctx->psoc);

deinit_config:
	cds_deinit_ini_config();

cds_free:
	ol_cds_free();

hif_close:
	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	hdd_hif_close(hdd_ctx, hif_ctx);
power_down:
	if (!reinit && !unint)
		pld_power_off(qdf_dev->dev);
release_lock:
	hdd_ctx->start_modules_in_progress = false;
	mutex_unlock(&hdd_ctx->iface_change_lock);
	if (hdd_ctx->target_hw_name) {
		qdf_mem_free(hdd_ctx->target_hw_name);
		hdd_ctx->target_hw_name = NULL;
	}

	hdd_check_for_leaks(hdd_ctx, reinit);
	hdd_debug_domain_set(QDF_DEBUG_DOMAIN_INIT);

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

	return  ret;
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
	struct hdd_config *cfg = hdd_ctx->config;

	wifi_pos_set_oem_target_type(psoc, hdd_ctx->target_type);
	wifi_pos_set_oem_fw_version(psoc, hdd_ctx->target_fw_version);
	wifi_pos_set_drv_ver_major(psoc, QWLAN_VERSION_MAJOR);
	wifi_pos_set_drv_ver_minor(psoc, QWLAN_VERSION_MINOR);
	wifi_pos_set_drv_ver_patch(psoc, QWLAN_VERSION_PATCH);
	wifi_pos_set_drv_ver_build(psoc, QWLAN_VERSION_BUILD);
	wifi_pos_set_dwell_time_min(psoc, cfg->nNeighborScanMinChanTime);
	wifi_pos_set_dwell_time_max(psoc, cfg->nNeighborScanMaxChanTime);
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
		   adapter->session_id, adapter->device_mode);

	mutex_lock(&hdd_init_deinit_lock);
	/* Nothing to be done if device is unloading */
	if (cds_is_driver_unloading()) {
		mutex_unlock(&hdd_init_deinit_lock);
		hdd_err("Driver is unloading can not open the hdd");
		return -EBUSY;
	}

	if (cds_is_driver_recovering()) {
		mutex_unlock(&hdd_init_deinit_lock);
		hdd_err("WLAN is currently recovering; Please try again.");
		return -EBUSY;
	}

	if (qdf_atomic_read(&hdd_ctx->con_mode_flag)) {
		mutex_unlock(&hdd_init_deinit_lock);
		hdd_err("con_mode_handler is in progress; Please try again.");
		return -EBUSY;
	}
	hdd_start_driver_ops_timer(eHDD_DRV_OP_IFF_UP);

	/*
	 * This scenario can be hit in cases where in the wlan driver after
	 * registering the netdevices and there is a failure in driver
	 * initialization. So return error gracefully because the netdevices
	 * will be de-registered as part of the load failure.
	 */

	if (!cds_is_driver_loaded()) {
		hdd_err("Failed to start the wlan driver!!");
		ret = -EIO;
		goto err_hdd_hdd_init_deinit_lock;
	}


	ret = hdd_trigger_psoc_idle_restart(hdd_ctx);
	if (ret) {
		hdd_err("Failed to start WLAN modules return");
		goto err_hdd_hdd_init_deinit_lock;
	}


	if (!test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		ret = hdd_start_adapter(adapter);
		if (ret) {
			hdd_err("Failed to start adapter :%d",
				adapter->device_mode);
			goto err_hdd_hdd_init_deinit_lock;
		}
	}

	set_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);
	if (hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
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

err_hdd_hdd_init_deinit_lock:
	hdd_stop_driver_ops_timer();
	mutex_unlock(&hdd_init_deinit_lock);
	return ret;
}


/**
 * hdd_open() - Wrapper function for __hdd_open to protect it from SSR
 * @dev:	Pointer to net_device structure
 *
 * This is called in response to ifconfig up
 *
 * Return: 0 for success; non-zero for failure
 */
static int hdd_open(struct net_device *dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_open(dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __hdd_stop() - HDD stop function
 * @dev:	Pointer to net_device structure
 *
 * This is called in response to ifconfig down
 *
 * Return: 0 for success; non-zero for failure
 */
static int __hdd_stop(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;
	mac_handle_t mac_handle;

	hdd_enter_dev(dev);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_STOP_REQUEST,
		   adapter->session_id, adapter->device_mode);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret) {
		set_bit(DOWN_DURING_SSR, &adapter->event_flags);
		return ret;
	}

	/* Nothing to be done if the interface is not opened */
	if (false == test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_err("NETDEV Interface is not OPENED");
		return -ENODEV;
	}

	/* Make sure the interface is marked as closed */
	clear_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);

	mac_handle = hdd_ctx->mac_handle;

	if (!wlan_hdd_is_session_type_monitor(adapter->device_mode) &&
	    adapter->device_mode != QDF_FTM_MODE) {
		hdd_debug("Disabling Auto Power save timer");
		sme_ps_disable_auto_ps_timer(
			mac_handle,
			adapter->session_id);
	}

	/*
	 * Disable TX on the interface, after this hard_start_xmit() will not
	 * be called on that interface
	 */
	hdd_debug("Disabling queues, adapter device mode: %s(%d)",
		  hdd_device_mode_to_string(adapter->device_mode),
		  adapter->device_mode);

	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	if (adapter->device_mode == QDF_STA_MODE) {
		hdd_lpass_notify_stop(hdd_ctx);

		if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc))
			hdd_close_monitor_interface(hdd_ctx);
	}

	if (wlan_hdd_is_session_type_monitor(adapter->device_mode) &&
	    adapter->vdev) {
		ucfg_pkt_capture_deregister_callbacks(adapter->vdev);
		hdd_objmgr_put_vdev(adapter->vdev);
		adapter->vdev = NULL;
	}

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
		return 0;

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

	if (hdd_is_any_interface_open(hdd_ctx))
		hdd_psoc_idle_timer_start(hdd_ctx);

	hdd_exit();
	return 0;
}

/**
 * hdd_stop() - Wrapper function for __hdd_stop to protect it from SSR
 * @dev: pointer to net_device structure
 *
 * This is called in response to ifconfig down
 *
 * Return: 0 for success and error number for failure
 */
static int hdd_stop(struct net_device *dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_stop(dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __hdd_uninit() - HDD uninit function
 * @dev:	Pointer to net_device structure
 *
 * This is called during the netdev unregister to uninitialize all data
 * associated with the device
 *
 * Return: None
 */
static void __hdd_uninit(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	do {
		if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
			hdd_err("Invalid magic");
			break;
		}

		hdd_ctx = WLAN_HDD_GET_CTX(adapter);
		if (!hdd_ctx) {
			hdd_err("NULL hdd_ctx");
			break;
		}

		if (dev != adapter->dev)
			hdd_err("Invalid device reference");

		hdd_deinit_adapter(hdd_ctx, adapter, true);

		/* after uninit our adapter structure will no longer be valid */
		adapter->dev = NULL;
		adapter->magic = 0;
	} while (0);

	hdd_exit();
}

/**
 * hdd_uninit() - Wrapper function to protect __hdd_uninit from SSR
 * @dev: pointer to net_device structure
 *
 * This is called during the netdev unregister to uninitialize all data
 * associated with the device
 *
 * Return: none
 */
static void hdd_uninit(struct net_device *dev)
{
	cds_ssr_protect(__func__);
	__hdd_uninit(dev);
	cds_ssr_unprotect(__func__);
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

	if (cesium_nl_srv_sock == NULL) {
		hdd_err("NLINK:  cesium netlink_kernel_create failed");
		ret = -ECONNREFUSED;
	}

	return ret;
}

static void hdd_close_cesium_nl_sock(void)
{
	if (NULL != cesium_nl_srv_sock) {
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

	hdd_enter_dev(dev);

	if (netif_running(dev)) {
		hdd_err("On iface up, set mac address change isn't supported");
		return -EBUSY;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	qdf_mem_copy(&mac_addr, psta_mac_addr->sa_data, sizeof(mac_addr));
	adapter_temp = hdd_get_adapter_by_macaddr(hdd_ctx, mac_addr.bytes);
	if (adapter_temp) {
		if (!qdf_str_cmp(adapter_temp->dev->name, dev->name))
			return 0;
		hdd_err("%s adapter exist with same address " MAC_ADDRESS_STR,
			adapter_temp->dev->name,
			MAC_ADDR_ARRAY(mac_addr.bytes));
		return -EINVAL;
	}

	qdf_ret_status = wlan_hdd_validate_mac_address(&mac_addr);
	if (QDF_IS_STATUS_ERROR(qdf_ret_status))
		return -EINVAL;

	hdd_nofl_debug("Changing MAC to "
		       MAC_ADDRESS_STR " of the interface %s ",
		       MAC_ADDR_ARRAY(mac_addr.bytes), dev->name);

	hdd_update_dynamic_mac(hdd_ctx, &adapter->mac_addr, &mac_addr);
	memcpy(&adapter->mac_addr, psta_mac_addr->sa_data, ETH_ALEN);
	memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);

	hdd_exit();
	return qdf_ret_status;
}

/**
 * hdd_set_mac_address() - Wrapper function to protect __hdd_set_mac_address()
 *			function from SSR
 * @dev: pointer to net_device structure
 * @addr: Pointer to the sockaddr
 *
 * This function sets the user specified mac address using
 * the command ifconfig wlanX hw ether <mac address>.
 *
 * Return: 0 for success.
 */
static int hdd_set_mac_address(struct net_device *dev, void *addr)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_set_mac_address(dev, addr);
	cds_ssr_unprotect(__func__);

	return ret;
}

static uint8_t *wlan_hdd_get_derived_intf_addr(struct hdd_context *hdd_ctx)
{
	int i, j;

	i = qdf_ffz(hdd_ctx->derived_intf_addr_mask);
	if (i < 0 || i >= hdd_ctx->num_derived_addr)
		return NULL;
	qdf_atomic_set_bit(i, &hdd_ctx->derived_intf_addr_mask);
	hdd_nofl_debug("Assigning MAC from derived list" MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(hdd_ctx->derived_mac_addr[i].bytes));

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
	hdd_debug("Assigning MAC from provisioned list" MAC_ADDRESS_STR,
		  MAC_ADDR_ARRAY(hdd_ctx->provisioned_mac_addr[i].bytes));

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
					  MAC_ADDRESS_STR,
					  MAC_ADDR_ARRAY(releaseAddr));
			} else {
				qdf_atomic_clear_bit(
						mac_pos_in_mask, &hdd_ctx->
						derived_intf_addr_mask);
				hdd_debug("Releasing MAC from derived list");
				hdd_debug(MAC_ADDRESS_STR,
					  MAC_ADDR_ARRAY(releaseAddr));
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
		hdd_err("Releasing non existing MAC" MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(releaseAddr));
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
/**
 * __hdd_set_multicast_list() - set the multicast address list
 * @dev:	Pointer to the WLAN device.
 * @skb:	Pointer to OS packet (sk_buff).
 *
 * This funciton sets the multicast address list.
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
		hdd_debug("%s: Driver module is closed", __func__);
		return;
	}

	mc_list_request = qdf_mem_malloc(sizeof(*mc_list_request));
	if (!mc_list_request) {
		hdd_err("Cannot allocate mc_list_request");
		return;
	}

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
		if (mc_count > pmo_ucfg_max_mc_addr_supported(psoc)) {
			hdd_debug("Exceeded max MC filter addresses (%d). Allowing all MC frames by disabling MC address filtering",
				  pmo_ucfg_max_mc_addr_supported(psoc));
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
			hdd_debug("mlist[%d] = %pM", i,
				  mc_list_request->mc_addr[i].bytes);
			i++;
		}
	}

	adapter->mc_addr_list.mc_cnt = mc_count;
	mc_list_request->psoc = psoc;
	mc_list_request->vdev_id = adapter->session_id;
	mc_list_request->count = mc_count;

	errno = hdd_cache_mc_addr_list(mc_list_request);
	if (errno) {
		hdd_debug("Failed to cache MC address list for vdev %u; errno:%d",
			  adapter->session_id, errno);
		goto free_req;
	}

	hdd_enable_mc_addr_filtering(adapter, pmo_mc_list_change_notify);

free_req:
	qdf_mem_free(mc_list_request);
}


/**
 * hdd_set_multicast_list() - SSR wrapper function for __hdd_set_multicast_list
 * @dev: pointer to net_device
 *
 * Return: none
 */
static void hdd_set_multicast_list(struct net_device *dev)
{
	cds_ssr_protect(__func__);
	__hdd_set_multicast_list(dev);
	cds_ssr_unprotect(__func__);
}
#endif

#ifdef WLAN_FEATURE_ETHTOOL
static int hdd_ethtool_get_ts_info(struct net_device *dev,
	struct ethtool_ts_info *info)
{
	info->so_timestamping =
		SOF_TIMESTAMPING_RX_SOFTWARE |
		SOF_TIMESTAMPING_SOFTWARE |
		SOF_TIMESTAMPING_TX_SOFTWARE |
		SOF_TIMESTAMPING_TX_HARDWARE |
		SOF_TIMESTAMPING_RX_HARDWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;
	/* phc index is required only if ptp clock supported */
	info->phc_index =  -1;
	info->tx_types =
		(1 << HWTSTAMP_TX_OFF) |
		(1 << HWTSTAMP_TX_ON);
	info->rx_filters =
		(1 << HWTSTAMP_FILTER_NONE) |
		(1 << HWTSTAMP_FILTER_ALL);

	return 0;
}

static const struct ethtool_ops ethtool_drv_ops = {
	.get_drvinfo = cfg80211_get_drvinfo,
	.get_ts_info = hdd_ethtool_get_ts_info,
};

static void hdd_set_ethtool_ops(struct net_device *dev)
{
	dev->ethtool_ops = &ethtool_drv_ops;
}
#else
static void hdd_set_ethtool_ops(struct net_device *dev)
{
}
#endif

static const struct net_device_ops wlan_drv_ops = {
	.ndo_open = hdd_open,
	.ndo_stop = hdd_stop,
	.ndo_uninit = hdd_uninit,
	.ndo_start_xmit = hdd_hard_start_xmit,
	.ndo_tx_timeout = hdd_tx_timeout,
	.ndo_get_stats = hdd_get_stats,
	.ndo_do_ioctl = hdd_ioctl,
	.ndo_set_mac_address = hdd_set_mac_address,
	.ndo_select_queue = hdd_select_queue,
#ifdef WLAN_FEATURE_PACKET_FILTERING
	.ndo_set_rx_mode = hdd_set_multicast_list,
#endif
};

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/* Monitor mode net_device_ops, doesnot Tx and most of operations. */
static const struct net_device_ops wlan_mon_drv_ops = {
	.ndo_open = hdd_mon_open,
	.ndo_stop = hdd_stop,
	.ndo_get_stats = hdd_get_stats,
};

/**
 * hdd_set_station_ops() - update net_device ops for monitor mode
 * @dev: Handle to struct net_device to be updated.
 * Return: None
 */
void hdd_set_station_ops(struct net_device *dev)
{
	if (QDF_GLOBAL_MONITOR_MODE == cds_get_conparam())
		dev->netdev_ops = &wlan_mon_drv_ops;
	else
		dev->netdev_ops = &wlan_drv_ops;
}
#else
void hdd_set_station_ops(struct net_device *dev)
{
	dev->netdev_ops = &wlan_drv_ops;
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE
/* Packet Capture mode net_device_ops, doesnot Tx and most of operations. */
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

/**
 * hdd_alloc_station_adapter() - allocate the station hdd adapter
 * @hdd_ctx: global hdd context
 * @mac_addr: mac address to assign to the interface
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

	/* cfg80211 initialization and registration */
	dev = alloc_netdev_mq(sizeof(*adapter), name,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) || defined(WITH_BACKPORTS)
			      name_assign_type,
#endif
			      ((cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE ||
			       wlan_hdd_is_session_type_monitor(session_type)) ?
			       hdd_mon_mode_ether_setup : ether_setup),
			      NUM_TX_QUEUES);

	if (!dev) {
		hdd_err("Failed to allocate new net_device '%s'", name);
		return NULL;
	}

	adapter = netdev_priv(dev);

	qdf_mem_zero(adapter, sizeof(*adapter));
	sta_ctx = &adapter->session.station;
	qdf_mem_set(sta_ctx->conn_info.staId, sizeof(sta_ctx->conn_info.staId),
		    HDD_WLAN_INVALID_STA_ID);
	adapter->dev = dev;
	adapter->hdd_ctx = hdd_ctx;
	adapter->magic = WLAN_HDD_ADAPTER_MAGIC;
	adapter->session_id = HDD_SESSION_ID_INVALID;

	qdf_status = qdf_event_create(&adapter->qdf_session_open_event);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		goto free_net_dev;

	qdf_status = qdf_event_create(&adapter->qdf_session_close_event);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		goto free_net_dev;

	adapter->offloads_configured = false;
	adapter->is_link_up_service_needed = false;
	adapter->disconnection_in_progress = false;
	adapter->send_mode_change = true;

	/* Init the net_device structure */
	strlcpy(dev->name, name, IFNAMSIZ);

	qdf_mem_copy(dev->dev_addr, mac_addr, sizeof(tSirMacAddr));
	qdf_mem_copy(adapter->mac_addr.bytes, mac_addr, sizeof(tSirMacAddr));
	dev->watchdog_timeo = HDD_TX_TIMEOUT;

	if (hdd_ctx->config->enable_ip_tcp_udp_checksum_offload)
		dev->features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
	dev->features |= NETIF_F_RXCSUM;

	hdd_set_tso_flags(hdd_ctx, dev);

	if (wlan_hdd_is_session_type_monitor(session_type))
		hdd_set_pktcapture_ops(adapter->dev);
	else
		hdd_set_station_ops(adapter->dev);

	hdd_dev_setup_destructor(dev);
	dev->ieee80211_ptr = &adapter->wdev;
	dev->tx_queue_len = HDD_NETDEV_TX_QUEUE_LEN;
	adapter->wdev.wiphy = hdd_ctx->wiphy;
	adapter->wdev.netdev = dev;

	/* set dev's parent to underlying device */
	SET_NETDEV_DEV(dev, hdd_ctx->parent_dev);
	hdd_wmm_init(adapter);
	spin_lock_init(&adapter->pause_map_lock);
	adapter->start_time = qdf_system_ticks();
	adapter->last_time = adapter->start_time;

	return adapter;

free_net_dev:
	free_netdev(adapter->dev);

	return NULL;
}

static QDF_STATUS hdd_register_interface(struct hdd_adapter *adapter, bool rtnl_held)
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

		ret = register_netdevice(dev);
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

QDF_STATUS hdd_sme_open_session_callback(uint8_t session_id,
					 QDF_STATUS qdf_status)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD_CTX");
		return QDF_STATUS_E_FAILURE;
	}

	adapter = hdd_get_adapter_by_sme_session_id(hdd_ctx, session_id);
	if (NULL == adapter) {
		hdd_err("NULL adapter for %d", session_id);
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_status == QDF_STATUS_SUCCESS)
		set_bit(SME_SESSION_OPENED, &adapter->event_flags);

	qdf_event_set(&adapter->qdf_session_open_event);
	hdd_debug("session %d opened", adapter->session_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_sme_close_session_callback(uint8_t session_id)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD_CTX");
		return QDF_STATUS_E_FAILURE;
	}

	adapter = hdd_get_adapter_by_sme_session_id(hdd_ctx, session_id);
	if (NULL == adapter) {
		hdd_err("NULL adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("Invalid magic");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	/*
	 * For NAN Data interface, the close session results in the final
	 * indication to the userspace
	 */
	if (adapter->device_mode == QDF_NDI_MODE)
		hdd_ndp_session_end_handler(adapter);

	clear_bit(SME_SESSION_OPENED, &adapter->event_flags);

	/*
	 * We can be blocked while waiting for scheduled work to be
	 * flushed, and the adapter structure can potentially be freed, in
	 * which case the magic will have been reset.  So make sure the
	 * magic is still good, and hence the adapter structure is still
	 * valid, before signaling completion
	 */
	if (WLAN_HDD_ADAPTER_MAGIC == adapter->magic)
		qdf_event_set(&adapter->qdf_session_close_event);

	return QDF_STATUS_SUCCESS;
}

int hdd_vdev_ready(struct hdd_adapter *adapter)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	status = pmo_vdev_ready(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	status = ucfg_reg_11d_vdev_created_update(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	if (wma_capability_enhanced_mcast_filter())
		status = pmo_ucfg_enhanced_mc_filter_enable(vdev);
	else
		status = pmo_ucfg_enhanced_mc_filter_disable(vdev);

	hdd_objmgr_put_vdev(vdev);

	return qdf_status_to_os_return(status);
}

static void hdd_cleanup_cached_sta_info(struct hdd_adapter *adapter)
{
	uint16_t sta_id;
	struct wlan_ies *ies;

	for (sta_id = 0; sta_id < WLAN_MAX_STA_COUNT; sta_id++) {
		ies = &adapter->cache_sta_info[sta_id].assoc_req_ies;
		if (ies->data && ies->len) {
			qdf_mem_free(ies->data);
			ies->data = NULL;
			ies->len = 0;
		}
	}
}

int hdd_vdev_destroy(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	int errno;
	struct hdd_context *hdd_ctx;
	uint8_t vdev_id;
	struct wlan_objmgr_vdev *vdev;

	vdev_id = adapter->session_id;
	hdd_nofl_debug("destroying vdev %d", vdev_id);

	/* vdev created sanity check */
	if (!test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		hdd_nofl_debug("vdev %u does not exist", vdev_id);
		return -EINVAL;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	ucfg_pmo_del_wow_pattern(vdev);

	status = ucfg_reg_11d_vdev_delete_update(vdev);

	hdd_cleanup_cached_sta_info(adapter);

	/* Disable Scan and abort any pending scans for this vdev */
	ucfg_scan_set_vdev_del_in_progress(vdev);
	hdd_objmgr_put_vdev(vdev);
	wlan_hdd_scan_abort(adapter);

	/* close sme session (destroy vdev in firmware via legacy API) */
	qdf_event_reset(&adapter->qdf_session_close_event);
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = sme_close_session(hdd_ctx->mac_handle, adapter->session_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to close sme session; status:%d", status);
		goto release_vdev;
	}

	/* block on a completion variable until sme session is closed */
	status = qdf_wait_for_event_completion(
			&adapter->qdf_session_close_event,
			SME_CMD_VDEV_CREATE_DELETE_TIMEOUT);

	if (QDF_IS_STATUS_ERROR(status)) {
		clear_bit(SME_SESSION_OPENED, &adapter->event_flags);

		if (adapter->device_mode == QDF_NDI_MODE)
			hdd_ndp_session_end_handler(adapter);

		if (status == QDF_STATUS_E_TIMEOUT) {
			hdd_err("timed out waiting for sme close session");
			sme_cleanup_session(hdd_ctx->mac_handle, vdev_id);
		} else if (adapter->qdf_session_close_event.force_set) {
			hdd_info("SSR occurred during sme close session");
			ucfg_mlme_force_objmgr_vdev_peer_cleanup(vdev_id);
		} else {
			hdd_err("failed to wait for sme close session; status:%u",
				status);
		}
	}

release_vdev:

	/* do vdev logical destroy via objmgr */
	errno = hdd_objmgr_release_and_destroy_vdev(adapter);
	if (errno) {
		hdd_err("failed to destroy objmgr vdev; errno:%d", errno);
		return errno;
	}

	hdd_nofl_debug("vdev %d destroyed successfully", vdev_id);

	return 0;
}

static int hdd_set_sme_session_param(struct hdd_adapter *adapter,
			struct sme_session_params *session_param,
			csr_roam_complete_cb callback,
			void *callback_ctx)
{
	uint32_t type;
	uint32_t sub_type;
	QDF_STATUS status;

	/* determine vdev (sub)type */
	status = cds_get_vdev_types(adapter->device_mode, &type, &sub_type);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("failed to get vdev type: %d", status);
		return qdf_status_to_os_return(status);
	}
	session_param->sme_session_id = adapter->session_id;
	session_param->self_mac_addr = (uint8_t *)&adapter->mac_addr;
	session_param->type_of_persona = type;
	session_param->subtype_of_persona = sub_type;
	session_param->session_open_cb = hdd_sme_open_session_callback;
	session_param->session_close_cb = hdd_sme_close_session_callback;
	session_param->callback = callback;
	session_param->callback_ctx = callback_ctx;

	return 0;
}

static void
hdd_check_nss_chains_ini_params(struct mlme_nss_chains *vdev_ini_cfg,
				uint8_t rf_chains_supported)
{
	enum nss_chains_band_info band;

	for (band = NSS_CHAINS_BAND_2GHZ; band < NSS_CHAINS_BAND_MAX; band++) {
		vdev_ini_cfg->rx_nss[band] = QDF_MIN(vdev_ini_cfg->rx_nss[band],
						     rf_chains_supported);
		vdev_ini_cfg->tx_nss[band] = QDF_MIN(vdev_ini_cfg->tx_nss[band],
						     rf_chains_supported);
	}
}

void hdd_fill_nss_chain_params(struct hdd_context *hdd_ctx,
			       struct mlme_nss_chains *vdev_ini_cfg,
			       uint8_t device_mode)
{
	uint8_t nss_chain_shift;
	uint8_t max_supported_nss;
	struct hdd_config *config = hdd_ctx->config;
	uint8_t rf_chains_supported = hdd_ctx->num_rf_chains;

	nss_chain_shift = hdd_get_nss_chain_shift(device_mode);
	max_supported_nss = config->enable2x2 ? MAX_VDEV_NSS : 1;

	/* If the fw doesn't support two chains, num rf chains can max be 1 */
	vdev_ini_cfg->num_rx_chains[NSS_CHAINS_BAND_2GHZ] =
			QDF_MIN(GET_VDEV_NSS_CHAIN(config->num_rx_chains_2g,
						   nss_chain_shift),
				rf_chains_supported);

	vdev_ini_cfg->num_tx_chains[NSS_CHAINS_BAND_2GHZ] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->num_tx_chains_2g,
					   nss_chain_shift),
			rf_chains_supported);

	/* If 2x2 mode is disabled, then max rx, tx nss can be 1 */
	vdev_ini_cfg->rx_nss[NSS_CHAINS_BAND_2GHZ] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->rx_nss_2g,
					   nss_chain_shift),
			max_supported_nss);

	vdev_ini_cfg->tx_nss[NSS_CHAINS_BAND_2GHZ] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->tx_nss_2g,
					   nss_chain_shift),
			max_supported_nss);

	vdev_ini_cfg->num_tx_chains_11a =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->num_tx_chains_11a,
					   nss_chain_shift),
			rf_chains_supported);

	/* If the fw doesn't support two chains, num rf chains can max be 1 */
	vdev_ini_cfg->num_tx_chains_11b =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->num_tx_chains_11b,
					   nss_chain_shift),
			rf_chains_supported);

	vdev_ini_cfg->num_tx_chains_11g =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->num_tx_chains_11g,
					   nss_chain_shift),
			rf_chains_supported);

	vdev_ini_cfg->disable_rx_mrc[NSS_CHAINS_BAND_2GHZ] =
						config->disable_rx_mrc_2g;

	vdev_ini_cfg->disable_tx_mrc[NSS_CHAINS_BAND_2GHZ] =
						config->disable_tx_mrc_2g;

	/* config for 5ghz ini values */

	vdev_ini_cfg->num_rx_chains[NSS_CHAINS_BAND_5GHZ] =
			QDF_MIN(GET_VDEV_NSS_CHAIN(config->num_rx_chains_2g,
						   nss_chain_shift),
				rf_chains_supported);

	vdev_ini_cfg->num_tx_chains[NSS_CHAINS_BAND_5GHZ] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->num_tx_chains_2g,
					   nss_chain_shift),
			rf_chains_supported);

	/* If 2x2 mode is disabled, then max rx, tx nss can be 1 */
	vdev_ini_cfg->rx_nss[NSS_CHAINS_BAND_5GHZ] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->rx_nss_2g,
					   nss_chain_shift),
			max_supported_nss);

	vdev_ini_cfg->tx_nss[NSS_CHAINS_BAND_5GHZ] =
		QDF_MIN(GET_VDEV_NSS_CHAIN(config->tx_nss_2g,
					   nss_chain_shift),
			max_supported_nss);

	vdev_ini_cfg->disable_rx_mrc[NSS_CHAINS_BAND_5GHZ] =
						config->disable_rx_mrc_2g;

	vdev_ini_cfg->disable_tx_mrc[NSS_CHAINS_BAND_5GHZ] =
						config->disable_tx_mrc_2g;
	hdd_check_nss_chains_ini_params(vdev_ini_cfg, rf_chains_supported);
}

void
hdd_store_nss_chains_cfg_in_vdev(struct hdd_adapter *adapter)
{
	struct mlme_nss_chains vdev_ini_cfg;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wlan_objmgr_vdev *vdev;

	/* Populate the nss chain params from ini for this vdev type */
	hdd_fill_nss_chain_params(hdd_ctx, &vdev_ini_cfg, adapter->device_mode);

	vdev = hdd_objmgr_get_vdev(adapter);
	/* Store the nss chain config into the vdev */
	if (vdev) {
		sme_store_nss_chains_cfg_in_vdev(adapter->vdev, &vdev_ini_cfg);
		hdd_objmgr_put_vdev(vdev);
	} else {
		hdd_err("Vdev is NULL");
	}
}

#ifdef WLAN_FEATURE_NAN
static void hdd_configure_nan_support_mp0_discovery(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (QDF_NAN_DISC_MODE == adapter->device_mode) {
		sme_cli_set_command(
		adapter->session_id,
		WMI_VDEV_PARAM_ALLOW_NAN_INITIAL_DISCOVERY_OF_MP0_CLUSTER,
		hdd_ctx->config->support_mp0_discovery,
		VDEV_CMD);
	}
}

static void hdd_set_nan_feature_config(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (!wlan_hdd_nan_is_supported(hdd_ctx))
		return;

	if (QDF_NAN_DISC_MODE == adapter->device_mode ||
	     (QDF_STA_MODE == adapter->device_mode &&
	      (!hdd_ctx->nan_seperate_vdev_supported ||
	       !wlan_hdd_nan_separate_iface_supported(hdd_ctx))))
		sme_cli_set_command(adapter->session_id,
			WMI_VDEV_PARAM_ENABLE_DISABLE_NAN_CONFIG_FEATURES,
			hdd_ctx->config->nan_feature_config,
			VDEV_CMD);
}
#else
static inline void hdd_configure_nan_support_mp0_discovery(
						struct hdd_adapter *adapter)
{
}

static inline void hdd_set_nan_feature_config(struct hdd_adapter *adapter)
{
}
#endif

int hdd_vdev_create(struct hdd_adapter *adapter,
		    csr_roam_complete_cb callback, void *ctx)
{
	QDF_STATUS status;
	int errno;
	struct hdd_context *hdd_ctx;
	struct sme_session_params sme_session_params = {0};
	struct wlan_objmgr_vdev *vdev;

	hdd_nofl_debug("creating new vdev");

	/* do vdev create via objmgr */
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = hdd_objmgr_create_and_store_vdev(hdd_ctx->pdev, adapter);
	if (errno) {
		hdd_err("failed to create objmgr vdev: %d", errno);
		return errno;
	}

	/* Open a SME session (prepare vdev in firmware via legacy API) */
	status = qdf_event_reset(&adapter->qdf_session_open_event);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("failed to reinit session open event");
		errno = qdf_status_to_os_return(status);
		goto objmgr_vdev_destroy_procedure;
	}
	errno = hdd_set_sme_session_param(adapter, &sme_session_params,
					  callback, ctx);
	if (errno) {
		hdd_err("failed to populating SME params");
		goto objmgr_vdev_destroy_procedure;
	}

	status = sme_open_session(hdd_ctx->mac_handle, &sme_session_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to open sme session: %d", status);
		errno = qdf_status_to_os_return(status);
		goto objmgr_vdev_destroy_procedure;
	}

	/* block on a completion variable until sme session is opened */
	status = qdf_wait_for_event_completion(&adapter->qdf_session_open_event,
			SME_CMD_VDEV_CREATE_DELETE_TIMEOUT);
	if (QDF_STATUS_SUCCESS != status) {
		if (adapter->qdf_session_open_event.force_set)
			/*
			 * SSR/PDR has caused shutdown, which has forcefully
			 * set the event.
			 */
			hdd_err("Session open event forcefully set");

		else if (QDF_STATUS_E_TIMEOUT == status)
			hdd_err("Session failed to open within timeout period");
		else
			hdd_err("Failed to wait for session open event(status-%d)",
				status);
		errno = -ETIMEDOUT;
		set_bit(SME_SESSION_OPENED, &adapter->event_flags);
		goto hdd_vdev_destroy_procedure;
	}

	if (!test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		hdd_err("Session failed to open due to vdev create failure");
		errno = -EINVAL;
		goto objmgr_vdev_destroy_procedure;
	}

	/* firmware ready for component communication, raise vdev_ready event */
	errno = hdd_vdev_ready(adapter);
	if (errno) {
		hdd_err("failed to dispatch vdev ready event: %d", errno);
		goto hdd_vdev_destroy_procedure;
	}

	if (adapter->device_mode == QDF_STA_MODE) {
		hdd_debug("setting RTT mac randomization param: %d",
			hdd_ctx->config->enable_rtt_mac_randomization);
		errno = sme_cli_set_command(adapter->session_id,
			WMI_VDEV_PARAM_ENABLE_DISABLE_RTT_INITIATOR_RANDOM_MAC,
			hdd_ctx->config->enable_rtt_mac_randomization,
			VDEV_CMD);
		if (0 != errno)
			hdd_err("RTT mac randomization param set failed %d",
				errno);
	}

	if (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
		vdev = hdd_objmgr_get_vdev(adapter);
		if (!vdev)
			goto hdd_vdev_destroy_procedure;
		wlan_vdev_set_max_peer_count(vdev,
					     HDD_MAX_VDEV_PEER_COUNT);
		hdd_objmgr_put_vdev(vdev);
	}

	/* store the ini and dynamic nss chain params to the vdev object */
	hdd_store_nss_chains_cfg_in_vdev(adapter);

	hdd_nofl_debug("vdev %d created successfully", adapter->session_id);

	hdd_configure_nan_support_mp0_discovery(adapter);
	hdd_set_nan_feature_config(adapter);

	return 0;

	/*
	 * Due to legacy constraints, we need to destroy in the same order as
	 * create. So, split error handling into 2 cases to accommodate.
	 */

objmgr_vdev_destroy_procedure:
	QDF_BUG(!hdd_objmgr_release_and_destroy_vdev(adapter));

	return errno;

hdd_vdev_destroy_procedure:
	QDF_BUG(!hdd_vdev_destroy(adapter));

	return errno;
}

QDF_STATUS hdd_init_station_mode(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx = &adapter->session.station;
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;
	int ret_val;
	mac_handle_t mac_handle;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	mac_handle = hdd_ctx->mac_handle;
	sme_set_curr_device_mode(mac_handle, adapter->device_mode);
	sme_set_pdev_ht_vht_ies(mac_handle, hdd_ctx->config->enable2x2);
	sme_set_vdev_ies_per_band(mac_handle, adapter->session_id);

	hdd_roam_profile_init(adapter);
	hdd_register_wext(adapter->dev);

	hdd_conn_set_connection_state(adapter, eConnectionState_NotConnected);

	qdf_mem_set(sta_ctx->conn_info.staId,
		sizeof(sta_ctx->conn_info.staId), HDD_WLAN_INVALID_STA_ID);
	sme_roam_reset_configs(mac_handle, adapter->session_id);
	/* set fast roaming capability in sme session */
	status = sme_config_fast_roaming(mac_handle, adapter->session_id,
					 true);
	/* Set the default operation channel */
	sta_ctx->conn_info.operationChannel =
		hdd_ctx->config->OperatingChannel;

	/* Make the default Auth Type as OPEN */
	sta_ctx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;

	status = hdd_init_tx_rx(adapter);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("hdd_init_tx_rx() failed, status code %08d [x%08x]",
		       status, status);
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

	ret_val = sme_cli_set_command(adapter->session_id,
				      WMI_PDEV_PARAM_BURST_ENABLE,
				      HDD_ENABLE_SIFS_BURST_DEFAULT,
				      PDEV_CMD);
	if (ret_val)
		hdd_err("WMI_PDEV_PARAM_BURST_ENABLE set failed %d", ret_val);

	/*
	 * In case of USB tethering, LRO is disabled. If SSR happened
	 * during that time, then as part of SSR init, do not enable
	 * the LRO again. Keep the LRO state same as before SSR.
	 */
	if (hdd_ctx->config->lro_enable &&
	    !(qdf_atomic_read(&hdd_ctx->vendor_disable_lro_flag)))
		adapter->dev->features |= NETIF_F_LRO;

	/* rcpi info initialization */
	qdf_mem_zero(&adapter->rcpi, sizeof(adapter->rcpi));

	return QDF_STATUS_SUCCESS;

error_wmm_init:
	clear_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);
	hdd_deinit_tx_rx(adapter);
error_init_txrx:
	hdd_unregister_wext(adapter->dev);
	QDF_BUG(!hdd_vdev_destroy(adapter));

	return status;
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

	if (adapter->dev) {
		if (rtnl_held)
			adapter->dev->wireless_handlers = NULL;
		else {
			rtnl_lock();
			adapter->dev->wireless_handlers = NULL;
			rtnl_unlock();
		}
	}

	if (test_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags)) {
		hdd_deinit_tx_rx(adapter);
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

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_MONITOR_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_IBSS_MODE:
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

	hdd_exit();
}

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

	hdd_nud_deinit_tracking(adapter);
	qdf_mutex_destroy(&adapter->disconnection_status_lock);
	hdd_periodic_sta_stats_mutex_destroy(adapter);
	hdd_apf_context_destroy(adapter);
	qdf_spinlock_destroy(&adapter->vdev_lock);

	wlan_hdd_debugfs_csr_deinit(adapter);
	if (adapter->device_mode == QDF_STA_MODE)
		hdd_sysfs_destroy_adapter_root_obj(adapter);

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
			unregister_netdevice(dev);
		else
			unregister_netdev(dev);
		/*
		 * Note that the adapter is no longer valid at this point
		 * since the memory has been reclaimed
		 */
	}
}

static QDF_STATUS hdd_check_for_existing_macaddr(struct hdd_context *hdd_ctx,
						 tSirMacAddr macAddr)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (!qdf_mem_cmp(adapter->mac_addr.bytes,
				 macAddr, sizeof(tSirMacAddr))) {
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_FW_LOGS_BASED_ON_INI
/**
 * hdd_set_fw_log_params() - Set log parameters to FW
 * @hdd_ctx: HDD Context
 * @adapter: HDD Adapter
 *
 * This function set the FW Debug log level based on the INI.
 *
 * Return: None
 */
static void hdd_set_fw_log_params(struct hdd_context *hdd_ctx,
				  struct hdd_adapter *adapter)
{
	uint8_t count = 0, numentries = 0,
			moduleloglevel[FW_MODULE_LOG_LEVEL_STRING_LENGTH];
	uint32_t value = 0;
	int ret;

	if (QDF_GLOBAL_FTM_MODE == cds_get_conparam() ||
	    (!hdd_ctx->config->enable_fw_log)) {
		hdd_debug("enable_fw_log not enabled in INI or in FTM mode return");
		return;
	}

	/* Enable FW logs based on INI configuration */
	hdd_ctx->fw_log_settings.dl_type =
			hdd_ctx->config->enableFwLogType;
	ret = sme_cli_set_command(adapter->session_id,
			WMI_DBGLOG_TYPE,
			hdd_ctx->config->enableFwLogType,
			DBG_CMD);
	if (ret != 0)
		hdd_err("Failed to enable FW log type ret %d",
			ret);

	hdd_ctx->fw_log_settings.dl_loglevel =
			hdd_ctx->config->enableFwLogLevel;
	ret = sme_cli_set_command(adapter->session_id,
			WMI_DBGLOG_LOG_LEVEL,
			hdd_ctx->config->enableFwLogLevel,
			DBG_CMD);
	if (ret != 0)
		hdd_err("Failed to enable FW log level ret %d",
			ret);

	hdd_string_to_u8_array(
		hdd_ctx->config->enableFwModuleLogLevel,
		moduleloglevel,
		&numentries,
		FW_MODULE_LOG_LEVEL_STRING_LENGTH);

	while (count < numentries) {
		/*
		 * FW module log level input string looks like
		 * below:
		 * gFwDebugModuleLoglevel=<FW Module ID>,
		 * <Log Level>,...
		 * For example:
		 * gFwDebugModuleLoglevel=
		 * 1,0,2,1,3,2,4,3,5,4,6,5,7,6
		 * Above input string means :
		 * For FW module ID 1 enable log level 0
		 * For FW module ID 2 enable log level 1
		 * For FW module ID 3 enable log level 2
		 * For FW module ID 4 enable log level 3
		 * For FW module ID 5 enable log level 4
		 * For FW module ID 6 enable log level 5
		 * For FW module ID 7 enable log level 6
		 */

		if ((moduleloglevel[count] > WLAN_MODULE_ID_MAX)
			|| (moduleloglevel[count + 1] > DBGLOG_LVL_MAX)) {
			hdd_err("Module id %d and dbglog level %d input length is more than max",
				moduleloglevel[count],
				moduleloglevel[count + 1]);
			return;
		}

		value = moduleloglevel[count] << 16;
		value |= moduleloglevel[count + 1];
		ret = sme_cli_set_command(adapter->session_id,
				WMI_DBGLOG_MOD_LOG_LEVEL,
				value, DBG_CMD);
		if (ret != 0)
			hdd_err("Failed to enable FW module log level %d ret %d",
				value, ret);

		count += 2;
	}

}
#else
static void hdd_set_fw_log_params(struct hdd_context *hdd_ctx,
				  struct hdd_adapter *adapter)
{
}

#endif

/**
 * hdd_configure_chain_mask() - programs chain mask to firmware
 * @adapter: HDD adapter
 *
 * Return: 0 on success or errno on failure
 */
static int hdd_configure_chain_mask(struct hdd_adapter *adapter)
{
	int ret_val;
	QDF_STATUS status;
	struct wma_caps_per_phy non_dbs_phy_cap;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	hdd_debug("enable2x2: %d, lte_coex: %d, ChainMask1x1: tx: %d rx: %d",
		  hdd_ctx->config->enable2x2, hdd_ctx->lte_coex_ant_share,
		  hdd_ctx->config->txchainmask1x1,
		  hdd_ctx->config->rxchainmask1x1);
	hdd_debug("disable_DBS: %d, tx_chain_mask_2g: %d, rx_chain_mask_2g: %d",
		  hdd_ctx->config->dual_mac_feature_disable,
		  hdd_ctx->config->tx_chain_mask_2g,
		  hdd_ctx->config->rx_chain_mask_2g);
	hdd_debug("tx_chain_mask_5g: %d, rx_chain_mask_5g: %d",
		  hdd_ctx->config->tx_chain_mask_5g,
		  hdd_ctx->config->rx_chain_mask_5g);
	hdd_debug("enable_bt_chain_separation %d",
		  hdd_ctx->config->enable_bt_chain_separation);

	status = wma_get_caps_for_phyidx_hwmode(&non_dbs_phy_cap,
						HW_MODE_DBS_NONE,
						CDS_BAND_ALL);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("couldn't get phy caps. skip chain mask programming");
		return qdf_status_to_os_return(status);
	}

	if (non_dbs_phy_cap.tx_chain_mask_2G < 3 ||
	    non_dbs_phy_cap.rx_chain_mask_2G < 3 ||
	    non_dbs_phy_cap.tx_chain_mask_5G < 3 ||
	    non_dbs_phy_cap.rx_chain_mask_5G < 3) {
		hdd_debug("firmware not capable. skip chain mask programming");
		return 0;
	}

	if (hdd_ctx->config->enable2x2 &&
	    !hdd_ctx->config->enable_bt_chain_separation) {
		hdd_debug("2x2 enabled. skip chain mask programming");
		return 0;
	}

	if (hdd_ctx->config->dual_mac_feature_disable !=
	    DISABLE_DBS_CXN_AND_SCAN) {
		hdd_debug("DBS enabled(%d). skip chain mask programming",
			  hdd_ctx->config->dual_mac_feature_disable);
		return 0;
	}

	if (hdd_ctx->lte_coex_ant_share) {
		hdd_debug("lte ant sharing enabled. skip chainmask programming");
		return 0;
	}

	if (hdd_ctx->config->txchainmask1x1) {
		ret_val = sme_cli_set_command(adapter->session_id,
					      WMI_PDEV_PARAM_TX_CHAIN_MASK,
					      hdd_ctx->config->txchainmask1x1,
					      PDEV_CMD);
		if (ret_val)
			goto error;
	}

	if (hdd_ctx->config->rxchainmask1x1) {
		ret_val = sme_cli_set_command(adapter->session_id,
					      WMI_PDEV_PARAM_RX_CHAIN_MASK,
					      hdd_ctx->config->rxchainmask1x1,
					      PDEV_CMD);
		if (ret_val)
			goto error;
	}

	if (hdd_ctx->config->txchainmask1x1 ||
	    hdd_ctx->config->rxchainmask1x1) {
		hdd_debug("band agnostic tx/rx chain mask set. skip per band chain mask");
		return 0;
	}

	if (hdd_ctx->config->tx_chain_mask_2g) {
		ret_val = sme_cli_set_command(adapter->session_id,
				WMI_PDEV_PARAM_TX_CHAIN_MASK_2G,
				hdd_ctx->config->tx_chain_mask_2g, PDEV_CMD);
		if (0 != ret_val)
			goto error;
	}

	if (hdd_ctx->config->rx_chain_mask_2g) {
		ret_val = sme_cli_set_command(adapter->session_id,
				WMI_PDEV_PARAM_RX_CHAIN_MASK_2G,
				hdd_ctx->config->rx_chain_mask_2g, PDEV_CMD);
		if (0 != ret_val)
			goto error;
	}

	if (hdd_ctx->config->tx_chain_mask_5g) {
		ret_val = sme_cli_set_command(adapter->session_id,
				WMI_PDEV_PARAM_TX_CHAIN_MASK_5G,
				hdd_ctx->config->tx_chain_mask_5g, PDEV_CMD);
		if (0 != ret_val)
			goto error;
	}

	if (hdd_ctx->config->rx_chain_mask_5g) {
		ret_val = sme_cli_set_command(adapter->session_id,
				WMI_PDEV_PARAM_RX_CHAIN_MASK_5G,
				hdd_ctx->config->rx_chain_mask_5g, PDEV_CMD);
		if (0 != ret_val)
			goto error;
	}

	return 0;

error:
	hdd_debug("WMI PDEV set param failed %d", ret_val);
	return -EINVAL;
}

/**
 * hdd_send_coex_config_params() - Send coex config params to FW
 * @hdd_ctx: HDD context
 *
 * This function is used to send all coex config related params to FW
 *
 * Return: 0 on success, -EINVAL on failure
 */
static int hdd_send_coex_config_params(struct hdd_context *hdd_ctx,
					      struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct coex_config_params coex_cfg_params = {0};
	struct hdd_config *config;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is invalid");
		goto err;
	}

	if (!adapter) {
		hdd_err("adapter is invalid");
		goto err;
	}

	config = hdd_ctx->config;
	if (!config) {
		hdd_err("hdd config got NULL");
		goto err;
	}

	coex_cfg_params.vdev_id = adapter->session_id;
	coex_cfg_params.config_type = WMI_COEX_CONFIG_TX_POWER;
	coex_cfg_params.config_arg1 = config->set_max_tx_power_for_btc;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex Tx power");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_HANDOVER_RSSI;
	coex_cfg_params.config_arg1 = config->set_wlan_low_rssi_threshold;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex handover RSSI");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_BTC_MODE;
	coex_cfg_params.config_arg1 = config->set_btc_mode;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BTC mode");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_ANTENNA_ISOLATION;
	coex_cfg_params.config_arg1 = config->set_antenna_isolation;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex antenna isolation");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_BT_LOW_RSSI_THRESHOLD;
	coex_cfg_params.config_arg1 = config->set_bt_low_rssi_threshold;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BT low RSSI threshold");
		goto err;
	}

	coex_cfg_params.config_type = WMI_COEX_CONFIG_BT_INTERFERENCE_LEVEL;
	coex_cfg_params.config_arg1 = config->set_bt_interference_low_ll;
	coex_cfg_params.config_arg2 = config->set_bt_interference_low_ul;
	coex_cfg_params.config_arg3 = config->set_bt_interference_medium_ll;
	coex_cfg_params.config_arg4 = config->set_bt_interference_medium_ul;
	coex_cfg_params.config_arg5 = config->set_bt_interference_high_ll;
	coex_cfg_params.config_arg6 = config->set_bt_interference_high_ul;

	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex BT interference level");
		goto err;
	}

	if (wlan_hdd_mpta_helper_enable(config))
		goto err;

	return 0;
err:
	return -EINVAL;
}

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
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(adapter->dev);

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return -EINVAL;

	if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_debug("FTM Mode is active; nothing to do");
		return 0;
	}

	ret = sme_cli_set_command(adapter->session_id,
				WMI_PDEV_PARAM_DTIM_SYNTH,
				hdd_ctx->config->enable_lprx, PDEV_CMD);
	if (ret) {
		hdd_err("Failed to set LPRx");
		goto error;
	}


	ret = sme_cli_set_command(
			adapter->session_id,
			WMI_PDEV_PARAM_1CH_DTIM_OPTIMIZED_CHAIN_SELECTION,
			hdd_ctx->config->enable_dtim_selection_diversity,
			PDEV_CMD);
	if (ret) {
		hdd_err("Failed to set DTIM_OPTIMIZED_CHAIN_SELECTION");
		goto error;
	}

	ret = sme_cli_set_command(
			adapter->session_id,
			WMI_PDEV_PARAM_TX_SCH_DELAY,
			hdd_ctx->config->enable_tx_sch_delay,
			PDEV_CMD);
	if (ret) {
		hdd_err("Failed to set WMI_PDEV_PARAM_TX_SCH_DELAY");
		goto error;
	}

	ret = sme_cli_set_command(
			adapter->session_id,
			WMI_PDEV_PARAM_SECONDARY_RETRY_ENABLE,
			hdd_ctx->config->enable_secondary_rate,
			PDEV_CMD);
	if (ret) {
		hdd_err("Failed to set WMI_PDEV_PARAM_SECONDARY_RETRY_ENABLE");
		goto error;
	}

	if (adapter->device_mode == QDF_STA_MODE) {
		sme_set_smps_cfg(adapter->session_id,
					HDD_STA_SMPS_PARAM_UPPER_BRSSI_THRESH,
					hdd_ctx->config->upper_brssi_thresh);

		sme_set_smps_cfg(adapter->session_id,
					HDD_STA_SMPS_PARAM_LOWER_BRSSI_THRESH,
					hdd_ctx->config->lower_brssi_thresh);

		sme_set_smps_cfg(adapter->session_id,
					HDD_STA_SMPS_PARAM_DTIM_1CHRX_ENABLE,
					hdd_ctx->config->enable_dtim_1chrx);
	}

	if (hdd_ctx->config->enable2x2) {
		hdd_debug("configuring 2x2 mode fw params");

		ret = sme_cli_set_command(adapter->session_id,
				       WMI_PDEV_PARAM_ENABLE_CCK_TXFIR_OVERRIDE,
				    hdd_ctx->config->enable_cck_tx_fir_override,
					  PDEV_CMD);
		if (ret) {
			hdd_err("WMI_PDEV_PARAM_ENABLE_CCK_TXFIR_OVERRIDE set failed %d",
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
		ret = sme_cli_set_command(adapter->session_id,
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

	ret = sme_cli_set_command(adapter->session_id,
				  WMI_PDEV_PARAM_HYST_EN,
				  hdd_ctx->config->enableMemDeepSleep,
				  PDEV_CMD);
	if (ret) {
		hdd_err("WMI_PDEV_PARAM_HYST_EN set failed %d", ret);
		goto error;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMI_PDEV_PARAM_SET_DFS_CHAN_AGEOUT_TIME,
				  hdd_ctx->config->dfs_chan_ageout_time,
				  PDEV_CMD);
	if (ret) {
		hdd_err("WMI_PDEV_PARAM_SET_DFS_CHAN_AGEOUT_TIME set failed %d",
			ret);
		goto error;
	}

	ret = sme_cli_set_command(adapter->session_id,
				  WMI_VDEV_PARAM_ENABLE_RTSCTS,
				  hdd_ctx->config->rts_profile,
				  VDEV_CMD);
	if (ret) {
		hdd_err("FAILED TO SET RTSCTS Profile ret:%d", ret);
		goto error;
	}

	hdd_debug("SET AMSDU num %d", hdd_ctx->config->max_amsdu_num);

	ret = wma_cli_set_command(adapter->session_id,
				  GEN_VDEV_PARAM_AMSDU,
				  hdd_ctx->config->max_amsdu_num,
				  GEN_CMD);
	if (ret != 0) {
		hdd_err("GEN_VDEV_PARAM_AMSDU set failed %d", ret);
		goto error;
	}

	hdd_set_fw_log_params(hdd_ctx, adapter);

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
	init_completion(&adapter->roaming_comp_var);
	init_completion(&adapter->linkup_event_var);
	init_completion(&adapter->cancel_rem_on_chan_var);
	init_completion(&adapter->rem_on_chan_ready_event);
	init_completion(&adapter->sta_authorized_event);
	init_completion(&adapter->offchannel_tx_event);
	init_completion(&adapter->tx_action_cnf_event);
	init_completion(&adapter->ibss_peer_info_comp);
	init_completion(&adapter->lfr_fw_status.disable_lfr_event);
}

static void hdd_reset_locally_admin_bit(struct hdd_context *hdd_ctx,
					tSirMacAddr macAddr)
{
	int i;
	/*
	 * Reset locally administered bit for dynamic_mac_list
	 * also as while releasing the MAC address for any
	 * interface mac will be compared with dynamic mac list
	 */
	for (i = 0; i < QDF_MAX_CONCURRENCY_PERSONA; i++) {
		if (!qdf_mem_cmp(
			macAddr,
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
	WLAN_HDD_RESET_LOCALLY_ADMINISTERED_BIT(macAddr);
	hdd_debug("locally administered bit reset in sta mode: "
		 MAC_ADDRESS_STR, MAC_ADDR_ARRAY(macAddr));
}


#if defined(WLAN_FEATURE_NAN) && \
	   (KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE)
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
#else
static void wlan_hdd_set_nan_if_type(struct hdd_adapter *adapter)
{
}
#endif

/**
 * hdd_open_adapter() - open and setup the hdd adatper
 * @hdd_ctx: global hdd context
 * @session_type: type of the interface to be created
 * @iface_name: User-visible name of the interface
 * @macAddr: MAC address to assign to the interface
 * @name_assign_type: the name of assign type of the netdev
 * @rtnl_held: the rtnl lock hold flag
 *
 * This function open and setup the hdd adpater according to the device
 * type request, assign the name, the mac address assigned, and then prepared
 * the hdd related parameters, queue, lock and ready to start.
 *
 * Return: the pointer of hdd adapter, otherwise NULL.
 */
struct hdd_adapter *hdd_open_adapter(struct hdd_context *hdd_ctx, uint8_t session_type,
				const char *iface_name, tSirMacAddr macAddr,
				unsigned char name_assign_type,
				bool rtnl_held)
{
	struct hdd_adapter *adapter = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (hdd_ctx->current_intf_count >= WLAN_MAX_VDEVS) {
		/*
		 * Max limit reached on the number of vdevs configured by the
		 * host. Return error
		 */
		hdd_err("Unable to add virtual intf: currentVdevCnt=%d,hostConfiguredVdevCnt=%d",
			hdd_ctx->current_intf_count, hdd_ctx->max_intf_count);
		return NULL;
	}

	status = wlan_hdd_validate_mac_address((struct qdf_mac_addr *)macAddr);
	if (QDF_IS_STATUS_ERROR(status)) {
		/* Not received valid macAddr */
		hdd_err("Unable to add virtual intf: Not able to get valid mac address");
		return NULL;
	}

	status = hdd_check_for_existing_macaddr(hdd_ctx, macAddr);
	if (QDF_STATUS_E_FAILURE == status) {
		hdd_err("Duplicate MAC addr: " MAC_ADDRESS_STR
				" already exists",
				MAC_ADDR_ARRAY(macAddr));
		return NULL;
	}

	switch (session_type) {
	case QDF_STA_MODE:
		if (!hdd_ctx->config->mac_provision) {
			hdd_reset_locally_admin_bit(hdd_ctx, macAddr);
			/*
			 * After resetting locally administered bit
			 * again check if the new mac address is already
			 * exists.
			 */
			status = hdd_check_for_existing_macaddr(hdd_ctx,
								macAddr);
			if (QDF_STATUS_E_FAILURE == status) {
				hdd_err("Duplicate MAC addr: " MAC_ADDRESS_STR
					" already exists",
					MAC_ADDR_ARRAY(macAddr));
				return NULL;
			}
		}

	/* fall through */
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_OCB_MODE:
	case QDF_NDI_MODE:
	case QDF_MONITOR_MODE:
	case QDF_NAN_DISC_MODE:
		adapter = hdd_alloc_station_adapter(hdd_ctx, macAddr,
						    name_assign_type,
						    iface_name, session_type);

		if (NULL == adapter) {
			hdd_err("failed to allocate adapter for session %d",
					session_type);
			return NULL;
		}

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
		status = hdd_register_interface(adapter, rtnl_held);
		if (QDF_STATUS_SUCCESS != status)
			goto err_free_netdev;

		/* Stop the Interface TX queue. */
		hdd_debug("Disabling queues");
		wlan_hdd_netif_queue_control(adapter,
					WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);

		hdd_nud_init_tracking(adapter);
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE)
			hdd_sysfs_create_adapter_root_obj(adapter);
		qdf_mutex_create(&adapter->disconnection_status_lock);
		hdd_periodic_sta_stats_mutex_create(adapter);

		break;

	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
		adapter = hdd_wlan_create_ap_dev(hdd_ctx, macAddr,
						 name_assign_type,
						 (uint8_t *) iface_name);
		if (NULL == adapter) {
			hdd_err("failed to allocate adapter for session %d",
					  session_type);
			return NULL;
		}

		adapter->wdev.iftype =
			(session_type ==
			 QDF_SAP_MODE) ? NL80211_IFTYPE_AP :
			NL80211_IFTYPE_P2P_GO;
		adapter->device_mode = session_type;

		status = hdd_register_interface(adapter, rtnl_held);
		if (QDF_STATUS_SUCCESS != status)
			goto err_free_netdev;

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
		break;
	case QDF_FTM_MODE:
		adapter = hdd_alloc_station_adapter(hdd_ctx, macAddr,
						    name_assign_type,
						    iface_name, session_type);
		if (NULL == adapter) {
			hdd_err("Failed to allocate adapter for FTM mode");
			return NULL;
		}
		adapter->wdev.iftype = NL80211_IFTYPE_STATION;
		adapter->device_mode = session_type;
		status = hdd_register_interface(adapter, rtnl_held);
		if (QDF_STATUS_SUCCESS != status)
			goto err_free_netdev;

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

	qdf_spinlock_create(&adapter->vdev_lock);

	hdd_init_completion(adapter);
	INIT_WORK(&adapter->scan_block_work, wlan_hdd_cfg80211_scan_block_cb);
	INIT_WORK(&adapter->sap_stop_bss_work,
		  hdd_stop_sap_due_to_invalid_channel);
	qdf_list_create(&adapter->blocked_scan_request_q, WLAN_MAX_SCAN_COUNT);
	qdf_mutex_create(&adapter->blocked_scan_request_q_lock);
	qdf_event_create(&adapter->acs_complete_event);
	qdf_event_create(&adapter->peer_cleanup_done);

	if (QDF_STATUS_SUCCESS == status) {
		/* Add it to the hdd's session list. */
		status = hdd_add_adapter_back(hdd_ctx, adapter);
	}

	if (QDF_STATUS_SUCCESS != status) {
		if (NULL != adapter) {
			hdd_cleanup_adapter(hdd_ctx, adapter, rtnl_held);
			adapter = NULL;
		}

		return NULL;
	}
	hdd_apf_context_init(adapter);

	if (QDF_STATUS_SUCCESS == status) {
		policy_mgr_set_concurrency_mode(hdd_ctx->psoc,
			session_type);

		/* Adapter successfully added. Increment the vdev count */
		hdd_ctx->current_intf_count++;

		hdd_debug("current_intf_count=%d",
		       hdd_ctx->current_intf_count);

		hdd_check_and_restart_sap_with_non_dfs_acs();
	}

	if (QDF_STATUS_SUCCESS != hdd_debugfs_init(adapter))
		hdd_err("Interface %s wow debug_fs init failed",
			netdev_name(adapter->dev));

	hdd_debug("%s interface created. iftype: %d", netdev_name(adapter->dev),
		  session_type);

	if (adapter->device_mode == QDF_STA_MODE)
		wlan_hdd_debugfs_csr_init(adapter);

	hdd_set_ethtool_ops(adapter->dev);

	hdd_periodic_sta_stats_init(adapter);

	return adapter;

err_free_netdev:
	wlan_hdd_release_intf_addr(hdd_ctx, adapter->mac_addr.bytes);
	free_netdev(adapter->dev);

	return NULL;
}

QDF_STATUS hdd_close_adapter(struct hdd_context *hdd_ctx, struct hdd_adapter *adapter,
			     bool rtnl_held)
{
	/*
	 * Here we are stopping global bus_bw timer & work per adapter.
	 *
	 * The reason is to fix one race condition between
	 * bus bandwidth work and cleaning up an adapter.
	 * Under some conditions, it is possible for the bus bandwidth
	 * work to access a particularly destroyed adapter, leading to
	 * use-after-free.
	 */
	hdd_bus_bw_compute_timer_stop(hdd_ctx);

	qdf_list_destroy(&adapter->blocked_scan_request_q);
	qdf_mutex_destroy(&adapter->blocked_scan_request_q_lock);
	qdf_event_destroy(&adapter->acs_complete_event);
	qdf_event_destroy(&adapter->peer_cleanup_done);

	/* cleanup adapter */
	policy_mgr_clear_concurrency_mode(hdd_ctx->psoc,
					  adapter->device_mode);
	hdd_remove_adapter(hdd_ctx, adapter);
	hdd_cleanup_adapter(hdd_ctx, adapter, rtnl_held);

	/* conditionally restart the bw timer */
	hdd_bus_bw_compute_timer_try_start(hdd_ctx);

	/* Adapter removed. Decrement vdev count */
	if (hdd_ctx->current_intf_count != 0)
		hdd_ctx->current_intf_count--;

	/* Fw will take care incase of concurrency */
	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_close_all_adapters - Close all open adapters
 * @hdd_ctx:	Hdd context
 * rtnl_held:	True if RTNL lock held
 *
 * Close all open adapters.
 *
 * Return: QDF status code
 */
QDF_STATUS hdd_close_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held)
{
	struct hdd_adapter *adapter;
	QDF_STATUS status;

	hdd_enter();

	do {
		status = hdd_remove_front_adapter(hdd_ctx, &adapter);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			wlan_hdd_release_intf_addr(hdd_ctx,
						   adapter->mac_addr.bytes);
			hdd_cleanup_adapter(hdd_ctx, adapter, rtnl_held);

			/* Adapter removed. Decrement vdev count */
			if (hdd_ctx->current_intf_count != 0)
				hdd_ctx->current_intf_count--;
		}
	} while (QDF_IS_STATUS_SUCCESS(status));

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

void wlan_hdd_reset_prob_rspies(struct hdd_adapter *adapter)
{
	struct qdf_mac_addr *bssid = NULL;
	tSirUpdateIE updateIE;
	mac_handle_t mac_handle;

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	{
		struct hdd_station_ctx *sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		bssid = &sta_ctx->conn_info.bssId;
		break;
	}
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_IBSS_MODE:
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

	qdf_copy_macaddr(&updateIE.bssid, bssid);
	updateIE.smeSessionId = adapter->session_id;
	updateIE.ieBufferlength = 0;
	updateIE.pAdditionIEBuffer = NULL;
	updateIE.append = true;
	updateIE.notify = false;
	mac_handle = hdd_adapter_get_mac_handle(adapter);
	if (sme_update_add_ie(mac_handle,
			      &updateIE,
			      eUPDATE_IE_PROBE_RESP) == QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass on PROBE_RSP_BCN data to PE");
	}
}

static void
hdd_peer_cleanup(struct hdd_context *hdd_ctx, struct hdd_adapter *adapter)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	/* Check if there is any peer present on the NDI */
	if (!hdd_any_valid_peer_present(adapter))
		return;

	qdf_status = sme_handle_peer_cleanup(hdd_ctx->mac_handle,
					     adapter->session_id);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return;
	qdf_status = qdf_wait_for_event_completion(&adapter->peer_cleanup_done,
						   WLAN_WAIT_PEER_CLEANUP);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		hdd_debug("peer_cleanup_done wait fail");
}

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

QDF_STATUS hdd_stop_adapter(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter)
{
	return hdd_stop_adapter_ext(hdd_ctx, adapter, 0);
}

QDF_STATUS hdd_stop_adapter_ext(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter,
				enum hdd_adapter_stop_flag_t flag)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_SUCCESS;
	struct csr_roam_profile *roam_profile;
	union iwreq_data wrqu;
	tSirUpdateIE updateIE;
	unsigned long rc;
	tsap_config_t *sap_config;
	mac_handle_t mac_handle;
	struct wlan_objmgr_vdev *vdev;
	enum eSirMacReasonCodes reason = eSIR_MAC_IFACE_DOWN;

	hdd_enter();

	if (adapter->device_mode == QDF_STA_MODE &&
	    ucfg_pkt_capture_get_mode(hdd_ctx->psoc))
		hdd_del_monitor_interface(hdd_ctx);

	if (adapter->session_id != HDD_SESSION_ID_INVALID)
		wlan_hdd_cfg80211_deregister_frames(adapter);

	hdd_nud_ignore_tracking(adapter, true);
	hdd_nud_reset_tracking(adapter);
	hdd_nud_flush_work(adapter);
	hdd_stop_tsf_sync(adapter);

	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);
	/*
	 * if this is the last active connection check & stop the
	 * opportunistic timer first
	 */
	if (((policy_mgr_get_connection_count(hdd_ctx->psoc) == 1) &&
		(policy_mgr_mode_specific_connection_count(hdd_ctx->psoc,
			policy_mgr_convert_device_mode_to_qdf_type(
				adapter->device_mode), NULL) == 1)) ||
			!policy_mgr_get_connection_count(hdd_ctx->psoc))
		policy_mgr_check_and_stop_opportunistic_timer(
			hdd_ctx->psoc, adapter->session_id);

	mac_handle = hdd_ctx->mac_handle;

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_IBSS_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_NDI_MODE:
	case QDF_NAN_DISC_MODE:
		if ((QDF_NDI_MODE == adapter->device_mode) ||
			hdd_conn_is_connected(
				WLAN_HDD_GET_STATION_CTX_PTR(adapter)) ||
			hdd_is_connecting(
				WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
			INIT_COMPLETION(adapter->disconnect_comp_var);
			roam_profile = hdd_roam_profile(adapter);
			if (cds_is_driver_recovering())
				reason = eSIR_MAC_DEVICE_RECOVERY;

			/* For NDI do not use roam_profile */
			if (QDF_NDI_MODE == adapter->device_mode) {
				hdd_peer_cleanup(hdd_ctx, adapter);
				qdf_ret_status = sme_roam_disconnect(
					mac_handle,
					adapter->session_id,
					eCSR_DISCONNECT_REASON_NDI_DELETE,
					reason);
			} else if (roam_profile->BSSType ==
						eCSR_BSS_TYPE_START_IBSS)
				qdf_ret_status = sme_roam_disconnect(
					mac_handle,
					adapter->session_id,
					eCSR_DISCONNECT_REASON_IBSS_LEAVE,
					reason);
			else if (QDF_STA_MODE == adapter->device_mode) {
				rc = wlan_hdd_disconnect(
						adapter,
						eCSR_DISCONNECT_REASON_DEAUTH,
						reason);
				if (rc != 0 && ucfg_ipa_is_enabled()) {
					hdd_err("STA disconnect failed");
					ucfg_ipa_uc_cleanup_sta(hdd_ctx->pdev,
								adapter->dev);
				}
			} else {
				qdf_ret_status = sme_roam_disconnect(
					mac_handle,
					adapter->session_id,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					reason);
			}
			/* success implies disconnect command got
			 * queued up successfully
			 */
			if (qdf_ret_status == QDF_STATUS_SUCCESS &&
					QDF_STA_MODE != adapter->device_mode) {
				rc = wait_for_completion_timeout(
					&adapter->disconnect_comp_var,
					msecs_to_jiffies
						(WLAN_WAIT_TIME_DISCONNECT));
				if (!rc)
					hdd_warn("disconn_comp_var wait fail");
				if (adapter->device_mode == QDF_NDI_MODE)
					hdd_cleanup_ndi(hdd_ctx, adapter);
			}
			if (qdf_ret_status != QDF_STATUS_SUCCESS)
				hdd_warn("failed to post disconnect");
			memset(&wrqu, '\0', sizeof(wrqu));
			wrqu.ap_addr.sa_family = ARPHRD_ETHER;
			memset(wrqu.ap_addr.sa_data, '\0', ETH_ALEN);
			wireless_send_event(adapter->dev, SIOCGIWAP, &wrqu,
					    NULL);
		}
		wlan_hdd_scan_abort(adapter);

		wlan_hdd_cleanup_actionframe(adapter);
		wlan_hdd_cleanup_remain_on_channel_ctx(adapter);
		hdd_clear_fils_connection_info(adapter);
		qdf_ret_status = sme_roam_del_pmkid_from_cache(
							mac_handle,
							adapter->session_id,
							NULL, true);
		if (QDF_IS_STATUS_ERROR(qdf_ret_status))
			hdd_debug("Cannot flush PMKIDCache");

#ifdef WLAN_OPEN_SOURCE
		cancel_work_sync(&adapter->ipv4_notifier_work);
#endif

		hdd_deregister_tx_flow_control(adapter);

#ifdef WLAN_NS_OFFLOAD
#ifdef WLAN_OPEN_SOURCE
		cancel_work_sync(&adapter->ipv6_notifier_work);
#endif
#endif

		if (adapter->device_mode == QDF_STA_MODE) {
			struct wlan_objmgr_vdev *vdev;

			vdev = hdd_objmgr_get_vdev(adapter);
			if (vdev) {
				wlan_cfg80211_sched_scan_stop(vdev);
				hdd_objmgr_put_vdev(vdev);
			}
		}

		/*
		 * During vdev destroy, if any STA is in connecting state the
		 * roam command will be in active queue and thus vdev destroy is
		 * queued in pending queue. In case STA tries to connect to
		 * multiple BSSID and fails to connect, due to auth/assoc
		 * timeouts it may take more than vdev destroy time to get
		 * completed. On vdev destroy timeout vdev is moved to logically
		 * deleted state. Once connection is completed, vdev destroy is
		 * activated and to release the self-peer ref count it try to
		 * get the ref of the vdev, which fails as vdev is logically
		 * deleted and this leads to peer ref leak. So before vdev
		 * destroy is queued abort any STA ongoing connection to avoid
		 * vdev destroy timeout.
		 */
		if (test_bit(SME_SESSION_OPENED, &adapter->event_flags))
			hdd_abort_ongoing_sta_connection(hdd_ctx);

		hdd_vdev_destroy(adapter);
		break;

	case QDF_MONITOR_MODE:
		if (wlan_hdd_is_session_type_monitor(QDF_MONITOR_MODE) &&
		    adapter->vdev) {
			ucfg_pkt_capture_deregister_callbacks(adapter->vdev);
			hdd_objmgr_put_vdev(adapter->vdev);
			adapter->vdev = NULL;
		}
		wlan_hdd_scan_abort(adapter);
		hdd_deregister_tx_flow_control(adapter);
		sme_delete_mon_session(mac_handle, adapter->session_id);
		hdd_vdev_destroy(adapter);
		break;

	case QDF_SAP_MODE:
		if (test_bit(ACS_PENDING, &adapter->event_flags)) {
			cds_flush_delayed_work(&adapter->acs_pending_work);
			clear_bit(ACS_PENDING, &adapter->event_flags);
		}
		wlan_hdd_scan_abort(adapter);
		/* Diassociate with all the peers before stop ap post */
		if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags))
			wlan_hdd_del_station(adapter);

		qdf_ret_status =
			sme_roam_del_pmkid_from_cache(hdd_ctx->mac_handle,
						      adapter->session_id,
						      NULL, true);
		if (QDF_IS_STATUS_ERROR(qdf_ret_status))
			hdd_err("Cannot flush PMKSA Cache");

		/* Flush IPA exception path packets */
		sap_config = &adapter->session.ap.sap_config;
		if (sap_config)
			wlansap_reset_sap_config_add_ie(sap_config,
							eUPDATE_IE_ALL);
		ucfg_ipa_flush(hdd_ctx->pdev);
		if ((!(flag & HDD_IN_CAC_WORK_TH_CONTEXT)) &&
		    (hdd_ctx->sap_pre_cac_work.fn))
			cds_flush_work(&hdd_ctx->sap_pre_cac_work);
		/* fallthrough */

	case QDF_P2P_GO_MODE:
		cds_flush_work(&adapter->sap_stop_bss_work);

		/* Any softap specific cleanup here... */
		if (qdf_atomic_read(&adapter->session.ap.acs_in_progress)) {
			hdd_info("ACS in progress, wait for complete");
			qdf_wait_for_event_completion(
				&adapter->acs_complete_event,
				ACS_COMPLETE_TIMEOUT);
		}

		wlan_hdd_undo_acs(adapter);
		if (adapter->device_mode == QDF_P2P_GO_MODE)
			wlan_hdd_cleanup_remain_on_channel_ctx(adapter);

		hdd_deregister_tx_flow_control(adapter);

		hdd_destroy_acs_timer(adapter);
		/**
		 * During vdev destroy, If any STA is in connecting state the
		 * roam command will be in active queue and thus vdev destroy is
		 * queued in pending queue. In case STA is tries to connected to
		 * multiple BSSID and fails to connect, due to auth/assoc
		 * timeouts it may take more than vdev destroy time to get
		 * completes. If vdev destroy timeout vdev is moved to logically
		 * deleted state. Once connection is completed, vdev destroy is
		 * activated and to release the self-peer ref count it try to
		 * get the ref of the vdev, which fails as vdev is logically
		 * deleted and this leads to peer ref leak. So before vdev
		 * destroy is queued abort any STA ongoing connection to avoid
		 * vdev destroy timeout.
		 */
		if (test_bit(SME_SESSION_OPENED, &adapter->event_flags))
			hdd_abort_ongoing_sta_connection(hdd_ctx);

		mutex_lock(&hdd_ctx->sap_lock);
		if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			QDF_STATUS status;
			QDF_STATUS qdf_status;

			/* Set the stop_bss_in_progress flag */
			wlansap_set_stop_bss_inprogress(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter), true);

			/* Stop Bss. */
			status = wlansap_stop_bss(
					WLAN_HDD_GET_SAP_CTX_PTR(adapter));

			if (QDF_IS_STATUS_SUCCESS(status)) {
				struct hdd_hostapd_state *hostapd_state =
					WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
				qdf_event_reset(&hostapd_state->
						qdf_stop_bss_event);
				qdf_status =
					qdf_wait_for_event_completion(
					&hostapd_state->qdf_stop_bss_event,
					SME_CMD_START_STOP_BSS_TIMEOUT);

				if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
					hdd_err("failure waiting for wlansap_stop_bss %d",
						qdf_status);
				}
			} else {
				hdd_err("failure in wlansap_stop_bss");
			}
			clear_bit(SOFTAP_BSS_STARTED, &adapter->event_flags);
			wlansap_set_stop_bss_inprogress(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter), false);
			policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
						adapter->device_mode,
						adapter->session_id);
			hdd_green_ap_start_state_mc(hdd_ctx,
						    adapter->device_mode,
						    false);

			qdf_copy_macaddr(&updateIE.bssid,
					 &adapter->mac_addr);
			updateIE.smeSessionId = adapter->session_id;
			updateIE.ieBufferlength = 0;
			updateIE.pAdditionIEBuffer = NULL;
			updateIE.append = false;
			updateIE.notify = false;
			/* Probe bcn reset */
			if (sme_update_add_ie(mac_handle,
					      &updateIE, eUPDATE_IE_PROBE_BCN)
			    == QDF_STATUS_E_FAILURE) {
				hdd_err("Could not pass on PROBE_RSP_BCN data to PE");
			}
			/* Assoc resp reset */
			if (sme_update_add_ie(mac_handle,
					      &updateIE,
					      eUPDATE_IE_ASSOC_RESP) ==
			    QDF_STATUS_E_FAILURE) {
				hdd_err("Could not pass on ASSOC_RSP data to PE");
			}
			/* Reset WNI_CFG_PROBE_RSP Flags */
			wlan_hdd_reset_prob_rspies(adapter);
		}
		clear_bit(SOFTAP_INIT_DONE, &adapter->event_flags);
		qdf_mem_free(adapter->session.ap.beacon);
		adapter->session.ap.beacon = NULL;

		/*
		 * If Do_Not_Break_Stream was enabled clear avoid channel list.
		 */
		vdev = hdd_objmgr_get_vdev(adapter);
		if (vdev) {
			if (policy_mgr_is_dnsc_set(vdev))
				wlan_hdd_send_avoid_freq_for_dnbs(hdd_ctx, 0);
			hdd_objmgr_put_vdev(vdev);
		}

#ifdef WLAN_OPEN_SOURCE
		cancel_work_sync(&adapter->ipv4_notifier_work);
#endif

#ifdef WLAN_NS_OFFLOAD
#ifdef WLAN_OPEN_SOURCE
		cancel_work_sync(&adapter->ipv6_notifier_work);
#endif
#endif

		hdd_vdev_destroy(adapter);

		mutex_unlock(&hdd_ctx->sap_lock);
		break;
	case QDF_OCB_MODE:
		cdp_clear_peer(cds_get_context(QDF_MODULE_ID_SOC),
			(struct cdp_pdev *)cds_get_context(QDF_MODULE_ID_TXRX),
			WLAN_HDD_GET_STATION_CTX_PTR(adapter)->conn_info.staId[0]);
		hdd_deregister_tx_flow_control(adapter);
		hdd_vdev_destroy(adapter);
		break;
	default:
		break;
	}

	if (adapter->scan_info.default_scan_ies) {
		qdf_mem_free(adapter->scan_info.default_scan_ies);
		adapter->scan_info.default_scan_ies = NULL;
	}

	/* This function should be invoked at the end of this api*/
	hdd_dump_func_call_map();
	hdd_exit();
	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_deinit_all_adapters - deinit all adapters
 * @hdd_ctx:   HDD context
 * @rtnl_held: True if RTNL lock held
 *
 */
void  hdd_deinit_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held)
{
	struct hdd_adapter *adapter;

	hdd_enter();

	hdd_for_each_adapter(hdd_ctx, adapter)
		hdd_deinit_adapter(hdd_ctx, adapter, rtnl_held);

	hdd_exit();
}

QDF_STATUS hdd_stop_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;

	hdd_enter();

	if (hdd_ctx->sap_pre_cac_work.fn)
		cds_flush_work(&hdd_ctx->sap_pre_cac_work);

	hdd_for_each_adapter(hdd_ctx, adapter)
		hdd_stop_adapter(hdd_ctx, adapter);

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

static void hdd_reset_scan_operation(struct hdd_context *hdd_ctx,
				     struct hdd_adapter *adapter)
{
	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_IBSS_MODE:
	case QDF_P2P_DEVICE_MODE:
	case QDF_NDI_MODE:
		wlan_hdd_scan_abort(adapter);
		wlan_hdd_cleanup_remain_on_channel_ctx(adapter);
		if (adapter->device_mode == QDF_STA_MODE) {
			struct wlan_objmgr_vdev *vdev;

			vdev = hdd_objmgr_get_vdev(adapter);
			if (vdev) {
				wlan_cfg80211_sched_scan_stop(vdev);
				hdd_objmgr_put_vdev(vdev);
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

QDF_STATUS hdd_reset_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;
	struct qdf_mac_addr peerMacAddr;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	/* do not flush work if it is not created */
	if (hdd_ctx->sap_pre_cac_work.fn)
		cds_flush_work(&hdd_ctx->sap_pre_cac_work);

	hdd_for_each_adapter(hdd_ctx, adapter) {
		hdd_info("[SSR] reset adapter with device mode %s(%d)",
			 hdd_device_mode_to_string(adapter->device_mode),
			 adapter->device_mode);

		if ((adapter->device_mode == QDF_STA_MODE) ||
		    (adapter->device_mode == QDF_P2P_CLIENT_MODE)) {
			/* Stop tdls timers */
			vdev = hdd_objmgr_get_vdev(adapter);
			if (vdev) {
				hdd_notify_tdls_reset_adapter(vdev);
				hdd_objmgr_put_vdev(vdev);
			}
			adapter->session.station.hdd_reassoc_scenario = false;
		}

		if (hdd_ctx->config->sap_internal_restart &&
		    adapter->device_mode == QDF_SAP_MODE) {
			wlan_hdd_netif_queue_control(adapter,
						     WLAN_STOP_ALL_NETIF_QUEUE,
						     WLAN_CONTROL_PATH);
			if (test_bit(ACS_PENDING, &adapter->event_flags)) {
				cds_flush_delayed_work(
						&adapter->acs_pending_work);
				clear_bit(ACS_PENDING, &adapter->event_flags);
			}
		} else {
			wlan_hdd_netif_queue_control(adapter,
					   WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					   WLAN_CONTROL_PATH);
		}

		if ((adapter->device_mode == QDF_P2P_GO_MODE ||
		     adapter->device_mode == QDF_SAP_MODE) &&
		    test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			hdd_sap_indicate_disconnect_for_sta(adapter);
			clear_bit(SOFTAP_BSS_STARTED, &adapter->event_flags);
		}

		hdd_reset_scan_operation(hdd_ctx, adapter);

		hdd_deinit_tx_rx(adapter);
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
				adapter->device_mode, adapter->session_id);
		hdd_green_ap_start_state_mc(hdd_ctx, adapter->device_mode,
					    false);
		if (test_bit(WMM_INIT_DONE, &adapter->event_flags)) {
			hdd_wmm_adapter_close(adapter);
			clear_bit(WMM_INIT_DONE, &adapter->event_flags);
		}

		if (adapter->device_mode == QDF_STA_MODE)
			hdd_clear_fils_connection_info(adapter);

		if (adapter->device_mode == QDF_SAP_MODE) {
			wlansap_cleanup_cac_timer(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter));
			/*
			 * If adapter is SAP, set session ID to invalid
			 * since SAP session will be cleanup during SSR.
			 */
			wlansap_set_invalid_session(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter));
		}

		/* Delete connection peers if any to avoid peer object leaks */
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
			sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			qdf_copy_macaddr(&peerMacAddr,
					 &sta_ctx->conn_info.bssId);

		}

		hdd_nud_ignore_tracking(adapter, true);
		hdd_nud_reset_tracking(adapter);
		hdd_nud_flush_work(adapter);

		if (adapter->device_mode != QDF_SAP_MODE &&
		    adapter->device_mode != QDF_P2P_GO_MODE &&
		    adapter->device_mode != QDF_FTM_MODE)
			hdd_set_disconnect_status(adapter, false);

		hdd_stop_tsf_sync(adapter);

		hdd_softap_deinit_tx_rx(adapter);
		hdd_deregister_tx_flow_control(adapter);

		/* Destroy vdev which will be recreated during reinit. */
		hdd_vdev_destroy(adapter);
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

bool hdd_is_vdev_in_conn_state(struct hdd_adapter *adapter)
{
	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_DEVICE_MODE:
		return hdd_conn_is_connected(
				WLAN_HDD_GET_STATION_CTX_PTR(adapter));
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

bool hdd_is_any_interface_open(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;
	bool close_modules = true;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_info("FTM mode, don't close the module");
		return false;
	}

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags) ||
		    test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
			hdd_debug("Still other ifaces are up cannot close modules");
			close_modules = false;
			break;
		}
	}

	return close_modules;
}

bool hdd_is_interface_up(struct hdd_adapter *adapter)
{
	if (test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags))
		return true;
	else
		return false;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)) \
	&& !defined(WITH_BACKPORTS) && !defined(IEEE80211_PRIVACY)
struct cfg80211_bss *hdd_cfg80211_get_bss(struct wiphy *wiphy,
					  struct ieee80211_channel *channel,
					  const u8 *bssid, const u8 *ssid,
					  size_t ssid_len)
{
	return cfg80211_get_bss(wiphy, channel, bssid,
				ssid, ssid_len,
				WLAN_CAPABILITY_ESS,
				WLAN_CAPABILITY_ESS);
}
#else
struct cfg80211_bss *hdd_cfg80211_get_bss(struct wiphy *wiphy,
					  struct ieee80211_channel *channel,
					  const u8 *bssid, const u8 *ssid,
					  size_t ssid_len)
{
	return cfg80211_get_bss(wiphy, channel, bssid,
				ssid, ssid_len,
				IEEE80211_BSS_TYPE_ESS,
				IEEE80211_PRIVACY_ANY);
}
#endif

#if defined CFG80211_CONNECT_BSS || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0))
#if defined CFG80211_CONNECT_TIMEOUT_REASON_CODE || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
/**
 * hdd_convert_timeout_reason() - Convert to kernel specific enum
 * @timeout_reason: reason for connect timeout
 *
 * This function is used to convert host timeout
 * reason enum to kernel specific enum.
 *
 * Return: nl timeout enum
 */
static enum nl80211_timeout_reason hdd_convert_timeout_reason(
						tSirResultCodes timeout_reason)
{
	switch (timeout_reason) {
	case eSIR_SME_JOIN_TIMEOUT_RESULT_CODE:
		return NL80211_TIMEOUT_SCAN;
	case eSIR_SME_AUTH_TIMEOUT_RESULT_CODE:
		return NL80211_TIMEOUT_AUTH;
	case eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE:
		return NL80211_TIMEOUT_ASSOC;
	default:
		return NL80211_TIMEOUT_UNSPECIFIED;
	}
}

/**
 * hdd_cfg80211_connect_timeout() - API to send connection timeout reason
 * @dev: network device
 * @bssid: bssid to which we want to associate
 * @timeout_reason: reason for connect timeout
 *
 * This API is used to send connection timeout reason to supplicant
 *
 * Return: void
 */
static void hdd_cfg80211_connect_timeout(struct net_device *dev,
					 const u8 *bssid,
					 tSirResultCodes timeout_reason)
{
	enum nl80211_timeout_reason nl_timeout_reason;

	nl_timeout_reason = hdd_convert_timeout_reason(timeout_reason);

	cfg80211_connect_timeout(dev, bssid, NULL, 0, GFP_KERNEL,
				 nl_timeout_reason);
}

/**
 * __hdd_connect_bss() - API to send connection status to supplicant
 * @dev: network device
 * @bssid: bssid to which we want to associate
 * @req_ie: Request Information Element
 * @req_ie_len: len of the req IE
 * @resp_ie: Response IE
 * @resp_ie_len: len of ht response IE
 * @status: status
 * @gfp: Kernel Flag
 * @timeout_reason: reason for connect timeout
 *
 * Return: void
 */
static void __hdd_connect_bss(struct net_device *dev, const u8 *bssid,
			      struct cfg80211_bss *bss, const u8 *req_ie,
			      size_t req_ie_len, const u8 *resp_ie,
			      size_t resp_ie_len, int status, gfp_t gfp,
			      tSirResultCodes timeout_reason)
{
	enum nl80211_timeout_reason nl_timeout_reason;

	nl_timeout_reason = hdd_convert_timeout_reason(timeout_reason);

	cfg80211_connect_bss(dev, bssid, bss, req_ie, req_ie_len,
			     resp_ie, resp_ie_len, status, gfp,
			     nl_timeout_reason);
}
#else
#if defined CFG80211_CONNECT_TIMEOUT || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
static void hdd_cfg80211_connect_timeout(struct net_device *dev,
					 const u8 *bssid,
					 tSirResultCodes timeout_reason)
{
	cfg80211_connect_timeout(dev, bssid, NULL, 0, GFP_KERNEL);
}
#endif

static void __hdd_connect_bss(struct net_device *dev, const u8 *bssid,
			      struct cfg80211_bss *bss, const u8 *req_ie,
			      size_t req_ie_len, const u8 *resp_ie,
			      size_t resp_ie_len, int status, gfp_t gfp,
			      tSirResultCodes timeout_reason)
{
	cfg80211_connect_bss(dev, bssid, bss, req_ie, req_ie_len,
			     resp_ie, resp_ie_len, status, gfp);
}
#endif

/**
 * hdd_connect_bss() - API to send connection status to supplicant
 * @dev: network device
 * @bssid: bssid to which we want to associate
 * @req_ie: Request Information Element
 * @req_ie_len: len of the req IE
 * @resp_ie: Response IE
 * @resp_ie_len: len of ht response IE
 * @status: status
 * @gfp: Kernel Flag
 * @connect_timeout: If timed out waiting for Auth/Assoc/Probe resp
 * @timeout_reason: reason for connect timeout
 *
 * The API is a wrapper to send connection status to supplicant
 *
 * Return: Void
 */
#if defined CFG80211_CONNECT_TIMEOUT || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
static void hdd_connect_bss(struct net_device *dev, const u8 *bssid,
			struct cfg80211_bss *bss, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, int status, gfp_t gfp,
			bool connect_timeout,
			tSirResultCodes timeout_reason)
{
	if (connect_timeout)
		hdd_cfg80211_connect_timeout(dev, bssid, timeout_reason);
	else
		__hdd_connect_bss(dev, bssid, bss, req_ie, req_ie_len, resp_ie,
				  resp_ie_len, status, gfp, timeout_reason);
}
#else
static void hdd_connect_bss(struct net_device *dev, const u8 *bssid,
			struct cfg80211_bss *bss, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, int status, gfp_t gfp,
			bool connect_timeout,
			tSirResultCodes timeout_reason)
{
	__hdd_connect_bss(dev, bssid, bss, req_ie, req_ie_len, resp_ie,
			  resp_ie_len, status, gfp, timeout_reason);
}
#endif

#if defined(WLAN_FEATURE_FILS_SK)
#if (defined(CFG80211_CONNECT_DONE) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
#if defined(CFG80211_FILS_SK_OFFLOAD_SUPPORT) || \
		 (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
/**
 * hdd_populate_fils_params() - Populate FILS keys to connect response
 * @fils_params: connect response to supplicant
 * @fils_kek: FILS kek
 * @fils_kek_len: FILS kek length
 * @pmk: FILS PMK
 * @pmk_len: FILS PMK length
 * @pmkid: PMKID
 * @fils_seq_num: FILS Seq number
 *
 * Return: None
 */
static void hdd_populate_fils_params(struct cfg80211_connect_resp_params
				     *fils_params, const uint8_t *fils_kek,
				     size_t fils_kek_len, const uint8_t *pmk,
				     size_t pmk_len, const uint8_t *pmkid,
				     uint16_t fils_seq_num)
{
	/* Increament seq number to be used for next FILS */
	fils_params->fils_erp_next_seq_num = fils_seq_num + 1;
	fils_params->update_erp_next_seq_num = true;
	fils_params->fils_kek = fils_kek;
	fils_params->fils_kek_len = fils_kek_len;
	fils_params->pmk = pmk;
	fils_params->pmk_len = pmk_len;
	fils_params->pmkid = pmkid;
	hdd_debug("FILS erp_next_seq_num:%d",
		  fils_params->fils_erp_next_seq_num);
}
#else
static inline void hdd_populate_fils_params(struct cfg80211_connect_resp_params
					    *fils_params, const uint8_t
					    *fils_kek, size_t fils_kek_len,
					    const uint8_t *pmk, size_t pmk_len,
					    const uint8_t *pmkid,
					    uint16_t fils_seq_num)
{ }
#endif

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
/**
 * hdd_populate_fils_params() - Populate FILS keys to connect response
 * @fils_params: connect response to supplicant
 * @fils_kek: FILS kek
 * @fils_kek_len: FILS kek length
 * @pmk: FILS PMK
 * @pmk_len: FILS PMK length
 * @pmkid: PMKID
 * @fils_seq_num: FILS Seq number
 *
 * Return: None
 */
static void hdd_populate_fils_params(struct cfg80211_connect_resp_params
				     *fils_params, const uint8_t *fils_kek,
				     size_t fils_kek_len, const uint8_t *pmk,
				     size_t pmk_len, const uint8_t *pmkid,
				     uint16_t fils_seq_num)
{
	/* Increament seq number to be used for next FILS */
	fils_params->fils.erp_next_seq_num = fils_seq_num + 1;
	fils_params->fils.update_erp_next_seq_num = true;
	fils_params->fils.kek = fils_kek;
	fils_params->fils.kek_len = fils_kek_len;
	fils_params->fils.pmk = pmk;
	fils_params->fils.pmk_len = pmk_len;
	fils_params->fils.pmkid = pmkid;
	hdd_debug("FILS erp_next_seq_num:%d",
		  fils_params->fils.erp_next_seq_num);
}
#endif /* CFG80211_CONNECT_DONE */

#if defined(CFG80211_CONNECT_DONE) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))

void hdd_update_hlp_info(struct net_device *dev,
			 struct csr_roam_info *roam_info)
{
	struct sk_buff *skb;
	uint16_t skb_len;
	struct llc_snap_hdr_t *llc_hdr;
	QDF_STATUS status;
	uint8_t *hlp_data;
	uint16_t hlp_data_len;
	struct fils_join_rsp_params *roam_fils_params
				= roam_info->fils_join_rsp;
	struct hdd_adapter *padapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if (!roam_fils_params) {
		hdd_err("FILS Roam Param NULL");
		return;
	}

	if (!roam_fils_params->hlp_data_len) {
		hdd_err("FILS HLP Data NULL, len %d",
			roam_fils_params->hlp_data_len);
		return;
	}

	hlp_data = roam_fils_params->hlp_data;
	hlp_data_len = roam_fils_params->hlp_data_len;

	/* Calculate skb length */
	skb_len = (2 * ETH_ALEN) + hlp_data_len;
	skb = qdf_nbuf_alloc(NULL, skb_len, 0, 4, false);
	if (skb == NULL) {
		hdd_err("HLP packet nbuf alloc fails");
		return;
	}

	qdf_mem_copy(skb_put(skb, ETH_ALEN), roam_fils_params->dst_mac.bytes,
				 QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(skb_put(skb, ETH_ALEN), roam_fils_params->src_mac.bytes,
				 QDF_MAC_ADDR_SIZE);

	llc_hdr = (struct llc_snap_hdr_t *) hlp_data;
	if (IS_SNAP(llc_hdr)) {
		hlp_data += LLC_SNAP_HDR_OFFSET_ETHERTYPE;
		hlp_data_len += LLC_SNAP_HDR_OFFSET_ETHERTYPE;
	}

	qdf_mem_copy(skb_put(skb, hlp_data_len), hlp_data, hlp_data_len);

	/*
	 * This HLP packet is formed from HLP info encapsulated
	 * in assoc response frame which is AEAD encrypted.
	 * Hence, this checksum validation can be set unnecessary.
	 * i.e. network layer need not worry about checksum.
	 */
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	status = hdd_rx_packet_cbk(padapter, skb);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Sending HLP packet fails");
		return;
	}
	hdd_debug("send HLP packet to netif successfully");
}

/**
 * hdd_connect_done() - Wrapper API to call cfg80211_connect_done
 * @dev: network device
 * @bssid: bssid to which we want to associate
 * @bss: cfg80211 bss info
 * @roam_info: information about connected bss
 * @req_ie: Request Information Element
 * @req_ie_len: len of the req IE
 * @resp_ie: Response IE
 * @resp_ie_len: len of ht response IE
 * @status: status
 * @gfp: allocation flags
 * @connect_timeout: If timed out waiting for Auth/Assoc/Probe resp
 * @timeout_reason: reason for connect timeout
 *
 * This API is used as wrapper to send FILS key/sequence number
 * params etc. to supplicant in case of FILS connection
 *
 * Return: None
 */
static void hdd_connect_done(struct net_device *dev, const u8 *bssid,
			     struct cfg80211_bss *bss,
			     struct csr_roam_info *roam_info,
			     const u8 *req_ie, size_t req_ie_len,
			     const u8 *resp_ie, size_t resp_ie_len, u16 status,
			     gfp_t gfp, bool connect_timeout,
			     tSirResultCodes timeout_reason)
{
	struct cfg80211_connect_resp_params fils_params;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct fils_join_rsp_params *roam_fils_params =
				roam_info->fils_join_rsp;

	qdf_mem_zero(&fils_params, sizeof(fils_params));

	if (!roam_fils_params) {
		fils_params.status = WLAN_STATUS_UNSPECIFIED_FAILURE;
	} else {
		fils_params.status = status;
		fils_params.bssid = bssid;
		fils_params.timeout_reason =
				hdd_convert_timeout_reason(timeout_reason);
		fils_params.req_ie = req_ie;
		fils_params.req_ie_len = req_ie_len;
		fils_params.resp_ie = resp_ie;
		fils_params.resp_ie_len = resp_ie_len;
		fils_params.bss = bss;
		hdd_populate_fils_params(&fils_params, roam_fils_params->kek,
					 roam_fils_params->kek_len,
					 roam_fils_params->fils_pmk,
					 roam_fils_params->fils_pmk_len,
					 roam_fils_params->fils_pmkid,
					 roam_info->fils_seq_num);
		hdd_save_gtk_params(adapter, roam_info, false);
	}
	hdd_debug("FILS indicate connect status %d", fils_params.status);

	cfg80211_connect_done(dev, &fils_params, gfp);

	if (roam_fils_params && roam_fils_params->hlp_data_len)
		hdd_update_hlp_info(dev, roam_info);

	/* Clear all the FILS key info */
	if (roam_fils_params && roam_fils_params->fils_pmk)
		qdf_mem_free(roam_fils_params->fils_pmk);
	if (roam_fils_params)
		qdf_mem_free(roam_fils_params);
	roam_info->fils_join_rsp = NULL;
}
#else /* CFG80211_CONNECT_DONE */
static inline void
hdd_connect_done(struct net_device *dev, const u8 *bssid,
		 struct cfg80211_bss *bss, struct csr_roam_info *roam_info,
		 const u8 *req_ie, size_t req_ie_len,
		 const u8 *resp_ie, size_t resp_ie_len, u16 status,
		 gfp_t gfp, bool connect_timeout,
		 tSirResultCodes timeout_reason)
{ }
#endif
#endif /* WLAN_FEATURE_FILS_SK */

#if defined(WLAN_FEATURE_FILS_SK) && \
	(defined(CFG80211_CONNECT_DONE) || \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)))
/**
 * hdd_fils_update_connect_results() - API to send fils connection status to
 * supplicant.
 * @dev: network device
 * @bssid: bssid to which we want to associate
 * @bss: cfg80211 bss info
 * @roam_info: information about connected bss
 * @req_ie: Request Information Element
 * @req_ie_len: len of the req IE
 * @resp_ie: Response IE
 * @resp_ie_len: len of ht response IE
 * @status: status
 * @gfp: allocation flags
 * @connect_timeout: If timed out waiting for Auth/Assoc/Probe resp
 * @timeout_reason: reason for connect timeout
 *
 * The API is a wrapper to send connection status to supplicant
 *
 * Return: 0 if success else failure
 */
static int hdd_fils_update_connect_results(struct net_device *dev,
			const u8 *bssid,
			struct cfg80211_bss *bss,
			struct csr_roam_info *roam_info, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, u16 status, gfp_t gfp,
			bool connect_timeout,
			tSirResultCodes timeout_reason)
{
	hdd_enter();
	if (!roam_info || !roam_info->is_fils_connection)
		return -EINVAL;

	hdd_connect_done(dev, bssid, bss, roam_info, req_ie, req_ie_len,
			 resp_ie, resp_ie_len, status, gfp, connect_timeout,
			 timeout_reason);
	return 0;
}
#else
static inline int hdd_fils_update_connect_results(struct net_device *dev,
			const u8 *bssid,
			struct cfg80211_bss *bss,
			struct csr_roam_info *roam_info, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, u16 status, gfp_t gfp,
			bool connect_timeout,
			tSirResultCodes timeout_reason)
{
	return -EINVAL;
}
#endif

/**
 * hdd_connect_result() - API to send connection status to supplicant
 * @dev: network device
 * @bssid: bssid to which we want to associate
 * @roam_info: information about connected bss
 * @req_ie: Request Information Element
 * @req_ie_len: len of the req IE
 * @resp_ie: Response IE
 * @resp_ie_len: len of ht response IE
 * @status: status
 * @gfp: Kernel Flag
 * @connect_timeout: If timed out waiting for Auth/Assoc/Probe resp
 * @timeout_reason: reason for connect timeout
 *
 * The API is a wrapper to send connection status to supplicant
 * and allow runtime suspend
 *
 * Return: Void
 */
void hdd_connect_result(struct net_device *dev, const u8 *bssid,
			struct csr_roam_info *roam_info, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, u16 status, gfp_t gfp,
			bool connect_timeout,
			tSirResultCodes timeout_reason)
{
	struct hdd_adapter *padapter = (struct hdd_adapter *) netdev_priv(dev);
	struct cfg80211_bss *bss = NULL;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(padapter);

	if (WLAN_STATUS_SUCCESS == status) {
		struct ieee80211_channel *chan;
		int freq;
		int chan_no = roam_info->pBssDesc->channelId;

		if (chan_no <= 14)
			freq = ieee80211_channel_to_frequency(chan_no,
				HDD_NL80211_BAND_2GHZ);
		else
			freq = ieee80211_channel_to_frequency(chan_no,
				HDD_NL80211_BAND_5GHZ);

		chan = ieee80211_get_channel(padapter->wdev.wiphy, freq);
		bss = hdd_cfg80211_get_bss(padapter->wdev.wiphy, chan, bssid,
			roam_info->u.pConnectedProfile->SSID.ssId,
			roam_info->u.pConnectedProfile->SSID.length);
	}

	if (hdd_fils_update_connect_results(dev, bssid, bss,
			roam_info, req_ie, req_ie_len, resp_ie,
			resp_ie_len, status, gfp, connect_timeout,
			timeout_reason) != 0) {
		hdd_connect_bss(dev, bssid, bss, req_ie,
			req_ie_len, resp_ie, resp_ie_len,
			status, gfp, connect_timeout, timeout_reason);
	}

	qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.connect);
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_CONNECT);
}
#else
void hdd_connect_result(struct net_device *dev, const u8 *bssid,
			struct csr_roam_info *roam_info, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, u16 status, gfp_t gfp,
			bool connect_timeout,
			tSirResultCodes timeout_reason)
{
	struct hdd_adapter *padapter = (struct hdd_adapter *) netdev_priv(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(padapter);

	cfg80211_connect_result(dev, bssid, req_ie, req_ie_len,
				resp_ie, resp_ie_len, status, gfp);

	qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.connect);
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_CONNECT);
}
#endif

#ifdef FEATURE_MONITOR_MODE_SUPPORT
int wlan_hdd_set_mon_chan(struct hdd_adapter *adapter, uint32_t chan,
				 uint32_t bandwidth)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct hdd_mon_set_ch_info *ch_info = &sta_ctx->ch_info;
	QDF_STATUS status;
	struct qdf_mac_addr bssid;
	struct csr_roam_profile roam_profile;
	enum phy_ch_width ch_width;
	uint8_t max_fw_bw;

	if (QDF_GLOBAL_MONITOR_MODE != hdd_get_conparam()) {
		hdd_err("Not supported, device is not in monitor mode");
		return -EINVAL;
	}

	/* Verify the BW before accepting this request */
	ch_width = bandwidth;

	if (ch_width > CH_WIDTH_10MHZ ||
	   (!cds_is_sub_20_mhz_enabled() && ch_width > CH_WIDTH_160MHZ)) {
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

	/* Validate Channel */
	if (!WLAN_REG_IS_24GHZ_CH(chan) && !WLAN_REG_IS_5GHZ_CH(chan)) {
		hdd_err("Channel %d Not supported", chan);
		return -EINVAL;
	}

	if (WLAN_REG_IS_24GHZ_CH(chan)) {
		if (bandwidth == CH_WIDTH_80MHZ) {
			hdd_err("BW80 not possible in 2.4GHz band");
			return -EINVAL;
		}
		if ((bandwidth != CH_WIDTH_20MHZ) && (chan == 14) &&
				(bandwidth != CH_WIDTH_MAX)) {
			hdd_err("Only BW20 possible on channel 14");
			return -EINVAL;
		}
	}

	if (WLAN_REG_IS_5GHZ_CH(chan)) {
		if ((bandwidth != CH_WIDTH_20MHZ) && (chan == 165) &&
				(bandwidth != CH_WIDTH_MAX)) {
			hdd_err("Only BW20 possible on channel 165");
			return -EINVAL;
		}
	}

	hdd_debug("Set monitor mode Channel %d", chan);
	qdf_mem_zero(&roam_profile, sizeof(roam_profile));
	roam_profile.ChannelInfo.ChannelList = &ch_info->channel;
	roam_profile.ChannelInfo.numOfChannels = 1;
	roam_profile.phyMode = ch_info->phy_mode;
	roam_profile.ch_params.ch_width = bandwidth;
	hdd_select_cbmode(adapter, chan, &roam_profile.ch_params);

	qdf_mem_copy(bssid.bytes, adapter->mac_addr.bytes,
		     QDF_MAC_ADDR_SIZE);

	if (wlan_hdd_change_hw_mode_for_given_chnl(adapter, chan,
				POLICY_MGR_UPDATE_REASON_SET_OPER_CHAN)) {
		hdd_err("Failed to change hw mode");
		return -EINVAL;
	}

	status = sme_roam_channel_change_req(hdd_ctx->mac_handle,
					     bssid, &roam_profile.ch_params,
					     &roam_profile);
	if (status) {
		hdd_err("Status: %d Failed to set sme_roam Channel for monitor mode",
			status);
	}

	adapter->mon_chan = chan;
	adapter->mon_bandwidth = bandwidth;
	return qdf_status_to_os_return(status);
}
#endif

#ifdef MSM_PLATFORM
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
static inline void hdd_stop_p2p_go(struct hdd_adapter *adapter)
{
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
	struct hdd_adapter *adapter;
	eConnectionState connState;

	hdd_enter();

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (!hdd_is_interface_up(adapter) &&
		    adapter->device_mode != QDF_NDI_MODE)
			continue;

		hdd_debug("[SSR] start adapter with device mode %s(%d)",
			  hdd_device_mode_to_string(adapter->device_mode),
			  adapter->device_mode);

		hdd_wmm_init(adapter);

		switch (adapter->device_mode) {
		case QDF_STA_MODE:
		case QDF_P2P_CLIENT_MODE:
		case QDF_P2P_DEVICE_MODE:
		case QDF_NAN_DISC_MODE:

			connState = (WLAN_HDD_GET_STATION_CTX_PTR(adapter))
					->conn_info.connState;

			hdd_start_station_adapter(adapter);
			/* Open the gates for HDD to receive Wext commands */
			adapter->is_link_up_service_needed = false;

			/* Indicate disconnect event to supplicant
			 * if associated previously
			 */
			if (eConnectionState_Associated == connState ||
			    eConnectionState_IbssConnected == connState ||
			    eConnectionState_NotConnected == connState ||
			    eConnectionState_IbssDisconnected == connState ||
			    eConnectionState_Disconnecting == connState) {
				union iwreq_data wrqu;

				memset(&wrqu, '\0', sizeof(wrqu));
				wrqu.ap_addr.sa_family = ARPHRD_ETHER;
				memset(wrqu.ap_addr.sa_data, '\0', ETH_ALEN);
				wireless_send_event(adapter->dev, SIOCGIWAP,
						    &wrqu, NULL);
				adapter->session.station.
				hdd_reassoc_scenario = false;

				/* indicate disconnected event to nl80211 */
				wlan_hdd_cfg80211_indicate_disconnect(
						adapter, true,
						eSIR_MAC_DEVICE_RECOVERY,
						NULL, 0);
			} else if (eConnectionState_Connecting == connState) {
				/*
				 * Indicate connect failure to supplicant if we
				 * were in the process of connecting
				 */
				hdd_connect_result(adapter->dev, NULL, NULL,
						   NULL, 0, NULL, 0,
						   WLAN_STATUS_ASSOC_DENIED_UNSPEC,
						   GFP_KERNEL, false, 0);
			}

			if (cds_is_driver_recovering() &&
			    (adapter->device_mode == QDF_NAN_DISC_MODE ||
			     (adapter->device_mode == QDF_STA_MODE &&
			      wlan_hdd_nan_is_supported(hdd_ctx) &&
			      !(hdd_ctx->nan_seperate_vdev_supported &&
			      wlan_hdd_nan_separate_iface_supported(hdd_ctx)))))
				hdd_nan_disable_ind_to_userspace(hdd_ctx);

			hdd_register_tx_flow_control(adapter,
					hdd_tx_resume_timer_expired_handler,
					hdd_tx_resume_cb,
					hdd_tx_flow_control_is_pause);

			hdd_lpass_notify_start(hdd_ctx, adapter);
			hdd_nud_ignore_tracking(adapter, false);
			break;

		case QDF_SAP_MODE:
			if (hdd_ctx->config->sap_internal_restart)
				hdd_start_ap_adapter(adapter);

			break;

		case QDF_P2P_GO_MODE:
			hdd_delete_sta(adapter);
			break;
		case QDF_MONITOR_MODE:
			if (wlan_hdd_is_session_type_monitor(
			    QDF_MONITOR_MODE) &&
			    ucfg_pkt_capture_get_mode(hdd_ctx->psoc)) {
				ucfg_pkt_capture_register_callbacks(
						adapter->vdev,
						hdd_mon_rx_packet_cbk,
						adapter);
				break;
			}
			hdd_start_station_adapter(adapter);
			hdd_set_mon_rx_cb(adapter->dev);

			/*
			 * Do not set channel for monitor mode if monitor iface
			 * went down during SSR, as for set channels host sends
			 * vdev start command to FW. For the interfaces went
			 * down during SSR, host stops those adapters by sending
			 * vdev stop/down/delete commands to FW. So FW doesn't
			 * sends response for vdev start and vdev start response
			 * timer expires and thus host triggers ASSERT.
			 */
			if (!test_bit(DOWN_DURING_SSR, &adapter->event_flags))
				wlan_hdd_set_mon_chan(
						adapter, adapter->mon_chan,
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
		wlan_hdd_cfg80211_register_frames(adapter);
	}

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (!hdd_is_interface_up(adapter))
			continue;

		if (adapter->device_mode == QDF_P2P_GO_MODE)
			hdd_stop_p2p_go(adapter);
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
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
	if (unlikely(!hdd_ctx)) {
		hdd_err("HDD context is Null");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock);
	for (hdd_get_front_adapter_no_lock(hdd_ctx, &adapter); adapter;
	     hdd_get_next_adapter_no_lock(hdd_ctx, adapter, &adapter)) {
		cache[n_cache++] = adapter;
	}
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock);

	for (i = 0; i < n_cache - 1; i++) {
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
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if ((adapter->device_mode == QDF_STA_MODE ||
		     adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		     adapter->device_mode == QDF_P2P_DEVICE_MODE) &&
		    ucfg_p2p_check_random_mac(hdd_ctx->psoc,
					      adapter->session_id, mac_addr))
			return adapter;
	}

	return NULL;
}

struct hdd_adapter *hdd_get_adapter_by_macaddr(struct hdd_context *hdd_ctx,
					  tSirMacAddr macAddr)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (!qdf_mem_cmp(adapter->mac_addr.bytes,
				 macAddr, sizeof(tSirMacAddr)))
			return adapter;
	}

	return NULL;
}

struct hdd_adapter *hdd_get_adapter_by_vdev(struct hdd_context *hdd_ctx,
				       uint32_t vdev_id)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter->session_id == vdev_id)
			return adapter;
	}

	return NULL;
}

/**
 * hdd_get_adapter_by_sme_session_id() - Return adapter with
 * the sessionid
 * @hdd_ctx: hdd context.
 * @sme_session_id: sme session is for the adapter to get.
 *
 * This function is used to get the adapter with provided session id
 *
 * Return: adapter pointer if found
 *
 */
struct hdd_adapter *
hdd_get_adapter_by_sme_session_id(struct hdd_context *hdd_ctx,
				  uint32_t sme_session_id)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter->session_id == sme_session_id)
			return adapter;
	}

	return NULL;
}

struct hdd_adapter *hdd_adapter_get_by_reference(struct hdd_context *hdd_ctx,
						 struct hdd_adapter *reference)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter == reference) {
			dev_hold(adapter->dev);
			break;
		}
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
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (!qdf_str_cmp(adapter->dev->name, iface_name))
			return adapter;
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
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter->device_mode == mode)
			return adapter;
	}

	return NULL;
}

enum QDF_OPMODE hdd_get_device_mode(uint32_t session_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return QDF_MAX_NO_OF_MODE;
	}

	adapter = hdd_get_adapter_by_sme_session_id(hdd_ctx, session_id);
	if (!adapter) {
		hdd_err("Invalid HDD adapter");
		return QDF_MAX_NO_OF_MODE;
	}

	return adapter->device_mode;
}

/**
 * hdd_get_operating_channel() - return operating channel of the device mode
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
 * Return: channel number. "0" id the requested device is not found OR it is
 *	   not connected.
 */
uint8_t hdd_get_operating_channel(struct hdd_context *hdd_ctx,
			enum QDF_OPMODE mode)
{
	struct hdd_adapter *adapter;
	uint8_t operatingChannel = 0;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (mode == adapter->device_mode) {
			switch (adapter->device_mode) {
			case QDF_STA_MODE:
			case QDF_P2P_CLIENT_MODE:
				if (hdd_conn_is_connected
					    (WLAN_HDD_GET_STATION_CTX_PTR
						(adapter))) {
					operatingChannel =
						(WLAN_HDD_GET_STATION_CTX_PTR
						(adapter))->conn_info.
							operationChannel;
				}
				break;
			case QDF_SAP_MODE:
			case QDF_P2P_GO_MODE:
				/* softap connection info */
				if (test_bit
					    (SOFTAP_BSS_STARTED,
					    &adapter->event_flags))
					operatingChannel =
						(WLAN_HDD_GET_AP_CTX_PTR
						(adapter))->operating_channel;
				break;
			default:
				break;
			}

			/* Found the device of interest. break the loop */
			break;
		}
	}

	return operatingChannel;
}

static inline QDF_STATUS hdd_unregister_wext_all_adapters(struct hdd_context *
							  hdd_ctx)
{
	struct hdd_adapter *adapter;

	hdd_enter();

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		    adapter->device_mode == QDF_IBSS_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE ||
		    adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			hdd_unregister_wext(adapter->dev);
		}
	}

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_abort_mac_scan_all_adapters(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;

	hdd_enter();

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		    adapter->device_mode == QDF_IBSS_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE ||
		    adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			wlan_abort_scan(hdd_ctx->pdev, INVAL_PDEV_ID,
					adapter->session_id, INVALID_SCAN_ID,
					true);
		}
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
	struct hdd_adapter *adapter;
	int err;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		    adapter->device_mode == QDF_IBSS_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE ||
		    adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			err = wlan_hdd_sched_scan_stop(adapter->dev);
			if (err)
				hdd_err("Unable to stop scheduled scan");
		}
	}

	return QDF_STATUS_SUCCESS;
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

void hdd_set_disconnect_status(struct hdd_adapter *adapter, bool status)
{
	qdf_mutex_acquire(&adapter->disconnection_status_lock);
	adapter->disconnection_in_progress = status;
	qdf_mutex_release(&adapter->disconnection_status_lock);
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

	ret = hdd_nud_register_netevent_notifier(hdd_ctx);
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
	hdd_nud_unregister_netevent_notifier(hdd_ctx);
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

	spectral_scan_activate_service();

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

/**
 * hdd_rx_wake_lock_destroy() - Destroy RX wakelock
 * @hdd_ctx:	HDD context.
 *
 * Destroy RX wakelock.
 *
 * Return: None.
 */
static void hdd_rx_wake_lock_destroy(struct hdd_context *hdd_ctx)
{
	qdf_wake_lock_destroy(&hdd_ctx->rx_wake_lock);
}

/**
 * hdd_rx_wake_lock_create() - Create RX wakelock
 * @hdd_ctx:	HDD context.
 *
 * Create RX wakelock.
 *
 * Return: None.
 */
static void hdd_rx_wake_lock_create(struct hdd_context *hdd_ctx)
{
	qdf_wake_lock_create(&hdd_ctx->rx_wake_lock, "qcom_rx_wakelock");
}

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

	hdd_sap_context_destroy(hdd_ctx);

	hdd_rx_wake_lock_destroy(hdd_ctx);

	hdd_scan_context_destroy(hdd_ctx);

	qdf_list_destroy(&hdd_ctx->hdd_adapters);

	return 0;
}

/**
 * hdd_context_destroy() - Destroy HDD context
 * @hdd_ctx:	HDD context to be destroyed.
 *
 * Free config and HDD context as well as destroy all the resources.
 *
 * Return: None
 */
void hdd_context_destroy(struct hdd_context *hdd_ctx)
{
	wlan_hdd_sar_timers_deinit(hdd_ctx);

	cds_set_context(QDF_MODULE_ID_HDD, NULL);

	hdd_context_deinit(hdd_ctx);

	qdf_mem_free(hdd_ctx->config);
	hdd_ctx->config = NULL;

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

/**
 * hdd_wlan_exit() - HDD WLAN exit function
 * @hdd_ctx:	Pointer to the HDD Context
 *
 * This is the driver exit point (invoked during rmmod)
 *
 * Return: None
 */
static void hdd_wlan_exit(struct hdd_context *hdd_ctx)
{
	struct wiphy *wiphy = hdd_ctx->wiphy;
	int driver_status;

	hdd_enter();

	hdd_debugfs_mws_coex_info_deinit(hdd_ctx);
	hdd_psoc_idle_timer_stop(hdd_ctx);

	hdd_unregister_notifiers(hdd_ctx);

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&hdd_ctx->skip_acs_scan_timer)) {
		qdf_mc_timer_stop(&hdd_ctx->skip_acs_scan_timer);
	}

	if (!QDF_IS_STATUS_SUCCESS
		    (qdf_mc_timer_destroy(&hdd_ctx->skip_acs_scan_timer))) {
		hdd_err("Cannot deallocate ACS Skip timer");
	}
	qdf_spin_lock(&hdd_ctx->acs_skip_lock);
	qdf_mem_free(hdd_ctx->last_acs_channel_list);
	hdd_ctx->last_acs_channel_list = NULL;
	hdd_ctx->num_of_channels = 0;
	qdf_spin_unlock(&hdd_ctx->acs_skip_lock);
#endif

	mutex_lock(&hdd_ctx->iface_change_lock);
	driver_status = hdd_ctx->driver_status;
	mutex_unlock(&hdd_ctx->iface_change_lock);

	/*
	 * Powersave Offload Case
	 * Disable Idle Power Save Mode
	 */
	hdd_set_idle_ps_config(hdd_ctx, false);
	/* clear the scan queue in all the scenarios */
	wlan_cfg80211_cleanup_scan_queue(hdd_ctx->pdev, NULL);

	if (driver_status != DRIVER_MODULES_CLOSED) {
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

	hdd_wlan_stop_modules(hdd_ctx, false);

	hdd_bus_bandwidth_deinit(hdd_ctx);
	hdd_driver_memdump_deinit();

	qdf_nbuf_deinit_replenish_timer();

	if (QDF_GLOBAL_MONITOR_MODE ==  hdd_get_conparam()) {
		hdd_info("Release wakelock for monitor mode!");
		qdf_wake_lock_release(&hdd_ctx->monitor_mode_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_MONITOR_MODE);
	}

	qdf_spinlock_destroy(&hdd_ctx->hdd_adapter_lock);
	qdf_spinlock_destroy(&hdd_ctx->sta_update_info_lock);
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

	hdd_exit_netlink_services(hdd_ctx);
	mutex_destroy(&hdd_ctx->iface_change_lock);
#ifdef FEATURE_WLAN_CH_AVOID
	mutex_destroy(&hdd_ctx->avoid_freq_lock);
#endif

	driver_status = hdd_objmgr_release_and_destroy_psoc(hdd_ctx);
	if (driver_status)
		hdd_err("Psoc delete failed");

	/* This function should be invoked at the end of this api*/
	hdd_dump_func_call_map();
}

void __hdd_wlan_exit(void)
{
	struct hdd_context *hdd_ctx;

	hdd_enter();

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD Context");
		hdd_exit();
		return;
	}

	/* Do all the cleanup before deregistering the driver */
	hdd_wlan_exit(hdd_ctx);

	hdd_exit();
}

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
	struct hdd_context *hdd_ctx = (struct hdd_context *) data;
	mac_handle_t mac_handle;

	hdd_debug("ACS Scan result expired. Reset ACS scan skip");
	hdd_ctx->skip_acs_scan_status = eSAP_DO_NEW_ACS_SCAN;
	qdf_spin_lock(&hdd_ctx->acs_skip_lock);
	qdf_mem_free(hdd_ctx->last_acs_channel_list);
	hdd_ctx->last_acs_channel_list = NULL;
	hdd_ctx->num_of_channels = 0;
	qdf_spin_unlock(&hdd_ctx->acs_skip_lock);

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle)
		return;
	sme_scan_flush_result(mac_handle);
}
#endif

#ifdef QCA_HT_2040_COEX
int hdd_wlan_set_ht2040_mode(struct hdd_adapter *adapter, uint16_t sta_id,
			     struct qdf_mac_addr sta_mac, int channel_type)
{
	int status;
	QDF_STATUS qdf_status;
	struct hdd_context *hdd_ctx;
	mac_handle_t mac_handle;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle)
		return -EINVAL;

	qdf_status = sme_notify_ht2040_mode(mac_handle, sta_id, sta_mac,
					    adapter->session_id, channel_type);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		hdd_err("Fail to send notification with ht2040 mode");
		return -EINVAL;
	}

	return 0;
}
#endif

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
 *
 * hdd_post_cds_enable_config() - HDD post cds start config helper
 * @adapter - Pointer to the HDD
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
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter && adapter->magic == WLAN_HDD_ADAPTER_MAGIC)
			return adapter;
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

static int hdd_wiphy_init(struct hdd_context *hdd_ctx)
{
	struct wiphy *wiphy;
	int ret_val;

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

#ifdef MSM_PLATFORM
/**
 * hdd_display_periodic_stats() - Function to display periodic stats
 * @hdd_ctx - handle to hdd context
 * @bool data_in_interval - true, if data detected in bw time interval
 *
 * The periodicity is determined by hdd_ctx->config->periodic_stats_disp_time.
 * Stats show up in wlan driver logs.
 *
 * Returns: None
 */
static inline
void hdd_display_periodic_stats(struct hdd_context *hdd_ctx,
				bool data_in_interval)
{
	static u32 counter;
	static bool data_in_time_period;
	ol_txrx_pdev_handle pdev;

	if (hdd_ctx->config->periodic_stats_disp_time == 0)
		return;

	pdev = cds_get_context(QDF_MODULE_ID_TXRX);
	if (!pdev) {
		hdd_err("pdev is NULL");
		return;
	}

	counter++;
	if (data_in_interval)
		data_in_time_period = data_in_interval;

	if (counter * hdd_ctx->config->busBandwidthComputeInterval >=
		hdd_ctx->config->periodic_stats_disp_time * 1000) {
		if (data_in_time_period) {
			wlan_hdd_display_txrx_stats(hdd_ctx);
			cdp_display_stats(cds_get_context(QDF_MODULE_ID_SOC),
					  CDP_TXRX_PATH_STATS,
					  QDF_STATS_VERBOSITY_LEVEL_LOW);
			wlan_hdd_display_netif_queue_history
				(hdd_ctx, QDF_STATS_VERBOSITY_LEVEL_LOW);
			qdf_dp_trace_dump_stats();
		}
		counter = 0;
		data_in_time_period = false;
	}
}

/**
 * hdd_clear_rps_cpu_mask - clear RPS CPU mask for interfaces
 * @hdd_ctx: pointer to struct hdd_context
 *
 * Return: none
 */
static void hdd_clear_rps_cpu_mask(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter)
		hdd_send_rps_disable_ind(adapter);
}

#ifdef CLD_PM_QOS
#define PLD_REMOVE_PM_QOS(x)
#define PLD_REQUEST_PM_QOS(x, y)
/**
 * hdd_pm_qos_update_cpu_mask() - Prepare CPU mask for PM_qos voting
 * @mask: return variable of cpumask for the TPUT
 * @high_throughput: only update high cores mask for high TPUT
 *
 * Return: none
 */
static inline void hdd_pm_qos_update_cpu_mask(cpumask_t *mask,
					      bool high_throughput)
{
	cpumask_set_cpu(0, mask);
	cpumask_set_cpu(1, mask);
	cpumask_set_cpu(2, mask);
	cpumask_set_cpu(3, mask);

	if (high_throughput) {
		/* For high TPUT include GOLD mask also */
		cpumask_set_cpu(4, mask);
		cpumask_set_cpu(5, mask);
		cpumask_set_cpu(6, mask);
	}
}

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
	cpumask_copy(&hdd_ctx->pm_qos_req.cpus_affine, pm_qos_cpu_mask);

	/* Latency value to be read from INI */
	if (cpumask_empty(pm_qos_cpu_mask))
		pm_qos_update_request(&hdd_ctx->pm_qos_req,
				      PM_QOS_DEFAULT_VALUE);
	else
		pm_qos_update_request(&hdd_ctx->pm_qos_req, 1);
}

#ifdef CONFIG_SMP
/**
 * hdd_update_pm_qos_affine_cores() - Update PM_qos request for AFFINE_CORES
 * @hdd_ctx: handle to hdd context
 *
 * Return: none
 */
static inline void hdd_update_pm_qos_affine_cores(struct hdd_context *hdd_ctx)
{
	hdd_ctx->pm_qos_req.type = PM_QOS_REQ_AFFINE_CORES;
	qdf_cpumask_clear(&hdd_ctx->pm_qos_req.cpus_affine);
	hdd_pm_qos_update_cpu_mask(&hdd_ctx->pm_qos_req.cpus_affine, false);
}
#else
static inline void hdd_update_pm_qos_affine_cores(struct hdd_context *hdd_ctx)
{
}
#endif
static inline void hdd_pm_qos_add_request(struct hdd_context *hdd_ctx)
{
	hdd_update_pm_qos_affine_cores(hdd_ctx);
	pm_qos_add_request(&hdd_ctx->pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
	hdd_info("Set cpu_mask %*pb for affine_cores",
		 cpumask_pr_args(&hdd_ctx->pm_qos_req.cpus_affine));
}

static inline void hdd_pm_qos_remove_request(struct hdd_context *hdd_ctx)
{
	pm_qos_remove_request(&hdd_ctx->pm_qos_req);
}
#else
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
#endif

/**
 * hdd_pld_request_bus_bandwidth() - Function to control bus bandwidth
 * @hdd_ctx - handle to hdd context
 * @tx_packets - transmit packet count
 * @rx_packets - receive packet count
 *
 * The function controls the bus bandwidth and dynamic control of
 * tcp delayed ack configuration
 *
 * Returns: None
 */

static void hdd_pld_request_bus_bandwidth(struct hdd_context *hdd_ctx,
					  const uint64_t tx_packets,
					  const uint64_t rx_packets)
{
	u64 total_pkts = tx_packets + rx_packets;
	uint64_t temp_tx = 0, avg_rx = 0;
	uint64_t no_rx_offload_pkts = 0, avg_no_rx_offload_pkts = 0;
	uint64_t rx_offload_pkts = 0, avg_rx_offload_pkts = 0;
	enum pld_bus_width_type next_vote_level = PLD_BUS_WIDTH_NONE;
	static enum wlan_tp_level next_rx_level = WLAN_SVC_TP_NONE;
	enum wlan_tp_level next_tx_level = WLAN_SVC_TP_NONE;
	uint32_t delack_timer_cnt = hdd_ctx->config->tcp_delack_timer_count;
	uint16_t index = 0;
	bool vote_level_change = false;
	bool rx_level_change = false;
	bool tx_level_change = false;
	bool rxthread_high_tput_req = false;
	bool dptrace_high_tput_req;
	cpumask_t pm_qos_cpu_mask;
	bool enable_pm_qos_high = false;

	cpumask_clear(&pm_qos_cpu_mask);
	if (total_pkts > hdd_ctx->config->busBandwidthHighThreshold)
		next_vote_level = PLD_BUS_WIDTH_HIGH;
	else if (total_pkts > hdd_ctx->config->busBandwidthMediumThreshold)
		next_vote_level = PLD_BUS_WIDTH_MEDIUM;
	else if (total_pkts > hdd_ctx->config->busBandwidthLowThreshold)
		next_vote_level = PLD_BUS_WIDTH_LOW;
	else
		next_vote_level = PLD_BUS_WIDTH_NONE;

	dptrace_high_tput_req =
			next_vote_level > PLD_BUS_WIDTH_NONE ? true : false;

	if (hdd_ctx->cur_vote_level != next_vote_level) {
		hdd_debug("trigger level %d, tx_packets: %lld, rx_packets: %lld",
			 next_vote_level, tx_packets, rx_packets);
		hdd_ctx->cur_vote_level = next_vote_level;
		vote_level_change = true;
		pld_request_bus_bandwidth(hdd_ctx->parent_dev, next_vote_level);
		if ((next_vote_level == PLD_BUS_WIDTH_LOW) ||
		    (next_vote_level == PLD_BUS_WIDTH_NONE)) {
			if (hdd_ctx->hbw_requested && !hdd_ctx->llm_enabled) {
				PLD_REMOVE_PM_QOS(hdd_ctx->parent_dev);
				hdd_ctx->hbw_requested = false;
			}
			if (hdd_ctx->dynamic_rps)
				hdd_clear_rps_cpu_mask(hdd_ctx);
		} else {
			if (!hdd_ctx->hbw_requested) {
				PLD_REQUEST_PM_QOS(hdd_ctx->parent_dev, 1);
				hdd_ctx->hbw_requested = true;
			}
			if (hdd_ctx->dynamic_rps)
				hdd_set_rps_cpu_mask(hdd_ctx);
		}

		if (hdd_ctx->config->rx_thread_ul_affinity_mask) {
			if (next_vote_level == PLD_BUS_WIDTH_HIGH &&
			    (tx_packets >
			    hdd_ctx->config->busBandwidthHighThreshold) &&
			    (rx_packets >
			    hdd_ctx->config->busBandwidthLowThreshold))
				cds_sched_handle_rx_thread_affinity_req(true);
			else if (next_vote_level != PLD_BUS_WIDTH_HIGH)
				cds_sched_handle_rx_thread_affinity_req(false);
		}

		if (hdd_ctx->config->napi_cpu_affinity_mask)
			hdd_napi_apply_throughput_policy(hdd_ctx,
							 tx_packets,
							 rx_packets);

		if (rx_packets < hdd_ctx->config->busBandwidthLowThreshold)
			hdd_disable_rx_ol_for_low_tput(hdd_ctx, true);
		else
			hdd_disable_rx_ol_for_low_tput(hdd_ctx, false);
	}

#ifdef CONFIG_DP_TRACE
	qdf_dp_trace_apply_tput_policy(dptrace_high_tput_req);
#endif

	/*
	 * Includes tcp+udp, if perf core is required for tcp, then
	 * perf core is also required for udp.
	 */
	no_rx_offload_pkts = hdd_ctx->no_rx_offload_pkt_cnt;
	hdd_ctx->no_rx_offload_pkt_cnt = 0;
	rx_offload_pkts = rx_packets - no_rx_offload_pkts;

	avg_no_rx_offload_pkts = (no_rx_offload_pkts +
				  hdd_ctx->prev_no_rx_offload_pkts) / 2;
	hdd_ctx->prev_no_rx_offload_pkts = no_rx_offload_pkts;

	avg_rx_offload_pkts = (rx_offload_pkts +
			       hdd_ctx->prev_rx_offload_pkts) / 2;
	hdd_ctx->prev_rx_offload_pkts = rx_offload_pkts;

	avg_rx = avg_no_rx_offload_pkts + avg_rx_offload_pkts;
	/*
	 * Takes care to set Rx_thread affinity for below case
	 * 1)LRO/GRO not supported ROME case
	 * 2)when rx_ol is disabled in cases like concurrency etc
	 * 3)For UDP cases
	 */
	if (avg_no_rx_offload_pkts >
			hdd_ctx->config->busBandwidthHighThreshold) {
		rxthread_high_tput_req = true;
		enable_pm_qos_high = true;
	} else {
		rxthread_high_tput_req = false;
		enable_pm_qos_high = false;
	}

	hdd_pm_qos_update_cpu_mask(&pm_qos_cpu_mask, enable_pm_qos_high);

	if (cds_sched_handle_throughput_req(rxthread_high_tput_req))
		hdd_warn("Rx thread high_tput(%d) affinity request failed",
			 rxthread_high_tput_req);

	/* fine-tuning parameters for RX Flows */
	if (avg_rx > hdd_ctx->config->tcpDelackThresholdHigh) {
		if ((hdd_ctx->cur_rx_level != WLAN_SVC_TP_HIGH) &&
		   (++hdd_ctx->rx_high_ind_cnt == delack_timer_cnt)) {
			next_rx_level = WLAN_SVC_TP_HIGH;
		}
	} else {
		hdd_ctx->rx_high_ind_cnt = 0;
		next_rx_level = WLAN_SVC_TP_LOW;
	}

	if (hdd_ctx->cur_rx_level != next_rx_level) {
		struct wlan_rx_tp_data rx_tp_data = {0};

		hdd_debug("TCP DELACK trigger level %d, average_rx: %llu",
			  next_rx_level, avg_rx);
		hdd_ctx->cur_rx_level = next_rx_level;
		rx_level_change = true;
		/* Send throughput indication only if it is enabled.
		 * Disabling tcp_del_ack will revert the tcp stack behavior
		 * to default delayed ack. Note that this will disable the
		 * dynamic delayed ack mechanism across the system
		 */
		if (hdd_ctx->en_tcp_delack_no_lro)
			rx_tp_data.rx_tp_flags |= TCP_DEL_ACK_IND;

		if (hdd_ctx->config->enable_tcp_adv_win_scale)
			rx_tp_data.rx_tp_flags |= TCP_ADV_WIN_SCL;

		rx_tp_data.level = next_rx_level;
		wlan_hdd_update_tcp_rx_param(hdd_ctx, &rx_tp_data);
	}

	/* fine-tuning parameters for TX Flows */
	temp_tx = (tx_packets + hdd_ctx->prev_tx) / 2;
	hdd_ctx->prev_tx = tx_packets;

	if (temp_tx > hdd_ctx->config->busBandwidthHighThreshold)
		enable_pm_qos_high = true;
	else
		enable_pm_qos_high = false;

	hdd_pm_qos_update_cpu_mask(&pm_qos_cpu_mask, enable_pm_qos_high);

	if (temp_tx > hdd_ctx->config->tcp_tx_high_tput_thres)
		next_tx_level = WLAN_SVC_TP_HIGH;
	else
		next_tx_level = WLAN_SVC_TP_LOW;

	if ((hdd_ctx->config->enable_tcp_limit_output) &&
	    (hdd_ctx->cur_tx_level != next_tx_level)) {
		struct wlan_tx_tp_data tx_tp_data = {0};

		hdd_debug("change TCP TX trigger level %d, average_tx: %llu",
				next_tx_level, temp_tx);
		hdd_ctx->cur_tx_level = next_tx_level;
		tx_level_change = true;
		tx_tp_data.level = next_tx_level;
		tx_tp_data.tcp_limit_output = true;
		wlan_hdd_update_tcp_tx_param(hdd_ctx, &tx_tp_data);
	}

	index = hdd_ctx->hdd_txrx_hist_idx;
	if (vote_level_change || tx_level_change || rx_level_change) {
		hdd_ctx->hdd_txrx_hist[index].next_tx_level = next_tx_level;
		hdd_ctx->hdd_txrx_hist[index].next_rx_level = next_rx_level;
		hdd_ctx->hdd_txrx_hist[index].next_vote_level = next_vote_level;
		hdd_ctx->hdd_txrx_hist[index].interval_rx = rx_packets;
		hdd_ctx->hdd_txrx_hist[index].interval_tx = tx_packets;
		hdd_ctx->hdd_txrx_hist[index].qtime = qdf_get_log_timestamp();
		hdd_ctx->hdd_txrx_hist_idx++;
		hdd_ctx->hdd_txrx_hist_idx &= NUM_TX_RX_HISTOGRAM_MASK;

		/* Clear all the mask if no silver/gold vote is required */
		if (next_vote_level < PLD_BUS_WIDTH_MEDIUM)
			cpumask_clear(&pm_qos_cpu_mask);

		hdd_pm_qos_update_request(hdd_ctx, &pm_qos_cpu_mask);
	}

	hdd_display_periodic_stats(hdd_ctx, (total_pkts > 0) ? true : false);

	hdd_periodic_sta_stats_display(hdd_ctx);
}

#define HDD_BW_GET_DIFF(_x, _y) (unsigned long)((ULONG_MAX - (_y)) + (_x) + 1)
static void __hdd_bus_bw_work_handler(struct work_struct *work)
{
	struct hdd_context *hdd_ctx = container_of(work, struct hdd_context,
					bus_bw_work);
	struct hdd_adapter *adapter = NULL, *con_sap_adapter = NULL;
	uint64_t tx_packets = 0, rx_packets = 0;
	uint64_t fwd_tx_packets = 0, fwd_rx_packets = 0;
	uint64_t fwd_tx_packets_diff = 0, fwd_rx_packets_diff = 0;
	uint64_t total_tx = 0, total_rx = 0;
	A_STATUS ret;
	bool connected = false;
	uint32_t ipa_tx_packets = 0, ipa_rx_packets = 0;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (hdd_ctx->is_wiphy_suspended)
		goto restart_timer;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		/*
		 * Validate magic so we don't end up accessing
		 * an invalid adapter.
		 */
		if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC)
			continue;

		if ((adapter->device_mode == QDF_STA_MODE ||
		     adapter->device_mode == QDF_P2P_CLIENT_MODE) &&
		    WLAN_HDD_GET_STATION_CTX_PTR(adapter)->conn_info.connState
		    != eConnectionState_Associated) {

			continue;
		}

		if ((adapter->device_mode == QDF_SAP_MODE ||
		     adapter->device_mode == QDF_P2P_GO_MODE) &&
		    WLAN_HDD_GET_AP_CTX_PTR(adapter)->ap_active == false) {

			continue;
		}

		tx_packets += HDD_BW_GET_DIFF(adapter->stats.tx_packets,
					      adapter->prev_tx_packets);
		rx_packets += HDD_BW_GET_DIFF(adapter->stats.rx_packets,
					      adapter->prev_rx_packets);

		if (adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE ||
		    adapter->device_mode == QDF_IBSS_MODE ||
		    adapter->device_mode == QDF_NDI_MODE) {

			ret = cdp_get_intra_bss_fwd_pkts_count(
				cds_get_context(QDF_MODULE_ID_SOC),
				adapter->session_id,
				&fwd_tx_packets, &fwd_rx_packets);
			if (ret == A_OK) {
				fwd_tx_packets_diff += HDD_BW_GET_DIFF(
					fwd_tx_packets,
					adapter->prev_fwd_tx_packets);
				fwd_rx_packets_diff += HDD_BW_GET_DIFF(
					fwd_tx_packets,
					adapter->prev_fwd_rx_packets);
			}
		}

		if (adapter->device_mode == QDF_SAP_MODE)
			con_sap_adapter = adapter;

		total_rx += adapter->stats.rx_packets;
		total_tx += adapter->stats.tx_packets;

		qdf_spin_lock_bh(&hdd_ctx->bus_bw_lock);
		adapter->prev_tx_packets = adapter->stats.tx_packets;
		adapter->prev_rx_packets = adapter->stats.rx_packets;
		adapter->prev_fwd_tx_packets = fwd_tx_packets;
		adapter->prev_fwd_rx_packets = fwd_rx_packets;
		qdf_spin_unlock_bh(&hdd_ctx->bus_bw_lock);
		connected = true;
	}

	if (!connected) {
		hdd_err("bus bandwidth timer running in disconnected state");
		return;
	}

	/* add intra bss forwarded tx and rx packets */
	tx_packets += fwd_tx_packets_diff;
	rx_packets += fwd_rx_packets_diff;

	if (ucfg_ipa_is_fw_wdi_activated(hdd_ctx->pdev)) {
		ucfg_ipa_uc_stat_query(hdd_ctx->pdev, &ipa_tx_packets,
				       &ipa_rx_packets);
		tx_packets += (uint64_t)ipa_tx_packets;
		rx_packets += (uint64_t)ipa_rx_packets;

		if (con_sap_adapter) {
			con_sap_adapter->stats.tx_packets += ipa_tx_packets;
			con_sap_adapter->stats.rx_packets += ipa_rx_packets;
		}

		ucfg_ipa_set_perf_level(hdd_ctx->pdev, tx_packets, rx_packets);
		ucfg_ipa_uc_stat_request(hdd_ctx->pdev, 2);
	}

	hdd_pld_request_bus_bandwidth(hdd_ctx, tx_packets, rx_packets);

restart_timer:
	/* ensure periodic timer should still be running before restarting it */
	qdf_spinlock_acquire(&hdd_ctx->bus_bw_timer_lock);
	if (hdd_ctx->bus_bw_timer_running)
		qdf_timer_mod(&hdd_ctx->bus_bw_timer,
				hdd_ctx->config->busBandwidthComputeInterval);
	qdf_spinlock_release(&hdd_ctx->bus_bw_timer_lock);
}

static void hdd_bus_bw_work_handler(struct work_struct *work)
{
	cds_ssr_protect(__func__);
	__hdd_bus_bw_work_handler(work);
	cds_ssr_unprotect(__func__);
}

/**
 * __hdd_bus_bw_cbk() - Bus bandwidth data structure callback.
 * @arg: Argument of timer function
 *
 * Schedule a workqueue in this function where all the processing is done.
 *
 * Return: None.
 */
static void __hdd_bus_bw_cbk(void *arg)
{
	struct hdd_context *hdd_ctx = (struct hdd_context *) arg;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	schedule_work(&hdd_ctx->bus_bw_work);
}

/**
 * hdd_bus_bw_cbk() - Wrapper for bus bw callback for SSR protection.
 * @arg: Argument of timer function
 *
 * Return: None.
 */
static void hdd_bus_bw_cbk(void *arg)
{
	cds_ssr_protect(__func__);
	__hdd_bus_bw_cbk(arg);
	cds_ssr_unprotect(__func__);
}

int hdd_bus_bandwidth_init(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	qdf_spinlock_create(&hdd_ctx->bus_bw_lock);

	hdd_pm_qos_add_request(hdd_ctx);

	INIT_WORK(&hdd_ctx->bus_bw_work, hdd_bus_bw_work_handler);
	hdd_ctx->bus_bw_timer_running = false;
	qdf_spinlock_create(&hdd_ctx->bus_bw_timer_lock);
	qdf_timer_init(NULL, &hdd_ctx->bus_bw_timer, hdd_bus_bw_cbk,
		       (void *)hdd_ctx, QDF_TIMER_TYPE_SW);

	hdd_exit();

	return 0;
}

void hdd_bus_bandwidth_deinit(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	QDF_BUG(!hdd_ctx->bus_bw_timer_running);

	qdf_timer_free(&hdd_ctx->bus_bw_timer);
	qdf_spinlock_destroy(&hdd_ctx->bus_bw_timer_lock);
	qdf_spinlock_destroy(&hdd_ctx->bus_bw_lock);
	hdd_pm_qos_remove_request(hdd_ctx);

	hdd_exit();
}

void hdd_bus_bw_compute_prev_txrx_stats(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	qdf_spin_lock_bh(&hdd_ctx->bus_bw_lock);
	adapter->prev_tx_packets = adapter->stats.tx_packets;
	adapter->prev_rx_packets = adapter->stats.rx_packets;
	cdp_get_intra_bss_fwd_pkts_count(cds_get_context(QDF_MODULE_ID_SOC),
					 adapter->session_id,
					 &adapter->prev_fwd_tx_packets,
					 &adapter->prev_fwd_rx_packets);
	qdf_spin_unlock_bh(&hdd_ctx->bus_bw_lock);
}

void hdd_bus_bw_compute_reset_prev_txrx_stats(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	qdf_spin_lock_bh(&hdd_ctx->bus_bw_lock);
	adapter->prev_tx_packets = 0;
	adapter->prev_rx_packets = 0;
	adapter->prev_fwd_tx_packets = 0;
	adapter->prev_fwd_rx_packets = 0;
	qdf_spin_unlock_bh(&hdd_ctx->bus_bw_lock);
}

#endif /* MSM_PLATFORM */

#ifdef WLAN_DEBUG
static uint8_t *convert_level_to_string(uint32_t level)
{
	switch (level) {
	/* initialize the wlan sub system */
	case WLAN_SVC_TP_NONE:
		return "NONE";
	case WLAN_SVC_TP_LOW:
		return "LOW";
	case WLAN_SVC_TP_MEDIUM:
		return "MED";
	case WLAN_SVC_TP_HIGH:
		return "HIGH";
	default:
		return "INVAL";
	}
}
#endif


/**
 * wlan_hdd_display_tx_rx_histogram() - display tx rx histogram
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void wlan_hdd_display_tx_rx_histogram(struct hdd_context *hdd_ctx)
{
	int i;

#ifdef MSM_PLATFORM
	hdd_debug("BW compute Interval: %dms",
		hdd_ctx->config->busBandwidthComputeInterval);
	hdd_debug("BW High TH: %d BW Med TH: %d BW Low TH: %d",
		hdd_ctx->config->busBandwidthHighThreshold,
		hdd_ctx->config->busBandwidthMediumThreshold,
		hdd_ctx->config->busBandwidthLowThreshold);
	hdd_debug("Enable TCP DEL ACK: %d",
		hdd_ctx->en_tcp_delack_no_lro);
	hdd_debug("TCP DEL High TH: %d TCP DEL Low TH: %d",
		hdd_ctx->config->tcpDelackThresholdHigh,
		hdd_ctx->config->tcpDelackThresholdLow);
	hdd_debug("TCP TX HIGH TP TH: %d (Use to set tcp_output_bytes_limit)",
		hdd_ctx->config->tcp_tx_high_tput_thres);
#endif

	hdd_debug("Total entries: %d Current index: %d",
		NUM_TX_RX_HISTOGRAM, hdd_ctx->hdd_txrx_hist_idx);

	hdd_debug("[index][timestamp]: interval_rx, interval_tx, bus_bw_level, RX TP Level, TX TP Level");

	for (i = 0; i < NUM_TX_RX_HISTOGRAM; i++) {
		/* using hdd_log to avoid printing function name */
		if (hdd_ctx->hdd_txrx_hist[i].qtime > 0)
			hdd_debug("[%3d][%15llu]: %6llu, %6llu, %s, %s, %s",
				  i, hdd_ctx->hdd_txrx_hist[i].qtime,
				  hdd_ctx->hdd_txrx_hist[i].interval_rx,
				  hdd_ctx->hdd_txrx_hist[i].interval_tx,
				  convert_level_to_string(
					hdd_ctx->hdd_txrx_hist[i].
						next_vote_level),
				  convert_level_to_string(
					hdd_ctx->hdd_txrx_hist[i].
						next_rx_level),
				  convert_level_to_string(
					hdd_ctx->hdd_txrx_hist[i].
						next_tx_level));
	}
}

/**
 * wlan_hdd_clear_tx_rx_histogram() - clear tx rx histogram
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void wlan_hdd_clear_tx_rx_histogram(struct hdd_context *hdd_ctx)
{
	hdd_ctx->hdd_txrx_hist_idx = 0;
	qdf_mem_zero(hdd_ctx->hdd_txrx_hist,
		(sizeof(struct hdd_tx_rx_histogram) * NUM_TX_RX_HISTOGRAM));
}

/* length of the netif queue log needed per adapter */
#define ADAP_NETIFQ_LOG_LEN ((20 * WLAN_REASON_TYPE_MAX) + 50)

/**
 *
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
	struct hdd_adapter *adapter = NULL;

	comb_log_str_size = (ADAP_NETIFQ_LOG_LEN * CSR_ROAM_SESSION_MAX) + 1;
	comb_log_str = qdf_mem_malloc(comb_log_str_size);
	if (!comb_log_str) {
		hdd_err("failed to alloc comb_log_str");
		return;
	}

	bytes_written = 0;

	hdd_for_each_adapter_dev_held(hdd_ctx, adapter) {
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
			adapter->session_id, adapter->device_mode,
			adapter->pause_map,
			qdf_system_ticks_to_msecs(pause),
			qdf_system_ticks_to_msecs(total),
			temp_str);

		adapter_num++;
		/* dev_put has to be done here */
		dev_put(adapter->dev);
	}

	/* using QDF_TRACE to avoid printing function name */
	QDF_TRACE(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_INFO_LOW,
		  "STATS |%s", comb_log_str);

	if (bytes_written >= comb_log_str_size)
		hdd_warn("log string truncated");

	qdf_mem_free(comb_log_str);
}

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

	hdd_debug("Netif queue operation statistics:");
	hdd_debug("Session_id %d device mode %d",
		  adapter->session_id, adapter->device_mode);
	hdd_debug("Current pause_map value %x", adapter->pause_map);
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
	hdd_debug("Total: %ums Pause: %ums Unpause: %ums",
		  qdf_system_ticks_to_msecs(total),
		  qdf_system_ticks_to_msecs(pause),
		  qdf_system_ticks_to_msecs(unpause));
	hdd_debug("reason_type: pause_cnt: unpause_cnt: pause_time");

	for (i = WLAN_CONTROL_PATH; i < WLAN_REASON_TYPE_MAX; i++) {
		qdf_time_t pause_delta = 0;

		if (adapter->pause_map & (1 << i))
			pause_delta = delta;

		/* using hdd_log to avoid printing function name */
		hdd_debug("%s: %d: %d: %ums",
			  hdd_reason_type_to_string(i),
			  adapter->queue_oper_stats[i].pause_count,
			  adapter->queue_oper_stats[i].unpause_count,
			  qdf_system_ticks_to_msecs(
			  adapter->queue_oper_stats[i].total_pause_time +
			  pause_delta));
	}

	hdd_debug("Netif queue operation history:");
	hdd_debug("Total entries: %d current index %d",
		  WLAN_HDD_MAX_HISTORY_ENTRY, adapter->history_index);

	hdd_debug("index: time: action_type: reason_type: pause_map");

	for (i = 0; i < WLAN_HDD_MAX_HISTORY_ENTRY; i++) {
		/* using hdd_log to avoid printing function name */
		if (adapter->queue_oper_history[i].time == 0)
			continue;
		hdd_debug("%d: %u: %s: %s: %x",
			  i, qdf_system_ticks_to_msecs(
				adapter->queue_oper_history[i].time),
			  hdd_action_type_to_string(
			  adapter->queue_oper_history[i].netif_action),
			  hdd_reason_type_to_string(
			  adapter->queue_oper_history[i].netif_reason),
			  adapter->queue_oper_history[i].pause_map);
	}
}

void
wlan_hdd_display_adapter_netif_queue_history(struct hdd_adapter *adapter)
{
	wlan_hdd_display_adapter_netif_queue_stats(adapter);
}

/**
 * wlan_hdd_display_netif_queue_history() - display netif queue history
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void
wlan_hdd_display_netif_queue_history(struct hdd_context *hdd_ctx,
				     enum qdf_stats_verbosity_level verb_lvl)
{
	struct hdd_adapter *adapter = NULL;

	if (verb_lvl == QDF_STATS_VERBOSITY_LEVEL_LOW) {
		hdd_display_netif_queue_history_compact(hdd_ctx);
		return;
	}

	hdd_for_each_adapter_dev_held(hdd_ctx, adapter) {
		wlan_hdd_display_adapter_netif_queue_stats(adapter);
		/* dev_put has to be done here */
		dev_put(adapter->dev);
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
	struct hdd_adapter *adapter = NULL;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		qdf_mem_zero(adapter->queue_oper_stats,
					sizeof(adapter->queue_oper_stats));
		qdf_mem_zero(adapter->queue_oper_history,
					sizeof(adapter->queue_oper_history));
		adapter->history_index = 0;
		adapter->start_time = adapter->last_time = qdf_system_ticks();
		adapter->total_pause_time = 0;
		adapter->total_unpause_time = 0;
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
 * @phddctx: struct hdd_context structure pointer
 * @enable: enable or disable this behaviour
 *
 * Return: int
 */
static int wlan_hdd_set_wow_pulse(struct hdd_context *phddctx, bool enable)
{
	struct hdd_config *pcfg_ini = phddctx->config;
	struct wow_pulse_mode wow_pulse_set_info;
	QDF_STATUS status;

	hdd_debug("wow pulse enable flag is %d", enable);

	if (false == phddctx->config->wow_pulse_support)
		return 0;

	/* prepare the request to send to SME */
	if (enable == true) {
		wow_pulse_set_info.wow_pulse_enable = true;
		wow_pulse_set_info.wow_pulse_pin =
				pcfg_ini->wow_pulse_pin;
		wow_pulse_set_info.wow_pulse_interval_low =
				pcfg_ini->wow_pulse_interval_low;
		wow_pulse_set_info.wow_pulse_interval_high =
				pcfg_ini->wow_pulse_interval_high;
	} else {
		wow_pulse_set_info.wow_pulse_enable = false;
		wow_pulse_set_info.wow_pulse_pin = 0;
		wow_pulse_set_info.wow_pulse_interval_low = 0;
		wow_pulse_set_info.wow_pulse_interval_high = 0;
	}
	hdd_debug("enable %d pin %d low %d high %d",
		wow_pulse_set_info.wow_pulse_enable,
		wow_pulse_set_info.wow_pulse_pin,
		wow_pulse_set_info.wow_pulse_interval_low,
		wow_pulse_set_info.wow_pulse_interval_high);

	status = sme_set_wow_pulse(&wow_pulse_set_info);
	if (QDF_STATUS_E_FAILURE == status) {
		hdd_debug("sme_set_wow_pulse failure!");
		return -EIO;
	}
	hdd_debug("sme_set_wow_pulse success!");
	return 0;
}
#else
static inline int wlan_hdd_set_wow_pulse(struct hdd_context *phddctx, bool enable)
{
	return 0;
}
#endif

#ifdef WLAN_FEATURE_FASTPATH
/**
 * hdd_enable_fastpath() - Enable fastpath if enabled in config INI
 * @hdd_cfg: hdd config
 * @context: lower layer context
 *
 * Return: none
 */
void hdd_enable_fastpath(struct hdd_config *hdd_cfg,
				void *context)
{
	if (hdd_cfg->fastpath_enable)
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

	policy_mgr_change_sap_channel_with_csa(hdd_ctx->psoc,
		adapter->session_id, channel,
		hdd_ap_ctx->sap_config.ch_width_orig, forced);
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
hdd_store_sap_restart_channel(uint8_t restart_chan, uint8_t *restart_chan_store)
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
 * @hdd_ctx: hdd context pointer
 *
 * hdd_unsafe_channel_restart_sap check all unsafe channel list
 * and if ACS is enabled, driver will ask userspace to restart the
 * sap. User space on LTE coex indication restart driver.
 *
 * Return - none
 */
void hdd_unsafe_channel_restart_sap(struct hdd_context *hdd_ctxt)
{
	struct hdd_adapter *adapter;
	uint32_t i;
	bool found = false;
	uint8_t restart_chan_store[SAP_MAX_NUM_SESSION] = {0};
	uint8_t restart_chan;

	hdd_for_each_adapter(hdd_ctxt, adapter) {
		if (!(adapter->device_mode == QDF_SAP_MODE &&
		    adapter->session.ap.sap_config.acs_cfg.acs_mode)) {
			hdd_debug("skip device mode:%d acs:%d",
				  adapter->device_mode,
				  adapter->session.ap.sap_config.
				  acs_cfg.acs_mode);
			continue;
		}

		found = false;
		/*
		 * If STA+SAP is doing SCC & g_sta_sap_scc_on_lte_coex_chan
		 * is set, no need to move SAP.
		 */
		if (policy_mgr_is_sta_sap_scc(hdd_ctxt->psoc,
			adapter->session.ap.operating_channel) &&
			hdd_ctxt->config->sta_sap_scc_on_lte_coex_chan)
			hdd_debug("SAP is allowed on SCC channel, no need to move SAP");
		else {
			for (i = 0; i < hdd_ctxt->unsafe_channel_count; i++) {
				if (adapter->session.ap.operating_channel ==
					hdd_ctxt->unsafe_channel_list[i]) {
					found = true;
					hdd_debug("operating ch:%d is unsafe",
					adapter->session.ap.operating_channel);
					break;
				}
			}
		}
		if (!found) {
			hdd_store_sap_restart_channel(
				adapter->session.ap.operating_channel,
				restart_chan_store);
			hdd_debug("ch:%d is safe. no need to change channel",
				  adapter->session.ap.operating_channel);
			continue;
		}

		if (hdd_ctxt->config->vendor_acs_support &&
		    hdd_ctxt->config->acs_support_for_dfs_ltecoex) {
			hdd_update_acs_timer_reason(adapter,
				QCA_WLAN_VENDOR_ACS_SELECT_REASON_LTE_COEX);
			continue;
		}

		restart_chan = 0;
		for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
			if (!restart_chan_store[i])
				continue;

			if (policy_mgr_is_force_scc(hdd_ctxt->psoc) &&
			    WLAN_REG_IS_SAME_BAND_CHANNELS(
							restart_chan_store[i],
							adapter->session.ap.
							operating_channel)) {
				restart_chan = restart_chan_store[i];
				break;
			}
		}
		if (!restart_chan)
			restart_chan =
				wlansap_get_safe_channel_from_pcl_and_acs_range(
					WLAN_HDD_GET_SAP_CTX_PTR(adapter));

		if (!restart_chan) {
			hdd_err("fail to restart SAP");
		} else {
			/*
			 * SAP restart due to unsafe channel. While
			 * restarting the SAP, make sure to clear
			 * acs_channel, channel to reset to
			 * 0. Otherwise these settings will override
			 * the ACS while restart.
			 */
			hdd_ctxt->acs_policy.acs_channel = AUTO_CHANNEL_SELECT;
			hdd_debug("sending coex indication");
			wlan_hdd_send_svc_nlink_msg(hdd_ctxt->radio_index,
					WLAN_SVC_LTE_COEX_IND, NULL, 0);
			hdd_debug("driver to start sap: %d",
				hdd_ctxt->config->sap_internal_restart);
			if (hdd_ctxt->config->sap_internal_restart) {
				wlan_hdd_set_sap_csa_reason(hdd_ctxt->psoc,
						adapter->session_id,
						CSA_REASON_UNSAFE_CHANNEL);
				hdd_switch_sap_channel(adapter, restart_chan,
						       true);
				hdd_store_sap_restart_channel(
							restart_chan,
							restart_chan_store);
			} else {
				return;
			}
		}
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

	pld_get_wlan_unsafe_channel(hdd_ctx->parent_dev,
				    hdd_ctx->unsafe_channel_list,
				     &(hdd_ctx->unsafe_channel_count),
				     sizeof(uint16_t) * NUM_CHANNELS);

	hdd_debug("num of unsafe channels is %d",
	       hdd_ctx->unsafe_channel_count);

	unsafe_channel_count = QDF_MIN((uint16_t)hdd_ctx->unsafe_channel_count,
				       (uint16_t)NUM_CHANNELS);

	for (index = 0; index < unsafe_channel_count; index++) {
		hdd_debug("channel %d is not safe",
		       hdd_ctx->unsafe_channel_list[index]);

	}

}

static void hdd_lte_coex_restart_sap(struct hdd_adapter *adapter,
				     struct hdd_context *hdd_ctx)
{
	uint8_t restart_chan;

	restart_chan = wlansap_get_safe_channel_from_pcl_and_acs_range(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter));
	if (!restart_chan) {
		hdd_alert("fail to restart SAP");
		return;
	}

	/* SAP restart due to unsafe channel. While restarting
	 * the SAP, make sure to clear acs_channel, channel to
	 * reset to 0. Otherwise these settings will override
	 * the ACS while restart.
	 */
	hdd_ctx->acs_policy.acs_channel = AUTO_CHANNEL_SELECT;

	hdd_debug("sending coex indication");

	wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
				    WLAN_SVC_LTE_COEX_IND, NULL, 0);
	wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc, adapter->session_id,
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
		if (!unsafe_list) {
			hdd_err("No memory for unsafe chan list size%d",
				size);
			return -ENOMEM;
		}
		qdf_mem_copy(unsafe_list, hdd_ctx->unsafe_channel_list, size);
	} else {
		unsafe_list = NULL;
	}

	*local_unsafe_list = unsafe_list;
	*local_unsafe_list_count = chan_count;

	return 0;
}

bool hdd_local_unsafe_channel_updated(struct hdd_context *hdd_ctx,
	uint16_t *local_unsafe_list, uint16_t local_unsafe_list_count)
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
	if (i >= local_unsafe_list_count) {
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

QDF_STATUS
wlan_hdd_get_adapter_by_vdev_id_from_objmgr(struct hdd_context *hdd_ctx,
					    struct hdd_adapter **adapter,
					    struct wlan_objmgr_vdev *vdev)
{
	*adapter = NULL;
	if (!hdd_ctx)
		return QDF_STATUS_E_INVAL;

	if (!vdev || !vdev->vdev_nif.osdev) {
		hdd_err("null vdev object");
		return QDF_STATUS_E_INVAL;
	}

	*adapter = vdev->vdev_nif.osdev->legacy_osif_priv;

	return QDF_STATUS_SUCCESS;
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
	struct hdd_adapter *adapter = NULL;
	int i, num_adapters;
	uint8_t vdev_id[WLAN_MAX_VDEVS];
	struct ieee80211_mgmt *mgmt =
		(struct ieee80211_mgmt *)frame_ind->frameBuf;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (frame_ind->frame_len < ieee80211_hdrlen(mgmt->frame_control)) {
		hdd_err(" Invalid frame length");
		return;
	}

	if (SME_SESSION_ID_ANY == frame_ind->sessionId) {
		for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
			adapter =
				hdd_get_adapter_by_sme_session_id(hdd_ctx, i);
			if (adapter)
				break;
		}
	} else if (SME_SESSION_ID_BROADCAST == frame_ind->sessionId) {
		num_adapters = 0;
		hdd_for_each_adapter_dev_held(hdd_ctx, adapter) {
			vdev_id[num_adapters] = adapter->session_id;
			num_adapters++;
			/* dev_put has to be done here */
			dev_put(adapter->dev);
		}

		adapter = NULL;

		for (i = 0; i < num_adapters; i++) {
			vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
							hdd_ctx->psoc,
							vdev_id[i],
							WLAN_OSIF_ID);

			if (!vdev)
				continue;

			status = wlan_hdd_get_adapter_by_vdev_id_from_objmgr(
						hdd_ctx, &adapter, vdev);

			if (QDF_IS_STATUS_ERROR(status) || !adapter) {
				wlan_objmgr_vdev_release_ref(vdev,
							     WLAN_OSIF_ID);
				continue;
			}

			__hdd_indicate_mgmt_frame(adapter,
						  frame_ind->frame_len,
						  frame_ind->frameBuf,
						  frame_ind->frameType,
						  frame_ind->rxChan,
						  frame_ind->rxRssi,
						  frame_ind->rx_flags);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_ID);
		}

		adapter = NULL;
	} else {
		adapter = hdd_get_adapter_by_sme_session_id(hdd_ctx,
					frame_ind->sessionId);
	}

	if ((NULL != adapter) &&
		(WLAN_HDD_ADAPTER_MAGIC == adapter->magic))
		__hdd_indicate_mgmt_frame(adapter,
						frame_ind->frame_len,
						frame_ind->frameBuf,
						frame_ind->frameType,
						frame_ind->rxChan,
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
		hdd_err("invlaid timer context");
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
					     adapter->session_id);
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

	if (0 == enable_dfs_chan_scan || 1 == enable_dfs_chan_scan) {
		hdd_ctx->config->enableDFSChnlScan = enable_dfs_chan_scan;
		hdd_debug("Module enable_dfs_chan_scan set to %d",
			   enable_dfs_chan_scan);
	}
	if (0 == enable_11d || 1 == enable_11d) {
		hdd_ctx->config->Is11dSupportEnabled = enable_11d;
		hdd_debug("Module enable_11d set to %d", enable_11d);
	}

	if (!ucfg_ipa_is_present()) {
		hdd_ctx->config->IpaConfig = 0;
		hdd_debug("IpaConfig override to %d",
			hdd_ctx->config->IpaConfig);
	}

	if (!hdd_ctx->config->rssi_assoc_reject_enabled ||
	    !hdd_ctx->config->enable_bcast_probe_rsp) {
		hdd_debug("OCE disabled, rssi_assoc_reject_enabled: %d enable_bcast_probe_rsp: %d",
			  hdd_ctx->config->rssi_assoc_reject_enabled,
			  hdd_ctx->config->enable_bcast_probe_rsp);
		hdd_ctx->config->oce_sta_enabled = 0;
	}

	if (hdd_ctx->config->action_oui_enable && !ucfg_action_oui_enabled()) {
		hdd_ctx->config->action_oui_enable = 0;
		hdd_err("Ignore ini: %s, since no action_oui component",
			CFG_ENABLE_ACTION_OUI);
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
 * hdd_set_trace_level_for_each - Set trace level for each INI config
 * @hdd_ctx - HDD context
 *
 * Set trace level for each module based on INI config.
 *
 * Return: None
 */
static void hdd_set_trace_level_for_each(struct hdd_context *hdd_ctx)
{
	hdd_qdf_trace_enable(QDF_MODULE_ID_WMI,
			     hdd_ctx->config->qdf_trace_enable_wdi);
	hdd_qdf_trace_enable(QDF_MODULE_ID_HDD,
			     hdd_ctx->config->qdf_trace_enable_hdd);
	hdd_qdf_trace_enable(QDF_MODULE_ID_SME,
			     hdd_ctx->config->qdf_trace_enable_sme);
	hdd_qdf_trace_enable(QDF_MODULE_ID_PE,
			     hdd_ctx->config->qdf_trace_enable_pe);
	hdd_qdf_trace_enable(QDF_MODULE_ID_WMA,
			     hdd_ctx->config->qdf_trace_enable_wma);
	hdd_qdf_trace_enable(QDF_MODULE_ID_SYS,
			     hdd_ctx->config->qdf_trace_enable_sys);
	hdd_qdf_trace_enable(QDF_MODULE_ID_QDF,
			     hdd_ctx->config->qdf_trace_enable_qdf);
	hdd_qdf_trace_enable(QDF_MODULE_ID_SAP,
			     hdd_ctx->config->qdf_trace_enable_sap);
	hdd_qdf_trace_enable(QDF_MODULE_ID_HDD_SOFTAP,
			     hdd_ctx->config->qdf_trace_enable_hdd_sap);
	hdd_qdf_trace_enable(QDF_MODULE_ID_BMI,
				hdd_ctx->config->qdf_trace_enable_bmi);
	hdd_qdf_trace_enable(QDF_MODULE_ID_CFG,
				hdd_ctx->config->qdf_trace_enable_cfg);
	hdd_qdf_trace_enable(QDF_MODULE_ID_EPPING,
				hdd_ctx->config->qdf_trace_enable_epping);
	hdd_qdf_trace_enable(QDF_MODULE_ID_QDF_DEVICE,
				hdd_ctx->config->qdf_trace_enable_qdf_devices);
	hdd_qdf_trace_enable(QDF_MODULE_ID_TXRX,
				hdd_ctx->config->qdf_trace_enable_txrx);
	hdd_qdf_trace_enable(QDF_MODULE_ID_DP,
				hdd_ctx->config->qdf_trace_enable_dp);
	hdd_qdf_trace_enable(QDF_MODULE_ID_HTC,
				hdd_ctx->config->qdf_trace_enable_htc);
	hdd_qdf_trace_enable(QDF_MODULE_ID_HIF,
				hdd_ctx->config->qdf_trace_enable_hif);
	hdd_qdf_trace_enable(QDF_MODULE_ID_HDD_SAP_DATA,
				hdd_ctx->config->qdf_trace_enable_hdd_sap_data);
	hdd_qdf_trace_enable(QDF_MODULE_ID_HDD_DATA,
				hdd_ctx->config->qdf_trace_enable_hdd_data);
	hdd_qdf_trace_enable(QDF_MODULE_ID_WIFIPOS,
				hdd_ctx->config->qdf_trace_enable_wifi_pos);
	hdd_qdf_trace_enable(QDF_MODULE_ID_NAN,
				hdd_ctx->config->qdf_trace_enable_nan);
	hdd_qdf_trace_enable(QDF_MODULE_ID_REGULATORY,
				hdd_ctx->config->qdf_trace_enable_regulatory);
	hdd_qdf_trace_enable(QDF_MODULE_ID_CP_STATS,
				hdd_ctx->config->qdf_trace_enable_cp_stats);

	hdd_set_mtrace_for_each(hdd_ctx);

	hdd_cfg_print(hdd_ctx);
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
	hdd_ctx->max_intf_count = CSR_ROAM_SESSION_MAX;

	init_completion(&hdd_ctx->mc_sus_event_var);
	init_completion(&hdd_ctx->ready_to_suspend);
	init_completion(&hdd_ctx->pdev_cmd_flushed_var);

	qdf_spinlock_create(&hdd_ctx->connection_status_lock);
	qdf_spinlock_create(&hdd_ctx->sta_update_info_lock);
	qdf_spinlock_create(&hdd_ctx->hdd_adapter_lock);

	qdf_list_create(&hdd_ctx->hdd_adapters, 0);

	ret = hdd_scan_context_init(hdd_ctx);
	if (ret)
		goto list_destroy;

	hdd_rx_wake_lock_create(hdd_ctx);

	ret = hdd_sap_context_init(hdd_ctx);
	if (ret)
		goto scan_destroy;

	wlan_hdd_cfg80211_extscan_init(hdd_ctx);

	hdd_init_offloaded_packets_ctx(hdd_ctx);

	ret = wlan_hdd_cfg80211_init(hdd_ctx->parent_dev, hdd_ctx->wiphy,
				     hdd_ctx->config);
	if (ret)
		goto sap_destroy;

	qdf_wake_lock_create(&hdd_ctx->monitor_mode_wakelock,
			     "monitor_mode_wakelock");

	return 0;

sap_destroy:
	hdd_sap_context_destroy(hdd_ctx);

scan_destroy:
	hdd_scan_context_destroy(hdd_ctx);
	hdd_rx_wake_lock_destroy(hdd_ctx);
list_destroy:
	qdf_list_destroy(&hdd_ctx->hdd_adapters);

	return ret;
}

/*
 * enum hdd_block_shutdown - Control if driver allows modem shutdown
 * @HDD_UNBLOCK_MODEM_SHUTDOWN: Unblock shutdown
 * @HDD_BLOCK_MODEM_SHUTDOWN: Block shutdown
 *
 * On calling pld_block_shutdown API with the given values, modem
 * graceful shutdown is blocked/unblocked.
 */
enum hdd_block_shutdown {
	HDD_UNBLOCK_MODEM_SHUTDOWN,
	HDD_BLOCK_MODEM_SHUTDOWN,
};

/**
 * ie_whitelist_attrs_init() - initialize ie whitelisting attributes
 * @hdd_ctx: pointer to hdd context
 *
 * Return: status of initialization
 *         0 - success
 *         negative value - failure
 */
static int ie_whitelist_attrs_init(struct hdd_context *hdd_ctx)
{
	int ret;

	if (!hdd_ctx->config->probe_req_ie_whitelist)
		return 0;

	if (!hdd_validate_prb_req_ie_bitmap(hdd_ctx)) {
		hdd_err("invalid ie bitmap and ouis: disable ie whitelisting");
		hdd_ctx->config->probe_req_ie_whitelist = false;
		return -EINVAL;
	}

	/* parse ini string probe req oui */
	ret = hdd_parse_probe_req_ouis(hdd_ctx);
	if (ret) {
		hdd_err("parsing error: disable ie whitelisting");
		hdd_ctx->config->probe_req_ie_whitelist = false;
	}

	return ret;
}

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
static void hdd_set_wlan_logging(struct hdd_context *hdd_ctx)
{
	wlan_logging_set_log_to_console(hdd_ctx->config->
					wlan_logging_to_console);
	wlan_logging_set_active(hdd_ctx->config->wlan_logging_enable);
}
#else
static void hdd_set_wlan_logging(struct hdd_context *hdd_ctx)
{ }
#endif


static int hdd_mode_change_psoc_idle_shutdown(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	int ret;

	if (!hdd_ctx)
		return -EINVAL;

	ret = hdd_wlan_stop_modules(hdd_ctx, true);
	if (ret)
		hdd_err("Failed to stop modules");

	is_mode_change_psoc_idle_shutdown = false;

	return ret;
}

static int hdd_mode_change_psoc_idle_restart(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return -EINVAL;

	return hdd_wlan_start_modules(hdd_ctx, false);
}

/**
 * hdd_psoc_idle_timeout_callback() - Handler for psoc idle timeout
 * @priv: pointer to hdd context
 *
 * Return: None
 */
static void hdd_psoc_idle_timeout_callback(void *priv)
{
	struct hdd_context *hdd_ctx = priv;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	hdd_info("Psoc idle timeout elapsed; starting psoc shutdown");

	pld_idle_shutdown(hdd_ctx->parent_dev, hdd_psoc_idle_shutdown);
}


static int __hdd_psoc_idle_restart(struct hdd_context *hdd_ctx)
{
	return hdd_wlan_start_modules(hdd_ctx, false);
}

int hdd_psoc_idle_restart(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		hdd_err_rl("hdd ctx is null");
		return -EINVAL;
	}

	return __hdd_psoc_idle_restart(hdd_ctx);
}

int hdd_trigger_psoc_idle_restart(struct hdd_context *hdd_ctx)
{
	int ret;

	QDF_BUG(rtnl_is_locked());

	mutex_lock(&hdd_ctx->iface_change_lock);
	if (hdd_ctx->driver_status == DRIVER_MODULES_ENABLED) {
		mutex_unlock(&hdd_ctx->iface_change_lock);
		hdd_psoc_idle_timer_stop(hdd_ctx);
		hdd_nofl_debug("Driver modules already Enabled");
		return 0;
	}

	mutex_unlock(&hdd_ctx->iface_change_lock);
	ret = pld_idle_restart(hdd_ctx->parent_dev, hdd_psoc_idle_restart);
	return ret;
}

int hdd_psoc_idle_shutdown(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	int ret;

	if (is_mode_change_psoc_idle_shutdown)
		return hdd_mode_change_psoc_idle_shutdown(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	hdd_enter();

	ret = hdd_wlan_stop_modules(hdd_ctx, false);
	if (ret) {
		hdd_err("Failed to start modules");
		return ret;
	}

	hdd_exit();

	return 0;
}

void hdd_psoc_idle_timer_start(struct hdd_context *hdd_ctx)
{
	uint32_t timeout_ms = hdd_ctx->config->iface_change_wait_time;
	enum wake_lock_reason reason =
		WIFI_POWER_EVENT_WAKELOCK_IFACE_CHANGE_TIMER;

	if (!timeout_ms) {
		hdd_info("psoc idle timer is disabled");
		return;
	}

	hdd_debug("Starting psoc idle timer");
	qdf_sched_delayed_work(&hdd_ctx->psoc_idle_timeout_work, timeout_ms);
	hdd_prevent_suspend_timeout(timeout_ms, reason);
}

void hdd_psoc_idle_timer_stop(struct hdd_context *hdd_ctx)
{
	qdf_cancel_delayed_work(&hdd_ctx->psoc_idle_timeout_work);
	hdd_debug("Stopped psoc idle timer");
}


/**
 * hdd_context_create() - Allocate and inialize HDD context.
 * @dev:	Device Pointer to the underlying device
 *
 * Allocate and initialize HDD context. HDD context is allocated as part of
 * wiphy allocation and then context is initialized.
 *
 * Return: HDD context on success and ERR_PTR on failure
 */
static struct hdd_context *hdd_context_create(struct device *dev)
{
	QDF_STATUS status;
	int ret = 0;
	struct hdd_context *hdd_ctx;

	hdd_enter();

	hdd_ctx = hdd_cfg80211_wiphy_alloc(sizeof(struct hdd_context));
	if (hdd_ctx == NULL) {
		ret = -ENOMEM;
		goto err_out;
	}

	qdf_create_delayed_work(&hdd_ctx->psoc_idle_timeout_work,
				hdd_psoc_idle_timeout_callback,
				(void *)hdd_ctx);

	mutex_init(&hdd_ctx->iface_change_lock);

	hdd_ctx->parent_dev = dev;
	hdd_ctx->last_scan_reject_session_id = 0xFF;

	hdd_ctx->config = qdf_mem_malloc(sizeof(struct hdd_config));
	if (hdd_ctx->config == NULL) {
		hdd_err("Failed to alloc memory for HDD config!");
		ret = -ENOMEM;
		goto err_free_hdd_context;
	}

	/* Read and parse the qcom_cfg.ini file */
	status = hdd_parse_config_ini(hdd_ctx);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Error (status: %d) parsing INI file: %s", status,
			  WLAN_INI_FILE);
		ret = -EINVAL;
		goto err_free_config;
	}

	ie_whitelist_attrs_init(hdd_ctx);

	hdd_debug("setting timer multiplier: %u",
		  hdd_ctx->config->timer_multiplier);
	qdf_timer_set_multiplier(hdd_ctx->config->timer_multiplier);


	if (hdd_ctx->config->fhostNSOffload)
		hdd_ctx->ns_offload_enable = true;

	cds_set_fatal_event(hdd_ctx->config->enable_fatal_event);

	hdd_override_ini_config(hdd_ctx);

	ret = hdd_context_init(hdd_ctx);

	if (ret)
		goto err_free_config;

	/* Uses to enabled logging after SSR */
	hdd_ctx->fw_log_settings.enable = hdd_ctx->config->enable_fw_log;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam())
		goto skip_multicast_logging;

	cds_set_multicast_logging(hdd_ctx->config->multicast_host_fw_msgs);

	ret = hdd_init_netlink_services(hdd_ctx);
	if (ret)
		goto err_deinit_hdd_context;

	hdd_set_wlan_logging(hdd_ctx);

skip_multicast_logging:
	hdd_set_trace_level_for_each(hdd_ctx);

	cds_set_context(QDF_MODULE_ID_HDD, hdd_ctx);

	wlan_hdd_sar_timers_init(hdd_ctx);

	hdd_exit();

	return hdd_ctx;

err_deinit_hdd_context:
	hdd_context_deinit(hdd_ctx);

err_free_config:
	qdf_mem_free(hdd_ctx->config);

err_free_hdd_context:
	mutex_destroy(&hdd_ctx->iface_change_lock);
	wiphy_free(hdd_ctx->wiphy);

err_out:
	return ERR_PTR(ret);
}

#ifdef WLAN_OPEN_P2P_INTERFACE
/**
 * hdd_open_p2p_interface - Open P2P interface
 * @hdd_ctx:	HDD context
 * @rtnl_held:	True if RTNL lock held
 *
 * Open P2P interface during probe. This function called to open the P2P
 * interface at probe along with STA interface.
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_open_p2p_interface(struct hdd_context *hdd_ctx, bool rtnl_held)
{
	struct hdd_adapter *adapter;
	uint8_t *p2p_dev_addr;
	bool is_p2p_locally_administered = false;

	if (hdd_ctx->config->isP2pDeviceAddrAdministrated) {
		if (hdd_ctx->num_provisioned_addr &&
		    !(hdd_ctx->provisioned_mac_addr[0].bytes[0] & 0x02)) {
		qdf_mem_copy(hdd_ctx->p2p_device_address.bytes,
			     hdd_ctx->provisioned_mac_addr[0].bytes,
			     sizeof(tSirMacAddr));

			/*
			 * Generate the P2P Device Address.  This consists of
			 * the device's primary MAC address with the locally
			 * administered bit set.
			 */

			hdd_ctx->p2p_device_address.bytes[0] |= 0x02;
			is_p2p_locally_administered = true;
		} else if (!(hdd_ctx->derived_mac_addr[0].bytes[0] & 0x02)) {
		qdf_mem_copy(hdd_ctx->p2p_device_address.bytes,
			     hdd_ctx->derived_mac_addr[0].bytes,
			     sizeof(tSirMacAddr));
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
		p2p_dev_addr = wlan_hdd_get_intf_addr(hdd_ctx,
						      QDF_P2P_DEVICE_MODE);
		if (!p2p_dev_addr) {
			hdd_err("Failed to allocate mac_address for p2p_device");
			return -ENOSPC;
		}

		qdf_mem_copy(&hdd_ctx->p2p_device_address.bytes[0],
			     p2p_dev_addr, QDF_MAC_ADDR_SIZE);
	}

	adapter = hdd_open_adapter(hdd_ctx, QDF_P2P_DEVICE_MODE, "p2p%d",
				   &hdd_ctx->p2p_device_address.bytes[0],
				   NET_NAME_UNKNOWN, rtnl_held);

	if (NULL == adapter) {
		hdd_err("Failed to do hdd_open_adapter for P2P Device Interface");
		return -ENOSPC;
	}

	return 0;
}
#else
static inline int hdd_open_p2p_interface(struct hdd_context *hdd_ctx,
					 bool rtnl_held)
{
	return 0;
}
#endif

static int hdd_open_ocb_interface(struct hdd_context *hdd_ctx, bool rtnl_held)
{
	struct hdd_adapter *adapter;
	int ret = 0;

	adapter = hdd_open_adapter(hdd_ctx, QDF_OCB_MODE, "wlanocb%d",
				   wlan_hdd_get_intf_addr(hdd_ctx,
							  QDF_OCB_MODE),
				   NET_NAME_UNKNOWN, rtnl_held);
	if (adapter == NULL) {
		hdd_err("Failed to open 802.11p interface");
		ret = -ENOSPC;
	}

	return ret;
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
			adapter->session_id);
		return qdf_status_to_os_return(QDF_STATUS_SUCCESS);
	}

	ret = hdd_vdev_create(adapter, hdd_sme_roam_callback, adapter);
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

	hdd_exit();

	return 0;
}

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

	hdd_enter();

	if (test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		hdd_err("session is already opened, %d",
			adapter->session_id);
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

	ret = hdd_vdev_create(adapter, wlansap_roam_callback,
			      adapter->session.ap.sap_context);
	if (ret) {
		hdd_err("failed to create vdev, status:%d", ret);
		hdd_sap_destroy_ctx(adapter);
		return ret;
	}

	if (adapter->device_mode == QDF_SAP_MODE)
		sme_cli_set_command(adapter->session_id,
			WMI_VDEV_PARAM_ENABLE_DISABLE_RTT_RESPONDER_ROLE,
			(bool)(hdd_ctx->config->fine_time_meas_cap &
							WMI_FW_AP_RTT_RESPR),
			VDEV_CMD);

	status = hdd_init_ap_mode(adapter, is_ssr);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Error Initializing the AP mode: %d", status);
		return qdf_status_to_os_return(status);
	}

	hdd_register_tx_flow_control(adapter,
		hdd_softap_tx_resume_timer_expired_handler,
		hdd_softap_tx_resume_cb,
		hdd_tx_flow_control_is_pause);

	hdd_exit();
	return 0;
}

static int hdd_open_concurrent_interface(struct hdd_context *hdd_ctx,
								bool rtnl_held)
{
	struct hdd_adapter *adapter;

	adapter = hdd_open_adapter(hdd_ctx, QDF_STA_MODE,
				       hdd_ctx->config->enableConcurrentSTA,
				       wlan_hdd_get_intf_addr(hdd_ctx,
							      QDF_STA_MODE),
				       NET_NAME_UNKNOWN, rtnl_held);

	if (!adapter)
		return -ENOSPC;

	return 0;
}

/**
 * hdd_open_interfaces - Open all required interfaces
 * hdd_ctx:	HDD context
 * rtnl_held: True if RTNL lock is held
 *
 * Open all the interfaces like STA, P2P and OCB based on the configuration.
 *
 * Return: 0 if all interfaces were created, otherwise negative errno
 */
static int hdd_open_interfaces(struct hdd_context *hdd_ctx, bool rtnl_held)
{
	struct hdd_adapter *adapter;
	enum QDF_GLOBAL_MODE curr_mode;
	int ret;
	bool nan_iface_support;

	curr_mode = hdd_get_conparam();
	/* open monitor mode adapter if con_mode is monitor mode */
	if (curr_mode == QDF_GLOBAL_MONITOR_MODE ||
	    curr_mode == QDF_GLOBAL_FTM_MODE) {
		uint8_t session_type = (curr_mode == QDF_GLOBAL_MONITOR_MODE) ?
					QDF_MONITOR_MODE : QDF_FTM_MODE;

		adapter = hdd_open_adapter(hdd_ctx, session_type, "wlan%d",
					   wlan_hdd_get_intf_addr(
								hdd_ctx,
								session_type),
					   NET_NAME_UNKNOWN, rtnl_held);
		if (!adapter) {
			hdd_err("open adapter failed");
			return -ENOSPC;
		}

		return 0;
	}

	if (hdd_ctx->config->dot11p_mode == WLAN_HDD_11P_STANDALONE)
		/* Create only 802.11p interface */
		return hdd_open_ocb_interface(hdd_ctx, rtnl_held);

	adapter = hdd_open_adapter(hdd_ctx, QDF_STA_MODE, "wlan%d",
				   wlan_hdd_get_intf_addr(hdd_ctx,
							  QDF_STA_MODE),
				   NET_NAME_UNKNOWN, rtnl_held);

	if (adapter == NULL)
		return -ENOSPC;

	if (strlen(hdd_ctx->config->enableConcurrentSTA) != 0) {
		ret = hdd_open_concurrent_interface(hdd_ctx, rtnl_held);
		if (ret)
			hdd_err("Cannot create concurrent STA interface");
	}

	ret = hdd_open_p2p_interface(hdd_ctx, rtnl_held);
	if (ret)
		goto err_close_adapters;

	nan_iface_support = wlan_hdd_nan_separate_iface_supported(hdd_ctx);
	if (!hdd_ctx->nan_seperate_vdev_supported || !nan_iface_support)
		hdd_debug("NAN separate vdev%s supported by host,%s supported by firmware",
			   nan_iface_support ? "" : " not",
			   hdd_ctx->nan_seperate_vdev_supported ? "" : " not");
	if (!wlan_hdd_nan_is_supported(hdd_ctx))
		hdd_debug("NAN is disabled");

	if (wlan_hdd_nan_is_supported(hdd_ctx) &&
	    hdd_ctx->nan_seperate_vdev_supported && nan_iface_support) {
		adapter = hdd_open_adapter(hdd_ctx, QDF_NAN_DISC_MODE, "wifi-aware%d",
				   wlan_hdd_get_intf_addr(hdd_ctx,
							  QDF_NAN_DISC_MODE),
				   NET_NAME_UNKNOWN, rtnl_held);
		if (!adapter) {
			hdd_err("Failed to create nan interface");
			goto err_close_adapters;
		}
	}

	/* Open 802.11p Interface */
	if (hdd_ctx->config->dot11p_mode == WLAN_HDD_11P_CONCURRENT) {
		ret = hdd_open_ocb_interface(hdd_ctx, rtnl_held);
		if (ret)
			goto err_close_adapters;
	}

	return 0;

err_close_adapters:
	hdd_close_all_adapters(hdd_ctx, rtnl_held);
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
		hdd_ctx->config->TxFlowStopQueueThreshold;
	cds_cfg->tx_flow_start_queue_offset =
		hdd_ctx->config->TxFlowStartQueueOffset;
}
#else
static inline void hdd_txrx_populate_cds_config(struct cds_config_info
						*cds_cfg,
						struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef FEATURE_WLAN_RA_FILTERING
/**
 * hdd_ra_populate_cds_config() - Populate RA filtering cds configuration
 * @cds_cfg: CDS Configuration
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static inline void hdd_ra_populate_cds_config(struct cds_config_info *cds_cfg,
			      struct hdd_context *hdd_ctx)
{
	cds_cfg->ra_ratelimit_interval =
		hdd_ctx->config->RArateLimitInterval;
	cds_cfg->is_ra_ratelimit_enabled =
		hdd_ctx->config->IsRArateLimitEnabled;
}
#else
static inline void hdd_ra_populate_cds_config(struct cds_config_info *cds_cfg,
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

	cds_cfg = (struct cds_config_info *)qdf_mem_malloc(sizeof(*cds_cfg));
	if (!cds_cfg) {
		hdd_err("failed to allocate cds config");
		return -ENOMEM;
	}

	cds_cfg->driver_type = QDF_DRIVER_TYPE_PRODUCTION;
	if (!hdd_ctx->config->nMaxPsPoll ||
			!hdd_ctx->config->enablePowersaveOffload) {
		cds_cfg->powersave_offload_enabled =
			hdd_ctx->config->enablePowersaveOffload;
	} else {
		if ((hdd_ctx->config->enablePowersaveOffload ==
				PS_QPOWER_NODEEPSLEEP) ||
				(hdd_ctx->config->enablePowersaveOffload ==
				 PS_LEGACY_NODEEPSLEEP))
			cds_cfg->powersave_offload_enabled =
				PS_LEGACY_NODEEPSLEEP;
		else
			cds_cfg->powersave_offload_enabled =
				PS_LEGACY_DEEPSLEEP;
		hdd_info("Qpower disabled in cds config, %d",
				cds_cfg->powersave_offload_enabled);
	}
	cds_cfg->sta_dynamic_dtim = hdd_ctx->config->enableDynamicDTIM;
	cds_cfg->sta_mod_dtim = hdd_ctx->config->enableModulatedDTIM;
	cds_cfg->sta_maxlimod_dtim = hdd_ctx->config->fMaxLIModulatedDTIM;
	cds_cfg->wow_enable = hdd_ctx->config->wowEnable;

	/*
	 * Copy the DFS Phyerr Filtering Offload status.
	 * This parameter reflects the value of the
	 * dfs_phyerr_filter_offload flag as set in the ini.
	 */
	cds_cfg->dfs_phyerr_filter_offload =
		hdd_ctx->config->fDfsPhyerrFilterOffload;
	if (hdd_ctx->config->ssdp)
		cds_cfg->ssdp = hdd_ctx->config->ssdp;

	cds_cfg->force_target_assert_enabled =
		hdd_ctx->config->crash_inject_enabled;

	cds_cfg->enable_mc_list = hdd_ctx->config->fEnableMCAddrList;
	cds_cfg->ap_maxoffload_peers = hdd_ctx->config->apMaxOffloadPeers;

	cds_cfg->ap_maxoffload_reorderbuffs =
		hdd_ctx->config->apMaxOffloadReorderBuffs;

	cds_cfg->ap_disable_intrabss_fwd =
		hdd_ctx->config->apDisableIntraBssFwd;

	cds_cfg->dfs_pri_multiplier =
		hdd_ctx->config->dfsRadarPriMultiplier;
	cds_cfg->reorder_offload =
			hdd_ctx->config->reorderOffloadSupport;

	/* IPA micro controller data path offload resource config item */
	cds_cfg->uc_offload_enabled = ucfg_ipa_uc_is_enabled();
	if (!is_power_of_2(hdd_ctx->config->IpaUcTxBufCount)) {
		/* IpaUcTxBufCount should be power of 2 */
		hdd_debug("Round down IpaUcTxBufCount %d to nearest power of 2",
			hdd_ctx->config->IpaUcTxBufCount);
		hdd_ctx->config->IpaUcTxBufCount =
			rounddown_pow_of_two(
				hdd_ctx->config->IpaUcTxBufCount);
		if (!hdd_ctx->config->IpaUcTxBufCount) {
			hdd_err("Failed to round down IpaUcTxBufCount");
			goto exit;
		}
		hdd_debug("IpaUcTxBufCount rounded down to %d",
			hdd_ctx->config->IpaUcTxBufCount);
	}
	cds_cfg->uc_txbuf_count = hdd_ctx->config->IpaUcTxBufCount;
	cds_cfg->uc_txbuf_size = hdd_ctx->config->IpaUcTxBufSize;
	if (!is_power_of_2(hdd_ctx->config->IpaUcRxIndRingCount)) {
		/* IpaUcRxIndRingCount should be power of 2 */
		hdd_debug("Round down IpaUcRxIndRingCount %d to nearest power of 2",
			hdd_ctx->config->IpaUcRxIndRingCount);
		hdd_ctx->config->IpaUcRxIndRingCount =
			rounddown_pow_of_two(
				hdd_ctx->config->IpaUcRxIndRingCount);
		if (!hdd_ctx->config->IpaUcRxIndRingCount) {
			hdd_err("Failed to round down IpaUcRxIndRingCount");
			goto exit;
		}
		hdd_debug("IpaUcRxIndRingCount rounded down to %d",
			hdd_ctx->config->IpaUcRxIndRingCount);
	}
	cds_cfg->uc_rxind_ringcount =
		hdd_ctx->config->IpaUcRxIndRingCount;
	cds_cfg->uc_tx_partition_base =
				hdd_ctx->config->IpaUcTxPartitionBase;
	cds_cfg->max_scan = hdd_ctx->config->max_scan_count;

	cds_cfg->ip_tcp_udp_checksum_offload =
		hdd_ctx->config->enable_ip_tcp_udp_checksum_offload;
	cds_cfg->enable_rxthread = hdd_ctx->enable_rxthread;
	cds_cfg->ce_classify_enabled =
		hdd_ctx->config->ce_classify_enabled;
	cds_cfg->apf_packet_filter_enable =
		hdd_ctx->config->apf_packet_filter_enable;
	cds_cfg->tx_chain_mask_cck = hdd_ctx->config->tx_chain_mask_cck;
	cds_cfg->self_gen_frm_pwr = hdd_ctx->config->self_gen_frm_pwr;
	cds_cfg->max_station = hdd_ctx->config->maxNumberOfPeers;
	cds_cfg->sub_20_channel_width = WLAN_SUB_20_CH_WIDTH_NONE;
	cds_cfg->flow_steering_enabled = hdd_ctx->config->flow_steering_enable;
	cds_cfg->max_msdus_per_rxinorderind =
		hdd_ctx->config->max_msdus_per_rxinorderind;
	cds_cfg->self_recovery_enabled = hdd_ctx->config->enableSelfRecovery;
	cds_cfg->fw_timeout_crash = hdd_ctx->config->fw_timeout_crash;
	cds_cfg->active_uc_apf_mode = hdd_ctx->config->active_uc_apf_mode;
	cds_cfg->active_mc_bc_apf_mode = hdd_ctx->config->active_mc_bc_apf_mode;
	cds_cfg->auto_power_save_fail_mode =
		hdd_ctx->config->auto_pwr_save_fail_mode;

	cds_cfg->ito_repeat_count = hdd_ctx->config->ito_repeat_count;
	cds_cfg->bandcapability = hdd_ctx->config->nBandCapability;
	cds_cfg->delay_before_vdev_stop =
		hdd_ctx->config->delay_before_vdev_stop;

	cds_cfg->num_vdevs = hdd_ctx->config->num_vdevs;
	cds_cfg->enable_peer_unmap_conf_support =
		hdd_ctx->config->enable_peer_unmap_conf_support;
	cds_cfg->bmiss_skip_full_scan = hdd_ctx->config->bmiss_skip_full_scan;

	hdd_ra_populate_cds_config(cds_cfg, hdd_ctx);
	cds_cfg->enable_tx_compl_tsf64 =
		hdd_tsf_is_tsf64_tx_set(hdd_ctx);
	cds_cfg->enable_three_way_coex_config_legacy =
		hdd_ctx->config->enable_three_way_coex_config_legacy;
	hdd_txrx_populate_cds_config(cds_cfg, hdd_ctx);
	hdd_nan_populate_cds_config(cds_cfg, hdd_ctx);

	hdd_lpass_populate_cds_config(cds_cfg, hdd_ctx);
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

	user_config = qdf_mem_malloc(sizeof(*user_config));
	if (user_config == NULL) {
		hdd_alert("Failed to alloc memory for user_config!");
		return -ENOMEM;
	}

	user_config->dot11_mode = hdd_ctx->config->dot11Mode;
	user_config->dual_mac_feature_disable =
		hdd_ctx->config->dual_mac_feature_disable;
	user_config->indoor_channel_support =
		hdd_ctx->config->indoor_channel_support;
	user_config->is_11d_support_enabled =
		hdd_ctx->config->Is11dSupportEnabled;
	user_config->is_11h_support_enabled =
		hdd_ctx->config->Is11hSupportEnabled;
	user_config->optimize_chan_avoid_event =
		hdd_ctx->config->goptimize_chan_avoid_event;
	user_config->skip_dfs_chnl_in_p2p_search =
		hdd_ctx->config->skipDfsChnlInP2pSearch;
	user_config->band_capability = hdd_ctx->config->nBandCapability;
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
	tSmeThermalParams thermal_param;
	QDF_STATUS status;
	mac_handle_t mac_handle;

	thermal_param.smeThermalMgmtEnabled =
		hdd_ctx->config->thermalMitigationEnable;
	thermal_param.smeThrottlePeriod = hdd_ctx->config->throttlePeriod;

	thermal_param.sme_throttle_duty_cycle_tbl[0] =
		hdd_ctx->config->throttle_dutycycle_level0;
	thermal_param.sme_throttle_duty_cycle_tbl[1] =
		hdd_ctx->config->throttle_dutycycle_level1;
	thermal_param.sme_throttle_duty_cycle_tbl[2] =
		hdd_ctx->config->throttle_dutycycle_level2;
	thermal_param.sme_throttle_duty_cycle_tbl[3] =
		hdd_ctx->config->throttle_dutycycle_level3;

	thermal_param.smeThermalLevels[0].smeMinTempThreshold =
		hdd_ctx->config->thermalTempMinLevel0;
	thermal_param.smeThermalLevels[0].smeMaxTempThreshold =
		hdd_ctx->config->thermalTempMaxLevel0;
	thermal_param.smeThermalLevels[1].smeMinTempThreshold =
		hdd_ctx->config->thermalTempMinLevel1;
	thermal_param.smeThermalLevels[1].smeMaxTempThreshold =
		hdd_ctx->config->thermalTempMaxLevel1;
	thermal_param.smeThermalLevels[2].smeMinTempThreshold =
		hdd_ctx->config->thermalTempMinLevel2;
	thermal_param.smeThermalLevels[2].smeMaxTempThreshold =
		hdd_ctx->config->thermalTempMaxLevel2;
	thermal_param.smeThermalLevels[3].smeMinTempThreshold =
		hdd_ctx->config->thermalTempMinLevel3;
	thermal_param.smeThermalLevels[3].smeMaxTempThreshold =
		hdd_ctx->config->thermalTempMaxLevel3;

	mac_handle = hdd_ctx->mac_handle;
	status = sme_init_thermal_info(mac_handle, thermal_param);

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
#define MAX_PKTLOG_SIZE		16

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
			hdd_err("Pktlog buff size is too large. max value is 16MB.\n");
			return -EINVAL;
		}
		return hdd_pktlog_set_buff_size(hdd_ctx, set_value2);
	} else if (set_value == PKTLOG_CLEAR_BUFF) {
		return hdd_pktlog_clear_buff(hdd_ctx);
	}

	/*
	 * set_value = 0 then disable packetlog
	 * set_value = 1 enable packetlog forcefully
	 * set_vlaue = 2 then disable packetlog if disabled through ini or
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
 * @enable: Flag to enable/disable
 * @user_triggered: triggered through iwpriv
 * @size: buffer size to be used for packetlog
 *
 * Return: 0 on success; error number otherwise
 */
int hdd_pktlog_enable_disable(struct hdd_context *hdd_ctx, bool enable,
				uint8_t user_triggered, int size)
{
	struct sir_wifi_start_log start_log;
	QDF_STATUS status;

	start_log.ring_id = RING_ID_PER_PACKET_STATS;
	start_log.verbose_level =
			enable ? WLAN_LOG_LEVEL_ACTIVE : WLAN_LOG_LEVEL_OFF;
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

	if (enable == true)
		hdd_ctx->is_pktlog_enabled = 1;
	else
		hdd_ctx->is_pktlog_enabled = 0;

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
		hdd_debug(MAC_ADDRESS_STR, MAC_ADDR_ARRAY(buf));
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
		hdd_info("provisioned MAC Addr [%d]" MAC_ADDRESS_STR, iter,
			 MAC_ADDR_ARRAY(buf));
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
			hdd_debug("derived MAC Addr [%d]" MAC_ADDRESS_STR, iter,
				  MAC_ADDR_ARRAY(buf));
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
	tSirMacAddr customMacAddr;
	QDF_STATUS status;

	if (hdd_ctx->num_provisioned_addr)
		qdf_mem_copy(&customMacAddr,
			     &hdd_ctx->provisioned_mac_addr[0].bytes[0],
			     sizeof(tSirMacAddr));
	else
		qdf_mem_copy(&customMacAddr,
			     &hdd_ctx->derived_mac_addr[0].bytes[0],
			     sizeof(tSirMacAddr));
	status = sme_set_custom_mac_addr(customMacAddr);
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
	bool update_mac_addr_to_fw = true;

	ret = hdd_platform_wlan_mac(hdd_ctx);
	if (hdd_ctx->config->mac_provision || !ret) {
		hdd_info("using MAC address from platform driver");
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
		update_mac_addr_to_fw = false;
		return 0;
	} else if (hdd_generate_macaddr_auto(hdd_ctx) != 0) {
		struct qdf_mac_addr mac_addr;

		hdd_err("MAC failure from device serial no.");
		cds_rand_get_bytes(0, (uint8_t *)(&mac_addr), sizeof(mac_addr));
		/*
		 * Reset multicast bit (bit-0) and set
		 * locally-administered bit
		 */
		mac_addr.bytes[0] = 0x2;
		hdd_update_macaddr(hdd_ctx, mac_addr, true);
	}

	if (update_mac_addr_to_fw) {
		ret = hdd_update_mac_addr_to_fw(hdd_ctx);
		if (ret)
			hdd_err("MAC address out-of-sync, ret:%d", ret);
	}
	return ret;
}

static int hdd_set_smart_chainmask_enabled(struct hdd_context *hdd_ctx)
{
	int vdev_id = 0;
	int param_id = WMI_PDEV_PARAM_SMART_CHAINMASK_SCHEME;
	int value = hdd_ctx->config->smart_chainmask_enabled;
	int vpdev = PDEV_CMD;
	int ret;

	ret = sme_cli_set_command(vdev_id, param_id, value, vpdev);
	if (ret)
		hdd_err("WMI_PDEV_PARAM_SMART_CHAINMASK_SCHEME failed %d", ret);

	return ret;
}

static int hdd_set_alternative_chainmask_enabled(struct hdd_context *hdd_ctx)
{
	int vdev_id = 0;
	int param_id = WMI_PDEV_PARAM_ALTERNATIVE_CHAINMASK_SCHEME;
	int value = hdd_ctx->config->alternative_chainmask_enabled;
	int vpdev = PDEV_CMD;
	int ret;

	ret = sme_cli_set_command(vdev_id, param_id, value, vpdev);
	if (ret)
		hdd_err("WMI_PDEV_PARAM_ALTERNATIVE_CHAINMASK_SCHEME failed %d",
			ret);

	return ret;
}

static int hdd_set_ani_enabled(struct hdd_context *hdd_ctx)
{
	int vdev_id = 0;
	int param_id = WMI_PDEV_PARAM_ANI_ENABLE;
	int value = hdd_ctx->config->ani_enabled;
	int vpdev = PDEV_CMD;
	int ret;

	ret = sme_cli_set_command(vdev_id, param_id, value, vpdev);
	if (ret)
		hdd_err("WMI_PDEV_PARAM_ANI_ENABLE failed %d", ret);

	return ret;
}

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
	uint8_t max_retry = 0;
	QDF_STATUS status;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	cdp_register_pause_cb(soc, wlan_hdd_txrx_pause_cb);
	/*
	 * Set 802.11p config
	 * TODO-OCB: This has been temporarily added here to ensure this
	 * parameter is set in CSR when we init the channel list. This should
	 * be removed once the 5.9 GHz channels are added to the regulatory
	 * domain.
	 */
	hdd_set_dot11p_config(hdd_ctx);

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

	ret = sme_cli_set_command(0, WMI_PDEV_PARAM_TX_CHAIN_MASK_1SS,
				  hdd_ctx->config->tx_chain_mask_1ss,
				  PDEV_CMD);
	if (0 != ret) {
		hdd_err("WMI_PDEV_PARAM_TX_CHAIN_MASK_1SS failed %d", ret);
		goto out;
	}

	max_retry = hdd_ctx->config->mgmt_retry_max;
	ret = sme_cli_set_command(0, WMI_PDEV_PARAM_MGMT_RETRY_LIMIT, max_retry,
				  PDEV_CMD);
	if (0 != ret) {
		hdd_err("WMI_PDEV_PARAM_MGMT_RETRY_LIMIT failed %d", ret);
		goto out;
	}

	ret = hdd_set_smart_chainmask_enabled(hdd_ctx);
	if (ret)
		goto out;

	ret = hdd_set_alternative_chainmask_enabled(hdd_ctx);
	if (ret)
		goto out;

	ret = hdd_set_ani_enabled(hdd_ctx);
	if (ret)
		goto out;

	ret = sme_cli_set_command(0, WMI_PDEV_PARAM_ARP_AC_OVERRIDE,
				  hdd_ctx->config->arp_ac_category,
				  PDEV_CMD);
	if (0 != ret) {
		hdd_err("WMI_PDEV_PARAM_ARP_AC_OVERRIDE ac: %d ret: %d",
			hdd_ctx->config->arp_ac_category, ret);
		goto out;
	}

	status = hdd_set_sme_chan_list(hdd_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to init channel list: %d", status);
		ret = qdf_status_to_os_return(status);
		goto out;
	}

	/* Apply the cfg.ini to cfg.dat */
	if (!hdd_update_config_cfg(hdd_ctx)) {
		hdd_err("config update failed");
		ret = -EINVAL;
		goto out;
	}


	hdd_init_channel_avoidance(hdd_ctx);

	/* update enable sap mandatory chan list */
	policy_mgr_enable_disable_sap_mandatory_chan_list(hdd_ctx->psoc,
			hdd_ctx->config->enable_sap_mandatory_chan_list);
out:
	return ret;
}

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

	hdd_enter();

	if (hdd_ctx == NULL) {
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
		cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
			&(adapter->wdev), sizeof(uint32_t) + NLMSG_HDRLEN,
			QCA_NL80211_VENDOR_SUBCMD_P2P_LO_EVENT_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_STOP_REASON,
			evt->reason_code)) {
		hdd_err("nla put failed");
		kfree_skb(vendor_event);
		return;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	hdd_debug("Sent P2P_LISTEN_OFFLOAD_STOP event for vdev_id = %d",
			evt->vdev_id);
}

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

	dwelltime_params.is_enabled =
			hdd_ctx->config->adaptive_dwell_mode_enabled;
	dwelltime_params.dwelltime_mode =
			hdd_ctx->config->global_adapt_dwelltime_mode;
	dwelltime_params.lpf_weight =
			hdd_ctx->config->adapt_dwell_lpf_weight;
	dwelltime_params.passive_mon_intval =
			hdd_ctx->config->adapt_dwell_passive_mon_intval;
	dwelltime_params.wifi_act_threshold =
			hdd_ctx->config->adapt_dwell_wifi_act_threshold;

	status = sme_set_adaptive_dwelltime_config(hdd_ctx->mac_handle,
						   &dwelltime_params);

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
	uint8_t dbs_scan_config[CDS_DBS_SCAN_PARAM_PER_CLIENT
				* CDS_DBS_SCAN_CLIENTS_MAX];

	/* check if DBS is enabled or supported */
	if ((hdd_ctx->config->dual_mac_feature_disable ==
	     DISABLE_DBS_CXN_AND_SCAN) ||
	    (hdd_ctx->config->dual_mac_feature_disable ==
	     ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN))
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

	if (!hdd_ctx->config->WlanAutoShutdown)
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
/**
 * hdd_set_mws_coex() - Set MWS coex configurations
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

	ret = sme_cli_set_command(0, WMI_PDEV_PARAM_MWSCOEX_4G_ALLOW_QUICK_FTDM,
				  hdd_ctx->config->mws_coex_4g_quick_tdm,
				  PDEV_CMD);
	if (ret) {
		hdd_warn("Unable to send MWS-COEX 4G quick FTDM policy");
		return ret;
	}

	ret = sme_cli_set_command(0, WMI_PDEV_PARAM_MWSCOEX_SET_5GNR_PWR_LIMIT,
				  hdd_ctx->config->mws_coex_5g_nr_pwr_limit,
				  PDEV_CMD);
	if (ret) {
		hdd_warn("Unable to send MWS-COEX 4G quick FTDM policy");
		return ret;
	}

	ret = sme_cli_set_command(
			0, WMI_PDEV_PARAM_MWSCOEX_PCC_CHAVD_DELAY,
			hdd_ctx->config->mws_coex_pcc_channel_avoid_delay,
			PDEV_CMD);
	if (ret)
		return ret;

	ret = sme_cli_set_command(
			0, WMI_PDEV_PARAM_MWSCOEX_SCC_CHAVD_DELAY,
			hdd_ctx->config->mws_coex_scc_channel_avoid_delay,
			PDEV_CMD);
	if (ret)
		return ret;

	return ret;
}
#else
static int hdd_init_mws_coex(struct hdd_context *hdd_ctx)
{
	return 0;
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
	tSirTxPowerLimit hddtxlimit;
	QDF_STATUS status;
	struct sme_5g_band_pref_params band_pref_params;
	int ret;
	mac_handle_t mac_handle;
	struct hdd_config *cfg;

	hdd_enter();

	ret = hdd_update_country_code(hdd_ctx);
	if (ret) {
		hdd_err("Failed to update country code; errno:%d", ret);
		return -EINVAL;
	}

	ret = hdd_init_mws_coex(hdd_ctx);
	if (ret)
		hdd_warn("Error initializing mws-coex");

	cfg = hdd_ctx->config;
	/* FW capabilities received, Set the Dot11 mode */
	mac_handle = hdd_ctx->mac_handle;
	sme_setdef_dot11mode(mac_handle);
	sme_set_prefer_80MHz_over_160MHz(mac_handle,
			hdd_ctx->config->sta_prefer_80MHz_over_160MHz);
	sme_set_etsi13_srd_ch_in_master_mode(mac_handle,
					     cfg->
					     etsi13_srd_chan_in_master_mode);

	if (hdd_ctx->config->fIsImpsEnabled)
		hdd_set_idle_ps_config(hdd_ctx, true);
	else
		hdd_set_idle_ps_config(hdd_ctx, false);

	/* Send Enable/Disable data stall detection cmd to FW */
	sme_cli_set_command(0, WMI_PDEV_PARAM_DATA_STALL_DETECT_ENABLE,
	hdd_ctx->config->enable_data_stall_det, PDEV_CMD);

	if (hdd_ctx->config->enable_go_cts2self_for_sta)
		sme_set_cts2self_for_p2p_go(mac_handle);

	if (sme_set_vc_mode_config(hdd_ctx->config->vc_mode_cfg_bitmap))
		hdd_warn("Error in setting Voltage Corner mode config to FW");

	if (hdd_rx_ol_init(hdd_ctx))
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
	if (cds_is_packet_log_enabled() ||
	    (cds_is_driver_recovering() && hdd_ctx->is_pktlog_enabled))
		hdd_pktlog_enable_disable(hdd_ctx, true, 0, 0);

	hddtxlimit.txPower2g = hdd_ctx->config->TxPower2g;
	hddtxlimit.txPower5g = hdd_ctx->config->TxPower5g;
	status = sme_txpower_limit(mac_handle, &hddtxlimit);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Error setting txlimit in sme: %d", status);

	wlan_hdd_tsf_init(hdd_ctx);

	if (hdd_ctx->config->goptimize_chan_avoid_event) {
		status = sme_enable_disable_chanavoidind_event(
							mac_handle, 0);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("Failed to disable Chan Avoidance Indication");
			return -EINVAL;
		}
	}

	if (hdd_ctx->config->enable_5g_band_pref) {
		band_pref_params.rssi_boost_threshold_5g =
				hdd_ctx->config->rssi_boost_threshold_5g;
		band_pref_params.rssi_boost_factor_5g =
				hdd_ctx->config->rssi_boost_factor_5g;
		band_pref_params.max_rssi_boost_5g =
				hdd_ctx->config->max_rssi_boost_5g;
		band_pref_params.rssi_penalize_threshold_5g =
				hdd_ctx->config->rssi_penalize_threshold_5g;
		band_pref_params.rssi_penalize_factor_5g =
				hdd_ctx->config->rssi_penalize_factor_5g;
		band_pref_params.max_rssi_penalize_5g =
				hdd_ctx->config->max_rssi_penalize_5g;
		sme_set_5g_band_pref(mac_handle, &band_pref_params);
	}

	/* register P2P Listen Offload event callback */
	if (wma_is_p2p_lo_capable())
		sme_register_p2p_lo_event(mac_handle, hdd_ctx,
					  wlan_hdd_p2p_lo_event_callback);

	ret = hdd_set_auto_shutdown_cb(hdd_ctx);

	if (ret)
		return -EINVAL;

	wlan_hdd_init_chan_info(hdd_ctx);
	wlan_hdd_twt_init(hdd_ctx);

	hdd_exit();
	return 0;
}

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
	wlan_hdd_twt_deinit(hdd_ctx);
	wlan_hdd_deinit_chan_info(hdd_ctx);
	wlan_hdd_tsf_deinit(hdd_ctx);
	if (cds_is_packet_log_enabled())
		hdd_pktlog_enable_disable(hdd_ctx, false, 0, 0);
}

/**
 * hdd_register_bcn_cb() - register scan beacon callback
 * @hdd_ctx - Pointer to the HDD context
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
				   cds_get_context(QDF_MODULE_ID_TXRX),
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
			    cds_get_context(QDF_MODULE_ID_TXRX), vdev_id);
}

/**
 * hdd_action_oui_config() - Configure action_oui strings
 * @hdd_ctx: pointer to hdd context
 *
 * This is a HDD wrapper function which invokes ucfg api
 * of action_oui component to parse action oui strings.
 *
 * Return: None
 */
static void hdd_action_oui_config(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	uint32_t id;
	uint8_t *str;

	if (!hdd_ctx->config->action_oui_enable)
		return;

	for (id = 0; id < ACTION_OUI_MAXIMUM_ID; id++) {
		str = hdd_ctx->config->action_oui_str[id];
		if (!qdf_str_len(str))
			continue;

		status = ucfg_action_oui_parse(hdd_ctx->psoc, str, id);
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("Failed to parse action_oui str: %u", id);
	}
}

/**
 * hdd_action_oui_send() - Send action_oui extensions to firmware
 * @hdd_ctx: pointer to hdd context
 *
 * This is a HDD wrapper function which invokes ucfg api
 * of action_oui component to send action oui extensions to firmware.
 *
 * Return: None
 */
static void hdd_action_oui_send(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	if (!hdd_ctx->config->action_oui_enable)
		return;

	status = ucfg_action_oui_send(hdd_ctx->psoc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to send one or all action_ouis");
}

/**
 * hdd_configure_cds() - Configure cds modules
 * @hdd_ctx:	HDD context
 * @adapter:	Primary adapter context
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
	uint32_t num_abg_tx_chains = 0;
	uint32_t num_11b_tx_chains = 0;
	uint32_t num_11ag_tx_chains = 0;
	struct policy_mgr_dp_cbacks dp_cbs = {0};
	qdf_device_t qdf_ctx;

	mac_handle = hdd_ctx->mac_handle;

	hdd_action_oui_send(hdd_ctx);

	if (hdd_ctx->config->is_force_1x1_enable)
		sme_cli_set_command(0, (int)WMI_PDEV_PARAM_SET_IOT_PATTERN,
				1, PDEV_CMD);
	/* set chip power save failure detected callback */
	sme_set_chip_pwr_save_fail_cb(mac_handle,
				      hdd_chip_pwr_save_fail_detected_cb);

	if (hdd_ctx->config->max_mpdus_inampdu) {
		set_value = hdd_ctx->config->max_mpdus_inampdu;
		sme_cli_set_command(0, (int)WMI_PDEV_PARAM_MAX_MPDUS_IN_AMPDU,
				    set_value, PDEV_CMD);
	}

	if (hdd_ctx->config->enable_rts_sifsbursting) {
		set_value = hdd_ctx->config->enable_rts_sifsbursting;
		sme_cli_set_command(0,
				    (int)WMI_PDEV_PARAM_ENABLE_RTS_SIFS_BURSTING,
				    set_value, PDEV_CMD);
	}

	if (hdd_ctx->config->sap_get_peer_info) {
		set_value = hdd_ctx->config->sap_get_peer_info;
		sme_cli_set_command(0,
				    (int)WMI_PDEV_PARAM_PEER_STATS_INFO_ENABLE,
				    set_value, PDEV_CMD);
	}

	num_11b_tx_chains = hdd_ctx->config->num_11b_tx_chains;
	num_11ag_tx_chains = hdd_ctx->config->num_11ag_tx_chains;
	if (!hdd_ctx->config->enable2x2) {
		if (num_11b_tx_chains > 1)
			num_11b_tx_chains = 1;
		if (num_11ag_tx_chains > 1)
			num_11ag_tx_chains = 1;
	}
	WMI_PDEV_PARAM_SET_11B_TX_CHAIN_NUM(num_abg_tx_chains,
					    num_11b_tx_chains);
	WMI_PDEV_PARAM_SET_11AG_TX_CHAIN_NUM(num_abg_tx_chains,
					     num_11ag_tx_chains);
	sme_cli_set_command(0, (int)WMI_PDEV_PARAM_ABG_MODE_TX_CHAIN_NUM,
			    num_abg_tx_chains, PDEV_CMD);

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

	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_ctx) {
		hdd_err("QDF device context is NULL");
		goto out;
	}

	if (ucfg_ipa_uc_ol_init(hdd_ctx->pdev, qdf_ctx)) {
		hdd_err("Failed to setup pipes");
		goto out;
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

	if (hdd_ctx->ol_enable)
		dp_cbs.hdd_disable_rx_ol_in_concurrency =
				hdd_disable_rx_ol_in_concurrency;
	dp_cbs.hdd_set_rx_mode_rps_cb = hdd_set_rx_mode_rps;
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

	if (0 != wlan_hdd_set_wow_pulse(hdd_ctx, true))
		hdd_debug("Failed to set wow pulse");

	sme_cli_set_command(0, WMI_PDEV_PARAM_GCMP_SUPPORT_ENABLE,
			    hdd_ctx->config->gcmp_enabled, PDEV_CMD);
	sme_cli_set_command(0, WMI_PDEV_AUTO_DETECT_POWER_FAILURE,
			    hdd_ctx->config->auto_pwr_save_fail_mode, PDEV_CMD);


	if (hdd_ctx->config->enable_phy_reg_retention)
		wma_cli_set_command(0, WMI_PDEV_PARAM_FAST_PWR_TRANSITION,
			hdd_ctx->config->enable_phy_reg_retention, PDEV_CMD);

	wlan_hdd_hang_event_notifier_register(hdd_ctx);
	return 0;

cds_disable:
	cds_disable(hdd_ctx->psoc);

out:
	return -EINVAL;
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
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_debug("Failed to deregister mode change cb with Policy Manager");
	}

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

/**
 * hdd_wlan_stop_modules - Single driver state machine for stoping modules
 * @hdd_ctx: HDD context
 * @ftm_mode: ftm mode
 *
 * This function maintains the driver state machine it will be invoked from
 * exit, shutdown and con_mode change handler. Depending on the driver state
 * shall perform the stopping/closing of the modules.
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_wlan_stop_modules(struct hdd_context *hdd_ctx, bool ftm_mode)
{
	void *hif_ctx;
	qdf_device_t qdf_ctx;
	QDF_STATUS qdf_status;
	bool is_recovery_stop = cds_is_driver_recovering();
	int ret = 0;
	int active_threads;
	int debugfs_threads;
	struct target_psoc_info *tgt_hdl;

	hdd_enter();
	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_ctx) {
		hdd_err("QDF device context NULL");
		return -EINVAL;
	}

	mutex_lock(&hdd_ctx->iface_change_lock);
	hdd_ctx->stop_modules_in_progress = true;
	cds_set_module_stop_in_progress(true);

	active_threads = cds_return_external_threads_count();
	debugfs_threads = hdd_return_debugfs_threads_count();

	if (active_threads > 0 || debugfs_threads > 0 ||
	    hdd_ctx->is_wiphy_suspended) {
		hdd_warn("External threads %d, Debugfs threads %d, wiphy suspend %d",
			 active_threads, debugfs_threads,
			 hdd_ctx->is_wiphy_suspended);

		if (active_threads)
			cds_print_external_threads();

		if (IS_IDLE_STOP && !ftm_mode) {
			mutex_unlock(&hdd_ctx->iface_change_lock);
			hdd_psoc_idle_timer_start(hdd_ctx);
			hdd_ctx->stop_modules_in_progress = false;
			cds_set_module_stop_in_progress(false);

			return 0;
		}
	}

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

		if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE)
			break;

		wlan_hdd_deinit_tx_rx_histogram(hdd_ctx);

		hdd_disable_power_management();
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

	hdd_sysfs_destroy_powerstats_interface();
	hdd_sysfs_destroy_version_interface();
	hdd_sysfs_destroy_driver_root_obj();
	hdd_debug("Closing CDS modules!");

	qdf_status = cds_post_disable();
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Failed to process post CDS disable Modules! :%d",
			qdf_status);
		ret = -EINVAL;
		QDF_ASSERT(0);
	}

	/* De-register the SME callbacks */
	hdd_deregister_cb(hdd_ctx);

	hdd_runtime_suspend_context_deinit(hdd_ctx);

	qdf_status = cds_dp_close(hdd_ctx->psoc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_warn("Failed to stop CDS DP: %d", qdf_status);
		ret = -EINVAL;
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
		hdd_err("WBUFF de-init unsuccessful; status: %d", qdf_status);

	dispatcher_pdev_close(hdd_ctx->pdev);
	ret = hdd_objmgr_release_and_destroy_pdev(hdd_ctx);
	if (ret) {
		hdd_err("Failed to destroy pdev; errno:%d", ret);
		QDF_ASSERT(0);
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
	if (!hif_ctx) {
		hdd_err("Hif context is Null");
		ret = -EINVAL;
	}

	if (hdd_ctx->target_hw_name) {
		qdf_mem_free(hdd_ctx->target_hw_name);
		hdd_ctx->target_hw_name = NULL;
	}

	hdd_hif_close(hdd_ctx, hif_ctx);

	ol_cds_free();

	if (IS_IDLE_STOP && cds_is_target_ready()) {
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

	hdd_check_for_leaks(hdd_ctx, is_recovery_stop);
	hdd_debug_domain_set(QDF_DEBUG_DOMAIN_INIT);

	/* Once the firmware sequence is completed reset this flag */
	hdd_ctx->imps_enabled = false;
	hdd_ctx->driver_status = DRIVER_MODULES_CLOSED;
	hdd_debug("Wlan transitioned (now CLOSED)");

done:
	hdd_ctx->stop_modules_in_progress = false;
	cds_set_module_stop_in_progress(false);
	mutex_unlock(&hdd_ctx->iface_change_lock);

	hdd_exit();

	return ret;
}


#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
/**
 * hdd_state_info_dump() - prints state information of hdd layer
 * @buf: buffer pointer
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
	struct hdd_adapter *adapter;
	uint16_t len = 0;
	char *buf = *buf_ptr;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Failed to get hdd context ");
		return;
	}

	hdd_debug("size of buffer: %d", *size);

	len += scnprintf(buf + len, *size - len,
		"\n is_wiphy_suspended %d", hdd_ctx->is_wiphy_suspended);
	len += scnprintf(buf + len, *size - len,
		"\n is_scheduler_suspended %d",
		hdd_ctx->is_scheduler_suspended);

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter->dev)
			len += scnprintf(buf + len, *size - len,
				"\n device name: %s", adapter->dev->name);
		len += scnprintf(buf + len, *size - len,
				"\n device_mode: %d", adapter->device_mode);
		switch (adapter->device_mode) {
		case QDF_STA_MODE:
		case QDF_P2P_CLIENT_MODE:
			hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			len += scnprintf(buf + len, *size - len,
				"\n connState: %d",
				hdd_sta_ctx->conn_info.connState);
			break;

		default:
			break;
		}
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

#ifdef CONFIG_DP_TRACE
void hdd_dp_trace_init(struct hdd_config *config)
{
	bool live_mode = DP_TRACE_CONFIG_DEFAULT_LIVE_MODE;
	uint8_t thresh = DP_TRACE_CONFIG_DEFAULT_THRESH;
	uint16_t thresh_time_limit = DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT;
	uint8_t verbosity = DP_TRACE_CONFIG_DEFAULT_VERBOSTY;
	uint8_t proto_bitmap = DP_TRACE_CONFIG_DEFAULT_BITMAP;
	uint8_t config_params[DP_TRACE_CONFIG_NUM_PARAMS];
	uint8_t num_entries = 0;
	uint32_t bw_compute_interval;

	qdf_dp_set_proto_event_bitmap(config->dp_proto_event_bitmap);

	if (!config->enable_dp_trace) {
		hdd_err("dp trace is disabled from ini");
		return;
	}

	hdd_string_to_u8_array(config->dp_trace_config, config_params,
				&num_entries, sizeof(config_params));

	/* calculating, num bw timer intervals in a second (1000ms) */
	bw_compute_interval = GET_BW_COMPUTE_INTV(config);
	if (bw_compute_interval <= 1000 && bw_compute_interval > 0)
		thresh_time_limit = 1000 / bw_compute_interval;
	else if (bw_compute_interval > 1000) {
		hdd_err("busBandwidthComputeInterval > 1000, using 1000");
		thresh_time_limit = 1;
	} else
		hdd_err("busBandwidthComputeInterval is 0, using defaults");

	switch (num_entries) {
	case 4:
		proto_bitmap = config_params[3];
		/* fall through */
	case 3:
		verbosity = config_params[2];
		/* fall through */
	case 2:
		thresh = config_params[1];
		/* fall through */
	case 1:
		live_mode = config_params[0];
		/* fall through */
	default:
		hdd_debug("live_mode %u thresh %u time_limit %u verbosity %u bitmap 0x%x",
			  live_mode, thresh, thresh_time_limit,
			  verbosity, proto_bitmap);
	};

	qdf_dp_trace_init(live_mode, thresh, thresh_time_limit,
			verbosity, proto_bitmap);

}
#endif

#ifdef DISABLE_CHANNEL_LIST
static int wlan_hdd_cache_chann_mutex_create(struct hdd_context *hdd_ctx)
{
	return qdf_mutex_create(&hdd_ctx->cache_channel_lock);
}
#else
static int wlan_hdd_cache_chann_mutex_create(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

/**
 * hdd_wlan_startup() - HDD init function
 * @dev:	Pointer to the underlying device
 *
 * This is the driver startup code executed once a WLAN device has been detected
 *
 * Return:  0 for success, < 0 for failure
 */
int hdd_wlan_startup(struct device *dev)
{
	QDF_STATUS status;
	struct hdd_context *hdd_ctx;
	int ret;
	bool rtnl_held;
	mac_handle_t mac_handle;

	hdd_enter();

	hdd_ctx = hdd_context_create(dev);

	if (IS_ERR(hdd_ctx))
		return PTR_ERR(hdd_ctx);

	ret = hdd_objmgr_create_and_store_psoc(hdd_ctx,
					   DEFAULT_PSOC_ID);
	if (ret) {
		hdd_err("Psoc creation fails!");
		QDF_BUG(0);
		goto err_hdd_free_context;
	}

	hdd_action_oui_config(hdd_ctx);

	qdf_nbuf_init_replenish_timer();

	ret = wlan_hdd_cache_chann_mutex_create(hdd_ctx);
	if (QDF_IS_STATUS_ERROR(ret))
		goto err_hdd_free_context;

#ifdef FEATURE_WLAN_CH_AVOID
	mutex_init(&hdd_ctx->avoid_freq_lock);
#endif

	osif_request_manager_init();
	qdf_atomic_init(&hdd_ctx->con_mode_flag);
	hdd_driver_memdump_init();
	hdd_bus_bandwidth_init(hdd_ctx);

	ret = hdd_wlan_start_modules(hdd_ctx, false);
	if (ret) {
		hdd_err("Failed to start modules: %d", ret);
		goto err_memdump_deinit;
	}

	wlan_hdd_update_wiphy(hdd_ctx);

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	hdd_ctx->mac_handle = mac_handle;
	if (!mac_handle) {
		hdd_err("Mac Handle is null");
		goto err_stop_modules;
	}

	ret = hdd_wiphy_init(hdd_ctx);
	if (ret) {
		hdd_err("Failed to initialize wiphy: %d", ret);
		goto err_stop_modules;
	}

	hdd_dp_trace_init(hdd_ctx->config);

	ret = hdd_initialize_mac_address(hdd_ctx);
	if (ret) {
		hdd_err("MAC initializtion failed: %d", ret);
		goto err_wiphy_unregister;
	}

	ret = register_netdevice_notifier(&hdd_netdev_notifier);
	if (ret) {
		hdd_err("register_netdevice_notifier failed: %d", ret);
		goto err_wiphy_unregister;
	}

	rtnl_held = hdd_hold_rtnl_lock();

	ret = hdd_open_interfaces(hdd_ctx, rtnl_held);
	if (ret) {
		hdd_err("Failed to open interfaces: %d", ret);
		goto err_release_rtnl_lock;
	}

	hdd_release_rtnl_lock();
	rtnl_held = false;

	wlan_hdd_update_11n_mode(hdd_ctx->config);

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	status = qdf_mc_timer_init(&hdd_ctx->skip_acs_scan_timer,
				   QDF_TIMER_TYPE_SW,
				   hdd_skip_acs_scan_timer_handler,
				   (void *)hdd_ctx);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to init ACS Skip timer");
	qdf_spinlock_create(&hdd_ctx->acs_skip_lock);
#endif

	hdd_lpass_notify_wlan_version(hdd_ctx);

	if (hdd_ctx->rps)
		hdd_set_rps_cpu_mask(hdd_ctx);

	ret = hdd_register_notifiers(hdd_ctx);
	if (ret)
		goto err_close_adapters;

	status = wlansap_global_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_unregister_notifiers(hdd_ctx);
		goto err_close_adapters;
	}

	if (hdd_ctx->config->fIsImpsEnabled)
		hdd_set_idle_ps_config(hdd_ctx, true);
	else
		hdd_set_idle_ps_config(hdd_ctx, false);

	if (QDF_GLOBAL_FTM_MODE != hdd_get_conparam())
		hdd_psoc_idle_timer_start(hdd_ctx);

	hdd_debugfs_mws_coex_info_init(hdd_ctx);
	goto success;

err_close_adapters:
	hdd_close_all_adapters(hdd_ctx, rtnl_held);

err_release_rtnl_lock:
	if (rtnl_held)
		hdd_release_rtnl_lock();

	unregister_netdevice_notifier(&hdd_netdev_notifier);

err_wiphy_unregister:
	wiphy_unregister(hdd_ctx->wiphy);

err_stop_modules:
	hdd_wlan_stop_modules(hdd_ctx, false);

err_memdump_deinit:
	hdd_bus_bandwidth_deinit(hdd_ctx);
	hdd_driver_memdump_deinit();

	osif_request_manager_deinit();
	hdd_exit_netlink_services(hdd_ctx);

	hdd_objmgr_release_and_destroy_psoc(hdd_ctx);

err_hdd_free_context:
	if (cds_is_fw_down())
		hdd_err("Not setting the complete event as fw is down");
	else
		hdd_start_complete(ret);

	qdf_nbuf_deinit_replenish_timer();
	hdd_context_destroy(hdd_ctx);
	return ret;

success:
	hdd_exit();
	return 0;
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

void hdd_get_nud_stats_cb(void *data, struct rsp_stats *rsp, void *context)
{
	struct hdd_context *hdd_ctx = (struct hdd_context *)data;
	int status;
	struct hdd_adapter *adapter = NULL;
	struct osif_request *request = NULL;

	hdd_enter();

	if (!rsp) {
		hdd_err("data is null");
		return;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status != 0)
		return;

	request = osif_request_get(context);
	if (!request) {
		hdd_err("obselete request");
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, rsp->vdev_id);
	if ((NULL == adapter) || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		hdd_err("Invalid adapter or adapter has invalid magic");
		osif_request_put(request);
		return;
	}

	hdd_debug("rsp->arp_req_enqueue :%x", rsp->arp_req_enqueue);
	hdd_debug("rsp->arp_req_tx_success :%x", rsp->arp_req_tx_success);
	hdd_debug("rsp->arp_req_tx_failure :%x", rsp->arp_req_tx_failure);
	hdd_debug("rsp->arp_rsp_recvd :%x", rsp->arp_rsp_recvd);
	hdd_debug("rsp->out_of_order_arp_rsp_drop_cnt :%x",
		  rsp->out_of_order_arp_rsp_drop_cnt);
	hdd_debug("rsp->dad_detected :%x", rsp->dad_detected);
	hdd_debug("rsp->connect_status :%x", rsp->connect_status);
	hdd_debug("rsp->ba_session_establishment_status :%x",
		  rsp->ba_session_establishment_status);

	adapter->hdd_stats.hdd_arp_stats.rx_fw_cnt = rsp->arp_rsp_recvd;
	adapter->dad |= rsp->dad_detected;
	adapter->con_status = rsp->connect_status;

	/* Flag true indicates connectivity check stats present. */
	if (rsp->connect_stats_present) {
		hdd_debug("rsp->tcp_ack_recvd :%x", rsp->tcp_ack_recvd);
		hdd_debug("rsp->icmpv4_rsp_recvd :%x", rsp->icmpv4_rsp_recvd);
		adapter->hdd_stats.hdd_tcp_stats.rx_fw_cnt = rsp->tcp_ack_recvd;
		adapter->hdd_stats.hdd_icmpv4_stats.rx_fw_cnt =
							rsp->icmpv4_rsp_recvd;
	}

	osif_request_complete(request);
	osif_request_put(request);

	hdd_exit();
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
	sme_nan_register_callback(mac_handle,
				  wlan_hdd_cfg80211_nan_callback);
	sme_stats_ext_register_callback(mac_handle,
					wlan_hdd_cfg80211_stats_ext_callback);

	sme_ext_scan_register_callback(mac_handle,
					wlan_hdd_cfg80211_extscan_callback);
	sme_stats_ext2_register_callback(mac_handle,
					wlan_hdd_cfg80211_stats_ext2_callback);

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
	/* print error and not block the startup process */
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("set lost link info callback failed");

	wlan_hdd_register_cp_stats_cb(hdd_ctx);

	ret = hdd_register_data_stall_detect_cb();
	if (ret) {
		hdd_err("Register data stall detect detect callback failed.");
		return ret;
	}

	wlan_hdd_dcc_register_for_dcc_stats_event(hdd_ctx);

	sme_register_set_connection_info_cb(mac_handle,
					    hdd_set_connection_in_progress,
					    hdd_is_connection_in_progress);

	status = sme_congestion_register_callback(mac_handle,
						  hdd_update_cca_info_cb);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("set congestion callback failed");

	status = sme_set_bt_activity_info_cb(mac_handle,
					     hdd_bt_activity_cb);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("set bt activity info callback failed");

	status = sme_register_tx_queue_cb(mac_handle,
					  hdd_tx_queue_cb);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Register tx queue callback failed");

	sme_set_roam_scan_ch_event_cb(mac_handle, hdd_get_roam_scan_ch_cb);

	status = sme_set_beacon_latency_event_cb(mac_handle,
						 hdd_beacon_latency_event_cb);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err_rl("Register beacon latency event callback failed");


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

	sme_deregister_tx_queue_cb(mac_handle);

	sme_reset_link_layer_stats_ind_cb(mac_handle);
	sme_reset_rssi_threshold_breached_cb(mac_handle);

	sme_stats_ext_register_callback(mac_handle,
					wlan_hdd_cfg80211_stats_ext_callback);

	sme_nan_deregister_callback(mac_handle);
	status = sme_reset_tsfcb(mac_handle);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to de-register tsfcb the callback:%d",
			status);

	ret = hdd_deregister_data_stall_detect_cb();
	if (ret)
		hdd_err("Failed to de-register data stall detect event callback");

	sme_deregister_oem_data_rsp_callback(mac_handle);

	hdd_exit();
}

/**
 * hdd_softap_sta_deauth() - handle deauth req from HDD
 * @adapter:	Pointer to the HDD
 * @enable:	bool value
 *
 * This to take counter measure to handle deauth req from HDD
 *
 * Return: None
 */
QDF_STATUS hdd_softap_sta_deauth(struct hdd_adapter *adapter,
				 struct csr_del_sta_params *pDelStaParams)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAULT;

	hdd_enter();

	/* Ignore request to deauth bcmc station */
	if (pDelStaParams->peerMacAddr.bytes[0] & 0x1)
		return qdf_status;

	qdf_status =
		wlansap_deauth_sta(WLAN_HDD_GET_SAP_CTX_PTR(adapter),
				   pDelStaParams);

	hdd_exit();
	return qdf_status;
}

/**
 * hdd_softap_sta_disassoc() - take counter measure to handle deauth req from HDD
 * @adapter:	Pointer to the HDD
 * @p_del_sta_params: pointer to station deletion parameters
 *
 * This to take counter measure to handle deauth req from HDD
 *
 * Return: None
 */
void hdd_softap_sta_disassoc(struct hdd_adapter *adapter,
			     struct csr_del_sta_params *pDelStaParams)
{
	hdd_enter();

	/* Ignore request to disassoc bcmc station */
	if (pDelStaParams->peerMacAddr.bytes[0] & 0x1)
		return;

	wlansap_disassoc_sta(WLAN_HDD_GET_SAP_CTX_PTR(adapter),
			     pDelStaParams);
}

/**
 * hdd_issta_p2p_clientconnected() - check if sta or p2p client is connected
 * @hdd_ctx:	HDD Context
 *
 * API to find if there is any STA or P2P-Client is connected
 *
 * Return: true if connected; false otherwise
 */
QDF_STATUS hdd_issta_p2p_clientconnected(struct hdd_context *hdd_ctx)
{
	return sme_is_sta_p2p_client_connected(hdd_ctx->mac_handle);
}

void wlan_hdd_disable_roaming(struct hdd_adapter *cur_adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(cur_adapter);
	struct hdd_adapter *adapter = NULL;
	struct csr_roam_profile *roam_profile;
	struct hdd_station_ctx *sta_ctx;

	if (!policy_mgr_is_sta_active_connection_exists(hdd_ctx->psoc))
		return;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		roam_profile = hdd_roam_profile(adapter);
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

		if (cur_adapter->session_id != adapter->session_id &&
		    adapter->device_mode == QDF_STA_MODE &&
		    hdd_conn_is_connected(sta_ctx)) {
			hdd_debug("%d Disable roaming", adapter->session_id);
			sme_stop_roaming(hdd_ctx->mac_handle,
					 adapter->session_id,
					 ecsr_driver_disabled);
		}
	}
}

void wlan_hdd_enable_roaming(struct hdd_adapter *cur_adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(cur_adapter);
	struct hdd_adapter *adapter = NULL;
	struct csr_roam_profile *roam_profile;
	struct hdd_station_ctx *sta_ctx;

	if (!policy_mgr_is_sta_active_connection_exists(hdd_ctx->psoc))
		return;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		roam_profile = hdd_roam_profile(adapter);
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

		if (cur_adapter->session_id != adapter->session_id &&
		    adapter->device_mode == QDF_STA_MODE &&
		    hdd_conn_is_connected(sta_ctx)) {
			hdd_debug("%d Enable roaming", adapter->session_id);
			sme_start_roaming(hdd_ctx->mac_handle,
					  adapter->session_id,
					  REASON_DRIVER_ENABLED);
		}
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

	if (skb == NULL)
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
	struct hdd_adapter *adapter;
	bool ap_connected = false, sta_connected = false;
	mac_handle_t mac_handle;

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle)
		return;

	if (hdd_ctx->config->WlanAutoShutdown == 0)
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
		hdd_for_each_adapter(hdd_ctx, adapter) {
			if (adapter->device_mode == QDF_STA_MODE) {
				if (WLAN_HDD_GET_STATION_CTX_PTR(adapter)->
				    conn_info.connState ==
				    eConnectionState_Associated) {
					sta_connected = true;
					break;
				}
			}

			if (adapter->device_mode == QDF_SAP_MODE) {
				if (WLAN_HDD_GET_AP_CTX_PTR(adapter)->
				    ap_active == true) {
					ap_connected = true;
					break;
				}
			}
		}
	}

	if (ap_connected == true || sta_connected == true) {
		hdd_debug("CC Session active. Shutdown timer not enabled");
		return;
	}

	if (sme_set_auto_shutdown_timer(mac_handle,
					hdd_ctx->config->WlanAutoShutdown)
	    != QDF_STATUS_SUCCESS)
		hdd_err("Failed to start wlan auto shutdown timer");
	else
		hdd_info("Auto Shutdown timer for %d seconds enabled",
			 hdd_ctx->config->WlanAutoShutdown);
}
#endif

struct hdd_adapter *
hdd_get_con_sap_adapter(struct hdd_adapter *this_sap_adapter,
			bool check_start_bss)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(this_sap_adapter);
	struct hdd_adapter *adapter, *con_sap_adapter;

	con_sap_adapter = NULL;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (adapter && ((adapter->device_mode == QDF_SAP_MODE) ||
				(adapter->device_mode == QDF_P2P_GO_MODE)) &&
						adapter != this_sap_adapter) {
			if (check_start_bss) {
				if (test_bit(SOFTAP_BSS_STARTED,
						&adapter->event_flags)) {
					con_sap_adapter = adapter;
					break;
				}
			} else {
				con_sap_adapter = adapter;
				break;
			}
		}
	}

	return con_sap_adapter;
}

#ifdef MSM_PLATFORM
static inline bool hdd_adapter_is_sta(struct hdd_adapter *adapter)
{
	return adapter->device_mode == QDF_STA_MODE ||
		adapter->device_mode == QDF_P2P_CLIENT_MODE;
}

static inline bool hdd_adapter_is_ap(struct hdd_adapter *adapter)
{
	return adapter->device_mode == QDF_SAP_MODE ||
		adapter->device_mode == QDF_P2P_GO_MODE;
}

static bool hdd_any_adapter_is_assoc(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter;

	hdd_for_each_adapter(hdd_ctx, adapter) {
		if (hdd_adapter_is_sta(adapter) &&
		    WLAN_HDD_GET_STATION_CTX_PTR(adapter)->
			conn_info.connState == eConnectionState_Associated) {
			return true;
		}

		if (hdd_adapter_is_ap(adapter) &&
		    WLAN_HDD_GET_AP_CTX_PTR(adapter)->ap_active) {
			return true;
		}

		if (adapter->device_mode == QDF_NDI_MODE &&
		    WLAN_HDD_GET_STATION_CTX_PTR(adapter)->
		    conn_info.connState == eConnectionState_NdiConnected)
			return true;
	}

	return false;
}

static bool hdd_bus_bw_compute_timer_is_running(struct hdd_context *hdd_ctx)
{
	bool is_running;

	qdf_spinlock_acquire(&hdd_ctx->bus_bw_timer_lock);
	is_running = hdd_ctx->bus_bw_timer_running;
	qdf_spinlock_release(&hdd_ctx->bus_bw_timer_lock);

	return is_running;
}

static void __hdd_bus_bw_compute_timer_start(struct hdd_context *hdd_ctx)
{
	qdf_spinlock_acquire(&hdd_ctx->bus_bw_timer_lock);
	hdd_ctx->bus_bw_timer_running = true;
	qdf_timer_start(&hdd_ctx->bus_bw_timer,
			hdd_ctx->config->busBandwidthComputeInterval);
	qdf_spinlock_release(&hdd_ctx->bus_bw_timer_lock);
}

void hdd_bus_bw_compute_timer_start(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx)) {
		hdd_exit();
		return;
	}

	if (hdd_bus_bw_compute_timer_is_running(hdd_ctx)) {
		hdd_debug("Bandwidth compute timer already started");
		return;
	}

	__hdd_bus_bw_compute_timer_start(hdd_ctx);

	hdd_exit();
}

void hdd_bus_bw_compute_timer_try_start(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	if (hdd_bus_bw_compute_timer_is_running(hdd_ctx)) {
		hdd_debug("Bandwidth compute timer already started");
		return;
	}

	if (hdd_any_adapter_is_assoc(hdd_ctx))
		__hdd_bus_bw_compute_timer_start(hdd_ctx);

	hdd_exit();
}

static void __hdd_bus_bw_compute_timer_stop(struct hdd_context *hdd_ctx)
{
	ucfg_ipa_set_perf_level(hdd_ctx->pdev, 0, 0);

	qdf_spinlock_acquire(&hdd_ctx->bus_bw_timer_lock);
	hdd_ctx->bus_bw_timer_running = false;
	qdf_timer_sync_cancel(&hdd_ctx->bus_bw_timer);
	qdf_spinlock_release(&hdd_ctx->bus_bw_timer_lock);

	/* work callback is long running; flush outside of lock */
	cancel_work_sync(&hdd_ctx->bus_bw_work);
	hdd_reset_tcp_delack(hdd_ctx);
	hdd_reset_tcp_adv_win_scale(hdd_ctx);
}

void hdd_bus_bw_compute_timer_stop(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	if (!hdd_bus_bw_compute_timer_is_running(hdd_ctx)) {
		hdd_debug("Bandwidth compute timer already stopped");
		return;
	}

	__hdd_bus_bw_compute_timer_stop(hdd_ctx);

	hdd_exit();
}

void hdd_bus_bw_compute_timer_try_stop(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	if (!hdd_bus_bw_compute_timer_is_running(hdd_ctx)) {
		hdd_debug("Bandwidth compute timer already stopped");
		return;
	}

	if (!hdd_any_adapter_is_assoc(hdd_ctx))
		__hdd_bus_bw_compute_timer_stop(hdd_ctx);

	hdd_exit();
}
#endif

/**
 * wlan_hdd_check_custom_con_channel_rules() - This function checks the sap's
 *                                            and sta's operating channel.
 * @sta_adapter:  Describe the first argument to foobar.
 * @ap_adapter:   Describe the second argument to foobar.
 * @roam_profile: Roam profile of AP to which STA wants to connect.
 * @concurrent_chnl_same: If both SAP and STA channels are same then
 *                        set this flag to true else false.
 *
 * This function checks the sap's operating channel and sta's operating channel.
 * if both are same then it will return false else it will restart the sap in
 * sta's channel and return true.
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE.
 */
QDF_STATUS
wlan_hdd_check_custom_con_channel_rules(struct hdd_adapter *sta_adapter,
					struct hdd_adapter *ap_adapter,
					struct csr_roam_profile *roam_profile,
					tScanResultHandle *scan_cache,
					bool *concurrent_chnl_same)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	uint8_t channel_id;
	QDF_STATUS status;
	enum QDF_OPMODE device_mode = ap_adapter->device_mode;
	*concurrent_chnl_same = true;

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	status =
	 sme_get_ap_channel_from_scan_cache(roam_profile,
					    scan_cache,
					    &channel_id);
	if (QDF_STATUS_SUCCESS == status) {
		if ((QDF_SAP_MODE == device_mode) &&
			(channel_id < SIR_11A_CHANNEL_BEGIN)) {
			if (hdd_ap_ctx->operating_channel != channel_id) {
				*concurrent_chnl_same = false;
				hdd_debug("channels are different");
			}
		} else if ((QDF_P2P_GO_MODE == device_mode) &&
				(channel_id >= SIR_11A_CHANNEL_BEGIN)) {
			if (hdd_ap_ctx->operating_channel != channel_id) {
				*concurrent_chnl_same = false;
				hdd_debug("channels are different");
			}
		}
	} else {
		/*
		 * Lets handle worst case scenario here, Scan cache lookup is
		 * failed so we have to stop the SAP to avoid any channel
		 * discrepancy  between SAP's channel and STA's channel.
		 * Return the status as failure so caller function could know
		 * that scan look up is failed.
		 */
		hdd_err("Finding AP from scan cache failed");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

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

	if (NULL == ap_adapter) {
		hdd_err("ap_adapter is NULL here");
		return;
	}

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	mutex_lock(&hdd_ctx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		wlan_hdd_del_station(ap_adapter);
		hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
		hdd_debug("Now doing SAP STOPBSS");
		qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
		if (QDF_STATUS_SUCCESS == wlansap_stop_bss(hdd_ap_ctx->
							sap_context)) {
			qdf_status = qdf_wait_for_event_completion(&hostapd_state->
					qdf_stop_bss_event,
					SME_CMD_START_STOP_BSS_TIMEOUT);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				mutex_unlock(&hdd_ctx->sap_lock);
				hdd_err("SAP Stop Failed");
				return;
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
						ap_adapter->device_mode,
						ap_adapter->session_id);
		hdd_green_ap_start_state_mc(hdd_ctx, ap_adapter->device_mode,
					    false);
		hdd_debug("SAP Stop Success");
	} else {
		hdd_err("Can't stop ap because its not started");
	}
	mutex_unlock(&hdd_ctx->sap_lock);
}

/**
 * wlan_hdd_start_sap() - this function starts bss of SAP.
 * @ap_adapter: SAP adapter
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
	tsap_config_t *sap_config;

	if (NULL == ap_adapter) {
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
	wlan_reg_set_channel_params(hdd_ctx->pdev,
				    hdd_ap_ctx->sap_config.channel, 0,
				    &hdd_ap_ctx->sap_config.ch_params);

	qdf_event_reset(&hostapd_state->qdf_event);
	if (wlansap_start_bss(hdd_ap_ctx->sap_context, hdd_hostapd_sap_event_cb,
			      &hdd_ap_ctx->sap_config,
			      ap_adapter->dev)
			      != QDF_STATUS_SUCCESS)
		goto end;

	hdd_debug("Waiting for SAP to start");
	qdf_status = qdf_wait_for_event_completion(&hostapd_state->qdf_event,
					SME_CMD_START_STOP_BSS_TIMEOUT);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("SAP Start failed");
		goto end;
	}
	hdd_info("SAP Start Success");
	wlansap_reset_sap_config_add_ie(sap_config, eUPDATE_IE_ALL);
	set_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
	if (hostapd_state->bss_state == BSS_START) {
		policy_mgr_incr_active_session(hdd_ctx->psoc,
					ap_adapter->device_mode,
					ap_adapter->session_id);
		hdd_green_ap_start_state_mc(hdd_ctx, ap_adapter->device_mode,
					    true);
	}
	mutex_unlock(&hdd_ctx->sap_lock);

	return;
end:
	wlansap_reset_sap_config_add_ie(sap_config, eUPDATE_IE_ALL);
	mutex_unlock(&hdd_ctx->sap_lock);
	/* SAP context and beacon cleanup will happen during driver unload
	 * in hdd_stop_adapter
	 */
	hdd_err("SAP restart after SSR failed! Reload WLAN and try SAP again");

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

static int hdd_qdf_print_init(void)
{
	int qdf_print_idx;
	QDF_STATUS status;

	status = qdf_print_setup();
	if (status != QDF_STATUS_SUCCESS) {
		pr_err("qdf_print_setup failed\n");
		return -EINVAL;
	}

	qdf_print_idx = qdf_print_ctrl_register(cinfo, NULL, NULL, "MCL_WLAN");

	if (qdf_print_idx < 0) {
		pr_err("qdf_print_ctrl_register failed, ret = %d\n",
		       qdf_print_idx);
		return -EINVAL;
	}

	qdf_set_pidx(qdf_print_idx);

	return 0;
}

static void hdd_qdf_print_deinit(void)
{
	int qdf_print_idx;

	qdf_print_idx = qdf_get_pidx();
	qdf_print_ctrl_cleanup(qdf_print_idx);
}

static inline int hdd_state_query_cb(void)
{
	return !!wlan_hdd_validate_context(cds_get_context(QDF_MODULE_ID_HDD));
}

/**
 * hdd_init() - Initialize Driver
 *
 * This function initilizes CDS global context with the help of cds_init. This
 * has to be the first function called after probe to get a valid global
 * context.
 *
 * Return: 0 for success, errno on failure
 */
int hdd_init(void)
{
	QDF_STATUS status;
	int ret = 0;

	status = cds_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to allocate CDS context");
		ret = -ENOMEM;
		goto err_out;
	}
	qdf_register_module_state_query_callback(hdd_state_query_cb);

	wlan_init_bug_report_lock();

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
	wlan_logging_sock_init_svc();
#endif

	qdf_timer_init(NULL, &hdd_drv_ops_inactivity_timer,
		(void *)hdd_drv_ops_inactivity_handler, NULL,
		QDF_TIMER_TYPE_SW);

	hdd_trace_init();

	hdd_register_debug_callback();

err_out:
	return ret;
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
	qdf_timer_free(&hdd_drv_ops_inactivity_timer);

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
	wlan_logging_sock_deinit_svc();
#endif

	wlan_destroy_bug_report_lock();
	cds_deinit();
}

#ifdef QCA_WIFI_NAPIER_EMULATION
#define HDD_WLAN_START_WAIT_TIME ((CDS_WMA_TIMEOUT + 5000) * 100)
#else
#define HDD_WLAN_START_WAIT_TIME (CDS_WMA_TIMEOUT + 5000)
#endif

static int wlan_hdd_state_ctrl_param_open(struct inode *inode,
					  struct file *file)
{
	qdf_atomic_inc(&wlan_hdd_state_fops_ref);

	return 0;
}

static void hdd_inform_wifi_off(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx || !hdd_ctx->mac_handle) {
		hdd_err("Invalid hdd/pdev context");
		return;
	}
	sme_free_blacklist(hdd_ctx->mac_handle);
}

static ssize_t wlan_hdd_state_ctrl_param_write(struct file *filp,
						const char __user *user_buf,
						size_t count,
						loff_t *f_pos)
{
	char buf[3];
	static const char wlan_off_str[] = "OFF";
	static const char wlan_on_str[] = "ON";
	int ret;
	unsigned long rc;

	if (copy_from_user(buf, user_buf, 3)) {
		pr_err("Failed to read buffer\n");
		return -EINVAL;
	}

	if (strncmp(buf, wlan_off_str, strlen(wlan_off_str)) == 0) {
		pr_debug("Wifi turning off from UI\n");
		hdd_inform_wifi_off();
		goto exit;
	}

	if (strncmp(buf, wlan_on_str, strlen(wlan_on_str)) == 0) {
		pr_info("Wifi Turning On from UI\n");
	}

	if (strncmp(buf, wlan_on_str, strlen(wlan_on_str)) != 0) {
		pr_err("Invalid value received from framework");
		goto exit;
	}

	if (!cds_is_driver_loaded() || cds_is_driver_recovering()) {
		init_completion(&wlan_start_comp);
		rc = wait_for_completion_timeout(&wlan_start_comp,
				msecs_to_jiffies(HDD_WLAN_START_WAIT_TIME));
		if (!rc) {
			pr_err("Timed-out in wlan_hdd_state_ctrl_param_write");
			ret = -EINVAL;
			return ret;
		}
		hdd_start_complete(0);
	}

exit:
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

	qdf_atomic_init(&wlan_hdd_state_fops_ref);
	device = MKDEV(wlan_hdd_state_major, 0);

	ret = alloc_chrdev_region(&device, 0, dev_num, "qcwlanstate");
	if (ret) {
		pr_err("Failed to register qcwlanstate");
		goto dev_alloc_err;
	}
	wlan_hdd_state_major = MAJOR(device);

	class = class_create(THIS_MODULE, WLAN_MODULE_NAME);
	if (IS_ERR(class)) {
		pr_err("wlan_hdd_state class_create error");
		goto class_err;
	}

	dev = device_create(class, NULL, device, NULL, WLAN_MODULE_NAME);
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
		WLAN_MODULE_NAME, wlan_hdd_state_major);

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

static void wlan_hdd_state_ctrl_param_destroy(void)
{
	cdev_del(&wlan_hdd_state_cdev);
	device_destroy(class, device);
	class_destroy(class);
	unregister_chrdev_region(device, dev_num);

	pr_info("Device node unregistered");
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
	status = dispatcher_init();
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	/* initialize non-converged components */
	status = disa_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto dispatcher_deinit;

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

	status = ucfg_mlme_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto oui_deinit;

	status = ucfg_pkt_capture_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto mlme_deinit;

	status = ucfg_ftm_time_sync_init();
	if (QDF_IS_STATUS_ERROR(status))
		goto pkt_capture_deinit;

	return QDF_STATUS_SUCCESS;

pkt_capture_deinit:
	ucfg_pkt_capture_deinit();
mlme_deinit:
	ucfg_mlme_deinit();
oui_deinit:
	ucfg_action_oui_deinit();
ipa_deinit:
	ipa_deinit();
ocb_deinit:
	ucfg_ocb_deinit();
pmo_deinit:
	pmo_deinit();
disa_deinit:
	disa_deinit();
dispatcher_deinit:
	dispatcher_deinit();

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
	ucfg_ftm_time_sync_deinit();
	ucfg_pkt_capture_deinit();
	ucfg_mlme_deinit();
	ucfg_action_oui_deinit();
	ipa_deinit();
	ucfg_ocb_deinit();
	pmo_deinit();
	disa_deinit();

	/* deinitialize converged components */
	dispatcher_deinit();
}

void hdd_component_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	ocb_psoc_enable(psoc);
	disa_psoc_enable(psoc);
}

void hdd_component_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	disa_psoc_disable(psoc);
	ocb_psoc_disable(psoc);
}

/**
 * hdd_fln() - logging macro for before/after qdf logging is initialized
 * @fmt: printk compatible format string
 * @args: arguments to be logged
 *
 * Note: To be used only in module insmod and rmmod code paths
 *
 * Return: None
 */
#define hdd_fln(fmt, args...) __hdd_fln(FL(fmt "\n"), ##args)
#define __hdd_fln(fmt, args...) pr_err(fmt, ##args)

/**
 * hdd_driver_load() - Perform the driver-level load operation
 *
 * Note: this is used in both static and DLKM driver builds
 *
 * Return: Errno
 */
static int hdd_driver_load(void)
{
	QDF_STATUS status;
	int errno;

	pr_err("%s: Loading driver v%s (%s)\n",
	       WLAN_MODULE_NAME,
	       g_wlan_driver_version,
	       TIMER_MANAGER_STR MEMORY_DEBUG_STR PANIC_ON_BUG_STR);

	hdd_qdf_print_init();
	errno = hdd_init();
	if (errno) {
		hdd_fln("Failed to init HDD; errno:%d", errno);
		goto exit;
	}

	status = hdd_component_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_fln("Failed to init components; status:%u", status);
		errno = qdf_status_to_os_return(status);
		goto hdd_deinit;
	}

	status = qdf_wake_lock_create(&wlan_wake_lock, "wlan");
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_fln("Failed to create wake lock; status:%u", status);
		errno = qdf_status_to_os_return(status);
		goto comp_deinit;
	}

	hdd_set_conparam(con_mode);

	errno = wlan_hdd_state_ctrl_param_create();
	if (errno) {
		hdd_fln("Failed to create ctrl param; errno:%d", errno);
		goto wakelock_destroy;
	}

	errno = pld_init();
	if (errno) {
		hdd_fln("Failed to init PLD; errno:%d", errno);
		goto param_destroy;
	}

	errno = wlan_hdd_register_driver();
	if (errno) {
		hdd_fln("Failed to register driver; errno:%d", errno);
		goto pld_deinit;
	}

	pr_info("%s: driver loaded\n", WLAN_MODULE_NAME);

	return 0;

pld_deinit:
	pld_deinit();
	/* Wait for any ref taken on /dev/wlan to be released */
	while (qdf_atomic_read(&wlan_hdd_state_fops_ref))
		;
param_destroy:
	wlan_hdd_state_ctrl_param_destroy();
wakelock_destroy:
	qdf_wake_lock_destroy(&wlan_wake_lock);
comp_deinit:
	hdd_component_deinit();
hdd_deinit:
	hdd_deinit();

exit:
	return errno;
}

/**
 * hdd_driver_unload() - Performs the driver-level unload operation
 *
 * Note: this is used in both static and DLKM driver builds
 *
 * Return: None
 */
static void hdd_driver_unload(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	pr_info("%s: Unloading driver v%s\n", WLAN_MODULE_NAME,
		QWLAN_VERSIONSTR);

	if (!hdd_wait_for_recovery_completion())
		return;

	mutex_lock(&hdd_init_deinit_lock);
	cds_set_driver_loaded(false);
	cds_set_unload_in_progress(true);
	mutex_unlock(&hdd_init_deinit_lock);

	if (hdd_ctx)
		hdd_psoc_idle_timer_stop(hdd_ctx);

	wlan_hdd_unregister_driver();

	pld_deinit();
	wlan_hdd_state_ctrl_param_destroy();
	hdd_set_conparam(0);
	qdf_wake_lock_destroy(&wlan_wake_lock);
	hdd_component_deinit();
	hdd_deinit();
	hdd_qdf_print_deinit();
}

#ifndef MODULE
/**
 * wlan_boot_cb() - Wlan boot callback
 * @kobj:      object whose directory we're creating the link in.
 * @attr:      attribute the user is interacting with
 * @buff:      the buffer containing the user data
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
		hdd_fln("wlan driver already initialized");
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

	wlan_loader->boot_wlan_obj = kobject_create_and_add("boot_wlan",
							    kernel_kobj);
	if (!wlan_loader->boot_wlan_obj) {
		hdd_fln("sysfs create and add failed");
		goto error_return;
	}

	ret = sysfs_create_group(wlan_loader->boot_wlan_obj,
				 wlan_loader->attr_group);
	if (ret) {
		hdd_fln("sysfs create group failed; errno:%d", ret);
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
		hdd_fln("wlan loader context is Null!");
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
static int hdd_module_init(void)
{
	if (hdd_driver_load())
		return -EINVAL;

	return 0;
}
#else
static int __init hdd_module_init(void)
{
	int ret = -EINVAL;

	ret = wlan_init_sysfs();
	if (ret)
		hdd_fln("Failed to create sysfs entry");

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
static void __exit hdd_module_exit(void)
{
	hdd_driver_unload();
}
#else
static void __exit hdd_module_exit(void)
{
	hdd_driver_unload();
	wlan_deinit_sysfs();
}
#endif

#undef hdd_fln

static int fwpath_changed_handler(const char *kmessage,
				  const struct kernel_param *kp)
{
	return param_set_copystring(kmessage, kp);
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

/**
 * is_con_mode_valid() check con mode is valid or not
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
	case QDF_GLOBAL_MISSION_MODE:
		return true;
	default:
		return false;
	}
}

/**
 * hdd_get_adpter_mode() - returns adapter mode based on global con mode
 * @mode: global con mode
 *
 * Return: adapter mode
 */
static enum QDF_OPMODE hdd_get_adpter_mode(
					enum QDF_GLOBAL_MODE mode)
{

	switch (mode) {
	case QDF_GLOBAL_MISSION_MODE:
		return QDF_STA_MODE;
	case QDF_GLOBAL_MONITOR_MODE:
		return QDF_MONITOR_MODE;
	case QDF_GLOBAL_EPPING_MODE:
		return QDF_EPPING_MODE;
	case QDF_GLOBAL_FTM_MODE:
		return QDF_FTM_MODE;
	case QDF_GLOBAL_QVIT_MODE:
		return QDF_QVIT_MODE;
	default:
		return QDF_MAX_NO_OF_MODE;
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
		/* fallthrough */
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
	int driver_status;

	driver_status = hdd_ctx->driver_status;

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

static int hdd_register_req_mode(struct hdd_context *hdd_ctx,
				 enum QDF_GLOBAL_MODE mode)
{
	struct hdd_adapter *adapter;
	int ret = 0;
	bool rtnl_held;
	qdf_device_t qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	QDF_STATUS status;

	if (!qdf_dev) {
		hdd_err("qdf device context is Null return!");
		return -EINVAL;
	}

	rtnl_held = hdd_hold_rtnl_lock();
	switch (mode) {
	case QDF_GLOBAL_MISSION_MODE:
		ret = hdd_open_interfaces(hdd_ctx, rtnl_held);
		if (ret)
			hdd_err("Failed to open interfaces: %d", ret);
		break;
	case QDF_GLOBAL_FTM_MODE:
		adapter = hdd_open_adapter(hdd_ctx, QDF_FTM_MODE, "wlan%d",
					   wlan_hdd_get_intf_addr(hdd_ctx,
								  QDF_FTM_MODE),
					   NET_NAME_UNKNOWN, rtnl_held);
		if (adapter == NULL)
			ret = -EINVAL;
		break;
	case QDF_GLOBAL_MONITOR_MODE:
		adapter = hdd_open_adapter(hdd_ctx, QDF_MONITOR_MODE, "wlan%d",
					   wlan_hdd_get_intf_addr(
							hdd_ctx,
							QDF_MONITOR_MODE),
					   NET_NAME_UNKNOWN, rtnl_held);
		if (adapter == NULL)
			ret = -EINVAL;
		break;
	case QDF_GLOBAL_EPPING_MODE:
		status = epping_open();
		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("Failed to open in eeping mode: %d", status);
			ret = -EINVAL;
			break;
		}
		ret = epping_enable(qdf_dev->dev);
		if (ret) {
			hdd_err("Failed to enable in epping mode : %d", ret);
			epping_close();
		}
		break;
	default:
		hdd_err("Mode not supported");
		ret = -ENOTSUPP;
		break;
	}
	hdd_release_rtnl_lock();
	rtnl_held = false;
	return ret;
}

/**
 * __con_mode_handler() - Handles module param con_mode change
 * @kmessage: con mode name on which driver to be bring up
 * @kp: The associated kernel parameter
 * @hdd_ctx: Pointer to the global HDD context
 *
 * This function is invoked when user updates con mode using sys entry,
 * to initialize and bring-up driver in that specific mode.
 *
 * Return - 0 on success and failure code on failure
 */
static int __con_mode_handler(const char *kmessage,
			      const struct kernel_param *kp,
			      struct hdd_context *hdd_ctx)
{
	int ret;
	enum QDF_GLOBAL_MODE curr_mode;
	enum QDF_OPMODE adapter_mode;
	int new_con_mode;

	hdd_info("con_mode handler: %s", kmessage);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	qdf_atomic_set(&hdd_ctx->con_mode_flag, 1);

	ret = kstrtoint(kmessage, 0, &new_con_mode);
	if (ret) {
		hdd_err("Failed to parse con_mode '%s'", kmessage);
		goto reset_flags;
	}
	mutex_lock(&hdd_init_deinit_lock);

	if (!is_con_mode_valid(new_con_mode)) {
		hdd_err("invalid con_mode %d", new_con_mode);
		ret = -EINVAL;
		goto reset_flags;
	}

	curr_mode = hdd_get_conparam();
	if (curr_mode == new_con_mode) {
		hdd_err("curr mode: %d is same as user triggered mode %d",
			curr_mode, new_con_mode);
		ret = 0;
		goto reset_flags;
	}

	/* ensure adapters are stopped */
	hdd_stop_present_mode(hdd_ctx, curr_mode);

	is_mode_change_psoc_idle_shutdown = true;
	ret = pld_idle_shutdown(hdd_ctx->parent_dev,
				hdd_mode_change_psoc_idle_shutdown);
	if (ret) {
		is_mode_change_psoc_idle_shutdown = false;
		hdd_err("Failed to change the mode because of idle shutdown");
		goto reset_flags;
	}

	/* Cleanup present mode before switching to new mode */
	hdd_cleanup_present_mode(hdd_ctx, curr_mode);

	hdd_set_conparam(new_con_mode);

	/* Register for new con_mode & then kick_start modules again */
	ret = hdd_register_req_mode(hdd_ctx, new_con_mode);
	if (ret) {
		hdd_err("Failed to register for new mode");
		goto reset_flags;
	}

	adapter_mode = hdd_get_adpter_mode(new_con_mode);
	if (adapter_mode == QDF_MAX_NO_OF_MODE) {
		hdd_err("invalid adapter");
		ret = -EINVAL;
		goto reset_flags;
	}

	ret = pld_idle_restart(hdd_ctx->parent_dev,
			       hdd_mode_change_psoc_idle_restart);
	if (ret) {
		is_mode_change_psoc_idle_shutdown = false;
		hdd_err("Start wlan modules failed: %d", ret);
		goto reset_flags;
	}

	if (new_con_mode == QDF_GLOBAL_MONITOR_MODE) {
		struct hdd_adapter *adapter =
			hdd_get_adapter(hdd_ctx, adapter_mode);

		if (!adapter) {
			hdd_err("Failed to get adapter:%d", adapter_mode);
			goto reset_flags;
		}

		if (hdd_start_adapter(adapter)) {
			hdd_err("Failed to start %s adapter", kmessage);
			ret = -EINVAL;
			goto reset_flags;
		}

		hdd_info("Acquire wakelock for monitor mode!");
		qdf_wake_lock_acquire(&hdd_ctx->monitor_mode_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_MONITOR_MODE);
	}

	/* con_mode is a global module parameter */
	con_mode = new_con_mode;
	hdd_info("Mode successfully changed to %s", kmessage);
	ret = 0;

reset_flags:
	mutex_unlock(&hdd_init_deinit_lock);
	qdf_atomic_set(&hdd_ctx->con_mode_flag, 0);
	return ret;
}


static int con_mode_handler(const char *kmessage, const struct kernel_param *kp)
{
	int ret;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!cds_wait_for_external_threads_completion(__func__)) {
		hdd_warn("External threads are still active, can not change mode");
		return -EAGAIN;
	}

	cds_ssr_protect(__func__);
	ret = __con_mode_handler(kmessage, kp, hdd_ctx);
	cds_ssr_unprotect(__func__);

	return ret;
}

static int con_mode_handler_ftm(const char *kmessage,
				const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(kmessage, kp);

	if (con_mode_ftm != QDF_GLOBAL_FTM_MODE) {
		pr_err("Only FTM mode supported!");
		return -ENOTSUPP;
	}

	hdd_set_conparam(con_mode_ftm);
	con_mode = con_mode_ftm;

	return ret;
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
static int con_mode_handler_monitor(const char *kmessage,
				    const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(kmessage, kp);

	if (con_mode_monitor != QDF_GLOBAL_MONITOR_MODE) {
		pr_err("Only Monitor mode supported!");
		return -ENOTSUPP;
	}

	hdd_set_conparam(con_mode_monitor);
	con_mode = con_mode_monitor;

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
 * hdd_clean_up_pre_cac_interface() - Clean up the pre cac interface
 * @hdd_ctx: HDD context
 *
 * Cleans up the pre cac interface, if it exists
 *
 * Return: None
 */
void hdd_clean_up_pre_cac_interface(struct hdd_context *hdd_ctx)
{
	uint8_t session_id;
	QDF_STATUS status;
	struct hdd_adapter *precac_adapter;

	status = wlan_sap_get_pre_cac_vdev_id(hdd_ctx->mac_handle, &session_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to get pre cac vdev id");
		return;
	}

	precac_adapter = hdd_get_adapter_by_vdev(hdd_ctx, session_id);
	if (!precac_adapter) {
		hdd_err("invalid pre cac adapter");
		return;
	}

	qdf_create_work(0, &hdd_ctx->sap_pre_cac_work,
			wlan_hdd_sap_pre_cac_failure,
			(void *)precac_adapter);
	qdf_sched_work(0, &hdd_ctx->sap_pre_cac_work);

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

	if (!ol_ctx)
		return;

	cfg.enable_self_recovery = hdd_ctx->config->enableSelfRecovery;
	cfg.enable_uart_print = hdd_ctx->config->enablefwprint;
	cfg.enable_fw_log = hdd_ctx->config->enable_fw_log;
	cfg.enable_ramdump_collection = hdd_ctx->config->is_ramdump_enabled;
	cfg.enable_lpass_support = hdd_lpass_is_supported(hdd_ctx);

	ol_init_ini_config(ol_ctx, &cfg);
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
	cfg->runtime_pm_delay = hdd_ctx->config->runtime_pm_delay;
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

	if (!scn)
		return;

	cfg.enable_self_recovery = hdd_ctx->config->enableSelfRecovery;
	hdd_populate_runtime_cfg(hdd_ctx, &cfg);
	hif_init_ini_config(scn, &cfg);

	if (hdd_ctx->config->prevent_link_down)
		hif_vote_link_up(scn);
}

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
static void
hdd_update_dp_config_queue_threshold(struct hdd_context *hdd_ctx,
				     struct cdp_config_params *params)
{
	params->tx_flow_stop_queue_threshold =
			hdd_ctx->config->TxFlowStopQueueThreshold;
	params->tx_flow_start_queue_offset =
			hdd_ctx->config->TxFlowStartQueueOffset;
}
#else
static inline void
hdd_update_dp_config_queue_threshold(struct hdd_context *hdd_ctx,
				     struct cdp_config_params *params)
{
}
#endif

/**
 * hdd_update_dp_config() - Propagate config parameters to Lithium
 *                          datapath
 * @hdd_ctx: HDD Context
 *
 * Return: 0 for success/errno for failure
 */
static int hdd_update_dp_config(struct hdd_context *hdd_ctx)
{
	struct cdp_config_params params = {0};
	QDF_STATUS status;

	params.tso_enable = hdd_ctx->config->tso_enable;
	params.lro_enable = hdd_ctx->config->lro_enable;
	hdd_update_dp_config_queue_threshold(hdd_ctx, &params);
	params.flow_steering_enable = hdd_ctx->config->flow_steering_enable;
	params.napi_enable = hdd_ctx->napi_enable;
	params.tcp_udp_checksumoffload =
			hdd_ctx->config->enable_ip_tcp_udp_checksum_offload;

	status = cdp_update_config_parameters(
					cds_get_context(QDF_MODULE_ID_SOC),
					&params);
	if (status) {
		hdd_err("Failed to attach config parameters");
		return status;
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

#ifdef FEATURE_WLAN_RA_FILTERING
/**
 * hdd_ra_populate_cds_config() - Populate RA filtering cds configuration
 * @psoc_cfg: pmo psoc Configuration
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static inline void hdd_ra_populate_pmo_config(
			struct pmo_psoc_cfg *psoc_cfg,
			struct hdd_context *hdd_ctx)
{
	psoc_cfg->ra_ratelimit_interval =
		hdd_ctx->config->RArateLimitInterval;
	psoc_cfg->ra_ratelimit_enable =
		hdd_ctx->config->IsRArateLimitEnabled;
}
#else
static inline void hdd_ra_populate_pmo_config(
			struct cds_config_info *cds_cfg,
			struct hdd_context *hdd_ctx)
{
}
#endif

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

	/*
	 * Value of hdd_ctx->wowEnable can be,
	 * 0 - Disable both magic pattern match and pattern byte match.
	 * 1 - Enable magic pattern match on all interfaces.
	 * 2 - Enable pattern byte match on all interfaces.
	 * 3 - Enable both magic patter and pattern byte match on
	 *     all interfaces.
	 */
	psoc_cfg.magic_ptrn_enable =
		(hdd_ctx->config->wowEnable & 0x01) ? true : false;
	psoc_cfg.ptrn_match_enable_all_vdev =
		(hdd_ctx->config->wowEnable & 0x02) ? true : false;
	psoc_cfg.apf_enable = hdd_ctx->config->apf_packet_filter_enable;
	psoc_cfg.arp_offload_enable = hdd_ctx->config->fhostArpOffload;
	psoc_cfg.hw_filter_mode_bitmap = hdd_ctx->config->hw_filter_mode_bitmap;
	psoc_cfg.ns_offload_enable_dynamic = hdd_ctx->config->fhostNSOffload;
	psoc_cfg.ns_offload_enable_static = hdd_ctx->config->fhostNSOffload;
	psoc_cfg.packet_filter_enabled = !hdd_ctx->config->disablePacketFilter;
	psoc_cfg.ssdp = hdd_ctx->config->ssdp;
	psoc_cfg.enable_mc_list = hdd_ctx->config->fEnableMCAddrList;
	psoc_cfg.active_mode_offload = hdd_ctx->config->active_mode_offload;
	psoc_cfg.ap_arpns_support = hdd_ctx->ap_arpns_support;
	psoc_cfg.d0_wow_supported = wma_d0_wow_is_supported();
	psoc_cfg.sta_dynamic_dtim = hdd_ctx->config->enableDynamicDTIM;
	psoc_cfg.sta_mod_dtim = hdd_ctx->config->enableModulatedDTIM;
	psoc_cfg.sta_max_li_mod_dtim = hdd_ctx->config->fMaxLIModulatedDTIM;
	psoc_cfg.power_save_mode = hdd_ctx->config->enablePowersaveOffload;
	psoc_cfg.auto_power_save_fail_mode =
		hdd_ctx->config->auto_pwr_save_fail_mode;
	psoc_cfg.wow_data_inactivity_timeout =
			hdd_ctx->config->wow_data_inactivity_timeout;
	psoc_cfg.ps_data_inactivity_timeout =
		hdd_ctx->config->nDataInactivityTimeout;
	psoc_cfg.ito_repeat_count = hdd_ctx->config->ito_repeat_count;

	hdd_ra_populate_pmo_config(&psoc_cfg, hdd_ctx);
	hdd_nan_populate_pmo_config(&psoc_cfg, hdd_ctx);
	hdd_lpass_populate_pmo_config(&psoc_cfg, hdd_ctx);

	status = ucfg_pmo_update_psoc_config(hdd_ctx->psoc, &psoc_cfg);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("failed pmo psoc configuration; status:%d", status);

	return qdf_status_to_os_return(status);
}

/**
 * hdd_update_pkt_capture_config - Update packet capture config parameters
 * @hdd_ctx: HDD context
 *
 * Return: 0 on success / error on failure
 */
static int hdd_update_pkt_capture_config(struct hdd_context *hdd_ctx)
{
	struct pkt_capture_cfg pkt_capture_cfg = {0};
	QDF_STATUS status;

	pkt_capture_cfg.pkt_capture_mode = hdd_ctx->config->pkt_capture_mode;

	status = ucfg_pkt_capture_psoc_config(hdd_ctx->psoc, &pkt_capture_cfg);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("packet capture config failed");

	return qdf_status_to_os_return(status);
}

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
/**
 * hdd_update_time_sync_ftm_config - Update time sync ftm config parameters
 * @hdd_ctx: HDD context
 *
 * Return: 0 on success / error on failure
 */
static int hdd_update_time_sync_ftm_config(struct hdd_context *hdd_ctx)
{
	struct ftm_time_sync_cfg ftm_time_sync_cfg = {0};
	QDF_STATUS status;

	ftm_time_sync_cfg.enable = hdd_ctx->config->time_sync_ftm_enable;
	ftm_time_sync_cfg.mode = hdd_ctx->config->time_sync_ftm_mode;
	ftm_time_sync_cfg.role = hdd_ctx->config->time_sync_ftm_role;

	status = ucfg_ftm_time_sync_psoc_config(hdd_ctx->psoc,
						&ftm_time_sync_cfg);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("ftm time sync config failed");

	return qdf_status_to_os_return(status);
}
#else
static inline int hdd_update_time_sync_ftm_config(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

#ifdef FEATURE_WLAN_SCAN_PNO
static inline void hdd_update_pno_config(struct pno_user_cfg *pno_cfg,
	struct hdd_config *cfg)
{
	struct nlo_mawc_params *mawc_cfg = &pno_cfg->mawc_params;

	pno_cfg->channel_prediction = cfg->pno_channel_prediction;
	pno_cfg->top_k_num_of_channels = cfg->top_k_num_of_channels;
	pno_cfg->stationary_thresh = cfg->stationary_thresh;
	pno_cfg->scan_timer_repeat_value = cfg->configPNOScanTimerRepeatValue;
	pno_cfg->slow_scan_multiplier = cfg->pno_slow_scan_multiplier;
	pno_cfg->dfs_chnl_scan_enabled = cfg->enable_dfs_pno_chnl_scan;
	pno_cfg->adaptive_dwell_mode = cfg->pnoscan_adaptive_dwell_mode;
	pno_cfg->channel_prediction_full_scan =
		cfg->channel_prediction_full_scan;
	mawc_cfg->enable = cfg->MAWCEnabled && cfg->mawc_nlo_enabled;
	mawc_cfg->exp_backoff_ratio = cfg->mawc_nlo_exp_backoff_ratio;
	mawc_cfg->init_scan_interval = cfg->mawc_nlo_init_scan_interval;
	mawc_cfg->max_scan_interval = cfg->mawc_nlo_max_scan_interval;
}
#else
static inline void
hdd_update_pno_config(struct pno_user_cfg *pno_cfg,
		      struct hdd_config *cfg)
{
}
#endif

void hdd_update_ie_whitelist_attr(struct probe_req_whitelist_attr *ie_whitelist,
				  struct hdd_config *cfg)
{
	uint8_t i = 0;

	ie_whitelist->white_list = cfg->probe_req_ie_whitelist;
	if (!ie_whitelist->white_list)
		return;

	ie_whitelist->ie_bitmap[0] = cfg->probe_req_ie_bitmap_0;
	ie_whitelist->ie_bitmap[1] = cfg->probe_req_ie_bitmap_1;
	ie_whitelist->ie_bitmap[2] = cfg->probe_req_ie_bitmap_2;
	ie_whitelist->ie_bitmap[3] = cfg->probe_req_ie_bitmap_3;
	ie_whitelist->ie_bitmap[4] = cfg->probe_req_ie_bitmap_4;
	ie_whitelist->ie_bitmap[5] = cfg->probe_req_ie_bitmap_5;
	ie_whitelist->ie_bitmap[6] = cfg->probe_req_ie_bitmap_6;
	ie_whitelist->ie_bitmap[7] = cfg->probe_req_ie_bitmap_7;

	ie_whitelist->num_vendor_oui = cfg->no_of_probe_req_ouis;
	for (i = 0; i < ie_whitelist->num_vendor_oui; i++)
		ie_whitelist->voui[i] = cfg->probe_req_voui[i];
}

uint32_t hdd_limit_max_per_index_score(uint32_t per_index_score)
{
	uint8_t i, score;

	for (i = 0; i < MAX_INDEX_PER_INI; i++) {
		score = WLAN_GET_SCORE_PERCENTAGE(per_index_score, i);
		if (score > MAX_INDEX_SCORE)
			WLAN_SET_SCORE_PERCENTAGE(per_index_score,
				MAX_INDEX_SCORE, i);
	}

	return per_index_score;
}

/**
 * hdd_update_score_config - API to update candidate scoring related params
 * configuration parameters
 * @score_config: score config to update
 * @cfg: config params
 *
 * Return: 0 if success else err
 */
static void hdd_update_score_config(
	struct scoring_config *score_config, struct hdd_config *cfg)
{
	int total_weight;

	score_config->weight_cfg.rssi_weightage = cfg->rssi_weightage;
	score_config->weight_cfg.ht_caps_weightage = cfg->ht_caps_weightage;
	score_config->weight_cfg.vht_caps_weightage =
					cfg->vht_caps_weightage;
	score_config->weight_cfg.he_caps_weightage =
					cfg->he_caps_weightage;
	score_config->weight_cfg.chan_width_weightage =
		cfg->chan_width_weightage;
	score_config->weight_cfg.chan_band_weightage =
		cfg->chan_band_weightage;
	score_config->weight_cfg.nss_weightage = cfg->nss_weightage;
	score_config->weight_cfg.beamforming_cap_weightage =
		cfg->beamforming_cap_weightage;
	score_config->weight_cfg.pcl_weightage = cfg->pcl_weightage;
	score_config->weight_cfg.channel_congestion_weightage =
			cfg->channel_congestion_weightage;
	score_config->weight_cfg.oce_wan_weightage = cfg->oce_wan_weightage;

	total_weight = score_config->weight_cfg.rssi_weightage +
		       score_config->weight_cfg.ht_caps_weightage +
		       score_config->weight_cfg.vht_caps_weightage +
		       score_config->weight_cfg.he_caps_weightage +
		       score_config->weight_cfg.chan_width_weightage +
		       score_config->weight_cfg.chan_band_weightage +
		       score_config->weight_cfg.nss_weightage +
		       score_config->weight_cfg.beamforming_cap_weightage +
		       score_config->weight_cfg.pcl_weightage +
		       score_config->weight_cfg.channel_congestion_weightage +
		       score_config->weight_cfg.oce_wan_weightage;

	if (total_weight > BEST_CANDIDATE_MAX_WEIGHT) {
		hdd_err("total weight is greater than %d fallback to default values",
			BEST_CANDIDATE_MAX_WEIGHT);

		score_config->weight_cfg.rssi_weightage = RSSI_WEIGHTAGE;
		score_config->weight_cfg.ht_caps_weightage =
			HT_CAPABILITY_WEIGHTAGE;
		score_config->weight_cfg.vht_caps_weightage = VHT_CAP_WEIGHTAGE;
		score_config->weight_cfg.he_caps_weightage = HE_CAP_WEIGHTAGE;
		score_config->weight_cfg.chan_width_weightage =
			CHAN_WIDTH_WEIGHTAGE;
		score_config->weight_cfg.chan_band_weightage =
			CHAN_BAND_WEIGHTAGE;
		score_config->weight_cfg.nss_weightage = NSS_WEIGHTAGE;
		score_config->weight_cfg.beamforming_cap_weightage =
			BEAMFORMING_CAP_WEIGHTAGE;
		score_config->weight_cfg.pcl_weightage = PCL_WEIGHT;
		score_config->weight_cfg.channel_congestion_weightage =
			CHANNEL_CONGESTION_WEIGHTAGE;
		score_config->weight_cfg.oce_wan_weightage = OCE_WAN_WEIGHTAGE;
	}

	score_config->bandwidth_weight_per_index =
		hdd_limit_max_per_index_score(
			cfg->bandwidth_weight_per_index);
	score_config->nss_weight_per_index =
		hdd_limit_max_per_index_score(cfg->nss_weight_per_index);
	score_config->band_weight_per_index =
		hdd_limit_max_per_index_score(cfg->band_weight_per_index);

	score_config->rssi_score.best_rssi_threshold =
				cfg->best_rssi_threshold;
	score_config->rssi_score.good_rssi_threshold =
				cfg->good_rssi_threshold;
	score_config->rssi_score.bad_rssi_threshold =
				cfg->bad_rssi_threshold;
	score_config->rssi_score.good_rssi_pcnt = cfg->good_rssi_pcnt;
	score_config->rssi_score.bad_rssi_pcnt = cfg->bad_rssi_pcnt;
	score_config->rssi_score.good_rssi_bucket_size =
		cfg->good_rssi_bucket_size;
	score_config->rssi_score.bad_rssi_bucket_size =
		cfg->bad_rssi_bucket_size;
	score_config->rssi_score.rssi_pref_5g_rssi_thresh =
		cfg->rssi_pref_5g_rssi_thresh;

	score_config->esp_qbss_scoring.num_slot = cfg->num_esp_qbss_slots;
	score_config->esp_qbss_scoring.score_pcnt3_to_0 =
		hdd_limit_max_per_index_score(
			cfg->esp_qbss_score_slots3_to_0);
	score_config->esp_qbss_scoring.score_pcnt7_to_4 =
		hdd_limit_max_per_index_score(
			cfg->esp_qbss_score_slots7_to_4);
	score_config->esp_qbss_scoring.score_pcnt11_to_8 =
		hdd_limit_max_per_index_score(
			cfg->esp_qbss_score_slots11_to_8);
	score_config->esp_qbss_scoring.score_pcnt15_to_12 =
		hdd_limit_max_per_index_score(
			cfg->esp_qbss_score_slots15_to_12);

	score_config->oce_wan_scoring.num_slot = cfg->num_oce_wan_slots;
	score_config->oce_wan_scoring.score_pcnt3_to_0 =
		hdd_limit_max_per_index_score(
			cfg->oce_wan_score_slots3_to_0);
	score_config->oce_wan_scoring.score_pcnt7_to_4 =
		hdd_limit_max_per_index_score(
			cfg->oce_wan_score_slots7_to_4);
	score_config->oce_wan_scoring.score_pcnt11_to_8 =
		hdd_limit_max_per_index_score(
			cfg->oce_wan_score_slots11_to_8);
	score_config->oce_wan_scoring.score_pcnt15_to_12 =
		hdd_limit_max_per_index_score(
			cfg->oce_wan_score_slots15_to_12);


	score_config->cb_mode_24G = cfg->nChannelBondingMode24GHz;
	score_config->cb_mode_5G = cfg->nChannelBondingMode5GHz;
	score_config->vdev_nss_24g =
			cfg->enable2x2 ?
				GET_VDEV_NSS_CHAIN(cfg->rx_nss_2g,
						   STA_NSS_CHAINS_SHIFT) :
				1;
	score_config->vdev_nss_5g =
			cfg->enable2x2 ?
				GET_VDEV_NSS_CHAIN(cfg->rx_nss_5g,
						   STA_NSS_CHAINS_SHIFT) :
				1;

	if (cfg->dot11Mode == eHDD_DOT11_MODE_AUTO ||
	    cfg->dot11Mode == eHDD_DOT11_MODE_11ax ||
	    cfg->dot11Mode == eHDD_DOT11_MODE_11ax_ONLY)
		score_config->he_cap = 1;

	if (score_config->he_cap ||
	    cfg->dot11Mode == eHDD_DOT11_MODE_11ac ||
	    cfg->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY)
		score_config->vht_cap = 1;

	if (score_config->vht_cap || cfg->dot11Mode == eHDD_DOT11_MODE_11n ||
	    cfg->dot11Mode == eHDD_DOT11_MODE_11n_ONLY)
		score_config->ht_cap = 1;

	if (score_config->vht_cap && cfg->enableVhtFor24GHzBand)
		score_config->vht_24G_cap = 1;

	if (cfg->enableTxBF)
		score_config->beamformee_cap = 1;

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
	struct hdd_config *cfg = hdd_ctx->config;
	struct dfs_user_config dfs_cfg = {0};
	QDF_STATUS status;

	dfs_cfg.dfs_is_phyerr_filter_offload = !!cfg->fDfsPhyerrFilterOffload;
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
static int hdd_update_scan_config(struct hdd_context *hdd_ctx)
{
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	struct scan_user_cfg scan_cfg = {0};
	struct hdd_config *cfg = hdd_ctx->config;
	QDF_STATUS status;

	/* The ini is disallow DFS channel scan if ini is 1, so negate that */
	scan_cfg.allow_dfs_chan_in_first_scan = !cfg->initial_scan_no_dfs_chnl;
	scan_cfg.allow_dfs_chan_in_scan = cfg->enableDFSChnlScan;
	scan_cfg.use_wake_lock_in_user_scan = cfg->wake_lock_in_user_scan;
	scan_cfg.active_dwell = cfg->nActiveMaxChnTime;
	scan_cfg.active_dwell_2g = cfg->active_dwell_2g;
	scan_cfg.passive_dwell = cfg->nPassiveMaxChnTime;
	scan_cfg.conc_active_dwell = cfg->nActiveMaxChnTimeConc;
	scan_cfg.conc_passive_dwell = cfg->nPassiveMaxChnTimeConc;
	scan_cfg.conc_max_rest_time = cfg->nRestTimeConc;
	scan_cfg.conc_min_rest_time = cfg->min_rest_time_conc;
	scan_cfg.conc_idle_time = cfg->idle_time_conc;
	/* convert to ms */
	scan_cfg.scan_cache_aging_time =
		cfg->scanAgingTimeout * 1000;
	scan_cfg.prefer_5ghz = cfg->nRoamPrefer5GHz;
	scan_cfg.select_5ghz_margin = cfg->nSelect5GHzMargin;
	scan_cfg.scan_bucket_threshold = cfg->first_scan_bucket_threshold;
	scan_cfg.rssi_cat_gap = cfg->nRssiCatGap;
	scan_cfg.scan_dwell_time_mode = cfg->scan_adaptive_dwell_mode;
	scan_cfg.scan_dwell_time_mode_nc =
		cfg->scan_adaptive_dwell_mode_nc;
	scan_cfg.honour_nl_scan_policy_flags =
		cfg->honour_nl_scan_policy_flags;
	scan_cfg.is_snr_monitoring_enabled = cfg->fEnableSNRMonitoring;
	scan_cfg.usr_cfg_probe_rpt_time = cfg->scan_probe_repeat_time;
	scan_cfg.usr_cfg_num_probes = cfg->scan_num_probes;
	scan_cfg.is_bssid_hint_priority = cfg->is_bssid_hint_priority;
	scan_cfg.enable_mac_spoofing = cfg->enable_mac_spoofing;
	scan_cfg.sta_miracast_mcc_rest_time =
				cfg->sta_miracast_mcc_rest_time_val;
	scan_cfg.sta_scan_burst_duration = cfg->sta_scan_burst_duration;
	scan_cfg.p2p_scan_burst_duration = cfg->p2p_scan_burst_duration;
	scan_cfg.go_scan_burst_duration = cfg->go_scan_burst_duration;
	scan_cfg.ap_scan_burst_duration = cfg->ap_scan_burst_duration;
	scan_cfg.skip_dfs_chan_in_p2p_search = cfg->skipDfsChnlInP2pSearch;

	hdd_update_pno_config(&scan_cfg.pno_cfg, cfg);
	hdd_update_ie_whitelist_attr(&scan_cfg.ie_whitelist, cfg);
	hdd_update_score_config(&scan_cfg.score_config, cfg);

	status = ucfg_scan_update_user_config(psoc, &scan_cfg);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("failed pmo psoc configuration");
		return -EINVAL;
	}
	ucfg_scan_set_global_config(
		psoc, SCAN_CFG_DROP_BCN_ON_CHANNEL_MISMATCH,
		cfg->drop_bcn_on_chan_mismatch);

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

	ret = hdd_update_pkt_capture_config(hdd_ctx);
	if (ret)
		return ret;

	ret = hdd_update_time_sync_ftm_config(hdd_ctx);

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
 * @hddctx: pointer to hdd context
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

	if (!hdd_ctx->config->goptimize_chan_avoid_event) {
		hdd_warn("goptimize_chan_avoid_event ini param disabled");
		return -EINVAL;
	}

	status = sme_enable_disable_chanavoidind_event(hdd_ctx->mac_handle,
						       set_value);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Failed to send chan avoid command to SME");
		return -EINVAL;
	}
	return 0;
}

/**
 * hdd_set_roaming_in_progress() - to set the roaming in progress flag
 * @value: value to set
 *
 * This function will set the passed value to roaming in progress flag.
 *
 * Return: None
 */
void hdd_set_roaming_in_progress(bool value)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return;
	}

	hdd_ctx->roaming_in_progress = value;
	hdd_debug("Roaming in Progress set to %d", value);
	if (!hdd_ctx->roaming_in_progress) {
		/* Reset scan reject params on successful roam complete */
		hdd_debug("Reset scan reject params");
		hdd_init_scan_reject_params(hdd_ctx);
	}
}

bool hdd_is_roaming_in_progress(struct hdd_context *hdd_ctx)
{
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return false;
	}

	if (hdd_ctx->roaming_in_progress)
		hdd_debug("Roaming is in progress");

	return hdd_ctx->roaming_in_progress;
}

/**
 * struct hdd_is_connection_in_progress_priv - adapter connection info
 * @out_vdev_id: id of vdev where connection is occurring
 * @out_reason: scan reject reason
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
 * @context: user context supplied
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
	uint8_t sta_id = 0;
	uint8_t *sta_mac;
	struct hdd_context *hdd_ctx;
	mac_handle_t mac_handle;
	struct hdd_is_connection_in_progress_priv *context = ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return QDF_STATUS_E_ABORTED;
	}

	mac_handle = hdd_ctx->mac_handle;

	if (((QDF_STA_MODE == adapter->device_mode)
		|| (QDF_P2P_CLIENT_MODE == adapter->device_mode)
		|| (QDF_P2P_DEVICE_MODE == adapter->device_mode))
		&& (eConnectionState_Connecting ==
			(WLAN_HDD_GET_STATION_CTX_PTR(adapter))->
				conn_info.connState)) {
		hdd_debug("%pK(%d) mode %d Connection is in progress",
			  WLAN_HDD_GET_STATION_CTX_PTR(adapter),
			  adapter->session_id, adapter->device_mode);

		context->out_vdev_id = adapter->session_id;
		context->out_reason = CONNECTION_IN_PROGRESS;
		context->connection_in_progress = true;

		return QDF_STATUS_E_ABORTED;
	}
	/*
	 * sme_neighbor_middle_of_roaming is for LFR2
	 * hdd_is_roaming_in_progress is for LFR3
	 */
	if (((QDF_STA_MODE == adapter->device_mode) &&
	     sme_neighbor_middle_of_roaming(
		     mac_handle,
		     adapter->session_id)) ||
	     hdd_is_roaming_in_progress(hdd_ctx)) {
		hdd_debug("%pK(%d) mode %d Reassociation in progress",
			  WLAN_HDD_GET_STATION_CTX_PTR(adapter),
			  adapter->session_id, adapter->device_mode);

		context->out_vdev_id = adapter->session_id;
		context->out_reason = REASSOC_IN_PROGRESS;
		context->connection_in_progress = true;
		return QDF_STATUS_E_ABORTED;
	}

	if ((QDF_STA_MODE == adapter->device_mode) ||
		(QDF_P2P_CLIENT_MODE == adapter->device_mode) ||
		(QDF_P2P_DEVICE_MODE == adapter->device_mode)) {
		hdd_sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if ((eConnectionState_Associated ==
		    hdd_sta_ctx->conn_info.connState)
		    && sme_is_sta_key_exchange_in_progress(
		    mac_handle, adapter->session_id)) {
			sta_mac = (uint8_t *)&(adapter->mac_addr.bytes[0]);
			hdd_debug("client " QDF_MAC_ADDR_STR
				  " is in middle of WPS/EAPOL exchange.",
				  QDF_MAC_ADDR_ARRAY(sta_mac));

			context->out_vdev_id = adapter->session_id;
			context->out_reason = EAPOL_IN_PROGRESS;
			context->connection_in_progress = true;

			return QDF_STATUS_E_ABORTED;
		}
	} else if ((QDF_SAP_MODE == adapter->device_mode) ||
			(QDF_P2P_GO_MODE == adapter->device_mode)) {
		for (sta_id = 0; sta_id < WLAN_MAX_STA_COUNT; sta_id++) {
			if (!((adapter->sta_info[sta_id].in_use)
			    && (OL_TXRX_PEER_STATE_CONN ==
			    adapter->sta_info[sta_id].peer_state)))
				continue;

			sta_mac = (uint8_t *)
					&(adapter->sta_info[sta_id].
						sta_mac.bytes[0]);
			hdd_debug("client " QDF_MAC_ADDR_STR
				  " of SAP/GO is in middle of WPS/EAPOL exchange",
				  QDF_MAC_ADDR_ARRAY(sta_mac));

			context->out_vdev_id = adapter->session_id;
			context->out_reason = SAP_EAPOL_IN_PROGRESS;
			context->connection_in_progress = true;
			return QDF_STATUS_E_ABORTED;
		}

		if (hdd_ctx->connection_in_progress) {
			hdd_debug("AP/GO: vdev %d connection is in progress",
				  adapter->session_id);
			context->out_reason = SAP_CONNECTION_IN_PROGRESS;
			context->out_vdev_id = adapter->session_id;
			context->connection_in_progress = true;
			return QDF_STATUS_E_ABORTED;
		}
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
	tsap_config_t *sap_config;
	void *sap_ctx;

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	sap_config = &hdd_ap_ctx->sap_config;
	sap_ctx = hdd_ap_ctx->sap_context;

	mutex_lock(&hdd_ctx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		wlan_hdd_del_station(ap_adapter);
		hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(ap_adapter);
		qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
		if (QDF_STATUS_SUCCESS == wlansap_stop_bss(sap_ctx)) {
			qdf_status =
				qdf_wait_for_event_completion(&hostapd_state->
					qdf_stop_bss_event,
					SME_CMD_START_STOP_BSS_TIMEOUT);

			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hdd_err("SAP Stop Failed");
				goto end;
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
			ap_adapter->device_mode, ap_adapter->session_id);
		hdd_green_ap_start_state_mc(hdd_ctx, ap_adapter->device_mode,
					    false);
		hdd_err("SAP Stop Success");

		if (0 != wlan_hdd_cfg80211_update_apies(ap_adapter)) {
			hdd_err("SAP Not able to set AP IEs");
			wlansap_reset_sap_config_add_ie(sap_config,
					eUPDATE_IE_ALL);
			goto end;
		}

		qdf_event_reset(&hostapd_state->qdf_event);
		if (wlansap_start_bss(sap_ctx, hdd_hostapd_sap_event_cb,
				      sap_config,
				      ap_adapter->dev) != QDF_STATUS_SUCCESS) {
			hdd_err("SAP Start Bss fail");
			wlansap_reset_sap_config_add_ie(sap_config,
					eUPDATE_IE_ALL);
			goto end;
		}

		hdd_info("Waiting for SAP to start");
		qdf_status =
			qdf_wait_for_event_completion(&hostapd_state->qdf_event,
					SME_CMD_START_STOP_BSS_TIMEOUT);
		wlansap_reset_sap_config_add_ie(sap_config,
				eUPDATE_IE_ALL);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("SAP Start failed");
			goto end;
		}
		hdd_err("SAP Start Success");
		set_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags);
		if (hostapd_state->bss_state == BSS_START) {
			policy_mgr_incr_active_session(hdd_ctx->psoc,
						ap_adapter->device_mode,
						ap_adapter->session_id);
			hdd_green_ap_start_state_mc(hdd_ctx,
						    ap_adapter->device_mode,
						    true);
		}
	}
end:
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

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		hdd_err("Invalid CDS Context");
		return;
	}

	if (policy_mgr_get_concurrency_mode(hdd_ctx->psoc)
		!= (QDF_STA_MASK | QDF_SAP_MASK)) {
		hdd_debug("Concurrency mode is not SAP");
		return;
	}

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (ap_adapter &&
	    test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags) &&
	    wlan_reg_is_dfs_ch(hdd_ctx->pdev,
			       ap_adapter->session.ap.operating_channel)) {
		if (policy_mgr_get_dfs_master_dynamic_enabled(
				hdd_ctx->psoc, ap_adapter->session_id))
			return;

		hdd_warn("STA-AP Mode DFS not supported, Switch SAP channel to Non DFS");

		restart_chan =
			wlansap_get_safe_channel_from_pcl_and_acs_range(
				WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter));
		if (!restart_chan ||
		    wlan_reg_is_dfs_ch(hdd_ctx->pdev, restart_chan))
			restart_chan = SAP_DEFAULT_5GHZ_CHANNEL;
		wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc,
					ap_adapter->session_id,
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
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return false;
	}

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

int wlan_hdd_send_p2p_quota(struct hdd_adapter *adapter, int set_value)
{
	if (!adapter) {
		hdd_err("Invalid adapter");
		return -EINVAL;
	}
	hdd_info("Send MCC P2P QUOTA to WMA: %d", set_value);
	sme_cli_set_command(adapter->session_id,
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
	sme_cli_set_command(adapter->session_id,
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
	struct csr_roam_profile *roam_profile;

	roam_profile = hdd_roam_profile(adapter);
	mac_handle = hdd_adapter_get_mac_handle(adapter);
	status = sme_get_rssi_snr_by_bssid(mac_handle,
					   roam_profile, bssid, rssi, snr);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_warn("sme_get_rssi_snr_by_bssid failed");
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_set_limit_off_chan_for_tos() - set limit off-channel command parameters
 * @adapter - HDD adapter
 * @tos - type of service
 * @status - status of the traffic
 *
 * Return: 0 on success and non zero value on failure
 */

int hdd_set_limit_off_chan_for_tos(struct hdd_adapter *adapter, enum tos tos,
		bool is_tos_active)
{
	int ac_bit;
	struct hdd_context *hdd_ctx;
	uint32_t max_off_chan_time = 0;
	QDF_STATUS status;
	int ret;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);

	if (ret < 0) {
		hdd_err("failed to set limit off chan params");
		return ret;
	}

	ac_bit = limit_off_chan_tbl[tos][HDD_AC_BIT_INDX];

	if (is_tos_active)
		adapter->active_ac |= ac_bit;
	else
		adapter->active_ac &= ~ac_bit;

	if (adapter->active_ac) {
		if (adapter->active_ac & HDD_AC_VO_BIT) {
			max_off_chan_time =
				limit_off_chan_tbl[TOS_VO][HDD_DWELL_TIME_INDX];
			policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
					PM_LATENCY);
		} else if (adapter->active_ac & HDD_AC_VI_BIT) {
			max_off_chan_time =
				limit_off_chan_tbl[TOS_VI][HDD_DWELL_TIME_INDX];
			policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
					PM_LATENCY);
		} else {
			/*ignore this command if only BE/BK is active */
			is_tos_active = false;
			policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
					hdd_ctx->config->conc_system_pref);
		}
	} else {
		/* No active tos */
		policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
				hdd_ctx->config->conc_system_pref);
	}

	status = sme_send_limit_off_channel_params(hdd_ctx->mac_handle,
						   adapter->session_id,
						   is_tos_active,
						   max_off_chan_time,
						   hdd_ctx->config->nRestTimeConc,
						   true);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("failed to set limit off chan params");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * hdd_reset_limit_off_chan() - reset limit off-channel command parameters
 * @adapter - HDD adapter
 *
 * Return: 0 on success and non zero value on failure
 */
int hdd_reset_limit_off_chan(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	int ret;
	QDF_STATUS status;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret < 0)
		return ret;

	/* set the system preferece to default */
	policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
			hdd_ctx->config->conc_system_pref);

	/* clear the bitmap */
	adapter->active_ac = 0;

	hdd_debug("reset ac_bitmap for session %hu active_ac %0x",
		  adapter->session_id, adapter->active_ac);

	status = sme_send_limit_off_channel_params(hdd_ctx->mac_handle,
						   adapter->session_id,
						   false, 0, 0, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("failed to reset limit off chan params");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * hdd_start_driver_ops_timer() - Starts driver ops inactivity timer
 * @drv_op: Enum indicating driver op
 *
 * Return: none
 */
void hdd_start_driver_ops_timer(int drv_op)
{
	memset(drv_ops_string, 0, MAX_OPS_NAME_STRING_SIZE);
	switch (drv_op) {
	case eHDD_DRV_OP_PROBE:
		memcpy(drv_ops_string, "probe", sizeof("probe"));
		break;
	case eHDD_DRV_OP_REMOVE:
		memcpy(drv_ops_string, "remove", sizeof("remove"));
		break;
	case eHDD_DRV_OP_SHUTDOWN:
		memcpy(drv_ops_string, "shutdown", sizeof("shutdown"));
		break;
	case eHDD_DRV_OP_REINIT:
		memcpy(drv_ops_string, "reinit", sizeof("reinit"));
		break;
	case eHDD_DRV_OP_IFF_UP:
		memcpy(drv_ops_string, "iff_up", sizeof("iff_up"));
		break;
	}

	hdd_drv_ops_task = current;
	qdf_timer_start(&hdd_drv_ops_inactivity_timer,
		HDD_OPS_INACTIVITY_TIMEOUT * qdf_timer_get_multiplier());
}

/**
 * hdd_stop_driver_ops_timer() - Stops driver ops inactivity timer
 *
 * Return: none
 */
void hdd_stop_driver_ops_timer(void)
{
	qdf_timer_sync_cancel(&hdd_drv_ops_inactivity_timer);
}

/**
 * hdd_drv_ops_inactivity_handler() - Timeout handler for driver ops
 * inactivity timer
 *
 * Return: None
 */
void hdd_drv_ops_inactivity_handler(void *context)
{
	hdd_err("WLAN_BUG_RCA %s: %d Sec timer expired while in .%s",
		__func__, HDD_OPS_INACTIVITY_TIMEOUT/1000, drv_ops_string);

	if (hdd_drv_ops_task) {
		printk("Call stack for \"%s\"\n", hdd_drv_ops_task->comm);
		qdf_print_thread_trace(hdd_drv_ops_task);
	} else {
		hdd_err("hdd_drv_ops_task is null");
	}

	/* Driver shutdown is stuck, no recovery possible at this point */
	if (0 == qdf_mem_cmp(&drv_ops_string[0], "shutdown",
		sizeof("shutdown")))
		QDF_BUG(0);

	if (cds_is_fw_down()) {
		hdd_err("FW is down");
		return;
	}

	cds_trigger_recovery(QDF_REASON_UNSPECIFIED);
}

void hdd_pld_ipa_uc_shutdown_pipes(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return;

	ucfg_ipa_uc_force_pipe_shutdown(hdd_ctx->pdev);
}

/**
 * hdd_set_rx_mode_rps() - Enable/disable RPS in SAP mode
 * @struct hdd_context *hdd_ctx
 * @struct hdd_adapter *padapter
 * @bool enble
 *
 * Return: none
 */
void hdd_set_rx_mode_rps(bool enable)
{
	struct cds_config_info *cds_cfg = cds_get_ini_config();
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	if (!cds_cfg)
		return;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (!adapter)
		return;

	if (!hdd_ctx->rps && cds_cfg->uc_offload_enabled) {
		if (enable && !cds_cfg->rps_enabled)
			hdd_send_rps_ind(adapter);
		else if (!enable && cds_cfg->rps_enabled)
			hdd_send_rps_disable_ind(adapter);
	}
}

#ifdef MSM_PLATFORM
void wlan_hdd_update_tcp_rx_param(struct hdd_context *hdd_ctx, void *data)
{
	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return;
	}

	if (!data) {
		hdd_err("Data is null");
		return;
	}
	if (hdd_ctx->config->enable_tcp_param_update)
		wlan_hdd_send_tcp_param_update_event(hdd_ctx, data, 1);
	else
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					    WLAN_SVC_WLAN_TP_IND,
					    data,
					    sizeof(struct wlan_rx_tp_data));
}

void wlan_hdd_update_tcp_tx_param(struct hdd_context *hdd_ctx, void *data)
{
	enum wlan_tp_level next_tx_level;
	struct wlan_tx_tp_data *tx_tp_data;

	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return;
	}

	if (!data) {
		hdd_err("Data is null");
		return;
	}

	tx_tp_data = (struct wlan_tx_tp_data *)data;
	next_tx_level = tx_tp_data->level;

	if (hdd_ctx->config->enable_tcp_param_update)
		wlan_hdd_send_tcp_param_update_event(hdd_ctx, data, 0);
	else
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					    WLAN_SVC_WLAN_TP_TX_IND,
					    &next_tx_level,
					    sizeof(next_tx_level));
}

/**
 * wlan_hdd_send_tcp_param_update_event() - Send vendor event to update
 * TCP parameter through Wi-Fi HAL
 * @hdd_ctx: Pointer to HDD context
 * @data: Parameters to update
 * @dir: Direction(tx/rx) to update
 *
 * Return: None
 */
void wlan_hdd_send_tcp_param_update_event(struct hdd_context *hdd_ctx,
					  void *data,
					  uint8_t dir)
{
	struct sk_buff *vendor_event;
	uint32_t event_len;
	bool tcp_limit_output = false;
	bool tcp_del_ack_ind_enabled = false;
	bool tcp_adv_win_scl_enabled = false;
	enum wlan_tp_level next_tp_level = WLAN_SVC_TP_NONE;

	event_len = sizeof(uint8_t) + sizeof(uint8_t) + NLMSG_HDRLEN;

	if (dir == 0) /*TX Flow */ {
		struct wlan_tx_tp_data *tx_tp_data =
				(struct wlan_tx_tp_data *)data;

		next_tp_level = tx_tp_data->level;

		if (tx_tp_data->tcp_limit_output) {
			/* TCP_LIMIT_OUTPUT_BYTES */
			event_len += sizeof(uint32_t);
			tcp_limit_output = true;
		}
	} else if (dir == 1) /* RX Flow */ {
		struct wlan_rx_tp_data *rx_tp_data =
				(struct wlan_rx_tp_data *)data;

		next_tp_level = rx_tp_data->level;

		if (rx_tp_data->rx_tp_flags & TCP_DEL_ACK_IND_MASK) {
			event_len += sizeof(uint32_t); /* TCP_DELACK_SEG */
			tcp_del_ack_ind_enabled = true;
		}
		if (rx_tp_data->rx_tp_flags & TCP_ADV_WIN_SCL_MASK) {
			event_len += sizeof(uint32_t); /* TCP_ADV_WIN_SCALE */
			tcp_adv_win_scl_enabled = true;
		}
	} else {
		hdd_err("Invalid Direction [%d]", dir);
		return;
	}

	vendor_event =
	cfg80211_vendor_event_alloc(
			hdd_ctx->wiphy,
			NULL, event_len,
			QCA_NL80211_VENDOR_SUBCMD_THROUGHPUT_CHANGE_EVENT_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u8(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_DIRECTION,
		dir))
		goto tcp_param_change_nla_failed;

	if (nla_put_u8(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_THROUGHPUT_LEVEL,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		QCA_WLAN_THROUGHPUT_LEVEL_LOW :
		QCA_WLAN_THROUGHPUT_LEVEL_HIGH)))
		goto tcp_param_change_nla_failed;

	if (tcp_limit_output &&
	    nla_put_u32(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_LIMIT_OUTPUT_BYTES,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		 TCP_LIMIT_OUTPUT_BYTES_LOW :
		 TCP_LIMIT_OUTPUT_BYTES_HI)))
		goto tcp_param_change_nla_failed;

	if (tcp_del_ack_ind_enabled &&
	    (nla_put_u32(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_DELACK_SEG,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		 TCP_DEL_ACK_LOW : TCP_DEL_ACK_HI))))
		goto tcp_param_change_nla_failed;

	if (tcp_adv_win_scl_enabled &&
	    (nla_put_u32(
		vendor_event,
		QCA_WLAN_VENDOR_ATTR_THROUGHPUT_CHANGE_TCP_ADV_WIN_SCALE,
		(next_tp_level == WLAN_SVC_TP_LOW ?
		 WIN_SCALE_LOW : WIN_SCALE_HI))))
		goto tcp_param_change_nla_failed;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	return;

tcp_param_change_nla_failed:
	hdd_err("nla_put api failed");
	kfree_skb(vendor_event);
}
#endif /* MSM_PLATFORM */

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
	wlan_hdd_enable_roaming(adapter);
}

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
void hdd_beacon_latency_event_cb(uint32_t latency_level)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err_rl("Invalid HDD_CTX");
		return;
	}
	wlan_hdd_set_wlm_mode(hdd_ctx, latency_level);
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE

/**
 * wlan_hdd_is_session_type_monitor() - check if session type is MONITOR
 * @session_type: session type
 *
 * Return: True - if session type for adapter is monitor, else False
 *
 */
bool wlan_hdd_is_session_type_monitor(uint8_t session_type)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		cds_err("HDD context is NULL");
		return false;
	}

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) &&
	    cds_get_conparam() != QDF_GLOBAL_MONITOR_MODE &&
	    session_type == QDF_MONITOR_MODE)
		return true;
	else
		return false;
}

/**
 * wlan_hdd_check_mon_concurrency() - check if MONITOR and STA concurrency
 * is UP when packet capture mode is enabled.
 * @void
 *
 * Return: True - if STA and monitor concurrency is there, else False
 *
 */
bool wlan_hdd_check_mon_concurrency(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return -EINVAL;
	}

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc)) {
		if (policy_mgr_get_concurrency_mode(hdd_ctx->psoc) ==
		    (QDF_STA_MASK | QDF_MONITOR_MASK)) {
			hdd_err("STA + MON mode is UP");
			return true;
		}
	}
	return false;
}

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
			  struct hdd_adapter *adapter, bool rtnl_held)
{
	wlan_hdd_release_intf_addr(hdd_ctx, adapter->mac_addr.bytes);
	hdd_stop_adapter(hdd_ctx, adapter);
	hdd_close_adapter(hdd_ctx, adapter, true);

	if (adapter->vdev) {
		hdd_objmgr_put_vdev(adapter->vdev);
		adapter->vdev = NULL;
	}

	hdd_open_p2p_interface(hdd_ctx, rtnl_held);
}

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
int
wlan_hdd_add_monitor_check(struct hdd_context *hdd_ctx,
			   struct hdd_adapter **adapter,
			   const char *name, bool rtnl_held,
			   unsigned char name_assign_type)
{
	struct hdd_adapter *sta_adapter;
	struct hdd_adapter *mon_adapter;
	uint32_t mode;
	uint8_t num_open_session = 0;
	QDF_STATUS status;

	/* if no interface is up do not add monitor mode */
	if (hdd_is_any_interface_open(hdd_ctx))
		return -EINVAL;

	/*
	 * If add interface request is for monitor mode, then it can run in
	 * parallel with only one station interface.
	 * If there is no existing station interface return error
	 */
	status = policy_mgr_mode_specific_num_open_sessions(hdd_ctx->psoc,
							    QDF_MONITOR_MODE,
							    &num_open_session);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	if (num_open_session) {
		hdd_err("monitor mode already exists, only one is possible");
		return -EBUSY;
	}

	/* Ensure there is only one station interface */
	status = policy_mgr_mode_specific_num_open_sessions(hdd_ctx->psoc,
							    QDF_STA_MODE,
							    &num_open_session);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	if (num_open_session != 1) {
		hdd_err("cannot add monitor mode, due to %u sta interfaces",
			num_open_session);
		return -EINVAL;
	}

	sta_adapter = hdd_get_adapter(hdd_ctx, QDF_STA_MODE);
	if (!sta_adapter) {
		hdd_err("No station adapter");
		return -EINVAL;
	}

	status = policy_mgr_mode_specific_num_open_sessions(hdd_ctx->psoc,
							    QDF_SAP_MODE,
							    &num_open_session);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	if (num_open_session) {
		hdd_err("cannot add monitor mode, due to SAP concurrency");
		return -EINVAL;
	}

	/* delete p2p interface */
	for (mode = QDF_P2P_CLIENT_MODE; mode <= QDF_P2P_DEVICE_MODE; mode++) {
		struct hdd_adapter *adapter;

		if (mode == QDF_FTM_MODE ||
		    mode == QDF_MONITOR_MODE ||
		    mode == QDF_IBSS_MODE)
			continue;

		adapter = hdd_get_adapter(hdd_ctx, mode);
		if (adapter) {
			wlan_hdd_release_intf_addr(hdd_ctx,
						   adapter->mac_addr.bytes);
			hdd_stop_adapter(hdd_ctx, adapter);
			hdd_deinit_adapter(hdd_ctx, adapter, true);
			hdd_close_adapter(hdd_ctx, adapter, rtnl_held);
		}
	}

	mon_adapter = hdd_open_adapter(hdd_ctx, QDF_MONITOR_MODE, name,
				       wlan_hdd_get_intf_addr(
				       hdd_ctx,
				       QDF_MONITOR_MODE),
				       name_assign_type, rtnl_held);
	if (!mon_adapter) {
		hdd_err("hdd_open_adapter failed");
		hdd_open_p2p_interface(hdd_ctx, rtnl_held);
		return -EINVAL;
	}

	*adapter = mon_adapter;
	return 0;
}
#endif /* WLAN_FEATURE_PKT_CAPTURE */
/* Register the module init/exit functions */
module_init(hdd_module_init);
module_exit(hdd_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Qualcomm Atheros, Inc.");
MODULE_DESCRIPTION("WLAN HOST DEVICE DRIVER");

static const struct kernel_param_ops con_mode_ops = {
	.set = con_mode_handler,
	.get = param_get_int,
};

static const struct kernel_param_ops con_mode_ftm_ops = {
	.set = con_mode_handler_ftm,
	.get = param_get_int,
};

#ifdef FEATURE_MONITOR_MODE_SUPPORT
static const struct kernel_param_ops con_mode_monitor_ops = {
	.set = con_mode_handler_monitor,
	.get = param_get_int,
};
#endif

static const struct kernel_param_ops fwpath_ops = {
	.set = fwpath_changed_handler,
	.get = param_get_string,
};

module_param_cb(con_mode, &con_mode_ops, &con_mode,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param_cb(con_mode_ftm, &con_mode_ftm_ops, &con_mode_ftm,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

#ifdef FEATURE_MONITOR_MODE_SUPPORT
module_param_cb(con_mode_monitor, &con_mode_monitor_ops, &con_mode_monitor,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif

module_param_cb(fwpath, &fwpath_ops, &fwpath,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param(enable_dfs_chan_scan, int, S_IRUSR | S_IRGRP | S_IROTH);

module_param(enable_11d, int, S_IRUSR | S_IRGRP | S_IROTH);

module_param(country_code, charp, S_IRUSR | S_IRGRP | S_IROTH);
