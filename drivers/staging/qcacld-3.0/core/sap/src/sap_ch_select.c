/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/*===========================================================================

			s a p C h S e l e c t . C
   OVERVIEW:

   This software unit holds the implementation of the WLAN SAP modules
   functions for channel selection.

   DEPENDENCIES:

   Are listed for each API below.
   ===========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "qdf_trace.h"
#include "csr_api.h"
#include "sme_api.h"
#include "sap_ch_select.h"
#include "sap_internal.h"
#ifdef ANI_OS_TYPE_QNX
#include "stdio.h"
#endif
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
#include "lim_utils.h"
#include "parser_api.h"
#include <wlan_utility.h>
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
#include "cds_utils.h"
#include "pld_common.h"
#include "wlan_reg_services_api.h"
#include <wlan_scan_utils_api.h>
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_policy_mgr_api.h>

/*--------------------------------------------------------------------------
   Function definitions
   --------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
   Defines
   --------------------------------------------------------------------------*/
#define SAP_DEBUG

#define IS_RSSI_VALID(extRssi, rssi) \
	( \
		((extRssi < rssi) ? true : false) \
	)

#define SET_ACS_BAND(acs_band, sap_ctx) \
{ \
	if (sap_ctx->acs_cfg->start_ch_freq <= \
	    WLAN_REG_CH_TO_FREQ(CHAN_ENUM_2484) && \
	    sap_ctx->acs_cfg->end_ch_freq <= \
			WLAN_REG_CH_TO_FREQ(CHAN_ENUM_2484)) \
		acs_band = eCSR_DOT11_MODE_11g; \
	else if (sap_ctx->acs_cfg->start_ch_freq >= \
		 WLAN_REG_CH_TO_FREQ(CHAN_ENUM_2484))\
		acs_band = eCSR_DOT11_MODE_11a; \
	else \
		acs_band = eCSR_DOT11_MODE_abg; \
}

#define ACS_WEIGHT_AMOUNT_LOCAL    240

#define ACS_WEIGHT_AMOUNT_CONFIG(weights) \
	(((weights) & 0xf) + \
	(((weights) & 0xf0) >> 4) + \
	(((weights) & 0xf00) >> 8) + \
	(((weights) & 0xf000) >> 12) + \
	(((weights) & 0xf0000) >> 16) + \
	(((weights) & 0xf00000) >> 20) + \
	(((weights) & 0xf000000) >> 24))

/*
 * LSH/RSH 4 to enhance the accurate since
 * need to do modulation to ACS_WEIGHT_AMOUNT_LOCAL.
 */
#define ACS_WEIGHT_COMPUTE(weights, weight, factor, base) \
	(((((((((weight) << 4) * ACS_WEIGHT_AMOUNT_LOCAL * (factor)) + \
	(ACS_WEIGHT_AMOUNT_CONFIG((weights)) >> 1)) / \
	ACS_WEIGHT_AMOUNT_CONFIG((weights))) + \
	((base) >> 1)) / (base)) + 8) >> 4)

#define ACS_WEIGHT_CFG_TO_LOCAL(weights, weight) \
	(((((((weight) << 4) * ACS_WEIGHT_AMOUNT_LOCAL) + \
	(ACS_WEIGHT_AMOUNT_CONFIG((weights)) >> 1)) / \
	ACS_WEIGHT_AMOUNT_CONFIG((weights))) + 8) >> 4)

#define ACS_WEIGHT_SOFTAP_RSSI_CFG(weights) \
	((weights) & 0xf)

#define ACS_WEIGHT_SOFTAP_COUNT_CFG(weights) \
	(((weights) & 0xf0) >> 4)

#define ACS_WEIGHT_SOFTAP_NOISE_FLOOR_CFG(weights) \
	(((weights) & 0xf00) >> 8)

#define ACS_WEIGHT_SOFTAP_CHANNEL_FREE_CFG(weights) \
	(((weights) & 0xf000) >> 12)

#define ACS_WEIGHT_SOFTAP_TX_POWER_RANGE_CFG(weights) \
	(((weights) & 0xf0000) >> 16)

#define ACS_WEIGHT_SOFTAP_TX_POWER_THROUGHPUT_CFG(weights) \
	(((weights) & 0xf00000) >> 20)

#define ACS_WEIGHT_SOFTAP_REG_MAX_POWER_CFG(weights) \
	(((weights) & 0xf000000) >> 24)

typedef struct {
	uint16_t chStartNum;
	uint32_t weight;
} sapAcsChannelInfo;

