/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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

/*
 * This file lim_api.cc contains the functions that are
 * exported by LIM to other modules.
 *
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "wni_cfg.h"
#include "wni_api.h"
#include "sir_common.h"
#include "sir_debug.h"

#include "sch_api.h"
#include "utils_api.h"
#include "lim_api.h"
#include "lim_global.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_admit_control.h"
#include "lim_send_sme_rsp_messages.h"
#include "lim_security_utils.h"
#include "wmm_apsd.h"
#include "lim_trace.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "wma_types.h"
#include "wlan_crypto_global_api.h"
#include "wlan_crypto_def_i.h"

#include "rrm_api.h"

#include <lim_ft.h>
#include "qdf_types.h"
#include "cds_packet.h"
#include "cds_utils.h"
#include "sys_startup.h"
#include "cds_api.h"
#include "wlan_policy_mgr_api.h"
#include "nan_datapath.h"
#include "wma.h"
#include "wlan_mgmt_txrx_utils_api.h"
#include "wlan_objmgr_psoc_obj.h"
#include "os_if_nan.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_scan_public_structs.h>
#include <wlan_p2p_ucfg_api.h>
#include "wlan_utility.h"
#include <wlan_tdls_cfg_api.h>
#include "cfg_ucfg_api.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_mlme_twt_api.h"
#include "wlan_scan_utils_api.h"
#include <qdf_hang_event_notifier.h>
#include <qdf_notifier.h>
#include "wlan_pkt_capture_ucfg_api.h"
#include <lim_mlo.h>
#include "wlan_mlo_mgr_roam.h"
#include "utils_mlo.h"
#include "wlan_mlo_mgr_sta.h"
#include "wlan_mlo_mgr_peer.h"
#include <wlan_twt_api.h>
#include "wlan_tdls_api.h"
#include "wlan_mlo_mgr_link_switch.h"
#include "wlan_cm_api.h"
#include "wlan_mlme_api.h"

struct pe_hang_event_fixed_param {
	uint16_t tlv_header;
	uint8_t vdev_id;
	uint8_t limmlmstate;
	uint8_t limprevmlmstate;
	uint8_t limsmestate;
	uint8_t limprevsmestate;
} qdf_packed;

static void __lim_init_bss_vars(struct mac_context *mac)
{
	qdf_mem_zero((void *)mac->lim.gpSession,
		    sizeof(*mac->lim.gpSession) * mac->lim.maxBssId);
}

static void __lim_init_stats_vars(struct mac_context *mac)
{
	/* / Variable to keep track of number of currently associated STAs */
	mac->lim.gLimNumOfAniSTAs = 0; /* count of ANI peers */

	qdf_mem_zero(mac->lim.gLimHeartBeatApMac[0],
			sizeof(tSirMacAddr));
	qdf_mem_zero(mac->lim.gLimHeartBeatApMac[1],
			sizeof(tSirMacAddr));
	mac->lim.gLimHeartBeatApMacIndex = 0;
}

static void __lim_init_states(struct mac_context *mac)
{
	/* Counts Heartbeat failures */
	mac->lim.gLimHBfailureCntInLinkEstState = 0;
	mac->lim.gLimProbeFailureAfterHBfailedCnt = 0;
	mac->lim.gLimHBfailureCntInOtherStates = 0;
	mac->lim.gLimRspReqd = 0;
	mac->lim.gLimPrevSmeState = eLIM_SME_OFFLINE_STATE;

	/* / MLM State visible across all Sirius modules */
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, NO_SESSION, eLIM_MLM_IDLE_STATE));
	mac->lim.gLimMlmState = eLIM_MLM_IDLE_STATE;

	/* / Previous MLM State */
	mac->lim.gLimPrevMlmState = eLIM_MLM_OFFLINE_STATE;

	/**
	 * Initialize state to eLIM_SME_OFFLINE_STATE
	 */
	mac->lim.gLimSmeState = eLIM_SME_OFFLINE_STATE;

	/**
	 * By default assume 'unknown' role. This will be updated
	 * when SME_START_BSS_REQ is received.
	 */

	qdf_mem_zero(&mac->lim.gLimNoShortParams, sizeof(tLimNoShortParams));
	qdf_mem_zero(&mac->lim.gLimNoShortSlotParams,
		    sizeof(tLimNoShortSlotParams));

	mac->lim.gLimPhyMode = 0;
}

static void __lim_init_vars(struct mac_context *mac)
{
	/* Place holder for Measurement Req/Rsp/Ind related info */


	/* Deferred Queue Parameters */
	qdf_mem_zero(&mac->lim.gLimDeferredMsgQ, sizeof(tSirAddtsReq));

	/* addts request if any - only one can be outstanding at any time */
	qdf_mem_zero(&mac->lim.gLimAddtsReq, sizeof(tSirAddtsReq));
	mac->lim.gLimAddtsSent = 0;
	mac->lim.gLimAddtsRspTimerCount = 0;

	/* protection related config cache */
	qdf_mem_zero(&mac->lim.cfgProtection, sizeof(tCfgProtection));
	mac->lim.gLimProtectionControl = 0;
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);

	/* WMM Related Flag */
	mac->lim.gUapsdEnable = 0;

	/* QoS-AC Downgrade: Initially, no AC is admitted */
	mac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] = 0;
	mac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] = 0;

	/* dialogue token List head/tail for Action frames request sent. */
	mac->lim.pDialogueTokenHead = NULL;
	mac->lim.pDialogueTokenTail = NULL;

	qdf_mem_zero(&mac->lim.tspecInfo,
		    sizeof(tLimTspecInfo) * LIM_NUM_TSPEC_MAX);

	/* admission control policy information */
	qdf_mem_zero(&mac->lim.admitPolicyInfo, sizeof(tLimAdmitPolicyInfo));
}

static void __lim_init_assoc_vars(struct mac_context *mac)
{
	mac->lim.gLimIbssStaLimit = 0;
	/* Place holder for current authentication request */
	/* being handled */
	mac->lim.gpLimMlmAuthReq = NULL;

	/* / MAC level Pre-authentication related globals */
	mac->lim.gLimPreAuthChannelNumber = 0;
	mac->lim.gLimPreAuthType = eSIR_OPEN_SYSTEM;
	qdf_mem_zero(&mac->lim.gLimPreAuthPeerAddr, sizeof(tSirMacAddr));
	mac->lim.gLimNumPreAuthContexts = 0;
	qdf_mem_zero(&mac->lim.gLimPreAuthTimerTable, sizeof(tLimPreAuthTable));

	/* Place holder for Pre-authentication node list */
	mac->lim.pLimPreAuthList = NULL;

	/* One cache for each overlap and associated case. */
	qdf_mem_zero(mac->lim.protStaOverlapCache,
		    sizeof(tCacheParams) * LIM_PROT_STA_OVERLAP_CACHE_SIZE);
	qdf_mem_zero(mac->lim.protStaCache,
		    sizeof(tCacheParams) * LIM_PROT_STA_CACHE_SIZE);

	mac->lim.pe_session = NULL;
	mac->lim.reAssocRetryAttempt = 0;

}

static void __lim_init_ht_vars(struct mac_context *mac)
{
	mac->lim.htCapabilityPresentInBeacon = 0;
	mac->lim.gHTGreenfield = 0;
	mac->lim.gHTShortGI40Mhz = 0;
	mac->lim.gHTShortGI20Mhz = 0;
	mac->lim.gHTMaxAmsduLength = 0;
	mac->lim.gHTDsssCckRate40MHzSupport = 0;
	mac->lim.gHTPSMPSupport = 0;
	mac->lim.gHTLsigTXOPProtection = 0;
	mac->lim.gHTMIMOPSState = eSIR_HT_MIMO_PS_STATIC;
	mac->lim.gHTAMpduDensity = 0;

	mac->lim.gMaxAmsduSizeEnabled = false;
	mac->lim.gHTMaxRxAMpduFactor = 0;
	mac->lim.gHTServiceIntervalGranularity = 0;
	mac->lim.gHTControlledAccessOnly = 0;
	mac->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
	mac->lim.gHTPCOActive = 0;

	mac->lim.gHTPCOPhase = 0;
	mac->lim.gHTSecondaryBeacon = 0;
	mac->lim.gHTDualCTSProtection = 0;
	mac->lim.gHTSTBCBasicMCS = 0;
}

static QDF_STATUS __lim_init_config(struct mac_context *mac)
{
	struct mlme_ht_capabilities_info *ht_cap_info;
#ifdef FEATURE_WLAN_TDLS
	QDF_STATUS status;
	uint32_t val1;
	bool valb;
#endif

	/* Read all the CFGs here that were updated before pe_start is called */
	/* All these CFG READS/WRITES are only allowed in init, at start when there is no session
	 * and they will be used throughout when there is no session
	 */
	mac->lim.gLimIbssStaLimit = mac->mlme_cfg->sap_cfg.assoc_sta_limit;
	ht_cap_info = &mac->mlme_cfg->ht_caps.ht_cap_info;

	/* channel bonding mode could be set to anything from 0 to 4(Titan had these */
	/* modes But for Taurus we have only two modes: enable(>0) or disable(=0) */
	ht_cap_info->supported_channel_width_set =
			mac->mlme_cfg->feature_flags.channel_bonding_mode ?
			WNI_CFG_CHANNEL_BONDING_MODE_ENABLE :
			WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;

	mac->mlme_cfg->ht_caps.info_field_1.recommended_tx_width_set =
		ht_cap_info->supported_channel_width_set;

	if (!mac->mlme_cfg->timeouts.heart_beat_threshold) {
		mac->sys.gSysEnableLinkMonitorMode = 0;
	} else {
		/* No need to activate the timer during init time. */
		mac->sys.gSysEnableLinkMonitorMode = 1;
	}

	/* WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA - not needed */

	/* This was initially done after resume notification from HAL. Now, DAL is
	   started before PE so this can be done here */
	handle_ht_capabilityand_ht_info(mac, NULL);
#ifdef FEATURE_WLAN_TDLS
	status = cfg_tdls_get_buffer_sta_enable(mac->psoc, &valb);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSBufStaEnabled failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSBufStaEnabled = (uint8_t)valb;

	status = cfg_tdls_get_uapsd_mask(mac->psoc, &val1);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSUapsdMask failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSUapsdMask = (uint8_t)val1;

	status = cfg_tdls_get_off_channel_enable(mac->psoc, &valb);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSUapsdMask failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSOffChannelEnabled = (uint8_t)valb;

	status = cfg_tdls_get_wmm_mode_enable(mac->psoc, &valb);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSWmmMode failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSWmmMode = (uint8_t)valb;
#endif

	return QDF_STATUS_SUCCESS;
}

/*
   lim_start
   This function is to replace the __lim_process_sme_start_req since there is no
   eWNI_SME_START_REQ post to PE.
 */
QDF_STATUS lim_start(struct mac_context *mac)
{
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;

	pe_debug("enter");

	if (mac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) {
		mac->lim.gLimSmeState = eLIM_SME_IDLE_STATE;

		MTRACE(mac_trace
			       (mac, TRACE_CODE_SME_STATE, NO_SESSION,
			       mac->lim.gLimSmeState));

		/* Initialize MLM state machine */
		if (QDF_STATUS_SUCCESS != lim_init_mlm(mac)) {
			pe_err("Init MLM failed");
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		/**
		 * Should not have received eWNI_SME_START_REQ in states
		 * other than OFFLINE. Return response to host and
		 * log error
		 */
		pe_warn("Invalid SME state: %X",
			mac->lim.gLimSmeState);
		retCode = QDF_STATUS_E_FAILURE;
	}

	mac->lim.req_id =
		wlan_scan_register_requester(mac->psoc,
					     "LIM",
					     lim_process_rx_scan_handler,
					     mac);
	return retCode;
}

/**
 * lim_initialize()
 *
 ***FUNCTION:
 * This function is called from LIM thread entry function.
 * LIM related global data structures are initialized in this function.
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac - Pointer to global MAC structure
 * @return None
 */

QDF_STATUS lim_initialize(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	mac->lim.tdls_frm_session_id = NO_SESSION;
	mac->lim.deferredMsgCnt = 0;
	mac->lim.retry_packet_cnt = 0;
	mac->lim.deauthMsgCnt = 0;
	mac->lim.disassocMsgCnt = 0;

	__lim_init_assoc_vars(mac);
	__lim_init_vars(mac);
	__lim_init_states(mac);
	__lim_init_stats_vars(mac);
	__lim_init_bss_vars(mac);
	__lim_init_ht_vars(mac);

	rrm_initialize(mac);

	if (QDF_IS_STATUS_ERROR(qdf_mutex_create(
					&mac->lim.lim_frame_register_lock))) {
		pe_err("lim lock init failed!");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_list_create(&mac->lim.gLimMgmtFrameRegistratinQueue, 0);

	/* initialize the TSPEC admission control table. */
	/* Note that this was initially done after resume notification from HAL. */
	/* Now, DAL is started before PE so this can be done here */
	lim_admit_control_init(mac);
	return status;

} /*** end lim_initialize() ***/

/**
 * lim_cleanup()
 *
 ***FUNCTION:
 * This function is called upon reset or persona change
 * to cleanup LIM state
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @return None
 */

void lim_cleanup(struct mac_context *mac)
{
	uint8_t i;
	qdf_list_node_t *lst_node;

	/*
	 * Before destroying the list making sure all the nodes have been
	 * deleted
	 */
	while (qdf_list_remove_front(
			&mac->lim.gLimMgmtFrameRegistratinQueue,
			&lst_node) == QDF_STATUS_SUCCESS) {
		qdf_mem_free(lst_node);
	}
	qdf_list_destroy(&mac->lim.gLimMgmtFrameRegistratinQueue);
	qdf_mutex_destroy(&mac->lim.lim_frame_register_lock);

	pe_deregister_mgmt_rx_frm_callback(mac);

	/* free up preAuth table */
	if (mac->lim.gLimPreAuthTimerTable.pTable) {
		for (i = 0; i < mac->lim.gLimPreAuthTimerTable.numEntry; i++)
			qdf_mem_free(mac->lim.gLimPreAuthTimerTable.pTable[i]);
		qdf_mem_free(mac->lim.gLimPreAuthTimerTable.pTable);
		mac->lim.gLimPreAuthTimerTable.pTable = NULL;
		mac->lim.gLimPreAuthTimerTable.numEntry = 0;
	}

	if (mac->lim.pDialogueTokenHead) {
		lim_delete_dialogue_token_list(mac);
	}

	if (mac->lim.pDialogueTokenTail) {
		qdf_mem_free(mac->lim.pDialogueTokenTail);
		mac->lim.pDialogueTokenTail = NULL;
	}

	if (mac->lim.gpLimMlmAuthReq) {
		qdf_mem_free(mac->lim.gpLimMlmAuthReq);
		mac->lim.gpLimMlmAuthReq = NULL;
	}

	if (mac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq) {
		qdf_mem_free(mac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq);
		mac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq = NULL;
	}

	if (mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq) {
		qdf_mem_free(mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq);
		mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq = NULL;
	}

	/* Now, finally reset the deferred message queue pointers */
	lim_reset_deferred_msg_q(mac);

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++)
		rrm_cleanup(mac, i);

	lim_ft_cleanup_all_ft_sessions(mac);

	wlan_scan_unregister_requester(mac->psoc, mac->lim.req_id);
} /*** end lim_cleanup() ***/

#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
/**
 * lim_state_info_dump() - print state information of lim layer
 * @buf: buffer pointer
 * @size: size of buffer to be filled
 *
 * This function is used to print state information of lim layer
 *
 * Return: None
 */
static void lim_state_info_dump(char **buf_ptr, uint16_t *size)
{
	struct mac_context *mac;
	uint16_t len = 0;
	char *buf = *buf_ptr;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		return;
	}

	pe_debug("size of buffer: %d", *size);

	len += qdf_scnprintf(buf + len, *size - len,
		"\n SmeState: %d", mac->lim.gLimSmeState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n PrevSmeState: %d", mac->lim.gLimPrevSmeState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n MlmState: %d", mac->lim.gLimMlmState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n PrevMlmState: %d", mac->lim.gLimPrevMlmState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n ProcessDefdMsgs: %d", mac->lim.gLimProcessDefdMsgs);

	*size -= len;
	*buf_ptr += len;
}

/**
 * lim_register_debug_callback() - registration function for lim layer
 * to print lim state information
 *
 * Return: None
 */
static void lim_register_debug_callback(void)
{
	qdf_register_debug_callback(QDF_MODULE_ID_PE, &lim_state_info_dump);
}
#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static void lim_register_debug_callback(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */

#ifdef WLAN_FEATURE_NAN
static void lim_nan_register_callbacks(struct mac_context *mac_ctx)
{
	struct nan_callbacks cb_obj = {0};

	cb_obj.add_ndi_peer = lim_add_ndi_peer_converged;
	cb_obj.ndp_delete_peers = lim_ndp_delete_peers_converged;
	cb_obj.delete_peers_by_addr = lim_ndp_delete_peers_by_addr_converged;

	ucfg_nan_register_lim_callbacks(mac_ctx->psoc, &cb_obj);
}
#else
static inline void lim_nan_register_callbacks(struct mac_context *mac_ctx)
{
}
#endif

void lim_stop_pmfcomeback_timer(struct pe_session *session)
{
	if (session->opmode != QDF_STA_MODE)
		return;

	qdf_mc_timer_stop(&session->pmf_retry_timer);
	session->pmf_retry_timer_info.retried = false;
}

/*
 * pe_shutdown_notifier_cb - Shutdown notifier callback
 * @ctx: Pointer to Global MAC structure
 *
 * Return: None
 */
static void pe_shutdown_notifier_cb(void *ctx)
{
	struct mac_context *mac_ctx = (struct mac_context *)ctx;
	struct pe_session *session;
	uint8_t i;

	lim_deactivate_timers(mac_ctx);
	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		session = &mac_ctx->lim.gpSession[i];
		if (session->valid == true) {
			if (LIM_IS_AP_ROLE(session))
				qdf_mc_timer_stop(&session->
						 protection_fields_reset_timer);
			lim_stop_pmfcomeback_timer(session);
		}
	}
}

bool is_mgmt_protected(uint32_t vdev_id,
		       const uint8_t *peer_mac_addr)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	struct pe_session *session;
	bool protected = false;
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac_ctx)
		return false;

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		/* couldn't find session */
		pe_err("Session not found for vdev_id: %d", vdev_id);
		return false;
	}

	sta_ds = dph_lookup_hash_entry(mac_ctx, (uint8_t *)peer_mac_addr, &aid,
				       &session->dph.dphHashTable);
	if (sta_ds) {
		/* rmfenabled will be set at the time of addbss.
		 * but sometimes EAP auth fails and keys are not
		 * installed then if we send any management frame
		 * like deauth/disassoc with this bit set then
		 * firmware crashes. so check for keys are
		 * installed or not also before setting the bit
		 */
		if (sta_ds->rmfEnabled && sta_ds->is_key_installed)
			protected = true;
	}

	return protected;
}

static void p2p_register_callbacks(struct mac_context *mac_ctx)
{
	struct p2p_protocol_callbacks p2p_cb = {0};

	p2p_cb.is_mgmt_protected = is_mgmt_protected;
	ucfg_p2p_register_callbacks(mac_ctx->psoc, &p2p_cb);
}

/*
 * lim_register_sap_bcn_callback(): Register a callback with scan module for SAP
 * @mac_ctx: pointer to the global mac context
 *
 * Registers the function lim_handle_sap_beacon as callback with the Scan
 * module to handle beacon frames for SAP sessions
 *
 * Return: QDF Status
 */
static QDF_STATUS lim_register_sap_bcn_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;

	status = ucfg_scan_register_bcn_cb(mac_ctx->psoc,
			lim_handle_sap_beacon,
			SCAN_CB_TYPE_UPDATE_BCN);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("failed with status code %08d [x%08x]",
			status, status);
	}

	return status;
}

/*
 * lim_unregister_sap_bcn_callback(): Unregister the callback with scan module
 * @mac_ctx: pointer to the global mac context
 *
 * Unregisters the callback registered with the Scan
 * module to handle beacon frames for SAP sessions
 *
 * Return: QDF Status
 */
static QDF_STATUS lim_unregister_sap_bcn_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;

	status = ucfg_scan_register_bcn_cb(mac_ctx->psoc,
			NULL, SCAN_CB_TYPE_UPDATE_BCN);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("failed with status code %08d [x%08x]",
			status, status);
	}

	return status;
}

