/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#if !defined(__LIM_SESSION_H)
#define __LIM_SESSION_H

#include "wlan_cm_public_struct.h"
/**=========================================================================

   \file  lim_session.h

   \brief prototype for lim Session related APIs

   \author Sunit Bhatia

   ========================================================================*/

/* Master Structure: This will be part of PE Session Entry */
typedef struct sPowersaveoffloadInfo {
	uint8_t bcnmiss;
} tPowersaveoffloadInfo, tpPowersaveoffloadInfo;

struct comeback_timer_info {
	struct mac_context *mac;
	uint8_t vdev_id;
	uint8_t retried;
	tLimMlmStates lim_prev_mlm_state;  /* Previous MLM State */
	tLimMlmStates lim_mlm_state;       /* MLM State */
};
/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
   Preprocessor definitions and constants
   ------------------------------------------------------------------------*/
#define SCH_PROTECTION_RESET_TIME 4000

/*--------------------------------------------------------------------------
   Type declarations
   ------------------------------------------------------------------------*/
typedef struct {
	tSirMacBeaconInterval beaconInterval;
	uint8_t fShortPreamble;
	uint8_t llaCoexist;
	uint8_t llbCoexist;
	uint8_t llgCoexist;
	uint8_t ht20Coexist;
	uint8_t llnNonGFCoexist;
	uint8_t fRIFSMode;
	uint8_t fLsigTXOPProtectionFullSupport;
	uint8_t gHTObssMode;
} tBeaconParams, *tpBeaconParams;

typedef struct join_params {
	uint16_t prot_status_code;
	uint16_t pe_session_id;
	tSirResultCodes result_code;
} join_params;

struct reassoc_params {
	uint16_t prot_status_code;
	tSirResultCodes result_code;
	struct pe_session *session;
};

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
#define MAX_BSS_COLOR_VALUE 63
#define TIME_BEACON_NOT_UPDATED 30000
#define BSS_COLOR_SWITCH_COUNTDOWN 5
#define OBSS_COLOR_COLLISION_DETECTION_STA_PERIOD_MS 120000
#define OBSS_COLOR_COLLISION_DETECTION_AP_PERIOD_MS 120000
/*
 * Have OBSS scan duration as 1200 seconds(20 minutes) when there is an active
 * NDP to avoid glitches during NDP traffic due the scan.
 */
#define OBSS_COLOR_COLLISION_DETECTION_NDP_PERIOD_MS 1200000
#define OBSS_COLOR_COLLISION_SCAN_PERIOD_MS 200
#define OBSS_COLOR_COLLISION_FREE_SLOT_EXPIRY_MS 50000
struct bss_color_info {
	qdf_time_t timestamp;
	uint64_t seen_count;
};
#endif

/**
 * struct obss_detection_cfg - current obss detection cfg set to firmware
 * @obss_11b_ap_detect_mode: detection mode for 11b access point.
 * @obss_11b_sta_detect_mode: detection mode for 11b station.
 * @obss_11g_ap_detect_mode: detection mode for 11g access point.
 * @obss_11a_detect_mode: detection mode for 11a access point.
 * @obss_ht_legacy_detect_mode: detection mode for ht ap with legacy mode.
 * @obss_ht_mixed_detect_mode: detection mode for ht ap with mixed mode.
 * @obss_ht_20mhz_detect_mode: detection mode for ht ap with 20mhz mode.
 */
struct obss_detection_cfg {
	uint8_t obss_11b_ap_detect_mode;
	uint8_t obss_11b_sta_detect_mode;
	uint8_t obss_11g_ap_detect_mode;
	uint8_t obss_11a_detect_mode;
	uint8_t obss_ht_legacy_detect_mode;
	uint8_t obss_ht_mixed_detect_mode;
	uint8_t obss_ht_20mhz_detect_mode;
};

#define ADAPTIVE_11R_STA_IE_LEN   0x0B
#define ADAPTIVE_11R_STA_OUI      "\x00\x00\x0f\x22"
#define ADAPTIVE_11R_OUI_LEN      0x04
#define ADAPTIVE_11R_OUI_SUBTYPE  0x00
#define ADAPTIVE_11R_OUI_VERSION  0x01
#define ADAPTIVE_11R_DATA_LEN      0x04
#define ADAPTIVE_11R_OUI_DATA     "\x00\x00\x00\x01"

#ifdef WLAN_FEATURE_11BE_MLO
#define WLAN_STA_PROFILE_MAX_LEN 514
#define WLAN_MLO_IE_COM_MAX_LEN 257

/**
 * struct wlan_mlo_sta_profile - Per STA profile structure
 * @num_data: the length of data
 * @data: the Per STA profile subelement data. Subelement ID + LEN + others,
 * if num_data more than 257, it includes the frag IE for tx; it does not
 * include the frag IE since it has been skipped when store the IE.
 */
struct wlan_mlo_sta_profile {
	uint16_t num_data;
	uint8_t data[WLAN_STA_PROFILE_MAX_LEN];
};

/**
 * struct medium_sync_delay - medium sync delay info
 * @medium_sync_duration: medium sync duration
 * @medium_sync_ofdm_ed_thresh: medium sync OFDM ED threshold
 * @medium_sync_max_txop_num: medium sync max txop num
 */
struct medium_sync_delay {
	uint16_t medium_sync_duration:8;
	uint16_t medium_sync_ofdm_ed_thresh:4;
	uint16_t medium_sync_max_txop_num:4;
};

/**
 * struct eml_capabilities - EML capability info
 * @emlsr_support: EMLSR support
 * @emlsr_padding_delay: EMLSR padding delay
 * @emlsr_transition_delay: EMLSR transition delay
 * @emlmr_support: EMLSR support
 * @emlmr_delay: EMLSR delay
 * @transition_timeout: transition timeout
 * @reserved: reserve
 */
struct eml_capabilities {
	uint16_t emlsr_support:1;
	uint16_t emlsr_padding_delay:3;
	uint16_t emlsr_transition_delay:3;
	uint16_t emlmr_support:1;
	uint16_t emlmr_delay:3;
	uint16_t transition_timeout:4;
	uint16_t reserved:1;
};

