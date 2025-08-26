/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: wlan_cm_roam_api.c
 *
 * Implementation for the Common Roaming interfaces.
 */

#include "wlan_cm_roam_api.h"
#include "wlan_vdev_mlme_api.h"
#include "wlan_mlme_main.h"
#include "wlan_policy_mgr_api.h"
#include <wmi_unified_priv.h>
#include <../../core/src/wlan_cm_vdev_api.h>
#include "wlan_crypto_global_api.h"
#include <wlan_cm_api.h>
#include "connection_mgr/core/src/wlan_cm_roam.h"
#include "wlan_cm_roam_api.h"
#include "wlan_dlm_api.h"
#include <../../core/src/wlan_cm_roam_i.h>
#include "wlan_reg_ucfg_api.h"
#include "wlan_connectivity_logging.h"
#include "target_if.h"
#include "wlan_mlo_mgr_roam.h"

/* Support for "Fast roaming" (i.e., ESE, LFR, or 802.11r.) */
#define BG_SCAN_OCCUPIED_CHANNEL_LIST_LEN 15
#define CM_MIN_RSSI 0 /* 0dbm */

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS
wlan_cm_enable_roaming_on_connected_sta(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t sta_vdev_id = WLAN_INVALID_VDEV_ID;
	uint32_t count;
	uint32_t idx;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	sta_vdev_id = policy_mgr_get_roam_enabled_sta_session_id(psoc, vdev_id);
	if (sta_vdev_id != WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_FAILURE;

	count = policy_mgr_get_mode_specific_conn_info(psoc,
						       op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);

	if (!count)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Loop through all connected STA vdevs and roaming will be enabled
	 * on the STA that has a different vdev id from the one passed as
	 * input and has non zero roam trigger value.
	 */
	for (idx = 0; idx < count; idx++) {
		if (vdev_id_list[idx] != vdev_id &&
		    mlme_get_roam_trigger_bitmap(psoc, vdev_id_list[idx])) {
			sta_vdev_id = vdev_id_list[idx];
			break;
		}
	}

	if (sta_vdev_id == WLAN_INVALID_VDEV_ID)
		return QDF_STATUS_E_FAILURE;

	mlme_debug("ROAM: Enabling roaming on vdev[%d]", sta_vdev_id);

	return cm_roam_state_change(pdev,
				    sta_vdev_id,
				    WLAN_ROAM_RSO_ENABLED,
				    REASON_CTX_INIT,
				    NULL, false);
}

QDF_STATUS wlan_cm_roam_state_change(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id,
				     enum roam_offload_state requested_state,
				     uint8_t reason)
{
	return cm_roam_state_change(pdev, vdev_id, requested_state, reason,
				    NULL, false);
}

QDF_STATUS wlan_cm_roam_send_rso_cmd(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id, uint8_t rso_command,
				     uint8_t reason)
{
	return cm_roam_send_rso_cmd(psoc, vdev_id, rso_command, reason);
}

void wlan_cm_handle_sta_sta_roaming_enablement(struct wlan_objmgr_psoc *psoc,
					       uint8_t vdev_id)
{
	return cm_handle_sta_sta_roaming_enablement(psoc, vdev_id);
}

QDF_STATUS
wlan_roam_update_cfg(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		     uint8_t reason)
{
	if (!MLME_IS_ROAM_STATE_RSO_ENABLED(psoc, vdev_id)) {
		mlme_debug("Update cfg received while ROAM RSO not started");
		return QDF_STATUS_E_INVAL;
	}

	return cm_roam_send_rso_cmd(psoc, vdev_id, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
				    reason);
}

#endif

void
cm_update_associated_ch_info(struct wlan_objmgr_vdev *vdev, bool is_update)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_channel *des_chan;
	struct connect_chan_info *chan_info_orig;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;

	chan_info_orig = &mlme_priv->connect_info.chan_info_orig;
	if (!is_update) {
		chan_info_orig->ch_width_orig = CH_WIDTH_INVALID;
		return;
	}

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	if (!des_chan)
		return;
	chan_info_orig->ch_width_orig = des_chan->ch_width;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(des_chan->ch_freq) &&
	    des_chan->ch_width == CH_WIDTH_40MHZ) {
		if (des_chan->ch_cfreq1 == des_chan->ch_freq + BW_10_MHZ)
			chan_info_orig->sec_2g_freq =
					des_chan->ch_freq + BW_20_MHZ;
		if (des_chan->ch_cfreq1 == des_chan->ch_freq - BW_10_MHZ)
			chan_info_orig->sec_2g_freq =
					des_chan->ch_freq - BW_20_MHZ;
	}

	mlme_debug("ch width :%d, ch_freq:%d, ch_cfreq1:%d, sec_2g_freq:%d",
		   chan_info_orig->ch_width_orig, des_chan->ch_freq,
		   des_chan->ch_cfreq1, chan_info_orig->sec_2g_freq);
}

char *cm_roam_get_requestor_string(enum wlan_cm_rso_control_requestor requestor)
{
	switch (requestor) {
	case RSO_INVALID_REQUESTOR:
	default:
		return "No requestor";
	case RSO_START_BSS:
		return "SAP start";
	case RSO_CHANNEL_SWITCH:
		return "CSA";
	case RSO_CONNECT_START:
		return "STA connection";
	case RSO_SAP_CHANNEL_CHANGE:
		return "SAP Ch switch";
	case RSO_NDP_CON_ON_NDI:
		return "NDP connection";
	case RSO_SET_PCL:
		return "Set PCL";
	}
}

QDF_STATUS
wlan_cm_rso_set_roam_trigger(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			     struct wlan_roam_triggers *trigger)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status = cm_roam_acquire_lock(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto release_ref;

	status = cm_rso_set_roam_trigger(pdev, vdev_id, trigger);

	cm_roam_release_lock(vdev);

release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return status;
}

QDF_STATUS wlan_cm_disable_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			       enum wlan_cm_rso_control_requestor requestor,
			       uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	if (reason == REASON_DRIVER_DISABLED && requestor)
		mlme_set_operations_bitmap(psoc, vdev_id, requestor, false);

	mlme_debug("ROAM_CONFIG: vdev[%d] Disable roaming - requestor:%s",
		   vdev_id, cm_roam_get_requestor_string(requestor));

	status = cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_STOPPED,
				      REASON_DRIVER_DISABLED, NULL, false);

	return status;
}

QDF_STATUS wlan_cm_enable_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			      enum wlan_cm_rso_control_requestor requestor,
			      uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	if (reason == REASON_DRIVER_ENABLED && requestor)
		mlme_set_operations_bitmap(psoc, vdev_id, requestor, true);

	mlme_debug("ROAM_CONFIG: vdev[%d] Enable roaming - requestor:%s",
		   vdev_id, cm_roam_get_requestor_string(requestor));

	status = cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_ENABLED,
				      REASON_DRIVER_ENABLED, NULL, false);

	return status;
}

bool wlan_cm_host_roam_in_progress(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool host_roam_in_progress = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", vdev_id);
		return host_roam_in_progress;
	}

	if (wlan_cm_is_vdev_roam_preauth_state(vdev) ||
	    wlan_cm_is_vdev_roam_reassoc_state(vdev))
		host_roam_in_progress = true;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return host_roam_in_progress;
}

bool wlan_cm_roaming_in_progress(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	bool roaming_in_progress = false;
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE opmode;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev)
		return roaming_in_progress;

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	if (opmode != QDF_STA_MODE && opmode != QDF_P2P_CLIENT_MODE)
		goto exit;

	roaming_in_progress = wlan_cm_is_vdev_roaming(vdev);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return roaming_in_progress;
}

QDF_STATUS wlan_cm_roam_stop_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 uint8_t reason)
{
	return cm_roam_stop_req(psoc, vdev_id, reason, NULL, false);
}

bool wlan_cm_same_band_sta_allowed(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct dual_sta_policy *dual_sta_policy;

	if (!wlan_mlme_get_dual_sta_roaming_enabled(psoc))
		return true;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return true;

	dual_sta_policy = &mlme_obj->cfg.gen.dual_sta_policy;
	if (dual_sta_policy->primary_vdev_id != WLAN_UMAC_VDEV_ID_MAX &&
	    dual_sta_policy->concurrent_sta_policy ==
	    QCA_WLAN_CONCURRENT_STA_POLICY_PREFER_PRIMARY)
		return true;

	return false;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
QDF_STATUS wlan_cm_send_roam_linkspeed_state(struct scheduler_msg *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct roam_link_speed_cfg *link_speed_cfg;
	struct roam_disable_cfg  roam_link_speed_cfg;

	if (!msg || !msg->bodyptr)
		return QDF_STATUS_E_FAILURE;

	link_speed_cfg = msg->bodyptr;
	roam_link_speed_cfg.vdev_id = link_speed_cfg->vdev_id;
	roam_link_speed_cfg.cfg = link_speed_cfg->is_link_speed_good;
	status = wlan_cm_tgt_send_roam_linkspeed_state(link_speed_cfg->psoc,
						       &roam_link_speed_cfg);
	qdf_mem_free(link_speed_cfg);

	return status;
}

void wlan_cm_roam_link_speed_update(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    bool is_link_speed_good)
{
	QDF_STATUS qdf_status;
	struct scheduler_msg msg = {0};
	struct roam_link_speed_cfg *link_speed_cfg;

	link_speed_cfg = qdf_mem_malloc(sizeof(*link_speed_cfg));
	if (!link_speed_cfg)
		return;

	link_speed_cfg->psoc = psoc;
	link_speed_cfg->vdev_id = vdev_id;
	link_speed_cfg->is_link_speed_good = is_link_speed_good;

	msg.bodyptr = link_speed_cfg;
	msg.callback = wlan_cm_send_roam_linkspeed_state;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_MLME,
					    QDF_MODULE_ID_OS_IF,
					    QDF_MODULE_ID_OS_IF, &msg);

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		mlme_err("post msg failed");
		qdf_mem_free(link_speed_cfg);
	}
}

bool wlan_cm_is_linkspeed_roam_trigger_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mlme_debug("Invalid WMI handle");
		return false;
	}

	return wmi_service_enabled(wmi_handle,
				   wmi_service_linkspeed_roam_trigger_support);
}
#endif

QDF_STATUS
wlan_cm_fw_roam_abort_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	return cm_fw_roam_abort_req(psoc, vdev_id);
}

uint32_t wlan_cm_get_roam_band_value(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_vdev *vdev)
{
	struct cm_roam_values_copy config;
	uint32_t band_mask;

	wlan_cm_roam_cfg_get_value(psoc, wlan_vdev_get_id(vdev), ROAM_BAND,
				   &config);

	band_mask = config.uint_value;
	mlme_debug("[ROAM BAND] band mask:%d", band_mask);
	return band_mask;
}

void wlan_cm_roam_activate_pcl_per_vdev(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, bool pcl_per_vdev)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	/* value - true (vdev pcl) false - pdev pcl */
	mlme_priv->cm_roam.pcl_vdev_cmd_active = pcl_per_vdev;
	mlme_debug("CM_ROAM: vdev[%d] SET PCL cmd level - [%s]", vdev_id,
		   pcl_per_vdev ? "VDEV" : "PDEV");
}

bool wlan_cm_roam_is_pcl_per_vdev_active(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return false;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return false;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return mlme_priv->cm_roam.pcl_vdev_cmd_active;
}

/**
 * wlan_cm_dual_sta_is_freq_allowed() - This API is used to check if the
 * provided frequency is allowed for the 2nd STA vdev for connection.
 * @psoc: Pointer to PSOC object
 * @freq: Frequency in the given frequency list for the STA that is about to
 * connect
 * @connected_sta_freq: 1st connected sta freq
 *
 * Make sure to validate the STA+STA condition before calling this
 *
 * Return: True if this channel is allowed for connection when dual sta roaming
 * is enabled
 */
static bool
wlan_cm_dual_sta_is_freq_allowed(struct wlan_objmgr_psoc *psoc,
				 qdf_freq_t freq, qdf_freq_t connected_sta_freq)
{
	if (!connected_sta_freq)
		return true;

	if (policy_mgr_2_freq_always_on_same_mac(psoc, freq,
						 connected_sta_freq))
		return false;

	return true;
}

void
wlan_cm_dual_sta_roam_update_connect_channels(struct wlan_objmgr_psoc *psoc,
					      struct scan_filter *filter)
{
	uint32_t i, num_channels = 0;
	uint32_t *channel_list;
	bool is_ch_allowed;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_mlme_cfg *mlme_cfg;
	struct dual_sta_policy *dual_sta_policy;
	uint32_t buff_len;
	char *chan_buff;
	uint32_t len = 0;
	uint32_t sta_count;
	qdf_freq_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};

	filter->num_of_channels = 0;
	sta_count = policy_mgr_get_mode_specific_conn_info(psoc, op_ch_freq_list,
							   vdev_id_list,
							   PM_STA_MODE);

	/* No need to fill freq list, if no other STA is in connected state */
	if (!sta_count)
		return;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;
	dual_sta_policy = &mlme_obj->cfg.gen.dual_sta_policy;
	mlme_cfg = &mlme_obj->cfg;

	mlme_debug("sta_count %d, primary vdev is %d dual sta roaming enabled %d",
		   sta_count, dual_sta_policy->primary_vdev_id,
		   wlan_mlme_get_dual_sta_roaming_enabled(psoc));

	if (!wlan_mlme_get_dual_sta_roaming_enabled(psoc))
		return;

	/*
	 * Check if primary iface is configured. If yes,
	 * then allow further STA connection to all
	 * available bands/channels irrespective of first
	 * STA connection band, which allow driver to
	 * connect with the best available AP present in
	 * environment, so that user can switch to second
	 * connection and mark it as primary.
	 */
	if (wlan_mlme_is_primary_interface_configured(psoc))
		return;

	/*
	 * If an ML STA exists with more than one link, allow further STA
	 * connection to all available bands/channels irrespective of existing
	 * STA connection/link band. Link that is causing MCC with the second
	 * STA can be disabled post connection.
	 * TODO: Check if disabling the MCC link is allowed or not. TID to
	 * link mapping restricts disabling the link.
	 *
	 * If only one ML link is present, allow the second STA only on other
	 * mac than this link mac. If second STA is allowed on the same mac
	 * also, it may result in MCC and the link can't be disabled
	 * post connection as only one link is present.
	 */
	if (policy_mgr_is_mlo_sta_present(psoc) && sta_count > 1) {
		mlme_debug("Multi link mlo sta present");
		return;
	}

	/*
	 * Get Reg domain valid channels and update to the scan filter
	 * if already 1st sta is in connected state. Don't allow channels
	 * on which the 1st STA is connected.
	 */
	num_channels = mlme_cfg->reg.valid_channel_list_num;
	channel_list = mlme_cfg->reg.valid_channel_freq_list;

	/*
	 * Buffer of (num channel * 5) + 1  to consider the 4 char freq,
	 * 1 space after it for each channel and 1 to end the string
	 * with NULL.
	 */
	buff_len = (num_channels * 5) + 1;
	chan_buff = qdf_mem_malloc(buff_len);
	if (!chan_buff)
		return;

	for (i = 0; i < num_channels; i++) {
		is_ch_allowed =
			wlan_cm_dual_sta_is_freq_allowed(psoc, channel_list[i],
							 op_ch_freq_list[0]);
		if (!is_ch_allowed)
			continue;

		filter->chan_freq_list[filter->num_of_channels] =
					channel_list[i];
		filter->num_of_channels++;

		len += qdf_scnprintf(chan_buff + len, buff_len - len,
				     "%d ", channel_list[i]);
	}

	if (filter->num_of_channels)
		mlme_debug("Freq list (%d): %s", filter->num_of_channels,
			   chan_buff);

	qdf_mem_free(chan_buff);
}

void
wlan_cm_roam_set_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_cm_roam_vendor_btm_params *param)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	qdf_mem_copy(&mlme_obj->cfg.lfr.vendor_btm_param, param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));
}

void
wlan_cm_roam_get_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_cm_roam_vendor_btm_params *param)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	qdf_mem_copy(param, &mlme_obj->cfg.lfr.vendor_btm_param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));
}

