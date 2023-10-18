/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_nan_datapath.c
 *
 * WLAN Host Device Driver nan datapath API implementation
 */
#include <wlan_hdd_includes.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include "wlan_hdd_includes.h"
#include "wlan_hdd_p2p.h"
#include "osif_sync.h"
#include "wma_api.h"
#include "wlan_hdd_assoc.h"
#include "sme_nan_datapath.h"
#include "wlan_hdd_object_manager.h"
#include <qca_vendor.h>
#include "os_if_nan.h"
#include "wlan_nan_api.h"
#include "nan_public_structs.h"
#include "cfg_nan_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "qdf_util.h"
#include "qdf_net_if.h"
#include <cdp_txrx_misc.h>
#include "wlan_fwol_ucfg_api.h"
#include "wlan_dp_ucfg_api.h"

/**
 * hdd_nan_datapath_target_config() - Configure NAN datapath features
 * @hdd_ctx: Pointer to HDD context
 * @tgt_cfg: Pointer to target device capability information
 *
 * NAN datapath functionality is enabled if it is enabled in
 * .ini file and also supported on target device.
 *
 * Return: None
 */
void hdd_nan_datapath_target_config(struct hdd_context *hdd_ctx,
					struct wma_tgt_cfg *tgt_cfg)
{
	hdd_ctx->nan_datapath_enabled =
			cfg_nan_get_datapath_enable(hdd_ctx->psoc) &&
			tgt_cfg->nan_datapath_enabled;
	hdd_debug("NAN Datapath Enable: %d (Host: %d FW: %d)",
		  hdd_ctx->nan_datapath_enabled,
		  cfg_nan_get_datapath_enable(hdd_ctx->psoc),
		  tgt_cfg->nan_datapath_enabled);
}

/**
 * hdd_close_ndi() - close NAN Data interface
 * @adapter: adapter context
 *
 * Close the adapter if start BSS fails
 *
 * Returns: 0 on success, negative error code otherwise
 */
static int hdd_close_ndi(struct hdd_adapter *adapter)
{
	int errno;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	hdd_enter();

	/* check if the adapter is in NAN Data mode */
	if (QDF_NDI_MODE != adapter->device_mode) {
		hdd_err("Interface is not in NDI mode");
		return -EINVAL;
	}
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

#ifdef WLAN_OPEN_SOURCE
	cancel_work_sync(&adapter->ipv4_notifier_work);
#endif
	hdd_deregister_hl_netdev_fc_timer(adapter);
	hdd_deregister_tx_flow_control(adapter);

#ifdef WLAN_NS_OFFLOAD
#ifdef WLAN_OPEN_SOURCE
	cancel_work_sync(&adapter->ipv6_notifier_work);
#endif
#endif
	errno = hdd_vdev_destroy(adapter);
	if (errno)
		hdd_err("failed to destroy vdev: %d", errno);

	adapter->is_virtual_iface = true;
	/* We are good to close the adapter */
	hdd_close_adapter(hdd_ctx, adapter, true);

	hdd_exit();
	return 0;
}

/**
 * hdd_is_ndp_allowed() - Indicates if NDP is allowed
 * @hdd_ctx: hdd context
 *
 * NDP is not allowed with any other role active except STA.
 *
 * Return:  true if allowed, false otherwise
 */
