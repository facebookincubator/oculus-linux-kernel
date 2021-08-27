/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
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
 * DOC: reg_build_chan_list.c
 * This file defines the API to build master and current channel list.
 */

#include <wlan_cmn.h>
#include <reg_services_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include "reg_priv_objs.h"
#include "reg_utils.h"
#include "reg_callbacks.h"
#include "reg_services_common.h"
#include "reg_db.h"
#include "reg_db_parser.h"
#include "reg_offload_11d_scan.h"
#include <scheduler_api.h>
#include "reg_build_chan_list.h"
#include <qdf_platform.h>

#define MAX_PWR_FCC_CHAN_12 8
#define MAX_PWR_FCC_CHAN_13 2
#define CHAN_144_CENT_FREQ 5720

#ifdef CONFIG_BAND_6GHZ
static void reg_fill_psd_info(enum channel_enum chan_enum,
			      struct cur_reg_rule *reg_rule,
			      struct regulatory_channel *master_list)
{
	master_list[chan_enum].psd_flag = reg_rule->psd_flag;

	master_list[chan_enum].psd_eirp = reg_rule->psd_eirp;
}
#else
static inline void reg_fill_psd_info(enum channel_enum chan_enum,
				     struct cur_reg_rule *reg_rule,
				     struct regulatory_channel *master_list)
{
}
#endif

/**
 * reg_fill_channel_info() - Populate TX power, antenna gain, channel state,
 * channel flags, min and max bandwidth to master channel list.
 * @chan_enum: Channel enum.
 * @reg_rule: Pointer to regulatory rule which has tx power and antenna gain.
 * @master_list: Pointer to master channel list.
 * @min_bw: minimum bandwidth to be used for given channel.
 */
static void reg_fill_channel_info(enum channel_enum chan_enum,
				  struct cur_reg_rule *reg_rule,
				  struct regulatory_channel *master_list,
				  uint16_t min_bw)
{
	master_list[chan_enum].chan_flags &= ~REGULATORY_CHAN_DISABLED;

	reg_fill_psd_info(chan_enum, reg_rule, master_list);
	master_list[chan_enum].tx_power = reg_rule->reg_power;
	master_list[chan_enum].ant_gain = reg_rule->ant_gain;
	master_list[chan_enum].state = CHANNEL_STATE_ENABLE;

	if (reg_rule->flags & REGULATORY_CHAN_NO_IR) {
		master_list[chan_enum].chan_flags |= REGULATORY_CHAN_NO_IR;
		master_list[chan_enum].state = CHANNEL_STATE_DFS;
	}

	if (reg_rule->flags & REGULATORY_CHAN_RADAR) {
		master_list[chan_enum].chan_flags |= REGULATORY_CHAN_RADAR;
		master_list[chan_enum].state = CHANNEL_STATE_DFS;
	}

	if (reg_rule->flags & REGULATORY_CHAN_INDOOR_ONLY)
		master_list[chan_enum].chan_flags |=
			REGULATORY_CHAN_INDOOR_ONLY;

	if (reg_rule->flags & REGULATORY_CHAN_NO_OFDM)
		master_list[chan_enum].chan_flags |= REGULATORY_CHAN_NO_OFDM;

	master_list[chan_enum].min_bw = min_bw;
	if (master_list[chan_enum].max_bw == 20)
		master_list[chan_enum].max_bw = reg_rule->max_bw;
}

#ifdef CONFIG_BAND_6GHZ
/**
 * reg_populate_band_channels_ext_for_6g() - For all the valid regdb channels in
 *	the master channel list, find the regulatory rules and call
 *	reg_fill_channel_info() to populate master channel list with txpower,
 *	antennagain, BW info, etc.
 * @start_idx: Start channel range in list
 * @end_idx: End channel range in list
 * @rule_start_ptr: Pointer to regulatory rules
 * @num_reg_rules: Number of regulatory rules
 * @min_reg_bw: Minimum regulatory bandwidth
 * @mas_chan_list: Pointer to master channel list
 */
static void reg_populate_band_channels_ext_for_6g(uint16_t start_idx,
				       uint16_t end_idx,
				       struct cur_reg_rule *rule_start_ptr,
				       uint32_t num_reg_rules,
				       uint16_t min_reg_bw,
				       struct regulatory_channel *mas_chan_list)
{
	struct cur_reg_rule *found_rule_ptr;
	struct cur_reg_rule *cur_rule_ptr;
	struct regulatory_channel;
	uint32_t rule_num, bw;
	uint16_t i, min_bw, max_bw;

	for (i = start_idx; i <= end_idx; i++) {
		found_rule_ptr = NULL;

		max_bw = QDF_MIN((uint16_t)20,
				 channel_map[MIN_6GHZ_CHANNEL + i].max_bw);
		min_bw = QDF_MAX(min_reg_bw,
				 channel_map[MIN_6GHZ_CHANNEL + i].min_bw);

		if (channel_map[MIN_6GHZ_CHANNEL + i].chan_num ==
		    INVALID_CHANNEL_NUM)
			continue;

		for (bw = max_bw; bw >= min_bw; bw = bw / 2) {
			for (rule_num = 0, cur_rule_ptr = rule_start_ptr;
			     rule_num < num_reg_rules;
			     cur_rule_ptr++, rule_num++) {
				if ((cur_rule_ptr->start_freq <=
				     mas_chan_list[i].center_freq -
				     bw / 2) &&
				    (cur_rule_ptr->end_freq >=
				     mas_chan_list[i].center_freq +
				     bw / 2) && (min_bw <= bw)) {
					found_rule_ptr = cur_rule_ptr;
					break;
				}
			}

			if (found_rule_ptr)
				break;
		}

		if (found_rule_ptr) {
			mas_chan_list[i].max_bw = bw;
			reg_fill_channel_info(i, found_rule_ptr,
					      mas_chan_list, min_bw);
		}
	}
}
#else
static inline void
reg_populate_band_channels_ext_for_6g(enum channel_enum start_chan,
				      enum channel_enum end_chan,
				      struct cur_reg_rule *rule_start_ptr,
				      uint32_t num_reg_rules,
				      uint16_t min_reg_bw,
				      struct regulatory_channel *mas_chan_list)
{
}
#endif

/**
 * reg_populate_band_channels() - For all the valid regdb channels in the master
 * channel list, find the regulatory rules and call reg_fill_channel_info() to
 * populate master channel list with txpower, antennagain, BW info, etc.
 * @start_chan: Start channel enum.
 * @end_chan: End channel enum.
 * @rule_start_ptr: Pointer to regulatory rules.
 * @num_reg_rules: Number of regulatory rules.
 * @min_reg_bw: Minimum regulatory bandwidth.
 * @mas_chan_list: Pointer to master channel list.
 */
static void reg_populate_band_channels(enum channel_enum start_chan,
				       enum channel_enum end_chan,
				       struct cur_reg_rule *rule_start_ptr,
				       uint32_t num_reg_rules,
				       uint16_t min_reg_bw,
				       struct regulatory_channel *mas_chan_list)
{
	struct cur_reg_rule *found_rule_ptr;
	struct cur_reg_rule *cur_rule_ptr;
	struct regulatory_channel;
	enum channel_enum chan_enum;
	uint32_t rule_num, bw;
	uint16_t max_bw;
	uint16_t min_bw;

	for (chan_enum = start_chan; chan_enum <= end_chan; chan_enum++) {
		found_rule_ptr = NULL;

		max_bw = QDF_MIN((uint16_t)20, channel_map[chan_enum].max_bw);
		min_bw = QDF_MAX(min_reg_bw, channel_map[chan_enum].min_bw);

		if (channel_map[chan_enum].chan_num == INVALID_CHANNEL_NUM)
			continue;

		for (bw = max_bw; bw >= min_bw; bw = bw / 2) {
			for (rule_num = 0, cur_rule_ptr = rule_start_ptr;
			     rule_num < num_reg_rules;
			     cur_rule_ptr++, rule_num++) {
				if ((cur_rule_ptr->start_freq <=
				     mas_chan_list[chan_enum].center_freq -
				     bw / 2) &&
				    (cur_rule_ptr->end_freq >=
				     mas_chan_list[chan_enum].center_freq +
				     bw / 2) && (min_bw <= bw)) {
					found_rule_ptr = cur_rule_ptr;
					break;
				}
			}

			if (found_rule_ptr)
				break;
		}

		if (found_rule_ptr) {
			mas_chan_list[chan_enum].max_bw = bw;
			reg_fill_channel_info(chan_enum, found_rule_ptr,
					      mas_chan_list, min_bw);
			/* Disable 2.4 Ghz channels that dont have 20 mhz bw */
			if (start_chan == MIN_24GHZ_CHANNEL &&
			    mas_chan_list[chan_enum].max_bw < 20) {
				mas_chan_list[chan_enum].chan_flags |=
						REGULATORY_CHAN_DISABLED;
				mas_chan_list[chan_enum].state =
						CHANNEL_STATE_DISABLE;
			}
		}
	}
}

/**
 * reg_update_max_bw_per_rule() - Update max bandwidth value for given regrules.
 * @num_reg_rules: Number of regulatory rules.
 * @reg_rule_start: Pointer to regulatory rules.
 * @max_bw: Maximum bandwidth
 */
static void reg_update_max_bw_per_rule(uint32_t num_reg_rules,
				       struct cur_reg_rule *reg_rule_start,
				       uint16_t max_bw)
{
	uint32_t count;

	for (count = 0; count < num_reg_rules; count++)
		reg_rule_start[count].max_bw =
			min(reg_rule_start[count].max_bw, max_bw);
}

/**
 * reg_do_auto_bw_correction() - Calculate and update the maximum bandwidth
 * value.
 * @num_reg_rules: Number of regulatory rules.
 * @reg_rule_ptr: Pointer to regulatory rules.
 * @max_bw: Maximum bandwidth
 */
