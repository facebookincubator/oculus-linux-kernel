/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_policy_mgr_i.h"
#include "cfg_ucfg_api.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_nan_api.h"

#ifdef WLAN_FEATURE_SR
/**
 * policy_mgr_init_same_mac_conc_sr_status() - Function initializes default
 * value to sr_in_same_mac_conc based on INI g_enable_sr_in_same_mac_conc
 *
 * @psoc: Pointer to PSOC
 *
 * Return: void
 */
static void
policy_mgr_init_same_mac_conc_sr_status(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	pm_ctx->cfg.sr_in_same_mac_conc =
		cfg_get(psoc, CFG_ENABLE_SR_IN_SAME_MAC_CONC);
}
#else
static void
policy_mgr_init_same_mac_conc_sr_status(struct wlan_objmgr_psoc *psoc)
{}
#endif

static QDF_STATUS policy_mgr_init_cfg(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_cfg *cfg;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	cfg = &pm_ctx->cfg;

	cfg->mcc_to_scc_switch = cfg_get(psoc, CFG_MCC_TO_SCC_SWITCH);
	cfg->sys_pref = cfg_get(psoc, CFG_CONC_SYS_PREF);

	if (wlan_is_mlo_sta_nan_ndi_allowed(psoc)) {
		cfg->max_conc_cxns = cfg_get(psoc, CFG_MAX_CONC_CXNS) + 1;
		 policy_mgr_err("max_conc_cxns %d nan", cfg->max_conc_cxns);
	} else {
		cfg->max_conc_cxns = cfg_get(psoc, CFG_MAX_CONC_CXNS);
		policy_mgr_err("max_conc_cxns %d non-nan", cfg->max_conc_cxns);
	}
	cfg->max_conc_cxns = QDF_MIN(cfg->max_conc_cxns,
				     MAX_NUMBER_OF_CONC_CONNECTIONS);
	cfg->conc_rule1 = cfg_get(psoc, CFG_ENABLE_CONC_RULE1);
	cfg->conc_rule2 = cfg_get(psoc, CFG_ENABLE_CONC_RULE2);
	cfg->pcl_band_priority = cfg_get(psoc, CFG_PCL_BAND_PRIORITY);
	cfg->dbs_selection_plcy = cfg_get(psoc, CFG_DBS_SELECTION_PLCY);
	cfg->vdev_priority_list = cfg_get(psoc, CFG_VDEV_CUSTOM_PRIORITY_LIST);
	cfg->chnl_select_plcy = cfg_get(psoc, CFG_CHNL_SELECT_LOGIC_CONC);
	cfg->enable_mcc_adaptive_sch =
		cfg_get(psoc, CFG_ENABLE_MCC_ADAPTIVE_SCH_ENABLED_NAME);
	cfg->enable_sta_cxn_5g_band =
		cfg_get(psoc, CFG_ENABLE_STA_CONNECTION_IN_5GHZ);
	cfg->allow_mcc_go_diff_bi =
		cfg_get(psoc, CFG_ALLOW_MCC_GO_DIFF_BI);
	cfg->dual_mac_feature =
		cfg_get(psoc, CFG_DUAL_MAC_FEATURE_DISABLE);
	cfg->sbs_enable =
		cfg_get(psoc, CFG_ENABLE_SBS);
	cfg->is_force_1x1_enable =
		cfg_get(psoc, CFG_FORCE_1X1_FEATURE);
	cfg->sta_sap_scc_on_dfs_chnl =
		cfg_get(psoc, CFG_STA_SAP_SCC_ON_DFS_CHAN);

	/*
	 * Override concurrency sta+sap indoor flag to true if global indoor
	 * flag is true
	 */
	cfg->sta_sap_scc_on_indoor_channel =
		cfg_get(psoc, CFG_STA_SAP_SCC_ON_INDOOR_CHAN);
	if (cfg_get(psoc, CFG_INDOOR_CHANNEL_SUPPORT))
		cfg->sta_sap_scc_on_indoor_channel = true;

	/*
	 * Force set sta_sap_scc_on_dfs_chnl on Non-DBS HW so that standalone
	 * SAP is not allowed on DFS channel on non-DBS HW, Also, force SCC in
	 * case of STA+SAP
	 */
	if (cfg->sta_sap_scc_on_dfs_chnl == 2 &&
	    !cfg_get(psoc, CFG_ENABLE_DFS_MASTER_CAPABILITY))
		cfg->sta_sap_scc_on_dfs_chnl = 0;
	cfg->nan_sap_scc_on_lte_coex_chnl =
		cfg_get(psoc, CFG_NAN_SAP_SCC_ON_LTE_COEX_CHAN);
	cfg->sta_sap_scc_on_lte_coex_chnl =
		cfg_get(psoc, CFG_STA_SAP_SCC_ON_LTE_COEX_CHAN);
	cfg->sap_mandatory_chnl_enable =
		cfg_get(psoc, CFG_ENABLE_SAP_MANDATORY_CHAN_LIST);
	cfg->mark_indoor_chnl_disable =
		cfg_get(psoc, CFG_MARK_INDOOR_AS_DISABLE_FEATURE);
	cfg->go_force_scc = cfg_get(psoc, CFG_P2P_GO_ENABLE_FORCE_SCC);
	cfg->multi_sap_allowed_on_same_band =
		cfg_get(psoc, CFG_MULTI_SAP_ALLOWED_ON_SAME_BAND);
	policy_mgr_init_same_mac_conc_sr_status(psoc);
	cfg->use_sap_original_bw =
		cfg_get(psoc, CFG_SAP_DEFAULT_BW_FOR_RESTART);
	cfg->move_sap_go_1st_on_dfs_sta_csa =
		cfg_get(psoc, CFG_MOVE_SAP_GO_1ST_ON_DFS_STA_CSA);

	return QDF_STATUS_SUCCESS;
}