void wlan_cm_set_psk_pmk(struct wlan_objmgr_pdev *pdev,
			 uint8_t vdev_id, uint8_t *psk_pmk,
			 uint8_t pmk_len)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev)
		return;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return;
	}
	qdf_mem_zero(rso_cfg->psk_pmk, sizeof(rso_cfg->psk_pmk));
	if (psk_pmk)
		qdf_mem_copy(rso_cfg->psk_pmk, psk_pmk, pmk_len);
	rso_cfg->pmk_len = pmk_len;

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_MLME, QDF_TRACE_LEVEL_DEBUG,
			   rso_cfg->psk_pmk, WLAN_MAX_PMK_DUMP_BYTES);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

void wlan_cm_get_psk_pmk(struct wlan_objmgr_pdev *pdev,
			 uint8_t vdev_id, uint8_t *psk_pmk,
			 uint8_t *pmk_len)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;

	if (!psk_pmk || !pmk_len)
		return;
	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return;
	}
	qdf_mem_copy(psk_pmk, rso_cfg->psk_pmk, rso_cfg->pmk_len);
	*pmk_len = rso_cfg->pmk_len;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

void
wlan_cm_roam_get_score_delta_params(struct wlan_objmgr_psoc *psoc,
				    struct wlan_roam_triggers *params)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	params->score_delta_param[IDLE_ROAM_TRIGGER] =
			mlme_obj->cfg.trig_score_delta[IDLE_ROAM_TRIGGER];
	params->score_delta_param[BTM_ROAM_TRIGGER] =
			mlme_obj->cfg.trig_score_delta[BTM_ROAM_TRIGGER];
}

void
wlan_cm_roam_get_min_rssi_params(struct wlan_objmgr_psoc *psoc,
				 struct wlan_roam_triggers *params)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	params->min_rssi_params[DEAUTH_MIN_RSSI] =
			mlme_obj->cfg.trig_min_rssi[DEAUTH_MIN_RSSI];
	params->min_rssi_params[BMISS_MIN_RSSI] =
			mlme_obj->cfg.trig_min_rssi[BMISS_MIN_RSSI];
	params->min_rssi_params[MIN_RSSI_2G_TO_5G_ROAM] =
			mlme_obj->cfg.trig_min_rssi[MIN_RSSI_2G_TO_5G_ROAM];
}
#endif

QDF_STATUS wlan_cm_roam_cfg_get_value(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id,
				      enum roam_cfg_param roam_cfg_type,
				      struct cm_roam_values_copy *dst_config)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *src_cfg;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	qdf_mem_zero(dst_config, sizeof(*dst_config));
	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}
	src_cfg = &rso_cfg->cfg_param;
	switch (roam_cfg_type) {
	case RSSI_CHANGE_THRESHOLD:
		dst_config->int_value = rso_cfg->rescan_rssi_delta;
		break;
	case BEACON_RSSI_WEIGHT:
		dst_config->uint_value = rso_cfg->beacon_rssi_weight;
		break;
	case HI_RSSI_DELAY_BTW_SCANS:
		dst_config->uint_value = rso_cfg->hi_rssi_scan_delay;
		break;
	case EMPTY_SCAN_REFRESH_PERIOD:
		dst_config->uint_value = src_cfg->empty_scan_refresh_period;
		break;
	case SCAN_MIN_CHAN_TIME:
		dst_config->uint_value = src_cfg->min_chan_scan_time;
		break;
	case SCAN_MAX_CHAN_TIME:
		dst_config->uint_value = src_cfg->max_chan_scan_time;
		break;
	case NEIGHBOR_SCAN_PERIOD:
		dst_config->uint_value = src_cfg->neighbor_scan_period;
		break;
	case FULL_ROAM_SCAN_PERIOD:
		dst_config->uint_value = src_cfg->full_roam_scan_period;
		break;
	case ROAM_RSSI_DIFF:
		dst_config->uint_value = src_cfg->roam_rssi_diff;
		break;
	case ROAM_RSSI_DIFF_6GHZ:
		dst_config->uint_value = src_cfg->roam_rssi_diff_6ghz;
		break;
	case NEIGHBOUR_LOOKUP_THRESHOLD:
		dst_config->uint_value = src_cfg->neighbor_lookup_threshold;
		break;
	case SCAN_N_PROBE:
		dst_config->uint_value = src_cfg->roam_scan_n_probes;
		break;
	case SCAN_HOME_AWAY:
		dst_config->uint_value = src_cfg->roam_scan_home_away_time;
		break;
	case NEIGHBOUR_SCAN_REFRESH_PERIOD:
		dst_config->uint_value =
				src_cfg->neighbor_results_refresh_period;
		break;
	case ROAM_CONTROL_ENABLE:
		dst_config->bool_value = rso_cfg->roam_control_enable;
		break;
	case UAPSD_MASK:
		dst_config->uint_value = rso_cfg->uapsd_mask;
		break;
	case MOBILITY_DOMAIN:
		dst_config->bool_value = rso_cfg->mdid.mdie_present;
		dst_config->uint_value = rso_cfg->mdid.mobility_domain;
		break;
	case IS_11R_CONNECTION:
		dst_config->bool_value = rso_cfg->is_11r_assoc;
		break;
	case ADAPTIVE_11R_CONNECTION:
		dst_config->bool_value = rso_cfg->is_adaptive_11r_connection;
		break;
	case HS_20_AP:
		dst_config->bool_value = rso_cfg->hs_20_ap;
		break;
	case MBO_OCE_ENABLED_AP:
		dst_config->uint_value = rso_cfg->mbo_oce_enabled_ap;
		break;
	case IS_SINGLE_PMK:
		dst_config->bool_value = rso_cfg->is_single_pmk;
		break;
	case LOST_LINK_RSSI:
		dst_config->int_value = rso_cfg->lost_link_rssi;
		break;
	case ROAM_BAND:
		dst_config->uint_value = rso_cfg->roam_band_bitmask;
		break;
	case HI_RSSI_SCAN_RSSI_DELTA:
		dst_config->uint_value = src_cfg->hi_rssi_scan_rssi_delta;
		break;
	case ROAM_CONFIG_ENABLE:
		dst_config->bool_value = rso_cfg->roam_control_enable;
		break;
	default:
		mlme_err("Invalid roam config requested:%d", roam_cfg_type);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}

void wlan_cm_set_disable_hi_rssi(struct wlan_objmgr_pdev *pdev,
				 uint8_t vdev_id, bool value)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return;
	}

	rso_cfg->disable_hi_rssi = value;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

void wlan_cm_set_country_code(struct wlan_objmgr_pdev *pdev,
			      uint8_t vdev_id, uint8_t  *cc)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg || !cc)
		goto release_vdev_ref;

	mlme_debug("Country info from bcn IE:%c%c 0x%x", cc[0], cc[1], cc[2]);

	qdf_mem_copy(rso_cfg->country_code, cc, REG_ALPHA2_LEN + 1);

release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

QDF_STATUS wlan_cm_get_country_code(struct wlan_objmgr_pdev *pdev,
				    uint8_t vdev_id, uint8_t *cc)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg || !cc) {
		status = QDF_STATUS_E_INVAL;
		goto release_vdev_ref;
	}

	qdf_mem_copy(cc, rso_cfg->country_code, REG_ALPHA2_LEN + 1);

release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
	return status;
}

#ifdef FEATURE_WLAN_ESE
void wlan_cm_set_ese_assoc(struct wlan_objmgr_pdev *pdev,
			   uint8_t vdev_id, bool value)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return;
	}

	rso_cfg->is_ese_assoc = value;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

bool wlan_cm_get_ese_assoc(struct wlan_objmgr_pdev *pdev,
			   uint8_t vdev_id)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;
	bool ese_assoc;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return false;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return false;
	}

	ese_assoc = rso_cfg->is_ese_assoc;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return ese_assoc;
}
#endif

static QDF_STATUS
cm_roam_update_cfg(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		   uint8_t reason)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status = cm_roam_acquire_lock(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto release_ref;
	if (!MLME_IS_ROAM_STATE_RSO_ENABLED(psoc, vdev_id)) {
		mlme_debug("Update cfg received while ROAM RSO not started");
		cm_roam_release_lock(vdev);
		status = QDF_STATUS_E_INVAL;
		goto release_ref;
	}

	status = cm_roam_send_rso_cmd(psoc, vdev_id,
				      ROAM_SCAN_OFFLOAD_UPDATE_CFG, reason);
	cm_roam_release_lock(vdev);

release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return status;
}

void cm_dump_freq_list(struct rso_chan_info *chan_info)
{
	uint8_t *channel_list;
	uint8_t i = 0, j = 0;
	uint32_t buflen = CFG_VALID_CHANNEL_LIST_LEN * 4;

	channel_list = qdf_mem_malloc(buflen);
	if (!channel_list)
		return;

	if (chan_info->freq_list) {
		for (i = 0; i < chan_info->num_chan; i++) {
			if (j < buflen)
				j += qdf_scnprintf(channel_list + j, buflen - j,
						   "%d ",
						   chan_info->freq_list[i]);
			else
				break;
		}
	}

	mlme_debug("frequency list [%u]: %s", i, channel_list);
	qdf_mem_free(channel_list);
}

static uint8_t
cm_append_pref_chan_list(struct rso_chan_info *chan_info, qdf_freq_t *freq_list,
			 uint8_t num_chan)
{
	uint8_t i = 0, j = 0;

	for (i = 0; i < chan_info->num_chan; i++) {
		for (j = 0; j < num_chan; j++)
			if (chan_info->freq_list[i] == freq_list[j])
				break;

		if (j < num_chan)
			continue;
		if (num_chan == CFG_VALID_CHANNEL_LIST_LEN)
			break;
		freq_list[num_chan++] = chan_info->freq_list[i];
	}

	return num_chan;
}

/**
 * cm_modify_chan_list_based_on_band() - Modify RSO channel list based on band
 * @freq_list: Channel list coming from user space
 * @num_chan: Number of channel present in freq_list buffer
 * @band_bitmap: On basis of this band host modify RSO channel list
 *
 * Return: valid number of channel as per bandmap
 */
static uint8_t
cm_modify_chan_list_based_on_band(qdf_freq_t *freq_list, uint8_t num_chan,
				  uint32_t band_bitmap)
{
	uint8_t i = 0, valid_chan_num = 0;

	if (!(band_bitmap & BIT(REG_BAND_2G))) {
		mlme_debug("disabling 2G");
		for (i = 0; i < num_chan; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(freq_list[i]))
				freq_list[i] = 0;
		}
	}

	if (!(band_bitmap & BIT(REG_BAND_5G))) {
		mlme_debug("disabling 5G");
		for (i = 0; i < num_chan; i++) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(freq_list[i]))
				freq_list[i] = 0;
		}
	}

	if (!(band_bitmap & BIT(REG_BAND_6G))) {
		mlme_debug("disabling 6G");
		for (i = 0; i < num_chan; i++) {
			if (WLAN_REG_IS_6GHZ_CHAN_FREQ(freq_list[i]))
				freq_list[i] = 0;
		}
	}

	for (i = 0; i < num_chan; i++) {
		if (freq_list[i])
			freq_list[valid_chan_num++] = freq_list[i];
	}

	return valid_chan_num;
}

static QDF_STATUS cm_create_bg_scan_roam_channel_list(struct rso_chan_info *chan_info,
						const qdf_freq_t *chan_freq_lst,
						const uint8_t num_chan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i;

	chan_info->freq_list = qdf_mem_malloc(sizeof(qdf_freq_t) * num_chan);
	if (!chan_info->freq_list)
		return QDF_STATUS_E_NOMEM;

	chan_info->num_chan = num_chan;
	for (i = 0; i < num_chan; i++)
		chan_info->freq_list[i] = chan_freq_lst[i];

	return status;
}

/**
 * cm_remove_disabled_channels() - Remove disabled channels as per current
 * connected band
 * @vdev: vdev common object
 * @freq_list: Channel list coming from user space
 * @num_chan: Number of channel present in freq_list buffer
 *
 * Return: Number of channels as per SETBAND mask
 */
static uint32_t cm_remove_disabled_channels(struct wlan_objmgr_vdev *vdev,
					    qdf_freq_t *freq_list,
					    uint8_t num_chan)
{
	struct regulatory_channel *cur_chan_list;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	uint32_t valid_chan_num = 0;
	enum channel_state state;
	uint32_t freq, i, j;
	QDF_STATUS status;
	uint32_t filtered_lst[NUM_CHANNELS] = {0};

	cur_chan_list =
	     qdf_mem_malloc(NUM_CHANNELS * sizeof(struct regulatory_channel));
	if (!cur_chan_list)
		return 0;

	status = wlan_reg_get_current_chan_list(pdev, cur_chan_list);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(cur_chan_list);
		return 0;
	}

	for (i = 0; i < NUM_CHANNELS; i++) {
		freq = cur_chan_list[i].center_freq;
		state = wlan_reg_get_channel_state_for_pwrmode(
							pdev, freq,
							REG_CURRENT_PWR_MODE);
		if (state != CHANNEL_STATE_DISABLE &&
		    state != CHANNEL_STATE_INVALID) {
			for (j = 0; j < num_chan; j++) {
				if (freq == freq_list[j]) {
					filtered_lst[valid_chan_num++] =
								freq_list[j];
					break;
				}
			}
		}
	}

	mlme_debug("[ROAM_BAND]: num channel :%d", valid_chan_num);
	for (i = 0; i < valid_chan_num; i++)
		freq_list[i] = filtered_lst[i];

	qdf_mem_free(cur_chan_list);

	return valid_chan_num;
}

