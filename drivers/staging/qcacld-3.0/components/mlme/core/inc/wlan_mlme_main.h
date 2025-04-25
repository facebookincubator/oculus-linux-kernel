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
 * DOC: declare internal API related to the mlme component
 */

#ifndef _WLAN_MLME_MAIN_H_
#define _WLAN_MLME_MAIN_H_

#include "qdf_periodic_work.h"
#include <wlan_mlme_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_cmn.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include "wlan_wfa_config_public_struct.h"
#include "wlan_connectivity_logging.h"

#define MAC_MAX_ADD_IE_LENGTH       2048
/* Join probe request Retry  timer default (200)ms */
#define JOIN_PROBE_REQ_TIMER_MS              200
#define MAX_JOIN_PROBE_REQ                   5

#define MAX_WAKELOCK_FOR_BSS_COLOR_CHANGE    2000

/* If AP reported link delete timer less than such value,
 * host will do link removel directly without wait for the
 * timer timeout.
 */
#define LINK_REMOVAL_MIN_TIMEOUT_MS 1000

/*
 * Following time is used to program WOW_TIMER_PATTERN to FW so that FW will
 * wake host up to do graceful disconnect in case PEER remains un-authorized
 * for this long.
 */
#define INSTALL_KEY_TIMEOUT_SEC      70
#define INSTALL_KEY_TIMEOUT_MS       \
			(INSTALL_KEY_TIMEOUT_SEC * SYSTEM_TIME_SEC_TO_MSEC)
/* 70 seconds, for WPA, WPA2, CCKM */
#define WAIT_FOR_KEY_TIMEOUT_PERIOD     \
	(INSTALL_KEY_TIMEOUT_SEC * QDF_MC_TIMER_TO_SEC_UNIT)
/* 120 seconds, for WPS */
#define WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD (120 * QDF_MC_TIMER_TO_SEC_UNIT)

#define MLME_PEER_SET_KEY_WAKELOCK_TIMEOUT WAKELOCK_DURATION_RECOMMENDED
/* QCN IE definitions */
#define QCN_IE_HDR_LEN     6

#define QCN_IE_VERSION_SUBATTR_ID        1
#define QCN_IE_VERSION_SUBATTR_DATA_LEN  2
#define QCN_IE_VERSION_SUBATTR_LEN       4
#define QCN_IE_VERSION_SUPPORTED    1
#define QCN_IE_SUBVERSION_SUPPORTED 0

#define QCN_IE_ATTR_ID_VERSION 1
#define QCN_IE_ATTR_ID_VHT_MCS11 2
#define QCN_IE_ATTR_ID_ALL 0xFF

#define mlme_legacy_fatal(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_MLME, params)
#define mlme_legacy_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_MLME, params)
#define mlme_legacy_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_MLME, params)
#define mlme_legacy_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_MLME, params)
#define mlme_legacy_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_MLME, params)
#define MAC_B_PR_SSID_OFFSET 12

enum size_of_len_field {
	ONE_BYTE = 1,
	TWO_BYTE = 2
};

enum medium_access_type {
	MEDIUM_ACCESS_AUTO = 0,
	MEDIUM_ACCESS_DCF,
	MEDIUM_ACCESS_11E_EDCF,
	MEDIUM_ACCESS_WMM_EDCF_DSCP,
};

enum wmm_user_mode {
	WMM_USER_MODE_AUTO = 0,
	WMM_USER_MODE_QBSS_ONLY = 1,
	WMM_USER_MODE_NO_QOS = 2,

};

struct pwr_channel_info {
	uint32_t first_freq;
	uint8_t num_chan;
	int8_t max_tx_pwr;
};

/**
 * struct peer_disconnect_stats_param -Peer disconnect stats params
 * @vdev_id: vdev_id of the SAP vdev on which disconnect stats request is sent
 * @is_disconn_stats_completed: Indicates if disconnect stats request is
 * completed or not
 * @disconn_stats_timer: Disconnect stats timer
 */
struct peer_disconnect_stats_param {
	uint8_t vdev_id;
	qdf_atomic_t is_disconn_stats_completed;
	qdf_mc_timer_t disconn_stats_timer;
};

/**
 * struct wlan_mlme_rx_ops  - structure of tx function pointers for
 * roaming related commands
 * @peer_oper_mode_eventid : Rx ops function pointer for operating mode event
 */
struct wlan_mlme_rx_ops {
	QDF_STATUS (*peer_oper_mode_eventid)(struct wlan_objmgr_psoc *psoc,
					     struct peer_oper_mode_event *data);
};

/**
 * struct wlan_mlme_tx_ops - structure of mlme tx function pointers
 * @send_csa_event_status_ind: Tx ops function to send csa event indication
 *
 */
struct wlan_mlme_tx_ops {
	QDF_STATUS
		(*send_csa_event_status_ind)(struct wlan_objmgr_vdev *vdev,
					     uint8_t csa_status);
};

/**
 * struct wlan_mlme_psoc_ext_obj -MLME ext psoc priv object
 * @cfg:     cfg items
 * @rso_tx_ops: Roam Tx ops to send roam offload commands to firmware
 * @rso_rx_ops: Roam Rx ops to receive roam offload events from firmware
 * @mlme_rx_ops: mlme Rx ops to receive events from firmware
 * @mlme_tx_ops: mlme tx ops
 * @wfa_testcmd: WFA config tx ops to send to FW
 * @disconnect_stats_param: Peer disconnect stats related params for SAP case
 * @scan_requester_id: mlme scan requester id
 */
struct wlan_mlme_psoc_ext_obj {
	struct wlan_mlme_cfg cfg;
	struct wlan_cm_roam_tx_ops rso_tx_ops;
	struct wlan_cm_roam_rx_ops rso_rx_ops;
	struct wlan_mlme_rx_ops mlme_rx_ops;
	struct wlan_mlme_tx_ops mlme_tx_ops;
	struct wlan_mlme_wfa_cmd wfa_testcmd;
	struct peer_disconnect_stats_param disconnect_stats_param;
	wlan_scan_requester scan_requester_id;
};

/**
 * struct wlan_disconnect_info - WLAN Disconnection Information
 * @self_discon_ies: Disconnect IEs to be sent in deauth/disassoc frames
 *                   originated from driver
 * @peer_discon_ies: Disconnect IEs received in deauth/disassoc frames
 *                       from peer
 */
struct wlan_disconnect_info {
	struct element_info self_discon_ies;
	struct element_info peer_discon_ies;
};

/**
 * struct sae_auth_retry - SAE auth retry Information
 * @sae_auth_max_retry: Max number of sae auth retries
 * @sae_auth: SAE auth frame information
 */
struct sae_auth_retry {
	uint8_t sae_auth_max_retry;
	struct element_info sae_auth;
};

/**
 * struct peer_mlme_priv_obj - peer MLME component object
 * @last_pn_valid: if last PN is valid
 * @last_pn: last pn received
 * @rmf_pn_replays: rmf pn replay count
 * @is_pmf_enabled: True if PMF is enabled
 * @last_assoc_received_time: last assoc received time
 * @last_disassoc_deauth_received_time: last disassoc/deauth received time
 * @twt_ctx: TWT context
 * @allow_kickout: True if the peer can be kicked out. Peer can't be kicked
 *                 out if it is being steered
 * @nss: Peer NSS
 * @peer_set_key_wakelock: wakelock to protect peer set key op with firmware
 * @peer_set_key_runtime_wakelock: runtime pm wakelock for set key
 * @is_key_wakelock_set: flag to check if key wakelock is pending to release
 * @assoc_rsp: assoc rsp IE received during connection
 * @peer_ind_bw: peer indication channel bandwidth
 */
struct peer_mlme_priv_obj {
	uint8_t last_pn_valid;
	uint64_t last_pn;
	uint32_t rmf_pn_replays;
	bool is_pmf_enabled;
	qdf_time_t last_assoc_received_time;
	qdf_time_t last_disassoc_deauth_received_time;
#ifdef WLAN_SUPPORT_TWT
	struct twt_context twt_ctx;
#endif
#ifdef WLAN_FEATURE_SON
	bool allow_kickout;
#endif
	uint8_t nss;
	qdf_wake_lock_t peer_set_key_wakelock;
	qdf_runtime_lock_t peer_set_key_runtime_wakelock;
	bool is_key_wakelock_set;
	struct element_info assoc_rsp;
	enum phy_ch_width peer_ind_bw;
};