sapAcsChannelInfo acs_ht40_channels5_g[] = {
	{36, SAP_ACS_WEIGHT_MAX},
	{44, SAP_ACS_WEIGHT_MAX},
	{52, SAP_ACS_WEIGHT_MAX},
	{60, SAP_ACS_WEIGHT_MAX},
	{100, SAP_ACS_WEIGHT_MAX},
	{108, SAP_ACS_WEIGHT_MAX},
	{116, SAP_ACS_WEIGHT_MAX},
	{124, SAP_ACS_WEIGHT_MAX},
	{132, SAP_ACS_WEIGHT_MAX},
	{140, SAP_ACS_WEIGHT_MAX},
	{149, SAP_ACS_WEIGHT_MAX},
	{157, SAP_ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acs_ht80_channels[] = {
	{36, SAP_ACS_WEIGHT_MAX},
	{52, SAP_ACS_WEIGHT_MAX},
	{100, SAP_ACS_WEIGHT_MAX},
	{116, SAP_ACS_WEIGHT_MAX},
	{132, SAP_ACS_WEIGHT_MAX},
	{149, SAP_ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acs_vht160_channels[] = {
	{36, SAP_ACS_WEIGHT_MAX},
	{100, SAP_ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acs_ht40_channels24_g[] = {
	{1, SAP_ACS_WEIGHT_MAX},
	{2, SAP_ACS_WEIGHT_MAX},
	{3, SAP_ACS_WEIGHT_MAX},
	{4, SAP_ACS_WEIGHT_MAX},
	{9, SAP_ACS_WEIGHT_MAX},
};

#define CHANNEL_165  165

/* rssi discount for channels in PCL */
#define PCL_RSSI_DISCOUNT 10

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * sap_check_n_add_channel() - checks and add given channel in sap context's
 * avoid_channels_info struct
 * @sap_ctx:           sap context.
 * @new_channel:       channel to be added to sap_ctx's avoid ch info
 *
 * sap_ctx contains sap_avoid_ch_info strcut containing the list of channels on
 * which MDM device's AP with MCC was detected. This function will add channels
 * to that list after checking for duplicates.
 *
 * Return: true: if channel was added or already present
 *   else false: if channel list was already full.
 */
static bool
sap_check_n_add_channel(struct sap_context *sap_ctx,
			uint8_t new_channel)
{
	uint8_t i = 0;
	struct sap_avoid_channels_info *ie_info =
		&sap_ctx->sap_detected_avoid_ch_ie;

	for (i = 0; i < sizeof(ie_info->channels); i++) {
		if (ie_info->channels[i] == new_channel)
			break;

		if (ie_info->channels[i] == 0) {
			ie_info->channels[i] = new_channel;
			break;
		}
	}
	if (i == sizeof(ie_info->channels))
		return false;
	else
		return true;
}
/**
 * sap_check_n_add_overlapped_chnls() - checks & add overlapped channels
 *                                      to primary channel in 2.4Ghz band.
 * @sap_ctx:           sap context.
 * @primary_channel:      primary channel to be avoided.
 *
 * sap_ctx contains sap_avoid_ch_info struct containing the list of channels on
 * which MDM device's AP with MCC was detected. This function will add channels
 * to that list after checking for duplicates.
 *
 * Return: true: if channel was added or already present
 *   else false: if channel list was already full.
 */
static bool
sap_check_n_add_overlapped_chnls(struct sap_context *sap_ctx,
				 uint8_t primary_channel)
{
	uint8_t i = 0, j = 0, upper_chnl = 0, lower_chnl = 0;
	struct sap_avoid_channels_info *ie_info =
		&sap_ctx->sap_detected_avoid_ch_ie;
	/*
	 * if primary channel less than channel 1 or out of 2g band then
	 * no further process is required. return true in this case.
	 */
	if (primary_channel < CHANNEL_1 || primary_channel > CHANNEL_14)
		return true;

	/* lower channel is one channel right before primary channel */
	lower_chnl = primary_channel - 1;
	/* upper channel is one channel right after primary channel */
	upper_chnl = primary_channel + 1;

	/* lower channel needs to be non-zero, zero is not valid channel */
	if (lower_chnl > (CHANNEL_1 - 1)) {
		for (i = 0; i < sizeof(ie_info->channels); i++) {
			if (ie_info->channels[i] == lower_chnl)
				break;
			if (ie_info->channels[i] == 0) {
				ie_info->channels[i] = lower_chnl;
				break;
			}
		}
	}
	/* upper channel needs to be atleast last channel in 2.4Ghz band */
	if (upper_chnl < (CHANNEL_14 + 1)) {
		for (j = 0; j < sizeof(ie_info->channels); j++) {
			if (ie_info->channels[j] == upper_chnl)
				break;
			if (ie_info->channels[j] == 0) {
				ie_info->channels[j] = upper_chnl;
				break;
			}
		}
	}
	if (i == sizeof(ie_info->channels) || j == sizeof(ie_info->channels))
		return false;
	else
		return true;
}

/**
 * sap_process_avoid_ie() - processes the detected Q2Q IE
 * context's avoid_channels_info struct
 * @mac_ctx:            pointer to mac_context structure
 * @sap_ctx:            sap context.
 * @scan_list:        scan results for ACS scan.
 * @spect_info:         spectrum weights array to update
 *
 * Detection of Q2Q IE indicates presence of another MDM device with its AP
 * operating in MCC mode. This function parses the scan results and processes
 * the Q2Q IE if found. It then extracts the channels and populates them in
 * sap_ctx struct. It also increases the weights of those channels so that
 * ACS logic will avoid those channels in its selection algorithm.
 *
 * Return: void
 */
static void
sap_process_avoid_ie(struct mac_context *mac_ctx,
		     struct sap_context *sap_ctx,
		     qdf_list_t *scan_list,
		     struct sap_sel_ch_info *spect_info)
{
	const uint8_t *temp_ptr = NULL;
	uint8_t i = 0;
	struct sAvoidChannelIE *avoid_ch_ie;
	struct sap_ch_info *spect_ch = NULL;
	qdf_list_node_t *cur_lst = NULL, *next_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	uint32_t chan_freq;

	spect_ch = spect_info->ch_info;

	if (scan_list)
		qdf_list_peek_front(scan_list, &cur_lst);

	while (cur_lst) {
		cur_node = qdf_container_of(cur_lst, struct scan_cache_node,
					    node);

		temp_ptr = wlan_get_vendor_ie_ptr_from_oui(
				SIR_MAC_QCOM_VENDOR_OUI,
				SIR_MAC_QCOM_VENDOR_SIZE,
				util_scan_entry_ie_data(cur_node->entry),
				util_scan_entry_ie_len(cur_node->entry));

		if (temp_ptr) {
			avoid_ch_ie = (struct sAvoidChannelIE *)temp_ptr;
			if (avoid_ch_ie->type !=
					QCOM_VENDOR_IE_MCC_AVOID_CH) {
				qdf_list_peek_next(scan_list,
						   cur_lst, &next_lst);
				cur_lst = next_lst;
				next_lst = NULL;
				continue;
			}

			sap_ctx->sap_detected_avoid_ch_ie.present = 1;

			chan_freq =
			    wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
							 avoid_ch_ie->channel);

			sap_debug("Q2Q-IE avoid freq = %d", chan_freq);
			/* add this channel to to_avoid channel list */
			sap_check_n_add_channel(sap_ctx, avoid_ch_ie->channel);
			sap_check_n_add_overlapped_chnls(sap_ctx,
							 avoid_ch_ie->channel);
			/*
			 * Mark weight of these channel present in IE to MAX
			 * so that ACS logic will to avoid thse channels
			 */
			for (i = 0; i < spect_info->num_ch; i++) {
				if (spect_ch[i].chan_freq != chan_freq)
					continue;
				/*
				 * weight is set more than max so that,
				 * in the case of other channels being
				 * assigned max weight due to noise,
				 * they may be preferred over channels
				 * with Q2Q IE.
				 */
				spect_ch[i].weight = SAP_ACS_WEIGHT_MAX + 1;
				spect_ch[i].weight_copy =
							SAP_ACS_WEIGHT_MAX + 1;
				break;
			}
		}

		qdf_list_peek_next(scan_list, cur_lst, &next_lst);
		cur_lst = next_lst;
		next_lst = NULL;
	}
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/**
 * sap_select_preferred_channel_from_channel_list() - to calc best channel
 * @best_ch_freq: best chan freq already calculated among all the channels
 * @sap_ctx: sap context
 * @spectinfo_param: Pointer to sap_sel_ch_info structure
 *
 * This function calculates the best channel among the configured channel list.
 * If channel list not configured then returns the best channel calculated
 * among all the channel list.
 *
 * Return: uint32_t best channel frequency
 */
static
uint32_t sap_select_preferred_channel_from_channel_list(uint32_t best_ch_freq,
				struct sap_context *sap_ctx,
				struct sap_sel_ch_info *spectinfo_param)
{
	/*
	 * If Channel List is not Configured don't do anything
	 * Else return the Best Channel from the Channel List
	 */
	if ((!sap_ctx->acs_cfg->freq_list) ||
	    (!spectinfo_param) ||
	    (!sap_ctx->acs_cfg->ch_list_count))
		return best_ch_freq;

	if (wlansap_is_channel_present_in_acs_list(best_ch_freq,
					sap_ctx->acs_cfg->freq_list,
					sap_ctx->acs_cfg->ch_list_count))
		return best_ch_freq;

	return SAP_CHANNEL_NOT_SELECTED;
}

/**
 * sap_chan_sel_init() - Initialize channel select
 * @mac: Opaque handle to the global MAC context
 * @ch_info_params: Pointer to tSapChSelSpectInfo structure
 * @sap_ctx: Pointer to SAP Context
 * @ignore_acs_range: Whether ignore channel which is out of acs range
 *
 * Function sap_chan_sel_init allocates the memory, initializes the
 * structures used by the channel selection algorithm
 *
 * Return: bool Success or FAIL
 */
static bool sap_chan_sel_init(struct mac_context *mac,
			      struct sap_sel_ch_info *ch_info_params,
			      struct sap_context *sap_ctx,
			      bool ignore_acs_range)
{
	struct sap_ch_info *ch_info = NULL;
	uint32_t *ch_list = NULL;
	uint16_t num_chan = 0;
	bool include_dfs_ch = true;
	uint8_t sta_sap_scc_on_dfs_chnl_config_value;
	bool ch_support_puncture;

	ch_info_params->num_ch =
		mac->scan.base_channels.numChannels;

	/* Allocate memory for weight computation of 2.4GHz */
	ch_info = qdf_mem_malloc((ch_info_params->num_ch) *
			sizeof(*ch_info));
	if (!ch_info)
		return false;

	/* Initialize the pointers in the DfsParams to the allocated memory */
	ch_info_params->ch_info = ch_info;

	ch_list = mac->scan.base_channels.channel_freq_list;

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(mac->psoc,
			&sta_sap_scc_on_dfs_chnl_config_value);
#if defined(FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE)
	if (sap_ctx->dfs_ch_disable == true)
		include_dfs_ch = false;
#endif
	if (!mac->mlme_cfg->dfs_cfg.dfs_master_capable ||
	    ACS_DFS_MODE_DISABLE == sap_ctx->dfs_mode)
		include_dfs_ch = false;

	/* Fill the channel number in the spectrum in the operating freq band */
	for (num_chan = 0;
	     num_chan < ch_info_params->num_ch;
	     num_chan++, ch_list++, ch_info++) {
		ch_support_puncture = false;
		ch_info->chan_freq = *ch_list;
		/* Initialise for all channels */
		ch_info->rssi_agr = SOFTAP_MIN_RSSI;
		/* Initialise max ACS weight for all channels */
		ch_info->weight = SAP_ACS_WEIGHT_MAX;

		/* check if the channel is in NOL denylist */
		if (sap_dfs_is_channel_in_nol_list(
					sap_ctx, *ch_list,
					PHY_SINGLE_CHANNEL_CENTERED)) {
			if (sap_acs_is_puncture_applicable(sap_ctx->acs_cfg)) {
				sap_debug_rl("freq %d is in NOL list, can be punctured",
					     *ch_list);
				ch_support_puncture = true;
			} else {
				sap_debug_rl("freq %d is in NOL list",
					     *ch_list);
				continue;
			}
		}

		if (!include_dfs_ch ||
		    (sta_sap_scc_on_dfs_chnl_config_value ==
				PM_STA_SAP_ON_DFS_MASTER_MODE_DISABLED &&
		     !policy_mgr_is_sta_sap_scc(mac->psoc,
						ch_info->chan_freq))) {
			if (wlan_reg_is_dfs_for_freq(mac->pdev,
						     ch_info->chan_freq)) {
				sap_debug("DFS Ch %d not considered for ACS. include_dfs_ch %u, sta_sap_scc_on_dfs_chnl_config_value %d",
					  *ch_list, include_dfs_ch,
					  sta_sap_scc_on_dfs_chnl_config_value);
				continue;
			}
		}

		if (!policy_mgr_is_sap_freq_allowed(mac->psoc,
			wlan_vdev_mlme_get_opmode(sap_ctx->vdev), *ch_list)) {
			if (sap_acs_is_puncture_applicable(sap_ctx->acs_cfg)) {
				sap_info("freq %d is not allowed, can be punctured",
					 *ch_list);
				ch_support_puncture = true;
			} else {
				sap_info("Skip freq %d", *ch_list);
				continue;
			}
		}

		/* OFDM rates are not supported on frequency 2484 */
		if (*ch_list == 2484 &&
		    eCSR_DOT11_MODE_11b != sap_ctx->phyMode)
			continue;

		/* Skip DSRC channels */
		if (wlan_reg_is_dsrc_freq(ch_info->chan_freq))
			continue;

		/* Skip indoor channels for non-scc indoor scenario*/
		if (!policy_mgr_is_sap_go_interface_allowed_on_indoor(
							mac->pdev,
							sap_ctx->sessionId,
							*ch_list)) {
			sap_debug("Do not allow SAP on indoor frequency %u",
				  *ch_list);
			continue;
		}

		/*
		 * Skip the channels which are not in ACS config from user
		 * space
		 */
		if (!ignore_acs_range &&
		    !wlansap_is_channel_present_in_acs_list(
		    *ch_list, sap_ctx->acs_cfg->freq_list,
		    sap_ctx->acs_cfg->ch_list_count)) {
			if (wlansap_is_channel_present_in_acs_list(
					ch_info->chan_freq,
					sap_ctx->acs_cfg->master_freq_list,
					sap_ctx->acs_cfg->master_ch_list_count))
				ch_info->weight = SAP_ACS_WEIGHT_ADJUSTABLE;
			continue;
		}

		ch_info->valid = true;
		if (!ch_support_puncture)
			ch_info->weight = 0;
	}

	return true;
}

/**
 * sapweight_rssi_count() - calculates the channel weight due to rssi
 *                          and data count(here number of BSS observed)
 * @sap_ctx     : Softap context
 * @rssi        : Max signal strength received from a BSS for the channel
 * @count       : Number of BSS observed in the channel
 *
 * Return: uint32_t Calculated channel weight based on above two
 */
static
uint32_t sapweight_rssi_count(struct sap_context *sap_ctx, int8_t rssi,
			      uint16_t count)
{
	int32_t rssiWeight = 0;
	int32_t countWeight = 0;
	uint32_t rssicountWeight = 0;
	uint8_t softap_rssi_weight_cfg, softap_count_weight_cfg;
	uint8_t softap_rssi_weight_local, softap_count_weight_local;

	softap_rssi_weight_cfg =
	    ACS_WEIGHT_SOFTAP_RSSI_CFG(sap_ctx->auto_channel_select_weight);

	softap_count_weight_cfg =
	    ACS_WEIGHT_SOFTAP_COUNT_CFG(sap_ctx->auto_channel_select_weight);

	softap_rssi_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_rssi_weight_cfg);

	softap_count_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_count_weight_cfg);

	/* Weight from RSSI */
	rssiWeight = ACS_WEIGHT_COMPUTE(sap_ctx->auto_channel_select_weight,
					softap_rssi_weight_cfg,
					rssi - SOFTAP_MIN_RSSI,
					SOFTAP_MAX_RSSI - SOFTAP_MIN_RSSI);

	if (rssiWeight > softap_rssi_weight_local)
		rssiWeight = softap_rssi_weight_local;

	else if (rssiWeight < 0)
		rssiWeight = 0;

	/* Weight from data count */
	countWeight = ACS_WEIGHT_COMPUTE(sap_ctx->auto_channel_select_weight,
					 softap_count_weight_cfg,
					 count - SOFTAP_MIN_COUNT,
					 SOFTAP_MAX_COUNT - SOFTAP_MIN_COUNT);

	if (countWeight > softap_count_weight_local)
		countWeight = softap_count_weight_local;

	rssicountWeight = rssiWeight + countWeight;

	return rssicountWeight;
}

/**
 * sap_get_channel_status() - get channel info via channel number
 * @p_mac: Pointer to Global MAC structure
 * @chan_freq: channel frequency
 *
 * Return: chan status info
 */
static struct channel_status *sap_get_channel_status
	(struct mac_context *p_mac, uint32_t chan_freq)
{
	if (!p_mac->sap.acs_with_more_param)
		return NULL;

	return ucfg_mc_cp_stats_get_channel_status(p_mac->pdev, chan_freq);
}

#ifndef WLAN_FEATURE_SAP_ACS_OPTIMIZE
/**
 * sap_clear_channel_status() - clear chan info
 * @p_mac: Pointer to Global MAC structure
 *
 * Return: none
 */
static void sap_clear_channel_status(struct mac_context *p_mac)
{
	if (!p_mac->sap.acs_with_more_param)
		return;

	ucfg_mc_cp_stats_clear_channel_status(p_mac->pdev);
}
#else
static void sap_clear_channel_status(struct mac_context *p_mac)
{
}
#endif

/**
 * sap_weight_channel_noise_floor() - compute noise floor weight
 * @sap_ctx:  sap context
 * @channel_stat: Pointer to chan status info
 *
 * Return: channel noise floor weight
 */
static uint32_t sap_weight_channel_noise_floor(struct sap_context *sap_ctx,
					       struct channel_status
						*channel_stat)
{
	uint32_t    noise_floor_weight;
	uint8_t     softap_nf_weight_cfg;
	uint8_t     softap_nf_weight_local;

	softap_nf_weight_cfg =
	    ACS_WEIGHT_SOFTAP_NOISE_FLOOR_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_nf_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_nf_weight_cfg);

	if (!channel_stat || channel_stat->channel_freq == 0)
		return softap_nf_weight_local;

	noise_floor_weight = (channel_stat->noise_floor == 0) ? 0 :
			    (ACS_WEIGHT_COMPUTE(
			     sap_ctx->auto_channel_select_weight,
			     softap_nf_weight_cfg,
			     channel_stat->noise_floor -
			     SOFTAP_MIN_NF,
			     SOFTAP_MAX_NF - SOFTAP_MIN_NF));

	if (noise_floor_weight > softap_nf_weight_local)
		noise_floor_weight = softap_nf_weight_local;

	sap_debug("nf=%d, nfwc=%d, nfwl=%d, nfw=%d freq=%d",
		  channel_stat->noise_floor,
		  softap_nf_weight_cfg, softap_nf_weight_local,
		  noise_floor_weight, channel_stat->channel_freq);

	return noise_floor_weight;
}

/**
 * sap_weight_channel_free() - compute channel free weight
 * @sap_ctx:  sap context
 * @channel_stat: Pointer to chan status info
 *
 * Return: channel free weight
 */
static uint32_t sap_weight_channel_free(struct sap_context *sap_ctx,
					struct channel_status
					*channel_stat)
{
	uint32_t     channel_free_weight;
	uint8_t      softap_channel_free_weight_cfg;
	uint8_t      softap_channel_free_weight_local;
	uint32_t     rx_clear_count = 0;
	uint32_t     cycle_count = 0;

	softap_channel_free_weight_cfg =
	    ACS_WEIGHT_SOFTAP_CHANNEL_FREE_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_channel_free_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_channel_free_weight_cfg);

	if (!channel_stat || channel_stat->channel_freq == 0)
		return softap_channel_free_weight_local;

	rx_clear_count = channel_stat->rx_clear_count -
			channel_stat->tx_frame_count -
			channel_stat->rx_frame_count;
	cycle_count = channel_stat->cycle_count;

	/* LSH 4, otherwise it is always 0. */
	channel_free_weight = (cycle_count == 0) ? 0 :
			 (ACS_WEIGHT_COMPUTE(
			  sap_ctx->auto_channel_select_weight,
			  softap_channel_free_weight_cfg,
			 ((rx_clear_count << 8) +
			 (cycle_count >> 1))/cycle_count -
			 (SOFTAP_MIN_CHNFREE << 8),
			 (SOFTAP_MAX_CHNFREE -
			 SOFTAP_MIN_CHNFREE) << 8));

	if (channel_free_weight > softap_channel_free_weight_local)
		channel_free_weight = softap_channel_free_weight_local;

	sap_debug_rl("rcc=%d, cc=%d, tc=%d, rc=%d, cfwc=%d, cfwl=%d, cfw=%d",
		     rx_clear_count, cycle_count,
		     channel_stat->tx_frame_count,
		     channel_stat->rx_frame_count,
		     softap_channel_free_weight_cfg,
		     softap_channel_free_weight_local,
		     channel_free_weight);

	return channel_free_weight;
}

/**
 * sap_weight_channel_txpwr_range() - compute channel tx power range weight
 * @sap_ctx:  sap context
 * @channel_stat: Pointer to chan status info
 *
 * Return: tx power range weight
 */
static uint32_t sap_weight_channel_txpwr_range(struct sap_context *sap_ctx,
					       struct channel_status
					       *channel_stat)
{
	uint32_t     txpwr_weight_low_speed;
	uint8_t      softap_txpwr_range_weight_cfg;
	uint8_t      softap_txpwr_range_weight_local;

	softap_txpwr_range_weight_cfg =
	    ACS_WEIGHT_SOFTAP_TX_POWER_RANGE_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_txpwr_range_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_txpwr_range_weight_cfg);

	if (!channel_stat || channel_stat->channel_freq == 0)
		return softap_txpwr_range_weight_local;


	txpwr_weight_low_speed = (channel_stat->chan_tx_pwr_range == 0) ? 0 :
				(ACS_WEIGHT_COMPUTE(
				 sap_ctx->auto_channel_select_weight,
				 softap_txpwr_range_weight_cfg,
				 SOFTAP_MAX_TXPWR -
				 channel_stat->chan_tx_pwr_range,
				 SOFTAP_MAX_TXPWR - SOFTAP_MIN_TXPWR));

	if (txpwr_weight_low_speed > softap_txpwr_range_weight_local)
		txpwr_weight_low_speed = softap_txpwr_range_weight_local;

	sap_debug_rl("tpr=%d, tprwc=%d, tprwl=%d, tprw=%d",
		     channel_stat->chan_tx_pwr_range,
		     softap_txpwr_range_weight_cfg,
		     softap_txpwr_range_weight_local,
		     txpwr_weight_low_speed);

	return txpwr_weight_low_speed;
}

/**
 * sap_weight_channel_txpwr_tput() - compute channel tx power
 * throughput weight
 * @sap_ctx:  sap context
 * @channel_stat: Pointer to chan status info
 *
 * Return: tx power throughput weight
 */
static uint32_t sap_weight_channel_txpwr_tput(struct sap_context *sap_ctx,
					      struct channel_status
					      *channel_stat)
{
	uint32_t     txpwr_weight_high_speed;
	uint8_t      softap_txpwr_tput_weight_cfg;
	uint8_t      softap_txpwr_tput_weight_local;

	softap_txpwr_tput_weight_cfg =
	    ACS_WEIGHT_SOFTAP_TX_POWER_THROUGHPUT_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_txpwr_tput_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_txpwr_tput_weight_cfg);

	if (!channel_stat || channel_stat->channel_freq == 0)
		return softap_txpwr_tput_weight_local;

	txpwr_weight_high_speed = (channel_stat->chan_tx_pwr_throughput == 0)
				  ? 0 : (ACS_WEIGHT_COMPUTE(
				  sap_ctx->auto_channel_select_weight,
				  softap_txpwr_tput_weight_cfg,
				  SOFTAP_MAX_TXPWR -
				  channel_stat->chan_tx_pwr_throughput,
				  SOFTAP_MAX_TXPWR - SOFTAP_MIN_TXPWR));

	if (txpwr_weight_high_speed > softap_txpwr_tput_weight_local)
		txpwr_weight_high_speed = softap_txpwr_tput_weight_local;

	sap_debug_rl("tpt=%d, tptwc=%d, tptwl=%d, tptw=%d",
		     channel_stat->chan_tx_pwr_throughput,
		     softap_txpwr_tput_weight_cfg,
		     softap_txpwr_tput_weight_local,
		     txpwr_weight_high_speed);

	return txpwr_weight_high_speed;
}

