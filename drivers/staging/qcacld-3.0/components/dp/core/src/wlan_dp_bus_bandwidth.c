/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_dp_bus_bandwidth.c
 *
 * Bus Bandwidth Manager implementation
 */

#include "wlan_dp_bus_bandwidth.h"
#include "wlan_dp_main.h"
#include <wlan_objmgr_psoc_obj_i.h>
#include "pld_common.h"
#include "cds_api.h"
#include <wlan_nlink_common.h>
#include "wlan_ipa_ucfg_api.h"
#include "wlan_dp_rx_thread.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "hif.h"
#include "qdf_trace.h"
#include <wlan_cm_api.h>
#include <qdf_threads.h>
#include <qdf_net_stats.h>
#include "wlan_dp_periodic_sta_stats.h"
#include "wlan_mlme_api.h"
#include "wlan_dp_txrx.h"
#include "cdp_txrx_host_stats.h"
#include "wlan_cm_roam_api.h"
#include "hif_main.h"

#ifdef FEATURE_BUS_BANDWIDTH_MGR
/*
 * bus_bw_table_default: default table which provides bus
 * bandwidth level corresponding to a given connection mode and throughput
 * level.
 */
static bus_bw_table_type bus_bw_table_default = {
	[QCA_WLAN_802_11_MODE_11B] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_1,
				      BUS_BW_LEVEL_2, BUS_BW_LEVEL_3,
				      BUS_BW_LEVEL_4, BUS_BW_LEVEL_6,
				      BUS_BW_LEVEL_7, BUS_BW_LEVEL_8,
				      BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11G] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5},
	[QCA_WLAN_802_11_MODE_11A] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5, BUS_BW_LEVEL_5,
				      BUS_BW_LEVEL_5},
	[QCA_WLAN_802_11_MODE_11N] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_1,
				      BUS_BW_LEVEL_2, BUS_BW_LEVEL_3,
				      BUS_BW_LEVEL_4, BUS_BW_LEVEL_6,
				      BUS_BW_LEVEL_7, BUS_BW_LEVEL_8,
				      BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11AC] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_1,
				       BUS_BW_LEVEL_2, BUS_BW_LEVEL_3,
				       BUS_BW_LEVEL_4, BUS_BW_LEVEL_6,
				       BUS_BW_LEVEL_7, BUS_BW_LEVEL_8,
				       BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11AX] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_1,
				       BUS_BW_LEVEL_2, BUS_BW_LEVEL_3,
				       BUS_BW_LEVEL_4, BUS_BW_LEVEL_6,
				       BUS_BW_LEVEL_7, BUS_BW_LEVEL_8,
				       BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11BE] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_1,
				       BUS_BW_LEVEL_2, BUS_BW_LEVEL_3,
				       BUS_BW_LEVEL_4, BUS_BW_LEVEL_6,
				       BUS_BW_LEVEL_7, BUS_BW_LEVEL_8,
				       BUS_BW_LEVEL_9},
};

/*
 * bus_bw_table_low_latency: table which provides bus
 * bandwidth level corresponding to a given connection mode and throughput
 * level in low latency setting.
 */
static bus_bw_table_type bus_bw_table_low_latency = {
	[QCA_WLAN_802_11_MODE_11B] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11G] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11A] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11N] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				      BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11AC] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11AX] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9},
	[QCA_WLAN_802_11_MODE_11BE] = {BUS_BW_LEVEL_NONE, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9, BUS_BW_LEVEL_9,
				       BUS_BW_LEVEL_9},
};

/**
 * bbm_convert_to_pld_bus_lvl() - Convert from internal bus vote level to
 *  PLD bus vote level
 * @vote_lvl: internal bus bw vote level
 *
 * Returns: PLD bus vote level
 */
static enum pld_bus_width_type
bbm_convert_to_pld_bus_lvl(enum bus_bw_level vote_lvl)
{
	switch (vote_lvl) {
	case BUS_BW_LEVEL_1:
		return PLD_BUS_WIDTH_IDLE;
	case BUS_BW_LEVEL_2:
		return PLD_BUS_WIDTH_LOW;
	case BUS_BW_LEVEL_3:
		return PLD_BUS_WIDTH_MEDIUM;
	case BUS_BW_LEVEL_4:
		return PLD_BUS_WIDTH_HIGH;
	case BUS_BW_LEVEL_5:
		return PLD_BUS_WIDTH_LOW_LATENCY;
	case BUS_BW_LEVEL_6:
		return PLD_BUS_WIDTH_MID_HIGH;
	case BUS_BW_LEVEL_7:
		return PLD_BUS_WIDTH_VERY_HIGH;
	case BUS_BW_LEVEL_8:
		return PLD_BUS_WIDTH_ULTRA_HIGH;
	case BUS_BW_LEVEL_9:
		return PLD_BUS_WIDTH_MAX;
	case BUS_BW_LEVEL_NONE:
	default:
		return PLD_BUS_WIDTH_NONE;
	}
}

/**
 * bbm_get_bus_bw_level_vote() - Select bus bw vote level per interface based
 *  on connection mode and throughput level
 * @dp_intf: DP Interface, caller assure that interface is valid.
 * @tput_level: throughput level
 *
 * Returns: Bus bw level
 */
static enum bus_bw_level
bbm_get_bus_bw_level_vote(struct wlan_dp_intf *dp_intf,
			  enum tput_level tput_level)
{
	enum qca_wlan_802_11_mode i;
	enum qca_wlan_802_11_mode dot11_mode;
	enum bus_bw_level vote_lvl = BUS_BW_LEVEL_NONE;
	struct wlan_dp_psoc_context *dp_ctx = dp_intf->dp_ctx;
	struct bbm_context *bbm_ctx = dp_ctx->bbm_ctx;
	bus_bw_table_type *lkp_table = bbm_ctx->curr_bus_bw_lookup_table;
	uint16_t client_count[QCA_WLAN_802_11_MODE_INVALID];
	struct wlan_dp_psoc_callbacks *cb_obj = &dp_ctx->dp_ops;
	hdd_cb_handle ctx = cb_obj->callback_ctx;

	if (tput_level >= TPUT_LEVEL_MAX) {
		dp_err("invalid tput level %d", tput_level);
		return  BUS_BW_LEVEL_NONE;
	}

	switch (dp_intf->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
		if (!cb_obj->wlan_dp_sta_get_dot11mode(ctx,
						       dp_intf->dev,
						       &dot11_mode))
			break;

		if (dot11_mode >= QCA_WLAN_802_11_MODE_INVALID)
			break;

		return (*lkp_table)[dot11_mode][tput_level];
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
		if (!cb_obj->wlan_dp_get_ap_client_count(ctx,
							 dp_intf->dev,
							 client_count))
			break;

		for (i = QCA_WLAN_802_11_MODE_11B;
		     i < QCA_WLAN_802_11_MODE_INVALID; i++) {
			if (client_count[i] &&
			    (*lkp_table)[i][tput_level] > vote_lvl)
				vote_lvl = (*lkp_table)[i][tput_level];
		}

		return vote_lvl;
	case QDF_NDI_MODE:
		if (!cb_obj->wlan_dp_sta_ndi_connected(ctx,
						       dp_intf->dev))
			break;

		/*
		 * If the tput levels are between mid to high range, then
		 * apply next SNOC voting level BUS_BW_LEVEL_5 which maps
		 * to PLD_BUS_WIDTH_LOW_LATENCY.
		 *
		 * NDI dot11mode is currently hardcoded to 11AC in driver and
		 * since the bus bw levels in table do not differ between 11AC
		 * and 11AX, using max supported mode instead. Dot11mode of the
		 * peers are not saved in driver and legacy modes are not
		 * supported in NAN.
		 */
		if (tput_level <= TPUT_LEVEL_HIGH)
			return BUS_BW_LEVEL_5;
		else
			return (*lkp_table)[QCA_WLAN_802_11_MODE_11AX][tput_level];
	default:
		break;
	}

	return vote_lvl;
}

/**
 * bbm_apply_tput_policy() - Apply tput BBM policy by considering
 *  throughput level and connection modes across adapters
 * @dp_ctx: DP context
 * @tput_level: throughput level
 *
 * Returns: None
 */
static void
bbm_apply_tput_policy(struct wlan_dp_psoc_context *dp_ctx,
		      enum tput_level tput_level)
{
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_intf *dp_intf_next;
	struct wlan_objmgr_psoc *psoc;
	enum bus_bw_level next_vote = BUS_BW_LEVEL_NONE;
	enum bus_bw_level tmp_vote;
	struct bbm_context *bbm_ctx = dp_ctx->bbm_ctx;
	hdd_cb_handle ctx = dp_ctx->dp_ops.callback_ctx;

	if (tput_level == TPUT_LEVEL_NONE) {
		/*
		 * This is to handle the scenario where bus bw periodic work
		 * is force cancelled
		 */
		if (dp_ctx->dp_ops.dp_any_adapter_connected(ctx))
			bbm_ctx->per_policy_vote[BBM_TPUT_POLICY] = next_vote;
		return;
	}

	psoc = dp_ctx->psoc;
	if (!psoc) {
		dp_err("psoc is NULL");
		return;
	}

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		if (dp_intf->num_links == 0)
			continue;

		tmp_vote = bbm_get_bus_bw_level_vote(dp_intf, tput_level);
		if (tmp_vote > next_vote)
			next_vote = tmp_vote;
	}

	bbm_ctx->per_policy_vote[BBM_TPUT_POLICY] = next_vote;
}

/**
 * bbm_apply_driver_mode_policy() - Apply driver mode BBM policy
 * @bbm_ctx: bus bw mgr context
 * @driver_mode: global driver mode
 *
 * Returns: None
 */
static void
bbm_apply_driver_mode_policy(struct bbm_context *bbm_ctx,
			     enum QDF_GLOBAL_MODE driver_mode)
{
	switch (driver_mode) {
	case QDF_GLOBAL_MONITOR_MODE:
	case QDF_GLOBAL_FTM_MODE:
		bbm_ctx->per_policy_vote[BBM_DRIVER_MODE_POLICY] =
							    BUS_BW_LEVEL_7;
		return;
	default:
		bbm_ctx->per_policy_vote[BBM_DRIVER_MODE_POLICY] =
							 BUS_BW_LEVEL_NONE;
		return;
	}
}

/**
 * bbm_apply_non_persistent_policy() - Apply non persistent policy and set
 *  the bus bandwidth
 * @dp_ctx: DP context
 * @flag: flag
 *
 * Returns: None
 */
static void
bbm_apply_non_persistent_policy(struct wlan_dp_psoc_context *dp_ctx,
				enum bbm_non_per_flag flag)
{
	hdd_cb_handle ctx = dp_ctx->dp_ops.callback_ctx;