static void policy_mgr_deinit_cfg(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return;
	}

	qdf_mem_zero(&pm_ctx->cfg, sizeof(pm_ctx->cfg));
}

QDF_STATUS ucfg_policy_mgr_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	status = policy_mgr_init_cfg(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("pm_ctx is NULL");
		return status;
	}

	status = policy_mgr_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("psoc open fail");
		policy_mgr_psoc_close(psoc);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

void ucfg_policy_mgr_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	policy_mgr_psoc_close(psoc);
	policy_mgr_deinit_cfg(psoc);
}

QDF_STATUS ucfg_policy_mgr_get_mcc_scc_switch(struct wlan_objmgr_psoc *psoc,
					      uint8_t *mcc_scc_switch)
{
	return policy_mgr_get_mcc_scc_switch(psoc, mcc_scc_switch);
}

QDF_STATUS ucfg_policy_mgr_get_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t *sys_pref)
{
	return policy_mgr_get_sys_pref(psoc, sys_pref);
}

QDF_STATUS ucfg_policy_mgr_set_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t sys_pref)
{
	return policy_mgr_set_sys_pref(psoc, sys_pref);
}

QDF_STATUS ucfg_policy_mgr_get_conc_rule1(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule1)
{
	return policy_mgr_get_conc_rule1(psoc, conc_rule1);
}

QDF_STATUS ucfg_policy_mgr_get_conc_rule2(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule2)
{
	return policy_mgr_get_conc_rule2(psoc, conc_rule2);
}

QDF_STATUS ucfg_policy_mgr_get_chnl_select_plcy(struct wlan_objmgr_psoc *psoc,
						uint32_t *chnl_select_plcy)
{
	return policy_mgr_get_chnl_select_plcy(psoc, chnl_select_plcy);
}


QDF_STATUS ucfg_policy_mgr_set_dynamic_mcc_adaptive_sch(
					struct wlan_objmgr_psoc *psoc,
					bool dynamic_mcc_adaptive_sch)
{
	return policy_mgr_set_dynamic_mcc_adaptive_sch(
					psoc, dynamic_mcc_adaptive_sch);
}

QDF_STATUS ucfg_policy_mgr_get_dynamic_mcc_adaptive_sch(
					struct wlan_objmgr_psoc *psoc,
					bool *dynamic_mcc_adaptive_sch)
{
	return policy_mgr_get_dynamic_mcc_adaptive_sch(
					psoc, dynamic_mcc_adaptive_sch);
}