/**
 * enum vdev_assoc_type - VDEV associate/reassociate type
 * @VDEV_ASSOC: associate
 * @VDEV_REASSOC: reassociate
 * @VDEV_FT_REASSOC: fast reassociate
 */
enum vdev_assoc_type {
	VDEV_ASSOC,
	VDEV_REASSOC,
	VDEV_FT_REASSOC
};

/**
 * struct wlan_mlme_roam_state_info - Structure containing roaming
 * state related details
 * @state: Roaming module state.
 * @mlme_operations_bitmap: Bitmap containing what mlme operations are in
 *  progress where roaming should not be allowed.
 */
struct wlan_mlme_roam_state_info {
	enum roam_offload_state state;
	uint8_t mlme_operations_bitmap;
};

/**
 * struct wlan_mlme_roaming_config - Roaming configurations structure
 * @roam_trigger_bitmap: Master bitmap of roaming triggers. If the bitmap is
 *  zero, roaming module will be deinitialized at firmware for this vdev.
 * @supplicant_disabled_roaming: Enable/disable roam scan in firmware; will be
 *  used by supplicant to do roam invoke after disabling roam scan in firmware,
 *  it is only effective for current connection, it will be cleared during new
 *  connection.
 */
struct wlan_mlme_roaming_config {
	uint32_t roam_trigger_bitmap;
	bool supplicant_disabled_roaming;
};

/**
 * struct wlan_mlme_roam - Roam structure containing roam state and
 *  roam config info
 * @roam_sm: Structure containing roaming state related details
 * @roam_cfg: Roaming configurations structure
 * @sae_single_pmk: Details for sae roaming using single pmk
 * @set_pmk_pending: RSO update status of PMK from set_key
 * @sae_auth_ta: SAE pre-auth tx address
 * @sae_auth_pending:  Roaming SAE auth pending
 */
struct wlan_mlme_roam {
	struct wlan_mlme_roam_state_info roam_sm;
	struct wlan_mlme_roaming_config roam_cfg;
#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
	struct wlan_mlme_sae_single_pmk sae_single_pmk;
#endif
	bool set_pmk_pending;
	struct qdf_mac_addr sae_auth_ta;
	uint8_t sae_auth_pending;
};

#ifdef WLAN_FEATURE_MSCS
/**
 * struct tclas_mask - TCLAS Mask Elements for mscs request
 * @classifier_type: specifies the type of classifier parameters
 * in TCLAS element. Currently driver supports classifier type = 4 only.
 * @classifier_mask: Mask for tclas elements. For example, if
 * classifier type = 4, value of classifier mask is 0x5F.
 * @info: information of classifier type
 */
struct tclas_mask {
	uint8_t classifier_type;
	uint8_t classifier_mask;
	union {
		struct {
			uint8_t reserved[16];
		} ip_param; /* classifier_type = 4 */
	} info;
};

/**
 * enum scs_request_type - scs request type to peer
 * @SCS_REQ_ADD: To set mscs parameters
 * @SCS_REQ_REMOVE: Remove mscs parameters
 * @SCS_REQ_CHANGE: Update mscs parameters
 */
enum scs_request_type {
	SCS_REQ_ADD = 0,
	SCS_REQ_REMOVE = 1,
	SCS_REQ_CHANGE = 2,
};

/**
 * struct descriptor_element - mscs Descriptor element
 * @request_type: mscs request type defined in enum scs_request_type
 * @user_priority_control: To set user priority of tx packet
 * @stream_timeout: minimum timeout value, in TUs, for maintaining
 * variable user priority in the MSCS list.
 * @tclas_mask: to specify how incoming MSDUs are classified into
 * streams in MSCS
 * @status_code: status of mscs request
 */
struct descriptor_element {
	uint8_t request_type;
	uint16_t user_priority_control;
	uint64_t stream_timeout;
	struct tclas_mask tclas_mask;
	uint8_t status_code;
};

/**
 * struct mscs_req_info - mscs request information
 * @vdev_id: session id
 * @bssid: peer bssid
 * @dialog_token: Token number of mscs req action frame
 * @dec: mscs Descriptor element defines information about
 * the parameters used to classify streams
 * @is_mscs_req_sent: To Save mscs req request if any (only
 * one can be outstanding at any time)
 */
struct mscs_req_info {
	uint8_t vdev_id;
	struct qdf_mac_addr bssid;
	uint8_t dialog_token;
	struct descriptor_element dec;
	bool is_mscs_req_sent;
};
#endif

#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * enum ft_ie_state - ft state
 * @FT_START_READY: Start before and after 11r assoc
 * @FT_AUTH_REQ_READY: When we have recvd the 1st or nth auth req
 * @FT_REASSOC_REQ_WAIT: waiting for reassoc
 * @FT_SET_KEY_WAIT: waiting for key
 */
enum ft_ie_state {
	FT_START_READY,
	FT_AUTH_REQ_READY,
	FT_REASSOC_REQ_WAIT,
	FT_SET_KEY_WAIT,
};
#endif

/**
 * struct ft_context - ft related information
 * @r0kh_id_len: r0kh id len
 * @r0kh_id: r0kh id
 * @auth_ft_ie: auth ft ies received during preauth phase
 * @auth_ie_len: auth ie length
 * @reassoc_ft_ie: reassoc ft ies received during reassoc phase
 * @reassoc_ie_len: reassoc ie length
 * @ric_ies: ric ie
 * @ric_ies_length: ric ie len
 * @set_ft_preauth_state: preauth state
 * @ft_state: ft state
 * @add_mdie: add mdie in assoc req
 */
struct ft_context {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	uint32_t r0kh_id_len;
	uint8_t r0kh_id[ROAM_R0KH_ID_MAX_LEN];
#endif
#ifdef WLAN_FEATURE_HOST_ROAM
	uint8_t auth_ft_ie[MAX_FTIE_SIZE];
	uint16_t auth_ie_len;
	uint8_t reassoc_ft_ie[MAX_FTIE_SIZE];
	uint16_t reassoc_ie_len;
	uint8_t ric_ies[MAX_FTIE_SIZE];
	uint16_t ric_ies_length;
	bool set_ft_preauth_state;
	enum ft_ie_state ft_state;
	bool add_mdie;
#endif
};

/**
 * struct assoc_channel_info - store channel info at the time of association
 * @assoc_ch_width: channel width at the time of initial connection
 * @omn_ie_ch_width: ch width present in operating mode notification IE of bcn
 * @sec_2g_freq: secondary 2 GHz freq
 * @cen320_freq: 320 MHz center freq
 */
struct assoc_channel_info {
	enum phy_ch_width assoc_ch_width;
	enum phy_ch_width omn_ie_ch_width;
	qdf_freq_t sec_2g_freq;
	qdf_freq_t cen320_freq;
};

/**
 * struct mlme_connect_info - mlme connect information
 * @timing_meas_cap: Timing meas cap
 * @chan_info: oem channel info
 * @tdls_chan_swit_prohibited: if tdls chan switch is prohobited by AP
 * @tdls_prohibited: if tdls is prohobited by AP
 * @uapsd_per_ac_bitmask: Used on STA, this is a static UAPSD mask setting
 * derived from JOIN_REQ and REASSOC_REQ. If a particular AC bit is set, it
 * means the AC is both trigger enabled and delivery enabled.
 * @qos_enabled: is qos enabled
 * @ft_info: ft related info
 * @hlp_ie: hldp ie
 * @hlp_ie_len: hlp ie length
 * @fils_con_info: Pointer to fils connection info from connect req
 * @cckm_ie: cck IE
 * @cckm_ie_len: cckm_ie len
 * @ese_tspec_info: ese tspec info
 * @ext_cap_ie: Ext CAP IE
 * @assoc_btm_cap: BSS transition management cap used in (re)assoc req
 * @assoc_chan_info: store channel info at the time of association
 * @force_20mhz_in_24ghz: Only 20 MHz BW allowed in 2.4 GHz
 *
 */
struct mlme_connect_info {
	uint8_t timing_meas_cap;
	struct oem_channel_info chan_info;
#ifdef FEATURE_WLAN_TDLS
	bool tdls_chan_swit_prohibited;
	bool tdls_prohibited;
#endif
	uint8_t uapsd_per_ac_bitmask;
	bool qos_enabled;
	struct ft_context ft_info;
#ifdef WLAN_FEATURE_FILS_SK
	uint8_t *hlp_ie;
	uint32_t hlp_ie_len;
	struct wlan_fils_connection_info *fils_con_info;
#endif
#ifdef FEATURE_WLAN_ESE
	uint8_t cckm_ie[DOT11F_IE_RSN_MAX_LEN];
	uint8_t cckm_ie_len;
#ifdef WLAN_FEATURE_HOST_ROAM
	tESETspecInfo ese_tspec_info;
#endif
#endif
	uint8_t ext_cap_ie[DOT11F_IE_EXTCAP_MAX_LEN + 2];
	bool assoc_btm_cap;
	struct assoc_channel_info assoc_chan_info;
	bool force_20mhz_in_24ghz;
};