	switch (flag) {
	case BBM_APPS_RESUME:
		if (dp_ctx->dp_ops.dp_any_adapter_connected(ctx)) {
			dp_ctx->bbm_ctx->curr_vote_level = BUS_BW_LEVEL_RESUME;
			pld_request_bus_bandwidth(dp_ctx->qdf_dev->dev,
			       bbm_convert_to_pld_bus_lvl(BUS_BW_LEVEL_RESUME));
		} else {
			dp_ctx->bbm_ctx->curr_vote_level = BUS_BW_LEVEL_NONE;
			pld_request_bus_bandwidth(dp_ctx->qdf_dev->dev,
				 bbm_convert_to_pld_bus_lvl(BUS_BW_LEVEL_NONE));
		}
		return;
	case BBM_APPS_SUSPEND:
		dp_ctx->bbm_ctx->curr_vote_level = BUS_BW_LEVEL_NONE;
		pld_request_bus_bandwidth(dp_ctx->qdf_dev->dev,
			    bbm_convert_to_pld_bus_lvl(BUS_BW_LEVEL_NONE));
		return;
	default:
		dp_info("flag %d not handled in res/sus BBM policy", flag);
		return;
	}
}

/**
 * bbm_apply_wlm_policy() - Apply WLM based BBM policy by selecting
 *  lookup tables based on the latency level
 * @bbm_ctx: Bus BW mgr context
 * @wlm_level: WLM latency level
 *
 * Returns: None
 */
static void
bbm_apply_wlm_policy(struct bbm_context *bbm_ctx, enum wlm_ll_level wlm_level)
{
	switch (wlm_level) {
	case WLM_LL_NORMAL:
		bbm_ctx->curr_bus_bw_lookup_table = &bus_bw_table_default;
		break;
	case WLM_LL_LOW:
		bbm_ctx->curr_bus_bw_lookup_table = &bus_bw_table_low_latency;
		break;
	default:
		dp_info("wlm level %d not handled in BBM WLM policy",
			wlm_level);
		break;
	}
}

/**
 * bbm_apply_user_policy() - Apply user specified bus voting
 *  level
 * @bbm_ctx: Bus BW mgr context
 * @set: set or reset flag
 * @user_level: user bus vote level
 *
 * Returns: qdf status
 */
static QDF_STATUS
bbm_apply_user_policy(struct bbm_context *bbm_ctx, bool set,
		      enum bus_bw_level user_level)
{
	if (user_level >= BUS_BW_LEVEL_MAX) {
		dp_err("Invalid user vote level %d", user_level);
		return QDF_STATUS_E_FAILURE;
	}

	if (set)
		bbm_ctx->per_policy_vote[BBM_USER_POLICY] = user_level;
	else
		bbm_ctx->per_policy_vote[BBM_USER_POLICY] = BUS_BW_LEVEL_NONE;

	return QDF_STATUS_SUCCESS;
}

/**
 * bbm_request_bus_bandwidth() - Set bus bandwidth level
 * @dp_ctx: DP context
 *
 * Returns: None
 */
static void
bbm_request_bus_bandwidth(struct wlan_dp_psoc_context *dp_ctx)
{
	enum bbm_policy i;
	enum bus_bw_level next_vote = BUS_BW_LEVEL_NONE;
	enum pld_bus_width_type pld_vote;
	struct bbm_context *bbm_ctx = dp_ctx->bbm_ctx;

	for (i = BBM_DRIVER_MODE_POLICY; i < BBM_MAX_POLICY; i++) {
		if (bbm_ctx->per_policy_vote[i] > next_vote)
			next_vote = bbm_ctx->per_policy_vote[i];
	}

	if (next_vote != bbm_ctx->curr_vote_level) {
		pld_vote = bbm_convert_to_pld_bus_lvl(next_vote);
		dp_info("Bus bandwidth vote level change from %d to %d pld_vote: %d",
			bbm_ctx->curr_vote_level, next_vote, pld_vote);
		bbm_ctx->curr_vote_level = next_vote;
		pld_request_bus_bandwidth(dp_ctx->qdf_dev->dev, pld_vote);
	}
}

void dp_bbm_apply_independent_policy(struct wlan_objmgr_psoc *psoc,
				     struct bbm_params *params)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct bbm_context *bbm_ctx;
	QDF_STATUS status;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx || !params)
		return;

	bbm_ctx = dp_ctx->bbm_ctx;

	qdf_mutex_acquire(&bbm_ctx->bbm_lock);

	switch (params->policy) {
	case BBM_TPUT_POLICY:
		bbm_apply_tput_policy(dp_ctx, params->policy_info.tput_level);
		break;
	case BBM_NON_PERSISTENT_POLICY:
		bbm_apply_non_persistent_policy(dp_ctx,
						params->policy_info.flag);
		goto done;
	case BBM_DRIVER_MODE_POLICY:
		bbm_apply_driver_mode_policy(bbm_ctx,
					     params->policy_info.driver_mode);
		break;
	case BBM_SELECT_TABLE_POLICY:
		bbm_apply_wlm_policy(bbm_ctx, params->policy_info.wlm_level);
		goto done;
	case BBM_USER_POLICY:
		/*
		 * This policy is not used currently.
		 */
		status = bbm_apply_user_policy(bbm_ctx,
					       params->policy_info.usr.set,
					       params->policy_info.usr.user_level);
		if (QDF_IS_STATUS_ERROR(status))
			goto done;
		break;
	default:
		dp_info("BBM policy %d not handled", params->policy);
		goto done;
	}

	bbm_request_bus_bandwidth(dp_ctx);

done:
	qdf_mutex_release(&bbm_ctx->bbm_lock);
}

int dp_bbm_context_init(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct bbm_context *bbm_ctx;
	QDF_STATUS status;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx)
		return -EINVAL;
	bbm_ctx = qdf_mem_malloc(sizeof(*bbm_ctx));
	if (!bbm_ctx)
		return -ENOMEM;

	bbm_ctx->curr_bus_bw_lookup_table = &bus_bw_table_default;

	status = qdf_mutex_create(&bbm_ctx->bbm_lock);
	if (QDF_IS_STATUS_ERROR(status))
		goto free_ctx;

	dp_ctx->bbm_ctx = bbm_ctx;

	return 0;

free_ctx:
	qdf_mem_free(bbm_ctx);

	return qdf_status_to_os_return(status);
}

void dp_bbm_context_deinit(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct bbm_context *bbm_ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx)
		return;
	bbm_ctx = dp_ctx->bbm_ctx;
	if (!bbm_ctx)
		return;

	dp_ctx->bbm_ctx = NULL;
	qdf_mutex_destroy(&bbm_ctx->bbm_lock);

	qdf_mem_free(bbm_ctx);
}
#endif /* FEATURE_BUS_BANDWIDTH_MGR */
#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
#ifdef FEATURE_RUNTIME_PM
void dp_rtpm_tput_policy_init(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct dp_rtpm_tput_policy_context *ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	ctx = &dp_ctx->rtpm_tput_policy_ctx;
	qdf_runtime_lock_init(&ctx->rtpm_lock);
	ctx->curr_state = DP_RTPM_TPUT_POLICY_STATE_REQUIRED;
	qdf_atomic_init(&ctx->high_tput_vote);
}

void dp_rtpm_tput_policy_deinit(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx;
	struct dp_rtpm_tput_policy_context *ctx;

	dp_ctx = dp_psoc_get_priv(psoc);
	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	ctx = &dp_ctx->rtpm_tput_policy_ctx;
	ctx->curr_state = DP_RTPM_TPUT_POLICY_STATE_INVALID;
	qdf_runtime_lock_deinit(&ctx->rtpm_lock);
}

/**
 * dp_rtpm_tput_policy_prevent() - prevent a runtime bus suspend
 * @dp_ctx: DP handle
 *
 * return: None
 */
static void dp_rtpm_tput_policy_prevent(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_rtpm_tput_policy_context *ctx;

	ctx = &dp_ctx->rtpm_tput_policy_ctx;
	qdf_runtime_pm_prevent_suspend(&ctx->rtpm_lock);
}

/**
 * dp_rtpm_tput_policy_allow() - allow a runtime bus suspend
 * @dp_ctx: DP handle
 *
 * return: None
 */
static void dp_rtpm_tput_policy_allow(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_rtpm_tput_policy_context *ctx;

	ctx = &dp_ctx->rtpm_tput_policy_ctx;
	qdf_runtime_pm_allow_suspend(&ctx->rtpm_lock);
}

#define DP_RTPM_POLICY_HIGH_TPUT_THRESH TPUT_LEVEL_MEDIUM

void dp_rtpm_tput_policy_apply(struct wlan_dp_psoc_context *dp_ctx,
			       enum tput_level tput_level)
{
	int vote;
	enum dp_rtpm_tput_policy_state temp_state;
	struct dp_rtpm_tput_policy_context *ctx;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (qdf_unlikely(!soc))
		return;

	ctx = &dp_ctx->rtpm_tput_policy_ctx;

	if (tput_level >= DP_RTPM_POLICY_HIGH_TPUT_THRESH)
		temp_state = DP_RTPM_TPUT_POLICY_STATE_NOT_REQUIRED;
	else
		temp_state = DP_RTPM_TPUT_POLICY_STATE_REQUIRED;

	if (ctx->curr_state == temp_state)
		return;

	if (temp_state == DP_RTPM_TPUT_POLICY_STATE_REQUIRED) {
		cdp_set_rtpm_tput_policy_requirement(soc, false);
		qdf_atomic_dec(&ctx->high_tput_vote);
		dp_rtpm_tput_policy_allow(dp_ctx);
	} else {
		cdp_set_rtpm_tput_policy_requirement(soc, true);
		qdf_atomic_inc(&ctx->high_tput_vote);
		dp_rtpm_tput_policy_prevent(dp_ctx);
	}

	ctx->curr_state = temp_state;
	vote = qdf_atomic_read(&ctx->high_tput_vote);

	if (vote < 0 || vote > 1) {
		dp_alert_rl("Incorrect vote!");
		QDF_BUG(0);
	}
}

int dp_rtpm_tput_policy_get_vote(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_rtpm_tput_policy_context *ctx;

	ctx = &dp_ctx->rtpm_tput_policy_ctx;
	return qdf_atomic_read(&ctx->high_tput_vote);
}
#endif /* FEATURE_RUNTIME_PM */

void dp_reset_tcp_delack(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	enum wlan_tp_level next_level = WLAN_SVC_TP_LOW;
	struct wlan_rx_tp_data rx_tp_data = {0};

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	if (!dp_ctx->en_tcp_delack_no_lro)
		return;

	rx_tp_data.rx_tp_flags |= TCP_DEL_ACK_IND;
	rx_tp_data.level = next_level;
	dp_ctx->rx_high_ind_cnt = 0;
	wlan_dp_update_tcp_rx_param(dp_ctx, &rx_tp_data);
}

