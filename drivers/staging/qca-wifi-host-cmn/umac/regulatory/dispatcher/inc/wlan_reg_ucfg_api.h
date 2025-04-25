/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_reg_ucfg_api.h
 * This file provides prototypes of the regulatory component user
 * config interface routines
 */

#ifndef __WLAN_REG_UCFG_API_H
#define __WLAN_REG_UCFG_API_H

#ifdef CONFIG_AFC_SUPPORT
#include <wlan_reg_afc.h>
#endif
#include <reg_services_public_struct.h>

typedef QDF_STATUS (*reg_event_cb)(void *status_struct);

/**
 * ucfg_reg_set_band() - Sets the band information for the PDEV
 * @pdev: The physical pdev to set the band for
 * @band_bitmap: The band bitmap parameter (over reg_wifi_band) to configure
 *	for the physical device
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_band(struct wlan_objmgr_pdev *pdev,
			     uint32_t band_bitmap);

/**
 * ucfg_reg_get_band() - Gets the band information for the PDEV
 * @pdev: The physical pdev to get the band for
 * @band_bitmap: The band parameter of the physical device
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_get_band(struct wlan_objmgr_pdev *pdev,
			     uint32_t *band_bitmap);

/**
 * ucfg_reg_notify_sap_event() - Notify regulatory domain for sap event
 * @pdev: The physical dev to notify
 * @sap_state: true for sap start else false
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_notify_sap_event(struct wlan_objmgr_pdev *pdev,
				     bool sap_state);

/**
 * ucfg_reg_cache_channel_freq_state() - Cache the current state of the
 * channels based on the channel center frequency.
 * @pdev: Pointer to pdev.
 * @channel_list: List of the channels for which states need to be cached.
 * @num_channels: Number of channels in the list.
 *
 * Return: QDF_STATUS
 */
#if defined(DISABLE_CHANNEL_LIST) && defined(CONFIG_CHAN_FREQ_API)
void ucfg_reg_cache_channel_freq_state(struct wlan_objmgr_pdev *pdev,
				       uint32_t *channel_list,
				       uint32_t num_channels);
#else
static inline
void ucfg_reg_cache_channel_freq_state(struct wlan_objmgr_pdev *pdev,
				       uint32_t *channel_list,
				       uint32_t num_channels)
{
}
#endif /* CONFIG_CHAN_FREQ_API */

#ifdef DISABLE_CHANNEL_LIST
/**
 * ucfg_reg_disable_cached_channels() - Disable cached channels
 * @pdev: The physical dev to cache the channels for
 *
 * Return: Void
 */