#ifdef NDP_SAP_CONCURRENCY_ENABLE
static bool hdd_is_ndp_allowed(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	struct hdd_station_ctx *sta_ctx;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IS_NDP_ALLOWED;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		switch (adapter->device_mode) {
		case QDF_P2P_GO_MODE:
			if (test_bit(SOFTAP_BSS_STARTED,
				     &adapter->event_flags)) {
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return false;
			}
			break;
		case QDF_P2P_CLIENT_MODE:
			sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			if (hdd_cm_is_vdev_associated(adapter) ||
			    hdd_cm_is_connecting(adapter)) {
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return false;
			}
			break;
		default:
			break;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return true;
}
#else
static bool hdd_is_ndp_allowed(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	struct hdd_station_ctx *sta_ctx;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IS_NDP_ALLOWED;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		switch (adapter->device_mode) {
		case QDF_P2P_GO_MODE:
		case QDF_SAP_MODE:
			if (test_bit(SOFTAP_BSS_STARTED,
				     &adapter->event_flags)) {
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return false;
			}
			break;
		case QDF_P2P_CLIENT_MODE:
			sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			if (hdd_cm_is_vdev_associated(adapter) ||
			    hdd_cm_is_connecting(adapter)) {
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return false;
			}
			break;
		default:
			break;
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return true;
}
#endif /* NDP_SAP_CONCURRENCY_ENABLE */

/**
 * hdd_ndi_select_valid_freq() - Find the valid freq for NDI start
 * @hdd_ctx: hdd context
 * @freq: pointer to freq, give preference to 5745, 5220 and 2437 to keep the
 * legacy behavior intact
 *
 * Unlike traditional device modes, where the higher application
 * layer initiates connect / join / start, the NAN data
 * interface does not have any such formal requests. The NDI
 * create request is responsible for starting the BSS as well.
 * Use the 5GHz Band NAN Social channel for BSS start if target
 * supports it, since a 2.4GHz channel will require a DBS HW mode change
 * first on a DBS 2x2 MAC target. Use a 2.4 GHz Band NAN Social channel
 * if the target is not 5GHz capable. If both of these channels are
 * not available, pick the next available channel. This would be used just to
 * start the NDI. Actual channel for NDP data transfer would be negotiated with
 * peer later.
 *
 * Return: SUCCESS if valid channel is obtained
 */
static QDF_STATUS hdd_ndi_select_valid_freq(struct hdd_context *hdd_ctx,
					    uint32_t *freq)
{
	static const qdf_freq_t valid_freq[] = {NAN_SOCIAL_FREQ_5GHZ_UPPER_BAND,
						NAN_SOCIAL_FREQ_5GHZ_LOWER_BAND,
						NAN_SOCIAL_FREQ_2_4GHZ};
	uint8_t i;
	struct regulatory_channel *cur_chan_list;
	QDF_STATUS status;

	for (i = 0; i < ARRAY_SIZE(valid_freq); i++) {
		if (wlan_reg_is_freq_enabled(hdd_ctx->pdev, valid_freq[i],
					     REG_CURRENT_PWR_MODE)) {
			*freq = valid_freq[i];
			return QDF_STATUS_SUCCESS;
		}
	}

	cur_chan_list = qdf_mem_malloc(sizeof(*cur_chan_list) *
							(NUM_CHANNELS + 2));
	if (!cur_chan_list)
		return QDF_STATUS_E_NOMEM;

	status = ucfg_reg_get_current_chan_list(hdd_ctx->pdev, cur_chan_list);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("Failed to get the current channel list");
		qdf_mem_free(cur_chan_list);
		return QDF_STATUS_E_IO;
	}

	for (i = 0; i < NUM_CHANNELS; i++) {
		/*
		 * current channel list includes all channels. Exclude
		 * disabled channels
		 */
		if (cur_chan_list[i].chan_flags & REGULATORY_CHAN_DISABLED ||
		    cur_chan_list[i].chan_flags & REGULATORY_CHAN_RADAR)
			continue;
		/*
		 * do not include 6 GHz channels for now as NAN would need
		 * 2.4 GHz and 5 GHz channels for discovery.
		 * <TODO> Need to consider the 6GHz channels when there is a
		 * case where all 2GHz and 5GHz channels are disabled and
		 * only 6GHz channels are enabled
		 */
		if (wlan_reg_is_6ghz_chan_freq(cur_chan_list[i].center_freq))
			continue;

		/* extracting first valid channel from regulatory list */
		if (wlan_reg_is_freq_enabled(hdd_ctx->pdev,
					     cur_chan_list[i].center_freq,
					     REG_CURRENT_PWR_MODE)) {
			*freq = cur_chan_list[i].center_freq;
			qdf_mem_free(cur_chan_list);
			return QDF_STATUS_SUCCESS;
		}
	}

	qdf_mem_free(cur_chan_list);

	return QDF_STATUS_E_FAILURE;
}

/**
 * hdd_ndi_start_bss() - Start BSS on NAN data interface
 * @adapter: adapter context
 *
 * Return: 0 on success, error value on failure
 */
static int hdd_ndi_start_bss(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct bss_dot11_config dot11_cfg = {0};
	struct start_bss_config ndi_bss_cfg = {0};
	qdf_freq_t valid_freq = 0;
	mac_handle_t mac_handle = hdd_adapter_get_mac_handle(adapter);
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct hdd_context *hdd_ctx;

	hdd_enter();
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	status = hdd_ndi_select_valid_freq(hdd_ctx, &valid_freq);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Unable to retrieve channel list for NAN");
		return -EINVAL;
	}

	dot11_cfg.vdev_id = adapter->vdev_id;
	dot11_cfg.bss_op_ch_freq = valid_freq;
	dot11_cfg.phy_mode = eCSR_DOT11_MODE_AUTO;
	if (!wlan_vdev_id_is_open_cipher(mac->pdev, adapter->vdev_id))
		dot11_cfg.privacy = 1;

	sme_get_network_params(mac, &dot11_cfg);
	ndi_bss_cfg.vdev_id = adapter->vdev_id;
	ndi_bss_cfg.oper_ch_freq = dot11_cfg.bss_op_ch_freq;
	ndi_bss_cfg.nwType = dot11_cfg.nw_type;
	ndi_bss_cfg.dot11mode = dot11_cfg.dot11_mode;

	if (dot11_cfg.opr_rates.numRates) {
		qdf_mem_copy(ndi_bss_cfg.operationalRateSet.rate,
			     dot11_cfg.opr_rates.rate,
			     dot11_cfg.opr_rates.numRates);
		ndi_bss_cfg.operationalRateSet.numRates =
					dot11_cfg.opr_rates.numRates;
	}

	if (dot11_cfg.ext_rates.numRates) {
		qdf_mem_copy(ndi_bss_cfg.extendedRateSet.rate,
			     dot11_cfg.ext_rates.rate,
			     dot11_cfg.ext_rates.numRates);
		ndi_bss_cfg.extendedRateSet.numRates =
				dot11_cfg.ext_rates.numRates;
	}

	status = sme_start_bss(mac_handle, adapter->vdev_id,
			       &ndi_bss_cfg);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("NDI sme_RoamConnect session %d failed with status %d -> NotConnected",
			adapter->vdev_id, status);
		/* change back to NotConnected */
		hdd_conn_set_connection_state(adapter,
					      eConnectionState_NotConnected);
	}

	hdd_exit();

	return 0;
}

