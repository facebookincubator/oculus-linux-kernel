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
 * DOC: wlan_policy_mgr_action.c
 *
 * WLAN Concurrenct Connection Management APIs
 *
 */

/* Include files */

#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "wlan_objmgr_global_obj.h"
#include "qdf_platform.h"
#include "wlan_nan_api.h"
#include "nan_ucfg_api.h"
#include "wlan_mlme_api.h"
#include "sap_api.h"
#include "wlan_mlme_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "target_if.h"
#include "wlan_cm_api.h"
#include "wlan_mlo_link_force.h"
#include "wlan_mlo_mgr_sta.h"
#include "wlan_mlo_mgr_link_switch.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_policy_mgr_ll_sap.h"

enum policy_mgr_conc_next_action (*policy_mgr_get_current_pref_hw_mode_ptr)
	(struct wlan_objmgr_psoc *psoc);

#define HW_MODE_DUMP_MAX_LEN 100
void
policy_mgr_dump_freq_range_n_vdev_map(uint32_t num_vdev_mac_entries,
			struct policy_mgr_vdev_mac_map *vdev_mac_map,
			uint32_t num_mac_freq,
			struct policy_mgr_pdev_mac_freq_map *mac_freq_range)
{
	char log_str[HW_MODE_DUMP_MAX_LEN] = {0};
	uint32_t str_len = HW_MODE_DUMP_MAX_LEN;
	uint32_t len = 0;
	uint32_t i;

	if (mac_freq_range) {
		for (i = 0, len = 0; i < num_mac_freq; i++)
			len += qdf_scnprintf(log_str + len, str_len - len,
					    "mac %d: %d => %d ",
					    mac_freq_range[i].mac_id,
					    mac_freq_range[i].start_freq,
					    mac_freq_range[i].end_freq);
		if (num_mac_freq)
			policymgr_nofl_debug("Freq range:: %s", log_str);
	}

	if (!vdev_mac_map || !num_vdev_mac_entries)
		return;

	for (i = 0, len = 0; i < num_vdev_mac_entries; i++)
		len += qdf_scnprintf(log_str + len, str_len - len,
				     "vdev %d -> mac %d ",
				     vdev_mac_map[i].vdev_id,
				     vdev_mac_map[i].mac_id);
	policymgr_nofl_debug("Vdev Map:: %s", log_str);
}

void policy_mgr_hw_mode_transition_cb(uint32_t old_hw_mode_index,
			uint32_t new_hw_mode_index,
			uint32_t num_vdev_mac_entries,
			struct policy_mgr_vdev_mac_map *vdev_mac_map,
			uint32_t num_mac_freq,
			struct policy_mgr_pdev_mac_freq_map *mac_freq_range,
			struct wlan_objmgr_psoc *context)
{
	QDF_STATUS status;
	struct policy_mgr_hw_mode_params hw_mode;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(context);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}

	if (!vdev_mac_map) {
		policy_mgr_err("vdev_mac_map is NULL");
		return;
	}

	status = policy_mgr_get_hw_mode_from_idx(context, new_hw_mode_index,
						 &hw_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Get HW mode for index %d reason: %d",
			       new_hw_mode_index, status);
		return;
	}

	policy_mgr_debug("HW mode: old %d new %d, DBS %d Agile %d SBS %d, MAC0:: SS:Tx %d Rx %d, BW %d band %d, MAC1:: SS:Tx %d Rx %d, BW %d",
			 old_hw_mode_index, new_hw_mode_index, hw_mode.dbs_cap,
			 hw_mode.agile_dfs_cap, hw_mode.sbs_cap,
			 hw_mode.mac0_tx_ss, hw_mode.mac0_rx_ss,
			 hw_mode.mac0_bw, hw_mode.mac0_band_cap,
			 hw_mode.mac1_tx_ss, hw_mode.mac1_rx_ss,
			 hw_mode.mac1_bw);
	policy_mgr_dump_freq_range_n_vdev_map(num_vdev_mac_entries,
					      vdev_mac_map, num_mac_freq,
					      mac_freq_range);

	/* update pm_conc_connection_list */
	policy_mgr_update_hw_mode_conn_info(context, num_vdev_mac_entries,
					    vdev_mac_map, hw_mode,
					    num_mac_freq, mac_freq_range);

	if (pm_ctx->mode_change_cb)
		pm_ctx->mode_change_cb();

	return;
}

QDF_STATUS policy_mgr_check_n_start_opportunistic_timer(
		struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	enum policy_mgr_conn_update_reason reason =
				POLICY_MGR_UPDATE_REASON_TIMER_START;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("PM ctx not valid. Oppurtunistic timer cannot start");
		return QDF_STATUS_E_FAILURE;
	}
	if (policy_mgr_need_opportunistic_upgrade(psoc, &reason)) {
	/* let's start the timer */
	qdf_mc_timer_stop(&pm_ctx->dbs_opportunistic_timer);
	status = qdf_mc_timer_start(
				&pm_ctx->dbs_opportunistic_timer,
				DBS_OPPORTUNISTIC_TIME * 1000);
	if (!QDF_IS_STATUS_SUCCESS(status))
		policy_mgr_err("Failed to start dbs opportunistic timer");
	}
	return status;
}

QDF_STATUS policy_mgr_pdev_set_hw_mode(struct wlan_objmgr_psoc *psoc,
		uint32_t session_id,
		enum hw_mode_ss_config mac0_ss,
		enum hw_mode_bandwidth mac0_bw,
		enum hw_mode_ss_config mac1_ss,
		enum hw_mode_bandwidth mac1_bw,
		enum hw_mode_mac_band_cap mac0_band_cap,
		enum hw_mode_dbs_capab dbs,
		enum hw_mode_agile_dfs_capab dfs,
		enum hw_mode_sbs_capab sbs,
		enum policy_mgr_conn_update_reason reason,
		uint8_t next_action, enum policy_mgr_conc_next_action action,
		uint32_t request_id)
{
	int8_t hw_mode_index;
	struct policy_mgr_hw_mode msg;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!pm_ctx->sme_cbacks.sme_pdev_set_hw_mode) {
		policy_mgr_debug("NOT supported");
		return QDF_STATUS_E_NOSUPPORT;
	}

	/*
	 * if HW is not capable of doing 2x2 or ini config disabled 2x2, don't
	 * allow to request FW for 2x2
	 */
	if ((HW_MODE_SS_2x2 == mac0_ss) && (!pm_ctx->user_cfg.enable2x2)) {
		policy_mgr_debug("2x2 is not allowed downgrading to 1x1 for mac0");
		mac0_ss = HW_MODE_SS_1x1;
	}
	if ((HW_MODE_SS_2x2 == mac1_ss) && (!pm_ctx->user_cfg.enable2x2)) {
		policy_mgr_debug("2x2 is not allowed downgrading to 1x1 for mac1");
		mac1_ss = HW_MODE_SS_1x1;
	}

	hw_mode_index = policy_mgr_get_hw_mode_idx_from_dbs_hw_list(psoc,
			mac0_ss, mac0_bw, mac1_ss, mac1_bw, mac0_band_cap,
			dbs, dfs, sbs);
	if (hw_mode_index < 0) {
		policy_mgr_err("Invalid HW mode index obtained");
		return QDF_STATUS_E_FAILURE;
	}

	/* Don't send WMI_PDEV_SET_HW_MODE_CMDID to FW if existing SAP / GO is
	 * in CAC-in-progress state. Host is blocking this command as FW is
	 * having design limitation and FW don't expect this command when CAC
	 * is in progress state.
	 */
	if (pm_ctx->hdd_cbacks.hdd_is_cac_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_cac_in_progress() &&
	    !policy_mgr_is_hw_dbs_2x2_capable(psoc)) {
		policy_mgr_err("SAP CAC_IN_PROGRESS state, drop WMI_PDEV_SET_HW_MODE_CMDID");
		return QDF_STATUS_E_FAILURE;
	}

	msg.hw_mode_index = hw_mode_index;
	msg.set_hw_mode_cb = (void *)policy_mgr_pdev_set_hw_mode_cb;
	msg.reason = reason;
	msg.session_id = session_id;
	msg.next_action = next_action;
	msg.action = action;
	msg.context = psoc;
	msg.request_id = request_id;

	policy_mgr_debug("set hw mode to sme: hw_mode_index: %d session:%d reason:%d action %d request_id %d",
			 msg.hw_mode_index, msg.session_id, msg.reason, action,
			 msg.request_id);

	status = pm_ctx->sme_cbacks.sme_pdev_set_hw_mode(msg);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to set hw mode to SME");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_get_sap_bw() - get current SAP bandwidth
 * @psoc: Pointer to psoc
 * @bw: Buffer to update the bandwidth
 *
 * Get the current SAP bandwidth. This API supports only single SAP
 * concurrencies and doesn't cover multi SAP(e.g. SAP+SAP).
 *
 * return : QDF_STATUS
 */
static QDF_STATUS
policy_mgr_get_sap_bw(struct wlan_objmgr_psoc *psoc, enum phy_ch_width *bw)
{
	uint32_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS + 1];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS + 1];
	struct wlan_objmgr_vdev *vdev;

	if (policy_mgr_get_mode_specific_conn_info(psoc, &freq_list[0],
						   &vdev_id_list[0],
						   PM_SAP_MODE) != 1 ||
	    !WLAN_REG_IS_6GHZ_CHAN_FREQ(freq_list[0]))
		return QDF_STATUS_E_NOSUPPORT;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id_list[0],
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d is NULL", vdev_id_list[0]);
		return QDF_STATUS_E_INVAL;
	}

	*bw = wlan_mlme_get_ap_oper_ch_width(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_get_sap_ch_width_update_action() - get SAP ch_width update action
 * @psoc: Pointer to psoc
 * @vdev_id: Vdev id of the caller
 * @ch_freq: channel frequency of new connection
 * @next_action: next action to happen in order to update bandwidth
 * @reason: Bandwidth upgrade/downgrade reason
 *
 * Check if current operating SAP needs a downgrade to 160MHz or an upgrade
 * to 320MHz based on the new connection.
 *
 * return : None
 */
static void
policy_mgr_get_sap_ch_width_update_action(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, qdf_freq_t ch_freq,
				enum policy_mgr_conc_next_action *next_action,
				enum policy_mgr_conn_update_reason *reason)
{
	enum phy_ch_width cur_bw;
	qdf_freq_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS + 1];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS + 1];
	bool eht_capab = false;

	/*
	 * Stop any running opportunistic timer as it will be started after
	 * decision if required.
	 */
	policy_mgr_stop_opportunistic_timer(psoc);
	if (QDF_IS_STATUS_ERROR(wlan_psoc_mlme_get_11be_capab(psoc,
							      &eht_capab)) ||
	    !eht_capab ||
	    QDF_IS_STATUS_ERROR(policy_mgr_get_sap_bw(psoc, &cur_bw)) ||
	    cur_bw < CH_WIDTH_160MHZ)
		return;

	policy_mgr_get_mode_specific_conn_info(psoc, &freq_list[0],
					       &vdev_id_list[0], PM_SAP_MODE);
	if (cur_bw == CH_WIDTH_320MHZ && ch_freq &&
	    policy_mgr_is_conn_lead_to_dbs_sbs(psoc, vdev_id, ch_freq))
		*next_action = PM_DOWNGRADE_BW;
	else if (cur_bw == CH_WIDTH_160MHZ &&
		 !ch_freq &&
		 !policy_mgr_is_conn_lead_to_dbs_sbs(psoc,
					vdev_id_list[0], freq_list[0]) &&
		 (reason &&
		  (*reason == POLICY_MGR_UPDATE_REASON_TIMER_START ||
		   *reason == POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC)))
		*next_action = PM_UPGRADE_BW;
}

enum policy_mgr_conc_next_action policy_mgr_need_opportunistic_upgrade(
		struct wlan_objmgr_psoc *psoc,
		enum policy_mgr_conn_update_reason *reason)
{
	uint32_t conn_index;
	enum policy_mgr_conc_next_action upgrade = PM_NOP;
	enum policy_mgr_conc_next_action preferred_dbs_action;
	uint8_t mac = 0;
	struct policy_mgr_hw_mode_params hw_mode;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (policy_mgr_is_hwmode_offload_enabled(psoc)) {
		policy_mgr_get_sap_ch_width_update_action(psoc,
							  WLAN_INVALID_VDEV_ID,
							  0, &upgrade,
							  reason);
		return upgrade;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		goto exit;
	}

	if (policy_mgr_is_hw_dbs_capable(psoc) == false) {
		policy_mgr_rl_debug("driver isn't dbs capable, no further action needed");
		goto exit;
	}

	status = policy_mgr_get_current_hw_mode(psoc, &hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("policy_mgr_get_current_hw_mode failed");
		goto exit;
	}
	if (!hw_mode.dbs_cap) {
		policy_mgr_debug("current HW mode is non-DBS capable");
		goto exit;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	/* Are both mac's still in use */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		policy_mgr_debug("index:%d mac:%d in_use:%d chan:%d org_nss:%d",
			conn_index,
			pm_conc_connection_list[conn_index].mac,
			pm_conc_connection_list[conn_index].in_use,
			pm_conc_connection_list[conn_index].freq,
			pm_conc_connection_list[conn_index].original_nss);
		if ((pm_conc_connection_list[conn_index].mac == 0) &&
			pm_conc_connection_list[conn_index].in_use) {
			mac |= POLICY_MGR_MAC0;
			if (POLICY_MGR_MAC0_AND_MAC1 == mac) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				goto done;
			}
		} else if ((pm_conc_connection_list[conn_index].mac == 1) &&
			pm_conc_connection_list[conn_index].in_use) {
			mac |= POLICY_MGR_MAC1;
			if (policy_mgr_is_hw_dbs_required_for_band(
					psoc, HW_MODE_MAC_BAND_2G) &&
			    WLAN_REG_IS_24GHZ_CH_FREQ(
				pm_conc_connection_list[conn_index].freq)) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				policy_mgr_debug("2X2 DBS capable with 2.4 GHZ connection");
				goto done;
			}
			if (POLICY_MGR_MAC0_AND_MAC1 == mac) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				goto done;
			}
		}
	}
	/* Let's request for single MAC mode */
	upgrade = PM_SINGLE_MAC;
	if (reason)
		*reason = POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC;
	/* Is there any connection had an initial connection with 2x2 */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].original_nss == 2) &&
			pm_conc_connection_list[conn_index].in_use) {
			upgrade = PM_SINGLE_MAC_UPGRADE;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			goto done;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

done:
	if (upgrade == PM_NOP && hw_mode.dbs_cap &&
	    policy_mgr_is_2x2_1x1_dbs_capable(psoc)) {
		preferred_dbs_action =
			policy_mgr_get_preferred_dbs_action_table(
					psoc, INVALID_VDEV_ID, 0, 0);
		if (hw_mode.action_type == PM_DBS1 &&
		    preferred_dbs_action == PM_DBS2) {
			upgrade = PM_DBS2_DOWNGRADE;
			if (reason)
				*reason =
				POLICY_MGR_UPDATE_REASON_PRI_VDEV_CHANGE;
		} else if (hw_mode.action_type == PM_DBS2 &&
		    preferred_dbs_action == PM_DBS1) {
			upgrade = PM_DBS1_DOWNGRADE;
			if (reason)
				*reason =
				POLICY_MGR_UPDATE_REASON_PRI_VDEV_CHANGE;
		}
	}
exit:
	return upgrade;
}

QDF_STATUS policy_mgr_update_connection_info(struct wlan_objmgr_psoc *psoc,
					uint32_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0, ch_freq, cur_freq;
	bool found = false;
	struct policy_mgr_vdev_entry_info conn_table_entry;
	enum policy_mgr_chain_mode chain_mask = POLICY_MGR_ONE_ONE;
	uint8_t nss_2g, nss_5g;
	enum policy_mgr_con_mode mode;
	uint32_t nss = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == pm_conc_connection_list[conn_index].vdev_id) {
			/* debug msg */
			found = true;
			break;
		}
		conn_index++;
	}

	if (!found) {
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		/* err msg */
		policy_mgr_err("can't find vdev_id %d in pm_conc_connection_list",
			vdev_id);
		return QDF_STATUS_NOT_INITIALIZED;
	}
	if (pm_ctx->wma_cbacks.wma_get_connection_info) {
		status = pm_ctx->wma_cbacks.wma_get_connection_info(
				vdev_id, &conn_table_entry);
		if (QDF_STATUS_SUCCESS != status) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			policy_mgr_err("can't find vdev_id %d in connection table",
			vdev_id);
			return status;
		}
	} else {
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		policy_mgr_err("wma_get_connection_info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cur_freq = pm_conc_connection_list[conn_index].freq;

	mode = policy_mgr_qdf_opmode_to_pm_con_mode(
					psoc,
					wlan_get_opmode_from_vdev_id(
								pm_ctx->pdev,
								vdev_id),
					vdev_id);

	ch_freq = conn_table_entry.mhz;
	status = policy_mgr_get_nss_for_vdev(psoc, mode, &nss_2g, &nss_5g);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if ((WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq) && nss_2g > 1) ||
		    (WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) && nss_5g > 1))
			chain_mask = POLICY_MGR_TWO_TWO;
		else
			chain_mask = POLICY_MGR_ONE_ONE;
		nss = (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) ? nss_2g : nss_5g;
	} else {
		policy_mgr_err("Error in getting nss");
	}

	policy_mgr_debug("update PM connection table for vdev:%d", vdev_id);

	/* add the entry */
	policy_mgr_update_conc_list(
			psoc, conn_index, mode, ch_freq,
			policy_mgr_get_bw(conn_table_entry.chan_width),
			conn_table_entry.mac_id, chain_mask,
			nss, vdev_id, true, true, conn_table_entry.ch_flagext);
	policy_mgr_dump_current_concurrency(psoc);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	/* do we need to change the HW mode */
	policy_mgr_check_n_start_opportunistic_timer(psoc);

	if (policy_mgr_is_conc_sap_present_on_sta_freq(psoc, mode, cur_freq) &&
	    policy_mgr_update_indoor_concurrency(psoc, vdev_id, 0,
						 SWITCH_WITH_CONCURRENCY))
		wlan_reg_recompute_current_chan_list(psoc, pm_ctx->pdev);
	else if (policy_mgr_update_indoor_concurrency(psoc, vdev_id, cur_freq,
						SWITCH_WITHOUT_CONCURRENCY))
		wlan_reg_recompute_current_chan_list(psoc, pm_ctx->pdev);
	else if (wlan_reg_get_keep_6ghz_sta_cli_connection(pm_ctx->pdev) &&
		 (mode == PM_STA_MODE || mode == PM_P2P_CLIENT_MODE))
		wlan_reg_recompute_current_chan_list(psoc, pm_ctx->pdev);

	ml_nlink_conn_change_notify(
		psoc, vdev_id, ml_nlink_connection_updated_evt, NULL);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_update_and_wait_for_connection_update(
		struct wlan_objmgr_psoc *psoc,
		uint8_t session_id,
		uint32_t ch_freq,
		enum policy_mgr_conn_update_reason reason)
{
	QDF_STATUS status;

