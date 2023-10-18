/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Define VDEV MLME structure and APIs
 */
#ifndef _WLAN_VDEV_MLME_H_
#define _WLAN_VDEV_MLME_H_

#include <wlan_vdev_mgr_tgt_if_rx_defs.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_vdev_mlme_api.h>
#include <wlan_ext_mlme_obj_types.h>

struct vdev_mlme_obj;
struct cnx_mgr;

/* Requestor ID for multiple vdev restart */
#define MULTIPLE_VDEV_RESTART_REQ_ID 0x1234

/* values for vdev_type */
#define WLAN_VDEV_MLME_TYPE_UNKNOWN   0x0
#define WLAN_VDEV_MLME_TYPE_AP   0x1
#define WLAN_VDEV_MLME_TYPE_STA  0x2
#define WLAN_VDEV_MLME_TYPE_IBSS 0x3
#define WLAN_VDEV_MLME_TYPE_MONITOR 0x4
#define WLAN_VDEV_MLME_TYPE_NAN 0x5
#define WLAN_VDEV_MLME_TYPE_OCB 0x6
#define WLAN_VDEV_MLME_TYPE_NDI 0x7

/* values for vdev_subtype */
#define WLAN_VDEV_MLME_SUBTYPE_UNKNOWN   0x0
#define WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE 0x1
#define WLAN_VDEV_MLME_SUBTYPE_P2P_CLIENT 0x2
#define WLAN_VDEV_MLME_SUBTYPE_P2P_GO 0x3
#define WLAN_VDEV_MLME_SUBTYPE_PROXY_STA 0x4
#define WLAN_VDEV_MLME_SUBTYPE_MESH 0x5
#define WLAN_VDEV_MLME_SUBTYPE_MESH_11S   0x6
#define WLAN_VDEV_MLME_SUBTYPE_SMART_MONITOR 0x7

/* vdev control flags (per bits) */
#define WLAN_VDEV_MLME_FLAGS_NON_MBSSID_AP      0x00000001
#define WLAN_VDEV_MLME_FLAGS_TRANSMIT_AP        0x00000002
#define WLAN_VDEV_MLME_FLAGS_NON_TRANSMIT_AP    0x00000004
#define WLAN_VDEV_MLME_FLAGS_EMA_MODE           0x00000008
#define WLAN_VDEV_MLME_FLAGS_MBSS_CMN_PARAM     0x00000010

/**
 * struct vdev_mlme_proto_generic - generic mlme proto structure
 * sent in frames
 * @dtim_period: frequency of data transmissions per beacon 1-255
 * @slot_time: slot time
 * @protection_mode: rts cts protection mode
 * @beacon_interval: beacon interval
 * @ldpc: low density parity check value
 * @nss: number of spatial stream
 * @nss_2g: 2.4GHz number of spatial stream
 * @nss_5g: 5GHz number of spatial stream
 * @tsfadjust: adjusted timer sync value
 */
struct vdev_mlme_proto_generic {
	uint8_t dtim_period;
	uint32_t slot_time;
	uint32_t protection_mode;
	uint16_t beacon_interval;
	uint8_t ldpc;
	uint8_t nss;
	uint8_t nss_2g;
	uint8_t nss_5g;
	uint64_t tsfadjust;
};

/**
 * struct vdev_mlme_proto_ap - ap specific mlme protocol
 * @mapping_switch_time: Mapping switch time of T2LM
 */
struct vdev_mlme_proto_ap {
	uint16_t mapping_switch_time;
};

/**
 * struct vdev_mlme_proto_sta - sta specific mlme protocol
 * @assoc_id: association id of station
 * @uapsd_cfg: uapsd configuration
 */
struct vdev_mlme_proto_sta {
	uint16_t assoc_id;
	uint16_t uapsd_cfg;
};

/**
 * struct vdev_mlme_proto_bss_color - bss color cfg
 * @flags: proposed for future use cases, currently not used.
 * @evt_type: bss color collision event.
 * @current_bss_color: current bss color.
 * @detection_period_ms: scan interval for both AP and STA mode.
 * @scan_period_ms: scan period for passive scan to detect collision.
 * @free_slot_expiry_time_ms: FW to notify host at timer expiry after
 *                            which Host will disable the bss color.
 */
struct vdev_mlme_proto_bss_color {
	uint32_t flags;
	uint8_t  evt_type;
	uint32_t current_bss_color;
	uint32_t detection_period_ms;
	uint32_t scan_period_ms;
	uint32_t free_slot_expiry_time_ms;
};

/**
 * struct vdev_mlme_vht_info - vdev VHT information
 * @caps: vht capabilities
 * @subfer: su beam former capability
 * @subfee: su beam formee capability
 * @mubfer: mu beam former capability
 * @mubfee: mu beam formee capability
 * @implicit_bf: Implicit BF support
 * @sounding_dimension: Beamformer number of sounding dimension
 * @bfee_sts_cap: beam formee STA capability
 * @allow_vht: vht capability status
 */
struct vdev_mlme_vht_info {
	uint32_t caps;
	uint8_t  subfer;
	uint8_t  mubfer;
	uint8_t  subfee;
	uint8_t  mubfee;
	uint8_t  implicit_bf;
	uint8_t  sounding_dimension;
	uint8_t  bfee_sts_cap;
	bool     allow_vht;
};

/**
 * struct vdev_mlme_ht_info - vdev HT information
 * @ht_caps: HT capabilities
 * @allow_ht: HT capability status
 */
struct vdev_mlme_ht_info {
	uint32_t ht_caps;
	bool     allow_ht;
};

/**
 * struct vdev_mlme_he_ops_info - vdev mlme HEOPS information
 * @he_ops: he ops
 */
struct vdev_mlme_he_ops_info {
	uint32_t he_ops;
};

#ifdef WLAN_FEATURE_11BE

