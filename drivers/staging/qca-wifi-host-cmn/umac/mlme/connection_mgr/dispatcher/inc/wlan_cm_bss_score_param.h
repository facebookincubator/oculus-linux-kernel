/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: Define bss scoring structures and APIs
 */

#ifndef _WLAN_CM_BSS_SCORE_H
#define _WLAN_CM_BSS_SCORE_H

#include <wlan_scan_utils_api.h>
#include "wlan_reg_services_api.h"

/**
 * struct weight_cfg - weight params to calculate best candidate
 * @rssi_weightage: RSSI weightage
 * @ht_caps_weightage: HT caps weightage
 * @vht_caps_weightage: VHT caps weightage
 * @he_caps_weightage: HE caps weightage
 * @chan_width_weightage: Channel width weightage
 * @chan_band_weightage: Channel band weightage
 * @nss_weightage: NSS weightage
 * @beamforming_cap_weightage: Beamforming caps weightage
 * @pcl_weightage: PCL weightage
 * @channel_congestion_weightage: channel congestion weightage
 * @oce_wan_weightage: OCE WAN metrics weightage
 * @oce_ap_tx_pwr_weightage: OCE AP tx power weigtage
 * @oce_subnet_id_weightage: OCE subnet id weigtage
 * @sae_pk_ap_weightage: SAE-PK AP weigtage
 * @eht_caps_weightage: EHT caps weightage
 * @mlo_weightage: MLO weightage
 * @joint_rssi_alpha: Joint RSSI alpha value
 * @joint_esp_alpha: Joint ESP alpha value
 * @joint_oce_alpha: Joint OCE alpha value
 * @low_band_rssi_boost: Flag to assign higher alpha weightage low band RSSI
 * @low_band_esp_boost: Flag to assign higher alpha weightage low band esp
 * @low_band_oce_boost: Flag to assign higher alpha weightage low band oce
 * @reserved: reserved/unused bits
 * @wlm_indication_weightage: WLM indication weightage
 * @emlsr_weightage: eMLSR weightage
 * @security_weightage: Security weightage
 */
struct weight_cfg {
	uint8_t rssi_weightage;
	uint8_t ht_caps_weightage;
	uint8_t vht_caps_weightage;
	uint8_t he_caps_weightage;
	uint8_t chan_width_weightage;
	uint8_t chan_band_weightage;
	uint8_t nss_weightage;
	uint8_t beamforming_cap_weightage;
	uint8_t pcl_weightage;
	uint8_t channel_congestion_weightage;
	uint8_t oce_wan_weightage;
	uint8_t oce_ap_tx_pwr_weightage;
	uint8_t oce_subnet_id_weightage;
	uint8_t sae_pk_ap_weightage;
#ifdef WLAN_FEATURE_11BE_MLO
	uint8_t eht_caps_weightage;
	uint8_t mlo_weightage;
	uint8_t joint_rssi_alpha;
	uint8_t joint_esp_alpha;
	uint8_t joint_oce_alpha;
	uint8_t low_band_rssi_boost:1,
		low_band_esp_boost:1,
		low_band_oce_boost:1,
		reserved:5;
	uint8_t wlm_indication_weightage;
	uint8_t emlsr_weightage;
#endif
	uint8_t security_weightage;
};

/**
 * struct rssi_config_score - rssi related params for scoring logic
 * @best_rssi_threshold: RSSI weightage
 * @good_rssi_threshold: HT caps weightage
 * @bad_rssi_threshold: VHT caps weightage
 * @good_rssi_pcnt: HE caps weightage
 * @bad_rssi_pcnt: Channel width weightage
 * @good_rssi_bucket_size: Channel band weightage
 * @bad_rssi_bucket_size: NSS weightage
 * @rssi_pref_5g_rssi_thresh: Beamforming caps weightage
 * @con_non_hint_target_rssi_threshold: RSSI threshold value
 */
struct rssi_config_score  {
	uint8_t best_rssi_threshold;
	uint8_t good_rssi_threshold;
	uint8_t bad_rssi_threshold;
	uint8_t good_rssi_pcnt;
	uint8_t bad_rssi_pcnt;
	uint8_t good_rssi_bucket_size;
	uint8_t bad_rssi_bucket_size;
	uint8_t rssi_pref_5g_rssi_thresh;
	int8_t con_non_hint_target_rssi_threshold;
};