	policy_mgr_debug("session:%d ch_freq:%d reason:%d",
			 session_id, ch_freq, reason);

	status = policy_mgr_reset_connection_update(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		policy_mgr_err("clearing event failed");

	status = policy_mgr_current_connections_update(
			psoc, session_id, ch_freq, reason,
			POLICY_MGR_DEF_REQ_ID);
	if (QDF_STATUS_E_FAILURE == status) {
		policy_mgr_err("connections update failed");
		return QDF_STATUS_E_FAILURE;
	}

	/* Wait only when status is success */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		status = policy_mgr_wait_for_connection_update(psoc);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("qdf wait for event failed");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_dbs_allowed_for_concurrency(
		struct wlan_objmgr_psoc *psoc, enum QDF_OPMODE new_conn_mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t count, dbs_for_sta_sta, dbs_for_sta_p2p;
	bool ret = true;
	uint32_t ch_sel_plcy;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return ret;
	}

	count = policy_mgr_get_connection_count(psoc);

	if (count != 1 || new_conn_mode == QDF_MAX_NO_OF_MODE)
		return ret;

	ch_sel_plcy = pm_ctx->cfg.chnl_select_plcy;
	dbs_for_sta_sta = PM_CHANNEL_SELECT_LOGIC_STA_STA_GET(ch_sel_plcy);
	dbs_for_sta_p2p = PM_CHANNEL_SELECT_LOGIC_STA_P2P_GET(ch_sel_plcy);

	switch (pm_conc_connection_list[0].mode) {
	case PM_STA_MODE:
		switch (new_conn_mode) {
		case QDF_STA_MODE:
			if (!dbs_for_sta_sta)
				return false;
			break;
		case QDF_P2P_DEVICE_MODE:
		case QDF_P2P_CLIENT_MODE:
		case QDF_P2P_GO_MODE:
			if (!dbs_for_sta_p2p)
				return false;
			break;
		default:
			break;
		}
		break;
	case PM_P2P_CLIENT_MODE:
	case PM_P2P_GO_MODE:
		switch (new_conn_mode) {
		case QDF_STA_MODE:
			if (!dbs_for_sta_p2p)
				return false;
			break;
		default:
			break;
		}
		break;
	case PM_NAN_DISC_MODE:
		switch (new_conn_mode) {
		case QDF_STA_MODE:
		case QDF_SAP_MODE:
		case QDF_NDI_MODE:
			return true;
		default:
			return false;
		}
		break;
	default:
		break;
	}

	return ret;
}

bool policy_mgr_is_chnl_in_diff_band(struct wlan_objmgr_psoc *psoc,
				     uint32_t ch_freq)
{
	uint8_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	/*
	 * check given channel freq against already existing connections'
	 * channel freqs. if they differ then channels are in different bands
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use)
			if (!WLAN_REG_IS_SAME_BAND_FREQS(
			    ch_freq, pm_conc_connection_list[i].freq)) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				policy_mgr_debug("channel is in diff band");
				return true;
			}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return false;
}

bool policy_mgr_is_hwmode_set_for_given_chnl(struct wlan_objmgr_psoc *psoc,
					     uint32_t ch_freq)
{
	enum policy_mgr_band band;
	bool is_hwmode_dbs, dbs_required_for_2g;

	if (policy_mgr_is_hwmode_offload_enabled(psoc))
		return true;

	if (policy_mgr_is_hw_dbs_capable(psoc) == false)
		return true;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq))
		band = POLICY_MGR_BAND_24;
	else
		band = POLICY_MGR_BAND_5;

	is_hwmode_dbs = policy_mgr_is_current_hwmode_dbs(psoc);
	dbs_required_for_2g = policy_mgr_is_hw_dbs_required_for_band(
					psoc, HW_MODE_MAC_BAND_2G);
	/*
	 * If HW supports 2x2 chains in DBS HW mode and if DBS HW mode is not
	 * yet set then this is the right time to block the connection.
	 */
	if (band == POLICY_MGR_BAND_24 && dbs_required_for_2g &&
	    !is_hwmode_dbs) {
		policy_mgr_err("HW mode is not yet in DBS!!!!!");
		return false;
	}

	return true;
}

/**
 * policy_mgr_pri_id_to_con_mode() - convert policy_mgr_pri_id to
 * policy_mgr_con_mode
 * @pri_id: policy_mgr_pri_id
 *
 * The help function converts policy_mgr_pri_id type to  policy_mgr_con_mode
 * type.
 *
 * Return: policy_mgr_con_mode type.
 */
static
enum policy_mgr_con_mode policy_mgr_pri_id_to_con_mode(
	enum policy_mgr_pri_id pri_id)
{
	switch (pri_id) {
	case PM_STA_PRI_ID:
		return PM_STA_MODE;
	case PM_SAP_PRI_ID:
		return PM_SAP_MODE;
	case PM_P2P_GO_PRI_ID:
		return PM_P2P_GO_MODE;
	case PM_P2P_CLI_PRI_ID:
		return PM_P2P_CLIENT_MODE;
	default:
		return PM_MAX_NUM_OF_MODE;
	}
}

enum policy_mgr_conc_next_action
policy_mgr_get_preferred_dbs_action_table(
	struct wlan_objmgr_psoc *psoc,
	uint32_t vdev_id,
	uint32_t ch_freq,
	enum policy_mgr_conn_update_reason reason)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum policy_mgr_con_mode pri_conn_mode = PM_MAX_NUM_OF_MODE;
	enum policy_mgr_con_mode new_conn_mode = PM_MAX_NUM_OF_MODE;
	enum QDF_OPMODE new_conn_op_mode = QDF_MAX_NO_OF_MODE;
	bool band_pref_5g = true;
	bool vdev_priority_enabled = false;
	bool dbs_2x2_5g_1x1_2g_supported;
	bool dbs_2x2_2g_1x1_5g_supported;
	uint32_t vdev_pri_list, vdev_pri_id;
	uint32_t ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS + 1];
	uint8_t vdev_list[MAX_NUMBER_OF_CONC_CONNECTIONS + 1];
	uint32_t vdev_count = 0;
	uint32_t i;
	bool found;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return PM_NOP;
	}
	dbs_2x2_5g_1x1_2g_supported =
		policy_mgr_is_2x2_5G_1x1_2G_dbs_capable(psoc);
	dbs_2x2_2g_1x1_5g_supported =
		policy_mgr_is_2x2_2G_1x1_5G_dbs_capable(psoc);
	policy_mgr_debug("target support DBS1 %d DBS2 %d",
			 dbs_2x2_5g_1x1_2g_supported,
			 dbs_2x2_2g_1x1_5g_supported);
	/*
	 * If both DBS1 and DBS2 not supported, this should be Legacy Single
	 * DBS mode HW. The policy_mgr_psoc_enable has setup the correct
	 * action tables.
	 */
	if (!dbs_2x2_5g_1x1_2g_supported && !dbs_2x2_2g_1x1_5g_supported)
		return PM_NOP;
	if (!dbs_2x2_5g_1x1_2g_supported) {
		band_pref_5g = false;
		policy_mgr_debug("target only supports DBS2!");
		goto DONE;
	}
	if (!dbs_2x2_2g_1x1_5g_supported) {
		policy_mgr_debug("target only supports DBS1!");
		goto DONE;
	}
	if (PM_GET_BAND_PREFERRED(pm_ctx->cfg.dbs_selection_plcy) == 1)
		band_pref_5g = false;

	if (PM_GET_VDEV_PRIORITY_ENABLED(
	    pm_ctx->cfg.dbs_selection_plcy) == 1 &&
	    pm_ctx->cfg.vdev_priority_list)
		vdev_priority_enabled = true;

	if (!vdev_priority_enabled)
		goto DONE;

	if (vdev_id != INVALID_VDEV_ID && ch_freq) {
		if (pm_ctx->hdd_cbacks.hdd_get_device_mode)
			new_conn_op_mode = pm_ctx->hdd_cbacks.
					hdd_get_device_mode(vdev_id);

		new_conn_mode =
			policy_mgr_qdf_opmode_to_pm_con_mode(psoc,
							     new_conn_op_mode,
							     vdev_id);
		if (new_conn_mode == PM_MAX_NUM_OF_MODE)
			policy_mgr_debug("new vdev %d op_mode %d freq %d reason %d: not prioritized",
					 vdev_id, new_conn_op_mode,
					 ch_freq, reason);
		else
			policy_mgr_debug("new vdev %d op_mode %d freq %d : reason %d",
					 vdev_id, new_conn_op_mode, ch_freq,
					 reason);
	}
	vdev_pri_list = pm_ctx->cfg.vdev_priority_list;
	while (vdev_pri_list) {
		vdev_pri_id = vdev_pri_list & 0xF;
		pri_conn_mode = policy_mgr_pri_id_to_con_mode(vdev_pri_id);
		if (pri_conn_mode == PM_MAX_NUM_OF_MODE) {
			policy_mgr_debug("vdev_pri_id %d prioritization not supported",
					 vdev_pri_id);
			goto NEXT;
		}
		vdev_count = policy_mgr_get_mode_specific_conn_info(
				psoc, ch_freq_list, vdev_list, pri_conn_mode);
		/**
		 * Take care of duplication case, the vdev id may
		 * exist in the conn list already with old chan.
		 * Replace with new chan before make decision.
		 */
		found = false;
		for (i = 0; i < vdev_count; i++) {
			policy_mgr_debug("[%d] vdev %d chan %d conn_mode %d",
					 i, vdev_list[i], ch_freq_list[i],
					 pri_conn_mode);

			if (new_conn_mode == pri_conn_mode &&
			    vdev_list[i] == vdev_id) {
				ch_freq_list[i] = ch_freq;
				found = true;
			}
		}
		/**
		 * The new coming vdev should be added to the list to
		 * make decision if it is prioritized.
		 */
		if (!found && new_conn_mode == pri_conn_mode) {
			ch_freq_list[vdev_count] = ch_freq;
			vdev_list[vdev_count++] = vdev_id;
		}
		/**
		 * if more than one vdev has same priority, keep "band_pref_5g"
		 * value as default band preference setting.
		 */
		if (vdev_count > 1)
			break;
		/**
		 * select the only active vdev (or new coming vdev) chan as
		 * preferred band.
		 */
		if (vdev_count > 0) {
			band_pref_5g =
				WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq_list[0]);
			break;
		}
NEXT:
		vdev_pri_list >>= 4;
	}
DONE:
	policy_mgr_debug("band_pref_5g %d", band_pref_5g);
	if (band_pref_5g)
		return PM_DBS1;
	else
		return PM_DBS2;
}

/**
 * policy_mgr_get_second_conn_action_table() - get second conn action table
 * @psoc: Pointer to psoc
 * @vdev_id: vdev Id
 * @ch_freq: channel frequency of vdev.
 * @reason: reason of request
 *
 * Get the action table based on current HW Caps and INI user preference.
 * This function will be called by policy_mgr_current_connections_update during
 * DBS action decision.
 *
 * return : action table address
 */
static policy_mgr_next_action_two_connection_table_type *
policy_mgr_get_second_conn_action_table(
	struct wlan_objmgr_psoc *psoc,
	uint32_t vdev_id,
	uint32_t ch_freq,
	enum policy_mgr_conn_update_reason reason)
{
	enum policy_mgr_conc_next_action preferred_action;

	if (!policy_mgr_is_2x2_1x1_dbs_capable(psoc))
		return next_action_two_connection_table;

	preferred_action = policy_mgr_get_preferred_dbs_action_table(
				psoc, vdev_id, ch_freq, reason);
	switch (preferred_action) {
	case PM_DBS2:
		return next_action_two_connection_2x2_2g_1x1_5g_table;
	default:
		return next_action_two_connection_table;
	}
}

/**
 * policy_mgr_get_third_conn_action_table() - get third connection action table
 * @psoc: Pointer to psoc
 * @vdev_id: vdev Id
 * @ch_freq: channel frequency of vdev.
 * @reason: reason of request
 *
 * Get the action table based on current HW Caps and INI user preference.
 * This function will be called by policy_mgr_current_connections_update during
 * DBS action decision.
 *
 * return : action table address
 */
static policy_mgr_next_action_three_connection_table_type *
policy_mgr_get_third_conn_action_table(
	struct wlan_objmgr_psoc *psoc,
	uint32_t vdev_id,
	uint32_t ch_freq,
	enum policy_mgr_conn_update_reason reason)
{
	enum policy_mgr_conc_next_action preferred_action;

	if (!policy_mgr_is_2x2_1x1_dbs_capable(psoc))
		return next_action_three_connection_table;

	preferred_action = policy_mgr_get_preferred_dbs_action_table(
				psoc, vdev_id, ch_freq, reason);
	switch (preferred_action) {
	case PM_DBS2:
		return next_action_three_connection_2x2_2g_1x1_5g_table;
	default:
		return next_action_three_connection_table;
	}
}

bool
policy_mgr_is_conn_lead_to_dbs_sbs(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, qdf_freq_t freq)
{
	struct connection_info info[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint32_t connection_count, i;

	connection_count = policy_mgr_get_connection_info(psoc, info);
	for (i = 0; i < connection_count; i++) {
		/* Ignore the vdev id for which freq is passed */
		if (vdev_id == info[i].vdev_id)
			continue;
		if (!policy_mgr_2_freq_always_on_same_mac(psoc, freq,
							  info[i].ch_freq))
			return true;
	}
	return false;
}

static QDF_STATUS
policy_mgr_get_next_action(struct wlan_objmgr_psoc *psoc,
			   uint32_t session_id,
			   uint32_t ch_freq,
			   enum policy_mgr_conn_update_reason reason,
			   enum policy_mgr_conc_next_action *next_action)
{
	uint32_t num_connections = 0;
	enum policy_mgr_one_connection_mode second_index = 0;
	enum policy_mgr_two_connection_mode third_index = 0;
	policy_mgr_next_action_two_connection_table_type *second_conn_table;
	policy_mgr_next_action_three_connection_table_type *third_conn_table;
	enum policy_mgr_band band;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum QDF_OPMODE new_conn_mode = QDF_MAX_NO_OF_MODE;

	if (!next_action) {
		policy_mgr_err("next_action is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (policy_mgr_is_hwmode_offload_enabled(psoc)) {
		*next_action = PM_NOP;
		policy_mgr_get_sap_ch_width_update_action(psoc, session_id,
							  ch_freq,
							  next_action, &reason);
		return QDF_STATUS_SUCCESS;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq))
		band = POLICY_MGR_BAND_24;
	else
		band = POLICY_MGR_BAND_5;

	num_connections = policy_mgr_get_connection_count(psoc);

	policy_mgr_debug("num_connections=%d freq=%d",
			 num_connections, ch_freq);

	switch (num_connections) {
	case 0:
		if (band == POLICY_MGR_BAND_24)
			if (policy_mgr_is_hw_dbs_required_for_band(
					psoc, HW_MODE_MAC_BAND_2G))
				*next_action = PM_DBS;
			else
				*next_action = PM_NOP;
		else
			*next_action = PM_NOP;
		break;
	case 1:
		second_index =
			policy_mgr_get_second_connection_pcl_table_index(psoc);
		if (PM_MAX_ONE_CONNECTION_MODE == second_index) {
			policy_mgr_err(
			"couldn't find index for 2nd connection next action table");
			return QDF_STATUS_E_FAILURE;
		}
		second_conn_table = policy_mgr_get_second_conn_action_table(
			psoc, session_id, ch_freq, reason);
		*next_action = (*second_conn_table)[second_index][band];
		break;
	case 2:
		third_index =
			policy_mgr_get_third_connection_pcl_table_index(psoc);
		if (PM_MAX_TWO_CONNECTION_MODE == third_index) {
			policy_mgr_err(
			"couldn't find index for 3rd connection next action table");
			return QDF_STATUS_E_FAILURE;
		}
		third_conn_table = policy_mgr_get_third_conn_action_table(
			psoc, session_id, ch_freq, reason);
		*next_action = (*third_conn_table)[third_index][band];
		break;
	default:
		policy_mgr_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	/*
	 * There is no adapter associated with NAN Discovery, hence skip the
	 * HDD callback and fill separately.
	 */
	if (reason == POLICY_MGR_UPDATE_REASON_NAN_DISCOVERY)
		new_conn_mode = QDF_NAN_DISC_MODE;
	else if (pm_ctx->hdd_cbacks.hdd_get_device_mode)
		new_conn_mode = pm_ctx->hdd_cbacks.
					hdd_get_device_mode(session_id);

	/*
	 * Based on channel_select_logic_conc ini, hw mode is set
	 * when second connection is about to come up that results
	 * in STA+STA and STA+P2P concurrency.
	 * 1) If MCC is set and if current hw mode is dbs, hw mode
	 *  should be set to single mac for above concurrency.
	 * 2) If MCC is set and if current hw mode is not dbs, hw
	 *  mode change is not required.
	 */
	if (policy_mgr_is_current_hwmode_dbs(psoc) &&
		!policy_mgr_is_dbs_allowed_for_concurrency(psoc, new_conn_mode))
		*next_action = PM_SINGLE_MAC;
	else if (!policy_mgr_is_current_hwmode_dbs(psoc) &&
		!policy_mgr_is_dbs_allowed_for_concurrency(psoc, new_conn_mode))
		*next_action = PM_NOP;

	policy_mgr_debug("idx2=%d idx3=%d next_action=%d, band=%d reason=%d session_id=%d",
			 second_index, third_index, *next_action, band,
			 reason, session_id);

	return QDF_STATUS_SUCCESS;
}

static bool
policy_mgr_is_hw_mode_change_required(struct wlan_objmgr_psoc *psoc,
				      uint32_t ch_freq, uint8_t vdev_id)
{
	if (policy_mgr_is_hw_dbs_required_for_band(psoc, HW_MODE_MAC_BAND_2G)) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq))
			return true;
	} else {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq) &&
		    policy_mgr_is_any_mode_active_on_band_along_with_session
			(psoc, vdev_id, POLICY_MGR_BAND_5))
			return true;

		if (WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) &&
		    policy_mgr_is_any_mode_active_on_band_along_with_session
			(psoc, vdev_id, POLICY_MGR_BAND_24))
			return true;
	}

	return false;
}