/** struct wait_for_key_timer - wait for key timer object
 * @vdev: Pointer to vdev
 * @timer: timer for wati for key
 */
struct wait_for_key_timer {
	struct wlan_objmgr_vdev *vdev;
	qdf_mc_timer_t timer;
};

/**
 * struct mlme_ap_config - VDEV MLME legacy private SAP
 * related configurations
 * @user_config_sap_ch_freq : Frequency from userspace to start SAP
 * @update_required_scc_sta_power: Change the 6 GHz power type of the
 * concurrent STA
 * @ap_policy: Concurrent ap policy config
 * @oper_ch_width: SAP current operating ch_width
 * @psd_20mhz: PSD power(dBm/MHz) of SAP operating in 20 MHz
 */
struct mlme_ap_config {
	qdf_freq_t user_config_sap_ch_freq;
#ifdef CONFIG_BAND_6GHZ
	bool update_required_scc_sta_power;
#endif
	enum host_concurrent_ap_policy ap_policy;
	enum phy_ch_width oper_ch_width;
	uint8_t psd_20mhz;
};

/**
 * struct roam_trigger_per - per roam trigger related information
 * @rx_rate_thresh_percent:  percentage of lower than rx rate threshold
 * @tx_rate_thresh_percent:  percentage of lower than tx rate threshold
 */
struct roam_trigger_per {
	uint8_t rx_rate_thresh_percent;
	uint8_t tx_rate_thresh_percent;
};

/**
 * struct roam_trigger_bmiss - bmiss roam trigger related information
 * @final_bmiss_cnt:        final beacon miss count
 * @consecutive_bmiss_cnt:  consecutive beacon miss count
 * @qos_null_success:       is Qos-Null tx Success: 0: success, 1:fail
 */
struct roam_trigger_bmiss {
	uint32_t final_bmiss_cnt;
	uint32_t consecutive_bmiss_cnt;
	bool qos_null_success;
};

/**
 * struct roam_trigger_poor_rssi - low rssi roam trigger
 * related information
 * @current_rssi:         Connected AP rssi in dBm
 * @roam_rssi_threshold:  rssi threshold value in dBm
 * @rx_linkspeed_status:  rx linkspeed status, 0:good linkspeed, 1:bad
 */
struct roam_trigger_poor_rssi {
	int8_t current_rssi;
	int8_t roam_rssi_threshold;
	bool rx_linkspeed_status;
};

/**
 * struct roam_trigger_better_rssi - high rssi roam trigger
 * related information
 * @current_rssi:      Connected AP rssi in dBm
 * @hi_rssi_threshold:  roam high RSSI threshold
 */
struct roam_trigger_better_rssi {
	int8_t current_rssi;
	int8_t hi_rssi_threshold;
};

/**
 * struct roam_trigger_congestion - congestion roam trigger
 * related information
 * @rx_tput:         RX Throughput in bytes per second in dense env
 * @tx_tput:         TX Throughput in bytes per second in dense env
 * @roamable_count:  roamable AP count info in dense env
 */
struct roam_trigger_congestion {
	uint32_t rx_tput;
	uint32_t tx_tput;
	uint8_t roamable_count;
};

/**
 * struct roam_trigger_user_trigger - user roam trigger related information
 * @invoke_reason: defined in roam_invoke_reason
 */
struct roam_trigger_user_trigger {
	enum roam_invoke_reason invoke_reason;
};

/**
 * struct roam_trigger_background -  roam trigger related information
 * @current_rssi:         Connected AP rssi in dBm
 * @data_rssi:            data frame rssi in dBm
 * @data_rssi_threshold:  data rssi threshold in dBm
 */
struct roam_trigger_background {
	int8_t current_rssi;
	int8_t data_rssi;
	int8_t data_rssi_threshold;
};

/**
 * struct roam_trigger_btm - BTM roam trigger related information
 * @btm_request_mode: mode values are defined in IEEE Std
 *  802.11-2020, 9.6.13.9
 * @disassoc_imminent_timer:     disassoc time in milliseconds
 * @validity_internal:           Preferred candidate list validity interval in
 *  milliseconds
 * @candidate_list_count:        Number of preferred candidates from BTM request
 * @btm_response_status_code:    Response status values are enumerated in
 *  IEEE Std 802.11-2020, Table 9-428 (BTM status code definitions)
 * @btm_bss_termination_timeout: BTM BSS termination timeout value in
 *  milliseconds
 * @btm_mbo_assoc_retry_timeout: BTM MBO assoc retry timeout value in
 *  milliseconds
 * @btm_req_dialog_token:        btm_req_dialog_token: dialog token number in
 *  BTM request frame
 */

struct roam_trigger_btm {
	uint8_t btm_request_mode;
	uint32_t disassoc_imminent_timer;
	uint32_t validity_internal;
	uint8_t candidate_list_count;
	uint8_t btm_response_status_code;
	uint32_t btm_bss_termination_timeout;
	uint32_t btm_mbo_assoc_retry_timeout;
	uint8_t btm_req_dialog_token;
};

/**
 * struct roam_trigger_bss_load - BSS Load roam trigger parameters
 * @cu_load: percentage of Connected AP channel congestion utilization
 */
struct roam_trigger_bss_load {
	uint8_t cu_load;
};

/**
 * struct roam_trigger_disconnection - Deauth roaming trigger related
 * parameters
 * @deauth_type:   1- Deauthentication 2- Disassociation
 * @deauth_reason: Status code of the Deauth/Disassoc received, Values
 *  are enumerated in IEEE Std 802.11-2020, Table 9-49 (Reason codes).
 */
struct roam_trigger_disconnection {
	uint8_t deauth_type;
	uint16_t deauth_reason;
};

/**
 * struct roam_trigger_periodic - periodic roam trigger
 * related information
 * @periodic_timer_ms: roam scan periodic, milliseconds
 */
struct roam_trigger_periodic {
	uint32_t periodic_timer_ms;
};

/**
 * struct roam_trigger_tx_failures - tx failures roam trigger
 * related information
 * @kickout_threshold:  consecutive tx failure threshold
 * @kickout_reason:     defined in roam_tx_failures_reason
 */
struct roam_trigger_tx_failures {
	uint32_t kickout_threshold;
	enum roam_tx_failures_reason kickout_reason;
};

/**
 * union roam_trigger_condition - union of all types roam trigger info
 * structures
 * @roam_per: per roam trigger related information
 * @roam_bmiss: bmiss roam trigger related information
 * @roam_poor_rssi: low rssi roam trigger related information
 * @roam_better_rssi: high rssi roam trigger related information
 * @roam_congestion: congestion roam trigger related information
 * @roam_user_trigger: user trigger roam related information
 * @roam_btm: btm roam trigger related information
 * @roam_bss_load: bss load roam trigger related information
 * @roam_disconnection: disconnect roam trigger related information
 * @roam_periodic: periodic roam trigger related information
 * @roam_background: background roam trigger related information
 * @roam_tx_failures: tx failures roam trigger related information
 */
union roam_trigger_condition {
	struct roam_trigger_per roam_per;
	struct roam_trigger_bmiss roam_bmiss;
	struct roam_trigger_poor_rssi roam_poor_rssi;
	struct roam_trigger_better_rssi roam_better_rssi;
	struct roam_trigger_congestion roam_congestion;
	struct roam_trigger_user_trigger roam_user_trigger;
	struct roam_trigger_btm roam_btm;
	struct roam_trigger_bss_load roam_bss_load;
	struct roam_trigger_disconnection roam_disconnection;
	struct roam_trigger_periodic roam_periodic;
	struct roam_trigger_background roam_background;
	struct roam_trigger_tx_failures roam_tx_failures;
};

/**
 * struct roam_trigger_abort_reason - roam abort related information
 * @abort_reason_code:    detail in roam_abort_reason
 * @data_rssi:            data rssi in dBm
 * @data_rssi_threshold:  data rssi threshold in dBm
 * @rx_linkspeed_status:  rx linkspeed status, 0:good linkspeed, 1:bad
 */
