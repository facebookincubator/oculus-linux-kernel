/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: hdd_cm_disconnect.c
 *
 * WLAN Host Device Driver disconnect APIs implementation
 *
 */

#include "wlan_hdd_main.h"
#include "wlan_hdd_object_manager.h"
#include "wlan_hdd_trace.h"
#include <osif_cm_req.h>
#include "wlan_hdd_cm_api.h"
#include "wlan_ipa_ucfg_api.h"
#include "wlan_hdd_stats.h"
#include "wlan_hdd_scan.h"
#include "sme_power_save_api.h"
#include <wlan_logging_sock_svc.h>
#include "wlan_hdd_ftm_time_sync.h"
#include "wlan_hdd_bcn_recv.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_ipa.h"
#include "wlan_hdd_green_ap.h"
#include "wlan_hdd_lpass.h"
#include "wlan_hdd_bootup_marker.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_crypto_global_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "hif.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_napi.h"
#include "wlan_hdd_cfr.h"
#include "wlan_roam_debug.h"
#include "wma_api.h"
#include "wlan_hdd_hostapd.h"
#include "wlan_dp_ucfg_api.h"
#include "wma.h"

void hdd_handle_disassociation_event(struct wlan_hdd_link_info *link_info,
				     struct qdf_mac_addr *peer_macaddr)
{
	struct hdd_adapter *adapter = link_info->adapter;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *sta_ctx;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_objmgr_vdev *vdev;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(link_info);
	hdd_green_ap_start_state_mc(hdd_ctx, adapter->device_mode, false);

	wlan_hdd_auto_shutdown_enable(hdd_ctx, true);

	if ((adapter->device_mode == QDF_STA_MODE) ||
	    (adapter->device_mode == QDF_P2P_CLIENT_MODE))
		/* send peer status indication to oem app */
		hdd_send_peer_status_ind_to_app(peer_macaddr,
						ePeerDisconnected, 0,
						link_info->vdev_id, NULL,
						adapter->device_mode);

	hdd_lpass_notify_disconnect(link_info);

	vdev = hdd_objmgr_get_vdev_by_user(link_info, WLAN_DP_ID);
	if (vdev) {
		ucfg_dp_del_latency_critical_client(vdev,
			hdd_convert_cfgdot11mode_to_80211mode(
				sta_ctx->conn_info.dot11mode));
		/* stop timer in sta/p2p_cli */
		ucfg_dp_bus_bw_compute_reset_prev_txrx_stats(vdev);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}

	ucfg_dp_bus_bw_compute_timer_try_stop(hdd_ctx->psoc);
	cdp_display_txrx_hw_info(soc);
}

/**
 * hdd_cm_print_bss_info() - print bss info
 * @hdd_sta_ctx: pointer to hdd station context
 *
 * Return: None
 */
static void hdd_cm_print_bss_info(struct hdd_station_ctx *hdd_sta_ctx)
{
	uint32_t *ht_cap_info;
	uint32_t *vht_cap_info;
	struct hdd_connection_info *conn_info;

	conn_info = &hdd_sta_ctx->conn_info;

	hdd_nofl_debug("*********** WIFI DATA LOGGER **************");
	hdd_nofl_debug("freq: %d dot11mode %d AKM %d ssid: \"" QDF_SSID_FMT "\" ,roam_count %d nss %d legacy %d mcs %d signal %d noise: %d",
		       conn_info->chan_freq, conn_info->dot11mode,
		       conn_info->last_auth_type,
		       QDF_SSID_REF(conn_info->last_ssid.SSID.length,
				    conn_info->last_ssid.SSID.ssId),
		       conn_info->roam_count,
		       conn_info->txrate.nss, conn_info->txrate.legacy,
		       conn_info->txrate.mcs, conn_info->signal,
		       conn_info->noise);
	ht_cap_info = (uint32_t *)&conn_info->ht_caps;
	vht_cap_info = (uint32_t *)&conn_info->vht_caps;
	hdd_nofl_debug("HT 0x%x VHT 0x%x ht20 info 0x%x",
		       conn_info->conn_flag.ht_present ? *ht_cap_info : 0,
		       conn_info->conn_flag.vht_present ? *vht_cap_info : 0,
		       conn_info->conn_flag.hs20_present ?
		       conn_info->hs20vendor_ie.release_num : 0);
}