/**
 * sap_weight_channel_status() - compute chan status weight
 * @sap_ctx:  sap context
 * @channel_stat: Pointer to chan status info
 *
 * Return: chan status weight
 */
static
uint32_t sap_weight_channel_status(struct sap_context *sap_ctx,
				   struct channel_status *channel_stat)
{
	return sap_weight_channel_noise_floor(sap_ctx, channel_stat) +
	       sap_weight_channel_free(sap_ctx, channel_stat) +
	       sap_weight_channel_txpwr_range(sap_ctx, channel_stat) +
	       sap_weight_channel_txpwr_tput(sap_ctx, channel_stat);
}

/**
 * sap_update_rssi_bsscount() - updates bss count and rssi effect.
 *
 * @ch_info:     Channel Information
 * @offset:       Channel Offset
 * @sap_24g:      Channel is in 2.4G or 5G
 * @ch_start: the start of channel array
 * @ch_end: the end of channel array
 *
 * sap_update_rssi_bsscount updates bss count and rssi effect based
 * on the channel offset.
 *
 * Return: None.
 */

static void sap_update_rssi_bsscount(struct sap_ch_info *ch_info,
				     int32_t offset, bool sap_24g,
				     struct sap_ch_info *ch_start,
				     struct sap_ch_info *ch_end)
{
	struct sap_ch_info *chan_info = NULL;
	int32_t rssi, rsssi_effect;

	chan_info = (ch_info + offset);
	if (chan_info && chan_info >= ch_start &&
	    chan_info < ch_end) {
		if (!WLAN_REG_IS_SAME_BAND_FREQS(ch_info->chan_freq,
						 chan_info->chan_freq))
			return;
		++chan_info->bss_count;
		switch (offset) {
		case -1:
		case 1:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
			break;
		case -2:
		case 2:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
			break;
		case -3:
		case 3:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND3_RSSI_EFFECT_PRIMARY;
			break;
		case -4:
		case 4:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND4_RSSI_EFFECT_PRIMARY;
			break;
		case -5:
		case 5:
			rsssi_effect = SAP_SUBBAND5_RSSI_EFFECT_PRIMARY;
			break;
		case -6:
		case 6:
			rsssi_effect = SAP_SUBBAND6_RSSI_EFFECT_PRIMARY;
			break;
		case -7:
		case 7:
			rsssi_effect = SAP_SUBBAND7_RSSI_EFFECT_PRIMARY;
			break;
		default:
			rsssi_effect = 0;
			break;
		}

		rssi = ch_info->rssi_agr + rsssi_effect;
		if (IS_RSSI_VALID(chan_info->rssi_agr, rssi))
			chan_info->rssi_agr = rssi;
		if (chan_info->rssi_agr < SOFTAP_MIN_RSSI)
			chan_info->rssi_agr = SOFTAP_MIN_RSSI;
	}
}

/**
 * sap_update_rssi_bsscount_vht_5G() - updates bss count and rssi effect.
 *
 * @spect_ch:     Channel Information
 * @offset:       Channel Offset
 * @num_ch:       no.of channels
 * @ch_start: the start of spect ch array
 * @ch_end: the end of spect ch array
 *
 * sap_update_rssi_bsscount_vht_5G updates bss count and rssi effect based
 * on the channel offset.
 *
 * Return: None.
 */

static void sap_update_rssi_bsscount_vht_5G(
					struct sap_ch_info *spect_ch,
					int32_t offset,
					uint16_t num_ch,
					struct sap_ch_info *ch_start,
					struct sap_ch_info *ch_end)
{
	int32_t ch_offset;
	uint16_t i, cnt;

	if (!offset)
		return;
	if (offset > 0)
		cnt = num_ch;
	else
		cnt = num_ch + 1;
	for (i = 0; i < cnt; i++) {
		ch_offset = offset + i;
		if (ch_offset == 0)
			continue;
		sap_update_rssi_bsscount(spect_ch, ch_offset, false,
			ch_start, ch_end);
	}
}
/**
 * sap_interference_rssi_count_5G() - sap_interference_rssi_count
 *                                    considers the Adjacent channel rssi and
 *                                    data count(here number of BSS observed)
 * @spect_ch:        Channel Information
 * @chan_width:      Channel width parsed from beacon IE
 * @sec_chan_offset: Secondary Channel Offset
 * @ch_freq0:     frequency_0 for the given channel.
 * @ch_freq1:     frequency_1 for the given channel.
 * @op_chan_freq: Operating channel frequency.
 * @ch_start: the start of spect ch array
 * @ch_end: the end of spect ch array
 *
 * sap_interference_rssi_count_5G considers the Adjacent channel rssi
 * and data count(here number of BSS observed)
 *
 * Return: NA.
 */

static void sap_interference_rssi_count_5G(struct sap_ch_info *spect_ch,
					   uint16_t chan_width,
					   uint16_t sec_chan_offset,
					   uint32_t ch_freq0,
					   uint32_t ch_freq1,
					   uint32_t op_chan_freq,
					   struct sap_ch_info *ch_start,
					   struct sap_ch_info *ch_end)
{
	uint16_t num_ch;
	int32_t offset = 0;

	sap_debug("freq = %d, ch width = %d, ch_freq0 = %d ch_freq1 = %d",
		  op_chan_freq, chan_width, ch_freq0, ch_freq1);

	switch (chan_width) {
	case eHT_CHANNEL_WIDTH_40MHZ:
		switch (sec_chan_offset) {
		/* Above the Primary Channel */
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
			sap_update_rssi_bsscount(spect_ch, 1, false,
						 ch_start, ch_end);
			return;

		/* Below the Primary channel */
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			sap_update_rssi_bsscount(spect_ch, -1, false,
						 ch_start, ch_end);
			return;
		}
		return;
	case eHT_CHANNEL_WIDTH_80MHZ:
	case eHT_CHANNEL_WIDTH_80P80MHZ:
		num_ch = 3;
		if ((ch_freq0 - op_chan_freq) == 30) {
			offset = 1;
		} else if ((ch_freq0 - op_chan_freq) == 10) {
			offset = -1;
		} else if ((ch_freq0 - op_chan_freq) == -10) {
			offset = -2;
		} else if ((ch_freq0 - op_chan_freq) == -30) {
			offset = -3;
		}
		break;
	case eHT_CHANNEL_WIDTH_160MHZ:
		num_ch = 7;
		if ((ch_freq0 - op_chan_freq) == 70)
			offset = 1;
		else if ((ch_freq0 - op_chan_freq) == 50)
			offset = -1;
		else if ((ch_freq0 - op_chan_freq) == 30)
			offset = -2;
		else if ((ch_freq0 - op_chan_freq) == 10)
			offset = -3;
		else if ((ch_freq0 - op_chan_freq) == -10)
			offset = -4;
		else if ((ch_freq0 - op_chan_freq) == -30)
			offset = -5;
		else if ((ch_freq0 - op_chan_freq) == -50)
			offset = -6;
		else if ((ch_freq0 - op_chan_freq) == -70)
			offset = -7;
		break;
	default:
		return;
	}

	sap_update_rssi_bsscount_vht_5G(spect_ch, offset, num_ch, ch_start,
					ch_end);
}

/**
 * sap_interference_rssi_count() - sap_interference_rssi_count
 *                                 considers the Adjacent channel rssi
 *                                 and data count(here number of BSS observed)
 * @spect_ch: Channel Information
 * @ch_start: the start of spect ch array
 * @ch_end: the end of spect ch array
 * @mac: Opaque handle to the global MAC context
 *
 * sap_interference_rssi_count considers the Adjacent channel rssi
 * and data count(here number of BSS observed)
 *
 * Return: None.
 */

static void sap_interference_rssi_count(struct sap_ch_info *spect_ch,
					struct sap_ch_info *ch_start,
					struct sap_ch_info *ch_end,
					struct mac_context *mac)
{
	if (!spect_ch) {
		sap_err("spect_ch is NULL");
		return;
	}

	switch (wlan_reg_freq_to_chan(mac->pdev, spect_ch->chan_freq)) {
	case CHANNEL_1:
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			ch_start, ch_end);
		break;

	case CHANNEL_2:
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			ch_start, ch_end);
		break;
	case CHANNEL_3:
		sap_update_rssi_bsscount(spect_ch, -2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			ch_start, ch_end);
		break;
	case CHANNEL_4:
		sap_update_rssi_bsscount(spect_ch, -3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			ch_start, ch_end);
		break;

	case CHANNEL_5:
	case CHANNEL_6:
	case CHANNEL_7:
	case CHANNEL_8:
	case CHANNEL_9:
	case CHANNEL_10:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			ch_start, ch_end);
		break;

	case CHANNEL_11:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			ch_start, ch_end);
		break;

	case CHANNEL_12:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			ch_start, ch_end);
		break;

	case CHANNEL_13:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			ch_start, ch_end);
		break;

	case CHANNEL_14:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			ch_start, ch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			ch_start, ch_end);
		break;

	default:
		break;
	}
}

/**
 * ch_in_pcl() - Is channel in the Preferred Channel List (PCL)
 * @sap_ctx: SAP context which contains the current PCL
 * @ch_freq: Input channel number to be checked
 *
 * Check if a channel is in the preferred channel list
 *
 * Return: True if channel is in PCL, else False
 */
static bool ch_in_pcl(struct sap_context *sap_ctx, uint32_t ch_freq)
{
	uint32_t i;

	for (i = 0; i < sap_ctx->acs_cfg->pcl_ch_count; i++) {
		if (ch_freq == sap_ctx->acs_cfg->pcl_chan_freq[i])
			return true;
	}

	return false;
}

/**
 * sap_upd_chan_spec_params() - sap_upd_chan_spec_params
 *  updates channel parameters obtained from Beacon
 * @scan_entry: Beacon structure populated by scan
 * @ch_width: Channel width
 * @sec_ch_offset: Secondary Channel Offset
 * @center_freq0: Central frequency 0 for the given channel
 * @center_freq1: Central frequency 1 for the given channel
 *
 * sap_upd_chan_spec_params updates the spectrum channels based on the
 * scan_entry
 *
 * Return: NA.
 */
