/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: wlan_cm_roam_ucfg_api.h
 *
 * Implementation for roaming public ucfg API interfaces.
 */

#ifndef _WLAN_CM_ROAM_UCFG_API_H_
#define _WLAN_CM_ROAM_UCFG_API_H_

#include "wlan_cm_roam_api.h"

/**
 * ucfg_user_space_enable_disable_rso() - Enable/Disable Roam Scan offload
 * to firmware.
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @is_fast_roam_enabled: Value provided by userspace.
 * is_fast_roam_enabled - true: enable RSO if FastRoamEnabled ini is enabled
 *                        false: disable RSO
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_user_space_enable_disable_rso(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id,
				   const bool is_fast_roam_enabled);

/**
 * ucfg_clear_user_disabled_roaming() - clear user/wpa_supplicant
 * disabled_roaming flag in driver
 * @psoc: Pointer to pdev
 * @vdev_id: vdev id
 *
 * Return: void
 */
void
ucfg_clear_user_disabled_roaming(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id);

/**
 * ucfg_is_rso_enabled() - Check if rso is enabled
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 *
 * Return: Wrapper for wlan_is_rso_enabled.
 */
bool ucfg_is_rso_enabled(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id);

/*
 * ucfg_cm_abort_roam_scan() -abort current roam scan cycle by roam scan
 * offload module.
 * @pdev: Pointer to pdev
 * vdev_id - vdev Identifier
 *
 * Return QDF_STATUS
 */
QDF_STATUS ucfg_cm_abort_roam_scan(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id);

#ifdef FEATURE_WLAN_ESE
/**
 * ucfg_cm_set_ese_roam_scan_channel_list() - To set ese roam scan channel list
 * @pdev: pdev pointer
 * @vdev_id: vdev_id id
 * @chan_freq_list: Output channel list
 * @num_chan: Output number of channels
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_cm_set_ese_roam_scan_channel_list(struct wlan_objmgr_pdev *pdev,
						  uint8_t vdev_id,
						  qdf_freq_t *chan_freq_list,
						  uint8_t num_chan);
QDF_STATUS ucfg_cm_set_cckm_ie(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			       const uint8_t *cck_ie, const uint8_t cck_ie_len);
#endif

/**
 * ucfg_cm_get_roam_band() - Get roam band from rso config
 * @psoc: Pointer to psoc
 * @vdev_id: vdev id
 * @roam_band: Pointer of a buffer to fill the roam band
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_cm_get_roam_band(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 uint32_t *roam_band);

/**
 * ucfg_cm_rso_set_roam_trigger() - Send roam trigger bitmap firmware
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @trigger: Carries pointer of the object containing vdev id and
 *  roam_trigger_bitmap.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_cm_rso_set_roam_trigger(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			     struct wlan_roam_triggers *trigger)
{
	return wlan_cm_rso_set_roam_trigger(pdev, vdev_id, trigger);
}

/**
 * ucfg_cm_update_session_assoc_ie() - Send assoc ie
 * @psoc: Pointer to psoc
 * @vdev_id: vdev id
 * @assoc_ie: assoc ir to update.
 *
 * Return: QDF_STATUS
 */
static inline void
ucfg_cm_update_session_assoc_ie(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				struct element_info *assoc_ie)
{
	cm_update_session_assoc_ie(psoc, vdev_id, assoc_ie);
}

static inline void
ucfg_cm_get_associated_ch_info(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			       enum phy_ch_width scanned_ch_width,
			       struct assoc_channel_info *assoc_chan_info)
{
	wlan_cm_get_associated_ch_info(psoc, vdev_id, scanned_ch_width,
				       assoc_chan_info);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
/**
 * ucfg_cm_roam_link_speed_update() - Update link speed state for roaming
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @is_link_speed_good: true means link speed good, false means bad
 *
 * Return: QDF_STATUS
 */
static inline
void ucfg_cm_roam_link_speed_update(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    bool is_link_speed_good)
{
	wlan_cm_roam_link_speed_update(psoc,
				       vdev_id,
				       is_link_speed_good);
}

/**
 * ucfg_cm_is_linkspeed_roam_trigger_supported() - Get roam linkspeed check
 * @psoc: pointer to psoc object
 *
 * Return: bool, true: Linkspeed check for low rssi roaming supported
 */
static inline bool
ucfg_cm_is_linkspeed_roam_trigger_supported(struct wlan_objmgr_psoc *psoc)
{
	return wlan_cm_is_linkspeed_roam_trigger_supported(psoc);
}
#endif

static inline QDF_STATUS
ucfg_cm_update_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id,
				       uint32_t roam_scan_scheme_bitmap)
{
	return wlan_cm_update_roam_scan_scheme_bitmap(psoc, vdev_id,
						      roam_scan_scheme_bitmap);
}