static bool
policy_mgr_is_ch_width_downgrade_required(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id,
					  struct scan_cache_entry *entry,
					  qdf_list_t *scan_list)

{
	if (policy_mgr_is_conn_lead_to_dbs_sbs(psoc, vdev_id,
					       entry->channel.chan_freq) ||
	    wlan_cm_bss_mlo_type(psoc, entry, scan_list))
		return true;

	return false;
}

static uint32_t
policy_mgr_check_for_hw_mode_change(struct wlan_objmgr_psoc *psoc,
				    qdf_list_t *scan_list, uint8_t vdev_id)
{

	struct scan_cache_node *scan_node = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	uint32_t ch_freq = 0;
	struct scan_cache_entry *entry;
	bool eht_capab =  false, check_sap_bw_downgrade = false;
	enum phy_ch_width cur_bw = CH_WIDTH_INVALID;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev)
		goto end;

	if (policy_mgr_is_hwmode_offload_enabled(psoc)) {
		/*
		 * Stop any running opportunistic timer as it will be started
		 * after decision if required.
		 */
		policy_mgr_stop_opportunistic_timer(psoc);
		wlan_psoc_mlme_get_11be_capab(psoc, &eht_capab);
		if (eht_capab &&
		    QDF_IS_STATUS_SUCCESS(policy_mgr_get_sap_bw(psoc,
								&cur_bw)) &&
						cur_bw == CH_WIDTH_320MHZ &&
		    !mlo_mgr_is_link_switch_in_progress(vdev))
			check_sap_bw_downgrade = true;
		else
			goto end;
	}

	if (!scan_list || !qdf_list_size(scan_list)) {
		policy_mgr_debug("Scan list is NULL or No BSSIDs present");
		goto end;
	}

	if (!policy_mgr_is_hw_dbs_capable(psoc)) {
		policy_mgr_debug("Driver isn't DBS capable");
		goto end;
	}

	if (check_sap_bw_downgrade)
		goto ch_width_update;

	if (!policy_mgr_is_dbs_allowed_for_concurrency(psoc, QDF_STA_MODE)) {
		policy_mgr_debug("DBS not allowed for concurrency combo");
		goto end;
	}

	if (!policy_mgr_is_hw_dbs_2x2_capable(psoc) &&
	    !policy_mgr_is_hw_dbs_required_for_band(psoc,
						    HW_MODE_MAC_BAND_2G) &&
	    !policy_mgr_get_connection_count(psoc)) {
		policy_mgr_debug("1x1 DBS with no existing connection, HW mode change not required");
		goto end;
	}

ch_width_update:
	qdf_list_peek_front(scan_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(scan_list, cur_node, &next_node);

		scan_node = qdf_container_of(cur_node, struct scan_cache_node,
					     node);
		entry = scan_node->entry;
		ch_freq = entry->channel.chan_freq;

		if (policy_mgr_is_hw_mode_change_required(psoc, ch_freq,
							  vdev_id) ||
		    policy_mgr_is_ch_width_downgrade_required(psoc, vdev_id,
							      entry,
							      scan_list)) {
			policy_mgr_debug("Scan list has BSS of freq %d hw mode/SAP ch_width:%d update required",
					 ch_freq, cur_bw);
			break;
		}

		ch_freq = 0;
		cur_node = next_node;
		next_node = NULL;
	}

end:
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return ch_freq;
}

QDF_STATUS
policy_mgr_change_hw_mode_sta_connect(struct wlan_objmgr_psoc *psoc,
				      qdf_list_t *scan_list, uint8_t vdev_id,
				      uint32_t connect_id)
{
	QDF_STATUS status;
	uint32_t ch_freq;

	ch_freq = policy_mgr_check_for_hw_mode_change(psoc, scan_list, vdev_id);
	if (!ch_freq)
		return QDF_STATUS_E_ALREADY;

	status = policy_mgr_current_connections_update(psoc, vdev_id, ch_freq,
			POLICY_MGR_UPDATE_REASON_STA_CONNECT, connect_id);

	/*
	 * If status is success then the callback of policy mgr hw mode change
	 * would be called.
	 * If status is no support then the DUT is already in required HW mode.
	 */

	if (status == QDF_STATUS_E_FAILURE)
		policy_mgr_err("Hw mode change failed");
	else if (status == QDF_STATUS_E_NOSUPPORT)
		status = QDF_STATUS_E_ALREADY;

	return status;
}

QDF_STATUS
policy_mgr_current_connections_update(struct wlan_objmgr_psoc *psoc,
				      uint32_t session_id, uint32_t ch_freq,
				      enum policy_mgr_conn_update_reason
				      reason, uint32_t request_id)
{
	enum policy_mgr_conc_next_action next_action = PM_NOP;
	QDF_STATUS status;

	if (!policy_mgr_is_hw_dbs_capable(psoc)) {
		policy_mgr_rl_debug("driver isn't dbs capable, no further action needed");
		return QDF_STATUS_E_NOSUPPORT;
	}

	status = policy_mgr_get_next_action(psoc, session_id, ch_freq, reason,
					    &next_action);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (PM_NOP != next_action)
		status = policy_mgr_next_actions(psoc, session_id,
						next_action, reason,
						request_id);
	else
		status = QDF_STATUS_E_NOSUPPORT;

	policy_mgr_debug("next_action %d reason=%d session_id=%d request_id %x",
			 next_action, reason, session_id, request_id);

	return status;
}

/**
 * policy_mgr_dbs1_dbs2_need_action() - whether more actions are needed
 *                                      in DBS1 and DBS2 hw mode
 * @psoc: psoc object
 * @action: action type
 * @hw_mode: hardware mode
 *
 * The function checks further action are needed or not for DBS1 and DBS2.
 *
 * Return: true if more action are needed, otherwise
 *         return false
 */
static bool
policy_mgr_dbs1_dbs2_need_action(struct wlan_objmgr_psoc *psoc,
				 enum policy_mgr_conc_next_action action,
				 struct policy_mgr_hw_mode_params *hw_mode)
{
	if (policy_mgr_is_2x2_5G_1x1_2G_dbs_capable(psoc) ||
	    policy_mgr_is_2x2_2G_1x1_5G_dbs_capable(psoc)) {
		policy_mgr_debug("curr dbs action %d new action %d",
				 hw_mode->action_type, action);
		if (hw_mode->action_type == PM_DBS1 &&
		    ((action == PM_DBS1 ||
		    action == PM_DBS1_DOWNGRADE))) {
			policy_mgr_debug("driver is already in DBS_5G_2x2_24G_1x1 (%d), no further action %d needed",
					 hw_mode->action_type, action);
			return false;
		} else if (hw_mode->action_type == PM_DBS2 &&
			   ((action == PM_DBS2 ||
			   action == PM_DBS2_DOWNGRADE))) {
			policy_mgr_debug("driver is already in DBS_24G_2x2_5G_1x1 (%d), no further action %d needed",
					 hw_mode->action_type, action);
			return false;
		}
	}

	return true;
}

QDF_STATUS
policy_mgr_validate_dbs_switch(struct wlan_objmgr_psoc *psoc,
			       enum policy_mgr_conc_next_action action)
{
	QDF_STATUS status;
	struct policy_mgr_hw_mode_params hw_mode;

	/* check for the current HW index to see if really need any action */
	status = policy_mgr_get_current_hw_mode(psoc, &hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("policy_mgr_get_current_hw_mode failed");
		return status;
	}

	if ((action == PM_SBS) || (action == PM_SBS_DOWNGRADE)) {
		if (!policy_mgr_is_hw_sbs_capable(psoc)) {
			/* No action */
			policy_mgr_notice("firmware is not sbs capable");
			return QDF_STATUS_E_NOSUPPORT;
		}
		/* current mode is already SBS nothing to be
		 * done
		 */
		if (hw_mode.sbs_cap) {
			policy_mgr_notice("current mode is already SBS");
			return QDF_STATUS_E_ALREADY;
		}
		return QDF_STATUS_SUCCESS;
	}

