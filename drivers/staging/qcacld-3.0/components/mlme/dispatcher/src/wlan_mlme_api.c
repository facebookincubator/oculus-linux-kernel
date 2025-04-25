/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: define public APIs exposed by the mlme component
 */

#include "cfg_ucfg_api.h"
#include "wlan_mlme_main.h"
#include "wlan_mlme_ucfg_api.h"
#include "wma_types.h"
#include "wmi_unified.h"
#include "wma.h"
#include "wma_internal.h"
#include "wlan_crypto_global_api.h"
#include "wlan_utility.h"
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_vdev_mgr_utils_api.h"
#include <../../core/src/wlan_cm_vdev_api.h>
#include "wlan_psoc_mlme_api.h"
#include "wlan_action_oui_main.h"
#include "target_if.h"
#include "wlan_vdev_mgr_tgt_if_tx_api.h"
#include "wmi_unified_vdev_api.h"
#include "wlan_mlme_api.h"
#include "../../core/src/wlan_cp_stats_defs.h"
#include "wlan_reg_services_api.h"

/* quota in milliseconds */
#define MCC_DUTY_CYCLE 70

QDF_STATUS wlan_mlme_get_cfg_str(uint8_t *dst, struct mlme_cfg_str *cfg_str,
				 qdf_size_t *len)
{
	if (*len < cfg_str->len) {
		mlme_legacy_err("Invalid len %zd", *len);
		return QDF_STATUS_E_INVAL;
	}

	*len = cfg_str->len;
	qdf_mem_copy(dst, cfg_str->data, *len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_cfg_str(uint8_t *src, struct mlme_cfg_str *dst_cfg_str,
				 qdf_size_t len)
{
	if (len > dst_cfg_str->max_len) {
		mlme_legacy_err("Invalid len %zd (>%zd)", len,
				dst_cfg_str->max_len);
		return QDF_STATUS_E_INVAL;
	}

	dst_cfg_str->len = len;
	qdf_mem_copy(dst_cfg_str->data, src, len);

	return QDF_STATUS_SUCCESS;
}

uint8_t wlan_mlme_get_tx_power(struct wlan_objmgr_psoc *psoc,
			       enum band_info band)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;

	switch (band) {
	case BAND_2G:
		return mlme_obj->cfg.power.tx_power_2g;
	case BAND_5G:
		return mlme_obj->cfg.power.tx_power_5g;
	default:
		break;
	}
	return 0;
}

char *wlan_mlme_get_power_usage(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return NULL;

	return mlme_obj->cfg.power.power_usage.data;
}

QDF_STATUS
wlan_mlme_get_enable_deauth_to_disassoc_map(struct wlan_objmgr_psoc *psoc,
					    bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*value = mlme_obj->cfg.gen.enable_deauth_to_disassoc_map;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     *ht_cap_info)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*ht_cap_info = mlme_obj->cfg.ht_caps.ht_cap_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     ht_cap_info)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.ht_caps.ht_cap_info = ht_cap_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.ht_caps.max_num_amsdu;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (!cfg_in_range(CFG_MAX_AMSDU_NUM, value)) {
		mlme_legacy_err("Error in setting Max AMSDU Num");
		return QDF_STATUS_E_INVAL;
	}

	mlme_obj->cfg.ht_caps.max_num_amsdu = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = (uint8_t)mlme_obj->cfg.ht_caps.ampdu_params.mpdu_density;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (!cfg_in_range(CFG_MPDU_DENSITY, value)) {
		mlme_legacy_err("Invalid value %d", value);
		return QDF_STATUS_E_INVAL;
	}

	mlme_obj->cfg.ht_caps.ampdu_params.mpdu_density = value;

	return QDF_STATUS_SUCCESS;
}

#ifdef MULTI_CLIENT_LL_SUPPORT
bool wlan_mlme_get_wlm_multi_client_ll_caps(struct wlan_objmgr_psoc *psoc)
{
	return wlan_psoc_nif_fw_ext2_cap_get(psoc,
					WLAN_SOC_WLM_MULTI_CLIENT_LL_SUPPORT);
}
#endif

#ifdef FEATURE_WLAN_CH_AVOID_EXT
uint32_t wlan_mlme_get_coex_unsafe_chan_nb_user_prefer(
		struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return cfg_default(CFG_COEX_UNSAFE_CHAN_NB_USER_PREFER);
	}

	return mlme_obj->cfg.reg.coex_unsafe_chan_nb_user_prefer;
}

bool wlan_mlme_get_coex_unsafe_chan_nb_user_prefer_for_sap(
		struct wlan_objmgr_psoc *psoc)
{
	return !!(wlan_mlme_get_coex_unsafe_chan_nb_user_prefer(psoc) &
					IGNORE_FW_COEX_INFO_ON_SAP_MODE);
}

bool wlan_mlme_get_coex_unsafe_chan_nb_user_prefer_for_p2p_go(
		struct wlan_objmgr_psoc *psoc)
{
	return !!(wlan_mlme_get_coex_unsafe_chan_nb_user_prefer(psoc) &
					IGNORE_FW_COEX_INFO_ON_P2P_GO_MODE);
}
#endif

QDF_STATUS wlan_mlme_get_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t *band_capability)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*band_capability = mlme_obj->cfg.gen.band_capability;

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_MULTIPASS_SUPPORT
QDF_STATUS
wlan_mlme_peer_config_vlan(struct wlan_objmgr_vdev *vdev,
			   uint8_t *mac_addr)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle;
	struct peer_vlan_config_param param;

	wmi_handle = get_wmi_unified_hdl_from_pdev(wlan_vdev_get_pdev(vdev));
	if (!wmi_handle) {
		mlme_err("unable to get wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_set(&param, sizeof(param), 0);

	param.rx_cmd = 1;
	/* Enabling Rx_insert_inner_vlan_tag */
	param.rx_insert_c_tag = 1;
	param.vdev_id = wlan_vdev_get_id(vdev);

	status = wmi_send_peer_vlan_config(wmi_handle, mac_addr, param);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlan_mlme_set_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t band_capability)

{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.band_capability = band_capability;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
bool wlan_mlme_get_vendor_handoff_control_caps(struct wlan_objmgr_psoc *psoc)
{
	return wlan_psoc_nif_fw_ext2_cap_get(psoc,
					     WLAN_SOC_VENDOR_HANDOFF_CONTROL);
}
#endif

QDF_STATUS wlan_mlme_set_dual_sta_policy(struct wlan_objmgr_psoc *psoc,
					 uint8_t dual_sta_config)

{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.dual_sta_policy.concurrent_sta_policy =
								dual_sta_config;
	mlme_debug("Set dual_sta_config to :%d", dual_sta_config);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_dual_sta_policy(struct wlan_objmgr_psoc *psoc,
					 uint8_t *dual_sta_config)

{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*dual_sta_config =
		mlme_obj->cfg.gen.dual_sta_policy.concurrent_sta_policy;

	return QDF_STATUS_SUCCESS;
}

enum host_concurrent_ap_policy
wlan_mlme_convert_ap_policy_config(
		enum qca_wlan_concurrent_ap_policy_config ap_config)
{
	switch (ap_config) {
	case QCA_WLAN_CONCURRENT_AP_POLICY_UNSPECIFIED:
		return HOST_CONCURRENT_AP_POLICY_UNSPECIFIED;
	case QCA_WLAN_CONCURRENT_AP_POLICY_GAMING_AUDIO:
		return HOST_CONCURRENT_AP_POLICY_GAMING_AUDIO;
	case QCA_WLAN_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING:
		return HOST_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING;
	case QCA_WLAN_CONCURRENT_AP_POLICY_XR:
		return HOST_CONCURRENT_AP_POLICY_XR;
	default:
		return HOST_CONCURRENT_AP_POLICY_UNSPECIFIED;
	}
}

void wlan_mlme_ll_lt_sap_send_oce_flags_fw(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	uint8_t vdev_id;
	uint8_t updated_fw_value = 0;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return;

	updated_fw_value = mlme_obj->cfg.oce.feature_bitmap;
	vdev_id = wlan_vdev_get_id(vdev);
	wma_debug("Vdev %d Disable FILS discovery", vdev_id);
	updated_fw_value &= ~(WMI_VDEV_OCE_FILS_DISCOVERY_FRAME_FEATURE_BITMAP);
	if (wma_cli_set_command(vdev_id,
				wmi_vdev_param_enable_disable_oce_features,
				updated_fw_value, VDEV_CMD))
		mlme_legacy_err("Vdev %d failed to send OCE update", vdev_id);
}

QDF_STATUS wlan_mlme_set_ap_policy(struct wlan_objmgr_vdev *vdev,
				   enum host_concurrent_ap_policy ap_cfg_policy)

{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->mlme_ap.ap_policy = ap_cfg_policy;
	mlme_debug("Set ap_cfg_policy to :%d", mlme_priv->mlme_ap.ap_policy);

	return QDF_STATUS_SUCCESS;
}

enum host_concurrent_ap_policy
wlan_mlme_get_ap_policy(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return HOST_CONCURRENT_AP_POLICY_UNSPECIFIED;
	}

	mlme_debug("Get ap_cfg_policy to :%d", mlme_priv->mlme_ap.ap_policy);

	return mlme_priv->mlme_ap.ap_policy;
}

QDF_STATUS wlan_mlme_get_prevent_link_down(struct wlan_objmgr_psoc *psoc,
					   bool *prevent_link_down)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*prevent_link_down = mlme_obj->cfg.gen.prevent_link_down;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_select_5ghz_margin(struct wlan_objmgr_psoc *psoc,
					    uint8_t *select_5ghz_margin)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*select_5ghz_margin = mlme_obj->cfg.gen.select_5ghz_margin;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_rtt_mac_randomization(struct wlan_objmgr_psoc *psoc,
					       bool *rtt_mac_randomization)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*rtt_mac_randomization = mlme_obj->cfg.gen.rtt_mac_randomization;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_crash_inject(struct wlan_objmgr_psoc *psoc,
				      bool *crash_inject)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*crash_inject = mlme_obj->cfg.gen.crash_inject;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_lpass_support(struct wlan_objmgr_psoc *psoc,
				       bool *lpass_support)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*lpass_support = mlme_obj->cfg.gen.lpass_support;

	return QDF_STATUS_SUCCESS;
}

void wlan_mlme_get_wls_6ghz_cap(struct wlan_objmgr_psoc *psoc,
				bool *wls_6ghz_capable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*wls_6ghz_capable = cfg_default(CFG_WLS_6GHZ_CAPABLE);
		return;
	}
	*wls_6ghz_capable = mlme_obj->cfg.gen.wls_6ghz_capable;
}

QDF_STATUS wlan_mlme_get_self_recovery(struct wlan_objmgr_psoc *psoc,
				       bool *self_recovery)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*self_recovery = mlme_obj->cfg.gen.self_recovery;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sub_20_chan_width(struct wlan_objmgr_psoc *psoc,
					   uint8_t *sub_20_chan_width)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*sub_20_chan_width = mlme_obj->cfg.gen.sub_20_chan_width;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_fw_timeout_crash(struct wlan_objmgr_psoc *psoc,
					  bool *fw_timeout_crash)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*fw_timeout_crash = mlme_obj->cfg.gen.fw_timeout_crash;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_ito_repeat_count(struct wlan_objmgr_psoc *psoc,
					  uint8_t *ito_repeat_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*ito_repeat_count = mlme_obj->cfg.gen.ito_repeat_count;

	return QDF_STATUS_SUCCESS;
}

void wlan_mlme_get_sap_inactivity_override(struct wlan_objmgr_psoc *psoc,
					   bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	*val = mlme_obj->cfg.qos_mlme_params.sap_max_inactivity_override;
}

QDF_STATUS wlan_mlme_get_acs_with_more_param(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_acs_with_more_param;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_auto_channel_weight(struct wlan_objmgr_psoc *psoc,
					     uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*value = cfg_default(CFG_AUTO_CHANNEL_SELECT_WEIGHT);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.acs.auto_channel_select_weight;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_vendor_acs_support(struct wlan_objmgr_psoc *psoc,
					    bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_vendor_acs_support;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_acs_support_for_dfs_ltecoex(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_acs_support_for_dfs_ltecoex;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_external_acs_policy(struct wlan_objmgr_psoc *psoc,
				  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_external_acs_policy;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_tx_chainmask_cck(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_cck;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_tx_chainmask_1ss(struct wlan_objmgr_psoc *psoc,
					  uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_1ss;
	return QDF_STATUS_SUCCESS;
}

bool
wlan_mlme_is_data_stall_recovery_fw_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_err("MLME obj is NULL");
		return false;
	}

	return mlme_obj->cfg.gen.data_stall_recovery_fw_support;
}

void
wlan_mlme_update_cfg_with_tgt_caps(struct wlan_objmgr_psoc *psoc,
				   struct mlme_tgt_caps *tgt_caps)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	/* Update the mlme cfg according to the tgt capability received */

	mlme_obj->cfg.gen.data_stall_recovery_fw_support =
				tgt_caps->data_stall_recovery_fw_support;

	mlme_obj->cfg.gen.bigtk_support = tgt_caps->bigtk_support;
	mlme_obj->cfg.gen.stop_all_host_scan_support =
			tgt_caps->stop_all_host_scan_support;
	mlme_obj->cfg.gen.dual_sta_roam_fw_support =
			tgt_caps->dual_sta_roam_fw_support;
	mlme_obj->cfg.gen.ocv_support = tgt_caps->ocv_support;
}

void
wlan_mlme_update_aux_dev_caps(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_mlme_aux_dev_caps wlan_mlme_aux_dev_caps[])
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	qdf_mem_copy(&mlme_obj->cfg.gen.wlan_mlme_aux0_dev_caps[0],
		     &wlan_mlme_aux_dev_caps[0],
		     sizeof(mlme_obj->cfg.gen.wlan_mlme_aux0_dev_caps));
}

bool wlan_mlme_cfg_get_aux_supported_modes(
			struct wlan_objmgr_psoc *psoc,
			uint32_t aux_index,
			enum wlan_mlme_hw_mode_config_type hw_mode_id,
			uint32_t *supported_modes_bitmap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_mlme_aux_dev_caps *wlan_mlme_aux0_dev_caps;

	if (aux_index != 0) {
		mlme_err("current only support aux0");
		return false;
	}

	if (hw_mode_id >= WLAN_MLME_HW_MODE_MAX) {
		mlme_err("invalid hw mode id %d.", hw_mode_id);
		return false;
	}

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_err("MLME obj is NULL");
		return false;
	}
	wlan_mlme_aux0_dev_caps = mlme_obj->cfg.gen.wlan_mlme_aux0_dev_caps;
	*supported_modes_bitmap =
		wlan_mlme_aux0_dev_caps[hw_mode_id].supported_modes_bitmap;
	return true;
}

/**
 * wlan_mlme_is_aux_cap_support() - checking the corresponding capability
 * @psoc: wlan_objmgr_psoc pointer
 * @bit: the corresponding bit
 * @hw_mode_id: hw mode id
 *
 * Return: true if corresponding capability supporting
 */
static bool
wlan_mlme_is_aux_cap_support(struct wlan_objmgr_psoc *psoc,
			     enum wlan_mlme_aux_caps_bit bit,
			     enum wlan_mlme_hw_mode_config_type hw_mode_id)
{
	uint32_t supported_modes_bitmap = 0;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_mlme_aux_dev_caps *wlan_mlme_aux0_dev_caps;
	int idx;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_err("MLME obj is NULL");
		return false;
	}

	wlan_mlme_aux0_dev_caps = mlme_obj->cfg.gen.wlan_mlme_aux0_dev_caps;
	if (hw_mode_id >= WLAN_MLME_HW_MODE_MAX) {
		for (idx = 0; idx < WLAN_MLME_HW_MODE_MAX; idx++)
			supported_modes_bitmap |=
			    wlan_mlme_aux0_dev_caps[idx].supported_modes_bitmap;
	} else {
		supported_modes_bitmap =
		     wlan_mlme_aux0_dev_caps[hw_mode_id].supported_modes_bitmap;
	}

	return (supported_modes_bitmap & (0x1 << bit)) ? true : false;
}

bool
wlan_mlme_is_aux_scan_support(struct wlan_objmgr_psoc *psoc,
			      enum wlan_mlme_hw_mode_config_type hw_mode_id)
{
	return wlan_mlme_is_aux_cap_support(psoc, WLAN_MLME_AUX_MODE_SCAN_BIT,
					    hw_mode_id);
}

bool
wlan_mlme_is_aux_listen_support(struct wlan_objmgr_psoc *psoc,
				enum wlan_mlme_hw_mode_config_type hw_mode_id)
{
	return wlan_mlme_is_aux_cap_support(psoc, WLAN_MLME_AUX_MODE_LISTEN_BIT,
					    hw_mode_id);
}

bool
wlan_mlme_is_aux_emlsr_support(struct wlan_objmgr_psoc *psoc,
			       enum wlan_mlme_hw_mode_config_type hw_mode_id)
{
	return wlan_mlme_is_aux_cap_support(psoc, WLAN_MLME_AUX_MODE_EMLSR_BIT,
					    hw_mode_id);
}