/*
 * lim_register_scan_mbssid_callback(): Register callback with scan module
 * @mac_ctx: pointer to the global mac context
 *
 * Registers the function lim_register_scan_mbssid_callback as callback
 * with the Scan module to handle generated frames by MBSSID IE
 *
 * Return: QDF Status
 */
static QDF_STATUS
lim_register_scan_mbssid_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;

	status = wlan_scan_register_mbssid_cb(mac_ctx->psoc,
					      lim_handle_frame_genby_mbssid);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("failed with status code %08d [x%08x]",
		       status, status);
	}

	return status;
}

/*
 * lim_unregister_scan_mbssid_callback(): Unregister callback with scan module
 * @mac_ctx: pointer to the global mac context
 *
 * Unregisters the callback registered with the Scan module to handle
 * generated frames by MBSSID IE
 *
 * Return: QDF Status
 */
static QDF_STATUS
lim_unregister_scan_mbssid_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;

	status = wlan_scan_register_mbssid_cb(mac_ctx->psoc, NULL);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("failed with status code %08d [x%08x]",
		       status, status);
	}

	return status;
}

static void lim_register_policy_mgr_callback(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_conc_cbacks conc_cbacks;

	qdf_mem_zero(&conc_cbacks, sizeof(conc_cbacks));
	conc_cbacks.connection_info_update = lim_send_conc_params_update;

	if (QDF_STATUS_SUCCESS != policy_mgr_register_conc_cb(psoc,
							      &conc_cbacks)) {
		pe_err("failed to register policy manager callbacks");
	}
}

static int pe_hang_event_notifier_call(struct notifier_block *block,
				       unsigned long state,
				       void *data)
{
	qdf_notif_block *notif_block = qdf_container_of(block, qdf_notif_block,
							notif_block);
	struct mac_context *mac;
	struct pe_session *session;
	struct qdf_notifer_data *pe_hang_data = data;
	uint8_t *pe_data;
	uint8_t i;
	struct pe_hang_event_fixed_param *cmd;
	size_t size;

	if (!data)
		return NOTIFY_STOP_MASK;

	mac = notif_block->priv_data;
	if (!mac)
		return NOTIFY_STOP_MASK;

	size = sizeof(*cmd);
	for (i = 0; i < mac->lim.maxBssId; i++) {
		session = &mac->lim.gpSession[i];
		if (!session->valid)
			continue;
		if (pe_hang_data->offset + size > QDF_WLAN_HANG_FW_OFFSET)
			return NOTIFY_STOP_MASK;

		pe_data = (pe_hang_data->hang_data + pe_hang_data->offset);
		cmd = (struct pe_hang_event_fixed_param *)pe_data;
		QDF_HANG_EVT_SET_HDR(&cmd->tlv_header, HANG_EVT_TAG_LEGACY_MAC,
				     QDF_HANG_GET_STRUCT_TLVLEN(*cmd));
		cmd->vdev_id = session->vdev_id;
		cmd->limmlmstate = session->limMlmState;
		cmd->limprevmlmstate = session->limPrevMlmState;
		cmd->limsmestate = session->limSmeState;
		cmd->limprevsmestate = session->limPrevSmeState;
		pe_hang_data->offset += size;
	}

	return NOTIFY_OK;
}

static qdf_notif_block pe_hang_event_notifier = {
	.notif_block.notifier_call = pe_hang_event_notifier_call,
};

/** -------------------------------------------------------------
   \fn pe_open
   \brief will be called in Open sequence from mac_open
   \param   struct mac_context *mac
   \param   tHalOpenParameters *pHalOpenParam
   \return  QDF_STATUS
   -------------------------------------------------------------*/

QDF_STATUS pe_open(struct mac_context *mac, struct cds_config_info *cds_cfg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (QDF_DRIVER_TYPE_MFG == cds_cfg->driver_type)
		return QDF_STATUS_SUCCESS;

	mac->lim.maxBssId = cds_cfg->max_bssid;
	mac->lim.maxStation = cds_cfg->max_station;
	mac->lim.max_sta_of_pe_session =
			(cds_cfg->max_station > SIR_SAP_MAX_NUM_PEERS) ?
				SIR_SAP_MAX_NUM_PEERS : cds_cfg->max_station;
	qdf_spinlock_create(&mac->sys.bbt_mgmt_lock);

	if ((mac->lim.maxBssId == 0) || (mac->lim.maxStation == 0)) {
		pe_err("max number of Bssid or Stations cannot be zero!");
		return QDF_STATUS_E_FAILURE;
	}

	if (!QDF_IS_STATUS_SUCCESS(pe_allocate_dph_node_array_buffer())) {
		pe_err("g_dph_node_array memory allocate failed!");
		return QDF_STATUS_E_NOMEM;
	}

	mac->lim.lim_timers.gpLimCnfWaitTimer =
		qdf_mem_malloc(sizeof(TX_TIMER) * (mac->lim.maxStation + 1));
	if (!mac->lim.lim_timers.gpLimCnfWaitTimer) {
		status = QDF_STATUS_E_NOMEM;
		goto pe_open_timer_fail;
	}

	mac->lim.gpSession =
		qdf_mem_common_alloc(sizeof(struct pe_session) * mac->lim.maxBssId);
	if (!mac->lim.gpSession) {
		status = QDF_STATUS_E_NOMEM;
		goto pe_open_psession_fail;
	}

	status = lim_initialize(mac);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("lim_initialize failed!");
		status = QDF_STATUS_E_FAILURE;
		goto  pe_open_lock_fail;
	}

	/*
	 * pe_open is successful by now, so it is right time to initialize
	 * MTRACE for PE module. if LIM_TRACE_RECORD is not defined in build
	 * file then nothing will be logged for PE module.
	 */
#ifdef LIM_TRACE_RECORD
	MTRACE(lim_trace_init(mac));
#endif
	lim_register_debug_callback();
	lim_nan_register_callbacks(mac);
	p2p_register_callbacks(mac);
	lim_register_scan_mbssid_callback(mac);
	lim_register_sap_bcn_callback(mac);
	wlan_reg_register_ctry_change_callback(
					mac->psoc,
					lim_update_tx_pwr_on_ctry_change_cb);

	wlan_reg_register_is_chan_connected_callback(mac->psoc,
					lim_get_connected_chan_for_mode);

	if (mac->mlme_cfg->edca_params.enable_edca_params)
		lim_register_policy_mgr_callback(mac->psoc);

	if (!QDF_IS_STATUS_SUCCESS(
	    cds_shutdown_notifier_register(pe_shutdown_notifier_cb, mac))) {
		pe_err("Shutdown notifier register failed");
	}

	pe_hang_event_notifier.priv_data = mac;
	qdf_hang_event_register_notifier(&pe_hang_event_notifier);

	return status; /* status here will be QDF_STATUS_SUCCESS */

pe_open_lock_fail:
	qdf_mem_common_free(mac->lim.gpSession);
	mac->lim.gpSession = NULL;
pe_open_psession_fail:
	qdf_mem_free(mac->lim.lim_timers.gpLimCnfWaitTimer);
	mac->lim.lim_timers.gpLimCnfWaitTimer = NULL;
pe_open_timer_fail:
	pe_free_dph_node_array_buffer();

	return status;
}

/** -------------------------------------------------------------
   \fn pe_close
   \brief will be called in close sequence from mac_close
   \param   struct mac_context *mac
   \return  QDF_STATUS
   -------------------------------------------------------------*/

QDF_STATUS pe_close(struct mac_context *mac)
{
	uint8_t i;

	if (ANI_DRIVER_TYPE(mac) == QDF_DRIVER_TYPE_MFG)
		return QDF_STATUS_SUCCESS;

	qdf_hang_event_unregister_notifier(&pe_hang_event_notifier);
	lim_cleanup_mlm(mac);
	lim_cleanup(mac);
	lim_unregister_scan_mbssid_callback(mac);
	lim_unregister_sap_bcn_callback(mac);
	wlan_reg_unregister_ctry_change_callback(
					mac->psoc,
					lim_update_tx_pwr_on_ctry_change_cb);

	wlan_reg_unregister_is_chan_connected_callback(mac->psoc,
					lim_get_connected_chan_for_mode);

	if (mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq) {
		qdf_mem_free(mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq);
		mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq = NULL;
	}

	qdf_spinlock_destroy(&mac->sys.bbt_mgmt_lock);
	for (i = 0; i < mac->lim.maxBssId; i++) {
		if (mac->lim.gpSession[i].valid == true)
			pe_delete_session(mac, &mac->lim.gpSession[i]);
	}
	qdf_mem_free(mac->lim.lim_timers.gpLimCnfWaitTimer);
	mac->lim.lim_timers.gpLimCnfWaitTimer = NULL;

	qdf_mem_common_free(mac->lim.gpSession);
	mac->lim.gpSession = NULL;

	pe_free_dph_node_array_buffer();

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn pe_start
   \brief will be called in start sequence from mac_start
   \param   struct mac_context *mac
   \return QDF_STATUS_SUCCESS on success, other QDF_STATUS on error
   -------------------------------------------------------------*/

QDF_STATUS pe_start(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	status = lim_start(mac);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("lim_start failed!");
		return status;
	}
	/* Initialize the configurations needed by PE */
	if (QDF_STATUS_E_FAILURE == __lim_init_config(mac)) {
		pe_err("lim init config failed!");
		/* We need to undo everything in lim_start */
		lim_cleanup_mlm(mac);
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

/** -------------------------------------------------------------
   \fn pe_stop
   \brief will be called in stop sequence from mac_stop
   \param   struct mac_context *mac
   \return none
   -------------------------------------------------------------*/

void pe_stop(struct mac_context *mac)
{
	pe_debug(" PE STOP: Set LIM state to eLIM_MLM_OFFLINE_STATE");
	SET_LIM_MLM_STATE(mac, eLIM_MLM_OFFLINE_STATE);
	return;
}

static void pe_free_nested_messages(struct scheduler_msg *msg)
{
	switch (msg->type) {
	default:
		break;
	}
}

/** -------------------------------------------------------------
   \fn pe_free_msg
   \brief Called by CDS scheduler (function cds_sched_flush_mc_mqs)
 \      to free a given PE message on the TX and MC thread.
 \      This happens when there are messages pending in the PE
 \      queue when system is being stopped and reset.
   \param   struct mac_context *mac
   \param   struct scheduler_msg       pMsg
   \return none
   -----------------------------------------------------------------*/
void pe_free_msg(struct mac_context *mac, struct scheduler_msg *pMsg)
{
	if (pMsg) {
		if (pMsg->bodyptr) {
			if (SIR_BB_XPORT_MGMT_MSG == pMsg->type) {
				cds_pkt_return_packet((cds_pkt_t *) pMsg->
						      bodyptr);
			} else {
				pe_free_nested_messages(pMsg);
				qdf_mem_free((void *)pMsg->bodyptr);
			}
		}
		pMsg->bodyptr = 0;
		pMsg->bodyval = 0;
		pMsg->type = 0;
	}
	return;
}

QDF_STATUS lim_post_msg_api(struct mac_context *mac, struct scheduler_msg *msg)
{
	return scheduler_post_message(QDF_MODULE_ID_PE,
				      QDF_MODULE_ID_PE,
				      QDF_MODULE_ID_PE, msg);
}

QDF_STATUS lim_post_msg_high_priority(struct mac_context *mac,
				      struct scheduler_msg *msg)
{
	return scheduler_post_msg_by_priority(QDF_MODULE_ID_PE,
					       msg, true);
}

QDF_STATUS pe_mc_process_handler(struct scheduler_msg *msg)
{
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac_ctx)
		return QDF_STATUS_E_FAILURE;

	if (ANI_DRIVER_TYPE(mac_ctx) == QDF_DRIVER_TYPE_MFG)
		return QDF_STATUS_SUCCESS;

	lim_message_processor(mac_ctx, msg);

	return QDF_STATUS_SUCCESS;
}

/**
 * pe_drop_pending_rx_mgmt_frames: To drop pending RX mgmt frames
 * @mac_ctx: Pointer to global MAC structure
 * @hdr: Management header
 * @cds_pkt: Packet
 *
 * This function is used to drop RX pending mgmt frames if pe mgmt queue
 * reaches threshold
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF_STATUS_E_FAILURE on failure
 */
static QDF_STATUS pe_drop_pending_rx_mgmt_frames(struct mac_context *mac_ctx,
				tpSirMacMgmtHdr hdr, cds_pkt_t *cds_pkt)
{
	qdf_spin_lock(&mac_ctx->sys.bbt_mgmt_lock);
	if (mac_ctx->sys.sys_bbt_pending_mgmt_count >=
	     MGMT_RX_PACKETS_THRESHOLD) {
		qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
		pe_debug("No.of pending RX management frames reaches to threshold, dropping management frames");
		cds_pkt_return_packet(cds_pkt);
		cds_pkt = NULL;
		mac_ctx->rx_packet_drop_counter++;
		return QDF_STATUS_E_FAILURE;
	} else if (mac_ctx->sys.sys_bbt_pending_mgmt_count >
		   (MGMT_RX_PACKETS_THRESHOLD / 2)) {
		/* drop all probereq, proberesp and beacons */
		if (hdr->fc.subType == SIR_MAC_MGMT_BEACON ||
		    hdr->fc.subType == SIR_MAC_MGMT_PROBE_REQ ||
		    hdr->fc.subType == SIR_MAC_MGMT_PROBE_RSP) {
			qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
			if (!(mac_ctx->rx_packet_drop_counter % 100))
				pe_debug("No.of pending RX mgmt frames reaches 1/2 thresh, dropping frame subtype: %d rx_packet_drop_counter: %d",
					hdr->fc.subType,
					mac_ctx->rx_packet_drop_counter);
			mac_ctx->rx_packet_drop_counter++;
			cds_pkt_return_packet(cds_pkt);
			cds_pkt = NULL;
			return QDF_STATUS_E_FAILURE;
		}
	}
	mac_ctx->sys.sys_bbt_pending_mgmt_count++;
	qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
	if (mac_ctx->sys.sys_bbt_pending_mgmt_count ==
	    (MGMT_RX_PACKETS_THRESHOLD / 4)) {
		if (!(mac_ctx->rx_packet_drop_counter % 100))
			pe_debug("No.of pending RX management frames reaches to 1/4th of threshold, rx_packet_drop_counter: %d",
				mac_ctx->rx_packet_drop_counter);
		mac_ctx->rx_packet_drop_counter++;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * pe_is_ext_scan_bcn_probe_rsp - Check if the beacon or probe response
 * is from Ext or EPNO scan
 *
 * @hdr: pointer to the 802.11 header of the frame
 * @rx_pkt_info: pointer to the rx packet meta
 *
 * Checks if the beacon or probe response is from Ext Scan or EPNO scan
 *
 * Return: true or false
 */
#ifdef FEATURE_WLAN_EXTSCAN
static inline bool pe_is_ext_scan_bcn_probe_rsp(tpSirMacMgmtHdr hdr,
				uint8_t *rx_pkt_info)
{
	if ((hdr->fc.subType == SIR_MAC_MGMT_BEACON ||
	     hdr->fc.subType == SIR_MAC_MGMT_PROBE_RSP) &&
	    (WMA_IS_EXTSCAN_SCAN_SRC(rx_pkt_info) ||
	    WMA_IS_EPNO_SCAN_SRC(rx_pkt_info)))
		return true;

	return false;
}
#else
static inline bool pe_is_ext_scan_bcn_probe_rsp(tpSirMacMgmtHdr hdr,
				uint8_t *rx_pkt_info)
{
	return false;
}
#endif

/**
 * pe_filter_drop_bcn_probe_frame - Apply filter on the received frame
 *
 * @mac_ctx: pointer to the global mac context
 * @hdr: pointer to the 802.11 header of the frame
 * @rx_pkt_info: pointer to the rx packet meta
 *
 * Applies the filter from global mac context on the received beacon/
 * probe response frame before posting it to the PE queue
 *
 * Return: true if frame is allowed, false if frame is to be dropped.
 */
static bool pe_filter_bcn_probe_frame(struct mac_context *mac_ctx,
					tpSirMacMgmtHdr hdr,
					uint8_t *rx_pkt_info)
{
	uint8_t session_id;
	struct mgmt_beacon_probe_filter *filter;

	if (pe_is_ext_scan_bcn_probe_rsp(hdr, rx_pkt_info))
		return true;

	filter = &mac_ctx->bcn_filter;

	/*
	 * If any STA session exists and beacon source matches any of the
	 * STA BSSIDs, allow the frame
	 */
	if (filter->num_sta_sessions) {
		for (session_id = 0; session_id < WLAN_MAX_VDEVS;
		     session_id++) {
			if (sir_compare_mac_addr(filter->sta_bssid[session_id],
			    hdr->bssId)) {
				return true;
			}
		}
	}

	return false;
}

static QDF_STATUS pe_handle_probe_req_frames(struct mac_context *mac_ctx,
					cds_pkt_t *pkt)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	uint32_t scan_queue_size = 0;

	/* Check if the probe request frame can be posted in the scan queue */
	status = scheduler_get_queue_size(QDF_MODULE_ID_SCAN, &scan_queue_size);
	if (!QDF_IS_STATUS_SUCCESS(status) ||
	    scan_queue_size > MAX_BCN_PROBE_IN_SCAN_QUEUE) {
		pe_debug_rl("Dropping probe req frame, queue size %d",
			    scan_queue_size);
		return QDF_STATUS_E_FAILURE;
	}

	/* Forward to MAC via mesg = SIR_BB_XPORT_MGMT_MSG */
	msg.type = SIR_BB_XPORT_MGMT_MSG;
	msg.bodyptr = pkt;
	msg.bodyval = 0;
	msg.callback = pe_mc_process_handler;

	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_SCAN, &msg);

	return status;
}

/* --------------------------------------------------------------------------- */
/**
 * pe_handle_mgmt_frame() - Process the Management frames from TXRX
 * @psoc: psoc context
 * @peer: peer
 * @buf: buffer
 * @mgmt_rx_params; rx event params
 * @frm_type: frame type
 *
 * This function handles the mgmt rx frame from mgmt txrx component and forms
 * a cds packet and schedule it in controller thread for further processing.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS pe_handle_mgmt_frame(struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_peer *peer, qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params,
			enum mgmt_frame_type frm_type)
{
	struct mac_context *mac;
	tpSirMacMgmtHdr mHdr;
	struct scheduler_msg msg = {0};
	cds_pkt_t *pVosPkt;
	QDF_STATUS qdf_status;
	uint8_t *pRxPacketInfo;
	int ret;

	/* skip offload packets */
	if ((ucfg_pkt_capture_get_mode(psoc) != PACKET_CAPTURE_MODE_DISABLE) &&
	    mgmt_rx_params->status & WMI_RX_OFFLOAD_MON_MODE) {
		qdf_nbuf_free(buf);
		return QDF_STATUS_SUCCESS;
	}

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		/* cannot log a failure without a valid mac */
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	if (mac->usr_cfg_disable_rsp_tx) {
		pe_debug("Drop Rx pkt with user config");
		qdf_nbuf_free(buf);
		return QDF_STATUS_SUCCESS;
	}
	pVosPkt = qdf_mem_malloc_atomic(sizeof(*pVosPkt));
	if (!pVosPkt) {
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_NOMEM;
	}

	ret = wma_form_rx_packet(buf, mgmt_rx_params, pVosPkt);
	if (ret) {
		pe_debug_rl("Failed to fill cds packet from event buffer");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_status =
		wma_ds_peek_rx_packet_info(pVosPkt, (void *)&pRxPacketInfo);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_pkt_return_packet(pVosPkt);
		pVosPkt = NULL;
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * The MPDU header is now present at a certain "offset" in
	 * the BD and is specified in the BD itself
	 */

	mHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);

	/*
	 * Filter the beacon/probe response frames before posting it
	 * on the PE queue
	 */
	if ((mHdr->fc.subType == SIR_MAC_MGMT_BEACON ||
	    mHdr->fc.subType == SIR_MAC_MGMT_PROBE_RSP) &&
	    !pe_filter_bcn_probe_frame(mac, mHdr, pRxPacketInfo)) {
		cds_pkt_return_packet(pVosPkt);
		pVosPkt = NULL;
		return QDF_STATUS_SUCCESS;
	}

	/*
	 * Post Probe Req frames to Scan queue and return
	 */
	if (mHdr->fc.subType == SIR_MAC_MGMT_PROBE_REQ) {
		qdf_status = pe_handle_probe_req_frames(mac, pVosPkt);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			cds_pkt_return_packet(pVosPkt);
			pVosPkt = NULL;
		}
		return qdf_status;
	}

	if (QDF_STATUS_SUCCESS !=
	    pe_drop_pending_rx_mgmt_frames(mac, mHdr, pVosPkt))
		return QDF_STATUS_E_FAILURE;

	/* Forward to MAC via mesg = SIR_BB_XPORT_MGMT_MSG */
	msg.type = SIR_BB_XPORT_MGMT_MSG;
	msg.bodyptr = pVosPkt;
	msg.bodyval = 0;

	if (QDF_STATUS_SUCCESS != sys_bbt_process_message_core(mac,
							 &msg,
							 mHdr->fc.type,
							 mHdr->fc.subType)) {
		cds_pkt_return_packet(pVosPkt);
		pVosPkt = NULL;
		/*
		 * Decrement sys_bbt_pending_mgmt_count if packet
		 * is dropped before posting to LIM
		 */
		lim_decrement_pending_mgmt_count(mac);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void pe_register_mgmt_rx_frm_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info;

	frm_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
	frm_cb_info.mgmt_rx_cb = pe_handle_mgmt_frame;

	status = wlan_mgmt_txrx_register_rx_cb(mac_ctx->psoc,
					 WLAN_UMAC_COMP_MLME, &frm_cb_info, 1);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("Registering the PE Handle with MGMT TXRX layer has failed");

	wma_register_mgmt_frm_client();
}

