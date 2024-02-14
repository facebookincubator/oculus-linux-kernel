/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef WLAN_QCT_WLANSAP_H
#define WLAN_QCT_WLANSAP_H

/**
 * W L A N   S O F T A P  P A L   L A Y E R
 * E X T E R N A L  A P I
 *
 * DESCRIPTION
 * This file contains the external API exposed by the wlan SAP PAL layer
 *  module.
 */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "cds_api.h"
#include "cds_packet.h"
#include "qdf_types.h"

#include "sme_api.h"
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 * defines and enum
 *--------------------------------------------------------------------------*/
#define       MAX_ACL_MAC_ADDRESS          32
#define       AUTO_CHANNEL_SELECT          0
#define       MAX_ASSOC_IND_IE_LEN         255
#define       MAX_ASSOC_REQ_IE_LEN         2000
#define       ASSOC_REQ_IE_OFFSET          4

/* defines for WPS config states */
#define       SAP_WPS_DISABLED             0
#define       SAP_WPS_ENABLED_UNCONFIGURED 1
#define       SAP_WPS_ENABLED_CONFIGURED   2

#define       MAX_CHANNEL_LIST_LEN         256
#define       QDF_MAX_NO_OF_SAP_MODE       2    /* max # of SAP */
#define       SAP_MAX_NUM_SESSION          5
#define       SAP_MAX_OBSS_STA_CNT         1    /* max # of OBSS STA */
#define       SAP_ACS_WEIGHT_MAX           (26664)
/* ACS will mark non ACS channels(filtered by PCL) or channels not in
 * ACS scan list to SAP_ACS_WEIGHT_MAX.
 * But the filtered channel still need a reasonable weight to
 * calculate the combined weight for ACS bw 40/80/160/320.
 * Assign SAP_ACS_WEIGHT_ADJUSTABLE to such channels and update it
 * with reasonable weight after all channels weight are computed.
 */
#define       SAP_ACS_WEIGHT_ADJUSTABLE    (SAP_ACS_WEIGHT_MAX - 1)

#define SAP_DEFAULT_24GHZ_CHANNEL     (6)
#define SAP_DEFAULT_5GHZ_CHANNEL      (40)
#define SAP_CHANNEL_NOT_SELECTED (0)

#define SAP_PRE_CAC_IFNAME "precac"

/*--------------------------------------------------------------------------
 * reasonCode taken from 802.11 standard.
 * ------------------------------------------------------------------------*/

typedef enum {
	eSAP_RC_RESERVED0,              /*0 */
	eSAP_RC_UNSPECIFIED,            /*1 */
	eSAP_RC_PREV_AUTH_INVALID,      /*2 */
	eSAP_RC_STA_LEFT_DEAUTH,        /*3 */
	eSAP_RC_INACTIVITY_DISASSOC,    /*4 */
	eSAP_RC_AP_CAPACITY_FULL,       /*5 */
	eSAP_RC_CLS2_FROM_NON_AUTH_STA, /*6 */
	eSAP_RC_CLS3_FROM_NON_AUTH_STA, /*7 */
	eSAP_RC_STA_LEFT_DISASSOC,      /*8 */
	eSAP_RC_STA_NOT_AUTH,           /*9 */
	eSAP_RC_PC_UNACCEPTABLE,        /*10 */
	eSAP_RC_SC_UNACCEPTABLE,        /*11 */
	eSAP_RC_RESERVED1,              /*12 */
	eSAP_RC_INVALID_IE,             /*13 */
	eSAP_RC_MIC_FAIL,               /*14 */
	eSAP_RC_4_WAY_HANDSHAKE_TO,     /*15 */
	eSAP_RC_GO_KEY_HANDSHAKE_TO,    /*16 */
	eSAP_RC_IE_MISMATCH,            /*17 */
	eSAP_RC_INVALID_GRP_CHIPHER,    /*18 */
	eSAP_RC_INVALID_PAIR_CHIPHER,   /*19 */
	eSAP_RC_INVALID_AKMP,           /*20 */
	eSAP_RC_UNSUPPORTED_RSN,        /*21 */
	eSAP_RC_INVALID_RSN,            /*22 */
	eSAP_RC_1X_AUTH_FAILED,         /*23 */
	eSAP_RC_CHIPER_SUITE_REJECTED,  /*24 */
} eSapReasonCode;

typedef enum {
	eSAP_ACCEPT_UNLESS_DENIED = 0,
	eSAP_DENY_UNLESS_ACCEPTED = 1,
	/* this type is added to support accept & deny list at the same time */
	eSAP_SUPPORT_ACCEPT_AND_DENY = 2,
	/*In this mode all MAC addresses are allowed to connect */
	eSAP_ALLOW_ALL = 3,
} eSapMacAddrACL;

typedef enum {
	SAP_DENY_LIST = 0,   /* List of mac addresses NOT allowed to assoc */
	SAP_ALLOW_LIST = 1,   /* List of mac addresses allowed to assoc */
} eSapACLType;

typedef enum {
	ADD_STA_TO_ACL = 0,       /* cmd to add STA to access control list */
	DELETE_STA_FROM_ACL = 1,  /* cmd to del STA from access control list */
} eSapACLCmdType;

typedef enum {
	eSAP_START_BSS_EVENT = 0,     /* Event sent when BSS is started */
	eSAP_STOP_BSS_EVENT,          /* Event sent when BSS is stopped */
	eSAP_STA_ASSOC_IND,           /* Indicate assoc req to upper layers */
	/*
	 * Event sent when we have successfully associated a station and
	 * upper layer needs to allocate a context
	 */
	eSAP_STA_ASSOC_EVENT,
	/*
	 * Event sent when we have successfully reassociated a station and
	 * upper layer needs to allocate a context
	 */
	eSAP_STA_REASSOC_EVENT,
	/*
	 * Event sent when associated a station has disassociated as a
	 * result of various conditions
	 */
	eSAP_STA_DISASSOC_EVENT,
	/* Event sent when user called wlansap_set_key_sta */
	eSAP_STA_SET_KEY_EVENT,
	/* Event sent whenever there is MIC failure detected */
	eSAP_STA_MIC_FAILURE_EVENT,
	/* Event send on WPS PBC probe request is received */
	eSAP_WPS_PBC_PROBE_REQ_EVENT,
	eSAP_DISCONNECT_ALL_P2P_CLIENT,
	eSAP_MAC_TRIG_STOP_BSS_EVENT,
	/*
	 * Event send when a STA in neither allow list or deny list tries to
	 * associate in softap mode
	 */
	eSAP_UNKNOWN_STA_JOIN,
	/* Event send when a new STA is rejected association since softAP
	 * max assoc limit has reached
	 */
	eSAP_MAX_ASSOC_EXCEEDED,
	eSAP_CHANNEL_CHANGE_EVENT,
	eSAP_DFS_CAC_START,
	eSAP_DFS_CAC_INTERRUPTED,
	eSAP_DFS_CAC_END,
	eSAP_DFS_RADAR_DETECT,
	/* No ch available after DFS RADAR detect */
	eSAP_DFS_NO_AVAILABLE_CHANNEL,
	eSAP_STOP_BSS_DUE_TO_NO_CHNL,
	eSAP_ACS_SCAN_SUCCESS_EVENT,
	eSAP_ACS_CHANNEL_SELECTED,
	eSAP_ECSA_CHANGE_CHAN_IND,
	eSAP_DFS_NEXT_CHANNEL_REQ,
	/* Event sent channel switch status to upper layer */
	eSAP_CHANNEL_CHANGE_RESP,
} eSapHddEvent;

typedef enum {
	eSAP_OPEN_SYSTEM,
	eSAP_SHARED_KEY,
	eSAP_AUTO_SWITCH
} eSapAuthType;

typedef enum {
	/* Disassociation was internally initiated from CORE stack */
	eSAP_MAC_INITATED_DISASSOC = 0x10000,
	/*
	 * Disassociation was internally initiated from host by
	 * invoking wlansap_disassoc_sta call
	 */
	eSAP_USR_INITATED_DISASSOC
} eSapDisassocReason;

typedef enum {
	eSAP_DFS_NOL_CLEAR,
	eSAP_DFS_NOL_RANDOMIZE,
} eSapDfsNolType;

/*---------------------------------------------------------------------------
  SAP PAL "status" and "reason" error code defines
  ---------------------------------------------------------------------------*/
typedef enum {
	eSAP_STATUS_SUCCESS,            /* Success.  */
	eSAP_STATUS_FAILURE,            /* General Failure.  */
	/* Channel not selected during initial scan.  */
	eSAP_START_BSS_CHANNEL_NOT_SELECTED,
	eSAP_ERROR_MAC_START_FAIL,     /* Failed to start Infra BSS */
} eSapStatus;