	if (!hw_mode.dbs_cap) {
		if (action == PM_SINGLE_MAC ||
		    action == PM_SINGLE_MAC_UPGRADE) {
			policy_mgr_notice("current mode is already single MAC");
			return QDF_STATUS_E_ALREADY;
		} else {
			return QDF_STATUS_SUCCESS;
		}
	}
	/**
	 * If already in DBS, no need to request DBS again (HL, Napier).
	 * For dual DBS HW, in case DBS1 -> DBS2 or DBS2 -> DBS1
	 * switching, we need to check the current DBS mode is same as
	 * requested or not.
	 */
	if (policy_mgr_is_2x2_5G_1x1_2G_dbs_capable(psoc) ||
	    policy_mgr_is_2x2_2G_1x1_5G_dbs_capable(psoc)) {
		if (!policy_mgr_dbs1_dbs2_need_action(psoc, action, &hw_mode))
			return QDF_STATUS_E_ALREADY;
	} else if ((action == PM_DBS_DOWNGRADE) || (action == PM_DBS) ||
		   (action == PM_DBS_UPGRADE)) {
		policy_mgr_debug("driver is already in %s mode, no further action needed",
				 (hw_mode.dbs_cap) ? "dbs" : "non dbs");
		return QDF_STATUS_E_ALREADY;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_validate_unsupported_action() - unsupported action validation
 * @psoc: psoc object
 * @action: action type
 *
 * The help function checks the Action supported by HW or not.
 *
 * Return: QDF_STATUS_SUCCESS if supported by HW, otherwise
 *         return QDF_STATUS_E_NOSUPPORT
 */
static QDF_STATUS policy_mgr_validate_unsupported_action
		(struct wlan_objmgr_psoc *psoc,
		 enum policy_mgr_conc_next_action action)
{
	if (action == PM_SBS || action == PM_SBS_DOWNGRADE) {
		if (!policy_mgr_is_hw_sbs_capable(psoc)) {
			/* No action */
			policy_mgr_notice("firmware is not sbs capable");
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_next_actions(
		struct wlan_objmgr_psoc *psoc,
		uint32_t session_id,
		enum policy_mgr_conc_next_action action,
		enum policy_mgr_conn_update_reason reason,
		uint32_t request_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct dbs_nss nss_dbs = {0};
	struct dbs_bw bw_dbs = {0};
	struct policy_mgr_hw_mode_params hw_mode;
	enum policy_mgr_conc_next_action next_action;
	bool is_sbs_supported;
	enum hw_mode_sbs_capab sbs_capab;

	if (policy_mgr_is_hw_dbs_capable(psoc) == false) {
		policy_mgr_rl_debug("driver isn't dbs capable, no further action needed");
		return QDF_STATUS_E_NOSUPPORT;
	}
	status = policy_mgr_validate_unsupported_action(psoc, action);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;
	/* check for the current HW index to see if really need any action */
	status = policy_mgr_get_current_hw_mode(psoc, &hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("policy_mgr_get_current_hw_mode failed");
		return status;
	}

	switch (action) {
	case PM_DBS_DOWNGRADE:
		/*
		* check if we have a beaconing entity that is using 2x2. If yes,
		* update the beacon template & notify FW. Once FW confirms
		*  beacon updated, send down the HW mode change req
		*/
		status = policy_mgr_complete_action(psoc, POLICY_MGR_RX_NSS_1,
					PM_DBS, reason, session_id, request_id);
		break;
	case PM_DBS:
		(void)policy_mgr_get_hw_dbs_nss(psoc, &nss_dbs);
		policy_mgr_get_hw_dbs_max_bw(psoc, &bw_dbs);
		is_sbs_supported = policy_mgr_is_hw_sbs_capable(psoc);
		sbs_capab = is_sbs_supported ? HW_MODE_SBS : HW_MODE_SBS_NONE;
		status = policy_mgr_pdev_set_hw_mode(psoc, session_id,
						     nss_dbs.mac0_ss,
						     bw_dbs.mac0_bw,
						     nss_dbs.mac1_ss,
						     bw_dbs.mac1_bw,
						     HW_MODE_MAC_BAND_NONE,
						     HW_MODE_DBS,
						     HW_MODE_AGILE_DFS_NONE,
						     sbs_capab,
						     reason, PM_NOP, PM_DBS,
						     request_id);
		break;
	case PM_SINGLE_MAC_UPGRADE:
		/*
		 * change the HW mode first before the NSS upgrade
		 */
		status = policy_mgr_pdev_set_hw_mode(psoc, session_id,
						HW_MODE_SS_2x2,
						HW_MODE_80_MHZ,
						HW_MODE_SS_0x0, HW_MODE_BW_NONE,
						HW_MODE_MAC_BAND_NONE,
						HW_MODE_DBS_NONE,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason, PM_UPGRADE,
						PM_SINGLE_MAC_UPGRADE,
						request_id);
		break;
	case PM_SINGLE_MAC:
		status = policy_mgr_pdev_set_hw_mode(psoc, session_id,
						HW_MODE_SS_2x2,
						HW_MODE_80_MHZ,
						HW_MODE_SS_0x0, HW_MODE_BW_NONE,
						HW_MODE_MAC_BAND_NONE,
						HW_MODE_DBS_NONE,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason, PM_NOP, PM_SINGLE_MAC,
						request_id);
		break;
	case PM_DBS_UPGRADE:
		status = policy_mgr_pdev_set_hw_mode(psoc, session_id,
						HW_MODE_SS_2x2,
						HW_MODE_80_MHZ,
						HW_MODE_SS_2x2, HW_MODE_80_MHZ,
						HW_MODE_MAC_BAND_NONE,
						HW_MODE_DBS,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason, PM_UPGRADE,
						PM_DBS_UPGRADE, request_id);
		break;
	case PM_SBS_DOWNGRADE:
		status = policy_mgr_complete_action(psoc, POLICY_MGR_RX_NSS_1,
					PM_SBS, reason, session_id, request_id);
		break;
	case PM_SBS:
		status = policy_mgr_pdev_set_hw_mode(psoc, session_id,
						HW_MODE_SS_1x1,
						HW_MODE_80_MHZ,
						HW_MODE_SS_1x1, HW_MODE_80_MHZ,
						HW_MODE_MAC_BAND_NONE,
						HW_MODE_DBS,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS,
						reason, PM_NOP, PM_SBS,
						request_id);
		break;
	case PM_DOWNGRADE:
		/*
		 * check if we have a beaconing entity that advertised 2x2
		 * initially. If yes, update the beacon template & notify FW.
		 */
		status = policy_mgr_nss_update(psoc, POLICY_MGR_RX_NSS_1,
					PM_NOP, POLICY_MGR_ANY, reason,
					session_id, request_id);
		break;
	case PM_UPGRADE:
		/*
		 * check if we have a beaconing entity that advertised 2x2
		 * initially. If yes, update the beacon template & notify FW.
		 */
		status = policy_mgr_nss_update(psoc, POLICY_MGR_RX_NSS_2,
					PM_NOP, POLICY_MGR_ANY, reason,
					session_id, request_id);
		break;
	case PM_DBS1_DOWNGRADE:
		if (policy_mgr_dbs1_dbs2_need_action(psoc, action, &hw_mode))
			status = policy_mgr_complete_action(psoc,
							    POLICY_MGR_RX_NSS_1,
							    PM_DBS1, reason,
							    session_id,
							    request_id);
		else
			status = QDF_STATUS_E_ALREADY;
		break;
	case PM_DBS2_DOWNGRADE:
		if (policy_mgr_dbs1_dbs2_need_action(psoc, action, &hw_mode))
			status = policy_mgr_complete_action(psoc,
							    POLICY_MGR_RX_NSS_1,
							    PM_DBS2, reason,
							    session_id,
							    request_id);
		else
			status = QDF_STATUS_E_ALREADY;
		break;
	case PM_DBS1:
		/*
		 * PM_DBS1 (2x2 5G + 1x1 2G) will support 5G 2x2. If previous
		 * mode is DBS, that should be 2x2 2G + 1x1 5G mode and
		 * the 5G band was downgraded to 1x1. So, we need to
		 * upgrade 5G vdevs after hw mode change.
		 */
		if (policy_mgr_dbs1_dbs2_need_action(psoc, action, &hw_mode)) {
			if (hw_mode.dbs_cap)
				next_action = PM_UPGRADE_5G;
			else
				next_action = PM_NOP;
			status = policy_mgr_pdev_set_hw_mode(
					psoc, session_id,
					HW_MODE_SS_2x2,
					HW_MODE_80_MHZ,
					HW_MODE_SS_1x1, HW_MODE_40_MHZ,
					HW_MODE_MAC_BAND_5G,
					HW_MODE_DBS,
					HW_MODE_AGILE_DFS_NONE,
					HW_MODE_SBS_NONE,
					reason, next_action, PM_DBS1,
					request_id);
		} else {
			status = QDF_STATUS_E_ALREADY;
		}
		break;
	case PM_DBS2:
		/*
		 * PM_DBS2 (2x2 2G + 1x1 5G) will support 2G 2x2. If previous
		 * mode is DBS, that should be 2x2 5G + 1x1 2G mode and
		 * the 2G band was downgraded to 1x1. So, we need to
		 * upgrade 5G vdevs after hw mode change.
		 */
		if (policy_mgr_dbs1_dbs2_need_action(psoc, action, &hw_mode)) {
			if (hw_mode.dbs_cap)
				next_action = PM_UPGRADE_2G;
			else
				next_action = PM_NOP;
			status = policy_mgr_pdev_set_hw_mode(
						psoc, session_id,
						HW_MODE_SS_2x2,
						HW_MODE_40_MHZ,
						HW_MODE_SS_1x1, HW_MODE_40_MHZ,
						HW_MODE_MAC_BAND_2G,
						HW_MODE_DBS,
						HW_MODE_AGILE_DFS_NONE,
						HW_MODE_SBS_NONE,
						reason, next_action, PM_DBS2,
						request_id);
		} else {
			status = QDF_STATUS_E_ALREADY;
		}
		break;
	case PM_UPGRADE_5G:
		status = policy_mgr_nss_update(
					psoc, POLICY_MGR_RX_NSS_2,
					PM_NOP, POLICY_MGR_BAND_5, reason,
					session_id, request_id);
		break;
	case PM_UPGRADE_2G:
		status = policy_mgr_nss_update(
					psoc, POLICY_MGR_RX_NSS_2,
					PM_NOP, POLICY_MGR_BAND_24, reason,
					session_id, request_id);
		break;
	case PM_DOWNGRADE_BW:
	case PM_UPGRADE_BW:
		policy_mgr_sap_ch_width_update(psoc, action, reason,
					       session_id, request_id);
		break;
	default:
		policy_mgr_err("unexpected action value %d", action);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	return status;
}

QDF_STATUS
policy_mgr_handle_conc_multiport(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, uint32_t ch_freq,
				 enum policy_mgr_conn_update_reason reason,
				 uint32_t request_id)
{
	QDF_STATUS status;
	uint8_t num_cxn_del = 0;
	struct policy_mgr_conc_connection_info info = {0};

	policy_mgr_store_and_del_conn_info_by_vdev_id(psoc, vdev_id,
						      &info, &num_cxn_del);

	if (!policy_mgr_check_for_session_conc(psoc, vdev_id, ch_freq)) {
		policy_mgr_err("Conc not allowed for the vdev %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	status = policy_mgr_reset_connection_update(psoc);
	if (!QDF_IS_STATUS_SUCCESS(status))
		policy_mgr_err("clearing event failed");

	status = policy_mgr_current_connections_update(psoc, vdev_id,
						       ch_freq, reason,
						       request_id);
	if (QDF_STATUS_E_FAILURE == status)
		policy_mgr_err("connections update failed");

	if (num_cxn_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, &info,
						     num_cxn_del);

	return status;
}

enum policy_mgr_con_mode
policy_mgr_con_mode_by_vdev_id(struct wlan_objmgr_psoc *psoc,
			       uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum policy_mgr_con_mode mode = PM_MAX_NUM_OF_MODE;
	enum QDF_OPMODE op_mode;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return mode;
	}

	op_mode = wlan_get_opmode_from_vdev_id(pm_ctx->pdev, vdev_id);
	return policy_mgr_qdf_opmode_to_pm_con_mode(psoc, op_mode, vdev_id);
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
qdf_freq_t
policy_mgr_get_user_config_sap_freq(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	qdf_freq_t freq;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev is NULL");
		return 0;
	}
	freq = wlan_get_sap_user_config_freq(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return freq;
}

/**
 * policy_mgr_is_sap_go_existed() - Check if restart SAP/Go exist
 * @psoc: PSOC object data
 *
 * To simplify, if SAP/P2P Go exist, they may need switch channel for
 * forcing scc with sta or band capability change.
 * Restart: true or false
 */
static bool policy_mgr_is_sap_go_existed(struct wlan_objmgr_psoc *psoc)
{
	uint32_t ap_present, go_present;

	ap_present = policy_mgr_get_sap_mode_count(psoc, NULL);
	if (ap_present)
		return true;

	go_present = policy_mgr_mode_specific_connection_count(
				psoc, PM_P2P_GO_MODE, NULL);
	if (go_present)
		return true;

	return false;
}

#ifdef FEATURE_WLAN_CH_AVOID_EXT
bool policy_mgr_is_safe_channel(struct wlan_objmgr_psoc *psoc,
				uint32_t ch_freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool is_safe = true;
	uint8_t j;
	unsigned long restriction_mask;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return is_safe;
	}

	if (pm_ctx->unsafe_channel_count == 0)
		return is_safe;

	restriction_mask =
		(unsigned long)policy_mgr_get_freq_restriction_mask(pm_ctx);
	for (j = 0; j < pm_ctx->unsafe_channel_count; j++) {
		if ((ch_freq == pm_ctx->unsafe_channel_list[j]) &&
		    (qdf_test_bit(QDF_SAP_MODE, &restriction_mask) ||
		     !wlan_mlme_get_coex_unsafe_chan_nb_user_prefer_for_sap(
								     psoc))) {
			is_safe = false;
			policy_mgr_warn("Freq %d is not safe, restriction mask %lu", ch_freq, restriction_mask);
			break;
		}
	}

	return is_safe;
}

bool policy_mgr_restrict_sap_on_unsafe_chan(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	unsigned long restriction_mask;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return false;
	}

	restriction_mask =
		(unsigned long)policy_mgr_get_freq_restriction_mask(pm_ctx);
	return qdf_test_bit(QDF_SAP_MODE, &restriction_mask);
}
#else
bool policy_mgr_is_safe_channel(struct wlan_objmgr_psoc *psoc,
				uint32_t ch_freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool is_safe = true;
	uint8_t j;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return is_safe;
	}

	if (pm_ctx->unsafe_channel_count == 0)
		return is_safe;

	for (j = 0; j < pm_ctx->unsafe_channel_count; j++) {
		if (ch_freq == pm_ctx->unsafe_channel_list[j]) {
			is_safe = false;
			policy_mgr_warn("Freq %d is not safe", ch_freq);
			break;
		}
	}

	return is_safe;
}
#endif

bool policy_mgr_is_sap_freq_allowed(struct wlan_objmgr_psoc *psoc,
				    enum QDF_OPMODE opmode,
				    uint32_t sap_freq)
{
	uint32_t nan_2g_freq, nan_5g_freq;

	/*
	 * Ignore safe channel validation when the mode is P2P_GO and user
	 * configures the corresponding bit in ini coex_unsafe_chan_nb_user_prefer.
	 */
	if ((opmode == QDF_P2P_GO_MODE &&
	     wlan_mlme_get_coex_unsafe_chan_nb_user_prefer_for_p2p_go(psoc)) ||
	    policy_mgr_is_safe_channel(psoc, sap_freq))
		return true;

	/*
	 * Return true if it's STA+SAP SCC and
	 * STA+SAP SCC on LTE coex channel is allowed.
	 */
	if (policy_mgr_sta_sap_scc_on_lte_coex_chan(psoc) &&
	    policy_mgr_is_sta_sap_scc(psoc, sap_freq)) {
		policy_mgr_debug("unsafe freq %d for sap is allowed", sap_freq);
		return true;
	}

	nan_2g_freq =
		policy_mgr_mode_specific_get_channel(psoc, PM_NAN_DISC_MODE);
	nan_5g_freq = wlan_nan_get_disc_5g_ch_freq(psoc);

	if ((WLAN_REG_IS_SAME_BAND_FREQS(nan_2g_freq, sap_freq) ||
	     WLAN_REG_IS_SAME_BAND_FREQS(nan_5g_freq, sap_freq)) &&
	    policy_mgr_is_force_scc(psoc) &&
	    policy_mgr_get_nan_sap_scc_on_lte_coex_chnl(psoc)) {
		policy_mgr_debug("NAN+SAP SCC on unsafe freq %d is allowed",
				  sap_freq);
		return true;
	}

	return false;
}

bool policy_mgr_is_sap_restart_required_after_sta_disconnect(
			struct wlan_objmgr_psoc *psoc,
			uint32_t sap_vdev_id, uint32_t *intf_ch_freq,
			bool is_acs_mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t curr_sap_freq = 0, new_sap_freq = 0;
	bool sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);
	bool sta_sap_scc_on_lte_coex_chan =
		policy_mgr_sta_sap_scc_on_lte_coex_chan(psoc);
	uint8_t sta_sap_scc_on_dfs_chnl_config_value = 0;
	uint32_t cc_count, i, go_index_start, pcl_len = 0;
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS * 2];
	uint8_t vdev_id[MAX_NUMBER_OF_CONC_CONNECTIONS * 2];
	enum policy_mgr_con_mode mode;
	uint32_t pcl_channels[NUM_CHANNELS + 1];
	uint8_t pcl_weight[NUM_CHANNELS + 1];
	struct policy_mgr_conc_connection_info info = {0};
	uint8_t num_cxn_del = 0;
	QDF_STATUS status;
	uint32_t sta_gc_present = 0;
	qdf_freq_t user_config_freq = 0;
	enum reg_wifi_band user_band, op_band;

	if (intf_ch_freq)
		*intf_ch_freq = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return false;
	}

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc, &sta_sap_scc_on_dfs_chnl_config_value);

	if (!policy_mgr_is_hw_dbs_capable(psoc))
		if (policy_mgr_get_connection_count(psoc) > 1)
			return false;

	cc_count = policy_mgr_get_mode_specific_conn_info(psoc,
							  &op_ch_freq_list[0],
							  &vdev_id[0],
							  PM_SAP_MODE);
	go_index_start = cc_count;
	if (cc_count < MAX_NUMBER_OF_CONC_CONNECTIONS)
		cc_count += policy_mgr_get_mode_specific_conn_info(
					psoc, &op_ch_freq_list[cc_count],
					&vdev_id[cc_count], PM_P2P_GO_MODE);

	sta_gc_present =
		policy_mgr_mode_specific_connection_count(psoc,
							  PM_STA_MODE, NULL) +
		policy_mgr_mode_specific_connection_count(psoc,
							  PM_P2P_CLIENT_MODE,
							  NULL);

	for (i = 0 ; i < cc_count; i++) {
		if (sap_vdev_id != INVALID_VDEV_ID &&
		    sap_vdev_id != vdev_id[i])
			continue;

		sap_vdev_id = vdev_id[i];
		user_config_freq =
			policy_mgr_get_user_config_sap_freq(psoc, sap_vdev_id);

		if (policy_mgr_is_any_mode_active_on_band_along_with_session(
				psoc,  vdev_id[i],
				WLAN_REG_IS_24GHZ_CH_FREQ(op_ch_freq_list[i]) ?
				POLICY_MGR_BAND_24 : POLICY_MGR_BAND_5))
			continue;

		if (sta_sap_scc_on_dfs_chan &&
		    (sta_sap_scc_on_dfs_chnl_config_value != 2) &&
		     wlan_reg_is_dfs_for_freq(pm_ctx->pdev,
					      op_ch_freq_list[i]) &&
		     pm_ctx->last_disconn_sta_freq == op_ch_freq_list[i]) {
			curr_sap_freq = op_ch_freq_list[i];
			policy_mgr_debug("sta_sap_scc_on_dfs_chan %u, sta_sap_scc_on_dfs_chnl_config_value %u, dfs sap_ch_freq %u",
					 sta_sap_scc_on_dfs_chan,
					 sta_sap_scc_on_dfs_chnl_config_value,
					 curr_sap_freq);
			break;
		}

		if ((is_acs_mode ||
		     policy_mgr_restrict_sap_on_unsafe_chan(psoc)) &&
		    sta_sap_scc_on_lte_coex_chan &&
		    !policy_mgr_is_safe_channel(psoc, op_ch_freq_list[i]) &&
		    pm_ctx->last_disconn_sta_freq == op_ch_freq_list[i]) {
			curr_sap_freq = op_ch_freq_list[i];
			policy_mgr_debug("sta_sap_scc_on_lte_coex_chan %u unsafe sap_ch_freq %u",
					 sta_sap_scc_on_lte_coex_chan,
					 curr_sap_freq);
			break;
		}
		/* When STA+SAP SCC is allowed on indoor channel,
		 * Restart the SAP when :
		 * 1. The user configured SAP frequency is not
		 * the same as current freq. (or)
		 * 2. The frequency is not allowed in the indoor
		 * channel.
		 */
		if (pm_ctx->last_disconn_sta_freq == op_ch_freq_list[i] &&
		    !policy_mgr_is_sap_go_interface_allowed_on_indoor(
							pm_ctx->pdev,
							sap_vdev_id,
							op_ch_freq_list[i])) {
			curr_sap_freq = op_ch_freq_list[i];
			policy_mgr_debug("indoor sap_ch_freq %u",
					 curr_sap_freq);
			break;
		}

		/*
		 * STA got disconnected & SAP has previously moved to 2.4 GHz
		 * due to concurrency, then move SAP back to user configured
		 * frequency if the user configured band is better than
		 * the current operating band.
		 */
		op_band = wlan_reg_freq_to_band(op_ch_freq_list[i]);
		user_band = wlan_reg_freq_to_band(user_config_freq);

		if (!sta_gc_present && user_config_freq &&
		    op_band < user_band) {
			curr_sap_freq = op_ch_freq_list[i];
			policy_mgr_debug("Move sap to user configured freq: %d",
					 user_config_freq);
			break;
		}
	}

	if (!curr_sap_freq) {
		policy_mgr_debug("SAP restart is not required");
		return false;
	}

	mode = i >= go_index_start ? PM_P2P_GO_MODE : PM_SAP_MODE;
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_store_and_del_conn_info_by_vdev_id(psoc, sap_vdev_id,
						      &info, &num_cxn_del);

	/* Add the user config ch as first condidate */
	pcl_channels[0] = user_config_freq;
	pcl_weight[0] = 0;
	status = policy_mgr_get_pcl(psoc, mode, &pcl_channels[1], &pcl_len,
				    &pcl_weight[1],
				    QDF_ARRAY_SIZE(pcl_weight) - 1,
				    sap_vdev_id);
	if (status == QDF_STATUS_SUCCESS)
		pcl_len++;
	else
		pcl_len = 1;


	for (i = 0; i < pcl_len; i++) {
		if (pcl_channels[i] == curr_sap_freq)
			continue;

		if (!policy_mgr_is_safe_channel(psoc, pcl_channels[i]) ||
		    wlan_reg_is_dfs_for_freq(pm_ctx->pdev, pcl_channels[i]))
			continue;

		/* SAP moved to 2.4 GHz, due to STA on DFS or Indoor where
		 * concurrency is not allowed, now that there is no
		 * STA/GC in 5 GHz band, move 2.4 GHz SAP to 5 GHz band if SAP
		 * was initially started on 5 GHz band.
		 * Checking again here as pcl_channels[0] could be
		 * on indoor which is not removed in policy_mgr_get_pcl
		 */
		if (!sta_gc_present &&
		    !policy_mgr_is_sap_go_interface_allowed_on_indoor(
							pm_ctx->pdev,
							sap_vdev_id,
							pcl_channels[i])) {
			policy_mgr_debug("Do not allow SAP on indoor frequency, STA is absent");
			continue;
		}

		new_sap_freq = pcl_channels[i];
		break;
	}

	/* Restore the connection entry */
	if (num_cxn_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, &info, num_cxn_del);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (new_sap_freq == 0 || curr_sap_freq == new_sap_freq)
		return false;
	if (!intf_ch_freq)
		return true;

	*intf_ch_freq = new_sap_freq;
	policy_mgr_debug("Standalone SAP(vdev_id %d) will be moved to channel %u",
			 sap_vdev_id, *intf_ch_freq);

	return true;
}

/**
 * policy_mgr_is_nan_sap_unsafe_ch_scc_allowed() - Check if NAN+SAP SCC is
 *                                               allowed in LTE COEX unsafe ch
 * @pm_ctx: policy_mgr_psoc_priv_obj policy mgr context
 * @ch_freq: Channel frequency to check
 *
 * Return: True if allowed else false
 */
static bool
policy_mgr_is_nan_sap_unsafe_ch_scc_allowed(struct policy_mgr_psoc_priv_obj
					    *pm_ctx, uint32_t ch_freq)
{
	if (policy_mgr_is_safe_channel(pm_ctx->psoc, ch_freq) ||
	    pm_ctx->cfg.nan_sap_scc_on_lte_coex_chnl)
		return true;

	return false;
}

/**
 * policy_mgr_nan_disable_work() - qdf defer function wrapper for NAN disable
 * @data: qdf_work data
 *
 * Return: None
 */
static void policy_mgr_nan_disable_work(void *data)
{
	struct wlan_objmgr_psoc *psoc = data;

	ucfg_nan_disable_concurrency(psoc);
}

bool policy_mgr_nan_sap_scc_on_unsafe_ch_chk(struct wlan_objmgr_psoc *psoc,
					     uint32_t sap_freq)
{
	uint32_t nan_2g_freq, nan_5g_freq;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	nan_2g_freq = policy_mgr_mode_specific_get_channel(psoc,
							   PM_NAN_DISC_MODE);
	if (nan_2g_freq == 0) {
		policy_mgr_debug("No NAN+SAP SCC");
		return false;
	}
	nan_5g_freq = wlan_nan_get_disc_5g_ch_freq(psoc);

	policy_mgr_debug("Freq SAP: %d NAN: %d %d", sap_freq,
			 nan_2g_freq, nan_5g_freq);
	if (WLAN_REG_IS_SAME_BAND_FREQS(nan_2g_freq, sap_freq)) {
		if (policy_mgr_is_force_scc(pm_ctx->psoc) &&
		    policy_mgr_is_nan_sap_unsafe_ch_scc_allowed(pm_ctx,
								nan_2g_freq))
			return true;
	} else if (WLAN_REG_IS_SAME_BAND_FREQS(nan_5g_freq, sap_freq)) {
		if (policy_mgr_is_force_scc(pm_ctx->psoc) &&
		    policy_mgr_is_nan_sap_unsafe_ch_scc_allowed(pm_ctx,
								nan_5g_freq))
			return true;
	} else {
		/*
		 * NAN + SAP in different bands. Continue to check for
		 * SAP in unsafe channel
		 */
		return false;
	}
	policy_mgr_info("NAN+SAP unsafe ch SCC not allowed. Disabling NAN");
	/* change context to worker since this is executed in sched thread ctx*/
	qdf_create_work(0, &pm_ctx->nan_sap_conc_work,
			policy_mgr_nan_disable_work, psoc);
	qdf_sched_work(0, &pm_ctx->nan_sap_conc_work);

	return false;
}

bool
policy_mgr_nan_sap_pre_enable_conc_check(struct wlan_objmgr_psoc *psoc,
					 enum policy_mgr_con_mode mode,
					 uint32_t ch_freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t sap_freq, nan_2g_freq, nan_5g_freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (!policy_mgr_is_sap_mode(mode) || mode == PM_NAN_DISC_MODE) {
		policy_mgr_debug("Not NAN or SAP mode");
		return true;
	}

	if (!ch_freq) {
		policy_mgr_err("Invalid channel");
		return false;
	}

	if (!wlan_nan_get_sap_conc_support(pm_ctx->psoc)) {
		policy_mgr_debug("NAN+SAP not supported in fw");
		/* Reject NAN as SAP is of high priority */
		if (mode == PM_NAN_DISC_MODE)
			return false;
		/* Before SAP start disable NAN */
		ucfg_nan_disable_concurrency(pm_ctx->psoc);
		return true;
	}

	if (mode == PM_NAN_DISC_MODE) {
		sap_freq = policy_mgr_mode_specific_get_channel(pm_ctx->psoc,
								PM_SAP_MODE);
		policy_mgr_debug("FREQ SAP: %d NAN: %d", sap_freq, ch_freq);
		if (!sap_freq) {
			sap_freq = policy_mgr_mode_specific_get_channel(
							pm_ctx->psoc,
							PM_LL_LT_SAP_MODE);
			policy_mgr_debug("FREQ LL_LT_SAP: %d NAN: %d",
					 sap_freq, ch_freq);
		}
		if (ucfg_is_nan_dbs_supported(pm_ctx->psoc) &&
		    !WLAN_REG_IS_SAME_BAND_FREQS(sap_freq, ch_freq))
			return true;

		if (sap_freq == ch_freq) {
			policy_mgr_debug("NAN+SAP SCC");
			return true;
		}

		if (!policy_mgr_is_force_scc(pm_ctx->psoc)) {
			policy_mgr_debug("SAP force SCC disabled");
			return false;
		}
		if (!policy_mgr_is_nan_sap_unsafe_ch_scc_allowed(
						pm_ctx, ch_freq)) {
			policy_mgr_debug("NAN+SAP unsafe ch SCC disabled");
			return false;
		}
		if (pm_ctx->hdd_cbacks.hdd_is_cac_in_progress &&
		    pm_ctx->hdd_cbacks.hdd_is_cac_in_progress()) {
			policy_mgr_debug("DFS CAC in progress, reject NAN enable");
			return false;
		}
	} else if (policy_mgr_is_sap_mode(mode)) {
		nan_2g_freq =
			policy_mgr_mode_specific_get_channel(pm_ctx->psoc,
							     PM_NAN_DISC_MODE);
		nan_5g_freq = wlan_nan_get_disc_5g_ch_freq(pm_ctx->psoc);
		policy_mgr_debug("SAP CH: %d NAN Ch: %d %d", ch_freq,
				 nan_2g_freq, nan_5g_freq);
		if (ucfg_is_nan_conc_control_supported(pm_ctx->psoc) &&
		    !ucfg_is_nan_dbs_supported(pm_ctx->psoc) &&
		    !WLAN_REG_IS_SAME_BAND_FREQS(nan_2g_freq, ch_freq)) {
			if (!policy_mgr_is_force_scc(pm_ctx->psoc)) {
				policy_mgr_debug("NAN and SAP are in different bands but SAP force SCC disabled");
				ucfg_nan_disable_concurrency(pm_ctx->psoc);
				return true;
			}
		} else if (WLAN_REG_IS_SAME_BAND_FREQS(nan_2g_freq, ch_freq) ||
			   WLAN_REG_IS_SAME_BAND_FREQS(nan_5g_freq, ch_freq)) {
			if (ch_freq == nan_2g_freq || ch_freq == nan_5g_freq) {
				policy_mgr_debug("NAN+SAP SCC");
				return true;
			}
			if (!policy_mgr_is_force_scc(pm_ctx->psoc)) {
				policy_mgr_debug("SAP force SCC disabled");
				ucfg_nan_disable_concurrency(pm_ctx->psoc);
				return true;
			}
			if ((WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) &&
			     !policy_mgr_is_nan_sap_unsafe_ch_scc_allowed(
			     pm_ctx, nan_5g_freq)) ||
			    (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq) &&
			     !policy_mgr_is_nan_sap_unsafe_ch_scc_allowed(
			     pm_ctx, nan_2g_freq))) {
				policy_mgr_debug("NAN+SAP unsafe ch SCC disabled");
				ucfg_nan_disable_concurrency(pm_ctx->psoc);
				return true;
			}
		}
	}
	return true;
}

