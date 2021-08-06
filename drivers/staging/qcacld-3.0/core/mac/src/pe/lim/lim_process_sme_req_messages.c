/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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
 * This file lim_process_sme_req_messages.cc contains the code
 * for processing SME request messages.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "cds_api.h"
#include "wni_api.h"
#include "wni_cfg.h"
#include "cfg_api.h"
#include "sir_api.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_sme_req_utils.h"
#include "lim_ibss_peer_mgmt.h"
#include "lim_admit_control.h"
#include "dph_hash_table.h"
#include "lim_send_messages.h"
#include "lim_api.h"
#include "wmm_apsd.h"
#include "sir_mac_prot_def.h"
#include "rrm_api.h"
#include "nan_datapath.h"

#include "sap_api.h"


#include <lim_ft.h>
#include "cds_regdomain.h"
#include <wlan_reg_services_api.h>
#include "lim_process_fils.h"
#include "wlan_utility.h"

/*
 * This overhead is time for sending NOA start to host in case of GO/sending
 * NULL data & receiving ACK in case of P2P Client and starting actual scanning
 * with init scan req/rsp plus in case of concurrency, taking care of sending
 * null data and receiving ACK to/from AP/Also SetChannel with calibration
 * is taking around 7ms .
 */
#define SCAN_MESSAGING_OVERHEAD             20  /* in msecs */
#define JOIN_NOA_DURATION                   2000        /* in msecs */
#define OEM_DATA_NOA_DURATION               60  /* in msecs */
#define DEFAULT_PASSIVE_MAX_CHANNEL_TIME    110 /* in msecs */

#define CONV_MS_TO_US 1024      /* conversion factor from ms to us */

#define BEACON_INTERVAL_THRESHOLD 50  /* in msecs */
#define STA_BURST_SCAN_DURATION 120   /* in msecs */

/* SME REQ processing function templates */
static bool __lim_process_sme_sys_ready_ind(tpAniSirGlobal, uint32_t *);
static bool __lim_process_sme_start_bss_req(tpAniSirGlobal,
					    struct scheduler_msg *pMsg);
static void __lim_process_sme_join_req(tpAniSirGlobal, uint32_t *);
static void __lim_process_sme_reassoc_req(tpAniSirGlobal, uint32_t *);
static void __lim_process_sme_disassoc_req(tpAniSirGlobal, uint32_t *);
static void __lim_process_sme_disassoc_cnf(tpAniSirGlobal, uint32_t *);
static void __lim_process_sme_deauth_req(tpAniSirGlobal, uint32_t *);
static void __lim_process_sme_set_context_req(tpAniSirGlobal, uint32_t *);
static bool __lim_process_sme_stop_bss_req(tpAniSirGlobal,
					   struct scheduler_msg *pMsg);
static void __lim_process_send_disassoc_frame(tpAniSirGlobal mac_ctx,
				uint32_t *msg_buf);
static void lim_process_sme_channel_change_request(tpAniSirGlobal pMac,
						   uint32_t *pMsg);
static void lim_process_sme_start_beacon_req(tpAniSirGlobal pMac, uint32_t *pMsg);
static void lim_process_sme_dfs_csa_ie_request(tpAniSirGlobal pMac, uint32_t *pMsg);
static void lim_process_nss_update_request(tpAniSirGlobal pMac, uint32_t *pMsg);
static void lim_process_set_ie_req(tpAniSirGlobal pMac, uint32_t *pMsg);

static void lim_start_bss_update_add_ie_buffer(tpAniSirGlobal pMac,
					       uint8_t **pDstData_buff,
					       uint16_t *pDstDataLen,
					       uint8_t *pSrcData_buff,
					       uint16_t srcDataLen);

static void lim_update_add_ie_buffer(tpAniSirGlobal pMac,
				     uint8_t **pDstData_buff,
				     uint16_t *pDstDataLen,
				     uint8_t *pSrcData_buff, uint16_t srcDataLen);
static bool lim_update_ibss_prop_add_ies(tpAniSirGlobal pMac,
					 uint8_t **pDstData_buff,
					 uint16_t *pDstDataLen,
					 tSirModifyIE *pModifyIE);
static void lim_process_modify_add_ies(tpAniSirGlobal pMac, uint32_t *pMsg);

static void lim_process_update_add_ies(tpAniSirGlobal pMac, uint32_t *pMsg);

static void lim_process_ext_change_channel(tpAniSirGlobal mac_ctx,
						uint32_t *msg);

/**
 * lim_process_set_hw_mode() - Send set HW mode command to WMA
 * @mac: Globacl MAC pointer
 * @msg: Message containing the hw mode index
 *
 * Send the set HW mode command to WMA
 *
 * Return: QDF_STATUS_SUCCESS if message posting is successful
 */
static QDF_STATUS lim_process_set_hw_mode(tpAniSirGlobal mac, uint32_t *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	struct policy_mgr_hw_mode *req_msg;
	uint32_t len;
	struct s_sir_set_hw_mode *buf;
	struct scheduler_msg resp_msg = {0};
	struct sir_set_hw_mode_resp *param;

	buf = (struct s_sir_set_hw_mode *) msg;
	if (!buf) {
		pe_err("Set HW mode param is NULL");
		status = QDF_STATUS_E_INVAL;
		/* To free the active command list */
		goto fail;
	}

	len = sizeof(*req_msg);

	req_msg = qdf_mem_malloc(len);
	if (!req_msg) {
		pe_err("qdf_mem_malloc failed");
		status = QDF_STATUS_E_NOMEM;
		/* Free the active command list
		 * Probably the malloc is going to fail there as well?!
		 */
		goto fail;
	}

	req_msg->hw_mode_index = buf->set_hw.hw_mode_index;
	req_msg->reason = buf->set_hw.reason;
	/* Other parameters are not needed for WMA */

	message.bodyptr = req_msg;
	message.type    = SIR_HAL_PDEV_SET_HW_MODE;

	pe_debug("Posting SIR_HAL_SOC_SET_HW_MOD to WMA");
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("scheduler_post_msg failed!(err=%d)",
			status);
		qdf_mem_free(req_msg);
		goto fail;
	}
	return status;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param) {
		pe_err("HW mode resp failed");
		return QDF_STATUS_E_FAILURE;
	}
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	param->cfgd_hw_mode_index = 0;
	param->num_vdev_mac_entries = 0;
	resp_msg.type = eWNI_SME_SET_HW_MODE_RESP;
	resp_msg.bodyptr = param;
	resp_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(mac, &resp_msg, ePROT);
	return status;
}

/**
 * lim_process_set_dual_mac_cfg_req() - Set dual mac config command to WMA
 * @mac: Global MAC pointer
 * @msg: Message containing the dual mac config parameter
 *
 * Send the set dual mac config command to WMA
 *
 * Return: QDF_STATUS_SUCCESS if message posting is successful
 */
static QDF_STATUS lim_process_set_dual_mac_cfg_req(tpAniSirGlobal mac,
		uint32_t *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	struct policy_mgr_dual_mac_config *req_msg;
	uint32_t len;
	struct sir_set_dual_mac_cfg *buf;
	struct scheduler_msg resp_msg = {0};
	struct sir_dual_mac_config_resp *param;

	buf = (struct sir_set_dual_mac_cfg *) msg;
	if (!buf) {
		pe_err("Set Dual mac config is NULL");
		status = QDF_STATUS_E_INVAL;
		/* To free the active command list */
		goto fail;
	}

	len = sizeof(*req_msg);

	req_msg = qdf_mem_malloc(len);
	if (!req_msg) {
		pe_err("failed to allocate memory");
		status = QDF_STATUS_E_NOMEM;
		/* Free the active command list
		 * Probably the malloc is going to fail there as well?!
		 */
		goto fail;
	}

	req_msg->scan_config = buf->set_dual_mac.scan_config;
	req_msg->fw_mode_config = buf->set_dual_mac.fw_mode_config;
	/* Other parameters are not needed for WMA */

	message.bodyptr = req_msg;
	message.type    = SIR_HAL_PDEV_DUAL_MAC_CFG_REQ;

	pe_debug("Post SIR_HAL_PDEV_DUAL_MAC_CFG_REQ to WMA: %x %x",
		req_msg->scan_config, req_msg->fw_mode_config);
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("scheduler_post_msg failed!(err=%d)",
				status);
		qdf_mem_free(req_msg);
		goto fail;
	}
	return status;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param) {
		pe_err("Dual mac config resp failed");
		return QDF_STATUS_E_FAILURE;
	}
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	resp_msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	resp_msg.bodyptr = param;
	resp_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(mac, &resp_msg, ePROT);
	return status;
}

/**
 * lim_process_set_antenna_mode_req() - Set antenna mode command
 * to WMA
 * @mac: Global MAC pointer
 * @msg: Message containing the antenna mode parameter
 *
 * Send the set antenna mode command to WMA
 *
 * Return: QDF_STATUS_SUCCESS if message posting is successful
 */
static QDF_STATUS lim_process_set_antenna_mode_req(tpAniSirGlobal mac,
		uint32_t *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg message = {0};
	struct sir_antenna_mode_param *req_msg;
	struct sir_set_antenna_mode *buf;
	struct scheduler_msg resp_msg = {0};
	struct sir_antenna_mode_resp *param;

	buf = (struct sir_set_antenna_mode *) msg;
	if (!buf) {
		pe_err("Set antenna mode is NULL");
		status = QDF_STATUS_E_INVAL;
		/* To free the active command list */
		goto fail;
	}

	req_msg = qdf_mem_malloc(sizeof(*req_msg));
	if (!req_msg) {
		pe_err("failed to allocate memory");
		status = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	req_msg->num_rx_chains = buf->set_antenna_mode.num_rx_chains;
	req_msg->num_tx_chains = buf->set_antenna_mode.num_tx_chains;

	message.bodyptr = req_msg;
	message.type    = SIR_HAL_SOC_ANTENNA_MODE_REQ;

	pe_debug("Post SIR_HAL_SOC_ANTENNA_MODE_REQ to WMA: %d %d",
		req_msg->num_rx_chains,
		req_msg->num_tx_chains);
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("scheduler_post_msg failed!(err=%d)",
				status);
		qdf_mem_free(req_msg);
		goto fail;
	}
	return status;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param) {
		pe_err("antenna mode resp failed");
		return QDF_STATUS_E_NOMEM;
	}
	param->status = SET_ANTENNA_MODE_STATUS_ECANCELED;
	resp_msg.type = eWNI_SME_SET_ANTENNA_MODE_RESP;
	resp_msg.bodyptr = param;
	resp_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(mac, &resp_msg, ePROT);
	return status;
}

/**
 * __lim_is_sme_assoc_cnf_valid()
 *
 ***FUNCTION:
 * This function is called by __lim_process_sme_assoc_cnf_new() upon
 * receiving SME_ASSOC_CNF.
 *
 ***LOGIC:
 * Message validity checks are performed in this function
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  pMeasReq  Pointer to Received ASSOC_CNF message
 * @return true      When received SME_ASSOC_CNF is formatted
 *                   correctly
 *         false     otherwise
 */

static inline uint8_t __lim_is_sme_assoc_cnf_valid(tpSirSmeAssocCnf pAssocCnf)
{
	if (qdf_is_macaddr_group(&pAssocCnf->peer_macaddr))
		return false;
	else
		return true;
} /*** end __lim_is_sme_assoc_cnf_valid() ***/

/**
 * __lim_get_sme_join_req_size_for_alloc()
 *
 ***FUNCTION:
 * This function is called in various places to get IE length
 * from tSirBssDescription structure
 * number being scanned.
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param     pBssDescr
 * @return    Total IE length
 */

static uint16_t __lim_get_sme_join_req_size_for_alloc(uint8_t *pBuf)
{
	uint16_t len = 0;

	if (!pBuf)
		return len;

	pBuf += sizeof(uint16_t);
	len = lim_get_u16(pBuf);
	return len;
}

/**
 * __lim_is_defered_msg_for_learn() - message handling in SME learn state
 * @pMac: Global MAC context
 * @pMsg: Pointer to message posted from SME to LIM.
 *
 * Has role only if 11h is enabled. Not used on STA side.
 * Defers the message if SME is in learn state and brings
 * the LIM back to normal mode.
 *
 * Return: true - If defered false - Otherwise
 */

static bool __lim_is_defered_msg_for_learn(tpAniSirGlobal pMac,
					   struct scheduler_msg *pMsg)
{
	if (lim_is_system_in_scan_state(pMac)) {
		if (lim_defer_msg(pMac, pMsg) != TX_SUCCESS) {
			pe_err("Could not defer Msg: %d", pMsg->type);
			return false;
		}
		pe_debug("Defer the message, in learn mode type: %d",
			pMsg->type);
		return true;
	}
	return false;
}

/**
 * __lim_is_defered_msg_for_radar() - Defers the message if radar is detected
 * @mac_ctx: Pointer to Global MAC structure
 * @message: Pointer to message posted from SME to LIM.
 *
 * Has role only if 11h is enabled. Not used on STA side.
 * Defers the message if radar is detected.
 *
 * Return: true, if defered otherwise return false.
 */
static bool
__lim_is_defered_msg_for_radar(tpAniSirGlobal mac_ctx,
			       struct scheduler_msg *message)
{
	/*
	 * fRadarDetCurOperChan will be set only if we
	 * detect radar in current operating channel and
	 * System Role == AP ROLE
	 *
	 * TODO: Need to take care radar detection.
	 *
	 * if (LIM_IS_RADAR_DETECTED(mac_ctx))
	 */
	if (0) {
		if (lim_defer_msg(mac_ctx, message) != TX_SUCCESS) {
			pe_err("Could not defer Msg: %d", message->type);
			return false;
		}
		pe_debug("Defer the message, in learn mode type: %d",
			message->type);
		return true;
	}
	return false;
}

/**
 * __lim_process_sme_sys_ready_ind () - Process ready indication from WMA
 * @pMac: Global MAC context
 * @pMsgBuf: Message from WMA
 *
 * handles the notification from HDD. PE just forwards this message to HAL.
 *
 * Return: true-Posting to HAL failed, so PE will consume the buffer.
 *         false-Posting to HAL successful, so HAL will consume the buffer.
 */

static bool __lim_process_sme_sys_ready_ind(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	struct scheduler_msg msg = {0};
	tSirSmeReadyReq *ready_req = (tSirSmeReadyReq *) pMsgBuf;

	msg.type = WMA_SYS_READY_IND;
	msg.reserved = 0;
	msg.bodyptr = pMsgBuf;
	msg.bodyval = 0;

	if (ANI_DRIVER_TYPE(pMac) != QDF_DRIVER_TYPE_MFG) {
		ready_req->pe_roam_synch_cb = pe_roam_synch_callback;
		ready_req->pe_disconnect_cb = pe_disconnect_callback;
		pe_register_mgmt_rx_frm_callback(pMac);
		pe_register_callbacks_with_wma(pMac, ready_req);
		pMac->lim.sme_msg_callback = ready_req->sme_msg_cb;
		pMac->lim.stop_roaming_callback = ready_req->stop_roaming_cb;
	}

	pe_debug("sending WMA_SYS_READY_IND msg to HAL");
	MTRACE(mac_trace_msg_tx(pMac, NO_SESSION, msg.type));

	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(pMac, &msg)) {
		pe_err("wma_post_ctrl_msg failed");
		return true;
	}
	return false;
}

#ifdef WLAN_BCN_RECV_FEATURE
/**
 * lim_register_bcn_report_send_cb() - Register bcn receive start
 * indication handler callback
 * @mac: Pointer to Global MAC structure
 * @msg: A pointer to the SME message buffer
 *
 * Once driver gets QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING vendor
 * command with attribute for start only. LIM layer register a sme
 * callback through this function.
 *
 * Return: None.
 */
static void lim_register_bcn_report_send_cb(struct sAniSirGlobal *mac,
					    struct scheduler_msg *msg)
{
	if (!msg) {
		pe_err("Invalid message");
		return;
	}

	mac->lim.sme_bcn_rcv_callback = msg->callback;
}
#else
static inline
void lim_register_bcn_report_send_cb(struct sAniSirGlobal *mac,
				     struct scheduler_msg *msg)
{
}
#endif
/**
 *lim_configure_ap_start_bss_session() - Configure the AP Start BSS in session.
 *@mac_ctx: Pointer to Global MAC structure
 *@session: A pointer to session entry
 *@sme_start_bss_req: Start BSS Request from upper layers.
 *
 * This function is used to configure the start bss parameters
 * in to the session.
 *
 * Return: None.
 */
static void
lim_configure_ap_start_bss_session(tpAniSirGlobal mac_ctx, tpPESession session,
			tpSirSmeStartBssReq sme_start_bss_req)
{
	session->limSystemRole = eLIM_AP_ROLE;
	session->privacy = sme_start_bss_req->privacy;
	session->fwdWPSPBCProbeReq = sme_start_bss_req->fwdWPSPBCProbeReq;
	session->authType = sme_start_bss_req->authType;
	/* Store the DTIM period */
	session->dtimPeriod = (uint8_t) sme_start_bss_req->dtimPeriod;
	/* Enable/disable UAPSD */
	session->apUapsdEnable = sme_start_bss_req->apUapsdEnable;
	if (session->pePersona == QDF_P2P_GO_MODE) {
		session->proxyProbeRspEn = 0;
	} else {
		/*
		 * To detect PBC overlap in SAP WPS mode,
		 * Host handles Probe Requests.
		 */
		if (SAP_WPS_DISABLED == sme_start_bss_req->wps_state)
			session->proxyProbeRspEn = 1;
		else
			session->proxyProbeRspEn = 0;
	}
	session->ssidHidden = sme_start_bss_req->ssidHidden;
	session->wps_state = sme_start_bss_req->wps_state;
	session->sap_dot11mc = sme_start_bss_req->sap_dot11mc;
	session->vendor_vht_sap =
			sme_start_bss_req->vendor_vht_sap;
	lim_get_short_slot_from_phy_mode(mac_ctx, session, session->gLimPhyMode,
		&session->shortSlotTimeSupported);
	session->isCoalesingInIBSSAllowed =
		sme_start_bss_req->isCoalesingInIBSSAllowed;

	session->beacon_tx_rate = sme_start_bss_req->beacon_tx_rate;

}

/**
 * __lim_handle_sme_start_bss_request() - process SME_START_BSS_REQ message
 *@mac_ctx: Pointer to Global MAC structure
 *@msg_buf: A pointer to the SME message buffer
 *
 * This function is called to process SME_START_BSS_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */
static void
__lim_handle_sme_start_bss_request(tpAniSirGlobal mac_ctx, uint32_t *msg_buf)
{
	uint16_t size;
	uint32_t val = 0;
	QDF_STATUS ret_status;
	tSirMacChanNum channel_number;
	tLimMlmStartReq *mlm_start_req = NULL;
	tpSirSmeStartBssReq sme_start_bss_req = NULL;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	/* Flag Used in case of IBSS to Auto generate BSSID. */
	uint32_t auto_gen_bssid = false;
	uint8_t session_id;
	tpPESession session = NULL;
	uint8_t sme_session_id = 0xFF;
	uint16_t sme_transaction_id = 0xFF;
	uint32_t chanwidth;
	struct vdev_type_nss *vdev_type_nss;
	QDF_STATUS cfg_get_wmi_dfs_master_param = QDF_STATUS_SUCCESS;

/* FEATURE_WLAN_DIAG_SUPPORT */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
	/*
	 * Since the session is not created yet, sending NULL.
	 * The response should have the correct state.
	 */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_START_BSS_REQ_EVENT,
			      NULL, 0, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	size = sizeof(tSirSmeStartBssReq);
	sme_start_bss_req = qdf_mem_malloc(size);
	if (NULL == sme_start_bss_req) {
		pe_err("Allocate Memory fail for LimStartBssReq");
		/* Send failure response to host */
		ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto free;
	}
	qdf_mem_copy(sme_start_bss_req, msg_buf, sizeof(tSirSmeStartBssReq));
	sme_session_id = sme_start_bss_req->sessionId;
	sme_transaction_id = sme_start_bss_req->transactionId;

	if ((mac_ctx->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) ||
	    (mac_ctx->lim.gLimSmeState == eLIM_SME_IDLE_STATE)) {
		if (!lim_is_sme_start_bss_req_valid(mac_ctx,
					sme_start_bss_req)) {
			pe_warn("Received invalid eWNI_SME_START_BSS_REQ");
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto free;
		}

		/*
		 * This is the place where PE is going to create a session.
		 * If session is not existed, then create a new session
		 */
		session = pe_find_session_by_bssid(mac_ctx,
				sme_start_bss_req->bssid.bytes, &session_id);
		if (session != NULL) {
			pe_warn("Session Already exists for given BSSID");
			ret_code = eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
			session = NULL;
			goto free;
		} else {
			session = pe_create_session(mac_ctx,
					sme_start_bss_req->bssid.bytes,
					&session_id, mac_ctx->lim.maxStation,
					sme_start_bss_req->bssType,
					sme_start_bss_req->sessionId,
					sme_start_bss_req->bssPersona);
			if (!session) {
				pe_warn("Session Can not be created");
				ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
				goto free;
			}

			/* Update the beacon/probe filter in mac_ctx */
			lim_set_bcn_probe_filter(mac_ctx, session,
						 &sme_start_bss_req->ssId,
						 sme_start_bss_req->channelId);
		}

		if (QDF_NDI_MODE != sme_start_bss_req->bssPersona) {
			/* Probe resp add ie */
			lim_start_bss_update_add_ie_buffer(mac_ctx,
				&session->addIeParams.probeRespData_buff,
				&session->addIeParams.probeRespDataLen,
				sme_start_bss_req->addIeParams.
					probeRespData_buff,
				sme_start_bss_req->addIeParams.
					probeRespDataLen);

			/* Probe Beacon add ie */
			lim_start_bss_update_add_ie_buffer(mac_ctx,
				&session->addIeParams.probeRespBCNData_buff,
				&session->addIeParams.probeRespBCNDataLen,
				sme_start_bss_req->addIeParams.
					probeRespBCNData_buff,
				sme_start_bss_req->addIeParams.
					probeRespBCNDataLen);

			/* Assoc resp IE */
			lim_start_bss_update_add_ie_buffer(mac_ctx,
				&session->addIeParams.assocRespData_buff,
				&session->addIeParams.assocRespDataLen,
				sme_start_bss_req->addIeParams.
					assocRespData_buff,
				sme_start_bss_req->addIeParams.
					assocRespDataLen);
		}
		/* Store the session related params in newly created session */
		session->pLimStartBssReq = sme_start_bss_req;

		session->transactionId = sme_start_bss_req->transactionId;

		qdf_mem_copy(&(session->htConfig),
			     &(sme_start_bss_req->htConfig),
			     sizeof(session->htConfig));

		qdf_mem_copy(&(session->vht_config),
			     &(sme_start_bss_req->vht_config),
			     sizeof(session->vht_config));

		sir_copy_mac_addr(session->selfMacAddr,
				  sme_start_bss_req->self_macaddr.bytes);

		/* Copy SSID to session table */
		qdf_mem_copy((uint8_t *) &session->ssId,
			     (uint8_t *) &sme_start_bss_req->ssId,
			     (sme_start_bss_req->ssId.length + 1));

		session->nwType = sme_start_bss_req->nwType;

		session->beaconParams.beaconInterval =
			sme_start_bss_req->beaconInterval;

		/* Store the channel number in session Table */
		session->currentOperChannel =
			sme_start_bss_req->channelId;

		/* Update the phymode */
		session->gLimPhyMode = sme_start_bss_req->nwType;

		session->maxTxPower =
			cfg_get_regulatory_max_transmit_power(mac_ctx,
				session->currentOperChannel);
		/* Store the dot 11 mode in to the session Table */
		session->dot11mode = sme_start_bss_req->dot11mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		session->cc_switch_mode =
			sme_start_bss_req->cc_switch_mode;
#endif
		session->htCapability =
			IS_DOT11_MODE_HT(session->dot11mode);
		session->vhtCapability =
			IS_DOT11_MODE_VHT(session->dot11mode);

		if (IS_DOT11_MODE_HE(session->dot11mode)) {
			lim_update_session_he_capable(mac_ctx, session);
			lim_copy_bss_he_cap(session, sme_start_bss_req);
		}

		session->txLdpcIniFeatureEnabled =
			sme_start_bss_req->txLdpcIniFeatureEnabled;
#ifdef WLAN_FEATURE_11W
		session->limRmfEnabled = sme_start_bss_req->pmfCapable ? 1 : 0;
		pe_debug("RMF enabled: %d", session->limRmfEnabled);
#endif

		qdf_mem_copy((void *)&session->rateSet,
			     (void *)&sme_start_bss_req->operationalRateSet,
			     sizeof(tSirMacRateSet));
		qdf_mem_copy((void *)&session->extRateSet,
			     (void *)&sme_start_bss_req->extendedRateSet,
			     sizeof(tSirMacRateSet));

		if (IS_5G_CH(session->currentOperChannel))
			vdev_type_nss = &mac_ctx->vdev_type_nss_5g;
		else
			vdev_type_nss = &mac_ctx->vdev_type_nss_2g;

		switch (sme_start_bss_req->bssType) {
		case eSIR_INFRA_AP_MODE:
			lim_configure_ap_start_bss_session(mac_ctx, session,
				sme_start_bss_req);
			if (session->pePersona == QDF_SAP_MODE)
				session->vdev_nss = vdev_type_nss->sap;
			else
				session->vdev_nss = vdev_type_nss->p2p_go;
			break;
		case eSIR_IBSS_MODE:
			session->limSystemRole = eLIM_STA_IN_IBSS_ROLE;
			lim_get_short_slot_from_phy_mode(mac_ctx, session,
				session->gLimPhyMode,
				&session->shortSlotTimeSupported);

			/*
			 * initialize to "OPEN".
			 * will be updated upon key installation
			 */
			session->encryptType = eSIR_ED_NONE;
			session->vdev_nss = vdev_type_nss->ibss;

			break;
		case eSIR_NDI_MODE:
			session->limSystemRole = eLIM_NDI_ROLE;
			break;


		/*
		 * There is one more mode called auto mode.
		 * which is used no where
		 */

		/* FORBUILD -TEMPFIX.. HOW TO use AUTO MODE????? */

		default:
			/* not used anywhere...used in scan function */
			break;
		}

		session->nss = session->vdev_nss;
		if (!mac_ctx->roam.configParam.enable2x2)
			session->nss = 1;
		/*
		 * Allocate memory for the array of
		 * parsed (Re)Assoc request structure
		 */
		if (sme_start_bss_req->bssType == eSIR_INFRA_AP_MODE) {
			session->parsedAssocReq =
				qdf_mem_malloc(session->dph.dphHashTable.
						size * sizeof(tpSirAssocReq));
			if (NULL == session->parsedAssocReq) {
				pe_warn("AllocateMemory() failed");
				ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
				goto free;
			}
		}

		if (!sme_start_bss_req->channelId &&
		    sme_start_bss_req->bssType != eSIR_NDI_MODE) {
			pe_err("Received invalid eWNI_SME_START_BSS_REQ");
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto free;
		}
		channel_number = sme_start_bss_req->channelId;
#ifdef QCA_HT_2040_COEX
		if (sme_start_bss_req->obssEnabled)
			session->htSupportedChannelWidthSet =
				session->htCapability;
		else
#endif
		session->htSupportedChannelWidthSet =
			(sme_start_bss_req->sec_ch_offset) ? 1 : 0;
		session->htSecondaryChannelOffset =
			sme_start_bss_req->sec_ch_offset;
		session->htRecommendedTxWidthSet =
			(session->htSecondaryChannelOffset) ? 1 : 0;
		if (lim_is_session_he_capable(session) ||
		    session->vhtCapability || session->htCapability) {
			chanwidth = sme_start_bss_req->vht_channel_width;
			session->ch_width = chanwidth;
			if (session->htSupportedChannelWidthSet) {
				session->ch_center_freq_seg0 =
					sme_start_bss_req->center_freq_seg0;
				session->ch_center_freq_seg1 =
					sme_start_bss_req->center_freq_seg1;
			} else {
				session->ch_center_freq_seg0 = 0;
				session->ch_center_freq_seg1 = 0;
			}
		}

		if (session->vhtCapability &&
			(session->ch_width > CH_WIDTH_80MHZ)) {
			session->nss = 1;
		}

		/* Delete pre-auth list if any */
		lim_delete_pre_auth_list(mac_ctx);

		/*
		 * keep the RSN/WPA IE information in PE Session Entry
		 * later will be using this to check when received (Re)Assoc req
		 */
		lim_set_rs_nie_wp_aiefrom_sme_start_bss_req_message(mac_ctx,
				&sme_start_bss_req->rsnIE, session);

		if (LIM_IS_AP_ROLE(session) ||
		    LIM_IS_IBSS_ROLE(session) ||
		    LIM_IS_NDI_ROLE(session)) {
			session->gLimProtectionControl =
				sme_start_bss_req->protEnabled;
			/*
			 * each byte will have the following info
			 * bit7       bit6    bit5   bit4 bit3   bit2  bit1 bit0
			 * reserved reserved   RIFS   Lsig n-GF   ht20  11g  11b
			 */
			qdf_mem_copy((void *)&session->cfgProtection,
				     (void *)&sme_start_bss_req->ht_capab,
				     sizeof(uint16_t));
			/* Initialize WPS PBC session link list */
			session->pAPWPSPBCSession = NULL;
		}
		/* Prepare and Issue LIM_MLM_START_REQ to MLM */
		mlm_start_req = qdf_mem_malloc(sizeof(tLimMlmStartReq));
		if (NULL == mlm_start_req) {
			pe_err("Allocate Memory failed for mlmStartReq");
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			goto free;
		}

		/* Copy SSID to the MLM start structure */
		qdf_mem_copy((uint8_t *) &mlm_start_req->ssId,
			     (uint8_t *) &sme_start_bss_req->ssId,
			     sme_start_bss_req->ssId.length + 1);
		mlm_start_req->ssidHidden = sme_start_bss_req->ssidHidden;
		mlm_start_req->obssProtEnabled =
			sme_start_bss_req->obssProtEnabled;

		mlm_start_req->bssType = session->bssType;

		/* Fill PE session Id from the session Table */
		mlm_start_req->sessionId = session->peSessionId;

		if (mlm_start_req->bssType == eSIR_INFRA_AP_MODE ||
		    mlm_start_req->bssType == eSIR_NDI_MODE) {
			/*
			 * Copy the BSSId from sessionTable to
			 * mlmStartReq struct
			 */
			sir_copy_mac_addr(mlm_start_req->bssId, session->bssId);
		} else {
			/* ibss mode */
			mac_ctx->lim.gLimIbssCoalescingHappened = false;

			ret_status = wlan_cfg_get_int(mac_ctx,
					WNI_CFG_IBSS_AUTO_BSSID,
					&auto_gen_bssid);
			if (ret_status != QDF_STATUS_SUCCESS) {
				pe_err("Get Auto Gen BSSID fail,Status: %d",
					ret_status);
				ret_code = eSIR_LOGE_EXCEPTION;
				goto free;
			}

			if (!auto_gen_bssid) {
				/*
				 * We're not auto generating BSSID.
				 * Instead, get it from session entry
				 */
				sir_copy_mac_addr(mlm_start_req->bssId,
						  session->bssId);
				/*
				 * Start IBSS group BSSID
				 * Auto Generating BSSID.
				 */
				auto_gen_bssid = ((mlm_start_req->bssId[0] &
							0x01) ? true : false);
			}

			if (auto_gen_bssid) {
				/*
				 * if BSSID is not any uc id.
				 * then use locally generated BSSID.
				 * Autogenerate the BSSID
				 */
				lim_get_random_bssid(mac_ctx,
						mlm_start_req->bssId);
				mlm_start_req->bssId[0] = 0x02;

				/*
				 * Copy randomly generated BSSID
				 * to the session Table
				 */
				sir_copy_mac_addr(session->bssId,
						  mlm_start_req->bssId);
			}
		}
		/* store the channel num in mlmstart req structure */
		mlm_start_req->channelNumber = session->currentOperChannel;
		mlm_start_req->cbMode = sme_start_bss_req->cbMode;
		mlm_start_req->beaconPeriod =
			session->beaconParams.beaconInterval;
		mlm_start_req->cac_duration_ms =
			sme_start_bss_req->cac_duration_ms;
		mlm_start_req->dfs_regdomain =
			sme_start_bss_req->dfs_regdomain;
		if (LIM_IS_AP_ROLE(session)) {
			mlm_start_req->dtimPeriod = session->dtimPeriod;
			mlm_start_req->wps_state = session->wps_state;

		} else {
			if (wlan_cfg_get_int(mac_ctx,
				WNI_CFG_DTIM_PERIOD, &val) != QDF_STATUS_SUCCESS)
				pe_err("could not retrieve DTIM Period");
			mlm_start_req->dtimPeriod = (uint8_t) val;
		}

		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_CFP_PERIOD, &val) !=
			QDF_STATUS_SUCCESS)
			pe_err("could not retrieve Beacon interval");
		mlm_start_req->cfParamSet.cfpPeriod = (uint8_t) val;

		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_CFP_MAX_DURATION, &val) !=
			QDF_STATUS_SUCCESS)
			pe_err("could not retrieve CFPMaxDuration");
		mlm_start_req->cfParamSet.cfpMaxDuration = (uint16_t) val;

		/*
		 * this may not be needed anymore now,
		 * as rateSet is now included in the
		 * session entry and MLM has session context.
		 */
		qdf_mem_copy((void *)&mlm_start_req->rateSet,
			     (void *)&session->rateSet,
			     sizeof(tSirMacRateSet));

		/* Now populate the 11n related parameters */
		mlm_start_req->nwType = session->nwType;
		mlm_start_req->htCapable = session->htCapability;

		mlm_start_req->htOperMode = mac_ctx->lim.gHTOperMode;
		/* Unused */
		mlm_start_req->dualCTSProtection =
			mac_ctx->lim.gHTDualCTSProtection;
		mlm_start_req->txChannelWidthSet =
			session->htRecommendedTxWidthSet;

		session->limRFBand = lim_get_rf_band(channel_number);

		/* Initialize 11h Enable Flag */
		session->lim11hEnable = 0;
		if (mlm_start_req->bssType != eSIR_IBSS_MODE &&
		    (CHAN_HOP_ALL_BANDS_ENABLE ||
		     BAND_5G == session->limRFBand)) {
			if (wlan_cfg_get_int(mac_ctx,
				WNI_CFG_11H_ENABLED, &val) != QDF_STATUS_SUCCESS)
				pe_err("Fail to get WNI_CFG_11H_ENABLED");
			else
				session->lim11hEnable = val;

			if (session->lim11hEnable &&
				(eSIR_INFRA_AP_MODE ==
					mlm_start_req->bssType)) {
				cfg_get_wmi_dfs_master_param =
					wlan_cfg_get_int(mac_ctx,
						WNI_CFG_DFS_MASTER_ENABLED,
						&val);
				session->lim11hEnable = val;
			}
			if (cfg_get_wmi_dfs_master_param != QDF_STATUS_SUCCESS)
				/* Failed get CFG WNI_CFG_DFS_MASTER_ENABLED */
				pe_err("Get Fail, CFG DFS ENABLE");
		}

		if (!session->lim11hEnable) {
			if (cfg_set_int(mac_ctx,
				WNI_CFG_LOCAL_POWER_CONSTRAINT, 0) !=
				QDF_STATUS_SUCCESS)
				/*
				 * Failed to set the CFG param
				 * WNI_CFG_LOCAL_POWER_CONSTRAINT
				 */
				pe_err("Set LOCAL_POWER_CONSTRAINT failed");
		}

		mlm_start_req->beacon_tx_rate = session->beacon_tx_rate;

		session->limPrevSmeState = session->limSmeState;
		session->limSmeState = eLIM_SME_WT_START_BSS_STATE;

		pe_debug("Chan %d width %d freq0 %d freq1 %d, dot11mode %d nss %d vendor vht %d",
			 session->currentOperChannel, session->ch_width,
			 session->ch_center_freq_seg0,
			 session->ch_center_freq_seg1,
			 session->dot11mode, session->vdev_nss,
			 session->vendor_vht_sap);
		MTRACE(mac_trace
			(mac_ctx, TRACE_CODE_SME_STATE,
			session->peSessionId,
			session->limSmeState));

		lim_post_mlm_message(mac_ctx, LIM_MLM_START_REQ,
			(uint32_t *) mlm_start_req);
		return;
	} else {

		pe_err("Received unexpected START_BSS_REQ, in state %X",
			mac_ctx->lim.gLimSmeState);
		ret_code = eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
		goto free;
	} /* if (mac_ctx->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) */