/*---------------------------------------------------------------------------
  SAP PAL "status" and "reason" error code defines
  ---------------------------------------------------------------------------*/
typedef enum {
	eSAP_WPSPBC_OVERLAP_IN120S,  /* Overlap */
	/* no WPS probe request in 120 second */
	eSAP_WPSPBC_NO_WPSPBC_PROBE_REQ_IN120S,
	/* One WPS probe request in 120 second  */
	eSAP_WPSPBC_ONE_WPSPBC_PROBE_REQ_IN120S,
} eWPSPBCOverlap;

/*---------------------------------------------------------------------------
  SAP Associated station types
  ---------------------------------------------------------------------------*/
typedef enum {
	eSTA_TYPE_NONE    = 0x00000000,  /* No station type */
	eSTA_TYPE_INFRA   = 0x00000001,  /* legacy station */
	eSTA_TYPE_P2P_CLI = 0x00000002,  /* p2p client */
} eStationType;

/*----------------------------------------------------------------------------
 *  Typedefs
 * -------------------------------------------------------------------------*/
typedef struct sap_StartBssCompleteEvent_s {
	uint8_t status;
	uint32_t operating_chan_freq;
	enum phy_ch_width ch_width;
	uint16_t staId;         /* self StaID */
	uint8_t sessionId;      /* SoftAP SME session ID */
} tSap_StartBssCompleteEvent;

typedef struct sap_StopBssCompleteEvent_s {
	uint8_t status;
} tSap_StopBssCompleteEvent;

typedef struct sap_StationAssocIndication_s {
	struct qdf_mac_addr staMac;
	uint8_t staId;
	uint8_t status;
	/* Required for indicating the frames to upper layer */
	uint32_t assocReqLength;
	uint8_t *assocReqPtr;
	bool fWmmEnabled;
	uint8_t ecsa_capable;
	uint32_t owe_ie_len;
	uint8_t *owe_ie;
} tSap_StationAssocIndication;

typedef struct sap_StationAssocReassocCompleteEvent_s {
	struct qdf_mac_addr staMac;
	eStationType staType;
	uint8_t staId;
	uint8_t status;
	uint8_t *ies;
	uint32_t ies_len;
	uint32_t status_code;
	bool wmmEnabled;
	uint8_t timingMeasCap;
	struct oem_channel_info chan_info;
	bool ampdu;
	bool sgi_enable;
	bool tx_stbc;
	bool rx_stbc;
	tSirMacHTChannelWidth ch_width;
	enum sir_sme_phy_mode mode;
	uint8_t max_supp_idx;
	uint8_t max_ext_idx;
	uint8_t max_mcs_idx;
	uint8_t max_real_mcs_idx;
	uint8_t rx_mcs_map;
	uint8_t tx_mcs_map;
	uint8_t ecsa_capable;
	uint32_t ext_cap;
	uint8_t supported_band;
	tDot11fIEHTCaps ht_caps;
	tDot11fIEVHTCaps vht_caps;
	tSirMacCapabilityInfo capability_info;
	bool he_caps_present;
	struct qdf_mac_addr sta_mld;
} tSap_StationAssocReassocCompleteEvent;

typedef struct sap_StationDisassocCompleteEvent_s {
	struct qdf_mac_addr staMac;
	uint8_t staId;          /* STAID should not be used */
	uint8_t status;
	uint32_t status_code;
	uint32_t reason_code;
	eSapDisassocReason reason;
	int rssi;
	int tx_rate;
	int rx_rate;
	uint32_t rx_mc_bc_cnt;
	uint32_t rx_retry_cnt;
} tSap_StationDisassocCompleteEvent;

typedef struct sap_StationSetKeyCompleteEvent_s {
	uint8_t status;
	struct qdf_mac_addr peerMacAddr;
} tSap_StationSetKeyCompleteEvent;

/*struct corresponding to SAP_STA_MIC_FAILURE_EVENT */
typedef struct sap_StationMICFailureEvent_s {
	struct qdf_mac_addr srcMacAddr;    /* address used to compute MIC */
	struct qdf_mac_addr staMac;        /* taMacAddr transmitter address */
	struct qdf_mac_addr dstMacAddr;
	bool multicast;
	uint8_t IV1;            /* first byte of IV */
	uint8_t keyId;          /* second byte of IV */
	uint8_t TSC[SIR_CIPHER_SEQ_CTR_SIZE];           /* sequence number */

} tSap_StationMICFailureEvent;

typedef struct sap_WPSPBCProbeReqEvent_s {
	uint8_t status;
	/* module id that was passed in wlansap_get_assoc_stations API */
	QDF_MODULE_ID module;
	tSirWPSPBCProbeReq WPSPBCProbeReq;
} tSap_WPSPBCProbeReqEvent;

typedef struct sap_SendActionCnf_s {
	eSapStatus actionSendSuccess;
} tSap_SendActionCnf;

typedef struct sap_UnknownSTAJoinEvent_s {
	struct qdf_mac_addr macaddr;
} tSap_UnknownSTAJoinEvent;

typedef struct sap_MaxAssocExceededEvent_s {
	struct qdf_mac_addr macaddr;
} tSap_MaxAssocExceededEvent;

/**
 * sap_acs_ch_selected_s - the structure to hold the selected channels
 * @pri_chan_freq: Holds the ACS selected primary channel frequency
 * @ht_sec_ch_freq: Holds the ACS selected secondary ht channel frequency
 * @vht_seg0_center_ch: Holds the ACS selected center channel of vht seg0
 * @vht_seg1_center_ch: Holds the ACS selected center channel of vht seg1
 * @ch_width: Holds the ACS selected channel bandwidth
 *
 * Holds the primary and secondary channel selected by ACS and is
 * used to send it to the HDD.
 */
struct sap_ch_selected_s {
	uint32_t pri_ch_freq;
	uint32_t ht_sec_ch_freq;
	uint16_t vht_seg0_center_ch_freq;
	uint16_t vht_seg1_center_ch_freq;
	uint16_t ch_width;
};

/**
 * struct sap_acs_scan_complete_event - acs scan complete event
 * @status: status of acs scan
 * @freq_list: acs scan channel frequency list
 * @num_of_channels: number of channels
 */
struct sap_acs_scan_complete_event {
	uint8_t status;
	uint32_t *freq_list;
	uint8_t num_of_channels;
};

/**
 * struct sap_ch_change_ind - channel change indication
 * @new_chan_freq: channel frequency to change to
 */
struct sap_ch_change_ind {
	uint32_t new_chan_freq;
};

/*
 * This struct will be filled in and passed to sap_event_cb that is
 * provided during wlansap_start_bss call The event id corresponding to
 * structure  in the union is defined in comment next to the structure
 */

struct sap_event {
	eSapHddEvent sapHddEventCode;
	union {
		/*SAP_START_BSS_EVENT */
		tSap_StartBssCompleteEvent sapStartBssCompleteEvent;
		/*SAP_STOP_BSS_EVENT */
		tSap_StopBssCompleteEvent sapStopBssCompleteEvent;
		/*SAP_ASSOC_INDICATION */
		tSap_StationAssocIndication sapAssocIndication;
		/*SAP_STA_ASSOC_EVENT, SAP_STA_REASSOC_EVENT */
		tSap_StationAssocReassocCompleteEvent
				sapStationAssocReassocCompleteEvent;
		/*SAP_STA_DISASSOC_EVENT */
		tSap_StationDisassocCompleteEvent
				sapStationDisassocCompleteEvent;
		/*SAP_STA_SET_KEY_EVENT */
		tSap_StationSetKeyCompleteEvent sapStationSetKeyCompleteEvent;
		/*SAP_STA_MIC_FAILURE_EVENT */
		tSap_StationMICFailureEvent sapStationMICFailureEvent;
		/*eSAP_WPS_PBC_PROBE_REQ_EVENT */
		tSap_WPSPBCProbeReqEvent sapPBCProbeReqEvent;
		tSap_SendActionCnf sapActionCnf;
		/* eSAP_UNKNOWN_STA_JOIN */
		tSap_UnknownSTAJoinEvent sapUnknownSTAJoin;
		/* eSAP_MAX_ASSOC_EXCEEDED */
		tSap_MaxAssocExceededEvent sapMaxAssocExceeded;
		struct sap_ch_selected_s sap_ch_selected;
		struct sap_ch_change_ind sap_chan_cng_ind;
		struct sap_acs_scan_complete_event sap_acs_scan_comp;
		QDF_STATUS ch_change_rsp_status;
	} sapevt;
};

