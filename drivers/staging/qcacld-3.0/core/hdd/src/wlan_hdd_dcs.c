/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_dcs.c
 *
 * DCS implementation.
 *
 */

#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_dcs.h>
#include <wlan_hdd_includes.h>
#include <wlan_dcs_ucfg_api.h>
#include <wlan_dlm_ucfg_api.h>
#include <wlan_osif_priv.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_dcs_ucfg_api.h>

/* Time(in milliseconds) before which the AP doesn't expect a connection */
#define HDD_DCS_AWGN_BSS_RETRY_DELAY (5 * 60 * 1000)

/**
 * hdd_dcs_add_bssid_to_reject_list() - add bssid to reject list
 * @pdev: pdev ptr
 * @bssid: bssid to be added
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
hdd_dcs_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				 struct qdf_mac_addr *bssid)
{
	struct reject_ap_info ap_info;

	qdf_mem_zero(&ap_info, sizeof(struct reject_ap_info));
	qdf_copy_macaddr(&ap_info.bssid, bssid);
	/* set retry_delay to reject new connect requests */
	ap_info.rssi_reject_params.retry_delay =
		HDD_DCS_AWGN_BSS_RETRY_DELAY;
	ap_info.reject_ap_type = DRIVER_RSSI_REJECT_TYPE;
	ap_info.reject_reason = REASON_STA_KICKOUT;
	ap_info.source = ADDED_BY_DRIVER;
	return ucfg_dlm_add_bssid_to_reject_list(pdev, &ap_info);
}

/**
 * hdd_dcs_switch_chan_cb() - hdd dcs switch channel callback
 * @vdev: vdev ptr
 * @tgt_freq: target channel frequency
 * @tgt_width: target channel width
 *
 * This callback is registered with dcs component to trigger channel switch.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS hdd_dcs_switch_chan_cb(struct wlan_objmgr_vdev *vdev,
					 qdf_freq_t tgt_freq,
					 enum phy_ch_width tgt_width)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	mac_handle_t mac_handle;
	struct qdf_mac_addr *bssid;
	int ret;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("Invalid vdev %d", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	adapter = link_info->adapter;
	switch (adapter->device_mode) {
	case QDF_STA_MODE:
		if (!hdd_cm_is_vdev_associated(link_info))
			return QDF_STATUS_E_INVAL;

		bssid = &link_info->session.station.conn_info.bssid;

		/* disconnect if got invalid freq or width */
		if (tgt_freq == 0 || tgt_width == CH_WIDTH_INVALID) {
			pdev = wlan_vdev_get_pdev(vdev);
			if (!pdev)
				return QDF_STATUS_E_INVAL;

			hdd_dcs_add_bssid_to_reject_list(pdev, bssid);
			wlan_hdd_cm_issue_disconnect(link_info,
						     REASON_UNSPEC_FAILURE,
						     true);
			return QDF_STATUS_SUCCESS;
		}

		mac_handle = hdd_context_get_mac_handle(adapter->hdd_ctx);
		if (!mac_handle)
			return QDF_STATUS_E_INVAL;

		status = sme_switch_channel(mac_handle, bssid,
					    tgt_freq, tgt_width);
		break;
	case QDF_SAP_MODE:
		if (!test_bit(SOFTAP_BSS_STARTED, &link_info->link_flags))
			return QDF_STATUS_E_INVAL;

		/* stop sap if got invalid freq or width */
		if (tgt_freq == 0 || tgt_width == CH_WIDTH_INVALID) {
			schedule_work(&adapter->sap_stop_bss_work);
			return QDF_STATUS_SUCCESS;
		}

		psoc = wlan_vdev_get_psoc(vdev);
		if (!psoc)
			return QDF_STATUS_E_INVAL;

		wlan_hdd_set_sap_csa_reason(psoc, link_info->vdev_id,
					    CSA_REASON_DCS);
		ret = hdd_softap_set_channel_change(adapter->dev, tgt_freq,
						    tgt_width, true);
		status = qdf_status_from_os_return(ret);
		break;
	default:
		hdd_err("OP mode %d not supported", adapter->device_mode);
		break;
	}

	return status;
}

#ifdef WLAN_FEATURE_SAP_ACS_OPTIMIZE
/**
 * hdd_get_bw_for_freq - get BW for provided freq
 * @res_msg: resp msg with freq info
 * @freq: freq for which BW is required
 * @total_chan: total no of channels
 *
 * Return: bandwidth
 */
