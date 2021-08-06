/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
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

#if !defined(__SME_API_H)
#define __SME_API_H

/**
 * file  smeApi.h
 *
 * brief prototype for SME APIs
 */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "csr_api.h"
#include "qdf_lock.h"
#include "qdf_types.h"
#include "sir_api.h"
#include "cds_regdomain.h"
#include "sme_internal.h"
#include "wma_tgt_cfg.h"
#include "wma_fips_public_structs.h"
#include "wma_sar_public_structs.h"

#include "sme_rrm_internal.h"
#include "sir_types.h"
#include "scheduler_api.h"
#include "wlan_serialization_legacy_api.h"
#include "wmi_unified.h"
#include "wmi_unified_param.h"

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#define SME_SUMMARY_STATS         (1 << eCsrSummaryStats)
#define SME_GLOBAL_CLASSA_STATS   (1 << eCsrGlobalClassAStats)
#define SME_GLOBAL_CLASSD_STATS   (1 << eCsrGlobalClassDStats)
#define SME_PER_CHAIN_RSSI_STATS  (1 << csr_per_chain_rssi_stats)

#define sme_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_SME, params)
#define sme_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_SME, params)
#define sme_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_SME, params)
#define sme_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_SME, params)
#define sme_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_SME, params)

#define sme_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_SME, params)
#define sme_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_SME, params)
#define sme_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_SME, params)
#define sme_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_SME, params)
#define sme_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_SME, params)

#define sme_alert_rl(params...) QDF_TRACE_FATAL_RL(QDF_MODULE_ID_SME, params)
#define sme_err_rl(params...) QDF_TRACE_ERROR_RL(QDF_MODULE_ID_SME, params)
#define sme_warn_rl(params...) QDF_TRACE_WARN_RL(QDF_MODULE_ID_SME, params)
#define sme_info_rl(params...) QDF_TRACE_INFO_RL(QDF_MODULE_ID_SME, params)
#define sme_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_SME, params)

#define SME_ENTER() sme_debug("enter")
#define SME_EXIT() sme_debug("exit")

#define SME_SESSION_ID_ANY        50
#define SME_SESSION_ID_BROADCAST  0xFF

#define SME_INVALID_COUNTRY_CODE "XX"
#define INVALID_ROAM_ID 0

#define SME_SET_CHANNEL_REG_POWER(reg_info_1, val) do {	\
	reg_info_1 &= 0xff00ffff;	      \
	reg_info_1 |= ((val & 0xff) << 16);   \
} while (0)

#define SME_SET_CHANNEL_MAX_TX_POWER(reg_info_2, val) do { \
	reg_info_2 &= 0xffff00ff;	      \
	reg_info_2 |= ((val & 0xff) << 8);   \
} while (0)

#define SME_CONFIG_TO_ROAM_CONFIG 1
#define ROAM_CONFIG_TO_SME_CONFIG 2

#define NUM_OF_BANDS 2

#define SME_ACTIVE_LIST_CMD_TIMEOUT_VALUE (30*1000)
#define SME_CMD_TIMEOUT_VALUE (SME_ACTIVE_LIST_CMD_TIMEOUT_VALUE + 1000)

/* SME timeout for Start/Stop BSS commands is set to 10 secs */
#define SME_START_STOP_BSS_CMD_TIMEOUT (10 * 1000)
#define SME_CMD_START_STOP_BSS_TIMEOUT (SME_START_STOP_BSS_CMD_TIMEOUT + 1000)

/* SME timeout for vdev delete is set to 10 secs */
#define SME_VDEV_DELETE_CMD_TIMEOUT (10 * 1000)
#define SME_CMD_VDEV_CREATE_DELETE_TIMEOUT (SME_VDEV_DELETE_CMD_TIMEOUT + 5000)

/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/
typedef struct _smeConfigParams {
	tCsrConfigParam csrConfig;
	struct rrm_config_param rrmConfig;
	bool snr_monitor_enabled;
} tSmeConfigParams, *tpSmeConfigParams;

#ifdef FEATURE_WLAN_TDLS
#define SME_TDLS_MAX_SUPP_CHANNELS       128
#define SME_TDLS_MAX_SUPP_OPER_CLASSES   32

typedef struct _smeTdlsPeerCapParams {
	uint8_t isPeerResponder;
	uint8_t peerUapsdQueue;
	uint8_t peerMaxSp;
	uint8_t peerBuffStaSupport;
	uint8_t peerOffChanSupport;
	uint8_t peerCurrOperClass;
	uint8_t selfCurrOperClass;
	uint8_t peerChanLen;
	uint8_t peerChan[SME_TDLS_MAX_SUPP_CHANNELS];
	uint8_t peerOperClassLen;
	uint8_t peerOperClass[SME_TDLS_MAX_SUPP_OPER_CLASSES];
	uint8_t prefOffChanNum;
	uint8_t prefOffChanBandwidth;
	uint8_t opClassForPrefOffChan;
} tSmeTdlsPeerCapParams;

/**
 * eSmeTdlsPeerState - tdls peer state
 * @eSME_TDLS_PEER_STATE_PEERING: tdls connection in progress
 * @eSME_TDLS_PEER_STATE_CONNECTED: tdls peer is connected
 * @eSME_TDLS_PEER_STATE_TEARDOWN: tdls peer is tear down
 * @eSME_TDLS_PEER_ADD_MAC_ADDR: add peer mac into connection table
 * @eSME_TDLS_PEER_REMOVE_MAC_ADDR: remove peer mac from connection table
 */
typedef enum {
	eSME_TDLS_PEER_STATE_PEERING,
	eSME_TDLS_PEER_STATE_CONNECTED,
	eSME_TDLS_PEER_STATE_TEARDOWN,
	eSME_TDLS_PEER_ADD_MAC_ADDR,
	eSME_TDLS_PEER_REMOVE_MAC_ADDR,
} eSmeTdlsPeerState;

typedef struct _smeTdlsPeerStateParams {
	uint32_t vdevId;
	tSirMacAddr peerMacAddr;
	uint32_t peerState;
	tSmeTdlsPeerCapParams peerCap;
} tSmeTdlsPeerStateParams;

#define ENABLE_CHANSWITCH  1
#define DISABLE_CHANSWITCH 2
#define BW_20_OFFSET_BIT   0
#define BW_40_OFFSET_BIT   1
#define BW_80_OFFSET_BIT   2
#define BW_160_OFFSET_BIT  3
typedef struct sme_tdls_chan_switch_param_struct {
	uint32_t vdev_id;
	tSirMacAddr peer_mac_addr;
	uint16_t tdls_off_ch_bw_offset;/* Target Off Channel Bandwidth offset */
	uint8_t tdls_off_channel;      /* Target Off Channel */
	uint8_t tdls_off_ch_mode;      /* TDLS Off Channel Mode */
	uint8_t is_responder;          /* is peer responder or initiator */
	uint8_t opclass;           /* tdls operating class */
} sme_tdls_chan_switch_params;
#endif /* FEATURE_WLAN_TDLS */

/* Thermal Mitigation*/
typedef struct {
	uint16_t smeMinTempThreshold;
	uint16_t smeMaxTempThreshold;
} tSmeThermalLevelInfo;

#define SME_MAX_THERMAL_LEVELS (4)
#define SME_MAX_THROTTLE_LEVELS (4)

typedef struct {
	/* Array of thermal levels */
	tSmeThermalLevelInfo smeThermalLevels[SME_MAX_THERMAL_LEVELS];
	uint8_t smeThermalMgmtEnabled;
	uint32_t smeThrottlePeriod;
	uint8_t sme_throttle_duty_cycle_tbl[SME_MAX_THROTTLE_LEVELS];
} tSmeThermalParams;

typedef enum {
	SME_AC_BK = 0,
	SME_AC_BE = 1,
	SME_AC_VI = 2,
	SME_AC_VO = 3
} sme_ac_enum_type;

/*
 * Enumeration of the various TSPEC directions
 * From 802.11e/WMM specifications
 */
enum sme_qos_wmm_dir_type {
	SME_QOS_WMM_TS_DIR_UPLINK = 0,
	SME_QOS_WMM_TS_DIR_DOWNLINK = 1,
	SME_QOS_WMM_TS_DIR_RESV = 2,    /* Reserved */
	SME_QOS_WMM_TS_DIR_BOTH = 3,
};

/**
 * struct sme_oem_capability - OEM capability to be exchanged between host
 *                             and userspace
 * @ftm_rr: FTM range report capability bit
 * @lci_capability: LCI capability bit
 * @reserved1: reserved
 * @reserved2: reserved
 */
struct sme_oem_capability {
	uint32_t ftm_rr:1;
	uint32_t lci_capability:1;
	uint32_t reserved1:30;
	uint32_t reserved2;
};

/**
 * struct sme_5g_pref_params : 5G preference params to be read from ini
 * @rssi_boost_threshold_5g: RSSI threshold above which 5 GHz is favored
 * @rssi_boost_factor_5g: Factor by which 5GHz RSSI is boosted
 * @max_rssi_boost_5g: Maximum boost that can be applied to 5GHz RSSI
 * @rssi_penalize_threshold_5g: RSSI threshold below which 5G is not favored
 * @rssi_penalize_factor_5g: Factor by which 5GHz RSSI is penalized
 * @max_rssi_penalize_5g: Maximum penalty that can be applied to 5G RSSI
 */
struct sme_5g_band_pref_params {
	int8_t      rssi_boost_threshold_5g;
	uint8_t     rssi_boost_factor_5g;
	uint8_t     max_rssi_boost_5g;
	int8_t      rssi_penalize_threshold_5g;
	uint8_t     rssi_penalize_factor_5g;
	uint8_t     max_rssi_penalize_5g;
};

/**
 * struct sme_session_params: Session creation params passed by HDD layer
 * @session_open_cb: callback to be registered with SME for opening the session
 * @session_close_cb: callback to be registered with SME for closing the session
 * @callback: callback to be invoked for roaming events
 * @callback_ctx: user-supplied context to be passed back on roaming events
 * @self_mac_addr: Self mac address
 * @sme_session_id: SME session id
 * @type_of_persona: person type
 * @subtype_of_persona: sub type of persona
 */
struct sme_session_params {
	csr_session_open_cb  session_open_cb;
	csr_session_close_cb session_close_cb;
	csr_roam_complete_cb callback;
	void *callback_ctx;
	uint8_t *self_mac_addr;
	uint8_t sme_session_id;
	uint32_t type_of_persona;
	uint32_t subtype_of_persona;
};

#define MAX_CANDIDATE_INFO 10

/**
 * struct bss_candidate_info - Candidate bss information
 *
 * @bssid : BSSID of candidate bss
 * @status : status code for candidate bss
 */
struct bss_candidate_info {
	struct qdf_mac_addr bssid;
	uint32_t status;
};

/*
 * MBO transition reason codes
 */
enum {
	MBO_TRANSITION_REASON_UNSPECIFIED,
	MBO_TRANSITION_REASON_EXCESSIVE_FRAME_LOSS_RATE,
	MBO_TRANSITION_REASON_EXCESSIVE_DELAY_FOR_CURRENT_TRAFFIC,
	MBO_TRANSITION_REASON_INSUFFICIENT_BANDWIDTH_FOR_CURRENT_TRAFFIC,
	MBO_TRANSITION_REASON_LOAD_BALANCING,
	MBO_TRANSITION_REASON_LOW_RSSI,
	MBO_TRANSITION_REASON_RECEIVED_EXCESSIVE_RETRANSMISSIONS,
	MBO_TRANSITION_REASON_HIGH_INTERFERENCE,
	MBO_TRANSITION_REASON_GRAY_ZONE,
	MBO_TRANSITION_REASON_TRANSITIONING_TO_PREMIUM_AP,
};

/*-------------------------------------------------------------------------
  Function declarations and documenation
  ------------------------------------------------------------------------*/
QDF_STATUS sme_open(tHalHandle hHal);
QDF_STATUS sme_init_chan_list(tHalHandle hal, uint8_t *alpha2,
		enum country_src cc_src);
QDF_STATUS sme_close(tHalHandle hHal);
QDF_STATUS sme_start(tHalHandle hHal);

/**
 * sme_stop() - Stop all SME modules and put them at idle state
 * @mac_handle: Opaque handle to the MAC context
 *
 * The function stops each module in SME. Upon return, all modules are
 * at idle state ready to start.
 *
 * This is a synchronous call
 *
 * Return: QDF_STATUS_SUCCESS if SME is stopped.  Other status means
 *         SME failed to stop one or more modules but caller should
 *         still consider SME is stopped.
 */
QDF_STATUS sme_stop(mac_handle_t mac_handle);

/*
 * sme_open_session() - Open a session for given persona
 *
 * This is a synchronous API. For any protocol stack related activity
 * requires session to be opened. This API needs to be called to open
 * the session in SME module.
 *
 * hal: The handle returned by mac_open.
 * params: to initialize the session open params
 *
 * Return:
 * QDF_STATUS_SUCCESS - session is opened.
 * Other status means SME is failed to open the session.
 */
QDF_STATUS sme_open_session(tHalHandle hal, struct sme_session_params *params);

/*
 * sme_close_session() - Close a session for given persona
 *
 * This is a synchronous API. This API needs to be called to close the session
 * in SME module before terminating the session completely.
 *
 * hal: The handle returned by mac_open.
 * session_id: A previous opened session's ID.
 *
 * Return:
 * QDF_STATUS_SUCCESS - session is closed.
 * Other status means SME is failed to open the session.
 */
QDF_STATUS sme_close_session(tHalHandle hal, uint8_t sessionId);

/**
 * sme_cleanup_session() -  clean up sme session info for vdev
 * @mac_handle: mac handle
 * @vdev_id: vdev id
 *
 * Return: none
 */
void sme_cleanup_session(mac_handle_t mac_handle, uint8_t vdev_id);

void sme_set_curr_device_mode(tHalHandle hHal,
		enum QDF_OPMODE currDeviceMode);
QDF_STATUS sme_update_roam_params(tHalHandle hHal, uint8_t session_id,
		struct roam_ext_params *roam_params_src, int update_param);
QDF_STATUS sme_update_config(tHalHandle hHal,
		tpSmeConfigParams pSmeConfigParams);

QDF_STATUS sme_set11dinfo(tHalHandle hHal, tpSmeConfigParams pSmeConfigParams);
QDF_STATUS sme_get_soft_ap_domain(tHalHandle hHal,
		v_REGDOMAIN_t *domainIdSoftAp);
QDF_STATUS sme_hdd_ready_ind(tHalHandle hHal);

#ifdef WLAN_BCN_RECV_FEATURE
/*
 * sme_register_bcn_report_pe_cb() - Register SME callback
 * @mac_handle: The handle returned by mac_open.
 * @cb: cb of type beacon_report_cb
 *
 * This function Register SME callback in order to send
 * beacon report to upper layer
 *
 * Return QDF_STATUS_SUCCESS -
 */