static void reg_do_auto_bw_correction(uint32_t num_reg_rules,
				      struct cur_reg_rule *reg_rule_ptr,
				      uint16_t max_bw)
{
	uint32_t count;
	uint16_t new_bw;

	for (count = 0; count < num_reg_rules - 1; count++) {
		if (reg_rule_ptr[count].end_freq ==
		    reg_rule_ptr[count + 1].start_freq) {
			new_bw = QDF_MIN(max_bw, reg_rule_ptr[count].max_bw +
					 reg_rule_ptr[count + 1].max_bw);
			reg_rule_ptr[count].max_bw = new_bw;
			reg_rule_ptr[count + 1].max_bw = new_bw;
		}
	}
}

/**
 * reg_modify_chan_list_for_dfs_channels() - disable the DFS channels if
 * dfs_enable set to false.
 * @chan_list: Pointer to regulatory channel list.
 * @dfs_enabled: if false, then disable the DFS channels.
 */
static void reg_modify_chan_list_for_dfs_channels(
		struct regulatory_channel *chan_list, bool dfs_enabled)
{
	enum channel_enum chan_enum;

	if (dfs_enabled)
		return;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		if (chan_list[chan_enum].state == CHANNEL_STATE_DFS) {
			chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_DISABLED;
		}
	}
}

/**
 * reg_modify_chan_list_for_indoor_channels() - Disable the indoor channels if
 * indoor_chan_enabled flag is set to false.
 * @pdev_priv_obj: Pointer to regulatory private pdev structure.
 */
static void reg_modify_chan_list_for_indoor_channels(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	enum channel_enum chan_enum;
	struct regulatory_channel *chan_list = pdev_priv_obj->cur_chan_list;

	if (!pdev_priv_obj->indoor_chan_enabled) {
		for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
			if (REGULATORY_CHAN_INDOOR_ONLY &
			    chan_list[chan_enum].chan_flags) {
				chan_list[chan_enum].state =
					CHANNEL_STATE_DFS;
				chan_list[chan_enum].chan_flags |=
					REGULATORY_CHAN_NO_IR;
			}
		}
	}

	if (pdev_priv_obj->force_ssc_disable_indoor_channel &&
	    pdev_priv_obj->sap_state) {
		for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
			if (REGULATORY_CHAN_INDOOR_ONLY &
			    chan_list[chan_enum].chan_flags) {
				chan_list[chan_enum].state =
					CHANNEL_STATE_DISABLE;
				chan_list[chan_enum].chan_flags |=
					REGULATORY_CHAN_DISABLED;
			}
		}
	}
}

#ifdef CONFIG_BAND_6GHZ
static void reg_modify_chan_list_for_band_6G(
					struct regulatory_channel *chan_list)
{
	enum channel_enum chan_enum;

	reg_debug("disabling 6G");
	for (chan_enum = MIN_6GHZ_CHANNEL;
	     chan_enum <= MAX_6GHZ_CHANNEL; chan_enum++) {
		chan_list[chan_enum].chan_flags |=
			REGULATORY_CHAN_DISABLED;
		chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
	}
}
#else
static inline void reg_modify_chan_list_for_band_6G(
					struct regulatory_channel *chan_list)
{
}
#endif

/**
 * reg_modify_chan_list_for_band() - Based on the input band bitmap, either
 * disable 2GHz, 5GHz, or 6GHz channels.
 * @chan_list: Pointer to regulatory channel list.
 * @band_bitmap: Input bitmap of reg_wifi_band values.
 */
static void reg_modify_chan_list_for_band(struct regulatory_channel *chan_list,
					  uint32_t band_bitmap)
{
	enum channel_enum chan_enum;

	if (!band_bitmap)
		return;

	if (!(band_bitmap & BIT(REG_BAND_5G))) {
		reg_debug("disabling 5G");
		for (chan_enum = MIN_5GHZ_CHANNEL;
		     chan_enum <= MAX_5GHZ_CHANNEL; chan_enum++) {
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_DISABLED;
			chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
		}
	}

	if (!(band_bitmap & BIT(REG_BAND_2G))) {
		reg_debug("disabling 2G");
		for (chan_enum = MIN_24GHZ_CHANNEL;
		     chan_enum <= MAX_24GHZ_CHANNEL; chan_enum++) {
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_DISABLED;
			chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
		}
	}

	if (!(band_bitmap & BIT(REG_BAND_6G)))
		reg_modify_chan_list_for_band_6G(chan_list);

}

/**
 * reg_modify_chan_list_for_fcc_channel() - Set maximum FCC txpower for channel
 * 12 and 13 if set_fcc_channel flag is set to true.
 * @chan_list: Pointer to regulatory channel list.
 * @set_fcc_channel: If this flag is set to true, then set the max FCC txpower
 * for channel 12 and 13.
 */
static void reg_modify_chan_list_for_fcc_channel(
		struct regulatory_channel *chan_list, bool set_fcc_channel)
{
	enum channel_enum chan_enum;

	if (!set_fcc_channel)
		return;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		if (chan_list[chan_enum].center_freq == CHAN_12_CENT_FREQ)
			chan_list[chan_enum].tx_power = MAX_PWR_FCC_CHAN_12;

		if (chan_list[chan_enum].center_freq == CHAN_13_CENT_FREQ)
			chan_list[chan_enum].tx_power = MAX_PWR_FCC_CHAN_13;
	}
}

/**
 * reg_modify_chan_list_for_chan_144() - Disable channel 144 if en_chan_144 flag
 * is set to false.
 * @chan_list: Pointer to regulatory channel list.
 * @en_chan_144: if false, then disable channel 144.
 */
static void reg_modify_chan_list_for_chan_144(
		struct regulatory_channel *chan_list, bool en_chan_144)
{
	enum channel_enum chan_enum;

	if (en_chan_144)
		return;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		if (chan_list[chan_enum].center_freq == CHAN_144_CENT_FREQ) {
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_DISABLED;
			chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
		}
	}
}

/**
 * reg_modify_chan_list_for_nol_list() - Disable the channel if nol_chan flag is
 * set.
 * @chan_list: Pointer to regulatory channel list.
 */
static void reg_modify_chan_list_for_nol_list(
		struct regulatory_channel *chan_list)
{
	enum channel_enum chan_enum;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		if (chan_list[chan_enum].nol_chan) {
			chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_DISABLED;
		}
	}
}

/**
 * reg_find_low_limit_chan_enum() - Find low limit 2G and 5G channel enums.
 * @chan_list: Pointer to regulatory channel list.
 * @low_freq: low limit frequency.
 * @low_limit: pointer to output low limit enum.
 *
 * Return: None
 */
static void reg_find_low_limit_chan_enum(
		struct regulatory_channel *chan_list, qdf_freq_t low_freq,
		uint32_t *low_limit)
{
	enum channel_enum chan_enum;
	uint16_t min_bw;
	uint16_t max_bw;
	qdf_freq_t center_freq;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		min_bw = chan_list[chan_enum].min_bw;
		max_bw = chan_list[chan_enum].max_bw;
		center_freq = chan_list[chan_enum].center_freq;

		if ((center_freq - min_bw / 2) >= low_freq) {
			if ((center_freq - max_bw / 2) < low_freq) {
				if (max_bw <= 20)
					max_bw = ((center_freq - low_freq) * 2);
				if (max_bw < min_bw)
					max_bw = min_bw;
				chan_list[chan_enum].max_bw = max_bw;
			}
			*low_limit = chan_enum;
			break;
		}
	}
}

/**
 * reg_find_high_limit_chan_enum() - Find high limit 2G and 5G channel enums.
 * @chan_list: Pointer to regulatory channel list.
 * @high_freq: high limit frequency.
 * @high_limit: pointer to output high limit enum.
 *
 * Return: None
 */
static void reg_find_high_limit_chan_enum(
		struct regulatory_channel *chan_list, qdf_freq_t high_freq,
		uint32_t *high_limit)
{
	enum channel_enum chan_enum;
	uint16_t min_bw;
	uint16_t max_bw;
	qdf_freq_t center_freq;

	for (chan_enum = NUM_CHANNELS - 1; chan_enum >= 0; chan_enum--) {
		min_bw = chan_list[chan_enum].min_bw;
		max_bw = chan_list[chan_enum].max_bw;
		center_freq = chan_list[chan_enum].center_freq;

		if (center_freq + min_bw / 2 <= high_freq) {
			if ((center_freq + max_bw / 2) > high_freq) {
				if (max_bw <= 20)
					max_bw = ((high_freq -
						   center_freq) * 2);
				if (max_bw < min_bw)
					max_bw = min_bw;
				chan_list[chan_enum].max_bw = max_bw;
			}
			*high_limit = chan_enum;
			break;
		}

		if (chan_enum == 0)
			break;
	}
}

#ifdef REG_DISABLE_JP_CH144
/**
 * reg_modify_chan_list_for_japan() - Disable channel 144 for MKK17_MKKC
 * regdomain by default.
 * @pdev: Pointer to pdev
 *
 * Return: None
 */
static void
reg_modify_chan_list_for_japan(struct wlan_objmgr_pdev *pdev)
{
#define MKK17_MKKC 0xE1
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	if (pdev_priv_obj->reg_dmn_pair == MKK17_MKKC)
		pdev_priv_obj->en_chan_144 = false;

#undef MKK17_MKKC
}
#else
static inline void
reg_modify_chan_list_for_japan(struct wlan_objmgr_pdev *pdev)
{
}
#endif
/**
 * reg_modify_chan_list_for_freq_range() - Modify channel list for the given low
 * and high frequency range.
 * @chan_list: Pointer to regulatory channel list.
 * @low_freq_2g: Low frequency 2G.
 * @high_freq_2g: High frequency 2G.
 * @low_freq_5g: Low frequency 5G.
 * @high_freq_5g: High frequency 5G.
 *
 * Return: None
 */
