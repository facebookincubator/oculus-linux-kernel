/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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

#include "wni_api.h"
#include "wni_cfg.h"
#include "cfg_api.h"
#include "sir_api.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_timer_utils.h"
#include "lim_send_messages.h"
#include "lim_admit_control.h"
#include "lim_send_messages.h"
#include "lim_ibss_peer_mgmt.h"
#include "lim_ft.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "lim_session_utils.h"
#include "rrm_api.h"
#include "wma_types.h"
#include "cds_utils.h"
#include "lim_types.h"
#include "wlan_policy_mgr_api.h"
#include "nan_datapath.h"
#include "wlan_reg_services_api.h"
#include "wlan_pkt_capture_ucfg_api.h"

#define MAX_SUPPORTED_PEERS_WEP 16

void lim_process_mlm_join_cnf(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_auth_cnf(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_start_cnf(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_assoc_ind(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_assoc_cnf(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_reassoc_ind(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_set_keys_cnf(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_disassoc_ind(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_disassoc_cnf(tpAniSirGlobal, uint32_t *);
static void lim_process_mlm_deauth_ind(tpAniSirGlobal, tLimMlmDeauthInd *);
void lim_process_mlm_deauth_cnf(tpAniSirGlobal, uint32_t *);
void lim_process_mlm_purge_sta_ind(tpAniSirGlobal, uint32_t *);
void lim_get_session_info(tpAniSirGlobal pMac, uint8_t *, uint8_t *,
				uint16_t *);
/**
 * lim_process_mlm_rsp_messages()
 *
 ***FUNCTION:
 * This function is called to processes various MLM response (CNF/IND
 * messages from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param pMac       Pointer to Global MAC structure
 * @param  msgType   Indicates the MLM message type
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 *
 * @return None
 */
void
lim_process_mlm_rsp_messages(tpAniSirGlobal pMac, uint32_t msgType,
			     uint32_t *pMsgBuf)
{

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	MTRACE(mac_trace(pMac, TRACE_CODE_TX_LIM_MSG, 0, msgType));
	switch (msgType) {
	case LIM_MLM_AUTH_CNF:
		lim_process_mlm_auth_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_ASSOC_CNF:
		lim_process_mlm_assoc_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_START_CNF:
		lim_process_mlm_start_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_JOIN_CNF:
		lim_process_mlm_join_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_ASSOC_IND:
		lim_process_mlm_assoc_ind(pMac, pMsgBuf);
		break;
	case LIM_MLM_REASSOC_CNF:
		lim_process_mlm_reassoc_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_DISASSOC_CNF:
		lim_process_mlm_disassoc_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_DISASSOC_IND:
		lim_process_mlm_disassoc_ind(pMac, pMsgBuf);
		break;
	case LIM_MLM_PURGE_STA_IND:
		lim_process_mlm_purge_sta_ind(pMac, pMsgBuf);
		break;
	case LIM_MLM_DEAUTH_CNF:
		lim_process_mlm_deauth_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_DEAUTH_IND:
		lim_process_mlm_deauth_ind(pMac, (tLimMlmDeauthInd *)pMsgBuf);
		break;
	case LIM_MLM_SETKEYS_CNF:
		lim_process_mlm_set_keys_cnf(pMac, pMsgBuf);
		break;
	case LIM_MLM_TSPEC_CNF:
		break;
	default:
		break;
	} /* switch (msgType) */
	return;
} /*** end lim_process_mlm_rsp_messages() ***/

/**
 * lim_process_mlm_start_cnf()
 *
 ***FUNCTION:
 * This function is called to processes MLM_START_CNF
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param pMac       Pointer to Global MAC structure
 * @param pMsgBuf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_start_cnf(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tpPESession psessionEntry = NULL;
	tLimMlmStartCnf *pLimMlmStartCnf;
	uint8_t smesessionId;
	uint16_t smetransactionId;
	uint8_t channelId;
	uint8_t send_bcon_ind = false;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pLimMlmStartCnf = (tLimMlmStartCnf *) pMsgBuf;
	psessionEntry = pe_find_session_by_session_id(pMac,
				pLimMlmStartCnf->sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session does Not exist with given sessionId");
		return;
	}
	smesessionId = psessionEntry->smeSessionId;
	smetransactionId = psessionEntry->transactionId;

	if (psessionEntry->limSmeState != eLIM_SME_WT_START_BSS_STATE) {
		/*
		 * Should not have received Start confirm from MLM
		 * in other states. Log error.
		 */
		pe_err("received unexpected MLM_START_CNF in state %X",
				psessionEntry->limSmeState);
		return;
	}
	if (((tLimMlmStartCnf *) pMsgBuf)->resultCode == eSIR_SME_SUCCESS) {

		/*
		 * Update global SME state so that Beacon Generation
		 * module starts writing Beacon frames into TFP's
		 * Beacon file register.
		 */
		psessionEntry->limSmeState = eLIM_SME_NORMAL_STATE;
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_SME_STATE, psessionEntry->peSessionId,
			       psessionEntry->limSmeState));
		if (psessionEntry->bssType == eSIR_INFRA_AP_MODE)
			pe_debug("*** Started BSS in INFRA AP SIDE***");
		else if (psessionEntry->bssType == eSIR_NDI_MODE)
			pe_debug("*** Started BSS in NDI mode ***");
		else
			pe_debug("*** Started BSS ***");
	} else {
		/* Start BSS is a failure */
		pe_delete_session(pMac, psessionEntry);
		psessionEntry = NULL;
		pe_err("Start BSS Failed");
	}
	/* Send response to Host */
	lim_send_sme_start_bss_rsp(pMac, eWNI_SME_START_BSS_RSP,
				((tLimMlmStartCnf *)pMsgBuf)->resultCode,
				psessionEntry, smesessionId, smetransactionId);
	if ((psessionEntry != NULL) &&
		(((tLimMlmStartCnf *) pMsgBuf)->resultCode ==
						eSIR_SME_SUCCESS)) {
		channelId = psessionEntry->pLimStartBssReq->channelId;

		/* We should start beacon transmission only if the channel
		 * on which we are operating is non-DFS until the channel
		 * availability check is done. The PE will receive an explicit
		 * request from upper layers to start the beacon transmission
		 */

		if (!(LIM_IS_IBSS_ROLE(psessionEntry) ||
			(LIM_IS_AP_ROLE(psessionEntry))))
				return;
		if (psessionEntry->ch_width == CH_WIDTH_160MHZ) {
			send_bcon_ind = false;
		} else if (psessionEntry->ch_width == CH_WIDTH_80P80MHZ) {
			if ((wlan_reg_get_channel_state(pMac->pdev, channelId)
						!= CHANNEL_STATE_DFS) &&
			    (wlan_reg_get_channel_state(pMac->pdev,
					psessionEntry->ch_center_freq_seg1 -
					SIR_80MHZ_START_CENTER_CH_DIFF) !=
						CHANNEL_STATE_DFS))
				send_bcon_ind = true;
		} else {
			if (wlan_reg_get_channel_state(pMac->pdev, channelId)
					!= CHANNEL_STATE_DFS)
				send_bcon_ind = true;
		}
		if (send_bcon_ind) {
			/* Configure beacon and send beacons to HAL */
			QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
					FL("Start Beacon with ssid %s Ch %d"),
					psessionEntry->ssId.ssId,
					psessionEntry->currentOperChannel);
			lim_send_beacon_ind(pMac,
					    psessionEntry, REASON_DEFAULT);
			lim_enable_obss_detection_config(pMac, psessionEntry);
			lim_send_obss_color_collision_cfg(pMac, psessionEntry,
					OBSS_COLOR_COLLISION_DETECTION);
		}
	}
}

/*** end lim_process_mlm_start_cnf() ***/

/**
 * lim_process_mlm_join_cnf() - Processes join confirmation
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: A pointer to the MLM message buffer
 *
 * This Function handles the join confirmation message
 * LIM_MLM_JOIN_CNF.
 *
 * Return: None
 */
void lim_process_mlm_join_cnf(tpAniSirGlobal mac_ctx,
	uint32_t *msg)
{
	tSirResultCodes result_code;
	tLimMlmJoinCnf *join_cnf;
	tpPESession session_entry;

	join_cnf = (tLimMlmJoinCnf *) msg;
	session_entry = pe_find_session_by_session_id(mac_ctx,
		join_cnf->sessionId);
	if (session_entry == NULL) {
		pe_err("SessionId:%d does not exist", join_cnf->sessionId);
		return;
	}

	if (session_entry->limSmeState != eLIM_SME_WT_JOIN_STATE) {
		pe_err("received unexpected MLM_JOIN_CNF in state %X",
			session_entry->limSmeState);
		return;
	}

	result_code = ((tLimMlmJoinCnf *) msg)->resultCode;
	/* Process Join confirm from MLM */
	if (result_code == eSIR_SME_SUCCESS) {
		/* Setup hardware upfront */
		if (lim_sta_send_add_bss_pre_assoc(mac_ctx, false,
			session_entry) == QDF_STATUS_SUCCESS)
			return;
		else
			result_code = eSIR_SME_REFUSED;
	}

	/*  Join failure */
	session_entry->limSmeState = eLIM_SME_JOIN_FAILURE_STATE;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
		session_entry->peSessionId,
		session_entry->limSmeState));
	/* Send Join response to Host */
	lim_handle_sme_join_result(mac_ctx, result_code,
		((tLimMlmJoinCnf *) msg)->protStatusCode, session_entry);
	return;
}

/**
 * lim_send_mlm_assoc_req() - Association request will be processed
 * mac_ctx:  Pointer to Global MAC structure
 * session_entry:  Pointer to session etnry
 *
 * This function is sends ASSOC request MLM message to MLM State machine.
 * ASSOC request packet would be by picking parameters from psessionEntry
 * automatically based on the current state of MLM state machine.
 * ASSUMPTIONS:
 * this function is called in middle of connection state machine and is
 * expected to be called after auth cnf has been received or after ASSOC rsp
 * with TRY_AGAIN_LATER was received and required time has elapsed after that.
 *
 * Return: None
 */

static void lim_send_mlm_assoc_req(tpAniSirGlobal mac_ctx,
	tpPESession session_entry)
{
	tLimMlmAssocReq *assoc_req;
	uint32_t val;
	uint16_t caps;
	uint32_t tele_bcn = 0;
	tpSirMacCapabilityInfo cap_info;

	if (!session_entry->pLimJoinReq) {
		pe_err("Join Request is NULL");
		/* No need to Assert. JOIN timeout will handle this error */
		return;
	}

	assoc_req = qdf_mem_malloc(sizeof(tLimMlmAssocReq));
	if (NULL == assoc_req) {
		pe_err("call to AllocateMemory failed for mlmAssocReq");
		return;
	}
	val = sizeof(tSirMacAddr);
	sir_copy_mac_addr(assoc_req->peerMacAddr, session_entry->bssId);
	if (wlan_cfg_get_int(mac_ctx,  WNI_CFG_ASSOCIATION_FAILURE_TIMEOUT,
		(uint32_t *) &assoc_req->assocFailureTimeout)
		!= QDF_STATUS_SUCCESS) {
		/* Could not get AssocFailureTimeout value from CFG.*/
		pe_err("could not retrieve AssocFailureTimeout value");
	}

	if (cfg_get_capability_info(mac_ctx, &caps, session_entry)
			!= QDF_STATUS_SUCCESS)
		/* Could not get Capabilities value from CFG.*/
		pe_err("could not retrieve Capabilities value");

	/* Clear spectrum management bit if AP doesn't support it */
	if (!(session_entry->pLimJoinReq->bssDescription.capabilityInfo &
		LIM_SPECTRUM_MANAGEMENT_BIT_MASK))
		/*
		 * AP doesn't support spectrum management
		 * clear spectrum management bit
		 */
		caps &= (~LIM_SPECTRUM_MANAGEMENT_BIT_MASK);

	/* Clear rrm bit if AP doesn't support it */
	if (!(session_entry->pLimJoinReq->bssDescription.capabilityInfo &
		LIM_RRM_BIT_MASK))
		caps &= (~LIM_RRM_BIT_MASK);

	/* Clear short preamble bit if AP does not support it */
	if (!(session_entry->pLimJoinReq->bssDescription.capabilityInfo &
		(LIM_SHORT_PREAMBLE_BIT_MASK))) {
		caps &= (~LIM_SHORT_PREAMBLE_BIT_MASK);
		pe_debug("Clearing short preamble:no AP support");
	}

	/* Clear immediate block ack bit if AP does not support it */
	if (!(session_entry->pLimJoinReq->bssDescription.capabilityInfo &
		(LIM_IMMEDIATE_BLOCK_ACK_MASK))) {
		caps &= (~LIM_IMMEDIATE_BLOCK_ACK_MASK);
		pe_debug("Clearing Immed Blk Ack:no AP support");
	}

	assoc_req->capabilityInfo = caps;
	cap_info = ((tpSirMacCapabilityInfo) &assoc_req->capabilityInfo);

	/*
	 * If telescopic beaconing is enabled, set listen interval to
	 * WNI_CFG_TELE_BCN_MAX_LI
	 */
	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_TELE_BCN_WAKEUP_EN, &tele_bcn)
		!= QDF_STATUS_SUCCESS)
		pe_err("Couldn't get WNI_CFG_TELE_BCN_WAKEUP_EN");

	val = WNI_CFG_LISTEN_INTERVAL_STADEF;
	if (tele_bcn) {
		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_TELE_BCN_MAX_LI, &val) !=
			QDF_STATUS_SUCCESS)
			pe_err("could not retrieve ListenInterval");
	} else {
		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_LISTEN_INTERVAL,
			 &val) != QDF_STATUS_SUCCESS)
			pe_err("could not retrieve ListenInterval");
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_ASSOC_REQ_EVENT,
		session_entry, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
#endif
	assoc_req->listenInterval = (uint16_t) val;
	/* Update PE session ID */
	assoc_req->sessionId = session_entry->peSessionId;
	session_entry->limPrevSmeState = session_entry->limSmeState;
	session_entry->limSmeState = eLIM_SME_WT_ASSOC_STATE;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
		session_entry->peSessionId, session_entry->limSmeState));
	lim_post_mlm_message(mac_ctx, LIM_MLM_ASSOC_REQ,
		(uint32_t *) assoc_req);
}

/**
 * lim_process_mlm_auth_cnf()-Process Auth confirmation
 * @mac_ctx:  Pointer to Global MAC structure
 * @msg: A pointer to the MLM message buffer
 *
 * This function is called to processes MLM_AUTH_CNF
 * message from MLM State machine.
 *
 * Return: None
 */