/**
 * dp_reset_tcp_adv_win_scale() - Reset TCP advance window scaling
 * value to default
 * @dp_ctx: pointer to DP context (Should not be NULL)
 *
 * Function used to reset TCP advance window scaling
 * value to its default value
 *
 * Return: None
 */
static void dp_reset_tcp_adv_win_scale(struct wlan_dp_psoc_context *dp_ctx)
{
	enum wlan_tp_level next_level = WLAN_SVC_TP_NONE;
	struct wlan_rx_tp_data rx_tp_data = {0};

	if (!dp_ctx->dp_cfg.enable_tcp_adv_win_scale)
		return;

	rx_tp_data.rx_tp_flags |= TCP_ADV_WIN_SCL;
	rx_tp_data.level = next_level;
	dp_ctx->cur_rx_level = WLAN_SVC_TP_NONE;
	wlan_dp_update_tcp_rx_param(dp_ctx, &rx_tp_data);
}

void wlan_dp_update_tcp_rx_param(struct wlan_dp_psoc_context *dp_ctx,
				 struct wlan_rx_tp_data *data)
{
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_ctx->dp_ops;

	if (!dp_ctx) {
		dp_err("psoc is null");
		return;
	}

	if (!data) {
		dp_err("Data is null");
		return;
	}

	if (dp_ctx->dp_cfg.enable_tcp_param_update)
		dp_ops->osif_dp_send_tcp_param_update_event(dp_ctx->psoc,
							    (union wlan_tp_data *)data,
							    1);
	else
		dp_ops->dp_send_svc_nlink_msg(cds_get_radio_index(),
					      WLAN_SVC_WLAN_TP_IND,
					      (void *)data,
					      sizeof(struct wlan_rx_tp_data));
}

/**
 * wlan_dp_update_tcp_tx_param() - update TCP param in Tx dir
 * @dp_ctx: Pointer to DP context
 * @data: Parameters to update
 *
 * Return: None
 */
static void wlan_dp_update_tcp_tx_param(struct wlan_dp_psoc_context *dp_ctx,
					struct wlan_tx_tp_data *data)
{
	enum wlan_tp_level next_tx_level;
	struct wlan_tx_tp_data *tx_tp_data;
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_ctx->dp_ops;

	if (!dp_ctx) {
		dp_err("psoc is null");
		return;
	}

	if (!data) {
		dp_err("Data is null");
		return;
	}

	tx_tp_data = (struct wlan_tx_tp_data *)data;
	next_tx_level = tx_tp_data->level;

	if (dp_ctx->dp_cfg.enable_tcp_param_update)
		dp_ops->osif_dp_send_tcp_param_update_event(dp_ctx->psoc,
							    (union wlan_tp_data *)data,
							    0);
	else
		dp_ops->dp_send_svc_nlink_msg(cds_get_radio_index(),
					      WLAN_SVC_WLAN_TP_TX_IND,
					      &next_tx_level,
					      sizeof(next_tx_level));
}

/**
 * dp_low_tput_gro_flush_skip_handler() - adjust GRO flush for low tput
 * @dp_ctx: dp_ctx object
 * @next_vote_level: next bus bandwidth level
 * @legacy_client: legacy connection mode active
 *
 * If bus bandwidth level is PLD_BUS_WIDTH_LOW consistently and hit
 * the bus_low_cnt_threshold, set flag to skip GRO flush.
 * If bus bandwidth keeps going to PLD_BUS_WIDTH_IDLE, perform a GRO
 * flush to avoid TCP traffic stall
 *
 * Return: none
 */
static inline void dp_low_tput_gro_flush_skip_handler(
			struct wlan_dp_psoc_context *dp_ctx,
			enum pld_bus_width_type next_vote_level,
			bool legacy_client)
{
	uint32_t threshold = dp_ctx->dp_cfg.bus_low_cnt_threshold;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);
	int i;

	if (next_vote_level == PLD_BUS_WIDTH_LOW && legacy_client) {
		if (++dp_ctx->bus_low_vote_cnt >= threshold)
			qdf_atomic_set(&dp_ctx->low_tput_gro_enable, 1);
	} else {
		if (qdf_atomic_read(&dp_ctx->low_tput_gro_enable) &&
		    dp_ctx->enable_dp_rx_threads) {
			/* flush pending rx pkts when LOW->IDLE */
			dp_info("flush queued GRO pkts");
			for (i = 0; i < cdp_get_num_rx_contexts(soc); i++) {
				dp_rx_gro_flush_ind(soc, i,
						    DP_RX_GRO_NORMAL_FLUSH);
			}
		}

		dp_ctx->bus_low_vote_cnt = 0;
		qdf_atomic_set(&dp_ctx->low_tput_gro_enable, 0);
	}
}

#ifdef WDI3_STATS_UPDATE
/**
 * dp_ipa_set_perf_level() - set IPA perf level
 * @dp_ctx: handle to dp context
 * @tx_pkts: transmit packet count
 * @rx_pkts: receive packet count
 * @ipa_tx_pkts: IPA transmit packet count
 * @ipa_rx_pkts: IPA receive packet count
 *
 * Return: none
 */
static inline
void dp_ipa_set_perf_level(struct wlan_dp_psoc_context *dp_ctx,
			   uint64_t *tx_pkts, uint64_t *rx_pkts,
			   uint32_t *ipa_tx_pkts, uint32_t *ipa_rx_pkts)
{
}
#else
static void dp_ipa_set_perf_level(struct wlan_dp_psoc_context *dp_ctx,
				  uint64_t *tx_pkts, uint64_t *rx_pkts,
				  uint32_t *ipa_tx_pkts, uint32_t *ipa_rx_pkts)
{
	if (ucfg_ipa_is_fw_wdi_activated(dp_ctx->pdev)) {
		ucfg_ipa_uc_stat_query(dp_ctx->pdev, ipa_tx_pkts,
				       ipa_rx_pkts);
		*tx_pkts += *ipa_tx_pkts;
		*rx_pkts += *ipa_rx_pkts;

		ucfg_ipa_set_perf_level(dp_ctx->pdev, *tx_pkts, *rx_pkts);
		ucfg_ipa_uc_stat_request(dp_ctx->pdev, 2);
	}
}
#endif /* WDI3_STATS_UPDATE */

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
/**
 * dp_set_vdev_bundle_require_flag() - set vdev bundle require flag
 * @vdev_id: vdev id
 * @dp_ctx: handle to dp context
 * @tx_bytes: Tx bytes
 *
 * Return: none
 */
static inline
void dp_set_vdev_bundle_require_flag(uint16_t vdev_id,
				     struct wlan_dp_psoc_context *dp_ctx,
				     uint64_t tx_bytes)
{
	struct wlan_dp_psoc_cfg *cfg = dp_ctx->dp_cfg;

	cdp_vdev_set_bundle_require_flag(cds_get_context(QDF_MODULE_ID_SOC),
					 vdev_id, tx_bytes,
					 cfg->bus_bw_compute_interval,
					 cfg->pkt_bundle_threshold_high,
					 cfg->pkt_bundle_threshold_low);
}
#else
static inline
void dp_set_vdev_bundle_require_flag(uint16_t vdev_id,
				     struct wlan_dp_psoc_context *dp_ctx,
				     uint64_t tx_bytes)
{
}
#endif /* WLAN_SUPPORT_TXRX_HL_BUNDLE */

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
/**
 * dp_set_driver_del_ack_enable() - set driver delayed ack enabled flag
 * @vdev_id: vdev id
 * @dp_ctx: handle to dp context
 * @rx_packets: receive packet count
 *
 * Return: none
 */
static inline
void dp_set_driver_del_ack_enable(uint16_t vdev_id,
				  struct wlan_dp_psoc_context *dp_ctx,
				  uint64_t rx_packets)
{
	struct wlan_dp_psoc_cfg *cfg = dp_ctx->dp_cfg;

	cdp_vdev_set_driver_del_ack_enable(cds_get_context(QDF_MODULE_ID_SOC),
					   vdev_id, rx_packets,
					   cfg->bus_bw_compute_interval,
					   cfg->del_ack_threshold_high,
					   cfg->del_ack_threshold_low);
}
#else
static inline
void dp_set_driver_del_ack_enable(uint16_t vdev_id,
				  struct wlan_dp_psoc_context *dp_ctx,
				  uint64_t rx_packets)
{
}
#endif /* QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK */

#define DP_BW_GET_DIFF(_x, _y) ((unsigned long)((ULONG_MAX - (_y)) + (_x) + 1))

#ifdef RX_PERFORMANCE
bool dp_is_current_high_throughput(struct wlan_dp_psoc_context *dp_ctx)
{
	if (dp_ctx->cur_vote_level < PLD_BUS_WIDTH_MEDIUM)
		return false;
	else
		return true;
}
#endif /* RX_PERFORMANCE */

/**
 * wlan_dp_validate_context() - check the DP context
 * @dp_ctx: Global DP context pointer
 *
 * Return: 0 if the context is valid. Error code otherwise
 */
static int wlan_dp_validate_context(struct wlan_dp_psoc_context *dp_ctx)
{
	if (!dp_ctx) {
		dp_err("DP context is null");
		return -ENODEV;
	}

	if (cds_is_driver_recovering()) {
		dp_info("Recovery in progress; state:0x%x",
			cds_get_driver_state());
		return -EAGAIN;
	}

	if (cds_is_load_or_unload_in_progress()) {
		dp_info("Load/unload in progress; state:0x%x",
			cds_get_driver_state());
		return -EAGAIN;
	}

	if (cds_is_driver_in_bad_state()) {
		dp_info("Driver in bad state; state:0x%x",
			cds_get_driver_state());
		return -EAGAIN;
	}

	if (cds_is_fw_down()) {
		dp_info("FW is down; state:0x%x", cds_get_driver_state());
		return -EAGAIN;
	}

	return 0;
}

/**
 * dp_tp_level_to_str() - Convert TPUT level to string
 * @level: TPUT level
 *
 * Return: converted string
 */
static uint8_t *dp_tp_level_to_str(uint32_t level)
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

