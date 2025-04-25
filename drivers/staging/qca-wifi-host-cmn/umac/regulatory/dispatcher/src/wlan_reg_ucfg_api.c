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
 * DOC: wlan_req_ucfg_api.c
 *      contains regulatory user config interface definitions
 */

#include <wlan_objmgr_vdev_obj.h>
#include <wlan_reg_ucfg_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <../../core/src/reg_priv_objs.h>
#include <../../core/src/reg_utils.h>
#include <../../core/src/reg_services_common.h>
#include <../../core/src/reg_opclass.h>
#include <../../core/src/reg_lte.h>
#include <../../core/src/reg_offload_11d_scan.h>
#include <../../core/src/reg_build_chan_list.h>
#include <../../core/src/reg_callbacks.h>
#include <qdf_module.h>

QDF_STATUS ucfg_reg_register_event_handler(uint8_t vdev_id, reg_event_cb cb,
		void *arg)
{
	/* Register a event cb handler */
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_reg_unregister_event_handler(uint8_t vdev_id, reg_event_cb cb,
		void *arg)
{
	/* unregister a event cb handler */
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_reg_init_handler(uint8_t pdev_id)
{
	/* regulatory initialization handler */
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_reg_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
					  struct regulatory_channel *chan_list)
{
	return reg_get_current_chan_list(pdev, chan_list);
}

qdf_export_symbol(ucfg_reg_get_current_chan_list);

QDF_STATUS ucfg_reg_modify_chan_144(struct wlan_objmgr_pdev *pdev,
				    bool enable_ch_144)
{
	return reg_modify_chan_144(pdev, enable_ch_144);
}

bool ucfg_reg_get_en_chan_144(struct wlan_objmgr_pdev *pdev)
{
	return reg_get_en_chan_144(pdev);
}

QDF_STATUS ucfg_reg_set_config_vars(struct wlan_objmgr_psoc *psoc,
				 struct reg_config_vars config_vars)
{
	return reg_set_config_vars(psoc, config_vars);
}

bool ucfg_reg_is_regdb_offloaded(struct wlan_objmgr_psoc *psoc)
{
	return reg_is_regdb_offloaded(psoc);
}

void ucfg_reg_program_mas_chan_list(struct wlan_objmgr_psoc *psoc,
				    struct regulatory_channel *reg_channels,
				    uint8_t *alpha2,
				    enum dfs_reg dfs_region)
{
	reg_program_mas_chan_list(psoc, reg_channels, alpha2, dfs_region);
}

QDF_STATUS ucfg_reg_get_regd_rules(struct wlan_objmgr_pdev *pdev,
				   struct reg_rule_info *reg_rules)
{
	return reg_get_regd_rules(pdev, reg_rules);
}

#ifdef WLAN_REG_PARTIAL_OFFLOAD
QDF_STATUS ucfg_reg_program_default_cc(struct wlan_objmgr_pdev *pdev,
				       uint16_t regdmn)
{
	return reg_program_default_cc(pdev, regdmn);
}
#endif

QDF_STATUS ucfg_reg_program_cc(struct wlan_objmgr_pdev *pdev,
			       struct cc_regdmn_s *rd)
{
	return reg_program_chan_list(pdev, rd);
}

QDF_STATUS ucfg_reg_get_current_cc(struct wlan_objmgr_pdev *pdev,
				   struct cc_regdmn_s *rd)
{
	return reg_get_current_cc(pdev, rd);
}

#ifdef CONFIG_REG_CLIENT

QDF_STATUS ucfg_reg_set_band(struct wlan_objmgr_pdev *pdev,
			     uint32_t band_bitmap)
{
	return reg_set_band(pdev, band_bitmap);
}

QDF_STATUS ucfg_reg_get_band(struct wlan_objmgr_pdev *pdev,
			     uint32_t *band_bitmap)
{
	return reg_get_band(pdev, band_bitmap);
}

QDF_STATUS ucfg_reg_notify_sap_event(struct wlan_objmgr_pdev *pdev,
				     bool sap_state)
{
	return reg_notify_sap_event(pdev, sap_state);
}

QDF_STATUS ucfg_reg_set_fcc_constraint(struct wlan_objmgr_pdev *pdev,
				       bool fcc_constraint)
{
	return reg_set_fcc_constraint(pdev, fcc_constraint);
}

QDF_STATUS ucfg_reg_get_current_country(struct wlan_objmgr_psoc *psoc,
					       uint8_t *country_code)
{
	return reg_read_current_country(psoc, country_code);
}

QDF_STATUS ucfg_reg_set_default_country(struct wlan_objmgr_psoc *psoc,
					uint8_t *country)
{
	return reg_set_default_country(psoc, country);
}

bool ucfg_reg_get_keep_6ghz_sta_cli_connection(
					struct wlan_objmgr_pdev *pdev)
{
	return reg_get_keep_6ghz_sta_cli_connection(pdev);
}

QDF_STATUS ucfg_reg_set_keep_6ghz_sta_cli_connection(
					struct wlan_objmgr_pdev *pdev,
					bool keep_6ghz_sta_cli_connection)
{
	return reg_set_keep_6ghz_sta_cli_connection(pdev,
						keep_6ghz_sta_cli_connection);
}

bool ucfg_reg_is_user_country_set_allowed(struct wlan_objmgr_psoc *psoc)
{
	return reg_is_user_country_set_allowed(psoc);
}

bool ucfg_reg_is_fcc_constraint_set(struct wlan_objmgr_pdev *pdev)
{
	return reg_is_fcc_constraint_set(pdev);
}
#endif

QDF_STATUS ucfg_reg_get_default_country(struct wlan_objmgr_psoc *psoc,
					uint8_t *country_code)
{
	return reg_read_default_country(psoc, country_code);
}

QDF_STATUS ucfg_reg_set_country(struct wlan_objmgr_pdev *pdev,
				uint8_t *country)
{
	return reg_set_country(pdev, country);
}

QDF_STATUS ucfg_reg_reset_country(struct wlan_objmgr_psoc *psoc)
{
	return reg_reset_country(psoc);
}

QDF_STATUS ucfg_reg_enable_dfs_channels(struct wlan_objmgr_pdev *pdev,
					bool dfs_enable)
{
	return reg_enable_dfs_channels(pdev, dfs_enable);
}

void ucfg_reg_register_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					    void *cbk, void *arg)
{
	reg_register_chan_change_callback(psoc, (reg_chan_change_callback)cbk,
					  arg);
}

void ucfg_reg_unregister_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					      void *cbk)
{
	reg_unregister_chan_change_callback(psoc,
					    (reg_chan_change_callback)cbk);
}

