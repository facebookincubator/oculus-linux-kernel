/*
 * Copyright (c) 2011-2018 The Linux Foundation. All rights reserved.
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

/**=========================================================================

   \file  lim_session.c

   \brief implementation for lim Session related APIs

   \author Sunit Bhatia

   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "ani_global.h"
#include "lim_ft_defs.h"
#include "lim_ft.h"
#include "lim_session.h"
#include "lim_utils.h"

#include "sch_api.h"
#include "lim_send_messages.h"


static struct sDphHashNode
	g_dph_node_array[SIR_MAX_SUPPORTED_BSS][SIR_SAP_MAX_NUM_PEERS + 1];

/*--------------------------------------------------------------------------

   \brief pe_init_beacon_params() - Initialize the beaconParams structure

   \param tpPESession          - pointer to the session context or NULL if session can not be created.
   \return void
   \sa

   --------------------------------------------------------------------------*/

static void pe_init_beacon_params(tpAniSirGlobal pMac,
				  tpPESession psessionEntry)
{
	psessionEntry->beaconParams.beaconInterval = 0;
	psessionEntry->beaconParams.fShortPreamble = 0;
	psessionEntry->beaconParams.llaCoexist = 0;
	psessionEntry->beaconParams.llbCoexist = 0;
	psessionEntry->beaconParams.llgCoexist = 0;
	psessionEntry->beaconParams.ht20Coexist = 0;
	psessionEntry->beaconParams.llnNonGFCoexist = 0;
	psessionEntry->beaconParams.fRIFSMode = 0;
	psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport = 0;
	psessionEntry->beaconParams.gHTObssMode = 0;

	/* Number of legacy STAs associated */
	qdf_mem_zero((void *)&psessionEntry->gLim11bParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&psessionEntry->gLim11aParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&psessionEntry->gLim11gParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&psessionEntry->gLimNonGfParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&psessionEntry->gLimHt20Params,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&psessionEntry->gLimLsigTxopParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&psessionEntry->gLimOlbcParams,
		    sizeof(tLimProtStaParams));
}

/*
 * pe_reset_protection_callback() - resets protection structs so that when an AP
 * causing use of protection goes away, corresponding protection bit can be
 * reset
 * @ptr:        pointer to pSessionEntry
 *
 * This function resets protection structs so that when an AP causing use of
 * protection goes away, corresponding protection bit can be reset. This allowes
 * protection bits to be reset once legacy overlapping APs are gone.
 *
 * Return: void
 */