/**
 * hdd_get_random_nan_mac_addr() - generate random non pre-existent mac address
 * @hdd_ctx: hdd context pointer
 * @mac_addr: mac address buffer to populate
 *
 * Return: status of operation
 */
static int hdd_get_random_nan_mac_addr(struct hdd_context *hdd_ctx,
				       struct qdf_mac_addr *mac_addr)
{
	struct hdd_adapter *adapter;
	uint8_t pos, bit_pos, byte_pos, mask;
	uint8_t i, attempts, max_attempt = 16;
	bool found;

	for (attempts = 0; attempts < max_attempt; attempts++) {
		found = false;
		/* if NDI is present next addr is required to be 1 bit apart  */
		adapter = hdd_get_adapter(hdd_ctx, QDF_NDI_MODE);
		if (adapter) {
			hdd_debug("NDI already exists, deriving next mac");
			qdf_mem_copy(mac_addr, &adapter->mac_addr,
				     sizeof(*mac_addr));
			qdf_get_random_bytes(&pos, sizeof(pos));
			/* skipping byte 0, 5 leaves 8*4=32 positions */
			pos = pos % 32;
			bit_pos = pos % 8;
			byte_pos = pos / 8;
			mask = 1 << bit_pos;
			/* flip the required bit */
			mac_addr->bytes[byte_pos + 1] ^= mask;
		} else {
			qdf_get_random_bytes(mac_addr, sizeof(*mac_addr));
			/*
			 * Reset multicast bit (bit-0) and set
			 * locally-administered bit
			 */
			mac_addr->bytes[0] = 0x2;

			/*
			 * to avoid potential conflict with FW's generated NMI
			 * mac addr, host sets LSB if 6th byte to 0
			 */
			mac_addr->bytes[5] &= 0xFE;
		}
		for (i = 0; i < hdd_ctx->num_provisioned_addr; i++) {
			if ((!qdf_mem_cmp(hdd_ctx->
					  provisioned_mac_addr[i].bytes,
			      mac_addr, sizeof(*mac_addr)))) {
				found = true;
				break;
			}
		}

		if (found)
			continue;

		for (i = 0; i < hdd_ctx->num_derived_addr; i++) {
			if ((!qdf_mem_cmp(hdd_ctx->
					  derived_mac_addr[i].bytes,
			      mac_addr, sizeof(*mac_addr)))) {
				found = true;
				break;
			}
		}
		if (found)
			continue;

		adapter = hdd_get_adapter_by_macaddr(hdd_ctx, mac_addr->bytes);
		if (!adapter)
			return 0;
	}

	hdd_err("unable to get non-pre-existing mac address in %d attempts",
		max_attempt);

	return -EINVAL;
}