void
__hdd_cm_disconnect_handler_pre_user_update(struct wlan_hdd_link_info *link_info)
{
	struct hdd_adapter *adapter = link_info->adapter;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *sta_ctx;
	uint32_t time_buffer_size;
	struct wlan_objmgr_vdev *vdev;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(link_info);
	hdd_stop_tsf_sync(adapter);
	time_buffer_size = sizeof(sta_ctx->conn_info.connect_time);
	qdf_mem_zero(sta_ctx->conn_info.connect_time, time_buffer_size);
	if (ucfg_ipa_is_enabled() &&
	    QDF_IS_STATUS_SUCCESS(wlan_hdd_validate_mac_address(
				  &sta_ctx->conn_info.bssid)))
		ucfg_ipa_wlan_evt(hdd_ctx->pdev, adapter->dev,
				  adapter->device_mode,
				  link_info->vdev_id,
				  WLAN_IPA_STA_DISCONNECT,
				  sta_ctx->conn_info.bssid.bytes,
				  false);

	vdev = hdd_objmgr_get_vdev_by_user(link_info, WLAN_DP_ID);
	if (vdev) {
		ucfg_dp_periodic_sta_stats_stop(vdev);
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}

	wlan_hdd_auto_shutdown_enable(hdd_ctx, true);

	DPTRACE(qdf_dp_trace_mgmt_pkt(QDF_DP_TRACE_MGMT_PACKET_RECORD,
		link_info->vdev_id,
		QDF_TRACE_DEFAULT_PDEV_ID,
		QDF_PROTO_TYPE_MGMT, QDF_PROTO_MGMT_DISASSOC));

	hdd_wmm_dscp_initial_state(adapter);
	wlan_deregister_txrx_packetdump(OL_TXRX_PDEV_ID);

	hdd_place_marker(adapter, "DISCONNECTED", NULL);
}

void
__hdd_cm_disconnect_handler_post_user_update(struct wlan_hdd_link_info *link_info,
					     struct wlan_objmgr_vdev *vdev,
					     enum wlan_cm_source source)
{
	struct hdd_adapter *adapter = link_info->adapter;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *sta_ctx;
	mac_handle_t mac_handle;
	struct hdd_adapter *link_adapter;
	struct hdd_station_ctx *link_sta_ctx;
	bool is_link_switch =
			wlan_vdev_mlme_is_mlo_link_switch_in_progress(vdev);

	mac_handle = hdd_ctx->mac_handle;
	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(link_info);

	/* update P2P connection status */
	ucfg_p2p_status_disconnect(vdev);
	hdd_cfr_disconnect(vdev);

	hdd_wmm_adapter_clear(adapter);
	ucfg_cm_ft_reset(vdev);
	ucfg_cm_reset_key(hdd_ctx->pdev, link_info->vdev_id);
	hdd_clear_roam_profile_ie(adapter);

	if (adapter->device_mode == QDF_STA_MODE)
		wlan_crypto_reset_vdev_params(vdev);

	hdd_remove_beacon_filter(adapter);
	if (sme_is_beacon_report_started(mac_handle, link_info->vdev_id)) {
		hdd_debug("Sending beacon pause indication to userspace");
		hdd_beacon_recv_pause_indication((hdd_handle_t)hdd_ctx,
						 link_info->vdev_id,
						 SCAN_EVENT_TYPE_MAX, true);
	}

	if (adapter->device_mode == QDF_STA_MODE &&
	    hdd_adapter_is_ml_adapter(adapter)) {
		/* Clear connection info in assoc link adapter as well */
		link_adapter = hdd_get_assoc_link_adapter(adapter);
		if (link_adapter) {
			link_sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(link_adapter->deflink);
			hdd_conn_remove_connect_info(link_sta_ctx);
		}
	}

	if (!is_link_switch && source != CM_MLO_ROAM_INTERNAL_DISCONNECT) {
		/* Clear saved connection information in HDD */
		hdd_conn_remove_connect_info(sta_ctx);

		/*
		 * Reset the IEEE link ID to invalid when disconnect is not
		 * due to link switch. This API resets link id for all the
		 * valid link_info for the given adapter. So avoid this reset
		 * for Link Switch disconnect/internal disconnect
		 */
		hdd_adapter_reset_station_ctx(adapter);
	}