void pe_deregister_mgmt_rx_frm_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info;

	frm_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
	frm_cb_info.mgmt_rx_cb = pe_handle_mgmt_frame;

	status = wlan_mgmt_txrx_deregister_rx_cb(mac_ctx->psoc,
					 WLAN_UMAC_COMP_MLME, &frm_cb_info, 1);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("Deregistering the PE Handle with MGMT TXRX layer has failed");

	wma_de_register_mgmt_frm_client();
}


/**
 * pe_register_callbacks_with_wma() - register SME and PE callback functions to
 * WMA.
 * (function documentation in lim_api.h)
 */
void pe_register_callbacks_with_wma(struct mac_context *mac,
				    struct sme_ready_req *ready_req)
{
	QDF_STATUS status;

	status = wma_register_roaming_callbacks(
			ready_req->csr_roam_auth_event_handle_cb,
			ready_req->pe_roam_synch_cb,
			ready_req->pe_disconnect_cb,
			ready_req->pe_roam_set_ie_cb);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("Registering roaming callbacks with WMA failed");
}

void
lim_received_hb_handler(struct mac_context *mac, uint32_t chan_freq,
			struct pe_session *pe_session)
{
	if (chan_freq == 0 || chan_freq == pe_session->curr_op_freq)
		pe_session->LimRxedBeaconCntDuringHB++;

	pe_session->pmmOffloadInfo.bcnmiss = false;
} /*** lim_init_wds_info_params() ***/

/** -------------------------------------------------------------
   \fn lim_update_overlap_sta_param
   \brief Updates overlap cache and param data structure
   \param      struct mac_context *   mac
   \param      tSirMacAddr bssId
   \param      tpLimProtStaParams pStaParams
   \return      None
   -------------------------------------------------------------*/
void
lim_update_overlap_sta_param(struct mac_context *mac, tSirMacAddr bssId,
			     tpLimProtStaParams pStaParams)
{
	int i;

	if (!pStaParams->numSta) {
		qdf_mem_copy(mac->lim.protStaOverlapCache[0].addr,
			     bssId, sizeof(tSirMacAddr));
		mac->lim.protStaOverlapCache[0].active = true;

		pStaParams->numSta = 1;

		return;
	}

	for (i = 0; i < LIM_PROT_STA_OVERLAP_CACHE_SIZE; i++) {
		if (mac->lim.protStaOverlapCache[i].active) {
			if (!qdf_mem_cmp
				    (mac->lim.protStaOverlapCache[i].addr, bssId,
				    sizeof(tSirMacAddr))) {
				return;
			}
		} else
			break;
	}

	if (i == LIM_PROT_STA_OVERLAP_CACHE_SIZE) {
		pe_debug("Overlap cache is full");
	} else {
		qdf_mem_copy(mac->lim.protStaOverlapCache[i].addr,
			     bssId, sizeof(tSirMacAddr));
		mac->lim.protStaOverlapCache[i].active = true;

		pStaParams->numSta++;
	}
}

/**
 * lim_enc_type_matched() - matches security type of incoming beracon with
 * current
 * @mac_ctx      Pointer to Global MAC structure
 * @bcn          Pointer to parsed Beacon structure
 * @session      PE session entry
 *
 * This function matches security type of incoming beracon with current
 *
 * @return true if matched, false otherwise
 */
static bool
lim_enc_type_matched(struct mac_context *mac_ctx,
		     tpSchBeaconStruct bcn,
		     struct pe_session *session)
{
	if (!bcn || !session)
		return false;

	/*
	 * This is handled by sending probe req due to IOT issues so
	 * return TRUE
	 */
	if ((bcn->capabilityInfo.privacy) !=
		SIR_MAC_GET_PRIVACY(session->limCurrentBssCaps)) {
		pe_warn("Privacy bit miss match");
		return true;
	}

	/* Open */
	if ((bcn->capabilityInfo.privacy == 0) &&
	    (session->encryptType == eSIR_ED_NONE))
		return true;

	/* WEP */
	if ((bcn->capabilityInfo.privacy == 1) &&
	    (bcn->wpaPresent == 0) && (bcn->rsnPresent == 0) &&
	    ((session->encryptType == eSIR_ED_WEP40) ||
		(session->encryptType == eSIR_ED_WEP104)
#ifdef FEATURE_WLAN_WAPI
		|| (session->encryptType == eSIR_ED_WPI)
#endif
	    ))
		return true;

	/* WPA OR RSN*/
	if ((bcn->capabilityInfo.privacy == 1) &&
	    ((bcn->wpaPresent == 1) || (bcn->rsnPresent == 1)) &&
	    ((session->encryptType == eSIR_ED_TKIP) ||
		(session->encryptType == eSIR_ED_CCMP) ||
		(session->encryptType == eSIR_ED_GCMP) ||
		(session->encryptType == eSIR_ED_GCMP_256) ||
		(session->encryptType == eSIR_ED_AES_128_CMAC)))
		return true;

	/*
	 * For HS2.0, RSN ie is not present
	 * in beacon. Therefore no need to
	 * check for security type in case
	 * OSEN session.
	 * For WPS registration session no need to detect
	 * detect security mismatch as it won't match and
	 * driver may end up sending probe request without
	 * WPS IE during WPS registration process.
	 */
	if (session->isOSENConnection ||
	   session->wps_registration)
		return true;

	pe_debug("AP:: Privacy %d WPA %d RSN %d, SELF:: Privacy %d Enc %d OSEN %d WPS %d",
		 bcn->capabilityInfo.privacy, bcn->wpaPresent, bcn->rsnPresent,
		 SIR_MAC_GET_PRIVACY(session->limCurrentBssCaps),
		 session->encryptType, session->isOSENConnection,
		 session->wps_registration);

	return false;
}

void
lim_detect_change_in_ap_capabilities(struct mac_context *mac,
				     tpSirProbeRespBeacon pBeacon,
				     struct pe_session *pe_session,
				     bool is_bcn)
{
	uint8_t len;
	uint32_t new_chan_freq;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool security_caps_matched = true;
	uint16_t ap_cap;

	ap_cap = lim_get_u16((uint8_t *) &pBeacon->capabilityInfo);
	new_chan_freq = pBeacon->chan_freq;

	security_caps_matched = lim_enc_type_matched(mac, pBeacon,
						     pe_session);
	if ((false == pe_session->limSentCapsChangeNtf) &&
	    (((!lim_is_null_ssid(&pBeacon->ssId)) &&
	       lim_cmp_ssid(&pBeacon->ssId, pe_session)) ||
	     ((SIR_MAC_GET_ESS(ap_cap) !=
	       SIR_MAC_GET_ESS(pe_session->limCurrentBssCaps)) ||
	      (SIR_MAC_GET_PRIVACY(ap_cap) !=
	       SIR_MAC_GET_PRIVACY(pe_session->limCurrentBssCaps)) ||
	      (SIR_MAC_GET_QOS(ap_cap) !=
	       SIR_MAC_GET_QOS(pe_session->limCurrentBssCaps)) ||
	      ((new_chan_freq != pe_session->curr_op_freq) &&
		(new_chan_freq != 0)) ||
	      (false == security_caps_matched)
	     ))) {
		if (!pe_session->fWaitForProbeRsp || is_bcn) {
			/* If Beacon capabilities is not matching with the current capability,
			 * then send unicast probe request to AP and take decision after
			 * receiving probe response */
			if (pe_session->fIgnoreCapsChange) {
				pe_debug_rl("Ignore the Capability change as probe rsp Capability matched");
				return;
			}
			pe_session->fWaitForProbeRsp = true;
			pe_info(QDF_MAC_ADDR_FMT ": capabilities are not matching, sending directed probe request",
				QDF_MAC_ADDR_REF(pe_session->bssId));
			status =
				lim_send_probe_req_mgmt_frame(
					mac, &pe_session->ssId,
					pe_session->bssId,
					pe_session->curr_op_freq,
					pe_session->self_mac_addr,
					pe_session->dot11mode,
					NULL, NULL);

			if (QDF_STATUS_SUCCESS != status) {
				pe_err("send ProbeReq failed");
				pe_session->fWaitForProbeRsp = false;
			}
			return;
		}
		/**
		 * BSS capabilities have changed.
		 * Inform Roaming.
		 */
		len = sizeof(tSirMacCapabilityInfo) + sizeof(tSirMacAddr) + sizeof(uint8_t) + 3 * sizeof(uint8_t) + /* reserved fields */
		      pBeacon->ssId.length + 1;

		if (new_chan_freq != pe_session->curr_op_freq) {
			pe_info(QDF_MAC_ADDR_FMT ": Channel freq Change from %d --> %d Ignoring beacon!",
				QDF_MAC_ADDR_REF(pe_session->bssId),
				pe_session->curr_op_freq, new_chan_freq);
			return;
		}

		/**
		 * When Cisco 1262 Enterprise APs are configured with WPA2-PSK with
		 * AES+TKIP Pairwise ciphers and WEP-40 Group cipher, they do not set
		 * the privacy bit in Beacons (wpa/rsnie is still present in beacons),
		 * the privacy bit is set in Probe and association responses.
		 * Due to this anomaly, we detect a change in
		 * AP capabilities when we receive a beacon after association and
		 * disconnect from the AP. The following check makes sure that we can
		 * connect to such APs
		 */
		else if ((SIR_MAC_GET_PRIVACY(ap_cap) == 0) &&
			 (pBeacon->rsnPresent || pBeacon->wpaPresent)) {
			pe_info_rl(QDF_MAC_ADDR_FMT ": BSS Caps (Privacy) bit 0 in beacon, but WPA or RSN IE present, Ignore Beacon!",
				   QDF_MAC_ADDR_REF(pe_session->bssId));
			return;
		}

		pe_session->fIgnoreCapsChange = false;
		pe_session->fWaitForProbeRsp = false;
		pe_session->limSentCapsChangeNtf = true;
		pe_info(QDF_MAC_ADDR_FMT ": initiate Disconnect due to cap mismatch!",
			QDF_MAC_ADDR_REF(pe_session->bssId));
		lim_send_deauth_mgmt_frame(mac, REASON_UNSPEC_FAILURE,
					   pe_session->bssId, pe_session,
					   false);
		lim_tear_down_link_with_ap(mac, pe_session->peSessionId,
					   REASON_UNSPEC_FAILURE,
					   eLIM_HOST_DISASSOC);
	} else if (pe_session->fWaitForProbeRsp) {
		/* Only for probe response frames and matching capabilities the control
		 * will come here. If beacon is with broadcast ssid then fWaitForProbeRsp
		 * will be false, the control will not come here*/

		pe_debug(QDF_MAC_ADDR_FMT ": capabilities in probe rsp are matching, so ignoring capability mismatch",
			 QDF_MAC_ADDR_REF(pe_session->bssId));
		pe_session->fIgnoreCapsChange = true;
		pe_session->fWaitForProbeRsp = false;
	}

} /*** lim_detect_change_in_ap_capabilities() ***/

/* --------------------------------------------------------------------- */
/**
 * lim_update_short_slot
 *
 * FUNCTION:
 * Enable/Disable short slot
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param enable        Flag to enable/disable short slot
 * @return None
 */

QDF_STATUS lim_update_short_slot(struct mac_context *mac,
				    tpSirProbeRespBeacon pBeacon,
				    tpUpdateBeaconParams pBeaconParams,
				    struct pe_session *pe_session)
{

	uint16_t ap_cap;
	uint32_t nShortSlot;
	uint32_t phyMode;

	/* Check Admin mode first. If it is disabled just return */
	if (!mac->mlme_cfg->feature_flags.enable_short_slot_time_11g)
		return QDF_STATUS_SUCCESS;

	/* Check for 11a mode or 11b mode. In both cases return since slot time is constant and cannot/should not change in beacon */
	lim_get_phy_mode(mac, &phyMode, pe_session);
	if ((phyMode == WNI_CFG_PHY_MODE_11A)
	    || (phyMode == WNI_CFG_PHY_MODE_11B))
		return QDF_STATUS_SUCCESS;

	ap_cap = lim_get_u16((uint8_t *) &pBeacon->capabilityInfo);

	/*  Earlier implementation: determine the appropriate short slot mode based on AP advertised modes */
	/* when erp is present, apply short slot always unless, prot=on  && shortSlot=off */
	/* if no erp present, use short slot based on current ap caps */

	/* Issue with earlier implementation : Cisco 1231 BG has shortSlot = 0, erpIEPresent and useProtection = 0 (Case4); */

	/* Resolution : always use the shortSlot setting the capability info to decide slot time. */
	/* The difference between the earlier implementation and the new one is only Case4. */
	/*
	   ERP IE Present  |   useProtection   |   shortSlot   =   QC STA Short Slot
	   Case1        1                                   1                       1                       1           //AP should not advertise this combination.
	   Case2        1                                   1                       0                       0
	   Case3        1                                   0                       1                       1
	   Case4        1                                   0                       0                       0
	   Case5        0                                   1                       1                       1
	   Case6        0                                   1                       0                       0
	   Case7        0                                   0                       1                       1
	   Case8        0                                   0                       0                       0
	 */
	nShortSlot = SIR_MAC_GET_SHORT_SLOT_TIME(ap_cap);

	if (nShortSlot != pe_session->shortSlotTimeSupported) {
		/* Short slot time capability of AP has changed. Adopt to it. */
		pe_debug("Shortslot capability of AP changed: %d",
			       nShortSlot);
			((tpSirMacCapabilityInfo) & pe_session->
			limCurrentBssCaps)->shortSlotTime = (uint16_t) nShortSlot;
		pe_session->shortSlotTimeSupported = nShortSlot;
		pBeaconParams->fShortSlotTime = (uint8_t) nShortSlot;
		pBeaconParams->paramChangeBitmap |=
			PARAM_SHORT_SLOT_TIME_CHANGED;
	}
	return QDF_STATUS_SUCCESS;
}


void lim_send_heart_beat_timeout_ind(struct mac_context *mac,
				     struct pe_session *pe_session)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};

	/* Prepare and post message to LIM Message Queue */
	msg.type = (uint16_t) SIR_LIM_HEART_BEAT_TIMEOUT;
	msg.bodyptr = pe_session;
	msg.bodyval = 0;
	pe_err("Heartbeat failure from Fw");

	status = lim_post_msg_api(mac, &msg);

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("posting message: %X to LIM failed, reason: %d",
			msg.type, status);
	}
}

void lim_ps_offload_handle_missed_beacon_ind(struct mac_context *mac,
					     struct scheduler_msg *msg)
{
	struct missed_beacon_ind *missed_beacon_ind = msg->bodyptr;
	struct pe_session *pe_session =
		pe_find_session_by_vdev_id(mac, missed_beacon_ind->bss_idx);

	if (!pe_session) {
		pe_err("session does not exist for vdev_id %d",
			missed_beacon_ind->bss_idx);
		return;
	}

	/* Set Beacon Miss in Powersave Offload */
	pe_session->pmmOffloadInfo.bcnmiss = true;
	pe_err("Received Heart Beat Failure");

	/*  Do AP probing immediately */
	lim_send_heart_beat_timeout_ind(mac, pe_session);
}

bool lim_is_sb_disconnect_allowed_fl(struct pe_session *session,
				     const char *func, uint32_t line)
{
	if (session->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE &&
	    session->limSmeState != eLIM_SME_WT_DISASSOC_STATE &&
	    session->limSmeState != eLIM_SME_WT_DEAUTH_STATE)
		return true;

	pe_nofl_info("%s:%u: Vdev %d (%d): limMlmState %s(%x) limSmeState %s(%x)",
		     func, line, session->vdev_id, session->peSessionId,
		     lim_mlm_state_str(session->limMlmState),
		     session->limMlmState,
		     lim_sme_state_str(session->limSmeState),
		     session->limSmeState);

	return false;
}

#ifdef WLAN_SUPPORT_TWT
#ifdef WLAN_TWT_CONV_SUPPORTED
void lim_set_twt_peer_capabilities(struct mac_context *mac_ctx,
				   struct qdf_mac_addr *peer_mac,
				   tDot11fIEhe_cap *he_cap,
				   tDot11fIEhe_op *he_op)
{
	uint8_t caps = 0;

	if (he_cap->twt_request)
		caps |= WLAN_TWT_CAPA_REQUESTOR;

	if (he_cap->twt_responder)
		caps |= WLAN_TWT_CAPA_RESPONDER;

	if (he_cap->broadcast_twt)
		caps |= WLAN_TWT_CAPA_BROADCAST;

	if (he_cap->flex_twt_sched)
		caps |= WLAN_TWT_CAPA_FLEXIBLE;

	if (he_op->twt_required)
		caps |= WLAN_TWT_CAPA_REQUIRED;

	wlan_set_peer_twt_capabilities(mac_ctx->psoc, peer_mac, caps);
}

void lim_set_twt_ext_capabilities(struct mac_context *mac_ctx,
				  struct qdf_mac_addr *peer_mac,
				  struct s_ext_cap *ext_cap)
{
	uint8_t caps = 0;

	if (ext_cap->twt_requestor_support)
		caps |= WLAN_TWT_CAPA_REQUESTOR;

	if (ext_cap->twt_responder_support)
		caps |= WLAN_TWT_CAPA_RESPONDER;

	wlan_set_peer_twt_capabilities(mac_ctx->psoc, peer_mac, caps);
}
#else
void lim_set_twt_peer_capabilities(struct mac_context *mac_ctx,
				   struct qdf_mac_addr *peer_mac,
				   tDot11fIEhe_cap *he_cap,
				   tDot11fIEhe_op *he_op)
{
	uint8_t caps = 0;

	if (he_cap->twt_request)
		caps |= WLAN_TWT_CAPA_REQUESTOR;

	if (he_cap->twt_responder)
		caps |= WLAN_TWT_CAPA_RESPONDER;

	if (he_cap->broadcast_twt)
		caps |= WLAN_TWT_CAPA_BROADCAST;

	if (he_cap->flex_twt_sched)
		caps |= WLAN_TWT_CAPA_FLEXIBLE;

	if (he_op->twt_required)
		caps |= WLAN_TWT_CAPA_REQUIRED;

	mlme_set_twt_peer_capabilities(mac_ctx->psoc, peer_mac,
				       caps);
}

void lim_set_twt_ext_capabilities(struct mac_context *mac_ctx,
				  struct qdf_mac_addr *peer_mac,
				  struct s_ext_cap *ext_cap)
{
	uint8_t caps = 0;

	if (ext_cap->twt_requestor_support)
		caps |= WLAN_TWT_CAPA_REQUESTOR;

	if (ext_cap->twt_responder_support)
		caps |= WLAN_TWT_CAPA_RESPONDER;

	mlme_set_twt_peer_capabilities(mac_ctx->psoc, peer_mac, caps);
}
#endif /* WLAN_TWT_CONV_SUPPORTED */
#endif /* WLAN_SUPPORT_TWT */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void pe_update_crypto_params(struct mac_context *mac_ctx,
				    struct pe_session *ft_session,
				    struct roam_offload_synch_ind *roam_synch)
{
	uint8_t *assoc_ies;
	uint32_t assoc_ies_len;
	uint8_t ies_offset = WLAN_REASSOC_REQ_IES_OFFSET;
	tpSirMacMgmtHdr hdr;
	const uint8_t *wpa_ie, *rsn_ie;
	uint32_t wpa_oui;
	struct wlan_crypto_params *crypto_params;