/**
 * struct mld_capab_and_op - MLD capability and operations info
 * @max_simultaneous_link_num: MAX simultaneous link num
 * @srs_support: SRS support
 * @tid_link_map_supported: TID link map support
 * @str_freq_separation: STR freq separation
 * @aar_support: AAR support
 * @reserved: reserve
 */
struct mld_capab_and_op {
	uint16_t max_simultaneous_link_num:4;
	uint16_t srs_support:1;
	uint16_t tid_link_map_supported:2;
	uint16_t str_freq_separation:5;
	uint16_t aar_support:1;
	uint16_t reserved:3;
};

/**
 * struct ext_mld_capab_and_op - EXT MLD capability and operations info
 * @op_parameter_update_support: operation parameter update support
 * @rec_max_simultaneous_links: recommended max simultaneous links
 * @reserved: reserved
 */
struct ext_mld_capab_and_op {
	uint16_t op_parameter_update_support:1;
	uint16_t rec_max_simultaneous_links:3;
	uint16_t reserved:11;
};

/**
 * struct wlan_mlo_ie - wlan ML IE info
 * @type: the variant of the ML IE
 * @reserved: reserved
 * @link_id_info_present: the present flag of link id info
 * @bss_param_change_cnt_present: the present flag of bss prarm change cnt
 * @medium_sync_delay_info_present: the present flag of medium sync delay info
 * @eml_capab_present: the present flag of EML capability
 * @mld_capab_and_op_present: the present flag of MLD capability and operation
 * @mld_id_present: the present flag of MLD ID
 * @ext_mld_capab_and_op_present: Extended MLD Capabilities And
 *                                Operations Present
 * @reserved_1: reserved
 * @common_info_length: common info length
 * @mld_mac_addr: MLD mac address
 * @link_id: link id
 * @bss_param_change_count: bss param change count
 * @medium_sync_delay_info: structure of medium_sync_delay
 * @eml_capabilities_info: structure of eml_capabilities
 * @mld_capab_and_op_info: structure of mld_capabilities and operations
 * @mld_id_info: MLD ID
 * @ext_mld_capab_and_op_info: structure of ext_mld_capab_and operations
 * @num_sta_profile: the number of sta profile
 * @sta_profile: structure of wlan_mlo_sta_profile
 * @num_data: the length of data
 * @data: the ML IE data, includes element ID + length + extension element ID +
 * multi-link control and common info.
 */
struct wlan_mlo_ie {
	uint16_t type:3;
	uint16_t reserved:1;
	uint16_t link_id_info_present:1;
	uint16_t bss_param_change_cnt_present:1;
	uint16_t medium_sync_delay_info_present:1;
	uint16_t eml_capab_present:1;
	uint16_t mld_capab_and_op_present: 1;
	uint16_t mld_id_present: 1;
	uint16_t ext_mld_capab_and_op_present: 1;
	uint16_t reserved_1:5;
	uint8_t common_info_length;
	uint8_t mld_mac_addr[6];
	uint8_t link_id;
	uint8_t bss_param_change_count;
	struct medium_sync_delay medium_sync_delay_info;
	struct eml_capabilities eml_capabilities_info;
	struct mld_capab_and_op mld_capab_and_op_info;
	uint8_t mld_id_info;
	struct ext_mld_capab_and_op ext_mld_capab_and_op_info;
	uint16_t num_sta_profile;
	struct wlan_mlo_sta_profile sta_profile[WLAN_MLO_MAX_VDEVS];
	uint16_t num_data;
	uint8_t data[WLAN_MLO_IE_COM_MAX_LEN];
};

/**
 * struct mlo_link_ie - IE per link to populate mlo ie
 * @link_ds: DS IE
 * @link_edca: ecsa IE
 * @link_wmm_params: wmm params IE
 * @link_wmm_caps: wmm caps IE
 * @link_csa: csa IE
 * @link_ecsa:ecsa IE
 * @link_swt_time: switch time IE
 * @link_quiet: quiet IE
 * @link_ht_cap: ht cap IE
 * @link_ht_info: ht info IE
 * @link_cap: link caps IE
 * @link_ext_cap: link extend cap IE
 * @link_vht_cap: vht cap IE
 * @link_vht_op: vht op IE
 * @link_qcn_ie: qcn IE
 * @link_he_cap: he cap IE
 * @link_he_op: he op IE
 * @link_he_6ghz_band_cap: 6G band cap IE
 * @link_eht_cap: eht cap IE
 * @link_eht_op: eht op IE
 * @max_chan_swt_time: MLOTD
 * @bss_param_change_cnt: bss param change count
 */
struct mlo_link_ie {
	tDot11fIEDSParams                    link_ds;
	tDot11fIEEDCAParamSet                link_edca;
	tDot11fIEWMMParams                   link_wmm_params;
	tDot11fIEWMMCaps                     link_wmm_caps;
	tDot11fIEChanSwitchAnn               link_csa;
	tDot11fIEext_chan_switch_ann         link_ecsa;
	tDot11fIEmax_chan_switch_time        link_swt_time;
	tDot11fIEQuiet                       link_quiet;
	tDot11fIEHTCaps                      link_ht_cap;
	tDot11fIEHTInfo                      link_ht_info;
	tDot11fFfCapabilities                link_cap;
	tDot11fIEExtCap                      link_ext_cap;
	tDot11fIEVHTCaps                     link_vht_cap;
	tDot11fIEVHTOperation                link_vht_op;
	tDot11fIEqcn_ie                      link_qcn_ie;
	tDot11fIEhe_cap                      link_he_cap;
	tDot11fIEhe_op                       link_he_op;
	tDot11fIEhe_6ghz_band_cap            link_he_6ghz_band_cap;
	tDot11fIEeht_cap                     link_eht_cap;
	tDot11fIEeht_op                      link_eht_op;
	uint32_t                             max_chan_swt_time;
	uint8_t                              bss_param_change_cnt;
};

/**
 * struct mlo_link_ie_info - information per link to populate mlo ie
 * @upt_bcn_mlo_ie: notify partner links to update their mlo ie of bcn temp
 * @bss_param_change: bss param changed
 * @bcn_tmpl_exist: bcn template is generated or not
 * @link_ie: IEs which will be used for generating partner mlo IE
 */