void lim_process_mlm_auth_cnf(tpAniSirGlobal mac_ctx, uint32_t *msg)
{
	tAniAuthType auth_type, auth_mode;
	tLimMlmAuthReq *auth_req;
	tLimMlmAuthCnf *auth_cnf;
	tpPESession session_entry;

	if (msg == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	auth_cnf = (tLimMlmAuthCnf *) msg;
	session_entry = pe_find_session_by_session_id(mac_ctx,
			auth_cnf->sessionId);
	if (session_entry == NULL) {
		pe_err("SessionId:%d session doesn't exist",
			auth_cnf->sessionId);
		return;
	}

	if ((session_entry->limSmeState != eLIM_SME_WT_AUTH_STATE &&
		session_entry->limSmeState != eLIM_SME_WT_PRE_AUTH_STATE) ||
		LIM_IS_AP_ROLE(session_entry)) {
		/**
		 * Should not have received AUTH confirm
		 * from MLM in other states or on AP.
		 * Log error
		 */
		pe_err("SessionId:%d received MLM_AUTH_CNF in state %X",
			session_entry->peSessionId, session_entry->limSmeState);
		return;
	}

	if (auth_cnf->resultCode == eSIR_SME_SUCCESS) {
		if (session_entry->limSmeState == eLIM_SME_WT_AUTH_STATE) {
			lim_send_mlm_assoc_req(mac_ctx, session_entry);
		} else {
			/*
			 * Successful Pre-authentication. Send
			 * Pre-auth response to host
			 */
			session_entry->limSmeState =
				session_entry->limPrevSmeState;
			MTRACE(mac_trace
				(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
		}
		/* Return for success case */
		return;
	}
	/*
	 * Failure case handle:
	 * Process AUTH confirm from MLM
	 */
	if (session_entry->limSmeState == eLIM_SME_WT_AUTH_STATE) {
		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_AUTHENTICATION_TYPE,
			(uint32_t *) &auth_type) !=  QDF_STATUS_SUCCESS) {
			pe_err("Fail to retrieve AuthType value");
		}
	} else {
		auth_type = mac_ctx->lim.gLimPreAuthType;
	}

	if ((auth_type == eSIR_AUTO_SWITCH) &&
		(auth_cnf->authType == eSIR_SHARED_KEY) &&
		((eSIR_MAC_AUTH_ALGO_NOT_SUPPORTED_STATUS ==
			auth_cnf->protStatusCode) ||
		(auth_cnf->resultCode == eSIR_SME_AUTH_TIMEOUT_RESULT_CODE))) {
		/*
		 * When shared authentication fails with reason
		 * code "13" and authType set to 'auto switch',
		 * Try with open Authentication
		 */
		auth_mode = eSIR_OPEN_SYSTEM;
		/* Trigger MAC based Authentication */
		auth_req = qdf_mem_malloc(sizeof(tLimMlmAuthReq));
		if (NULL == auth_req) {
			pe_err("mlmAuthReq :Memory alloc failed");
			return;
		}
		if (session_entry->limSmeState ==
			eLIM_SME_WT_AUTH_STATE) {
			sir_copy_mac_addr(auth_req->peerMacAddr,
				session_entry->bssId);
		} else {
			qdf_mem_copy((uint8_t *)&auth_req->peerMacAddr,
			(uint8_t *)&mac_ctx->lim.gLimPreAuthPeerAddr,
			sizeof(tSirMacAddr));
		}
		auth_req->authType = auth_mode;
		/* Update PE session Id */
		auth_req->sessionId = auth_cnf->sessionId;
		if (wlan_cfg_get_int(mac_ctx,
			WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT,
			(uint32_t *) &auth_req->authFailureTimeout)
			!= QDF_STATUS_SUCCESS) {
			pe_err("Fail:retrieve AuthFailureTimeout");
		}
		lim_post_mlm_message(mac_ctx, LIM_MLM_AUTH_REQ,
			(uint32_t *) auth_req);
		return;
	} else {
		/* MAC based authentication failure */
		if (session_entry->limSmeState ==
			eLIM_SME_WT_AUTH_STATE) {
			pe_err("Auth Failure occurred");
			session_entry->limSmeState =
				eLIM_SME_JOIN_FAILURE_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
			session_entry->limMlmState =
				eLIM_MLM_IDLE_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
				session_entry->peSessionId,
				session_entry->limMlmState));

			/* WAR for some IOT issue on specific AP:
			 * STA failed to connect to SAE AP due to incorrect
			 * password is used. Then AP will still reject the
			 * authentication even correct password is used unless
			 * STA send deauth to AP upon authentication failure.
			 */
			if (auth_type == eSIR_AUTH_TYPE_SAE) {
				pe_debug("Send deauth for SAE auth failure");
				lim_send_deauth_mgmt_frame(mac_ctx,
						       auth_cnf->protStatusCode,
						       auth_cnf->peerMacAddr,
						       session_entry, false);
			}

			/*
			 * Need to send Join response with
			 * auth failure to Host.
			 */
			lim_handle_sme_join_result(mac_ctx,
				auth_cnf->resultCode,
				auth_cnf->protStatusCode,
				session_entry);
		} else {
			/*
			 * Pre-authentication failure.
			 * Send Pre-auth failure response to host
			 */
			session_entry->limSmeState =
				session_entry->limPrevSmeState;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
		}
	}
}

/**
 * lim_process_mlm_assoc_cnf() - Process association confirmation
 * @mac_ctx:  Pointer to Global MAC structure
 * @msg:  A pointer to the MLM message buffer
 *
 * This function is called to processes MLM_ASSOC_CNF
 * message from MLM State machine.
 *
 * Return: None
 */
void lim_process_mlm_assoc_cnf(tpAniSirGlobal mac_ctx,
	uint32_t *msg)
{
	tpPESession session_entry;
	tLimMlmAssocCnf *assoc_cnf;

	if (msg == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	assoc_cnf = (tLimMlmAssocCnf *) msg;
	session_entry = pe_find_session_by_session_id(mac_ctx,
				assoc_cnf->sessionId);
	if (session_entry == NULL) {
		pe_err("SessionId:%d Session does not exist",
			assoc_cnf->sessionId);
		return;
	}
	if (session_entry->limSmeState != eLIM_SME_WT_ASSOC_STATE ||
		 LIM_IS_AP_ROLE(session_entry)) {
		/*
		 * Should not have received Assocication confirm
		 * from MLM in other states OR on AP.
		 * Log error
		 */
		pe_err("SessionId:%d Received MLM_ASSOC_CNF in state %X",
			session_entry->peSessionId, session_entry->limSmeState);
		return;
	}
	if (((tLimMlmAssocCnf *) msg)->resultCode != eSIR_SME_SUCCESS) {
		/* Association failure */
		pe_err("SessionId:%d Association failure resultCode: %d limSmeState:%d",
			session_entry->peSessionId,
			((tLimMlmAssocCnf *) msg)->resultCode,
			session_entry->limSmeState);

		/* If driver gets deauth when its waiting for ADD_STA_RSP then
		 * we need to do DEL_STA followed by DEL_BSS. So based on below
		 * reason-code here we decide whether to do only DEL_BSS or
		 * DEL_STA + DEL_BSS.
		 */
		if (((tLimMlmAssocCnf *) msg)->resultCode !=
		    eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA)
			session_entry->limSmeState =
				eLIM_SME_JOIN_FAILURE_STATE;

		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId, mac_ctx->lim.gLimSmeState));
		/*
		 * Need to send Join response with
		 * Association failure to Host.
		 */
		lim_handle_sme_join_result(mac_ctx,
			((tLimMlmAssocCnf *) msg)->resultCode,
			((tLimMlmAssocCnf *) msg)->protStatusCode,
			session_entry);
	} else {
		/* Successful Association */
		pe_debug("SessionId:%d Associated with BSS",
			session_entry->peSessionId);
		session_entry->limSmeState = eLIM_SME_LINK_EST_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId,
			session_entry->limSmeState));
		/**
		 * Need to send Join response with
		 * Association success to Host.
		 */
		lim_handle_sme_join_result(mac_ctx,
			((tLimMlmAssocCnf *) msg)->resultCode,
			((tLimMlmAssocCnf *) msg)->protStatusCode,
			session_entry);
	}
}

/**
 * lim_fill_assoc_ind_params() - Initialize association indication
 * mac_ctx: Pointer to Global MAC structure
 * assoc_ind: PE association indication structure
 * sme_assoc_ind: SME association indication
 * session_entry: PE session entry
 *
 * This function is called to initialzie the association
 * indication strucutre to process association indication.
 *
 * Return: None
 */

static void
lim_fill_assoc_ind_params(tpAniSirGlobal mac_ctx,
	tpLimMlmAssocInd assoc_ind, tSirSmeAssocInd *sme_assoc_ind,
	tpPESession session_entry)
{
	sme_assoc_ind->length = sizeof(tSirSmeAssocInd);
	sme_assoc_ind->sessionId = session_entry->smeSessionId;

	/* Required for indicating the frames to upper layer */
	sme_assoc_ind->assocReqLength = assoc_ind->assocReqLength;
	sme_assoc_ind->assocReqPtr = assoc_ind->assocReqPtr;

	sme_assoc_ind->beaconPtr = session_entry->beacon;
	sme_assoc_ind->beaconLength = session_entry->bcnLen;

	/* Fill in peerMacAddr */
	qdf_mem_copy(sme_assoc_ind->peerMacAddr, assoc_ind->peerMacAddr,
		sizeof(tSirMacAddr));

	/* Fill in aid */
	sme_assoc_ind->aid = assoc_ind->aid;
	/* Fill in bssId */
	qdf_mem_copy(sme_assoc_ind->bssId, session_entry->bssId,
		sizeof(tSirMacAddr));
	/* Fill in authType */
	sme_assoc_ind->authType = assoc_ind->authType;
	/* Fill in rsn_akm_type */
	sme_assoc_ind->akm_type = assoc_ind->akm_type;
	/* Fill in ssId */
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->ssId,
		(uint8_t *) &(assoc_ind->ssId), assoc_ind->ssId.length + 1);
	sme_assoc_ind->rsnIE.length = assoc_ind->rsnIE.length;
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->rsnIE.rsnIEdata,
		(uint8_t *) &(assoc_ind->rsnIE.rsnIEdata),
		assoc_ind->rsnIE.length);

#ifdef FEATURE_WLAN_WAPI
	sme_assoc_ind->wapiIE.length = assoc_ind->wapiIE.length;
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->wapiIE.wapiIEdata,
		(uint8_t *) &(assoc_ind->wapiIE.wapiIEdata),
		assoc_ind->wapiIE.length);
#endif
	sme_assoc_ind->addIE.length = assoc_ind->addIE.length;
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->addIE.addIEdata,
		(uint8_t *) &(assoc_ind->addIE.addIEdata),
		assoc_ind->addIE.length);

	/* Copy the new TITAN capabilities */
	sme_assoc_ind->spectrumMgtIndicator = assoc_ind->spectrumMgtIndicator;
	if (assoc_ind->spectrumMgtIndicator == true) {
		sme_assoc_ind->powerCap.minTxPower =
			assoc_ind->powerCap.minTxPower;
		sme_assoc_ind->powerCap.maxTxPower =
			assoc_ind->powerCap.maxTxPower;
		sme_assoc_ind->supportedChannels.numChnl =
			assoc_ind->supportedChannels.numChnl;
		qdf_mem_copy((uint8_t *) &sme_assoc_ind->supportedChannels.
			channelList,
			(uint8_t *) &(assoc_ind->supportedChannels.channelList),
			assoc_ind->supportedChannels.numChnl);
	}
	qdf_mem_copy(&sme_assoc_ind->chan_info, &assoc_ind->chan_info,
		sizeof(tSirSmeChanInfo));
	/* Fill in WmmInfo */
	sme_assoc_ind->wmmEnabledSta = assoc_ind->WmmStaInfoPresent;
	sme_assoc_ind->ampdu = assoc_ind->ampdu;
	sme_assoc_ind->sgi_enable = assoc_ind->sgi_enable;
	sme_assoc_ind->tx_stbc = assoc_ind->tx_stbc;
	sme_assoc_ind->rx_stbc = assoc_ind->rx_stbc;
	sme_assoc_ind->ch_width = assoc_ind->ch_width;
	sme_assoc_ind->mode = assoc_ind->mode;
	sme_assoc_ind->max_supp_idx = assoc_ind->max_supp_idx;
	sme_assoc_ind->max_ext_idx = assoc_ind->max_ext_idx;
	sme_assoc_ind->max_mcs_idx = assoc_ind->max_mcs_idx;
	sme_assoc_ind->rx_mcs_map = assoc_ind->rx_mcs_map;
	sme_assoc_ind->tx_mcs_map = assoc_ind->tx_mcs_map;
	sme_assoc_ind->ecsa_capable = assoc_ind->ecsa_capable;

	if (assoc_ind->ht_caps.present)
		sme_assoc_ind->HTCaps = assoc_ind->ht_caps;
	if (assoc_ind->vht_caps.present)
		sme_assoc_ind->VHTCaps = assoc_ind->vht_caps;
	sme_assoc_ind->capability_info = assoc_ind->capabilityInfo;
	sme_assoc_ind->he_caps_present = assoc_ind->he_caps_present;
	sme_assoc_ind->is_sae_authenticated = assoc_ind->is_sae_authenticated;
}