	hdr = (tpSirMacMgmtHdr)((uint8_t *)roam_synch +
		roam_synch->reassoc_req_offset);
	if (hdr->fc.type == SIR_MAC_MGMT_FRAME &&
	    hdr->fc.subType == SIR_MAC_MGMT_ASSOC_REQ) {
		ies_offset = WLAN_ASSOC_REQ_IES_OFFSET;
		pe_debug("roam assoc req frm");
	} else {
		pe_debug("roam reassoc req frm");
	}

	if (roam_synch->reassoc_req_length <
	    (sizeof(tSirMacMgmtHdr) + ies_offset)) {
		pe_err("invalid reassoc req len %d",
		       roam_synch->reassoc_req_length);
		return;
	}
	qdf_trace_hex_dump(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   (uint8_t *)roam_synch +
				roam_synch->reassoc_req_offset,
			   roam_synch->reassoc_req_length);

	ft_session->limRmfEnabled = false;

	assoc_ies = (uint8_t *)roam_synch + roam_synch->reassoc_req_offset +
				sizeof(tSirMacMgmtHdr) + ies_offset;
	assoc_ies_len = roam_synch->reassoc_req_length -
				sizeof(tSirMacMgmtHdr) - ies_offset;

	rsn_ie = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSN, assoc_ies,
					  assoc_ies_len);
	wpa_oui = WLAN_WPA_SEL(WLAN_WPA_OUI_TYPE);
	wpa_ie = wlan_get_vendor_ie_ptr_from_oui((uint8_t *)&wpa_oui,
						 WLAN_OUI_SIZE, assoc_ies,
						 assoc_ies_len);
	if (!wpa_ie && !rsn_ie) {
		pe_nofl_debug("RSN and WPA IE not present");
		return;
	}

	wlan_set_vdev_crypto_prarams_from_ie(ft_session->vdev, assoc_ies,
					     assoc_ies_len);
	ft_session->limRmfEnabled =
		lim_get_vdev_rmf_capable(mac_ctx, ft_session);
	crypto_params = wlan_crypto_vdev_get_crypto_params(ft_session->vdev);
	if (!crypto_params) {
		pe_err("crypto params is null");
		return;
	}

	ft_session->connected_akm =
		lim_get_connected_akm(ft_session, crypto_params->ucastcipherset,
				      crypto_params->authmodeset,
				      crypto_params->key_mgmt);
	ft_session->encryptType =
		lim_get_encrypt_ed_type(crypto_params->ucastcipherset);
	pe_nofl_debug("vdev %d roam auth 0x%x akm 0x%0x rsn_caps 0x%x ucastcipher 0x%x akm %d enc: %d",
		      ft_session->vdev_id,
		      crypto_params->authmodeset,
		      crypto_params->key_mgmt,
		      crypto_params->rsn_caps,
		      crypto_params->ucastcipherset,
		      ft_session->connected_akm,
		      ft_session->encryptType);
}

/**
 * sir_parse_bcn_fixed_fields() - Parse fixed fields in Beacon IE's
 *
 * @mac_ctx: MAC Context
 * @beacon_struct: Beacon/Probe Response structure
 * @buf: Fixed Fields buffer
 */
static void sir_parse_bcn_fixed_fields(struct mac_context *mac_ctx,
					tpSirProbeRespBeacon beacon_struct,
					uint8_t *buf)
{
	tDot11fFfCapabilities dst;

	beacon_struct->timeStamp[0] = lim_get_u32(buf);
	beacon_struct->timeStamp[1] = lim_get_u32(buf + 4);
	buf += 8;

	beacon_struct->beaconInterval = lim_get_u16(buf);
	buf += 2;

	dot11f_unpack_ff_capabilities(mac_ctx, buf, &dst);

	sir_copy_caps_info(mac_ctx, dst, beacon_struct);
}

static QDF_STATUS
lim_roam_gen_mbssid_beacon(struct mac_context *mac,
			   struct roam_offload_synch_ind *roam_ind,
			   tpSirProbeRespBeacon parsed_frm,
			   uint8_t **ie, uint32_t *ie_len)
{
	qdf_list_t *scan_list;
	struct mgmt_rx_event_params rx_param = {0};
	uint8_t list_count = 0, i;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	qdf_list_node_t *next_node = NULL, *cur_node = NULL;
	struct scan_cache_node *scan_node;
	struct scan_cache_entry *scan_entry;
	uint8_t *bcn_prb_ptr;
	uint32_t nontx_bcn_prbrsp_len = 0, offset, length;
	uint8_t *nontx_bcn_prbrsp = NULL;
	uint8_t ie_offset;

	ie_offset = SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET;
	bcn_prb_ptr = (uint8_t *)roam_ind +
				roam_ind->beacon_probe_resp_offset;

	rx_param.chan_freq = roam_ind->chan_freq;
	rx_param.pdev_id = wlan_objmgr_pdev_get_pdev_id(mac->pdev);
	rx_param.rssi = roam_ind->rssi;

	/* Set all per chain rssi as invalid */
	for (i = 0; i < WLAN_MGMT_TXRX_HOST_MAX_ANTENNA; i++)
		rx_param.rssi_ctl[i] = WLAN_INVALID_PER_CHAIN_RSSI;

	scan_list = util_scan_unpack_beacon_frame(mac->pdev, bcn_prb_ptr,
						  roam_ind->beacon_probe_resp_length,
						  MGMT_SUBTYPE_BEACON, &rx_param);
	if (!scan_list) {
		pe_err("failed to parse");
		return QDF_STATUS_E_FAILURE;
	}

	list_count = qdf_list_size(scan_list);
	status = qdf_list_peek_front(scan_list, &cur_node);
	if (QDF_IS_STATUS_ERROR(status) || !cur_node) {
		pe_debug("list peek front failure. list size %d", list_count);
		goto error;
	}

	for (i = 1; i < list_count; i++) {
		scan_node = qdf_container_of(cur_node,
					     struct scan_cache_node, node);
		scan_entry = scan_node->entry;
		if (qdf_is_macaddr_equal(&roam_ind->bssid,
					 &scan_entry->bssid)) {
			pe_debug("matched BSSID "QDF_MAC_ADDR_FMT" bcn len %d profiles %d",
				 QDF_MAC_ADDR_REF(scan_entry->bssid.bytes),
				 scan_entry->raw_frame.len,
				 list_count);
			nontx_bcn_prbrsp = scan_entry->raw_frame.ptr;
			nontx_bcn_prbrsp_len = scan_entry->raw_frame.len;
			QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE,
					   QDF_TRACE_LEVEL_DEBUG,
					   scan_entry->raw_frame.ptr,
					   nontx_bcn_prbrsp_len);
			break;
		}
		status = qdf_list_peek_next(scan_list, cur_node, &next_node);
		if (QDF_IS_STATUS_ERROR(status) || !next_node) {
			pe_debug("list remove failure i:%d, lsize:%d",
				 i, list_count);
			goto error;
		}
		cur_node = next_node;
	}

	if (!nontx_bcn_prbrsp_len) {
		pe_debug("failed to generate/find MBSSID beacon");
		goto error;
	}

	if (roam_ind->is_beacon) {
		offset = SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET;
		length = nontx_bcn_prbrsp_len - SIR_MAC_HDR_LEN_3A;
		if (sir_parse_beacon_ie(mac, parsed_frm,
					&nontx_bcn_prbrsp[offset],
					length) != QDF_STATUS_SUCCESS ||
			!parsed_frm->ssidPresent) {
			pe_err("Parse error Beacon, length: %d",
			       roam_ind->beacon_probe_resp_length);
			status =  QDF_STATUS_E_FAILURE;
			goto error;
		}
	} else {
		offset = SIR_MAC_HDR_LEN_3A;
		length = nontx_bcn_prbrsp_len - SIR_MAC_HDR_LEN_3A;
		if (sir_convert_probe_frame2_struct(mac,
					    &nontx_bcn_prbrsp[offset],
					    length,
					    parsed_frm) != QDF_STATUS_SUCCESS ||
			!parsed_frm->ssidPresent) {
			pe_err("Parse error ProbeResponse, length: %d",
			       roam_ind->beacon_probe_resp_length);
			status = QDF_STATUS_E_FAILURE;
			goto error;
		}
	}

	*ie_len = nontx_bcn_prbrsp_len - ie_offset;
	if (*ie_len) {
		*ie = qdf_mem_malloc(*ie_len);
		if (!*ie)
			return QDF_STATUS_E_NOMEM;
		qdf_mem_copy(*ie, nontx_bcn_prbrsp + ie_offset, *ie_len);
		pe_debug("beacon/probe Ie length: %d", *ie_len);
	}
error:
	for (i = 0; i < list_count; i++) {
		status = qdf_list_remove_front(scan_list, &next_node);
		if (QDF_IS_STATUS_ERROR(status) || !next_node) {
			pe_debug("list remove failure i:%d, lsize:%d",
				 i, list_count);
			break;
		}
		scan_node = qdf_container_of(next_node,
					     struct scan_cache_node, node);
		util_scan_free_cache_entry(scan_node->entry);
		qdf_mem_free(scan_node);
	}
	qdf_mem_free(scan_list);

	return status;
}

static QDF_STATUS
lim_roam_gen_beacon_descr(struct mac_context *mac,
			  uint8_t *bcn_prb_ptr,
			  uint16_t bcn_prb_len, bool is_mlo_link,
			  struct roam_offload_synch_ind *roam_ind,
			  tpSirProbeRespBeacon parsed_frm,
			  uint8_t **ie, uint32_t *ie_len,
			  struct qdf_mac_addr *bssid)
{
	QDF_STATUS status;
	tpSirMacMgmtHdr mac_hdr;
	uint8_t ie_offset;
	bool is_beacon;

	mac_hdr = (tpSirMacMgmtHdr)bcn_prb_ptr;
	ie_offset = SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)mac_hdr->bssId)) {
		pe_debug("bssid is 0 in beacon/probe update it with bssId"
			 QDF_MAC_ADDR_FMT "in sync ind",
			 QDF_MAC_ADDR_REF(bssid->bytes));
		qdf_mem_copy(mac_hdr->bssId, bssid->bytes,
			     sizeof(tSirMacAddr));
	}

	is_beacon = is_mlo_link ? roam_ind->is_link_beacon : roam_ind->is_beacon;

	if ((!is_multi_link_roam(roam_ind)) &&
	    (qdf_mem_cmp(bssid->bytes,
			 &mac_hdr->bssId, QDF_MAC_ADDR_SIZE) != 0)) {
		pe_debug("LFR3:MBSSID Beacon/Prb Rsp: %d bssid "
			 QDF_MAC_ADDR_FMT,
			 roam_ind->is_beacon,
			 QDF_MAC_ADDR_REF(mac_hdr->bssId));
		/*
		 * Its a MBSSID non-tx BSS roaming scenario.
		 * Generate non tx BSS beacon/probe response
		 */
		status = lim_roam_gen_mbssid_beacon(mac,
						    roam_ind,
						    parsed_frm,
						    ie, ie_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("failed to gen mbssid beacon");
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		if (is_beacon) {
			if (sir_parse_beacon_ie(mac, parsed_frm,
				&bcn_prb_ptr[SIR_MAC_HDR_LEN_3A +
				SIR_MAC_B_PR_SSID_OFFSET],
				bcn_prb_len - SIR_MAC_HDR_LEN_3A) !=
						QDF_STATUS_SUCCESS ||
			    !parsed_frm->ssidPresent) {
				pe_err("Parse error Beacon, length: %d",
				       bcn_prb_len);
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			if (sir_convert_probe_frame2_struct(mac,
				&bcn_prb_ptr[SIR_MAC_HDR_LEN_3A],
				bcn_prb_len -
				SIR_MAC_HDR_LEN_3A, parsed_frm) !=
				QDF_STATUS_SUCCESS ||
				!parsed_frm->ssidPresent) {
				pe_err("Parse error ProbeResponse, length: %d",
				       bcn_prb_len);
				return QDF_STATUS_E_FAILURE;
			}
		}
		/* 24 byte MAC header and 12 byte to ssid IE */
		if (bcn_prb_len > ie_offset) {
			*ie_len = bcn_prb_len - ie_offset;
			*ie = qdf_mem_malloc(*ie_len);
			if (!*ie)
				return QDF_STATUS_E_NOMEM;
			qdf_mem_copy(*ie, bcn_prb_ptr + ie_offset, *ie_len);
			pe_debug("beacon/probe Ie length: %d", *ie_len);
		}
	}
	/*
	 * For probe response, unpack core parses beacon interval, capabilities,
	 * timestamp. For beacon IEs, these fields are not parsed.
	 */
	if (is_beacon)
		sir_parse_bcn_fixed_fields(mac, parsed_frm,
			&bcn_prb_ptr[SIR_MAC_HDR_LEN_3A]);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
lim_roam_fill_bss_descr(struct mac_context *mac,
			struct roam_offload_synch_ind *roam_synch_ind,
			struct bss_description *bss_desc_ptr,
			struct pe_session *session)
{
	uint32_t ie_len = 0;
	tpSirProbeRespBeacon parsed_frm_ptr = NULL;
	tpSirMacMgmtHdr mac_hdr;
	uint8_t *bcn_proberesp_ptr = NULL;
	uint16_t bcn_proberesp_len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t *ie = NULL;
	struct qdf_mac_addr bssid;
	bool is_mlo_link;
	uint8_t vdev_id = session->vdev_id;
	struct element_info frame;
	struct cm_roam_values_copy mdie_cfg = {0};

	bcn_proberesp_ptr = (uint8_t *)roam_synch_ind +
		roam_synch_ind->beacon_probe_resp_offset;
	bcn_proberesp_len = roam_synch_ind->beacon_probe_resp_length;

	frame.ptr = NULL;
	frame.len = 0;
	if (is_multi_link_roam(roam_synch_ind)) {
		mlo_get_sta_link_mac_addr(vdev_id, roam_synch_ind, &bssid);
		is_mlo_link = wlan_vdev_mlme_get_is_mlo_link(mac->psoc, vdev_id);

		status = wlan_scan_get_entry_by_mac_addr(mac->pdev, &bssid,
							 &frame);
		if (QDF_IS_STATUS_ERROR(status) || !frame.len) {
			pe_err("Failed to get scan entry for " QDF_MAC_ADDR_FMT,
			       QDF_MAC_ADDR_REF(bssid.bytes));
			return status;
		}
		bcn_proberesp_ptr = frame.ptr;
		bcn_proberesp_len = frame.len;
	} else {
		bssid = roam_synch_ind->bssid;
		is_mlo_link = false;
	}

	mac_hdr = (tpSirMacMgmtHdr)bcn_proberesp_ptr;
	parsed_frm_ptr = qdf_mem_malloc(sizeof(tSirProbeRespBeacon));
	if (!parsed_frm_ptr) {
		status = QDF_STATUS_E_NOMEM;
		goto done;
	}

	if (bcn_proberesp_len <= SIR_MAC_HDR_LEN_3A) {
		pe_err("very few bytes in synchInd %s beacon / probe resp frame! length: %d",
		       is_mlo_link ? "link" : "", bcn_proberesp_len);
		status = QDF_STATUS_E_FAILURE;
		goto done;
	}

	pe_debug("LFR3:Beacon/Prb Rsp: %d bssid " QDF_MAC_ADDR_FMT
		 " beacon " QDF_MAC_ADDR_FMT,
		 is_mlo_link ? roam_synch_ind->is_link_beacon :
			roam_synch_ind->is_beacon,
		 QDF_MAC_ADDR_REF(bssid.bytes),
		 QDF_MAC_ADDR_REF(mac_hdr->bssId));

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   bcn_proberesp_ptr,
			   bcn_proberesp_len);

	status = lim_roam_gen_beacon_descr(mac, bcn_proberesp_ptr,
					   bcn_proberesp_len, is_mlo_link,
					   roam_synch_ind, parsed_frm_ptr,
					   &ie, &ie_len,
					   &bssid);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to parse beacon");
		status = QDF_STATUS_E_FAILURE;
		goto done;
	}

	/*
	 * Length of BSS description is without length of
	 * length itself and length of pointer
	 * that holds ieFields
	 *
	 * struct bss_description
	 * +--------+---------------------------------+---------------+
	 * | length | other fields                    | pointer to IEs|
	 * +--------+---------------------------------+---------------+
	 *                                            ^
	 *                                            ieFields
	 */
	bss_desc_ptr->length = (uint16_t) (offsetof(struct bss_description,
					   ieFields[0]) -
				sizeof(bss_desc_ptr->length) + ie_len);

	bss_desc_ptr->fProbeRsp = !(is_mlo_link ?
					roam_synch_ind->is_link_beacon :
					roam_synch_ind->is_beacon);
	bss_desc_ptr->rssi = roam_synch_ind->rssi;
	/* Copy Timestamp */
	bss_desc_ptr->scansystimensec = qdf_get_monotonic_boottime_ns();

	if (is_multi_link_roam(roam_synch_ind)) {
		bss_desc_ptr->chan_freq =
				mlo_roam_get_chan_freq(vdev_id, roam_synch_ind);
	} else if (parsed_frm_ptr->he_op.oper_info_6g_present) {
		bss_desc_ptr->chan_freq = wlan_reg_chan_band_to_freq(mac->pdev,
			parsed_frm_ptr->he_op.oper_info_6g.info.primary_ch,
			BIT(REG_BAND_6G));
	} else if (parsed_frm_ptr->dsParamsPresent) {
		bss_desc_ptr->chan_freq = parsed_frm_ptr->chan_freq;
	} else if (parsed_frm_ptr->HTInfo.present) {
		bss_desc_ptr->chan_freq =
			wlan_reg_legacy_chan_to_freq(mac->pdev,
						     parsed_frm_ptr->HTInfo.primaryChannel);
	} else {
		/*
		 * If DS Params or HTIE is not present in the probe resp or
		 * beacon, then use the channel frequency provided by firmware
		 * to fill the channel in the BSS descriptor.*/
		bss_desc_ptr->chan_freq = roam_synch_ind->chan_freq;
	}

	bss_desc_ptr->nwType = lim_get_nw_type(mac, bss_desc_ptr->chan_freq,
					       SIR_MAC_MGMT_FRAME,
					       parsed_frm_ptr);

	bss_desc_ptr->sinr = 0;
	bss_desc_ptr->beaconInterval = parsed_frm_ptr->beaconInterval;
	bss_desc_ptr->timeStamp[0]   = parsed_frm_ptr->timeStamp[0];
	bss_desc_ptr->timeStamp[1]   = parsed_frm_ptr->timeStamp[1];
	qdf_mem_copy(&bss_desc_ptr->capabilityInfo,
	&bcn_proberesp_ptr[SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_CAPAB_OFFSET], 2);

	qdf_mem_copy((uint8_t *) &bss_desc_ptr->bssId,
		     (uint8_t *)&bssid.bytes,
		     sizeof(tSirMacAddr));

	qdf_mem_copy((uint8_t *)&bss_desc_ptr->seq_ctrl,
		     (uint8_t *)&mac_hdr->seqControl,
		     sizeof(tSirMacSeqCtl));

	bss_desc_ptr->received_time =
		      (uint64_t)qdf_mc_timer_get_system_time();
	if (parsed_frm_ptr->mdiePresent) {
		bss_desc_ptr->mdiePresent = parsed_frm_ptr->mdiePresent;
		qdf_mem_copy((uint8_t *)bss_desc_ptr->mdie,
				(uint8_t *)parsed_frm_ptr->mdie,
				SIR_MDIE_SIZE);

		mdie_cfg.bool_value = true;
		mdie_cfg.uint_value =
			(bss_desc_ptr->mdie[1] << 8) | (bss_desc_ptr->mdie[0]);

		wlan_cm_roam_cfg_set_value(mac->psoc, vdev_id,
					   MOBILITY_DOMAIN, &mdie_cfg);
	}
	pe_debug("chan: %d rssi: %d ie_len %d mdie_present:%d mdie = %02x %02x %02x",
		 bss_desc_ptr->chan_freq,
		 bss_desc_ptr->rssi, ie_len, bss_desc_ptr->mdiePresent,
		 bss_desc_ptr->mdie[0], bss_desc_ptr->mdie[1], bss_desc_ptr->mdie[2]);

	if (ie_len) {
		qdf_mem_copy(&bss_desc_ptr->ieFields,
			     ie, ie_len);
		qdf_mem_free(ie);
	} else {
		pe_err("Beacon/Probe rsp doesn't have any IEs");
		status = QDF_STATUS_E_FAILURE;
		goto done;
	}
done:
	qdf_mem_free(frame.ptr);
	qdf_mem_free(parsed_frm_ptr);
	return status;
}

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * lim_copy_and_free_hlp_data_from_session - Copy HLP info
 * @session_ptr: PE session
 * @roam_sync_ind_ptr: Roam Synch Indication pointer
 *
 * This API is used to copy the parsed HLP info from PE session
 * to roam synch indication data. THe HLP info is expected to be
 * parsed/stored in PE session already from assoc IE's received
 * from fw as part of Roam Synch Indication.
 *
 * Return: None
 */