/**
 * cm_update_roam_scan_channel_list() - Update channel list as per RSO chan info
 * band bitmask
 * @psoc: Psoc common object
 * @vdev: vdev common object
 * @rso_cfg: connect config to be used to send info in RSO
 * @vdev_id: vdev id
 * @chan_info: hannel list already sent via RSO
 * @freq_list: Channel list coming from user space
 * @num_chan: Number of channel present in freq_list buffer
 * @update_preferred_chan: Decide whether to update preferred chan list or not
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_update_roam_scan_channel_list(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev,
				 struct rso_config *rso_cfg, uint8_t vdev_id,
				 struct rso_chan_info *chan_info,
				 qdf_freq_t *freq_list, uint8_t num_chan,
				 bool update_preferred_chan)
{
	uint16_t pref_chan_cnt = 0;
	uint32_t valid_chan_num = 0;
	struct cm_roam_values_copy config;
	uint32_t current_band;

	if (chan_info->num_chan) {
		mlme_debug("Current channel num: %d", chan_info->num_chan);
		cm_dump_freq_list(chan_info);
	}

	if (update_preferred_chan) {
		pref_chan_cnt = cm_append_pref_chan_list(chan_info, freq_list,
							 num_chan);
		num_chan = pref_chan_cnt;
	}

	num_chan = cm_remove_disabled_channels(vdev, freq_list, num_chan);
	if (!num_chan)
		return QDF_STATUS_E_FAILURE;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id, ROAM_BAND, &config);
	ucfg_reg_get_band(wlan_vdev_get_pdev(vdev), &current_band);
	/* No need to modify channel list if all channel is allowed */
	if (config.uint_value != current_band) {
		valid_chan_num =
			cm_modify_chan_list_based_on_band(freq_list, num_chan,
							  config.uint_value);
		if (!valid_chan_num) {
			mlme_debug("No valid channels left to send to the fw");
			return QDF_STATUS_E_FAILURE;
		}
		num_chan = valid_chan_num;
	}

	cm_flush_roam_channel_list(chan_info);
	cm_create_bg_scan_roam_channel_list(chan_info, freq_list, num_chan);

	mlme_debug("New channel num: %d", num_chan);
	cm_dump_freq_list(chan_info);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_cm_roam_cfg_set_value(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   enum roam_cfg_param roam_cfg_type,
			   struct cm_roam_values_copy *src_config)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *dst_cfg;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}
	dst_cfg = &rso_cfg->cfg_param;
	mlme_debug("roam_cfg_type %d, uint val %d int val %d bool val %d num chan %d",
		   roam_cfg_type, src_config->uint_value, src_config->int_value,
		   src_config->bool_value, src_config->chan_info.num_chan);
	switch (roam_cfg_type) {
	case RSSI_CHANGE_THRESHOLD:
		rso_cfg->rescan_rssi_delta  = src_config->uint_value;
		break;
	case BEACON_RSSI_WEIGHT:
		rso_cfg->beacon_rssi_weight = src_config->uint_value;
		break;
	case HI_RSSI_DELAY_BTW_SCANS:
		rso_cfg->hi_rssi_scan_delay = src_config->uint_value;
		break;
	case EMPTY_SCAN_REFRESH_PERIOD:
		dst_cfg->empty_scan_refresh_period = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					  REASON_EMPTY_SCAN_REF_PERIOD_CHANGED);
		break;
	case FULL_ROAM_SCAN_PERIOD:
		dst_cfg->full_roam_scan_period = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					  REASON_ROAM_FULL_SCAN_PERIOD_CHANGED);
		break;
	case ENABLE_SCORING_FOR_ROAM:
		dst_cfg->enable_scoring_for_roam = src_config->bool_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_SCORING_CRITERIA_CHANGED);
		break;
	case SCAN_MIN_CHAN_TIME:
		mlme_obj->cfg.lfr.neighbor_scan_min_chan_time =
							src_config->uint_value;
		dst_cfg->min_chan_scan_time = src_config->uint_value;
		break;
	case SCAN_MAX_CHAN_TIME:
		dst_cfg->max_chan_scan_time = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_SCAN_CH_TIME_CHANGED);
		break;
	case NEIGHBOR_SCAN_PERIOD:
		dst_cfg->neighbor_scan_period = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_SCAN_HOME_TIME_CHANGED);
		break;
	case ROAM_CONFIG_ENABLE:
		rso_cfg->roam_control_enable = src_config->bool_value;
		if (!rso_cfg->roam_control_enable)
			break;
		dst_cfg->roam_scan_period_after_inactivity = 0;
		dst_cfg->roam_inactive_data_packet_count = 0;
		dst_cfg->roam_scan_inactivity_time = 0;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_ROAM_CONTROL_CONFIG_ENABLED);
		break;
	case ROAM_PREFERRED_CHAN:
		/*
		 * In RSO update command, the specific channel list is
		 * given priority. So flush the Specific channel list if
		 * preferred channel list is received. Else the channel list
		 * type will be filled as static instead of dynamic.
		 */
		cm_flush_roam_channel_list(&dst_cfg->specific_chan_info);
		status = cm_update_roam_scan_channel_list(psoc, vdev, rso_cfg,
					vdev_id, &dst_cfg->pref_chan_info,
					src_config->chan_info.freq_list,
					src_config->chan_info.num_chan, true);
		if (QDF_IS_STATUS_ERROR(status))
			break;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_CHANNEL_LIST_CHANGED);
		break;
	case ROAM_SPECIFIC_CHAN:
		status = cm_update_roam_scan_channel_list(psoc, vdev, rso_cfg,
					vdev_id, &dst_cfg->specific_chan_info,
					src_config->chan_info.freq_list,
					src_config->chan_info.num_chan,
					false);
		if (QDF_IS_STATUS_ERROR(status))
			break;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_CHANNEL_LIST_CHANGED);
		break;
	case ROAM_RSSI_DIFF:
		dst_cfg->roam_rssi_diff = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_RSSI_DIFF_CHANGED);
		break;
	case ROAM_RSSI_DIFF_6GHZ:
		dst_cfg->roam_rssi_diff_6ghz = src_config->uint_value;
		break;
	case NEIGHBOUR_LOOKUP_THRESHOLD:
		dst_cfg->neighbor_lookup_threshold = src_config->uint_value;
		break;
	case SCAN_N_PROBE:
		dst_cfg->roam_scan_n_probes = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_NPROBES_CHANGED);
		break;
	case SCAN_HOME_AWAY:
		dst_cfg->roam_scan_home_away_time = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled &&
		    src_config->bool_value)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_HOME_AWAY_TIME_CHANGED);
		break;
	case NEIGHBOUR_SCAN_REFRESH_PERIOD:
		dst_cfg->neighbor_results_refresh_period =
						src_config->uint_value;
		mlme_obj->cfg.lfr.neighbor_scan_results_refresh_period =
				src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
				REASON_NEIGHBOR_SCAN_REFRESH_PERIOD_CHANGED);
		break;
	case UAPSD_MASK:
		rso_cfg->uapsd_mask = src_config->uint_value;
		break;
	case MOBILITY_DOMAIN:
		rso_cfg->mdid.mdie_present = src_config->bool_value;
		rso_cfg->mdid.mobility_domain = src_config->uint_value;
		break;
	case IS_11R_CONNECTION:
		rso_cfg->is_11r_assoc = src_config->bool_value;
		break;
	case ADAPTIVE_11R_CONNECTION:
		rso_cfg->is_adaptive_11r_connection = src_config->bool_value;
		break;
	case HS_20_AP:
		rso_cfg->hs_20_ap  = src_config->bool_value;
		break;
	case MBO_OCE_ENABLED_AP:
		rso_cfg->mbo_oce_enabled_ap  = src_config->uint_value;
		break;
	case IS_SINGLE_PMK:
		rso_cfg->is_single_pmk = src_config->bool_value;
		break;
	case LOST_LINK_RSSI:
		rso_cfg->lost_link_rssi = src_config->int_value;
		break;
	case ROAM_BAND:
		rso_cfg->roam_band_bitmask = src_config->uint_value;
		mlme_debug("[ROAM BAND] Set roam band:%d",
			   rso_cfg->roam_band_bitmask);
		break;
	default:
		mlme_err("Invalid roam config requested:%d", roam_cfg_type);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}

void wlan_roam_reset_roam_params(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct rso_config_params *rso_usr_cfg;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	rso_usr_cfg = &mlme_obj->cfg.lfr.rso_user_config;

	/*
	 * clear all the allowlist parameters and remaining
	 * needs to be retained across connections.
	 */
	rso_usr_cfg->num_ssid_allowed_list = 0;
	qdf_mem_zero(&rso_usr_cfg->ssid_allowed_list,
		     sizeof(struct wlan_ssid) * MAX_SSID_ALLOWED_LIST);
}

static void cm_rso_chan_to_freq_list(struct wlan_objmgr_pdev *pdev,
				     qdf_freq_t *freq_list,
				     const uint8_t *chan_list,
				     uint32_t chan_list_len)
{
	uint32_t count;

	for (count = 0; count < chan_list_len; count++)
		freq_list[count] =
			wlan_reg_legacy_chan_to_freq(pdev, chan_list[count]);
}

#ifdef WLAN_FEATURE_HOST_ROAM
static QDF_STATUS cm_init_reassoc_timer(struct rso_config *rso_cfg)
{
	QDF_STATUS status;

	status = qdf_mc_timer_init(&rso_cfg->reassoc_timer, QDF_TIMER_TYPE_SW,
				   cm_reassoc_timer_callback, &rso_cfg->ctx);

	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("Preauth Reassoc interval Timer allocation failed");

	return status;
}

static void cm_deinit_reassoc_timer(struct rso_config *rso_cfg)
{
	/* check if the timer is running */
	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&rso_cfg->reassoc_timer))
		qdf_mc_timer_stop(&rso_cfg->reassoc_timer);

	qdf_mc_timer_destroy(&rso_cfg->reassoc_timer);
}
#else
static inline QDF_STATUS cm_init_reassoc_timer(struct rso_config *rso_cfg)
{
	return QDF_STATUS_SUCCESS;
}
static inline void cm_deinit_reassoc_timer(struct rso_config *rso_cfg) {}
#endif

QDF_STATUS wlan_cm_rso_config_init(struct wlan_objmgr_vdev *vdev,
				   struct rso_config *rso_cfg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct rso_chan_info *chan_info;
	struct rso_cfg_params *cfg_params;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	uint32_t current_band = REG_BAND_MASK_ALL;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_INVAL;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_INVAL;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	status = cm_init_reassoc_timer(rso_cfg);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	qdf_mutex_create(&rso_cfg->cm_rso_lock);
	cfg_params = &rso_cfg->cfg_param;
	cfg_params->max_chan_scan_time =
		mlme_obj->cfg.lfr.neighbor_scan_max_chan_time;
	cfg_params->passive_max_chan_time =
		mlme_obj->cfg.lfr.passive_max_channel_time;
	cfg_params->min_chan_scan_time =
		mlme_obj->cfg.lfr.neighbor_scan_min_chan_time;
	cfg_params->neighbor_lookup_threshold =
		mlme_obj->cfg.lfr.neighbor_lookup_rssi_threshold;
	cfg_params->rssi_thresh_offset_5g =
		mlme_obj->cfg.lfr.rssi_threshold_offset_5g;
	cfg_params->opportunistic_threshold_diff =
		mlme_obj->cfg.lfr.opportunistic_scan_threshold_diff;
	cfg_params->roam_rescan_rssi_diff =
		mlme_obj->cfg.lfr.roam_rescan_rssi_diff;

	cfg_params->roam_bmiss_first_bcn_cnt =
		mlme_obj->cfg.lfr.roam_bmiss_first_bcnt;
	cfg_params->roam_bmiss_final_cnt =
		mlme_obj->cfg.lfr.roam_bmiss_final_bcnt;

	cfg_params->neighbor_scan_period =
		mlme_obj->cfg.lfr.neighbor_scan_timer_period;
	cfg_params->neighbor_scan_min_period =
		mlme_obj->cfg.lfr.neighbor_scan_min_timer_period;
	cfg_params->neighbor_results_refresh_period =
		mlme_obj->cfg.lfr.neighbor_scan_results_refresh_period;
	cfg_params->empty_scan_refresh_period =
		mlme_obj->cfg.lfr.empty_scan_refresh_period;
	cfg_params->full_roam_scan_period =
		mlme_obj->cfg.lfr.roam_full_scan_period;
	cfg_params->enable_scoring_for_roam =
		mlme_obj->cfg.roam_scoring.enable_scoring_for_roam;
	cfg_params->roam_scan_n_probes =
		mlme_obj->cfg.lfr.roam_scan_n_probes;
	cfg_params->roam_scan_home_away_time =
		mlme_obj->cfg.lfr.roam_scan_home_away_time;
	cfg_params->roam_scan_inactivity_time =
		mlme_obj->cfg.lfr.roam_scan_inactivity_time;
	cfg_params->roam_inactive_data_packet_count =
		mlme_obj->cfg.lfr.roam_inactive_data_packet_count;
	cfg_params->roam_scan_period_after_inactivity =
		mlme_obj->cfg.lfr.roam_scan_period_after_inactivity;

	chan_info = &cfg_params->specific_chan_info;
	chan_info->num_chan =
		mlme_obj->cfg.lfr.neighbor_scan_channel_list_num;
	mlme_debug("number of channels: %u", chan_info->num_chan);
	if (chan_info->num_chan) {
		chan_info->freq_list =
			qdf_mem_malloc(sizeof(qdf_freq_t) *
				       chan_info->num_chan);
		if (!chan_info->freq_list) {
			chan_info->num_chan = 0;
			return QDF_STATUS_E_NOMEM;
		}
		/* Update the roam global structure from CFG */
		cm_rso_chan_to_freq_list(pdev, chan_info->freq_list,
			mlme_obj->cfg.lfr.neighbor_scan_channel_list,
			mlme_obj->cfg.lfr.neighbor_scan_channel_list_num);
	} else {
		chan_info->freq_list = NULL;
	}

	cfg_params->hi_rssi_scan_max_count =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_maxcount;
	cfg_params->hi_rssi_scan_rssi_delta =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_delta;

	cfg_params->hi_rssi_scan_delay =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_delay;

	cfg_params->hi_rssi_scan_rssi_ub =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_ub;
	cfg_params->roam_rssi_diff =
		mlme_obj->cfg.lfr.roam_rssi_diff;
	cfg_params->roam_rssi_diff_6ghz =
		mlme_obj->cfg.lfr.roam_rssi_diff_6ghz;
	cfg_params->bg_rssi_threshold =
		mlme_obj->cfg.lfr.bg_rssi_threshold;

	ucfg_reg_get_band(wlan_vdev_get_pdev(vdev), &current_band);
	rso_cfg->roam_band_bitmask = current_band;

	return status;
}

void wlan_cm_rso_config_deinit(struct wlan_objmgr_vdev *vdev,
			       struct rso_config *rso_cfg)
{
	struct rso_cfg_params *cfg_params;

	cfg_params = &rso_cfg->cfg_param;
	if (rso_cfg->assoc_ie.ptr) {
		qdf_mem_free(rso_cfg->assoc_ie.ptr);
		rso_cfg->assoc_ie.ptr = NULL;
		rso_cfg->assoc_ie.len = 0;
	}
	if (rso_cfg->prev_ap_bcn_ie.ptr) {
		qdf_mem_free(rso_cfg->prev_ap_bcn_ie.ptr);
		rso_cfg->prev_ap_bcn_ie.ptr = NULL;
		rso_cfg->prev_ap_bcn_ie.len = 0;
	}
	if (rso_cfg->roam_scan_freq_lst.freq_list)
		qdf_mem_free(rso_cfg->roam_scan_freq_lst.freq_list);
	rso_cfg->roam_scan_freq_lst.freq_list = NULL;
	rso_cfg->roam_scan_freq_lst.num_chan = 0;

	cm_flush_roam_channel_list(&cfg_params->specific_chan_info);
	cm_flush_roam_channel_list(&cfg_params->pref_chan_info);

	qdf_mutex_destroy(&rso_cfg->cm_rso_lock);

	cm_deinit_reassoc_timer(rso_cfg);
}

struct rso_config *wlan_cm_get_rso_config_fl(struct wlan_objmgr_vdev *vdev,
					     const char *func, uint32_t line)

{
	struct cm_ext_obj *cm_ext_obj;
	enum QDF_OPMODE op_mode = wlan_vdev_mlme_get_opmode(vdev);

	/* get only for CLI and STA */
	if (op_mode != QDF_STA_MODE && op_mode != QDF_P2P_CLIENT_MODE)
		return NULL;

	cm_ext_obj = cm_get_ext_hdl_fl(vdev, func, line);
	if (!cm_ext_obj)
		return NULL;

	return &cm_ext_obj->rso_cfg;
}

QDF_STATUS cm_roam_acquire_lock(struct wlan_objmgr_vdev *vdev)
{
	static struct rso_config *rso_cfg;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		return QDF_STATUS_E_INVAL;

	return qdf_mutex_acquire(&rso_cfg->cm_rso_lock);
}

QDF_STATUS cm_roam_release_lock(struct wlan_objmgr_vdev *vdev)
{
	static struct rso_config *rso_cfg;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		return QDF_STATUS_E_INVAL;

	return qdf_mutex_release(&rso_cfg->cm_rso_lock);
}

QDF_STATUS
wlan_cm_roam_invoke(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
		    struct qdf_mac_addr *bssid, qdf_freq_t chan_freq,
		    enum wlan_cm_source source)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		mlme_err("Invalid psoc");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = cm_start_roam_invoke(psoc, vdev, bssid, chan_freq, source);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}

bool cm_is_fast_roam_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	if (!mlme_obj->cfg.lfr.lfr_enabled)
		return false;

	if (mlme_obj->cfg.lfr.enable_fast_roam_in_concurrency)
		return true;
	/* return true if no concurrency */
	if (policy_mgr_get_connection_count(psoc) < 2)
		return true;

	return false;
}

bool cm_is_rsn_or_8021x_sha256_auth_type(struct wlan_objmgr_vdev *vdev)
{
	int32_t akm;

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256) ||
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X))
		return true;

	return false;
}

#ifdef WLAN_FEATURE_HOST_ROAM
QDF_STATUS wlan_cm_host_roam_start(struct scheduler_msg *msg)
{
	QDF_STATUS status;
	struct cm_host_roam_start_ind *req;
	struct qdf_mac_addr bssid = QDF_MAC_ADDR_ZERO_INIT;

	if (!msg || !msg->bodyptr)
		return QDF_STATUS_E_FAILURE;

	req = msg->bodyptr;
	status = wlan_cm_roam_invoke(req->pdev, req->vdev_id, &bssid, 0,
				     CM_ROAMING_FW);
	qdf_mem_free(req);

	return status;
}

QDF_STATUS cm_mlme_roam_preauth_fail(struct wlan_objmgr_vdev *vdev,
				     struct wlan_cm_roam_req *req,
				     enum wlan_cm_connect_fail_reason reason)
{
	uint8_t vdev_id, roam_reason;
	struct wlan_objmgr_pdev *pdev;

