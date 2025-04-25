/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains policy manager ll_sap definitions specific to the ll_sap module
 */

#include "wlan_policy_mgr_ll_sap.h"
#include "wlan_policy_mgr_public_struct.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_cmn.h"
#include "wlan_ll_sap_api.h"

void policy_mgr_ll_lt_sap_get_valid_freq(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_pdev *pdev,
					 uint8_t vdev_id,
					 qdf_freq_t sap_ch_freq,
					 uint8_t cc_switch_mode,
					 qdf_freq_t *new_sap_freq,
					 bool *is_ll_lt_sap_present)
{
	enum sap_csa_reason_code csa_reason;
	enum policy_mgr_con_mode conn_mode;
	qdf_freq_t ll_lt_sap_freq = 0;
	*is_ll_lt_sap_present = false;

	/* If Vdev is ll_lt_sap, check if the frequency on which it is
	 * coming up is correct, else, get new frequency
	 */
	if (policy_mgr_is_vdev_ll_lt_sap(psoc, vdev_id)) {
		*new_sap_freq = wlan_get_ll_lt_sap_restart_freq(pdev,
								sap_ch_freq,
								vdev_id,
								&csa_reason);
		*is_ll_lt_sap_present = true;
	}

	ll_lt_sap_freq = policy_mgr_get_ll_lt_sap_freq(psoc);
	if (!ll_lt_sap_freq)
		return;

	conn_mode = policy_mgr_get_mode_by_vdev_id(psoc, vdev_id);

	if (conn_mode == PM_SAP_MODE) {
		/* If ll_lt_sap and concurrent SAP are on same MAC,
		 * update the frequency of concurrent SAP, else return.
		 */
		if (!policy_mgr_are_2_freq_on_same_mac(psoc, sap_ch_freq,
						       ll_lt_sap_freq))
			return;
		goto policy_mgr_check_scc;
	} else if (conn_mode == PM_P2P_GO_MODE) {
		/* If ll_lt_sap and P2P_GO are in SCC,
		 * update the frequency of concurrent GO else, return.
		 */
		if (ll_lt_sap_freq != sap_ch_freq)
			return;
		goto policy_mgr_check_scc;
	} else {
		policy_mgr_debug("Invalid con mode %d vdev %d", conn_mode,
				 vdev_id);
		return;
	}

policy_mgr_check_scc:
	policy_mgr_check_scc_channel(psoc, new_sap_freq, sap_ch_freq, vdev_id,
				     cc_switch_mode);
	policy_mgr_debug("vdev_id %d old_freq %d new_freq %d", vdev_id,
			 sap_ch_freq, new_sap_freq);
}

uint8_t wlan_policy_mgr_get_ll_lt_sap_vdev_id(struct wlan_objmgr_psoc *psoc)
{
	uint8_t ll_lt_sap_cnt;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];

	ll_lt_sap_cnt = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							vdev_id_list,
							PM_LL_LT_SAP_MODE);

	/* Currently only 1 ll_lt_sap is supported */
	if (!ll_lt_sap_cnt)
		return WLAN_INVALID_VDEV_ID;

	return vdev_id_list[0];
}

bool __policy_mgr_is_ll_lt_sap_restart_required(struct wlan_objmgr_psoc *psoc,
						const char *func)
{
	qdf_freq_t ll_lt_sap_freq = 0;
	uint8_t scc_vdev_id;
	bool is_scc = false;
	uint8_t conn_idx = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm ctx");
		return false;
	}

	ll_lt_sap_freq = policy_mgr_get_ll_lt_sap_freq(psoc);

	if (!ll_lt_sap_freq)
		return false;

	/*
	 * Restart ll_lt_sap if any other interface is present in SCC
	 * with LL_LT_SAP.
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_idx = 0; conn_idx < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_idx++) {
		if (pm_conc_connection_list[conn_idx].mode ==
		      PM_LL_LT_SAP_MODE)
			continue;

		if (ll_lt_sap_freq == pm_conc_connection_list[conn_idx].freq) {
			scc_vdev_id = pm_conc_connection_list[conn_idx].vdev_id;
			is_scc = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (is_scc) {
		uint8_t ll_lt_sap_vdev_id =
				wlan_policy_mgr_get_ll_lt_sap_vdev_id(psoc);

		policymgr_nofl_debug("%s ll_lt_sap vdev %d with freq %d is in scc with vdev %d",
				     func, ll_lt_sap_vdev_id, ll_lt_sap_freq,
				     scc_vdev_id);
		return true;
	}

	return false;
}

/**
 * policy_mgr_ll_lt_sap_get_restart_freq_for_concurent_sap() - Get restart frequency
 * for concurrent SAP which is in concurrency with LL_LT_SAP
 * @pm_ctx: Policy manager context
 * @vdev_id: Vdev id of the SAP for which restart freq is required
 * @curr_freq: Current frequency of the SAP for which restart freq is required
 * @ll_lt_sap_enabled: Indicates if ll_lt_sap is getting enabled or disabled
 *
 * This API returns user configured frequency if ll_lt_sap is going down and
 * if ll_lt_sap is coming up it returns frequency according to ll_lt_sap
 * concurrency.
 *
 * Return: Restart frequency
 */