#ifdef CONFIG_AFC_SUPPORT
QDF_STATUS ucfg_reg_register_afc_req_rx_callback(struct wlan_objmgr_pdev *pdev,
						 afc_req_rx_evt_handler cbf,
						 void *arg)
{
	return reg_register_afc_req_rx_callback(pdev, cbf, arg);
}

qdf_export_symbol(ucfg_reg_register_afc_req_rx_callback);

QDF_STATUS ucfg_reg_unregister_afc_req_rx_callback(struct wlan_objmgr_pdev *pdev,
						   afc_req_rx_evt_handler cbf)
{
	return reg_unregister_afc_req_rx_callback(pdev, cbf);
}

QDF_STATUS
ucfg_reg_register_afc_power_event_callback(struct wlan_objmgr_pdev *pdev,
					   afc_power_tx_evt_handler cbf,
					   void *arg)
{
	return reg_register_afc_power_event_callback(pdev, cbf, arg);
}

qdf_export_symbol(ucfg_reg_register_afc_power_event_callback);

QDF_STATUS
ucfg_reg_unregister_afc_power_event_callback(struct wlan_objmgr_pdev *pdev,
					     afc_power_tx_evt_handler cbf)
{
	return reg_unregister_afc_power_event_callback(pdev, cbf);
}

QDF_STATUS
ucfg_reg_register_afc_payload_reset_event_callback(
		struct wlan_objmgr_pdev *pdev,
		afc_payload_reset_tx_evt_handler cbf,
		void *arg) {
	return reg_register_afc_payload_reset_event_callback(pdev, cbf, arg);
}

qdf_export_symbol(ucfg_reg_register_afc_payload_reset_event_callback);

QDF_STATUS ucfg_reg_unregister_afc_payload_reset_event_callback(
		struct wlan_objmgr_pdev *pdev,
		afc_payload_reset_tx_evt_handler cbf)
{
	return reg_unregister_afc_payload_reset_event_callback(pdev, cbf);
}