static void pe_reset_protection_callback(void *ptr)
{
	tpPESession pe_session_entry = (tpPESession)ptr;
	tpAniSirGlobal mac_ctx = (tpAniSirGlobal)pe_session_entry->mac_ctx;
	int8_t i = 0;
	tUpdateBeaconParams beacon_params;
	uint16_t current_protection_state = 0;
	tpDphHashNode station_hash_node = NULL;
	tSirMacHTOperatingMode old_op_mode;
	bool bcn_prms_changed = false;

	if (pe_session_entry->valid == false) {
		pe_err("session already deleted. exiting timer callback");
		return;
	}

	/*
	 * During CAC period, if the callback is triggered, the beacon
	 * template may get updated. Subsequently if the vdev is not up, the
	 * vdev would be made up -- which should not happen during the CAC
	 * period. To avoid this, ignore the protection callback if the session
	 * is not yet up.
	 */
	if (!wma_is_vdev_up(pe_session_entry->smeSessionId)) {
		pe_err("session is not up yet. exiting timer callback");
		return;
	}

	/*
	 * If dfsIncludeChanSwIe is set restrat timer as we are going to change
	 * channel and no point in checking protection mode for this channel.
	 */
	if (pe_session_entry->dfsIncludeChanSwIe) {
		pe_err("CSA going on restart timer");
		goto restart_timer;
	}
	current_protection_state |=
	       pe_session_entry->gLimOverlap11gParams.protectionEnabled        |
	       pe_session_entry->gLimOverlap11aParams.protectionEnabled   << 1 |
	       pe_session_entry->gLimOverlapHt20Params.protectionEnabled  << 2 |
	       pe_session_entry->gLimOverlapNonGfParams.protectionEnabled << 3 |
	       pe_session_entry->gLimOlbcParams.protectionEnabled         << 4;

	pe_debug("old protection state: 0x%04X, new protection state: 0x%04X",
		  pe_session_entry->old_protection_state,
		  current_protection_state);

	qdf_mem_zero(&pe_session_entry->gLimOverlap11gParams,
		     sizeof(pe_session_entry->gLimOverlap11gParams));
	qdf_mem_zero(&pe_session_entry->gLimOverlap11aParams,
		     sizeof(pe_session_entry->gLimOverlap11aParams));
	qdf_mem_zero(&pe_session_entry->gLimOverlapHt20Params,
		     sizeof(pe_session_entry->gLimOverlapHt20Params));
	qdf_mem_zero(&pe_session_entry->gLimOverlapNonGfParams,
		     sizeof(pe_session_entry->gLimOverlapNonGfParams));

	qdf_mem_zero(&pe_session_entry->gLimOlbcParams,
		     sizeof(pe_session_entry->gLimOlbcParams));

	/*
	 * Do not reset fShortPreamble and beaconInterval, as they
	 * are not updated.
	 */
	pe_session_entry->beaconParams.llaCoexist = 0;
	pe_session_entry->beaconParams.llbCoexist = 0;
	pe_session_entry->beaconParams.llgCoexist = 0;
	pe_session_entry->beaconParams.ht20Coexist = 0;
	pe_session_entry->beaconParams.llnNonGFCoexist = 0;
	pe_session_entry->beaconParams.fRIFSMode = 0;
	pe_session_entry->beaconParams.fLsigTXOPProtectionFullSupport = 0;
	pe_session_entry->beaconParams.gHTObssMode = 0;


	old_op_mode = pe_session_entry->htOperMode;
	pe_session_entry->htOperMode = eSIR_HT_OP_MODE_PURE;
	mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;

	qdf_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
	/* index 0, is self node, peers start from 1 */
	for (i = 1 ; i <= mac_ctx->lim.gLimAssocStaLimit ; i++) {
		station_hash_node = dph_get_hash_entry(mac_ctx, i,
					&pe_session_entry->dph.dphHashTable);
		if (NULL == station_hash_node)
			continue;
		lim_decide_ap_protection(mac_ctx, station_hash_node->staAddr,
		&beacon_params, pe_session_entry);
	}

	if (pe_session_entry->htOperMode != old_op_mode)
		bcn_prms_changed = true;

	if ((current_protection_state !=
		pe_session_entry->old_protection_state) &&
		(false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)) {
		pe_debug("protection changed, update beacon template");
		/* update beacon fix params and send update to FW */
		qdf_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
		beacon_params.bssIdx = pe_session_entry->bssIdx;
		beacon_params.fShortPreamble =
				pe_session_entry->beaconParams.fShortPreamble;
		beacon_params.beaconInterval =
				pe_session_entry->beaconParams.beaconInterval;
		beacon_params.llaCoexist =
				pe_session_entry->beaconParams.llaCoexist;
		beacon_params.llbCoexist =
				pe_session_entry->beaconParams.llbCoexist;
		beacon_params.llgCoexist =
				pe_session_entry->beaconParams.llgCoexist;
		beacon_params.ht20MhzCoexist =
				pe_session_entry->beaconParams.ht20Coexist;
		beacon_params.llnNonGFCoexist =
				pe_session_entry->beaconParams.llnNonGFCoexist;
		beacon_params.fLsigTXOPProtectionFullSupport =
				pe_session_entry->beaconParams.
					fLsigTXOPProtectionFullSupport;
		beacon_params.fRIFSMode =
				pe_session_entry->beaconParams.fRIFSMode;
		beacon_params.smeSessionId =
				pe_session_entry->smeSessionId;
		beacon_params.paramChangeBitmap |= PARAM_llBCOEXIST_CHANGED;
		bcn_prms_changed = true;
	}

	if (bcn_prms_changed) {
		sch_set_fixed_beacon_fields(mac_ctx, pe_session_entry);
		lim_send_beacon_params(mac_ctx, &beacon_params, pe_session_entry);
	}

	pe_session_entry->old_protection_state = current_protection_state;
restart_timer:
	if (qdf_mc_timer_start(&pe_session_entry->
				protection_fields_reset_timer,
				SCH_PROTECTION_RESET_TIME)
		!= QDF_STATUS_SUCCESS) {
		pe_err("cannot create or start protectionFieldsResetTimer");
	}
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * pe_delete_fils_info: API to delete fils session info
 * @session: pe session
 *
 * Return: void
 */
void pe_delete_fils_info(tpPESession session)
{
	struct pe_fils_session *fils_info;

	if (!session || (session && !session->valid)) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("session is not valid"));
		return;
	}
	fils_info = session->fils_info;
	if (!fils_info) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("fils info not found"));
		return;
	}
	if (fils_info->keyname_nai_data)
		qdf_mem_free(fils_info->keyname_nai_data);
	if (fils_info->fils_erp_reauth_pkt)
		qdf_mem_free(fils_info->fils_erp_reauth_pkt);
	if (fils_info->fils_rrk)
		qdf_mem_free(fils_info->fils_rrk);
	if (fils_info->fils_rik)
		qdf_mem_free(fils_info->fils_rik);
	if (fils_info->fils_eap_finish_pkt)
		qdf_mem_free(fils_info->fils_eap_finish_pkt);
	if (fils_info->fils_rmsk)
		qdf_mem_free(fils_info->fils_rmsk);
	if (fils_info->fils_pmk)
		qdf_mem_free(fils_info->fils_pmk);
	if (fils_info->auth_info.keyname)
		qdf_mem_free(fils_info->auth_info.keyname);
	if (fils_info->auth_info.domain_name)
		qdf_mem_free(fils_info->auth_info.domain_name);
	if (fils_info->hlp_data)
		qdf_mem_free(fils_info->hlp_data);
	qdf_mem_free(fils_info);
	session->fils_info = NULL;
}
/**
 * pe_init_fils_info: API to initialize fils session info elements to null
 * @session: pe session
 *
 * Return: void
 */