free:
	if ((session != NULL) &&
	    (session->pLimStartBssReq == sme_start_bss_req)) {
		session->pLimStartBssReq = NULL;
	}
	if (NULL != sme_start_bss_req)
		qdf_mem_free(sme_start_bss_req);
	if (NULL != mlm_start_req)
		qdf_mem_free(mlm_start_req);
	if (NULL != session) {
		pe_delete_session(mac_ctx, session);
		session = NULL;
	}
	lim_send_sme_start_bss_rsp(mac_ctx, eWNI_SME_START_BSS_RSP, ret_code,
		session, sme_session_id, sme_transaction_id);
}

/**
 * __lim_process_sme_start_bss_req() - Call handler to start BSS
 *
 * @pMac: Global MAC context
 * @pMsg: Message pointer
 *
 * Wrapper for the function __lim_handle_sme_start_bss_request
 * This message will be defered until softmac come out of
 * scan mode or if we have detected radar on the current
 * operating channel.
 *
 * return true - If we consumed the buffer
 *        false - If have defered the message.
 */
static bool __lim_process_sme_start_bss_req(tpAniSirGlobal pMac,
					    struct scheduler_msg *pMsg)
{
	if (__lim_is_defered_msg_for_learn(pMac, pMsg) ||
	    __lim_is_defered_msg_for_radar(pMac, pMsg)) {
		/**
		 * If message defered, buffer is not consumed yet.
		 * So return false
		 */
		return false;
	}

	__lim_handle_sme_start_bss_request(pMac, (uint32_t *) pMsg->bodyptr);
	return true;
}

/**
 *  lim_get_random_bssid()
 *
 *  FUNCTION:This function is called to process generate the random number for bssid
 *  This function is called to process SME_SCAN_REQ message
 *  from HDD or upper layer application.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 * 1. geneartes the unique random number for bssid in ibss
 *
 *  @param  pMac      Pointer to Global MAC structure
 *  @param  *data      Pointer to  bssid  buffer
 *  @return None
 */
void lim_get_random_bssid(tpAniSirGlobal pMac, uint8_t *data)
{
	uint32_t random[2];

	random[0] = tx_time_get();
	random[0] |= (random[0] << 15);
	random[1] = random[0] >> 1;
	qdf_mem_copy(data, (uint8_t *) random, sizeof(tSirMacAddr));
}

#ifdef WLAN_FEATURE_SAE

/**
 * lim_update_sae_config()- This API update SAE session info to csr config
 * from join request.
 * @session: PE session
 * @sme_join_req: pointer to join request
 *
 * Return: None
 */
static void lim_update_sae_config(tpPESession session,
		tpSirSmeJoinReq sme_join_req)
{
	session->sae_pmk_cached = sme_join_req->sae_pmk_cached;

	pe_debug("pmk_cached %d for BSSID=" MAC_ADDRESS_STR,
		session->sae_pmk_cached,
		MAC_ADDR_ARRAY(sme_join_req->bssDescription.bssId));
}
#else
static inline void lim_update_sae_config(tpPESession session,
		tpSirSmeJoinReq sme_join_req)
{}
#endif


/**
 * __lim_process_sme_join_req() - process SME_JOIN_REQ message
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: A pointer to the SME message buffer
 *
 * This function is called to process SME_JOIN_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */
static void
__lim_process_sme_join_req(tpAniSirGlobal mac_ctx, uint32_t *msg_buf)
{
	tpSirSmeJoinReq sme_join_req = NULL;
	tLimMlmJoinReq *mlm_join_req;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	uint32_t val = 0;
	uint16_t n_size;
	uint8_t session_id;
	tpPESession session = NULL;
	uint8_t sme_session_id = 0;
	uint16_t sme_transaction_id = 0;
	int8_t local_power_constraint = 0, reg_max = 0;
	uint16_t ie_len;
	const uint8_t *vendor_ie;
	tSirBssDescription *bss_desc;
	struct lim_max_tx_pwr_attr tx_pwr_attr = {0};

	if (!mac_ctx || !msg_buf) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
			  FL("JOIN REQ with invalid data"));
		return;
	}

/* FEATURE_WLAN_DIAG_SUPPORT */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
	/*
	 * Not sending any session, since it is not created yet.
	 * The response whould have correct state.
	 */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_JOIN_REQ_EVENT, NULL, 0, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	/*
	 * Expect Join request in idle state.
	 * Reassociate request is expected in link established state.
	 */

	/* Global SME and LIM states are not defined yet for BT-AMP Support */
	if (mac_ctx->lim.gLimSmeState == eLIM_SME_IDLE_STATE) {
		n_size = __lim_get_sme_join_req_size_for_alloc((uint8_t *)
				msg_buf);

		sme_join_req = qdf_mem_malloc(n_size);
		if (NULL == sme_join_req) {
			pe_err("AllocateMemory failed for sme_join_req");
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			goto end;
		}
		(void)qdf_mem_copy((void *)sme_join_req, (void *)msg_buf,
			n_size);

		if (!lim_is_sme_join_req_valid(mac_ctx, sme_join_req)) {
			/* Received invalid eWNI_SME_JOIN_REQ */
			/* Log the event */
			pe_warn("SessionId:%d JOIN REQ with invalid data",
				sme_join_req->sessionId);
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}

		/*
		 * Update the capability here itself as this is used in
		 * lim_extract_ap_capability() below. If not updated issues
		 * like not honoring power constraint on 1st association after
		 * driver loading might occur.
		 */
		lim_update_rrm_capability(mac_ctx, sme_join_req);

		bss_desc = &sme_join_req->bssDescription;
		/* check for the existence of start BSS session  */
		session = pe_find_session_by_bssid(mac_ctx, bss_desc->bssId,
				&session_id);

		if (session != NULL) {
			pe_err("Session(%d) Already exists for BSSID: "
				   MAC_ADDRESS_STR " in limSmeState = %X",
				session_id,
				MAC_ADDR_ARRAY(bss_desc->bssId),
				session->limSmeState);

			if (session->limSmeState == eLIM_SME_LINK_EST_STATE &&
			    session->smeSessionId == sme_join_req->sessionId) {
				/*
				 * Received eWNI_SME_JOIN_REQ for same
				 * BSS as currently associated.
				 * Log the event and send success
				 */
				pe_warn("SessionId: %d", session_id);
				pe_warn("JOIN_REQ for current joined BSS");
				/* Send Join success response to host */
				ret_code = eSIR_SME_ALREADY_JOINED_A_BSS;
				session = NULL;
				goto end;
			} else {
				pe_err("JOIN_REQ not for current joined BSS");
				ret_code = eSIR_SME_REFUSED;
				session = NULL;
				goto end;
			}
		} else {
			/*
			 * Session Entry does not exist for given BSSId
			 * Try to Create a new session
			 */
			session = pe_create_session(mac_ctx, bss_desc->bssId,
					&session_id, mac_ctx->lim.maxStation,
					eSIR_INFRASTRUCTURE_MODE,
					sme_join_req->sessionId,
					sme_join_req->staPersona);
			if (!session) {
				pe_err("Session Can not be created");
				ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
				goto end;
			}
			/* Update the beacon/probe filter in mac_ctx */
			lim_set_bcn_probe_filter(mac_ctx, session,
						 &sme_join_req->ssId,
						 bss_desc->channelId);
		}
		session->max_amsdu_num = sme_join_req->max_amsdu_num;
		session->enable_session_twt_support =
			sme_join_req->enable_session_twt_support;
		/*
		 * Store Session related parameters
		 */

		/* store the smejoin req handle in session table */
		session->pLimJoinReq = sme_join_req;

		/* Store SME transaction Id in session Table */
		session->transactionId = sme_join_req->transactionId;

		/* Store beaconInterval */
		session->beaconParams.beaconInterval =
			bss_desc->beaconInterval;

		qdf_mem_copy(&(session->htConfig), &(sme_join_req->htConfig),
			sizeof(session->htConfig));

		qdf_mem_copy(&(session->vht_config),
			&(sme_join_req->vht_config),
			sizeof(session->vht_config));

		/* Copying of bssId is already done, while creating session */
		sir_copy_mac_addr(session->selfMacAddr,
			sme_join_req->selfMacAddr);

		session->statypeForBss = STA_ENTRY_PEER;
		session->limWmeEnabled = sme_join_req->isWMEenabled;
		session->limQosEnabled = sme_join_req->isQosEnabled;
		session->wps_registration = sme_join_req->wps_registration;
		session->he_with_wep_tkip = sme_join_req->he_with_wep_tkip;

		session->enable_bcast_probe_rsp =
				sme_join_req->enable_bcast_probe_rsp;

		/* Store vendor specific IE for CISCO AP */
		ie_len = (bss_desc->length + sizeof(bss_desc->length) -
			 GET_FIELD_OFFSET(tSirBssDescription, ieFields));

		vendor_ie = wlan_get_vendor_ie_ptr_from_oui(
				SIR_MAC_CISCO_OUI, SIR_MAC_CISCO_OUI_SIZE,
				((uint8_t *)&bss_desc->ieFields), ie_len);

		if (vendor_ie)
			session->isCiscoVendorAP = true;
		else
			session->isCiscoVendorAP = false;

		/* Copy the dot 11 mode in to the session table */

		session->dot11mode = sme_join_req->dot11mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		session->cc_switch_mode = sme_join_req->cc_switch_mode;
#endif
		session->nwType = bss_desc->nwType;
		session->enableAmpduPs = sme_join_req->enableAmpduPs;
		session->enableHtSmps = sme_join_req->enableHtSmps;
		session->htSmpsvalue = sme_join_req->htSmps;
		session->send_smps_action =
			sme_join_req->send_smps_action;
		/*
		 * By default supported NSS 1x1 is set to true
		 * and later on updated while determining session
		 * supported rates which is the intersection of
		 * self and peer rates
		 */
		session->supported_nss_1x1 = true;
		session->currentOperChannel = bss_desc->channelId;

		session->vhtCapability =
			IS_DOT11_MODE_VHT(session->dot11mode);
		if (session->vhtCapability) {
			if (session->pePersona == QDF_STA_MODE) {
				session->vht_config.su_beam_formee =
					sme_join_req->vht_config.su_beam_formee;
			} else {
				session->vht_config.su_beam_formee = 0;
			}
			session->enableVhtpAid =
				sme_join_req->enableVhtpAid;
			session->enableVhtGid =
				sme_join_req->enableVhtGid;
		}

		/*Phy mode */
		session->gLimPhyMode = bss_desc->nwType;
		handle_ht_capabilityand_ht_info(mac_ctx, session);
		session->force_24ghz_in_ht20 =
			sme_join_req->force_24ghz_in_ht20;
		/* cbMode is already merged value of peer and self -
		 * done by csr in csr_get_cb_mode_from_ies */
		session->htSupportedChannelWidthSet =
			(sme_join_req->cbMode) ? 1 : 0;
		session->htRecommendedTxWidthSet =
			session->htSupportedChannelWidthSet;
		session->htSecondaryChannelOffset = sme_join_req->cbMode;

		if (PHY_DOUBLE_CHANNEL_HIGH_PRIMARY == sme_join_req->cbMode) {
			session->ch_center_freq_seg0 =
				session->currentOperChannel - 2;
			session->ch_width = CH_WIDTH_40MHZ;
		} else if (PHY_DOUBLE_CHANNEL_LOW_PRIMARY ==
				sme_join_req->cbMode) {
			session->ch_center_freq_seg0 =
				session->currentOperChannel + 2;
			session->ch_width = CH_WIDTH_40MHZ;
		} else {
			session->ch_center_freq_seg0 = 0;
			session->ch_width = CH_WIDTH_20MHZ;
		}

		if (IS_DOT11_MODE_HE(session->dot11mode)) {
			lim_update_session_he_capable(mac_ctx, session);
			lim_copy_join_req_he_cap(session, sme_join_req);
		}


		/* Record if management frames need to be protected */
#ifdef WLAN_FEATURE_11W
		if ((eSIR_ED_AES_128_CMAC ==
					    sme_join_req->MgmtEncryptionType) ||
		   (eSIR_ED_AES_GMAC_128 == sme_join_req->MgmtEncryptionType) ||
		   (eSIR_ED_AES_GMAC_256 == sme_join_req->MgmtEncryptionType))
			session->limRmfEnabled = 1;
		else
			session->limRmfEnabled = 0;
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
		session->rssi = bss_desc->rssi;
#endif

		/* Copy the SSID from smejoinreq to session entry  */
		session->ssId.length = sme_join_req->ssId.length;
		qdf_mem_copy(session->ssId.ssId, sme_join_req->ssId.ssId,
			session->ssId.length);

		/*
		 * Determin 11r or ESE connection based on input from SME
		 * which inturn is dependent on the profile the user wants
		 * to connect to, So input is coming from supplicant
		 */
		session->is11Rconnection = sme_join_req->is11Rconnection;
		session->connected_akm = sme_join_req->akm;
		session->is_adaptive_11r_connection =
				sme_join_req->is_adaptive_11r_connection;
#ifdef FEATURE_WLAN_ESE
		session->isESEconnection = sme_join_req->isESEconnection;
#endif
		session->isFastTransitionEnabled =
			sme_join_req->isFastTransitionEnabled;

		session->isFastRoamIniFeatureEnabled =
			sme_join_req->isFastRoamIniFeatureEnabled;
		session->txLdpcIniFeatureEnabled =
			sme_join_req->txLdpcIniFeatureEnabled;

		lim_update_fils_config(mac_ctx, session, sme_join_req);
		lim_update_sae_config(session, sme_join_req);
		if (session->bssType == eSIR_INFRASTRUCTURE_MODE) {
			session->limSystemRole = eLIM_STA_ROLE;
		} else {
			/*
			 * Throw an error and return and make
			 * sure to delete the session.
			 */
			pe_err("recvd JOIN_REQ with invalid bss type %d",
				session->bssType);
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}

		if (sme_join_req->addIEScan.length)
			qdf_mem_copy(&session->pLimJoinReq->addIEScan,
				&sme_join_req->addIEScan, sizeof(tSirAddie));

		if (sme_join_req->addIEAssoc.length)
			qdf_mem_copy(&session->pLimJoinReq->addIEAssoc,
				&sme_join_req->addIEAssoc, sizeof(tSirAddie));

		val = sizeof(tLimMlmJoinReq) +
			session->pLimJoinReq->bssDescription.length + 2;
		mlm_join_req = qdf_mem_malloc(val);
		if (NULL == mlm_join_req) {
			pe_err("AllocateMemory failed for mlmJoinReq");
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			goto end;
		}

		/* PE SessionId is stored as a part of JoinReq */
		mlm_join_req->sessionId = session->peSessionId;

		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_JOIN_FAILURE_TIMEOUT,
			(uint32_t *) &mlm_join_req->joinFailureTimeout) !=
			QDF_STATUS_SUCCESS) {
			pe_err("couldn't retrieve JoinFailureTimer value"
				" setting to default value");
			mlm_join_req->joinFailureTimeout =
				WNI_CFG_JOIN_FAILURE_TIMEOUT_STADEF;
		}

		/* copy operational rate from session */
		qdf_mem_copy((void *)&session->rateSet,
			(void *)&sme_join_req->operationalRateSet,
			sizeof(tSirMacRateSet));
		qdf_mem_copy((void *)&session->extRateSet,
			(void *)&sme_join_req->extendedRateSet,
			sizeof(tSirMacRateSet));
		/*
		 * this may not be needed anymore now, as rateSet is now
		 * included in the session entry and MLM has session context.
		 */
		qdf_mem_copy((void *)&mlm_join_req->operationalRateSet,
			(void *)&session->rateSet,
			sizeof(tSirMacRateSet));

		session->encryptType = sme_join_req->UCEncryptionType;

		session->supported_nss_1x1 = sme_join_req->supported_nss_1x1;
		session->vdev_nss = sme_join_req->vdev_nss;
		session->nss = sme_join_req->nss;
		session->nss_forced_1x1 = sme_join_req->nss_forced_1x1;

		mlm_join_req->bssDescription.length =
			session->pLimJoinReq->bssDescription.length;

		qdf_mem_copy((uint8_t *) &mlm_join_req->bssDescription.bssId,
			(uint8_t *)
			&session->pLimJoinReq->bssDescription.bssId,
			session->pLimJoinReq->bssDescription.length + 2);

		session->limCurrentBssCaps =
			session->pLimJoinReq->bssDescription.capabilityInfo;

		reg_max = cfg_get_regulatory_max_transmit_power(mac_ctx,
				session->currentOperChannel);
		local_power_constraint = reg_max;

		lim_extract_ap_capability(mac_ctx,
			(uint8_t *)
			session->pLimJoinReq->bssDescription.ieFields,
			lim_get_ielen_from_bss_description(
			&session->pLimJoinReq->bssDescription),
			&session->limCurrentBssQosCaps,
			&session->limCurrentBssPropCap,
			&session->gLimCurrentBssUapsd,
			&local_power_constraint, session);

		tx_pwr_attr.reg_max = reg_max;
		tx_pwr_attr.ap_tx_power = local_power_constraint;
		tx_pwr_attr.ini_tx_power =
				mac_ctx->roam.configParam.nTxPowerCap;
		tx_pwr_attr.frequency =
			wlan_reg_get_channel_freq(mac_ctx->pdev,
						  session->currentOperChannel);

		session->maxTxPower = lim_get_max_tx_power(mac_ctx,
							   &tx_pwr_attr);
		session->def_max_tx_pwr = session->maxTxPower;

		pe_debug("Reg %d local %d session %d join req %d",
			 reg_max, local_power_constraint, session->maxTxPower,
			 sme_join_req->powerCap.maxTxPower);

		if (sme_join_req->powerCap.maxTxPower > session->maxTxPower)
			sme_join_req->powerCap.maxTxPower = session->maxTxPower;

		if (session->gLimCurrentBssUapsd) {
			session->gUapsdPerAcBitmask =
				session->pLimJoinReq->uapsdPerAcBitmask;
			pe_debug("UAPSD flag for all AC - 0x%2x",
				 session->gUapsdPerAcBitmask);

			/* resetting the dynamic uapsd mask  */
			session->gUapsdPerAcDeliveryEnableMask = 0;
			session->gUapsdPerAcTriggerEnableMask = 0;
		}

		session->limRFBand =
			lim_get_rf_band(session->currentOperChannel);

		/* Initialize 11h Enable Flag */
		if (BAND_5G == session->limRFBand) {
			if (wlan_cfg_get_int(mac_ctx, WNI_CFG_11H_ENABLED,
				&val) != QDF_STATUS_SUCCESS) {
				pe_err("Fail to get WNI_CFG_11H_ENABLED");
				session->lim11hEnable =
					WNI_CFG_11H_ENABLED_STADEF;
			} else {
				session->lim11hEnable = val;
			}
		} else {
			session->lim11hEnable = 0;
		}

		/*
		 * To care of the scenario when STA transitions from
		 * IBSS to Infrastructure mode.
		 */
		mac_ctx->lim.gLimIbssCoalescingHappened = false;

		session->limPrevSmeState = session->limSmeState;
		session->limSmeState = eLIM_SME_WT_JOIN_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session->peSessionId,
				session->limSmeState));

		/* Indicate whether spectrum management is enabled */
		session->spectrumMgtEnabled =
			sme_join_req->spectrumMgtIndicator;

		/* Enable the spectrum management if this is a DFS channel */
		if (session->country_info_present &&
			lim_isconnected_on_dfs_channel(mac_ctx,
					session->currentOperChannel))
			session->spectrumMgtEnabled = true;

		session->isOSENConnection = sme_join_req->isOSENConnection;
		pe_debug("Chan %d width %d freq0 %d freq1 %d, Smps %d: mode %d action %d, nss 1x1 %d vdev_nss %d nss %d cbMode %d dot11mode %d subfer %d subfee %d csn %d is_cisco %d",
			 session->currentOperChannel, session->ch_width,
			 session->ch_center_freq_seg0,
			 session->ch_center_freq_seg1,
			 session->enableHtSmps, session->htSmpsvalue,
			 session->send_smps_action, session->supported_nss_1x1,
			 session->vdev_nss, session->nss,
			 sme_join_req->cbMode, session->dot11mode,
			 session->vht_config.su_beam_former,
			 session->vht_config.su_beam_formee,
			 session->vht_config.csnof_beamformer_antSup,
			 session->isCiscoVendorAP);

		/* Issue LIM_MLM_JOIN_REQ to MLM */
		lim_post_mlm_message(mac_ctx, LIM_MLM_JOIN_REQ,
				     (uint32_t *) mlm_join_req);
		return;

	} else {
		/* Received eWNI_SME_JOIN_REQ un expected state */
		pe_err("received unexpected SME_JOIN_REQ in state %X",
			mac_ctx->lim.gLimSmeState);
		lim_print_sme_state(mac_ctx, LOGE, mac_ctx->lim.gLimSmeState);
		ret_code = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		session = NULL;
		goto end;
	}