/**
 * lim_process_mlm_assoc_ind()
 *
 ***FUNCTION:
 * This function is called to processes MLM_ASSOC_IND
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param pMac       Pointer to Global MAC structure
 * @param pMsgBuf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_assoc_ind(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	uint32_t len;
	struct scheduler_msg msg = {0};
	tSirSmeAssocInd *pSirSmeAssocInd;
	tpDphHashNode pStaDs = 0;
	tpPESession psessionEntry;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	psessionEntry = pe_find_session_by_session_id(pMac,
				((tpLimMlmAssocInd) pMsgBuf)->
				sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given sessionId");
		return;
	}
	/* / Inform Host of STA association */
	len = sizeof(tSirSmeAssocInd);
	pSirSmeAssocInd = qdf_mem_malloc(len);
	if (NULL == pSirSmeAssocInd) {
		pe_err("call to AllocateMemory failed for eWNI_SME_ASSOC_IND");
		return;
	}

	pSirSmeAssocInd->messageType = eWNI_SME_ASSOC_IND;
	lim_fill_assoc_ind_params(pMac, (tpLimMlmAssocInd) pMsgBuf,
				  pSirSmeAssocInd,
				  psessionEntry);
	msg.type = eWNI_SME_ASSOC_IND;
	msg.bodyptr = pSirSmeAssocInd;
	msg.bodyval = 0;
	pStaDs = dph_get_hash_entry(pMac,
				    ((tpLimMlmAssocInd) pMsgBuf)->aid,
				    &psessionEntry->dph.dphHashTable);
	if (!pStaDs) {
		pe_err("MLM AssocInd: Station context no longer valid (aid %d)",
			((tpLimMlmAssocInd) pMsgBuf)->aid);
		qdf_mem_free(pSirSmeAssocInd);

		return;
	}
	pSirSmeAssocInd->staId = pStaDs->staIndex;
	pSirSmeAssocInd->reassocReq = pStaDs->mlmStaContext.subType;
	pSirSmeAssocInd->timingMeasCap = pStaDs->timingMeasCap;
	MTRACE(mac_trace(pMac, TRACE_CODE_TX_SME_MSG,
			 psessionEntry->peSessionId, msg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(pMac, WLAN_PE_DIAG_ASSOC_IND_EVENT, psessionEntry, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
	pe_debug("Create CNF_WAIT_TIMER after received LIM_MLM_ASSOC_IND");
	/*
	** turn on a timer to detect the loss of ASSOC CNF
	**/
	lim_activate_cnf_timer(pMac,
			       (uint16_t) ((tpLimMlmAssocInd) pMsgBuf)->aid,
			       psessionEntry);

	pMac->lim.sme_msg_callback(pMac, &msg);
} /*** end lim_process_mlm_assoc_ind() ***/

/**
 * lim_process_mlm_disassoc_ind()
 *
 ***FUNCTION:
 * This function is called to processes MLM_DISASSOC_IND
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param pMac       Pointer to Global MAC structure
 * @param pMsgBuf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_disassoc_ind(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tLimMlmDisassocInd *pMlmDisassocInd;
	tpPESession psessionEntry;

	pMlmDisassocInd = (tLimMlmDisassocInd *) pMsgBuf;
	psessionEntry = pe_find_session_by_session_id(pMac,
				pMlmDisassocInd->sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}
	switch (GET_LIM_SYSTEM_ROLE(psessionEntry)) {
	case eLIM_STA_IN_IBSS_ROLE:
		break;
	case eLIM_STA_ROLE:
		psessionEntry->limSmeState = eLIM_SME_WT_DISASSOC_STATE;
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_SME_STATE, psessionEntry->peSessionId,
			       psessionEntry->limSmeState));
		break;
	default:        /* eLIM_AP_ROLE */
		pe_debug("*** Peer staId=%d Disassociated ***",
			       pMlmDisassocInd->aid);
		/* Send SME_DISASOC_IND after Polaris cleanup */
		/* (after receiving LIM_MLM_PURGE_STA_IND) */
		break;
	} /* end switch (GET_LIM_SYSTEM_ROLE(psessionEntry)) */
} /*** end lim_process_mlm_disassoc_ind() ***/

/**
 * lim_process_mlm_disassoc_cnf() - Processes disassociation
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: A pointer to the MLM message buffer
 *
 * This function is called to processes MLM_DISASSOC_CNF
 * message from MLM State machine.
 *
 * Return: None
 */
void lim_process_mlm_disassoc_cnf(tpAniSirGlobal mac_ctx,
	uint32_t *msg)
{
	tSirResultCodes result_code;
	tLimMlmDisassocCnf *disassoc_cnf;
	tpPESession session_entry;

	disassoc_cnf = (tLimMlmDisassocCnf *) msg;

	session_entry =
		pe_find_session_by_session_id(mac_ctx, disassoc_cnf->sessionId);
	if (session_entry == NULL) {
		pe_err("session Does not exist for given session Id");
		return;
	}
	result_code = (tSirResultCodes)(disassoc_cnf->disassocTrigger ==
		eLIM_LINK_MONITORING_DISASSOC) ?
		eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE :
		disassoc_cnf->resultCode;
	if (LIM_IS_STA_ROLE(session_entry)) {
		/* Disassociate Confirm from MLM */
		if ((session_entry->limSmeState != eLIM_SME_WT_DISASSOC_STATE)
			&& (session_entry->limSmeState !=
			eLIM_SME_WT_DEAUTH_STATE)) {
			/*
			 * Should not have received
			 * Disassocate confirm
			 * from MLM in other states.Log error
			 */
			pe_err("received MLM_DISASSOC_CNF in state %X",
				session_entry->limSmeState);
			return;
		}
		if (mac_ctx->lim.gLimRspReqd)
			mac_ctx->lim.gLimRspReqd = false;
		if (disassoc_cnf->disassocTrigger ==
			eLIM_PROMISCUOUS_MODE_DISASSOC) {
			if (disassoc_cnf->resultCode != eSIR_SME_SUCCESS)
				session_entry->limSmeState =
					session_entry->limPrevSmeState;
			else
				session_entry->limSmeState =
					eLIM_SME_OFFLINE_STATE;
			MTRACE(mac_trace
				(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
		} else {
			if (disassoc_cnf->resultCode != eSIR_SME_SUCCESS)
				session_entry->limSmeState =
					session_entry->limPrevSmeState;
			else
				session_entry->limSmeState =
					eLIM_SME_IDLE_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
			lim_send_sme_disassoc_ntf(mac_ctx,
				disassoc_cnf->peerMacAddr, result_code,
				disassoc_cnf->disassocTrigger,
				disassoc_cnf->aid, session_entry->smeSessionId,
				session_entry->transactionId, session_entry);
		}
	} else if (LIM_IS_AP_ROLE(session_entry)) {
		lim_send_sme_disassoc_ntf(mac_ctx, disassoc_cnf->peerMacAddr,
			result_code, disassoc_cnf->disassocTrigger,
			disassoc_cnf->aid, session_entry->smeSessionId,
			session_entry->transactionId, session_entry);
	}
}

/**
 * lim_process_mlm_deauth_ind() - processes MLM_DEAUTH_IND
 * @mac_ctx: global mac structure
 * @deauth_ind: deauth indication
 *
 * This function is called to processes MLM_DEAUTH_IND
 * message from MLM State machine.
 *
 * Return: None
 */
static void lim_process_mlm_deauth_ind(tpAniSirGlobal mac_ctx,
				       tLimMlmDeauthInd *deauth_ind)
{
	tpPESession session;
	uint8_t session_id;
	enum eLimSystemRole role;

	if (!deauth_ind) {
		pe_err("deauth_ind is null");
		return;
	}
	session = pe_find_session_by_bssid(mac_ctx,
					   deauth_ind->peerMacAddr,
					   &session_id);
	if (!session) {
		pe_err("session does not exist for Addr:" MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(deauth_ind->peerMacAddr));
		return;
	}
	role = GET_LIM_SYSTEM_ROLE(session);
	if (role == eLIM_STA_ROLE) {
		session->limSmeState = eLIM_SME_WT_DEAUTH_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				 session->peSessionId, session->limSmeState));
	}
}

/**
 * lim_process_mlm_deauth_cnf()
 *
 ***FUNCTION:
 * This function is called to processes MLM_DEAUTH_CNF
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param pMac       Pointer to Global MAC structure
 * @param pMsgBuf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_deauth_cnf(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	uint16_t aid;
	tSirResultCodes resultCode;
	tLimMlmDeauthCnf *pMlmDeauthCnf;
	tpPESession psessionEntry;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pMlmDeauthCnf = (tLimMlmDeauthCnf *) pMsgBuf;
	psessionEntry = pe_find_session_by_session_id(pMac,
				pMlmDeauthCnf->sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given session Id");
		return;
	}

	resultCode = (tSirResultCodes)
		     (pMlmDeauthCnf->deauthTrigger ==
		      eLIM_LINK_MONITORING_DEAUTH) ?
		     eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE :
		     pMlmDeauthCnf->resultCode;
	aid = LIM_IS_AP_ROLE(psessionEntry) ? pMlmDeauthCnf->aid : 1;
	if (LIM_IS_STA_ROLE(psessionEntry)) {
		/* Deauth Confirm from MLM */
		if ((psessionEntry->limSmeState != eLIM_SME_WT_DISASSOC_STATE)
			&& psessionEntry->limSmeState !=
					eLIM_SME_WT_DEAUTH_STATE) {
			/**
			 * Should not have received Deauth confirm
			 * from MLM in other states.
			 * Log error
			 */
			pe_err("received unexpected MLM_DEAUTH_CNF in state %X",
				       psessionEntry->limSmeState);
			return;
		}
		if (pMlmDeauthCnf->resultCode == eSIR_SME_SUCCESS) {
			psessionEntry->limSmeState = eLIM_SME_IDLE_STATE;
		} else
			psessionEntry->limSmeState =
				psessionEntry->limPrevSmeState;
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_SME_STATE, psessionEntry->peSessionId,
			       psessionEntry->limSmeState));

		if (pMac->lim.gLimRspReqd)
			pMac->lim.gLimRspReqd = false;
	}
	/* On STA or on BASIC AP, send SME_DEAUTH_RSP to host */
	lim_send_sme_deauth_ntf(pMac, pMlmDeauthCnf->peer_macaddr.bytes,
				resultCode,
				pMlmDeauthCnf->deauthTrigger,
				aid, psessionEntry->smeSessionId,
				psessionEntry->transactionId);
} /*** end lim_process_mlm_deauth_cnf() ***/

/**
 * lim_process_mlm_purge_sta_ind()
 *
 ***FUNCTION:
 * This function is called to processes MLM_PURGE_STA_IND
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param pMac       Pointer to Global MAC structure
 * @param pMsgBuf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_purge_sta_ind(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tSirResultCodes resultCode;
	tpLimMlmPurgeStaInd pMlmPurgeStaInd;
	tpPESession psessionEntry;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pMlmPurgeStaInd = (tpLimMlmPurgeStaInd) pMsgBuf;
	psessionEntry = pe_find_session_by_session_id(pMac,
				pMlmPurgeStaInd->sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given bssId");
		return;
	}
	/* Purge STA indication from MLM */
	resultCode = (tSirResultCodes) pMlmPurgeStaInd->reasonCode;
	switch (GET_LIM_SYSTEM_ROLE(psessionEntry)) {
	case eLIM_STA_IN_IBSS_ROLE:
		break;
	case eLIM_STA_ROLE:
	default:        /* eLIM_AP_ROLE */
		if (LIM_IS_STA_ROLE(psessionEntry) &&
		   (psessionEntry->limSmeState !=
			eLIM_SME_WT_DISASSOC_STATE) &&
		   (psessionEntry->limSmeState != eLIM_SME_WT_DEAUTH_STATE)) {
			/**
			 * Should not have received
			 * Purge STA indication
			 * from MLM in other states.
			 * Log error
			 */
			pe_err("received unexpected MLM_PURGE_STA_IND in state %X",
				       psessionEntry->limSmeState);
			break;
		}
		pe_debug("*** Cleanup completed for staId=%d ***",
			       pMlmPurgeStaInd->aid);
		if (LIM_IS_STA_ROLE(psessionEntry)) {
			psessionEntry->limSmeState = eLIM_SME_IDLE_STATE;
			MTRACE(mac_trace
				       (pMac, TRACE_CODE_SME_STATE,
				       psessionEntry->peSessionId,
				       psessionEntry->limSmeState));

		}
		if (pMlmPurgeStaInd->purgeTrigger == eLIM_PEER_ENTITY_DEAUTH) {
			lim_send_sme_deauth_ntf(pMac,
						pMlmPurgeStaInd->peerMacAddr,
						resultCode,
						pMlmPurgeStaInd->purgeTrigger,
						pMlmPurgeStaInd->aid,
						psessionEntry->smeSessionId,
						psessionEntry->transactionId);
		} else
			lim_send_sme_disassoc_ntf(pMac,
						  pMlmPurgeStaInd->peerMacAddr,
						  resultCode,
						  pMlmPurgeStaInd->purgeTrigger,
						  pMlmPurgeStaInd->aid,
						  psessionEntry->smeSessionId,
						  psessionEntry->transactionId,
						  psessionEntry);
	} /* end switch (GET_LIM_SYSTEM_ROLE(psessionEntry)) */
} /*** end lim_process_mlm_purge_sta_ind() ***/

/**
 * lim_process_mlm_set_keys_cnf()
 *
 ***FUNCTION:
 * This function is called to processes MLM_SETKEYS_CNF
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param pMac       Pointer to Global MAC structure
 * @param pMsgBuf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_set_keys_cnf(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	/* Prepare and send SME_SETCONTEXT_RSP message */
	tLimMlmSetKeysCnf *pMlmSetKeysCnf;
	tpPESession psessionEntry;
	uint16_t aid;
	tpDphHashNode sta_ds;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pMlmSetKeysCnf = (tLimMlmSetKeysCnf *) pMsgBuf;
	psessionEntry = pe_find_session_by_session_id(pMac,
					   pMlmSetKeysCnf->sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given sessionId");
		return;
	}
	psessionEntry->is_key_installed = 0;
	pe_debug("Received MLM_SETKEYS_CNF with resultCode = %d",
		pMlmSetKeysCnf->resultCode);
	/* if the status is success keys are installed in the
	* Firmware so we can set the protection bit
	*/
	if (eSIR_SME_SUCCESS == pMlmSetKeysCnf->resultCode) {
		if (pMlmSetKeysCnf->key_len_nonzero)
			psessionEntry->is_key_installed = 1;
		if (LIM_IS_AP_ROLE(psessionEntry)) {
			sta_ds = dph_lookup_hash_entry(pMac,
				pMlmSetKeysCnf->peer_macaddr.bytes,
				&aid, &psessionEntry->dph.dphHashTable);
			if (sta_ds != NULL && pMlmSetKeysCnf->key_len_nonzero)
				sta_ds->is_key_installed = 1;
		}
	}
	pe_debug("is_key_installed = %d", psessionEntry->is_key_installed);

	lim_send_sme_set_context_rsp(pMac,
				     pMlmSetKeysCnf->peer_macaddr,
				     1,
				     (tSirResultCodes) pMlmSetKeysCnf->resultCode,
				     psessionEntry, psessionEntry->smeSessionId,
				     psessionEntry->transactionId);
} /*** end lim_process_mlm_set_keys_cnf() ***/

/**
 * lim_join_result_callback() - Callback to handle join rsp
 * @mac: Pointer to Global MAC structure
 * @param: callback argument
 * @status: status
 *
 * This callback function is used to delete PE session
 * entry and send join response to sme.
 *
 * Return: None
 */
static void lim_join_result_callback(tpAniSirGlobal mac, void *param,
				     bool status)
{
	join_params *link_state_params = (join_params *) param;
	tpPESession session;
	uint8_t sme_session_id;
	uint16_t sme_trans_id;

	if (!link_state_params) {
		pe_err("Link state params is NULL");
		return;
	}
	session = pe_find_session_by_session_id(mac, link_state_params->
						pe_session_id);
	if (!session) {
		qdf_mem_free(link_state_params);
		return;
	}
	sme_session_id = session->smeSessionId;
	sme_trans_id = session->transactionId;
	lim_send_sme_join_reassoc_rsp(mac, eWNI_SME_JOIN_RSP,
				      link_state_params->result_code,
				      link_state_params->prot_status_code,
				      session, sme_session_id, sme_trans_id);
	pe_delete_session(mac, session);
	qdf_mem_free(link_state_params);
}

/**
 * lim_handle_sme_join_result() - Handles sme join result
 * @mac_ctx:  Pointer to Global MAC structure
 * @result_code: Failure code to be sent
 * @prot_status_code : Protocol status code
 * @session_entry: PE session handle
 *
 * This function is called to process join/auth/assoc failures
 * upon receiving MLM_JOIN/AUTH/ASSOC_CNF with a failure code or
 * MLM_ASSOC_CNF with a success code in case of STA role and
 * MLM_JOIN_CNF with success in case of STA in IBSS role.
 *
 * Return: None
 */
