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
 * DOC:  wlan_hdd_hostapd.c
 *
 * WLAN Host Device Driver implementation
 */

/* Include Files */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <linux/semaphore.h>
#include <linux/compat.h>
#include <cdp_txrx_cmn.h>
#include <cds_api.h>
#include <cds_sched.h>
#include <linux/etherdevice.h>
#include "osif_sync.h"
#include <linux/ethtool.h>
#include <wlan_hdd_includes.h>
#include <qc_sap_ioctl.h>
#include "osif_sync.h"
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_hostapd_wext.h>
#include <wlan_hdd_green_ap.h>
#include <sap_api.h>
#include <sap_internal.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_ioctl.h>
#include <wlan_hdd_stats.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/mmc/sdio_func.h>
#include "wlan_hdd_p2p.h"
#include <wlan_hdd_ipa.h>
#include "wni_cfg.h"
#include "wlan_hdd_misc.h"
#include <cds_utils.h>
#include "pld_common.h"
#include "wma.h"
#ifdef WLAN_DEBUG
#include "wma_api.h"
#endif
#include "wlan_hdd_trace.h"
#include "qdf_str.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "qdf_net_if.h"
#include "wlan_hdd_cfg.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_hdd_tsf.h"
#include <cdp_txrx_misc.h>
#include <cdp_txrx_ctrl.h>
#include "wlan_hdd_object_manager.h"
#include <qca_vendor.h>
#include <cds_api.h>
#include "wlan_hdd_he.h"
#include "wlan_hdd_eht.h"
#include "wlan_dfs_tgt_api.h"
#include <wlan_reg_ucfg_api.h>
#include "wlan_utility.h"
#include <wlan_p2p_ucfg_api.h>
#include "sir_api.h"
#include "wlan_policy_mgr_ucfg.h"
#include "sme_api.h"
#include "wlan_hdd_regulatory.h"
#include <wlan_ipa_ucfg_api.h>
#include <wlan_cp_stats_mc_ucfg_api.h>
#include "wlan_mlme_ucfg_api.h"
#include "cfg_ucfg_api.h"
#include "wlan_crypto_global_api.h"
#include "wlan_action_oui_ucfg_api.h"
#include "wlan_fwol_ucfg_api.h"
#include "nan_ucfg_api.h"
#include <wlan_reg_services_api.h>
#include "wlan_hdd_sta_info.h"
#include "ftm_time_sync_ucfg_api.h"
#include <wlan_hdd_dcs.h>
#include "wlan_tdls_ucfg_api.h"
#include "wlan_mlme_twt_ucfg_api.h"
#include "wlan_if_mgr_ucfg_api.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_hdd_bootup_marker.h"
#include "wlan_hdd_medium_assess.h"
#include "wlan_hdd_scan.h"
#ifdef WLAN_FEATURE_11BE_MLO
#include <wlan_mlo_mgr_ap.h>
#endif
#include "spatial_reuse_ucfg_api.h"
#include "wlan_hdd_son.h"
#include "wlan_hdd_mcc_quota.h"
#include "wlan_hdd_wds.h"
#include "wlan_hdd_pre_cac.h"
#include "wlan_osif_features.h"
#include "wlan_pre_cac_ucfg_api.h"
#include <wlan_dp_ucfg_api.h>
#include "wlan_twt_ucfg_ext_api.h"
#include "wlan_twt_ucfg_api.h"
#include "wlan_vdev_mgr_ucfg_api.h"

#define ACS_SCAN_EXPIRY_TIMEOUT_S 4

/*
 * Defines the BIT position of 11bg/11abg/11abgn 802.11 support mode field
 * of stainfo
 */
#define HDD_80211_MODE_ABGN 0
/* Defines the BIT position of 11ac support mode field of stainfo */
#define HDD_80211_MODE_AC 1
/* Defines the BIT position of 11ax support mode field of stainfo */
#define HDD_80211_MODE_AX 2

#define HDD_MAX_CUSTOM_START_EVENT_SIZE 64

#ifdef NDP_SAP_CONCURRENCY_ENABLE
#define MAX_SAP_NUM_CONCURRENCY_WITH_NAN 2
#else
#define MAX_SAP_NUM_CONCURRENCY_WITH_NAN 1
#endif
#include "../../core/src/reg_priv_objs.h"

#ifndef BSS_MEMBERSHIP_SELECTOR_HT_PHY
#define BSS_MEMBERSHIP_SELECTOR_HT_PHY  127
#endif

#ifndef BSS_MEMBERSHIP_SELECTOR_VHT_PHY
#define BSS_MEMBERSHIP_SELECTOR_VHT_PHY 126
#endif

#ifndef BSS_MEMBERSHIP_SELECTOR_SAE_H2E
#define BSS_MEMBERSHIP_SELECTOR_SAE_H2E 123
#endif

#ifndef BSS_MEMBERSHIP_SELECTOR_HE_PHY
#define BSS_MEMBERSHIP_SELECTOR_HE_PHY  122
#endif

/*
 * 11B, 11G Rate table include Basic rate and Extended rate
 * The IDX field is the rate index
 * The HI field is the rate when RSSI is strong or being ignored
 * (in this case we report actual rate)
 * The MID field is the rate when RSSI is moderate
 * (in this case we cap 11b rates at 5.5 and 11g rates at 24)
 * The LO field is the rate when RSSI is low
 * (in this case we don't report rates, actual current rate used)
 */
static const struct index_data_rate_type supported_data_rate[] = {
	/* IDX     HI  HM  LM LO (RSSI-based index */
	{2,   { 10,  10, 10, 0} },
	{4,   { 20,  20, 10, 0} },
	{11,  { 55,  20, 10, 0} },
	{12,  { 60,  55, 20, 0} },
	{18,  { 90,  55, 20, 0} },
	{22,  {110,  55, 20, 0} },
	{24,  {120,  90, 60, 0} },
	{36,  {180, 120, 60, 0} },
	{44,  {220, 180, 60, 0} },
	{48,  {240, 180, 90, 0} },
	{66,  {330, 180, 90, 0} },
	{72,  {360, 240, 90, 0} },
	{96,  {480, 240, 120, 0} },
	{108, {540, 240, 120, 0} }
};

/* MCS Based rate table */
/* HT MCS parameters with Nss = 1 */
static const struct index_data_rate_type supported_mcs_rate_nss1[] = {
	/* MCS  L20   L40   S20  S40 */
	{0,  { 65,  135,  72,  150} },
	{1,  { 130, 270,  144, 300} },
	{2,  { 195, 405,  217, 450} },
	{3,  { 260, 540,  289, 600} },
	{4,  { 390, 810,  433, 900} },
	{5,  { 520, 1080, 578, 1200} },
	{6,  { 585, 1215, 650, 1350} },
	{7,  { 650, 1350, 722, 1500} }
};

/* HT MCS parameters with Nss = 2 */
static const struct index_data_rate_type supported_mcs_rate_nss2[] = {
	/* MCS  L20    L40   S20   S40 */
	{0,  {130,  270,  144,  300} },
	{1,  {260,  540,  289,  600} },
	{2,  {390,  810,  433,  900} },
	{3,  {520,  1080, 578,  1200} },
	{4,  {780,  1620, 867,  1800} },
	{5,  {1040, 2160, 1156, 2400} },
	{6,  {1170, 2430, 1300, 2700} },
	{7,  {1300, 2700, 1444, 3000} }
};

/* MCS Based VHT rate table */
/* MCS parameters with Nss = 1*/
static const struct index_vht_data_rate_type supported_vht_mcs_rate_nss1[] = {
	/* MCS  L80    S80     L40   S40    L20   S40*/
	{0,  {293,  325},  {135,  150},  {65,   72} },
	{1,  {585,  650},  {270,  300},  {130,  144} },
	{2,  {878,  975},  {405,  450},  {195,  217} },
	{3,  {1170, 1300}, {540,  600},  {260,  289} },
	{4,  {1755, 1950}, {810,  900},  {390,  433} },
	{5,  {2340, 2600}, {1080, 1200}, {520,  578} },
	{6,  {2633, 2925}, {1215, 1350}, {585,  650} },
	{7,  {2925, 3250}, {1350, 1500}, {650,  722} },
	{8,  {3510, 3900}, {1620, 1800}, {780,  867} },
	{9,  {3900, 4333}, {1800, 2000}, {780,  867} },
	{10, {4388, 4875}, {2025, 2250}, {975, 1083} },
	{11, {4875, 5417}, {2250, 2500}, {1083, 1203} }
};

/*MCS parameters with Nss = 2*/
static const struct index_vht_data_rate_type supported_vht_mcs_rate_nss2[] = {
	/* MCS  L80    S80     L40   S40    L20   S40*/
	{0,  {585,  650},  {270,  300},  {130,  144} },
	{1,  {1170, 1300}, {540,  600},  {260,  289} },
	{2,  {1755, 1950}, {810,  900},  {390,  433} },
	{3,  {2340, 2600}, {1080, 1200}, {520,  578} },
	{4,  {3510, 3900}, {1620, 1800}, {780,  867} },
	{5,  {4680, 5200}, {2160, 2400}, {1040, 1156} },
	{6,  {5265, 5850}, {2430, 2700}, {1170, 1300} },
	{7,  {5850, 6500}, {2700, 3000}, {1300, 1444} },
	{8,  {7020, 7800}, {3240, 3600}, {1560, 1733} },
	{9,  {7800, 8667}, {3600, 4000}, {1730, 1920} },
	{10, {8775, 9750}, {4050, 4500}, {1950, 2167} },
	{11, {9750, 10833}, {4500, 5000}, {2167, 2407} }
};

/* Function definitions */

/**
 * hdd_sap_context_init() - Initialize SAP context.
 * @hdd_ctx:	HDD context.
 *
 * Initialize SAP context.
 *
 * Return: 0 on success.
 */
int hdd_sap_context_init(struct hdd_context *hdd_ctx)
{
	qdf_wake_lock_create(&hdd_ctx->sap_dfs_wakelock, "sap_dfs_wakelock");
	atomic_set(&hdd_ctx->sap_dfs_ref_cnt, 0);

	mutex_init(&hdd_ctx->sap_lock);
	qdf_wake_lock_create(&hdd_ctx->sap_wake_lock, "qcom_sap_wakelock");

	return 0;
}

/**
 * hdd_hostapd_init_sap_session() - To init the sap session completely
 * @adapter: SAP/GO adapter
 * @reinit: if called as part of reinit
 *
 * This API will do
 * 1) sap_init_ctx()
 *
 * Return: 0 if success else non-zero value.
 */
static struct sap_context *
hdd_hostapd_init_sap_session(struct hdd_adapter *adapter, bool reinit)
{
	struct sap_context *sap_ctx;
	QDF_STATUS status;

	if (!adapter) {
		hdd_err("invalid adapter");
		return NULL;
	}

	sap_ctx = adapter->session.ap.sap_context;

	if (!sap_ctx) {
		hdd_err("can't allocate the sap_ctx");
		return NULL;
	}
	status = sap_init_ctx(sap_ctx, adapter->device_mode,
			       adapter->mac_addr.bytes,
			       adapter->vdev_id, reinit);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("wlansap_start failed!! status: %d", status);
		adapter->session.ap.sap_context = NULL;
		goto error;
	}
	return sap_ctx;
error:
	wlansap_context_put(sap_ctx);
	hdd_err("releasing the sap context for session-id:%d",
		adapter->vdev_id);

	return NULL;
}

/**
 * hdd_hostapd_deinit_sap_session() - To de-init the sap session completely
 * @adapter: SAP/GO adapter
 *
 * This API will do
 * 1) sap_init_ctx()
 * 2) sap_destroy_ctx()
 *
 * Return: 0 if success else non-zero value.
 */
static int hdd_hostapd_deinit_sap_session(struct hdd_adapter *adapter)
{
	struct sap_context *sap_ctx;
	int status = 0;

	if (!adapter) {
		hdd_err("invalid adapter");
		return -EINVAL;
	}

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	if (!sap_ctx) {
		hdd_debug("sap context already released, nothing to be done");
		return 0;
	}

	wlan_hdd_undo_acs(adapter);
	if (!QDF_IS_STATUS_SUCCESS(sap_deinit_ctx(sap_ctx))) {
		hdd_err("Error stopping the sap session");
		status = -EINVAL;
	}

	if (!hdd_sap_destroy_ctx(adapter)) {
		hdd_err("Error closing the sap session");
		status = -EINVAL;
	}

	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_debug("sap has issue closing the session");
	else
		hdd_debug("sap has been closed successfully");


	return status;
}

/**
 * hdd_hostapd_channel_allow_suspend() - allow suspend in a channel.
 * Called when, 1. bss stopped, 2. channel switch
 *
 * @adapter: pointer to hdd adapter
 * @chan_freq: current channel frequency
 *
 * Return: None
 */
static void hdd_hostapd_channel_allow_suspend(struct hdd_adapter *adapter,
					      uint32_t chan_freq)
{

	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_hostapd_state *hostapd_state =
		WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

	hdd_debug("bss_state: %d, chan_freq: %d, dfs_ref_cnt: %d",
		  hostapd_state->bss_state, chan_freq,
		  atomic_read(&hdd_ctx->sap_dfs_ref_cnt));

	/* Return if BSS is already stopped */
	if (hostapd_state->bss_state == BSS_STOP)
		return;

	if (!wlan_reg_chan_has_dfs_attribute_for_freq(hdd_ctx->pdev,
						      chan_freq))
		return;

	/* Release wakelock when no more DFS channels are used */
	if (atomic_dec_and_test(&hdd_ctx->sap_dfs_ref_cnt)) {
		hdd_err("DFS: allowing suspend (chan_freq: %d)", chan_freq);
		qdf_wake_lock_release(&hdd_ctx->sap_dfs_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_DFS);
		qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.dfs);

	}
}

/**
 * hdd_hostapd_channel_prevent_suspend() - prevent suspend in a channel.
 * Called when, 1. bss started, 2. channel switch
 *
 * @adapter: pointer to hdd adapter
 * @chan_freq: current channel frequency
 *
 * Return - None
 */
static void hdd_hostapd_channel_prevent_suspend(struct hdd_adapter *adapter,
						uint32_t chan_freq)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_hostapd_state *hostapd_state =
		WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

	hdd_debug("bss_state: %d, chan_freq: %d, dfs_ref_cnt: %d",
		  hostapd_state->bss_state, chan_freq,
		  atomic_read(&hdd_ctx->sap_dfs_ref_cnt));
	/* Return if BSS is already started && wakelock is acquired */
	if ((hostapd_state->bss_state == BSS_START) &&
		(atomic_read(&hdd_ctx->sap_dfs_ref_cnt) >= 1))
		return;

	if (!wlan_reg_chan_has_dfs_attribute_for_freq(hdd_ctx->pdev,
						      chan_freq))
		return;

	/* Acquire wakelock if we have at least one DFS channel in use */
	if (atomic_inc_return(&hdd_ctx->sap_dfs_ref_cnt) == 1) {
		hdd_err("DFS: preventing suspend (chan_freq: %d)", chan_freq);
		qdf_runtime_pm_prevent_suspend(&hdd_ctx->runtime_context.dfs);
		qdf_wake_lock_acquire(&hdd_ctx->sap_dfs_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_DFS);
	}
}

/**
 * hdd_sap_context_destroy() - Destroy SAP context
 *
 * @hdd_ctx:	HDD context.
 *
 * Destroy SAP context.
 *
 * Return: None
 */
void hdd_sap_context_destroy(struct hdd_context *hdd_ctx)
{
	if (atomic_read(&hdd_ctx->sap_dfs_ref_cnt)) {
		qdf_wake_lock_release(&hdd_ctx->sap_dfs_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_DRIVER_EXIT);

		atomic_set(&hdd_ctx->sap_dfs_ref_cnt, 0);
		hdd_debug("DFS: Allowing suspend");
	}

	qdf_wake_lock_destroy(&hdd_ctx->sap_dfs_wakelock);

	mutex_destroy(&hdd_ctx->sap_lock);
	qdf_wake_lock_destroy(&hdd_ctx->sap_wake_lock);
}

/**
 * __hdd_hostapd_open() - hdd open function for hostapd interface
 * This is called in response to ifconfig up
 * @dev: pointer to net_device structure
 *
 * Return - 0 for success non-zero for failure
 */
static int __hdd_hostapd_open(struct net_device *dev)
{
	struct hdd_adapter *adapter = netdev_priv(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;

	hdd_enter_dev(dev);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_HOSTAPD_OPEN_REQUEST,
		   NO_SESSION, 0);

	/* Nothing to be done if device is unloading */
	if (cds_is_driver_unloading()) {
		hdd_err("Driver is unloading can not open the hdd");
		return -EBUSY;
	}

	if (cds_is_driver_recovering()) {
		hdd_err("WLAN is currently recovering; Please try again.");
		return -EBUSY;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	/* ensure the physical soc is up */
	ret = hdd_trigger_psoc_idle_restart(hdd_ctx);
	if (ret) {
		hdd_err("Failed to start WLAN modules return");
		return ret;
	}

	ret = hdd_start_adapter(adapter);
	if (ret) {
		hdd_err("Error Initializing the AP mode: %d", ret);
		return ret;
	}

	set_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);

	/* Enable all Tx queues */
	hdd_debug("Enabling queues");
	wlan_hdd_netif_queue_control(adapter,
				   WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
				   WLAN_CONTROL_PATH);
	hdd_exit();
	return 0;
}

/**
 * hdd_hostapd_open() - SSR wrapper for __hdd_hostapd_open
 * @net_dev: pointer to net device
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_hostapd_open(struct net_device *net_dev)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_trans_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_hostapd_open(net_dev);
	if (!errno)
		osif_vdev_cache_command(vdev_sync, NO_COMMAND);
	osif_vdev_sync_trans_stop(vdev_sync);

	return errno;
}

int hdd_hostapd_stop_no_trans(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;

	hdd_enter_dev(dev);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_HOSTAPD_STOP_REQUEST,
		   NO_SESSION, 0);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	/*
	 * Some tests requires to do "ifconfig down" only to bring
	 * down the SAP/GO without killing hostapd/wpa_supplicant.
	 * In such case, user will do "ifconfig up" to bring-back
	 * the SAP/GO session. to fulfill this requirement, driver
	 * needs to de-init the sap session here and re-init when
	 * __hdd_hostapd_open() API
	 */
	hdd_stop_adapter(hdd_ctx, adapter);
	hdd_deinit_adapter(hdd_ctx, adapter, true);
	clear_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);
	/* Stop all tx queues */
	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	if (!hdd_is_any_interface_open(hdd_ctx))
		hdd_psoc_idle_timer_start(hdd_ctx);

	hdd_exit();
	return 0;
}

/**
 * hdd_hostapd_stop() - SSR wrapper for__hdd_hostapd_stop
 * @net_dev: pointer to net_device
 *
 * This is called in response to ifconfig down
 *
 * Return: 0 on success, error number otherwise
 */
int hdd_hostapd_stop(struct net_device *net_dev)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_trans_start(net_dev, &vdev_sync);
	if (errno) {
		if (vdev_sync)
			osif_vdev_cache_command(vdev_sync, INTERFACE_DOWN);
		return errno;
	}

	errno = hdd_hostapd_stop_no_trans(net_dev);

	osif_vdev_sync_trans_stop(vdev_sync);

	return errno;
}

/**
 * hdd_hostapd_uninit() - hdd uninit function
 * @dev: pointer to net_device structure
 *
 * This is called during the netdev unregister to uninitialize all data
 * associated with the device.
 *
 * This function must be protected by a transition
 *
 * Return: None
 */
static void hdd_hostapd_uninit(struct net_device *dev)
{
	struct hdd_adapter *adapter = netdev_priv(dev);
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err("Invalid magic");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("NULL hdd_ctx");
		return;
	}

	hdd_deinit_adapter(hdd_ctx, adapter, true);

	/* after uninit our adapter structure will no longer be valid */
	adapter->magic = 0;

	hdd_exit();
}

/**
 * __hdd_hostapd_change_mtu() - change mtu
 * @dev: pointer to net_device
 * @new_mtu: new mtu
 *
 * Return: 0 on success, error number otherwise
 */
static int __hdd_hostapd_change_mtu(struct net_device *dev, int new_mtu)
{
	hdd_enter_dev(dev);

	return 0;
}

/**
 * hdd_hostapd_change_mtu() - SSR wrapper for __hdd_hostapd_change_mtu
 * @net_dev: pointer to net_device
 * @new_mtu: new mtu
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_hostapd_change_mtu(struct net_device *net_dev, int new_mtu)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_hostapd_change_mtu(net_dev, new_mtu);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

#ifdef QCA_HT_2040_COEX
QDF_STATUS hdd_set_sap_ht2040_mode(struct hdd_adapter *adapter,
				   uint8_t channel_type)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	mac_handle_t mac_handle;

	hdd_debug("change HT20/40 mode");

	if (QDF_SAP_MODE == adapter->device_mode) {
		mac_handle = adapter->hdd_ctx->mac_handle;
		if (!mac_handle) {
			hdd_err("mac handle is null");
			return QDF_STATUS_E_FAULT;
		}
		qdf_ret_status =
			sme_set_ht2040_mode(mac_handle, adapter->vdev_id,
					    channel_type, true);
		if (qdf_ret_status == QDF_STATUS_E_FAILURE) {
			hdd_err("Failed to change HT20/40 mode");
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_get_sap_ht2040_mode(struct hdd_adapter *adapter,
				   enum eSirMacHTChannelType *channel_type)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	mac_handle_t mac_handle;

	hdd_debug("get HT20/40 mode vdev_id %d", adapter->vdev_id);

	if (adapter->device_mode == QDF_SAP_MODE) {
		mac_handle = adapter->hdd_ctx->mac_handle;
		if (!mac_handle) {
			hdd_err("mac handle is null");
			return status;
		}
		status = sme_get_ht2040_mode(mac_handle, adapter->vdev_id,
					     channel_type);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("Failed to get HT20/40 mode");
	}

	return status;
}
#endif

/**
 * __hdd_hostapd_set_mac_address() -
 * This function sets the user specified mac address using
 * the command ifconfig wlanX hw ether <mac address>.
 *
 * @dev: pointer to the net device.
 * @addr: pointer to the sockaddr.
 *
 * Return: 0 for success, non zero for failure
 */
static int __hdd_hostapd_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *psta_mac_addr = addr;
	struct hdd_adapter *adapter, *adapter_temp;
	struct hdd_context *hdd_ctx;
	int ret = 0;
	struct qdf_mac_addr mac_addr;

	hdd_enter_dev(dev);

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

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

	if (qdf_is_macaddr_zero(&mac_addr)) {
		hdd_err("MAC is all zero");
		return -EINVAL;
	}

	if (qdf_is_macaddr_broadcast(&mac_addr)) {
		hdd_err("MAC is Broadcast");
		return -EINVAL;
	}

	if (qdf_is_macaddr_group(&mac_addr)) {
		hdd_err("MAC is Multicast");
		return -EINVAL;
	}

	hdd_debug("Changing MAC to " QDF_MAC_ADDR_FMT " of interface %s ",
		  QDF_MAC_ADDR_REF(mac_addr.bytes),
		  dev->name);

	if (adapter->vdev) {
		if (!hdd_is_dynamic_set_mac_addr_allowed(adapter))
			return -ENOTSUPP;

		ret = hdd_dynamic_mac_address_set(hdd_ctx, adapter, mac_addr);
		if (ret)
			return ret;
	}

	hdd_set_mld_address(adapter, &mac_addr);

	/* Currently for SL-ML-SAP use same MAC for both MLD and link */
	hdd_update_dynamic_mac(hdd_ctx, &adapter->mac_addr, &mac_addr);
	ucfg_dp_update_inf_mac(hdd_ctx->psoc, &adapter->mac_addr, &mac_addr);
	memcpy(&adapter->mac_addr, psta_mac_addr->sa_data, ETH_ALEN);
	qdf_net_update_net_device_dev_addr(dev, psta_mac_addr->sa_data,
					   ETH_ALEN);
	hdd_exit();
	return 0;
}

/**
 * hdd_hostapd_set_mac_address() - set mac address
 * @net_dev: pointer to net_device
 * @addr: mac address
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_hostapd_set_mac_address(struct net_device *net_dev, void *addr)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_hostapd_set_mac_address(net_dev, addr);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static void hdd_clear_sta(struct hdd_adapter *adapter,
			  struct hdd_station_info *sta_info)
{
	struct hdd_ap_ctx *ap_ctx;
	struct csr_del_sta_params del_sta_params;

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

	if (qdf_is_macaddr_broadcast(&sta_info->sta_mac))
		return;

	wlansap_populate_del_sta_params(sta_info->sta_mac.bytes,
					REASON_DEAUTH_NETWORK_LEAVING,
					SIR_MAC_MGMT_DISASSOC,
					&del_sta_params);

	hdd_softap_sta_disassoc(adapter, &del_sta_params);
}

static void hdd_clear_all_sta(struct hdd_adapter *adapter)
{
	struct hdd_station_info *sta_info, *tmp = NULL;

	hdd_enter_dev(adapter->dev);

	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_HDD_CLEAR_ALL_STA) {
		hdd_clear_sta(adapter, sta_info);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_HDD_CLEAR_ALL_STA);
	}
}

static int hdd_stop_bss_link(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	int errno;
	QDF_STATUS status;

	hdd_enter();

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		status = wlansap_stop_bss(
			WLAN_HDD_GET_SAP_CTX_PTR(adapter));
		if (QDF_IS_STATUS_SUCCESS(status))
			hdd_debug("Deleting SAP/P2P link!!!!!!");

		clear_bit(SOFTAP_BSS_STARTED, &adapter->event_flags);
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
					adapter->device_mode,
					adapter->vdev_id);
		hdd_green_ap_start_state_mc(hdd_ctx, adapter->device_mode,
					    false);
		errno = (status == QDF_STATUS_SUCCESS) ? 0 : -EBUSY;
	}
	hdd_exit();
	return errno;
}

#if defined(WLAN_FEATURE_11BE) && defined(CFG80211_11BE_BASIC)
static void
wlan_hdd_set_chandef_320mhz(struct cfg80211_chan_def *chandef,
			    struct hdd_chan_change_params chan_change)
{
	if (chan_change.chan_params.ch_width != CH_WIDTH_320MHZ)
		return;

	chandef->width = NL80211_CHAN_WIDTH_320;
	if (chan_change.chan_params.mhz_freq_seg1)
		chandef->center_freq1 = chan_change.chan_params.mhz_freq_seg1;
}

static void wlan_hdd_set_chandef_width(struct cfg80211_chan_def *chandef,
				       enum phy_ch_width width)
{
	if (width == CH_WIDTH_320MHZ)
		chandef->width = NL80211_CHAN_WIDTH_320;
}

static inline bool wlan_hdd_is_chwidth_320mhz(enum phy_ch_width ch_width)
{
	return ch_width == CH_WIDTH_320MHZ;
}

static uint16_t
wlan_hdd_get_puncture_bitmap(struct hdd_chan_change_params chan_change)
{
	return chan_change.chan_params.reg_punc_bitmap;
}
#else /* !WLAN_FEATURE_11BE */
static inline
void wlan_hdd_set_chandef_320mhz(struct cfg80211_chan_def *chandef,
				 struct hdd_chan_change_params chan_change)
{
}

static inline void wlan_hdd_set_chandef_width(struct cfg80211_chan_def *chandef,
					      enum phy_ch_width width)
{
}

static inline bool wlan_hdd_is_chwidth_320mhz(enum phy_ch_width ch_width)
{
	return false;
}

static inline uint16_t
wlan_hdd_get_puncture_bitmap(struct hdd_chan_change_params chan_change)
{
	return 0;
}
#endif /* WLAN_FEATURE_11BE */