end:
	lim_get_session_info(mac_ctx, (uint8_t *) msg_buf,
		&sme_session_id, &sme_transaction_id);

	if (sme_join_req) {
		qdf_mem_free(sme_join_req);
		sme_join_req = NULL;
		if (NULL != session)
			session->pLimJoinReq = NULL;
	}
	if (ret_code != eSIR_SME_SUCCESS) {
		if (NULL != session) {
			pe_delete_session(mac_ctx, session);
			session = NULL;
		}
	}
	pe_debug("Send failure status on sessionid: %d with ret_code: %d",
		sme_session_id, ret_code);
	lim_send_sme_join_reassoc_rsp(mac_ctx, eWNI_SME_JOIN_RSP, ret_code,
		eSIR_MAC_UNSPEC_FAILURE_STATUS, session, sme_session_id,
		sme_transaction_id);
}

uint8_t lim_get_max_tx_power(tpAniSirGlobal mac,
			     struct lim_max_tx_pwr_attr *attr)
{
	uint8_t max_tx_power = 0;
	uint8_t tx_power;

	if (!attr)
		return 0;

	if (wlan_reg_get_fcc_constraint(mac->pdev, attr->frequency))
		return attr->reg_max;

	tx_power = QDF_MIN(attr->reg_max, attr->ap_tx_power);
	tx_power = QDF_MIN(tx_power, attr->ini_tx_power);

	if (tx_power >= MIN_TX_PWR_CAP && tx_power <= MAX_TX_PWR_CAP)
		max_tx_power = tx_power;
	else if (tx_power < MIN_TX_PWR_CAP)
		max_tx_power = MIN_TX_PWR_CAP;
	else
		max_tx_power = MAX_TX_PWR_CAP;

	return max_tx_power;
}

/**
 * __lim_process_sme_reassoc_req() - process reassoc req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_REASSOC_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */

static void __lim_process_sme_reassoc_req(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	uint16_t caps;
	uint32_t val;
	tpSirSmeJoinReq reassoc_req = NULL;
	tLimMlmReassocReq *mlm_reassoc_req;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	tpPESession session_entry = NULL;
	uint8_t session_id;
	uint8_t sme_session_id;
	uint16_t transaction_id;
	int8_t local_pwr_constraint = 0, reg_max = 0;
	uint32_t tele_bcn_en = 0;
	uint16_t size;

	size = __lim_get_sme_join_req_size_for_alloc((uint8_t *)msg_buf);
	reassoc_req = qdf_mem_malloc(size);
	if (NULL == reassoc_req) {
		pe_err("call to AllocateMemory failed for reassoc_req");

		ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}
	(void)qdf_mem_copy((void *)reassoc_req, (void *)msg_buf, size);

	if (!lim_is_sme_join_req_valid(mac_ctx,
				(tpSirSmeJoinReq)reassoc_req)) {
		/*
		 * Received invalid eWNI_SME_REASSOC_REQ
		 */
		pe_warn("received SME_REASSOC_REQ with invalid data");

		ret_code = eSIR_SME_INVALID_PARAMETERS;
		goto end;
	}

	session_entry = pe_find_session_by_bssid(mac_ctx,
			reassoc_req->bssDescription.bssId,
			&session_id);
	if (session_entry == NULL) {
		pe_err("Session does not exist for given bssId");
		lim_print_mac_addr(mac_ctx, reassoc_req->bssDescription.bssId,
				LOGE);
		ret_code = eSIR_SME_INVALID_PARAMETERS;
		lim_get_session_info(mac_ctx, (uint8_t *)msg_buf,
				&sme_session_id, &transaction_id);
		session_entry =
			pe_find_session_by_sme_session_id(mac_ctx,
					sme_session_id);
		if (session_entry != NULL)
			lim_handle_sme_join_result(mac_ctx,
					eSIR_SME_INVALID_PARAMETERS,
					eSIR_MAC_UNSPEC_FAILURE_STATUS,
					session_entry);
		goto end;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_REASSOC_REQ_EVENT,
			session_entry, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
	/* mac_ctx->lim.gpLimReassocReq = reassoc_req;//TO SUPPORT BT-AMP */

	/* Store the reassoc handle in the session Table */
	session_entry->pLimReAssocReq = reassoc_req;

	session_entry->dot11mode = reassoc_req->dot11mode;
	session_entry->vhtCapability =
		IS_DOT11_MODE_VHT(reassoc_req->dot11mode);

	if (session_entry->vhtCapability) {
		if (session_entry->pePersona == QDF_STA_MODE) {
			session_entry->vht_config.su_beam_formee =
				reassoc_req->vht_config.su_beam_formee;
		} else {
			reassoc_req->vht_config.su_beam_formee = 0;
		}
		session_entry->enableVhtpAid =
			reassoc_req->enableVhtpAid;
		session_entry->enableVhtGid =
			reassoc_req->enableVhtGid;
		pe_debug("vht su bformer [%d]", session_entry->vht_config.su_beam_former);
	}

	session_entry->supported_nss_1x1 = reassoc_req->supported_nss_1x1;
	session_entry->vdev_nss = reassoc_req->vdev_nss;
	session_entry->nss = reassoc_req->nss;
	session_entry->nss_forced_1x1 = reassoc_req->nss_forced_1x1;

	pe_debug("vhtCapability: %d su_beam_formee: %d su_tx_bformer %d",
		session_entry->vhtCapability,
		session_entry->vht_config.su_beam_formee,
		session_entry->vht_config.su_beam_former);

	session_entry->enableHtSmps = reassoc_req->enableHtSmps;
	session_entry->htSmpsvalue = reassoc_req->htSmps;
	session_entry->send_smps_action =
		reassoc_req->send_smps_action;
	pe_debug("enableHtSmps: %d htSmps: %d send action: %d supported nss 1x1: %d",
		session_entry->enableHtSmps,
		session_entry->htSmpsvalue,
		session_entry->send_smps_action,
		session_entry->supported_nss_1x1);
	/*
	 * Reassociate request is expected
	 * in link established state only.
	 */

	if (session_entry->limSmeState != eLIM_SME_LINK_EST_STATE) {
		if (session_entry->limSmeState == eLIM_SME_WT_REASSOC_STATE) {
			/*
			 * May be from 11r FT pre-auth. So lets check it
			 * before we bail out
			 */
			pe_debug("Session in reassoc state is %d",
					session_entry->peSessionId);

			/* Make sure its our preauth bssid */
			if (qdf_mem_cmp(reassoc_req->bssDescription.bssId,
					     session_entry->limReAssocbssId,
					     6)) {
				lim_print_mac_addr(mac_ctx,
						   reassoc_req->bssDescription.
						   bssId, LOGE);
				pe_err("Unknown bssId in reassoc state");
				ret_code = eSIR_SME_INVALID_PARAMETERS;
				goto end;
			}

			lim_process_mlm_ft_reassoc_req(mac_ctx, msg_buf,
					session_entry);
			return;
		}
		/*
		 * Should not have received eWNI_SME_REASSOC_REQ
		 */
		pe_err("received unexpected SME_REASSOC_REQ in state %X",
			session_entry->limSmeState);
		lim_print_sme_state(mac_ctx, LOGE, session_entry->limSmeState);

		ret_code = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		goto end;
	}

	qdf_mem_copy(session_entry->limReAssocbssId,
		     session_entry->pLimReAssocReq->bssDescription.bssId,
		     sizeof(tSirMacAddr));

	session_entry->limReassocChannelId =
		session_entry->pLimReAssocReq->bssDescription.channelId;

	session_entry->reAssocHtSupportedChannelWidthSet =
		(session_entry->pLimReAssocReq->cbMode) ? 1 : 0;
	session_entry->reAssocHtRecommendedTxWidthSet =
		session_entry->reAssocHtSupportedChannelWidthSet;
	session_entry->reAssocHtSecondaryChannelOffset =
		session_entry->pLimReAssocReq->cbMode;

	session_entry->limReassocBssCaps =
		session_entry->pLimReAssocReq->bssDescription.capabilityInfo;
	reg_max = cfg_get_regulatory_max_transmit_power(mac_ctx,
			session_entry->currentOperChannel);
	local_pwr_constraint = reg_max;

	lim_extract_ap_capability(mac_ctx,
		(uint8_t *)session_entry->pLimReAssocReq->bssDescription.ieFields,
		lim_get_ielen_from_bss_description(
			&session_entry->pLimReAssocReq->bssDescription),
		&session_entry->limReassocBssQosCaps,
		&session_entry->limReassocBssPropCap,
		&session_entry->gLimCurrentBssUapsd,
		&local_pwr_constraint, session_entry);
	session_entry->maxTxPower = QDF_MIN(reg_max, (local_pwr_constraint));
	pe_err("Reg max = %d, local pwr constraint = %d, max tx = %d",
		reg_max, local_pwr_constraint, session_entry->maxTxPower);
	/* Copy the SSID from session entry to local variable */
	session_entry->limReassocSSID.length = reassoc_req->ssId.length;
	qdf_mem_copy(session_entry->limReassocSSID.ssId,
		     reassoc_req->ssId.ssId,
		     session_entry->limReassocSSID.length);
	if (session_entry->gLimCurrentBssUapsd) {
		session_entry->gUapsdPerAcBitmask =
			session_entry->pLimReAssocReq->uapsdPerAcBitmask;
		pe_debug("UAPSD flag for all AC - 0x%2x",
			session_entry->gUapsdPerAcBitmask);
	}

	mlm_reassoc_req = qdf_mem_malloc(sizeof(tLimMlmReassocReq));
	if (NULL == mlm_reassoc_req) {
		pe_err("call to AllocateMemory failed for mlmReassocReq");

		ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}

	qdf_mem_copy(mlm_reassoc_req->peerMacAddr,
		     session_entry->limReAssocbssId, sizeof(tSirMacAddr));

	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
			(uint32_t *)&mlm_reassoc_req->reassocFailureTimeout) !=
			QDF_STATUS_SUCCESS)
		pe_err("could not retrieve ReassocFailureTimeout value");

	if (cfg_get_capability_info(mac_ctx, &caps, session_entry) !=
			QDF_STATUS_SUCCESS)
		pe_err("could not retrieve Capabilities value");

	lim_update_caps_info_for_bss(mac_ctx, &caps,
				reassoc_req->bssDescription.capabilityInfo);
	pe_debug("Capabilities info Reassoc: 0x%X", caps);

	mlm_reassoc_req->capabilityInfo = caps;

	/* Update PE session_id */
	mlm_reassoc_req->sessionId = session_id;

	/*
	 * If telescopic beaconing is enabled, set listen interval to
	 * WNI_CFG_TELE_BCN_MAX_LI
	 */
	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_TELE_BCN_WAKEUP_EN,
				&tele_bcn_en) != QDF_STATUS_SUCCESS)
		pe_err("Couldn't get WNI_CFG_TELE_BCN_WAKEUP_EN");

	val = WNI_CFG_LISTEN_INTERVAL_STADEF;

	if (tele_bcn_en) {
		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_TELE_BCN_MAX_LI, &val) !=
		    QDF_STATUS_SUCCESS)
			pe_err("could not retrieve ListenInterval");
	} else {
		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_LISTEN_INTERVAL, &val) !=
		    QDF_STATUS_SUCCESS)
			pe_err("could not retrieve ListenInterval");
	}

	mlm_reassoc_req->listenInterval = (uint16_t) val;

	/* Indicate whether spectrum management is enabled */
	session_entry->spectrumMgtEnabled = reassoc_req->spectrumMgtIndicator;

	/* Enable the spectrum management if this is a DFS channel */
	if (session_entry->country_info_present &&
			lim_isconnected_on_dfs_channel(mac_ctx,
				session_entry->currentOperChannel))
		session_entry->spectrumMgtEnabled = true;

	session_entry->limPrevSmeState = session_entry->limSmeState;
	session_entry->limSmeState = eLIM_SME_WT_REASSOC_STATE;

	MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));

	lim_post_mlm_message(mac_ctx,
			     LIM_MLM_REASSOC_REQ, (uint32_t *)mlm_reassoc_req);
	return;
end:
	if (reassoc_req) {
		qdf_mem_free(reassoc_req);
		if (session_entry)
			session_entry->pLimReAssocReq = NULL;
	}

	if (session_entry) {
		/*
		 * error occurred after we determined the session so extract
		 * session and transaction info from there
		 */
		sme_session_id = session_entry->smeSessionId;
		transaction_id = session_entry->transactionId;
	} else {
		/*
		 * error occurred before or during the time we determined
		 * the session so extract the session and transaction info
		 * from the message
		 */
		lim_get_session_info(mac_ctx, (uint8_t *) msg_buf,
				&sme_session_id, &transaction_id);
	}
	/*
	 * Send Reassoc failure response to host
	 * (note session_entry may be NULL, but that's OK)
	 */
	lim_send_sme_join_reassoc_rsp(mac_ctx, eWNI_SME_REASSOC_RSP,
				      ret_code, eSIR_MAC_UNSPEC_FAILURE_STATUS,
				      session_entry, sme_session_id,
				      transaction_id);
}

bool send_disassoc_frame = 1;
/**
 * __lim_process_sme_disassoc_req()
 *
 ***FUNCTION:
 * This function is called to process SME_DISASSOC_REQ message
 * from HDD or upper layer application.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void __lim_process_sme_disassoc_req(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	uint16_t disassocTrigger, reasonCode;
	tLimMlmDisassocReq *pMlmDisassocReq;
	tSirResultCodes retCode = eSIR_SME_SUCCESS;
	tSirSmeDisassocReq smeDisassocReq;
	tpPESession psessionEntry = NULL;
	uint8_t sessionId;
	uint8_t smesessionId;
	uint16_t smetransactionId;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	qdf_mem_copy(&smeDisassocReq, pMsgBuf, sizeof(tSirSmeDisassocReq));
	smesessionId = smeDisassocReq.sessionId;
	smetransactionId = smeDisassocReq.transactionId;
	if (!lim_is_sme_disassoc_req_valid(pMac,
					   &smeDisassocReq,
					   psessionEntry)) {
		pe_err("received invalid SME_DISASSOC_REQ message");
		if (pMac->lim.gLimRspReqd) {
			pMac->lim.gLimRspReqd = false;

			retCode = eSIR_SME_INVALID_PARAMETERS;
			disassocTrigger = eLIM_HOST_DISASSOC;
			goto sendDisassoc;
		}

		return;
	}

	psessionEntry = pe_find_session_by_bssid(pMac,
				smeDisassocReq.bssid.bytes,
				&sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given bssId "
			   MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(smeDisassocReq.bssid.bytes));
		retCode = eSIR_SME_INVALID_PARAMETERS;
		disassocTrigger = eLIM_HOST_DISASSOC;
		goto sendDisassoc;
	}
	pe_debug("vdev %d (session %d) Systemrole %d Reason: %u SmeState: %d limMlmState %d ho fail %d  send OTA %d from: "
		 QDF_MAC_ADDR_STR, psessionEntry->smeSessionId,
		 psessionEntry->peSessionId, GET_LIM_SYSTEM_ROLE(psessionEntry),
		 smeDisassocReq.reasonCode,
		 psessionEntry->limSmeState, psessionEntry->limMlmState,
		 smeDisassocReq.process_ho_fail,
		 smeDisassocReq.doNotSendOverTheAir,
		 QDF_MAC_ADDR_ARRAY(smeDisassocReq.peer_macaddr.bytes));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(pMac, WLAN_PE_DIAG_DISASSOC_REQ_EVENT, psessionEntry,
			      0, smeDisassocReq.reasonCode);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	/* Update SME session Id and SME transaction ID */

	psessionEntry->smeSessionId = smesessionId;
	psessionEntry->transactionId = smetransactionId;
	psessionEntry->process_ho_fail = smeDisassocReq.process_ho_fail;

	switch (GET_LIM_SYSTEM_ROLE(psessionEntry)) {
	case eLIM_STA_ROLE:
		switch (psessionEntry->limSmeState) {
		case eLIM_SME_ASSOCIATED_STATE:
		case eLIM_SME_LINK_EST_STATE:
			psessionEntry->limPrevSmeState =
				psessionEntry->limSmeState;
			psessionEntry->limSmeState = eLIM_SME_WT_DISASSOC_STATE;
			/* Delete all TDLS peers connected before leaving BSS */
			lim_delete_tdls_peers(pMac, psessionEntry);
			MTRACE(mac_trace(pMac, TRACE_CODE_SME_STATE,
				psessionEntry->peSessionId,
				psessionEntry->limSmeState));
			break;

		case eLIM_SME_WT_DEAUTH_STATE:
			/* PE shall still process the DISASSOC_REQ and proceed with
			 * link tear down even if it had already sent a DEAUTH_IND to
			 * to SME. pMac->lim.gLimPrevSmeState shall remain the same as
			 * its been set when PE entered WT_DEAUTH_STATE.
			 */
			psessionEntry->limSmeState = eLIM_SME_WT_DISASSOC_STATE;
			MTRACE(mac_trace
				       (pMac, TRACE_CODE_SME_STATE,
				       psessionEntry->peSessionId,
				       psessionEntry->limSmeState));
			break;

		case eLIM_SME_WT_DISASSOC_STATE:
			/* PE Received a Disassoc frame. Normally it gets DISASSOC_CNF but it
			 * received DISASSOC_REQ. Which means host is also trying to disconnect.
			 * PE can continue processing DISASSOC_REQ and send the response instead
			 * of failing the request. SME will anyway ignore DEAUTH_IND that was sent
			 * for disassoc frame.
			 *
			 * It will send a disassoc, which is ok. However, we can use the global flag
			 * sendDisassoc to not send disassoc frame.
			 */
			break;

		case eLIM_SME_JOIN_FAILURE_STATE: {
			/* Already in Disconnected State, return success */
			if (pMac->lim.gLimRspReqd) {
				retCode = eSIR_SME_SUCCESS;
				disassocTrigger = eLIM_HOST_DISASSOC;
				goto sendDisassoc;
			}
		}
		break;
		default:
			/**
			 * STA is not currently associated.
			 * Log error and send response to host
			 */
			pe_err("received unexpected SME_DISASSOC_REQ in state %X",
				psessionEntry->limSmeState);
			lim_print_sme_state(pMac, LOGE,
				psessionEntry->limSmeState);

			if (pMac->lim.gLimRspReqd) {
				if (psessionEntry->limSmeState !=
				    eLIM_SME_WT_ASSOC_STATE)
					pMac->lim.gLimRspReqd = false;

				retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
				disassocTrigger = eLIM_HOST_DISASSOC;
				goto sendDisassoc;
			}

			return;
		}

		break;

	case eLIM_AP_ROLE:
		/* Fall through */
		break;

	case eLIM_STA_IN_IBSS_ROLE:
	default:
		/* eLIM_UNKNOWN_ROLE */
		pe_err("received unexpected SME_DISASSOC_REQ for role %d",
			GET_LIM_SYSTEM_ROLE(psessionEntry));

		retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		disassocTrigger = eLIM_HOST_DISASSOC;
		goto sendDisassoc;
	} /* end switch (pMac->lim.gLimSystemRole) */

	disassocTrigger = eLIM_HOST_DISASSOC;
	reasonCode = smeDisassocReq.reasonCode;

	if (smeDisassocReq.doNotSendOverTheAir)
		send_disassoc_frame = 0;

	pMlmDisassocReq = qdf_mem_malloc(sizeof(tLimMlmDisassocReq));
	if (NULL == pMlmDisassocReq) {
		pe_err("call to AllocateMemory failed for mlmDisassocReq");
		return;
	}

	qdf_copy_macaddr(&pMlmDisassocReq->peer_macaddr,
			 &smeDisassocReq.peer_macaddr);

	pMlmDisassocReq->reasonCode = reasonCode;
	pMlmDisassocReq->disassocTrigger = disassocTrigger;

	/* Update PE session ID */
	pMlmDisassocReq->sessionId = sessionId;

	lim_post_mlm_message(pMac,
			     LIM_MLM_DISASSOC_REQ, (uint32_t *) pMlmDisassocReq);
	return;

sendDisassoc:
	if (psessionEntry)
		lim_send_sme_disassoc_ntf(pMac,
					  smeDisassocReq.peer_macaddr.bytes,
					  retCode,
					  disassocTrigger,
					  1, smesessionId, smetransactionId,
					  psessionEntry);
	else
		lim_send_sme_disassoc_ntf(pMac,
					  smeDisassocReq.peer_macaddr.bytes,
					  retCode, disassocTrigger, 1,
					  smesessionId, smetransactionId, NULL);

} /*** end __lim_process_sme_disassoc_req() ***/

/** -----------------------------------------------------------------
   \brief __lim_process_sme_disassoc_cnf() - Process SME_DISASSOC_CNF

   This function is called to process SME_DISASSOC_CNF message
   from HDD or upper layer application.

   \param pMac - global mac structure
   \param pStaDs - station dph hash node
   \return none
   \sa
   ----------------------------------------------------------------- */