static void
sap_upd_chan_spec_params(struct scan_cache_node *scan_entry,
			 tSirMacHTChannelWidth *ch_width,
			 uint16_t *sec_ch_offset,
			 uint32_t *center_freq0,
			 uint32_t *center_freq1)
{
	enum wlan_phymode phy_mode;
	struct channel_info *chan;

	phy_mode = util_scan_entry_phymode(scan_entry->entry);
	chan = util_scan_entry_channel(scan_entry->entry);

	if (IS_WLAN_PHYMODE_160MHZ(phy_mode)) {
		if (phy_mode == WLAN_PHYMODE_11AC_VHT80_80 ||
		    phy_mode == WLAN_PHYMODE_11AXA_HE80_80) {
			*ch_width = eHT_CHANNEL_WIDTH_80P80MHZ;
			*center_freq0 = chan->cfreq0;
			*center_freq1 = chan->cfreq1;
		} else {
			*ch_width = eHT_CHANNEL_WIDTH_160MHZ;
			if (chan->cfreq1)
				*center_freq0 = chan->cfreq1;
			else
				*center_freq0 = chan->cfreq0;
		}

	} else if (IS_WLAN_PHYMODE_80MHZ(phy_mode)) {
		*ch_width = eHT_CHANNEL_WIDTH_80MHZ;
		*center_freq0 = chan->cfreq0;
	} else if (IS_WLAN_PHYMODE_40MHZ(phy_mode)) {
		if (chan->cfreq0 > chan->chan_freq)
			*sec_ch_offset = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		else
			*sec_ch_offset = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		*ch_width = eHT_CHANNEL_WIDTH_40MHZ;
		*center_freq0 = chan->cfreq0;
	} else {
		*ch_width = eHT_CHANNEL_WIDTH_20MHZ;
	}
}

/**
 * sap_weight_channel_reg_max_power() - API to calculate channel weight of max
 *                                      tx power allowed
 * @sap_ctx: SAP context
 * @freq: channel frequency
 *
 * This function get channel tx power limit from secondary current channel
 * list and calculate weight with power factor configure
 *
 * Return: channel power weight
 */
static uint32_t
sap_weight_channel_reg_max_power(struct sap_context *sap_ctx, qdf_freq_t freq)
{
	struct wlan_objmgr_pdev *pdev;
	int32_t power_weight;
	uint8_t power_weight_cfg, power_weight_local;
	uint16_t eirp_pwr, psd_pwr;
	bool is_psd;
	uint32_t chan_flags;
	QDF_STATUS status;

	power_weight_cfg = ACS_WEIGHT_SOFTAP_REG_MAX_POWER_CFG(
			sap_ctx->auto_channel_select_weight);

	/* reg max power factor not configure, return zero weight */
	if (!power_weight_cfg)
		return 0;

	power_weight_local = ACS_WEIGHT_CFG_TO_LOCAL(
			sap_ctx->auto_channel_select_weight, power_weight_cfg);

	if (!sap_ctx->vdev) {
		sap_err("sap ctx vdev is null.");
		return power_weight_local;
	}
	pdev = wlan_vdev_get_pdev(sap_ctx->vdev);
	status = wlan_reg_get_chan_pwr_attr_from_secondary_list_for_freq(
			pdev, freq, &is_psd, &eirp_pwr, &psd_pwr, &chan_flags);
	if (status != QDF_STATUS_SUCCESS) {
		sap_err("fail to get power attribute.");
		return power_weight_local;
	}

	if (eirp_pwr > REG_MAX_EIRP_POWER) {
		sap_debug("eirp_pwr %d exceed max", eirp_pwr);
		eirp_pwr = REG_MAX_EIRP_POWER;
	}
	if (eirp_pwr < REG_MIN_EIRP_POWER) {
		sap_debug("eirp_pwr %d below min", eirp_pwr);
		eirp_pwr = REG_MIN_EIRP_POWER;
	}

	power_weight = ACS_WEIGHT_COMPUTE(
			sap_ctx->auto_channel_select_weight,
			power_weight_cfg,
			REG_MAX_EIRP_POWER - eirp_pwr,
			REG_MAX_EIRP_POWER - REG_MIN_EIRP_POWER);

	if (power_weight > power_weight_local)
		power_weight = power_weight_local;
	else if (power_weight < 0)
		power_weight = 0;

	return power_weight;
}

static void
sap_normalize_channel_weight_with_factors(struct mac_context *mac,
					  struct sap_ch_info *spect_ch)
{
	uint32_t normalized_weight;
	uint8_t normalize_factor = 100;
	uint8_t dfs_normalize_factor;
	uint32_t chan_freq, i;
	struct acs_weight *weight_list =
			mac->mlme_cfg->acs.normalize_weight_chan;
	struct acs_weight_range *range_list =
			mac->mlme_cfg->acs.normalize_weight_range;
	bool freq_present_in_list = false;

	chan_freq = spect_ch->chan_freq;

	/* Check if the freq is present in range list */
	for (i = 0; i < mac->mlme_cfg->acs.num_weight_range; i++) {
		if (chan_freq >= range_list[i].start_freq &&
		    chan_freq <= range_list[i].end_freq) {
			normalize_factor = range_list[i].normalize_weight;
			sap_debug_rl("Range list, freq %d normalize weight factor %d",
				     chan_freq, normalize_factor);
			freq_present_in_list = true;
		}
	}

	/* Check if user wants a special factor for this freq */
	for (i = 0; i < mac->mlme_cfg->acs.normalize_weight_num_chan; i++) {
		if (chan_freq == weight_list[i].chan_freq) {
			normalize_factor = weight_list[i].normalize_weight;
			sap_debug("freq %d normalize weight factor %d",
				  chan_freq, normalize_factor);
			freq_present_in_list = true;
		}
	}

	if (wlan_reg_is_dfs_for_freq(mac->pdev, chan_freq)) {
		dfs_normalize_factor = MLME_GET_DFS_CHAN_WEIGHT(
				mac->mlme_cfg->acs.np_chan_weightage);
		if (freq_present_in_list)
			normalize_factor = qdf_min(dfs_normalize_factor,
						   normalize_factor);
		else
			normalize_factor = dfs_normalize_factor;
		freq_present_in_list = true;
		sap_debug_rl("DFS channel weightage %d min %d",
			     dfs_normalize_factor, normalize_factor);
	}

	if (freq_present_in_list) {
		normalized_weight =
			((SAP_ACS_WEIGHT_MAX - spect_ch->weight) *
			(100 - normalize_factor)) / 100;
		sap_debug_rl("freq %d old weight %d new weight %d",
			     chan_freq, spect_ch->weight,
			     spect_ch->weight + normalized_weight);
		spect_ch->weight += normalized_weight;
	}
}

/**
 * sap_update_6ghz_max_weight() - Update 6 GHz channel max weight
 * @ch_info_params: Pointer to the sap_sel_ch_info structure
 * @max_valid_weight: max valid weight on 6 GHz channels
 *
 * If ACS frequency list includes 6 GHz channels, the user prefers
 * to start SAP on 6 GHz as much as possible. The acs logic in
 * sap_chan_sel_init will mark channel weight to Max weight value
 * of SAP_ACS_WEIGHT_MAX if channel is no in ACS channel list(filtered
 * by PCL).
 * In ACS bw 160 case, sometime the combined weight of 8 channels
 * on 6 GHz(some of them have weight SAP_ACS_WEIGHT_MAX)
 * may higher than 5 GHz channels and finally select 5 GHz channel.
 * This API is to update the 6 GHz weight to max valid weight in
 * 6 GHz instead of value SAP_ACS_WEIGHT_MAX. All those channels have
 * special weight value SAP_ACS_WEIGHT_ADJUSTABLE which is assigned
 * sap_chan_sel_init.
 *
 * Return: void
 */
static void sap_update_6ghz_max_weight(struct sap_sel_ch_info *ch_info_params,
				       uint32_t max_valid_weight)
{
	uint8_t chn_num;
	struct sap_ch_info *pspect_ch;

	sap_debug("max_valid_weight_on_6ghz_channels %d",
		  max_valid_weight);
	if (!max_valid_weight)
		return;
	for (chn_num = 0; chn_num < ch_info_params->num_ch;
	     chn_num++) {
		pspect_ch = &ch_info_params->ch_info[chn_num];
		if (!wlan_reg_is_6ghz_chan_freq(pspect_ch->chan_freq))
			continue;
		if (pspect_ch->weight == SAP_ACS_WEIGHT_ADJUSTABLE) {
			pspect_ch->weight = max_valid_weight;
			pspect_ch->weight_copy = pspect_ch->weight;
		}
	}
}

/**
 * sap_update_5ghz_low_freq_weight() - Update weight of 5GHz low frequency
 * @psoc: Pointer to psoc
 * @ch_info_params: Pointer to sap_sel_ch_info structure
 *
 * This api helps to lower the 5GHz low frequency weight by
 * SAP_NORMALISE_ACS_WEIGHT so that it will get more preference to get
 * selected during ACS.
 *
 * Return: void
 */
static void sap_update_5ghz_low_freq_weight(
					struct wlan_objmgr_psoc *psoc,
					struct sap_sel_ch_info *ch_info_params)
{
	uint8_t ch_num;
	qdf_freq_t freq;
	uint32_t weight;

	if (!policy_mgr_is_hw_sbs_capable(psoc))
		return;

	for (ch_num = 0; ch_num < ch_info_params->num_ch; ch_num++) {
		freq = ch_info_params->ch_info[ch_num].chan_freq;
		weight = ch_info_params->ch_info[ch_num].weight;
		if (policy_mgr_is_given_freq_5g_low(psoc, freq)) {
			/*
			 * Lower the weight by SAP_NORMALISE_ACS_WEIGHT i.e 5%
			 * from channel weight itself. Later if required, modify
			 * this value.
			 * Here are the few observation captured which results
			 * to go with SAP_NORMALISE_ACS_WEIGHT.
			 *
			 * +-----------+-------------+------------+---------------+--------------------------------------+
			 * |   freq    |  bss_count  |    rssi    |     weight    |              observation             |
			 * +---------------------------------------------------------------------------------------------+
			 * |  5G low   |    >6       | -76 - -56  | 17419 - 17774 | Diff b/w 5G low & 5G high min weight |
			 * |  5G high  |    <4       | -100 - -50 | 16842 - 17685 | is ~5% of 5G low min weight		 |
			 * |	       |	     |		  |		  |					 |
			 * |  5G low   |    >6       | -77 - -54  | 17419 - 17730 | Diff b/w 5G low & 5G high min weight |
			 * |  5G high  |    <4	     | -100 - -50 | 16842 - 17552 | is ~5% of 5G low min weight		 |
			 * |	       |	     |		  |		  |					 |
			 * |  5G low   |    >5       | -77 - -57  | 17286 - 17552 | Diff b/w 5G low & 5G high min weight |
			 * |  5G high  |    <4       | -100 - -50 | 16842 - 17596 | is ~5% of 5G low min weight		 |
			 * +-----------+-------------+------------+---------------+--------------------------------------+
			 */

			weight = weight - ((weight * SAP_NORMALISE_ACS_WEIGHT ) / 100);
			ch_info_params->ch_info[ch_num].weight = weight;
		}
	}
}

/**
 * sap_compute_spect_weight() - Compute spectrum weight
 * @ch_info_params: Pointer to the tSpectInfoParams structure
 * @mac: Pointer to mac_context struucture
 * @scan_list: Pointer to channel list
 * @sap_ctx: Context of the SAP
 *
 * Main function for computing the weight of each channel in the
 * spectrum based on the RSSI value of the BSSes on the channel
 * and number of BSS
 */
static void sap_compute_spect_weight(struct sap_sel_ch_info *ch_info_params,
				     struct mac_context *mac,
				     qdf_list_t *scan_list,
				     struct sap_context *sap_ctx)
{
	int8_t rssi = 0;
	uint8_t chn_num = 0;
	struct sap_ch_info *ch_info = ch_info_params->ch_info;
	tSirMacHTChannelWidth ch_width = 0;
	uint16_t secondaryChannelOffset;
	uint32_t center_freq0, center_freq1, chan_freq;
	uint8_t i;
	bool found;
	struct sap_ch_info *ch_start = ch_info_params->ch_info;
	struct sap_ch_info *ch_end = ch_info_params->ch_info +
		ch_info_params->num_ch;
	qdf_list_node_t *cur_lst = NULL, *next_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	uint32_t rssi_bss_weight = 0, chan_status_weight = 0, power_weight = 0;
	uint32_t max_valid_weight_6ghz = 0;

	sap_debug("Computing spectral weight");

	if (scan_list)
		qdf_list_peek_front(scan_list, &cur_lst);
	while (cur_lst) {
		cur_node = qdf_container_of(cur_lst, struct scan_cache_node,
					    node);
		ch_info = ch_info_params->ch_info;
		/* Defining the default values, so that any value will hold the default values */

		secondaryChannelOffset = PHY_SINGLE_CHANNEL_CENTERED;
		center_freq0 = 0;
		center_freq1 = 0;

		chan_freq =
		    util_scan_entry_channel_frequency(cur_node->entry);

		sap_upd_chan_spec_params(cur_node, &ch_width,
					 &secondaryChannelOffset,
					 &center_freq0, &center_freq1);

		/* Processing for each tCsrScanResultInfo in the tCsrScanResult DLink list */
		for (chn_num = 0; chn_num < ch_info_params->num_ch;
		     chn_num++) {

			if (chan_freq != ch_info->chan_freq) {
				ch_info++;
				continue;
			}

			if (ch_info->rssi_agr < cur_node->entry->rssi_raw)
				ch_info->rssi_agr = cur_node->entry->rssi_raw;

			++ch_info->bss_count;

			if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
				sap_interference_rssi_count(ch_info, ch_start,
							    ch_end, mac);
			else
				sap_interference_rssi_count_5G(
				    ch_info, ch_width, secondaryChannelOffset,
				    center_freq0, center_freq1, chan_freq,
				    ch_start, ch_end);

			ch_info++;
			break;

		}

		qdf_list_peek_next(scan_list, cur_lst, &next_lst);
		cur_lst = next_lst;
		next_lst = NULL;
	}