QDF_STATUS ucfg_reg_get_afc_req_info(struct wlan_objmgr_pdev *pdev,
				     struct wlan_afc_host_request **afc_req,
				     uint64_t req_id)
{
	QDF_STATUS status;

	status = reg_get_afc_req_info(pdev, afc_req);

	if (status == QDF_STATUS_SUCCESS)
		reg_dmn_set_afc_req_id(*afc_req, req_id);

	return status;
}

void
ucfg_reg_free_afc_req(struct wlan_objmgr_pdev *pdev,
		      struct wlan_afc_host_request *afc_req)
{
	reg_free_afc_req(pdev, afc_req);
}
#endif

enum country_src ucfg_reg_get_cc_and_src(struct wlan_objmgr_psoc *psoc,
					 uint8_t *alpha2)
{
	return reg_get_cc_and_src(psoc, alpha2);
}

void ucfg_reg_unit_simulate_ch_avoid(struct wlan_objmgr_psoc *psoc,
	struct ch_avoid_ind_type *ch_avoid)
{
	reg_process_ch_avoid_event(psoc, ch_avoid);
}

void ucfg_reg_ch_avoid(struct wlan_objmgr_psoc *psoc,
		       struct ch_avoid_ind_type *ch_avoid)
{
	reg_process_ch_avoid_event(psoc, ch_avoid);
}

#ifdef FEATURE_WLAN_CH_AVOID_EXT
void ucfg_reg_ch_avoid_ext(struct wlan_objmgr_psoc *psoc,
			   struct ch_avoid_ind_type *ch_avoid)
{
	reg_process_ch_avoid_ext_event(psoc, ch_avoid);
}
#endif

#ifdef TARGET_11D_SCAN
QDF_STATUS ucfg_reg_11d_vdev_delete_update(struct wlan_objmgr_psoc *psoc,
					   enum QDF_OPMODE op_mode,
					   uint32_t vdev_id)
{
	return reg_11d_vdev_delete_update(psoc, op_mode, vdev_id);
}

QDF_STATUS ucfg_reg_11d_vdev_created_update(struct wlan_objmgr_vdev *vdev)
{
	return reg_11d_vdev_created_update(vdev);
}
#endif

QDF_STATUS ucfg_reg_update_hal_cap_wireless_modes(struct wlan_objmgr_psoc *psoc,
					       uint64_t modes, uint8_t phy_id)
{
	return reg_update_hal_cap_wireless_modes(psoc, modes, phy_id);
}

qdf_export_symbol(ucfg_reg_update_hal_cap_wireless_modes);

struct wlan_psoc_host_hal_reg_capabilities_ext *ucfg_reg_get_hal_reg_cap(
				struct wlan_objmgr_psoc *psoc)
{
	return reg_get_hal_reg_cap(psoc);
}
qdf_export_symbol(ucfg_reg_get_hal_reg_cap);

QDF_STATUS ucfg_reg_set_hal_reg_cap(struct wlan_objmgr_psoc *psoc,
		struct wlan_psoc_host_hal_reg_capabilities_ext *hal_reg_cap,
		uint16_t phy_cnt)

{
	return reg_set_hal_reg_cap(psoc, hal_reg_cap, phy_cnt);
}
qdf_export_symbol(ucfg_reg_set_hal_reg_cap);

QDF_STATUS ucfg_reg_update_hal_reg_range_caps(struct wlan_objmgr_psoc *psoc,
					      uint32_t low_2g_chan,
					      uint32_t high_2g_chan,
					      uint32_t low_5g_chan,
					      uint32_t high_5g_chan,
					      uint8_t phy_id)
{
	return reg_update_hal_reg_range_caps(psoc, low_2g_chan,
				      high_2g_chan, low_5g_chan, high_5g_chan,
				      phy_id);
}

qdf_export_symbol(ucfg_reg_update_hal_reg_range_caps);

#ifdef DISABLE_CHANNEL_LIST
#ifdef CONFIG_CHAN_FREQ_API
void ucfg_reg_cache_channel_freq_state(struct wlan_objmgr_pdev *pdev,
				       uint32_t *channel_list,
				       uint32_t num_channels)
{
	reg_cache_channel_freq_state(pdev, channel_list, num_channels);
}
#endif /* CONFIG_CHAN_FREQ_API */

