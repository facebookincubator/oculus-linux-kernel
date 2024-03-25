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

/*===========================================================================

			s a p F s m . C

   OVERVIEW:

   This software unit holds the implementation of the WLAN SAP Finite
   State Machine modules

   DEPENDENCIES:

   Are listed for each API below.
   ===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "sap_internal.h"
#include <wlan_dfs_tgt_api.h>
#include <wlan_dfs_utils_api.h>
#include <wlan_dfs_public_struct.h>
#include <wlan_reg_services_api.h>
/* Pick up the SME API definitions */
#include "sme_api.h"
/* Pick up the PMC API definitions */
#include "cds_utils.h"
#include "cds_ieee80211_common_i.h"
#include "cds_reg_service.h"
#include "qdf_util.h"
#include "wlan_policy_mgr_api.h"
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_utility.h>
#include <linux/netdevice.h>
#include <net/cfg80211.h>
#include <qca_vendor.h>
#include <wlan_scan_api.h>
#include "wlan_reg_services_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_policy_mgr_ucfg.h"
#include "cfg_ucfg_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "wlan_vdev_mgr_utils_api.h"
#include "wlan_pre_cac_api.h"
#include <wlan_cmn_ieee80211.h>
#include <target_if.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *  External declarations for global context
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/
#ifdef SOFTAP_CHANNEL_RANGE
static QDF_STATUS sap_get_freq_list(struct sap_context *sap_ctx,
				    uint32_t **freq_list,
				    uint8_t *num_ch);
#endif

/*==========================================================================
   FUNCTION    sapStopDfsCacTimer

   DESCRIPTION
    Function to sttop the DFS CAC timer when SAP is stopped
   DEPENDENCIES
    NA.

   PARAMETERS

    IN
    sap_ctx: SAP Context
   RETURN VALUE
    DFS Timer start status
   SIDE EFFECTS
   ============================================================================*/

static int sap_stop_dfs_cac_timer(struct sap_context *sap_ctx);

/*==========================================================================
   FUNCTION    sapStartDfsCacTimer

   DESCRIPTION
    Function to start the DFS CAC timer when SAP is started on DFS Channel
   DEPENDENCIES
    NA.

   PARAMETERS

    IN
    sap_ctx: SAP Context
   RETURN VALUE
    DFS Timer start status
   SIDE EFFECTS
   ============================================================================*/

static int sap_start_dfs_cac_timer(struct sap_context *sap_ctx);

/** sap_hdd_event_to_string() - convert hdd event to string
 * @event: eSapHddEvent event type
 *
 * This function converts eSapHddEvent into string
 *
 * Return: string for the @event.
 */
static uint8_t *sap_hdd_event_to_string(eSapHddEvent event)
{
	switch (event) {
	CASE_RETURN_STRING(eSAP_START_BSS_EVENT);
	CASE_RETURN_STRING(eSAP_STOP_BSS_EVENT);
	CASE_RETURN_STRING(eSAP_STA_ASSOC_IND);
	CASE_RETURN_STRING(eSAP_STA_ASSOC_EVENT);
	CASE_RETURN_STRING(eSAP_STA_REASSOC_EVENT);
	CASE_RETURN_STRING(eSAP_STA_DISASSOC_EVENT);
	CASE_RETURN_STRING(eSAP_STA_SET_KEY_EVENT);
	CASE_RETURN_STRING(eSAP_STA_MIC_FAILURE_EVENT);
	CASE_RETURN_STRING(eSAP_WPS_PBC_PROBE_REQ_EVENT);
	CASE_RETURN_STRING(eSAP_DISCONNECT_ALL_P2P_CLIENT);
	CASE_RETURN_STRING(eSAP_MAC_TRIG_STOP_BSS_EVENT);
	CASE_RETURN_STRING(eSAP_UNKNOWN_STA_JOIN);
	CASE_RETURN_STRING(eSAP_MAX_ASSOC_EXCEEDED);
	CASE_RETURN_STRING(eSAP_CHANNEL_CHANGE_EVENT);
	CASE_RETURN_STRING(eSAP_DFS_CAC_START);
	CASE_RETURN_STRING(eSAP_DFS_CAC_INTERRUPTED);
	CASE_RETURN_STRING(eSAP_DFS_CAC_END);
	CASE_RETURN_STRING(eSAP_DFS_RADAR_DETECT);
	CASE_RETURN_STRING(eSAP_DFS_NO_AVAILABLE_CHANNEL);
#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	CASE_RETURN_STRING(eSAP_ACS_SCAN_SUCCESS_EVENT);
#endif
	CASE_RETURN_STRING(eSAP_ACS_CHANNEL_SELECTED);
	CASE_RETURN_STRING(eSAP_ECSA_CHANGE_CHAN_IND);
	default:
		return "eSAP_HDD_EVENT_UNKNOWN";
	}
}

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

#ifdef DFS_COMPONENT_ENABLE
/**
 * sap_random_channel_sel() - This function randomly pick up an available
 * channel
 * @sap_ctx: sap context.
 *
 * This function first eliminates invalid channel, then selects random channel
 * using following algorithm:
 *
 * Return: channel frequency picked
 */
static qdf_freq_t sap_random_channel_sel(struct sap_context *sap_ctx)
{
	uint16_t chan_freq;
	uint8_t ch_wd;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct ch_params *ch_params;
	uint32_t hw_mode, flag  = 0;
	struct mac_context *mac_ctx;
	struct dfs_acs_info acs_info = {0};

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return 0;
	}

	pdev = mac_ctx->pdev;
	if (!pdev) {
		sap_err("null pdev");
		return 0;
	}

	ch_params = &mac_ctx->sap.SapDfsInfo.new_ch_params;
	if (mac_ctx->sap.SapDfsInfo.orig_chanWidth == 0) {
		ch_wd = sap_ctx->ch_width_orig;
		mac_ctx->sap.SapDfsInfo.orig_chanWidth = ch_wd;
	} else {
		ch_wd = mac_ctx->sap.SapDfsInfo.orig_chanWidth;
	}

	ch_params->ch_width = ch_wd;
	if (sap_ctx->acs_cfg) {
		acs_info.acs_mode = sap_ctx->acs_cfg->acs_mode;
		acs_info.chan_freq_list = sap_ctx->acs_cfg->master_freq_list;
		acs_info.num_of_channel =
					sap_ctx->acs_cfg->master_ch_list_count;
	} else {
		acs_info.acs_mode = false;
	}

	if (mac_ctx->mlme_cfg->dfs_cfg.dfs_prefer_non_dfs)
		flag |= DFS_RANDOM_CH_FLAG_NO_DFS_CH;
	if (mac_ctx->mlme_cfg->dfs_cfg.dfs_disable_japan_w53)
		flag |= DFS_RANDOM_CH_FLAG_NO_JAPAN_W53_CH;
	if (mac_ctx->sap.SapDfsInfo.sap_operating_chan_preferred_location
	    == 1)
		flag |= DFS_RANDOM_CH_FLAG_NO_UPEER_5G_CH;
	else if (mac_ctx->sap.SapDfsInfo.
		 sap_operating_chan_preferred_location == 2)
		flag |= DFS_RANDOM_CH_FLAG_NO_LOWER_5G_CH;

	/*
	 * Dont choose 6ghz channel currently as legacy clients won't be able to
	 * scan them. In future create an ini if any customer wants 6ghz freq
	 * to be prioritize over 5ghz/2.4ghz.
	 * Currently for SAP there is a high chance of 6ghz being selected as
	 * an op frequency as PCL will have only 5, 6ghz freq as preferred for
	 * standalone SAP, and 6ghz channels being high in number.
	 */
	flag |= DFS_RANDOM_CH_FLAG_NO_6GHZ_CH;

	if (sap_ctx->candidate_freq &&
	    sap_ctx->chan_freq != sap_ctx->candidate_freq &&
	    !utils_dfs_is_freq_in_nol(pdev, sap_ctx->candidate_freq)) {
		chan_freq = sap_ctx->candidate_freq;
		if (sap_phymode_is_eht(sap_ctx->phyMode))
			wlan_reg_set_create_punc_bitmap(ch_params, true);
		wlan_reg_set_channel_params_for_pwrmode(pdev, chan_freq, 0,
							ch_params,
							REG_CURRENT_PWR_MODE);
		sap_debug("random chan select candidate freq=%d", chan_freq);
		sap_ctx->candidate_freq = 0;
	} else if (QDF_IS_STATUS_ERROR(
				utils_dfs_get_vdev_random_channel_for_freq(
					pdev, sap_ctx->vdev, flag, ch_params,
					&hw_mode, &chan_freq, &acs_info))) {
		/* No available channel found */
		sap_err("No available channel found!!!");
		sap_signal_hdd_event(sap_ctx, NULL,
				     eSAP_DFS_NO_AVAILABLE_CHANNEL,
				     (void *)eSAP_STATUS_SUCCESS);
		return 0;
	}
	mac_ctx->sap.SapDfsInfo.new_chanWidth = ch_params->ch_width;
	sap_ctx->ch_params.ch_width = ch_params->ch_width;
	sap_ctx->ch_params.sec_ch_offset = ch_params->sec_ch_offset;
	sap_ctx->ch_params.center_freq_seg0 = ch_params->center_freq_seg0;
	sap_ctx->ch_params.center_freq_seg1 = ch_params->center_freq_seg1;
	return chan_freq;
}
#else
static uint8_t sap_random_channel_sel(struct sap_context *sap_ctx)
{
	return 0;
}
#endif

/**
 * sap_is_channel_bonding_etsi_weather_channel() - check weather chan bonding.
 * @sap_ctx: sap context.
 * @chan_freq: chan frequency
 * @ch_params: pointer to ch_params
 *
 * Check if given channel and channel params are bonded to weather radar
 * channel in ETSI domain.
 *
 * Return: True if bonded to weather channel in ETSI
 */
static bool
sap_is_channel_bonding_etsi_weather_channel(struct sap_context *sap_ctx,
					    qdf_freq_t chan_freq,
					    struct ch_params *ch_params)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(sap_ctx->vdev);

	if (IS_CH_BONDING_WITH_WEATHER_CH(wlan_reg_freq_to_chan(pdev,
								chan_freq)) &&
	    ch_params->ch_width != CH_WIDTH_20MHZ)
		return true;

	return false;
}

/*
 * sap_get_bonding_channels() - get bonding channels from primary channel.
 * @sap_ctx: Handle to SAP context.
 * @chan_freq: Channel frequency to get bonded channels.
 * @freq_list: Bonded channel frequency list
 * @size: Max bonded channels
 * @chanBondState: The channel bonding mode of the passed channel.
 *
 * Return: Number of sub channels
 */
static uint8_t sap_get_bonding_channels(struct sap_context *sap_ctx,
					qdf_freq_t chan_freq,
					qdf_freq_t *freq_list, uint8_t size,
					ePhyChanBondState chanBondState)
{
	uint8_t num_freq;

	if (!freq_list)
		return 0;

	if (size < MAX_BONDED_CHANNELS)
		return 0;

	switch (chanBondState) {
	case PHY_SINGLE_CHANNEL_CENTERED:
		num_freq = 1;
		freq_list[0] = chan_freq;
		break;
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		num_freq = 2;
		freq_list[0] = chan_freq - 20;
		freq_list[1] = chan_freq;
		break;
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		num_freq = 2;
		freq_list[0] = chan_freq;
		freq_list[1] = chan_freq + 20;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		num_freq = 4;
		freq_list[0] = chan_freq;
		freq_list[1] = chan_freq + 20;
		freq_list[2] = chan_freq + 40;
		freq_list[3] = chan_freq + 60;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		num_freq = 4;
		freq_list[0] = chan_freq - 20;
		freq_list[1] = chan_freq;
		freq_list[2] = chan_freq + 20;
		freq_list[3] = chan_freq + 40;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		num_freq = 4;
		freq_list[0] = chan_freq - 40;
		freq_list[1] = chan_freq - 20;
		freq_list[2] = chan_freq;
		freq_list[3] = chan_freq + 20;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		num_freq = 4;
		freq_list[0] = chan_freq - 60;
		freq_list[1] = chan_freq - 40;
		freq_list[2] = chan_freq - 20;
		freq_list[3] = chan_freq;
		break;
	default:
		num_freq = 1;
		freq_list[0] = chan_freq;
		break;
	}

	return num_freq;
}

/**
 * sap_ch_params_to_bonding_channels() - get bonding channels from channel param
 * @ch_params: channel params ( bw, pri and sec channel info)
 * @freq_list: bonded channel frequency list
 *
 * Return: Number of sub channel frequencies
 */
static uint8_t sap_ch_params_to_bonding_channels(
		struct ch_params *ch_params,
		qdf_freq_t *freq_list)
{
	qdf_freq_t center_freq = ch_params->mhz_freq_seg0;
	uint8_t num_freq = 0;

	switch (ch_params->ch_width) {
	case CH_WIDTH_160MHZ:
		num_freq = 8;
		center_freq = ch_params->mhz_freq_seg1;
		freq_list[0] = center_freq - 70;
		freq_list[1] = center_freq - 50;
		freq_list[2] = center_freq - 30;
		freq_list[3] = center_freq - 10;
		freq_list[4] = center_freq + 10;
		freq_list[5] = center_freq + 30;
		freq_list[6] = center_freq + 50;
		freq_list[7] = center_freq + 70;
		break;
	case CH_WIDTH_80P80MHZ:
		num_freq = 8;
		freq_list[0] = center_freq - 30;
		freq_list[1] = center_freq - 10;
		freq_list[2] = center_freq + 10;
		freq_list[3] = center_freq + 30;

		center_freq = ch_params->mhz_freq_seg1;
		freq_list[4] = center_freq - 30;
		freq_list[5] = center_freq - 10;
		freq_list[6] = center_freq + 10;
		freq_list[7] = center_freq + 30;
		break;
	case CH_WIDTH_80MHZ:
		num_freq = 4;
		freq_list[0] = center_freq - 30;
		freq_list[1] = center_freq - 10;
		freq_list[2] = center_freq + 10;
		freq_list[3] = center_freq + 30;
		break;
	case CH_WIDTH_40MHZ:
		num_freq = 2;
		freq_list[0] = center_freq - 10;
		freq_list[1] = center_freq + 10;
		break;
	default:
		num_freq = 1;
		freq_list[0] = center_freq;
		break;
	}

	return num_freq;
}

/**
 * sap_operating_on_dfs() - check current sap operating on dfs
 * @mac_ctx: mac ctx
 * @sap_ctx: SAP context
 *
 * Return: true if any sub channel is dfs channel
 */
static
bool sap_operating_on_dfs(struct mac_context *mac_ctx,
			  struct sap_context *sap_ctx)
{
	struct wlan_channel *chan;

	if (!sap_ctx->vdev) {
		sap_debug("vdev invalid");
		return false;
	}

	chan = wlan_vdev_get_active_channel(sap_ctx->vdev);
	if (!chan) {
		sap_debug("Couldn't get vdev active channel");
		return false;
	}

	if (chan->ch_flagext & (IEEE80211_CHAN_DFS |
				IEEE80211_CHAN_DFS_CFREQ2))
		return true;

	return false;
}

/**
 * is_wlansap_cac_required_for_chan() - Is cac required for given channel
 * @mac_ctx: mac ctx
 * @sap_ctx: sap context
 * @chan_freq: given channel
 * @ch_params: pointer to ch_params
 *
 * Return: True if cac is required for given channel
 */
static bool
is_wlansap_cac_required_for_chan(struct mac_context *mac_ctx,
				 struct sap_context *sap_ctx,
				 qdf_freq_t chan_freq,
				 struct ch_params *ch_params)
{
	bool is_ch_dfs = false;
	bool cac_required;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t sta_cnt, i;

	if (ch_params->ch_width == CH_WIDTH_160MHZ) {
		wlan_reg_set_create_punc_bitmap(ch_params, true);
		if (wlan_reg_get_5g_bonded_channel_state_for_pwrmode(mac_ctx->pdev,
								     chan_freq,
								     ch_params,
								     REG_CURRENT_PWR_MODE) ==
		    CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	} else if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
		if (wlan_reg_get_channel_state_for_pwrmode(
						mac_ctx->pdev,
						chan_freq,
						REG_CURRENT_PWR_MODE) ==
		    CHANNEL_STATE_DFS ||
		    wlan_reg_get_channel_state_for_pwrmode(
					mac_ctx->pdev,
					ch_params->mhz_freq_seg1,
					REG_CURRENT_PWR_MODE) ==
				CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	} else {
		/* Indoor channels are also marked DFS, therefore
		 * check if the channel has REGULATORY_CHAN_RADAR
		 * channel flag to identify if the channel is DFS
		 */
		if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev, chan_freq))
			is_ch_dfs = true;
	}
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq))
		is_ch_dfs = false;

	sap_debug("vdev id %d chan %d is_ch_dfs %d pre_cac_complete %d ignore_cac %d cac_state %d",
		  sap_ctx->sessionId, chan_freq, is_ch_dfs,
		  wlan_pre_cac_complete_get(sap_ctx->vdev),
		  mac_ctx->sap.SapDfsInfo.ignore_cac,
		  mac_ctx->sap.SapDfsInfo.cac_state);

	if (!is_ch_dfs || wlan_pre_cac_complete_get(sap_ctx->vdev) ||
	    mac_ctx->sap.SapDfsInfo.ignore_cac ||
	    mac_ctx->sap.SapDfsInfo.cac_state == eSAP_DFS_SKIP_CAC)
		cac_required = false;
	else
		cac_required = true;

	if (cac_required) {
		sta_cnt =
		  policy_mgr_get_mode_specific_conn_info(mac_ctx->psoc,
							 freq_list,
							 vdev_id_list,
							 PM_STA_MODE);

		for (i = 0; i < sta_cnt; i++) {
			if (chan_freq == freq_list[i]) {
				sap_debug("STA vdev id %d exists, ignore CAC",
					  vdev_id_list[i]);
				cac_required = false;
			}
		}
	}

	return cac_required;
}

void sap_get_cac_dur_dfs_region(struct sap_context *sap_ctx,
				uint32_t *cac_duration_ms,
				uint32_t *dfs_region,
				qdf_freq_t chan_freq,
				struct ch_params *ch_params)
{
	int i;
	qdf_freq_t freq_list[MAX_BONDED_CHANNELS];
	uint8_t num_freq;
	struct mac_context *mac;
	bool cac_required;

	*cac_duration_ms = 0;
	if (!sap_ctx) {
		sap_err("null sap_ctx");
		return;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return;
	}

	wlan_reg_get_dfs_region(mac->pdev, dfs_region);
	cac_required = is_wlansap_cac_required_for_chan(mac, sap_ctx,
							chan_freq, ch_params);

	if (!cac_required) {
		sap_debug("cac is not required");
		return;
	}
	*cac_duration_ms = DEFAULT_CAC_TIMEOUT;

	if (*dfs_region != DFS_ETSI_REGION) {
		sap_debug("sapdfs: default cac duration");
		return;
	}

	if (sap_is_channel_bonding_etsi_weather_channel(sap_ctx, chan_freq,
							ch_params)) {
		*cac_duration_ms = ETSI_WEATHER_CH_CAC_TIMEOUT;
		sap_debug("sapdfs: bonding_etsi_weather_channel");
		return;
	}

	qdf_mem_zero(freq_list, sizeof(freq_list));
	num_freq = sap_ch_params_to_bonding_channels(ch_params, freq_list);
	for (i = 0; i < num_freq; i++) {
		if (IS_ETSI_WEATHER_FREQ(freq_list[i])) {
			*cac_duration_ms = ETSI_WEATHER_CH_CAC_TIMEOUT;
			sap_debug("sapdfs: ch freq=%d is etsi weather channel",
				  freq_list[i]);
			return;
		}
	}

}

void sap_dfs_set_current_channel(void *ctx)
{
	struct sap_context *sap_ctx = ctx;
	uint8_t vht_seg0 = sap_ctx->ch_params.center_freq_seg0;
	uint8_t vht_seg1 = sap_ctx->ch_params.center_freq_seg1;
	struct wlan_objmgr_pdev *pdev;
	struct mac_context *mac_ctx;
	uint32_t use_nol = 0;
	int error;
	bool is_dfs;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return;
	}

	pdev = mac_ctx->pdev;
	if (!pdev) {
		sap_err("null pdev");
		return;
	}

	is_dfs = wlan_mlme_check_chan_param_has_dfs(pdev,
						    &sap_ctx->ch_params,
						    sap_ctx->chan_freq);

	sap_debug("freq=%d, dfs %d seg0=%d, seg1=%d, bw %d",
		  sap_ctx->chan_freq, is_dfs, vht_seg0, vht_seg1,
		  sap_ctx->ch_params.ch_width);

	if (is_dfs) {
		if (policy_mgr_concurrent_beaconing_sessions_running(
		    mac_ctx->psoc)) {
			uint16_t con_ch_freq;
			mac_handle_t handle = MAC_HANDLE(mac_ctx);

			con_ch_freq =
				sme_get_beaconing_concurrent_operation_channel(
					handle, sap_ctx->sessionId);
			if (!con_ch_freq ||
			    !wlan_reg_is_dfs_for_freq(pdev,
							con_ch_freq))
				tgt_dfs_get_radars(pdev);
		} else {
			tgt_dfs_get_radars(pdev);
		}
		tgt_dfs_set_phyerr_filter_offload(pdev);

		if (mac_ctx->mlme_cfg->dfs_cfg.dfs_disable_channel_switch)
			tgt_dfs_control(pdev, DFS_SET_USENOL, &use_nol,
					sizeof(uint32_t), NULL, NULL, &error);
	}
}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * sap_check_in_avoid_ch_list() - checks if given channel present is channel
 * avoidance list
 *
 * @sap_ctx:        sap context.
 * @channel:        channel to be checked in sap_ctx's avoid ch list
 *
 * sap_ctx contains sap_avoid_ch_info strcut containing the list of channels on
 * which MDM device's AP with MCC was detected. This function checks if given
 * channel is present in that list.
 *
 * Return: true, if channel was present, false othersie.
 */