void lim_handle_sme_join_result(tpAniSirGlobal mac_ctx,
	tSirResultCodes result_code, uint16_t prot_status_code,
	tpPESession session_entry)
{
	tpDphHashNode sta_ds = NULL;
	uint8_t sme_session_id;
	uint16_t sme_trans_id;
	join_params *param = NULL;

	if (session_entry == NULL) {
		pe_err("psessionEntry is NULL");
		return;
	}
	sme_session_id = session_entry->smeSessionId;
	sme_trans_id = session_entry->transactionId;
	/*
	 * When associations is failed , delete the session created
	 * and pass NULL  to  limsendsmeJoinReassocRsp()
	 */
	if (result_code != eSIR_SME_SUCCESS) {
		sta_ds =
			dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
		if (sta_ds != NULL) {
			sta_ds->mlmStaContext.disassocReason =
				eSIR_MAC_UNSPEC_FAILURE_REASON;
			sta_ds->mlmStaContext.cleanupTrigger =
				eLIM_JOIN_FAILURE;
			sta_ds->mlmStaContext.resultCode = result_code;
			sta_ds->mlmStaContext.protStatusCode = prot_status_code;
			/*
			 * FIX_ME: at the end of lim_cleanup_rx_path,
			 * make sure PE is sending eWNI_SME_JOIN_RSP
			 * to SME
			 */
			lim_cleanup_rx_path(mac_ctx, sta_ds, session_entry);
			qdf_mem_free(session_entry->pLimJoinReq);
			session_entry->pLimJoinReq = NULL;
			/* Cleanup if add bss failed */
			if (session_entry->add_bss_failed) {
				dph_delete_hash_entry(mac_ctx,
					 sta_ds->staAddr, sta_ds->assocId,
					 &session_entry->dph.dphHashTable);
				goto error;
			}
			return;
		}
		qdf_mem_free(session_entry->pLimJoinReq);
		session_entry->pLimJoinReq = NULL;
	}
error:
	/* Delete the session if JOIN failure occurred.
	 * if the peer is not created, then there is no
	 * need to send down the set link state which will
	 * try to delete the peer. Instead a join response
	 * failure should be sent to the upper layers.
	 */
	if (result_code != eSIR_SME_SUCCESS &&
	    result_code != eSIR_SME_PEER_CREATE_FAILED) {
		param = qdf_mem_malloc(sizeof(join_params));
		if (param != NULL) {
			param->result_code = result_code;
			param->prot_status_code = prot_status_code;
			param->pe_session_id = session_entry->peSessionId;
		}
		if (lim_set_link_state
			(mac_ctx, eSIR_LINK_DOWN_STATE, session_entry->bssId,
			 session_entry->selfMacAddr, lim_join_result_callback,
			 param) != QDF_STATUS_SUCCESS) {
			qdf_mem_free(param);
			pe_err("Failed to set the LinkState");
		}
		return;
	}

	lim_send_sme_join_reassoc_rsp(mac_ctx, eWNI_SME_JOIN_RSP, result_code,
		prot_status_code, session_entry, sme_session_id, sme_trans_id);
}

/**
 * lim_process_mlm_add_sta_rsp()
 *
 ***FUNCTION:
 * This function is called to process a WMA_ADD_STA_RSP from HAL.
 * Upon receipt of this message from HAL, MLME -
 * > Determines the "state" in which this message was received
 * > Forwards it to the appropriate callback
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  struct scheduler_msg  The MsgQ header, which contains the
 *  response buffer
 *
 * @return None
 */
void lim_process_mlm_add_sta_rsp(tpAniSirGlobal pMac,
				 struct scheduler_msg *limMsgQ,
				 tpPESession psessionEntry)
{
	/* we need to process the deferred message since the initiating req. there might be nested request. */
	/* in the case of nested request the new request initiated from the response will take care of resetting */
	/* the deffered flag. */
	SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
	if (LIM_IS_AP_ROLE(psessionEntry)) {
		lim_process_ap_mlm_add_sta_rsp(pMac, limMsgQ, psessionEntry);
		return;
	}
	lim_process_sta_mlm_add_sta_rsp(pMac, limMsgQ, psessionEntry);
}

/**
 * lim_process_sta_mlm_add_sta_rsp () - Process add sta response
 * @mac_ctx:  Pointer to mac context
 * @msg:  struct scheduler_msg *an Message structure
 * @session_entry: PE session entry
 *
 * Process ADD STA response sent from WMA and posts results
 * to SME.
 *
 * Return: Null
 */

void lim_process_sta_mlm_add_sta_rsp(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg, tpPESession session_entry)
{
	tLimMlmAssocCnf mlm_assoc_cnf;
	tpDphHashNode sta_ds;
	uint32_t msg_type = LIM_MLM_ASSOC_CNF;
	tpAddStaParams add_sta_params = (tpAddStaParams) msg->bodyptr;
	tpPESession ft_session = NULL;
	uint8_t ft_session_id;

	if (NULL == add_sta_params) {
		pe_err("Encountered NULL Pointer");
		return;
	}

	if (session_entry->limSmeState == eLIM_SME_WT_REASSOC_STATE)
		msg_type = LIM_MLM_REASSOC_CNF;

	if (true == session_entry->fDeauthReceived) {
		pe_err("Received Deauth frame in ADD_STA_RESP state");
		if (QDF_STATUS_SUCCESS == add_sta_params->status) {
			pe_err("ADD_STA success, send update result code with eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA staIdx: %d limMlmState: %d",
				add_sta_params->staIdx,
				session_entry->limMlmState);

			if (session_entry->limSmeState ==
					eLIM_SME_WT_REASSOC_STATE)
				msg_type = LIM_MLM_REASSOC_CNF;
			/*
			 * We are sending result code
			 * eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA which
			 * will trigger proper cleanup (DEL_STA/DEL_BSS both
			 * required) in either assoc cnf or reassoc cnf handler.
			 */
			mlm_assoc_cnf.resultCode =
				eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA;
			mlm_assoc_cnf.protStatusCode =
					   eSIR_MAC_UNSPEC_FAILURE_STATUS;
			session_entry->staId = add_sta_params->staIdx;
			goto end;
		}
	}

	if (QDF_STATUS_SUCCESS == add_sta_params->status) {
		if (eLIM_MLM_WT_ADD_STA_RSP_STATE !=
			session_entry->limMlmState) {
			pe_err("Received WMA_ADD_STA_RSP in state %X",
				session_entry->limMlmState);
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
			goto end;
		}
		if (session_entry->limSmeState == eLIM_SME_WT_REASSOC_STATE) {
			/* check if we have keys(PTK)to install in case of 11r */
			tpftPEContext ft_ctx = &session_entry->ftPEContext;

			ft_session = pe_find_session_by_bssid(mac_ctx,
				session_entry->limReAssocbssId, &ft_session_id);
			if (ft_session != NULL &&
				ft_ctx->PreAuthKeyInfo.extSetStaKeyParamValid
				== true) {
				tpLimMlmSetKeysReq pMlmStaKeys =
					&ft_ctx->PreAuthKeyInfo.extSetStaKeyParam;
				lim_send_set_sta_key_req(mac_ctx, pMlmStaKeys,
					0, 0, ft_session, false);
				ft_ctx->PreAuthKeyInfo.extSetStaKeyParamValid =
					false;
			}
		}
		/*
		 * Update the DPH Hash Entry for this STA
		 * with proper state info
		 */
		sta_ds =
			dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
		if (NULL != sta_ds) {
			sta_ds->mlmStaContext.mlmState =
				eLIM_MLM_LINK_ESTABLISHED_STATE;
			sta_ds->nss = add_sta_params->nss;
		} else
			pe_warn("Fail to get DPH Hash Entry for AID - %d",
				DPH_STA_HASH_INDEX_PEER);
		session_entry->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			session_entry->peSessionId,
			session_entry->limMlmState));
		/*
		 * Storing the self StaIndex(Generated by HAL) in
		 * session context, instead of storing it in DPH Hash
		 * entry for Self STA.
		 * DPH entry for the self STA stores the sta index for
		 * the BSS entry to which the STA is associated
		 */
		session_entry->staId = add_sta_params->staIdx;

#ifdef WLAN_DEBUG
		mac_ctx->lim.gLimNumLinkEsts++;
#endif
#ifdef FEATURE_WLAN_TDLS
		/* initialize TDLS peer related data */
		lim_init_tdls_data(mac_ctx, session_entry);
#endif
		/*
		 * Return Assoc confirm to SME with success
		 * FIXME - Need the correct ASSOC RSP code to
		 * be passed in here
		 */
		mlm_assoc_cnf.resultCode = (tSirResultCodes) eSIR_SME_SUCCESS;
		lim_send_obss_color_collision_cfg(mac_ctx, session_entry,
					OBSS_COLOR_COLLISION_DETECTION);
	} else {
		pe_err("ADD_STA failed!");
		if (session_entry->limSmeState == eLIM_SME_WT_REASSOC_STATE)
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_FT_REASSOC_FAILURE;
		else
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
		mlm_assoc_cnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
	}
end:
	if (NULL != msg->bodyptr) {
		qdf_mem_free(add_sta_params);
		msg->bodyptr = NULL;
	}
	/* Updating PE session Id */
	mlm_assoc_cnf.sessionId = session_entry->peSessionId;
	lim_post_sme_message(mac_ctx, msg_type, (uint32_t *) &mlm_assoc_cnf);
	if (true == session_entry->fDeauthReceived)
		session_entry->fDeauthReceived = false;
	return;
}

void lim_process_mlm_del_bss_rsp(tpAniSirGlobal pMac,
				 struct scheduler_msg *limMsgQ,
				 tpPESession psessionEntry)
{
	/* we need to process the deferred message since the initiating req. there might be nested request. */
	/* in the case of nested request the new request initiated from the response will take care of resetting */
	/* the deffered flag. */
	SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
	pMac->sys.gSysFrameCount[SIR_MAC_MGMT_FRAME][SIR_MAC_MGMT_DEAUTH] = 0;

	if (LIM_IS_AP_ROLE(psessionEntry) &&
	    (psessionEntry->statypeForBss == STA_ENTRY_SELF)) {
		lim_process_ap_mlm_del_bss_rsp(pMac, limMsgQ, psessionEntry);
		return;
	}
	lim_process_sta_mlm_del_bss_rsp(pMac, limMsgQ, psessionEntry);

#ifdef WLAN_FEATURE_11W
	if (psessionEntry->limRmfEnabled) {
		if (QDF_STATUS_SUCCESS !=
		    lim_send_exclude_unencrypt_ind(pMac, true, psessionEntry)) {
			pe_err("Could not send down Exclude Unencrypted Indication!");
		}
	}
#endif
}

void lim_process_sta_mlm_del_bss_rsp(tpAniSirGlobal pMac,
				     struct scheduler_msg *limMsgQ,
				     tpPESession psessionEntry)
{
	tpDeleteBssParams pDelBssParams = (tpDeleteBssParams) limMsgQ->bodyptr;
	tpDphHashNode pStaDs =
		dph_get_hash_entry(pMac, DPH_STA_HASH_INDEX_PEER,
				   &psessionEntry->dph.dphHashTable);
	tSirResultCodes statusCode = eSIR_SME_SUCCESS;

	if (NULL == pDelBssParams) {
		pe_err("Invalid body pointer in message");
		goto end;
	}
	if (QDF_STATUS_SUCCESS == pDelBssParams->status) {
		if (lim_set_link_state
			    (pMac, eSIR_LINK_IDLE_STATE, psessionEntry->bssId,
			    psessionEntry->selfMacAddr, NULL,
			    NULL) != QDF_STATUS_SUCCESS) {
			pe_err("Failure in setting link state to IDLE");
			statusCode = eSIR_SME_REFUSED;
			goto end;
		}
		if (pStaDs == NULL) {
			pe_err("DPH Entry for STA 1 missing");
			statusCode = eSIR_SME_REFUSED;
			goto end;
		}
		if (eLIM_MLM_WT_DEL_BSS_RSP_STATE !=
		    pStaDs->mlmStaContext.mlmState) {
			pe_err("Received unexpected WMA_DEL_BSS_RSP in state %X",
				       pStaDs->mlmStaContext.mlmState);
			statusCode = eSIR_SME_REFUSED;
			goto end;
		}
		pe_debug("STA AssocID %d MAC %pM", pStaDs->assocId,
			 pStaDs->staAddr);
	} else {
		pe_err("DEL BSS failed!");
		statusCode = eSIR_SME_STOP_BSS_FAILURE;
	}
end:
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pDelBssParams);
		limMsgQ->bodyptr = NULL;
	}
	if (pStaDs == NULL)
		return;
	if ((LIM_IS_STA_ROLE(psessionEntry)) &&
	    (psessionEntry->limSmeState !=
			eLIM_SME_WT_DISASSOC_STATE &&
	    psessionEntry->limSmeState !=
			eLIM_SME_WT_DEAUTH_STATE) &&
	    pStaDs->mlmStaContext.cleanupTrigger !=
			eLIM_JOIN_FAILURE) {
		/** The Case where the DelBss is invoked from
		 *  context of other than normal DisAssoc / Deauth OR
		 *  as part of Join Failure.
		 */
		lim_handle_del_bss_in_re_assoc_context(pMac, pStaDs, psessionEntry);
		return;
	}
	lim_prepare_and_send_del_sta_cnf(pMac, pStaDs, statusCode, psessionEntry);
	return;
}

void lim_process_ap_mlm_del_bss_rsp(tpAniSirGlobal pMac,
				    struct scheduler_msg *limMsgQ,
				    tpPESession psessionEntry)
{
	tSirResultCodes rc = eSIR_SME_SUCCESS;
	QDF_STATUS status;
	tpDeleteBssParams pDelBss = (tpDeleteBssParams) limMsgQ->bodyptr;
	tSirMacAddr nullBssid = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	if (psessionEntry == NULL) {
		pe_err("Session entry passed is NULL");
		if (pDelBss != NULL) {
			qdf_mem_free(pDelBss);
			limMsgQ->bodyptr = NULL;
		}
		return;
	}

	if (pDelBss == NULL) {
		pe_err("BSS: DEL_BSS_RSP with no body!");
		rc = eSIR_SME_REFUSED;
		goto end;
	}
	pMac->lim.gLimMlmState = eLIM_MLM_IDLE_STATE;
	MTRACE(mac_trace
		       (pMac, TRACE_CODE_MLM_STATE, NO_SESSION,
		       pMac->lim.gLimMlmState));

	if (eLIM_MLM_WT_DEL_BSS_RSP_STATE != psessionEntry->limMlmState) {
		pe_err("Received unexpected WMA_DEL_BSS_RSP in state %X",
			psessionEntry->limMlmState);
		rc = eSIR_SME_REFUSED;
		goto end;
	}
	if (pDelBss->status != QDF_STATUS_SUCCESS) {
		pe_err("BSS: DEL_BSS_RSP error (%x) Bss %d",
			pDelBss->status, pDelBss->bssIdx);
		rc = eSIR_SME_STOP_BSS_FAILURE;
		goto end;
	}
	status = lim_set_link_state(pMac, eSIR_LINK_IDLE_STATE, nullBssid,
				    psessionEntry->selfMacAddr, NULL, NULL);
	if (status != QDF_STATUS_SUCCESS) {
		rc = eSIR_SME_REFUSED;
		goto end;
	}
	/** Softmac may send all the buffered packets right after resuming the transmission hence
	 * to occupy the medium during non channel occupancy period. So resume the transmission after
	 * HAL gives back the response.
	 */
	dph_hash_table_class_init(pMac, &psessionEntry->dph.dphHashTable);
	lim_delete_pre_auth_list(pMac);
	/* Initialize number of associated stations during cleanup */
	psessionEntry->gLimNumOfCurrentSTAs = 0;
end:
	lim_send_sme_rsp(pMac, eWNI_SME_STOP_BSS_RSP, rc,
			 psessionEntry->smeSessionId,
			 psessionEntry->transactionId);
	pe_delete_session(pMac, psessionEntry);

	if (pDelBss != NULL) {
		qdf_mem_free(pDelBss);
		limMsgQ->bodyptr = NULL;
	}
}