static void pe_init_fils_info(tpPESession session)
{
	struct pe_fils_session *fils_info;

	if (!session || (session && !session->valid)) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("session is not valid"));
		return;
	}
	session->fils_info = qdf_mem_malloc(sizeof(struct pe_fils_session));
	fils_info = session->fils_info;
	if (!fils_info) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("fils info not found"));
		return;
	}
	fils_info->keyname_nai_data = NULL;
	fils_info->fils_erp_reauth_pkt = NULL;
	fils_info->fils_rrk = NULL;
	fils_info->fils_rik = NULL;
	fils_info->fils_eap_finish_pkt = NULL;
	fils_info->fils_rmsk = NULL;
	fils_info->fils_pmk = NULL;
	fils_info->auth_info.keyname = NULL;
	fils_info->auth_info.domain_name = NULL;
}
#else
static void pe_delete_fils_info(tpPESession session) { }
static void pe_init_fils_info(tpPESession session) { }
#endif

/**
 * lim_get_peer_idxpool_size: get number of peer idx pool size
 * @num_sta: Max number of STA
 * @bss_type: BSS type
 *
 * The peer index start from 1 and thus index 0 is not used, so
 * add 1 to the max sta. For STA if TDLS is enabled add 2 as
 * index 1 is reserved for peer BSS.
 *
 * Return: number of peer idx pool size
 */
#ifdef FEATURE_WLAN_TDLS
static inline uint8_t
lim_get_peer_idxpool_size(uint16_t num_sta, tSirBssType bss_type)
{
	/*
	 * In station role, index 1 is reserved for peer
	 * corresponding to AP. For TDLS the index should
	 * start from 2
	 */
	if (bss_type == eSIR_INFRASTRUCTURE_MODE)
		return num_sta + 2;
	else
		return num_sta + 1;

}
#else
static inline uint8_t
lim_get_peer_idxpool_size(uint16_t num_sta, tSirBssType bss_type)
{
	return num_sta + 1;
}
#endif

void lim_set_bcn_probe_filter(tpAniSirGlobal mac_ctx,
				tpPESession session,
				tSirMacSSid *ibss_ssid,
				uint8_t sap_channel)
{
	struct mgmt_beacon_probe_filter *filter;
	tSirBssType bss_type;
	uint8_t session_id;
	tSirMacAddr *bssid;

	if (!session) {
		pe_err("Invalid session pointer");
		return;
	}

	bss_type = session->bssType;
	session_id = session->peSessionId;
	bssid = &session->bssId;

	if (session_id >= SIR_MAX_SUPPORTED_BSS) {
		pe_err("Invalid session_id %d of type %d",
			session_id, bss_type);
		return;
	}

	filter = &mac_ctx->bcn_filter;

	if (eSIR_INFRASTRUCTURE_MODE == bss_type) {
		filter->num_sta_sessions++;
		sir_copy_mac_addr(filter->sta_bssid[session_id], *bssid);
		pe_debug("Set filter for STA Session %d bssid "MAC_ADDRESS_STR,
			session_id, MAC_ADDR_ARRAY(*bssid));
	} else if (eSIR_IBSS_MODE == bss_type) {
		if (!ibss_ssid) {
			pe_err("IBSS Type with NULL SSID");
			goto done;
		}
		filter->num_ibss_sessions++;
		filter->ibss_ssid[session_id].length = ibss_ssid->length;
		qdf_mem_copy(&filter->ibss_ssid[session_id].length,
			     ibss_ssid->ssId,
			     ibss_ssid->length);
		pe_debug("Set filter for IBSS session %d ssid %s",
			session_id, ibss_ssid->ssId);
	} else if (eSIR_INFRA_AP_MODE == bss_type) {
		if (!sap_channel) {
			pe_err("SAP Type with invalid channel");
			goto done;
		}
		filter->num_sap_sessions++;
		filter->sap_channel[session_id] = sap_channel;
		pe_debug("Set filter for SAP session %d channel %d",
			session_id, sap_channel);
	}

done:
	pe_debug("sta %d ibss %d sap %d",
		filter->num_sta_sessions, filter->num_ibss_sessions,
		filter->num_sap_sessions);
}