#ifdef WLAN_FEATURE_11AX
QDF_STATUS wlan_mlme_cfg_get_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.he_caps.dot11_he_cap.ul_mu;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_cfg_get_he_caps(struct wlan_objmgr_psoc *psoc,
				tDot11fIEhe_cap *he_cap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*he_cap = mlme_obj->cfg.he_caps.he_cap_orig;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (!cfg_in_range(CFG_HE_UL_MUMIMO, value)) {
		mlme_legacy_debug("Failed to set CFG_HE_UL_MUMIMO with %d",
				  value);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_obj->cfg.he_caps.dot11_he_cap.ul_mu = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_enable_ul_mimo(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.he_caps.enable_ul_mimo;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_enable_ul_ofdm(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;


	*value = mlme_obj->cfg.he_caps.enable_ul_ofdm;

	return QDF_STATUS_SUCCESS;
}

/* mlme_get_min_rate_cap() - get minimum capability for HE-MCS between
 *                           ini value and fw capability.
 *
 * Rx HE-MCS Map and Tx HE-MCS Map subfields format where 2-bit indicates
 * 0 indicates support for HE-MCS 0-7 for n spatial streams
 * 1 indicates support for HE-MCS 0-9 for n spatial streams
 * 2 indicates support for HE-MCS 0-11 for n spatial streams
 * 3 indicates that n spatial streams is not supported for HE PPDUs
 *
 */
static uint16_t mlme_get_min_rate_cap(uint16_t val1, uint16_t val2)
{
	uint16_t ret = 0, i;

	for (i = 0; i < 8; i++) {
		if (((val1 >> (2 * i)) & 0x3) == 0x3 ||
		    ((val2 >> (2 * i)) & 0x3) == 0x3) {
			ret |= 0x3 << (2 * i);
			continue;
		}
		ret |= QDF_MIN((val1 >> (2 * i)) & 0x3,
			      (val2 >> (2 * i)) & 0x3) << (2 * i);
	}
	return ret;
}

QDF_STATUS mlme_update_tgt_he_caps_in_cfg(struct wlan_objmgr_psoc *psoc,
					  struct wma_tgt_cfg *wma_cfg)
{
	uint8_t chan_width;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tDot11fIEhe_cap *he_cap = &wma_cfg->he_cap;
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);
	uint8_t value, twt_req, twt_resp;
	uint16_t tx_mcs_map = 0;
	uint16_t rx_mcs_map = 0;
	uint8_t nss;

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.he_caps.dot11_he_cap.present = 1;
	mlme_obj->cfg.he_caps.dot11_he_cap.htc_he = he_cap->htc_he;

	twt_req = QDF_MIN(he_cap->twt_request,
			  mlme_obj->cfg.he_caps.dot11_he_cap.twt_request);
	mlme_obj->cfg.he_caps.dot11_he_cap.twt_request = twt_req;

	twt_resp = QDF_MIN(he_cap->twt_responder,
			   mlme_obj->cfg.he_caps.dot11_he_cap.twt_responder);
	mlme_obj->cfg.he_caps.dot11_he_cap.twt_responder = twt_resp;

	value = QDF_MIN(he_cap->fragmentation,
			mlme_obj->cfg.he_caps.he_dynamic_fragmentation);

	if (cfg_in_range(CFG_HE_FRAGMENTATION, value))
		mlme_obj->cfg.he_caps.dot11_he_cap.fragmentation = value;

	if (cfg_in_range(CFG_HE_MAX_FRAG_MSDU,
			 he_cap->max_num_frag_msdu_amsdu_exp))
		mlme_obj->cfg.he_caps.dot11_he_cap.max_num_frag_msdu_amsdu_exp =
					he_cap->max_num_frag_msdu_amsdu_exp;
	if (cfg_in_range(CFG_HE_MIN_FRAG_SIZE, he_cap->min_frag_size))
		mlme_obj->cfg.he_caps.dot11_he_cap.min_frag_size =
					he_cap->min_frag_size;
	if (cfg_in_range(CFG_HE_TRIG_PAD, he_cap->trigger_frm_mac_pad))
		mlme_obj->cfg.he_caps.dot11_he_cap.trigger_frm_mac_pad =
			QDF_MIN(he_cap->trigger_frm_mac_pad,
				mlme_obj->cfg.he_caps.dot11_he_cap.trigger_frm_mac_pad);
	if (cfg_in_range(CFG_HE_MTID_AGGR_RX, he_cap->multi_tid_aggr_rx_supp))
		mlme_obj->cfg.he_caps.dot11_he_cap.multi_tid_aggr_rx_supp =
					he_cap->multi_tid_aggr_rx_supp;
	if (cfg_in_range(CFG_HE_MTID_AGGR_TX, he_cap->multi_tid_aggr_tx_supp))
		mlme_obj->cfg.he_caps.dot11_he_cap.multi_tid_aggr_tx_supp =
					he_cap->multi_tid_aggr_tx_supp;
	if (cfg_in_range(CFG_HE_LINK_ADAPTATION, he_cap->he_link_adaptation))
		mlme_obj->cfg.he_caps.dot11_he_cap.he_link_adaptation =
					he_cap->he_link_adaptation;
	mlme_obj->cfg.he_caps.dot11_he_cap.all_ack = he_cap->all_ack;
	mlme_obj->cfg.he_caps.dot11_he_cap.trigd_rsp_sched =
					he_cap->trigd_rsp_sched;
	mlme_obj->cfg.he_caps.dot11_he_cap.a_bsr = he_cap->a_bsr;

	value = QDF_MIN(he_cap->broadcast_twt,
			mlme_obj->cfg.he_caps.dot11_he_cap.broadcast_twt);
	mlme_obj->cfg.he_caps.dot11_he_cap.broadcast_twt = value;

	/*
	 * As per 802.11ax spec, Flexible TWT capability can be set
	 * independent of TWT Requestor/Responder capability.
	 * But currently we don't have any such usecase and firmware
	 * does not support it. Hence enabling Flexible TWT only when
	 * either or both of the TWT Requestor/Responder capability
	 * is set/enabled.
	 */
	value = QDF_MIN(he_cap->flex_twt_sched, (twt_req || twt_resp));
	/*
	 * mlme obj will have intersected flex_twt_sched value
	 * taken from ini value, FW capability and twt req/resp
	 */
	value = QDF_MIN(value,
			mlme_obj->cfg.he_caps.dot11_he_cap.flex_twt_sched);
	mlme_obj->cfg.he_caps.dot11_he_cap.flex_twt_sched = value;

	mlme_obj->cfg.he_caps.dot11_he_cap.ba_32bit_bitmap =
					he_cap->ba_32bit_bitmap;
	mlme_obj->cfg.he_caps.dot11_he_cap.mu_cascade = he_cap->mu_cascade;
	mlme_obj->cfg.he_caps.dot11_he_cap.ack_enabled_multitid =
					he_cap->ack_enabled_multitid;
	mlme_obj->cfg.he_caps.dot11_he_cap.omi_a_ctrl = he_cap->omi_a_ctrl;
	mlme_obj->cfg.he_caps.dot11_he_cap.ofdma_ra = he_cap->ofdma_ra;
	if (cfg_in_range(CFG_HE_MAX_AMPDU_LEN, he_cap->max_ampdu_len_exp_ext))
		mlme_obj->cfg.he_caps.dot11_he_cap.max_ampdu_len_exp_ext =
					he_cap->max_ampdu_len_exp_ext;
	mlme_obj->cfg.he_caps.dot11_he_cap.amsdu_frag = he_cap->amsdu_frag;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_ctrl_frame =
					he_cap->rx_ctrl_frame;
	mlme_obj->cfg.he_caps.dot11_he_cap.bsrp_ampdu_aggr =
					he_cap->bsrp_ampdu_aggr;
	mlme_obj->cfg.he_caps.dot11_he_cap.qtp = he_cap->qtp;
	mlme_obj->cfg.he_caps.dot11_he_cap.a_bqr = he_cap->a_bqr;
	mlme_obj->cfg.he_caps.dot11_he_cap.spatial_reuse_param_rspder =
					he_cap->spatial_reuse_param_rspder;
	mlme_obj->cfg.he_caps.dot11_he_cap.ndp_feedback_supp =
					he_cap->ndp_feedback_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.ops_supp = he_cap->ops_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.amsdu_in_ampdu =
					he_cap->amsdu_in_ampdu;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_sub_ch_sel_tx_supp =
					he_cap->he_sub_ch_sel_tx_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.ul_2x996_tone_ru_supp =
					he_cap->ul_2x996_tone_ru_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.om_ctrl_ul_mu_data_dis_rx =
					he_cap->om_ctrl_ul_mu_data_dis_rx;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_dynamic_smps =
					he_cap->he_dynamic_smps;
	mlme_obj->cfg.he_caps.dot11_he_cap.punctured_sounding_supp =
					he_cap->punctured_sounding_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.ht_vht_trg_frm_rx_supp =
					he_cap->ht_vht_trg_frm_rx_supp;

	chan_width = HE_CH_WIDTH_COMBINE(he_cap->chan_width_0,
					 he_cap->chan_width_1,
					 he_cap->chan_width_2,
					 he_cap->chan_width_3,
					 he_cap->chan_width_4,
					 he_cap->chan_width_5,
					 he_cap->chan_width_6);
	if (cfg_in_range(CFG_HE_CHAN_WIDTH, chan_width)) {
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_0 =
						he_cap->chan_width_0;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_1 =
						he_cap->chan_width_1;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_2 =
						he_cap->chan_width_2;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_3 =
						he_cap->chan_width_3;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_4 =
						he_cap->chan_width_4;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_5 =
						he_cap->chan_width_5;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_6 =
						he_cap->chan_width_6;
	}
	if (cfg_in_range(CFG_HE_RX_PREAM_PUNC, he_cap->rx_pream_puncturing))
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_pream_puncturing =
				he_cap->rx_pream_puncturing;
	mlme_obj->cfg.he_caps.dot11_he_cap.device_class = he_cap->device_class;
	mlme_obj->cfg.he_caps.dot11_he_cap.ldpc_coding = he_cap->ldpc_coding;
	if (cfg_in_range(CFG_HE_LTF_PPDU, he_cap->he_1x_ltf_800_gi_ppdu))
		mlme_obj->cfg.he_caps.dot11_he_cap.he_1x_ltf_800_gi_ppdu =
					he_cap->he_1x_ltf_800_gi_ppdu;
	if (cfg_in_range(CFG_HE_MIDAMBLE_RX_MAX_NSTS,
			 he_cap->midamble_tx_rx_max_nsts))
		mlme_obj->cfg.he_caps.dot11_he_cap.midamble_tx_rx_max_nsts =
					he_cap->midamble_tx_rx_max_nsts;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_4x_ltf_3200_gi_ndp =
					he_cap->he_4x_ltf_3200_gi_ndp;
	if (mlme_obj->cfg.vht_caps.vht_cap_info.rx_stbc) {
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_lt_80mhz =
					he_cap->rx_stbc_lt_80mhz;
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_gt_80mhz =
					he_cap->rx_stbc_gt_80mhz;
	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_lt_80mhz = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_gt_80mhz = 0;
	}
	if (mlme_obj->cfg.vht_caps.vht_cap_info.tx_stbc) {
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_lt_80mhz =
					he_cap->tb_ppdu_tx_stbc_lt_80mhz;
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_gt_80mhz =
					he_cap->tb_ppdu_tx_stbc_gt_80mhz;
	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_lt_80mhz = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_gt_80mhz = 0;
	}

	if (cfg_in_range(CFG_HE_DOPPLER, he_cap->doppler))
		mlme_obj->cfg.he_caps.dot11_he_cap.doppler = he_cap->doppler;
	if (cfg_in_range(CFG_HE_DCM_TX, he_cap->dcm_enc_tx))
		mlme_obj->cfg.he_caps.dot11_he_cap.dcm_enc_tx =
						he_cap->dcm_enc_tx;
	if (cfg_in_range(CFG_HE_DCM_RX, he_cap->dcm_enc_rx))
		mlme_obj->cfg.he_caps.dot11_he_cap.dcm_enc_rx =
						he_cap->dcm_enc_rx;
	mlme_obj->cfg.he_caps.dot11_he_cap.ul_he_mu = he_cap->ul_he_mu;
	if (mlme_obj->cfg.vht_caps.vht_cap_info.su_bformer) {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformer =
					he_cap->su_beamformer;
		if (cfg_in_range(CFG_HE_NUM_SOUND_LT80,
				 he_cap->num_sounding_lt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_lt_80 =
						he_cap->num_sounding_lt_80;
		if (cfg_in_range(CFG_HE_NUM_SOUND_GT80,
				 he_cap->num_sounding_gt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_gt_80 =
						he_cap->num_sounding_gt_80;
		mlme_obj->cfg.he_caps.dot11_he_cap.mu_beamformer =
					he_cap->mu_beamformer;

	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformer = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_lt_80 = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_gt_80 = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.mu_beamformer = 0;
	}

	if (mlme_obj->cfg.vht_caps.vht_cap_info.su_bformee) {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformee =
					he_cap->su_beamformee;
		if (cfg_in_range(CFG_HE_BFEE_STS_LT80, he_cap->bfee_sts_lt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_lt_80 =
						he_cap->bfee_sts_lt_80;
		if (cfg_in_range(CFG_HE_BFEE_STS_GT80, he_cap->bfee_sts_gt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_gt_80 =
						he_cap->bfee_sts_gt_80;

	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformee = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_lt_80 = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_gt_80 = 0;
	}

	if (!mlme_obj->cfg.he_caps.enable_ul_mimo) {
		mlme_debug("UL MIMO feature is disabled via ini, fw caps :%d",
			   he_cap->ul_mu);
		mlme_obj->cfg.he_caps.dot11_he_cap.ul_mu = 0;
	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.ul_mu = he_cap->ul_mu;
	}

	mlme_obj->cfg.he_caps.dot11_he_cap.su_feedback_tone16 =
					he_cap->su_feedback_tone16;
	mlme_obj->cfg.he_caps.dot11_he_cap.mu_feedback_tone16 =
					he_cap->mu_feedback_tone16;
	mlme_obj->cfg.he_caps.dot11_he_cap.codebook_su = he_cap->codebook_su;
	mlme_obj->cfg.he_caps.dot11_he_cap.codebook_mu = he_cap->codebook_mu;
	if (cfg_in_range(CFG_HE_BFRM_FEED, he_cap->beamforming_feedback))
		mlme_obj->cfg.he_caps.dot11_he_cap.beamforming_feedback =
					he_cap->beamforming_feedback;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_er_su_ppdu =
					he_cap->he_er_su_ppdu;
	mlme_obj->cfg.he_caps.dot11_he_cap.dl_mu_mimo_part_bw =
					he_cap->dl_mu_mimo_part_bw;
	mlme_obj->cfg.he_caps.dot11_he_cap.ppet_present = he_cap->ppet_present;
	mlme_obj->cfg.he_caps.dot11_he_cap.srp = he_cap->srp;
	mlme_obj->cfg.he_caps.dot11_he_cap.power_boost = he_cap->power_boost;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ltf_800_gi_4x =
					he_cap->he_ltf_800_gi_4x;
	if (cfg_in_range(CFG_HE_MAX_NC, he_cap->max_nc))
		mlme_obj->cfg.he_caps.dot11_he_cap.max_nc = he_cap->max_nc;
	mlme_obj->cfg.he_caps.dot11_he_cap.er_he_ltf_800_gi_4x =
					he_cap->er_he_ltf_800_gi_4x;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ppdu_20_in_40Mhz_2G =
					he_cap->he_ppdu_20_in_40Mhz_2G;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ppdu_20_in_160_80p80Mhz =
					he_cap->he_ppdu_20_in_160_80p80Mhz;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ppdu_80_in_160_80p80Mhz =
					he_cap->he_ppdu_80_in_160_80p80Mhz;
	mlme_obj->cfg.he_caps.dot11_he_cap.er_1x_he_ltf_gi =
					he_cap->er_1x_he_ltf_gi;
	mlme_obj->cfg.he_caps.dot11_he_cap.midamble_tx_rx_1x_he_ltf =
					he_cap->midamble_tx_rx_1x_he_ltf;
	if (cfg_in_range(CFG_HE_DCM_MAX_BW, he_cap->dcm_max_bw))
		mlme_obj->cfg.he_caps.dot11_he_cap.dcm_max_bw =
					he_cap->dcm_max_bw;
	mlme_obj->cfg.he_caps.dot11_he_cap.longer_than_16_he_sigb_ofdm_sym =
					he_cap->longer_than_16_he_sigb_ofdm_sym;
	mlme_obj->cfg.he_caps.dot11_he_cap.tx_1024_qam_lt_242_tone_ru =
					he_cap->tx_1024_qam_lt_242_tone_ru;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_1024_qam_lt_242_tone_ru =
					he_cap->rx_1024_qam_lt_242_tone_ru;
	mlme_obj->cfg.he_caps.dot11_he_cap.non_trig_cqi_feedback =
					he_cap->non_trig_cqi_feedback;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_full_bw_su_he_mu_compress_sigb =
				he_cap->rx_full_bw_su_he_mu_compress_sigb;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_full_bw_su_he_mu_non_cmpr_sigb =
				he_cap->rx_full_bw_su_he_mu_non_cmpr_sigb;

	tx_mcs_map = mlme_get_min_rate_cap(
		mlme_obj->cfg.he_caps.dot11_he_cap.tx_he_mcs_map_lt_80,
		he_cap->tx_he_mcs_map_lt_80);
	rx_mcs_map = mlme_get_min_rate_cap(
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_he_mcs_map_lt_80,
		he_cap->rx_he_mcs_map_lt_80);
	if (!mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2) {
		nss = 2;
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, HE_MCS_DISABLE, nss);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, HE_MCS_DISABLE, nss);
	}

	if (cfg_in_range(CFG_HE_RX_MCS_MAP_LT_80, rx_mcs_map))
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_he_mcs_map_lt_80 =
			rx_mcs_map;
	if (cfg_in_range(CFG_HE_TX_MCS_MAP_LT_80, tx_mcs_map))
		mlme_obj->cfg.he_caps.dot11_he_cap.tx_he_mcs_map_lt_80 =
			tx_mcs_map;
	tx_mcs_map = mlme_get_min_rate_cap(
	   *((uint16_t *)mlme_obj->cfg.he_caps.dot11_he_cap.tx_he_mcs_map_160),
	   *((uint16_t *)he_cap->tx_he_mcs_map_160));
	rx_mcs_map = mlme_get_min_rate_cap(
	   *((uint16_t *)mlme_obj->cfg.he_caps.dot11_he_cap.rx_he_mcs_map_160),
	   *((uint16_t *)he_cap->rx_he_mcs_map_160));

	if (!mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2) {
		nss = 2;
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, HE_MCS_DISABLE, nss);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, HE_MCS_DISABLE, nss);
	}

	if (cfg_in_range(CFG_HE_RX_MCS_MAP_160, rx_mcs_map))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     rx_he_mcs_map_160,
			     &rx_mcs_map, sizeof(uint16_t));

	if (cfg_in_range(CFG_HE_TX_MCS_MAP_160, tx_mcs_map))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     tx_he_mcs_map_160,
			     &tx_mcs_map, sizeof(uint16_t));

	if (cfg_in_range(CFG_HE_RX_MCS_MAP_80_80,
			 *((uint16_t *)he_cap->rx_he_mcs_map_80_80)))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     rx_he_mcs_map_80_80,
			     he_cap->rx_he_mcs_map_80_80, sizeof(uint16_t));

	if (cfg_in_range(CFG_HE_TX_MCS_MAP_80_80,
			 *((uint16_t *)he_cap->tx_he_mcs_map_80_80)))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     tx_he_mcs_map_80_80,
			     he_cap->tx_he_mcs_map_80_80, sizeof(uint16_t));

	qdf_mem_copy(mlme_obj->cfg.he_caps.he_ppet_2g, wma_cfg->ppet_2g,
		     HE_MAX_PPET_SIZE);

	qdf_mem_copy(mlme_obj->cfg.he_caps.he_ppet_5g, wma_cfg->ppet_5g,
		     HE_MAX_PPET_SIZE);

	mlme_obj->cfg.he_caps.he_cap_orig = mlme_obj->cfg.he_caps.dot11_he_cap;
	/* Take intersection of host and FW capabilities */
	mlme_obj->cfg.he_caps.he_mcs_12_13_supp_2g &=
						  wma_cfg->he_mcs_12_13_supp_2g;
	mlme_obj->cfg.he_caps.he_mcs_12_13_supp_5g &=
						  wma_cfg->he_mcs_12_13_supp_5g;
	mlme_debug("mcs_12_13 2G: %x 5G: %x FW_cap: 2G: %x 5G: %x",
		   mlme_obj->cfg.he_caps.he_mcs_12_13_supp_2g,
		   mlme_obj->cfg.he_caps.he_mcs_12_13_supp_5g,
		   wma_cfg->he_mcs_12_13_supp_2g,
		   wma_cfg->he_mcs_12_13_supp_5g);

	return status;
}
#ifdef WLAN_FEATURE_SR
void
wlan_mlme_get_sr_enable_modes(struct wlan_objmgr_psoc *psoc, uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*val = cfg_default(CFG_SR_ENABLE_MODES);
		return;
	}
	*val = mlme_obj->cfg.gen.sr_enable_modes;
}
#endif
#endif

#ifdef WLAN_FEATURE_11BE
QDF_STATUS mlme_update_tgt_eht_caps_in_cfg(struct wlan_objmgr_psoc *psoc,
					   struct wma_tgt_cfg *wma_cfg)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);
	tDot11fIEeht_cap *eht_cap = &wma_cfg->eht_cap;
	tDot11fIEeht_cap *mlme_eht_cap;
	bool eht_capab;

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	wlan_psoc_mlme_get_11be_capab(psoc, &eht_capab);
	if (!eht_capab)
		return QDF_STATUS_SUCCESS;

	mlme_obj->cfg.eht_caps.dot11_eht_cap.present = 1;
	qdf_mem_copy(&mlme_obj->cfg.eht_caps.dot11_eht_cap, eht_cap,
		     sizeof(tDot11fIEeht_cap));
	mlme_eht_cap = &mlme_obj->cfg.eht_caps.dot11_eht_cap;
	if (mlme_obj->cfg.vht_caps.vht_cap_info.su_bformer) {
		mlme_eht_cap->su_beamformer = eht_cap->su_beamformer;
		if (cfg_in_range(CFG_EHT_NUM_SOUNDING_DIM_LE_80MHZ,
				 eht_cap->num_sounding_dim_le_80mhz))
			mlme_eht_cap->num_sounding_dim_le_80mhz =
				eht_cap->num_sounding_dim_le_80mhz;
		if (cfg_in_range(CFG_EHT_NUM_SOUNDING_DIM_160MHZ,
				 eht_cap->num_sounding_dim_160mhz))
			mlme_eht_cap->num_sounding_dim_160mhz =
				eht_cap->num_sounding_dim_160mhz;
		if (cfg_in_range(CFG_EHT_NUM_SOUNDING_DIM_320MHZ,
				 eht_cap->num_sounding_dim_320mhz))
			mlme_eht_cap->num_sounding_dim_320mhz =
				eht_cap->num_sounding_dim_320mhz;
		mlme_eht_cap->mu_bformer_le_80mhz =
			eht_cap->mu_bformer_le_80mhz;
		mlme_eht_cap->mu_bformer_160mhz = eht_cap->mu_bformer_160mhz;
		mlme_eht_cap->mu_bformer_320mhz = eht_cap->mu_bformer_320mhz;

	} else {
		mlme_eht_cap->su_beamformer = 0;
		mlme_eht_cap->num_sounding_dim_le_80mhz = 0;
		mlme_eht_cap->num_sounding_dim_160mhz = 0;
		mlme_eht_cap->num_sounding_dim_320mhz = 0;
		mlme_eht_cap->mu_bformer_le_80mhz = 0;
		mlme_eht_cap->mu_bformer_160mhz = 0;
		mlme_eht_cap->mu_bformer_320mhz = 0;
	}

	if (mlme_obj->cfg.vht_caps.vht_cap_info.su_bformee) {
		mlme_eht_cap->su_beamformee = eht_cap->su_beamformee;
		if (cfg_in_range(CFG_EHT_BFEE_SS_LE_80MHZ,
				 eht_cap->bfee_ss_le_80mhz))
			mlme_eht_cap->bfee_ss_le_80mhz =
						eht_cap->bfee_ss_le_80mhz;
		if (cfg_in_range(CFG_EHT_BFEE_SS_160MHZ,
				 eht_cap->bfee_ss_160mhz))
			mlme_eht_cap->bfee_ss_160mhz = eht_cap->bfee_ss_160mhz;
		if (cfg_in_range(CFG_EHT_BFEE_SS_320MHZ,
				 eht_cap->bfee_ss_320mhz))
			mlme_eht_cap->bfee_ss_320mhz = eht_cap->bfee_ss_320mhz;

	} else {
		mlme_eht_cap->su_beamformee = 0;
		mlme_eht_cap->bfee_ss_le_80mhz = 0;
		mlme_eht_cap->bfee_ss_160mhz = 0;
		mlme_eht_cap->bfee_ss_320mhz = 0;
	}
	mlme_obj->cfg.eht_caps.eht_cap_orig =
		mlme_obj->cfg.eht_caps.dot11_eht_cap;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_update_tgt_mlo_caps_in_cfg(struct wlan_objmgr_psoc *psoc)
{
	struct target_psoc_info *tgt_hdl;
	QDF_STATUS status;
	uint16_t value;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_hdl) {
		mlme_debug("target psoc info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	value = target_if_res_cfg_get_num_max_mlo_link(tgt_hdl);
	status = wlan_mlme_set_sta_mlo_conn_max_num(psoc, value);
	mlme_debug("Max ML link supported: %d", value);

	return status;
}

uint8_t wlan_mlme_convert_phy_ch_width_to_eht_op_bw(enum phy_ch_width ch_width)
{
	switch (ch_width) {
	case CH_WIDTH_320MHZ:
		return WLAN_EHT_CHWIDTH_320;
	case CH_WIDTH_160MHZ:
		return WLAN_EHT_CHWIDTH_160;
	case CH_WIDTH_80MHZ:
		return WLAN_EHT_CHWIDTH_80;
	case CH_WIDTH_40MHZ:
		return WLAN_EHT_CHWIDTH_40;
	default:
		return WLAN_EHT_CHWIDTH_20;
	}
}

enum phy_ch_width wlan_mlme_convert_eht_op_bw_to_phy_ch_width(
						uint8_t channel_width)
{
	enum phy_ch_width phy_bw = CH_WIDTH_20MHZ;

	if (channel_width == WLAN_EHT_CHWIDTH_320)
		phy_bw = CH_WIDTH_320MHZ;
	else if (channel_width == WLAN_EHT_CHWIDTH_160)
		phy_bw = CH_WIDTH_160MHZ;
	else if (channel_width == WLAN_EHT_CHWIDTH_80)
		phy_bw = CH_WIDTH_80MHZ;
	else if (channel_width == WLAN_EHT_CHWIDTH_40)
		phy_bw = CH_WIDTH_40MHZ;

	return phy_bw;
}

bool wlan_mlme_get_epcs_capability(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return true;

	return mlme_obj->cfg.sta.epcs_capability;
}

void wlan_mlme_set_epcs_capability(struct wlan_objmgr_psoc *psoc, bool flag)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return;

	mlme_debug("set mlme epcs capability from %d to %d",
		   mlme_obj->cfg.sta.epcs_capability, flag);
	mlme_obj->cfg.sta.epcs_capability = flag;
}

bool wlan_mlme_get_eht_disable_punct_in_us_lpi(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.sta.eht_disable_punct_in_us_lpi;
}

void wlan_mlme_set_eht_disable_punct_in_us_lpi(struct wlan_objmgr_psoc *psoc, bool flag)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return;

	mlme_debug("set mlme epcs capability to %d", flag);
	mlme_obj->cfg.sta.eht_disable_punct_in_us_lpi = flag;
}