void hdd_ndp_event_handler(struct hdd_adapter *adapter,
			   struct csr_roam_info *roam_info,
			   eRoamCmdStatus roam_status,
			   eCsrRoamResult roam_result)
{
	bool success;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_NAN_ID);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_NAN_ID);

	if (roam_status == eCSR_ROAM_NDP_STATUS_UPDATE) {
		switch (roam_result) {
		case eCSR_ROAM_RESULT_NDI_CREATE_RSP:
			success = (roam_info->ndp.ndi_create_params.status ==
					NAN_DATAPATH_RSP_STATUS_SUCCESS);
			hdd_debug("posting ndi create status: %d (%s) to umac",
				  success, success ? "Success" : "Failure");
			os_if_nan_post_ndi_create_rsp(psoc, adapter->vdev_id,
							success);
			return;
		case eCSR_ROAM_RESULT_NDI_DELETE_RSP:
			success = (roam_info->ndp.ndi_create_params.status ==
					NAN_DATAPATH_RSP_STATUS_SUCCESS);
			hdd_debug("posting ndi delete status: %d (%s) to umac",
				  success, success ? "Success" : "Failure");
			os_if_nan_post_ndi_delete_rsp(psoc, adapter->vdev_id,
							success);
			return;
		default:
			hdd_err("in correct roam_result: %d", roam_result);
			return;
		}
	} else {
		hdd_err("in correct roam_status: %d", roam_status);
		return;
	}
}

/**
 * __wlan_hdd_cfg80211_process_ndp_cmd() - handle NDP request
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function is invoked to handle vendor command
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	int ret_val;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err_rl("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (!WLAN_HDD_IS_NDP_ENABLED(hdd_ctx)) {
		hdd_debug_rl("NAN datapath is not enabled");
		return -EPERM;
	}

	return os_if_nan_process_ndp_cmd(hdd_ctx->psoc, data, data_len,
					 hdd_is_ndp_allowed(hdd_ctx), wdev);
}

/**
 * wlan_hdd_cfg80211_process_ndp_cmd() - handle NDP request
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function is called to send a NAN request to
 * firmware. This is an SSR-protected wrapper function.
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len)
{
	/* This call is intentionally not protected by op_start/op_stop, due to
	 * the various protection needs of the callbacks dispatched within.
	 */
	return __wlan_hdd_cfg80211_process_ndp_cmd(wiphy, wdev,
						   data, data_len);
}

static int update_ndi_state(struct hdd_adapter *adapter, uint32_t state)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_NAN_ID);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}
	status = os_if_nan_set_ndi_state(vdev, state);

	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_NAN_ID);
	return status;
}

/**
 * hdd_init_nan_data_mode() - initialize nan data mode
 * @adapter: adapter context
 *
 * Returns: 0 on success negative error code on error
 */
int hdd_init_nan_data_mode(struct hdd_adapter *adapter)
{
	struct net_device *wlan_dev = adapter->dev;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;
	int32_t ret_val;
	mac_handle_t mac_handle;
	bool bval = false;
	uint8_t enable_sifs_burst = 0;
	struct wlan_objmgr_vdev *vdev;
	uint16_t rts_profile = 0;

	ret_val = hdd_vdev_create(adapter);
	if (ret_val) {
		hdd_err("failed to create vdev: %d", ret_val);
		return ret_val;
	}

	mac_handle = hdd_ctx->mac_handle;

	/* Configure self HT/VHT capabilities */
	status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &bval);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get vht_enable2x2");

	sme_set_pdev_ht_vht_ies(mac_handle, bval);
	sme_set_vdev_ies_per_band(mac_handle, adapter->vdev_id,
				  adapter->device_mode);

	hdd_roam_profile_init(adapter);
	hdd_register_wext(wlan_dev);

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
	if (!vdev) {
		ret_val = -EAGAIN;
		goto error_init_txrx;
	}

	status = ucfg_dp_init_txrx(vdev);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		hdd_err("ucfg_dp_init_tx_rx() init failed, status %d", status);
		ret_val = -EAGAIN;
		goto error_init_txrx;
	}

	set_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);

	status = hdd_wmm_adapter_init(adapter);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("hdd_wmm_adapter_init() failed, status %d", status);
		ret_val = -EAGAIN;
		goto error_wmm_init;
	}

	set_bit(WMM_INIT_DONE, &adapter->event_flags);

	/* ENABLE SIFS BURST */
	status = ucfg_get_enable_sifs_burst(hdd_ctx->psoc, &enable_sifs_burst);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to get sifs burst value, use default");

	ret_val = wma_cli_set_command((int)adapter->vdev_id,
				      (int)wmi_pdev_param_burst_enable,
				      enable_sifs_burst,
				      PDEV_CMD);
	if (0 != ret_val)
		hdd_err("wmi_pdev_param_burst_enable set failed %d", ret_val);

	/* RTS CTS PARAM  */
	status = ucfg_fwol_get_rts_profile(hdd_ctx->psoc, &rts_profile);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("FAILED TO GET RTSCTS Profile status:%d", status);

	ret_val = sme_cli_set_command(adapter->vdev_id,
				      wmi_vdev_param_enable_rtscts, rts_profile,
				      VDEV_CMD);
	if (ret_val)
		hdd_err("FAILED TO SET RTSCTS Profile ret:%d", ret_val);

	hdd_set_netdev_flags(adapter);

	update_ndi_state(adapter, NAN_DATA_NDI_CREATING_STATE);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	return ret_val;