void wlan_dp_display_tx_rx_histogram(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	int i;

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	dp_nofl_info("BW compute Interval: %d ms",
		     dp_ctx->dp_cfg.bus_bw_compute_interval);
	dp_nofl_info("BW TH - Very High: %d Mid High: %d High: %d Med: %d Low: %d DBS: %d",
		     dp_ctx->dp_cfg.bus_bw_very_high_threshold,
		     dp_ctx->dp_cfg.bus_bw_mid_high_threshold,
		     dp_ctx->dp_cfg.bus_bw_high_threshold,
		     dp_ctx->dp_cfg.bus_bw_medium_threshold,
		     dp_ctx->dp_cfg.bus_bw_low_threshold,
		     dp_ctx->dp_cfg.bus_bw_dbs_threshold);
	dp_nofl_info("Enable TCP DEL ACK: %d",
		     dp_ctx->en_tcp_delack_no_lro);
	dp_nofl_info("TCP DEL High TH: %d TCP DEL Low TH: %d",
		     dp_ctx->dp_cfg.tcp_delack_thres_high,
		     dp_ctx->dp_cfg.tcp_delack_thres_low);
	dp_nofl_info("TCP TX HIGH TP TH: %d (Use to set tcp_output_bytes_lim)",
		     dp_ctx->dp_cfg.tcp_tx_high_tput_thres);

	dp_nofl_info("Total entries: %d Current index: %d",
		     NUM_TX_RX_HISTOGRAM, dp_ctx->txrx_hist_idx);

	if (dp_ctx->txrx_hist) {
		dp_nofl_info("[index][timestamp]: interval_rx, interval_tx, bus_bw_level, RX TP Level, TX TP Level, Rx:Tx pm_qos");

		for (i = 0; i < NUM_TX_RX_HISTOGRAM; i++) {
			struct tx_rx_histogram *hist;

			/* using dp_log to avoid printing function name */
			if (dp_ctx->txrx_hist[i].qtime <= 0)
				continue;
			hist = &dp_ctx->txrx_hist[i];
			dp_nofl_info("[%3d][%15llu]: %6llu, %6llu, %s, %s, %s, %s:%s",
				     i, hist->qtime, hist->interval_rx,
				     hist->interval_tx,
				     pld_bus_width_type_to_str(hist->next_vote_level),
				     dp_tp_level_to_str(hist->next_rx_level),
				     dp_tp_level_to_str(hist->next_tx_level),
				     hist->is_rx_pm_qos_high ? "HIGH" : "LOW",
				     hist->is_tx_pm_qos_high ? "HIGH" : "LOW");
		}
	}
}

void wlan_dp_clear_tx_rx_histogram(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	dp_ctx->txrx_hist_idx = 0;
	if (dp_ctx->txrx_hist)
		qdf_mem_zero(dp_ctx->txrx_hist,
			     (sizeof(struct tx_rx_histogram) *
			     NUM_TX_RX_HISTOGRAM));
}

/**
 * wlan_dp_init_tx_rx_histogram() - init tx/rx histogram stats
 * @dp_ctx: dp context
 *
 * Return: 0 for success or error code
 */
static int wlan_dp_init_tx_rx_histogram(struct wlan_dp_psoc_context *dp_ctx)
{
	dp_ctx->txrx_hist = qdf_mem_malloc(
		(sizeof(struct tx_rx_histogram) * NUM_TX_RX_HISTOGRAM));
	if (!dp_ctx->txrx_hist)
		return -ENOMEM;

	return 0;
}

/**
 * wlan_dp_deinit_tx_rx_histogram() - deinit tx/rx histogram stats
 * @dp_ctx: dp context
 *
 * Return: none
 */
static void wlan_dp_deinit_tx_rx_histogram(struct wlan_dp_psoc_context *dp_ctx)
{
	if (!dp_ctx || !dp_ctx->txrx_hist)
		return;

	qdf_mem_free(dp_ctx->txrx_hist);
	dp_ctx->txrx_hist = NULL;
}

/**
 * wlan_dp_display_txrx_stats() - Display tx/rx histogram stats
 * @dp_ctx: dp context
 *
 * Return: none
 */
static void wlan_dp_display_txrx_stats(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_dp_intf *dp_intf = NULL, *next_dp_intf = NULL;
	struct dp_tx_rx_stats *stats;
	hdd_cb_handle ctx = dp_ctx->dp_ops.callback_ctx;
	int i = 0;
	uint32_t total_rx_pkt, total_rx_dropped,
		 total_rx_delv, total_rx_refused;
	uint32_t total_tx_pkt;
	uint32_t total_tx_dropped;
	uint32_t total_tx_orphaned;

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, next_dp_intf) {
		total_rx_pkt = 0;
		total_rx_dropped = 0;
		total_rx_delv = 0;
		total_rx_refused = 0;
		total_tx_pkt = 0;
		total_tx_dropped = 0;
		total_tx_orphaned = 0;
		stats = &dp_intf->dp_stats.tx_rx_stats;

		if (!dp_intf->num_links)
			continue;

		dp_info("dp_intf: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(dp_intf->mac_addr.bytes));
		for (i = 0; i < NUM_CPUS; i++) {
			total_rx_pkt += stats->per_cpu[i].rx_packets;
			total_rx_dropped += stats->per_cpu[i].rx_dropped;
			total_rx_delv += stats->per_cpu[i].rx_delivered;
			total_rx_refused += stats->per_cpu[i].rx_refused;
			total_tx_pkt += stats->per_cpu[i].tx_called;
			total_tx_dropped += stats->per_cpu[i].tx_dropped;
			total_tx_orphaned += stats->per_cpu[i].tx_orphaned;
		}

		for (i = 0; i < NUM_CPUS; i++) {
			if (!stats->per_cpu[i].tx_called)
				continue;

			dp_info("Tx CPU[%d]: called %u, dropped %u, orphaned %u",
				i, stats->per_cpu[i].tx_called,
				stats->per_cpu[i].tx_dropped,
				stats->per_cpu[i].tx_orphaned);
		}

		dp_info("TX - called %u, dropped %u orphan %u",
			total_tx_pkt, total_tx_dropped,
			total_tx_orphaned);

		dp_ctx->dp_ops.wlan_dp_display_tx_multiq_stats(ctx,
							       dp_intf->dev);

		for (i = 0; i < NUM_CPUS; i++) {
			if (stats->per_cpu[i].rx_packets == 0)
				continue;
			dp_info("Rx CPU[%d]: packets %u, dropped %u, delivered %u, refused %u",
				i, stats->per_cpu[i].rx_packets,
				stats->per_cpu[i].rx_dropped,
				stats->per_cpu[i].rx_delivered,
				stats->per_cpu[i].rx_refused);
		}

		dp_info("RX - packets %u, dropped %u, unsol_arp_mcast_drp %u, delivered %u, refused %u GRO - agg %u drop %u non-agg %u flush_skip %u low_tput_flush %u disabled(conc %u low-tput %u)",
			total_rx_pkt, total_rx_dropped,
			qdf_atomic_read(&stats->rx_usolict_arp_n_mcast_drp),
			total_rx_delv,
			total_rx_refused, stats->rx_aggregated,
			stats->rx_gro_dropped, stats->rx_non_aggregated,
			stats->rx_gro_flush_skip,
			stats->rx_gro_low_tput_flush,
			qdf_atomic_read(&dp_ctx->disable_rx_ol_in_concurrency),
			qdf_atomic_read(&dp_ctx->disable_rx_ol_in_low_tput));
	}
}

/**
 * dp_display_periodic_stats() - Function to display periodic stats
 * @dp_ctx: handle to dp context
 * @data_in_interval: true, if data detected in bw time interval
 *
 * The periodicity is determined by dp_ctx->dp_cfg->periodic_stats_disp_time.
 * Stats show up in wlan driver logs.
 *
 * Returns: None
 */
static void dp_display_periodic_stats(struct wlan_dp_psoc_context *dp_ctx,
				      bool data_in_interval)
{
	static uint32_t counter;
	static bool data_in_time_period;
	ol_txrx_soc_handle soc;
	uint32_t periodic_stats_disp_time = 0;
	hdd_cb_handle ctx = dp_ctx->dp_ops.callback_ctx;

	wlan_mlme_stats_get_periodic_display_time(dp_ctx->psoc,
						  &periodic_stats_disp_time);
	if (!periodic_stats_disp_time)
		return;

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc)
		return;

	counter++;
	if (data_in_interval)
		data_in_time_period = data_in_interval;

	if (counter * dp_ctx->dp_cfg.bus_bw_compute_interval >=
		periodic_stats_disp_time * 1000) {
		hif_rtpm_display_last_busy_hist(cds_get_context(QDF_MODULE_ID_HIF));
		if (data_in_time_period) {
			wlan_dp_display_txrx_stats(dp_ctx);
			dp_txrx_ext_dump_stats(soc, CDP_DP_RX_THREAD_STATS);
			cdp_display_stats(soc,
					  CDP_RX_RING_STATS,
					  QDF_STATS_VERBOSITY_LEVEL_LOW);
			cdp_display_stats(soc,
					  CDP_DP_NAPI_STATS,
					  QDF_STATS_VERBOSITY_LEVEL_LOW);
			cdp_display_stats(soc,
					  CDP_TXRX_PATH_STATS,
					  QDF_STATS_VERBOSITY_LEVEL_LOW);
			cdp_display_stats(soc,
					  CDP_DUMP_TX_FLOW_POOL_INFO,
					  QDF_STATS_VERBOSITY_LEVEL_LOW);
			cdp_display_stats(soc,
					  CDP_DP_SWLM_STATS,
					  QDF_STATS_VERBOSITY_LEVEL_LOW);
			dp_ctx->dp_ops.wlan_dp_display_netif_queue_history
				(ctx, QDF_STATS_VERBOSITY_LEVEL_LOW);
			cdp_display_txrx_hw_info(soc);
			qdf_dp_trace_dump_stats();
		}
		counter = 0;
		data_in_time_period = false;
	}
}

/**
 * dp_pm_qos_update_cpu_mask() - Prepare CPU mask for PM_qos voting
 * @mask: return variable of cpumask for the TPUT
 * @enable_perf_cluster: Enable PERF cluster or not
 *
 * By default, the function sets CPU mask for silver cluster unless
 * enable_perf_cluster is set as true.
 *
 * Return: none
 */
static inline void dp_pm_qos_update_cpu_mask(qdf_cpu_mask *mask,
					     bool enable_perf_cluster)
{
	int package_id;
	unsigned int cpus;
	int perf_cpu_cluster = hif_get_perf_cluster_bitmap();
	int little_cpu_cluster = BIT(CPU_CLUSTER_TYPE_LITTLE);

	qdf_cpumask_clear(mask);
	qdf_for_each_online_cpu(cpus) {
		package_id = qdf_topology_physical_package_id(cpus);
		if (package_id >= 0 &&
		    (BIT(package_id) & little_cpu_cluster ||
		     (enable_perf_cluster &&
		      BIT(package_id) & perf_cpu_cluster))) {
			qdf_cpumask_set_cpu(cpus, mask);
		}
	}
}

/**
 * dp_bus_bandwidth_work_tune_rx() - Function to tune for RX
 * @dp_ctx: handle to dp context
 * @rx_packets: receive packet count in last bus bandwidth interval
 * @diff_us: delta time since last invocation.
 * @next_rx_level: pointer to next_rx_level to be filled
 * @cpu_mask: pm_qos cpu_mask needed for RX, to be filled
 * @is_rx_pm_qos_high: pointer indicating if high qos is needed, to be filled
 *
 * The function tunes various aspects of driver based on a running average
 * of RX packets received in last bus bandwidth interval.
 *
 * Returns: true if RX level has changed, else return false
 */