static void
reg_modify_chan_list_for_freq_range(struct regulatory_channel *chan_list,
				    qdf_freq_t low_freq_2g,
				    qdf_freq_t high_freq_2g,
				    qdf_freq_t low_freq_5g,
				    qdf_freq_t high_freq_5g)
{
	uint32_t low_limit_2g = NUM_CHANNELS;
	uint32_t high_limit_2g = NUM_CHANNELS;
	uint32_t low_limit_5g = NUM_CHANNELS;
	uint32_t high_limit_5g = NUM_CHANNELS;
	enum channel_enum chan_enum;
	bool chan_in_range;

	reg_find_low_limit_chan_enum(chan_list, low_freq_2g, &low_limit_2g);
	reg_find_low_limit_chan_enum(chan_list, low_freq_5g, &low_limit_5g);
	reg_find_high_limit_chan_enum(chan_list, high_freq_2g, &high_limit_2g);
	reg_find_high_limit_chan_enum(chan_list, high_freq_5g, &high_limit_5g);

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		chan_in_range = false;
		if  ((low_limit_2g <= chan_enum) &&
		     (high_limit_2g >= chan_enum) &&
		     (low_limit_2g != NUM_CHANNELS) &&
		     (high_limit_2g != NUM_CHANNELS))
			chan_in_range = true;

		if  ((low_limit_5g <= chan_enum) &&
		     (high_limit_5g >= chan_enum) &&
		     (low_limit_5g != NUM_CHANNELS) &&
		     (high_limit_5g != NUM_CHANNELS))
			chan_in_range = true;

		if (!chan_in_range) {
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_DISABLED;
			chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
		}
	}
}

#ifdef CONFIG_BAND_6GHZ
/**
 * reg_propagate_6g_mas_channel_list() - Copy master chan list from PSOC to PDEV
 * @pdev_priv_obj: Pointer to pdev
 * @mas_chan_params: Master channel parameters
 *
 * Return: None
 */
static void reg_propagate_6g_mas_channel_list(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
		struct mas_chan_params *mas_chan_params)
{
	uint8_t i, j;
	struct regulatory_channel *src_6g_chan, *dst_6g_chan;
	uint32_t size_of_6g_chan_list =
		NUM_6GHZ_CHANNELS * sizeof(struct regulatory_channel);

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		qdf_mem_copy(pdev_priv_obj->mas_chan_list_6g_ap[i],
			     mas_chan_params->mas_chan_list_6g_ap[i],
			     size_of_6g_chan_list);

		for (j = 0; j < REG_MAX_CLIENT_TYPE; j++) {
			dst_6g_chan =
				pdev_priv_obj->mas_chan_list_6g_client[i][j];
			src_6g_chan =
				mas_chan_params->mas_chan_list_6g_client[i][j];
			qdf_mem_copy(dst_6g_chan, src_6g_chan,
				     size_of_6g_chan_list);
		}
	}

	pdev_priv_obj->reg_cur_6g_client_mobility_type =
				mas_chan_params->client_type;
	pdev_priv_obj->reg_rnr_tpe_usable = mas_chan_params->rnr_tpe_usable;
	pdev_priv_obj->reg_unspecified_ap_usable =
				mas_chan_params->unspecified_ap_usable;
	pdev_priv_obj->is_6g_channel_list_populated =
		mas_chan_params->is_6g_channel_list_populated;
	pdev_priv_obj->reg_6g_superid =
		mas_chan_params->reg_6g_superid;
}
#else
static inline void reg_propagate_6g_mas_channel_list(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
		struct mas_chan_params *mas_chan_params)
{
}
#endif

void reg_init_pdev_mas_chan_list(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
		struct mas_chan_params *mas_chan_params)
{
	qdf_mem_copy(pdev_priv_obj->mas_chan_list,
		     mas_chan_params->mas_chan_list,
		     NUM_CHANNELS * sizeof(struct regulatory_channel));

	reg_propagate_6g_mas_channel_list(pdev_priv_obj, mas_chan_params);

	pdev_priv_obj->dfs_region = mas_chan_params->dfs_region;

	pdev_priv_obj->phybitmap = mas_chan_params->phybitmap;

	pdev_priv_obj->reg_dmn_pair = mas_chan_params->reg_dmn_pair;
	pdev_priv_obj->ctry_code =  mas_chan_params->ctry_code;

	pdev_priv_obj->def_region_domain = mas_chan_params->reg_dmn_pair;
	pdev_priv_obj->def_country_code =  mas_chan_params->ctry_code;

	qdf_mem_copy(pdev_priv_obj->default_country,
		     mas_chan_params->default_country, REG_ALPHA2_LEN + 1);

	qdf_mem_copy(pdev_priv_obj->current_country,
		     mas_chan_params->current_country, REG_ALPHA2_LEN + 1);
}

/**
 * reg_modify_chan_list_for_cached_channels() - If num_cache_channels are
 * non-zero, then disable the pdev channels which is given in
 * cache_disable_chan_list.
 * @pdev_priv_obj: Pointer to regulatory pdev private object.
 */
#ifdef DISABLE_CHANNEL_LIST
static void reg_modify_chan_list_for_cached_channels(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	uint32_t i, j;
	uint32_t num_cache_channels = pdev_priv_obj->num_cache_channels;
	struct regulatory_channel *chan_list = pdev_priv_obj->cur_chan_list;
	struct regulatory_channel *cache_chan_list =
					pdev_priv_obj->cache_disable_chan_list;

	if (!num_cache_channels)
		return;

	if (pdev_priv_obj->disable_cached_channels) {
		for (i = 0; i < num_cache_channels; i++)
			for (j = 0; j < NUM_CHANNELS; j++)
				if (cache_chan_list[i].chan_num ==
							chan_list[j].chan_num) {
					chan_list[j].state =
							CHANNEL_STATE_DISABLE;
					chan_list[j].chan_flags |=
						REGULATORY_CHAN_DISABLED;
				}
	}
}
#else
static void reg_modify_chan_list_for_cached_channels(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}
#endif

#ifdef CONFIG_REG_CLIENT
/**
 * reg_modify_chan_list_for_srd_channels() - Modify SRD channels in ETSI13
 * @pdev: Pointer to pdev object
 * @chan_list: Current channel list
 *
 * This function converts SRD channels to passive in ETSI13 regulatory domain
 * when enable_srd_chan_in_master_mode is not set.
 */
static void
reg_modify_chan_list_for_srd_channels(struct wlan_objmgr_pdev *pdev,
				      struct regulatory_channel *chan_list)
{
	enum channel_enum chan_enum;

	if (!reg_is_etsi13_regdmn(pdev))
		return;

	if (reg_is_etsi13_srd_chan_allowed_master_mode(pdev))
		return;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		if (chan_list[chan_enum].chan_flags & REGULATORY_CHAN_DISABLED)
			continue;

		if (reg_is_etsi13_srd_chan(pdev,
					   chan_list[chan_enum].chan_num)) {
			chan_list[chan_enum].state =
				CHANNEL_STATE_DFS;
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_NO_IR;
		}
	}
}
#else
static inline void
reg_modify_chan_list_for_srd_channels(struct wlan_objmgr_pdev *pdev,
				      struct regulatory_channel *chan_list)
{
}
#endif

/**
 * reg_modify_chan_list_for_5dot9_ghz_channels() - Modify 5.9 GHz channels
 * in FCC
 * @pdev: Pointer to pdev object
 * @chan_list: Current channel list
 *
 * This function disables 5.9 GHz channels if service bit
 * wmi_service_5dot9_ghz_support is not set or the reg db is not offloaded
 * to FW. If service bit is set but ini enable_5dot9_ghz_chan_in_master_mode
 * is not set, it converts these channels to passive in FCC regulatory domain.
 * If both service bit and ini are set, the channels remain enabled.
 */
static void
reg_modify_chan_list_for_5dot9_ghz_channels(struct wlan_objmgr_pdev *pdev,
					    struct regulatory_channel
					    *chan_list)
{
	enum channel_enum chan_enum;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);

	if (!reg_is_fcc_regdmn(pdev))
		return;

	if (!reg_is_5dot9_ghz_supported(psoc) ||
	    !reg_is_regdb_offloaded(psoc)) {
		for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
			if (reg_is_5dot9_ghz_freq(pdev, chan_list[chan_enum].
						  center_freq)) {
				chan_list[chan_enum].state =
					CHANNEL_STATE_DISABLE;
				chan_list[chan_enum].chan_flags =
					REGULATORY_CHAN_DISABLED;
			}
		}
		return;
	}

	if (reg_is_5dot9_ghz_chan_allowed_master_mode(pdev))
		return;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		if (chan_list[chan_enum].chan_flags & REGULATORY_CHAN_DISABLED)
			continue;

		if (reg_is_5dot9_ghz_freq(pdev,
					  chan_list[chan_enum].center_freq)) {
			chan_list[chan_enum].state =
				CHANNEL_STATE_DFS;
			chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_NO_IR;
		}
	}
}

#if defined(CONFIG_BAND_6GHZ) && defined(CONFIG_REG_CLIENT)
/**
 * reg_modify_chan_list_for_6g_edge_channels() - Modify 6 GHz edge channels
 *
 * @pdev: Pointer to pdev object
 * @chan_list: Current channel list
 *
 * This function disables lower 6G edge channel (5935MHz) if service bit
 * wmi_service_lower_6g_edge_ch_supp is not set. If service bit is set
 * the channels remain enabled. It disables upper 6G edge channel (7115MHz)
 * if the service bit wmi_service_disable_upper_6g_edge_ch_supp is set, it
 * is enabled by default.
 *
 */
static void
reg_modify_chan_list_for_6g_edge_channels(struct wlan_objmgr_pdev *pdev,
					  struct regulatory_channel
					  *chan_list)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);

	if (!reg_is_lower_6g_edge_ch_supp(psoc)) {
		chan_list[CHAN_ENUM_5935].state = CHANNEL_STATE_DISABLE;
		chan_list[CHAN_ENUM_5935].chan_flags |=
						REGULATORY_CHAN_DISABLED;
	}

	if (reg_is_upper_6g_edge_ch_disabled(psoc)) {
		chan_list[CHAN_ENUM_7115].state = CHANNEL_STATE_DISABLE;
		chan_list[CHAN_ENUM_7115].chan_flags |=
						REGULATORY_CHAN_DISABLED;
	}
}
#else
static inline void
reg_modify_chan_list_for_6g_edge_channels(struct wlan_objmgr_pdev *pdev,
					  struct regulatory_channel
					  *chan_list)
{
}
#endif

#ifdef DISABLE_UNII_SHARED_BANDS
/**
 * reg_is_reg_unii_band_1_set() - Check UNII bitmap
 * @unii_bitmap: 5G UNII band bitmap
 *
 * This function checks the input bitmap to disable UNII-1 band channels.
 *
 * Return: Return true if UNII-1 channels need to be disabled,
 * else return false.
 */