void lim_reset_bcn_probe_filter(tpAniSirGlobal mac_ctx,
				tpPESession session)
{
	struct mgmt_beacon_probe_filter *filter;
	tSirBssType bss_type;
	uint8_t session_id;

	if (!session) {
		pe_err("Invalid session pointer");
		return;
	}

	bss_type = session->bssType;
	session_id = session->peSessionId;

	if (session_id >= SIR_MAX_SUPPORTED_BSS) {
		pe_err("Invalid session_id %d of type %d",
			session_id, bss_type);
		return;
	}

	filter = &mac_ctx->bcn_filter;

	if (eSIR_INFRASTRUCTURE_MODE == bss_type) {
		if (filter->num_sta_sessions)
			filter->num_sta_sessions--;
		qdf_mem_zero(&filter->sta_bssid[session_id],
			    sizeof(tSirMacAddr));
		pe_debug("Cleared STA Filter for session %d", session_id);
	} else if (eSIR_IBSS_MODE == bss_type) {
		if (filter->num_ibss_sessions)
			filter->num_ibss_sessions--;
		filter->ibss_ssid[session_id].length = 0;
		qdf_mem_zero(&filter->ibss_ssid[session_id].ssId,
			    SIR_MAC_MAX_SSID_LENGTH);
		pe_debug("Cleared IBSS Filter for session %d", session_id);
	} else if (eSIR_INFRA_AP_MODE == bss_type) {
		if (filter->num_sap_sessions)
			filter->num_sap_sessions--;
		filter->sap_channel[session_id] = 0;
		pe_debug("Cleared SAP Filter for session %d", session_id);
	}

	pe_debug("sta %d ibss %d sap %d",
		filter->num_sta_sessions, filter->num_ibss_sessions,
		filter->num_sap_sessions);
}

void lim_update_bcn_probe_filter(tpAniSirGlobal mac_ctx,
					tpPESession session)
{
	struct mgmt_beacon_probe_filter *filter;
	tSirBssType bss_type;
	uint8_t session_id;

	if (!session) {
		pe_err("Invalid session pointer");
		return;
	}

	bss_type = session->bssType;
	session_id = session->peSessionId;

	if (session_id >= SIR_MAX_SUPPORTED_BSS) {
		pe_err("Invalid session_id %d of type %d",
			session_id, bss_type);
		return;
	}

	filter = &mac_ctx->bcn_filter;

	if (eSIR_INFRA_AP_MODE == bss_type) {
		filter->sap_channel[session_id] = session->currentOperChannel;
		pe_debug("Updated SAP Filter for session %d channel %d",
			session_id, filter->sap_channel[session_id]);
	} else {
		pe_debug("Invalid session type %d session id %d",
			bss_type, session_id);
	}

	pe_debug("sta %d ibss %d sap %d",
		filter->num_sta_sessions, filter->num_ibss_sessions,
		filter->num_sap_sessions);
}

tpPESession
pe_create_session(tpAniSirGlobal pMac, uint8_t *bssid, uint8_t *sessionId,
		  uint16_t numSta, tSirBssType bssType, uint8_t vdev_id,
		  enum QDF_OPMODE opmode)
{
	QDF_STATUS status;
	uint8_t i;
	tpPESession session_ptr;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		/* Find first free room in session table */
		if (pMac->lim.gpSession[i].valid == true)
			continue;
		break;
	}

	if (i == pMac->lim.maxBssId) {
		pe_err("Session can't be created. Reached max sessions");
		return NULL;
	}

	session_ptr = &pMac->lim.gpSession[i];
	qdf_mem_zero((void *)session_ptr, sizeof(tPESession));
	/* Allocate space for Station Table for this session. */
	session_ptr->dph.dphHashTable.pHashTable =
		qdf_mem_malloc(sizeof(tpDphHashNode) * (numSta + 1));
	if (NULL == session_ptr->dph.dphHashTable.pHashTable) {
		pe_err("memory allocate failed!");
		return NULL;
	}

	session_ptr->dph.dphHashTable.pDphNodeArray = g_dph_node_array[i];
	session_ptr->dph.dphHashTable.size = numSta + 1;
	dph_hash_table_class_init(pMac, &session_ptr->dph.dphHashTable);
	session_ptr->gpLimPeerIdxpool = qdf_mem_malloc(
		sizeof(*(session_ptr->gpLimPeerIdxpool)) *
		lim_get_peer_idxpool_size(numSta, bssType));
	if (NULL == session_ptr->gpLimPeerIdxpool) {
		pe_err("memory allocate failed!");
		qdf_mem_free(session_ptr->dph.dphHashTable.pHashTable);
		qdf_mem_zero(session_ptr->dph.dphHashTable.pDphNodeArray,
			sizeof(struct sDphHashNode) *
			(SIR_SAP_MAX_NUM_PEERS + 1));
		session_ptr->dph.dphHashTable.pHashTable = NULL;
		session_ptr->dph.dphHashTable.pDphNodeArray = NULL;
		return NULL;
	}
	session_ptr->freePeerIdxHead = 0;
	session_ptr->freePeerIdxTail = 0;
	session_ptr->gLimNumOfCurrentSTAs = 0;
	/* Copy the BSSID to the session table */
	sir_copy_mac_addr(session_ptr->bssId, bssid);
	if (bssType == eSIR_MONITOR_MODE)
		sir_copy_mac_addr(pMac->lim.gpSession[i].selfMacAddr, bssid);
	session_ptr->valid = true;
	/* Initialize the SME and MLM states to IDLE */
	session_ptr->limMlmState = eLIM_MLM_IDLE_STATE;
	session_ptr->limSmeState = eLIM_SME_IDLE_STATE;
	session_ptr->limCurrentAuthType = eSIR_OPEN_SYSTEM;
	pe_init_beacon_params(pMac, &pMac->lim.gpSession[i]);
	session_ptr->is11Rconnection = false;