QDF_STATUS hdd_chan_change_notify(struct hdd_adapter *adapter,
		struct net_device *dev,
		struct hdd_chan_change_params chan_change,
		bool legacy_phymode)
{
	struct ieee80211_channel *chan;
	struct cfg80211_chan_def chandef;
	enum nl80211_channel_type channel_type;
	uint32_t freq;
	mac_handle_t mac_handle = adapter->hdd_ctx->mac_handle;
	struct wlan_objmgr_vdev *vdev;
	uint16_t link_id = 0;
	uint16_t puncture_bitmap = 0;
	struct hdd_adapter *assoc_adapter;

	if (!mac_handle) {
		hdd_err("mac_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	freq = chan_change.chan_freq;

	chan = ieee80211_get_channel(adapter->wdev.wiphy, freq);

	if (!chan) {
		hdd_err("Invalid input frequency %d for channel conversion",
			freq);
		return QDF_STATUS_E_FAILURE;
	}

	if (legacy_phymode) {
		channel_type = NL80211_CHAN_NO_HT;
	} else {
		switch (chan_change.chan_params.sec_ch_offset) {
		case PHY_SINGLE_CHANNEL_CENTERED:
			channel_type = NL80211_CHAN_HT20;
			break;
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			channel_type = NL80211_CHAN_HT40MINUS;
			break;
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
			channel_type = NL80211_CHAN_HT40PLUS;
			break;
		default:
			channel_type = NL80211_CHAN_NO_HT;
			break;
		}
	}

	cfg80211_chandef_create(&chandef, chan, channel_type);

	/* cfg80211_chandef_create() does update of width and center_freq1
	 * only for NL80211_CHAN_NO_HT, NL80211_CHAN_HT20, NL80211_CHAN_HT40PLUS
	 * and NL80211_CHAN_HT40MINUS.
	 */
	switch (chan_change.chan_params.ch_width) {
	case CH_WIDTH_80MHZ:
		chandef.width = NL80211_CHAN_WIDTH_80;
		break;
	case CH_WIDTH_80P80MHZ:
		chandef.width = NL80211_CHAN_WIDTH_80P80;
		if (chan_change.chan_params.mhz_freq_seg1)
			chandef.center_freq2 =
				chan_change.chan_params.mhz_freq_seg1;
		break;
	case CH_WIDTH_160MHZ:
		chandef.width = NL80211_CHAN_WIDTH_160;
		if (chan_change.chan_params.mhz_freq_seg1)
			chandef.center_freq1 =
				chan_change.chan_params.mhz_freq_seg1;
		break;
	default:
		break;
	}

	wlan_hdd_set_chandef_320mhz(&chandef, chan_change);

	if ((chan_change.chan_params.ch_width == CH_WIDTH_80MHZ) ||
	    (chan_change.chan_params.ch_width == CH_WIDTH_80P80MHZ)) {
		if (chan_change.chan_params.mhz_freq_seg0)
			chandef.center_freq1 =
				chan_change.chan_params.mhz_freq_seg0;
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	if (!vdev)
		return -EINVAL;

	if (wlan_vdev_mlme_is_mlo_vdev(vdev))
		link_id = wlan_vdev_get_link_id(vdev);
	hdd_debug("link_id is %d", link_id);

	puncture_bitmap = wlan_hdd_get_puncture_bitmap(chan_change);

	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
	hdd_debug("notify: chan:%d width:%d freq1:%d freq2:%d",
		  chandef.chan->center_freq, chandef.width,
		  chandef.center_freq1, chandef.center_freq2);

	if (hdd_adapter_is_link_adapter(adapter)) {
		hdd_debug("replace link adapter dev with ml adapter dev");
		assoc_adapter = hdd_adapter_get_mlo_adapter_from_link(adapter);
		if (!assoc_adapter) {
			hdd_err("Assoc adapter is NULL");
			return -EINVAL;
		}
		dev = assoc_adapter->dev;
	}

	wlan_cfg80211_ch_switch_notify(dev, &chandef, link_id,
				       puncture_bitmap);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_send_radar_event() - Function to send radar events to user space
 * @hdd_context:	HDD context
 * @event:		Type of radar event
 * @dfs_info:		Structure containing DFS channel and country
 * @wdev:		Wireless device structure
 *
 * This function is used to send radar events such as CAC start, CAC
 * end etc., to userspace
 *
 * Return: Success on sending notifying userspace
 *
 */
static QDF_STATUS hdd_send_radar_event(struct hdd_context *hdd_context,
				       eSapHddEvent event,
				       struct wlan_dfs_info dfs_info,
				       struct wireless_dev *wdev)
{

	struct sk_buff *vendor_event;
	enum qca_nl80211_vendor_subcmds_index index;
	uint32_t freq, ret;
	uint32_t data_size;

	if (!hdd_context) {
		hdd_err("HDD context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	freq = cds_chan_to_freq(dfs_info.channel);

	switch (event) {
	case eSAP_DFS_CAC_START:
		index =
		    QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED_INDEX;
		data_size = sizeof(uint32_t);
		break;
	case eSAP_DFS_CAC_END:
		index =
		    QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_FINISHED_INDEX;
		data_size = sizeof(uint32_t);
		break;
	case eSAP_DFS_RADAR_DETECT:
		index =
		    QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_RADAR_DETECTED_INDEX;
		data_size = sizeof(uint32_t);
		break;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	vendor_event = wlan_cfg80211_vendor_event_alloc(hdd_context->wiphy,
							wdev,
							data_size +
							NLMSG_HDRLEN,
							index, GFP_KERNEL);
	if (!vendor_event) {
		hdd_err("wlan_cfg80211_vendor_event_alloc failed for %d",
			index);
		return QDF_STATUS_E_FAILURE;
	}

	ret = nla_put_u32(vendor_event, NL80211_ATTR_WIPHY_FREQ, freq);

	if (ret) {
		hdd_err("NL80211_ATTR_WIPHY_FREQ put fail");
		wlan_cfg80211_vendor_free_skb(vendor_event);
		return QDF_STATUS_E_FAILURE;
	}

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
/**
 * hdd_handle_acs_scan_event() - handle acs scan event for SAP
 * @sap_event: tpSap_Event
 * @adapter: struct hdd_adapter for SAP
 *
 * The function is to handle the eSAP_ACS_SCAN_SUCCESS_EVENT event.
 * It will update scan result to cfg80211 and start a timer to flush the
 * cached acs scan result.
 *
 * Return: QDF_STATUS_SUCCESS on success,
 *      other value on failure
 */
static QDF_STATUS hdd_handle_acs_scan_event(struct sap_event *sap_event,
		struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	struct sap_acs_scan_complete_event *comp_evt;
	QDF_STATUS qdf_status;
	int freq_list_size;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return QDF_STATUS_E_FAILURE;
	}
	comp_evt = &sap_event->sapevt.sap_acs_scan_comp;
	hdd_ctx->skip_acs_scan_status = eSAP_SKIP_ACS_SCAN;
	qdf_spin_lock(&hdd_ctx->acs_skip_lock);
	qdf_mem_free(hdd_ctx->last_acs_freq_list);
	hdd_ctx->last_acs_freq_list = NULL;
	hdd_ctx->num_of_channels = 0;
	/* cache the previous ACS scan channel list .
	 * If the following OBSS scan chan list is covered by ACS chan list,
	 * we can skip OBSS Scan to save SAP starting total time.
	 */
	if (comp_evt->num_of_channels && comp_evt->freq_list) {
		freq_list_size = comp_evt->num_of_channels *
			sizeof(comp_evt->freq_list[0]);
		hdd_ctx->last_acs_freq_list = qdf_mem_malloc(
			freq_list_size);
		if (hdd_ctx->last_acs_freq_list) {
			qdf_mem_copy(hdd_ctx->last_acs_freq_list,
				     comp_evt->freq_list,
				     freq_list_size);
			hdd_ctx->num_of_channels = comp_evt->num_of_channels;
		}
	}
	qdf_spin_unlock(&hdd_ctx->acs_skip_lock);

	hdd_debug("Reusing Last ACS scan result for %d sec",
		ACS_SCAN_EXPIRY_TIMEOUT_S);
	qdf_mc_timer_stop(&hdd_ctx->skip_acs_scan_timer);
	qdf_status = qdf_mc_timer_start(&hdd_ctx->skip_acs_scan_timer,
			ACS_SCAN_EXPIRY_TIMEOUT_S * 1000);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		hdd_err("Failed to start ACS scan expiry timer");
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS hdd_handle_acs_scan_event(struct sap_event *sap_event,
		struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * get_max_rate_vht() - calculate max rate for VHT mode
 * @nss: num of streams
 * @ch_width: channel width
 * @sgi: short gi
 * @vht_mcs_map: vht mcs map
 *
 * This function calculate max rate for VHT mode
 *
 * Return: max rate
 */
static int get_max_rate_vht(int nss, int ch_width, int sgi, int vht_mcs_map)
{
	const struct index_vht_data_rate_type *supported_vht_mcs_rate;
	enum data_rate_11ac_max_mcs vht_max_mcs;
	int maxrate = 0;
	int maxidx;

	if (nss == 1) {
		supported_vht_mcs_rate = supported_vht_mcs_rate_nss1;
	} else if (nss == 2) {
		supported_vht_mcs_rate = supported_vht_mcs_rate_nss2;
	} else {
		/* Not Supported */
		hdd_debug("nss %d not supported", nss);
		return maxrate;
	}

	vht_max_mcs =
		(enum data_rate_11ac_max_mcs)
		(vht_mcs_map & DATA_RATE_11AC_MCS_MASK);

	if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_7) {
		maxidx = 7;
	} else if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_8) {
		maxidx = 8;
	} else if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_9) {
		if (ch_width == eHT_CHANNEL_WIDTH_20MHZ)
			/* MCS9 is not valid for VHT20 when nss=1,2 */
			maxidx = 8;
		else
			maxidx = 9;
	} else {
		hdd_err("vht mcs map %x not supported",
			vht_mcs_map & DATA_RATE_11AC_MCS_MASK);
		return maxrate;
	}

	if (ch_width == eHT_CHANNEL_WIDTH_20MHZ) {
		maxrate =
		supported_vht_mcs_rate[maxidx].supported_VHT20_rate[sgi];
	} else if (ch_width == eHT_CHANNEL_WIDTH_40MHZ) {
		maxrate =
		supported_vht_mcs_rate[maxidx].supported_VHT40_rate[sgi];
	} else if (ch_width == eHT_CHANNEL_WIDTH_80MHZ) {
		maxrate =
		supported_vht_mcs_rate[maxidx].supported_VHT80_rate[sgi];
	} else {
		hdd_err("ch_width %d not supported", ch_width);
		return maxrate;
	}

	return maxrate;
}

/**
 * calculate_max_phy_rate() - calculate maximum phy rate (100kbps)
 * @mode: phymode: Legacy, 11a/b/g, HT, VHT
 * @nss: num of stream (maximum num is 2)
 * @ch_width: channel width
 * @sgi: short gi enabled or not
 * @supp_idx: max supported idx
 * @ext_idx: max extended idx
 * @ht_mcs_idx: max mcs index for HT
 * @vht_mcs_map: mcs map for VHT
 *
 * return: maximum phy rate in 100kbps
 */
static int calculate_max_phy_rate(int mode, int nss, int ch_width,
				 int sgi, int supp_idx, int ext_idx,
				 int ht_mcs_idx, int vht_mcs_map)
{
	const struct index_data_rate_type *supported_mcs_rate;
	int maxidx = 12; /*default 6M mode*/
	int maxrate = 0, tmprate;
	int i;

	/* check supported rates */
	if (supp_idx != 0xff && maxidx < supp_idx)
		maxidx = supp_idx;

	/* check extended rates */
	if (ext_idx != 0xff && maxidx < ext_idx)
		maxidx = ext_idx;

	for (i = 0; i < QDF_ARRAY_SIZE(supported_data_rate); i++) {
		if (supported_data_rate[i].beacon_rate_index == maxidx)
			maxrate = supported_data_rate[i].supported_rate[0];
	}

	if (mode == SIR_SME_PHY_MODE_HT) {
		/* check for HT Mode */
		maxidx = ht_mcs_idx;
		if (maxidx > 7) {
			hdd_err("ht_mcs_idx %d is incorrect", ht_mcs_idx);
			return maxrate;
		}
		if (nss == 1) {
			supported_mcs_rate = supported_mcs_rate_nss1;
		} else if (nss == 2) {
			supported_mcs_rate = supported_mcs_rate_nss2;
		} else {
			/* Not Supported */
			hdd_err("nss %d not supported", nss);
			return maxrate;
		}

		if (ch_width == eHT_CHANNEL_WIDTH_20MHZ) {
			tmprate = sgi ?
				supported_mcs_rate[maxidx].supported_rate[2] :
				supported_mcs_rate[maxidx].supported_rate[0];
		} else if (ch_width == eHT_CHANNEL_WIDTH_40MHZ) {
			tmprate = sgi ?
				supported_mcs_rate[maxidx].supported_rate[3] :
				supported_mcs_rate[maxidx].supported_rate[1];
		} else {
			hdd_err("invalid mode %d ch_width %d",
				mode, ch_width);
			return maxrate;
		}

		if (maxrate < tmprate)
			maxrate = tmprate;
	}

	if (mode == SIR_SME_PHY_MODE_VHT) {
		/* check for VHT Mode */
		tmprate = get_max_rate_vht(nss, ch_width, sgi, vht_mcs_map);
		if (maxrate < tmprate)
			maxrate = tmprate;
	}

	return maxrate;
}

#if SUPPORT_11AX
/**
 * hdd_convert_11ax_phymode_to_dot11mode() - get dot11 mode from phymode
 * @phymode: phymode of sta associated to SAP
 *
 * The function is to convert the 11ax phymode to corresponding dot11 mode
 *
 * Return: dot11mode.
 */
static inline enum qca_wlan_802_11_mode
hdd_convert_11ax_phymode_to_dot11mode(int phymode)
{
	switch (phymode) {
	case MODE_11AX_HE20:
	case MODE_11AX_HE40:
	case MODE_11AX_HE80:
	case MODE_11AX_HE80_80:
	case MODE_11AX_HE160:
	case MODE_11AX_HE20_2G:
	case MODE_11AX_HE40_2G:
	case MODE_11AX_HE80_2G:
		return QCA_WLAN_802_11_MODE_11AX;
	default:
		return QCA_WLAN_802_11_MODE_INVALID;
	}
}
#else
static inline enum qca_wlan_802_11_mode
hdd_convert_11ax_phymode_to_dot11mode(int phymode)
{
	return QCA_WLAN_802_11_MODE_INVALID;
}
#endif

enum qca_wlan_802_11_mode hdd_convert_dot11mode_from_phymode(int phymode)
{

	switch (phymode) {

	case MODE_11A:
		return QCA_WLAN_802_11_MODE_11A;

	case MODE_11B:
		return QCA_WLAN_802_11_MODE_11B;

	case MODE_11G:
	case MODE_11GONLY:
		return QCA_WLAN_802_11_MODE_11G;

	case MODE_11NA_HT20:
	case MODE_11NG_HT20:
	case MODE_11NA_HT40:
	case MODE_11NG_HT40:
		return QCA_WLAN_802_11_MODE_11N;

	case MODE_11AC_VHT20:
	case MODE_11AC_VHT40:
	case MODE_11AC_VHT80:
	case MODE_11AC_VHT20_2G:
	case MODE_11AC_VHT40_2G:
	case MODE_11AC_VHT80_2G:
#ifdef CONFIG_160MHZ_SUPPORT
	case MODE_11AC_VHT80_80:
	case MODE_11AC_VHT160:
#endif
		return QCA_WLAN_802_11_MODE_11AC;
	default:
		return hdd_convert_11ax_phymode_to_dot11mode(phymode);
	}

}

/**
 * hdd_fill_station_info() - fill stainfo once connected
 * @adapter: pointer to hdd adapter
 * @event: associate/reassociate event received
 *
 * The function is to update rate stats to stainfo
 *
 * Return: None.
 */
static void hdd_fill_station_info(struct hdd_adapter *adapter,
				  tSap_StationAssocReassocCompleteEvent *event)
{
	struct hdd_station_info *stainfo, *cache_sta_info;
	struct hdd_station_info *oldest_disassoc_sta_info = NULL;
	qdf_time_t oldest_disassoc_sta_ts = 0;
	bool is_dot11_mode_abgn;

	stainfo = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					  event->staMac.bytes,
					  STA_INFO_FILL_STATION_INFO);

	if (!stainfo) {
		hdd_err("invalid stainfo");
		return;
	}

	qdf_mem_copy(&stainfo->capability, &event->capability_info,
		     sizeof(uint16_t));
	stainfo->freq = event->chan_info.mhz;
	stainfo->sta_type = event->staType;
	stainfo->dot11_mode =
		hdd_convert_dot11mode_from_phymode(event->chan_info.info);

	stainfo->nss = event->chan_info.nss;
	stainfo->rate_flags = event->chan_info.rate_flags;
	stainfo->ampdu = event->ampdu;
	stainfo->sgi_enable = event->sgi_enable;
	stainfo->tx_stbc = event->tx_stbc;
	stainfo->rx_stbc = event->rx_stbc;
	stainfo->ch_width = event->ch_width;
	stainfo->mode = event->mode;
	stainfo->max_supp_idx = event->max_supp_idx;
	stainfo->max_ext_idx = event->max_ext_idx;
	stainfo->max_mcs_idx = event->max_mcs_idx;
	stainfo->max_real_mcs_idx = event->max_real_mcs_idx;
	stainfo->rx_mcs_map = event->rx_mcs_map;
	stainfo->tx_mcs_map = event->tx_mcs_map;
	stainfo->assoc_ts = qdf_system_ticks();
	stainfo->max_phy_rate =
		calculate_max_phy_rate(stainfo->mode,
				       stainfo->nss,
				       stainfo->ch_width,
				       stainfo->sgi_enable,
				       stainfo->max_supp_idx,
				       stainfo->max_ext_idx,
				       stainfo->max_mcs_idx,
				       stainfo->rx_mcs_map);
	/* expect max_phy_rate report in kbps */
	stainfo->max_phy_rate *= 100;

	/*
	 * Connected Peer always supports atleast one of the
	 * 802.11 mode out of 11bg/11abg/11abgn, hence this field
	 * should always be true.
	 */
	is_dot11_mode_abgn = true;
	stainfo->ecsa_capable = event->ecsa_capable;
	stainfo->ext_cap = event->ext_cap;
	stainfo->supported_band = event->supported_band;

	if (event->vht_caps.present) {
		stainfo->vht_present = true;
		hdd_copy_vht_caps(&stainfo->vht_caps, &event->vht_caps);
		stainfo->support_mode |=
				(stainfo->vht_present << HDD_80211_MODE_AC);
	}
	if (event->ht_caps.present) {
		stainfo->ht_present = true;
		hdd_copy_ht_caps(&stainfo->ht_caps, &event->ht_caps);
	}

	stainfo->support_mode |=
			(event->he_caps_present << HDD_80211_MODE_AX);

	if (event->he_caps_present && !(event->vht_caps.present ||
					event->ht_caps.present))
		is_dot11_mode_abgn = false;

	stainfo->support_mode |= is_dot11_mode_abgn << HDD_80211_MODE_ABGN;
	/* Initialize DHCP info */
	stainfo->dhcp_phase = DHCP_PHASE_ACK;
	stainfo->dhcp_nego_status = DHCP_NEGO_STOP;

	/* Save assoc request IEs */
	if (event->ies_len) {
		qdf_mem_free(stainfo->assoc_req_ies.ptr);
		stainfo->assoc_req_ies.len = 0;
		stainfo->assoc_req_ies.ptr = qdf_mem_malloc(event->ies_len);
		if (stainfo->assoc_req_ies.ptr) {
			qdf_mem_copy(stainfo->assoc_req_ies.ptr, event->ies,
				     event->ies_len);
			stainfo->assoc_req_ies.len = event->ies_len;
		}
	}

	qdf_mem_copy(&stainfo->mld_addr, &event->sta_mld, QDF_MAC_ADDR_SIZE);

	cache_sta_info =
		hdd_get_sta_info_by_mac(&adapter->cache_sta_info_list,
					event->staMac.bytes,
					STA_INFO_FILL_STATION_INFO);

	if (!cache_sta_info) {
		cache_sta_info = qdf_mem_malloc(sizeof(*cache_sta_info));
		if (!cache_sta_info)
			goto exit;

		qdf_mem_copy(cache_sta_info, stainfo, sizeof(*cache_sta_info));
		cache_sta_info->is_attached = 0;
		cache_sta_info->assoc_req_ies.ptr =
				qdf_mem_malloc(event->ies_len);
		if (cache_sta_info->assoc_req_ies.ptr) {
			qdf_mem_copy(cache_sta_info->assoc_req_ies.ptr,
				     event->ies, event->ies_len);
			cache_sta_info->assoc_req_ies.len = event->ies_len;
		}
		qdf_atomic_init(&cache_sta_info->ref_cnt);

		/*
		 * If cache_sta_info is not present and cache limit is not
		 * reached, then create and attach. Else find the cache that is
		 * the oldest and replace that with the new cache.
		 */
		if (qdf_atomic_read(&adapter->cache_sta_count) <
		    WLAN_MAX_STA_COUNT) {
			hdd_sta_info_attach(&adapter->cache_sta_info_list,
					    cache_sta_info);
			qdf_atomic_inc(&adapter->cache_sta_count);
		} else {
			struct hdd_station_info *temp_sta_info, *tmp = NULL;
			struct hdd_sta_info_obj *sta_list =
						&adapter->cache_sta_info_list;

			hdd_debug("reached max caching, removing oldest");

			/* Find the oldest cached station */
			hdd_for_each_sta_ref_safe(adapter->cache_sta_info_list,
						  temp_sta_info, tmp,
						  STA_INFO_FILL_STATION_INFO) {
				if (temp_sta_info->disassoc_ts &&
				    (!oldest_disassoc_sta_ts ||
				    qdf_system_time_after(
				    oldest_disassoc_sta_ts,
				    temp_sta_info->disassoc_ts))) {
					oldest_disassoc_sta_ts =
						temp_sta_info->disassoc_ts;
					oldest_disassoc_sta_info =
						temp_sta_info;
				}
				hdd_put_sta_info_ref(
						sta_list, &temp_sta_info,
						true,
						STA_INFO_FILL_STATION_INFO);
			}

			/* Remove the oldest and store the current */
			hdd_sta_info_detach(&adapter->cache_sta_info_list,
					    &oldest_disassoc_sta_info);
			hdd_sta_info_attach(&adapter->cache_sta_info_list,
					    cache_sta_info);
		}
	} else {
		qdf_copy_macaddr(&cache_sta_info->sta_mac, &event->staMac);
		qdf_copy_macaddr(&cache_sta_info->mld_addr, &event->sta_mld);
		hdd_put_sta_info_ref(&adapter->cache_sta_info_list,
				     &cache_sta_info, true,
				     STA_INFO_FILL_STATION_INFO);
	}

	hdd_debug("cap %d %d %d %d %d %d %d %d %d %x %d",
		  stainfo->ampdu,
		  stainfo->sgi_enable,
		  stainfo->tx_stbc,
		  stainfo->rx_stbc,
		  stainfo->is_qos_enabled,
		  stainfo->ch_width,
		  stainfo->mode,
		  event->wmmEnabled,
		  event->chan_info.nss,
		  event->chan_info.rate_flags,
		  stainfo->max_phy_rate);
	hdd_debug("rate info %d %d %d %d %d",
		  stainfo->max_supp_idx,
		  stainfo->max_ext_idx,
		  stainfo->max_mcs_idx,
		  stainfo->rx_mcs_map,
		  stainfo->tx_mcs_map);
exit:
	hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
			     STA_INFO_FILL_STATION_INFO);
	return;
}

void hdd_stop_sap_due_to_invalid_channel(struct work_struct *work)
{
	struct hdd_adapter *sap_adapter = container_of(work, struct hdd_adapter,
						       sap_stop_bss_work);
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(sap_adapter->dev, &vdev_sync))
		return;

	hdd_debug("work started for sap session[%d]", sap_adapter->vdev_id);
	wlan_hdd_stop_sap(sap_adapter);
	wlansap_cleanup_cac_timer(WLAN_HDD_GET_SAP_CTX_PTR(sap_adapter));
	hdd_debug("work finished for sap");

	osif_vdev_sync_op_stop(vdev_sync);
}

/**
 * hdd_hostapd_apply_action_oui() - Check for action_ouis to be applied on peers
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 * @event: assoc complete params
 *
 * This function is used to check whether aggressive tx should be disabled
 * based on the soft-ap configuration and action_oui ini
 * gActionOUIDisableAggressiveTX
 *
 * Return: None
 */
static void
hdd_hostapd_apply_action_oui(struct hdd_context *hdd_ctx,
			     struct hdd_adapter *adapter,
			     tSap_StationAssocReassocCompleteEvent *event)
{
	bool found;
	uint32_t freq;
	tSirMacHTChannelWidth ch_width;
	enum sir_sme_phy_mode mode;
	struct action_oui_search_attr attr = {0};
	QDF_STATUS status;

	ch_width = event->ch_width;
	if (ch_width != eHT_CHANNEL_WIDTH_20MHZ)
		return;

	freq = event->chan_info.mhz;
	if (WLAN_REG_IS_24GHZ_CH_FREQ(freq))
		attr.enable_2g = true;
	else if (WLAN_REG_IS_5GHZ_CH_FREQ(freq))
		attr.enable_5g = true;
	else
		return;

	mode = event->mode;
	if (event->vht_caps.present && mode == SIR_SME_PHY_MODE_VHT)
		attr.vht_cap = true;
	else if (event->ht_caps.present && mode == SIR_SME_PHY_MODE_HT)
		attr.ht_cap = true;

	attr.mac_addr = (uint8_t *)(&event->staMac);

	found = ucfg_action_oui_search(hdd_ctx->psoc,
				       &attr,
				       ACTION_OUI_DISABLE_AGGRESSIVE_TX);
	if (!found)
		return;

	status = sme_set_peer_param(attr.mac_addr,
				    WMI_PEER_PARAM_DISABLE_AGGRESSIVE_TX,
				    true, adapter->vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to disable aggregation for peer");
}

static void hdd_hostapd_set_sap_key(struct hdd_adapter *adapter)
{
	struct wlan_crypto_key *crypto_key;
	uint8_t key_index;

	for (key_index = 0; key_index < WLAN_CRYPTO_MAXKEYIDX; ++key_index) {
		crypto_key = wlan_crypto_get_key(adapter->vdev, key_index);
		if (!crypto_key)
			continue;
		ucfg_crypto_set_key_req(adapter->vdev, crypto_key,
					WLAN_CRYPTO_KEY_TYPE_GROUP);
		wma_update_set_key(adapter->vdev_id, false, key_index,
				   crypto_key->cipher_type);
	}
}

#ifdef WLAN_FEATURE_11BE
static void
hdd_fill_channel_change_puncture(struct hdd_chan_change_params *chan_change,
				 struct ch_params *sap_ch_param)
{
	chan_change->chan_params.reg_punc_bitmap =
			sap_ch_param->reg_punc_bitmap;
}
#else
static void
hdd_fill_channel_change_puncture(struct hdd_chan_change_params *chan_change,
				 struct ch_params *sap_ch_param)
{
}
#endif

/**
 * hdd_hostapd_chan_change() - prepare new operation chan info to kernel
 * @adapter: pointre to hdd_adapter
 * @sap_event: pointer to sap_event
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS hdd_hostapd_chan_change(struct hdd_adapter *adapter,
					  struct sap_event *sap_event)
{
	struct hdd_chan_change_params chan_change;
	struct ch_params sap_ch_param = {0};
	eCsrPhyMode phy_mode;
	bool legacy_phymode;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct sap_ch_selected_s *sap_chan_selected =
			&sap_event->sapevt.sap_ch_selected;

	sap_ch_param.ch_width = sap_chan_selected->ch_width;
	sap_ch_param.mhz_freq_seg0 =
		sap_chan_selected->vht_seg0_center_ch_freq;
	sap_ch_param.mhz_freq_seg1 =
		sap_chan_selected->vht_seg1_center_ch_freq;

	wlan_reg_set_channel_params_for_pwrmode(
		hdd_ctx->pdev,
		sap_chan_selected->pri_ch_freq,
		sap_chan_selected->ht_sec_ch_freq,
		&sap_ch_param, REG_CURRENT_PWR_MODE);

	phy_mode = wlan_sap_get_phymode(
			WLAN_HDD_GET_SAP_CTX_PTR(adapter));

	switch (phy_mode) {
	case eCSR_DOT11_MODE_11n:
	case eCSR_DOT11_MODE_11n_ONLY:
	case eCSR_DOT11_MODE_11ac:
	case eCSR_DOT11_MODE_11ac_ONLY:
	case eCSR_DOT11_MODE_11ax:
	case eCSR_DOT11_MODE_11ax_ONLY:
		legacy_phymode = false;
		break;
	default:
		legacy_phymode = true;
		break;
	}

	chan_change.chan_freq = sap_chan_selected->pri_ch_freq;
	chan_change.chan_params.ch_width = sap_chan_selected->ch_width;
	chan_change.chan_params.sec_ch_offset =
		sap_ch_param.sec_ch_offset;
	chan_change.chan_params.mhz_freq_seg0 =
			sap_chan_selected->vht_seg0_center_ch_freq;
	chan_change.chan_params.mhz_freq_seg1 =
			sap_chan_selected->vht_seg1_center_ch_freq;
	hdd_fill_channel_change_puncture(&chan_change, &sap_ch_param);

	return hdd_chan_change_notify(adapter, adapter->dev,
				      chan_change, legacy_phymode);
}

static inline void
hdd_hostapd_update_beacon_country_ie(struct hdd_adapter *adapter)
{
	struct hdd_station_info *sta_info, *tmp = NULL;
	struct hdd_context *hdd_ctx;
	struct hdd_ap_ctx *ap_ctx;
	struct action_oui_search_attr attr;
	QDF_STATUS status;
	bool found = false;

	if (!adapter) {
		hdd_err("invalid adapter");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return;
	}

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_HOSTAPD_SAP_EVENT_CB) {
		if (!sta_info->assoc_req_ies.len)
			goto release_ref;

		qdf_mem_zero(&attr, sizeof(struct action_oui_search_attr));
		attr.ie_data = sta_info->assoc_req_ies.ptr;
		attr.ie_length = sta_info->assoc_req_ies.len;

		found = ucfg_action_oui_search(hdd_ctx->psoc,
					       &attr,
					       ACTION_OUI_TAKE_ALL_BAND_INFO);
		if (found) {
			if (!ap_ctx->country_ie_updated) {
				status = sme_update_beacon_country_ie(
						hdd_ctx->mac_handle,
						adapter->vdev_id,
						true);
				if (status == QDF_STATUS_SUCCESS)
					ap_ctx->country_ie_updated = true;
				else
					hdd_err("fail to update country ie");
			}
			hdd_put_sta_info_ref(&adapter->sta_info_list,
					     &sta_info, true,
					     STA_INFO_HOSTAPD_SAP_EVENT_CB);
			if (tmp)
				hdd_put_sta_info_ref(
					&adapter->sta_info_list,
					&tmp, true,
					STA_INFO_HOSTAPD_SAP_EVENT_CB);
			return;
		}
release_ref:
		hdd_put_sta_info_ref(&adapter->sta_info_list,
				     &sta_info, true,
				     STA_INFO_HOSTAPD_SAP_EVENT_CB);
	}

	if (ap_ctx->country_ie_updated) {
		status = sme_update_beacon_country_ie(
						hdd_ctx->mac_handle,
						adapter->vdev_id, false);
		if (status == QDF_STATUS_SUCCESS)
			ap_ctx->country_ie_updated = false;
		else
			hdd_err("fail to update country ie");
	}
}

#ifdef WLAN_MLD_AP_STA_CONNECT_SUPPORT
static void
hdd_hostapd_sap_fill_peer_ml_info(struct hdd_adapter *adapter,
				  struct station_info *sta_info,
				  uint8_t *peer_mac)
{
	bool is_mlo_vdev;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *sta_peer;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	if (!vdev) {
		hdd_err("Failed to get link id, VDEV NULL");
		return;
	}

	is_mlo_vdev = wlan_vdev_mlme_is_mlo_vdev(vdev);
	if (is_mlo_vdev)
		sta_info->link_id = wlan_vdev_get_link_id(vdev);
	else
		sta_info->link_id = -1;
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);

	if (!is_mlo_vdev)
		return;

	sta_peer = wlan_objmgr_get_peer_by_mac(adapter->hdd_ctx->psoc,
					       peer_mac, WLAN_OSIF_ID);

	if (!sta_peer) {
		hdd_err("Peer not found with MAC " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_mac));
		return;
	}
	qdf_mem_copy(sta_info->mld_addr, wlan_peer_mlme_get_mldaddr(sta_peer),
		     ETH_ALEN);

	status = ucfg_mlme_peer_get_assoc_rsp_ies(
					sta_peer,
					&sta_info->assoc_resp_ies,
					&sta_info->assoc_resp_ies_len);
	wlan_objmgr_peer_release_ref(sta_peer, WLAN_OSIF_ID);
}
#elif defined(CFG80211_MLD_AP_STA_CONNECT_UPSTREAM_SUPPORT)
static void
hdd_hostapd_sap_fill_peer_ml_info(struct hdd_adapter *adapter,
				  struct station_info *sta_info,
				  uint8_t *peer_mac)
{
	bool is_mlo_vdev;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *sta_peer;

	vdev = hdd_objmgr_get_vdev_by_user(adapter,
					   WLAN_HDD_ID_OBJ_MGR);
	if (!vdev) {
		hdd_err("Failed to get link id, VDEV NULL");
		return;
	}

	is_mlo_vdev = wlan_vdev_mlme_is_mlo_vdev(vdev);
	if (!is_mlo_vdev) {
		sta_info->assoc_link_id = -1;
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_HDD_ID_OBJ_MGR);
		return;
	}

	sta_info->assoc_link_id = wlan_vdev_get_link_id(vdev);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_HDD_ID_OBJ_MGR);

	sta_peer = wlan_objmgr_get_peer_by_mac(adapter->hdd_ctx->psoc,
					       peer_mac, WLAN_HDD_ID_OBJ_MGR);

	if (!sta_peer) {
		hdd_err("Peer not found with MAC " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_mac));
		return;
	}
	qdf_mem_copy(sta_info->mld_addr, wlan_peer_mlme_get_mldaddr(sta_peer),
		     ETH_ALEN);

	status = ucfg_mlme_peer_get_assoc_rsp_ies(
					sta_peer,
					&sta_info->assoc_resp_ies,
					&sta_info->assoc_resp_ies_len);
	wlan_objmgr_peer_release_ref(sta_peer, WLAN_HDD_ID_OBJ_MGR);
	sta_info->mlo_params_valid = true;
}
#else
static void
hdd_hostapd_sap_fill_peer_ml_info(struct hdd_adapter *adapter,
				  struct station_info *sta_info,
				  uint8_t *peer_mac)
{
}
#endif

QDF_STATUS hdd_hostapd_sap_event_cb(struct sap_event *sap_event,
				    void *context)
{
	struct hdd_adapter *adapter;
	struct hdd_ap_ctx *ap_ctx;
	struct hdd_hostapd_state *hostapd_state;
	struct net_device *dev;
	eSapHddEvent event_id;
	union iwreq_data wrqu;
	uint8_t *we_custom_event_generic = NULL;
	int we_event = 0;
	uint8_t sta_id;
	QDF_STATUS qdf_status;
	bool bAuthRequired = true;
	char *unknownSTAEvent = NULL;
	char *maxAssocExceededEvent = NULL;
	uint8_t *we_custom_start_event = NULL;
	char *startBssEvent;
	struct hdd_context *hdd_ctx;
	struct iw_michaelmicfailure msg;
	uint8_t ignoreCAC = 0;
	eSapDfsCACState_t cac_state = eSAP_DFS_DO_NOT_SKIP_CAC;
	struct hdd_config *cfg = NULL;
	struct wlan_dfs_info dfs_info;
	struct hdd_adapter *con_sap_adapter;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSap_StationAssocReassocCompleteEvent *event;
	tSap_StationSetKeyCompleteEvent *key_complete;
	int ret = 0;
	tSap_StationDisassocCompleteEvent *disassoc_comp;
	struct hdd_station_info *stainfo, *cache_stainfo, *tmp = NULL;
	mac_handle_t mac_handle;
	struct sap_config *sap_config;
	struct sap_context *sap_ctx = NULL;
	uint8_t pdev_id;
	bool notify_new_sta = true;
	struct wlan_objmgr_vdev *vdev;
	struct qdf_mac_addr sta_addr = {0};
	qdf_freq_t dfs_freq;
	uint8_t sta_cnt;

	dev = context;
	if (!dev) {
		hdd_err("context is null");
		return QDF_STATUS_E_FAILURE;
	}

	adapter = netdev_priv(dev);

	if ((!adapter) ||
	    (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		hdd_err("invalid adapter or adapter has invalid magic");
		return QDF_STATUS_E_FAILURE;
	}

	hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

	if (!sap_event) {
		hdd_err("sap_event is null");
		return QDF_STATUS_E_FAILURE;
	}

	event_id = sap_event->sapHddEventCode;
	memset(&wrqu, '\0', sizeof(wrqu));
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return QDF_STATUS_E_FAILURE;
	}

	cfg = hdd_ctx->config;

	if (!cfg) {
		hdd_err("HDD config is null");
		return QDF_STATUS_E_FAILURE;
	}

	mac_handle = hdd_ctx->mac_handle;
	dfs_info.channel = wlan_reg_freq_to_chan(
			hdd_ctx->pdev, ap_ctx->operating_chan_freq);
	wlan_reg_get_cc_and_src(hdd_ctx->psoc, dfs_info.country_code);
	sta_id = sap_event->sapevt.sapStartBssCompleteEvent.staId;
	sap_config = &adapter->session.ap.sap_config;

	switch (event_id) {
	case eSAP_START_BSS_EVENT:
		hdd_debug("BSS status = %s, channel = %u, bc sta Id = %d",
		       sap_event->sapevt.sapStartBssCompleteEvent.
		       status ? "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS",
		       sap_event->sapevt.sapStartBssCompleteEvent.
		       operating_chan_freq,
		       sap_event->sapevt.sapStartBssCompleteEvent.staId);
		ap_ctx->operating_chan_freq =
			sap_event->sapevt.sapStartBssCompleteEvent
			.operating_chan_freq;

		adapter->vdev_id =
			sap_event->sapevt.sapStartBssCompleteEvent.sessionId;
		sap_config->chan_freq =
			sap_event->sapevt.sapStartBssCompleteEvent.
			operating_chan_freq;
		sap_config->ch_params.ch_width =
			sap_event->sapevt.sapStartBssCompleteEvent.ch_width;

		hdd_nofl_info("AP started vid %d freq %d BW %d",
			      adapter->vdev_id,
			      ap_ctx->operating_chan_freq,
			      sap_config->ch_params.ch_width);

		sap_config->ch_params = ap_ctx->sap_context->ch_params;
		sap_config->sec_ch_freq = ap_ctx->sap_context->sec_ch_freq;

		hostapd_state->qdf_status =
			sap_event->sapevt.sapStartBssCompleteEvent.status;

		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);
		wlansap_get_dfs_ignore_cac(mac_handle, &ignoreCAC);
		if (!policy_mgr_get_dfs_master_dynamic_enabled(
				hdd_ctx->psoc, adapter->vdev_id))
			ignoreCAC = true;

		wlansap_get_dfs_cac_state(mac_handle, &cac_state);

		/* DFS requirement: DO NOT transmit during CAC. */

		/* Indoor channels are also marked DFS, therefore
		 * check if the channel has REGULATORY_CHAN_RADAR
		 * channel flag to identify if the channel is DFS
		 */
		if (!wlan_reg_is_dfs_for_freq(hdd_ctx->pdev,
					      ap_ctx->operating_chan_freq) ||
		    ignoreCAC ||
		    hdd_ctx->dev_dfs_cac_status == DFS_CAC_ALREADY_DONE ||
		    eSAP_DFS_SKIP_CAC == cac_state)
			ap_ctx->dfs_cac_block_tx = false;
		else
			ap_ctx->dfs_cac_block_tx = true;

		ucfg_ipa_set_dfs_cac_tx(hdd_ctx->pdev,
					ap_ctx->dfs_cac_block_tx);

		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
		if (vdev) {
			ucfg_dp_set_dfs_cac_tx(vdev, ap_ctx->dfs_cac_block_tx);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		}

		hdd_debug("The value of dfs_cac_block_tx[%d] for ApCtx[%pK]:%d",
				ap_ctx->dfs_cac_block_tx, ap_ctx,
				adapter->vdev_id);

		if (hostapd_state->qdf_status) {
			hdd_err("startbss event failed!!");
			/*
			 * Make sure to set the event before proceeding
			 * for error handling otherwise caller thread will
			 * wait till 10 secs and no other connection will
			 * go through before that.
			 */
			hostapd_state->bss_state = BSS_STOP;
			vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
			if (vdev) {
				ucfg_dp_set_bss_state_start(vdev, false);
				hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
			}
			qdf_event_set(&hostapd_state->qdf_event);
			goto stopbss;
		} else {
			sme_ch_avoid_update_req(mac_handle);

			ap_ctx->broadcast_sta_id =
				sap_event->sapevt.sapStartBssCompleteEvent.staId;

			cdp_hl_fc_set_td_limit(
				cds_get_context(QDF_MODULE_ID_SOC),
				adapter->vdev_id,
				ap_ctx->operating_chan_freq);

			hdd_register_tx_flow_control(adapter,
				hdd_softap_tx_resume_timer_expired_handler,
				hdd_softap_tx_resume_cb,
				hdd_tx_flow_control_is_pause);

			hdd_register_hl_netdev_fc_timer(
				adapter,
				hdd_tx_resume_timer_expired_handler);

			/* @@@ need wep logic here to set privacy bit */
			qdf_status =
				hdd_softap_register_bc_sta(adapter,
							   ap_ctx->privacy);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hdd_warn("Failed to register BC STA %d",
				       qdf_status);
				hdd_stop_bss_link(adapter);
			}
		}

		if (ucfg_ipa_is_enabled()) {
			status = ucfg_ipa_wlan_evt(
					hdd_ctx->pdev,
					adapter->dev,
					adapter->device_mode,
					adapter->vdev_id,
					WLAN_IPA_AP_CONNECT,
					adapter->dev->dev_addr,
					WLAN_REG_IS_24GHZ_CH_FREQ(
						ap_ctx->operating_chan_freq));
			if (status)
				hdd_err("WLAN_AP_CONNECT event failed");
		}

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(hdd_ctx, true);
#endif
		hdd_hostapd_channel_prevent_suspend(adapter,
			ap_ctx->operating_chan_freq);

		hostapd_state->bss_state = BSS_START;
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
		if (vdev) {
			ucfg_dp_set_bss_state_start(vdev, true);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		}
		hdd_start_tsf_sync(adapter);

		hdd_hostapd_set_sap_key(adapter);

		/* Fill the params for sending IWEVCUSTOM Event
		 * with SOFTAP.enabled
		 */
		we_custom_start_event =
				qdf_mem_malloc(HDD_MAX_CUSTOM_START_EVENT_SIZE);
		if (!we_custom_start_event)
			goto stopbss;

		startBssEvent = "SOFTAP.enabled";
		memset(we_custom_start_event, '\0',
		       sizeof(HDD_MAX_CUSTOM_START_EVENT_SIZE));
		memcpy(we_custom_start_event, startBssEvent,
		       strlen(startBssEvent));
		memset(&wrqu, 0, sizeof(wrqu));
		wrqu.data.length = strlen(startBssEvent);
		we_event = IWEVCUSTOM;
		we_custom_event_generic = we_custom_start_event;
		wlan_hdd_set_tx_flow_info();
		sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
		if (!sap_ctx) {
			hdd_err("sap ctx is null");
			return QDF_STATUS_E_FAILURE;
		}

		if (sap_ctx->is_chan_change_inprogress) {
			hdd_debug("check for possible hw mode change");
			status = policy_mgr_set_hw_mode_on_channel_switch(
				hdd_ctx->psoc, adapter->vdev_id);
			if (QDF_IS_STATUS_ERROR(status))
				hdd_debug("set hw mode change not done");
		}

		/*
		 * Enable wds source port learning on the dp vdev in AP mode
		 * when WDS feature is enabled.
		 */
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
		if (vdev) {
			if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
				hdd_wds_config_dp_repeater_mode(vdev);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
		}
		/*
		 * set this event at the very end because once this events
		 * get set, caller thread is waiting to do further processing.
		 * so once this event gets set, current worker thread might get
		 * pre-empted by caller thread.
		 */
		qdf_status = qdf_event_set(&hostapd_state->qdf_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("qdf_event_set failed! status: %d", qdf_status);
			goto stopbss;
		}

		wlan_hdd_apply_user_mcc_quota(adapter);
		break;          /* Event will be sent after Switch-Case stmt */

	case eSAP_STOP_BSS_EVENT:
		hdd_debug("BSS stop status = %s",
		       sap_event->sapevt.sapStopBssCompleteEvent.
		       status ? "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

		hdd_hostapd_channel_allow_suspend(adapter,
						  ap_ctx->operating_chan_freq);

		/* Invalidate the channel info. */
		ap_ctx->operating_chan_freq = 0;

		/* reset the dfs_cac_status and dfs_cac_block_tx flag only when
		 * the last BSS is stopped
		 */
		con_sap_adapter = hdd_get_con_sap_adapter(adapter, true);
		if (!con_sap_adapter) {
			ap_ctx->dfs_cac_block_tx = true;
			hdd_ctx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
			vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
			if (vdev) {
				ucfg_dp_set_dfs_cac_tx(vdev,
						ap_ctx->dfs_cac_block_tx);
				hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
			}
		}
		hdd_nofl_info("Ap stopped vid %d reason=%d", adapter->vdev_id,
			      ap_ctx->bss_stop_reason);
		qdf_status =
			policy_mgr_get_mac_id_by_session_id(hdd_ctx->psoc,
							    adapter->vdev_id,
							    &pdev_id);
		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_medium_assess_stop_timer(pdev_id, hdd_ctx);

		hdd_medium_assess_deinit();

		/* clear the reason code in case BSS is stopped
		 * in another place
		 */
		ap_ctx->bss_stop_reason = BSS_STOP_REASON_INVALID;
		ap_ctx->ap_active = false;
		goto stopbss;

	case eSAP_DFS_CAC_START:
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					WLAN_SVC_DFS_CAC_START_IND,
					    &dfs_info,
					    sizeof(struct wlan_dfs_info));
		hdd_ctx->dev_dfs_cac_status = DFS_CAC_IN_PROGRESS;
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(hdd_ctx, eSAP_DFS_CAC_START,
				dfs_info, &adapter->wdev)) {
			hdd_err("Unable to indicate CAC start NL event");
		} else {
			hdd_debug("Sent CAC start to user space");
		}

		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);
		hdd_stop_tsf_sync(adapter);
		break;
	case eSAP_DFS_CAC_INTERRUPTED:
		/*
		 * The CAC timer did not run completely and a radar was detected
		 * during the CAC time. This new state will keep the tx path
		 * blocked since we do not want any transmission on the DFS
		 * channel. CAC end will only be reported here since the user
		 * space applications are waiting on CAC end for their state
		 * management.
		 */
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(hdd_ctx, eSAP_DFS_CAC_END,
				dfs_info, &adapter->wdev)) {
			hdd_err("Unable to indicate CAC end (interrupted) event");
		} else {
			hdd_debug("Sent CAC end (interrupted) to user space");
		}
		dfs_freq = wlan_reg_chan_band_to_freq(hdd_ctx->pdev,
						      dfs_info.channel,
						      BIT(REG_BAND_5G));
		hdd_son_deliver_cac_status_event(adapter, dfs_freq, true);
		break;
	case eSAP_DFS_CAC_END:
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					WLAN_SVC_DFS_CAC_END_IND,
					    &dfs_info,
					    sizeof(struct wlan_dfs_info));
		ap_ctx->dfs_cac_block_tx = false;
		ucfg_ipa_set_dfs_cac_tx(hdd_ctx->pdev,
					ap_ctx->dfs_cac_block_tx);
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
		if (vdev) {
			ucfg_dp_set_dfs_cac_tx(vdev,
					       ap_ctx->dfs_cac_block_tx);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		}

		hdd_ctx->dev_dfs_cac_status = DFS_CAC_ALREADY_DONE;
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(hdd_ctx, eSAP_DFS_CAC_END,
				dfs_info, &adapter->wdev)) {
			hdd_err("Unable to indicate CAC end NL event");
		} else {
			hdd_debug("Sent CAC end to user space");
		}
		dfs_freq = wlan_reg_chan_band_to_freq(hdd_ctx->pdev,
						      dfs_info.channel,
						      BIT(REG_BAND_5G));
		hdd_son_deliver_cac_status_event(adapter, dfs_freq, false);
		break;
	case eSAP_DFS_RADAR_DETECT:
	{
		int i;
		struct sap_config *sap_config =
				&adapter->session.ap.sap_config;

		hdd_dfs_indicate_radar(hdd_ctx);
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					WLAN_SVC_DFS_RADAR_DETECT_IND,
					    &dfs_info,
					    sizeof(struct wlan_dfs_info));
		hdd_ctx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
		for (i = 0; i < sap_config->channel_info_count; i++) {
			if (sap_config->channel_info[i].ieee_chan_number
							== dfs_info.channel)
				sap_config->channel_info[i].flags |=
					IEEE80211_CHAN_RADAR_DFS;
		}
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(hdd_ctx, eSAP_DFS_RADAR_DETECT,
				dfs_info, &adapter->wdev)) {
			hdd_err("Unable to indicate Radar detect NL event");
		} else {
			hdd_debug("Sent radar detected to user space");
		}
		dfs_freq = wlan_reg_chan_band_to_freq(hdd_ctx->pdev,
						      dfs_info.channel,
						      BIT(REG_BAND_5G));
		hdd_son_deliver_cac_status_event(adapter, dfs_freq, true);
		break;
	}

	case eSAP_DFS_NO_AVAILABLE_CHANNEL:
		wlan_hdd_send_svc_nlink_msg
			(hdd_ctx->radio_index,
			WLAN_SVC_DFS_ALL_CHANNEL_UNAVAIL_IND, &dfs_info,
			sizeof(struct wlan_dfs_info));
		break;

	case eSAP_STA_SET_KEY_EVENT:
		/* TODO:
		 * forward the message to hostapd once implementation
		 * is done for now just print
		 */
		key_complete = &sap_event->sapevt.sapStationSetKeyCompleteEvent;
		hdd_debug("SET Key: configured status = %s",
			  key_complete->status ?
			  "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

		if (QDF_IS_STATUS_SUCCESS(key_complete->status)) {
			hdd_softap_change_sta_state(adapter,
						    &key_complete->peerMacAddr,
						    OL_TXRX_PEER_STATE_AUTH);
		status = wlan_hdd_send_sta_authorized_event(
						adapter, hdd_ctx,
						&key_complete->peerMacAddr);

		}
		return QDF_STATUS_SUCCESS;
	case eSAP_STA_MIC_FAILURE_EVENT:
	{
		memset(&msg, '\0', sizeof(msg));
		msg.src_addr.sa_family = ARPHRD_ETHER;
		memcpy(msg.src_addr.sa_data,
		       &sap_event->sapevt.sapStationMICFailureEvent.
		       staMac, QDF_MAC_ADDR_SIZE);
		hdd_debug("MIC MAC " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(msg.src_addr.sa_data));
		if (sap_event->sapevt.sapStationMICFailureEvent.
		    multicast == true)
			msg.flags = IW_MICFAILURE_GROUP;
		else
			msg.flags = IW_MICFAILURE_PAIRWISE;
		memset(&wrqu, 0, sizeof(wrqu));
		wrqu.data.length = sizeof(msg);
		we_event = IWEVMICHAELMICFAILURE;
		we_custom_event_generic = (uint8_t *) &msg;
	}
		/* inform mic failure to nl80211 */
		cfg80211_michael_mic_failure(dev,
					     sap_event->
					     sapevt.sapStationMICFailureEvent.
					     staMac.bytes,
					     ((sap_event->sapevt.
					       sapStationMICFailureEvent.
					       multicast ==
					       true) ?
					      NL80211_KEYTYPE_GROUP :
					      NL80211_KEYTYPE_PAIRWISE),
					     sap_event->sapevt.
					     sapStationMICFailureEvent.keyId,
					     sap_event->sapevt.
					     sapStationMICFailureEvent.TSC,
					     GFP_KERNEL);
		break;

	case eSAP_STA_ASSOC_EVENT:
	case eSAP_STA_REASSOC_EVENT:
		event = &sap_event->sapevt.sapStationAssocReassocCompleteEvent;

		/* Reset scan reject params on assoc */
		hdd_init_scan_reject_params(hdd_ctx);
		if (eSAP_STATUS_FAILURE == event->status) {
			hdd_info("assoc failure: " QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(wrqu.addr.sa_data));
			hdd_place_marker(adapter, "CLIENT ASSOC FAILURE",
					 wrqu.addr.sa_data);
			break;
		}

		hdd_hostapd_apply_action_oui(hdd_ctx, adapter, event);

		wrqu.addr.sa_family = ARPHRD_ETHER;
		memcpy(wrqu.addr.sa_data,
		       &event->staMac, QDF_MAC_ADDR_SIZE);
		hdd_info("associated " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(wrqu.addr.sa_data));
		hdd_place_marker(adapter, "CLIENT ASSOCIATED",
				 wrqu.addr.sa_data);
		we_event = IWEVREGISTERED;

		if ((eCSR_ENCRYPT_TYPE_NONE == ap_ctx->encryption_type) ||
		    (eCSR_ENCRYPT_TYPE_WEP40_STATICKEY ==
		     ap_ctx->encryption_type)
		    || (eCSR_ENCRYPT_TYPE_WEP104_STATICKEY ==
			ap_ctx->encryption_type)) {
			bAuthRequired = false;
		}

		if (bAuthRequired) {
			qdf_status = hdd_softap_register_sta(
						adapter,
						true,
						ap_ctx->privacy,
						(struct qdf_mac_addr *)
						wrqu.addr.sa_data,
						event);
			if (QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_fill_station_info(adapter, event);
			else
				hdd_err("Failed to register STA %d "
					QDF_MAC_ADDR_FMT, qdf_status,
					QDF_MAC_ADDR_REF(wrqu.addr.sa_data));
		} else {
			qdf_status = hdd_softap_register_sta(
						adapter,
						false,
						ap_ctx->privacy,
						(struct qdf_mac_addr *)
						wrqu.addr.sa_data,
						event);
			if (QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_fill_station_info(adapter, event);
			else
				hdd_err("Failed to register STA %d "
					QDF_MAC_ADDR_FMT, qdf_status,
					QDF_MAC_ADDR_REF(wrqu.addr.sa_data));
		}

		sta_id = event->staId;

		if (ucfg_ipa_is_enabled()) {
			status = ucfg_ipa_wlan_evt(hdd_ctx->pdev,
						   adapter->dev,
						   adapter->device_mode,
						   adapter->vdev_id,
						   WLAN_IPA_CLIENT_CONNECT_EX,
						   event->staMac.bytes,
						   false);
			if (status)
				hdd_err("WLAN_CLIENT_CONNECT_EX event failed");
		}

		DPTRACE(qdf_dp_trace_mgmt_pkt(QDF_DP_TRACE_MGMT_PACKET_RECORD,
			adapter->vdev_id,
			QDF_TRACE_DEFAULT_PDEV_ID,
			QDF_PROTO_TYPE_MGMT, QDF_PROTO_MGMT_ASSOC));

		/* start timer in sap/p2p_go */
		if (ap_ctx->ap_active == false) {
			vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
			if (vdev) {
				ucfg_dp_bus_bw_compute_prev_txrx_stats(vdev);
				hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
			}
			ucfg_dp_bus_bw_compute_timer_start(hdd_ctx->psoc);
		}
		ap_ctx->ap_active = true;

		hdd_hostapd_update_beacon_country_ie(adapter);

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(hdd_ctx, false);
#endif
		cds_host_diag_log_work(&hdd_ctx->sap_wake_lock,
				       HDD_SAP_WAKE_LOCK_DURATION,
				       WIFI_POWER_EVENT_WAKELOCK_SAP);
		qdf_wake_lock_timeout_acquire(&hdd_ctx->sap_wake_lock,
					      HDD_SAP_WAKE_LOCK_DURATION);
		{
			struct station_info *sta_info;
			uint32_t ies_len = event->ies_len;

			sta_info = qdf_mem_malloc(sizeof(*sta_info));
			if (!sta_info)
				return QDF_STATUS_E_FAILURE;

			sta_info->assoc_req_ies = event->ies;
			sta_info->assoc_req_ies_len = ies_len;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)) && !defined(WITH_BACKPORTS)
			/*
			 * After Kernel 4.0, it's no longer need to set
			 * STATION_INFO_ASSOC_REQ_IES flag, as it
			 * changed to use assoc_req_ies_len length to
			 * check the existence of request IE.
			 */
			sta_info->filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
			/* For ML clients need to fill assoc resp IEs
			 * and MLD address.
			 * For Legacy clients MLD address will be
			 * NULL MAC address.
			 */
			hdd_hostapd_sap_fill_peer_ml_info(adapter, sta_info,
							  event->staMac.bytes);

			if (notify_new_sta)
				cfg80211_new_sta(dev,
						 (const u8 *)&event->
						 staMac.bytes[0],
						 sta_info, GFP_KERNEL);

			if (adapter->device_mode == QDF_SAP_MODE &&
			    ucfg_mlme_get_wds_mode(hdd_ctx->psoc))
				hdd_softap_ind_l2_update(adapter,
							 &event->staMac);
			qdf_mem_free(sta_info);
		}
		/* Lets abort scan to ensure smooth authentication for client */
		if (ucfg_scan_get_vdev_status(adapter->vdev) !=
				SCAN_NOT_IN_PROGRESS) {
			wlan_abort_scan(hdd_ctx->pdev, INVAL_PDEV_ID,
					adapter->vdev_id, INVALID_SCAN_ID,
					false);
		}
		if (adapter->device_mode == QDF_P2P_GO_MODE) {
			/* send peer status indication to oem app */
			hdd_send_peer_status_ind_to_app(
				&event->staMac,
				ePeerConnected,
				event->timingMeasCap,
				adapter->vdev_id,
				&event->chan_info,
				adapter->device_mode);
		}

		hdd_green_ap_add_sta(hdd_ctx);
		hdd_son_deliver_assoc_disassoc_event(adapter,
						     event->staMac,
						     event->status,
						     ALD_ASSOC_EVENT);
		break;

	case eSAP_STA_DISASSOC_EVENT:
		disassoc_comp =
			&sap_event->sapevt.sapStationDisassocCompleteEvent;
		memcpy(wrqu.addr.sa_data,
		       &disassoc_comp->staMac, QDF_MAC_ADDR_SIZE);

		/* Reset scan reject params on disconnect */
		hdd_init_scan_reject_params(hdd_ctx);
		cache_stainfo = hdd_get_sta_info_by_mac(
						&adapter->cache_sta_info_list,
						disassoc_comp->staMac.bytes,
						STA_INFO_HOSTAPD_SAP_EVENT_CB);
		if (cache_stainfo) {
			/* Cache the disassoc info */
			cache_stainfo->rssi = disassoc_comp->rssi;
			cache_stainfo->tx_rate = disassoc_comp->tx_rate;
			cache_stainfo->rx_rate = disassoc_comp->rx_rate;
			cache_stainfo->rx_mc_bc_cnt =
						disassoc_comp->rx_mc_bc_cnt;
			cache_stainfo->rx_retry_cnt =
						disassoc_comp->rx_retry_cnt;
			cache_stainfo->reason_code = disassoc_comp->reason_code;
			cache_stainfo->disassoc_ts = qdf_system_ticks();
			hdd_debug("Cache_stainfo rssi %d txrate %d rxrate %d reason_code %d",
				  cache_stainfo->rssi,
				  cache_stainfo->tx_rate,
				  cache_stainfo->rx_rate,
				  cache_stainfo->reason_code);
			hdd_put_sta_info_ref(&adapter->cache_sta_info_list,
					     &cache_stainfo, true,
					     STA_INFO_HOSTAPD_SAP_EVENT_CB);
		}
		hdd_nofl_info("SAP disassociated " QDF_MAC_ADDR_FMT,
			      QDF_MAC_ADDR_REF(wrqu.addr.sa_data));
		hdd_place_marker(adapter, "CLIENT DISASSOCIATED FROM SAP",
				 wrqu.addr.sa_data);
		qdf_status = qdf_event_set(&hostapd_state->qdf_sta_disassoc_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_err("Station Deauth event Set failed");

		if (sap_event->sapevt.sapStationDisassocCompleteEvent.reason ==
		    eSAP_USR_INITATED_DISASSOC)
			hdd_debug(" User initiated disassociation");
		else
			hdd_debug(" MAC initiated disassociation");
		we_event = IWEVEXPIRED;

		DPTRACE(qdf_dp_trace_mgmt_pkt(QDF_DP_TRACE_MGMT_PACKET_RECORD,
			adapter->vdev_id,
			QDF_TRACE_DEFAULT_PDEV_ID,
			QDF_PROTO_TYPE_MGMT, QDF_PROTO_MGMT_DISASSOC));

		stainfo = hdd_get_sta_info_by_mac(
						&adapter->sta_info_list,
						disassoc_comp->staMac.bytes,
						STA_INFO_HOSTAPD_SAP_EVENT_CB);
		if (!stainfo) {
			hdd_err("Failed to find the right station");
			return QDF_STATUS_E_INVAL;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(adapter->vdev) &&
		    !qdf_is_macaddr_zero(&stainfo->mld_addr)) {
			qdf_copy_macaddr(&sta_addr, &stainfo->mld_addr);
		} else {
			/* Copy legacy MAC address on
			 * non-ML type client disassoc.
			 */
			qdf_copy_macaddr(&sta_addr, &disassoc_comp->staMac);
		}

		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
		if (vdev) {
			ucfg_dp_update_dhcp_state_on_disassoc(vdev,
						      &disassoc_comp->staMac);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		}

		hdd_softap_deregister_sta(adapter, &stainfo);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
				     STA_INFO_HOSTAPD_SAP_EVENT_CB);

		ap_ctx->ap_active = false;

		hdd_for_each_sta_ref_safe(adapter->sta_info_list, stainfo,
					  tmp, STA_INFO_HOSTAPD_SAP_EVENT_CB) {
			if (!qdf_is_macaddr_broadcast(
			    &stainfo->sta_mac)) {
				ap_ctx->ap_active = true;
				hdd_put_sta_info_ref(
						&adapter->sta_info_list,
						&stainfo, true,
						STA_INFO_HOSTAPD_SAP_EVENT_CB);
				if (tmp)
					hdd_put_sta_info_ref(
						&adapter->sta_info_list,
						&tmp, true,
						STA_INFO_HOSTAPD_SAP_EVENT_CB);
				break;
			}
			hdd_put_sta_info_ref(&adapter->sta_info_list,
					     &stainfo, true,
					     STA_INFO_HOSTAPD_SAP_EVENT_CB);
		}

		hdd_hostapd_update_beacon_country_ie(adapter);

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(hdd_ctx, true);
#endif

		cds_host_diag_log_work(&hdd_ctx->sap_wake_lock,
				       HDD_SAP_WAKE_LOCK_DURATION,
				       WIFI_POWER_EVENT_WAKELOCK_SAP);
		qdf_wake_lock_timeout_acquire(&hdd_ctx->sap_wake_lock,
			 HDD_SAP_CLIENT_DISCONNECT_WAKE_LOCK_DURATION);

		/*
		 * Don't indicate delete station event if P2P GO and
		 * SSR in progress. Since supplicant will change mode
		 * fail and down during this time.
		 */

		if ((adapter->device_mode != QDF_P2P_GO_MODE) ||
		     (!cds_is_driver_recovering())) {
			cfg80211_del_sta(dev,
					 (const u8 *)&sta_addr.bytes[0],
					 GFP_KERNEL);
			hdd_debug("indicate sta deletion event");
		}

		/* Update the beacon Interval if it is P2P GO */
		qdf_status = policy_mgr_change_mcc_go_beacon_interval(
			hdd_ctx->psoc, adapter->vdev_id,
			adapter->device_mode);
		if (QDF_STATUS_SUCCESS != qdf_status) {
			hdd_err("Failed to update Beacon interval status: %d",
				qdf_status);
		}
		if (adapter->device_mode == QDF_P2P_GO_MODE) {
			/* send peer status indication to oem app */
			hdd_send_peer_status_ind_to_app(&sap_event->sapevt.
						sapStationDisassocCompleteEvent.
						staMac, ePeerDisconnected,
						0,
						adapter->vdev_id,
						NULL,
						adapter->device_mode);
		}

		/*stop timer in sap/p2p_go */
		if (ap_ctx->ap_active == false) {
			vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
			if (vdev) {
				ucfg_dp_bus_bw_compute_reset_prev_txrx_stats(vdev);
				hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
			}
			ucfg_dp_bus_bw_compute_timer_try_stop(hdd_ctx->psoc);
		}
		hdd_son_deliver_assoc_disassoc_event(adapter,
						     disassoc_comp->staMac,
						     disassoc_comp->reason_code,
						     ALD_DISASSOC_EVENT);
		hdd_green_ap_del_sta(hdd_ctx);
		break;

	case eSAP_WPS_PBC_PROBE_REQ_EVENT:
		hdd_debug("WPS PBC probe req");
		return QDF_STATUS_SUCCESS;

	case eSAP_UNKNOWN_STA_JOIN:
		unknownSTAEvent = qdf_mem_malloc(IW_CUSTOM_MAX + 1);
		if (!unknownSTAEvent)
			return QDF_STATUS_E_NOMEM;

		snprintf(unknownSTAEvent, IW_CUSTOM_MAX,
			 "JOIN_UNKNOWN_STA-"QDF_FULL_MAC_FMT,
			 QDF_FULL_MAC_REF(sap_event->sapevt.sapUnknownSTAJoin.macaddr.bytes));
		we_event = IWEVCUSTOM;  /* Discovered a new node (AP mode). */
		wrqu.data.pointer = unknownSTAEvent;
		wrqu.data.length = strlen(unknownSTAEvent);
		we_custom_event_generic = (uint8_t *) unknownSTAEvent;
		hdd_err("%s", unknownSTAEvent);
		break;

	case eSAP_MAX_ASSOC_EXCEEDED:
		maxAssocExceededEvent = qdf_mem_malloc(IW_CUSTOM_MAX + 1);
		if (!maxAssocExceededEvent)
			return QDF_STATUS_E_NOMEM;

		snprintf(maxAssocExceededEvent, IW_CUSTOM_MAX,
			 "Peer "QDF_FULL_MAC_FMT" denied"
			 " assoc due to Maximum Mobile Hotspot connections reached. Please disconnect"
			 " one or more devices to enable the new device connection",
			 QDF_FULL_MAC_REF(sap_event->sapevt.sapMaxAssocExceeded.macaddr.bytes));
		we_event = IWEVCUSTOM;  /* Discovered a new node (AP mode). */
		wrqu.data.pointer = maxAssocExceededEvent;
		wrqu.data.length = strlen(maxAssocExceededEvent);
		we_custom_event_generic = (uint8_t *) maxAssocExceededEvent;
		hdd_debug("%s", maxAssocExceededEvent);
		break;
	case eSAP_STA_ASSOC_IND:
		if (sap_event->sapevt.sapAssocIndication.owe_ie) {
			hdd_send_update_owe_info_event(adapter,
			      sap_event->sapevt.sapAssocIndication.staMac.bytes,
			      sap_event->sapevt.sapAssocIndication.owe_ie,
			      sap_event->sapevt.sapAssocIndication.owe_ie_len);
			qdf_mem_free(
				   sap_event->sapevt.sapAssocIndication.owe_ie);
			sap_event->sapevt.sapAssocIndication.owe_ie = NULL;
			sap_event->sapevt.sapAssocIndication.owe_ie_len = 0;
		}
		return QDF_STATUS_SUCCESS;

	case eSAP_DISCONNECT_ALL_P2P_CLIENT:
		hdd_clear_all_sta(adapter);
		return QDF_STATUS_SUCCESS;

	case eSAP_MAC_TRIG_STOP_BSS_EVENT:
		ret = hdd_stop_bss_link(adapter);
		if (ret)
			hdd_warn("hdd_stop_bss_link failed %d", ret);
		return QDF_STATUS_SUCCESS;

	case eSAP_CHANNEL_CHANGE_EVENT:
		if (hostapd_state->bss_state != BSS_STOP) {
			/* Allow suspend for old channel */
			hdd_hostapd_channel_allow_suspend(adapter,
				ap_ctx->sap_context->freq_before_ch_switch);
			/* Prevent suspend for new channel */
			hdd_hostapd_channel_prevent_suspend(adapter,
				sap_event->sapevt.sap_ch_selected.pri_ch_freq);
		}
		/* SME/PE is already updated for new operation
		 * channel. So update HDD layer also here. This
		 * resolves issue in AP-AP mode where AP1 channel is
		 * changed due to RADAR then CAC is going on and
		 * START_BSS on new channel has not come to HDD. At
		 * this case if AP2 is started it needs current
		 * operation channel for MCC DFS restriction
		 */
		ap_ctx->operating_chan_freq =
			sap_event->sapevt.sap_ch_selected.pri_ch_freq;
		ap_ctx->sap_config.acs_cfg.pri_ch_freq =
			sap_event->sapevt.sap_ch_selected.pri_ch_freq;
		ap_ctx->sap_config.acs_cfg.ht_sec_ch_freq =
			sap_event->sapevt.sap_ch_selected.ht_sec_ch_freq;
		ap_ctx->sap_config.acs_cfg.vht_seg0_center_ch_freq =
		sap_event->sapevt.sap_ch_selected.vht_seg0_center_ch_freq;
		ap_ctx->sap_config.acs_cfg.vht_seg1_center_ch_freq =
		sap_event->sapevt.sap_ch_selected.vht_seg1_center_ch_freq;
		ap_ctx->sap_config.acs_cfg.ch_width =
			sap_event->sapevt.sap_ch_selected.ch_width;

		cdp_hl_fc_set_td_limit(cds_get_context(QDF_MODULE_ID_SOC),
				       adapter->vdev_id,
				       ap_ctx->operating_chan_freq);
		sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
		if (!sap_ctx) {
			hdd_err("sap ctx is null");
			return QDF_STATUS_E_FAILURE;
		}

		if (sap_ctx->is_chan_change_inprogress) {
			hdd_debug("check for possible hw mode change");
			status = policy_mgr_set_hw_mode_on_channel_switch(
					hdd_ctx->psoc, adapter->vdev_id);
			if (QDF_IS_STATUS_ERROR(status))
				hdd_debug("set hw mode change not done");
		}

		hdd_son_deliver_chan_change_event(
			adapter, sap_event->sapevt.sap_ch_selected.pri_ch_freq);
		return hdd_hostapd_chan_change(adapter, sap_event);
	case eSAP_ACS_SCAN_SUCCESS_EVENT:
		return hdd_handle_acs_scan_event(sap_event, adapter);

	case eSAP_ACS_CHANNEL_SELECTED:
		hdd_son_deliver_acs_complete_event(adapter);
		ap_ctx->sap_config.acs_cfg.pri_ch_freq =
			sap_event->sapevt.sap_ch_selected.pri_ch_freq;
		ap_ctx->sap_config.acs_cfg.ht_sec_ch_freq =
			sap_event->sapevt.sap_ch_selected.ht_sec_ch_freq;
		ap_ctx->sap_config.acs_cfg.vht_seg0_center_ch_freq =
		sap_event->sapevt.sap_ch_selected.vht_seg0_center_ch_freq;
		ap_ctx->sap_config.acs_cfg.vht_seg1_center_ch_freq =
		sap_event->sapevt.sap_ch_selected.vht_seg1_center_ch_freq;
		ap_ctx->sap_config.acs_cfg.ch_width =
			sap_event->sapevt.sap_ch_selected.ch_width;
		hdd_nofl_info("ACS Completed vid %d freq %d BW %d",
			      adapter->vdev_id,
			      ap_ctx->sap_config.acs_cfg.pri_ch_freq,
			      ap_ctx->sap_config.acs_cfg.ch_width);

		if (qdf_atomic_read(&adapter->session.ap.acs_in_progress) &&
		    test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			hdd_dcs_chan_select_complete(adapter);
		} else {
			wlan_hdd_cfg80211_acs_ch_select_evt(adapter);
			wlansap_dcs_set_wlan_interference_mitigation_on_band(
					WLAN_HDD_GET_SAP_CTX_PTR(adapter),
					&ap_ctx->sap_config);
		}

		return QDF_STATUS_SUCCESS;
	case eSAP_ECSA_CHANGE_CHAN_IND:
		hdd_debug("Channel change indication from peer for channel freq %d",
			  sap_event->sapevt.sap_chan_cng_ind.new_chan_freq);
		wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc, adapter->vdev_id,
					    CSA_REASON_PEER_ACTION_FRAME);
		if (hdd_softap_set_channel_change(dev,
			 sap_event->sapevt.sap_chan_cng_ind.new_chan_freq,
			 CH_WIDTH_MAX, false))
			return QDF_STATUS_E_FAILURE;
		else
			return QDF_STATUS_SUCCESS;

	case eSAP_DFS_NEXT_CHANNEL_REQ:
		hdd_debug("Sending next channel query to userspace");
		hdd_update_acs_timer_reason(adapter,
				QCA_WLAN_VENDOR_ACS_SELECT_REASON_DFS);
		return QDF_STATUS_SUCCESS;

	case eSAP_STOP_BSS_DUE_TO_NO_CHNL:
		hdd_debug("Stop sap session[%d]",
			  adapter->vdev_id);
		schedule_work(&adapter->sap_stop_bss_work);
		return QDF_STATUS_SUCCESS;

	case eSAP_CHANNEL_CHANGE_RESP:
		/*
		 * Set the ch_switch_in_progress flag to zero and also enable
		 * roaming once channel change process (success/failure)
		 * is completed
		 */
		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);
		policy_mgr_set_chan_switch_complete_evt(hdd_ctx->psoc);
		wlan_hdd_enable_roaming(adapter,
					RSO_SAP_CHANNEL_CHANGE);

		/* Indoor channels are also marked DFS, therefore
		 * check if the channel has REGULATORY_CHAN_RADAR
		 * channel flag to identify if the channel is DFS
		 */
		if (!wlan_reg_is_dfs_for_freq(hdd_ctx->pdev,
					      ap_ctx->operating_chan_freq)) {
			ap_ctx->dfs_cac_block_tx = false;
			vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
			if (vdev) {
				ucfg_dp_set_dfs_cac_tx(vdev,
						ap_ctx->dfs_cac_block_tx);
				hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
			}
		}

		/* Check any other sap need restart */
		if (ap_ctx->sap_context->csa_reason ==
		    CSA_REASON_UNSAFE_CHANNEL)
			hdd_unsafe_channel_restart_sap(hdd_ctx);
		else if (ap_ctx->sap_context->csa_reason == CSA_REASON_DCS)
			hdd_dcs_hostapd_set_chan(
				hdd_ctx, adapter->vdev_id,
				adapter->session.ap.operating_chan_freq);

		/* Added the sta cnt check as we don't support sta+sap+nan
		 * today. But this needs to be re-visited when we start
		 * supporting this combo.
		 */
		sta_cnt = policy_mgr_mode_specific_connection_count(hdd_ctx->psoc,
								    PM_STA_MODE,
								    NULL);
		if (!sta_cnt)
			policy_mgr_nan_sap_post_enable_conc_check(hdd_ctx->psoc);
		policy_mgr_check_sap_go_force_scc(
				hdd_ctx->psoc, adapter->vdev,
				ap_ctx->sap_context->csa_reason);
		qdf_status = qdf_event_set(&hostapd_state->qdf_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_err("qdf_event_set failed! status: %d",
				qdf_status);
		return hdd_hostapd_chan_change(adapter, sap_event);
	default:
		hdd_debug("SAP message is not handled");
		goto stopbss;
		return QDF_STATUS_SUCCESS;
	}

	hdd_wext_send_event(dev, we_event, &wrqu,
			    (char *)we_custom_event_generic);
	qdf_mem_free(we_custom_start_event);
	qdf_mem_free(unknownSTAEvent);
	qdf_mem_free(maxAssocExceededEvent);

	return QDF_STATUS_SUCCESS;