typedef struct sap_SSID {
	uint8_t length;
	uint8_t ssId[WLAN_SSID_MAX_LEN];
} qdf_packed tSap_SSID_t;

typedef struct sap_SSIDInfo {
	tSap_SSID_t ssid;     /* SSID of the AP */
	/* SSID should/shouldn't be bcast in probe RSP & beacon */
	uint8_t ssidHidden;
} qdf_packed tSap_SSIDInfo_t;

/**
 * struct master_acs - acs attributes received from userspace
 * @hw_mode: hw mode
 * @ht: ht flag
 * @ht40: ht40 flag
 * @vht: vht flag
 * @eht: eht flag
 * @ch_width: channel bandwidth
 */
struct master_acs {
	uint8_t    hw_mode;
	uint8_t    ht;
	uint8_t    ht40;
	uint8_t    vht;
	uint8_t    eht;
	uint16_t   ch_width;
};

struct sap_acs_cfg {
	/* ACS Algo Input */
	uint8_t    acs_mode;
	eCsrPhyMode hw_mode;
	uint32_t    start_ch_freq;
	uint32_t    end_ch_freq;
	uint32_t   *freq_list;
	uint8_t    ch_list_count;
	uint32_t   *master_freq_list;
	uint8_t    master_ch_list_count;
#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	uint8_t    skip_scan_status;
	uint32_t    skip_scan_range1_stch;
	uint32_t    skip_scan_range1_endch;
	uint32_t    skip_scan_range2_stch;
	uint32_t    skip_scan_range2_endch;
#endif

	uint16_t   ch_width;
	uint32_t   pcl_chan_freq[NUM_CHANNELS];
	uint8_t    pcl_channels_weight_list[NUM_CHANNELS];
	uint32_t   pcl_ch_count;
	uint8_t    is_ht_enabled;
	uint8_t    is_vht_enabled;
	/* ACS Algo Output */
	uint32_t   pri_ch_freq;
	uint32_t   ht_sec_ch_freq;
	uint32_t    vht_seg0_center_ch_freq;
	uint32_t    vht_seg1_center_ch_freq;
	uint32_t   band;
#ifdef WLAN_FEATURE_11BE
	bool       is_eht_enabled;
	uint16_t   acs_puncture_bitmap;
#endif
	bool       skip_acs_scan;
	uint32_t   last_scan_ageout_time;
	struct master_acs master_acs_cfg;
};

/*
 * enum sap_acs_dfs_mode- state of DFS mode
 * @ACS_DFS_MODE_NONE: DFS mode attribute is not valid
 * @ACS_DFS_MODE_ENABLE:  DFS mode is enabled
 * @ACS_DFS_MODE_DISABLE: DFS mode is disabled
 * @ACS_DFS_MODE_DEPRIORITIZE: Deprioritize DFS channels in scanning
 */
enum  sap_acs_dfs_mode {
	ACS_DFS_MODE_NONE,
	ACS_DFS_MODE_ENABLE,
	ACS_DFS_MODE_DISABLE,
	ACS_DFS_MODE_DEPRIORITIZE
};

struct sap_config {
	tSap_SSIDInfo_t SSIDinfo;
	eCsrPhyMode sap_orig_hw_mode;	/* Previous wireless Mode */
	eCsrPhyMode SapHw_mode;         /* Wireless Mode */
	eSapMacAddrACL SapMacaddr_acl;
	struct qdf_mac_addr accept_mac[MAX_ACL_MAC_ADDRESS]; /* MAC filtering */
	struct qdf_mac_addr deny_mac[MAX_ACL_MAC_ADDRESS];  /* MAC filtering */
	struct qdf_mac_addr self_macaddr;       /* self macaddress or BSSID */
	uint32_t chan_freq;          /* Operation channel frequency */
	uint32_t sec_ch_freq;
	struct ch_params ch_params;
	enum phy_ch_width ch_width_orig;
	uint8_t dtim_period;      /* dtim interval */
	uint16_t num_accept_mac;
	uint16_t num_deny_mac;
	/* Max ie length 255 * 2(WPA+RSN) + 2 bytes(vendor specific ID) * 2 */
	uint8_t RSNWPAReqIE[(WLAN_MAX_IE_LEN * 2) + 4];
	eSapAuthType authType;
	tCsrAuthList akm_list;
	bool privacy;
	/* 0 - disabled, 1 - not configured , 2 - configured */
	uint8_t wps_state;
	uint16_t RSNWPAReqIELength;     /* The byte count in the pWPAReqIE */
	uint32_t beacon_int;            /* Beacon Interval */
	enum QDF_OPMODE persona; /* Tells us which persona, GO or AP */
	bool enOverLapCh;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint8_t cc_switch_mode;
#endif
	struct sap_acs_cfg acs_cfg;
	uint16_t probeRespIEsBufferLen;
	/* buffer for addn ies comes from hostapd */
	void *pProbeRespIEsBuffer;
	uint16_t assocRespIEsLen;
	/* buffer for addn ies comes from hostapd */
	void *pAssocRespIEsBuffer;
	uint16_t probeRespBcnIEsLen;
	/* buffer for addn ies comes from hostapd */
	void *pProbeRespBcnIEsBuffer;
	uint16_t beacon_tx_rate;
	uint8_t *vendor_ie;
	tSirMacRateSet supported_rates;
	tSirMacRateSet extended_rates;
	bool require_h2e;
	enum sap_acs_dfs_mode acs_dfs_mode;
	struct hdd_channel_info *channel_info;
	uint32_t channel_info_count;
	bool dfs_cac_offload;
#ifdef WLAN_FEATURE_11BE_MLO
	bool mlo_sap;
	uint8_t link_id;
	uint8_t num_link;
#endif
};

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
typedef enum {
	eSAP_DO_NEW_ACS_SCAN,
	eSAP_DO_PAR_ACS_SCAN,
	eSAP_SKIP_ACS_SCAN
} tSap_skip_acs_scan;
#endif

typedef enum {
	eSAP_DFS_DO_NOT_SKIP_CAC,
	eSAP_DFS_SKIP_CAC
} eSapDfsCACState_t;

typedef enum {
	eSAP_DFS_CHANNEL_USABLE,
	eSAP_DFS_CHANNEL_AVAILABLE,
	eSAP_DFS_CHANNEL_UNAVAILABLE
} eSapDfsChanStatus_t;

typedef struct sSapDfsNolInfo {
	uint8_t dfs_channel_number;
	eSapDfsChanStatus_t radar_status_flag;
	uint64_t radar_found_timestamp;
} tSapDfsNolInfo;

typedef struct sSapDfsInfo {
	qdf_mc_timer_t sap_dfs_cac_timer;
	/*
	 * New channel frequency to move to when a  Radar is
	 * detected on current Channel
	 */
	uint32_t target_chan_freq;
	uint8_t ignore_cac;
	eSapDfsCACState_t cac_state;
	uint32_t user_provided_target_chan_freq;

	/*
	 * Requests for Channel Switch Announcement IE
	 * generation and transmission
	 */
	uint8_t csaIERequired;
	uint8_t is_dfs_cac_timer_running;
	/*
	 * New channel width and new channel bonding mode
	 * will only be updated via channel fallback mechanism
	 */
	enum phy_ch_width orig_chanWidth;
	enum phy_ch_width new_chanWidth;
	struct ch_params new_ch_params;

	/*
	 * sap_operating_channel_location holds SAP indoor,
	 * outdoor location information. Currently, if this
	 * param is  set this Indoor/outdoor channel interop
	 * restriction will only be implemented for JAPAN
	 * regulatory domain.
	 *
	 * 0 - Indicates that location unknown
	 * (or) SAP Indoor/outdoor interop is allowed
	 *
	 * 1 - Indicates device is operating on Indoor channels
	 * and SAP cannot pick next random channel from outdoor
	 * list of channels when a radar is found on current operating
	 * DFS channel.
	 *
	 * 2 - Indicates device is operating on Outdoor Channels
	 * and SAP cannot pick next random channel from indoor
	 * list of channels when a radar is found on current
	 * operating DFS channel.
	 */
	uint8_t sap_operating_chan_preferred_location;

	/*
	 * Flag to indicate if DFS test mode is enabled and
	 * channel switch is disabled.
	 */
	uint8_t disable_dfs_ch_switch;
	uint16_t tx_leakage_threshold;
	/* beacon count before channel switch */
	uint8_t sap_ch_switch_beacon_cnt;
	uint8_t sap_ch_switch_mode;
	uint16_t reduced_beacon_interval;
} tSapDfsInfo;