/**
 * struct vdev_mlme_eht_caps_info - vdev mlme EHT capability information
 * @eht_maccap_epcspriaccess_support : EPCS Priority Access Supported
 * @eht_maccap_ehtomctrl_support     : EHT OM Control Support
 * @eht_maccap_trigtxop_sharing_mode1: Triggered TXOP Sharing mode1 Support
 * @eht_maccap_trigtxop_sharing_mode2: Triggered TXOP Sharing mode2 Support
 * @eht_maccap_rtwt_support          : Restricted TWT Support
 * @eht_maccap_scs_traffic_description : SCS Traffic Description Support
 * @eht_maccap_max_mpdu_len          : Maximum MPDU Length
 * @eht_maccap_max_ampdu_len_exp_etn : Maximum A-MPDU Length Exponent Extension
 * @eht_maccap_trs_support           : EHT TRS Support
 * @eht_maccap_txop_ret_supp_in_txop_mode2: TXOP Return Support In TXOP
 *                                          Sharing Mode 2
 * @eht_phycap_reserved              : Reserved
 * @eht_phycap_320mhzin6ghz          : Support For 320 MHz In 6 GHz
 * @eht_phycap_242tonerubwlt20mhz    : Support for 242-tone RU In BW Wider Than
 *                                     20 MHz
 * @eht_phycap_ndp4xehtltfand320nsgi : NDP With 4. EHT-LTF And 3.2 .s GI
 * @eht_phycap_partialbwulmu         : Partial Bandwidth UL MU-MIMO
 * @eht_phycap_subfmr                : SU Beamformer
 * @eht_phycap_subfme                : SU Beamformee
 * @eht_phycap_bfmesslt80mhz         : Beamformee SS (<= 80 MHz)
 * @eht_phycap_bfmess160mhz          : Beamformee SS (= 160 MHz)
 * @eht_phycap_bfmess320mhz          : Beamformee SS (= 320 MHz)
 * @eht_phycap_numsoundlt80mhz       : Number Of Sounding Dimensions (<=80 MHz)
 * @eht_phycap_numsound160mhz        : Number Of Sounding Dimensions (=160 MHz)
 * @eht_phycap_numsound320mhz        : Number Of Sounding Dimensions (=320 MHz)
 * @eht_phycap_ng16sufb              : Ng = 16 SU Feedback
 * @eht_phycap_ng16mufb              : Ng = 16 MU Feedback
 * @eht_phycap_codbk42sufb           : Codebook Size {4,2} SU Feedback
 * @eht_phycap_codbk75mufb           : Codebook Size {4,2} MU Feedback
 * @eht_phycap_trigsubffb            : Triggered SU Beamforming Feedback
 * @eht_phycap_trigmubfpartbwfb      : Triggered MU Beamforming Partial B
 *                                     Feedback
 * @eht_phycap_trigcqifb             : Triggered CQI Feedback
 * @eht_phycap_partbwdlmumimo        : Partial Bandwidth DL MU-MIMO
 * @eht_phycap_psrsr                 : PSR-Based SR Support
 * @eht_phycap_pwrbstfactor          : Power Boost Factor Support
 * @eht_phycap_4xehtltfand800nsgi    : EHT MU PPDU With 4xEHT-LTF And 0.8 .s GI
 * @eht_phycap_maxnc                 : Max Nc
 * @eht_phycap_nontrigcqifb          : Non-Triggered CQI Feedback
 * @eht_phycap_tx1024and4096qamls242toneru : Tx 1024-QAM And 4096-QAM <
 *                                           242-tone RU Support
 * @eht_phycap_rx1024and4096qamls242toneru : Rx 1024-QAM And 4096-QAM <
 *                                           242-tone RU Support
 * @eht_phycap_ppethrespresent       : PPE Thresholds Present
 * @eht_phycap_cmnnompktpad          : Common Nominal Packet Padding
 * @eht_phycap_maxnumehtltf          : Maximum Number Of Supported EHT-LTFs
 * @eht_phycap_supmcs15              : Support of MCS 15
 * @eht_phycap_ehtdupin6ghz          : Support Of EHT DUP In 6 GHz
 * @eht_phycap_20mhzopstarxndpwiderbw : Support For 20 MHz Operating STA
 *                                      Receiving NDP With Wider Bandwidth
 * @eht_phycap_nonofdmaulmumimolt80mhz : Non-OFDMA UL MU-MIMO (BW <= 80 MHz)
 * @eht_phycap_nonofdmaulmumimo160mhz : Non-OFDMA UL MU-MIMO (BW = 160 MHz)
 * @eht_phycap_nonofdmaulmumimo320mhz : Non-OFDMA UL MU-MIMO (BW = 320 MHz)
 * @eht_phycap_mubfmrlt80mhz         : MU Beamformer (BW <= 80 MHz)
 * @eht_phycap_mubfmr160mhz          : MU Beamformer (BW = 160 MHz)
 * @eht_phycap_mubfmr320mhz          : MU Beamformer (BW = 320 MHz)
 * @eht_phycap_tb_sounding_feedback_rl:
 * @eht_phycap_rx1024qamwiderbwdlofdma:
 * @eht_phycap_rx4096qamwiderbwdlofdma:
 */
struct vdev_mlme_eht_caps_info {
	uint32_t eht_maccap_epcspriaccess_support :1,
		 eht_maccap_ehtomctrl_support     :1,
		 eht_maccap_trigtxop_sharing_mode1      :1,
		 eht_maccap_trigtxop_sharing_mode2      :1,
		 eht_maccap_rtwt_support                :1,
		 eht_maccap_scs_traffic_description     :1,
		 eht_maccap_max_mpdu_len                :2,
		 eht_maccap_max_ampdu_len_exp_etn       :1,
		 eht_maccap_trs_support                 :1,
		 eht_maccap_txop_ret_supp_in_txop_mode2 :1;
	uint32_t eht_phycap_reserved                    :1,
		 eht_phycap_320mhzin6ghz                :1,
		 eht_phycap_242tonerubwlt20mhz          :1,
		 eht_phycap_ndp4xehtltfand320nsgi       :1,
		 eht_phycap_partialbwulmu               :1,
		 eht_phycap_subfmr                      :1,
		 eht_phycap_subfme                      :1,
		 eht_phycap_bfmesslt80mhz               :3,
		 eht_phycap_bfmess160mhz                :3,
		 eht_phycap_bfmess320mhz                :3,
		 eht_phycap_numsoundlt80mhz             :3,
		 eht_phycap_numsound160mhz              :3,
		 eht_phycap_numsound320mhz              :3,
		 eht_phycap_ng16sufb                    :1,
		 eht_phycap_ng16mufb                    :1,
		 eht_phycap_codbk42sufb                 :1,
		 eht_phycap_codbk75mufb                 :1,
		 eht_phycap_trigsubffb                  :1,
		 eht_phycap_trigmubfpartbwfb            :1,
		 eht_phycap_trigcqifb                   :1;
	uint32_t eht_phycap_partbwdlmumimo              :1,
		 eht_phycap_psrsr                       :1,
		 eht_phycap_pwrbstfactor                :1,
		 eht_phycap_4xehtltfand800nsgi          :1,
		 eht_phycap_maxnc                       :4,
		 eht_phycap_nontrigcqifb                :1,
		 eht_phycap_tx1024and4096qamls242toneru :1,
		 eht_phycap_rx1024and4096qamls242toneru :1,
		 eht_phycap_ppethrespresent             :1,
		 eht_phycap_cmnnompktpad                :2,
		 eht_phycap_maxnumehtltf                :5,
		 eht_phycap_supmcs15                    :4,
		 eht_phycap_ehtdupin6ghz                :1,
		 eht_phycap_20mhzopstarxndpwiderbw      :1,
		 eht_phycap_nonofdmaulmumimolt80mhz     :1,
		 eht_phycap_nonofdmaulmumimo160mhz      :1,
		 eht_phycap_nonofdmaulmumimo320mhz      :1,
		 eht_phycap_mubfmrlt80mhz               :1,
		 eht_phycap_mubfmr160mhz                :1,
		 eht_phycap_mubfmr320mhz                :1,
		 eht_phycap_tb_sounding_feedback_rl     :1;
	uint32_t eht_phycap_rx1024qamwiderbwdlofdma     :1,
		 eht_phycap_rx4096qamwiderbwdlofdma     :1;
};

/**
 * struct vdev_mlme_eht_ops_info - vdev mlme EHTOPS information
 * @eht_ops: eht ops
 */
struct vdev_mlme_eht_ops_info {
	uint32_t eht_ops;
};
#endif

/**
 * enum mlme_vdev_dot11_mode - Dot11 mode of the vdev
 * @MLME_VDEV_DOT11_MODE_AUTO: vdev uses mlme_dot11_mode
 * @MLME_VDEV_DOT11_MODE_11N: vdev supports 11N mode
 * @MLME_VDEV_DOT11_MODE_11AC: vdev supports 11AC mode
 * @MLME_VDEV_DOT11_MODE_11AX: vdev supports 11AX mode
 * @MLME_VDEV_DOT11_MODE_11BE: vdev supports 11BE mode
 */