QDF_STATUS
sme_register_bcn_report_pe_cb(mac_handle_t mac_handle, beacon_report_cb cb);
#else
static inline QDF_STATUS
sme_register_bcn_report_pe_cb(mac_handle_t mac_handle, beacon_report_cb cb)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * sme_ser_cmd_callback() - callback from serialization module
 * @cmd: serialization command
 * @reason: reason why serialization module has given this callback
 *
 * Serialization module will give callback to SME for why it triggered
 * the callback
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS sme_ser_cmd_callback(struct wlan_serialization_command *cmd,
				enum wlan_serialization_cb_reason reason);

/**
 * sme_purge_pdev_all_ser_cmd_list_sync() - purge all scan and non-scan
 * active and pending cmds for pdev
 * @mac_handle: pointer to global MAC context
 * @cb: callback to hdd
 *
 * Return : QDF_STATUS
 */
QDF_STATUS sme_purge_pdev_all_ser_cmd_list_sync(mac_handle_t mac_handle,
						sir_purge_pdev_cmd_cb cb);

/*
 * sme_process_msg() - The main message processor for SME.
 * @mac: The global mac context
 * @msg: The message to be processed.
 *
 * This function is called by a message dispatcher when to process a message
 * targeted for SME.
 * This is a synchronous call
 *
 * Return: QDF_STATUS_SUCCESS - SME successfully processed the message.
 * Other status means SME failed to process the message to HAL.
 */
QDF_STATUS sme_process_msg(tpAniSirGlobal pMac, struct scheduler_msg *pMsg);

QDF_STATUS sme_mc_process_handler(struct scheduler_msg *msg);
QDF_STATUS sme_scan_get_result(tHalHandle hHal, uint8_t sessionId,
		tCsrScanResultFilter *pFilter,
		tScanResultHandle *phResult);
QDF_STATUS sme_get_ap_channel_from_scan_cache(
		struct csr_roam_profile *profile,
		tScanResultHandle *scan_cache,
		uint8_t *ap_chnl_id);
QDF_STATUS sme_get_ap_channel_from_scan(void *profile,
		tScanResultHandle *scan_cache,
		uint8_t *ap_chnl_id);
bool sme_store_joinreq_param(tHalHandle hal_handle,
		struct csr_roam_profile *profile,
		tScanResultHandle scan_cache,
		uint32_t *roam_id,
		uint32_t session_id);
bool sme_clear_joinreq_param(tHalHandle hal_handle,
		uint32_t session_id);
QDF_STATUS sme_issue_stored_joinreq(tHalHandle hal_handle,
		uint32_t *roam_id,
		uint32_t session_id);
QDF_STATUS sme_scan_flush_result(tHalHandle hHal);
QDF_STATUS sme_filter_scan_results(tHalHandle hHal, uint8_t sessionId);
QDF_STATUS sme_scan_flush_p2p_result(tHalHandle hHal, uint8_t sessionId);
tCsrScanResultInfo *sme_scan_result_get_first(tHalHandle,
		tScanResultHandle hScanResult);
tCsrScanResultInfo *sme_scan_result_get_next(tHalHandle,
		tScanResultHandle hScanResult);
QDF_STATUS sme_scan_result_purge(tScanResultHandle hScanResult);
QDF_STATUS sme_roam_connect(tHalHandle hHal, uint8_t sessionId,
		struct csr_roam_profile *pProfile, uint32_t *pRoamId);
QDF_STATUS sme_roam_reassoc(tHalHandle hHal, uint8_t sessionId,
		struct csr_roam_profile *pProfile,
		tCsrRoamModifyProfileFields modProfileFields,
		uint32_t *pRoamId, bool fForce);
QDF_STATUS sme_roam_connect_to_last_profile(tHalHandle hHal, uint8_t sessionId);

/**
 * sme_roam_disconnect() - API to request CSR to disconnect
 * @hal: HAL context
 * @session: SME session identifier
 * @reason: Reason to disconnect
 * @mac_reason: Reason to disconnect as per enum eSirMacReasonCodes
 *
 * Return: QDF Status success or failure
 */
QDF_STATUS sme_roam_disconnect(tHalHandle hal, uint8_t session,
			       eCsrRoamDisconnectReason reason,
			       tSirMacReasonCodes mac_reason);

void sme_dhcp_done_ind(tHalHandle hal, uint8_t session_id);
QDF_STATUS sme_roam_stop_bss(tHalHandle hHal, uint8_t sessionId);
QDF_STATUS sme_roam_get_associated_stas(tHalHandle hHal, uint8_t sessionId,
		QDF_MODULE_ID modId, void *pUsrContext,
		void *pfnSapEventCallback,
		uint8_t *pAssocStasBuf);
QDF_STATUS sme_roam_disconnect_sta(tHalHandle hHal, uint8_t sessionId,
		struct csr_del_sta_params *p_del_sta_params);
QDF_STATUS sme_roam_deauth_sta(tHalHandle hHal, uint8_t sessionId,
		struct csr_del_sta_params *pDelStaParams);
QDF_STATUS sme_roam_get_connect_state(tHalHandle hHal, uint8_t sessionId,
		eCsrConnectState *pState);
QDF_STATUS sme_roam_get_connect_profile(tHalHandle hHal, uint8_t sessionId,
		tCsrRoamConnectedProfile *pProfile);
void sme_roam_free_connect_profile(tCsrRoamConnectedProfile *profile);
QDF_STATUS sme_roam_set_pmkid_cache(tHalHandle hHal, uint8_t sessionId,
		tPmkidCacheInfo *pPMKIDCache,
		uint32_t numItems,
		bool update_entire_cache);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * sme_set_roam_scan_ch_event_cb() - Register roam scan ch callback
 * @mac_handle: Opaque handle to the MAC context
 * @cb: callback to be registered
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_set_roam_scan_ch_event_cb(mac_handle_t mac_handle,
			      sme_get_raom_scan_ch_Callback cb);

/**
 * sme_get_roam_scan_ch() sme api to get roam scan channel list
 * @hal: hal handle
 * @vdev_id: vdev id
 * @pcontext: pointer to cookie
 *
 * This API is used to get roam scan channels form firmware.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_roam_scan_ch(tHalHandle hal, uint8_t vdev_id,
				void *pcontext);

/**
 * sme_get_pmk_info(): A wrapper function to request CSR to save PMK
 * @hal: Global structure
 * @session_id: SME session_id
 * @pmk_cache: pointer to a structure of pmk
 *
 * Return: none
 */
void sme_get_pmk_info(tHalHandle hal, uint8_t session_id,
		      tPmkidCacheInfo *pmk_cache);

QDF_STATUS sme_roam_set_psk_pmk(tHalHandle hHal, uint8_t sessionId,
		uint8_t *pPSK_PMK, size_t pmk_len);
#else
static inline QDF_STATUS
sme_get_roam_scan_ch(tHalHandle hal, uint8_t vdev_id,
		     void *pcontext)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
sme_set_roam_scan_ch_event_cb(mac_handle_t mac_handle,
			      void *cb)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * sme_roam_get_wpa_rsn_req_ie() - Retrieve WPA/RSN Request IE
 * @hal: HAL handle
 * @session_id: ID of the specific session
 * @len: Caller allocated memory that has the length of @buf as input.
 *	Upon returned, @len has the length of the IE store in @buf
 * @buf: Caller allocated memory that contain the IE field, if any,
 *	upon return
 *
 * A wrapper function to request CSR to return the WPA or RSN IE CSR
 * passes to PE to JOIN request or START_BSS request
 * This is a synchronous call.
 *
 * Return: QDF_STATUS - when fail, it usually means the buffer allocated is not
 *			 big enough
 */
QDF_STATUS sme_roam_get_wpa_rsn_req_ie(tHalHandle hal, uint8_t session_id,
				       uint32_t *len, uint8_t *buf);

/**
 * sme_roam_get_wpa_rsn_rsp_ie() - Retrieve WPA/RSN Response IE
 * @hal: HAL handle
 * @session_id: ID of the specific session
 * @len: Caller allocated memory that has the length of @buf as input.
 *	Upon returned, @len has the length of the IE store in @buf
 * @buf: Caller allocated memory that contain the IE field, if any,
 *	upon return
 *
 * A wrapper function to request CSR to return the WPA or RSN IE CSR
 * passes to PE to JOIN request or START_BSS request
 * This is a synchronous call.
 *
 * Return: QDF_STATUS - when fail, it usually means the buffer allocated is not
 *			 big enough
 */
QDF_STATUS sme_roam_get_wpa_rsn_rsp_ie(tHalHandle hal, uint8_t session_id,
				       uint32_t *len, uint8_t *buf);

uint32_t sme_roam_get_num_pmkid_cache(tHalHandle hHal, uint8_t sessionId);
QDF_STATUS sme_roam_get_pmkid_cache(tHalHandle hHal, uint8_t sessionId,
		uint32_t *pNum,
		tPmkidCacheInfo *pPmkidCache);
QDF_STATUS sme_get_config_param(tHalHandle hHal, tSmeConfigParams *pParam);
#ifndef QCA_SUPPORT_CP_STATS
QDF_STATUS sme_get_statistics(tHalHandle hHal,
		eCsrStatsRequesterType requesterId,
		uint32_t statsMask, tCsrStatsCallback callback,
		uint8_t staId, void *pContext, uint8_t sessionId);
#endif
QDF_STATUS sme_get_rssi(tHalHandle hHal,
		tCsrRssiCallback callback,
		uint8_t staId, struct qdf_mac_addr bssId, int8_t lastRSSI,
		void *pContext);
QDF_STATUS sme_get_snr(tHalHandle hHal,
		tCsrSnrCallback callback,
		uint8_t staId, struct qdf_mac_addr bssId, void *pContext);

#ifdef FEATURE_WLAN_ESE
QDF_STATUS sme_get_tsm_stats(tHalHandle hHal,
		tCsrTsmStatsCallback callback,
		uint8_t staId, struct qdf_mac_addr bssId,
		void *pContext, uint8_t tid);
QDF_STATUS sme_set_cckm_ie(tHalHandle hHal,
		uint8_t sessionId,
		uint8_t *pCckmIe, uint8_t cckmIeLen);
QDF_STATUS sme_set_ese_beacon_request(tHalHandle hHal, const uint8_t sessionId,
		const tCsrEseBeaconReq *pEseBcnReq);
QDF_STATUS sme_set_plm_request(tHalHandle hHal, tpSirPlmReq pPlm);
#endif /*FEATURE_WLAN_ESE */
QDF_STATUS sme_cfg_set_int(tHalHandle hal, uint16_t cfg_id, uint32_t value);
QDF_STATUS sme_cfg_set_str(tHalHandle hal, uint16_t cfg_id, uint8_t *str,
		uint32_t length);
QDF_STATUS sme_cfg_get_int(tHalHandle hal, uint16_t cfg_id,
		uint32_t *cfg_value);
QDF_STATUS sme_cfg_get_str(tHalHandle hal, uint16_t cfg_id, uint8_t *str,
		uint32_t *length);
QDF_STATUS sme_get_modify_profile_fields(tHalHandle hHal, uint8_t sessionId,
					 tCsrRoamModifyProfileFields *
					 pModifyProfileFields);

extern QDF_STATUS sme_set_host_power_save(tHalHandle hHal, bool psMode);

void sme_set_dhcp_till_power_active_flag(tHalHandle hHal, uint8_t flag);

#ifdef FEATURE_OEM_DATA_SUPPORT
extern QDF_STATUS sme_register_oem_data_rsp_callback(tHalHandle h_hal,
		sme_send_oem_data_rsp_msg callback);
void sme_deregister_oem_data_rsp_callback(tHalHandle h_hal);

#else
static inline QDF_STATUS sme_register_oem_data_rsp_callback(tHalHandle hal,
		void *callback)
{
	return QDF_STATUS_SUCCESS;
}
static inline void sme_deregister_oem_data_rsp_callback(tHalHandle h_hal)
{
}

#endif

QDF_STATUS sme_roam_set_key(tHalHandle, uint8_t sessionId,
		tCsrRoamSetKey *pSetKey, uint32_t *pRoamId);
QDF_STATUS sme_get_country_code(tHalHandle hHal, uint8_t *pBuf, uint8_t *pbLen);


/* some support functions */
bool sme_is11d_supported(tHalHandle hHal);
bool sme_is11h_supported(tHalHandle hHal);
bool sme_is_wmm_supported(tHalHandle hHal);

QDF_STATUS sme_generic_change_country_code(tHalHandle hHal,
					   uint8_t *pCountry);

/**
 * sme_store_nss_chains_cfg_in_vdev() - fill vdev nss chain params from ini
 * @vdev: Pointer to vdev obj
 * @vdev_ini_cfg: pointer to the structure the values are to be filled from
 *
 * This API will copy the nss chain params for the particular vdev from ini
 * configuration to the respective vdev's dynamic, and ini config.
 *
 * Return: none
 */
void
sme_store_nss_chains_cfg_in_vdev(struct wlan_objmgr_vdev *vdev,
				 struct mlme_nss_chains *vdev_ini_cfg);

/**
 * sme_nss_chains_update() - validate and send the user params to fw
 * @mac_handle: The handle returned by mac_open.
 * @user_cfg: pointer to the structure to be validated and sent to fw
 * @vdev_id: vdev id
 *
 *
 * This API will validate the config, and if found correct will update the
 * config in dynamic config, and send to the fw.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_nss_chains_update(mac_handle_t mac_handle,
		      struct mlme_nss_chains *user_cfg,
		      uint8_t vdev_id);

/**
 * sme_update_channel_list() - Update configured channel list to fwr
 * This is a synchronous API.
 * @hal: HAL handle returned by mac_open.
 *
 * Return: QDF_STATUS  SUCCESS.
 * FAILURE or RESOURCES  The API finished and failed.
 */
QDF_STATUS sme_update_channel_list(tHalHandle hal);

QDF_STATUS sme_tx_fail_monitor_start_stop_ind(tHalHandle hHal,
		uint8_t tx_fail_count,
		void *txFailIndCallback);
QDF_STATUS sme_dhcp_start_ind(tHalHandle hHal,
		uint8_t device_mode,
		uint8_t *macAddr, uint8_t sessionId);
QDF_STATUS sme_dhcp_stop_ind(tHalHandle hHal,
		uint8_t device_mode,
		uint8_t *macAddr, uint8_t sessionId);
void sme_get_recovery_stats(tHalHandle hHal);
QDF_STATUS sme_neighbor_report_request(tHalHandle hHal, uint8_t sessionId,
		tpRrmNeighborReq pRrmNeighborReq,
		tpRrmNeighborRspCallbackInfo callbackInfo);
QDF_STATUS sme_get_wcnss_wlan_compiled_version(tHalHandle hHal,
		tSirVersionType * pVersion);
QDF_STATUS sme_get_wcnss_wlan_reported_version(tHalHandle hHal,
		tSirVersionType *pVersion);
QDF_STATUS sme_get_wcnss_software_version(tHalHandle hHal,
		uint8_t *pVersion, uint32_t versionBufferSize);
QDF_STATUS sme_get_wcnss_hardware_version(tHalHandle hHal,
		uint8_t *pVersion, uint32_t versionBufferSize);

