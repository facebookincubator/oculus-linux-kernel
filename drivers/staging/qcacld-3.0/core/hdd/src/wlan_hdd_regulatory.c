/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_regulatory.c
 *
 * hdd regulatory implementation
 */

#include "qdf_types.h"
#include "qdf_trace.h"
#include "wlan_hdd_main.h"
#include <wlan_osif_priv.h>
#include "wlan_hdd_regulatory.h"
#include <wlan_reg_ucfg_api.h>
#include "cds_regdomain.h"
#include "cds_utils.h"
#include "pld_common.h"
#include <net/cfg80211.h>
#include "wlan_policy_mgr_ucfg.h"

#define REG_RULE_2412_2462    REG_RULE(2412-10, 2462+10, 40, 0, 20, 0)

#define REG_RULE_2467_2472    REG_RULE(2467-10, 2472+10, 40, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_2484         REG_RULE(2484-10, 2484+10, 20, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS | \
				       NL80211_RRF_NO_OFDM)

#define REG_RULE_5180_5320    REG_RULE(5180-10, 5320+10, 160, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_5500_5720    REG_RULE(5500-10, 5720+10, 160, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_5745_5925    REG_RULE(5745-10, 5925+10, 80, 0, 20, \
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

static bool init_by_driver;
static bool init_by_reg_core;

struct regulatory_channel reg_channels[NUM_CHANNELS];