#ifdef FEATURE_WLAN_ESE
	session_ptr->isESEconnection = false;
#endif
	session_ptr->isFastTransitionEnabled = false;
	session_ptr->isFastRoamIniFeatureEnabled = false;
	*sessionId = i;
	session_ptr->peSessionId = i;
	session_ptr->bssType = bssType;
	session_ptr->pePersona = opmode;
	session_ptr->smeSessionId = vdev_id;
	session_ptr->gLimPhyMode = WNI_CFG_PHY_MODE_11G;
	/* Initialize CB mode variables when session is created */
	session_ptr->htSupportedChannelWidthSet = 0;
	session_ptr->htRecommendedTxWidthSet = 0;
	session_ptr->htSecondaryChannelOffset = 0;
#ifdef FEATURE_WLAN_TDLS
	qdf_mem_zero(session_ptr->peerAIDBitmap,
		    sizeof(session_ptr->peerAIDBitmap));
	session_ptr->tdls_prohibited = false;
	session_ptr->tdls_chan_swit_prohibited = false;
#endif
	lim_update_tdls_set_state_for_fw(session_ptr, true);
	session_ptr->fWaitForProbeRsp = 0;
	session_ptr->fIgnoreCapsChange = 0;
	session_ptr->ignore_assoc_disallowed = pMac->ignore_assoc_disallowed;
	session_ptr->is_session_obss_color_collision_det_enabled =
		pMac->lim.global_obss_color_collision_det_offload;

	pe_debug("Create PE session: %d opmode %d vdev_id %d  BSSID: "QDF_MAC_ADDR_STR" Max No of STA: %d",
		 *sessionId, opmode, vdev_id, QDF_MAC_ADDR_ARRAY(bssid),
		 numSta);

	if (eSIR_INFRA_AP_MODE == bssType || eSIR_IBSS_MODE == bssType) {
		session_ptr->pSchProbeRspTemplate =
			qdf_mem_malloc(SIR_MAX_PROBE_RESP_SIZE);
		session_ptr->pSchBeaconFrameBegin =
			qdf_mem_malloc(SIR_MAX_BEACON_SIZE);
		session_ptr->pSchBeaconFrameEnd =
			qdf_mem_malloc(SIR_MAX_BEACON_SIZE);
		if ((NULL == session_ptr->pSchProbeRspTemplate)
		    || (NULL == session_ptr->pSchBeaconFrameBegin)
		    || (NULL == session_ptr->pSchBeaconFrameEnd)) {
			pe_err("memory allocate failed!");
			qdf_mem_free(session_ptr->dph.dphHashTable.pHashTable);
			qdf_mem_zero(
				session_ptr->dph.dphHashTable.pDphNodeArray,
				sizeof(struct sDphHashNode) *
				(SIR_SAP_MAX_NUM_PEERS + 1));
			qdf_mem_free(session_ptr->gpLimPeerIdxpool);
			qdf_mem_free(session_ptr->pSchProbeRspTemplate);
			qdf_mem_free(session_ptr->pSchBeaconFrameBegin);
			qdf_mem_free(session_ptr->pSchBeaconFrameEnd);

			session_ptr->dph.dphHashTable.pHashTable = NULL;
			session_ptr->dph.dphHashTable.pDphNodeArray = NULL;
			session_ptr->gpLimPeerIdxpool = NULL;
			session_ptr->pSchProbeRspTemplate = NULL;
			session_ptr->pSchBeaconFrameBegin = NULL;
			session_ptr->pSchBeaconFrameEnd = NULL;
			return NULL;
		}
	}
	if (eSIR_INFRASTRUCTURE_MODE == bssType)
		lim_ft_open(pMac, &pMac->lim.gpSession[i]);

	if (eSIR_MONITOR_MODE == bssType)
		lim_ft_open(pMac, &pMac->lim.gpSession[i]);

	if (eSIR_INFRA_AP_MODE == bssType) {
		session_ptr->old_protection_state = 0;
		session_ptr->is_session_obss_offload_enabled = false;
		session_ptr->is_obss_reset_timer_initialized = false;
		session_ptr->mac_ctx = (void *)pMac;

		status = qdf_mc_timer_init(&session_ptr->
					   protection_fields_reset_timer,
					   QDF_TIMER_TYPE_SW,
					   pe_reset_protection_callback,
					   (void *)&pMac->lim.gpSession[i]);

		if (QDF_IS_STATUS_ERROR(status))
			pe_err("cannot create protection fields reset timer");
		else
			session_ptr->is_obss_reset_timer_initialized = true;

		status = qdf_mc_timer_init(&session_ptr->ap_ecsa_timer,
					   QDF_TIMER_TYPE_WAKE_APPS,
					   lim_process_ap_ecsa_timeout,
					   (void *)&pMac->lim.gpSession[i]);
		if (status != QDF_STATUS_SUCCESS)
			pe_err("cannot create ap_ecsa_timer");
		qdf_wake_lock_create(&session_ptr->ap_ecsa_wakelock,
				     "ap_ecsa_wakelock");
		qdf_runtime_lock_init(&session_ptr->ap_ecsa_runtime_lock);
	}
	pe_init_fils_info(session_ptr);
	session_ptr->ht_client_cnt = 0;
	/* following is invalid value since seq number is 12 bit */
	session_ptr->prev_auth_seq_num = 0xFFFF;

	return &pMac->lim.gpSession[i];
}