static qdf_freq_t
policy_mgr_ll_lt_sap_get_restart_freq_for_concurent_sap(
					struct policy_mgr_psoc_priv_obj *pm_ctx,
					uint8_t vdev_id,
					qdf_freq_t curr_freq,
					bool ll_lt_sap_enabled)
{
	qdf_freq_t user_config_freq;
	uint8_t i;
	QDF_STATUS status;
	uint32_t channel_list[NUM_CHANNELS];
	uint32_t num_channels;
	qdf_freq_t restart_freq = 0;

	/*
	 * If ll_lt_sap is getting disabled, return user configured frequency
	 * for concurrent SAP restart, if user configured frequency is not valid
	 * frequency, remain on the same frequency and do not restart the SAP
	 */
	if (!ll_lt_sap_enabled) {
		user_config_freq = policy_mgr_get_user_config_sap_freq(
								pm_ctx->psoc,
								vdev_id);
		if (wlan_reg_is_enable_in_secondary_list_for_freq(
							pm_ctx->pdev,
							user_config_freq) &&
		    policy_mgr_is_safe_channel(pm_ctx->psoc, user_config_freq))
			return user_config_freq;
		return curr_freq;
	}

	status = policy_mgr_get_valid_chans(pm_ctx->psoc, channel_list,
					    &num_channels);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Error in getting valid channels");
		return curr_freq;
	}

	/* return first valid 2.4 GHz frequency */
	for (i = 0; i < num_channels; i++) {
		if (wlan_reg_is_24ghz_ch_freq(channel_list[i])) {
			if (!restart_freq)
				restart_freq = channel_list[i];
			/* Prefer SCC frequency */
			if (policy_mgr_get_connection_count_with_ch_freq(
							channel_list[i])) {
				restart_freq = channel_list[i];
				break;
			}
		}
	}
	return restart_freq;
}

void policy_mgr_ll_lt_sap_restart_concurrent_sap(struct wlan_objmgr_psoc *psoc,
						 bool is_ll_lt_sap_enabled)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info sap_info = {0};
	qdf_freq_t restart_freq;
	struct ch_params ch_params = {0};
	uint8_t i;
	enum sap_csa_reason_code csa_reason;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return;
	}

	qdf_mem_zero(&sap_info, sizeof(sap_info));

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (!pm_conc_connection_list[i].in_use)
			continue;
		if (PM_SAP_MODE == pm_conc_connection_list[i].mode ||
		    PM_LL_LT_SAP_MODE == pm_conc_connection_list[i].mode) {
			qdf_mem_copy(&sap_info, &pm_conc_connection_list[i],
				     sizeof(sap_info));
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	/* No concurrent SAP or ll_lt_sap present, return */
	if (!sap_info.in_use)
		return;

	if (sap_info.mode == PM_SAP_MODE) {
		/*
		 * For SBS case, no need to restart concurrent SAP as LL_LT_SAP
		 * and concurrent SAP can be on different MACs
		 */
		if (policy_mgr_is_hw_sbs_capable(psoc))
			return;

		/*
		 * If concurrent SAP is 2.4 GHz and ll_lt_sap is getting enabled
		 * then there is no need to restart the concurrent SAP
		 */
		if (is_ll_lt_sap_enabled &&
		    wlan_reg_is_24ghz_ch_freq(sap_info.freq))
			return;

		/*
		 * If concurrent SAP is 5 GHz/6 GHz and ll_lt_sap is getting
		 * disabled then there is no need to restart the concurrent SAP
		 */
		else if (!is_ll_lt_sap_enabled &&
			 (wlan_reg_is_5ghz_ch_freq(sap_info.freq) ||
			 wlan_reg_is_6ghz_chan_freq(sap_info.freq)))
			return;

		restart_freq =
		policy_mgr_ll_lt_sap_get_restart_freq_for_concurent_sap(
							pm_ctx,
							sap_info.vdev_id,
							sap_info.freq,
							is_ll_lt_sap_enabled);
		csa_reason = CSA_REASON_CONCURRENT_LL_LT_SAP_EVENT;
	} else {
		restart_freq = wlan_get_ll_lt_sap_restart_freq(pm_ctx->pdev,
							       sap_info.freq,
							       sap_info.vdev_id,
							       &csa_reason);
	}

	if (pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress &&
	    pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress()) {
		policy_mgr_debug("channel switch is already in progress");
		return;
	}

	if (!restart_freq) {
		policy_mgr_err("Restart freq not found for vdev %d",
			       sap_info.vdev_id);
		return;
	}
	if (restart_freq == sap_info.freq) {
		policy_mgr_debug("vdev %d restart freq %d same as current freq",
				 sap_info.vdev_id, restart_freq);
		return;
	}
	ch_params.ch_width = policy_mgr_get_ch_width(sap_info.bw);
	wlan_reg_set_channel_params_for_pwrmode(pm_ctx->pdev, restart_freq,
						0, &ch_params,
						REG_CURRENT_PWR_MODE);
	policy_mgr_debug("Restart SAP vdev %d with %d freq width %d",
			 sap_info.vdev_id, restart_freq, ch_params.ch_width);

	if (pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason)
		pm_ctx->hdd_cbacks.wlan_hdd_set_sap_csa_reason(psoc,
							       sap_info.vdev_id,
							       csa_reason);

	policy_mgr_change_sap_channel_with_csa(psoc, sap_info.vdev_id,
					       restart_freq,
					       ch_params.ch_width, true);
}