static const struct ieee80211_regdomain
hdd_world_regrules_60_61_62 = {
	.n_reg_rules = 6,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_2467_2472,
		REG_RULE_2484,
		REG_RULE_5180_5320,
		REG_RULE_5500_5720,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_63_65 = {
	.n_reg_rules = 4,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_2467_2472,
		REG_RULE_5180_5320,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_64 = {
	.n_reg_rules = 3,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_5180_5320,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_66_69 = {
	.n_reg_rules = 4,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_5180_5320,
		REG_RULE_5500_5720,
		REG_RULE_5745_5925,
	}
};

static const struct ieee80211_regdomain
hdd_world_regrules_67_68_6A_6C = {
	.n_reg_rules = 5,
	.alpha2 =  "00",
	.reg_rules = {
		REG_RULE_2412_2462,
		REG_RULE_2467_2472,
		REG_RULE_5180_5320,
		REG_RULE_5500_5720,
		REG_RULE_5745_5925,
	}
};

/**
 * hdd_get_world_regrules() - get the appropriate world regrules
 * @reg: regulatory data
 *
 * Return: regulatory rules ptr
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
static const struct ieee80211_regdomain *hdd_get_world_regrules(
	struct regulatory *reg)
{
	struct reg_dmn_pair *regpair =
		(struct reg_dmn_pair *)reg->regpair;

	switch (regpair->reg_dmn_pair) {
	case 0x60:
	case 0x61:
	case 0x62:
		return &hdd_world_regrules_60_61_62;
	case 0x63:
	case 0x65:
		return &hdd_world_regrules_63_65;
	case 0x64:
		return &hdd_world_regrules_64;
	case 0x66:
	case 0x69:
		return &hdd_world_regrules_66_69;
	case 0x67:
	case 0x68:
	case 0x6A:
	case 0x6C:
		return &hdd_world_regrules_67_68_6A_6C;
	default:
		hdd_warn("invalid world mode in BDF");
		return &hdd_world_regrules_60_61_62;
	}
}

/**
 * hdd_is_world_regdomain() - whether world regdomain
 * @reg_domain: integer regulatory domain
 *
 * Return: bool
 */
static bool hdd_is_world_regdomain(uint32_t reg_domain)
{
	uint32_t temp_regd = reg_domain & ~WORLD_ROAMING_FLAG;

	return ((temp_regd & CTRY_FLAG) != CTRY_FLAG) &&
		((temp_regd & WORLD_ROAMING_MASK) ==
		 WORLD_ROAMING_PREFIX);
}

/**
 * hdd_update_regulatory_info() - update regulatory info
 * @hdd_ctx: hdd context
 *
 * Return: Error Code
 */
static int hdd_update_regulatory_info(struct hdd_context *hdd_ctx)
{
	uint32_t country_code;

	country_code = cds_get_country_from_alpha2(hdd_ctx->reg.alpha2);

	hdd_ctx->reg.reg_domain = CTRY_FLAG;
	hdd_ctx->reg.reg_domain |= country_code;

	return cds_fill_some_regulatory_info(&hdd_ctx->reg);

}
#endif

/**
 * hdd_reset_global_reg_params - Reset global static reg params
 *
 * This function is helpful in static driver to reset
 * the global params.
 *
 * Return: void
 */
void hdd_reset_global_reg_params(void)
{
	init_by_driver = false;
	init_by_reg_core = false;
}

static void reg_program_config_vars(struct hdd_context *hdd_ctx,
				    struct reg_config_vars *config_vars)
{
	uint8_t band_capability = 0, indoor_chnl_marking = 0;
	uint32_t scan_11d_interval = 0;
	bool indoor_chan_enabled = false;
	uint32_t restart_beaconing = 0;
	bool enable_srd_chan = false;
	QDF_STATUS status;
	bool country_priority = 0;
	bool value = false;
	bool enable_dfs_scan = true;

	status = ucfg_mlme_get_band_capability(hdd_ctx->psoc, &band_capability);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get MLME band cap, defaulting to BAND_ALL");

	status = ucfg_policy_mgr_get_indoor_chnl_marking(hdd_ctx->psoc,
							 &indoor_chnl_marking);
	if (QDF_STATUS_SUCCESS != status)
		hdd_err("can't get indoor channel marking, using default");

	status = ucfg_mlme_is_11d_enabled(hdd_ctx->psoc, &value);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Invalid 11d_enable flag");
	config_vars->enable_11d_support = value;

	ucfg_mlme_get_scan_11d_interval(hdd_ctx->psoc, &scan_11d_interval);
	config_vars->scan_11d_interval = scan_11d_interval;

	ucfg_mlme_get_sap_country_priority(hdd_ctx->psoc,
					   &country_priority);
	config_vars->userspace_ctry_priority = country_priority;

	ucfg_scan_cfg_get_dfs_chan_scan_allowed(hdd_ctx->psoc,
						&enable_dfs_scan);

	config_vars->dfs_enabled = enable_dfs_scan;

	ucfg_mlme_get_indoor_channel_support(hdd_ctx->psoc,
					     &indoor_chan_enabled);
	config_vars->indoor_chan_enabled = indoor_chan_enabled;

	config_vars->force_ssc_disable_indoor_channel = indoor_chnl_marking;
	config_vars->band_capability = band_capability;

	ucfg_mlme_get_restart_beaconing_on_ch_avoid(hdd_ctx->psoc,
						    &restart_beaconing);
	config_vars->restart_beaconing = restart_beaconing;

	ucfg_mlme_get_etsi13_srd_chan_in_master_mode(hdd_ctx->psoc,
						     &enable_srd_chan);
	config_vars->enable_srd_chan_in_master_mode = enable_srd_chan;

	ucfg_mlme_get_11d_in_world_mode(hdd_ctx->psoc,
					&config_vars->enable_11d_in_world_mode);
}

/**
 * hdd_regulatory_wiphy_init() - regulatory wiphy init
 * @hdd_ctx: hdd context
 * @reg: regulatory data
 * @wiphy: wiphy structure
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_regulatory_wiphy_init(struct hdd_context *hdd_ctx,
				     struct regulatory *reg,
				     struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *reg_rules;
	int chan_num;
	struct ieee80211_channel *chan;

	if (hdd_is_world_regdomain(reg->reg_domain)) {
		reg_rules = hdd_get_world_regrules(reg);
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
	} else {
		wiphy->regulatory_flags |= REGULATORY_STRICT_REG;
		reg_rules = &hdd_world_regrules_60_61_62;
	}

	/*
	 * save the original driver regulatory flags
	 */
	hdd_ctx->reg.reg_flags = wiphy->regulatory_flags;
	wiphy_apply_custom_regulatory(wiphy, reg_rules);

	/*
	 * disable 2.4 Ghz channels that dont have 20 mhz bw
	 */
	for (chan_num = 0;
	     chan_num < wiphy->bands[HDD_NL80211_BAND_2GHZ]->n_channels;
	     chan_num++) {
		chan = &(wiphy->bands[HDD_NL80211_BAND_2GHZ]->channels[chan_num]);
		if (chan->flags & IEEE80211_CHAN_NO_20MHZ)
			chan->flags |= IEEE80211_CHAN_DISABLED;
	}

	/*
	 * restore the driver regulatory flags since
	 * wiphy_apply_custom_regulatory may have
	 * changed them
	 */
	wiphy->regulatory_flags = hdd_ctx->reg.reg_flags;

}
#else
static void hdd_regulatory_wiphy_init(struct hdd_context *hdd_ctx,
				     struct regulatory *reg,
				     struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *reg_rules;

	if (hdd_is_world_regdomain(reg->reg_domain)) {
		reg_rules = hdd_get_world_regrules(reg);
		wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
	} else {
		wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
		reg_rules = &hdd_world_regrules_60_61_62;
	}

	/*
	 * save the original driver regulatory flags
	 */
	hdd_ctx->reg.reg_flags = wiphy->flags;
	wiphy_apply_custom_regulatory(wiphy, reg_rules);

	/*
	 * restore the driver regulatory flags since
	 * wiphy_apply_custom_regulatory may have
	 * changed them
	 */
	wiphy->flags = hdd_ctx->reg.reg_flags;

}
#endif

/**
 * is_wiphy_custom_regulatory() - is custom regulatory defined
 * @wiphy: wiphy
 *
 * Return: int
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static int is_wiphy_custom_regulatory(struct wiphy *wiphy)
{

	return wiphy->regulatory_flags & REGULATORY_CUSTOM_REG;
}
#else
static int is_wiphy_custom_regulatory(struct wiphy *wiphy)
{
	return wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY;
}
#endif

/**
 * hdd_modify_wiphy() - modify wiphy
 * @wiphy: wiphy
 * @chan: channel structure
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
static void hdd_modify_wiphy(struct wiphy  *wiphy,
			     struct ieee80211_channel *chan)
{
	const struct ieee80211_reg_rule *reg_rule;

	if (is_wiphy_custom_regulatory(wiphy)) {
		reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(chan->center_freq));
		if (!IS_ERR(reg_rule)) {
			chan->flags &= ~IEEE80211_CHAN_DISABLED;

			if (!(reg_rule->flags & NL80211_RRF_DFS)) {
				hdd_debug("Remove dfs restriction for %u",
					chan->center_freq);
				chan->flags &= ~IEEE80211_CHAN_RADAR;
			}

			if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN)) {
				hdd_debug("Remove passive restriction for %u",
					chan->center_freq);
				chan->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
			}

			if (!(reg_rule->flags & NL80211_RRF_NO_IBSS)) {
				hdd_debug("Remove no ibss restriction for %u",
					chan->center_freq);
				chan->flags &= ~IEEE80211_CHAN_NO_IBSS;
			}

			chan->max_power =
				MBM_TO_DBM(reg_rule->power_rule.max_eirp);
		}
	}
}
#endif

/**
 * hdd_set_dfs_region() - set the dfs_region
 * @dfs_region: the dfs_region to set
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_set_dfs_region(struct hdd_context *hdd_ctx,
			       enum dfs_reg dfs_reg)
{
	wlan_reg_set_dfs_region(hdd_ctx->pdev, dfs_reg);
}
#endif

/**
 * hdd_process_regulatory_data() - process regulatory data
 * @hdd_ctx: hdd context
 * @wiphy: wiphy
 * @reset: whether to reset channel data
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
static void hdd_process_regulatory_data(struct hdd_context *hdd_ctx,
					struct wiphy *wiphy,
					bool reset)
{
	int band_num;
	int chan_num;
	enum channel_enum chan_enum = CHAN_ENUM_1;
	struct ieee80211_channel *wiphy_chan, *wiphy_chan_144 = NULL;
	struct regulatory_channel *cds_chan;
	uint8_t band_capability, indoor_chnl_marking = 0;
	bool indoor;
	QDF_STATUS status;

	band_capability = hdd_ctx->curr_band;

	status = ucfg_policy_mgr_get_indoor_chnl_marking(hdd_ctx->psoc,
							 &indoor_chnl_marking);
	if (QDF_STATUS_SUCCESS != status)
		hdd_err("can't get indoor channel marking, using default");

	for (band_num = 0; band_num < HDD_NUM_NL80211_BANDS; band_num++) {

		if (!wiphy->bands[band_num])
			continue;

		for (chan_num = 0;
		     chan_num < wiphy->bands[band_num]->n_channels &&
		     chan_enum < NUM_CHANNELS;
		     chan_num++) {
			wiphy_chan =
				&(wiphy->bands[band_num]->channels[chan_num]);
			cds_chan = &(reg_channels[chan_enum]);
			cds_chan->chan_flags = 0;
			if (CHAN_ENUM_144 == chan_enum)
				wiphy_chan_144 = wiphy_chan;

			chan_enum++;

			if (!reset)
				hdd_modify_wiphy(wiphy, wiphy_chan);

			if (indoor_chnl_marking &&
			    (wiphy_chan->flags & IEEE80211_CHAN_INDOOR_ONLY))
				cds_chan->chan_flags |=
					REGULATORY_CHAN_INDOOR_ONLY;

			if (wiphy_chan->flags & IEEE80211_CHAN_DISABLED) {
				cds_chan->state = CHANNEL_STATE_DISABLE;
				cds_chan->chan_flags |=
					REGULATORY_CHAN_DISABLED;
			} else if (wiphy_chan->flags &
				    (IEEE80211_CHAN_RADAR |
				     IEEE80211_CHAN_PASSIVE_SCAN)) {
				cds_chan->state = CHANNEL_STATE_DFS;
				if (wiphy_chan->flags & IEEE80211_CHAN_RADAR)
					cds_chan->chan_flags |=
						REGULATORY_CHAN_RADAR;
				if (wiphy_chan->flags &
				    IEEE80211_CHAN_PASSIVE_SCAN)
					cds_chan->chan_flags |=
						REGULATORY_CHAN_NO_IR;
			} else if (wiphy_chan->flags &
				     IEEE80211_CHAN_INDOOR_ONLY) {
				cds_chan->chan_flags |=
						REGULATORY_CHAN_INDOOR_ONLY;

				ucfg_mlme_get_indoor_channel_support(
								hdd_ctx->psoc,
								&indoor);
				if (!indoor) {
					cds_chan->state = CHANNEL_STATE_DFS;
					wiphy_chan->flags |=
						IEEE80211_CHAN_PASSIVE_SCAN;
					cds_chan->chan_flags |=
						REGULATORY_CHAN_NO_IR;
				} else
					cds_chan->state = CHANNEL_STATE_ENABLE;
			} else
				cds_chan->state = CHANNEL_STATE_ENABLE;
			cds_chan->tx_power = wiphy_chan->max_power;
			if (wiphy_chan->flags & IEEE80211_CHAN_NO_10MHZ)
				cds_chan->max_bw = 5;
			else if (wiphy_chan->flags & IEEE80211_CHAN_NO_20MHZ)
				cds_chan->max_bw = 10;
			/*
			 * IEEE80211_CHAN_NO_HT40  is defined as 0x30 in kernel
			 * 4th BIT representing IEEE80211_CHAN_NO_HT40PLUS
			 * 5th BIT representing IEEE80211_CHAN_NO_HT40MINUS
			 *
			 * In order to claim no 40Mhz support value of
			 * wiphy_chan->flags needs to be 0x30.
			 * 0x20 and 0x10 values shows that either HT40+ or
			 * HT40- is not supported based on BIT set but they
			 * can support 40Mhz Operation.
			 */
			else if ((wiphy_chan->flags & IEEE80211_CHAN_NO_HT40) ==
					IEEE80211_CHAN_NO_HT40)
				cds_chan->max_bw = 20;
			else if (wiphy_chan->flags & IEEE80211_CHAN_NO_80MHZ)
				cds_chan->max_bw = 40;
			else if (wiphy_chan->flags & IEEE80211_CHAN_NO_160MHZ)
				cds_chan->max_bw = 80;
			else
				cds_chan->max_bw = 160;
		}
	}

	if (0 == (hdd_ctx->reg.eeprom_rd_ext &
		  (1 << WMI_REG_EXT_FCC_CH_144))) {
		cds_chan = &(reg_channels[CHAN_ENUM_144]);
		cds_chan->state = CHANNEL_STATE_DISABLE;
		if (wiphy_chan_144)
			wiphy_chan_144->flags |= IEEE80211_CHAN_DISABLED;
	}

	wlan_hdd_cfg80211_update_band(hdd_ctx, wiphy, band_capability);
}