static void __lim_process_sme_disassoc_cnf(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tSirSmeDisassocCnf smeDisassocCnf;
	uint16_t aid;
	tpDphHashNode pStaDs;
	tpPESession psessionEntry;
	uint8_t sessionId;
	uint32_t *msg = NULL;
	QDF_STATUS status;

	qdf_mem_copy(&smeDisassocCnf, pMsgBuf,
			sizeof(struct sSirSmeDisassocCnf));

	psessionEntry = pe_find_session_by_bssid(pMac,
				smeDisassocCnf.bssid.bytes,
				&sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given bssId");
		status = lim_prepare_disconnect_done_ind(pMac, &msg,
						smeDisassocCnf.sme_session_id,
						eSIR_SME_INVALID_SESSION,
						NULL);
		if (QDF_IS_STATUS_SUCCESS(status))
			lim_send_sme_disassoc_deauth_ntf(pMac,
							 QDF_STATUS_SUCCESS,
							 (uint32_t *)msg);
		return;
	}

	if (!lim_is_sme_disassoc_cnf_valid(pMac, &smeDisassocCnf, psessionEntry)) {
		pe_err("received invalid SME_DISASSOC_CNF message");
		status = lim_prepare_disconnect_done_ind(pMac, &msg,
						psessionEntry->smeSessionId,
						eSIR_SME_INVALID_PARAMETERS,
						&smeDisassocCnf.bssid.bytes[0]);
		if (QDF_IS_STATUS_SUCCESS(status))
			lim_send_sme_disassoc_deauth_ntf(pMac,
							 QDF_STATUS_SUCCESS,
							 (uint32_t *)msg);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	if (smeDisassocCnf.messageType == eWNI_SME_DISASSOC_CNF)
		lim_diag_event_report(pMac, WLAN_PE_DIAG_DISASSOC_CNF_EVENT,
				      psessionEntry,
				      (uint16_t) smeDisassocCnf.statusCode, 0);
	else if (smeDisassocCnf.messageType == eWNI_SME_DEAUTH_CNF)
		lim_diag_event_report(pMac, WLAN_PE_DIAG_DEAUTH_CNF_EVENT,
				      psessionEntry,
				      (uint16_t) smeDisassocCnf.statusCode, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	switch (GET_LIM_SYSTEM_ROLE(psessionEntry)) {
	case eLIM_STA_ROLE:
		if ((psessionEntry->limSmeState != eLIM_SME_IDLE_STATE) &&
		    (psessionEntry->limSmeState != eLIM_SME_WT_DISASSOC_STATE)
		    && (psessionEntry->limSmeState !=
			eLIM_SME_WT_DEAUTH_STATE)) {
			pe_err("received unexp SME_DISASSOC_CNF in state %X",
				psessionEntry->limSmeState);
			lim_print_sme_state(pMac, LOGE,
					    psessionEntry->limSmeState);
			status = lim_prepare_disconnect_done_ind(pMac, &msg,
						psessionEntry->smeSessionId,
						eSIR_SME_INVALID_STATE,
						&smeDisassocCnf.bssid.bytes[0]);
			if (QDF_IS_STATUS_SUCCESS(status))
				lim_send_sme_disassoc_deauth_ntf(pMac,
							QDF_STATUS_SUCCESS,
							(uint32_t *)msg);
			return;
		}
		break;

	case eLIM_AP_ROLE:
		/* Fall through */
		break;

	case eLIM_STA_IN_IBSS_ROLE:
	default:                /* eLIM_UNKNOWN_ROLE */
		pe_err("received unexpected SME_DISASSOC_CNF role %d",
			GET_LIM_SYSTEM_ROLE(psessionEntry));
		status = lim_prepare_disconnect_done_ind(pMac, &msg,
						psessionEntry->smeSessionId,
						eSIR_SME_INVALID_STATE,
						&smeDisassocCnf.bssid.bytes[0]);
		if (QDF_IS_STATUS_SUCCESS(status))
			lim_send_sme_disassoc_deauth_ntf(pMac,
							 QDF_STATUS_SUCCESS,
							 (uint32_t *)msg);
		return;
	}

	if ((psessionEntry->limSmeState == eLIM_SME_WT_DISASSOC_STATE) ||
	    (psessionEntry->limSmeState == eLIM_SME_WT_DEAUTH_STATE) ||
	    LIM_IS_AP_ROLE(psessionEntry)) {
		pStaDs = dph_lookup_hash_entry(pMac,
				smeDisassocCnf.peer_macaddr.bytes, &aid,
				&psessionEntry->dph.dphHashTable);
		if (pStaDs == NULL) {
			pe_err("DISASSOC_CNF for a STA with no context, addr= "
				MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(smeDisassocCnf.peer_macaddr.bytes));
			status = lim_prepare_disconnect_done_ind(pMac, &msg,
						psessionEntry->smeSessionId,
						eSIR_SME_INVALID_PARAMETERS,
						&smeDisassocCnf.bssid.bytes[0]);
			if (QDF_IS_STATUS_SUCCESS(status))
				lim_send_sme_disassoc_deauth_ntf(pMac,
							QDF_STATUS_SUCCESS,
							(uint32_t *)msg);
			return;
		}

		if ((pStaDs->mlmStaContext.mlmState ==
				eLIM_MLM_WT_DEL_STA_RSP_STATE) ||
			(pStaDs->mlmStaContext.mlmState ==
				eLIM_MLM_WT_DEL_BSS_RSP_STATE)) {
			pe_err("No need of cleanup for addr:" MAC_ADDRESS_STR "as MLM state is %d",
				MAC_ADDR_ARRAY(smeDisassocCnf.peer_macaddr.bytes),
				pStaDs->mlmStaContext.mlmState);
			status = lim_prepare_disconnect_done_ind(pMac, &msg,
						psessionEntry->smeSessionId,
						eSIR_SME_SUCCESS,
						NULL);
			if (QDF_IS_STATUS_SUCCESS(status))
				lim_send_sme_disassoc_deauth_ntf(pMac,
							QDF_STATUS_SUCCESS,
							(uint32_t *)msg);
			return;
		}

		/* Delete FT session if there exists one */
		lim_ft_cleanup_pre_auth_info(pMac, psessionEntry);
		lim_cleanup_rx_path(pMac, pStaDs, psessionEntry);

		lim_clean_up_disassoc_deauth_req(pMac,
				 (char *)&smeDisassocCnf.peer_macaddr, 0);
	}

	return;
}

/**
 * __lim_process_sme_deauth_req() - process sme deauth req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_DEAUTH_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */

static void __lim_process_sme_deauth_req(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	uint16_t deauth_trigger, reason_code;
	tLimMlmDeauthReq *mlm_deauth_req;
	tSirSmeDeauthReq sme_deauth_req;
	tSirResultCodes ret_code = eSIR_SME_SUCCESS;
	tpPESession session_entry;
	uint8_t session_id;      /* PE sessionId */
	uint8_t sme_session_id;
	uint16_t sme_transaction_id;

	qdf_mem_copy(&sme_deauth_req, msg_buf, sizeof(tSirSmeDeauthReq));
	sme_session_id = sme_deauth_req.sessionId;
	sme_transaction_id = sme_deauth_req.transactionId;

	/*
	 * We need to get a session first but we don't even know
	 * if the message is correct.
	 */
	session_entry = pe_find_session_by_bssid(mac_ctx,
					sme_deauth_req.bssid.bytes,
					&session_id);
	if (session_entry == NULL) {
		pe_err("session does not exist for given bssId");
		ret_code = eSIR_SME_INVALID_PARAMETERS;
		deauth_trigger = eLIM_HOST_DEAUTH;
		goto send_deauth;
	}

	if (!lim_is_sme_deauth_req_valid(mac_ctx, &sme_deauth_req,
				session_entry)) {
		pe_err("received invalid SME_DEAUTH_REQ message");
		mac_ctx->lim.gLimRspReqd = false;

		ret_code = eSIR_SME_INVALID_PARAMETERS;
		deauth_trigger = eLIM_HOST_DEAUTH;
		goto send_deauth;
	}
	pe_debug("vdev %d (session %d) Systemrole %d reasoncode %u limSmestate %d limMlmState %d from "
		 QDF_MAC_ADDR_STR, sme_session_id, session_entry->peSessionId,
		 GET_LIM_SYSTEM_ROLE(session_entry), sme_deauth_req.reasonCode,
		 session_entry->limSmeState, session_entry->limMlmState,
		 QDF_MAC_ADDR_ARRAY(sme_deauth_req.peer_macaddr.bytes));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_DEAUTH_REQ_EVENT,
			session_entry, 0, sme_deauth_req.reasonCode);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	/* Update SME session ID and Transaction ID */
	session_entry->smeSessionId = sme_session_id;
	session_entry->transactionId = sme_transaction_id;

	switch (GET_LIM_SYSTEM_ROLE(session_entry)) {
	case eLIM_STA_ROLE:
		switch (session_entry->limSmeState) {
		case eLIM_SME_ASSOCIATED_STATE:
		case eLIM_SME_LINK_EST_STATE:
			/* Delete all TDLS peers connected before leaving BSS */
			lim_delete_tdls_peers(mac_ctx, session_entry);
		/* fallthrough */
		case eLIM_SME_WT_ASSOC_STATE:
		case eLIM_SME_JOIN_FAILURE_STATE:
		case eLIM_SME_IDLE_STATE:
			session_entry->limPrevSmeState =
				session_entry->limSmeState;
			session_entry->limSmeState = eLIM_SME_WT_DEAUTH_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				       session_entry->peSessionId,
				       session_entry->limSmeState));
			/* Send Deauthentication request to MLM below */
			break;
		case eLIM_SME_WT_DEAUTH_STATE:
		case eLIM_SME_WT_DISASSOC_STATE:
			/*
			 * PE Received a Deauth/Disassoc frame. Normally it get
			 * DEAUTH_CNF/DISASSOC_CNF but it received DEAUTH_REQ.
			 * Which means host is also trying to disconnect.
			 * PE can continue processing DEAUTH_REQ and send
			 * the response instead of failing the request.
			 * SME will anyway ignore DEAUTH_IND/DISASSOC_IND that
			 * was sent for deauth/disassoc frame.
			 */
			session_entry->limSmeState = eLIM_SME_WT_DEAUTH_STATE;
			break;
		default:
			/*
			 * STA is not in a state to deauthenticate with
			 * peer. Log error and send response to host.
			 */
			pe_err("received unexp SME_DEAUTH_REQ in state %X",
				session_entry->limSmeState);
			lim_print_sme_state(mac_ctx, LOGE,
					    session_entry->limSmeState);

			if (mac_ctx->lim.gLimRspReqd) {
				mac_ctx->lim.gLimRspReqd = false;

				ret_code = eSIR_SME_STA_NOT_AUTHENTICATED;
				deauth_trigger = eLIM_HOST_DEAUTH;

				/*
				 * here we received deauth request from AP so
				 * sme state is eLIM_SME_WT_DEAUTH_STATE.if we
				 * have ISSUED delSta then mlm state should be
				 * eLIM_MLM_WT_DEL_STA_RSP_STATE and ifwe got
				 * delBSS rsp then mlm state should be
				 * eLIM_MLM_IDLE_STATE so the below condition
				 * captures the state where delSta not done
				 * and firmware still in connected state.
				 */
				if (session_entry->limSmeState ==
					eLIM_SME_WT_DEAUTH_STATE &&
					session_entry->limMlmState !=
					eLIM_MLM_IDLE_STATE &&
					session_entry->limMlmState !=
					eLIM_MLM_WT_DEL_STA_RSP_STATE)
					ret_code = eSIR_SME_DEAUTH_STATUS;
				goto send_deauth;
			}
			return;
		}
		break;

	case eLIM_STA_IN_IBSS_ROLE:
		pe_err("Deauth not allowed in IBSS");
		if (mac_ctx->lim.gLimRspReqd) {
			mac_ctx->lim.gLimRspReqd = false;
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			deauth_trigger = eLIM_HOST_DEAUTH;
			goto send_deauth;
		}
		return;
	case eLIM_AP_ROLE:
		break;
	default:
		pe_err("received unexpected SME_DEAUTH_REQ for role %X",
			GET_LIM_SYSTEM_ROLE(session_entry));
		if (mac_ctx->lim.gLimRspReqd) {
			mac_ctx->lim.gLimRspReqd = false;
			ret_code = eSIR_SME_INVALID_PARAMETERS;
			deauth_trigger = eLIM_HOST_DEAUTH;
			goto send_deauth;
		}
		return;
	} /* end switch (mac_ctx->lim.gLimSystemRole) */

	if (sme_deauth_req.reasonCode == eLIM_LINK_MONITORING_DEAUTH) {
		/* Deauthentication is triggered by Link Monitoring */
		pe_debug("** Lost link with AP **");
		deauth_trigger = eLIM_LINK_MONITORING_DEAUTH;
		reason_code = eSIR_MAC_UNSPEC_FAILURE_REASON;
	} else {
		deauth_trigger = eLIM_HOST_DEAUTH;
		reason_code = sme_deauth_req.reasonCode;
	}

	/* Trigger Deauthentication frame to peer MAC entity */
	mlm_deauth_req = qdf_mem_malloc(sizeof(tLimMlmDeauthReq));
	if (NULL == mlm_deauth_req) {
		pe_err("call to AllocateMemory failed for mlmDeauthReq");
		if (mac_ctx->lim.gLimRspReqd) {
			mac_ctx->lim.gLimRspReqd = false;
			ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
			deauth_trigger = eLIM_HOST_DEAUTH;
			goto send_deauth;
		}
		return;
	}

	qdf_copy_macaddr(&mlm_deauth_req->peer_macaddr,
			 &sme_deauth_req.peer_macaddr);

	mlm_deauth_req->reasonCode = reason_code;
	mlm_deauth_req->deauthTrigger = deauth_trigger;

	/* Update PE session Id */
	mlm_deauth_req->sessionId = session_id;

	lim_post_mlm_message(mac_ctx, LIM_MLM_DEAUTH_REQ,
			(uint32_t *)mlm_deauth_req);
	return;

send_deauth:
	lim_send_sme_deauth_ntf(mac_ctx, sme_deauth_req.peer_macaddr.bytes,
				ret_code, deauth_trigger, 1,
				sme_session_id, sme_transaction_id);
}

/**
 * __lim_process_sme_set_context_req()
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_SETCONTEXT_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */

static void
__lim_process_sme_set_context_req(tpAniSirGlobal mac_ctx, uint32_t *msg_buf)
{
	tpSirSmeSetContextReq set_context_req;
	tLimMlmSetKeysReq *mlm_set_key_req;
	tpPESession session_entry;
	uint8_t session_id;      /* PE sessionID */
	uint8_t sme_session_id;
	uint16_t sme_transaction_id;

	if (msg_buf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	set_context_req = qdf_mem_malloc(sizeof(struct sSirSmeSetContextReq));
	if (NULL == set_context_req) {
		pe_err("call to AllocateMemory failed for set_context_req");
		return;
	}
	qdf_mem_copy(set_context_req, msg_buf,
			sizeof(struct sSirSmeSetContextReq));
	qdf_mem_zero(msg_buf, sizeof(struct sSirSmeSetContextReq));
	sme_session_id = set_context_req->sessionId;
	sme_transaction_id = set_context_req->transactionId;

	if ((!lim_is_sme_set_context_req_valid(mac_ctx, set_context_req))) {
		pe_warn("received invalid SME_SETCONTEXT_REQ message");
		goto end;
	}

	if (set_context_req->keyMaterial.numKeys >
			SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS) {
		pe_err("numKeys:%d is more than SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS",
					set_context_req->keyMaterial.numKeys);
		lim_send_sme_set_context_rsp(mac_ctx,
				set_context_req->peer_macaddr, 1,
				eSIR_SME_INVALID_PARAMETERS, NULL,
				sme_session_id, sme_transaction_id);
		goto end;
	}

	session_entry = pe_find_session_by_bssid(mac_ctx,
			set_context_req->bssid.bytes, &session_id);
	if (session_entry == NULL) {
		pe_err("Session does not exist for given BSSID");
		lim_send_sme_set_context_rsp(mac_ctx,
				set_context_req->peer_macaddr, 1,
				eSIR_SME_INVALID_PARAMETERS, NULL,
				sme_session_id, sme_transaction_id);
		goto end;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_SETCONTEXT_REQ_EVENT,
			      session_entry, 0, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	if ((LIM_IS_STA_ROLE(session_entry) &&
	    (session_entry->limSmeState == eLIM_SME_LINK_EST_STATE)) ||
	    ((LIM_IS_IBSS_ROLE(session_entry) ||
	    LIM_IS_AP_ROLE(session_entry)) &&
	    (session_entry->limSmeState == eLIM_SME_NORMAL_STATE))) {
		/* Trigger MLM_SETKEYS_REQ */
		mlm_set_key_req = qdf_mem_malloc(sizeof(tLimMlmSetKeysReq));
		if (NULL == mlm_set_key_req) {
			pe_err("mem alloc failed for mlmSetKeysReq");
			goto end;
		}
		mlm_set_key_req->edType = set_context_req->keyMaterial.edType;
		mlm_set_key_req->numKeys =
			set_context_req->keyMaterial.numKeys;
		if (mlm_set_key_req->numKeys >
				SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS) {
			pe_err("no.of keys exceeded max num of default keys limit");
			qdf_mem_free(mlm_set_key_req);
			goto end;
		}
		qdf_copy_macaddr(&mlm_set_key_req->peer_macaddr,
				 &set_context_req->peer_macaddr);

		qdf_mem_copy((uint8_t *) &mlm_set_key_req->key,
			     (uint8_t *) &set_context_req->keyMaterial.key,
			     sizeof(tSirKeys) *
			     (mlm_set_key_req->numKeys ? mlm_set_key_req->
			      numKeys : 1));

		mlm_set_key_req->sessionId = session_id;
		mlm_set_key_req->smesessionId = sme_session_id;
		pe_debug("received SETCONTEXT_REQ message sessionId=%d",
					mlm_set_key_req->sessionId);

		if (((set_context_req->keyMaterial.edType == eSIR_ED_WEP40) ||
		    (set_context_req->keyMaterial.edType == eSIR_ED_WEP104)) &&
		    LIM_IS_AP_ROLE(session_entry)) {
			if (set_context_req->keyMaterial.key[0].keyLength) {
				uint8_t key_id;

				key_id =
					set_context_req->keyMaterial.key[0].keyId;
				qdf_mem_copy((uint8_t *)
					&session_entry->WEPKeyMaterial[key_id],
					(uint8_t *) &set_context_req->keyMaterial,
					sizeof(tSirKeyMaterial));
			} else {
				uint32_t i;

				for (i = 0; i < SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS;
				     i++) {
					qdf_mem_copy((uint8_t *)
						&mlm_set_key_req->key[i],
						(uint8_t *)session_entry->WEPKeyMaterial[i].key,
						sizeof(tSirKeys));
				}
			}
		}
		lim_post_mlm_message(mac_ctx, LIM_MLM_SETKEYS_REQ,
				     (uint32_t *) mlm_set_key_req);
	} else {
		pe_err("rcvd unexpected SME_SETCONTEXT_REQ for role %d, state=%X",
				GET_LIM_SYSTEM_ROLE(session_entry),
				session_entry->limSmeState);
		lim_print_sme_state(mac_ctx, LOGE, session_entry->limSmeState);

		lim_send_sme_set_context_rsp(mac_ctx,
				set_context_req->peer_macaddr, 1,
				eSIR_SME_UNEXPECTED_REQ_RESULT_CODE,
				session_entry, sme_session_id,
				sme_transaction_id);
	}
end:
	qdf_mem_zero(set_context_req, sizeof(struct sSirSmeSetContextReq));
	qdf_mem_free(set_context_req);
	return;
}

/**
 * lim_process_sme_get_assoc_sta_info() - process sme assoc sta req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_GET_ASSOC_STAS_REQ message
 * from HDD or upper layer application.
 *
 * Return: None
 */

static void lim_process_sme_get_assoc_sta_info(tpAniSirGlobal mac_ctx,
					       uint32_t *msg_buf)
{
	tSirSmeGetAssocSTAsReq get_assoc_stas_req;
	tpDphHashNode sta_ds = NULL;
	tpPESession session_entry = NULL;
	tSap_Event sap_event;
	tpWLAN_SAPEventCB sap_event_cb = NULL;
	tpSap_AssocMacAddr assoc_sta_tmp = NULL;
	uint8_t session_id = CSR_SESSION_ID_INVALID;
	uint8_t assoc_id = 0;
	uint8_t sta_cnt = 0;

	if (msg_buf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	qdf_mem_copy(&get_assoc_stas_req, msg_buf,
				sizeof(struct sSirSmeGetAssocSTAsReq));
	/*
	 * Get Associated stations from PE.
	 * Find PE session Entry
	 */
	session_entry = pe_find_session_by_bssid(mac_ctx,
			get_assoc_stas_req.bssid.bytes,
			&session_id);
	if (session_entry == NULL) {
		pe_err("session does not exist for given bssId");
		goto lim_assoc_sta_end;
	}

	if (!LIM_IS_AP_ROLE(session_entry)) {
		pe_err("Received unexpected message in state %X, in role %X",
			session_entry->limSmeState,
			GET_LIM_SYSTEM_ROLE(session_entry));
		goto lim_assoc_sta_end;
	}
	/* Retrieve values obtained in the request message */
	sap_event_cb = (tpWLAN_SAPEventCB)get_assoc_stas_req.pSapEventCallback;
	assoc_sta_tmp = (tpSap_AssocMacAddr)get_assoc_stas_req.pAssocStasArray;

	if (NULL == assoc_sta_tmp)
		goto lim_assoc_sta_end;
	for (assoc_id = 0; assoc_id < session_entry->dph.dphHashTable.size;
		assoc_id++) {
		sta_ds = dph_get_hash_entry(mac_ctx, assoc_id,
				&session_entry->dph.dphHashTable);
		if (NULL == sta_ds)
			continue;
		if (sta_ds->valid) {
			qdf_mem_copy((uint8_t *) &assoc_sta_tmp->staMac,
					(uint8_t *) &sta_ds->staAddr,
					 QDF_MAC_ADDR_SIZE);
			assoc_sta_tmp->assocId = (uint8_t) sta_ds->assocId;
			assoc_sta_tmp->staId = (uint8_t) sta_ds->staIndex;

			qdf_mem_copy((uint8_t *)&assoc_sta_tmp->supportedRates,
				     (uint8_t *)&sta_ds->supportedRates,
				     sizeof(tSirSupportedRates));
			assoc_sta_tmp->ShortGI40Mhz = sta_ds->htShortGI40Mhz;
			assoc_sta_tmp->ShortGI20Mhz = sta_ds->htShortGI20Mhz;
			assoc_sta_tmp->Support40Mhz =
				sta_ds->htDsssCckRate40MHzSupport;

			pe_debug("dph Station Number = %d",
				sta_cnt + 1);
			pe_debug("MAC = " MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(sta_ds->staAddr));
			pe_debug("Association Id: %d Station Index: %d",
				sta_ds->assocId, sta_ds->staIndex);
			assoc_sta_tmp++;
			sta_cnt++;
		}
	}
lim_assoc_sta_end:
	/*
	 * Call hdd callback with sap event to send the list of
	 * associated stations from PE
	 */
	if (sap_event_cb != NULL) {
		sap_event.sapHddEventCode = eSAP_ASSOC_STA_CALLBACK_EVENT;
		sap_event.sapevt.sapAssocStaListEvent.module =
			QDF_MODULE_ID_PE;
		sap_event.sapevt.sapAssocStaListEvent.noOfAssocSta = sta_cnt;
		sap_event.sapevt.sapAssocStaListEvent.pAssocStas =
			(tpSap_AssocMacAddr)get_assoc_stas_req.pAssocStasArray;
		sap_event_cb(&sap_event, get_assoc_stas_req.pUsrContext);
	}
}

/**
 * __lim_counter_measures()
 *
 * FUNCTION:
 * This function is called to "implement" MIC counter measure
 * and is *temporary* only
 *
 * LOGIC: on AP, disassoc all STA associated thru TKIP,
 * we don't do the proper STA disassoc sequence since the
 * BSS will be stopped anyway
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void __lim_counter_measures(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
	tSirMacAddr mac = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	if (LIM_IS_AP_ROLE(psessionEntry))
		lim_send_disassoc_mgmt_frame(pMac, eSIR_MAC_MIC_FAILURE_REASON,
					     mac, psessionEntry, false);
};

static void
__lim_handle_sme_stop_bss_request(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tSirSmeStopBssReq stopBssReq;
	QDF_STATUS status;
	tLimSmeStates prevState;
	tpPESession psessionEntry;
	uint8_t smesessionId;
	uint8_t sessionId;
	uint16_t smetransactionId;
	uint8_t i = 0;
	tpDphHashNode pStaDs = NULL;

	qdf_mem_copy(&stopBssReq, pMsgBuf, sizeof(tSirSmeStopBssReq));
	smesessionId = stopBssReq.sessionId;
	smetransactionId = stopBssReq.transactionId;

	if (!lim_is_sme_stop_bss_req_valid(pMsgBuf)) {
		pe_warn("received invalid SME_STOP_BSS_REQ message");
		/* Send Stop BSS response to host */
		lim_send_sme_rsp(pMac, eWNI_SME_STOP_BSS_RSP,
				 eSIR_SME_INVALID_PARAMETERS, smesessionId,
				 smetransactionId);
		return;
	}

	psessionEntry = pe_find_session_by_bssid(pMac,
				stopBssReq.bssid.bytes,
				&sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given BSSID");
		lim_send_sme_rsp(pMac, eWNI_SME_STOP_BSS_RSP,
				 eSIR_SME_INVALID_PARAMETERS, smesessionId,
				 smetransactionId);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(pMac, WLAN_PE_DIAG_STOP_BSS_REQ_EVENT, psessionEntry,
			      0, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	if (psessionEntry->limSmeState != eLIM_SME_NORMAL_STATE ||    /* Added For BT -AMP Support */
	    LIM_IS_STA_ROLE(psessionEntry)) {
		/**
		 * Should not have received STOP_BSS_REQ in states
		 * other than 'normal' state or on STA in Infrastructure
		 * mode. Log error and return response to host.
		 */
		pe_err("received unexpected SME_STOP_BSS_REQ in state %X, for role %d",
			psessionEntry->limSmeState,
			GET_LIM_SYSTEM_ROLE(psessionEntry));
		lim_print_sme_state(pMac, LOGE, psessionEntry->limSmeState);
		/* / Send Stop BSS response to host */
		lim_send_sme_rsp(pMac, eWNI_SME_STOP_BSS_RSP,
				 eSIR_SME_UNEXPECTED_REQ_RESULT_CODE, smesessionId,
				 smetransactionId);
		return;
	}

	if (LIM_IS_AP_ROLE(psessionEntry))
		lim_wpspbc_close(pMac, psessionEntry);

	pe_debug("RECEIVED STOP_BSS_REQ with reason code=%d",
		stopBssReq.reasonCode);

	prevState = psessionEntry->limSmeState;

	psessionEntry->limSmeState = eLIM_SME_IDLE_STATE;
	MTRACE(mac_trace
		       (pMac, TRACE_CODE_SME_STATE, psessionEntry->peSessionId,
		       psessionEntry->limSmeState));

	/* Update SME session Id and Transaction Id */
	psessionEntry->smeSessionId = smesessionId;
	psessionEntry->transactionId = smetransactionId;

	/* STA_IN_IBSS and NDI should NOT send Disassoc frame */
	if (!LIM_IS_IBSS_ROLE(psessionEntry) &&
	    !LIM_IS_NDI_ROLE(psessionEntry)) {
		tSirMacAddr bcAddr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

		if (stopBssReq.reasonCode == eSIR_SME_MIC_COUNTER_MEASURES)
			/* Send disassoc all stations associated thru TKIP */
			__lim_counter_measures(pMac, psessionEntry);
		else
			lim_send_disassoc_mgmt_frame(pMac,
				eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
				bcAddr, psessionEntry, false);
	}

	if (!LIM_IS_NDI_ROLE(psessionEntry)) {
		/* Free the buffer allocated in START_BSS_REQ */
		qdf_mem_free(psessionEntry->addIeParams.probeRespData_buff);
		psessionEntry->addIeParams.probeRespDataLen = 0;
		psessionEntry->addIeParams.probeRespData_buff = NULL;

		qdf_mem_free(psessionEntry->addIeParams.assocRespData_buff);
		psessionEntry->addIeParams.assocRespDataLen = 0;
		psessionEntry->addIeParams.assocRespData_buff = NULL;

		qdf_mem_free(psessionEntry->addIeParams.probeRespBCNData_buff);
		psessionEntry->addIeParams.probeRespBCNDataLen = 0;
		psessionEntry->addIeParams.probeRespBCNData_buff = NULL;

		/*
		 * lim_del_bss is also called as part of coalescing,
		 * when we send DEL BSS followed by Add Bss msg.
		 */
		pMac->lim.gLimIbssCoalescingHappened = false;
	}
	for (i = 1; i < psessionEntry->dph.dphHashTable.size; i++) {
		pStaDs =
			dph_get_hash_entry(pMac, i, &psessionEntry->dph.dphHashTable);
		if (NULL == pStaDs)
			continue;
		status = lim_del_sta(pMac, pStaDs, false, psessionEntry);
		if (QDF_STATUS_SUCCESS == status) {
			lim_delete_dph_hash_entry(pMac, pStaDs->staAddr,
						  pStaDs->assocId, psessionEntry);
			lim_release_peer_idx(pMac, pStaDs->assocId, psessionEntry);
		} else {
			pe_err("lim_del_sta failed with Status: %d", status);
			QDF_ASSERT(0);
		}
	}
	/* send a delBss to HAL and wait for a response */
	status = lim_del_bss(pMac, NULL, psessionEntry->bssIdx, psessionEntry);

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("delBss failed for bss %d", psessionEntry->bssIdx);
		psessionEntry->limSmeState = prevState;

		MTRACE(mac_trace
			       (pMac, TRACE_CODE_SME_STATE, psessionEntry->peSessionId,
			       psessionEntry->limSmeState));

		lim_send_sme_rsp(pMac, eWNI_SME_STOP_BSS_RSP,
				 eSIR_SME_STOP_BSS_FAILURE, smesessionId,
				 smetransactionId);
	}
}

/**
 * __lim_process_sme_stop_bss_req() - Process STOP_BSS from SME
 * @pMac: Global MAC context
 * @pMsg: Message from SME
 *
 * Wrapper for the function __lim_handle_sme_stop_bss_request
 * This message will be defered until softmac come out of
 * scan mode. Message should be handled even if we have
 * detected radar in the current operating channel.
 *
 * Return: true - If we consumed the buffer
 *         false - If have defered the message.
 */

static bool __lim_process_sme_stop_bss_req(tpAniSirGlobal pMac,
					   struct scheduler_msg *pMsg)
{
	if (__lim_is_defered_msg_for_learn(pMac, pMsg)) {
		/**
		 * If message defered, buffer is not consumed yet.
		 * So return false
		 */
		return false;
	}
	__lim_handle_sme_stop_bss_request(pMac, (uint32_t *) pMsg->bodyptr);
	return true;
} /*** end __lim_process_sme_stop_bss_req() ***/

void lim_process_sme_del_bss_rsp(tpAniSirGlobal pMac,
				 uint32_t body, tpPESession psessionEntry)
{

	(void)body;
	SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
	lim_ibss_delete(pMac, psessionEntry);
	dph_hash_table_class_init(pMac, &psessionEntry->dph.dphHashTable);
	lim_delete_pre_auth_list(pMac);
	lim_send_sme_rsp(pMac, eWNI_SME_STOP_BSS_RSP, eSIR_SME_SUCCESS,
			 psessionEntry->smeSessionId,
			 psessionEntry->transactionId);
	return;
}

/**
 * __lim_process_sme_assoc_cnf_new() - process sme assoc/reassoc cnf
 *
 * @mac_ctx: pointer to mac context
 * @msg_type: message type
 * @msg_buf: pointer to the SME message buffer
 *
 * This function handles SME_ASSOC_CNF/SME_REASSOC_CNF
 * in BTAMP AP.
 *
 * Return: None
 */

void __lim_process_sme_assoc_cnf_new(tpAniSirGlobal mac_ctx, uint32_t msg_type,
				uint32_t *msg_buf)
{
	tSirSmeAssocCnf assoc_cnf;
	tpDphHashNode sta_ds = NULL;
	tpPESession session_entry = NULL;
	uint8_t session_id;
	tpSirAssocReq assoc_req;

	if (msg_buf == NULL) {
		pe_err("msg_buf is NULL");
		return;
	}

	qdf_mem_copy(&assoc_cnf, msg_buf, sizeof(struct sSirSmeAssocCnf));
	if (!__lim_is_sme_assoc_cnf_valid(&assoc_cnf)) {
		pe_err("Received invalid SME_RE(ASSOC)_CNF message");
		goto end;
	}

	session_entry = pe_find_session_by_bssid(mac_ctx, assoc_cnf.bssid.bytes,
			&session_id);
	if (session_entry == NULL) {
		pe_err("session does not exist for given bssId");
		goto end;
	}

	if ((!LIM_IS_AP_ROLE(session_entry)) ||
	    ((session_entry->limSmeState != eLIM_SME_NORMAL_STATE) &&
	    (session_entry->limSmeState !=
			eLIM_SME_NORMAL_CHANNEL_SCAN_STATE))) {
		pe_err("Rcvd unexpected msg %X in state %X, in role %X",
			msg_type, session_entry->limSmeState,
			GET_LIM_SYSTEM_ROLE(session_entry));
		goto end;
	}
	sta_ds = dph_get_hash_entry(mac_ctx, assoc_cnf.aid,
			&session_entry->dph.dphHashTable);
	if (sta_ds == NULL) {
		pe_err("Rcvd invalid msg %X due to no STA ctx, aid %d, peer",
				msg_type, assoc_cnf.aid);
		lim_print_mac_addr(mac_ctx, assoc_cnf.peer_macaddr.bytes, LOGE);

		/*
		 * send a DISASSOC_IND message to WSM to make sure
		 * the state in WSM and LIM is the same
		 */
		lim_send_sme_disassoc_ntf(mac_ctx, assoc_cnf.peer_macaddr.bytes,
				eSIR_SME_STA_NOT_ASSOCIATED,
				eLIM_PEER_ENTITY_DISASSOC, assoc_cnf.aid,
				session_entry->smeSessionId,
				session_entry->transactionId,
				session_entry);
		goto end;
	}
	if (qdf_mem_cmp((uint8_t *)sta_ds->staAddr,
				(uint8_t *) assoc_cnf.peer_macaddr.bytes,
				QDF_MAC_ADDR_SIZE)) {
		pe_debug("peerMacAddr mismatched for aid %d, peer ",
				assoc_cnf.aid);
		lim_print_mac_addr(mac_ctx, assoc_cnf.peer_macaddr.bytes, LOGD);
		goto end;
	}

	if ((sta_ds->mlmStaContext.mlmState != eLIM_MLM_WT_ASSOC_CNF_STATE) ||
		((sta_ds->mlmStaContext.subType == LIM_ASSOC) &&
		 (msg_type != eWNI_SME_ASSOC_CNF)) ||
		((sta_ds->mlmStaContext.subType == LIM_REASSOC) &&
		 (msg_type != eWNI_SME_ASSOC_CNF))) {
		pe_debug("not in MLM_WT_ASSOC_CNF_STATE, for aid %d, peer"
			"StaD mlmState: %d",
			assoc_cnf.aid, sta_ds->mlmStaContext.mlmState);
		lim_print_mac_addr(mac_ctx, assoc_cnf.peer_macaddr.bytes, LOGD);
		goto end;
	}
	/*
	 * Deactivate/delet CNF_WAIT timer since ASSOC_CNF
	 * has been received
	 */
	pe_debug("Received SME_ASSOC_CNF. Delete Timer");
	lim_deactivate_and_change_per_sta_id_timer(mac_ctx,
			eLIM_CNF_WAIT_TIMER, sta_ds->assocId);

	if (assoc_cnf.statusCode == eSIR_SME_SUCCESS) {
		/*
		 * In BTAMP-AP, PE already finished the WMA_ADD_STA sequence
		 * when it had received Assoc Request frame. Now, PE just needs
		 * to send association rsp frame to the requesting BTAMP-STA.
		 */
		sta_ds->mlmStaContext.mlmState =
			eLIM_MLM_LINK_ESTABLISHED_STATE;
		sta_ds->mlmStaContext.owe_ie = assoc_cnf.owe_ie;
		sta_ds->mlmStaContext.owe_ie_len = assoc_cnf.owe_ie_len;
		pe_debug("sending Assoc Rsp frame to STA (assoc id=%d)",
			sta_ds->assocId);
		lim_send_assoc_rsp_mgmt_frame(mac_ctx, QDF_STATUS_SUCCESS,
					sta_ds->assocId, sta_ds->staAddr,
					sta_ds->mlmStaContext.subType, sta_ds,
					session_entry);
		sta_ds->mlmStaContext.owe_ie = NULL;
		sta_ds->mlmStaContext.owe_ie_len = 0;
		goto end;
	} else {
		uint8_t add_pre_auth_context = true;
		/*
		 * SME_ASSOC_CNF status is non-success, so STA is not allowed
		 * to be associated since the HAL sta entry is created for
		 * denied STA we need to remove this HAL entry.
		 * So to do that set updateContext to 1
		 */
		tSirMacStatusCodes mac_status_code =
					eSIR_MAC_UNSPEC_FAILURE_STATUS;

		if (!sta_ds->mlmStaContext.updateContext)
			sta_ds->mlmStaContext.updateContext = 1;
		pe_debug("Recv Assoc Cnf, status Code : %d(assoc id=%d) Reason code: %d",
			 assoc_cnf.statusCode, sta_ds->assocId,
			 assoc_cnf.mac_status_code);
		if (assoc_cnf.mac_status_code)
			mac_status_code = assoc_cnf.mac_status_code;
		if (assoc_cnf.mac_status_code == eSIR_MAC_INVALID_PMKID ||
		    assoc_cnf.mac_status_code ==
			eSIR_MAC_AUTH_ALGO_NOT_SUPPORTED_STATUS)
			add_pre_auth_context = false;

		lim_reject_association(mac_ctx, sta_ds->staAddr,
				       sta_ds->mlmStaContext.subType,
				       add_pre_auth_context,
				       sta_ds->mlmStaContext.authType,
				       sta_ds->assocId, true,
				       mac_status_code,
				       session_entry);
	}
end:
	if (((session_entry != NULL) && (sta_ds != NULL)) &&
		(session_entry->parsedAssocReq[sta_ds->assocId] != NULL)) {
		assoc_req = (tpSirAssocReq)
			session_entry->parsedAssocReq[sta_ds->assocId];
		if (assoc_req->assocReqFrame) {
			qdf_mem_free(assoc_req->assocReqFrame);
			assoc_req->assocReqFrame = NULL;
		}
		qdf_mem_free(session_entry->parsedAssocReq[sta_ds->assocId]);
		session_entry->parsedAssocReq[sta_ds->assocId] = NULL;
	}
	qdf_mem_free(assoc_cnf.owe_ie);
}

static void __lim_process_sme_addts_req(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tpDphHashNode pStaDs;
	tSirMacAddr peerMac;
	tpSirAddtsReq pSirAddts;
	uint32_t timeout;
	tpPESession psessionEntry;
	uint8_t sessionId;      /* PE sessionId */
	uint8_t smesessionId;
	uint16_t smetransactionId;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	lim_get_session_info(pMac, (uint8_t *) pMsgBuf, &smesessionId,
			     &smetransactionId);

	pSirAddts = (tpSirAddtsReq) pMsgBuf;

	psessionEntry = pe_find_session_by_bssid(pMac, pSirAddts->bssid.bytes,
						 &sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given bssId");
		lim_send_sme_addts_rsp(pMac, pSirAddts->rspReqd, QDF_STATUS_E_FAILURE,
				       NULL, pSirAddts->req.tspec,
				       smesessionId, smetransactionId);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(pMac, WLAN_PE_DIAG_ADDTS_REQ_EVENT, psessionEntry, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	/* if sta
	 *  - verify assoc state
	 *  - send addts request to ap
	 *  - wait for addts response from ap
	 * if ap, just ignore with error log
	 */
	pe_debug("Received SME_ADDTS_REQ (TSid %d, UP %d)",
		pSirAddts->req.tspec.tsinfo.traffic.tsid,
		pSirAddts->req.tspec.tsinfo.traffic.userPrio);

	if (!LIM_IS_STA_ROLE(psessionEntry)) {
		pe_err("AddTs received on AP - ignoring");
		goto send_failure_addts_rsp;
	}

	pStaDs =
		dph_get_hash_entry(pMac, DPH_STA_HASH_INDEX_PEER,
				   &psessionEntry->dph.dphHashTable);

	if (pStaDs == NULL) {
		pe_err("Cannot find AP context for addts req");
		goto send_failure_addts_rsp;
	}

	if ((!pStaDs->valid) || (pStaDs->mlmStaContext.mlmState !=
	    eLIM_MLM_LINK_ESTABLISHED_STATE)) {
		pe_err("AddTs received in invalid MLM state");
		goto send_failure_addts_rsp;
	}

	pSirAddts->req.wsmTspecPresent = 0;
	pSirAddts->req.wmeTspecPresent = 0;
	pSirAddts->req.lleTspecPresent = 0;

	if ((pStaDs->wsmEnabled) &&
	    (pSirAddts->req.tspec.tsinfo.traffic.accessPolicy !=
	     SIR_MAC_ACCESSPOLICY_EDCA))
		pSirAddts->req.wsmTspecPresent = 1;
	else if (pStaDs->wmeEnabled)
		pSirAddts->req.wmeTspecPresent = 1;
	else if (pStaDs->lleEnabled)
		pSirAddts->req.lleTspecPresent = 1;
	else {
		pe_warn("ADDTS_REQ ignore - qos is disabled");
		goto send_failure_addts_rsp;
	}

	if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
	    (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE)) {
		pe_err("AddTs received in invalid LIMsme state (%d)",
			psessionEntry->limSmeState);
		goto send_failure_addts_rsp;
	}

	if (pMac->lim.gLimAddtsSent) {
		pe_err("Addts (token %d, tsid %d, up %d) is still pending",
			pMac->lim.gLimAddtsReq.req.dialogToken,
			pMac->lim.gLimAddtsReq.req.tspec.tsinfo.traffic.tsid,
			pMac->lim.gLimAddtsReq.req.tspec.tsinfo.traffic.
			userPrio);
		goto send_failure_addts_rsp;
	}

	sir_copy_mac_addr(peerMac, psessionEntry->bssId);

	/* save the addts request */
	pMac->lim.gLimAddtsSent = true;
	qdf_mem_copy((uint8_t *) &pMac->lim.gLimAddtsReq,
		     (uint8_t *) pSirAddts, sizeof(tSirAddtsReq));

	/* ship out the message now */
	lim_send_addts_req_action_frame(pMac, peerMac, &pSirAddts->req,
					psessionEntry);
	pe_err("Sent ADDTS request");
	/* start a timer to wait for the response */
	if (pSirAddts->timeout)
		timeout = pSirAddts->timeout;
	else if (wlan_cfg_get_int(pMac, WNI_CFG_ADDTS_RSP_TIMEOUT, &timeout) !=
		 QDF_STATUS_SUCCESS) {
		pe_debug("Unable to get Cfg param %d (Addts Rsp Timeout)",
			WNI_CFG_ADDTS_RSP_TIMEOUT);
		goto send_failure_addts_rsp;
	}

	timeout = SYS_MS_TO_TICKS(timeout);
	if (tx_timer_change(&pMac->lim.limTimers.gLimAddtsRspTimer, timeout, 0)
	    != TX_SUCCESS) {
		pe_err("AddtsRsp timer change failed!");
		goto send_failure_addts_rsp;
	}
	pMac->lim.gLimAddtsRspTimerCount++;
	if (tx_timer_change_context(&pMac->lim.limTimers.gLimAddtsRspTimer,
				    pMac->lim.gLimAddtsRspTimerCount) !=
	    TX_SUCCESS) {
		pe_err("AddtsRsp timer change failed!");
		goto send_failure_addts_rsp;
	}
	MTRACE(mac_trace
		       (pMac, TRACE_CODE_TIMER_ACTIVATE, psessionEntry->peSessionId,
		       eLIM_ADDTS_RSP_TIMER));

	/* add the sessionId to the timer object */
	pMac->lim.limTimers.gLimAddtsRspTimer.sessionId = sessionId;
	if (tx_timer_activate(&pMac->lim.limTimers.gLimAddtsRspTimer) !=
	    TX_SUCCESS) {
		pe_err("AddtsRsp timer activation failed!");
		goto send_failure_addts_rsp;
	}
	return;

send_failure_addts_rsp:
	lim_send_sme_addts_rsp(pMac, pSirAddts->rspReqd, QDF_STATUS_E_FAILURE,
			       psessionEntry, pSirAddts->req.tspec,
			       smesessionId, smetransactionId);
}

static void __lim_process_sme_delts_req(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tSirMacAddr peerMacAddr;
	uint8_t ac;
	tSirMacTSInfo *pTsinfo;
	tpSirDeltsReq pDeltsReq = (tpSirDeltsReq) pMsgBuf;
	tpDphHashNode pStaDs = NULL;
	tpPESession psessionEntry;
	uint8_t sessionId;
	uint32_t status = QDF_STATUS_SUCCESS;
	uint8_t smesessionId;
	uint16_t smetransactionId;

	lim_get_session_info(pMac, (uint8_t *) pMsgBuf, &smesessionId,
			     &smetransactionId);

	psessionEntry = pe_find_session_by_bssid(pMac,
				pDeltsReq->bssid.bytes,
				&sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given bssId");
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(pMac, WLAN_PE_DIAG_DELTS_REQ_EVENT, psessionEntry, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	if (QDF_STATUS_SUCCESS !=
	    lim_validate_delts_req(pMac, pDeltsReq, peerMacAddr, psessionEntry)) {
		pe_err("lim_validate_delts_req failed");
		status = QDF_STATUS_E_FAILURE;
		lim_send_sme_delts_rsp(pMac, pDeltsReq, QDF_STATUS_E_FAILURE, psessionEntry,
				       smesessionId, smetransactionId);
		return;
	}

	pe_debug("Sent DELTS request to station with assocId = %d MacAddr = "
		MAC_ADDRESS_STR,
		pDeltsReq->aid, MAC_ADDR_ARRAY(peerMacAddr));

	lim_send_delts_req_action_frame(pMac, peerMacAddr,
					pDeltsReq->req.wmeTspecPresent,
					&pDeltsReq->req.tsinfo,
					&pDeltsReq->req.tspec, psessionEntry);

	pTsinfo =
		pDeltsReq->req.wmeTspecPresent ? &pDeltsReq->req.tspec.
		tsinfo : &pDeltsReq->req.tsinfo;

	/* We've successfully send DELTS frame to AP. Update the
	 * dynamic UAPSD mask. The AC for this TSPEC to be deleted
	 * is no longer trigger enabled or delivery enabled
	 */
	lim_set_tspec_uapsd_mask_per_session(pMac, psessionEntry,
					     pTsinfo, CLEAR_UAPSD_MASK);

	/* We're deleting the TSPEC, so this particular AC is no longer
	 * admitted.  PE needs to downgrade the EDCA
	 * parameters(for the AC for which TS is being deleted) to the
	 * next best AC for which ACM is not enabled, and send the
	 * updated values to HAL.
	 */
	ac = upToAc(pTsinfo->traffic.userPrio);

	if (pTsinfo->traffic.direction == SIR_MAC_DIRECTION_UPLINK) {
		psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &=
			~(1 << ac);
	} else if (pTsinfo->traffic.direction ==
		   SIR_MAC_DIRECTION_DNLINK) {
		psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &=
			~(1 << ac);
	} else if (pTsinfo->traffic.direction ==
		   SIR_MAC_DIRECTION_BIDIR) {
		psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &=
			~(1 << ac);
		psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &=
			~(1 << ac);
	}

	lim_set_active_edca_params(pMac, psessionEntry->gLimEdcaParams,
				   psessionEntry);

	pStaDs =
		dph_get_hash_entry(pMac, DPH_STA_HASH_INDEX_PEER,
				   &psessionEntry->dph.dphHashTable);
	if (pStaDs != NULL) {
		lim_send_edca_params(pMac, psessionEntry->gLimEdcaParamsActive,
				     pStaDs->bssId, false);
		status = QDF_STATUS_SUCCESS;
	} else {
		pe_err("Self entry missing in Hash Table");
		status = QDF_STATUS_E_FAILURE;
	}
#ifdef FEATURE_WLAN_ESE
	lim_send_sme_tsm_ie_ind(pMac, psessionEntry, 0, 0, 0);
#endif

	/* send an sme response back */
end:
	lim_send_sme_delts_rsp(pMac, pDeltsReq, QDF_STATUS_SUCCESS, psessionEntry,
			       smesessionId, smetransactionId);
}

void lim_process_sme_addts_rsp_timeout(tpAniSirGlobal pMac, uint32_t param)
{
	/* fetch the sessionEntry based on the sessionId */
	tpPESession psessionEntry;

	psessionEntry = pe_find_session_by_session_id(pMac,
				pMac->lim.limTimers.gLimAddtsRspTimer.
				sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}

	if (!LIM_IS_STA_ROLE(psessionEntry)) {
		pe_warn("AddtsRspTimeout in non-Sta role (%d)",
			GET_LIM_SYSTEM_ROLE(psessionEntry));
		pMac->lim.gLimAddtsSent = false;
		return;
	}

	if (!pMac->lim.gLimAddtsSent) {
		pe_warn("AddtsRspTimeout but no AddtsSent");
		return;
	}

	if (param != pMac->lim.gLimAddtsRspTimerCount) {
		pe_err("Invalid AddtsRsp Timer count %d (exp %d)", param,
			pMac->lim.gLimAddtsRspTimerCount);
		return;
	}
	/* this a real response timeout */
	pMac->lim.gLimAddtsSent = false;
	pMac->lim.gLimAddtsRspTimerCount++;

	lim_send_sme_addts_rsp(pMac, true, eSIR_SME_ADDTS_RSP_TIMEOUT,
			       psessionEntry, pMac->lim.gLimAddtsReq.req.tspec,
			       psessionEntry->smeSessionId,
			       psessionEntry->transactionId);
}

#ifndef QCA_SUPPORT_CP_STATS
/**
 * __lim_process_sme_get_statistics_request()
 *
 ***FUNCTION:
 *
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */
static void
__lim_process_sme_get_statistics_request(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tpAniGetPEStatsReq pPEStatsReq;
	struct scheduler_msg msgQ = {0};

	pPEStatsReq = (tpAniGetPEStatsReq) pMsgBuf;

	msgQ.type = WMA_GET_STATISTICS_REQ;

	msgQ.reserved = 0;
	msgQ.bodyptr = pMsgBuf;
	msgQ.bodyval = 0;
	MTRACE(mac_trace_msg_tx(pMac, NO_SESSION, msgQ.type));

	if (QDF_STATUS_SUCCESS != (wma_post_ctrl_msg(pMac, &msgQ))) {
		qdf_mem_free(pMsgBuf);
		pMsgBuf = NULL;
		pe_err("Unable to forward request");
		return;
	}

	return;
}
#else
static void __lim_process_sme_get_statistics_request(
			struct sAniSirGlobal *mac_ctx, uint32_t *pMsgBuf) {}
#endif

#ifdef FEATURE_WLAN_ESE
/**
 * __lim_process_sme_get_tsm_stats_request() - get tsm stats request
 *
 * @pMac: Pointer to Global MAC structure
 * @pMsgBuf: A pointer to the SME message buffer
 *
 * Return: None
 */
static void
__lim_process_sme_get_tsm_stats_request(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	struct scheduler_msg msgQ = {0};

	msgQ.type = WMA_TSM_STATS_REQ;
	msgQ.reserved = 0;
	msgQ.bodyptr = pMsgBuf;
	msgQ.bodyval = 0;
	MTRACE(mac_trace_msg_tx(pMac, NO_SESSION, msgQ.type));

	if (QDF_STATUS_SUCCESS != (wma_post_ctrl_msg(pMac, &msgQ))) {
		qdf_mem_free(pMsgBuf);
		pMsgBuf = NULL;
		pe_err("Unable to forward request");
		return;
	}
}
#endif /* FEATURE_WLAN_ESE */

static void lim_process_sme_set_addba_accept(tpAniSirGlobal mac_ctx,
		struct sme_addba_accept *msg)
{
	if (!msg) {
		pe_err("Msg Buffer is NULL");
		return;
	}
	if (!msg->addba_accept)
		mac_ctx->reject_addba_req = 1;
	else
		mac_ctx->reject_addba_req = 0;
}

static void lim_process_sme_update_edca_params(tpAniSirGlobal mac_ctx,
					       uint32_t sme_session_id)
{
	tpPESession pe_session;
	tpDphHashNode sta_ds_ptr;

	pe_session = pe_find_session_by_sme_session_id(mac_ctx, sme_session_id);
	if (!pe_session) {
		pe_err("Session does not exist: sme_id %d", sme_session_id);
		return;
	}
	pe_session->gLimEdcaParamsActive[EDCA_AC_BE].no_ack =
		mac_ctx->no_ack_policy_cfg[EDCA_AC_BE];
	pe_session->gLimEdcaParamsActive[EDCA_AC_BK].no_ack =
		mac_ctx->no_ack_policy_cfg[EDCA_AC_BK];
	pe_session->gLimEdcaParamsActive[EDCA_AC_VI].no_ack =
		mac_ctx->no_ack_policy_cfg[EDCA_AC_VI];
	pe_session->gLimEdcaParamsActive[EDCA_AC_VO].no_ack =
		mac_ctx->no_ack_policy_cfg[EDCA_AC_VO];
	sta_ds_ptr = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
					&pe_session->dph.dphHashTable);
	if (sta_ds_ptr)
		lim_send_edca_params(mac_ctx,
				     pe_session->gLimEdcaParamsActive,
				     sta_ds_ptr->bssId, false);
	else
		pe_err("Self entry missing in Hash Table");
}

static void lim_process_sme_update_config(tpAniSirGlobal mac_ctx,
					  struct update_config *msg)
{
	tpPESession pe_session;

	pe_debug("received eWNI_SME_UPDATE_HT_CONFIG message");
	if (msg == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pe_session = pe_find_session_by_sme_session_id(mac_ctx,
						       msg->sme_session_id);
	if (pe_session == NULL) {
		pe_warn("Session does not exist for given BSSID");
		return;
	}

	switch (msg->capab) {
	case WNI_CFG_HT_CAP_INFO_ADVANCE_CODING:
		pe_session->htConfig.ht_rx_ldpc = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_TX_STBC:
		pe_session->htConfig.ht_tx_stbc = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_RX_STBC:
		pe_session->htConfig.ht_rx_stbc = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ:
		pe_session->htConfig.ht_sgi20 = msg->value;
		break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_40MHZ:
		pe_session->htConfig.ht_sgi40 = msg->value;
		break;
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		sch_set_fixed_beacon_fields(mac_ctx, pe_session);
		lim_send_beacon_ind(mac_ctx, pe_session, REASON_CONFIG_UPDATE);
	}
}

void
lim_send_vdev_restart(tpAniSirGlobal pMac,
		      tpPESession psessionEntry, uint8_t sessionId)
{
	tpHalHiddenSsidVdevRestart pHalHiddenSsidVdevRestart = NULL;
	struct scheduler_msg msgQ = {0};
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;

	if (psessionEntry == NULL) {
		pe_err("Invalid parameters");
		return;
	}

	pHalHiddenSsidVdevRestart =
		qdf_mem_malloc(sizeof(tHalHiddenSsidVdevRestart));
	if (NULL == pHalHiddenSsidVdevRestart) {
		pe_err("Unable to allocate memory");
		return;
	}

	pHalHiddenSsidVdevRestart->ssidHidden = psessionEntry->ssidHidden;
	pHalHiddenSsidVdevRestart->sessionId = sessionId;
	pHalHiddenSsidVdevRestart->pe_session_id = psessionEntry->peSessionId;

	msgQ.type = WMA_HIDDEN_SSID_VDEV_RESTART;
	msgQ.bodyptr = pHalHiddenSsidVdevRestart;
	msgQ.bodyval = 0;

	retCode = wma_post_ctrl_msg(pMac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		pe_err("wma_post_ctrl_msg() failed");
		qdf_mem_free(pHalHiddenSsidVdevRestart);
	}
}

/**
 * __lim_process_roam_scan_offload_req() - Process Roam scan offload from csr
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to SME message buffer
 *
 * Return: None
 */
static void __lim_process_roam_scan_offload_req(tpAniSirGlobal mac_ctx,
						uint32_t *msg_buf)
{
	tpPESession pe_session;
	struct scheduler_msg wma_msg = {0};
	QDF_STATUS status;
	tSirRoamOffloadScanReq *req_buffer;
	uint16_t local_ie_len;
	uint8_t *local_ie_buf;

	req_buffer = (tSirRoamOffloadScanReq *)msg_buf;
	pe_session = pe_find_session_by_sme_session_id(mac_ctx,
					req_buffer->sessionId);

	local_ie_buf = qdf_mem_malloc(MAX_DEFAULT_SCAN_IE_LEN);
	if (!local_ie_buf) {
		pe_err("Mem Alloc failed for local_ie_buf");
		qdf_mem_zero(req_buffer, sizeof(*req_buffer));
		qdf_mem_free(req_buffer);
		return;
	}

	local_ie_len = req_buffer->assoc_ie.length;
	/* Update ext cap IE if present */
	if (local_ie_len &&
	    !lim_update_ext_cap_ie(mac_ctx, req_buffer->assoc_ie.addIEdata,
				   local_ie_buf, &local_ie_len)) {
		if (local_ie_len <
		    QDF_ARRAY_SIZE(req_buffer->assoc_ie.addIEdata)) {
			req_buffer->assoc_ie.length = local_ie_len;
			qdf_mem_copy(req_buffer->assoc_ie.addIEdata,
				     local_ie_buf, local_ie_len);
		}
	}
	qdf_mem_free(local_ie_buf);

	if (pe_session)
		lim_update_fils_rik(pe_session, req_buffer);

	wma_msg.type = WMA_ROAM_SCAN_OFFLOAD_REQ;
	wma_msg.bodyptr = req_buffer;
	status = wma_post_ctrl_msg(mac_ctx, &wma_msg);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Posting WMA_ROAM_SCAN_OFFLOAD_REQ failed");
		qdf_mem_zero(req_buffer, sizeof(*req_buffer));
		qdf_mem_free(req_buffer);
	}
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * lim_send_roam_per_command() - Process roam send PER command from csr
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to SME message buffer
 *
 * Return: None
 */
static void lim_send_roam_per_command(tpAniSirGlobal mac_ctx,
				      uint32_t *msg_buf)
{
	struct scheduler_msg wma_msg = {0};
	QDF_STATUS status;

	wma_msg.type = WMA_SET_PER_ROAM_CONFIG_CMD;
	wma_msg.bodyptr = msg_buf;

	status = wma_post_ctrl_msg(mac_ctx, &wma_msg);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Posting WMA_ROAM_INIT_PARAM failed");
		qdf_mem_free(msg_buf);
	}
}
#else
static void lim_send_roam_per_command(tpAniSirGlobal mac_ctx,
				      uint32_t *msg_buf)
{
	qdf_mem_free(msg_buf);
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * lim_process_roam_invoke() - process the Roam Invoke req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to the SME message buffer
 *
 * This function is called by limProcessMessageQueue(). This function sends the
 * ROAM_INVOKE command to WMA.
 *
 * Return: None
 */
static void lim_process_roam_invoke(tpAniSirGlobal mac_ctx,
				    uint32_t *msg_buf)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	msg.type = SIR_HAL_ROAM_INVOKE;
	msg.bodyptr = msg_buf;
	msg.reserved = 0;

	status = wma_post_ctrl_msg(mac_ctx, &msg);
	if (QDF_STATUS_SUCCESS != status)
		pe_err("Not able to post SIR_HAL_ROAM_INVOKE to WMA");
}
#else
static void lim_process_roam_invoke(tpAniSirGlobal mac_ctx,
				    uint32_t *msg_buf)
{
	qdf_mem_free(msg_buf);
}
#endif

/*
 * lim_handle_update_ssid_hidden() - Processes SSID hidden update
 * @mac_ctx: Pointer to global mac context
 * @session: Pointer to PE session
 * @ssid_hidden: SSID hidden value to set; 0 - Broadcast SSID,
 *    1 - Disable broadcast SSID
 *
 * Return: None
 */
static void lim_handle_update_ssid_hidden(tpAniSirGlobal mac_ctx,
				tpPESession session, uint8_t ssid_hidden)
{
	pe_debug("rcvd HIDE_SSID message old HIDE_SSID: %d new HIDE_SSID: %d",
			session->ssidHidden, ssid_hidden);

	if (ssid_hidden != session->ssidHidden) {
		session->ssidHidden = ssid_hidden;
	} else {
		pe_debug("Dont process HIDE_SSID msg with existing setting");
		return;
	}

	/* Send vdev restart */
	lim_send_vdev_restart(mac_ctx, session, session->smeSessionId);

	return;
}

/**
 * __lim_process_sme_session_update - process SME session update msg
 *
 * @mac_ctx: Pointer to global mac context
 * @msg_buf: Pointer to the received message buffer
 *
 * Return: None
 */
static void __lim_process_sme_session_update(tpAniSirGlobal mac_ctx,
						uint32_t *msg_buf)
{
	struct sir_update_session_param *msg;
	tpPESession session;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	msg = (struct sir_update_session_param *) msg_buf;

	session = pe_find_session_by_sme_session_id(mac_ctx, msg->session_id);
	if (!session) {
		pe_warn("Session does not exist for given sessionId %d",
			msg->session_id);
		return;
	}

	pe_debug("received SME Session update for %d val %d",
			msg->param_type, msg->param_val);
	switch (msg->param_type) {
	case SIR_PARAM_SSID_HIDDEN:
		lim_handle_update_ssid_hidden(mac_ctx, session, msg->param_val);
		break;
	case SIR_PARAM_IGNORE_ASSOC_DISALLOWED:
		session->ignore_assoc_disallowed = msg->param_val;
		break;
	default:
		pe_err("Unknown session param");
		break;
	}
}

/*
   Update the beacon Interval dynamically if beaconInterval is different in MCC
 */
static void __lim_process_sme_change_bi(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tpSirChangeBIParams pChangeBIParams;
	tpPESession psessionEntry;
	uint8_t sessionId = 0;
	tUpdateBeaconParams beaconParams;

	pe_debug("received Update Beacon Interval message");

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	qdf_mem_zero(&beaconParams, sizeof(tUpdateBeaconParams));
	pChangeBIParams = (tpSirChangeBIParams) pMsgBuf;

	psessionEntry = pe_find_session_by_bssid(pMac,
				pChangeBIParams->bssid.bytes,
				&sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session does not exist for given BSSID");
		return;
	}

	/*Update sessionEntry Beacon Interval */
	if (psessionEntry->beaconParams.beaconInterval !=
	    pChangeBIParams->beaconInterval) {
		psessionEntry->beaconParams.beaconInterval =
			pChangeBIParams->beaconInterval;
	}

	/*Update sch beaconInterval */
	if (pMac->sch.schObject.gSchBeaconInterval !=
	    pChangeBIParams->beaconInterval) {
		pMac->sch.schObject.gSchBeaconInterval =
			pChangeBIParams->beaconInterval;

		pe_debug("LIM send update BeaconInterval Indication: %d",
			pChangeBIParams->beaconInterval);

		if (false == pMac->sap.SapDfsInfo.is_dfs_cac_timer_running) {
			/* Update beacon */
			sch_set_fixed_beacon_fields(pMac, psessionEntry);

			beaconParams.bssIdx = psessionEntry->bssIdx;
			/* Set change in beacon Interval */
			beaconParams.beaconInterval =
				pChangeBIParams->beaconInterval;
			beaconParams.paramChangeBitmap =
				PARAM_BCN_INTERVAL_CHANGED;
			lim_send_beacon_params(pMac, &beaconParams, psessionEntry);
		}
	}

	return;
} /*** end __lim_process_sme_change_bi(tpAniSirGlobal pMac, uint32_t *pMsgBuf) ***/

#ifdef QCA_HT_2040_COEX
static void __lim_process_sme_set_ht2040_mode(tpAniSirGlobal pMac,
					      uint32_t *pMsgBuf)
{
	tpSirSetHT2040Mode pSetHT2040Mode;
	tpPESession psessionEntry;
	uint8_t sessionId = 0;
	struct scheduler_msg msg = {0};
	tUpdateVHTOpMode *pHtOpMode = NULL;
	uint16_t staId = 0;
	tpDphHashNode pStaDs = NULL;

	pe_debug("received Set HT 20/40 mode message");
	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pSetHT2040Mode = (tpSirSetHT2040Mode) pMsgBuf;

	psessionEntry = pe_find_session_by_bssid(pMac,
				pSetHT2040Mode->bssid.bytes,
				&sessionId);
	if (psessionEntry == NULL) {
		pe_debug("Session does not exist for given BSSID");
		lim_print_mac_addr(pMac, pSetHT2040Mode->bssid.bytes, LOGD);
		return;
	}

	pe_debug("Update session entry for cbMod=%d",
		pSetHT2040Mode->cbMode);
	/*Update sessionEntry HT related fields */
	switch (pSetHT2040Mode->cbMode) {
	case PHY_SINGLE_CHANNEL_CENTERED:
		psessionEntry->htSecondaryChannelOffset =
			PHY_SINGLE_CHANNEL_CENTERED;
		psessionEntry->htRecommendedTxWidthSet = 0;
		if (pSetHT2040Mode->obssEnabled)
			psessionEntry->htSupportedChannelWidthSet
					= eHT_CHANNEL_WIDTH_40MHZ;
		else
			psessionEntry->htSupportedChannelWidthSet
					= eHT_CHANNEL_WIDTH_20MHZ;
		break;
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		psessionEntry->htSecondaryChannelOffset =
			PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		psessionEntry->htRecommendedTxWidthSet = 1;
		break;
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		psessionEntry->htSecondaryChannelOffset =
			PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		psessionEntry->htRecommendedTxWidthSet = 1;
		break;
	default:
		pe_err("Invalid cbMode");
		return;
	}

	/* Update beacon */
	sch_set_fixed_beacon_fields(pMac, psessionEntry);
	lim_send_beacon_ind(pMac, psessionEntry, REASON_SET_HT2040);

	/* update OP Mode for each associated peer */
	for (staId = 0; staId < psessionEntry->dph.dphHashTable.size; staId++) {
		pStaDs = dph_get_hash_entry(pMac, staId,
				&psessionEntry->dph.dphHashTable);
		if (NULL == pStaDs)
			continue;

		if (pStaDs->valid && pStaDs->htSupportedChannelWidthSet) {
			pHtOpMode = qdf_mem_malloc(sizeof(tUpdateVHTOpMode));
			if (NULL == pHtOpMode) {
				pe_err("Not able to allocate memory for setting OP mode");
				return;
			}
			pHtOpMode->opMode =
				(psessionEntry->htSecondaryChannelOffset ==
				 PHY_SINGLE_CHANNEL_CENTERED) ?
				eHT_CHANNEL_WIDTH_20MHZ : eHT_CHANNEL_WIDTH_40MHZ;
			pHtOpMode->staId = staId;
			qdf_mem_copy(pHtOpMode->peer_mac, &pStaDs->staAddr,
				     sizeof(tSirMacAddr));
			pHtOpMode->smesessionId = sessionId;

			msg.type = WMA_UPDATE_OP_MODE;
			msg.reserved = 0;
			msg.bodyptr = pHtOpMode;
			if (!QDF_IS_STATUS_SUCCESS
				    (scheduler_post_message(QDF_MODULE_ID_PE,
							    QDF_MODULE_ID_WMA,
							    QDF_MODULE_ID_WMA,
							    &msg))) {
				pe_err("Not able to post WMA_UPDATE_OP_MODE message to WMA");
				qdf_mem_free(pHtOpMode);
				return;
			}
			pe_debug("Notified FW about OP mode: %d for staId=%d",
				pHtOpMode->opMode, staId);

		} else
			pe_debug("station %d does not support HT40", staId);
	}

	return;
}
#endif

/* -------------------------------------------------------------------- */
/**
 * __lim_process_report_message
 *
 * FUNCTION:  Processes the next received Radio Resource Management message
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

static void __lim_process_report_message(tpAniSirGlobal pMac,
					 struct scheduler_msg *pMsg)
{
	switch (pMsg->type) {
	case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
		rrm_process_neighbor_report_req(pMac, pMsg->bodyptr);
		break;
	case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
		rrm_process_beacon_report_xmit(pMac, pMsg->bodyptr);
		break;
	default:
		pe_err("Invalid msg type: %d", pMsg->type);
	}
}

/* -------------------------------------------------------------------- */
/**
 * lim_send_set_max_tx_power_req
 *
 * FUNCTION:  Send SIR_HAL_SET_MAX_TX_POWER_REQ message to change the max tx power.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param txPower txPower to be set.
 * @param pSessionEntry session entry.
 * @return None
 */
QDF_STATUS
lim_send_set_max_tx_power_req(tpAniSirGlobal pMac, int8_t txPower,
			      tpPESession pSessionEntry)
{
	tpMaxTxPowerParams pMaxTxParams = NULL;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	if (pSessionEntry == NULL) {
		pe_err("Invalid parameters");
		return QDF_STATUS_E_FAILURE;
	}

	pMaxTxParams = qdf_mem_malloc(sizeof(tMaxTxPowerParams));
	if (NULL == pMaxTxParams) {
		pe_err("Unable to allocate memory for pMaxTxParams");
		return QDF_STATUS_E_NOMEM;

	}
	pMaxTxParams->power = txPower;
	qdf_mem_copy(pMaxTxParams->bssId.bytes, pSessionEntry->bssId,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(pMaxTxParams->selfStaMacAddr.bytes,
			pSessionEntry->selfMacAddr,
			QDF_MAC_ADDR_SIZE);

	msgQ.type = WMA_SET_MAX_TX_POWER_REQ;
	msgQ.bodyptr = pMaxTxParams;
	msgQ.bodyval = 0;
	pe_debug("Post WMA_SET_MAX_TX_POWER_REQ to WMA");
	MTRACE(mac_trace_msg_tx(pMac, pSessionEntry->peSessionId, msgQ.type));
	retCode = wma_post_ctrl_msg(pMac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		pe_err("wma_post_ctrl_msg() failed");
		qdf_mem_free(pMaxTxParams);
	}
	return retCode;
}

/**
 * __lim_process_sme_register_mgmt_frame_req() - process sme reg mgmt frame req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process eWNI_SME_REGISTER_MGMT_FRAME_REQ message
 * from SME. It Register this information within PE.
 *
 * Return: None
 */
static void __lim_process_sme_register_mgmt_frame_req(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	QDF_STATUS qdf_status;
	tpSirRegisterMgmtFrame sme_req = (tpSirRegisterMgmtFrame)msg_buf;
	struct mgmt_frm_reg_info *lim_mgmt_regn = NULL;
	struct mgmt_frm_reg_info *next = NULL;
	bool match = false;

	pe_nofl_debug("Register Frame: register %d, type %d, match length %d",
		      sme_req->registerFrame, sme_req->frameType,
		      sme_req->matchLen);
	/* First check whether entry exists already */
	qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
	qdf_list_peek_front(&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
			    (qdf_list_node_t **) &lim_mgmt_regn);
	qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);

	while (lim_mgmt_regn != NULL) {
		if (lim_mgmt_regn->frameType != sme_req->frameType)
			goto skip_match;
		if (sme_req->matchLen) {
			if ((lim_mgmt_regn->matchLen == sme_req->matchLen) &&
				(!qdf_mem_cmp(lim_mgmt_regn->matchData,
					sme_req->matchData,
					lim_mgmt_regn->matchLen))) {
					/* found match! */
					match = true;
					break;
			}
		} else {
			/* found match! */
			match = true;
			break;
		}
skip_match:
		qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
		qdf_status = qdf_list_peek_next(
				&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
				(qdf_list_node_t *)lim_mgmt_regn,
				(qdf_list_node_t **)&next);
		qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);
		lim_mgmt_regn = next;
		next = NULL;
	}
	if (match) {
		qdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
		if (QDF_STATUS_SUCCESS ==
				qdf_list_remove_node(
				&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
				(qdf_list_node_t *)lim_mgmt_regn))
			qdf_mem_free(lim_mgmt_regn);
		qdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);
	}

	if (sme_req->registerFrame) {
		lim_mgmt_regn =
			qdf_mem_malloc(sizeof(struct mgmt_frm_reg_info) +
					sme_req->matchLen);
		if (lim_mgmt_regn != NULL) {
			lim_mgmt_regn->frameType = sme_req->frameType;
			lim_mgmt_regn->matchLen = sme_req->matchLen;
			lim_mgmt_regn->sessionId = sme_req->sessionId;
			if (sme_req->matchLen) {
				qdf_mem_copy(lim_mgmt_regn->matchData,
					     sme_req->matchData,
					     sme_req->matchLen);
			}
			qdf_mutex_acquire(
					&mac_ctx->lim.lim_frame_register_lock);
			qdf_list_insert_front(&mac_ctx->lim.
					      gLimMgmtFrameRegistratinQueue,
					      &lim_mgmt_regn->node);
			qdf_mutex_release(
					&mac_ctx->lim.lim_frame_register_lock);
		}
	}
	return;
}

static void
__lim_process_sme_reset_ap_caps_change(tpAniSirGlobal pMac, uint32_t *pMsgBuf)
{
	tpSirResetAPCapsChange pResetCapsChange;
	tpPESession psessionEntry;
	uint8_t sessionId = 0;

	if (pMsgBuf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pResetCapsChange = (tpSirResetAPCapsChange) pMsgBuf;
	psessionEntry =
		pe_find_session_by_bssid(pMac, pResetCapsChange->bssId.bytes,
					 &sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session does not exist for given BSSID");
		return;
	}

	psessionEntry->limSentCapsChangeNtf = false;
	return;
}

/**
 * lim_register_mgmt_frame_ind_cb() - Save the Management frame
 * indication callback in PE.
 * @mac_ptr: Mac pointer
 * @msg_buf: Msg pointer containing the callback
 *
 * This function is used save the Management frame
 * indication callback in PE.
 *
 * Return: None
 */
static void lim_register_mgmt_frame_ind_cb(tpAniSirGlobal mac_ctx,
							uint32_t *msg_buf)
{
	struct sir_sme_mgmt_frame_cb_req *sme_req =
		(struct sir_sme_mgmt_frame_cb_req *)msg_buf;

	if (NULL == msg_buf) {
		pe_err("msg_buf is null");
		return;
	}
	if (sme_req->callback)
		mac_ctx->mgmt_frame_ind_cb =
			(sir_mgmt_frame_ind_callback)sme_req->callback;
	else
		pe_err("sme_req->callback is null");
}

/**
 *__lim_process_send_disassoc_frame: function processes disassoc frame
 * @mac_ctx: pointer to mac context
 * @msg_buf: message buffer
 *
 * function processes disassoc request received from SME
 *
 * return: none
 */
static void __lim_process_send_disassoc_frame(tpAniSirGlobal mac_ctx,
					uint32_t *msg_buf)
{
	struct sme_send_disassoc_frm_req sme_send_disassoc_frame_req;
	QDF_STATUS status;
	tpPESession session_entry = NULL;
	uint8_t sme_session_id;
	uint16_t sme_trans_id;

	if (msg_buf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	lim_get_session_info(mac_ctx, (uint8_t *)msg_buf, &sme_session_id,
			&sme_trans_id);

	status = lim_send_disassoc_frm_req_ser_des(mac_ctx,
				&sme_send_disassoc_frame_req,
				(uint8_t *)msg_buf);

	if ((QDF_STATUS_E_FAILURE == status) ||
		(lim_is_group_addr(sme_send_disassoc_frame_req.peer_mac) &&
		!lim_is_addr_bc(sme_send_disassoc_frame_req.peer_mac))) {
		pe_err("received invalid SME_DISASSOC_REQ message");
		return;
	}

	session_entry = pe_find_session_by_sme_session_id(
				mac_ctx, sme_session_id);
	if (session_entry == NULL) {
		pe_err("session does not exist for given bssId "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(sme_send_disassoc_frame_req.peer_mac));
		return;
	}

	pe_debug("msg_type->%d len->%d sess_id->%d trans_id->%d mac->"MAC_ADDRESS_STR" reason->%d wait_for_ack->%d",
			sme_send_disassoc_frame_req.msg_type,
			sme_send_disassoc_frame_req.length,
			sme_send_disassoc_frame_req.session_id,
			sme_send_disassoc_frame_req.trans_id,
			MAC_ADDR_ARRAY(sme_send_disassoc_frame_req.peer_mac),
			sme_send_disassoc_frame_req.reason,
			sme_send_disassoc_frame_req.wait_for_ack);

	lim_send_disassoc_mgmt_frame(mac_ctx,
		sme_send_disassoc_frame_req.reason,
		sme_send_disassoc_frame_req.peer_mac,
		session_entry, sme_send_disassoc_frame_req.wait_for_ack);
}

/**
 * lim_set_pdev_ht_ie() - sends the set HT IE req to FW
 * @mac_ctx: Pointer to Global MAC structure
 * @pdev_id: pdev id to set the IE.
 * @nss: Nss values to prepare the HT IE.
 *
 * Prepares the HT IE with self capabilities for different
 * Nss values and sends the set HT IE req to FW.
 *
 * Return: None
 */
static void lim_set_pdev_ht_ie(tpAniSirGlobal mac_ctx, uint8_t pdev_id,
		uint8_t nss)
{
	struct set_ie_param *ie_params;
	struct scheduler_msg msg = {0};
	QDF_STATUS rc = QDF_STATUS_SUCCESS;
	const uint8_t *p_ie = NULL;
	tHtCaps *p_ht_cap;
	int i;

	for (i = 1; i <= nss; i++) {
		ie_params = qdf_mem_malloc(sizeof(*ie_params));
		if (NULL == ie_params) {
			pe_err("mem alloc failed");
			return;
		}
		ie_params->nss = i;
		ie_params->pdev_id = pdev_id;
		ie_params->ie_type = DOT11_HT_IE;
		/* 2 for IE len and EID */
		ie_params->ie_len = 2 + sizeof(tHtCaps);
		ie_params->ie_ptr = qdf_mem_malloc(ie_params->ie_len);
		if (NULL == ie_params->ie_ptr) {
			qdf_mem_free(ie_params);
			pe_err("mem alloc failed");
			return;
		}
		*ie_params->ie_ptr = SIR_MAC_HT_CAPABILITIES_EID;
		*(ie_params->ie_ptr + 1) = ie_params->ie_len - 2;
		lim_set_ht_caps(mac_ctx, NULL, ie_params->ie_ptr,
				ie_params->ie_len);

		if (NSS_1x1_MODE == i) {
			p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_HTCAPS,
					ie_params->ie_ptr, ie_params->ie_len);
			if (NULL == p_ie) {
				qdf_mem_free(ie_params->ie_ptr);
				qdf_mem_free(ie_params);
				pe_err("failed to get IE ptr");
				return;
			}
			p_ht_cap = (tHtCaps *)&p_ie[2];
			p_ht_cap->supportedMCSSet[1] = 0;
			p_ht_cap->txSTBC = 0;
		}

		msg.type = WMA_SET_PDEV_IE_REQ;
		msg.bodyptr = ie_params;
		msg.bodyval = 0;

		rc = wma_post_ctrl_msg(mac_ctx, &msg);
		if (rc != QDF_STATUS_SUCCESS) {
			pe_err("wma_post_ctrl_msg() return failure");
			qdf_mem_free(ie_params->ie_ptr);
			qdf_mem_free(ie_params);
			return;
		}
	}
}

/**
 * lim_set_pdev_vht_ie() - sends the set VHT IE to req FW
 * @mac_ctx: Pointer to Global MAC structure
 * @pdev_id: pdev id to set the IE.
 * @nss: Nss values to prepare the VHT IE.
 *
 * Prepares the VHT IE with self capabilities for different
 * Nss values and sends the set VHT IE req to FW.
 *
 * Return: None
 */
static void lim_set_pdev_vht_ie(tpAniSirGlobal mac_ctx, uint8_t pdev_id,
		uint8_t nss)
{
	struct set_ie_param *ie_params;
	struct scheduler_msg msg = {0};
	QDF_STATUS rc = QDF_STATUS_SUCCESS;
	const uint8_t *p_ie = NULL;
	tSirMacVHTCapabilityInfo *vht_cap;
	int i;
	tSirVhtMcsInfo *vht_mcs;

	for (i = 1; i <= nss; i++) {
		ie_params = qdf_mem_malloc(sizeof(*ie_params));
		if (NULL == ie_params) {
			pe_err("mem alloc failed");
			return;
		}
		ie_params->nss = i;
		ie_params->pdev_id = pdev_id;
		ie_params->ie_type = DOT11_VHT_IE;
		/* 2 for IE len and EID */
		ie_params->ie_len = 2 + sizeof(tSirMacVHTCapabilityInfo) +
			sizeof(tSirVhtMcsInfo);
		ie_params->ie_ptr = qdf_mem_malloc(ie_params->ie_len);
		if (NULL == ie_params->ie_ptr) {
			qdf_mem_free(ie_params);
			pe_err("mem alloc failed");
			return;
		}
		*ie_params->ie_ptr = SIR_MAC_VHT_CAPABILITIES_EID;
		*(ie_params->ie_ptr + 1) = ie_params->ie_len - 2;
		lim_set_vht_caps(mac_ctx, NULL, ie_params->ie_ptr,
				ie_params->ie_len);

		if (NSS_1x1_MODE == i) {
			p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_VHTCAPS,
					ie_params->ie_ptr, ie_params->ie_len);
			if (NULL == p_ie) {
				qdf_mem_free(ie_params->ie_ptr);
				qdf_mem_free(ie_params);
				pe_err("failed to get IE ptr");
				return;
			}
			vht_cap = (tSirMacVHTCapabilityInfo *)&p_ie[2];
			vht_cap->txSTBC = 0;
			vht_mcs =
				(tSirVhtMcsInfo *)&p_ie[2 +
				sizeof(tSirMacVHTCapabilityInfo)];
			vht_mcs->rxMcsMap |= DISABLE_NSS2_MCS;
			vht_mcs->rxHighest =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
			vht_mcs->txMcsMap |= DISABLE_NSS2_MCS;
			vht_mcs->txHighest =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
		}
		msg.type = WMA_SET_PDEV_IE_REQ;
		msg.bodyptr = ie_params;
		msg.bodyval = 0;

		rc = wma_post_ctrl_msg(mac_ctx, &msg);
		if (rc != QDF_STATUS_SUCCESS) {
			pe_err("wma_post_ctrl_msg failure");
			qdf_mem_free(ie_params->ie_ptr);
			qdf_mem_free(ie_params);
			return;
		}
	}
}

/**
 * lim_process_set_vdev_ies_per_band() - process the set vdev IE req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to the SME message buffer
 *
 * This function is called by limProcessMessageQueue(). This function sets the
 * VDEV IEs to the FW.
 *
 * Return: None
 */
static void lim_process_set_vdev_ies_per_band(tpAniSirGlobal mac_ctx,
						uint32_t *msg_buf)
{
	struct sir_set_vdev_ies_per_band *p_msg =
				(struct sir_set_vdev_ies_per_band *)msg_buf;

	if (NULL == p_msg) {
		pe_err("NULL p_msg");
		return;
	}

	pe_debug("rcvd set vdev ie per band req vdev_id = %d",
		p_msg->vdev_id);
	/* intentionally using NULL here so that self capabilty are sent */
	if (lim_send_ies_per_band(mac_ctx, NULL, p_msg->vdev_id) !=
			QDF_STATUS_SUCCESS)
		pe_err("Unable to send HT/VHT Cap to FW");
}

/**
 * lim_process_set_pdev_IEs() - process the set pdev IE req
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: Pointer to the SME message buffer
 *
 * This function is called by limProcessMessageQueue(). This
 * function sets the PDEV IEs to the FW.
 *
 * Return: None
 */
static void lim_process_set_pdev_IEs(tpAniSirGlobal mac_ctx, uint32_t *msg_buf)
{
	struct sir_set_ht_vht_cfg *ht_vht_cfg;

	ht_vht_cfg = (struct sir_set_ht_vht_cfg *)msg_buf;

	if (NULL == ht_vht_cfg) {
		pe_err("NULL ht_vht_cfg");
		return;
	}

	pe_debug("rcvd set pdev ht vht ie req with nss = %d",
			ht_vht_cfg->nss);
	lim_set_pdev_ht_ie(mac_ctx, ht_vht_cfg->pdev_id, ht_vht_cfg->nss);

	if (IS_DOT11_MODE_VHT(ht_vht_cfg->dot11mode))
		lim_set_pdev_vht_ie(mac_ctx, ht_vht_cfg->pdev_id,
				ht_vht_cfg->nss);
}

/**
 * lim_process_sme_update_access_policy_vendor_ie: function updates vendor IE
 *
 * access policy
 * @mac_ctx: pointer to mac context
 * @msg: message buffer
 *
 * function processes vendor IE and access policy from SME and updates PE
 *
 * session entry
 *
 * return: none
*/
static void lim_process_sme_update_access_policy_vendor_ie(
						tpAniSirGlobal mac_ctx,
						uint32_t *msg)
{
	struct sme_update_access_policy_vendor_ie *update_vendor_ie;
	struct sPESession *pe_session_entry;
	uint16_t num_bytes;

	if (!msg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	update_vendor_ie = (struct sme_update_access_policy_vendor_ie *) msg;
	pe_session_entry = pe_find_session_by_sme_session_id(mac_ctx,
					update_vendor_ie->sme_session_id);

	if (!pe_session_entry) {
		pe_err("Session does not exist for given sme session id(%hu)",
			update_vendor_ie->sme_session_id);
		return;
	}
	if (pe_session_entry->access_policy_vendor_ie)
		qdf_mem_free(pe_session_entry->access_policy_vendor_ie);

	num_bytes = update_vendor_ie->ie[1] + 2;
	pe_session_entry->access_policy_vendor_ie = qdf_mem_malloc(num_bytes);

	if (!pe_session_entry->access_policy_vendor_ie) {
		pe_err("Failed to allocate memory for vendor ie");
		return;
	}
	qdf_mem_copy(pe_session_entry->access_policy_vendor_ie,
		&update_vendor_ie->ie[0], num_bytes);

	pe_session_entry->access_policy = update_vendor_ie->access_policy;
}

/**
 * lim_process_sme_req_messages()
 *
 ***FUNCTION:
 * This function is called by limProcessMessageQueue(). This
 * function processes SME request messages from HDD or upper layer
 * application.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  msgType   Indicates the SME message type
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return Boolean - true - if pMsgBuf is consumed and can be freed.
 *                   false - if pMsgBuf is not to be freed.
 */

bool lim_process_sme_req_messages(tpAniSirGlobal pMac,
				  struct scheduler_msg *pMsg)
{
	bool bufConsumed = true;        /* Set this flag to false within case block of any following message, that doesn't want pMsgBuf to be freed. */
	uint32_t *pMsgBuf = pMsg->bodyptr;

	pe_nofl_debug("LIM handle SME Msg %s(%d)",
		      lim_msg_str(pMsg->type), pMsg->type);

	/* If no insert NOA required then execute the code below */

	switch (pMsg->type) {
	case eWNI_SME_SYS_READY_IND:
		bufConsumed = __lim_process_sme_sys_ready_ind(pMac, pMsgBuf);
		break;

	case eWNI_SME_START_BSS_REQ:
		bufConsumed = __lim_process_sme_start_bss_req(pMac, pMsg);
		break;

	case eWNI_SME_JOIN_REQ:
		__lim_process_sme_join_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_REASSOC_REQ:
		__lim_process_sme_reassoc_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_DISASSOC_REQ:
		__lim_process_sme_disassoc_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_DISASSOC_CNF:
	case eWNI_SME_DEAUTH_CNF:
		__lim_process_sme_disassoc_cnf(pMac, pMsgBuf);
		break;

	case eWNI_SME_DEAUTH_REQ:
		__lim_process_sme_deauth_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_SEND_DISASSOC_FRAME:
		__lim_process_send_disassoc_frame(pMac, pMsgBuf);
		break;

	case eWNI_SME_SETCONTEXT_REQ:
		__lim_process_sme_set_context_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_STOP_BSS_REQ:
		bufConsumed = __lim_process_sme_stop_bss_req(pMac, pMsg);
		break;

	case eWNI_SME_ASSOC_CNF:
#ifdef WLAN_DEBUG
		if (pMsg->type == eWNI_SME_ASSOC_CNF)
			pe_debug("Received ASSOC_CNF message");
#endif
			__lim_process_sme_assoc_cnf_new(pMac, pMsg->type,
							pMsgBuf);
		break;

	case eWNI_SME_ADDTS_REQ:
		pe_debug("Received ADDTS_REQ message");
		__lim_process_sme_addts_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_DELTS_REQ:
		pe_debug("Received DELTS_REQ message");
		__lim_process_sme_delts_req(pMac, pMsgBuf);
		break;

	case SIR_LIM_ADDTS_RSP_TIMEOUT:
		pe_debug("Received SIR_LIM_ADDTS_RSP_TIMEOUT message");
		lim_process_sme_addts_rsp_timeout(pMac, pMsg->bodyval);
		break;

	case eWNI_SME_GET_STATISTICS_REQ:
		__lim_process_sme_get_statistics_request(pMac, pMsgBuf);
		/* HAL consumes pMsgBuf. It will be freed there. Set bufConsumed to false. */
		bufConsumed = false;
		break;
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_REQ:
		__lim_process_sme_get_tsm_stats_request(pMac, pMsgBuf);
		bufConsumed = false;
		break;
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_GET_ASSOC_STAS_REQ:
		lim_process_sme_get_assoc_sta_info(pMac, pMsgBuf);
		break;
	case eWNI_SME_SESSION_UPDATE_PARAM:
		__lim_process_sme_session_update(pMac, pMsgBuf);
		break;
	case eWNI_SME_ROAM_SCAN_OFFLOAD_REQ:
		__lim_process_roam_scan_offload_req(pMac, pMsgBuf);
		bufConsumed = false;
		break;
	case eWNI_SME_ROAM_SEND_PER_REQ:
		lim_send_roam_per_command(pMac, pMsgBuf);
		bufConsumed = false;
		break;
	case eWNI_SME_ROAM_INVOKE:
		lim_process_roam_invoke(pMac, pMsgBuf);
		bufConsumed = false;
		break;
	case eWNI_SME_CHNG_MCC_BEACON_INTERVAL:
		/* Update the beaconInterval */
		__lim_process_sme_change_bi(pMac, pMsgBuf);
		break;

#ifdef QCA_HT_2040_COEX
	case eWNI_SME_SET_HT_2040_MODE:
		__lim_process_sme_set_ht2040_mode(pMac, pMsgBuf);
		break;
#endif

	case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
	case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
		__lim_process_report_message(pMac, pMsg);
		break;

	case eWNI_SME_FT_PRE_AUTH_REQ:
		bufConsumed = (bool) lim_process_ft_pre_auth_req(pMac, pMsg);
		break;
	case eWNI_SME_FT_UPDATE_KEY:
		lim_process_ft_update_key(pMac, pMsgBuf);
		break;

	case eWNI_SME_FT_AGGR_QOS_REQ:
		lim_process_ft_aggr_qos_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_REGISTER_MGMT_FRAME_REQ:
		__lim_process_sme_register_mgmt_frame_req(pMac, pMsgBuf);
		break;
#ifdef FEATURE_WLAN_TDLS
	case eWNI_SME_TDLS_SEND_MGMT_REQ:
		lim_process_sme_tdls_mgmt_send_req(pMac, pMsgBuf);
		break;
	case eWNI_SME_TDLS_ADD_STA_REQ:
		lim_process_sme_tdls_add_sta_req(pMac, pMsgBuf);
		break;
	case eWNI_SME_TDLS_DEL_STA_REQ:
		lim_process_sme_tdls_del_sta_req(pMac, pMsgBuf);
		break;
#endif
	case eWNI_SME_RESET_AP_CAPS_CHANGED:
		__lim_process_sme_reset_ap_caps_change(pMac, pMsgBuf);
		break;

	case eWNI_SME_CHANNEL_CHANGE_REQ:
		lim_process_sme_channel_change_request(pMac, pMsgBuf);
		break;

	case eWNI_SME_START_BEACON_REQ:
		lim_process_sme_start_beacon_req(pMac, pMsgBuf);
		break;

	case eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ:
		lim_process_sme_dfs_csa_ie_request(pMac, pMsgBuf);
		break;

	case eWNI_SME_UPDATE_ADDITIONAL_IES:
		lim_process_update_add_ies(pMac, pMsgBuf);
		break;

	case eWNI_SME_MODIFY_ADDITIONAL_IES:
		lim_process_modify_add_ies(pMac, pMsgBuf);
		break;
	case eWNI_SME_SET_HW_MODE_REQ:
		lim_process_set_hw_mode(pMac, pMsgBuf);
		break;
	case eWNI_SME_NSS_UPDATE_REQ:
		lim_process_nss_update_request(pMac, pMsgBuf);
		break;
	case eWNI_SME_SET_DUAL_MAC_CFG_REQ:
		lim_process_set_dual_mac_cfg_req(pMac, pMsgBuf);
		break;
	case eWNI_SME_SET_IE_REQ:
		lim_process_set_ie_req(pMac, pMsgBuf);
		break;
	case eWNI_SME_REGISTER_MGMT_FRAME_CB:
		lim_register_mgmt_frame_ind_cb(pMac, pMsgBuf);
		break;
	case eWNI_SME_EXT_CHANGE_CHANNEL:
		lim_process_ext_change_channel(pMac, pMsgBuf);
		break;
	case eWNI_SME_SET_ANTENNA_MODE_REQ:
		lim_process_set_antenna_mode_req(pMac, pMsgBuf);
		break;
	case eWNI_SME_PDEV_SET_HT_VHT_IE:
		lim_process_set_pdev_IEs(pMac, pMsgBuf);
		break;
	case eWNI_SME_SET_VDEV_IES_PER_BAND:
		lim_process_set_vdev_ies_per_band(pMac, pMsgBuf);
		break;
	case eWNI_SME_UPDATE_ACCESS_POLICY_VENDOR_IE:
		lim_process_sme_update_access_policy_vendor_ie(pMac, pMsgBuf);
		break;
	case eWNI_SME_UPDATE_CONFIG:
		lim_process_sme_update_config(pMac,
					(struct update_config *)pMsgBuf);
		break;
	case eWNI_SME_SET_ADDBA_ACCEPT:
		lim_process_sme_set_addba_accept(pMac,
					(struct sme_addba_accept *)pMsgBuf);
		break;
	case eWNI_SME_UPDATE_EDCA_PROFILE:
		lim_process_sme_update_edca_params(pMac, pMsg->bodyval);
		break;
	case WNI_SME_REGISTER_BCN_REPORT_SEND_CB:
		lim_register_bcn_report_send_cb(pMac, pMsg);
		break;
	default:
		qdf_mem_free((void *)pMsg->bodyptr);
		pMsg->bodyptr = NULL;
		break;
	} /* switch (msgType) */

	return bufConsumed;
} /*** end lim_process_sme_req_messages() ***/

/**
 * lim_process_sme_start_beacon_req()
 *
 ***FUNCTION:
 * This function is called by limProcessMessageQueue(). This
 * function processes SME request messages from HDD or upper layer
 * application.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  msgType   Indicates the SME message type
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return Boolean - true - if pMsgBuf is consumed and can be freed.
 *                   false - if pMsgBuf is not to be freed.
 */
static void lim_process_sme_start_beacon_req(tpAniSirGlobal pMac, uint32_t *pMsg)
{
	tpSirStartBeaconIndication pBeaconStartInd;
	tpPESession psessionEntry;
	uint8_t sessionId;      /* PE sessionID */

	if (pMsg == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	pBeaconStartInd = (tpSirStartBeaconIndication) pMsg;
	psessionEntry = pe_find_session_by_bssid(pMac,
				pBeaconStartInd->bssid,
				&sessionId);
	if (psessionEntry == NULL) {
		lim_print_mac_addr(pMac, pBeaconStartInd->bssid, LOGE);
		pe_err("Session does not exist for given bssId");
		return;
	}

	if (pBeaconStartInd->beaconStartStatus == true) {
		/*
		 * Currently this Indication comes from SAP
		 * to start Beacon Tx on a DFS channel
		 * since beaconing has to be done on DFS
		 * channel only after CAC WAIT is completed.
		 * On a DFS Channel LIM does not start beacon
		 * Tx right after the WMA_ADD_BSS_RSP.
		 */
		lim_apply_configuration(pMac, psessionEntry);
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("Start Beacon with ssid %s Ch %d"),
			  psessionEntry->ssId.ssId,
			  psessionEntry->currentOperChannel);
		lim_send_beacon_ind(pMac, psessionEntry, REASON_DEFAULT);
		lim_enable_obss_detection_config(pMac, psessionEntry);
		lim_send_obss_color_collision_cfg(pMac, psessionEntry,
					OBSS_COLOR_COLLISION_DETECTION);
	} else {
		pe_err("Invalid Beacon Start Indication");
		return;
	}
}

/**
 * lim_process_sme_channel_change_request() - process sme ch change req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function is called to process SME_CHANNEL_CHANGE_REQ message
 *
 * Return: None
 */
static void lim_process_sme_channel_change_request(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	tpSirChanChangeRequest ch_change_req;
	tpPESession session_entry;
	uint8_t session_id;      /* PE session_id */
	int8_t max_tx_pwr;
	uint32_t val = 0;

	if (msg_buf == NULL) {
		pe_err("msg_buf is NULL");
		return;
	}
	ch_change_req = (tpSirChanChangeRequest)msg_buf;

	max_tx_pwr = cfg_get_regulatory_max_transmit_power(mac_ctx,
			ch_change_req->targetChannel);

	if ((ch_change_req->messageType != eWNI_SME_CHANNEL_CHANGE_REQ) ||
			(max_tx_pwr == WMA_MAX_TXPOWER_INVALID)) {
		pe_err("Invalid Request/max_tx_pwr");
		return;
	}

	session_entry = pe_find_session_by_bssid(mac_ctx,
			ch_change_req->bssid, &session_id);
	if (session_entry == NULL) {
		lim_print_mac_addr(mac_ctx, ch_change_req->bssid, LOGE);
		pe_err("Session does not exist for given bssId");
		return;
	}

	if ((session_entry->currentOperChannel ==
			ch_change_req->targetChannel) &&
	     (session_entry->ch_width == ch_change_req->ch_width)) {
		pe_err("Target channel and mode is same as current channel and mode channel %d and mode %d",
		       session_entry->currentOperChannel, session_entry->ch_width);
		return;
	}

	if (LIM_IS_AP_ROLE(session_entry))
		session_entry->channelChangeReasonCode =
			LIM_SWITCH_CHANNEL_SAP_DFS;
	else
		session_entry->channelChangeReasonCode =
			LIM_SWITCH_CHANNEL_OPERATION;

	pe_nofl_debug("SAP CSA: %d ---> %d, ch_bw %d, nw_type %d, dot11mode %d",
		      session_entry->currentOperChannel,
		      ch_change_req->targetChannel,
		      ch_change_req->ch_width, ch_change_req->nw_type,
		      ch_change_req->dot11mode);

	/* Store the New Channel Params in session_entry */
	session_entry->ch_width = ch_change_req->ch_width;
	session_entry->ch_center_freq_seg0 =
		ch_change_req->center_freq_seg_0;
	session_entry->ch_center_freq_seg1 =
		ch_change_req->center_freq_seg_1;
	session_entry->htSecondaryChannelOffset = ch_change_req->sec_ch_offset;
	session_entry->htSupportedChannelWidthSet =
		(ch_change_req->ch_width ? 1 : 0);
	session_entry->htRecommendedTxWidthSet =
		session_entry->htSupportedChannelWidthSet;
	session_entry->currentOperChannel =
		ch_change_req->targetChannel;
	session_entry->limRFBand =
		lim_get_rf_band(session_entry->currentOperChannel);

	/* Update the global beacon filter */
	lim_update_bcn_probe_filter(mac_ctx, session_entry);

	/* Initialize 11h Enable Flag */
	if (CHAN_HOP_ALL_BANDS_ENABLE ||
	    BAND_5G == session_entry->limRFBand) {
		if (wlan_cfg_get_int(mac_ctx, WNI_CFG_11H_ENABLED, &val) !=
				QDF_STATUS_SUCCESS)
			pe_err("Fail to get WNI_CFG_11H_ENABLED");
	}

	session_entry->lim11hEnable = val;
	session_entry->dot11mode = ch_change_req->dot11mode;
	session_entry->nwType = ch_change_req->nw_type;
	qdf_mem_copy(&session_entry->rateSet,
			&ch_change_req->operational_rateset,
			sizeof(session_entry->rateSet));
	qdf_mem_copy(&session_entry->extRateSet,
			&ch_change_req->extended_rateset,
			sizeof(session_entry->extRateSet));
	lim_set_channel(mac_ctx, ch_change_req->targetChannel,
			session_entry->ch_center_freq_seg0,
			session_entry->ch_center_freq_seg1,
			session_entry->ch_width,
			max_tx_pwr, session_entry->peSessionId,
			ch_change_req->cac_duration_ms,
			ch_change_req->dfs_regdomain);
}

/******************************************************************************
* lim_start_bss_update_add_ie_buffer()
*
***FUNCTION:
* This function checks the src buffer and its length and then malloc for
* dst buffer update the same
*
***LOGIC:
*
***ASSUMPTIONS:
*
***NOTE:
*
* @param  pMac      Pointer to Global MAC structure
* @param  **pDstData_buff  A pointer to pointer of  uint8_t dst buffer
* @param  *pDstDataLen  A pointer to pointer of  uint16_t dst buffer length
* @param  *pSrcData_buff  A pointer of  uint8_t  src buffer
* @param  srcDataLen  src buffer length
******************************************************************************/

static void
lim_start_bss_update_add_ie_buffer(tpAniSirGlobal pMac,
				   uint8_t **pDstData_buff,
				   uint16_t *pDstDataLen,
				   uint8_t *pSrcData_buff, uint16_t srcDataLen)
{

	if (srcDataLen > 0 && pSrcData_buff != NULL) {
		*pDstDataLen = srcDataLen;

		*pDstData_buff = qdf_mem_malloc(*pDstDataLen);

		if (NULL == *pDstData_buff) {
			pe_err("AllocateMemory failed for pDstData_buff");
			return;
		}
		qdf_mem_copy(*pDstData_buff, pSrcData_buff, *pDstDataLen);
	} else {
		*pDstData_buff = NULL;
		*pDstDataLen = 0;
	}
}

/******************************************************************************
* lim_update_add_ie_buffer()
*
***FUNCTION:
* This function checks the src buffer and length if src buffer length more
* than dst buffer length then free the dst buffer and malloc for the new src
* length, and update the dst buffer and length. But if dst buffer is bigger
* than src buffer length then it just update the dst buffer and length
*
***LOGIC:
*
***ASSUMPTIONS:
*
***NOTE:
*
* @param  pMac      Pointer to Global MAC structure
* @param  **pDstData_buff  A pointer to pointer of  uint8_t dst buffer
* @param  *pDstDataLen  A pointer to pointer of  uint16_t dst buffer length
* @param  *pSrcData_buff  A pointer of  uint8_t  src buffer
* @param  srcDataLen  src buffer length
******************************************************************************/

static void
lim_update_add_ie_buffer(tpAniSirGlobal pMac,
			 uint8_t **pDstData_buff,
			 uint16_t *pDstDataLen,
			 uint8_t *pSrcData_buff, uint16_t srcDataLen)
{

	if (NULL == pSrcData_buff) {
		pe_err("src buffer is null");
		return;
	}

	if (srcDataLen > *pDstDataLen) {
		*pDstDataLen = srcDataLen;
		/* free old buffer */
		qdf_mem_free(*pDstData_buff);
		/* allocate a new */
		*pDstData_buff = qdf_mem_malloc(*pDstDataLen);

		if (NULL == *pDstData_buff) {
			pe_err("Memory allocation failed");
			*pDstDataLen = 0;
			return;
		}
	}

	/* copy the content of buffer into dst buffer
	 */
	*pDstDataLen = srcDataLen;
	qdf_mem_copy(*pDstData_buff, pSrcData_buff, *pDstDataLen);

}

/**
 * lim_update_ibss_prop_add_ies() - update IBSS prop IE
 * @pMac          : Pointer to Global MAC structure
 * @pDstData_buff : A pointer to pointer of  dst buffer
 * @pDstDataLen  :  A pointer to pointer of  dst buffer length
 * @pModifyIE    :  A pointer to tSirModifyIE
 *
 * This function replaces previous ibss prop_ie with new ibss prop_ie.
 *
 * Return:
 *  True or false depending upon whether IE is updated or not
 */
static bool
lim_update_ibss_prop_add_ies(tpAniSirGlobal pMac, uint8_t **pDstData_buff,
			     uint16_t *pDstDataLen, tSirModifyIE *pModifyIE)
{
	int32_t oui_length;
	uint8_t *ibss_ie = NULL;
	uint8_t *vendor_ie;
#define MAC_VENDOR_OUI  "\x00\x16\x32"
#define MAC_VENDOR_SIZE 3

	ibss_ie = pModifyIE->pIEBuffer;
	oui_length = pModifyIE->oui_length;

	if ((0 == oui_length) || (NULL == ibss_ie)) {
		pe_err("Invalid set IBSS vendor IE command length %d",
			oui_length);
		return false;
	}

	/*
	 * Why replace only beacon OUI data here:
	 * 1. other ie (such as wpa) shall not be overwritten here.
	 * 2. per spec, beacon oui ie might be set twice and original one
	 * shall be updated.
	 */
	vendor_ie = (uint8_t *)wlan_get_vendor_ie_ptr_from_oui(MAC_VENDOR_OUI,
			MAC_VENDOR_SIZE, *pDstData_buff, *pDstDataLen);
	if (vendor_ie) {
		QDF_ASSERT((vendor_ie[1] + 2) == pModifyIE->ieBufferlength);
		qdf_mem_copy(vendor_ie, pModifyIE->pIEBuffer,
				pModifyIE->ieBufferlength);
	} else {
		uint16_t new_length;
		uint8_t *new_ptr;

		/*
		 * check for uint16 overflow before using sum of two numbers as
		 * length of size to malloc
		 */
		if (USHRT_MAX - pModifyIE->ieBufferlength < *pDstDataLen) {
			pe_err("U16 overflow due to %d + %d",
				pModifyIE->ieBufferlength, *pDstDataLen);
			return false;
		}

		new_length = pModifyIE->ieBufferlength + *pDstDataLen;
		new_ptr = qdf_mem_malloc(new_length);
		if (NULL == new_ptr) {
			pe_err("Memory allocation failed");
			return false;
		}
		qdf_mem_copy(new_ptr, *pDstData_buff, *pDstDataLen);
		qdf_mem_copy(&new_ptr[*pDstDataLen], pModifyIE->pIEBuffer,
				pModifyIE->ieBufferlength);
		qdf_mem_free(*pDstData_buff);
		*pDstDataLen = new_length;
		*pDstData_buff = new_ptr;
	}
	return true;
}

/*
* lim_process_modify_add_ies() - process modify additional IE req.
*
* @mac_ctx: Pointer to Global MAC structure
* @msg_buf: pointer to the SME message buffer
*
* This function update the PE buffers for additional IEs.
*
* Return: None
*/
static void lim_process_modify_add_ies(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	tpSirModifyIEsInd modify_add_ies;
	tpPESession session_entry;
	uint8_t session_id;
	bool ret = false;
	tSirAddIeParams *add_ie_params;

	if (msg_buf == NULL) {
		pe_err("msg_buf is NULL");
		return;
	}

	modify_add_ies = (tpSirModifyIEsInd)msg_buf;
	/* Incoming message has smeSession, use BSSID to find PE session */
	session_entry = pe_find_session_by_bssid(mac_ctx,
			modify_add_ies->modifyIE.bssid.bytes, &session_id);

	if (NULL == session_entry) {
		pe_err("Session not found for given bssid"
					MAC_ADDRESS_STR,
		MAC_ADDR_ARRAY(modify_add_ies->modifyIE.bssid.bytes));
		goto end;
	}
	if ((0 == modify_add_ies->modifyIE.ieBufferlength) ||
		(0 == modify_add_ies->modifyIE.ieIDLen) ||
		(NULL == modify_add_ies->modifyIE.pIEBuffer)) {
		pe_err("Invalid request pIEBuffer %pK ieBufferlength %d ieIDLen %d ieID %d. update Type %d",
				modify_add_ies->modifyIE.pIEBuffer,
				modify_add_ies->modifyIE.ieBufferlength,
				modify_add_ies->modifyIE.ieID,
				modify_add_ies->modifyIE.ieIDLen,
				modify_add_ies->updateType);
		goto end;
	}
	add_ie_params = &session_entry->addIeParams;
	switch (modify_add_ies->updateType) {
	case eUPDATE_IE_PROBE_RESP:
		/* Probe resp */
		if (LIM_IS_IBSS_ROLE(session_entry)) {
			lim_update_ibss_prop_add_ies(mac_ctx,
				&add_ie_params->probeRespData_buff,
				&add_ie_params->probeRespDataLen,
				&modify_add_ies->modifyIE);
		}
		break;
	case eUPDATE_IE_ASSOC_RESP:
		/* assoc resp IE */
		if (add_ie_params->assocRespDataLen == 0) {
			QDF_TRACE(QDF_MODULE_ID_PE,
					QDF_TRACE_LEVEL_ERROR, FL(
				"assoc resp add ie not present %d"),
				add_ie_params->assocRespDataLen);
		}
		/* search through the buffer and modify the IE */
		break;
	case eUPDATE_IE_PROBE_BCN:
		/*probe beacon IE */
		if (LIM_IS_IBSS_ROLE(session_entry)) {
			ret = lim_update_ibss_prop_add_ies(mac_ctx,
				&add_ie_params->probeRespBCNData_buff,
				&add_ie_params->probeRespBCNDataLen,
				&modify_add_ies->modifyIE);
		}
		if (ret == true && modify_add_ies->modifyIE.notify) {
			lim_handle_param_update(mac_ctx,
					modify_add_ies->updateType);
		}
		break;
	default:
		pe_err("unhandled buffer type %d",
				modify_add_ies->updateType);
		break;
	}
end:
	qdf_mem_free(modify_add_ies->modifyIE.pIEBuffer);
	modify_add_ies->modifyIE.pIEBuffer = NULL;
}

/*
* lim_process_update_add_ies() - process additional IE update req
*
* @mac_ctx: Pointer to Global MAC structure
* @msg_buf: pointer to the SME message buffer
*
* This function update the PE buffers for additional IEs.
*
* Return: None
*/
static void lim_process_update_add_ies(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	tpSirUpdateIEsInd update_add_ies = (tpSirUpdateIEsInd)msg_buf;
	uint8_t session_id;
	tpPESession session_entry;
	tSirAddIeParams *addn_ie;
	uint16_t new_length = 0;
	uint8_t *new_ptr = NULL;
	tSirUpdateIE *update_ie;

	if (msg_buf == NULL) {
		pe_err("msg_buf is NULL");
		return;
	}
	update_ie = &update_add_ies->updateIE;
	/* incoming message has smeSession, use BSSID to find PE session */
	session_entry = pe_find_session_by_bssid(mac_ctx,
			update_ie->bssid.bytes, &session_id);

	if (NULL == session_entry) {
		pe_debug("Session not found for given bssid"
			 MAC_ADDRESS_STR,
			 MAC_ADDR_ARRAY(update_ie->bssid.bytes));
		goto end;
	}
	addn_ie = &session_entry->addIeParams;
	/* if len is 0, upper layer requested freeing of buffer */
	if (0 == update_ie->ieBufferlength) {
		switch (update_add_ies->updateType) {
		case eUPDATE_IE_PROBE_RESP:
			qdf_mem_free(addn_ie->probeRespData_buff);
			addn_ie->probeRespData_buff = NULL;
			addn_ie->probeRespDataLen = 0;
			break;
		case eUPDATE_IE_ASSOC_RESP:
			qdf_mem_free(addn_ie->assocRespData_buff);
			addn_ie->assocRespData_buff = NULL;
			addn_ie->assocRespDataLen = 0;
			break;
		case eUPDATE_IE_PROBE_BCN:
			qdf_mem_free(addn_ie->probeRespBCNData_buff);
			addn_ie->probeRespBCNData_buff = NULL;
			addn_ie->probeRespBCNDataLen = 0;

			if (update_ie->notify)
				lim_handle_param_update(mac_ctx,
						update_add_ies->updateType);
			break;
		default:
			break;
		}
		return;
	}
	switch (update_add_ies->updateType) {
	case eUPDATE_IE_PROBE_RESP:
		if (update_ie->append) {
			/*
			 * In case of append, allocate new memory
			 * with combined length.
			 * Multiple back to back append commands
			 * can lead to a huge length.So, check
			 * for the validity of the length.
			 */
			if (addn_ie->probeRespDataLen >
				(USHRT_MAX - update_ie->ieBufferlength)) {
				pe_err("IE Length overflow, curr:%d, new:%d",
					addn_ie->probeRespDataLen,
					update_ie->ieBufferlength);
				goto end;
			}
			new_length = update_ie->ieBufferlength +
				addn_ie->probeRespDataLen;
			new_ptr = qdf_mem_malloc(new_length);
			if (NULL == new_ptr) {
				pe_err("Memory allocation failed");
				goto end;
			}
			/* append buffer to end of local buffers */
			qdf_mem_copy(new_ptr, addn_ie->probeRespData_buff,
					addn_ie->probeRespDataLen);
			qdf_mem_copy(&new_ptr[addn_ie->probeRespDataLen],
				     update_ie->pAdditionIEBuffer,
				     update_ie->ieBufferlength);
			/* free old memory */
			qdf_mem_free(addn_ie->probeRespData_buff);
			/* adjust length accordingly */
			addn_ie->probeRespDataLen = new_length;
			/* save refernece of local buffer in PE session */
			addn_ie->probeRespData_buff = new_ptr;
			goto end;
		}
		lim_update_add_ie_buffer(mac_ctx, &addn_ie->probeRespData_buff,
				&addn_ie->probeRespDataLen,
				update_ie->pAdditionIEBuffer,
				update_ie->ieBufferlength);
		break;
	case eUPDATE_IE_ASSOC_RESP:
		/* assoc resp IE */
		lim_update_add_ie_buffer(mac_ctx, &addn_ie->assocRespData_buff,
				&addn_ie->assocRespDataLen,
				update_ie->pAdditionIEBuffer,
				update_ie->ieBufferlength);
		break;
	case eUPDATE_IE_PROBE_BCN:
		/* probe resp Bcn IE */
		lim_update_add_ie_buffer(mac_ctx,
				&addn_ie->probeRespBCNData_buff,
				&addn_ie->probeRespBCNDataLen,
				update_ie->pAdditionIEBuffer,
				update_ie->ieBufferlength);
		if (update_ie->notify)
			lim_handle_param_update(mac_ctx,
					update_add_ies->updateType);
		break;
	default:
		pe_err("unhandled buffer type %d", update_add_ies->updateType);
		break;
	}
end:
	qdf_mem_free(update_ie->pAdditionIEBuffer);
	update_ie->pAdditionIEBuffer = NULL;
}

/**
 * send_extended_chan_switch_action_frame()- function to send ECSA
 * action frame for each sta connected to SAP/GO and AP in case of
 * STA .
 * @mac_ctx: pointer to global mac structure
 * @new_channel: new channel to switch to.
 * @ch_bandwidth: BW of channel to calculate op_class
 * @session_entry: pe session
 *
 * This function is called to send ECSA frame for STA/CLI and SAP/GO.
 *
 * Return: void
 */

static void send_extended_chan_switch_action_frame(tpAniSirGlobal mac_ctx,
				uint16_t new_channel, uint8_t ch_bandwidth,
						tpPESession session_entry)
{
	uint16_t op_class;
	uint8_t switch_mode = 0, i;
	tpDphHashNode psta;
	uint8_t switch_count;

	op_class = wlan_reg_dmn_get_opclass_from_channel(
				mac_ctx->scan.countryCodeCurrent,
				new_channel,
				ch_bandwidth);

	if (LIM_IS_AP_ROLE(session_entry) &&
		(mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch == false))
		switch_mode = session_entry->gLimChannelSwitch.switchMode;

	switch_count = session_entry->gLimChannelSwitch.switchCount;

	if (LIM_IS_AP_ROLE(session_entry)) {
		for (i = 0; i <= mac_ctx->lim.maxStation; i++) {
			psta =
			  session_entry->dph.dphHashTable.pDphNodeArray + i;
			if (psta && psta->added)
				lim_send_extended_chan_switch_action_frame(
					mac_ctx,
					psta->staAddr,
					switch_mode, op_class, new_channel,
					switch_count, session_entry);
		}
	} else if (LIM_IS_STA_ROLE(session_entry)) {
		lim_send_extended_chan_switch_action_frame(mac_ctx,
					session_entry->bssId,
					switch_mode, op_class, new_channel,
					switch_count, session_entry);
	}

}

void lim_send_chan_switch_action_frame(tpAniSirGlobal mac_ctx,
				       uint16_t new_channel,
				       uint8_t ch_bandwidth,
				       tpPESession session_entry)
{
	uint16_t op_class;
	uint8_t switch_mode = 0, i;
	uint8_t switch_count;
	tpDphHashNode psta;
	tpDphHashNode dph_node_array_ptr;

	dph_node_array_ptr = session_entry->dph.dphHashTable.pDphNodeArray;

	op_class = wlan_reg_dmn_get_opclass_from_channel(
			mac_ctx->scan.countryCodeCurrent,
			new_channel, ch_bandwidth);

	if (LIM_IS_AP_ROLE(session_entry) &&
	    (false == mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch))
		switch_mode = session_entry->gLimChannelSwitch.switchMode;

	switch_count = session_entry->gLimChannelSwitch.switchCount;

	if (LIM_IS_AP_ROLE(session_entry)) {
		for (i = 0; i < mac_ctx->lim.maxStation; i++) {
			psta = dph_node_array_ptr + i;
			if (!(psta && psta->added))
				continue;
			if (session_entry->lim_non_ecsa_cap_num == 0)
				lim_send_extended_chan_switch_action_frame
					(mac_ctx, psta->staAddr, switch_mode,
					 op_class, new_channel, switch_count,
					 session_entry);
			else
				lim_send_channel_switch_mgmt_frame
					(mac_ctx, psta->staAddr, switch_mode,
					 new_channel, switch_count,
					 session_entry);
		}
	} else if (LIM_IS_STA_ROLE(session_entry)) {
		lim_send_extended_chan_switch_action_frame
			(mac_ctx, session_entry->bssId, switch_mode, op_class,
			 new_channel, switch_count, session_entry);
	}
}

#define MAX_WAKELOCK_FOR_CSA         5000

/**
 * lim_process_sme_dfs_csa_ie_request() - process sme dfs csa ie req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function processes SME request messages from HDD or upper layer
 * application.
 *
 * Return: None
 */
static void lim_process_sme_dfs_csa_ie_request(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	tpSirDfsCsaIeRequest dfs_csa_ie_req;
	tpPESession session_entry = NULL;
	uint8_t session_id;
	tLimWiderBWChannelSwitchInfo *wider_bw_ch_switch;
	enum offset_t ch_offset;

	if (msg_buf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	dfs_csa_ie_req = (tSirDfsCsaIeRequest *)msg_buf;
	session_entry = pe_find_session_by_bssid(mac_ctx,
			dfs_csa_ie_req->bssid, &session_id);
	if (session_entry == NULL) {
		pe_err("Session not found for given BSSID" MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(dfs_csa_ie_req->bssid));
		return;
	}

	if (session_entry->valid && !LIM_IS_AP_ROLE(session_entry)) {
		pe_err("Invalid SystemRole %d",
			GET_LIM_SYSTEM_ROLE(session_entry));
		return;
	}

	/* target channel */
	session_entry->gLimChannelSwitch.primaryChannel =
		dfs_csa_ie_req->targetChannel;

	/* Channel switch announcement needs to be included in beacon */
	session_entry->dfsIncludeChanSwIe = true;
	session_entry->gLimChannelSwitch.switchCount =
		 dfs_csa_ie_req->ch_switch_beacon_cnt;
	session_entry->gLimChannelSwitch.ch_width =
				 dfs_csa_ie_req->ch_params.ch_width;
	session_entry->gLimChannelSwitch.sec_ch_offset =
				 dfs_csa_ie_req->ch_params.sec_ch_offset;
	if (mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch == false)
		session_entry->gLimChannelSwitch.switchMode =
			 dfs_csa_ie_req->ch_switch_mode;

	/*
	 * Validate if SAP is operating HT or VHT mode and set the Channel
	 * Switch Wrapper element with the Wide Band Switch subelement.
	 */
	if (true != session_entry->vhtCapability)
		goto skip_vht;

	/* Now encode the Wider Ch BW element depending on the ch width */
	wider_bw_ch_switch = &session_entry->gLimWiderBWChannelSwitch;
	switch (dfs_csa_ie_req->ch_params.ch_width) {
	case CH_WIDTH_20MHZ:
		/*
		 * Wide channel BW sublement in channel wrapper element is not
		 * required in case of 20 Mhz operation. Currently It is set
		 * only set in case of 40/80 Mhz Operation.
		 */
		session_entry->dfsIncludeChanWrapperIe = false;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
		break;
	case CH_WIDTH_40MHZ:
		session_entry->dfsIncludeChanWrapperIe = false;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
		break;
	case CH_WIDTH_80MHZ:
		session_entry->dfsIncludeChanWrapperIe = true;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
		break;
	case CH_WIDTH_160MHZ:
		session_entry->dfsIncludeChanWrapperIe = true;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;
		break;
	case CH_WIDTH_80P80MHZ:
		session_entry->dfsIncludeChanWrapperIe = true;
		wider_bw_ch_switch->newChanWidth =
			WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ;
		/*
		 * This is not applicable for 20/40/80 Mhz.
		 * Only used when we support 80+80 Mhz operation.
		 * In case of 80+80 Mhz, this parameter indicates
		 * center channel frequency index of 80 Mhz channel of
		 * frequency segment 1.
		 */
		wider_bw_ch_switch->newCenterChanFreq1 =
			dfs_csa_ie_req->ch_params.center_freq_seg1;
		break;
	default:
		session_entry->dfsIncludeChanWrapperIe = false;
		/*
		 * Need to handle 80+80 Mhz Scenario. When 80+80 is supported
		 * set the gLimWiderBWChannelSwitch.newChanWidth to 3
		 */
		pe_err("Invalid Channel Width");
		break;
	}
	/* Fetch the center channel based on the channel width */
	wider_bw_ch_switch->newCenterChanFreq0 =
		dfs_csa_ie_req->ch_params.center_freq_seg0;
skip_vht:
	qdf_wake_lock_timeout_acquire(&session_entry->ap_ecsa_wakelock,
				      MAX_WAKELOCK_FOR_CSA);
	qdf_runtime_pm_prevent_suspend(&session_entry->ap_ecsa_runtime_lock);
	/* Send CSA IE request from here */
	lim_send_dfs_chan_sw_ie_update(mac_ctx, session_entry);

	if (dfs_csa_ie_req->ch_params.ch_width == CH_WIDTH_80MHZ)
		ch_offset = BW80;
	else
		ch_offset = dfs_csa_ie_req->ch_params.sec_ch_offset;

	pe_debug("IE count:%d chan:%d width:%d wrapper:%d ch_offset:%d",
			session_entry->gLimChannelSwitch.switchCount,
			session_entry->gLimChannelSwitch.primaryChannel,
			session_entry->gLimChannelSwitch.ch_width,
			session_entry->dfsIncludeChanWrapperIe,
			ch_offset);

	/* Send ECSA/CSA Action frame after updating the beacon */
	if (CHAN_HOP_ALL_BANDS_ENABLE)
		lim_send_chan_switch_action_frame(mac_ctx,
			session_entry->gLimChannelSwitch.primaryChannel,
			ch_offset, session_entry);
	else
		send_extended_chan_switch_action_frame(mac_ctx,
			session_entry->gLimChannelSwitch.primaryChannel,
			ch_offset, session_entry);
}

/**
 * lim_process_ext_change_channel()- function to send ECSA
 * action frame for STA/CLI .
 * @mac_ctx: pointer to global mac structure
 * @msg: params from sme for new channel.
 *
 * This function is called to send ECSA frame for STA/CLI.
 *
 * Return: void
 */

static void lim_process_ext_change_channel(tpAniSirGlobal mac_ctx,
							uint32_t *msg)
{
	struct sir_sme_ext_cng_chan_req *ext_chng_channel =
				(struct sir_sme_ext_cng_chan_req *) msg;
	tpPESession session_entry = NULL;

	if (NULL == msg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	session_entry =
		pe_find_session_by_sme_session_id(mac_ctx,
						ext_chng_channel->session_id);
	if (NULL == session_entry) {
		pe_err("Session not found for given session %d",
			ext_chng_channel->session_id);
		return;
	}
	if (LIM_IS_AP_ROLE(session_entry)) {
		pe_err("not an STA/CLI session");
		return;
	}
	send_extended_chan_switch_action_frame(mac_ctx,
			ext_chng_channel->new_channel,
				0, session_entry);
}

/**
 * lim_nss_update_rsp() - send NSS update response to SME
 * @mac_ctx Pointer to Global MAC structure
 * @vdev_id: vdev id
 * @status: nss update status
 *
 * Return: None
 */
static void lim_nss_update_rsp(tpAniSirGlobal mac_ctx,
			       uint8_t vdev_id, QDF_STATUS status)
{
	struct scheduler_msg msg = {0};
	struct sir_bcn_update_rsp *nss_rsp;
	QDF_STATUS qdf_status;

	nss_rsp = qdf_mem_malloc(sizeof(*nss_rsp));
	if (!nss_rsp) {
		pe_err("AllocateMemory failed for nss_rsp");
		return;
	}

	nss_rsp->vdev_id = vdev_id;
	nss_rsp->status = status;
	nss_rsp->reason = REASON_NSS_UPDATE;

	msg.type = eWNI_SME_NSS_UPDATE_RSP;
	msg.bodyptr = nss_rsp;
	msg.bodyval = 0;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_PE, QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &msg);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		pe_err("Failed to post eWNI_SME_NSS_UPDATE_RSP");
		qdf_mem_free(nss_rsp);
	}
}

void lim_send_bcn_rsp(tpAniSirGlobal mac_ctx, tpSendbeaconParams rsp)
{
	if (!rsp) {
		pe_err("rsp is NULL");
		return;
	}

	pe_debug("Send beacon resp status %d for reason %d",
		 rsp->status, rsp->reason);

	if (rsp->reason == REASON_NSS_UPDATE)
		lim_nss_update_rsp(mac_ctx, rsp->vdev_id, rsp->status);
}

/**
 * lim_process_nss_update_request() - process sme nss update req
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function processes SME request messages from HDD or upper layer
 * application.
 *
 * Return: None
 */
static void lim_process_nss_update_request(tpAniSirGlobal mac_ctx,
		uint32_t *msg_buf)
{
	struct sir_nss_update_request *nss_update_req_ptr;
	tpPESession session_entry = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t vdev_id;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	nss_update_req_ptr = (struct sir_nss_update_request *)msg_buf;
	vdev_id = nss_update_req_ptr->vdev_id;
	session_entry = pe_find_session_by_sme_session_id(mac_ctx,
				nss_update_req_ptr->vdev_id);
	if (!session_entry) {
		pe_err("Session not found for given session_id %d",
			nss_update_req_ptr->vdev_id);
		goto end;
	}

	if (session_entry->valid && !LIM_IS_AP_ROLE(session_entry)) {
		pe_err("Invalid SystemRole %d",
			GET_LIM_SYSTEM_ROLE(session_entry));
		goto end;
	}

	/* populate nss field in the beacon */
	session_entry->gLimOperatingMode.present = 1;
	session_entry->gLimOperatingMode.rxNSS = nss_update_req_ptr->new_nss;
	session_entry->gLimOperatingMode.chanWidth = session_entry->ch_width;

	if ((nss_update_req_ptr->new_nss == NSS_1x1_MODE) &&
			(session_entry->ch_width > CH_WIDTH_80MHZ))
		session_entry->gLimOperatingMode.chanWidth = CH_WIDTH_80MHZ;

	pe_debug("ch width %d Rx NSS %d",
		 session_entry->gLimOperatingMode.chanWidth,
		 session_entry->gLimOperatingMode.rxNSS);

	/* Send nss update request from here */
	status = sch_set_fixed_beacon_fields(mac_ctx, session_entry);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Unable to set op mode IE in beacon");
		goto end;
	}

	status = lim_send_beacon_ind(mac_ctx, session_entry, REASON_NSS_UPDATE);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	pe_err("Unable to send beacon");
end:
	/*
	 * send resp only in case of failure,
	 * success case response will be from wma.
	 */
	lim_nss_update_rsp(mac_ctx, vdev_id, status);
}

/**
 * lim_process_set_ie_req() - process sme set IE request
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to the SME message buffer
 *
 * This function processes SME request messages from HDD or upper layer
 * application.
 *
 * Return: None
 */
static void lim_process_set_ie_req(tpAniSirGlobal mac_ctx, uint32_t *msg_buf)
{
	struct send_extcap_ie *msg;
	QDF_STATUS status;

	if (msg_buf == NULL) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	msg = (struct send_extcap_ie *)msg_buf;
	status = lim_send_ext_cap_ie(mac_ctx, msg->session_id, NULL, false);
	if (QDF_STATUS_SUCCESS != status)
		pe_err("Unable to send ExtCap to FW");

}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR

/**
 * obss_color_collision_process_color_disable() - Disable bss color
 * @mac_ctx: Pointer to Global MAC structure
 * @session: pointer to session
 *
 * This function will disbale bss color.
 *
 * Return: None
 */
static void obss_color_collision_process_color_disable(tpAniSirGlobal mac_ctx,
						       tpPESession session)
{
	tUpdateBeaconParams beacon_params;

	if (!session) {
		pe_err("Invalid session");
		return;
	}

	if (session->valid && !LIM_IS_AP_ROLE(session)) {
		pe_err("Invalid SystemRole %d",
		       GET_LIM_SYSTEM_ROLE(session));
		return;
	}

	if (session->bss_color_changing == 1) {
		pe_warn("%d: color change in progress", session->smeSessionId);
		/* Continue color collision detection */
		lim_send_obss_color_collision_cfg(mac_ctx, session,
				OBSS_COLOR_COLLISION_DETECTION);
		return;
	}

	if (session->he_op.bss_col_disabled == 1) {
		pe_warn("%d: bss color already disabled",
			session->smeSessionId);
		/* Continue free color detection */
		lim_send_obss_color_collision_cfg(mac_ctx, session,
				OBSS_COLOR_FREE_SLOT_AVAILABLE);
		return;
	}

	qdf_mem_zero(&beacon_params, sizeof(beacon_params));
	beacon_params.paramChangeBitmap |= PARAM_BSS_COLOR_CHANGED;
	session->he_op.bss_col_disabled = 1;
	beacon_params.bss_color_disabled = 1;
	beacon_params.bss_color = session->he_op.bss_color;

	if (sch_set_fixed_beacon_fields(mac_ctx, session) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("Unable to set op mode IE in beacon");
		return;
	}

	lim_send_beacon_params(mac_ctx, &beacon_params, session);
	lim_send_obss_color_collision_cfg(mac_ctx, session,
					  OBSS_COLOR_FREE_SLOT_AVAILABLE);
}

/**
 * obss_color_collision_process_color_change() - Process bss color change
 * @mac_ctx: Pointer to Global MAC structure
 * @session: pointer to session
 * @obss_color_info: obss color collision/free slot indication info
 *
 * This function selects new color ib case of bss color collision.
 *
 * Return: None
 */
static void obss_color_collision_process_color_change(tpAniSirGlobal mac_ctx,
		tpPESession session,
		struct wmi_obss_color_collision_info *obss_color_info)
{
	int i, num_bss_color = 0;
	uint32_t bss_color_bitmap;
	uint8_t bss_color_index_array[MAX_BSS_COLOR_VALUE];
	uint32_t rand_byte = 0;
	struct sir_set_he_bss_color he_bss_color;
	bool is_color_collision = false;


	if (session->bss_color_changing == 1) {
		pe_err("%d: color change in progress", session->smeSessionId);
		return;
	}

	if (!session->he_op.bss_col_disabled) {
		if (session->he_op.bss_color < 32)
			is_color_collision = (obss_color_info->
					     obss_color_bitmap_bit0to31 >>
					     session->he_op.bss_color) & 0x01;
		else
			is_color_collision = (obss_color_info->
					     obss_color_bitmap_bit32to63 >>
					     (session->he_op.bss_color -
					      32)) & 0x01;
		if (!is_color_collision) {
			pe_err("%d: color collision not found, curr_color: %d",
			       session->smeSessionId,
			       session->he_op.bss_color);
			return;
		}
	}

	bss_color_bitmap = obss_color_info->obss_color_bitmap_bit0to31;

	/* Skip color zero */
	bss_color_bitmap = bss_color_bitmap >> 1;
	for (i = 0; (i < 31) && (num_bss_color < MAX_BSS_COLOR_VALUE); i++) {
		if (!(bss_color_bitmap & 0x01)) {
			bss_color_index_array[num_bss_color] = i + 1;
			num_bss_color++;
		}
		bss_color_bitmap = bss_color_bitmap >> 1;
	}

	bss_color_bitmap = obss_color_info->obss_color_bitmap_bit32to63;
	for (i = 0; (i < 32) && (num_bss_color < MAX_BSS_COLOR_VALUE); i++) {
		if (!(bss_color_bitmap & 0x01)) {
			bss_color_index_array[num_bss_color] = i + 32;
			num_bss_color++;
		}
		bss_color_bitmap = bss_color_bitmap >> 1;
	}

	if (num_bss_color) {
		qdf_get_random_bytes((void *) &rand_byte, 1);
		i = (rand_byte + qdf_mc_timer_get_system_ticks()) %
		    num_bss_color;
		pe_debug("New bss color = %d", bss_color_index_array[i]);
		he_bss_color.session_id = obss_color_info->vdev_id;
		he_bss_color.bss_color = bss_color_index_array[i];
		lim_process_set_he_bss_color(mac_ctx,
					     (uint32_t *)&he_bss_color);
	} else {
		pe_err("Unable to find bss color from bitmasp");
		if (obss_color_info->evt_type ==
		    OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY &&
		    session->obss_color_collision_dec_evt ==
		    OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY)
			/* In dot11BSSColorCollisionAPPeriod and
			 * timer expired, time to disable bss color.
			 */
			obss_color_collision_process_color_disable(mac_ctx,
								   session);
		else
			/*
			 * Enter dot11BSSColorCollisionAPPeriod period.
			 */
			lim_send_obss_color_collision_cfg(mac_ctx, session,
					OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY);
	}
}

void lim_process_set_he_bss_color(tpAniSirGlobal mac_ctx, uint32_t *msg_buf)
{
	struct sir_set_he_bss_color *bss_color;
	tpPESession session_entry = NULL;
	tUpdateBeaconParams beacon_params;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	bss_color = (struct sir_set_he_bss_color *)msg_buf;
	session_entry = pe_find_session_by_sme_session_id(mac_ctx,
				bss_color->session_id);
	if (!session_entry) {
		pe_err("Session not found for given session_id %d",
			bss_color->session_id);
		return;
	}

	if (session_entry->valid && !LIM_IS_AP_ROLE(session_entry)) {
		pe_err("Invalid SystemRole %d",
			GET_LIM_SYSTEM_ROLE(session_entry));
		return;
	}

	if (bss_color->bss_color == session_entry->he_op.bss_color) {
		pe_err("No change in  BSS color, current BSS color %d",
			bss_color->bss_color);
		return;
	}
	qdf_mem_zero(&beacon_params, sizeof(beacon_params));
	beacon_params.paramChangeBitmap |= PARAM_BSS_COLOR_CHANGED;
	session_entry->he_op.bss_col_disabled = 1;
	session_entry->he_bss_color_change.countdown =
		BSS_COLOR_SWITCH_COUNTDOWN;
	session_entry->he_bss_color_change.new_color = bss_color->bss_color;
	beacon_params.bss_color_disabled = 1;
	beacon_params.bss_color = session_entry->he_op.bss_color;
	session_entry->bss_color_changing = 1;

	if (sch_set_fixed_beacon_fields(mac_ctx, session_entry) !=
			QDF_STATUS_SUCCESS) {
		pe_err("Unable to set op mode IE in beacon");
		return;
	}

	lim_send_beacon_params(mac_ctx, &beacon_params, session_entry);
	lim_send_obss_color_collision_cfg(mac_ctx, session_entry,
			OBSS_COLOR_COLLISION_DETECTION_DISABLE);
}

void lim_send_obss_color_collision_cfg(tpAniSirGlobal mac_ctx,
				       tpPESession session,
				       enum wmi_obss_color_collision_evt_type
				       event_type)
{
	struct wmi_obss_color_collision_cfg_param *cfg_param;
	struct scheduler_msg msg = {0};

	if (!session) {
		pe_err("Invalid session");
		return;
	}

	if (!session->he_capable ||
	    !session->is_session_obss_color_collision_det_enabled) {
		pe_debug("%d: obss color det not enabled, he_cap:%d, sup:%d:%d",
			 session->smeSessionId, session->he_capable,
			 session->is_session_obss_color_collision_det_enabled,
			 mac_ctx->lim.global_obss_color_collision_det_offload);
		return;
	}

	cfg_param = qdf_mem_malloc(sizeof(*cfg_param));
	if (!cfg_param) {
		pe_err("Failed to allocate memory");
		return;
	}

	pe_debug("%d: sending event:%d", session->smeSessionId, event_type);
	qdf_mem_zero(cfg_param, sizeof(*cfg_param));
	cfg_param->vdev_id = session->smeSessionId;
	cfg_param->evt_type = event_type;
	if (LIM_IS_AP_ROLE(session))
		cfg_param->detection_period_ms =
			OBSS_COLOR_COLLISION_DETECTION_AP_PERIOD_MS;
	else
		cfg_param->detection_period_ms =
			OBSS_COLOR_COLLISION_DETECTION_STA_PERIOD_MS;

	cfg_param->scan_period_ms = OBSS_COLOR_COLLISION_SCAN_PERIOD_MS;
	if (event_type == OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY)
		cfg_param->free_slot_expiry_time_ms =
			OBSS_COLOR_COLLISION_FREE_SLOT_EXPIRY_MS;

	msg.type = WMA_OBSS_COLOR_COLLISION_REQ;
	msg.bodyptr = cfg_param;
	msg.reserved = 0;

	if (QDF_IS_STATUS_ERROR(scheduler_post_message(QDF_MODULE_ID_PE,
						       QDF_MODULE_ID_WMA,
						       QDF_MODULE_ID_WMA,
						       &msg))) {
		pe_err("Failed to post WMA_OBSS_COLOR_COLLISION_REQ to WMA");
		qdf_mem_free(cfg_param);
	} else {
		session->obss_color_collision_dec_evt = event_type;
	}
}

void lim_process_obss_color_collision_info(tpAniSirGlobal mac_ctx,
					   uint32_t *msg_buf)
{
	struct wmi_obss_color_collision_info *obss_color_info;
	tpPESession session;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	obss_color_info = (struct wmi_obss_color_collision_info *)msg_buf;
	session = pe_find_session_by_sme_session_id(mac_ctx,
						    obss_color_info->vdev_id);
	if (!session) {
		pe_err("Session not found for given session_id %d",
			obss_color_info->vdev_id);
		return;
	}

	pe_debug("vdev_id:%d, evt:%d:%d, 0to31:0x%x, 32to63:0x%x, cap:%d:%d:%d",
		 obss_color_info->vdev_id,
		 obss_color_info->evt_type,
		 session->obss_color_collision_dec_evt,
		 obss_color_info->obss_color_bitmap_bit0to31,
		 obss_color_info->obss_color_bitmap_bit32to63,
		 session->he_capable,
		 session->is_session_obss_color_collision_det_enabled,
		 mac_ctx->lim.global_obss_color_collision_det_offload);

	if (!session->he_capable ||
	    !session->is_session_obss_color_collision_det_enabled) {
		return;
	}

	switch (obss_color_info->evt_type) {
	case OBSS_COLOR_COLLISION_DETECTION_DISABLE:
		pe_err("%d: FW disabled obss color det. he_cap:%d, sup:%d:%d",
		       session->smeSessionId, session->he_capable,
		       session->is_session_obss_color_collision_det_enabled,
		       mac_ctx->lim.global_obss_color_collision_det_offload);
		session->is_session_obss_color_collision_det_enabled = false;
		return;
	case OBSS_COLOR_FREE_SLOT_AVAILABLE:
	case OBSS_COLOR_COLLISION_DETECTION:
	case OBSS_COLOR_FREE_SLOT_TIMER_EXPIRY:
		if (session->valid && !LIM_IS_AP_ROLE(session)) {
			pe_debug("Invalid System Role %d",
				 GET_LIM_SYSTEM_ROLE(session));
			return;
		}

		if (session->obss_color_collision_dec_evt !=
		    obss_color_info->evt_type) {
			pe_debug("%d: Wrong event: %d, skiping",
				 obss_color_info->vdev_id,
				 obss_color_info->evt_type);
			return;
		}
		obss_color_collision_process_color_change(mac_ctx, session,
							  obss_color_info);
		break;
	default:
		pe_err("%d: Invalid event type %d",
		       obss_color_info->vdev_id, obss_color_info->evt_type);
		return;
	}
}
#endif

void lim_remove_duplicate_bssid_node(struct sir_rssi_disallow_lst *entry,
				     qdf_list_t *list)
{
	qdf_list_node_t *cur_list = NULL;
	qdf_list_node_t *next_list = NULL;
	struct sir_rssi_disallow_lst *cur_entry;
	QDF_STATUS status;

	qdf_list_peek_front(list, &cur_list);
	while (cur_list) {
		qdf_list_peek_next(list, cur_list, &next_list);
		cur_entry = qdf_container_of(cur_list,
					     struct sir_rssi_disallow_lst,
					     node);

		/*
		 * Remove the node from blacklisting if the timeout is 0 and
		 * rssi is 0 dbm i.e btm blacklisted entry
		 */
		if ((lim_assoc_rej_get_remaining_delta(cur_entry) == 0) &&
		    cur_entry->expected_rssi == LIM_MIN_RSSI) {
			status = qdf_list_remove_node(list, cur_list);
			if (QDF_IS_STATUS_SUCCESS(status))
				qdf_mem_free(cur_entry);
		} else if (qdf_is_macaddr_equal(&entry->bssid,
						&cur_entry->bssid)) {
			/*
			 * Remove the node if we try to add the same bssid again
			 * Copy the old rssi value alone
			 */
			entry->expected_rssi = cur_entry->expected_rssi;
			status = qdf_list_remove_node(list, cur_list);
			if (QDF_IS_STATUS_SUCCESS(status)) {
				qdf_mem_free(cur_entry);
				break;
			}
		}
		cur_list = next_list;
		next_list = NULL;
	}
}

#define BLACKLIST_MAX_TIME_TO_HONOR      255000

void lim_add_roam_blacklist_ap(tpAniSirGlobal mac_ctx,
			       struct roam_blacklist_event *src_lst)
{
	uint32_t i;
	struct sir_rssi_disallow_lst *entry;
	struct roam_blacklist_timeout *blacklist;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	blacklist = &src_lst->roam_blacklist[0];
	for (i = 0; i < src_lst->num_entries; i++) {
		if (blacklist->timeout > BLACKLIST_MAX_TIME_TO_HONOR) {
			pe_info("%pM timeout %u greater than %d ignoring entry",
				blacklist->bssid.bytes, blacklist->timeout,
				BLACKLIST_MAX_TIME_TO_HONOR);
			blacklist++;
			continue;
		}
		entry = qdf_mem_malloc(sizeof(struct sir_rssi_disallow_lst));
		if (!entry)
			return;

		qdf_copy_macaddr(&entry->bssid, &blacklist->bssid);
		entry->retry_delay = blacklist->timeout;
		entry->time_during_rejection = blacklist->received_time;
		/* set 0dbm as expected rssi for btm blaclisted entries */
		entry->expected_rssi = LIM_MIN_RSSI;

		qdf_mutex_acquire(&mac_ctx->roam.rssi_disallow_bssid_lock);
		lim_remove_duplicate_bssid_node(
					entry,
					&mac_ctx->roam.rssi_disallow_bssid);

		if (qdf_list_size(&mac_ctx->roam.rssi_disallow_bssid) >=
		    MAX_RSSI_AVOID_BSSID_LIST) {
			status = lim_rem_blacklist_entry_with_lowest_delta(
					&mac_ctx->roam.rssi_disallow_bssid);
			if (QDF_IS_STATUS_ERROR(status)) {
				qdf_mutex_release(&mac_ctx->roam.
					rssi_disallow_bssid_lock);
				pe_err("Failed to remove entry with lowest delta");
				qdf_mem_free(entry);
				return;
			}
		}

		if (QDF_IS_STATUS_SUCCESS(status)) {
			status = qdf_list_insert_back(
					&mac_ctx->roam.rssi_disallow_bssid,
					&entry->node);
			if (QDF_IS_STATUS_ERROR(status)) {
				qdf_mutex_release(&mac_ctx->roam.
					rssi_disallow_bssid_lock);
				pe_err("Failed to enqueue bssid: %pM",
				       entry->bssid.bytes);
				qdf_mem_free(entry);
				return;
			}
			pe_debug("Added BTM blacklisted bssid: %pM",
				 entry->bssid.bytes);
		}
		qdf_mutex_release(&mac_ctx->roam.rssi_disallow_bssid_lock);
		blacklist++;
	}
}