error_wmm_init:
	clear_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);
	ucfg_dp_deinit_txrx(vdev);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);

error_init_txrx:
	hdd_wext_unregister(wlan_dev, true);

	QDF_BUG(!hdd_vdev_destroy(adapter));

	return ret_val;
}

/**
 * hdd_is_max_ndi_count_reached() - Check the NDI max limit
 * @hdd_ctx: Pointer to HDD context
 *
 * This function does not allow to create more than ndi_max_support
 *
 * Return: false if max value not reached otherwise true
 */
static bool hdd_is_max_ndi_count_reached(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	uint8_t ndi_adapter_count = 0;
	QDF_STATUS status;
	uint32_t max_ndi;

	if (!hdd_ctx)
		return true;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_NDI_OPEN) {
		if (WLAN_HDD_IS_NDI(adapter))
			ndi_adapter_count++;
		hdd_adapter_dev_put_debug(adapter, NET_DEV_HOLD_NDI_OPEN);
	}

	status = cfg_nan_get_max_ndi(hdd_ctx->psoc, &max_ndi);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err(" Unable to fetch Max NDI");
		return true;
	}

	if (ndi_adapter_count >= max_ndi) {
		hdd_err("Can't allow more than %d NDI adapters",
			max_ndi);
		return true;
	}

	return false;
}

int hdd_ndi_open(const char *iface_name, bool is_add_virtual_iface)
{
	struct hdd_adapter *adapter;
	struct qdf_mac_addr random_ndi_mac;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	uint8_t *ndi_mac_addr;
	struct hdd_adapter_create_param params = {0};

	hdd_enter();

	if (hdd_is_max_ndi_count_reached(hdd_ctx))
		return -EINVAL;

	params.is_add_virtual_iface = is_add_virtual_iface;

	hdd_debug("is_add_virtual_iface %d", is_add_virtual_iface);

	if (cfg_nan_get_ndi_mac_randomize(hdd_ctx->psoc)) {
		if (hdd_get_random_nan_mac_addr(hdd_ctx, &random_ndi_mac)) {
			hdd_err("get random mac address failed");
			return -EFAULT;
		}
		ndi_mac_addr = &random_ndi_mac.bytes[0];
	} else {
		ndi_mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_NDI_MODE);
		if (!ndi_mac_addr) {
			hdd_err("get intf address failed");
			return -EFAULT;
		}
	}

	params.is_add_virtual_iface = 1;
	adapter = hdd_open_adapter(hdd_ctx, QDF_NDI_MODE, iface_name,
				   ndi_mac_addr, NET_NAME_UNKNOWN, true,
				   &params);
	if (!adapter) {
		if (!cfg_nan_get_ndi_mac_randomize(hdd_ctx->psoc))
			wlan_hdd_release_intf_addr(hdd_ctx, ndi_mac_addr);
		hdd_err("hdd_open_adapter failed");
		return -EINVAL;
	}

	hdd_exit();
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
int hdd_ndi_set_mode(const char *iface_name)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct qdf_mac_addr random_ndi_mac;
	uint8_t *ndi_mac_addr = NULL;

	hdd_enter();
	if (hdd_is_max_ndi_count_reached(hdd_ctx))
		return -EINVAL;

	adapter = hdd_get_adapter_by_iface_name(hdd_ctx, iface_name);
	if (!adapter) {
		hdd_err("adapter is null");
		return -EINVAL;
	}

	if (cfg_nan_get_ndi_mac_randomize(hdd_ctx->psoc)) {
		if (hdd_get_random_nan_mac_addr(hdd_ctx, &random_ndi_mac)) {
			hdd_err("get random mac address failed");
			return -EFAULT;
		}
		ndi_mac_addr = &random_ndi_mac.bytes[0];
		ucfg_dp_update_inf_mac(hdd_ctx->psoc, &adapter->mac_addr,
				       (struct qdf_mac_addr *)ndi_mac_addr);
		hdd_update_dynamic_mac(hdd_ctx, &adapter->mac_addr,
				       (struct qdf_mac_addr *)ndi_mac_addr);
		qdf_mem_copy(&adapter->mac_addr, ndi_mac_addr, ETH_ALEN);
		qdf_net_update_net_device_dev_addr(adapter->dev,
						   ndi_mac_addr, ETH_ALEN);
	}

	adapter->device_mode = QDF_NDI_MODE;
	hdd_debug("Created NDI with device mode:%d and iface_name:%s",
		  adapter->device_mode, iface_name);

	return 0;
}
#endif