bool wlan_mlme_get_usr_disable_sta_eht(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return true;

	return mlme_obj->cfg.sta.usr_disable_eht;
}

void wlan_mlme_set_usr_disable_sta_eht(struct wlan_objmgr_psoc *psoc,
				       bool disable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return;

	mlme_debug("set usr_disable_eht from %d to %d",
		   mlme_obj->cfg.sta.usr_disable_eht, disable);
	mlme_obj->cfg.sta.usr_disable_eht = disable;
}

enum phy_ch_width wlan_mlme_get_max_bw(void)
{
	uint32_t max_bw = wma_get_eht_ch_width();

	if (max_bw == WNI_CFG_EHT_CHANNEL_WIDTH_320MHZ)
		return CH_WIDTH_320MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		return CH_WIDTH_160MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
		return CH_WIDTH_80P80MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
		return CH_WIDTH_80MHZ;
	else
		return CH_WIDTH_40MHZ;
}
#else
enum phy_ch_width wlan_mlme_get_max_bw(void)
{
	uint32_t max_bw = wma_get_vht_ch_width();

	if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		return CH_WIDTH_160MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
		return CH_WIDTH_80P80MHZ;
	else if (max_bw == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
		return CH_WIDTH_80MHZ;
	else
		return CH_WIDTH_40MHZ;
}
#endif

QDF_STATUS wlan_mlme_get_sta_ch_width(struct wlan_objmgr_vdev *vdev,
				      enum phy_ch_width *ch_width)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct wlan_objmgr_peer *peer;
	enum wlan_phymode phymode;
	enum QDF_OPMODE op_mode;

	peer = wlan_vdev_get_bsspeer(vdev);
	op_mode = wlan_vdev_mlme_get_opmode(vdev);

	if (ch_width && peer &&
	    (op_mode == QDF_STA_MODE ||
	     op_mode == QDF_P2P_CLIENT_MODE)) {
		wlan_peer_obj_lock(peer);
		phymode = wlan_peer_get_phymode(peer);
		wlan_peer_obj_unlock(peer);
		*ch_width = wlan_mlme_get_ch_width_from_phymode(phymode);
		status = QDF_STATUS_SUCCESS;
	}

	return  status;
}

void
wlan_mlme_set_bt_profile_con(struct wlan_objmgr_psoc *psoc,
			     bool bt_profile_con)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	mlme_obj->cfg.gen.bt_profile_con = bt_profile_con;
}

bool
wlan_mlme_get_bt_profile_con(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.gen.bt_profile_con;
}

#ifdef WLAN_FEATURE_11BE_MLO
uint8_t wlan_mlme_get_sta_mlo_simultaneous_links(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;

	return mlme_obj->cfg.sta.mlo_max_simultaneous_links;
}

QDF_STATUS
wlan_mlme_set_sta_mlo_simultaneous_links(struct wlan_objmgr_psoc *psoc,
					 uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.sta.mlo_max_simultaneous_links = value;
	mlme_legacy_debug("mlo_max_simultaneous_links %d", value);

	return QDF_STATUS_SUCCESS;
}

uint8_t wlan_mlme_get_sta_mlo_conn_max_num(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;

	return mlme_obj->cfg.sta.mlo_support_link_num;
}

QDF_STATUS wlan_mlme_set_sta_mlo_conn_max_num(struct wlan_objmgr_psoc *psoc,
					      uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct target_psoc_info *tgt_hdl;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_hdl) {
		mlme_err("target psoc info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!value)
		mlme_obj->cfg.sta.mlo_support_link_num =
				target_if_res_cfg_get_num_max_mlo_link(tgt_hdl);
	else
		mlme_obj->cfg.sta.mlo_support_link_num = value;

	mlme_legacy_debug("mlo_support_link_num user input %d intersected value :%d",
			  value, mlme_obj->cfg.sta.mlo_support_link_num);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_user_set_link_num(struct wlan_objmgr_psoc *psoc,
					   uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.sta.user_set_link_num = value;
	mlme_legacy_debug("user_set_link_num %d", value);

	return QDF_STATUS_SUCCESS;
}

void wlan_mlme_set_ml_link_control_mode(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_objmgr_vdev *vdev;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev)
		return;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		mlme_legacy_debug("not mlo vdev");
		goto release_ref;
	}

	if (!vdev->mlo_dev_ctx || !vdev->mlo_dev_ctx->sta_ctx) {
		mlme_legacy_debug("mlo dev/sta ctx is null");
		goto release_ref;
	}

	vdev->mlo_dev_ctx->sta_ctx->ml_link_control_mode = value;
	mlme_legacy_debug("set ml_link_control_mode %d", value);

release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return;
}

uint8_t wlan_mlme_get_ml_link_control_mode(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_objmgr_vdev *vdev;
	uint8_t value = 0;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_SB_ID);
	if (!vdev)
		return 0;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		mlme_legacy_debug("not mlo vdev");
		goto release_ref;
	}

	if (!vdev->mlo_dev_ctx || !vdev->mlo_dev_ctx->sta_ctx) {
		mlme_legacy_debug("mlo dev/sta ctx is null");
		goto release_ref;
	}

	value = vdev->mlo_dev_ctx->sta_ctx->ml_link_control_mode;
	mlme_legacy_debug("get ml_link_control_mode %d", value);
release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return value;
}

void wlan_mlme_restore_user_set_link_num(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	if (!mlme_obj->cfg.sta.user_set_link_num)
		return;

	mlme_obj->cfg.sta.mlo_support_link_num =
				mlme_obj->cfg.sta.user_set_link_num;
	mlme_legacy_debug("restore mlo_support_link_num %d",
			  mlme_obj->cfg.sta.user_set_link_num);
}

void wlan_mlme_clear_user_set_link_num(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	mlme_obj->cfg.sta.user_set_link_num = 0;
}

uint8_t wlan_mlme_get_sta_mlo_conn_band_bmp(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;

	return mlme_obj->cfg.sta.mlo_support_link_band;
}

QDF_STATUS wlan_mlme_set_sta_mlo_conn_band_bmp(struct wlan_objmgr_psoc *psoc,
					       uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.sta.mlo_support_link_band = value;
	mlme_legacy_debug("mlo_support_link_conn band %d", value);

	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_is_5gl_5gh_mlsr_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;
	return mlme_obj->cfg.sta.mlo_5gl_5gh_mlsr;
}

void
wlan_mlme_get_mlo_prefer_percentage(struct wlan_objmgr_psoc *psoc,
				    int8_t *mlo_prefer_percentage)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("invalid mlo object");
		return;
	}

	*mlo_prefer_percentage = mlme_obj->cfg.sta.mlo_prefer_percentage;
	mlme_legacy_debug("mlo_prefer_percentage %d", *mlo_prefer_percentage);
}

bool wlan_mlme_get_sta_same_link_mld_addr(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.sta.mlo_same_link_mld_address;
}
#endif

QDF_STATUS wlan_mlme_get_num_11b_tx_chains(struct wlan_objmgr_psoc *psoc,
					   uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.num_11b_tx_chains;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bt_chain_separation_flag(struct wlan_objmgr_psoc *psoc,
						  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.enable_bt_chain_separation;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_num_11ag_tx_chains(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.num_11ag_tx_chains;
	return QDF_STATUS_SUCCESS;
}


static
bool wlan_mlme_configure_chain_mask_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wma_caps_per_phy non_dbs_phy_cap = {0};
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);
	QDF_STATUS status;
	bool as_enabled, enable_bt_chain_sep, enable2x2;
	uint8_t dual_mac_feature;
	bool hw_dbs_2x2_cap;

	if (!mlme_obj)
		return false;

	status = wma_get_caps_for_phyidx_hwmode(&non_dbs_phy_cap,
						HW_MODE_DBS_NONE,
						CDS_BAND_ALL);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_legacy_err("couldn't get phy caps. skip chain mask programming");
		return false;
	}

	if (non_dbs_phy_cap.tx_chain_mask_2G < 3 ||
	    non_dbs_phy_cap.rx_chain_mask_2G < 3 ||
	    non_dbs_phy_cap.tx_chain_mask_5G < 3 ||
	    non_dbs_phy_cap.rx_chain_mask_5G < 3) {
		mlme_legacy_debug("firmware not capable. skip chain mask programming");
		return false;
	}

	enable_bt_chain_sep =
			mlme_obj->cfg.chainmask_cfg.enable_bt_chain_separation;
	as_enabled = mlme_obj->cfg.gen.as_enabled;
	ucfg_policy_mgr_get_dual_mac_feature(psoc, &dual_mac_feature);

	hw_dbs_2x2_cap = policy_mgr_is_hw_dbs_2x2_capable(psoc);
	enable2x2 = mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2;

	if ((enable2x2 && !enable_bt_chain_sep) || as_enabled ||
	   (!hw_dbs_2x2_cap && (dual_mac_feature != DISABLE_DBS_CXN_AND_SCAN) &&
	    enable2x2)) {
		mlme_legacy_debug("Cannot configure chainmask enable_bt_chain_sep %d as_enabled %d enable2x2 %d hw_dbs_2x2_cap %d dual_mac_feature %d",
				  enable_bt_chain_sep, as_enabled, enable2x2,
				  hw_dbs_2x2_cap, dual_mac_feature);
		return false;
	}

	return true;
}

bool wlan_mlme_is_chain_mask_supported(struct wlan_objmgr_psoc *psoc)

{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return false;

	if (!wlan_mlme_configure_chain_mask_supported(psoc))
		return false;

	/* If user has configured 1x1 from INI */
	if (mlme_obj->cfg.chainmask_cfg.txchainmask1x1 != 3 ||
	    mlme_obj->cfg.chainmask_cfg.rxchainmask1x1 != 3) {
		mlme_legacy_debug("txchainmask1x1 %d rxchainmask1x1 %d",
				  mlme_obj->cfg.chainmask_cfg.txchainmask1x1,
				  mlme_obj->cfg.chainmask_cfg.rxchainmask1x1);
		return false;
	}

	return true;

}

#define MAX_PDEV_CHAIN_MASK_PARAMS 6
/* params being sent:
 * wmi_pdev_param_tx_chain_mask
 * wmi_pdev_param_rx_chain_mask
 * wmi_pdev_param_tx_chain_mask_2g
 * wmi_pdev_param_rx_chain_mask_2g
 * wmi_pdev_param_tx_chain_mask_5g
 * wmi_pdev_param_rx_chain_mask_5g
 */
QDF_STATUS wlan_mlme_configure_chain_mask(struct wlan_objmgr_psoc *psoc,
					  uint8_t session_id)
{
	QDF_STATUS ret_val = QDF_STATUS_E_FAILURE;
	uint8_t ch_msk_val;
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);
	bool mrc_disabled_2g_rx, mrc_disabled_2g_tx;
	bool mrc_disabled_5g_rx, mrc_disabled_5g_tx;
	struct dev_set_param setparam[MAX_PDEV_CHAIN_MASK_PARAMS];
	uint8_t index = 0;

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_legacy_debug("txchainmask1x1: %d rxchainmask1x1: %d",
			  mlme_obj->cfg.chainmask_cfg.txchainmask1x1,
			  mlme_obj->cfg.chainmask_cfg.rxchainmask1x1);
	mlme_legacy_debug("tx_chain_mask_2g: %d, rx_chain_mask_2g: %d",
			  mlme_obj->cfg.chainmask_cfg.tx_chain_mask_2g,
			  mlme_obj->cfg.chainmask_cfg.rx_chain_mask_2g);
	mlme_legacy_debug("tx_chain_mask_5g: %d, rx_chain_mask_5g: %d",
			  mlme_obj->cfg.chainmask_cfg.tx_chain_mask_5g,
			  mlme_obj->cfg.chainmask_cfg.rx_chain_mask_5g);

	mrc_disabled_2g_rx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_rx_mrc[NSS_CHAINS_BAND_2GHZ];
	mrc_disabled_2g_tx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_tx_mrc[NSS_CHAINS_BAND_2GHZ];
	mrc_disabled_5g_rx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_rx_mrc[NSS_CHAINS_BAND_5GHZ];
	mrc_disabled_5g_tx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_tx_mrc[NSS_CHAINS_BAND_5GHZ];

	mlme_legacy_debug("MRC values TX:- 2g %d 5g %d RX:- 2g %d 5g %d",
			  mrc_disabled_2g_tx, mrc_disabled_5g_tx,
			  mrc_disabled_2g_rx, mrc_disabled_5g_rx);

	if (!wlan_mlme_configure_chain_mask_supported(psoc))
		return QDF_STATUS_E_FAILURE;

	if (mlme_obj->cfg.chainmask_cfg.txchainmask1x1) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.txchainmask1x1;
		if (wma_validate_txrx_chain_mask(wmi_pdev_param_tx_chain_mask,
						 ch_msk_val)) {
			goto error;
		}
		ret_val = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_tx_chain_mask,
					      ch_msk_val, index++,
					      MAX_PDEV_CHAIN_MASK_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret_val)) {
			mlme_err("failed at  wmi_pdev_param_tx_chain_mask");
			goto error;
		}
	}

	if (mlme_obj->cfg.chainmask_cfg.rxchainmask1x1) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.rxchainmask1x1;
		if (wma_validate_txrx_chain_mask(wmi_pdev_param_rx_chain_mask,
								ch_msk_val)) {
			goto error;
		}
		ret_val = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_rx_chain_mask,
					      ch_msk_val, index++,
					      MAX_PDEV_CHAIN_MASK_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret_val)) {
			mlme_err("failed at wmi_pdev_param_rx_chain_mask");
			goto error;
		}
	}

	if (mlme_obj->cfg.chainmask_cfg.txchainmask1x1 ||
	    mlme_obj->cfg.chainmask_cfg.rxchainmask1x1) {
		mlme_legacy_debug("band agnostic tx/rx chain mask set. skip per band chain mask");
		goto sendparam;
	}

	if (mlme_obj->cfg.chainmask_cfg.tx_chain_mask_2g &&
	    mrc_disabled_2g_tx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_2g;
		ret_val = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_tx_chain_mask_2g,
					      ch_msk_val, index++,
					      MAX_PDEV_CHAIN_MASK_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret_val)) {
			mlme_err("failed at  wmi_pdev_param_tx_chain_mask_2g");
			goto error;
		}
	}

	if (mlme_obj->cfg.chainmask_cfg.rx_chain_mask_2g &&
	    mrc_disabled_2g_rx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.rx_chain_mask_2g;
		ret_val = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_rx_chain_mask_2g,
					      ch_msk_val, index++,
					      MAX_PDEV_CHAIN_MASK_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret_val)) {
			mlme_err("failed at wmi_pdev_param_rx_chain_mask_2g");
			goto error;
		}
	}

	if (mlme_obj->cfg.chainmask_cfg.tx_chain_mask_5g &&
	    mrc_disabled_5g_tx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_5g;
		ret_val = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_tx_chain_mask_5g,
					      ch_msk_val, index++,
					      MAX_PDEV_CHAIN_MASK_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret_val)) {
			mlme_err("failed at  wmi_pdev_param_tx_chain_mask_5g");
			goto error;
		}
	}

	if (mlme_obj->cfg.chainmask_cfg.rx_chain_mask_5g &&
	    mrc_disabled_5g_rx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.rx_chain_mask_5g;
		ret_val = mlme_check_index_setparam(
					      setparam,
					      wmi_pdev_param_rx_chain_mask_5g,
					      ch_msk_val, index++,
					      MAX_PDEV_CHAIN_MASK_PARAMS);
		if (QDF_IS_STATUS_ERROR(ret_val)) {
			mlme_err("failed at wmi_pdev_param_rx_chain_mask_5g");
			goto error;
		}
	}
sendparam:
	ret_val = wma_send_multi_pdev_vdev_set_params(MLME_PDEV_SETPARAM,
						      WMI_PDEV_ID_SOC, setparam,
						      index);
	if (QDF_IS_STATUS_ERROR(ret_val))
		mlme_err("failed to send chainmask params");
error:
	return ret_val;
}

QDF_STATUS
wlan_mlme_get_manufacturer_name(struct wlan_objmgr_psoc *psoc,
				uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			      mlme_obj->cfg.product_details.manufacturer_name,
			      *plen);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_model_number(struct wlan_objmgr_psoc *psoc,
			   uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			      mlme_obj->cfg.product_details.model_number,
			      *plen);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_model_name(struct wlan_objmgr_psoc *psoc,
			 uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			      mlme_obj->cfg.product_details.model_name,
			      *plen);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_manufacture_product_version(struct wlan_objmgr_psoc *psoc,
					  uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
		     mlme_obj->cfg.product_details.manufacture_product_version,
		     *plen);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_manufacture_product_name(struct wlan_objmgr_psoc *psoc,
				       uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			mlme_obj->cfg.product_details.manufacture_product_name,
			*plen);
	return QDF_STATUS_SUCCESS;
}


void wlan_mlme_get_tl_delayed_trgr_frm_int(struct wlan_objmgr_psoc *psoc,
					   uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_TL_DELAYED_TRGR_FRM_INTERVAL);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.delayed_trigger_frm_int;
}