	/* Calculate the weights for all channels in the spectrum ch_info */
	ch_info = ch_info_params->ch_info;

	for (chn_num = 0; chn_num < (ch_info_params->num_ch);
	     chn_num++) {

		/*
		   rssi : Maximum received signal strength among all BSS on that channel
		   bss_count : Number of BSS on that channel
		 */

		rssi = (int8_t)ch_info->rssi_agr;
		if (ch_in_pcl(sap_ctx, ch_info->chan_freq))
			rssi -= PCL_RSSI_DISCOUNT;

		if (rssi < SOFTAP_MIN_RSSI)
			rssi = SOFTAP_MIN_RSSI;

		if (ch_info->weight == SAP_ACS_WEIGHT_MAX ||
		    ch_info->weight == SAP_ACS_WEIGHT_ADJUSTABLE) {
			ch_info->weight_copy = ch_info->weight;
			goto debug_info;
		}

		/* There may be channels in scanlist, which were not sent to
		 * FW for scanning as part of ACS scan list, but they do have an
		 * effect on the neighbouring channels, so they help to find a
		 * suitable channel, but there weight should be max as they were
		 * and not meant to be included in the ACS scan results.
		 * So just assign RSSI as -100, bsscount as 0, and weight as max
		 * to them, so that they always stay low in sorting of best
		 * channels which were included in ACS scan list
		 */
		found = false;
		for (i = 0; i < sap_ctx->num_of_channel; i++) {
			if (ch_info->chan_freq == sap_ctx->freq_list[i]) {
			/* Scan channel was included in ACS scan list */
				found = true;
				break;
			}
		}

		rssi_bss_weight = 0;
		chan_status_weight = 0;
		power_weight = 0;
		if (found) {
			rssi_bss_weight = sapweight_rssi_count(
					sap_ctx,
					rssi,
					ch_info->bss_count);
			chan_status_weight = sap_weight_channel_status(
					sap_ctx,
					sap_get_channel_status(
					mac, ch_info->chan_freq));
			power_weight = sap_weight_channel_reg_max_power(
					sap_ctx, ch_info->chan_freq);
			ch_info->weight = SAPDFS_NORMALISE_1000 *
					(rssi_bss_weight + chan_status_weight
					+ power_weight);
		} else {
			if (wlansap_is_channel_present_in_acs_list(
					ch_info->chan_freq,
					sap_ctx->acs_cfg->master_freq_list,
					sap_ctx->acs_cfg->master_ch_list_count))
				ch_info->weight = SAP_ACS_WEIGHT_ADJUSTABLE;
			else
				ch_info->weight = SAP_ACS_WEIGHT_MAX;
			ch_info->rssi_agr = SOFTAP_MIN_RSSI;
			rssi = SOFTAP_MIN_RSSI;
			ch_info->bss_count = SOFTAP_MIN_COUNT;
		}

		sap_normalize_channel_weight_with_factors(mac, ch_info);

		if (ch_info->weight > SAP_ACS_WEIGHT_MAX)
			ch_info->weight = SAP_ACS_WEIGHT_MAX;
		ch_info->weight_copy = ch_info->weight;

debug_info:
		if (wlan_reg_is_6ghz_chan_freq(ch_info->chan_freq) &&
		    ch_info->weight < SAP_ACS_WEIGHT_ADJUSTABLE &&
		    max_valid_weight_6ghz < ch_info->weight)
			max_valid_weight_6ghz = ch_info->weight;

		sap_debug("freq %d valid %d weight %d(%d,%d,%d) rssi %d bss %d",
			  ch_info->chan_freq, ch_info->valid,
			  ch_info->weight, rssi_bss_weight,
			  chan_status_weight, power_weight,
			  ch_info->rssi_agr, ch_info->bss_count);

		ch_info++;
	}
	sap_update_6ghz_max_weight(ch_info_params,
				   max_valid_weight_6ghz);

	if (policy_mgr_is_vdev_ll_lt_sap(mac->psoc, sap_ctx->vdev_id))
		sap_update_5ghz_low_freq_weight(mac->psoc, ch_info_params);

	sap_clear_channel_status(mac);
}

void sap_chan_sel_exit(struct sap_sel_ch_info *ch_info_params)
{
	/* Free all the allocated memory */
	qdf_mem_free(ch_info_params->ch_info);
}

/*==========================================================================
   FUNCTION    sap_sort_chl_weight

   DESCRIPTION
    Function to sort the channels with the least weight first for 20MHz channels

   DEPENDENCIES
    NA.

   PARAMETERS

    IN
    ch_info_params       : Pointer to the tSapChSelSpectInfo structure

   RETURN VALUE
    void     : NULL

   SIDE EFFECTS
   ============================================================================*/
static void sap_sort_chl_weight(struct sap_sel_ch_info *ch_info_params)
{
	struct sap_ch_info temp;

	struct sap_ch_info *ch_info = NULL;
	uint32_t i = 0, j = 0, min_weight_index = 0;

	ch_info = ch_info_params->ch_info;
	for (i = 0; i < ch_info_params->num_ch; i++) {
		min_weight_index = i;
		for (j = i + 1; j < ch_info_params->num_ch; j++) {
			if (ch_info[j].weight <
			    ch_info[min_weight_index].weight) {
				min_weight_index = j;
			} else if (ch_info[j].weight ==
				   ch_info[min_weight_index].weight) {
				if (ch_info[j].bss_count <
				    ch_info[min_weight_index].bss_count)
					min_weight_index = j;
			}
		}
		if (min_weight_index != i) {
			qdf_mem_copy(&temp, &ch_info[min_weight_index],
				     sizeof(*ch_info));
			qdf_mem_copy(&ch_info[min_weight_index], &ch_info[i],
				     sizeof(*ch_info));
			qdf_mem_copy(&ch_info[i], &temp, sizeof(*ch_info));
		}
	}
}

/**
 * sap_override_6ghz_psc_minidx() - override mindex to 6 GHz PSC channel's idx
 * @mac_ctx: pointer to max context
 * @spectinfo: Pointer to array of tSach_infoInfo
 * @count: number of tSach_infoInfo element to search
 * @minidx: index to be overridden
 *
 * Return: QDF STATUS
 */
static void
sap_override_6ghz_psc_minidx(struct mac_context *mac_ctx,
			     struct sap_ch_info *spectinfo,
			     uint8_t count,
			     uint8_t *minidx)
{
	uint8_t i;

	if (!mac_ctx->mlme_cfg->acs.acs_prefer_6ghz_psc)
		return;

	for (i = 0; i < count; i++) {
		if (wlan_reg_is_6ghz_chan_freq(
				spectinfo[i].chan_freq) &&
		    wlan_reg_is_6ghz_psc_chan_freq(
				spectinfo[i].chan_freq)) {
			*minidx = i;
			return;
		}
	}
}

/**
 * sap_sort_chl_weight_80_mhz() - to sort the channels with the least weight
 * @mac_ctx: pointer to max context
 * @sap_ctx: Pointer to the struct sap_context *structure
 * @ch_info_params: Pointer to the tSapChSelSpectInfo structure
 * Function to sort the channels with the least weight first for HT80 channels
 *
 * Return: QDF STATUS
 */
static QDF_STATUS
sap_sort_chl_weight_80_mhz(struct mac_context *mac_ctx,
			   struct sap_context *sap_ctx,
			   struct sap_sel_ch_info *ch_info_params)
{
	uint8_t i, j;
	struct sap_ch_info *chan_info;
	uint8_t minIdx;
	struct ch_params acs_ch_params = {0};
	int8_t center_freq_diff;
	uint32_t combined_weight;
	uint32_t min_ch_weight;
	uint32_t valid_chans = 0;
	bool has_valid;

	chan_info = ch_info_params->ch_info;

	for (j = 0; j < ch_info_params->num_ch; j++) {

		if (chan_info[j].weight_calc_done)
			continue;

		acs_ch_params.ch_width = CH_WIDTH_80MHZ;
		sap_acs_set_puncture_support(sap_ctx, &acs_ch_params);

		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
							chan_info[j].chan_freq,
							0, &acs_ch_params,
							REG_CURRENT_PWR_MODE);

		/* Check if the freq supports 80 Mhz */
		if (acs_ch_params.ch_width != CH_WIDTH_80MHZ) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 4;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		center_freq_diff = acs_ch_params.mhz_freq_seg0 -
				   chan_info[j].chan_freq;

		/* This channel frequency does not have all channels */
		if (center_freq_diff != 30) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 4;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		/* no other freq left for 80 Mhz operation in spectrum */
		if (j + 3 > ch_info_params->num_ch)
			continue;

		/* Check whether all frequencies are present for 80 Mhz */

		if (!(((chan_info[j].chan_freq + 20) ==
			chan_info[j + 1].chan_freq) &&
			((chan_info[j].chan_freq + 40) ==
				 chan_info[j + 2].chan_freq) &&
			((chan_info[j].chan_freq + 60) ==
				 chan_info[j + 3].chan_freq))) {
			/*
			 * some channels does not exist in pSectInfo array,
			 * skip this channel and those in the same HT80 width
			 */
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 4;
			chan_info[j].weight_calc_done = true;
			if ((chan_info[j].chan_freq + 20) ==
					chan_info[j + 1].chan_freq) {
				chan_info[j + 1].weight =
					SAP_ACS_WEIGHT_MAX * 4;
				chan_info[j + 1].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 40) ==
					chan_info[j + 2].chan_freq) {
				chan_info[j + 2].weight =
					SAP_ACS_WEIGHT_MAX * 4;
				chan_info[j + 2].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 60) ==
					chan_info[j + 3].chan_freq) {
				chan_info[j + 3].weight =
					SAP_ACS_WEIGHT_MAX * 4;
				chan_info[j + 3].weight_calc_done = true;
			}

			continue;
		}

		/* We have 4 channels to calculate cumulative weight */

		combined_weight = chan_info[j].weight +
				  chan_info[j + 1].weight +
				  chan_info[j + 2].weight +
				  chan_info[j + 3].weight;

		min_ch_weight = chan_info[j].weight;
		minIdx = 0;
		has_valid = false;

		for (i = 0; i < 4; i++) {
			if (min_ch_weight > chan_info[j + i].weight) {
				min_ch_weight = chan_info[j + i].weight;
				minIdx = i;
			}
			chan_info[j + i].weight = SAP_ACS_WEIGHT_MAX * 4;
			chan_info[j + i].weight_calc_done = true;
			if (chan_info[j + i].valid)
				has_valid = true;
		}
		sap_override_6ghz_psc_minidx(mac_ctx, &chan_info[j], 4,
					     &minIdx);

		chan_info[j + minIdx].weight = combined_weight;
		if (has_valid)
			valid_chans++;

		sap_debug("best freq = %d for 80mhz center freq %d combined weight = %d valid %d cnt %d",
			  chan_info[j + minIdx].chan_freq,
			  acs_ch_params.mhz_freq_seg0,
			  combined_weight, has_valid, valid_chans);
	}

	if (!valid_chans) {
		sap_debug("no valid chan bonding with CH_WIDTH_80MHZ");
		return QDF_STATUS_E_INVAL;
	}

	sap_sort_chl_weight(ch_info_params);

	return QDF_STATUS_SUCCESS;
}

/**
 * sap_sort_chl_weight_160_mhz() - to sort the channels with the least weight
 * @mac_ctx: pointer to max context
 * @sap_ctx: Pointer to the struct sap_context *structure
 * @ch_info_params: Pointer to the tSapChSelSpectInfo structure
 *
 * Function to sort the channels with the least weight first for VHT160 channels
 *
 * Return: QDF STATUS
 */
static QDF_STATUS
sap_sort_chl_weight_160_mhz(struct mac_context *mac_ctx,
			    struct sap_context *sap_ctx,
			    struct sap_sel_ch_info *ch_info_params)
{
	uint8_t i, j;
	struct sap_ch_info *chan_info;
	uint8_t minIdx;
	struct ch_params acs_ch_params = {0};
	int8_t center_freq_diff;
	uint32_t combined_weight;
	uint32_t min_ch_weight;
	uint32_t valid_chans = 0;
	bool has_valid;

	chan_info = ch_info_params->ch_info;

	for (j = 0; j < ch_info_params->num_ch; j++) {

		if (chan_info[j].weight_calc_done)
			continue;

		acs_ch_params.ch_width = CH_WIDTH_160MHZ;
		sap_acs_set_puncture_support(sap_ctx, &acs_ch_params);

		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
							chan_info[j].chan_freq,
							0, &acs_ch_params,
							REG_CURRENT_PWR_MODE);

		/* Check if the freq supports 160 Mhz */
		if (acs_ch_params.ch_width != CH_WIDTH_160MHZ) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 8;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		center_freq_diff = acs_ch_params.mhz_freq_seg1 -
				   chan_info[j].chan_freq;

		/* This channel frequency does not have all channels */
		if (center_freq_diff != 70) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 8;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		/* no other freq left for 160 Mhz operation in spectrum */
		if (j + 7 > ch_info_params->num_ch)
			continue;

		/* Check whether all frequencies are present for 160 Mhz */