int hdd_ndi_start(const char *iface_name, uint16_t transaction_id)
{
	int ret;
	QDF_STATUS status;
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();
	if (!hdd_ctx)
		return -EINVAL;

	adapter = hdd_get_adapter_by_iface_name(hdd_ctx, iface_name);
	if (!adapter) {
		hdd_err("adapter is null");
		return -EINVAL;
	}

	/* create nan vdev */
	status = hdd_init_nan_data_mode(adapter);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("failed to init nan data intf, status :%d", status);
		ret = -EFAULT;
		goto err_handler;
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_NAN_ID);
	if (!vdev) {
		hdd_err("vdev is NULL");
		ret = -EINVAL;
		goto err_handler;
	}
	/*
	 * Create transaction id is required to be saved since the firmware
	 * does not honor the transaction id for create request
	 */
	ucfg_nan_set_ndp_create_transaction_id(vdev, transaction_id);
	ucfg_nan_set_ndi_state(vdev, NAN_DATA_NDI_CREATING_STATE);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_NAN_ID);

	if (hdd_ndi_start_bss(adapter)) {
		hdd_err("NDI start bss failed");
		ret = -EFAULT;
		goto err_handler;
	}

	hdd_exit();
	return 0;

err_handler:

	/* Start BSS failed, delete the interface */
	hdd_close_ndi(adapter);
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0))
static int hdd_delete_ndi_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_context *hdd_ctx = (struct hdd_context *)wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	hdd_enter_dev(dev);

	wlan_hdd_release_intf_addr(hdd_ctx,
				   adapter->mac_addr.bytes);
	hdd_stop_adapter(hdd_ctx, adapter);
	hdd_deinit_adapter(hdd_ctx, adapter, true);

	hdd_exit();

	return 0;
}
#else
static int hdd_delete_ndi_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	int ret;

	ret = __wlan_hdd_del_virtual_intf(wiphy, wdev);

	if (ret)
		hdd_err("NDI delete request failed");
	else
		hdd_err("NDI delete request successfully issued");

	return ret;
}
#endif

int hdd_ndi_delete(uint8_t vdev_id, const char *iface_name,
		   uint16_t transaction_id)
{
	int ret;
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct wlan_objmgr_vdev *vdev;

	if (!hdd_ctx)
		return -EINVAL;

	/* check if adapter by vdev_id is valid NDI */
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter || !WLAN_HDD_IS_NDI(adapter)) {
		hdd_err("NAN data interface %s is not available", iface_name);
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!sta_ctx) {
		hdd_err("sta_ctx is NULL");
		return -EINVAL;
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_NAN_ID);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return -EINVAL;
	}

	os_if_nan_set_ndp_delete_transaction_id(vdev, transaction_id);
	os_if_nan_set_ndi_state(vdev, NAN_DATA_NDI_DELETING_STATE);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_NAN_ID);
	/* Delete the interface */
	adapter->is_virtual_iface = true;
	ret = hdd_delete_ndi_intf(hdd_ctx->wiphy, &adapter->wdev);

	return ret;
}

#define MAX_VDEV_NDP_PARAMS 2
/* params being sent:
 * wmi_vdev_param_ndp_inactivity_timeout
 * wmi_vdev_param_ndp_keepalive_timeout
 */