/**
 * lim_process_mlm_del_sta_rsp() - Process DEL STA response
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: The MsgQ header, which contains the response buffer
 *
 * This function is called to process a WMA_DEL_STA_RSP from
 * WMA Upon receipt of this message from FW.
 *
 * Return: None
 */
void lim_process_mlm_del_sta_rsp(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg)
{
	/*
	 * we need to process the deferred message since the
	 * initiating req. there might be nested request
	 * in the case of nested request the new request
	 * initiated from the response will take care of resetting
	 * the deffered flag.
	 */
	tpPESession session_entry;
	tpDeleteStaParams del_sta_params;

	del_sta_params = (tpDeleteStaParams) msg->bodyptr;
	if (NULL == del_sta_params) {
		pe_err("null pointer del_sta_params msg");
		return;
	}
	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);

	session_entry = pe_find_session_by_session_id(mac_ctx,
				del_sta_params->sessionId);
	if (NULL == session_entry) {
		pe_err("Session Doesn't exist: %d",
			del_sta_params->sessionId);
		qdf_mem_free(del_sta_params);
		msg->bodyptr = NULL;
		return;
	}

	if (LIM_IS_AP_ROLE(session_entry)) {
		lim_process_ap_mlm_del_sta_rsp(mac_ctx, msg,
				session_entry);
		return;
	}
	if (LIM_IS_IBSS_ROLE(session_entry)) {
		lim_process_ibss_del_sta_rsp(mac_ctx, msg,
				session_entry);
		return;
	}
	if (LIM_IS_NDI_ROLE(session_entry)) {
		lim_process_ndi_del_sta_rsp(mac_ctx, msg, session_entry);
		return;
	}
	lim_process_sta_mlm_del_sta_rsp(mac_ctx, msg, session_entry);
}

/**
 * lim_process_ap_mlm_del_sta_rsp() - Process WMA_DEL_STA_RSP
 * @mac_ctx: Global pointer to MAC context
 * @msg: Received message
 * @session_entry: Session entry
 *
 * Process WMA_DEL_STA_RSP for AP role
 *
 * Retunrn: None
 */
void lim_process_ap_mlm_del_sta_rsp(tpAniSirGlobal mac_ctx,
					   struct scheduler_msg *msg,
					   tpPESession session_entry)
{
	tpDeleteStaParams del_sta_params = (tpDeleteStaParams) msg->bodyptr;
	tpDphHashNode sta_ds;
	tSirResultCodes status_code = eSIR_SME_SUCCESS;

	if (msg->bodyptr == NULL) {
		pe_err("msg->bodyptr NULL");
		return;
	}

	sta_ds = dph_get_hash_entry(mac_ctx, del_sta_params->assocId,
				    &session_entry->dph.dphHashTable);
	if (sta_ds == NULL) {
		pe_err("DPH Entry for STA %X missing",
			del_sta_params->assocId);
		status_code = eSIR_SME_REFUSED;
		qdf_mem_free(del_sta_params);
		msg->bodyptr = NULL;
		return;
	}
	pe_debug("Received del Sta Rsp in StaD MlmState: %d",
		sta_ds->mlmStaContext.mlmState);
	if (QDF_STATUS_SUCCESS != del_sta_params->status) {
		pe_warn("DEL STA failed!");
		status_code = eSIR_SME_REFUSED;
		goto end;
	}

	pe_debug("AP received the DEL_STA_RSP for assocID: %X",
		 del_sta_params->assocId);
	if ((eLIM_MLM_WT_DEL_STA_RSP_STATE != sta_ds->mlmStaContext.mlmState) &&
	    (eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE !=
	     sta_ds->mlmStaContext.mlmState)) {
		pe_err("Received unexpected WMA_DEL_STA_RSP in state %s for staId %d assocId %d",
			lim_mlm_state_str(sta_ds->mlmStaContext.mlmState),
			sta_ds->staIndex, sta_ds->assocId);
		status_code = eSIR_SME_REFUSED;
		goto end;
	}

	pe_debug("Deleted STA AssocID %d staId %d MAC",
		sta_ds->assocId, sta_ds->staIndex);
	lim_print_mac_addr(mac_ctx, sta_ds->staAddr, LOGD);
	if (eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE ==
	    sta_ds->mlmStaContext.mlmState) {
		qdf_mem_free(del_sta_params);
		msg->bodyptr = NULL;
		if (lim_add_sta(mac_ctx, sta_ds, false, session_entry) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not Add STA with assocId: %d",
				sta_ds->assocId);
			/*
			 * delete the TS if it has already been added.
			 * send the response with error status.
			 */
			if (sta_ds->qos.addtsPresent) {
				tpLimTspecInfo pTspecInfo;

				if (QDF_STATUS_SUCCESS ==
				    lim_tspec_find_by_assoc_id(mac_ctx,
					sta_ds->assocId,
					&sta_ds->qos.addts.tspec,
					&mac_ctx->lim.tspecInfo[0],
					&pTspecInfo)) {
					lim_admit_control_delete_ts(mac_ctx,
						sta_ds->assocId,
						&sta_ds->qos.addts.tspec.tsinfo,
						NULL,
						&pTspecInfo->idx);
				}
			}
			lim_reject_association(mac_ctx, sta_ds->staAddr,
				sta_ds->mlmStaContext.subType, true,
				sta_ds->mlmStaContext.authType, sta_ds->assocId,
				true,
				eSIR_MAC_UNSPEC_FAILURE_STATUS,
				session_entry);
		}
		return;
	}
end:
	qdf_mem_free(del_sta_params);
	msg->bodyptr = NULL;
	if (eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE !=
	    sta_ds->mlmStaContext.mlmState) {
		lim_prepare_and_send_del_sta_cnf(mac_ctx, sta_ds, status_code,
						 session_entry);
	}
	return;
}

void lim_process_sta_mlm_del_sta_rsp(tpAniSirGlobal pMac,
				     struct scheduler_msg *limMsgQ,
				     tpPESession psessionEntry)
{
	tSirResultCodes statusCode = eSIR_SME_SUCCESS;
	tpDeleteStaParams pDelStaParams = (tpDeleteStaParams) limMsgQ->bodyptr;
	tpDphHashNode pStaDs = NULL;

	if (NULL == pDelStaParams) {
		pe_err("Encountered NULL Pointer");
		goto end;
	}
	pe_debug("Del STA RSP received. Status: %d AssocID: %d",
			pDelStaParams->status, pDelStaParams->assocId);

#ifdef FEATURE_WLAN_TDLS
	if (pDelStaParams->staType == STA_ENTRY_TDLS_PEER) {
		pe_debug("TDLS Del STA RSP received");
		lim_process_tdls_del_sta_rsp(pMac, limMsgQ, psessionEntry);
		return;
	}
#endif
	if (QDF_STATUS_SUCCESS != pDelStaParams->status)
		pe_err("Del STA failed! Status:%d, proceeding with Del BSS",
			pDelStaParams->status);

	pStaDs = dph_get_hash_entry(pMac, DPH_STA_HASH_INDEX_PEER,
			&psessionEntry->dph.dphHashTable);
	if (pStaDs == NULL) {
		pe_err("DPH Entry for STA %X missing",
				pDelStaParams->assocId);
		statusCode = eSIR_SME_REFUSED;
		goto end;
	}
	if (eLIM_MLM_WT_DEL_STA_RSP_STATE != psessionEntry->limMlmState) {
		pe_err("Received unexpected WDA_DELETE_STA_RSP in state %s",
			lim_mlm_state_str(psessionEntry->limMlmState));
		statusCode = eSIR_SME_REFUSED;
		goto end;
	}
	pe_debug("STA AssocID %d MAC", pStaDs->assocId);
	lim_print_mac_addr(pMac, pStaDs->staAddr, LOGD);
	/*
	 * we must complete all cleanup related to delSta before
	 * calling limDelBSS.
	 */
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pDelStaParams);
		limMsgQ->bodyptr = NULL;
	}
	/* Proceed to do DelBSS even if DelSta resulted in failure */
	statusCode = (tSirResultCodes)lim_del_bss(pMac, pStaDs, 0,
			psessionEntry);
	return;
end:
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pDelStaParams);
		limMsgQ->bodyptr = NULL;
	}
	return;
}

void lim_process_ap_mlm_add_sta_rsp(tpAniSirGlobal pMac,
				    struct scheduler_msg *limMsgQ,
				    tpPESession psessionEntry)
{
	tpAddStaParams pAddStaParams = (tpAddStaParams) limMsgQ->bodyptr;
	tpDphHashNode pStaDs = NULL;

	if (NULL == pAddStaParams) {
		pe_err("Invalid body pointer in message");
		goto end;
	}

	pStaDs =
		dph_get_hash_entry(pMac, pAddStaParams->assocId,
				   &psessionEntry->dph.dphHashTable);
	if (pStaDs == NULL) {
		pe_err("DPH Entry for STA %X missing", pAddStaParams->assocId);
		goto end;
	}

	if (eLIM_MLM_WT_ADD_STA_RSP_STATE != pStaDs->mlmStaContext.mlmState) {
		pe_err("Received unexpected WMA_ADD_STA_RSP in state %X",
			pStaDs->mlmStaContext.mlmState);
		goto end;
	}
	if (QDF_STATUS_SUCCESS != pAddStaParams->status) {
		pe_err("Error! rcvd delSta rsp from HAL with status %d",
			       pAddStaParams->status);
		lim_reject_association(pMac, pStaDs->staAddr,
				       pStaDs->mlmStaContext.subType,
				       true, pStaDs->mlmStaContext.authType,
				       pStaDs->assocId, true,
				       eSIR_MAC_UNSPEC_FAILURE_STATUS,
				       psessionEntry);
		goto end;
	}
	pStaDs->bssId = pAddStaParams->bssIdx;
	pStaDs->staIndex = pAddStaParams->staIdx;
	pStaDs->nss = pAddStaParams->nss;
	/* if the AssocRsp frame is not acknowledged, then keep alive timer will take care of the state */
	pStaDs->valid = 1;
	pStaDs->mlmStaContext.mlmState = eLIM_MLM_WT_ASSOC_CNF_STATE;
	pe_debug("AddStaRsp Success.STA AssocID %d staId %d MAC",
		pStaDs->assocId, pStaDs->staIndex);
	lim_print_mac_addr(pMac, pStaDs->staAddr, LOGD);

	/* For BTAMP-AP, the flow sequence shall be:
	 * 1) PE sends eWNI_SME_ASSOC_IND to SME
	 * 2) PE receives eWNI_SME_ASSOC_CNF from SME
	 * 3) BTAMP-AP sends Re/Association Response to BTAMP-STA
	 */
	if (lim_send_mlm_assoc_ind(pMac, pStaDs, psessionEntry) !=
							QDF_STATUS_SUCCESS) {
		lim_reject_association(pMac, pStaDs->staAddr,
				       pStaDs->mlmStaContext.subType,
				       true, pStaDs->mlmStaContext.authType,
				       pStaDs->assocId, true,
				       eSIR_MAC_UNSPEC_FAILURE_STATUS,
				       psessionEntry);
	}
	/* fall though to reclaim the original Add STA Response message */
end:
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pAddStaParams);
		limMsgQ->bodyptr = NULL;
	}
	return;
}

/**
 * lim_process_ap_mlm_add_bss_rsp()
 *
 ***FUNCTION:
 * This function is called to process a WMA_ADD_BSS_RSP from HAL.
 * Upon receipt of this message from HAL, MLME -
 * > Validates the result of WMA_ADD_BSS_REQ
 * > Init other remaining LIM variables
 * > Init the AID pool, for that BSSID
 * > Init the Pre-AUTH list, for that BSSID
 * > Create LIM timers, specific to that BSSID
 * > Init DPH related parameters that are specific to that BSSID
 * > TODO - When do we do the actual change channel?
 *
 ***LOGIC:
 * SME sends eWNI_SME_START_BSS_REQ to LIM
 * LIM sends LIM_MLM_START_REQ to MLME
 * MLME sends WMA_ADD_BSS_REQ to HAL
 * HAL responds with WMA_ADD_BSS_RSP to MLME
 * MLME responds with LIM_MLM_START_CNF to LIM
 * LIM responds with eWNI_SME_START_BSS_RSP to SME
 *
 ***ASSUMPTIONS:
 * struct scheduler_msg.body is allocated by MLME during
 * lim_process_mlm_start_req
 * struct scheduler_msg.body will now be freed by this routine
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  struct scheduler_msg  The MsgQ header, which contains
 *  the response buffer
 *
 * @return None
 */
static void lim_process_ap_mlm_add_bss_rsp(tpAniSirGlobal pMac,
					   struct scheduler_msg *limMsgQ)
{
	tLimMlmStartCnf mlmStartCnf;
	tpPESession psessionEntry;
	uint8_t isWepEnabled = false;
	tpAddBssParams pAddBssParams = (tpAddBssParams) limMsgQ->bodyptr;