#ifdef FEATURE_OEM_DATA
/**
 * sme_oem_data_cmd() - the wrapper to send oem data cmd to wma
 * @mac_handle: Opaque handle to the global MAC context.
 * @@oem_data_event_handler_cb: callback to be registered
 * @oem_data: the pointer of oem data
 * @vdev id: vdev id to fetch adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_oem_data_cmd(mac_handle_t mac_handle,
			    void (*oem_data_event_handler_cb)
			    (const struct oem_data *oem_event_data,
			     uint8_t vdev_id),
			     struct oem_data *oem_data,
			     uint8_t vdev_id);
#endif

#ifdef FEATURE_OEM_DATA_SUPPORT
/**
 * sme_oem_req_cmd() - send oem request cmd to WMA
 * @mac_handle: Opaque handle to the global MAC context
 * @oem_req: OEM data request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_oem_req_cmd(mac_handle_t mac_handle,
			   struct oem_data_req *oem_req);
QDF_STATUS sme_oem_update_capability(tHalHandle hHal,
				     struct sme_oem_capability *cap);
QDF_STATUS sme_oem_get_capability(tHalHandle hHal,
				  struct sme_oem_capability *cap);
#endif /*FEATURE_OEM_DATA_SUPPORT */
QDF_STATUS sme_change_mcc_beacon_interval(uint8_t sessionId);
QDF_STATUS sme_set_host_offload(tHalHandle hHal, uint8_t sessionId,
		tpSirHostOffloadReq pRequest);
QDF_STATUS sme_set_keep_alive(tHalHandle hHal, uint8_t sessionId,
		tpSirKeepAliveReq pRequest);
QDF_STATUS sme_get_operation_channel(tHalHandle hHal, uint32_t *pChannel,
		uint8_t sessionId);
QDF_STATUS sme_register_mgmt_frame(tHalHandle hHal, uint8_t sessionId,
		uint16_t frameType, uint8_t *matchData,
		uint16_t matchLen);
QDF_STATUS sme_deregister_mgmt_frame(tHalHandle hHal, uint8_t sessionId,
		uint16_t frameType, uint8_t *matchData,
		uint16_t matchLen);
QDF_STATUS sme_ConfigureAppsCpuWakeupState(tHalHandle hHal, bool isAppsAwake);
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
QDF_STATUS sme_configure_ext_wow(tHalHandle hHal,
		tpSirExtWoWParams wlanExtParams,
		csr_readyToSuspendCallback callback,
		void *callbackContext);
QDF_STATUS sme_configure_app_type1_params(tHalHandle hHal,
		tpSirAppType1Params wlanAppType1Params);
QDF_STATUS sme_configure_app_type2_params(tHalHandle hHal,
		tpSirAppType2Params wlanAppType2Params);
#endif
int8_t sme_get_infra_session_id(tHalHandle hHal);
uint8_t sme_get_infra_operation_channel(tHalHandle hHal, uint8_t sessionId);
uint8_t sme_get_concurrent_operation_channel(tHalHandle hHal);
/**
 * sme_get_beaconing_concurrent_operation_channel() - To get concurrent
 * operating channel of beaconing interface
 * @hal: Pointer to hal context
 * @vdev_id_to_skip: channel of which vdev id to skip
 *
 * This routine will return operating channel of active AP/GO channel
 * and will skip the channel of vdev_id_to_skip.
 * If other no reqested mode is active it will return 0
 *
 * Return: uint8_t
 */
uint8_t sme_get_beaconing_concurrent_operation_channel(tHalHandle hal,
						       uint8_t vdev_id_to_skip);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
uint16_t sme_check_concurrent_channel_overlap(tHalHandle hHal, uint16_t sap_ch,
		eCsrPhyMode sapPhyMode,
		uint8_t cc_switch_mode);
#endif
QDF_STATUS sme_get_cfg_valid_channels(uint8_t *aValidChannels,
		uint32_t *len);
#ifdef WLAN_FEATURE_PACKET_FILTERING
QDF_STATUS sme_8023_multicast_list(tHalHandle hHal, uint8_t sessionId,
		tpSirRcvFltMcAddrList pMulticastAddrs);
#endif /* WLAN_FEATURE_PACKET_FILTERING */
bool sme_is_channel_valid(tHalHandle hHal, uint8_t channel);
QDF_STATUS sme_set_freq_band(tHalHandle hHal, uint8_t sessionId,
		enum band_info eBand);
QDF_STATUS sme_get_freq_band(tHalHandle hHal, enum band_info *pBand);
uint16_t sme_chn_to_freq(uint8_t chanNum);
bool sme_is_channel_valid(tHalHandle hHal, uint8_t channel);
QDF_STATUS sme_set_max_tx_power(tHalHandle hHal, struct qdf_mac_addr pBssid,
		struct qdf_mac_addr pSelfMacAddress, int8_t dB);
QDF_STATUS sme_set_max_tx_power_per_band(enum band_info band, int8_t db);
QDF_STATUS sme_set_tx_power(tHalHandle hHal, uint8_t sessionId,
		struct qdf_mac_addr bssid,
		enum QDF_OPMODE dev_mode, int power);
QDF_STATUS sme_set_custom_mac_addr(tSirMacAddr customMacAddr);
QDF_STATUS sme_hide_ssid(tHalHandle hHal, uint8_t sessionId,
		uint8_t ssidHidden);
QDF_STATUS sme_set_tm_level(tHalHandle hHal, uint16_t newTMLevel,
		uint16_t tmMode);
void sme_feature_caps_exchange(tHalHandle hHal);
void sme_disable_feature_capablity(uint8_t feature_index);
void sme_reset_power_values_for5_g(tHalHandle hHal);
QDF_STATUS sme_update_roam_prefer5_g_hz(tHalHandle hHal, bool nRoamPrefer5GHz);
QDF_STATUS sme_set_roam_intra_band(tHalHandle hHal, const bool nRoamIntraBand);

/**
 * sme_update_roam_scan_n_probes() - Update no.of roam scan probes
 * @mac_handle: The handle returned by mac_open
 * @vdev_id: vdev identifier
 * @probes: number of probe requests to be sent out
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_roam_scan_n_probes(tHalHandle hHal,
					 uint8_t vdev_id,
					 const uint8_t probes);

/**
 * sme_update_roam_scan_home_away_time() - Update roam scan Home away time
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev identifier
 * @roam_scan_home_away_time: Scan home away time
 * @send_offload_cmd: If it's true, the command is sent to firmware,
 *		      otherwise the command is not sent to firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_update_roam_scan_home_away_time(mac_handle_t mac_handle, uint8_t vdev_id,
				    const uint16_t roam_scan_home_away_time,
				    const bool send_offload_cmd);

/**
 * sme_get_roam_scan_home_away_time() - get Roam scan home away time
 * @mac_handle: The handle returned by mac_open
 * @vdev_id: vdev identifier
 * @roam_scan_home_away_time: Buffer to fill the roam scan home away time.
 *			      Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_roam_scan_home_away_time(mac_handle_t mac_handle,
					    uint8_t vdev_id,
					    uint16_t *roam_scan_home_away_time);

bool sme_get_roam_intra_band(tHalHandle hHal);

/**
 * sme_get_roam_scan_n_probes() - get Roam scan number of probes
 * @mac_handle: The handle returned by mac_open
 * @vdev_id: vdev identifier
 * @roam_scan_n_probes: Buffer to fill the number of probes.
 *			Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_roam_scan_n_probes(mac_handle_t mac_handle, uint8_t vdev_id,
				      uint8_t *roam_scan_n_probes);

/**
 * sme_update_roam_rssi_diff() - Update RoamRssiDiff
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev identifier
 * @roam_rssi_diff: Minimum rssi difference between potential candidate and
 *		    current AP.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_roam_rssi_diff(mac_handle_t mac_handle, uint8_t vdev_id,
				     uint8_t roam_rssi_diff);
QDF_STATUS sme_update_fast_transition_enabled(tHalHandle hHal,
		bool isFastTransitionEnabled);
QDF_STATUS sme_update_wes_mode(tHalHandle hHal, bool isWESModeEnabled,
		uint8_t sessionId);
QDF_STATUS sme_set_roam_scan_control(tHalHandle hHal, uint8_t sessionId,
		bool roamScanControl);

QDF_STATUS sme_update_is_fast_roam_ini_feature_enabled(tHalHandle hHal,
		uint8_t sessionId,
		const bool
		isFastRoamIniFeatureEnabled);

QDF_STATUS sme_config_fast_roaming(tHalHandle hal, uint8_t session_id,
		const bool is_fast_roam_enabled);

QDF_STATUS sme_update_is_mawc_ini_feature_enabled(tHalHandle hHal,
		const bool MAWCEnabled);
QDF_STATUS sme_stop_roaming(tHalHandle hHal, uint8_t sessionId, uint8_t reason);

QDF_STATUS sme_start_roaming(tHalHandle hHal, uint8_t sessionId,
		uint8_t reason);
QDF_STATUS sme_update_enable_fast_roam_in_concurrency(tHalHandle hHal,
		bool bFastRoamInConIniFeatureEnabled);
#ifdef FEATURE_WLAN_ESE
QDF_STATUS sme_update_is_ese_feature_enabled(tHalHandle hHal, uint8_t sessionId,
		const bool isEseIniFeatureEnabled);
#endif /* FEATURE_WLAN_ESE */
QDF_STATUS sme_set_roam_rescan_rssi_diff(tHalHandle hHal,
		uint8_t sessionId,
		const uint8_t nRoamRescanRssiDiff);
uint8_t sme_get_roam_rescan_rssi_diff(tHalHandle hHal);

QDF_STATUS sme_set_roam_opportunistic_scan_threshold_diff(tHalHandle hHal,
		uint8_t sessionId,
		const uint8_t nOpportunisticThresholdDiff);
uint8_t sme_get_roam_opportunistic_scan_threshold_diff(tHalHandle hHal);
QDF_STATUS sme_set_delay_before_vdev_stop(tHalHandle hHal,
		uint8_t sessionId, uint8_t delay_before_vdev_stop);

/**
 * sme_set_neighbor_lookup_rssi_threshold() - update neighbor lookup rssi thr
 * @mac_handle: The handle returned by mac_open
 * @vdev_id: vdev identifier
 * @neighbor_lookup_rssi_threshold: Neighbor lookup rssi threshold
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_set_neighbor_lookup_rssi_threshold(mac_handle_t mac_handle,
				       uint8_t vdev_id,
				       uint8_t neighbor_lookup_rssi_threshold);

/**
 * sme_get_neighbor_lookup_rssi_threshold() - get neighbor lookup rssi threshold
 * @mac_handle: The handle returned by mac_open
 * @vdev_id: vdev identifier
 * @lookup_threshold: Buffer to fill the neighbor lookup threshold.
 *		      Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_neighbor_lookup_rssi_threshold(mac_handle_t mac_handle,
						  uint8_t vdev_id,
						  uint8_t *lookup_threshold);
QDF_STATUS sme_set_neighbor_scan_refresh_period(tHalHandle hHal,
		uint8_t sessionId, uint16_t neighborScanResultsRefreshPeriod);
uint16_t sme_get_neighbor_scan_refresh_period(tHalHandle hHal);

/**
 * sme_get_empty_scan_refresh_period_global() - get global scan refresh period
 * @mac_handle: The handle returned by mac_open
 *
 * Return: Empty scan refresh period configured through ini
 */
uint16_t sme_get_empty_scan_refresh_period_global(mac_handle_t mac_handle);

/**
 * sme_get_empty_scan_refresh_period() - get empty scan refresh period
 * @mac_handle: The handle returned by mac_open.
 * @vdev_id: vdev identifier
 * @refresh_threshold: Buffer to fill the empty scan refresh period.
 *		       Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_empty_scan_refresh_period(mac_handle_t mac_handle,
					     uint8_t vdev_id,
					     uint16_t *refresh_threshold);
QDF_STATUS sme_update_empty_scan_refresh_period(tHalHandle hHal,
		uint8_t sessionId, uint16_t nEmptyScanRefreshPeriod);
/**
 * sme_update_full_roam_scan_period() - Send full roam scan period to SME
 * @mac_handle: Opaque handle to the MAC context
 * @vdev_id: vdev id
 * @full_roam_scan_period: Idle period in seconds between two successive
 * full channel roam scans
 *
 * Updated full scan period in roam info and a roam_offload_scan request.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_full_roam_scan_period(mac_handle_t mac_handle,
					    uint8_t vdev_id,
					    uint32_t full_roam_scan_period);

/**
 * sme_modify_roam_cand_sel_criteria() - Modify candidate selection criteria
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev Identifier
 * @enable_scoring_for_roam: Carries enable/disable indication
 *
 * Enable/disable scoring for roam candidate selection based on the value of
 * enable_scoring_for_roam. Below is the description of enable/disable,
 * Disable-0: Disable scoring for roam candidate selection. Roaming
 *	      shall fallback to legacy selection criteria, only RSSI.
 * Enable-1 : Enable scoring for roam candidate selection.
 *
 * Return: Success or failure
 */
QDF_STATUS
sme_modify_roam_cand_sel_criteria(mac_handle_t mac_handle,
				  uint8_t vdev_id,
				  bool enable_scoring_for_roam);
QDF_STATUS sme_set_neighbor_scan_min_chan_time(tHalHandle hHal,
		const uint16_t nNeighborScanMinChanTime,
		uint8_t sessionId);
QDF_STATUS sme_set_neighbor_scan_max_chan_time(tHalHandle hHal,
		uint8_t sessionId, const uint16_t nNeighborScanMaxChanTime);
uint16_t sme_get_neighbor_scan_min_chan_time(tHalHandle hHal,
		uint8_t sessionId);