/*--------------------------------------------------------------------------
   \brief pe_find_session_by_bssid() - looks up the PE session given the BSSID.

   This function returns the session context and the session ID if the session
   corresponding to the given BSSID is found in the PE session table.

   \param pMac                   - pointer to global adapter context
   \param bssid                   - BSSID of the session
   \param sessionId             -session ID is returned here, if session is found.

   \return tpPESession          - pointer to the session context or NULL if session is not found.

   \sa
   --------------------------------------------------------------------------*/
tpPESession pe_find_session_by_bssid(tpAniSirGlobal pMac, uint8_t *bssid,
				     uint8_t *sessionId)
{
	uint8_t i;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		/* If BSSID matches return corresponding tables address */
		if ((pMac->lim.gpSession[i].valid)
		    && (sir_compare_mac_addr(pMac->lim.gpSession[i].bssId,
					    bssid))) {
			*sessionId = i;
			return &pMac->lim.gpSession[i];
		}
	}

	return NULL;

}

/*--------------------------------------------------------------------------
   \brief pe_find_session_by_bss_idx() - looks up the PE session given the bssIdx.

   This function returns the session context  if the session
   corresponding to the given bssIdx is found in the PE session table.
   \param pMac                   - pointer to global adapter context
   \param bssIdx                   - bss index of the session
   \return tpPESession          - pointer to the session context or NULL if session is not found.
   \sa
   --------------------------------------------------------------------------*/
tpPESession pe_find_session_by_bss_idx(tpAniSirGlobal pMac, uint8_t bssIdx)
{
	uint8_t i;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		/* If BSSID matches return corresponding tables address */
		if ((pMac->lim.gpSession[i].valid)
		    && (pMac->lim.gpSession[i].bssIdx == bssIdx)) {
			return &pMac->lim.gpSession[i];
		}
	}
	pe_debug("Session lookup fails for bssIdx: %d", bssIdx);
	return NULL;
}

/*--------------------------------------------------------------------------
   \brief pe_find_session_by_session_id() - looks up the PE session given the session ID.

   This function returns the session context  if the session
   corresponding to the given session ID is found in the PE session table.

   \param pMac                   - pointer to global adapter context
   \param sessionId             -session ID for which session context needs to be looked up.

   \return tpPESession          - pointer to the session context or NULL if session is not found.

   \sa
   --------------------------------------------------------------------------*/
tpPESession pe_find_session_by_session_id(tpAniSirGlobal pMac,
					  uint8_t sessionId)
{
	if (sessionId >= pMac->lim.maxBssId) {
		pe_err("Invalid sessionId: %d", sessionId);
		return NULL;
	}

	if (pMac->lim.gpSession[sessionId].valid)
		return &pMac->lim.gpSession[sessionId];

	return NULL;
}

/**
 * pe_find_session_by_sta_id() - looks up the PE session given staid.
 * @mac_ctx:       pointer to global adapter context
 * @staid:         StaId of the session
 * @session_id:    session ID is returned here, if session is found.
 *
 * This function returns the session context and the session ID if the session
 * corresponding to the given StaId is found in the PE session table.
 *
 * Return: session pointer
 */
tpPESession
pe_find_session_by_sta_id(tpAniSirGlobal mac_ctx,
			  uint8_t staid,
			  uint8_t *session_id)
{
	uint8_t i, j;
	tpPESession session_ptr;
	dphHashTableClass *dph_ptr;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if (!mac_ctx->lim.gpSession[i].valid)
			continue;
		session_ptr = &mac_ctx->lim.gpSession[i];
		dph_ptr = &session_ptr->dph.dphHashTable;
		for (j = 0; j < dph_ptr->size; j++) {
			if (dph_ptr->pDphNodeArray[j].valid
			    && dph_ptr->pDphNodeArray[j].added
			    && staid == dph_ptr->pDphNodeArray[j].staIndex) {
				*session_id = i;
				return session_ptr;
			}
		}
	}

	pe_debug("Session lookup fails for StaId: %d", staid);
	return NULL;
}

/**
 * pe_delete_session() - deletes the PE session given the session ID.
 * @mac_ctx: pointer to global adapter context
 * @session: session to be deleted.
 *
 * Deletes the given PE session
 *
 * Return: void
 */