QDF_STATUS wlan_mlme_get_wmm_dir_ac_vo(struct wlan_objmgr_psoc *psoc,
				       uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.dir_ac_vo;

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vo(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.nom_msdu_size_ac_vo;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.mean_data_rate_ac_vo;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.min_phy_rate_ac_vo;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_sba_ac_vo(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.wmm_params.ac_vo.sba_ac_vo;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_vo_srv_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.uapsd_vo_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_vo_sus_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.uapsd_vo_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_vht_ampdu_len_exp(struct wlan_objmgr_psoc *psoc,
					       uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.ampdu_len_exponent;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_vht_max_mpdu_len(struct wlan_objmgr_psoc *psoc,
					      uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.ampdu_len;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_ht_smps(struct wlan_objmgr_psoc *psoc,
				     uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.ht_caps.smps;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_dir_ac_vi(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.dir_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vi(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value =
		mlme_obj->cfg.wmm_params.ac_vi.nom_msdu_size_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.mean_data_rate_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.min_phy_rate_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_sba_ac_vi(struct wlan_objmgr_psoc *psoc,
					uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.sba_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_vi_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.uapsd_vi_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_vi_sus_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.uapsd_vi_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_dir_ac_be(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.dir_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_be(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.nom_msdu_size_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_be(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.mean_data_rate_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.min_phy_rate_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_sba_ac_be(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.sba_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_be_srv_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.uapsd_be_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_be_sus_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.uapsd_be_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_dir_ac_bk(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.dir_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_bk(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.nom_msdu_size_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.mean_data_rate_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.min_phy_rate_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_sba_ac_bk(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.sba_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.uapsd_bk_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.uapsd_bk_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mode(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_config.wmm_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_80211e_is_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_config.b80211e_is_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_mask(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_config.uapsd_mask;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
void wlan_mlme_get_inactivity_interval(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_QOS_WMM_INACTIVITY_INTERVAL);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.inactivity_intv;
}
#endif

void wlan_mlme_get_is_ts_burst_size_enable(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_QOS_WMM_BURST_SIZE_DEFN);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.burst_size_def;
}

void wlan_mlme_get_ts_info_ack_policy(struct wlan_objmgr_psoc *psoc,
				      enum mlme_ts_info_ack_policy *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_QOS_WMM_TS_INFO_ACK_POLICY);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.ts_ack_policy;

}

QDF_STATUS
wlan_mlme_get_ts_acm_value_for_ac(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.ts_acm_is_off;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.listen_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (cfg_in_range(CFG_LISTEN_INTERVAL, value))
		mlme_obj->cfg.sap_cfg.listen_interval = value;
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (cfg_in_range(CFG_ASSOC_STA_LIMIT, value) &&
	    (value <= mlme_obj->cfg.sap_cfg.sap_max_no_peers))
		mlme_obj->cfg.sap_cfg.assoc_sta_limit = value;
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.assoc_sta_limit;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_get_peer_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.sap_cfg.sap_get_peer_info = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sap_bcast_deauth_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.is_sap_bcast_deauth_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_6g_sap_fd_enabled(struct wlan_objmgr_psoc *psoc,
			       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.is_6g_sap_fd_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_allow_all_channels(struct wlan_objmgr_psoc *psoc,
						bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_allow_all_chan_param_name;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_max_no_peers;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (cfg_in_range(CFG_SAP_MAX_NO_PEERS, value))
		mlme_obj->cfg.sap_cfg.sap_max_no_peers = value;
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_offload_peers(struct wlan_objmgr_psoc *psoc,
					       int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_max_offload_peers;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_offload_reorder_buffs(struct wlan_objmgr_psoc
						       *psoc, int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_max_offload_reorder_buffs;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chn_switch_bcn_count(struct wlan_objmgr_psoc *psoc,
						  int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_ch_switch_beacon_cnt;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chn_switch_mode(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_ch_switch_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_internal_restart(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_internal_restart;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_modulated_dtim(struct wlan_objmgr_psoc *psoc,
						uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.max_li_modulated_dtim_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chan_pref_location(struct wlan_objmgr_psoc *psoc,
						uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_pref_chan_location;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_country_priority(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.country_code_priority;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_reduced_beacon_interval(struct wlan_objmgr_psoc
						     *psoc, int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_REDUCED_BEACON_INTERVAL);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.sap_cfg.reduced_beacon_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chan_switch_rate_enabled(struct wlan_objmgr_psoc
						      *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.chan_switch_hostapd_rate_enabled_name;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_force_11n_for_11ac(struct wlan_objmgr_psoc
						*psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_force_11n_for_11ac;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_go_force_11n_for_11ac(struct wlan_objmgr_psoc
					       *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.go_force_11n_for_11ac;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_SAP_11AC_OVERRIDE);
		return QDF_STATUS_E_FAILURE;
	}
	*value = mlme_obj->cfg.sap_cfg.sap_11ac_override;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					 bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_GO_11AC_OVERRIDE);
		return QDF_STATUS_E_FAILURE;
	}
	*value = mlme_obj->cfg.sap_cfg.go_11ac_override;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;
	mlme_obj->cfg.sap_cfg.sap_11ac_override = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;
	mlme_obj->cfg.sap_cfg.go_11ac_override = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bigtk_support(struct wlan_objmgr_psoc *psoc,
				       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.bigtk_support;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_ocv_support(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.ocv_support;

	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_get_host_scan_abort_support(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.gen.stop_all_host_scan_support;
}

bool wlan_mlme_get_dual_sta_roam_support(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.gen.dual_sta_roam_fw_support;
}

QDF_STATUS wlan_mlme_get_oce_sta_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.oce.oce_sta_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_oce_sap_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.oce.oce_sap_enabled;
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_mlme_send_oce_flags_fw() - Send the oce flags to FW
 * @pdev: pointer to pdev object
 * @object: vdev object
 * @arg: Arguments to the handler
 *
 * Return: void
 */
static void wlan_mlme_send_oce_flags_fw(struct wlan_objmgr_pdev *pdev,
					void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = object;
	uint8_t *updated_fw_value = arg;
	uint8_t *dynamic_fw_value = 0;
	uint8_t vdev_id;

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		dynamic_fw_value = mlme_get_dynamic_oce_flags(vdev);
		if (!dynamic_fw_value)
			return;
		if (*updated_fw_value == *dynamic_fw_value) {
			mlme_legacy_debug("Current FW flags matches with updated value.");
			return;
		}
		*dynamic_fw_value = *updated_fw_value;
		vdev_id = wlan_vdev_get_id(vdev);
		if (wma_cli_set_command(vdev_id,
					wmi_vdev_param_enable_disable_oce_features,
					*updated_fw_value, VDEV_CMD))
			mlme_legacy_err("Failed to send OCE update to FW");
	}
}

void wlan_mlme_update_oce_flags(struct wlan_objmgr_pdev *pdev)
{
	uint16_t sap_connected_peer, go_connected_peer;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	uint8_t updated_fw_value = 0;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return;

	sap_connected_peer =
	wlan_util_get_peer_count_for_mode(pdev, QDF_SAP_MODE);
	go_connected_peer =
	wlan_util_get_peer_count_for_mode(pdev, QDF_P2P_GO_MODE);
	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return;

	if (sap_connected_peer || go_connected_peer) {
		updated_fw_value = mlme_obj->cfg.oce.feature_bitmap;
		updated_fw_value &=
		~(WMI_VDEV_OCE_PROBE_REQUEST_RATE_FEATURE_BITMAP);
		updated_fw_value &=
		~(WMI_VDEV_OCE_PROBE_REQUEST_DEFERRAL_FEATURE_BITMAP);
		mlme_legacy_debug("Disable STA OCE probe req rate and defferal updated_fw_value :%d",
				  updated_fw_value);
	} else {
		updated_fw_value = mlme_obj->cfg.oce.feature_bitmap;
		mlme_legacy_debug("Update the STA OCE flags to default INI updated_fw_value :%d",
				  updated_fw_value);
	}

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
				wlan_mlme_send_oce_flags_fw,
				&updated_fw_value, 0, WLAN_MLME_NB_ID);
}

bool wlan_mlme_is_ap_prot_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.sap_protection_cfg.is_ap_prot_enabled;
}

QDF_STATUS wlan_mlme_get_ap_protection_mode(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_protection_cfg.ap_protection_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_ap_obss_prot_enabled(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_protection_cfg.enable_ap_obss_protection;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.threshold.rts_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle)
		return QDF_STATUS_E_INVAL;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.threshold.rts_threshold = value;
	wma_update_rts_params(wma_handle, value);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.threshold.frag_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle)
		return QDF_STATUS_E_INVAL;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.threshold.frag_threshold = value;
	wma_update_frag_params(wma_handle,
			       value);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.oce.fils_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.oce.fils_enabled = value;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_primary_interface(struct wlan_objmgr_psoc *psoc,
					   uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.dual_sta_policy.primary_vdev_id = value;
	mlme_debug("Set primary iface to :%d", value);

	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_is_primary_interface_configured(struct wlan_objmgr_psoc *psoc)
{
	return wlan_cm_same_band_sta_allowed(psoc);
}

QDF_STATUS wlan_mlme_peer_get_assoc_rsp_ies(struct wlan_objmgr_peer *peer,
					    const uint8_t **ie_buf,
					    size_t *ie_len)
{
	struct peer_mlme_priv_obj *peer_priv;

	if (!peer || !ie_buf || !ie_len)
		return QDF_STATUS_E_INVAL;

	*ie_buf = NULL;
	*ie_len = 0;

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);

	if (!peer_priv || peer_priv->assoc_rsp.len == 0)
		return QDF_STATUS_SUCCESS;

	*ie_buf = peer_priv->assoc_rsp.ptr;
	*ie_len = peer_priv->assoc_rsp.len;

	return QDF_STATUS_SUCCESS;
}

int wlan_mlme_get_mcc_duty_cycle_percentage(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t i, operating_channel, quota_value = MCC_DUTY_CYCLE;
	struct dual_sta_policy *dual_sta_policy;
	uint32_t count, primary_sta_freq = 0, secondary_sta_freq = 0;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return -EINVAL;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return -EINVAL;
	dual_sta_policy  = &mlme_obj->cfg.gen.dual_sta_policy;

	if (dual_sta_policy->primary_vdev_id == WLAN_INVALID_VDEV_ID ||
	    (dual_sta_policy->concurrent_sta_policy ==
	     QCA_WLAN_CONCURRENT_STA_POLICY_UNBIASED)) {
		mlme_debug("Invalid primary vdev id or policy is unbaised :%d",
			   dual_sta_policy->concurrent_sta_policy);
		return -EINVAL;
	}

	count = policy_mgr_get_mode_specific_conn_info(psoc, op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);

	/* Proceed only in case of STA+STA */
	if (count != 2) {
		mlme_debug("STA+STA concurrency is not present");
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		if (vdev_id_list[i] == dual_sta_policy->primary_vdev_id) {
			primary_sta_freq = op_ch_freq_list[i];
			mlme_debug("primary sta vdev:%d at inxex:%d, freq:%d",
				   i, vdev_id_list[i], op_ch_freq_list[i]);
		} else {
			secondary_sta_freq = op_ch_freq_list[i];
			mlme_debug("secondary sta vdev:%d at inxex:%d, freq:%d",
				   i, vdev_id_list[i], op_ch_freq_list[i]);
		}
	}

	if (!primary_sta_freq || !secondary_sta_freq) {
		mlme_debug("Invalid primary or secondary sta freq");
		return -EINVAL;
	}

	operating_channel = wlan_freq_to_chan(primary_sta_freq);

	/*
	 * The channel numbers for both adapters and the time
	 * quota for the 1st adapter, i.e., one specified in cmd
	 * are formatted as a bit vector
	 * ******************************************************
	 * |bit 31-24  | bit 23-16 |  bits 15-8  |bits 7-0   |
	 * |  Unused   | Quota for | chan. # for |chan. # for|
	 * |           |  1st chan | 1st chan.   |2nd chan.  |
	 * ******************************************************
	 */
	mlme_debug("First connection channel No.:%d and quota:%dms",
		   operating_channel, quota_value);
	/* Move the time quota for first channel to bits 15-8 */
	quota_value = quota_value << 8;
	/*
	 * Store the channel number of 1st channel at bits 7-0
	 * of the bit vector
	 */
	quota_value |= operating_channel;
		/* Second STA Connection */
	operating_channel = wlan_freq_to_chan(secondary_sta_freq);
	if (!operating_channel)
		mlme_debug("Secondary adapter op channel is invalid");
	/*
	 * Now move the time quota and channel number of the
	 * 1st adapter to bits 23-16 and bits 15-8 of the bit
	 * vector, respectively.
	 */
	quota_value = quota_value << 8;
	/*
	 * Set the channel number for 2nd MCC vdev at bits
	 * 7-0 of set_value
	 */
	quota_value |= operating_channel;
	mlme_debug("quota value:%x", quota_value);

	return quota_value;
}

QDF_STATUS wlan_mlme_set_enable_bcast_probe_rsp(struct wlan_objmgr_psoc *psoc,
						bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.oce.enable_bcast_probe_rsp = value;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sta_miracast_mcc_rest_time(struct wlan_objmgr_psoc *psoc,
					 uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sta.sta_miracast_mcc_rest_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_max_modulated_dtim_ms(struct wlan_objmgr_psoc *psoc,
				    uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sta.max_li_modulated_dtim_time_ms;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_scan_probe_unicast_ra(struct wlan_objmgr_psoc *psoc,
				    bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sta.usr_scan_probe_unicast_ra;

	mlme_legacy_debug("scan_probe_unicast_ra %d", *value);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_scan_probe_unicast_ra(struct wlan_objmgr_psoc *psoc,
				    bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_legacy_debug("scan_probe_unicast_ra %d", value);
	mlme_obj->cfg.sta.usr_scan_probe_unicast_ra = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sap_mcc_chnl_avoid(struct wlan_objmgr_psoc *psoc,
				 uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_mcc_chnl_avoid;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mcc_bcast_prob_resp(struct wlan_objmgr_psoc *psoc,
				  uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.feature_flags.mcc_bcast_prob_rsp;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mcc_rts_cts_prot(struct wlan_objmgr_psoc *psoc,
			       uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.feature_flags.mcc_rts_cts_prot;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mcc_feature(struct wlan_objmgr_psoc *psoc,
			  uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.feature_flags.enable_mcc;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_edca_params(struct wlan_mlme_edca_params *edca_params,
				     uint8_t *data, enum e_edca_type edca_ac)
{
	qdf_size_t len;

	switch (edca_ac) {
	case edca_ani_acbe_local:
		len = edca_params->ani_acbe_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbe_l, &len);
		break;

	case edca_ani_acbk_local:
		len = edca_params->ani_acbk_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbk_l, &len);
		break;

	case edca_ani_acvi_local:
		len = edca_params->ani_acvi_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvi_l, &len);
		break;

	case edca_ani_acvo_local:
		len = edca_params->ani_acvo_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvo_l, &len);
		break;

	case edca_ani_acbk_bcast:
		len = edca_params->ani_acbk_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbk_b, &len);
		break;

	case edca_ani_acbe_bcast:
		len = edca_params->ani_acbe_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbe_b, &len);
		break;

	case edca_ani_acvi_bcast:
		len = edca_params->ani_acvi_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvi_b, &len);
		break;

	case edca_ani_acvo_bcast:
		len = edca_params->ani_acvo_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvo_b, &len);
		break;

	case edca_wme_acbe_local:
		len = edca_params->wme_acbe_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbe_l, &len);
		break;

	case edca_wme_acbk_local:
		len = edca_params->wme_acbk_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbk_l, &len);
		break;

	case edca_wme_acvi_local:
		len = edca_params->wme_acvi_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvi_l, &len);
		break;

	case edca_wme_acvo_local:
		len = edca_params->wme_acvo_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvo_l, &len);
		break;

	case edca_wme_acbe_bcast:
		len = edca_params->wme_acbe_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbe_b, &len);
		break;

	case edca_wme_acbk_bcast:
		len = edca_params->wme_acbk_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbk_b, &len);
		break;

	case edca_wme_acvi_bcast:
		len = edca_params->wme_acvi_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvi_b, &len);
		break;

	case edca_wme_acvo_bcast:
		len = edca_params->wme_acvo_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvo_b, &len);
		break;

	case edca_etsi_acbe_local:
		len = edca_params->etsi_acbe_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbe_l, &len);
		break;

	case edca_etsi_acbk_local:
		len = edca_params->etsi_acbk_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbk_l, &len);
		break;

	case edca_etsi_acvi_local:
		len = edca_params->etsi_acvi_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvi_l, &len);
		break;

	case edca_etsi_acvo_local:
		len = edca_params->etsi_acvo_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvo_l, &len);
		break;

	case edca_etsi_acbe_bcast:
		len = edca_params->etsi_acbe_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbe_b, &len);
		break;

	case edca_etsi_acbk_bcast:
		len = edca_params->etsi_acbk_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbk_b, &len);
		break;

	case edca_etsi_acvi_bcast:
		len = edca_params->etsi_acvi_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvi_b, &len);
		break;

	case edca_etsi_acvo_bcast:
		len = edca_params->etsi_acvo_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvo_b, &len);
		break;
	default:
		mlme_legacy_err("Invalid edca access category");
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_wep_key(struct wlan_objmgr_vdev *vdev,
			    struct wlan_mlme_wep_cfg *wep_params,
			    enum wep_key_id wep_keyid, uint8_t *default_key,
			    qdf_size_t *key_len)
{
	struct wlan_crypto_key *crypto_key = NULL;

	if (wep_keyid >= WLAN_CRYPTO_MAXKEYIDX) {
		mlme_legacy_err("Incorrect wep key index %d", wep_keyid);
		return QDF_STATUS_E_INVAL;
	}
	crypto_key = wlan_crypto_get_key(vdev, wep_keyid);
	if (!crypto_key) {
		mlme_legacy_err("Crypto KEY not present");
		return QDF_STATUS_E_INVAL;
	}

	if (crypto_key->keylen > WLAN_CRYPTO_KEY_WEP104_LEN) {
		mlme_legacy_err("Key too large to hold");
		return QDF_STATUS_E_INVAL;
	}
	*key_len = crypto_key->keylen;
	qdf_mem_copy(default_key, &crypto_key->keyval, crypto_key->keylen);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_11h_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.enabled_11h;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_11h_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.enabled_11h = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_11d_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.enabled_11d;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_11d_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.enabled_11d = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = false;
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.gen.enabled_rf_test_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.enabled_rf_test_mode = value;

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_BAND_6GHZ
QDF_STATUS
wlan_mlme_is_standard_6ghz_conn_policy_enabled(struct wlan_objmgr_psoc *psoc,
					       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.std_6ghz_conn_policy;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_disable_vlp_sta_conn_to_sp_ap_enabled(
						struct wlan_objmgr_psoc *psoc,
						bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.disable_vlp_sta_conn_to_sp_ap;

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
QDF_STATUS
wlan_mlme_get_eht_mode(struct wlan_objmgr_psoc *psoc, enum wlan_eht_mode *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.eht_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_emlsr_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.enable_emlsr_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_eht_mode(struct wlan_objmgr_psoc *psoc, enum wlan_eht_mode value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.eht_mode = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_emlsr_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.enable_emlsr_mode = value;

	return QDF_STATUS_SUCCESS;
}

void
wlan_mlme_set_eml_params(struct wlan_objmgr_psoc *psoc,
			 struct wlan_psoc_host_mac_phy_caps_ext2 *cap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	if (!cap->emlcap.emlsr_supp) {
		mlme_legacy_debug("EMLSR supp: %d", cap->emlcap.emlsr_supp);
		return;
	}

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("No psoc object");
		return;
	}
	mlme_obj->cfg.eml_cap.emlsr_supp = cap->emlcap.emlsr_supp;
	mlme_obj->cfg.eml_cap.emlsr_pad_delay = cap->emlcap.emlsr_pad_delay;
	mlme_obj->cfg.eml_cap.emlsr_trans_delay = cap->emlcap.emlsr_trans_delay;
	mlme_obj->cfg.eml_cap.emlmr_supp = cap->emlcap.emlmr_supp;
}

void
wlan_mlme_get_eml_params(struct wlan_objmgr_psoc *psoc,
			 struct wlan_mlo_eml_cap *cap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("No psoc object");
		return;
	}
	cap->emlsr_supp = mlme_obj->cfg.eml_cap.emlsr_supp;
	cap->emlsr_pad_delay = mlme_obj->cfg.eml_cap.emlsr_pad_delay;
	cap->emlsr_trans_delay = mlme_obj->cfg.eml_cap.emlsr_trans_delay;
	cap->emlmr_supp = mlme_obj->cfg.eml_cap.emlmr_supp;
}

void
wlan_mlme_cfg_set_emlsr_pad_delay(struct wlan_objmgr_psoc *psoc, uint8_t val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("No psoc object");
		return;
	}

	if (val > mlme_obj->cfg.eml_cap.emlsr_pad_delay &&
	    val <= WLAN_ML_BV_CINFO_EMLCAP_EMLSRDELAY_256US) {
		mlme_obj->cfg.eml_cap.emlsr_pad_delay = val;
		mlme_debug("EMLSR padding delay configured to %d", val);
	}
}

enum t2lm_negotiation_support
wlan_mlme_get_t2lm_negotiation_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return T2LM_NEGOTIATION_DISABLED;

	return mlme_obj->cfg.gen.t2lm_negotiation_support;
}

QDF_STATUS
wlan_mlme_set_t2lm_negotiation_supported(struct wlan_objmgr_psoc *psoc,
					 uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (value > T2LM_NEGOTIATION_MAX) {
		mlme_err("Invalid value %d", value);
		return QDF_STATUS_E_INVAL;
	}

	mlme_obj->cfg.gen.t2lm_negotiation_support = value;

	return QDF_STATUS_SUCCESS;
}

uint8_t
wlan_mlme_get_eht_mld_id(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;

	return mlme_obj->cfg.gen.mld_id;
}

QDF_STATUS
wlan_mlme_set_eht_mld_id(struct wlan_objmgr_psoc *psoc,
			 uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.mld_id = value;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
wlan_mlme_set_btm_abridge_flag(struct wlan_objmgr_psoc *psoc,
			       bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.btm.abridge_flag = value;

	return QDF_STATUS_SUCCESS;
}

bool
wlan_mlme_get_btm_abridge_flag(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.btm.abridge_flag;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_chan_width(struct wlan_objmgr_psoc *psoc, uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.supp_chan_width = value;
	if (value == VHT_CAP_160_AND_80P80_SUPP ||
	    value == VHT_CAP_160_SUPP) {
		mlme_obj->cfg.vht_caps.vht_cap_info.vht_extended_nss_bw_cap = 1;
		mlme_obj->cfg.vht_caps.vht_cap_info.extended_nss_bw_supp = 0;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_chan_width(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.supp_chan_width;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_vht_ldpc_coding_cap(struct wlan_objmgr_psoc *psoc,
						 bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.ldpc_coding_cap = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc,
				   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.short_gi_160mhz = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.short_gi_160mhz;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_tx_stbc(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_stbc;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_stbc;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
					uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.tx_bfee_ant_supp = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
					uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_bfee_ant_supp;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs_map;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs_map = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs_map;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs_map = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_rx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.rx_supp_data_rate = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_tx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.tx_supp_data_rate = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.basic_mcs_set;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.basic_mcs_set = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable_tx_bf(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.su_bformee;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_tx_su_beamformer(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.su_bformer;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_channel_width(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.channel_width;

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS
wlan_mlme_get_vht_rx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_tx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_rx_mcs_2x2(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs2x2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_tx_mcs_2x2(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs2x2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht20_mcs9(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable_vht20_mcs9;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_indoor_support_for_nan(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = false;
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_INVAL;
	}

	*value = mlme_obj->cfg.reg.enable_nan_on_indoor_channels;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_srd_master_mode_for_vdev(struct wlan_objmgr_psoc *psoc,
				       enum QDF_OPMODE vdev_opmode,
				       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = false;
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_INVAL;
	}

	switch (vdev_opmode) {
	case QDF_SAP_MODE:
		*value = mlme_obj->cfg.reg.etsi_srd_chan_in_master_mode &
			 MLME_SRD_MASTER_MODE_SAP;
		break;
	case QDF_P2P_GO_MODE:
		*value = mlme_obj->cfg.reg.etsi_srd_chan_in_master_mode &
			 MLME_SRD_MASTER_MODE_P2P_GO;
		break;
	case QDF_NAN_DISC_MODE:
		*value = mlme_obj->cfg.reg.etsi_srd_chan_in_master_mode &
			 MLME_SRD_MASTER_MODE_NAN;
		break;
	default:
		mlme_legacy_err("Unexpected opmode %d", vdev_opmode);
		*value = false;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_enable_dynamic_nss_chains_cfg(struct wlan_objmgr_psoc *psoc,
					    bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.nss_chains_ini_cfg.enable_dynamic_nss_chains_cfg;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_restart_sap_on_dynamic_nss_chains_cfg(
						struct wlan_objmgr_psoc *psoc,
						bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value =
	mlme_obj->cfg.nss_chains_ini_cfg.restart_sap_on_dyn_nss_chains_cfg;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_dynamic_nss_chains_support(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.dynamic_nss_chains_support;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_dynamic_nss_chains_support(struct wlan_objmgr_psoc *psoc,
					     bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.dynamic_nss_chains_support = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_force_sap_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.force_sap_start;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2 = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable_paid(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable_paid;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable_gid(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable_gid;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.b24ghz_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.b24ghz_band = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vendor_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.vendor_24ghz_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
mlme_update_vht_cap(struct wlan_objmgr_psoc *psoc, struct wma_tgt_vht_cap *cfg)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct mlme_vht_capabilities_info *vht_cap_info;
	uint32_t value = 0;
	bool hw_rx_ldpc_enabled;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vht_cap_info = &mlme_obj->cfg.vht_caps.vht_cap_info;

	/*
	 * VHT max MPDU length:
	 * override if user configured value is too high
	 * that the target cannot support
	 */
	if (vht_cap_info->ampdu_len > cfg->vht_max_mpdu)
		vht_cap_info->ampdu_len = cfg->vht_max_mpdu;
	if (vht_cap_info->ampdu_len >= 1)
		mlme_obj->cfg.ht_caps.ht_cap_info.maximal_amsdu_size = 1;
	value = (CFG_VHT_BASIC_MCS_SET_STADEF & VHT_MCS_1x1) |
		vht_cap_info->basic_mcs_set;
	if (vht_cap_info->enable2x2)
		value = (value & VHT_MCS_2x2) | (vht_cap_info->rx_mcs2x2 << 2);
	vht_cap_info->basic_mcs_set = value;

	value = (CFG_VHT_RX_MCS_MAP_STADEF & VHT_MCS_1x1) |
		 vht_cap_info->rx_mcs;

	if (vht_cap_info->enable2x2)
		value = (value & VHT_MCS_2x2) | (vht_cap_info->rx_mcs2x2 << 2);
	vht_cap_info->rx_mcs_map = value;

	value = (CFG_VHT_TX_MCS_MAP_STADEF & VHT_MCS_1x1) |
		 vht_cap_info->tx_mcs;
	if (vht_cap_info->enable2x2)
		value = (value & VHT_MCS_2x2) | (vht_cap_info->tx_mcs2x2 << 2);
	vht_cap_info->tx_mcs_map = value;

	 /* Set HW RX LDPC capability */
	hw_rx_ldpc_enabled = !!cfg->vht_rx_ldpc;
	if (vht_cap_info->ldpc_coding_cap && !hw_rx_ldpc_enabled)
		vht_cap_info->ldpc_coding_cap = hw_rx_ldpc_enabled;

	/* set the Guard interval 80MHz */
	if (vht_cap_info->short_gi_80mhz && !cfg->vht_short_gi_80)
		vht_cap_info->short_gi_80mhz = cfg->vht_short_gi_80;

	/* Set VHT TX/RX STBC cap */
	if (vht_cap_info->enable2x2) {
		if (vht_cap_info->tx_stbc && !cfg->vht_tx_stbc)
			vht_cap_info->tx_stbc = cfg->vht_tx_stbc;

		if (vht_cap_info->rx_stbc && !cfg->vht_rx_stbc)
			vht_cap_info->rx_stbc = cfg->vht_rx_stbc;
	} else {
		vht_cap_info->tx_stbc = 0;
		vht_cap_info->rx_stbc = 0;
	}

	/* Set VHT SU Beamformer cap */
	if (vht_cap_info->su_bformer && !cfg->vht_su_bformer)
		vht_cap_info->su_bformer = cfg->vht_su_bformer;

	/* check and update SU BEAMFORMEE capabality */
	if (vht_cap_info->su_bformee && !cfg->vht_su_bformee)
		vht_cap_info->su_bformee = cfg->vht_su_bformee;

	/* Set VHT MU Beamformer cap */
	if (vht_cap_info->mu_bformer && !cfg->vht_mu_bformer)
		vht_cap_info->mu_bformer = cfg->vht_mu_bformer;

	/* Set VHT MU Beamformee cap */
	if (vht_cap_info->enable_mu_bformee && !cfg->vht_mu_bformee)
		vht_cap_info->enable_mu_bformee = cfg->vht_mu_bformee;

	/*
	 * VHT max AMPDU len exp:
	 * override if user configured value is too high
	 * that the target cannot support.
	 * Even though Rome publish ampdu_len=7, it can
	 * only support 4 because of some h/w bug.
	 */
	if (vht_cap_info->ampdu_len_exponent > cfg->vht_max_ampdu_len_exp)
		vht_cap_info->ampdu_len_exponent = cfg->vht_max_ampdu_len_exp;

	/* Set VHT TXOP PS CAP */
	if (vht_cap_info->txop_ps && !cfg->vht_txop_ps)
		vht_cap_info->txop_ps = cfg->vht_txop_ps;

	/* set the Guard interval 160MHz */
	if (vht_cap_info->short_gi_160mhz && !cfg->vht_short_gi_160)
		vht_cap_info->short_gi_160mhz = cfg->vht_short_gi_160;

	if (cfg_get(psoc, CFG_ENABLE_VHT_MCS_10_11))
		vht_cap_info->vht_mcs_10_11_supp = cfg->vht_mcs_10_11_supp;

	mlme_legacy_debug("vht_mcs_10_11_supp %d",
			  vht_cap_info->vht_mcs_10_11_supp);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_update_nss_vht_cap(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct mlme_vht_capabilities_info *vht_cap_info;
	uint32_t temp = 0;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vht_cap_info = &mlme_obj->cfg.vht_caps.vht_cap_info;

	temp = vht_cap_info->basic_mcs_set;
	temp = (temp & 0xFFFC) | vht_cap_info->rx_mcs;
	if (vht_cap_info->enable2x2)
		temp = (temp & 0xFFF3) | (vht_cap_info->rx_mcs2x2 << 2);
	else
		temp |= 0x000C;

	vht_cap_info->basic_mcs_set = temp;

	temp = vht_cap_info->rx_mcs_map;
	temp = (temp & 0xFFFC) | vht_cap_info->rx_mcs;
	if (vht_cap_info->enable2x2)
		temp = (temp & 0xFFF3) | (vht_cap_info->rx_mcs2x2 << 2);
	else
		temp |= 0x000C;

	vht_cap_info->rx_mcs_map = temp;

	temp = vht_cap_info->tx_mcs_map;
	temp = (temp & 0xFFFC) | vht_cap_info->tx_mcs;
	if (vht_cap_info->enable2x2)
		temp = (temp & 0xFFF3) | (vht_cap_info->tx_mcs2x2 << 2);
	else
		temp |= 0x000C;

	vht_cap_info->tx_mcs_map = temp;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE
bool mlme_get_bss_11be_allowed(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *bssid,
			       uint8_t *ie_data,
			       uint32_t ie_length)
{
	struct action_oui_search_attr search_attr;

	if (wlan_action_oui_is_empty(psoc, ACTION_OUI_11BE_OUI_ALLOW))
		return true;

	qdf_mem_zero(&search_attr, sizeof(search_attr));
	search_attr.ie_data = ie_data;
	search_attr.ie_length = ie_length;
	if (wlan_action_oui_search(psoc, &search_attr,
				   ACTION_OUI_11BE_OUI_ALLOW))
		return true;

	mlme_legacy_debug("AP not in 11be oui allow list "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(bssid->bytes));

	return false;
}

QDF_STATUS wlan_mlme_get_oem_eht_mlo_config(struct wlan_objmgr_psoc *psoc,
					    uint32_t *oem_eht_cfg)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*oem_eht_cfg = mlme_obj->cfg.gen.oem_eht_mlo_crypto_bitmap;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wlan_mlme_is_sap_uapsd_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.qos_mlme_params.sap_uapsd_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_dtim_selection_diversity(struct wlan_objmgr_psoc *psoc,
						  uint32_t *dtim_selection_div)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*dtim_selection_div = cfg_default(CFG_DTIM_SELECTION_DIVERSITY);
		return QDF_STATUS_E_FAILURE;
	}

	*dtim_selection_div = mlme_obj->cfg.ps_params.dtim_selection_diversity;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bmps_min_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_BMPS_MINIMUM_LI);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.bmps_min_listen_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bmps_max_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_BMPS_MAXIMUM_LI);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.bmps_max_listen_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_uapsd_flag(struct wlan_objmgr_psoc *psoc,
					bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.qos_mlme_params.sap_uapsd_enabled &= value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_rrm_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.rrm_config.rrm_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_auto_bmps_timer_value(struct wlan_objmgr_psoc *psoc,
					       uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_AUTO_BMPS_ENABLE_TIMER);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.auto_bmps_timer_val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_bmps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_ENABLE_PS);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.is_bmps_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_imps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_ENABLE_IMPS);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.is_imps_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_override_bmps_imps(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.ps_params.is_imps_enabled = 0;
	mlme_obj->cfg.ps_params.is_bmps_enabled = 0;

	return QDF_STATUS_SUCCESS;
}

void wlan_mlme_get_wps_uuid(struct wlan_mlme_wps_params *wps_params,
			    uint8_t *data)
{
	qdf_size_t len = wps_params->wps_uuid.len;

	wlan_mlme_get_cfg_str(data, &wps_params->wps_uuid, &len);
}

QDF_STATUS
wlan_mlme_get_self_gen_frm_pwr(struct wlan_objmgr_psoc *psoc,
			       uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_SELF_GEN_FRM_PWR);
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.reg.self_gen_frm_pwr;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_4way_hs_offload(struct wlan_objmgr_psoc *psoc, uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_DISABLE_4WAY_HS_OFFLOAD);
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.gen.disable_4way_hs_offload;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bmiss_skip_full_scan_value(struct wlan_objmgr_psoc *psoc,
					 bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_BMISS_SKIP_FULL_SCAN);
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.gen.bmiss_skip_full_scan;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_peer_phymode(struct wlan_objmgr_psoc *psoc, uint8_t *mac,
				 enum wlan_phymode *peer_phymode)
{
	struct wlan_objmgr_peer *peer;

	peer = wlan_objmgr_get_peer_by_mac(psoc, mac, WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("peer object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	*peer_phymode = wlan_peer_get_phymode(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_set_tgt_wpa3_roam_cap(struct wlan_objmgr_psoc *psoc,
				      uint32_t akm_bitmap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.lfr.fw_akm_bitmap |= akm_bitmap;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc,
					bool *disabled)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*disabled = mlme_obj->cfg.reg.ignore_fw_reg_offload_ind;
	return QDF_STATUS_SUCCESS;
}

char *mlme_get_roam_status_str(uint32_t roam_status)
{
	switch (roam_status) {
	case 0:
		return "SUCCESS";
	case 1:
		return "FAILED";
	case 2:
		return "NO ROAM";
	default:
		return "UNKNOWN";
	}
}

char *mlme_get_roam_trigger_str(uint32_t roam_scan_trigger)
{
	switch (roam_scan_trigger) {
	case WMI_ROAM_TRIGGER_REASON_PER:
		return "PER";
	case WMI_ROAM_TRIGGER_REASON_BMISS:
		return "BEACON MISS";
	case WMI_ROAM_TRIGGER_REASON_LOW_RSSI:
		return "LOW RSSI";
	case WMI_ROAM_TRIGGER_REASON_HIGH_RSSI:
		return "HIGH RSSI";
	case WMI_ROAM_TRIGGER_REASON_PERIODIC:
		return "PERIODIC SCAN";
	case WMI_ROAM_TRIGGER_REASON_MAWC:
		return "MAWC";
	case WMI_ROAM_TRIGGER_REASON_DENSE:
		return "DENSE ENVIRONMENT";
	case WMI_ROAM_TRIGGER_REASON_BACKGROUND:
		return "BACKGROUND SCAN";
	case WMI_ROAM_TRIGGER_REASON_FORCED:
		return "FORCED SCAN";
	case WMI_ROAM_TRIGGER_REASON_BTM:
		return "BTM TRIGGER";
	case WMI_ROAM_TRIGGER_REASON_UNIT_TEST:
		return "TEST COMMAND";
	case WMI_ROAM_TRIGGER_REASON_BSS_LOAD:
		return "HIGH BSS LOAD";
	case WMI_ROAM_TRIGGER_REASON_DEAUTH:
		return "DEAUTH RECEIVED";
	case WMI_ROAM_TRIGGER_REASON_IDLE:
		return "IDLE STATE SCAN";
	case WMI_ROAM_TRIGGER_REASON_STA_KICKOUT:
		return "STA KICKOUT";
	case WMI_ROAM_TRIGGER_REASON_ESS_RSSI:
		return "ESS RSSI";
	case WMI_ROAM_TRIGGER_REASON_WTC_BTM:
		return "WTC BTM";
	case WMI_ROAM_TRIGGER_REASON_NONE:
		return "NONE";
	case WMI_ROAM_TRIGGER_REASON_PMK_TIMEOUT:
		return "PMK Expired";
	case WMI_ROAM_TRIGGER_REASON_BTC:
		return "BTC TRIGGER";
	default:
		return "UNKNOWN";
	}
}

void mlme_get_converted_timestamp(uint32_t timestamp, char *time)
{
	uint32_t hr, mins, secs;

	secs = timestamp / 1000;
	mins = secs / 60;
	hr = mins / 60;
	qdf_snprint(time, TIME_STRING_LEN, "[%02d:%02d:%02d.%06u]",
		    (hr % 24), (mins % 60), (secs % 60),
		    (timestamp % 1000) * 1000);
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
void wlan_mlme_set_sae_single_pmk_bss_cap(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id, bool val)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);

	if (!vdev) {
		mlme_err("get vdev failed");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap = val;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
}

void wlan_mlme_update_sae_single_pmk(struct wlan_objmgr_vdev *vdev,
				     struct mlme_pmk_info *sae_single_pmk)
{
	struct mlme_legacy_priv *mlme_priv;
	int32_t keymgmt;
	bool is_sae_connection = false;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0) {
		mlme_legacy_err("Invalid mgmt cipher");
		return;
	}

	if (keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE))
		is_sae_connection = true;

	mlme_legacy_debug("SAE_SPMK: single_pmk_ap:%d, is_sae_connection:%d, pmk_len:%d",
			  mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap,
			  is_sae_connection, sae_single_pmk->pmk_len);

	if (mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap &&
	    is_sae_connection)
		mlme_priv->mlme_roam.sae_single_pmk.pmk_info = *sae_single_pmk;
}

bool wlan_mlme_is_sae_single_pmk_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_SAE_SINGLE_PMK);

	return mlme_obj->cfg.lfr.sae_single_pmk_feature_enabled;
}

void wlan_mlme_get_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
				       struct wlan_mlme_sae_single_pmk *pmksa)
{
	struct mlme_legacy_priv *mlme_priv;
	struct mlme_pmk_info *pmk_info;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	pmk_info = &mlme_priv->mlme_roam.sae_single_pmk.pmk_info;

	pmksa->sae_single_pmk_ap =
		mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap;
	pmksa->pmk_info.spmk_timeout_period = pmk_info->spmk_timeout_period;
	pmksa->pmk_info.spmk_timestamp = pmk_info->spmk_timestamp;

	if (pmk_info->pmk_len) {
		qdf_mem_copy(pmksa->pmk_info.pmk, pmk_info->pmk,
			     pmk_info->pmk_len);
		pmksa->pmk_info.pmk_len = pmk_info->pmk_len;
		return;
	}

	qdf_mem_zero(pmksa->pmk_info.pmk, sizeof(*pmksa->pmk_info.pmk));
	pmksa->pmk_info.pmk_len = 0;
}

void wlan_mlme_clear_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
					 struct mlme_pmk_info *pmk_recv)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_mlme_sae_single_pmk *sae_single_pmk;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	sae_single_pmk = &mlme_priv->mlme_roam.sae_single_pmk;

	if (!pmk_recv) {
		/* Process flush pmk cmd */
		mlme_legacy_debug("Flush sae_single_pmk info");
		qdf_mem_zero(&sae_single_pmk->pmk_info,
			     sizeof(sae_single_pmk->pmk_info));
	} else if (pmk_recv->pmk_len != sae_single_pmk->pmk_info.pmk_len) {
		mlme_legacy_debug("Invalid pmk len");
		return;
	} else if (!qdf_mem_cmp(&sae_single_pmk->pmk_info.pmk, pmk_recv->pmk,
		   pmk_recv->pmk_len)) {
			/* Process delete pmk cmd */
			mlme_legacy_debug("Clear sae_single_pmk info");
			qdf_mem_zero(&sae_single_pmk->pmk_info,
				     sizeof(sae_single_pmk->pmk_info));
	}
}
#endif

char *mlme_get_roam_fail_reason_str(enum wlan_roam_failure_reason_code result)
{
	switch (result) {
	case ROAM_FAIL_REASON_NO_SCAN_START:
		return "SCAN NOT STARTED";
	case ROAM_FAIL_REASON_NO_AP_FOUND:
		return "NO AP FOUND";
	case ROAM_FAIL_REASON_NO_CAND_AP_FOUND:
		return "NO CANDIDATE FOUND";
	case ROAM_FAIL_REASON_HOST:
		return "HOST ABORTED";
	case ROAM_FAIL_REASON_AUTH_SEND:
		return "Send AUTH Failed";
	case ROAM_FAIL_REASON_AUTH_RECV:
		return "Received AUTH with FAILURE Status";
	case ROAM_FAIL_REASON_NO_AUTH_RESP:
		return "No Auth response from AP";
	case ROAM_FAIL_REASON_REASSOC_SEND:
		return "Send Re-assoc request failed";
	case ROAM_FAIL_REASON_REASSOC_RECV:
		return "Received Re-Assoc resp with Failure status";
	case ROAM_FAIL_REASON_NO_REASSOC_RESP:
		return "No Re-assoc response from AP";
	case ROAM_FAIL_REASON_EAPOL_TIMEOUT:
		return "EAPOL M1 timed out";
	case ROAM_FAIL_REASON_MLME:
		return "MLME error";
	case ROAM_FAIL_REASON_INTERNAL_ABORT:
		return "Fw aborted roam";
	case ROAM_FAIL_REASON_SCAN_START:
		return "Unable to start roam scan";
	case ROAM_FAIL_REASON_AUTH_NO_ACK:
		return "No ACK for Auth req";
	case ROAM_FAIL_REASON_AUTH_INTERNAL_DROP:
		return "Auth req dropped internally";
	case ROAM_FAIL_REASON_REASSOC_NO_ACK:
		return "No ACK for Re-assoc req";
	case ROAM_FAIL_REASON_REASSOC_INTERNAL_DROP:
		return "Re-assoc dropped internally";
	case ROAM_FAIL_REASON_EAPOL_M2_SEND:
		return "Unable to send M2 frame";
	case ROAM_FAIL_REASON_EAPOL_M2_INTERNAL_DROP:
		return "M2 Frame dropped internally";
	case ROAM_FAIL_REASON_EAPOL_M2_NO_ACK:
		return "No ACK for M2 frame";
	case ROAM_FAIL_REASON_EAPOL_M3_TIMEOUT:
		return "EAPOL M3 timed out";
	case ROAM_FAIL_REASON_EAPOL_M4_SEND:
		return "Unable to send M4 frame";
	case ROAM_FAIL_REASON_EAPOL_M4_INTERNAL_DROP:
		return "M4 frame dropped internally";
	case ROAM_FAIL_REASON_EAPOL_M4_NO_ACK:
		return "No ACK for M4 frame";
	case ROAM_FAIL_REASON_NO_SCAN_FOR_FINAL_BMISS:
		return "No scan on final BMISS";
	case ROAM_FAIL_REASON_DISCONNECT:
		return "Disconnect received during handoff";
	case ROAM_FAIL_REASON_SYNC:
		return "Previous roam sync pending";
	case ROAM_FAIL_REASON_SAE_INVALID_PMKID:
		return "Reason assoc reject - invalid PMKID";
	case ROAM_FAIL_REASON_SAE_PREAUTH_TIMEOUT:
		return "SAE preauth timed out";
	case ROAM_FAIL_REASON_SAE_PREAUTH_FAIL:
		return "SAE preauth failed";
	case ROAM_FAIL_REASON_UNABLE_TO_START_ROAM_HO:
		return "Start handoff failed- internal error";
	case ROAM_FAIL_REASON_NO_AP_FOUND_AND_FINAL_BMISS_SENT:
		return "No AP found on final BMISS";
	case ROAM_FAIL_REASON_NO_CAND_AP_FOUND_AND_FINAL_BMISS_SENT:
		return "No Candidate AP found on final BMISS";
	case ROAM_FAIL_REASON_CURR_AP_STILL_OK:
		return "CURRENT AP STILL OK";
	default:
		return "UNKNOWN";
	}
}

char *mlme_get_sub_reason_str(enum roam_trigger_sub_reason sub_reason)
{
	switch (sub_reason) {
	case ROAM_TRIGGER_SUB_REASON_PERIODIC_TIMER:
		return "PERIODIC TIMER";
	case ROAM_TRIGGER_SUB_REASON_LOW_RSSI_PERIODIC:
		return "LOW RSSI PERIODIC TIMER1";
	case ROAM_TRIGGER_SUB_REASON_BTM_DI_TIMER:
		return "BTM DISASSOC IMMINENT TIMER";
	case ROAM_TRIGGER_SUB_REASON_FULL_SCAN:
		return "FULL SCAN";
	case ROAM_TRIGGER_SUB_REASON_CU_PERIODIC:
		return "CU PERIODIC Timer1";
	case ROAM_TRIGGER_SUB_REASON_INACTIVITY_TIMER_LOW_RSSI:
		return "LOW RSSI INACTIVE TIMER";
	case ROAM_TRIGGER_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY_CU:
		return "CU PERIODIC TIMER2";
	case ROAM_TRIGGER_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY:
		return "LOW RSSI PERIODIC TIMER2";
	case ROAM_TRIGGER_SUB_REASON_INACTIVITY_TIMER_CU:
		return "CU INACTIVITY TIMER";
	default:
		return "NONE";
	}
}

QDF_STATUS
wlan_mlme_get_mgmt_max_retry(struct wlan_objmgr_psoc *psoc,
			     uint8_t *max_retry)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*max_retry = cfg_default(CFG_MGMT_RETRY_MAX);
		return QDF_STATUS_E_FAILURE;
	}

	*max_retry = mlme_obj->cfg.gen.mgmt_retry_max;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mgmt_6ghz_rate_support(struct wlan_objmgr_psoc *psoc,
				     bool *enable_he_mcs0_for_6ghz_mgmt)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*enable_he_mcs0_for_6ghz_mgmt =
			cfg_default(CFG_ENABLE_HE_MCS0_MGMT_6GHZ);
		return QDF_STATUS_E_FAILURE;
	}

	*enable_he_mcs0_for_6ghz_mgmt =
		mlme_obj->cfg.gen.enable_he_mcs0_for_6ghz_mgmt;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_status_ring_buffer(struct wlan_objmgr_psoc *psoc,
				 bool *enable_ring_buffer)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*enable_ring_buffer = cfg_default(CFG_ENABLE_RING_BUFFER);
		return QDF_STATUS_E_FAILURE;
	}

	*enable_ring_buffer = mlme_obj->cfg.gen.enable_ring_buffer;
	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_get_peer_unmap_conf(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.gen.enable_peer_unmap_conf_support;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_mlme_get_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t *roam_reason_vsie_enable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*roam_reason_vsie_enable =
			cfg_default(CFG_ENABLE_ROAM_REASON_VSIE);
		return QDF_STATUS_E_FAILURE;
	}

	*roam_reason_vsie_enable = mlme_obj->cfg.lfr.enable_roam_reason_vsie;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t roam_reason_vsie_enable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.lfr.enable_roam_reason_vsie = roam_reason_vsie_enable;
	return QDF_STATUS_SUCCESS;
}

uint32_t wlan_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_ROAM_TRIGGER_BITMAP);

	return mlme_obj->cfg.lfr.roam_trigger_bitmap;
}

void wlan_mlme_set_roaming_triggers(struct wlan_objmgr_psoc *psoc,
				    uint32_t trigger_bitmap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	mlme_obj->cfg.lfr.roam_trigger_bitmap = trigger_bitmap;
}

QDF_STATUS
wlan_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR3_ROAMING_OFFLOAD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.lfr3_roaming_offload;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_enable_disconnect_roam_offload(struct wlan_objmgr_psoc *psoc,
					     bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ENABLE_DISCONNECT_ROAM);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.enable_disconnect_roam_offload;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_enable_idle_roam(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ENABLE_IDLE_ROAM);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.enable_idle_roam;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_rssi_delta(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_RSSI_DELTA);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_rssi_delta;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_roam_info_stats_num(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR3_ROAM_INFO_STATS_NUM);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.roam_info_stats_num;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_inactive_time(struct wlan_objmgr_psoc *psoc,
				      uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_INACTIVE_TIME);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_inactive_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_data_packet_count(struct wlan_objmgr_psoc *psoc,
				     uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_PACKET_COUNT);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_data_packet_count;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_min_rssi(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_MIN_RSSI);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_min_rssi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_band(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_BAND);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_self_bss_roam(struct wlan_objmgr_psoc *psoc,
			    uint8_t *enable_self_bss_roam)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*enable_self_bss_roam =
			cfg_get(psoc, CFG_LFR3_ENABLE_SELF_BSS_ROAM);
		return QDF_STATUS_E_FAILURE;
	}

	*enable_self_bss_roam = mlme_obj->cfg.lfr.enable_self_bss_roam;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
wlan_mlme_get_peer_indicated_ch_width(struct wlan_objmgr_psoc *psoc,
				      struct peer_oper_mode_event *data)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!data) {
		mlme_err("Data params is NULL");
		return QDF_STATUS_E_INVAL;
	}

	peer = wlan_objmgr_get_peer_by_mac(psoc, data->peer_mac_address.bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_err("peer not found for mac: " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(data->peer_mac_address.bytes));
		return QDF_STATUS_E_NULL_VALUE;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		mlme_err("peer priv not found for mac: " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(peer->macaddr));
		status = QDF_STATUS_E_NULL_VALUE;
		goto done;
	}

	if (peer_priv->peer_ind_bw == CH_WIDTH_INVALID) {
		status = QDF_STATUS_E_INVAL;
		goto done;
	}

	data->new_bw = peer_priv->peer_ind_bw;

done:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return status;
}

QDF_STATUS
wlan_mlme_set_peer_indicated_ch_width(struct wlan_objmgr_psoc *psoc,
				      struct peer_oper_mode_event *data)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!data) {
		mlme_err("Data params is NULL");
		return QDF_STATUS_E_INVAL;
	}

	peer = wlan_objmgr_get_peer_by_mac(psoc, data->peer_mac_address.bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_err("peer not found for mac: " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(data->peer_mac_address.bytes));
		return QDF_STATUS_E_NULL_VALUE;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		mlme_err("peer priv not found for mac: " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(peer->macaddr));
		status = QDF_STATUS_E_NULL_VALUE;
		goto done;
	}

	peer_priv->peer_ind_bw =
			target_if_wmi_chan_width_to_phy_ch_width(data->new_bw);

done:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return status;
}

QDF_STATUS
wlan_mlme_set_ft_over_ds(struct wlan_objmgr_psoc *psoc,
			 uint8_t ft_over_ds_enable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.lfr.enable_ft_over_ds = ft_over_ds_enable;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_dfs_chan_ageout_time(struct wlan_objmgr_psoc *psoc,
				   uint8_t *dfs_chan_ageout_time)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*dfs_chan_ageout_time =
			cfg_default(CFG_DFS_CHAN_AGEOUT_TIME);
		return QDF_STATUS_E_FAILURE;
	}

	*dfs_chan_ageout_time = mlme_obj->cfg.gen.dfs_chan_ageout_time;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_SAE

#define NUM_RETRY_BITS 3
#define ROAM_AUTH_INDEX 2
#define ASSOC_INDEX 1
#define AUTH_INDEX 0
#define MAX_RETRIES 2
#define MAX_ROAM_AUTH_RETRIES 1
#define MAX_AUTH_RETRIES 3

QDF_STATUS
wlan_mlme_get_sae_assoc_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*retry_count = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*retry_count =
		QDF_GET_BITS(mlme_obj->cfg.gen.sae_connect_retries,
			     ASSOC_INDEX * NUM_RETRY_BITS, NUM_RETRY_BITS);

	*retry_count = QDF_MIN(MAX_RETRIES, *retry_count);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sae_auth_retry_count(struct wlan_objmgr_psoc *psoc,
				   uint8_t *retry_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*retry_count = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*retry_count =
		QDF_GET_BITS(mlme_obj->cfg.gen.sae_connect_retries,
			     AUTH_INDEX * NUM_RETRY_BITS, NUM_RETRY_BITS);

	*retry_count = QDF_MIN(MAX_AUTH_RETRIES, *retry_count);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sae_roam_auth_retry_count(struct wlan_objmgr_psoc *psoc,
					uint8_t *retry_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*retry_count = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*retry_count =
		QDF_GET_BITS(mlme_obj->cfg.gen.sae_connect_retries,
			     ROAM_AUTH_INDEX * NUM_RETRY_BITS, NUM_RETRY_BITS);

	*retry_count = QDF_MIN(MAX_ROAM_AUTH_RETRIES, *retry_count);

	return QDF_STATUS_SUCCESS;
}

#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
bool
wlan_mlme_get_dual_sta_roaming_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	bool dual_sta_roaming_enabled;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return cfg_default(CFG_ENABLE_DUAL_STA_ROAM_OFFLOAD);

	dual_sta_roaming_enabled =
			mlme_obj->cfg.lfr.lfr3_roaming_offload &&
			mlme_obj->cfg.lfr.lfr3_dual_sta_roaming_enabled &&
			wlan_mlme_get_dual_sta_roam_support(psoc) &&
			policy_mgr_is_hw_dbs_capable(psoc);

	return dual_sta_roaming_enabled;
}
#endif

QDF_STATUS
wlan_mlme_get_roam_scan_offload_enabled(struct wlan_objmgr_psoc *psoc,
					bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ROAM_SCAN_OFFLOAD_ENABLED);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.roam_scan_offload_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_roam_bmiss_final_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ROAM_BMISS_FINAL_BCNT);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.roam_bmiss_final_bcnt;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bmiss_timeout_on_wakeup(struct wlan_objmgr_psoc *psoc,
					      uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_BEACONLOSS_TIMEOUT_ON_WAKEUP);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.beaconloss_timeout_onwakeup;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bmiss_timeout_on_sleep(struct wlan_objmgr_psoc *psoc,
				     uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_BEACONLOSS_TIMEOUT_ON_SLEEP);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.beaconloss_timeout_onsleep;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_roam_bmiss_first_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ROAM_BMISS_FIRST_BCNT);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.roam_bmiss_first_bcnt;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_ADAPTIVE_11R
bool wlan_mlme_adaptive_11r_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_ADAPTIVE_11R);

	return mlme_obj->cfg.lfr.enable_adaptive_11r;
}
#endif

QDF_STATUS
wlan_mlme_get_mawc_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_FEATURE_ENABLED);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_ENABLED);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_traffic_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_TRAFFIC_THRESHOLD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_traffic_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_ap_rssi_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_AP_RSSI_THRESHOLD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_ap_rssi_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_high_adjust(struct wlan_objmgr_psoc *psoc,
					 uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_RSSI_HIGH_ADJUST);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_rssi_high_adjust;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_low_adjust(struct wlan_objmgr_psoc *psoc,
					uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_RSSI_LOW_ADJUST);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_rssi_low_adjust;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_threshold(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_THRESHOLD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_sample_time(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_SAMPLE_TIME);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.sample_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_6ghz(struct wlan_objmgr_psoc *psoc,
					   int32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_TRIG_6G_RSSI_THRES);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.rssi_threshold_6ghz;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_5ghz(struct wlan_objmgr_psoc *psoc,
					   int32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_TRIG_5G_RSSI_THRES);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.rssi_threshold_5ghz;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_24ghz(struct wlan_objmgr_psoc *psoc,
					    int32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_TRIG_2G_RSSI_THRES);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.rssi_threshold_24ghz;

	return QDF_STATUS_SUCCESS;
}

bool
wlan_mlme_check_chan_param_has_dfs(struct wlan_objmgr_pdev *pdev,
				   struct ch_params *ch_params,
				   uint32_t chan_freq)
{
	bool is_dfs = false;

	if (ch_params->ch_width == CH_WIDTH_160MHZ) {
		wlan_reg_set_create_punc_bitmap(ch_params, true);
		if (wlan_reg_get_5g_bonded_channel_state_for_pwrmode(pdev,
								     chan_freq,
								     ch_params,
								     REG_CURRENT_PWR_MODE) ==
		    CHANNEL_STATE_DFS)
			is_dfs = true;
	} else if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
		if (wlan_reg_get_channel_state_for_pwrmode(
			pdev,
			chan_freq,
			REG_CURRENT_PWR_MODE) == CHANNEL_STATE_DFS ||
		    wlan_reg_get_channel_state_for_pwrmode(
			pdev,
			ch_params->mhz_freq_seg1,
			REG_CURRENT_PWR_MODE) == CHANNEL_STATE_DFS)
			is_dfs = true;
	} else if (wlan_reg_is_dfs_for_freq(pdev, chan_freq)) {
		/*Indoor channels are also marked DFS, therefore
		 * check if the channel has REGULATORY_CHAN_RADAR
		 * channel flag to identify if the channel is DFS
		 */
		is_dfs = true;
	}

	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq) ||
	    WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
		is_dfs = false;

	return is_dfs;
}

QDF_STATUS
wlan_mlme_set_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.sta.usr_disabled_roaming = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.sta.usr_disabled_roaming;

	return QDF_STATUS_SUCCESS;
}

qdf_size_t mlme_get_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
			     qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !dst || !len) {
		mlme_legacy_err("invalid params");
		return 0;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return 0;
	}

	if (len < mlme_priv->opr_rate_set.len) {
		mlme_legacy_err("Invalid length %zd (<%zd)", len,
				mlme_priv->opr_rate_set.len);
		return 0;
	}

	qdf_mem_copy(dst, mlme_priv->opr_rate_set.data,
		     mlme_priv->opr_rate_set.len);

	return mlme_priv->opr_rate_set.len;
}