struct roam_trigger_abort_reason {
	enum roam_abort_reason abort_reason_code;
	int8_t data_rssi;
	int8_t data_rssi_threshold;
	bool rx_linkspeed_status;
};

/**
 * struct eroam_trigger_info - roam trigger related information
 * @timestamp: timestamp of roaming start
 * @trigger_reason: roam trigger reason, enum in roam_trigger_reason
 * @condition: roam trigger detail information
 * @abort: roam abort related information
 * @roam_scan_type: roam scan type, enum in roam_stats_scan_type
 * @roam_status: roam result, 0 - Roaming is success, 1 - Roaming is failed
 * @roam_fail_reason: roam fail reason, enum in wlan_roam_failure_reason_code
 */
struct eroam_trigger_info {
	uint64_t timestamp;
	uint32_t trigger_reason;
	union roam_trigger_condition condition;
	struct roam_trigger_abort_reason abort;
	enum roam_stats_scan_type roam_scan_type;
	uint8_t roam_status;
	enum wlan_roam_failure_reason_code roam_fail_reason;
};

/**
 * struct roam_scan_chn - each roam scan channel information
 * @chan_freq: roam scan channel frequency(MHz)
 * @dwell_type: indicate channel dwell type, enum in roam_scan_dwell_type
 * @max_dwell_time: max dwell time of each channel
 */
struct roam_scan_chn {
	uint16_t chan_freq;
	enum roam_scan_dwell_type dwell_type;
	uint32_t max_dwell_time;
};

/**
 * struct eroam_scan_info - all roam scan channel related information
 * @num_channels: total number of channels scanned during roam scan
 * @roam_chn: each roam scan channel information
 * @total_scan_time: total scan time of all roam channel
 * @original_bssid: connected AP before roam happens, regardless of
 *  the roam resulting in success or failure.
 *  For non-MLO scenario, it indicates the original connected AP BSSID.
 *  For MLO scenario, it indicates the original BSSID of the link
 *  for which the reassociation occurred during the roam.
 * @roamed_bssid: roamed AP BSSID when roam succeeds.
 *  For non-MLO case, it indicates new AP BSSID which has been
 *  successfully roamed.
 *  For MLO case, it indicates the new AP BSSID of the link on
 *  which the reassociation occurred during the roam.
 */
struct eroam_scan_info {
	uint8_t num_channels;
	struct roam_scan_chn roam_chn[MAX_ROAM_SCAN_CHAN];
	uint32_t total_scan_time;
	struct qdf_mac_addr original_bssid;
	struct qdf_mac_addr roamed_bssid;
};

/**
 * struct eroam_frame_info - frame information during roaming
 * @frame_type: frame subtype defined in eroam_frame_subtype
 * @status: frame status defined in eroam_roam_status
 * @timestamp: timestamp of the auth/assoc/eapol-M1/M2/M3/M4 frame,
 *  if status is successful, indicate received or send success,
 *  if status is failed, timestamp indicate roaming fail at that time
 * @bssid: Source address for auth/assoc/eapol frame.
 */
struct eroam_frame_info {
	enum eroam_frame_subtype frame_type;
	enum eroam_frame_status status;
	uint64_t timestamp;
	struct qdf_mac_addr bssid;

};

/**
 * struct enhance_roam_info - enhance roam information
 * @trigger: roam trigger information
 * @scan: roam scan information
 * @timestamp: types of frame information during roaming
 */
struct enhance_roam_info {
	struct eroam_trigger_info trigger;
	struct eroam_scan_info scan;
	struct eroam_frame_info timestamp[WLAN_ROAM_MAX_FRAME_INFO];
};

/**
 * struct mlme_legacy_priv - VDEV MLME legacy priv object
 * @chan_switch_in_progress: flag to indicate that channel switch is in progress
 * @hidden_ssid_restart_in_progress: flag to indicate hidden ssid restart is
 *                                   in progress
 * @vdev_start_failed: flag to indicate that vdev start failed.
 * @connection_fail: flag to indicate connection failed
 * @cac_required_for_new_channel: if CAC is required for new channel
 * @follow_ap_edca: if true, it is forced to follow the AP's edca.
 * @reconn_after_assoc_timeout: reconnect to the same AP if association timeout
 * @assoc_type: vdev associate/reassociate type
 * @dynamic_cfg: current configuration of nss, chains for vdev.
 * @ini_cfg: Max configuration of nss, chains supported for vdev.
 * @sta_dynamic_oce_value: Dynamic oce flags value for sta
 * @disconnect_info: Disconnection information
 * @vdev_stop_type: vdev stop type request
 * @mlme_roam: Roam offload state
 * @cm_roam: Roaming configuration
 * @auth_log: Cached log records for SAE authentication frame
 * related information.
 * @roam_info: enhanced roam information include trigger, scan and
 *  frame information.
 * @roam_cache_num: number of roam information cached in driver
 * @roam_write_index: indicate current write position of ring buffer
 * @roam_rd_wr_lock: protect roam buffer write and read
 * @bigtk_vdev_support: BIGTK feature support for this vdev (SAP)
 * @sae_retry: SAE auth retry information
 * @roam_reason_better_ap: roam due to better AP found
 * @hb_failure_rssi: heartbeat failure AP RSSI
 * @opr_rate_set: operational rates set
 * @ext_opr_rate_set: extended operational rates set
 * @mcs_rate_set: MCS Based rates set
 * @mscs_req_info: Information related to mscs request
 * @he_config: he config
 * @he_sta_obsspd: he_sta_obsspd
 * @twt_wait_for_notify: TWT session teardown received, wait for
 * notify event from firmware before next TWT setup is done.
 * @rso_cfg: per vdev RSO config to be sent to FW
 * @connect_info: mlme connect information
 * @wait_key_timer: wait key timer
 * @eht_config: Eht capability configuration
 * @last_delba_sent_time: Last delba sent time to handle back to back delba
 *			  requests from some IOT APs
 * @ba_2k_jump_iot_ap: This is set to true if connected to the ba 2k jump IOT AP
 * @is_usr_ps_enabled: Is Power save enabled
 * @notify_co_located_ap_upt_rnr: Notify co located AP to update RNR or not
 * @is_user_std_set: true if user set the @wifi_std
 * @wifi_std: wifi standard version
 * @max_mcs_index: Max supported mcs index of vdev
 * @vdev_traffic_type: to set if vdev is LOW_LATENCY or HIGH_TPUT
 * @country_ie_for_all_band: take all band channel info in country ie
 * @mlme_ap: SAP related vdev private configurations
 * @is_single_link_mlo_roam: Single link mlo roam flag
 * @bss_color_change_wakelock: wakelock to complete bss color change
 *				operation on bss color collision detection
 * @bss_color_change_runtime_lock: runtime lock to complete bss color change
 * @disconnect_runtime_lock: runtime lock to complete disconnection
 * @best_6g_power_type: best 6g power type
 */
struct mlme_legacy_priv {
	bool chan_switch_in_progress;
	bool hidden_ssid_restart_in_progress;
	bool vdev_start_failed;
	bool connection_fail;
	bool cac_required_for_new_channel;
	bool follow_ap_edca;
	bool reconn_after_assoc_timeout;
	enum vdev_assoc_type assoc_type;
	struct wlan_mlme_nss_chains dynamic_cfg;
	struct wlan_mlme_nss_chains ini_cfg;
	uint8_t sta_dynamic_oce_value;
	struct wlan_disconnect_info disconnect_info;
	uint32_t vdev_stop_type;
	struct wlan_mlme_roam mlme_roam;
	struct wlan_cm_roam cm_roam;
#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && \
		defined(WLAN_FEATURE_CONNECTIVITY_LOGGING)
	struct wlan_log_record
	    auth_log[MAX_ROAM_CANDIDATE_AP][WLAN_ROAM_MAX_CACHED_AUTH_FRAMES];
#elif defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(CONNECTIVITY_DIAG_EVENT)
	struct wlan_diag_packet_info
	    auth_log[MAX_ROAM_CANDIDATE_AP][WLAN_ROAM_MAX_CACHED_AUTH_FRAMES];
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef WLAN_FEATURE_ROAM_INFO_STATS
	struct enhance_roam_info *roam_info;
	uint32_t roam_cache_num;
	uint32_t roam_write_index;
	qdf_mutex_t roam_rd_wr_lock;
#endif
#endif
	bool bigtk_vdev_support;
	struct sae_auth_retry sae_retry;
	bool roam_reason_better_ap;
	uint32_t hb_failure_rssi;
	struct mlme_cfg_str opr_rate_set;
	struct mlme_cfg_str ext_opr_rate_set;
	struct mlme_cfg_str mcs_rate_set;
	bool twt_wait_for_notify;
#ifdef WLAN_FEATURE_MSCS
	struct mscs_req_info mscs_req_info;
#endif
#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_cap he_config;
	uint32_t he_sta_obsspd;
#endif
	struct mlme_connect_info connect_info;
	struct wait_for_key_timer wait_key_timer;
#ifdef WLAN_FEATURE_11BE
	tDot11fIEeht_cap eht_config;
#endif
	qdf_time_t last_delba_sent_time;
	bool ba_2k_jump_iot_ap;
	bool is_usr_ps_enabled;
	bool notify_co_located_ap_upt_rnr;
	bool is_user_std_set;
	WMI_HOST_WIFI_STANDARD wifi_std;
#ifdef WLAN_FEATURE_SON
	uint8_t max_mcs_index;
#endif
	uint8_t vdev_traffic_type;
	bool country_ie_for_all_band;
	struct mlme_ap_config mlme_ap;
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
	bool is_single_link_mlo_roam;
#endif
	qdf_wake_lock_t bss_color_change_wakelock;
	qdf_runtime_lock_t bss_color_change_runtime_lock;
	qdf_runtime_lock_t disconnect_runtime_lock;
	enum reg_6g_ap_type best_6g_power_type;
};