struct mlo_link_ie_info {
	bool upt_bcn_mlo_ie;
	bool bss_param_change;
	bool bcn_tmpl_exist;
	struct mlo_link_ie link_ie;
};

/**
 * struct wlan_mlo_ie_info - struct for mlo IE information
 * @mld_mac_addr: MLD MAC address
 * @common_info_length: Common Info Length
 * @reserved_1: reserved bits
 * @mld_id_present: MLD ID present
 * @mld_capab_and_op_present: MLD capability and operations present
 * @eml_capab_present: EML capability present
 * @medium_sync_delay_info_present: Medium sync delay information present
 * @bss_param_change_cnt_present: BSS parameter change count present
 * @link_id_info_present: Link ID information present
 * @ext_mld_capab_and_op_present: Extended MLD Capabilities And
 *                                Operations Present
 * @reserved: reserved bit
 * @type: Type bits
 */
struct wlan_mlo_ie_info {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t mld_mac_addr[6];
	uint8_t common_info_length;
	uint16_t reserved_1:5;
	uint16_t ext_mld_capab_and_op_present:1;
	uint16_t mld_id_present:1;
	uint16_t mld_capab_and_op_present:1;
	uint16_t eml_capab_present:1;
	uint16_t medium_sync_delay_info_present:1;
	uint16_t bss_param_change_cnt_present:1;
	uint16_t link_id_info_present:1;
	uint16_t reserved:1;
	uint16_t type:3;
#else
	uint16_t type:3;
	uint16_t reserved:1;
	uint16_t link_id_info_present:1;
	uint16_t bss_param_change_cnt_present:1;
	uint16_t medium_sync_delay_info_present:1;
	uint16_t eml_capab_present:1;
	uint16_t mld_capab_and_op_present:1;
	uint16_t mld_id_present:1;
	uint16_t ext_mld_capab_and_op_present:1;
	uint16_t reserved_1:5;
	uint8_t common_info_length;
	uint8_t mld_mac_addr[6];
#endif
} qdf_packed;

#endif

