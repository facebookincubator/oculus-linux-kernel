/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 *
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
 * DOC: reg_priv.h
 * This file contains regulatory component private data structures.
 */

#ifndef __REG_PRIV_H
#define __REG_PRIV_H

#include "reg_db.h"
#include "reg_services.h"

#define reg_log(level, args...) \
	QDF_TRACE(QDF_MODULE_ID_REGULATORY, level, ## args)
#define reg_logfl(level, format, args...) reg_log(level, FL(format), ## args)
#define reg_alert(format, args...) \
		reg_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define reg_err(format, args...) \
		reg_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define reg_warn(format, args...) \
		reg_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define reg_notice(format, args...) \
		reg_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define reg_info(format, args...) \
		reg_logfl(QDF_TRACE_LEVEL_INFO_HIGH, format, ## args)
#define reg_debug(format, args...) \
		reg_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)
#define reg_debug_rl(params...) \
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_REGULATORY, params)


/**
 * struct wlan_regulatory_psoc_priv_obj - wlan regulatory psoc private object
 * @new_user_ctry_pending: In this array, element[phy_id] is true if any user
 *	country update is pending for pdev (phy_id), used in case of MCL.
 * @new_init_ctry_pending: In this array, element[phy_id] is true if any user
 *	country update is pending for pdev (phy_id), used in case of WIN.
 * @new_11d_ctry_pending: In this array, element[phy_id] is true if any 11d
 *	country update is pending for pdev (phy_id).
 * @world_country_pending: In this array, element[phy_id] is true if any world
 *	country update is pending for pdev (phy_id).
 * @def_pdev_id: Default pdev id, used in case of MCL
 * @ignore_fw_reg_offload_ind: Ignore FW reg offload indication
 */
struct wlan_regulatory_psoc_priv_obj {
	struct mas_chan_params mas_chan_params[PSOC_MAX_PHY_REG_CAP];
	bool offload_enabled;
	uint8_t num_phy;
	char cur_country[REG_ALPHA2_LEN + 1];
	char def_country[REG_ALPHA2_LEN + 1];
	uint16_t def_country_code;
	uint16_t def_region_domain;
	enum country_src cc_src;
	struct wlan_objmgr_psoc *psoc_ptr;
	bool new_user_ctry_pending[PSOC_MAX_PHY_REG_CAP];
	bool new_init_ctry_pending[PSOC_MAX_PHY_REG_CAP];
	bool new_11d_ctry_pending[PSOC_MAX_PHY_REG_CAP];
	bool world_country_pending[PSOC_MAX_PHY_REG_CAP];
	bool dfs_enabled;
	enum band_info band_capability;
	bool indoor_chan_enabled;
	bool ignore_fw_reg_offload_ind;
	bool enable_11d_supp_original;
	bool enable_11d_supp;
	bool is_11d_offloaded;
	uint8_t vdev_id_for_11d_scan;
	uint8_t master_vdev_cnt;
	uint8_t vdev_cnt_11d;
	uint32_t scan_11d_interval;
	uint8_t vdev_ids_11d[MAX_STA_VDEV_CNT];
	bool user_ctry_priority;
	bool user_ctry_set;
	struct chan_change_cbk_entry cbk_list[REG_MAX_CHAN_CHANGE_CBKS];
	uint8_t num_chan_change_cbks;
	uint8_t ch_avoid_ind;
	struct unsafe_ch_list unsafe_chan_list;
	struct ch_avoid_ind_type avoid_freq_list;
	enum restart_beaconing_on_ch_avoid_rule restart_beaconing;
	struct wlan_psoc_host_hal_reg_capabilities_ext
			reg_cap[PSOC_MAX_PHY_REG_CAP];
	bool force_ssc_disable_indoor_channel;
	uint8_t enable_srd_chan_in_master_mode;
	bool enable_11d_in_world_mode;
	int8_t def_pdev_id;
	qdf_spinlock_t cbk_list_lock;
};

struct wlan_regulatory_pdev_priv_obj {
	struct regulatory_channel cur_chan_list[NUM_CHANNELS];
	struct regulatory_channel mas_chan_list[NUM_CHANNELS];
#ifdef DISABLE_CHANNEL_LIST
	struct regulatory_channel cache_disable_chan_list[NUM_CHANNELS];
	uint32_t num_cache_channels;
	bool disable_cached_channels;
#endif
	char default_country[REG_ALPHA2_LEN + 1];
	uint16_t def_region_domain;
	uint16_t def_country_code;
	char current_country[REG_ALPHA2_LEN + 1];
	uint16_t reg_dmn_pair;
	uint16_t ctry_code;
	enum dfs_reg dfs_region;
	uint32_t phybitmap;
	struct wlan_objmgr_pdev *pdev_ptr;
	uint32_t range_2g_low;
	uint32_t range_2g_high;
	uint32_t range_5g_low;
	uint32_t range_5g_high;
	bool dfs_enabled;
	bool set_fcc_channel;
	enum band_info band_capability;
	bool indoor_chan_enabled;
	bool en_chan_144;
	uint32_t wireless_modes;
	struct ch_avoid_ind_type freq_avoid_list;
	bool force_ssc_disable_indoor_channel;
	bool sap_state;
	struct reg_rule_info reg_rules;
	qdf_spinlock_t reg_rules_lock;
};

#endif