void ucfg_reg_restore_cached_channels(struct wlan_objmgr_pdev *pdev)
{
	reg_restore_cached_channels(pdev);
}

void ucfg_reg_disable_cached_channels(struct wlan_objmgr_pdev *pdev)
{
	reg_disable_cached_channels(pdev);
}

#endif

QDF_STATUS ucfg_set_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc)
{
	return reg_set_ignore_fw_reg_offload_ind(psoc);
}

#ifdef DISABLE_UNII_SHARED_BANDS
QDF_STATUS
ucfg_reg_get_unii_5g_bitmap(struct wlan_objmgr_pdev *pdev, uint8_t *bitmap)
{
	return reg_get_unii_5g_bitmap(pdev, bitmap);
}
#endif

#if defined(CONFIG_BAND_6GHZ)
QDF_STATUS
ucfg_reg_set_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type reg_cur_6g_ap_pwr_type)
{
	return reg_set_cur_6g_ap_pwr_type(pdev, reg_cur_6g_ap_pwr_type);
}

QDF_STATUS
ucfg_reg_get_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type *reg_cur_6g_ap_pwr_type)
{
	return reg_get_cur_6g_ap_pwr_type(pdev, reg_cur_6g_ap_pwr_type);
}

qdf_export_symbol(ucfg_reg_get_cur_6g_ap_pwr_type);
#endif

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
bool ucfg_reg_get_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc)
{
	return reg_get_enable_6ghz_sp_mode_support(psoc);
}

qdf_export_symbol(ucfg_reg_get_enable_6ghz_sp_mode_support);

void ucfg_reg_set_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc,
					      bool value)
{
	reg_set_enable_6ghz_sp_mode_support(psoc, value);
}

qdf_export_symbol(ucfg_reg_set_enable_6ghz_sp_mode_support);

bool ucfg_reg_get_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc)
{
	return reg_get_afc_disable_timer_check(psoc);
}

qdf_export_symbol(ucfg_reg_get_afc_disable_timer_check);

void ucfg_reg_set_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc,
					  bool value)
{
	reg_set_afc_disable_timer_check(psoc, value);
}

qdf_export_symbol(ucfg_reg_set_afc_disable_timer_check);

bool ucfg_reg_get_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc)
{
	return reg_get_afc_disable_request_id_check(psoc);
}

qdf_export_symbol(ucfg_reg_get_afc_disable_request_id_check);

void ucfg_reg_set_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc,
					       bool value)
{
	reg_set_afc_disable_request_id_check(psoc, value);
}

qdf_export_symbol(ucfg_reg_set_afc_disable_request_id_check);

bool ucfg_reg_get_afc_no_action(struct wlan_objmgr_psoc *psoc)
{
	return reg_get_afc_noaction(psoc);
}

qdf_export_symbol(ucfg_reg_get_afc_no_action);

void ucfg_reg_set_afc_no_action(struct wlan_objmgr_psoc *psoc, bool value)
{
	reg_set_afc_noaction(psoc, value);
}

qdf_export_symbol(ucfg_reg_set_afc_no_action);
#endif

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
QDF_STATUS
ucfg_reg_send_afc_resp_rx_ind(struct wlan_objmgr_pdev *pdev,
			      struct reg_afc_resp_rx_ind_info *afc_ind_obj)
{
	return reg_send_afc_cmd(pdev, afc_ind_obj);
}

QDF_STATUS
ucfg_reg_afc_start(struct wlan_objmgr_pdev *pdev, uint64_t req_id)
{
	return reg_afc_start(pdev, req_id);
}
#endif

#ifndef CONFIG_REG_CLIENT
QDF_STATUS ucfg_reg_enable_disable_opclass_chans(struct wlan_objmgr_pdev *pdev,
						 bool is_disable,
						 uint8_t opclass,
						 uint8_t *ieee_chan_list,
						 uint8_t chan_list_size,
						 bool global_tbl_lookup)
{
	return reg_enable_disable_opclass_chans(pdev, is_disable, opclass,
						ieee_chan_list, chan_list_size,
						global_tbl_lookup);
}
#endif