static
bool dp_bus_bandwidth_work_tune_rx(struct wlan_dp_psoc_context *dp_ctx,
				   const uint64_t rx_packets,
				   uint64_t diff_us,
				   enum wlan_tp_level *next_rx_level,
				   qdf_cpu_mask *cpu_mask,
				   bool *is_rx_pm_qos_high)
{
	bool rx_level_change = false;
	bool rxthread_high_tput_req;
	uint32_t bw_interval_us;
	uint32_t delack_timer_cnt = dp_ctx->dp_cfg.tcp_delack_timer_count;
	uint64_t avg_rx;
	uint64_t no_rx_offload_pkts, avg_no_rx_offload_pkts;
	uint64_t rx_offload_pkts, avg_rx_offload_pkts;

	bw_interval_us = dp_ctx->dp_cfg.bus_bw_compute_interval * 1000;
	no_rx_offload_pkts = dp_ctx->no_rx_offload_pkt_cnt;
	dp_ctx->no_rx_offload_pkt_cnt = 0;

	/* adjust for any sched delays */
	no_rx_offload_pkts = no_rx_offload_pkts * bw_interval_us;
	no_rx_offload_pkts = qdf_do_div(no_rx_offload_pkts, (uint32_t)diff_us);

	/* average no-offload RX packets over last 2 BW intervals */
	avg_no_rx_offload_pkts = (no_rx_offload_pkts +
				  dp_ctx->prev_no_rx_offload_pkts) / 2;
	dp_ctx->prev_no_rx_offload_pkts = no_rx_offload_pkts;

	if (rx_packets >= no_rx_offload_pkts)
		rx_offload_pkts = rx_packets - no_rx_offload_pkts;
	else
		rx_offload_pkts = 0;

	/* average offloaded RX packets over last 2 BW intervals */
	avg_rx_offload_pkts = (rx_offload_pkts +
			       dp_ctx->prev_rx_offload_pkts) / 2;
	dp_ctx->prev_rx_offload_pkts = rx_offload_pkts;

	avg_rx = avg_no_rx_offload_pkts + avg_rx_offload_pkts;

	qdf_cpumask_clear(cpu_mask);

	if (avg_no_rx_offload_pkts > dp_ctx->dp_cfg.bus_bw_high_threshold) {
		rxthread_high_tput_req = true;
		*is_rx_pm_qos_high = true;
		/*Todo: move hdd implementation to qdf */
		dp_pm_qos_update_cpu_mask(cpu_mask, true);
	} else if (avg_rx > dp_ctx->dp_cfg.bus_bw_high_threshold) {
		rxthread_high_tput_req = false;
		*is_rx_pm_qos_high = false;
		dp_pm_qos_update_cpu_mask(cpu_mask, false);
	} else {
		*is_rx_pm_qos_high = false;
		rxthread_high_tput_req = false;
	}

	/*
	 * Takes care to set Rx_thread affinity for below case
	 * 1)LRO/GRO not supported ROME case
	 * 2)when rx_ol is disabled in cases like concurrency etc
	 * 3)For UDP cases
	 */
	if (cds_sched_handle_throughput_req(rxthread_high_tput_req))
		dp_warn("Rx thread high_tput(%d) affinity request failed",
			rxthread_high_tput_req);

	/* fine-tuning parameters for RX Flows */
	if (avg_rx > dp_ctx->dp_cfg.tcp_delack_thres_high) {
		if (dp_ctx->cur_rx_level != WLAN_SVC_TP_HIGH &&
		    ++dp_ctx->rx_high_ind_cnt == delack_timer_cnt) {
			*next_rx_level = WLAN_SVC_TP_HIGH;
		}
	} else {
		dp_ctx->rx_high_ind_cnt = 0;
		*next_rx_level = WLAN_SVC_TP_LOW;
	}

	if (dp_ctx->cur_rx_level != *next_rx_level) {
		struct wlan_rx_tp_data rx_tp_data = {0};

		dp_ctx->cur_rx_level = *next_rx_level;
		rx_level_change = true;
		/* Send throughput indication only if it is enabled.
		 * Disabling tcp_del_ack will revert the tcp stack behavior
		 * to default delayed ack. Note that this will disable the
		 * dynamic delayed ack mechanism across the system
		 */
		if (dp_ctx->en_tcp_delack_no_lro)
			rx_tp_data.rx_tp_flags |= TCP_DEL_ACK_IND;

		if (dp_ctx->dp_cfg.enable_tcp_adv_win_scale)
			rx_tp_data.rx_tp_flags |= TCP_ADV_WIN_SCL;

		rx_tp_data.level = *next_rx_level;
		wlan_dp_update_tcp_rx_param(dp_ctx, &rx_tp_data);
	}

	return rx_level_change;
}

/**
 * dp_bus_bandwidth_work_tune_tx() - Function to tune for TX
 * @dp_ctx: handle to dp context
 * @tx_packets: transmit packet count in last bus bandwidth interval
 * @diff_us: delta time since last invocation.
 * @next_tx_level: pointer to next_tx_level to be filled
 * @cpu_mask: pm_qos cpu_mask needed for TX, to be filled
 * @is_tx_pm_qos_high: pointer indicating if high qos is needed, to be filled
 *
 * The function tunes various aspects of the driver based on a running average
 * of TX packets received in last bus bandwidth interval.
 *
 * Returns: true if TX level has changed, else return false
 */
static
bool dp_bus_bandwidth_work_tune_tx(struct wlan_dp_psoc_context *dp_ctx,
				   const uint64_t tx_packets,
				   uint64_t diff_us,
				   enum wlan_tp_level *next_tx_level,
				   qdf_cpu_mask *cpu_mask,
				   bool *is_tx_pm_qos_high)
{
	bool tx_level_change = false;
	uint32_t bw_interval_us;
	uint64_t no_tx_offload_pkts, avg_no_tx_offload_pkts;
	uint64_t tx_offload_pkts, avg_tx_offload_pkts;
	uint64_t avg_tx;

	bw_interval_us = dp_ctx->dp_cfg.bus_bw_compute_interval * 1000;
	no_tx_offload_pkts = dp_ctx->no_tx_offload_pkt_cnt;

	/* adjust for any sched delays */
	no_tx_offload_pkts = no_tx_offload_pkts * bw_interval_us;
	no_tx_offload_pkts = qdf_do_div(no_tx_offload_pkts, (uint32_t)diff_us);

	/* average no-offload TX packets over last 2 BW intervals */
	avg_no_tx_offload_pkts = (no_tx_offload_pkts +
				  dp_ctx->prev_no_tx_offload_pkts) / 2;
	dp_ctx->no_tx_offload_pkt_cnt = 0;
	dp_ctx->prev_no_tx_offload_pkts = no_tx_offload_pkts;

	if (tx_packets >= no_tx_offload_pkts)
		tx_offload_pkts = tx_packets - no_tx_offload_pkts;
	else
		tx_offload_pkts = 0;

	/* average offloaded TX packets over last 2 BW intervals */
	avg_tx_offload_pkts = (tx_offload_pkts +
			       dp_ctx->prev_tx_offload_pkts) / 2;
	dp_ctx->prev_tx_offload_pkts = tx_offload_pkts;

	avg_tx = avg_no_tx_offload_pkts + avg_tx_offload_pkts;

	/* fine-tuning parameters for TX Flows */
	dp_ctx->prev_tx = tx_packets;

	qdf_cpumask_clear(cpu_mask);

	if (avg_no_tx_offload_pkts >
		dp_ctx->dp_cfg.bus_bw_very_high_threshold) {
		dp_pm_qos_update_cpu_mask(cpu_mask, true);
		*is_tx_pm_qos_high = true;
	} else if (avg_tx > dp_ctx->dp_cfg.bus_bw_high_threshold) {
		dp_pm_qos_update_cpu_mask(cpu_mask, false);
		*is_tx_pm_qos_high = false;
	} else {
		*is_tx_pm_qos_high = false;
	}

	if (avg_tx > dp_ctx->dp_cfg.tcp_tx_high_tput_thres)
		*next_tx_level = WLAN_SVC_TP_HIGH;
	else
		*next_tx_level = WLAN_SVC_TP_LOW;

	if (dp_ctx->dp_cfg.enable_tcp_limit_output &&
	    dp_ctx->cur_tx_level != *next_tx_level) {
		struct wlan_tx_tp_data tx_tp_data = {0};

		dp_ctx->cur_tx_level = *next_tx_level;
		tx_level_change = true;
		tx_tp_data.level = *next_tx_level;
		tx_tp_data.tcp_limit_output = true;
		wlan_dp_update_tcp_tx_param(dp_ctx, &tx_tp_data);
	}

	return tx_level_change;
}

/**
 * dp_sap_p2p_update_mid_high_tput() - Update mid high BW for SAP and P2P mode
 * @dp_ctx: DP context
 * @total_pkts: Total Tx and Rx packets
 *
 * Return: True if mid high threshold is set and opmode is SAP or P2P GO
 */
static inline
bool dp_sap_p2p_update_mid_high_tput(struct wlan_dp_psoc_context *dp_ctx,
				     uint64_t total_pkts)
{
	struct wlan_dp_intf *dp_intf = NULL;
	struct wlan_dp_intf *dp_intf_next = NULL;

	if (dp_ctx->dp_cfg.bus_bw_mid_high_threshold &&
	    total_pkts > dp_ctx->dp_cfg.bus_bw_mid_high_threshold) {
		dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
			if (dp_intf->device_mode == QDF_SAP_MODE ||
			    dp_intf->device_mode == QDF_P2P_GO_MODE)
				return true;
		}
	}

	return false;
}

/**
 * dp_pld_request_bus_bandwidth() - Function to control bus bandwidth
 * @dp_ctx: handle to DP context
 * @tx_packets: transmit packet count received in BW interval
 * @rx_packets: receive packet count received in BW interval
 * @diff_us: delta time since last invocation.
 *
 * The function controls the bus bandwidth and dynamic control of
 * tcp delayed ack configuration.
 *
 * Returns: None
 */