/**
 * hdd_regulatory_init_no_offload() - regulatory init
 * @hdd_ctx: hdd context
 * @wiphy: wiphy
 *
 * Return: int
 */
static int hdd_regulatory_init_no_offload(struct hdd_context *hdd_ctx,
					  struct wiphy *wiphy)
{
	int ret_val;
	struct regulatory *reg_info;
	enum dfs_reg dfs_reg;
	struct reg_config_vars config_vars;

	reg_info = &hdd_ctx->reg;

	ret_val = cds_fill_some_regulatory_info(reg_info);
	if (ret_val) {
		hdd_err("incorrect BDF regulatory data");
		return ret_val;
	}

	hdd_set_dfs_region(hdd_ctx, DFS_FCC_REGION);

	hdd_regulatory_wiphy_init(hdd_ctx, reg_info, wiphy);

	hdd_process_regulatory_data(hdd_ctx, wiphy, true);

	reg_info->cc_src = SOURCE_DRIVER;

	ucfg_reg_set_default_country(hdd_ctx->psoc, reg_info->alpha2);

	cds_fill_and_send_ctl_to_fw(reg_info);

	wlan_reg_get_dfs_region(hdd_ctx->pdev, &dfs_reg);

	reg_program_config_vars(hdd_ctx, &config_vars);
	ucfg_reg_set_config_vars(hdd_ctx->psoc, config_vars);
	ucfg_reg_program_mas_chan_list(hdd_ctx->psoc,
				       reg_channels,
				       hdd_ctx->reg.alpha2,
				       dfs_reg);

	return 0;
}
#endif

/**
 * hdd_modify_indoor_channel_state_flags() - modify wiphy flags and cds state
 * @wiphy_chan: wiphy channel number
 * @cds_chan: cds channel structure
 * @disable: Disable/enable the flags
 *
 * Modify wiphy flags and cds state if channel is indoor.
 *
 * Return: void
 */
void hdd_modify_indoor_channel_state_flags(
	struct hdd_context *hdd_ctx,
	struct ieee80211_channel *wiphy_chan,
	struct regulatory_channel *cds_chan,
	enum channel_enum chan_enum, int chan_num, bool disable)
{
	bool indoor_support;