	if (!vdev || !req) {
		mlme_err("vdev or req is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (reason == CM_NO_CANDIDATE_FOUND)
		roam_reason = REASON_NO_CAND_FOUND_OR_NOT_ROAMING_NOW;
	else
		roam_reason = REASON_PREAUTH_FAILED_FOR_ALL;

	pdev = wlan_vdev_get_pdev(vdev);
	vdev_id = wlan_vdev_get_id(vdev);

	if (req->source == CM_ROAMING_FW)
		cm_roam_state_change(pdev, vdev_id,
				     ROAM_SCAN_OFFLOAD_RESTART,
				     roam_reason, NULL, false);
	else
		cm_roam_state_change(pdev, vdev_id,
				     ROAM_SCAN_OFFLOAD_START,
				     roam_reason, NULL, false);
	return QDF_STATUS_SUCCESS;
}
#endif

void wlan_cm_fill_crypto_filter_from_vdev(struct wlan_objmgr_vdev *vdev,
					  struct scan_filter *filter)
{
	struct rso_config *rso_cfg;

	filter->authmodeset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_AUTH_MODE);
	filter->mcastcipherset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_MCAST_CIPHER);
	filter->ucastcipherset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
	filter->key_mgmt =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	filter->mgmtcipherset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_MGMT_CIPHER);

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		return;

	if (rso_cfg->orig_sec_info.rsn_caps &
	    WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED)
		filter->pmf_cap = WLAN_PMF_REQUIRED;
	else if (rso_cfg->orig_sec_info.rsn_caps &
		 WLAN_CRYPTO_RSN_CAP_MFP_ENABLED)
		filter->pmf_cap = WLAN_PMF_CAPABLE;
}

static void cm_dump_occupied_chan_list(struct wlan_chan_list *occupied_ch)
{
	uint8_t idx;
	uint32_t buff_len;
	char *chan_buff;
	uint32_t len = 0;

	buff_len = (occupied_ch->num_chan * 5) + 1;
	chan_buff = qdf_mem_malloc(buff_len);
	if (!chan_buff)
		return;

	for (idx = 0; idx < occupied_ch->num_chan; idx++)
		len += qdf_scnprintf(chan_buff + len, buff_len - len, " %d",
				     occupied_ch->freq_list[idx]);

	mlme_nofl_debug("Occupied chan list[%d]:%s",
			occupied_ch->num_chan, chan_buff);

	qdf_mem_free(chan_buff);
}

/**
 * cm_should_add_to_occupied_channels() - validates bands of active_ch_freq and
 * curr node freq before addition of curr node freq to occupied channels
 *
 * @active_ch_freq: active channel frequency
 * @cur_node_chan_freq: curr channel frequency
 * @dual_sta_roam_active: dual sta roam active
 *
 * Return: True if active_ch_freq and cur_node_chan_freq belongs to same
 * bands else false
 **/
static bool cm_should_add_to_occupied_channels(qdf_freq_t active_ch_freq,
					       qdf_freq_t cur_node_chan_freq,
					       bool dual_sta_roam_active)
{
	/* all channels can be added if dual STA roam is not active */
	if (!dual_sta_roam_active)
		return true;

	/* when dual STA roam is active, channels must be in the same band */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(active_ch_freq) &&
	    WLAN_REG_IS_24GHZ_CH_FREQ(cur_node_chan_freq))
		return true;

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(active_ch_freq) &&
	    !WLAN_REG_IS_24GHZ_CH_FREQ(cur_node_chan_freq))
		return true;

	/* not in same band */
	return false;
}

static QDF_STATUS cm_add_to_freq_list_front(qdf_freq_t *ch_freq_lst,
					    int num_chan, qdf_freq_t chan_freq)
{
	int i = 0;

	/* Check for NULL pointer */
	if (!ch_freq_lst)
		return QDF_STATUS_E_NULL_VALUE;

	/* Make room for the addition.  (Start moving from the back.) */
	for (i = num_chan; i > 0; i--)
		ch_freq_lst[i] = ch_freq_lst[i - 1];

	/* Now add the NEW channel...at the front */
	ch_freq_lst[0] = chan_freq;

	return QDF_STATUS_SUCCESS;
}

/* Add the channel to the occupied channels array */
static void cm_add_to_occupied_channels(qdf_freq_t ch_freq,
					struct rso_config *rso_cfg,
					bool is_init_list)
{
	QDF_STATUS status;
	uint8_t num_occupied_ch = rso_cfg->occupied_chan_lst.num_chan;
	qdf_freq_t *occupied_ch_lst = rso_cfg->occupied_chan_lst.freq_list;

	if (is_init_list)
		rso_cfg->roam_candidate_count++;

	if (wlan_is_channel_present_in_list(occupied_ch_lst,
					    num_occupied_ch, ch_freq))
		return;

	if (num_occupied_ch >= CFG_VALID_CHANNEL_LIST_LEN)
		num_occupied_ch = CFG_VALID_CHANNEL_LIST_LEN - 1;

	status = cm_add_to_freq_list_front(occupied_ch_lst,
					   num_occupied_ch, ch_freq);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		rso_cfg->occupied_chan_lst.num_chan++;
		if (rso_cfg->occupied_chan_lst.num_chan >
		    BG_SCAN_OCCUPIED_CHANNEL_LIST_LEN)
			rso_cfg->occupied_chan_lst.num_chan =
				BG_SCAN_OCCUPIED_CHANNEL_LIST_LEN;
	}
}

void wlan_cm_init_occupied_ch_freq_list(struct wlan_objmgr_pdev *pdev,
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id)
{
	qdf_list_t *list = NULL;
	qdf_list_node_t *cur_lst = NULL;
	qdf_list_node_t *next_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	struct scan_filter *filter;
	bool dual_sta_roam_active;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *cfg_params;
	struct wlan_ssid ssid;
	qdf_freq_t op_freq, freq;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", vdev_id);
		return;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		goto rel_vdev_ref;
	op_freq = wlan_get_operation_chan_freq(vdev);
	if (!op_freq) {
		mlme_debug("failed to get op freq");
		goto rel_vdev_ref;
	}
	status = wlan_vdev_mlme_get_ssid(vdev, ssid.ssid, &ssid.length);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("failed to find SSID for vdev %d", vdev_id);
		goto rel_vdev_ref;
	}

	cfg_params = &rso_cfg->cfg_param;

	if (cfg_params->specific_chan_info.num_chan) {
		/*
		 * Ini file contains neighbor scan channel list, hence NO need
		 * to build occupied channel list"
		 */
		mlme_debug("Ini contains neighbor scan ch list");
		goto rel_vdev_ref;
	}

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		goto rel_vdev_ref;

	wlan_cm_fill_crypto_filter_from_vdev(vdev, filter);
	filter->num_of_ssid = 1;
	qdf_mem_copy(&filter->ssid_list[0], &ssid, sizeof(ssid));

	/* Empty occupied channels here */
	rso_cfg->occupied_chan_lst.num_chan = 0;
	rso_cfg->roam_candidate_count = 0;

	cm_add_to_occupied_channels(op_freq, rso_cfg, true);
	list = wlan_scan_get_result(pdev, filter);
	qdf_mem_free(filter);
	if (!list || (list && !qdf_list_size(list)))
		goto err;

	dual_sta_roam_active =
			wlan_mlme_get_dual_sta_roaming_enabled(psoc);
	dual_sta_roam_active = dual_sta_roam_active &&
			       policy_mgr_mode_specific_connection_count
				(psoc, PM_STA_MODE, NULL) >= 2;

	qdf_list_peek_front(list, &cur_lst);
	while (cur_lst) {
		cur_node = qdf_container_of(cur_lst, struct scan_cache_node,
					    node);
		freq = cur_node->entry->channel.chan_freq;
		if (cm_should_add_to_occupied_channels(op_freq, freq,
						       dual_sta_roam_active))
			cm_add_to_occupied_channels(freq, rso_cfg, true);

		qdf_list_peek_next(list, cur_lst, &next_lst);
		cur_lst = next_lst;
		next_lst = NULL;
	}
err:
	cm_dump_occupied_chan_list(&rso_cfg->occupied_chan_lst);
	if (list)
		wlan_scan_purge_results(list);
rel_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

#ifdef WLAN_FEATURE_FILS_SK
QDF_STATUS
wlan_cm_update_mlme_fils_info(struct wlan_objmgr_vdev *vdev,
			      struct wlan_fils_con_info *src_fils_info)
{
	struct mlme_legacy_priv *mlme_priv;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct wlan_fils_connection_info *tgt_info;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL for vdev %d",
			 vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!src_fils_info) {
		mlme_debug("FILS: vdev:%d Clear fils info", vdev_id);
		qdf_mem_free(mlme_priv->connect_info.fils_con_info);
		mlme_priv->connect_info.fils_con_info = NULL;
		return QDF_STATUS_SUCCESS;
	}

	if (mlme_priv->connect_info.fils_con_info)
		qdf_mem_free(mlme_priv->connect_info.fils_con_info);

	mlme_priv->connect_info.fils_con_info =
		qdf_mem_malloc(sizeof(struct wlan_fils_connection_info));
	if (!mlme_priv->connect_info.fils_con_info)
		return QDF_STATUS_E_NOMEM;

	tgt_info = mlme_priv->connect_info.fils_con_info;
	mlme_debug("FILS: vdev:%d update fils info", vdev_id);
	tgt_info->is_fils_connection = src_fils_info->is_fils_connection;
	tgt_info->key_nai_length = src_fils_info->username_len;
	qdf_mem_copy(tgt_info->keyname_nai, src_fils_info->username,
		     tgt_info->key_nai_length);

	tgt_info->realm_len = src_fils_info->realm_len;
	qdf_mem_copy(tgt_info->realm, src_fils_info->realm,
		     tgt_info->realm_len);

	tgt_info->r_rk_length = src_fils_info->rrk_len;
	qdf_mem_copy(tgt_info->r_rk, src_fils_info->rrk,
		     tgt_info->r_rk_length);
	tgt_info->erp_sequence_number = src_fils_info->next_seq_num;
	tgt_info->auth_type = src_fils_info->auth_type;

	return QDF_STATUS_SUCCESS;
}

void wlan_cm_update_hlp_info(struct wlan_objmgr_psoc *psoc,
			     const uint8_t *gen_ie, uint16_t len,
			     uint8_t vdev_id, bool flush)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev_id %d", vdev_id);
		return;
	}

	cm_update_hlp_info(vdev, gen_ie, len, flush);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

struct wlan_fils_connection_info *wlan_cm_get_fils_connection_info(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_fils_connection_info *fils_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return NULL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return NULL;
	}

	fils_info = mlme_priv->connect_info.fils_con_info;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return fils_info;
}

QDF_STATUS wlan_cm_update_fils_ft(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id, uint8_t *fils_ft,
				  uint8_t fils_ft_len)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	if (!mlme_priv->connect_info.fils_con_info || !fils_ft ||
	    !fils_ft_len ||
	    !mlme_priv->connect_info.fils_con_info->is_fils_connection) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->connect_info.fils_con_info->fils_ft_len = fils_ft_len;
	qdf_mem_copy(mlme_priv->connect_info.fils_con_info->fils_ft, fils_ft,
		     fils_ft_len);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}
#endif

void wlan_cm_get_associated_ch_info(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    struct connect_chan_info *chan_info)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	chan_info->ch_width_orig = CH_WIDTH_INVALID;
	chan_info->sec_2g_freq = 0;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		goto release;

	chan_info->ch_width_orig =
			mlme_priv->connect_info.chan_info_orig.ch_width_orig;
	chan_info->sec_2g_freq =
			mlme_priv->connect_info.chan_info_orig.sec_2g_freq;

	mlme_debug("vdev %d: associated_ch_width:%d, sec_2g_freq:%d", vdev_id,
		   chan_info->ch_width_orig, chan_info->sec_2g_freq);
release:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_cm_update_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id,
				       uint32_t roam_scan_scheme_bitmap)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}
	rso_cfg->roam_scan_scheme_bitmap = roam_scan_scheme_bitmap;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cm_set_roam_band_bitmask(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id,
					 uint32_t roam_band_bitmask)
{
	struct cm_roam_values_copy src_config = {};

	src_config.uint_value = roam_band_bitmask;
	return wlan_cm_roam_cfg_set_value(psoc, vdev_id, ROAM_BAND,
					  &src_config);
}

QDF_STATUS wlan_cm_set_roam_band_update(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id)
{
	return cm_roam_update_cfg(psoc, vdev_id,
				  REASON_ROAM_CONTROL_CONFIG_ENABLED);
}

uint32_t wlan_cm_get_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
					     uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	uint32_t roam_scan_scheme_bitmap;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return 0;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return 0;
	}

	roam_scan_scheme_bitmap = rso_cfg->roam_scan_scheme_bitmap;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return roam_scan_scheme_bitmap;
}

QDF_STATUS
wlan_cm_update_roam_states(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   uint32_t value, enum roam_fail_params states)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	switch (states) {
	case ROAM_TRIGGER_REASON:
		rso_cfg->roam_trigger_reason = value;
		break;
	case ROAM_INVOKE_FAIL_REASON:
		rso_cfg->roam_invoke_fail_reason = value;
		break;
	case ROAM_FAIL_REASON:
		rso_cfg->roam_fail_reason = value;
		break;
	default:
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

uint32_t wlan_cm_get_roam_states(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 enum roam_fail_params states)
{
	struct wlan_objmgr_vdev *vdev;
	uint32_t roam_states = 0;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return 0;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return 0;
	}

	switch (states) {
	case ROAM_TRIGGER_REASON:
		roam_states = rso_cfg->roam_trigger_reason;
		break;
	case ROAM_INVOKE_FAIL_REASON:
		roam_states = rso_cfg->roam_invoke_fail_reason;
		break;
	case ROAM_FAIL_REASON:
		roam_states = rso_cfg->roam_fail_reason;
		break;
	default:
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return roam_states;
}

QDF_STATUS
wlan_cm_update_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
			     uint8_t value, enum roam_rt_stats_params stats)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_cm_roam_rt_stats *roam_rt_stats;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	roam_rt_stats = &mlme_obj->cfg.lfr.roam_rt_stats;

	switch (stats) {
	case ROAM_RT_STATS_ENABLE:
		roam_rt_stats->roam_stats_enabled = value;
		break;
	case ROAM_RT_STATS_SUSPEND_MODE_ENABLE:
		roam_rt_stats->roam_stats_wow_sent = value;
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

uint8_t wlan_cm_get_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
				  enum roam_rt_stats_params stats)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_cm_roam_rt_stats *roam_rt_stats;
	uint8_t rstats_value = 0;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	roam_rt_stats = &mlme_obj->cfg.lfr.roam_rt_stats;
	switch (stats) {
	case ROAM_RT_STATS_ENABLE:
		rstats_value = roam_rt_stats->roam_stats_enabled;
		break;
	case ROAM_RT_STATS_SUSPEND_MODE_ENABLE:
		rstats_value = roam_rt_stats->roam_stats_wow_sent;
		break;
	default:
		break;
	}

	return rstats_value;
}
#endif