QDF_STATUS ucfg_policy_mgr_get_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
						bool *mcc_adaptive_sch)
{
	return policy_mgr_get_mcc_adaptive_sch(psoc, mcc_adaptive_sch);
}

QDF_STATUS ucfg_policy_mgr_get_sta_cxn_5g_band(struct wlan_objmgr_psoc *psoc,
					       uint8_t *enable_sta_cxn_5g_band)
{
	return policy_mgr_get_sta_cxn_5g_band(psoc, enable_sta_cxn_5g_band);
}

QDF_STATUS
ucfg_policy_mgr_get_allow_mcc_go_diff_bi(struct wlan_objmgr_psoc *psoc,
					 uint8_t *allow_mcc_go_diff_bi)
{
	return policy_mgr_get_allow_mcc_go_diff_bi(psoc, allow_mcc_go_diff_bi);
}

QDF_STATUS ucfg_policy_mgr_get_dual_mac_feature(struct wlan_objmgr_psoc *psoc,
						uint8_t *dual_mac_feature)
{
	return policy_mgr_get_dual_mac_feature(psoc, dual_mac_feature);
}

bool ucfg_policy_mgr_get_dual_sta_feature(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_allow_multiple_sta_connections(psoc);
}

QDF_STATUS ucfg_policy_mgr_get_force_1x1(struct wlan_objmgr_psoc *psoc,
					 uint8_t *force_1x1)
{
	return policy_mgr_get_force_1x1(psoc, force_1x1);
}

uint32_t ucfg_policy_mgr_get_max_conc_cxns(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_get_max_conc_cxns(psoc);
}

QDF_STATUS ucfg_policy_mgr_set_max_conc_cxns(struct wlan_objmgr_psoc *psoc,
					     uint32_t max_conc_cxns)
{
	return policy_mgr_set_max_conc_cxns(psoc, max_conc_cxns);
}

QDF_STATUS
ucfg_policy_mgr_get_radio_combinations(struct wlan_objmgr_psoc *psoc,
				       struct radio_combination *comb,
				       uint32_t comb_max,
				       uint32_t *comb_num)
{
	return policy_mgr_get_radio_combinations(psoc, comb,
						 comb_max, comb_num);
}

QDF_STATUS
ucfg_policy_mgr_get_sta_sap_scc_on_dfs_chnl(struct wlan_objmgr_psoc *psoc,
					    uint8_t *sta_sap_scc_on_dfs_chnl)
{
	return policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc,
						      sta_sap_scc_on_dfs_chnl);
}

bool
ucfg_policy_mgr_get_dfs_master_dynamic_enabled(struct wlan_objmgr_psoc *psoc,
					       uint8_t vdev_id)
{
	return policy_mgr_get_dfs_master_dynamic_enabled(psoc, vdev_id);
}

QDF_STATUS
ucfg_policy_mgr_get_sta_sap_scc_lte_coex_chnl(struct wlan_objmgr_psoc *psoc,
					      uint8_t *sta_sap_scc_lte_coex)
{
	return policy_mgr_get_sta_sap_scc_lte_coex_chnl(psoc,
							sta_sap_scc_lte_coex);
}

QDF_STATUS
ucfg_policy_mgr_init_chan_avoidance(struct wlan_objmgr_psoc *psoc,
				    qdf_freq_t *chan_freq_list,
				    uint16_t chan_cnt)
{
	return policy_mgr_init_chan_avoidance(psoc, chan_freq_list, chan_cnt);
}

QDF_STATUS ucfg_policy_mgr_get_sap_mandt_chnl(struct wlan_objmgr_psoc *psoc,
					      uint8_t *sap_mandt_chnl)
{
	return policy_mgr_get_sap_mandt_chnl(psoc, sap_mandt_chnl);
}

QDF_STATUS
ucfg_policy_mgr_get_indoor_chnl_marking(struct wlan_objmgr_psoc *psoc,
					uint8_t *indoor_chnl_marking)
{
	return policy_mgr_get_indoor_chnl_marking(psoc, indoor_chnl_marking);
}

bool
ucfg_policy_mgr_get_sta_sap_scc_on_indoor_chnl(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc) ?
								true : false;
}