/**
 * struct pe_session - per-vdev PE context
 * @available: true if the entry is available, false if it is in use
 * @cm_id:
 * @peSessionId: unique ID assigned to the entry
 * @smeSessionId: ID of the session (legacy nomenclature)
 * @vdev_id: ID of the vdev for which this entry is applicable
 * @vdev: the actual vdev for which this entry is applicable
 * @bssId: BSSID of the session
 * @self_mac_addr: self MAC address
 * * In AP role: BSSID and self_mac_addr will be the same.
 * * In STA role: they will be different
 * @ssId:
 * @valid:
 * @limMlmState: MLM State
 * @limPrevMlmState: Previous MLM State
 * @limSmeState: SME State
 * @limPrevSmeState: Previous SME State
 * @limSystemRole:
 * @bssType:
 * @nwType:
 * @pLimStartBssReq:
 * @lim_join_req: handle to sme join req
 * @pLimReAssocReq: handle to sme reassoc req
 * @pLimMlmJoinReq: handle to MLM join Req
 * @pLimMlmReassocRetryReq: keep reasoc req for retry
 * @pLimMlmReassocReq: handle to MLM reassoc Req
 * @channelChangeReasonCode:
 * @dot11mode:
 * @htCapability:
 * @connected_akm: AKM of current connection
 * @htSupportedChannelWidthSet: HT Supported Channel Width Set:
 * * 0 - 20MHz
 * * 1 - 40MHz
 * @htRecommendedTxWidthSet: Recommended Tx Width Set:
 * * 0 - use 20 MHz channel (control channel)
 * * 1 - use channel width enabled under Supported Channel Width Set
 * @htSecondaryChannelOffset: Identifies the 40 MHz extension channel
 * @limRFBand:
 * @limCurrentAuthType:
 * @limCurrentBssCaps:
 * @limCurrentBssQosCaps:
 * @limSentCapsChangeNtf:
 * @limAID:
 * @limReAssocbssId:
 * @lim_reassoc_chan_freq:
 * @reAssocHtSupportedChannelWidthSet:
 * @reAssocHtRecommendedTxWidthSet:
 * @reAssocHtSecondaryChannelOffset:
 * @limReassocSSID:
 * @limReassocBssCaps:
 * @limReassocBssQosCaps:
 * @limAssocResponseData: Assoc or ReAssoc Response Data/Frame
 * @statypeForBss: to know session is for PEER or SELF
 * @shortSlotTimeSupported:
 * @dtimPeriod:
 * @rateSet:
 * @extRateSet:
 * @htOperMode:
 * @curr_op_freq:
 * @curr_req_chan_freq:
 * @LimRxedBeaconCntDuringHB:
 * @lastBeaconTimeStamp: Time stamp of the last beacon received from the BSS
 *                       to which STA is connected.
 * @currentBssBeaconCnt: RX Beacon count for the current BSS to which STA
 *                       is connected.
 * @bcon_dtim_period: Beacon DTIM period
 * @bcnLen: Length of @beacon
 * @beacon: Used to store last beacon / probe response before assoc.
 * @assocReqLen: Length of @assoc_req
 * @assoc_req: Used to store association request frame
 * @assocRspLen: Length of @assocRsp
 * @assocRsp: Used to store association response received while associating
 * @dph:
 * @parsedAssocReq: Used to store parsed assoc req from various requesting
 *                  station
 * @RICDataLen: Length of @ricData
 * @ricData: Used to store the Ric data received in the assoc response
 * @tspecLen: Length of @tspecIes
 * @tspecIes: Used to store the TSPEC IEs received in the assoc response
 * @encryptType:
 * @gLimProtectionControl: used for 11n protection
 * @gHTNonGFDevicesPresent:
 * @cfgProtection: protection related config cache
 * @gLim11bParams: Number of legacy STAs associated
 * @gLim11aParams: Number of 11A STAs associated
 * @gLim11gParams: Number of non-ht non-legacy STAs associated
 * @gLimNonGfParams: Number of nonGf STA associated
 * @gLimHt20Params: Number of HT 20 STAs associated
 * @gLimLsigTxopParams: Number of Lsig Txop not supported STAs associated
 * @gLimNoShortParams: Number of STAs that do not support short preamble
 * @gLimNoShortSlotParams: Number of STAs that do not support short slot time
 * @gLimOlbcParams: OLBC parameters
 * @gLimOverlap11gParams: OLBC parameters
 * @gLimOverlap11aParams:
 * @gLimOverlapHt20Params:
 * @gLimOverlapNonGfParams:
 * @protStaCache: cache for each overlap
 * @privacy:
 * @authType:
 * @WEPKeyMaterial:
 * @wmm_params:
 * @gStartBssRSNIe:
 * @gStartBssWPAIe:
 * @APWPSIEs:
 * @apUapsdEnable:
 * @pAPWPSPBCSession:
 * @DefProbeRspIeBitmap:
 * @proxyProbeRspEn:
 * @probeRespFrame:
 * @ssidHidden:
 * @fwdWPSPBCProbeReq:
 * @wps_state:
 * @wps_registration:
 * @limQosEnabled: Is 802.11e QoS enabled
 * @limWmeEnabled: Is WME enabled
 * @limWsmEnabled: Is WSM enabled
 * @limHcfEnabled: Is HCF enabled
 * @limRmfEnabled: Is 802.11w RMF enabled
 * @lim11hEnable: Is 802.11h enabled
 * @maxTxPower: Max transmit power, the minimum of Regulatory and local
 *              power constraint)
 * @min_11h_pwr:
 * @max_11h_pwr:
 * @opmode:
 * @txMgmtPower:
 * @is11Rconnection:
 * @is_adaptive_11r_connection: flag to check if we are connecting
 * @isESEconnection:
 * @eseContext:
 * @isFastTransitionEnabled:
 * @isFastRoamIniFeatureEnabled:
 * @p2pGoPsUpdate:
 * @defaultAuthFailureTimeout:
 * @gLimEdcaParams: These EDCA parameters are used locally on AP or STA.
 * If STA, then these are values taken from the Assoc Rsp when associating,
 * or Beacons/Probe Response after association.  If AP, then these are
 * values originally set locally on AP.
 * @gLimEdcaParamsBC: These EDCA parameters are use by AP to broadcast
 * to other STATIONs in the BSS.
 * @gLimEdcaParamsActive: These EDCA parameters are what's actively being
 * used on station. Specific AC values may be downgraded depending on
 * admission control for that particular AC.
 * @gLimEdcaParamSetCount:
 * @beaconParams:
 * @vhtCapability:
 * @gLimOperatingMode:
 * @vhtCapabilityPresentInBeacon:
 * @ch_center_freq_seg0: center freq number as advertised OTA
 * @ch_width:    Session max channel width
 * @ap_ch_width: AP advertised channel width
 * @puncture_bitmap:
 * @ch_center_freq_seg1:
 * @enableVhtpAid:
 * @enableVhtGid:
 * @gLimWiderBWChannelSwitch:
 * @enableAmpduPs:
 * @send_smps_action:
 * @spectrumMgtEnabled:
 * @gLimSpecMgmt:
 * @gLimChannelSwitch: CB Primary/Secondary Channel Switch Info
 * @gLimPhyMode:
 * @txLdpcIniFeatureEnabled:
 * @gpLimPeerIdxpool: free peer index pool. A non-zero value indicates that
 *                    peer index is available for assignment.
 * @freePeerIdxHead:
 * @freePeerIdxTail:
 * @gLimNumOfCurrentSTAs:
 * @peerAIDBitmap:
 * @tdls_send_set_state_disable:
 * @fWaitForProbeRsp:
 * @fIgnoreCapsChange:
 * @fDeauthReceived:
 * @rssi:
 * @max_amsdu_num:
 * @ht_config:
 * @vht_config:
 * @gLimCurrentBssUapsd:
 * @gUapsdPerAcBitmask: Used on STA, this is a static UAPSD mask setting
 *                      derived from SME_JOIN_REQ and SME_REASSOC_REQ. If a
 *                      particular AC bit is set, it means the AC is both
 *                      trigger enabled and delivery enabled.
 * @gUapsdPerAcTriggerEnableMask: Used on STA, this is a dynamic UPASD mask
 *                                setting derived from AddTS Rsp and DelTS
 *                                frame. If a particular AC bit is set, it
 *                                means AC is trigger enabled.
 * @gUapsdPerAcDeliveryEnableMask: Used on STA, dynamic UPASD mask setting
 *                                 derived from AddTS Rsp and DelTs frame. If
 *                                 a particular AC bit is set, it means AC is
 *                                 delivery enabled.
 * @csaOffloadEnable: Flag to skip CSA IE processing when CSA offload is
 *                    enabled.
 * @gAcAdmitMask: Used on STA for AC downgrade. This is a dynamic mask
 *                setting which keep tracks of ACs being admitted.
 *                If bit is set to 0: That particular AC is not admitted
 *                If bit is set to 1: That particular AC is admitted
 * @pmmOffloadInfo: Power Save Off load Parameters
 * @smpsMode: SMPS mode
 * @chainMask:
 * @dfsIncludeChanSwIe: Flag to indicate Chan Sw announcement is required
 * @dfsIncludeChanWrapperIe: Flag to indicate Chan Wrapper Element is required
 * @bw_update_include_ch_sw_ie: Flag to indicate chan switch Element is required
 *                              due to bandwidth update
 * @cc_switch_mode:
 * @isCiscoVendorAP:
 * @add_ie_params:
 * @pSchProbeRspTemplate:
 * @pSchBeaconFrameBegin: Beginning portion of the beacon frame to be written
 *                        to TFP
 * @pSchBeaconFrameEnd: Trailing portion of the beacon frame to be written
 *                      to TFP
 * @schBeaconOffsetBegin: Size of the beginning portion
 * @schBeaconOffsetEnd: Size of the trailing portion
 * @isOSENConnection:
 * @QosMapSet: DSCP to UP mapping for HS 2.0
 * @bRoamSynchInProgress:
 * @ftPEContext: Fast Transition (FT) Context
 * @isNonRoamReassoc:
 * @pmf_retry_timer:
 * @pmf_retry_timer_info:
 * @protection_fields_reset_timer: timer for resetting protection fields
 *                                 at regular intervals
 * @ap_ecsa_timer: timer to decrement CSA/ECSA count
 * @ap_ecsa_wakelock: wakelock to complete CSA operation.
 * @ap_ecsa_runtime_lock: runtime lock to complete SAP CSA operation.
 *                        to Adaptive 11R network
 * @mac_ctx: MAC context
 * @old_protection_state: variable to store state of various protection
 *                        struct like gLimOlbcParams, gLimOverlap11gParams,
 *                         gLimOverlapHt20Params etc
 * @prev_ap_bssid:
 * @sap_advertise_avoid_ch_ie: tells if Q2Q IE, from another MDM device in
 *                             AP MCC mode was received
 * @is_ese_version_ie_present:
 * @sap_dot11mc:
 * @is_vendor_specific_vhtcaps:
 * @vendor_specific_vht_ie_sub_type:
 * @vendor_vht_sap:
 * @hs20vendor_ie: HS 2.0 Indication
 * @country_info_present: flag to indicate country code in beacon
 * @nss:
 * @nss_forced_1x1:
 * @add_bss_failed:
 * @obss_ht40_scanparam: OBSS Scan IE Parameters
 * @vdev_nss:
 * @supported_nss_1x1: Supported NSS is intersection of self and peer NSS
 * @is_ext_caps_present:
 * @beacon_tx_rate:
 * @access_policy_vendor_ie:
 * @access_policy:
 * @send_p2p_conf_frame:
 * @process_ho_fail:
 * @lim_non_ecsa_cap_num: Number of STAs that do not support ECSA capability
 * @he_capable:
 * @he_config:
 * @he_op:
 * @he_sta_obsspd:
 * @he_6ghz_band:
 * @he_bss_color_change:
 * @bss_color_info:
 * @bss_color_changing:
 * @deauth_retry:
 * @enable_bcast_probe_rsp:
 * @ht_client_cnt:
 * @ch_switch_in_progress:
 * @he_with_wep_tkip:
 * @fils_info:
 * @prev_auth_seq_num: Sequence number of previously received auth frame to
 *                     detect duplicate frames.
 * @prev_auth_mac_addr: mac_addr of the sta correspond to @prev_auth_seq_num
 * @obss_offload_cfg:
 * @current_obss_detection:
 * @is_session_obss_offload_enabled:
 * @is_obss_reset_timer_initialized:
 * @sae_pmk_cached:
 * @recvd_deauth_while_roaming:
 * @recvd_disassoc_while_roaming:
 * @deauth_disassoc_rc:
 * @obss_color_collision_dec_evt:
 * @is_session_obss_color_collision_det_enabled:
 * @ap_mu_edca_params:
 * @mu_edca_present:
 * @def_max_tx_pwr:
 * @active_ba_64_session:
 * @is_mbssid_enabled:
 * @peer_twt_requestor:
 * @peer_twt_responder:
 * @enable_session_twt_support:
 * @cac_duration_ms:
 * @stop_bss_reason:
 * @prot_status_code:
 * @result_code:
 * @dfs_regdomain:
 * @ap_defined_power_type_6g: 6 GHz power type advertised by AP
 * @best_6g_power_type: best 6 GHz power type
 * @sta_follows_sap_power:
 * @eht_capable:
 * @eht_config:
 * @eht_op:
 * @mlo_link_info:
 * @ml_partner_info:
 * @mlo_ie_total_len:
 * @mlo_ie:
 * @user_edca_set:
 * @is_oui_auth_assoc_6mbps_2ghz_enable: send auth/assoc req with 6 Mbps rate
 * @is_unexpected_peer_error: true if unexpected peer error
 * on 2.4 GHz
 * @join_probe_cnt: join probe request count
 */