static bool reg_is_reg_unii_band_1_set(uint8_t unii_bitmap)
{
	return !!(unii_bitmap & BIT(REG_UNII_BAND_1));
}

/**
 * reg_is_reg_unii_band_2a_set() - Check UNII bitmap
 * @unii_bitmap: 5G UNII band bitmap
 *
 * This function checks the input bitmap to disable UNII-2A band channels.
 *
 * Return: Return true if UNII-2A channels need to be disabled,
 * else return false.
 */
static bool reg_is_reg_unii_band_2a_set(uint8_t unii_bitmap)
{
	return !!(unii_bitmap & BIT(REG_UNII_BAND_2A));
}

/**
 * reg_is_5g_enum() - Check if channel enum is a 5G channel enum
 * @chan_enum: channel enum
 *
 * Return: Return true if the input channel enum is 5G, else return false.
 */
static bool reg_is_5g_enum(enum channel_enum chan_enum)
{
	return (chan_enum >= MIN_5GHZ_CHANNEL && chan_enum <= MAX_5GHZ_CHANNEL);
}

/**
 * reg_remove_unii_chan_from_chan_list() - Remove UNII band channels
 * @chan_list: Pointer to current channel list
 * @start_enum: starting enum value
 * @end_enum: ending enum value
 *
 * Remove channels in a unii band based in on the input start_enum and end_enum.
 * Disable the state and flags. Set disable_coex flag to true.
 *
 * return: void.
 */
static void
reg_remove_unii_chan_from_chan_list(struct regulatory_channel *chan_list,
				    enum channel_enum start_enum,
				    enum channel_enum end_enum)
{
	enum channel_enum chan_enum;

	if (!(reg_is_5g_enum(start_enum) && reg_is_5g_enum(end_enum))) {
		reg_err_rl("start_enum or end_enum is invalid");
		return;
	}

	for (chan_enum = start_enum; chan_enum <= end_enum; chan_enum++) {
		chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
		chan_list[chan_enum].chan_flags |= REGULATORY_CHAN_DISABLED;
	}
}

/**
 * reg_modify_disable_chan_list_for_unii1_and_unii2a() - Disable UNII-1 and
 * UNII2A band
 * @pdev_priv_obj: Pointer to pdev private object
 *
 * This function disables the UNII-1 and UNII-2A band channels
 * based on input unii_5g_bitmap.
 *
 * Return: void.
 */
static void
reg_modify_disable_chan_list_for_unii1_and_unii2a(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	uint8_t unii_bitmap = pdev_priv_obj->unii_5g_bitmap;
	struct regulatory_channel *chan_list = pdev_priv_obj->cur_chan_list;

	if (reg_is_reg_unii_band_1_set(unii_bitmap)) {
		reg_remove_unii_chan_from_chan_list(chan_list,
						    MIN_UNII_1_BAND_CHANNEL,
						    MAX_UNII_1_BAND_CHANNEL);
	}

	if (reg_is_reg_unii_band_2a_set(unii_bitmap)) {
		reg_remove_unii_chan_from_chan_list(chan_list,
						    MIN_UNII_2A_BAND_CHANNEL,
						    MAX_UNII_2A_BAND_CHANNEL);
	}
}
#else
static inline bool reg_is_reg_unii_band_1_set(uint8_t unii_bitmap)
{
	return false;
}

static inline bool reg_is_reg_unii_band_2a_set(uint8_t unii_bitmap)
{
	return false;
}

static inline bool reg_is_5g_enum(enum channel_enum chan_enum)
{
	return false;
}

static inline void
reg_remove_unii_chan_from_chan_list(struct regulatory_channel *chan_list,
				    enum channel_enum start_enum,
				    enum channel_enum end_enum)
{
}

static inline void
reg_modify_disable_chan_list_for_unii1_and_unii2a(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}
#endif

#ifdef CONFIG_BAND_6GHZ
#ifdef CONFIG_REG_CLIENT
static void
reg_append_mas_chan_list_for_6g(struct wlan_regulatory_pdev_priv_obj
				*pdev_priv_obj)
{
	struct regulatory_channel *master_chan_list_6g_client;

	if (pdev_priv_obj->reg_cur_6g_ap_pwr_type >= REG_CURRENT_MAX_AP_TYPE ||
	    pdev_priv_obj->reg_cur_6g_client_mobility_type >=
	    REG_MAX_CLIENT_TYPE) {
		reg_debug("invalid 6G AP or client power type");
		return;
	}

	master_chan_list_6g_client =
		pdev_priv_obj->mas_chan_list_6g_client[REG_INDOOR_AP]
			[pdev_priv_obj->reg_cur_6g_client_mobility_type];

	qdf_mem_copy(&pdev_priv_obj->mas_chan_list[MIN_6GHZ_CHANNEL],
		     master_chan_list_6g_client,
		     NUM_6GHZ_CHANNELS *
		     sizeof(struct regulatory_channel));
}

static void
reg_populate_secondary_cur_chan_list(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj)
{
	qdf_mem_copy(pdev_priv_obj->secondary_cur_chan_list,
		     pdev_priv_obj->mas_chan_list,
		     (NUM_CHANNELS - NUM_6GHZ_CHANNELS) *
		     sizeof(struct regulatory_channel));
	qdf_mem_copy(&pdev_priv_obj->
		     secondary_cur_chan_list[MIN_6GHZ_CHANNEL],
		     pdev_priv_obj->mas_chan_list_6g_ap
		     [pdev_priv_obj->reg_cur_6g_ap_pwr_type],
		     NUM_6GHZ_CHANNELS * sizeof(struct regulatory_channel));
}
#else /* CONFIG_REG_CLIENT */
static void
reg_append_mas_chan_list_for_6g(struct wlan_regulatory_pdev_priv_obj
				*pdev_priv_obj)
{
	enum reg_6g_ap_type ap_pwr_type = pdev_priv_obj->reg_cur_6g_ap_pwr_type;

	if (ap_pwr_type >= REG_CURRENT_MAX_AP_TYPE) {
		reg_debug("invalid 6G AP power type");
		return;
	}

	qdf_mem_copy(&pdev_priv_obj->mas_chan_list[MIN_6GHZ_CHANNEL],
		     pdev_priv_obj->mas_chan_list_6g_ap[ap_pwr_type],
		     NUM_6GHZ_CHANNELS * sizeof(struct regulatory_channel));
}

static inline void
reg_populate_secondary_cur_chan_list(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj)
{
}
#endif /* CONFIG_REG_CLIENT */

static void reg_copy_6g_cur_mas_chan_list_to_cmn(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	if (pdev_priv_obj->is_6g_channel_list_populated)
		reg_append_mas_chan_list_for_6g(pdev_priv_obj);
}
#else /* CONFIG_BAND_6GHZ */
static inline void
reg_copy_6g_cur_mas_chan_list_to_cmn(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}

static inline void
reg_append_mas_chan_list_for_6g(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}

#ifdef CONFIG_REG_CLIENT
static void
reg_populate_secondary_cur_chan_list(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj)
{
	qdf_mem_copy(pdev_priv_obj->secondary_cur_chan_list,
		     pdev_priv_obj->mas_chan_list,
		     NUM_CHANNELS * sizeof(struct regulatory_channel));
}
#else /* CONFIG_REG_CLIENT */
static inline void
reg_populate_secondary_cur_chan_list(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj)
{
}
#endif /* CONFIG_REG_CLIENT */
#endif /* CONFIG_BAND_6GHZ */

void reg_compute_pdev_current_chan_list(struct wlan_regulatory_pdev_priv_obj
					*pdev_priv_obj)
{
	reg_copy_6g_cur_mas_chan_list_to_cmn(pdev_priv_obj);

	qdf_mem_copy(pdev_priv_obj->cur_chan_list, pdev_priv_obj->mas_chan_list,
		     NUM_CHANNELS * sizeof(struct regulatory_channel));

	reg_populate_secondary_cur_chan_list(pdev_priv_obj);

	reg_modify_chan_list_for_freq_range(pdev_priv_obj->cur_chan_list,
					    pdev_priv_obj->range_2g_low,
					    pdev_priv_obj->range_2g_high,
					    pdev_priv_obj->range_5g_low,
					    pdev_priv_obj->range_5g_high);

	reg_modify_chan_list_for_band(pdev_priv_obj->cur_chan_list,
				      pdev_priv_obj->band_capability);

	reg_modify_disable_chan_list_for_unii1_and_unii2a(pdev_priv_obj);

	reg_modify_chan_list_for_dfs_channels(pdev_priv_obj->cur_chan_list,
					      pdev_priv_obj->dfs_enabled);

	reg_modify_chan_list_for_nol_list(pdev_priv_obj->cur_chan_list);

	reg_modify_chan_list_for_indoor_channels(pdev_priv_obj);

	reg_modify_chan_list_for_fcc_channel(pdev_priv_obj->cur_chan_list,
					     pdev_priv_obj->set_fcc_channel);

	reg_modify_chan_list_for_chan_144(pdev_priv_obj->cur_chan_list,
					  pdev_priv_obj->en_chan_144);

	reg_modify_chan_list_for_cached_channels(pdev_priv_obj);

	reg_modify_chan_list_for_srd_channels(pdev_priv_obj->pdev_ptr,
					      pdev_priv_obj->cur_chan_list);

	reg_modify_chan_list_for_5dot9_ghz_channels(pdev_priv_obj->pdev_ptr,
						    pdev_priv_obj->
						    cur_chan_list);

	reg_modify_chan_list_for_6g_edge_channels(pdev_priv_obj->pdev_ptr,
						  pdev_priv_obj->
						  cur_chan_list);
}

void reg_reset_reg_rules(struct reg_rule_info *reg_rules)
{
	qdf_mem_zero(reg_rules, sizeof(*reg_rules));
}

#ifdef CONFIG_REG_CLIENT
#ifdef CONFIG_BAND_6GHZ
/**
 * reg_copy_6g_reg_rules() - Copy the 6G reg rules from PSOC to PDEV
 * @pdev_reg_rules: Pointer to pdev reg rules
 * @psoc_reg_rules: Pointer to psoc reg rules
 *
 * Return: void
 */