static void
lim_copy_and_free_hlp_data_from_session(struct pe_session *session_ptr,
					struct roam_offload_synch_ind
					*roam_sync_ind_ptr)
{
	if (session_ptr->fils_info->hlp_data &&
	    session_ptr->fils_info->hlp_data_len) {
		cds_copy_hlp_info(&session_ptr->fils_info->dst_mac,
				  &session_ptr->fils_info->src_mac,
				  session_ptr->fils_info->hlp_data_len,
				  session_ptr->fils_info->hlp_data,
				  &roam_sync_ind_ptr->dst_mac,
				  &roam_sync_ind_ptr->src_mac,
				  &roam_sync_ind_ptr->hlp_data_len,
				  roam_sync_ind_ptr->hlp_data);

		qdf_mem_free(session_ptr->fils_info->hlp_data);
		session_ptr->fils_info->hlp_data = NULL;
		session_ptr->fils_info->hlp_data_len = 0;
	}
}
#else
static inline void
lim_copy_and_free_hlp_data_from_session(struct pe_session *session_ptr,
					struct roam_offload_synch_ind
					*roam_sync_ind_ptr)
{}
#endif

static
uint8_t *lim_process_rmf_disconnect_frame(struct mac_context *mac,
					  struct pe_session *session,
					  uint8_t *deauth_disassoc_frame,
					  uint16_t deauth_disassoc_frame_len,
					  uint16_t *extracted_length)
{
	struct wlan_frame_hdr *mac_hdr;
	uint8_t mic_len, hdr_len, pdev_id;
	uint8_t *orig_ptr, *efrm;
	int32_t mgmtcipherset;
	uint32_t mmie_len;
	QDF_STATUS status;

	mac_hdr = (struct wlan_frame_hdr *)deauth_disassoc_frame;
	orig_ptr = (uint8_t *)mac_hdr;

	if (mac_hdr->i_fc[1] & IEEE80211_FC1_WEP) {
		if (QDF_IS_ADDR_BROADCAST(mac_hdr->i_addr1) ||
		    IEEE80211_IS_MULTICAST(mac_hdr->i_addr1)) {
			pe_err("Encrypted BC/MC frame dropping the frame");
			*extracted_length = 0;
			return NULL;
		}

		pdev_id = wlan_objmgr_pdev_get_pdev_id(mac->pdev);
		status = mlme_get_peer_mic_len(mac->psoc, pdev_id,
					       mac_hdr->i_addr2, &mic_len,
					       &hdr_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("Failed to get mic hdr and length");
			*extracted_length = 0;
			return NULL;
		}

		if (deauth_disassoc_frame_len <
		    (sizeof(*mac_hdr) + hdr_len + mic_len)) {
			pe_err("Frame len less than expected %d",
			       deauth_disassoc_frame_len);
			*extracted_length = 0;
			return NULL;
		}

		/*
		 * Strip the privacy headers and trailer
		 * for the received deauth/disassoc frame
		 */
		qdf_mem_move(orig_ptr + hdr_len, mac_hdr,
			     sizeof(*mac_hdr));
		*extracted_length = deauth_disassoc_frame_len -
				    (hdr_len + mic_len);
		return orig_ptr + hdr_len;
	}

	if (!(QDF_IS_ADDR_BROADCAST(mac_hdr->i_addr1) ||
	      IEEE80211_IS_MULTICAST(mac_hdr->i_addr1))) {
		pe_err("Rx unprotected unicast mgmt frame");
		*extracted_length = 0;
		return NULL;
	}

	mgmtcipherset = wlan_crypto_get_param(session->vdev,
					      WLAN_CRYPTO_PARAM_MGMT_CIPHER);
	if (mgmtcipherset < 0) {
		pe_err("Invalid mgmt cipher");
		*extracted_length = 0;
		return NULL;
	}

	mmie_len = (mgmtcipherset & (1 << WLAN_CRYPTO_CIPHER_AES_CMAC) ?
		    cds_get_mmie_size() : cds_get_gmac_mmie_size());

	efrm = orig_ptr + deauth_disassoc_frame_len;
	if (!mac->pmf_offload &&
	    !wlan_crypto_is_mmie_valid(session->vdev, orig_ptr, efrm)) {
		pe_err("Invalid MMIE");
		*extracted_length = 0;
		return NULL;
	}

	*extracted_length = deauth_disassoc_frame_len - mmie_len;

	return deauth_disassoc_frame;
}

QDF_STATUS
pe_disconnect_callback(struct mac_context *mac, uint8_t vdev_id,
		       uint8_t *deauth_disassoc_frame,
		       uint16_t deauth_disassoc_frame_len,
		       uint16_t reason_code)
{
	struct pe_session *session;
	uint8_t *extracted_frm = NULL;
	uint16_t extracted_frm_len;
	bool is_pmf_connection;

	session = pe_find_session_by_vdev_id(mac, vdev_id);
	if (!session) {
		pe_err("LFR3: Vdev %d doesn't exist", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!lim_is_sb_disconnect_allowed(session))
		return QDF_STATUS_SUCCESS;

	if (!deauth_disassoc_frame ||
	    deauth_disassoc_frame_len <
	    (sizeof(struct wlan_frame_hdr) + sizeof(reason_code))) {
		pe_err_rl("Discard invalid disconnect evt. frame len:%d",
			  deauth_disassoc_frame_len);
		goto end;
	}

	/*
	 * Use vdev pmf status instead of peer pmf capability as
	 * the firmware might roam to new AP in powersave case and
	 * roam synch can come before emergency deauth event.
	 * In that case, get peer will fail and reason code received
	 * from the WMI_ROAM_EVENTID  will be sent to upper layers.
	 */
	is_pmf_connection = lim_get_vdev_rmf_capable(mac, session);
	if (is_pmf_connection) {
		extracted_frm = lim_process_rmf_disconnect_frame(
						mac, session,
						deauth_disassoc_frame,
						deauth_disassoc_frame_len,
						&extracted_frm_len);
		if (!extracted_frm) {
			pe_err("PMF frame validation failed");
			goto end;
		}
	} else {
		extracted_frm = deauth_disassoc_frame;
		extracted_frm_len = deauth_disassoc_frame_len;
	}

	lim_extract_ies_from_deauth_disassoc(session, extracted_frm,
					     extracted_frm_len);

	reason_code = sir_read_u16(extracted_frm +
				   sizeof(struct wlan_frame_hdr));
end:
	lim_tear_down_link_with_ap(mac, session->peSessionId,
				   reason_code,
				   eLIM_PEER_ENTITY_DEAUTH);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
static void
lim_fill_fils_ft(struct pe_session *src_session,
		 struct pe_session *dst_session)
{
      if (src_session->fils_info &&
          src_session->fils_info->fils_ft_len) {
              dst_session->fils_info->fils_ft_len =
                      src_session->fils_info->fils_ft_len;
              qdf_mem_copy(dst_session->fils_info->fils_ft,
                           src_session->fils_info->fils_ft,
                           src_session->fils_info->fils_ft_len);
      }
}
#else
static inline void
lim_fill_fils_ft(struct pe_session *src_session,
		 struct pe_session *dst_session)
{}
#endif

#ifdef WLAN_SUPPORT_TWT
void
lim_fill_roamed_peer_twt_caps(struct mac_context *mac_ctx,
			      uint8_t vdev_id,
			      struct roam_offload_synch_ind *roam_synch)
{
	uint8_t *reassoc_body;
	uint16_t len;
	uint32_t status;
	tDot11fReAssocResponse *reassoc_rsp;
	struct pe_session *pe_session;
	struct qdf_mac_addr mac_addr;

	pe_session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!pe_session) {
		pe_err("session not found for given vdev_id %d", vdev_id);
		return;
	}

	reassoc_rsp = qdf_mem_malloc(sizeof(*reassoc_rsp));
	if (!reassoc_rsp)
		return;

	len = roam_synch->reassoc_resp_length - sizeof(tSirMacMgmtHdr);
	reassoc_body = (uint8_t *)roam_synch + sizeof(tSirMacMgmtHdr) +
			roam_synch->reassoc_resp_offset;

	status = dot11f_unpack_re_assoc_response(mac_ctx, reassoc_body, len,
						 reassoc_rsp, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a Re-association Rsp (0x%08x, %d bytes):",
		       status, len);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_INFO,
				   reassoc_body, len);
		qdf_mem_free(reassoc_rsp);
		return;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("Warnings while unpacking a Re-association Rsp (0x%08x, %d bytes):",
			 status, len);
	}

	if (lim_is_session_he_capable(pe_session)) {
		if (is_multi_link_roam(roam_synch))
			mlo_get_sta_link_mac_addr(vdev_id, roam_synch,
						  &mac_addr);
		else
			qdf_copy_macaddr(&mac_addr, &roam_synch->bssid);
		lim_set_twt_peer_capabilities(mac_ctx,
					      &mac_addr,
					      &reassoc_rsp->he_cap,
					      &reassoc_rsp->he_op);
	}
	qdf_mem_free(reassoc_rsp);
}
#endif

/**
 * lim_check_ft_initial_im_association() - To check FT initial mobility(im)
 * association
 * @roam_synch: A pointer to roam sync ind structure
 * @session_entry: pe session
 *
 * This function is to check ft_initial_im_association.
 *
 * Return: None
 */
void
lim_check_ft_initial_im_association(struct roam_offload_synch_ind *roam_synch,
				    struct pe_session *session_entry)
{
	tpSirMacMgmtHdr hdr;
	uint8_t *assoc_req_ptr;

	assoc_req_ptr = (uint8_t *) roam_synch + roam_synch->reassoc_req_offset;
	hdr = (tpSirMacMgmtHdr) assoc_req_ptr;

	if (hdr->fc.type == SIR_MAC_MGMT_FRAME &&
	    hdr->fc.subType == SIR_MAC_MGMT_ASSOC_REQ) {
		roam_synch->is_assoc = true;
		if (session_entry->is11Rconnection) {
			pe_debug("Frame subtype: %d and connection is %d",
				 hdr->fc.subType,
				 session_entry->is11Rconnection);
			roam_synch->is_ft_im_roam = true;
		}
	}
}

#ifdef WLAN_FEATURE_11BE_MLO
static void
lim_mlo_roam_copy_partner_info_to_session(struct pe_session *session,
					  struct roam_offload_synch_ind *sync_ind)
{
	mlo_roam_copy_partner_info(&session->ml_partner_info,
				   sync_ind, sync_ind->roamed_vdev_id, false);
}

static QDF_STATUS
lim_gen_link_specific_assoc_rsp(struct mac_context *mac_ctx,
				struct pe_session *session_entry,
				uint8_t *reassoc_rsp,
				uint32_t reassoc_rsp_len)
{
	struct element_info link_reassoc_rsp;
	struct qdf_mac_addr sta_link_addr;
	struct mlo_partner_info *ml_partner_info;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t idx = 0, num_partner_links, link_id, link_vdev_id;

	link_reassoc_rsp.ptr = qdf_mem_malloc(reassoc_rsp_len);
	if (!link_reassoc_rsp.ptr)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(&sta_link_addr, session_entry->self_mac_addr,
		     QDF_MAC_ADDR_SIZE);

	link_reassoc_rsp.len = reassoc_rsp_len;

	ml_partner_info = &session_entry->ml_partner_info;
	num_partner_links = ml_partner_info->num_partner_links;
	for (idx = 0; idx < num_partner_links; idx++) {
		link_vdev_id = ml_partner_info->partner_link_info[idx].vdev_id;
		if (link_vdev_id == WLAN_INVALID_VDEV_ID)
			continue;

		if (link_vdev_id != session_entry->vdev_id)
			continue;

		link_id = ml_partner_info->partner_link_info[idx].link_id;
		status = util_gen_link_assoc_rsp(
					reassoc_rsp + WLAN_MAC_HDR_LEN_3A,
					reassoc_rsp_len - WLAN_MAC_HDR_LEN_3A,
					true, link_id, sta_link_addr,
					link_reassoc_rsp.ptr, reassoc_rsp_len,
					(qdf_size_t *)&link_reassoc_rsp.len);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("MLO ROAM: link_id:%d vdev:%d Reassoc generation failed %d",
			       link_id, link_vdev_id, status);
			goto end;
		}

		lim_process_assoc_rsp_frame(mac_ctx, link_reassoc_rsp.ptr,
					    link_reassoc_rsp.len - WLAN_MAC_HDR_LEN_3A,
					    LIM_REASSOC, session_entry);
	}
end:
	qdf_mem_free(link_reassoc_rsp.ptr);
	link_reassoc_rsp.len = 0;
	return status;
}

#else
static inline void
lim_mlo_roam_copy_partner_info_to_session(struct pe_session *session,
					struct roam_offload_synch_ind *sync_ind)
{}

static QDF_STATUS
lim_gen_link_specific_assoc_rsp(struct mac_context *mac_ctx,
				struct pe_session *session_entry,
				uint8_t *reassoc_rsp,
				uint32_t reassoc_rsp_len)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#ifdef WLAN_FEATURE_11AX
static void pe_roam_fill_obss_scan_param(struct pe_session *src_session,
					 struct pe_session *dst_session)
{
	dst_session->obss_color_collision_dec_evt =
				src_session->obss_color_collision_dec_evt;
	dst_session->he_op.bss_color = src_session->he_op.bss_color;
}
#else
static inline void pe_roam_fill_obss_scan_param(struct pe_session *src_session,
						struct pe_session *dst_session)
{
}
#endif

#ifdef WLAN_FEATURE_SR
void lim_handle_sr_cap(struct wlan_objmgr_vdev *vdev,
		       enum sr_osif_reason_code reason)
{
	int32_t non_srg_pd_threshold = 0;
	int32_t srg_pd_threshold = 0;
	uint8_t non_srg_pd_offset = 0;
	uint8_t srg_max_pd_offset = 0;
	uint8_t srg_min_pd_offset = 0;
	uint8_t sr_ctrl, sr_enable_modes;
	bool is_pd_threshold_present = false;
	struct wlan_objmgr_pdev *pdev;
	enum sr_status_of_roamed_ap sr_status;
	enum sr_osif_operation sr_op;
	enum QDF_OPMODE opmode;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		pe_err("mac ctx is null");
		return;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		pe_err("invalid pdev");
		return;
	}

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	/* If SR is disabled in INI for the session-operating mode
	 * Then return.
	 */
	wlan_mlme_get_sr_enable_modes(mac->psoc, &sr_enable_modes);
	if (!(sr_enable_modes & (1 << opmode))) {
		pe_debug("SR is disabled in INI for mode: %d", opmode);
		return;
	}
	if (!wlan_vdev_mlme_get_he_spr_enabled(vdev)) {
		pe_debug("SR is not enabled");
		return;
	}
	non_srg_pd_offset = wlan_vdev_mlme_get_non_srg_pd_offset(vdev);
	wlan_vdev_mlme_get_srg_pd_offset(vdev, &srg_max_pd_offset,
					 &srg_min_pd_offset);
	wlan_vdev_mlme_get_current_non_srg_pd_threshold(vdev,
							&non_srg_pd_threshold);
	wlan_vdev_mlme_get_current_srg_pd_threshold(vdev,
						    &srg_pd_threshold);

	sr_ctrl = wlan_vdev_mlme_get_sr_ctrl(vdev);
	if ((sr_ctrl & NON_SRG_PD_SR_DISALLOWED) &&
	    (!(sr_ctrl & SRG_INFO_PRESENT))) {
		sr_status = SR_DISALLOW;
	} else {
		if ((!(sr_ctrl & NON_SRG_PD_SR_DISALLOWED) &&
		     (non_srg_pd_threshold > non_srg_pd_offset +
		      SR_PD_THRESHOLD_MIN)) ||
		     ((sr_ctrl & SRG_INFO_PRESENT) &&
		      ((srg_pd_threshold > srg_max_pd_offset +
		     SR_PD_THRESHOLD_MIN) ||
		      (srg_pd_threshold < srg_min_pd_offset +
		       SR_PD_THRESHOLD_MIN))))
			sr_status = SR_THRESHOLD_NOT_IN_RANGE;
		else
			sr_status = SR_THRESHOLD_IN_RANGE;
	}
	pe_debug("sr status %d reason %d existing thresholds srg: %d non-srg: %d New: sr offset srg: max %d min %d non-srg: %d",
		 sr_status, reason, srg_pd_threshold, non_srg_pd_threshold,
		 srg_max_pd_offset, srg_min_pd_offset, non_srg_pd_offset);
	switch (sr_status) {
	case SR_DISALLOW:
		/** clear thresholds set by previous AP **/
		wlan_vdev_mlme_set_current_non_srg_pd_threshold(vdev, 0);
		wlan_vdev_mlme_set_current_srg_pd_threshold(vdev, 0);
		wlan_spatial_reuse_osif_event(vdev,
					      SR_OPERATION_SUSPEND,
					      reason);
		/*
		 * If SR is disabled due to beacon update,
		 * notify the firmware to disable SR
		 */
		if (reason == SR_REASON_CODE_BCN_IE_CHANGE)
			wlan_sr_setup_req(vdev, pdev, false, 0, 0);
	break;
	case SR_THRESHOLD_NOT_IN_RANGE:
		wlan_vdev_mlme_get_pd_threshold_present(
						vdev, &is_pd_threshold_present);
		/*
		 * if userspace gives pd threshold then check if its within
		 * range of roamed AP's min and max thresholds, if not in
		 * range disable and let userspace decide to re-enable.
		 * if userspace dosesnt give PD threshold then always enable
		 * SRG based on AP's recommendation of thresholds.
		 */
		if (is_pd_threshold_present) {
			wlan_vdev_mlme_set_current_non_srg_pd_threshold(vdev,
									0);
			wlan_vdev_mlme_set_current_srg_pd_threshold(vdev, 0);
			wlan_spatial_reuse_osif_event(vdev,
						      SR_OPERATION_SUSPEND,
						      reason);
		} else {
			sr_op = (reason == SR_REASON_CODE_ROAMING) ?
				SR_OPERATION_RESUME :
				SR_OPERATION_UPDATE_PARAMS;
			wlan_sr_setup_req(
				vdev, pdev, true,
				srg_max_pd_offset + SR_PD_THRESHOLD_MIN,
				non_srg_pd_threshold + SR_PD_THRESHOLD_MIN);
			wlan_spatial_reuse_osif_event(vdev, sr_op, reason);
		}
	break;
	case SR_THRESHOLD_IN_RANGE:
		/* Send enable command to fw, as fw disables SR on roaming */
		wlan_sr_setup_req(vdev, pdev, true, srg_pd_threshold,
				  non_srg_pd_threshold);
	break;
	}
}
#endif

QDF_STATUS
pe_roam_synch_callback(struct mac_context *mac_ctx,
		       uint8_t vdev_id,
		       struct roam_offload_synch_ind *roam_sync_ind_ptr,
		       uint16_t ie_len,
		       enum sir_roam_op_code reason)
{
	struct pe_session *session_ptr;
	struct pe_session *ft_session_ptr;
	uint8_t session_id;
	uint8_t *reassoc_resp;
	tpDphHashNode curr_sta_ds = NULL, sta_ds = NULL;
	uint16_t aid;
	struct bss_params *add_bss_params;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct bss_description *bss_desc = NULL;
	uint16_t ric_tspec_len;
	struct qdf_mac_addr bssid;
	uint8_t *oui_ie_ptr;
	uint16_t oui_ie_len;