/**
 * struct per_slot_score - define % score for different slots for a
 *                               scoring param.
 * @num_slot: number of slots in which the param will be divided.
 *           Max 15. index 0 is used for 'not_present. Num_slot will
 *           equally divide 100. e.g, if num_slot = 4 slot 0 = 0-25%, slot
 *           1 = 26-50% slot 2 = 51-75%, slot 3 = 76-100%
 * @score_pcnt3_to_0: Contains score percentage for slot 0-3
 *             BITS 0-7   :- the scoring pcnt when not present
 *             BITS 8-15  :- SLOT_1
 *             BITS 16-23 :- SLOT_2
 *             BITS 24-31 :- SLOT_3
 * @score_pcnt7_to_4: Contains score percentage for slot 4-7
 *             BITS 0-7   :- SLOT_4
 *             BITS 8-15  :- SLOT_5
 *             BITS 16-23 :- SLOT_6
 *             BITS 24-31 :- SLOT_7
 * @score_pcnt11_to_8: Contains score percentage for slot 8-11
 *             BITS 0-7   :- SLOT_8
 *             BITS 8-15  :- SLOT_9
 *             BITS 16-23 :- SLOT_10
 *             BITS 24-31 :- SLOT_11
 * @score_pcnt15_to_12: Contains score percentage for slot 12-15
 *             BITS 0-7   :- SLOT_12
 *             BITS 8-15  :- SLOT_13
 *             BITS 16-23 :- SLOT_14
 *             BITS 24-31 :- SLOT_15
 */
struct per_slot_score {
	uint8_t num_slot;
	uint32_t score_pcnt3_to_0;
	uint32_t score_pcnt7_to_4;
	uint32_t score_pcnt11_to_8;
	uint32_t score_pcnt15_to_12;
};

#ifndef WLAN_FEATURE_11BE
#define CM_20MHZ_BW_INDEX                  0
#define CM_40MHZ_BW_INDEX                  1
#define CM_80MHZ_BW_INDEX                  2
#define CM_160MHZ_BW_INDEX                 3
#define CM_MAX_BW_INDEX                    4

#define CM_NSS_1x1_INDEX                   0
#define CM_NSS_2x2_INDEX                   1
#define CM_NSS_3x3_INDEX                   2
#define CM_NSS_4x4_INDEX                   3
#define CM_MAX_NSS_INDEX                   4
#else
enum cm_bw_idx {
	CM_20MHZ_BW_INDEX = 0,
	CM_40MHZ_BW_INDEX = 1,
	CM_80MHZ_BW_INDEX = 2,
	CM_160MHZ_BW_INDEX = 3,
	CM_320MHZ_BW_INDEX = 4,
	CM_80MHZ_BW_20MHZ_PUNCTURE_INDEX = 5,
	CM_160MHZ_BW_40MHZ_PUNCTURE_INDEX = 6,
	CM_160MHZ_BW_20MHZ_PUNCTURE_INDEX = 7,
	CM_320MHZ_BW_40MHZ_80MHZ_PUNCTURE_INDEX = 8,
	CM_320MHZ_BW_80MHZ_PUNCTURE_INDEX = 9,
	CM_320MHZ_BW_40MHZ_PUNCTURE_INDEX = 10,
#ifdef WLAN_FEATURE_11BE_MLO
	CM_MLO_20_PLUS_20MHZ_BW_INDEX = 11,
	CM_MLO_20_PLUS_40MHZ_BW_INDEX = 12,
	CM_MLO_40_PLUS_40MHZ_BW_INDEX = 13,
	CM_MLO_20_PLUS_80MHZ_BW_20MHZ_PUNCTURE_INDEX = 14,
	CM_MLO_20_PLUS_80MHZ_BW_INDEX = 15,
	CM_MLO_40_PLUS_80MHZ_BW_20MHZ_PUNCTURE_INDEX = 16,
	CM_MLO_40_PLUS_80MHZ_BW_INDEX = 17,
	CM_MLO_80_PLUS_80MHZ_BW_40MHZ_PUNCTURE_INDEX = 18,
	CM_MLO_80_PLUS_80MHZ_BW_20MHZ_PUNCTURE_INDEX = 19,
	CM_MLO_80_PLUS_80MHZ_BW_INDEX = 20,
	CM_MLO_20_PLUS_160HZ_BW_40MHZ_PUNCTURE_INDEX = 21,
	CM_MLO_20_PLUS_160HZ_BW_20MHZ_PUNCTURE_INDEX = 22,
	CM_MLO_20_PLUS_160HZ_BW_INDEX = 23,
	CM_MLO_40_PLUS_160HZ_BW_40MHZ_PUNCTURE_INDEX = 24,
	CM_MLO_40_PLUS_160HZ_BW_20MHZ_PUNCTURE_INDEX = 25,
	CM_MLO_40_PLUS_160HZ_BW_INDEX = 26,
	CM_MLO_80_PLUS_160HZ_BW_60MHZ_PUNCTURE_INDEX = 27,
	CM_MLO_80_PLUS_160HZ_BW_40MHZ_PUNCTURE_INDEX = 28,
	CM_MLO_80_PLUS_160HZ_BW_20MHZ_PUNCTURE_INDEX = 29,
	CM_MLO_80_PLUS_160HZ_BW_INDEX = 30,
	CM_MLO_160_PLUS_160HZ_BW_80MHZ_PUNCTURE_INDEX = 31,
	CM_MLO_160_PLUS_160HZ_BW_60MHZ_PUNCTURE_INDEX = 32,
	CM_MLO_160_PLUS_160HZ_BW_40MHZ_PUNCTURE_INDEX = 33,
	CM_MLO_160_PLUS_160HZ_BW_20MHZ_PUNCTURE_INDEX = 34,
	CM_MLO_160_PLUS_160HZ_BW_INDEX = 35,
#endif
	CM_MAX_BW_INDEX
};