/* MAX number of CAC channels to be recorded */
#define MAX_NUM_OF_CAC_HISTORY 8

/**
 * struct prev_cac_result - previous cac result
 * @ap_start_time: ap start timestamp
 * @ap_end_time: ap stop or cac end timestamp
 * @cac_complete: cac complete without found radar event
 * @cac_ch_param: ap channel parameters
 */
struct prev_cac_result {
	uint64_t ap_start_time;
	uint64_t ap_end_time;
	bool cac_complete;
	struct ch_params cac_ch_param;
};

/**
 * struct dfs_radar_history - radar found history element
 * @time: timestamp in us from system boot
 * @radar_found: radar found or not
 * @ch_freq: channel frequency in Mhz
 */
struct dfs_radar_history {
	uint64_t time;
	bool radar_found;
	uint16_t ch_freq;
};

#ifdef DCS_INTERFERENCE_DETECTION
/**
 * struct sap_dcs_info - record sap dcs information.
 * @wlan_interference_mitigation_enable: wlan interference mitigation
 *                                       is enabled per vdev.
 * @is_vdev_starting: is vdev doing restart because of dcs.
 */
struct sap_dcs_info {
	bool wlan_interference_mitigation_enable[WLAN_MAX_VDEVS];
	bool is_vdev_starting[WLAN_MAX_VDEVS];
};
#endif

struct sap_ctx_list {
	void *sap_context;
	enum QDF_OPMODE sapPersona;
};

typedef struct tagSapStruct {
	/* Information Required for SAP DFS Master mode */
	tSapDfsInfo SapDfsInfo;
	struct sap_ctx_list sapCtxList[SAP_MAX_NUM_SESSION];
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	bool sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	bool acs_with_more_param;
	bool enable_dfs_phy_error_logs;
	uint8_t one_time_csa_count;
#ifdef DCS_INTERFERENCE_DETECTION
	struct sap_dcs_info dcs_info;
#endif
} tSapStruct, *tpSapStruct;

/**
 * struct sap_context - per-BSS Context for SAP
 *
 * struct sap_context is used to share per-BSS context between SAP and
 * its clients. A context is generated by sap_create_ctx() and is
 * destroyed by sap_destroy_ctx(). During the lifetime of the BSS the
 * SAP context is passed as the primary parameter to SAP APIs. Note
 * that by design the contents of the structure are opaque to the
 * clients and a SAP context pointer must only be dereferenced by SAP.
 */
struct sap_context;

/**
 * wlansap_roam_callback() - API to get the events for SAP persona
 * @ctx: callback context registered with SME (sap context is registered)
 * @csr_roam_info: pointer to SME CSR roam info structure
 * @roam_status: status of the event reported by SME to SAP
 * @roam_result: result of the event reported by SME to SAP
 *
 * Any activity like start_bss, stop_bss, and etc for SAP persona
 * happens, SME reports the result of those events to SAP through this
 * callback.
 *
 * Return: QDF_STATUS based on overall result
 */
QDF_STATUS wlansap_roam_callback(void *ctx,
				 struct csr_roam_info *csr_roam_info,
				 eRoamCmdStatus roam_status,
				 eCsrRoamResult roam_result);

/**
 * sap_create_ctx() - API to create the sap context
 *
 * This API assigns the sap context from global sap context pool
 * stored in gp_sap_ctx[i] array.
 *
 * Return: Pointer to the SAP context, or NULL if a context could not
 * be allocated
 */
struct sap_context *sap_create_ctx(void);

/**
 * sap_destroy_ctx - API to destroy the sap context
 * @sap_ctx: Pointer to the SAP context
 *
 * This API puts back the given sap context to global sap context pool which
 * makes current sap session's sap context invalid.
 *
 * Return: The result code associated with performing the operation
 *         QDF_STATUS_E_FAULT: Pointer to SAP cb is NULL;
 *                             access would cause a page fault
 *         QDF_STATUS_SUCCESS: Success
 */
QDF_STATUS sap_destroy_ctx(struct sap_context *sap_ctx);

/**
 * sap_init_ctx - Initialize the sap context
 * @sap_ctx: Pointer to the SAP context
 * @mode: Device mode
 * @addr: MAC address of the SAP
 * @session_id: Pointer to the session id
 * @reinit: if called as part of reinit
 *
 * sap_create_ctx() allocates the sap context which is uninitialized.
 * This API needs to be called to properly initialize the sap context
 * which is just created.
 *
 * Return: The result code associated with performing the operation
 *         QDF_STATUS_E_FAULT: BSS could not be started
 *         QDF_STATUS_SUCCESS: Success
 */
QDF_STATUS sap_init_ctx(struct sap_context *sap_ctx,
			 enum QDF_OPMODE mode,
			 uint8_t *addr, uint32_t session_id, bool reinit);

/**
 * sap_deinit_ctx() - De-initialize the sap context
 * @sap_ctx: Pointer to the SAP context
 *
 * When SAP session is about to close, this API needs to be called
 * to de-initialize all the members of sap context structure, so that
 * nobody can accidentally start using the sap context.
 *
 * Return: The result code associated with performing the operation
 *         QDF_STATUS_E_FAULT: BSS could not be stopped
 *         QDF_STATUS_SUCCESS: Success
 */
QDF_STATUS sap_deinit_ctx(struct sap_context *sap_ctx);

/**
 * sap_is_auto_channel_select() - is channel AUTO_CHANNEL_SELECT
 * @sapcontext: Pointer to the SAP context
 *
 * Return: true on AUTO_CHANNEL_SELECT, false otherwise
 */
bool sap_is_auto_channel_select(struct sap_context *sapcontext);

QDF_STATUS wlansap_global_init(void);
QDF_STATUS wlansap_global_deinit(void);
typedef QDF_STATUS (*sap_event_cb)(struct sap_event *sap_event,
				   void *user_context);

/**
 * wlansap_is_channel_in_nol_list() - This API checks if channel is
 * in nol list
 * @sap_ctx: SAP context pointer
 * @chan_freq: channel frequency
 * @chanBondState: channel bonding state
 *
 * Return: True if the channel is in the NOL list, false otherwise
 */
bool wlansap_is_channel_in_nol_list(struct sap_context *sap_ctx,
				    qdf_freq_t chan_freq,
				    ePhyChanBondState chanBondState);

/**
 * wlansap_is_channel_leaking_in_nol() - This API checks if channel is leaking
 * in nol list
 * @sap_ctx: SAP context pointer
 * @chan_freq: channel frequency
 * @chan_bw: channel bandwidth
 *
 * Return: True/False
 */
bool wlansap_is_channel_leaking_in_nol(struct sap_context *sap_ctx,
				       uint16_t chan_freq,
				       uint8_t chan_bw);

/**
 * wlansap_start_bss() - start BSS
 * @sap_ctx: Pointer to the SAP context
 * @sap_event_cb: Callback function in HDD called by SAP to inform HDD
 *                        about SAP results
 * @config: Pointer to configuration structure passed down from
 *                    HDD(HostApd for Android)
 * @user_context: Parameter that will be passed back in all the SAP callback
 *               events.
 *
 * This api function provides SAP FSM event eWLAN_SAP_PHYSICAL_LINK_CREATE for
 * starting AP BSS
 *
 * Return: The result code associated with performing the operation
 *         QDF_STATUS_E_FAULT: Pointer to SAP cb is NULL;
 *                             access would cause a page fault
 *         QDF_STATUS_SUCCESS: Success
 */
QDF_STATUS wlansap_start_bss(struct sap_context *sap_ctx,
			     sap_event_cb sap_event_cb,
			     struct sap_config *config, void *user_context);

/**
 * wlansap_stop_bss() - stop BSS.
 * @sap_ctx: Pointer to SAP context
 *
 * This api function provides SAP FSM event eSAP_HDD_STOP_INFRA_BSS for
 * stopping AP BSS
 *
 * Return: The result code associated with performing the operation
 *         QDF_STATUS_E_FAULT: Pointer to SAP cb is NULL;
 *                             access would cause a page fault
 *         QDF_STATUS_SUCCESS: Success
 */
QDF_STATUS wlansap_stop_bss(struct sap_context *sap_ctx);