struct pe_session {
	uint8_t available;
	wlan_cm_id cm_id;
	uint16_t peSessionId;
	union {
		uint8_t smeSessionId;
		uint8_t vdev_id;
	};
	struct wlan_objmgr_vdev *vdev;

	tSirMacAddr bssId;
	tSirMacAddr self_mac_addr;
	tSirMacSSid ssId;
	uint8_t valid;
	tLimMlmStates limMlmState;
	tLimMlmStates limPrevMlmState;
	tLimSmeStates limSmeState;
	tLimSmeStates limPrevSmeState;
	tLimSystemRole limSystemRole;
	enum bss_type bssType;
	tSirNwType nwType;
	struct start_bss_config *pLimStartBssReq;
	struct join_req *lim_join_req;
	struct join_req *pLimReAssocReq;
	tpLimMlmJoinReq pLimMlmJoinReq;
	void *pLimMlmReassocRetryReq;
	void *pLimMlmReassocReq;
	uint16_t channelChangeReasonCode;
	uint8_t dot11mode;
	uint8_t htCapability;
	enum ani_akm_type connected_akm;

	uint8_t htSupportedChannelWidthSet;
	uint8_t htRecommendedTxWidthSet;
	ePhyChanBondState htSecondaryChannelOffset;
	enum reg_wifi_band limRFBand;

	tAniAuthType limCurrentAuthType;
	uint16_t limCurrentBssCaps;
	uint8_t limCurrentBssQosCaps;
	uint8_t limSentCapsChangeNtf;
	uint16_t limAID;

	tSirMacAddr limReAssocbssId;
	uint32_t lim_reassoc_chan_freq;
	uint8_t reAssocHtSupportedChannelWidthSet;
	uint8_t reAssocHtRecommendedTxWidthSet;
	ePhyChanBondState reAssocHtSecondaryChannelOffset;
	tSirMacSSid limReassocSSID;
	uint16_t limReassocBssCaps;
	uint8_t limReassocBssQosCaps;

	void *limAssocResponseData;

	uint16_t statypeForBss;
	uint8_t shortSlotTimeSupported;
	uint8_t dtimPeriod;
	tSirMacRateSet rateSet;
	tSirMacRateSet extRateSet;
	tSirMacHTOperatingMode htOperMode;
	qdf_freq_t curr_op_freq;
	uint32_t curr_req_chan_freq;
	uint8_t LimRxedBeaconCntDuringHB;
	uint64_t lastBeaconTimeStamp;
	uint32_t currentBssBeaconCnt;
	uint8_t bcon_dtim_period;

	uint32_t bcnLen;
	uint8_t *beacon;

