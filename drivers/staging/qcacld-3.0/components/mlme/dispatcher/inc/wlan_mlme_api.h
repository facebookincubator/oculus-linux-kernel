/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
/**
 * DOC: declare public APIs exposed by the mlme component
 */

#ifndef _WLAN_MLME_API_H_
#define _WLAN_MLME_API_H_

#include <wlan_mlme_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_cmn.h>
#include "sme_api.h"

#ifdef FEATURE_SET
/**
 * wlan_mlme_get_feature_info() - Get mlme features
 * @psoc: psoc context
 * @mlme_feature_set: MLME feature set info structure
 *
 * Return: None
 */
void wlan_mlme_get_feature_info(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_mlme_features *mlme_feature_set);
#endif

/**
 * wlan_mlme_get_cfg_str() - Copy the uint8_t array for a particular CFG
 * @dst:       pointer to the destination buffer.
 * @cfg_str:   pointer to the cfg string structure
 * @len:       length to be copied
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_mlme_get_cfg_str(uint8_t *dst, struct mlme_cfg_str *cfg_str,
				 qdf_size_t *len);

/**
 * wlan_mlme_set_cfg_str() - Set values for a particular CFG
 * @src:            pointer to the source buffer.
 * @dst_cfg_str:    pointer to the cfg string structure to be modified
 * @len:            length to be written
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_mlme_set_cfg_str(uint8_t *src, struct mlme_cfg_str *dst_cfg_str,
				 qdf_size_t len);

/**
 * wlan_mlme_get_edca_params() - get the EDCA parameters corresponding to the
 * edca profile access category
 * @edca_params:   pointer to mlme edca parameters structure
 * @data:          data to which the parameter is to be copied
 * @edca_ac:       edca ac type enum passed to get the cfg value
 *
 * Return QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE
 *
 */
QDF_STATUS wlan_mlme_get_edca_params(struct wlan_mlme_edca_params *edca_params,
				     uint8_t *data, enum e_edca_type edca_ac);

/**
 * wlan_mlme_update_cfg_with_tgt_caps() - Update mlme cfg with tgt caps
 * @psoc: pointer to psoc object
 * @tgt_caps:  Pointer to the mlme related capability structure
 *
 * Return: None
 */
void
wlan_mlme_update_cfg_with_tgt_caps(struct wlan_objmgr_psoc *psoc,
				   struct mlme_tgt_caps *tgt_caps);

/*
 * mlme_get_wep_key() - get the wep key to process during auth frame
 * @vdev: VDEV object for which the wep key is being requested
 * @wep_params: cfg wep parameters structure
 * @wep_key_id: default key number
 * @default_key: default key to be copied
 * @key_len: length of the key to copy
 *
 * Return QDF_STATUS
 */
QDF_STATUS mlme_get_wep_key(struct wlan_objmgr_vdev *vdev,
			    struct wlan_mlme_wep_cfg *wep_params,
			    enum wep_key_id wep_keyid, uint8_t *default_key,
			    qdf_size_t *key_len);

/**
 * wlan_mlme_get_tx_power() - Get the max tx power in particular band
 * @psoc: pointer to psoc object
 * @band: 2ghz/5ghz band
 *
 * Return: value of tx power in the respective band
 */
uint8_t wlan_mlme_get_tx_power(struct wlan_objmgr_psoc *psoc,
			       enum band_info band);

/**
 * wlan_mlme_get_power_usage() - Get the power usage info
 * @psoc: pointer to psoc object
 *
 * Return: pointer to character array of power usage
 */