stopbss:
	{
		uint8_t *we_custom_event;
		char *stopBssEvent = "STOP-BSS.response";       /* 17 */
		int event_len = strlen(stopBssEvent);

		hdd_debug("BSS stop status = %s",
		       sap_event->sapevt.sapStopBssCompleteEvent.status ?
		       "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

		/* Change the BSS state now since, as we are shutting
		 * things down, we don't want interfaces to become
		 * re-enabled
		 */
		hostapd_state->bss_state = BSS_STOP;
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
		if (vdev) {
			ucfg_dp_set_bss_state_start(vdev, false);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		}

		hdd_stop_tsf_sync(adapter);

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(hdd_ctx, true);
#endif

		/* Stop the pkts from n/w stack as we are going to free all of
		 * the TX WMM queues for all STAID's
		 */
		hdd_debug("Disabling queues");
		wlan_hdd_netif_queue_control(adapter,
					WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);

		/* reclaim all resources allocated to the BSS */
		qdf_status = hdd_softap_stop_bss(adapter);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_debug("hdd_softap_stop_bss failed %d",
				  qdf_status);
			if (ucfg_ipa_is_enabled()) {
				ucfg_ipa_uc_disconnect_ap(hdd_ctx->pdev,
							  adapter->dev);
				ucfg_ipa_cleanup_dev_iface(hdd_ctx->pdev,
							   adapter->dev);
			}
		}

		we_custom_event =
			qdf_mem_malloc(HDD_MAX_CUSTOM_START_EVENT_SIZE);
		if (!we_custom_event)
			return QDF_STATUS_E_NOMEM;

		/* notify userspace that the BSS has stopped */
		memset(we_custom_event, '\0',
		       sizeof(HDD_MAX_CUSTOM_START_EVENT_SIZE));
		memcpy(we_custom_event, stopBssEvent, event_len);
		memset(&wrqu, 0, sizeof(wrqu));
		wrqu.data.length = event_len;
		we_event = IWEVCUSTOM;
		we_custom_event_generic = we_custom_event;
		hdd_wext_send_event(dev, we_event, &wrqu,
				    (char *)we_custom_event_generic);

		qdf_mem_free(we_custom_start_event);
		qdf_mem_free(unknownSTAEvent);
		qdf_mem_free(maxAssocExceededEvent);
		qdf_mem_free(we_custom_event);

		/* once the event is set, structure dev/adapter should
		 * not be touched since they are now subject to being deleted
		 * by another thread
		 */
		if (eSAP_STOP_BSS_EVENT == event_id) {
			qdf_event_set(&hostapd_state->qdf_stop_bss_event);
			ucfg_dp_bus_bw_compute_timer_try_stop(hdd_ctx->psoc);
		}

		wlan_hdd_set_tx_flow_info();
	}
	return QDF_STATUS_SUCCESS;
}