/**
 * wlan_sap_update_next_channel() - Update next channel configured using vendor
 * command in SAP context
 * @sap_ctx: SAP context
 * @channel: channel number
 * @chan_bw: channel width
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_sap_update_next_channel(struct sap_context *sap_ctx,
					uint8_t channel,
					enum phy_ch_width chan_bw);

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * wlansap_check_cc_intf() - Get interfering concurrent channel
 * @sap_ctx: SAP context pointer
 *
 * Determine if a concurrent channel is interfering.
 *
 * Return: Channel freq (Mhz) of the interfering channel, or 0 if none.
 */
uint16_t wlansap_check_cc_intf(struct sap_context *sap_ctx);
#endif

/**
 * wlansap_set_mac_acl() - set MAC list entry in ACL.
 * @sap_ctx: Pointer to the SAP context
 * @config: Pointer to SAP config.
 *
 * This api function provides SAP to set mac list entry in accept list as well
 * as deny list
 *
 * Return: The result code associated with performing the operation
 *         QDF_STATUS_E_FAULT: Pointer to SAP cb is NULL;
 *                             access would cause a page fault
 *         QDF_STATUS_SUCCESS: Success
 */
QDF_STATUS wlansap_set_mac_acl(struct sap_context *sap_ctx,
			       struct sap_config *config);

/**
 * wlansap_disassoc_sta() - initiate disassociation of station.
 * @sap_ctx: Pointer to the SAP context
 * @p_del_sta_params: pointer to station deletion parameters
 *
 * This api function provides for Ap App/HDD initiated disassociation of station
 *
 * Return: The QDF_STATUS code associated with performing the operation
 *         QDF_STATUS_SUCCESS:  Success
 */
QDF_STATUS wlansap_disassoc_sta(struct sap_context *sap_ctx,
				struct csr_del_sta_params *p_del_sta_params);

/**
 * wlansap_deauth_sta() - Ap App/HDD initiated deauthentication of station
 * @sap_ctx: Pointer to the SAP context
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * This api function provides for Ap App/HDD initiated deauthentication of
 * station
 *
 * Return: The QDF_STATUS code associated with performing the operation
 */
QDF_STATUS wlansap_deauth_sta(struct sap_context *sap_ctx,
			      struct csr_del_sta_params *pDelStaParams);

/**
 * wlansap_set_channel_change_with_csa() - Set channel change with CSA
 * @sap_ctx: Pointer to SAP context
 * @target_chan_freq: Target channel frequency
 * @target_bw: Target bandwidth
 * @strict: if true switch to the requested channel always, fail
 *        otherwise
 *
 * This api function does a channel change to the target channel specified.
 * CSA IE is included in the beacons before doing a channel change.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_set_channel_change_with_csa(struct sap_context *sap_ctx,
					       uint32_t target_chan_freq,
					       enum phy_ch_width target_bw,
					       bool strict);


/**
 * wlan_sap_getstation_ie_information() - RSNIE Population
 * @sap_ctx: Pointer to the SAP context
 * @len: Length of @buf
 * @buf: RSNIE IE data
 *
 *  Populate RSN IE from CSR to HDD context
 *
 * Return: QDF_STATUS enumeration
 */

QDF_STATUS wlan_sap_getstation_ie_information(struct sap_context *sap_ctx,
					      uint32_t *len, uint8_t *buf);

/**
 * wlansap_clear_acl() - Clear all ACLs
 * @sap_ctx: Pointer to the SAP context
 *
 * Return: QDF_STATUS. If success the ACLs were cleared, otherwise an
 *    error occurred.
 */
QDF_STATUS wlansap_clear_acl(struct sap_context *sap_ctx);

/**
 * wlansap_get_acl_accept_list() - Get ACL accept list
 * @sap_ctx: Pointer to the SAP context
 * @pAcceptList: Pointer to the buffer to store the ACL accept list
 * @nAcceptList: Pointer to the location to store the number of
 *    entries in the ACL accept list.
 *
 * Return: QDF_STATUS. If success the data was returned, otherwise an
 *    error occurred.
 */
QDF_STATUS wlansap_get_acl_accept_list(struct sap_context *sap_ctx,
				       struct qdf_mac_addr *pAcceptList,
				       uint16_t *nAcceptList);

/**
 * wlansap_is_channel_present_in_acs_list() - Freq present in ACS list or not
 * @freq: Frequency to be searched
 * @ch_freq_list: channel frequency list.
 * @ch_count: Channel frequency list count
 *
 * Return: True is found, false otherwise
 */
bool wlansap_is_channel_present_in_acs_list(uint32_t freq,
					    uint32_t *ch_freq_list,
					    uint8_t ch_count);

/**
 * wlansap_get_acl_deny_list() - Get ACL deny list
 * @sap_ctx: Pointer to the SAP context
 * @pDenyList: Pointer to the buffer to store the ACL deny list
 * @nDenyList: Pointer to the location to store the number of
 *    entries in the ACL deny list.
 *
 * Return: QDF_STATUS. If success the data was returned, otherwise an
 *    error occurred.
 */
QDF_STATUS wlansap_get_acl_deny_list(struct sap_context *sap_ctx,
				     struct qdf_mac_addr *pDenyList,
				     uint16_t *nDenyList);

/**
 * wlansap_set_acl_mode() - Set the SAP ACL mode
 * @sap_ctx: The SAP context pointer
 * @mode: the desired ACL mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_set_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL mode);

/**
 * wlansap_get_acl_mode() - Get the SAP ACL mode
 * @sap_ctx: The SAP context pointer
 * @mode: Pointer where to return the current ACL mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_get_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL *mode);

/**
 * wlansap_modify_acl() - Update ACL entries
 * @sap_ctx: Pointer to the SAP context
 * @peer_sta_mac: peer sta mac to be updated.
 * @list_type: allow/Deny list type.
 * @cmd: command to be executed on ACL.
 *
 * This function is called when a peer needs to be added or deleted from the
 * allow/deny ACL
 *
 * Return: Status
 */
QDF_STATUS wlansap_modify_acl(struct sap_context *sap_ctx,
			      uint8_t *peer_sta_mac,
			      eSapACLType list_type, eSapACLCmdType cmd);

/**
 * wlansap_channel_change_request() - Send channel change request
 * @sap_ctx: Pointer to the SAP context
 * @target_channel: Target channel
 *
 * This API is used to send an Indication to SME/PE to change the
 * current operating channel to a different target channel.
 *
 * The Channel change will be issued by SAP under the following
 * scenarios.
 * 1. A radar indication is received  during SAP CAC WAIT STATE and
 *    channel change is required.
 * 2. A radar indication is received during SAP STARTED STATE and
 *    channel change is required.
 *
 * Return: The QDF_STATUS code associated with performing the operation
 *   QDF_STATUS_SUCCESS:  Success
 *
 */
QDF_STATUS wlansap_channel_change_request(struct sap_context *sap_ctx,
					  uint32_t target_chan_freq);

/**
 * wlansap_get_sec_channel() - get the secondary sap channel
 * @sec_ch_offset: secondary channel offset.
 * @op_chan_freq: Operating sap channel frequency.
 * @sec_chan_freq: channel frequency to be filled.
 *
 * This API will get the secondary sap channel from the offset, and
 * operating channel.
 *
 * Return: None
 *
 */
void wlansap_get_sec_channel(uint8_t sec_ch_offset,
			     uint32_t op_chan_freq,
			     uint32_t *sec_chan_freq);

/**
 * wlansap_start_beacon_req() - Send Start Beaconing Request
 * @sap_ctx: Pointer to the SAP context
 *
 * This API is used to send an Indication to SME/PE to start
 * beaconing on the current operating channel.
 *
 * When SAP is started on DFS channel and when ADD BSS RESP is received
 * LIM temporarily holds off Beaconing for SAP to do CAC WAIT. When
 * CAC WAIT is done SAP resumes the Beacon Tx by sending a start beacon
 * request to LIM.
 *
 * Return: The QDF_STATUS code associated with performing the operation
 *   QDF_STATUS_SUCCESS:  Success
 */
QDF_STATUS wlansap_start_beacon_req(struct sap_context *sap_ctx);

/**
 * wlansap_dfs_send_csa_ie_request() - Send CSA IE
 * @sap_ctx: Pointer to the SAP context
 *
 * This API is used to send channel switch announcement request to PE
 *
 * Return: The QDF_STATUS code associated with performing the operation
 *    QDF_STATUS_SUCCESS:  Success
 */
QDF_STATUS wlansap_dfs_send_csa_ie_request(struct sap_context *sap_ctx);

/**
 * wlansap_get_dfs_ignore_cac() - Get ignore_cac value
 * @mac_handle: Opaque handle to the global MAC context
 * @ignore_cac: Location to store ignore_cac value
 *
 * This API is used to Get the value of ignore_cac value
 *
 * Return: The QDF_STATUS code associated with performing the operation
 */