	ucfg_dp_remove_conn_info(vdev);

	/* Setting the RTS profile to original value */
	if (sme_cli_set_command(link_info->vdev_id,
				wmi_vdev_param_enable_rtscts,
				cfg_get(hdd_ctx->psoc,
					CFG_ENABLE_FW_RTS_PROFILE),
				VDEV_CMD))
		hdd_debug("Failed to set RTS_PROFILE");

	hdd_init_scan_reject_params(hdd_ctx);
	ucfg_pmo_flush_gtk_offload_req(vdev);

	if ((QDF_STA_MODE == adapter->device_mode) ||
	    (QDF_P2P_CLIENT_MODE == adapter->device_mode)) {
		sme_ps_disable_auto_ps_timer(mac_handle, link_info->vdev_id);
		adapter->send_mode_change = true;
	}
	wlan_hdd_clear_link_layer_stats(adapter);

	ucfg_dp_reset_cont_txtimeout_cnt(vdev);

	ucfg_dp_nud_reset_tracking(vdev);
	hdd_reset_limit_off_chan(adapter);

	hdd_cm_print_bss_info(sta_ctx);
}

#ifdef WLAN_FEATURE_MSCS
void reset_mscs_params(struct wlan_hdd_link_info *link_info)
{
	mlme_set_is_mscs_req_sent(link_info->vdev, false);
	link_info->mscs_counter = 0;
}
#endif

QDF_STATUS
wlan_hdd_cm_issue_disconnect(struct wlan_hdd_link_info *link_info,
			     enum wlan_reason_code reason, bool sync)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	struct hdd_station_ctx *sta_ctx;
	struct hdd_adapter *adapter = link_info->adapter;

	vdev = hdd_objmgr_get_vdev_by_user(link_info, WLAN_OSIF_CM_ID);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(link_info);
	hdd_place_marker(adapter, "TRY TO DISCONNECT", NULL);
	reset_mscs_params(link_info);
	hdd_conn_set_authenticated(link_info, false);
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	qdf_rtpm_sync_resume();

	wlan_rec_conn_info(link_info->vdev_id, DEBUG_CONN_DISCONNECT,
			   sta_ctx->conn_info.bssid.bytes, 0, reason);

	if (sync)
		status = osif_cm_disconnect_sync(vdev, reason);
	else
		status = osif_cm_disconnect(adapter->dev, vdev, reason);

	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_CM_ID);

	return status;
}

int wlan_hdd_cm_disconnect(struct wiphy *wiphy,
			   struct net_device *dev, u16 reason)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;
	int ret;
	struct wlan_hdd_link_info *link_info = adapter->deflink;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (wlan_hdd_validate_vdev_id(link_info->vdev_id))
		return -EINVAL;

	if (hdd_ctx->is_wiphy_suspended) {
		hdd_info_rl("wiphy is suspended retry disconnect");
		return -EAGAIN;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_DISCONNECT,
		   link_info->vdev_id, reason);

	hdd_print_netdev_txq_status(dev);

	if (reason == WLAN_REASON_DEAUTH_LEAVING)
		qdf_dp_trace_dump_all(
				WLAN_DEAUTH_DPTRACE_DUMP_COUNT,
				QDF_TRACE_DEFAULT_PDEV_ID);
	/*
	 * for Supplicant initiated disconnect always wait for complete,
	 * as for WPS connection or back to back connect, supplicant initiate a
	 * disconnect which is followed by connect and if kernel is not yet
	 * disconnected, this new connect will be rejected by kernel with status
	 * EALREADY. In case connect is rejected with EALREADY, supplicant will
	 * queue one more disconnect followed by connect immediately, Now if
	 * driver is not disconnected by this time, the kernel will again reject
	 * connect and thus the failing the connect req in supplicant.
	 * Thus we need to wait for disconnect to complete in this case,
	 * and thus use sync API here.
	 */
	status = wlan_hdd_cm_issue_disconnect(link_info, reason, true);

	return qdf_status_to_os_return(status);
}