enum mlme_vdev_dot11_mode {
	MLME_VDEV_DOT11_MODE_AUTO,
	MLME_VDEV_DOT11_MODE_11N,
	MLME_VDEV_DOT11_MODE_11AC,
	MLME_VDEV_DOT11_MODE_11AX,
	MLME_VDEV_DOT11_MODE_11BE,
};

/**
 * struct vdev_mlme_proto - vdev protocol structure holding information
 * that is used in frames
 * @vdev_dot11_mode: supported dot11 mode
 * @generic: generic protocol information
 * @ap: ap specific protocol information
 * @sta: sta specific protocol information
 * @vht_info: vht information
 * @ht_info: ht capabilities information
 * @he_ops_info: he ops information
 * @eht_cap_info: EHT capability information
 * @eht_ops_info: EHT operation information
 * @bss_color: 11ax HE BSS Color information
 */
struct vdev_mlme_proto {
	enum mlme_vdev_dot11_mode vdev_dot11_mode;
	struct vdev_mlme_proto_generic generic;
	struct vdev_mlme_proto_ap ap;
	struct vdev_mlme_proto_sta sta;
	struct vdev_mlme_vht_info vht_info;
	struct vdev_mlme_ht_info ht_info;
	struct vdev_mlme_he_ops_info he_ops_info;
#ifdef WLAN_FEATURE_11BE
	struct vdev_mlme_eht_caps_info eht_cap_info;
	struct vdev_mlme_eht_ops_info eht_ops_info;
#endif
	struct vdev_mlme_proto_bss_color bss_color;
};

/**
 * struct vdev_mlme_mgmt_generic - generic vdev mlme mgmt cfg
 * @rts_threshold: RTS threshold
 * @frag_threshold: Fragmentation threshold
 * @probe_delay: time in msec for delaying to send first probe request
 * @repeat_probe_time: probe request transmission time
 * @drop_unencry: drop unencrypted status
 * @tx_pwrlimit: Tx power limit
 * @tx_power: Tx power
 * @minpower: Min power
 * @maxpower: Max power
 * @maxregpower: max regulatory power
 * @antennamax: max antenna
 * @reg_class_id: reg domain class id
 * @ampdu: ampdu limit
 * @amsdu: amsdu limit
 * @ssid: service set identifier
 * @ssid_len: ssid length
 * @type: vdev type
 * @subtype: vdev subtype
 * @rx_decap_type: rx decap type
 * @tx_encap_type: tx encap type
 * @disable_hw_ack: disable ha ack flag
 * @bssid: bssid
 * @phy_mode: phy mode
 * @special_vdev_mode: indicates special vdev mode
 * @is_sap_go_moved_1st_on_csa: Indicates if STA receives
 *				CSA to a DFS channel
 * @he_spr_sr_ctrl:     Spatial reuse SR control
 * @he_spr_non_srg_pd_max_offset: Non-SRG PD max offset
 * @he_spr_srg_max_pd_offset: SRG PD max offset
 * @he_spr_srg_min_pd_offset: SRG PD min offset
 * @he_spr_enabled:     Spatial reuse enabled or not
 * @he_spr_disabled_due_conc: spr disabled due to concurrency
 * @sr_prohibit_enabled:
 * @srg_bss_color: srg bss color
 * @srg_partial_bssid: srg partial bssid
 * @he_curr_non_srg_pd_threshold: current configured NON-SRG PD threshold
 * @he_curr_srg_pd_threshold: current configured SRG PD threshold
 * @is_pd_threshold_present: PD threshold is present in SR enable command or not
 */
struct vdev_mlme_mgmt_generic {
	uint32_t rts_threshold;
	uint32_t frag_threshold;
	uint32_t probe_delay;
	uint32_t repeat_probe_time;
	uint32_t drop_unencry;
	uint32_t tx_pwrlimit;
	uint8_t tx_power;
	uint8_t minpower;
	uint8_t maxpower;
	uint8_t maxregpower;
	uint8_t antennamax;
	uint8_t reg_class_id;
	uint8_t ampdu;
	uint8_t amsdu;
	char ssid[WLAN_SSID_MAX_LEN + 1];
	uint8_t ssid_len;
	uint8_t type;
	uint8_t subtype;
	uint8_t rx_decap_type;
	uint8_t tx_encap_type;
	bool disable_hw_ack;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint32_t phy_mode;
	bool special_vdev_mode;
	bool is_sap_go_moved_1st_on_csa;
#ifdef WLAN_FEATURE_SR
	uint8_t he_spr_sr_ctrl;
	uint8_t he_spr_non_srg_pd_max_offset;
	uint8_t he_spr_srg_max_pd_offset;
	uint8_t he_spr_srg_min_pd_offset;
	bool he_spr_enabled;
	bool he_spr_disabled_due_conc;
	bool sr_prohibit_enabled;
	uint64_t srg_bss_color;
	uint64_t srg_partial_bssid;
	int32_t he_curr_non_srg_pd_threshold;
	int32_t he_curr_srg_pd_threshold;
	bool is_pd_threshold_present;
#endif
};

/*
 * struct wlan_vdev_aid_mgr - AID manager
 * @aid_bitmap: AID bitmap array
 * @start_aid: start of AID index
 * @max_aid: Max allowed AID
 * @ref_cnt:  to share AID across VDEVs for MBSSID
 *
 * NB: Not using kernel-doc comment since the kernel-doc script
 *     doesn't handle the qdf_bitmap() macro.
 */
struct wlan_vdev_aid_mgr {
	qdf_bitmap(aid_bitmap, WLAN_UMAC_MAX_AID);
	uint16_t start_aid;
	uint16_t max_aid;
	qdf_atomic_t ref_cnt;
};

/**
 * struct vdev_mlme_mgmt_ap - ap specific vdev mlme mgmt cfg
 * @hidden_ssid: flag to indicate whether it is hidden ssid
 * @cac_duration_ms: cac duration in milliseconds
 * @aid_mgr: AID bitmap mgr
 * @max_chan_switch_time: Max channel switch time in milliseconds.
 * @last_bcn_ts_ms: Timestamp (in milliseconds) of the last beacon sent on the
 *                  CSA triggered channel.
 * @is_acs_mode: True if SAP is started in ACS mode
 */
struct vdev_mlme_mgmt_ap {
	bool hidden_ssid;
	uint32_t cac_duration_ms;
	struct wlan_vdev_aid_mgr *aid_mgr;
	uint32_t max_chan_switch_time;
	unsigned long last_bcn_ts_ms;
	bool is_acs_mode;
};

/**
 * struct vdev_mlme_mgmt_sta - sta specific vdev mlme mgmt cfg
 * @he_mcs_12_13_map: map to indicate mcs12/13 caps of peer&dut
 */
struct vdev_mlme_mgmt_sta {
	uint16_t he_mcs_12_13_map;
};

/**
 * struct vdev_mlme_inactivity_params - vdev mlme inactivity parameters
 * @bmiss_first_bcnt: bmiss first time
 * @bmiss_final_bcnt: bmiss final time
 * @keepalive_min_idle_inactive_time_secs: min time AP consider STA to be
 * inactive
 * @keepalive_max_idle_inactive_time_secs: max inactive idle time for AP to send
 * data-null
 * @keepalive_max_unresponsive_time_secs: max time to send WMI_STA_KICKOUT
 */