QDF_STATUS
policy_mgr_nan_sap_post_enable_conc_check(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info *sap_info = NULL;
	uint8_t i;
	qdf_freq_t nan_freq_2g, nan_freq_5g;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (policy_mgr_is_sap_mode(pm_conc_connection_list[i].mode) &&
		    pm_conc_connection_list[i].in_use) {
			sap_info = &pm_conc_connection_list[i];
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (!sap_info)
		goto end;
	if (sap_info->freq == 0)
		goto end;
	nan_freq_2g = policy_mgr_mode_specific_get_channel(psoc,
							   PM_NAN_DISC_MODE);
	nan_freq_5g = wlan_nan_get_disc_5g_ch_freq(psoc);
	if (sap_info->freq == nan_freq_2g || sap_info->freq == nan_freq_5g) {
		policy_mgr_debug("NAN and SAP already in SCC");
		goto end;
	}
	if (nan_freq_2g == 0)
		goto end;

	if (pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress()) {
		policy_mgr_debug("channel switch is already in progress");
		return status;
	}

	if (pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason)
		pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason(psoc,
					sap_info->vdev_id,
					CSA_REASON_CONCURRENT_NAN_EVENT);

	/* SAP should be moved to 2g NAN channel on non-DBS platforms */
	if (!ucfg_is_nan_dbs_supported(pm_ctx->psoc) ||
	    WLAN_REG_IS_24GHZ_CH_FREQ(sap_info->freq)) {
		policy_mgr_debug("Force SCC for NAN+SAP Ch freq: %d",
				 nan_freq_2g);
		status =
		policy_mgr_change_sap_channel_with_csa(psoc, sap_info->vdev_id,
						       nan_freq_2g,
						       policy_mgr_get_ch_width(
						       sap_info->bw),
						       true);
		if (status == QDF_STATUS_SUCCESS)
			status = QDF_STATUS_E_PENDING;
	} else if (nan_freq_5g && WLAN_REG_IS_5GHZ_CH_FREQ(sap_info->freq)) {
		policy_mgr_debug("Force SCC for NAN+SAP Ch freq: %d",
				 nan_freq_5g);
		status =
		policy_mgr_change_sap_channel_with_csa(psoc, sap_info->vdev_id,
						       nan_freq_5g,
						       policy_mgr_get_ch_width(
						       sap_info->bw),
						       true);
		if (status == QDF_STATUS_SUCCESS)
			status = QDF_STATUS_E_PENDING;
	}

end:
	pm_ctx->sta_ap_intf_check_work_info->nan_force_scc_in_progress = false;

	return status;
}

void policy_mgr_nan_sap_post_disable_conc_check(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info *sap_info = NULL;
	uint32_t sap_freq = 0, i;
	QDF_STATUS status;
	uint32_t user_config_freq;
	uint8_t band_mask = 0;
	uint8_t chn_idx, num_chan;
	struct regulatory_channel *channel_list;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return;
	}

	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].mode == PM_SAP_MODE &&
		    pm_conc_connection_list[i].in_use) {
			sap_info = &pm_conc_connection_list[i];
			sap_freq = sap_info->freq;
			break;
		}
	}
	if (sap_freq == 0 || policy_mgr_is_safe_channel(psoc, sap_freq))
		return;

	user_config_freq = policy_mgr_get_user_config_sap_freq(
						psoc, sap_info->vdev_id);

	sap_freq = policy_mgr_get_nondfs_preferred_channel(psoc, PM_SAP_MODE,
							   false,
							   sap_info->vdev_id);
	policy_mgr_debug("User/ACS orig Freq: %d New SAP Freq: %d",
			 user_config_freq, sap_freq);

	if (wlan_reg_is_enable_in_secondary_list_for_freq(pm_ctx->pdev,
							  user_config_freq) &&
	    policy_mgr_is_safe_channel(psoc, user_config_freq)) {
		policy_mgr_debug("Give preference to user config freq");
		sap_freq = user_config_freq;
	} else {
		channel_list = qdf_mem_malloc(
					sizeof(struct regulatory_channel) *
					       NUM_CHANNELS);
		if (!channel_list)
			return;

		band_mask |= BIT(wlan_reg_freq_to_band(user_config_freq));
		num_chan = wlan_reg_get_band_channel_list_for_pwrmode(
						pm_ctx->pdev,
						band_mask,
						channel_list,
						REG_CURRENT_PWR_MODE);
		for (chn_idx = 0; chn_idx < num_chan; chn_idx++) {
			if (wlan_reg_is_enable_in_secondary_list_for_freq(
					pm_ctx->pdev,
					channel_list[chn_idx].center_freq) &&
			    policy_mgr_is_safe_channel(
					psoc,
					channel_list[chn_idx].center_freq)) {
				policy_mgr_debug("Prefer user config band freq %d",
						 channel_list[chn_idx].center_freq);
				sap_freq = channel_list[chn_idx].center_freq;
			}
		}
		qdf_mem_free(channel_list);
	}

	if (pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress()) {
		policy_mgr_debug("wait as channel switch is already in progress");
		status = qdf_wait_single_event(
					&pm_ctx->channel_switch_complete_evt,
					CHANNEL_SWITCH_COMPLETE_TIMEOUT);
		if (QDF_IS_STATUS_ERROR(status))
			policy_mgr_err("wait for event failed, still continue with channel switch");
	}

	if (pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason)
		pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason(
				psoc, sap_info->vdev_id,
				CSA_REASON_CONCURRENT_NAN_EVENT);

	policy_mgr_change_sap_channel_with_csa(psoc, sap_info->vdev_id,
					       sap_freq,
					       policy_mgr_get_ch_width(
					       sap_info->bw), true);
}

void policy_mgr_check_sap_restart(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id)
{
	QDF_STATUS status;
	uint32_t ch_freq;
	struct policy_mgr_psoc_priv_obj *pm_ctx = NULL;

	if (!psoc) {
		policy_mgr_err("Invalid psoc");
		return;
	}
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}

	if (pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress()) {
		policy_mgr_debug("channel switch is already in progress");
		return;
	}

	/*
	 * Restart should be handled by sap_fsm_validate_and_change_channel(),
	 * after SAP starts.
	 */
	if (pm_ctx->hdd_cbacks.hdd_is_cac_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_cac_in_progress()) {
		policy_mgr_debug("DFS CAC in progress, do not restart SAP");
		return;
	}

	if (!pm_ctx->hdd_cbacks.wlan_hdd_get_channel_for_sap_restart) {
		policy_mgr_err("SAP restart get channel callback in NULL");
		goto end;
	}
	status =
		pm_ctx->hdd_cbacks.wlan_hdd_get_channel_for_sap_restart(psoc,
								      vdev_id,
								      &ch_freq);
	if (status == QDF_STATUS_SUCCESS)
		policy_mgr_debug("SAP vdev id %d switch to new ch freq: %d",
				 vdev_id, ch_freq);

end:
	pm_ctx->last_disconn_sta_freq = 0;
}

/**
 * policy_mgr_handle_sap_plus_go_force_scc() - Do SAP/GO force SCC
 * @psoc: soc object
 *
 * This function will check SAP/GO channel state and select channel
 * to avoid MCC, then do channel change on the second interface.
 *
 * Return: QDF_STATUS_SUCCESS if successfully handle the SAP/GO
 * force SCC.
 */
static QDF_STATUS
policy_mgr_handle_sap_plus_go_force_scc(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint8_t existing_vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	enum policy_mgr_con_mode existing_vdev_mode = PM_MAX_NUM_OF_MODE;
	enum policy_mgr_con_mode vdev_con_mode;
	uint32_t existing_ch_freq, chan_freq, intf_ch_freq;
	enum phy_ch_width existing_ch_width;
	uint8_t vdev_id;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct sta_ap_intf_check_work_ctx *work_info;
	struct ch_params ch_params = {0};
	enum QDF_OPMODE opmode;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}

	work_info = pm_ctx->sta_ap_intf_check_work_info;
	if (!work_info) {
		policy_mgr_err("invalid work info");
		return status;
	}
	if (work_info->sap_plus_go_force_scc.reason == CSA_REASON_UNKNOWN)
		return status;

	vdev_id = work_info->sap_plus_go_force_scc.initiator_vdev_id;
	chan_freq = wlan_get_operation_chan_freq_vdev_id(pm_ctx->pdev, vdev_id);
	opmode = wlan_get_opmode_from_vdev_id(pm_ctx->pdev, vdev_id);
	vdev_con_mode = policy_mgr_qdf_opmode_to_pm_con_mode(psoc, opmode,
							     vdev_id);

	existing_vdev_id =
		policy_mgr_fetch_existing_con_info(
				psoc,
				vdev_id,
				chan_freq,
				&existing_vdev_mode,
				&existing_ch_freq, &existing_ch_width);
	policy_mgr_debug("initiator vdev %d mode %d freq %d, existing vdev %d mode %d freq %d reason %d",
			 vdev_id, vdev_con_mode, chan_freq, existing_vdev_id,
			 existing_vdev_mode, existing_ch_freq,
			 work_info->sap_plus_go_force_scc.reason);

	if (existing_vdev_id == WLAN_UMAC_VDEV_ID_MAX)
		goto force_scc_done;

	if (!((vdev_con_mode == PM_P2P_GO_MODE &&
	       existing_vdev_mode == PM_SAP_MODE) ||
	      (vdev_con_mode == PM_SAP_MODE &&
	       existing_vdev_mode == PM_P2P_GO_MODE)))
		goto force_scc_done;

	if (!pm_ctx->hdd_cbacks.wlan_check_cc_intf_cb)
		goto force_scc_done;

	intf_ch_freq = 0;
	status = pm_ctx->hdd_cbacks.wlan_check_cc_intf_cb(psoc,
							  existing_vdev_id,
							  &intf_ch_freq);
	policy_mgr_debug("vdev %d freq %d intf %d status %d",
			 existing_vdev_id, existing_ch_freq,
			 intf_ch_freq, status);
	if (QDF_IS_STATUS_ERROR(status))
		goto force_scc_done;
	if (!intf_ch_freq || intf_ch_freq == existing_ch_freq)
		goto force_scc_done;

	ch_params.ch_width = existing_ch_width;
	if (pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params) {
		status = pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params(
			psoc, existing_vdev_id, intf_ch_freq, &ch_params);
		if (QDF_IS_STATUS_ERROR(status))
			policy_mgr_debug("no candidate valid bw for vdev %d intf %d",
					 existing_vdev_id, intf_ch_freq);
	}

	status = policy_mgr_valid_sap_conc_channel_check(
		    psoc, &intf_ch_freq, existing_ch_freq, existing_vdev_id,
		    &ch_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("warning no candidate freq for vdev %d freq %d intf %d",
			       existing_vdev_id, existing_ch_freq,
			       intf_ch_freq);
		goto force_scc_done;
	}

	if (pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason)
		pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason(
				psoc, existing_vdev_id,
				work_info->sap_plus_go_force_scc.reason);

	status = policy_mgr_change_sap_channel_with_csa(
			psoc, existing_vdev_id, intf_ch_freq,
			ch_params.ch_width, true);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("warning sap/go vdev %d freq %d intf %d csa failed",
			       existing_vdev_id, existing_ch_freq,
			       intf_ch_freq);
	}