char *wlan_mlme_get_power_usage(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_enable_deauth_to_disassoc_map() - Get the deauth to disassoc
 * map
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_enable_deauth_to_disassoc_map(struct wlan_objmgr_psoc *psoc,
					    bool *value);

/**
 * wlan_mlme_get_ht_cap_info() - Get the HT cap info config
 * @psoc: pointer to psoc object
 * @ht_cap_info: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     *ht_cap_info);

/**
 * wlan_mlme_get_manufacturer_name() - get manufacturer name
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets manufacturer name
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_manufacturer_name(struct wlan_objmgr_psoc *psoc,
				uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_model_number() - get model number
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets model number
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_model_number(struct wlan_objmgr_psoc *psoc,
			   uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_model_name() - get model name
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets model name
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_model_name(struct wlan_objmgr_psoc *psoc,
			 uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_manufacture_product_name() - get manufacture product name
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets manufacture product name
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_manufacture_product_name(struct wlan_objmgr_psoc *psoc,
				       uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_manufacture_product_version() - get manufacture product version
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets manufacture product version
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_manufacture_product_version(struct wlan_objmgr_psoc *psoc,
					  uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_set_ht_cap_info() - Set the HT cap info config
 * @psoc: pointer to psoc object
 * @ht_cap_info: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     ht_cap_info);

/**
 * wlan_mlme_get_max_amsdu_num() - get the max amsdu num
 * @psoc: pointer to psoc object
 * @value: pointer to the value where the max_amsdu num is to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t *value);

/**
 * wlan_mlme_set_max_amsdu_num() - set the max amsdu num
 * @psoc: pointer to psoc object
 * @value: value to be set for max_amsdu_num
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_set_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t value);

/**
 * wlan_mlme_get_ht_mpdu_density() - get the ht mpdu density
 * @psoc: pointer to psoc object
 * @value: pointer to the value where the ht mpdu density is to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t *value);

/**
 * wlan_mlme_set_ht_mpdu_density() - set the ht mpdu density
 * @psoc: pointer to psoc object
 * @value: value to be set for ht mpdu density
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_set_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t value);

/**
 * wlan_mlme_get_band_capability() - Get the Band capability config
 * @psoc: pointer to psoc object
 * @band_capability: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t *band_capability);

#ifdef QCA_MULTIPASS_SUPPORT
/**
 * wlan_mlme_peer_config_vlan() - send vlan id to FW for RX path
 * @vdev: vdev pointer
 * @mac_addr: mac address of the peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_peer_config_vlan(struct wlan_objmgr_vdev *vdev,
			   uint8_t *mac_addr);
#else
static inline QDF_STATUS
wlan_mlme_peer_config_vlan(struct wlan_objmgr_vdev *vdev,
			   uint8_t *mac_addr)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#ifdef MULTI_CLIENT_LL_SUPPORT
/**
 * wlan_mlme_get_wlm_multi_client_ll_caps() - Get the wlm multi client latency
 * level capability flag
 * @psoc: pointer to psoc object
 *
 * Return: True is multi client ll cap present
 */
bool wlan_mlme_get_wlm_multi_client_ll_caps(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
wlan_mlme_get_wlm_multi_client_ll_caps(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

#ifdef FEATURE_WLAN_CH_AVOID_EXT
/**
 * wlan_mlme_get_coex_unsafe_chan_nb_user_prefer() - get coex unsafe nb
 * support
 * @psoc:   pointer to psoc object
 *
 * Return: coex_unsafe_chan_nb_user_prefer
 */
bool wlan_mlme_get_coex_unsafe_chan_nb_user_prefer(
		struct wlan_objmgr_psoc *psoc);
#else
static inline
bool wlan_mlme_get_coex_unsafe_chan_nb_user_prefer(
		struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * wlan_mlme_set_band_capability() - Set the Band capability config
 * @psoc: pointer to psoc object
 * @band_capability: Value to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t band_capability);

#ifdef WLAN_VENDOR_HANDOFF_CONTROL
/**
 * wlan_mlme_get_vendor_handoff_control_caps() - Get the vendor handoff control
 * capability flag
 * @psoc: pointer to psoc object
 *
 * Return: True if vendor handoff control caps present
 */
bool wlan_mlme_get_vendor_handoff_control_caps(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
wlan_mlme_get_vendor_handoff_control_caps(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * wlan_mlme_set_dual_sta_policy() - Set the dual sta config
 * @psoc: pointer to psoc object
 * @dual_sta_config: Value to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_dual_sta_policy(struct wlan_objmgr_psoc *psoc,
					 uint8_t dual_sta_config);

/**
 * wlan_mlme_get_dual_sta_policy() - Get the dual sta policy
 * @psoc: pointer to psoc object
 * @dual_sta_config: Value to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_dual_sta_policy(struct wlan_objmgr_psoc *psoc,
					 uint8_t *dual_sta_config);

/**
 * wlan_mlme_convert_ap_policy_config() - Convert vendor attr ap policy
 * config to host enum
 * @ap_config: Value to convert
 *
 * Return: enum host_concurrent_ap_policy
 */
enum host_concurrent_ap_policy
wlan_mlme_convert_ap_policy_config(
		enum qca_wlan_concurrent_ap_policy_config ap_config);

/**
 * wlan_mlme_set_ap_policy() - Set ap config policy value
 * @vdev: pointer to vdev object
 * @ap_cfg_policy: Value to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_set_ap_policy(struct wlan_objmgr_vdev *vdev,
			enum host_concurrent_ap_policy ap_cfg_policy);

/**
 * wlan_mlme_get_ap_policy() - Get ap config policy value
 * @vdev: pointer to vdev object
 *
 * Return: enum host_concurrent_ap_policy
 */
enum host_concurrent_ap_policy
wlan_mlme_get_ap_policy(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlme_get_prevent_link_down() - Get the prevent link down config
 * @psoc: pointer to psoc object
 * @prevent_link_down: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_prevent_link_down(struct wlan_objmgr_psoc *psoc,
					   bool *prevent_link_down);

/**
 * wlan_mlme_get_select_5ghz_margin() - Get the select 5Ghz margin config
 * @psoc: pointer to psoc object
 * @select_5ghz_margin: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_select_5ghz_margin(struct wlan_objmgr_psoc *psoc,
					    uint8_t *select_5ghz_margin);

/**
 * wlan_mlme_get_rtt_mac_randomization() - Get the RTT MAC randomization config
 * @psoc: pointer to psoc object
 * @rtt_mac_randomization: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_rtt_mac_randomization(struct wlan_objmgr_psoc *psoc,
					       bool *rtt_mac_randomization);

/**
 * wlan_mlme_get_crash_inject() - Get the crash inject config
 * @psoc: pointer to psoc object
 * @crash_inject: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_crash_inject(struct wlan_objmgr_psoc *psoc,
				      bool *crash_inject);

/**
 * wlan_mlme_get_lpass_support() - Get the LPASS Support config
 * @psoc: pointer to psoc object
 * @lpass_support: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_lpass_support(struct wlan_objmgr_psoc *psoc,
				       bool *lpass_support);

/**
 * wlan_mlme_get_wls_6ghz_cap() - Get the wifi location service(WLS)
 * 6ghz capability
 * @psoc: pointer to psoc object
 * @wls_6ghz_capable: Pointer to the variable from caller
 *
 * Return: void
 */
void wlan_mlme_get_wls_6ghz_cap(struct wlan_objmgr_psoc *psoc,
				bool *wls_6ghz_capable);

/**
 * wlan_mlme_get_self_recovery() - Get the self recovery config
 * @psoc: pointer to psoc object
 * @self_recovery: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_self_recovery(struct wlan_objmgr_psoc *psoc,
				       bool *self_recovery);

/**
 * wlan_mlme_get_sub_20_chan_width() - Get the sub 20 chan width config
 * @psoc: pointer to psoc object
 * @sub_20_chan_width: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sub_20_chan_width(struct wlan_objmgr_psoc *psoc,
					   uint8_t *sub_20_chan_width);

/**
 * wlan_mlme_get_fw_timeout_crash() - Get the fw timeout crash config
 * @psoc: pointer to psoc object
 * @fw_timeout_crash: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_fw_timeout_crash(struct wlan_objmgr_psoc *psoc,
					  bool *fw_timeout_crash);

/**
 * wlan_mlme_get_ito_repeat_count() - Get the fw timeout crash config
 * @psoc: pointer to psoc object
 * @ito_repeat_count: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ito_repeat_count(struct wlan_objmgr_psoc *psoc,
					  uint8_t *ito_repeat_count);

/**
 * wlan_mlme_get_acs_with_more_param() - Get the acs_with_more_param flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_acs_with_more_param(struct wlan_objmgr_psoc *psoc,
					     bool *value);

/**
 * wlan_mlme_get_auto_channel_weight() - Get the auto channel weight
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_auto_channel_weight(struct wlan_objmgr_psoc *psoc,
					     uint32_t *value);

/**
 * wlan_mlme_get_vendor_acs_support() - Get the vendor based channel selece
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */

QDF_STATUS wlan_mlme_get_vendor_acs_support(struct wlan_objmgr_psoc *psoc,
					    bool *value);

/**
 * wlan_mlme_get_acs_support_for_dfs_ltecoex() - Get the flag for
 *						 acs support for dfs ltecoex
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_acs_support_for_dfs_ltecoex(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_get_external_acs_policy() - Get the flag for external acs policy
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_external_acs_policy(struct wlan_objmgr_psoc *psoc,
				  bool *value);

/**
 * wlan_mlme_get_sap_inactivity_override() - Check if sap max inactivity
 * override flag is set.
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
void wlan_mlme_get_sap_inactivity_override(struct wlan_objmgr_psoc *psoc,
					   bool *value);

/**
 * wlan_mlme_get_ignore_peer_ht_mode() - Get the ignore peer ht opmode flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ignore_peer_ht_mode(struct wlan_objmgr_psoc *psoc,
					bool *value);
/**
 * wlan_mlme_get_tx_chainmask_cck() - Get the tx_chainmask_cfg value
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_tx_chainmask_cck(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_get_tx_chainmask_1ss() - Get the tx_chainmask_1ss value
 * @psoc: pointer to psoc object
 * @value: Value that caller needs to get
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_tx_chainmask_1ss(struct wlan_objmgr_psoc *psoc,
					  uint8_t *value);

/**
 * wlan_mlme_get_num_11b_tx_chains() -  Get the number of 11b only tx chains
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_num_11b_tx_chains(struct wlan_objmgr_psoc *psoc,
					   uint16_t *value);

/**
 * wlan_mlme_get_num_11ag_tx_chains() - get the total number of 11a/g tx chains
 * @psoc: pointer to psoc object
 * @value: Value that caller needs to get
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_num_11ag_tx_chains(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value);

/**
 * wlan_mlme_get_bt_chain_separation_flag() - get the enable_bt_chain_separation
 * flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_bt_chain_separation_flag(struct wlan_objmgr_psoc *psoc,
						  bool *value);
/**
 * wlan_mlme_configure_chain_mask() - configure chainmask parameters
 * @psoc: pointer to psoc object
 * @session_id: vdev_id
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_configure_chain_mask(struct wlan_objmgr_psoc *psoc,
					  uint8_t session_id);

/**
 * wlan_mlme_is_chain_mask_supported() - check if configure chainmask can
 * be supported
 * @psoc: pointer to psoc object
 *
 * Return: true if supported else false
 */
bool wlan_mlme_is_chain_mask_supported(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_listen_interval() - Get listen interval
 * @psoc: pointer to psoc object
 * @value: Pointer to value that needs to be filled by MLME
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int *value);

/**
 * wlan_mlme_set_sap_listen_interval() - Set the sap listen interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int value);

/**
 * wlan_mlme_set_assoc_sta_limit() - Set the assoc sta limit
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int value);

/**
 * wlan_mlme_get_assoc_sta_limit() - Get the assoc sta limit
 * @psoc: pointer to psoc object
 * @value: Pointer to value that needs to be filled by MLME
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int *value);

/**
 * wlan_mlme_get_sap_get_peer_info() - get the sap get peer info
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool *value);

/**
 * wlan_mlme_set_sap_get_peer_info() - set the sap get peer info
 * @psoc: pointer to psoc object
 * @value: value to overwrite the sap get peer info
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool value);

/**
 * wlan_mlme_get_sap_bcast_deauth_enabled() - get the enable/disable value
 *                                           for broadcast deauth in sap
 * @psoc: pointer to psoc object
 * @value: Value that needs to get from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sap_bcast_deauth_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *value);

/**
 * wlan_mlme_get_sap_allow_all_channels() - get the value of sap allow all
 * channels
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_allow_all_channels(struct wlan_objmgr_psoc *psoc,
						bool *value);

/**
 * wlan_mlme_is_6g_sap_fd_enabled() - get the enable/disable value
 *                                     for 6g sap fils discovery
 * @psoc: pointer to psoc object
 * @value: Value that needs to get from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_is_6g_sap_fd_enabled(struct wlan_objmgr_psoc *psoc,
			       bool *value);

/**
 * wlan_mlme_get_sap_max_peers() - get the value sap max peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int *value);

/**
 * wlan_mlme_set_sap_max_peers() - set the value sap max peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int value);

/**
 * wlan_mlme_get_sap_max_offload_peers() - get the value sap max offload peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_offload_peers(struct wlan_objmgr_psoc *psoc,
					       int *value);

/**
 * wlan_mlme_get_sap_max_offload_reorder_buffs() - get the value sap max offload
 * reorder buffs.
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_offload_reorder_buffs(struct wlan_objmgr_psoc
						       *psoc, int *value);

/**
 * wlan_mlme_get_sap_chn_switch_bcn_count() - get the value sap max channel
 * switch beacon count
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chn_switch_bcn_count(struct wlan_objmgr_psoc *psoc,
						  int *value);

/**
 * wlan_mlme_get_sap_chn_switch_mode() - get the sap channel
 * switch mode
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chn_switch_mode(struct wlan_objmgr_psoc *psoc,
					     bool *value);

/**
 * wlan_mlme_get_sap_internal_restart() - get the sap internal
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_internal_restart(struct wlan_objmgr_psoc *psoc,
					      bool *value);
/**
 * wlan_mlme_get_sap_max_modulated_dtim() - get the max modulated dtim
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_modulated_dtim(struct wlan_objmgr_psoc *psoc,
						uint8_t *value);

/**
 * wlan_mlme_get_sap_chan_pref_location() - get the sap chan pref location
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chan_pref_location(struct wlan_objmgr_psoc *psoc,
						uint8_t *value);

/**
 * wlan_mlme_get_sap_country_priority() - get the sap country code priority
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_country_priority(struct wlan_objmgr_psoc *psoc,
					      bool *value);

/**
 * wlan_mlme_get_sap_reduced_beacon_interval() - get the sap reduced
 * beacon interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_reduced_beacon_interval(struct wlan_objmgr_psoc
						     *psoc, int *value);

/**
 * wlan_mlme_get_sap_chan_switch_rate_enabled() - get the sap rate hostapd
 * enabled beacon interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chan_switch_rate_enabled(struct wlan_objmgr_psoc
						      *psoc, bool *value);

/**
 * wlan_mlme_get_sap_force_11n_for_11ac() - get the sap 11n for 11ac
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_force_11n_for_11ac(struct wlan_objmgr_psoc
						*psoc, bool *value);

/**
 * wlan_mlme_get_go_force_11n_for_11ac() - get the go 11n for 11ac
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_go_force_11n_for_11ac(struct wlan_objmgr_psoc
					       *psoc, bool *value);

/**
 * wlan_mlme_is_go_11ac_override() - Override 11ac bandwdith for P2P GO
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_is_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					 bool *value);

/**
 * wlan_mlme_is_sap_11ac_override() - Override 11ac bandwdith for SAP
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_is_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_set_go_11ac_override() - set override 11ac bandwdith for P2P GO
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool value);

/**
 * wlan_mlme_set_sap_11ac_override() - set override 11ac bandwdith for SAP
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					   bool value);

/**
 * wlan_mlme_get_oce_sta_enabled_info() - Get the OCE feature enable
 * info for STA
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_oce_sta_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value);

/**
 * wlan_mlme_get_bigtk_support() - Get the BIGTK support
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_bigtk_support(struct wlan_objmgr_psoc *psoc,
				       bool *value);

/**
 * wlan_mlme_get_ocv_support() - Get the OCV support
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ocv_support(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_host_scan_abort_support() - Get support for stop all host
 * scans service capability.
 * @psoc: PSOC object pointer
 *
 * Return: True if capability is supported, else False
 */
bool wlan_mlme_get_host_scan_abort_support(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_dual_sta_roam_support  - Get support for dual sta roaming
 * feature
 * @psoc: PSOC object pointer
 *
 * Return: True if capability is supported, else False
 */
bool wlan_mlme_get_dual_sta_roam_support(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_oce_sap_enabled_info() - Get the OCE feature enable
 * info for SAP
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_oce_sap_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value);

/**
 * wlan_mlme_update_oce_flags() - Update the oce flags to FW
 * @pdev: pointer to pdev object
 *
 * Return: void
 */
void wlan_mlme_update_oce_flags(struct wlan_objmgr_pdev *pdev);

#ifdef WLAN_FEATURE_11AX
/**
 * wlan_mlme_cfg_get_he_ul_mumimo() - Get the HE Ul Mumio
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_get_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t *value);

/**
 * wlan_mlme_cfg_set_he_ul_mumimo() - Set the HE Ul Mumio
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_set_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t value);

/**
 * mlme_cfg_get_he_caps() - Get the HE capability info
 * @psoc: pointer to psoc object
 * @he_cap: Caps that needs to be filled.
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_cfg_get_he_caps(struct wlan_objmgr_psoc *psoc,
				tDot11fIEhe_cap *he_cap);

/**
 * wlan_mlme_cfg_get_enable_ul_mimo() - Get the HE Ul mimo
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_get_enable_ul_mimo(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * wlan_mlme_cfg_get_enable_ul_ofdm() - Get enable ul ofdm
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_get_enable_ul_ofdm(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * mlme_update_tgt_he_caps_in_cfg() - Update tgt he cap in mlme component
 * @psoc: pointer to psoc object
 * @cfg: pointer to config params from target
 *
 * This api to be used by callers to update
 * he caps in mlme.
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS mlme_update_tgt_he_caps_in_cfg(struct wlan_objmgr_psoc *psoc,
					  struct wma_tgt_cfg *cfg);
#endif

/**
 * wlan_mlme_convert_vht_op_bw_to_phy_ch_width() - convert channel width in VHT
 *                                                 operation IE to phy_ch_width
 * @channel_width: channel width in VHT operation IE. If it is 0, please use HT
 *                 information IE to check whether it is 20MHz or 40MHz.
 *
 * Return: phy_ch_width
 */
enum phy_ch_width wlan_mlme_convert_vht_op_bw_to_phy_ch_width(
						uint8_t channel_width);

/**
 * wlan_mlme_chan_stats_scan_event_cb() - process connected channel stats
 * scan event
 * @vdev: pointer to vdev object
 * @event: scan event definition
 * @arg: scan argument
 *
 * Return: none
 */
void wlan_mlme_chan_stats_scan_event_cb(struct wlan_objmgr_vdev *vdev,
					struct scan_event *event, void *arg);

/**
 * wlan_mlme_send_ch_width_update_with_notify() - update connected VDEV
 * channel bandwidth
 * @psoc: pointer to psoc object
 * @vdev: pointer to vdev object
 * @vdev_id: vdev id
 * @ch_width: channel width to update
 *
 * Return: none
 */
QDF_STATUS
wlan_mlme_send_ch_width_update_with_notify(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev,
					   uint8_t vdev_id,
					   enum phy_ch_width ch_width);

#ifdef WLAN_FEATURE_11BE
/**
 * mlme_update_tgt_eht_caps_in_cfg() - Update tgt eht cap in mlme component
 * @psoc: pointer to psoc object
 * @cfg: pointer to config params from target
 *
 * This api to be used by callers to update EHT caps in mlme.
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS mlme_update_tgt_eht_caps_in_cfg(struct wlan_objmgr_psoc *psoc,
					  struct wma_tgt_cfg *cfg);

/**
 * wlan_mlme_convert_eht_op_bw_to_phy_ch_width() - convert channel width in eht
 *                                                 operation IE to phy_ch_width
 * @channel_width: channel width in eht operation IE
 *
 * Return: phy_ch_width
 */
enum phy_ch_width wlan_mlme_convert_eht_op_bw_to_phy_ch_width(
						uint8_t channel_width);

/**
 * wlan_mlme_get_usr_disable_sta_eht() - Get user disable sta eht flag
 * @psoc: psoc object
 *
 * Return: true if user has disabled eht in connect request
 */
bool wlan_mlme_get_usr_disable_sta_eht(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_set_usr_disable_sta_eht() - Set user disable sta eht flag
 * @psoc: psoc object
 * @disable: eht disable flag
 *
 * Return: void
 */
void wlan_mlme_set_usr_disable_sta_eht(struct wlan_objmgr_psoc *psoc,
				       bool disable);
#else
static inline
bool wlan_mlme_get_usr_disable_sta_eht(struct wlan_objmgr_psoc *psoc)
{
	return true;
}

static inline
void wlan_mlme_set_usr_disable_sta_eht(struct wlan_objmgr_psoc *psoc,
				       bool disable)
{
}
#endif

/**
 * wlan_mlme_is_ap_prot_enabled() - check if sap protection is enabled
 * @psoc: pointer to psoc object
 *
 * Return: is_ap_prot_enabled flag
 */
bool wlan_mlme_is_ap_prot_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_ap_protection_mode() - Get ap_protection_mode value
 * @psoc: pointer to psoc object
 * @value: pointer to the value which needs to be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ap_protection_mode(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value);

/**
 * wlan_mlme_is_ap_obss_prot_enabled() - Get ap_obss_protection is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: pointer to the value which needs to be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_is_ap_obss_prot_enabled(struct wlan_objmgr_psoc *psoc,
					     bool *value);

/**
 * wlan_mlme_get_rts_threshold() - Get the RTS threshold config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value);

/**
 * wlan_mlme_set_rts_threshold() - Set the RTS threshold config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t value);

/**
 * wlan_mlme_get_frag_threshold() - Get the Fragmentation threshold
 *                                  config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t *value);

/**
 * wlan_mlme_set_frag_threshold() - Set the Fragmentation threshold
 *                                  config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t value);

/**
 * wlan_mlme_get_fils_enabled_info() - Get the fils enable info for driver
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool *value);
/**
 * wlan_mlme_set_fils_enabled_info() - Set the fils enable info for driver
 * @psoc: pointer to psoc object
 * @value: value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool value);

/**
 * wlan_mlme_set_primary_interface() - Set the primary iface id for driver
 * @psoc: pointer to psoc object
 * @value: value that needs to be set from the caller
 *
 * When a vdev is set as primary then based on the dual sta policy
 * "qca_wlan_concurrent_sta_policy_config" mcc preference and roaming has
 * to be enabled on the primary vdev
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_primary_interface(struct wlan_objmgr_psoc *psoc,
					   uint8_t value);

/**
 * wlan_mlme_set_default_primary_iface() - Set the default primary iface id
 * for driver
 * @psoc: pointer to psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_default_primary_iface(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_is_primary_interface_configured() - Check if primary iface is set
 * @psoc: pointer to psoc object
 *
 * Check if primary iface is configured from userspace through vendor command.
 * Return true if it's configured. If it's not configured, default value would
 * be 0xFF and return false then.
 *
 * Return: True or False
 */
bool wlan_mlme_is_primary_interface_configured(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_peer_get_assoc_rsp_ies() - Get the assoc response IEs of peer
 * @peer: WLAN peer objmgr
 * @ie_buf: Pointer to IE buffer
 * @ie_len: Length of the IE buffer
 *
 * Get the pointer to assoc response IEs of the peer from MLME
 * and length of the IE buffer.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_peer_get_assoc_rsp_ies(struct wlan_objmgr_peer *peer,
					    const uint8_t **ie_buf,
					    size_t *ie_len);

/**
 * wlan_mlme_get_mcc_duty_cycle_percentage() - Get primary STA iface duty
 * cycle percentage
 * @pdev: pointer to pdev object
 *
 * API to get the MCC duty cycle for primary and secondary STA's
 *
 * Return: primary iface quota on success
 */
int wlan_mlme_get_mcc_duty_cycle_percentage(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_mlme_get_tl_delayed_trgr_frm_int() - Get delay interval(in ms)
 * of UAPSD auto trigger
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: None
 */
void wlan_mlme_get_tl_delayed_trgr_frm_int(struct wlan_objmgr_psoc *psoc,
					   uint32_t *value);

/**
 * wlan_mlme_get_wmm_dir_ac_vi() - Get TSPEC direction
 * for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_vi(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_vi() - Get normal
 * MSDU size for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vi(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_vi() - mean data
 * rate for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
					uint32_t *value);

/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_vi() - min PHY
 * rate for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);

/**
 * wlan_mlme_get_wmm_sba_ac_vi() - surplus bandwidth
 * allowance for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_vi(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_vi_srv_intv() - Get Uapsd service
 * interval for video
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vi_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_vi_sus_intv() - Get Uapsd suspension
 * interval for video
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vi_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_dir_ac_be() - Get TSPEC direction
 * for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_be(struct wlan_objmgr_psoc *psoc,
			    uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_be() - Get normal
 * MSDU size for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_be(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_be() - mean data
 * rate for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_mean_data_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_be() - min PHY
 * rate for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);

/**
 * wlan_mlme_get_wmm_sba_ac_be() - surplus bandwidth
 * allowance for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_be(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_be_srv_intv() - Get Uapsd service
 * interval for BE
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_be_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_be_sus_intv() - Get Uapsd suspension
 * interval for BE
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_be_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_dir_ac_bk() - Get TSPEC direction
 * for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_bk(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_bk() - Get normal
 * MSDU size for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_bk(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_bk() - mean data
 * rate for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_mean_data_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_bk() - min PHY
 * rate for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);

/**
 * wlan_mlme_get_wmm_sba_ac_bk() - surplus bandwidth
 * allowance for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_bk(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_bk_srv_intv() - Get Uapsd service
 * interval for BK
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_bk_sus_intv() - Get Uapsd suspension
 * interval for BK
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_mode() - Enable WMM feature
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_mode(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_80211e_is_enabled() - Enable 802.11e feature
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_80211e_is_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_wmm_uapsd_mask() - setup U-APSD mask for ACs
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_mask(struct wlan_objmgr_psoc *psoc, uint8_t *value);

#ifdef FEATURE_WLAN_ESE
/**
 * wlan_mlme_get_inactivity_interval() - Infra Inactivity Interval
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: None
 */
void
wlan_mlme_get_inactivity_interval(struct wlan_objmgr_psoc *psoc,
				  uint32_t *value);
#endif

/**
 * wlan_mlme_get_is_ts_burst_size_enable() - Get TS burst size flag
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: None
 */
void wlan_mlme_get_is_ts_burst_size_enable(struct wlan_objmgr_psoc *psoc,
					   bool *value);

/**
 * wlan_mlme_get_ts_info_ack_policy() - Get TS ack policy
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: None
 */
void wlan_mlme_get_ts_info_ack_policy(struct wlan_objmgr_psoc *psoc,
				      enum mlme_ts_info_ack_policy *value);

/**
 * wlan_mlme_get_ts_acm_value_for_ac() - Get ACM value for AC
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_ts_acm_value_for_ac(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_wmm_dir_ac_vo() - Get TSPEC direction
 * for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_vo(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_vo() - Get normal
 * MSDU size for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vo(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_vo() - mean data rate for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_mean_data_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);
/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_vo() - min PHY
 * rate for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);
/**
 * wlan_mlme_get_wmm_sba_ac_vo() - surplus bandwidth allowance for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 *  Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_vo(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_set_enable_bcast_probe_rsp() - Set enable bcast probe resp info
 * @psoc: pointer to psoc object
 * @value: value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_enable_bcast_probe_rsp(struct wlan_objmgr_psoc *psoc,
						bool value);

/**
 * wlan_mlme_get_wmm_uapsd_vo_srv_intv() - Get Uapsd service
 * interval for voice
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vo_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_vo_sus_intv() - Get Uapsd suspension
 * interval for voice
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vo_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_cfg_get_vht_max_mpdu_len() - gets vht max mpdu length from cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_max_mpdu_len(struct wlan_objmgr_psoc *psoc,
				   uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_max_mpdu_len() - sets vht max mpdu length into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_max_mpdu_len(struct wlan_objmgr_psoc *psoc,
				   uint8_t value);

/**
 * wlan_mlme_cfg_get_ht_smps() - gets HT SM Power Save mode from cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_ht_smps(struct wlan_objmgr_psoc *psoc,
				     uint8_t *value);

/**
 * wlan_mlme_cfg_get_vht_chan_width() - gets vht supported channel width from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_chan_width(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_chan_width() - sets vht supported channel width into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_chan_width(struct wlan_objmgr_psoc *psoc,
					    uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_chan_width() - sets vht supported channel width into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_chan_width(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * wlan_mlme_cfg_get_vht_ldpc_coding_cap() - gets vht ldpc coding cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_ldpc_coding_cap(struct wlan_objmgr_psoc *psoc,
						 bool *value);

/**
 * wlan_mlme_cfg_set_vht_ldpc_coding_cap() - sets vht ldpc coding cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_ldpc_coding_cap(struct wlan_objmgr_psoc *psoc,
						 bool value);

/**
 * wlan_mlme_cfg_get_vht_short_gi_80mhz() - gets vht short gi 80MHz from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_short_gi_80mhz(struct wlan_objmgr_psoc *psoc,
						bool *value);

/**
 * wlan_mlme_cfg_set_vht_short_gi_80mhz() - sets vht short gi 80MHz into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_short_gi_80mhz(struct wlan_objmgr_psoc *psoc,
						bool value);

/**
 * wlan_mlme_cfg_get_short_gi_160_mhz() - gets vht short gi 160MHz from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_short_gi_160_mhz() - sets vht short gi 160MHz into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_tx_stbc() - gets vht tx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_tx_stbc(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_get_vht_rx_stbc() - gets vht rx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_tx_stbc() - sets vht tx stbc into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_tx_stbc(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_rx_stbc() - gets vht rx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_rx_stbc() - sets vht rx stbc into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_su_bformer() - gets vht su beam former cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_su_bformer(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_su_bformer() - sets vht su beam former cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_su_bformer(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_set_vht_su_bformee() - sets vht su beam formee cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_su_bformee(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_set_vht_tx_bfee_ant_supp() - sets vht Beamformee antenna
 * support cap
 * into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
						  uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_tx_bfee_ant_supp() - Gets vht Beamformee antenna
 * support cap into cfg item
 *
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
						  uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_num_sounding_dim() - sets vht no of sounding dimensions
 * into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_num_sounding_dim(struct wlan_objmgr_psoc *psoc,
						  uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_mu_bformer() - gets vht mu beam former cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_mu_bformer(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_mu_bformer() - sets vht mu beam former cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_mu_bformer(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_mu_bformee() - gets vht mu beam formee cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_mu_bformee(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_mu_bformee() - sets vht mu beam formee cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_mu_bformee(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_txop_ps() - gets vht tx ops ps cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_txop_ps(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_txop_ps() - sets vht tx ops ps cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_txop_ps(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_ampdu_len_exp() - gets vht max AMPDU length exponent from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_ampdu_len_exp(struct wlan_objmgr_psoc *psoc,
				    uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_ampdu_len_exp() - sets vht max AMPDU length exponent into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_ampdu_len_exp(struct wlan_objmgr_psoc *psoc,
				    uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_rx_mcs_map() - gets vht rx mcs map from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value);

/**
 * wlan_mlme_cfg_set_vht_rx_mcs_map() - sets rx mcs map into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc, uint32_t value);

/**
 * wlan_mlme_cfg_get_vht_tx_mcs_map() - gets vht tx mcs map from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value);

/**
 * wlan_mlme_cfg_set_vht_tx_mcs_map() - sets tx mcs map into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t value);

/**
 * wlan_mlme_cfg_set_vht_rx_supp_data_rate() - sets rx supported data rate into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_rx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value);

/**
 * wlan_mlme_cfg_set_vht_tx_supp_data_rate() - sets tx supported data rate into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_tx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value);

/**
 * wlan_mlme_cfg_get_vht_basic_mcs_set() - gets basic mcs set from
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_cfg_set_vht_basic_mcs_set() - sets basic mcs set into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t value);

/**
 * wlan_mlme_get_vht_enable_tx_bf() - Get vht enable tx bf
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable_tx_bf(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_tx_su_beamformer() - VHT enable tx su beamformer
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_tx_su_beamformer(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_channel_width() - gets Channel width capability
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_channel_width(struct wlan_objmgr_psoc *psoc,
					   uint8_t *value);

/**
 * wlan_mlme_get_vht_rx_mcs_8_9() - VHT Rx MCS capability for 1x1 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_rx_mcs_8_9(struct wlan_objmgr_psoc *psoc,
					uint8_t *value);

/**
 * wlan_mlme_get_vht_tx_mcs_8_9() - VHT Tx MCS capability for 1x1 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_tx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_vht_rx_mcs_2x2() - VHT Rx MCS capability for 2x2 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_rx_mcs_2x2(struct wlan_objmgr_psoc *psoc,
					uint8_t *value);

/**
 * wlan_mlme_get_vht_tx_mcs_2x2() - VHT Tx MCS capability for 2x2 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_tx_mcs_2x2(struct wlan_objmgr_psoc *psoc,
					uint8_t *value);

/**
 * wlan_mlme_get_vht20_mcs9() - Enables VHT MCS9 in 20M BW operation
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht20_mcs9(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_srd_master_mode_for_vdev  - Get SRD master mode for vdev
 * @psoc:          pointer to psoc object
 * @vdev_opmode:   vdev operating mode
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_srd_master_mode_for_vdev(struct wlan_objmgr_psoc *psoc,
				       enum QDF_OPMODE vdev_opmode,
				       bool *value);

/**
 * wlan_mlme_get_indoor_support_for_nan  - Get indoor channel support for NAN
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_indoor_support_for_nan(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_force_sap_enabled() - Get the value of force SAP enabled
 * @psoc: psoc context
 * @value: data to get
 *
 * Get the value of force SAP enabled
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_force_sap_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_enable_dynamic_nss_chains_cfg() - API to get whether dynamic
 * nss and chain config is enabled or not
 * @psoc: psoc context
 * @value: data to be set
 *
 * API to get whether dynamic nss and chain config is enabled or not
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_enable_dynamic_nss_chains_cfg(struct wlan_objmgr_psoc *psoc,
					    bool *value);

/**
 * wlan_mlme_get_restart_sap_on_dynamic_nss_chains_cfg() - API to get whether
 * SAP needs to be restarted or not on dynamic nss chain config
 * @psoc: psoc context
 * @value: data to be set
 *
 * API to get whether SAP needs to be restarted or not on dynamic nss chain
 * config
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_restart_sap_on_dynamic_nss_chains_cfg(
						struct wlan_objmgr_psoc *psoc,
						bool *value);

/**
 * wlan_mlme_get_vht_enable2x2() - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_vht_enable2x2() - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_get_vht_enable_paid() - Enables/disables paid feature
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable_paid(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_enable_gid() - Enables/disables VHT GID feature
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable_gid(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_for_24ghz() - Enables/disables VHT for 24 ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_vht_for_24ghz() - Enables/disables VHT for 24 ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_get_vendor_vht_for_24ghz() - nables/disables vendor VHT for 24 ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vendor_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * mlme_update_vht_cap() - update vht capabilities
 * @psoc: psoc context
 * @cfg: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_update_vht_cap(struct wlan_objmgr_psoc *psoc, struct wma_tgt_vht_cap *cfg);

/**
 * mlme_update_nss_vht_cap() - Update the number of spatial
 * streams supported for vht
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_update_nss_vht_cap(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_11BE
/**
 * mlme_get_bss_11be_allowed() - Check BSS allowed in 11be mode
 * @psoc: psoc context
 * @bssid: bssid
 * @ie_data: ie data
 * @ie_length: ie data length
 *
 * Return: true if AP in 11be oui allow list
 */
bool mlme_get_bss_11be_allowed(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *bssid,
			       uint8_t *ie_data,
			       uint32_t ie_length);
#else
static inline
bool mlme_get_bss_11be_allowed(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *bssid,
			       uint8_t *ie_data,
			       uint32_t ie_length)
{
	return false;
}
#endif

/**
 * wlan_mlme_is_sap_uapsd_enabled() - Get if SAP UAPSD is enabled/disabled
 * @psoc: psoc context
 * @value: value to be filled for caller
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_is_sap_uapsd_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_set_sap_uapsd_flag() - Enable/Disable SAP UAPSD
 * @psoc:  psoc context
 * @value: Enable/Disable control value for sap_uapsd_enabled field
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_set_sap_uapsd_flag(struct wlan_objmgr_psoc *psoc,
					bool value);
/**
 * wlan_mlme_is_11h_enabled() - Get the 11h flag
 * @psoc: psoc context
 * @value: Enable/Disable value ptr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_11h_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_11h_enabled() - Set the 11h flag
 * @psoc: psoc context
 * @value: Enable/Disable value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_11h_enabled(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_is_11d_enabled() - Get the 11d flag
 * @psoc: psoc context
 * @value: Enable/Disable value ptr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_11d_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_11d_enabled() - Set the 11h flag
 * @psoc: psoc context
 * @value: Enable/Disable value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_11d_enabled(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_is_rf_test_mode_enabled() - Get the rf test mode flag
 * @psoc: psoc context
 * @value: Enable/Disable value ptr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_rf_test_mode_enabled() - Set the rf test mode flag
 * @psoc: psoc context
 * @value: Enable/Disable value.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value);

#ifdef CONFIG_BAND_6GHZ
/**
 * wlan_mlme_is_disable_vlp_sta_conn_to_sp_ap_enabled() - Get the disable vlp
 *                                                       STA conn to SP AP flag
 * @psoc: psoc context
 * @value: Enable/Disable value ptr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_disable_vlp_sta_conn_to_sp_ap_enabled(
						struct wlan_objmgr_psoc *psoc,
						bool *value);

/**
 * wlan_mlme_is_standard_6ghz_conn_policy_enabled() - Get the 6 GHz standard
 *                                                    connection policy flag
 * @psoc: psoc context
 * @value: Enable/Disable value ptr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_standard_6ghz_conn_policy_enabled(struct wlan_objmgr_psoc *psoc,
					       bool *value);

#else
static inline QDF_STATUS
wlan_mlme_is_disable_vlp_sta_conn_to_sp_ap_enabled(
						struct wlan_objmgr_psoc *psoc,
						bool *value)
{
	*value = false;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_is_standard_6ghz_conn_policy_enabled(struct wlan_objmgr_psoc *psoc,
					       bool *value)
{
	*value = false;
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_mlme_get_eht_mode() - Get the EHT mode of operations
 * @psoc: psoc context
 * @value: EHT mode value ptr
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_eht_mode(struct wlan_objmgr_psoc *psoc,
		       enum wlan_eht_mode *value);

/**
 * wlan_mlme_set_eht_mode() - Set the EHT mode of operation
 * @psoc: psoc context
 * @value: EHT mode value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_eht_mode(struct wlan_objmgr_psoc *psoc, enum wlan_eht_mode value);

/**
 * wlan_mlme_get_emlsr_mode_enabled() - Get the eMLSR mode flag
 * @psoc: psoc context
 * @value: Enable/Disable value ptr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_emlsr_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_emlsr_mode_enabled() - Set the eMLSR mode flag
 * @psoc: psoc context
 * @value: Enable/Disable value.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_emlsr_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_set_eml_params() - Set EML subfields in psoc mlme obj that
 * are received from FW
 * @psoc: psoc context
 * @cap: psoc mac/phy capability ptr
 *
 * Return: none
 */
void
wlan_mlme_set_eml_params(struct wlan_objmgr_psoc *psoc,
			 struct wlan_psoc_host_mac_phy_caps_ext2 *cap);

/**
 * wlan_mlme_get_eml_params() - Get EML subfields from psoc mlme obj
 * @psoc: psoc context
 * @cap: EML capability subfield ptr
 *
 * Return: none
 */
void
wlan_mlme_get_eml_params(struct wlan_objmgr_psoc *psoc,
			 struct wlan_mlo_eml_cap *cap);

/**
 * wlan_mlme_cfg_set_emlsr_pad_delay() - Configure EMLSR padding delay subfield
 * @psoc: psoc context
 * @val: EMLSR padding delay subfield value
 *
 * API to configure EMLSR padding delay subfield in psoc mlme obj with user
 * requested value if it greater than the value configured by FW during boot-up.
 *
 * Return: none
 */
void
wlan_mlme_cfg_set_emlsr_pad_delay(struct wlan_objmgr_psoc *psoc, uint8_t val);

/**
 * wlan_mlme_get_t2lm_negotiation_supported() - Get the T2LM
 * negotiation supported value
 * @psoc: psoc context
 *
 * Return: t2lm negotiation supported value
 */
enum t2lm_negotiation_support
wlan_mlme_get_t2lm_negotiation_supported(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_set_t2lm_negotiation_supported() - Set the T2LM
 * negotiation supported value
 * @psoc: psoc context
 * @value: t2lm negotiation supported value
 *
 * Return: qdf status
 */
QDF_STATUS
wlan_mlme_set_t2lm_negotiation_supported(struct wlan_objmgr_psoc *psoc,
					 uint8_t value);
#else
static inline QDF_STATUS
wlan_mlme_get_eht_mode(struct wlan_objmgr_psoc *psoc, enum wlan_eht_mode *value)
{
	*value = WLAN_EHT_MODE_DISABLED;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_set_eht_mode(struct wlan_objmgr_psoc *psoc, enum wlan_eht_mode value)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_get_emlsr_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	*value = false;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_set_emlsr_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
wlan_mlme_set_eml_params(struct wlan_objmgr_psoc *psoc,
			 struct wlan_psoc_host_mac_phy_caps_ext2 *cap)
{
}

static inline void
wlan_mlme_get_eml_params(struct wlan_objmgr_psoc *psoc,
			 struct wlan_mlo_eml_cap *cap)
{
}

static inline void
wlan_mlme_cfg_set_emlsr_pad_delay(struct wlan_objmgr_psoc *psoc, uint8_t val)
{
}

static inline enum t2lm_negotiation_support
wlan_mlme_get_t2lm_negotiation_supported(struct wlan_objmgr_psoc *psoc)
{
	return T2LM_NEGOTIATION_DISABLED;
}

static inline QDF_STATUS
wlan_mlme_set_t2lm_negotiation_supported(struct wlan_objmgr_psoc *psoc,
					 uint8_t value)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * wlan_mlme_get_sta_miracast_mcc_rest_time() - Get STA/MIRACAST MCC rest time
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * This API gives rest time to be used when STA and MIRACAST MCC conc happens
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_sta_miracast_mcc_rest_time(struct wlan_objmgr_psoc *psoc,
					 uint32_t *value);

/**
 * wlan_mlme_get_max_modulated_dtim_ms() - get the max modulated dtim in ms
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_max_modulated_dtim_ms(struct wlan_objmgr_psoc *psoc,
				    uint16_t *value);

/**
 * wlan_mlme_get_scan_probe_unicast_ra() - Get scan probe unicast RA cfg
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * This API gives scan probe request with unicast RA user config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_scan_probe_unicast_ra(struct wlan_objmgr_psoc *psoc,
				    bool *value);

/**
 * wlan_mlme_set_scan_probe_unicast_ra() - Set scan probe unicast RA cfg
 * @psoc: pointer to psoc object
 * @value: set value
 *
 * This API sets scan probe request with unicast RA user config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_scan_probe_unicast_ra(struct wlan_objmgr_psoc *psoc,
				    bool value);

/**
 * wlan_mlme_get_sap_mcc_chnl_avoid() - Check if SAP MCC needs to be avoided
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * This API fetches the user setting to determine if SAP MCC with other persona
 * to be avoided.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_sap_mcc_chnl_avoid(struct wlan_objmgr_psoc *psoc,
				 uint8_t *value);
/**
 * wlan_mlme_get_mcc_bcast_prob_resp() - Get broadcast probe rsp in MCC
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * To get INI value which helps to determe whether to enable/disable use of
 * broadcast probe response to increase the detectability of SAP in MCC mode.
 *
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_mcc_bcast_prob_resp(struct wlan_objmgr_psoc *psoc,
				  uint8_t *value);
/**
 * wlan_mlme_get_mcc_rts_cts_prot() - To get RTS-CTS protection in MCC.
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * To get INI value which helps to determine whether to enable/disable
 * use of long duration RTS-CTS protection when SAP goes off
 * channel in MCC mode.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_mcc_rts_cts_prot(struct wlan_objmgr_psoc *psoc,
			       uint8_t *value);
/**
 * wlan_mlme_get_mcc_feature() - To find out to enable/disable MCC feature
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * To get INI value which helps to determine whether to enable MCC feature
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_mcc_feature(struct wlan_objmgr_psoc *psoc,
			  uint8_t *value);

/**
 * wlan_mlme_get_rrm_enabled() - Get the RRM enabled ini
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_rrm_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_dtim_selection_diversity() - get dtim selection diversity
 * bitmap
 * @psoc: pointer to psoc object
 * @dtim_selection_div: value that is requested by the caller
 * This function gets the dtim selection diversity bitmap to be
 * sent to the firmware
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_dtim_selection_diversity(struct wlan_objmgr_psoc *psoc,
						  uint32_t *dtim_selection_div);

/**
 * wlan_mlme_get_bmps_min_listen_interval() - get beacon mode powersave
 * minimum listen interval value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_bmps_min_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_bmps_max_listen_interval() - get beacon mode powersave
 * maximum listen interval value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_bmps_max_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_auto_bmps_timer_value() - get bmps timer value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_auto_bmps_timer_value(struct wlan_objmgr_psoc *psoc,
					       uint32_t *value);

/**
 * wlan_mlme_is_bmps_enabled() - check if beacon mode powersave is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_is_bmps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_override_bmps_imps() - disable imps/bmps
 * @psoc: pointer to psoc object
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_override_bmps_imps(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_is_imps_enabled() - check if idle mode powersave is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_is_imps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_wps_uuid() - get the wps uuid string
 * @wps_params:   pointer to mlme wps parameters structure
 * @data:          data to which the parameter is to be copied
 *
 * Return None
 *
 */
void
wlan_mlme_get_wps_uuid(struct wlan_mlme_wps_params *wps_params, uint8_t *data);

/**
 * wlan_mlme_get_self_gen_frm_pwr() - get self gen frm pwr
 * @psoc: pointer to psoc object
 * @value:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_self_gen_frm_pwr(struct wlan_objmgr_psoc *psoc,
			       uint32_t *value);

/**
 * wlan_mlme_get_4way_hs_offload() - get 4-way hs offload to fw cfg
 * @psoc: pointer to psoc object
 * @value:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_4way_hs_offload(struct wlan_objmgr_psoc *psoc, uint32_t *value);

/**
 * wlan_mlme_get_bmiss_skip_full_scan_value() - To get value of
 * bmiss_skip_full_scan ini
 * @psoc: pointer to psoc object
 * @value:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bmiss_skip_full_scan_value(struct wlan_objmgr_psoc *psoc,
					 bool *value);

/**
 * mlme_get_peer_phymode() - get phymode of peer
 * @psoc: pointer to psoc object
 * @mac:  Pointer to the mac addr of the peer
 * @peer_phymode: phymode
 *
 * Return: QDF Status
 */
QDF_STATUS
mlme_get_peer_phymode(struct wlan_objmgr_psoc *psoc, uint8_t *mac,
		      enum wlan_phymode *peer_phymode);

/**
 * mlme_set_tgt_wpa3_roam_cap() - Set the target WPA3 roam support
 * to mlme
 * @psoc: pointer to PSOC object
 * @akm_bitmap: Bitmap of akm suites supported for roaming by the firmware
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_set_tgt_wpa3_roam_cap(struct wlan_objmgr_psoc *psoc,
				      uint32_t akm_bitmap);
/**
 * wlan_mlme_get_ignore_fw_reg_offload_ind() - Get the
 * ignore_fw_reg_offload_ind ini
 * @psoc: pointer to psoc object
 * @disabled: output pointer to hold user config
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc,
					bool *disabled);

/**
 * mlme_get_roam_trigger_str() - Get the string for enum
 * WMI_ROAM_TRIGGER_REASON_ID reason.
 * @roam_scan_trigger: roam scan trigger ID
 *
 *  Return: Meaningful string from enum WMI_ROAM_TRIGGER_REASON_ID
 */
char *mlme_get_roam_trigger_str(uint32_t roam_scan_trigger);

/**
 * mlme_get_roam_status_str() - Get the string for roam status
 * @roam_status: roam status coming from fw via
 * wmi_roam_scan_info tlv
 *
 *  Return: Meaningful string for roam status
 */
char *mlme_get_roam_status_str(uint32_t roam_status);

/**
 * mlme_get_converted_timestamp() - Return time of the day
 * from timestamp
 * @timestamp:    Timestamp value in milliseconds
 * @time:         Output buffer to fill time into
 *
 * Return: Time of the day in [HH:MM:SS.uS]
 */
void mlme_get_converted_timestamp(uint32_t timestamp, char *time);

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * wlan_mlme_set_sae_single_pmk_bss_cap - API to set WPA3 single pmk AP IE
 * @psoc: Pointer to psoc object
 * @vdev_id: vdev id
 * @val: value to be set
 *
 * Return : None
 */
void wlan_mlme_set_sae_single_pmk_bss_cap(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id, bool val);

/**
 * wlan_mlme_update_sae_single_pmk - API to update mlme_pmkid_info
 * @vdev: vdev object
 * @sae_single_pmk: pointer to sae_single_pmk_info struct
 *
 * Return : None
 */
void
wlan_mlme_update_sae_single_pmk(struct wlan_objmgr_vdev *vdev,
				struct mlme_pmk_info *sae_single_pmk);

/**
 * wlan_mlme_get_sae_single_pmk_info - API to get mlme_pmkid_info
 * @vdev: vdev object
 * @pmksa: pointer to PMKSA struct
 *
 * Return : None
 */
void
wlan_mlme_get_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
				  struct wlan_mlme_sae_single_pmk *pmksa);

/**
 * wlan_mlme_is_sae_single_pmk_enabled() - Get is SAE single pmk feature enabled
 * @psoc: Pointer to Global psoc
 *
 * Return: True if SAE single PMK is enabled
 */
bool wlan_mlme_is_sae_single_pmk_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_clear_sae_single_pmk_info - API to clear mlme_pmkid_info ap caps
 * @vdev: vdev object
 * @pmk : pmk info to clear
 *
 * Return : None
 */
void wlan_mlme_clear_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
					 struct mlme_pmk_info *pmk);
#else
static inline void
wlan_mlme_set_sae_single_pmk_bss_cap(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id, bool val)
{
}

static inline
bool wlan_mlme_is_sae_single_pmk_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline void
wlan_mlme_update_sae_single_pmk(struct wlan_objmgr_vdev *vdev,
				struct mlme_pmk_info *sae_single_pmk)
{
}

static inline void
wlan_mlme_get_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
				  struct wlan_mlme_sae_single_pmk *pmksa)
{
}

static inline
void wlan_mlme_clear_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
					 struct mlme_pmk_info *pmk)
{
}
#endif

/**
 * mlme_get_roam_fail_reason_str() - Get fail string from enum
 * WMI_ROAM_FAIL_REASON_ID
 * @result:   Roam fail reason
 *
 * Return: Meaningful string from enum
 */
char *mlme_get_roam_fail_reason_str(uint32_t result);

/**
 * mlme_get_sub_reason_str() - Get roam trigger sub reason from enum
 * WMI_ROAM_TRIGGER_SUB_REASON_ID
 * @sub_reason: Sub reason value
 *
 * Return: Meaningful string from enum WMI_ROAM_TRIGGER_SUB_REASON_ID
 */
char *mlme_get_sub_reason_str(uint32_t sub_reason);

/**
 * wlan_mlme_get_mgmt_max_retry() - Get the
 * max mgmt retry
 * @psoc: pointer to psoc object
 * @max_retry: output pointer to hold user config
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mgmt_max_retry(struct wlan_objmgr_psoc *psoc,
			     uint8_t *max_retry);

/**
 * wlan_mlme_get_mgmt_6ghz_rate_support() - Get status of HE rates for
 * 6GHz mgmt frames
 * @psoc: pointer to psoc object
 * @enable_he_mcs0_for_6ghz_mgmt: pointer to check for HE rates support
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mgmt_6ghz_rate_support(struct wlan_objmgr_psoc *psoc,
				     bool *enable_he_mcs0_for_6ghz_mgmt);

/**
 * wlan_mlme_get_status_ring_buffer() - Get the
 * status of ring buffer
 * @psoc: pointer to psoc object
 * @enable_ring_buffer: output pointer to point the configured value of
 * ring buffer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_status_ring_buffer(struct wlan_objmgr_psoc *psoc,
				 bool *enable_ring_buffer);

/**
 * wlan_mlme_get_peer_unmap_conf() - Indicate if peer unmap confirmation
 * support is enabled or disabled
 * @psoc: pointer to psoc object
 *
 * Return: true if peer unmap confirmation support is enabled, else false
 */
bool wlan_mlme_get_peer_unmap_conf(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_mlme_get_roam_reason_vsie_status() - Indicate if roam reason
 * vsie is enabled or disabled
 * @psoc: pointer to psoc object
 * @roam_reason_vsie_enabled: pointer to hold value of roam reason
 * vsie
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t *roam_reason_vsie_enabled);

/**
 * wlan_mlme_set_roam_reason_vsie_status() - Update roam reason vsie status
 * @psoc: pointer to psoc object
 * @roam_reason_vsie_enabled: value of roam reason vsie
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t roam_reason_vsie_enabled);

/**
 * wlan_mlme_get_roaming_triggers  - Get the roaming triggers bitmap
 * @psoc: Pointer to PSOC object
 *
 * Return: Roaming triggers value
 */
uint32_t wlan_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_roaming_offload() - Get roaming offload setting
 * @psoc: pointer to psoc object
 * @val:  Pointer to enable/disable roaming offload
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val);

/**
 * wlan_mlme_get_enable_disconnect_roam_offload() - Get emergency roaming
 * Enable/Disable status during deauth/disassoc
 * @psoc: pointer to psoc object
 * @val:  Pointer to emergency roaming Enable/Disable status
 *        during deauth/disassoc
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_enable_disconnect_roam_offload(struct wlan_objmgr_psoc *psoc,
					     bool *val);

/**
 * wlan_mlme_get_enable_idle_roam() - Get Enable/Disable idle roaming status
 * @psoc: pointer to psoc object
 * @val:  Pointer to Enable/Disable idle roaming status
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_enable_idle_roam(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_idle_roam_rssi_delta() - Get idle roam rssi delta
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam rssi delta
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_rssi_delta(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val);

/**
 * wlan_mlme_get_idle_roam_inactive_time() - Get idle roam inactive time
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam inactive time
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_inactive_time(struct wlan_objmgr_psoc *psoc,
				      uint32_t *val);
/**
 * wlan_mlme_get_idle_data_packet_count() - Get idle data packet count
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle data packet count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_data_packet_count(struct wlan_objmgr_psoc *psoc,
				     uint32_t *val);

/**
 * wlan_mlme_get_idle_roam_min_rssi() - Get idle roam min rssi
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam min rssi
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_min_rssi(struct wlan_objmgr_psoc *psoc, uint32_t *val);

/**
 * wlan_mlme_get_idle_roam_band() - Get idle roam band
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam band
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_band(struct wlan_objmgr_psoc *psoc, uint32_t *val);

/**
 * wlan_mlme_get_self_bss_roam() - Get self bss roam enable status
 * @psoc: pointer to psoc object
 * @enable_self_bss_roam:  Pointer to self bss roam enable status
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_self_bss_roam(struct wlan_objmgr_psoc *psoc,
			    uint8_t *enable_self_bss_roam);
#else
static inline QDF_STATUS
wlan_mlme_get_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t *roam_reason_vsie_enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
wlan_mlme_set_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t roam_reason_vsie_enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
uint32_t wlan_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc)
{
	return 0xFFFF;
}

static inline QDF_STATUS
wlan_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_mlme_set_ft_over_ds() - Update ft_over_ds
 * @psoc: pointer to psoc object
 * @ft_over_ds_enable: value of ft_over_ds
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_set_ft_over_ds(struct wlan_objmgr_psoc *psoc,
				    uint8_t ft_over_ds_enable);
/**
 * wlan_mlme_get_dfs_chan_ageout_time() - Get the DFS Channel ageout time
 * @psoc: pointer to psoc object
 * @dfs_chan_ageout_time: output pointer to hold configured value of DFS
 * Channel ageout time
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_dfs_chan_ageout_time(struct wlan_objmgr_psoc *psoc,
				   uint8_t *dfs_chan_ageout_time);

#ifdef WLAN_FEATURE_SAE
/**
 * wlan_mlme_get_sae_assoc_retry_count() - Get the sae assoc retry count
 * @psoc: pointer to psoc object
 * @retry_count: assoc retry count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sae_assoc_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count);
/**
 * wlan_mlme_get_sae_auth_retry_count() - Get the sae auth retry count
 * @psoc: pointer to psoc object
 * @retry_count: auth retry count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sae_auth_retry_count(struct wlan_objmgr_psoc *psoc,
				   uint8_t *retry_count);

/**
 * wlan_mlme_get_sae_roam_auth_retry_count() - Get the sae roam auth retry count
 * @psoc: pointer to psoc object
 * @retry_count: auth retry count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sae_roam_auth_retry_count(struct wlan_objmgr_psoc *psoc,
					uint8_t *retry_count);

#else
static inline QDF_STATUS
wlan_mlme_get_sae_assoc_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count)
{
	*retry_count = 0;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_get_sae_auth_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count)
{
	*retry_count = 0;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_get_sae_roam_auth_retry_count(struct wlan_objmgr_psoc *psoc,
					uint8_t *retry_count)
{
	*retry_count = 0;
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_mlme_get_dual_sta_roaming_enabled  - API to get if the dual sta
 * roaming support is enabled.
 * @psoc: Pointer to global psoc object
 *
 * Return: True if dual sta roaming feature is enabled else return false
 */
bool
wlan_mlme_get_dual_sta_roaming_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
wlan_mlme_get_dual_sta_roaming_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * mlme_store_fw_scan_channels - Update the valid channel list to mlme.
 * @psoc: Pointer to global psoc object
 * @chan_list: Source channel list pointer
 *
 * Currently the channel list is saved to wma_handle to be updated in the
 * PCL command. This cannot be accessed at target_if while sending vdev
 * set pcl command. So save the channel list to mlme.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_store_fw_scan_channels(struct wlan_objmgr_psoc *psoc,
			    tSirUpdateChanList *chan_list);

/**
 * mlme_get_fw_scan_channels  - Copy the saved valid channel
 * list to the provided buffer
 * @psoc: Pointer to global psoc object
 * @freq_list: Pointer to the frequency list buffer to be filled
 * @saved_num_chan: Number of channels filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_get_fw_scan_channels(struct wlan_objmgr_psoc *psoc,
				     uint32_t *freq_list,
				     uint8_t *saved_num_chan);
/**
 * wlan_mlme_get_roam_scan_offload_enabled() - Roam scan offload enable or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roam_scan_offload_enabled(struct wlan_objmgr_psoc *psoc,
					bool *val);

/**
 * wlan_mlme_get_roam_bmiss_final_bcnt() - Get roam bmiss final count
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roam_bmiss_final_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val);

/**
 * wlan_mlme_get_roam_bmiss_first_bcnt() - Get roam bmiss first count
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roam_bmiss_first_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val);

/**
 * wlan_mlme_get_bmiss_timeout_on_wakeup() - Get bmiss timeout
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bmiss_timeout_on_wakeup(struct wlan_objmgr_psoc *psoc,
				      uint8_t *val);

/**
 * wlan_mlme_get_bmiss_timeout_on_sleep() - Get roam conbmiss timeout
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bmiss_timeout_on_sleep(struct wlan_objmgr_psoc *psoc,
				     uint8_t *val);

/**
 * wlan_mlme_adaptive_11r_enabled() - check if adaptive 11r feature is enaled
 * or not
 * @psoc: pointer to psoc object
 *
 * Return: bool
 */
#ifdef WLAN_ADAPTIVE_11R
bool wlan_mlme_adaptive_11r_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool wlan_mlme_adaptive_11r_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * wlan_mlme_get_mawc_enabled() - Get mawc enabled status
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_mawc_roam_enabled() - Get mawc roam enabled status
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_mawc_roam_traffic_threshold() - Get mawc traffic threshold
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_traffic_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val);

/**
 * wlan_mlme_get_mawc_roam_ap_rssi_threshold() - Get AP RSSI threshold for
 * MAWC roaming
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_ap_rssi_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val);

/**
 * wlan_mlme_get_mawc_roam_rssi_high_adjust() - Get high adjustment value
 * for suppressing scan
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_high_adjust(struct wlan_objmgr_psoc *psoc,
					 uint8_t *val);

/**
 * wlan_mlme_get_mawc_roam_rssi_low_adjust() - Get low adjustment value
 * for suppressing scan
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_low_adjust(struct wlan_objmgr_psoc *psoc,
					uint8_t *val);

/**
 * wlan_mlme_get_bss_load_enabled() - Get bss load based roam trigger
 * enabled status
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_bss_load_threshold() - Get bss load threshold
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_threshold(struct wlan_objmgr_psoc *psoc, uint32_t *val);

/**
 * wlan_mlme_get_bss_load_sample_time() - Get bss load sample time
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_sample_time(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val);

/**
 * wlan_mlme_get_bss_load_rssi_threshold_6ghz() - Get bss load RSSI
 * threshold on 6G
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_6ghz(struct wlan_objmgr_psoc *psoc,
					   int32_t *val);

/**
 * wlan_mlme_get_bss_load_rssi_threshold_5ghz() - Get bss load RSSI
 * threshold on 5G
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_5ghz(struct wlan_objmgr_psoc *psoc,
					   int32_t *val);

/**
 * wlan_mlme_get_bss_load_rssi_threshold_24ghz() - Get bss load RSSI
 * threshold on 2.4G
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_24ghz(struct wlan_objmgr_psoc *psoc,
					    int32_t *val);
/**
 * wlan_mlme_check_chan_param_has_dfs() - Get dfs flag based on
 * channel & channel parameters
 * @pdev: pdev object
 * @ch_params: channel parameters
 * @chan_freq: channel frequency in MHz
 *
 * Return: True for dfs
 */
bool
wlan_mlme_check_chan_param_has_dfs(struct wlan_objmgr_pdev *pdev,
				   struct ch_params *ch_params,
				   uint32_t chan_freq);

/**
 * wlan_mlme_set_usr_disabled_roaming() - Set user config for roaming disable
 * @psoc: pointer to psoc object
 * @val: user config for roaming disable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool val);

/**
 * wlan_mlme_get_usr_disabled_roaming() - Get user config for roaming disable
 * @psoc: pointer to psoc object
 * @val: user config for roaming disable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * mlme_get_opr_rate() - get operational rate
 * @vdev: vdev pointer
 * @dst: buffer to get rates set
 * @len: length of the buffer
 *
 * Return: length of the rates set
 */
qdf_size_t mlme_get_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
			     qdf_size_t len);

/**
 * mlme_set_opr_rate() - set operational rate
 * @vdev: vdev pointer
 * @src: pointer to set operational rate
 * @len: length of operational rate
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_set_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len);

/**
 * mlme_get_ext_opr_rate() - get extended operational rate
 * @vdev: vdev pointer
 * @dst: buffer to get rates set
 * @len: length of the buffer
 *
 * Return: length of the rates set
 */
qdf_size_t mlme_get_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
				 qdf_size_t len);

/**
 * mlme_set_ext_opr_rate() - set extended operational rate
 * @vdev: vdev pointer
 * @src: pointer to set extended operational rate
 * @len: length of extended operational rate
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_set_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
				 qdf_size_t len);

/**
 * mlme_clear_ext_opr_rate() - clear extended operational rate
 * @vdev: vdev pointer
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_clear_ext_opr_rate(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_mcs_rate() - get MCS based rate
 * @vdev: vdev pointer
 * @dst: buffer to get rates set
 * @len: length of the buffer
 *
 * Return: length of the rates set
 */
qdf_size_t mlme_get_mcs_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
			     qdf_size_t len);

/**
 * mlme_set_mcs_rate() - set MCS based rate
 * @vdev: vdev pointer
 * @src: pointer to set MCS based rate
 * @len: length of MCS based rate
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_set_mcs_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len);

/**
 * mlme_clear_mcs_rate() - clear MCS based rate
 * @vdev: vdev pointer
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_clear_mcs_rate(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlme_is_sta_mon_conc_supported() - Check if STA + Monitor mode
 * concurrency is supported
 * @psoc: pointer to psoc object
 *
 * Return: True if supported
 */
bool wlan_mlme_is_sta_mon_conc_supported(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_phy_max_freq_range() - Get phy supported max channel
 * frequency range
 * @psoc: psoc for country information
 * @low_2ghz_chan: 2.4 GHz low channel frequency
 * @high_2ghz_chan: 2.4 GHz high channel frequency
 * @low_5ghz_chan: 5 GHz low channel frequency
 * @high_5ghz_chan: 5 GHz high channel frequency
 *
 * Return: QDF status
 */
QDF_STATUS wlan_mlme_get_phy_max_freq_range(struct wlan_objmgr_psoc *psoc,
					    uint32_t *low_2ghz_chan,
					    uint32_t *high_2ghz_chan,
					    uint32_t *low_5ghz_chan,
					    uint32_t *high_5ghz_chan);

#ifdef FEATURE_WDS
/**
 * wlan_mlme_get_wds_mode() - Check wds mode supported
 * @psoc: pointer to psoc object
 *
 * Return: supported wds mode
 */
enum wlan_wds_mode
wlan_mlme_get_wds_mode(struct wlan_objmgr_psoc *psoc);
#else
static inline enum wlan_wds_mode
wlan_mlme_get_wds_mode(struct wlan_objmgr_psoc *psoc)
{
	return WLAN_WDS_MODE_DISABLED;
}
#endif

#ifdef WLAN_SUPPORT_TWT
/**
 * mlme_is_twt_enabled() - Get if TWT is enabled via ini.
 * @psoc: pointer to psoc object
 *
 * Return: True if TWT is enabled else false.
 */
bool
mlme_is_twt_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
mlme_is_twt_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif /* WLAN_SUPPORT_TWT */

/**
 * wlan_mlme_is_local_tpe_pref() - Get preference to use local TPE or
 * regulatory TPE values
 * @psoc: pointer to psoc object
 *
 * Return: True if there is local preference, false if there is regulatory
 * preference
 */
bool wlan_mlme_is_local_tpe_pref(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_skip_tpe() - Get preference to not consider TPE in 2G/5G case
 *
 * @psoc: pointer to psoc object
 *
 * Return: True if host should not consider TPE IE in TX power calculation when
 * operating in 2G/5G bands, false if host should always consider TPE IE values
 */
bool wlan_mlme_skip_tpe(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_is_data_stall_recovery_fw_supported() - Check if data stall
 * recovery is supported by fw
 * @psoc: pointer to psoc object
 *
 * Return: True if supported
 */
bool
wlan_mlme_is_data_stall_recovery_fw_supported(struct wlan_objmgr_psoc *psoc);

/**
 * mlme_cfg_get_eht_caps() - Get the EHT capability info
 * @psoc: pointer to psoc object
 * @eht_cap: Caps that needs to be filled.
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_cfg_get_eht_caps(struct wlan_objmgr_psoc *psoc,
				 tDot11fIEeht_cap *eht_cap);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_mlme_get_sta_mlo_conn_max_num() - get max number of links that sta mlo
 *                                        connection can support
 * @psoc: pointer to psoc object
 *
 * Return: max number of links that sta mlo connection can support
 */
uint8_t wlan_mlme_get_sta_mlo_conn_max_num(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_set_sta_mlo_conn_max_num() - set max number of links that sta mlo
 *                                        connection can support
 * @psoc: pointer to psoc object
 * @value: value to set
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sta_mlo_conn_max_num(struct wlan_objmgr_psoc *psoc,
					      uint8_t value);

/**
 * wlan_mlme_get_sta_mlo_conn_band_bmp() - get band bitmap that sta mlo
 *                                         connection can support
 * @psoc: pointer to psoc object
 *
 * Return: band bitmap that sta mlo connection can support
 */
uint8_t wlan_mlme_get_sta_mlo_conn_band_bmp(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_set_sta_mlo_simultaneous_links() - set mlo simultaneous links
 * @psoc: pointer to psoc object
 * @value: value to set
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_set_sta_mlo_simultaneous_links(struct wlan_objmgr_psoc *psoc,
					 uint8_t value);

/**
 * wlan_mlme_get_sta_mlo_simultaneous_links() - get mlo simultaneous links
 * @psoc: pointer to psoc object
 *
 * Return: number of links
 */
uint8_t wlan_mlme_get_sta_mlo_simultaneous_links(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_set_sta_mlo_conn_band_bmp() - set band bitmap that sta mlo
 *                                         connection can support
 * @psoc: pointer to psoc object
 * @value: value to set
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sta_mlo_conn_band_bmp(struct wlan_objmgr_psoc *psoc,
					       uint8_t value);
#else
static inline QDF_STATUS
wlan_mlme_set_sta_mlo_conn_max_num(struct wlan_objmgr_psoc *psoc,
				   uint8_t value)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_set_sta_mlo_simultaneous_links(struct wlan_objmgr_psoc *psoc,
					 uint8_t value)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_set_sta_mlo_conn_band_bmp(struct wlan_objmgr_psoc *psoc,
				    uint8_t value)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_mlme_set_ba_2k_jump_iot_ap() - Set a flag if ba 2k jump IOT AP is found
 * @vdev: vdev pointer
 * @found: Carries the value true if ba 2k jump IOT AP is found
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_set_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev, bool found);

/**
 * wlan_mlme_is_ba_2k_jump_iot_ap() - Check if ba 2k jump IOT AP is found
 * @vdev: vdev pointer
 *
 * Return: true if ba 2k jump IOT AP is found
 */
bool
wlan_mlme_is_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlme_set_last_delba_sent_time() - Cache the last delba sent ts
 * @vdev: vdev pointer
 * @delba_sent_time: Last delba sent timestamp
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_set_last_delba_sent_time(struct wlan_objmgr_vdev *vdev,
				   qdf_time_t delba_sent_time);

/**
 * wlan_mlme_get_last_delba_sent_time() - Get the last delba sent ts
 * @vdev: vdev pointer
 *
 * Return: Last delba timestamp if cached, 0 otherwise
 */
qdf_time_t
wlan_mlme_get_last_delba_sent_time(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_user_ps() - Set the PS user config
 * @psoc: Pointer to psoc object
 * @vdev_id: vdev id
 * @ps_enable: User PS enable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			    bool ps_enable);

/**
 * mlme_get_user_ps() - Set the user ps flag
 * @psoc: Pointer to psoc object
 * @vdev_id: vdev id
 *
 * Return: True if user_ps flag is set
 */
bool mlme_get_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id);

#ifdef WLAN_FEATURE_P2P_P2P_STA
/**
 * wlan_mlme_get_p2p_p2p_conc_support() - Get p2p+p2p conc support
 * @psoc: pointer to psoc object
 *
 * Return: Success/failure
 */
bool
wlan_mlme_get_p2p_p2p_conc_support(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
wlan_mlme_get_p2p_p2p_conc_support(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * mlme_get_vht_ch_width() - get vht channel width of fw capability
 *
 * Return: vht channel width
 */
enum phy_ch_width mlme_get_vht_ch_width(void);

/**
 * wlan_mlme_get_mgmt_hw_tx_retry_count() - Get mgmt frame hw tx retry count
 * @psoc: pointer to psoc object
 * @frm_type: frame type of the query
 *
 * Return: hw tx retry count
 */
uint8_t
wlan_mlme_get_mgmt_hw_tx_retry_count(struct wlan_objmgr_psoc *psoc,
				     enum mlme_cfg_frame_type frm_type);

/**
 * wlan_mlme_get_tx_retry_multiplier() - Get the tx retry multiplier percentage
 * @psoc: pointer to psoc object
 * @tx_retry_multiplier: pointer to hold user config value of
 * tx_retry_multiplier
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_tx_retry_multiplier(struct wlan_objmgr_psoc *psoc,
				  uint32_t *tx_retry_multiplier);

/**
 * wlan_mlme_get_channel_bonding_5ghz  - Get the channel bonding
 * val for 5ghz freq
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_channel_bonding_5ghz(struct wlan_objmgr_psoc *psoc,
				   uint32_t *value);

/**
 * wlan_mlme_update_ratemask_params() - Update ratemask params
 *
 * @vdev: pointer to vdev object
 * @num_ratemask: number of rate masks
 * @rate_params: pointer to ratemask structure
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_update_ratemask_params(struct wlan_objmgr_vdev *vdev,
				 uint8_t num_ratemask,
				 struct config_ratemask_params *rate_params);

/**
 * wlan_mlme_is_channel_valid() - validate channel frequency
 * @psoc: psoc object manager
 * @chan_freq: channel frequency
 *
 * This function validates channel frequency present in valid channel
 * list or not.
 *
 * Return: true or false
 */
bool wlan_mlme_is_channel_valid(struct wlan_objmgr_psoc *psoc,
				uint32_t chan_freq);
#ifdef WLAN_FEATURE_MCC_QUOTA
/**
 * wlan_mlme_set_user_mcc_quota() - set the user mcc quota in mlme
 * @psoc: pointer to psoc object
 * @quota: pointer to user set mcc quota object
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_user_mcc_quota(struct wlan_objmgr_psoc *psoc,
					struct wlan_user_mcc_quota *quota);

/**
 * wlan_mlme_get_user_mcc_quota() - Get the user mcc quota from mlme
 * @psoc: pointer to psoc object
 * @quota: pointer to user set mcc quota object
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_user_mcc_quota(struct wlan_objmgr_psoc *psoc,
					struct wlan_user_mcc_quota *quota);

/**
 * wlan_mlme_get_user_mcc_duty_cycle_percentage() - Get user mcc duty cycle
 * @psoc: pointer to psoc object
 *
 * Return: MCC duty cycle if MCC exists for the user MCC quota, else 0
 */
uint32_t
wlan_mlme_get_user_mcc_duty_cycle_percentage(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS
wlan_mlme_set_user_mcc_quota(struct wlan_objmgr_psoc *psoc,
			     struct wlan_user_mcc_quota *quota)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_get_user_mcc_quota(struct wlan_objmgr_psoc *psoc,
			     struct wlan_user_mcc_quota *quota)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline uint32_t
wlan_mlme_get_user_mcc_duty_cycle_percentage(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}
#endif /* WLAN_FEATURE_MCC_QUOTA */

/**
 * mlme_get_max_he_mcs_idx() -  get max mcs index from he cap information
 * @mcs_ch_width: channel width
 * @hecap_rxmcsnssmap: rx mcs map from he cap
 * @hecap_txmcsnssmap: tx mcs map from he cap
 *
 * Return: the maximum MCS supported
 */
uint8_t mlme_get_max_he_mcs_idx(enum phy_ch_width mcs_ch_width,
				u_int16_t *hecap_rxmcsnssmap,
				u_int16_t *hecap_txmcsnssmap);

/**
 * mlme_get_max_vht_mcs_idx() -  get max mcs index from vht cap information
 * @rx_vht_mcs_map: rx mcs map from vht cap
 * @tx_vht_mcs_map: tx mcs map from vht cap
 *
 * Return: the maximum MCS supported
 */
uint8_t mlme_get_max_vht_mcs_idx(u_int16_t rx_vht_mcs_map,
				 u_int16_t tx_vht_mcs_map);

#ifdef WLAN_FEATURE_SON
/**
 * mlme_save_vdev_max_mcs_idx() - Save max mcs index of vdev
 * @vdev: pointer to vdev object
 * @max_mcs_idx: max_mcs_idx to save
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_save_vdev_max_mcs_idx(struct wlan_objmgr_vdev *vdev,
				      uint8_t max_mcs_idx);

/**
 * mlme_get_vdev_max_mcs_idx() - Get max mcs index of vdev
 * @vdev: pointer to vdev object
 *
 * Return max mcs index of vdev
 */
uint8_t mlme_get_vdev_max_mcs_idx(struct wlan_objmgr_vdev *vdev);
#endif /* WLAN_FEATURE_SON */
/**
 * wlan_mlme_set_safe_mode_enable() - set safe_mode_enable flag
 * based on value set by user space.
 *
 * @psoc: psoc context
 * @safe_mode_enable: safe mode enabled or not
 *
 * Return: none
 */
void wlan_mlme_set_safe_mode_enable(struct wlan_objmgr_psoc *psoc,
				    bool safe_mode_enable);

/**
 * wlan_mlme_get_safe_mode_enable() - get safe_mode_enable set by user
 * space
 *
 * @psoc: psoc context
 * @safe_mode_enable: safe mode enabled or not
 *
 * Return: none
 */
void wlan_mlme_get_safe_mode_enable(struct wlan_objmgr_psoc *psoc,
				    bool *safe_mode_enable);

/**
 * wlan_mlme_get_6g_ap_power_type() - get the power type of the
 * vdev operating on 6GHz.
 *
 * @vdev: vdev context
 *
 * Return: 6g_power_type
 */
uint32_t wlan_mlme_get_6g_ap_power_type(struct wlan_objmgr_vdev *vdev);

QDF_STATUS wlan_connect_hw_mode_change_resp(struct wlan_objmgr_pdev *pdev,
					    uint8_t vdev_id,
					    wlan_cm_id cm_id,
					    QDF_STATUS status);

/**
 * wlan_mlme_get_ch_width_from_phymode() - Convert phymode to ch_width
 * @phy_mode: Phy mode
 *
 * Return: enum phy_ch_width
 */
enum phy_ch_width
wlan_mlme_get_ch_width_from_phymode(enum wlan_phymode phy_mode);

/**
 * wlan_mlme_get_peer_ch_width() - get ch_width of the given peer
 * @psoc: psoc context
 * @mac: peer mac
 *
 * Return: enum phy_ch_width
 */
enum phy_ch_width
wlan_mlme_get_peer_ch_width(struct wlan_objmgr_psoc *psoc, uint8_t *mac);

#if defined(WLAN_FEATURE_SR)
/**
 * wlan_mlme_get_sr_enable_modes() - get mode for which SR is enabled
 *
 * @psoc: psoc context
 * @val: pointer to hold the value of SR(Spatial Reuse) enable modes
 *
 * Return: void
 */
void
wlan_mlme_get_sr_enable_modes(struct wlan_objmgr_psoc *psoc, uint8_t *val);
#endif

/**
 * wlan_mlme_set_edca_pifs_param() - set edca/pifs param for ll sap
 * @ep: pointer to wlan_edca_pifs_param_ie
 * @type: edca_param_type
 *
 * Return: None
 */
void
wlan_mlme_set_edca_pifs_param(struct wlan_edca_pifs_param_ie *ep,
			      enum host_edca_param_type type);
/**
 * wlan_mlme_stats_get_periodic_display_time() - get display time
 * @psoc: pointer to psoc object
 * @periodic_display_time: buffer to hold value
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_stats_get_periodic_display_time(struct wlan_objmgr_psoc *psoc,
					  uint32_t *periodic_display_time);

/**
 * wlan_mlme_is_bcn_prot_disabled_for_sap() - Is beacon protection config
 * disabled for SAP interface
 *
 * @psoc: pointer to psoc object
 *
 * Return: is beacon protection disabled
 */
bool
wlan_mlme_is_bcn_prot_disabled_for_sap(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_src_addr_from_frame() - Get source address of the frame
 * @frame: frame ptr
 *
 * Extract source mac address of the frame
 *
 * Return: Ptr for extracted src mac address
 *
 */
uint8_t *
wlan_mlme_get_src_addr_from_frame(struct element_info *frame);
#endif /* _WLAN_MLME_API_H_ */