QDF_STATUS mlme_set_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !src) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (len > mlme_priv->opr_rate_set.max_len) {
		mlme_legacy_err("Invalid len %zd (>%zd)", len,
				mlme_priv->opr_rate_set.max_len);
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv->opr_rate_set.len = len;
	qdf_mem_copy(mlme_priv->opr_rate_set.data, src, len);

	return QDF_STATUS_SUCCESS;
}

qdf_size_t mlme_get_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
				 qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !dst || !len) {
		mlme_legacy_err("invalid params");
		return 0;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return 0;
	}

	if (len < mlme_priv->ext_opr_rate_set.len) {
		mlme_legacy_err("Invalid length %zd (<%zd)", len,
				mlme_priv->ext_opr_rate_set.len);
		return 0;
	}

	qdf_mem_copy(dst, mlme_priv->ext_opr_rate_set.data,
		     mlme_priv->ext_opr_rate_set.len);

	return mlme_priv->ext_opr_rate_set.len;
}

QDF_STATUS mlme_set_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !src) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (len > mlme_priv->ext_opr_rate_set.max_len) {
		mlme_legacy_err("Invalid len %zd (>%zd)", len,
				mlme_priv->ext_opr_rate_set.max_len);
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv->ext_opr_rate_set.len = len;
	qdf_mem_copy(mlme_priv->ext_opr_rate_set.data, src, len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_clear_ext_opr_rate(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->ext_opr_rate_set.len = 0;
	qdf_mem_set(mlme_priv->ext_opr_rate_set.data, CFG_STR_DATA_LEN, 0);

	return QDF_STATUS_SUCCESS;
}

qdf_size_t mlme_get_mcs_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
			     qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !dst || !len) {
		mlme_legacy_err("invalid params");
		return 0;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return 0;
	}

	if (len < mlme_priv->mcs_rate_set.len) {
		mlme_legacy_err("Invalid length %zd (<%zd)", len,
				mlme_priv->mcs_rate_set.len);
		return 0;
	}

	qdf_mem_copy(dst, mlme_priv->mcs_rate_set.data,
		     mlme_priv->mcs_rate_set.len);

	return mlme_priv->mcs_rate_set.len;
}