static int hdd_softap_unpack_ie(mac_handle_t mac_handle,
				eCsrEncryptionType *encrypt_type,
				eCsrEncryptionType *mc_encrypt_type,
				tCsrAuthList *akm_list,
				bool *mfp_capable,
				bool *mfp_required,
				uint16_t gen_ie_len, uint8_t *gen_ie)
{
	uint32_t ret;
	uint8_t *rsn_ie;
	uint16_t rsn_ie_len, i;
	tDot11fIERSN dot11_rsn_ie = {0};
	tDot11fIEWPA dot11_wpa_ie = {0};
	tDot11fIEWAPI dot11_wapi_ie = {0};

	if (!mac_handle) {
		hdd_err("NULL mac Handle");
		return -EINVAL;
	}
	/* Validity checks */
	if ((gen_ie_len < QDF_MIN(DOT11F_IE_RSN_MIN_LEN, DOT11F_IE_WPA_MIN_LEN))
	    || (gen_ie_len >
		QDF_MAX(DOT11F_IE_RSN_MAX_LEN, DOT11F_IE_WPA_MAX_LEN)))
		return -EINVAL;
	/* Type check */
	if (gen_ie[0] == DOT11F_EID_RSN) {
		/* Validity checks */
		if ((gen_ie_len < DOT11F_IE_RSN_MIN_LEN) ||
		    (gen_ie_len > DOT11F_IE_RSN_MAX_LEN)) {
			return QDF_STATUS_E_FAILURE;
		}
		/* Skip past the EID byte and length byte */
		rsn_ie = gen_ie + 2;
		rsn_ie_len = gen_ie_len - 2;
		/* Unpack the RSN IE */
		memset(&dot11_rsn_ie, 0, sizeof(tDot11fIERSN));
		ret = sme_unpack_rsn_ie(mac_handle, rsn_ie, rsn_ie_len,
					&dot11_rsn_ie, false);
		if (!DOT11F_SUCCEEDED(ret)) {
			hdd_err("unpack failed, 0x%x", ret);
			return -EINVAL;
		}
		/* Copy out the encryption and authentication types */
		hdd_debug("pairwise cipher count: %d akm count:%d",
			  dot11_rsn_ie.pwise_cipher_suite_count,
			  dot11_rsn_ie.akm_suite_cnt);
		/*
		 * Translate akms in akm suite
		 */
		for (i = 0; i < dot11_rsn_ie.akm_suite_cnt; i++)
			akm_list->authType[i] =
				hdd_translate_rsn_to_csr_auth_type(
						       dot11_rsn_ie.akm_suite[i]);
		akm_list->numEntries = dot11_rsn_ie.akm_suite_cnt;
		/* dot11_rsn_ie.pwise_cipher_suite_count */
		*encrypt_type =
			hdd_translate_rsn_to_csr_encryption_type(dot11_rsn_ie.
								 pwise_cipher_suites[0]);
		/* dot11_rsn_ie.gp_cipher_suite_count */
		*mc_encrypt_type =
			hdd_translate_rsn_to_csr_encryption_type(dot11_rsn_ie.
								 gp_cipher_suite);
		/* Set the PMKSA ID Cache for this interface */
		*mfp_capable = 0 != (dot11_rsn_ie.RSN_Cap[0] & 0x80);
		*mfp_required = 0 != (dot11_rsn_ie.RSN_Cap[0] & 0x40);
	} else if (gen_ie[0] == DOT11F_EID_WPA) {
		/* Validity checks */
		if ((gen_ie_len < DOT11F_IE_WPA_MIN_LEN) ||
		    (gen_ie_len > DOT11F_IE_WPA_MAX_LEN)) {
			return QDF_STATUS_E_FAILURE;
		}
		/* Skip past the EID byte and length byte and 4 byte WiFi OUI */
		rsn_ie = gen_ie + 2 + 4;
		rsn_ie_len = gen_ie_len - (2 + 4);
		/* Unpack the WPA IE */
		memset(&dot11_wpa_ie, 0, sizeof(tDot11fIEWPA));
		ret = dot11f_unpack_ie_wpa(MAC_CONTEXT(mac_handle),
					   rsn_ie, rsn_ie_len,
					   &dot11_wpa_ie, false);
		if (!DOT11F_SUCCEEDED(ret)) {
			hdd_err("unpack failed, 0x%x", ret);
			return -EINVAL;
		}
		/* Copy out the encryption and authentication types */
		hdd_debug("WPA unicast cipher suite count: %d akm count: %d",
			  dot11_wpa_ie.unicast_cipher_count,
			  dot11_wpa_ie.auth_suite_count);
		/*
		 * Translate akms in akm suite
		 */
		for (i = 0; i < dot11_wpa_ie.auth_suite_count; i++)
			akm_list->authType[i] =
				hdd_translate_wpa_to_csr_auth_type(
						     dot11_wpa_ie.auth_suites[i]);
		akm_list->numEntries = dot11_wpa_ie.auth_suite_count;
		/* dot11_wpa_ie.unicast_cipher_count */
		*encrypt_type =
			hdd_translate_wpa_to_csr_encryption_type(dot11_wpa_ie.
								 unicast_ciphers[0]);
		/* dot11_wpa_ie.unicast_cipher_count */
		*mc_encrypt_type =
			hdd_translate_wpa_to_csr_encryption_type(dot11_wpa_ie.
								 multicast_cipher);
		*mfp_capable = false;
		*mfp_required = false;
	} else if (gen_ie[0] == DOT11F_EID_WAPI) {
		/* Validity checks */
		if ((gen_ie_len < DOT11F_IE_WAPI_MIN_LEN) ||
		    (gen_ie_len > DOT11F_IE_WAPI_MAX_LEN))
			return QDF_STATUS_E_FAILURE;

		/* Skip past the EID byte and length byte */
		rsn_ie = gen_ie + 2;
		rsn_ie_len = gen_ie_len - 2;
		/* Unpack the WAPI IE */
		memset(&dot11_wapi_ie, 0, sizeof(tDot11fIEWPA));
		ret = dot11f_unpack_ie_wapi(MAC_CONTEXT(mac_handle),
					    rsn_ie, rsn_ie_len,
					    &dot11_wapi_ie, false);
		if (!DOT11F_SUCCEEDED(ret)) {
			hdd_err("unpack failed, 0x%x", ret);
			return -EINVAL;
		}
		/* Copy out the encryption and authentication types */
		hdd_debug("WAPI unicast cipher suite count: %d akm count: %d",
			  dot11_wapi_ie.unicast_cipher_suite_count,
			  dot11_wapi_ie.akm_suite_count);
		/*
		 * Translate akms in akm suite
		 */
		for (i = 0; i < dot11_wapi_ie.akm_suite_count; i++)
			akm_list->authType[i] =
				hdd_translate_wapi_to_csr_auth_type(
						dot11_wapi_ie.akm_suites[i]);

		akm_list->numEntries = dot11_wapi_ie.akm_suite_count;
		/* dot11_wapi_ie.akm_suite_count */
		*encrypt_type =
			hdd_translate_wapi_to_csr_encryption_type(
				dot11_wapi_ie.unicast_cipher_suites[0]);
		/* dot11_wapi_ie.unicast_cipher_count */
		*mc_encrypt_type =
			hdd_translate_wapi_to_csr_encryption_type(
				dot11_wapi_ie.multicast_cipher_suite);
		*mfp_capable = false;
		*mfp_required = false;
	} else {
		hdd_err("gen_ie[0]: %d", gen_ie[0]);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

bool hdd_is_any_sta_connecting(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	struct hdd_station_ctx *sta_ctx;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IS_ANY_STA_CONNECTING;

	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return false;
	}

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if ((adapter->device_mode == QDF_STA_MODE) ||
		    (adapter->device_mode == QDF_P2P_CLIENT_MODE)) {
			if (hdd_cm_is_connecting(adapter)) {
				hdd_debug("vdev_id %d: connecting",
					  adapter->vdev_id);
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return true;
			}
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return false;
}

int hdd_softap_set_channel_change(struct net_device *dev, int target_chan_freq,
				  enum phy_ch_width target_bw, bool forced)
{
	QDF_STATUS status;
	int ret = 0;
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_context *hdd_ctx = NULL;
	struct hdd_adapter *sta_adapter;
	struct hdd_station_ctx *sta_ctx;
	struct sap_context *sap_ctx;
	uint8_t conc_rule1 = 0;
	uint8_t  sta_sap_scc_on_dfs_chnl;
	bool is_p2p_go_session = false;
	struct wlan_objmgr_vdev *vdev;
	bool strict;
	uint32_t sta_cnt = 0;
	struct ch_params ch_params = {0};

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (adapter->device_mode != QDF_SAP_MODE &&
	    adapter->device_mode != QDF_P2P_GO_MODE)
		return -EINVAL;
	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	if (!sap_ctx)
		return -EINVAL;
	/*
	 * If sta connection is in progress do not allow SAP channel change from
	 * user space as it may change the HW mode requirement, for which sta is
	 * trying to connect.
	 */
	if (hdd_is_any_sta_connecting(hdd_ctx)) {
		hdd_err("STA connection is in progress");
		return -EBUSY;
	}

	if (wlan_reg_is_6ghz_chan_freq(target_chan_freq) &&
	    !wlan_reg_is_6ghz_band_set(hdd_ctx->pdev)) {
		hdd_err("6 GHz band disabled");
		return -EINVAL;
	}

	ret = hdd_validate_channel_and_bandwidth(adapter,
						 target_chan_freq, target_bw);
	if (ret) {
		hdd_err("Invalid CH and BW combo");
		return ret;
	}

	sta_adapter = hdd_get_adapter(hdd_ctx, QDF_STA_MODE);
	ucfg_policy_mgr_get_conc_rule1(hdd_ctx->psoc, &conc_rule1);
	/*
	 * conc_custom_rule1:
	 * Force SCC for SAP + STA
	 * if STA is already connected then we shouldn't allow
	 * channel switch in SAP interface.
	 */
	if (sta_adapter && conc_rule1) {
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(sta_adapter);
		if (hdd_cm_is_vdev_associated(sta_adapter)) {
			hdd_err("Channel switch not allowed after STA connection with conc_custom_rule1 enabled");
			return -EBUSY;
		}
	}

	sta_cnt = policy_mgr_mode_specific_connection_count(hdd_ctx->psoc,
							    PM_STA_MODE,
							    NULL);
	/*
	 * For non-dbs HW, don't allow Channel switch on DFS channel if STA is
	 * not connected and sta_sap_scc_on_dfs_chnl is 1.
	 */
	status = policy_mgr_get_sta_sap_scc_on_dfs_chnl(
				hdd_ctx->psoc, &sta_sap_scc_on_dfs_chnl);
	if (QDF_STATUS_SUCCESS != status) {
		return status;
	}

	if (!policy_mgr_is_sap_allowed_on_indoor(hdd_ctx->pdev,
						 adapter->vdev_id,
						 target_chan_freq)) {
		hdd_debug("Channel switch is not allowed to indoor frequency %d",
			  target_chan_freq);
		return -EINVAL;
	}

	if (!sta_cnt && !policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc) &&
	    (sta_sap_scc_on_dfs_chnl ==
	     PM_STA_SAP_ON_DFS_MASTER_MODE_DISABLED) &&
	    (wlan_reg_is_dfs_for_freq(hdd_ctx->pdev, target_chan_freq) ||
	    (wlan_reg_is_5ghz_ch_freq(target_chan_freq) &&
	     target_bw == CH_WIDTH_160MHZ))) {
		hdd_debug("Channel switch not allowed for non-DBS HW on DFS channel %d width %d", target_chan_freq, target_bw);
		return -EINVAL;
	}

	/*
	 * Set the ch_switch_in_progress flag to mimic channel change
	 * when a radar is found. This will enable synchronizing
	 * SAP and HDD states similar to that of radar indication.
	 * Suspend the netif queues to stop queuing Tx frames
	 * from upper layers.  netif queues will be resumed
	 * once the channel change is completed and SAP will
	 * post eSAP_START_BSS_EVENT success event to HDD.
	 */
	if (qdf_atomic_inc_return(&adapter->ch_switch_in_progress) > 1) {
		hdd_err("Channel switch in progress!!");
		return -EBUSY;
	}
	ch_params.ch_width = target_bw;
	target_bw = wlansap_get_csa_chanwidth_from_phymode(
			sap_ctx, target_chan_freq, &ch_params);
	/*
	 * Do SAP concurrency check to cover channel switch case as following:
	 * There is already existing SAP+GO combination but due to upper layer
	 * notifying LTE-COEX event or sending command to move one connection
	 * to different channel. Before moving existing connection to new
	 * channel, check if new channel can co-exist with the other existing
	 * connection. For example, SAP1 is on channel-6 and SAP2 is on
	 * channel-36 and lets say they are doing DBS, and upper layer sends
	 * LTE-COEX to move SAP1 from channel-6 to channel-149. SAP1 and
	 * SAP2 will end up doing MCC which may not be desirable result. It
	 * should will be prevented.
	 */
	if (!policy_mgr_allow_concurrency_csa(
				hdd_ctx->psoc,
				policy_mgr_convert_device_mode_to_qdf_type(
					adapter->device_mode),
				target_chan_freq, policy_mgr_get_bw(target_bw),
				adapter->vdev_id,
				forced,
				sap_ctx->csa_reason)) {
		hdd_err("Channel switch failed due to concurrency check failure");
		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);
		return -EINVAL;
	}

	/*
	 * Reject channel change req  if reassoc in progress on any adapter.
	 * sme_is_any_session_in_middle_of_roaming is for LFR2 and
	 * hdd_is_roaming_in_progress is for LFR3
	 */
	if (sme_is_any_session_in_middle_of_roaming(hdd_ctx->mac_handle) ||
	    hdd_is_roaming_in_progress(hdd_ctx)) {
		hdd_info("Channel switch not allowed as reassoc in progress");
		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);
		return -EINVAL;
	}
	/* Disable Roaming on all adapters before doing channel change */
	wlan_hdd_disable_roaming(adapter, RSO_SAP_CHANNEL_CHANGE);

	/*
	 * Post the Channel Change request to SAP.
	 */

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	if (!vdev) {
		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);
		wlan_hdd_enable_roaming(adapter, RSO_SAP_CHANNEL_CHANGE);
		return -EINVAL;
	}
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE)
		is_p2p_go_session = true;
	else
		forced = wlansap_override_csa_strict_for_sap(
					hdd_ctx->mac_handle, sap_ctx,
					target_chan_freq, forced);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);

	strict = is_p2p_go_session;
	strict = strict || forced;
	hdd_place_marker(adapter, "CHANNEL CHANGE", NULL);
	status = wlansap_set_channel_change_with_csa(
		WLAN_HDD_GET_SAP_CTX_PTR(adapter),
		target_chan_freq,
		target_bw,
		strict);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("SAP set channel failed for channel freq: %d, bw: %d",
		        target_chan_freq, target_bw);
		/*
		 * If channel change command fails then clear the
		 * radar found flag and also restart the netif
		 * queues.
		 */
		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);

		/*
		 * If Posting of the Channel Change request fails
		 * enable roaming on all adapters
		 */
		wlan_hdd_enable_roaming(adapter,
					RSO_SAP_CHANNEL_CHANGE);

		ret = -EINVAL;
	}

	return ret;
}


#if defined(FEATURE_WLAN_CH_AVOID) && defined(FEATURE_WLAN_CH_AVOID_EXT)
/**
 * wlan_hdd_get_sap_restriction_mask() - get restriction mask for sap
 * after sap start
 * @hdd_ctx: hdd context
 *
 * Return: Restriction mask
 */
static inline
uint32_t wlan_hdd_get_sap_restriction_mask(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->coex_avoid_freq_list.restriction_mask;
}

#else
static inline
uint32_t wlan_hdd_get_sap_restriction_mask(struct hdd_context *hdd_ctx)
{
	return -EINVAL;
}
#endif

void hdd_stop_sap_set_tx_power(struct wlan_objmgr_psoc *psoc,
			       struct hdd_adapter *adapter)
{
	struct wlan_objmgr_vdev *vdev =
		hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	struct wlan_objmgr_pdev *pdev;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct qdf_mac_addr bssid;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	int32_t set_tx_power, tx_power = 0;
	struct sap_context *sap_ctx;
	uint32_t restriction_mask;
	int ch_loop, unsafe_chan_count;
	struct unsafe_ch_list *unsafe_ch_list;
	uint32_t chan_freq;
	bool is_valid_txpower = false;

	if (!vdev)
		return;
	pdev = wlan_vdev_get_pdev(vdev);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return;
	}

	restriction_mask = wlan_hdd_get_sap_restriction_mask(hdd_ctx);
	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	chan_freq = sap_ctx->chan_freq;
	unsafe_ch_list = &psoc_priv_obj->unsafe_chan_list;

	hdd_debug("Restriction_mask %d CSA reason %d ", restriction_mask,
		  sap_ctx->csa_reason);

	if (sap_ctx->csa_reason == CSA_REASON_UNSAFE_CHANNEL) {
		if (restriction_mask & BIT(QDF_SAP_MODE)) {
			schedule_work(&adapter->sap_stop_bss_work);
		} else {
			unsafe_chan_count = unsafe_ch_list->chan_cnt;
			qdf_copy_macaddr(&bssid, &adapter->mac_addr);
			set_tx_power =
			wlan_reg_get_channel_reg_power_for_freq(pdev,
								chan_freq);
			for (ch_loop = 0; ch_loop < unsafe_chan_count;
			     ch_loop++) {
				if (unsafe_ch_list->chan_freq_list[ch_loop] ==
				    chan_freq) {
					tx_power =
					unsafe_ch_list->txpower[ch_loop];
					is_valid_txpower =
					unsafe_ch_list->is_valid_txpower[ch_loop];
					break;
				}
			}

			if (is_valid_txpower)
				set_tx_power = QDF_MIN(set_tx_power, tx_power);

			if (QDF_STATUS_SUCCESS !=
				sme_set_tx_power(hdd_ctx->mac_handle,
						 adapter->vdev_id, bssid,
						 adapter->device_mode,
						 set_tx_power)) {
				hdd_err("Setting tx power failed");
			}
		}
	}
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
QDF_STATUS hdd_sap_restart_with_channel_switch(struct wlan_objmgr_psoc *psoc,
					       struct hdd_adapter *ap_adapter,
					       uint32_t target_chan_freq,
					       uint32_t target_bw,
					       bool forced)
{
	struct net_device *dev = ap_adapter->dev;
	int ret;

	hdd_enter();

	if (!dev) {
		hdd_err("Invalid dev pointer");
		return QDF_STATUS_E_INVAL;
	}

	ret = hdd_softap_set_channel_change(dev, target_chan_freq,
					    target_bw, forced);
	if (ret && ret != -EBUSY) {
		hdd_err("channel switch failed");
		hdd_stop_sap_set_tx_power(psoc, ap_adapter);
	}

	return qdf_status_from_os_return(ret);
}

QDF_STATUS hdd_sap_restart_chan_switch_cb(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id, uint32_t ch_freq,
					  uint32_t channel_bw, bool forced)
{
	struct hdd_adapter *ap_adapter =
		wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);

	if (!ap_adapter) {
		hdd_err("Adapter is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return hdd_sap_restart_with_channel_switch(psoc,
						   ap_adapter,
						   ch_freq,
						   channel_bw, forced);
}

QDF_STATUS wlan_hdd_check_cc_intf_cb(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id, uint32_t *ch_freq)
{
	struct hdd_adapter *ap_adapter;
	struct sap_context *sap_context;

	ap_adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (!ap_adapter) {
		hdd_err("ap_adapter is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		hdd_err("SOFTAP_BSS_STARTED not set");
		return QDF_STATUS_E_FAILURE;
	}

	sap_context = WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter);
	if (!sap_context) {
		hdd_err("sap_context is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_IS_STATUS_ERROR(wlansap_context_get(sap_context))) {
		hdd_err("sap_context is invalid");
		return QDF_STATUS_E_FAILURE;
	}

	*ch_freq = wlansap_check_cc_intf(sap_context);
	wlansap_context_put(sap_context);

	return QDF_STATUS_SUCCESS;
}

void wlan_hdd_set_sap_csa_reason(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 uint8_t reason)
{
	struct sap_context *sap_ctx;
	struct hdd_adapter *ap_adapter = wlan_hdd_get_adapter_from_vdev(
				psoc, vdev_id);
	if (!ap_adapter) {
		hdd_err("ap adapter is NULL");
		return;
	}
	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter);
	if (sap_ctx)
		sap_ctx->csa_reason = reason;
	hdd_nofl_debug("set csa reason %d %s vdev %d",
		       reason, sap_get_csa_reason_str(reason), vdev_id);
}

QDF_STATUS wlan_hdd_get_channel_for_sap_restart(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, uint32_t *ch_freq)
{
	mac_handle_t mac_handle;
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct hdd_context *hdd_ctx;
	uint8_t mcc_to_scc_switch = 0;
	struct ch_params ch_params = {0};
	struct hdd_adapter *ap_adapter = wlan_hdd_get_adapter_from_vdev(
					psoc, vdev_id);
	uint32_t sap_ch_freq, intf_ch_freq, temp_ch_freq;
	struct sap_context *sap_context;
	enum sap_csa_reason_code csa_reason =
		CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL;
	QDF_STATUS status;
	bool use_sap_original_bw = false;

	if (!ap_adapter) {
		hdd_err("ap_adapter is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!ch_freq) {
		hdd_err("Null parameters");
		return QDF_STATUS_E_FAILURE;
	}

	if (!test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		hdd_err("SOFTAP_BSS_STARTED not set");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle) {
		hdd_err("mac_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	sap_context = hdd_ap_ctx->sap_context;
	if (!sap_context) {
		hdd_err("sap_context is null");
		return QDF_STATUS_E_FAILURE;
	}
	if (QDF_IS_STATUS_ERROR(wlansap_context_get(sap_context))) {
		hdd_err("sap_context is invalid");
		return QDF_STATUS_E_FAILURE;
	}
	wlan_hdd_set_sap_csa_reason(psoc, vdev_id, csa_reason);

	policy_mgr_get_original_bw_for_sap_restart(psoc, &use_sap_original_bw);
	if (use_sap_original_bw)
		ch_params.ch_width = sap_context->ch_width_orig;
	else
		ch_params.ch_width = CH_WIDTH_MAX;

	intf_ch_freq = wlansap_get_chan_band_restrict(sap_context, &csa_reason);
	if (intf_ch_freq)
		goto sap_restart;

	/*
	 * If STA+SAP sessions are on DFS channel and STA+SAP SCC is
	 * enabled on DFS channel then move the SAP out of DFS channel
	 * as soon as STA gets disconnect.
	 */
	if (policy_mgr_is_sap_restart_required_after_sta_disconnect(
	    psoc, vdev_id, &intf_ch_freq,
	    !!ap_adapter->session.ap.sap_config.acs_cfg.acs_mode)) {
		hdd_debug("Move the sap (vdev %d) to user configured channel %u",
			  vdev_id, intf_ch_freq);
		goto sap_restart;
	}

	if (ap_adapter->device_mode == QDF_P2P_GO_MODE &&
	    !policy_mgr_go_scc_enforced(psoc)) {
		wlansap_context_put(sap_context);
		hdd_debug("p2p go no scc required");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * If liberal mode is enabled. If P2P-Cli is not yet connected
	 * Skipping CSA as this is done as part of set_key
	 */

	if (ap_adapter->device_mode == QDF_P2P_GO_MODE &&
	    policy_mgr_go_scc_enforced(psoc) &&
	    !policy_mgr_is_go_scc_strict(psoc) &&
	    (wlan_vdev_get_peer_count(sap_context->vdev) == 1)) {
		hdd_debug("p2p go liberal mode enabled. Skipping CSA");
		wlansap_context_put(sap_context);
		return QDF_STATUS_E_FAILURE;
	}

	ucfg_policy_mgr_get_mcc_scc_switch(hdd_ctx->psoc,
					   &mcc_to_scc_switch);
	policy_mgr_get_chan_by_session_id(psoc, vdev_id, &sap_ch_freq);
	if (!policy_mgr_is_restart_sap_required(hdd_ctx->psoc, vdev_id,
						sap_ch_freq,
						mcc_to_scc_switch)) {
		wlansap_context_put(sap_context);
		hdd_debug("SAP needn't restart");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Check if STA's channel is DFS or passive or part of LTE avoided
	 * channel list. In that case move SAP to other band if DBS is
	 * supported, return from here if DBS is not supported.
	 * Need to take care of 3 port cases with 2 STA iface in future.
	 */
	intf_ch_freq = wlansap_check_cc_intf(sap_context);
	hdd_debug("sap_vdev %d intf_ch: %d, orig freq: %d",
		  vdev_id, intf_ch_freq, sap_ch_freq);

	temp_ch_freq = intf_ch_freq ? intf_ch_freq : sap_ch_freq;
	wlansap_get_csa_chanwidth_from_phymode(sap_context, temp_ch_freq,
					       &ch_params);
	if (QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION !=
		mcc_to_scc_switch) {
		if (QDF_IS_STATUS_ERROR(
		    policy_mgr_valid_sap_conc_channel_check(
		    hdd_ctx->psoc, &intf_ch_freq, sap_ch_freq, vdev_id,
		    &ch_params))) {
			schedule_work(&ap_adapter->sap_stop_bss_work);
			wlansap_context_put(sap_context);
			hdd_debug("can't move sap to chan(freq): %u, stopping SAP",
				  intf_ch_freq);
			return QDF_STATUS_E_FAILURE;
		}
	}

sap_restart:
	if (!intf_ch_freq) {
		hdd_debug("Unable to find safe channel, Hence stop the SAP or Set Tx power");
		sap_context->csa_reason = csa_reason;
		hdd_stop_sap_set_tx_power(psoc, ap_adapter);
		wlansap_context_put(sap_context);
		return QDF_STATUS_E_FAILURE;
	} else {
		sap_context->csa_reason = csa_reason;
	}
	if (ch_params.ch_width == CH_WIDTH_MAX)
		wlansap_get_csa_chanwidth_from_phymode(
					sap_context, intf_ch_freq,
					&ch_params);

	hdd_debug("mhz_freq_seg0: %d, ch_width: %d",
		  ch_params.mhz_freq_seg0, ch_params.ch_width);
	if (sap_context->csa_reason == CSA_REASON_UNSAFE_CHANNEL &&
	    (!policy_mgr_check_bw_with_unsafe_chan_freq(hdd_ctx->psoc,
							ch_params.mhz_freq_seg0,
							ch_params.ch_width))) {
		hdd_debug("SAP bw shrink to 20M for unsafe");
		ch_params.ch_width = CH_WIDTH_20MHZ;
	}

	hdd_debug("SAP restart orig chan freq: %d, new freq: %d bw %d",
		  hdd_ap_ctx->sap_config.chan_freq, intf_ch_freq,
		  ch_params.ch_width);
	hdd_ap_ctx->bss_stop_reason = BSS_STOP_DUE_TO_MCC_SCC_SWITCH;
	*ch_freq = intf_ch_freq;
	hdd_debug("SAP channel change with CSA/ECSA");
	status = hdd_sap_restart_chan_switch_cb(psoc, vdev_id, *ch_freq,
						ch_params.ch_width, false);
	wlansap_context_put(sap_context);

	return status;
}

QDF_STATUS
wlan_get_sap_acs_band(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		      uint32_t *acs_band)
{
	struct hdd_adapter *ap_adapter = wlan_hdd_get_adapter_from_vdev(psoc,
								vdev_id);
	struct sap_config *sap_config;

	if (!ap_adapter || (ap_adapter->device_mode != QDF_SAP_MODE &&
			    ap_adapter->device_mode != QDF_P2P_GO_MODE)) {
		hdd_err("invalid adapter");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * If acs mode is false, that means acs is disabled and acs band can be
	 * QCA_ACS_MODE_IEEE80211ANY
	 */
	sap_config = &ap_adapter->session.ap.sap_config;
	if (sap_config->acs_cfg.acs_mode == false) {
		*acs_band = QCA_ACS_MODE_IEEE80211ANY;
		return QDF_STATUS_SUCCESS;
	}

	*acs_band = sap_config->acs_cfg.band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_get_ap_prefer_conc_ch_params(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id, uint32_t chan_freq,
		struct ch_params *ch_params)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct sap_context *sap_context;
	struct hdd_adapter *ap_adapter = wlan_hdd_get_adapter_from_vdev(
					psoc, vdev_id);

	if (!ap_adapter || (ap_adapter->device_mode != QDF_SAP_MODE &&
			    ap_adapter->device_mode != QDF_P2P_GO_MODE)) {
		hdd_err("invalid adapter");
		return QDF_STATUS_E_FAILURE;
	}
	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	sap_context = hdd_ap_ctx->sap_context;
	if (!sap_context) {
		hdd_err("sap_context is null");
		return QDF_STATUS_E_FAILURE;
	}
	if (QDF_IS_STATUS_ERROR(wlansap_context_get(sap_context))) {
		hdd_err("sap_context is invalid");
		return QDF_STATUS_E_FAILURE;
	}

	wlansap_get_csa_chanwidth_from_phymode(sap_context,
					       chan_freq,
					       ch_params);
	wlansap_context_put(sap_context);

	return QDF_STATUS_SUCCESS;
}

#if defined(CONFIG_BAND_6GHZ) && defined(WLAN_FEATURE_11AX)
uint32_t hdd_get_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	int32_t keymgmt;
	struct hdd_adapter *ap_adapter;
	struct hdd_ap_ctx *ap_ctx;
	struct sap_context *sap_context;
	struct sap_config *sap_config;
	uint32_t capable = 0;
	enum policy_mgr_con_mode conn_mode;

	if (!psoc) {
		hdd_err("PSOC is NULL");
		return 0;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_HDD_ID_OBJ_MGR);
	if (!vdev) {
		hdd_err("vdev is NULL %d", vdev_id);
		return 0;
	}

	ap_adapter = wlan_hdd_get_adapter_from_vdev(
					psoc, vdev_id);
	if (!ap_adapter) {
		hdd_err("ap_adapter is NULL %d", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);
		return 0;
	}

	conn_mode = policy_mgr_convert_device_mode_to_qdf_type(
			ap_adapter->device_mode);
	if ((conn_mode != PM_SAP_MODE && conn_mode != PM_P2P_GO_MODE) ||
	    !policy_mgr_is_6ghz_conc_mode_supported(psoc, conn_mode)) {
		hdd_err("unexpected device mode %d", ap_adapter->device_mode);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);
		return 0;
	}

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	sap_config = &ap_ctx->sap_config;
	sap_context = ap_ctx->sap_context;
	if (QDF_IS_STATUS_ERROR(wlansap_context_get(sap_context))) {
		hdd_err("sap_context is get failed %d", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);
		return 0;
	}
	/* SAP is allowed on 6GHz with explicit indication from user space:
	 * a. SAP is started on 6Ghz already.
	 * b. SAP is configured on 6Ghz fixed channel from userspace.
	 * c. SAP is configured by ACS range which includes any 6Ghz channel.
	 */
	if (test_bit(SOFTAP_BSS_STARTED, &ap_adapter->event_flags)) {
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(
				ap_ctx->operating_chan_freq))
			capable |= CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED;
	} else {
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_config->chan_freq))
			capable |= CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED;
		else if (sap_context && WLAN_REG_IS_6GHZ_CHAN_FREQ(
				sap_context->chan_freq))
			capable |= CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED;
	}
	if (wlansap_is_6ghz_included_in_acs_range(sap_context))
		capable |= CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED;

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0) {
		hdd_err("Invalid mgmt cipher");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);
		return 0;
	}

	/*
	 * 6 GHz SAP is allowed in open mode only if the
	 * check_6ghz_security ini is disabled.
	 */
	if (!cfg_get(psoc, CFG_CHECK_6GHZ_SECURITY) &&
	    (!keymgmt || (keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_NONE))))
		capable |= CONN_6GHZ_FLAG_SECURITY_ALLOWED;

	if ((keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE |
			1 << WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B |
			1 << WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192 |
			1 << WLAN_CRYPTO_KEY_MGMT_OWE |
			1 << WLAN_CRYPTO_KEY_MGMT_SAE_EXT_KEY))) {
		capable |= CONN_6GHZ_FLAG_SECURITY_ALLOWED;
	}
	capable |= CONN_6GHZ_FLAG_VALID;
	hdd_debug("vdev_id %d keymgmt 0x%08x capable 0x%x",
		  vdev_id, keymgmt, capable);
	wlansap_context_put(sap_context);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);

	return capable;
}
#else
uint32_t hdd_get_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	return 0;
}
#endif
#endif

#ifdef WLAN_FEATURE_TSF_PTP
static const struct ethtool_ops wlan_hostapd_ethtool_ops = {
	.get_ts_info = wlan_get_ts_info,
};
#endif

const struct net_device_ops net_ops_struct = {
	.ndo_open = hdd_hostapd_open,
	.ndo_stop = hdd_hostapd_stop,
	.ndo_uninit = hdd_hostapd_uninit,
	.ndo_start_xmit = hdd_softap_hard_start_xmit,
	.ndo_tx_timeout = hdd_softap_tx_timeout,
	.ndo_get_stats = hdd_get_stats,
	.ndo_set_mac_address = hdd_hostapd_set_mac_address,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
	.ndo_do_ioctl = hdd_ioctl,
#endif
	.ndo_change_mtu = hdd_hostapd_change_mtu,
	.ndo_select_queue = hdd_select_queue,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	.ndo_siocdevprivate = hdd_dev_private_ioctl,
#endif
};

#ifdef WLAN_FEATURE_TSF_PTP
void hdd_set_ap_ops(struct net_device *dev)
{
	dev->netdev_ops = &net_ops_struct;
	dev->ethtool_ops = &wlan_hostapd_ethtool_ops;
}
#else
void hdd_set_ap_ops(struct net_device *dev)
{
	dev->netdev_ops = &net_ops_struct;
}
#endif

bool hdd_sap_create_ctx(struct hdd_adapter *adapter)
{
	hdd_debug("creating sap context");
	adapter->session.ap.sap_context = sap_create_ctx();
	if (adapter->session.ap.sap_context)
		return true;

	return false;
}

bool hdd_sap_destroy_ctx(struct hdd_adapter *adapter)
{
	struct sap_context *sap_ctx = adapter->session.ap.sap_context;

	if (adapter->session.ap.beacon) {
		qdf_mem_free(adapter->session.ap.beacon);
		adapter->session.ap.beacon = NULL;
	}

	if (!sap_ctx) {
		hdd_debug("sap context is NULL");
		return true;
	}

	hdd_debug("destroying sap context");

	if (QDF_IS_STATUS_ERROR(sap_destroy_ctx(sap_ctx)))
		return false;

	adapter->session.ap.sap_context = NULL;

	return true;
}

