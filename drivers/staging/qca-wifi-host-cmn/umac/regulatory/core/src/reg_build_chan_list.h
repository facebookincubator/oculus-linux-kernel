/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: reg_build_chan_list.h
 * This file provides prototypes of the regulatory component to build master
 * and current channel list.
 */

#ifndef __REG_BUILD_CHAN_LIST_H__
#define __REG_BUILD_CHAN_LIST_H__

#define CHAN_12_CENT_FREQ 2467
#define CHAN_13_CENT_FREQ 2472

#ifdef WLAN_FEATURE_11BE
#define REG_MAX_20M_SUB_CH 16
#else
#define REG_MAX_20M_SUB_CH  8
#endif

#ifdef CONFIG_AFC_SUPPORT
#define MIN_AFC_BW 2
#ifdef WLAN_FEATURE_11BE
#define MAX_AFC_BW 320
#else
#define MAX_AFC_BW 160
#endif
#endif

#define HALF_IEEE_CH_SEP  2
#define IEEE_20MHZ_CH_SEP 4

#include "reg_priv_objs.h"
/**
 * reg_reset_reg_rules() - provides the reg domain rules info
 * @reg_rules: reg rules pointer
 *
 * Return: None
 */
void reg_reset_reg_rules(struct reg_rule_info *reg_rules);

/**
 * reg_init_pdev_mas_chan_list() - Initialize pdev master channel list
 * @pdev_priv_obj: Pointer to regdb pdev private object.
 * @mas_chan_params: Master channel params.
 */
void reg_init_pdev_mas_chan_list(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
		struct mas_chan_params *mas_chan_params);

/**
 * reg_save_reg_rules_to_pdev() - Save psoc reg-rules to pdev.
 * @pdev_priv_obj: Pointer to regdb pdev private object.
 */
void reg_save_reg_rules_to_pdev(
		struct reg_rule_info *psoc_reg_rules,
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj);

/**
 * reg_compute_pdev_current_chan_list() - Compute pdev current channel list.
 * @pdev_priv_obj: Pointer to regdb pdev private object.
 */
void reg_compute_pdev_current_chan_list(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj);

/**
 * reg_propagate_mas_chan_list_to_pdev() - Propagate master channel list to pdev
 * @psoc: Pointer to psoc object.
 * @object: Void pointer to pdev object.
 * @arg: Pointer to direction.
 */
void reg_propagate_mas_chan_list_to_pdev(struct wlan_objmgr_psoc *psoc,
					 void *object, void *arg);

#ifdef CONFIG_BAND_6GHZ
/**
 * reg_process_master_chan_list_ext() - Compute master channel extended list
 * based on the regulatory rules.
 * @reg_info: Pointer to regulatory info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
reg_process_master_chan_list_ext(struct cur_regulatory_info *reg_info);

/**
 * reg_get_6g_ap_master_chan_list() - Get  an ap  master channel list depending
 * on * ap power type
 * @ap_pwr_type: Power type (LPI/VLP/SP)
 * @chan_list: Pointer to the channel list. The output channel list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_get_6g_ap_master_chan_list(struct wlan_objmgr_pdev *pdev,
					  enum reg_6g_ap_type ap_pwr_type,
					  struct regulatory_channel *chan_list);

/**
 * reg_get_reg_maschan_lst_frm_6g_pwr_mode() - Return the mas_chan_list entry
 * for based on the channel index and input power mode
 * @supp_pwr_mode: 6G supported power mode
 * @pdev_priv_obj: Pointer to pdev_priv_obj
 * @chan_idx: Channel index
 *
 * Return: Pointer to struct regulatory_channel
 */
struct regulatory_channel *reg_get_reg_maschan_lst_frm_6g_pwr_mode(
			enum supported_6g_pwr_types supp_pwr_mode,
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			uint16_t chan_idx);

/**
 * reg_convert_supported_6g_pwr_type_to_ap_pwr_type() - The supported 6G power
 * type is a combination of AP and client power types. This API return the 6G AP
 * power type portion of the supported 6G power type.
 * @in_6g_pwr_type: input 6G supported power type.
 *
 * Return: 6G AP power type.
 */