QDF_STATUS mlme_set_mcs_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !src) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (len > mlme_priv->mcs_rate_set.max_len) {
		mlme_legacy_err("Invalid len %zd (>%zd)", len,
				mlme_priv->mcs_rate_set.max_len);
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv->mcs_rate_set.len = len;
	qdf_mem_copy(mlme_priv->mcs_rate_set.data, src, len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_clear_mcs_rate(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->mcs_rate_set.len = 0;
	qdf_mem_set(mlme_priv->mcs_rate_set.data, CFG_STR_DATA_LEN, 0);

	return QDF_STATUS_SUCCESS;
}

static enum monitor_mode_concurrency
wlan_mlme_get_monitor_mode_concurrency(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_MONITOR_MODE_CONCURRENCY);

	return mlme_obj->cfg.gen.monitor_mode_concurrency;
}

#ifdef FEATURE_WDS
enum wlan_wds_mode
wlan_mlme_get_wds_mode(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_WDS_MODE);

	return mlme_obj->cfg.gen.wds_mode;
}

void wlan_mlme_set_wds_mode(struct wlan_objmgr_psoc *psoc,
			    enum wlan_wds_mode mode)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;
	if (mode <= WLAN_WDS_MODE_MAX)
		mlme_obj->cfg.gen.wds_mode = mode;
}
#endif

bool wlan_mlme_is_sta_mon_conc_supported(struct wlan_objmgr_psoc *psoc)
{
	if (wlan_mlme_get_monitor_mode_concurrency(psoc) ==
						MONITOR_MODE_CONC_STA_SCAN_MON)
		return true;

	return false;
}

bool wlan_mlme_skip_tpe(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.power.skip_tpe;
}

#ifdef WLAN_FEATURE_11BE
QDF_STATUS mlme_cfg_get_orig_eht_caps(struct wlan_objmgr_psoc *psoc,
				      tDot11fIEeht_cap *eht_cap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*eht_cap = mlme_obj->cfg.eht_caps.eht_cap_orig;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_cfg_get_eht_caps(struct wlan_objmgr_psoc *psoc,
				 tDot11fIEeht_cap *eht_cap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*eht_cap = mlme_obj->cfg.eht_caps.dot11_eht_cap;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
wlan_mlme_set_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev, bool found)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->ba_2k_jump_iot_ap = found;

	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_is_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->ba_2k_jump_iot_ap;
}

QDF_STATUS
wlan_mlme_set_last_delba_sent_time(struct wlan_objmgr_vdev *vdev,
				   qdf_time_t delba_sent_time)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->last_delba_sent_time = delba_sent_time;

	return QDF_STATUS_SUCCESS;
}

qdf_time_t
wlan_mlme_get_last_delba_sent_time(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return 0;
	}

	return mlme_priv->last_delba_sent_time;
}

QDF_STATUS mlme_set_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			    bool ps_enable)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev)
		return status;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (mlme_priv) {
		mlme_priv->is_usr_ps_enabled = ps_enable;
		status = QDF_STATUS_SUCCESS;
		mlme_legacy_debug("vdev:%d user PS:%d", vdev_id,
				  mlme_priv->is_usr_ps_enabled);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return status;
}

bool mlme_get_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	bool usr_ps_enable = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev)
		return false;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (mlme_priv)
		usr_ps_enable = mlme_priv->is_usr_ps_enabled;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return usr_ps_enable;
}

bool wlan_mlme_is_multipass_sap(struct wlan_objmgr_psoc *psoc)
{
	struct target_psoc_info *info;

	info = wlan_psoc_get_tgt_if_handle(psoc);
	if (!info) {
		mlme_legacy_err("target_psoc_info is null");
		return QDF_STATUS_E_FAILURE;
	}

	return target_is_multipass_sap(info);
}

QDF_STATUS wlan_mlme_get_phy_max_freq_range(struct wlan_objmgr_psoc *psoc,
					    uint32_t *low_2ghz_chan,
					    uint32_t *high_2ghz_chan,
					    uint32_t *low_5ghz_chan,
					    uint32_t *high_5ghz_chan)
{
	uint32_t i;
	uint32_t reg_low_2ghz_chan;
	uint32_t reg_high_2ghz_chan;
	uint32_t reg_low_5ghz_chan;
	uint32_t reg_high_5ghz_chan;
	struct target_psoc_info *info;
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
	struct wlan_psoc_host_hal_reg_cap_ext *reg_cap_ext;

	info = wlan_psoc_get_tgt_if_handle(psoc);
	if (!info) {
		mlme_legacy_err("target_psoc_info is null");
		return QDF_STATUS_E_FAILURE;
	}
	mac_phy_cap = info->info.mac_phy_cap;
	reg_cap_ext = &mac_phy_cap->reg_cap_ext;
	reg_low_2ghz_chan = reg_cap_ext->low_2ghz_chan;
	reg_high_2ghz_chan = reg_cap_ext->high_2ghz_chan;
	reg_low_5ghz_chan = reg_cap_ext->low_5ghz_chan;
	reg_high_5ghz_chan = reg_cap_ext->high_5ghz_chan;
	for (i = 1; i < PSOC_MAX_MAC_PHY_CAP; i++) {
		mac_phy_cap = &info->info.mac_phy_cap[i];
		reg_cap_ext = &mac_phy_cap->reg_cap_ext;

		if (reg_cap_ext->low_2ghz_chan) {
			reg_low_2ghz_chan = reg_low_2ghz_chan ?
				QDF_MIN(reg_cap_ext->low_2ghz_chan,
					reg_low_2ghz_chan) :
				reg_cap_ext->low_2ghz_chan;
		}
		if (reg_cap_ext->high_2ghz_chan) {
			reg_high_2ghz_chan = reg_high_2ghz_chan ?
				QDF_MAX(reg_cap_ext->high_2ghz_chan,
					reg_high_2ghz_chan) :
				reg_cap_ext->high_2ghz_chan;
		}
		if (reg_cap_ext->low_5ghz_chan) {
			reg_low_5ghz_chan = reg_low_5ghz_chan ?
				QDF_MIN(reg_cap_ext->low_5ghz_chan,
					reg_low_5ghz_chan) :
				reg_cap_ext->low_5ghz_chan;
		}
		if (reg_cap_ext->high_5ghz_chan) {
			reg_high_5ghz_chan = reg_high_5ghz_chan ?
				QDF_MAX(reg_cap_ext->high_5ghz_chan,
					reg_high_5ghz_chan) :
				reg_cap_ext->high_5ghz_chan;
		}
	}
	/* For old hw, no reg_cap_ext reported from service ready ext,
	 * fill the low/high with default of regulatory.
	 */
	if (!reg_low_2ghz_chan && !reg_high_2ghz_chan &&
	    !reg_low_5ghz_chan && !reg_high_5ghz_chan) {
		mlme_legacy_debug("no reg_cap_ext in mac_phy_cap");
		reg_low_2ghz_chan = TWOG_STARTING_FREQ - 10;
		reg_high_2ghz_chan = TWOG_CHAN_14_IN_MHZ + 10;
		reg_low_5ghz_chan = FIVEG_STARTING_FREQ - 10;
		reg_high_5ghz_chan = SIXG_CHAN_233_IN_MHZ + 10;
	}
	if (!wlan_reg_is_6ghz_supported(psoc)) {
		mlme_legacy_debug("disabling 6ghz channels");
		reg_high_5ghz_chan = FIVEG_CHAN_177_IN_MHZ + 10;
	}
	mlme_legacy_debug("%d %d %d %d", reg_low_2ghz_chan, reg_high_2ghz_chan,
			  reg_low_5ghz_chan, reg_high_5ghz_chan);
	*low_2ghz_chan = reg_low_2ghz_chan;
	*high_2ghz_chan = reg_high_2ghz_chan;
	*low_5ghz_chan = reg_low_5ghz_chan;
	*high_5ghz_chan = reg_high_5ghz_chan;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_P2P_P2P_STA
bool
wlan_mlme_get_p2p_p2p_conc_support(struct wlan_objmgr_psoc *psoc)
{
	return wlan_psoc_nif_fw_ext_cap_get(psoc,
					    WLAN_SOC_EXT_P2P_P2P_CONC_SUPPORT);
}
#endif

enum phy_ch_width mlme_get_vht_ch_width(void)
{
	enum phy_ch_width bandwidth = CH_WIDTH_INVALID;
	uint32_t fw_ch_wd = wma_get_vht_ch_width();

	if (fw_ch_wd == WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
		bandwidth = CH_WIDTH_80P80MHZ;
	else if (fw_ch_wd == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		bandwidth = CH_WIDTH_160MHZ;
	else
		bandwidth = CH_WIDTH_80MHZ;

	return bandwidth;
}

uint8_t
wlan_mlme_get_mgmt_hw_tx_retry_count(struct wlan_objmgr_psoc *psoc,
				     enum mlme_cfg_frame_type frm_type)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return 0;

	if (frm_type >= CFG_FRAME_TYPE_MAX)
		return 0;

	return mlme_obj->cfg.gen.mgmt_hw_tx_retry_count[frm_type];
}

QDF_STATUS
wlan_mlme_get_tx_retry_multiplier(struct wlan_objmgr_psoc *psoc,
				  uint32_t *tx_retry_multiplier)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*tx_retry_multiplier =
				cfg_default(CFG_TX_RETRY_MULTIPLIER);
		return QDF_STATUS_E_FAILURE;
	}

	*tx_retry_multiplier = mlme_obj->cfg.gen.tx_retry_multiplier;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_update_chan_width_allowed(struct wlan_objmgr_psoc *psoc,
					bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_ALLOW_UPDATE_CHANNEL_WIDTH);
		return QDF_STATUS_E_INVAL;
	}

	*value = mlme_obj->cfg.feature_flags.update_cw_allowed;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_channel_bonding_5ghz(struct wlan_objmgr_psoc *psoc,
				   uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_CHANNEL_BONDING_MODE_5GHZ);
		return QDF_STATUS_E_INVAL;
	}

	*value = mlme_obj->cfg.feature_flags.channel_bonding_mode_5ghz;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_update_ratemask_params(struct wlan_objmgr_vdev *vdev,
				 uint8_t num_ratemask,
				 struct config_ratemask_params *rate_params)
{
	struct vdev_mlme_obj *vdev_mlme;
	struct vdev_mlme_rate_info *rate_info;
	QDF_STATUS ret;
	uint8_t i = 0;
	uint8_t index;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	rate_info = &vdev_mlme->mgmt.rate_info;
	while (i < num_ratemask) {
		index = rate_params[i].type;
		if (index >= WLAN_VDEV_RATEMASK_TYPE_MAX) {
			mlme_legacy_err("Invalid ratemask type");
			++i;
			continue;
		}

		if (rate_info->ratemask_params[index].lower32 !=
		    rate_params[i].lower32 ||
		    rate_info->ratemask_params[index].lower32_2 !=
		    rate_params[i].lower32_2 ||
		    rate_info->ratemask_params[index].higher32 !=
		    rate_params[i].higher32 ||
		    rate_info->ratemask_params[index].higher32_2 !=
		    rate_params[i].higher32_2) {
			rate_info->ratemask_params[index].lower32 =
						rate_params[i].lower32;
			rate_info->ratemask_params[index].higher32 =
						rate_params[i].higher32;
			rate_info->ratemask_params[index].lower32_2 =
						rate_params[i].lower32_2;
			rate_info->ratemask_params[index].higher32_2 =
						rate_params[i].higher32_2;
			ret = wlan_util_vdev_mlme_set_ratemask_config(vdev_mlme,
								      index);
			if (ret != QDF_STATUS_SUCCESS)
				mlme_legacy_err("ratemask config failed");
		} else {
			mlme_legacy_debug("Ratemask same as configured mask");
		}
		++i;
	}
	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_is_channel_valid(struct wlan_objmgr_psoc *psoc,
				uint32_t chan_freq)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return wlan_roam_is_channel_valid(&mlme_obj->cfg.reg,
					  chan_freq);
}