struct vdev_mlme_inactivity_params {
	uint32_t bmiss_first_bcnt;
	uint32_t bmiss_final_bcnt;
	uint32_t keepalive_min_idle_inactive_time_secs;
	uint32_t keepalive_max_idle_inactive_time_secs;
	uint32_t keepalive_max_unresponsive_time_secs;
};

/**
 * enum vdev_ratemask_type - ratemask phy type
 * @WLAN_VDEV_RATEMASK_TYPE_CCK: phy type CCK
 * @WLAN_VDEV_RATEMASK_TYPE_HT: phy type ht
 * @WLAN_VDEV_RATEMASK_TYPE_VHT: phy type vht
 * @WLAN_VDEV_RATEMASK_TYPE_HE: phy type he
 * @WLAN_VDEV_RATEMASK_TYPE_MAX: Maximum enumeration
 */
enum vdev_ratemask_type {
	WLAN_VDEV_RATEMASK_TYPE_CCK,
	WLAN_VDEV_RATEMASK_TYPE_HT,
	WLAN_VDEV_RATEMASK_TYPE_VHT,
	WLAN_VDEV_RATEMASK_TYPE_HE,
	WLAN_VDEV_RATEMASK_TYPE_MAX,
};

/**
 * struct vdev_ratemask_params -  vdev ratemask parameters
 * @type: ratemask phy type
 * @lower32: ratemask lower32 bitmask
 * @higher32: ratemask higher32 bitmask
 * @lower32_2: ratemask lower32_2 bitmask
 * @higher32_2: rtaemask higher32_2 bitmask
 */
struct vdev_ratemask_params {
	uint32_t lower32;
	uint32_t higher32;
	uint32_t lower32_2;
	uint32_t higher32_2;
};

/**
 * struct vdev_mlme_rate_info - vdev mlme rate information
 * @rate_flags: dynamic bandwidth info
 * @per_band_tx_mgmt_rate: per band Tx mgmt rate
 * @max_rate: max bandwidth rate
 * @tx_mgmt_rate: Tx Mgmt rate
 * @bcn_tx_rate: beacon Tx rate
 * @bcn_tx_rate_code: beacon Tx rate code
 * @rtscts_tx_rate: RTS/CTS Tx rate
 * @ratemask_params: vdev ratemask params per phy type
 * @half_rate: Half rate
 * @quarter_rate: quarter rate
 */
struct vdev_mlme_rate_info {
	uint32_t rate_flags;
	uint32_t per_band_tx_mgmt_rate;
	uint32_t max_rate;
	uint32_t tx_mgmt_rate;
	uint32_t bcn_tx_rate;
#ifdef WLAN_BCN_RATECODE_ENABLE
	uint32_t bcn_tx_rate_code;
#endif
	uint32_t rtscts_tx_rate;
	struct vdev_ratemask_params ratemask_params[
					WLAN_VDEV_RATEMASK_TYPE_MAX];
	bool     half_rate;
	bool     quarter_rate;
};

/**
 * struct vdev_mlme_chainmask_info - vdev mlme chainmask information
 * @tx_chainmask: Tx chainmask
 * @rx_chainmask: Rx Chainmask
 * @num_rx_chain: Num of bits set in Rx chain
 * @num_tx_chain: Num of bits set in Tx chain
 */
struct vdev_mlme_chainmask_info {
	uint8_t tx_chainmask;
	uint8_t rx_chainmask;
	uint8_t num_rx_chain;
	uint8_t num_tx_chain;
};

/**
 * struct vdev_mlme_powersave_info - vdev mlme powersave information
 * @packet_powersave: packet powersave
 * @max_li_of_moddtim: max mod dtim
 * @dyndtim_cnt: dynamic dtim count
 * @listen_interval: listen interval
 * @moddtim_cnt: mod dtim count
 */
struct vdev_mlme_powersave_info {
	uint32_t packet_powersave;
	uint32_t max_li_of_moddtim;
	uint32_t dyndtim_cnt;
	uint32_t listen_interval;
	uint32_t moddtim_cnt;
};

/**
 * struct vdev_mlme_beacon_info - vdev mlme beacon information
 * @beacon_buffer: buffer allocated for beacon frame
 * @beacon_offsets: beacon IE's offsets
 */
struct vdev_mlme_beacon_info {
	qdf_nbuf_t beacon_buffer;
	void *beacon_offsets;
};

/**
 * struct vdev_mlme_mbss_11ax - mbss 11ax fields required for up cmd
 * @profile_idx: profile index of the connected non-trans ap (mbssid case).
 *              0  means invalid.
 * @profile_num: the total profile numbers of non-trans aps (mbssid case).
 *              0 means non-MBSS AP.
 * @mbssid_flags: MBSS IE flags indicating vdev type
 * @vdevid_trans: id of transmitting vdev for MBSS IE
 * @vdev_bmap: vdev bitmap of VAPs in MBSS group
 * @is_cmn_param: flag to check mbss common param
 * @trans_bssid: bssid of transmitted AP (MBSS IE case)
 * @is_multi_mbssid: Flag to identify multi group mbssid support
 * @grp_id: Group id of current vdev
 */
struct vdev_mlme_mbss_11ax {
	uint32_t profile_idx;
	uint32_t profile_num;
	uint32_t mbssid_flags;
	uint8_t vdevid_trans;
	unsigned long vdev_bmap;
	bool is_cmn_param;
	uint8_t trans_bssid[QDF_MAC_ADDR_SIZE];
	bool is_multi_mbssid;
	uint32_t grp_id;
};

/**
 * struct vdev_mlme_mgmt - vdev mlme mgmt related cfg
 * @generic: generic mgmt information
 * @ap: ap specific mgmt information
 * @sta: sta specific mgmt information
 * @inactivity_params: inactivity parameters
 * @rate_info: bandwidth rate information
 * @chainmask_info: Chainmask information
 * @powersave_info: Power save parameters
 * @beacon_info: beacon buffer information
 * @mbss_11ax: MBSS 11ax information
 */
struct vdev_mlme_mgmt {
	struct vdev_mlme_mgmt_generic generic;
	struct vdev_mlme_mgmt_ap ap;
	struct vdev_mlme_mgmt_sta sta;
	struct vdev_mlme_inactivity_params inactivity_params;
	struct vdev_mlme_rate_info rate_info;
	struct vdev_mlme_chainmask_info chainmask_info;
	struct vdev_mlme_powersave_info powersave_info;
	struct vdev_mlme_beacon_info beacon_info;
	struct vdev_mlme_mbss_11ax mbss_11ax;
};

/**
 * enum beacon_update_op - Beacon update op type
 * @BEACON_INIT:      Initialize beacon
 * @BEACON_REINIT:    Re-initialize beacon
 * @BEACON_UPDATE:    Update dynamic fields of beacon
 * @BEACON_CSA:       Enable CSA IE
 * @BEACON_FREE:      Beacon buffer free
 */
enum beacon_update_op {
	BEACON_INIT,
	BEACON_REINIT,
	BEACON_UPDATE,
	BEACON_CSA,
	BEACON_FREE,
};

/**
 * enum vdev_cmd_type - Command type
 * @START_REQ:      Start request
 * @RESTART_REQ:    Restart request
 * @STOP_REQ: STOP request
 * @DELETE_REQ: DELETE request
 */
enum vdev_cmd_type {
	START_REQ,
	RESTART_REQ,
	STOP_REQ,
	DELETE_REQ,
};

/**
 * enum vdev_start_resp_type - start respone type
 * @START_RESPONSE:  Start response
 * @RESTART_RESPONSE: Restart response
 */