static void dp_pld_request_bus_bandwidth(struct wlan_dp_psoc_context *dp_ctx,
					 const uint64_t tx_packets,
					 const uint64_t rx_packets,
					 const uint64_t diff_us)
{
	uint16_t index;
	bool vote_level_change = false;
	bool rx_level_change;
	bool tx_level_change;
	bool dptrace_high_tput_req;
	u64 total_pkts = tx_packets + rx_packets;
	enum pld_bus_width_type next_vote_level = PLD_BUS_WIDTH_IDLE;
	static enum wlan_tp_level next_rx_level = WLAN_SVC_TP_NONE;
	enum wlan_tp_level next_tx_level = WLAN_SVC_TP_NONE;
	qdf_cpu_mask pm_qos_cpu_mask_tx, pm_qos_cpu_mask_rx, pm_qos_cpu_mask;
	bool is_rx_pm_qos_high;
	bool is_tx_pm_qos_high;
	bool pmqos_on_low_tput = false;
	enum tput_level tput_level;
	bool is_tput_level_high;
	struct bbm_params param = {0};
	bool legacy_client = false;
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);
	static enum tput_level prev_tput_level = TPUT_LEVEL_NONE;
	struct wlan_dp_psoc_callbacks *dp_ops = &dp_ctx->dp_ops;
	hdd_cb_handle ctx = dp_ops->callback_ctx;

	if (!soc)
		return;

	if (dp_ctx->high_bus_bw_request) {
		next_vote_level = PLD_BUS_WIDTH_VERY_HIGH;
		tput_level = TPUT_LEVEL_VERY_HIGH;
	} else if (total_pkts > dp_ctx->dp_cfg.bus_bw_super_high_threshold) {
		next_vote_level = PLD_BUS_WIDTH_MAX;
		tput_level = TPUT_LEVEL_SUPER_HIGH;
	} else if (total_pkts > dp_ctx->dp_cfg.bus_bw_ultra_high_threshold) {
		next_vote_level = PLD_BUS_WIDTH_ULTRA_HIGH;
		tput_level = TPUT_LEVEL_ULTRA_HIGH;
	} else if (total_pkts > dp_ctx->dp_cfg.bus_bw_very_high_threshold) {
		next_vote_level = PLD_BUS_WIDTH_VERY_HIGH;
		tput_level = TPUT_LEVEL_VERY_HIGH;
	} else if (total_pkts > dp_ctx->dp_cfg.bus_bw_high_threshold) {
		next_vote_level = PLD_BUS_WIDTH_HIGH;
		tput_level = TPUT_LEVEL_HIGH;
		if (dp_sap_p2p_update_mid_high_tput(dp_ctx, total_pkts)) {
			next_vote_level = PLD_BUS_WIDTH_MID_HIGH;
			tput_level = TPUT_LEVEL_MID_HIGH;
		}
	} else if (total_pkts > dp_ctx->dp_cfg.bus_bw_medium_threshold) {
		next_vote_level = PLD_BUS_WIDTH_MEDIUM;
		tput_level = TPUT_LEVEL_MEDIUM;
	} else if (total_pkts > dp_ctx->dp_cfg.bus_bw_low_threshold) {
		next_vote_level = PLD_BUS_WIDTH_LOW;
		tput_level = TPUT_LEVEL_LOW;
	} else {
		next_vote_level = PLD_BUS_WIDTH_IDLE;
		tput_level = TPUT_LEVEL_IDLE;
	}

	/*
	 * DBS mode requires more DDR/SNOC resources, vote to ultra high
	 * only when TPUT can reach VHT80 KPI and IPA is disabled,
	 * for other cases, follow general voting logic
	 */
	if (!ucfg_ipa_is_fw_wdi_activated(dp_ctx->pdev) &&
	    policy_mgr_is_current_hwmode_dbs(dp_ctx->psoc) &&
	    (total_pkts > dp_ctx->dp_cfg.bus_bw_dbs_threshold) &&
	    (tput_level < TPUT_LEVEL_SUPER_HIGH)) {
		next_vote_level = PLD_BUS_WIDTH_ULTRA_HIGH;
		tput_level = TPUT_LEVEL_ULTRA_HIGH;
	}

	param.policy = BBM_TPUT_POLICY;
	param.policy_info.tput_level = tput_level;
	dp_bbm_apply_independent_policy(dp_ctx->psoc, &param);

	dp_rtpm_tput_policy_apply(dp_ctx, tput_level);

	dptrace_high_tput_req =
			next_vote_level > PLD_BUS_WIDTH_IDLE ? true : false;

	if (qdf_atomic_read(&dp_ctx->num_latency_critical_clients))
		legacy_client = true;

	dp_low_tput_gro_flush_skip_handler(dp_ctx, next_vote_level,
					   legacy_client);

	if (dp_ctx->cur_vote_level != next_vote_level) {
		/* Set affinity for tx completion grp interrupts */
		if (tput_level >= TPUT_LEVEL_VERY_HIGH &&
		    prev_tput_level < TPUT_LEVEL_VERY_HIGH)
			hif_set_grp_intr_affinity(hif_ctx,
				cdp_get_tx_rings_grp_bitmap(soc), true);
		else if (tput_level < TPUT_LEVEL_VERY_HIGH &&
			 prev_tput_level >= TPUT_LEVEL_VERY_HIGH)
			hif_set_grp_intr_affinity(hif_ctx,
				cdp_get_tx_rings_grp_bitmap(soc),
				false);

		prev_tput_level = tput_level;
		dp_ctx->cur_vote_level = next_vote_level;
		vote_level_change = true;

		if ((next_vote_level == PLD_BUS_WIDTH_LOW) ||
		    (next_vote_level == PLD_BUS_WIDTH_IDLE)) {
			dp_ops->dp_pld_remove_pm_qos(ctx);
			if (dp_ctx->dynamic_rps)
				dp_clear_rps_cpu_mask(dp_ctx);
		} else {
			dp_ops->dp_pld_request_pm_qos(ctx);
			if (dp_ctx->dynamic_rps)
				/*Todo : check once hdd_set_rps_cpu_mask */
				dp_set_rps_cpu_mask(dp_ctx);
		}

		if (dp_ctx->dp_cfg.rx_thread_ul_affinity_mask) {
			if (next_vote_level == PLD_BUS_WIDTH_HIGH &&
			    tx_packets >
			    dp_ctx->dp_cfg.bus_bw_high_threshold &&
			    rx_packets >
			    dp_ctx->dp_cfg.bus_bw_low_threshold)
				cds_sched_handle_rx_thread_affinity_req(true);
			else if (next_vote_level != PLD_BUS_WIDTH_HIGH)
				cds_sched_handle_rx_thread_affinity_req(false);
		}

		dp_ops->dp_napi_apply_throughput_policy(ctx,
							tx_packets,
							rx_packets);

		if (rx_packets < dp_ctx->dp_cfg.bus_bw_low_threshold)
			dp_disable_rx_ol_for_low_tput(dp_ctx, true);
		else
			dp_disable_rx_ol_for_low_tput(dp_ctx, false);

		/*
		 * force disable pktlog and only re-enable based
		 * on ini config
		 */
		if (next_vote_level >= PLD_BUS_WIDTH_HIGH)
			dp_ops->dp_pktlog_enable_disable(ctx,
							 false, 0, 0);
		else if (cds_is_packet_log_enabled())
			dp_ops->dp_pktlog_enable_disable(ctx,
							 true, 0, 0);
	}

	qdf_dp_trace_apply_tput_policy(dptrace_high_tput_req);

	rx_level_change = dp_bus_bandwidth_work_tune_rx(dp_ctx,
							rx_packets,
							diff_us,
							&next_rx_level,
							&pm_qos_cpu_mask_rx,
							&is_rx_pm_qos_high);

	tx_level_change = dp_bus_bandwidth_work_tune_tx(dp_ctx,
							tx_packets,
							diff_us,
							&next_tx_level,
							&pm_qos_cpu_mask_tx,
							&is_tx_pm_qos_high);

	index = dp_ctx->txrx_hist_idx;

	if (vote_level_change) {
		/* Clear mask if BW is not HIGH or more */
		if (next_vote_level < PLD_BUS_WIDTH_HIGH) {
			is_rx_pm_qos_high = false;
			is_tx_pm_qos_high = false;
			qdf_cpumask_clear(&pm_qos_cpu_mask);
			if (next_vote_level == PLD_BUS_WIDTH_LOW &&
			    rx_packets > tx_packets &&
			    !legacy_client) {
				pmqos_on_low_tput = true;
				dp_pm_qos_update_cpu_mask(&pm_qos_cpu_mask,
							  false);
			}
		} else {
			qdf_cpumask_clear(&pm_qos_cpu_mask);
			qdf_cpumask_or(&pm_qos_cpu_mask,
				       &pm_qos_cpu_mask_tx,
				       &pm_qos_cpu_mask_rx);

			/* Default mask in case throughput is high */
			if (qdf_cpumask_empty(&pm_qos_cpu_mask))
				dp_pm_qos_update_cpu_mask(&pm_qos_cpu_mask,
							  false);
		}
		dp_ops->dp_pm_qos_update_request(ctx, &pm_qos_cpu_mask);
		is_tput_level_high =
			tput_level >= TPUT_LEVEL_HIGH ? true : false;
		cdp_set_bus_vote_lvl_high(soc, is_tput_level_high);
	}

	if (vote_level_change || tx_level_change || rx_level_change) {
		dp_info("tx:%llu[%llu(off)+%llu(no-off)] rx:%llu[%llu(off)+%llu(no-off)] next_level(vote %u rx %u tx %u rtpm %d) pm_qos(rx:%u,%*pb tx:%u,%*pb on_low_tput:%u)",
			tx_packets,
			dp_ctx->prev_tx_offload_pkts,
			dp_ctx->prev_no_tx_offload_pkts,
			rx_packets,
			dp_ctx->prev_rx_offload_pkts,
			dp_ctx->prev_no_rx_offload_pkts,
			next_vote_level, next_rx_level, next_tx_level,
			dp_rtpm_tput_policy_get_vote(dp_ctx),
			is_rx_pm_qos_high,
			qdf_cpumask_pr_args(&pm_qos_cpu_mask_rx),
			is_tx_pm_qos_high,
			qdf_cpumask_pr_args(&pm_qos_cpu_mask_tx),
			pmqos_on_low_tput);

		if (dp_ctx->txrx_hist) {
			dp_ctx->txrx_hist[index].next_tx_level = next_tx_level;
			dp_ctx->txrx_hist[index].next_rx_level = next_rx_level;
			dp_ctx->txrx_hist[index].is_rx_pm_qos_high =
				is_rx_pm_qos_high;
			dp_ctx->txrx_hist[index].is_tx_pm_qos_high =
				is_tx_pm_qos_high;
			dp_ctx->txrx_hist[index].next_vote_level =
				next_vote_level;
			dp_ctx->txrx_hist[index].interval_rx = rx_packets;
			dp_ctx->txrx_hist[index].interval_tx = tx_packets;
			dp_ctx->txrx_hist[index].qtime =
				qdf_get_log_timestamp();
			dp_ctx->txrx_hist_idx++;
			dp_ctx->txrx_hist_idx &= NUM_TX_RX_HISTOGRAM_MASK;
		}
	}

	/* Roaming is a high priority job but gets processed in scheduler
	 * thread, bypassing printing stats so that kworker exits quickly and
	 * scheduler thread can utilize CPU.
	 */
	if (!dp_ops->dp_is_roaming_in_progress(ctx)) {
		dp_display_periodic_stats(dp_ctx, (total_pkts > 0) ?
					  true : false);
		dp_periodic_sta_stats_display(dp_ctx);
	}

	hif_affinity_mgr_set_affinity(hif_ctx);
}