bool sap_check_in_avoid_ch_list(struct sap_context *sap_ctx, uint8_t channel)
{
	uint8_t i = 0;
	struct sap_avoid_channels_info *ie_info =
		&sap_ctx->sap_detected_avoid_ch_ie;
	for (i = 0; i < sizeof(ie_info->channels); i++)
		if (ie_info->channels[i] == channel)
			return true;
	return false;
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/**
 * sap_dfs_is_channel_in_nol_list() - given bonded channel is available
 * @sap_context: Handle to SAP context.
 * @channel_freq: Channel freq on which availability should be checked.
 * @chan_bondState: The channel bonding mode of the passed channel.
 *
 * This function Checks if a given bonded channel is available or
 * usable for DFS operation.
 *
 * Return: false if channel is available, true if channel is in NOL.
 */
bool
sap_dfs_is_channel_in_nol_list(struct sap_context *sap_context,
			       qdf_freq_t channel_freq,
			       ePhyChanBondState chan_bondState)
{
	int i;
	struct mac_context *mac_ctx;
	qdf_freq_t freq_list[MAX_BONDED_CHANNELS];
	uint8_t num_ch_freq;
	struct wlan_objmgr_pdev *pdev = NULL;
	enum channel_state ch_state;
	qdf_freq_t ch_freq;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return false;
	}

	pdev = mac_ctx->pdev;
	if (!pdev) {
		sap_err("null pdev");
		return false;
	}

	sap_context->ch_params.mhz_freq_seg0 =
			wlan_reg_legacy_chan_to_freq(
				pdev,
				sap_context->ch_params.center_freq_seg0);
	sap_context->ch_params.mhz_freq_seg1 =
			wlan_reg_legacy_chan_to_freq(
				pdev,
				sap_context->ch_params.center_freq_seg1);

	/* get the bonded channels */
	if (channel_freq == sap_context->chan_freq &&
	    chan_bondState >= PHY_CHANNEL_BONDING_STATE_MAX)
		num_ch_freq = sap_ch_params_to_bonding_channels(
					&sap_context->ch_params, freq_list);
	else
		num_ch_freq = sap_get_bonding_channels(
					sap_context, channel_freq,
					freq_list, MAX_BONDED_CHANNELS,
					chan_bondState);

	/* check for NOL, first on will break the loop */
	for (i = 0; i < num_ch_freq; i++) {
		ch_freq = freq_list[i];

		ch_state =
			wlan_reg_get_channel_state_from_secondary_list_for_freq(
								pdev, ch_freq);
		if (CHANNEL_STATE_ENABLE != ch_state &&
		    CHANNEL_STATE_DFS != ch_state) {
			sap_err_rl("Invalid ch freq = %d, ch state=%d", ch_freq,
				   ch_state);
			return true;
		}
	} /* loop for bonded channels */

	return false;
}

bool
sap_chan_bond_dfs_sub_chan(struct sap_context *sap_context,
			   qdf_freq_t channel_freq,
			   ePhyChanBondState bond_state)
{
	int i;
	struct mac_context *mac_ctx;
	qdf_freq_t freq_list[MAX_BONDED_CHANNELS];
	uint8_t num_freq;
	struct wlan_objmgr_pdev *pdev;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return false;
	}

	pdev = mac_ctx->pdev;
	if (!pdev) {
		sap_err("null pdev");
		return false;
	}

	if (wlan_reg_chan_has_dfs_attribute_for_freq(pdev, channel_freq))
		return true;

	/* get the bonded channels */
	if (channel_freq == sap_context->chan_freq &&
	    bond_state >= PHY_CHANNEL_BONDING_STATE_MAX)
		num_freq = sap_ch_params_to_bonding_channels(
					&sap_context->ch_params, freq_list);
	else
		num_freq = sap_get_bonding_channels(
					sap_context, channel_freq, freq_list,
					MAX_BONDED_CHANNELS, bond_state);

	for (i = 0; i < num_freq; i++) {
		if (wlan_reg_chan_has_dfs_attribute_for_freq(pdev, freq_list[i])) {
			sap_debug("sub ch freq=%d is dfs in %d",
				  freq_list[i], channel_freq);
			return true;
		}
	}

	return false;
}

uint32_t sap_select_default_oper_chan(struct mac_context *mac_ctx,
				      struct sap_acs_cfg *acs_cfg)
{
	uint16_t i;
	uint32_t freq0 = 0, freq1 = 0, freq2 = 0, default_freq;

	if (!acs_cfg)
		return 0;

	if (!acs_cfg->ch_list_count || !acs_cfg->freq_list) {
		if (mac_ctx->mlme_cfg->acs.force_sap_start) {
			sap_debug("SAP forced, freq selected %d",
				  acs_cfg->master_freq_list[0]);
			return acs_cfg->master_freq_list[0];
		} else {
			sap_debug("No channel left for operation");
			return 0;
		}
	}
	/*
	 * There could be both 2.4Ghz and 5ghz channels present in the list
	 * based upon the Hw mode received from hostapd, it is always better
	 * to chose a default 5ghz operating channel than 2.4ghz, as it can
	 * provide a better throughput, latency than 2.4ghz. Also 40 Mhz is
	 * rare in 2.4ghz band, so 5ghz should be preferred. If we get a 5Ghz
	 * chan in the acs cfg ch list , we should go for that first else the
	 * default channel can be 2.4ghz.
	 * Add check regulatory channel state before select the channel.
	 */

	for (i = 0; i < acs_cfg->ch_list_count; i++) {
		enum channel_state state =
			wlan_reg_get_channel_state_for_pwrmode(
					mac_ctx->pdev, acs_cfg->freq_list[i],
					REG_CURRENT_PWR_MODE);
		if (!freq0 && state == CHANNEL_STATE_ENABLE &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(acs_cfg->freq_list[i])) {
			freq0 = acs_cfg->freq_list[i];
			break;
		} else if (!freq1 && state == CHANNEL_STATE_DFS &&
			   WLAN_REG_IS_5GHZ_CH_FREQ(acs_cfg->freq_list[i])) {
			freq1 = acs_cfg->freq_list[i];
		} else if (!freq2 && state == CHANNEL_STATE_ENABLE) {
			freq2 = acs_cfg->freq_list[i];
		}
	}
	default_freq = freq0;
	if (!default_freq)
		default_freq = freq1;
	if (!default_freq)
		default_freq = freq2;
	if (!default_freq)
		default_freq = acs_cfg->freq_list[0];

	sap_debug("default freq %d chosen from %d %d %d %d", default_freq,
		  freq0, freq1, freq2, acs_cfg->freq_list[0]);

	return default_freq;
}

static bool is_mcc_preferred(struct sap_context *sap_context,
			     uint32_t con_ch_freq)
{
	/*
	 * If SAP ACS channel list is 1-11 and STA is on non-preferred
	 * channel i.e. 12, 13, 14 then MCC is unavoidable. This is because
	 * if SAP is started on 12,13,14 some clients may not be able to
	 * join dependending on their regulatory country.
	 */
	if ((con_ch_freq >= 2467) && (con_ch_freq <= 2484) &&
	    (sap_context->acs_cfg->start_ch_freq >= 2412 &&
	     sap_context->acs_cfg->end_ch_freq <= 2462)) {
		sap_debug("conc ch freq %d & sap acs ch list is 1-11, prefer mcc",
			  con_ch_freq);
		return true;
	}

	return false;
}

/**
 * sap_process_force_scc_with_go_start - Check GO force SCC or not
 * psoc: psoc object
 * sap_context: sap_context
 *
 * This function checks the current SAP MCC or not with the GO's home channel.
 * If it is, skip the GO's force SCC. The SAP will do force SCC after
 * GO's started.
 *
 * Return: true if skip GO's force SCC
 */
static bool
sap_process_force_scc_with_go_start(struct wlan_objmgr_psoc *psoc,
				    struct sap_context *sap_context)
{
	uint8_t existing_vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	enum policy_mgr_con_mode existing_vdev_mode = PM_MAX_NUM_OF_MODE;
	uint32_t con_freq;
	enum phy_ch_width ch_width;

	existing_vdev_id =
		policy_mgr_fetch_existing_con_info(psoc,
						   sap_context->sessionId,
						   sap_context->chan_freq,
						   &existing_vdev_mode,
						   &con_freq, &ch_width);
	if (existing_vdev_id < WLAN_UMAC_VDEV_ID_MAX &&
	    existing_vdev_mode == PM_SAP_MODE) {
		sap_debug("concurrent sap vdev: %d on freq %d, skip GO force scc",
			  existing_vdev_id, con_freq);
		return true;
	}

	return false;
}

#ifdef WLAN_FEATURE_P2P_P2P_STA
/**
 * sap_set_forcescc_required - set force scc flag for provided p2p go vdev
 *
 * vdev_id - vdev_id for which flag needs to be set
 *
 * Return: None
 */
static void sap_set_forcescc_required(uint8_t vdev_id)
{
	struct mac_context *mac_ctx;
	struct sap_context *sap_ctx;
	uint8_t i = 0;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return;
	}

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		sap_ctx = mac_ctx->sap.sapCtxList[i].sap_context;
		if (QDF_P2P_GO_MODE == mac_ctx->sap.sapCtxList[i].sapPersona &&
		    sap_ctx->sessionId == vdev_id) {
			sap_debug("update forcescc restart for vdev %d",
				  vdev_id);
			sap_ctx->is_forcescc_restart_required = true;
		}
	}
}

/**
 * sap_process_liberal_scc_for_go - based on existing connections this
 * function decides current go should start on provided channel or not and
 * sets force scc required bit for existing GO.
 *
 * sap_context: sap_context
 *
 * Return: bool
 */
static bool sap_process_liberal_scc_for_go(struct sap_context *sap_context)
{
	uint8_t existing_vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	enum policy_mgr_con_mode existing_vdev_mode = PM_MAX_NUM_OF_MODE;
	uint32_t con_freq;
	enum phy_ch_width ch_width;
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		sap_alert("invalid MAC handle");
		return true;
	}

	existing_vdev_id =
		policy_mgr_fetch_existing_con_info(
				mac_ctx->psoc,
				sap_context->sessionId,
				sap_context->chan_freq,
				&existing_vdev_mode,
				&con_freq, &ch_width);

	if (existing_vdev_id <
			WLAN_UMAC_VDEV_ID_MAX &&
			existing_vdev_mode == PM_P2P_GO_MODE) {
		sap_debug("set forcescc flag for go vdev: %d",
			  existing_vdev_id);
		sap_set_forcescc_required(
				existing_vdev_id);
		return true;
	}
	if (existing_vdev_id < WLAN_UMAC_VDEV_ID_MAX &&
	    (existing_vdev_mode == PM_STA_MODE ||
	    existing_vdev_mode == PM_P2P_CLIENT_MODE)) {
		sap_debug("don't override channel, start go on %d",
			  sap_context->chan_freq);
		return true;
	}

	return false;
}
#else
static bool sap_process_liberal_scc_for_go(struct sap_context *sap_context)
{
	return false;
}
#endif

QDF_STATUS
sap_validate_chan(struct sap_context *sap_context,
		  bool pre_start_bss,
		  bool check_for_connection_update)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;
	uint32_t con_ch_freq;
	bool sta_sap_scc_on_dfs_chan;
	uint32_t sta_go_bit_mask = QDF_STA_MASK | QDF_P2P_GO_MASK;
	uint32_t sta_sap_bit_mask = QDF_STA_MASK | QDF_SAP_MASK;
	uint32_t concurrent_state;
	bool go_force_scc;
	struct ch_params ch_params = {0};
	bool is_go_scc_strict = false;
	bool start_sap_on_provided_freq = false;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		/* we have a serious problem */
		sap_alert("invalid MAC handle");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_context->chan_freq) {
		sap_err("Invalid channel");
		return QDF_STATUS_E_FAILURE;
	}

	if (sap_context->vdev &&
	    sap_context->vdev->vdev_mlme.vdev_opmode == QDF_P2P_GO_MODE) {
	       /*
		* check whether go_force_scc is enabled or not.
		* If it not enabled then don't any force scc on existing go and
		* new p2p go vdevs.
		* Otherwise, if it is enabled then check whether it's in strict
		* mode or liberal mode.
		* For strict mode, do force scc on newly p2p go to existing vdev
		* channel.
		* For liberal first form new p2p go on requested channel and
		* follow below rules:
		* a.) If Existing vdev mode is P2P GO Once set key is done, do
		* force scc for existing p2p go and move that go to new p2p
		* go's channel.
		*
		* b.) If Existing vdev mode is P2P CLI/STA Once set key is
		* done, do force scc for p2p go and move go to cli/sta channel.
		*/
		go_force_scc = policy_mgr_go_scc_enforced(mac_ctx->psoc);
		sap_debug("go force scc enabled %d", go_force_scc);

		if (sap_process_force_scc_with_go_start(mac_ctx->psoc,
							sap_context))
			goto validation_done;

		if (go_force_scc) {
			is_go_scc_strict =
				policy_mgr_is_go_scc_strict(mac_ctx->psoc);
			if (!is_go_scc_strict) {
				sap_debug("liberal mode is enabled");
				start_sap_on_provided_freq =
				sap_process_liberal_scc_for_go(sap_context);
				if (start_sap_on_provided_freq)
					goto validation_done;
			}
		} else {
			goto validation_done;
		}
	}

	concurrent_state = policy_mgr_get_concurrency_mode(mac_ctx->psoc);
	if (policy_mgr_concurrent_beaconing_sessions_running(mac_ctx->psoc) ||
	    ((concurrent_state & sta_sap_bit_mask) == sta_sap_bit_mask) ||
	    ((concurrent_state & sta_go_bit_mask) == sta_go_bit_mask)) {
#ifdef FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE
		if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
					     sap_context->chan_freq)) {
			sap_warn("DFS not supported in STA_AP Mode");
			return QDF_STATUS_E_ABORTED;
		}
#endif
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		if (sap_context->cc_switch_mode !=
					QDF_MCC_TO_SCC_SWITCH_DISABLE) {
			con_ch_freq = sme_check_concurrent_channel_overlap(
					mac_handle,
					sap_context->chan_freq,
					sap_context->phyMode,
					sap_context->cc_switch_mode,
					sap_context->sessionId);
			sap_debug("After check overlap: sap freq %d con freq:%d",
				  sap_context->chan_freq, con_ch_freq);
			/*
			 * For non-DBS platform, a 2.4Ghz can become a 5Ghz freq
			 * so lets used max BW in that case, if it remain 2.4Ghz
			 * then BW will be limited to 20 anyway
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(sap_context->chan_freq))
				ch_params.ch_width = CH_WIDTH_MAX;
			else
				ch_params = sap_context->ch_params;

			if (sap_context->cc_switch_mode !=
		QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION) {
				if (QDF_IS_STATUS_ERROR(
					policy_mgr_valid_sap_conc_channel_check(
						mac_ctx->psoc, &con_ch_freq,
						sap_context->chan_freq,
						sap_context->sessionId,
						&ch_params))) {
					sap_warn("SAP can't start (no MCC)");
					return QDF_STATUS_E_ABORTED;
				}
			}
			/* if CH width didn't change fallback to original */
			if (ch_params.ch_width == CH_WIDTH_MAX)
				ch_params = sap_context->ch_params;

			sap_debug("After check concurrency: con freq:%d",
				  con_ch_freq);
			sta_sap_scc_on_dfs_chan =
				policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(
						mac_ctx->psoc);
			if (con_ch_freq &&
			    (policy_mgr_sta_sap_scc_on_lte_coex_chan(
						mac_ctx->psoc) ||
			     policy_mgr_is_safe_channel(
						mac_ctx->psoc, con_ch_freq)) &&
			    (!wlan_mlme_check_chan_param_has_dfs(
					mac_ctx->pdev, &ch_params,
					con_ch_freq) ||
			    sta_sap_scc_on_dfs_chan)) {
				if (is_mcc_preferred(sap_context, con_ch_freq))
					goto validation_done;

				sap_debug("Override ch freq %d (bw %d) to %d (bw %d) due to CC Intf",
					  sap_context->chan_freq,
					  sap_context->ch_params.ch_width,
					  con_ch_freq, ch_params.ch_width);
				sap_context->chan_freq = con_ch_freq;
				sap_context->ch_params = ch_params;
			}
		}
#endif
	}
validation_done:
	sap_debug("for configured channel, Ch_freq = %d",
		  sap_context->chan_freq);

	/*
	 * Don't check if the frequency is allowed or not if SAP is started
	 * in fixed channel and coex fixed channel SAP is supported.
	 */

	if ((sap_context->acs_cfg->acs_mode ||
	     !target_psoc_get_sap_coex_fixed_chan_cap(
			wlan_psoc_get_tgt_if_handle(mac_ctx->psoc))) &&
	    !policy_mgr_is_sap_freq_allowed(mac_ctx->psoc,
					    sap_context->chan_freq)) {
		sap_warn("Abort SAP start due to unsafe channel");
		return QDF_STATUS_E_ABORTED;
	}

	if (check_for_connection_update) {
		/* This wait happens in the hostapd context. The event
		 * is set in the MC thread context.
		 */
		qdf_status =
		policy_mgr_update_and_wait_for_connection_update(
			mac_ctx->psoc, sap_context->sessionId,
			sap_context->chan_freq,
			POLICY_MGR_UPDATE_REASON_START_AP);
		if (QDF_IS_STATUS_ERROR(qdf_status))
			return qdf_status;
	}

	if (pre_start_bss) {
		sap_info("ACS end due to Ch override. Sel Ch freq = %d",
			  sap_context->chan_freq);
		sap_context->acs_cfg->pri_ch_freq = sap_context->chan_freq;
		sap_context->acs_cfg->ch_width =
					 sap_context->ch_width_orig;
		sap_config_acs_result(mac_handle, sap_context, 0);
		return QDF_STATUS_E_CANCELED;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_SAP_ACS_OPTIMIZE

static void sap_sort_freq_list(struct chan_list *list,
			       uint8_t num_ch)
{
	int i, j, temp;

	for (i = 0; i < num_ch - 1; i++) {
		for (j = 0 ; j < num_ch - i - 1; j++) {
			if (list->chan[j].freq < list->chan[j + 1].freq) {
				temp = list->chan[j].freq;
				list->chan[j].freq = list->chan[j + 1].freq;
				list->chan[j + 1].freq = temp;
			}
		}
	}
}

/**
 * sap_acs_scan_freq_list_optimize - optimize the ACS scan freq list based
 * on when last scan was performed on particular frequency. If last scan
 * performed on particular frequency is less than configured last_scan_ageout
 * time, then skip that frequency from ACS scan freq list.
 *
 * sap_ctx: sap context
 * list: ACS scan frequency list
 * ch_count: number of frequency in list
 *
 * Return: None
 */
static void sap_acs_scan_freq_list_optimize(struct sap_context *sap_ctx,
					    struct chan_list *list,
					    uint8_t *ch_count)
{
	int loop_count = 0, j = 0;
	uint32_t ts_last_scan;

	sap_ctx->partial_acs_scan = false;

	while (loop_count < *ch_count) {
		ts_last_scan = scm_get_last_scan_time_per_channel(
				sap_ctx->vdev, list->chan[loop_count].freq);

		if (qdf_system_time_before(
		    qdf_get_time_of_the_day_ms(),
		    ts_last_scan + sap_ctx->acs_cfg->last_scan_ageout_time)) {
			sap_info("ACS chan %d skipped from scan as last scan ts %lu\n",
				 list->chan[loop_count].freq,
				 qdf_get_time_of_the_day_ms() - ts_last_scan);

			for (j = loop_count; j < *ch_count - 1; j++)
				list->chan[j].freq = list->chan[j + 1].freq;

			(*ch_count)--;
			sap_ctx->partial_acs_scan = true;
			continue;
		}
		loop_count++;
	}
	if (*ch_count == 0)
		sap_info("All ACS freq channels are scanned recently, skip ACS scan\n");
	else
		sap_sort_freq_list(list, *ch_count);
}
#else
static void sap_acs_scan_freq_list_optimize(struct sap_context *sap_ctx,
					    struct chan_list *list,
					    uint8_t *ch_count)
{
}
#endif

#ifdef WLAN_FEATURE_SAP_ACS_OPTIMIZE
/**
 * sap_reset_clean_freq_array(): clear freq array that contains info
 * channel is free or not
 * @sap_context: sap context
 *
 * Return: void
 */
static
void sap_reset_clean_freq_array(struct sap_context *sap_context)
{
	memset(sap_context->clean_channel_array, 0, NUM_CHANNELS);
}
#else
static inline
void sap_reset_clean_freq_array(struct sap_context *sap_context)
{}
#endif

QDF_STATUS sap_channel_sel(struct sap_context *sap_context)
{
	QDF_STATUS qdf_ret_status;
	struct mac_context *mac_ctx;
	struct scan_start_request *req;
	struct wlan_objmgr_vdev *vdev = NULL;
	uint8_t i, j;
	uint32_t *freq_list = NULL;
	uint8_t num_of_channels = 0;
	mac_handle_t mac_handle;
	uint32_t con_ch_freq;
	uint8_t vdev_id;
	uint32_t scan_id;
	uint32_t default_op_freq;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle)
		return QDF_STATUS_E_FAULT;

	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAILURE;
	}
	if (sap_context->fsm_state != SAP_STARTED && sap_context->chan_freq)
		return sap_validate_chan(sap_context, true, false);

	if (policy_mgr_concurrent_beaconing_sessions_running(mac_ctx->psoc) ||
	    ((sap_context->cc_switch_mode ==
	      QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION) &&
	     (policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
							PM_SAP_MODE, NULL) ||
	     policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
						       PM_P2P_GO_MODE,
						       NULL)))) {
		con_ch_freq = sme_get_beaconing_concurrent_operation_channel(
					mac_handle, sap_context->sessionId);
#ifdef FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE
		if (con_ch_freq)
			sap_context->dfs_ch_disable = true;
#endif
	}

	if ((policy_mgr_get_concurrency_mode(mac_ctx->psoc) ==
		(QDF_STA_MASK | QDF_SAP_MASK)) ||
		((sap_context->cc_switch_mode ==
		QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION) &&
		(policy_mgr_get_concurrency_mode(mac_ctx->psoc) ==
		(QDF_STA_MASK | QDF_P2P_GO_MASK)))) {
#ifdef FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE
		sap_context->dfs_ch_disable = true;
#endif
	}