static QDF_STATUS
hdd_cm_disconnect_complete_pre_user_update(struct wlan_objmgr_vdev *vdev,
					   struct wlan_cm_discon_rsp *rsp)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	link_info = hdd_get_link_info_by_vdev(hdd_ctx, wlan_vdev_get_id(vdev));
	if (!link_info) {
		hdd_err("adapter is NULL for vdev %d", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	adapter = link_info->adapter;
	hdd_conn_set_authenticated(link_info, false);
	hdd_napi_serialize(0);
	hdd_disable_and_flush_mc_addr_list(adapter, pmo_peer_disconnect);
	__hdd_cm_disconnect_handler_pre_user_update(link_info);

	hdd_handle_disassociation_event(link_info, &rsp->req.req.bssid);

	wlan_rec_conn_info(link_info->vdev_id,
			   DEBUG_CONN_DISCONNECT_HANDLER,
			   rsp->req.req.bssid.bytes,
			   rsp->req.cm_id,
			   rsp->req.req.reason_code << 16 |
			   rsp->req.req.source);
	wlan_hdd_set_tx_flow_info();
	/*
	 * Convert and cache internal reason code in adapter. This can be
	 * sent to userspace with a vendor event.
	 */
	adapter->last_disconnect_reason =
			osif_cm_mac_to_qca_reason(rsp->req.req.reason_code);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_cm_set_default_wlm_mode - reset the default wlm mode if
 *				 wlm_latency_reset_on_disconnect is set.
 *@adapter: adapter pointer
 *
 * return: None.
 */
static void hdd_cm_set_default_wlm_mode(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	bool reset;
	uint8_t def_level;
	uint32_t client_id_bitmap;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return;
	}

	status = ucfg_mlme_cfg_get_wlm_reset(hdd_ctx->psoc, &reset);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get wlm reset flag");
		return;
	}
	if (!reset)
		return;

	status = ucfg_mlme_cfg_get_wlm_level(hdd_ctx->psoc, &def_level);
	if (QDF_IS_STATUS_ERROR(status))
		def_level = QCA_WLAN_VENDOR_ATTR_CONFIG_LATENCY_LEVEL_NORMAL;

	if (hdd_get_multi_client_ll_support(adapter)) {
		client_id_bitmap = wlan_hdd_get_client_id_bitmap(adapter);
		hdd_debug("client_id_bitmap: 0x%x", client_id_bitmap);
		status = wlan_hdd_set_wlm_latency_level(adapter, def_level,
							client_id_bitmap, true);
		wlan_hdd_deinit_multi_client_info_table(adapter);
	} else {
		status =
			sme_set_wlm_latency_level(hdd_ctx->mac_handle,
						  adapter->deflink->vdev_id,
						  def_level, 0, false);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			hdd_debug("reset wlm mode %x on disconnection",
				  def_level);
			adapter->latency_level = def_level;
		} else {
			hdd_err("reset wlm mode failed: %d", status);
		}
	}
}

/**
 * hdd_cm_reset_udp_qos_upgrade_config() - Reset the threshold for UDP packet
 * QoS upgrade.
 * @adapter: adapter for which this configuration is to be applied
 *
 * Return: None
 */
static void hdd_cm_reset_udp_qos_upgrade_config(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	bool reset;
	QDF_STATUS status;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return;
	}

	status = ucfg_mlme_cfg_get_wlm_reset(hdd_ctx->psoc, &reset);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get the wlm reset flag");
		return;
	}

	if (reset) {
		adapter->upgrade_udp_qos_threshold = QCA_WLAN_AC_BK;
		hdd_debug("UDP packets qos upgrade to: %d",
			  adapter->upgrade_udp_qos_threshold);
	}
}

#ifdef WLAN_FEATURE_11BE
static inline enum eSirMacHTChannelWidth get_max_bw(void)
{
	uint32_t max_bw = wma_get_orig_eht_ch_width();

	if (max_bw == WNI_CFG_EHT_CHANNEL_WIDTH_320MHZ)
		return eHT_CHANNEL_WIDTH_320MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		return eHT_CHANNEL_WIDTH_160MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
		return eHT_CHANNEL_WIDTH_80P80MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
		return eHT_CHANNEL_WIDTH_80MHZ;
	else
		return eHT_CHANNEL_WIDTH_40MHZ;
}