force_scc_done:
	work_info->sap_plus_go_force_scc.reason = CSA_REASON_UNKNOWN;
	work_info->sap_plus_go_force_scc.initiator_vdev_id =
					WLAN_UMAC_VDEV_ID_MAX;
	work_info->sap_plus_go_force_scc.responder_vdev_id =
					WLAN_UMAC_VDEV_ID_MAX;

	return status;
}

QDF_STATUS
policy_mgr_check_sap_go_force_scc(struct wlan_objmgr_psoc *psoc,
				  struct wlan_objmgr_vdev *vdev,
				  enum sap_csa_reason_code reason_code)
{
	uint8_t existing_vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	enum policy_mgr_con_mode existing_vdev_mode = PM_MAX_NUM_OF_MODE;
	enum policy_mgr_con_mode vdev_con_mode;
	uint32_t con_freq, chan_freq;
	enum phy_ch_width ch_width;
	uint8_t vdev_id;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct sta_ap_intf_check_work_ctx *work_info;
	enum QDF_OPMODE opmode;

	if (reason_code != CSA_REASON_GO_BSS_STARTED &&
	    reason_code != CSA_REASON_USER_INITIATED)
		return QDF_STATUS_SUCCESS;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}
	if (pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL)
		return QDF_STATUS_SUCCESS;

	if (!vdev) {
		policy_mgr_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = wlan_vdev_get_id(vdev);
	opmode = wlan_vdev_mlme_get_opmode(vdev);
	work_info = pm_ctx->sta_ap_intf_check_work_info;
	if (!work_info) {
		policy_mgr_err("invalid work info");
		return QDF_STATUS_E_INVAL;
	}

	chan_freq = wlan_get_operation_chan_freq(vdev);
	vdev_con_mode = policy_mgr_qdf_opmode_to_pm_con_mode(psoc, opmode,
							     vdev_id);

	existing_vdev_id =
		policy_mgr_fetch_existing_con_info(psoc,
						   vdev_id,
						   chan_freq,
						   &existing_vdev_mode,
						   &con_freq, &ch_width);
	if (existing_vdev_id == WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_SUCCESS;

	if (!((vdev_con_mode == PM_P2P_GO_MODE &&
	       existing_vdev_mode == PM_SAP_MODE) ||
	      (vdev_con_mode == PM_SAP_MODE &&
	       existing_vdev_mode == PM_P2P_GO_MODE)))
		return QDF_STATUS_SUCCESS;

	work_info->sap_plus_go_force_scc.reason = reason_code;
	work_info->sap_plus_go_force_scc.initiator_vdev_id = vdev_id;
	work_info->sap_plus_go_force_scc.responder_vdev_id = existing_vdev_id;

	policy_mgr_debug("initiator vdev %d freq %d, existing vdev %d freq %d reason %d",
			 vdev_id, chan_freq, existing_vdev_id,
			 con_freq, reason_code);

	if (!qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work,
				    WAIT_BEFORE_GO_FORCESCC_RESTART))
		policy_mgr_debug("change interface request already queued");

	return QDF_STATUS_E_PENDING;
}

/**
 * policy_mgr_is_any_conn_in_transition() - Check if any STA/CLI
 * connection is disconnecting or roaming state
 * @psoc: PSOC object information
 *
 * This function will check connection table and find any STA/CLI
 * in transition state such as disconnecting, link switch or roaming.
 *
 * Return: true if there is one STA/CLI in transition state.
 */
static bool
policy_mgr_is_any_conn_in_transition(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;
	struct policy_mgr_conc_connection_info *conn_info;
	struct wlan_objmgr_vdev *vdev;
	bool non_connected = false;
	bool in_link_switch = false;
	uint8_t vdev_id;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		conn_info = &pm_conc_connection_list[i];
		if (!(conn_info->in_use &&
		      (conn_info->mode == PM_STA_MODE ||
		       conn_info->mode == PM_P2P_CLIENT_MODE)))
			continue;
		vdev_id = conn_info->vdev_id;
		vdev = wlan_objmgr_get_vdev_by_id_from_pdev(
				pm_ctx->pdev, vdev_id, WLAN_POLICY_MGR_ID);
		if (!vdev) {
			policy_mgr_err("vdev %d: not found", vdev_id);
			continue;
		}

		non_connected = !wlan_cm_is_vdev_connected(vdev);

		if (mlo_is_mld_sta(vdev) &&
		    mlo_mgr_is_link_switch_in_progress(vdev))
			in_link_switch = true;

		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		if (non_connected) {
			policy_mgr_debug("vdev %d: is in transition state",
					 vdev_id);
			break;
		}
		if (in_link_switch) {
			policy_mgr_debug("vdev %d: sta mld is in link switch state",
					 vdev_id);
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return non_connected || in_link_switch;
}

static void __policy_mgr_check_sta_ap_concurrent_ch_intf(
				struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	uint32_t mcc_to_scc_switch, cc_count = 0, i;
	QDF_STATUS status;
	uint32_t ch_freq;
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct sta_ap_intf_check_work_ctx *work_info;

	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}
	work_info = pm_ctx->sta_ap_intf_check_work_info;

	if (work_info->nan_force_scc_in_progress) {
		policy_mgr_nan_sap_post_enable_conc_check(pm_ctx->psoc);
		return;
	}
	/*
	 * Check if force scc is required for GO + GO case. vdev id will be
	 * valid in case of GO+GO force scc only. So, for valid vdev id move
	 * first GO to newly formed GO channel.
	 */
	policy_mgr_debug("p2p go vdev id: %d csa reason: %d",
			 work_info->go_plus_go_force_scc.vdev_id,
			 work_info->sap_plus_go_force_scc.reason);
	if (pm_ctx->sta_ap_intf_check_work_info->go_plus_go_force_scc.vdev_id <
	    WLAN_UMAC_VDEV_ID_MAX) {
		policy_mgr_do_go_plus_go_force_scc(
			pm_ctx->psoc, work_info->go_plus_go_force_scc.vdev_id,
			work_info->go_plus_go_force_scc.ch_freq,
			work_info->go_plus_go_force_scc.ch_width);
		work_info->go_plus_go_force_scc.vdev_id = WLAN_UMAC_VDEV_ID_MAX;
		goto end;
	}
	/*
	 * Check if force scc is required for GO + SAP case.
	 */
	if (pm_ctx->sta_ap_intf_check_work_info->sap_plus_go_force_scc.reason !=
	    CSA_REASON_UNKNOWN) {
		status = policy_mgr_handle_sap_plus_go_force_scc(pm_ctx->psoc);
		goto end;
	}

	mcc_to_scc_switch =
		policy_mgr_get_mcc_to_scc_switch_mode(pm_ctx->psoc);

	policy_mgr_debug("Concurrent open sessions running: %d",
			 policy_mgr_concurrent_open_sessions_running(pm_ctx->psoc));

	if (!policy_mgr_is_sap_go_existed(pm_ctx->psoc))
		goto end;

	cc_count = policy_mgr_get_sap_mode_info(pm_ctx->psoc,
						&op_ch_freq_list[cc_count],
						&vdev_id[cc_count]);

	policy_mgr_debug("Number of concurrent SAP: %d", cc_count);
	if (cc_count < MAX_NUMBER_OF_CONC_CONNECTIONS)
		cc_count = cc_count +
				policy_mgr_get_mode_specific_conn_info(
					pm_ctx->psoc, &op_ch_freq_list[cc_count],
					&vdev_id[cc_count], PM_P2P_GO_MODE);
	policy_mgr_debug("Number of beaconing entities (SAP + GO):%d",
							cc_count);
	if (!cc_count) {
		policy_mgr_err("Could not retrieve SAP/GO operating channel&vdevid");
		goto end;
	}

	/* When any STA/CLI is transition state, such as roaming or
	 * disconnecting, skip force scc for this time.
	 */
	if (policy_mgr_is_any_conn_in_transition(pm_ctx->psoc)) {
		policy_mgr_debug("defer sap conc check to a later time due to another sta/cli dicon/roam pending");
		qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work,
				       SAP_CONC_CHECK_DEFER_TIMEOUT_MS);
		goto end;
	}

	if (policy_mgr_is_ap_start_in_progress(pm_ctx->psoc)) {
		policy_mgr_debug("defer sap conc check to a later time due to another sap/go start pending");
		qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work,
				       SAP_CONC_CHECK_DEFER_TIMEOUT_MS);
		goto end;
	}
	if (policy_mgr_is_set_link_in_progress(pm_ctx->psoc)) {
		policy_mgr_debug("defer sap conc check to a later time due to ml sta set link in progress");
		qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work,
				       SAP_CONC_CHECK_DEFER_TIMEOUT_MS);
		goto end;
	}

	if (pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress()) {
		policy_mgr_debug("wait as channel switch is already in progress");
		status = qdf_wait_single_event(
					&pm_ctx->channel_switch_complete_evt,
					CHANNEL_SWITCH_COMPLETE_TIMEOUT);
		if (QDF_IS_STATUS_ERROR(status))
			policy_mgr_err("wait for event failed, still continue with channel switch");
	}

	if (!pm_ctx->hdd_cbacks.wlan_hdd_get_channel_for_sap_restart) {
		policy_mgr_err("SAP restart get channel callback in NULL");
		goto end;
	}
	if (cc_count <= MAX_NUMBER_OF_CONC_CONNECTIONS)
		for (i = 0; i < cc_count; i++) {
			status = pm_ctx->hdd_cbacks.
				wlan_hdd_get_channel_for_sap_restart
					(pm_ctx->psoc, vdev_id[i], &ch_freq);
			if (status == QDF_STATUS_SUCCESS) {
				policy_mgr_debug("SAP vdev id %d restarts, old ch freq :%d new ch freq: %d",
						 vdev_id[i],
						 op_ch_freq_list[i], ch_freq);
				break;
			}
		}

end:
	pm_ctx->last_disconn_sta_freq = 0;
}

void policy_mgr_check_sta_ap_concurrent_ch_intf(void *data)
{
	struct qdf_op_sync *op_sync;
	struct policy_mgr_psoc_priv_obj *pm_ctx = data;
	int ret;

	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}

	ret = qdf_op_protect(&op_sync);
	if (ret) {
		if (ret == -EAGAIN) {
			if (qdf_is_driver_unloading() ||
			    qdf_is_recovering() ||
			    qdf_is_driver_state_module_stop()) {
				policy_mgr_debug("driver not ready");
				return;
			}

			if (!pm_ctx->sta_ap_intf_check_work_info)
				return;

			pm_ctx->work_fail_count++;
			policy_mgr_debug("qdf_op start fail, ret %d, work_fail_count %d",
					 ret, pm_ctx->work_fail_count);
			if (pm_ctx->work_fail_count > 1) {
				pm_ctx->work_fail_count = 0;
				return;
			}
			qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work,
					       SAP_CONC_CHECK_DEFER_TIMEOUT_MS);
		}
		return;
	}
	pm_ctx->work_fail_count = 0;
	__policy_mgr_check_sta_ap_concurrent_ch_intf(data);

	qdf_op_unprotect(op_sync);
}

static bool policy_mgr_valid_sta_channel_check(struct wlan_objmgr_psoc *psoc,
						uint32_t sta_ch_freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool sta_sap_scc_on_dfs_chan, sta_sap_scc_on_indoor_channel;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return false;
	}

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);
	if (wlan_reg_is_dfs_for_freq(pm_ctx->pdev, sta_ch_freq) &&
	    sta_sap_scc_on_dfs_chan) {
		policy_mgr_debug("STA, SAP SCC is allowed on DFS chan %u",
				 sta_ch_freq);
		return true;
	}

	sta_sap_scc_on_indoor_channel =
		policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc);
	if (wlan_reg_is_freq_indoor(pm_ctx->pdev, sta_ch_freq) &&
	    sta_sap_scc_on_indoor_channel) {
		policy_mgr_debug("STA, SAP SCC is allowed on indoor chan %u",
				 sta_ch_freq);
		return true;
	}

	if ((wlan_reg_is_dfs_for_freq(pm_ctx->pdev, sta_ch_freq) &&
	     !sta_sap_scc_on_dfs_chan) ||
	    wlan_reg_is_passive_or_disable_for_pwrmode(
	    pm_ctx->pdev, sta_ch_freq, REG_CURRENT_PWR_MODE) ||
	    (wlan_reg_is_freq_indoor(pm_ctx->pdev, sta_ch_freq) &&
	     !sta_sap_scc_on_indoor_channel) ||
	    (!policy_mgr_sta_sap_scc_on_lte_coex_chan(psoc) &&
	     !policy_mgr_is_safe_channel(psoc, sta_ch_freq))) {
		if (policy_mgr_is_hw_dbs_capable(psoc))
			return true;
		else
			return false;
	}
	else
		return true;
}

static bool policy_mgr_get_srd_enable_for_vdev(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE vdev_opmode;
	bool enable_srd_channel = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev is NULL");
		return false;
	}

	vdev_opmode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	wlan_mlme_get_srd_master_mode_for_vdev(psoc, vdev_opmode,
					       &enable_srd_channel);
	return enable_srd_channel;
}