QDF_STATUS wlan_get_chan_by_bssid_from_rnr(struct wlan_objmgr_vdev *vdev,
					   wlan_cm_id cm_id,
					   struct qdf_mac_addr *link_addr,
					   uint8_t *chan, uint8_t *op_class)
{
	struct reduced_neighbor_report *rnr;
	int i;

	*chan = 0;

	rnr = wlan_cm_get_rnr(vdev, cm_id);

	if (!rnr) {
		mlme_err("no rnr IE is gotten");
		return QDF_STATUS_E_EMPTY;
	}

	for (i = 0; i < MAX_RNR_BSS; i++) {
		if (!rnr->bss_info[i].channel_number)
			continue;
		if (qdf_is_macaddr_equal(link_addr, &rnr->bss_info[i].bssid)) {
			*chan = rnr->bss_info[i].channel_number;
			*op_class = rnr->bss_info[i].operating_class;
			break;
		}
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * mlo_rnr_link_id_cmp() - compare given link id with link id in rnr
 * @rnr_bss_info: rnr bss info
 * @link_id: link id
 *
 * Return: true if given link id is the same with link id in rnr
 */
static bool mlo_rnr_link_id_cmp(struct rnr_bss_info *rnr_bss_info,
				uint8_t link_id)
{
	if (rnr_bss_info)
		return link_id == rnr_bss_info->mld_info.link_id;

	return false;
}

QDF_STATUS wlan_get_chan_by_link_id_from_rnr(struct wlan_objmgr_vdev *vdev,
					     wlan_cm_id cm_id,
					     uint8_t link_id,
					     uint8_t *chan, uint8_t *op_class)
{
	struct reduced_neighbor_report *rnr;
	int i;

	*chan = 0;

	rnr = wlan_cm_get_rnr(vdev, cm_id);

	if (!rnr) {
		mlme_err("no rnr IE is gotten");
		return QDF_STATUS_E_EMPTY;
	}

	for (i = 0; i < MAX_RNR_BSS; i++) {
		if (!rnr->bss_info[i].channel_number)
			continue;
		if (mlo_rnr_link_id_cmp(&rnr->bss_info[i], link_id)) {
			*chan = rnr->bss_info[i].channel_number;
			*op_class = rnr->bss_info[i].operating_class;
			break;
		}
	}

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlan_cm_sta_mlme_vdev_roam_notify(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t data_len, void *data)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	status = cm_roam_sync_event_handler_cb(vdev_mlme->vdev, data, data_len);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("Failed to process roam synch event");
#endif
	return status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void
cm_handle_roam_offload_events(struct roam_offload_roam_event *roam_event)
{
	switch (roam_event->reason) {
	case ROAM_REASON_HO_FAILED: {
		struct qdf_mac_addr bssid;

		bssid.bytes[0] = roam_event->notif_params >> 0 & 0xFF;
		bssid.bytes[1] = roam_event->notif_params >> 8 & 0xFF;
		bssid.bytes[2] = roam_event->notif_params >> 16 & 0xFF;
		bssid.bytes[3] = roam_event->notif_params >> 24 & 0xFF;
		bssid.bytes[4] = roam_event->notif_params1 >> 0 & 0xFF;
		bssid.bytes[5] = roam_event->notif_params1 >> 8 & 0xFF;
		cm_handle_roam_reason_ho_failed(roam_event->vdev_id, bssid,
						roam_event->hw_mode_trans_ind);
	}
	break;
	case ROAM_REASON_INVALID:
		cm_invalid_roam_reason_handler(roam_event->vdev_id,
					       roam_event->notif);
		break;
	default:
		break;
	}
}

QDF_STATUS
cm_vdev_disconnect_event_handler(struct vdev_disconnect_event_data *data)
{
	return cm_handle_disconnect_reason(data);
}

QDF_STATUS
cm_roam_auth_offload_event_handler(struct auth_offload_event *auth_event)
{
	return cm_handle_auth_offload(auth_event);
}

QDF_STATUS
cm_roam_pmkid_request_handler(struct roam_pmkid_req_event *data)
{
	QDF_STATUS status;

	status = cm_roam_pmkid_req_ind(data->psoc, data->vdev_id, data);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("Pmkid request failed");

	return status;
}
#else
static void
cm_handle_roam_offload_events(struct roam_offload_roam_event *roam_event)
{
	mlme_debug("Unhandled roam event with reason 0x%x for vdev_id %u",
		   roam_event->reason, roam_event->vdev_id);
}

QDF_STATUS
cm_vdev_disconnect_event_handler(struct vdev_disconnect_event_data *data)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_roam_auth_offload_event_handler(struct auth_offload_event *auth_event)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_roam_pmkid_request_handler(struct roam_pmkid_req_event *data)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
void
cm_roam_vendor_handoff_event_handler(struct wlan_objmgr_psoc *psoc,
				     struct roam_vendor_handoff_params *data)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	void *vendor_handoff_context;
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, data->vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", data->vdev_id);
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;

	vendor_handoff_context =
		mlme_priv->cm_roam.vendor_handoff_param.vendor_handoff_context;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	status = mlme_cm_osif_get_vendor_handoff_params(psoc,
							vendor_handoff_context);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("Failed to free vendor handoff request");
		return;
	}

	mlme_debug("Reset vendor handoff req in progress context");
	mlme_priv->cm_roam.vendor_handoff_param.req_in_progress = false;
	mlme_priv->cm_roam.vendor_handoff_param.vendor_handoff_context = NULL;

	status = cm_roam_update_vendor_handoff_config(psoc, data);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("Failed to update params in rso_config struct");
}
#endif

QDF_STATUS
cm_roam_event_handler(struct roam_offload_roam_event *roam_event)
{
	switch (roam_event->reason) {
	case ROAM_REASON_BTM:
		cm_handle_roam_reason_btm(roam_event->vdev_id);
		break;
	case ROAM_REASON_BMISS:
		cm_handle_roam_reason_bmiss(roam_event->vdev_id,
					    roam_event->rssi);
		break;
	case ROAM_REASON_BETTER_AP:
		cm_handle_roam_reason_better_ap(roam_event->vdev_id,
						roam_event->rssi);
		break;
	case ROAM_REASON_SUITABLE_AP:
		cm_handle_roam_reason_suitable_ap(roam_event->vdev_id,
						  roam_event->rssi);
		break;
	case ROAM_REASON_HO_FAILED:
		/*
		 * Continue disconnect only if RSO_STOP timer is running when
		 * this event is received and stopped as part of this.
		 * Otherwise it's a normal HO_FAIL event and handle it in
		 * legacy way.
		 */
		if (roam_event->rso_timer_stopped)
			wlan_cm_rso_stop_continue_disconnect(roam_event->psoc,
						roam_event->vdev_id, true);
		fallthrough;
	case ROAM_REASON_INVALID:
		cm_handle_roam_offload_events(roam_event);
		break;
	case ROAM_REASON_RSO_STATUS:
		/*
		 * roam_event->rso_timer_stopped is set to true in target_if
		 * only if RSO_STOP timer is running and it's stopped
		 * successfully
		 */
		if (roam_event->rso_timer_stopped &&
		    (roam_event->notif == CM_ROAM_NOTIF_SCAN_MODE_SUCCESS ||
		     roam_event->notif == CM_ROAM_NOTIF_SCAN_MODE_FAIL))
			wlan_cm_rso_stop_continue_disconnect(roam_event->psoc,
						roam_event->vdev_id, false);
		cm_rso_cmd_status_event_handler(roam_event->vdev_id,
						roam_event->notif);
		break;
	case ROAM_REASON_INVOKE_ROAM_FAIL:
		cm_handle_roam_reason_invoke_roam_fail(roam_event->vdev_id,
						roam_event->notif_params,
						roam_event->hw_mode_trans_ind);
		break;
	case ROAM_REASON_DEAUTH:
		cm_handle_roam_reason_deauth(roam_event->vdev_id,
					     roam_event->notif_params,
					     roam_event->deauth_disassoc_frame,
					     roam_event->notif_params1);
		break;
	default:
		mlme_debug("Unhandled roam event with reason 0x%x for vdev_id %u",
			   roam_event->reason, roam_event->vdev_id);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

static void
cm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
			    struct sir_rssi_disallow_lst *entry)
{
	struct reject_ap_info ap_info;

	qdf_mem_zero(&ap_info, sizeof(struct reject_ap_info));

	ap_info.bssid = entry->bssid;
	ap_info.reject_ap_type = DRIVER_RSSI_REJECT_TYPE;
	ap_info.rssi_reject_params.expected_rssi = entry->expected_rssi;
	ap_info.rssi_reject_params.retry_delay = entry->retry_delay;
	ap_info.reject_reason = entry->reject_reason;
	ap_info.source = entry->source;
	ap_info.rssi_reject_params.received_time = entry->received_time;
	ap_info.rssi_reject_params.original_timeout = entry->original_timeout;
	/* Add this ap info to the rssi reject ap type in denylist manager */
	wlan_dlm_add_bssid_to_reject_list(pdev, &ap_info);
}

QDF_STATUS
cm_btm_denylist_event_handler(struct wlan_objmgr_psoc *psoc,
			      struct roam_denylist_event *list)
{
	uint32_t i, pdev_id;
	struct sir_rssi_disallow_lst entry;
	struct roam_denylist_timeout *denylist;
	struct wlan_objmgr_pdev *pdev;

	pdev_id = wlan_get_pdev_id_from_vdev_id(psoc, list->vdev_id,
						WLAN_MLME_CM_ID);
	if (pdev_id == WLAN_INVALID_PDEV_ID) {
		mlme_err("Invalid pdev id");
		return QDF_STATUS_E_INVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, pdev_id, WLAN_MLME_CM_ID);
	if (!pdev) {
		mlme_err("Invalid pdev");
		return QDF_STATUS_E_INVAL;
	}

	mlme_debug("Received Denylist event from FW num entries %d",
		   list->num_entries);
	denylist = &list->roam_denylist[0];
	for (i = 0; i < list->num_entries; i++) {
		qdf_mem_zero(&entry, sizeof(struct sir_rssi_disallow_lst));
		entry.bssid = denylist->bssid;
		entry.time_during_rejection = denylist->received_time;
		entry.reject_reason = denylist->reject_reason;
		entry.source = denylist->source ? denylist->source :
						   ADDED_BY_TARGET;
		entry.original_timeout = denylist->original_timeout;
		entry.received_time = denylist->received_time;
		/* If timeout = 0 and rssi = 0 ignore the entry */
		if (!denylist->timeout && !denylist->rssi) {
			continue;
		} else if (denylist->timeout) {
			entry.retry_delay = denylist->timeout;
			/* set 0dbm as expected rssi */
			entry.expected_rssi = CM_MIN_RSSI;
		} else {
			/* denylist timeout as 0 */
			entry.retry_delay = denylist->timeout;
			entry.expected_rssi = denylist->rssi;
		}

		/* Add this bssid to the rssi reject ap type in denylist mgr */
		cm_add_bssid_to_reject_list(pdev, &entry);
		denylist++;
	}
	wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_CM_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_roam_scan_ch_list_event_handler(struct cm_roam_scan_ch_resp *data)
{
	return cm_handle_scan_ch_list_data(data);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * cm_roam_stats_get_trigger_detail_str - Return roam trigger string from the
 * enum roam_trigger_reason
 * @ptr: Pointer to the roam trigger info
 * @buf: Destination buffer to write the reason string
 * @is_full_scan: Is roam scan partial scan or all channels scan
 * @vdev_id: vdev id
 *
 * Return: None
 */
static void
cm_roam_stats_get_trigger_detail_str(struct wmi_roam_trigger_info *ptr,
				     char *buf, bool is_full_scan,
				     uint8_t vdev_id)
{
	uint16_t buf_cons, buf_left = MAX_ROAM_DEBUG_BUF_SIZE;
	char *temp = buf;

	buf_cons = qdf_snprint(temp, buf_left, "Reason: \"%s\" ",
			       mlme_get_roam_trigger_str(ptr->trigger_reason));
	temp += buf_cons;
	buf_left -= buf_cons;

	if (ptr->trigger_sub_reason) {
		buf_cons = qdf_snprint(temp, buf_left, "Sub-Reason: %s",
			      mlme_get_sub_reason_str(ptr->trigger_sub_reason));
		temp += buf_cons;
		buf_left -= buf_cons;
	}

	switch (ptr->trigger_reason) {
	case ROAM_TRIGGER_REASON_PER:
	case ROAM_TRIGGER_REASON_BMISS:
	case ROAM_TRIGGER_REASON_HIGH_RSSI:
	case ROAM_TRIGGER_REASON_MAWC:
	case ROAM_TRIGGER_REASON_DENSE:
	case ROAM_TRIGGER_REASON_BACKGROUND:
	case ROAM_TRIGGER_REASON_IDLE:
	case ROAM_TRIGGER_REASON_FORCED:
	case ROAM_TRIGGER_REASON_UNIT_TEST:
		break;
	case ROAM_TRIGGER_REASON_BTM:
		cm_roam_btm_req_event(&ptr->btm_trig_data, ptr, vdev_id, false);
		buf_cons = qdf_snprint(
				temp, buf_left,
				"Req_mode: %d Disassoc_timer: %d",
				ptr->btm_trig_data.btm_request_mode,
				ptr->btm_trig_data.disassoc_timer / 1000);
		temp += buf_cons;
		buf_left -= buf_cons;

		buf_cons = qdf_snprint(temp, buf_left,
				"validity_interval: %d candidate_list_cnt: %d resp_status: %d, bss_termination_timeout: %d, mbo_assoc_retry_timeout: %d",
				ptr->btm_trig_data.validity_interval / 1000,
				ptr->btm_trig_data.candidate_list_count,
				ptr->btm_trig_data.btm_resp_status,
				ptr->btm_trig_data.btm_bss_termination_timeout,
				ptr->btm_trig_data.btm_mbo_assoc_retry_timeout);
		buf_left -= buf_cons;
		temp += buf_cons;
		break;
	case ROAM_TRIGGER_REASON_BSS_LOAD:
		buf_cons = qdf_snprint(temp, buf_left, "CU: %d %% ",
				       ptr->cu_trig_data.cu_load);
		temp += buf_cons;
		buf_left -= buf_cons;
		break;
	case ROAM_TRIGGER_REASON_DEAUTH:
		buf_cons = qdf_snprint(temp, buf_left, "Type: %d Reason: %d ",
				       ptr->deauth_trig_data.type,
				       ptr->deauth_trig_data.reason);
		temp += buf_cons;
		buf_left -= buf_cons;
		break;
	case ROAM_TRIGGER_REASON_LOW_RSSI:
	case ROAM_TRIGGER_REASON_PERIODIC:
		/*
		 * Use ptr->current_rssi get the RSSI of current AP after
		 * roam scan is triggered. This avoids discrepancy with the
		 * next rssi threshold value printed in roam scan details.
		 * ptr->rssi_trig_data.threshold gives the rssi threshold
		 * for the Low Rssi/Periodic scan trigger.
		 */
		buf_cons = qdf_snprint(temp, buf_left,
				       "Cur_Rssi threshold:%d Current AP RSSI: %d",
				       ptr->rssi_trig_data.threshold,
				       ptr->current_rssi);
		temp += buf_cons;
		buf_left -= buf_cons;
		break;
	case ROAM_TRIGGER_REASON_WTC_BTM:
		cm_roam_btm_resp_event(ptr, NULL, vdev_id, true);

		if (ptr->wtc_btm_trig_data.wtc_candi_rssi_ext_present) {
			buf_cons = qdf_snprint(temp, buf_left,
				   "Roaming Mode: %d, Trigger Reason: %d, Sub code:%d, wtc mode:%d, wtc scan mode:%d, wtc rssi th:%d, wtc candi rssi th_2g:%d, wtc_candi_rssi_th_5g:%d, wtc_candi_rssi_th_6g:%d",
				   ptr->wtc_btm_trig_data.roaming_mode,
				   ptr->wtc_btm_trig_data.vsie_trigger_reason,
				   ptr->wtc_btm_trig_data.sub_code,
				   ptr->wtc_btm_trig_data.wtc_mode,
				   ptr->wtc_btm_trig_data.wtc_scan_mode,
				   ptr->wtc_btm_trig_data.wtc_rssi_th,
				   ptr->wtc_btm_trig_data.wtc_candi_rssi_th,
				   ptr->wtc_btm_trig_data.wtc_candi_rssi_th_5g,
				   ptr->wtc_btm_trig_data.wtc_candi_rssi_th_6g);
		} else {
			buf_cons = qdf_snprint(temp, buf_left,
				   "Roaming Mode: %d, Trigger Reason: %d, Sub code:%d, wtc mode:%d, wtc scan mode:%d, wtc rssi th:%d, wtc candi rssi th:%d",
				   ptr->wtc_btm_trig_data.roaming_mode,
				   ptr->wtc_btm_trig_data.vsie_trigger_reason,
				   ptr->wtc_btm_trig_data.sub_code,
				   ptr->wtc_btm_trig_data.wtc_mode,
				   ptr->wtc_btm_trig_data.wtc_scan_mode,
				   ptr->wtc_btm_trig_data.wtc_rssi_th,
				   ptr->wtc_btm_trig_data.wtc_candi_rssi_th);
		}

		temp += buf_cons;
		buf_left -= buf_cons;
		break;
	default:
		break;
	}
}

/**
 * cm_roam_stats_print_trigger_info  - Roam trigger related details
 * @psoc:  Pointer to PSOC object
 * @data:  Pointer to the roam trigger data
 * @scan_data: Roam scan data pointer
 * @vdev_id: Vdev ID
 * @is_full_scan: Was a full scan performed
 *
 * Prints the vdev, roam trigger reason, time of the day at which roaming
 * was triggered.
 *
 * Return: None
 */
static void
cm_roam_stats_print_trigger_info(struct wlan_objmgr_psoc *psoc,
				 struct wmi_roam_trigger_info *data,
				 struct wmi_roam_scan_data *scan_data,
				 uint8_t vdev_id, bool is_full_scan)
{
	char *buf;
	char time[TIME_STRING_LEN];
	QDF_STATUS status;

	buf = qdf_mem_malloc(MAX_ROAM_DEBUG_BUF_SIZE);
	if (!buf)
		return;

	cm_roam_stats_get_trigger_detail_str(data, buf, is_full_scan, vdev_id);
	mlme_get_converted_timestamp(data->timestamp, time);

	/* Update roam trigger info to userspace */
	cm_roam_trigger_info_event(data, scan_data, vdev_id, is_full_scan);
	mlme_nofl_info("%s [ROAM_TRIGGER]: VDEV[%d] %s", time, vdev_id, buf);
	qdf_mem_free(buf);

	status = wlan_cm_update_roam_states(psoc, vdev_id, data->trigger_reason,
					    ROAM_TRIGGER_REASON);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("failed to update rt stats trigger reason");
}

/**
 * cm_roam_stats_print_btm_rsp_info - BTM RSP related details
 * @trigger_info:   Roam scan trigger reason
 * @data:    Pointer to the btm rsp data
 * @vdev_id: vdev id
 * @is_wtc: is WTC?
 *
 * Prints the vdev, btm status, target_bssid and vsie reason
 *
 * Return: None
 */
static void
cm_roam_stats_print_btm_rsp_info(struct wmi_roam_trigger_info *trigger_info,
				 struct roam_btm_response_data *data,
				 uint8_t vdev_id, bool is_wtc)
{
	char time[TIME_STRING_LEN];

	mlme_get_converted_timestamp(data->timestamp, time);
	mlme_nofl_info("%s [BTM RSP]:VDEV[%d], Status:%d, VSIE reason:%d, BSSID: "
		       QDF_MAC_ADDR_FMT, time, vdev_id, data->btm_status,
		       data->vsie_reason,
		       QDF_MAC_ADDR_REF(data->target_bssid.bytes));
	cm_roam_btm_resp_event(trigger_info, data, vdev_id, is_wtc);
}

/**
 * cm_roam_stats_print_roam_initial_info - Roaming related initial details
 * @data:    Pointer to the btm rsp data
 * @vdev_id: vdev id
 *
 * Prints the vdev, roam_full_scan_count, channel and rssi
 * utilization threshold and timer
 *
 * Return: None
 */
static void
cm_roam_stats_print_roam_initial_info(struct roam_initial_data *data,
				      uint8_t vdev_id)
{
	mlme_nofl_info("[ROAM INIT INFO]: VDEV[%d], roam_full_scan_count: %d, rssi_th: %d, cu_th: %d, fw_cancel_timer_bitmap: %d",
		       vdev_id, data->roam_full_scan_count, data->rssi_th,
		       data->cu_th, data->fw_cancel_timer_bitmap);
}

/**
 * cm_roam_stats_print_roam_msg_info - Roaming related message details
 * @data:    Pointer to the btm rsp data
 * @vdev_id: vdev id
 *
 * Prints the vdev, msg_id, msg_param1, msg_param2 and timer
 *
 * Return: None
 */
static void cm_roam_stats_print_roam_msg_info(struct roam_msg_info *data,
					      uint8_t vdev_id)
{
	char time[TIME_STRING_LEN];
	static const char msg_id1_str[] = "Roam RSSI TH Reset";

	if (data->msg_id == WMI_ROAM_MSG_RSSI_RECOVERED) {
		mlme_get_converted_timestamp(data->timestamp, time);
		mlme_nofl_info("%s [ROAM MSG INFO]: VDEV[%d] %s, Current rssi: %d dbm, next_rssi_threshold: %d dbm",
			       time, vdev_id, msg_id1_str, data->msg_param1,
			       data->msg_param2);
	}
}

/**
 * cm_stats_log_roam_scan_candidates  - Print roam scan candidate AP info
 * @ap:           Pointer to the candidate AP list
 * @num_entries:  Number of candidate APs
 *
 * Print the RSSI, CU load, Cu score, RSSI score, total score, BSSID
 * and time stamp at which the candidate was found details.
 *
 * Return: None
 */
static void
cm_stats_log_roam_scan_candidates(struct wmi_roam_candidate_info *ap,
				  uint8_t num_entries)
{
	uint16_t i;
	char time[TIME_STRING_LEN], time2[TIME_STRING_LEN];


	mlme_nofl_info("%62s%62s", LINE_STR, LINE_STR);
	mlme_nofl_info("%13s %16s %8s %4s %4s %5s/%3s %3s/%3s %7s %7s %6s %12s %20s",
		       "AP BSSID", "TSTAMP", "CH", "TY", "ETP", "RSSI",
		       "SCR", "CU%", "SCR", "TOT_SCR", "BL_RSN", "BL_SRC",
		       "BL_TSTAMP", "BL_TIMEOUT(ms)");
	mlme_nofl_info("%62s%62s", LINE_STR, LINE_STR);

	if (num_entries > MAX_ROAM_CANDIDATE_AP)
		num_entries = MAX_ROAM_CANDIDATE_AP;

	for (i = 0; i < num_entries; i++) {
		mlme_get_converted_timestamp(ap->timestamp, time);
		mlme_get_converted_timestamp(ap->dl_timestamp, time2);
		mlme_nofl_info(QDF_MAC_ADDR_FMT " %17s %4d %-4s %4d %3d/%-4d %2d/%-4d %5d %7d %7d %17s %9d",
			       QDF_MAC_ADDR_REF(ap->bssid.bytes), time,
			  ap->freq,
			  ((ap->type == 0) ? "C_AP" :
			  ((ap->type == 2) ? "R_AP" : "P_AP")),
			  ap->etp, ap->rssi, ap->rssi_score, ap->cu_load,
			  ap->cu_score, ap->total_score, ap->dl_reason,
			  ap->dl_source, time2, ap->dl_original_timeout);
		/* Update roam candidates info to userspace */
		cm_roam_candidate_info_event(ap, i);
		ap++;
	}
}

/**
 * cm_get_roam_scan_type_str() - Get the string for roam scan type
 * @roam_scan_type: roam scan type coming from fw via
 * wmi_roam_scan_info tlv
 *
 *  Return: Meaningful string for roam scan type
 */
static char *cm_get_roam_scan_type_str(uint32_t roam_scan_type)
{
	switch (roam_scan_type) {
	case ROAM_STATS_SCAN_TYPE_PARTIAL:
		return "PARTIAL";
	case ROAM_STATS_SCAN_TYPE_FULL:
		return "FULL";
	case ROAM_STATS_SCAN_TYPE_NO_SCAN:
		return "NO SCAN";
	case ROAM_STATS_SCAN_TYPE_HIGHER_BAND_5GHZ_6GHZ:
		return "Higher Band: 5 GHz + 6 GHz";
	case ROAM_STATS_SCAN_TYPE_HIGHER_BAND_6GHZ:
		return "Higher Band : 6 GHz";
	default:
		return "UNKNOWN";
	}
}

/**
 * cm_roam_stats_print_scan_info  - Print the roam scan details and candidate AP
 * details
 * @psoc:      psoc common object
 * @scan:      Pointer to the received tlv after sanitization
 * @vdev_id:   Vdev ID
 * @trigger:   Roam scan trigger reason
 * @timestamp: Host timestamp in millisecs
 *
 * Prinst the roam scan details with time of the day when the scan was
 * triggered and roam candidate AP with score details
 *
 * Return: None
 */
static void
cm_roam_stats_print_scan_info(struct wlan_objmgr_psoc *psoc,
			      struct wmi_roam_scan_data *scan, uint8_t vdev_id,
			      uint32_t trigger, uint32_t timestamp)
{
	uint16_t num_ch = scan->num_chan;
	uint16_t buf_cons = 0, buf_left = ROAM_CHANNEL_BUF_SIZE;
	uint8_t i;
	char *buf, *buf1, *tmp;
	char time[TIME_STRING_LEN];

	/* Update roam scan info to userspace */
	cm_roam_scan_info_event(psoc, scan, vdev_id);

	buf = qdf_mem_malloc(ROAM_CHANNEL_BUF_SIZE);
	if (!buf)
		return;

	tmp = buf;
	/* For partial scans, print the channel info */
	if (scan->type == ROAM_STATS_SCAN_TYPE_PARTIAL) {
		buf_cons = qdf_snprint(tmp, buf_left, "{");
		buf_left -= buf_cons;
		tmp += buf_cons;

		for (i = 0; i < num_ch; i++) {
			buf_cons = qdf_snprint(tmp, buf_left, "%d ",
					       scan->chan_freq[i]);
			buf_left -= buf_cons;
			tmp += buf_cons;
		}
		buf_cons = qdf_snprint(tmp, buf_left, "}");
		buf_left -= buf_cons;
		tmp += buf_cons;
	}

	buf1 = qdf_mem_malloc(ROAM_FAILURE_BUF_SIZE);
	if (!buf1) {
		qdf_mem_free(buf);
		return;
	}

	if (trigger == ROAM_TRIGGER_REASON_LOW_RSSI ||
	    trigger == ROAM_TRIGGER_REASON_PERIODIC)
		qdf_snprint(buf1, ROAM_FAILURE_BUF_SIZE,
			    "next_rssi_threshold: %d dBm",
			    scan->next_rssi_threshold);

	mlme_get_converted_timestamp(timestamp, time);
	mlme_nofl_info("%s [ROAM_SCAN]: VDEV[%d] Scan_type: %s %s %s",
		       time, vdev_id, cm_get_roam_scan_type_str(scan->type),
		       buf1, buf);
	cm_stats_log_roam_scan_candidates(scan->ap, scan->num_ap);

	qdf_mem_free(buf);
	qdf_mem_free(buf1);
}

/**
 * cm_roam_stats_print_roam_result()  - Print roam result related info
 * @psoc: Pointer to psoc object
 * @trigger: roam trigger information
 * @res:     Roam result structure pointer
 * @scan_data: scan data
 * @vdev_id: Vdev id
 *
 * Print roam result and failure reason if roaming failed.
 *
 * Return: None
 */
static void
cm_roam_stats_print_roam_result(struct wlan_objmgr_psoc *psoc,
				struct wmi_roam_trigger_info *trigger,
				struct wmi_roam_result *res,
				struct wmi_roam_scan_data *scan_data,
				uint8_t vdev_id)
{
	char *buf;
	char time[TIME_STRING_LEN];
	QDF_STATUS status;

	/* Update roam result info to userspace */
	cm_roam_result_info_event(psoc, trigger, res, scan_data, vdev_id);

	buf = qdf_mem_malloc(ROAM_FAILURE_BUF_SIZE);
	if (!buf)
		return;

	if (res->status == 1)
		qdf_snprint(buf, ROAM_FAILURE_BUF_SIZE, "Reason: %s",
			    mlme_get_roam_fail_reason_str(res->fail_reason));

	mlme_get_converted_timestamp(res->timestamp, time);
	mlme_nofl_info("%s [ROAM_RESULT]: VDEV[%d] %s %s",
		       time, vdev_id, mlme_get_roam_status_str(res->status),
		       buf);
	qdf_mem_free(buf);

	status = wlan_cm_update_roam_states(psoc, vdev_id, res->fail_reason,
					    ROAM_FAIL_REASON);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("failed to update rt stats roam fail reason");
}

#define WLAN_ROAM_11KV_REQ_TYPE_BTM        1
#define WLAN_ROAM_11KV_REQ_TYPE_NEIGH_RPT  2

/**
 * cm_roam_stats_print_11kv_info  - Print neighbor report/BTM related data
 * @psoc: Pointer to psoc object
 * @neigh_rpt: Pointer to the extracted TLV structure
 * @vdev_id:   Vdev ID
 *
 * Print BTM/neighbor report info that is sent by firmware after
 * connection/roaming to an AP.
 *
 * Return: none
 */
static void
cm_roam_stats_print_11kv_info(struct wlan_objmgr_psoc *psoc,
			      struct wmi_neighbor_report_data *neigh_rpt,
			      uint8_t vdev_id)
{
	char time[TIME_STRING_LEN], time1[TIME_STRING_LEN];
	char *buf, *tmp;
	uint8_t type = neigh_rpt->req_type, i;
	uint16_t buf_left = ROAM_CHANNEL_BUF_SIZE, buf_cons;
	uint8_t num_ch = neigh_rpt->num_freq;
	struct wlan_objmgr_vdev *vdev;

	if (!type)
		return;

	buf = qdf_mem_malloc(ROAM_CHANNEL_BUF_SIZE);
	if (!buf)
		return;

	tmp = buf;
	if (num_ch) {
		buf_cons = qdf_snprint(tmp, buf_left, "{ ");
		buf_left -= buf_cons;
		tmp += buf_cons;

		for (i = 0; i < num_ch; i++) {
			buf_cons = qdf_snprint(tmp, buf_left, "%d ",
					       neigh_rpt->freq[i]);
			buf_left -= buf_cons;
			tmp += buf_cons;
		}

		buf_cons = qdf_snprint(tmp, buf_left, "}");
		buf_left -= buf_cons;
		tmp += buf_cons;
	}

	mlme_get_converted_timestamp(neigh_rpt->req_time, time);
	mlme_nofl_info("%s [%s] VDEV[%d]", time,
		       (type == WLAN_ROAM_11KV_REQ_TYPE_BTM) ?
		       "BTM_QUERY" : "NEIGH_RPT_REQ", vdev_id);

	if (type == WLAN_ROAM_11KV_REQ_TYPE_BTM)
		cm_roam_btm_query_event(neigh_rpt, vdev_id);
	else if (type == WLAN_ROAM_11KV_REQ_TYPE_NEIGH_RPT) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							WLAN_MLME_OBJMGR_ID);
		if (!vdev) {
			mlme_err("vdev pointer not found");
			goto out;
		}

		cm_roam_neigh_rpt_req_event(neigh_rpt, vdev);

		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
	}

	if (neigh_rpt->resp_time) {
		mlme_get_converted_timestamp(neigh_rpt->resp_time, time1);
		mlme_nofl_info("%s [%s] VDEV[%d] %s", time1,
			       (type == WLAN_ROAM_11KV_REQ_TYPE_BTM) ?
			       "BTM_REQ" : "NEIGH_RPT_RSP",
			       vdev_id,
			       (num_ch > 0) ? buf : "NO Ch update");

		if (type == WLAN_ROAM_11KV_REQ_TYPE_NEIGH_RPT)
			cm_roam_neigh_rpt_resp_event(neigh_rpt, vdev_id);

	} else {
		mlme_nofl_info("%s No response received from AP",
			       (type == WLAN_ROAM_11KV_REQ_TYPE_BTM) ?
			       "BTM" : "NEIGH_RPT");
	}
out:
	qdf_mem_free(buf);
}

static char *
cm_get_frame_subtype_str(enum mgmt_subtype frame_subtype)
{
	switch (frame_subtype) {
	case MGMT_SUBTYPE_ASSOC_REQ:
		return "ASSOC";
	case MGMT_SUBTYPE_ASSOC_RESP:
		return "ASSOC";
	case MGMT_SUBTYPE_REASSOC_REQ:
		return "REASSOC";
	case MGMT_SUBTYPE_REASSOC_RESP:
		return "REASSOC";
	case MGMT_SUBTYPE_DISASSOC:
		return "DISASSOC";
	case MGMT_SUBTYPE_AUTH:
		return "AUTH";
	case MGMT_SUBTYPE_DEAUTH:
		return "DEAUTH";
	default:
		break;
	}

	return "Invalid frm";
}

#define WLAN_SAE_AUTH_ALGO 3
static void
cm_roam_print_frame_info(struct wlan_objmgr_psoc *psoc,
			 struct roam_frame_stats *frame_data,
			 struct wmi_roam_scan_data *scan_data, uint8_t vdev_id)
{
	struct roam_frame_info *frame_info;
	char time[TIME_STRING_LEN];
	uint8_t i;

	if (!frame_data->num_frame)
		return;

	for (i = 0; i < frame_data->num_frame; i++) {
		frame_info = &frame_data->frame_info[i];
		if (frame_info->auth_algo == WLAN_SAE_AUTH_ALGO &&
		    wlan_is_log_record_present_for_bssid(psoc,
							 &frame_info->bssid,
							 vdev_id)) {
			wlan_print_cached_sae_auth_logs(psoc,
							&frame_info->bssid,
							vdev_id);
			continue;
		}

		qdf_mem_zero(time, TIME_STRING_LEN);
		mlme_get_converted_timestamp(frame_info->timestamp, time);

		if (frame_info->type != ROAM_FRAME_INFO_FRAME_TYPE_EXT)
			mlme_nofl_info("%s [%s%s] VDEV[%d] status:%d seq_num:%d",
				       time,
				       cm_get_frame_subtype_str(frame_info->subtype),
				       frame_info->subtype ==  MGMT_SUBTYPE_AUTH ?
				       (frame_info->is_rsp ? " RX" : " TX") : "",
				       vdev_id,
				       frame_info->status_code,
				       frame_info->seq_num);

		cm_roam_mgmt_frame_event(frame_info, scan_data, vdev_id);
	}
}

void cm_report_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id,
			     enum roam_rt_stats_type events,
			     struct roam_stats_event *roam_info,
			     uint32_t value, uint8_t idx)
{
	struct roam_stats_event *roam_event = NULL;

	if (!wlan_cm_get_roam_rt_stats(psoc, ROAM_RT_STATS_ENABLE)) {
		mlme_debug("Roam events stats is disabled");
		return;
	}

	switch (events) {
	case ROAM_RT_STATS_TYPE_SCAN_STATE:
		roam_event = qdf_mem_malloc(sizeof(*roam_event));
		if (!roam_event)
			return;

		if (value == WMI_ROAM_NOTIF_SCAN_START)
			roam_event->roam_event_param.roam_scan_state =
					QCA_WLAN_VENDOR_ROAM_SCAN_STATE_START;
		else if (value == WMI_ROAM_NOTIF_SCAN_END)
			roam_event->roam_event_param.roam_scan_state =
					QCA_WLAN_VENDOR_ROAM_SCAN_STATE_END;

		//TO DO: Add a new CB in CM and register the hdd function to it
		//And call the new CB from here.
		mlme_debug("Invoke HDD roam events callback for roam "
			   "scan notif");
		roam_event->vdev_id = vdev_id;
		qdf_mem_free(roam_event);
		break;
	case ROAM_RT_STATS_TYPE_INVOKE_FAIL_REASON:
		roam_event = qdf_mem_malloc(sizeof(*roam_event));
		if (!roam_event)
			return;

		roam_event->roam_event_param.roam_invoke_fail_reason = value;

		//TO DO: Add a new CB in CM and register the hdd function to it
		//And call the new CB from here.
		mlme_debug("Invoke HDD roam events callback for roam "
			   "invoke fail");
		roam_event->vdev_id = vdev_id;
		qdf_mem_free(roam_event);
		break;
	case ROAM_RT_STATS_TYPE_ROAM_SCAN_INFO:
		if (roam_info->scan[idx].present ||
		    roam_info->trigger[idx].present ||
		    (roam_info->result[idx].present &&
		     roam_info->result[idx].fail_reason)) {
			mlme_debug("Invoke HDD roam events callback for roam "
				   "stats event");
			roam_info->vdev_id = vdev_id;
		//TO DO: Add a new CB in CM and register the hdd function to it
		//And call the new CB from here.
		}
		break;
	default:
		break;
	}
}

/**
 * cm_roam_handle_btm_stats() - Handle BTM related logging roam stats.
 * @psoc: psoc pointer
 * @stats_info: Pointer to the roam stats
 * @i: TLV indev for BTM roam trigger
 * @rem_tlv_len: Remaining TLV length
 *
 * Return: None
 */
static void
cm_roam_handle_btm_stats(struct wlan_objmgr_psoc *psoc,
			 struct roam_stats_event *stats_info, uint8_t i,
			 uint8_t *rem_tlv_len)
{
	bool log_btm_frames_only = false;

	if (stats_info->data_11kv[i].present)
		cm_roam_stats_print_11kv_info(psoc, &stats_info->data_11kv[i],
					      stats_info->vdev_id);

	/*
	 * If roam trigger is BTM and roam scan type is no scan then
	 * the roam stats event is for BTM frames logging.
	 * So log the BTM frames alone and return.
	 */
	if (stats_info->scan[i].present &&
	    stats_info->scan[i].type == ROAM_STATS_SCAN_TYPE_NO_SCAN) {
		cm_roam_btm_req_event(&stats_info->trigger[i].btm_trig_data,
				      &stats_info->trigger[i],
				      stats_info->vdev_id, false);
		log_btm_frames_only = true;
		goto log_btm_frames_only;
	}

	if (stats_info->trigger[i].present) {
		bool is_full_scan = stats_info->scan[i].present &&
				    stats_info->scan[i].type;

		/* BTM request diag log event will be sent from inside below */
		cm_roam_stats_print_trigger_info(psoc, &stats_info->trigger[i],
						 &stats_info->scan[i],
						 stats_info->vdev_id,
						 is_full_scan);

		if (stats_info->scan[i].present)
			cm_roam_stats_print_scan_info(
				psoc, &stats_info->scan[i],
				stats_info->vdev_id,
				stats_info->trigger[i].trigger_reason,
				stats_info->trigger[i].timestamp);
	}

	if (stats_info->result[i].present)
		cm_roam_stats_print_roam_result(psoc, &stats_info->trigger[i],
						&stats_info->result[i],
						&stats_info->scan[i],
						stats_info->vdev_id);

	if (stats_info->frame_stats[i].num_frame)
		cm_roam_print_frame_info(psoc, &stats_info->frame_stats[i],
					 &stats_info->scan[i],
					 stats_info->vdev_id);

log_btm_frames_only:
	/*
	 * Print BTM resp TLV info (wmi_roam_btm_response_info) only
	 * when trigger reason is BTM or WTC_BTM. As for other roam
	 * triggers this TLV contains zeros, so host should not print.
	 */
	if (stats_info->btm_rsp[i].present && stats_info->trigger[i].present &&
	    (stats_info->trigger[i].trigger_reason == ROAM_TRIGGER_REASON_BTM ||
	     stats_info->trigger[i].trigger_reason ==
						ROAM_TRIGGER_REASON_WTC_BTM))
		cm_roam_stats_print_btm_rsp_info(&stats_info->trigger[i],
						 &stats_info->btm_rsp[i],
						 stats_info->vdev_id, false);

	if (log_btm_frames_only)
		return;

	if (stats_info->roam_init_info[i].present)
		cm_roam_stats_print_roam_initial_info(
				&stats_info->roam_init_info[i],
				stats_info->vdev_id);

	if (stats_info->roam_msg_info && stats_info->roam_msg_info[i].present &&
	    i < stats_info->num_roam_msg_info) {
		*rem_tlv_len = *rem_tlv_len + 1;
		cm_roam_stats_print_roam_msg_info(
						  &stats_info->roam_msg_info[i],
						  stats_info->vdev_id);
	}

	cm_report_roam_rt_stats(psoc, stats_info->vdev_id,
				ROAM_RT_STATS_TYPE_ROAM_SCAN_INFO,
				stats_info, 0, i);
}

QDF_STATUS
cm_roam_stats_event_handler(struct wlan_objmgr_psoc *psoc,
			    struct roam_stats_event *stats_info)
{
	uint8_t i, rem_tlv = 0;
	bool is_wtc = false;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!stats_info)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < stats_info->num_tlv; i++) {
		if (stats_info->trigger[i].present) {
			bool is_full_scan =
				stats_info->scan[i].present &&
				stats_info->scan[i].type;

			if (stats_info->trigger[i].trigger_reason ==
			    ROAM_TRIGGER_REASON_BTM) {
				cm_roam_handle_btm_stats(psoc, stats_info, i,
							 &rem_tlv);
				continue;
			}

			cm_roam_stats_print_trigger_info(
					psoc, &stats_info->trigger[i],
					&stats_info->scan[i],
					stats_info->vdev_id, is_full_scan);

			if (stats_info->scan[i].present)
				cm_roam_stats_print_scan_info(
					psoc, &stats_info->scan[i],
					stats_info->vdev_id,
					stats_info->trigger[i].trigger_reason,
					stats_info->trigger[i].timestamp);
		}

		if (stats_info->result[i].present)
			cm_roam_stats_print_roam_result(psoc,
							&stats_info->trigger[i],
							&stats_info->result[i],
							&stats_info->scan[i],
							stats_info->vdev_id);

		if (stats_info->frame_stats[i].num_frame)
			cm_roam_print_frame_info(psoc,
						 &stats_info->frame_stats[i],
						 &stats_info->scan[i],
						 stats_info->vdev_id);

		/*
		 * Print BTM resp TLV info (wmi_roam_btm_response_info) only
		 * when trigger reason is BTM or WTC_BTM. As for other roam
		 * triggers this TLV contains zeros, so host should not print.
		 */
		if (stats_info->btm_rsp[i].present &&
		    stats_info->trigger[i].present &&
		    (stats_info->trigger[i].trigger_reason ==
		     ROAM_TRIGGER_REASON_BTM ||
		     stats_info->trigger[i].trigger_reason ==
		     ROAM_TRIGGER_REASON_WTC_BTM))
			cm_roam_stats_print_btm_rsp_info(
						&stats_info->trigger[i],
						&stats_info->btm_rsp[i],
						stats_info->vdev_id, false);

		if (stats_info->roam_init_info[i].present)
			cm_roam_stats_print_roam_initial_info(
						 &stats_info->roam_init_info[i],
						 stats_info->vdev_id);

		if (stats_info->roam_msg_info &&
		    i < stats_info->num_roam_msg_info &&
		    stats_info->roam_msg_info[i].present) {
			rem_tlv++;
			cm_roam_stats_print_roam_msg_info(
						  &stats_info->roam_msg_info[i],
						  stats_info->vdev_id);
		}

		cm_report_roam_rt_stats(psoc, stats_info->vdev_id,
					ROAM_RT_STATS_TYPE_ROAM_SCAN_INFO,
					stats_info, 0, i);
	}

	if (!stats_info->num_tlv) {
		/*
		 * wmi_roam_trigger_reason TLV is sent only for userspace
		 * logging of BTM/WTC frame without roam scans.
		 */

		if (stats_info->data_11kv[0].present)
			cm_roam_stats_print_11kv_info(psoc,
						      &stats_info->data_11kv[0],
						      stats_info->vdev_id);

		if (stats_info->trigger[0].present &&
		    (stats_info->trigger[0].trigger_reason ==
		     ROAM_TRIGGER_REASON_BTM ||
		     stats_info->trigger[0].trigger_reason ==
		     ROAM_TRIGGER_REASON_WTC_BTM)) {
			if (stats_info->trigger[0].trigger_reason ==
			    ROAM_TRIGGER_REASON_WTC_BTM)
				is_wtc = true;

			cm_roam_btm_req_event(&stats_info->trigger[0].btm_trig_data,
					      &stats_info->trigger[0],
					      stats_info->vdev_id, is_wtc);
		}

		if (stats_info->scan[0].present &&
		    stats_info->trigger[0].present)
			cm_roam_stats_print_scan_info(psoc,
					  &stats_info->scan[0],
					  stats_info->vdev_id,
					  stats_info->trigger[0].trigger_reason,
					  stats_info->trigger[0].timestamp);

		if (stats_info->btm_rsp[0].present)
			cm_roam_stats_print_btm_rsp_info(
					&stats_info->trigger[0],
					&stats_info->btm_rsp[0],
					stats_info->vdev_id, 0);

		/*
		 * WTC BTM response with reason code
		 * WTC print should be after the normal BTM
		 * response print
		 */
		if (stats_info->trigger[0].present &&
		    stats_info->trigger[0].trigger_reason ==
		    ROAM_TRIGGER_REASON_WTC_BTM)
			cm_roam_btm_resp_event(&stats_info->trigger[0], NULL,
					       stats_info->vdev_id, true);
	}

	if (stats_info->roam_msg_info && stats_info->num_roam_msg_info &&
	    stats_info->num_roam_msg_info - rem_tlv) {
		for (i = 0; i < (stats_info->num_roam_msg_info-rem_tlv); i++) {
			if (stats_info->roam_msg_info[rem_tlv + i].present)
				cm_roam_stats_print_roam_msg_info(
					&stats_info->roam_msg_info[rem_tlv + i],
					stats_info->vdev_id);
		}
	}

	wlan_clear_sae_auth_logs_cache(psoc, stats_info->vdev_id);
	qdf_mem_free(stats_info->roam_msg_info);
	qdf_mem_free(stats_info);

	return status;
}