		if (!(((chan_info[j].chan_freq + 20) ==
			chan_info[j + 1].chan_freq) &&
			((chan_info[j].chan_freq + 40) ==
				 chan_info[j + 2].chan_freq) &&
			((chan_info[j].chan_freq + 60) ==
				 chan_info[j + 3].chan_freq) &&
			((chan_info[j].chan_freq + 80) ==
				 chan_info[j + 4].chan_freq) &&
			((chan_info[j].chan_freq + 100) ==
				 chan_info[j + 5].chan_freq) &&
			((chan_info[j].chan_freq + 120) ==
				 chan_info[j + 6].chan_freq) &&
			((chan_info[j].chan_freq + 140) ==
				 chan_info[j + 7].chan_freq))) {
			/*
			 * some channels does not exist in pSectInfo array,
			 * skip this channel and those in the same VHT160 width
			 */
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 8;
			chan_info[j].weight_calc_done = true;
			if ((chan_info[j].chan_freq + 20) ==
					chan_info[j + 1].chan_freq) {
				chan_info[j + 1].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				chan_info[j + 1].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 40) ==
					chan_info[j + 2].chan_freq) {
				chan_info[j + 2].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				chan_info[j + 2].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 60) ==
					chan_info[j + 3].chan_freq) {
				chan_info[j + 3].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				chan_info[j + 3].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 80) ==
					chan_info[j + 4].chan_freq) {
				chan_info[j + 4].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				chan_info[j + 4].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 100) ==
					chan_info[j + 5].chan_freq) {
				chan_info[j + 5].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				chan_info[j + 5].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 120) ==
					chan_info[j + 6].chan_freq) {
				chan_info[j + 6].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				chan_info[j + 6].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 140) ==
					chan_info[j + 7].chan_freq) {
				chan_info[j + 7].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				chan_info[j + 7].weight_calc_done = true;
			}

			continue;
		}

		/* We have 8 channels to calculate cumulative weight */

		combined_weight = chan_info[j].weight +
				  chan_info[j + 1].weight +
				  chan_info[j + 2].weight +
				  chan_info[j + 3].weight +
				  chan_info[j + 4].weight +
				  chan_info[j + 5].weight +
				  chan_info[j + 6].weight +
				  chan_info[j + 7].weight;

		min_ch_weight = chan_info[j].weight;
		minIdx = 0;
		has_valid = false;

		for (i = 0; i < 8; i++) {
			if (min_ch_weight > chan_info[j + i].weight) {
				min_ch_weight = chan_info[j + i].weight;
				minIdx = i;
			}
			chan_info[j + i].weight = SAP_ACS_WEIGHT_MAX * 8;
			chan_info[j + i].weight_calc_done = true;
			if (chan_info[j + i].valid)
				has_valid = true;
		}
		sap_override_6ghz_psc_minidx(mac_ctx, &chan_info[j], 8,
					     &minIdx);

		chan_info[j + minIdx].weight = combined_weight;
		if (has_valid)
			valid_chans++;

		sap_debug("best freq = %d for 160mhz center freq %d combined weight = %d valid %d cnt %d",
			  chan_info[j + minIdx].chan_freq,
			  acs_ch_params.mhz_freq_seg1,
			  combined_weight, has_valid, valid_chans);
	}

	if (!valid_chans) {
		sap_debug("no valid chan bonding with CH_WIDTH_160MHZ");
		return QDF_STATUS_E_INVAL;
	}

	sap_sort_chl_weight(ch_info_params);

	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_11BE)
/**
 * sap_sort_chl_weight_320_mhz() - to sort the channels with the least weight
 * @mac_ctx: pointer to max context
 * @sap_ctx: Pointer to the struct sap_context *structure
 * @ch_info_params: Pointer to the tSapChSelSpectInfo structure
 *
 * Function to sort the channels with the least weight first for 320MHz channels
 *
 * Return: QDF STATUS
 */
static QDF_STATUS
sap_sort_chl_weight_320_mhz(struct mac_context *mac_ctx,
			    struct sap_context *sap_ctx,
			    struct sap_sel_ch_info *ch_info_params)
{
	uint8_t i, j;
	struct sap_ch_info *chan_info;
	uint8_t minIdx;
	struct ch_params acs_ch_params = {0};
	uint32_t combined_weight;
	uint32_t min_ch_weight;
	uint32_t valid_chans = 0;
	bool has_valid;

	chan_info = ch_info_params->ch_info;

	for (j = 0; j < ch_info_params->num_ch; j++) {
		if (chan_info[j].weight_calc_done)
			continue;

		qdf_mem_zero(&acs_ch_params, sizeof(acs_ch_params));
		acs_ch_params.ch_width = CH_WIDTH_320MHZ;
		sap_acs_set_puncture_support(sap_ctx, &acs_ch_params);

		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
							chan_info[j].chan_freq,
							0, &acs_ch_params,
							REG_CURRENT_PWR_MODE);

		/* Check if the freq supports 320 Mhz */
		if (acs_ch_params.ch_width != CH_WIDTH_320MHZ) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 16;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		/* no other freq left for 320 Mhz operation in spectrum */
		if (j + 15 > ch_info_params->num_ch) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 16;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		/* Check whether all frequencies are present for 160 Mhz */

		if (!(((chan_info[j].chan_freq + 20) ==
			chan_info[j + 1].chan_freq) &&
			((chan_info[j].chan_freq + 40) ==
				 chan_info[j + 2].chan_freq) &&
			((chan_info[j].chan_freq + 60) ==
				 chan_info[j + 3].chan_freq) &&
			((chan_info[j].chan_freq + 80) ==
				 chan_info[j + 4].chan_freq) &&
			((chan_info[j].chan_freq + 100) ==
				 chan_info[j + 5].chan_freq) &&
			((chan_info[j].chan_freq + 120) ==
				 chan_info[j + 6].chan_freq) &&
			((chan_info[j].chan_freq + 140) ==
				 chan_info[j + 7].chan_freq) &&
			((chan_info[j].chan_freq + 160) ==
				 chan_info[j + 8].chan_freq) &&
			((chan_info[j].chan_freq + 180) ==
				 chan_info[j + 9].chan_freq) &&
			((chan_info[j].chan_freq + 200) ==
				 chan_info[j + 10].chan_freq) &&
			((chan_info[j].chan_freq + 220) ==
				 chan_info[j + 11].chan_freq) &&
			((chan_info[j].chan_freq + 240) ==
				 chan_info[j + 12].chan_freq) &&
			((chan_info[j].chan_freq + 260) ==
				 chan_info[j + 13].chan_freq) &&
			((chan_info[j].chan_freq + 280) ==
				 chan_info[j + 14].chan_freq) &&
			((chan_info[j].chan_freq + 300) ==
				 chan_info[j + 15].chan_freq))) {
			/*
			 * some channels does not exist in pSectInfo array,
			 * skip this channel and those in the same ETH320 width
			 */
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 16;
			chan_info[j].weight_calc_done = true;
			if ((chan_info[j].chan_freq + 20) ==
					chan_info[j + 1].chan_freq) {
				chan_info[j + 1].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 1].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 40) ==
					chan_info[j + 2].chan_freq) {
				chan_info[j + 2].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 2].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 60) ==
					chan_info[j + 3].chan_freq) {
				chan_info[j + 3].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 3].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 80) ==
					chan_info[j + 4].chan_freq) {
				chan_info[j + 4].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 4].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 100) ==
					chan_info[j + 5].chan_freq) {
				chan_info[j + 5].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 5].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 120) ==
					chan_info[j + 6].chan_freq) {
				chan_info[j + 6].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 6].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 140) ==
					chan_info[j + 7].chan_freq) {
				chan_info[j + 7].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 7].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 160) ==
					chan_info[j + 8].chan_freq) {
				chan_info[j + 8].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 8].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 180) ==
					chan_info[j + 9].chan_freq) {
				chan_info[j + 9].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 9].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 200) ==
					chan_info[j + 10].chan_freq) {
				chan_info[j + 10].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 10].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 220) ==
					chan_info[j + 11].chan_freq) {
				chan_info[j + 11].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 11].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 240) ==
					chan_info[j + 12].chan_freq) {
				chan_info[j + 12].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 12].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 260) ==
					chan_info[j + 13].chan_freq) {
				chan_info[j + 13].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 13].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 280) ==
					chan_info[j + 14].chan_freq) {
				chan_info[j + 14].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 14].weight_calc_done = true;
			}
			if ((chan_info[j].chan_freq + 300) ==
					chan_info[j + 15].chan_freq) {
				chan_info[j + 15].weight =
					SAP_ACS_WEIGHT_MAX * 16;
				chan_info[j + 15].weight_calc_done = true;
			}

			continue;
		}

		/* We have 16 channels to calculate cumulative weight */
		combined_weight = chan_info[j].weight +
				  chan_info[j + 1].weight +
				  chan_info[j + 2].weight +
				  chan_info[j + 3].weight +
				  chan_info[j + 4].weight +
				  chan_info[j + 5].weight +
				  chan_info[j + 6].weight +
				  chan_info[j + 7].weight +
				  chan_info[j + 8].weight +
				  chan_info[j + 9].weight +
				  chan_info[j + 10].weight +
				  chan_info[j + 11].weight +
				  chan_info[j + 12].weight +
				  chan_info[j + 13].weight +
				  chan_info[j + 14].weight +
				  chan_info[j + 15].weight;

		min_ch_weight = chan_info[j].weight;
		minIdx = 0;
		has_valid = false;
		for (i = 0; i < 16; i++) {
			if (min_ch_weight > chan_info[j + i].weight) {
				min_ch_weight = chan_info[j + i].weight;
				minIdx = i;
			}
			chan_info[j + i].weight = SAP_ACS_WEIGHT_MAX * 16;
			chan_info[j + i].weight_calc_done = true;
			if (chan_info[j + i].valid)
				has_valid = true;
		}
		sap_override_6ghz_psc_minidx(mac_ctx, &chan_info[j], 16,
					     &minIdx);

		chan_info[j + minIdx].weight = combined_weight;
		if (has_valid)
			valid_chans++;

		sap_debug("best freq = %d for 320mhz center freq %d combined weight = %d valid %d cnt %d",
			  chan_info[j + minIdx].chan_freq,
			  acs_ch_params.mhz_freq_seg1,
			  combined_weight,
			  has_valid, valid_chans);
	}

	if (!valid_chans) {
		sap_debug("no valid chan bonding with CH_WIDTH_320MHZ");
		return QDF_STATUS_E_INVAL;
	}

	sap_sort_chl_weight(ch_info_params);

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_11BE */

/**
 * sap_allocate_max_weight_40_mhz_24_g() - allocate max weight for 40Mhz
 *                                       to all 2.4Ghz channels
 * @spect_info_params: Pointer to the tSapChSelSpectInfo structure
 *
 * Return: none
 */
static void
sap_allocate_max_weight_40_mhz_24_g(struct sap_sel_ch_info *spect_info_params)
{
	struct sap_ch_info *spect_info;
	uint8_t j;

	/*
	 * Assign max weight for 40Mhz (SAP_ACS_WEIGHT_MAX * 2) to all
	 * 2.4 Ghz channels
	 */
	spect_info = spect_info_params->ch_info;
	for (j = 0; j < spect_info_params->num_ch; j++) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(spect_info[j].chan_freq))
			spect_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
	}
}

/**
 * sap_allocate_max_weight_40_mhz() - allocate max weight for 40Mhz
 *                                      to all 5Ghz channels
 * @spect_info_params: Pointer to the tSapChSelSpectInfo structure
 *
 * Return: none
 */
static void
sap_allocate_max_weight_40_mhz(struct sap_sel_ch_info *spect_info_params)
{
	struct sap_ch_info *spect_info;
	uint8_t j;

	/*
	 * Assign max weight for 40Mhz (SAP_ACS_WEIGHT_MAX * 2) to all
	 * 5 Ghz channels
	 */
	spect_info = spect_info_params->ch_info;
	for (j = 0; j < spect_info_params->num_ch; j++) {
		if (WLAN_REG_IS_5GHZ_CH_FREQ(spect_info[j].chan_freq) ||
		    WLAN_REG_IS_6GHZ_CHAN_FREQ(spect_info[j].chan_freq))
			spect_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
	}
}

/**
 * sap_sort_chl_weight_ht40_24_g() - To sort channel with the least weight
 * @mac_ctx: Pointer to mac_ctx
 * @ch_info_params: Pointer to the sap_sel_ch_info structure
 * @domain: Regulatory domain
 *
 * Function to sort the channels with the least weight first for HT40 channels
 *
 * Return: none
 */