enum vdev_start_resp_type {
	START_RESPONSE = 0,
	RESTART_RESPONSE,
};

/**
 * struct vdev_mlme_ops - VDEV MLME operation callbacks structure
 * @mlme_vdev_validate_basic_params:    callback to validate VDEV basic params
 * @mlme_vdev_reset_proto_params:       callback to Reset protocol params
 * @mlme_vdev_start_send:               callback to initiate actions of VDEV
 *                                      MLME start operation
 * @mlme_vdev_restart_send:             callback to initiate actions of VDEV
 *                                      MLME restart operation
 * @mlme_vdev_stop_start_send:          callback to block start/restart VDEV
 *                                      request command
 * @mlme_vdev_start_continue:           callback to initiate operations on
 *                                      LMAC/FW start response
 * @mlme_vdev_sta_conn_start:           callback to initiate STA connection
 * @mlme_vdev_start_req_failed:
 * @mlme_vdev_up_send:                  callback to initiate actions of VDEV
 *                                      MLME up operation
 * @mlme_vdev_notify_up_complete:       callback to notify VDEV MLME on moving
 *                                      to UP state
 * @mlme_vdev_notify_roam_start:        callback to initiate roaming
 * @mlme_vdev_update_beacon:            callback to initiate beacon update
 * @mlme_vdev_disconnect_peers:         callback to initiate disconnection of
 *                                      peers
 * @mlme_vdev_dfs_cac_timer_stop:       callback to stop the DFS CAC timer
 * @mlme_vdev_stop_send:                callback to initiate actions of VDEV
 *                                      MLME stop operation
 * @mlme_vdev_stop_continue:            callback to initiate operations on
 *                                      LMAC/FW stop response
 * @mlme_vdev_bss_peer_delete_continue: callback to initiate operations on BSS
 *                                      peer delete completion
 * @mlme_vdev_down_send:                callback to initiate actions of VDEV
 *                                      MLME down operation
 * @mlme_vdev_notify_down_complete:
 * @mlme_vdev_ext_stop_rsp:
 * @mlme_vdev_ext_start_rsp:
 * @mlme_vdev_notify_start_state_exit:  callback to notify on vdev start
 *                                      start state exit
 * @mlme_vdev_is_newchan_no_cac:        callback to check CAC is required
 * @mlme_vdev_ext_peer_delete_all_rsp:  callback to initiate actions for
 *                                      vdev mlme peer delete all response
 * @mlme_vdev_dfs_cac_wait_notify:      callback to notify about CAC state
 * @mlme_vdev_csa_complete:             callback to indicate CSA complete
 * @mlme_vdev_sta_disconn_start:        callback to initiate STA disconnection
 * @mlme_vdev_reconfig_notify:          callback to notify ml reconfing link
 *                                      delete start operation after receive
 *                                      the first ml reconfig IE
 * @mlme_vdev_reconfig_timer_complete:  callback to process ml reconfing
 *                                      operation
 * @mlme_vdev_notify_mlo_sync_wait_entry:
 */
struct vdev_mlme_ops {
	QDF_STATUS (*mlme_vdev_validate_basic_params)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_reset_proto_params)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_start_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_restart_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_stop_start_send)(
				struct vdev_mlme_obj *vdev_mlme,
				enum vdev_cmd_type type,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_start_continue)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_sta_conn_start)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_start_req_failed)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_up_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_notify_up_complete)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_notify_roam_start)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_update_beacon)(
				struct vdev_mlme_obj *vdev_mlme,
				enum beacon_update_op op,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_disconnect_peers)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_dfs_cac_timer_stop)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_stop_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_stop_continue)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_down_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_notify_down_complete)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_ext_stop_rsp)(
				struct vdev_mlme_obj *vdev_mlme,
				struct vdev_stop_response *rsp);
	QDF_STATUS (*mlme_vdev_ext_start_rsp)(
				struct vdev_mlme_obj *vdev_mlme,
				struct vdev_start_response *rsp);
	QDF_STATUS (*mlme_vdev_notify_start_state_exit)(
				struct vdev_mlme_obj *vdev_mlme);
	QDF_STATUS (*mlme_vdev_is_newchan_no_cac)(
				struct vdev_mlme_obj *vdev_mlme);
	QDF_STATUS (*mlme_vdev_ext_peer_delete_all_rsp)(
				struct vdev_mlme_obj *vdev_mlme,
				struct peer_delete_all_response *rsp);
	QDF_STATUS (*mlme_vdev_dfs_cac_wait_notify)(
				struct vdev_mlme_obj *vdev_mlme);
	QDF_STATUS (*mlme_vdev_csa_complete)(
				struct vdev_mlme_obj *vdev_mlme);
	QDF_STATUS (*mlme_vdev_sta_disconn_start)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_reconfig_notify)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t *tbtt_count, uint16_t bcn_int);
	void (*mlme_vdev_reconfig_timer_complete)(
				struct vdev_mlme_obj *vdev_mlme);
	QDF_STATUS (*mlme_vdev_notify_mlo_sync_wait_entry)(
				struct vdev_mlme_obj *vdev_mlme);
};

/**
 * struct vdev_mlme_obj - VDEV MLME component object
 * @proto: VDEV MLME proto substructure
 * @mgmt: VDEV MLME mgmt substructure
 * @sm_lock:              VDEV SM lock
 * @vdev_cmd_lock:        VDEV MLME command atomicity
 * @sm_hdl:               VDEV SM handle
 * @cnx_mgr_ctx: connection manager context, valid for STA and P2P-CLI mode only
 * @vdev: Pointer to vdev objmgr
 * @ops:                  VDEV MLME callback table
 * @ext_vdev_ptr:         VDEV MLME legacy pointer
 * @reg_tpc_obj:          Regulatory transmit power info
 * @ml_reconfig_timer: VDEV ml reconfig timer
 * @ml_reconfig_started:  Flag to indicate reconfig status for vdev
 */
struct vdev_mlme_obj {
	struct vdev_mlme_proto proto;
	struct vdev_mlme_mgmt  mgmt;
#ifdef VDEV_SM_LOCK_SUPPORT
	qdf_spinlock_t sm_lock;
	qdf_mutex_t vdev_cmd_lock;
#endif
	struct wlan_sm *sm_hdl;
	union {
		struct cnx_mgr *cnx_mgr_ctx;
	};
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mlme_ops *ops;
	mlme_vdev_ext_t *ext_vdev_ptr;
	struct reg_tpc_power_info reg_tpc_obj;
	qdf_timer_t ml_reconfig_timer;
	bool ml_reconfig_started;
};

/**
 * wlan_vdev_mlme_set_ssid() - set ssid
 * @vdev: VDEV object
 * @ssid: SSID (input)
 * @ssid_len: Length of SSID
 *
 * API to set the SSID of VDEV
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: QDF_STATUS_SUCCESS, if update is done
 *         QDF_STATUS error, if ssid length is > max ssid len
 */