static void reg_copy_6g_reg_rules(struct reg_rule_info *pdev_reg_rules,
				  struct reg_rule_info *psoc_reg_rules)
{
	uint32_t reg_rule_len_6g_ap, reg_rule_len_6g_client;
	uint8_t i;

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		pdev_reg_rules->num_of_6g_ap_reg_rules[i] =
			psoc_reg_rules->num_of_6g_ap_reg_rules[i];
		reg_rule_len_6g_ap = psoc_reg_rules->num_of_6g_ap_reg_rules[i] *
						sizeof(struct cur_reg_rule);
		qdf_mem_copy(pdev_reg_rules->reg_rules_6g_ap[i],
			     psoc_reg_rules->reg_rules_6g_ap[i],
			     reg_rule_len_6g_ap);

		pdev_reg_rules->num_of_6g_client_reg_rules[i] =
			psoc_reg_rules->num_of_6g_client_reg_rules[i];
		reg_rule_len_6g_client =
			psoc_reg_rules->num_of_6g_client_reg_rules[i] *
						sizeof(struct cur_reg_rule);
		qdf_mem_copy(pdev_reg_rules->reg_rules_6g_client[i],
			     psoc_reg_rules->reg_rules_6g_client[i],
			     reg_rule_len_6g_client);
	}
}

/**
 * reg_append_6g_reg_rules_in_pdev() - Append the 6G reg rules to the reg rules
 * list in pdev so that all currently used reg rules are in one common list
 * @pdev_priv_obj: Pointer to pdev private object
 *
 * Return: void
 */
static void reg_append_6g_reg_rules_in_pdev(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	struct reg_rule_info *pdev_reg_rules;
	enum reg_6g_ap_type cur_pwr_type;
	uint8_t num_reg_rules;

	pdev_reg_rules = &pdev_priv_obj->reg_rules;
	cur_pwr_type = pdev_priv_obj->reg_cur_6g_ap_pwr_type;

	num_reg_rules = pdev_reg_rules->num_of_reg_rules;
	pdev_reg_rules->num_of_reg_rules +=
		pdev_reg_rules->num_of_6g_client_reg_rules[cur_pwr_type];

	qdf_mem_copy(&pdev_reg_rules->reg_rules[num_reg_rules],
		     pdev_reg_rules->reg_rules_6g_client[cur_pwr_type],
		     pdev_reg_rules->num_of_6g_client_reg_rules[cur_pwr_type] *
		     sizeof(struct cur_reg_rule));
}

#else /* CONFIG_BAND_6GHZ */
static inline void reg_copy_6g_reg_rules(struct reg_rule_info *pdev_reg_rules,
					 struct reg_rule_info *psoc_reg_rules)
{
}

static inline void reg_append_6g_reg_rules_in_pdev(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}
#endif /* CONFIG_BAND_6GHZ */

void reg_save_reg_rules_to_pdev(
		struct reg_rule_info *psoc_reg_rules,
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	uint32_t reg_rule_len;
	struct reg_rule_info *pdev_reg_rules;

	qdf_spin_lock_bh(&pdev_priv_obj->reg_rules_lock);

	pdev_reg_rules = &pdev_priv_obj->reg_rules;
	reg_reset_reg_rules(pdev_reg_rules);

	pdev_reg_rules->num_of_reg_rules = psoc_reg_rules->num_of_reg_rules;
	if (!pdev_reg_rules->num_of_reg_rules) {
		qdf_spin_unlock_bh(&pdev_priv_obj->reg_rules_lock);
		reg_err("no reg rules in psoc");
		return;
	}

	reg_rule_len = pdev_reg_rules->num_of_reg_rules *
		       sizeof(struct cur_reg_rule);
	qdf_mem_copy(pdev_reg_rules->reg_rules, psoc_reg_rules->reg_rules,
		     reg_rule_len);

	reg_copy_6g_reg_rules(pdev_reg_rules, psoc_reg_rules);
	reg_append_6g_reg_rules_in_pdev(pdev_priv_obj);

	qdf_mem_copy(pdev_reg_rules->alpha2, pdev_priv_obj->current_country,
		     REG_ALPHA2_LEN + 1);
	pdev_reg_rules->dfs_region = pdev_priv_obj->dfs_region;

	qdf_spin_unlock_bh(&pdev_priv_obj->reg_rules_lock);
}
#endif /* CONFIG_REG_CLIENT */

void reg_propagate_mas_chan_list_to_pdev(struct wlan_objmgr_psoc *psoc,
					 void *object, void *arg)
{
	struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)object;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum direction *dir = arg;
	uint8_t pdev_id;
	uint8_t phy_id;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;
	struct reg_rule_info *psoc_reg_rules;

	psoc_priv_obj = (struct wlan_regulatory_psoc_priv_obj *)
		wlan_objmgr_psoc_get_comp_private_obj(
				psoc, WLAN_UMAC_COMP_REGULATORY);

	if (!psoc_priv_obj) {
		reg_err("psoc priv obj is NULL");
		return;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);
	if (reg_tx_ops->get_phy_id_from_pdev_id)
		reg_tx_ops->get_phy_id_from_pdev_id(psoc, pdev_id, &phy_id);
	else
		phy_id = pdev_id;

	reg_init_pdev_mas_chan_list(
			pdev_priv_obj,
			&psoc_priv_obj->mas_chan_params[phy_id]);
	psoc_reg_rules = &psoc_priv_obj->mas_chan_params[phy_id].reg_rules;
	reg_save_reg_rules_to_pdev(psoc_reg_rules, pdev_priv_obj);
	reg_modify_chan_list_for_japan(pdev);
	pdev_priv_obj->chan_list_recvd =
		psoc_priv_obj->chan_list_recvd[phy_id];
	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	if (reg_tx_ops->fill_umac_legacy_chanlist) {
		reg_tx_ops->fill_umac_legacy_chanlist(
				pdev, pdev_priv_obj->cur_chan_list);
	} else {
		if (*dir == NORTHBOUND)
			reg_send_scheduler_msg_nb(psoc, pdev);
		else
			reg_send_scheduler_msg_sb(psoc, pdev);
	}
}

/**
 * reg_populate_49g_band_channels() - For all the valid 4.9GHz regdb channels
 * in the master channel list, find the regulatory rules and call
 * reg_fill_channel_info() to populate master channel list with txpower,
 * antennagain, BW info, etc.
 * @reg_rule_5g: Pointer to regulatory rule.
 * @num_5g_reg_rules: Number of regulatory rules.
 * @min_bw_5g: Minimum regulatory bandwidth.
 * @mas_chan_list: Pointer to the master channel list.
 */
#ifdef CONFIG_49GHZ_CHAN
static void
reg_populate_49g_band_channels(struct cur_reg_rule *reg_rule_5g,
			       uint32_t num_5g_reg_rules,
			       uint16_t min_bw_5g,
			       struct regulatory_channel *mas_chan_list)
{
	reg_populate_band_channels(MIN_49GHZ_CHANNEL,
				   MAX_49GHZ_CHANNEL,
				   reg_rule_5g,
				   num_5g_reg_rules,
				   min_bw_5g,
				   mas_chan_list);
}
#else
static void
reg_populate_49g_band_channels(struct cur_reg_rule *reg_rule_5g,
			       uint32_t num_5g_reg_rules,
			       uint16_t min_bw_5g,
			       struct regulatory_channel *mas_chan_list)
{
}
#endif /* CONFIG_49GHZ_CHAN */

/**
 * reg_populate_6g_band_channels() - For all the valid 6GHz regdb channels
 * in the master channel list, find the regulatory rules and call
 * reg_fill_channel_info() to populate master channel list with txpower,
 * antennagain, BW info, etc.
 * @reg_rule_5g: Pointer to regulatory rule.
 * @num_5g_reg_rules: Number of regulatory rules.
 * @min_bw_5g: Minimum regulatory bandwidth.
 * @mas_chan_list: Pointer to the master channel list.
 */
#ifdef CONFIG_BAND_6GHZ
static void
reg_populate_6g_band_channels(struct cur_reg_rule *reg_rule_5g,
			      uint32_t num_5g_reg_rules,
			      uint16_t min_bw_5g,
			      struct regulatory_channel *mas_chan_list)
{
	reg_populate_band_channels(MIN_6GHZ_CHANNEL,
				   MAX_6GHZ_CHANNEL,
				   reg_rule_5g,
				   num_5g_reg_rules,
				   min_bw_5g,
				   mas_chan_list);
}
#else
static void
reg_populate_6g_band_channels(struct cur_reg_rule *reg_rule_5g,
			      uint32_t num_5g_reg_rules,
			      uint16_t min_bw_5g,
			      struct regulatory_channel *mas_chan_list)
{
}
#endif /* CONFIG_BAND_6GHZ */