void pe_delete_session(tpAniSirGlobal mac_ctx, tpPESession session)
{
	uint16_t i = 0;
	uint16_t n;
	TX_TIMER *timer_ptr;
	tpSirAssocRsp assoc_rsp;

	if (!session || (session && !session->valid)) {
		pe_debug("session already deleted or not valid");
		return;
	}

	pe_debug("Delete PE session: %d opmode: %d vdev_id: %d BSSID: "QDF_MAC_ADDR_STR,
		 session->peSessionId, session->pePersona,
		 session->smeSessionId,
		 QDF_MAC_ADDR_ARRAY(session->bssId));

	lim_reset_bcn_probe_filter(mac_ctx, session);

	/* Restore default failure timeout */
	if (session->defaultAuthFailureTimeout) {
		pe_debug("Restore default failure timeout");
		cfg_set_int(mac_ctx, WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT,
				session->defaultAuthFailureTimeout);
	}

	for (n = 0; n < (mac_ctx->lim.maxStation + 1); n++) {
		timer_ptr = &mac_ctx->lim.limTimers.gpLimCnfWaitTimer[n];
		if (session->peSessionId == timer_ptr->sessionId)
			if (true == tx_timer_running(timer_ptr))
				tx_timer_deactivate(timer_ptr);
	}

	if (LIM_IS_AP_ROLE(session)) {
		qdf_runtime_lock_deinit(&session->ap_ecsa_runtime_lock);
		qdf_wake_lock_destroy(&session->ap_ecsa_wakelock);
		qdf_mc_timer_stop(&session->protection_fields_reset_timer);
		qdf_mc_timer_destroy(&session->protection_fields_reset_timer);
		session->dfsIncludeChanSwIe = 0;
		qdf_mc_timer_stop(&session->ap_ecsa_timer);
		qdf_mc_timer_destroy(&session->ap_ecsa_timer);
		lim_del_pmf_sa_query_timer(mac_ctx, session);
	}

	/* Delete FT related information */
	lim_ft_cleanup(mac_ctx, session);
	if (session->pLimStartBssReq != NULL) {
		qdf_mem_free(session->pLimStartBssReq);
		session->pLimStartBssReq = NULL;
	}

	if (session->pLimJoinReq != NULL) {
		qdf_mem_free(session->pLimJoinReq);
		session->pLimJoinReq = NULL;
	}

	if (session->pLimReAssocReq != NULL) {
		qdf_mem_free(session->pLimReAssocReq);
		session->pLimReAssocReq = NULL;
	}

	if (session->pLimMlmJoinReq != NULL) {
		qdf_mem_free(session->pLimMlmJoinReq);
		session->pLimMlmJoinReq = NULL;
	}

	if (session->dph.dphHashTable.pHashTable != NULL) {
		qdf_mem_free(session->dph.dphHashTable.pHashTable);
		session->dph.dphHashTable.pHashTable = NULL;
	}

	if (session->dph.dphHashTable.pDphNodeArray != NULL) {
		qdf_mem_zero(session->dph.dphHashTable.pDphNodeArray,
			sizeof(struct sDphHashNode) *
			(SIR_SAP_MAX_NUM_PEERS + 1));
		session->dph.dphHashTable.pDphNodeArray = NULL;
	}

	if (session->gpLimPeerIdxpool != NULL) {
		qdf_mem_free(session->gpLimPeerIdxpool);
		session->gpLimPeerIdxpool = NULL;
	}

	if (session->beacon != NULL) {
		qdf_mem_free(session->beacon);
		session->beacon = NULL;
		session->bcnLen = 0;
	}

	if (session->assocReq != NULL) {
		qdf_mem_free(session->assocReq);
		session->assocReq = NULL;
		session->assocReqLen = 0;
	}

	if (session->assocRsp != NULL) {
		qdf_mem_free(session->assocRsp);
		session->assocRsp = NULL;
		session->assocRspLen = 0;
	}

	if (session->parsedAssocReq != NULL) {
		tpSirAssocReq tmp_ptr = NULL;
		/* Cleanup the individual allocation first */
		for (i = 0; i < session->dph.dphHashTable.size; i++) {
			if (session->parsedAssocReq[i] == NULL)
				continue;
			tmp_ptr = ((tpSirAssocReq)
				  (session->parsedAssocReq[i]));
			if (tmp_ptr->assocReqFrame) {
				qdf_mem_free(tmp_ptr->assocReqFrame);
				tmp_ptr->assocReqFrame = NULL;
				tmp_ptr->assocReqFrameLength = 0;
			}
			qdf_mem_free(session->parsedAssocReq[i]);
			session->parsedAssocReq[i] = NULL;
		}
		/* Cleanup the whole block */
		qdf_mem_free(session->parsedAssocReq);
		session->parsedAssocReq = NULL;
	}
	if (NULL != session->limAssocResponseData) {
		assoc_rsp = (tpSirAssocRsp) session->limAssocResponseData;
		qdf_mem_free(assoc_rsp->sha384_ft_subelem.gtk);
		qdf_mem_free(assoc_rsp->sha384_ft_subelem.igtk);
		qdf_mem_free(session->limAssocResponseData);
		session->limAssocResponseData = NULL;
	}
	if (NULL != session->pLimMlmReassocRetryReq) {
		qdf_mem_free(session->pLimMlmReassocRetryReq);
		session->pLimMlmReassocRetryReq = NULL;
	}
	if (NULL != session->pLimMlmReassocReq) {
		qdf_mem_free(session->pLimMlmReassocReq);
		session->pLimMlmReassocReq = NULL;
	}

	if (NULL != session->pSchProbeRspTemplate) {
		qdf_mem_free(session->pSchProbeRspTemplate);
		session->pSchProbeRspTemplate = NULL;
	}

	if (NULL != session->pSchBeaconFrameBegin) {
		qdf_mem_free(session->pSchBeaconFrameBegin);
		session->pSchBeaconFrameBegin = NULL;
	}

	if (NULL != session->pSchBeaconFrameEnd) {
		qdf_mem_free(session->pSchBeaconFrameEnd);
		session->pSchBeaconFrameEnd = NULL;
	}

	/* Must free the buffer before peSession invalid */
	if (NULL != session->addIeParams.probeRespData_buff) {
		qdf_mem_free(session->addIeParams.probeRespData_buff);
		session->addIeParams.probeRespData_buff = NULL;
		session->addIeParams.probeRespDataLen = 0;
	}
	if (NULL != session->addIeParams.assocRespData_buff) {
		qdf_mem_free(session->addIeParams.assocRespData_buff);
		session->addIeParams.assocRespData_buff = NULL;
		session->addIeParams.assocRespDataLen = 0;
	}
	if (NULL != session->addIeParams.probeRespBCNData_buff) {
		qdf_mem_free(session->addIeParams.probeRespBCNData_buff);
		session->addIeParams.probeRespBCNData_buff = NULL;
		session->addIeParams.probeRespBCNDataLen = 0;
	}
	pe_delete_fils_info(session);
	session->valid = false;

	qdf_mem_zero(session->WEPKeyMaterial,
		     sizeof(session->WEPKeyMaterial));
	if (session->access_policy_vendor_ie)
		qdf_mem_free(session->access_policy_vendor_ie);

	session->access_policy_vendor_ie = NULL;

	if (LIM_IS_AP_ROLE(session))
		lim_check_and_reset_protection_params(mac_ctx);

	return;
}