#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	sap_debug("skip_acs_status = %d",
		  sap_context->acs_cfg->skip_scan_status);
	if (sap_context->acs_cfg->skip_scan_status !=
					eSAP_SKIP_ACS_SCAN) {
#endif

		if (sap_context->freq_list) {
			qdf_mem_free(sap_context->freq_list);
			sap_context->freq_list = NULL;
			sap_context->num_of_channel = 0;
		}

		sap_get_freq_list(sap_context, &freq_list, &num_of_channels);
		if (!num_of_channels || !freq_list) {
			sap_err("No freq sutiable for SAP in current list, SAP failed");
			return QDF_STATUS_E_FAILURE;
		}

		req = qdf_mem_malloc(sizeof(*req));
		if (!req) {
			qdf_mem_free(freq_list);
			return QDF_STATUS_E_NOMEM;
		}

		vdev_id = sap_context->sessionId;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							    vdev_id,
							    WLAN_LEGACY_SME_ID);
		if (!vdev) {
			sap_err("Invalid vdev objmgr");
			qdf_mem_free(freq_list);
			qdf_mem_free(req);
			return QDF_STATUS_E_INVAL;
		}

		/* Initiate a SCAN request */
		wlan_scan_init_default_params(vdev, req);
		scan_id = wlan_scan_get_scan_id(mac_ctx->psoc);
		req->scan_req.scan_id = scan_id;
		req->scan_req.vdev_id = vdev_id;
		req->scan_req.scan_f_passive = false;
		req->scan_req.scan_req_id = sap_context->req_id;
		req->scan_req.scan_priority = SCAN_PRIORITY_HIGH;
		req->scan_req.scan_f_bcast_probe = true;
		for (i = 0, j = 0; i < num_of_channels; i++) {
			if (wlan_reg_is_6ghz_chan_freq(freq_list[i]) &&
			    !wlan_reg_is_6ghz_psc_chan_freq(freq_list[i]))
				continue;
			req->scan_req.chan_list.chan[j++].freq = freq_list[i];
		}
		req->scan_req.chan_list.num_chan = j;
		sap_context->freq_list = freq_list;
		sap_context->num_of_channel = num_of_channels;
		sap_context->optimize_acs_chan_selected = false;
		sap_reset_clean_freq_array(sap_context);
		/* Set requestType to Full scan */

		/*
		 * send partial channels to be scanned in SCAN request if
		 * vendor command included last scan ageout time to be used to
		 * optimize the SAP bring up time
		 */
		if (sap_context->acs_cfg->last_scan_ageout_time)
			sap_acs_scan_freq_list_optimize(
					sap_context, &req->scan_req.chan_list,
					&req->scan_req.chan_list.num_chan);

		if (!req->scan_req.chan_list.num_chan) {
			sap_info("## SKIPPED ACS SCAN");
			sap_context->acs_cfg->skip_acs_scan = true;
			wlansap_pre_start_bss_acs_scan_callback(
				mac_handle, sap_context, sap_context->sessionId,
				0, eCSR_SCAN_SUCCESS);
			qdf_mem_free(req);
			qdf_ret_status = QDF_STATUS_SUCCESS;
			goto release_vdev_ref;
		}

		sap_context->acs_req_timestamp = qdf_get_time_of_the_day_ms();
		qdf_ret_status = wlan_scan_start(req);
		if (qdf_ret_status != QDF_STATUS_SUCCESS) {
			sap_err("scan request  fail %d!!!", qdf_ret_status);
			sap_info("SAP Configuring default ch, Ch_freq=%d",
				  sap_context->chan_freq);
			default_op_freq = sap_select_default_oper_chan(
						mac_ctx, sap_context->acs_cfg);
			wlansap_set_acs_ch_freq(sap_context, default_op_freq);

			if (sap_context->freq_list) {
				wlansap_set_acs_ch_freq(
					sap_context, sap_context->freq_list[0]);
				qdf_mem_free(sap_context->freq_list);
				sap_context->freq_list = NULL;
				sap_context->num_of_channel = 0;
			}
			/*
			* In case of ACS req before start Bss,
			* return failure so that the calling
			* function can use the default channel.
			*/
			qdf_ret_status = QDF_STATUS_E_FAILURE;
			goto release_vdev_ref;
		} else {
			wlansap_dump_acs_ch_freq(sap_context);
			host_log_acs_scan_start(scan_id, vdev_id);
		}

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	} else {
		sap_context->acs_cfg->skip_scan_status = eSAP_SKIP_ACS_SCAN;
	}

	if (sap_context->acs_cfg->skip_scan_status == eSAP_SKIP_ACS_SCAN) {
		sap_err("## SKIPPED ACS SCAN");
		wlansap_pre_start_bss_acs_scan_callback(mac_handle,
				sap_context, sap_context->sessionId, 0,
				eCSR_SCAN_SUCCESS);
	}
#endif

	wlansap_dump_acs_ch_freq(sap_context);

	qdf_ret_status = QDF_STATUS_SUCCESS;

release_vdev_ref:
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	return qdf_ret_status;
}

/**
 * sap_find_valid_concurrent_session() - to find valid concurrent session
 * @mac_handle: Opaque handle to the global MAC context
 *
 * This API will check if any valid concurrent SAP session is present
 *
 * Return: pointer to sap context of valid concurrent session
 */
static struct sap_context *
sap_find_valid_concurrent_session(mac_handle_t mac_handle)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t intf = 0;
	struct sap_context *sap_ctx;

	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		if (((QDF_SAP_MODE ==
				mac_ctx->sap.sapCtxList[intf].sapPersona) ||
		     (QDF_P2P_GO_MODE ==
				mac_ctx->sap.sapCtxList[intf].sapPersona)) &&
		    mac_ctx->sap.sapCtxList[intf].sap_context) {
			sap_ctx = mac_ctx->sap.sapCtxList[intf].sap_context;
			if (sap_ctx->fsm_state != SAP_INIT)
				return sap_ctx;
		}
	}

	return NULL;
}

QDF_STATUS sap_clear_global_dfs_param(mac_handle_t mac_handle,
				      struct sap_context *sap_ctx)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct sap_context *con_sap_ctx;

	con_sap_ctx = sap_find_valid_concurrent_session(mac_handle);
	if (con_sap_ctx && WLAN_REG_IS_5GHZ_CH_FREQ(con_sap_ctx->chan_freq)) {
		sap_debug("conc session exists, no need to clear dfs struct");
		return QDF_STATUS_SUCCESS;
	}
	/*
	 * CAC timer will be initiated and started only when SAP starts
	 * on DFS channel and it will be stopped and destroyed
	 * immediately once the radar detected or timedout. So
	 * as per design CAC timer should be destroyed after stop
	 */
	wlansap_cleanup_cac_timer(sap_ctx);
	mac_ctx->sap.SapDfsInfo.cac_state = eSAP_DFS_DO_NOT_SKIP_CAC;
	sap_cac_reset_notify(mac_handle);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_acquire_vdev_ref(struct wlan_objmgr_psoc *psoc,
				struct sap_context *sap_ctx,
				uint8_t session_id)
{
	struct wlan_objmgr_vdev *vdev;

	if (sap_ctx->vdev) {
		sap_err("Invalid vdev obj in sap context");
		return QDF_STATUS_E_FAULT;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, session_id,
						    WLAN_LEGACY_SAP_ID);
	if (!vdev) {
		sap_err("vdev is NULL for vdev_id: %u", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	sap_ctx->vdev = vdev;
	return QDF_STATUS_SUCCESS;
}

void sap_release_vdev_ref(struct sap_context *sap_ctx)
{
	struct wlan_objmgr_vdev *vdev;

	if (!sap_ctx) {
		sap_debug("Invalid SAP pointer");
		return;
	}

	vdev = sap_ctx->vdev;
	if (vdev) {
		sap_ctx->vdev = NULL;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SAP_ID);
	}
}

QDF_STATUS sap_set_session_param(mac_handle_t mac_handle,
				 struct sap_context *sapctx,
				 uint32_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	int i;

	sapctx->sessionId = session_id;
	wlan_pre_cac_set_status(sapctx->vdev, false);
	wlan_pre_cac_complete_set(sapctx->vdev, false);
	wlan_pre_cac_set_freq_before_pre_cac(sapctx->vdev, 0);

	/* When SSR, SAP will restart, clear the old context,sessionId */
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (mac_ctx->sap.sapCtxList[i].sap_context == sapctx)
			mac_ctx->sap.sapCtxList[i].sap_context = NULL;
	}

	mac_ctx->sap.sapCtxList[sapctx->sessionId].sap_context = sapctx;
	mac_ctx->sap.sapCtxList[sapctx->sessionId].sapPersona =
			wlan_get_opmode_vdev_id(mac_ctx->pdev, session_id);
	sap_debug("Initializing sap_ctx = %pK with session = %d",
		   sapctx, session_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_clear_session_param(mac_handle_t mac_handle,
				   struct sap_context *sapctx,
				   uint32_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (sapctx->sessionId >= SAP_MAX_NUM_SESSION)
		return QDF_STATUS_E_FAILURE;

	mac_ctx->sap.sapCtxList[sapctx->sessionId].sap_context = NULL;
	mac_ctx->sap.sapCtxList[sapctx->sessionId].sapPersona =
		QDF_MAX_NO_OF_MODE;
	sap_clear_global_dfs_param(mac_handle, sapctx);

	sap_err("Set sapCtxList null for session %d", sapctx->sessionId);
	qdf_mem_zero(sapctx, sizeof(*sapctx));
	sapctx->sessionId = WLAN_UMAC_VDEV_ID_MAX;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11AX
static uint16_t he_mcs_12_13_support(void)
{
	struct mac_context *mac_ctx;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return 0;
	}

	return mac_ctx->mlme_cfg->he_caps.he_mcs_12_13_supp_5g;
}
#else
static inline uint16_t he_mcs_12_13_support(void)
{
	return 0;
}
#endif

static bool is_mcs13_ch_width(enum phy_ch_width ch_width)
{
	if ((ch_width == CH_WIDTH_320MHZ) ||
	    (ch_width == CH_WIDTH_160MHZ) ||
	    (ch_width == CH_WIDTH_80P80MHZ))
		return true;

	return false;
}

/**
 * sap_update_mcs_rate() - Update SAP MCS rate
 * @sap_ctx: pointer to sap Context
 * @is_start: Start or stop SAP
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
sap_update_mcs_rate(struct sap_context *sap_ctx, bool is_start)
{
	uint32_t default_mcs[] = {26, 0x3fff};
	uint32_t fixed_mcs[] = {26, 0x1fff};
	bool disable_mcs13_support = false;
	uint16_t he_mcs_12_13_supp;
	struct mac_context *mac_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_INVAL;
	}

	he_mcs_12_13_supp = he_mcs_12_13_support();
	disable_mcs13_support = cfg_get(mac_ctx->psoc,
					CFG_DISABLE_MCS13_SUPPORT);
	sap_debug("session id %d, disable mcs13 support %d, he_mcs_12_13 %d, start %d, disabled_mcs13 %d, ch width %d",
		  sap_ctx->sessionId, disable_mcs13_support,
		  he_mcs_12_13_supp,
		  is_start, sap_ctx->disabled_mcs13,
		  sap_ctx->ch_params.ch_width);

	if (!disable_mcs13_support ||
	    !he_mcs_12_13_supp)
		return status;

	if (!is_start && !sap_ctx->disabled_mcs13)
		return status;

	if (!is_mcs13_ch_width(sap_ctx->ch_params.ch_width))
		return status;

	if (is_start) {
		status = sme_send_unit_test_cmd(sap_ctx->sessionId,
						10, 2, fixed_mcs);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("Set fixed mcs rate failed, session %d",
				sap_ctx->sessionId);
		} else {
			sap_ctx->disabled_mcs13 = true;
		}
	} else {
		status = sme_send_unit_test_cmd(sap_ctx->sessionId,
						10, 2, default_mcs);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("Set default mcs rate failed, session %d",
				sap_ctx->sessionId);
		} else {
			sap_ctx->disabled_mcs13 = false;
		}
	}

	return status;
}

/**
 * sap_goto_stopping() - Processing of SAP FSM stopping state
 * @sap_ctx: pointer to sap Context
 *
 * Return: QDF_STATUS code associated with performing the operation
 */
static QDF_STATUS sap_goto_stopping(struct sap_context *sap_ctx)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		/* we have a serious problem */
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	sap_update_mcs_rate(sap_ctx, false);
	qdf_mem_zero(&sap_ctx->sap_bss_cfg, sizeof(sap_ctx->sap_bss_cfg));

	status = sme_roam_stop_bss(MAC_HANDLE(mac_ctx), sap_ctx->sessionId);
	if (status != QDF_STATUS_SUCCESS) {
		sap_err("Calling sme_roam_stop_bss status = %d", status);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * sap_goto_init() - Function for setting the SAP FSM to init state
 * @sap_ctx: pointer to sap context
 *
 * Return: QDF_STATUS code associated with performing the operation
 */
static QDF_STATUS sap_goto_init(struct sap_context *sap_ctx)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct sap_sm_event sap_event;
	/* Processing has to be coded */

	/*
	 * Clean up stations from TL etc as AP BSS is shut down
	 * then set event
	 */

	/* hardcoded event */
	sap_event.event = eSAP_MAC_READY_FOR_CONNECTIONS;
	sap_event.params = 0;
	sap_event.u1 = 0;
	sap_event.u2 = 0;
	/* Handle event */
	qdf_status = sap_fsm(sap_ctx, &sap_event);

	return qdf_status;
}

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
/**
 * sap_handle_acs_scan_event() - handle acs scan event for SAP
 * @sap_context: ptSapContext
 * @sap_event: struct sap_event
 * @status: status of acs scan
 *
 * The function is to handle the eSAP_ACS_SCAN_SUCCESS_EVENT event.
 *
 * Return: void
 */
static void sap_handle_acs_scan_event(struct sap_context *sap_context,
		struct sap_event *sap_event, eSapStatus status)
{
	sap_event->sapHddEventCode = eSAP_ACS_SCAN_SUCCESS_EVENT;
	sap_event->sapevt.sap_acs_scan_comp.status = status;
	sap_event->sapevt.sap_acs_scan_comp.num_of_channels =
			sap_context->num_of_channel;
	sap_event->sapevt.sap_acs_scan_comp.freq_list =
			sap_context->freq_list;
}
#else
static void sap_handle_acs_scan_event(struct sap_context *sap_context,
		struct sap_event *sap_event, eSapStatus status)
{
}
#endif

#define DH_OUI_TYPE      "\x20"
#define DH_OUI_TYPE_SIZE (1)
/**
 * sap_fill_owe_ie_in_assoc_ind() - Fill OWE IE in assoc indication
 * Function to fill OWE IE in assoc indication
 * @assoc_ind: SAP STA association indication
 * @sme_assoc_ind: SME association indication
 * @reassoc: True if it is reassoc frame
 *
 * This function is to get OWE IEs (RSN IE, DH IE etc) from assoc request
 * and fill them in association indication.
 *
 * Return: true for success and false for failure
 */
static bool sap_fill_owe_ie_in_assoc_ind(tSap_StationAssocIndication *assoc_ind,
					 struct assoc_ind *sme_assoc_ind,
					 bool reassoc)
{
	uint32_t owe_ie_len, rsn_ie_len, dh_ie_len;
	const uint8_t *rsn_ie, *dh_ie;
	uint8_t *assoc_req_ie;
	uint16_t assoc_req_ie_len;

	if (reassoc) {
		if (assoc_ind->assocReqLength < WLAN_REASSOC_REQ_IES_OFFSET) {
			sap_err("Invalid reassoc req");
			return false;
		}

		assoc_req_ie = assoc_ind->assocReqPtr +
			       WLAN_REASSOC_REQ_IES_OFFSET;
		assoc_req_ie_len = assoc_ind->assocReqLength -
				   WLAN_REASSOC_REQ_IES_OFFSET;
	} else {
		if (assoc_ind->assocReqLength < WLAN_ASSOC_REQ_IES_OFFSET) {
			sap_err("Invalid assoc req");
			return false;
		}

		assoc_req_ie = assoc_ind->assocReqPtr +
			       WLAN_ASSOC_REQ_IES_OFFSET;
		assoc_req_ie_len = assoc_ind->assocReqLength -
				   WLAN_ASSOC_REQ_IES_OFFSET;
	}
	rsn_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_RSN,
					  assoc_req_ie, assoc_req_ie_len);
	if (!rsn_ie) {
		sap_err("RSN IE is not present");
		return false;
	}
	rsn_ie_len = rsn_ie[1] + 2;
	if (rsn_ie_len < DOT11F_IE_RSN_MIN_LEN ||
	    rsn_ie_len > DOT11F_IE_RSN_MAX_LEN) {
		sap_err("Invalid RSN IE len %d", rsn_ie_len);
		return false;
	}

	dh_ie = wlan_get_ext_ie_ptr_from_ext_id(DH_OUI_TYPE, DH_OUI_TYPE_SIZE,
						assoc_req_ie, assoc_req_ie_len);
	if (!dh_ie) {
		sap_err("DH IE is not present");
		return false;
	}
	dh_ie_len = dh_ie[1] + 2;
	if (dh_ie_len < DOT11F_IE_DH_PARAMETER_ELEMENT_MIN_LEN ||
	    dh_ie_len > DOT11F_IE_DH_PARAMETER_ELEMENT_MAX_LEN) {
		sap_err("Invalid DH IE len %d", dh_ie_len);
		return false;
	}

	sap_debug("rsn_ie_len = %d, dh_ie_len = %d", rsn_ie_len, dh_ie_len);

	owe_ie_len = rsn_ie_len + dh_ie_len;
	assoc_ind->owe_ie = qdf_mem_malloc(owe_ie_len);
	if (!assoc_ind->owe_ie)
		return false;

	qdf_mem_copy(assoc_ind->owe_ie, rsn_ie, rsn_ie_len);
	qdf_mem_copy(assoc_ind->owe_ie + rsn_ie_len, dh_ie, dh_ie_len);
	assoc_ind->owe_ie_len = owe_ie_len;

	return true;
}

/**
 * sap_save_owe_pending_assoc_ind() - Save pending assoc indication
 * Function to save pending assoc indication in SAP context
 * @sap_ctx: SAP context
 * @sme_assoc_ind: SME association indication
 *
 * This function is to save pending assoc indication in linked list
 * in SAP context.
 *
 * Return: true for success and false for failure
 */
static bool sap_save_owe_pending_assoc_ind(struct sap_context *sap_ctx,
				       struct assoc_ind *sme_assoc_ind)
{
	struct owe_assoc_ind *assoc_ind;
	QDF_STATUS status;

	assoc_ind = qdf_mem_malloc(sizeof(*assoc_ind));
	if (!assoc_ind)
		return false;
	assoc_ind->assoc_ind = sme_assoc_ind;
	status = qdf_list_insert_back(&sap_ctx->owe_pending_assoc_ind_list,
				      &assoc_ind->node);
	if (QDF_STATUS_SUCCESS != status) {
		qdf_mem_free(assoc_ind);
		return false;
	}

	return true;
}

static bool sap_save_ft_pending_assoc_ind(struct sap_context *sap_ctx,
					  struct assoc_ind *sme_assoc_ind)
{
	struct ft_assoc_ind *assoc_ind;
	QDF_STATUS status;

	assoc_ind = qdf_mem_malloc(sizeof(*assoc_ind));
	if (!assoc_ind)
		return false;
	assoc_ind->assoc_ind = sme_assoc_ind;
	status = qdf_list_insert_back(&sap_ctx->ft_pending_assoc_ind_list,
				      &assoc_ind->node);
	if (QDF_STATUS_SUCCESS != status) {
		qdf_mem_free(assoc_ind);
		return false;
	}
	qdf_event_set(&sap_ctx->ft_pending_event);

	return true;
}

#ifdef FEATURE_RADAR_HISTORY
/* Last cac result */
static struct prev_cac_result prev_cac_history;

/**
 * sap_update_cac_history() - record SAP Radar found result in last
 * "active" or CAC period
 * @mac_ctx: mac context
 * @sap_ctx: sap context
 * @event_id: sap event
 *
 *  The function is to save the dfs channel information
 *  If SAP has been "active" or "CAC" on DFS channel for 60s and
 *  no found radar event.
 *
 * Return: void
 */
static void
sap_update_cac_history(struct mac_context *mac_ctx,
		       struct sap_context *sap_ctx,
		       eSapHddEvent event_id)
{
	struct prev_cac_result *cac_result = &sap_ctx->cac_result;

	switch (event_id) {
	case eSAP_START_BSS_EVENT:
	case eSAP_CHANNEL_CHANGE_RESP:
	case eSAP_DFS_CAC_START:
		if (sap_operating_on_dfs(mac_ctx, sap_ctx)) {
			qdf_mem_zero(cac_result,
				     sizeof(struct prev_cac_result));
			if (!sap_ctx->ch_params.mhz_freq_seg0) {
				sap_debug("invalid seq0");
				return;
			}
			cac_result->ap_start_time =
				qdf_get_monotonic_boottime();
			cac_result->cac_ch_param = sap_ctx->ch_params;
			sap_debug("ap start(CAC) (%d, %d) bw %d",
				  cac_result->cac_ch_param.mhz_freq_seg0,
				  cac_result->cac_ch_param.mhz_freq_seg1,
				  cac_result->cac_ch_param.ch_width);
		}
		break;
	case eSAP_DFS_RADAR_DETECT:
		qdf_mem_zero(cac_result,
			     sizeof(struct prev_cac_result));
		break;
	case eSAP_DFS_CAC_END:
	case eSAP_STOP_BSS_EVENT:
		if (cac_result->ap_start_time) {
			uint64_t diff_ms;

			cac_result->ap_end_time =
				qdf_get_monotonic_boottime();
			diff_ms = qdf_do_div(cac_result->ap_end_time -
				     cac_result->ap_start_time, 1000);
			if (diff_ms < DEFAULT_CAC_TIMEOUT - 5000) {
				if (event_id == eSAP_STOP_BSS_EVENT)
					qdf_mem_zero(
					cac_result,
					sizeof(struct prev_cac_result));
				sap_debug("ap cac dur %llu ms", diff_ms);
				break;
			}
			cac_result->cac_complete = true;
			qdf_mem_copy(&prev_cac_history, cac_result,
				     sizeof(struct prev_cac_result));
			sap_debug("ap cac saved %llu ms %llu (%d, %d) bw %d",
				  diff_ms,
				  cac_result->ap_end_time,
				  cac_result->cac_ch_param.mhz_freq_seg0,
				  cac_result->cac_ch_param.mhz_freq_seg1,
				  cac_result->cac_ch_param.ch_width);
			if (event_id == eSAP_STOP_BSS_EVENT)
				qdf_mem_zero(cac_result,
					     sizeof(struct prev_cac_result));
		}
		break;
	default:
		break;
	}
}

/**
 * find_ch_freq_in_radar_hist() - check channel frequency existing
 * in radar history buffer
 * @radar_result: radar history buffer
 * @count: radar history element number
 * @ch_freq: channel frequency
 *
 * Return: bool
 */
static
bool find_ch_freq_in_radar_hist(struct dfs_radar_history *radar_result,
				uint32_t count, uint16_t ch_freq)
{
	while (count) {
		if (radar_result->ch_freq == ch_freq)
			return true;
		radar_result++;
		count--;
	}

	return false;
}

/**
 * sap_append_cac_history() - Add CAC history to list
 * @radar_result: radar history buffer
 * @idx: current radar history element number
 * @max_elems: max elements number of radar history buffer.
 *
 * This function is to add the CAC history to radar history list.
 *
 * Return: void
 */
static
void sap_append_cac_history(struct mac_context *mac_ctx,
			    struct dfs_radar_history *radar_result,
			    uint32_t *idx, uint32_t max_elems)
{
	struct prev_cac_result *cac_result = &prev_cac_history;
	struct ch_params ch_param = cac_result->cac_ch_param;
	uint32_t count = *idx;

	if (!cac_result->cac_complete || !cac_result->ap_end_time) {
		sap_debug("cac hist empty");
		return;
	}

	if (ch_param.ch_width <= CH_WIDTH_20MHZ) {
		if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
					     ch_param.mhz_freq_seg0) &&
		    !find_ch_freq_in_radar_hist(radar_result, count,
						ch_param.mhz_freq_seg0) &&
		    *idx < max_elems) {
			radar_result[*idx].ch_freq = ch_param.mhz_freq_seg0;
			radar_result[*idx].time = cac_result->ap_end_time;
			radar_result[*idx].radar_found = false;
			sap_debug("radar hist[%d] freq %d time %llu no radar",
				  *idx, ch_param.mhz_freq_seg0,
				  cac_result->ap_end_time);
			(*idx)++;
		}
	} else {
		uint16_t chan_cfreq;
		enum channel_state state;
		const struct bonded_channel_freq *bonded_chan_ptr = NULL;

		state = wlan_reg_get_5g_bonded_channel_and_state_for_freq
			(mac_ctx->pdev, ch_param.mhz_freq_seg0,
			 ch_param.ch_width, &bonded_chan_ptr);
		if (!bonded_chan_ptr || state == CHANNEL_STATE_INVALID) {
			sap_debug("invalid freq %d", ch_param.mhz_freq_seg0);
			return;
		}

		chan_cfreq = bonded_chan_ptr->start_freq;
		while (chan_cfreq <= bonded_chan_ptr->end_freq) {
			state = wlan_reg_get_channel_state_for_pwrmode(
					mac_ctx->pdev, chan_cfreq,
					REG_CURRENT_PWR_MODE);
			if (state == CHANNEL_STATE_INVALID) {
				sap_debug("invalid ch freq %d",
					  chan_cfreq);
				chan_cfreq = chan_cfreq + 20;
				continue;
			}
			if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						     chan_cfreq) &&
			    !find_ch_freq_in_radar_hist(radar_result, count,
							chan_cfreq) &&
			    *idx < max_elems) {
				radar_result[*idx].ch_freq = chan_cfreq;
				radar_result[*idx].time =
						cac_result->ap_end_time;
				radar_result[*idx].radar_found = false;
				sap_debug("radar hist[%d] freq %d time %llu no radar",
					  *idx, chan_cfreq,
					  cac_result->ap_end_time);
				(*idx)++;
			}
			chan_cfreq = chan_cfreq + 20;
		}
	}
}