#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
/**
 * dp_rx_check_qdisc_for_intf() - Check if any ingress qdisc is configured
 *  for given adapter
 * @dp_intf: pointer to DP interface context
 *
 * The function checks if ingress qdisc is registered for a given
 * net device.
 *
 * Return: None
 */
static void
dp_rx_check_qdisc_for_intf(struct wlan_dp_intf *dp_intf)
{
	struct wlan_dp_psoc_callbacks *dp_ops;
	QDF_STATUS status;

	dp_ops = &dp_intf->dp_ctx->dp_ops;
	status = dp_ops->dp_rx_check_qdisc_configured(dp_intf->dev,
				 dp_intf->dp_ctx->dp_agg_param.tc_ingress_prio);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (qdf_likely(qdf_atomic_read(&dp_intf->gro_disallowed)))
			return;

		dp_debug("ingress qdisc/filter configured disable GRO");
		qdf_atomic_set(&dp_intf->gro_disallowed, 1);

		return;
	} else if (status == QDF_STATUS_E_NOSUPPORT) {
		if (qdf_unlikely(qdf_atomic_read(&dp_intf->gro_disallowed))) {
			dp_debug("ingress qdisc/filter removed enable GRO");
			qdf_atomic_set(&dp_intf->gro_disallowed, 0);
		}
	}
}
#else
static void
dp_rx_check_qdisc_for_intf(struct wlan_dp_intf *dp_intf)
{
}
#endif

#define NO_RX_PKT_LINK_SPEED_AGEOUT_COUNT 50
static void
dp_link_monitoring(struct wlan_dp_psoc_context *dp_ctx,
		   struct wlan_dp_intf *dp_intf)
{
	struct cdp_peer_stats *peer_stats;
	QDF_STATUS status;
	ol_txrx_soc_handle soc;
	struct wlan_objmgr_peer *bss_peer;
	static uint32_t no_rx_times;
	uint64_t  rx_packets;
	uint32_t link_speed;
	struct wlan_objmgr_psoc *psoc;
	struct link_monitoring link_mon;
	struct wlan_dp_link *def_link = dp_intf->def_link;

	/*
	 *  If throughput is high, link speed should be good,  don't check it
	 *  to avoid performance penalty
	 */
	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (cdp_get_bus_lvl_high(soc) == true)
		return;

	link_mon = dp_intf->link_monitoring;
	if (!dp_ctx->dp_ops.link_monitoring_cb)
		return;

	psoc = dp_ctx->psoc;
	/* If no rx packets received for N sec, set link speed to poor */
	if (link_mon.is_rx_linkspeed_good) {
		rx_packets = DP_BW_GET_DIFF(
			qdf_net_stats_get_rx_pkts(&dp_intf->stats),
			dp_intf->prev_rx_packets);
		if (!rx_packets)
			no_rx_times++;
		else
			no_rx_times = 0;
		if (no_rx_times >= NO_RX_PKT_LINK_SPEED_AGEOUT_COUNT) {
			no_rx_times = 0;
			dp_ctx->dp_ops.link_monitoring_cb(psoc,
							  def_link->link_id,
							  false);
			dp_intf->link_monitoring.is_rx_linkspeed_good = false;

			return;
		}
	}
	/* Get rx link speed from dp peer */
	peer_stats = qdf_mem_malloc(sizeof(*peer_stats));
	if (!peer_stats)
		return;

	/* TODO - Temp WAR, check what to do here */
	/* Peer stats for any link peer is going to return the
	 * stats from MLD peer, so its okay to query deflink
	 */
	bss_peer = wlan_vdev_get_bsspeer(def_link->vdev);
	if (!bss_peer) {
		dp_debug("Invalid bss peer");
		qdf_mem_free(peer_stats);
		return;
	}

	status = cdp_host_get_peer_stats(soc, def_link->link_id,
					 bss_peer->macaddr,
					 peer_stats);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(peer_stats);
		return;
	}
	/* Convert rx linkspeed from kbps to mbps to compare with threshold */
	link_speed = peer_stats->rx.last_rx_rate / 1000;

	/*
	 * When found current rx link speed becomes good(above threshold) or
	 * poor, update to firmware.
	 * If the current RX link speed is above the threshold, low rssi
	 * roaming is not needed. If linkspeed_threshold is set to 0, the
	 * firmware will not consider RX link speed in the roaming decision,
	 * driver will send rx link speed poor state to firmware.
	 */
	if (!link_mon.rx_linkspeed_threshold) {
		dp_ctx->dp_ops.link_monitoring_cb(psoc, def_link->link_id,
						  false);
		dp_intf->link_monitoring.is_rx_linkspeed_good = false;
	} else if (link_speed > link_mon.rx_linkspeed_threshold &&
	     !link_mon.is_rx_linkspeed_good) {
		dp_ctx->dp_ops.link_monitoring_cb(psoc, def_link->link_id,
						  true);
		dp_intf->link_monitoring.is_rx_linkspeed_good = true;
	} else if (link_speed < link_mon.rx_linkspeed_threshold &&
		   link_mon.is_rx_linkspeed_good) {
		dp_ctx->dp_ops.link_monitoring_cb(psoc, def_link->link_id,
						  false);
		dp_intf->link_monitoring.is_rx_linkspeed_good = false;
	}

	qdf_mem_free(peer_stats);
}

/**
 * __dp_bus_bw_work_handler() - Bus bandwidth work handler
 * @dp_ctx: handle to DP context
 *
 * The function handles the bus bandwidth work schedule
 *
 * Returns: None
 */
static void __dp_bus_bw_work_handler(struct wlan_dp_psoc_context *dp_ctx)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_dp_intf *dp_intf = NULL, *con_sap_dp_intf = NULL;
	struct wlan_dp_intf *dp_intf_next = NULL;
	struct wlan_dp_link *dp_link = NULL;
	struct wlan_dp_link *dp_link_next;
	uint64_t tx_packets = 0, rx_packets = 0, tx_bytes = 0;
	uint64_t fwd_tx_packets = 0, fwd_rx_packets = 0;
	uint64_t fwd_tx_packets_temp = 0, fwd_rx_packets_temp = 0;
	uint64_t fwd_tx_packets_diff = 0, fwd_rx_packets_diff = 0;
	uint64_t total_tx = 0, total_rx = 0;
	A_STATUS ret;
	bool connected = false;
	uint32_t ipa_tx_packets = 0, ipa_rx_packets = 0;
	uint64_t sta_tx_bytes = 0, sap_tx_bytes = 0;
	uint64_t diff_us;
	uint64_t curr_time_us;
	uint32_t bw_interval_us;
	hdd_cb_handle ctx = dp_ctx->dp_ops.callback_ctx;

	if (wlan_dp_validate_context(dp_ctx))
		goto stop_work;

	if (dp_ctx->is_suspend)
		return;

	bw_interval_us = dp_ctx->dp_cfg.bus_bw_compute_interval * 1000;

	curr_time_us = qdf_get_log_timestamp();
	diff_us = qdf_log_timestamp_to_usecs(
			curr_time_us - dp_ctx->bw_vote_time);
	dp_ctx->bw_vote_time = curr_time_us;

	dp_for_each_intf_held_safe(dp_ctx, dp_intf, dp_intf_next) {
		vdev = dp_objmgr_get_vdev_by_user(dp_intf->def_link,
						  WLAN_DP_ID);
		if (!vdev)
			continue;

		if ((dp_intf->device_mode == QDF_STA_MODE ||
		     dp_intf->device_mode == QDF_P2P_CLIENT_MODE) &&
		    !wlan_cm_is_vdev_active(vdev)) {
			dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
			continue;
		}

		if ((dp_intf->device_mode == QDF_SAP_MODE ||
		     dp_intf->device_mode == QDF_P2P_GO_MODE) &&
		     !dp_ctx->dp_ops.dp_is_ap_active(ctx,
						     dp_intf->dev)) {
			dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
			continue;
		}

		if (dp_ctx->dp_agg_param.tc_based_dyn_gro)
			dp_rx_check_qdisc_for_intf(dp_intf);

		tx_packets += DP_BW_GET_DIFF(
			qdf_net_stats_get_tx_pkts(&dp_intf->stats),
			dp_intf->prev_tx_packets);
		rx_packets += DP_BW_GET_DIFF(
			qdf_net_stats_get_rx_pkts(&dp_intf->stats),
			dp_intf->prev_rx_packets);
		tx_bytes = DP_BW_GET_DIFF(
			qdf_net_stats_get_tx_bytes(&dp_intf->stats),
			dp_intf->prev_tx_bytes);

		if (dp_intf->device_mode == QDF_STA_MODE &&
		    wlan_cm_is_vdev_active(vdev)) {
			dp_ctx->dp_ops.dp_send_mscs_action_frame(ctx,
							dp_intf->dev);
			if (dp_intf->link_monitoring.enabled)
				dp_link_monitoring(dp_ctx, dp_intf);
		}

		ret = A_ERROR;
		fwd_tx_packets = 0;
		fwd_rx_packets = 0;
		if (dp_intf->device_mode == QDF_SAP_MODE ||
		    dp_intf->device_mode == QDF_P2P_GO_MODE ||
		    dp_intf->device_mode == QDF_NDI_MODE) {
			dp_for_each_link_held_safe(dp_intf, dp_link,
						   dp_link_next) {
				ret = cdp_get_intra_bss_fwd_pkts_count(
					cds_get_context(QDF_MODULE_ID_SOC),
					dp_link->link_id,
					&fwd_tx_packets_temp,
					&fwd_rx_packets_temp);
				if (ret == A_OK) {
					fwd_tx_packets += fwd_tx_packets_temp;
					fwd_rx_packets += fwd_rx_packets_temp;
				} else {
					break;
				}
			}
		}

		if (ret == A_OK) {
			fwd_tx_packets_diff += DP_BW_GET_DIFF(
				fwd_tx_packets,
				dp_intf->prev_fwd_tx_packets);
			fwd_rx_packets_diff += DP_BW_GET_DIFF(
				fwd_rx_packets,
				dp_intf->prev_fwd_rx_packets);
		}

		if (dp_intf->device_mode == QDF_SAP_MODE) {
			con_sap_dp_intf = dp_intf;
			sap_tx_bytes =
				qdf_net_stats_get_tx_bytes(&dp_intf->stats);
		}

		if (dp_intf->device_mode == QDF_STA_MODE)
			sta_tx_bytes =
				qdf_net_stats_get_tx_bytes(&dp_intf->stats);

		dp_for_each_link_held_safe(dp_intf, dp_link, dp_link_next) {
			dp_set_driver_del_ack_enable(dp_link->link_id, dp_ctx,
						     rx_packets);

			dp_set_vdev_bundle_require_flag(dp_link->link_id,
							dp_ctx, tx_bytes);
		}

		total_rx += qdf_net_stats_get_rx_pkts(&dp_intf->stats);
		total_tx += qdf_net_stats_get_tx_pkts(&dp_intf->stats);

		qdf_spin_lock_bh(&dp_ctx->bus_bw_lock);
		dp_intf->prev_tx_packets =
			qdf_net_stats_get_tx_pkts(&dp_intf->stats);
		dp_intf->prev_rx_packets =
			qdf_net_stats_get_rx_pkts(&dp_intf->stats);
		dp_intf->prev_fwd_tx_packets = fwd_tx_packets;
		dp_intf->prev_fwd_rx_packets = fwd_rx_packets;
		dp_intf->prev_tx_bytes =
			qdf_net_stats_get_tx_bytes(&dp_intf->stats);
		qdf_spin_unlock_bh(&dp_ctx->bus_bw_lock);
		connected = true;

		dp_objmgr_put_vdev_by_user(vdev, WLAN_DP_ID);
	}

	if (!connected) {
		dp_err("bus bandwidth timer running in disconnected state");
		goto stop_work;
	}

	/* add intra bss forwarded tx and rx packets */
	tx_packets += fwd_tx_packets_diff;
	rx_packets += fwd_rx_packets_diff;

	/* Send embedded Tx packet bytes on STA & SAP interface to IPA driver */
	ucfg_ipa_update_tx_stats(dp_ctx->pdev, sta_tx_bytes, sap_tx_bytes);

	dp_ipa_set_perf_level(dp_ctx, &tx_packets, &rx_packets,
			      &ipa_tx_packets, &ipa_rx_packets);
	if (con_sap_dp_intf) {
		qdf_net_stats_add_tx_pkts(&con_sap_dp_intf->stats,
					  ipa_tx_packets);
		qdf_net_stats_add_rx_pkts(&con_sap_dp_intf->stats,
					  ipa_rx_packets);
	}

	tx_packets = tx_packets * bw_interval_us;
	tx_packets = qdf_do_div(tx_packets, (uint32_t)diff_us);

	rx_packets = rx_packets * bw_interval_us;
	rx_packets = qdf_do_div(rx_packets, (uint32_t)diff_us);

	dp_pld_request_bus_bandwidth(dp_ctx, tx_packets, rx_packets, diff_us);

	return;