static inline QDF_STATUS
ucfg_cm_set_roam_band_mask(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   uint32_t roam_band_mask)
{
	return wlan_cm_set_roam_band_bitmask(psoc, vdev_id, roam_band_mask);
}

/**
 * ucfg_cm_set_btm_config() - Inline ucfg api to set btm roaming disable flag
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id
 * @is_disable_btm: btm config flag that needs to be set from the caller
 *
 * Return: QDF Status
 */
static inline QDF_STATUS ucfg_cm_set_btm_config(struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id,
						bool is_disable_btm)
{
	return wlan_cm_set_btm_config(psoc, vdev_id, is_disable_btm);
}

static inline QDF_STATUS
ucfg_cm_set_roam_band_update(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	return wlan_cm_set_roam_band_update(psoc, vdev_id);
}

static inline bool
ucfg_cm_is_change_in_band_allowed(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id, uint32_t roam_band_mask)
{
	return cm_roam_is_change_in_band_allowed(psoc, vdev_id, roam_band_mask);
}

static inline QDF_STATUS
ucfg_cm_update_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
			     uint8_t value, enum roam_rt_stats_params stats)
{
	return wlan_cm_update_roam_rt_stats(psoc, value, stats);
}

static inline uint8_t
ucfg_cm_get_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
			  enum roam_rt_stats_params stats)
{
	return wlan_cm_get_roam_rt_stats(psoc, stats);
}
#else
static inline QDF_STATUS
ucfg_cm_update_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id,
				       uint32_t roam_scan_scheme_bitmap)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_cm_set_roam_band_mask(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   uint32_t roam_band_mask)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_cm_set_btm_config(struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id,
						bool is_disable_btm)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_cm_set_roam_band_update(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool
ucfg_cm_is_change_in_band_allowed(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id, uint32_t roam_band_mask)
{
	return true;
}

static inline QDF_STATUS
ucfg_cm_update_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
			     uint8_t value, enum roam_rt_stats_params stats)
{
	return QDF_STATUS_SUCCESS;
}

static inline uint8_t
ucfg_cm_get_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
			  enum roam_rt_stats_params stats)
{
	return 0;
}

#endif

/**
 * ucfg_wlan_cm_roam_invoke() - Invokes Roam request
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @bssid: Pointer to bssid to look for in scan cache
 * @ch_freq: channel on which reassoc should be send
 * @source: source of roam
 *
 * Return: true or false
 */
QDF_STATUS
ucfg_wlan_cm_roam_invoke(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			 struct qdf_mac_addr *bssid, qdf_freq_t ch_freq,
			 enum wlan_cm_source source);

#ifdef WLAN_FEATURE_FILS_SK
QDF_STATUS
ucfg_cm_update_fils_config(struct wlan_objmgr_psoc *psoc,
			   uint8_t vdev_id,
			   struct wlan_fils_con_info *fils_info);
#else
static inline QDF_STATUS
ucfg_cm_update_fils_config(struct wlan_objmgr_psoc *psoc,
			   uint8_t vdev_id,
			   struct wlan_fils_con_info *fils_info)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_HOST_ROAM