QDF_STATUS
wlansap_query_radar_history(mac_handle_t mac_handle,
			    struct dfs_radar_history **radar_history,
			    uint32_t *count)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct dfsreq_nolinfo *nol_info;
	uint32_t i;
	uint32_t hist_count;
	struct dfs_radar_history *radar_result;

	nol_info = qdf_mem_malloc(sizeof(struct dfsreq_nolinfo));
	if (!nol_info)
		return QDF_STATUS_E_NOMEM;

	ucfg_dfs_getnol(mac_ctx->pdev, nol_info);

	hist_count = nol_info->dfs_ch_nchans + MAX_NUM_OF_CAC_HISTORY;
	radar_result = qdf_mem_malloc(sizeof(struct dfs_radar_history) *
					hist_count);
	if (!radar_result) {
		qdf_mem_free(nol_info);
		return QDF_STATUS_E_NOMEM;
	}

	for (i = 0; i < nol_info->dfs_ch_nchans && i < DFS_CHAN_MAX; i++) {
		radar_result[i].ch_freq = nol_info->dfs_nol[i].nol_freq;
		radar_result[i].time = nol_info->dfs_nol[i].nol_start_us;
		radar_result[i].radar_found = true;
		sap_debug("radar hist[%d] freq %d time %llu radar",
			  i, nol_info->dfs_nol[i].nol_freq,
			  nol_info->dfs_nol[i].nol_start_us);
	}

	sap_append_cac_history(mac_ctx, radar_result, &i, hist_count);
	sap_debug("hist count %d cur %llu", i, qdf_get_monotonic_boottime());

	*radar_history = radar_result;
	*count = i;
	qdf_mem_free(nol_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline void
sap_update_cac_history(struct mac_context *mac_ctx,
		       struct sap_context *sap_ctx,
		       eSapHddEvent event_id)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
static inline
bool sap_check_peer_for_peer_null_mldaddr(struct wlan_objmgr_peer *peer)
{
	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)peer->mldaddr))
		return true;
	else
		return false;
}
#else
static inline
bool sap_check_peer_for_peer_null_mldaddr(struct wlan_objmgr_peer *peer)
{
	return true;
}
#endif

static
QDF_STATUS sap_populate_peer_assoc_info(struct mac_context *mac_ctx,
					struct csr_roam_info *csr_roaminfo,
					struct sap_event *sap_ap_event)
{
	struct wlan_objmgr_peer *peer;
	tSap_StationAssocReassocCompleteEvent *reassoc_complete;

	reassoc_complete =
		&sap_ap_event->sapevt.sapStationAssocReassocCompleteEvent;

	peer = wlan_objmgr_get_peer_by_mac(mac_ctx->psoc,
					   csr_roaminfo->peerMac.bytes,
					   WLAN_LEGACY_MAC_ID);
	if (!peer) {
		sap_err("Peer object not found");
		return QDF_STATUS_E_FAILURE;
	}

	sap_debug("mlo peer assoc:%d", wlan_peer_mlme_is_assoc_peer(peer));

	if (sap_check_peer_for_peer_null_mldaddr(peer) ||
	    wlan_peer_mlme_is_assoc_peer(peer)) {
		if (csr_roaminfo->assocReqLength < ASSOC_REQ_IE_OFFSET) {
			sap_err("Invalid assoc request length:%d",
				csr_roaminfo->assocReqLength);
			wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
			return QDF_STATUS_E_INVAL;
		}
		reassoc_complete->ies_len = (csr_roaminfo->assocReqLength -
					    ASSOC_REQ_IE_OFFSET);
		reassoc_complete->ies = (csr_roaminfo->assocReqPtr +
					 ASSOC_REQ_IE_OFFSET);
		/* skip current AP address in reassoc frame */
		if (csr_roaminfo->fReassocReq) {
			reassoc_complete->ies_len -= QDF_MAC_ADDR_SIZE;
			reassoc_complete->ies += QDF_MAC_ADDR_SIZE;
		}
	}