static
void wlan_hdd_re_enable_320mhz_6g_conection(struct hdd_context *hdd_ctx,
					    enum phy_ch_width assoc_ch_width)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(hdd_ctx->psoc);
	if (!mlme_obj)
		return;
	/*
	 * Initial connection was in 320 MHz and if via SET_MAX_BANDWIDTH
	 * command, current channel BW (des_chan->ch_width) gets modified
	 * to less than 320MHz, driver disables 6 GHz connection by disabling
	 * support_320mhz_6ghz EHT capability. So, in order to allow
	 * re-connection (after disconnection) in 320 MHz, also re-enable
	 * support_320mhz_6ghz EHT capability before disconnect complete.
	 */
	if (assoc_ch_width == CH_WIDTH_320MHZ)
		mlme_obj->cfg.eht_caps.dot11_eht_cap.support_320mhz_6ghz = 1;
}
#else
static inline enum eSirMacHTChannelWidth get_max_bw(void)
{
	uint32_t max_bw = wma_get_vht_ch_width();

	if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		return eHT_CHANNEL_WIDTH_160MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
		return eHT_CHANNEL_WIDTH_80P80MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
		return eHT_CHANNEL_WIDTH_80MHZ;
	else
		return eHT_CHANNEL_WIDTH_40MHZ;
}

static
void wlan_hdd_re_enable_320mhz_6g_conection(struct hdd_context *hdd_ctx,
					    enum phy_ch_width assoc_ch_width)
{
}
#endif

static void hdd_cm_restore_ch_width(struct wlan_objmgr_vdev *vdev,
				    struct wlan_hdd_link_info *link_info)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(link_info->adapter);
	struct mlme_legacy_priv *mlme_priv;
	enum eSirMacHTChannelWidth max_bw;
	struct wlan_channel *des_chan;
	uint8_t link_id = 0xFF;
	int ret;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	enum phy_ch_width assoc_ch_width;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	if (!des_chan)
		return;

	assoc_ch_width = mlme_priv->connect_info.assoc_chan_info.assoc_ch_width;
	if (!ucfg_mlme_is_chwidth_with_notify_supported(hdd_ctx->psoc) ||
	    assoc_ch_width == CH_WIDTH_INVALID)
		return;

	cm_update_associated_ch_info(vdev, false);

	if (des_chan->ch_width != assoc_ch_width)
		wlan_hdd_re_enable_320mhz_6g_conection(hdd_ctx, assoc_ch_width);

	max_bw = get_max_bw();
	ret = hdd_set_mac_chan_width(link_info, max_bw, link_id, true);
	if (ret) {
		hdd_err("vdev %d : fail to set max ch width", vdev_id);
		return;
	}

	hdd_debug("vdev %d : updated ch width to: %d on disconnection", vdev_id,
		  max_bw);
}