#ifdef CONFIG_REG_CLIENT
/**
 * reg_send_ctl_info() - Send CTL info to firmware when regdb is not offloaded
 * @soc_reg: soc private object for regulatory
 * @regulatory_info: regulatory info
 * @tx_ops: send operations for regulatory component
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
reg_send_ctl_info(struct wlan_regulatory_psoc_priv_obj *soc_reg,
		  struct cur_regulatory_info *regulatory_info,
		  struct wlan_lmac_if_reg_tx_ops *tx_ops)
{
	struct wlan_objmgr_psoc *psoc = regulatory_info->psoc;
	struct reg_ctl_params params = {0};
	QDF_STATUS status;
	uint16_t regd_index;
	uint32_t index_2g, index_5g;

	if (soc_reg->offload_enabled)
		return QDF_STATUS_SUCCESS;

	if (!tx_ops || !tx_ops->send_ctl_info) {
		reg_err("No regulatory tx_ops");
		return QDF_STATUS_E_FAULT;
	}

	status = reg_get_rdpair_from_regdmn_id(regulatory_info->reg_dmn_pair,
					       &regd_index);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_err("Failed to get regdomain index for regdomain pair: %x",
			regulatory_info->reg_dmn_pair);
		return status;
	}

	index_2g = g_reg_dmn_pairs[regd_index].dmn_id_2g;
	index_5g = g_reg_dmn_pairs[regd_index].dmn_id_5g;
	params.ctl_2g = regdomains_2g[index_2g].ctl_val;
	params.ctl_5g = regdomains_5g[index_5g].ctl_val;
	params.regd_2g = reg_2g_sub_dmn_code[index_2g];
	params.regd_5g = reg_5g_sub_dmn_code[index_5g];

	if (reg_is_world_ctry_code(regulatory_info->reg_dmn_pair))
		params.regd = regulatory_info->reg_dmn_pair;
	else
		params.regd = regulatory_info->ctry_code | COUNTRY_ERD_FLAG;

	reg_debug("regdomain pair = %u, regdomain index = %u",
		  regulatory_info->reg_dmn_pair, regd_index);
	reg_debug("index_2g = %u, index_5g = %u, ctl_2g = %x, ctl_5g = %x",
		  index_2g, index_5g, params.ctl_2g, params.ctl_5g);
	reg_debug("regd_2g = %x, regd_5g = %x, regd = %x",
		  params.regd_2g, params.regd_5g, params.regd);

	status = tx_ops->send_ctl_info(psoc, &params);
	if (QDF_IS_STATUS_ERROR(status))
		reg_err("Failed to send CTL info to firmware");

	return status;
}
#else
static QDF_STATUS
reg_send_ctl_info(struct wlan_regulatory_psoc_priv_obj *soc_reg,
		  struct cur_regulatory_info *regulatory_info,
		  struct wlan_lmac_if_reg_tx_ops *tx_ops)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * reg_soc_vars_reset_on_failure() - Reset the PSOC private object variables
 *	when there is a failure
 * @status_code: status code of CC setting event
 * @soc_reg: soc private object for regulatory
 * @tx_ops: send operations for regulatory component
 * @psoc: pointer to PSOC object
 * @dbg_id: object manager reference debug ID
 * @phy_id: physical ID
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
reg_soc_vars_reset_on_failure(enum cc_setting_code status_code,
			      struct wlan_regulatory_psoc_priv_obj *soc_reg,
			      struct wlan_lmac_if_reg_tx_ops *tx_ops,
			      struct wlan_objmgr_psoc *psoc,
			      wlan_objmgr_ref_dbgid dbg_id,
			      uint8_t phy_id)
{
	struct wlan_objmgr_pdev *pdev;

	if (status_code != REG_SET_CC_STATUS_PASS) {
		reg_err("Set country code failed, status code %d",
			status_code);

		pdev = wlan_objmgr_get_pdev_by_id(psoc, phy_id, dbg_id);
		if (!pdev) {
			reg_err("pdev is NULL");
			return QDF_STATUS_E_FAILURE;
		}

		if (tx_ops->set_country_failed)
			tx_ops->set_country_failed(pdev);

		wlan_objmgr_pdev_release_ref(pdev, dbg_id);

		if (status_code != REG_CURRENT_ALPHA2_NOT_FOUND)
			return QDF_STATUS_E_FAILURE;

		soc_reg->new_user_ctry_pending[phy_id] = false;
		soc_reg->new_11d_ctry_pending[phy_id] = false;
		soc_reg->world_country_pending[phy_id] = true;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * reg_init_chan() - Initialize the channel list from the channel_map global
 *	list
 * @dst_list: list to initialize
 * @beg_enum: starting point in list(inclusive)
 * @end_enum: ending point in list(inclusive)
 * @dst_idx_adj: offset between channel_map and dst_list
 * @soc_reg: soc private object for regulatory
 *
 * Return: none
 */
static void reg_init_chan(struct regulatory_channel *dst_list,
			  enum channel_enum beg_enum,
			  enum channel_enum end_enum, uint8_t dst_idx_adj,
			  struct wlan_regulatory_psoc_priv_obj *soc_reg)
{
	enum channel_enum chan_enum;
	uint8_t dst_idx;

	for (chan_enum = beg_enum; chan_enum <= end_enum; chan_enum++) {
		dst_idx = chan_enum - dst_idx_adj;

		dst_list[dst_idx].chan_num = channel_map[chan_enum].chan_num;
		dst_list[dst_idx].center_freq =
					channel_map[chan_enum].center_freq;
		dst_list[dst_idx].chan_flags = REGULATORY_CHAN_DISABLED;
		dst_list[dst_idx].state = CHANNEL_STATE_DISABLE;
		if (!soc_reg->retain_nol_across_regdmn_update)
			dst_list[dst_idx].nol_chan = false;
	}
}

static void reg_init_legacy_master_chan(struct regulatory_channel *dst_list,
				struct wlan_regulatory_psoc_priv_obj *soc_reg)
{
	reg_init_chan(dst_list, 0, NUM_CHANNELS - 1, 0, soc_reg);
}

#ifdef CONFIG_BAND_6GHZ
static void reg_init_2g_5g_master_chan(struct regulatory_channel *dst_list,
				struct wlan_regulatory_psoc_priv_obj *soc_reg)
{
	reg_init_chan(dst_list, 0, MAX_5GHZ_CHANNEL, 0, soc_reg);
}

static void reg_init_6g_master_chan(struct regulatory_channel *dst_list,
				struct wlan_regulatory_psoc_priv_obj *soc_reg)
{
	reg_init_chan(dst_list, MIN_6GHZ_CHANNEL, MAX_6GHZ_CHANNEL,
		      MIN_6GHZ_CHANNEL, soc_reg);
}

/**
 * reg_store_regulatory_ext_info_to_socpriv() - Copy ext info from regulatory
 *	to regulatory PSOC private obj
 * @soc_reg: soc private object for regulatory
 * @regulat_info: regulatory info from CC event
 * @phy_id: physical ID
 *
 * Return: none
 */
static void reg_store_regulatory_ext_info_to_socpriv(
				struct wlan_regulatory_psoc_priv_obj *soc_reg,
				struct cur_regulatory_info *regulat_info,
				uint8_t phy_id)
{
	uint32_t i;

	soc_reg->num_phy = regulat_info->num_phy;
	soc_reg->mas_chan_params[phy_id].phybitmap = regulat_info->phybitmap;
	soc_reg->mas_chan_params[phy_id].dfs_region = regulat_info->dfs_region;
	soc_reg->mas_chan_params[phy_id].ctry_code = regulat_info->ctry_code;
	soc_reg->mas_chan_params[phy_id].reg_dmn_pair =
		regulat_info->reg_dmn_pair;
	soc_reg->mas_chan_params[phy_id].reg_6g_superid =
		regulat_info->domain_code_6g_super_id;
	qdf_mem_copy(soc_reg->mas_chan_params[phy_id].current_country,
		     regulat_info->alpha2,
		     REG_ALPHA2_LEN + 1);
	qdf_mem_copy(soc_reg->cur_country,
		     regulat_info->alpha2,
		     REG_ALPHA2_LEN + 1);
	reg_debug("set cur_country %.2s", soc_reg->cur_country);

	soc_reg->mas_chan_params[phy_id].ap_pwr_type = REG_INDOOR_AP;
	soc_reg->mas_chan_params[phy_id].client_type =
					regulat_info->client_type;
	soc_reg->mas_chan_params[phy_id].rnr_tpe_usable =
					regulat_info->rnr_tpe_usable;
	soc_reg->mas_chan_params[phy_id].unspecified_ap_usable =
					regulat_info->unspecified_ap_usable;

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		soc_reg->domain_code_6g_ap[i] =
			regulat_info->domain_code_6g_ap[i];

		if (soc_reg->domain_code_6g_ap[i])
			soc_reg->mas_chan_params[phy_id].
				is_6g_channel_list_populated = true;

		qdf_mem_copy(soc_reg->domain_code_6g_client[i],
			     regulat_info->domain_code_6g_client[i],
			     REG_MAX_CLIENT_TYPE * sizeof(uint8_t));
	}
}