	uint32_t assocReqLen;
	uint8_t *assoc_req;

	uint32_t assocRspLen;
	uint8_t *assocRsp;
	tAniSirDph dph;
	void **parsedAssocReq;
	uint32_t RICDataLen;
	uint8_t *ricData;
#ifdef FEATURE_WLAN_ESE
	uint32_t tspecLen;
	uint8_t *tspecIes;
#endif
	uint32_t encryptType;

	uint8_t gLimProtectionControl;

	uint8_t gHTNonGFDevicesPresent;

	tCfgProtection cfgProtection;
	tLimProtStaParams gLim11bParams;
	tLimProtStaParams gLim11aParams;
	tLimProtStaParams gLim11gParams;
	tLimProtStaParams gLimNonGfParams;
	tLimProtStaParams gLimHt20Params;
	tLimProtStaParams gLimLsigTxopParams;
	tLimNoShortParams gLimNoShortParams;
	tLimNoShortSlotParams gLimNoShortSlotParams;
	tLimProtStaParams gLimOlbcParams;
	tLimProtStaParams gLimOverlap11gParams;
	tLimProtStaParams gLimOverlap11aParams;
	tLimProtStaParams gLimOverlapHt20Params;
	tLimProtStaParams gLimOverlapNonGfParams;

	tCacheParams protStaCache[LIM_PROT_STA_CACHE_SIZE];

	uint8_t privacy;
	tAniAuthType authType;
	tDot11fIEWMMParams wmm_params;
	tDot11fIERSN gStartBssRSNIe;
	tDot11fIEWPA gStartBssWPAIe;
	tSirAPWPSIEs APWPSIEs;
	uint8_t apUapsdEnable;
	tSirWPSPBCSession *pAPWPSPBCSession;
	uint32_t DefProbeRspIeBitmap[8];
	uint32_t proxyProbeRspEn;
	tDot11fProbeResponse probeRespFrame;
	uint8_t ssidHidden;
	bool fwdWPSPBCProbeReq;
	uint8_t wps_state;
	bool wps_registration;

	uint8_t limQosEnabled:1;
	uint8_t limWmeEnabled:1;
	uint8_t limWsmEnabled:1;
	uint8_t limHcfEnabled:1;
	uint8_t limRmfEnabled:1;
	uint32_t lim11hEnable;

	int8_t maxTxPower;
	int8_t min_11h_pwr;
	int8_t max_11h_pwr;
	enum QDF_OPMODE opmode;
	int8_t txMgmtPower;
	bool is11Rconnection;
	bool is_adaptive_11r_connection;

#ifdef FEATURE_WLAN_ESE
	tEsePEContext eseContext;
#endif
	tSirP2PNoaAttr p2pGoPsUpdate;
	uint32_t defaultAuthFailureTimeout;

	tSirMacEdcaParamRecord gLimEdcaParams[QCA_WLAN_AC_ALL];
	tSirMacEdcaParamRecord gLimEdcaParamsBC[QCA_WLAN_AC_ALL];
	tSirMacEdcaParamRecord gLimEdcaParamsActive[QCA_WLAN_AC_ALL];

	uint8_t gLimEdcaParamSetCount;

	tBeaconParams beaconParams;
	uint8_t vhtCapability;
	tLimOperatingModeInfo gLimOperatingMode;
	uint8_t vhtCapabilityPresentInBeacon;
	uint8_t ch_center_freq_seg0;
	enum phy_ch_width ch_width;
	enum phy_ch_width ap_ch_width;
#ifdef WLAN_FEATURE_11BE
	uint16_t puncture_bitmap;
#endif
	uint8_t ch_center_freq_seg1;
	uint8_t enableVhtpAid;
	uint8_t enableVhtGid;
	tLimWiderBWChannelSwitchInfo gLimWiderBWChannelSwitch;
	uint8_t enableAmpduPs;
	bool send_smps_action;
	uint8_t spectrumMgtEnabled;

	tLimSpecMgmtInfo gLimSpecMgmt;
	tLimChannelSwitchInfo gLimChannelSwitch;

	uint32_t gLimPhyMode;
	uint8_t txLdpcIniFeatureEnabled;
	uint8_t *gpLimPeerIdxpool;
	uint8_t freePeerIdxHead;
	uint8_t freePeerIdxTail;
	uint16_t gLimNumOfCurrentSTAs;
#ifdef FEATURE_WLAN_TDLS
	uint32_t peerAIDBitmap[2];
	bool tdls_send_set_state_disable;
#endif
	bool fWaitForProbeRsp;
	bool fIgnoreCapsChange;
	bool fDeauthReceived;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
	int8_t rssi;
#endif
	uint8_t max_amsdu_num;
	struct mlme_ht_capabilities_info ht_config;
	struct wlan_vht_config vht_config;
	uint8_t gLimCurrentBssUapsd;
	uint8_t gUapsdPerAcBitmask;
	uint8_t gUapsdPerAcTriggerEnableMask;
	uint8_t gUapsdPerAcDeliveryEnableMask;
	uint8_t csaOffloadEnable;
	uint8_t gAcAdmitMask[SIR_MAC_DIRECTION_DIRECT];

	tPowersaveoffloadInfo pmmOffloadInfo;
	uint8_t smpsMode;

	uint8_t chainMask;

	uint8_t dfsIncludeChanSwIe;

	uint8_t dfsIncludeChanWrapperIe;
	uint8_t bw_update_include_ch_sw_ie;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint8_t cc_switch_mode;
#endif

	bool isCiscoVendorAP;

	struct add_ie_params add_ie_params;

	uint8_t *pSchProbeRspTemplate;
	uint8_t *pSchBeaconFrameBegin;
	uint8_t *pSchBeaconFrameEnd;
	uint16_t schBeaconOffsetBegin;
	uint16_t schBeaconOffsetEnd;
	bool isOSENConnection;
	struct qos_map_set QosMapSet;

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	bool bRoamSynchInProgress;
#endif