bool wlan_cm_is_roam_sync_in_progress(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool ret;
	enum QDF_OPMODE opmode;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_CM_ID);

	if (!vdev)
		return false;

	opmode = wlan_vdev_mlme_get_opmode(vdev);

	if (opmode != QDF_STA_MODE) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return false;
	}

	ret = cm_is_vdev_roam_sync_inprogress(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return ret;
}

void
wlan_cm_set_roam_offload_bssid(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr *bssid)
{
	struct mlme_legacy_priv *mlme_priv;
	struct qdf_mac_addr *mlme_bssid;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		return;
	}

	mlme_bssid = &(mlme_priv->cm_roam.sae_offload.bssid);

	if (!bssid || qdf_is_macaddr_zero(bssid)) {
		mlme_err("NULL BSSID");
		return;
	}
	qdf_mem_copy(mlme_bssid->bytes, bssid->bytes, QDF_MAC_ADDR_SIZE);
}

void
wlan_cm_get_roam_offload_bssid(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr *bssid)
{
	struct mlme_legacy_priv *mlme_priv;
	struct qdf_mac_addr *mlme_bssid;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		return;
	}

	mlme_bssid = &(mlme_priv->cm_roam.sae_offload.bssid);

	if (!bssid)
		return;
	qdf_mem_copy(bssid->bytes, mlme_bssid->bytes, QDF_MAC_ADDR_SIZE);
}