void hdd_sap_destroy_ctx_all(struct hdd_context *hdd_ctx, bool is_ssr)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;

	/* sap_ctx is not destroyed as it will be leveraged for sap restart */
	if (is_ssr)
		return;

	hdd_debug("destroying all the sap context");

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_SAP_DESTROY_CTX_ALL) {
		if (adapter->device_mode == QDF_SAP_MODE)
			hdd_sap_destroy_ctx(adapter);
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_SAP_DESTROY_CTX_ALL);
	}
}

static void
hdd_indicate_peers_deleted(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct hdd_adapter *adapter;

	if (!psoc) {
		hdd_err("psoc obj is NULL");
		return;
	}

	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (hdd_validate_adapter(adapter)) {
		hdd_err("invalid adapter");
		return;
	}

	hdd_sap_indicate_disconnect_for_sta(adapter);
}

QDF_STATUS hdd_init_ap_mode(struct hdd_adapter *adapter, bool reinit)
{
	struct hdd_hostapd_state *phostapdBuf;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct sap_context *sap_ctx;
	int ret;
	enum dfs_mode acs_dfs_mode;
	bool acs_with_more_param = 0;
	uint8_t enable_sifs_burst = 0;
	bool is_6g_sap_fd_enabled = 0;
	enum reg_6g_ap_type ap_pwr_type;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	hdd_debug("SSR in progress: %d", reinit);
	qdf_atomic_init(&adapter->session.ap.acs_in_progress);

	sap_ctx = hdd_hostapd_init_sap_session(adapter, reinit);
	if (!sap_ctx) {
		hdd_err("Invalid sap_ctx");
		goto error_release_vdev;
	}

	if (!reinit) {
		adapter->session.ap.sap_config.chan_freq =
					      hdd_ctx->acs_policy.acs_chan_freq;
		acs_dfs_mode = hdd_ctx->acs_policy.acs_dfs_mode;
		adapter->session.ap.sap_config.acs_dfs_mode =
			wlan_hdd_get_dfs_mode(acs_dfs_mode);
	}

	status = ucfg_mlme_get_acs_with_more_param(hdd_ctx->psoc,
						   &acs_with_more_param);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("can't get sap acs with more param, use def");

	wlan_sap_set_acs_with_more_param(hdd_ctx->mac_handle,
					 acs_with_more_param);

	/* Allocate the Wireless Extensions state structure */
	phostapdBuf = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

	ap_pwr_type = wlan_reg_decide_6g_ap_pwr_type(hdd_ctx->pdev);
	hdd_debug("selecting AP power type %d", ap_pwr_type);

	/* Zero the memory.  This zeros the profile structure. */
	memset(phostapdBuf, 0, sizeof(struct hdd_hostapd_state));

	status = qdf_event_create(&phostapdBuf->qdf_event);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Hostapd HDD qdf event init failed!!");
		goto error_deinit_sap_session;
	}

	status = qdf_event_create(&phostapdBuf->qdf_stop_bss_event);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Hostapd HDD stop bss event init failed!!");
		goto error_deinit_sap_session;
	}

	status = qdf_event_create(&phostapdBuf->qdf_sta_disassoc_event);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Hostapd HDD sta disassoc event init failed!!");
		goto error_deinit_sap_session;
	}

	status = qdf_event_create(&phostapdBuf->qdf_sta_eap_frm_done_event);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Hostapd HDD sta eap frm done event init failed!!");
		goto error_deinit_sap_session;
	}

	/* Register as a wireless device */
	hdd_register_hostapd_wext(adapter->dev);

	/* Cache station count initialize to zero */
	qdf_atomic_init(&adapter->cache_sta_count);

	/* Initialize the data path module */
	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
	if (vdev)
		ucfg_dp_softap_init_txrx(vdev);

	status = hdd_wmm_adapter_init(adapter);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("hdd_wmm_adapter_init() failed code: %08d [x%08x]",
		       status, status);
		goto error_release_softap_tx_rx;
	}

	set_bit(WMM_INIT_DONE, &adapter->event_flags);

	status = ucfg_get_enable_sifs_burst(hdd_ctx->psoc, &enable_sifs_burst);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to get sifs burst value, use default");

	ret = wma_cli_set_command(adapter->vdev_id,
				  wmi_pdev_param_burst_enable,
				  enable_sifs_burst,
				  PDEV_CMD);
	if (0 != ret)
		hdd_err("wmi_pdev_param_burst_enable set failed: %d", ret);


	ucfg_mlme_is_6g_sap_fd_enabled(hdd_ctx->psoc, &is_6g_sap_fd_enabled);
	hdd_debug("6g sap fd enabled %d", is_6g_sap_fd_enabled);
	if (is_6g_sap_fd_enabled && vdev)
		wlan_vdev_mlme_feat_ext_cap_set(vdev,
						WLAN_VDEV_FEXT_FILS_DISC_6G_SAP);

	hdd_set_netdev_flags(adapter);

	if (!reinit) {
		adapter->session.ap.sap_config.acs_cfg.acs_mode = false;
		wlansap_dcs_set_vdev_wlan_interference_mitigation(sap_ctx,
								  false);
		wlansap_dcs_set_vdev_starting(sap_ctx, false);
		qdf_mem_zero(&adapter->session.ap.sap_config.acs_cfg,
			     sizeof(struct sap_acs_cfg));
	}

	sme_set_del_peers_ind_callback(hdd_ctx->mac_handle,
				       &hdd_indicate_peers_deleted);
	/* rcpi info initialization */
	qdf_mem_zero(&adapter->rcpi, sizeof(adapter->rcpi));

	if (vdev)
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	hdd_exit();

	return status;

error_release_softap_tx_rx:
	hdd_unregister_wext(adapter->dev);
	if (vdev) {
		ucfg_dp_softap_deinit_txrx(vdev);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}
error_deinit_sap_session:
	hdd_hostapd_deinit_sap_session(adapter);
error_release_vdev:
	hdd_exit();
	return status;
}

void hdd_deinit_ap_mode(struct hdd_context *hdd_ctx,
			struct hdd_adapter *adapter,
			bool rtnl_held)
{
	struct wlan_objmgr_vdev *vdev;

	hdd_enter_dev(adapter->dev);

	if (test_bit(WMM_INIT_DONE, &adapter->event_flags)) {
		hdd_wmm_adapter_close(adapter);
		clear_bit(WMM_INIT_DONE, &adapter->event_flags);
	}
	qdf_atomic_set(&adapter->session.ap.acs_in_progress, 0);
	if (qdf_atomic_read(&adapter->ch_switch_in_progress)) {
		qdf_atomic_set(&adapter->ch_switch_in_progress, 0);
		policy_mgr_set_chan_switch_complete_evt(hdd_ctx->psoc);

		/* Re-enable roaming on all connected STA vdev */
		wlan_hdd_enable_roaming(adapter, RSO_SAP_CHANNEL_CHANGE);
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
	if (vdev) {
		ucfg_dp_softap_deinit_txrx(vdev);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}

	if (hdd_hostapd_deinit_sap_session(adapter))
		hdd_err("Failed:hdd_hostapd_deinit_sap_session");

	hdd_exit();
}

/**
 * hdd_wlan_create_ap_dev() - create an AP-mode device
 * @hdd_ctx: Global HDD context
 * @mac_addr: MAC address to assign to the interface
 * @name_assign_type: the name of assign type of the netdev
 * @iface_name: User-visible name of the interface
 *
 * This function will allocate a Linux net_device and configuration it
 * for an AP mode of operation.  Note that the device is NOT actually
 * registered with the kernel at this time.
 *
 * Return: A pointer to the private data portion of the net_device if
 * the allocation and initialization was successful, NULL otherwise.
 */
struct hdd_adapter *hdd_wlan_create_ap_dev(struct hdd_context *hdd_ctx,
				      tSirMacAddr mac_addr,
				      unsigned char name_assign_type,
				      uint8_t *iface_name)
{
	struct net_device *dev;
	struct hdd_adapter *adapter;

	hdd_debug("iface_name = %s", iface_name);

	dev = alloc_netdev_mqs(sizeof(struct hdd_adapter), iface_name,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) || defined(WITH_BACKPORTS)
			       name_assign_type,
#endif
			       ether_setup, NUM_TX_QUEUES, NUM_RX_QUEUES);

	if (!dev)
		return NULL;

	adapter = netdev_priv(dev);

	/* Init the net_device structure */
	ether_setup(dev);

	/* Initialize the adapter context to zeros. */
	qdf_mem_zero(adapter, sizeof(struct hdd_adapter));
	adapter->dev = dev;
	adapter->hdd_ctx = hdd_ctx;
	adapter->magic = WLAN_HDD_ADAPTER_MAGIC;
	adapter->vdev_id = WLAN_UMAC_VDEV_ID_MAX;

	hdd_debug("dev = %pK, adapter = %pK, concurrency_mode=0x%x",
		dev, adapter,
		(int)policy_mgr_get_concurrency_mode(hdd_ctx->psoc));

	/* Init the net_device structure */
	strlcpy(dev->name, (const char *)iface_name, IFNAMSIZ);

	hdd_set_ap_ops(dev);

	dev->watchdog_timeo = HDD_TX_TIMEOUT;
	dev->mtu = HDD_DEFAULT_MTU;
	dev->tx_queue_len = HDD_NETDEV_TX_QUEUE_LEN;

	qdf_net_update_net_device_dev_addr(dev, mac_addr, sizeof(tSirMacAddr));
	qdf_mem_copy(adapter->mac_addr.bytes, mac_addr, sizeof(tSirMacAddr));

	hdd_update_dynamic_tsf_sync(adapter);
	hdd_dev_setup_destructor(dev);
	dev->ieee80211_ptr = &adapter->wdev;
	adapter->wdev.wiphy = hdd_ctx->wiphy;
	adapter->wdev.netdev = dev;

	init_completion(&adapter->vdev_destroy_event);

	SET_NETDEV_DEV(dev, hdd_ctx->parent_dev);
	spin_lock_init(&adapter->pause_map_lock);
	adapter->start_time = adapter->last_time = qdf_system_ticks();

	qdf_atomic_init(&adapter->ch_switch_in_progress);

	return adapter;
}

/**
 * wlan_hdd_rate_is_11g() - check if rate is 11g rate or not
 * @rate: Rate to be checked
 *
 * Return: true if rate if 11g else false
 */
static bool wlan_hdd_rate_is_11g(u8 rate)
{
	static const u8 gRateArray[8] = {12, 18, 24, 36, 48, 72,
					 96, 108}; /* actual rate * 2 */
	u8 i;

	for (i = 0; i < 8; i++) {
		if (rate == gRateArray[i])
			return true;
	}
	return false;
}

#ifdef QCA_HT_2040_COEX
/**
 * wlan_hdd_get_sap_obss() - Get SAP OBSS enable config based on HT_CAPAB IE
 * @adapter: Pointer to hostapd adapter
 *
 * Return: HT support channel width config value
 */
static bool wlan_hdd_get_sap_obss(struct hdd_adapter *adapter)
{
	uint32_t ret;
	const uint8_t *ie;
	uint8_t ht_cap_ie[DOT11F_IE_HTCAPS_MAX_LEN];
	tDot11fIEHTCaps dot11_ht_cap_ie = {0};
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_beacon_data *beacon = adapter->session.ap.beacon;
	mac_handle_t mac_handle;

	mac_handle = hdd_ctx->mac_handle;
	ie = wlan_get_ie_ptr_from_eid(WLAN_EID_HT_CAPABILITY,
				      beacon->tail, beacon->tail_len);
	if (ie && ie[1] && (ie[1] <= DOT11F_IE_HTCAPS_MAX_LEN)) {
		qdf_mem_copy(ht_cap_ie, &ie[2], ie[1]);
		ret = dot11f_unpack_ie_ht_caps(MAC_CONTEXT(mac_handle),
					       ht_cap_ie, ie[1],
					       &dot11_ht_cap_ie, false);
		if (DOT11F_FAILED(ret)) {
			hdd_err("unpack failed, ret: 0x%x", ret);
			return false;
		}
		return dot11_ht_cap_ie.supportedChannelWidthSet;
	}

	return false;
}
#else
static bool wlan_hdd_get_sap_obss(struct hdd_adapter *adapter)
{
	return false;
}
#endif
/**
 * wlan_hdd_set_channel() - set channel in sap mode
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 * @chandef: Pointer to channel definition structure
 * @channel_type: Channel type
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_set_channel(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_chan_def *chandef,
				enum nl80211_channel_type channel_type)
{
	struct hdd_adapter *adapter = NULL;
	uint32_t num_ch = 0;
	int channel_seg2 = 0;
	struct hdd_context *hdd_ctx;
	int status;
	mac_handle_t mac_handle;
	struct sme_config_params *sme_config;
	struct sap_config *sap_config;

	if (!dev) {
		hdd_err("Called with dev = NULL");
		return -ENODEV;
	}
	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_SET_CHANNEL,
		   adapter->vdev_id, channel_type);

	hdd_debug("Dev_mode %s(%d) freq %d, ch_bw %d ccfs1 %d ccfs2 %d",
		  qdf_opmode_str(adapter->device_mode),
		  adapter->device_mode, chandef->chan->center_freq,
		  chandef->width, chandef->center_freq1, chandef->center_freq2);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	mac_handle = hdd_ctx->mac_handle;

	/* Check freq range */
	if ((wlan_reg_min_chan_freq() >
	     chandef->chan->center_freq) ||
	    (wlan_reg_max_chan_freq() < chandef->chan->center_freq)) {
		hdd_err("channel: %d is outside valid freq range",
			chandef->chan->center_freq);
		return -EINVAL;
	}

	if (NL80211_CHAN_WIDTH_80P80 == chandef->width) {
		if ((wlan_reg_min_chan_freq() > chandef->center_freq2) ||
		    (wlan_reg_max_chan_freq() < chandef->center_freq2)) {
			hdd_err("center_freq2: %d is outside valid freq range",
				chandef->center_freq2);
			return -EINVAL;
		}

		if (chandef->center_freq2)
			channel_seg2 = ieee80211_frequency_to_channel(
				chandef->center_freq2);
		else
			hdd_err("Invalid center_freq2");
	}

	num_ch = CFG_VALID_CHANNEL_LIST_LEN;

	if (QDF_STATUS_SUCCESS !=  wlan_hdd_validate_operation_channel(
	    adapter, chandef->chan->center_freq)) {
		hdd_err("Invalid freq: %d", chandef->chan->center_freq);
		return -EINVAL;
	}

	if (adapter->device_mode == QDF_SAP_MODE ||
		   adapter->device_mode == QDF_P2P_GO_MODE) {
		sap_config = &((WLAN_HDD_GET_AP_CTX_PTR(adapter))->sap_config);
		sap_config->chan_freq = chandef->chan->center_freq;
		sap_config->ch_params.center_freq_seg1 = channel_seg2;
		sap_config->ch_params.center_freq_seg0 =
			ieee80211_frequency_to_channel(chandef->center_freq1);

		if (QDF_SAP_MODE == adapter->device_mode) {
			/* set channel to what hostapd configured */
			sap_config->chan_freq = chandef->chan->center_freq;
			sap_config->ch_params.center_freq_seg1 = channel_seg2;

			sme_config = qdf_mem_malloc(sizeof(*sme_config));
			if (!sme_config)
				return -ENOMEM;

			sme_get_config_param(mac_handle, sme_config);
			switch (channel_type) {
			case NL80211_CHAN_HT20:
			case NL80211_CHAN_NO_HT:
				sme_config->csr_config.obssEnabled = false;
				sap_config->sec_ch_freq = 0;
				break;
			case NL80211_CHAN_HT40MINUS:
				sap_config->sec_ch_freq =
					sap_config->chan_freq - 20;
				break;
			case NL80211_CHAN_HT40PLUS:
				sap_config->sec_ch_freq =
					sap_config->chan_freq + 20;
				break;
			default:
				hdd_err("Error!!! Invalid HT20/40 mode !");
				qdf_mem_free(sme_config);
				return -EINVAL;
			}
			sme_config->csr_config.obssEnabled =
				wlan_hdd_get_sap_obss(adapter);

			sme_update_config(mac_handle, sme_config);
			qdf_mem_free(sme_config);
		}
	} else {
		hdd_err("Invalid device mode failed to set valid channel");
		return -EINVAL;
	}

	return status;
}

/**
 * wlan_hdd_check_11gmode() - check for 11g mode
 * @ie: Pointer to IE
 * @require_ht: Pointer to require ht
 * @require_vht: Pointer to require vht
 * @pCheckRatesfor11g: Pointer to check rates for 11g mode
 * @pSapHw_mode: SAP HW mode
 *
 * Check for 11g rate and set proper 11g only mode
 *
 * Return: none
 */
static void wlan_hdd_check_11gmode(const u8 *ie, u8 *require_ht,
				   u8 *require_vht, u8 *pCheckRatesfor11g,
				   eCsrPhyMode *pSapHw_mode)
{
	u8 i, num_rates = ie[0];

	ie += 1;
	for (i = 0; i < num_rates; i++) {
		if (*pCheckRatesfor11g
		    && (true == wlan_hdd_rate_is_11g(ie[i] & RATE_MASK))) {
			/* If rate set have 11g rate than change the mode
			 * to 11G
			 */
			*pSapHw_mode = eCSR_DOT11_MODE_11g;
			if (ie[i] & BASIC_RATE_MASK) {
				/* If we have 11g rate as  basic rate, it
				 * means mode is 11g only mode.
				 */
				*pSapHw_mode = eCSR_DOT11_MODE_11g_ONLY;
				*pCheckRatesfor11g = false;
			}
		} else {
			if ((BASIC_RATE_MASK |
			     BSS_MEMBERSHIP_SELECTOR_HT_PHY) == ie[i])
				*require_ht = true;
			else if ((BASIC_RATE_MASK |
				  BSS_MEMBERSHIP_SELECTOR_VHT_PHY) == ie[i])
				*require_vht = true;
		}
	}
}

/**
 * wlan_hdd_check_h2e() - check SAE/H2E require flag from support rate sets
 * @rs: support rate or extended support rate set
 * @require_h2e: pointer to store require h2e flag
 *
 * Return: none
 */
static void wlan_hdd_check_h2e(const tSirMacRateSet *rs, bool *require_h2e)
{
	uint8_t i;

	if (!rs || !require_h2e)
		return;

	for (i = 0; i < rs->numRates; i++) {
		if (rs->rate[i] == (BASIC_RATE_MASK |
				    BSS_MEMBERSHIP_SELECTOR_SAE_H2E))
			*require_h2e = true;
	}
}

#ifdef WLAN_FEATURE_11AX
/**
 * wlan_hdd_add_extn_ie() - add extension IE
 * @adapter: Pointer to hostapd adapter
 * @genie: Pointer to ie to be added
 * @total_ielen: Pointer to store total ie length
 * @oui: Pointer to oui
 * @oui_size: Size of oui
 *
 * Return: 0 for success non-zero for failure
 */
static int wlan_hdd_add_extn_ie(struct hdd_adapter *adapter, uint8_t *genie,
				uint16_t *total_ielen, uint8_t *oui,
				uint8_t oui_size)
{
	const uint8_t *ie;
	uint16_t ielen = 0;
	struct hdd_beacon_data *beacon = adapter->session.ap.beacon;

	ie = wlan_get_ext_ie_ptr_from_ext_id(oui, oui_size,
					     beacon->tail,
					     beacon->tail_len);
	if (ie) {
		ielen = ie[1] + 2;
		if ((*total_ielen + ielen) <= MAX_GENIE_LEN) {
			qdf_mem_copy(&genie[*total_ielen], ie, ielen);
		} else {
			hdd_err("**Ie Length is too big***");
			return -EINVAL;
		}
		*total_ielen += ielen;
	}
	return 0;
}
#endif

/**
 * wlan_hdd_add_hostapd_conf_vsie() - configure Vendor IE in sap mode
 * @adapter: Pointer to hostapd adapter
 * @genie: Pointer to Vendor IE
 * @total_ielen: Pointer to store total ie length
 *
 * Return: none
 */
static void wlan_hdd_add_hostapd_conf_vsie(struct hdd_adapter *adapter,
					   uint8_t *genie,
					   uint16_t *total_ielen)
{
	struct hdd_beacon_data *beacon = adapter->session.ap.beacon;
	int left = beacon->tail_len;
	uint8_t *ptr = beacon->tail;
	uint8_t elem_id, elem_len;
	uint16_t ielen = 0;
	bool skip_ie;

	if (!ptr || 0 == left)
		return;

	while (left >= 2) {
		elem_id = ptr[0];
		elem_len = ptr[1];
		left -= 2;
		if (elem_len > left) {
			hdd_err("**Invalid IEs eid: %d elem_len: %d left: %d**",
				elem_id, elem_len, left);
			return;
		}
		if (WLAN_ELEMID_VENDOR == elem_id) {
			/*
			 * skipping the Vendor IE's which we don't want to
			 * include or it will be included by existing code.
			 */
			if (elem_len >= WPS_OUI_TYPE_SIZE &&
			    (!qdf_mem_cmp(&ptr[2], ALLOWLIST_OUI_TYPE,
					  WPA_OUI_TYPE_SIZE) ||
			     !qdf_mem_cmp(&ptr[2], DENYLIST_OUI_TYPE,
					  WPA_OUI_TYPE_SIZE) ||
			     !qdf_mem_cmp(&ptr[2], "\x00\x50\xf2\x02",
					  WPA_OUI_TYPE_SIZE) ||
			     !qdf_mem_cmp(&ptr[2], WPA_OUI_TYPE,
					  WPA_OUI_TYPE_SIZE)))
				skip_ie = true;
			else
				skip_ie = false;

			if (!skip_ie) {
				ielen = ptr[1] + 2;
				if ((*total_ielen + ielen) <= MAX_GENIE_LEN) {
					qdf_mem_copy(&genie[*total_ielen], ptr,
						     ielen);
					*total_ielen += ielen;
				} else {
					hdd_err("IE Length is too big IEs eid: %d elem_len: %d total_ie_lent: %d",
					       elem_id, elem_len, *total_ielen);
				}
			}
		}

		left -= elem_len;
		ptr += (elem_len + 2);
	}
}

/**
 * wlan_hdd_add_extra_ie() - add extra ies in beacon
 * @adapter: Pointer to hostapd adapter
 * @genie: Pointer to extra ie
 * @total_ielen: Pointer to store total ie length
 * @temp_ie_id: ID of extra ie
 *
 * Return: none
 */
static void wlan_hdd_add_extra_ie(struct hdd_adapter *adapter,
				  uint8_t *genie, uint16_t *total_ielen,
				  uint8_t temp_ie_id)
{
	struct hdd_beacon_data *beacon = adapter->session.ap.beacon;
	int left = beacon->tail_len;
	uint8_t *ptr = beacon->tail;
	uint8_t elem_id, elem_len;
	uint16_t ielen = 0;

	if (!ptr || 0 == left)
		return;

	while (left >= 2) {
		elem_id = ptr[0];
		elem_len = ptr[1];
		left -= 2;
		if (elem_len > left) {
			hdd_err("**Invalid IEs eid: %d elem_len: %d left: %d**",
			       elem_id, elem_len, left);
			return;
		}

		if (temp_ie_id == elem_id) {
			ielen = ptr[1] + 2;
			if ((*total_ielen + ielen) <= MAX_GENIE_LEN) {
				qdf_mem_copy(&genie[*total_ielen], ptr, ielen);
				*total_ielen += ielen;
			} else {
				hdd_err("IE Length is too big IEs eid: %d elem_len: %d total_ie_len: %d",
				       elem_id, elem_len, *total_ielen);
			}
		}

		left -= elem_len;
		ptr += (elem_len + 2);
	}
}

/**
 * wlan_hdd_cfg80211_alloc_new_beacon() - alloc beacon in ap mode
 * @adapter: Pointer to hostapd adapter
 * @out_beacon: Location to store newly allocated beacon data
 * @params: Pointer to beacon parameters
 * @dtim_period: DTIM period
 *
 * Return: 0 for success non-zero for failure
 */
static int
wlan_hdd_cfg80211_alloc_new_beacon(struct hdd_adapter *adapter,
				   struct hdd_beacon_data **out_beacon,
				   struct cfg80211_beacon_data *params,
				   int dtim_period)
{
	int size;
	struct hdd_beacon_data *beacon = NULL;
	struct hdd_beacon_data *old = NULL;
	int head_len, tail_len, proberesp_ies_len, assocresp_ies_len;
	const u8 *head, *tail, *proberesp_ies, *assocresp_ies;

	hdd_enter();
	if (params->head && !params->head_len) {
		hdd_err("head_len is NULL");
		return -EINVAL;
	}

	old = adapter->session.ap.beacon;

	if (!params->head && !old) {
		hdd_err("session: %d old and new heads points to NULL",
		       adapter->vdev_id);
		return -EINVAL;
	}

	if (params->head) {
		head_len = params->head_len;
		head = params->head;
	} else {
		head_len = old->head_len;
		head = old->head;
	}

	if (params->tail || !old) {
		tail_len = params->tail_len;
		tail = params->tail;
	} else {
		tail_len = old->tail_len;
		tail = old->tail;
	}

	if (params->proberesp_ies || !old) {
		proberesp_ies_len = params->proberesp_ies_len;
		proberesp_ies = params->proberesp_ies;
	} else {
		proberesp_ies_len = old->proberesp_ies_len;
		proberesp_ies = old->proberesp_ies;
	}

	if (params->assocresp_ies || !old) {
		assocresp_ies_len = params->assocresp_ies_len;
		assocresp_ies = params->assocresp_ies;
	} else {
		assocresp_ies_len = old->assocresp_ies_len;
		assocresp_ies = old->assocresp_ies;
	}

	size = sizeof(struct hdd_beacon_data) + head_len + tail_len +
		proberesp_ies_len + assocresp_ies_len;

	beacon = qdf_mem_malloc(size);
	if (!beacon)
		return -ENOMEM;

	if (dtim_period)
		beacon->dtim_period = dtim_period;
	else if (old)
		beacon->dtim_period = old->dtim_period;
	/* -----------------------------------------------
	 * | head | tail | proberesp_ies | assocresp_ies |
	 * -----------------------------------------------
	 */
	beacon->head = ((u8 *) beacon) + sizeof(struct hdd_beacon_data);
	beacon->tail = beacon->head + head_len;
	beacon->proberesp_ies = beacon->tail + tail_len;
	beacon->assocresp_ies = beacon->proberesp_ies + proberesp_ies_len;

	beacon->head_len = head_len;
	beacon->tail_len = tail_len;
	beacon->proberesp_ies_len = proberesp_ies_len;
	beacon->assocresp_ies_len = assocresp_ies_len;

	if (head && head_len)
		memcpy(beacon->head, head, head_len);
	if (tail && tail_len)
		memcpy(beacon->tail, tail, tail_len);
	if (proberesp_ies && proberesp_ies_len)
		memcpy(beacon->proberesp_ies, proberesp_ies, proberesp_ies_len);
	if (assocresp_ies && assocresp_ies_len)
		memcpy(beacon->assocresp_ies, assocresp_ies, assocresp_ies_len);

	*out_beacon = beacon;

	adapter->session.ap.beacon = NULL;
	qdf_mem_free(old);

	return 0;

}

#ifdef QCA_HT_2040_COEX
static void wlan_hdd_add_sap_obss_scan_ie(
	struct hdd_adapter *hostapd_adapter, uint8_t *ie_buf, uint16_t *ie_len)
{
	if (QDF_SAP_MODE == hostapd_adapter->device_mode) {
		if (wlan_hdd_get_sap_obss(hostapd_adapter))
			wlan_hdd_add_extra_ie(hostapd_adapter, ie_buf, ie_len,
					WLAN_EID_OVERLAP_BSS_SCAN_PARAM);
	}
}
#else
static void wlan_hdd_add_sap_obss_scan_ie(
	struct hdd_adapter *hostapd_adapter, uint8_t *ie_buf, uint16_t *ie_len)
{
}
#endif

/**
 * hdd_update_11ax_apies() - update ap mode 11ax ies
 * @adapter: Pointer to hostapd adapter
 * @genie: generic IE buffer
 * @total_ielen: out param to update total ielen
 *
 * Return: 0 for success non-zero for failure
 */

#ifdef WLAN_FEATURE_11AX
static int hdd_update_11ax_apies(struct hdd_adapter *adapter,
				 uint8_t *genie, uint16_t *total_ielen)
{
	if (wlan_hdd_add_extn_ie(adapter, genie, total_ielen,
				 HE_CAP_OUI_TYPE, HE_CAP_OUI_SIZE)) {
		hdd_err("Adding HE Cap ie failed");
		return -EINVAL;
	}

	if (wlan_hdd_add_extn_ie(adapter, genie, total_ielen,
				 HE_OP_OUI_TYPE, HE_OP_OUI_SIZE)) {
		hdd_err("Adding HE Op ie failed");
		return -EINVAL;
	}

	return 0;
}
#else
static int hdd_update_11ax_apies(struct hdd_adapter *adapter,
				 uint8_t *genie, uint16_t *total_ielen)
{
	return 0;
}
#endif

#ifdef WLAN_FEATURE_11BE
static int hdd_update_11be_apies(struct hdd_adapter *adapter,
				 uint8_t *genie, uint16_t *total_ielen)
{
	if (wlan_hdd_add_extn_ie(adapter, genie, total_ielen,
				 EHT_OP_OUI_TYPE, EHT_OP_OUI_SIZE)) {
		hdd_err("Adding EHT Cap IE failed");
		return -EINVAL;
	}

	if (wlan_hdd_add_extn_ie(adapter, genie, total_ielen,
				 EHT_CAP_OUI_TYPE, EHT_CAP_OUI_SIZE)) {
		hdd_err("Adding EHT Op IE failed");
		return -EINVAL;
	}

	return 0;
}
#else
static int hdd_update_11be_apies(struct hdd_adapter *adapter,
				 uint8_t *genie, uint16_t *total_ielen)
{
	return 0;
}
#endif

/**
 * wlan_hdd_cfg80211_update_apies() - update ap mode ies
 * @adapter: Pointer to hostapd adapter
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_cfg80211_update_apies(struct hdd_adapter *adapter)
{
	uint8_t *genie;
	uint16_t total_ielen = 0;
	int ret = 0;
	struct sap_config *config;
	tSirUpdateIE update_ie;
	struct hdd_beacon_data *beacon = NULL;
	uint16_t proberesp_ies_len;
	uint8_t *proberesp_ies = NULL;
	mac_handle_t mac_handle;

	config = &adapter->session.ap.sap_config;
	beacon = adapter->session.ap.beacon;
	if (!beacon) {
		hdd_err("Beacon is NULL !");
		return -EINVAL;
	}

	genie = qdf_mem_malloc(MAX_GENIE_LEN);
	if (!genie)
		return -ENOMEM;

	mac_handle = adapter->hdd_ctx->mac_handle;

	/* Extract and add the extended capabilities and interworking IE */
	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
			      WLAN_EID_EXT_CAPABILITY);

	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
			      WLAN_EID_INTERWORKING);
	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
			      WLAN_EID_ADVERTISEMENT_PROTOCOL);
	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen, WLAN_ELEMID_RSNXE);
	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
			      WLAN_ELEMID_MOBILITY_DOMAIN);
#ifdef FEATURE_WLAN_WAPI
	if (QDF_SAP_MODE == adapter->device_mode) {
		wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
				      WLAN_ELEMID_WAPI);
	}
#endif
	/* extract and add rrm ie from hostapd */
	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen, WLAN_ELEMID_RRM);

	wlan_hdd_add_hostapd_conf_vsie(adapter, genie,
				       &total_ielen);

	ret = hdd_update_11ax_apies(adapter, genie, &total_ielen);
	if (ret)
		goto done;

	ret = hdd_update_11be_apies(adapter, genie, &total_ielen);
	if (ret)
		goto done;

	wlan_hdd_add_sap_obss_scan_ie(adapter, genie, &total_ielen);

	qdf_copy_macaddr(&update_ie.bssid, &adapter->mac_addr);
	update_ie.vdev_id = adapter->vdev_id;

	/* Added for Probe Response IE */
	proberesp_ies = qdf_mem_malloc(beacon->proberesp_ies_len +
				      MAX_GENIE_LEN);
	if (!proberesp_ies) {
		ret = -EINVAL;
		goto done;
	}
	qdf_mem_copy(proberesp_ies, beacon->proberesp_ies,
		    beacon->proberesp_ies_len);
	proberesp_ies_len = beacon->proberesp_ies_len;

	wlan_hdd_add_sap_obss_scan_ie(adapter, proberesp_ies,
				     &proberesp_ies_len);
	wlan_hdd_add_extra_ie(adapter, proberesp_ies, &proberesp_ies_len,
			      WLAN_ELEMID_RSNXE);
	wlan_hdd_add_extra_ie(adapter, proberesp_ies, &proberesp_ies_len,
			      WLAN_ELEMID_MOBILITY_DOMAIN);

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		update_ie.ieBufferlength = proberesp_ies_len;
		update_ie.pAdditionIEBuffer = proberesp_ies;
		update_ie.append = false;
		update_ie.notify = false;
		if (sme_update_add_ie(mac_handle,
				      &update_ie,
				      eUPDATE_IE_PROBE_RESP) ==
		    QDF_STATUS_E_FAILURE) {
			hdd_err("Could not pass on PROBE_RESP add Ie data");
			ret = -EINVAL;
			goto done;
		}
		wlansap_reset_sap_config_add_ie(config, eUPDATE_IE_PROBE_RESP);
	} else {
		wlansap_update_sap_config_add_ie(config,
						 proberesp_ies,
						 proberesp_ies_len,
						 eUPDATE_IE_PROBE_RESP);
	}

	/* Assoc resp Add ie Data */
	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		update_ie.ieBufferlength = beacon->assocresp_ies_len;
		update_ie.pAdditionIEBuffer = (uint8_t *) beacon->assocresp_ies;
		update_ie.append = false;
		update_ie.notify = false;
		if (sme_update_add_ie(mac_handle,
				      &update_ie,
				      eUPDATE_IE_ASSOC_RESP) ==
		    QDF_STATUS_E_FAILURE) {
			hdd_err("Could not pass on Add Ie Assoc Response data");
			ret = -EINVAL;
			goto done;
		}
		wlansap_reset_sap_config_add_ie(config, eUPDATE_IE_ASSOC_RESP);
	} else {
		wlansap_update_sap_config_add_ie(config,
						 beacon->assocresp_ies,
						 beacon->assocresp_ies_len,
						 eUPDATE_IE_ASSOC_RESP);
	}

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		update_ie.ieBufferlength = total_ielen;
		update_ie.pAdditionIEBuffer = genie;
		update_ie.append = false;
		update_ie.notify = true;
		if (sme_update_add_ie(mac_handle,
				      &update_ie,
				      eUPDATE_IE_PROBE_BCN) ==
		    QDF_STATUS_E_FAILURE) {
			hdd_err("Could not pass on Add Ie probe beacon data");
			ret = -EINVAL;
			goto done;
		}
		wlansap_reset_sap_config_add_ie(config, eUPDATE_IE_PROBE_BCN);
	} else {
		wlansap_update_sap_config_add_ie(config,
						 genie,
						 total_ielen,
						 eUPDATE_IE_PROBE_BCN);
	}

done:
	qdf_mem_free(genie);
	qdf_mem_free(proberesp_ies);
	return ret;
}

/**
 * wlan_hdd_set_sap_hwmode() - set sap hw mode
 * @adapter: Pointer to hostapd adapter
 *
 * Return: none
 */