	if (NULL == pAddBssParams) {
		pe_err("Encountered NULL Pointer");
		goto end;
	}
	/* TBD: free the memory before returning, do it for all places where lookup fails. */
	psessionEntry = pe_find_session_by_session_id(pMac,
					   pAddBssParams->sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given sessionId");
		if (NULL != pAddBssParams) {
			qdf_mem_free(pAddBssParams);
			limMsgQ->bodyptr = NULL;
		}
		return;
	}
	/* Update PE session Id */
	mlmStartCnf.sessionId = pAddBssParams->sessionId;
	if (QDF_STATUS_SUCCESS == pAddBssParams->status) {
		pe_debug("WMA_ADD_BSS_RSP returned with QDF_STATUS_SUCCESS");
		if (lim_set_link_state
			    (pMac, eSIR_LINK_AP_STATE, psessionEntry->bssId,
			    psessionEntry->selfMacAddr, NULL,
			    NULL) != QDF_STATUS_SUCCESS)
			goto end;
		/* Set MLME state */
		psessionEntry->limMlmState = eLIM_MLM_BSS_STARTED_STATE;
		psessionEntry->chainMask = pAddBssParams->chainMask;
		psessionEntry->smpsMode = pAddBssParams->smpsMode;
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId,
			       psessionEntry->limMlmState));
		if (eSIR_IBSS_MODE == pAddBssParams->bssType) {
			/** IBSS is 'active' when we receive
			 * Beacon frames from other STAs that are part of same IBSS.
			 * Mark internal state as inactive until then.
			 */
			psessionEntry->limIbssActive = false;
			psessionEntry->statypeForBss = STA_ENTRY_PEER; /* to know session created for self/peer */
			limResetHBPktCount(psessionEntry);
		}
		psessionEntry->bssIdx = (uint8_t) pAddBssParams->bssIdx;

		psessionEntry->limSystemRole = eLIM_STA_IN_IBSS_ROLE;

		if (eSIR_INFRA_AP_MODE == pAddBssParams->bssType)
			psessionEntry->limSystemRole = eLIM_AP_ROLE;
		else
			psessionEntry->limSystemRole = eLIM_STA_IN_IBSS_ROLE;
		sch_edca_profile_update(pMac, psessionEntry);
		lim_init_pre_auth_list(pMac);
		/* Check the SAP security configuration.If configured to
		 * WEP then max clients supported is 16
		 */
		if (psessionEntry->privacy) {
			if ((psessionEntry->gStartBssRSNIe.present)
			    || (psessionEntry->gStartBssWPAIe.present))
				pe_debug("WPA/WPA2 SAP configuration");
			else {
				if (pMac->lim.gLimAssocStaLimit >
				    MAX_SUPPORTED_PEERS_WEP) {
					pe_debug("WEP SAP Configuration");
					pMac->lim.gLimAssocStaLimit =
						MAX_SUPPORTED_PEERS_WEP;
					isWepEnabled = true;
				}
			}
		}
		lim_init_peer_idxpool(pMac, psessionEntry);

		/* Start OLBC timer */
		if (tx_timer_activate
			    (&pMac->lim.limTimers.gLimUpdateOlbcCacheTimer) !=
		    TX_SUCCESS) {
			pe_err("tx_timer_activate failed");
		}

		/* Apply previously set configuration at HW */
		lim_apply_configuration(pMac, psessionEntry);

		/* In lim_apply_configuration gLimAssocStaLimit is assigned from cfg.
		 * So update the value to 16 in case SoftAP is configured in WEP.
		 */
		if ((pMac->lim.gLimAssocStaLimit > MAX_SUPPORTED_PEERS_WEP)
		    && (isWepEnabled))
			pMac->lim.gLimAssocStaLimit = MAX_SUPPORTED_PEERS_WEP;
		psessionEntry->staId = pAddBssParams->staContext.staIdx;
		mlmStartCnf.resultCode = eSIR_SME_SUCCESS;
	} else {
		pe_err("WMA_ADD_BSS_REQ failed with status %d",
			pAddBssParams->status);
		mlmStartCnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
	}
	lim_post_sme_message(pMac, LIM_MLM_START_CNF, (uint32_t *) &mlmStartCnf);
end:
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pAddBssParams);
		limMsgQ->bodyptr = NULL;
	}
}

/**
 * lim_process_ibss_mlm_add_bss_rsp()
 *
 ***FUNCTION:
 * This function is called to process a WMA_ADD_BSS_RSP from HAL.
 * Upon receipt of this message from HAL, MLME -
 * > Validates the result of WMA_ADD_BSS_REQ
 * > Init other remaining LIM variables
 * > Init the AID pool, for that BSSID
 * > Init the Pre-AUTH list, for that BSSID
 * > Create LIM timers, specific to that BSSID
 * > Init DPH related parameters that are specific to that BSSID
 * > TODO - When do we do the actual change channel?
 *
 ***LOGIC:
 * SME sends eWNI_SME_START_BSS_REQ to LIM
 * LIM sends LIM_MLM_START_REQ to MLME
 * MLME sends WMA_ADD_BSS_REQ to HAL
 * HAL responds with WMA_ADD_BSS_RSP to MLME
 * MLME responds with LIM_MLM_START_CNF to LIM
 * LIM responds with eWNI_SME_START_BSS_RSP to SME
 *
 ***ASSUMPTIONS:
 * struct scheduler_msg.body is allocated by MLME during
 * lim_process_mlm_start_req
 * struct scheduler_msg.body will now be freed by this routine
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  struct scheduler_msg  The MsgQ header, which contains
 *  the response buffer
 *
 * @return None
 */
static void
lim_process_ibss_mlm_add_bss_rsp(tpAniSirGlobal pMac,
				 struct scheduler_msg *limMsgQ,
				 tpPESession psessionEntry)
{
	tLimMlmStartCnf mlmStartCnf;
	tpAddBssParams pAddBssParams = (tpAddBssParams) limMsgQ->bodyptr;

	if (NULL == pAddBssParams) {
		pe_err("Invalid body pointer in message");
		goto end;
	}
	if (QDF_STATUS_SUCCESS == pAddBssParams->status) {
		pe_debug("WMA_ADD_BSS_RSP returned with QDF_STATUS_SUCCESS");

		if (lim_set_link_state
			    (pMac, eSIR_LINK_IBSS_STATE, psessionEntry->bssId,
			    psessionEntry->selfMacAddr, NULL,
			    NULL) != QDF_STATUS_SUCCESS)
			goto end;
		/* Set MLME state */
		psessionEntry->limMlmState = eLIM_MLM_BSS_STARTED_STATE;
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId,
			       psessionEntry->limMlmState));
		/** IBSS is 'active' when we receive
		 * Beacon frames from other STAs that are part of same IBSS.
		 * Mark internal state as inactive until then.
		 */
		psessionEntry->limIbssActive = false;
		limResetHBPktCount(psessionEntry);
		psessionEntry->bssIdx = (uint8_t) pAddBssParams->bssIdx;
		psessionEntry->limSystemRole = eLIM_STA_IN_IBSS_ROLE;
		psessionEntry->statypeForBss = STA_ENTRY_SELF;
		sch_edca_profile_update(pMac, psessionEntry);
		if (0 == psessionEntry->freePeerIdxHead)
			lim_init_peer_idxpool(pMac, psessionEntry);

		/* Apply previously set configuration at HW */
		lim_apply_configuration(pMac, psessionEntry);
		psessionEntry->staId = pAddBssParams->staContext.staIdx;
		mlmStartCnf.resultCode = eSIR_SME_SUCCESS;
		/* If ADD BSS was issued as part of IBSS coalescing, don't send the message to SME, as that is internal to LIM */
		if (true == pMac->lim.gLimIbssCoalescingHappened) {
			lim_ibss_add_bss_rsp_when_coalescing(pMac, limMsgQ->bodyptr,
							     psessionEntry);
			goto end;
		}
	} else {
		pe_err("WMA_ADD_BSS_REQ failed with status %d",
			pAddBssParams->status);
		mlmStartCnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
	}
	/* Send this message to SME, when ADD_BSS is initiated by SME */
	/* If ADD_BSS is done as part of coalescing, this won't happen. */
	/* Update PE session Id */
	mlmStartCnf.sessionId = psessionEntry->peSessionId;
	lim_post_sme_message(pMac, LIM_MLM_START_CNF, (uint32_t *) &mlmStartCnf);
end:
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pAddBssParams);
		limMsgQ->bodyptr = NULL;
	}
}

#ifdef WLAN_FEATURE_FILS_SK
/*
 * lim_update_fils_auth_mode: API to update Auth mode in case of fils session
 * @session_entry: pe session entry
 * @auth_mode: auth mode needs to be updated
 *
 * Return: None
 */
static void lim_update_fils_auth_mode(tpPESession session_entry,
			tAniAuthType *auth_mode)
{
	if (!session_entry->fils_info)
		return;

	if (session_entry->fils_info->is_fils_connection)
		*auth_mode = session_entry->fils_info->auth;
}
#else
static void lim_update_fils_auth_mode(tpPESession session_entry,
			tAniAuthType *auth_mode)
{ }
#endif

/**
 * csr_neighbor_roam_handoff_req_hdlr - Processes handoff request
 * @mac_ctx:  Pointer to mac context
 * @msg:  message sent to HDD
 * @session_entry: PE session handle
 *
 * This function is called to process a WMA_ADD_BSS_RSP from HAL.
 * Upon receipt of this message from HAL if the state is pre assoc.
 *
 * Return: Null
 */
static void
lim_process_sta_add_bss_rsp_pre_assoc(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg, tpPESession session_entry)
{
	tpAddBssParams pAddBssParams = (tpAddBssParams) msg->bodyptr;
	tAniAuthType cfgAuthType, authMode;
	tLimMlmAuthReq *pMlmAuthReq;
	tpDphHashNode pStaDs = NULL;

	if (NULL == pAddBssParams) {
		pe_err("Invalid body pointer in message");
		goto joinFailure;
	}
	if (QDF_STATUS_SUCCESS == pAddBssParams->status) {
		pStaDs = dph_add_hash_entry(mac_ctx,
				pAddBssParams->staContext.staMac,
				DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
		if (pStaDs == NULL) {
			/* Could not add hash table entry */
			pe_err("could not add hash entry at DPH for");
			lim_print_mac_addr(mac_ctx,
				pAddBssParams->staContext.staMac, LOGE);
			goto joinFailure;
		}
		session_entry->bssIdx = (uint8_t) pAddBssParams->bssIdx;
		/* Success, handle below */
		pStaDs->bssId = pAddBssParams->bssIdx;
		/* STA Index(genr by HAL) for the BSS entry is stored here */
		pStaDs->staIndex = pAddBssParams->staContext.staIdx;
		/* Trigger Authentication with AP */
		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_AUTHENTICATION_TYPE,
			(uint32_t *) &cfgAuthType) != QDF_STATUS_SUCCESS) {
			pe_warn("could not retrieve AuthType");
		}
		/* Try shared Authentication first */
		if (cfgAuthType == eSIR_AUTO_SWITCH)
			authMode = eSIR_SHARED_KEY;
		else
			authMode = cfgAuthType;

		lim_update_fils_auth_mode(session_entry, &authMode);
		/* Trigger MAC based Authentication */
		pMlmAuthReq = qdf_mem_malloc(sizeof(tLimMlmAuthReq));
		if (NULL == pMlmAuthReq) {
			pe_err("Allocate Memory failed for mlmAuthReq");
			return;
		}
		sir_copy_mac_addr(pMlmAuthReq->peerMacAddr,
			session_entry->bssId);

		pMlmAuthReq->authType = authMode;
		if (wlan_cfg_get_int(mac_ctx,
			WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT,
			(uint32_t *) &pMlmAuthReq->authFailureTimeout)
			!= QDF_STATUS_SUCCESS) {
			pe_warn("Fail: retrieve AuthFailureTimeout value");
		}
		session_entry->limMlmState = eLIM_MLM_JOINED_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			session_entry->peSessionId, eLIM_MLM_JOINED_STATE));
		pMlmAuthReq->sessionId = session_entry->peSessionId;
		session_entry->limPrevSmeState = session_entry->limSmeState;
		session_entry->limSmeState = eLIM_SME_WT_AUTH_STATE;
		/* remember staId in case of assoc timeout/failure handling */
		session_entry->staId = pAddBssParams->staContext.staIdx;

		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId,
			session_entry->limSmeState));
		lim_post_mlm_message(mac_ctx, LIM_MLM_AUTH_REQ,
			(uint32_t *) pMlmAuthReq);
		return;
	}

joinFailure:
	{
		session_entry->limSmeState = eLIM_SME_JOIN_FAILURE_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId,
			session_entry->limSmeState));

		/* Send Join response to Host */
		lim_handle_sme_join_result(mac_ctx, eSIR_SME_REFUSED,
			eSIR_MAC_UNSPEC_FAILURE_STATUS, session_entry);
	}

}

/**
 * lim_process_sta_mlm_add_bss_rsp() - Process ADD BSS response
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: The MsgQ header, which contains the response buffer
 *
 * This function is called to process a WMA_ADD_BSS_RSP from HAL.
 * Upon receipt of this message from HAL, MLME -
 * > Validates the result of WMA_ADD_BSS_REQ
 * > Now, send an ADD_STA to HAL and ADD the "local" STA itself
 *
 * MLME had sent WMA_ADD_BSS_REQ to HAL
 * HAL responded with WMA_ADD_BSS_RSP to MLME
 * MLME now sends WMA_ADD_STA_REQ to HAL
 * ASSUMPTIONS:
 * struct scheduler_msg.body is allocated by MLME during
 * lim_process_mlm_join_req
 * struct scheduler_msg.body will now be freed by this routine
 *
 * Return: None
 */