	if (csr_roaminfo->addIELen) {
		if (wlan_get_vendor_ie_ptr_from_oui(
		    SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE,
		    csr_roaminfo->paddIE, csr_roaminfo->addIELen)) {
			reassoc_complete->staType = eSTA_TYPE_P2P_CLI;
		} else {
			reassoc_complete->staType = eSTA_TYPE_INFRA;
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE_MLO
static void
sap_reassoc_mld_copy(struct csr_roam_info *csr_roaminfo,
		     tSap_StationAssocReassocCompleteEvent *reassoc_complete)
{
	qdf_copy_macaddr(&reassoc_complete->sta_mld,
			 &csr_roaminfo->peer_mld);
	sap_debug("reassoc_complete->staMld: " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(reassoc_complete->sta_mld.bytes));
}
#else /* WLAN_FEATURE_11BE_MLO */
static inline void
sap_reassoc_mld_copy(struct csr_roam_info *csr_roaminfo,
		     tSap_StationAssocReassocCompleteEvent *reassoc_complete)
{
}
#endif /* WLAN_FEATURE_11BE_MLO */

/**
 * sap_signal_hdd_event() - send event notification
 * @sap_ctx: Sap Context
 * @csr_roaminfo: Pointer to CSR roam information
 * @sap_hddevent: SAP HDD event
 * @context: to pass the element for future support
 *
 * Function for HDD to send the event notification using callback
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sap_signal_hdd_event(struct sap_context *sap_ctx,
		struct csr_roam_info *csr_roaminfo, eSapHddEvent sap_hddevent,
		void *context)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct sap_event *sap_ap_event;
	struct mac_context *mac_ctx;
	struct oem_channel_info *chaninfo;
	tSap_StationAssocIndication *assoc_ind;
	tSap_StartBssCompleteEvent *bss_complete;
	struct sap_ch_selected_s *acs_selected;
	tSap_StationAssocReassocCompleteEvent *reassoc_complete;
	tSap_StationDisassocCompleteEvent *disassoc_comp;
	tSap_StationSetKeyCompleteEvent *key_complete;
	tSap_StationMICFailureEvent *mic_failure;

	/* Format the Start BSS Complete event to return... */
	if (!sap_ctx->sap_event_cb)
		return QDF_STATUS_E_FAILURE;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAILURE;
	}

	sap_ap_event = qdf_mem_malloc(sizeof(*sap_ap_event));
	if (!sap_ap_event)
		return QDF_STATUS_E_NOMEM;

	sap_debug("SAP event callback event = %s",
		  sap_hdd_event_to_string(sap_hddevent));

	switch (sap_hddevent) {
	case eSAP_STA_ASSOC_IND:
		if (!csr_roaminfo) {
			sap_debug("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		/*  TODO - Indicate the assoc request indication to OS */
		sap_ap_event->sapHddEventCode = eSAP_STA_ASSOC_IND;
		assoc_ind = &sap_ap_event->sapevt.sapAssocIndication;

		qdf_copy_macaddr(&assoc_ind->staMac, &csr_roaminfo->peerMac);
		assoc_ind->staId = csr_roaminfo->staId;
		assoc_ind->status = 0;
		/* Required for indicating the frames to upper layer */
		assoc_ind->assocReqLength = csr_roaminfo->assocReqLength;
		assoc_ind->assocReqPtr = csr_roaminfo->assocReqPtr;
		assoc_ind->fWmmEnabled = csr_roaminfo->wmmEnabledSta;
		assoc_ind->ecsa_capable = csr_roaminfo->ecsa_capable;
		if (csr_roaminfo->owe_pending_assoc_ind) {
			if (!sap_fill_owe_ie_in_assoc_ind(assoc_ind,
					 csr_roaminfo->owe_pending_assoc_ind,
					 csr_roaminfo->fReassocReq)) {
				sap_err("Failed to fill OWE IE");
				qdf_mem_free(csr_roaminfo->
					     owe_pending_assoc_ind);
				csr_roaminfo->owe_pending_assoc_ind = NULL;
				qdf_mem_free(sap_ap_event);
				return QDF_STATUS_E_INVAL;
			}
			if (!sap_save_owe_pending_assoc_ind(sap_ctx,
					 csr_roaminfo->owe_pending_assoc_ind)) {
				sap_err("Failed to save assoc ind");
				qdf_mem_free(csr_roaminfo->
					     owe_pending_assoc_ind);
				csr_roaminfo->owe_pending_assoc_ind = NULL;
				qdf_mem_free(sap_ap_event);
				return QDF_STATUS_E_INVAL;
			}
			csr_roaminfo->owe_pending_assoc_ind = NULL;
		}

		if (csr_roaminfo->ft_pending_assoc_ind) {
			if (!sap_save_ft_pending_assoc_ind(sap_ctx,
			    csr_roaminfo->ft_pending_assoc_ind)) {
				sap_err("Failed to save ft assoc ind");
				qdf_mem_free(csr_roaminfo->ft_pending_assoc_ind);
				csr_roaminfo->ft_pending_assoc_ind = NULL;
				qdf_mem_free(sap_ap_event);
				return QDF_STATUS_E_INVAL;
			}
			csr_roaminfo->ft_pending_assoc_ind = NULL;
		}
		break;
	case eSAP_START_BSS_EVENT:
		sap_ap_event->sapHddEventCode = eSAP_START_BSS_EVENT;
		bss_complete = &sap_ap_event->sapevt.sapStartBssCompleteEvent;

		bss_complete->sessionId = sap_ctx->sessionId;
		if (bss_complete->sessionId == WLAN_UMAC_VDEV_ID_MAX) {
			sap_err("Invalid sessionId");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}

		bss_complete->status = (eSapStatus) context;
		bss_complete->staId = sap_ctx->sap_sta_id;

		sap_debug("(eSAP_START_BSS_EVENT): staId = %d",
			  bss_complete->staId);

		bss_complete->operating_chan_freq = sap_ctx->chan_freq;
		bss_complete->ch_width = sap_ctx->ch_params.ch_width;
		if (QDF_IS_STATUS_SUCCESS(bss_complete->status)) {
			sap_update_cac_history(mac_ctx, sap_ctx,
					       sap_hddevent);
			sap_update_mcs_rate(sap_ctx, true);
		}
		break;
	case eSAP_DFS_CAC_START:
	case eSAP_DFS_CAC_INTERRUPTED:
	case eSAP_DFS_CAC_END:
	case eSAP_DFS_RADAR_DETECT:
	case eSAP_DFS_NO_AVAILABLE_CHANNEL:
		sap_ap_event->sapHddEventCode = sap_hddevent;
		sap_ap_event->sapevt.sapStopBssCompleteEvent.status =
			(eSapStatus) context;
		sap_update_cac_history(mac_ctx, sap_ctx,
				       sap_hddevent);
		break;
	case eSAP_ACS_SCAN_SUCCESS_EVENT:
		sap_handle_acs_scan_event(sap_ctx, sap_ap_event,
					  (eSapStatus)context);
		break;
	case eSAP_ACS_CHANNEL_SELECTED:
		sap_ap_event->sapHddEventCode = sap_hddevent;
		acs_selected = &sap_ap_event->sapevt.sap_ch_selected;
		if (eSAP_STATUS_SUCCESS == (eSapStatus)context) {
			acs_selected->pri_ch_freq =
						sap_ctx->acs_cfg->pri_ch_freq;
			acs_selected->ht_sec_ch_freq =
					sap_ctx->acs_cfg->ht_sec_ch_freq;
			acs_selected->ch_width = sap_ctx->acs_cfg->ch_width;
			acs_selected->vht_seg0_center_ch_freq =
				sap_ctx->acs_cfg->vht_seg0_center_ch_freq;
			acs_selected->vht_seg1_center_ch_freq =
				sap_ctx->acs_cfg->vht_seg1_center_ch_freq;
		} else if (eSAP_STATUS_FAILURE == (eSapStatus)context) {
			acs_selected->pri_ch_freq = 0;
		}
		break;

	case eSAP_STOP_BSS_EVENT:
		sap_ap_event->sapHddEventCode = eSAP_STOP_BSS_EVENT;
		sap_ap_event->sapevt.sapStopBssCompleteEvent.status =
			(eSapStatus) context;
		break;

	case eSAP_STA_ASSOC_EVENT:
	case eSAP_STA_REASSOC_EVENT:

		if (!csr_roaminfo) {
			sap_err("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		if (sap_ctx->fsm_state == SAP_STOPPING) {
			sap_err("SAP is stopping, not able to handle any incoming (re)assoc req");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_ABORTED;
		}

		qdf_status = sap_populate_peer_assoc_info(mac_ctx, csr_roaminfo,
							  sap_ap_event);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}

		reassoc_complete =
		    &sap_ap_event->sapevt.sapStationAssocReassocCompleteEvent;

		if (csr_roaminfo->fReassocReq)
			sap_ap_event->sapHddEventCode = eSAP_STA_REASSOC_EVENT;
		else
			sap_ap_event->sapHddEventCode = eSAP_STA_ASSOC_EVENT;

		qdf_copy_macaddr(&reassoc_complete->staMac,
				 &csr_roaminfo->peerMac);
		sap_reassoc_mld_copy(csr_roaminfo, reassoc_complete);
		reassoc_complete->staId = csr_roaminfo->staId;
		reassoc_complete->status_code = csr_roaminfo->status_code;

		/* also fill up the channel info from the csr_roamInfo */
		chaninfo = &reassoc_complete->chan_info;

		chaninfo->mhz = csr_roaminfo->chan_info.mhz;
		chaninfo->info = csr_roaminfo->chan_info.info;
		chaninfo->band_center_freq1 =
			csr_roaminfo->chan_info.band_center_freq1;
		chaninfo->band_center_freq2 =
			csr_roaminfo->chan_info.band_center_freq2;
		chaninfo->reg_info_1 =
			csr_roaminfo->chan_info.reg_info_1;
		chaninfo->reg_info_2 =
			csr_roaminfo->chan_info.reg_info_2;
		chaninfo->nss = csr_roaminfo->chan_info.nss;
		chaninfo->rate_flags = csr_roaminfo->chan_info.rate_flags;

		reassoc_complete->wmmEnabled = csr_roaminfo->wmmEnabledSta;
		reassoc_complete->status = (eSapStatus) context;
		reassoc_complete->timingMeasCap = csr_roaminfo->timingMeasCap;
		reassoc_complete->ampdu = csr_roaminfo->ampdu;
		reassoc_complete->sgi_enable = csr_roaminfo->sgi_enable;
		reassoc_complete->tx_stbc = csr_roaminfo->tx_stbc;
		reassoc_complete->rx_stbc = csr_roaminfo->rx_stbc;
		reassoc_complete->ch_width = csr_roaminfo->ch_width;
		reassoc_complete->mode = csr_roaminfo->mode;
		reassoc_complete->max_supp_idx = csr_roaminfo->max_supp_idx;
		reassoc_complete->max_ext_idx = csr_roaminfo->max_ext_idx;
		reassoc_complete->max_mcs_idx = csr_roaminfo->max_mcs_idx;
		reassoc_complete->max_real_mcs_idx =
						csr_roaminfo->max_real_mcs_idx;
		reassoc_complete->rx_mcs_map = csr_roaminfo->rx_mcs_map;
		reassoc_complete->tx_mcs_map = csr_roaminfo->tx_mcs_map;
		reassoc_complete->ecsa_capable = csr_roaminfo->ecsa_capable;
		reassoc_complete->ext_cap = csr_roaminfo->ext_cap;
		reassoc_complete->supported_band = csr_roaminfo->supported_band;
		if (csr_roaminfo->ht_caps.present)
			reassoc_complete->ht_caps = csr_roaminfo->ht_caps;
		if (csr_roaminfo->vht_caps.present)
			reassoc_complete->vht_caps = csr_roaminfo->vht_caps;
		reassoc_complete->he_caps_present =
						csr_roaminfo->he_caps_present;
		reassoc_complete->capability_info =
						csr_roaminfo->capability_info;

		break;

	case eSAP_STA_DISASSOC_EVENT:
		if (!csr_roaminfo) {
			sap_debug("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		sap_ap_event->sapHddEventCode = eSAP_STA_DISASSOC_EVENT;
		disassoc_comp =
			&sap_ap_event->sapevt.sapStationDisassocCompleteEvent;

		qdf_copy_macaddr(&disassoc_comp->staMac,
				 &csr_roaminfo->peerMac);
		disassoc_comp->staId = csr_roaminfo->staId;
		if (csr_roaminfo->reasonCode == eCSR_ROAM_RESULT_FORCED)
			disassoc_comp->reason = eSAP_USR_INITATED_DISASSOC;
		else
			disassoc_comp->reason = eSAP_MAC_INITATED_DISASSOC;

		disassoc_comp->status_code = csr_roaminfo->status_code;
		disassoc_comp->status = (eSapStatus) context;
		disassoc_comp->rssi = csr_roaminfo->rssi;
		disassoc_comp->rx_rate = csr_roaminfo->rx_rate;
		disassoc_comp->tx_rate = csr_roaminfo->tx_rate;
		disassoc_comp->rx_mc_bc_cnt = csr_roaminfo->rx_mc_bc_cnt;
		disassoc_comp->rx_retry_cnt = csr_roaminfo->rx_retry_cnt;
		disassoc_comp->reason_code = csr_roaminfo->disassoc_reason;
		break;

	case eSAP_STA_SET_KEY_EVENT:

		if (!csr_roaminfo) {
			sap_debug("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		sap_ap_event->sapHddEventCode = eSAP_STA_SET_KEY_EVENT;
		key_complete =
			&sap_ap_event->sapevt.sapStationSetKeyCompleteEvent;
		key_complete->status = (eSapStatus) context;
		qdf_copy_macaddr(&key_complete->peerMacAddr,
				 &csr_roaminfo->peerMac);
		break;

	case eSAP_STA_MIC_FAILURE_EVENT:

		if (!csr_roaminfo) {
			sap_debug("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		sap_ap_event->sapHddEventCode = eSAP_STA_MIC_FAILURE_EVENT;
		mic_failure = &sap_ap_event->sapevt.sapStationMICFailureEvent;

		qdf_mem_copy(&mic_failure->srcMacAddr,
			     csr_roaminfo->u.pMICFailureInfo->srcMacAddr,
			     sizeof(tSirMacAddr));
		qdf_mem_copy(&mic_failure->staMac.bytes,
			     csr_roaminfo->u.pMICFailureInfo->taMacAddr,
			     sizeof(tSirMacAddr));
		qdf_mem_copy(&mic_failure->dstMacAddr.bytes,
			     csr_roaminfo->u.pMICFailureInfo->dstMacAddr,
			     sizeof(tSirMacAddr));
		mic_failure->multicast =
			csr_roaminfo->u.pMICFailureInfo->multicast;
		mic_failure->IV1 = csr_roaminfo->u.pMICFailureInfo->IV1;
		mic_failure->keyId = csr_roaminfo->u.pMICFailureInfo->keyId;
		qdf_mem_copy(mic_failure->TSC,
			     csr_roaminfo->u.pMICFailureInfo->TSC,
			     SIR_CIPHER_SEQ_CTR_SIZE);
		break;

	case eSAP_WPS_PBC_PROBE_REQ_EVENT:

		if (!csr_roaminfo) {
			sap_debug("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		sap_ap_event->sapHddEventCode = eSAP_WPS_PBC_PROBE_REQ_EVENT;

		qdf_mem_copy(&sap_ap_event->sapevt.sapPBCProbeReqEvent.
			     WPSPBCProbeReq, csr_roaminfo->u.pWPSPBCProbeReq,
			     sizeof(tSirWPSPBCProbeReq));
		break;

	case eSAP_DISCONNECT_ALL_P2P_CLIENT:
		sap_ap_event->sapHddEventCode = eSAP_DISCONNECT_ALL_P2P_CLIENT;
		sap_ap_event->sapevt.sapActionCnf.actionSendSuccess =
			(eSapStatus) context;
		break;

	case eSAP_MAC_TRIG_STOP_BSS_EVENT:
		sap_ap_event->sapHddEventCode = eSAP_MAC_TRIG_STOP_BSS_EVENT;
		sap_ap_event->sapevt.sapActionCnf.actionSendSuccess =
			(eSapStatus) context;
		break;

	case eSAP_UNKNOWN_STA_JOIN:
		sap_ap_event->sapHddEventCode = eSAP_UNKNOWN_STA_JOIN;
		qdf_mem_copy((void *)sap_ap_event->sapevt.sapUnknownSTAJoin.
			     macaddr.bytes, (void *) context,
			     QDF_MAC_ADDR_SIZE);
		break;

	case eSAP_MAX_ASSOC_EXCEEDED:

		if (!csr_roaminfo) {
			sap_debug("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		sap_ap_event->sapHddEventCode = eSAP_MAX_ASSOC_EXCEEDED;
		qdf_copy_macaddr(&sap_ap_event->sapevt.
				 sapMaxAssocExceeded.macaddr,
				 &csr_roaminfo->peerMac);
		break;

	case eSAP_CHANNEL_CHANGE_EVENT:
		/*
		 * Reconfig ACS result info. For DFS AP-AP Mode Sec AP ACS
		 * follows pri AP
		 */
		sap_ctx->acs_cfg->pri_ch_freq = sap_ctx->chan_freq;
		sap_ctx->acs_cfg->ch_width =
					sap_ctx->ch_params.ch_width;
		sap_config_acs_result(MAC_HANDLE(mac_ctx), sap_ctx,
				      sap_ctx->sec_ch_freq);

		sap_ap_event->sapHddEventCode = eSAP_CHANNEL_CHANGE_EVENT;

		acs_selected = &sap_ap_event->sapevt.sap_ch_selected;
		acs_selected->pri_ch_freq = sap_ctx->chan_freq;
		acs_selected->ht_sec_ch_freq = sap_ctx->sec_ch_freq;
		acs_selected->ch_width =
			sap_ctx->acs_cfg->ch_width;
		acs_selected->vht_seg0_center_ch_freq =
			sap_ctx->acs_cfg->vht_seg0_center_ch_freq;
		acs_selected->vht_seg1_center_ch_freq =
			sap_ctx->acs_cfg->vht_seg1_center_ch_freq;
		break;

	case eSAP_ECSA_CHANGE_CHAN_IND:

		if (!csr_roaminfo) {
			sap_debug("Invalid CSR Roam Info");
			qdf_mem_free(sap_ap_event);
			return QDF_STATUS_E_INVAL;
		}
		sap_debug("SAP event callback event = %s",
			  "eSAP_ECSA_CHANGE_CHAN_IND");
		sap_ap_event->sapHddEventCode = eSAP_ECSA_CHANGE_CHAN_IND;
		sap_ap_event->sapevt.sap_chan_cng_ind.new_chan_freq =
					   csr_roaminfo->target_chan_freq;
		break;
	case eSAP_DFS_NEXT_CHANNEL_REQ:
		sap_debug("SAP event callback event = %s",
			  "eSAP_DFS_NEXT_CHANNEL_REQ");
		sap_ap_event->sapHddEventCode = eSAP_DFS_NEXT_CHANNEL_REQ;
		break;
	case eSAP_STOP_BSS_DUE_TO_NO_CHNL:
		sap_ap_event->sapHddEventCode = eSAP_STOP_BSS_DUE_TO_NO_CHNL;
		sap_debug("stopping session_id:%d, bssid:"QDF_MAC_ADDR_FMT", chan_freq:%d",
			   sap_ctx->sessionId,
			   QDF_MAC_ADDR_REF(sap_ctx->self_mac_addr),
			   sap_ctx->chan_freq);
		break;

	case eSAP_CHANNEL_CHANGE_RESP:
		sap_ap_event->sapHddEventCode = eSAP_CHANNEL_CHANGE_RESP;
		acs_selected = &sap_ap_event->sapevt.sap_ch_selected;
		acs_selected->pri_ch_freq = sap_ctx->chan_freq;
		acs_selected->ht_sec_ch_freq = sap_ctx->sec_ch_freq;
		acs_selected->ch_width =
				sap_ctx->ch_params.ch_width;
		acs_selected->vht_seg0_center_ch_freq =
				sap_ctx->ch_params.mhz_freq_seg0;
		acs_selected->vht_seg1_center_ch_freq =
				sap_ctx->ch_params.mhz_freq_seg1;
		sap_update_cac_history(mac_ctx, sap_ctx,
				       sap_hddevent);
		sap_debug("SAP event callback event = %s",
			  "eSAP_CHANNEL_CHANGE_RESP");
		break;

	default:
		sap_err("SAP Unknown callback event = %d", sap_hddevent);
		break;
	}
	qdf_status = (*sap_ctx->sap_event_cb)
			(sap_ap_event, sap_ctx->user_context);

	qdf_mem_free(sap_ap_event);

	return qdf_status;
}

bool sap_is_dfs_cac_wait_state(struct sap_context *sap_ctx)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;

	if (!sap_ctx) {
		sap_err("Invalid sap context");
		return false;
	}

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle)
		return false;

	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return false;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    sap_ctx->sessionId,
						    WLAN_DFS_ID);
	if (!vdev) {
		sap_err("vdev is NULL for vdev_id: %u", sap_ctx->sessionId);
		return false;
	}

	status = wlan_vdev_is_dfs_cac_wait(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_DFS_ID);

	return QDF_IS_STATUS_SUCCESS(status);
}

/**
 * sap_find_cac_wait_session() - Get context of a SAP session in CAC wait state
 * @handle: Global MAC handle
 *
 * Finds and gets the context of a SAP session in CAC wait state.
 *
 * Return: Valid SAP context on success, else NULL
 */
static struct sap_context *sap_find_cac_wait_session(mac_handle_t handle)
{
	struct mac_context *mac = MAC_CONTEXT(handle);
	uint8_t i = 0;
	struct sap_context *sap_ctx;

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		sap_ctx = mac->sap.sapCtxList[i].sap_context;
		if (((QDF_SAP_MODE == mac->sap.sapCtxList[i].sapPersona)
		    ||
		    (QDF_P2P_GO_MODE == mac->sap.sapCtxList[i].sapPersona)) &&
		    (sap_is_dfs_cac_wait_state(sap_ctx))) {
			sap_debug("found SAP in cac wait state");
			return sap_ctx;
		}
		if (sap_ctx) {
			sap_debug("sapdfs: mode:%d intf:%d state:%d",
				  mac->sap.sapCtxList[i].sapPersona, i,
				  sap_ctx->fsm_state);
		}
	}

	return NULL;
}

void sap_cac_reset_notify(mac_handle_t mac_handle)
{
	uint8_t intf = 0;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		struct sap_context *sap_context =
			mac->sap.sapCtxList[intf].sap_context;
		if (((QDF_SAP_MODE == mac->sap.sapCtxList[intf].sapPersona)
		    ||
		    (QDF_P2P_GO_MODE == mac->sap.sapCtxList[intf].sapPersona))
		    && mac->sap.sapCtxList[intf].sap_context) {
			sap_context->isCacStartNotified = false;
			sap_context->isCacEndNotified = false;
		}
	}
}

/**
 * sap_cac_start_notify() - Notify CAC start to HDD
 * @mac_handle: Opaque handle to the global MAC context
 *
 * Function will be called to notify eSAP_DFS_CAC_START event to HDD
 *
 * Return: QDF_STATUS_SUCCESS if the notification was sent, otherwise
 *         an appropriate QDF_STATUS error
 */
static QDF_STATUS sap_cac_start_notify(mac_handle_t mac_handle)
{
	uint8_t intf = 0;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		struct sap_context *sap_context =
			mac->sap.sapCtxList[intf].sap_context;

		if (((QDF_SAP_MODE == mac->sap.sapCtxList[intf].sapPersona)
		    ||
		    (QDF_P2P_GO_MODE == mac->sap.sapCtxList[intf].sapPersona))
		    && mac->sap.sapCtxList[intf].sap_context &&
		    (false == sap_context->isCacStartNotified)) {
			/* Don't start CAC for non-dfs channel, its violation */
			if (!sap_operating_on_dfs(
					mac,
					mac->sap.sapCtxList[intf].sap_context))
				continue;
			sap_debug("sapdfs: Signaling eSAP_DFS_CAC_START to HDD for sapctx[%pK]",
				  sap_context);

			qdf_status = sap_signal_hdd_event(sap_context, NULL,
							  eSAP_DFS_CAC_START,
							  (void *)
							  eSAP_STATUS_SUCCESS);
			if (QDF_STATUS_SUCCESS != qdf_status) {
				sap_err("Failed setting isCacStartNotified on interface[%d]",
					 intf);
				return qdf_status;
			}
			sap_context->isCacStartNotified = true;
		}
	}
	return qdf_status;
}

#ifdef PRE_CAC_SUPPORT
/**
 * wlansap_pre_cac_end_notify() - Update pre cac end to upper layer
 * @sap_context: SAP context
 * @mac: Global MAC structure
 * @intf: Interface number
 *
 * Notifies pre cac end to upper layer
 *
 * Return: None
 */
static void wlansap_pre_cac_end_notify(struct sap_context *sap_context,
				       struct mac_context *mac,
				       uint8_t intf)
{
	sap_context->isCacEndNotified = true;
	sap_context->sap_radar_found_status = false;
	sap_context->fsm_state = SAP_STARTED;
	sap_warn("sap_fsm: vdev %d => SAP_STARTED, pre cac end notify on %d",
		 sap_context->vdev_id, intf);
	wlan_pre_cac_handle_cac_end(sap_context->vdev);
}

QDF_STATUS sap_cac_end_notify(mac_handle_t mac_handle,
			      struct csr_roam_info *roamInfo)
{
	uint8_t intf;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	/*
	 * eSAP_DFS_CHANNEL_CAC_END:
	 * CAC Period elapsed and there was no radar
	 * found so, SAP can continue beaconing.
	 * sap_radar_found_status is set to 0
	 */
	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		struct sap_context *sap_context =
			mac->sap.sapCtxList[intf].sap_context;

		if (((QDF_SAP_MODE == mac->sap.sapCtxList[intf].sapPersona)
		    ||
		    (QDF_P2P_GO_MODE == mac->sap.sapCtxList[intf].sapPersona))
		    && mac->sap.sapCtxList[intf].sap_context &&
		    (false == sap_context->isCacEndNotified) &&
		    sap_is_dfs_cac_wait_state(sap_context)) {
			sap_context = mac->sap.sapCtxList[intf].sap_context;
			/* Don't check CAC for non-dfs channel */
			if (!sap_operating_on_dfs(
					mac,
					mac->sap.sapCtxList[intf].sap_context))
				continue;

			/* If this is an end notification of a pre cac, the
			 * SAP must not start beaconing and must delete the
			 * temporary interface created for pre cac and switch
			 * the original SAP to the pre CAC channel.
			 */
			if (wlan_pre_cac_get_status(mac->psoc)) {
				wlansap_pre_cac_end_notify(sap_context,
							   mac, intf);
				/* pre CAC is not allowed with any concurrency.
				 * So, we can break from here.
				 */
				break;
			}

			qdf_status = sap_signal_hdd_event(sap_context, NULL,
							  eSAP_DFS_CAC_END,
							  (void *)
							  eSAP_STATUS_SUCCESS);
			if (QDF_STATUS_SUCCESS != qdf_status) {
				sap_err("failed setting isCacEndNotified on interface[%d]",
					 intf);
				return qdf_status;
			}
			sap_context->isCacEndNotified = true;
			sap_context->sap_radar_found_status = false;
			sap_debug("sapdfs: Start beacon request on sapctx[%pK]",
				  sap_context);

			/* Start beaconing on the new channel */
			wlansap_start_beacon_req(sap_context);

			/* Transition from SAP_STARTING to SAP_STARTED
			 * (both without substates)
			 */
			sap_context->fsm_state = SAP_STARTED;
			sap_debug("sap_fsm: vdev %d: SAP_STARTING => SAP_STARTED, freq %d",
				  sap_context->vdev_id, sap_context->chan_freq);

			/*Action code for transition */
			qdf_status = sap_signal_hdd_event(sap_context, roamInfo,
							  eSAP_START_BSS_EVENT,
							  (void *)
							  eSAP_STATUS_SUCCESS);
			if (QDF_STATUS_SUCCESS != qdf_status) {
				sap_err("Failed setting isCacEndNotified on interface[%d]",
					 intf);
				return qdf_status;
			}
		}
	}
	/*
	 * All APs are done with CAC timer, all APs should start beaconing.
	 * Lets assume AP1 and AP2 started beaconing on DFS channel, Now lets
	 * say AP1 goes down and comes back on same DFS channel. In this case
	 * AP1 shouldn't start CAC timer and start beacon immediately because
	 * AP2 is already beaconing on this channel. This case will be handled
	 * by checking against eSAP_DFS_SKIP_CAC while starting the timer.
	 */
	mac->sap.SapDfsInfo.cac_state = eSAP_DFS_SKIP_CAC;
	return qdf_status;
}
#endif

/**
 * sap_validate_dfs_nol() - Validate SAP channel with NOL list
 * @sap_ctx: SAP context
 * @sap_ctx: MAC context
 *
 * Function will be called to validate SAP channel and bonded sub channels
 * included in DFS NOL or not.
 *
 * Return: QDF_STATUS_SUCCESS for NOT in NOL
 */
static QDF_STATUS sap_validate_dfs_nol(struct sap_context *sap_ctx,
				       struct mac_context *mac_ctx)
{
	bool b_leak_chan = false;
	uint16_t temp_freq;
	uint16_t sap_freq;
	enum channel_state ch_state;
	bool is_chan_nol = false;

	sap_freq = sap_ctx->chan_freq;
	temp_freq = sap_freq;
	utils_dfs_mark_leaking_chan_for_freq(mac_ctx->pdev,
					     sap_ctx->ch_params.ch_width, 1,
					     &temp_freq);

	/*
	 * if selelcted channel has leakage to channels
	 * in NOL, the temp_freq will be reset
	 */
	b_leak_chan = (temp_freq != sap_freq);
	/*
	 * check if channel is in DFS_NOL or if the channel
	 * has leakage to the channels in NOL
	 */

	if (sap_phymode_is_eht(sap_ctx->phyMode)) {
		ch_state =
			wlan_reg_get_channel_state_from_secondary_list_for_freq(
						mac_ctx->pdev, sap_freq);
		if (CHANNEL_STATE_ENABLE != ch_state &&
		    CHANNEL_STATE_DFS != ch_state) {
			sap_err_rl("Invalid sap freq = %d, ch state=%d",
				   sap_freq, ch_state);
			is_chan_nol = true;
		}
	} else {
		is_chan_nol = sap_dfs_is_channel_in_nol_list(
					sap_ctx, sap_ctx->chan_freq,
					PHY_CHANNEL_BONDING_STATE_MAX);
	}

	if (is_chan_nol || b_leak_chan) {
		qdf_freq_t chan_freq;

		/* find a new available channel */
		chan_freq = sap_random_channel_sel(sap_ctx);
		if (!chan_freq) {
			/* No available channel found */
			sap_err("No available channel found!!!");
			sap_signal_hdd_event(sap_ctx, NULL,
					     eSAP_DFS_NO_AVAILABLE_CHANNEL,
					     (void *)eSAP_STATUS_SUCCESS);
			return QDF_STATUS_E_FAULT;
		}

		sap_debug("ch_freq %d is in NOL, start bss on new freq %d",
			  sap_ctx->chan_freq, chan_freq);

		sap_ctx->chan_freq = chan_freq;
		if (sap_phymode_is_eht(sap_ctx->phyMode))
			wlan_reg_set_create_punc_bitmap(&sap_ctx->ch_params,
							true);
		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
							sap_ctx->chan_freq,
							sap_ctx->sec_ch_freq,
							&sap_ctx->ch_params,
							REG_CURRENT_PWR_MODE);
	}

	return QDF_STATUS_SUCCESS;
}

static void sap_validate_chanmode_and_chwidth(struct mac_context *mac_ctx,
					      struct sap_context *sap_ctx)
{
	uint32_t orig_phymode;
	enum phy_ch_width orig_ch_width;

	orig_ch_width = sap_ctx->ch_params.ch_width;
	orig_phymode = sap_ctx->phyMode;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(sap_ctx->chan_freq) &&
	    (sap_ctx->phyMode == eCSR_DOT11_MODE_11g ||
	     sap_ctx->phyMode == eCSR_DOT11_MODE_11g_ONLY)) {
		sap_ctx->phyMode = eCSR_DOT11_MODE_11a;
	} else if (WLAN_REG_IS_24GHZ_CH_FREQ(sap_ctx->chan_freq) &&
		   (sap_ctx->phyMode == eCSR_DOT11_MODE_11a)) {
		sap_ctx->phyMode = eCSR_DOT11_MODE_11g;
	}

	if (sap_ctx->ch_params.ch_width > CH_WIDTH_20MHZ &&
	    (sap_ctx->phyMode == eCSR_DOT11_MODE_abg ||
	     sap_ctx->phyMode == eCSR_DOT11_MODE_11a ||
	     sap_ctx->phyMode == eCSR_DOT11_MODE_11g ||
	     sap_ctx->phyMode == eCSR_DOT11_MODE_11b)) {
		sap_ctx->ch_params.ch_width = CH_WIDTH_20MHZ;
		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
					       sap_ctx->chan_freq,
					       sap_ctx->ch_params.sec_ch_offset,
					       &sap_ctx->ch_params,
					       REG_CURRENT_PWR_MODE);
	} else if (sap_ctx->ch_params.ch_width > CH_WIDTH_40MHZ &&
		   sap_ctx->phyMode == eCSR_DOT11_MODE_11n) {
		sap_ctx->ch_params.ch_width = CH_WIDTH_40MHZ;
		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
					       sap_ctx->chan_freq,
					       sap_ctx->ch_params.sec_ch_offset,
					       &sap_ctx->ch_params,
					       REG_CURRENT_PWR_MODE);
	}

	if (orig_ch_width != sap_ctx->ch_params.ch_width ||
	    orig_phymode != sap_ctx->phyMode)
		sap_info("Freq %d Updated BW %d -> %d , phymode %d -> %d",
			 sap_ctx->chan_freq, orig_ch_width,
			 sap_ctx->ch_params.ch_width,
			 orig_phymode, sap_ctx->phyMode);
}

static bool
wlansap_is_power_change_required(struct mac_context *mac_ctx,
				 qdf_freq_t sap_freq)
{
	struct wlan_objmgr_vdev *sta_vdev;
	uint8_t sta_vdev_id;
	enum hw_mode_bandwidth ch_wd;
	uint8_t country[CDS_COUNTRY_CODE_LEN + 1];
	enum channel_state state;
	uint32_t ap_pwr_type_6g = 0;
	bool indoor_ch_support = false;

	if (!mac_ctx || !mac_ctx->psoc || !mac_ctx->pdev)
		return false;

	if (!policy_mgr_is_sta_present_on_freq(mac_ctx->psoc, &sta_vdev_id,
					       sap_freq, &ch_wd)) {
		return false;
	}

	sta_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							sta_vdev_id,
							WLAN_LEGACY_SAP_ID);
	if (!sta_vdev)
		return false;

	ap_pwr_type_6g = wlan_mlme_get_6g_ap_power_type(sta_vdev);

	wlan_objmgr_vdev_release_ref(sta_vdev, WLAN_LEGACY_SAP_ID);

	if (ap_pwr_type_6g == REG_VERY_LOW_POWER_AP)
		return false;
	ucfg_mlme_get_indoor_channel_support(mac_ctx->psoc, &indoor_ch_support);

	if (ap_pwr_type_6g == REG_INDOOR_AP && indoor_ch_support) {
		sap_debug("STA is connected to Indoor AP and indoor concurrency is supported");
		return false;
	}

	wlan_reg_read_current_country(mac_ctx->psoc, country);
	if (!wlan_reg_ctry_support_vlp(country)) {
		sap_debug("Device country doesn't support VLP");
		return false;
	}

	state = wlan_reg_get_channel_state_for_pwrmode(mac_ctx->pdev,
						       sap_freq, REG_AP_VLP);

	return state & CHANNEL_STATE_ENABLE;
}

/**
 * sap_goto_starting() - Trigger softap start
 * @sap_ctx: SAP context
 * @sap_event: SAP event buffer
 * @mac_ctx: global MAC context
 * @mac_handle: Opaque handle to the global MAC context
 *
 * This function triggers start of softap. Before starting, it can select
 * new channel if given channel has leakage or if given channel in DFS_NOL.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_goto_starting(struct sap_context *sap_ctx,
				    struct sap_sm_event *sap_event,
				    struct mac_context *mac_ctx,
				    mac_handle_t mac_handle)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct bss_dot11_config dot11_cfg = {0};
	tSirMacRateSet *opr_rates = &sap_ctx->sap_bss_cfg.operationalRateSet;
	tSirMacRateSet *ext_rates = &sap_ctx->sap_bss_cfg.extendedRateSet;
	uint8_t h2e;

	/*
	 * check if channel is in DFS_NOL or if the channel
	 * has leakage to the channels in NOL.
	 */
	if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ctx->chan_freq)) {
		qdf_status = sap_validate_dfs_nol(sap_ctx, mac_ctx);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			return qdf_status;
	} else if (!policy_mgr_get_ap_6ghz_capable(mac_ctx->psoc,
						   sap_ctx->sessionId, NULL)) {
		return QDF_STATUS_E_FAILURE;
	} else if (wlansap_is_power_change_required(mac_ctx,
						    sap_ctx->chan_freq)) {
		wlan_set_tpc_update_required_for_sta(sap_ctx->vdev, true);
	}

	/*
	 * when AP2 is started while AP1 is performing ACS, we may not
	 * have the AP1 channel yet.So here after the completion of AP2
	 * ACS check if AP1 ACS resulting channel is DFS and if yes
	 * override AP2 ACS scan result with AP1 DFS channel
	 */
	if (policy_mgr_concurrent_beaconing_sessions_running(mac_ctx->psoc)) {
		uint32_t con_ch_freq;
		uint16_t con_ch;

		con_ch_freq = sme_get_beaconing_concurrent_operation_channel(
				mac_handle, sap_ctx->sessionId);
		con_ch = wlan_reg_freq_to_chan(mac_ctx->pdev, con_ch_freq);
		/* Overwrite second AP's channel with first only when:
		 * 1. If operating mode is single mac
		 * 2. or if 2nd AP is coming up on 5G band channel
		 */
		if ((!policy_mgr_is_hw_dbs_capable(mac_ctx->psoc) ||
		     WLAN_REG_IS_5GHZ_CH_FREQ(sap_ctx->chan_freq)) &&
		     con_ch &&
		     wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
					      con_ch_freq)) {
			sap_ctx->chan_freq = con_ch_freq;
			if (sap_phymode_is_eht(sap_ctx->phyMode))
				wlan_reg_set_create_punc_bitmap(
					&sap_ctx->ch_params, true);
			wlan_reg_set_channel_params_for_pwrmode(
						    mac_ctx->pdev,
						    con_ch_freq, 0,
						    &sap_ctx->ch_params,
						    REG_CURRENT_PWR_MODE);
		}
	}

	sap_validate_chanmode_and_chwidth(mac_ctx, sap_ctx);
	/* Channel selected. Now can sap_goto_starting */
	sap_ctx->fsm_state = SAP_STARTING;
	sap_debug("sap_fsm: vdev %d: SAP_INIT => SAP_STARTING, phyMode %d bw %d",
		  sap_ctx->vdev_id, sap_ctx->phyMode,
		  sap_ctx->ch_params.ch_width);
	/* Specify the channel */
	sap_get_cac_dur_dfs_region(sap_ctx,
				   &sap_ctx->sap_bss_cfg.cac_duration_ms,
				   &sap_ctx->sap_bss_cfg.dfs_regdomain,
				   sap_ctx->chan_freq,
				   &sap_ctx->ch_params);
	mlme_set_cac_required(sap_ctx->vdev,
			      !!sap_ctx->sap_bss_cfg.cac_duration_ms);

	sap_ctx->sap_bss_cfg.oper_ch_freq = sap_ctx->chan_freq;
	sap_ctx->sap_bss_cfg.vht_channel_width = sap_ctx->ch_params.ch_width;
	sap_ctx->sap_bss_cfg.center_freq_seg0 =
					sap_ctx->ch_params.center_freq_seg0;
	sap_ctx->sap_bss_cfg.center_freq_seg1 =
					sap_ctx->ch_params.center_freq_seg1;
	sap_ctx->sap_bss_cfg.sec_ch_offset = sap_ctx->ch_params.sec_ch_offset;

	dot11_cfg.vdev_id = sap_ctx->sessionId;
	dot11_cfg.bss_op_ch_freq = sap_ctx->chan_freq;
	dot11_cfg.phy_mode = sap_ctx->phyMode;
	dot11_cfg.privacy = sap_ctx->sap_bss_cfg.privacy;

	qdf_mem_copy(dot11_cfg.opr_rates.rate,
		     opr_rates->rate, opr_rates->numRates);
	dot11_cfg.opr_rates.numRates = opr_rates->numRates;

	qdf_mem_copy(dot11_cfg.ext_rates.rate,
		     ext_rates->rate, ext_rates->numRates);
	dot11_cfg.ext_rates.numRates = ext_rates->numRates;

	sme_get_network_params(mac_ctx, &dot11_cfg);

	sap_ctx->sap_bss_cfg.nwType = dot11_cfg.nw_type;
	sap_ctx->sap_bss_cfg.dot11mode = dot11_cfg.dot11_mode;

	if (dot11_cfg.opr_rates.numRates) {
		qdf_mem_copy(opr_rates->rate,
			     dot11_cfg.opr_rates.rate,
			     dot11_cfg.opr_rates.numRates);
		opr_rates->numRates = dot11_cfg.opr_rates.numRates;
	} else {
		qdf_mem_zero(opr_rates, sizeof(tSirMacRateSet));
	}

	if (dot11_cfg.ext_rates.numRates) {
		qdf_mem_copy(ext_rates->rate,
			     dot11_cfg.ext_rates.rate,
			     dot11_cfg.ext_rates.numRates);
		ext_rates->numRates = dot11_cfg.ext_rates.numRates;
	} else {
		qdf_mem_zero(ext_rates, sizeof(tSirMacRateSet));
	}

	if (sap_ctx->require_h2e) {
		h2e = WLAN_BASIC_RATE_MASK |
			WLAN_BSS_MEMBERSHIP_SELECTOR_SAE_H2E;
		if (ext_rates->numRates < SIR_MAC_MAX_NUMBER_OF_RATES) {
			ext_rates->rate[ext_rates->numRates] = h2e;
			ext_rates->numRates++;
			sap_debug("H2E bss membership add to ext support rate");
		} else if (opr_rates->numRates < SIR_MAC_MAX_NUMBER_OF_RATES) {
			opr_rates->rate[opr_rates->numRates] = h2e;
			opr_rates->numRates++;
			sap_debug("H2E bss membership add to support rate");
		} else {
			sap_err("rates full, can not add H2E bss membership");
		}
	}
	sap_dfs_set_current_channel(sap_ctx);
	/* Reset radar found flag before start sap, the flag will
	 * be set when radar found in CAC wait.
	 */
	sap_ctx->sap_radar_found_status = false;

	sap_debug("session: %d", sap_ctx->sessionId);

	qdf_status = sme_start_bss(mac_handle, sap_ctx->sessionId,
				   &sap_ctx->sap_bss_cfg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sap_err("Failed to issue sme_roam_connect");

	return qdf_status;
}