void ucfg_reg_disable_cached_channels(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_reg_restore_cached_channels() - Restore disabled cached channels
 * @pdev: The physical dev to cache the channels for
 *
 * Return: Void
 */
void ucfg_reg_restore_cached_channels(struct wlan_objmgr_pdev *pdev);
#else
static inline
void ucfg_reg_disable_cached_channels(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_reg_restore_cached_channels(struct wlan_objmgr_pdev *pdev)
{
}
#endif

/**
 * ucfg_reg_get_keep_6ghz_sta_cli_connection() - Get keep 6ghz sta cli
 *                                               connection flag
 * @pdev: The physical pdev to get keep_6ghz_sta_cli_connection
 *
 * Return: Return true if keep 6ghz sta cli connection set else return flase
 */
bool ucfg_reg_get_keep_6ghz_sta_cli_connection(
					struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_reg_set_keep_6ghz_sta_cli_connection() - Set keep 6ghz sta cli
 *                                               connection flag
 * @pdev: The physical pdev to get keep_6ghz_sta_cli_connection
 * @keep_6ghz_sta_cli_connection: Parameter to set
 *
 * Return: QDF_STATUS
 */

QDF_STATUS ucfg_reg_set_keep_6ghz_sta_cli_connection(
					struct wlan_objmgr_pdev *pdev,
					bool keep_6ghz_sta_cli_connection);
/**
 * ucfg_reg_set_fcc_constraint() - apply fcc constraints on channels 12/13
 * @pdev: The physical pdev to reduce tx power for
 * @fcc_constraint: true to apply the constraint, false to remove it
 *
 * This function adjusts the transmit power on channels 12 and 13, to comply
 * with FCC regulations in the USA.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_fcc_constraint(struct wlan_objmgr_pdev *pdev,
				       bool fcc_constraint);

/**
 * ucfg_reg_get_default_country() - Get the default regulatory country
 * @psoc: The physical SoC to get default country from
 * @country_code: the buffer to populate the country code into
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_get_default_country(struct wlan_objmgr_psoc *psoc,
					uint8_t *country_code);

/**
 * ucfg_reg_get_current_country() - Get the current regulatory country
 * @psoc: The physical SoC to get current country from
 * @country_code: the buffer to populate the country code into
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_get_current_country(struct wlan_objmgr_psoc *psoc,
					uint8_t *country_code);

/**
 * ucfg_reg_set_default_country() - Set the default regulatory country
 * @psoc: The physical SoC to set default country for
 * @country: The country information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_default_country(struct wlan_objmgr_psoc *psoc,
					       uint8_t *country);

/**
 * ucfg_reg_set_country() - Set the current regulatory country
 * @pdev: The physical dev to set current country for
 * @country: The country information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_country(struct wlan_objmgr_pdev *pdev,
				uint8_t *country);

/**
 * ucfg_reg_reset_country() - Reset the regulatory country to default
 * @psoc: The physical SoC to reset country for
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_reset_country(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_enable_dfs_channels() - Enable the use of DFS channels
 * @pdev: The physical dev to enable DFS channels for
 * @dfs_enable: true to enable DFS channels, false to disable them
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_enable_dfs_channels(struct wlan_objmgr_pdev *pdev,
					bool dfs_enable);

QDF_STATUS ucfg_reg_register_event_handler(uint8_t vdev_id, reg_event_cb cb,
		void *arg);
QDF_STATUS ucfg_reg_unregister_event_handler(uint8_t vdev_id, reg_event_cb cb,
		void *arg);
QDF_STATUS ucfg_reg_init_handler(uint8_t pdev_id);

#ifdef WLAN_REG_PARTIAL_OFFLOAD
/**
 * ucfg_reg_program_default_cc() - Program default country code
 * @pdev: Pdev pointer
 * @regdmn: Regdomain value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_program_default_cc(struct wlan_objmgr_pdev *pdev,
				       uint16_t regdmn);
#endif

/**
 * ucfg_reg_program_cc() - Program user country code or regdomain
 * @pdev: The physical dev to program country code or regdomain
 * @rd: User country code or regdomain
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_program_cc(struct wlan_objmgr_pdev *pdev,
			       struct cc_regdmn_s *rd);

/**
 * ucfg_reg_get_current_cc() - get current country code or regdomain
 * @pdev: The physical dev to program country code or regdomain
 * @rd: Pointer to country code or regdomain
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_get_current_cc(struct wlan_objmgr_pdev *pdev,
				   struct cc_regdmn_s *rd);

/**
 * ucfg_reg_set_config_vars () - Set the config vars in reg component
 * @psoc: psoc ptr
 * @config_vars: config variables structure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_config_vars(struct wlan_objmgr_psoc *psoc,
				    struct reg_config_vars config_vars);

/**
 * ucfg_reg_get_current_chan_list () - get current channel list
 * @pdev: pdev ptr
 * @chan_list: channel list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
				    struct regulatory_channel *chan_list);

/**
 * ucfg_reg_modify_chan_144() - Enable/Disable channel 144
 * @pdev: pdev pointer
 * @enable_ch_144: flag to disable/enable channel 144
 *
 * Return: Success or Failure
 */
QDF_STATUS ucfg_reg_modify_chan_144(struct wlan_objmgr_pdev *pdev,
				    bool enable_ch_144);

/**
 * ucfg_reg_get_en_chan_144() - get en_chan_144 flag value
 * @pdev: pdev pointer
 *
 * Return: en_chan_144 flag value
 */
bool ucfg_reg_get_en_chan_144(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_reg_is_regdb_offloaded () - is regulatory database offloaded
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool ucfg_reg_is_regdb_offloaded(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_program_mas_chan_list () - program master channel list
 * @psoc: psoc ptr
 * @reg_channels: regulatory channels
 * @alpha2: country code
 * @dfs_region: dfs region
 *
 * Return: void
 */
void ucfg_reg_program_mas_chan_list(struct wlan_objmgr_psoc *psoc,
				    struct regulatory_channel *reg_channels,
				    uint8_t *alpha2,
				    enum dfs_reg dfs_region);

/**
 * ucfg_reg_get_regd_rules() - provides the reg domain rules info pointer
 * @pdev: pdev ptr
 * @reg_rules: regulatory rules
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_get_regd_rules(struct wlan_objmgr_pdev *pdev,
				   struct reg_rule_info *reg_rules);

/**
 * ucfg_reg_register_chan_change_callback () - add chan change cbk
 * @psoc: psoc ptr
 * @cbk: callback
 * @arg: argument
 *
 * Return: void
 */
void ucfg_reg_register_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					    void *cbk, void *arg);

/**
 * ucfg_reg_unregister_chan_change_callback () - remove chan change cbk
 * @psoc: psoc ptr
 * @cbk: callback
 *
 * Return: void
 */
void ucfg_reg_unregister_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					      void *cbk);

#ifdef CONFIG_AFC_SUPPORT
/**
 * ucfg_reg_register_afc_req_rx_callback () - add AFC request received callback
 * @pdev: Pointer to pdev
 * @cbf: Pointer to callback function
 * @arg: Pointer to opaque argument
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_register_afc_req_rx_callback(struct wlan_objmgr_pdev *pdev,
						 afc_req_rx_evt_handler cbf,
						 void *arg);

/**
 * ucfg_reg_unregister_afc_req_rx_callback () - remove AFC request received
 * callback
 * @pdev: Pointer to pdev
 * @cbf: Pointer to callback function
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_unregister_afc_req_rx_callback(struct wlan_objmgr_pdev *pdev,
						   afc_req_rx_evt_handler cbf);

/**
 * ucfg_reg_get_afc_req_info() - Get the the frequency ranges and
 * opclass + channel ranges. This is partial because in the AFC request there
 * are a few more parameters: Longitude, Latitude a few other information
 * @pdev: Pointer to PDEV object.
 * @afc_req: Address of AFC request pointer.
 * @req_id: AFC request ID.
 *
 * Return: QDF_STATUS_E_INVAL if unable to set and QDF_STATUS_SUCCESS is set.
 */
QDF_STATUS ucfg_reg_get_afc_req_info(struct wlan_objmgr_pdev *pdev,
				     struct wlan_afc_host_request **afc_req,
				     uint64_t req_id);

/**
 * ucfg_reg_free_afc_req() - Free the  memory allocated for AFC request
 * structure and its members.
 * @pdev: Pointer to pdev.
 * @afc_req: Pointer to AFC request structure.
 *
 * Return: void
 */
void
ucfg_reg_free_afc_req(struct wlan_objmgr_pdev *pdev,
		      struct wlan_afc_host_request *afc_req);

/**
 * ucfg_reg_register_afc_power_event_callback() - add AFC power event received
 * @pdev: Pointer to pdev
 * @cbf: Pointer to callback function
 * @arg: Pointer to opaque argument
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_reg_register_afc_power_event_callback(struct wlan_objmgr_pdev *pdev,
					   afc_power_tx_evt_handler cbf,
					   void *arg);

/**
 * ucfg_reg_unregister_afc_power_event_callback() - remove AFC power event
 * received callback
 * @pdev: Pointer to pdev
 * @cbf: Pointer to callback function
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_reg_unregister_afc_power_event_callback(struct wlan_objmgr_pdev *pdev,
					     afc_power_tx_evt_handler cbf);

/**
 * ucfg_reg_register_afc_payload_reset_event_callback() - Add AFC payload reset
 * event received callback
 * @pdev: Pointer to pdev
 * @cbf: Pointer to callback function
 * @arg: Pointer to opaque argument
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_register_afc_payload_reset_event_callback(
		struct wlan_objmgr_pdev *pdev,
		afc_payload_reset_tx_evt_handler cbf,
		void *arg);

/**
 * ucfg_reg_unregister_afc_payload_reset_event_callback() - Remove AFC payload
 * reset event received callback
 * @pdev: Pointer to pdev
 * @cbf: Pointer to callback function
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_unregister_afc_payload_reset_event_callback(
		struct wlan_objmgr_pdev *pdev,
		afc_payload_reset_tx_evt_handler cbf);
#endif

/**
 * ucfg_reg_get_cc_and_src () - get country code and src
 * @psoc: psoc ptr
 * @alpha2: country code alpha2
 *
 * Return: void
 */
enum country_src ucfg_reg_get_cc_and_src(struct wlan_objmgr_psoc *psoc,
					 uint8_t *alpha2);

/**
 * ucfg_reg_unit_simulate_ch_avoid () - fake a ch avoid event
 * @psoc: psoc ptr
 * @ch_avoid: ch_avoid_ind_type ranges
 *
 * This function inject a ch_avoid event for unit test sap chan switch.
 *
 * Return: void
 */
void ucfg_reg_unit_simulate_ch_avoid(struct wlan_objmgr_psoc *psoc,
	struct ch_avoid_ind_type *ch_avoid);

/**
 * ucfg_reg_ch_avoid () - Send channel avoid cmd to regulatory
 * @psoc: psoc ptr
 * @ch_avoid: ch_avoid_ind_type ranges
 *
 * This function send channel avoid cmd to regulatory from os_if/upper layer
 *
 * Return: void
 */
void ucfg_reg_ch_avoid(struct wlan_objmgr_psoc *psoc,
		       struct ch_avoid_ind_type *ch_avoid);

#ifdef FEATURE_WLAN_CH_AVOID_EXT
/**
 * ucfg_reg_ch_avoid_ext () - Send channel avoid extend cmd to regulatory
 * @psoc: psoc ptr
 * @ch_avoid: ch_avoid_ind_type ranges
 *
 * This function send channel avoid extend cmd to regulatory from
 * os_if/upper layer
 *
 * Return: void
 */
void ucfg_reg_ch_avoid_ext(struct wlan_objmgr_psoc *psoc,
			   struct ch_avoid_ind_type *ch_avoid);
#endif

#if defined(CONFIG_BAND_6GHZ) && defined(CONFIG_AFC_SUPPORT)
/**
 * ucfg_reg_get_enable_6ghz_sp_mode_support() - Get enable 6 GHz SP mode support
 * @psoc: psoc ptr
 *
 * Return: enable 6 GHz SP mode support flag
 */
bool ucfg_reg_get_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_set_enable_6ghz_sp_mode_support() - Set enable 6 GHz SP mode support
 * @psoc: psoc ptr
 * @value: value to be set
 *
 * Return: None
 */
void ucfg_reg_set_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc,
					      bool value);

/**
 * ucfg_reg_get_afc_disable_timer_check() - Get AFC timer check disable flag
 * @psoc: psoc ptr
 *
 * Return: AFC timer check disable flag
 */
bool ucfg_reg_get_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_set_afc_disable_timer_check() - Set AFC timer check disable flag
 * @psoc: psoc ptr
 * @value: value to be set
 *
 * Return: None
 */
void ucfg_reg_set_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc,
					  bool value);

/**
 * ucfg_reg_get_afc_disable_request_id_check() - Get AFC request id check flag
 * @psoc: psoc ptr
 *
 * Return: AFC request id check disable flag
 */
bool ucfg_reg_get_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_set_afc_disable_request_id_check() - Set AFC request id check flag
 * @psoc: psoc ptr
 * @value: value to be set
 *
 * Return: None
 */
void ucfg_reg_set_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc,
					       bool value);