QDF_STATUS wlansap_get_dfs_ignore_cac(mac_handle_t mac_handle,
				      uint8_t *ignore_cac);

/**
 * wlansap_set_dfs_ignore_cac() - Set ignore_cac value
 * @mac_handle: Opaque handle to the global MAC context
 * @ignore_cac: value to set for ignore_cac variable in DFS global structure.
 *
 * This API is used to Set the value of ignore_cac value
 *
 * Return: The QDF_STATUS code associated with performing the operation
 */
QDF_STATUS wlansap_set_dfs_ignore_cac(mac_handle_t mac_handle,
				      uint8_t ignore_cac);
/**
 * wlansap_get_dfs_cac_state() - Get cac_state value
 * @mac_handle: Opaque handle to the global MAC context
 * @cac_state: Location to store cac_state value
 *
 * This API is used to Get the value of ignore_cac value
 *
 * Return: The QDF_STATUS code associated with performing the operation
 */
QDF_STATUS wlansap_get_dfs_cac_state(mac_handle_t mac_handle,
				     eSapDfsCACState_t *cac_state);

/**
 * wlansap_get_csa_chanwidth_from_phymode() - function to populate
 * channel width from user configured phymode for csa
 * @sap_context: sap adapter context
 * @chan_freq: target channel frequency (MHz)
 * @tgt_ch_params: target new channel bw parameters to be updated
 *
 * Return: phy_ch_width
 */
enum phy_ch_width
wlansap_get_csa_chanwidth_from_phymode(struct sap_context *sap_context,
				       uint32_t chan_freq,
				       struct ch_params *tgt_ch_params);

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
QDF_STATUS
wlan_sap_set_channel_avoidance(mac_handle_t mac_handle,
			       bool sap_channel_avoidance);
#endif

/**
 * wlan_sap_set_acs_with_more_param() - sets acs_with_more_param ini param
 * @mac_handle: Opaque handle to the global MAC context
 * @acs_with_more_param: ini parameter value
 *
 * Return: The QDF_STATUS code.
 */
QDF_STATUS
wlan_sap_set_acs_with_more_param(mac_handle_t mac_handle,
				 bool acs_with_more_param);

/**
 * wlansap_set_dfs_preferred_channel_location() - set dfs preferred channel
 * @mac_handle: Opaque handle to the global MAC context
 *
 * This API is used to set sap preferred channels location
 * to resetrict the DFS random channel selection algorithm
 * either Indoor/Outdoor channels only.
 * dfs_Preferred_Channels_location :
 *       0 - Indicates No preferred channel location restrictions
 *       1 - Indicates SAP Indoor Channels operation only.
 *       2 - Indicates SAP Outdoor Channels operation only.
 *
 * Return: The QDF_STATUS code associated with performing the operation
 *         QDF_STATUS_SUCCESS:  Success and error code otherwise.
 */
QDF_STATUS wlansap_set_dfs_preferred_channel_location(mac_handle_t mac_handle);

/**
 * wlansap_set_dfs_target_chnl() - Set target channel
 * @mac_handle: Opaque handle for the global MAC context
 * @target_chan_freq: target channel frequency to be set
 *
 * This API is used to set next target chnl as provided channel.
 * you can provide any valid channel to this API.
 *
 * Return: The QDF_STATUS code associated with performing the operation
 */
QDF_STATUS wlansap_set_dfs_target_chnl(mac_handle_t mac_handle,
				       uint32_t target_chan_freq);

/**
 * wlan_sap_get_phymode() - Returns sap phymode.
 * @sap_ctx:	Pointer to Sap Context.
 *
 * This function provides the SAP current phymode.
 *
 * Return: phymode
 */
eCsrPhyMode wlan_sap_get_phymode(struct sap_context *sap_ctx);

/**
 * wlan_sap_get_concurrent_bw() - Returns SAP BW based on concurrent channel &
 *                                STA DFS channel
 * @pdev: Pointer to Pdev
 * @psoc: Pointer to Psoc
 * @con_ch_freq: interfering concurrent channel
 * @channel_width: Channel width
 *
 * Return: Channel width. If STA is not present on con_ch_freq, it returns
 *        max of STA BW and 80 Mhz. If STA is not connected in dfs chan or STA
 *........BW is not 160 Mhz (which includes DFS channel), then it will return
 *        BW maximum of STA BW and 80 Mhz. If DFS STA is present, then return
 *        BW as min of 80 and STA BW.
 */
enum phy_ch_width wlan_sap_get_concurrent_bw(struct wlan_objmgr_pdev *pdev,
					     struct wlan_objmgr_psoc *psoc,
					     qdf_freq_t con_ch_freq,
					     enum phy_ch_width channel_width);

/**
 * wlan_sap_get_vht_ch_width() - Returns SAP VHT channel width.
 * @sap_ctx:	Pointer to Sap Context
 *
 * This function provides the SAP current VHT channel with.
 *
 * Return: VHT channel width
 */
uint32_t wlan_sap_get_vht_ch_width(struct sap_context *sap_ctx);

/**
 * wlan_sap_get_ch_params() - get ch params
 * @sap_ctx: Pointer to Sap Context
 * @ch_params: returned ch_params
 *
 * This function get sap's ch_params
 *
 * Return: true for success
 */
bool wlan_sap_get_ch_params(struct sap_context *sap_ctx,
			    struct ch_params *ch_params);

/**
 * wlan_sap_set_sap_ctx_acs_cfg() - Sets acs cfg
 * @sap_ctx:  Pointer to Sap Context
 * @sap_config:  Pointer to sap config
 *
 * This function sets the acs cfg in sap context.
 *
 * Return: None
 */
void wlan_sap_set_sap_ctx_acs_cfg(struct sap_context *sap_ctx,
				  struct sap_config *sap_config);

void sap_config_acs_result(mac_handle_t mac_handle,
			   struct sap_context *sap_ctx,
			   uint32_t sec_ch_freq);

QDF_STATUS wlansap_update_sap_config_add_ie(struct sap_config *config,
					    const uint8_t *pAdditionIEBuffer,
					    uint16_t additionIELength,
					    eUpdateIEsType updateType);

QDF_STATUS wlansap_reset_sap_config_add_ie(struct sap_config *config,
					   eUpdateIEsType updateType);

void wlansap_extend_to_acs_range(mac_handle_t mac_handle,
				 uint32_t *start_ch_freq,
				 uint32_t *end_ch_freq,
				 uint32_t *bandStartChannel,
				 uint32_t *bandEndChannel);

#ifdef WLAN_FEATURE_SON
/**
 * wlansap_son_update_sap_config_phymode() - update sap config according to
 *                                           phy_mode. This API is for son,
 *                                           There is no band switching when
 *                                           son phy mode is changed.
 * @sap_ctx:  Pointer to Sap Context
 * @sap_config:  Pointer to sap config
 * @phy_mode: pointer to phy mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlansap_son_update_sap_config_phymode(struct wlan_objmgr_vdev *vdev,
				      struct sap_config *config,
				      enum qca_wlan_vendor_phy_mode phy_mode);
#endif

/**
 * wlansap_set_dfs_nol() - Set dfs nol
 * @sap_ctx: SAP context
 * @conf: set type
 *
 * Return: QDF_STATUS
 */
#ifdef DFS_COMPONENT_ENABLE
QDF_STATUS wlansap_set_dfs_nol(struct sap_context *sap_ctx,
			       eSapDfsNolType conf);
#else
static inline QDF_STATUS wlansap_set_dfs_nol(struct sap_context *sap_ctx,
			       eSapDfsNolType conf)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_sap_set_dfs_pri_multiplier() - Set dfs_pri_multiplier
 * @mac_handle: Opaque handle to the global MAC context
 *
 * Return: none
 */
#ifdef DFS_PRI_MULTIPLIER
void wlan_sap_set_dfs_pri_multiplier(mac_handle_t mac_handle);
#else
static inline void wlan_sap_set_dfs_pri_multiplier(mac_handle_t mac_handle)
{
}
#endif

/**
 * wlan_sap_set_vendor_acs() - Set vendor specific acs in sap context
 * @sap_context: SAP context
 * @is_vendor_acs: if vendor specific acs is enabled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_sap_set_vendor_acs(struct sap_context *sap_context,
				   bool is_vendor_acs);

/**
 * wlansap_populate_del_sta_params() - populate delete station parameter
 * @mac: Pointer to peer mac address.
 * @reason_code: Reason code for the disassoc/deauth.
 * @subtype: Subtype points to either disassoc/deauth frame.
 * @params: Parameters to be populated.
 *
 * This API is used to populate delete station parameter structure
 *
 * Return: none
 */