enum reg_6g_ap_type
reg_convert_supported_6g_pwr_type_to_ap_pwr_type(enum supported_6g_pwr_types
						in_6g_pwr_type);

#ifdef CONFIG_REG_CLIENT
/**
 * reg_get_power_string() - get power string from power enum type
 * @power_type: power type enum value
 *
 * Return: power type string
 */
const char *reg_get_power_string(enum reg_6g_ap_type power_type);
#endif

#ifdef CONFIG_AFC_SUPPORT
/**
 * reg_process_afc_event() - Process the afc event and compute the 6G AFC
 * channel list based on the frequency range and channel frequency indices set.
 * @reg_info: Pointer to regulatory info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
reg_process_afc_event(struct afc_regulatory_info *afc_info);

/**
 * reg_get_subchannels_for_opclass() - Get the list of subchannels based on the
 * the channel frequency index and opclass.
 * @cfi: Channel frequency index
 * @opclass: Operating class
 * @subchannels: Pointer to list of subchannels
 *
 * Return: void
 */
uint8_t reg_get_subchannels_for_opclass(uint8_t cfi,
					uint8_t opclass,
					uint8_t *subchannels);
#endif

/**
 * reg_psd_2_eirp() - Calculate EIRP from PSD and bandwidth
 * channel list
 * @pdev: pdev pointer
 * @psd: Power Spectral Density in dBm/MHz
 * @ch_bw: Bandwdith of a channel in MHz (20/40/80/160/320 etc)
 * @eirp:  EIRP power  in dBm
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_psd_2_eirp(struct wlan_objmgr_pdev *pdev,
			  int16_t psd,
			  uint16_t ch_bw,
			  int16_t *eirp);

/**
 * reg_eirp_2_psd() - Calculate PSD from EIRP and bandwidth
 * channel list
 * @pdev: pdev pointer
 * @ch_bw: Bandwdith of a channel in MHz (20/40/80/160/320 etc)
 * @eirp:  EIRP power  in dBm
 * @psd: Power Spectral Density in dBm/MHz
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_eirp_2_psd(struct wlan_objmgr_pdev *pdev,
			  uint16_t ch_bw,
			  int16_t eirp,
			  int16_t *psd);

/**
 * reg_is_supp_pwr_mode_invalid - Indicates if the given 6G power mode is
 * one of the valid power modes enumerated by enum supported_6g_pwr_types
 * from REG_AP_LPI to REG_CLI_SUB_VLP.
 *
 * Note: REG_BEST_PWR_MODE and REG_CURRENT_PWR_MODE are not valid 6G power
 * modes.
 *
 * Return: True for any valid power mode from REG_AP_LPI tp REG_CLI_SUB_VLP.
 * False otherwise.
 */
static inline bool
reg_is_supp_pwr_mode_invalid(enum supported_6g_pwr_types supp_pwr_mode)
{
	return (supp_pwr_mode < REG_AP_LPI || supp_pwr_mode > REG_CLI_SUB_VLP);
}

/**
 * reg_copy_from_super_chan_info_to_reg_channel - Copy the structure fields from
 * a super channel entry to the regulatory channel fields.
 *
 * chan - Pointer to the regulatory channel where the fields of super channel
 * entry is copied to.
 * sc_entry - Input super channel entry whose fields are copied to the
 * regulatory channel structure.
 * in_6g_pwr_mode - Input 6g power type. If the power type is best power mode,
 * get the best power mode of the given super channel entry and copy its
 * information to the regulatory channel fields.
 */
void
reg_copy_from_super_chan_info_to_reg_channel(struct regulatory_channel *chan,
					     const struct super_chan_info sc_entry,
					     enum supported_6g_pwr_types
					     in_6g_pwr_mode);

/**
 * reg_set_ap_pwr_type() - Set the AP power type.
 * @pdev_priv_obj: pdev private object
 *
 * Set the AP power type as per AFC device deployment if AFC is available.
 * Otherwise set it to indoor by default.
 *
 * Return: None
 */