	if (!roam_sync_ind_ptr) {
		pe_err("LFR3:roam_sync_ind_ptr is NULL");
		return status;
	}
	session_ptr = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session_ptr) {
		pe_err("LFR3:Unable to find session");
		return status;
	}

	if (!LIM_IS_STA_ROLE(session_ptr)) {
		pe_err("LFR3:session is not in STA mode");
		return status;
	}

	if (is_multi_link_roam(roam_sync_ind_ptr))
		mlo_get_sta_link_mac_addr(vdev_id, roam_sync_ind_ptr, &bssid);
	else
		bssid = roam_sync_ind_ptr->bssid;

	pe_debug("LFR3: PE callback reason: %d", reason);
	switch (reason) {
	case SIR_ROAMING_ABORT:
		/*
		 * If there was a disassoc or deauth that was received
		 * during roaming and it was not honored, then we have
		 * to internally initiate a disconnect because with
		 * ROAM_ABORT we come back to original AP.
		 */
		if (session_ptr->recvd_deauth_while_roaming)
			lim_perform_deauth(mac_ctx, session_ptr,
					   session_ptr->deauth_disassoc_rc,
					   session_ptr->bssId, 0);
		if (session_ptr->recvd_disassoc_while_roaming) {
			lim_disassoc_tdls_peers(mac_ctx, session_ptr,
						session_ptr->bssId);
			lim_perform_disassoc(mac_ctx, 0,
					     session_ptr->deauth_disassoc_rc,
					     session_ptr, session_ptr->bssId);
		}
		return QDF_STATUS_SUCCESS;
	case SIR_ROAM_SYNCH_PROPAGATION:
		break;
	default:
		return status;
	}

	pe_debug("LFR3:Received ROAM SYNCH IND bssid "QDF_MAC_ADDR_FMT" auth: %d vdevId: %d",
		 QDF_MAC_ADDR_REF(roam_sync_ind_ptr->bssid.bytes),
		 roam_sync_ind_ptr->auth_status,
		 vdev_id);

	/*
	 * If deauth from AP already in progress, ignore Roam Synch Indication
	 * from firmware.
	 */
	if (session_ptr->limSmeState != eLIM_SME_LINK_EST_STATE) {
		pe_err("LFR3: Not in Link est state");
		return status;
	}

	bss_desc = qdf_mem_malloc(sizeof(struct bss_description) + ie_len);
	if (!bss_desc) {
		QDF_ASSERT(bss_desc);
		status = -QDF_STATUS_E_NOMEM;
		return status;
	}

	status = lim_roam_fill_bss_descr(mac_ctx, roam_sync_ind_ptr,
					 bss_desc, session_ptr);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("LFR3:Failed to fill Bss Descr");
		qdf_mem_free(bss_desc);
		return status;
	}
	status = QDF_STATUS_E_FAILURE;
	ft_session_ptr = pe_create_session(mac_ctx, bss_desc->bssId,
					   &session_id,
					   mac_ctx->lim.max_sta_of_pe_session,
					   session_ptr->bssType,
					   session_ptr->vdev_id);
	if (!ft_session_ptr) {
		pe_err("LFR3:Cannot create PE Session bssid: "QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(bss_desc->bssId));
		qdf_mem_free(bss_desc);
		return status;
	}
	/* Update the beacon/probe filter in mac_ctx */
	lim_set_bcn_probe_filter(mac_ctx, ft_session_ptr, 0);
	sir_copy_mac_addr(ft_session_ptr->limReAssocbssId, bss_desc->bssId);
	session_ptr->bRoamSynchInProgress = true;
	ft_session_ptr->bRoamSynchInProgress = true;
	ft_session_ptr->limSystemRole = eLIM_STA_ROLE;
	sir_copy_mac_addr(session_ptr->limReAssocbssId, bss_desc->bssId);
	ft_session_ptr->csaOffloadEnable = session_ptr->csaOffloadEnable;

	/* Next routine will update nss and vdev_nss with AP's capabilities */
	status = lim_fill_ft_session(mac_ctx, bss_desc, ft_session_ptr,
				     session_ptr, roam_sync_ind_ptr->phy_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to fill ft session for vdev id %d",
		       ft_session_ptr->vdev_id);
		qdf_mem_free(bss_desc);
		goto roam_sync_fail;
	}

	roam_sync_ind_ptr->ssid.length =
		qdf_min((qdf_size_t)ft_session_ptr->ssId.length,
			sizeof(roam_sync_ind_ptr->ssid.ssid));
	qdf_mem_copy(roam_sync_ind_ptr->ssid.ssid, ft_session_ptr->ssId.ssId,
		     roam_sync_ind_ptr->ssid.length);
	pe_update_crypto_params(mac_ctx, ft_session_ptr, roam_sync_ind_ptr);

	/* Reset the SPMK global cache */
	wlan_mlme_set_sae_single_pmk_bss_cap(mac_ctx->psoc, vdev_id, false);

	/* Next routine may update nss based on dot11Mode */

	lim_ft_prepare_add_bss_req(mac_ctx, ft_session_ptr, bss_desc);
	lim_set_tpc_power(mac_ctx, ft_session_ptr, bss_desc);

	if (session_ptr->is11Rconnection)
		lim_fill_fils_ft(session_ptr, ft_session_ptr);

	roam_sync_ind_ptr->add_bss_params =
		(struct bss_params *) ft_session_ptr->ftPEContext.pAddBssReq;
	add_bss_params = ft_session_ptr->ftPEContext.pAddBssReq;
	lim_delete_tdls_peers(mac_ctx, session_ptr);
	/*
	 * After deleting the TDLS peers notify the Firmware about TDLS STA
	 * disconnection due to roaming
	 */
	wlan_tdls_notify_sta_disconnect(vdev_id, true,
					false, session_ptr->vdev);

	sta_ds = dph_lookup_hash_entry(mac_ctx, session_ptr->bssId, &aid,
				       &session_ptr->dph.dphHashTable);
	if (!sta_ds && !is_multi_link_roam(roam_sync_ind_ptr)) {
		pe_err("LFR3:failed to lookup hash entry");
		ft_session_ptr->bRoamSynchInProgress = false;
		qdf_mem_free(bss_desc);
		goto roam_sync_fail;
	}

	/* update OBSS scan param */
	pe_roam_fill_obss_scan_param(session_ptr, ft_session_ptr);

	curr_sta_ds = dph_add_hash_entry(mac_ctx,
					 bssid.bytes,
					 DPH_STA_HASH_INDEX_PEER,
					 &ft_session_ptr->dph.dphHashTable);
	if (!curr_sta_ds) {
		pe_err("LFR3:failed to add hash entry for "QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(add_bss_params->staContext.staMac));
		ft_session_ptr->bRoamSynchInProgress = false;
		qdf_mem_free(bss_desc);
		goto roam_sync_fail;
	}

	if (roam_sync_ind_ptr->auth_status == ROAM_AUTH_STATUS_AUTHENTICATED)
		curr_sta_ds->is_key_installed = true;

	lim_mlo_roam_copy_partner_info_to_session(ft_session_ptr,
						  roam_sync_ind_ptr);

	reassoc_resp = (uint8_t *)roam_sync_ind_ptr +
			roam_sync_ind_ptr->reassoc_resp_offset;

	if (wlan_vdev_mlme_get_is_mlo_link(mac_ctx->psoc, vdev_id)) {
		status = lim_gen_link_specific_assoc_rsp(mac_ctx,
						ft_session_ptr,
						reassoc_resp,
						roam_sync_ind_ptr->reassoc_resp_length);
		if (ft_session_ptr->is_unexpected_peer_error)
			status = QDF_STATUS_E_FAILURE;

		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mem_free(bss_desc);
			goto roam_sync_fail;
		}
	} else {
		lim_process_assoc_rsp_frame(mac_ctx, reassoc_resp,
					    roam_sync_ind_ptr->reassoc_resp_length - SIR_MAC_HDR_LEN_3A,
					    LIM_REASSOC, ft_session_ptr);
		if (ft_session_ptr->is_unexpected_peer_error) {
			status = QDF_STATUS_E_FAILURE;
			qdf_mem_free(bss_desc);
			goto roam_sync_fail;
		}
	}

	oui_ie_ptr = (uint8_t *)&bss_desc->ieFields[0];
	oui_ie_len = wlan_get_ielen_from_bss_description(bss_desc);
	lim_enable_cts_to_self_for_exempted_iot_ap(mac_ctx,
						   ft_session_ptr,
						   oui_ie_ptr, oui_ie_len);
	qdf_mem_free(bss_desc);
	oui_ie_len = 0;
	oui_ie_ptr = NULL;

	lim_check_ft_initial_im_association(roam_sync_ind_ptr, ft_session_ptr);

	lim_copy_and_free_hlp_data_from_session(ft_session_ptr,
						roam_sync_ind_ptr);
	roam_sync_ind_ptr->aid = ft_session_ptr->limAID;
	curr_sta_ds->mlmStaContext.mlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
	curr_sta_ds->nss = ft_session_ptr->nss;
	roam_sync_ind_ptr->nss = ft_session_ptr->nss;
	ft_session_ptr->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
	ft_session_ptr->limPrevMlmState = ft_session_ptr->limMlmState;
	lim_init_tdls_data(mac_ctx, ft_session_ptr);
	ric_tspec_len = ft_session_ptr->RICDataLen;
	pe_debug("LFR3: Session RicLength: %d", ft_session_ptr->RICDataLen);
	lim_handle_sr_cap(ft_session_ptr->vdev, SR_REASON_CODE_ROAMING);
#ifdef FEATURE_WLAN_ESE
	ric_tspec_len += ft_session_ptr->tspecLen;
	pe_debug("LFR3: tspecLen: %d", ft_session_ptr->tspecLen);
#endif
	if (ric_tspec_len) {
		roam_sync_ind_ptr->ric_tspec_data =
				qdf_mem_malloc(ric_tspec_len);
		if (!roam_sync_ind_ptr->ric_tspec_data) {
			ft_session_ptr->bRoamSynchInProgress = false;
			status = QDF_STATUS_E_NOMEM;
			goto roam_sync_fail;
		}

		if (ft_session_ptr->ricData) {
			roam_sync_ind_ptr->ric_data_len =
					ft_session_ptr->RICDataLen;
			qdf_mem_copy(roam_sync_ind_ptr->ric_tspec_data,
				     ft_session_ptr->ricData,
				     roam_sync_ind_ptr->ric_data_len);
			qdf_mem_free(ft_session_ptr->ricData);
			ft_session_ptr->ricData = NULL;
			ft_session_ptr->RICDataLen = 0;
		}
#ifdef FEATURE_WLAN_ESE
		if (ft_session_ptr->tspecIes) {
			roam_sync_ind_ptr->tspec_len = ft_session_ptr->tspecLen;
			qdf_mem_copy(roam_sync_ind_ptr->ric_tspec_data +
				     roam_sync_ind_ptr->ric_data_len,
				     ft_session_ptr->tspecIes,
				     roam_sync_ind_ptr->tspec_len);
			qdf_mem_free(ft_session_ptr->tspecIes);
			ft_session_ptr->tspecIes = NULL;
			ft_session_ptr->tspecLen = 0;
		}
#endif
	}
	roam_sync_ind_ptr->chan_width = ft_session_ptr->ch_width;
	roam_sync_ind_ptr->max_rate_flags =
			lim_get_max_rate_flags(mac_ctx, curr_sta_ds);
	ft_session_ptr->limSmeState = eLIM_SME_LINK_EST_STATE;
	ft_session_ptr->limPrevSmeState = ft_session_ptr->limSmeState;
	ft_session_ptr->bRoamSynchInProgress = false;

	/* Cleanup the old session */
	session_ptr->limSmeState = eLIM_SME_IDLE_STATE;

	/*
	 * Delete the ml_peer only if DUT is roamed to a non-11BE candidate.
	 * ml_peer is already cleaned up in wma_delete_all_peers() at the
	 * beginning of roam_sync handling for 11BE candidates.
	 */
	if (sta_ds) {
		if (!wlan_vdev_mlme_is_mlo_vdev(session_ptr->vdev)) {
			lim_mlo_notify_peer_disconn(session_ptr, sta_ds);
			lim_mlo_roam_delete_link_peer(session_ptr, sta_ds);
		}
		lim_cleanup_rx_path(mac_ctx, sta_ds, session_ptr, false);
		lim_delete_dph_hash_entry(mac_ctx, sta_ds->staAddr, aid,
					  session_ptr);
	}

	pe_delete_session(mac_ctx, session_ptr);

	return QDF_STATUS_SUCCESS;

roam_sync_fail:
	pe_err("Roam sync failure status %d session vdev %d", status,
	       session_ptr->vdev_id);
	/*
	 * Cleanup the new session upon roam sync failure.
	 * Retain the old session for graceful HO failure handling.
	 */
	if (curr_sta_ds) {
		lim_cleanup_rx_path(mac_ctx, curr_sta_ds, ft_session_ptr,
				    false);
		lim_delete_dph_hash_entry(mac_ctx, curr_sta_ds->staAddr,
					  curr_sta_ds->assocId, ft_session_ptr);
	}
	pe_delete_session(mac_ctx, ft_session_ptr);
	return status;
}

QDF_STATUS
pe_set_ie_for_roam_invoke(struct mac_context *mac_ctx, uint8_t vdev_id,
			  uint16_t dot11_mode, enum QDF_OPMODE opmode)
{
	QDF_STATUS status;

	if (!mac_ctx)
		return QDF_STATUS_E_FAILURE;

	status = lim_send_ies_per_band(mac_ctx, vdev_id, dot11_mode, opmode);
	return status;
}

#endif

static bool lim_is_beacon_miss_scenario(struct mac_context *mac,
					uint8_t *pRxPacketInfo)
{
	tpSirMacMgmtHdr pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	uint8_t sessionId;
	struct pe_session *pe_session =
		pe_find_session_by_bssid(mac, pHdr->bssId, &sessionId);

	if (pe_session && pe_session->pmmOffloadInfo.bcnmiss)
		return true;
	return false;
}

/** -----------------------------------------------------------------
   \brief lim_is_pkt_candidate_for_drop() - decides whether to drop the frame or not

   This function is called before enqueuing the frame to PE queue for further processing.
   This prevents unnecessary frames getting into PE Queue and drops them right away.
   Frames will be dropped in the following scenarios:

   - In Scan State, drop the frames which are not marked as scan frames
   - In non-Scan state, drop the frames which are marked as scan frames.

   \param mac - global mac structure
   \return - none
   \sa
   ----------------------------------------------------------------- */

tMgmtFrmDropReason lim_is_pkt_candidate_for_drop(struct mac_context *mac,
						 uint8_t *pRxPacketInfo,
						 uint32_t subType)
{
	uint32_t framelen;
	uint8_t *pBody;
	tSirMacCapabilityInfo capabilityInfo;
	tpSirMacMgmtHdr pHdr = NULL;
	struct wlan_objmgr_vdev *vdev;

	/*
	 *
	 * In scan mode, drop only Beacon/Probe Response which are NOT marked as scan-frames.
	 * In non-scan mode, drop only Beacon/Probe Response which are marked as scan frames.
	 * Allow other mgmt frames, they must be from our own AP, as we don't allow
	 * other than beacons or probe responses in scan state.
	 */
	if ((subType == SIR_MAC_MGMT_BEACON) ||
	    (subType == SIR_MAC_MGMT_PROBE_RSP)) {
		if (lim_is_beacon_miss_scenario(mac, pRxPacketInfo)) {
			MTRACE(mac_trace(mac, TRACE_CODE_INFO_LOG, 0,
					 eLOG_NODROP_MISSED_BEACON_SCENARIO));
			return eMGMT_DROP_NO_DROP;
		}

		framelen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
		pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
		/* drop the frame if length is less than 12 */
		if (framelen < LIM_MIN_BCN_PR_LENGTH)
			return eMGMT_DROP_INVALID_SIZE;

		*((uint16_t *) &capabilityInfo) =
			sir_read_u16(pBody + LIM_BCN_PR_CAPABILITY_OFFSET);

		/* Note sure if this is sufficient, basically this condition allows all probe responses and
		 *   beacons from an infrastructure network
		 */
		if (!capabilityInfo.ibss)
			return eMGMT_DROP_NO_DROP;

		/* Drop INFRA Beacons and Probe Responses in IBSS Mode */
		/* This can be enhanced to even check the SSID before deciding to enqueue the frame. */
		if (capabilityInfo.ess)
			return eMGMT_DROP_INFRA_BCN_IN_IBSS;

	} else if (subType == SIR_MAC_MGMT_AUTH) {
		uint16_t curr_seq_num = 0;
		struct tLimPreAuthNode *auth_node;

		pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
		vdev = wlan_objmgr_get_vdev_by_macaddr_from_pdev(mac->pdev,
								 pHdr->da,
								 WLAN_LEGACY_MAC_ID);
		if (!vdev)
			return eMGMT_DROP_NO_DROP;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

		curr_seq_num = ((pHdr->seqControl.seqNumHi << 4) |
				(pHdr->seqControl.seqNumLo));
		auth_node = lim_search_pre_auth_list(mac, pHdr->sa);
		if (auth_node && pHdr->fc.retry &&
		    (auth_node->seq_num == curr_seq_num)) {
			pe_err_rl("auth frame, seq num: %d is already processed, drop it",
				  curr_seq_num);
			return eMGMT_DROP_DUPLICATE_AUTH_FRAME;
		}
	} else if ((subType == SIR_MAC_MGMT_ASSOC_REQ) ||
		   (subType == SIR_MAC_MGMT_DISASSOC) ||
		   (subType == SIR_MAC_MGMT_DEAUTH)) {
		struct peer_mlme_priv_obj *peer_priv;
		struct wlan_objmgr_peer *peer;
		qdf_time_t *timestamp;

		pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
		vdev = wlan_objmgr_get_vdev_by_macaddr_from_pdev(mac->pdev,
								 pHdr->da,
								 WLAN_LEGACY_MAC_ID);
		if (!vdev)
			return eMGMT_DROP_SPURIOUS_FRAME;

		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE &&
		    wlan_cm_is_vdev_roam_started(vdev) &&
		    (subType == SIR_MAC_MGMT_DISASSOC ||
		     subType == SIR_MAC_MGMT_DEAUTH)) {
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
			return eMGMT_DROP_DEAUTH_DURING_ROAM_STARTED;
		}
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

		peer = wlan_objmgr_get_peer_by_mac(mac->psoc,
						   pHdr->sa,
						   WLAN_LEGACY_MAC_ID);
		if (!peer) {
			if (subType == SIR_MAC_MGMT_ASSOC_REQ)
				return eMGMT_DROP_NO_DROP;
			return eMGMT_DROP_SPURIOUS_FRAME;
		}

		peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							WLAN_UMAC_COMP_MLME);
		if (!peer_priv) {
			wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
			if (subType == SIR_MAC_MGMT_ASSOC_REQ)
				return eMGMT_DROP_NO_DROP;
			return eMGMT_DROP_SPURIOUS_FRAME;
		}

		if (subType == SIR_MAC_MGMT_ASSOC_REQ)
			timestamp =
			   &peer_priv->last_assoc_received_time;
		else
			timestamp =
			   &peer_priv->last_disassoc_deauth_received_time;

		if (*timestamp > 0 &&
		    qdf_system_time_before(qdf_get_system_timestamp(),
					   *timestamp +
					   LIM_DOS_PROTECTION_TIME)) {
			pe_debug_rl(FL("Dropping subtype 0x%x frame. %s %d ms %s %d ms"),
				    subType, "It is received after",
				    (int)(qdf_get_system_timestamp() - *timestamp),
				    "of last frame. Allow it only after",
				    LIM_DOS_PROTECTION_TIME);
			wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
			return eMGMT_DROP_EXCESSIVE_MGMT_FRAME;
		}

		*timestamp = qdf_get_system_timestamp();
		wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);

	}

	return eMGMT_DROP_NO_DROP;
}

void lim_update_lost_link_info(struct mac_context *mac, struct pe_session *session,
				int32_t rssi)
{
	struct sir_lost_link_info *lost_link_info;
	struct scheduler_msg mmh_msg = {0};

	if ((!mac) || (!session)) {
		pe_err("parameter NULL");
		return;
	}
	if (!LIM_IS_STA_ROLE(session))
		return;

	lost_link_info = qdf_mem_malloc(sizeof(*lost_link_info));
	if (!lost_link_info)
		return;

	lost_link_info->vdev_id = session->smeSessionId;
	lost_link_info->rssi = rssi;
	mmh_msg.type = eWNI_SME_LOST_LINK_INFO_IND;
	mmh_msg.bodyptr = lost_link_info;
	mmh_msg.bodyval = 0;
	pe_debug("post eWNI_SME_LOST_LINK_INFO_IND, bss_idx: %d rssi: %d",
		lost_link_info->vdev_id, lost_link_info->rssi);

	lim_sys_process_mmh_msg_api(mac, &mmh_msg);
}