void hdd_ndi_drv_ndi_create_rsp_handler(uint8_t vdev_id,
				struct nan_datapath_inf_create_rsp *ndi_rsp)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;
	struct csr_roam_info *roam_info;
	uint16_t ndp_inactivity_timeout = 0;
	uint16_t ndp_keep_alive_period;
	struct qdf_mac_addr bc_mac_addr = QDF_MAC_ADDR_BCAST_INIT;
	struct wlan_objmgr_vdev *vdev;
	struct dev_set_param setparam[MAX_VDEV_NDP_PARAMS] = {};
	uint8_t index = 0;
	QDF_STATUS status;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!sta_ctx) {
		hdd_err("sta_ctx is null");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;

	if (ndi_rsp->status == QDF_STATUS_SUCCESS) {
		hdd_alert("NDI interface successfully created");
		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_NAN_ID);
		if (!vdev) {
			qdf_mem_free(roam_info);
			hdd_err("vdev is NULL");
			return;
		}

		os_if_nan_set_ndp_create_transaction_id(vdev, 0);
		os_if_nan_set_ndi_state(vdev, NAN_DATA_NDI_CREATED_STATE);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_NAN_ID);

		wlan_hdd_netif_queue_control(adapter,
					WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);

		if (QDF_IS_STATUS_ERROR(cfg_nan_get_ndp_inactivity_timeout(
		    hdd_ctx->psoc, &ndp_inactivity_timeout)))
			hdd_err("Failed to fetch inactivity timeout value");
		status = mlme_check_index_setparam(
					setparam,
					wmi_vdev_param_ndp_inactivity_timeout,
					ndp_inactivity_timeout, index++,
					MAX_VDEV_NDP_PARAMS);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("failed at wmi_vdev_param_ndp_inactivity_timeout");
			goto error;
		}

		if (QDF_IS_STATUS_SUCCESS(cfg_nan_get_ndp_keepalive_period(
						hdd_ctx->psoc,
						&ndp_keep_alive_period))) {
			status = mlme_check_index_setparam(
					setparam,
					wmi_vdev_param_ndp_keepalive_timeout,
					ndp_keep_alive_period, index++,
					MAX_VDEV_NDP_PARAMS);
			if (QDF_IS_STATUS_ERROR(status)) {
				hdd_err("failed at wmi_vdev_param_ndp_keepalive_timeout");
				goto error;
			}
		}
		status = sme_send_multi_pdev_vdev_set_params(MLME_VDEV_SETPARAM,
							     adapter->vdev_id,
							     setparam,
							     index);
		if (QDF_IS_STATUS_ERROR(status))
			hdd_err("failed to send vdev set params");
	} else {
		hdd_alert("NDI interface creation failed with reason %d",
			ndi_rsp->reason /* create_reason */);
	}

	hdd_save_peer(sta_ctx, &bc_mac_addr);
	qdf_copy_macaddr(&roam_info->bssid, &bc_mac_addr);
	hdd_roam_register_sta(adapter, &roam_info->bssid,
			      roam_info->fAuthRequired);

error:
	qdf_mem_free(roam_info);
}

void hdd_ndi_close(uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return;
	}

	adapter->is_virtual_iface = true;
	hdd_close_ndi(adapter);
}

void hdd_ndi_drv_ndi_delete_rsp_handler(uint8_t vdev_id)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;
	struct qdf_mac_addr bc_mac_addr = QDF_MAC_ADDR_BCAST_INIT;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!sta_ctx) {
		hdd_err("sta_ctx is null");
		return;
	}

	hdd_delete_peer(sta_ctx, &bc_mac_addr);

	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	/*
	 * For NAN Data interface, the close session results in the final
	 * indication to the userspace
	 */
	if (adapter->device_mode == QDF_NDI_MODE)
		hdd_ndp_session_end_handler(adapter);

	complete(&adapter->disconnect_comp_var);
}

void hdd_ndp_session_end_handler(struct hdd_adapter *adapter)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_OSIF_NAN_ID);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	os_if_nan_ndi_session_end(vdev);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_NAN_ID);
}

/**
 * hdd_send_obss_scan_req() - send OBSS scan request to SME layer.
 * @hdd_ctx: hdd context pointer
 * @val: true if new NDP peer is added and false when last peer NDP is deleted.
 *
 * Return: void
 */
static void hdd_send_obss_scan_req(struct hdd_context *hdd_ctx, bool val)
{
	QDF_STATUS status;
	uint32_t sta_vdev_id = 0;

	status = hdd_get_first_connected_sta_vdev_id(hdd_ctx, &sta_vdev_id);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		hdd_debug("reconfig OBSS scan param: %d", val);
		sme_reconfig_obss_scan_param(hdd_ctx->mac_handle, sta_vdev_id,
					     val);
	} else {
		hdd_debug("Connected STA not found");
	}
}