#ifdef WLAN_FEATURE_MCC_QUOTA
#define WLAN_MCC_MIN_QUOTA 10 /* in %age */
#define WLAN_MCC_MAX_QUOTA 90 /* in %age */
QDF_STATUS wlan_mlme_set_user_mcc_quota(struct wlan_objmgr_psoc *psoc,
					struct wlan_user_mcc_quota *quota)

{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	if (!quota)
		return QDF_STATUS_E_NULL_VALUE;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (quota->quota < WLAN_MCC_MIN_QUOTA)
		quota->quota = WLAN_MCC_MIN_QUOTA;
	else if (quota->quota > WLAN_MCC_MAX_QUOTA)
		quota->quota = WLAN_MCC_MAX_QUOTA;

	mlme_obj->cfg.gen.user_mcc_quota.quota = quota->quota;
	mlme_obj->cfg.gen.user_mcc_quota.op_mode = quota->op_mode;
	mlme_obj->cfg.gen.user_mcc_quota.vdev_id = quota->vdev_id;

	mlme_debug("quota : %u, op_mode : %d, vdev_id : %u",
		   quota->quota, quota->op_mode, quota->vdev_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_user_mcc_quota(struct wlan_objmgr_psoc *psoc,
					struct wlan_user_mcc_quota *quota)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	if (!quota)
		return QDF_STATUS_E_NULL_VALUE;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	quota->quota = mlme_obj->cfg.gen.user_mcc_quota.quota;
	quota->op_mode = mlme_obj->cfg.gen.user_mcc_quota.op_mode;
	quota->vdev_id = mlme_obj->cfg.gen.user_mcc_quota.vdev_id;

	return QDF_STATUS_SUCCESS;
}

uint32_t
wlan_mlme_get_user_mcc_duty_cycle_percentage(struct wlan_objmgr_psoc *psoc)
{
	uint32_t mcc_freq, ch_freq, quota_value;
	struct wlan_user_mcc_quota quota;
	uint8_t operating_channel;
	int status;

	quota.vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	quota.quota = 0;
	if (QDF_IS_STATUS_ERROR(wlan_mlme_get_user_mcc_quota(psoc, &quota))) {
		mlme_debug("Error getting user quota set");
		return 0;
	}

	if (quota.vdev_id == WLAN_UMAC_VDEV_ID_MAX || quota.quota == 0) {
		mlme_debug("Invalid quota : vdev %u, quota %u",
			   quota.vdev_id, quota.quota);
		return 0;
	}
	status = policy_mgr_get_chan_by_session_id(psoc, quota.vdev_id,
						   &ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("Could not get vdev %u chan", quota.vdev_id);
		return 0;
	}
	mcc_freq = policy_mgr_get_mcc_operating_channel(psoc, quota.vdev_id);
	if (mcc_freq == INVALID_CHANNEL_ID)
		return 0;

	operating_channel = wlan_freq_to_chan(ch_freq);
	if (!operating_channel) {
		mlme_debug("Primary op channel is invalid");
		return 0;
	}
	/*
	 * The channel numbers for both adapters and the time
	 * quota for the 1st adapter, i.e., one specified in cmd
	 * are formatted as a bit vector
	 * ******************************************************
	 * |bit 31-24  | bit 23-16 |  bits 15-8  |bits 7-0   |
	 * |  Unused   | Quota for | chan. # for |chan. # for|
	 * |           |  1st chan | 1st chan.   |2nd chan.  |
	 * ******************************************************
	 */
	mlme_debug("Opmode (%d) vdev (%u) channel %u and quota %u",
		   quota.op_mode, quota.vdev_id,
		   operating_channel, quota.quota);
	quota_value = quota.quota;
	/* Move the time quota for first channel to bits 15-8 */
	quota_value = quota_value << 8;
	/*
	 * Store the channel number of 1st channel at bits 7-0
	 * of the bit vector
	 */
	quota_value |= operating_channel;

	operating_channel = wlan_freq_to_chan(mcc_freq);
	if (!operating_channel) {
		mlme_debug("Secondary op channel is invalid");
		return 0;
	}

	/*
	 * Now move the time quota and channel number of the
	 * 1st adapter to bits 23-16 and bits 15-8 of the bit
	 * vector, respectively.
	 */
	quota_value = quota_value << 8;
	/*
	 * Set the channel number for 2nd MCC vdev at bits
	 * 7-0 of set_value
	 */
	quota_value |= operating_channel;
	mlme_debug("quota value:%x", quota_value);

	return quota_value;
}
#endif /* WLAN_FEATURE_MCC_QUOTA */

uint8_t mlme_get_max_he_mcs_idx(enum phy_ch_width mcs_ch_width,
				u_int16_t *hecap_rxmcsnssmap,
				u_int16_t *hecap_txmcsnssmap)
{
	uint8_t rx_max_mcs, tx_max_mcs, max_mcs = INVALID_MCS_NSS_INDEX;

	switch (mcs_ch_width) {
	case CH_WIDTH_80P80MHZ:
		if (hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] &&
		    hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]) {
			rx_max_mcs = hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] & 0x03;
			tx_max_mcs = hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] & 0x03;
			max_mcs = rx_max_mcs < tx_max_mcs ? rx_max_mcs : tx_max_mcs;
			if (max_mcs < 0x03)
				max_mcs = 7 + 2 * max_mcs;
		}
		fallthrough;
	case CH_WIDTH_160MHZ:
		if (hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] &&
		    hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]) {
			rx_max_mcs = hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] & 0x03;
			tx_max_mcs = hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] & 0x03;
			max_mcs = rx_max_mcs < tx_max_mcs ? rx_max_mcs : tx_max_mcs;
			if (max_mcs < 0x03)
				max_mcs = 7 + 2 * max_mcs;
		}
		fallthrough;
	default:
		if (hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] &&
		    hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]) {
			rx_max_mcs = hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] & 0x03;
			tx_max_mcs = hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] & 0x03;
			max_mcs = rx_max_mcs < tx_max_mcs ? rx_max_mcs : tx_max_mcs;
			if (max_mcs < 0x03)
				max_mcs = 7 + 2 * max_mcs;
		}
	}

	return max_mcs;
}

uint8_t mlme_get_max_vht_mcs_idx(u_int16_t rx_vht_mcs_map,
				 u_int16_t tx_vht_mcs_map)
{
	uint8_t rx_max_mcs, tx_max_mcs, max_mcs = INVALID_MCS_NSS_INDEX;

	if (rx_vht_mcs_map && tx_vht_mcs_map) {
		rx_max_mcs = rx_vht_mcs_map & 0x03;
		tx_max_mcs = tx_vht_mcs_map & 0x03;
		max_mcs = rx_max_mcs < tx_max_mcs ? rx_max_mcs : tx_max_mcs;
		if (max_mcs < 0x03)
			return 7 + max_mcs;
	}

	return max_mcs;
}

#ifdef WLAN_FEATURE_SON
QDF_STATUS mlme_save_vdev_max_mcs_idx(struct wlan_objmgr_vdev *vdev,
				      uint8_t max_mcs_idx)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev) {
		mlme_legacy_err("invalid vdev");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->max_mcs_index = max_mcs_idx;

	return QDF_STATUS_SUCCESS;
}

uint8_t mlme_get_vdev_max_mcs_idx(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev) {
		mlme_legacy_err("invalid vdev");
		return INVALID_MCS_NSS_INDEX;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return INVALID_MCS_NSS_INDEX;
	}

	return mlme_priv->max_mcs_index;
}
#endif /* WLAN_FEATURE_SON */

void wlan_mlme_get_safe_mode_enable(struct wlan_objmgr_psoc *psoc,
				    bool *safe_mode_enable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("invalid mlme obj");
		*safe_mode_enable = false;
		return;
	}

	*safe_mode_enable = mlme_obj->cfg.gen.safe_mode_enable;
}

void wlan_mlme_set_safe_mode_enable(struct wlan_objmgr_psoc *psoc,
				    bool safe_mode_enable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("invalid mlme obj");
		return;
	}

	mlme_obj->cfg.gen.safe_mode_enable = safe_mode_enable;
}

uint32_t wlan_mlme_get_6g_ap_power_type(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *mlme_obj;

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);

	if (!mlme_obj) {
		mlme_legacy_err("vdev component object is NULL");
		return REG_MAX_AP_TYPE;
	}

	return mlme_obj->reg_tpc_obj.power_type_6g;
}

QDF_STATUS wlan_connect_hw_mode_change_resp(struct wlan_objmgr_pdev *pdev,
					    uint8_t vdev_id,
					    wlan_cm_id cm_id, QDF_STATUS status)
{
	return wlan_cm_handle_hw_mode_change_resp(pdev, vdev_id, cm_id,
						  status);
}

enum phy_ch_width
wlan_mlme_get_ch_width_from_phymode(enum wlan_phymode phy_mode)
{
	enum phy_ch_width ch_width;

	if (IS_WLAN_PHYMODE_320MHZ(phy_mode))
		ch_width = CH_WIDTH_320MHZ;
	else if (IS_WLAN_PHYMODE_160MHZ(phy_mode))
		ch_width = CH_WIDTH_160MHZ;
	else if (IS_WLAN_PHYMODE_80MHZ(phy_mode))
		ch_width = CH_WIDTH_80MHZ;
	else if (IS_WLAN_PHYMODE_40MHZ(phy_mode))
		ch_width = CH_WIDTH_40MHZ;
	else
		ch_width = CH_WIDTH_20MHZ;

	mlme_legacy_debug("phymode: %d, ch_width: %d ", phy_mode, ch_width);

	return ch_width;
}

enum phy_ch_width
wlan_mlme_get_peer_ch_width(struct wlan_objmgr_psoc *psoc, uint8_t *mac)
{
	enum wlan_phymode phy_mode;
	QDF_STATUS status;

	status = mlme_get_peer_phymode(psoc, mac, &phy_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_legacy_err("failed to fetch phy_mode status: %d for mac: " QDF_MAC_ADDR_FMT,
				status, QDF_MAC_ADDR_REF(mac));
		return CH_WIDTH_20MHZ;
	}

	return wlan_mlme_get_ch_width_from_phymode(phy_mode);
}

#ifdef FEATURE_SET

/**
 * wlan_mlme_get_latency_enable() - get wlm latency cfg value
 * @psoc: psoc context
 * @value: Pointer in which wlam latency cfg value needs to be filled
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF_STATUS_E_INVAL on failure
 */
static QDF_STATUS
wlan_mlme_get_latency_enable(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("mlme obj null");
		return QDF_STATUS_E_INVAL;
	}

	*value = mlme_obj->cfg.wlm_config.latency_enable;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_ADAPTIVE_11R
/**
 * wlan_mlme_get_adaptive11r_enabled() - get adaptive 11r cfg value
 * @psoc: psoc context
 * @val: Pointer in which adaptive 11r cfg value needs to be filled
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF_STATUS_E_INVAL on failure
 */
static QDF_STATUS
wlan_mlme_get_adaptive11r_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_ADAPTIVE_11R);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.enable_adaptive_11r;

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
wlan_mlme_get_adaptive11r_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	*val = false;
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(WLAN_FEATURE_P2P_P2P_STA) && \
	!defined(WLAN_FEATURE_NO_P2P_CONCURRENCY)
static bool
wlan_mlme_get_p2p_p2p_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_p2p_p2p_host_conc_support(void)
{
	return false;
}
#endif

#ifndef WLAN_FEATURE_NO_STA_SAP_CONCURRENCY
static bool
wlan_mlme_get_sta_sap_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_sap_host_conc_support(void)
{
	return false;
}
#endif

#ifndef WLAN_FEATURE_NO_STA_NAN_CONCURRENCY
static bool
wlan_mlme_get_sta_nan_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_nan_host_conc_support(void)
{
	return false;
}
#endif

#ifdef FEATURE_WLAN_TDLS
static bool
wlan_mlme_get_sta_tdls_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_tdls_host_conc_support(void)
{
	return false;
}
#endif

#if !defined(WLAN_FEATURE_NO_STA_SAP_CONCURRENCY) && \
	(!defined(WLAN_FEATURE_NO_P2P_CONCURRENCY) || \
	 defined(WLAN_FEATURE_STA_SAP_P2P_CONCURRENCY))
static bool
wlan_mlme_get_sta_sap_p2p_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_sap_p2p_host_conc_support(void)
{
	return false;
}
#endif

#if defined(FEATURE_WLAN_TDLS)
static bool
wlan_mlme_get_sta_p2p_tdls_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_p2p_tdls_host_conc_support(void)
{
	return false;
}
#endif

#if defined(FEATURE_WLAN_TDLS) && !defined(WLAN_FEATURE_NO_STA_SAP_CONCURRENCY)
static bool
wlan_mlme_get_sta_sap_tdls_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_sap_tdls_host_conc_support(void)
{
	return false;
}
#endif

#if defined(FEATURE_WLAN_TDLS) && \
	!defined(WLAN_FEATURE_NO_STA_SAP_CONCURRENCY) && \
	(!defined(WLAN_FEATURE_NO_P2P_CONCURRENCY) || \
	 defined(WLAN_FEATURE_STA_SAP_P2P_CONCURRENCY))

static bool
wlan_mlme_get_sta_sap_p2p_tdls_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_sap_p2p_tdls_host_conc_support(void)
{
	return false;
}
#endif

#if defined(FEATURE_WLAN_TDLS) && defined(WLAN_FEATURE_P2P_P2P_STA) && \
	!defined(WLAN_FEATURE_NO_P2P_CONCURRENCY)
static bool
wlan_mlme_get_sta_p2p_p2p_tdls_host_conc_support(void)
{
	return true;
}
#else
static bool
wlan_mlme_get_sta_p2p_p2p_tdls_host_conc_support(void)
{
	return false;
}
#endif

/**
 * wlan_mlme_set_iface_combinations() - Set interface combinations
 * @mlme_feature_set: Pointer to wlan_mlme_features
 *
 * Return: None
 */