static void wlan_hdd_set_sap_hwmode(struct hdd_adapter *adapter)
{
	struct sap_config *config = &adapter->session.ap.sap_config;
	struct hdd_beacon_data *beacon = adapter->session.ap.beacon;
	struct ieee80211_mgmt *mgmt_frame =
		(struct ieee80211_mgmt *)beacon->head;
	u8 checkRatesfor11g = true;
	u8 require_ht = false, require_vht = false;
	const u8 *ie;
	ssize_t size;

	config->SapHw_mode = eCSR_DOT11_MODE_11b;

	size = beacon->head_len - sizeof(mgmt_frame->u.beacon) -
	      (sizeof(*mgmt_frame) - sizeof(mgmt_frame->u));

	if (size <= 0) {
		hdd_err_rl("Invalid length: %zu", size);
		return;
	}

	ie = wlan_get_ie_ptr_from_eid(WLAN_EID_SUPP_RATES,
				      &mgmt_frame->u.beacon.variable[0],
				      size);
	if (ie) {
		ie += 1;
		wlan_hdd_check_11gmode(ie, &require_ht, &require_vht,
			&checkRatesfor11g, &config->SapHw_mode);
	}

	ie = wlan_get_ie_ptr_from_eid(WLAN_EID_EXT_SUPP_RATES,
				      beacon->tail, beacon->tail_len);
	if (ie) {
		ie += 1;
		wlan_hdd_check_11gmode(ie, &require_ht, &require_vht,
			&checkRatesfor11g, &config->SapHw_mode);
	}

	if (WLAN_REG_IS_5GHZ_CH_FREQ(config->chan_freq))
		config->SapHw_mode = eCSR_DOT11_MODE_11a;

	ie = wlan_get_ie_ptr_from_eid(WLAN_EID_HT_CAPABILITY,
				      beacon->tail, beacon->tail_len);
	if (ie) {
		config->SapHw_mode = eCSR_DOT11_MODE_11n;
		if (require_ht)
			config->SapHw_mode = eCSR_DOT11_MODE_11n_ONLY;
	}

	ie = wlan_get_ie_ptr_from_eid(WLAN_EID_VHT_CAPABILITY,
				      beacon->tail, beacon->tail_len);
	if (ie) {
		config->SapHw_mode = eCSR_DOT11_MODE_11ac;
		if (require_vht)
			config->SapHw_mode = eCSR_DOT11_MODE_11ac_ONLY;
	}

	wlan_hdd_check_11ax_support(beacon, config);

	wlan_hdd_check_11be_support(beacon, config);

	hdd_debug("SAP hw_mode: %d", config->SapHw_mode);
}

/**
 * wlan_hdd_config_acs() - config ACS needed parameters
 * @hdd_ctx: HDD context
 * @adapter: Adapter pointer
 *
 * This function get ACS related INI parameters and populated
 * sap config and smeConfig for ACS needed configurations.
 *
 * Return: The QDF_STATUS code associated with performing the operation.
 */
QDF_STATUS wlan_hdd_config_acs(struct hdd_context *hdd_ctx,
			       struct hdd_adapter *adapter)
{
	struct sap_config *sap_config;
	struct hdd_config *ini_config;
	mac_handle_t mac_handle;

	mac_handle = hdd_ctx->mac_handle;
	sap_config = &adapter->session.ap.sap_config;
	ini_config = hdd_ctx->config;

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	hdd_debug("HDD_ACS_SKIP_STATUS = %d", hdd_ctx->skip_acs_scan_status);
	if (hdd_ctx->skip_acs_scan_status == eSAP_SKIP_ACS_SCAN) {
		struct hdd_adapter *con_sap_adapter;
		struct sap_config *con_sap_config = NULL;

		con_sap_adapter = hdd_get_con_sap_adapter(adapter, false);

		if (con_sap_adapter)
			con_sap_config =
				&con_sap_adapter->session.ap.sap_config;

		sap_config->acs_cfg.skip_scan_status = eSAP_DO_NEW_ACS_SCAN;

		if (con_sap_config &&
			con_sap_config->acs_cfg.acs_mode == true &&
			hdd_ctx->skip_acs_scan_status == eSAP_SKIP_ACS_SCAN &&
			con_sap_config->acs_cfg.hw_mode ==
						sap_config->acs_cfg.hw_mode) {
			uint32_t con_sap_st_ch_freq, con_sap_end_ch_freq;
			uint32_t cur_sap_st_ch_freq, cur_sap_end_ch_freq;
			uint32_t bandStartChannel, bandEndChannel;

			con_sap_st_ch_freq =
					con_sap_config->acs_cfg.start_ch_freq;
			con_sap_end_ch_freq =
					con_sap_config->acs_cfg.end_ch_freq;
			cur_sap_st_ch_freq =
					sap_config->acs_cfg.start_ch_freq;
			cur_sap_end_ch_freq =
					sap_config->acs_cfg.end_ch_freq;

			wlansap_extend_to_acs_range(
					mac_handle, &cur_sap_st_ch_freq,
					&cur_sap_end_ch_freq,
					&bandStartChannel, &bandEndChannel);

			wlansap_extend_to_acs_range(
					mac_handle, &con_sap_st_ch_freq,
					&con_sap_end_ch_freq,
					&bandStartChannel, &bandEndChannel);

			if (con_sap_st_ch_freq <= cur_sap_st_ch_freq &&
			    con_sap_end_ch_freq >= cur_sap_end_ch_freq) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_SKIP_ACS_SCAN;

			} else if (con_sap_st_ch_freq >= cur_sap_st_ch_freq &&
				   con_sap_end_ch_freq >=
						cur_sap_end_ch_freq) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_PAR_ACS_SCAN;

				sap_config->acs_cfg.skip_scan_range1_stch =
							cur_sap_st_ch_freq;
				sap_config->acs_cfg.skip_scan_range1_endch =
							con_sap_st_ch_freq - 5;
				sap_config->acs_cfg.skip_scan_range2_stch =
							0;
				sap_config->acs_cfg.skip_scan_range2_endch =
							0;

			} else if (con_sap_st_ch_freq <= cur_sap_st_ch_freq &&
				   con_sap_end_ch_freq <=
						cur_sap_end_ch_freq) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_PAR_ACS_SCAN;

				sap_config->acs_cfg.skip_scan_range1_stch =
							con_sap_end_ch_freq + 5;
				sap_config->acs_cfg.skip_scan_range1_endch =
							cur_sap_end_ch_freq;
				sap_config->acs_cfg.skip_scan_range2_stch =
							0;
				sap_config->acs_cfg.skip_scan_range2_endch =
							0;

			} else if (con_sap_st_ch_freq >= cur_sap_st_ch_freq &&
				   con_sap_end_ch_freq <=
						cur_sap_end_ch_freq) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_PAR_ACS_SCAN;

				sap_config->acs_cfg.skip_scan_range1_stch =
							cur_sap_st_ch_freq;
				sap_config->acs_cfg.skip_scan_range1_endch =
							con_sap_st_ch_freq - 5;
				sap_config->acs_cfg.skip_scan_range2_stch =
							con_sap_end_ch_freq;
				sap_config->acs_cfg.skip_scan_range2_endch =
						cur_sap_end_ch_freq + 5;

			} else
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_NEW_ACS_SCAN;


			hdd_debug("SecAP ACS Skip=%d, ACS CH RANGE=%d-%d, %d-%d",
				  sap_config->acs_cfg.skip_scan_status,
				  sap_config->acs_cfg.skip_scan_range1_stch,
				  sap_config->acs_cfg.skip_scan_range1_endch,
				  sap_config->acs_cfg.skip_scan_range2_stch,
				  sap_config->acs_cfg.skip_scan_range2_endch);
		}
	}
#endif

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_hdd_sap_p2p_11ac_overrides: API to overwrite 11ac config in case of
 * SAP or p2p go
 * @ap_adapter: pointer to adapter
 *
 * This function overrides SAP / P2P Go configuration based on driver INI
 * parameters for 11AC override and ACS. This overrides are done to support
 * android legacy configuration method.
 *
 * NOTE: Non android platform supports concurrency and these overrides shall
 * not be used. Also future driver based overrides shall be consolidated in this
 * function only. Avoid random overrides in other location based on ini.
 *
 * Return: 0 for Success or Negative error codes.
 */
static int wlan_hdd_sap_p2p_11ac_overrides(struct hdd_adapter *ap_adapter)
{
	struct sap_config *sap_cfg = &ap_adapter->session.ap.sap_config;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	uint8_t ch_width;
	uint8_t sub_20_chan_width;
	QDF_STATUS status;
	bool sap_force_11n_for_11ac = 0;
	bool go_force_11n_for_11ac = 0;
	uint32_t channel_bonding_mode;
	bool go_11ac_override = 0;
	bool sap_11ac_override = 0;

	/*
	 * No need to override for Go/Sap on 6 GHz band
	 */
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_cfg->chan_freq))
		return 0;

	ucfg_mlme_get_sap_force_11n_for_11ac(hdd_ctx->psoc,
					     &sap_force_11n_for_11ac);
	ucfg_mlme_get_go_force_11n_for_11ac(hdd_ctx->psoc,
					    &go_force_11n_for_11ac);

	/* Fixed channel 11AC override:
	 * 11AC override in qcacld is introduced for following reasons:
	 * 1. P2P GO also follows start_bss and since p2p GO could not be
	 *    configured to setup VHT channel width in wpa_supplicant
	 * 2. Android UI does not provide advanced configuration options for SAP
	 *
	 * Default override enabled (for android). MDM shall disable this in ini
	 */
	/*
	 * sub_20 MHz channel width is incompatible with 11AC rates, hence do
	 * not allow 11AC rates or more than 20 MHz channel width when
	 * enable_sub_20_channel_width is non zero
	 */
	status = ucfg_mlme_get_sub_20_chan_width(hdd_ctx->psoc,
						 &sub_20_chan_width);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get sub_20_chan_width config");
		return -EIO;
	}

	ucfg_mlme_is_go_11ac_override(hdd_ctx->psoc,
				      &go_11ac_override);
	ucfg_mlme_is_sap_11ac_override(hdd_ctx->psoc,
				       &sap_11ac_override);

	if (!sub_20_chan_width &&
	    (sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11n ||
	    sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11ac ||
	    sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11ac_ONLY ||
	    sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11ax ||
	    sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11ax_ONLY) &&
	    ((ap_adapter->device_mode == QDF_SAP_MODE &&
	    !sap_force_11n_for_11ac &&
	    sap_11ac_override) ||
	    (ap_adapter->device_mode == QDF_P2P_GO_MODE &&
	    !go_force_11n_for_11ac &&
	    go_11ac_override))) {
		hdd_debug("** Driver force 11AC override for SAP/Go **");

		/* 11n only shall not be overridden since it may be on purpose*/
		if (sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11n)
			sap_cfg->SapHw_mode = eCSR_DOT11_MODE_11ac;

		if (!WLAN_REG_IS_24GHZ_CH_FREQ(sap_cfg->chan_freq)) {
			status =
			    ucfg_mlme_get_vht_channel_width(hdd_ctx->psoc,
							    &ch_width);
			if (!QDF_IS_STATUS_SUCCESS(status))
				hdd_err("Failed to set channel_width");
			sap_cfg->ch_width_orig = ch_width;
		} else {
			/*
			 * Allow 40 Mhz in 2.4 Ghz only if indicated by
			 * supplicant after OBSS scan and if 2.4 Ghz channel
			 * bonding is set in INI
			 */
			ucfg_mlme_get_channel_bonding_24ghz(
				hdd_ctx->psoc, &channel_bonding_mode);
			if (sap_cfg->ch_width_orig >= CH_WIDTH_40MHZ &&
			    channel_bonding_mode)
				sap_cfg->ch_width_orig =
					CH_WIDTH_40MHZ;
			else
				sap_cfg->ch_width_orig =
					CH_WIDTH_20MHZ;
		}
	}

	return 0;
}

/**
 * wlan_hdd_setup_driver_overrides : Overrides SAP / P2P GO Params
 * @ap_adapter: pointer to adapter struct
 *
 * This function overrides SAP / P2P Go configuration based on driver INI
 * parameters for 11AC override and ACS. These overrides are done to support
 * android legacy configuration method.
 *
 * NOTE: Non android platform supports concurrency and these overrides shall
 * not be used. Also future driver based overrides shall be consolidated in this
 * function only. Avoid random overrides in other location based on ini.
 *
 * Return: 0 for Success or Negative error codes.
 */
static int wlan_hdd_setup_driver_overrides(struct hdd_adapter *ap_adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);
	QDF_STATUS qdf_status;
	bool is_vendor_acs_support =
		cfg_default(CFG_USER_AUTO_CHANNEL_SELECTION);

	qdf_status = ucfg_mlme_get_vendor_acs_support(hdd_ctx->psoc,
						      &is_vendor_acs_support);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		hdd_err("get_vendor_acs_support failed, set default");

	if (!is_vendor_acs_support)
		return wlan_hdd_sap_p2p_11ac_overrides(ap_adapter);
	else
		return 0;
}

void
hdd_check_and_disconnect_sta_on_invalid_channel(struct hdd_context *hdd_ctx,
						enum wlan_reason_code reason)
{
	struct hdd_adapter *sta_adapter;
	uint32_t sta_chan_freq;

	sta_chan_freq = hdd_get_operating_chan_freq(hdd_ctx, QDF_STA_MODE);
	if (!sta_chan_freq) {
		hdd_err("STA not connected");
		return;
	}

	hdd_err("STA connected on %d", sta_chan_freq);

	if (sme_is_channel_valid(hdd_ctx->mac_handle, sta_chan_freq)) {
		hdd_err("STA connected on %d and it is valid", sta_chan_freq);
		return;
	}

	sta_adapter = hdd_get_adapter(hdd_ctx, QDF_STA_MODE);

	if (!sta_adapter) {
		hdd_err("STA adapter does not exist");
		return;
	}

	hdd_err("chan %d not valid, issue disconnect", sta_chan_freq);
	wlan_hdd_cm_issue_disconnect(sta_adapter, reason, false);
}

/**
 * hdd_handle_acs_2g_preferred_sap_conc() - Handle 2G pereferred SAP
 * concurrency with GO
 * @psoc: soc object
 * @adapter: SAP adapter
 * @sap_config: sap config
 *
 * In SAP+GO concurrency, if GO is started on 2G and SAP is doing ACS
 * with 2G preferred channel list, then we will move GO to 5G band.
 * The purpose is to have more selection for SAP instead of starting on
 * GO home channel for SCC.
 *
 * Return: void
 */
void
hdd_handle_acs_2g_preferred_sap_conc(struct wlan_objmgr_psoc *psoc,
				     struct hdd_adapter *adapter,
				     struct sap_config *sap_config)
{
	uint32_t i;
	int ret;
	QDF_STATUS status;
	uint32_t *ch_freq_list;
	struct sap_acs_cfg *acs_cfg;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_num;
	uint8_t sta_vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t sta_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t sta_vdev_num;
	struct hdd_hostapd_state *hostapd_state;
	uint8_t go_vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	qdf_freq_t go_ch_freq = 0, go_new_ch_freq;
	struct hdd_adapter *go_adapter;

	if (adapter->device_mode != QDF_SAP_MODE)
		return;
	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return;

	acs_cfg = &sap_config->acs_cfg;
	if (!acs_cfg->master_ch_list_count)
		return;
	ch_freq_list = acs_cfg->master_freq_list;
	for (i = 0; i < acs_cfg->master_ch_list_count; i++)
		if (!WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq_list[i]))
			return;

	/* Move existing GO interface to 5G band */
	vdev_num = policy_mgr_get_mode_specific_conn_info(
			psoc, freq_list, vdev_id_list,
			PM_P2P_GO_MODE);
	if (!vdev_num || vdev_num > 1)
		return;
	for (i = 0; i < vdev_num; i++) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(freq_list[i])) {
			go_vdev_id = vdev_id_list[i];
			go_ch_freq = freq_list[i];
			break;
		}
	}
	if (!go_ch_freq)
		return;
	sta_vdev_num = policy_mgr_get_mode_specific_conn_info(
			psoc, sta_freq_list, sta_vdev_id_list,
			PM_STA_MODE);
	for (i = 0; i < sta_vdev_num; i++)
		if (go_ch_freq == sta_freq_list[i])
			return;

	go_new_ch_freq =
		policy_mgr_get_alternate_channel_for_sap(psoc,
							 go_vdev_id,
							 go_ch_freq,
							 REG_BAND_UNKNOWN);
	if (!go_new_ch_freq || WLAN_REG_IS_24GHZ_CH_FREQ(go_new_ch_freq)) {
		hdd_err("no available 5G channel");
		return;
	}
	go_adapter = hdd_get_adapter_by_vdev(adapter->hdd_ctx, go_vdev_id);
	if (hdd_validate_adapter(go_adapter))
		return;

	wlan_hdd_set_sap_csa_reason(psoc, go_vdev_id,
				    CSA_REASON_SAP_ACS);
	hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(go_adapter);
	qdf_event_reset(&hostapd_state->qdf_event);

	ret = hdd_softap_set_channel_change(go_adapter->dev, go_new_ch_freq,
					    CH_WIDTH_80MHZ, false);
	if (ret) {
		hdd_err("CSA failed to %d, ret %d", go_new_ch_freq, ret);
		return;
	}

	status = qdf_wait_for_event_completion(&hostapd_state->qdf_event,
					       SME_CMD_START_BSS_TIMEOUT);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("wait for qdf_event failed!!");
}

/**
 * hdd_handle_p2p_go_for_3rd_ap_conc() - Move P2P GO to other band if
 * SAP starting
 * @psoc: soc object
 * @adapter: sap adapter
 * @sap_ch_freq: sap channel frequency
 *
 * In GO+STA+SAP concurrency, if GO is MCC with STA, the new SAP
 * will not be allowed in same MAC. FW doesn't support MCC in same
 * MAC for 3 or more vdevs. Move GO to other band to avoid SAP
 * starting failed.
 *
 * Return: true if GO is moved to other band successfully.
 */
static bool
hdd_handle_p2p_go_for_3rd_ap_conc(struct wlan_objmgr_psoc *psoc,
				  struct hdd_adapter *adapter,
				  uint32_t sap_ch_freq)
{
	uint32_t i;
	int ret;
	QDF_STATUS status;
	uint8_t go_vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t go_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t go_vdev_num;
	uint8_t sta_vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t sta_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t sta_vdev_num;
	struct hdd_hostapd_state *hostapd_state;
	uint8_t go_vdev_id;
	qdf_freq_t go_ch_freq, go_new_ch_freq;
	struct hdd_adapter *go_adapter;

	if (adapter->device_mode != QDF_SAP_MODE)
		return false;
	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return false;
	go_vdev_num = policy_mgr_get_mode_specific_conn_info(
			psoc, go_freq_list, go_vdev_id_list,
			PM_P2P_GO_MODE);
	if (!go_vdev_num || go_vdev_num > 1)
		return false;

	if (!policy_mgr_2_freq_always_on_same_mac(
			psoc, go_freq_list[0], sap_ch_freq))
		return false;

	sta_vdev_num = policy_mgr_get_mode_specific_conn_info(
			psoc, sta_freq_list, sta_vdev_id_list,
			PM_STA_MODE);
	if (!sta_vdev_num)
		return false;
	for (i = 0; i < sta_vdev_num; i++) {
		if (go_freq_list[0] != sta_freq_list[i] &&
		    policy_mgr_2_freq_always_on_same_mac(
				psoc, go_freq_list[0], sta_freq_list[i]))
			break;
	}
	if (i == sta_vdev_num)
		return false;
	go_ch_freq = go_freq_list[0];
	go_vdev_id = go_vdev_id_list[0];
	go_new_ch_freq =
		policy_mgr_get_alternate_channel_for_sap(psoc,
							 go_vdev_id,
							 go_ch_freq,
							 REG_BAND_UNKNOWN);
	if (!go_new_ch_freq) {
		hdd_debug("no alternate GO channel");
		return false;
	}
	if (go_new_ch_freq != sta_freq_list[i] &&
	    policy_mgr_3_freq_always_on_same_mac(
			psoc, sap_ch_freq, go_new_ch_freq, sta_freq_list[i])) {
		hdd_debug("no alternate GO channel to avoid 3vif MCC");
		return false;
	}

	go_adapter = hdd_get_adapter_by_vdev(adapter->hdd_ctx, go_vdev_id);
	if (hdd_validate_adapter(go_adapter))
		return false;

	wlan_hdd_set_sap_csa_reason(psoc, go_vdev_id,
				    CSA_REASON_SAP_FIX_CH_CONC_WITH_GO);
	hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(go_adapter);
	qdf_event_reset(&hostapd_state->qdf_event);

	ret = hdd_softap_set_channel_change(go_adapter->dev, go_new_ch_freq,
					    CH_WIDTH_80MHZ, false);
	if (ret) {
		hdd_err("CSA failed to %d, ret %d", go_new_ch_freq, ret);
		return false;
	}

	status = qdf_wait_for_event_completion(&hostapd_state->qdf_event,
					       SME_CMD_START_BSS_TIMEOUT);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("wait for qdf_event failed!!");

	return true;
}

#ifdef DISABLE_CHANNEL_LIST
/**
 * wlan_hdd_get_wiphy_channel() - Get wiphy channel
 * @wiphy: Pointer to wiphy structure
 * @freq: Frequency of the channel for which the wiphy hw value is required
 *
 * Return: wiphy channel for valid frequency else return NULL
 */
static struct ieee80211_channel *wlan_hdd_get_wiphy_channel(
						struct wiphy *wiphy,
						uint32_t freq)
{
	uint32_t band_num, channel_num;
	struct ieee80211_channel *wiphy_channel = NULL;

	for (band_num = 0; band_num < HDD_NUM_NL80211_BANDS; band_num++) {
		if (!wiphy->bands[band_num]) {
			hdd_debug("Band %d not part of wiphy", band_num);
			continue;
		}
		for (channel_num = 0; channel_num <
				wiphy->bands[band_num]->n_channels;
				channel_num++) {
			wiphy_channel = &(wiphy->bands[band_num]->
							channels[channel_num]);
			if (wiphy_channel->center_freq == freq)
				return wiphy_channel;
		}
	}
	return wiphy_channel;
}

int wlan_hdd_restore_channels(struct hdd_context *hdd_ctx)
{
	struct hdd_cache_channels *cache_chann;
	struct wiphy *wiphy;
	int freq, status;
	int i;
	struct ieee80211_channel *wiphy_channel = NULL;

	hdd_enter();

	if (!hdd_ctx) {
		hdd_err("HDD Context is NULL");
		return -EINVAL;
	}

	wiphy = hdd_ctx->wiphy;
	if (!wiphy) {
		hdd_err("Wiphy is NULL");
		return -EINVAL;
	}

	qdf_mutex_acquire(&hdd_ctx->cache_channel_lock);

	cache_chann = hdd_ctx->original_channels;

	if (!cache_chann || !cache_chann->num_channels) {
		qdf_mutex_release(&hdd_ctx->cache_channel_lock);
		hdd_nofl_debug("channel list is NULL or num channels are zero");
		return -EINVAL;
	}

	for (i = 0; i < cache_chann->num_channels; i++) {
		freq = cache_chann->channel_info[i].freq;
		if (!freq)
			continue;

		wiphy_channel = wlan_hdd_get_wiphy_channel(wiphy, freq);
		if (!wiphy_channel)
			continue;
		/*
		 * Restore the original states of the channels
		 * only if we have cached non zero values
		 */
		wiphy_channel->flags =
				cache_chann->channel_info[i].wiphy_status;

		hdd_debug("Restore channel_freq %d reg_stat %d wiphy_stat 0x%x",
			  cache_chann->channel_info[i].freq,
			  cache_chann->channel_info[i].reg_status,
			  wiphy_channel->flags);
	}

	qdf_mutex_release(&hdd_ctx->cache_channel_lock);

	ucfg_reg_restore_cached_channels(hdd_ctx->pdev);
	status = sme_update_channel_list(hdd_ctx->mac_handle);
	if (status)
		hdd_err("Can't Restore channel list");
	else
		/*
		 * Free the cache channels when the
		 * disabled channels are restored
		 */
		wlan_hdd_free_cache_channels(hdd_ctx);
	hdd_exit();
	return 0;
}

int wlan_hdd_disable_channels(struct hdd_context *hdd_ctx)
{
	struct hdd_cache_channels *cache_chann;
	struct wiphy *wiphy;
	int freq, status;
	int i;
	struct ieee80211_channel *wiphy_channel = NULL;

	hdd_enter();

	if (!hdd_ctx) {
		hdd_err("HDD Context is NULL");
		return -EINVAL;
	}

	wiphy = hdd_ctx->wiphy;
	if (!wiphy) {
		hdd_err("Wiphy is NULL");
		return -EINVAL;
	}

	qdf_mutex_acquire(&hdd_ctx->cache_channel_lock);
	cache_chann = hdd_ctx->original_channels;

	if (!cache_chann || !cache_chann->num_channels) {
		qdf_mutex_release(&hdd_ctx->cache_channel_lock);
		hdd_err("channel list is NULL or num channels are zero");
		return -EINVAL;
	}

	for (i = 0; i < cache_chann->num_channels; i++) {
		freq = cache_chann->channel_info[i].freq;
		if (!freq)
			continue;
		wiphy_channel = wlan_hdd_get_wiphy_channel(wiphy, freq);
		if (!wiphy_channel)
			continue;
		/*
		 * Cache the current states of
		 * the channels
		 */
		cache_chann->channel_info[i].reg_status =
			wlan_reg_get_channel_state_from_secondary_list_for_freq(
							hdd_ctx->pdev,
							freq);
		cache_chann->channel_info[i].wiphy_status =
							wiphy_channel->flags;
		hdd_debug("Disable channel_freq %d reg_stat %d wiphy_stat 0x%x",
			  cache_chann->channel_info[i].freq,
			  cache_chann->channel_info[i].reg_status,
			  wiphy_channel->flags);

		wiphy_channel->flags |= IEEE80211_CHAN_DISABLED;
	}

	qdf_mutex_release(&hdd_ctx->cache_channel_lock);
	 ucfg_reg_disable_cached_channels(hdd_ctx->pdev);
	status = sme_update_channel_list(hdd_ctx->mac_handle);

	hdd_exit();
	return status;
}
#else
int wlan_hdd_disable_channels(struct hdd_context *hdd_ctx)
{
	return 0;
}