enum cm_nss_idx {
	CM_NSS_1x1_INDEX,
	CM_NSS_2x2_INDEX,
	CM_NSS_3x3_INDEX,
	CM_NSS_4x4_INDEX,
#ifdef WLAN_FEATURE_11BE_MLO
	CM_NSS_2x2_PLUS_2x2_INDEX,
	CM_NSS_4x4_PLUS_4x4_INDEX,
#endif
	CM_MAX_NSS_INDEX
};
#endif

enum cm_security_idx {
	CM_SECURITY_WPA_INDEX,
	CM_SECURITY_WPA2_INDEX,
	CM_SECURITY_WPA3_INDEX,
	CM_SECURITY_WPA_OPEN_WEP_INDEX,
	CM_MAX_SECURITY_INDEX
};

/**
 * struct scoring_cfg - Scoring related configuration
 * @weight_config: weightage config for scoring config
 * @rssi_score: Rssi related config for scoring config
 * @esp_qbss_scoring: esp and qbss related scoring config
 * @oce_wan_scoring: oce related scoring config
 * @bandwidth_weight_per_index: BW weight per index
 * @nss_weight_per_index: nss weight per index
 * @band_weight_per_index: band weight per index
 * @is_bssid_hint_priority: True if bssid_hint is given priority
 * @check_assoc_disallowed: Should assoc be disallowed if MBO OCE IE indicate so
 * @vendor_roam_score_algorithm: Preferred ETP vendor roam score algorithm
 * @check_6ghz_security: check security for 6 GHz candidate
 * @standard_6ghz_conn_policy: check for 6 GHz standard connection policy
 * @disable_vlp_sta_conn_to_sp_ap: check for disable vlp sta conn to sp ap
 * @key_mgmt_mask_6ghz: user configurable mask for 6 GHz AKM
 * @mlsr_link_selection: MLSR link selection config
 * @roam_tgt_score_cap: Roam score capability
 * @security_weight_per_index: security weight per index
 */
struct scoring_cfg {
	struct weight_cfg weight_config;
	struct rssi_config_score rssi_score;
	struct per_slot_score esp_qbss_scoring;
	struct per_slot_score oce_wan_scoring;
	uint32_t bandwidth_weight_per_index[qdf_ceil(CM_MAX_BW_INDEX, 4)];
	uint32_t nss_weight_per_index[qdf_ceil(CM_MAX_NSS_INDEX, 4)];
	uint32_t band_weight_per_index;
	uint8_t is_bssid_hint_priority:1,
		 check_assoc_disallowed:1,
		 vendor_roam_score_algorithm:1,
		 check_6ghz_security:1,
		 standard_6ghz_conn_policy:1,
		 disable_vlp_sta_conn_to_sp_ap:1;

	uint32_t key_mgmt_mask_6ghz;
#ifdef WLAN_FEATURE_11BE_MLO
	uint8_t mlsr_link_selection;
#endif
	uint32_t roam_tgt_score_cap;
	uint32_t security_weight_per_index;
};