uint32_t sme_get_neighbor_roam_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_current_roam_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_current_roam_sub_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_lim_sme_state(tHalHandle hHal);
uint32_t sme_get_lim_mlm_state(tHalHandle hHal);
bool sme_is_lim_session_valid(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_lim_sme_session_state(tHalHandle hHal, uint8_t sessionId);
uint32_t sme_get_lim_mlm_session_state(tHalHandle hHal, uint8_t sessionId);
uint16_t sme_get_neighbor_scan_max_chan_time(tHalHandle hHal,
		uint8_t sessionId);
QDF_STATUS sme_set_neighbor_scan_period(tHalHandle hHal, uint8_t sessionId,
		const uint16_t nNeighborScanPeriod);
uint16_t sme_get_neighbor_scan_period(tHalHandle hHal, uint8_t sessionId);
QDF_STATUS sme_set_neighbor_scan_min_period(tHalHandle h_hal,
		uint8_t session_id, const uint16_t neighbor_scan_min_period);
QDF_STATUS sme_set_roam_bmiss_first_bcnt(tHalHandle hHal,
		uint8_t sessionId, const uint8_t nRoamBmissFirstBcnt);
uint8_t sme_get_roam_bmiss_first_bcnt(tHalHandle hHal);
QDF_STATUS sme_set_roam_bmiss_final_bcnt(tHalHandle hHal, uint8_t sessionId,
		const uint8_t nRoamBmissFinalBcnt);
uint8_t sme_get_roam_bmiss_final_bcnt(tHalHandle hHal);
QDF_STATUS sme_set_roam_beacon_rssi_weight(tHalHandle hHal, uint8_t sessionId,
		const uint8_t nRoamBeaconRssiWeight);
uint8_t sme_get_roam_beacon_rssi_weight(tHalHandle hHal);
void sme_free_blacklist(tHalHandle mac_handle);

/**
 * sme_get_roam_rssi_diff() - get Roam rssi diff
 * @mac_handle: The handle returned by mac_open
 * @vdev_id: vdev identifier
 * @rssi_diff: Buffer to fill the roam RSSI diff.
 *	       Valid only if the return status is success.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_roam_rssi_diff(mac_handle_t mac_handle, uint8_t vdev_id,
				  uint8_t *rssi_diff);
QDF_STATUS sme_change_roam_scan_channel_list(tHalHandle hHal, uint8_t sessionId,
		uint8_t *pChannelList,
		uint8_t numChannels);

/**
 * sme_update_roam_scan_freq_list() - Update roam scan freq list
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev identifier
 * @freq_list: List of frequencies to be configured
 * @num_channels: Number of frequencies to be configured
 * @freq_list_type: Type of frequency list to be configured to
 *
 * Update the frequencies from freq_list to the corresponding channel list
 * in neighborRoamInfo
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_update_roam_scan_freq_list(mac_handle_t mac_handle, uint8_t vdev_id,
			       uint32_t *freq_list, uint8_t num_chan,
			       uint32_t freq_list_type);
QDF_STATUS sme_set_ese_roam_scan_channel_list(tHalHandle hHal,
		uint8_t sessionId, uint8_t *pChannelList,
		uint8_t numChannels);
QDF_STATUS sme_get_roam_scan_channel_list(tHalHandle hHal,
		uint8_t *pChannelList, uint8_t *pNumChannels,
		uint8_t sessionId);
/**
 * sme_dump_chan_list() - Dump the channels from given chan info
 * @chan_info: Contains the channel list and number of frequencies
 *
 * Extract number of channels and channel list from chan_info and print
 * to the logs.
 *
 * Return: None
 */
void sme_dump_chan_list(tCsrChannelInfo *chan_info);
bool sme_get_is_ese_feature_enabled(tHalHandle hHal);
bool sme_get_wes_mode(tHalHandle hHal);
bool sme_get_roam_scan_control(tHalHandle hHal);
bool sme_get_is_lfr_feature_enabled(tHalHandle hHal);
bool sme_get_is_ft_feature_enabled(tHalHandle hHal);
QDF_STATUS sme_update_roam_scan_offload_enabled(tHalHandle hHal,
		bool nRoamScanOffloadEnabled);
bool sme_is_feature_supported_by_fw(enum cap_bitmap feature);

/*
 * SME API to enable/disable WLAN driver initiated SSR
 */
void sme_update_enable_ssr(tHalHandle hHal, bool enableSSR);
QDF_STATUS sme_set_phy_mode(tHalHandle hHal, eCsrPhyMode phyMode);
eCsrPhyMode sme_get_phy_mode(tHalHandle hHal);
QDF_STATUS sme_handoff_request(tHalHandle hHal, uint8_t sessionId,
			       tCsrHandoffRequest *pHandoffInfo);
QDF_STATUS sme_is_sta_p2p_client_connected(tHalHandle hHal);
QDF_STATUS sme_add_periodic_tx_ptrn(tHalHandle hHal, tSirAddPeriodicTxPtrn
		*addPeriodicTxPtrnParams);
QDF_STATUS sme_del_periodic_tx_ptrn(tHalHandle hHal, tSirDelPeriodicTxPtrn
		*delPeriodicTxPtrnParams);
QDF_STATUS sme_send_rate_update_ind(tHalHandle hHal,
		tSirRateUpdateInd *rateUpdateParams);
QDF_STATUS sme_roam_del_pmkid_from_cache(tHalHandle hHal, uint8_t sessionId,
		tPmkidCacheInfo *pmksa, bool flush_cache);

void sme_get_command_q_status(tHalHandle hHal);

QDF_STATUS sme_enable_rmc(tHalHandle hHal, uint32_t sessionId);
QDF_STATUS sme_disable_rmc(tHalHandle hHal, uint32_t sessionId);
QDF_STATUS sme_send_rmc_action_period(tHalHandle hHal, uint32_t sessionId);
QDF_STATUS sme_request_ibss_peer_info(tHalHandle hHal, void *pUserData,
	pIbssPeerInfoCb peerInfoCbk, bool allPeerInfoReqd, uint8_t staIdx);
QDF_STATUS sme_send_cesium_enable_ind(tHalHandle hHal, uint32_t sessionId);

#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
/**
 * sme_send_dscp_up_map_to_fw() - send DSCP-to-UP map values to fw
 * @dscp_to_up_map: array of DSCP-to-UP map values
 *
 * Used to send DSCP-to-UP map values to fw
 *
 * Return QDF_STATUS
 */
QDF_STATUS sme_send_dscp_up_map_to_fw(uint32_t *dscp_to_up_map);
#else
static inline
QDF_STATUS sme_send_dscp_up_map_to_fw(uint32_t *dscp_to_up_map)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * sme_set_wlm_latency_level_ind() - Used to set the latency level to fw
 * @hal
 * @session_id
 * @latency_level
 *
 * Return QDF_STATUS
 */
QDF_STATUS sme_set_wlm_latency_level(tHalHandle hal,
				     uint16_t session_id,
				     uint16_t latency_level);
/*
 * SME API to enable/disable idle mode powersave
 * This should be called only if powersave offload
 * is enabled
 */
QDF_STATUS sme_set_idle_powersave_config(bool value);
QDF_STATUS sme_notify_modem_power_state(tHalHandle hHal, uint32_t value);

/*SME API to convert convert the ini value to the ENUM used in csr and MAC*/
ePhyChanBondState sme_get_cb_phy_state_from_cb_ini_value(uint32_t cb_ini_value);
int sme_update_ht_config(tHalHandle hHal, uint8_t sessionId, uint16_t htCapab,
		int value);
int16_t sme_get_ht_config(tHalHandle hHal, uint8_t session_id,
		uint16_t ht_capab);
#ifdef QCA_HT_2040_COEX
QDF_STATUS sme_notify_ht2040_mode(tHalHandle hHal, uint16_t staId,
		struct qdf_mac_addr macAddrSTA,
		uint8_t sessionId,
		uint8_t channel_type);
QDF_STATUS sme_set_ht2040_mode(tHalHandle hHal, uint8_t sessionId,
		uint8_t channel_type, bool obssEnabled);
#endif
QDF_STATUS sme_get_reg_info(tHalHandle hHal, uint8_t chanId,
		uint32_t *regInfo1, uint32_t *regInfo2);
#ifdef FEATURE_WLAN_TDLS
QDF_STATUS sme_update_fw_tdls_state(tHalHandle hHal, void *psmeTdlsParams,
		bool useSmeLock);
#endif /* FEATURE_WLAN_TDLS */

#ifdef FEATURE_WLAN_CH_AVOID
QDF_STATUS sme_ch_avoid_update_req(tHalHandle hal_handle);
#else
static inline
QDF_STATUS sme_ch_avoid_update_req(tHalHandle hal_handle)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
QDF_STATUS sme_set_auto_shutdown_cb(tHalHandle hHal, void (*pCallbackfn)(void));
QDF_STATUS sme_set_auto_shutdown_timer(tHalHandle hHal, uint32_t timer_value);
#endif
QDF_STATUS sme_roam_channel_change_req(tHalHandle hHal,
				       struct qdf_mac_addr bssid,
				       struct ch_params *ch_params,
				       struct csr_roam_profile *profile);

QDF_STATUS sme_roam_start_beacon_req(tHalHandle hHal,
		struct qdf_mac_addr bssid, uint8_t dfsCacWaitStatus);
QDF_STATUS sme_roam_csa_ie_request(tHalHandle hHal, struct qdf_mac_addr bssid,
				   uint8_t targetChannel, uint8_t csaIeReqd,
				   struct ch_params *ch_params);

/**
 * sme_set_addba_accept() - Allow/Reject the ADDBA req session
 * @hal: handle returned by mac_open
 * @session_id: sme session id
 * @value: Allow/Reject AddBA session
 *
 * Allows/Rejects the ADDBA req session
 *
 * Return: 0 on success else errno
 */
int sme_set_addba_accept(tHalHandle hal, uint8_t session_id, int value);

QDF_STATUS sme_init_thermal_info(tHalHandle hHal,
				 tSmeThermalParams thermalParam);

QDF_STATUS sme_set_thermal_level(tHalHandle hHal, uint8_t level);
QDF_STATUS sme_txpower_limit(tHalHandle hHal, tSirTxPowerLimit *psmetx);
QDF_STATUS sme_get_link_speed(tHalHandle hHal, tSirLinkSpeedInfo *lsReq,
		void *plsContext,
		void (*pCallbackfn)(tSirLinkSpeedInfo *indParam,
		void *pContext));
QDF_STATUS sme_modify_add_ie(tHalHandle hHal,
		tSirModifyIE *pModifyIE, eUpdateIEsType updateType);
QDF_STATUS sme_update_add_ie(tHalHandle hHal,
		tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType);
QDF_STATUS sme_update_connect_debug(tHalHandle hHal, uint32_t set_value);
const char *sme_bss_type_to_string(const uint8_t bss_type);
QDF_STATUS sme_ap_disable_intra_bss_fwd(tHalHandle hHal, uint8_t sessionId,
		bool disablefwd);
QDF_STATUS sme_get_channel_bonding_mode5_g(tHalHandle hHal, uint32_t *mode);
QDF_STATUS sme_get_channel_bonding_mode24_g(tHalHandle hHal, uint32_t *mode);
#ifdef WLAN_FEATURE_STATS_EXT
typedef struct sStatsExtRequestReq {
	uint32_t request_data_len;
	uint8_t *request_data;
} tStatsExtRequestReq, *tpStatsExtRequestReq;
typedef void (*StatsExtCallback)(void *, tStatsExtEvent *);
void sme_stats_ext_register_callback(tHalHandle hHal,
		StatsExtCallback callback);
/**
 * sme_register_stats_ext2_callback() - Register stats ext2 register
 * @hal_handle: hal handle for getting global mac struct
 * @stats_ext2_cb: callback to be registered
 *
 * This function will register a callback for frame aggregation failure
 * indications processing.
 *
 * Return: void
 */
void sme_stats_ext2_register_callback(tHalHandle hal_handle,
	void (*stats_ext2_cb)(void *, struct sir_sme_rx_aggr_hole_ind *));
/**
 * sme_send_unit_test_cmd() - send unit test command to lower layer
 * @session_id: sme session id to be filled while forming the command
 * @module_id: module id given by user to be filled in the command
 * @arg_count: number of argument count
 * @arg: pointer to argument list
 *
 * This API exposed to HDD layer which takes the argument from user and sends
 * down to lower layer for further processing
 *
 * Return: QDF_STATUS based on overall success
 */
QDF_STATUS sme_send_unit_test_cmd(uint32_t vdev_id, uint32_t module_id,
				  uint32_t arg_count, uint32_t *arg);
void sme_stats_ext_deregister_callback(tHalHandle hhal);
QDF_STATUS sme_stats_ext_request(uint8_t session_id,
		tpStatsExtRequestReq input);
#endif
QDF_STATUS sme_update_dfs_scan_mode(tHalHandle hHal,
		uint8_t sessionId,
		uint8_t allowDFSChannelRoam);
uint8_t sme_get_dfs_scan_mode(tHalHandle hHal);

QDF_STATUS sme_get_valid_channels_by_band(tHalHandle hHal, uint8_t wifiBand,
		uint32_t *aValidChannels,
		uint8_t *pNumChannels);

#ifdef FEATURE_WLAN_EXTSCAN
QDF_STATUS sme_ext_scan_get_capabilities(tHalHandle hHal,
		tSirGetExtScanCapabilitiesReqParams *pReq);
QDF_STATUS sme_ext_scan_start(tHalHandle hHal,
		tSirWifiScanCmdReqParams *pStartCmd);
QDF_STATUS sme_ext_scan_stop(tHalHandle hHal,
		tSirExtScanStopReqParams *pStopReq);

/**
 * sme_set_bss_hotlist() - SME API to set BSSID hotlist
 * @mac_handle: Opaque handle to the MAC context
 * @params: extscan set hotlist structure
 *
 * Handles the request to set the BSSID hotlist in firmware.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_set_bss_hotlist(mac_handle_t mac_handle,
		    struct extscan_bssid_hotlist_set_params *params);

/**
 * sme_reset_bss_hotlist() - SME API to reset BSSID hotlist
 * @mac_handle: Opaque handle to the MAC context
 * @params: extscan reset hotlist structure
 *
 * Handles the request to reset the BSSID hotlist in firmware.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_reset_bss_hotlist(mac_handle_t mac_handle,
		      struct extscan_bssid_hotlist_reset_params *params);

/**
 * sme_set_significant_change() - SME API to set significant change
 * @mac_handle: Opaque handle to the MAC context
 * @params: extscan set significant change structure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_set_significant_change(mac_handle_t mac_handle,
			   struct extscan_set_sig_changereq_params *params);

/**
 * sme_reset_significant_change() -  SME API to reset significant change
 * @mac_handle: Opaque handle to the MAC context
 * @params: extscan reset significant change structure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_reset_significant_change(mac_handle_t mac_handle,
			     struct extscan_capabilities_reset_params *params);

QDF_STATUS sme_get_cached_results(tHalHandle hHal,
		tSirExtScanGetCachedResultsReqParams *
		pCachedResultsReq);

QDF_STATUS sme_set_epno_list(tHalHandle hal,
			     struct wifi_epno_params *req_msg);
QDF_STATUS sme_set_passpoint_list(tHalHandle hal,
					struct wifi_passpoint_req *req_msg);
QDF_STATUS sme_reset_passpoint_list(tHalHandle hal,
					struct wifi_passpoint_req *req_msg);

QDF_STATUS sme_ext_scan_register_callback(tHalHandle hHal,
		void (*pExtScanIndCb)(void *, const uint16_t, void *));
#else
static inline QDF_STATUS sme_ext_scan_register_callback(tHalHandle hHal,
		void (*pExtScanIndCb)(void *, const uint16_t, void *))
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_EXTSCAN */
QDF_STATUS sme_abort_roam_scan(tHalHandle hHal, uint8_t sessionId);
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
QDF_STATUS sme_ll_stats_clear_req(tHalHandle hHal,
		tSirLLStatsClearReq * pclearStatsReq);
QDF_STATUS sme_ll_stats_set_req(tHalHandle hHal,
		tSirLLStatsSetReq *psetStatsReq);
QDF_STATUS sme_ll_stats_get_req(mac_handle_t mac_handle,
				tSirLLStatsGetReq *get_stats_req,
				void *context);
QDF_STATUS sme_set_link_layer_stats_ind_cb(mac_handle_t mac_handle,
					   void (*callback_routine)(
					   void *callback_ctx, int ind_type,
					   void *rsp, void *cookie));
QDF_STATUS sme_set_link_layer_ext_cb(tHalHandle hal,
		     void (*ll_stats_ext_cb)(hdd_handle_t callback_ctx,
					     tSirLLStatsResults * rsp));
QDF_STATUS sme_reset_link_layer_stats_ind_cb(tHalHandle hhal);
QDF_STATUS sme_ll_stats_set_thresh(tHalHandle hal,
				struct sir_ll_ext_stats_threshold *threshold);
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

QDF_STATUS sme_set_wisa_params(tHalHandle hal,
				struct sir_wisa_params *wisa_params);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS sme_update_roam_offload_enabled(tHalHandle hHal,
		bool nRoamOffloadEnabled);
QDF_STATUS sme_update_roam_key_mgmt_offload_enabled(tHalHandle hal_ctx,
		uint8_t session_id,
		bool key_mgmt_offload_enabled,
		struct pmkid_mode_bits *pmkid_modes);
#endif
#ifdef WLAN_FEATURE_NAN
QDF_STATUS sme_nan_event(tHalHandle hHal, void *pMsg);
#endif /* WLAN_FEATURE_NAN */
QDF_STATUS sme_get_link_status(mac_handle_t mac_handle,
			       csr_link_status_callback callback,
			       void *context, uint8_t session_id);
QDF_STATUS sme_get_temperature(tHalHandle hHal,
		void *tempContext,
		void (*pCallbackfn)(int temperature,
			void *pContext));
QDF_STATUS sme_set_scanning_mac_oui(tHalHandle hHal,
		tSirScanMacOui *pScanMacOui);

#ifdef DHCP_SERVER_OFFLOAD
QDF_STATUS sme_set_dhcp_srv_offload(tHalHandle hHal,
		tSirDhcpSrvOffloadInfo * pDhcpSrvInfo);
#endif /* DHCP_SERVER_OFFLOAD */
#ifdef WLAN_FEATURE_GPIO_LED_FLASHING
QDF_STATUS sme_set_led_flashing(tHalHandle hHal, uint8_t type,
		uint32_t x0, uint32_t x1);
#endif
QDF_STATUS sme_handle_dfs_chan_scan(tHalHandle hHal, uint8_t dfs_flag);
QDF_STATUS sme_enable_dfs_chan_scan(tHalHandle hHal, uint8_t dfs_flag);
QDF_STATUS sme_set_mas(uint32_t val);
QDF_STATUS sme_set_miracast(tHalHandle hal, uint8_t filter_type);
QDF_STATUS sme_ext_change_channel(tHalHandle hHal, uint32_t channel,
					  uint8_t session_id);

QDF_STATUS sme_configure_stats_avg_factor(tHalHandle hal, uint8_t session_id,
					  uint16_t stats_avg_factor);

QDF_STATUS sme_configure_guard_time(tHalHandle hal, uint8_t session_id,
				    uint32_t guard_time);

QDF_STATUS sme_wifi_start_logger(tHalHandle hal,
		struct sir_wifi_start_log start_log);

bool sme_neighbor_middle_of_roaming(tHalHandle hHal,
						uint8_t sessionId);

/*
 * sme_is_any_session_in_middle_of_roaming() - check if roaming is in progress
 * @hal: MAC Handle
 *
 * Checks if any SME session is in middle of roaming
 *
 * Return : true if roaming is in progress else false
 */
bool sme_is_any_session_in_middle_of_roaming(mac_handle_t hal);

/**
 * sme_enable_uapsd_for_ac() - enable uapsd for access category request to WMA
 * @sta_id: station id
 * @ac: access category
 * @tid: tid value
 * @pri: user priority
 * @srvc_int: service interval
 * @sus_int: suspend interval
 * @dir: tspec direction
 * @psb: PSB value
 * @sessionId: session id
 * @delay_interval: delay interval
 *
 * Return: QDF status
 */
QDF_STATUS sme_enable_uapsd_for_ac(uint8_t sta_id,
				      sme_ac_enum_type ac, uint8_t tid,
				      uint8_t pri, uint32_t srvc_int,
				      uint32_t sus_int,
				      enum sme_qos_wmm_dir_type dir,
				      uint8_t psb, uint32_t sessionId,
				      uint32_t delay_interval);

/**
 * sme_disable_uapsd_for_ac() - disable uapsd access category request to WMA
 * @sta_id: station id
 * @ac: access category
 * @sessionId: session id
 *
 * Return: QDF status
 */
QDF_STATUS sme_disable_uapsd_for_ac(uint8_t sta_id,
				       sme_ac_enum_type ac,
				       uint32_t sessionId);

QDF_STATUS sme_set_rssi_monitoring(tHalHandle hal,
					struct rssi_monitor_req *input);
QDF_STATUS sme_set_rssi_threshold_breached_cb(tHalHandle hal,
			void (*cb)(void *, struct rssi_breach_event *));
QDF_STATUS sme_reset_rssi_threshold_breached_cb(tHalHandle hal);

QDF_STATUS sme_register_mgmt_frame_ind_callback(tHalHandle hal,
			sir_mgmt_frame_ind_callback callback);

QDF_STATUS sme_update_nss(tHalHandle h_hal, uint8_t nss);
void sme_update_user_configured_nss(tHalHandle hal, uint8_t nss);

bool sme_is_any_session_in_connected_state(tHalHandle h_hal);

QDF_STATUS sme_pdev_set_pcl(struct policy_mgr_pcl_list *msg);
QDF_STATUS sme_pdev_set_hw_mode(struct policy_mgr_hw_mode msg);
void sme_register_hw_mode_trans_cb(tHalHandle hal,
		hw_mode_transition_cb callback);
QDF_STATUS sme_nss_update_request(uint32_t vdev_id,
				uint8_t  new_nss, policy_mgr_nss_update_cback cback,
				uint8_t next_action, struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_conn_update_reason reason,
				uint32_t original_vdev_id);

typedef void (*sme_peer_authorized_fp) (uint32_t vdev_id);
QDF_STATUS sme_set_peer_authorized(uint8_t *peer_addr,
				   sme_peer_authorized_fp auth_fp,
				   uint32_t vdev_id);
QDF_STATUS sme_soc_set_dual_mac_config(struct policy_mgr_dual_mac_config msg);
QDF_STATUS sme_soc_set_antenna_mode(tHalHandle hal,
		struct sir_antenna_mode_param *msg);

void sme_set_scan_disable(tHalHandle h_hal, int value);
void sme_setdef_dot11mode(tHalHandle hal);

QDF_STATUS sme_handle_set_fcc_channel(tHalHandle hHal,
				      bool fcc_constraint,
				      bool scan_pending);

QDF_STATUS sme_update_roam_scan_hi_rssi_scan_params(tHalHandle hal_handle,
	uint8_t session_id,
	uint32_t notify_id,
	int32_t val);

/**
 * sme_update_tx_bfee_supp() - sets the Tx Bfee support
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @cfg_val: Tx Bfee config value
 *
 * Return: 0 on success else err code
 */
int sme_update_tx_bfee_supp(tHalHandle hal, uint8_t session_id,
		uint8_t cfg_val);

void wlan_sap_enable_phy_error_logs(tHalHandle hal, uint32_t enable_log);
#ifdef WLAN_FEATURE_DSRC
void sme_set_dot11p_config(tHalHandle hal, bool enable_dot11p);

int sme_ocb_gen_timing_advert_frame(tHalHandle hHal, tSirMacAddr self_addr,
				    uint8_t **buf, uint32_t *timestamp_offset,
				    uint32_t *time_value_offset);

static inline void
sme_set_etsi13_srd_ch_in_master_mode(tHalHandle hal,
				     bool etsi13_srd_chan_support)
{
}
#else
static inline void sme_set_dot11p_config(tHalHandle hal, bool enable_dot11p)
{
	return;
}

static inline int sme_ocb_gen_timing_advert_frame(tHalHandle hHal,
		tSirMacAddr self_addr, uint8_t **buf,
		uint32_t *timestamp_offset,
		uint32_t *time_value_offset)
{
	return 0;
}

/**
 * sme_set_etsi13_srd_ch_in_master_mode() - master mode UNI-III band ch support
 * @hal: HAL pointer
 * @srd_chan_support: ETSI SRD channel support
 *
 * This function set master ETSI SRD channel support
 *
 * Return: None
 */
void sme_set_etsi13_srd_ch_in_master_mode(tHalHandle hal,
					  bool etsi13_srd_chan_support);
#endif

void sme_add_set_thermal_level_callback(tHalHandle hal,
		sme_set_thermal_level_callback callback);

void sme_update_tgt_services(tHalHandle hal, struct wma_tgt_services *cfg);
bool sme_validate_sap_channel_switch(tHalHandle hal,
		uint16_t sap_ch, eCsrPhyMode sap_phy_mode,
		uint8_t cc_switch_mode, uint8_t session_id);

bool sme_is_session_id_valid(tHalHandle hal, uint32_t session_id);

#ifdef FEATURE_WLAN_TDLS
void sme_get_opclass(tHalHandle hal, uint8_t channel, uint8_t bw_offset,
		uint8_t *opclass);
#else
static inline void
sme_get_opclass(tHalHandle hal, uint8_t channel, uint8_t bw_offset,
		uint8_t *opclass)
{
}
#endif

#ifdef FEATURE_LFR_SUBNET_DETECTION
QDF_STATUS sme_gateway_param_update(tHalHandle hHal,
				struct gateway_param_update_req *request);
#endif

void sme_update_fine_time_measurement_capab(tHalHandle hal, uint8_t session_id,
								uint32_t val);
QDF_STATUS sme_ht40_stop_obss_scan(tHalHandle hHal, uint32_t vdev_id);
QDF_STATUS sme_set_fw_test(struct set_fwtest_params *fw_test);
QDF_STATUS sme_set_tsfcb(tHalHandle hHal,
	int (*cb_fn)(void *cb_ctx, struct stsf *ptsf), void *cb_ctx);

QDF_STATUS sme_reset_tsfcb(tHalHandle h_hal);

#if defined(WLAN_FEATURE_TSF) && !defined(WLAN_FEATURE_TSF_PLUS_NOIRQ)
QDF_STATUS sme_set_tsf_gpio(tHalHandle h_hal, uint32_t pinvalue);
#endif

QDF_STATUS sme_update_mimo_power_save(tHalHandle hHal,
				      uint8_t is_ht_smps_enabled,
				      uint8_t ht_smps_mode,
				      bool send_smps_action);
#ifdef WLAN_BCN_RECV_FEATURE
/**
 * sme_handle_bcn_recv_start() - Enable fw to start sending
 * beacons of the current connected AP
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: SME session id
 * @nth_value: Beacon report period
 * @do_not_resume: beacon reporting resume after a pause is completed
 *
 * This function remove beacon filter. It allow fw to send
 * all beacons from connected peer to driver.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_handle_bcn_recv_start(mac_handle_t mac_handle,
				     uint32_t vdev_id,
				     uint32_t nth_value,
				     bool do_not_resume);

/**
 * sme_is_beacon_report_started() - Check bcn recv started
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: SME session id
 *
 * This function is to check beacon report started or not.
 *
 * Return: true on success
 */
bool sme_is_beacon_report_started(mac_handle_t mac_handle,
				  uint32_t session_id);

/**
 * sme_is_beacon_reporting_do_not_resume() - Check auto resume allowed or not
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: SME session id
 *
 * This function is to check auto resume of beacon reporting is allowed or not.
 *
 * Return: true on success
 */
bool sme_is_beacon_reporting_do_not_resume(mac_handle_t mac_handle,
					   uint32_t session_id);

/**
 * stop_beacon_report() - To stop beacon report
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: SME session id
 *
 * Return: None
 */
void sme_stop_beacon_report(mac_handle_t mac_handle,
			    uint32_t session_id);

#else
static inline
bool sme_is_beacon_report_started(mac_handle_t mac_handle,
				  uint32_t session_id)
{
	return true;
}

static inline
bool sme_is_beacon_reporting_do_not_resume(mac_handle_t mac_handle,
					   uint32_t session_id)
{
	return false;
}

static inline
void sme_stop_beacon_report(mac_handle_t mac_handle,
			    uint32_t session_id)
{
}

#endif

bool sme_is_sta_smps_allowed(tHalHandle hHal, uint8_t session_id);
QDF_STATUS sme_add_beacon_filter(tHalHandle hal,
				uint32_t session_id, uint32_t *ie_map);
QDF_STATUS sme_remove_beacon_filter(tHalHandle hal, uint32_t session_id);

#ifdef FEATURE_WLAN_APF
/**
 * sme_get_apf_capabilities() - Get APF capabilities
 * @hal: Global HAL handle
 * @callback: Callback function to be called with the result
 * @context: Opaque context to be used by the caller to associate the
 *   request with the response
 *
 * This function constructs the cds message and fill in message type,
 * post the same to WDA.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_get_apf_capabilities(tHalHandle hal,
				    apf_get_offload_cb callback,
				    void *context);

/**
 * sme_set_apf_instructions() - Set APF apf filter instructions.
 * @hal: HAL handle
 * @apf_set_offload: struct to set apf filter instructions.
 *
 * APFv2 (Legacy APF) API to set the APF packet filter.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS sme_set_apf_instructions(tHalHandle hal,
				    struct sir_apf_set_offload
							*apf_set_offload);

/**
 * sme_set_apf_enable_disable - Send apf enable/disable cmd
 * @hal: global hal handle
 * @vdev_id: vdev id
 * @apf_enable: true: Enable APF Int., false: Disable APF Int.
 *
 * API to either enable or disable the APF interpreter.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS sme_set_apf_enable_disable(tHalHandle hal, uint8_t vdev_id,
				      bool apf_enable);

/**
 * sme_apf_write_work_memory - Write into the apf work memory
 * @hal: global hal handle
 * @write_params: APF parameters for the write operation
 *
 * API for writing into the APF work memory.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS sme_apf_write_work_memory(tHalHandle hal,
				    struct wmi_apf_write_memory_params
								*write_params);

/**
 * sme_apf_read_work_memory - Read part of apf work memory
 * @hal: global hal handle
 * @read_params: APF parameters for the get operation
 * @callback: callback to handle the the read response
 *
 * API for issuing a APF read memory request.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS
sme_apf_read_work_memory(tHalHandle hal,
			 struct wmi_apf_read_memory_params *read_params,
			 apf_read_mem_cb callback);

#endif /* FEATURE_WLAN_APF */

uint32_t sme_get_wni_dot11_mode(tHalHandle hal);

/**
 * sme_create_mon_session() - post message to create PE session for monitormode
 * operation
 * @hal_handle: Handle to the HAL
 * @bssid: pointer to bssid
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS_SUCCESS on success, non-zero error code on failure.
 */
QDF_STATUS sme_create_mon_session(tHalHandle hal_handle,
				  uint8_t *bssid, uint8_t vdev_id);
/**
 * sme_delete_mon_session() - post message to delete PE session for mon_mode
 * operation
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: sme session id
 *
 * Return: QDF_STATUS_SUCCESS on success, non-zero error code on failure.
 */
QDF_STATUS sme_delete_mon_session(mac_handle_t mac_handle, uint8_t vdev_id);
QDF_STATUS sme_set_adaptive_dwelltime_config(tHalHandle hal,
			struct adaptive_dwelltime_params *dwelltime_params);

void sme_set_vdev_ies_per_band(tHalHandle hal, uint8_t vdev_id);
void sme_set_pdev_ht_vht_ies(tHalHandle hHal, bool enable2x2);

void sme_update_vdev_type_nss(tHalHandle hal, uint8_t max_supp_nss,
		uint32_t vdev_type_nss, enum band_info band);
void sme_update_hw_dbs_capable(tHalHandle hal, uint8_t hw_dbs_capable);
void sme_register_p2p_lo_event(tHalHandle hHal, void *context,
					p2p_lo_callback callback);

QDF_STATUS sme_remove_bssid_from_scan_list(tHalHandle hal,
	tSirMacAddr bssid);

QDF_STATUS sme_process_mac_pwr_dbg_cmd(tHalHandle hal, uint32_t session_id,
				       struct sir_mac_pwr_dbg_cmd*
				       dbg_args);

void sme_get_vdev_type_nss(enum QDF_OPMODE dev_mode,
			   uint8_t *nss_2g, uint8_t *nss_5g);
QDF_STATUS sme_roam_set_default_key_index(tHalHandle hal, uint8_t session_id,
					  uint8_t default_idx);
void sme_send_disassoc_req_frame(tHalHandle hal, uint8_t session_id, uint8_t
				*peer_mac, uint16_t reason, uint8_t
				wait_for_ack);
QDF_STATUS sme_update_access_policy_vendor_ie(tHalHandle hal,
					uint8_t session_id, uint8_t *vendor_ie,
					int access_policy);

/**
 * sme_set_peer_param() - set peer param
 * @vdev_id: vdev ID
 * @peer_addr: peer MAC address
 * @param_id: param ID to be updated
 * @param_Value: paraam value
 *
 * This SME API is used to send the peer param to WMA to be sent to FW.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_peer_param(uint8_t *peer_addr, uint32_t param_id,
			      uint32_t param_value, uint32_t vdev_id);

QDF_STATUS sme_update_sta_roam_policy(tHalHandle hal,
		enum sta_roam_policy_dfs_mode dfs_mode,
		bool skip_unsafe_channels,
		uint8_t session_id, uint8_t sap_operating_band);
QDF_STATUS sme_enable_disable_chanavoidind_event(tHalHandle hal,
					uint8_t set_value);
QDF_STATUS sme_set_default_scan_ie(tHalHandle hal, uint16_t session_id,
				uint8_t *ie_data, uint16_t ie_len);
/**
 * sme_update_session_param() - API to update PE session param
 * @hal: HAL handle for device
 * @session_id: Session ID
 * @param_type: Param type to be updated
 * @param_val: Param value to be update
 *
 * Note: this setting will not persist over reboots.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_session_param(tHalHandle hal, uint8_t session_id,
		uint32_t param_type, uint32_t param_val);

/**
 * sme_update_fils_setting() - API to update PE FILS setting
 * @hal: HAL handle for device
 * @session_id: Session ID
 * @param_val: Param value to be update
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_fils_setting(tHalHandle hal, uint8_t session_id,
				   uint8_t param_val);
#ifdef WLAN_FEATURE_FIPS
/**
 * sme_fips_request() - Perform a FIPS certification operation
 * @hal: Hal handle for the object being certified
 * @param: The FIPS certification parameters
 * @callback: Callback function to invoke with the results
 * @context: Opaque context to pass back to caller in the callback
 *
 * Return: QDF_STATUS_SUCCESS if the request is successfully sent
 * to firmware for processing, otherwise an error status.
 */
QDF_STATUS sme_fips_request(tHalHandle hal, struct fips_params *param,
			    wma_fips_cb callback, void *context);
#else
static inline
QDF_STATUS sme_fips_request(tHalHandle hal, struct fips_params *param,
			    wma_fips_cb callback, void *context)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_FEATURE_FIPS */

/**
 * sme_set_cts2self_for_p2p_go() - sme function to set ini parms to FW.
 * @hal:                    reference to the HAL
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_cts2self_for_p2p_go(tHalHandle hal);
void sme_set_prefer_80MHz_over_160MHz(tHalHandle hal,
		bool sta_prefer_80MHz_over_160MHz);
QDF_STATUS sme_update_tx_fail_cnt_threshold(tHalHandle hal_handle,
		uint8_t session_id, uint32_t tx_fail_count);
QDF_STATUS sme_update_short_retry_limit_threshold(tHalHandle hal_handle,
		struct sme_short_retry_limit *short_retry_limit_th);
QDF_STATUS sme_update_long_retry_limit_threshold(tHalHandle hal_handle,
		struct sme_long_retry_limit  *long_retry_limit_th);
/**
 * sme_roam_is_ese_assoc() - Check if association type is ESE
 * @roam_info: Pointer to roam info
 *
 * Return: true if ESE Association, false otherwise.
 */
#ifdef FEATURE_WLAN_ESE
bool sme_roam_is_ese_assoc(struct csr_roam_info *roam_info);
#else
static inline bool sme_roam_is_ese_assoc(struct csr_roam_info *roam_info)
{
	return false;
}
#endif
/**
 * sme_neighbor_roam_is11r_assoc() - Check if association type is 11R
 * @hal_ctx: HAL handle
 * @session_id: session id
 *
 * Return: true if 11r Association, false otherwise.
 */
bool sme_neighbor_roam_is11r_assoc(tHalHandle hal_ctx, uint8_t session_id);

/**
 * sme_update_sta_inactivity_timeout(): Update sta_inactivity_timeout to FW
 * @hal: Handle returned by mac_open
 * @sta_inactivity_timer:  struct for sta inactivity timer
 *
 * If a station does not send anything in sta_inactivity_timeout seconds, an
 * empty data frame is sent to it in order to verify whether it is
 * still in range. If this frame is not ACKed, the station will be
 * disassociated and then deauthenticated.
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure.
*/
QDF_STATUS sme_update_sta_inactivity_timeout(tHalHandle hal_handle,
		struct sme_sta_inactivity_timeout  *sta_inactivity_timer);

/**
 * sme_set_lost_link_info_cb() - plug in callback function for receiving
 * @mac_handle: Opaque handle to the MAC context
 * @cb: callback function
 *
 * Return: HAL status
 */
QDF_STATUS sme_set_lost_link_info_cb(mac_handle_t mac_handle,
				     lost_link_info_cb cb);

/**
 * sme_update_new_channel_event() - update new channel event for sapFsm
 * @hal: HAL handle
 * @session_id: session id
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure.
 */
QDF_STATUS sme_update_new_channel_event(tHalHandle hal, uint8_t session_id);
#ifdef WLAN_POWER_DEBUGFS
QDF_STATUS sme_power_debug_stats_req(tHalHandle hal, void (*callback_fn)
				(struct  power_stats_response *response,
				void *context), void *power_stats_context);
#endif

#ifdef WLAN_FEATURE_BEACON_RECEPTION_STATS
/**
 * sme_beacon_debug_stats_req() - SME API to collect beacon debug stats
 * @vdev_id: Vdev id on which stats is being requested
 * @callback_fn: Pointer to the callback function for beacon stats event
 * @beacon_stats_context: Pointer to context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_beacon_debug_stats_req(
		mac_handle_t mac_handle, uint32_t vdev_id,
		void (*callback_fn)(struct bcn_reception_stats_rsp
				    *response, void *context),
		void *beacon_stats_context);
#endif

/**
 * sme_get_sar_power_limits() - get SAR limits
 * @hal: HAL handle
 * @callback: Callback function to invoke with the results
 * @context: Opaque context to pass back to caller in the callback
 *
 * Return: QDF_STATUS_SUCCESS if the request is successfully sent
 * to firmware for processing, otherwise an error status.
 */
QDF_STATUS sme_get_sar_power_limits(tHalHandle hal,
				    wma_sar_cb callback, void *context);

/**
 * sme_set_sar_power_limits() - set sar limits
 * @hal: HAL handle
 * @sar_limit_cmd: struct to send sar limit cmd.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS sme_set_sar_power_limits(tHalHandle hal,
		struct sar_limit_cmd_params *sar_limit_cmd);

/**
 * sme_send_coex_config_cmd() - Send COEX config params
 * @coex_cfg_params: struct to coex config params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_send_coex_config_cmd(struct coex_config_params *coex_cfg_params);

void sme_set_cc_src(tHalHandle hal_handle, enum country_src);


#ifdef WLAN_FEATURE_WOW_PULSE
QDF_STATUS sme_set_wow_pulse(struct wow_pulse_mode *wow_pulse_set_info);
#endif

/* ARP DEBUG STATS */
QDF_STATUS sme_set_nud_debug_stats(tHalHandle hal,
				   struct set_arp_stats_params
				   *set_stats_param);
QDF_STATUS sme_get_nud_debug_stats(tHalHandle hal,
				   struct get_arp_stats_params
				   *get_stats_param);
QDF_STATUS sme_set_nud_debug_stats_cb(tHalHandle hal,
			void (*cb)(void *, struct rsp_stats *, void *context),
			void *context);

/**
 * sme_set_chan_info_callback() - Register chan info callback
 * @hal - MAC global handle
 * @callback_routine - callback routine from HDD
 *
 * This API is invoked by HDD to register its callback to mac
 *
 * Return: QDF_STATUS
 */
void sme_set_chan_info_callback(tHalHandle hal_handle,
			void (*callback)(struct scan_chan_info *chan_info));

/**
 * sme_get_rssi_snr_by_bssid() - gets the rssi and snr by bssid from scan cache
 * @hal: handle returned by mac_open
 * @profile: current connected profile
 * @bssid: bssid to look for in scan cache
 * @rssi: rssi value found
 * @snr: snr value found
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_rssi_snr_by_bssid(tHalHandle hal,
				     struct csr_roam_profile *profile,
				     const uint8_t *bssid, int8_t *rssi,
				     int8_t *snr);

/**
 * sme_get_beacon_frm() - gets the bss descriptor from scan cache and prepares
 * beacon frame
 * @hal: handle returned by mac_open
 * @profile: current connected profile
 * @bssid: bssid to look for in scan cache
 * @frame_buf: frame buffer to populate
 * @frame_len: length of constructed frame
 * @channel: Pointer to channel info to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_beacon_frm(tHalHandle hal, struct csr_roam_profile *profile,
			      const tSirMacAddr bssid,
			      uint8_t **frame_buf, uint32_t *frame_len,
			      int *channel);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD

/**
 * sme_is_fast_reassoc_allowed  - API to check if roam invoke is
 * allowed. Get the roam enabled vdev id and allow roaming only on
 * that vdev id.
 * @mac_handle: Opaque mac handle
 * @vdev_id: vdev id
 *
 * Return: true if roam invoke is allowed, else return false
 */
bool sme_is_fast_reassoc_allowed(mac_handle_t mac_handle, uint8_t vdev_id);

/**
 * sme_fast_reassoc() - invokes FAST REASSOC command
 * @hal: handle returned by mac_open
 * @profile: current connected profile
 * @bssid: bssid to look for in scan cache
 * @channel: channel on which reassoc should be send
 * @vdev_id: vdev id
 * @connected_bssid: bssid of currently connected profile
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_fast_reassoc(tHalHandle hal, struct csr_roam_profile *profile,
			    const tSirMacAddr bssid, int channel,
			    uint8_t vdev_id, const tSirMacAddr connected_bssid);
#else
static inline
bool sme_is_fast_reassoc_allowed(mac_handle_t mac_handle, uint8_t vdev_id)
{
	return true;
}
#endif
/**
 * sme_congestion_register_callback() - registers congestion callback
 * @hal: handler for HAL
 * @congestion_cb: congestion callback
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_congestion_register_callback(tHalHandle hal,
					    congestion_cb congestion_cb);

/**
 * sme_register_tx_queue_cb(): Register tx queue callback
 * @hal: handler for HAL
 * @tx_queue_cb: Transmit Queues callback
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_register_tx_queue_cb(tHalHandle hal,
				    void (*tx_queue_cb)(void *,
				    uint32_t vdev_id,
				    enum netif_action_type action,
				    enum netif_reason_type reason));

/**
 * sme_deregister_tx_queue_cb() - Deregister the tx queue callback
 * @hal: HAL handle
 *
 * Return: QDF status
 */
QDF_STATUS sme_deregister_tx_queue_cb(tHalHandle hal);

/**
 * sme_rso_cmd_status_cb() - Set RSO cmd status callback
 * @mac_handle: Opaque handle for the MAC context
 * @cb: HDD Callback to rso command status read
 *
 * This function is used to save HDD RSO Command status callback in MAC
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_rso_cmd_status_cb(mac_handle_t mac_handle,
				 rso_cmd_status_cb cb);

/**
 * sme_register_set_connection_info_cb() - Register connection
 * info callback
 * @hal - MAC global handle
 * @set_connection_info_cb - callback routine from HDD to set
 *                   connection info flag
 * @get_connection_info_cb - callback routine from HDD to get
 *                         connection info
 *
 * This API is invoked by HDD to register its callback to mac
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_register_set_connection_info_cb(tHalHandle hHal,
				bool (*set_connection_info_cb)(bool),
				bool (*get_connection_info_cb)(uint8_t *session_id,
				enum scan_reject_states *reason));

/**
 * sme_set_dbs_scan_selection_config() - Update DBS scan selection
 * configuration
 * @hal: The handle returned by macOpen
 * @params: wmi_dbs_scan_sel_params config
 *
 * Return: QDF_STATUS if DBS scan selection update
 * configuration success else failure status
 */
QDF_STATUS sme_set_dbs_scan_selection_config(tHalHandle hal,
		struct wmi_dbs_scan_sel_params *params);

/**
 * sme_store_pdev() - store pdev
 * @hal - MAC global handle
 * @pdev - pdev ptr
 *
 * Return: QDF_STATUS
 */
void sme_store_pdev(tHalHandle hal, struct wlan_objmgr_pdev *pdev);

/**
 * sme_ipa_uc_stat_request() - set ipa config parameters
 * @vdev_id: virtual device for the command
 * @param_id: parameter id
 * @param_val: parameter value
 * @req_cat: parameter category
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_ipa_uc_stat_request(tHalHandle hal,
			uint32_t vdev_id, uint32_t param_id,
			uint32_t param_val, uint32_t req_cat);

/**
 * sme_set_reorder_timeout() - set reorder timeout value
 * including Voice,Video,Besteffort,Background parameters
 * @hal: hal handle for getting global mac struct
 * @reg: struct sir_set_rx_reorder_timeout_val
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure.
 */
QDF_STATUS sme_set_reorder_timeout(tHalHandle hal,
		struct sir_set_rx_reorder_timeout_val *req);

/**
 * sme_set_rx_set_blocksize() - set blocksize value
 * including mac_addr and win_limit parameters
 * @hal: hal handle for getting global mac struct
 * @reg: struct sir_peer_set_rx_blocksize
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure.
 */

QDF_STATUS sme_set_rx_set_blocksize(tHalHandle hal,
		struct sir_peer_set_rx_blocksize *req);

/**
 * sme_get_rcpi() - gets the rcpi value for peer mac addr
 * @hal: handle returned by mac_open
 * @rcpi: rcpi request containing peer mac addr, callback and related info
 *
 * This function posts the rcpi measurement request message to wma queue
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_rcpi(tHalHandle hal, struct sme_rcpi_req *rcpi);

/**
 * sme_set_chip_pwr_save_fail_cb() - set chip power save failure callback
 * @mac_handle: opaque handle to the MAC context
 * @cb: callback function pointer
 *
 * This function stores the chip power save failure callback function.
 *
 * Return: QDF_STATUS enumeration.
 */

QDF_STATUS sme_set_chip_pwr_save_fail_cb(mac_handle_t mac_handle,
					 pwr_save_fail_cb cb);
/**
 * sme_cli_set_command() - SME wrapper API over WMA "set" command
 * processor cmd
 * @vdev_id: virtual device for the command
 * @param_id: parameter id
 * @sval: parameter value
 * @vpdev: parameter category
 *
 * Command handler for set operations
 *
 * Return: 0 on success, errno on failure
 */
int sme_cli_set_command(int vdev_id, int param_id, int sval, int vpdev);

/**
 * sme_set_bt_activity_info_cb - set the callback handler for bt events
 * @mac_handle: handle returned by mac_open
 * @cb: callback handler
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_bt_activity_info_cb(mac_handle_t mac_handle,
				       bt_activity_info_cb cb);

/**
 * sme_set_smps_cfg() - set SMPS config params
 * @vdev_id: virtual device for the command
 * @param_id: parameter id
 * @param_val: parameter value
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */

QDF_STATUS sme_set_smps_cfg(uint32_t vdev_id, uint32_t param_id,
				uint32_t param_val);

/**
 * sme_get_peer_info() - sme api to get peer info
 * @hal: hal handle for getting global mac struct
 * @req: peer info request struct send to wma
 * @context: context of callback function
 * @callbackfn: hdd callback function when receive response
 *
 * This function will send WMA_GET_PEER_INFO to WMA
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_get_peer_info(tHalHandle hal,
		struct sir_peer_info_req req,
		void *context,
		void (*callbackfn)(struct sir_peer_info_resp *param,
			void *pcontext));

/**
 * sme_get_peer_info_ext() - sme api to get peer ext info
 * @hal: hal handle for getting global mac struct
 * @req: peer ext info request struct send to wma
 * @context: context of callback function
 * @callbackfn: hdd callback function when receive response
 *
 * This function will send WMA_GET_PEER_INFO_EXT to WMA
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_get_peer_info_ext(tHalHandle hal,
		struct sir_peer_info_ext_req *req,
		void *context,
		void (*callbackfn)(struct sir_peer_info_ext_resp *param,
			void *pcontext));

/**
 * sme_set_5g_band_pref() - sme api to set 5Ghz preference
 * @hal: hal handle for getting global mac struct
 * @pref_params: preference info request struct
 *
 * This function will set 5Ghz preference for STA connection
 *
 * Return: None
 */
void sme_set_5g_band_pref(tHalHandle hal_handle,
			  struct sme_5g_band_pref_params *pref_params);
/**
 * sme_get_chain_rssi() - Get chain rssi
 * @hal: Global HAL handle
 * @input: get chain rssi req params
 * @callback: Callback function to be called with the result
 * @context: Opaque context to be used by the caller to associate the
 *   request with the response
 *
 * This function constructs the cds message and fill in message type,
 * post the same to WDA.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_get_chain_rssi(tHalHandle hal,
			      struct get_chain_rssi_req_params *input,
			      get_chain_rssi_callback callback,
			      void *context);

/**
 * sme_get_isolation() - sme api to get antenna isolation
 * @mac_handle: hal handle for getting global mac struct
 * @context: context of callback function
 * @callbackfn: hdd callback function when receive response
 *
 * This function will send WMA_GET_ISOLATION to WMA
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_get_isolation(mac_handle_t mac_handle,
			     void *context,
			     sme_get_isolation_cb callbackfn);

/**
 * sme_get_valid_channels() - sme api to get valid channels for
 * current regulatory domain
 * @chan_list: list of the valid channels
 * @list_len: length of the channel list
 *
 * This function will get valid channels for current regulatory
 * domain
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_get_valid_channels(uint8_t *chan_list, uint32_t *list_len);

/**
 * sme_get_mac_context() - sme api to get the pmac context
 *
 * This function will return the pmac context
 *
 * Return: pointer to pmac context
 */
tpAniSirGlobal sme_get_mac_context(void);

/**
 * sme_display_disconnect_stats() - Display per session Disconnect stats
 * @hal: hal global context
 * session_id: SME session id
 *
 * Return: None
 */
void sme_display_disconnect_stats(tHalHandle hal, uint8_t session_id);

/**
 * sme_process_msg_callback() - process callback message from LIM
 * @mac: global mac context
 * @msg: scheduler message
 *
 * This function process the callback messages from LIM.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS sme_process_msg_callback(tpAniSirGlobal mac,
				    struct scheduler_msg *msg);

/**
 * sme_set_bmiss_bcnt() - set bmiss config parameters
 * @vdev_id: virtual device for the command
 * @first_cnt: bmiss first value
 * @final_cnt: bmiss final value
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS sme_set_bmiss_bcnt(uint32_t vdev_id, uint32_t first_cnt,
		uint32_t final_cnt);

/**
 * sme_send_limit_off_channel_params() - send limit off channel parameters
 * @hal: global hal handle
 * @vdev_id: vdev id
 * @is_tos_active: tos active or inactive
 * @max_off_chan_time: max off channel time
 * @rest_time: rest time
 * @skip_dfs_chan: skip dfs channel
 *
 * This function sends command to WMA for setting limit off channel command
 * parameters.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS sme_send_limit_off_channel_params(tHalHandle hal, uint8_t vdev_id,
		bool is_tos_active, uint32_t max_off_chan_time,
		uint32_t rest_time, bool skip_dfs_chan);

/**
 * sme_set_vc_mode_config() - Set voltage corner config to FW.
 * @bitmap:	Bitmap that refers to voltage corner config with
 * different phymode and bw configuration
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_vc_mode_config(uint32_t vc_bitmap);

/**
 * sme_set_del_pmkid_cache() - API to update PMKID cache
 * @hal: HAL handle for device
 * @session_id: Session id
 * @pmk_cache_info: Pointer to PMK cache info
 * @is_add: boolean that implies whether to add or delete PMKID entry
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_del_pmkid_cache(tHalHandle hal, uint8_t session_id,
				   tPmkidCacheInfo *pmk_cache_info,
				   bool is_add);

/**
 * sme_send_hlp_ie_info() - API to send HLP IE info to fw
 * @hal: HAL handle for device
 * @vdev_id: vdev id
 * @profile: CSR Roam profile
 * @if_addr: IP address
 *
 * This API is used to send HLP IE info along with IP address
 * to fw if LFR3 is enabled.
 *
 * Return: None
 */
void sme_send_hlp_ie_info(mac_handle_t hal, uint8_t vdev_id,
			  struct csr_roam_profile *profile, uint32_t if_addr);

/**
 * sme_update_session_assoc_ie() - Updates the assoc IEs to csr_roam_session
 * @hal: Opaque handle to the global MAC context
 * @vdev_id: vdev id
 * @src_profile: Pointer to Roam profile in HDD
 *
 * This API is used to copy the assoc IE sent from user space to
 * csr_roam_session
 *
 * Return: None
 */
void sme_update_session_assoc_ie(mac_handle_t hal,
				 uint8_t vdev_id,
				 struct csr_roam_profile *src_profile);

/**
 * sme_send_rso_connect_params() - Updates the assoc IEs to csr_roam_session
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev id
 * @src_profile: CSR Roam profile
 *
 * When the user space updates the assoc IEs or FILS auth type or FILS ERP info,
 * host driver needs to send these updated parameters to firmware via
 * RSO update command.
 *
 * Return: None
 */
QDF_STATUS sme_send_rso_connect_params(mac_handle_t mac_handle,
				       uint8_t vdev_id,
				       struct csr_roam_profile *src_profile);

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * sme_update_fils_config - Update FILS config to CSR roam session
 * @hal: HAL handle for device
 * @vdev_id: vdev id
 * @src_profile: Source profile having latest FILS config
 *
 * API to update FILS config to roam csr session.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_fils_config(tHalHandle hal, uint8_t vdev_id,
				struct csr_roam_profile *src_profile);

/**
 * sme_free_join_rsp_fils_params - free fils params
 * @roam_info: roam info
 *
 * Return: void
 */
void sme_free_join_rsp_fils_params(struct csr_roam_info *roam_info);
#else
static inline QDF_STATUS sme_update_fils_config(tHalHandle hal,
				uint8_t vdev_id,
				struct csr_roam_profile *src_profile)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void sme_free_join_rsp_fils_params(struct csr_roam_info *roam_info)
{}

#endif

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
/**
 * sme_set_he_bss_color() - Sets the HE BSS color
 *
 * @hal: The handle returned by mac_open
 * @session_id: session_id of the request
 * @bss_color: HE BSS color value to set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_he_bss_color(tHalHandle hal, uint8_t session_id,
		uint8_t bss_color);
#else
static inline QDF_STATUS sme_set_he_bss_color(tHalHandle hal,
		uint8_t session_id, uint8_t bss_color)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * sme_is_conn_state_connected() -- check if SME connection state is connected
 * @hal: global hal handle
 * @session_id: current Session Id
 *
 * This API checks if the current SME connection state is connected for the
 * given session id.
 *
 * Return: True if connected, false if any other state.
 */
bool sme_is_conn_state_connected(mac_handle_t hal, uint8_t session_id);

/**
 * sme_scan_get_result_for_bssid - gets the scan result from scan cache for the
 * bssid specified
 * @hal: handle returned by mac_open
 * @bssid: bssid to get the scan result for
 * @res: pointer to tCsrScanResultInfo allocated from caller
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_scan_get_result_for_bssid(tHalHandle hal_handle,
					 struct qdf_mac_addr *bssid,
					 tCsrScanResultInfo *res);

/**
 * sme_get_bss_transition_status() - get bss transition status all cadidates
 * @hal: handle returned by mac_open
 * @transition_reason : Transition reason
 * @bssid: bssid to get BSS transition status
 * @info : bss candidate information
 * @n_candidates : number of candidates
 * @is_bt_in_progress: bt activity indicator
 *
 * Return : 0 on success otherwise errno
 */
int sme_get_bss_transition_status(tHalHandle hal,
		uint8_t transition_reason,
		struct qdf_mac_addr *bssid,
		struct bss_candidate_info *info,
		uint16_t n_candidates,
		bool is_bt_in_progress);

/**
 * sme_unpack_rsn_ie: wrapper to unpack RSN IE and update def RSN params
 * if optional fields are not present.
 * @hal: handle returned by mac_open
 * @buf: rsn ie buffer pointer
 * @buf_len: rsn ie buffer length
 * @rsn_ie: outframe rsn ie structure
 * @append_ie: flag to indicate if the rsn_ie need to be appended from buf
 *
 * Return: parse status
 */
uint32_t sme_unpack_rsn_ie(tHalHandle hal, uint8_t *buf,
				  uint8_t buf_len, tDot11fIERSN *rsn_ie,
				  bool append_ie);
/**
 * sme_get_oper_chan_freq - gets the operating channel freq
 * @vdev: vdev handle
 *
 * Return: operating channel frequency
 */
int16_t sme_get_oper_chan_freq(struct wlan_objmgr_vdev *vdev);

/**
 * sme_get_oper_ch_width - gets the operating channel width
 * @vdev: vdev handle
 *
 * Return: operating channel width
 */
enum phy_ch_width sme_get_oper_ch_width(struct wlan_objmgr_vdev *vdev);

/**
 * sme_get_oper_ch_width - gets the secondary channel frequency
 * @vdev: vdev handle
 * @sec20chan_freq: secondary channel frequency
 *
 * Return: secondary channel frequency
 */
int sme_get_sec20chan_freq_mhz(struct wlan_objmgr_vdev *vdev,
						uint16_t *sec20chan_freq);

/**
 * sme_enable_roaming_on_connected_sta() - Enable roaming on an connected sta
 * @hal: handle returned by mac_open
 *
 * The function check if any connected STA is present on which roaming is not
 * enabled and if present enabled roaming on that STA.
 *
 * Return: none
 */
void sme_enable_roaming_on_connected_sta(tHalHandle hal);

/**
 * sme_send_mgmt_tx() - Sends mgmt frame from CSR to LIM
 * @hal: The handle returned by mac_open
 * @session_id: session id
 * @buf: pointer to frame
 * @len: frame length
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_send_mgmt_tx(tHalHandle hal, uint8_t session_id,
			   const uint8_t *buf, uint32_t len);

#ifdef WLAN_FEATURE_SAE
/**
 * sme_handle_sae_msg() - Sends SAE message received from supplicant
 * @mac_handle: The handle returned by mac_open
 * @session_id: session id
 * @sae_status: status of SAE authentication
 * @peer_mac_addr: mac address of the peer to be authenticated
 * @pmkid: PMKID derived at the end of SAE authentication
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_handle_sae_msg(mac_handle_t mac_handle,
			      uint8_t session_id,
			      uint8_t sae_status,
			      struct qdf_mac_addr peer_mac_addr,
			      const uint8_t *pmkid);

#else
QDF_STATUS sme_handle_sae_msg(mac_handle_t mac_handle,
			      uint8_t session_id,
			      uint8_t sae_status,
			      struct qdf_mac_addr peer_mac_addr,
			      const uint8_t *pmkid)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * sme_set_ba_buff_size() - sets BA buffer size
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @buff_size: BA buffer size
 *
 * Return: 0 on success else err code
 */
int sme_set_ba_buff_size(tHalHandle hal, uint8_t session_id,
		uint16_t buff_size);

/**
 * sme_send_addba_req() - send ADDBA request with user config
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @tid: tid val for BA session
 * @buff_size: BA buffer size
 *
 * Return: 0 on success else err code
 */
int sme_send_addba_req(tHalHandle hal, uint8_t session_id, uint8_t tid,
		uint16_t buff_size);

/**
 * sme_set_no_ack_policy() - Sets no ack policy for AC
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @val: no ack policy value
 * @ac: access category
 *
 * Return: 0 on success else err code
 */
int sme_set_no_ack_policy(tHalHandle hal, uint8_t session_id,
		uint8_t val, uint8_t ac);

/**
 * sme_set_auto_rate_he_sgi() - Sets SGI for auto rate
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @cfg_val: SGI configuration value
 *
 * Return: 0 on success else err code
 */
int sme_set_auto_rate_he_sgi(tHalHandle hal, uint8_t session_id,
		uint8_t cfg_val);

/**
 * sme_set_auto_rate_he_ltf() - Sets HE LTF for auto rate
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @cfg_val: LTF configuration value
 *
 * Return: 0 on success else err code
 */
int sme_set_auto_rate_he_ltf(tHalHandle hal, uint8_t session_id,
		uint8_t cfg_val);

#ifdef WLAN_FEATURE_11AX
/**
 * sme_update_he_cap_nss() - sets the nss based on user request
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @nss: no.of spatial streams value
 *
 * Return: None
 */
void sme_update_he_cap_nss(tHalHandle hal, uint8_t session_id,
		uint8_t nss);

/**
 * sme_update_he_tx_bfee_supp() - sets the HE Tx Bfee support
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @cfg_val: Tx Bfee config value
 *
 * Return: 0 on success else err code
 */
int sme_update_he_tx_bfee_supp(tHalHandle hal, uint8_t session_id,
		uint8_t cfg_val);
/**
 * sme_update_he_mcs() - sets the HE MCS based on user request
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @he_mcs: HE MCS value
 *
 * Return: 0 on success else err code
 */
int sme_update_he_mcs(tHalHandle hal, uint8_t session_id, uint16_t he_mcs);
/**
 * sme_update_he_tx_stbc_cap() - Sets the HE Tx STBC capability
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @value: set value
 *
 * Return: 0 on success else err code
 */
int sme_update_he_tx_stbc_cap(tHalHandle hal, uint8_t session_id, int value);

/**
 * sme_update_he_rx_stbc_cap() - Sets the HE Rx STBC capability
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @value: set value
 *
 * Return: 0 on success else err code
 */
int sme_update_he_rx_stbc_cap(tHalHandle hal, uint8_t session_id, int value);

/**
 * sme_update_he_frag_supp() - sets the HE fragmentation support
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @he_frag: HE fragmention support value
 *
 * Return: 0 on success else err code
 */
int sme_update_he_frag_supp(tHalHandle hal, uint8_t session_id,
		uint16_t he_frag);

/**
 * sme_update_he_ldpc_supp() - sets the HE LDPC support
 * @hal: Pointer to HAL
 * @session_id: SME session id
 * @he_ldpc: HE LDPC support value
 *
 * Return: 0 on success else err code
 */
int sme_update_he_ldpc_supp(tHalHandle hal, uint8_t session_id,
			    uint16_t he_ldpc);
#else
static inline void sme_update_he_cap_nss(tHalHandle hal, uint8_t session_id,
		uint8_t nss)
{}
static inline int sme_update_he_mcs(tHalHandle hal, uint8_t session_id,
				    uint16_t he_mcs)
{
	return 0;
}
static inline int sme_update_he_tx_stbc_cap(tHalHandle hal, uint8_t session_id,
					    int value)
{
	return 0;
}

static inline int sme_update_he_rx_stbc_cap(tHalHandle hal, uint8_t session_id,
					    int value)
{
	return 0;
}

static inline int sme_update_he_frag_supp(tHalHandle hal, uint8_t session_id,
		uint16_t he_frag)
{
	return 0;
}

static inline int sme_update_he_ldpc_supp(tHalHandle hal, uint8_t session_id,
					  uint16_t he_ldpc)
{
	return 0;
}

static inline int sme_update_he_tx_bfee_supp(tHalHandle hal, uint8_t session_id,
		uint8_t cfg_val)
{
	return 0;
}
#endif

/**
 * sme_is_sta_key_exchange_in_progress() - checks whether the STA/P2P client
 * session has key exchange in progress
 *
 * @hal: global hal handle
 * @session_id: session id
 *
 * Return: true - if key exchange in progress
 *         false - if not in progress
 */
bool sme_is_sta_key_exchange_in_progress(tHalHandle hal, uint8_t session_id);

/*
 * sme_validate_channel_list() - Validate the given channel list
 * @hal: handle to global hal context
 * @chan_list: Pointer to the channel list
 * @num_channels: number of channels present in the chan_list
 *
 * Validates the given channel list with base channels in mac context
 *
 * Return: True if all channels in the list are valid, false otherwise
 */
bool sme_validate_channel_list(tHalHandle hal,
				      uint8_t *chan_list,
				      uint8_t num_channels);
/**
 * sme_set_amsdu() - set amsdu enable/disable based on user cfg
 * @hal: Pointer to HAL
 * @enable: enable or disable
 *
 * Return: None
 */
void sme_set_amsdu(tHalHandle hal, bool enable);

/**
 * sme_get_mcs_idx() - gets mcs index
 * @max_rate: max rate
 * @rate_flags: rate flags
 * @nss: number of nss
 * @mcs_rate_flags: mcs rate flag
 *
 * Return: return mcs index
 */
uint8_t sme_get_mcs_idx(uint16_t max_rate, uint8_t rate_flags,
			uint8_t *nss, uint8_t *mcs_rate_flags);

#ifdef WLAN_SUPPORT_TWT
/**
 * sme_register_twt_enable_complete_cb() - TWT enable registrar
 * @hal: HAL handle
 * @twt_enable_cb: Function callback to handle enable event
 * @hdd_ctx: Global HDD Context
 *
 * Return: QDF Status
 */
QDF_STATUS sme_register_twt_enable_complete_cb(tHalHandle hal,
		void (*twt_enable_cb)(void *hdd_ctx,
		struct wmi_twt_enable_complete_event_param *params));

/**
 * sme_register_twt_disable_complete_cb - TWT disable registrar
 * @hal: HAL handle
 * @twt_disable_cb: Function callback to handle disable event
 *
 * Return: QDF Status
 */
QDF_STATUS sme_register_twt_disable_complete_cb(tHalHandle hal,
			void (*twt_disable_cb)(void *hdd_ctx));

/**
 * sme_deregister_twt_enable_complete_cb() - TWT enable deregistrar
 * @hal: HAL handle
 *
 * Return: QDF Status
 */
QDF_STATUS sme_deregister_twt_enable_complete_cb(tHalHandle hal);

/**
 * sme_deregister_twt_disable_complete_cb - TWT disable deregistrar
 * @hal: HAL handle
 *
 * Return: QDF Status
 */
QDF_STATUS sme_deregister_twt_disable_complete_cb(tHalHandle hal);

#else
static inline QDF_STATUS sme_register_twt_enable_complete_cb(tHalHandle hal,
		void (*twt_enable_cb)(void *hdd_ctx,
		struct wmi_twt_enable_complete_event_param *params))
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS sme_register_twt_disable_complete_cb(tHalHandle hal,
					void (*twt_disable_cb)(void *hdd_ctx))
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS sme_deregister_twt_enable_complete_cb(tHalHandle hal)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS sme_deregister_twt_disable_complete_cb(tHalHandle hal)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * sme_get_roam_scan_stats() - Send roam scan stats cmd to wma
 * @hal: handle returned by mac_open
 * @cb: call-back invoked for roam scan stats response
 * @context: context of callback
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_get_roam_scan_stats(tHalHandle hal, roam_scan_stats_cb cb, void *context,
			uint32_t vdev_id);

/**
 * sme_update_hidden_ssid_status_cb() - cb fun to update hidden ssid stats
 * @mac_handle: mac handler
 * @cb: cb of type hidden_ssid_cb
 */
QDF_STATUS sme_update_hidden_ssid_status_cb(mac_handle_t mac_handle,
					    hidden_ssid_cb cb);

#ifdef WLAN_MWS_INFO_DEBUGFS
/**
 * sme_get_mws_coex_info() - SME API to get the coex information
 * @mac_handle: mac handler
 * @vdev_id: Vdev_id
 * @cmd_id: enum mws_coex_cmdid which information is needed.
 * @callback_fn: Callback function
 * @context: callback context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_get_mws_coex_info(mac_handle_t mac_handle, uint32_t vdev_id,
		      uint32_t cmd_id, void (*callback_fn)(void *coex_info_data,
							   void *context,
							   wmi_mws_coex_cmd_id
							   cmd_id),
		      void *context);
#endif

#ifdef WLAN_BCN_RECV_FEATURE
/**
 * sme_register_bcn_recv_pause_ind_cb() - Register pause ind cb
 * mac_handle: man handler
 * cb: callback function to HDD
 *
 * This function register HDD callback in order to indicate beacon
 * receive pause indication to userspace.
 *
 * return QDF_STATUS of cb registration
 */
QDF_STATUS sme_register_bcn_recv_pause_ind_cb(mac_handle_t mac_handle,
					      beacon_pause_cb cb);

#else
static inline
QDF_STATUS sme_register_bcn_recv_pause_ind_cb(mac_handle_t mac_handle,
					      beacon_pause_cb cb)
{
	return QDF_STATUS_SUCCESS;
}
#endif

void sme_freq_to_chan_list(struct wlan_objmgr_pdev *pdev,
			   uint8_t *chan_list,
			   uint32_t *freq_list,
			   uint32_t chan_list_len);

void sme_chan_to_freq_list(struct wlan_objmgr_pdev *pdev,
			   uint32_t *freq_list,
			   const uint8_t *chan_list,
			   uint32_t chan_list_len);

/**
 * sme_set_roam_config_enable() - Cache roam config status in SME
 * @mac_handle: Opaque handle to the MAC context
 * @vdev_id: vdev id
 * @roam_control_enable: Carries a non-zero value if the current set request is
 *			 for enable, otherwise carries a 0.
 *
 * Cache control roam config enable/disable status in SME so that the
 * userspace can query for the status based on a vdev/session at any time.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_roam_config_enable(mac_handle_t mac_handle,
				      uint8_t vdev_id,
				      uint8_t roam_control_enable);

/**
 * sme_get_roam_config_status() - Get roam config status from SME
 * @mac_handle: Opaque handle to the MAC context
 * @vdev_id: vdev id
 * @config_status: Pointer of a buffer to fill the status
 *
 * Get the cached control roam config status in SME and copy to status.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_roam_config_status(mac_handle_t mac_handle, uint8_t vdev_id,
				      uint8_t *config_status);

/**
 * sme_get_full_roam_scan_period_global() - get global full scan refresh period
 * @mac_handle: The handle returned by mac_open
 *
 * Return: Full roam scan period configured through ini
 */
uint16_t sme_get_full_roam_scan_period_global(mac_handle_t mac_handle);

/**
 * sme_get_full_roam_scan_period() - Get full roam scan period
 * @mac_handle: Opaque handle to the MAC context
 * @vdev_id: vdev id
 * @full_roam_scan_period: Pointer of a buffer to fill the full roam scan period
 *
 * Get the full scan period cached in neighborRoamInfo and fill in the given
 * buffer full_roam_scan_period.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_full_roam_scan_period(mac_handle_t mac_handle,
					 uint8_t vdev_id,
					 uint32_t *full_roam_scan_period);

/**
 * sme_set_roam_triggers() - Send roam trigger bitmap to WMA
 * @mac_handle: Opaque handle to the MAC context
 * @triggers: Carries pointer of the object containing vdev id and
 *	      roam_trigger_bitmap.
 *
 * Send the roam trigger bitmap received to WMA/WMI. WMI converts
 * the bitmap to firmware compatible bitmap as per reasons
 * defined @WMI_ROAM_TRIGGER_REASON_ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_roam_triggers(mac_handle_t mac_handle,
				 struct roam_triggers *triggers);

/**
 * sme_roam_control_restore_default_config - Restore roam config to default
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev Identifier
 *
 * Restore enable_scoring_for_roam, emptyScanRefreshPeriod,
 * full_roam_scan_period to their default values and send RSO command to
 * firmware with the updated values.
 *
 * Return: Success or failure
 */
QDF_STATUS sme_roam_control_restore_default_config(mac_handle_t mac_handle,
						   uint8_t vdev_id);

/**
 * sme_roam_reset_configs() - API to reset roam configurations
 * @mac_handle: Opaque handle to the global MAC context
 * @vdev_id: vdev Identifier
 *
 * Return: void
 */
void sme_roam_reset_configs(mac_handle_t mac_handle, uint8_t vdev_id);

/**
 * sme_set_disconnect_ies() - set disconnect IEs
 * @mac_handle: handle returned by mac_open
 * @vdev_id: vdev id
 * @ie_data: Disconnect IE data
 * @ie_len: Disconnect IE length
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_set_disconnect_ies(mac_handle_t mac_handle, uint8_t vdev_id,
				  uint8_t *ie_data, uint16_t ie_len);

#ifdef FEATURE_ANI_LEVEL_REQUEST
/*
 * sme_get_ani_level() -
 * A wrapper function that client calls to register a callback to get ani level
 *
 * @mac_handle - pointer to mac handle
 * @freqs - frequencies for which ANI level has to be fetched
 * @num_freqs - number of frequencies
 * @callback - SME sends back the ani level using the callback
 * @context - user context to be passed back along with the callback
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_ani_level(mac_handle_t mac_handle, uint32_t *freqs,
			     uint8_t num_freqs, void (*callback)(
			     struct wmi_host_ani_level_event *ani, uint8_t num,
			     void *context), void *context);
#endif /* FEATURE_ANI_LEVEL_REQUEST */

/**
 * sme_get_prev_connected_bss_ies() - Get the previous connected AP IEs
 * @mac_handle: The handle returned by mac_open.
 * @vdev_id: vdev id
 * @ies: IEs of the disconnected AP. Currently to carry beacon IEs.
 * @ie_len: Length of the @ies
 *
 * This API extracts the IEs from the previous connected AP info and update
 * them to the ies and ie_len.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_get_prev_connected_bss_ies(mac_handle_t mac_handle,
					  uint8_t vdev_id,
					  uint8_t **ies, uint32_t *ie_len);

/**
 * sme_handle_peer_cleanup() - Cleanup all peers on the given vdev
 * @hal: handle returned by mac_open
 * @vdev_id: vdev id
 *
 * Remove all peers present on the given vdev id.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_handle_peer_cleanup(tHalHandle hal, uint8_t vdev_id);

/**
 * sme_update_owe_info() - Update OWE info
 * @mac: mac context
 * @assoc_ind: assoc ind
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_update_owe_info(tpAniSirGlobal mac,
			       tSirSmeAssocInd *assoc_ind);

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
/**
 * sme_set_beacon_latency_event_cb() - Register beacon latency IE callback
 * @mac_handle: Opaque handle to the MAC context
 * @beacon_latency_event_cb: callback to be registered
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
sme_set_beacon_latency_event_cb(mac_handle_t mac_handle,
				void (*beacon_latency_event_cb)
				(uint32_t latency_level));
#else
static inline QDF_STATUS
sme_set_beacon_latency_event_cb(mac_handle_t mac_handle,
				void (*beacon_latency_event_cb)
				(uint32_t latency_level))
{
	return QDF_STATUS_SUCCESS;
}
#endif

#endif /* #if !defined( __SME_API_H ) */