QDF_STATUS
policy_mgr_valid_sap_conc_channel_check(struct wlan_objmgr_psoc *psoc,
					uint32_t *con_ch_freq,
					uint32_t sap_ch_freq,
					uint8_t sap_vdev_id,
					struct ch_params *ch_params)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t ch_freq = *con_ch_freq;
	bool find_alternate = false;
	enum phy_ch_width old_ch_width;
	bool sta_sap_scc_on_dfs_chan, sta_sap_scc_on_indoor_channel;
	bool is_dfs;
	bool is_6ghz_cap;
	bool is_sta_sap_scc;
	enum policy_mgr_con_mode con_mode;
	uint32_t nan_2g_freq, nan_5g_freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * If force SCC is set, Check if conc channel is DFS
	 * or passive or part of LTE avoided channel list.
	 * In that case move SAP to other band if DBS is supported,
	 * return otherwise
	 */
	if (!policy_mgr_is_force_scc(psoc))
		return QDF_STATUS_SUCCESS;

	/*
	 * If interference is 0, it could be STA/SAP SCC,
	 * check further if SAP can start on STA home channel or
	 * select other band channel if not.
	 */
	if (!ch_freq) {
		if (!policy_mgr_any_other_vdev_on_same_mac_as_freq(psoc,
								   sap_ch_freq,
								   sap_vdev_id))
			return QDF_STATUS_SUCCESS;

		ch_freq = sap_ch_freq;
	}

	if (!ch_freq)
		return QDF_STATUS_SUCCESS;

	con_mode = policy_mgr_con_mode_by_vdev_id(psoc, sap_vdev_id);

	is_sta_sap_scc = policy_mgr_is_sta_sap_scc(psoc, ch_freq);

	nan_2g_freq =
		policy_mgr_mode_specific_get_channel(psoc, PM_NAN_DISC_MODE);
	nan_5g_freq = wlan_nan_get_disc_5g_ch_freq(psoc);

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);

	sta_sap_scc_on_indoor_channel =
		policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc);
	old_ch_width = ch_params->ch_width;
	if (pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params)
		pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params(
			psoc, sap_vdev_id, ch_freq, ch_params);

	is_dfs = wlan_mlme_check_chan_param_has_dfs(
			pm_ctx->pdev, ch_params, ch_freq);
	is_6ghz_cap = policy_mgr_get_ap_6ghz_capable(psoc, sap_vdev_id, NULL);

	if (WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) && is_dfs &&
	    !sta_sap_scc_on_dfs_chan && is_sta_sap_scc) {
		find_alternate = true;
		policymgr_nofl_debug("sap not capable of DFS SCC on con ch_freq %d",
				     ch_freq);
	} else if (wlan_reg_is_disable_for_pwrmode(pm_ctx->pdev, ch_freq,
						   REG_CURRENT_PWR_MODE)) {
		find_alternate = true;
		policymgr_nofl_debug("sap not capable on disabled con ch_freq %d",
				     ch_freq);
	} else if (con_mode == PM_P2P_GO_MODE &&
		   wlan_reg_is_passive_or_disable_for_pwrmode(
						pm_ctx->pdev,
						ch_freq,
						REG_CURRENT_PWR_MODE) &&
		   !(policy_mgr_is_go_scc_strict(psoc) &&
		     (!is_sta_sap_scc || sta_sap_scc_on_dfs_chan))) {
		find_alternate = true;
		policymgr_nofl_debug("Go not capable on dfs/disabled con ch_freq %d",
				     ch_freq);
	} else if (!policy_mgr_is_safe_channel(psoc, ch_freq) &&
		   !(policy_mgr_sta_sap_scc_on_lte_coex_chan(psoc) &&
		     is_sta_sap_scc) &&
		   !(policy_mgr_get_nan_sap_scc_on_lte_coex_chnl(psoc) &&
		    (WLAN_REG_IS_SAME_BAND_FREQS(nan_2g_freq, ch_freq) ||
		     WLAN_REG_IS_SAME_BAND_FREQS(nan_5g_freq, ch_freq)))) {
		find_alternate = true;
		policymgr_nofl_debug("sap not capable unsafe con ch_freq %d",
				     ch_freq);
	} else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq) &&
		   !WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ch_freq) &&
		   !is_6ghz_cap) {
		policymgr_nofl_debug("sap not capable on 6GHZ con ch_freq %d",
				     ch_freq);
		find_alternate = true;
	} else if (wlan_reg_is_etsi_srd_chan_for_freq(pm_ctx->pdev,
						      ch_freq) &&
		   !policy_mgr_get_srd_enable_for_vdev(psoc, sap_vdev_id)) {
		find_alternate = true;
		policymgr_nofl_debug("sap not capable on SRD con ch_freq %d",
				     ch_freq);
	} else if (!policy_mgr_is_sap_go_interface_allowed_on_indoor(
							pm_ctx->pdev,
							sap_vdev_id, ch_freq)) {
		policymgr_nofl_debug("sap not capable on indoor con ch_freq %d is_sta_sap_scc:%d",
				     ch_freq, is_sta_sap_scc);
		find_alternate = true;
	}

	if (find_alternate) {
		if (policy_mgr_is_hw_dbs_capable(psoc)) {
			ch_freq = policy_mgr_get_alternate_channel_for_sap(
						psoc, sap_vdev_id, sap_ch_freq,
						REG_BAND_UNKNOWN);
			policymgr_nofl_debug("selected alternate ch %d",
					     ch_freq);
			if (!ch_freq) {
				policymgr_nofl_debug("Sap can't have concurrency on %d in dbs hw",
						     *con_ch_freq);
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			/* MCC not supported for non-DBS chip*/
			ch_freq = 0;
			if (con_mode == PM_SAP_MODE) {
				policymgr_nofl_debug("MCC situation in non-dbs hw STA freq %d SAP freq %d not supported",
						     *con_ch_freq, sap_ch_freq);
				return QDF_STATUS_E_FAILURE;
			} else {
				policymgr_nofl_debug("MCC situation in non-dbs hw STA freq %d GO freq %d SCC not supported",
						     *con_ch_freq, sap_ch_freq);
			}
		}
	}

	if (ch_freq != sap_ch_freq || old_ch_width != ch_params->ch_width) {
		*con_ch_freq = ch_freq;
		policymgr_nofl_debug("sap conc result con freq %d bw %d org freq %d bw %d",
				     ch_freq, ch_params->ch_width, sap_ch_freq,
				     old_ch_width);
	}

	if (*con_ch_freq &&
	    pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params)
		pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params(
			psoc, sap_vdev_id, ch_freq, ch_params);

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_check_concurrent_intf_and_restart_sap(
		struct wlan_objmgr_psoc *psoc, bool is_acs_mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t mcc_to_scc_switch;
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint32_t cc_count = 0;
	uint32_t timeout_ms = 0;
	bool restart_sap = false;
	uint32_t sap_freq;
	/*
	 * if no sta, sap/p2p go may need switch channel for band
	 * capability change.
	 * If sta exist, sap/p2p go may need switch channel to force scc
	 */
	bool sta_check = false, gc_check = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}
	if (!pm_ctx->sta_ap_intf_check_work_info) {
		policy_mgr_err("Invalid sta_ap_intf_check_work_info");
		return;
	}
	if (!policy_mgr_is_sap_go_existed(psoc)) {
		policy_mgr_debug(
			"No action taken at check_concurrent_intf_and_restart_sap");
		return;
	}

	if (policy_mgr_is_ll_lt_sap_restart_required(psoc)) {
		restart_sap = true;
		goto sap_restart;
	}

	/*
	 * If STA+SAP sessions are on DFS channel and STA+SAP SCC is
	 * enabled on DFS channel then move the SAP out of DFS channel
	 * as soon as STA gets disconnect.
	 * If STA+SAP sessions are on unsafe channel and STA+SAP SCC is
	 * enabled on unsafe channel then move the SAP to safe channel
	 * as soon as STA disconnected.
	 */
	if (policy_mgr_is_sap_restart_required_after_sta_disconnect(
			psoc, INVALID_VDEV_ID, &sap_freq, is_acs_mode)) {
		policy_mgr_debug("move the SAP to configured channel %u",
				 sap_freq);
		restart_sap = true;
		goto sap_restart;
	}

	/*
	 * This is to check the cases where STA got disconnected or
	 * sta is present on some valid channel where SAP evaluation/restart
	 * might be needed.
	 * force SCC with STA+STA+SAP will need some additional logic
	 */
	cc_count = policy_mgr_get_mode_specific_conn_info(
				psoc, &op_ch_freq_list[cc_count],
				&vdev_id[cc_count], PM_STA_MODE);

	sta_check = !cc_count ||
		    policy_mgr_valid_sta_channel_check(psoc, op_ch_freq_list[0]);

	cc_count = 0;
	cc_count = policy_mgr_get_mode_specific_conn_info(
				psoc, &op_ch_freq_list[cc_count],
				&vdev_id[cc_count], PM_P2P_CLIENT_MODE);

	gc_check = !!cc_count;

	mcc_to_scc_switch =
		policy_mgr_get_mcc_to_scc_switch_mode(psoc);
	policy_mgr_debug("MCC to SCC switch: %d chan: %d sta_check: %d, gc_check: %d",
			 mcc_to_scc_switch, op_ch_freq_list[0],
			 sta_check, gc_check);

	cc_count = 0;
	cc_count = policy_mgr_get_mode_specific_conn_info(
				psoc, &op_ch_freq_list[cc_count],
				&vdev_id[cc_count], PM_SAP_MODE);

	/* SAP + SAP case needs additional handling */
	if (cc_count == 1 && !is_acs_mode &&
	    target_psoc_get_sap_coex_fixed_chan_cap(
			wlan_psoc_get_tgt_if_handle(psoc)) &&
	    !policy_mgr_is_safe_channel(psoc, op_ch_freq_list[0])) {
		policy_mgr_debug("Avoid channel switch as it's allowed to operate on unsafe channel: %d",
				 op_ch_freq_list[0]);
		return;
	}

sap_restart:
	/*
	 * If sta_sap_scc_on_dfs_chan is true then standalone SAP is not
	 * allowed on DFS channel. SAP is allowed on DFS channel only when STA
	 * is already connected on that channel.
	 * In following condition restart_sap will be true if
	 * sta_sap_scc_on_dfs_chan is true and SAP is on DFS channel.
	 * This scenario can come if STA+SAP are operating on DFS channel and
	 * STA gets disconnected.
	 */
	if (restart_sap ||
	    ((mcc_to_scc_switch != QDF_MCC_TO_SCC_SWITCH_DISABLE) &&
	    (sta_check || gc_check))) {
		if (!pm_ctx->sta_ap_intf_check_work_info) {
			policy_mgr_err("invalid sta_ap_intf_check_work_info");
			return;
		}

		policy_mgr_debug("Checking for Concurrent Change interference");

		if (policy_mgr_mode_specific_connection_count(
					psoc, PM_P2P_GO_MODE, NULL))
			timeout_ms = MAX_NOA_TIME;

		if (!qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work,
					    timeout_ms)) {
			policy_mgr_debug("change interface request already queued");
			return;
		}
	}
}

bool policy_mgr_check_bw_with_unsafe_chan_freq(struct wlan_objmgr_psoc *psoc,
					       qdf_freq_t center_freq,
					       enum phy_ch_width ch_width)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t freq_start, freq_end, bw, i, unsafe_chan_freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return true;
	}

	if (ch_width <= CH_WIDTH_20MHZ || !center_freq)
		return true;

	if (!pm_ctx->unsafe_channel_count)
		return true;

	bw = wlan_reg_get_bw_value(ch_width);
	freq_start = center_freq - bw / 2;
	freq_end = center_freq + bw / 2;

	for (i = 0; i < pm_ctx->unsafe_channel_count; i++) {
		unsafe_chan_freq = pm_ctx->unsafe_channel_list[i];
		if (unsafe_chan_freq > freq_start &&
		    unsafe_chan_freq < freq_end) {
			policy_mgr_debug("unsafe ch freq %d is in range %d-%d",
					 unsafe_chan_freq,
					 freq_start,
					 freq_end);
			return false;
		}
	}
	return true;
}

/**
 * policy_mgr_change_sap_channel_with_csa() - Move SAP channel using (E)CSA
 * @psoc: PSOC object information
 * @vdev_id: Vdev id
 * @ch_freq: Channel to change
 * @ch_width: channel width to change
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Invoke the callback function to change SAP channel using (E)CSA
 *
 * Return: QDF_STATUS_SUCCESS for success
 */
QDF_STATUS
policy_mgr_change_sap_channel_with_csa(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id, uint32_t ch_freq,
				       uint32_t ch_width, bool forced)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct ch_params ch_params = {0};
	qdf_freq_t center_freq;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_INVAL;
	}
	if (pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params) {
		ch_params.ch_width = ch_width;
		status = pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params(
			psoc, vdev_id, ch_freq, &ch_params);
		if (QDF_IS_STATUS_SUCCESS(status) &&
		    ch_width > ch_params.ch_width)
			ch_width = ch_params.ch_width;
	}

	if (ch_params.mhz_freq_seg1)
		center_freq = ch_params.mhz_freq_seg1;
	else
		center_freq = ch_params.mhz_freq_seg0;

	if (!policy_mgr_check_bw_with_unsafe_chan_freq(psoc,
						       center_freq,
						       ch_width)) {
		policy_mgr_info("SAP bw shrink to 20M for unsafe");
		ch_width = CH_WIDTH_20MHZ;
	}

	if (pm_ctx->hdd_cbacks.sap_restart_chan_switch_cb) {
		policy_mgr_info("SAP change change without restart");
		status = pm_ctx->hdd_cbacks.sap_restart_chan_switch_cb(psoc,
								       vdev_id,
								       ch_freq,
								       ch_width,
								       forced);
	} else {
		status = QDF_STATUS_E_INVAL;
	}

	return status;
}
#endif /* FEATURE_WLAN_MCC_TO_SCC_SWITCH */

QDF_STATUS
policy_mgr_sta_sap_dfs_scc_conc_check(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id,
				      struct csa_offload_params *csa_event)
{
	uint8_t concur_vdev_id, i;
	bool move_sap_go_first;
	enum hw_mode_bandwidth bw;
	qdf_freq_t cur_freq, new_freq;
	struct wlan_objmgr_vdev *vdev, *conc_vdev;
	struct wlan_objmgr_pdev *pdev;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum policy_mgr_con_mode cur_mode;
	enum policy_mgr_con_mode concur_mode = PM_MAX_NUM_OF_MODE;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_INVAL;
	}
	if (!csa_event) {
		policy_mgr_err("CSA IE Received event is NULL");
		return QDF_STATUS_E_INVAL;
	}

	policy_mgr_get_dfs_sta_sap_go_scc_movement(psoc, &move_sap_go_first);
	if (!move_sap_go_first) {
		policy_mgr_err("g_move_sap_go_1st_on_dfs_sta_csa is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	cur_mode = policy_mgr_get_mode_by_vdev_id(psoc, vdev_id);
	if (cur_mode != PM_STA_MODE) {
		policy_mgr_err("CSA received on non-STA connection");
		return QDF_STATUS_E_INVAL;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	cur_freq = wlan_get_operation_chan_freq_vdev_id(pdev, vdev_id);

	if (!wlan_reg_is_dfs_for_freq(pdev, cur_freq) &&
	    !wlan_reg_is_freq_indoor(pdev, cur_freq)) {
		policy_mgr_err("SAP / GO operating channel is non-DFS");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		return QDF_STATUS_E_INVAL;
	}

	/* Check if there is any SAP / GO operating on the same channel or not
	 * If yes, then get the current bandwidth and vdev_id of concurrent SAP
	 * or GO and trigger channel switch to new channel received in CSA on
	 * STA interface. If this new channel is DFS then trigger channel
	 * switch to non-DFS channel. Once STA moves to this new channel and
	 * when it receives very first beacon, it will then enforce SCC again
	 */
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use &&
		    pm_conc_connection_list[i].freq == cur_freq &&
		    pm_conc_connection_list[i].vdev_id != vdev_id &&
		    (pm_conc_connection_list[i].mode == PM_P2P_GO_MODE ||
		     pm_conc_connection_list[i].mode == PM_SAP_MODE)) {
			concur_mode = pm_conc_connection_list[i].mode;
			bw = pm_conc_connection_list[i].bw;
			concur_vdev_id = pm_conc_connection_list[i].vdev_id;
			break;
		}
	}

	/* If there is no concurrent SAP / GO, then return */
	if (concur_mode == PM_MAX_NUM_OF_MODE) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		return QDF_STATUS_E_INVAL;
	}

	conc_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, concur_vdev_id,
							 WLAN_POLICY_MGR_ID);
	if (!conc_vdev) {
		policy_mgr_err("conc_vdev is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_mlme_set_sap_go_move_before_sta(conc_vdev, true);
	wlan_vdev_mlme_set_sap_go_move_before_sta(vdev, true);
	wlan_objmgr_vdev_release_ref(conc_vdev, WLAN_POLICY_MGR_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	/*Change the CSA count*/
	if (pm_ctx->sme_cbacks.sme_change_sap_csa_count)
		/* Total 4 CSA frames are allowed so that GO / SAP
		 * will move to new channel within 500ms
		 */
		pm_ctx->sme_cbacks.sme_change_sap_csa_count(4);
	new_freq = csa_event->csa_chan_freq;

	/* If the new channel is DFS or indoor, then select another channel
	 * and switch the SAP / GO to avoid CAC. This will resume traffic on
	 * SAP / GO interface immediately. Once STA moves to this new channel
	 * and receives the very first beacon, then it will enforece SCC
	 */
	if (wlan_reg_is_dfs_for_freq(pdev, new_freq) ||
	    wlan_reg_is_freq_indoor(pdev, new_freq)) {
		if (wlan_reg_is_24ghz_ch_freq(new_freq)) {
			new_freq = wlan_reg_min_24ghz_chan_freq();
		} else if (wlan_reg_is_5ghz_ch_freq(new_freq)) {
			new_freq = wlan_reg_min_5ghz_chan_freq();
			/* if none of the 5G channel is non-DFS */
			if (wlan_reg_is_dfs_for_freq(pdev, new_freq) ||
			    wlan_reg_is_freq_indoor(pdev, new_freq))
				new_freq = policy_mgr_get_nondfs_preferred_channel(psoc,
										   concur_mode,
										   true,
										   concur_vdev_id);
		} else {
			new_freq = wlan_reg_min_6ghz_chan_freq();
		}
	}
	policy_mgr_debug("Restart vdev: %u on freq: %u",
			 concur_vdev_id, new_freq);

	return policy_mgr_change_sap_channel_with_csa(psoc, concur_vdev_id,
						      new_freq, bw, true);
}

void policy_mgr_sta_sap_dfs_enforce_scc(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id)
{
	bool is_sap_go_moved_before_sta, move_sap_go_first;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	enum policy_mgr_con_mode cur_mode;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev is NULL");
		return;
	}
	is_sap_go_moved_before_sta =
			wlan_vdev_mlme_is_sap_go_move_before_sta(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	wlan_vdev_mlme_set_sap_go_move_before_sta(vdev, false);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	policy_mgr_get_dfs_sta_sap_go_scc_movement(psoc, &move_sap_go_first);
	if (!is_sap_go_moved_before_sta || !move_sap_go_first) {
		policy_mgr_debug("SAP / GO moved before STA: %u INI g_move_sap_go_1st_on_dfs_sta_csa: %u",
				 is_sap_go_moved_before_sta, move_sap_go_first);
		return;
	}

	cur_mode = policy_mgr_get_mode_by_vdev_id(psoc, vdev_id);
	if (cur_mode != PM_STA_MODE) {
		policy_mgr_err("CSA received on non-STA connection");
		return;
	}

	policy_mgr_debug("Enforce SCC");
	policy_mgr_check_concurrent_intf_and_restart_sap(psoc, false);
}

#ifdef WLAN_FEATURE_P2P_P2P_STA
void policy_mgr_do_go_plus_go_force_scc(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, uint32_t ch_freq,
					uint32_t ch_width)
{
	uint8_t total_connection;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	total_connection = policy_mgr_mode_specific_connection_count(
						psoc, PM_P2P_GO_MODE, NULL);

	policy_mgr_debug("Total p2p go connection %d", total_connection);

	/* If any p2p disconnected, don't do csa */
	if (total_connection > 1) {
		if (pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason)
			pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason(
				psoc, vdev_id,
				CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL);

		policy_mgr_change_sap_channel_with_csa(psoc, vdev_id,
						       ch_freq, ch_width, true);
	}
}

void policy_mgr_process_forcescc_for_go(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, uint32_t ch_freq,
					uint32_t ch_width,
					enum policy_mgr_con_mode mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct sta_ap_intf_check_work_ctx *work_info;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	if (!pm_ctx->sta_ap_intf_check_work_info) {
		policy_mgr_err("invalid work info");
		return;
	}
	work_info = pm_ctx->sta_ap_intf_check_work_info;
	if (mode == PM_P2P_GO_MODE) {
		work_info->go_plus_go_force_scc.vdev_id = vdev_id;
		work_info->go_plus_go_force_scc.ch_freq = ch_freq;
		work_info->go_plus_go_force_scc.ch_width = ch_width;
	}

	if (!qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work,
				    WAIT_BEFORE_GO_FORCESCC_RESTART))
		policy_mgr_debug("change interface request already queued");
}
#endif

bool policy_mgr_is_chan_switch_in_progress(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return false;
	}
	if (pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress()) {
		policy_mgr_debug("channel switch is in progress");
		return true;
	}

	return false;
}

QDF_STATUS policy_mgr_wait_chan_switch_complete_evt(
		struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_wait_single_event(
				&pm_ctx->channel_switch_complete_evt,
				CHANNEL_SWITCH_COMPLETE_TIMEOUT);
	if (QDF_IS_STATUS_ERROR(status))
		policy_mgr_err("wait for event failed, still continue with channel switch");

	return status;
}