	ucfg_mlme_get_indoor_channel_support(hdd_ctx->psoc, &indoor_support);

	/* Mark indoor channel to disable in wiphy and cds */
	if (disable) {
		if (wiphy_chan->flags & IEEE80211_CHAN_INDOOR_ONLY) {
			wiphy_chan->flags |=
				IEEE80211_CHAN_DISABLED;
			hdd_info("Mark indoor channel %d as disable",
				cds_chan->center_freq);
			cds_chan->state =
				CHANNEL_STATE_DISABLE;
		}
	} else {
		if (wiphy_chan->flags & IEEE80211_CHAN_INDOOR_ONLY) {
			wiphy_chan->flags &=
					~IEEE80211_CHAN_DISABLED;
			 /*
			  * Indoor channels may be marked as dfs / enable
			  * during regulatory processing
			  */
			if ((wiphy_chan->flags &
				(IEEE80211_CHAN_RADAR |
				IEEE80211_CHAN_PASSIVE_SCAN)) ||
			     ((indoor_support == false) &&
				(wiphy_chan->flags &
				IEEE80211_CHAN_INDOOR_ONLY)))
				cds_chan->state =
					CHANNEL_STATE_DFS;
			else
				cds_chan->state =
					CHANNEL_STATE_ENABLE;
			hdd_debug("Mark indoor channel %d as cds_chan state %d",
					cds_chan->chan_num, cds_chan->state);
		}
	}

}

void hdd_update_indoor_channel(struct hdd_context *hdd_ctx,
					bool disable)
{
	int band_num;
	int chan_num;
	enum channel_enum chan_enum = CHAN_ENUM_1;
	struct ieee80211_channel *wiphy_chan, *wiphy_chan_144 = NULL;
	struct regulatory_channel *cds_chan;
	uint8_t band_capability;
	struct wiphy *wiphy = hdd_ctx->wiphy;

	hdd_enter();
	hdd_debug("mark indoor channel disable: %d", disable);

	band_capability = hdd_ctx->curr_band;
	for (band_num = 0; band_num < HDD_NUM_NL80211_BANDS; band_num++) {

		if (!wiphy->bands[band_num])
			continue;

		for (chan_num = 0;
		     chan_num < wiphy->bands[band_num]->n_channels &&
		     chan_enum < NUM_CHANNELS;
		     chan_num++) {

			wiphy_chan =
				&(wiphy->bands[band_num]->channels[chan_num]);
			cds_chan = &(reg_channels[chan_enum]);
			if (chan_enum == CHAN_ENUM_144)
				wiphy_chan_144 = wiphy_chan;

			chan_enum++;
			hdd_modify_indoor_channel_state_flags(hdd_ctx,
				wiphy_chan, cds_chan,
				chan_enum, chan_num, disable);
		}
	}

	/* Notify the regulatory domain to update the channel list */
	if (QDF_IS_STATUS_ERROR(ucfg_reg_notify_sap_event(hdd_ctx->pdev,
							  disable))) {
		hdd_err("Failed to notify sap event");
	}
	hdd_exit();

}