static void sap_sort_chl_weight_ht40_24_g(
				struct mac_context *mac_ctx,
				struct sap_sel_ch_info *ch_info_params,
				v_REGDOMAIN_t domain)
{
	uint8_t i, j;
	struct sap_ch_info *chan_info;
	uint32_t tmp_weight1, tmp_weight2;
	uint32_t ht40plus2gendch = 0;
	uint32_t channel;
	uint32_t chan_freq;

	chan_info = ch_info_params->ch_info;
	/*
	 * for each HT40 channel, calculate the combined weight of the
	 * two 20MHz weight
	 */
	for (i = 0; i < ARRAY_SIZE(acs_ht40_channels24_g); i++) {
		for (j = 0; j < ch_info_params->num_ch; j++) {
			channel = wlan_reg_freq_to_chan(mac_ctx->pdev,
							chan_info[j].chan_freq);
			if (channel == acs_ht40_channels24_g[i].chStartNum)
				break;
		}
		if (j == ch_info_params->num_ch)
			continue;

		if (!((chan_info[j].chan_freq + 20) ==
		       chan_info[j + 4].chan_freq)) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			continue;
		}
		/*
		 * check if there is another channel combination possibility
		 * e.g., {1, 5} & {5, 9}
		 */
		if ((chan_info[j + 4].chan_freq + 20) ==
		     chan_info[j + 8].chan_freq) {
			/* need to compare two channel pairs */
			tmp_weight1 = chan_info[j].weight +
						chan_info[j + 4].weight;
			tmp_weight2 = chan_info[j + 4].weight +
						chan_info[j + 8].weight;
			if (tmp_weight1 <= tmp_weight2) {
				if (chan_info[j].weight <=
						chan_info[j + 4].weight) {
					chan_info[j].weight =
						tmp_weight1;
					chan_info[j + 4].weight =
						SAP_ACS_WEIGHT_MAX * 2;
					chan_info[j + 8].weight =
						SAP_ACS_WEIGHT_MAX * 2;
				} else {
					chan_info[j + 4].weight =
						tmp_weight1;
					/* for secondary channel selection */
					chan_info[j].weight =
						SAP_ACS_WEIGHT_MAX * 2
						- 1;
					chan_info[j + 8].weight =
						SAP_ACS_WEIGHT_MAX * 2;
				}
			} else {
				if (chan_info[j + 4].weight <=
						chan_info[j + 8].weight) {
					chan_info[j + 4].weight =
						tmp_weight2;
					chan_info[j].weight =
						SAP_ACS_WEIGHT_MAX * 2;
					/* for secondary channel selection */
					chan_info[j + 8].weight =
						SAP_ACS_WEIGHT_MAX * 2
						- 1;
				} else {
					chan_info[j + 8].weight =
						tmp_weight2;
					chan_info[j].weight =
						SAP_ACS_WEIGHT_MAX * 2;
					chan_info[j + 4].weight =
						SAP_ACS_WEIGHT_MAX * 2;
				}
			}
		} else {
			tmp_weight1 = chan_info[j].weight_copy +
						chan_info[j + 4].weight_copy;
			if (chan_info[j].weight_copy <=
					chan_info[j + 4].weight_copy) {
				chan_info[j].weight = tmp_weight1;
				chan_info[j + 4].weight =
					SAP_ACS_WEIGHT_MAX * 2;
			} else {
				chan_info[j + 4].weight = tmp_weight1;
				chan_info[j].weight =
					SAP_ACS_WEIGHT_MAX * 2;
			}
		}
	}
	/*
	 * Every channel should be checked. Add the check for the omissive
	 * channel. Mark the channel whose combination can't satisfy 40MHZ
	 * as max value, so that it will be sorted to the bottom.
	 */
	if (REGDOMAIN_FCC == domain)
		ht40plus2gendch = HT40PLUS_2G_FCC_CH_END;
	else
		ht40plus2gendch = HT40PLUS_2G_EURJAP_CH_END;
	for (i = HT40MINUS_2G_CH_START; i <= ht40plus2gendch; i++) {
		chan_freq = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev, i);
		for (j = 0; j < ch_info_params->num_ch; j++) {
			if (chan_info[j].chan_freq == chan_freq &&
			    ((chan_info[j].chan_freq + 20) !=
					chan_info[j + 4].chan_freq) &&
			    ((chan_info[j].chan_freq - 20) !=
					chan_info[j - 4].chan_freq))
				chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
		}
	}
	for (i = ht40plus2gendch + 1; i <= HT40MINUS_2G_CH_END; i++) {
		chan_freq = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev, i);
		for (j = 0; j < ch_info_params->num_ch; j++) {
			if (chan_info[j].chan_freq == chan_freq &&
			    (chan_info[j].chan_freq - 20) !=
					chan_info[j - 4].chan_freq)
				chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
		}
	}

	chan_info = ch_info_params->ch_info;
	for (j = 0; j < (ch_info_params->num_ch); j++) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_TRACE,
			  "%s: freq = %d weight = %d rssi = %d bss count = %d",
			  __func__, chan_info->chan_freq, chan_info->weight,
			     chan_info->rssi_agr, chan_info->bss_count);

		chan_info++;
	}

	sap_sort_chl_weight(ch_info_params);
}

/**
 * sap_sort_chl_weight_40_mhz() - To sort 5 GHz channel in 40 MHz bandwidth
 * @mac_ctx: mac context handle
 * @sap_ctx: pointer to SAP context
 * @ch_info_params: pointer to the tSapChSelSpectInfo structure
 *
 * Return: QDF STATUS
 */
static QDF_STATUS
sap_sort_chl_weight_40_mhz(struct mac_context *mac_ctx,
			   struct sap_context *sap_ctx,
			   struct sap_sel_ch_info *ch_info_params)
{
	uint8_t i, j;
	struct sap_ch_info *chan_info;
	uint8_t minIdx;
	struct ch_params acs_ch_params = {0};
	int8_t center_freq_diff;
	uint32_t combined_weight;
	uint32_t min_ch_weight;
	uint32_t valid_chans = 0;
	bool has_valid;

	chan_info = ch_info_params->ch_info;

	for (j = 0; j < ch_info_params->num_ch; j++) {

		if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_info[j].chan_freq))
			continue;

		if (chan_info[j].weight_calc_done)
			continue;

		acs_ch_params.ch_width = CH_WIDTH_40MHZ;

		wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev,
							chan_info[j].chan_freq,
							0, &acs_ch_params,
							REG_CURRENT_PWR_MODE);

		/* Check if the freq supports 40 Mhz */
		if (acs_ch_params.ch_width != CH_WIDTH_40MHZ) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		center_freq_diff = acs_ch_params.mhz_freq_seg0 -
				   chan_info[j].chan_freq;

		/* This channel frequency does not have all channels */
		if (center_freq_diff != 10) {
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			chan_info[j].weight_calc_done = true;
			continue;
		}

		/* no other freq left for 40 Mhz operation in spectrum */
		if (j + 1 > ch_info_params->num_ch)
			continue;

		/* Check whether all frequencies are present for 40 Mhz */

		if (!((chan_info[j].chan_freq + 20) ==
		       chan_info[j + 1].chan_freq)) {
			/*
			 * some channels does not exist in pSectInfo array,
			 * skip this channel and those in the same 40 width
			 */
			chan_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			chan_info[j].weight_calc_done = true;

			if ((chan_info[j].chan_freq + 20) ==
					chan_info[j + 1].chan_freq) {
				chan_info[j + 1].weight =
					SAP_ACS_WEIGHT_MAX * 2;
				chan_info[j + 1].weight_calc_done = true;
			}

			continue;
		}

		/* We have 2 channels to calculate cumulative weight */

		combined_weight = chan_info[j].weight +
				  chan_info[j + 1].weight;

		min_ch_weight = chan_info[j].weight;
		minIdx = 0;
		has_valid = false;

		for (i = 0; i < 2; i++) {
			if (min_ch_weight > chan_info[j + i].weight) {
				min_ch_weight = chan_info[j + i].weight;
				minIdx = i;
			}
			chan_info[j + i].weight = SAP_ACS_WEIGHT_MAX * 2;
			chan_info[j + i].weight_calc_done = true;
			if (chan_info[j + i].valid)
				has_valid = true;
		}
		sap_override_6ghz_psc_minidx(mac_ctx, &chan_info[j], 2,
					     &minIdx);

		chan_info[j + minIdx].weight = combined_weight;
		if (has_valid)
			valid_chans++;

		sap_debug("best freq = %d for 40mhz center freq %d combined weight = %d valid %d cnt %d",
			  chan_info[j + minIdx].chan_freq,
			  acs_ch_params.mhz_freq_seg0,
			  combined_weight, has_valid, valid_chans);
	}

	if (!valid_chans) {
		sap_debug("no valid chan bonding with CH_WIDTH_40MHZ");
		return QDF_STATUS_E_INVAL;
	}

	sap_sort_chl_weight(ch_info_params);

	return QDF_STATUS_SUCCESS;
}

/**
 * sap_restore_chan_weight() - Restore every channel weight to original
 * @spect_info: pointer to the tSapChSelSpectInfo structure
 *
 * Return: None
 */
static void sap_restore_chan_weight(struct sap_sel_ch_info *spect_info)
{
	uint32_t i;
	struct sap_ch_info *spect_ch = spect_info->ch_info;

	for (i = 0; i < spect_info->num_ch; i++) {
		spect_ch->weight = spect_ch->weight_copy;
		spect_ch->weight_calc_done = false;
		spect_ch++;
	}
}

/**
 * sap_sort_chl_weight_all() - Function to sort the channels with the least
 * weight first
 * @mac_ctx: Pointer to mac_ctx structure
 * @sap_ctx: Pointer to sap_context structure
 * @ch_info_params: Pointer to the sap_sel_ch_info structure
 * @operating_band: Operating Band
 * @domain: Regulatory domain
 * @bw: Bandwidth
 *
 * Return: NULL
 */
static void sap_sort_chl_weight_all(struct mac_context *mac_ctx,
				    struct sap_context *sap_ctx,
				    struct sap_sel_ch_info *ch_info_params,
				    uint32_t operating_band,
				    v_REGDOMAIN_t domain,
				    enum phy_ch_width *bw)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum phy_ch_width ch_width = *bw;

next_bw:
	switch (ch_width) {
	case CH_WIDTH_40MHZ:
		/*
		 * Assign max weight to all 5Ghz channels when operating band
		 * is 11g and to all 2.4Ghz channels when operating band is 11a
		 * or 11abg to avoid selection in ACS algorithm for starting SAP
		 */
		if (eCSR_DOT11_MODE_11g == operating_band) {
			sap_allocate_max_weight_40_mhz(ch_info_params);
			sap_sort_chl_weight_ht40_24_g(
					mac_ctx,
					ch_info_params,
					domain);
		} else {
			sap_allocate_max_weight_40_mhz_24_g(ch_info_params);
			status = sap_sort_chl_weight_40_mhz(mac_ctx,
							    sap_ctx,
							    ch_info_params);
		}
		break;
	case CH_WIDTH_80MHZ:
	case CH_WIDTH_80P80MHZ:
		status = sap_sort_chl_weight_80_mhz(mac_ctx,
						    sap_ctx,
						    ch_info_params);
		break;
	case CH_WIDTH_160MHZ:
		status = sap_sort_chl_weight_160_mhz(mac_ctx,
						     sap_ctx,
						     ch_info_params);
		break;
#if defined(WLAN_FEATURE_11BE)
	case CH_WIDTH_320MHZ:
		status = sap_sort_chl_weight_320_mhz(mac_ctx,
						     sap_ctx,
						     ch_info_params);
		break;
#endif
	case CH_WIDTH_20MHZ:
	default:
		/* Sorting the channels as per weights as 20MHz channels */
		sap_sort_chl_weight(ch_info_params);
		status = QDF_STATUS_SUCCESS;
	}

	if (status != QDF_STATUS_SUCCESS) {
		ch_width = wlan_reg_get_next_lower_bandwidth(ch_width);
		sap_restore_chan_weight(ch_info_params);
		goto next_bw;
	}

	if (ch_width != *bw) {
		sap_info("channel width change from %d to %d", *bw, ch_width);
		*bw = ch_width;
	}
}

/**
 * sap_is_ch_non_overlap() - returns true if non-overlapping channel
 * @sap_ctx: Sap context
 * @ch: channel number
 *
 * Returns: true if non-overlapping (1, 6, 11) channel, false otherwise
 */
static bool sap_is_ch_non_overlap(struct sap_context *sap_ctx, uint16_t ch)
{
	if (sap_ctx->enableOverLapCh)
		return true;

	if ((ch == CHANNEL_1) || (ch == CHANNEL_6) || (ch == CHANNEL_11))
		return true;

	return false;
}

static enum phy_ch_width
sap_acs_next_lower_bandwidth(enum phy_ch_width ch_width)
{
	if (ch_width <= CH_WIDTH_20MHZ ||
	    ch_width == CH_WIDTH_5MHZ ||
	    ch_width == CH_WIDTH_10MHZ ||
	    ch_width >= CH_WIDTH_INVALID)
		return CH_WIDTH_INVALID;

	return wlan_reg_get_next_lower_bandwidth(ch_width);
}

void sap_sort_channel_list(struct mac_context *mac_ctx, uint8_t vdev_id,
			   qdf_list_t *ch_list, struct sap_sel_ch_info *ch_info,
			   v_REGDOMAIN_t *domain, uint32_t *operating_band)
{
	uint8_t country[CDS_COUNTRY_CODE_LEN + 1];
	struct sap_context *sap_ctx;
	enum phy_ch_width cur_bw;
	v_REGDOMAIN_t reg_domain;
	uint32_t op_band;

	sap_ctx = mac_ctx->sap.sapCtxList[vdev_id].sap_context;
	cur_bw = sap_ctx->acs_cfg->ch_width;

	/* Initialize the structure pointed by spect_info */
	if (!sap_chan_sel_init(mac_ctx, ch_info, sap_ctx, false)) {
		sap_err("vdev %d ch select initialization failed", vdev_id);
		return;
	}

	/* Compute the weight of the entire spectrum in the operating band */
	sap_compute_spect_weight(ch_info, mac_ctx, ch_list, sap_ctx);

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	/* process avoid channel IE to collect all channels to avoid */
	sap_process_avoid_ie(mac_ctx, sap_ctx, ch_list, ch_info);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

	wlan_reg_read_current_country(mac_ctx->psoc, country);
	wlan_reg_get_domain_from_country_code(&reg_domain, country,
					      SOURCE_DRIVER);

	SET_ACS_BAND(op_band, sap_ctx);

	/* Sort the ch lst as per the computed weights, lesser weight first. */
	sap_sort_chl_weight_all(mac_ctx, sap_ctx, ch_info, op_band,
				reg_domain, &cur_bw);
	sap_ctx->acs_cfg->ch_width = cur_bw;

	if (domain)
		*domain = reg_domain;
	if (operating_band)
		*operating_band = op_band;
}