/**
 * struct del_bss_resp - params required for del bss response
 * @status: QDF status
 * @vdev_id: vdev_id
 */
struct del_bss_resp {
	QDF_STATUS status;
	uint8_t vdev_id;
};

/**
 * mlme_init_rate_config() - initialize rate configuration of vdev
 * @vdev_mlme: pointer to vdev mlme object
 *
 * Return: Success or Failure status
 */
QDF_STATUS mlme_init_rate_config(struct vdev_mlme_obj *vdev_mlme);

/**
 * mlme_init_connect_chan_info_config() - initialize channel info for a
 * connection
 * @vdev_mlme: pointer to vdev mlme object
 *
 * Return: Success or Failure status
 */
QDF_STATUS mlme_init_connect_chan_info_config(struct vdev_mlme_obj *vdev_mlme);

/**
 * mlme_get_peer_mic_len() - get mic hdr len and mic length for peer
 * @psoc: psoc
 * @pdev_id: pdev id for the peer
 * @peer_mac: peer mac
 * @mic_len: mic length for peer
 * @mic_hdr_len: mic header length for peer
 *
 * Return: Success or Failure status
 */
QDF_STATUS mlme_get_peer_mic_len(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id,
				 uint8_t *peer_mac, uint8_t *mic_len,
				 uint8_t *mic_hdr_len);

/**
 * mlme_peer_object_created_notification(): mlme peer create handler
 * @peer: peer which is going to created by objmgr
 * @arg: argument for vdev create handler
 *
 * Register this api with objmgr to detect peer is created
 *
 * Return: QDF_STATUS status in case of success else return error
 */

QDF_STATUS
mlme_peer_object_created_notification(struct wlan_objmgr_peer *peer,
				      void *arg);

/**
 * mlme_peer_object_destroyed_notification(): mlme peer delete handler
 * @peer: peer which is going to delete by objmgr
 * @arg: argument for vdev delete handler
 *
 * Register this api with objmgr to detect peer is deleted
 *
 * Return: QDF_STATUS status in case of success else return error
 */
QDF_STATUS
mlme_peer_object_destroyed_notification(struct wlan_objmgr_peer *peer,
					void *arg);

/**
 * mlme_get_dynamic_oce_flags(): mlme get dynamic oce flags
 * @vdev: pointer to vdev object
 *
 * This api is used to get the dynamic oce flags pointer
 *
 * Return: QDF_STATUS status in case of success else return error
 */