/**
 * hdd_program_country_code() - process channel information from country code
 * @hdd_ctx: hddc context
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
void hdd_program_country_code(struct hdd_context *hdd_ctx)
{
}
#else
void hdd_program_country_code(struct hdd_context *hdd_ctx)
{
	struct wiphy *wiphy = hdd_ctx->wiphy;
	uint8_t *country_alpha2 = hdd_ctx->reg.alpha2;

	if (!init_by_reg_core && !init_by_driver) {
		init_by_driver = true;
		if (('0' != country_alpha2[0]) ||
		    ('0' != country_alpha2[1]))
			regulatory_hint(wiphy, country_alpha2);
	}
}
#endif

int hdd_reg_set_country(struct hdd_context *hdd_ctx, char *country_code)
{
	QDF_STATUS status;
	uint8_t cc[REG_ALPHA2_LEN + 1];

	if (!country_code) {
		hdd_err("country_code is null");
		return -EINVAL;
	}

	qdf_mem_copy(cc, country_code, REG_ALPHA2_LEN);
	cc[REG_ALPHA2_LEN] = '\0';

	status = ucfg_reg_set_country(hdd_ctx->pdev, cc);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to set country");

	return qdf_status_to_os_return(status);
}

int hdd_reg_set_band(struct net_device *dev, u8 ui_band)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	mac_handle_t mac_handle;
	enum band_info band;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx;
	enum band_info current_band;
	enum band_info connected_band;
	long lrc;
	uint8_t band_capability;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	switch (ui_band) {
	case WLAN_HDD_UI_BAND_AUTO:
		band = BAND_ALL;
		break;
	case WLAN_HDD_UI_BAND_5_GHZ:
		band = BAND_5G;
		break;
	case WLAN_HDD_UI_BAND_2_4_GHZ:
		band = BAND_2G;
		break;
	default:
		hdd_err("Invalid band value %u", ui_band);
		return -EINVAL;
	}

	hdd_debug("change band to %u", band);

	status = ucfg_mlme_get_band_capability(hdd_ctx->psoc, &band_capability);
	if (QDF_IS_STATUS_ERROR(status))
		return -EIO;

	if ((band == BAND_2G && band_capability == 2) ||
	    (band == BAND_5G && band_capability == 1) ||
	    (band == BAND_ALL && band_capability != 0)) {
		hdd_err("band value %u violate INI settings %u",
			  band, band_capability);
		return -EIO;
	}

	if (band == BAND_ALL) {
		hdd_debug("Auto band received. Setting band same as ini value %d",
			band_capability);
		band = band_capability;
	}

	if (ucfg_reg_get_curr_band(hdd_ctx->pdev, &current_band) !=
	    QDF_STATUS_SUCCESS) {
		hdd_debug("Failed to get current band config");
		return -EIO;
	}

	if (current_band == band)
		return 0;

	hdd_ctx->curr_band = band;

	/* Change band request received.
	 * Abort pending scan requests, flush the existing scan results,
	 * and change the band capability
	 */
	hdd_debug("Current band value = %u, new setting %u ",
			current_band, band);

	mac_handle = hdd_ctx->mac_handle;
	hdd_for_each_adapter(hdd_ctx, adapter) {
		wlan_abort_scan(hdd_ctx->pdev, INVAL_PDEV_ID,
				adapter->vdev_id, INVALID_SCAN_ID, false);
		connected_band = hdd_conn_get_connected_band(
				WLAN_HDD_GET_STATION_CTX_PTR(adapter));

		/* Handling is done only for STA and P2P */
		if (band != BAND_ALL &&
		    ((adapter->device_mode == QDF_STA_MODE) ||
		     (adapter->device_mode == QDF_P2P_CLIENT_MODE)) &&
		    (hdd_conn_is_connected(
				WLAN_HDD_GET_STATION_CTX_PTR(adapter)))
			&& (connected_band != band)) {
			status = QDF_STATUS_SUCCESS;

			/* STA already connected on current
			 * band, So issue disconnect first,
			 * then change the band
			 */

			hdd_debug("STA (Device mode %s(%d)) connected in band %u, Changing band to %u, Issuing Disconnect",
				  qdf_opmode_str(adapter->device_mode),
				  adapter->device_mode, current_band, band);
			INIT_COMPLETION(adapter->disconnect_comp_var);

			status = sme_roam_disconnect(
					mac_handle,
					adapter->vdev_id,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					eSIR_MAC_OPER_CHANNEL_BAND_CHANGE);

			if (QDF_STATUS_SUCCESS != status) {
				hdd_err("sme_roam_disconnect failure, status: %d",
						(int)status);
				return -EINVAL;
			}

			lrc = wait_for_completion_timeout(
					&adapter->disconnect_comp_var,
					msecs_to_jiffies(
						SME_DISCONNECT_TIMEOUT));

			if (lrc == 0) {
				hdd_err("Timeout while waiting for csr_roam_disconnect");
				return -ETIMEDOUT;
			}
		}
		ucfg_scan_flush_results(hdd_ctx->pdev, NULL);
	}

	if (QDF_IS_STATUS_ERROR(ucfg_reg_set_band(hdd_ctx->pdev, band))) {
		hdd_err("Failed to set the band value to %u", band);
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_restore_custom_reg_settings() - restore custom reg settings
 * @wiphy: wiphy structure
 * @country_alpha2: alpha2 of the country
 * @reset: whether wiphy is reset
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_restore_custom_reg_settings(struct wiphy *wiphy,
					    uint8_t *country_alpha2,
					    bool *reset)
{
}
#else
static void hdd_restore_custom_reg_settings(struct wiphy *wiphy,
					    uint8_t *country_alpha2,
					    bool *reset)
{
	struct ieee80211_supported_band *sband;
	enum nl80211_band band;
	struct ieee80211_channel *chan;
	int i;

	if ((country_alpha2[0] == '0') &&
	    (country_alpha2[1] == '0') &&
	    (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY)) {

		for (band = 0; band < HDD_NUM_NL80211_BANDS; band++) {
			sband = wiphy->bands[band];
			if (!sband)
				continue;
			for (i = 0; i < sband->n_channels; i++) {
				chan = &sband->channels[i];
				chan->flags = chan->orig_flags;
				chan->max_antenna_gain = chan->orig_mag;
				chan->max_power = chan->orig_mpwr;
			}
		}
		*reset = true;
	}
}
#endif

/**
 * hdd_restore_reg_flags() - restore regulatory flags
 * @flags: regulatory flags
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
static void hdd_restore_reg_flags(struct wiphy *wiphy, uint32_t flags)
{
	wiphy->regulatory_flags = flags;
}
#else
static void hdd_restore_reg_flags(struct wiphy *wiphy, uint32_t flags)
{
	wiphy->flags = flags;
}
#endif

/**
 * hdd_reg_notifier() - regulatory notifier
 * @wiphy: wiphy
 * @request: regulatory request
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
void hdd_reg_notifier(struct wiphy *wiphy,
		      struct regulatory_request *request)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	char country[REG_ALPHA2_LEN + 1] = {0};

	hdd_debug("country: %c%c, initiator %d, dfs_region: %d",
		  request->alpha2[0],
		  request->alpha2[1],
		  request->initiator,
		  request->dfs_region);

	switch (request->initiator) {
	case NL80211_REGDOM_SET_BY_USER:

		if (request->user_reg_hint_type !=
		    NL80211_USER_REG_HINT_CELL_BASE)
			return;

		qdf_mem_copy(country, request->alpha2, QDF_MIN(
			     sizeof(request->alpha2), sizeof(country)));
		status = ucfg_reg_set_country(hdd_ctx->pdev, country);
		break;
	case NL80211_REGDOM_SET_BY_CORE:
	case NL80211_REGDOM_SET_BY_COUNTRY_IE:
	case NL80211_REGDOM_SET_BY_DRIVER:
	default:
		break;
	}

	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to set country");
}
#else
void hdd_reg_notifier(struct wiphy *wiphy,
		      struct regulatory_request *request)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	bool reset = false;
	enum dfs_reg dfs_reg;
	struct reg_config_vars config_vars;
	int ret_val;

	hdd_debug("country: %c%c, initiator %d, dfs_region: %d",
		  request->alpha2[0],
		  request->alpha2[1],
		  request->initiator,
		  request->dfs_region);

	if (!hdd_ctx) {
		hdd_err("invalid hdd_ctx pointer");
		return;
	}

	if (cds_is_driver_unloading() || cds_is_driver_recovering() ||
	    cds_is_driver_in_bad_state()) {
		hdd_err("%s: unloading or ssr in progress, ignore",
			__func__);
		return;
	}

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_err("Driver module is closed; dropping request");
		return;
	}

	if (hdd_ctx->is_wiphy_suspended == true) {
		hdd_err("%s: system/cfg80211 is already suspend", __func__);
		return;
	}

	if (('K' == request->alpha2[0]) &&
	    ('R' == request->alpha2[1]))
		request->dfs_region = (enum nl80211_dfs_regions)DFS_KR_REGION;

	if (('C' == request->alpha2[0]) &&
	    ('N' == request->alpha2[1]))
		request->dfs_region = (enum nl80211_dfs_regions)DFS_CN_REGION;

	/* first check if this callback is in response to the driver callback */
	switch (request->initiator) {
	case NL80211_REGDOM_SET_BY_DRIVER:
	case NL80211_REGDOM_SET_BY_CORE:
	case NL80211_REGDOM_SET_BY_USER:

		if ((false == init_by_driver) &&
		    (false == init_by_reg_core)) {

			if (NL80211_REGDOM_SET_BY_CORE == request->initiator)
				return;
			init_by_reg_core = true;
		}

		if ((NL80211_REGDOM_SET_BY_DRIVER == request->initiator) &&
		    (true == init_by_driver)) {

			/*
			 * restore the driver regulatory flags since
			 * regulatory_hint may have
			 * changed them
			 */
			hdd_restore_reg_flags(wiphy, hdd_ctx->reg.reg_flags);
		}

		if (NL80211_REGDOM_SET_BY_CORE == request->initiator) {
			hdd_ctx->reg.cc_src = SOURCE_CORE;
			if (is_wiphy_custom_regulatory(wiphy))
				reset = true;
		} else if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator) {
			hdd_ctx->reg.cc_src = SOURCE_DRIVER;
			sme_set_cc_src(hdd_ctx->mac_handle, SOURCE_DRIVER);
		} else {
			hdd_ctx->reg.cc_src = SOURCE_USERSPACE;
			hdd_restore_custom_reg_settings(wiphy,
							request->alpha2,
							&reset);
		}

		hdd_ctx->reg.alpha2[0] = request->alpha2[0];
		hdd_ctx->reg.alpha2[1] = request->alpha2[1];

		ret_val = hdd_update_regulatory_info(hdd_ctx);
		if (ret_val) {
			hdd_err("invalid reg info, do not process");
			return;
		}

		hdd_process_regulatory_data(hdd_ctx, wiphy, reset);

		sme_generic_change_country_code(hdd_ctx->mac_handle,
						hdd_ctx->reg.alpha2);

		cds_fill_and_send_ctl_to_fw(&hdd_ctx->reg);

		hdd_set_dfs_region(hdd_ctx, request->dfs_region);
		wlan_reg_get_dfs_region(hdd_ctx->pdev, &dfs_reg);

		reg_program_config_vars(hdd_ctx, &config_vars);
		ucfg_reg_set_config_vars(hdd_ctx->psoc, config_vars);
		ucfg_reg_program_mas_chan_list(hdd_ctx->psoc,
					       reg_channels,
					       hdd_ctx->reg.alpha2,
					       dfs_reg);
		break;

	default:
		break;
	}
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
static void fill_wiphy_channel(struct ieee80211_channel *wiphy_chan,
			       struct regulatory_channel *cur_chan)
{