static void
wlan_mlme_set_iface_combinations(struct wlan_mlme_features *mlme_feature_set)
{
	mlme_feature_set->iface_combinations = 0;
	mlme_feature_set->iface_combinations |= MLME_IFACE_STA_P2P_SUPPORT;
	if (wlan_mlme_get_sta_sap_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_SAP_SUPPORT;
	if (wlan_mlme_get_sta_nan_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_NAN_SUPPORT;
	if (wlan_mlme_get_sta_tdls_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_TDLS_SUPPORT;
	if (wlan_mlme_get_p2p_p2p_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_DUAL_P2P_SUPPORT;
	if (wlan_mlme_get_sta_sap_p2p_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_SAP_P2P_SUPPORT;
	if (wlan_mlme_get_sta_p2p_tdls_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_P2P_TDLS_SUPPORT;
	if (wlan_mlme_get_sta_sap_tdls_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_SAP_TDLS_SUPPORT;
	if (wlan_mlme_get_sta_sap_p2p_tdls_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_SAP_P2P_TDLS_SUPPORT;
	if (wlan_mlme_get_sta_p2p_p2p_tdls_host_conc_support())
		mlme_feature_set->iface_combinations |=
					MLME_IFACE_STA_P2P_P2P_TDLS_SUPPORT;
	mlme_debug("iface combinations = %x",
		   mlme_feature_set->iface_combinations);
}

void wlan_mlme_get_feature_info(struct wlan_objmgr_psoc *psoc,
				struct wlan_mlme_features *mlme_feature_set)
{
	uint32_t roam_triggers;
	int sap_max_num_clients = 0;
	bool is_enable_idle_roam = false, is_bss_load_enabled = false;

	wlan_mlme_get_latency_enable(psoc,
				     &mlme_feature_set->enable_wifi_optimizer);
	wlan_mlme_get_sap_max_peers(psoc, &sap_max_num_clients);
	mlme_feature_set->sap_max_num_clients = sap_max_num_clients;
	mlme_feature_set->vendor_req_1_version =
					WMI_HOST_VENDOR1_REQ1_VERSION_4_00;
	roam_triggers = wlan_mlme_get_roaming_triggers(psoc);

	wlan_mlme_get_bss_load_enabled(psoc, &is_bss_load_enabled);
	mlme_feature_set->roaming_high_cu_roam_trigger =
			(roam_triggers & BIT(ROAM_TRIGGER_REASON_BSS_LOAD)) &&
			is_bss_load_enabled;

	mlme_feature_set->roaming_emergency_trigger =
			roam_triggers & BIT(ROAM_TRIGGER_REASON_FORCED);
	mlme_feature_set->roaming_btm_trihgger =
			roam_triggers & BIT(ROAM_TRIGGER_REASON_BTM);

	wlan_mlme_get_enable_idle_roam(psoc, &is_enable_idle_roam);
	mlme_feature_set->roaming_idle_trigger =
			(roam_triggers & BIT(ROAM_TRIGGER_REASON_IDLE)) &&
			is_enable_idle_roam;

	mlme_feature_set->roaming_wtc_trigger =
			roam_triggers & BIT(ROAM_TRIGGER_REASON_WTC_BTM);
	mlme_feature_set->roaming_btcoex_trigger =
			roam_triggers & BIT(ROAM_TRIGGER_REASON_BTC);
	mlme_feature_set->roaming_btw_wpa_wpa2 = true;
	mlme_feature_set->roaming_manage_chan_list_api = true;

	wlan_mlme_get_adaptive11r_enabled(
				psoc,
				&mlme_feature_set->roaming_adaptive_11r);
	mlme_feature_set->roaming_ctrl_api_get_set = true;
	mlme_feature_set->roaming_ctrl_api_reassoc = true;
	mlme_feature_set->roaming_ctrl_get_cu = true;

	mlme_feature_set->vendor_req_2_version =
					WMI_HOST_VENDOR1_REQ2_VERSION_3_50;
	wlan_mlme_set_iface_combinations(mlme_feature_set);
	wlan_mlme_get_vht_enable2x2(psoc, &mlme_feature_set->enable2x2);
}
#endif

void wlan_mlme_chan_stats_scan_event_cb(struct wlan_objmgr_vdev *vdev,
					struct scan_event *event, void *arg)
{
	bool success = false;

	if (!util_is_scan_completed(event, &success))
		return;

	mlme_send_scan_done_complete_cb(event->vdev_id);
}

static QDF_STATUS
wlan_mlme_update_vdev_chwidth_with_notify(struct wlan_objmgr_psoc *psoc,
					  struct wlan_objmgr_vdev *vdev,
					  uint8_t vdev_id,
					  wmi_host_channel_width ch_width)
{
	struct vdev_mlme_obj *vdev_mlme;
	struct vdev_set_params param = {0};
	QDF_STATUS status;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	param.param_id = wmi_vdev_param_chwidth_with_notify;
	param.vdev_id = vdev_id;
	param.param_value = ch_width;
	status = tgt_vdev_mgr_set_param_send(vdev_mlme, &param);
	policy_mgr_handle_ml_sta_link_on_traffic_type_change(psoc, vdev);

	return status;
}

#ifdef WLAN_FEATURE_11BE
static
void wlan_mlme_set_puncture(struct wlan_channel *des_chan,
			    uint16_t puncture_bitmap)
{
	des_chan->puncture_bitmap = puncture_bitmap;
}
#else
static
void wlan_mlme_set_puncture(struct wlan_channel *des_chan,
			    uint16_t puncture_bitmap)
{
}
#endif

static QDF_STATUS wlan_mlme_update_ch_width(struct wlan_objmgr_vdev *vdev,
					    uint8_t vdev_id,
					    enum phy_ch_width ch_width,
					    uint16_t puncture_bitmap,
					    qdf_freq_t sec_2g_freq)
{
	struct wlan_channel *des_chan;
	struct wlan_channel *bss_chan;
	uint16_t curr_op_freq;
	struct ch_params ch_params = {0};
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	if (!des_chan)
		return QDF_STATUS_E_FAILURE;

	bss_chan = wlan_vdev_mlme_get_bss_chan(vdev);
	if (!bss_chan)
		return QDF_STATUS_E_FAILURE;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlme_err("vdev %d: Pdev is NULL", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	ch_params.ch_width = ch_width;
	curr_op_freq = des_chan->ch_freq;

	wlan_reg_set_channel_params_for_pwrmode(pdev, curr_op_freq,
						sec_2g_freq, &ch_params,
						REG_CURRENT_PWR_MODE);

	des_chan->ch_width = ch_width;
	des_chan->ch_freq_seg1 = ch_params.center_freq_seg0;
	des_chan->ch_freq_seg2 = ch_params.center_freq_seg1;
	des_chan->ch_cfreq1 = ch_params.mhz_freq_seg0;
	des_chan->ch_cfreq2 = ch_params.mhz_freq_seg1;
	wlan_mlme_set_puncture(des_chan, puncture_bitmap);

	status = wlan_update_peer_phy_mode(des_chan, vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Failed to update phymode");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_copy(bss_chan, des_chan, sizeof(struct wlan_channel));

	mlme_legacy_debug("vdev id %d freq %d seg0 %d seg1 %d ch_width %d mhz seg0 %d mhz seg1 %d",
			  vdev_id, curr_op_freq, ch_params.center_freq_seg0,
			  ch_params.center_freq_seg1, ch_params.ch_width,
			  ch_params.mhz_freq_seg0, ch_params.mhz_freq_seg1);

	return QDF_STATUS_SUCCESS;
}

static uint32_t
wlan_mlme_get_vht_rate_flags(enum phy_ch_width ch_width)
{
	uint32_t rate_flags = 0;

	if (ch_width == CH_WIDTH_80P80MHZ || ch_width == CH_WIDTH_160MHZ)
		rate_flags |= TX_RATE_VHT160 | TX_RATE_VHT80 | TX_RATE_VHT40 |
				TX_RATE_VHT20;
	if (ch_width == CH_WIDTH_80MHZ)
		rate_flags |= TX_RATE_VHT80 | TX_RATE_VHT40 | TX_RATE_VHT20;
	else if (ch_width)
		rate_flags |= TX_RATE_VHT40 | TX_RATE_VHT20;
	else
		rate_flags |= TX_RATE_VHT20;
	return rate_flags;
}

static uint32_t wlan_mlme_get_ht_rate_flags(enum phy_ch_width ch_width)
{
	uint32_t rate_flags = 0;

	if (ch_width)
		rate_flags |= TX_RATE_HT40 | TX_RATE_HT20;
	else
		rate_flags |= TX_RATE_HT20;

	return rate_flags;
}

#ifdef WLAN_FEATURE_11BE
static uint32_t
wlan_mlme_get_eht_rate_flags(enum phy_ch_width ch_width)
{
	uint32_t rate_flags = 0;

	if (ch_width == CH_WIDTH_320MHZ)
		rate_flags |= TX_RATE_EHT320 | TX_RATE_EHT160 |
				TX_RATE_EHT80 | TX_RATE_EHT40 | TX_RATE_EHT20;
	else if (ch_width == CH_WIDTH_160MHZ || ch_width == CH_WIDTH_80P80MHZ)
		rate_flags |= TX_RATE_EHT160 | TX_RATE_EHT80 | TX_RATE_EHT40 |
				TX_RATE_EHT20;
	else if (ch_width == CH_WIDTH_80MHZ)
		rate_flags |= TX_RATE_EHT80 | TX_RATE_EHT40 | TX_RATE_EHT20;
	else if (ch_width)
		rate_flags |= TX_RATE_EHT40 | TX_RATE_EHT20;
	else
		rate_flags |= TX_RATE_EHT20;

	return rate_flags;
}

static QDF_STATUS
wlan_mlme_set_bss_rate_flags_eht(uint32_t *rate_flags, uint8_t eht_present,
				 enum phy_ch_width ch_width)
{
	if (!eht_present)
		return QDF_STATUS_E_NOSUPPORT;

	*rate_flags |= wlan_mlme_get_eht_rate_flags(ch_width);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
wlan_mlme_set_bss_rate_flags_eht(uint32_t *rate_flags, uint8_t eht_present,
				 enum phy_ch_width ch_width)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#ifdef WLAN_FEATURE_11AX
static uint32_t wlan_mlme_get_he_rate_flags(enum phy_ch_width ch_width)
{
	uint32_t rate_flags = 0;

	if (ch_width == CH_WIDTH_160MHZ ||
	    ch_width == CH_WIDTH_80P80MHZ)
		rate_flags |= TX_RATE_HE160 | TX_RATE_HE80 | TX_RATE_HE40 |
				TX_RATE_HE20;
	else if (ch_width == CH_WIDTH_80MHZ)
		rate_flags |= TX_RATE_HE80 | TX_RATE_HE40 | TX_RATE_HE20;
	else if (ch_width)
		rate_flags |= TX_RATE_HE40 | TX_RATE_HE20;
	else
		rate_flags |= TX_RATE_HE20;

	return rate_flags;
}

static QDF_STATUS wlan_mlme_set_bss_rate_flags_he(uint32_t *rate_flags,
						  uint8_t he_present,
						  enum phy_ch_width ch_width)
{
	if (!he_present)
		return QDF_STATUS_E_NOSUPPORT;

	*rate_flags |= wlan_mlme_get_he_rate_flags(ch_width);

	return QDF_STATUS_SUCCESS;
}

#else
static inline QDF_STATUS
wlan_mlme_set_bss_rate_flags_he(uint32_t *rate_flags,
				uint8_t he_present,
				enum phy_ch_width ch_width)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

static QDF_STATUS
wlan_mlme_cp_stats_set_rate_flags(struct wlan_objmgr_vdev *vdev,
				  uint32_t flags)
{
	struct vdev_mc_cp_stats *vdev_mc_stats;
	struct vdev_cp_stats *vdev_cp_stats_priv;

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;
	vdev_mc_stats->tx_rate_flags = flags;
	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_update_bss_rate_flags(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				enum phy_ch_width cw, uint8_t eht_present,
				uint8_t he_present, uint8_t vht_present,
				uint8_t ht_present)
{
	uint32_t *rate_flags;
	struct vdev_mlme_obj *vdev_mlme;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	if (!eht_present && !he_present && !vht_present && !ht_present)
		return QDF_STATUS_E_INVAL;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_HDD_ID_OBJ_MGR);
	if (!vdev) {
		mlme_debug("vdev: %d vdev not found", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_debug("vdev: %d mlme obj not found", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);
		return QDF_STATUS_E_INVAL;
	}

	rate_flags = &vdev_mlme->mgmt.rate_info.rate_flags;
	*rate_flags = 0;

	status = wlan_mlme_set_bss_rate_flags_eht(rate_flags, eht_present, cw);
	if (QDF_IS_STATUS_ERROR(status)) {
		status = wlan_mlme_set_bss_rate_flags_he(rate_flags,
							 he_present, cw);
		if (QDF_IS_STATUS_ERROR(status)) {
			if (vht_present)
				*rate_flags = wlan_mlme_get_vht_rate_flags(cw);
			else if (ht_present)
				*rate_flags |= wlan_mlme_get_ht_rate_flags(cw);
		}
	}

	mlme_debug("vdev:%d, eht:%u, he:%u, vht:%u, ht:%u, flag:%x, cw:%d",
		   vdev_id, eht_present, he_present, vht_present, ht_present,
		   *rate_flags, cw);

	status = wlan_mlme_cp_stats_set_rate_flags(vdev, *rate_flags);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_HDD_ID_OBJ_MGR);
	return status;
}

QDF_STATUS
wlan_mlme_send_ch_width_update_with_notify(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev,
					   uint8_t vdev_id,
					   enum phy_ch_width ch_width)
{
	QDF_STATUS status;
	wmi_host_channel_width wmi_chan_width;
	enum phy_ch_width associated_ch_width, omn_ie_ch_width;
	struct wlan_channel *des_chan;
	struct mlme_legacy_priv *mlme_priv;
	qdf_freq_t sec_2g_freq = 0;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return QDF_STATUS_E_INVAL;

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	if (!des_chan)
		return QDF_STATUS_E_INVAL;

	omn_ie_ch_width =
		mlme_priv->connect_info.assoc_chan_info.omn_ie_ch_width;
	if (omn_ie_ch_width != CH_WIDTH_INVALID && ch_width > omn_ie_ch_width) {
		mlme_debug("vdev %d: Invalid new chwidth:%d, omn_ie_cw:%d",
			   vdev_id, ch_width, omn_ie_ch_width);
		return QDF_STATUS_E_INVAL;
	}

	associated_ch_width =
		mlme_priv->connect_info.assoc_chan_info.assoc_ch_width;
	if (associated_ch_width == CH_WIDTH_INVALID ||
	    ch_width > associated_ch_width) {
		mlme_debug("vdev %d: Invalid new chwidth:%d, assoc ch_width:%d",
			   vdev_id, ch_width, associated_ch_width);
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_reg_is_24ghz_ch_freq(des_chan->ch_freq)) {
		if (ch_width == CH_WIDTH_40MHZ &&
		    mlme_priv->connect_info.assoc_chan_info.sec_2g_freq) {
			sec_2g_freq =
			mlme_priv->connect_info.assoc_chan_info.sec_2g_freq;
		} else if (ch_width != CH_WIDTH_20MHZ) {
			mlme_debug("vdev %d: CW:%d update not supported for freq:%d sec_2g_freq %d",
				   vdev_id, ch_width, des_chan->ch_freq,
				   mlme_priv->connect_info.assoc_chan_info.sec_2g_freq);
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	/* update ch width to internal host structure */
	status = wlan_mlme_update_ch_width(vdev, vdev_id, ch_width, 0,
					   sec_2g_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("vdev %d: Failed to update CW:%d to host, status:%d",
			 vdev_id, ch_width, status);
		return status;
	}

	wmi_chan_width = target_if_phy_ch_width_to_wmi_chan_width(ch_width);

	/* update ch width to fw */
	status = wlan_mlme_update_vdev_chwidth_with_notify(psoc, vdev, vdev_id,
							   wmi_chan_width);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("vdev %d: Failed to update CW:%d to fw, status:%d",
			 vdev_id, ch_width, status);

	return status;
}

enum phy_ch_width
wlan_mlme_convert_vht_op_bw_to_phy_ch_width(uint8_t channel_width,
					    uint8_t chan_id,
					    uint8_t ccfs0,
					    uint8_t ccfs1)
{
	/** channel_width in vht op from 802.11-2020
	 * Set to 0 for 20 MHz or 40 MHz BSS bandwidth.
	 * Set to 1 for 80 MHz, 160 MHz or 80+80 MHz BSS
	 * bandwidth.
	 * Set to 2 for 160 MHz BSS bandwidth (deprecated).
	 * Set to 3 for noncontiguous 80+80 MHz BSS
	 * bandwidth (deprecated).
	 * Values in the range 4 to 255 are reserved
	 *
	 * 80+80 not supported by MCC platform, so downgrade to 80
	 */
	enum phy_ch_width phy_bw = CH_WIDTH_20MHZ;

	if (channel_width == WLAN_VHTOP_CHWIDTH_2040) {
		phy_bw = CH_WIDTH_20MHZ;
		if (abs(ccfs0 - chan_id) == 2)
			phy_bw = CH_WIDTH_40MHZ;
	} else if (channel_width == WLAN_VHTOP_CHWIDTH_80) {
		if (ccfs1 && (abs(ccfs1 - ccfs0) == 8))
			phy_bw = CH_WIDTH_160MHZ;
		else
			phy_bw = CH_WIDTH_80MHZ;
	} else if (channel_width == WLAN_VHTOP_CHWIDTH_160) {
		phy_bw = CH_WIDTH_160MHZ;
	} else if (channel_width == WLAN_VHTOP_CHWIDTH_80_80) {
		phy_bw = WLAN_VHTOP_CHWIDTH_80;
	}

	return phy_bw;
}

enum phy_ch_width
wlan_mlme_convert_he_6ghz_op_bw_to_phy_ch_width(uint8_t channel_width,
						uint8_t chan_id,
						uint8_t ccfs0,
						uint8_t ccfs1)
{
	enum phy_ch_width phy_bw = CH_WIDTH_20MHZ;

	if (channel_width == WLAN_HE_6GHZ_CHWIDTH_20) {
		phy_bw = CH_WIDTH_20MHZ;
	} else if (channel_width == WLAN_HE_6GHZ_CHWIDTH_40) {
		phy_bw = CH_WIDTH_40MHZ;
	} else if (channel_width == WLAN_HE_6GHZ_CHWIDTH_80) {
		phy_bw = CH_WIDTH_80MHZ;
	} else if (channel_width == WLAN_HE_6GHZ_CHWIDTH_160_80_80) {
		phy_bw = CH_WIDTH_160MHZ;
		/* 80+80 not supported */
		if (ccfs1 && abs(ccfs0 - ccfs1) > 8)
			phy_bw = CH_WIDTH_80MHZ;
	}

	return phy_bw;
}

void
wlan_mlme_set_edca_pifs_param(struct wlan_edca_pifs_param_ie *ep,
			      enum host_edca_param_type type)
{
	ep->edca_param_type = type;

	if (type == HOST_EDCA_PARAM_TYPE_AGGRESSIVE) {
		ep->edca_pifs_param.eparam.acvo_aifsn = CFG_EDCA_PARAM_AIFSN;
		ep->edca_pifs_param.eparam.acvo_acm = CFG_EDCA_PARAM_ACM;
		ep->edca_pifs_param.eparam.acvo_aci = CFG_EDCA_PARAM_ACI;
		ep->edca_pifs_param.eparam.acvo_cwmin = CFG_EDCA_PARAM_CWMIN;
		ep->edca_pifs_param.eparam.acvo_cwmax = CFG_EDCA_PARAM_CWMAX;
		ep->edca_pifs_param.eparam.acvo_txoplimit = CFG_EDCA_PARAM_TXOP;
	} else if (type == HOST_EDCA_PARAM_TYPE_PIFS) {
		ep->edca_pifs_param.pparam.sap_pifs_offset =
						CFG_PIFS_PARAM_SAP_OFFSET;
		ep->edca_pifs_param.pparam.leb_pifs_offset =
						CFG_PIFS_PARAM_LEB_OFFSET;
		ep->edca_pifs_param.pparam.reb_pifs_offset =
						CFG_PIFS_PARAM_REB_OFFSET;
	}
}

QDF_STATUS
wlan_mlme_stats_get_periodic_display_time(struct wlan_objmgr_psoc *psoc,
					  uint32_t *periodic_display_time)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*periodic_display_time =
			cfg_default(CFG_PERIODIC_STATS_DISPLAY_TIME);
		return QDF_STATUS_E_INVAL;
	}

	*periodic_display_time =
		mlme_obj->cfg.stats.stats_periodic_display_time;

	return QDF_STATUS_SUCCESS;
}

bool
wlan_mlme_is_bcn_prot_disabled_for_sap(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_DISABLE_SAP_BCN_PROT);

	return mlme_obj->cfg.sap_cfg.disable_bcn_prot;
}

uint8_t *wlan_mlme_get_src_addr_from_frame(struct element_info *frame)
{
	struct wlan_frame_hdr *hdr;

	if (!frame || !frame->len || frame->len < WLAN_MAC_HDR_LEN_3A)
		return NULL;

	hdr = (struct wlan_frame_hdr *)frame->ptr;

	return hdr->i_addr2;
}

bool
wlan_mlme_get_sap_ps_with_twt(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_SAP_PS_WITH_TWT);

	return mlme_obj->cfg.sap_cfg.sap_ps_with_twt_enable;
}

/**
 * set_omi_ch_width() - set OMI ch_bw/eht_ch_bw_ext bit value from channel width
 * @ch_width: channel width
 * @omi_data: Pointer to omi_data object
 *
 * If the channel width is 20Mhz, 40Mhz, 80Mhz, 160Mhz and 80+80Mhz ch_bw set
 * to 0, 1, 2, 3 accordingly, if channel width is 320Mhz then eht_ch_bw_ext
 * set to 1
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF_STATUS_E_INVAL on failure
 */
static QDF_STATUS
set_omi_ch_width(enum phy_ch_width ch_width, struct omi_ctrl_tx *omi_data)
{
	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		omi_data->ch_bw = 0;
		break;
	case CH_WIDTH_40MHZ:
		omi_data->ch_bw = 1;
		break;
	case CH_WIDTH_80MHZ:
		omi_data->ch_bw = 2;
		break;
	case CH_WIDTH_160MHZ:
	case CH_WIDTH_80P80MHZ:
		omi_data->ch_bw = 3;
		break;
	case CH_WIDTH_320MHZ:
		omi_data->eht_ch_bw_ext = 1;
		break;
	default:
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_ul_mu_config(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   uint8_t ulmu_disable)
{
	struct omi_ctrl_tx omi_data = {0};
	uint32_t param_val = 0;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	enum phy_ch_width ch_width;
	uint8_t rx_nss, tx_nsts;
	struct qdf_mac_addr macaddr = {0};
	enum wlan_phymode peer_phymode;
	qdf_freq_t op_chan_freq;
	qdf_freq_t freq_seg_0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev) {
		mlme_err("vdev %d: vdev is NULL", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlme_err("pdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto err;
	}

	if (!cm_is_vdevid_connected(pdev, vdev_id)) {
		mlme_err("STA is not connected, Session_id: %d", vdev_id);
		status = QDF_STATUS_E_INVAL;
		goto err;
	}

	status = wlan_vdev_get_bss_peer_mac(vdev, &macaddr);
	if (QDF_STATUS_SUCCESS != status) {
		mlme_err("Failed to get bss peer mac, Err : %d", status);
		goto err;
	}

	status = mlme_get_peer_phymode(psoc, macaddr.bytes, &peer_phymode);
	if (QDF_STATUS_SUCCESS != status) {
		mlme_err("Failed to get peer phymode, Err : %d", status);
		goto err;
	}

	if (!(IS_WLAN_PHYMODE_HE(peer_phymode) ||
	      IS_WLAN_PHYMODE_EHT(peer_phymode))) {
		mlme_err("Invalid mode");
		status = QDF_STATUS_E_INVAL;
		goto err;
	}

	status = wlan_mlme_get_sta_rx_nss(psoc, vdev, &rx_nss);
	if (QDF_STATUS_SUCCESS != status) {
		mlme_err("Failed to get sta_rx_nss, Err : %d", status);
		goto err;
	}

	status = wlan_mlme_get_sta_tx_nss(psoc, vdev, &tx_nsts);
	if (QDF_STATUS_SUCCESS != status) {
		mlme_err("Failed to get sta_tx_nss, Err : %d", status);
		goto err;
	}

	status = wlan_get_op_chan_freq_info_vdev_id(pdev, vdev_id,
						    &op_chan_freq,
						    &freq_seg_0, &ch_width);
	if (QDF_STATUS_SUCCESS != status) {
		mlme_err("Failed to get bw, Err : %d", status);
		goto err;
	}

	omi_data.omi_in_vht = 0x1;
	omi_data.omi_in_he = 0x1;
	omi_data.a_ctrl_id = 0x1;

	status = set_omi_ch_width(ch_width, &omi_data);
	if (QDF_STATUS_SUCCESS != status) {
		mlme_err("Failed to set bw, Err : %d", status);
		goto err;
	}

	omi_data.rx_nss = rx_nss - 1;
	omi_data.tx_nsts = tx_nsts - 1;
	omi_data.ul_mu_dis = ulmu_disable;
	omi_data.ul_mu_data_dis = 0;

	qdf_mem_copy(&param_val, &omi_data, sizeof(omi_data));

	mlme_debug("OMI: BW %d TxNSTS %d RxNSS %d ULMU %d OMI_VHT %d OMI_HE %d, EHT OMI: BW %d RxNSS %d TxNSS %d, param val: %08X, bssid:" QDF_MAC_ADDR_FMT,
		   omi_data.ch_bw, omi_data.tx_nsts, omi_data.rx_nss,
		   omi_data.ul_mu_dis, omi_data.omi_in_vht, omi_data.omi_in_he,
		   omi_data.eht_ch_bw_ext, omi_data.eht_rx_nss_ext,
		   omi_data.eht_tx_nss_ext, param_val,
		   QDF_MAC_ADDR_REF(macaddr.bytes));

	status = wlan_util_vdev_peer_set_param_send(vdev, macaddr.bytes,
						    WMI_PEER_PARAM_XMIT_OMI,
						    param_val);
	if (QDF_STATUS_SUCCESS != status)
		mlme_err("set_peer_param_cmd returned %d", status);

err:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
	return status;
}

uint32_t
wlan_mlme_assemble_rate_code(uint8_t preamble, uint8_t nss, uint8_t rate)
{
	uint32_t set_value;

	if (wma_get_fw_wlan_feat_caps(DOT11AX))
		set_value = ASSEMBLE_RATECODE_V1(preamble, nss, rate);
	else
		set_value = (preamble << 6) | (nss << 4) | rate;

	return set_value;
}

QDF_STATUS
wlan_mlme_set_ap_oper_ch_width(struct wlan_objmgr_vdev *vdev,
			       enum phy_ch_width ch_width)

{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev %d legacy private object is NULL",
				wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->mlme_ap.oper_ch_width = ch_width;
	mlme_debug("SAP oper ch_width: %d, vdev %d",
		   mlme_priv->mlme_ap.oper_ch_width, wlan_vdev_get_id(vdev));

	return QDF_STATUS_SUCCESS;
}

enum phy_ch_width
wlan_mlme_get_ap_oper_ch_width(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev %d legacy private object is NULL",
				wlan_vdev_get_id(vdev));
		return CH_WIDTH_INVALID;
	}

	return mlme_priv->mlme_ap.oper_ch_width;
}

QDF_STATUS
wlan_mlme_send_csa_event_status_ind(struct wlan_objmgr_vdev *vdev,
				    uint8_t csa_status)
{
	return wlan_mlme_send_csa_event_status_ind_cmd(vdev, csa_status);
}

#ifdef WLAN_FEATURE_11BE
QDF_STATUS
wlan_mlme_get_bw_no_punct(struct wlan_objmgr_psoc *psoc,
			  struct wlan_objmgr_vdev *vdev,
			  struct wlan_channel *bss_chan,
			  enum phy_ch_width *new_ch_width)
{
	uint16_t new_punct_bitmap = 0;
	enum phy_ch_width ch_width;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t country[REG_ALPHA2_LEN + 1];

	if (!wlan_mlme_get_eht_disable_punct_in_us_lpi(psoc))
		return status;

	wlan_reg_read_current_country(psoc, country);

	if (!wlan_reg_is_6ghz_chan_freq(bss_chan->ch_freq) ||
	    !bss_chan->puncture_bitmap ||
	    qdf_mem_cmp(country, "US", REG_ALPHA2_LEN) ||
	    mlme_get_best_6g_power_type(vdev) != REG_INDOOR_AP ||
	    !IS_WLAN_PHYMODE_EHT(bss_chan->ch_phymode))
		goto err;

	ch_width = bss_chan->ch_width;

	while (ch_width != CH_WIDTH_INVALID) {
		status = wlan_reg_extract_puncture_by_bw(bss_chan->ch_width,
							 bss_chan->puncture_bitmap,
							 bss_chan->ch_freq,
							 bss_chan->ch_cfreq2,
							 ch_width,
							 &new_punct_bitmap);
		if (QDF_IS_STATUS_SUCCESS(status) && new_punct_bitmap)
			ch_width = wlan_get_next_lower_bandwidth(ch_width);
		else
			break;
	}

	if (ch_width == bss_chan->ch_width)
		return QDF_STATUS_E_FAILURE;

	mlme_debug("freq %d ccfs2 %d punct 0x%x BW old %d, new %d",
		   bss_chan->ch_freq, bss_chan->ch_cfreq2, bss_chan->puncture_bitmap,
		   bss_chan->ch_width, ch_width);

	*new_ch_width = ch_width;
	bss_chan->puncture_bitmap = 0;
err:
	return status;
}

QDF_STATUS
wlan_mlme_update_bw_no_punct(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	enum phy_ch_width new_ch_width;
	struct wlan_objmgr_pdev *pdev;

	if (!wlan_mlme_get_eht_disable_punct_in_us_lpi(psoc))
		return status;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_MLME_NB_ID);
	if (!pdev) {
		sme_err("pdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("VDEV not found for vdev id : %d", vdev_id);
		goto rel_pdev;
	}

	status = wlan_mlme_get_bw_no_punct(psoc, vdev,
					   wlan_vdev_mlme_get_des_chan(vdev),
					   &new_ch_width);
	if (QDF_IS_STATUS_ERROR(status))
		goto rel_vdev;

	status = wlan_mlme_send_ch_width_update_with_notify(psoc,
							    vdev,
							    vdev_id,
							    new_ch_width);
rel_vdev:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
rel_pdev:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);

	return status;
}
#endif

QDF_STATUS
wlan_mlme_is_hs_20_btm_offload_disabled(struct wlan_objmgr_psoc *psoc,
					bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_HS_20_BTM_OFFLOAD_DISABLE);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.hs20_btm_offload_disable;

	return QDF_STATUS_SUCCESS;
}