int hdd_ndp_new_peer_handler(uint8_t vdev_id, uint16_t sta_id,
			struct qdf_mac_addr *peer_mac_addr, bool first_peer)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;
	struct csr_roam_info *roam_info;
	struct wlan_objmgr_vdev *vdev;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return -EINVAL;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!sta_ctx) {
		hdd_err("sta_ctx is null");
		return -EINVAL;
	}

	/* save peer in ndp ctx */
	if (!hdd_save_peer(sta_ctx, peer_mac_addr)) {
		hdd_err("Ndp peer table full. cannot save new peer");
		return -EPERM;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return -ENOMEM;
	qdf_copy_macaddr(&roam_info->bssid, peer_mac_addr);

	/* this function is called for each new peer */
	hdd_roam_register_sta(adapter, &roam_info->bssid,
			      roam_info->fAuthRequired);

	/* perform following steps for first new peer ind */
	if (first_peer) {
		hdd_debug("Set ctx connection state to connected");
		/* Disable LRO/GRO for NDI Mode */
		if (ucfg_dp_is_ol_enabled(hdd_ctx->psoc) &&
		    !NAN_CONCURRENCY_SUPPORTED(hdd_ctx->psoc)) {
			hdd_debug("Disable LRO/GRO in NDI Mode");
			hdd_disable_rx_ol_in_concurrency(true);
		}

		vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
		if (vdev) {
			ucfg_dp_bus_bw_compute_prev_txrx_stats(vdev);
			hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
		}
		ucfg_dp_bus_bw_compute_timer_start(hdd_ctx->psoc);
		sta_ctx->conn_info.conn_state = eConnectionState_NdiConnected;
		hdd_wmm_connect(adapter, roam_info, eCSR_BSS_TYPE_NDI);
		wlan_hdd_netif_queue_control(
					adapter,
					WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);
		/*
		 * This is called only for first peer. So, no.of NDP sessions
		 * are always 1
		 */
		if (!NDI_CONCURRENCY_SUPPORTED(hdd_ctx->psoc))
			hdd_indicate_active_ndp_cnt(hdd_ctx->psoc, vdev_id, 1);
		hdd_send_obss_scan_req(hdd_ctx, true);

		wlan_twt_concurrency_update(hdd_ctx);
	}
	qdf_mem_free(roam_info);
	return 0;
}

void hdd_cleanup_ndi(struct hdd_context *hdd_ctx,
		     struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct wlan_objmgr_vdev *vdev;

	if (sta_ctx->conn_info.conn_state != eConnectionState_NdiConnected) {
		hdd_debug("NDI has no NDPs");
		return;
	}
	sta_ctx->conn_info.conn_state = eConnectionState_NdiDisconnected;
	hdd_conn_set_connection_state(adapter,
		eConnectionState_NdiDisconnected);
	hdd_debug("Stop netif tx queues.");
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);
	vdev = hdd_objmgr_get_vdev_by_user(adapter, WLAN_DP_ID);
	if (vdev) {
		ucfg_dp_bus_bw_compute_reset_prev_txrx_stats(vdev);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}
	ucfg_dp_bus_bw_compute_timer_try_stop(hdd_ctx->psoc);
	if ((ucfg_dp_is_ol_enabled(hdd_ctx->psoc) &&
	     !NAN_CONCURRENCY_SUPPORTED(hdd_ctx->psoc)) &&
	    ((policy_mgr_get_connection_count(hdd_ctx->psoc) == 0) ||
	     ((policy_mgr_get_connection_count(hdd_ctx->psoc) == 1) &&
	      (policy_mgr_mode_specific_connection_count(
						hdd_ctx->psoc,
						PM_STA_MODE,
						NULL) == 1)))) {
		hdd_debug("Enable LRO/GRO");
		ucfg_dp_rx_handle_concurrency(hdd_ctx->psoc, false);
	}
}

void hdd_ndp_peer_departed_handler(uint8_t vdev_id, uint16_t sta_id,
			struct qdf_mac_addr *peer_mac_addr, bool last_peer)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct hdd_station_ctx *sta_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("adapter is null");
		return;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!sta_ctx) {
		hdd_err("sta_ctx is null");
		return;
	}

	hdd_delete_peer(sta_ctx, peer_mac_addr);

	if (last_peer) {
		hdd_debug("No more ndp peers.");
		ucfg_nan_clear_peer_mc_list(hdd_ctx->psoc, adapter->vdev,
					    peer_mac_addr);
		hdd_cleanup_ndi(hdd_ctx, adapter);
		qdf_event_set(&adapter->peer_cleanup_done);
		/*
		 * This is called only for last peer. So, no.of NDP sessions
		 * are always 0
		 */
		if (!NDI_CONCURRENCY_SUPPORTED(hdd_ctx->psoc))
			hdd_indicate_active_ndp_cnt(hdd_ctx->psoc, vdev_id, 0);
		hdd_send_obss_scan_req(hdd_ctx, false);

		wlan_twt_concurrency_update(hdd_ctx);
	}
}