	wiphy_chan->flags = 0;
	wiphy_chan->max_power = cur_chan->tx_power;

	if (cur_chan->chan_flags & REGULATORY_CHAN_DISABLED)
		wiphy_chan->flags  |= IEEE80211_CHAN_DISABLED;
	if (cur_chan->chan_flags & REGULATORY_CHAN_NO_IR)
		wiphy_chan->flags  |= IEEE80211_CHAN_NO_IR;
	if (cur_chan->chan_flags & REGULATORY_CHAN_RADAR)
		wiphy_chan->flags  |= IEEE80211_CHAN_RADAR;
	if (cur_chan->chan_flags & REGULATORY_CHAN_NO_OFDM)
		wiphy_chan->flags  |= IEEE80211_CHAN_NO_OFDM;
	if (cur_chan->chan_flags & REGULATORY_CHAN_INDOOR_ONLY)
		wiphy_chan->flags  |= IEEE80211_CHAN_INDOOR_ONLY;

	if (cur_chan->max_bw < 10)
		wiphy_chan->flags |= IEEE80211_CHAN_NO_10MHZ;
	if (cur_chan->max_bw < 20)
		wiphy_chan->flags |= IEEE80211_CHAN_NO_20MHZ;
	if (cur_chan->max_bw < 40)
		wiphy_chan->flags |= IEEE80211_CHAN_NO_HT40;
	if (cur_chan->max_bw < 80)
		wiphy_chan->flags |= IEEE80211_CHAN_NO_80MHZ;
	if (cur_chan->max_bw < 160)
		wiphy_chan->flags |= IEEE80211_CHAN_NO_160MHZ;

	wiphy_chan->orig_flags = wiphy_chan->flags;
}

static void fill_wiphy_band_channels(struct wiphy *wiphy,
				     struct regulatory_channel *cur_chan_list,
				     uint8_t band_id)
{
	uint32_t wiphy_num_chan, wiphy_index;
	uint32_t chan_cnt;
	struct ieee80211_channel *wiphy_chan;

	if (!wiphy->bands[band_id])
		return;

	wiphy_num_chan = wiphy->bands[band_id]->n_channels;
	wiphy_chan = wiphy->bands[band_id]->channels;

	for (wiphy_index = 0; wiphy_index < wiphy_num_chan; wiphy_index++) {
		for (chan_cnt = 0; chan_cnt < NUM_CHANNELS; chan_cnt++) {
			if (wiphy_chan[wiphy_index].center_freq ==
			    cur_chan_list[chan_cnt].center_freq) {
				fill_wiphy_channel(&(wiphy_chan[wiphy_index]),
						   &(cur_chan_list[chan_cnt]));
				break;
			}
		}
	}
}

#ifdef FEATURE_WLAN_CH_AVOID
/**
 * hdd_ch_avoid_ind() - Avoid notified channels from FW handler
 * @adapter:	HDD adapter pointer
 * @indParam:	Channel avoid notification parameter
 *
 * Avoid channel notification from FW handler.
 * FW will send un-safe channel list to avoid over wrapping.
 * hostapd should not use notified channel
 *
 * Return: None
 */
void hdd_ch_avoid_ind(struct hdd_context *hdd_ctxt,
		struct unsafe_ch_list *unsafe_chan_list,
		struct ch_avoid_ind_type *avoid_freq_list)
{
	uint16_t *local_unsafe_list;
	uint16_t local_unsafe_list_count;

	/* Basic sanity */
	if (!hdd_ctxt) {
		hdd_err("Invalid arguments");
		return;
	}

	mutex_lock(&hdd_ctxt->avoid_freq_lock);
	qdf_mem_copy(&hdd_ctxt->coex_avoid_freq_list, avoid_freq_list,
			sizeof(struct ch_avoid_ind_type));
	mutex_unlock(&hdd_ctxt->avoid_freq_lock);

	if (hdd_clone_local_unsafe_chan(hdd_ctxt,
					&local_unsafe_list,
					&local_unsafe_list_count) != 0) {
		hdd_err("failed to clone cur unsafe chan list");
		return;
	}

	/* clear existing unsafe channel cache */
	hdd_ctxt->unsafe_channel_count = 0;
	qdf_mem_zero(hdd_ctxt->unsafe_channel_list,
					sizeof(hdd_ctxt->unsafe_channel_list));