void wlansap_populate_del_sta_params(const uint8_t *mac,
				     uint16_t reason_code,
				     uint8_t subtype,
				     struct csr_del_sta_params *params);

/**
 * wlansap_acs_chselect() - Initiates acs channel selection
 * @sap_context:               Pointer to SAP context structure
 * @acs_event_callback:       Callback function in hdd called by sap
 *                             to inform hdd about channel selection result
 * @config:                   Pointer to configuration structure
 *                             passed down from hdd
 * @pusr_context:              Parameter that will be passed back in all
 *                             the sap callback events.
 *
 * This function serves as an api for hdd to initiate acs scan pre
 * start bss.
 *
 * Return: The QDF_STATUS code associated with performing the operation.
 */
QDF_STATUS wlansap_acs_chselect(struct sap_context *sap_context,
				sap_event_cb acs_event_callback,
				struct sap_config *config,
				void *pusr_context);

/**
 * sap_undo_acs() - Undo acs i.e free the allocated ch lists
 * @sap_ctx: pointer to the SAP context
 *
 * This function will free the memory allocated to the sap ctx channel list, acs
 * cfg ch list and master ch list.
 *
 * Return: None
 */
void sap_undo_acs(struct sap_context *sap_context, struct sap_config *sap_cfg);

/**
 * wlansap_get_chan_width() - get sap channel width.
 * @sap_ctx: pointer to the SAP context
 *
 * This function get channel width of sap.
 *
 * Return: sap channel width
 */
uint32_t wlansap_get_chan_width(struct sap_context *sap_ctx);

/**
 * wlansap_get_max_bw_by_phymode() - get max channel width based on phymode
 * @sap_ctx: pointer to the SAP context
 *
 * This function get max channel width of sap based on phymode.
 *
 * Return: channel width
 */
enum phy_ch_width
wlansap_get_max_bw_by_phymode(struct sap_context *sap_ctx);

/*
 * wlansap_set_invalid_session() - set session ID to invalid
 * @sap_ctx: pointer to the SAP context
 *
 * This function sets session ID to invalid
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_set_invalid_session(struct sap_context *sap_ctx);

/*
 * wlansap_set_invalid_session() - Release vdev ref taken by sap context
 * @sap_ctx: pointer to the SAP context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_release_vdev_ref(struct sap_context *sap_ctx);

/**
 * sap_get_cac_dur_dfs_region() - get cac duration and dfs region.
 * @sap_ctxt: sap context
 * @cac_duration_ms: pointer to cac duration
 * @dfs_region: pointer to dfs region
 * @chan_freq: channel frequency
 * @ch_params: pointer to ch_params
 *
 * Get cac duration and dfs region.
 *
 * Return: None
 */
void sap_get_cac_dur_dfs_region(struct sap_context *sap_ctx,
				uint32_t *cac_duration_ms,
				uint32_t *dfs_region,
				qdf_freq_t chan_freq,
				struct ch_params *ch_params);

/**
 * sap_clear_global_dfs_param() - Reset global dfs param of sap ctx
 * @mac_handle: pointer to mac handle
 * @sap_ctx: sap context
 *
 * This API resets global dfs param of sap ctx.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sap_clear_global_dfs_param(mac_handle_t mac_handle,
				      struct sap_context *sap_ctx);

/**
 * sap_dfs_set_current_channel() - Set current channel params in dfs component
 * @sap_ctx: sap context
 *
 * Set current channel params in dfs component, this info will be used to mark
 * the channels in nol when radar is detected.
 *
 * Return: None
 */
void sap_dfs_set_current_channel(void *sap_ctx);

/**
 * wlansap_cleanup_cac_timer() - Force cleanup DFS CAC timer
 * @sap_ctx: sap context
 *
 * Force cleanup DFS CAC timer when reset all adapters. It will not
 * check concurrency SAP since just called when reset all adapters.
 *
 * Return: None
 */
void wlansap_cleanup_cac_timer(struct sap_context *sap_ctx);

/**
 * wlansap_update_owe_info() - Update OWE info
 * @sap_ctx: sap context
 * @peer: peer mac
 * @ie: IE from hostapd
 * @ie_len: IE length
 * @owe_status: status from hostapd
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_update_owe_info(struct sap_context *sap_ctx,
				   uint8_t *peer, const uint8_t *ie,
				   uint32_t ie_len, uint16_t owe_status);

/**
 * wlansap_update_ft_info() - Update FT info
 * @sap_ctx: sap context
 * @peer: peer mac
 * @ie: IE from hostapd
 * @ie_len: IE length
 * @ft_status: wlan status codes
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_update_ft_info(struct sap_context *sap_ctx,
				  uint8_t *peer, const uint8_t *ie,
				  uint32_t ie_len, uint16_t ft_status);

/**
 * wlansap_filter_ch_based_acs() -filter out channel based on acs
 * @sap_ctx: sap context
 * @ch_freq_list: pointer to channel frequency list
 * @ch_cnt: channel number of channel list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_filter_ch_based_acs(struct sap_context *sap_ctx,
				       uint32_t *ch_freq_list,
				       uint32_t *ch_cnt);

/**
 * wlansap_is_6ghz_included_in_acs_range() - check 6ghz channel included in
 * ACS range
 * @sap_ctx: sap context
 *
 * Return: QDF_STATUS
 */
bool wlansap_is_6ghz_included_in_acs_range(struct sap_context *sap_ctx);

/**
 * wlansap_get_safe_channel_from_pcl_and_acs_range() - Get safe channel for SAP
 * restart
 * @sap_ctx: sap context
 *
 * Get a safe channel to restart SAP. PCL already takes into account the
 * unsafe channels. So, the PCL is validated with the ACS range to provide
 * a safe channel for the SAP to restart.
 *
 * Return: Chan freq num to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
uint32_t
wlansap_get_safe_channel_from_pcl_and_acs_range(struct sap_context *sap_ctx);

/**
 * wlansap_get_safe_channel_from_pcl_for_sap() - Get safe and active channel
 * for SAP restart
 * @sap_ctx: sap context
 *
 * Get a safe and active channel to restart SAP. PCL already takes into account
 * the unsafe channels.
 *
 * Return: Chan freq num to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
uint32_t wlansap_get_safe_channel_from_pcl_for_sap(struct sap_context *sap_ctx);

/**
 * wlansap_get_chan_band_restrict() -  get new chan for band change
 * @sap_ctx: sap context pointer
 * @csa_reason: channel switch reason to update
 *
 * Sap/p2p go channel switch from 5G to 2G by CSA when 5G band disabled to
 * avoid conflict with modem N79.
 * Sap/p2p go channel restore to 5G channel when 5G band enabled.
 * Note: csa_reason is only updated when channel is disabled or band is
 * restricted, so it must be initialized to a default value beforehand
 *
 * Return - restart channel in MHZ
 */
qdf_freq_t wlansap_get_chan_band_restrict(struct sap_context *sap_ctx,
					  enum sap_csa_reason_code *csa_reason);

/**
 * wlansap_override_csa_strict_for_sap() - check user CSA strict or not
 * @mac: mac ctx
 * @sap_ctx: sap context
 * @target_chan_freq: target channel frequency in MHz
 * @strict: CSA strict flag
 *
 * If force SCC enabled, user trigger SAP CSA and target channel
 * doesn't cause MCC with existing STA/CLI, then override strict flag to
 * true, so that driver can skip the overlap interference check and
 * allow the CSA go through. This is to allow SAP/GO force SCC in
 * same band.
 *
 * Return: true if CSA is strict, otherwise false
 */
bool
wlansap_override_csa_strict_for_sap(mac_handle_t mac_handle,
				    struct sap_context *sap_ctx,
				    uint32_t target_chan_freq,
				    bool strict);

/**
 * sap_get_csa_reason_str() - Get csa reason in string
 * @reason: sap reason enum value
 *
 * Return: string reason
 */
const char *sap_get_csa_reason_str(enum sap_csa_reason_code reason);

#ifdef FEATURE_RADAR_HISTORY
/**
 * wlansap_query_radar_history() -  get radar history info
 * @mac_handle: mac context
 * @radar_history: radar history buffer to be returned
 * @count: total history count
 *
 * The API will return the dfs nol list(Radar found history) and
 * CAC history (no Radar found).
 *
 * Return - QDF_STATUS
 */