/**
 * sap_fsm_cac_start() - start cac wait timer
 * @sap_ctx: SAP context
 * @mac_ctx: global MAC context
 * @mac_handle: Opaque handle to the global MAC context
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_fsm_cac_start(struct sap_context *sap_ctx,
				    struct mac_context *mac_ctx,
				    mac_handle_t mac_handle)
{
	if (!mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running) {
		sap_debug("sapdfs: starting dfs cac timer on sapctx[%pK]",
			  sap_ctx);
		sap_start_dfs_cac_timer(sap_ctx);
	}

	return sap_cac_start_notify(mac_handle);
}

/**
 * sap_fsm_state_init() - utility function called from sap fsm
 * @sap_ctx: SAP context
 * @sap_event: SAP event buffer
 * @mac_ctx: global MAC context
 * @mac_handle: Opaque handle to the global MAC context
 *
 * This function is called for state transition from "SAP_INIT"
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_fsm_state_init(struct sap_context *sap_ctx,
				     struct sap_sm_event *sap_event,
				     struct mac_context *mac_ctx,
				     mac_handle_t mac_handle)
{
	uint32_t msg = sap_event->event;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	if (msg == eSAP_HDD_START_INFRA_BSS) {
		/* init dfs channel nol */
		sap_init_dfs_channel_nol_list(sap_ctx);

		/*
		 * Perform sme_ScanRequest. This scan request is post start bss
		 * request so, set the third to false.
		 */
		qdf_status = sap_validate_chan(sap_ctx, false, true);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			sap_err("vdev %d: channel is not valid!",
				sap_ctx->vdev_id);
			goto exit;
		}

		qdf_status = sap_goto_starting(sap_ctx, sap_event,
					       mac_ctx, mac_handle);
		if (QDF_IS_STATUS_ERROR(qdf_status))
			sap_err("vdev %d: sap_goto_starting failed",
				sap_ctx->vdev_id);
	} else {
		sap_err("sap_fsm: vdev %d: SAP_INIT, invalid event %d",
			sap_ctx->vdev_id, msg);
	}

exit:
	return qdf_status;
}

/**
 * sap_fsm_handle_radar_during_cac() - uhandle radar event during cac
 * @sap_ctx: SAP context
 * @mac_ctx: global MAC context
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_fsm_handle_radar_during_cac(struct sap_context *sap_ctx,
						  struct mac_context *mac_ctx)
{
	uint8_t intf;

	if (mac_ctx->sap.SapDfsInfo.target_chan_freq) {
		if (sap_phymode_is_eht(sap_ctx->phyMode))
			wlan_reg_set_create_punc_bitmap(&sap_ctx->ch_params,
							true);
		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
				    mac_ctx->sap.SapDfsInfo.target_chan_freq, 0,
				    &sap_ctx->ch_params, REG_CURRENT_PWR_MODE);
	} else {
		sap_err("Invalid target channel freq %d",
			 mac_ctx->sap.SapDfsInfo.target_chan_freq);
		return QDF_STATUS_E_FAILURE;
	}

	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		struct sap_context *t_sap_ctx;

		t_sap_ctx = mac_ctx->sap.sapCtxList[intf].sap_context;
		if (((QDF_SAP_MODE ==
		      mac_ctx->sap.sapCtxList[intf].sapPersona) ||
		     (QDF_P2P_GO_MODE ==
		      mac_ctx->sap.sapCtxList[intf].sapPersona)) &&
		    t_sap_ctx && t_sap_ctx->fsm_state != SAP_INIT) {
			if (!sap_operating_on_dfs(mac_ctx, t_sap_ctx))
				continue;
			t_sap_ctx->is_chan_change_inprogress = true;
			/*
			 * eSAP_DFS_CHANNEL_CAC_RADAR_FOUND:
			 * A Radar is found on current DFS Channel
			 * while in CAC WAIT period So, do a channel
			 * switch to randomly selected	target channel.
			 * Send the Channel change message to SME/PE.
			 * sap_radar_found_status is set to 1
			 */
			wlansap_channel_change_request(t_sap_ctx,
				mac_ctx->sap.SapDfsInfo.target_chan_freq);
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * sap_fsm_handle_start_failure() - handle sap start failure
 * @sap_ctx: SAP context
 * @msg: event msg
 * @mac_handle: Opaque handle to the global MAC context
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_fsm_handle_start_failure(struct sap_context *sap_ctx,
					       uint32_t msg,
					       mac_handle_t mac_handle)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	if (msg == eSAP_HDD_STOP_INFRA_BSS) {
		/*
		 * Stop the CAC timer only in following conditions
		 * single AP: if there is a single AP then stop timer
		 * multiple APs: incase of multiple APs, make sure that
		 * all APs are down.
		 */
		if (!sap_find_valid_concurrent_session(mac_handle)) {
			sap_debug("sapdfs: no sessions are valid, stopping timer");
			sap_stop_dfs_cac_timer(sap_ctx);
		}
		/* Transition from SAP_STARTING to SAP_STOPPING */
		sap_ctx->fsm_state = SAP_STOPPING;
		sap_debug("sap_fsm: vdev %d: SAP_STARTING => SAP_STOPPING, start is in progress",
			  sap_ctx->vdev_id);
		qdf_status = sap_goto_stopping(sap_ctx);
	} else {
		/*
		 * Transition from SAP_STARTING to SAP_INIT
		 * (both without substates)
		 */
		sap_ctx->fsm_state = SAP_INIT;
		sap_debug("sap_fsm: vdev %d: SAP_STARTING => SAP_INIT",
			  sap_ctx->vdev_id);
		qdf_status = sap_signal_hdd_event(sap_ctx, NULL,
						  eSAP_START_BSS_EVENT,
						  (void *)
						  eSAP_STATUS_FAILURE);
		qdf_status = sap_goto_init(sap_ctx);
	}

	return qdf_status;
}

/**
 * sap_propagate_cac_events() - Indicate CAC START/END event
 * @sap_ctx: SAP context
 *
 * This function is to indicate CAC START/END event if CAC process
 * is skipped.
 *
 * Return: void
 */
static void sap_propagate_cac_events(struct sap_context *sap_ctx)
{
	QDF_STATUS qdf_status;

	qdf_status = sap_signal_hdd_event(sap_ctx, NULL,
					  eSAP_DFS_CAC_START,
					  (void *)
					  eSAP_STATUS_SUCCESS);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		sap_err("Failed to indicate CAC START vdev %d",
			sap_ctx->sessionId);
		return;
	}

	qdf_status = sap_signal_hdd_event(sap_ctx, NULL,
					  eSAP_DFS_CAC_END,
					  (void *)
					  eSAP_STATUS_SUCCESS);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		sap_debug("Failed to indicate CAC End vdev %d",
			  sap_ctx->sessionId);
	}
}

static void sap_check_and_update_vdev_ch_params(struct sap_context *sap_ctx)
{
	struct wlan_channel *chan;
	enum phy_ch_width orig_ch_width;

	chan = wlan_vdev_get_active_channel(sap_ctx->vdev);
	if (!chan) {
		sap_debug("Couldn't get vdev active channel");
		return;
	}
	if (sap_ctx->ch_params.ch_width == chan->ch_width)
		return;

	orig_ch_width = sap_ctx->ch_params.ch_width;

	sap_ctx->ch_params.ch_width = chan->ch_width;
	sap_ctx->ch_params.center_freq_seg0 = chan->ch_freq_seg1;
	sap_ctx->ch_params.center_freq_seg1 = chan->ch_freq_seg2;
	sap_ctx->ch_params.mhz_freq_seg0 = chan->ch_cfreq1;
	sap_ctx->ch_params.mhz_freq_seg1 = chan->ch_cfreq2;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(sap_ctx->chan_freq) &&
	    (chan->ch_width == CH_WIDTH_40MHZ)) {
		if (sap_ctx->chan_freq < chan->ch_freq_seg1)
			sap_ctx->ch_params.sec_ch_offset = LOW_PRIMARY_CH;
		else
			sap_ctx->ch_params.sec_ch_offset = HIGH_PRIMARY_CH;
	}
	sap_debug("updated BW %d -> %d", orig_ch_width,
		  sap_ctx->ch_params.ch_width);
}

static qdf_freq_t sap_get_safe_channel_freq(struct sap_context *sap_ctx)
{
	qdf_freq_t freq;

	freq = wlan_pre_cac_get_freq_before_pre_cac(sap_ctx->vdev);
	if (!freq)
		freq = wlansap_get_safe_channel_from_pcl_and_acs_range(
								sap_ctx);

	sap_debug("new selected freq %d as target chan as current freq unsafe %d",
		  freq, sap_ctx->chan_freq);

	return freq;
}

/**
 * sap_fsm_send_csa_restart_req() - send csa start event
 * @mac_ctx: mac ctx
 * @sap_ctx: SAP context
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
sap_fsm_send_csa_restart_req(struct mac_context *mac_ctx,
			     struct sap_context *sap_ctx)
{
	QDF_STATUS status;

	status = policy_mgr_check_and_set_hw_mode_for_channel_switch(
				mac_ctx->psoc, sap_ctx->sessionId,
				mac_ctx->sap.SapDfsInfo.target_chan_freq,
				POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_SAP);

	/*
	 * If hw_mode_status is QDF_STATUS_E_FAILURE, mean HW
	 * mode change was required but driver failed to set HW
	 * mode so ignore CSA for the channel.
	 */
	if (status == QDF_STATUS_E_FAILURE) {
		sap_err("HW change required but failed to set hw mode");
		return status;
	}

	/*
	 * If hw_mode_status is QDF_STATUS_SUCCESS mean HW mode
	 * change was required and was successfully requested so
	 * the channel switch will continue after HW mode change
	 * completion.
	 */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		sap_info("Channel change will continue after HW mode change");
		return QDF_STATUS_SUCCESS;
	}

	return sme_csa_restart(mac_ctx, sap_ctx->sessionId);
}

/**
 * sap_fsm_validate_and_change_channel() - handle channel Avoid event event
 *                                         or channel list update during cac
 * @mac_ctx: global MAC context
 *
 * Return: QDF_STATUS
 */
static void sap_fsm_validate_and_change_channel(struct mac_context *mac_ctx,
						struct sap_context *sap_ctx)
{
	qdf_freq_t target_chan_freq;
	struct ch_params ch_params = {0};
	QDF_STATUS status;
	enum phy_ch_width target_bw = sap_ctx->ch_params.ch_width;

	if (((!sap_ctx->acs_cfg || !sap_ctx->acs_cfg->acs_mode) &&
	     target_psoc_get_sap_coex_fixed_chan_cap(
				wlan_psoc_get_tgt_if_handle(mac_ctx->psoc))) ||
	    (policy_mgr_is_sap_freq_allowed(mac_ctx->psoc, sap_ctx->chan_freq) &&
	     !wlan_reg_is_disable_for_pwrmode(mac_ctx->pdev, sap_ctx->chan_freq,
					      REG_CURRENT_PWR_MODE)))
		return;

	/*
	 * The selected channel is not safe channel. Hence,
	 * change the sap channel to a safe channel.
	 */
	target_chan_freq = sap_get_safe_channel_freq(sap_ctx);
	ch_params.ch_width = target_bw;
	target_bw = wlansap_get_csa_chanwidth_from_phymode(
			sap_ctx, target_chan_freq, &ch_params);
	sap_debug("sap vdev %d change to safe ch freq %d bw %d from unsafe %d",
		  sap_ctx->sessionId, target_chan_freq, target_bw,
		  sap_ctx->chan_freq);
	status = wlansap_set_channel_change_with_csa(
			sap_ctx, target_chan_freq, target_bw, false);
	if (QDF_IS_STATUS_ERROR(status))
		sap_err("SAP set channel failed for freq: %d, bw: %d",
			target_chan_freq, target_bw);
}