	hdd_ctxt->unsafe_channel_count = unsafe_chan_list->ch_cnt;

	qdf_mem_copy(hdd_ctxt->unsafe_channel_list, unsafe_chan_list->ch_list,
					sizeof(hdd_ctxt->unsafe_channel_list));
	hdd_debug("number of unsafe channels is %d ",
	       hdd_ctxt->unsafe_channel_count);

	if (pld_set_wlan_unsafe_channel(hdd_ctxt->parent_dev,
					hdd_ctxt->unsafe_channel_list,
				hdd_ctxt->unsafe_channel_count)) {
		hdd_err("Failed to set unsafe channel");

		/* clear existing unsafe channel cache */
		hdd_ctxt->unsafe_channel_count = 0;
		qdf_mem_zero(hdd_ctxt->unsafe_channel_list,
			sizeof(hdd_ctxt->unsafe_channel_list));
		qdf_mem_free(local_unsafe_list);
		return;
	}

	mutex_lock(&hdd_ctxt->avoid_freq_lock);
	if (hdd_ctxt->dnbs_avoid_freq_list.ch_avoid_range_cnt)
		if (wlan_hdd_merge_avoid_freqs(avoid_freq_list,
					&hdd_ctxt->dnbs_avoid_freq_list)) {
			mutex_unlock(&hdd_ctxt->avoid_freq_lock);
			hdd_debug("unable to merge avoid freqs");
			qdf_mem_free(local_unsafe_list);
			return;
	}
	mutex_unlock(&hdd_ctxt->avoid_freq_lock);
	/*
	 * first update the unsafe channel list to the platform driver and
	 * send the avoid freq event to the application
	 */
	wlan_hdd_send_avoid_freq_event(hdd_ctxt, avoid_freq_list);

	if (!hdd_ctxt->unsafe_channel_count) {
		hdd_debug("no unsafe channels - not restarting SAP");
		qdf_mem_free(local_unsafe_list);
		return;
	}
	if (hdd_local_unsafe_channel_updated(hdd_ctxt,
					    local_unsafe_list,
					    local_unsafe_list_count))
		hdd_unsafe_channel_restart_sap(hdd_ctxt);
	qdf_mem_free(local_unsafe_list);

}
#endif

#if defined CFG80211_USER_HINT_CELL_BASE_SELF_MANAGED || \
	    (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0))
static void map_nl_reg_rule_flags(uint16_t drv_reg_rule_flag,
				  uint32_t *regd_rule_flag)
{
	if (drv_reg_rule_flag & REGULATORY_CHAN_NO_IR)
		*regd_rule_flag |= NL80211_RRF_NO_IR;
	if (drv_reg_rule_flag & REGULATORY_CHAN_RADAR)
		*regd_rule_flag |= NL80211_RRF_DFS;
	if (drv_reg_rule_flag & REGULATORY_CHAN_INDOOR_ONLY)
		*regd_rule_flag |= NL80211_RRF_NO_OUTDOOR;
	if (drv_reg_rule_flag & REGULATORY_CHAN_NO_OFDM)
		*regd_rule_flag |= NL80211_RRF_NO_OFDM;
}

/**
 * dfs_reg_to_nl80211_dfs_regions() - convert dfs_reg to nl80211_dfs_regions
 * @dfs_region: DFS region
 *
 * Return: nl80211_dfs_regions
 */
static enum nl80211_dfs_regions dfs_reg_to_nl80211_dfs_regions(
					enum dfs_reg dfs_region)
{
	switch (dfs_region) {
	case DFS_UNINIT_REGION:
		return NL80211_DFS_UNSET;
	case DFS_FCC_REGION:
		return NL80211_DFS_FCC;
	case DFS_ETSI_REGION:
		return NL80211_DFS_ETSI;
	case DFS_MKK_REGION:
		return NL80211_DFS_JP;
	default:
		return NL80211_DFS_UNSET;
	}
}

void hdd_send_wiphy_regd_sync_event(struct hdd_context *hdd_ctx)
{
	struct ieee80211_regdomain *regd;
	struct ieee80211_reg_rule *regd_rules;
	struct reg_rule_info reg_rules_struct;
	struct reg_rule_info *reg_rules;
	QDF_STATUS  status;
	uint8_t i;

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return;
	}

	status = ucfg_reg_get_regd_rules(hdd_ctx->pdev, &reg_rules_struct);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get reg rules");
		return;
	}

	reg_rules = &reg_rules_struct;
	if (!reg_rules->num_of_reg_rules) {
		hdd_err("no reg rules %d", reg_rules->num_of_reg_rules);
		return;
	}

	regd = qdf_mem_malloc((reg_rules->num_of_reg_rules *
				sizeof(*regd_rules) + sizeof(*regd)));
	if (!regd) {
		hdd_err("mem alloc failed for reg rules");
		return;
	}

	regd->n_reg_rules = reg_rules->num_of_reg_rules;
	qdf_mem_copy(regd->alpha2, reg_rules->alpha2, REG_ALPHA2_LEN + 1);
	regd->dfs_region =
		dfs_reg_to_nl80211_dfs_regions(reg_rules->dfs_region);
	regd_rules = regd->reg_rules;
	hdd_debug("Regulatory Domain %s", regd->alpha2);
	hdd_debug("start freq\tend freq\t@ max_bw\tant_gain\tpwr\tflags");
	for (i = 0; i < reg_rules->num_of_reg_rules; i++) {
		regd_rules[i].freq_range.start_freq_khz =
			reg_rules->reg_rules[i].start_freq * 1000;
		regd_rules[i].freq_range.end_freq_khz =
			reg_rules->reg_rules[i].end_freq * 1000;
		regd_rules[i].freq_range.max_bandwidth_khz =
			reg_rules->reg_rules[i].max_bw * 1000;
		regd_rules[i].power_rule.max_antenna_gain =
			reg_rules->reg_rules[i].ant_gain * 100;
		regd_rules[i].power_rule.max_eirp =
			reg_rules->reg_rules[i].reg_power * 100;
		map_nl_reg_rule_flags(reg_rules->reg_rules[i].flags,
				      &regd_rules[i].flags);
		hdd_debug("%d KHz\t%d KHz\t@ %d KHz\t%d\t\t%d\t%d",
			  regd_rules[i].freq_range.start_freq_khz,
			  regd_rules[i].freq_range.end_freq_khz,
			  regd_rules[i].freq_range.max_bandwidth_khz,
			  regd_rules[i].power_rule.max_antenna_gain,
			  regd_rules[i].power_rule.max_eirp,
			  regd_rules[i].flags);
	}

	regulatory_set_wiphy_regd(hdd_ctx->wiphy, regd);

	hdd_debug("regd sync event sent with reg rules info");
	qdf_mem_free(regd);
}
#endif