/**
 * struct pcl_freq_weight_list - pcl freq weight info
 * @num_of_pcl_channels: number of pcl channel
 * @pcl_freq_list: pcl freq list
 * @pcl_weight_list: pcl freq weight list
 */
struct pcl_freq_weight_list {
	uint32_t num_of_pcl_channels;
	uint32_t pcl_freq_list[NUM_CHANNELS];
	uint8_t pcl_weight_list[NUM_CHANNELS];
};

/**
 * enum cm_denylist_action - action taken by denylist manager for the bssid
 * @CM_DLM_NO_ACTION: No operation to be taken for the BSSID in the scan list.
 * @CM_DLM_REMOVE: Remove the BSSID from the scan list (AP is denylisted)
 * This param is a way to inform the caller that this BSSID is denylisted
 * but it is a driver denylist and we can connect to them if required.
 * @CM_DLM_FORCE_REMOVE: Forcefully remove the BSSID from scan list.
 * This param is introduced as we want to differentiate between optional
 * mandatory denylisting. Driver denylisting is optional and won't
 * fail any CERT or protocol violations as it is internal implementation.
 * hence FORCE_REMOVE will mean that driver cannot connect to this BSSID
 * in any situation.
 * @CM_DLM_AVOID: Add the Ap at last of the scan list (AP to Avoid)
 */
enum cm_denylist_action {
	CM_DLM_NO_ACTION,
	CM_DLM_REMOVE,
	CM_DLM_FORCE_REMOVE,
	CM_DLM_AVOID,
};

/**
 * struct etp_params - params for estimated throughput
 * @airtime_fraction: Portion of airtime available for outbound transmissions
 * @data_ppdu_dur_target_us: Expected duration of a single PPDU, in us
 * @ba_window_size: Block ack window size of the transmitter
 */
struct etp_params {
	uint32_t airtime_fraction;
	uint32_t data_ppdu_dur_target_us;
	uint32_t ba_window_size;
};

#ifdef FEATURE_DENYLIST_MGR
enum cm_denylist_action
wlan_denylist_action_on_bssid(struct wlan_objmgr_pdev *pdev,
			      struct scan_cache_entry *entry);
#else
static inline enum cm_denylist_action
wlan_denylist_action_on_bssid(struct wlan_objmgr_pdev *pdev,
			      struct scan_cache_entry *entry)
{
	return CM_DLM_NO_ACTION;
}
#endif

/**
 * wlan_cm_calculate_bss_score() - calculate bss score for the scan list
 * @pdev: pointer to pdev object
 * @pcl_lst: pcl list for scoring
 * @scan_list: scan list, contains the input list and after the
 *             func it will have sorted list
 * @bssid_hint: bssid hint
 * @self_mac: connecting vdev self mac address
 *
 * Return: void
 */
void wlan_cm_calculate_bss_score(struct wlan_objmgr_pdev *pdev,
				 struct pcl_freq_weight_list *pcl_lst,
				 qdf_list_t *scan_list,
				 struct qdf_mac_addr *bssid_hint,
				 struct qdf_mac_addr *self_mac);

/**
 * wlan_cm_init_score_config() - Init score INI and config
 * @psoc: pointer to psoc object
 * @score_cfg: score config
 *
 * Return: void
 */
void wlan_cm_init_score_config(struct wlan_objmgr_psoc *psoc,
			       struct scoring_cfg *score_cfg);

/**
 * wlan_cm_6ghz_allowed_for_akm() - check if 6 GHz channel can be allowed
 *                                  for AKM
 * @psoc: pointer to psoc object
 * @key_mgmt: key mgmt used
 * @rsn_caps: rsn caps
 * @rsnxe: rsnxe pointer if present
 * @sae_pwe: support for SAE password
 * @is_wps: if security is WPS
 *
 * Return: bool
 */
#ifdef CONFIG_BAND_6GHZ
bool wlan_cm_6ghz_allowed_for_akm(struct wlan_objmgr_psoc *psoc,
				  uint32_t key_mgmt, uint16_t rsn_caps,
				  const uint8_t *rsnxe, uint8_t sae_pwe,
				  bool is_wps);

/**
 * wlan_cm_set_check_6ghz_security() - Set check 6 GHz security
 * @psoc: pointer to psoc object
 * @value: value to be set
 *
 * Return: void
 */
void wlan_cm_set_check_6ghz_security(struct wlan_objmgr_psoc *psoc,
				     bool value);