static void __policy_mgr_is_ap_start_in_progress(struct wlan_objmgr_pdev *pdev,
						 void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	uint32_t *ap_starting_vdev_id = (uint32_t *)arg;
	enum wlan_serialization_cmd_type cmd_type;
	enum QDF_OPMODE op_mode;

	if (!vdev || !ap_starting_vdev_id)
		return;
	if (*ap_starting_vdev_id != WLAN_INVALID_VDEV_ID)
		return;
	op_mode = wlan_vdev_mlme_get_opmode(vdev);
	if (op_mode != QDF_SAP_MODE && op_mode != QDF_P2P_GO_MODE &&
	    op_mode != QDF_NDI_MODE)
		return;
	/* Check AP start is present in active and pending queue or not */
	cmd_type = wlan_serialization_get_vdev_active_cmd_type(vdev);
	if (cmd_type == WLAN_SER_CMD_VDEV_START_BSS ||
	    wlan_ser_is_non_scan_cmd_type_in_vdev_queue(
			vdev, WLAN_SER_CMD_VDEV_START_BSS)) {
		*ap_starting_vdev_id = wlan_vdev_get_id(vdev);
		policy_mgr_debug("vdev %d op mode %d start bss is pending",
				 *ap_starting_vdev_id, op_mode);
	}
}

bool policy_mgr_is_ap_start_in_progress(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t ap_starting_vdev_id = WLAN_INVALID_VDEV_ID;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return false;
	}

	wlan_objmgr_pdev_iterate_obj_list(pm_ctx->pdev, WLAN_VDEV_OP,
					  __policy_mgr_is_ap_start_in_progress,
					  &ap_starting_vdev_id, 0,
					  WLAN_POLICY_MGR_ID);

	return ap_starting_vdev_id != WLAN_INVALID_VDEV_ID;
}

void policy_mgr_process_force_scc_for_nan(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	if (!pm_ctx->sta_ap_intf_check_work_info) {
		policy_mgr_err("invalid work info");
		return;
	}

	pm_ctx->sta_ap_intf_check_work_info->nan_force_scc_in_progress = true;

	if (!qdf_delayed_work_start(&pm_ctx->sta_ap_intf_check_work, 0))
		policy_mgr_debug("change interface request already queued");
}

QDF_STATUS policy_mgr_wait_for_connection_update(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *policy_mgr_context;

	policy_mgr_context = policy_mgr_get_context(psoc);
	if (!policy_mgr_context) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_wait_single_event(
			&policy_mgr_context->connection_update_done_evt,
			CONNECTION_UPDATE_TIMEOUT);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("wait for event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_reset_connection_update(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *policy_mgr_context;

	policy_mgr_context = policy_mgr_get_context(psoc);
	if (!policy_mgr_context) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_event_reset(
		&policy_mgr_context->connection_update_done_evt);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("clear event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_reset_hw_mode_change(struct wlan_objmgr_psoc *psoc)
{
	policy_mgr_err("Clear hw mode change and connection update evt");
	policy_mgr_set_hw_mode_change_in_progress(
			psoc, POLICY_MGR_HW_MODE_NOT_IN_PROGRESS);
	policy_mgr_reset_connection_update(psoc);
}

QDF_STATUS policy_mgr_set_connection_update(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *policy_mgr_context;

	policy_mgr_context = policy_mgr_get_context(psoc);
	if (!policy_mgr_context) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_event_set(&policy_mgr_context->connection_update_done_evt);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("set event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_chan_switch_complete_evt(
		struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Set channel_switch_complete_evt only if no vdev has channel switch
	 * in progress.
	 */
	if (pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress()) {
		policy_mgr_info("Not all channel switch completed");
		return QDF_STATUS_SUCCESS;
	}

	status = qdf_event_set_all(&pm_ctx->channel_switch_complete_evt);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("set event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_reset_chan_switch_complete_evt(
		struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *policy_mgr_context;

	policy_mgr_context = policy_mgr_get_context(psoc);

	if (!policy_mgr_context) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}
	status = qdf_event_reset(
			&policy_mgr_context->channel_switch_complete_evt);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("reset event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_opportunistic_update(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *policy_mgr_context;

	policy_mgr_context = policy_mgr_get_context(psoc);
	if (!policy_mgr_context) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	status = qdf_event_set(
			&policy_mgr_context->opportunistic_update_done_evt);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("set event failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_stop_opportunistic_timer(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;

	policy_mgr_ctx = policy_mgr_get_context(psoc);
	if (!policy_mgr_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	if (policy_mgr_ctx->dbs_opportunistic_timer.state !=
	    QDF_TIMER_STATE_RUNNING)
		return QDF_STATUS_SUCCESS;

	qdf_mc_timer_stop(&policy_mgr_ctx->dbs_opportunistic_timer);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_restart_opportunistic_timer(
		struct wlan_objmgr_psoc *psoc, bool check_state)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;

	if (policy_mgr_is_hwmode_offload_enabled(psoc))
		return QDF_STATUS_E_NOSUPPORT;

	policy_mgr_ctx = policy_mgr_get_context(psoc);
	if (!policy_mgr_ctx) {
		policy_mgr_err("Invalid context");
		return status;
	}

	if (check_state &&
			QDF_TIMER_STATE_RUNNING !=
			policy_mgr_ctx->dbs_opportunistic_timer.state)
		return status;

	qdf_mc_timer_stop(&policy_mgr_ctx->dbs_opportunistic_timer);

	status = qdf_mc_timer_start(
			&policy_mgr_ctx->dbs_opportunistic_timer,
			DBS_OPPORTUNISTIC_TIME * 1000);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("failed to start opportunistic timer");
		return status;
	}

	return status;
}

QDF_STATUS policy_mgr_set_hw_mode_on_channel_switch(
			struct wlan_objmgr_psoc *psoc, uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE, qdf_status;
	enum policy_mgr_conc_next_action action;

	if (policy_mgr_is_hwmode_offload_enabled(psoc))
		return QDF_STATUS_E_NOSUPPORT;

	if (!policy_mgr_is_hw_dbs_capable(psoc)) {
		policy_mgr_rl_debug("PM/DBS is disabled");
		return status;
	}

	action = (*policy_mgr_get_current_pref_hw_mode_ptr)(psoc);
	if ((action != PM_DBS_DOWNGRADE) &&
	    (action != PM_SINGLE_MAC_UPGRADE) &&
	    (action != PM_DBS1_DOWNGRADE) &&
	    (action != PM_DBS2_DOWNGRADE)) {
		policy_mgr_err("Invalid action: %d", action);
		status = QDF_STATUS_SUCCESS;
		goto done;
	}

	policy_mgr_debug("action:%d session id:%d", action, session_id);

	/* Opportunistic timer is started, PM will check if MCC upgrade can be
	 * done on timer expiry. This avoids any possible ping pong effect
	 * as well.
	 */
	if (action == PM_SINGLE_MAC_UPGRADE) {
		qdf_status = policy_mgr_restart_opportunistic_timer(
			psoc, false);
		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			policy_mgr_debug("opportunistic timer for MCC upgrade");
		goto done;
	}

	/* For DBS, we want to move right away to DBS mode */
	status = policy_mgr_next_actions(psoc, session_id, action,
			POLICY_MGR_UPDATE_REASON_AFTER_CHANNEL_SWITCH,
			POLICY_MGR_DEF_REQ_ID);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("no set hw mode command was issued");
		goto done;
	}
done:
	/* success must be returned only when a set hw mode was done */
	return status;
}

QDF_STATUS policy_mgr_check_and_set_hw_mode_for_channel_switch(
		struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		uint32_t ch_freq, enum policy_mgr_conn_update_reason reason)
{
	QDF_STATUS status;
	struct policy_mgr_conc_connection_info info;
	uint8_t num_cxn_del = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum policy_mgr_conc_next_action next_action = PM_NOP;
	bool eht_capab =  false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	wlan_psoc_mlme_get_11be_capab(psoc, &eht_capab);
	if (eht_capab &&
	    policy_mgr_mode_specific_connection_count(psoc,
						      PM_SAP_MODE,
						      NULL) == 1) {
		policy_mgr_stop_opportunistic_timer(psoc);
		goto ch_width_update;
	}

	if (!policy_mgr_is_hw_dbs_capable(psoc) ||
	    (!policy_mgr_is_hw_dbs_2x2_capable(psoc) &&
	    !policy_mgr_is_hw_dbs_required_for_band(
					psoc, HW_MODE_MAC_BAND_2G)))
		return QDF_STATUS_E_NOSUPPORT;

	/*
	 * Stop opportunistic timer as current connection info will change once
	 * channel is switched and thus if required it will be started once
	 * channel switch is completed. With new connection info.
	 */
	policy_mgr_stop_opportunistic_timer(psoc);

	if (wlan_reg_freq_to_band(ch_freq) != REG_BAND_2G)
		return QDF_STATUS_E_NOSUPPORT;

ch_width_update:
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	/*
	 * Store the connection's parameter and temporarily delete it
	 * from the concurrency table. This way the allow concurrency
	 * check can be used as though a new connection is coming up,
	 * after check, restore the connection to concurrency table.
	 */
	policy_mgr_store_and_del_conn_info_by_vdev_id(psoc, vdev_id,
						      &info, &num_cxn_del);

	status = policy_mgr_get_next_action(psoc, vdev_id, ch_freq,
					    reason, &next_action);
	/* Restore the connection entry */
	if (num_cxn_del)
		policy_mgr_restore_deleted_conn_info(psoc, &info, num_cxn_del);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (QDF_IS_STATUS_ERROR(status))
		goto chk_opportunistic_timer;

	if (PM_NOP != next_action)
		status = policy_mgr_next_actions(psoc, vdev_id,
						 next_action, reason,
						 POLICY_MGR_DEF_REQ_ID);
	else
		status = QDF_STATUS_E_NOSUPPORT;

chk_opportunistic_timer:
	/*
	 * If hw mode change failed restart the opportunistic timer to
	 * Switch to single mac if required.
	 */
	if (status == QDF_STATUS_E_FAILURE) {
		policy_mgr_err("Failed to update HW modeStatus %d", status);
		policy_mgr_check_n_start_opportunistic_timer(psoc);
	}

	return status;
}

void policy_mgr_checkn_update_hw_mode_single_mac_mode(
		struct wlan_objmgr_psoc *psoc, uint32_t ch_freq)
{
	uint8_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool dbs_required_2g;
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return;

	if (QDF_TIMER_STATE_RUNNING == pm_ctx->dbs_opportunistic_timer.state)
		qdf_mc_timer_stop(&pm_ctx->dbs_opportunistic_timer);

	dbs_required_2g =
	    policy_mgr_is_hw_dbs_required_for_band(psoc, HW_MODE_MAC_BAND_2G);

	if (dbs_required_2g && WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) {
		policy_mgr_debug("DBS required for new connection");
		return;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use) {
			if (!WLAN_REG_IS_SAME_BAND_FREQS(
			    ch_freq, pm_conc_connection_list[i].freq) &&
			    (WLAN_REG_IS_24GHZ_CH_FREQ(
			    pm_conc_connection_list[i].freq) ||
			    WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq))) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				policy_mgr_debug("DBS required");
				return;
			}
			if (dbs_required_2g && WLAN_REG_IS_24GHZ_CH_FREQ(
			    pm_conc_connection_list[i].freq)) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				policy_mgr_debug("DBS required");
				return;
			}
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	pm_dbs_opportunistic_timer_handler((void *)psoc);
}

void policy_mgr_check_and_stop_opportunistic_timer(
	struct wlan_objmgr_psoc *psoc, uint8_t id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum policy_mgr_conc_next_action action = PM_NOP;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	enum policy_mgr_conn_update_reason reason =
					POLICY_MGR_UPDATE_REASON_MAX;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	if (QDF_TIMER_STATE_RUNNING ==
		pm_ctx->dbs_opportunistic_timer.state) {
		qdf_mc_timer_stop(&pm_ctx->dbs_opportunistic_timer);
		action = policy_mgr_need_opportunistic_upgrade(psoc, &reason);
		if (action) {
			qdf_event_reset(&pm_ctx->opportunistic_update_done_evt);
			status = policy_mgr_next_actions(psoc, id, action,
							 reason,
							 POLICY_MGR_DEF_REQ_ID);
			if (status != QDF_STATUS_SUCCESS) {
				policy_mgr_err("Failed in policy_mgr_next_actions");
				return;
			}
			status = qdf_wait_single_event(
					&pm_ctx->opportunistic_update_done_evt,
					CONNECTION_UPDATE_TIMEOUT);

			if (!QDF_IS_STATUS_SUCCESS(status)) {
				policy_mgr_err("wait for event failed");
				return;
			}
		}
	}
}

void policy_mgr_set_hw_mode_change_in_progress(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_hw_mode_change value)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	pm_ctx->hw_mode_change_in_progress = value;
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	policy_mgr_debug("hw_mode_change_in_progress:%d", value);
}

enum policy_mgr_hw_mode_change policy_mgr_is_hw_mode_change_in_progress(
	struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_hw_mode_change value;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	value = POLICY_MGR_HW_MODE_NOT_IN_PROGRESS;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return value;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	value = pm_ctx->hw_mode_change_in_progress;
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return value;
}

enum policy_mgr_hw_mode_change policy_mgr_get_hw_mode_change_from_hw_mode_index(
	struct wlan_objmgr_psoc *psoc, uint32_t hw_mode_index)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_hw_mode_params hw_mode;
	enum policy_mgr_hw_mode_change value
		= POLICY_MGR_HW_MODE_NOT_IN_PROGRESS;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return value;
	}

	status = policy_mgr_get_hw_mode_from_idx(psoc, hw_mode_index, &hw_mode);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to get HW mode index");
		return value;
	}

	if (hw_mode.dbs_cap) {
		policy_mgr_debug("DBS is requested with HW (%d)",
				 hw_mode_index);
		value = POLICY_MGR_DBS_IN_PROGRESS;
		goto ret_value;
	}

	if (hw_mode.sbs_cap) {
		policy_mgr_debug("SBS is requested with HW (%d)",
				 hw_mode_index);
		value = POLICY_MGR_SBS_IN_PROGRESS;
		goto ret_value;
	}

	value = POLICY_MGR_SMM_IN_PROGRESS;
	policy_mgr_debug("SMM is requested with HW (%d)", hw_mode_index);

ret_value:
	return value;
}

#ifdef WLAN_FEATURE_TDLS_CONCURRENCIES
bool
policy_mgr_get_allowed_tdls_offchannel_freq(struct wlan_objmgr_psoc *psoc,
					    struct wlan_objmgr_vdev *vdev,
					    qdf_freq_t *ch_freq)
{
	struct connection_info info[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t connection_count, i, j, sta_vdev_id;

	*ch_freq = 0;
	/*
	 * TDLS off channel is not allowed in any MCC scenario
	 */
	if (policy_mgr_current_concurrency_is_mcc(psoc)) {
		policy_mgr_dump_current_concurrency(psoc);
		policy_mgr_debug("TDLS off channel not allowed in MCC");
		return false;
	}

	/*
	 * TDLS offchannel is done only when STA is connected on 2G channel and
	 * the current concurrency is not MCC
	 */
	if (!policy_mgr_is_sta_connected_2g(psoc)) {
		policy_mgr_debug("STA not-connected on 2.4 Ghz");
		return false;
	}

	/*
	 * 2 Port DBS scenario - Allow non-STA vdev channel for
	 * TDLS off-channel operation
	 *
	 * 3 Port Scenario - If STA Vdev is on SCC, allow TDLS off-channel on
	 * the channel of vdev on the other MAC
	 * If STA vdev is standalone on one mac, and scc on another mac, then
	 * allow TDLS off channel on other mac scc channel
	 */
	sta_vdev_id = wlan_vdev_get_id(vdev);
	connection_count = policy_mgr_get_connection_info(psoc, info);
	switch (connection_count) {
	case 1:
		return true;
	case 2:
		/*
		 * Allow all the 5GHz/6GHz channels when STA is in SCC
		 */
		if (policy_mgr_current_concurrency_is_scc(psoc)) {
			*ch_freq = 0;
			return true;
		} else if (policy_mgr_is_current_hwmode_dbs(psoc)) {
			/*
			 * In DBS case, allow off-channel operation on the
			 * other mac 5GHz/6GHz channel where the STA is not
			 * present
			 * Don't consider SBS case since STA should be
			 * connected in 2.4GHz channel for TDLS
			 * off-channel and MCC on SBS ex. 3 PORT
			 * 2.4GHz STA + 5GHz Lower MCC + 5GHz Upper will
			 * not be allowed
			 */
			if (sta_vdev_id == info[0].vdev_id)
				*ch_freq = info[1].ch_freq;
			else
				*ch_freq = info[0].ch_freq;

			return true;
		}

		break;
	case 3:

		/*
		 * 3 Vdev SCC on 2.4GHz band. Allow TDLS off-channel operation
		 * on all the 5GHz & 6GHz channels
		 */
		if (info[0].ch_freq == info[1].ch_freq &&
		    info[0].ch_freq == info[2].ch_freq) {
			*ch_freq = 0;
			return true;
		}

		/*
		 * DBS with SCC on one vdev scenario. Allow TDLS off-channel
		 * on other mac frequency where STA is not present
		 * SBS case is not considered since STA should be connected
		 * on 2.4GHz and TDLS off-channel on SBS MCC is not allowed
		 */
		for (i = 0; i < connection_count; i++) {
			for (j = i + 1; j < connection_count; j++) {
				/*
				 * Find 2 vdevs such that STA is one of the vdev
				 * and STA + other vdev are not on same mac.
				 * Return the foreign vdev frequency which is
				 * not on same mac along with STA
				 */
				if (!policy_mgr_2_freq_always_on_same_mac(
							psoc, info[i].ch_freq,
							info[j].ch_freq)) {
					if (sta_vdev_id == info[i].vdev_id) {
						*ch_freq = info[j].ch_freq;
						return true;
					} else if (sta_vdev_id ==
						   info[j].vdev_id) {
						*ch_freq = info[i].ch_freq;
						return true;
					}
				}
			}
		}

		return false;
	default:
		policy_mgr_debug("TDLS off channel not allowed on > 3 port conc");
		break;
	}

	return false;
}
#endif