uint8_t *mlme_get_dynamic_oce_flags(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_dynamic_vdev_config() - get the vdev dynamic config params
 * @vdev: vdev pointer
 *
 * Return: pointer to the dynamic vdev config structure
 */
struct wlan_mlme_nss_chains *mlme_get_dynamic_vdev_config(
					struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_vdev_he_ops()  - Get vdev HE operations IE info
 * @psoc: Pointer to PSOC object
 * @vdev_id: vdev id
 *
 * Return: HE ops IE
 */
uint32_t mlme_get_vdev_he_ops(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id);

/**
 * mlme_connected_chan_stats_request() - process connected channel stats
 * request
 * @psoc: pointer to psoc object
 * @vdev_id: Vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_connected_chan_stats_request(struct wlan_objmgr_psoc *psoc,
					     uint8_t vdev_id);

/**
 * mlme_get_ini_vdev_config() - get the vdev ini config params
 * @vdev: vdev pointer
 *
 * Return: pointer to the ini vdev config structure
 */
struct wlan_mlme_nss_chains *mlme_get_ini_vdev_config(
					struct wlan_objmgr_vdev *vdev);

/**
 * mlme_cfg_on_psoc_enable() - Populate MLME structure from CFG and INI
 * @psoc: pointer to the psoc object
 *
 * Populate the MLME CFG structure from CFG and INI values using CFG APIs
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_cfg_on_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * mlme_get_psoc_ext_obj() - Get MLME object from psoc
 * @psoc: pointer to the psoc object
 *
 * Get the MLME object pointer from the psoc
 *
 * Return: pointer to MLME object
 */
#define mlme_get_psoc_ext_obj(psoc) \
			mlme_get_psoc_ext_obj_fl(psoc, __func__, __LINE__)
struct wlan_mlme_psoc_ext_obj *mlme_get_psoc_ext_obj_fl(struct wlan_objmgr_psoc
							*psoc,
							const char *func,
							uint32_t line);

/**
 * mlme_get_sae_auth_retry() - Get sae_auth_retry pointer
 * @vdev: vdev pointer
 *
 * Return: Pointer to struct sae_auth_retry or NULL
 */
struct sae_auth_retry *mlme_get_sae_auth_retry(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_free_sae_auth_retry() - Free the SAE auth info
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlme_free_sae_auth_retry(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_self_disconnect_ies() - Set diconnect IEs configured from userspace
 * @vdev: vdev pointer
 * @ie: pointer for disconnect IEs
 *
 * Return: None
 */
void mlme_set_self_disconnect_ies(struct wlan_objmgr_vdev *vdev,
				  struct element_info *ie);

/**
 * mlme_free_self_disconnect_ies() - Free the self diconnect IEs
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlme_free_self_disconnect_ies(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_self_disconnect_ies() - Get diconnect IEs from vdev object
 * @vdev: vdev pointer
 *
 * Return: Returns a pointer to the self disconnect IEs present in vdev object
 */
struct element_info *mlme_get_self_disconnect_ies(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_peer_disconnect_ies() - Cache disconnect IEs received from peer
 * @vdev: vdev pointer
 * @ie: pointer for disconnect IEs
 *
 * Return: None
 */
void mlme_set_peer_disconnect_ies(struct wlan_objmgr_vdev *vdev,
				  struct element_info *ie);

/**
 * mlme_free_peer_disconnect_ies() - Free the peer diconnect IEs
 * @vdev: vdev pointer
 *
 * Return: None
 */
void mlme_free_peer_disconnect_ies(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_follow_ap_edca_flag() - Set follow ap's edca flag
 * @vdev: vdev pointer
 * @flag: carries if following ap's edca is true or not.
 *
 * Return: None
 */
void mlme_set_follow_ap_edca_flag(struct wlan_objmgr_vdev *vdev, bool flag);

/**
 * mlme_get_follow_ap_edca_flag() - Get follow ap's edca flag
 * @vdev: vdev pointer
 *
 * Return: value of follow_ap_edca
 */
bool mlme_get_follow_ap_edca_flag(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_best_6g_power_type() - Set best 6g power type
 * @vdev: vdev pointer
 * @best_6g_power_type: best 6g power type
 *
 * Return: None
 */
void mlme_set_best_6g_power_type(struct wlan_objmgr_vdev *vdev,
				 enum reg_6g_ap_type best_6g_power_type);

/**
 * mlme_get_best_6g_power_type() - Get best 6g power type
 * @vdev: vdev pointer
 *
 * Return: value of best 6g power type
 */
enum reg_6g_ap_type mlme_get_best_6g_power_type(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_reconn_after_assoc_timeout_flag() - Set reconn after assoc timeout
 * flag
 * @psoc: soc object
 * @vdev_id: vdev id
 * @flag: enable or disable reconnect
 *
 * Return: void
 */
void mlme_set_reconn_after_assoc_timeout_flag(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id, bool flag);

/**
 * mlme_get_reconn_after_assoc_timeout_flag() - Get reconn after assoc timeout
 * flag
 * @psoc: soc object
 * @vdev_id: vdev id
 *
 * Return: true for enabling reconnect, otherwise false
 */
bool mlme_get_reconn_after_assoc_timeout_flag(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id);

/**
 * mlme_get_peer_disconnect_ies() - Get diconnect IEs from vdev object
 * @vdev: vdev pointer
 *
 * Return: Returns a pointer to the peer disconnect IEs present in vdev object
 */
struct element_info *mlme_get_peer_disconnect_ies(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_free_peer_assoc_rsp_ie() - Free the peer Assoc resp IE
 * @peer_priv: Peer priv object
 *
 * Return: None
 */
void mlme_free_peer_assoc_rsp_ie(struct peer_mlme_priv_obj *peer_priv);

/**
 * mlme_set_peer_assoc_rsp_ie() - Cache Assoc resp IE send to peer
 * @psoc: soc object
 * @peer_addr: Mac address of requesting peer
 * @ie: pointer for assoc resp IEs
 *
 * Return: None
 */
void mlme_set_peer_assoc_rsp_ie(struct wlan_objmgr_psoc *psoc,
				uint8_t *peer_addr, struct element_info *ie);

/**
 * mlme_set_peer_pmf_status() - set pmf status of peer
 * @peer: PEER object
 * @is_pmf_enabled: Carries if PMF is enabled or not
 *
 * is_pmf_enabled will be set to true if PMF is enabled by peer
 *
 * Return: void
 */
void mlme_set_peer_pmf_status(struct wlan_objmgr_peer *peer,
			      bool is_pmf_enabled);
/**
 * mlme_get_peer_pmf_status() - get if peer is of pmf capable
 * @peer: PEER object
 *
 * Return: Value of is_pmf_enabled; True if PMF is enabled by peer
 */
bool mlme_get_peer_pmf_status(struct wlan_objmgr_peer *peer);

/**
 * wlan_get_opmode_from_vdev_id() - Get opmode from vdevid
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 *
 * Return: opmode
 */
enum QDF_OPMODE wlan_get_opmode_from_vdev_id(struct wlan_objmgr_pdev *pdev,
					     uint8_t vdev_id);

/**
 * wlan_mlme_get_bssid_vdev_id() - get bss peer mac address(BSSID) using vdev id
 * @pdev: pdev
 * @vdev_id: vdev_id
 * @bss_peer_mac: pointer to bss_peer_mac_address
 *
 * This API is used to get mac address of bss peer/bssid.
 *
 * Context: Any context.
 *
 * Return: QDF_STATUS based on overall success
 */
QDF_STATUS wlan_mlme_get_bssid_vdev_id(struct wlan_objmgr_pdev *pdev,
				       uint8_t vdev_id,
				       struct qdf_mac_addr *bss_peer_mac);

/**
 * mlme_update_freq_in_scan_start_req() - Fill frequencies in wide
 * band scan req for mlo connection
 * @vdev: vdev common object
 * @req: pointer to scan request
 * @scan_ch_width: Channel width for which to trigger a wide band scan
 * @scan_freq: frequency for which to trigger a wide band RRM scan
 * @cen320_freq: 320 MHz center freq
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_update_freq_in_scan_start_req(struct wlan_objmgr_vdev *vdev,
				   struct scan_start_request *req,
				   enum phy_ch_width scan_ch_width,
				   qdf_freq_t scan_freq,
				   qdf_freq_t cen320_freq);

/**
 * wlan_get_operation_chan_freq() - get operating chan freq of
 * given vdev
 * @vdev: vdev
 *
 * Return: chan freq of given vdev id
 */
qdf_freq_t wlan_get_operation_chan_freq(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_get_operation_chan_freq_vdev_id() - get operating chan freq of
 * given vdev id
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 *
 * Return: chan freq of given vdev id
 */
qdf_freq_t wlan_get_operation_chan_freq_vdev_id(struct wlan_objmgr_pdev *pdev,
						uint8_t vdev_id);

/**
 * wlan_vdev_set_dot11mode - Set the dot11mode of the vdev
 * @mac_mlme_cfg: MAC's MLME config pointer
 * @device_mode: OPMODE of the vdev
 * @vdev_mlme: MLME component of the vdev
 *
 * Use this API to set the dot11mode of the vdev.
 * For non-ML type vdev, this API restricts the connection
 * of vdev to 11ax on 11be capable operation.
 *
 * Return: void
 */
void wlan_vdev_set_dot11mode(struct wlan_mlme_cfg *mac_mlme_cfg,
			     enum QDF_OPMODE device_mode,
			     struct vdev_mlme_obj *vdev_mlme);

/**
 * wlan_is_open_wep_cipher() - check if cipher is open or WEP
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 *
 * Return: if cipher is open or WEP
 */
bool wlan_is_open_wep_cipher(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id);

/**
 * wlan_vdev_id_is_open_cipher() - check if cipher is open
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 *
 * Return: if cipher is open
 */
bool wlan_vdev_id_is_open_cipher(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id);

/**
 * wlan_vdev_is_open_mode() - check if cipher is open
 * @vdev: Pointer to vdev
 *
 * Return: if cipher is open
 */
bool wlan_vdev_is_open_mode(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_vdev_id_is_11n_allowed() - check if 11n allowed
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 *
 * Return: false if cipher is TKIP or WEP
 */
bool wlan_vdev_id_is_11n_allowed(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id);

/**
 * wlan_is_vdev_id_up() - check if vdev id is in UP state
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 *
 * Return: if vdev is up
 */
bool wlan_is_vdev_id_up(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id);

QDF_STATUS
wlan_get_op_chan_freq_info_vdev_id(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id, qdf_freq_t *op_freq,
				   qdf_freq_t *freq_seg_0,
				   enum phy_ch_width *ch_width);

/**
 * wlan_strip_ie() - strip requested IE from IE buffer
 * @addn_ie: Additional IE buffer
 * @addn_ielen: Length of additional IE
 * @eid: EID of IE to strip
 * @size_of_len_field: length of IE length field
 * @oui: if present matches OUI also
 * @oui_length: if previous present, this is length of oui
 * @extracted_ie: if not NULL, copy the stripped IE to this buffer
 * @eid_max_len: maximum length of IE @eid
 *
 * This utility function is used to strip of the requested IE if present
 * in IE buffer.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_strip_ie(uint8_t *addn_ie, uint16_t *addn_ielen,
			 uint8_t eid, enum size_of_len_field size_of_len_field,
			 uint8_t *oui, uint8_t oui_length,
			 uint8_t *extracted_ie, uint32_t eid_max_len);

/**
 * wlan_is_channel_present_in_list() - check if rfeq is present in the list
 * given vdev id
 * @freq_lst: given freq list
 * @num_chan: num of chan freq
 * @chan_freq: chan freq to check
 *
 * Return: chan freq of given vdev id
 */
bool wlan_is_channel_present_in_list(qdf_freq_t *freq_lst,
				     uint32_t num_chan, qdf_freq_t chan_freq);

/**
 * wlan_roam_is_channel_valid() - validate channel frequency
 * @reg: regulatory context
 * @chan_freq: channel frequency
 *
 * This function validates channel frequency present in valid channel
 * list or not.
 *
 * Return: true or false
 */
bool wlan_roam_is_channel_valid(struct wlan_mlme_reg *reg,
				qdf_freq_t chan_freq);

int8_t wlan_get_cfg_max_tx_power(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_pdev *pdev,
				 uint32_t ch_freq);

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * mlme_get_supplicant_disabled_roaming() - Get supplicant disabled roaming
 *  value for a given vdev.
 * @psoc: PSOC pointer
 * @vdev_id: Vdev for which the supplicant disabled roaming value is being
 *  requested
 *
 * Return: True if supplicant disabled roaming else false
 */
bool
mlme_get_supplicant_disabled_roaming(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id);

/**
 * mlme_set_supplicant_disabled_roaming - Set the supplicant disabled
 *  roaming flag.
 * @psoc: PSOC pointer
 * @vdev_id: Vdev for which the supplicant disabled roaming needs to
 *  be set
 * @val: value true is to disable RSO and false to enable RSO
 *
 * Return: None
 */
void mlme_set_supplicant_disabled_roaming(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id, bool val);

/**
 * mlme_get_roam_trigger_bitmap() - Get roaming trigger bitmap value for a given
 *  vdev.
 * @psoc: PSOC pointer
 * @vdev_id: Vdev for which the roam trigger bitmap is being requested
 *
 * Return: roaming trigger bitmap
 */
uint32_t
mlme_get_roam_trigger_bitmap(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id);

/**
 * mlme_set_roam_trigger_bitmap() - Set the roaming trigger bitmap value for
 *  the given vdev. If the bitmap is zero then roaming is completely disabled
 *  on the vdev which means roam structure in firmware is not allocated and no
 *  RSO start/stop commands can be sent
 * @psoc: PSOC pointer
 * @vdev_id: Vdev for which the roam trigger bitmap is to be set
 * @val: bitmap value to set
 *
 * Return: None
 */
void mlme_set_roam_trigger_bitmap(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id, uint32_t val);

/**
 * mlme_get_roam_state() - Get roam state from vdev object
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 *
 * Return: Returns roam offload state
 */
enum roam_offload_state
mlme_get_roam_state(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id);

/**
 * mlme_set_roam_state() - Set roam state in vdev object
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @val: roam offload state
 *
 * Return: None
 */
void mlme_set_roam_state(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			 enum roam_offload_state val);

/**
 * mlme_get_operations_bitmap() - Get the mlme operations bitmap which
 *  contains the bitmap of mlme operations which have disabled roaming
 *  temporarily
 * @psoc: PSOC pointer
 * @vdev_id: vdev for which the mlme operation bitmap is requested
 *
 * Return: bitmap value
 */
uint8_t
mlme_get_operations_bitmap(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id);

/**
 * mlme_set_operations_bitmap() - Set the mlme operations bitmap which
 *  indicates what mlme operations are in progress
 * @psoc: PSOC pointer
 * @vdev_id: vdev for which the mlme operation bitmap is requested
 * @reqs: RSO stop requestor
 * @clear: clear bit if true else set bit
 *
 * Return: None
 */
void
mlme_set_operations_bitmap(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   enum wlan_cm_rso_control_requestor reqs, bool clear);
/**
 * mlme_clear_operations_bitmap() - Clear mlme operations bitmap which
 *  indicates what mlme operations are in progress
 * @psoc: PSOC pointer
 * @vdev_id: vdev for which the mlme operation bitmap is requested
 *
 * Return: None
 */
void
mlme_clear_operations_bitmap(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id);

/**
 * mlme_get_cfg_wlm_level() - Get the WLM level value
 * @psoc: pointer to psoc object
 * @level: level that needs to be filled.
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_get_cfg_wlm_level(struct wlan_objmgr_psoc *psoc,
				  uint8_t *level);

#ifdef MULTI_CLIENT_LL_SUPPORT
/**
 * mlme_get_cfg_multi_client_ll_ini_support() - Get the ini value of wlm multi
 * client latency level feature
 * @psoc: pointer to psoc object
 * @multi_client_ll_support: parameter that needs to be filled.
 *
 * Return: QDF Status
 */
QDF_STATUS
mlme_get_cfg_multi_client_ll_ini_support(struct wlan_objmgr_psoc *psoc,
					 bool *multi_client_ll_support);
#else
static inline QDF_STATUS
mlme_get_cfg_multi_client_ll_ini_support(struct wlan_objmgr_psoc *psoc,
					 bool *multi_client_ll_support)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * mlme_get_cfg_wlm_reset() - Get the WLM reset flag
 * @psoc: pointer to psoc object
 * @reset: reset that needs to be filled.
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_get_cfg_wlm_reset(struct wlan_objmgr_psoc *psoc,
				  bool *reset);

#define MLME_IS_ROAM_STATE_RSO_ENABLED(psoc, vdev_id) \
	(mlme_get_roam_state(psoc, vdev_id) == WLAN_ROAM_RSO_ENABLED)

#define MLME_IS_ROAM_STATE_DEINIT(psoc, vdev_id) \
	(mlme_get_roam_state(psoc, vdev_id) == WLAN_ROAM_DEINIT)

#define MLME_IS_ROAM_STATE_INIT(psoc, vdev_id) \
	(mlme_get_roam_state(psoc, vdev_id) == WLAN_ROAM_INIT)

#define MLME_IS_ROAM_STATE_STOPPED(psoc, vdev_id) \
	(mlme_get_roam_state(psoc, vdev_id) == WLAN_ROAM_RSO_STOPPED)

#define MLME_IS_ROAM_INITIALIZED(psoc, vdev_id) \
	(mlme_get_roam_state(psoc, vdev_id) >= WLAN_ROAM_INIT)
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#define MLME_IS_ROAMING_IN_PROG(psoc, vdev_id) \
	(mlme_get_roam_state(psoc, vdev_id) == WLAN_ROAMING_IN_PROG)

#define MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) \
	(mlme_get_roam_state(psoc, vdev_id) == WLAN_ROAM_SYNCH_IN_PROG)

#else
#define MLME_IS_ROAMING_IN_PROG(psoc, vdev_id) (false)
#define MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) (false)
#endif

#if defined (WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
#define MLME_IS_MLO_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) \
		(mlme_get_roam_state(psoc, vdev_id) == WLAN_MLO_ROAM_SYNCH_IN_PROG)
#else
#define MLME_IS_MLO_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) (false)
#endif

/**
 * mlme_reinit_control_config_lfr_params() - Reinitialize roam control config
 * @psoc: PSOC pointer
 * @lfr: Pointer of an lfr_cfg buffer to fill.
 *
 * Reinitialize/restore the param related control roam config lfr params with
 * default values of corresponding ini params.
 *
 * Return: None
 */
void mlme_reinit_control_config_lfr_params(struct wlan_objmgr_psoc *psoc,
					   struct wlan_mlme_lfr_cfg *lfr);

/**
 * wlan_mlme_get_mac_vdev_id() - get vdev self mac address using vdev id
 * @pdev: pdev
 * @vdev_id: vdev_id
 * @self_mac: pointer to self_mac_address
 *
 * This API is used to get self mac address.
 *
 * Context: Any context.
 *
 * Return: QDF_STATUS based on overall success
 */
QDF_STATUS wlan_mlme_get_mac_vdev_id(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id,
				     struct qdf_mac_addr *self_mac);

/**
 * wlan_acquire_peer_key_wakelock -api to get key wakelock
 * @pdev: pdev
 * @mac_addr: peer mac addr
 *
 * This function acquires wakelock and prevent runtime pm during key
 * installation
 *
 * Return: None
 */
void wlan_acquire_peer_key_wakelock(struct wlan_objmgr_pdev *pdev,
				    uint8_t *mac_addr);

/**
 * wlan_release_peer_key_wakelock -api to release key wakelock
 * @pdev: pdev
 * @mac_addr: peer mac addr
 *
 * This function releases wakelock and allow runtime pm after key
 * installation
 *
 * Return: None
 */
void wlan_release_peer_key_wakelock(struct wlan_objmgr_pdev *pdev,
				    uint8_t *mac_addr);

/**
 * wlan_get_sap_user_config_freq() - Get the user configured frequency
 *
 * @vdev: pointer to vdev
 *
 * Return: User configured sap frequency.
 */
qdf_freq_t
wlan_get_sap_user_config_freq(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_set_sap_user_config_freq() - Set the user configured frequency
 *
 * @vdev: pointer to vdev
 * @freq: user configured SAP frequency
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_set_sap_user_config_freq(struct wlan_objmgr_vdev *vdev,
			      qdf_freq_t freq);

#if defined(WLAN_FEATURE_11BE_MLO)
/**
 * wlan_clear_mlo_sta_link_removed_flag() - Clear link removal flag on all
 * vdev of same ml dev
 * @vdev: pointer to vdev
 *
 * Return: void
 */
void wlan_clear_mlo_sta_link_removed_flag(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_get_mlo_link_agnostic_flag() - Update mlo link agnostic flag
 *
 * @vdev: pointer to vdev
 * @dest_addr: destination address
 *
 * Return: true/false
 */
bool wlan_get_mlo_link_agnostic_flag(struct wlan_objmgr_vdev *vdev,
				     uint8_t *dest_addr);
/**
 * wlan_set_vdev_link_removed_flag_by_vdev_id() - Set link removal flag
 * on vdev
 * @psoc: psoc object
 * @vdev_id: vdev id
 * @removed: link removal flag
 *
 * Return: QDF_STATUS_SUCCESS if success, otherwise error code
 */
QDF_STATUS
wlan_set_vdev_link_removed_flag_by_vdev_id(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id, bool removed);

/**
 * wlan_get_vdev_link_removed_flag_by_vdev_id() - Get link removal flag
 * of vdev
 * @psoc: psoc object
 * @vdev_id: vdev id
 *
 * Return: true if link is removed on vdev, otherwise false.
 */
bool
wlan_get_vdev_link_removed_flag_by_vdev_id(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id);

/**
 * wlan_drop_mgmt_frame_on_link_removal() - Check mgmt frame
 * allow dropped due to link removal
 * @vdev: pointer to vdev
 *
 * Return: true if frame can be dropped.
 */
bool wlan_drop_mgmt_frame_on_link_removal(struct wlan_objmgr_vdev *vdev);
#else
static inline void
wlan_clear_mlo_sta_link_removed_flag(struct wlan_objmgr_vdev *vdev)
{
}

static inline
bool wlan_get_mlo_link_agnostic_flag(struct wlan_objmgr_vdev *vdev,
				     uint8_t *dest_addr)
{
	return false;
}

static inline QDF_STATUS
wlan_set_vdev_link_removed_flag_by_vdev_id(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id, bool removed)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool
wlan_get_vdev_link_removed_flag_by_vdev_id(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id)
{
	return false;
}

static inline bool
wlan_drop_mgmt_frame_on_link_removal(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

#ifdef CONFIG_BAND_6GHZ
/**
 * wlan_get_tpc_update_required_for_sta() - Get the tpc update required config
 * to identify whether the tpc power has changed for concurrent STA interface
 *
 * @vdev: pointer to SAP vdev
 *
 * Return: Change scc power config
 */
bool
wlan_get_tpc_update_required_for_sta(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_set_tpc_update_required_for_sta() - Set the tpc update required config
 * for the concurrent STA interface
 *
 * @vdev:   pointer to SAP vdev
 * @value:  change scc power config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_set_tpc_update_required_for_sta(struct wlan_objmgr_vdev *vdev, bool value);
#else
static inline bool
wlan_get_tpc_update_required_for_sta(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline QDF_STATUS
wlan_set_tpc_update_required_for_sta(struct wlan_objmgr_vdev *vdev, bool value)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_mlme_get_sta_num_tx_chains() - API to get station num tx chains
 *
 * @psoc: psoc context
 * @vdev: pointer to vdev
 * @tx_chains : tx_chains out parameter
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_sta_num_tx_chains(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				uint8_t *tx_chains);

/**
 * wlan_mlme_get_sta_tx_nss() - API to get station tx NSS
 *
 * @psoc: psoc context
 * @vdev: pointer to vdev
 * @tx_nss : tx_nss out parameter
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_sta_tx_nss(struct wlan_objmgr_psoc *psoc,
			 struct wlan_objmgr_vdev *vdev,
			 uint8_t *tx_nss);

/**
 * wlan_mlme_get_sta_num_rx_chains() - API to get station num rx chains
 *
 * @psoc: psoc context
 * @vdev: pointer to vdev
 * @rx_chains : rx_chains out parameter
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_sta_num_rx_chains(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				uint8_t *rx_chains);

/**
 * wlan_mlme_get_sta_rx_nss() - API to get station rx NSS
 *
 * @psoc: psoc context
 * @vdev: pointer to vdev
 * @rx_nss : rx_nss out parameter
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_sta_rx_nss(struct wlan_objmgr_psoc *psoc,
			 struct wlan_objmgr_vdev *vdev,
			 uint8_t *rx_nss);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_mlme_defer_pmk_set_in_roaming() - Set the set_key pending status
 *
 * @psoc: pointer to psoc
 * @vdev_id: vdev id
 * @set_pmk_pending: set_key pending status
 *
 * Return: None
 */
void
wlan_mlme_defer_pmk_set_in_roaming(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool set_pmk_pending);

/**
 * wlan_mlme_is_pmk_set_deferred() - Get the set_key pending status
 *
 * @psoc: pointer to psoc
 * @vdev_id: vdev id
 *
 * Return : set_key pending status
 */
bool
wlan_mlme_is_pmk_set_deferred(struct wlan_objmgr_psoc *psoc,
			      uint8_t vdev_id);
#else
static inline void
wlan_mlme_defer_pmk_set_in_roaming(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool set_pmk_pending)
{
}

static inline bool
wlan_mlme_is_pmk_set_deferred(struct wlan_objmgr_psoc *psoc,
			      uint8_t vdev_id)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_SAE
/**
 * wlan_vdev_is_sae_auth_type() - is vdev SAE auth type
 * @vdev: pointer to vdev
 *
 * Return: true if vdev is SAE auth type
 */
bool wlan_vdev_is_sae_auth_type(struct wlan_objmgr_vdev *vdev);
#endif /* WLAN_FEATURE_SAE */

/**
 * wlan_get_rand_from_lst_for_freq()- Get random channel from a given channel
 * list.
 * @freq_lst: Frequency list
 * @num_chan: number of channels
 *
 * Get random channel from given channel list.
 *
 * Return: channel frequency.
 */
uint16_t wlan_get_rand_from_lst_for_freq(uint16_t *freq_lst,
					 uint8_t num_chan);
#if defined WLAN_FEATURE_SR
/**
 * mlme_sr_update() - MLME sr update callback
 * @vdev: vdev object
 * @enable: true or false
 *
 * This function is called to update the SR threshold
 */
void mlme_sr_update(struct wlan_objmgr_vdev *vdev, bool enable);

/**
 * mlme_sr_is_enable: Check whether SR is enabled or not
 * @vdev: object manager vdev
 *
 * Return: True/False
 */
int mlme_sr_is_enable(struct wlan_objmgr_vdev *vdev);
#else
static inline void mlme_sr_update(struct wlan_objmgr_vdev *vdev, bool enable)
{
}

static inline int mlme_sr_is_enable(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}
#endif /* WLAN_FEATURE_SR */

/**
 * mlme_peer_oper_mode_change_event_handler() - Handle peer oper mode event
 * @scn: handle
 * @event: Event data received from firmware
 * @len: Event data length received from firmware
 */
int mlme_peer_oper_mode_change_event_handler(ol_scn_t scn, uint8_t *event,
					     uint32_t len);

/**
 * wmi_extract_peer_oper_mode_event() - Extract the peer operating
 * mode change event and update the new bandwidth
 * @wmi_handle: wmi handle
 * @event: Event data received from firmware
 * @len: Event data length received from firmware
 * @data: Extract the event and fill in data
 */
QDF_STATUS
wmi_extract_peer_oper_mode_event(wmi_unified_t wmi_handle,
				 uint8_t *event,
				 uint32_t len,
				 struct peer_oper_mode_event *data);

/**
 * wlan_mlme_register_rx_ops - Target IF mlme API to register mlme
 * related rx op.
 * @rx_ops: Pointer to rx ops fp struct
 *
 * Return: none
 */
void wlan_mlme_register_rx_ops(struct wlan_mlme_rx_ops *rx_ops);

/**
 * mlme_get_rx_ops - Get mlme rx ops from psoc
 * @psoc: psoc
 *
 * Return: Pointer to rx ops fp struct
 */
struct wlan_mlme_rx_ops *
mlme_get_rx_ops(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_mlme_register_common_events - Wrapper to register common events
 * @psoc: psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_register_common_events(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS
wlan_mlme_register_common_events(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_mlme_send_csa_event_status_ind_cmd() - send csa event status indication
 * @vdev: vdev obj
 * @csa_status: csa status
 *
 *  Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_send_csa_event_status_ind_cmd(struct wlan_objmgr_vdev *vdev,
					uint8_t csa_status);

/**
 * wlan_mlme_get_sap_psd_for_20mhz() - Get the PSD power for 20 MHz
 * frequency
 * @vdev: pointer to vdev object
 *
 * Return: psd power
 */
uint8_t wlan_mlme_get_sap_psd_for_20mhz(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlme_set_sap_psd_for_20mhz() - Set the PSD power for 20 MHz
 * frequency
 * @vdev: pointer to vdev object
 * @psd_power : psd power
 *
 * Return: None
 */
QDF_STATUS wlan_mlme_set_sap_psd_for_20mhz(struct wlan_objmgr_vdev *vdev,
					   uint8_t psd_power);
#endif