stop_work:
	qdf_periodic_work_stop_async(&dp_ctx->bus_bw_work);
}

/**
 * dp_bus_bw_work_handler() - Bus bandwidth work handler
 * @context: handle to DP context
 *
 * The function handles the bus bandwidth work schedule
 *
 * Returns: None
 */
static void dp_bus_bw_work_handler(void *context)
{
	struct wlan_dp_psoc_context *dp_ctx = context;
	struct qdf_op_sync *op_sync;

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	if (qdf_op_protect(&op_sync))
		return;

	__dp_bus_bw_work_handler(dp_ctx);

	qdf_op_unprotect(op_sync);
}

int dp_bus_bandwidth_init(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	hdd_cb_handle ctx = dp_ctx->dp_ops.callback_ctx;
	QDF_STATUS status;

	if (QDF_GLOBAL_FTM_MODE == cds_get_conparam())
		return QDF_STATUS_SUCCESS;

	dp_enter();

	qdf_spinlock_create(&dp_ctx->bus_bw_lock);

	dp_ctx->dp_ops.dp_pm_qos_add_request(ctx);

	wlan_dp_init_tx_rx_histogram(dp_ctx);
	status = qdf_periodic_work_create(&dp_ctx->bus_bw_work,
					  dp_bus_bw_work_handler,
					  dp_ctx);

	dp_exit();

	return qdf_status_to_os_return(status);
}

void dp_bus_bandwidth_deinit(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	hdd_cb_handle ctx;

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	ctx = dp_ctx->dp_ops.callback_ctx;

	if (QDF_GLOBAL_FTM_MODE == cds_get_conparam())
		return;

	dp_enter();

	/* it is expecting the timer has been stopped or not started
	 * when coming deinit.
	 */
	QDF_BUG(!qdf_periodic_work_stop_sync(&dp_ctx->bus_bw_work));

	qdf_periodic_work_destroy(&dp_ctx->bus_bw_work);
	qdf_spinlock_destroy(&dp_ctx->bus_bw_lock);
	wlan_dp_deinit_tx_rx_histogram(dp_ctx);
	dp_ctx->dp_ops.dp_pm_qos_remove_request(ctx);

	dp_exit();
}

/**
 * __dp_bus_bw_compute_timer_start() - start the bus bandwidth timer
 * @psoc: psoc handle
 *
 * Return: None
 */
static void __dp_bus_bw_compute_timer_start(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	if (QDF_GLOBAL_FTM_MODE == cds_get_conparam())
		return;

	qdf_periodic_work_start(&dp_ctx->bus_bw_work,
				dp_ctx->dp_cfg.bus_bw_compute_interval);
	dp_ctx->bw_vote_time = qdf_get_log_timestamp();
}

void dp_bus_bw_compute_timer_start(struct wlan_objmgr_psoc *psoc)
{
	dp_enter();

	__dp_bus_bw_compute_timer_start(psoc);

	dp_exit();
}

void dp_bus_bw_compute_timer_try_start(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	hdd_cb_handle ctx;

	dp_enter();

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	ctx = dp_ctx->dp_ops.callback_ctx;

	if (dp_ctx->dp_ops.dp_any_adapter_connected(ctx))
		__dp_bus_bw_compute_timer_start(psoc);

	dp_exit();
}

/**
 * __dp_bus_bw_compute_timer_stop() - stop the bus bandwidth timer
 * @psoc: psoc handle
 *
 * Return: None
 */
static void __dp_bus_bw_compute_timer_stop(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	hdd_cb_handle ctx;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	struct bbm_params param = {0};
	bool is_any_adapter_conn;

	if (QDF_GLOBAL_FTM_MODE == cds_get_conparam())
		return;

	if (!dp_ctx || !soc)
		return;

	ctx = dp_ctx->dp_ops.callback_ctx;
	is_any_adapter_conn = dp_ctx->dp_ops.dp_any_adapter_connected(ctx);

	if (!qdf_periodic_work_stop_sync(&dp_ctx->bus_bw_work))
		goto exit;

	ucfg_ipa_set_perf_level(dp_ctx->pdev, 0, 0);

	dp_reset_tcp_delack(psoc);

	if (!is_any_adapter_conn)
		dp_reset_tcp_adv_win_scale(dp_ctx);

	cdp_pdev_reset_driver_del_ack(cds_get_context(QDF_MODULE_ID_SOC),
				      OL_TXRX_PDEV_ID);
	cdp_pdev_reset_bundle_require_flag(cds_get_context(QDF_MODULE_ID_SOC),
					   OL_TXRX_PDEV_ID);

	cdp_set_bus_vote_lvl_high(soc, false);
	dp_ctx->bw_vote_time = 0;

exit:
	/**
	 * This check if for the case where the bus bw timer is forcibly
	 * stopped. We should remove the bus bw voting, if no adapter is
	 * connected
	 */
	if (!is_any_adapter_conn) {
		uint64_t interval_us =
			dp_ctx->dp_cfg.bus_bw_compute_interval * 1000;
		qdf_atomic_set(&dp_ctx->num_latency_critical_clients, 0);
		dp_pld_request_bus_bandwidth(dp_ctx, 0, 0, interval_us);
	}
	param.policy = BBM_TPUT_POLICY;
	param.policy_info.tput_level = TPUT_LEVEL_NONE;
	dp_bbm_apply_independent_policy(psoc, &param);
}

void dp_bus_bw_compute_timer_stop(struct wlan_objmgr_psoc *psoc)
{
	dp_enter();

	__dp_bus_bw_compute_timer_stop(psoc);

	dp_exit();
}

void dp_bus_bw_compute_timer_try_stop(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);
	hdd_cb_handle ctx;

	dp_enter();

	if (!dp_ctx) {
		dp_err("Unable to get DP context");
		return;
	}

	ctx = dp_ctx->dp_ops.callback_ctx;

	if (!dp_ctx->dp_ops.dp_any_adapter_connected(ctx))
		__dp_bus_bw_compute_timer_stop(psoc);

	dp_exit();
}

void dp_bus_bw_compute_prev_txrx_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_link) {
		dp_err("No dp_link for objmgr vdev %pK", vdev);
		return;
	}

	dp_intf = dp_link->dp_intf;
	if (!dp_intf) {
		dp_err("Invalid dp_intf for dp_link %pK (" QDF_MAC_ADDR_FMT ")",
		       dp_link, QDF_MAC_ADDR_REF(dp_link->mac_addr.bytes));
		return;
	}

	if (QDF_GLOBAL_FTM_MODE == cds_get_conparam())
		return;

	qdf_spin_lock_bh(&dp_ctx->bus_bw_lock);
	dp_intf->prev_tx_packets = qdf_net_stats_get_tx_pkts(&dp_intf->stats);
	dp_intf->prev_rx_packets = qdf_net_stats_get_rx_pkts(&dp_intf->stats);
	dp_intf->prev_tx_bytes = qdf_net_stats_get_tx_bytes(&dp_intf->stats);

	/*
	 * TODO - Should the prev_fwd_tx_packets and
	 * such stats be per link ??
	 */
	cdp_get_intra_bss_fwd_pkts_count(cds_get_context(QDF_MODULE_ID_SOC),
					 dp_link->link_id,
					 &dp_intf->prev_fwd_tx_packets,
					 &dp_intf->prev_fwd_rx_packets);
	qdf_spin_unlock_bh(&dp_ctx->bus_bw_lock);
}

void dp_bus_bw_compute_reset_prev_txrx_stats(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct wlan_dp_link *dp_link = dp_get_vdev_priv_obj(vdev);
	struct wlan_dp_intf *dp_intf;
	struct wlan_dp_psoc_context *dp_ctx = dp_psoc_get_priv(psoc);

	if (!dp_link) {
		dp_err("No dp_link for objmgr vdev %pK", vdev);
		return;
	}

	dp_intf = dp_link->dp_intf;
	if (!dp_intf) {
		dp_err("Invalid dp_intf for dp_link %pK (" QDF_MAC_ADDR_FMT ")",
		       dp_link, QDF_MAC_ADDR_REF(dp_link->mac_addr.bytes));
		return;
	}

	if (QDF_GLOBAL_FTM_MODE == cds_get_conparam())
		return;

	qdf_spin_lock_bh(&dp_ctx->bus_bw_lock);
	dp_intf->prev_tx_packets = 0;
	dp_intf->prev_rx_packets = 0;
	dp_intf->prev_fwd_tx_packets = 0;
	dp_intf->prev_fwd_rx_packets = 0;
	dp_intf->prev_tx_bytes = 0;
	qdf_spin_unlock_bh(&dp_ctx->bus_bw_lock);
}
#endif /* WLAN_FEATURE_DP_BUS_BANDWIDTH */