/**
 * lim_mon_init_session() - create PE session for monitor mode operation
 * @mac_ptr: mac pointer
 * @msg: Pointer to struct sir_create_session type.
 *
 * Return: NONE
 */
void lim_mon_init_session(struct mac_context *mac_ptr,
			  struct sir_create_session *msg)
{
	struct pe_session *psession_entry;
	uint8_t session_id;

	psession_entry = pe_create_session(mac_ptr, msg->bss_id.bytes,
					   &session_id,
					   mac_ptr->lim.max_sta_of_pe_session,
					   eSIR_MONITOR_MODE,
					   msg->vdev_id);
	if (!psession_entry) {
		pe_err("Monitor mode: Session Can not be created bssid: "
			QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(msg->bss_id.bytes));
		return;
	}
	psession_entry->vhtCapability = 1;
}

void lim_mon_deinit_session(struct mac_context *mac_ptr,
			    struct sir_delete_session *msg)
{
	struct pe_session *session;

	session = pe_find_session_by_vdev_id(mac_ptr, msg->vdev_id);

	if (session && session->bssType == eSIR_MONITOR_MODE)
		pe_delete_session(mac_ptr, session);
}

/**
 * lim_update_ext_cap_ie() - Update Extended capabilities IE(if present)
 *          with capabilities of Fine Time measurements(FTM) if set in driver
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @ie_data: Default Scan IE data
 * @local_ie_buf: Local Scan IE data
 * @local_ie_len: Pointer to length of @ie_data
 * @session: Pointer to pe session
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_update_ext_cap_ie(struct mac_context *mac_ctx, uint8_t *ie_data,
				 uint8_t *local_ie_buf, uint16_t *local_ie_len,
				 struct pe_session *session)
{
	uint32_t dot11mode;
	bool vht_enabled = false;
	tDot11fIEExtCap default_scan_ext_cap = {0}, driver_ext_cap = {0};
	QDF_STATUS status;

	status = lim_strip_extcap_update_struct(mac_ctx, ie_data,
				   local_ie_len, &default_scan_ext_cap);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Strip ext cap fails %d", status);
		return QDF_STATUS_E_FAILURE;
	}

	if ((*local_ie_len) > (MAX_DEFAULT_SCAN_IE_LEN - EXT_CAP_IE_HDR_LEN)) {
		pe_err("Invalid Scan IE length");
		return QDF_STATUS_E_FAILURE;
	}
	/* copy ie prior to ext cap to local buffer */
	qdf_mem_copy(local_ie_buf, ie_data, (*local_ie_len));

	/* from here ext cap ie starts, set EID */
	local_ie_buf[*local_ie_len] = DOT11F_EID_EXTCAP;

	dot11mode = mac_ctx->mlme_cfg->dot11_mode.dot11_mode;
	if (IS_DOT11_MODE_VHT(dot11mode))
		vht_enabled = true;

	status = populate_dot11f_ext_cap(mac_ctx, vht_enabled,
					&driver_ext_cap, NULL);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Failed %d to create ext cap IE. Use default value instead",
				status);
		local_ie_buf[*local_ie_len + 1] = DOT11F_IE_EXTCAP_MAX_LEN;

		if ((*local_ie_len) > (MAX_DEFAULT_SCAN_IE_LEN -
		    (DOT11F_IE_EXTCAP_MAX_LEN + EXT_CAP_IE_HDR_LEN))) {
			pe_err("Invalid Scan IE length");
			return QDF_STATUS_E_FAILURE;
		}
		(*local_ie_len) += EXT_CAP_IE_HDR_LEN;
		qdf_mem_copy(local_ie_buf + (*local_ie_len),
				default_scan_ext_cap.bytes,
				DOT11F_IE_EXTCAP_MAX_LEN);
		(*local_ie_len) += DOT11F_IE_EXTCAP_MAX_LEN;
		return QDF_STATUS_SUCCESS;
	}
	lim_merge_extcap_struct(&driver_ext_cap, &default_scan_ext_cap, true);

	if (session)
		populate_dot11f_twt_extended_caps(mac_ctx, session,
						  &driver_ext_cap);
	else
		pe_debug("Session NULL, cannot set TWT caps");

	local_ie_buf[*local_ie_len + 1] = driver_ext_cap.num_bytes;

	if ((*local_ie_len) > (MAX_DEFAULT_SCAN_IE_LEN -
	    (EXT_CAP_IE_HDR_LEN + driver_ext_cap.num_bytes))) {
		pe_err("Invalid Scan IE length");
		return QDF_STATUS_E_FAILURE;
	}
	(*local_ie_len) += EXT_CAP_IE_HDR_LEN;
	qdf_mem_copy(local_ie_buf + (*local_ie_len),
			driver_ext_cap.bytes, driver_ext_cap.num_bytes);
	(*local_ie_len) += driver_ext_cap.num_bytes;
	return QDF_STATUS_SUCCESS;
}

#define LIM_RSN_OUI_SIZE 4

struct rsn_oui_akm_type_map {
	enum ani_akm_type akm_type;
	uint8_t rsn_oui[LIM_RSN_OUI_SIZE];
};

static const struct rsn_oui_akm_type_map rsn_oui_akm_type_mapping_table[] = {
	{ANI_AKM_TYPE_RSN,                  {0x00, 0x0F, 0xAC, 0x01} },
	{ANI_AKM_TYPE_RSN_PSK,              {0x00, 0x0F, 0xAC, 0x02} },
	{ANI_AKM_TYPE_FT_RSN,               {0x00, 0x0F, 0xAC, 0x03} },
	{ANI_AKM_TYPE_FT_RSN_PSK,           {0x00, 0x0F, 0xAC, 0x04} },
	{ANI_AKM_TYPE_RSN_8021X_SHA256,     {0x00, 0x0F, 0xAC, 0x05} },
	{ANI_AKM_TYPE_RSN_PSK_SHA256,       {0x00, 0x0F, 0xAC, 0x06} },
#ifdef WLAN_FEATURE_SAE
	{ANI_AKM_TYPE_SAE,                  {0x00, 0x0F, 0xAC, 0x08} },
	{ANI_AKM_TYPE_FT_SAE,               {0x00, 0x0F, 0xAC, 0x09} },
#endif
	{ANI_AKM_TYPE_SUITEB_EAP_SHA256,    {0x00, 0x0F, 0xAC, 0x0B} },
	{ANI_AKM_TYPE_SUITEB_EAP_SHA384,    {0x00, 0x0F, 0xAC, 0x0C} },
	{ANI_AKM_TYPE_FT_SUITEB_EAP_SHA384, {0x00, 0x0F, 0xAC, 0x0D} },
	{ANI_AKM_TYPE_FILS_SHA256,          {0x00, 0x0F, 0xAC, 0x0E} },
	{ANI_AKM_TYPE_FILS_SHA384,          {0x00, 0x0F, 0xAC, 0x0F} },
	{ANI_AKM_TYPE_FT_FILS_SHA256,       {0x00, 0x0F, 0xAC, 0x10} },
	{ANI_AKM_TYPE_FT_FILS_SHA384,       {0x00, 0x0F, 0xAC, 0x11} },
	{ANI_AKM_TYPE_OWE,                  {0x00, 0x0F, 0xAC, 0x12} },
#ifdef FEATURE_WLAN_ESE
	{ANI_AKM_TYPE_CCKM,                 {0x00, 0x40, 0x96, 0x00} },
#endif
	{ANI_AKM_TYPE_OSEN,                 {0x50, 0x6F, 0x9A, 0x01} },
	{ANI_AKM_TYPE_DPP_RSN,              {0x50, 0x6F, 0x9A, 0x02} },
	{ANI_AKM_TYPE_WPA,                  {0x00, 0x50, 0xF2, 0x01} },
	{ANI_AKM_TYPE_WPA_PSK,              {0x00, 0x50, 0xF2, 0x02} },
	{ANI_AKM_TYPE_SAE_EXT_KEY,          {0x00, 0x0F, 0xAC, 0x18} },
	{ANI_AKM_TYPE_FT_SAE_EXT_KEY,       {0x00, 0x0F, 0xAC, 0x19} },
	/* Add akm type above here */
	{ANI_AKM_TYPE_UNKNOWN, {0} },
};

enum ani_akm_type lim_translate_rsn_oui_to_akm_type(uint8_t auth_suite[4])
{
	const struct rsn_oui_akm_type_map *map;
	enum ani_akm_type akm_type;

	map = rsn_oui_akm_type_mapping_table;
	while (true) {
		akm_type = map->akm_type;
		if ((akm_type == ANI_AKM_TYPE_UNKNOWN) ||
		    (qdf_mem_cmp(auth_suite, map->rsn_oui, 4) == 0))
			break;
		map++;
	}

	pe_debug("akm_type: %d", akm_type);

	return akm_type;
}

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_11BE_MLO)
QDF_STATUS
lim_cm_fill_link_session(struct mac_context *mac_ctx,
			 uint8_t vdev_id,
			 struct pe_session *pe_session,
			 struct roam_offload_synch_ind *sync_ind,
			 uint16_t ie_len)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_vdev *assoc_vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct bss_description *bss_desc = NULL;
	uint32_t bss_len;
	struct join_req *pe_join_req;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		pe_err("Vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	bss_len = (uint16_t)(offsetof(struct bss_description,
			   ieFields[0]) + ie_len);

	assoc_vdev = wlan_mlo_get_assoc_link_vdev(vdev);

	if (!assoc_vdev) {
		pe_err("Assoc vdev is NULL");
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	status = wlan_vdev_mlme_get_ssid(assoc_vdev,
					 pe_session->ssId.ssId,
					 &pe_session->ssId.length);

	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to get ssid vdev id %d",
		       vdev_id);
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	pe_session->lim_join_req =
		qdf_mem_malloc(sizeof(*pe_session->lim_join_req) + bss_len);
	if (!pe_session->lim_join_req) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}

	pe_join_req = pe_session->lim_join_req;

	mlo_roam_copy_partner_info(&pe_join_req->partner_info,
				   sync_ind, vdev_id, false);

	bss_desc = &pe_session->lim_join_req->bssDescription;

	status = lim_roam_fill_bss_descr(mac_ctx, sync_ind, bss_desc,
					 pe_session);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("LFR3:Failed to fill Bss Descr");
		goto end;
	}

	status = lim_fill_pe_session(mac_ctx, pe_session, bss_desc);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to fill pe session vdev id %d",
		       pe_session->vdev_id);
		goto end;
	}

	if (pe_session->limSmeState == eLIM_SME_WT_JOIN_STATE) {
		pe_session->limSmeState = eLIM_SME_LINK_EST_STATE;
		pe_session->limMlmState = eLIM_MLM_WT_REASSOC_RSP_STATE;
	}
end:
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(pe_session->lim_join_req);
		pe_session->lim_join_req = NULL;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	return status;
}

struct pe_session *
lim_cm_roam_create_session(struct mac_context *mac_ctx,
			   uint8_t vdev_id,
			   struct roam_offload_synch_ind *sync_ind)
{
	struct pe_session *pe_session = NULL;
	struct qdf_mac_addr link_mac_addr;
	bool is_link_vdev = false;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t session_id;

	is_link_vdev = wlan_vdev_mlme_get_is_mlo_link(mac_ctx->psoc, vdev_id);
	status = mlo_get_sta_link_mac_addr(vdev_id, sync_ind,
					   &link_mac_addr);

	if (QDF_IS_STATUS_ERROR(status))
		return NULL;

	/* In case of legacy to mlo roaming, create pe session */
	if (!pe_session && is_link_vdev) {
		pe_session = pe_create_session(mac_ctx, &link_mac_addr.bytes[0],
					       &session_id,
					       mac_ctx->lim.max_sta_of_pe_session,
					       eSIR_INFRASTRUCTURE_MODE,
					       vdev_id);
		if (!pe_session) {
			pe_err("vdev_id %d : pe session create failed BSSID"
			       QDF_MAC_ADDR_FMT, vdev_id,
			       QDF_MAC_ADDR_REF(link_mac_addr.bytes));
			return NULL;
		}
	}

	return pe_session;
}

QDF_STATUS
lim_create_and_fill_link_session(struct mac_context *mac_ctx,
				 uint8_t vdev_id,
				 struct roam_offload_synch_ind *sync_ind,
				 uint16_t ie_len)
{
	struct pe_session *pe_session;
	QDF_STATUS status;

	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	pe_session = lim_cm_roam_create_session(mac_ctx, vdev_id, sync_ind);
	if (!pe_session)
		goto fail;

	status = lim_cm_fill_link_session(mac_ctx, vdev_id,
					  pe_session, sync_ind, ie_len);
	if (QDF_IS_STATUS_ERROR(status))
		goto fail;

	return QDF_STATUS_SUCCESS;

fail:
	if (pe_session)
		pe_delete_session(mac_ctx, pe_session);

	pe_err("MLO ROAM: Link session creation failed");
	return QDF_STATUS_E_FAILURE;
}

void lim_roam_mlo_create_peer(struct mac_context *mac,
			      struct roam_offload_synch_ind *sync_ind,
			      uint8_t vdev_id,
			      uint8_t *peer_mac)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *link_peer = NULL;
	uint8_t link_id;
	struct mlo_partner_info partner_info;
	struct qdf_mac_addr link_addr;
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc,
						    vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev)
		return;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev))
		goto end;

	link_id = mlo_roam_get_link_id(vdev_id, sync_ind);
	/* currently only 2 link MLO supported */
	partner_info.num_partner_links = 1;
	status = mlo_get_sta_link_mac_addr(vdev_id, sync_ind, &link_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Link mac address not found");
		goto end;
	}

	qdf_mem_copy(partner_info.partner_link_info[0].link_addr.bytes,
		     link_addr.bytes, QDF_MAC_ADDR_SIZE);
	partner_info.partner_link_info[0].link_id = link_id;
	pe_debug("link_addr " QDF_MAC_ADDR_FMT,
		 QDF_MAC_ADDR_REF(
			partner_info.partner_link_info[0].link_addr.bytes));

	/* Get the bss peer obj */
	link_peer = wlan_objmgr_get_peer_by_mac(mac->psoc, peer_mac,
						WLAN_LEGACY_MAC_ID);
	if (!link_peer)
		goto end;

	status = wlan_mlo_peer_create(vdev, link_peer,
				      &partner_info, NULL, 0);

	if (QDF_IS_STATUS_ERROR(status))
		pe_err("Peer creation failed");

	wlan_objmgr_peer_release_ref(link_peer, WLAN_LEGACY_MAC_ID);

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

void
lim_mlo_roam_delete_link_peer(struct pe_session *pe_session,
			      tpDphHashNode sta_ds)
{
	struct wlan_objmgr_peer *peer = NULL;
	struct mac_context *mac;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		pe_err("mac ctx is null");
		return;
	}
	if (!pe_session) {
		pe_err("pe session is null");
		return;
	}
	if (!sta_ds) {
		pe_err("sta ds is null");
		return;
	}

	peer = wlan_objmgr_get_peer_by_mac(mac->psoc,
					   sta_ds->staAddr,
					   WLAN_LEGACY_MAC_ID);
	if (!peer) {
		mlo_err("Peer is null");
		return;
	}

	wlan_mlo_link_peer_delete(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
}
#endif

void lim_update_omn_ie_ch_width(struct wlan_objmgr_vdev *vdev,
				enum phy_ch_width ch_width)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	mlme_priv->connect_info.assoc_chan_info.omn_ie_ch_width = ch_width;
}

#ifdef WLAN_FEATURE_11BE_MLO
static bool
lim_match_link_info(uint8_t req_link_id,
		    struct qdf_mac_addr *link_addr,
		    struct mlo_partner_info *partner_info)
{
	uint8_t i;

	for (i = 0; i < partner_info->num_partner_links; i++) {
		if (partner_info->partner_link_info[i].link_id == req_link_id &&
		    (qdf_is_macaddr_equal(link_addr,
					  &partner_info->partner_link_info[i].link_addr)))
			return true;
	}

	return false;
}

QDF_STATUS
lim_add_bcn_probe(struct wlan_objmgr_vdev *vdev, uint8_t *bcn_probe,
		  uint32_t len, qdf_freq_t freq, int32_t rssi)
{
	qdf_nbuf_t buf;
	struct wlan_objmgr_pdev *pdev;
	uint8_t *data, i, vdev_id;
	struct mgmt_rx_event_params rx_param = {0};
	struct wlan_frame_hdr *hdr;
	enum mgmt_frame_type frm_type = MGMT_BEACON;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev_id = wlan_vdev_get_id(vdev);
	if (!bcn_probe || !len || (len < sizeof(*hdr)) ||
	    len > MAX_MGMT_MPDU_LEN) {
		pe_err("bcn_probe is null or invalid len %d",
		       len);
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		pe_err("Failed to find pdev");
		return QDF_STATUS_E_FAILURE;
	}

	hdr = (struct wlan_frame_hdr *)bcn_probe;
	if ((hdr->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_MASK) ==
	    MGMT_SUBTYPE_PROBE_RESP)
		frm_type = MGMT_PROBE_RESP;

	rx_param.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	rx_param.chan_freq = freq;
	rx_param.rssi = rssi;

	/* Set all per chain rssi as invalid */
	for (i = 0; i < WLAN_MGMT_TXRX_HOST_MAX_ANTENNA; i++)
		rx_param.rssi_ctl[i] = WLAN_INVALID_PER_CHAIN_RSSI;

	buf = qdf_nbuf_alloc(NULL, qdf_roundup(len, 4), 0, 4, false);
	if (!buf)
		return QDF_STATUS_E_FAILURE;

	qdf_nbuf_put_tail(buf, len);
	qdf_nbuf_set_protocol(buf, ETH_P_CONTROL);

	data = qdf_nbuf_data(buf);
	qdf_mem_copy(data, bcn_probe, len);

	pe_debug("MLO: add prb rsp to scan db");
	/* buf will be freed by scan module in error or success case */
	status = wlan_scan_process_bcn_probe_rx_sync(wlan_pdev_get_psoc(pdev), buf,
			&rx_param, frm_type);

	return status;
}

#ifdef WLAN_FEATURE_11BE_MLO
static QDF_STATUS
lim_validate_probe_rsp_link_info(struct pe_session *session_entry,
				 uint8_t *probe_rsp,
				 uint32_t probe_rsp_len)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t *ml_ie = NULL;
	qdf_size_t ml_ie_total_len;
	struct mlo_partner_info partner_info;
	uint8_t i;
	struct mlo_partner_info ml_partner_info;

	status = util_find_mlie(probe_rsp + WLAN_PROBE_RESP_IES_OFFSET,
				probe_rsp_len - WLAN_PROBE_RESP_IES_OFFSET,
				&ml_ie, &ml_ie_total_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Mlo ie not found in Probe response");
		return status;
	}
	status = util_get_bvmlie_persta_partner_info(ml_ie,
						     ml_ie_total_len,
						     &partner_info);

	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Per STA profile parsing failed");
		return status;
	}

	ml_partner_info = session_entry->lim_join_req->partner_info;
	for (i = 0; i < ml_partner_info.num_partner_links; i++) {
		if (!lim_match_link_info(ml_partner_info.partner_link_info[i].link_id,
					 &ml_partner_info.partner_link_info[i].link_addr,
					 &partner_info)) {
			pe_err("Atleast one Per-STA profile is missing in the ML-probe response");
			return QDF_STATUS_E_PROTO;
		}
	}

	return status;
}

static void
lim_clear_ml_partner_info(struct pe_session *session_entry)
{
	uint8_t idx;
	struct mlo_partner_info *partner_info = NULL;

	if (!session_entry || !session_entry->lim_join_req)
		return;

	partner_info = &session_entry->lim_join_req->partner_info;
	if (!partner_info) {
		pe_err("Partner link info not present");
		return;
	}
	pe_debug_rl("Clear Partner Link/s information");
	for (idx = 0; idx < partner_info->num_partner_links; idx++) {
		mlo_mgr_clear_ap_link_info(session_entry->vdev,
			partner_info->partner_link_info[idx].link_addr.bytes);

		partner_info->partner_link_info[idx].link_id = 0;
		qdf_zero_macaddr(
			&partner_info->partner_link_info[idx].link_addr);
	}
	partner_info->num_partner_links = 0;
}