/*--------------------------------------------------------------------------
   \brief pe_find_session_by_peer_sta() - looks up the PE session given the Station Address.

   This function returns the session context and the session ID if the session
   corresponding to the given station address is found in the PE session table.

   \param pMac                   - pointer to global adapter context
   \param sa                       - Peer STA Address of the session
   \param sessionId             -session ID is returned here, if session is found.

   \return tpPESession          - pointer to the session context or NULL if session is not found.

   \sa
   --------------------------------------------------------------------------*/

tpPESession pe_find_session_by_peer_sta(tpAniSirGlobal pMac, uint8_t *sa,
					uint8_t *sessionId)
{
	uint8_t i;
	tpDphHashNode pSta;
	uint16_t aid;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		if ((pMac->lim.gpSession[i].valid)) {
			pSta =
				dph_lookup_hash_entry(pMac, sa, &aid,
						      &pMac->lim.gpSession[i].dph.
						      dphHashTable);
			if (pSta != NULL) {
				*sessionId = i;
				return &pMac->lim.gpSession[i];
			}
		}
	}

	pe_debug("Session lookup fails for Peer StaId: %pM", sa);

	return NULL;
}

/**
 * pe_find_session_by_sme_session_id() - looks up the PE session for given sme
 * session id
 * @mac_ctx:          pointer to global adapter context
 * @sme_session_id:   sme session id
 *
 * looks up the PE session for given sme session id
 *
 * Return: pe session entry for given sme session if found else NULL
 */
tpPESession pe_find_session_by_sme_session_id(tpAniSirGlobal mac_ctx,
					      uint8_t sme_session_id)
{
	uint8_t i;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if ((mac_ctx->lim.gpSession[i].valid) &&
		     (mac_ctx->lim.gpSession[i].smeSessionId ==
			sme_session_id)) {
			return &mac_ctx->lim.gpSession[i];
		}
	}
	return NULL;
}

/**
 * pe_get_active_session_count() - function to return active pe session count
 *
 * @mac_ctx: pointer to global mac structure
 *
 * returns number of active pe session count
 *
 * Return: 0 if there are no active sessions else return number of active
 *          sessions
 */
uint8_t pe_get_active_session_count(tpAniSirGlobal mac_ctx)
{
	uint8_t i, active_session_count = 0;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++)
		if (mac_ctx->lim.gpSession[i].valid)
			active_session_count++;

	return active_session_count;
}