int wlan_hdd_restore_channels(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

#ifdef DHCP_SERVER_OFFLOAD
static void wlan_hdd_set_dhcp_server_offload(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct dhcp_offload_info_params dhcp_srv_info;
	uint8_t num_entries = 0;
	uint8_t *srv_ip;
	uint8_t num;
	uint32_t temp;
	uint32_t dhcp_max_num_clients;
	mac_handle_t mac_handle;
	QDF_STATUS status;

	if (!hdd_ctx->config->dhcp_server_ip.is_dhcp_server_ip_valid)
		return;

	srv_ip = hdd_ctx->config->dhcp_server_ip.dhcp_server_ip;
	dhcp_srv_info.vdev_id = adapter->vdev_id;
	dhcp_srv_info.dhcp_offload_enabled = true;

	status = ucfg_fwol_get_dhcp_max_num_clients(hdd_ctx->psoc,
						    &dhcp_max_num_clients);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	dhcp_srv_info.dhcp_client_num = dhcp_max_num_clients;

	if ((srv_ip[0] >= 224) && (srv_ip[0] <= 239)) {
		hdd_err("Invalid IP address (%d)! It could NOT be multicast IP address!",
			srv_ip[0]);
		return;
	}

	if (srv_ip[IPADDR_NUM_ENTRIES - 1] >= 100) {
		hdd_err("Invalid IP address (%d)! The last field must be less than 100!",
			srv_ip[IPADDR_NUM_ENTRIES - 1]);
		return;
	}

	dhcp_srv_info.dhcp_srv_addr = 0;
	for (num = 0; num < num_entries; num++) {
		temp = srv_ip[num];
		dhcp_srv_info.dhcp_srv_addr |= (temp << (8 * num));
	}

	mac_handle = hdd_ctx->mac_handle;
	status = sme_set_dhcp_srv_offload(mac_handle, &dhcp_srv_info);
	if (QDF_IS_STATUS_SUCCESS(status))
		hdd_debug("enable DHCP Server offload successfully!");
	else
		hdd_err("sme_set_dhcp_srv_offload fail!");
}

/**
 * wlan_hdd_dhcp_offload_enable: Enable DHCP offload
 * @hdd_ctx: HDD context handler
 * @adapter: Adapter pointer
 *
 * Enables the DHCP Offload feature in firmware if it has been configured.
 *
 * Return: None
 */
static void wlan_hdd_dhcp_offload_enable(struct hdd_context *hdd_ctx,
					 struct hdd_adapter *adapter)
{
	bool enable_dhcp_server_offload;
	QDF_STATUS status;

	status = ucfg_fwol_get_enable_dhcp_server_offload(
						hdd_ctx->psoc,
						&enable_dhcp_server_offload);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	if (enable_dhcp_server_offload)
		wlan_hdd_set_dhcp_server_offload(adapter);
}
#else
static void wlan_hdd_dhcp_offload_enable(struct hdd_context *hdd_ctx,
					 struct hdd_adapter *adapter)
{
}
#endif /* DHCP_SERVER_OFFLOAD */

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
static void wlan_hdd_set_sap_mcc_chnl_avoid(struct hdd_context *hdd_ctx)
{
	uint8_t sap_mcc_avoid = 0;
	QDF_STATUS status;

	status = ucfg_mlme_get_sap_mcc_chnl_avoid(hdd_ctx->psoc,
						  &sap_mcc_avoid);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("can't get sap mcc chnl avoid, use def");
	wlan_sap_set_channel_avoidance(hdd_ctx->mac_handle, sap_mcc_avoid);
}
#else
static void wlan_hdd_set_sap_mcc_chnl_avoid(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_hdd_mlo_update() - handle mlo scenario for start bss
 * @hdd_ctx: Pointer to hdd context
 * @config: Pointer to sap config
 * @adapter: Pointer to hostapd adapter
 * @beacon: Pointer to beacon
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlan_hdd_mlo_update(struct hdd_context *hdd_ctx,
				      struct sap_config *config,
				      struct hdd_adapter *adapter,
				      struct hdd_beacon_data *beacon)
{
	uint8_t link_id = 0;
	uint8_t num_link = 0;

	if (config->SapHw_mode == eCSR_DOT11_MODE_11be ||
	    config->SapHw_mode == eCSR_DOT11_MODE_11be_ONLY) {
		wlan_hdd_get_mlo_link_id(beacon, &link_id, &num_link);
		hdd_debug("MLO SAP vdev id %d, link id %d total link %d",
			  adapter->vdev_id, link_id, num_link);
		if (!num_link) {
			hdd_debug("start 11be AP without mlo");
			return QDF_STATUS_SUCCESS;
		}
		if (!mlo_ap_vdev_attach(adapter->vdev, link_id, num_link)) {
			hdd_err("MLO SAP attach fails");
			return QDF_STATUS_E_INVAL;
		}

		config->mlo_sap = true;
		config->link_id = link_id;
		config->num_link = num_link;
	}

	if (!policy_mgr_is_mlo_sap_concurrency_allowed(
		hdd_ctx->psoc, wlan_vdev_mlme_is_mlo_ap(adapter->vdev))) {
		hdd_err("MLO SAP concurrency check fails");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

void wlan_hdd_mlo_reset(struct hdd_adapter *adapter)
{
	if (wlan_vdev_mlme_is_mlo_ap(adapter->vdev)) {
		adapter->session.ap.sap_config.mlo_sap = false;
		adapter->session.ap.sap_config.link_id = 0;
		adapter->session.ap.sap_config.num_link = 0;
		mlo_ap_vdev_detach(adapter->vdev);
	}
}
#else
static QDF_STATUS wlan_hdd_mlo_update(struct hdd_context *hdd_ctx,
				      struct sap_config *config,
				      struct hdd_adapter *adapter,
				      struct hdd_beacon_data *beacon)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static void
hdd_softap_update_pasn_vdev_params(struct hdd_context *hdd_ctx,
				   uint8_t vdev_id,
				   struct hdd_beacon_data *beacon,
				   bool mfp_capable, bool mfp_required)
{
	uint32_t pasn_vdev_param = 0;
	const uint8_t *rsnx_ie, *rsnxe_cap;
	uint8_t cap_len;

	if (mfp_capable)
		pasn_vdev_param |= WLAN_CRYPTO_MFPC;

	if (mfp_required)
		pasn_vdev_param |= WLAN_CRYPTO_MFPR;

	rsnx_ie = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSNXE,
					   beacon->tail, beacon->tail_len);
	if (!rsnx_ie)
		return;

	rsnxe_cap = wlan_crypto_parse_rsnxe_ie(rsnx_ie, &cap_len);
	if (rsnxe_cap && *rsnxe_cap & WLAN_CRYPTO_RSNX_CAP_URNM_MFPR)
		pasn_vdev_param |= WLAN_CRYPTO_URNM_MFPR;

	wlan_crypto_vdev_set_param(hdd_ctx->psoc, vdev_id,
				   wmi_vdev_param_11az_security_config,
				   pasn_vdev_param);
}

#ifdef QCA_MULTIPASS_SUPPORT
static void
wlan_hdd_set_multipass(struct wlan_objmgr_vdev *vdev)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	cdp_config_param_type vdev_param;
	uint8_t vdev_id;

	vdev_id = wlan_vdev_get_id(vdev);
	vdev_param.cdp_vdev_param_update_multipass = true;
	cdp_txrx_set_vdev_param(soc, vdev_id, CDP_UPDATE_MULTIPASS, vdev_param);
}
#else
static void
wlan_hdd_set_multipass(struct wlan_objmgr_vdev *vdev)
{
}
#endif

/**
 * wlan_hdd_cfg80211_start_bss() - start bss
 * @adapter: Pointer to hostapd adapter
 * @params: Pointer to start bss beacon parameters
 * @ssid: Pointer ssid
 * @ssid_len: Length of ssid
 * @hidden_ssid: Hidden SSID parameter
 * @check_for_concurrency: Flag to indicate if check for concurrency is needed
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_cfg80211_start_bss(struct hdd_adapter *adapter,
				       struct cfg80211_beacon_data *params,
				       const u8 *ssid, size_t ssid_len,
				       enum nl80211_hidden_ssid hidden_ssid,
				       bool check_for_concurrency)
{
	struct sap_config *config;
	struct hdd_beacon_data *beacon = NULL;
	struct ieee80211_mgmt *mgmt_frame;
	struct ieee80211_mgmt mgmt;
	const uint8_t *ie = NULL;
	eCsrEncryptionType rsn_encrypt_type;
	eCsrEncryptionType mc_rsn_encrypt_type;
	uint16_t capab_info;
	int status = QDF_STATUS_SUCCESS, ret;
	int qdf_status = QDF_STATUS_SUCCESS;
	sap_event_cb sap_event_callback;
	struct hdd_hostapd_state *hostapd_state;
	mac_handle_t mac_handle;
	int32_t i;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	uint8_t mcc_to_scc_switch = 0, conc_rule1 = 0;
	struct sme_config_params *sme_config = NULL;
	bool mfp_capable = false;
	bool mfp_required = false;
	uint16_t prev_rsn_length = 0;
	enum dfs_mode mode;
	bool ignore_cac = 0;
	uint8_t beacon_fixed_len, indoor_chnl_marking = 0;
	bool sap_force_11n_for_11ac = 0;
	bool go_force_11n_for_11ac = 0;
	bool bval = false;
	bool enable_dfs_scan = true;
	bool deliver_start_evt = true;
	struct s_ext_cap p_ext_cap = {0};
	enum reg_phymode reg_phy_mode, updated_phy_mode;
	struct sap_context *sap_ctx;
	struct wlan_objmgr_vdev *vdev;
	uint32_t user_config_freq = 0;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	if (policy_mgr_is_sta_mon_concurrency(hdd_ctx->psoc))
		return -EINVAL;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_HDD_ID_OBJ_MGR);
	if (!vdev)
		return -EINVAL;

	ucfg_mlme_get_sap_force_11n_for_11ac(hdd_ctx->psoc,
					     &sap_force_11n_for_11ac);
	ucfg_mlme_get_go_force_11n_for_11ac(hdd_ctx->psoc,
					    &go_force_11n_for_11ac);

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags))
		deliver_start_evt = false;

	if (deliver_start_evt) {
		status = ucfg_if_mgr_deliver_event(
					vdev, WLAN_IF_MGR_EV_AP_START_BSS,
					NULL);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("start bss failed!!");
			ret = -EINVAL;
			goto deliver_start_err;
		}
	}

	mac_handle = hdd_ctx->mac_handle;

	sme_config = qdf_mem_malloc(sizeof(*sme_config));
	if (!sme_config) {
		ret = -ENOMEM;
		goto free;
	}

	hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

	config = &adapter->session.ap.sap_config;
	if (!config->chan_freq) {
		hdd_err("Invalid channel");
		ret = -EINVAL;
		goto free;
	}

	user_config_freq = config->chan_freq;

	if (QDF_STATUS_SUCCESS !=
	    ucfg_policy_mgr_get_indoor_chnl_marking(hdd_ctx->psoc,
						    &indoor_chnl_marking))
		hdd_err("can't get indoor channel marking, using default");
	/* Mark the indoor channel (passive) to disable */
	if (indoor_chnl_marking && adapter->device_mode == QDF_SAP_MODE) {
		hdd_update_indoor_channel(hdd_ctx, true);
		if (QDF_IS_STATUS_ERROR(
		    sme_update_channel_list(mac_handle))) {
			hdd_update_indoor_channel(hdd_ctx, false);
			hdd_err("Can't start BSS: update channel list failed");
			ret = -EINVAL;
			goto free;
		}

		/* check if STA is on indoor channel*/
		if (policy_mgr_is_force_scc(hdd_ctx->psoc))
			hdd_check_and_disconnect_sta_on_invalid_channel(
					hdd_ctx,
					REASON_OPER_CHANNEL_DISABLED_INDOOR);
	}

	beacon = adapter->session.ap.beacon;

	/*
	 * beacon_fixed_len is the fixed length of beacon
	 * frame which includes only mac header length and
	 * beacon manadatory fields like timestamp,
	 * beacon_int and capab_info.
	 * (From the reference of struct ieee80211_mgmt)
	 */
	beacon_fixed_len = sizeof(mgmt) - sizeof(mgmt.u) +
			   sizeof(mgmt.u.beacon);
	if (beacon->head_len < beacon_fixed_len) {
		hdd_err("Invalid beacon head len");
		ret = -EINVAL;
		goto error;
	}

	mgmt_frame = (struct ieee80211_mgmt *)beacon->head;

	config->beacon_int = mgmt_frame->u.beacon.beacon_int;
	config->dfs_cac_offload = hdd_ctx->dfs_cac_offload;
	config->dtim_period = beacon->dtim_period;

	if (config->acs_cfg.acs_mode == true) {
		hdd_debug("acs_chan_freq %u, acs_dfs_mode %u",
			  hdd_ctx->acs_policy.acs_chan_freq,
			  hdd_ctx->acs_policy.acs_dfs_mode);

		if (hdd_ctx->acs_policy.acs_chan_freq)
			config->chan_freq = hdd_ctx->acs_policy.acs_chan_freq;
		mode = hdd_ctx->acs_policy.acs_dfs_mode;
		config->acs_dfs_mode = wlan_hdd_get_dfs_mode(mode);
	}
	ucfg_util_vdev_mgr_set_acs_mode_for_vdev(vdev,
						 config->acs_cfg.acs_mode);

	ucfg_policy_mgr_get_mcc_scc_switch(hdd_ctx->psoc, &mcc_to_scc_switch);

	if (adapter->device_mode == QDF_SAP_MODE ||
	    adapter->device_mode == QDF_P2P_GO_MODE) {
		ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_EXTCAP,
						   beacon->tail,
						   beacon->tail_len);
		if (ie && (ie[0] != DOT11F_EID_EXTCAP ||
		    ie[1] > DOT11F_IE_EXTCAP_MAX_LEN)) {
			hdd_err("Invalid IEs eid: %d elem_len: %d", ie[0],
				ie[1]);
			ret = -EINVAL;
			goto error;
		}
		if (ie) {
			bool target_bigtk_support = false;

			memcpy(&p_ext_cap, &ie[2], (ie[1] > sizeof(p_ext_cap)) ?
			       sizeof(p_ext_cap) : ie[1]);

			hdd_debug("beacon protection %d",
				  p_ext_cap.beacon_protection_enable);

			ucfg_mlme_get_bigtk_support(hdd_ctx->psoc,
						    &target_bigtk_support);
			if (target_bigtk_support &&
			    p_ext_cap.beacon_protection_enable)
				mlme_set_bigtk_support(vdev, true);
		}

		/* Overwrite second AP's channel with first only when:
		 * 1. If operating mode is single mac
		 * 2. or if 2nd AP is coming up on 5G band channel
		 */
		ret = 0;
		if (!policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc) ||
		    !WLAN_REG_IS_24GHZ_CH_FREQ(config->chan_freq)) {
			ret = wlan_hdd_sap_cfg_dfs_override(adapter);
			if (ret < 0)
				goto error;
		}
		if (!ret && wlan_reg_is_dfs_for_freq(hdd_ctx->pdev,
						     config->chan_freq))
			hdd_ctx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;

		if (QDF_STATUS_SUCCESS !=
		    wlan_hdd_validate_operation_channel(adapter,
							config->chan_freq)) {
			hdd_err("Invalid Ch_freq: %d", config->chan_freq);
			ret = -EINVAL;
			goto error;
		}
		ucfg_scan_cfg_get_dfs_chan_scan_allowed(hdd_ctx->psoc,
							&enable_dfs_scan);

		/* reject SAP if DFS channel scan is not allowed */
		if (!(enable_dfs_scan) &&
		    (CHANNEL_STATE_DFS ==
		     wlan_reg_get_channel_state_from_secondary_list_for_freq(
							hdd_ctx->pdev,
							config->chan_freq))) {
			hdd_err("No SAP start on DFS channel");
			ret = -EOPNOTSUPP;
			goto error;
		}

		status = ucfg_mlme_get_dfs_ignore_cac(hdd_ctx->psoc,
						      &ignore_cac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("can't get ignore cac flag");

		wlansap_set_dfs_ignore_cac(mac_handle, ignore_cac);

		wlansap_set_dfs_preferred_channel_location(mac_handle);

		wlan_hdd_set_sap_mcc_chnl_avoid(hdd_ctx);
	}

	tgt_dfs_set_tx_leakage_threshold(hdd_ctx->pdev);

	capab_info = mgmt_frame->u.beacon.capab_info;

	config->privacy = (mgmt_frame->u.beacon.capab_info &
			    WLAN_CAPABILITY_PRIVACY) ? true : false;

	(WLAN_HDD_GET_AP_CTX_PTR(adapter))->privacy = config->privacy;

	/*Set wps station to configured */
	ie = wlan_hdd_get_wps_ie_ptr(beacon->tail, beacon->tail_len);

	if (ie) {
		/* To access ie[15], length needs to be at least 14 */
		if (ie[1] < 14) {
			hdd_err("Wps Ie Length(%hhu) is too small",
				ie[1]);
			ret = -EINVAL;
			goto error;
		} else if (memcmp(&ie[2], WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE) ==
			   0) {
			hdd_debug("WPS IE(len %d)", (ie[1] + 2));
			/* Check 15 bit of WPS IE as it contain information for
			 * wps state
			 */
			if (SAP_WPS_ENABLED_UNCONFIGURED == ie[15]) {
				config->wps_state =
					SAP_WPS_ENABLED_UNCONFIGURED;
			} else if (SAP_WPS_ENABLED_CONFIGURED == ie[15]) {
				config->wps_state = SAP_WPS_ENABLED_CONFIGURED;
			}
		}
	} else {
		config->wps_state = SAP_WPS_DISABLED;
	}
	(WLAN_HDD_GET_AP_CTX_PTR(adapter))->encryption_type =
		eCSR_ENCRYPT_TYPE_NONE;

	config->RSNWPAReqIELength = 0;
	memset(&config->RSNWPAReqIE[0], 0, sizeof(config->RSNWPAReqIE));
	ie = wlan_get_ie_ptr_from_eid(WLAN_EID_RSN, beacon->tail,
				      beacon->tail_len);
	/* the RSN and WAPI are not coexisting, so we can check the
	 * WAPI IE if the RSN IE is not set.
	 */
	if (!ie)
		ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_WAPI, beacon->tail,
					      beacon->tail_len);

	if (ie && ie[1]) {
		config->RSNWPAReqIELength = ie[1] + 2;
		if (config->RSNWPAReqIELength < sizeof(config->RSNWPAReqIE))
			memcpy(&config->RSNWPAReqIE[0], ie,
			       config->RSNWPAReqIELength);
		else
			hdd_err("RSN/WPA/WAPI IE MAX Length exceeded; length =%d",
			       config->RSNWPAReqIELength);
		/* The actual processing may eventually be more extensive than
		 * this. Right now, just consume any PMKIDs that are sent in
		 * by the app.
		 */
		status =
			hdd_softap_unpack_ie(cds_get_context
						     (QDF_MODULE_ID_SME),
					     &rsn_encrypt_type,
					     &mc_rsn_encrypt_type,
					     &config->akm_list,
					     &mfp_capable,
					     &mfp_required,
					     config->RSNWPAReqIE[1] + 2,
					     config->RSNWPAReqIE);
		if (status != QDF_STATUS_SUCCESS) {
			ret = -EINVAL;
			goto error;
		} else {
			/* Now copy over all the security attributes you have
			 * parsed out. Use the cipher type in the RSN IE
			 */
			(WLAN_HDD_GET_AP_CTX_PTR(adapter))->
			encryption_type = rsn_encrypt_type;
			hdd_debug("CSR Encryption: %d mcEncryption: %d num_akm_suites:%d",
				  rsn_encrypt_type, mc_rsn_encrypt_type,
				  config->akm_list.numEntries);
			QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD,
					   QDF_TRACE_LEVEL_DEBUG,
					   config->akm_list.authType,
					   config->akm_list.numEntries);

			hdd_softap_update_pasn_vdev_params(hdd_ctx,
							   adapter->vdev_id,
							   beacon,
							   mfp_capable,
							   mfp_required);
		}
	}

	ie = wlan_get_vendor_ie_ptr_from_oui(WPA_OUI_TYPE, WPA_OUI_TYPE_SIZE,
					     beacon->tail, beacon->tail_len);

	if (ie && ie[1] && (ie[0] == DOT11F_EID_WPA)) {
		if (config->RSNWPAReqIE[0]) {
			/*Mixed mode WPA/WPA2 */
			prev_rsn_length = config->RSNWPAReqIELength;
			config->RSNWPAReqIELength += ie[1] + 2;
			if (config->RSNWPAReqIELength <
			    sizeof(config->RSNWPAReqIE))
				memcpy(&config->RSNWPAReqIE[0] +
				       prev_rsn_length, ie, ie[1] + 2);
			else
				hdd_err("RSNWPA IE MAX Length exceeded; length: %d",
				       config->RSNWPAReqIELength);
		} else {
			config->RSNWPAReqIELength = ie[1] + 2;
			if (config->RSNWPAReqIELength <
			    sizeof(config->RSNWPAReqIE))
				memcpy(&config->RSNWPAReqIE[0], ie,
				       config->RSNWPAReqIELength);
			else
				hdd_err("RSNWPA IE MAX Length exceeded; length: %d",
				       config->RSNWPAReqIELength);
			status = hdd_softap_unpack_ie
					(cds_get_context(QDF_MODULE_ID_SME),
					 &rsn_encrypt_type,
					 &mc_rsn_encrypt_type,
					 &config->akm_list,
					 &mfp_capable, &mfp_required,
					 config->RSNWPAReqIE[1] + 2,
					 config->RSNWPAReqIE);

			if (status != QDF_STATUS_SUCCESS) {
				ret = -EINVAL;
				goto error;
			} else {
				(WLAN_HDD_GET_AP_CTX_PTR(adapter))->
				encryption_type = rsn_encrypt_type;
				hdd_debug("CSR Encryption: %d mcEncryption: %d num_akm_suites:%d",
					  rsn_encrypt_type, mc_rsn_encrypt_type,
					  config->akm_list.numEntries);
				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD,
						   QDF_TRACE_LEVEL_DEBUG,
						   config->akm_list.authType,
						   config->akm_list.numEntries);
			}
		}
	}

	if (config->RSNWPAReqIELength > sizeof(config->RSNWPAReqIE)) {
		hdd_err("RSNWPAReqIELength is too large");
		ret = -EINVAL;
		goto error;
	}

	config->SSIDinfo.ssidHidden = false;

	if (ssid) {
		qdf_mem_copy(config->SSIDinfo.ssid.ssId, ssid, ssid_len);
		config->SSIDinfo.ssid.length = ssid_len;

		switch (hidden_ssid) {
		case NL80211_HIDDEN_SSID_NOT_IN_USE:
			config->SSIDinfo.ssidHidden = eHIDDEN_SSID_NOT_IN_USE;
			break;
		case NL80211_HIDDEN_SSID_ZERO_LEN:
			hdd_debug("HIDDEN_SSID_ZERO_LEN");
			config->SSIDinfo.ssidHidden = eHIDDEN_SSID_ZERO_LEN;
			break;
		case NL80211_HIDDEN_SSID_ZERO_CONTENTS:
			hdd_debug("HIDDEN_SSID_ZERO_CONTENTS");
			config->SSIDinfo.ssidHidden =
				eHIDDEN_SSID_ZERO_CONTENTS;
			break;
		default:
			hdd_err("Wrong hidden_ssid param: %d", hidden_ssid);
			break;
		}
	}

	wlan_hdd_set_multipass(vdev);

	qdf_mem_copy(config->self_macaddr.bytes,
		     adapter->mac_addr.bytes,
		     QDF_MAC_ADDR_SIZE);

	/* default value */
	config->SapMacaddr_acl = eSAP_ACCEPT_UNLESS_DENIED;
	config->num_accept_mac = 0;
	config->num_deny_mac = 0;
	status = ucfg_policy_mgr_get_conc_rule1(hdd_ctx->psoc, &conc_rule1);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("can't get ucfg_policy_mgr_get_conc_rule1, use def");
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	/*
	 * We don't want P2PGO to follow STA's channel
	 * so lets limit the logic for SAP only.
	 * Later if we decide to make p2pgo follow STA's
	 * channel then remove this check.
	 */
	if ((0 == conc_rule1) ||
	    (conc_rule1 && (QDF_SAP_MODE == adapter->device_mode)))
		config->cc_switch_mode = mcc_to_scc_switch;
#endif

	if (!(ssid && qdf_str_len(PRE_CAC_SSID) == ssid_len &&
	      (0 == qdf_mem_cmp(ssid, PRE_CAC_SSID, ssid_len)))) {
		uint16_t beacon_data_len;

		beacon_data_len = beacon->head_len - beacon_fixed_len;

		ie = wlan_get_ie_ptr_from_eid(WLAN_EID_SUPP_RATES,
					&mgmt_frame->u.beacon.variable[0],
					beacon_data_len);

		if (ie) {
			ie++;
			if (ie[0] > WLAN_SUPPORTED_RATES_IE_MAX_LEN) {
				hdd_err("Invalid supported rates %d",
					ie[0]);
				ret = -EINVAL;
				goto error;
			}
			config->supported_rates.numRates = ie[0];
			ie++;
			for (i = 0;
			     i < config->supported_rates.numRates; i++) {
				if (ie[i])
					config->supported_rates.rate[i] = ie[i];
			}
			hdd_debug("Configured Num Supported rates: %d",
				  config->supported_rates.numRates);
			QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD,
					   QDF_TRACE_LEVEL_DEBUG,
					   config->supported_rates.rate,
					   config->supported_rates.numRates);
		}
		ie = wlan_get_ie_ptr_from_eid(WLAN_EID_EXT_SUPP_RATES,
					      beacon->tail,
					      beacon->tail_len);
		if (ie) {
			ie++;
			if (ie[0] > WLAN_SUPPORTED_RATES_IE_MAX_LEN) {
				hdd_err("Invalid supported rates %d",
					ie[0]);
				ret = -EINVAL;
				goto error;
			}
			config->extended_rates.numRates = ie[0];
			ie++;
			for (i = 0; i < config->extended_rates.numRates; i++) {
				if (ie[i])
					config->extended_rates.rate[i] = ie[i];
			}

			hdd_debug("Configured Num Extended rates: %d",
				  config->extended_rates.numRates);
			QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD,
					   QDF_TRACE_LEVEL_DEBUG,
					   config->extended_rates.rate,
					   config->extended_rates.numRates);
		}

		config->require_h2e = false;
		wlan_hdd_check_h2e(&config->supported_rates,
				   &config->require_h2e);
		wlan_hdd_check_h2e(&config->extended_rates,
				   &config->require_h2e);
	}

	if (!cds_is_sub_20_mhz_enabled())
		wlan_hdd_set_sap_hwmode(adapter);

	if (QDF_IS_STATUS_ERROR(wlan_hdd_mlo_update(hdd_ctx, config,
						    adapter, beacon))) {
		ret = -EINVAL;
		goto error;
	}
	status = ucfg_mlme_get_vht_for_24ghz(hdd_ctx->psoc, &bval);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		hdd_err("Failed to get vht_for_24ghz");

	if (WLAN_REG_IS_24GHZ_CH_FREQ(config->chan_freq) && bval &&
	    (config->SapHw_mode == eCSR_DOT11_MODE_11n ||
	    config->SapHw_mode == eCSR_DOT11_MODE_11n_ONLY))
		config->SapHw_mode = eCSR_DOT11_MODE_11ac;

	if (((adapter->device_mode == QDF_SAP_MODE) &&
	     (sap_force_11n_for_11ac)) ||
	     ((adapter->device_mode == QDF_P2P_GO_MODE) &&
	     (go_force_11n_for_11ac))) {
		if (config->SapHw_mode == eCSR_DOT11_MODE_11ac ||
		    config->SapHw_mode == eCSR_DOT11_MODE_11ac_ONLY)
			config->SapHw_mode = eCSR_DOT11_MODE_11n;
	}

	config->sap_orig_hw_mode = config->SapHw_mode;
	reg_phy_mode = csr_convert_to_reg_phy_mode(config->SapHw_mode,
						   config->chan_freq);
	updated_phy_mode = wlan_reg_get_max_phymode(hdd_ctx->pdev, reg_phy_mode,
						    config->chan_freq);
	config->SapHw_mode = csr_convert_from_reg_phy_mode(updated_phy_mode);
	if (config->sap_orig_hw_mode != config->SapHw_mode)
		hdd_info("orig phymode %d new phymode %d",
			 config->sap_orig_hw_mode,
			 config->SapHw_mode);
	qdf_mem_zero(sme_config, sizeof(*sme_config));
	sme_get_config_param(mac_handle, sme_config);
	/* Override hostapd.conf wmm_enabled only for 11n and 11AC configs (IOT)
	 * As per spec 11N/AC STA are QOS STA and may not connect or throughput
	 * may not be good with non QOS 11N AP
	 * Default: enable QOS for SAP unless WMM IE not present for 11bga
	 */
	sme_config->csr_config.WMMSupportMode = WMM_USER_MODE_AUTO;
	ie = wlan_get_vendor_ie_ptr_from_oui(WMM_OUI_TYPE, WMM_OUI_TYPE_SIZE,
					     beacon->tail, beacon->tail_len);
	if (!ie && (config->SapHw_mode == eCSR_DOT11_MODE_11a ||
		config->SapHw_mode == eCSR_DOT11_MODE_11g ||
		config->SapHw_mode == eCSR_DOT11_MODE_11b))
		sme_config->csr_config.WMMSupportMode = WMM_USER_MODE_NO_QOS;
	sme_update_config(mac_handle, sme_config);

	if (((adapter->device_mode == QDF_SAP_MODE) &&
	     (sap_force_11n_for_11ac)) ||
	     ((adapter->device_mode == QDF_P2P_GO_MODE) &&
	     (go_force_11n_for_11ac))) {
		if (config->ch_width_orig > CH_WIDTH_40MHZ)
			config->ch_width_orig = CH_WIDTH_40MHZ;
	}

	if (wlan_hdd_setup_driver_overrides(adapter)) {
		ret = -EINVAL;
		goto error;
	}

	config->ch_params.ch_width = config->ch_width_orig;
	if (sap_phymode_is_eht(config->SapHw_mode))
		wlan_reg_set_create_punc_bitmap(&config->ch_params, true);
	if ((config->ch_params.ch_width == CH_WIDTH_80P80MHZ) &&
	    ucfg_mlme_get_restricted_80p80_bw_supp(hdd_ctx->psoc)) {
		if (!((config->ch_params.center_freq_seg0 == 138 &&
		    config->ch_params.center_freq_seg1 == 155) ||
		    (config->ch_params.center_freq_seg1 == 138 &&
		     config->ch_params.center_freq_seg0 == 155))) {
			hdd_debug("Falling back to 80 from 80p80 as non supported freq_seq0 %d and freq_seq1 %d",
				  config->ch_params.mhz_freq_seg0,
				  config->ch_params.mhz_freq_seg1);
			config->ch_params.center_freq_seg1 = 0;
			config->ch_params.mhz_freq_seg1 = 0;
			config->ch_width_orig = CH_WIDTH_80MHZ;
			config->ch_params.ch_width = config->ch_width_orig;
		}
	}

	wlan_reg_set_channel_params_for_pwrmode(hdd_ctx->pdev,
						config->chan_freq,
						config->sec_ch_freq,
						&config->ch_params,
						REG_CURRENT_PWR_MODE);
	if (0 != wlan_hdd_cfg80211_update_apies(adapter)) {
		hdd_err("SAP Not able to set AP IEs");
		ret = -EINVAL;
		goto error;
	}

	hdd_nofl_debug("SAP mac:" QDF_MAC_ADDR_FMT " SSID: " QDF_SSID_FMT " BCNINTV:%d Freq:%d freq_seg0:%d freq_seg1:%d ch_width:%d HW mode:%d privacy:%d akm:%d acs_mode:%d acs_dfs_mode %d dtim period:%d MFPC %d, MFPR %d",
		       QDF_MAC_ADDR_REF(adapter->mac_addr.bytes),
		       QDF_SSID_REF(config->SSIDinfo.ssid.length,
				    config->SSIDinfo.ssid.ssId),
		       (int)config->beacon_int,
		       config->chan_freq, config->ch_params.mhz_freq_seg0,
		       config->ch_params.mhz_freq_seg1,
		       config->ch_params.ch_width,
		       config->SapHw_mode, config->privacy,
		       config->authType, config->acs_cfg.acs_mode,
		       config->acs_dfs_mode, config->dtim_period,
		       mfp_capable, mfp_required);

	mutex_lock(&hdd_ctx->sap_lock);
	if (cds_is_driver_unloading()) {
		mutex_unlock(&hdd_ctx->sap_lock);

		hdd_err("The driver is unloading, ignore the bss starting");
		ret = -EINVAL;
		goto error;
	}

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		mutex_unlock(&hdd_ctx->sap_lock);

		wlansap_reset_sap_config_add_ie(config, eUPDATE_IE_ALL);
		/* Bss already started. just return. */
		/* TODO Probably it should update some beacon params. */
		hdd_debug("Bss Already started...Ignore the request");
		hdd_exit();
		ret = 0;
		goto free;
	}

	if (check_for_concurrency) {
		if (!policy_mgr_allow_concurrency(hdd_ctx->psoc,
				policy_mgr_convert_device_mode_to_qdf_type(
					adapter->device_mode),
				config->chan_freq, HW_MODE_20_MHZ,
				policy_mgr_get_conc_ext_flags(vdev, false))) {
			mutex_unlock(&hdd_ctx->sap_lock);

			hdd_err("This concurrency combination is not allowed");
			ret = -EINVAL;
			goto error;
		}
	}

	if (!hdd_set_connection_in_progress(true)) {
		mutex_unlock(&hdd_ctx->sap_lock);

		hdd_err("Can't start BSS: set connection in progress failed");
		ret = -EINVAL;
		goto error;
	}

	config->persona = adapter->device_mode;

	sap_event_callback = hdd_hostapd_sap_event_cb;

	(WLAN_HDD_GET_AP_CTX_PTR(adapter))->dfs_cac_block_tx = true;
	set_bit(SOFTAP_INIT_DONE, &adapter->event_flags);

	ucfg_dp_set_dfs_cac_tx(vdev, true);

	qdf_event_reset(&hostapd_state->qdf_event);

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	if (!sap_ctx) {
		ret = -EINVAL;
		goto error;
	}

	status = wlansap_start_bss(sap_ctx, sap_event_callback, config,
				   adapter->dev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		mutex_unlock(&hdd_ctx->sap_lock);

		hdd_set_connection_in_progress(false);
		hdd_err("SAP Start Bss fail");
		ret = -EINVAL;
		goto error;
	}

	qdf_status = qdf_wait_single_event(&hostapd_state->qdf_event,
					SME_CMD_START_BSS_TIMEOUT);

	wlansap_reset_sap_config_add_ie(config, eUPDATE_IE_ALL);

	if (QDF_IS_STATUS_ERROR(qdf_status) ||
	    QDF_IS_STATUS_ERROR(hostapd_state->qdf_status)) {
		mutex_unlock(&hdd_ctx->sap_lock);
		if (QDF_IS_STATUS_ERROR(qdf_status))
			hdd_err("Wait for start BSS failed status %d",
				qdf_status);
		else
			hdd_err("Start BSS failed status %d",
				hostapd_state->qdf_status);
		hdd_set_connection_in_progress(false);
		sme_get_command_q_status(mac_handle);
		wlansap_stop_bss(WLAN_HDD_GET_SAP_CTX_PTR(adapter));
		if (!cds_is_driver_recovering())
			QDF_ASSERT(0);
		ret = -EINVAL;
		goto error;
	}
	/* Successfully started Bss update the state bit. */
	set_bit(SOFTAP_BSS_STARTED, &adapter->event_flags);

	mutex_unlock(&hdd_ctx->sap_lock);

	/* Initialize WMM configuration */
	hdd_wmm_dscp_initial_state(adapter);
	if (hostapd_state->bss_state == BSS_START) {
		policy_mgr_incr_active_session(hdd_ctx->psoc,
					adapter->device_mode,
					adapter->vdev_id);

		hdd_green_ap_start_state_mc(hdd_ctx, adapter->device_mode,
					    true);
		wlan_set_sap_user_config_freq(vdev, user_config_freq);
	}

	wlan_hdd_dhcp_offload_enable(hdd_ctx, adapter);
	ucfg_p2p_status_start_bss(vdev);

	/* Check and restart SAP if it is on unsafe channel */
	hdd_unsafe_channel_restart_sap(hdd_ctx);

	ucfg_ftm_time_sync_update_bss_state(vdev,
					    FTM_TIME_SYNC_BSS_STARTED);

	hdd_set_connection_in_progress(false);
	policy_mgr_process_force_scc_for_nan(hdd_ctx->psoc);
	ret = 0;
	hdd_medium_assess_init();
	goto free;

error:
	wlan_hdd_mlo_reset(adapter);
	/* Revert the indoor to passive marking if START BSS fails */
	if (indoor_chnl_marking && adapter->device_mode == QDF_SAP_MODE) {
		hdd_update_indoor_channel(hdd_ctx, false);
		sme_update_channel_list(mac_handle);
	}
	clear_bit(SOFTAP_INIT_DONE, &adapter->event_flags);
	qdf_atomic_set(&adapter->session.ap.acs_in_progress, 0);
	wlansap_reset_sap_config_add_ie(config, eUPDATE_IE_ALL);

free:
	wlan_twt_concurrency_update(hdd_ctx);
	if (deliver_start_evt) {
		status = ucfg_if_mgr_deliver_event(
					vdev,
					WLAN_IF_MGR_EV_AP_START_BSS_COMPLETE,
					NULL);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("start bss complete failed!!");
			ret = -EINVAL;
		}
	}
	qdf_mem_free(sme_config);
deliver_start_err:
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_HDD_ID_OBJ_MGR);

	return ret;
}

int hdd_destroy_acs_timer(struct hdd_adapter *adapter)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	if (!adapter->session.ap.vendor_acs_timer_initialized)
		return 0;

	adapter->session.ap.vendor_acs_timer_initialized = false;

	clear_bit(VENDOR_ACS_RESPONSE_PENDING, &adapter->event_flags);
	if (QDF_TIMER_STATE_RUNNING ==
			adapter->session.ap.vendor_acs_timer.state) {
		qdf_status =
			qdf_mc_timer_stop(&adapter->session.ap.
					vendor_acs_timer);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_err("Failed to stop ACS timer");
	}

	if (adapter->session.ap.vendor_acs_timer.user_data)
		qdf_mem_free(adapter->session.ap.vendor_acs_timer.user_data);

	qdf_mc_timer_destroy(&adapter->session.ap.vendor_acs_timer);

	return 0;
}

/**
 * __wlan_hdd_cfg80211_stop_ap() - stop soft ap
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 *
 * Return: 0 for success non-zero for failure
 */
static int __wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy,
					struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	tSirUpdateIE update_ie;
	int ret;
	mac_handle_t mac_handle;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	/*
	 * In case of SSR and other FW down cases, validate context will
	 * fail. But return success to upper layer so that it can clean up
	 * kernel variables like beacon interval. If the failure status
	 * is returned then next set beacon command will fail as beacon
	 * interval in not reset.
	 */
	if (ret)
		goto exit;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		goto exit;
	}

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_err("Driver module is closed; dropping request");
		goto exit;
	}

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		goto exit;

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_STOP_AP,
		   adapter->vdev_id, adapter->device_mode);

	if (!(adapter->device_mode == QDF_SAP_MODE ||
	      adapter->device_mode == QDF_P2P_GO_MODE)) {
		hdd_err("stop ap is given on device modes other than SAP/GO. Hence return");
		goto exit;
	}

	/* Clear SOFTAP_INIT_DONE flag to mark stop_ap deinit. So that we do
	 * not restart SAP after SSR as SAP is already stopped from user space.
	 * This update is moved to start of this function to resolve stop_ap
	 * call during SSR case. Adapter gets cleaned up as part of SSR.
	 */
	clear_bit(SOFTAP_INIT_DONE, &adapter->event_flags);
	hdd_debug("Event flags 0x%lx(%s) Device_mode %s(%d)",
		  adapter->event_flags, (adapter->dev)->name,
		  qdf_opmode_str(adapter->device_mode), adapter->device_mode);

	if (adapter->device_mode == QDF_SAP_MODE) {
		wlan_hdd_del_station(adapter, NULL);
		mac_handle = hdd_ctx->mac_handle;
		status = wlan_hdd_flush_pmksa_cache(adapter);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_debug("Cannot flush PMKIDCache");
	}

	cds_flush_work(&adapter->sap_stop_bss_work);
	adapter->session.ap.sap_config.acs_cfg.acs_mode = false;
	hdd_dcs_clear(adapter);
	qdf_atomic_set(&adapter->session.ap.acs_in_progress, 0);
	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	wlan_hdd_cleanup_actionframe(adapter);
	wlan_hdd_cleanup_remain_on_channel_ctx(adapter);
	mutex_lock(&hdd_ctx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		struct hdd_hostapd_state *hostapd_state =
			WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

		hdd_place_marker(adapter, "TRY TO STOP", NULL);
		qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
		status = wlansap_stop_bss(WLAN_HDD_GET_SAP_CTX_PTR(adapter));
		if (QDF_IS_STATUS_SUCCESS(status)) {
			qdf_status =
				qdf_wait_single_event(&hostapd_state->
					qdf_stop_bss_event,
					SME_CMD_STOP_BSS_TIMEOUT);

			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hdd_err("qdf wait for single_event failed!!");
				hdd_place_marker(adapter, "STOP with FAILURE",
						 NULL);
				hdd_sap_indicate_disconnect_for_sta(adapter);
				hdd_medium_assess_deinit();
				QDF_ASSERT(0);
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &adapter->event_flags);

		/*BSS stopped, clear the active sessions for this device mode*/
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
						adapter->device_mode,
						adapter->vdev_id);
		hdd_green_ap_start_state_mc(hdd_ctx, adapter->device_mode,
					    false);
		wlan_twt_concurrency_update(hdd_ctx);
		wlan_set_sap_user_config_freq(adapter->vdev, 0);
		status = ucfg_if_mgr_deliver_event(adapter->vdev,
				WLAN_IF_MGR_EV_AP_STOP_BSS_COMPLETE,
				NULL);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("Stopping the BSS failed");
			goto exit;
		}
		if (adapter->session.ap.beacon) {
			qdf_mem_free(adapter->session.ap.beacon);
			adapter->session.ap.beacon = NULL;
		}
	} else {
		hdd_debug("SAP already down");
		mutex_unlock(&hdd_ctx->sap_lock);
		goto exit;
	}

	mutex_unlock(&hdd_ctx->sap_lock);

	mac_handle = hdd_ctx->mac_handle;

	if (ucfg_pre_cac_is_active(hdd_ctx->psoc))
		ucfg_pre_cac_clean_up(hdd_ctx->psoc);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Stopping the BSS");
		goto exit;
	}

	qdf_copy_macaddr(&update_ie.bssid, &adapter->mac_addr);
	update_ie.vdev_id = adapter->vdev_id;
	update_ie.ieBufferlength = 0;
	update_ie.pAdditionIEBuffer = NULL;
	update_ie.append = true;
	update_ie.notify = true;
	if (sme_update_add_ie(mac_handle,
			      &update_ie,
			      eUPDATE_IE_PROBE_BCN) == QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass on PROBE_RSP_BCN data to PE");
	}

	if (sme_update_add_ie(mac_handle,
			      &update_ie,
			      eUPDATE_IE_ASSOC_RESP) == QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass on ASSOC_RSP data to PE");
	}
	/* Reset WNI_CFG_PROBE_RSP Flags */
	wlan_hdd_reset_prob_rspies(adapter);
	hdd_destroy_acs_timer(adapter);

	ucfg_p2p_status_stop_bss(adapter->vdev);
	ucfg_ftm_time_sync_update_bss_state(adapter->vdev,
					    FTM_TIME_SYNC_BSS_STOPPED);

exit:
	if (adapter->session.ap.beacon) {
		qdf_mem_free(adapter->session.ap.beacon);
		adapter->session.ap.beacon = NULL;
	}
	if (QDF_IS_STATUS_SUCCESS(status))
		hdd_place_marker(adapter, "STOP with SUCCESS", NULL);
	else
		hdd_place_marker(adapter, "STOP with FAILURE", NULL);

	hdd_exit();

	return 0;
}

#ifdef CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT
int wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev,
			      unsigned int link_id)
#else
int wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
#endif
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	/*
	 * The stop_ap can be called in the same context through
	 * wlan_hdd_del_virtual_intf. As vdev_trans is already taking place as
	 * part of the del_vitrtual_intf, this vdev_op cannot start.
	 * Return 0 in case op is not started so that the kernel frees the
	 * beacon memory properly.
	 */
	if (errno)
		return 0;

	errno = __wlan_hdd_cfg80211_stop_ap(wiphy, dev);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0))