void
wlan_cm_set_roam_offload_ssid(struct wlan_objmgr_vdev *vdev,
			      uint8_t *ssid, uint8_t len)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_ssid *mlme_ssid;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		return;
	}

	mlme_ssid = &mlme_priv->cm_roam.sae_offload.ssid;

	if (len > WLAN_SSID_MAX_LEN)
		len = WLAN_SSID_MAX_LEN;

	qdf_mem_zero(mlme_ssid, sizeof(struct wlan_ssid));
	qdf_mem_copy(&mlme_ssid->ssid[0], ssid, len);
	mlme_ssid->length = len;

	mlme_debug("Set roam offload ssid: " QDF_SSID_FMT,
		   QDF_SSID_REF(mlme_ssid->length,
				mlme_ssid->ssid));
}

void
wlan_cm_get_roam_offload_ssid(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			      uint8_t *ssid, uint8_t *len)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_ssid *mlme_ssid;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("VDEV is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		goto ret;
	}

	mlme_ssid = &mlme_priv->cm_roam.sae_offload.ssid;

	qdf_mem_copy(ssid, mlme_ssid->ssid, mlme_ssid->length);
	*len = mlme_ssid->length;

ret:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

void
wlan_cm_roam_set_ho_delay_config(struct wlan_objmgr_psoc *psoc,
				 uint16_t roam_ho_delay)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	mlme_obj->cfg.lfr.roam_ho_delay_config = roam_ho_delay;
}

uint16_t
wlan_cm_roam_get_ho_delay_config(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return 0;
	}

	return mlme_obj->cfg.lfr.roam_ho_delay_config;
}

void
wlan_cm_set_exclude_rm_partial_scan_freq(struct wlan_objmgr_psoc *psoc,
					 uint8_t exclude_rm_partial_scan_freq)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	mlme_obj->cfg.lfr.exclude_rm_partial_scan_freq =
						exclude_rm_partial_scan_freq;
}

uint8_t
wlan_cm_get_exclude_rm_partial_scan_freq(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return 0;
	}

	return mlme_obj->cfg.lfr.exclude_rm_partial_scan_freq;
}

void
wlan_cm_roam_set_full_scan_6ghz_on_disc(struct wlan_objmgr_psoc *psoc,
					uint8_t roam_full_scan_6ghz_on_disc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	mlme_obj->cfg.lfr.roam_full_scan_6ghz_on_disc =
						roam_full_scan_6ghz_on_disc;
}

uint8_t wlan_cm_roam_get_full_scan_6ghz_on_disc(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return 0;
	}

	return mlme_obj->cfg.lfr.roam_full_scan_6ghz_on_disc;
}

#else
QDF_STATUS
cm_roam_stats_event_handler(struct wlan_objmgr_psoc *psoc,
			    struct roam_stats_event *stats_info)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_FIPS