void ucfg_cm_ft_reset(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_cm_set_ft_ies() - to set FT IEs
 * @pdev: pdev ctx
 * @vdev_id: vdev identifier
 * @ft_ies: pointer to FT IEs
 * @ft_ies_length: length of FT IEs
 *
 * Each time the supplicant sends down the FT IEs to the driver. This function
 * is called in SME. This function packages and sends the FT IEs to PE.
 *
 * Return: none
 */
void ucfg_cm_set_ft_ies(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			const uint8_t *ft_ies, uint16_t ft_ies_length);

/**
 * ucfg_cm_check_ft_status() - Check for key wait status in FT mode
 * @pdev: pdev ctx
 * @vdev_id: vdev identifier
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_cm_check_ft_status(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id);

/**
 * ucfg_cm_ft_key_ready_for_install() - API to check ft key ready for install
 * @vdev: pdev handle
 *
 * It is only applicable for LFR2.0 enabled
 *
 * Return: true when ft key is ready otherwise false
 */
bool ucfg_cm_ft_key_ready_for_install(struct wlan_objmgr_vdev *vdev);
void ucfg_cm_set_ft_pre_auth_state(struct wlan_objmgr_vdev *vdev, bool state);

#else /* WLAN_FEATURE_HOST_ROAM */

static inline void ucfg_cm_ft_reset(struct wlan_objmgr_vdev *vdev) {}
static inline
void ucfg_cm_set_ft_ies(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			const uint8_t *ft_ies, uint16_t ft_ies_length) {}

static inline
QDF_STATUS ucfg_cm_check_ft_status(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id)
{
	return QDF_STATUS_E_FAILURE;
}

static inline void ucfg_cm_set_ft_pre_auth_state(struct wlan_objmgr_vdev *vdev,
						 bool state) {}
#endif /* WLAN_FEATURE_HOST_ROAM */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * ucfg_cm_reset_key() -Reset key information
 * @pdev: pdev handle
 * @vdev_id: vdev identifier
 *
 * Return: None
 */
void ucfg_cm_reset_key(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id);

/**
 * ucfg_cm_roam_send_rt_stats_config() - Enable/Disable Roam event stats from FW
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @param_value: Value set based on the userspace attributes.
 * param_value - 0: if configure attribute is 0
 *               1: if configure is 1 and suspend_state is not set
 *               3: if configure is 1 and suspend_state is set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_roam_send_rt_stats_config(struct wlan_objmgr_pdev *pdev,
				  uint8_t vdev_id, uint8_t param_value);

/**
 * ucfg_cm_roam_send_ho_delay_config() - Send the HO delay value to Firmware
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @param_value: Value will be from range 20 to 1000 in msec.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_roam_send_ho_delay_config(struct wlan_objmgr_pdev *pdev,
				  uint8_t vdev_id, uint16_t param_value);

/**
 * ucfg_cm_exclude_rm_partial_scan_freq() - Exclude the channels in roam full
 * scan that are already scanned as part of partial scan.
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @param_value: Include/exclude the partial scan channel in roam full scan
 * 1 - Exclude
 * 0 - Include
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_exclude_rm_partial_scan_freq(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id, uint8_t param_value);

/**
 * ucfg_cm_roam_full_scan_6ghz_on_disc() - Include the 6 GHz channels in roam
 * full scan only on prior discovery of any 6 GHz support in the environment.
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @param_value: Include the 6 GHz channels in roam full scan:
 * 1 - Include only on prior discovery of any 6 GHz support in the environment
 * 0 - Include all the supported 6 GHz channels by default
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_cm_roam_full_scan_6ghz_on_disc(struct wlan_objmgr_pdev *pdev,
					       uint8_t vdev_id,
					       uint8_t param_value);

/**
 * ucfg_cm_set_roam_scan_high_rssi_offset() - Set the delta change in high RSSI
 * at which roam scan is triggered in 2.4/5 GHz.
 * @psoc: Pointer to psoc object
 * @vdev_id: vdev id
 * @param_value: Set the High RSSI delta for roam scan trigger
 * 0    - Disable
 * 1-16 - Set an offset value in this range
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_cm_set_roam_scan_high_rssi_offset(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id, uint8_t param_value)
{
	return cm_set_roam_scan_high_rssi_offset(psoc, vdev_id, param_value);
}
#else
static inline void
ucfg_cm_reset_key(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id) {}

static inline QDF_STATUS
ucfg_cm_roam_send_rt_stats_config(struct wlan_objmgr_pdev *pdev,
				  uint8_t vdev_id, uint8_t param_value)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_cm_roam_send_ho_delay_config(struct wlan_objmgr_pdev *pdev,
				  uint8_t vdev_id, uint16_t param_value)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_cm_exclude_rm_partial_scan_freq(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id, uint8_t param_value)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_cm_roam_full_scan_6ghz_on_disc(struct wlan_objmgr_pdev *pdev,
				    uint8_t vdev_id, uint8_t param_value)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_cm_set_roam_scan_high_rssi_offset(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id, uint8_t param_value)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
/**
 * ucfg_cm_roam_send_vendor_handoff_param_req() - send vendor handoff params
 * command request to FW
 * @psoc: Pointer to psoc
 * @vdev_id: vdev id
 * @param_id: Vendor Control Param ID from
 * enum WMI_ROAM_GET_VENDOR_CONTROL_PARAM_ID
 * @vendor_handoff_context:
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_roam_send_vendor_handoff_param_req(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id, uint32_t param_id,
					   void *vendor_handoff_context);

/**
 * ucfg_cm_roam_is_vendor_handoff_control_enable() - check whether vendor
 * handoff control feature is enable or not in driver
 * @psoc: psoc pointer
 *
 * Return: true if feature supports
 */
bool
ucfg_cm_roam_is_vendor_handoff_control_enable(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
ucfg_cm_roam_is_vendor_handoff_control_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * ucfg_cm_get_sae_auth_ta() - Get SAE auth tx address
 * @pdev: pointer to pdev object
 * @vdev_id: Vdev id
 * @sae_auth_ta: SAE auth tx address
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_cm_get_sae_auth_ta(struct wlan_objmgr_pdev *pdev,
			uint8_t vdev_id,
			struct qdf_mac_addr *sae_auth_ta)
{
	return wlan_cm_get_sae_auth_ta(pdev, vdev_id, sae_auth_ta);
}

/**
 * ucfg_cm_get_roam_intra_band() - get Intra band roaming
 * @psoc: pointer to psoc object
 * @val:  Infra band value
 *
 * Return: Success or failure
 */
QDF_STATUS
ucfg_cm_get_roam_intra_band(struct wlan_objmgr_psoc *psoc, uint16_t *val);

/**
 * ucfg_cm_get_roam_rescan_rssi_diff() - gets roam rescan rssi diff
 * @psoc: pointer to psoc object
 * @val: value for rescan rssi diff
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_roam_rescan_rssi_diff(struct wlan_objmgr_psoc *psoc, uint8_t *val);

/**
 * ucfg_cm_get_neighbor_lookup_rssi_threshold() -
 * get neighbor lookup rssi threshold
 * @psoc: pointer to psoc object
 * @vdev_id: vdev identifier
 * @lookup_threshold: Buffer to fill the neighbor lookup threshold.
 *			Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_neighbor_lookup_rssi_threshold(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id,
					   uint8_t *lookup_threshold);

/**
 * ucfg_cm_get_empty_scan_refresh_period() - get empty scan refresh period
 * @psoc: pointer to psoc object
 * @vdev_id: Vdev id
 * @refresh_threshold: Buffer to fill the empty scan refresh period.
 *			Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_empty_scan_refresh_period(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id,
				      uint16_t *refresh_threshold);

/**
 * ucfg_cm_get_neighbor_scan_min_chan_time() -
 * get neighbor scan min channel time
 * @psoc: pointer to psoc object
 * @vdev_id: vdev_id
 *
 * Return: channel min time value
 */
uint16_t
ucfg_cm_get_neighbor_scan_min_chan_time(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id);

/**
 * ucfg_cm_get_roam_rssi_diff() - Get Roam rssi diff
 * @psoc: pointer to psoc object
 * @vdev_id: vdev identifier
 * @rssi_diff: Buffer to fill the roam RSSI diff.
 *		Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_roam_rssi_diff(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   uint8_t *rssi_diff);

#ifdef FEATURE_WLAN_ESE
/**
 * ucfg_cm_get_is_ese_feature_enabled() - Get ESE feature enabled or not
 * This is a synchronous call
 * @psoc: pointer to psoc object
 *
 * Return: true (1) - if the ESE feature is enabled
 *	false (0) - if feature is disabled (compile or runtime)
 */
bool
ucfg_cm_get_is_ese_feature_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
ucfg_cm_get_is_ese_feature_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * ucfg_cm_get_neighbor_scan_max_chan_time() - get neighbor
 * scan max channel time
 * @psoc: pointer to psoc object
 * @vdev_id: vdev identifier
 *
 * Return: channel max time value
 */
uint16_t
ucfg_cm_get_neighbor_scan_max_chan_time(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id);

/**
 * ucfg_cm_get_neighbor_scan_period() - get neighbor scan period
 * @psoc: pointer to psoc object
 * @vdev_id: vdev identifier
 *
 * Return: neighbor scan period
 */
uint16_t
ucfg_cm_get_neighbor_scan_period(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id);

/**
 * ucfg_cm_get_wes_mode() - Get WES Mode
 * This is a synchronous call
 * @psoc: pointer to psoc object
 *
 * Return: WES Mode Enabled(1)/Disabled(0)
 */
bool ucfg_cm_get_wes_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_cm_get_is_lfr_feature_enabled() - Get LFR feature enabled or not
 * This is a synchronous call
 * @psoc: pointer to psoc object
 *
 * Return: true (1) - if the feature is enabled
 *	false (0) - if feature is disabled (compile or runtime)
 */
bool ucfg_cm_get_is_lfr_feature_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_cm_get_is_ft_feature_enabled() - Get FT feature enabled or not
 * This is a synchronous call
 * @psoc: pointer to psoc object
 *
 * Return: true (1) - if the feature is enabled
 *	false (0) - if feature is disabled (compile or runtime)
 */
bool ucfg_cm_get_is_ft_feature_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_cm_get_roam_scan_home_away_time() - Get Roam scan home away time
 * @psoc: Pointer to psoc
 * @vdev_id: vdev identifier
 * @roam_scan_home_away_time: Buffer to fill the roam scan home away time.
 *				Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_roam_scan_home_away_time(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     uint16_t *roam_scan_home_away_time);
/**
 * ucfg_cm_get_roam_opportunistic_scan_threshold_diff() -
 * get Opportunistic Scan threshold diff.
 * This is a synchronous call
 * @psoc: Pointer to psoc
 * @val: Opportunistic Scan threshold diff.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_roam_opportunistic_scan_threshold_diff(
					struct wlan_objmgr_psoc *psoc,
					int8_t *val);

/**
 * ucfg_cm_get_neighbor_scan_refresh_period() - Get neighbor scan results
 * refresh period.
 * This is a synchronous call
 * @psoc: pointer to psoc object
 * @value: value for scan results refresh period
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_neighbor_scan_refresh_period(struct wlan_objmgr_psoc *psoc,
					 uint16_t *value);

/**
 * ucfg_cm_get_empty_scan_refresh_period_global() - Get global scan
 * refresh period
 * @psoc: pointer to psoc object
 * @roam_scan_period_global: value for empty scan refresh period
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_cm_get_empty_scan_refresh_period_global(struct wlan_objmgr_psoc *psoc,
					     uint16_t *roam_scan_period_global);

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_ROAM_INFO_STATS)
/**
 * ucfg_cm_roam_stats_info_get() - get vdev roam stats info
 *
 * @vdev: pointer to vdev
 * @roam_info: pointer to buffer to copy roam stats info
 * @roam_num: pointer to valid roam stats num
 *
 * After use, roam_info must be released by using
 * ucfg_cm_roam_stats_info_put()
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_cm_roam_stats_info_get(struct wlan_objmgr_vdev *vdev,
			    struct enhance_roam_info **roam_info,
			    uint32_t *roam_num)
{
	return wlan_cm_roam_stats_info_get(vdev, roam_info, roam_num);
}

/**
 * ucfg_cm_roam_stats_info_put() - put vdev roam stats info
 *
 * @roam_info: pointer to buffer of roam stats info
 *
 * Return: QDF_STATUS
 */
static inline void
ucfg_cm_roam_stats_info_put(struct enhance_roam_info *roam_info)
{
	qdf_mem_free(roam_info);
}
#else
static inline QDF_STATUS
ucfg_cm_roam_stats_info_get(struct wlan_objmgr_vdev *vdev,
			    struct enhance_roam_info **roam_info,
			    uint32_t *roam_num)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
ucfg_cm_roam_stats_info_put(struct enhance_roam_info *roam_info)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * ucfg_cm_is_sae_auth_addr_conversion_required() - this api is wrapper for
 * "wlan_cm_is_sae_auth_addr_conversion_required" function
 * @vdev: pointer to vdev
 *
 * Return: true for address conversion otherwise false
 */
static inline bool
ucfg_cm_is_sae_auth_addr_conversion_required(struct wlan_objmgr_vdev *vdev)
{
	return wlan_cm_is_sae_auth_addr_conversion_required(vdev);
}
#else
static inline bool
ucfg_cm_is_sae_auth_addr_conversion_required(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_11BE_MLO)
/**
 * ucfg_cm_roaming_get_peer_mld_addr() - this api is wrapper for
 * "wlan_cm_roaming_get_peer_mld_addr" function.
 * @vdev: pointer to vdev
 *
 * Return: mld address of peer
 */
static inline struct qdf_mac_addr *
ucfg_cm_roaming_get_peer_mld_addr(struct wlan_objmgr_vdev *vdev)
{
	return wlan_cm_roaming_get_peer_mld_addr(vdev);
}
#else
static inline struct qdf_mac_addr *
ucfg_cm_roaming_get_peer_mld_addr(struct wlan_objmgr_vdev *vdev)
{
	return NULL;
}
#endif
#endif /* _WLAN_CM_ROAM_UCFG_API_H_ */