QDF_STATUS
wlansap_query_radar_history(mac_handle_t mac_handle,
			    struct dfs_radar_history **radar_history,
			    uint32_t *count);
#endif

#ifdef DCS_INTERFERENCE_DETECTION
/**
 * wlansap_dcs_set_vdev_wlan_interference_mitigation() - set wlan
 * interference mitigation enable information per vdev
 * @sap_context: sap context
 * @wlan_interference_mitigation_enable: wlan interference mitigation
 *                                       enable or not
 *
 * This function is used to set whether wlan interference mitigation
 * enable or not
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_dcs_set_vdev_wlan_interference_mitigation(
				struct sap_context *sap_context,
				bool wlan_interference_mitigation_enable);

/**
 * wlansap_dcs_set_wlan_interference_mitigation_on_band() - set wlan
 * interference mitigation enable information based on band information
 * @sap_context: sap context
 * @sap_cfg: sap config
 *
 * This function is used to set whether wlan interference mitigation
 * enable or not based on band information
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_dcs_set_wlan_interference_mitigation_on_band(
					struct sap_context *sap_context,
					struct sap_config *sap_cfg);

/**
 * wlansap_dcs_set_vdev_starting() - set vdev starting
 * @sap_context: sap context
 * @vdev_starting: vdev in starting states
 *
 * This function is used to set whether vdev starting or not
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_dcs_set_vdev_starting(struct sap_context *sap_context,
					 bool vdev_starting);

/**
 * wlansap_dcs_is_wlan_interference_mitigation_enabled() - get wlan interference
 * mitigation enabled information
 * @sap_context: sap context
 *
 * This function is used to get wlan interference mitigation enabled information
 * with given sap
 *
 * Return: true if wlan interference mitigation is enabled with given sap
 */
bool wlansap_dcs_is_wlan_interference_mitigation_enabled(
					struct sap_context *sap_context);

/**
 * wlansap_dcs_get_freq() - get dcs channel frequency
 * @sap_context: sap context
 *
 * This function is used to get dcs channel frequency with give sap
 *
 * Return: sap dcs channel frequency
 */
qdf_freq_t wlansap_dcs_get_freq(struct sap_context *sap_context);
#else
static inline QDF_STATUS wlansap_dcs_set_vdev_wlan_interference_mitigation(
				struct sap_context *sap_context,
				bool wlan_interference_mitigation_enable)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS wlansap_dcs_set_wlan_interference_mitigation_on_band(
						struct sap_context *sap_context,
						struct sap_config *sap_cfg)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS wlansap_dcs_set_vdev_starting(
	struct sap_context *sap_context, bool vdev_starting)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool wlansap_dcs_is_wlan_interference_mitigation_enabled(
					struct sap_context *sap_context)
{
	return false;
}

static inline qdf_freq_t wlansap_dcs_get_freq(struct sap_context *sap_context)
{
	return 0;
}
#endif

/**
 * wlansap_filter_vendor_unsafe_ch_freq() - filter sap acs ch list by
 *  vendor unsafe ch freq ranges
 * @sap_context: sap context
 * @sap_config: sap conifg
 *
 * This function is used to filter out unsafe channel frequency from acs
 * channel frequency list based on vendor unsafe channel frequency ranges.
 *
 * Return: true if vendor unsafe ch range is present, otherwise false
 */
bool wlansap_filter_vendor_unsafe_ch_freq(
	struct sap_context *sap_context, struct sap_config *sap_config);

/**
 * wlansap_dump_acs_ch_freq() - print acs channel frequency
 * @sap_ctx: sap context
 *
 * This function is used to print acs channel frequecny
 *
 * Return: None
 */
void wlansap_dump_acs_ch_freq(struct sap_context *sap_context);

/**
 * wlansap_set_acs_ch_freq() - set acs channel frequency
 * @sap_ctx: sap context
 * @ch_freq: ch_freq to be set
 *
 * This function is used to set acs channel frequency
 *
 * Return: None
 */
void wlansap_set_acs_ch_freq(struct sap_context *sap_context,
			     qdf_freq_t ch_freq);

/**
 * sap_acquire_vdev_ref() - Increment reference count for vdev object
 * @psoc: Object Manager PSoC object
 * @sap_ctx: to store vdev object pointer
 * @session_id: used to get vdev object
 *
 * This function is used to increment vdev object reference count and store
 * vdev pointer in sap_ctx.
 *
 * Return: QDF_STATUS_SUCCESS - If able to get vdev object reference
 *				else qdf status failure codes
 */
QDF_STATUS sap_acquire_vdev_ref(struct wlan_objmgr_psoc *psoc,
				struct sap_context *sap_ctx,
				uint8_t session_id);

/**
 * sap_dump_acs_channel() - dump acs channel list
 * @acs_cfg: acs config
 *
 * This function dump acs channel list
 *
 * Return: void.
 */
void sap_dump_acs_channel(struct sap_acs_cfg *acs_cfg);

/**
 * sap_release_vdev_ref() - Decrement reference count for vdev object
 * @sap_ctx: for which vdev reference is to be decremented
 *
 * Return: None
 */
void sap_release_vdev_ref(struct sap_context *sap_ctx);

#ifdef CONFIG_AFC_SUPPORT
/**
 * sap_afc_dcs_sel_chan() - API to select best SAP best channel/bandwidth with
 *                          channel ACS weighted algorithm
 * @sap_ctx: SAP context handle
 * @cur_freq: SAP current home channel frequency
 * @cur_bw: SAP current channel bandwidth
 * @pref_bw: pointer to channel bandwidth prefer to set as input, and target
 *           channel bandwidth can set as output
 *
 * Return: target home channel frequency selected
 */
qdf_freq_t sap_afc_dcs_sel_chan(struct sap_context *sap_ctx,
				qdf_freq_t cur_freq,
				enum phy_ch_width cur_bw,
				enum phy_ch_width *pref_bw);
#endif

#ifdef WLAN_FEATURE_11BE
/**
 * sap_phymode_is_eht() - Is sap phymode EHT
 * @phymode: phy mode
 *
 * Return: true if phy mode is EHT
 */
bool sap_phymode_is_eht(eCsrPhyMode phymode);

/**
 * sap_acs_is_puncture_applicable() - Is static puncturing applicable according
 *                                    to ACS configure of given sap acs config.
 * @acs_cfg: pointer to sap_acs_cfg
 *
 * Return: true if static puncturing is applicable to given sap acs config.
 */
bool sap_acs_is_puncture_applicable(struct sap_acs_cfg *acs_cfg);

/**
 * sap_acs_set_puncture_support() - Set puncturing support according
 *                                  to ACS configure of given sap.
 * @sap_ctx: Pointer to SAP Context
 * @ch_params: pointer to ch_params
 *
 * Return: void.
 */
void sap_acs_set_puncture_support(struct sap_context *sap_ctx,
				  struct ch_params *ch_params);
#else
static inline bool sap_phymode_is_eht(eCsrPhyMode phymode)
{
	return false;
}

static inline bool sap_acs_is_puncture_applicable(struct sap_acs_cfg *acs_cfg)
{
	return false;
}

static inline void sap_acs_set_puncture_support(struct sap_context *sap_ctx,
						struct ch_params *ch_params)
{
}
#endif /* WLAN_FEATURE_11BE */

#ifdef PRE_CAC_SUPPORT
/**
 * sap_cac_end_notify() - Notify CAC end to HDD
 * @mac_handle: Opaque handle to the global MAC context
 *
 * Function will be called to notify eSAP_DFS_CAC_END event to HDD
 *
 * Return: QDF_STATUS_SUCCESS if the notification was sent, otherwise
 *         an appropriate QDF_STATUS error
 */
QDF_STATUS sap_cac_end_notify(mac_handle_t mac_handle,
			      struct csr_roam_info *roamInfo);
#else
static inline QDF_STATUS
sap_cac_end_notify(mac_handle_t mac_handle,
		   struct csr_roam_info *roamInfo)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* PRE_CAC_SUPPORT */

#ifdef WLAN_FEATURE_SAP_ACS_OPTIMIZE
static inline bool sap_is_acs_scan_optimize_enable(void)
{
	return true;
}

void wlansap_process_chan_info_event(struct sap_context *sap_ctx,
				     struct csr_roam_info *roam_info);
#else
static inline bool sap_is_acs_scan_optimize_enable(void)
{
	return false;
}

static inline
void wlansap_process_chan_info_event(struct sap_context *sap_ctx,
				     struct csr_roam_info *roam_info)
{
}
#endif

#ifdef __cplusplus
}
#endif
#endif /* #ifndef WLAN_QCT_WLANSAP_H */