static enum phy_ch_width
hdd_get_bw_for_freq(struct get_usable_chan_res_params *res_msg,
		    uint16_t freq, uint16_t total_chan)
{
	uint16_t i;

	for (i = 0; i < total_chan; i++) {
		if (res_msg[i].freq == freq)
			return res_msg[i].bw;
	}
	return CH_WIDTH_INVALID;
}

/**
 * hdd_dcs_select_random_chan: To select random 6G channel
 * for CSA
 * @pdev: pdev object
 * @vdev: vdevobject
 *
 * Return: success/failure
 */
static QDF_STATUS
hdd_dcs_select_random_chan(struct wlan_objmgr_pdev *pdev,
			   struct wlan_objmgr_vdev *vdev)
{
	struct get_usable_chan_req_params req_msg;
	struct get_usable_chan_res_params *res_msg;
	enum phy_ch_width tgt_width;
	uint16_t final_lst[NUM_CHANNELS] = {0};
	uint16_t intf_ch_freq = 0;
	uint32_t count;
	uint32_t i;
	QDF_STATUS status = QDF_STATUS_E_EMPTY;

	res_msg = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(*res_msg));

	if (!res_msg) {
		hdd_err("res_msg invalid");
		return QDF_STATUS_E_NOMEM;
	}
	req_msg.band_mask = BIT(REG_BAND_6G);
	req_msg.iface_mode_mask = BIT(NL80211_IFTYPE_AP);
	req_msg.filter_mask = 0;
	status = wlan_reg_get_usable_channel(pdev, req_msg, res_msg, &count,
					     REG_CURRENT_PWR_MODE);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("get usable channel failed %d", status);
		qdf_mem_free(res_msg);
		return QDF_STATUS_E_INVAL;
	}
	hdd_debug("channel count %d for band %d", count, REG_BAND_6G);
	for (i = 0; i < count; i++)
		final_lst[i] = res_msg[i].freq;

	intf_ch_freq = wlan_get_rand_from_lst_for_freq(final_lst, count);
	if (!intf_ch_freq || intf_ch_freq > wlan_reg_max_6ghz_chan_freq()) {
		hdd_debug("ch freq gt max 6g freq %d",
			  wlan_reg_max_6ghz_chan_freq());
		qdf_mem_free(res_msg);
		return QDF_STATUS_E_INVAL;
	}
	tgt_width = hdd_get_bw_for_freq(res_msg, intf_ch_freq, count);
	if (tgt_width >= CH_WIDTH_INVALID) {
		qdf_mem_free(res_msg);
		return QDF_STATUS_E_INVAL;
	}
	if (tgt_width > CH_WIDTH_160MHZ) {
		hdd_debug("restrict max bw to 160");
		tgt_width = CH_WIDTH_160MHZ;
	}
	qdf_mem_free(res_msg);
	return ucfg_dcs_switch_chan(vdev, intf_ch_freq,
				    tgt_width);
}
#else
static inline QDF_STATUS
hdd_dcs_select_random_chan(struct wlan_objmgr_pdev *pdev,
			   struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * hdd_dcs_cb() - hdd dcs specific callback
 * @psoc: psoc
 * @mac_id: mac_id
 * @interference_type: wlan or continuous wave interference type
 * @arg: List of arguments
 *
 * This callback is registered with dcs component to start acs operation
 *
 * Return: None
 */
static void hdd_dcs_cb(struct wlan_objmgr_psoc *psoc, uint8_t mac_id,
		       uint8_t interference_type, void *arg)
{
	struct hdd_context *hdd_ctx = (struct hdd_context *)arg;
	struct wlan_hdd_link_info *link_info;
	struct sap_context *sap_ctx;
	uint32_t count;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t index;
	QDF_STATUS status;

	/*
	 * so far CAP_DCS_CWIM interference mitigation is not supported
	 */
	if (interference_type == WLAN_HOST_DCS_CWIM) {
		hdd_debug("CW interference mitigation is not supported");
		return;
	}

	if (policy_mgr_is_force_scc(psoc) &&
	    policy_mgr_is_sta_gc_active_on_mac(psoc, mac_id)) {
		ucfg_config_dcs_event_data(psoc, mac_id, true);

		hdd_debug("force scc %d, mac id %d sta gc count %d",
			  policy_mgr_is_force_scc(psoc), mac_id,
			  policy_mgr_is_sta_gc_active_on_mac(psoc, mac_id));
		return;
	}

	count = policy_mgr_get_sap_go_count_on_mac(psoc, list, mac_id);
	for (index = 0; index < count; index++) {
		link_info = hdd_get_link_info_by_vdev(hdd_ctx, list[index]);
		if (!link_info) {
			hdd_err("vdev_id %u does not exist with host",
				list[index]);
			return;
		}

		sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
		if (!wlansap_dcs_is_wlan_interference_mitigation_enabled(sap_ctx))
			continue;

		hdd_debug("DCS triggers ACS on vdev_id=%u, mac_id=%u",
			  list[index], mac_id);
		/*
		 * Select Random channel for low latency sap as
		 * ACS can't select channel of same MAC from which
		 * CSA is triggered because same MAC frequencies
		 * will not be present in scan list and results and
		 * selecting freq of other MAC may cause MCC with
		 * other modes if present.
		 */
		if (wlan_mlme_get_ap_policy(link_info->vdev) !=
		    HOST_CONCURRENT_AP_POLICY_UNSPECIFIED) {
			status = hdd_dcs_select_random_chan(hdd_ctx->pdev,
							    link_info->vdev);
			if (QDF_IS_STATUS_SUCCESS(status))
				return;
		}
		wlan_hdd_cfg80211_start_acs(link_info);
		return;
	}
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * hdd_dcs_afc_sel_chan_cb() - Callback to select best SAP channel/bandwidth
 *                             after channel state update by AFC
 * @arg: argument
 * @vdev_id: vdev id of SAP
 * @cur_freq: SAP current channel frequency
 * @cur_bw: SAP current channel bandwidth
 * @pref_bw: pointer to channel bandwidth prefer to set as input and output
 *           as target bandwidth can set
 *
 * Return: Target home channel frequency selected
 */
static qdf_freq_t hdd_dcs_afc_sel_chan_cb(void *arg,
					  uint32_t vdev_id,
					  qdf_freq_t cur_freq,
					  enum phy_ch_width cur_bw,
					  enum phy_ch_width *pref_bw)
{
	struct hdd_context *hdd_ctx = (struct hdd_context *)arg;
	struct wlan_hdd_link_info *link_info;
	struct sap_context *sap_ctx;
	qdf_freq_t target_freq;

	if (!hdd_ctx)
		return 0;

	link_info = hdd_get_link_info_by_vdev(hdd_ctx, vdev_id);
	if (!link_info)
		return 0;

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	if (!sap_ctx)
		return 0;

	target_freq = sap_afc_dcs_sel_chan(sap_ctx, cur_freq, cur_bw, pref_bw);

	return target_freq;
}
#else
static inline qdf_freq_t hdd_dcs_afc_sel_chan_cb(void *arg,
						 uint32_t vdev_id,
						 qdf_freq_t cur_freq,
						 enum phy_ch_width cur_bw,
						 enum phy_ch_width *pref_bw)
{
	return 0;
}
#endif

void hdd_dcs_register_cb(struct hdd_context *hdd_ctx)
{
	ucfg_dcs_register_cb(hdd_ctx->psoc, hdd_dcs_cb, hdd_ctx);
	ucfg_dcs_register_awgn_cb(hdd_ctx->psoc, hdd_dcs_switch_chan_cb);
	ucfg_dcs_register_afc_sel_chan_cb(hdd_ctx->psoc,
					  hdd_dcs_afc_sel_chan_cb,
					  hdd_ctx);
}

QDF_STATUS hdd_dcs_hostapd_set_chan(struct hdd_context *hdd_ctx,
				    uint8_t vdev_id,
				    qdf_freq_t dcs_ch_freq)
{
	struct hdd_ap_ctx *ap_ctx;
	struct sap_context *sap_ctx;
	QDF_STATUS status;
	uint8_t mac_id;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t conn_idx, count;
	struct wlan_hdd_link_info *link_info;
	uint32_t dcs_ch = wlan_reg_freq_to_chan(hdd_ctx->pdev, dcs_ch_freq);

	status = policy_mgr_get_mac_id_by_session_id(hdd_ctx->psoc, vdev_id,
						     &mac_id);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("get mac id failed");
		return QDF_STATUS_E_INVAL;
	}
	count = policy_mgr_get_sap_go_count_on_mac(hdd_ctx->psoc, list, mac_id);

	/*
	 * Dcs can only be enabled after all vdev finish csa.
	 * Set vdev starting for every vdev before doing csa.
	 * The CSA triggered by DCS will be done in serial.
	 */
	for (conn_idx = 0; conn_idx < count; conn_idx++) {
		link_info = hdd_get_link_info_by_vdev(hdd_ctx, list[conn_idx]);
		if (!link_info) {
			hdd_err("vdev_id %u does not exist with host",
				list[conn_idx]);
			return QDF_STATUS_E_INVAL;
		}

		ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(link_info);
		sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
		if (ap_ctx->operating_chan_freq != dcs_ch_freq)
			wlansap_dcs_set_vdev_starting(sap_ctx, true);
		else
			wlansap_dcs_set_vdev_starting(sap_ctx, false);
	}
	for (conn_idx = 0; conn_idx < count; conn_idx++) {
		link_info = hdd_get_link_info_by_vdev(hdd_ctx, list[conn_idx]);
		if (!link_info) {
			hdd_err("vdev_id %u does not exist with host",
				list[conn_idx]);
			return QDF_STATUS_E_INVAL;
		}

		ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(link_info);
		if (ap_ctx->operating_chan_freq == dcs_ch_freq)
			continue;

		hdd_ctx->acs_policy.acs_chan_freq = AUTO_CHANNEL_SELECT;
		hdd_debug("dcs triggers old ch:%d new ch:%d",
			  ap_ctx->operating_chan_freq, dcs_ch_freq);
		wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc,
					    link_info->vdev_id, CSA_REASON_DCS);
		status = hdd_switch_sap_channel(link_info, dcs_ch, true);
		if (status == QDF_STATUS_SUCCESS)
			status = QDF_STATUS_E_PENDING;
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_dcs_hostapd_enable_wlan_interference_mitigation() - enable wlan
 * interference mitigation
 * @hdd_ctx: hdd ctx
 * @vdev_id: vdev id
 *
 * This function is used to enable wlan interference mitigation through
 * send dcs command.
 *
 * Return: None
 */
static void hdd_dcs_hostapd_enable_wlan_interference_mitigation(
					struct hdd_context *hdd_ctx,
					uint8_t vdev_id)
{
	QDF_STATUS status;
	uint8_t mac_id;
	struct wlan_hdd_link_info *link_info;
	struct hdd_ap_ctx *ap_ctx;
	struct sap_context *sap_ctx;

	status = policy_mgr_get_mac_id_by_session_id(hdd_ctx->psoc, vdev_id,
						     &mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("get mac id failed");
		return;
	}

	link_info = hdd_get_link_info_by_vdev(hdd_ctx, vdev_id);
	if (!link_info) {
		hdd_err("vdev_id %u does not exist with host", vdev_id);
		return;
	}

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(link_info);
	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	if (wlansap_dcs_is_wlan_interference_mitigation_enabled(sap_ctx) &&
	    !WLAN_REG_IS_24GHZ_CH_FREQ(ap_ctx->operating_chan_freq))
		ucfg_config_dcs_event_data(hdd_ctx->psoc, mac_id, true);
}

void hdd_dcs_chan_select_complete(struct hdd_adapter *adapter)
{
	qdf_freq_t dcs_freq;
	struct hdd_context *hdd_ctx;
	uint32_t chan_freq;
	struct hdd_ap_ctx *ap_ctx;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context pointer");
		return;
	}

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter->deflink);
	dcs_freq = wlansap_dcs_get_freq(ap_ctx->sap_context);
	chan_freq = ap_ctx->operating_chan_freq;
	if (dcs_freq && dcs_freq != chan_freq)
		hdd_dcs_hostapd_set_chan(hdd_ctx, adapter->deflink->vdev_id,
					 dcs_freq);
	else
		hdd_dcs_hostapd_enable_wlan_interference_mitigation(
					hdd_ctx, adapter->deflink->vdev_id);

	qdf_atomic_set(&ap_ctx->acs_in_progress, 0);
}

void hdd_dcs_clear(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	uint8_t mac_id;
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_psoc *psoc;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct sap_context *sap_ctx;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context pointer");
		return;
	}

	psoc = hdd_ctx->psoc;

	status = policy_mgr_get_mac_id_by_session_id(psoc,
						     adapter->deflink->vdev_id,
						     &mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("get mac id failed");
		return;
	}

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter->deflink);
	if (policy_mgr_get_sap_go_count_on_mac(psoc, list, mac_id) <= 1) {
		ucfg_config_dcs_disable(psoc, mac_id, WLAN_HOST_DCS_WLANIM);
		ucfg_wlan_dcs_cmd(psoc, mac_id, true);
		if (wlansap_dcs_is_wlan_interference_mitigation_enabled(sap_ctx))
			ucfg_dcs_clear(psoc, mac_id);
	}

	wlansap_dcs_set_vdev_wlan_interference_mitigation(sap_ctx, false);
	wlansap_dcs_set_vdev_starting(sap_ctx, false);
}