/**
 * reg_fill_master_channels() - Fill the master channel lists based on the
 *	regulatory rules
 * @regulat_info: regulatory info
 * @reg_rules: regulatory rules
 * @client_mobility_type: client mobility type
 * @mas_chan_list_2g_5g: master chan list to fill with 2GHz and 5GHz channels
 * @mas_chan_list_6g_ap: master AP chan list to fill with 6GHz channels
 * @mas_chan_list_6g_client: master client chan list to fill with 6GHz channels
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
reg_fill_master_channels(struct cur_regulatory_info *regulat_info,
			 struct reg_rule_info *reg_rules,
			 enum reg_6g_client_type client_mobility_type,
			 struct regulatory_channel *mas_chan_list_2g_5g,
	struct regulatory_channel *mas_chan_list_6g_ap[REG_CURRENT_MAX_AP_TYPE],
	struct regulatory_channel *mas_chan_list_6g_client
		[REG_CURRENT_MAX_AP_TYPE][REG_MAX_CLIENT_TYPE])
{
	uint32_t i, j, k, curr_reg_rule_location;
	uint32_t num_2g_reg_rules, num_5g_reg_rules;
	uint32_t num_6g_reg_rules_ap[REG_CURRENT_MAX_AP_TYPE];
	uint32_t *num_6g_reg_rules_client[REG_CURRENT_MAX_AP_TYPE];
	struct cur_reg_rule *reg_rule_2g, *reg_rule_5g,
		*reg_rule_6g_ap[REG_CURRENT_MAX_AP_TYPE],
		**reg_rule_6g_client[REG_CURRENT_MAX_AP_TYPE];
	uint32_t min_bw_2g, max_bw_2g, min_bw_5g, max_bw_5g,
		min_bw_6g_ap[REG_CURRENT_MAX_AP_TYPE],
		max_bw_6g_ap[REG_CURRENT_MAX_AP_TYPE],
		*min_bw_6g_client[REG_CURRENT_MAX_AP_TYPE],
		*max_bw_6g_client[REG_CURRENT_MAX_AP_TYPE];

	min_bw_2g = regulat_info->min_bw_2g;
	max_bw_2g = regulat_info->max_bw_2g;
	reg_rule_2g = regulat_info->reg_rules_2g_ptr;
	num_2g_reg_rules = regulat_info->num_2g_reg_rules;
	reg_update_max_bw_per_rule(num_2g_reg_rules, reg_rule_2g, max_bw_2g);

	min_bw_5g = regulat_info->min_bw_5g;
	max_bw_5g = regulat_info->max_bw_5g;
	reg_rule_5g = regulat_info->reg_rules_5g_ptr;
	num_5g_reg_rules = regulat_info->num_5g_reg_rules;
	reg_update_max_bw_per_rule(num_5g_reg_rules, reg_rule_5g, max_bw_5g);

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		min_bw_6g_ap[i] = regulat_info->min_bw_6g_ap[i];
		max_bw_6g_ap[i] = regulat_info->max_bw_6g_ap[i];
		reg_rule_6g_ap[i] = regulat_info->reg_rules_6g_ap_ptr[i];
		num_6g_reg_rules_ap[i] = regulat_info->num_6g_reg_rules_ap[i];
		reg_update_max_bw_per_rule(num_6g_reg_rules_ap[i],
					   reg_rule_6g_ap[i], max_bw_6g_ap[i]);
	}

	for (j = 0; j < REG_CURRENT_MAX_AP_TYPE; j++) {
		min_bw_6g_client[j] = regulat_info->min_bw_6g_client[j];
		max_bw_6g_client[j] = regulat_info->max_bw_6g_client[j];
		reg_rule_6g_client[j] =
			regulat_info->reg_rules_6g_client_ptr[j];
		num_6g_reg_rules_client[j] =
			regulat_info->num_6g_reg_rules_client[j];
		for (k = 0; k < REG_MAX_CLIENT_TYPE; k++) {
			reg_update_max_bw_per_rule(
						num_6g_reg_rules_client[j][k],
						reg_rule_6g_client[j][k],
						max_bw_6g_client[j][k]);
		}
	}

	reg_reset_reg_rules(reg_rules);

	reg_rules->num_of_reg_rules = num_5g_reg_rules + num_2g_reg_rules;

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		reg_rules->num_of_6g_ap_reg_rules[i] = num_6g_reg_rules_ap[i];
		if (num_6g_reg_rules_ap[i] > MAX_6G_REG_RULES) {
			reg_err("number of reg rules for 6g ap exceeds limit");
			return QDF_STATUS_E_FAILURE;
		}

		reg_rules->num_of_6g_client_reg_rules[i] =
			num_6g_reg_rules_client[i][client_mobility_type];
		for (j = 0; j < REG_MAX_CLIENT_TYPE; j++) {
			if (num_6g_reg_rules_client[i][j] > MAX_6G_REG_RULES) {
				reg_err("number of reg rules for 6g client exceeds limit");
				return QDF_STATUS_E_FAILURE;
			}
		}
	}

	if (reg_rules->num_of_reg_rules > MAX_REG_RULES) {
		reg_err("number of reg rules exceeds limit");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rules->num_of_reg_rules) {
		if (num_2g_reg_rules)
			qdf_mem_copy(reg_rules->reg_rules,
				     reg_rule_2g, num_2g_reg_rules *
				     sizeof(struct cur_reg_rule));
		curr_reg_rule_location = num_2g_reg_rules;
		if (num_5g_reg_rules)
			qdf_mem_copy(reg_rules->reg_rules +
				     curr_reg_rule_location, reg_rule_5g,
				     num_5g_reg_rules *
				     sizeof(struct cur_reg_rule));
	}

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		if (num_6g_reg_rules_ap[i])
			qdf_mem_copy(reg_rules->reg_rules_6g_ap[i],
				     reg_rule_6g_ap[i],
				     num_6g_reg_rules_ap[i] *
				     sizeof(struct cur_reg_rule));

		if (num_6g_reg_rules_client[i][client_mobility_type])
			qdf_mem_copy(reg_rules->reg_rules_6g_client[i],
				reg_rule_6g_client[i][client_mobility_type],
				num_6g_reg_rules_client[i]
				[client_mobility_type] *
				sizeof(struct cur_reg_rule));
	}


	if (num_5g_reg_rules)
		reg_do_auto_bw_correction(num_5g_reg_rules,
					  reg_rule_5g, max_bw_5g);

	if (num_2g_reg_rules)
		reg_populate_band_channels(MIN_24GHZ_CHANNEL, MAX_24GHZ_CHANNEL,
					   reg_rule_2g, num_2g_reg_rules,
					   min_bw_2g, mas_chan_list_2g_5g);

	if (num_5g_reg_rules) {
		reg_populate_band_channels(MIN_5GHZ_CHANNEL, MAX_5GHZ_CHANNEL,
					   reg_rule_5g, num_5g_reg_rules,
					   min_bw_5g, mas_chan_list_2g_5g);
		reg_populate_49g_band_channels(reg_rule_5g,
					       num_5g_reg_rules,
					       min_bw_5g,
					       mas_chan_list_2g_5g);
	}

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		if (num_6g_reg_rules_ap[i])
			reg_populate_band_channels_ext_for_6g(0,
							NUM_6GHZ_CHANNELS - 1,
							reg_rule_6g_ap[i],
							num_6g_reg_rules_ap[i],
							min_bw_6g_ap[i],
							mas_chan_list_6g_ap[i]);

		for (j = 0; j < REG_MAX_CLIENT_TYPE; j++) {
			if (num_6g_reg_rules_client[i][j])
				reg_populate_band_channels_ext_for_6g(0,
						NUM_6GHZ_CHANNELS - 1,
						reg_rule_6g_client[i][j],
						num_6g_reg_rules_client[i][j],
						min_bw_6g_client[i][j],
						mas_chan_list_6g_client[i][j]);
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * reg_set_socpriv_vars() - Set the regulatory PSOC variables based on
 *	pending country status
 * @soc_reg: regulatory PSOC private object
 * @regulat_info: regulatory info
 * @psoc: pointer to PSOC object
 * @phy_id: physical ID
 *
 * Return: none
 */
static void reg_set_socpriv_vars(struct wlan_regulatory_psoc_priv_obj *soc_reg,
				 struct cur_regulatory_info *regulat_info,
				 struct wlan_objmgr_psoc *psoc,
				 uint8_t phy_id)
{
	soc_reg->chan_list_recvd[phy_id] = true;

	if (soc_reg->new_user_ctry_pending[phy_id]) {
		soc_reg->new_user_ctry_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_USERSPACE;
		soc_reg->user_ctry_set = true;
		reg_debug("new user country is set");
		reg_run_11d_state_machine(psoc);
	} else if (soc_reg->new_init_ctry_pending[phy_id]) {
		soc_reg->new_init_ctry_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_USERSPACE;
		reg_debug("new init country is set");
	} else if (soc_reg->new_11d_ctry_pending[phy_id]) {
		soc_reg->new_11d_ctry_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_11D;
		soc_reg->user_ctry_set = false;
		reg_run_11d_state_machine(psoc);
	} else if (soc_reg->world_country_pending[phy_id]) {
		soc_reg->world_country_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_CORE;
		soc_reg->user_ctry_set = false;
		reg_run_11d_state_machine(psoc);
	} else {
		if (soc_reg->cc_src == SOURCE_UNKNOWN &&
		    soc_reg->num_phy == phy_id + 1)
			soc_reg->cc_src = SOURCE_DRIVER;

		qdf_mem_copy(soc_reg->mas_chan_params[phy_id].default_country,
			     regulat_info->alpha2,
			     REG_ALPHA2_LEN + 1);

		soc_reg->mas_chan_params[phy_id].def_country_code =
			regulat_info->ctry_code;
		soc_reg->mas_chan_params[phy_id].def_region_domain =
			regulat_info->reg_dmn_pair;

		if (soc_reg->cc_src == SOURCE_DRIVER) {
			qdf_mem_copy(soc_reg->def_country,
				     regulat_info->alpha2,
				     REG_ALPHA2_LEN + 1);

			soc_reg->def_country_code = regulat_info->ctry_code;
			soc_reg->def_region_domain =
				regulat_info->reg_dmn_pair;

			if (reg_is_world_alpha2(regulat_info->alpha2)) {
				soc_reg->cc_src = SOURCE_CORE;
				reg_run_11d_state_machine(psoc);
			}
		}
	}
}

QDF_STATUS reg_process_master_chan_list_ext(
		struct cur_regulatory_info *regulat_info)
{
	struct wlan_regulatory_psoc_priv_obj *soc_reg;
	uint32_t i, j;
	struct regulatory_channel *mas_chan_list_2g_5g,
		*mas_chan_list_6g_ap[REG_CURRENT_MAX_AP_TYPE],
		*mas_chan_list_6g_client[REG_CURRENT_MAX_AP_TYPE]
							[REG_MAX_CLIENT_TYPE];
	struct wlan_objmgr_psoc *psoc;
	wlan_objmgr_ref_dbgid dbg_id;
	enum direction dir;
	uint8_t phy_id;
	uint8_t pdev_id;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	QDF_STATUS status;
	struct mas_chan_params *this_mchan_params;

	psoc = regulat_info->psoc;
	soc_reg = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(soc_reg)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	tx_ops = reg_get_psoc_tx_ops(psoc);
	phy_id = regulat_info->phy_id;

	if (tx_ops->get_pdev_id_from_phy_id)
		tx_ops->get_pdev_id_from_phy_id(psoc, phy_id, &pdev_id);
	else
		pdev_id = phy_id;