	tftPEContext ftPEContext;
	bool isNonRoamReassoc;
	qdf_mc_timer_t pmf_retry_timer;
	struct comeback_timer_info pmf_retry_timer_info;
	qdf_mc_timer_t protection_fields_reset_timer;
	qdf_mc_timer_t ap_ecsa_timer;
	qdf_wake_lock_t ap_ecsa_wakelock;
	qdf_runtime_lock_t ap_ecsa_runtime_lock;
	struct mac_context *mac_ctx;
	uint16_t old_protection_state;
	tSirMacAddr prev_ap_bssid;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	bool sap_advertise_avoid_ch_ie;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
#ifdef FEATURE_WLAN_ESE
	uint8_t is_ese_version_ie_present;
#endif
	bool sap_dot11mc;
	bool is_vendor_specific_vhtcaps;
	uint8_t vendor_specific_vht_ie_sub_type;
	bool vendor_vht_sap;
	tDot11fIEhs20vendor_ie hs20vendor_ie;
	uint8_t country_info_present;
	uint8_t nss;
	bool nss_forced_1x1;
	bool add_bss_failed;
	struct obss_scanparam obss_ht40_scanparam;
	uint8_t vdev_nss;
	bool supported_nss_1x1;
	bool is_ext_caps_present;
	uint16_t beacon_tx_rate;
	uint8_t *access_policy_vendor_ie;
	uint8_t access_policy;
	bool send_p2p_conf_frame;
	bool process_ho_fail;
	uint8_t lim_non_ecsa_cap_num;
#ifdef WLAN_FEATURE_11AX
	bool he_capable;
	tDot11fIEhe_cap he_config;
	tDot11fIEhe_op he_op;
	uint32_t he_sta_obsspd;
	bool he_6ghz_band;
#ifdef WLAN_FEATURE_11AX_BSS_COLOR
	tDot11fIEbss_color_change he_bss_color_change;
	struct bss_color_info bss_color_info[MAX_BSS_COLOR_VALUE];
	uint8_t bss_color_changing;
#endif
#endif
	struct deauth_retry_params deauth_retry;
	bool enable_bcast_probe_rsp;
	uint8_t ht_client_cnt;
	bool ch_switch_in_progress;
	bool he_with_wep_tkip;
#ifdef WLAN_FEATURE_FILS_SK
	struct pe_fils_session *fils_info;
#endif

	uint16_t prev_auth_seq_num;
	tSirMacAddr prev_auth_mac_addr;
	struct obss_detection_cfg obss_offload_cfg;
	struct obss_detection_cfg current_obss_detection;
	bool is_session_obss_offload_enabled;
	bool is_obss_reset_timer_initialized;
	bool sae_pmk_cached;
	bool recvd_deauth_while_roaming;
	bool recvd_disassoc_while_roaming;
	uint16_t deauth_disassoc_rc;
	enum wmi_obss_color_collision_evt_type obss_color_collision_dec_evt;
	bool is_session_obss_color_collision_det_enabled;
	tSirMacEdcaParamRecord ap_mu_edca_params[QCA_WLAN_AC_ALL];
	bool mu_edca_present;
	int8_t def_max_tx_pwr;
	bool active_ba_64_session;
	bool is_mbssid_enabled;
#ifdef WLAN_SUPPORT_TWT
	uint8_t peer_twt_requestor;
	uint8_t peer_twt_responder;
#endif
	bool enable_session_twt_support;
	uint32_t cac_duration_ms;
	tSirResultCodes stop_bss_reason;
	uint16_t prot_status_code;
	tSirResultCodes result_code;
	uint32_t dfs_regdomain;
	uint8_t ap_defined_power_type_6g;
	uint8_t best_6g_power_type;
	bool sta_follows_sap_power;
#ifdef WLAN_FEATURE_11BE
	bool eht_capable;
	tDot11fIEeht_cap eht_config;
	tDot11fIEeht_op eht_op;
#ifdef WLAN_FEATURE_11BE_MLO
	struct mlo_link_ie_info mlo_link_info;
	struct mlo_partner_info ml_partner_info;
	uint16_t mlo_ie_total_len;
	struct wlan_mlo_ie mlo_ie;
#endif
#endif /* WLAN_FEATURE_11BE */
	uint8_t user_edca_set;
	bool is_oui_auth_assoc_6mbps_2ghz_enable;
	bool is_unexpected_peer_error;
	uint8_t join_probe_cnt;
};

/*-------------------------------------------------------------------------
   Function declarations and documentation
   ------------------------------------------------------------------------*/

#ifdef WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY
/**
 * pe_allocate_dph_node_array_buffer() - Allocate g_dph_node_array
 * memory dynamically
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_NOMEM on failure
 */
QDF_STATUS pe_allocate_dph_node_array_buffer(void);

/**
 * pe_free_dph_node_array_buffer() - Free memory allocated dynamically
 *
 * Return: None
 */