/**
 * ucfg_reg_get_afc_no_action() - Get AFC no action flag
 * @psoc: psoc ptr
 *
 * Return: AFC no action flag
 */
bool ucfg_reg_get_afc_no_action(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_set_afc_no_action() - Set AFC no action flag
 * @psoc: psoc ptr
 * @value: value to be set
 *
 * Return: None
 */
void ucfg_reg_set_afc_no_action(struct wlan_objmgr_psoc *psoc, bool value);
#else
static inline
bool ucfg_reg_get_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void ucfg_reg_set_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc,
					      bool value)
{
}

static inline
bool ucfg_reg_get_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void ucfg_reg_set_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc,
					  bool value)
{
}

static inline
bool ucfg_reg_get_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void ucfg_reg_set_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc,
					       bool value)
{
}

static inline
bool ucfg_reg_get_afc_no_action(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void ucfg_reg_set_afc_no_action(struct wlan_objmgr_psoc *psoc, bool value)
{
}
#endif

#ifdef TARGET_11D_SCAN
/**
 * ucfg_reg_11d_vdev_delete_update() - update vdev delete to regulatory
 * @psoc: psoc pointer
 * @op_mode: Operating mode of the deleted vdev
 * @vdev_id: Vdev id of the deleted vdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_11d_vdev_delete_update(struct wlan_objmgr_psoc *psoc,
					   enum QDF_OPMODE op_mode,
					   uint32_t vdev_id);

/**
 * ucfg_reg_11d_vdev_created_update() - update vdev create to regulatory
 * @vdev: vdev ptr
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_11d_vdev_created_update(struct wlan_objmgr_vdev *vdev);
#else
static inline
QDF_STATUS ucfg_reg_11d_vdev_delete_update(struct wlan_objmgr_psoc *psoc,
					   enum QDF_OPMODE op_mode,
					   uint32_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_reg_11d_vdev_created_update(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * ucfg_reg_update_hal_cap_wireless_modes() - update wireless modes
 * @psoc: psoc ptr
 * @modes: value of modes to update
 * @phy_id: phy id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_update_hal_cap_wireless_modes(struct wlan_objmgr_psoc *psoc,
					       uint64_t modes, uint8_t phy_id);

/**
 * ucfg_reg_get_hal_reg_cap() - return hal reg cap
 * @psoc: psoc ptr
 *
 * Return: ptr to  wlan_psoc_host_hal_reg_capabilities_ext
 */
struct wlan_psoc_host_hal_reg_capabilities_ext *ucfg_reg_get_hal_reg_cap(
				struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_set_hal_reg_cap() - update hal reg cap
 * @psoc: psoc ptr
 * @reg_cap: Regulatory cap array
 * @phy_cnt: Number of phy
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_hal_reg_cap(struct wlan_objmgr_psoc *psoc,
			struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap,
			uint16_t phy_cnt);

/**
 * ucfg_reg_update_hal_reg_range_caps() - update hal reg frequency range fields
 * @psoc: psoc ptr
 * @low_2g_chan: low 2g channel
 * @high_2g_chan: high 2g channel
 * @low_5g_chan: low 5g channel
 * @high_5g_chan: high 2g channel
 * @phy_id: phy id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_update_hal_reg_range_caps(struct wlan_objmgr_psoc *psoc,
					      uint32_t low_2g_chan,
					      uint32_t high_2g_chan,
					      uint32_t low_5g_chan,
					      uint32_t high_5g_chan,
					      uint8_t phy_id);

/**
 * ucfg_set_ignore_fw_reg_offload_ind() - API to set ignore regdb offload ind
 * @psoc: psoc ptr
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_set_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_get_unii_5g_bitmap() - get unii_5g_bitmap value
 * @pdev: pdev pointer
 * @bitmap: Pointer to retrieve unii_5g_bitmap of enum reg_unii_band.
 *
 * Return: QDF_STATUS
 */
#ifdef DISABLE_UNII_SHARED_BANDS
QDF_STATUS
ucfg_reg_get_unii_5g_bitmap(struct wlan_objmgr_pdev *pdev, uint8_t *bitmap);
#else
static inline QDF_STATUS
ucfg_reg_get_unii_5g_bitmap(struct wlan_objmgr_pdev *pdev, uint8_t *bitmap)
{
	*bitmap = 0;
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(CONFIG_BAND_6GHZ)
/**
 * ucfg_reg_get_cur_6g_ap_pwr_type() - Get the current 6G regulatory AP power
 * type.
 * @pdev: Pointer to PDEV object.
 * @reg_cur_6g_ap_pwr_type: The current regulatory 6G AP type ie VLPI/LPI/SP.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
ucfg_reg_get_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type *reg_cur_6g_ap_pwr_type);

/**
 * ucfg_reg_set_cur_6g_ap_pwr_type() - Set the current 6G regulatory AP power
 * type.
 * @pdev: Pointer to PDEV object.
 * @reg_cur_6g_ap_pwr_type: Regulatory 6G AP type ie VLPI/LPI/SP.
 *
 * Return: QDF_STATUS_E_INVAL if unable to set and QDF_STATUS_SUCCESS is set.
 */
QDF_STATUS
ucfg_reg_set_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type reg_cur_6g_ap_pwr_type);
#else
static inline QDF_STATUS
ucfg_reg_get_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type *reg_cur_6g_ap_pwr_type)
{
	*reg_cur_6g_ap_pwr_type = REG_INDOOR_AP;
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
ucfg_reg_set_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type reg_cur_6g_ap_type)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
/**
 * ucfg_reg_send_afc_resp_rx_ind() - Send AFC response received indication to
 * the FW.
 * @pdev: pdev ptr
 * @afc_ind_obj: Pointer to hold AFC indication
 *
 * Return: QDF_STATUS_SUCCESS if the WMI command is sent or QDF_STATUS_E_FAILURE
 * otherwise
 */
QDF_STATUS
ucfg_reg_send_afc_resp_rx_ind(struct wlan_objmgr_pdev *pdev,
			      struct reg_afc_resp_rx_ind_info *afc_ind_obj);

/**
 * ucfg_reg_afc_start() - Start the AFC request from regulatory. This finally
 *                   sends the request to registered callbacks
 * @pdev: Pointer to pdev
 * @req_id: The AFC request ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_afc_start(struct wlan_objmgr_pdev *pdev, uint64_t req_id);
#endif

#ifndef CONFIG_REG_CLIENT
/**
 * ucfg_reg_enable_disable_opclass_chans() - Disable or enable the input 20 MHz
 * operating channels in the radio's current channel list.
 * @pdev: Pointer to pdev
 * @is_disable: Boolean to disable or enable the channels
 * @opclass: Operating class. Only 20MHz opclasses are supported.
 * @ieee_chan_list: Pointer to ieee_chan_list
 * @chan_list_size: Size of ieee_chan_list
 * @global_tbl_lookup: Whether to lookup global op class table
 *
 * Return - Return QDF_STATUS
 */
QDF_STATUS ucfg_reg_enable_disable_opclass_chans(struct wlan_objmgr_pdev *pdev,
						 bool is_disable,
						 uint8_t opclass,
						 uint8_t *ieee_chan_list,
						 uint8_t chan_list_size,
						 bool global_tbl_lookup);

static inline
bool ucfg_reg_is_user_country_set_allowed(struct wlan_objmgr_psoc *psoc)
{
	return true;
}

static inline
bool ucfg_reg_is_fcc_constraint_set(struct wlan_objmgr_pdev *pdev)
{
	return false;
}

#else
static inline QDF_STATUS
ucfg_reg_enable_disable_opclass_chans(struct wlan_objmgr_pdev *pdev,
				      bool is_disable,
				      uint8_t opclass,
				      uint8_t *ieee_chan_list,
				      uint8_t chan_list_size,
				      bool global_tbl_lookup)
{
	return QDF_STATUS_E_NOSUPPORT;
}

/**
 * ucfg_reg_is_user_country_set_allowed() - Checks whether user country is
 * allowed to set
 * @psoc: psoc ptr
 *
 * Return: bool
 */
bool ucfg_reg_is_user_country_set_allowed(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_reg_is_fcc_constraint_set() - Check if fcc constraint is set
 * @pdev: pointer to pdev
 *
 * Return: Return true if fcc constraint is set
 */
bool ucfg_reg_is_fcc_constraint_set(struct wlan_objmgr_pdev *pdev);
#endif
#endif