static inline QDF_STATUS wlan_vdev_mlme_set_ssid(
				struct wlan_objmgr_vdev *vdev,
				const uint8_t *ssid, uint8_t ssid_len)
{
	struct vdev_mlme_obj *vdev_mlme;

	/* This API is invoked with lock acquired, do not add log prints */
	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	if (ssid_len <= WLAN_SSID_MAX_LEN) {
		qdf_mem_copy(vdev_mlme->mgmt.generic.ssid, ssid, ssid_len);
		vdev_mlme->mgmt.generic.ssid_len = ssid_len;
	} else {
		vdev_mlme->mgmt.generic.ssid_len = 0;
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_vdev_mlme_get_ssid() - get ssid
 * @vdev: VDEV object
 * @ssid: SSID
 * @ssid_len: Length of SSID
 *
 * API to get the SSID of VDEV, it updates the SSID and its length
 * in @ssid, @ssid_len respectively
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: SUCCESS, if update is done
 *          FAILURE, if ssid length is > max ssid len
 */
static inline QDF_STATUS wlan_vdev_mlme_get_ssid(
				struct wlan_objmgr_vdev *vdev,
				 uint8_t *ssid, uint8_t *ssid_len)
{
	struct vdev_mlme_obj *vdev_mlme;

	/* This API is invoked with lock acquired, do not add log prints */
	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	if (vdev_mlme->mgmt.generic.ssid_len > 0) {
		*ssid_len = vdev_mlme->mgmt.generic.ssid_len;
		qdf_mem_copy(ssid, vdev_mlme->mgmt.generic.ssid, *ssid_len);
	} else {
		*ssid_len = 0;
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_vdev_mlme_set_nss() - set NSS
 * @vdev: VDEV object
 * @nss: nss configured by user
 *
 * API to set the Number of Spatial streams
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_nss(
				struct wlan_objmgr_vdev *vdev,
				uint8_t nss)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->proto.generic.nss = nss;
}

/**
 * wlan_vdev_mlme_get_nss() - get NSS
 * @vdev: VDEV object
 *
 * API to get the Number of Spatial Streams
 *
 * Return: nss value
 */
static inline uint8_t wlan_vdev_mlme_get_nss(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->proto.generic.nss;
}

/**
 * wlan_vdev_mlme_set_txchainmask() - set Tx chainmask
 * @vdev: VDEV object
 * @chainmask : chainmask either configured by user or max supported
 *
 * API to set the Tx chainmask
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_txchainmask(
				struct wlan_objmgr_vdev *vdev,
				uint8_t chainmask)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);

	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.chainmask_info.tx_chainmask = chainmask;
}

/**
 * wlan_vdev_mlme_get_txchainmask() - get Tx chainmask
 * @vdev: VDEV object
 *
 * API to get the Tx chainmask
 *
 * Return: Tx chainmask either configured by user or max supported
 */
static inline uint8_t wlan_vdev_mlme_get_txchainmask(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.chainmask_info.tx_chainmask;
}

/**
 * wlan_vdev_mlme_set_rxchainmask() - set Rx chainmask
 * @vdev: VDEV object
 * @chainmask : Rx chainmask either configured by user or max supported
 *
 * API to set the Rx chainmask
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_rxchainmask(
				struct wlan_objmgr_vdev *vdev,
				uint8_t chainmask)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.chainmask_info.rx_chainmask = chainmask;
}

/**
 * wlan_vdev_mlme_get_rxchainmask() - get Rx chainmask
 * @vdev: VDEV object
 *
 * API to get the Rx chainmask
 *
 * Return: Rx chainmask either configured by user or max supported
 */
static inline uint8_t wlan_vdev_mlme_get_rxchainmask(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	/* This API is invoked with lock acquired, do not add log prints */
	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.chainmask_info.rx_chainmask;
}

/**
 * wlan_vdev_mlme_set_txpower() - set tx power
 * @vdev: VDEV object
 * @txpow: tx power either configured by used or max allowed
 *
 * API to set the tx power
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_txpower(
					struct wlan_objmgr_vdev *vdev,
					uint8_t txpow)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.tx_power = txpow;
}

/**
 * wlan_vdev_mlme_get_txpower() - get tx power
 * @vdev: VDEV object
 *
 * API to get the tx power
 *
 * Return: tx power either configured by used or max allowed
 */
static inline uint8_t wlan_vdev_mlme_get_txpower(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.generic.tx_power;
}

/**
 * wlan_vdev_mlme_set_maxrate() - set max rate
 * @vdev: VDEV object
 * @maxrate: configured by used or based on configured mode
 *
 * API to set the max rate the vdev supports
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_maxrate(
				struct wlan_objmgr_vdev *vdev,
				uint32_t maxrate)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.rate_info.max_rate = maxrate;
}

/**
 * wlan_vdev_mlme_get_maxrate() - get max rate
 * @vdev: VDEV object
 *
 * API to get the max rate the vdev supports
 *
 * Return: configured by used or based on configured mode
 */
static inline uint32_t wlan_vdev_mlme_get_maxrate(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.rate_info.max_rate;
}

/**
 * wlan_vdev_mlme_set_txmgmtrate() - set txmgmtrate
 * @vdev: VDEV object
 * @txmgmtrate: Tx Mgmt rate
 *
 * API to set Mgmt Tx rate
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_txmgmtrate(
				struct wlan_objmgr_vdev *vdev,
				uint32_t txmgmtrate)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.rate_info.tx_mgmt_rate = txmgmtrate;
}

/**
 * wlan_vdev_mlme_get_txmgmtrate() - get txmgmtrate
 * @vdev: VDEV object
 *
 * API to get Mgmt Tx rate
 *
 * Return: Tx Mgmt rate
 */
static inline uint32_t wlan_vdev_mlme_get_txmgmtrate(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.rate_info.tx_mgmt_rate;
}

/**
 * wlan_vdev_mlme_is_special_vdev() - check given vdev is a special vdev
 * @vdev: VDEV object
 *
 * API to check given vdev is a special vdev.
 *
 * Return: true if given vdev is special vdev, else false
 */
static inline bool wlan_vdev_mlme_is_special_vdev(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	if (!vdev)
		return false;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return false;

	return vdev_mlme->mgmt.generic.special_vdev_mode;
}

/**
 * wlan_vdev_mlme_is_sap_go_move_before_sta() - check if SAP / GO
 * moved to new channel before STA's movement upon receiving CSA
 *
 * @vdev: VDEV object
 *
 * API to check in STA+SAP/GO SCC concurrency, whether SAP / GO moved before
 * STA's movement on receiving CSA from peer AP to connected STA.
 *
 * Return: true if SAP / GO moved before STA else false
 */
static inline
bool wlan_vdev_mlme_is_sap_go_move_before_sta(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return false;

	return vdev_mlme->mgmt.generic.is_sap_go_moved_1st_on_csa;
}

/**
 * wlan_vdev_mlme_set_sap_go_move_before_sta() - Set flag if SAP / GO
 * moves to new channel before STA's movement upon receiving CSA
 *
 * @vdev: VDEV object
 * @sap_go_moved_before_sta: Flag to indicate True when SAP / GO
 *  moves before STA
 *
 * API to set True in STA+SAP/GO SCC concurrency, when SAP / GO moves before
 * STA's movement on receiving CSA from peer AP to connected STA.
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_set_sap_go_move_before_sta(struct wlan_objmgr_vdev *vdev,
					       bool sap_go_moved_before_sta)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.is_sap_go_moved_1st_on_csa =
						sap_go_moved_before_sta;
}

#ifdef WLAN_FEATURE_11AX
/**
 * wlan_vdev_mlme_set_he_mcs_12_13_map() - set he mcs12/13 map
 * @vdev: VDEV object
 * @he_mcs_12_13_map: he mcs12/13 map from self&peer
 *
 * API to set he mcs 12/13 map
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_he_mcs_12_13_map(
				struct wlan_objmgr_vdev *vdev,
				uint16_t he_mcs_12_13_map)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.sta.he_mcs_12_13_map = he_mcs_12_13_map;
}

/**
 * wlan_vdev_mlme_get_he_mcs_12_13_map() - get he mcs12/13 map
 * @vdev: VDEV object
 *
 * API to get he mcs12/13 support capability
 *
 * Return: he mcs12/13 map
 */
static inline uint16_t wlan_vdev_mlme_get_he_mcs_12_13_map(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.sta.he_mcs_12_13_map;
}

#ifdef WLAN_FEATURE_SR
/**
 * wlan_vdev_mlme_get_sr_ctrl() - get spatial reuse SR control
 * @vdev: VDEV object
 *
 * API to retrieve the spatial reuse SR control from VDEV
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: SR control
 */
static inline uint8_t wlan_vdev_mlme_get_sr_ctrl(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.generic.he_spr_sr_ctrl;
}

/**
 * wlan_vdev_mlme_get_non_srg_pd_offset() - get spatial reuse non srg pd offset
 * @vdev: VDEV object
 *
 * API to retrieve the spatial reuse pd offset from VDEV
 *
 * Return: max non srg pd offset
 */
static inline uint8_t wlan_vdev_mlme_get_non_srg_pd_offset(
						struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return 0;

	return vdev_mlme->mgmt.generic.he_spr_non_srg_pd_max_offset;
}

/**
 * wlan_vdev_mlme_get_he_spr_enabled() - spatial reuse enabled or not
 * @vdev: VDEV object
 *
 * API to check whether the spatial reuse enabled or not
 *
 * Return: true if Spatial reuse enabled, false if not
 */
static inline bool wlan_vdev_mlme_get_he_spr_enabled(
						struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return false;

	return vdev_mlme->mgmt.generic.he_spr_enabled;
}

/**
 * wlan_vdev_mlme_is_sr_disable_due_conc() - spatial reuse disabled due
 *					     to concurrency
 * @vdev: VDEV object
 *
 * API to check whether the spatial reuse disabled due to concurrency or not
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return:
 * true/false: true if spatial reuse disabled due to concurrency else false
 */
static inline
bool wlan_vdev_mlme_is_sr_disable_due_conc(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return false;

	return vdev_mlme->mgmt.generic.he_spr_disabled_due_conc;
}

/**
 * wlan_vdev_mlme_is_sr_prohibit_en() - spatial reuse PD prohibit enabled
 *					/ disabled (HE_SIGA_Val15_Allowed)
 * @vdev: VDEV object
 *
 * API to check whether the spatial reuse PD prohibit is enabled / disabled
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: true/false: Spatial reuse PD prohibit enabled / disabled
 */
static inline
bool wlan_vdev_mlme_is_sr_prohibit_en(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return false;

	return vdev_mlme->mgmt.generic.sr_prohibit_enabled;
}

/**
 * wlan_vdev_mlme_set_sr_ctrl() - set spatial reuse SR control
 * @vdev: VDEV object
 * @sr_ctrl: value to set
 *
 * API to set the spatial reuse SR control
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_sr_ctrl(struct wlan_objmgr_vdev *vdev,
					      uint8_t sr_ctrl)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.he_spr_sr_ctrl = sr_ctrl;
}

/**
 * wlan_vdev_mlme_set_non_srg_pd_offset() - set spatial reuse non srg
 * pd max offset
 * @vdev: VDEV object
 * @non_srg_pd_max_offset: value to set
 *
 * API to set the spatial reuse pd max offset
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_set_non_srg_pd_offset(struct wlan_objmgr_vdev *vdev,
				     uint8_t non_srg_pd_max_offset)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.he_spr_non_srg_pd_max_offset =
						non_srg_pd_max_offset;
}

/**
 * wlan_vdev_mlme_set_he_spr_enabled() - set spatial reuse enabled
 * @vdev: VDEV object
 * @enable_he_spr: value to set
 *
 * API to set the spatial reuse enabled
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_he_spr_enabled(
						struct wlan_objmgr_vdev *vdev,
						bool enable_he_spr)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.he_spr_enabled = enable_he_spr;
}

/**
 * wlan_vdev_mlme_set_sr_disable_due_conc() - set spatial reuse disabled due
 *					      to concurrency
 * @vdev: VDEV object
 * @he_spr_disabled_due_conc: value to set
 *
 * API to set the spatial reuse disabled due to concurrency
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_set_sr_disable_due_conc(struct wlan_objmgr_vdev *vdev,
					    bool he_spr_disabled_due_conc)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.he_spr_disabled_due_conc =
						he_spr_disabled_due_conc;
}

/**
 * wlan_vdev_mlme_set_sr_prohibit_en() - set spatial reuse PD prohibit enabled
 *					 / disabled (HE_SIGA_Val15_Allowed)
 * @vdev: VDEV object
 * @sr_prohibit_enabled: True / False - PD Prohibit enabled / disabled
 *
 * API to set spatial reuse PD prohibit enabled / disabled
 *
 * Caller need to acquire lock with wlan_vdev_obj_lock()
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_set_sr_prohibit_en(struct wlan_objmgr_vdev *vdev,
				       bool sr_prohibit_enabled)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.sr_prohibit_enabled = sr_prohibit_enabled;
}

/**
 * wlan_vdev_mlme_set_srg_pd_offset() - set spatial reuse SRG pd max/min offset
 * @vdev: VDEV object
 * @srg_max_pd_offset: SRG max pd offset
 * @srg_min_pd_offset: SRG min pd offset
 *
 * API to set the spatial reuse SRG pd min max offset
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_set_srg_pd_offset(struct wlan_objmgr_vdev *vdev,
				      uint8_t srg_max_pd_offset,
				      uint8_t srg_min_pd_offset)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.he_spr_srg_max_pd_offset = srg_max_pd_offset;
	vdev_mlme->mgmt.generic.he_spr_srg_min_pd_offset = srg_min_pd_offset;
}

/**
 * wlan_vdev_mlme_get_srg_pd_offset() - get spatial reuse SRG pd min/max offset
 * @vdev: VDEV object
 * @srg_max_pd_offset: SRG max pd offset
 * @srg_min_pd_offset: SRG min pd offset
 *
 * API to set the spatial reuse SRG pd min max offset
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_get_srg_pd_offset(struct wlan_objmgr_vdev *vdev,
				      uint8_t *srg_max_pd_offset,
				      uint8_t *srg_min_pd_offset)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	*srg_max_pd_offset = vdev_mlme->mgmt.generic.he_spr_srg_max_pd_offset;
	*srg_min_pd_offset = vdev_mlme->mgmt.generic.he_spr_srg_min_pd_offset;
}

/**
 * wlan_vdev_mlme_set_srg_bss_color_bit_map() - set spatial reuse bss
 *					        color bitmap
 * @vdev: VDEV object
 * @srg_bss_color: SRG BSS color bitmap
 *
 * API to set the spatial reuse bss color bit map
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_set_srg_bss_color_bit_map(struct wlan_objmgr_vdev *vdev,
					      uint64_t srg_bss_color)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.srg_bss_color = srg_bss_color;
}

/**
 * wlan_vdev_mlme_set_srg_partial_bssid_bit_map() - set spatial reuse
 *						srg partial bitmap
 * @vdev: VDEV object
 * @srg_partial_bssid: SRG partial BSSID bitmap
 *
 * API to set the spatial reuse partial bssid bitmap
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_set_srg_partial_bssid_bit_map(struct wlan_objmgr_vdev *vdev,
						  uint64_t srg_partial_bssid)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.srg_partial_bssid = srg_partial_bssid;
}

/**
 * wlan_vdev_mlme_get_srg_bss_color_bit_map() - get spatial reuse bss
 *						colorbitmap
 * @vdev: VDEV object
 * @srg_bss_color: SRG BSS color bitmap
 *
 * API to get the spatial reuse bss color bit map
 *
 * Return: void
 */
static inline
void wlan_vdev_mlme_get_srg_bss_color_bit_map(struct wlan_objmgr_vdev *vdev,
					      uint64_t *srg_bss_color)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	*srg_bss_color = vdev_mlme->mgmt.generic.srg_bss_color;
}

/**
 * wlan_vdev_mlme_get_srg_partial_bssid_bit_map() - get spatial reuse
 *						    srg partial bitmap
 * @vdev: VDEV object
 * @srg_partial_bssid: SRG partial BSSID bitmap
 *
 * API to get the spatial reuse partial bssid bitmap
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_get_srg_partial_bssid_bit_map(struct wlan_objmgr_vdev *vdev,
					     uint64_t *srg_partial_bssid)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	*srg_partial_bssid = vdev_mlme->mgmt.generic.srg_partial_bssid;
}

/**
 * wlan_vdev_mlme_get_current_non_srg_pd_threshold() - get current non srg pd
 * threshold
 * @vdev: VDEV object
 * @non_srg_pd_threshold: NON-SRG pd threshold
 *
 * API to get non srg pd threshold
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_get_current_non_srg_pd_threshold(struct wlan_objmgr_vdev *vdev,
						int32_t *non_srg_pd_threshold)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	*non_srg_pd_threshold =
		vdev_mlme->mgmt.generic.he_curr_non_srg_pd_threshold;
}

/**
 * wlan_vdev_mlme_get_current_srg_pd_threshold() - get current srg pd threshold
 * @vdev: VDEV object
 * @srg_pd_threshold: SRG pd threshold
 *
 * API to get srg pd threshold
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_get_current_srg_pd_threshold(struct wlan_objmgr_vdev *vdev,
					    int32_t *srg_pd_threshold)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	*srg_pd_threshold =
		vdev_mlme->mgmt.generic.he_curr_srg_pd_threshold;
}

/**
 * wlan_vdev_mlme_set_current_non_srg_pd_threshold() - set current non srg pd
 * threshold
 * @vdev: VDEV object
 * @non_srg_pd_threshold: NON-SRG pd threshold
 *
 * API to set non srg pd threshold
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_set_current_non_srg_pd_threshold(struct wlan_objmgr_vdev *vdev,
						int32_t non_srg_pd_threshold)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.generic.he_curr_non_srg_pd_threshold =
						non_srg_pd_threshold;
}

/**
 * wlan_vdev_mlme_set_current_srg_pd_threshold() - set current srg pd threshold
 * @vdev: VDEV object
 * @srg_pd_threshold: SRG pd threshold
 *
 * API to set srg pd threshold
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_set_current_srg_pd_threshold(struct wlan_objmgr_vdev *vdev,
					    int32_t srg_pd_threshold)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;
	vdev_mlme->mgmt.generic.he_curr_srg_pd_threshold =
						srg_pd_threshold;
}

/**
 * wlan_vdev_mlme_set_pd_threshold_present() - set is PD threshold
 * present or not.
 * @vdev: VDEV object
 * @is_pd_threshold_present: is PD threshold present
 *
 * API to set pd threshold present flag
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_set_pd_threshold_present(struct wlan_objmgr_vdev *vdev,
					bool is_pd_threshold_present)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;
	vdev_mlme->mgmt.generic.is_pd_threshold_present =
						is_pd_threshold_present;
}

/**
 * wlan_vdev_mlme_get_pd_threshold_present() - get is PD threshold
 * present or not.
 * @vdev: VDEV object
 * @is_pd_threshold_present: is PD threshold present
 *
 * API to get pd threshold present flag
 *
 * Return: void
 */
static inline void
wlan_vdev_mlme_get_pd_threshold_present(struct wlan_objmgr_vdev *vdev,
					bool *is_pd_threshold_present)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		*is_pd_threshold_present = false;
		return;
	}
	*is_pd_threshold_present =
			vdev_mlme->mgmt.generic.is_pd_threshold_present;
}
#else
static inline uint8_t wlan_vdev_mlme_get_sr_ctrl(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline uint8_t wlan_vdev_mlme_get_non_srg_pd_offset(
						struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline bool wlan_vdev_mlme_get_he_spr_enabled(
						struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline
bool wlan_vdev_mlme_is_sr_disable_due_conc(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool wlan_vdev_mlme_is_sr_prohibit_en(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void wlan_vdev_mlme_set_sr_ctrl(struct wlan_objmgr_vdev *vdev,
					      uint8_t sr_ctrl)
{
}

static inline void
wlan_vdev_mlme_set_non_srg_pd_offset(struct wlan_objmgr_vdev *vdev,
				     uint8_t non_srg_pd_max_offset)
{
}

static inline void wlan_vdev_mlme_set_he_spr_enabled(
						struct wlan_objmgr_vdev *vdev,
						bool enable_he_spr)
{
}

static inline
void wlan_vdev_mlme_set_sr_disable_due_conc(struct wlan_objmgr_vdev *vdev,
					    bool he_spr_disabled_due_conc)
{
}

static inline
void wlan_vdev_mlme_set_sr_prohibit_en(struct wlan_objmgr_vdev *vdev,
				       bool sr_prohibit_enabled)
{
}
#endif
#else
static inline void wlan_vdev_mlme_set_he_mcs_12_13_map(
				struct wlan_objmgr_vdev *vdev,
				uint16_t he_mcs_12_13_map)
{
}

static inline uint16_t wlan_vdev_mlme_get_he_mcs_12_13_map(
				struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

#endif

/**
 * wlan_vdev_mlme_set_aid_mgr() - set aid mgr
 * @vdev: VDEV object
 * @aid_mgr: AID mgr
 *
 * API to set AID mgr in VDEV MLME cmpt object
 *
 * Return: void
 */
static inline void wlan_vdev_mlme_set_aid_mgr(
				struct wlan_objmgr_vdev *vdev,
				struct wlan_vdev_aid_mgr *aid_mgr)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return;

	vdev_mlme->mgmt.ap.aid_mgr = aid_mgr;
}

/**
 * wlan_vdev_mlme_get_aid_mgr() - get aid mgr
 * @vdev: VDEV object
 *
 * API to get AID mgr in VDEV MLME cmpt object
 *
 * Return: aid_mgr
 */
static inline struct wlan_vdev_aid_mgr *wlan_vdev_mlme_get_aid_mgr(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return NULL;

	return vdev_mlme->mgmt.ap.aid_mgr;
}

#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
/**
 * vdev_mgr_cdp_vdev_attach() - MLME API to attach CDP vdev
 * @mlme_obj: pointer to vdev_mlme_obj
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS vdev_mgr_cdp_vdev_attach(struct vdev_mlme_obj *mlme_obj);

/**
 * vdev_mgr_cdp_vdev_detach() - MLME API to detach CDP vdev
 * @mlme_obj: pointer to vdev_mlme_obj
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS vdev_mgr_cdp_vdev_detach(struct vdev_mlme_obj *mlme_obj);
#endif
#endif