static void hdd_regulatory_dyn_cbk(struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_pdev *pdev,
				   struct regulatory_channel *chan_list,
				   struct avoid_freq_ind_data *avoid_freq_ind,
				   void *arg)
{
	struct wiphy *wiphy;
	struct pdev_osif_priv *pdev_priv;
	struct hdd_context *hdd_ctx;
	enum country_src cc_src;
	uint8_t alpha2[REG_ALPHA2_LEN + 1];

	pdev_priv = wlan_pdev_get_ospriv(pdev);
	wiphy = pdev_priv->wiphy;
	hdd_ctx = wiphy_priv(wiphy);

	hdd_debug("process channel list update from regulatory");

	fill_wiphy_band_channels(wiphy, chan_list, NL80211_BAND_2GHZ);
	fill_wiphy_band_channels(wiphy, chan_list, NL80211_BAND_5GHZ);

	cc_src = ucfg_reg_get_cc_and_src(hdd_ctx->psoc, alpha2);
	qdf_mem_copy(hdd_ctx->reg.alpha2, alpha2, REG_ALPHA2_LEN + 1);
	sme_set_cc_src(hdd_ctx->mac_handle, cc_src);

	/* Check the kernel version for upstream commit aced43ce780dc5 that
	 * has support for processing user cell_base hints when wiphy is
	 * self managed or check the backport flag for the same.
	 */
#if defined CFG80211_USER_HINT_CELL_BASE_SELF_MANAGED || \
	    (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0))
	if (wiphy->registered)
		hdd_send_wiphy_regd_sync_event(hdd_ctx);
#endif

	if (avoid_freq_ind) {
		hdd_ch_avoid_ind(hdd_ctx, &avoid_freq_ind->chan_list,
				&avoid_freq_ind->freq_list);
	} else {
		hdd_config_tdls_with_band_switch(hdd_ctx);

		sme_generic_change_country_code(hdd_ctx->mac_handle,
				hdd_ctx->reg.alpha2);
		/*Check whether need restart SAP/P2p Go*/
		policy_mgr_check_concurrent_intf_and_restart_sap(hdd_ctx->psoc);
	}
}

int hdd_update_regulatory_config(struct hdd_context *hdd_ctx)
{
	struct reg_config_vars config_vars;

	reg_program_config_vars(hdd_ctx, &config_vars);
	ucfg_reg_set_config_vars(hdd_ctx->psoc, config_vars);
	return 0;
}

int hdd_regulatory_init(struct hdd_context *hdd_ctx, struct wiphy *wiphy)
{
	bool offload_enabled;
	struct regulatory_channel cur_chan_list[NUM_CHANNELS];
	enum country_src cc_src;
	uint8_t alpha2[REG_ALPHA2_LEN + 1];

	ucfg_reg_register_chan_change_callback(hdd_ctx->psoc,
					       hdd_regulatory_dyn_cbk,
					       NULL);

	wiphy->regulatory_flags |= REGULATORY_WIPHY_SELF_MANAGED;
	/* Check the kernel version for upstream commit aced43ce780dc5 that
	 * has support for processing user cell_base hints when wiphy is
	 * self managed or check the backport flag for the same.
	 */
#if defined CFG80211_USER_HINT_CELL_BASE_SELF_MANAGED || \
	    (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 18, 0))
	wiphy->features |= NL80211_FEATURE_CELL_BASE_REG_HINTS;
#endif
	wiphy->reg_notifier = hdd_reg_notifier;
	offload_enabled = ucfg_reg_is_regdb_offloaded(hdd_ctx->psoc);
	hdd_debug("regulatory offload_enabled %d", offload_enabled);
	if (offload_enabled) {
		hdd_ctx->reg_offload = true;
		ucfg_reg_get_current_chan_list(hdd_ctx->pdev,
					       cur_chan_list);
		fill_wiphy_band_channels(wiphy, cur_chan_list,
					 NL80211_BAND_2GHZ);
		fill_wiphy_band_channels(wiphy, cur_chan_list,
					 NL80211_BAND_5GHZ);

		cc_src = ucfg_reg_get_cc_and_src(hdd_ctx->psoc, alpha2);
		qdf_mem_copy(hdd_ctx->reg.alpha2, alpha2, REG_ALPHA2_LEN + 1);
		sme_set_cc_src(hdd_ctx->mac_handle, cc_src);
	} else {
		hdd_ctx->reg_offload = false;
	}

	return 0;
}

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
int hdd_regulatory_init(struct hdd_context *hdd_ctx, struct wiphy *wiphy)
{
	hdd_ctx->reg_offload = false;
	wiphy->reg_notifier = hdd_reg_notifier;
	wiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS;
	wiphy->regulatory_flags |= REGULATORY_COUNTRY_IE_IGNORE;
	hdd_regulatory_init_no_offload(hdd_ctx, wiphy);

	return 0;
}

#else
int hdd_regulatory_init(struct hdd_context *hdd_ctx, struct wiphy *wiphy)
{
	hdd_ctx->reg_offload = false;
	wiphy->reg_notifier = hdd_reg_notifier;
	wiphy->flags |= WIPHY_FLAG_DISABLE_BEACON_HINTS;
	wiphy->country_ie_pref |= NL80211_COUNTRY_IE_IGNORE_CORE;
	hdd_regulatory_init_no_offload(hdd_ctx, wiphy);

	return 0;
}
#endif

void hdd_update_regdb_offload_config(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	bool ignore_fw_reg_offload_ind = false;

	status = ucfg_mlme_get_ignore_fw_reg_offload_ind(
						hdd_ctx->psoc,
						&ignore_fw_reg_offload_ind);
	if (!ignore_fw_reg_offload_ind) {
		hdd_debug("regdb offload is based on firmware capability");
		return;
	}

	hdd_debug("Ignore regdb offload Indication from FW");
	ucfg_set_ignore_fw_reg_offload_ind(hdd_ctx->psoc);
}