void reg_set_ap_pwr_type(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj);

#else /* CONFIG_BAND_6GHZ */
static inline QDF_STATUS
reg_get_6g_ap_master_chan_list(struct wlan_objmgr_pdev *pdev,
			       enum reg_6g_ap_type ap_pwr_type,
			       struct regulatory_channel *chan_list)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
struct regulatory_channel *reg_get_reg_maschan_lst_frm_6g_pwr_mode(
			enum supported_6g_pwr_types supp_pwr_mode,
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			uint16_t chan_idx)
{
	return NULL;
}

static inline uint8_t
reg_get_subchannels_for_opclass(uint8_t cfi,
				uint8_t opclass,
				uint8_t *subchannels)
{
	return 0;
}

static inline QDF_STATUS reg_psd_2_eirp(struct wlan_objmgr_pdev *pdev,
					int16_t psd,
					uint16_t ch_bw,
					int16_t *eirp)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS reg_eirp_2_psd(struct wlan_objmgr_pdev *pdev,
					uint16_t ch_bw,
					int16_t eirp,
					int16_t *psd)
{
	return QDF_STATUS_E_FAILURE;
}

static inline bool
reg_is_supp_pwr_mode_invalid(enum supported_6g_pwr_types supp_pwr_mode)
{
	return true;
}

static inline void
reg_copy_from_super_chan_info_to_reg_channel(struct regulatory_channel *chan,
					     const struct super_chan_info sc_entry,
					     enum supported_6g_pwr_types
					     in_6g_pwr_mode)
{
}

static inline void
reg_set_ap_pwr_type(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}
#endif /* CONFIG_BAND_6GHZ */
/**
 * reg_process_master_chan_list() - Compute master channel list based on the
 * regulatory rules.
 * @reg_info: Pointer to regulatory info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_process_master_chan_list(struct cur_regulatory_info *reg_info);

/**
 * reg_get_pwrmode_chan_list() - Get the modified channel list. A modified
 * current channel list consists of 2G and 5G portions of the current channel
 * list and the 6G portion of the current channel list is derived from the input
 * 6g power type.
 * @pdev: Pointer to pdev
 * @in_6g_pwr_mode: Input 6GHz power mode.
 *
 * Return:
 * QDF_STATUS_SUCCESS: Success
 * QDF_STATUS_E_INVAL: Failed to get channel list
 */
QDF_STATUS reg_get_pwrmode_chan_list(struct wlan_objmgr_pdev *pdev,
				     struct regulatory_channel *chan_list,
				     enum supported_6g_pwr_types
				     in_6g_pwr_mode);

/**
 * reg_get_current_chan_list() - provide the pdev current channel list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
				     struct regulatory_channel *chan_list);

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
/**
 * reg_get_6g_afc_chan_list() - provide the pdev afc channel list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS reg_get_6g_afc_chan_list(struct wlan_objmgr_pdev *pdev,
				    struct regulatory_channel *chan_list);

/**
 * reg_get_6g_afc_mas_chan_list() - provide the pdev afc master channel list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
reg_get_6g_afc_mas_chan_list(struct wlan_objmgr_pdev *pdev,
			     struct regulatory_channel *chan_list);
#endif

#ifdef CONFIG_REG_CLIENT
/**
 * reg_get_secondary_current_chan_list() - provide the pdev secondary current
 * channel list
 * @pdev: pdev pointer
 * @chan_list: channel list pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
reg_get_secondary_current_chan_list(struct wlan_objmgr_pdev *pdev,
				    struct regulatory_channel *chan_list);
#endif

/**
 * reg_is_chan_disabled_and_not_nol() - In the regulatory channel list, a
 * channel may be disabled by the regulatory/device or by radar. Radar is
 * temporary and a radar disabled channel does not mean that the channel is
 * permanently disabled. The API checks if the channel is disabled, but not due
 * to radar.
 * @chan - Regulatory channel object
 *
 * Return - True,  the channel is disabled, but not due to radar, else false.
 */
bool reg_is_chan_disabled_and_not_nol(struct regulatory_channel *chan);
#endif