/*
 * Beginning with 4.7 struct ieee80211_channel uses enum nl80211_band
 * ieee80211_channel_band() - return channel band
 * chan: channel
 *
 * Return: channel band
 */
static inline
enum nl80211_band ieee80211_channel_band(const struct ieee80211_channel *chan)
{
	return chan->band;
}
#else
/*
 * Prior to 4.7 struct ieee80211_channel used enum ieee80211_band. However the
 * ieee80211_band enum values are assigned from enum nl80211_band so we can
 * safely typecast one to another.
 */
static inline
enum nl80211_band ieee80211_channel_band(const struct ieee80211_channel *chan)
{
	enum ieee80211_band band = chan->band;

	return (enum nl80211_band)band;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)) || \
	defined(CFG80211_BEACON_TX_RATE_CUSTOM_BACKPORT)
/**
 * hdd_get_data_rate_from_rate_mask() - convert mask to rate
 * @wiphy: Pointer to wiphy
 * @band: band
 * @bit_rate_mask: pointer to bit_rake_mask
 *
 * This function takes band and bit_rate_mask as input and
 * derives the beacon_tx_rate based on the supported rates
 * published as part of wiphy register.
 *
 * Return: data rate for success or zero for failure
 */
static uint16_t hdd_get_data_rate_from_rate_mask(struct wiphy *wiphy,
		enum nl80211_band band,
		struct cfg80211_bitrate_mask *bit_rate_mask)
{
	struct ieee80211_supported_band *sband = wiphy->bands[band];
	int sband_n_bitrates;
	struct ieee80211_rate *sband_bitrates;
	int i;

	if (sband) {
		sband_bitrates = sband->bitrates;
		sband_n_bitrates = sband->n_bitrates;
		for (i = 0; i < sband_n_bitrates; i++) {
			if (bit_rate_mask->control[band].legacy ==
			    sband_bitrates[i].hw_value)
				return sband_bitrates[i].bitrate;
		}
	}
	return 0;
}

/**
 * hdd_update_beacon_rate() - Update beacon tx rate
 * @adapter: Pointer to hdd_adapter_t
 * @wiphy: Pointer to wiphy
 * @params: Pointet to cfg80211_ap_settings
 *
 * This function updates the beacon tx rate which is provided
 * as part of cfg80211_ap_settions in to the sap_config
 * structure
 *
 * Return: none
 */
static void hdd_update_beacon_rate(struct hdd_adapter *adapter,
		struct wiphy *wiphy,
		struct cfg80211_ap_settings *params)
{
	struct cfg80211_bitrate_mask *beacon_rate_mask;
	enum nl80211_band band;

	band = ieee80211_channel_band(params->chandef.chan);
	beacon_rate_mask = &params->beacon_rate;
	if (beacon_rate_mask->control[band].legacy) {
		adapter->session.ap.sap_config.beacon_tx_rate =
			hdd_get_data_rate_from_rate_mask(wiphy, band,
					beacon_rate_mask);
		hdd_debug("beacon mask value %u, rate %hu",
			  params->beacon_rate.control[0].legacy,
			  adapter->session.ap.sap_config.beacon_tx_rate);
	}
}
#else
static void hdd_update_beacon_rate(struct hdd_adapter *adapter,
		struct wiphy *wiphy,
		struct cfg80211_ap_settings *params)
{
}
#endif

/**
 * wlan_hdd_get_sap_ch_params() - Get channel parameters of SAP
 * @hdd_ctx: HDD context pointer
 * @vdev_id: vdev id
 * @freq: channel frequency (MHz)
 * @ch_params: pointer to channel parameters
 *
 * The function gets channel parameters of SAP by vdev id.
 *
 * Return: QDF_STATUS_SUCCESS if get channel parameters successful
 */
static QDF_STATUS
wlan_hdd_get_sap_ch_params(struct hdd_context *hdd_ctx,
			   uint8_t vdev_id, uint32_t freq,
			   struct ch_params *ch_params)
{
	struct hdd_adapter *adapter;

	if (!hdd_ctx || !ch_params) {
		hdd_err("invalid hdd_ctx or ch_params");
		return QDF_STATUS_E_INVAL;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter)
		return QDF_STATUS_E_INVAL;

	if (!wlan_sap_get_ch_params(WLAN_HDD_GET_SAP_CTX_PTR(adapter),
				    ch_params))
		wlan_reg_set_channel_params_for_pwrmode(hdd_ctx->pdev, freq, 0,
							ch_params,
							REG_CURRENT_PWR_MODE);
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_hdd_is_same_freq_seg() - Check freq segment is same or not
 * @chandef: channel of new SAP
 * @ch_params: channel parameters of existed SAP
 *
 * If two SAP on same channel, and channel type is NL80211_CHAN_HT40MINUS or
 * NL80211_CHAN_HT40PLUS, or channel offset is PHY_DOUBLE_CHANNEL_HIGH_PRIMARY
 * or PHY_DOUBLE_CHANNEL_LOW_PRIMARY, then check freq segment is same or not.
 *
 * Return: true if freq segment is same
 */
static bool
wlan_hdd_is_same_freq_seg(struct cfg80211_chan_def *chandef,
			  struct ch_params *ch_params)
{
	if (!chandef || !ch_params)
		return false;

	/* they are equal if NL80211_CHAN_NO_HT or NL80211_CHAN_HT20 */
	if (chandef->center_freq1 == chandef->chan->center_freq)
		return true;

	if ((ch_params->sec_ch_offset != PHY_DOUBLE_CHANNEL_HIGH_PRIMARY) &&
	    (ch_params->sec_ch_offset != PHY_DOUBLE_CHANNEL_LOW_PRIMARY))
		return true;

	if ((chandef->center_freq1 != ch_params->mhz_freq_seg0) ||
	    (chandef->center_freq2 != ch_params->mhz_freq_seg1))
		return false;

	return true;
}

/**
 * wlan_hdd_is_ap_ap_force_scc_override() - force Same band SCC chan override
 * @adapter: SAP adapter pointer
 * @chandef: SAP starting channel
 * @new_chandef: new override SAP channel
 *
 * The function will override the second SAP chan to the first SAP's home
 * channel if the FW doesn't support MCC and force SCC enabled in INI.
 *
 * Return: true if channel override
 */
static bool
wlan_hdd_is_ap_ap_force_scc_override(struct hdd_adapter *adapter,
				     struct cfg80211_chan_def *chandef,
				     struct cfg80211_chan_def *new_chandef)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	uint32_t cc_count, i;
	uint32_t op_freq[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct ch_params ch_params;
	enum nl80211_channel_type channel_type;
	uint8_t con_vdev_id;
	uint32_t con_freq;
	struct ieee80211_channel *ieee_chan;
	uint32_t freq;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	if (!hdd_ctx || !chandef) {
		hdd_err("hdd context or chandef is NULL");
		return false;
	}
	freq = chandef->chan->center_freq;
	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	if (!vdev) {
		hdd_err("failed to get vdev");
		return false;
	}
	if (policy_mgr_is_ap_ap_mcc_allow(
			hdd_ctx->psoc, hdd_ctx->pdev, vdev, freq,
			hdd_map_nl_chan_width(chandef->width))) {
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
		return false;
	}
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);

	if (hdd_handle_p2p_go_for_3rd_ap_conc(hdd_ctx->psoc, adapter, freq))
		return false;

	cc_count = policy_mgr_get_mode_specific_conn_info(hdd_ctx->psoc,
							  &op_freq[0],
							  &vdev_id[0],
							  PM_SAP_MODE);
	if (cc_count < MAX_NUMBER_OF_CONC_CONNECTIONS)
		cc_count = cc_count +
				policy_mgr_get_mode_specific_conn_info(
					hdd_ctx->psoc,
					&op_freq[cc_count],
					&vdev_id[cc_count],
					PM_P2P_GO_MODE);
	for (i = 0 ; i < cc_count; i++) {
		if (freq == op_freq[i]) {
			status = wlan_hdd_get_sap_ch_params(hdd_ctx,
							    vdev_id[i],
							    op_freq[i],
							    &ch_params);
			if (QDF_IS_STATUS_SUCCESS(status) &&
			    (!wlan_hdd_is_same_freq_seg(chandef, &ch_params)))
				break;
			continue;
		}
		if (!policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc))
			break;
		if (wlan_reg_is_same_band_freqs(freq, op_freq[i]) &&
		    !policy_mgr_are_sbs_chan(hdd_ctx->psoc, freq, op_freq[i]))
			break;
	}
	if (i >= cc_count)
		return false;
	con_freq = op_freq[i];
	con_vdev_id = vdev_id[i];
	ieee_chan = ieee80211_get_channel(hdd_ctx->wiphy,
					  con_freq);
	if (!ieee_chan) {
		hdd_err("channel conversion failed");
		return false;
	}

	status = wlan_hdd_get_sap_ch_params(hdd_ctx, con_vdev_id, con_freq,
					    &ch_params);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	switch (ch_params.sec_ch_offset) {
	case PHY_SINGLE_CHANNEL_CENTERED:
		channel_type = NL80211_CHAN_HT20;
		break;
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		channel_type = NL80211_CHAN_HT40MINUS;
		break;
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		channel_type = NL80211_CHAN_HT40PLUS;
		break;
	default:
		channel_type = NL80211_CHAN_NO_HT;
		break;
	}
	cfg80211_chandef_create(new_chandef, ieee_chan, channel_type);
	switch (ch_params.ch_width) {
	case CH_WIDTH_80MHZ:
		new_chandef->width = NL80211_CHAN_WIDTH_80;
		break;
	case CH_WIDTH_80P80MHZ:
		new_chandef->width = NL80211_CHAN_WIDTH_80P80;
		if (ch_params.mhz_freq_seg1)
			new_chandef->center_freq2 = ch_params.mhz_freq_seg1;
		break;
	case CH_WIDTH_160MHZ:
		new_chandef->width = NL80211_CHAN_WIDTH_160;
		break;
	default:
		break;
	}

	wlan_hdd_set_chandef_width(new_chandef, ch_params.ch_width);

	if ((ch_params.ch_width == CH_WIDTH_80MHZ) ||
	    (ch_params.ch_width == CH_WIDTH_80P80MHZ) ||
	    (ch_params.ch_width == CH_WIDTH_160MHZ) ||
	    wlan_hdd_is_chwidth_320mhz(ch_params.ch_width)) {
		if (ch_params.mhz_freq_seg0)
			new_chandef->center_freq1 = ch_params.mhz_freq_seg0;
	}

	hdd_debug("override AP freq %d to first AP(vdev_id %d) center_freq:%d width:%d freq1:%d freq2:%d ",
		  freq, con_vdev_id, new_chandef->chan->center_freq,
		  new_chandef->width, new_chandef->center_freq1,
		  new_chandef->center_freq2);
	return true;
}

#ifdef NDP_SAP_CONCURRENCY_ENABLE
/**
 * hdd_sap_nan_check_and_disable_unsupported_ndi: Wrapper function for
 * ucfg_nan_check_and_disable_unsupported_ndi
 * @psoc: pointer to psoc object
 * @force: When set forces NDI disable
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
hdd_sap_nan_check_and_disable_unsupported_ndi(struct wlan_objmgr_psoc *psoc,
					      bool force)
{
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
hdd_sap_nan_check_and_disable_unsupported_ndi(struct wlan_objmgr_psoc *psoc,
					      bool force)
{
	return  ucfg_nan_check_and_disable_unsupported_ndi(psoc, force);
}
#endif

#if defined(WLAN_SUPPORT_TWT) && \
	((LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)) || \
	  defined(CFG80211_TWT_RESPONDER_SUPPORT))
#ifdef WLAN_TWT_CONV_SUPPORTED
static void
wlan_hdd_update_twt_responder(struct hdd_context *hdd_ctx,
			      struct cfg80211_ap_settings *params)
{
	bool twt_res_svc_cap, enable_twt;
	uint32_t reason;

	enable_twt = ucfg_twt_cfg_is_twt_enabled(hdd_ctx->psoc);
	ucfg_twt_get_responder(hdd_ctx->psoc, &twt_res_svc_cap);
	ucfg_twt_cfg_set_responder(hdd_ctx->psoc,
				   QDF_MIN(twt_res_svc_cap,
					   (enable_twt &&
					    params->twt_responder)));
	hdd_debug("cfg80211 TWT responder:%d", params->twt_responder);
	if (enable_twt && params->twt_responder) {
		hdd_send_twt_responder_enable_cmd(hdd_ctx);
	} else {
		reason = HOST_TWT_DISABLE_REASON_NONE;
		hdd_send_twt_responder_disable_cmd(hdd_ctx, reason);
	}
}
#else
static void
wlan_hdd_update_twt_responder(struct hdd_context *hdd_ctx,
			      struct cfg80211_ap_settings *params)
{
	bool twt_res_svc_cap, enable_twt;
	uint32_t reason;

	enable_twt = ucfg_mlme_is_twt_enabled(hdd_ctx->psoc);
	ucfg_mlme_get_twt_res_service_cap(hdd_ctx->psoc, &twt_res_svc_cap);
	ucfg_mlme_set_twt_responder(hdd_ctx->psoc,
				    QDF_MIN(twt_res_svc_cap,
					    (enable_twt && params->twt_responder)));
	hdd_debug("cfg80211 TWT responder:%d", params->twt_responder);
	if (enable_twt && params->twt_responder) {
		hdd_send_twt_responder_enable_cmd(hdd_ctx);
	} else {
		reason = HOST_TWT_DISABLE_REASON_NONE;
		hdd_send_twt_responder_disable_cmd(hdd_ctx, reason);
	}
}
#endif
#else
static inline void
wlan_hdd_update_twt_responder(struct hdd_context *hdd_ctx,
			      struct cfg80211_ap_settings *params)
{}
#endif

#ifdef CFG80211_SINGLE_NETDEV_MULTI_LINK_SUPPORT
static inline uint32_t
wlan_util_get_centre_freq(struct wireless_dev *wdev, unsigned int link_id)
{
	return wdev->links[link_id].ap.chandef.chan->center_freq;
}

static inline struct cfg80211_chan_def
wlan_util_get_chan_def(struct wireless_dev *wdev, unsigned int link_id)
{
	return wdev->links[link_id].ap.chandef;
}
#else
static inline struct cfg80211_chan_def
wlan_util_get_chan_def(struct wireless_dev *wdev, unsigned int link_id)
{
	return wdev->chandef;
}

static inline uint32_t
wlan_util_get_centre_freq(struct wireless_dev *wdev, unsigned int link_id)
{
	return wdev->chandef.chan->center_freq;
}
#endif

#if defined(WLAN_FEATURE_SR) && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
/**
 * hdd_update_he_obss_pd() - Enable or disable spatial reuse
 * based on user space input and concurrency combination.
 * @adapter:  Pointer to hostapd adapter
 * @params: Pointer to AP configuration from cfg80211
 *
 * Return: void
 */
static void hdd_update_he_obss_pd(struct hdd_adapter *adapter,
				  struct cfg80211_ap_settings *params)
{
	struct wlan_objmgr_vdev *vdev;
	struct ieee80211_he_obss_pd *obss_pd;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
	if (!vdev)
		return;

	if (params && params->he_obss_pd.enable) {
		obss_pd = &params->he_obss_pd;
		ucfg_spatial_reuse_set_sr_config(vdev,
						 obss_pd->sr_ctrl,
						 obss_pd->non_srg_max_offset);
		ucfg_spatial_reuse_set_sr_enable(vdev, obss_pd->enable);
		hdd_debug("obss_pd_enable: %d, sr_ctrl: %d, non_srg_max_offset: %d",
			  obss_pd->enable, obss_pd->sr_ctrl,
			  obss_pd->non_srg_max_offset);
	}
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);
}
#else
static inline
void hdd_update_he_obss_pd(struct hdd_adapter *adapter,
			   struct cfg80211_ap_settings *params)
{
}
#endif

/**
 * __wlan_hdd_cfg80211_start_ap() - start soft ap mode
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 * @params: Pointer to AP settings parameters
 *
 * Return: 0 for success non-zero for failure
 */
static int __wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
					struct net_device *dev,
					struct cfg80211_ap_settings *params)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	enum hw_mode_bandwidth channel_width;
	int status;
	struct sme_sta_inactivity_timeout  *sta_inactivity_timer;
	uint8_t mandt_chnl_list = 0;
	qdf_freq_t freq;
	uint16_t sta_cnt, sap_cnt;
	bool val;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_chan_def new_chandef;
	struct cfg80211_chan_def *chandef;
	bool srd_channel_allowed, disable_nan = true;
	enum QDF_OPMODE vdev_opmode;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS], i;
	enum policy_mgr_con_mode intf_pm_mode;
	struct wlan_objmgr_vdev *vdev;
	uint16_t link_id = 0;

	hdd_enter();

	clear_bit(SOFTAP_INIT_DONE, &adapter->event_flags);
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_START_AP,
		   adapter->vdev_id, params->beacon_interval);

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("HDD adapter magic is invalid");
		return -ENODEV;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return status;

	hdd_nofl_info("%s(vdevid-%d): START AP: mode %s(%d) %d bw %d sub20 %d",
		      dev->name, adapter->vdev_id,
		      qdf_opmode_str(adapter->device_mode),
		      adapter->device_mode,
		      params->chandef.chan->center_freq,
		      params->chandef.width,
		      cds_is_sub_20_mhz_enabled());
	if (policy_mgr_is_hw_mode_change_in_progress(hdd_ctx->psoc)) {
		status = policy_mgr_wait_for_connection_update(
			hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("qdf wait for event failed!!");
			return -EINVAL;
		}
	}

	hdd_reg_wait_for_country_change(hdd_ctx);

	channel_width = wlan_hdd_get_channel_bw(params->chandef.width);
	freq = (qdf_freq_t)params->chandef.chan->center_freq;

	if (wlan_reg_is_6ghz_chan_freq(freq) &&
	    !wlan_reg_is_6ghz_band_set(hdd_ctx->pdev)) {
		hdd_err("6 GHz band disabled.");
		return -EINVAL;
	}

	chandef = &params->chandef;
	if ((adapter->device_mode == QDF_SAP_MODE ||
	     adapter->device_mode == QDF_P2P_GO_MODE) &&
	    wlan_hdd_is_ap_ap_force_scc_override(adapter,
						 chandef,
						 &new_chandef)) {
		chandef = &new_chandef;
		freq = (qdf_freq_t)chandef->chan->center_freq;
		channel_width = wlan_hdd_get_channel_bw(chandef->width);
	}

	if (QDF_STATUS_SUCCESS !=
	    ucfg_policy_mgr_get_sap_mandt_chnl(hdd_ctx->psoc, &mandt_chnl_list))
		hdd_err("can't get mandatory channel list");
	if (mandt_chnl_list)
		policy_mgr_init_sap_mandatory_chan(hdd_ctx->psoc,
						   chandef->chan->center_freq);

	adapter->session.ap.sap_config.ch_params.center_freq_seg0 =
				cds_freq_to_chan(chandef->center_freq1);
	adapter->session.ap.sap_config.ch_params.center_freq_seg1 =
				cds_freq_to_chan(chandef->center_freq2);
	adapter->session.ap.sap_config.ch_params.mhz_freq_seg0 =
							chandef->center_freq1;
	adapter->session.ap.sap_config.ch_params.mhz_freq_seg1 =
							chandef->center_freq2;

	status = policy_mgr_is_sap_allowed_on_dfs_freq(
						hdd_ctx->pdev,
						adapter->vdev_id,
						chandef->chan->center_freq);
	if (!status)
		return -EINVAL;

	status = policy_mgr_is_sap_allowed_on_indoor(
						hdd_ctx->pdev,
						adapter->vdev_id,
						chandef->chan->center_freq);
	if (!status) {
		hdd_debug("SAP start not allowed on indoor channel %d",
			  chandef->chan->center_freq);
		return -EINVAL;
	}

	intf_pm_mode = policy_mgr_convert_device_mode_to_qdf_type(
							adapter->device_mode);
	status = policy_mgr_is_multi_sap_allowed_on_same_band(
				hdd_ctx->pdev,
				intf_pm_mode,
				chandef->chan->center_freq);
	if (!status)
		return -EINVAL;

	vdev_opmode = wlan_vdev_mlme_get_opmode(adapter->vdev);
	ucfg_mlme_get_srd_master_mode_for_vdev(hdd_ctx->psoc, vdev_opmode,
					       &srd_channel_allowed);

	if (!srd_channel_allowed &&
	    wlan_reg_is_etsi13_srd_chan_for_freq(hdd_ctx->pdev, freq)) {
		hdd_err("vdev opmode %d not allowed on SRD channel.",
			vdev_opmode);
		return -EINVAL;
	}
	if (cds_is_sub_20_mhz_enabled()) {
		enum channel_state ch_state;
		enum phy_ch_width sub_20_ch_width = CH_WIDTH_INVALID;
		struct sap_config *sap_cfg = &adapter->session.ap.sap_config;

		if (CHANNEL_STATE_DFS ==
		    wlan_reg_get_channel_state_from_secondary_list_for_freq(
								hdd_ctx->pdev,
								freq)) {
			hdd_err("Can't start SAP-DFS (channel=%d)with sub 20 MHz ch wd",
				freq);
			return -EINVAL;
		}
		if (channel_width != HW_MODE_20_MHZ) {
			hdd_err("Hostapd (20+ MHz) conflits with config.ini (sub 20 MHz)");
			return -EINVAL;
		}
		if (cds_is_5_mhz_enabled())
			sub_20_ch_width = CH_WIDTH_5MHZ;
		if (cds_is_10_mhz_enabled())
			sub_20_ch_width = CH_WIDTH_10MHZ;
		if (WLAN_REG_IS_5GHZ_CH_FREQ(freq))
			ch_state = wlan_reg_get_5g_bonded_channel_state_for_freq(hdd_ctx->pdev, freq,
										 sub_20_ch_width);
		else
			ch_state = wlan_reg_get_2g_bonded_channel_state_for_freq(hdd_ctx->pdev, freq,
										 sub_20_ch_width, 0);
		if (CHANNEL_STATE_DISABLE == ch_state) {
			hdd_err("Given ch width not supported by reg domain");
			return -EINVAL;
		}
		sap_cfg->SapHw_mode = eCSR_DOT11_MODE_abg;
	}

	sta_cnt = policy_mgr_get_mode_specific_conn_info(hdd_ctx->psoc, NULL,
							 vdev_id_list,
							 PM_STA_MODE);
	sap_cnt = policy_mgr_get_mode_specific_conn_info(hdd_ctx->psoc, NULL,
							 &vdev_id_list[sta_cnt],
							 PM_SAP_MODE);
	/* Disable NAN Disc before starting P2P GO or STA+SAP or SAP+SAP */
	if (adapter->device_mode == QDF_P2P_GO_MODE || sta_cnt ||
	    (sap_cnt > (MAX_SAP_NUM_CONCURRENCY_WITH_NAN - 1))) {
		hdd_debug("Invalid NAN concurrency. SAP: %d STA: %d P2P_GO: %d",
			  sap_cnt, sta_cnt,
			  (adapter->device_mode == QDF_P2P_GO_MODE));
		for (i = 0; i < sta_cnt + sap_cnt; i++)
			if (vdev_id_list[i] == adapter->vdev_id)
				disable_nan = false;
		if (disable_nan)
			ucfg_nan_disable_concurrency(hdd_ctx->psoc);
	}

	/* NDI + SAP conditional supported */
	hdd_sap_nan_check_and_disable_unsupported_ndi(hdd_ctx->psoc, true);

	if (policy_mgr_mode_specific_connection_count(hdd_ctx->psoc,
						      PM_NAN_DISC_MODE, NULL) &&
	    !policy_mgr_nan_sap_pre_enable_conc_check(hdd_ctx->psoc,
						      PM_SAP_MODE, freq))
		hdd_debug("NAN disabled due to concurrency constraints");

	/* check if concurrency is allowed */
	if (!policy_mgr_allow_concurrency(hdd_ctx->psoc,
				intf_pm_mode,
				freq,
				channel_width,
				policy_mgr_get_conc_ext_flags(adapter->vdev,
							      false))) {
		hdd_err("Connection failed due to concurrency check failure");
		return -EINVAL;
	}

	status = policy_mgr_reset_connection_update(hdd_ctx->psoc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("ERR: clear event failed");

	/*
	 * For Start Ap, the driver checks whether the SAP comes up in a
	 * different or same band( whether we require DBS or Not).
	 * If we dont require DBS, then the driver does nothing assuming
	 * the state would be already in non DBS mode, and just continues
	 * with vdev up on same MAC, by stopping the opportunistic timer,
	 * which results in a connection of 1x1 if already the state was in
	 * DBS. So first stop timer, and check the current hw mode.
	 * If the SAP comes up in band different from STA, DBS mode is already
	 * set. IF not, then well check for upgrade, and shift the connection
	 * back to single MAC 2x2 (if initial was 2x2).
	 */

	policy_mgr_checkn_update_hw_mode_single_mac_mode(
		hdd_ctx->psoc, freq);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to stop DBS opportunistic timer");
		return -EINVAL;
	}

	status = policy_mgr_current_connections_update(
			hdd_ctx->psoc, adapter->vdev_id,
			freq,
			POLICY_MGR_UPDATE_REASON_START_AP,
			POLICY_MGR_DEF_REQ_ID);
	if (status == QDF_STATUS_E_FAILURE) {
		hdd_err("ERROR: connections update failed!!");
		return -EINVAL;
	}

	if (QDF_STATUS_SUCCESS == status) {
		status = policy_mgr_wait_for_connection_update(hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("qdf wait for event failed!!");
			return -EINVAL;
		}
	}

	if (adapter->device_mode == QDF_P2P_GO_MODE) {
		struct hdd_adapter  *p2p_adapter;

		p2p_adapter = hdd_get_adapter(hdd_ctx, QDF_P2P_DEVICE_MODE);
		if (p2p_adapter) {
			hdd_debug("Cancel active p2p device ROC before GO starting");
			wlan_hdd_cancel_existing_remain_on_channel(
				p2p_adapter);
		}
	}

	if ((adapter->device_mode == QDF_SAP_MODE)
	    || (adapter->device_mode == QDF_P2P_GO_MODE)
	    ) {
		struct hdd_beacon_data *old, *new;
		enum nl80211_channel_type channel_type;
		struct sap_config *sap_config =
			&((WLAN_HDD_GET_AP_CTX_PTR(adapter))->sap_config);

		old = adapter->session.ap.beacon;

		if (old)
			return -EALREADY;

		status =
			wlan_hdd_cfg80211_alloc_new_beacon(adapter, &new,
							   &params->beacon,
							   params->dtim_period);

		if (status != 0) {
			hdd_err("Error!!! Allocating the new beacon");
			return -EINVAL;
		}
		adapter->session.ap.beacon = new;

		if (chandef->width < NL80211_CHAN_WIDTH_80)
			channel_type = cfg80211_get_chandef_type(
						chandef);
		else
			channel_type = NL80211_CHAN_HT40PLUS;


		wlan_hdd_set_channel(wiphy, dev,
				     chandef,
				     channel_type);

		hdd_update_beacon_rate(adapter, wiphy, params);

		/* set authentication type */
		switch (params->auth_type) {
		case NL80211_AUTHTYPE_OPEN_SYSTEM:
			adapter->session.ap.sap_config.authType =
				eSAP_OPEN_SYSTEM;
			break;
		case NL80211_AUTHTYPE_SHARED_KEY:
			adapter->session.ap.sap_config.authType =
				eSAP_SHARED_KEY;
			break;
		default:
			adapter->session.ap.sap_config.authType =
				eSAP_AUTO_SWITCH;
		}
		adapter->session.ap.sap_config.ch_width_orig =
					hdd_map_nl_chan_width(chandef->width);

		/*
		 * Enable/disable TWT responder based on
		 * the twt_responder flag
		 */
		wlan_hdd_update_twt_responder(hdd_ctx, params);

		/* Enable/disable non-srg obss pd spatial reuse */
		hdd_update_he_obss_pd(adapter, params);

		hdd_place_marker(adapter, "TRY TO START", NULL);
		status =
			wlan_hdd_cfg80211_start_bss(adapter,
				&params->beacon,
				params->ssid, params->ssid_len,
				params->hidden_ssid, true);

		if (status != 0) {
			hdd_err("Error Start bss Failed");
			goto err_start_bss;
		}
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_ID);
		if (!vdev)
			return -EINVAL;

		if (wlan_vdev_mlme_is_mlo_vdev(vdev))
			link_id = wlan_vdev_get_link_id(vdev);

		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_ID);

		if (wlan_util_get_centre_freq(wdev, link_id) !=
				params->chandef.chan->center_freq)
			params->chandef = wlan_util_get_chan_def(wdev, link_id);
		/*
		 * If Do_Not_Break_Stream enabled send avoid channel list
		 * to application.
		 */
		if (sap_config->chan_freq &&
		    policy_mgr_is_dnsc_set(adapter->vdev))
			wlan_hdd_send_avoid_freq_for_dnbs(hdd_ctx,
							  sap_config->chan_freq);

		ucfg_mlme_get_sap_inactivity_override(hdd_ctx->psoc, &val);
		if (val) {
			sta_inactivity_timer = qdf_mem_malloc(
					sizeof(*sta_inactivity_timer));
			if (!sta_inactivity_timer) {
				status = QDF_STATUS_E_FAILURE;
				goto err_start_bss;
			}
			sta_inactivity_timer->session_id = adapter->vdev_id;
			sta_inactivity_timer->sta_inactivity_timeout =
				params->inactivity_timeout;
			sme_update_sta_inactivity_timeout(hdd_ctx->mac_handle,
							  sta_inactivity_timer);
			qdf_mem_free(sta_inactivity_timer);
		}
	}

	goto success;

err_start_bss:
	hdd_place_marker(adapter, "START with FAILURE", NULL);
	if (adapter->session.ap.beacon)
		qdf_mem_free(adapter->session.ap.beacon);
	adapter->session.ap.beacon = NULL;

success:
	if (QDF_IS_STATUS_SUCCESS(status))
		hdd_place_marker(adapter, "START with SUCCESS", NULL);
	hdd_exit();
	return status;
}

/**
 * wlan_hdd_cfg80211_start_ap() - start sap
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to start ap configuration parameters
 *
 * Return: zero for success non-zero for failure
 */
int wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_ap_settings *params)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_start_ap(wiphy, dev, params);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_change_beacon() - change beacon for sofatap/p2p go
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 * @params: Pointer to change beacon parameters
 *
 * Return: 0 for success non-zero for failure
 */
static int __wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
					struct net_device *dev,
					struct cfg80211_beacon_data *params)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	struct hdd_beacon_data *old, *new;
	int status;

	hdd_enter();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_CHANGE_BEACON,
		   adapter->vdev_id, adapter->device_mode);

	hdd_debug("Device_mode %s(%d)",
		  qdf_opmode_str(adapter->device_mode), adapter->device_mode);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);

	if (0 != status)
		return status;

	if (!(adapter->device_mode == QDF_SAP_MODE ||
	      adapter->device_mode == QDF_P2P_GO_MODE)) {
		return -EOPNOTSUPP;
	}

	old = adapter->session.ap.beacon;

	if (!old) {
		hdd_err("session id: %d beacon data points to NULL",
		       adapter->vdev_id);
		return -EINVAL;
	}

	status = wlan_hdd_cfg80211_alloc_new_beacon(adapter, &new, params, 0);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("new beacon alloc failed");
		return -EINVAL;
	}

	adapter->session.ap.beacon = new;
	hdd_debug("update beacon for P2P GO/SAP");
	status = wlan_hdd_cfg80211_start_bss(adapter, params, NULL,
					0, 0, false);

	hdd_exit();
	return status;
}

/**
 * wlan_hdd_cfg80211_change_beacon() - change beacon content in sap mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to change beacon parameters
 *
 * Return: zero for success non-zero for failure
 */
int wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_beacon_data *params)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_change_beacon(wiphy, dev, params);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * hdd_sap_indicate_disconnect_for_sta() - Indicate disconnect indication
 * to supplicant, if there any clients connected to SAP interface.
 * @adapter: sap adapter context
 *
 * Return:   nothing
 */
void hdd_sap_indicate_disconnect_for_sta(struct hdd_adapter *adapter)
{
	struct sap_event sap_event;
	struct sap_context *sap_ctx;
	struct hdd_station_info *sta_info, *tmp = NULL;

	hdd_enter();

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	if (!sap_ctx) {
		hdd_err("invalid sap context");
		return;
	}

	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_SAP_INDICATE_DISCONNECT_FOR_STA) {
		hdd_debug("sta_mac: " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(sta_info->sta_mac.bytes));

		if (qdf_is_macaddr_broadcast(&sta_info->sta_mac)) {
			hdd_softap_deregister_sta(adapter, &sta_info);
			hdd_put_sta_info_ref(
				&adapter->sta_info_list,
				&sta_info, true,
				STA_INFO_SAP_INDICATE_DISCONNECT_FOR_STA);
			continue;
		}

		sap_event.sapHddEventCode = eSAP_STA_DISASSOC_EVENT;

		qdf_mem_copy(
		     &sap_event.sapevt.sapStationDisassocCompleteEvent.staMac,
		     &sta_info->sta_mac, sizeof(struct qdf_mac_addr));
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SAP_INDICATE_DISCONNECT_FOR_STA);

		sap_event.sapevt.sapStationDisassocCompleteEvent.reason =
				eSAP_MAC_INITATED_DISASSOC;
		sap_event.sapevt.sapStationDisassocCompleteEvent.status_code =
				QDF_STATUS_E_RESOURCES;
		hdd_hostapd_sap_event_cb(&sap_event, sap_ctx->user_context);
	}

	hdd_exit();
}

bool hdd_is_peer_associated(struct hdd_adapter *adapter,
			    struct qdf_mac_addr *mac_addr)
{
	bool is_associated = false;
	struct hdd_station_info *sta_info, *tmp = NULL;

	if (!adapter || !mac_addr) {
		hdd_err("Invalid adapter or mac_addr");
		return false;
	}

	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_IS_PEER_ASSOCIATED) {
		if (!qdf_mem_cmp(&sta_info->sta_mac, mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
			is_associated = true;
			hdd_put_sta_info_ref(&adapter->sta_info_list,
					     &sta_info, true,
					     STA_INFO_IS_PEER_ASSOCIATED);
			if (tmp)
				hdd_put_sta_info_ref(
						&adapter->sta_info_list,
						&tmp, true,
						STA_INFO_IS_PEER_ASSOCIATED);
			break;
		}
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_IS_PEER_ASSOCIATED);
	}

	return is_associated;
}

#ifdef WLAN_FEATURE_SAP_ACS_OPTIMIZE
bool hdd_sap_is_acs_in_progress(struct wlan_objmgr_vdev *vdev)
{
	struct hdd_adapter *adapter;
	bool in_progress = false;

	if (!vdev) {
		hdd_err("vdev is NULL");
		return in_progress;
	}

	adapter = wlan_hdd_get_adapter_from_objmgr(vdev);
	if (!adapter) {
		hdd_err("null adapter");
		return in_progress;
	}

	if (!hdd_adapter_is_ap(adapter)) {
		hdd_err("vdev id %d is not AP", adapter->vdev_id);
		return in_progress;
	}

	in_progress = qdf_atomic_read(&adapter->session.ap.acs_in_progress);

	return in_progress;
}
#endif