QDF_STATUS cm_roam_pmkid_req_ind(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 struct roam_pmkid_req_event *src_lst)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev;
	struct qdf_mac_addr *dst_list;
	uint32_t num_entries, i;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	num_entries = src_lst->num_entries;
	mlme_debug("Num entries %d", num_entries);
	for (i = 0; i < num_entries; i++) {
		dst_list = &src_lst->ap_bssid[i];
		status = mlme_cm_osif_pmksa_candidate_notify(vdev, dst_list,
							     1, false);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err("Number %d Notify failed for " QDF_MAC_ADDR_FMT,
				 i, QDF_MAC_ADDR_REF(dst_list->bytes));
			goto rel_ref;
		}
	}

rel_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

	return status;
}
#endif /* WLAN_FEATURE_FIPS */

QDF_STATUS
cm_cleanup_mlo_link(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;

	/* Use MLO roam internal disconnect as this is for cleanup and
	 * no need to inform OSIF, and REASON_FW_TRIGGERED_ROAM_FAILURE will
	 * cleanup host without informing the FW
	 */
	status = wlan_cm_disconnect(vdev,
				    CM_MLO_ROAM_INTERNAL_DISCONNECT,
				    REASON_FW_TRIGGERED_ROAM_FAILURE,
				    NULL);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("Failed to post disconnect for link vdev");

	return status;
}

bool wlan_is_rso_enabled(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	enum roam_offload_state cur_state;

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	if (cur_state == WLAN_ROAM_RSO_ENABLED ||
	    cur_state == WLAN_ROAMING_IN_PROG ||
	    cur_state == WLAN_ROAM_SYNCH_IN_PROG ||
	    cur_state == WLAN_MLO_ROAM_SYNCH_IN_PROG)
		return true;

	return false;
}

bool wlan_is_roaming_enabled(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (mlme_get_roam_state(psoc, vdev_id) == WLAN_ROAM_DEINIT)
		return false;

	return true;
}

QDF_STATUS
wlan_cm_set_sae_auth_ta(struct wlan_objmgr_pdev *pdev,
			uint8_t vdev_id,
			struct qdf_mac_addr sae_auth_ta)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_objmgr_vdev *vdev;

	if (!pdev)
		return QDF_STATUS_E_INVAL;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return QDF_STATUS_E_INVAL;
	}
	qdf_mem_copy(mlme_priv->mlme_roam.sae_auth_ta.bytes, sae_auth_ta.bytes,
		     QDF_MAC_ADDR_SIZE);
	mlme_priv->mlme_roam.sae_auth_pending = true;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_cm_get_sae_auth_ta(struct wlan_objmgr_pdev *pdev,
			uint8_t vdev_id,
			struct qdf_mac_addr *sae_auth_ta)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_objmgr_vdev *vdev;

	if (!pdev)
		return QDF_STATUS_E_INVAL;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return QDF_STATUS_E_INVAL;
	}

	if (mlme_priv->mlme_roam.sae_auth_pending) {
		qdf_mem_copy(sae_auth_ta->bytes,
			     mlme_priv->mlme_roam.sae_auth_ta.bytes,
			     QDF_MAC_ADDR_SIZE);
		mlme_priv->mlme_roam.sae_auth_pending = false;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return QDF_STATUS_SUCCESS;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	return QDF_STATUS_E_ALREADY;
}

void
wlan_cm_set_assoc_btm_cap(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev)
		return;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;

	mlme_priv->connect_info.assoc_btm_cap = val;
}

bool
wlan_cm_get_assoc_btm_cap(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev)
		return true;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return true;

	return mlme_priv->connect_info.assoc_btm_cap;
}

bool wlan_cm_is_self_mld_roam_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mlme_debug("Invalid WMI handle");
		return false;
	}

	return wmi_service_enabled(wmi_handle,
				   wmi_service_self_mld_roam_between_dbs_and_hbs);
}

void
wlan_cm_set_force_20mhz_in_24ghz(struct wlan_objmgr_vdev *vdev,
				 bool is_40mhz_cap)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct mlme_legacy_priv *mlme_priv;
	uint16_t dot11_mode;
	bool send_ie_to_fw = false;

	if (!vdev)
		return;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj || !mlme_obj->cfg.obss_ht40.is_override_ht20_40_24g)
		return;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;

	/*
	 * Force 20 MHz in 2.4 GHz only if "override_ht20_40_24g" ini
	 * is set and userspace connect req doesn't have 40 MHz HT caps
	 */
	if (mlme_priv->connect_info.force_20mhz_in_24ghz != !is_40mhz_cap)
		send_ie_to_fw = true;

	mlme_priv->connect_info.force_20mhz_in_24ghz = !is_40mhz_cap;

	if (cm_is_vdev_connected(vdev) && send_ie_to_fw) {
		dot11_mode =
			cm_csr_get_vdev_dot11_mode(wlan_vdev_get_id(vdev));
		cm_send_ies_for_roam_invoke(vdev, dot11_mode);
	}
}

bool
wlan_cm_get_force_20mhz_in_24ghz(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev)
		return true;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return true;

	return mlme_priv->connect_info.force_20mhz_in_24ghz;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_cm_update_offload_ssid_from_candidate(struct wlan_objmgr_pdev *pdev,
					   uint8_t vdev_id,
					   struct qdf_mac_addr *ap_bssid)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	qdf_list_t *list = NULL;
	struct scan_cache_node *first_node = NULL;
	struct scan_filter *scan_filter;
	struct scan_cache_entry *entry;
	struct qdf_mac_addr cache_bssid;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_NULL_VALUE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	/*
	 * sae_offload ssid, bssid would have already been cached
	 * from the tx profile of the roam_candidate_frame from FW,
	 * but if the roam offload is received for a non-tx BSSID,
	 * then the ssid stored in mlme_priv would be incorrect.
	 *
	 * If the bssid cached in sae_offload_param doesn't match
	 * with the bssid received in roam_offload_event, then
	 * get the scan entry from scan table to save the proper
	 * ssid, bssid.
	 */
	wlan_cm_get_roam_offload_bssid(vdev, &cache_bssid);
	if (!qdf_mem_cmp(cache_bssid.bytes, ap_bssid->bytes, QDF_MAC_ADDR_SIZE))
		goto end;

	mlme_debug("Update the roam offload ssid from scan cache");

	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}

	scan_filter->num_of_bssid = 1;
	qdf_mem_copy(scan_filter->bssid_list[0].bytes,
		     ap_bssid, sizeof(struct qdf_mac_addr));

	list = wlan_scan_get_result(pdev, scan_filter);
	qdf_mem_free(scan_filter);

	if (!list || !qdf_list_size(list)) {
		mlme_err("Scan result is empty, candidate entry not found");
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	qdf_list_peek_front(list, (qdf_list_node_t **)&first_node);
	if (first_node && first_node->entry) {
		entry = first_node->entry;
		wlan_cm_set_roam_offload_ssid(vdev,
					      &entry->ssid.ssid[0],
					      entry->ssid.length);
		wlan_cm_set_roam_offload_bssid(vdev, ap_bssid);
	}

end:
	if (list)
		wlan_scan_purge_results(list);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
	return status;
}

QDF_STATUS
wlan_cm_add_frame_to_scan_db(struct wlan_objmgr_psoc *psoc,
			     struct roam_scan_candidate_frame *frame)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct cnx_mgr *cm_ctx;
	uint32_t ie_offset, ie_len;
	uint8_t *ie_ptr = NULL;
	uint8_t *extracted_ie = NULL;
	uint8_t primary_channel, band;
	qdf_freq_t op_freq;
	struct wlan_frame_hdr *wh;
	struct qdf_mac_addr bssid;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, frame->vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlme_err("pdev object is NULL");
		goto err;
	}

	cm_ctx = cm_get_cm_ctx(vdev);
	if (!cm_ctx) {
		mlme_err("cm ctx is NULL");
		goto err;
	}

	/* Fixed parameters offset */
	ie_offset = sizeof(struct wlan_frame_hdr) + MAC_B_PR_SSID_OFFSET;

	if (frame->frame_length <= ie_offset) {
		mlme_err("Invalid frame length");
		goto err;
	}

	ie_ptr = frame->frame + ie_offset;
	ie_len = frame->frame_length - ie_offset;

	extracted_ie = (uint8_t *)wlan_get_ie_ptr_from_eid(WLAN_ELEMID_SSID,
							   ie_ptr, ie_len);
	if (extracted_ie && extracted_ie[0] == WLAN_ELEMID_SSID) {
		wh = (struct wlan_frame_hdr *)frame->frame;
		WLAN_ADDR_COPY(&bssid.bytes[0], wh->i_addr2);

		mlme_debug("SSID of the candidate is " QDF_SSID_FMT,
			   QDF_SSID_REF(extracted_ie[1], &extracted_ie[2]));
		wlan_cm_set_roam_offload_ssid(vdev, &extracted_ie[2],
					      extracted_ie[1]);
		wlan_cm_set_roam_offload_bssid(vdev, &bssid);
	}

	/* For 2.4GHz,5GHz get channel from DS IE */
	extracted_ie = (uint8_t *)wlan_get_ie_ptr_from_eid(WLAN_ELEMID_DSPARMS,
							   ie_ptr, ie_len);
	if (extracted_ie && extracted_ie[0] == WLAN_ELEMID_DSPARMS &&
	    extracted_ie[1] == WLAN_DS_PARAM_IE_MAX_LEN) {
		band = BIT(REG_BAND_2G) | BIT(REG_BAND_5G);
		primary_channel = *(extracted_ie + 2);
		mlme_debug("Extracted primary channel from DS : %d",
			   primary_channel);
		goto update_beacon;
	}

	/* For HT, VHT and non-6GHz HE, get channel from HTINFO IE */
	extracted_ie = (uint8_t *)
			wlan_get_ie_ptr_from_eid(WLAN_ELEMID_HTINFO_ANA,
						 ie_ptr, ie_len);
	if (extracted_ie && extracted_ie[0] == WLAN_ELEMID_HTINFO_ANA &&
	    extracted_ie[1] == sizeof(struct wlan_ie_htinfo_cmn)) {
		band = BIT(REG_BAND_2G) | BIT(REG_BAND_5G);
		primary_channel =
			((struct wlan_ie_htinfo *)extracted_ie)->
						hi_ie.hi_ctrlchannel;
		mlme_debug("Extracted primary channel from HT INFO : %d",
			   primary_channel);
		goto update_beacon;
	}
	/* For 6GHz, get channel from HE OP IE */
	extracted_ie = (uint8_t *)
			wlan_get_ext_ie_ptr_from_ext_id(WLAN_HEOP_OUI_TYPE,
							(uint8_t)
							WLAN_HEOP_OUI_SIZE,
							ie_ptr, ie_len);
	if (extracted_ie && !qdf_mem_cmp(&extracted_ie[2], WLAN_HEOP_OUI_TYPE,
					 WLAN_HEOP_OUI_SIZE) &&
	    extracted_ie[1] <= WLAN_MAX_HEOP_IE_LEN) {
		band = BIT(REG_BAND_6G);
		primary_channel = util_scan_get_6g_oper_channel(extracted_ie);
		mlme_debug("Extracted primary channel from HE OP : %d",
			   primary_channel);
		if (primary_channel)
			goto update_beacon;
	}

	mlme_err("Ignore beacon, Primary channel was not found in the candidate frame");
	goto err;

update_beacon:
	op_freq = wlan_reg_chan_band_to_freq(pdev, primary_channel, band);
	cm_inform_bcn_probe(cm_ctx, frame->frame, frame->frame_length,
			    op_freq,
			    frame->rssi,
			    cm_ctx->active_cm_id);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
	return QDF_STATUS_SUCCESS;
err:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
	return QDF_STATUS_E_FAILURE;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
bool wlan_cm_is_sae_auth_addr_conversion_required(struct wlan_objmgr_vdev *vdev)
{
	if (!wlan_vdev_get_mlo_external_sae_auth_conversion(vdev))
		return false;

	if (wlan_cm_is_vdev_roaming(vdev)) {
		if (!wlan_cm_roam_is_mlo_ap(vdev))
			return false;
	} else if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		return false;
	}

	return true;
}
#endif /* WLAN_FEATURE_11BE_MLO */

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * wlan_cm_reset_mlo_roam_peer_address() - this API reset the sae_roam_auth
 * structure values to zero.
 * @rso_config: pointer to struct rso_config
 *
 * Return: void
 */
static void wlan_cm_reset_mlo_roam_peer_address(struct rso_config *rso_config)
{
	qdf_mem_zero(&rso_config->sae_roam_auth.peer_mldaddr,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_zero(&rso_config->sae_roam_auth.peer_linkaddr,
		     QDF_MAC_ADDR_SIZE);
}

void wlan_cm_store_mlo_roam_peer_address(struct wlan_objmgr_pdev *pdev,
					 struct auth_offload_event *auth_event)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_config;
	struct qdf_mac_addr mld_addr;
	QDF_STATUS status;

	if (!pdev) {
		mlme_err("pdev is NULL");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, auth_event->vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		mlme_err("vdev %d not found", auth_event->vdev_id);
		return;
	}

	if (!wlan_vdev_get_mlo_external_sae_auth_conversion(vdev))
		goto rel_ref;

	if (!wlan_cm_is_vdev_roaming(vdev))
		goto rel_ref;

	rso_config = wlan_cm_get_rso_config(vdev);
	if (!rso_config)
		goto rel_ref;

	if (qdf_is_macaddr_zero(&auth_event->ta)) {
		/* ta have zero value for non-ML AP */
		rso_config->sae_roam_auth.is_mlo_ap = false;
		wlan_cm_reset_mlo_roam_peer_address(rso_config);
		goto rel_ref;
	}

	status = scm_get_mld_addr_by_link_addr(pdev, &auth_event->ap_bssid,
					       &mld_addr);

	rso_config->sae_roam_auth.is_mlo_ap = true;

	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_cm_reset_mlo_roam_peer_address(rso_config);
		goto rel_ref;
	}

	qdf_mem_copy(rso_config->sae_roam_auth.peer_mldaddr.bytes,
		     mld_addr.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(rso_config->sae_roam_auth.peer_linkaddr.bytes,
		     &auth_event->ap_bssid, QDF_MAC_ADDR_SIZE);

	mlme_debug("mld addr " QDF_MAC_ADDR_FMT "link addr " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(rso_config->sae_roam_auth.peer_mldaddr.bytes),
		   QDF_MAC_ADDR_REF(rso_config->sae_roam_auth.peer_linkaddr.bytes));
rel_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

struct qdf_mac_addr *
wlan_cm_roaming_get_peer_mld_addr(struct wlan_objmgr_vdev *vdev)
{
	struct rso_config *rso_config;

	rso_config = wlan_cm_get_rso_config(vdev);
	if (!rso_config)
		return NULL;

	if (qdf_is_macaddr_zero(&rso_config->sae_roam_auth.peer_mldaddr))
		return NULL;

	return &rso_config->sae_roam_auth.peer_mldaddr;
}

struct qdf_mac_addr *
wlan_cm_roaming_get_peer_link_addr(struct wlan_objmgr_vdev *vdev)
{
	struct rso_config *rso_config;

	rso_config = wlan_cm_get_rso_config(vdev);
	if (!rso_config)
		return NULL;

	if (qdf_is_macaddr_zero(&rso_config->sae_roam_auth.peer_linkaddr))
		return NULL;

	return &rso_config->sae_roam_auth.peer_linkaddr;
}

bool wlan_cm_roam_is_mlo_ap(struct wlan_objmgr_vdev *vdev)
{
	struct rso_config *rso_config;

	rso_config = wlan_cm_get_rso_config(vdev);
	if (!rso_config)
		return false;

	return rso_config->sae_roam_auth.is_mlo_ap;
}

QDF_STATUS
cm_roam_candidate_event_handler(struct wlan_objmgr_psoc *psoc,
				struct roam_scan_candidate_frame *candidate)
{
	return mlo_add_all_link_probe_rsp_to_scan_db(psoc, candidate);
}
#elif defined(WLAN_FEATURE_ROAM_OFFLOAD) /* end WLAN_FEATURE_11BE_MLO */
QDF_STATUS
cm_roam_candidate_event_handler(struct wlan_objmgr_psoc *psoc,
				struct roam_scan_candidate_frame *candidate)
{
	return wlan_cm_add_frame_to_scan_db(psoc, candidate);
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