/**
 * sap_fsm_state_starting() - utility function called from sap fsm
 * @sap_ctx: SAP context
 * @sap_event: SAP event buffer
 * @mac_ctx: global MAC context
 * @mac_handle: Opaque handle to the global MAC context
 *
 * This function is called for state transition from "SAP_STARTING"
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_fsm_state_starting(struct sap_context *sap_ctx,
					 struct sap_sm_event *sap_event,
					 struct mac_context *mac_ctx,
					 mac_handle_t mac_handle)
{
	uint32_t msg = sap_event->event;
	struct csr_roam_info *roam_info =
		(struct csr_roam_info *) (sap_event->params);
	tSapDfsInfo *sap_dfs_info;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	uint8_t is_dfs = false;
	uint32_t sap_chan_freq;
	uint32_t ch_cfreq1 = 0;
	enum reg_wifi_band band;

	if (msg == eSAP_MAC_START_BSS_SUCCESS) {
		/*
		 * Update sap_ctx->ch_params from vdev to make up with any BW
		 * change in lower layer
		 */
		sap_check_and_update_vdev_ch_params(sap_ctx);

		/*
		 * Transition from SAP_STARTING to SAP_STARTED
		 * (both without substates)
		 */
		sap_ctx->fsm_state = SAP_STARTED;
		sap_debug("sap_fsm: vdev %d: SAP_STARTING => SAP_STARTED, freq %d ch_width %d",
			  sap_ctx->vdev_id, sap_ctx->chan_freq,
			  sap_ctx->ch_params.ch_width);

		if (sap_ctx->is_chan_change_inprogress) {
			/* SAP channel change request processing is completed */
			qdf_status = sap_signal_hdd_event(sap_ctx, roam_info,
						eSAP_CHANNEL_CHANGE_EVENT,
						(void *)eSAP_STATUS_SUCCESS);
			sap_ctx->is_chan_change_inprogress = false;
		} else {
			sap_debug("vdev %d notify hostapd about chan freq selection: %d",
				  sap_ctx->vdev_id, sap_ctx->chan_freq);
			qdf_status =
				sap_signal_hdd_event(sap_ctx, roam_info,
						     eSAP_CHANNEL_CHANGE_EVENT,
						     (void *)eSAP_STATUS_SUCCESS);
			/* Action code for transition */
			qdf_status = sap_signal_hdd_event(sap_ctx, roam_info,
					eSAP_START_BSS_EVENT,
					(void *) eSAP_STATUS_SUCCESS);
		}
		sap_chan_freq = sap_ctx->chan_freq;
		band = wlan_reg_freq_to_band(sap_ctx->chan_freq);
		if (sap_ctx->ch_params.center_freq_seg1)
			ch_cfreq1 = wlan_reg_chan_band_to_freq(
					mac_ctx->pdev,
					sap_ctx->ch_params.center_freq_seg1,
					BIT(band));

		/*
		 * The upper layers have been informed that AP is up and
		 * running, however, the AP is still not beaconing, until
		 * CAC is done if the operating channel is DFS
		 */
		if (sap_ctx->ch_params.ch_width == CH_WIDTH_160MHZ) {
			struct ch_params ch_params = {0};

			wlan_reg_set_create_punc_bitmap(&ch_params, true);
			ch_params.ch_width = CH_WIDTH_160MHZ;
			is_dfs =
			wlan_reg_get_5g_bonded_channel_state_for_pwrmode(mac_ctx->pdev,
									 sap_chan_freq,
									 &ch_params,
									 REG_CURRENT_PWR_MODE) ==
			CHANNEL_STATE_DFS;
		} else if (sap_ctx->ch_params.ch_width == CH_WIDTH_80P80MHZ) {
			if (wlan_reg_get_channel_state_for_pwrmode(
							mac_ctx->pdev,
							sap_chan_freq,
							REG_CURRENT_PWR_MODE) ==
			    CHANNEL_STATE_DFS ||
			    wlan_reg_get_channel_state_for_pwrmode(
							mac_ctx->pdev,
							ch_cfreq1,
							REG_CURRENT_PWR_MODE) ==
					CHANNEL_STATE_DFS)
				is_dfs = true;
		} else {
			/* Indoor channels are also marked DFS, therefore
			 * check if the channel has REGULATORY_CHAN_RADAR
			 * channel flag to identify if the channel is DFS
			 */
			if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						     sap_chan_freq))
				is_dfs = true;
		}
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ctx->chan_freq))
			is_dfs = false;

		sap_debug("vdev %d freq %d, is_dfs %d", sap_ctx->vdev_id,
			  sap_ctx->chan_freq, is_dfs);
		if (is_dfs) {
			sap_dfs_info = &mac_ctx->sap.SapDfsInfo;
			if ((false == sap_dfs_info->ignore_cac) &&
			    (eSAP_DFS_DO_NOT_SKIP_CAC ==
			    sap_dfs_info->cac_state) &&
			    !wlan_pre_cac_complete_get(sap_ctx->vdev) &&
			    policy_mgr_get_dfs_master_dynamic_enabled(
					mac_ctx->psoc,
					sap_ctx->sessionId)) {
				sap_ctx->fsm_state = SAP_STARTING;
				sap_debug("sap_fsm: vdev %d: SAP_STARTED => SAP_STARTING to start cac timer",
					  sap_ctx->vdev_id);
				qdf_status = sap_fsm_cac_start(sap_ctx, mac_ctx,
							       mac_handle);
			} else {
				sap_debug("vdev %d skip cac timer",
					  sap_ctx->vdev_id);
				sap_ctx->sap_radar_found_status = false;
				/*
				 * If hostapd starts AP on dfs channel,
				 * hostapd will wait for CAC START/CAC END
				 * event and finish AP start process.
				 * If we skip CAC timer, we will need to
				 * indicate the CAC event even though driver
				 * doesn't perform CAC.
				 */
				sap_propagate_cac_events(sap_ctx);

				wlansap_start_beacon_req(sap_ctx);
			}
		}
		/*
		 * During CSA, it might be possible that ch avoidance event to
		 * avoid the sap frequency is received. So, check after CSA,
		 * whether sap frequency is safe if not restart sap to a safe
		 * channel.
		 */
		sap_fsm_validate_and_change_channel(mac_ctx, sap_ctx);
	} else if (msg == eSAP_MAC_START_FAILS ||
		 msg == eSAP_HDD_STOP_INFRA_BSS) {
			qdf_status = sap_fsm_handle_start_failure(sap_ctx, msg,
								  mac_handle);
	} else if (msg == eSAP_OPERATING_CHANNEL_CHANGED) {
		/* The operating channel has changed, update hostapd */
		sap_ctx->chan_freq = mac_ctx->sap.SapDfsInfo.target_chan_freq;

		sap_ctx->fsm_state = SAP_STARTED;
		sap_debug("sap_fsm: vdev %d: SAP_STARTING => SAP_STARTED",
			  sap_ctx->vdev_id);

		/* Indicate change in the state to upper layers */
		qdf_status = sap_signal_hdd_event(sap_ctx, roam_info,
				  eSAP_START_BSS_EVENT,
				  (void *)eSAP_STATUS_SUCCESS);
	} else if (msg == eSAP_DFS_CHANNEL_CAC_RADAR_FOUND) {
		qdf_status = sap_fsm_handle_radar_during_cac(sap_ctx, mac_ctx);
	} else if (msg == eSAP_DFS_CHANNEL_CAC_END) {
		if (sap_ctx->vdev &&
		    wlan_util_vdev_mgr_get_cac_timeout_for_vdev(sap_ctx->vdev)) {
			qdf_status = sap_cac_end_notify(mac_handle, roam_info);
		} else {
			sap_debug("vdev %d cac duration is zero",
				  sap_ctx->vdev_id);
			qdf_status = QDF_STATUS_SUCCESS;
		}
	} else if (msg == eSAP_DFS_CHANNEL_CAC_START) {
		if (sap_ctx->is_chan_change_inprogress) {
			sap_signal_hdd_event(sap_ctx,
					     NULL,
					     eSAP_CHANNEL_CHANGE_EVENT,
					     (void *)eSAP_STATUS_SUCCESS);
			sap_ctx->is_chan_change_inprogress = false;
		}
		qdf_status = sap_fsm_cac_start(sap_ctx, mac_ctx, mac_handle);
	} else {
		sap_err("sap_fsm: vdev %d: SAP_STARTING, invalid event %d",
			sap_ctx->vdev_id, msg);
	}

	return qdf_status;
}

/**
 * sap_fsm_state_started() - utility function called from sap fsm
 * @sap_ctx: SAP context
 * @sap_event: SAP event buffer
 * @mac_ctx: global MAC context
 *
 * This function is called for state transition from "SAP_STARTED"
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_fsm_state_started(struct sap_context *sap_ctx,
					struct sap_sm_event *sap_event,
					struct mac_context *mac_ctx)
{
	uint32_t msg = sap_event->event;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	if (msg == eSAP_HDD_STOP_INFRA_BSS) {
		/*
		 * Transition from SAP_STARTED to SAP_STOPPING
		 * (both without substates)
		 */
		sap_ctx->fsm_state = SAP_STOPPING;
		sap_debug("sap_fsm: vdev %d: SAP_STARTED => SAP_STOPPING",
			  sap_ctx->vdev_id);
		qdf_status = sap_goto_stopping(sap_ctx);
	} else if (eSAP_DFS_CHNL_SWITCH_ANNOUNCEMENT_START == msg) {
		uint8_t intf;
		if (!mac_ctx->sap.SapDfsInfo.target_chan_freq) {
			sap_err("Invalid target channel freq %d",
				mac_ctx->sap.SapDfsInfo.target_chan_freq);
			return qdf_status;
		}

		/*
		 * Radar is seen on the current operating channel
		 * send CSA IE for all associated stations
		 * Request for CSA IE transmission
		 */
		for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
			struct sap_context *temp_sap_ctx;

			if (((QDF_SAP_MODE ==
				mac_ctx->sap.sapCtxList[intf].sapPersona) ||
			    (QDF_P2P_GO_MODE ==
				mac_ctx->sap.sapCtxList[intf].sapPersona)) &&
			    mac_ctx->sap.sapCtxList[intf].sap_context) {
				temp_sap_ctx =
				    mac_ctx->sap.sapCtxList[intf].sap_context;
				/*
				 * Radar won't come on non-dfs channel, so
				 * no need to move them
				 */
				if (!sap_operating_on_dfs(
						mac_ctx, temp_sap_ctx)) {
					sap_debug("vdev %d freq %d (state %d) is not DFS or disabled so continue",
						  temp_sap_ctx->sessionId,
						  temp_sap_ctx->chan_freq,
						  wlan_reg_get_channel_state_for_pwrmode(
						  mac_ctx->pdev,
						  temp_sap_ctx->chan_freq,
						  REG_CURRENT_PWR_MODE));
					continue;
				}
				sap_debug("vdev %d switch freq %d -> %d",
					  temp_sap_ctx->sessionId,
					  temp_sap_ctx->chan_freq,
					  mac_ctx->sap.SapDfsInfo.target_chan_freq);
				qdf_status =
				   sap_fsm_send_csa_restart_req(mac_ctx,
								temp_sap_ctx);
			}
		}
	} else {
		sap_err("sap_fsm: vdev %d: SAP_STARTED, invalid event %d",
			sap_ctx->vdev_id, msg);
	}

	return qdf_status;
}

/**
 * sap_fsm_state_stopping() - utility function called from sap fsm
 * @sap_ctx: SAP context
 * @sap_event: SAP event buffer
 * @mac_ctx: global MAC context
 *
 * This function is called for state transition from "SAP_STOPPING"
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
sap_fsm_state_stopping(struct sap_context *sap_ctx,
		       struct sap_sm_event *sap_event,
		       struct mac_context *mac_ctx,
		       mac_handle_t mac_handle)
{
	uint32_t msg = sap_event->event;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;

	if (msg == eSAP_MAC_READY_FOR_CONNECTIONS) {
		/*
		 * Transition from SAP_STOPPING to SAP_INIT
		 * (both without substates)
		 */
		sap_ctx->fsm_state = SAP_INIT;
		sap_debug("sap_fsm: vdev %d: SAP_STOPPING => SAP_INIT",
			  sap_ctx->vdev_id);

		/* Close the SME session */
		qdf_status = sap_signal_hdd_event(sap_ctx, NULL,
					eSAP_STOP_BSS_EVENT,
					(void *)eSAP_STATUS_SUCCESS);
	} else if (msg == eSAP_HDD_STOP_INFRA_BSS) {
		/*
		 * In case the SAP is already in stopping case and
		 * we get a STOP request, return success.
		 */
		sap_debug("vdev %d SAP already in Stopping state",
			  sap_ctx->vdev_id);
		qdf_status = QDF_STATUS_SUCCESS;
	} else {
		sap_err("sap_fsm: vdev %d: SAP_STOPPING, invalid event %d",
			sap_ctx->vdev_id, msg);
	}

	return qdf_status;
}

/**
 * sap_fsm() - SAP statem machine entry function
 * @sap_ctx: SAP context
 * @sap_event: SAP event
 *
 * SAP state machine entry function
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sap_fsm(struct sap_context *sap_ctx, struct sap_sm_event *sap_event)
{
	/*
	 * Retrieve the phy link state machine structure
	 * from the sap_ctx value
	 * state var that keeps track of state machine
	 */
	enum sap_fsm_state state_var = sap_ctx->fsm_state;
	uint32_t msg = sap_event->event; /* State machine input event message */
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAILURE;
	}
	mac_handle = MAC_HANDLE(mac_ctx);

	sap_debug("vdev %d: state %d event %d", sap_ctx->vdev_id, state_var,
		  msg);

	switch (state_var) {
	case SAP_INIT:
		qdf_status = sap_fsm_state_init(sap_ctx, sap_event,
						mac_ctx, mac_handle);
		break;

	case SAP_STARTING:
		qdf_status = sap_fsm_state_starting(sap_ctx, sap_event,
						    mac_ctx, mac_handle);
		break;

	case SAP_STARTED:
		qdf_status = sap_fsm_state_started(sap_ctx, sap_event,
						   mac_ctx);
		break;

	case SAP_STOPPING:
		qdf_status = sap_fsm_state_stopping(sap_ctx, sap_event,
						    mac_ctx, mac_handle);
		break;
	}
	return qdf_status;
}

void sap_sort_mac_list(struct qdf_mac_addr *macList, uint16_t size)
{
	uint16_t outer, inner;
	struct qdf_mac_addr temp;
	int32_t nRes = -1;

	if ((!macList) || (size > MAX_ACL_MAC_ADDRESS)) {
		sap_err("either buffer is NULL or size = %d is more", size);
		return;
	}

	for (outer = 0; outer < size; outer++) {
		for (inner = 0; inner < size - 1; inner++) {
			nRes =
				qdf_mem_cmp((macList + inner)->bytes,
						 (macList + inner + 1)->bytes,
						 QDF_MAC_ADDR_SIZE);
			if (nRes > 0) {
				qdf_mem_copy(temp.bytes,
					     (macList + inner + 1)->bytes,
					     QDF_MAC_ADDR_SIZE);
				qdf_mem_copy((macList + inner + 1)->bytes,
					     (macList + inner)->bytes,
					     QDF_MAC_ADDR_SIZE);
				qdf_mem_copy((macList + inner)->bytes,
					     temp.bytes, QDF_MAC_ADDR_SIZE);
			}
		}
	}
}

bool
sap_search_mac_list(struct qdf_mac_addr *macList,
		    uint16_t num_mac, uint8_t *peerMac,
		    uint16_t *index)
{
	int32_t nRes = -1, nStart = 0, nEnd, nMiddle;

	nEnd = num_mac - 1;

	if ((!macList) || (num_mac > MAX_ACL_MAC_ADDRESS)) {
		sap_err("either buffer is NULL or size = %d is more", num_mac);
		return false;
	}

	while (nStart <= nEnd) {
		nMiddle = (nStart + nEnd) / 2;
		nRes =
			qdf_mem_cmp(&macList[nMiddle], peerMac,
					 QDF_MAC_ADDR_SIZE);

		if (0 == nRes) {
			sap_debug("search SUCC");
			/* "index equals NULL" means the caller does not need the */
			/* index value of the peerMac being searched */
			if (index) {
				*index = (uint16_t)nMiddle;
				sap_debug("index %d", *index);
			}
			return true;
		}
		if (nRes < 0)
			nStart = nMiddle + 1;
		else
			nEnd = nMiddle - 1;
	}

	sap_debug("search not succ");
	return false;
}

void sap_add_mac_to_acl(struct qdf_mac_addr *macList,
			uint16_t *size, uint8_t *peerMac)
{
	int32_t nRes = -1;
	int i;

	sap_debug("add acl entered");

	if (!macList || *size > MAX_ACL_MAC_ADDRESS) {
		sap_debug("either buffer is NULL or size = %d is incorrect",
			  *size);
		return;
	}

	for (i = ((*size) - 1); i >= 0; i--) {
		nRes =
			qdf_mem_cmp(&macList[i], peerMac, QDF_MAC_ADDR_SIZE);
		if (nRes > 0) {
			/* Move alphabetically greater mac addresses one index down to allow for insertion
			   of new mac in sorted order */
			qdf_mem_copy((macList + i + 1)->bytes,
				     (macList + i)->bytes, QDF_MAC_ADDR_SIZE);
		} else {
			break;
		}
	}
	/* This should also take care of if the element is the first to be added in the list */
	qdf_mem_copy((macList + i + 1)->bytes, peerMac, QDF_MAC_ADDR_SIZE);
	/* increment the list size */
	(*size)++;
}

void sap_remove_mac_from_acl(struct qdf_mac_addr *macList,
			     uint16_t *size, uint16_t index)
{
	int i;

	sap_debug("remove acl entered");
	/*
	 * Return if the list passed is empty. Ideally this should never happen
	 * since this funcn is always called after sap_search_mac_list to get
	 * the index of the mac addr to be removed and this will only get
	 * called if the search is successful. Still no harm in having the check
	 */
	if ((!macList) || (*size == 0) ||
					(*size > MAX_ACL_MAC_ADDRESS)) {
		sap_err("either buffer is NULL or size %d is incorrect",
			 *size);
		return;
	}
	for (i = index; i < ((*size) - 1); i++) {
		/* Move mac addresses starting from "index" passed one index up to delete the void
		   created by deletion of a mac address in ACL */
		qdf_mem_copy((macList + i)->bytes, (macList + i + 1)->bytes,
			     QDF_MAC_ADDR_SIZE);
	}
	/* The last space should be made empty since all mac addresses moved one step up */
	qdf_mem_zero((macList + (*size) - 1)->bytes, QDF_MAC_ADDR_SIZE);
	/* reduce the list size by 1 */
	(*size)--;
}

void sap_print_acl(struct qdf_mac_addr *macList, uint16_t size)
{
	uint16_t i;
	uint8_t *macArray;

	sap_debug("print acl entered");

	if ((!macList) || (size == 0) || (size > MAX_ACL_MAC_ADDRESS)) {
		sap_err("Either buffer is NULL or size %d is incorrect", size);
		return;
	}

	for (i = 0; i < size; i++) {
		macArray = (macList + i)->bytes;
		sap_debug("** ACL entry %i - " QDF_MAC_ADDR_FMT, i,
			  QDF_MAC_ADDR_REF(macArray));
	}
	return;
}