static QDF_STATUS
lim_check_scan_db_for_join_req_partner_info(struct pe_session *session_entry,
					    struct mac_context *mac_ctx)
{
	struct join_req *lim_join_req;
	struct wlan_objmgr_pdev *pdev;
	struct mlo_partner_info *partner_info;
	struct mlo_link_info *link_info;
	struct scan_cache_entry *partner_entry;
	uint8_t i;

	if (!session_entry) {
		pe_err("session entry is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mac_ctx) {
		pe_err("mac context is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	lim_join_req = session_entry->lim_join_req;
	if (!lim_join_req) {
		pe_err("join req is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pdev = mac_ctx->pdev;
	if (!pdev) {
		pe_err("pdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	partner_info = &lim_join_req->partner_info;
	for (i = 0; i < partner_info->num_partner_links; i++) {
		link_info = &partner_info->partner_link_info[i];
		partner_entry =
			wlan_scan_get_entry_by_bssid(pdev,
						     &link_info->link_addr);
		if (!partner_entry) {
			pe_err("Scan entry is not found for partner link %d "
			       QDF_MAC_ADDR_FMT,
			       link_info->link_id,
			       QDF_MAC_ADDR_REF(link_info->link_addr.bytes));
			return QDF_STATUS_E_FAILURE;
		}
		util_scan_free_cache_entry(partner_entry);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_update_mlo_mgr_info(struct mac_context *mac_ctx,
				   struct wlan_objmgr_vdev *vdev,
				   struct qdf_mac_addr *link_addr,
				   uint8_t link_id, uint16_t freq)
{
	struct wlan_objmgr_pdev *pdev;
	struct scan_cache_entry *cache_entry;
	struct wlan_channel channel;
	bool is_security_allowed;

	pdev = mac_ctx->pdev;
	if (!pdev) {
		pe_err("pdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	cache_entry = wlan_scan_get_scan_entry_by_mac_freq(pdev, link_addr,
							   freq);
	if (!cache_entry)
		return QDF_STATUS_E_FAILURE;

	/**
	 * Reject all the partner link if any partner link  doesn’t pass the
	 * security check and proceed connection with single link.
	 */
	is_security_allowed =
		wlan_cm_is_eht_allowed_for_current_security(
					wlan_pdev_get_psoc(mac_ctx->pdev),
					cache_entry, true);

	if (!is_security_allowed) {
		mlme_debug("current security is not valid for partner link link_addr:" QDF_MAC_ADDR_FMT,
			   QDF_MAC_ADDR_REF(link_addr->bytes));
		util_scan_free_cache_entry(cache_entry);
		return QDF_STATUS_E_FAILURE;
	}

	channel.ch_freq = cache_entry->channel.chan_freq;
	channel.ch_ieee = wlan_reg_freq_to_chan(pdev, channel.ch_freq);
	channel.ch_phymode = cache_entry->phy_mode;
	channel.ch_cfreq1 = cache_entry->channel.cfreq0;
	channel.ch_cfreq2 = cache_entry->channel.cfreq1;
	channel.ch_width =
		wlan_mlme_get_ch_width_from_phymode(cache_entry->phy_mode);

	/*
	 * Supplicant needs non zero center_freq1 in case of 20 MHz connection
	 * also as a response of get_channel request. In case of 20 MHz channel
	 * width central frequency is same as channel frequency
	 */
	if (channel.ch_width == CH_WIDTH_20MHZ)
		channel.ch_cfreq1 = channel.ch_freq;

	util_scan_free_cache_entry(cache_entry);

	mlo_mgr_update_ap_channel_info(vdev, link_id, (uint8_t *)link_addr,
				       channel);

	return QDF_STATUS_SUCCESS;
}
#else
static inline void
lim_clear_ml_partner_info(struct pe_session *session_entry)
{
}

static QDF_STATUS
lim_check_db_for_join_req_partner_info(struct pe_session *session_entry,
				       struct mac_context *mac_ctx)
{

	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS lim_check_for_ml_probe_req(struct pe_session *session)
{
	if (!session || !session->lim_join_req)
		return QDF_STATUS_E_NULL_VALUE;

	if (session->lim_join_req->is_ml_probe_req_sent)
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS lim_check_partner_link_for_cmn_akm(struct pe_session *session)
{
	struct scan_filter *filter;
	qdf_list_t *scan_list = NULL;
	struct wlan_objmgr_pdev *pdev;
	uint8_t idx;
	struct mlo_link_info *link_info;
	struct scan_cache_entry *cur_entry;
	struct scan_cache_node *link_node;
	struct mlo_partner_info *partner_info;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pdev =  wlan_vdev_get_pdev(session->vdev);
	if (!pdev) {
		pe_err("PDEV NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	cur_entry =
		wlan_cm_get_curr_candidate_entry(session->vdev, session->cm_id);
	if (!cur_entry) {
		pe_err("Current candidate NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter) {
		status = QDF_STATUS_E_NOMEM;
		goto mem_free;
	}

	partner_info = &session->lim_join_req->partner_info;
	for (idx = 0; idx < partner_info->num_partner_links; idx++) {
		link_info = &partner_info->partner_link_info[idx];
		qdf_copy_macaddr(&filter->bssid_list[idx],
				 &link_info->link_addr);
		filter->num_of_bssid++;

		filter->chan_freq_list[idx] = link_info->chan_freq;
		filter->num_of_channels++;
	}

	/* If no.of. scan entries fetched not equal to no.of partner links
	 * then fail as common AKM is not determined.
	 */
	scan_list = wlan_scan_get_result(pdev, filter);
	qdf_mem_free(filter);
	if (!scan_list ||
	    (qdf_list_size(scan_list) != partner_info->num_partner_links)) {
		status = QDF_STATUS_E_INVAL;
		goto mem_free;
	}

	qdf_list_peek_front(scan_list, &cur_node);
	while (cur_node) {
		qdf_list_peek_next(scan_list, cur_node, &next_node);
		link_node = qdf_container_of(cur_node, struct scan_cache_node,
					     node);

		if (!wlan_scan_entries_contain_cmn_akm(cur_entry,
						       link_node->entry)) {
			status = QDF_STATUS_E_FAILURE;
			break;
		}

		cur_node = next_node;
		next_node = NULL;
	}

mem_free:
	if (scan_list)
		wlan_scan_purge_results(scan_list);
	util_scan_free_cache_entry(cur_entry);
	return status;
}

QDF_STATUS lim_gen_link_specific_probe_rsp(struct mac_context *mac_ctx,
					   struct pe_session *session_entry,
					   tpSirProbeRespBeacon rcvd_probe_resp,
					   uint8_t *probe_rsp,
					   uint32_t probe_rsp_len, int32_t rssi)
{
	struct element_info link_probe_rsp = {0};
	struct qdf_mac_addr sta_link_addr;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mlo_link_info *link_info = NULL;
	struct mlo_partner_info *partner_info;
	uint8_t chan;
	uint8_t op_class;
	uint16_t chan_freq, gen_frame_len;
	uint8_t idx;
	uint8_t req_link_id;

	if (!session_entry)
		return QDF_STATUS_E_NULL_VALUE;

	if (!session_entry->lim_join_req)
		return status;

	partner_info = &session_entry->lim_join_req->partner_info;
	if (!partner_info->num_partner_links) {
		pe_debug("No partner link info since supports 1 link only");
		return status;
	}

	if (session_entry->lim_join_req->is_ml_probe_req_sent &&
	    rcvd_probe_resp->mlo_ie.mlo_ie_present) {
		session_entry->lim_join_req->is_ml_probe_req_sent = false;

		partner_info = &session_entry->lim_join_req->partner_info;
		if (!partner_info->num_partner_links) {
			pe_err("STA doesn't have any partner link information");
			return QDF_STATUS_E_FAILURE;
		}

		status = lim_validate_probe_rsp_link_info(session_entry,
							  probe_rsp,
							  probe_rsp_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			if(QDF_IS_STATUS_ERROR(
				lim_check_scan_db_for_join_req_partner_info(
						session_entry,
						mac_ctx)))
				lim_clear_ml_partner_info(session_entry);
			return status;
		}

		/*
		 * When an MLO probe response is received from a link,
		 * the other link might be superior in features compared to the
		 * link that sent ML probe rsp and the per-STA profile
		 * info may carry corresponding IEs. These IEs are extracted
		 * and added to IE list of link probe response while generating
		 * it. So, the new link probe response generated might be of
		 * more size than the original link probe rsp. Allocate buffer
		 * for the scan entry to accommodate all of the IEs got
		 * generated as part of link probe rsp generation. Allocate
		 * MAX_MGMT_MPDU_LEN bytes for IEs as the max frame size that
		 * can be received from AP is MAX_MGMT_MPDU_LEN bytes.
		 */
		gen_frame_len = MAX_MGMT_MPDU_LEN;

		link_probe_rsp.ptr = qdf_mem_malloc(gen_frame_len);
		if (!link_probe_rsp.ptr) {
			if(QDF_IS_STATUS_ERROR(
				lim_check_scan_db_for_join_req_partner_info(
					session_entry,
					mac_ctx)))
				lim_clear_ml_partner_info(session_entry);
			return QDF_STATUS_E_NOMEM;
		}

		link_probe_rsp.len = gen_frame_len;
		qdf_mem_copy(&sta_link_addr, session_entry->self_mac_addr,
			     QDF_MAC_ADDR_SIZE);

		for (idx = 0; idx < partner_info->num_partner_links; idx++) {
			req_link_id =
				partner_info->partner_link_info[idx].link_id;
			status = util_gen_link_probe_rsp(probe_rsp,
					probe_rsp_len, req_link_id,
					sta_link_addr, link_probe_rsp.ptr,
					gen_frame_len,
					(qdf_size_t *)&link_probe_rsp.len);

			if (QDF_IS_STATUS_ERROR(status)) {
				pe_err("MLO: Link %d probe resp gen failed %d",
				       req_link_id, status);
				status =
				   lim_check_scan_db_for_join_req_partner_info(
							session_entry, mac_ctx);
				if (QDF_IS_STATUS_ERROR(status))
				       lim_clear_ml_partner_info(session_entry);

				goto end;
			}

			pe_debug("MLO:link probe rsp size:%u orig probe rsp:%u",
				 link_probe_rsp.len, probe_rsp_len);

			link_info = &partner_info->partner_link_info[idx];
			wlan_get_chan_by_bssid_from_rnr(session_entry->vdev,
							session_entry->cm_id,
							&link_info->link_addr,
							&chan, &op_class);
			if (!chan)
				wlan_get_chan_by_link_id_from_rnr(
							session_entry->vdev,
							session_entry->cm_id,
							link_info->link_id,
							&chan, &op_class);
			if (!chan) {
				pe_err("Invalid link id %d link mac: " QDF_MAC_ADDR_FMT,
				  link_info->link_id,
				  QDF_MAC_ADDR_REF(link_info->link_addr.bytes));
				status =
				   lim_check_scan_db_for_join_req_partner_info(
					session_entry, mac_ctx);
				if (QDF_IS_STATUS_ERROR(status))
				       lim_clear_ml_partner_info(session_entry);

				status = QDF_STATUS_E_FAILURE;
				goto end;
			}
			chan_freq =
				wlan_reg_chan_opclass_to_freq(chan, op_class,
							      true);

			status = lim_add_bcn_probe(session_entry->vdev,
						   link_probe_rsp.ptr,
						   link_probe_rsp.len,
						   chan_freq, rssi);
			if (QDF_IS_STATUS_ERROR(status)) {
				pe_err("failed to add bcn probe %d", status);
				status =
				   lim_check_scan_db_for_join_req_partner_info(
					session_entry, mac_ctx);
				if (QDF_IS_STATUS_ERROR(status))
				       lim_clear_ml_partner_info(session_entry);

				goto end;
			}

			status = lim_update_mlo_mgr_info(mac_ctx,
							 session_entry->vdev,
							 &link_info->link_addr,
							 link_info->link_id,
							 link_info->chan_freq);
			if (QDF_IS_STATUS_ERROR(status)) {
				pe_err("failed to update mlo_mgr %d", status);
				lim_clear_ml_partner_info(session_entry);

				goto end;
			}
		}
		/*
		 * If the partner link's AKM not matching current candidate
		 * remove the partner links for this association.
		 */
		status = lim_check_partner_link_for_cmn_akm(session_entry);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_debug("Non overlapping partner link AKM %d",
				 status);
			lim_clear_ml_partner_info(session_entry);
			goto end;
		}
	} else if (session_entry->lim_join_req->is_ml_probe_req_sent &&
		   !rcvd_probe_resp->mlo_ie.mlo_ie_present) {
		status =
			lim_check_scan_db_for_join_req_partner_info(
						session_entry, mac_ctx);
		if (QDF_IS_STATUS_ERROR(status))
			lim_clear_ml_partner_info(session_entry);

		return QDF_STATUS_E_FAILURE;
	} else {
		return status;
	}
end:
	if (link_probe_rsp.ptr)
		qdf_mem_free(link_probe_rsp.ptr);
	link_probe_rsp.ptr = NULL;
	link_probe_rsp.len = 0;
	return status;
}

QDF_STATUS
lim_process_cu_for_probe_rsp(struct mac_context *mac_ctx,
			     struct pe_session *session,
			     uint8_t *probe_rsp,
			     uint32_t probe_rsp_len)
{
	struct element_info link_probe_rsp;
	struct qdf_mac_addr sta_link_addr;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_vdev *partner_vdev;
	uint8_t *ml_ie = NULL;
	qdf_size_t ml_ie_total_len = 0;
	struct mlo_partner_info partner_info;
	uint8_t i, link_id, vdev_id;
	uint8_t bpcc, aui;
	bool cu_flag = false;
	const uint8_t *rnr;
	bool msd_cap_found = false;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	vdev = session->vdev;
	if (!vdev || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return status;

	rnr = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_REDUCED_NEIGHBOR_REPORT,
				   probe_rsp + WLAN_PROBE_RESP_IES_OFFSET,
				   probe_rsp_len - WLAN_PROBE_RESP_IES_OFFSET);
	if (!rnr)
		return status;

	status = util_find_mlie(probe_rsp + WLAN_PROBE_RESP_IES_OFFSET,
				probe_rsp_len - WLAN_PROBE_RESP_IES_OFFSET,
				&ml_ie, &ml_ie_total_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Mlo ie not found in Probe response");
		return status;
	}

	util_get_bvmlie_msd_cap(ml_ie, ml_ie_total_len, &msd_cap_found,
				NULL);
	if (msd_cap_found) {
		wlan_vdev_mlme_cap_clear(vdev, WLAN_VDEV_C_EMLSR_CAP);
		pe_debug("EMLSR not supported with D2.0 AP");
	}

	status = util_get_bvmlie_persta_partner_info(ml_ie,
						     ml_ie_total_len,
						     &partner_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Per STA profile parsing failed");
		return status;
	}

	link_probe_rsp.ptr = qdf_mem_malloc(probe_rsp_len);
	if (!link_probe_rsp.ptr)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < partner_info.num_partner_links; i++) {
		link_id = partner_info.partner_link_info[i].link_id;
		partner_vdev = mlo_get_vdev_by_link_id(vdev, link_id,
						       WLAN_LEGACY_MAC_ID);
		if (!partner_vdev) {
			pe_debug("No partner vdev for link id %d", link_id);
			continue;
		}

		status = lim_cu_info_from_rnr_per_link_id(rnr, link_id,
							  &bpcc, &aui);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_debug("no cu info in rnr for link id %d", link_id);
			goto ref_rel;
		}

		cu_flag = lim_check_cu_happens(partner_vdev, bpcc);
		if (!cu_flag)
			goto ref_rel;

		vdev_id = wlan_vdev_get_id(partner_vdev);
		session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
		if (!session) {
			pe_debug("session is null for vdev id %d", vdev_id);
			goto ref_rel;
		}

		qdf_mem_copy(&sta_link_addr, session->self_mac_addr,
			     QDF_MAC_ADDR_SIZE);

		link_probe_rsp.len = probe_rsp_len;
		/* Todo:
		 * it needs to use link_id as parameter to generate
		 * specific probe rsp frame when api util_gen_link_probe_rsp
		 * updated.
		 */
		status =
		     util_gen_link_probe_rsp(probe_rsp, probe_rsp_len, link_id,
					     sta_link_addr, link_probe_rsp.ptr,
					     probe_rsp_len,
					     (qdf_size_t *)&link_probe_rsp.len);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("MLO: Link probe response generation failed %d",
			       status);
			goto ref_rel;
		}

		lim_process_gen_probe_rsp_frame(mac_ctx, session,
						link_probe_rsp.ptr,
						link_probe_rsp.len);

ref_rel:
		wlan_objmgr_vdev_release_ref(partner_vdev, WLAN_LEGACY_MAC_ID);
	}

	qdf_mem_free(link_probe_rsp.ptr);
	link_probe_rsp.ptr = NULL;
	link_probe_rsp.len = 0;
	return status;
}
#endif

#ifdef WLAN_FEATURE_SR
static
void lim_store_array_to_bit_map(uint64_t *val, uint8_t array[8])
{
	uint32_t bit_map_0 = 0;
	uint32_t bit_map_1 = 0;

	QDF_SET_BITS(bit_map_0, 0, SR_PADDING_BYTE, array[0]);
	QDF_SET_BITS(bit_map_0, 8, SR_PADDING_BYTE, array[1]);
	QDF_SET_BITS(bit_map_0, 16, SR_PADDING_BYTE, array[2]);
	QDF_SET_BITS(bit_map_0, 24, SR_PADDING_BYTE, array[3]);
	QDF_SET_BITS(bit_map_1, 0, SR_PADDING_BYTE, array[4]);
	QDF_SET_BITS(bit_map_1, 8, SR_PADDING_BYTE, array[5]);
	QDF_SET_BITS(bit_map_1, 16, SR_PADDING_BYTE, array[6]);
	QDF_SET_BITS(bit_map_1, 24, SR_PADDING_BYTE, array[7]);
	*val = (uint64_t) bit_map_0 |
	       (((uint64_t)bit_map_1) << 32);
}

void lim_update_vdev_sr_elements(struct pe_session *session_entry,
				 tpDphHashNode sta_ds)
{
	uint8_t sr_ctrl;
	uint8_t non_srg_max_pd_offset, srg_min_pd_offset, srg_max_pd_offset;
	uint64_t srg_color_bit_map = 0;
	uint64_t srg_partial_bssid_bit_map = 0;
	tDot11fIEspatial_reuse *srp_ie = &sta_ds->parsed_ies.srp_ie;

	sr_ctrl = srp_ie->sr_value15_allow << 4 |
		  srp_ie->srg_info_present << 3 |
		  srp_ie->non_srg_offset_present << 2 |
		  srp_ie->non_srg_pd_sr_disallow << 1 |
		  srp_ie->psr_disallow;
	non_srg_max_pd_offset =
		srp_ie->non_srg_offset.info.non_srg_pd_max_offset;
	srg_min_pd_offset = srp_ie->srg_info.info.srg_pd_min_offset;
	srg_max_pd_offset = srp_ie->srg_info.info.srg_pd_max_offset;
	lim_store_array_to_bit_map(&srg_color_bit_map,
				   srp_ie->srg_info.info.srg_color);
	lim_store_array_to_bit_map(&srg_partial_bssid_bit_map,
				   srp_ie->srg_info.info.srg_partial_bssid);
	pe_debug("Spatial Reuse Control field: %x Non-SRG Max PD Offset: %x SRG range %d - %d srg_color_bit_map:%llu srg_partial_bssid_bit_map: %llu",
		 sr_ctrl, non_srg_max_pd_offset, srg_min_pd_offset,
		 srg_max_pd_offset, srg_color_bit_map,
		 srg_partial_bssid_bit_map);
	wlan_vdev_mlme_set_srg_partial_bssid_bit_map(session_entry->vdev,
						     srg_partial_bssid_bit_map);
	wlan_vdev_mlme_set_srg_bss_color_bit_map(session_entry->vdev,
						 srg_color_bit_map);
	wlan_vdev_mlme_set_sr_ctrl(session_entry->vdev, sr_ctrl);
	wlan_vdev_mlme_set_non_srg_pd_offset(session_entry->vdev,
					     non_srg_max_pd_offset);
	wlan_vdev_mlme_set_srg_pd_offset(session_entry->vdev, srg_max_pd_offset,
					 srg_min_pd_offset);

}
#endif