void pe_free_dph_node_array_buffer(void);
#else /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */
static inline QDF_STATUS pe_allocate_dph_node_array_buffer(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline void pe_free_dph_node_array_buffer(void)
{
}
#endif /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */

/**
 * pe_create_session() - Creates a new PE session given the BSSID
 * @mac: pointer to global adapter context
 * @bssid: BSSID of the new session
 * @sessionId: PE session ID is returned here, if PE session is created.
 * @numSta: number of stations
 * @bssType: bss type of new session to do conditional memory allocation.
 * @vdev_id: vdev_id
 *
 * This function returns the session context and the session ID if the session
 * corresponding to the passed BSSID is found in the PE session table.
 *
 * Return: ptr to the session context or NULL if session can not be created.
 */
struct pe_session *pe_create_session(struct mac_context *mac,
				     uint8_t *bssid, uint8_t *sessionId,
				     uint16_t numSta, enum bss_type bssType,
				     uint8_t vdev_id);

/**
 * pe_find_session_by_bssid() - looks up the PE session given the BSSID.
 *
 * @mac:          pointer to global adapter context
 * @bssid:         BSSID of the new session
 * @sessionId:     session ID is returned here, if session is created.
 *
 * This function returns the session context and the session ID if the session
 * corresponding to the given BSSID is found in the PE session table.
 *
 * Return: pointer to the session context or NULL if session is not found.
 */
struct pe_session *pe_find_session_by_bssid(struct mac_context *mac, uint8_t *bssid,
				     uint8_t *sessionId);

/**
 * pe_find_session_by_vdev_id() - looks up the PE session given the vdev_id.
 * @mac:             pointer to global adapter context
 * @vdev_id:         vdev id the session
 *
 * Return: pointer to the session context or NULL if session is not found.
 */
struct pe_session *pe_find_session_by_vdev_id(struct mac_context *mac,
					      uint8_t vdev_id);

/**
 * pe_find_session_by_vdev_id_and_state() - Find PE session by vdev_id and
 * mlm state.
 * @mac:             pointer to global adapter context
 * @vdev_id:         vdev id the session
 * @lim_state:       LIM state of the session
 *
 * During LFR2 roaming, new pe session is created before old pe session
 * deleted, the 2 pe sessions have different pe session id, but same vdev id,
 * can't get correct pe session by vdev id at this time.
 *
 * Return: pointer to the session context or NULL if session is not found.
 */
struct pe_session
*pe_find_session_by_vdev_id_and_state(struct mac_context *mac,
				      uint8_t vdev_id,
				      enum eLimMlmStates lim_state);

/**
 * pe_find_session_by_bssid_and_vdev_id() - looks up the PE session given
 * the BSSID and vdev id.
 * @mac:          pointer to global adapter context
 * @bssid:         BSSID of the new session
 * @vdev_id:         vdev id the session
 * @sessionId:     session ID is returned here, if session is created.
 *
 * This function returns the session context and the session ID if the session
 * corresponding to the given BSSID and vdev id is found in the PE
 * session table.
 *
 * Return: pointer to the session context or NULL if session is not found.
 */
struct pe_session *pe_find_session_by_bssid_and_vdev_id(struct mac_context *mac,
							uint8_t *bssid,
							uint8_t vdev_id,
							uint8_t *sessionId);

/**
 * pe_find_session_by_peer_sta() - looks up the PE session given the Peer
 * Station Address.
 *
 * @mac:          pointer to global adapter context
 * @sa:            Peer STA Address of the session
 * @sessionId:     session ID is returned here, if session is found.
 *
 * This function returns the session context and the session ID if the session
 * corresponding to the given destination address is found in the PE session
 * table.
 *
 * Return: pointer to the session context or NULL if session is not found.
 */
struct pe_session *pe_find_session_by_peer_sta(struct mac_context *mac, uint8_t *sa,
					uint8_t *sessionId);

/**
 * pe_find_session_by_session_id() - looks up the PE session given the session
 * ID.
 *
 * @mac:          pointer to global adapter context
 * @sessionId:     session ID for which session context needs to be looked up.
 *
 * This function returns the session context  if the session corresponding to
 * the given session ID is found in the PE session table.
 *
 * Return: pointer to the session context or NULL if session is not found.
 */
struct pe_session *pe_find_session_by_session_id(struct mac_context *mac,
					  uint8_t sessionId);

/**
 * pe_delete_session() - deletes the PE session given the session ID.
 *
 * @mac:        pointer to global adapter context
 * @pe_session: session to delete
 *
 * Return: void
 */
void pe_delete_session(struct mac_context *mac, struct pe_session *pe_session);

/**
 * pe_find_session_by_scan_id() - looks up the PE session for given scan id
 * @mac_ctx:   pointer to global adapter context
 * @scan_id:   scan id
 *
 * looks up the PE session for given scan id
 *
 * Return: pe session entry for given scan id if found else NULL
 */
struct pe_session *pe_find_session_by_scan_id(struct mac_context *mac_ctx,
				       uint32_t scan_id);

uint8_t pe_get_active_session_count(struct mac_context *mac_ctx);

/**
 * lim_dump_session_info() - Dump the key parameters of PE session
 * @mac_ctx: Global MAC context
 * @pe_session: PE session
 *
 * Dumps the fields from the @pe_session for debugging.
 *
 * Return: void
 */
void lim_dump_session_info(struct mac_context *mac_ctx,
			   struct pe_session *pe_session);

#ifdef WLAN_FEATURE_11AX
/**
 * lim_dump_he_info() - Dump HE fields in PE session
 * @mac: Global MAC context
 * @session: PE session
 *
 * Dumps the fields related to HE from the @session for debugging
 *
 * Return: void
 */
void lim_dump_he_info(struct mac_context *mac, struct pe_session *session);
#else
static inline void lim_dump_he_info(struct mac_context *mac,
				    struct pe_session *session)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * lim_dump_eht_info() - Dump EHT fields in PE session
 * @session: PE session
 *
 * Dumps the fields related to EHT from @session for debugging
 *
 * Return: void
 */
void lim_dump_eht_info(struct pe_session *session);
#else
static inline void lim_dump_eht_info(struct pe_session *session)
{
}
#endif

#ifdef WLAN_FEATURE_FILS_SK
/**
 * pe_delete_fils_info: API to delete fils session info
 * @session: pe session
 *
 * Return: void
 */
void pe_delete_fils_info(struct pe_session *session);
#endif

/**
 * lim_set_bcn_probe_filter - set the beacon/probe filter in mac context
 *
 * @mac_ctx: pointer to global mac context
 * @session: pointer to the PE session
 * @sap_channel: Operating Channel of the session for SAP sessions
 *
 * Sets the beacon/probe filter in the global mac context to filter
 * and drop beacon/probe frames before posting it to PE queue
 *
 * Return: None
 */
void lim_set_bcn_probe_filter(struct mac_context *mac_ctx,
				struct pe_session *session,
				uint8_t sap_channel);

/**
 * lim_reset_bcn_probe_filter - clear the beacon/probe filter in mac context
 *
 * @mac_ctx: pointer to the global mac context
 * @session: pointer to the PE session whose filter is to be cleared
 *
 * Return: None
 */
void lim_reset_bcn_probe_filter(struct mac_context *mac_ctx, struct pe_session *session);

/**
 * lim_update_bcn_probe_filter - Update the beacon/probe filter in mac context
 *
 * @mac_ctx: pointer to the global mac context
 * @session: pointer to the PE session whose filter is to be cleared
 *
 * This API is applicable only for SAP sessions to update the SAP channel
 * in the filter during a channel switch
 *
 * Return: None
 */
void lim_update_bcn_probe_filter(struct mac_context *mac_ctx, struct pe_session *session);

#endif /* #if !defined( __LIM_SESSION_H ) */