uint32_t sap_select_channel(mac_handle_t mac_handle,
			    struct sap_context *sap_ctx,
			    qdf_list_t *scan_list)
{
	/* DFS param object holding all the data req by the algo */
	struct sap_sel_ch_info spect_info_obj = { NULL, 0 };
	struct sap_sel_ch_info *spect_info = &spect_info_obj;
	uint8_t best_ch_num = SAP_CHANNEL_NOT_SELECTED;
	uint32_t best_ch_weight = SAP_ACS_WEIGHT_MAX;
	uint32_t ht40plus2gendch = 0;
	v_REGDOMAIN_t domain;
	uint8_t count;
	uint32_t operating_band = 0;
	struct mac_context *mac_ctx;
	uint32_t best_chan_freq = 0;

	mac_ctx = MAC_CONTEXT(mac_handle);

	sap_sort_channel_list(mac_ctx, sap_ctx->vdev_id, scan_list,
			      spect_info, &domain, &operating_band);

	/*Loop till get the best channel in the given range */
	for (count = 0; count < spect_info->num_ch; count++) {
		if (!spect_info->ch_info[count].valid)
			continue;

		best_chan_freq = spect_info->ch_info[count].chan_freq;
		/* check if best_ch_num is in preferred channel list */
		best_chan_freq =
			sap_select_preferred_channel_from_channel_list(
				best_chan_freq, sap_ctx, spect_info);
		/* if not in preferred ch lst, go to nxt best ch */
		if (best_chan_freq == SAP_CHANNEL_NOT_SELECTED)
			continue;

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		/*
		 * Weight of the channels(device's AP is operating)
		 * increased to MAX+1 so that they will be chosen only
		 * when there is no other best channel to choose
		 */
		if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(best_chan_freq) &&
		    sap_check_in_avoid_ch_list(sap_ctx,
		    wlan_reg_freq_to_chan(mac_ctx->pdev, best_chan_freq))) {
			best_chan_freq = SAP_CHANNEL_NOT_SELECTED;
			continue;
		}
#endif

		/* Give preference to Non-overlap channels */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(best_chan_freq) &&
		    !sap_is_ch_non_overlap(sap_ctx,
		    wlan_reg_freq_to_chan(mac_ctx->pdev, best_chan_freq))) {
			sap_debug("freq %d is overlapping, skipped",
				  best_chan_freq);
			continue;
		}

		best_ch_weight = spect_info->ch_info[count].weight;
		sap_debug("Freq = %d selected as best frequency weight = %d",
			  best_chan_freq, best_ch_weight);

		break;
	}

	/*
	 * in case the best channel selected is not in PCL and there is another
	 * channel which has same weightage and is in PCL, choose the one in
	 * PCL
	 */
	if (!ch_in_pcl(sap_ctx, best_chan_freq)) {
		uint32_t cal_chan_freq, cal_chan_weight;

		enum phy_ch_width pref_bw = sap_ctx->acs_cfg->ch_width;
next_bw:
		sap_debug("check bw %d", pref_bw);
		for (count = 0; count < spect_info->num_ch; count++) {
			struct ch_params ch_params = {0};

			if (!spect_info->ch_info[count].valid)
				continue;

			cal_chan_freq = spect_info->ch_info[count].chan_freq;
			cal_chan_weight = spect_info->ch_info[count].weight;
			/* skip pcl channel whose weight is bigger than best */
			if (!ch_in_pcl(sap_ctx, cal_chan_freq) ||
			    (cal_chan_weight > best_ch_weight))
				continue;

			if (best_chan_freq == cal_chan_freq)
				continue;

			if (sap_select_preferred_channel_from_channel_list(
				cal_chan_freq, sap_ctx,
				spect_info)
				== SAP_CHANNEL_NOT_SELECTED)
				continue;
			ch_params.ch_width = pref_bw;
			sap_acs_set_puncture_support(sap_ctx, &ch_params);
			wlan_reg_set_channel_params_for_pwrmode(
				mac_ctx->pdev, cal_chan_freq, 0, &ch_params,
				REG_CURRENT_PWR_MODE);
			if (ch_params.ch_width != pref_bw)
				continue;
			best_chan_freq = cal_chan_freq;
			sap_ctx->acs_cfg->ch_width = pref_bw;
			sap_debug("Changed best freq to %d Preferred freq bw %d",
				  best_chan_freq, pref_bw);
			break;
		}
		if (count == spect_info->num_ch) {
			pref_bw = sap_acs_next_lower_bandwidth(pref_bw);
			if (pref_bw != CH_WIDTH_INVALID)
				goto next_bw;
		}
	}

	sap_ctx->acs_cfg->pri_ch_freq = best_chan_freq;

	/* Below code is for 2.4Ghz freq, so freq to channel is safe here */

	/* determine secondary channel for 2.4G channel 5, 6, 7 in HT40 */
	if ((operating_band != eCSR_DOT11_MODE_11g) ||
	    (sap_ctx->acs_cfg->ch_width != CH_WIDTH_40MHZ))
		goto sap_ch_sel_end;

	best_ch_num = wlan_reg_freq_to_chan(mac_ctx->pdev, best_chan_freq);

	if (REGDOMAIN_FCC == domain)
		ht40plus2gendch = HT40PLUS_2G_FCC_CH_END;
	else
		ht40plus2gendch = HT40PLUS_2G_EURJAP_CH_END;
	if ((best_ch_num >= HT40MINUS_2G_CH_START) &&
			(best_ch_num <= ht40plus2gendch)) {
		int weight_below, weight_above, i;
		struct sap_ch_info *pspect_info;

		weight_below = weight_above = SAP_ACS_WEIGHT_MAX;
		pspect_info = spect_info->ch_info;
		for (i = 0; i < spect_info->num_ch; i++) {
			if (pspect_info[i].chan_freq == (best_chan_freq - 20))
				weight_below = pspect_info[i].weight;
			if (pspect_info[i].chan_freq == (best_ch_num + 20))
				weight_above = pspect_info[i].weight;
		}

		if (weight_below < weight_above)
			sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq - 20;
		else
			sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq + 20;
	} else if (best_ch_num >= 1 && best_ch_num <= 4) {
		sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq + 20;
	} else if (best_ch_num >= ht40plus2gendch && best_ch_num <=
			HT40MINUS_2G_CH_END) {
		sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq - 20;
	} else if (best_ch_num == 14) {
		sap_ctx->acs_cfg->ht_sec_ch_freq = 0;
	}
	sap_ctx->sec_ch_freq = sap_ctx->acs_cfg->ht_sec_ch_freq;

sap_ch_sel_end:
	/* Free all the allocated memory */
	sap_chan_sel_exit(spect_info);

	return best_chan_freq;
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * sap_max_weight_invalidate_2ghz_channels() - Invalidate 2 GHz channel and set
 *                                             max channel weight
 * @spect_info: pointer to array of channel spectrum info
 *
 * Return: None
 */
static void
sap_max_weight_invalidate_2ghz_channels(struct sap_sel_ch_info *spect_info)
{
	uint32_t i;
	struct sap_ch_info *spect_ch;

	spect_ch = spect_info->ch_info;
	for (i = 0; i < spect_info->num_ch; i++) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(spect_ch[i].chan_freq)) {
			spect_ch[i].weight = SAP_ACS_WEIGHT_MAX;
			spect_ch[i].valid = false;
		}
	}
}

/**
 * sap_compute_spect_max_power_weight() - Compute channel weight use max power
 *                                        factor
 * @spect_info: pointer to SAP channel select structure of spectrum info
 * @mac: mac context
 * @sap_ctx: pointer to SAP context
 *
 * Return: None
 */
static void
sap_compute_spect_max_power_weight(struct sap_sel_ch_info *spect_info,
				   struct mac_context *mac,
				   struct sap_context *sap_ctx)
{
	uint32_t i;
	struct sap_ch_info *spect_ch = spect_info->ch_info;

	for (i = 0; i < spect_info->num_ch; i++) {
		if (spect_ch[i].weight == SAP_ACS_WEIGHT_MAX) {
			spect_ch[i].weight_copy = spect_ch[i].weight;
			continue;
		}
		spect_ch[i].weight = SAPDFS_NORMALISE_1000 *
			sap_weight_channel_reg_max_power(sap_ctx,
							 spect_ch[i].chan_freq);

		sap_normalize_channel_weight_with_factors(mac, &spect_ch[i]);

		if (spect_ch[i].weight > SAP_ACS_WEIGHT_MAX)
			spect_ch[i].weight = SAP_ACS_WEIGHT_MAX;
		spect_ch[i].weight_copy = spect_ch[i].weight;

		sap_debug("freq = %d, weight = %d",
			  spect_ch[i].chan_freq, spect_ch[i].weight);
	}
}

/**
 * sap_afc_dcs_target_chan() - Select best channel frequency from sorted list
 * @mac_ctx: pointer to mac context
 * @sap_ctx: pointer to SAP context
 * @spect_info: pointer to SAP channel select structure of spectrum info
 * @cur_freq: SAP current home channel frequency
 * @cur_bw: SAP current channel bandwidth
 * @pref_bw: SAP target channel bandwidth can switch to
 *
 * Return: Best home channel frequency, if no available channel return 0.
 */
static qdf_freq_t
sap_afc_dcs_target_chan(struct mac_context *mac_ctx,
			struct sap_context *sap_ctx,
			struct sap_sel_ch_info *spect_info,
			qdf_freq_t cur_freq,
			enum phy_ch_width cur_bw,
			enum phy_ch_width pref_bw)
{
	uint32_t i, best_weight;
	qdf_freq_t best_chan_freq;
	struct sap_ch_info *spect_ch = spect_info->ch_info;

	best_weight = spect_ch[0].weight;
	best_chan_freq = spect_ch[0].chan_freq;

	/*
	 * If current channel is already best channel and no bandwidth
	 * change, return the current channel so no channel switch happen.
	 */
	if (cur_bw == pref_bw) {
		for (i = 1; i < spect_info->num_ch; i++) {
			if (!spect_ch[i].valid)
				continue;
			if (spect_ch[i].weight <= best_weight) {
				sap_debug("best freq = %d, weight = %d",
					  spect_ch[i].chan_freq,
					  spect_ch[i].weight);
				if (spect_ch[i].chan_freq == cur_freq)
					return cur_freq;
			}
		}
	}

	return best_chan_freq;
}

#ifdef WLAN_FEATURE_AFC_DCS_SKIP_ACS_RANGE
/**
 * is_sap_afc_dcs_skip_acs() - API to get whether to skip ACS range
 * when doing automatically channel selection for AFC DCS.
 * @sap_ctx: SAP context pointer
 *
 * Return: True if skip ACS range and can select channel out of it.
 */
static bool is_sap_afc_dcs_skip_acs(struct sap_context *sap_ctx)
{
	struct sap_acs_cfg *acs_cfg;
	uint32_t i;

	if (!sap_ctx || !sap_ctx->acs_cfg)
		return false;

	acs_cfg = sap_ctx->acs_cfg;
	for (i = 0; i < acs_cfg->ch_list_count; i++) {
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(acs_cfg->freq_list[i]))
			return false;
	}
	return true;
}
#else
static bool is_sap_afc_dcs_skip_acs(struct sap_context *sap_ctx)
{
	return false;
}
#endif

qdf_freq_t sap_afc_dcs_sel_chan(struct sap_context *sap_ctx,
				qdf_freq_t cur_freq,
				enum phy_ch_width cur_bw,
				enum phy_ch_width *pref_bw)
{
	struct mac_context *mac_ctx;
	mac_handle_t mac_handle;
	struct sap_sel_ch_info spect_info_obj = {NULL, 0};
	struct sap_sel_ch_info *spect_info = &spect_info_obj;
	qdf_freq_t target_freq;

	if (!sap_ctx || !pref_bw)
		return SAP_CHANNEL_NOT_SELECTED;

	if (!sap_ctx->acs_cfg || !sap_ctx->acs_cfg->acs_mode) {
		sap_debug("SAP session id %d acs not enable",
			  sap_ctx->sessionId);
		return SAP_CHANNEL_NOT_SELECTED;
	}

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx)
		return SAP_CHANNEL_NOT_SELECTED;

	/*
	 * If AFC response received after SAP started, SP channels are
	 * not included in current ACS range, ignore ACS range check
	 * in this scenario so that SAP can move to new SP channel.
	 */
	sap_chan_sel_init(mac_ctx, spect_info, sap_ctx,
			  is_sap_afc_dcs_skip_acs(sap_ctx));

	sap_max_weight_invalidate_2ghz_channels(spect_info);

	sap_compute_spect_max_power_weight(spect_info, mac_ctx, sap_ctx);

	sap_sort_chl_weight_all(mac_ctx, sap_ctx, spect_info,
				eCSR_DOT11_MODE_11a, REGDOMAIN_FCC, pref_bw);

	target_freq = sap_afc_dcs_target_chan(mac_ctx,
					      sap_ctx,
					      spect_info,
					      cur_freq,
					      cur_bw,
					      *pref_bw);
	sap_chan_sel_exit(spect_info);
	return target_freq;
}
#endif