static void
lim_process_sta_mlm_add_bss_rsp(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg, tpPESession session_entry)
{
	tpAddBssParams add_bss_params = (tpAddBssParams) msg->bodyptr;
	tLimMlmAssocCnf mlm_assoc_cnf;
	uint32_t msg_type = LIM_MLM_ASSOC_CNF;
	uint32_t sub_type = LIM_ASSOC;
	tpDphHashNode sta_ds = NULL;
	uint16_t sta_idx = STA_INVALID_IDX;
	uint8_t update_sta = false;

	mlm_assoc_cnf.resultCode = eSIR_SME_SUCCESS;

	if (eLIM_MLM_WT_ADD_BSS_RSP_PREASSOC_STATE ==
		session_entry->limMlmState) {
		pe_debug("SessionId: %d lim_process_sta_add_bss_rsp_pre_assoc",
			session_entry->peSessionId);
		lim_process_sta_add_bss_rsp_pre_assoc(mac_ctx, msg,
			session_entry);
		goto end;
	}
	if (eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE == session_entry->limMlmState
		|| (eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE ==
		session_entry->limMlmState)) {
		msg_type = LIM_MLM_REASSOC_CNF;
		sub_type = LIM_REASSOC;
		/*
		 * If Reassoc is happening for the same BSS, then
		 * use the existing StaId and indicate to HAL to update
		 * the existing STA entry.
		 * If Reassoc is happening for the new BSS, then
		 * old BSS and STA entry would have been already deleted
		 * before PE tries to add BSS for the new BSS, so set the
		 * updateSta to false and pass INVALID STA Index.
		 */
		if (sir_compare_mac_addr(session_entry->bssId,
			session_entry->limReAssocbssId)) {
			sta_idx = session_entry->staId;
			update_sta = true;
		}
	}

	if (add_bss_params == 0)
		goto end;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (session_entry->bRoamSynchInProgress)
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			"LFR3:lim_process_sta_mlm_add_bss_rsp");
#endif

	if (QDF_STATUS_SUCCESS == add_bss_params->status) {
		if (eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE ==
			session_entry->limMlmState) {
			pe_debug("Mlm=%d %d", session_entry->limMlmState,
				eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE);
			lim_process_sta_mlm_add_bss_rsp_ft(mac_ctx, msg,
				session_entry);
			goto end;
		}

		/* Set MLME state */
		session_entry->limMlmState = eLIM_MLM_WT_ADD_STA_RSP_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			session_entry->peSessionId,
			session_entry->limMlmState));
		/* to know the session  started for self or for  peer  */
		session_entry->statypeForBss = STA_ENTRY_PEER;
		sta_ds =
			dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
		if (sta_ds == NULL) {
			pe_err("Session:%d Fail to add Self Entry for STA",
				session_entry->peSessionId);
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
		} else {
			session_entry->bssIdx =
				(uint8_t) add_bss_params->bssIdx;
			/* Success, handle below */
			sta_ds->bssId = add_bss_params->bssIdx;
			/*
			 * STA Index(genr by HAL) for the BSS
			 * entry is stored here
			*/
			sta_ds->staIndex = add_bss_params->staContext.staIdx;
			/* Downgrade the EDCA parameters if needed */
			lim_set_active_edca_params(mac_ctx,
				session_entry->gLimEdcaParams, session_entry);
			lim_send_edca_params(mac_ctx,
				session_entry->gLimEdcaParamsActive,
				sta_ds->bssId, false);
			rrm_cache_mgmt_tx_power(mac_ctx,
				add_bss_params->txMgmtPower, session_entry);
			if (lim_add_sta_self(mac_ctx, sta_idx, update_sta,
				session_entry) != QDF_STATUS_SUCCESS) {
				/* Add STA context at HW */
				pe_err("Session:%d could not Add Self"
					"Entry for the station",
					session_entry->peSessionId);
				mlm_assoc_cnf.resultCode =
					(tSirResultCodes) eSIR_SME_REFUSED;
			}
		}
	} else {
		pe_err("SessionId: %d ADD_BSS failed!",
			session_entry->peSessionId);
		mlm_assoc_cnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
		/* Return Assoc confirm to SME with failure */
		if (eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE ==
				session_entry->limMlmState)
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_FT_REASSOC_FAILURE;
		else
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
		session_entry->add_bss_failed = true;
	}

	if (mlm_assoc_cnf.resultCode != eSIR_SME_SUCCESS) {
		session_entry->limMlmState = eLIM_MLM_IDLE_STATE;
		if (lim_set_link_state(mac_ctx, eSIR_LINK_IDLE_STATE,
					session_entry->bssId,
					session_entry->selfMacAddr,
					NULL, NULL) != QDF_STATUS_SUCCESS)
			pe_err("Failed to set the LinkState");
		/* Update PE session Id */
		mlm_assoc_cnf.sessionId = session_entry->peSessionId;
		lim_post_sme_message(mac_ctx, msg_type,
			(uint32_t *) &mlm_assoc_cnf);
	}
end:
	if (0 != msg->bodyptr) {
		qdf_mem_free(add_bss_params);
		msg->bodyptr = NULL;
	}
}

/**
 * lim_process_mlm_add_bss_rsp() - Processes ADD BSS Response
 *
 * @mac_ctx - Pointer to Global MAC structure
 * @msg - The MsgQ header, which contains the response buffer
 *
 * This function is called to process a WMA_ADD_BSS_RSP from HAL.
 * Upon receipt of this message from HAL, MLME -
 *  Determines the "state" in which this message was received
 *  Forwards it to the appropriate callback
 *
 *LOGIC:
 * WMA_ADD_BSS_RSP can be received by MLME while the LIM is
 * in the following two states:
 * 1) As AP, LIM state = eLIM_SME_WT_START_BSS_STATE
 * 2) As STA, LIM state = eLIM_SME_WT_JOIN_STATE
 * Based on these two states, this API will determine where to
 * route the message to
 *
 * Return None
 */
void lim_process_mlm_add_bss_rsp(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg)
{
	tLimMlmStartCnf mlm_start_cnf;
	tpPESession session_entry;
	tpAddBssParams add_bss_param = (tpAddBssParams) (msg->bodyptr);
	tSirBssType bss_type;

	if (NULL == add_bss_param) {
		pe_err("Encountered NULL Pointer");
		return;
	}

	/*
	 * we need to process the deferred message since the
	 * initiating req.there might be nested request.
	 * in the case of nested request the new request initiated
	 * from the response will take care of resetting the deffered
	 * flag.
	 */
	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);
	/* Validate SME/LIM/MLME state */
	session_entry = pe_find_session_by_session_id(mac_ctx,
			add_bss_param->sessionId);
	if (session_entry == NULL) {
		pe_err("SessionId:%d Session Doesn't exist",
			add_bss_param->sessionId);
		if (NULL != add_bss_param) {
			qdf_mem_free(add_bss_param);
			msg->bodyptr = NULL;
		}
		return;
	}

	bss_type = session_entry->bssType;
	/* update PE session Id */
	mlm_start_cnf.sessionId = session_entry->peSessionId;
	if (eSIR_IBSS_MODE == bss_type) {
		lim_process_ibss_mlm_add_bss_rsp(mac_ctx, msg, session_entry);
	} else if (eSIR_NDI_MODE == session_entry->bssType) {
		lim_process_ndi_mlm_add_bss_rsp(mac_ctx, msg, session_entry);
	} else {
		if (eLIM_SME_WT_START_BSS_STATE == session_entry->limSmeState) {
			if (eLIM_MLM_WT_ADD_BSS_RSP_STATE !=
				session_entry->limMlmState) {
				pe_err("SessionId:%d Received "
					" WMA_ADD_BSS_RSP in state %X",
					session_entry->peSessionId,
					session_entry->limMlmState);
				mlm_start_cnf.resultCode =
					eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
				if (0 != msg->bodyptr) {
					qdf_mem_free(add_bss_param);
					msg->bodyptr = NULL;
				}
				lim_post_sme_message(mac_ctx, LIM_MLM_START_CNF,
					(uint32_t *) &mlm_start_cnf);
			} else
				lim_process_ap_mlm_add_bss_rsp(mac_ctx, msg);
		} else {
			/* Called while processing assoc response */
			lim_process_sta_mlm_add_bss_rsp(mac_ctx, msg,
				session_entry);
		}
	}

#ifdef WLAN_FEATURE_11W
	if (session_entry->limRmfEnabled) {
		if (QDF_STATUS_SUCCESS !=
			lim_send_exclude_unencrypt_ind(mac_ctx, false,
				session_entry)) {
			pe_err("Failed to send Exclude Unencrypted Ind");
		}
	}
#endif
}

void lim_process_mlm_update_hidden_ssid_rsp(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg)
{
	tpPESession session_entry;
	tpHalHiddenSsidVdevRestart hidden_ssid_vdev_restart;
	struct scheduler_msg message = {0};
	QDF_STATUS status;

	hidden_ssid_vdev_restart = (tpHalHiddenSsidVdevRestart)(msg->bodyptr);

	if (NULL == hidden_ssid_vdev_restart) {
		pe_err("NULL msg pointer");
		return;
	}

	session_entry = pe_find_session_by_session_id(mac_ctx,
			hidden_ssid_vdev_restart->pe_session_id);

	if (session_entry == NULL) {
		pe_err("SessionId:%d Session Doesn't exist",
			hidden_ssid_vdev_restart->pe_session_id);
		goto free_req;
	}
	/* Update beacon */
	sch_set_fixed_beacon_fields(mac_ctx, session_entry);
	lim_send_beacon_ind(mac_ctx, session_entry, REASON_DEFAULT);

	message.type = eWNI_SME_HIDDEN_SSID_RESTART_RSP;
	message.bodyval = hidden_ssid_vdev_restart->sessionId;
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &message);

	if (status != QDF_STATUS_SUCCESS)
		pe_err("Failed to post message %u", status);

free_req:
	if (NULL != hidden_ssid_vdev_restart) {
		qdf_mem_free(hidden_ssid_vdev_restart);
		msg->bodyptr = NULL;
	}
}

/**
 * lim_process_mlm_set_sta_key_rsp() - Process STA key response
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: The MsgQ header, which contains the response buffer
 *
 * This function is called to process the following two
 * messages from HAL:
 * 1) WMA_SET_BSSKEY_RSP
 * 2) WMA_SET_STAKEY_RSP
 * 3) WMA_SET_STA_BCASTKEY_RSP
 * Upon receipt of this message from HAL,
 * MLME -
 * > Determines the "state" in which this message was received
 * > Forwards it to the appropriate callback
 * LOGIC:
 * WMA_SET_BSSKEY_RSP/WMA_SET_STAKEY_RSP can be
 * received by MLME while in the following state:
 * MLME state = eLIM_MLM_WT_SET_BSS_KEY_STATE --OR--
 * MLME state = eLIM_MLM_WT_SET_STA_KEY_STATE --OR--
 * MLME state = eLIM_MLM_WT_SET_STA_BCASTKEY_STATE
 * Based on this state, this API will determine where to
 * route the message to
 * Assumption:
 * ONLY the MLME state is being taken into account for now.
 * This is because, it appears that the handling of the
 * SETKEYS REQ is handled symmetrically on both the AP & STA
 *
 * Return: None
 */
void lim_process_mlm_set_sta_key_rsp(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg)
{
	uint8_t resp_reqd = 1;
	struct sLimMlmSetKeysCnf mlm_set_key_cnf;
	uint8_t session_id = 0;
	uint8_t sme_session_id;
	tpPESession session_entry;
	uint16_t key_len;
	uint16_t result_status;

	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);
	qdf_mem_zero((void *)&mlm_set_key_cnf, sizeof(tLimMlmSetKeysCnf));
	if (NULL == msg->bodyptr) {
		pe_err("msg bodyptr is NULL");
		return;
	}
	session_id = ((tpSetStaKeyParams) msg->bodyptr)->sessionId;
	sme_session_id = ((tpSetBssKeyParams) msg->bodyptr)->smesessionId;
	session_entry = pe_find_session_by_session_id(mac_ctx, session_id);
	if (session_entry == NULL) {
		pe_err("session does not exist for given session_id");
		qdf_mem_zero(msg->bodyptr, sizeof(tSetStaKeyParams));
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		lim_send_sme_set_context_rsp(mac_ctx,
					     mlm_set_key_cnf.peer_macaddr,
					     0, eSIR_SME_INVALID_SESSION, NULL,
					     sme_session_id, 0);
		return;
	}
	if (eLIM_MLM_WT_SET_STA_KEY_STATE != session_entry->limMlmState) {
		pe_err("Received unexpected [Mesg Id - %d] in state %X",
			msg->type, session_entry->limMlmState);
		resp_reqd = 0;
	} else {
		mlm_set_key_cnf.resultCode =
			(uint16_t)(((tpSetStaKeyParams) msg->bodyptr)->status);
	}

	result_status = (uint16_t)(((tpSetStaKeyParams) msg->bodyptr)->status);
	key_len = ((tpSetStaKeyParams)msg->bodyptr)->key[0].keyLength;

	if (result_status == eSIR_SME_SUCCESS && key_len)
		mlm_set_key_cnf.key_len_nonzero = true;
	else
		mlm_set_key_cnf.key_len_nonzero = false;

	qdf_mem_zero(msg->bodyptr, sizeof(tSetStaKeyParams));
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;
	/* Restore MLME state */
	session_entry->limMlmState = session_entry->limPrevMlmState;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
		session_entry->peSessionId, session_entry->limMlmState));
	if (resp_reqd) {
		tpLimMlmSetKeysReq lpLimMlmSetKeysReq =
			(tpLimMlmSetKeysReq) mac_ctx->lim.gpLimMlmSetKeysReq;
		/* Prepare and Send LIM_MLM_SETKEYS_CNF */
		if (NULL != lpLimMlmSetKeysReq) {
			qdf_copy_macaddr(&mlm_set_key_cnf.peer_macaddr,
					 &lpLimMlmSetKeysReq->peer_macaddr);
			/*
			 * Free the buffer cached for the global
			 * mac_ctx->lim.gpLimMlmSetKeysReq
			 */
			qdf_mem_zero(mac_ctx->lim.gpLimMlmSetKeysReq,
				     sizeof(tLimMlmSetKeysReq));
			qdf_mem_free(mac_ctx->lim.gpLimMlmSetKeysReq);
			mac_ctx->lim.gpLimMlmSetKeysReq = NULL;
		}
		mlm_set_key_cnf.sessionId = session_id;
		lim_post_sme_message(mac_ctx, LIM_MLM_SETKEYS_CNF,
			(uint32_t *) &mlm_set_key_cnf);
	}
}

/**
 * lim_process_mlm_set_bss_key_rsp() - handles BSS key
 *
 * @mac_ctx: A pointer to Global MAC structure
 * @msg: Message from SME
 *
 * This function processes BSS key response and updates
 * PE status accordingly.
 *
 * Return: NULL
 */
void lim_process_mlm_set_bss_key_rsp(tpAniSirGlobal mac_ctx,
	struct scheduler_msg *msg)
{
	struct sLimMlmSetKeysCnf set_key_cnf;
	uint16_t result_status;
	uint8_t session_id = 0;
	uint8_t sme_session_id;
	tpPESession session_entry;
	tpLimMlmSetKeysReq set_key_req;
	uint16_t key_len;

	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);
	qdf_mem_zero((void *)&set_key_cnf, sizeof(tLimMlmSetKeysCnf));
	if (NULL == msg->bodyptr) {
		pe_err("msg bodyptr is null");
		return;
	}
	session_id = ((tpSetBssKeyParams) msg->bodyptr)->sessionId;
	sme_session_id = ((tpSetBssKeyParams) msg->bodyptr)->smesessionId;
	session_entry = pe_find_session_by_session_id(mac_ctx, session_id);
	if (session_entry == NULL) {
		pe_err("session does not exist for given sessionId [%d]",
			session_id);
		qdf_mem_zero(msg->bodyptr, sizeof(tSetBssKeyParams));
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		lim_send_sme_set_context_rsp(mac_ctx, set_key_cnf.peer_macaddr,
					     0, eSIR_SME_INVALID_SESSION, NULL,
					     sme_session_id, 0);
		return;
	}
	if (eLIM_MLM_WT_SET_BSS_KEY_STATE == session_entry->limMlmState) {
		result_status =
			(uint16_t)(((tpSetBssKeyParams)msg->bodyptr)->status);
		key_len = ((tpSetBssKeyParams)msg->bodyptr)->key[0].keyLength;
	} else {
		/*
		 * BCAST key also uses tpSetStaKeyParams.
		 * Done this way for readabilty.
		 */
		result_status =
			(uint16_t)(((tpSetStaKeyParams)msg->bodyptr)->status);
		key_len = ((tpSetStaKeyParams)msg->bodyptr)->key[0].keyLength;
	}

	if (result_status == eSIR_SME_SUCCESS && key_len)
		set_key_cnf.key_len_nonzero = true;
	else
		set_key_cnf.key_len_nonzero = false;

	/* Validate MLME state */
	if (eLIM_MLM_WT_SET_BSS_KEY_STATE != session_entry->limMlmState &&
		eLIM_MLM_WT_SET_STA_BCASTKEY_STATE !=
			session_entry->limMlmState) {
		pe_err("Received unexpected [Mesg Id - %d] in state %X",
			msg->type, session_entry->limMlmState);
	} else {
		set_key_cnf.resultCode = result_status;
	}
	qdf_mem_zero(msg->bodyptr, sizeof(tSetBssKeyParams));
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;
	/* Restore MLME state */
	session_entry->limMlmState = session_entry->limPrevMlmState;

	MTRACE(mac_trace
		(mac_ctx, TRACE_CODE_MLM_STATE, session_entry->peSessionId,
		session_entry->limMlmState));
	set_key_req =
		(tpLimMlmSetKeysReq) mac_ctx->lim.gpLimMlmSetKeysReq;
	set_key_cnf.sessionId = session_id;

	/* Prepare and Send LIM_MLM_SETKEYS_CNF */
	if (NULL != set_key_req) {
		qdf_copy_macaddr(&set_key_cnf.peer_macaddr,
				 &set_key_req->peer_macaddr);
		/*
		 * Free the buffer cached for the
		 * global mac_ctx->lim.gpLimMlmSetKeysReq
		 */
		qdf_mem_zero(mac_ctx->lim.gpLimMlmSetKeysReq,
			     sizeof(tLimMlmSetKeysReq));
		qdf_mem_free(mac_ctx->lim.gpLimMlmSetKeysReq);
		mac_ctx->lim.gpLimMlmSetKeysReq = NULL;
	}
	lim_post_sme_message(mac_ctx, LIM_MLM_SETKEYS_CNF,
		(uint32_t *) &set_key_cnf);
}