static QDF_STATUS
hdd_cm_disconnect_complete_post_user_update(struct wlan_objmgr_vdev *vdev,
					    struct wlan_cm_discon_rsp *rsp)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	link_info = hdd_get_link_info_by_vdev(hdd_ctx, wlan_vdev_get_id(vdev));
	if (!link_info) {
		hdd_err("adapter is NULL for vdev %d", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	adapter = link_info->adapter;
	if (adapter->device_mode == QDF_STA_MODE) {
	/* Inform FTM TIME SYNC about the disconnection with the AP */
		hdd_ftm_time_sync_sta_state_notify(
				adapter, FTM_TIME_SYNC_STA_DISCONNECTED);
	}

	/*
	 * via the SET_MAX_BANDWIDTH command, the upper layer can update channel
	 * width. The host should update channel bandwidth to the max supported
	 * bandwidth on disconnection so that post disconnection DUT can
	 * connect in max BW.
	 */
	hdd_cm_restore_ch_width(vdev, link_info);

	/*
	 * same WLM configuration is applicable for all links, So no need to
	 * restore it while processing disconnection due to link switch.
	 */
	if (rsp->req.req.source != CM_MLO_LINK_SWITCH_DISCONNECT)
		hdd_cm_set_default_wlm_mode(adapter);

	__hdd_cm_disconnect_handler_post_user_update(link_info, vdev,
						     rsp->req.req.source);
	wlan_twt_concurrency_update(hdd_ctx);
	hdd_cm_reset_udp_qos_upgrade_config(adapter);
	ucfg_mlme_set_ml_link_control_mode(hdd_ctx->psoc,
					   vdev->vdev_objmgr.vdev_id, 0);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_RUNTIME_PM
static void
wlan_hdd_runtime_pm_wow_disconnect_handler(struct hdd_context *hdd_ctx)
{
	struct hif_opaque_softc *hif_ctx;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("hif_ctx is NULL");
		return;
	}

	if (hdd_is_any_sta_connected(hdd_ctx)) {
		hdd_debug("active connections: runtime pm prevented: %d",
			  hdd_ctx->runtime_pm_prevented);
		return;
	}

	hdd_debug("Runtime allowed : %d", hdd_ctx->runtime_pm_prevented);
	qdf_spin_lock_irqsave(&hdd_ctx->pm_qos_lock);
	if (hdd_ctx->runtime_pm_prevented) {
		qdf_rtpm_put(QDF_RTPM_PUT, QDF_RTPM_ID_PM_QOS_NOTIFY);
		hdd_ctx->runtime_pm_prevented = false;
	}
	qdf_spin_unlock_irqrestore(&hdd_ctx->pm_qos_lock);
}
#else
static void
wlan_hdd_runtime_pm_wow_disconnect_handler(struct hdd_context *hdd_ctx)
{
}
#endif

QDF_STATUS hdd_cm_disconnect_complete(struct wlan_objmgr_vdev *vdev,
				      struct wlan_cm_discon_rsp *rsp,
				      enum osif_cb_type type)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	switch (type) {
	case OSIF_PRE_USERSPACE_UPDATE:
		return hdd_cm_disconnect_complete_pre_user_update(vdev, rsp);
	case OSIF_POST_USERSPACE_UPDATE:
		hdd_debug("Wifi disconnected: vdev id %d",
			  vdev->vdev_objmgr.vdev_id);
		wlan_hdd_runtime_pm_wow_disconnect_handler(hdd_ctx);

		return hdd_cm_disconnect_complete_post_user_update(vdev, rsp);
	default:
		hdd_cm_disconnect_complete_pre_user_update(vdev, rsp);
		hdd_cm_disconnect_complete_post_user_update(vdev, rsp);
		return QDF_STATUS_SUCCESS;
	}
}

QDF_STATUS hdd_cm_netif_queue_control(struct wlan_objmgr_vdev *vdev,
				      enum netif_action_type action,
				      enum netif_reason_type reason)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct wlan_hdd_link_info *link_info;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	link_info = hdd_get_link_info_by_vdev(hdd_ctx, wlan_vdev_get_id(vdev));
	if (!link_info) {
		hdd_err("adapter is NULL for vdev %d", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	wlan_hdd_netif_queue_control(link_info->adapter, action, reason);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_cm_napi_serialize_control(bool action)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	hdd_napi_serialize(action);

	/* reinit scan reject params for napi off (roam abort/ho fail) */
	if (!action)
		hdd_init_scan_reject_params(hdd_ctx);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_BOOST_CPU_FREQ_IN_ROAM
QDF_STATUS hdd_cm_perfd_set_cpufreq(bool action)
{
	struct wlan_core_minfreq req;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (unlikely(!hdd_ctx)) {
		hdd_err("cannot get hdd_context");
		return QDF_STATUS_E_INVAL;
	}

	if (action) {
		req.magic    = WLAN_CORE_MINFREQ_MAGIC;
		req.reserved = 0; /* unused */
		req.coremask = 0x00ff;/* big and little cluster */
		req.freq     = 0xfff;/* set to max freq */
	} else {
		req.magic    = WLAN_CORE_MINFREQ_MAGIC;
		req.reserved = 0; /* unused */
		req.coremask = 0; /* not valid */
		req.freq     = 0; /* reset */
	}

	hdd_debug("CPU min freq to 0x%x coremask 0x%x", req.freq, req.coremask);
	/* the following service function returns void */
	wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
				    WLAN_SVC_CORE_MINFREQ,
				    &req, sizeof(struct wlan_core_minfreq));
	return QDF_STATUS_SUCCESS;
}
#endif