	if (reg_ignore_default_country(soc_reg, regulat_info)) {
		status = reg_set_curr_country(soc_reg, regulat_info, tx_ops);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			reg_debug("WLAN restart - Ignore default CC for phy_id: %u",
				  phy_id);
			return QDF_STATUS_SUCCESS;
		}
	}

	reg_debug("process reg master chan extended list");

	if (soc_reg->offload_enabled) {
		dbg_id = WLAN_REGULATORY_NB_ID;
		dir = NORTHBOUND;
	} else {
		dbg_id = WLAN_REGULATORY_SB_ID;
		dir = SOUTHBOUND;
	}

	status = reg_soc_vars_reset_on_failure(regulat_info->status_code,
					       soc_reg, tx_ops, psoc, dbg_id,
					       phy_id);

	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	this_mchan_params = &soc_reg->mas_chan_params[phy_id];
	mas_chan_list_2g_5g = this_mchan_params->mas_chan_list;

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		mas_chan_list_6g_ap[i] =
			this_mchan_params->mas_chan_list_6g_ap[i];

		for (j = 0; j < REG_MAX_CLIENT_TYPE; j++)
			mas_chan_list_6g_client[i][j] =
				this_mchan_params->mas_chan_list_6g_client[i][j];
	}

	reg_init_channel_map(regulat_info->dfs_region);

	reg_init_2g_5g_master_chan(mas_chan_list_2g_5g, soc_reg);

	for (i = 0; i < REG_CURRENT_MAX_AP_TYPE; i++) {
		reg_init_6g_master_chan(mas_chan_list_6g_ap[i], soc_reg);
		for (j = 0; j < REG_MAX_CLIENT_TYPE; j++)
			reg_init_6g_master_chan(mas_chan_list_6g_client[i][j],
						soc_reg);
	}

	reg_store_regulatory_ext_info_to_socpriv(soc_reg, regulat_info, phy_id);

	status = reg_fill_master_channels(regulat_info,
					  &this_mchan_params->reg_rules,
					  this_mchan_params->client_type,
					  mas_chan_list_2g_5g,
					  mas_chan_list_6g_ap,
					  mas_chan_list_6g_client);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = reg_send_ctl_info(soc_reg, regulat_info, tx_ops);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	reg_set_socpriv_vars(soc_reg, regulat_info, psoc, phy_id);

	pdev = wlan_objmgr_get_pdev_by_id(psoc, pdev_id, dbg_id);
	if (pdev) {
		reg_propagate_mas_chan_list_to_pdev(psoc, pdev, &dir);
		wlan_objmgr_pdev_release_ref(pdev, dbg_id);
	}

	return QDF_STATUS_SUCCESS;
}
#endif /* CONFIG_BAND_6GHZ */

QDF_STATUS reg_process_master_chan_list(
		struct cur_regulatory_info *regulat_info)
{
	struct wlan_regulatory_psoc_priv_obj *soc_reg;
	uint32_t num_2g_reg_rules, num_5g_reg_rules;
	struct cur_reg_rule *reg_rule_2g, *reg_rule_5g;
	uint16_t min_bw_2g, max_bw_2g, min_bw_5g, max_bw_5g;
	struct regulatory_channel *mas_chan_list;
	struct wlan_objmgr_psoc *psoc;
	wlan_objmgr_ref_dbgid dbg_id;
	enum direction dir;
	uint8_t phy_id;
	uint8_t pdev_id;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	struct reg_rule_info *reg_rules;
	QDF_STATUS status;

	psoc = regulat_info->psoc;
	soc_reg = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(soc_reg)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	tx_ops = reg_get_psoc_tx_ops(psoc);
	phy_id = regulat_info->phy_id;

	if (tx_ops->get_pdev_id_from_phy_id)
		tx_ops->get_pdev_id_from_phy_id(psoc, phy_id, &pdev_id);
	else
		pdev_id = phy_id;

	if (reg_ignore_default_country(soc_reg, regulat_info)) {
		status = reg_set_curr_country(soc_reg, regulat_info, tx_ops);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			reg_debug("WLAN restart - Ignore default CC for phy_id: %u",
				  phy_id);
			return QDF_STATUS_SUCCESS;
		}
	}

	reg_debug("process reg master chan list");

	if (soc_reg->offload_enabled) {
		dbg_id = WLAN_REGULATORY_NB_ID;
		dir = NORTHBOUND;
	} else {
		dbg_id = WLAN_REGULATORY_SB_ID;
		dir = SOUTHBOUND;
	}

	status = reg_soc_vars_reset_on_failure(regulat_info->status_code,
					       soc_reg, tx_ops, psoc, dbg_id,
					       phy_id);

	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	mas_chan_list = soc_reg->mas_chan_params[phy_id].mas_chan_list;

	reg_init_channel_map(regulat_info->dfs_region);

	reg_init_legacy_master_chan(mas_chan_list, soc_reg);

	soc_reg->num_phy = regulat_info->num_phy;
	soc_reg->mas_chan_params[phy_id].phybitmap =
		regulat_info->phybitmap;
	soc_reg->mas_chan_params[phy_id].dfs_region =
		regulat_info->dfs_region;
	soc_reg->mas_chan_params[phy_id].ctry_code =
		regulat_info->ctry_code;
	soc_reg->mas_chan_params[phy_id].reg_dmn_pair =
		regulat_info->reg_dmn_pair;
	qdf_mem_copy(soc_reg->mas_chan_params[phy_id].current_country,
		     regulat_info->alpha2,
		     REG_ALPHA2_LEN + 1);
	qdf_mem_copy(soc_reg->cur_country,
		     regulat_info->alpha2,
		     REG_ALPHA2_LEN + 1);
	reg_debug("set cur_country %.2s", soc_reg->cur_country);

	min_bw_2g = regulat_info->min_bw_2g;
	max_bw_2g = regulat_info->max_bw_2g;
	reg_rule_2g = regulat_info->reg_rules_2g_ptr;
	num_2g_reg_rules = regulat_info->num_2g_reg_rules;
	reg_update_max_bw_per_rule(num_2g_reg_rules,
				   reg_rule_2g, max_bw_2g);

	min_bw_5g = regulat_info->min_bw_5g;
	max_bw_5g = regulat_info->max_bw_5g;
	reg_rule_5g = regulat_info->reg_rules_5g_ptr;
	num_5g_reg_rules = regulat_info->num_5g_reg_rules;
	reg_update_max_bw_per_rule(num_5g_reg_rules,
				   reg_rule_5g, max_bw_5g);

	reg_rules = &soc_reg->mas_chan_params[phy_id].reg_rules;
	reg_reset_reg_rules(reg_rules);

	reg_rules->num_of_reg_rules = num_5g_reg_rules + num_2g_reg_rules;
	if (reg_rules->num_of_reg_rules > MAX_REG_RULES) {
		reg_err("number of reg rules exceeds limit");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_rules->num_of_reg_rules) {
		if (num_2g_reg_rules)
			qdf_mem_copy(reg_rules->reg_rules,
				     reg_rule_2g, num_2g_reg_rules *
				     sizeof(struct cur_reg_rule));
		if (num_5g_reg_rules)
			qdf_mem_copy(reg_rules->reg_rules +
				     num_2g_reg_rules, reg_rule_5g,
				     num_5g_reg_rules *
				     sizeof(struct cur_reg_rule));
	}

	if (num_5g_reg_rules != 0)
		reg_do_auto_bw_correction(num_5g_reg_rules,
					  reg_rule_5g, max_bw_5g);

	if (num_2g_reg_rules != 0)
		reg_populate_band_channels(MIN_24GHZ_CHANNEL, MAX_24GHZ_CHANNEL,
					   reg_rule_2g, num_2g_reg_rules,
					   min_bw_2g, mas_chan_list);

	if (num_5g_reg_rules != 0) {
		reg_populate_band_channels(MIN_5GHZ_CHANNEL, MAX_5GHZ_CHANNEL,
					   reg_rule_5g, num_5g_reg_rules,
					   min_bw_5g, mas_chan_list);
		reg_populate_49g_band_channels(reg_rule_5g,
					       num_5g_reg_rules,
					       min_bw_5g,
					       mas_chan_list);
		reg_populate_6g_band_channels(reg_rule_5g,
					      num_5g_reg_rules,
					      min_bw_5g,
					      mas_chan_list);
	}

	soc_reg->chan_list_recvd[phy_id] = true;
	status = reg_send_ctl_info(soc_reg, regulat_info, tx_ops);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	if (soc_reg->new_user_ctry_pending[phy_id]) {
		soc_reg->new_user_ctry_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_USERSPACE;
		soc_reg->user_ctry_set = true;
		reg_debug("new user country is set");
		reg_run_11d_state_machine(psoc);
	} else if (soc_reg->new_init_ctry_pending[phy_id]) {
		soc_reg->new_init_ctry_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_USERSPACE;
		reg_debug("new init country is set");
	} else if (soc_reg->new_11d_ctry_pending[phy_id]) {
		soc_reg->new_11d_ctry_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_11D;
		soc_reg->user_ctry_set = false;
		reg_run_11d_state_machine(psoc);
	} else if (soc_reg->world_country_pending[phy_id]) {
		soc_reg->world_country_pending[phy_id] = false;
		soc_reg->cc_src = SOURCE_CORE;
		soc_reg->user_ctry_set = false;
		reg_run_11d_state_machine(psoc);
	} else {
		if (soc_reg->cc_src == SOURCE_UNKNOWN &&
		    soc_reg->num_phy == phy_id + 1)
			soc_reg->cc_src = SOURCE_DRIVER;

		qdf_mem_copy(soc_reg->mas_chan_params[phy_id].default_country,
			     regulat_info->alpha2,
			     REG_ALPHA2_LEN + 1);

		soc_reg->mas_chan_params[phy_id].def_country_code =
			regulat_info->ctry_code;
		soc_reg->mas_chan_params[phy_id].def_region_domain =
			regulat_info->reg_dmn_pair;

		if (soc_reg->cc_src == SOURCE_DRIVER) {
			qdf_mem_copy(soc_reg->def_country,
				     regulat_info->alpha2,
				     REG_ALPHA2_LEN + 1);

			soc_reg->def_country_code = regulat_info->ctry_code;
			soc_reg->def_region_domain =
				regulat_info->reg_dmn_pair;

			if (reg_is_world_alpha2(regulat_info->alpha2)) {
				soc_reg->cc_src = SOURCE_CORE;
				reg_run_11d_state_machine(psoc);
			}
		}
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, pdev_id, dbg_id);
	if (pdev) {
		reg_propagate_mas_chan_list_to_pdev(psoc, pdev, &dir);
		wlan_objmgr_pdev_release_ref(pdev, dbg_id);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
				     struct regulatory_channel *chan_list)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(chan_list, pdev_priv_obj->cur_chan_list,
		     NUM_CHANNELS * sizeof(struct regulatory_channel));

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_REG_CLIENT
QDF_STATUS
reg_get_secondary_current_chan_list(struct wlan_objmgr_pdev *pdev,
				    struct regulatory_channel *chan_list)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(chan_list, pdev_priv_obj->secondary_cur_chan_list,
		     NUM_CHANNELS * sizeof(struct regulatory_channel));

	return QDF_STATUS_SUCCESS;
}
#endif