QDF_STATUS sap_is_peer_mac_allowed(struct sap_context *sap_ctx,
				   uint8_t *peerMac)
{
	if (eSAP_ALLOW_ALL == sap_ctx->eSapMacAddrAclMode)
		return QDF_STATUS_SUCCESS;

	if (sap_search_mac_list
		    (sap_ctx->acceptMacList, sap_ctx->nAcceptMac, peerMac, NULL))
		return QDF_STATUS_SUCCESS;

	if (sap_search_mac_list
		    (sap_ctx->denyMacList, sap_ctx->nDenyMac, peerMac, NULL)) {
		sap_err("Peer " QDF_MAC_ADDR_FMT " in deny list",
			 QDF_MAC_ADDR_REF(peerMac));
		return QDF_STATUS_E_FAILURE;
	}
	/* A new station CAN associate, unless in deny list. Less stringent mode */
	if (eSAP_ACCEPT_UNLESS_DENIED == sap_ctx->eSapMacAddrAclMode)
		return QDF_STATUS_SUCCESS;

	/* A new station CANNOT associate, unless in accept list. More stringent mode */
	if (eSAP_DENY_UNLESS_ACCEPTED == sap_ctx->eSapMacAddrAclMode) {
		sap_debug("Peer " QDF_MAC_ADDR_FMT
			  " denied, Mac filter mode is eSAP_DENY_UNLESS_ACCEPTED",
			  QDF_MAC_ADDR_REF(peerMac));
		return QDF_STATUS_E_FAILURE;
	}

	/* The new STA is neither in accept list nor in deny list. In this case, deny the association
	 * but send a wifi event notification indicating the mac address being denied
	 */
	if (eSAP_SUPPORT_ACCEPT_AND_DENY == sap_ctx->eSapMacAddrAclMode) {
		sap_signal_hdd_event(sap_ctx, NULL, eSAP_UNKNOWN_STA_JOIN,
				     (void *) peerMac);
		sap_debug("Peer " QDF_MAC_ADDR_FMT
			  " denied, Mac filter mode is eSAP_SUPPORT_ACCEPT_AND_DENY",
			  QDF_MAC_ADDR_REF(peerMac));
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

void sap_dump_acs_channel(struct sap_acs_cfg *acs_cfg)
{
	uint32_t buf_len = 0, len = 0, i;
	uint8_t *chan_buff = NULL;

	/*
	 * Buffer of (num channel * 5) + 1  to consider the 4 char freq
	 * and 1 space after it for each channel and 1 to end the string
	 * with NULL.
	 */
	buf_len = (acs_cfg->ch_list_count * 5) + 1;
	chan_buff = qdf_mem_malloc(buf_len);
	if (!chan_buff)
		return;

	for (i = 0; i < acs_cfg->ch_list_count; i++)
		len += qdf_scnprintf(chan_buff + len, buf_len - len,
				     " %d", acs_cfg->freq_list[i]);

	sap_nofl_debug("ACS freq list[%d]:%s",
		       acs_cfg->ch_list_count, chan_buff);
	qdf_mem_free(chan_buff);
}

#ifdef SOFTAP_CHANNEL_RANGE
/**
 * sap_get_freq_list() - get the list of channel frequency
 * @sap_ctx: sap context
 * @freq_list: pointer to channel list array
 * @num_ch: pointer to number of channels.
 *
 * This function populates the list of channel frequency for scanning.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_get_freq_list(struct sap_context *sap_ctx,
				    uint32_t **freq_list,
				    uint8_t *num_ch)
{
	uint8_t loop_count;
	uint32_t *list;
	uint8_t ch_count;
	uint8_t dfs_master_enable;
	uint32_t start_ch_freq, band_start_ch;
	uint32_t end_ch_freq, band_end_ch;
	uint32_t en_lte_coex;
	struct mac_context *mac_ctx;
	uint16_t ch_width;
	uint8_t normalize_factor = 100;
	uint32_t chan_freq;
	struct acs_weight *weight_list;
	struct acs_weight_range *range_list;
	bool freq_present_in_list = false;
	uint8_t i;
	bool srd_chan_enabled;
	enum QDF_OPMODE vdev_opmode;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		*num_ch = 0;
		*freq_list = NULL;
		return QDF_STATUS_E_FAULT;
	}

	weight_list = mac_ctx->mlme_cfg->acs.normalize_weight_chan;
	range_list = mac_ctx->mlme_cfg->acs.normalize_weight_range;

	dfs_master_enable = mac_ctx->mlme_cfg->dfs_cfg.dfs_master_capable;
	if (sap_ctx->dfs_mode == ACS_DFS_MODE_DISABLE)
		dfs_master_enable = false;

	start_ch_freq = sap_ctx->acs_cfg->start_ch_freq;
	end_ch_freq = sap_ctx->acs_cfg->end_ch_freq;
	ch_width = sap_ctx->acs_cfg->ch_width;

	sap_debug("startChannel %d, EndChannel %d, ch_width %d, HW:%d",
		  start_ch_freq, end_ch_freq, ch_width,
		  sap_ctx->acs_cfg->hw_mode);

	wlansap_extend_to_acs_range(MAC_HANDLE(mac_ctx),
				    &start_ch_freq, &end_ch_freq,
				    &band_start_ch, &band_end_ch);

	sap_debug("expanded startChannel %d,EndChannel %d band_start_ch %d, band_end_ch %d",
		  start_ch_freq, end_ch_freq, band_start_ch, band_end_ch);

	en_lte_coex = mac_ctx->mlme_cfg->sap_cfg.enable_lte_coex;

	/* Check if LTE coex is enabled and 2.4GHz is selected */
	if (en_lte_coex && (band_start_ch == CHAN_ENUM_2412) &&
	    (band_end_ch == CHAN_ENUM_2484)) {
		/* Set 2.4GHz upper limit to channel 9 for LTE COEX */
		band_end_ch = CHAN_ENUM_2452;
	}

	/* Allocate the max number of channel supported */
	list = qdf_mem_malloc((NUM_CHANNELS) * sizeof(uint32_t));
	if (!list) {
		*num_ch = 0;
		*freq_list = NULL;
		return QDF_STATUS_E_NOMEM;
	}

	/* Search for the Active channels in the given range */
	ch_count = 0;
	for (loop_count = band_start_ch; loop_count <= band_end_ch;
	     loop_count++) {
		chan_freq = WLAN_REG_CH_TO_FREQ(loop_count);

		/* go to next channel if rf_channel is out of range */
		if (start_ch_freq > WLAN_REG_CH_TO_FREQ(loop_count) ||
		    end_ch_freq < WLAN_REG_CH_TO_FREQ(loop_count))
			continue;
		/*
		 * go to next channel if none of these condition pass
		 * - DFS scan enabled and chan not in CHANNEL_STATE_DISABLE
		 * - DFS scan disable but chan in CHANNEL_STATE_ENABLE
		 */
		if (!(((true == mac_ctx->scan.fEnableDFSChnlScan) &&
		      wlan_reg_get_channel_state_from_secondary_list_for_freq(
			mac_ctx->pdev, WLAN_REG_CH_TO_FREQ(loop_count)))
		      ||
		    ((false == mac_ctx->scan.fEnableDFSChnlScan) &&
		     (CHANNEL_STATE_ENABLE ==
		      wlan_reg_get_channel_state_from_secondary_list_for_freq(
			mac_ctx->pdev, WLAN_REG_CH_TO_FREQ(loop_count)))
		     )))
			continue;

		/* check if the channel is in NOL denylist */
		if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(WLAN_REG_CH_TO_FREQ(
					loop_count))) {
			if (sap_dfs_is_channel_in_nol_list(
					sap_ctx,
					chan_freq,
					PHY_SINGLE_CHANNEL_CENTERED)) {
				sap_debug("Ch freq %d in NOL list", chan_freq);
				continue;
			}
		}
		/* Skip DSRC channels */
		if (wlan_reg_is_dsrc_freq(WLAN_REG_CH_TO_FREQ(loop_count)))
			continue;

		/*
		 * Skip the channels which are not in ACS config from user
		 * space
		 */
		if (!wlansap_is_channel_present_in_acs_list(
					chan_freq,
					sap_ctx->acs_cfg->freq_list,
					sap_ctx->acs_cfg->ch_list_count))
			continue;
		/* Dont scan DFS channels in case of MCC disallowed
		 * As it can result in SAP starting on DFS channel
		 * resulting  MCC on DFS channel
		 */
		if (wlan_reg_is_dfs_in_secondary_list_for_freq(
					mac_ctx->pdev,
					WLAN_REG_CH_TO_FREQ(loop_count))) {
			if (!dfs_master_enable)
				continue;
			if (wlansap_dcs_is_wlan_interference_mitigation_enabled(
								sap_ctx))
				sap_debug("dfs chan_freq %d added when dcs enabled",
					  WLAN_REG_CH_TO_FREQ(loop_count));
			else if (policy_mgr_disallow_mcc(
					mac_ctx->psoc,
					WLAN_REG_CH_TO_FREQ(loop_count)))
				continue;
			normalize_factor =
				MLME_GET_DFS_CHAN_WEIGHT(
				mac_ctx->mlme_cfg->acs.np_chan_weightage);
			freq_present_in_list = true;
		}

		vdev_opmode = wlan_vdev_mlme_get_opmode(sap_ctx->vdev);
		wlan_mlme_get_srd_master_mode_for_vdev(mac_ctx->psoc,
						       vdev_opmode,
						       &srd_chan_enabled);

		if (!srd_chan_enabled &&
		    wlan_reg_is_etsi13_srd_chan_for_freq(mac_ctx->pdev,
					WLAN_REG_CH_TO_FREQ(loop_count))) {
			sap_debug("vdev opmode %d not allowed on SRD freq %d",
				  vdev_opmode, WLAN_REG_CH_TO_FREQ(loop_count));
			continue;
		}

		/* Check if the freq is present in range list */
		for (i = 0; i < mac_ctx->mlme_cfg->acs.num_weight_range; i++) {
			if (chan_freq >= range_list[i].start_freq &&
			    chan_freq <= range_list[i].end_freq) {
				normalize_factor =
					range_list[i].normalize_weight;
				sap_debug("Range list, freq %d normalize weight factor %d",
					  chan_freq, normalize_factor);
				freq_present_in_list = true;
			}
		}

		for (i = 0;
		     i < mac_ctx->mlme_cfg->acs.normalize_weight_num_chan;
		     i++) {
			if (chan_freq == weight_list[i].chan_freq) {
				normalize_factor =
					weight_list[i].normalize_weight;
				sap_debug("freq %d normalize weight factor %d",
					  chan_freq, normalize_factor);
				freq_present_in_list = true;
			}
		}

		/* This would mean that the user does not want this freq */
		if (freq_present_in_list && !normalize_factor) {
			sap_debug("chan_freq %d ecluded normalize weight 0",
				  chan_freq);
			freq_present_in_list = false;
			continue;
		}
#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
		if (sap_ctx->acs_cfg->skip_scan_status ==
			eSAP_DO_PAR_ACS_SCAN) {
			uint32_t ch_freq;

			ch_freq = WLAN_REG_CH_TO_FREQ(loop_count);
			if ((ch_freq >=
				sap_ctx->acs_cfg->skip_scan_range1_stch &&
			      ch_freq <=
				sap_ctx->acs_cfg->skip_scan_range1_endch) ||
			     (ch_freq >=
				sap_ctx->acs_cfg->skip_scan_range2_stch &&
			      ch_freq <=
				sap_ctx->acs_cfg->skip_scan_range2_endch)) {
				list[ch_count] =
					WLAN_REG_CH_TO_FREQ(loop_count);
				ch_count++;
				sap_debug("%d %d added to ACS ch range",
					  ch_count, ch_freq);
			} else {
				sap_debug("%d %d skipped from ACS ch range",
					  ch_count, ch_freq);
			}
		} else {
			list[ch_count] = WLAN_REG_CH_TO_FREQ(loop_count);
			ch_count++;
			sap_debug("%d added to ACS ch range", ch_count);
		}
#else
		list[ch_count] = WLAN_REG_CH_TO_FREQ(loop_count);
		ch_count++;
#endif
	}
	if (!ch_count) {
		sap_info("No active channels present for the current region");
		/*
		 * LTE COEX: channel range outside the restricted 2.4GHz
		 * band limits
		 */
		if (en_lte_coex &&
		    start_ch_freq > WLAN_REG_CH_TO_FREQ(band_end_ch))
			sap_info("SAP can't be started as due to LTE COEX");
	}

	/* return the channel list and number of channels to scan */
	*num_ch = ch_count;
	if (ch_count != 0) {
		*freq_list = list;
	} else {
		*freq_list = NULL;
		qdf_mem_free(list);
		return QDF_STATUS_SUCCESS;
	}

	for (loop_count = 0; loop_count < ch_count; loop_count++) {
		sap_ctx->acs_cfg->freq_list[loop_count] = list[loop_count];
	}
	sap_ctx->acs_cfg->ch_list_count = ch_count;
	sap_dump_acs_channel(sap_ctx->acs_cfg);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef DFS_COMPONENT_ENABLE
qdf_freq_t sap_indicate_radar(struct sap_context *sap_ctx)
{
	qdf_freq_t chan_freq = 0;
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("null sap_ctx");
		return 0;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return 0;
	}

	/*
	 * SAP needs to generate Channel Switch IE
	 * if the radar is found in the STARTED state
	 */
	if (sap_ctx->fsm_state == SAP_STARTED)
		mac->sap.SapDfsInfo.csaIERequired = true;

	if (mac->mlme_cfg->dfs_cfg.dfs_disable_channel_switch)
		return sap_ctx->chan_freq;

	/* set the Radar Found flag in SapDfsInfo */
	sap_ctx->sap_radar_found_status = true;

	chan_freq = wlan_pre_cac_get_freq_before_pre_cac(sap_ctx->vdev);
	if (chan_freq) {
		sap_info("sapdfs: set chan freq before pre cac %d as target chan",
			 chan_freq);
		return chan_freq;
	}

	if (sap_ctx->vendor_acs_dfs_lte_enabled && (QDF_STATUS_SUCCESS ==
	    sap_signal_hdd_event(sap_ctx, NULL, eSAP_DFS_NEXT_CHANNEL_REQ,
	    (void *) eSAP_STATUS_SUCCESS)))
		return 0;

	chan_freq = sap_random_channel_sel(sap_ctx);
	if (!chan_freq)
		sap_signal_hdd_event(sap_ctx, NULL,
		eSAP_DFS_NO_AVAILABLE_CHANNEL, (void *) eSAP_STATUS_SUCCESS);

	sap_warn("sapdfs: New selected target freq is [%d]", chan_freq);

	return chan_freq;
}
#endif

/*
 * CAC timer callback function.
 * Post eSAP_DFS_CHANNEL_CAC_END event to sap_fsm().
 */
void sap_dfs_cac_timer_callback(void *data)
{
	struct sap_context *sap_ctx;
	struct sap_sm_event sap_event;
	mac_handle_t mac_handle = data;
	struct mac_context *mac;

	if (!mac_handle) {
		sap_err("Invalid mac_handle");
		return;
	}
	mac = MAC_CONTEXT(mac_handle);
	sap_ctx = sap_find_cac_wait_session(mac_handle);
	if (!sap_ctx) {
		sap_err("no SAP contexts in wait state");
		return;
	}

	/*
	 * SAP may not be in CAC wait state, when the timer runs out.
	 * if following flag is set, then timer is in initialized state,
	 * destroy timer here.
	 */
	if (mac->sap.SapDfsInfo.is_dfs_cac_timer_running == true) {
		if (!sap_ctx->dfs_cac_offload)
			qdf_mc_timer_destroy(
				&mac->sap.SapDfsInfo.sap_dfs_cac_timer);
		mac->sap.SapDfsInfo.is_dfs_cac_timer_running = false;
	}

	/*
	 * CAC Complete, post eSAP_DFS_CHANNEL_CAC_END to sap_fsm
	 */
	sap_debug("sapdfs: Sending eSAP_DFS_CHANNEL_CAC_END for target_chan_freq = %d on sapctx[%pK]",
		  sap_ctx->chan_freq, sap_ctx);

	sap_event.event = eSAP_DFS_CHANNEL_CAC_END;
	sap_event.params = 0;
	sap_event.u1 = 0;
	sap_event.u2 = 0;

	sap_fsm(sap_ctx, &sap_event);
}

/*
 * Function to stop the DFS CAC Timer
 */
static int sap_stop_dfs_cac_timer(struct sap_context *sap_ctx)
{
	struct mac_context *mac;

	if (!sap_ctx)
		return 0;

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return 0;
	}

	if (sap_ctx->dfs_cac_offload) {
		mac->sap.SapDfsInfo.is_dfs_cac_timer_running = 0;
		return 0;
	}

	if (QDF_TIMER_STATE_RUNNING !=
	    qdf_mc_timer_get_current_state(&mac->sap.SapDfsInfo.
					   sap_dfs_cac_timer)) {
		return 0;
	}

	qdf_mc_timer_stop(&mac->sap.SapDfsInfo.sap_dfs_cac_timer);
	mac->sap.SapDfsInfo.is_dfs_cac_timer_running = 0;
	qdf_mc_timer_destroy(&mac->sap.SapDfsInfo.sap_dfs_cac_timer);

	return 0;
}

/*
 * Function to start the DFS CAC Timer
 * when SAP is started on a DFS channel
 */
static int sap_start_dfs_cac_timer(struct sap_context *sap_ctx)
{
	QDF_STATUS status;
	uint32_t cac_dur;
	struct mac_context *mac;
	enum dfs_reg dfs_region;

	if (!sap_ctx) {
		sap_err("null sap_ctx");
		return 0;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return 0;
	}

	if (sap_ctx->dfs_cac_offload) {
		sap_debug("cac timer offloaded to firmware");
		mac->sap.SapDfsInfo.is_dfs_cac_timer_running = true;
		return 1;
	}

	sap_get_cac_dur_dfs_region(sap_ctx, &cac_dur, &dfs_region,
				   sap_ctx->chan_freq, &sap_ctx->ch_params);
	if (0 == cac_dur)
		return 0;

#ifdef QCA_WIFI_EMULATION
	cac_dur = cac_dur / 100;
#endif
	sap_debug("sapdfs: SAP_DFS_CHANNEL_CAC_START on CH freq %d, CAC_DUR-%d sec",
		  sap_ctx->chan_freq, cac_dur / 1000);

	qdf_mc_timer_init(&mac->sap.SapDfsInfo.sap_dfs_cac_timer,
			  QDF_TIMER_TYPE_SW,
			  sap_dfs_cac_timer_callback, MAC_HANDLE(mac));

	/* Start the CAC timer */
	status = qdf_mc_timer_start(&mac->sap.SapDfsInfo.sap_dfs_cac_timer,
			cac_dur);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to start cac timer");
		goto destroy_timer;
	}

	mac->sap.SapDfsInfo.is_dfs_cac_timer_running = true;

	return 0;

destroy_timer:
	mac->sap.SapDfsInfo.is_dfs_cac_timer_running = false;
	qdf_mc_timer_destroy(&mac->sap.SapDfsInfo.sap_dfs_cac_timer);

	return 1;
}

/*
 * This function initializes the NOL list
 * parameters required to track the radar
 * found DFS channels in the current Reg. Domain .
 */
QDF_STATUS sap_init_dfs_channel_nol_list(struct sap_context *sap_ctx)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	utils_dfs_init_nol(mac->pdev);

	return QDF_STATUS_SUCCESS;
}

/**
 * sap_is_active() - Check SAP active or not by sap_context object
 * @sap_ctx: Pointer to the SAP context
 *
 * Return: true if SAP is active
 */
static bool sap_is_active(struct sap_context *sap_ctx)
{
	return sap_ctx->fsm_state != SAP_INIT;
}

/*
 * This function will calculate how many interfaces
 * have sap persona and returns total number of sap persona.
 */
uint8_t sap_get_total_number_sap_intf(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	uint8_t intf = 0;
	uint8_t intf_count = 0;
	struct sap_context *sap_context;

	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		if (((QDF_SAP_MODE == mac->sap.sapCtxList[intf].sapPersona)
		    ||
		    (QDF_P2P_GO_MODE == mac->sap.sapCtxList[intf].sapPersona))
		    && mac->sap.sapCtxList[intf].sap_context) {
			sap_context =
				mac->sap.sapCtxList[intf].sap_context;
			if (!sap_is_active(sap_context))
				continue;
			intf_count++;
		}
	}
	return intf_count;
}

/**
 * is_concurrent_sap_ready_for_channel_change() - to check all saps are ready
 *						  for channel change
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_ctx: sap context for which this function has been called
 *
 * This function will find the concurrent sap context apart from
 * passed sap context and return its channel change ready status
 *
 *
 * Return: true if other SAP personas are ready to channel switch else false
 */
bool is_concurrent_sap_ready_for_channel_change(mac_handle_t mac_handle,
						struct sap_context *sap_ctx)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct sap_context *sap_context;
	uint8_t intf = 0;

	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		if (((QDF_SAP_MODE == mac->sap.sapCtxList[intf].sapPersona)
		    ||
		    (QDF_P2P_GO_MODE == mac->sap.sapCtxList[intf].sapPersona))
		    && mac->sap.sapCtxList[intf].sap_context) {
			sap_context =
				mac->sap.sapCtxList[intf].sap_context;
			if (!sap_is_active(sap_context))
				continue;
			if (sap_context == sap_ctx) {
				sap_err("sapCtx matched [%pK]", sap_ctx);
				continue;
			} else {
				sap_err("concurrent sapCtx[%pK] didn't matche with [%pK]",
					 sap_context, sap_ctx);
				return sap_context->is_sap_ready_for_chnl_chng;
			}
		}
	}
	return false;
}

/**
 * sap_is_conc_sap_doing_scc_dfs() - check if conc SAPs are doing SCC DFS
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_context: current SAP persona's channel
 *
 * If provided SAP's channel is DFS then Loop through each SAP or GO persona and
 * check if other beaconing entity's channel is same DFS channel. If they are
 * same then concurrent sap is doing SCC DFS.
 *
 * Return: true if two or more beaconing entities doing SCC DFS else false
 */
bool sap_is_conc_sap_doing_scc_dfs(mac_handle_t mac_handle,
				   struct sap_context *given_sapctx)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	struct sap_context *sap_ctx;
	uint8_t intf = 0, scc_dfs_counter = 0;
	qdf_freq_t ch_freq;

	ch_freq = given_sapctx->chan_freq;
	/*
	 * current SAP persona's channel itself is not DFS, so no need to check
	 * what other persona's channel is
	 */
	if (!wlan_reg_is_dfs_for_freq(mac->pdev,
				      ch_freq)) {
		sap_debug("skip this loop as provided channel is non-dfs");
		return false;
	}

	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		if ((QDF_SAP_MODE != mac->sap.sapCtxList[intf].sapPersona) &&
		    (QDF_P2P_GO_MODE != mac->sap.sapCtxList[intf].sapPersona))
			continue;
		if (!mac->sap.sapCtxList[intf].sap_context)
			continue;
		sap_ctx = mac->sap.sapCtxList[intf].sap_context;
		if (!sap_is_active(sap_ctx))
			continue;
		/* if same SAP contexts then skip to next context */
		if (sap_ctx == given_sapctx)
			continue;
		if (given_sapctx->chan_freq == sap_ctx->chan_freq)
			scc_dfs_counter++;
	}

	/* Found atleast two of the beaconing entities doing SCC DFS */
	if (scc_dfs_counter)
		return true;

	return false;
}

/**
 * sap_build_start_bss_config() - Fill the start bss request for SAP
 * @sap_bss_cfg: start bss config
 * @config: sap config
 *
 * This function fills the start bss request for SAP
 *
 * Return: None
 */
void
sap_build_start_bss_config(struct start_bss_config *sap_bss_cfg,
			   struct sap_config *config)
{
	qdf_mem_zero(&sap_bss_cfg->ssId.ssId, sizeof(sap_bss_cfg->ssId.ssId));
	sap_bss_cfg->ssId.length = config->SSIDinfo.ssid.length;
	qdf_mem_copy(&sap_bss_cfg->ssId.ssId, config->SSIDinfo.ssid.ssId,
		     config->SSIDinfo.ssid.length);

	if (config->authType == eSAP_SHARED_KEY)
		sap_bss_cfg->authType = eSIR_SHARED_KEY;
	else if (config->authType == eSAP_OPEN_SYSTEM)
		sap_bss_cfg->authType = eSIR_OPEN_SYSTEM;
	else
		sap_bss_cfg->authType = eSIR_AUTO_SWITCH;

	sap_bss_cfg->beaconInterval = (uint16_t)config->beacon_int;
	sap_bss_cfg->privacy = config->privacy;
	sap_bss_cfg->ssidHidden = config->SSIDinfo.ssidHidden;
	sap_bss_cfg->dtimPeriod = config->dtim_period;
	sap_bss_cfg->wps_state = config->wps_state;
	sap_bss_cfg->beacon_tx_rate = config->beacon_tx_rate;

	/* RSNIE */
	sap_bss_cfg->rsnIE.length = config->RSNWPAReqIELength;
	if (config->RSNWPAReqIELength)
		qdf_mem_copy(sap_bss_cfg->rsnIE.rsnIEdata,
			     config->RSNWPAReqIE, config->RSNWPAReqIELength);

	/* Probe response IE */
	if (config->probeRespIEsBufferLen > 0 &&
	    config->pProbeRespIEsBuffer) {
		sap_bss_cfg->add_ie_params.probeRespDataLen =
						config->probeRespIEsBufferLen;
		sap_bss_cfg->add_ie_params.probeRespData_buff =
						config->pProbeRespIEsBuffer;
	} else {
		sap_bss_cfg->add_ie_params.probeRespDataLen = 0;
		sap_bss_cfg->add_ie_params.probeRespData_buff = NULL;
	}

	/*assoc resp IE */
	if (config->assocRespIEsLen > 0 && config->pAssocRespIEsBuffer) {
		sap_bss_cfg->add_ie_params.assocRespDataLen =
						config->assocRespIEsLen;
		sap_bss_cfg->add_ie_params.assocRespData_buff =
						config->pAssocRespIEsBuffer;
	} else {
		sap_bss_cfg->add_ie_params.assocRespDataLen = 0;
		sap_bss_cfg->add_ie_params.assocRespData_buff = NULL;
	}

	if (config->probeRespBcnIEsLen > 0 &&
	    config->pProbeRespBcnIEsBuffer) {
		sap_bss_cfg->add_ie_params.probeRespBCNDataLen =
						config->probeRespBcnIEsLen;
		sap_bss_cfg->add_ie_params.probeRespBCNData_buff =
						config->pProbeRespBcnIEsBuffer;
	} else {
		sap_bss_cfg->add_ie_params.probeRespBCNDataLen = 0;
		sap_bss_cfg->add_ie_params.probeRespBCNData_buff = NULL;
	}

	if (config->supported_rates.numRates) {
		qdf_mem_copy(sap_bss_cfg->operationalRateSet.rate,
			     config->supported_rates.rate,
			     config->supported_rates.numRates);
		sap_bss_cfg->operationalRateSet.numRates =
					config->supported_rates.numRates;
	}

	if (config->extended_rates.numRates) {
		qdf_mem_copy(sap_bss_cfg->extendedRateSet.rate,
			     config->extended_rates.rate,
			     config->extended_rates.numRates);
		sap_bss_cfg->extendedRateSet.numRates =
				config->extended_rates.numRates;
	}

	return;
}