/**
 * wlan_cm_reset_check_6ghz_security() - reset check 6 GHz security to original
 *                                       value
 * @psoc: pointer to psoc object
 *
 * Return: void
 */
void wlan_cm_reset_check_6ghz_security(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cm_get_check_6ghz_security() - Get 6 GHz allowed AKM mask
 * @psoc: pointer to psoc object
 *
 * Return: value
 */
bool wlan_cm_get_check_6ghz_security(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cm_set_6ghz_key_mgmt_mask() - Set 6 GHz allowed AKM mask
 * @psoc: pointer to psoc object
 * @value: value to be set
 *
 * Return: void
 */
void wlan_cm_set_6ghz_key_mgmt_mask(struct wlan_objmgr_psoc *psoc,
				    uint32_t value);

/**
 * wlan_cm_get_6ghz_key_mgmt_mask() - Get 6 GHz allowed AKM mask
 * @psoc: pointer to psoc object
 *
 * Return: value
 */
uint32_t wlan_cm_get_6ghz_key_mgmt_mask(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cm_get_disable_vlp_sta_conn_to_sp_ap() - Set disable vlp sta connection
 *                                               to sp ap
 * @psoc: pointer to psoc object
 *
 * Return: value
 */
bool wlan_cm_get_disable_vlp_sta_conn_to_sp_ap(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cm_set_disable_vlp_sta_conn_to_sp_ap() - Set disable vlp sta connection
 *                                               to sp ap
 * @psoc: pointer to psoc object
 * @value: value to be set
 *
 * Return: void
 */
void wlan_cm_set_disable_vlp_sta_conn_to_sp_ap(struct wlan_objmgr_psoc *psoc,
					       bool value);
/**
 * wlan_cm_set_standard_6ghz_conn_policy() - Set 6 GHz standard connection
 *					     policy
 * @psoc: pointer to psoc object
 * @value: value to be set
 *
 * Return: void
 */
void wlan_cm_set_standard_6ghz_conn_policy(struct wlan_objmgr_psoc *psoc,
					   bool value);

/**
 * wlan_cm_get_standard_6ghz_conn_policy() - Get 6Ghz standard connection
 *					     policy
 * @psoc: pointer to psoc object
 *
 * Return: value
 */
bool wlan_cm_get_standard_6ghz_conn_policy(struct wlan_objmgr_psoc *psoc);

#else
static inline bool
wlan_cm_6ghz_allowed_for_akm(struct wlan_objmgr_psoc *psoc,
			     uint32_t key_mgmt, uint16_t rsn_caps,
			     const uint8_t *rsnxe, uint8_t sae_pwe,
			     bool is_wps)
{
	return true;
}

static inline
void wlan_cm_set_check_6ghz_security(struct wlan_objmgr_psoc *psoc,
				     bool value) {}

static inline
void wlan_cm_reset_check_6ghz_security(struct wlan_objmgr_psoc *psoc) {}

static inline
bool wlan_cm_get_check_6ghz_security(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void wlan_cm_set_standard_6ghz_conn_policy(struct wlan_objmgr_psoc *psoc,
					   uint32_t value)
{
}

static inline
bool wlan_cm_get_standard_6ghz_conn_policy(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void wlan_cm_set_disable_vlp_sta_conn_to_sp_ap(struct wlan_objmgr_psoc *psoc,
					       bool value)
{}

static inline
bool wlan_cm_get_disable_vlp_sta_conn_to_sp_ap(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void wlan_cm_set_6ghz_key_mgmt_mask(struct wlan_objmgr_psoc *psoc,
				    uint32_t value) {}

static inline
uint32_t wlan_cm_get_6ghz_key_mgmt_mask(struct wlan_objmgr_psoc *psoc)
{
	return DEFAULT_KEYMGMT_6G_MASK;
}
#endif

#ifdef CONN_MGR_ADV_FEATURE
/**
 * wlan_cm_set_check_assoc_disallowed() - Set check assoc disallowed param
 * @psoc: pointer to psoc object
 * @value: value to be set
 *
 * Return: void
 */
void wlan_cm_set_check_assoc_disallowed(struct wlan_objmgr_psoc *psoc,
					bool value);

/**
 * wlan_cm_get_check_assoc_disallowed() - get check assoc disallowed param
 * @psoc: pointer to psoc object
 * @value: value to be filled
 *
 * Return: void
 */
void wlan_cm_get_check_assoc_disallowed(struct wlan_objmgr_psoc *psoc,
					bool *value);
#endif
#endif