/**
 * lim_process_switch_channel_re_assoc_req()
 *
 ***FUNCTION:
 * This function is called to send the reassoc req mgmt frame after the
 * switchChannelRsp message is received from HAL.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  pMac          - Pointer to Global MAC structure.
 * @param  psessionEntry - session related information.
 * @param  status        - channel switch success/failure.
 *
 * @return None
 */
static void lim_process_switch_channel_re_assoc_req(tpAniSirGlobal pMac,
						    tpPESession psessionEntry,
						    QDF_STATUS status)
{
	tLimMlmReassocCnf mlmReassocCnf;
	tLimMlmReassocReq *pMlmReassocReq;

	pMlmReassocReq =
		(tLimMlmReassocReq *) (psessionEntry->pLimMlmReassocReq);
	if (pMlmReassocReq == NULL) {
		pe_err("pLimMlmReassocReq does not exist for given switchChanSession");
		mlmReassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("Change channel failed!!");
		mlmReassocCnf.resultCode = eSIR_SME_CHANNEL_SWITCH_FAIL;
		goto end;
	}
	/* / Start reassociation failure timer */
	MTRACE(mac_trace
		       (pMac, TRACE_CODE_TIMER_ACTIVATE, psessionEntry->peSessionId,
		       eLIM_REASSOC_FAIL_TIMER));
	if (tx_timer_activate(&pMac->lim.limTimers.gLimReassocFailureTimer)
	    != TX_SUCCESS) {
		pe_err("could not start Reassociation failure timer");
		/* Return Reassoc confirm with */
		/* Resources Unavailable */
		mlmReassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}
	/* / Prepare and send Reassociation request frame */
	lim_send_reassoc_req_mgmt_frame(pMac, pMlmReassocReq, psessionEntry);
	return;
end:
	/* Free up buffer allocated for reassocReq */
	if (pMlmReassocReq != NULL) {
		/* Update PE session Id */
		mlmReassocCnf.sessionId = pMlmReassocReq->sessionId;
		qdf_mem_free(pMlmReassocReq);
		psessionEntry->pLimMlmReassocReq = NULL;
	} else {
		mlmReassocCnf.sessionId = 0;
	}

	mlmReassocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
	/* Update PE sessio Id */
	mlmReassocCnf.sessionId = psessionEntry->peSessionId;

	lim_post_sme_message(pMac, LIM_MLM_REASSOC_CNF,
			     (uint32_t *) &mlmReassocCnf);
}


/**
 * lim_process_switch_channel_join_req() -Initiates probe request
 *
 * @mac_ctx - A pointer to Global MAC structure
 * @sessionEntry - session related information.
 * @status        - channel switch success/failure
 *
 * This function is called to send the probe req mgmt frame
 * after the switchChannelRsp message is received from HAL.
 *
 * Return None
 */
static void lim_process_switch_channel_join_req(
	tpAniSirGlobal mac_ctx, tpPESession session_entry,
	QDF_STATUS status)
{
	tSirMacSSid ssId;
	tLimMlmJoinCnf join_cnf;

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("Change channel failed!!");
		goto error;
	}

	if ((NULL == session_entry) || (NULL == session_entry->pLimMlmJoinReq)
		|| (NULL == session_entry->pLimJoinReq)) {
		pe_err("invalid pointer!!");
		goto error;
	}

	session_entry->limPrevMlmState = session_entry->limMlmState;
	session_entry->limMlmState = eLIM_MLM_WT_JOIN_BEACON_STATE;

	/* Apply previously set configuration at HW */
	lim_apply_configuration(mac_ctx, session_entry);

	/*
	* If send_deauth_before_con is enabled, Send Deauth first to AP if last
	* disconnection was caused by HB failure.
	*/
	if (mac_ctx->roam.configParam.send_deauth_before_con) {
		int apCount;

		for (apCount = 0; apCount < 2; apCount++) {

			if (!qdf_mem_cmp(session_entry->pLimMlmJoinReq->bssDescription.bssId,
				mac_ctx->lim.gLimHeartBeatApMac[apCount], sizeof(tSirMacAddr))) {

				pe_err("Index %d Sessionid: %d Send deauth on "
				"channel %d to BSSID: "MAC_ADDRESS_STR, apCount,
				session_entry->peSessionId, session_entry->currentOperChannel,
				MAC_ADDR_ARRAY(session_entry->pLimMlmJoinReq->bssDescription.
											bssId));

				lim_send_deauth_mgmt_frame(mac_ctx, eSIR_MAC_UNSPEC_FAILURE_REASON,
					session_entry->pLimMlmJoinReq->bssDescription.bssId,
					session_entry, false);

				qdf_mem_zero(mac_ctx->lim.gLimHeartBeatApMac[apCount],
					sizeof(tSirMacAddr));
				break;
			}
		}
	}

	/* Wait for Beacon to announce join success */
	qdf_mem_copy(ssId.ssId,
		session_entry->ssId.ssId, session_entry->ssId.length);
	ssId.length = session_entry->ssId.length;

	lim_deactivate_and_change_timer(mac_ctx,
		eLIM_PERIODIC_JOIN_PROBE_REQ_TIMER);

	/* assign appropriate sessionId to the timer object */
	mac_ctx->lim.limTimers.gLimPeriodicJoinProbeReqTimer.sessionId =
		session_entry->peSessionId;
	pe_debug("vdev %d Send Probe req on freq %d %.*s " QDF_MAC_ADDR_STR,
		 session_entry->smeSessionId,
		 session_entry->currentOperChannel, ssId.length, ssId.ssId,
		 QDF_MAC_ADDR_ARRAY(
		 session_entry->pLimMlmJoinReq->bssDescription.bssId));

	/*
	 * We need to wait for probe response, so start join
	 * timeout timer.This timer will be deactivated once
	 * we receive probe response.
	 */
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_TIMER_ACTIVATE,
		session_entry->peSessionId, eLIM_JOIN_FAIL_TIMER));
	if (tx_timer_activate(&mac_ctx->lim.limTimers.gLimJoinFailureTimer) !=
		TX_SUCCESS) {
		pe_err("couldn't activate Join failure timer");
		session_entry->limMlmState = session_entry->limPrevMlmState;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			 session_entry->peSessionId,
			 mac_ctx->lim.gLimMlmState));
		goto error;
	}
	/* include additional IE if there is */
	lim_send_probe_req_mgmt_frame(mac_ctx, &ssId,
		session_entry->pLimMlmJoinReq->bssDescription.bssId,
		session_entry->currentOperChannel, session_entry->selfMacAddr,
		session_entry->dot11mode,
		&session_entry->pLimJoinReq->addIEScan.length,
		session_entry->pLimJoinReq->addIEScan.addIEdata);

	if (session_entry->pePersona == QDF_P2P_CLIENT_MODE) {
		/* Activate Join Periodic Probe Req timer */
		if (tx_timer_activate
			(&mac_ctx->lim.limTimers.gLimPeriodicJoinProbeReqTimer)
			!= TX_SUCCESS) {
			pe_err("Periodic JoinReq timer activate failed");
			goto error;
		}
	}
	return;
error:
	if (NULL != session_entry) {
		if (session_entry->pLimMlmJoinReq) {
			qdf_mem_free(session_entry->pLimMlmJoinReq);
			session_entry->pLimMlmJoinReq = NULL;
		}
		if (session_entry->pLimJoinReq) {
			qdf_mem_free(session_entry->pLimJoinReq);
			session_entry->pLimJoinReq = NULL;
		}
		join_cnf.sessionId = session_entry->peSessionId;
	} else {
		join_cnf.sessionId = 0;
	}
	join_cnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
	join_cnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
	lim_post_sme_message(mac_ctx, LIM_MLM_JOIN_CNF, (uint32_t *)&join_cnf);
}

/**
 * lim_process_switch_channel_rsp()
 *
 ***FUNCTION:
 * This function is called to process switchChannelRsp message from HAL.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  body - message body.
 *
 * @return None
 */
void lim_process_switch_channel_rsp(tpAniSirGlobal pMac, void *body)
{
	tpSwitchChannelParams pChnlParams = NULL;
	QDF_STATUS status;
	uint16_t channelChangeReasonCode;
	uint8_t peSessionId;
	tpPESession psessionEntry;
	/* we need to process the deferred message since the initiating req. there might be nested request. */
	/* in the case of nested request the new request initiated from the response will take care of resetting */
	/* the deffered flag. */
	SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
	pChnlParams = (tpSwitchChannelParams) body;
	status = pChnlParams->status;
	peSessionId = pChnlParams->peSessionId;

	psessionEntry = pe_find_session_by_session_id(pMac, peSessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given sessionId");
		goto free;
	}
	psessionEntry->ch_switch_in_progress = false;
	/* HAL fills in the tx power used for mgmt frames in this field. */
	/* Store this value to use in TPC report IE. */
	rrm_cache_mgmt_tx_power(pMac, pChnlParams->txMgmtPower, psessionEntry);
	channelChangeReasonCode = psessionEntry->channelChangeReasonCode;
	/* initialize it back to invalid id */
	psessionEntry->chainMask = pChnlParams->chainMask;
	psessionEntry->smpsMode = pChnlParams->smpsMode;
	psessionEntry->channelChangeReasonCode = 0xBAD;
	switch (channelChangeReasonCode) {
	case LIM_SWITCH_CHANNEL_REASSOC:
		lim_process_switch_channel_re_assoc_req(pMac, psessionEntry, status);
		break;
	case LIM_SWITCH_CHANNEL_JOIN:
		lim_process_switch_channel_join_req(pMac, psessionEntry, status);
		break;

	case LIM_SWITCH_CHANNEL_OPERATION:
		/*
		 * The above code should also use the callback.
		 * mechanism below, there is scope for cleanup here.
		 * THat way all this response handler does is call the call back
		 * We can get rid of the reason code here.
		 */
		if (pMac->lim.gpchangeChannelCallback)
			pMac->lim.gpchangeChannelCallback(pMac, status,
							  pMac->lim.
							  gpchangeChannelData,
							  psessionEntry);
		/* If MCC upgrade/DBS downgrade happended during channel switch,
		 * the policy manager connection table needs to be updated.
		 */
		policy_mgr_update_connection_info(pMac->psoc,
			psessionEntry->smeSessionId);
		if (psessionEntry->pePersona == QDF_P2P_CLIENT_MODE) {
			pe_debug("Send p2p operating channel change conf action frame once first beacon is received on new channel");
			psessionEntry->send_p2p_conf_frame = true;
		}

		if (ucfg_pkt_capture_get_pktcap_mode())
			ucfg_pkt_capture_record_channel();
		break;
	case LIM_SWITCH_CHANNEL_SAP_DFS:
	{
		/* Note: This event code specific to SAP mode
		 * When SAP session issues channel change as performing
		 * DFS, we will come here. Other sessions, for e.g. P2P
		 * will have to define their own event code and channel
		 * switch handler. This is required since the SME may
		 * require completely different information for P2P unlike
		 * SAP.
		 */
		lim_send_sme_ap_channel_switch_resp(pMac, psessionEntry,
						pChnlParams);
		/* If MCC upgrade/DBS downgrade happended during channel switch,
		 * the policy manager connection table needs to be updated.
		 */
		policy_mgr_update_connection_info(pMac->psoc,
						psessionEntry->smeSessionId);
		policy_mgr_set_do_hw_mode_change_flag(pMac->psoc, true);
	}
	break;
	default:
		break;
	}
free:
	qdf_mem_free(body);
}

QDF_STATUS lim_send_beacon_ind(tpAniSirGlobal pMac, tpPESession psessionEntry,
			       enum sir_bcn_update_reason reason)
{
	tBeaconGenParams *pBeaconGenParams = NULL;
	struct scheduler_msg limMsg = {0};
	/** Allocate the Memory for Beacon Pre Message and for Stations in PoweSave*/
	if (!psessionEntry) {
		pe_err("Error:Unable to get the PESessionEntry");
		return QDF_STATUS_E_INVAL;
	}
	pBeaconGenParams = qdf_mem_malloc(sizeof(*pBeaconGenParams));
	if (!pBeaconGenParams) {
		pe_err("Unable to allocate memory during sending beaconPreMessage");
		return QDF_STATUS_E_NOMEM;
	}
	qdf_mem_copy((void *)pBeaconGenParams->bssId,
		     (void *)psessionEntry->bssId, QDF_MAC_ADDR_SIZE);
	limMsg.bodyptr = pBeaconGenParams;
	return sch_process_pre_beacon_ind(pMac, &limMsg, reason);
}

void lim_process_rx_channel_status_event(tpAniSirGlobal mac_ctx, void *buf)
{
	struct lim_channel_status *chan_status = buf;

	if (NULL == chan_status) {
		QDF_TRACE(QDF_MODULE_ID_PE,
			  QDF_TRACE_LEVEL_ERROR,
			  "%s: ACS evt report buf NULL", __func__);
		return;
	}

	if (mac_ctx->sap.acs_with_more_param)
		lim_add_channel_status_info(mac_ctx, chan_status,
					    chan_status->channel_id);
	else
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_WARN,
			  "%s: Error evt report", __func__);

	qdf_mem_free(buf);

	return;
}
