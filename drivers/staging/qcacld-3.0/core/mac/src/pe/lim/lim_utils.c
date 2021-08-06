/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * This file lim_utils.cc contains the utility functions
 * LIM uses.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "sch_api.h"
#include "lim_utils.h"
#include "lim_types.h"
#include "lim_security_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_send_messages.h"
#include "lim_ser_des_utils.h"
#include "lim_admit_control.h"
#include "lim_sta_hash_api.h"
#include "dot11f.h"
#include "dot11fdefs.h"
#include "wmm_apsd.h"
#include "lim_trace.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include "host_diag_core_event.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
#include "lim_ibss_peer_mgmt.h"
#include "lim_session_utils.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "cds_reg_service.h"
#include "nan_datapath.h"
#include "wma.h"
#include "wlan_reg_services_api.h"
#include "wlan_policy_mgr_api.h"
#ifdef WLAN_FEATURE_11AX_BSS_COLOR
#include "wma_he.h"
#endif
#include "wlan_utility.h"
#include "wlan_mlme_main.h"

#ifdef WLAN_FEATURE_11W
#include "wni_cfg.h"
#endif
#define ASCII_SPACE_CHARACTER 0x20

/** -------------------------------------------------------------
   \fn lim_delete_dialogue_token_list
   \brief deletes the complete lim dialogue token linked list.
   \param     tpAniSirGlobal    pMac
   \return     None
   -------------------------------------------------------------*/
void lim_delete_dialogue_token_list(tpAniSirGlobal pMac)
{
	tpDialogueToken pCurrNode = pMac->lim.pDialogueTokenHead;

	while (NULL != pMac->lim.pDialogueTokenHead) {
		pCurrNode = pMac->lim.pDialogueTokenHead;
		pMac->lim.pDialogueTokenHead =
			pMac->lim.pDialogueTokenHead->next;
		qdf_mem_free(pCurrNode);
		pCurrNode = NULL;
	}
	pMac->lim.pDialogueTokenTail = NULL;
}

char *lim_dot11_reason_str(uint16_t reasonCode)
{
	switch (reasonCode) {
	case 0:
		return " ";
		CASE_RETURN_STRING(eSIR_MAC_UNSPEC_FAILURE_REASON);
		CASE_RETURN_STRING(eSIR_MAC_PREV_AUTH_NOT_VALID_REASON);
		CASE_RETURN_STRING(eSIR_MAC_DEAUTH_LEAVING_BSS_REASON);
		CASE_RETURN_STRING(eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON);
		CASE_RETURN_STRING(eSIR_MAC_DISASSOC_DUE_TO_DISABILITY_REASON);
		CASE_RETURN_STRING
			(eSIR_MAC_CLASS2_FRAME_FROM_NON_AUTH_STA_REASON);
		CASE_RETURN_STRING
			(eSIR_MAC_CLASS3_FRAME_FROM_NON_ASSOC_STA_REASON);
		CASE_RETURN_STRING(eSIR_MAC_DISASSOC_LEAVING_BSS_REASON);
		CASE_RETURN_STRING(eSIR_MAC_STA_NOT_PRE_AUTHENTICATED_REASON);
		CASE_RETURN_STRING(eSIR_MAC_PWR_CAPABILITY_BAD_REASON);
		CASE_RETURN_STRING(eSIR_MAC_SPRTD_CHANNELS_BAD_REASON);

		CASE_RETURN_STRING(eSIR_MAC_INVALID_IE_REASON);
		CASE_RETURN_STRING(eSIR_MAC_MIC_FAILURE_REASON);
		CASE_RETURN_STRING(eSIR_MAC_4WAY_HANDSHAKE_TIMEOUT_REASON);
		CASE_RETURN_STRING(eSIR_MAC_GR_KEY_UPDATE_TIMEOUT_REASON);
		CASE_RETURN_STRING(eSIR_MAC_RSN_IE_MISMATCH_REASON);

		CASE_RETURN_STRING(eSIR_MAC_INVALID_MC_CIPHER_REASON);
		CASE_RETURN_STRING(eSIR_MAC_INVALID_UC_CIPHER_REASON);
		CASE_RETURN_STRING(eSIR_MAC_INVALID_AKMP_REASON);
		CASE_RETURN_STRING(eSIR_MAC_UNSUPPORTED_RSN_IE_VER_REASON);
		CASE_RETURN_STRING(eSIR_MAC_INVALID_RSN_CAPABILITIES_REASON);
		CASE_RETURN_STRING(eSIR_MAC_1X_AUTH_FAILURE_REASON);
		CASE_RETURN_STRING(eSIR_MAC_CIPHER_SUITE_REJECTED_REASON);
#ifdef FEATURE_WLAN_TDLS
		CASE_RETURN_STRING(eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE);
		CASE_RETURN_STRING(eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON);
#endif
		/* Reserved   27 - 30 */
#ifdef WLAN_FEATURE_11W
		CASE_RETURN_STRING
			(eSIR_MAC_ROBUST_MGMT_FRAMES_POLICY_VIOLATION);
#endif
		CASE_RETURN_STRING(eSIR_MAC_QOS_UNSPECIFIED_REASON);
		CASE_RETURN_STRING(eSIR_MAC_QAP_NO_BANDWIDTH_REASON);
		CASE_RETURN_STRING(eSIR_MAC_XS_UNACKED_FRAMES_REASON);
		CASE_RETURN_STRING(eSIR_MAC_BAD_TXOP_USE_REASON);
		CASE_RETURN_STRING(eSIR_MAC_PEER_STA_REQ_LEAVING_BSS_REASON);
		CASE_RETURN_STRING(eSIR_MAC_PEER_REJECT_MECHANISIM_REASON);
		CASE_RETURN_STRING(eSIR_MAC_MECHANISM_NOT_SETUP_REASON);

		CASE_RETURN_STRING(eSIR_MAC_PEER_TIMEDOUT_REASON);
		CASE_RETURN_STRING(eSIR_MAC_CIPHER_NOT_SUPPORTED_REASON);
		CASE_RETURN_STRING(eSIR_MAC_DISASSOC_DUE_TO_FTHANDOFF_REASON);
	/* Reserved 47 - 65535 */
	default:
		return "Unknown";
	}
}

char *lim_mlm_state_str(tLimMlmStates state)
{
	switch (state) {
	case eLIM_MLM_OFFLINE_STATE:
		return "eLIM_MLM_OFFLINE_STATE";
	case eLIM_MLM_IDLE_STATE:
		return "eLIM_MLM_IDLE_STATE";
	case eLIM_MLM_WT_PROBE_RESP_STATE:
		return "eLIM_MLM_WT_PROBE_RESP_STATE";
	case eLIM_MLM_PASSIVE_SCAN_STATE:
		return "eLIM_MLM_PASSIVE_SCAN_STATE";
	case eLIM_MLM_WT_JOIN_BEACON_STATE:
		return "eLIM_MLM_WT_JOIN_BEACON_STATE";
	case eLIM_MLM_JOINED_STATE:
		return "eLIM_MLM_JOINED_STATE";
	case eLIM_MLM_BSS_STARTED_STATE:
		return "eLIM_MLM_BSS_STARTED_STATE";
	case eLIM_MLM_WT_AUTH_FRAME2_STATE:
		return "eLIM_MLM_WT_AUTH_FRAME2_STATE";
	case eLIM_MLM_WT_AUTH_FRAME3_STATE:
		return "eLIM_MLM_WT_AUTH_FRAME3_STATE";
	case eLIM_MLM_WT_AUTH_FRAME4_STATE:
		return "eLIM_MLM_WT_AUTH_FRAME4_STATE";
	case eLIM_MLM_AUTH_RSP_TIMEOUT_STATE:
		return "eLIM_MLM_AUTH_RSP_TIMEOUT_STATE";
	case eLIM_MLM_AUTHENTICATED_STATE:
		return "eLIM_MLM_AUTHENTICATED_STATE";
	case eLIM_MLM_WT_ASSOC_RSP_STATE:
		return "eLIM_MLM_WT_ASSOC_RSP_STATE";
	case eLIM_MLM_WT_REASSOC_RSP_STATE:
		return "eLIM_MLM_WT_REASSOC_RSP_STATE";
	case eLIM_MLM_WT_FT_REASSOC_RSP_STATE:
		return "eLIM_MLM_WT_FT_REASSOC_RSP_STATE";
	case eLIM_MLM_WT_DEL_STA_RSP_STATE:
		return "eLIM_MLM_WT_DEL_STA_RSP_STATE";
	case eLIM_MLM_WT_DEL_BSS_RSP_STATE:
		return "eLIM_MLM_WT_DEL_BSS_RSP_STATE";
	case eLIM_MLM_WT_ADD_STA_RSP_STATE:
		return "eLIM_MLM_WT_ADD_STA_RSP_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_STATE";
	case eLIM_MLM_REASSOCIATED_STATE:
		return "eLIM_MLM_REASSOCIATED_STATE";
	case eLIM_MLM_LINK_ESTABLISHED_STATE:
		return "eLIM_MLM_LINK_ESTABLISHED_STATE";
	case eLIM_MLM_WT_ASSOC_CNF_STATE:
		return "eLIM_MLM_WT_ASSOC_CNF_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE";
	case eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE:
		return "eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE";
	case eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE:
		return "eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE";
	case eLIM_MLM_WT_SET_BSS_KEY_STATE:
		return "eLIM_MLM_WT_SET_BSS_KEY_STATE";
	case eLIM_MLM_WT_SET_STA_KEY_STATE:
		return "eLIM_MLM_WT_SET_STA_KEY_STATE";
	default:
		return "INVALID MLM state";
	}
}

void
lim_print_mlm_state(tpAniSirGlobal pMac, uint16_t logLevel, tLimMlmStates state)
{
	pe_debug("Mlm state: %s", lim_mlm_state_str(state));
}

char *lim_sme_state_str(tLimSmeStates state)
{
	switch (state) {
	case eLIM_SME_OFFLINE_STATE:
		return "eLIM_SME_OFFLINE_STATE";
	case  eLIM_SME_IDLE_STATE:
		return "eLIM_SME_OFFLINE_STATE";
	case eLIM_SME_SUSPEND_STATE:
		return "eLIM_SME_SUSPEND_STATE";
	case eLIM_SME_WT_SCAN_STATE:
		return "eLIM_SME_WT_SCAN_STATE";
	case eLIM_SME_WT_JOIN_STATE:
		return "eLIM_SME_WT_JOIN_STATE";
	case eLIM_SME_WT_AUTH_STATE:
		return "eLIM_SME_WT_AUTH_STATE";
	case eLIM_SME_WT_ASSOC_STATE:
		return "eLIM_SME_WT_ASSOC_STATE";
	case eLIM_SME_WT_REASSOC_STATE:
		return "eLIM_SME_WT_REASSOC_STATE";
	case eLIM_SME_JOIN_FAILURE_STATE:
		return "eLIM_SME_JOIN_FAILURE_STATE";
	case eLIM_SME_ASSOCIATED_STATE:
		return "eLIM_SME_ASSOCIATED_STATE";
	case eLIM_SME_REASSOCIATED_STATE:
		return "eLIM_SME_REASSOCIATED_STATE";
	case eLIM_SME_LINK_EST_STATE:
		return "eLIM_SME_LINK_EST_STATE";
	case eLIM_SME_LINK_EST_WT_SCAN_STATE:
		return "eLIM_SME_LINK_EST_WT_SCAN_STATE";
	case eLIM_SME_WT_PRE_AUTH_STATE:
		return "eLIM_SME_WT_PRE_AUTH_STATE";
	case eLIM_SME_WT_DISASSOC_STATE:
		return "eLIM_SME_WT_DISASSOC_STATE";
	case eLIM_SME_WT_DEAUTH_STATE:
		return "eLIM_SME_WT_DEAUTH_STATE";
	case eLIM_SME_WT_START_BSS_STATE:
		return "eLIM_SME_WT_START_BSS_STATE";
	case eLIM_SME_WT_STOP_BSS_STATE:
		return "eLIM_SME_WT_STOP_BSS_STATE";
	case eLIM_SME_NORMAL_STATE:
		return "eLIM_SME_NORMAL_STATE";
	case eLIM_SME_CHANNEL_SCAN_STATE:
		return "eLIM_SME_CHANNEL_SCAN_STATE";
	case eLIM_SME_NORMAL_CHANNEL_SCAN_STATE:
		return "eLIM_SME_NORMAL_CHANNEL_SCAN_STATE";
	default:
		return "INVALID SME STATE";
	}
}

void
lim_print_sme_state(tpAniSirGlobal pMac, uint16_t logLevel, tLimSmeStates state)
{
	pe_debug("SME state: %s", lim_sme_state_str(state));
}

char *lim_msg_str(uint32_t msgType)
{
#ifdef FIXME_GEN6
	switch (msgType) {
	case eWNI_SME_SYS_READY_IND:
		return "eWNI_SME_SYS_READY_IND";
	case eWNI_SME_JOIN_REQ:
		return "eWNI_SME_JOIN_REQ";
	case eWNI_SME_JOIN_RSP:
		return "eWNI_SME_JOIN_RSP";
	case eWNI_SME_SETCONTEXT_REQ:
		return "eWNI_SME_SETCONTEXT_REQ";
	case eWNI_SME_SETCONTEXT_RSP:
		return "eWNI_SME_SETCONTEXT_RSP";
	case eWNI_SME_REASSOC_REQ:
		return "eWNI_SME_REASSOC_REQ";
	case eWNI_SME_REASSOC_RSP:
		return "eWNI_SME_REASSOC_RSP";
	case eWNI_SME_DISASSOC_REQ:
		return "eWNI_SME_DISASSOC_REQ";
	case eWNI_SME_DISASSOC_RSP:
		return "eWNI_SME_DISASSOC_RSP";
	case eWNI_SME_DISASSOC_IND:
		return "eWNI_SME_DISASSOC_IND";
	case eWNI_SME_DISASSOC_CNF:
		return "eWNI_SME_DISASSOC_CNF";
	case eWNI_SME_DEAUTH_REQ:
		return "eWNI_SME_DEAUTH_REQ";
	case eWNI_SME_DEAUTH_RSP:
		return "eWNI_SME_DEAUTH_RSP";
	case eWNI_SME_DEAUTH_IND:
		return "eWNI_SME_DEAUTH_IND";
	case eWNI_SME_WM_STATUS_CHANGE_NTF:
		return "eWNI_SME_WM_STATUS_CHANGE_NTF";
	case eWNI_SME_START_BSS_REQ:
		return "eWNI_SME_START_BSS_REQ";
	case eWNI_SME_START_BSS_RSP:
		return "eWNI_SME_START_BSS_RSP";
	case eWNI_SME_ASSOC_IND:
		return "eWNI_SME_ASSOC_IND";
	case eWNI_SME_ASSOC_CNF:
		return "eWNI_SME_ASSOC_CNF";
	case eWNI_SME_SWITCH_CHL_IND:
		return "eWNI_SME_SWITCH_CHL_IND";
	case eWNI_SME_STOP_BSS_REQ:
		return "eWNI_SME_STOP_BSS_REQ";
	case eWNI_SME_STOP_BSS_RSP:
		return "eWNI_SME_STOP_BSS_RSP";
	case eWNI_SME_DEAUTH_CNF:
		return "eWNI_SME_DEAUTH_CNF";
	case eWNI_SME_ADDTS_REQ:
		return "eWNI_SME_ADDTS_REQ";
	case eWNI_SME_ADDTS_RSP:
		return "eWNI_SME_ADDTS_RSP";
	case eWNI_SME_DELTS_REQ:
		return "eWNI_SME_DELTS_REQ";
	case eWNI_SME_DELTS_RSP:
		return "eWNI_SME_DELTS_RSP";
	case eWNI_SME_DELTS_IND:
		return "eWNI_SME_DELTS_IND";
	case WMA_SUSPEND_ACTIVITY_RSP:
		return "WMA_SUSPEND_ACTIVITY_RSP";
	case SIR_LIM_RETRY_INTERRUPT_MSG:
		return "SIR_LIM_RETRY_INTERRUPT_MSG";
	case SIR_BB_XPORT_MGMT_MSG:
		return "SIR_BB_XPORT_MGMT_MSG";
	case SIR_LIM_INV_KEY_INTERRUPT_MSG:
		return "SIR_LIM_INV_KEY_INTERRUPT_MSG";
	case SIR_LIM_KEY_ID_INTERRUPT_MSG:
		return "SIR_LIM_KEY_ID_INTERRUPT_MSG";
	case SIR_LIM_REPLAY_THRES_INTERRUPT_MSG:
		return "SIR_LIM_REPLAY_THRES_INTERRUPT_MSG";
	case SIR_LIM_JOIN_FAIL_TIMEOUT:
		return "SIR_LIM_JOIN_FAIL_TIMEOUT";
	case SIR_LIM_AUTH_FAIL_TIMEOUT:
		return "SIR_LIM_AUTH_FAIL_TIMEOUT";
	case SIR_LIM_AUTH_RSP_TIMEOUT:
		return "SIR_LIM_AUTH_RSP_TIMEOUT";
	case SIR_LIM_ASSOC_FAIL_TIMEOUT:
		return "SIR_LIM_ASSOC_FAIL_TIMEOUT";
	case SIR_LIM_REASSOC_FAIL_TIMEOUT:
		return "SIR_LIM_REASSOC_FAIL_TIMEOUT";
	case SIR_LIM_HEART_BEAT_TIMEOUT:
		return "SIR_LIM_HEART_BEAT_TIMEOUT";
	case SIR_LIM_ADDTS_RSP_TIMEOUT:
		return "SIR_LIM_ADDTS_RSP_TIMEOUT";
	case SIR_LIM_LINK_TEST_DURATION_TIMEOUT:
		return "SIR_LIM_LINK_TEST_DURATION_TIMEOUT";
	case SIR_LIM_HASH_MISS_THRES_TIMEOUT:
		return "SIR_LIM_HASH_MISS_THRES_TIMEOUT";
	case SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT:
		return "SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT";
	case SIR_LIM_CNF_WAIT_TIMEOUT:
		return "SIR_LIM_CNF_WAIT_TIMEOUT";
	case SIR_LIM_RADAR_DETECT_IND:
		return "SIR_LIM_RADAR_DETECT_IND";
	case SIR_LIM_FT_PREAUTH_RSP_TIMEOUT:
		return "SIR_LIM_FT_PREAUTH_RSP_TIMEOUT";
	case WNI_CFG_PARAM_UPDATE_IND:
		return "WNI_CFG_PARAM_UPDATE_IND";
	case WNI_CFG_DNLD_REQ:
		return "WNI_CFG_DNLD_REQ";
	case WNI_CFG_DNLD_CNF:
		return "WNI_CFG_DNLD_CNF";
	case WNI_CFG_GET_RSP:
		return "WNI_CFG_GET_RSP";
	case WNI_CFG_SET_CNF:
		return "WNI_CFG_SET_CNF";
	case WNI_CFG_GET_ATTRIB_RSP:
		return "WNI_CFG_GET_ATTRIB_RSP";
	case WNI_CFG_ADD_GRP_ADDR_CNF:
		return "WNI_CFG_ADD_GRP_ADDR_CNF";
	case WNI_CFG_DEL_GRP_ADDR_CNF:
		return "WNI_CFG_DEL_GRP_ADDR_CNF";
	case ANI_CFG_GET_RADIO_STAT_RSP:
		return "ANI_CFG_GET_RADIO_STAT_RSP";
	case ANI_CFG_GET_PER_STA_STAT_RSP:
		return "ANI_CFG_GET_PER_STA_STAT_RSP";
	case ANI_CFG_GET_AGG_STA_STAT_RSP:
		return "ANI_CFG_GET_AGG_STA_STAT_RSP";
	case ANI_CFG_CLEAR_STAT_RSP:
		return "ANI_CFG_CLEAR_STAT_RSP";
	case WNI_CFG_DNLD_RSP:
		return "WNI_CFG_DNLD_RSP";
	case WNI_CFG_GET_REQ:
		return "WNI_CFG_GET_REQ";
	case eWNI_PMC_ENTER_IMPS_RSP:
		return "eWNI_PMC_ENTER_IMPS_RSP";
	case eWNI_PMC_EXIT_IMPS_RSP:
		return "eWNI_PMC_EXIT_IMPS_RSP";
	case eWNI_PMC_ENTER_BMPS_RSP:
		return "eWNI_PMC_ENTER_BMPS_RSP";
	case eWNI_PMC_EXIT_BMPS_RSP:
		return "eWNI_PMC_EXIT_BMPS_RSP";
	case eWNI_PMC_EXIT_BMPS_IND:
		return "eWNI_PMC_EXIT_BMPS_IND";
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_REQ:
		return "eWNI_SME_GET_TSM_STATS_REQ";
	case eWNI_SME_GET_TSM_STATS_RSP:
		return "eWNI_SME_GET_TSM_STATS_RSP";
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_CSA_OFFLOAD_EVENT:
		return "eWNI_SME_CSA_OFFLOAD_EVENT";
	case eWNI_SME_SET_HW_MODE_REQ:
		return "eWNI_SME_SET_HW_MODE_REQ";
	case eWNI_SME_SET_HW_MODE_RESP:
		return "eWNI_SME_SET_HW_MODE_RESP";
	case eWNI_SME_HW_MODE_TRANS_IND:
		return "eWNI_SME_HW_MODE_TRANS_IND";
	default:
		return "Unknown";
	}
#endif
	return "";
}

char *lim_result_code_str(tSirResultCodes resultCode)
{
	switch (resultCode) {
	case eSIR_SME_SUCCESS:
		return "eSIR_SME_SUCCESS";
	case eSIR_LOGE_EXCEPTION:
		return "eSIR_LOGE_EXCEPTION";
	case eSIR_SME_INVALID_PARAMETERS:
		return "eSIR_SME_INVALID_PARAMETERS";
	case eSIR_SME_UNEXPECTED_REQ_RESULT_CODE:
		return "eSIR_SME_UNEXPECTED_REQ_RESULT_CODE";
	case eSIR_SME_RESOURCES_UNAVAILABLE:
		return "eSIR_SME_RESOURCES_UNAVAILABLE";
	case eSIR_SME_SCAN_FAILED:
		return "eSIR_SME_SCAN_FAILED";
	case eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED:
		return "eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED";
	case eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE:
		return "eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE";
	case eSIR_SME_REFUSED:
		return "eSIR_SME_REFUSED";
	case eSIR_SME_JOIN_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_JOIN_TIMEOUT_RESULT_CODE";
	case eSIR_SME_AUTH_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_AUTH_TIMEOUT_RESULT_CODE";
	case eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE";
	case eSIR_SME_REASSOC_TIMEOUT_RESULT_CODE:
		return "eSIR_SME_REASSOC_TIMEOUT_RESULT_CODE";
	case eSIR_SME_MAX_NUM_OF_PRE_AUTH_REACHED:
		return "eSIR_SME_MAX_NUM_OF_PRE_AUTH_REACHED";
	case eSIR_SME_AUTH_REFUSED:
		return "eSIR_SME_AUTH_REFUSED";
	case eSIR_SME_INVALID_WEP_DEFAULT_KEY:
		return "eSIR_SME_INVALID_WEP_DEFAULT_KEY";
	case eSIR_SME_ASSOC_REFUSED:
		return "eSIR_SME_ASSOC_REFUSED";
	case eSIR_SME_REASSOC_REFUSED:
		return "eSIR_SME_REASSOC_REFUSED";
	case eSIR_SME_STA_NOT_AUTHENTICATED:
		return "eSIR_SME_STA_NOT_AUTHENTICATED";
	case eSIR_SME_STA_NOT_ASSOCIATED:
		return "eSIR_SME_STA_NOT_ASSOCIATED";
	case eSIR_SME_ALREADY_JOINED_A_BSS:
		return "eSIR_SME_ALREADY_JOINED_A_BSS";
	case eSIR_SME_MORE_SCAN_RESULTS_FOLLOW:
		return "eSIR_SME_MORE_SCAN_RESULTS_FOLLOW";
	case eSIR_SME_INVALID_ASSOC_RSP_RXED:
		return "eSIR_SME_INVALID_ASSOC_RSP_RXED";
	case eSIR_SME_MIC_COUNTER_MEASURES:
		return "eSIR_SME_MIC_COUNTER_MEASURES";
	case eSIR_SME_ADDTS_RSP_TIMEOUT:
		return "eSIR_SME_ADDTS_RSP_TIMEOUT";
	case eSIR_SME_CHANNEL_SWITCH_FAIL:
		return "eSIR_SME_CHANNEL_SWITCH_FAIL";
	case eSIR_SME_HAL_SCAN_INIT_FAILED:
		return "eSIR_SME_HAL_SCAN_INIT_FAILED";
	case eSIR_SME_HAL_SCAN_END_FAILED:
		return "eSIR_SME_HAL_SCAN_END_FAILED";
	case eSIR_SME_HAL_SCAN_FINISH_FAILED:
		return "eSIR_SME_HAL_SCAN_FINISH_FAILED";
	case eSIR_SME_HAL_SEND_MESSAGE_FAIL:
		return "eSIR_SME_HAL_SEND_MESSAGE_FAIL";

	default:
		return "Unknown resultCode";
	}
}

void lim_print_msg_name(tpAniSirGlobal pMac, uint16_t logLevel, uint32_t msgType)
{
	pe_debug("Msg: %s", lim_msg_str(msgType));
}

/**
 * lim_init_mlm() -  This function is called by limProcessSmeMessages() to
 * initialize MLM state machine on STA
 * @pMac: Pointer to Global MAC structure
 *
 * @Return: Status of operation
 */
QDF_STATUS lim_init_mlm(tpAniSirGlobal pMac)
{
	uint32_t retVal;

	pMac->lim.gLimTimersCreated = 0;

	MTRACE(mac_trace(pMac, TRACE_CODE_MLM_STATE, NO_SESSION,
			  pMac->lim.gLimMlmState));

	/* Initialize number of pre-auth contexts */
	pMac->lim.gLimNumPreAuthContexts = 0;

	/* Initialize MAC based Authentication STA list */
	lim_init_pre_auth_list(pMac);

	/* Create timers used by LIM */
	retVal = lim_create_timers(pMac);
	if (retVal != TX_SUCCESS) {
		pe_err("lim_create_timers Failed");
		return QDF_STATUS_SUCCESS;
	}

	pMac->lim.gLimTimersCreated = 1;
	return QDF_STATUS_SUCCESS;
} /*** end lim_init_mlm() ***/

void lim_deactivate_timers(tpAniSirGlobal mac_ctx)
{
	uint32_t n;
	tLimTimers *lim_timer = &mac_ctx->lim.limTimers;

	lim_deactivate_timers_host_roam(mac_ctx);

	/* Deactivate channel switch timer. */
	tx_timer_deactivate(&lim_timer->gLimChannelSwitchTimer);

	/* Deactivate addts response timer. */
	tx_timer_deactivate(&lim_timer->gLimAddtsRspTimer);

	if (tx_timer_running(&lim_timer->gLimJoinFailureTimer)) {
		pe_err("Join failure timer running call the timeout API");
		/* Cleanup as if join timer expired */
		lim_timer_handler(mac_ctx, SIR_LIM_JOIN_FAIL_TIMEOUT);
	}
	/* Deactivate Join failure timer. */
	tx_timer_deactivate(&lim_timer->gLimJoinFailureTimer);

	/* Deactivate Periodic Join Probe Request timer. */
	tx_timer_deactivate(&lim_timer->gLimPeriodicJoinProbeReqTimer);

	/* Deactivate Auth Retry timer. */
	tx_timer_deactivate
			(&lim_timer->g_lim_periodic_auth_retry_timer);

	if (tx_timer_running(&lim_timer->gLimAssocFailureTimer)) {
		pe_err("Assoc failure timer running call the timeout API");
		/* Cleanup as if assoc timer expired */
		lim_assoc_failure_timer_handler(mac_ctx, LIM_ASSOC);
	}
	/* Deactivate Association failure timer. */
	tx_timer_deactivate(&lim_timer->gLimAssocFailureTimer);

	if (tx_timer_running(&mac_ctx->lim.limTimers.gLimAuthFailureTimer)) {
		pe_err("Auth failure timer running call the timeout API");
		/* Cleanup as if auth timer expired */
		lim_timer_handler(mac_ctx, SIR_LIM_AUTH_FAIL_TIMEOUT);
	}
	/* Deactivate Authentication failure timer. */
	tx_timer_deactivate(&lim_timer->gLimAuthFailureTimer);

	/* Deactivate wait-for-probe-after-Heartbeat timer. */
	tx_timer_deactivate(&lim_timer->gLimProbeAfterHBTimer);

	/* Deactivate and delete Quiet timer. */
	tx_timer_deactivate(&lim_timer->gLimQuietTimer);

	/* Deactivate Quiet BSS timer. */
	tx_timer_deactivate(&lim_timer->gLimQuietBssTimer);

	/* Deactivate cnf wait timer */
	for (n = 0; n < (mac_ctx->lim.maxStation + 1); n++) {
		tx_timer_deactivate(&lim_timer->gpLimCnfWaitTimer[n]);
	}

	/* Deactivate any Authentication response timers */
	lim_delete_pre_auth_list(mac_ctx);

	tx_timer_deactivate(&lim_timer->gLimUpdateOlbcCacheTimer);
	tx_timer_deactivate(&lim_timer->gLimPreAuthClnupTimer);

	if (tx_timer_running(&lim_timer->gLimDisassocAckTimer)) {
		pe_err("Disassoc timer running call the timeout API");
		lim_timer_handler(mac_ctx, SIR_LIM_DISASSOC_ACK_TIMEOUT);
	}
	tx_timer_deactivate(&lim_timer->gLimDisassocAckTimer);

	if (tx_timer_running(&lim_timer->gLimDeauthAckTimer)) {
		pe_err("Deauth timer running call the timeout API");
		lim_timer_handler(mac_ctx, SIR_LIM_DEAUTH_ACK_TIMEOUT);
	}
	tx_timer_deactivate(&lim_timer->gLimDeauthAckTimer);

	tx_timer_deactivate(&lim_timer->sae_auth_timer);
}


/**
 * lim_cleanup_mlm() - This function is called to cleanup
 * @mac_ctx: Pointer to Global MAC structure
 *
 * Function is called to cleanup any resources allocated by the  MLM
 * state machine.
 *
 * Return: none
 */
void lim_cleanup_mlm(tpAniSirGlobal mac_ctx)
{
	uint32_t n;

	tLimPreAuthNode **pAuthNode;
	tLimTimers *lim_timer = NULL;

	if (mac_ctx->lim.gLimTimersCreated == 1) {
		lim_timer = &mac_ctx->lim.limTimers;

		lim_deactivate_timers(mac_ctx);

		lim_delete_timers_host_roam(mac_ctx);
		/* Delete channel switch timer. */
		tx_timer_delete(&lim_timer->gLimChannelSwitchTimer);

		/* Delete addts response timer. */
		tx_timer_delete(&lim_timer->gLimAddtsRspTimer);

		/* Delete Join failure timer. */
		tx_timer_delete(&lim_timer->gLimJoinFailureTimer);

		/* Delete Periodic Join Probe Request timer. */
		tx_timer_delete(&lim_timer->gLimPeriodicJoinProbeReqTimer);

		/* Delete Auth Retry timer. */
		tx_timer_delete(&lim_timer->g_lim_periodic_auth_retry_timer);

		/* Delete Association failure timer. */
		tx_timer_delete(&lim_timer->gLimAssocFailureTimer);

		/* Delete Authentication failure timer. */
		tx_timer_delete(&lim_timer->gLimAuthFailureTimer);

		/* Delete wait-for-probe-after-Heartbeat timer. */
		tx_timer_delete(&lim_timer->gLimProbeAfterHBTimer);

		/* Delete Quiet timer. */
		tx_timer_delete(&lim_timer->gLimQuietTimer);

		/* Delete Quiet BSS timer. */
		tx_timer_delete(&lim_timer->gLimQuietBssTimer);

		/* Delete cnf wait timer */
		for (n = 0; n < (mac_ctx->lim.maxStation + 1); n++) {
			tx_timer_delete(&lim_timer->gpLimCnfWaitTimer[n]);
		}

		pAuthNode = mac_ctx->lim.gLimPreAuthTimerTable.pTable;

		/* Delete any Auth rsp timers, which might have been started */
		for (n = 0; n < mac_ctx->lim.gLimPreAuthTimerTable.numEntry;
				n++)
			tx_timer_delete(&pAuthNode[n]->timer);

		tx_timer_delete(&lim_timer->gLimUpdateOlbcCacheTimer);
		tx_timer_delete(&lim_timer->gLimPreAuthClnupTimer);

		tx_timer_delete(&lim_timer->gLimDisassocAckTimer);

		tx_timer_delete(&lim_timer->gLimDeauthAckTimer);

		tx_timer_delete(&lim_timer->sae_auth_timer);

		mac_ctx->lim.gLimTimersCreated = 0;
	}
} /*** end lim_cleanup_mlm() ***/

/**
 * lim_is_addr_bc()
 *
 ***FUNCTION:
 * This function is called in various places within LIM code
 * to determine whether passed MAC address is a broadcast or not
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param macAddr  Indicates MAC address that need to be determined
 *                 whether it is Broadcast address or not
 *
 * @return true if passed address is Broadcast address else false
 */

uint8_t lim_is_addr_bc(tSirMacAddr macAddr)
{
	int i;

	for (i = 0; i < 6; i++) {
		if ((macAddr[i] & 0xFF) != 0xFF)
			return false;
	}

	return true;
} /****** end lim_is_addr_bc() ******/

/**
 * lim_is_group_addr()
 *
 ***FUNCTION:
 * This function is called in various places within LIM code
 * to determine whether passed MAC address is a group address or not
 *
 ***LOGIC:
 * If least significant bit of first octet of the MAC address is
 * set to 1, it is a Group address.
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param macAddr  Indicates MAC address that need to be determined
 *                 whether it is Group address or not
 *
 * @return true if passed address is Group address else false
 */

uint8_t lim_is_group_addr(tSirMacAddr macAddr)
{
	if ((macAddr[0] & 0x01) == 0x01)
		return true;
	else
		return false;
} /****** end lim_is_group_addr() ******/

/**
 * lim_print_mac_addr()
 *
 ***FUNCTION:
 * This function is called to print passed MAC address
 * in : format.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * @param  macAddr  - MacAddr to be printed
 * @param  logLevel - Loglevel to be used
 *
 * @return None.
 */

void lim_print_mac_addr(tpAniSirGlobal pMac, tSirMacAddr macAddr, uint8_t logLevel)
{
	pe_debug(MAC_ADDRESS_STR, MAC_ADDR_ARRAY(macAddr));
} /****** end lim_print_mac_addr() ******/

/*
 * lim_reset_deferred_msg_q()
 *
 ***FUNCTION:
 * This function resets the deferred message queue parameters.
 *
 ***PARAMS:
 * @param pMac     - Pointer to Global MAC structure
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 ***RETURNS:
 * None
 */

void lim_reset_deferred_msg_q(tpAniSirGlobal pMac)
{
	struct scheduler_msg *read_msg = {0};

	if (pMac->lim.gLimDeferredMsgQ.size > 0) {
		while ((read_msg = lim_read_deferred_msg_q(pMac)) != NULL) {
			pe_free_msg(pMac, read_msg);
		}
	}

	pMac->lim.gLimDeferredMsgQ.size =
		pMac->lim.gLimDeferredMsgQ.write =
			pMac->lim.gLimDeferredMsgQ.read = 0;

}

#define LIM_DEFERRED_Q_CHECK_THRESHOLD  (MAX_DEFERRED_QUEUE_LEN/2)
#define LIM_MAX_NUM_MGMT_FRAME_DEFERRED (MAX_DEFERRED_QUEUE_LEN/2)

/**
 * lim_write_deferred_msg_q() - This function queues up a deferred message
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @lim_msg: a LIM message
 *
 * Function queues up a deferred message for later processing on the
 * STA side.
 *
 * Return: none
 */

uint8_t lim_write_deferred_msg_q(tpAniSirGlobal mac_ctx,
				 struct scheduler_msg *lim_msg)
{
	uint8_t type, subtype;

	pe_debug("Queue a deferred message size: %d write: %d - type: 0x%x",
		mac_ctx->lim.gLimDeferredMsgQ.size,
		mac_ctx->lim.gLimDeferredMsgQ.write,
		lim_msg->type);

	/* check if the deferred message queue is full */
	if (mac_ctx->lim.gLimDeferredMsgQ.size >= MAX_DEFERRED_QUEUE_LEN) {
		if (!(mac_ctx->lim.deferredMsgCnt & 0xF)) {
			pe_err("queue->MsgQ full Msg: %d Msgs Failed: %d",
				lim_msg->type,
				++mac_ctx->lim.deferredMsgCnt);
			cds_flush_logs(WLAN_LOG_TYPE_NON_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_QUEUE_FULL,
				false, false);
		} else {
			mac_ctx->lim.deferredMsgCnt++;
		}
		return TX_QUEUE_FULL;
	}

	/*
	 * In the application, there should not be more than 1 message get
	 * queued up. If happens, flags a warning. In the future, this can
	 * happen.
	 */
	if (mac_ctx->lim.gLimDeferredMsgQ.size > 0)
		pe_debug("%d Deferred Msg type: 0x%x scan: %d global sme: %d global mlme: %d addts: %d",
			mac_ctx->lim.gLimDeferredMsgQ.size,
			lim_msg->type,
			lim_is_system_in_scan_state(mac_ctx),
			mac_ctx->lim.gLimSmeState,
			mac_ctx->lim.gLimMlmState,
			mac_ctx->lim.gLimAddtsSent);

	if (SIR_BB_XPORT_MGMT_MSG == lim_msg->type) {
		lim_util_get_type_subtype(lim_msg->bodyptr,
					&type, &subtype);
		pe_debug(" Deferred management type %d subtype %d ",
			type, subtype);
	}

	/*
	 * To prevent the deferred Q is full of management frames, only give
	 * them certain space
	 */
	if ((SIR_BB_XPORT_MGMT_MSG == lim_msg->type) &&
		(LIM_DEFERRED_Q_CHECK_THRESHOLD <
			mac_ctx->lim.gLimDeferredMsgQ.size)) {
		uint16_t idx, count = 0;

		for (idx = 0; idx < mac_ctx->lim.gLimDeferredMsgQ.size;
								idx++) {
			if (SIR_BB_XPORT_MGMT_MSG ==
					mac_ctx->lim.gLimDeferredMsgQ.
						deferredQueue[idx].type) {
				count++;
			}
		}
		if (LIM_MAX_NUM_MGMT_FRAME_DEFERRED < count) {
			/*
			 * We reach the quota for management frames,
			 * drop this one
			 */
			pe_warn("Too many queue->MsgQ Msg: %d count: %d",
				lim_msg->type, count);
			/* Return error, caller knows what to do */
			return TX_QUEUE_FULL;
		}
	}

	++mac_ctx->lim.gLimDeferredMsgQ.size;

	/* reset the count here since we are able to defer the message */
	if (mac_ctx->lim.deferredMsgCnt != 0)
		mac_ctx->lim.deferredMsgCnt = 0;

	/* if the write pointer hits the end of the queue, rewind it */
	if (mac_ctx->lim.gLimDeferredMsgQ.write >= MAX_DEFERRED_QUEUE_LEN)
		mac_ctx->lim.gLimDeferredMsgQ.write = 0;

	/* save the message to the queue and advanced the write pointer */
	qdf_mem_copy((uint8_t *) &mac_ctx->lim.gLimDeferredMsgQ.
			deferredQueue[mac_ctx->lim.gLimDeferredMsgQ.write++],
				(uint8_t *) lim_msg,
				sizeof(struct scheduler_msg));
	return TX_SUCCESS;

}

/*
 * lim_read_deferred_msg_q()
 *
 ***FUNCTION:
 * This function dequeues a deferred message for processing on the
 * STA side.
 *
 ***PARAMS:
 * @param pMac     - Pointer to Global MAC structure
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 *
 ***RETURNS:
 * Returns the message at the head of the deferred message queue
 */

struct scheduler_msg *lim_read_deferred_msg_q(tpAniSirGlobal pMac)
{
	struct scheduler_msg *msg = {0};

	/*
	** check any messages left. If no, return
	**/
	if (pMac->lim.gLimDeferredMsgQ.size <= 0)
		return NULL;

	/*
	** decrement the queue size
	**/
	pMac->lim.gLimDeferredMsgQ.size--;

	/*
	** retrieve the message from the head of the queue
	**/
	msg =
		&pMac->lim.gLimDeferredMsgQ.deferredQueue[pMac->lim.
							  gLimDeferredMsgQ.read];

	/*
	** advance the read pointer
	**/
	pMac->lim.gLimDeferredMsgQ.read++;

	/*
	** if the read pointer hits the end of the queue, rewind it
	**/
	if (pMac->lim.gLimDeferredMsgQ.read >= MAX_DEFERRED_QUEUE_LEN)
		pMac->lim.gLimDeferredMsgQ.read = 0;

	pe_debug("DeQueue a deferred message size: %d read: %d - type: 0x%x",
			pMac->lim.gLimDeferredMsgQ.size,
			pMac->lim.gLimDeferredMsgQ.read, msg->type);

	pe_debug("DQ msg -- scan: %d global sme: %d global mlme: %d addts: %d",
		lim_is_system_in_scan_state(pMac), pMac->lim.gLimSmeState,
		pMac->lim.gLimMlmState, pMac->lim.gLimAddtsSent);

	return msg;
}

QDF_STATUS
lim_sys_process_mmh_msg_api(tpAniSirGlobal pMac,
			    struct scheduler_msg *pMsg, uint8_t qType)
{
	/* FIXME */
	sys_process_mmh_msg(pMac, pMsg);
	return QDF_STATUS_SUCCESS;
}

/*
 * lim_handle_update_olbc_cache() - This function update olbc cache
 *
 * @mac_ctx: Pointer to Global MAC structure
 *
 * Function updates olbc cache
 *
 * Return: none
 */
void lim_handle_update_olbc_cache(tpAniSirGlobal mac_ctx)
{
	int i;
	static int enable;
	tUpdateBeaconParams beaconParams;

	tpPESession psessionEntry = lim_is_ap_session_active(mac_ctx);

	if (psessionEntry == NULL) {
		pe_debug(" Session not found");
		return;
	}

	if (psessionEntry->is_session_obss_offload_enabled) {
		pe_debug("protection offloaded");
		return;
	}
	qdf_mem_zero((uint8_t *) &beaconParams, sizeof(tUpdateBeaconParams));
	beaconParams.bssIdx = psessionEntry->bssIdx;

	beaconParams.paramChangeBitmap = 0;
	/*
	 * This is doing a 2 pass check. The first pass is to invalidate
	 * all the cache entries. The second pass is to decide whether to
	 * disable protection.
	 */
	if (!enable) {
		pe_debug("Resetting OLBC cache");
		psessionEntry->gLimOlbcParams.numSta = 0;
		psessionEntry->gLimOverlap11gParams.numSta = 0;
		psessionEntry->gLimOverlapHt20Params.numSta = 0;
		psessionEntry->gLimNonGfParams.numSta = 0;
		psessionEntry->gLimLsigTxopParams.numSta = 0;

		for (i = 0; i < LIM_PROT_STA_OVERLAP_CACHE_SIZE; i++)
			mac_ctx->lim.protStaOverlapCache[i].active = false;

		enable = 1;
	} else {
		if ((!psessionEntry->gLimOlbcParams.numSta) &&
			(psessionEntry->gLimOlbcParams.protectionEnabled) &&
			(!psessionEntry->gLim11bParams.protectionEnabled)) {
			pe_debug("Overlap cache clear and no 11B STA set");
			lim_enable11g_protection(mac_ctx, false, true,
						&beaconParams,
						psessionEntry);
		}

		if ((!psessionEntry->gLimOverlap11gParams.numSta) &&
			(psessionEntry->gLimOverlap11gParams.protectionEnabled)
			&& (!psessionEntry->gLim11gParams.protectionEnabled)) {
			pe_debug("Overlap cache clear and no 11G STA set");
			lim_enable_ht_protection_from11g(mac_ctx, false, true,
							&beaconParams,
							psessionEntry);
		}

		if ((!psessionEntry->gLimOverlapHt20Params.numSta) &&
			(psessionEntry->gLimOverlapHt20Params.protectionEnabled)
			&& (!psessionEntry->gLimHt20Params.protectionEnabled)) {
			pe_debug("Overlap cache clear and no HT20 STA set");
			lim_enable11g_protection(mac_ctx, false, true,
						&beaconParams,
						psessionEntry);
		}

		enable = 0;
	}

	if ((false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)
					&& beaconParams.paramChangeBitmap) {
		sch_set_fixed_beacon_fields(mac_ctx, psessionEntry);
		lim_send_beacon_params(mac_ctx, &beaconParams, psessionEntry);
	}
	/* Start OLBC timer */
	if (tx_timer_activate(&mac_ctx->lim.limTimers.gLimUpdateOlbcCacheTimer)
						!= TX_SUCCESS)
		pe_err("tx_timer_activate failed");
}

/**
 * lim_is_null_ssid() - This function checks if ssid supplied is Null SSID
 * @ssid: pointer to tSirMacSSid
 *
 * Function checks if ssid supplied is Null SSID
 *
 * Return: none
 */

uint8_t lim_is_null_ssid(tSirMacSSid *ssid)
{
	uint8_t fnull_ssid = false;
	uint32_t ssid_len;
	uint8_t *ssid_str;

	if (0 == ssid->length) {
		fnull_ssid = true;
		return fnull_ssid;
	}
	/* If the first charactes is space, then check if all
	 * characters in SSID are spaces to consider it as NULL SSID
	 */
	if ((ASCII_SPACE_CHARACTER == ssid->ssId[0]) &&
		(ssid->length == 1)) {
			fnull_ssid = true;
			return fnull_ssid;
	} else {
		/* check if all the charactes in SSID are NULL */
		ssid_len = ssid->length;
		ssid_str = ssid->ssId;

		while (ssid_len) {
			if (*ssid_str)
				return fnull_ssid;

			ssid_str++;
			ssid_len--;
		}

		if (0 == ssid_len) {
			fnull_ssid = true;
			return fnull_ssid;
		}
	}

	return fnull_ssid;
}

/** -------------------------------------------------------------
   \fn lim_update_prot_sta_params
   \brief updates protection related counters.
   \param      tpAniSirGlobal    pMac
   \param      tSirMacAddr peerMacAddr
   \param      tLimProtStaCacheType protStaCacheType
   \param      tHalBitVal gfSupported
   \param      tHalBitVal lsigTxopSupported
   \return      None
   -------------------------------------------------------------*/
static void
lim_update_prot_sta_params(tpAniSirGlobal pMac,
			   tSirMacAddr peerMacAddr,
			   tLimProtStaCacheType protStaCacheType,
			   tHalBitVal gfSupported, tHalBitVal lsigTxopSupported,
			   tpPESession psessionEntry)
{
	uint32_t i;

	pe_debug("Associated STA addr is:");
	lim_print_mac_addr(pMac, peerMacAddr, LOGD);

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (psessionEntry->protStaCache[i].active) {
			pe_debug("Addr:");
				lim_print_mac_addr
				(pMac, psessionEntry->protStaCache[i].addr,
				LOGD);

			if (!qdf_mem_cmp
				    (psessionEntry->protStaCache[i].addr,
				    peerMacAddr, sizeof(tSirMacAddr))) {
				pe_debug("matching cache entry at: %d already active",
					i);
				return;
			}
		}
	}

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (!psessionEntry->protStaCache[i].active)
			break;
	}

	if (i >= LIM_PROT_STA_CACHE_SIZE) {
		pe_err("No space in ProtStaCache");
		return;
	}

	qdf_mem_copy(psessionEntry->protStaCache[i].addr,
		     peerMacAddr, sizeof(tSirMacAddr));

	psessionEntry->protStaCache[i].protStaCacheType = protStaCacheType;
	psessionEntry->protStaCache[i].active = true;
	if (eLIM_PROT_STA_CACHE_TYPE_llB == protStaCacheType) {
		psessionEntry->gLim11bParams.numSta++;
		pe_debug("11B,");
	} else if (eLIM_PROT_STA_CACHE_TYPE_llG == protStaCacheType) {
		psessionEntry->gLim11gParams.numSta++;
		pe_debug("11G,");
	} else if (eLIM_PROT_STA_CACHE_TYPE_HT20 == protStaCacheType) {
		psessionEntry->gLimHt20Params.numSta++;
		pe_debug("HT20,");
	}

	if (!gfSupported) {
		psessionEntry->gLimNonGfParams.numSta++;
		pe_debug("NonGf,");
	}
	if (!lsigTxopSupported) {
		psessionEntry->gLimLsigTxopParams.numSta++;
		pe_debug("!lsigTxopSupported");
	}
} /* --------------------------------------------------------------------- */

/** -------------------------------------------------------------
   \fn lim_decide_ap_protection
   \brief Decides all the protection related staiton coexistence and also sets
 \        short preamble and short slot appropriately. This function will be called
 \        when AP is ready to send assocRsp tp the station joining right now.
   \param      tpAniSirGlobal    pMac
   \param      tSirMacAddr peerMacAddr
   \return      None
   -------------------------------------------------------------*/
void
lim_decide_ap_protection(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr,
			 tpUpdateBeaconParams pBeaconParams,
			 tpPESession psessionEntry)
{
	uint16_t tmpAid;
	tpDphHashNode pStaDs;
	enum band_info rfBand = BAND_UNKNOWN;
	uint32_t phyMode;
	tLimProtStaCacheType protStaCacheType =
		eLIM_PROT_STA_CACHE_TYPE_INVALID;
	tHalBitVal gfSupported = eHAL_SET, lsigTxopSupported = eHAL_SET;

	pBeaconParams->paramChangeBitmap = 0;
	/* check whether to enable protection or not */
	pStaDs =
		dph_lookup_hash_entry(pMac, peerMacAddr, &tmpAid,
				      &psessionEntry->dph.dphHashTable);
	if (NULL == pStaDs) {
		pe_err("pStaDs is NULL");
		return;
	}
	lim_get_rf_band_new(pMac, &rfBand, psessionEntry);
	/* if we are in 5 GHZ band */
	if (BAND_5G == rfBand) {
		/* We are 11N. we need to protect from 11A and Ht20. we don't need any other protection in 5 GHZ. */
		/* HT20 case is common between both the bands and handled down as common code. */
		if (true == psessionEntry->htCapability) {
			/* we are 11N and 11A station is joining. */
			/* protection from 11A required. */
			if (false == pStaDs->mlmStaContext.htCapability) {
				lim_update_11a_protection(pMac, true, false,
							 pBeaconParams,
							 psessionEntry);
				return;
			}
		}
	} else if (BAND_2G == rfBand) {
		lim_get_phy_mode(pMac, &phyMode, psessionEntry);

		/* We are 11G. Check if we need protection from 11b Stations. */
		if ((phyMode == WNI_CFG_PHY_MODE_11G) &&
		    (false == psessionEntry->htCapability)) {

			if (pStaDs->erpEnabled == eHAL_CLEAR) {
				protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_llB;
				/* enable protection */
				pe_debug("Enabling protection from 11B");
				lim_enable11g_protection(pMac, true, false,
							 pBeaconParams,
							 psessionEntry);
			}
		}
		/* HT station. */
		if (true == psessionEntry->htCapability) {
			/* check if we need protection from 11b station */
			if ((pStaDs->erpEnabled == eHAL_CLEAR) &&
			    (!pStaDs->mlmStaContext.htCapability)) {
				protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_llB;
				/* enable protection */
				pe_debug("Enabling protection from 11B");
				lim_enable11g_protection(pMac, true, false,
							 pBeaconParams,
							 psessionEntry);
			}
			/* station being joined is non-11b and non-ht ==> 11g device */
			else if (!pStaDs->mlmStaContext.htCapability) {
				protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_llG;
				/* enable protection */
				lim_enable_ht_protection_from11g(pMac, true, false,
								 pBeaconParams,
								 psessionEntry);
			}
			/* ERP mode is enabled for the latest station joined */
			/* latest station joined is HT capable */
			/* This case is being handled in common code (commn between both the bands) below. */
		}
	}
	/* we are HT and HT station is joining. This code is common for both the bands. */
	if ((true == psessionEntry->htCapability) &&
	    (true == pStaDs->mlmStaContext.htCapability)) {
		if (!pStaDs->htGreenfield) {
			lim_enable_ht_non_gf_protection(pMac, true, false,
							pBeaconParams,
							psessionEntry);
			gfSupported = eHAL_CLEAR;
		}
		/* Station joining is HT 20Mhz */
		if ((eHT_CHANNEL_WIDTH_20MHZ ==
		pStaDs->htSupportedChannelWidthSet) &&
		(eHT_CHANNEL_WIDTH_20MHZ !=
		 psessionEntry->htSupportedChannelWidthSet)){
			protStaCacheType = eLIM_PROT_STA_CACHE_TYPE_HT20;
			lim_enable_ht20_protection(pMac, true, false,
						   pBeaconParams, psessionEntry);
		}
		/* Station joining does not support LSIG TXOP Protection */
		if (!pStaDs->htLsigTXOPProtection) {
			lim_enable_ht_lsig_txop_protection(pMac, false, false,
							   pBeaconParams,
							   psessionEntry);
			lsigTxopSupported = eHAL_CLEAR;
		}
	}

	lim_update_prot_sta_params(pMac, peerMacAddr, protStaCacheType,
				   gfSupported, lsigTxopSupported, psessionEntry);

	return;
}

/** -------------------------------------------------------------
   \fn lim_enable_overlap11g_protection
   \brief wrapper function for setting overlap 11g protection.
   \param      tpAniSirGlobal    pMac
   \param      tpUpdateBeaconParams pBeaconParams
   \param      tpSirMacMgmtHdr         pMh
   \return      None
   -------------------------------------------------------------*/
void
lim_enable_overlap11g_protection(tpAniSirGlobal pMac,
				 tpUpdateBeaconParams pBeaconParams,
				 tpSirMacMgmtHdr pMh, tpPESession psessionEntry)
{
	lim_update_overlap_sta_param(pMac, pMh->bssId,
				     &(psessionEntry->gLimOlbcParams));

	if (psessionEntry->gLimOlbcParams.numSta &&
	    !psessionEntry->gLimOlbcParams.protectionEnabled) {
		/* enable protection */
		pe_debug("OLBC happens!!!");
		lim_enable11g_protection(pMac, true, true, pBeaconParams,
					 psessionEntry);
	}
}

/**
 * lim_update_short_preamble() - This function Updates short preamble
 * @mac_ctx: pointer to Global MAC structure
 * @peer_mac_addr: pointer to tSirMacAddr
 * @pbeaconparams: pointer to tpUpdateBeaconParams
 * @psession_entry: pointer to tpPESession
 *
 * Function Updates short preamble if needed when a new station joins
 *
 * Return: none
 */
void
lim_update_short_preamble(tpAniSirGlobal mac_ctx, tSirMacAddr peer_mac_addr,
				tpUpdateBeaconParams beaconparams,
				tpPESession psession_entry)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	uint32_t phy_mode;
	uint16_t i;

	/* check whether to enable protection or not */
	sta_ds =
		dph_lookup_hash_entry(mac_ctx, peer_mac_addr, &aid,
				      &psession_entry->dph.dphHashTable);

	lim_get_phy_mode(mac_ctx, &phy_mode, psession_entry);

	if (sta_ds == NULL || phy_mode != WNI_CFG_PHY_MODE_11G)
		return;

	if (sta_ds->shortPreambleEnabled != eHAL_CLEAR)
		return;

	pe_debug("Short Preamble is not enabled in Assoc Req from");

	lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGD);

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(psession_entry) &&
			(psession_entry->gLimNoShortParams.
				staNoShortCache[i].active) &&
			(!qdf_mem_cmp
				(psession_entry->gLimNoShortParams.
				staNoShortCache[i].addr,
				peer_mac_addr, sizeof(tSirMacAddr))))
			return;
		else if (!LIM_IS_AP_ROLE(psession_entry) &&
				(mac_ctx->lim.gLimNoShortParams.
					staNoShortCache[i].active) &&
			(!qdf_mem_cmp(mac_ctx->lim.gLimNoShortParams.
				staNoShortCache[i].addr,
				peer_mac_addr,
				sizeof(tSirMacAddr))))
			return;
	}

	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(psession_entry) &&
			!psession_entry->gLimNoShortParams.
				staNoShortCache[i].active)
			break;
		else if (!mac_ctx->lim.gLimNoShortParams.
				staNoShortCache[i].active)
			break;
	}

	if (i >= LIM_PROT_STA_CACHE_SIZE) {
#ifdef WLAN_DEBUG
		tLimNoShortParams *lim_params =
				&psession_entry->gLimNoShortParams;
#endif
		if (LIM_IS_AP_ROLE(psession_entry)) {
			pe_err("No space in Short cache active: %d sta: %d for sta",
				i, lim_params->numNonShortPreambleSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		} else {
			pe_err("No space in Short cache active: %d sta: %d for sta",
				i, lim_params->numNonShortPreambleSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		}

	}

	if (LIM_IS_AP_ROLE(psession_entry)) {
		qdf_mem_copy(psession_entry->gLimNoShortParams.
				staNoShortCache[i].addr,
				peer_mac_addr, sizeof(tSirMacAddr));
		psession_entry->gLimNoShortParams.staNoShortCache[i].
							active = true;
		psession_entry->gLimNoShortParams.numNonShortPreambleSta++;
	} else {
		qdf_mem_copy(mac_ctx->lim.gLimNoShortParams.
					staNoShortCache[i].addr,
				peer_mac_addr, sizeof(tSirMacAddr));
		mac_ctx->lim.gLimNoShortParams.staNoShortCache[i].active = true;
		mac_ctx->lim.gLimNoShortParams.numNonShortPreambleSta++;
	}

	/* enable long preamble */
	pe_debug("Disabling short preamble");

	if (lim_enable_short_preamble(mac_ctx, false, beaconparams,
					psession_entry) != QDF_STATUS_SUCCESS)
		pe_err("Cannot enable long preamble");
}

/**
 * lim_update_short_slot_time() - This function Updates short slot time
 * @mac_ctx: pointer to Global MAC structure
 * @peer_mac_addr: pointer to tSirMacAddr
 * @beacon_params: pointer to tpUpdateBeaconParams
 * @psession_entry: pointer to tpPESession
 *
 * Function Updates short slot time if needed when a new station joins
 *
 * Return: None
 */
void
lim_update_short_slot_time(tpAniSirGlobal mac_ctx, tSirMacAddr peer_mac_addr,
			   tpUpdateBeaconParams beacon_params,
			   tpPESession session_entry)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	uint32_t phy_mode;
	uint32_t val;
	uint16_t i;

	/* check whether to enable protection or not */
	sta_ds = dph_lookup_hash_entry(mac_ctx, peer_mac_addr, &aid,
				       &session_entry->dph.dphHashTable);
	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	if (sta_ds == NULL || phy_mode != WNI_CFG_PHY_MODE_11G)
		return;

	/*
	 * Only in case of softap in 11g mode, slot time might change
	 * depending on the STA being added. In 11a case, it should
	 * be always 1 and in 11b case, it should be always 0.
	 * Only when the new STA has short slot time disabled, we need to
	 * change softap's overall slot time settings else the default for
	 * softap is always short slot enabled. When the last long slot STA
	 * leaves softAP, we take care of it in lim_decide_short_slot
	 */
	if (sta_ds->shortSlotTimeEnabled != eHAL_CLEAR)
		return;

	pe_debug("Short Slot Time is not enabled in Assoc Req from");
	lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGD);
	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(session_entry) &&
		    session_entry->gLimNoShortSlotParams.
		    staNoShortSlotCache[i].active) {
			if (!qdf_mem_cmp(session_entry->
			    gLimNoShortSlotParams.staNoShortSlotCache[i].addr,
			    peer_mac_addr, sizeof(tSirMacAddr)))
				return;
		} else if (!LIM_IS_AP_ROLE(session_entry)) {
			if (mac_ctx->lim.gLimNoShortSlotParams.
			    staNoShortSlotCache[i].active) {
				if (!qdf_mem_cmp(mac_ctx->
				    lim.gLimNoShortSlotParams.
				    staNoShortSlotCache[i].addr,
				    peer_mac_addr, sizeof(tSirMacAddr)))
						return;
			}
		}
	}
	for (i = 0; i < LIM_PROT_STA_CACHE_SIZE; i++) {
		if (LIM_IS_AP_ROLE(session_entry) &&
		    !session_entry->gLimNoShortSlotParams.
		    staNoShortSlotCache[i].active)
			break;
		else
			if (!mac_ctx->lim.gLimNoShortSlotParams.
			    staNoShortSlotCache[i].active)
				break;
	}

	if (i >= LIM_PROT_STA_CACHE_SIZE) {
		if (LIM_IS_AP_ROLE(session_entry)) {
			pe_err("No space in ShortSlot cache active: %d sta: %d for sta",
				i, session_entry->gLimNoShortSlotParams.
				numNonShortSlotSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		} else {
			pe_err("No space in ShortSlot cache active: %d sta: %d for sta",
				i, mac_ctx->lim.gLimNoShortSlotParams.
				numNonShortSlotSta);
			lim_print_mac_addr(mac_ctx, peer_mac_addr, LOGE);
			return;
		}
	}

	if (LIM_IS_AP_ROLE(session_entry)) {
		qdf_mem_copy(session_entry->gLimNoShortSlotParams.
			staNoShortSlotCache[i].addr,
			peer_mac_addr, sizeof(tSirMacAddr));
		session_entry->gLimNoShortSlotParams.
			staNoShortSlotCache[i].active = true;
		session_entry->gLimNoShortSlotParams.numNonShortSlotSta++;
	} else {
		qdf_mem_copy(mac_ctx->lim.gLimNoShortSlotParams.
			staNoShortSlotCache[i].addr,
			peer_mac_addr, sizeof(tSirMacAddr));
		mac_ctx->lim.gLimNoShortSlotParams.
			staNoShortSlotCache[i].active = true;
		mac_ctx->lim.gLimNoShortSlotParams.
			numNonShortSlotSta++;
	}
	wlan_cfg_get_int(mac_ctx, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED, &val);
	/*
	 * Here we check if we are AP role and short slot enabled
	 * (both admin and oper modes) but we have atleast one STA
	 * connected with only long slot enabled, we need to change
	 * our beacon/pb rsp to broadcast short slot disabled
	 */
	if ((LIM_IS_AP_ROLE(session_entry)) && (val &&
	    session_entry->gLimNoShortSlotParams.numNonShortSlotSta
	    && session_entry->shortSlotTimeSupported)) {
		/* enable long slot time */
		beacon_params->fShortSlotTime = false;
		beacon_params->paramChangeBitmap |=
				PARAM_SHORT_SLOT_TIME_CHANGED;
		pe_debug("Disable short slot time. Enable long slot time");
		session_entry->shortSlotTimeSupported = false;
	} else if (!LIM_IS_AP_ROLE(session_entry) &&
		   (val && mac_ctx->lim.gLimNoShortSlotParams.
		    numNonShortSlotSta &&
		    session_entry->shortSlotTimeSupported)) {
		/* enable long slot time */
		beacon_params->fShortSlotTime = false;
		beacon_params->paramChangeBitmap |=
			PARAM_SHORT_SLOT_TIME_CHANGED;
		pe_debug("Disable short slot time. Enable long slot time");
		session_entry->shortSlotTimeSupported = false;
	}
}

/** -------------------------------------------------------------
   \fn lim_decide_sta_protection_on_assoc
   \brief Decide protection related settings on Sta while association.
   \param      tpAniSirGlobal    pMac
   \param      tpSchBeaconStruct pBeaconStruct
   \return      None
   -------------------------------------------------------------*/
void
lim_decide_sta_protection_on_assoc(tpAniSirGlobal pMac,
				   tpSchBeaconStruct pBeaconStruct,
				   tpPESession psessionEntry)
{
	enum band_info rfBand = BAND_UNKNOWN;
	uint32_t phyMode = WNI_CFG_PHY_MODE_NONE;

	lim_get_rf_band_new(pMac, &rfBand, psessionEntry);
	lim_get_phy_mode(pMac, &phyMode, psessionEntry);

	if (BAND_5G == rfBand) {
		if ((eSIR_HT_OP_MODE_MIXED == pBeaconStruct->HTInfo.opMode) ||
		    (eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
		     pBeaconStruct->HTInfo.opMode)) {
			if (pMac->lim.cfgProtection.fromlla)
				psessionEntry->beaconParams.llaCoexist = true;
		} else if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
			   pBeaconStruct->HTInfo.opMode) {
			if (pMac->lim.cfgProtection.ht20)
				psessionEntry->beaconParams.ht20Coexist = true;
		}

	} else if (BAND_2G == rfBand) {
		/* spec 7.3.2.13 */
		/* UseProtection will be set when nonERP STA is associated. */
		/* NonERPPresent bit will be set when: */
		/* --nonERP Sta is associated OR */
		/* --nonERP Sta exists in overlapping BSS */
		/* when useProtection is not set then protection from nonERP stations is optional. */

		/* CFG protection from 11b is enabled and */
		/* 11B device in the BSS */
		/* TODO, This is not sessionized */
		if (phyMode != WNI_CFG_PHY_MODE_11B) {
			if (pMac->lim.cfgProtection.fromllb &&
			    pBeaconStruct->erpPresent &&
			    (pBeaconStruct->erpIEInfo.useProtection ||
			     pBeaconStruct->erpIEInfo.nonErpPresent)) {
				psessionEntry->beaconParams.llbCoexist = true;
			}
			/* AP has no 11b station associated. */
			else {
				psessionEntry->beaconParams.llbCoexist = false;
			}
		}
		/* following code block is only for HT station. */
		if ((psessionEntry->htCapability) &&
		    (pBeaconStruct->HTInfo.present)) {
			tDot11fIEHTInfo htInfo = pBeaconStruct->HTInfo;

			/* Obss Non HT STA present mode */
			psessionEntry->beaconParams.gHTObssMode =
				(uint8_t) htInfo.obssNonHTStaPresent;

			/* CFG protection from 11G is enabled and */
			/* our AP has at least one 11G station associated. */
			if (pMac->lim.cfgProtection.fromllg &&
			    ((eSIR_HT_OP_MODE_MIXED == htInfo.opMode) ||
			     (eSIR_HT_OP_MODE_OVERLAP_LEGACY == htInfo.opMode))
			    && (!psessionEntry->beaconParams.llbCoexist)) {
				if (pMac->lim.cfgProtection.fromllg)
					psessionEntry->beaconParams.llgCoexist =
						true;
			}
			/* AP has only HT stations associated and at least one station is HT 20 */
			/* disable protection from any non-HT devices. */
			/* decision for disabling protection from 11b has already been taken above. */
			if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT == htInfo.opMode) {
				/* Disable protection from 11G station. */
				psessionEntry->beaconParams.llgCoexist = false;
				/* CFG protection from HT 20 is enabled. */
				if (pMac->lim.cfgProtection.ht20)
					psessionEntry->beaconParams.
					ht20Coexist = true;
			}
			/* Disable protection from non-HT and HT20 devices. */
			/* decision for disabling protection from 11b has already been taken above. */
			if (eSIR_HT_OP_MODE_PURE == htInfo.opMode) {
				psessionEntry->beaconParams.llgCoexist = false;
				psessionEntry->beaconParams.ht20Coexist = false;
			}

		}
	}
	/* protection related factors other than HT operating mode. Applies to 2.4 GHZ as well as 5 GHZ. */
	if ((psessionEntry->htCapability) && (pBeaconStruct->HTInfo.present)) {
		tDot11fIEHTInfo htInfo = pBeaconStruct->HTInfo;

		psessionEntry->beaconParams.fRIFSMode =
			(uint8_t) htInfo.rifsMode;
		psessionEntry->beaconParams.llnNonGFCoexist =
			(uint8_t) htInfo.nonGFDevicesPresent;
		psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport =
			(uint8_t) htInfo.lsigTXOPProtectionFullSupport;
	}
}


/**
 * lim_decide_sta_11bg_protection() - decides protection related settings on sta
 * @mac_ctx: pointer to global mac structure
 * @beacon_struct: pointer to tpschbeaconstruct
 * @beaconparams: pointer to tpupdatebeaconparams
 * @psession_entry: pointer to tppesession
 * @phy_mode: phy mode index
 *
 * decides 11bg protection related settings on sta while processing beacon
 *
 * Return: none
 */
static void
lim_decide_sta_11bg_protection(tpAniSirGlobal mac_ctx,
			tpSchBeaconStruct beacon_struct,
			tpUpdateBeaconParams beaconparams,
			tpPESession psession_entry,
			uint32_t phy_mode)
{

	tDot11fIEHTInfo htInfo;

	/*
	 * spec 7.3.2.13
	 * UseProtection will be set when nonERP STA is associated.
	 * NonERPPresent bit will be set when:
	 * --nonERP Sta is associated OR
	 * --nonERP Sta exists in overlapping BSS
	 * when useProtection is not set then protection from
	 * nonERP stations is optional.
	 */
	if (phy_mode != WNI_CFG_PHY_MODE_11B) {
		if (beacon_struct->erpPresent &&
			(beacon_struct->erpIEInfo.useProtection ||
			beacon_struct->erpIEInfo.nonErpPresent)) {
			lim_enable11g_protection(mac_ctx, true, false,
						beaconparams,
						psession_entry);
		}
		/* AP has no 11b station associated. */
		else {
			/* disable protection from 11b station */
			lim_enable11g_protection(mac_ctx, false, false,
						beaconparams,
						psession_entry);
		}
	}

	if (!(psession_entry->htCapability) ||
		!(beacon_struct->HTInfo.present))
		return;

	/* following code is only for HT station. */

	htInfo = beacon_struct->HTInfo;
	/* AP has at least one 11G station associated. */
	if (((eSIR_HT_OP_MODE_MIXED == htInfo.opMode) ||
		(eSIR_HT_OP_MODE_OVERLAP_LEGACY == htInfo.opMode)) &&
		(!psession_entry->beaconParams.llbCoexist)) {
		lim_enable_ht_protection_from11g(mac_ctx, true, false,
						beaconparams, psession_entry);

	}
	/*
	 * no HT operating mode change  ==> no change in
	 * protection settings except for MIXED_MODE/Legacy
	 * Mode.
	 */
	/*
	 * in Mixed mode/legacy Mode even if there is no
	 * change in HT operating mode, there might be
	 * change in 11bCoexist or 11gCoexist. Hence this
	 * check is being done after mixed/legacy mode
	 * check.
	 */
	if (mac_ctx->lim.gHTOperMode !=
		(tSirMacHTOperatingMode)htInfo.opMode) {
		mac_ctx->lim.gHTOperMode =
			(tSirMacHTOperatingMode) htInfo.opMode;
		/*
		 * AP has only HT stations associated and
		 * at least one station is HT 20
		 */

		/* disable protection from any non-HT devices. */

		/*
		 * decision for disabling protection from
		 * 11b has already been taken above.
		 */
		if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
				htInfo.opMode) {
			/* Disable protection from 11G station. */
			lim_enable_ht_protection_from11g(mac_ctx, false,
						false, beaconparams,
						psession_entry);

			lim_enable_ht20_protection(mac_ctx, true, false,
						beaconparams,
						psession_entry);
		}
		/*
		 * Disable protection from non-HT and
		 * HT20 devices.
		 */
		/*
		 * decision for disabling protection from
		 * 11b has already been taken above.
		 */
		else if (eSIR_HT_OP_MODE_PURE == htInfo.opMode) {
			lim_enable_ht_protection_from11g(mac_ctx, false,
						false, beaconparams,
						psession_entry);

			lim_enable_ht20_protection(mac_ctx, false,
						false, beaconparams,
						psession_entry);

		}
	}

}

/**
 * lim_decide_sta_protection() -  decides protection related settings on sta
 * @mac_ctx: pointer to global mac structure
 * @beacon_struct: pointer to tpschbeaconstruct
 * @beaconparams: pointer to tpupdatebeaconparams
 * @psession_entry: pointer to tppesession
 *
 * decides protection related settings on sta while processing beacon
 *
 * Return: none
 */
void
lim_decide_sta_protection(tpAniSirGlobal mac_ctx,
				tpSchBeaconStruct beacon_struct,
				tpUpdateBeaconParams beaconparams,
				tpPESession psession_entry)
{

	enum band_info rfband = BAND_UNKNOWN;
	uint32_t phy_mode = WNI_CFG_PHY_MODE_NONE;

	lim_get_rf_band_new(mac_ctx, &rfband, psession_entry);
	lim_get_phy_mode(mac_ctx, &phy_mode, psession_entry);

	if ((BAND_5G == rfband) &&
		/* we are HT capable. */
		(true == psession_entry->htCapability) &&
		(beacon_struct->HTInfo.present)) {
		/*
		 * we are HT capable, AP's HT OPMode is
		 * mixed / overlap legacy ==> need protection
		 * from 11A.
		 */
		if ((eSIR_HT_OP_MODE_MIXED ==
				beacon_struct->HTInfo.opMode) ||
			(eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
				beacon_struct->HTInfo.opMode)) {
			lim_update_11a_protection(mac_ctx, true, false,
						beaconparams, psession_entry);
		}
		/*
		 * we are HT capable, AP's HT OPMode is
		 * HT20 ==> disable protection from 11A if
		 * enabled.
		 */
		/* protection from HT20 if needed. */
		else if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
					beacon_struct->HTInfo.opMode) {
			lim_update_11a_protection(mac_ctx, false, false,
						beaconparams, psession_entry);
			lim_enable_ht20_protection(mac_ctx, true, false,
						beaconparams, psession_entry);
		} else if (eSIR_HT_OP_MODE_PURE ==
				beacon_struct->HTInfo.opMode) {
			lim_update_11a_protection(mac_ctx, false, false,
						beaconparams, psession_entry);
			lim_enable_ht20_protection(mac_ctx, false,
						false, beaconparams,
						psession_entry);
		}
	} else if (BAND_2G == rfband) {
		lim_decide_sta_11bg_protection(mac_ctx, beacon_struct,
					beaconparams, psession_entry, phy_mode);
	}
	/*
	 * following code block is only for HT station.
	 * (2.4 GHZ as well as 5 GHZ)
	 */
	if ((psession_entry->htCapability) && (beacon_struct->HTInfo.present)) {
		tDot11fIEHTInfo htInfo = beacon_struct->HTInfo;
		/*
		 * Check for changes in protection related factors other
		 * than HT operating mode.
		 */
		/*
		 * Check for changes in RIFS mode, nonGFDevicesPresent,
		 * lsigTXOPProtectionFullSupport.
		 */
		if (psession_entry->beaconParams.fRIFSMode !=
				(uint8_t) htInfo.rifsMode) {
			beaconparams->fRIFSMode =
				psession_entry->beaconParams.fRIFSMode =
						(uint8_t) htInfo.rifsMode;
			beaconparams->paramChangeBitmap |=
						PARAM_RIFS_MODE_CHANGED;
		}

		if (psession_entry->beaconParams.llnNonGFCoexist !=
					htInfo.nonGFDevicesPresent) {
			beaconparams->llnNonGFCoexist =
				psession_entry->beaconParams.llnNonGFCoexist =
					(uint8_t) htInfo.nonGFDevicesPresent;
			beaconparams->paramChangeBitmap |=
					PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		}

		if (psession_entry->beaconParams.
			fLsigTXOPProtectionFullSupport !=
			(uint8_t) htInfo.lsigTXOPProtectionFullSupport) {
			beaconparams->fLsigTXOPProtectionFullSupport =
				psession_entry->beaconParams.
					fLsigTXOPProtectionFullSupport =
						(uint8_t) htInfo.
						lsigTXOPProtectionFullSupport;
			beaconparams->paramChangeBitmap |=
					PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		}
		/*
		 * For Station just update the global lim variable,
		 * no need to send message to HAL since Station already
		 * taking care of HT OPR Mode=01,
		 * meaning AP is seeing legacy
		 */
		/* stations in overlapping BSS. */
		if (psession_entry->beaconParams.gHTObssMode !=
				(uint8_t) htInfo.obssNonHTStaPresent)
			psession_entry->beaconParams.gHTObssMode =
				(uint8_t) htInfo.obssNonHTStaPresent;

	}
}

/**
 * lim_process_channel_switch_timeout()
 *
 ***FUNCTION:
 * This function is invoked when Channel Switch Timer expires at
 * the STA.  Now, STA must stop traffic, and then change/disable
 * primary or secondary channel.
 *
 *
 ***NOTE:
 * @param  pMac           - Pointer to Global MAC structure
 * @return None
 */
void lim_process_channel_switch_timeout(tpAniSirGlobal pMac)
{
	tpPESession psessionEntry = NULL;
	uint8_t channel; /* This is received and stored from channelSwitch Action frame */

	psessionEntry = pe_find_session_by_session_id(pMac,
			pMac->lim.limTimers.gLimChannelSwitchTimer.sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}

	if (!LIM_IS_STA_ROLE(psessionEntry)) {
		pe_warn("Channel switch can be done only in STA role, Current Role: %d",
			       GET_LIM_SYSTEM_ROLE(psessionEntry));
		return;
	}

	if (psessionEntry->gLimSpecMgmt.dot11hChanSwState !=
	   eLIM_11H_CHANSW_RUNNING) {
		pe_warn("Channel switch timer should not have been running in state: %d",
			psessionEntry->gLimSpecMgmt.dot11hChanSwState);
		return;
	}

	channel = psessionEntry->gLimChannelSwitch.primaryChannel;
	/* Restore Channel Switch parameters to default */
	psessionEntry->gLimChannelSwitch.switchTimeoutValue = 0;

	/* Channel-switch timeout has occurred. reset the state */
	psessionEntry->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_END;

	/* Check if the AP is switching to a channel that we support.
	 * Else, just don't bother to switch. Indicate HDD to look for a
	 * better AP to associate
	 */
	if (!lim_is_channel_valid_for_channel_switch(pMac, channel)) {
		/* We need to restore pre-channelSwitch state on the STA */
		if (lim_restore_pre_channel_switch_state(pMac, psessionEntry) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("Could not restore pre-channelSwitch (11h) state, resetting the system");
			return;
		}

		/*
		 * If the channel-list that AP is asking us to switch is invalid
		 * then we cannot switch the channel. Just disassociate from AP.
		 * We will find a better AP !!!
		 */
		if ((psessionEntry->limMlmState ==
		   eLIM_MLM_LINK_ESTABLISHED_STATE) &&
		   (psessionEntry->limSmeState != eLIM_SME_WT_DISASSOC_STATE) &&
		   (psessionEntry->limSmeState != eLIM_SME_WT_DEAUTH_STATE)) {
			pe_err("Invalid channel! Disconnect");
			lim_tear_down_link_with_ap(pMac,
					   pMac->lim.limTimers.
					   gLimChannelSwitchTimer.sessionId,
					   eSIR_MAC_UNSUPPORTED_CHANNEL_CSA,
					   eLIM_LINK_MONITORING_DISASSOC);
			return;
		}
	}
	switch (psessionEntry->gLimChannelSwitch.state) {
	case eLIM_CHANNEL_SWITCH_PRIMARY_ONLY:
		lim_switch_primary_channel(pMac,
					   psessionEntry->gLimChannelSwitch.
					   primaryChannel, psessionEntry);
		psessionEntry->gLimChannelSwitch.state =
			eLIM_CHANNEL_SWITCH_IDLE;
		break;
	case eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY:
		lim_switch_primary_secondary_channel(pMac, psessionEntry,
			psessionEntry->gLimChannelSwitch.primaryChannel,
			psessionEntry->gLimChannelSwitch.ch_center_freq_seg0,
			psessionEntry->gLimChannelSwitch.ch_center_freq_seg1,
			psessionEntry->gLimChannelSwitch.ch_width);
		psessionEntry->gLimChannelSwitch.state =
			eLIM_CHANNEL_SWITCH_IDLE;
		break;

	case eLIM_CHANNEL_SWITCH_IDLE:
	default:
		pe_err("incorrect state");
		if (lim_restore_pre_channel_switch_state(pMac, psessionEntry) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("Could not restore pre-channelSwitch (11h) state, resetting the system");
		}
		return; /* Please note, this is 'return' and not 'break' */
	}
}

/**
 * lim_update_channel_switch() - This Function updates channel switch
 * @mac_ctx: pointer to Global MAC structure
 * @beacon: pointer to tpSirProbeRespBeacon
 * @psessionentry: pointer to tpPESession
 *
 * This function is invoked whenever Station receives
 * either 802.11h channel switch IE or airgo proprietary
 * channel switch IE.
 *
 * Return: none
 */
void
lim_update_channel_switch(struct sAniSirGlobal *mac_ctx,
			tpSirProbeRespBeacon beacon,
			tpPESession psession_entry)
{
	uint16_t beacon_period;
	tDot11fIEChanSwitchAnn *chnl_switch;
	tLimChannelSwitchInfo *ch_switch_params;
	tDot11fIEWiderBWChanSwitchAnn *widerchnl_switch;

	beacon_period = psession_entry->beaconParams.beaconInterval;

	/* 802.11h standard channel switch IE */
	chnl_switch = &(beacon->channelSwitchIE);
	ch_switch_params = &psession_entry->gLimChannelSwitch;
	ch_switch_params->primaryChannel =
		chnl_switch->newChannel;
	ch_switch_params->switchCount = chnl_switch->switchCount;
	ch_switch_params->switchTimeoutValue =
		SYS_MS_TO_TICKS(beacon_period) * (chnl_switch->switchCount);
	ch_switch_params->switchMode = chnl_switch->switchMode;
	widerchnl_switch = &(beacon->WiderBWChanSwitchAnn);
	if (beacon->WiderBWChanSwitchAnnPresent) {
		psession_entry->gLimWiderBWChannelSwitch.newChanWidth =
				widerchnl_switch->newChanWidth;
		psession_entry->gLimWiderBWChannelSwitch.newCenterChanFreq0 =
				widerchnl_switch->newCenterChanFreq0;
		psession_entry->gLimWiderBWChannelSwitch.newCenterChanFreq1 =
				widerchnl_switch->newCenterChanFreq1;
	}
	/* Only primary channel switch element is present */
	ch_switch_params->state =
			eLIM_CHANNEL_SWITCH_PRIMARY_ONLY;
	ch_switch_params->ch_width = CH_WIDTH_20MHZ;

	/*
	 * Do not bother to look and operate on extended channel switch element
	 * if our own channel-bonding state is not enabled
	 */
	if (psession_entry->htSupportedChannelWidthSet &&
			beacon->sec_chan_offset_present) {
		if (beacon->sec_chan_offset.secondaryChannelOffset ==
					PHY_DOUBLE_CHANNEL_LOW_PRIMARY) {
			ch_switch_params->state =
				    eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
			ch_switch_params->ch_width = CH_WIDTH_40MHZ;
			ch_switch_params->ch_center_freq_seg0 =
				ch_switch_params->primaryChannel + 2;
		} else if (beacon->sec_chan_offset.secondaryChannelOffset ==
				PHY_DOUBLE_CHANNEL_HIGH_PRIMARY) {
			ch_switch_params->state =
				    eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
			ch_switch_params->ch_width = CH_WIDTH_40MHZ;
			ch_switch_params->ch_center_freq_seg0 =
				ch_switch_params->primaryChannel - 2;
		}
		if (psession_entry->vhtCapability &&
			beacon->WiderBWChanSwitchAnnPresent) {
			ch_switch_params->ch_width =
				widerchnl_switch->newChanWidth + 1;
			ch_switch_params->ch_center_freq_seg0 =
				psession_entry->gLimWiderBWChannelSwitch.
						newCenterChanFreq0;
			ch_switch_params->ch_center_freq_seg1 =
				psession_entry->gLimWiderBWChannelSwitch.
						newCenterChanFreq1;
		}
	}
	if (QDF_STATUS_SUCCESS != lim_start_channel_switch(mac_ctx, psession_entry))
		pe_warn("Could not start Channel Switch");

	pe_debug("session: %d primary chl: %d ch_width: %d count: %d (%d ticks)",
		psession_entry->peSessionId,
		psession_entry->gLimChannelSwitch.primaryChannel,
		psession_entry->gLimChannelSwitch.ch_width,
		psession_entry->gLimChannelSwitch.switchCount,
		psession_entry->gLimChannelSwitch.switchTimeoutValue);
	return;
}

/**
 * lim_cancel_dot11h_channel_switch
 *
 ***FUNCTION:
 * This function is called when STA does not send updated channel-swith IE
 * after indicating channel-switch start. This will cancel the channel-swith
 * timer which is already running.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 *
 * @return None
 */
void lim_cancel_dot11h_channel_switch(tpAniSirGlobal pMac,
				      tpPESession psessionEntry)
{
	if (!LIM_IS_STA_ROLE(psessionEntry))
		return;

	pe_debug("Received a beacon without channel switch IE");

	MTRACE(mac_trace
		       (pMac, TRACE_CODE_TIMER_DEACTIVATE,
		       psessionEntry->peSessionId, eLIM_CHANNEL_SWITCH_TIMER));

	if (tx_timer_deactivate(&pMac->lim.limTimers.gLimChannelSwitchTimer) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("tx_timer_deactivate failed!");
	}

	/* We need to restore pre-channelSwitch state on the STA */
	if (lim_restore_pre_channel_switch_state(pMac, psessionEntry) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("LIM: Could not restore pre-channelSwitch (11h) state, resetting the system");
	}
}

/**
 * lim_cancel_dot11h_quiet()
 *
 * @mac_ctx: pointer to global mac structure
 * @psession_entry: pointer to tppesession
 *
 * Cancel the quieting on Station if latest beacon
 * doesn't contain quiet IE in it.
 *
 * Return: none
 */
void lim_cancel_dot11h_quiet(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
	if (!LIM_IS_STA_ROLE(psessionEntry))
		return;

	if (psessionEntry->gLimSpecMgmt.quietState == eLIM_QUIET_BEGIN) {
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_TIMER_DEACTIVATE,
			       psessionEntry->peSessionId, eLIM_QUIET_TIMER));
		if (tx_timer_deactivate(&pMac->lim.limTimers.gLimQuietTimer) !=
		    TX_SUCCESS) {
			pe_err("tx_timer_deactivate failed");
		}
	} else if (psessionEntry->gLimSpecMgmt.quietState == eLIM_QUIET_RUNNING) {
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_TIMER_DEACTIVATE,
			       psessionEntry->peSessionId, eLIM_QUIET_BSS_TIMER));
		if (tx_timer_deactivate(&pMac->lim.limTimers.gLimQuietBssTimer)
		    != TX_SUCCESS) {
			pe_err("tx_timer_deactivate failed");
		}
		/**
		 * If the channel switch is already running in silent mode, dont resume the
		 * transmission. Channel switch timer when timeout, transmission will be resumed.
		 */
		if (!
		    ((psessionEntry->gLimSpecMgmt.dot11hChanSwState ==
		      eLIM_11H_CHANSW_RUNNING)
		     && (psessionEntry->gLimChannelSwitch.switchMode ==
			 eSIR_CHANSW_MODE_SILENT))) {
			lim_frame_transmission_control(pMac, eLIM_TX_ALL,
						       eLIM_RESUME_TX);
			lim_restore_pre_quiet_state(pMac, psessionEntry);
		}
	}
	psessionEntry->gLimSpecMgmt.quietState = eLIM_QUIET_INIT;
}

/**
 * lim_process_quiet_timeout
 *
 * FUNCTION:
 * This function is active only on the STA.
 * Handles SIR_LIM_QUIET_TIMEOUT
 *
 * LOGIC:
 * This timeout can occur under only one circumstance:
 *
 * 1) When gLimQuietState = eLIM_QUIET_BEGIN
 * This indicates that the timeout "interval" has
 * expired. This is a trigger for the STA to now
 * shut-off Tx/Rx for the specified gLimQuietDuration
 * -> The TIMER object gLimQuietBssTimer is
 * activated
 * -> With timeout = gLimQuietDuration
 * -> gLimQuietState is set to eLIM_QUIET_RUNNING
 *
 * ASSUMPTIONS:
 * Using two TIMER objects -
 * gLimQuietTimer & gLimQuietBssTimer
 *
 * NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 *
 * @return None
 */
void lim_process_quiet_timeout(tpAniSirGlobal pMac)
{
	tpPESession psessionEntry;

	psessionEntry = pe_find_session_by_session_id(pMac,
			pMac->lim.limTimers.gLimQuietTimer.sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}

	pe_debug("quietState: %d", psessionEntry->gLimSpecMgmt.quietState);
	switch (psessionEntry->gLimSpecMgmt.quietState) {
	case eLIM_QUIET_BEGIN:
		/* Time to Stop data traffic for quietDuration */
		/* lim_deactivate_and_change_timer(pMac, eLIM_QUIET_BSS_TIMER); */
		if (TX_SUCCESS != tx_timer_deactivate(
				&pMac->lim.limTimers.gLimQuietBssTimer)) {
			pe_err("Unable to de-activate gLimQuietBssTimer! Will attempt to activate anyway");
		}
		/* gLimQuietDuration appears to be in units of ticks */
		/* Use it as is */
		if (TX_SUCCESS !=
		    tx_timer_change(&pMac->lim.limTimers.gLimQuietBssTimer,
				    psessionEntry->gLimSpecMgmt.quietDuration,
				    0)) {
			pe_err("Unable to change gLimQuietBssTimer! Will still attempt to activate anyway");
		}
		MTRACE(mac_trace
			       (pMac, TRACE_CODE_TIMER_ACTIVATE,
			       pMac->lim.limTimers.gLimQuietTimer.sessionId,
			       eLIM_QUIET_BSS_TIMER));
		if (TX_SUCCESS !=
		    tx_timer_activate(&pMac->lim.limTimers.gLimQuietBssTimer)) {
			pe_warn("Unable to activate gLimQuietBssTimer! The STA will be unable to honor Quiet BSS");
		} else {
			/* Transition to eLIM_QUIET_RUNNING */
			psessionEntry->gLimSpecMgmt.quietState =
				eLIM_QUIET_RUNNING;
			/* Shut-off Tx/Rx for gLimSpecMgmt.quietDuration */
			/* freeze the transmission */
			lim_frame_transmission_control(pMac, eLIM_TX_ALL,
						       eLIM_STOP_TX);

			pe_debug("Quiet BSS: STA shutting down for %d ticks",
				psessionEntry->gLimSpecMgmt.quietDuration);
		}
		break;

	case eLIM_QUIET_RUNNING:
	case eLIM_QUIET_INIT:
	case eLIM_QUIET_END:
	default:
		/* */
		/* As of now, nothing to be done */
		/* */
		break;
	}
}

/**
 * lim_process_quiet_bss_timeout() - Handles SIR_LIM_QUIET_BSS_TIMEOUT
 * @mac_ctx: pointer to Globale Mac Structure
 *
 * This function is active on the AP and STA.
 * Handles SIR_LIM_QUIET_BSS_TIMEOUT
 *
 * On the AP -
 * When the SIR_LIM_QUIET_BSS_TIMEOUT is triggered, it is
 * an indication for the AP to START sending out the
 * Quiet BSS IE.
 * If 802.11H is enabled, the Quiet BSS IE is sent as per
 * the 11H spec
 * If 802.11H is not enabled, the Quiet BSS IE is sent as
 * a Proprietary IE. This will be understood by all the
 * TITAN STA's
 * Transitioning gLimQuietState to eLIM_QUIET_BEGIN will
 * initiate the SCH to include the Quiet BSS IE in all
 * its subsequent Beacons/PR's.
 * The Quiet BSS IE will be included in all the Beacons
 * & PR's until the next DTIM period
 *
 * On the STA -
 * When gLimQuietState = eLIM_QUIET_RUNNING
 * This indicates that the STA was successfully shut-off
 * for the specified gLimQuietDuration. This is a trigger
 * for the STA to now resume data traffic.
 * -> gLimQuietState is set to eLIM_QUIET_INIT
 *
 *
 * Return: none
 */
void lim_process_quiet_bss_timeout(tpAniSirGlobal mac_ctx)
{
	tpPESession psession_entry = NULL;
	tLimTimers *lim_timer = &mac_ctx->lim.limTimers;

	psession_entry = pe_find_session_by_session_id(mac_ctx,
					lim_timer->gLimQuietBssTimer.sessionId);

	if (psession_entry == NULL) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}

	pe_debug("quietState: %d",
			psession_entry->gLimSpecMgmt.quietState);

	if (LIM_IS_AP_ROLE(psession_entry))
		return;

	/* eLIM_STA_ROLE */
	switch (psession_entry->gLimSpecMgmt.quietState) {
	case eLIM_QUIET_RUNNING:
		/* Transition to eLIM_QUIET_INIT */
		psession_entry->gLimSpecMgmt.quietState = eLIM_QUIET_INIT;

		/*
		 * Resume data traffic only if channel switch is
		 * not running in silent mode.
		 */
		if (!((psession_entry->gLimSpecMgmt.dot11hChanSwState ==
				eLIM_11H_CHANSW_RUNNING) &&
			(psession_entry->gLimChannelSwitch.switchMode ==
				eSIR_CHANSW_MODE_SILENT))) {
			lim_frame_transmission_control(mac_ctx, eLIM_TX_ALL,
				eLIM_RESUME_TX);
			lim_restore_pre_quiet_state(mac_ctx, psession_entry);
		}
		pe_debug("Quiet BSS: Resuming traffic");
		break;

	case eLIM_QUIET_INIT:
	case eLIM_QUIET_BEGIN:
	case eLIM_QUIET_END:
		pe_debug("Quiet state not in RUNNING");
		/*
		 * If the quiet period has ended, then resume the
		 * frame transmission
		 */
		lim_frame_transmission_control(mac_ctx, eLIM_TX_ALL,
					eLIM_RESUME_TX);
		lim_restore_pre_quiet_state(mac_ctx, psession_entry);
		psession_entry->gLimSpecMgmt.quietState = eLIM_QUIET_INIT;
		break;

	default:
		/* As of now, nothing to be done */
		break;
	}
}

/**----------------------------------------------
   \fn        lim_start_quiet_timer
   \brief    Starts the quiet timer.

   \param pMac
   \return NONE
   -----------------------------------------------*/
void lim_start_quiet_timer(tpAniSirGlobal pMac, uint8_t sessionId)
{
	tpPESession psessionEntry;

	psessionEntry = pe_find_session_by_session_id(pMac, sessionId);

	if (psessionEntry == NULL) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}

	if (!LIM_IS_STA_ROLE(psessionEntry))
		return;
	/* First, de-activate Timer, if its already active */
	lim_cancel_dot11h_quiet(pMac, psessionEntry);

	MTRACE(mac_trace
		       (pMac, TRACE_CODE_TIMER_ACTIVATE, sessionId, eLIM_QUIET_TIMER));
	if (TX_SUCCESS !=
	    tx_timer_deactivate(&pMac->lim.limTimers.gLimQuietTimer)) {
		pe_err("Unable to deactivate gLimQuietTimer! Will still attempt to re-activate anyway");
	}
	/* Set the NEW timeout value, in ticks */
	if (TX_SUCCESS != tx_timer_change(&pMac->lim.limTimers.gLimQuietTimer,
					  SYS_MS_TO_TICKS(psessionEntry->
							  gLimSpecMgmt.
							  quietTimeoutValue),
					  0)) {
		pe_err("Unable to change gLimQuietTimer! Will still attempt to re-activate anyway");
	}

	pMac->lim.limTimers.gLimQuietTimer.sessionId = sessionId;
	if (TX_SUCCESS !=
	    tx_timer_activate(&pMac->lim.limTimers.gLimQuietTimer)) {
		pe_err("Unable to activate gLimQuietTimer! STA cannot honor Quiet BSS!");
		lim_restore_pre_quiet_state(pMac, psessionEntry);

		psessionEntry->gLimSpecMgmt.quietState = eLIM_QUIET_INIT;
		return;
	}
}

/** ------------------------------------------------------------------------ **/
/**
 * keep track of the number of ANI peers associated in the BSS
 * For the first and last ANI peer, we have to update EDCA params as needed
 *
 * When the first ANI peer joins the BSS, we notify SCH
 * When the last ANI peer leaves the BSS, we notfiy SCH
 */
void
lim_util_count_sta_add(tpAniSirGlobal pMac,
		       tpDphHashNode pSta, tpPESession psessionEntry)
{

	if ((!pSta) || (!pSta->valid) || (pSta->fAniCount))
		return;

	pSta->fAniCount = 1;

	if (pMac->lim.gLimNumOfAniSTAs++ != 0)
		return;

	/* get here only if this is the first ANI peer in the BSS */
	sch_edca_profile_update(pMac, psessionEntry);
}

void
lim_util_count_sta_del(tpAniSirGlobal pMac,
		       tpDphHashNode pSta, tpPESession psessionEntry)
{

	if ((pSta == NULL) || (!pSta->fAniCount))
		return;

	/* Only if sta is invalid and the validInDummyState bit is set to 1,
	 * then go ahead and update the count and profiles. This ensures
	 * that the "number of ani station" count is properly incremented/decremented.
	 */
	if (pSta->valid == 1)
		return;

	pSta->fAniCount = 0;

	if (pMac->lim.gLimNumOfAniSTAs <= 0) {
		pe_err("CountStaDel: ignoring Delete Req when AniPeer count: %d",
			pMac->lim.gLimNumOfAniSTAs);
		return;
	}

	pMac->lim.gLimNumOfAniSTAs--;

	if (pMac->lim.gLimNumOfAniSTAs != 0)
		return;

	/* get here only if this is the last ANI peer in the BSS */
	sch_edca_profile_update(pMac, psessionEntry);
}

/**
 * lim_switch_channel_cback()
 *
 ***FUNCTION:
 *  This is the callback function registered while requesting to switch channel
 *  after AP indicates a channel switch for spectrum management (11h).
 *
 ***NOTE:
 * @param  pMac               Pointer to Global MAC structure
 * @param  status             Status of channel switch request
 * @param  data               User data
 * @param  psessionEntry      Session information
 * @return NONE
 */
void lim_switch_channel_cback(tpAniSirGlobal pMac, QDF_STATUS status,
			      uint32_t *data, tpPESession psessionEntry)
{
	struct scheduler_msg mmhMsg = { 0 };
	tSirSmeSwitchChannelInd *pSirSmeSwitchChInd;

	psessionEntry->currentOperChannel = psessionEntry->currentReqChannel;

	/* We need to restore pre-channelSwitch state on the STA */
	if (lim_restore_pre_channel_switch_state(pMac, psessionEntry) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("Could not restore pre-channelSwitch (11h) state, resetting the system");
		return;
	}

	mmhMsg.type = eWNI_SME_SWITCH_CHL_IND;
	pSirSmeSwitchChInd = qdf_mem_malloc(sizeof(tSirSmeSwitchChannelInd));
	if (NULL == pSirSmeSwitchChInd) {
		pe_err("Failed to allocate buffer for buffer descriptor");
		return;
	}

	pSirSmeSwitchChInd->messageType = eWNI_SME_SWITCH_CHL_IND;
	pSirSmeSwitchChInd->length = sizeof(tSirSmeSwitchChannelInd);
	pSirSmeSwitchChInd->newChannelId =
		psessionEntry->gLimChannelSwitch.primaryChannel;
	pSirSmeSwitchChInd->sessionId = psessionEntry->smeSessionId;
	pSirSmeSwitchChInd->chan_params.ch_width =
			psessionEntry->gLimChannelSwitch.ch_width;
	pSirSmeSwitchChInd->chan_params.sec_ch_offset =
			psessionEntry->gLimChannelSwitch.sec_ch_offset;
	pSirSmeSwitchChInd->chan_params.center_freq_seg0 =
			psessionEntry->gLimChannelSwitch.ch_center_freq_seg0;
	pSirSmeSwitchChInd->chan_params.center_freq_seg1 =
			psessionEntry->gLimChannelSwitch.ch_center_freq_seg1;

	qdf_mem_copy(pSirSmeSwitchChInd->bssid.bytes, psessionEntry->bssId,
		     QDF_MAC_ADDR_SIZE);
	mmhMsg.bodyptr = pSirSmeSwitchChInd;
	mmhMsg.bodyval = 0;

	MTRACE(mac_trace(pMac, TRACE_CODE_TX_SME_MSG,
			 psessionEntry->peSessionId, mmhMsg.type));

	sys_process_mmh_msg(pMac, &mmhMsg);
}

/**
 * lim_switch_primary_channel()
 *
 ***FUNCTION:
 *  This function changes the current operating channel
 *  and sets the new new channel ID in WNI_CFG_CURRENT_CHANNEL.
 *
 ***NOTE:
 * @param  pMac        Pointer to Global MAC structure
 * @param  newChannel  new chnannel ID
 * @return NONE
 */
void lim_switch_primary_channel(tpAniSirGlobal pMac, uint8_t newChannel,
				tpPESession psessionEntry)
{
	pe_debug("chan: %d --> chan: %d",
		 psessionEntry->currentOperChannel, newChannel);

	psessionEntry->currentReqChannel = newChannel;
	psessionEntry->limRFBand = lim_get_rf_band(newChannel);

	psessionEntry->channelChangeReasonCode = LIM_SWITCH_CHANNEL_OPERATION;

	pMac->lim.gpchangeChannelCallback = lim_switch_channel_cback;
	pMac->lim.gpchangeChannelData = NULL;

	lim_send_switch_chnl_params(pMac, newChannel, 0, 0, CH_WIDTH_20MHZ,
				    psessionEntry->maxTxPower,
				    psessionEntry->peSessionId, false, 0, 0);
	return;
}

/**
 * lim_switch_primary_secondary_channel()
 *
 ***FUNCTION:
 *  This function changes the primary and secondary channel.
 *  If 11h is enabled and user provides a "new channel ID"
 *  that is different from the current operating channel,
 *  then we must set this new channel in WNI_CFG_CURRENT_CHANNEL,
 *  assign notify LIM of such change.
 *
 ***NOTE:
 * @param  pMac        Pointer to Global MAC structure
 * @param  newChannel  New chnannel ID (or current channel ID)
 * @param  subband     CB secondary info:
 *                       - eANI_CB_SECONDARY_NONE
 *                       - eANI_CB_SECONDARY_UP
 *                       - eANI_CB_SECONDARY_DOWN
 * @return NONE
 */
void lim_switch_primary_secondary_channel(tpAniSirGlobal pMac,
					tpPESession psessionEntry,
					uint8_t newChannel,
					uint8_t ch_center_freq_seg0,
					uint8_t ch_center_freq_seg1,
					enum phy_ch_width ch_width)
{

	/* Assign the callback to resume TX once channel is changed. */
	psessionEntry->currentReqChannel = newChannel;
	psessionEntry->limRFBand = lim_get_rf_band(newChannel);
	psessionEntry->channelChangeReasonCode = LIM_SWITCH_CHANNEL_OPERATION;
	pMac->lim.gpchangeChannelCallback = lim_switch_channel_cback;
	pMac->lim.gpchangeChannelData = NULL;

	lim_send_switch_chnl_params(pMac, newChannel, ch_center_freq_seg0,
					ch_center_freq_seg1, ch_width,
					psessionEntry->maxTxPower,
					psessionEntry->peSessionId,
					false, 0, 0);

	/* Store the new primary and secondary channel in session entries if different */
	if (psessionEntry->currentOperChannel != newChannel) {
		pe_warn("chan: %d --> chan: %d",
			psessionEntry->currentOperChannel, newChannel);
		psessionEntry->currentOperChannel = newChannel;
	}
	if (psessionEntry->htSecondaryChannelOffset !=
			psessionEntry->gLimChannelSwitch.sec_ch_offset) {
		pe_warn("HT sec chnl: %d --> HT sec chnl: %d",
			psessionEntry->htSecondaryChannelOffset,
			psessionEntry->gLimChannelSwitch.sec_ch_offset);
		psessionEntry->htSecondaryChannelOffset =
			psessionEntry->gLimChannelSwitch.sec_ch_offset;
		if (psessionEntry->htSecondaryChannelOffset ==
		    PHY_SINGLE_CHANNEL_CENTERED) {
			psessionEntry->htSupportedChannelWidthSet =
				WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
		} else {
			psessionEntry->htSupportedChannelWidthSet =
				WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;
		}
		psessionEntry->htRecommendedTxWidthSet =
			psessionEntry->htSupportedChannelWidthSet;
	}

	return;
}

/**
 * lim_get_ht_capability()
 *
 ***FUNCTION:
 * A utility function that returns the "current HT capability state" for the HT
 * capability of interest (as requested in the API)
 *
 ***LOGIC:
 * This routine will return with the "current" setting of a requested HT
 * capability. This state info could be retrieved from -
 * a) CFG (for static entries)
 * b) Run time info
 *   - Dynamic state maintained by LIM
 *   - Configured at radio init time by SME
 *
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  pMac  Pointer to Global MAC structure
 * @param  htCap The HT capability being queried
 * @return uint8_t The current state of the requested HT capability is returned in a
 *            uint8_t variable
 */

uint8_t lim_get_ht_capability(tpAniSirGlobal pMac,
			      uint32_t htCap, tpPESession psessionEntry)
{
	uint8_t retVal = 0;
	uint8_t *ptr;
	uint32_t cfgValue;
	tSirMacHTCapabilityInfo macHTCapabilityInfo = { 0 };
	tSirMacExtendedHTCapabilityInfo macExtHTCapabilityInfo = { 0 };
	tSirMacTxBFCapabilityInfo macTxBFCapabilityInfo = { 0 };
	tSirMacASCapabilityInfo macASCapabilityInfo = { 0 };

	/* */
	/* Determine which CFG to read from. Not ALL of the HT */
	/* related CFG's need to be read each time this API is */
	/* accessed */
	/* */
	if (htCap >= eHT_ANTENNA_SELECTION && htCap < eHT_SI_GRANULARITY) {
		/* Get Antenna Seletion HT Capabilities */
		if (QDF_STATUS_SUCCESS !=
		    wlan_cfg_get_int(pMac, WNI_CFG_AS_CAP, &cfgValue))
			cfgValue = 0;
		ptr = (uint8_t *) &macASCapabilityInfo;
		*((uint8_t *) ptr) = (uint8_t) (cfgValue & 0xff);
	} else {
		if (htCap >= eHT_TX_BEAMFORMING &&
		    htCap < eHT_ANTENNA_SELECTION) {
			/* Get Transmit Beam Forming HT Capabilities */
			if (QDF_STATUS_SUCCESS !=
			    wlan_cfg_get_int(pMac, WNI_CFG_TX_BF_CAP, &cfgValue))
				cfgValue = 0;
			ptr = (uint8_t *) &macTxBFCapabilityInfo;
			*((uint32_t *) ptr) = (uint32_t) (cfgValue);
		} else {
			if (htCap >= eHT_PCO && htCap < eHT_TX_BEAMFORMING) {
				/* Get Extended HT Capabilities */
				if (QDF_STATUS_SUCCESS !=
				    wlan_cfg_get_int(pMac,
						     WNI_CFG_EXT_HT_CAP_INFO,
						     &cfgValue))
					cfgValue = 0;
				ptr = (uint8_t *) &macExtHTCapabilityInfo;
				*((uint16_t *) ptr) =
					(uint16_t) (cfgValue & 0xffff);
			} else {
				if (htCap < eHT_MAX_RX_AMPDU_FACTOR) {
					/* Get HT Capabilities */
					if (QDF_STATUS_SUCCESS !=
					    wlan_cfg_get_int(pMac,
							     WNI_CFG_HT_CAP_INFO,
							     &cfgValue))
						cfgValue = 0;
					ptr = (uint8_t *) &macHTCapabilityInfo;
					/* CR 265282 MDM SoftAP 2.4PL: SoftAP boot up crash in 2.4 PL builds while same WLAN SU is working on 2.1 PL */
					*ptr++ = cfgValue & 0xff;
					*ptr = (cfgValue >> 8) & 0xff;
				}
			}
		}
	}

	switch (htCap) {
	case eHT_LSIG_TXOP_PROTECTION:
		retVal = pMac->lim.gHTLsigTXOPProtection;
		break;

	case eHT_STBC_CONTROL_FRAME:
		retVal = (uint8_t) macHTCapabilityInfo.stbcControlFrame;
		break;

	case eHT_PSMP:
		retVal = pMac->lim.gHTPSMPSupport;
		break;

	case eHT_DSSS_CCK_MODE_40MHZ:
		retVal = pMac->lim.gHTDsssCckRate40MHzSupport;
		break;

	case eHT_MAX_AMSDU_LENGTH:
		retVal = (uint8_t) macHTCapabilityInfo.maximalAMSDUsize;
		break;

	case eHT_MAX_AMSDU_NUM:
		retVal = (uint8_t) psessionEntry->max_amsdu_num;
		break;

	case eHT_RX_STBC:
		retVal = (uint8_t) psessionEntry->htConfig.ht_rx_stbc;
		break;

	case eHT_TX_STBC:
		retVal = (uint8_t) psessionEntry->htConfig.ht_tx_stbc;
		break;

	case eHT_SHORT_GI_40MHZ:
		retVal = (uint8_t) (psessionEntry->htConfig.ht_sgi40) ?
			 macHTCapabilityInfo.shortGI40MHz : 0;
		break;

	case eHT_SHORT_GI_20MHZ:
		retVal = (uint8_t) (psessionEntry->htConfig.ht_sgi20) ?
			 macHTCapabilityInfo.shortGI20MHz : 0;
		break;

	case eHT_GREENFIELD:
		retVal = (uint8_t) macHTCapabilityInfo.greenField;
		break;

	case eHT_MIMO_POWER_SAVE:
		retVal = (uint8_t) pMac->lim.gHTMIMOPSState;
		break;

	case eHT_SUPPORTED_CHANNEL_WIDTH_SET:
		retVal = (uint8_t) psessionEntry->htSupportedChannelWidthSet;
		break;

	case eHT_ADVANCED_CODING:
		retVal = (uint8_t) psessionEntry->htConfig.ht_rx_ldpc;
		break;

	case eHT_MAX_RX_AMPDU_FACTOR:
		retVal = pMac->lim.gHTMaxRxAMpduFactor;
		break;

	case eHT_MPDU_DENSITY:
		retVal = pMac->lim.gHTAMpduDensity;
		break;

	case eHT_PCO:
		retVal = (uint8_t) macExtHTCapabilityInfo.pco;
		break;

	case eHT_TRANSITION_TIME:
		retVal = (uint8_t) macExtHTCapabilityInfo.transitionTime;
		break;

	case eHT_MCS_FEEDBACK:
		retVal = (uint8_t) macExtHTCapabilityInfo.mcsFeedback;
		break;

	case eHT_TX_BEAMFORMING:
		retVal = (uint8_t) macTxBFCapabilityInfo.txBF;
		break;

	case eHT_ANTENNA_SELECTION:
		retVal = (uint8_t) macASCapabilityInfo.antennaSelection;
		break;

	case eHT_SI_GRANULARITY:
		retVal = pMac->lim.gHTServiceIntervalGranularity;
		break;

	case eHT_CONTROLLED_ACCESS:
		retVal = pMac->lim.gHTControlledAccessOnly;
		break;

	case eHT_RIFS_MODE:
		retVal = psessionEntry->beaconParams.fRIFSMode;
		break;

	case eHT_RECOMMENDED_TX_WIDTH_SET:
		retVal = psessionEntry->htRecommendedTxWidthSet;
		break;

	case eHT_EXTENSION_CHANNEL_OFFSET:
		retVal = psessionEntry->htSecondaryChannelOffset;
		break;

	case eHT_OP_MODE:
		if (LIM_IS_AP_ROLE(psessionEntry))
			retVal = psessionEntry->htOperMode;
		else
			retVal = pMac->lim.gHTOperMode;
		break;

	case eHT_BASIC_STBC_MCS:
		retVal = pMac->lim.gHTSTBCBasicMCS;
		break;

	case eHT_DUAL_CTS_PROTECTION:
		retVal = pMac->lim.gHTDualCTSProtection;
		break;

	case eHT_LSIG_TXOP_PROTECTION_FULL_SUPPORT:
		retVal =
			psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport;
		break;

	case eHT_PCO_ACTIVE:
		retVal = pMac->lim.gHTPCOActive;
		break;

	case eHT_PCO_PHASE:
		retVal = pMac->lim.gHTPCOPhase;
		break;

	default:
		break;
	}

	return retVal;
}

/**
 * lim_enable_11a_protection() - updates protection params for enable 11a
 * protection request
 * @mac_ctx:    pointer to Global MAC structure
 * @overlap:    1=> called from overlap context, 0 => called from assoc context.
 * @bcn_prms:   beacon parameters
 * @pe_session: pe session entry
 *
 * This function updates protection params for enable 11a protection request
 *
 * @Return: void
 */
static void
lim_enable_11a_protection(tpAniSirGlobal mac_ctx,
			 uint8_t overlap,
			 tpUpdateBeaconParams bcn_prms,
			 tpPESession pe_session)
{
	/*
	 * If we are AP and HT capable, we need to set the HT OP mode
	 * appropriately.
	 */
	if (LIM_IS_AP_ROLE(pe_session) && (true == pe_session->htCapability)) {
		if (overlap) {
			pe_session->gLimOverlap11aParams.protectionEnabled =
				true;
			if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
			    mac_ctx->lim.gHTOperMode)
				&& (eSIR_HT_OP_MODE_MIXED !=
				    mac_ctx->lim.gHTOperMode)) {
				mac_ctx->lim.gHTOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				pe_session->htOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				lim_enable_ht_rifs_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
				lim_enable_ht_obss_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
			}
		} else {
			pe_session->gLim11aParams.protectionEnabled = true;
			if (eSIR_HT_OP_MODE_MIXED != pe_session->htOperMode) {
				mac_ctx->lim.gHTOperMode =
					eSIR_HT_OP_MODE_MIXED;
				pe_session->htOperMode = eSIR_HT_OP_MODE_MIXED;
				lim_enable_ht_rifs_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
				lim_enable_ht_obss_protection(mac_ctx, true,
					overlap, bcn_prms, pe_session);
			}
		}
	}
	/* This part is common for station as well. */
	if (false == pe_session->beaconParams.llaCoexist) {
		pe_debug(" => protection from 11A Enabled");
		bcn_prms->llaCoexist = true;
		pe_session->beaconParams.llaCoexist = true;
		bcn_prms->paramChangeBitmap |= PARAM_llACOEXIST_CHANGED;
	}
}

/**
 * lim_disable_11a_protection() - updates protection params for disable 11a
 * protection request
 * @mac_ctx:    pointer to Global MAC structure
 * @overlap:    1=> called from overlap context, 0 => called from assoc context.
 * @bcn_prms:   beacon parameters
 * @pe_session: pe session entry
 *
 * This function updates protection params for disable 11a protection request
 *
 * @Return: void
 */
static void
lim_disable_11a_protection(tpAniSirGlobal mac_ctx,
			   uint8_t overlap,
			   tpUpdateBeaconParams bcn_prms,
			   tpPESession pe_session)
{
	if (false == pe_session->beaconParams.llaCoexist)
		return;

	/* for station role */
	if (!LIM_IS_AP_ROLE(pe_session)) {
		pe_debug("===> Protection from 11A Disabled");
		bcn_prms->llaCoexist = false;
		pe_session->beaconParams.llaCoexist = false;
		bcn_prms->paramChangeBitmap |= PARAM_llACOEXIST_CHANGED;
		return;
	}
	/*
	 * for AP role.
	 * we need to take care of HT OP mode change if needed.
	 * We need to take care of Overlap cases.
	 */
	if (overlap) {
		/* Overlap Legacy protection disabled. */
		pe_session->gLimOverlap11aParams.protectionEnabled = false;

		/*
		 * We need to take care of HT OP mode iff we are HT AP.
		 * OR no HT op-mode change is needed if any of the overlap
		 * protection enabled.
		 */
		if (!pe_session->htCapability ||
		     (pe_session->gLimOverlap11aParams.protectionEnabled
		     || pe_session->gLimOverlapHt20Params.protectionEnabled
		     || pe_session->gLimOverlapNonGfParams.protectionEnabled))
			goto disable_11a_end;

		/* Check if there is a need to change HT OP mode. */
		if (eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
		    mac_ctx->lim.gHTOperMode) {
			lim_enable_ht_rifs_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);
			lim_enable_ht_obss_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);

			if (pe_session->gLimHt20Params.protectionEnabled)
				mac_ctx->lim.gHTOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			else
				mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
		}
	} else {
		/* Disable protection from 11A stations. */
		pe_session->gLim11aParams.protectionEnabled = false;
		lim_enable_ht_obss_protection(mac_ctx, false, overlap,
					      bcn_prms, pe_session);

		/*
		 * Check if any other non-HT protection enabled. Right now we
		 * are in HT OP Mixed mode. Change HT op mode appropriately.
		 */

		/* Change HT OP mode to 01 if any overlap protection enabled */
		if (pe_session->gLimOverlap11aParams.protectionEnabled
		    || pe_session->gLimOverlapHt20Params.protectionEnabled
		    || pe_session->gLimOverlapNonGfParams.protectionEnabled) {
			mac_ctx->lim.gHTOperMode =
				eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			pe_session->htOperMode = eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			lim_enable_ht_rifs_protection(mac_ctx, true, overlap,
						      bcn_prms, pe_session);
		} else if (pe_session->gLimHt20Params.protectionEnabled) {
			mac_ctx->lim.gHTOperMode =
				eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			pe_session->htOperMode =
				eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			lim_enable_ht_rifs_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);
		} else {
			mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
			pe_session->htOperMode = eSIR_HT_OP_MODE_PURE;
			lim_enable_ht_rifs_protection(mac_ctx, false, overlap,
						      bcn_prms, pe_session);
		}
	}

disable_11a_end:
	if (!pe_session->gLimOverlap11aParams.protectionEnabled &&
	    !pe_session->gLim11aParams.protectionEnabled) {
		pe_warn("===> Protection from 11A Disabled");
		bcn_prms->llaCoexist = false;
		pe_session->beaconParams.llaCoexist = false;
		bcn_prms->paramChangeBitmap |= PARAM_llACOEXIST_CHANGED;
	}
}

/**
 * lim_update_11a_protection() - based on config setting enables\disables 11a
 * protection.
 * @mac_ctx:    pointer to Global MAC structure
 * @enable:     1=> enable protection, 0=> disable protection.
 * @overlap:    1=> called from overlap context, 0 => called from assoc context.
 * @bcn_prms:   beacon parameters
 * @session:    pe session entry
 *
 * This based on config setting enables\disables 11a protection.
 *
 * @Return: success of failure of operation
 */
QDF_STATUS
lim_update_11a_protection(tpAniSirGlobal mac_ctx, uint8_t enable,
			 uint8_t overlap, tpUpdateBeaconParams bcn_prms,
			 tpPESession session)
{
	if (NULL == session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/* overlapping protection configuration check. */
	if (!overlap) {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(session)) &&
		    (!session->cfgProtection.fromlla)) {
			/* protection disabled. */
			pe_warn("protection from 11a is disabled");
			return QDF_STATUS_SUCCESS;
		}
	}

	if (enable)
		lim_enable_11a_protection(mac_ctx, overlap, bcn_prms, session);
	else
		lim_disable_11a_protection(mac_ctx, overlap, bcn_prms, session);

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_handle_enable11g_protection_enabled() - handle 11g protection enabled
 * @mac_ctx: pointer to Globale Mac structure
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @session_entry: pointer to tpPESession
 *
 * Function handles 11g protection enaled case
 *
 * Return: none
 */
static void
lim_handle_enable11g_protection_enabled(tpAniSirGlobal mac_ctx,
			tpUpdateBeaconParams beaconparams,
			uint8_t overlap, tpPESession session_entry)
{
	/*
	 * If we are AP and HT capable, we need to set the HT OP mode
	 * appropriately.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		session_entry->gLimOlbcParams.protectionEnabled = true;

		pe_debug("protection from olbc is enabled");

		if (true == session_entry->htCapability) {
			if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
				session_entry->htOperMode) &&
				(eSIR_HT_OP_MODE_MIXED !=
				session_entry->htOperMode)) {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			}
			/*
			 * CR-263021: OBSS bit is not switching back to 0 after
			 * disabling the overlapping legacy BSS
			 */
			/*
			 * This fixes issue of OBSS bit not set after 11b, 11g
			 * station leaves
			 */
			lim_enable_ht_rifs_protection(mac_ctx, true,
					overlap, beaconparams, session_entry);
			/*
			 * Not processing OBSS bit from other APs, as we are
			 * already taking care of Protection from overlapping
			 * BSS based on erp IE or useProtection bit
			 */
			lim_enable_ht_obss_protection(mac_ctx, true,
					overlap, beaconparams, session_entry);
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		session_entry->gLim11bParams.protectionEnabled = true;
		pe_debug("protection from 11b is enabled");
		if (true == session_entry->htCapability) {
			if (eSIR_HT_OP_MODE_MIXED !=
				session_entry->htOperMode) {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_MIXED;
				lim_enable_ht_rifs_protection(mac_ctx,
						true, overlap, beaconparams,
						session_entry);
				lim_enable_ht_obss_protection(mac_ctx,
						true, overlap, beaconparams,
						session_entry);
			}
		}
	}

	/* This part is common for staiton as well. */
	if (false == session_entry->beaconParams.llbCoexist) {
		pe_debug("=> 11G Protection Enabled");
		beaconparams->llbCoexist =
			session_entry->beaconParams.llbCoexist = true;
		beaconparams->paramChangeBitmap |=
			PARAM_llBCOEXIST_CHANGED;
	}
}

/**
 * lim_handle_11g_protection_for_11bcoexist() - 11g protection for 11b co-ex
 * @mac_ctx: pointer to Globale Mac structure
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @session_entry: pointer to tpPESession
 *
 * Function handles 11g protection for 11b co-exist
 *
 * Return: none
 */
static void
lim_handle_11g_protection_for_11bcoexist(tpAniSirGlobal mac_ctx,
			tpUpdateBeaconParams beaconparams,
			uint8_t overlap, tpPESession session_entry)
{
	/*
	 * For AP role:
	 * we need to take care of HT OP mode change if needed.
	 * We need to take care of Overlap cases.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		/* Overlap Legacy protection disabled. */
		session_entry->gLimOlbcParams.protectionEnabled = false;

		/* We need to take care of HT OP mode if we are HT AP. */
		if (session_entry->htCapability) {
			/*
			 * no HT op mode change if any of the overlap
			 * protection enabled.
			 */
			if (!(session_entry->gLimOverlap11gParams.
					protectionEnabled ||
				session_entry->gLimOverlapHt20Params.
					protectionEnabled ||
				session_entry->gLimOverlapNonGfParams.
					protectionEnabled) &&
				/*
				 * Check if there is a need to change HT
				 * OP mode.
				 */
				(eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
						session_entry->htOperMode)) {
				lim_enable_ht_rifs_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
				lim_enable_ht_obss_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
				if (session_entry->gLimHt20Params.
						protectionEnabled) {
				if (eHT_CHANNEL_WIDTH_20MHZ ==
					session_entry->htSupportedChannelWidthSet)
					session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				else
					session_entry->htOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
				} else
					session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
			}
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		/* Disable protection from 11B stations. */
		session_entry->gLim11bParams.protectionEnabled = false;
		pe_debug("===> 11B Protection Disabled");
		/* Check if any other non-HT protection enabled. */
		if (!session_entry->gLim11gParams.protectionEnabled) {
			/* Right now we are in HT OP Mixed mode. */
			/* Change HT op mode appropriately. */
			lim_enable_ht_obss_protection(mac_ctx, false, overlap,
					beaconparams, session_entry);
			/*
			 * Change HT OP mode to 01 if any overlap protection
			 * enabled
			 */
			if (session_entry->gLimOlbcParams.protectionEnabled ||
				session_entry->gLimOverlap11gParams.
					protectionEnabled ||
				session_entry->gLimOverlapHt20Params.
					protectionEnabled ||
				session_entry->gLimOverlapNonGfParams.
					protectionEnabled) {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				pe_debug("===> 11G Protection Disabled");
				lim_enable_ht_rifs_protection(mac_ctx, true,
						overlap, beaconparams,
						session_entry);
			} else if (session_entry->gLimHt20Params.
						protectionEnabled) {
				/* Commenting because of CR 258588 WFA cert */
				/* session_entry->htOperMode =
				eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT; */
				session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				pe_debug("===> 11G Protection Disabled");
				lim_enable_ht_rifs_protection(mac_ctx, false,
						overlap, beaconparams,
						session_entry);
			} else {
				session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				lim_enable_ht_rifs_protection(mac_ctx, false,
						overlap, beaconparams,
						session_entry);
			}
		}
	}
	if (LIM_IS_AP_ROLE(session_entry)) {
		if (!session_entry->gLimOlbcParams.protectionEnabled &&
			!session_entry->gLim11bParams.protectionEnabled) {
			pe_debug("===> 11G Protection Disabled");
			beaconparams->llbCoexist =
				session_entry->beaconParams.llbCoexist =
							false;
			beaconparams->paramChangeBitmap |=
				PARAM_llBCOEXIST_CHANGED;
		}
	}
	/* For station role */
	if (!LIM_IS_AP_ROLE(session_entry)) {
		pe_debug("===> 11G Protection Disabled");
		beaconparams->llbCoexist =
			session_entry->beaconParams.llbCoexist = false;
		beaconparams->paramChangeBitmap |=
			PARAM_llBCOEXIST_CHANGED;
	}
}

/**
 * lim_enable11g_protection() - Function to enable 11g protection
 * @mac_ctx: pointer to Global Mac structure
 * @enable: 1=> enable protection, 0=> disable protection.
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to tpPESession
 *
 * based on config setting enables\disables 11g protection.
 *
 * Return: Success - QDF_STATUS_SUCCESS - Success, Error number - Failure
 */
QDF_STATUS
lim_enable11g_protection(tpAniSirGlobal mac_ctx, uint8_t enable,
			 uint8_t overlap, tpUpdateBeaconParams beaconparams,
			 tpPESession session_entry)
{

	/* overlapping protection configuration check. */
	if (!overlap) {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(session_entry)) &&
			!session_entry->cfgProtection.fromllb) {
			/* protection disabled. */
			pe_debug("protection from 11b is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(session_entry)) {
			if (!mac_ctx->lim.cfgProtection.fromllb) {
				/* protection disabled. */
				pe_debug("protection from 11b is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (enable) {
		lim_handle_enable11g_protection_enabled(mac_ctx, beaconparams,
					overlap, session_entry);
	} else if (true == session_entry->beaconParams.llbCoexist) {
		lim_handle_11g_protection_for_11bcoexist(mac_ctx, beaconparams,
					overlap, session_entry);
	}
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_enable_ht_protection_from11g
   \brief based on cofig enables\disables protection from 11g.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_protection_from11g(tpAniSirGlobal pMac, uint8_t enable,
				 uint8_t overlap,
				 tpUpdateBeaconParams pBeaconParams,
				 tpPESession psessionEntry)
{
	if (!psessionEntry->htCapability)
		return QDF_STATUS_SUCCESS;  /* protection from 11g is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
		if ((LIM_IS_AP_ROLE(psessionEntry))
		    && (!psessionEntry->cfgProtection.overlapFromllg)) {
			/* protection disabled. */
			pe_debug("overlap protection from 11g is disabled");
			return QDF_STATUS_SUCCESS;
		}
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(psessionEntry) &&
		    !psessionEntry->cfgProtection.fromllg) {
			/* protection disabled. */
			pe_debug("protection from 11g is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(psessionEntry)) {
			if (!pMac->lim.cfgProtection.fromllg) {
				/* protection disabled. */
				pe_debug("protection from 11g is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}
	if (enable) {
		/* If we are AP and HT capable, we need to set the HT OP mode */
		/* appropriately. */

		if (LIM_IS_AP_ROLE(psessionEntry)) {
			if (overlap) {
				psessionEntry->gLimOverlap11gParams.
				protectionEnabled = true;
				/* 11g exists in overlap BSS. */
				/* need not to change the operating mode to overlap_legacy */
				/* if higher or same protection operating mode is enabled right now. */
				if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
				     psessionEntry->htOperMode)
				    && (eSIR_HT_OP_MODE_MIXED !=
					psessionEntry->htOperMode)) {
					psessionEntry->htOperMode =
						eSIR_HT_OP_MODE_OVERLAP_LEGACY;
				}
				lim_enable_ht_rifs_protection(pMac, true, overlap,
							      pBeaconParams,
							      psessionEntry);
				lim_enable_ht_obss_protection(pMac, true, overlap,
							      pBeaconParams,
							      psessionEntry);
			} else {
				/* 11g is associated to an AP operating in 11n mode. */
				/* Change the HT operating mode to 'mixed mode'. */
				psessionEntry->gLim11gParams.protectionEnabled =
					true;
				if (eSIR_HT_OP_MODE_MIXED !=
				    psessionEntry->htOperMode) {
					psessionEntry->htOperMode =
						eSIR_HT_OP_MODE_MIXED;
					lim_enable_ht_rifs_protection(pMac, true,
								      overlap,
								      pBeaconParams,
								      psessionEntry);
					lim_enable_ht_obss_protection(pMac, true,
								      overlap,
								      pBeaconParams,
								      psessionEntry);
				}
			}
		}
		/* This part is common for staiton as well. */
		if (false == psessionEntry->beaconParams.llgCoexist) {
			pBeaconParams->llgCoexist =
				psessionEntry->beaconParams.llgCoexist = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_llGCOEXIST_CHANGED;
		} else if (true ==
			   psessionEntry->gLimOverlap11gParams.
			   protectionEnabled) {
			/* As operating mode changed after G station assoc some way to update beacon */
			/* This addresses the issue of mode not changing to - 11 in beacon when OBSS overlap is enabled */
			/* pMac->sch.schObject.fBeaconChanged = 1; */
			pBeaconParams->paramChangeBitmap |=
				PARAM_llGCOEXIST_CHANGED;
		}
	} else if (true == psessionEntry->beaconParams.llgCoexist) {
		/* for AP role. */
		/* we need to take care of HT OP mode change if needed. */
		/* We need to take care of Overlap cases. */

		if (LIM_IS_AP_ROLE(psessionEntry)) {
			if (overlap) {
				/* Overlap Legacy protection disabled. */
				if (psessionEntry->gLim11gParams.numSta == 0)
					psessionEntry->gLimOverlap11gParams.
					protectionEnabled = false;

				/* no HT op mode change if any of the overlap protection enabled. */
				if (!
				    (psessionEntry->gLimOlbcParams.
				     protectionEnabled
				     || psessionEntry->gLimOverlapHt20Params.
				     protectionEnabled
				     || psessionEntry->gLimOverlapNonGfParams.
				     protectionEnabled)) {
					/* Check if there is a need to change HT OP mode. */
					if (eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
					    psessionEntry->htOperMode) {
						lim_enable_ht_rifs_protection(pMac,
									      false,
									      overlap,
									      pBeaconParams,
									      psessionEntry);
						lim_enable_ht_obss_protection(pMac,
									      false,
									      overlap,
									      pBeaconParams,
									      psessionEntry);

						if (psessionEntry->gLimHt20Params.protectionEnabled) {
						if (eHT_CHANNEL_WIDTH_20MHZ ==
							psessionEntry->htSupportedChannelWidthSet)
							psessionEntry->htOperMode =
								eSIR_HT_OP_MODE_PURE;
						else
							psessionEntry->htOperMode =
								eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
						} else
							psessionEntry->htOperMode =
								eSIR_HT_OP_MODE_PURE;
					}
				}
			} else {
				/* Disable protection from 11G stations. */
				psessionEntry->gLim11gParams.protectionEnabled =
					false;
				/* Check if any other non-HT protection enabled. */
				if (!psessionEntry->gLim11bParams.
				    protectionEnabled) {

					/* Right now we are in HT OP Mixed mode. */
					/* Change HT op mode appropriately. */
					lim_enable_ht_obss_protection(pMac, false,
								      overlap,
								      pBeaconParams,
								      psessionEntry);

					/* Change HT OP mode to 01 if any overlap protection enabled */
					if (psessionEntry->gLimOlbcParams.
					    protectionEnabled
					    || psessionEntry->
					    gLimOverlap11gParams.
					    protectionEnabled
					    || psessionEntry->
					    gLimOverlapHt20Params.
					    protectionEnabled
					    || psessionEntry->
					    gLimOverlapNonGfParams.
					    protectionEnabled) {
						psessionEntry->htOperMode =
							eSIR_HT_OP_MODE_OVERLAP_LEGACY;
						lim_enable_ht_rifs_protection(pMac,
									      true,
									      overlap,
									      pBeaconParams,
									      psessionEntry);
					} else if (psessionEntry->
						   gLimHt20Params.
						   protectionEnabled) {
						/* Commenting because of CR 258588 WFA cert */
						/* psessionEntry->htOperMode = eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT; */
						psessionEntry->htOperMode =
							eSIR_HT_OP_MODE_PURE;
						lim_enable_ht_rifs_protection(pMac,
									      false,
									      overlap,
									      pBeaconParams,
									      psessionEntry);
					} else {
						psessionEntry->htOperMode =
							eSIR_HT_OP_MODE_PURE;
						lim_enable_ht_rifs_protection(pMac,
									      false,
									      overlap,
									      pBeaconParams,
									      psessionEntry);
					}
				}
			}
			if (!psessionEntry->gLimOverlap11gParams.
			    protectionEnabled
			    && !psessionEntry->gLim11gParams.
			    protectionEnabled) {
				pe_debug("===> Protection from 11G Disabled");
				pBeaconParams->llgCoexist =
					psessionEntry->beaconParams.llgCoexist =
						false;
				pBeaconParams->paramChangeBitmap |=
					PARAM_llGCOEXIST_CHANGED;
			}
		}
		/* for station role */
		else {
			pe_debug("===> Protection from 11G Disabled");
			pBeaconParams->llgCoexist =
				psessionEntry->beaconParams.llgCoexist = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_llGCOEXIST_CHANGED;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/* FIXME_PROTECTION : need to check for no APSD whenever we want to enable this protection. */
/* This check will be done at the caller. */

/** -------------------------------------------------------------
   \fn limEnableHtObssProtection
   \brief based on cofig enables\disables obss protection.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_obss_protection(tpAniSirGlobal pMac, uint8_t enable,
			      uint8_t overlap, tpUpdateBeaconParams pBeaconParams,
			      tpPESession psessionEntry)
{

	if (!psessionEntry->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
		/* overlapping protection configuration check. */
	} else {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(psessionEntry)) &&
		    !psessionEntry->cfgProtection.obss) { /* ToDo Update this field */
			/* protection disabled. */
			pe_debug("protection from Obss is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(psessionEntry)) {
			if (!pMac->lim.cfgProtection.obss) { /* ToDo Update this field */
				/* protection disabled. */
				pe_debug("protection from Obss is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (LIM_IS_AP_ROLE(psessionEntry)) {
		if ((enable)
		    && (false == psessionEntry->beaconParams.gHTObssMode)) {
			pe_debug("=>obss protection enabled");
			psessionEntry->beaconParams.gHTObssMode = true;
			pBeaconParams->paramChangeBitmap |= PARAM_OBSS_MODE_CHANGED; /* UPDATE AN ENUM FOR OBSS MODE <todo> */

		} else if (!enable
			   && (true ==
			       psessionEntry->beaconParams.gHTObssMode)) {
			pe_debug("===> obss Protection disabled");
			psessionEntry->beaconParams.gHTObssMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_OBSS_MODE_CHANGED;

		}
/* CR-263021: OBSS bit is not switching back to 0 after disabling the overlapping legacy BSS */
		if (!enable && !overlap) {
			psessionEntry->gLimOverlap11gParams.protectionEnabled =
				false;
		}
	} else {
		if ((enable)
		    && (false == psessionEntry->beaconParams.gHTObssMode)) {
			pe_debug("=>obss protection enabled");
			psessionEntry->beaconParams.gHTObssMode = true;
			pBeaconParams->paramChangeBitmap |= PARAM_OBSS_MODE_CHANGED; /* UPDATE AN ENUM FOR OBSS MODE <todo> */

		} else if (!enable
			   && (true ==
			       psessionEntry->beaconParams.gHTObssMode)) {
			pe_debug("===> obss Protection disabled");
			psessionEntry->beaconParams.gHTObssMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_OBSS_MODE_CHANGED;

		}
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_handle_ht20protection_enabled() - Handle ht20 protection  enabled
 * @mac_ctx: pointer to Gloal Mac Structure
 * @overlap: variable for overlap detection
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to tpPESession
 *
 * Function handles ht20 protection enabled
 *
 * Return: none
 */
static void lim_handle_ht20protection_enabled(tpAniSirGlobal mac_ctx,
			uint8_t overlap, tpUpdateBeaconParams beaconparams,
			tpPESession session_entry)
{
	/*
	 * If we are AP and HT capable, we need to set the HT OP mode
	 * appropriately.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		session_entry->gLimOverlapHt20Params.protectionEnabled = true;
		if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY !=
				session_entry->htOperMode) &&
			(eSIR_HT_OP_MODE_MIXED !=
				session_entry->htOperMode)) {
			session_entry->htOperMode =
				eSIR_HT_OP_MODE_OVERLAP_LEGACY;
			lim_enable_ht_rifs_protection(mac_ctx, true,
				overlap, beaconparams, session_entry);
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		session_entry->gLimHt20Params.protectionEnabled = true;
		if (eSIR_HT_OP_MODE_PURE == session_entry->htOperMode) {
			if (session_entry->htSupportedChannelWidthSet !=
					eHT_CHANNEL_WIDTH_20MHZ)
				 session_entry->htOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
			lim_enable_ht_rifs_protection(mac_ctx, false,
				overlap, beaconparams, session_entry);
			lim_enable_ht_obss_protection(mac_ctx, false,
				overlap, beaconparams, session_entry);
		}
	}
	/* This part is common for staiton as well. */
	if (false == session_entry->beaconParams.ht20Coexist) {
		pe_debug("=> Protection from HT20 Enabled");
		beaconparams->ht20MhzCoexist =
			session_entry->beaconParams.ht20Coexist = true;
		beaconparams->paramChangeBitmap |=
			PARAM_HT20MHZCOEXIST_CHANGED;
	}
}

/**
 * lim_handle_ht20coexist_ht20protection() - ht20 protection for ht20 coexist
 * @mac_ctx: pointer to Gloal Mac Structure
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to tpPESession
 * @overlap: variable for overlap detection
 *
 * Function handles ht20 protection for ht20 coexist
 *
 * Return: none
 */
static void lim_handle_ht20coexist_ht20protection(tpAniSirGlobal mac_ctx,
			tpUpdateBeaconParams beaconparams,
			tpPESession session_entry, uint8_t overlap)
{
	/*
	 * For AP role:
	 * we need to take care of HT OP mode change if needed.
	 * We need to take care of Overlap cases.
	 */
	if (LIM_IS_AP_ROLE(session_entry) && overlap) {
		/* Overlap Legacy protection disabled. */
		session_entry->gLimOverlapHt20Params.protectionEnabled =
			false;
		/*
		 * no HT op mode change if any of the overlap
		 * protection enabled.
		 */
		if (!(session_entry->gLimOlbcParams.protectionEnabled ||
			session_entry->gLimOverlap11gParams.protectionEnabled ||
			session_entry->gLimOverlapHt20Params.protectionEnabled
			|| session_entry->gLimOverlapNonGfParams.
				protectionEnabled) &&
			/*
			 * Check if there is a need to change HT
			 * OP mode.
			 */
			(eSIR_HT_OP_MODE_OVERLAP_LEGACY ==
				session_entry->htOperMode)) {
			if (session_entry->gLimHt20Params.
				protectionEnabled) {
				if (eHT_CHANNEL_WIDTH_20MHZ ==
					session_entry->
					htSupportedChannelWidthSet)
					session_entry->htOperMode =
						eSIR_HT_OP_MODE_PURE;
				else
					session_entry->htOperMode =
					eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;

				lim_enable_ht_rifs_protection(mac_ctx,
						false, overlap, beaconparams,
						session_entry);
				lim_enable_ht_obss_protection(mac_ctx,
						false, overlap, beaconparams,
						session_entry);
			} else {
				session_entry->htOperMode =
					eSIR_HT_OP_MODE_PURE;
			}
		}
	} else if (LIM_IS_AP_ROLE(session_entry) && !overlap) {
		/* Disable protection from 11G stations. */
		session_entry->gLimHt20Params.protectionEnabled = false;
		/* Change HT op mode appropriately. */
		if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT ==
				session_entry->htOperMode) {
			session_entry->htOperMode =
				eSIR_HT_OP_MODE_PURE;
			lim_enable_ht_rifs_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
			lim_enable_ht_obss_protection(mac_ctx, false,
					overlap, beaconparams, session_entry);
		}
	}
	if (LIM_IS_AP_ROLE(session_entry)) {
		pe_debug("===> Protection from HT 20 Disabled");
		beaconparams->ht20MhzCoexist =
			session_entry->beaconParams.ht20Coexist = false;
		beaconparams->paramChangeBitmap |=
			PARAM_HT20MHZCOEXIST_CHANGED;
	}
	if (!LIM_IS_AP_ROLE(session_entry)) {
		/* For station role */
		pe_debug("===> Protection from HT20 Disabled");
		beaconparams->ht20MhzCoexist =
			session_entry->beaconParams.ht20Coexist = false;
		beaconparams->paramChangeBitmap |=
			PARAM_HT20MHZCOEXIST_CHANGED;
	}
}

/**
 * lim_enable_ht20_protection() -  Function to enable ht20 protection
 * @mac_ctx: pointer to Global Mac structure
 * @enable: 1=> enable protection, 0=> disable protection.
 * @overlap: 1=> called from overlap context, 0 => called from assoc context.
 * @beaconparams: pointer to tpUpdateBeaconParams
 * @session_entry: pointer to tpPESession
 *
 * based on cofig enables\disables protection from Ht20
 *
 * Return: 0 - success
 */
QDF_STATUS lim_enable_ht20_protection(tpAniSirGlobal mac_ctx, uint8_t enable,
			   uint8_t overlap, tpUpdateBeaconParams beaconparams,
			   tpPESession session_entry)
{
	/* This protection  is only for HT stations. */
	if (!session_entry->htCapability)
		return QDF_STATUS_SUCCESS;

	/* overlapping protection configuration check. */
	if (!overlap) {
		/* normal protection config check */
		if ((LIM_IS_AP_ROLE(session_entry)) &&
		    !session_entry->cfgProtection.ht20) {
			/* protection disabled. */
			pe_debug("protection from HT20 is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(session_entry)) {
			if (!mac_ctx->lim.cfgProtection.ht20) {
				/* protection disabled. */
				pe_debug("protection from HT20 is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (enable)
		lim_handle_ht20protection_enabled(mac_ctx, overlap,
				beaconparams, session_entry);
	else if (true == session_entry->beaconParams.ht20Coexist)
		lim_handle_ht20coexist_ht20protection(mac_ctx, beaconparams,
					session_entry, overlap);

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_enable_ht_non_gf_protection
   \brief based on cofig enables\disables protection from NonGf.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_non_gf_protection(tpAniSirGlobal pMac, uint8_t enable,
				uint8_t overlap, tpUpdateBeaconParams pBeaconParams,
				tpPESession psessionEntry)
{
	if (!psessionEntry->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(psessionEntry) &&
		    !psessionEntry->cfgProtection.nonGf) {
			/* protection disabled. */
			pe_debug("protection from NonGf is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(psessionEntry)) {
			/* normal protection config check */
			if (!pMac->lim.cfgProtection.nonGf) {
				/* protection disabled. */
				pe_debug("protection from NonGf is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}
	if (LIM_IS_AP_ROLE(psessionEntry)) {
		if ((enable)
		    && (false == psessionEntry->beaconParams.llnNonGFCoexist)) {
			pe_debug(" => Protection from non GF Enabled");
			pBeaconParams->llnNonGFCoexist =
				psessionEntry->beaconParams.llnNonGFCoexist = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		} else if (!enable
			   && (true ==
			       psessionEntry->beaconParams.llnNonGFCoexist)) {
			pe_debug("===> Protection from Non GF Disabled");
			pBeaconParams->llnNonGFCoexist =
				psessionEntry->beaconParams.llnNonGFCoexist = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		}
	} else {
		if ((enable)
		    && (false == psessionEntry->beaconParams.llnNonGFCoexist)) {
			pe_debug(" => Protection from non GF Enabled");
			pBeaconParams->llnNonGFCoexist =
				psessionEntry->beaconParams.llnNonGFCoexist = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		} else if (!enable
			   && (true ==
			       psessionEntry->beaconParams.llnNonGFCoexist)) {
			pe_debug("===> Protection from Non GF Disabled");
			pBeaconParams->llnNonGFCoexist =
				psessionEntry->beaconParams.llnNonGFCoexist = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_NON_GF_DEVICES_PRESENT_CHANGED;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_enable_ht_lsig_txop_protection
   \brief based on cofig enables\disables LsigTxop protection.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_lsig_txop_protection(tpAniSirGlobal pMac, uint8_t enable,
				   uint8_t overlap,
				   tpUpdateBeaconParams pBeaconParams,
				   tpPESession psessionEntry)
{
	if (!psessionEntry->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(psessionEntry) &&
			!psessionEntry->cfgProtection.lsigTxop) {
			/* protection disabled. */
			pe_debug("protection from LsigTxop not supported is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(psessionEntry)) {
			/* normal protection config check */
			if (!pMac->lim.cfgProtection.lsigTxop) {
				/* protection disabled. */
				pe_debug("protection from LsigTxop not supported is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (LIM_IS_AP_ROLE(psessionEntry)) {
		if ((enable)
		    && (false ==
			psessionEntry->beaconParams.
			fLsigTXOPProtectionFullSupport)) {
			pe_debug(" => Protection from LsigTxop Enabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				psessionEntry->beaconParams.
				fLsigTXOPProtectionFullSupport = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		} else if (!enable
			   && (true ==
			       psessionEntry->beaconParams.
			       fLsigTXOPProtectionFullSupport)) {
			pe_debug("===> Protection from LsigTxop Disabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				psessionEntry->beaconParams.
				fLsigTXOPProtectionFullSupport = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		}
	} else {
		if ((enable)
		    && (false ==
			psessionEntry->beaconParams.
			fLsigTXOPProtectionFullSupport)) {
			pe_debug(" => Protection from LsigTxop Enabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				psessionEntry->beaconParams.
				fLsigTXOPProtectionFullSupport = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		} else if (!enable
			   && (true ==
			       psessionEntry->beaconParams.
			       fLsigTXOPProtectionFullSupport)) {
			pe_debug("===> Protection from LsigTxop Disabled");
			pBeaconParams->fLsigTXOPProtectionFullSupport =
				psessionEntry->beaconParams.
				fLsigTXOPProtectionFullSupport = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/* FIXME_PROTECTION : need to check for no APSD whenever we want to enable this protection. */
/* This check will be done at the caller. */
/** -------------------------------------------------------------
   \fn lim_enable_ht_rifs_protection
   \brief based on cofig enables\disables Rifs protection.
   \param      uint8_t enable : 1=> enable protection, 0=> disable protection.
   \param      uint8_t overlap: 1=> called from overlap context, 0 => called from assoc context.
   \param      tpUpdateBeaconParams pBeaconParams
   \return      None
   -------------------------------------------------------------*/
QDF_STATUS
lim_enable_ht_rifs_protection(tpAniSirGlobal pMac, uint8_t enable,
			      uint8_t overlap, tpUpdateBeaconParams pBeaconParams,
			      tpPESession psessionEntry)
{
	if (!psessionEntry->htCapability)
		return QDF_STATUS_SUCCESS;  /* this protection  is only for HT stations. */

	/* overlapping protection configuration check. */
	if (overlap) {
	} else {
		/* normal protection config check */
		if (LIM_IS_AP_ROLE(psessionEntry) &&
		    !psessionEntry->cfgProtection.rifs) {
			/* protection disabled. */
			pe_debug("protection from Rifs is disabled");
			return QDF_STATUS_SUCCESS;
		} else if (!LIM_IS_AP_ROLE(psessionEntry)) {
			/* normal protection config check */
			if (!pMac->lim.cfgProtection.rifs) {
				/* protection disabled. */
				pe_debug("protection from Rifs is disabled");
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	if (LIM_IS_AP_ROLE(psessionEntry)) {
		/* Disabling the RIFS Protection means Enable the RIFS mode of operation in the BSS */
		if ((!enable)
		    && (false == psessionEntry->beaconParams.fRIFSMode)) {
			pe_debug(" => Rifs protection Disabled");
			pBeaconParams->fRIFSMode =
				psessionEntry->beaconParams.fRIFSMode = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
		/* Enabling the RIFS Protection means Disable the RIFS mode of operation in the BSS */
		else if (enable
			 && (true == psessionEntry->beaconParams.fRIFSMode)) {
			pe_debug("===> Rifs Protection Enabled");
			pBeaconParams->fRIFSMode =
				psessionEntry->beaconParams.fRIFSMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
	} else {
		/* Disabling the RIFS Protection means Enable the RIFS mode of operation in the BSS */
		if ((!enable)
		    && (false == psessionEntry->beaconParams.fRIFSMode)) {
			pe_debug(" => Rifs protection Disabled");
			pBeaconParams->fRIFSMode =
				psessionEntry->beaconParams.fRIFSMode = true;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
		/* Enabling the RIFS Protection means Disable the RIFS mode of operation in the BSS */
		else if (enable
			 && (true == psessionEntry->beaconParams.fRIFSMode)) {
			pe_debug("===> Rifs Protection Enabled");
			pBeaconParams->fRIFSMode =
				psessionEntry->beaconParams.fRIFSMode = false;
			pBeaconParams->paramChangeBitmap |=
				PARAM_RIFS_MODE_CHANGED;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/* --------------------------------------------------------------------- */
/**
 * lim_enable_short_preamble
 *
 * FUNCTION:
 * Enable/Disable short preamble
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param enable        Flag to enable/disable short preamble
 * @return None
 */

QDF_STATUS
lim_enable_short_preamble(tpAniSirGlobal pMac, uint8_t enable,
			  tpUpdateBeaconParams pBeaconParams,
			  tpPESession psessionEntry)
{
	uint32_t val;

	if (wlan_cfg_get_int(pMac, WNI_CFG_SHORT_PREAMBLE, &val) != QDF_STATUS_SUCCESS) {
		/* Could not get short preamble enabled flag from CFG. Log error. */
		pe_err("could not retrieve short preamble flag");
		return QDF_STATUS_E_FAILURE;
	}

	if (!val)
		return QDF_STATUS_SUCCESS;

	if (wlan_cfg_get_int(pMac, WNI_CFG_11G_SHORT_PREAMBLE_ENABLED, &val) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("could not retrieve 11G short preamble switching  enabled flag");
		return QDF_STATUS_E_FAILURE;
	}

	if (!val) /* 11G short preamble switching is disabled. */
		return QDF_STATUS_SUCCESS;

	if (LIM_IS_AP_ROLE(psessionEntry)) {
		if (enable && (psessionEntry->beaconParams.fShortPreamble == 0)) {
			pe_debug("===> Short Preamble Enabled");
			psessionEntry->beaconParams.fShortPreamble = true;
			pBeaconParams->fShortPreamble =
				(uint8_t) psessionEntry->beaconParams.
				fShortPreamble;
			pBeaconParams->paramChangeBitmap |=
				PARAM_SHORT_PREAMBLE_CHANGED;
		} else if (!enable
			   && (psessionEntry->beaconParams.fShortPreamble ==
			       1)) {
			pe_debug("===> Short Preamble Disabled");
			psessionEntry->beaconParams.fShortPreamble = false;
			pBeaconParams->fShortPreamble =
				(uint8_t) psessionEntry->beaconParams.
				fShortPreamble;
			pBeaconParams->paramChangeBitmap |=
				PARAM_SHORT_PREAMBLE_CHANGED;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_tx_complete
 *
 * Function:
 * This is LIM's very own "TX MGMT frame complete" completion routine.
 *
 * Logic:
 * LIM wants to send a MGMT frame (broadcast or unicast)
 * LIM allocates memory using cds_packet_alloc( ..., **pData, **pPacket )
 * LIM transmits the MGMT frame using the API:
 *  wma_tx_frame( ... pPacket, ..., (void *) lim_tx_complete, pData )
 * HDD, via wma_tx_frame/DXE, "transfers" the packet over to BMU
 * HDD, if it determines that a TX completion routine (in this case
 * lim_tx_complete) has been provided, will invoke this callback
 * LIM will try to free the TX MGMT packet that was earlier allocated, in order
 * to send this MGMT frame, using the PAL API cds_packet_free( ... pData, pPacket )
 *
 * Assumptions:
 * Presently, this is ONLY being used for MGMT frames/packets
 * TODO:
 * Would it do good for LIM to have some sort of "signature" validation to
 * ensure that the pData argument passed in was a buffer that was actually
 * allocated by LIM and/or is not corrupted?
 *
 * Note: FIXME and TODO
 * Looks like cds_packet_free() is interested in pPacket. But, when this completion
 * routine is called, only pData is made available to LIM!!
 *
 * @param void A pointer to pData. Shouldn't it be pPacket?!
 *
 * @return QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS lim_tx_complete(void *context, qdf_nbuf_t buf, bool free)
{
	if (free)
		cds_packet_free((void *)buf);

	return QDF_STATUS_SUCCESS;
}

/**
 * \brief This function updates lim global structure, if CB parameters in the BSS
 *  have changed, and sends an indication to HAL also with the
 * updated HT Parameters.
 * This function does not detect the change in the primary channel, that is done as part
 * of channel Swtich IE processing.
 * If STA is configured with '20Mhz only' mode, then this function does not do anything
 * This function changes the CB mode, only if the self capability is set to '20 as well as 40Mhz'
 *
 *
 * \param pMac Pointer to global MAC structure
 *
 * \param pRcvdHTInfo Pointer to HT Info IE obtained from a  Beacon or
 * Probe Response
 *
 * \param bssIdx BSS Index of the Bss to which Station is associated.
 *
 *
 */

void lim_update_sta_run_time_ht_switch_chnl_params(tpAniSirGlobal pMac,
						   tDot11fIEHTInfo *pHTInfo,
						   uint8_t bssIdx,
						   tpPESession psessionEntry)
{
	uint8_t center_freq = 0;
	enum phy_ch_width ch_width = CH_WIDTH_20MHZ;

	/* If self capability is set to '20Mhz only', then do not change the CB mode. */
	if (!lim_get_ht_capability
		    (pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET, psessionEntry))
		return;

	if (WLAN_REG_IS_24GHZ_CH(psessionEntry->currentOperChannel) &&
		psessionEntry->force_24ghz_in_ht20) {
		pe_debug("force_24ghz_in_ht20 is set and channel is 2.4 Ghz");
		return;
	}

	if (psessionEntry->ftPEContext.ftPreAuthSession) {
		pe_err("FT PREAUTH channel change is in progress");
		return;
	}

	/*
	 * Do not try to switch channel if RoC is in progress. RoC code path
	 * uses pMac->lim.gpLimRemainOnChanReq to notify the upper layers that
	 * the device has started listening on the channel requested as part of
	 * RoC, if we set pMac->lim.gpLimRemainOnChanReq to NULL as we do below
	 * then the upper layers will think that the channel change is not
	 * successful and the RoC from the upper layer perspective will never
	 * end...
	 */
	if (pMac->lim.gpLimRemainOnChanReq) {
		pe_debug("RoC is in progress");
		return;
	}

	if (psessionEntry->ch_switch_in_progress == true) {
		pe_debug("ch switch is in progress, ignore HT IE BW update");
		return;
	}

	if (reg_get_chan_enum(pHTInfo->primaryChannel) == INVALID_CHANNEL) {
		pe_debug("Ignore Invalid channel in HT info");
		return;
	}

	/* If channel mismatch the CSA will take care of this change */
	if (pHTInfo->primaryChannel != psessionEntry->currentOperChannel) {
		pe_debug("Current channel doesnt match HT info ignore");
		return;
	}

	if (psessionEntry->htSecondaryChannelOffset !=
	    (uint8_t) pHTInfo->secondaryChannelOffset
	    || psessionEntry->htRecommendedTxWidthSet !=
	    (uint8_t) pHTInfo->recommendedTxWidthSet) {
		psessionEntry->htSecondaryChannelOffset =
			(ePhyChanBondState) pHTInfo->secondaryChannelOffset;
		psessionEntry->htRecommendedTxWidthSet =
			(uint8_t) pHTInfo->recommendedTxWidthSet;
		if (eHT_CHANNEL_WIDTH_40MHZ ==
		    psessionEntry->htRecommendedTxWidthSet) {
			ch_width = CH_WIDTH_40MHZ;
			if (PHY_DOUBLE_CHANNEL_LOW_PRIMARY ==
					pHTInfo->secondaryChannelOffset)
				center_freq = pHTInfo->primaryChannel + 2;
			else if (PHY_DOUBLE_CHANNEL_HIGH_PRIMARY ==
					pHTInfo->secondaryChannelOffset)
				center_freq = pHTInfo->primaryChannel - 2;
			else
				ch_width = CH_WIDTH_20MHZ;
		}

		/* notify HAL */
		pe_debug("Channel Information in HT IE change"
				       "d; sending notification to HAL.");
		pe_debug("Primary Channel: %d Secondary Chan"
				       "nel Offset: %d Channel Width: %d",
			pHTInfo->primaryChannel, center_freq,
			psessionEntry->htRecommendedTxWidthSet);
		psessionEntry->channelChangeReasonCode =
			LIM_SWITCH_CHANNEL_OPERATION;
		pMac->lim.gpchangeChannelCallback = NULL;
		pMac->lim.gpchangeChannelData = NULL;

		/* Before restarting vdev, delete the tdls peers */
		lim_update_tdls_set_state_for_fw(psessionEntry, false);
		lim_delete_tdls_peers(pMac, psessionEntry);
		lim_send_switch_chnl_params(pMac, (uint8_t) pHTInfo->primaryChannel,
					    center_freq, 0, ch_width,
					    psessionEntry->maxTxPower,
					    psessionEntry->peSessionId,
					    true, 0, 0);



		/* In case of IBSS, if STA should update HT Info IE in its beacons. */
		if (LIM_IS_IBSS_ROLE(psessionEntry)) {
			sch_set_fixed_beacon_fields(pMac, psessionEntry);
		}

	}
} /* End limUpdateStaRunTimeHTParams. */

/**
 * \brief This function updates the lim global structure, if any of the
 * HT Capabilities have changed.
 *
 *
 * \param pMac Pointer to Global MAC structure
 *
 * \param pHTCapability Pointer to HT Capability Information Element
 * obtained from a Beacon or Probe Response
 *
 *
 *
 */

void lim_update_sta_run_time_ht_capability(tpAniSirGlobal pMac,
					   tDot11fIEHTCaps *pHTCaps)
{

	if (pMac->lim.gHTLsigTXOPProtection !=
	    (uint8_t) pHTCaps->lsigTXOPProtection) {
		pMac->lim.gHTLsigTXOPProtection =
			(uint8_t) pHTCaps->lsigTXOPProtection;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTAMpduDensity != (uint8_t) pHTCaps->mpduDensity) {
		pMac->lim.gHTAMpduDensity = (uint8_t) pHTCaps->mpduDensity;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTMaxRxAMpduFactor !=
	    (uint8_t) pHTCaps->maxRxAMPDUFactor) {
		pMac->lim.gHTMaxRxAMpduFactor =
			(uint8_t) pHTCaps->maxRxAMPDUFactor;
		/* Send change notification to HAL */
	}

} /* End lim_update_sta_run_time_ht_capability. */

/**
 * \brief This function updates lim global structure, if any of the HT
 * Info Parameters have changed.
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param pHTInfo Pointer to the HT Info IE obtained from a Beacon or
 * Probe Response
 *
 *
 */

void lim_update_sta_run_time_ht_info(tpAniSirGlobal pMac,
				     tDot11fIEHTInfo *pHTInfo,
				     tpPESession psessionEntry)
{
	if (psessionEntry->htRecommendedTxWidthSet !=
	    (uint8_t) pHTInfo->recommendedTxWidthSet) {
		psessionEntry->htRecommendedTxWidthSet =
			(uint8_t) pHTInfo->recommendedTxWidthSet;
		/* Send change notification to HAL */
	}

	if (psessionEntry->beaconParams.fRIFSMode !=
	    (uint8_t) pHTInfo->rifsMode) {
		psessionEntry->beaconParams.fRIFSMode =
			(uint8_t) pHTInfo->rifsMode;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTServiceIntervalGranularity !=
	    (uint8_t) pHTInfo->serviceIntervalGranularity) {
		pMac->lim.gHTServiceIntervalGranularity =
			(uint8_t) pHTInfo->serviceIntervalGranularity;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTOperMode != (tSirMacHTOperatingMode) pHTInfo->opMode) {
		pMac->lim.gHTOperMode =
			(tSirMacHTOperatingMode) pHTInfo->opMode;
		/* Send change notification to HAL */
	}

	if (psessionEntry->beaconParams.llnNonGFCoexist !=
	    pHTInfo->nonGFDevicesPresent) {
		psessionEntry->beaconParams.llnNonGFCoexist =
			(uint8_t) pHTInfo->nonGFDevicesPresent;
	}

	if (pMac->lim.gHTSTBCBasicMCS != (uint8_t) pHTInfo->basicSTBCMCS) {
		pMac->lim.gHTSTBCBasicMCS = (uint8_t) pHTInfo->basicSTBCMCS;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTDualCTSProtection !=
	    (uint8_t) pHTInfo->dualCTSProtection) {
		pMac->lim.gHTDualCTSProtection =
			(uint8_t) pHTInfo->dualCTSProtection;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTSecondaryBeacon != (uint8_t) pHTInfo->secondaryBeacon) {
		pMac->lim.gHTSecondaryBeacon =
			(uint8_t) pHTInfo->secondaryBeacon;
		/* Send change notification to HAL */
	}

	if (psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport !=
	    (uint8_t) pHTInfo->lsigTXOPProtectionFullSupport) {
		psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport =
			(uint8_t) pHTInfo->lsigTXOPProtectionFullSupport;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTPCOActive != (uint8_t) pHTInfo->pcoActive) {
		pMac->lim.gHTPCOActive = (uint8_t) pHTInfo->pcoActive;
		/* Send change notification to HAL */
	}

	if (pMac->lim.gHTPCOPhase != (uint8_t) pHTInfo->pcoPhase) {
		pMac->lim.gHTPCOPhase = (uint8_t) pHTInfo->pcoPhase;
		/* Send change notification to HAL */
	}

} /* End lim_update_sta_run_time_ht_info. */

/** -------------------------------------------------------------
   \fn lim_process_hal_ind_messages
   \brief callback function for HAL indication
   \param   tpAniSirGlobal pMac
   \param    uint32_t mesgId
   \param    void *mesgParam
   \return tSirRetStatu - status
   -------------------------------------------------------------*/

QDF_STATUS lim_process_hal_ind_messages(tpAniSirGlobal pMac, uint32_t msgId,
					   void *msgParam)
{
	/* its PE's responsibility to free msgparam when its done extracting the message parameters. */
	struct scheduler_msg msg = {0};

	switch (msgId) {
	case SIR_LIM_DEL_TS_IND:
	case SIR_LIM_DELETE_STA_CONTEXT_IND:
	case SIR_LIM_BEACON_GEN_IND:
		msg.type = (uint16_t) msgId;
		msg.bodyptr = msgParam;
		msg.bodyval = 0;
		break;

	default:
		qdf_mem_free(msgParam);
		pe_err("invalid message id: %d received", msgId);
		return QDF_STATUS_E_FAILURE;
	}

	if (lim_post_msg_api(pMac, &msg) != QDF_STATUS_SUCCESS) {
		qdf_mem_free(msgParam);
		pe_err("lim_post_msg_api failed for msgid: %d", msg.type);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_validate_delts_req() - This function validates DelTs req
 * @mac_ctx: pointer to Global Mac structure
 * @delts_req: pointer to delete traffic stream structure
 * @peer_mac_addr: variable for peer mac address
 *
 * Function validates DelTs req originated by SME or by HAL and also
 * sends halMsg_DelTs to HAL
 *
 * Return: QDF_STATUS_SUCCESS - Success, QDF_STATUS_E_FAILURE - Failure
 */

QDF_STATUS
lim_validate_delts_req(tpAniSirGlobal mac_ctx, tpSirDeltsReq delts_req,
		       tSirMacAddr peer_mac_addr, tpPESession psession_entry)
{
	tpDphHashNode sta;
	uint8_t ts_status;
	tSirMacTSInfo *tsinfo;
	uint32_t i;
	uint8_t tspec_idx;

	/*
	 * if sta
	 *  - verify assoc state
	 *  - del tspec locally
	 * if ap
	 *  - verify sta is in assoc state
	 *  - del sta tspec locally
	 */
	if (delts_req == NULL) {
		pe_err("Delete TS request pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (LIM_IS_STA_ROLE(psession_entry)) {
		uint32_t val;

		/* station always talks to the AP */
		sta = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
					&psession_entry->dph.dphHashTable);

		val = sizeof(tSirMacAddr);
		sir_copy_mac_addr(peer_mac_addr, psession_entry->bssId);

	} else {
		uint16_t associd;
		uint8_t *macaddr = (uint8_t *) peer_mac_addr;

		associd = delts_req->aid;
		if (associd != 0)
			sta = dph_get_hash_entry(mac_ctx, associd,
					&psession_entry->dph.dphHashTable);
		else
			sta = dph_lookup_hash_entry(mac_ctx,
						delts_req->macaddr.bytes,
						&associd,
						&psession_entry->dph.
							dphHashTable);

		if (sta != NULL)
			/* TBD: check sta assoc state as well */
			for (i = 0; i < sizeof(tSirMacAddr); i++)
				macaddr[i] = sta->staAddr[i];
	}

	if (sta == NULL) {
		pe_err("Cannot find station context for delts req");
		return QDF_STATUS_E_FAILURE;
	}

	if ((!sta->valid) ||
		(sta->mlmStaContext.mlmState !=
			eLIM_MLM_LINK_ESTABLISHED_STATE)) {
		pe_err("Invalid Sta (or state) for DelTsReq");
		return QDF_STATUS_E_FAILURE;
	}

	delts_req->req.wsmTspecPresent = 0;
	delts_req->req.wmeTspecPresent = 0;
	delts_req->req.lleTspecPresent = 0;

	if ((sta->wsmEnabled) &&
		(delts_req->req.tspec.tsinfo.traffic.accessPolicy !=
						SIR_MAC_ACCESSPOLICY_EDCA))
		delts_req->req.wsmTspecPresent = 1;
	else if (sta->wmeEnabled)
		delts_req->req.wmeTspecPresent = 1;
	else if (sta->lleEnabled)
		delts_req->req.lleTspecPresent = 1;
	else {
		pe_warn("DELTS_REQ ignore - qos is disabled");
		return QDF_STATUS_E_FAILURE;
	}

	tsinfo = delts_req->req.wmeTspecPresent ? &delts_req->req.tspec.tsinfo
						: &delts_req->req.tsinfo;
	pe_debug("received DELTS_REQ message wmeTspecPresent: %d lleTspecPresent: %d wsmTspecPresent: %d tsid: %d  up: %d direction: %d",
		delts_req->req.wmeTspecPresent,
		delts_req->req.lleTspecPresent,
		delts_req->req.wsmTspecPresent, tsinfo->traffic.tsid,
		tsinfo->traffic.userPrio, tsinfo->traffic.direction);

	/* if no Access Control, ignore the request */
	if (lim_admit_control_delete_ts(mac_ctx, sta->assocId, tsinfo,
				&ts_status, &tspec_idx) != QDF_STATUS_SUCCESS) {
		pe_err("DELTS request for sta assocId: %d tsid: %d up: %d",
			sta->assocId, tsinfo->traffic.tsid,
			tsinfo->traffic.userPrio);
		return QDF_STATUS_E_FAILURE;
	} else if ((tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_HCCA)
				|| (tsinfo->traffic.accessPolicy ==
			SIR_MAC_ACCESSPOLICY_BOTH)) {
		/* edca only now. */
	} else if (tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_EDCA) {
		/* send message to HAL to delete TS */
		if (QDF_STATUS_SUCCESS !=
			lim_send_hal_msg_del_ts(mac_ctx, sta->staIndex,
						tspec_idx, delts_req->req,
						psession_entry->peSessionId,
						psession_entry->bssId)) {
			pe_warn("DelTs with UP: %d failed in lim_send_hal_msg_del_ts - ignoring request",
				tsinfo->traffic.userPrio);
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_register_hal_ind_call_back
   \brief registers callback function to HAL for any indication.
   \param   tpAniSirGlobal pMac
   \return none.
   -------------------------------------------------------------*/
void lim_register_hal_ind_call_back(tpAniSirGlobal pMac)
{
	struct scheduler_msg msg = {0};
	tpHalIndCB pHalCB;

	pHalCB = qdf_mem_malloc(sizeof(tHalIndCB));
	if (NULL == pHalCB) {
		pe_err("AllocateMemory() failed");
		return;
	}

	pHalCB->pHalIndCB = lim_process_hal_ind_messages;

	msg.type = WMA_REGISTER_PE_CALLBACK;
	msg.bodyptr = pHalCB;
	msg.bodyval = 0;

	MTRACE(mac_trace_msg_tx(pMac, NO_SESSION, msg.type));
	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(pMac, &msg)) {
		qdf_mem_free(pHalCB);
		pe_err("wma_post_ctrl_msg() failed");
	}

	return;
}

/**
 * lim_process_del_ts_ind() - handle del_ts_ind from HAL
 *
 * @mac_ctx: pointer to Global Mac Structure
 * @lim_msg: pointer to msg buff
 *
 * handles the DeleteTS indication coming from HAL or generated by PE itself
 * in some error cases. Validates the request, sends the DelTs action frame
 * to the Peer and sends DelTs indicatoin to HDD.
 *
 * Return: none
 */
void lim_process_del_ts_ind(tpAniSirGlobal pMac, struct scheduler_msg *limMsg)
{
	tpDphHashNode pSta;
	tpDelTsParams pDelTsParam = (tpDelTsParams) (limMsg->bodyptr);
	tpSirDeltsReq pDelTsReq = NULL;
	tSirMacAddr peerMacAddr;
	tpSirDeltsReqInfo pDelTsReqInfo;
	tpLimTspecInfo pTspecInfo;
	tpPESession psessionEntry;
	uint8_t sessionId;

	psessionEntry = pe_find_session_by_bssid(pMac, pDelTsParam->bssId,
			&sessionId);
	if (psessionEntry == NULL) {
		pe_err("session does not exist for given BssId");
		qdf_mem_free(limMsg->bodyptr);
		limMsg->bodyptr = NULL;
		return;
	}

	pTspecInfo = &(pMac->lim.tspecInfo[pDelTsParam->tspecIdx]);
	if (pTspecInfo->inuse == false) {
		pe_err("tspec entry with index: %d is not in use",
				pDelTsParam->tspecIdx);
		goto error1;
	}

	pSta =
		dph_get_hash_entry(pMac, pTspecInfo->assocId,
				   &psessionEntry->dph.dphHashTable);
	if (pSta == NULL) {
		pe_err("Could not find entry in DPH table for assocId: %d",
			pTspecInfo->assocId);
		goto error1;
	}

	pDelTsReq = qdf_mem_malloc(sizeof(tSirDeltsReq));
	if (NULL == pDelTsReq) {
		pe_err("AllocateMemory() failed");
		goto error1;
	}

	if (pSta->wmeEnabled)
		qdf_mem_copy(&(pDelTsReq->req.tspec), &(pTspecInfo->tspec),
			     sizeof(tSirMacTspecIE));
	else
		qdf_mem_copy(&(pDelTsReq->req.tsinfo),
			     &(pTspecInfo->tspec.tsinfo),
			     sizeof(tSirMacTSInfo));

	/* validate the req */
	if (QDF_STATUS_SUCCESS !=
	    lim_validate_delts_req(pMac, pDelTsReq, peerMacAddr, psessionEntry)) {
		pe_err("lim_validate_delts_req failed");
		goto error2;
	}
	pe_debug("Sent DELTS request to station with "
		       "assocId = %d MacAddr = " MAC_ADDRESS_STR,
		       pDelTsReq->aid, MAC_ADDR_ARRAY(peerMacAddr));

	lim_send_delts_req_action_frame(pMac, peerMacAddr,
					pDelTsReq->req.wmeTspecPresent,
					&pDelTsReq->req.tsinfo,
					&pDelTsReq->req.tspec, psessionEntry);

	/* prepare and send an sme indication to HDD */
	pDelTsReqInfo = qdf_mem_malloc(sizeof(tSirDeltsReqInfo));
	if (NULL == pDelTsReqInfo) {
		pe_err("AllocateMemory() failed");
		goto error3;
	}

	if (pSta->wmeEnabled)
		qdf_mem_copy(&(pDelTsReqInfo->tspec), &(pTspecInfo->tspec),
			     sizeof(tSirMacTspecIE));
	else
		qdf_mem_copy(&(pDelTsReqInfo->tsinfo),
			     &(pTspecInfo->tspec.tsinfo),
			     sizeof(tSirMacTSInfo));

	lim_send_sme_delts_ind(pMac, pDelTsReqInfo, pDelTsReq->aid, psessionEntry);

error3:
	qdf_mem_free(pDelTsReqInfo);
error2:
	qdf_mem_free(pDelTsReq);
error1:
	qdf_mem_free(limMsg->bodyptr);
	limMsg->bodyptr = NULL;
	return;
}

/**
 * @function :  lim_post_sm_state_update()
 *
 * @brief  :  This function Updates the HAL and Softmac about the change in the STA's SMPS state.
 *
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  limMsg - Lim Message structure object with the MimoPSparam in body
 * @return None
 */
QDF_STATUS
lim_post_sm_state_update(tpAniSirGlobal pMac,
			 uint16_t staIdx, tSirMacHTMIMOPowerSaveState state,
			 uint8_t *pPeerStaMac, uint8_t sessionId)
{
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};
	tpSetMIMOPS pMIMO_PSParams;

	msgQ.reserved = 0;
	msgQ.type = WMA_SET_MIMOPS_REQ;

	/* Allocate for WMA_SET_MIMOPS_REQ */
	pMIMO_PSParams = qdf_mem_malloc(sizeof(tSetMIMOPS));
	if (NULL == pMIMO_PSParams) {
		pe_err(" AllocateMemory failed");
		return QDF_STATUS_E_NOMEM;
	}

	pMIMO_PSParams->htMIMOPSState = state;
	pMIMO_PSParams->staIdx = staIdx;
	pMIMO_PSParams->fsendRsp = true;
	pMIMO_PSParams->sessionId = sessionId;
	qdf_mem_copy(pMIMO_PSParams->peerMac, pPeerStaMac, sizeof(tSirMacAddr));

	msgQ.bodyptr = pMIMO_PSParams;
	msgQ.bodyval = 0;

	pe_debug("Sending WMA_SET_MIMOPS_REQ");

	MTRACE(mac_trace_msg_tx(pMac, NO_SESSION, msgQ.type));
	retCode = wma_post_ctrl_msg(pMac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		pe_err("Posting WMA_SET_MIMOPS_REQ to HAL failed! Reason: %d",
			retCode);
		qdf_mem_free(pMIMO_PSParams);
		return retCode;
	}

	return retCode;
}

void lim_pkt_free(tpAniSirGlobal pMac,
		  eFrameType frmType, uint8_t *pRxPacketInfo, void *pBody)
{
	(void)pMac;
	(void)frmType;
	(void)pRxPacketInfo;
	(void)pBody;
}

/**
 * lim_get_b_dfrom_rx_packet()
 *
 ***FUNCTION:
 * This function is called to get pointer to Polaris
 * Buffer Descriptor containing MAC header & other control
 * info from the body of the message posted to LIM.
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
 * @param  body    - Received message body
 * @param  pRxPacketInfo     - Pointer to received BD
 * @return None
 */

void
lim_get_b_dfrom_rx_packet(tpAniSirGlobal pMac, void *body, uint32_t **pRxPacketInfo)
{
	*pRxPacketInfo = (uint32_t *) body;
} /*** end lim_get_b_dfrom_rx_packet() ***/

void lim_add_channel_status_info(tpAniSirGlobal p_mac,
				 struct lim_channel_status *channel_stat,
				 uint8_t channel_id)
{
	uint8_t i;
	bool found = false;
	struct lim_scan_channel_status *channel_info =
		&p_mac->lim.scan_channel_status;
	struct lim_channel_status *channel_status_list =
		channel_info->channel_status_list;
	uint8_t total_channel = channel_info->total_channel;

	if (!p_mac->sap.acs_with_more_param)
		return;

	for (i = 0; i < total_channel; i++) {
		if (channel_status_list[i].channel_id == channel_id) {
			if (channel_stat->cmd_flags ==
			    WMI_CHAN_InFO_END_RESP &&
			    channel_status_list[i].cmd_flags ==
			    WMI_CHAN_InFO_START_RESP) {
				/* adjust to delta value for counts */
				channel_stat->rx_clear_count -=
				    channel_status_list[i].rx_clear_count;
				channel_stat->cycle_count -=
				    channel_status_list[i].cycle_count;
				channel_stat->rx_frame_count -=
				    channel_status_list[i].rx_frame_count;
				channel_stat->tx_frame_count -=
				    channel_status_list[i].tx_frame_count;
				channel_stat->bss_rx_cycle_count -=
				    channel_status_list[i].bss_rx_cycle_count;
			}
			qdf_mem_copy(&channel_status_list[i], channel_stat,
				     sizeof(*channel_status_list));
			found = true;
			break;
		}
	}

	if (!found) {
		if (total_channel < SIR_MAX_SUPPORTED_CHANNEL_LIST) {
			qdf_mem_copy(&channel_status_list[total_channel++],
				     channel_stat,
				     sizeof(*channel_status_list));
			channel_info->total_channel = total_channel;
		} else {
			pe_warn("Chan cnt exceed, channel_id=%d", channel_id);
		}
	}

	return;
}

/**
 * @function :  lim_is_channel_valid_for_channel_switch()
 *
 * @brief  :  This function checks if the channel to which AP
 *            is expecting us to switch, is a valid channel for us.
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  channel - New channel to which we are expected to move
 * @return None
 */
bool lim_is_channel_valid_for_channel_switch(tpAniSirGlobal pMac, uint8_t channel)
{
	uint8_t index;
	uint32_t validChannelListLen = WNI_CFG_VALID_CHANNEL_LIST_LEN;
	tSirMacChanNum validChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN];
	bool ok = false;

	if (policy_mgr_is_chan_ok_for_dnbs(pMac->psoc, channel, &ok)) {
		pe_err("policy_mgr_is_chan_ok_for_dnbs() returned error");
		return false;
	}

	if (!ok) {
		pe_debug("channel not ok for DNBS");
		return false;
	}

	if (wlan_cfg_get_str(pMac, WNI_CFG_VALID_CHANNEL_LIST,
			     (uint8_t *) validChannelList,
			     (uint32_t *) &validChannelListLen) !=
			QDF_STATUS_SUCCESS) {
		pe_err("could not retrieve valid channel list");
		return false;
	}

	for (index = 0; index < validChannelListLen; index++) {
		if (validChannelList[index] != channel)
			continue;

		ok = policy_mgr_is_valid_for_channel_switch(pMac->psoc,
							    channel);
		return ok;
	}

	/* channel does not belong to list of valid channels */
	return false;
}

/**------------------------------------------------------
   \fn     __lim_fill_tx_control_params
   \brief  Fill the message for stopping/resuming tx.

   \param  pMac
   \param  pTxCtrlMsg - Pointer to tx control message.
   \param  type - Which way we want to stop/ resume tx.
   \param  mode - To stop/resume.
   -------------------------------------------------------*/
static QDF_STATUS
__lim_fill_tx_control_params(tpAniSirGlobal pMac, tpTxControlParams pTxCtrlMsg,
			     tLimQuietTxMode type, tLimControlTx mode)
{

	tpPESession psessionEntry = &pMac->lim.gpSession[0];

	if (mode == eLIM_STOP_TX)
		pTxCtrlMsg->stopTx = true;
	else
		pTxCtrlMsg->stopTx = false;

	switch (type) {
	case eLIM_TX_ALL:
		/** Stops/resumes transmission completely */
		pTxCtrlMsg->fCtrlGlobal = 1;
		break;

	case eLIM_TX_BSS_BUT_BEACON:
		/** Stops/resumes transmission on a particular BSS. Stopping BSS, doesn't
		 *  stop beacon transmission.
		 */
		pTxCtrlMsg->ctrlBss = 1;
		pTxCtrlMsg->bssBitmap |= (1 << psessionEntry->bssIdx);
		break;

	case eLIM_TX_STA:
	/** Memory for station bitmap is allocated dynamically in caller of this
	 *  so decode properly here and fill the bitmap. Now not implemented,
	 *  fall through.
	 */
	case eLIM_TX_BSS:
	/* Fall thru... */
	default:
		pe_warn("Invalid case: Not Handled");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * @function :  lim_frame_transmission_control()
 *
 * @brief  :  This API is called by the user to halt/resume any frame
 *       transmission from the device. If stopped, all frames will be
 *            queued starting from hardware. Then back-pressure
 *            is built till the driver.
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */
void lim_frame_transmission_control(tpAniSirGlobal pMac, tLimQuietTxMode type,
				    tLimControlTx mode)
{

	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tpTxControlParams pTxCtrlMsg;
	struct scheduler_msg msgQ = {0};
	uint8_t nBytes = 0; /* No of bytes required for station bitmap. */

	/** Allocate only required number of bytes for station bitmap
	 * Make it to align to 4 byte boundary  */
	nBytes = (uint8_t) HALMSG_NUMBYTES_STATION_BITMAP(pMac->lim.maxStation);

	pTxCtrlMsg = qdf_mem_malloc(sizeof(*pTxCtrlMsg) + nBytes);
	if (NULL == pTxCtrlMsg) {
		pe_err("AllocateMemory() failed");
		return;
	}

	status = __lim_fill_tx_control_params(pMac, pTxCtrlMsg, type, mode);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mem_free(pTxCtrlMsg);
		pe_err("__lim_fill_tx_control_params failed, status: %d",
			status);
		return;
	}

	msgQ.bodyptr = (void *)pTxCtrlMsg;
	msgQ.bodyval = 0;
	msgQ.reserved = 0;
	msgQ.type = WMA_TRANSMISSION_CONTROL_IND;

	MTRACE(mac_trace_msg_tx(pMac, NO_SESSION, msgQ.type));
	if (wma_post_ctrl_msg(pMac, &msgQ) != QDF_STATUS_SUCCESS) {
		qdf_mem_free(pTxCtrlMsg);
		pe_err("Posting Message to HAL failed");
		return;
	}

	pe_debug("Stopping the transmission of all packets, indicated softmac tx_control: %d",
		mode);

	return;
}

/**
 * @function :  lim_restore_pre_channel_switch_state()
 *
 * @brief  :  This API is called by the user to undo any
 *            specific changes done on the device during
 *            channel switch.
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

QDF_STATUS
lim_restore_pre_channel_switch_state(tpAniSirGlobal pMac, tpPESession psessionEntry)
{

	QDF_STATUS retCode = QDF_STATUS_SUCCESS;

	if (!LIM_IS_STA_ROLE(psessionEntry))
		return retCode;

	/* Channel switch should be ready for the next time */
	psessionEntry->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_INIT;

	/* Restore the frame transmission, all the time. */
	lim_frame_transmission_control(pMac, eLIM_TX_ALL, eLIM_RESUME_TX);

	return retCode;
}

/**--------------------------------------------
   \fn       lim_restore_pre_quiet_state
   \brief   Restore the pre quiet state

   \param pMac
   \return NONE
   ---------------------------------------------*/
QDF_STATUS lim_restore_pre_quiet_state(tpAniSirGlobal pMac,
					  tpPESession psessionEntry)
{

	QDF_STATUS retCode = QDF_STATUS_SUCCESS;

	if (pMac->lim.gLimSystemRole != eLIM_STA_ROLE)
		return retCode;

	/* Quiet should be ready for the next time */
	psessionEntry->gLimSpecMgmt.quietState = eLIM_QUIET_INIT;

	/* Restore the frame transmission, all the time. */
	if (psessionEntry->gLimSpecMgmt.quietState == eLIM_QUIET_RUNNING)
		lim_frame_transmission_control(pMac, eLIM_TX_ALL, eLIM_RESUME_TX);

	return retCode;
}

/**
 * @function: lim_prepare_for11h_channel_switch()
 *
 * @brief  :  This API is called by the user to prepare for
 *            11h channel switch. As of now, the API does
 *            very minimal work. User can add more into the
 *            same API if needed.
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  psessionEntry
 * @return None
 */
void
lim_prepare_for11h_channel_switch(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
	if (!LIM_IS_STA_ROLE(psessionEntry))
		return;

	/* Flag to indicate 11h channel switch in progress */
	psessionEntry->gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_RUNNING;

	if (pMac->lim.gLimSmeState == eLIM_SME_LINK_EST_WT_SCAN_STATE ||
	    pMac->lim.gLimSmeState == eLIM_SME_CHANNEL_SCAN_STATE) {
		pe_debug("Posting finish scan as we are in scan state");
		/* Stop ongoing scanning if any */
		if (GET_LIM_PROCESS_DEFD_MESGS(pMac)) {
			/* Set the resume channel to Any valid channel (invalid). */
			/* This will instruct HAL to set it to any previous valid channel. */
			pe_set_resume_channel(pMac, 0, 0);
		} else {
			lim_restore_pre_channel_switch_state(pMac, psessionEntry);
		}
		return;
	} else {
		/** We are safe to switch channel at this point */
		lim_stop_tx_and_switch_channel(pMac, psessionEntry->peSessionId);
	}
}

/**----------------------------------------------------
   \fn        lim_get_nw_type

   \brief    Get type of the network from data packet or beacon
   \param pMac
   \param channelNum - Channel number
   \param type - Type of packet.
   \param pBeacon - Pointer to beacon or probe response

   \return Network type a/b/g.
   -----------------------------------------------------*/
tSirNwType lim_get_nw_type(tpAniSirGlobal pMac, uint8_t channelNum, uint32_t type,
			   tpSchBeaconStruct pBeacon)
{
	tSirNwType nwType = eSIR_11B_NW_TYPE;

	if (type == SIR_MAC_DATA_FRAME) {
		if ((channelNum > 0) && (channelNum < 15)) {
			nwType = eSIR_11G_NW_TYPE;
		} else {
			nwType = eSIR_11A_NW_TYPE;
		}
	} else {
		if ((channelNum > 0) && (channelNum < 15)) {
			int i;
			/* 11b or 11g packet */
			/* 11g iff extended Rate IE is present or */
			/* if there is an A rate in suppRate IE */
			for (i = 0; i < pBeacon->supportedRates.numRates; i++) {
				if (sirIsArate
					    (pBeacon->supportedRates.rate[i] & 0x7f)) {
					nwType = eSIR_11G_NW_TYPE;
					break;
				}
			}
			if (pBeacon->extendedRatesPresent) {
				nwType = eSIR_11G_NW_TYPE;
			} else if (pBeacon->HTInfo.present ||
				   IS_BSS_VHT_CAPABLE(pBeacon->VHTCaps)) {
				nwType = eSIR_11G_NW_TYPE;
			}
		} else {
			/* 11a packet */
			nwType = eSIR_11A_NW_TYPE;
		}
	}
	return nwType;
}

/**---------------------------------------------------------
   \fn        lim_get_channel_from_beacon
   \brief    To extract channel number from beacon

   \param pMac
   \param pBeacon - Pointer to beacon or probe rsp
   \return channel number
   -----------------------------------------------------------*/
uint8_t lim_get_channel_from_beacon(tpAniSirGlobal pMac, tpSchBeaconStruct pBeacon)
{
	uint8_t channelNum = 0;

	if (pBeacon->dsParamsPresent)
		channelNum = pBeacon->channelNumber;
	else if (pBeacon->HTInfo.present)
		channelNum = pBeacon->HTInfo.primaryChannel;
	else
		channelNum = pBeacon->channelNumber;

	return channelNum;
}

void lim_set_tspec_uapsd_mask_per_session(tpAniSirGlobal pMac,
					  tpPESession psessionEntry,
					  tSirMacTSInfo *pTsInfo, uint32_t action)
{
	uint8_t userPrio = (uint8_t) pTsInfo->traffic.userPrio;
	uint16_t direction = pTsInfo->traffic.direction;
	uint8_t ac = upToAc(userPrio);

	pe_debug("Set UAPSD mask for AC: %d dir: %d action: %d"
			, ac, direction, action);

	/* Converting AC to appropriate Uapsd Bit Mask
	 * AC_BE(0) --> UAPSD_BITOFFSET_ACVO(3)
	 * AC_BK(1) --> UAPSD_BITOFFSET_ACVO(2)
	 * AC_VI(2) --> UAPSD_BITOFFSET_ACVO(1)
	 * AC_VO(3) --> UAPSD_BITOFFSET_ACVO(0)
	 */
	ac = ((~ac) & 0x3);

	if (action == CLEAR_UAPSD_MASK) {
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			psessionEntry->gUapsdPerAcTriggerEnableMask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			psessionEntry->gUapsdPerAcDeliveryEnableMask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			psessionEntry->gUapsdPerAcTriggerEnableMask &=
				~(1 << ac);
			psessionEntry->gUapsdPerAcDeliveryEnableMask &=
				~(1 << ac);
		}
	} else if (action == SET_UAPSD_MASK) {
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			psessionEntry->gUapsdPerAcTriggerEnableMask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			psessionEntry->gUapsdPerAcDeliveryEnableMask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			psessionEntry->gUapsdPerAcTriggerEnableMask |=
				(1 << ac);
			psessionEntry->gUapsdPerAcDeliveryEnableMask |=
				(1 << ac);
		}
	}

	pe_debug("New psessionEntry->gUapsdPerAcTriggerEnableMask 0x%x psessionEntry->gUapsdPerAcDeliveryEnableMask 0x%x",
		psessionEntry->gUapsdPerAcTriggerEnableMask,
		psessionEntry->gUapsdPerAcDeliveryEnableMask);

	return;
}

/**
 * lim_handle_heart_beat_timeout_for_session() - Handle heart beat time out
 * @mac_ctx: pointer to Global Mac Structure
 * @psession_entry: pointer to tpPESession
 *
 * Function handles heart beat time out for session
 *
 * Return: none
 */
void lim_handle_heart_beat_timeout_for_session(tpAniSirGlobal mac_ctx,
					       tpPESession psession_entry)
{
	if (psession_entry->valid == true) {
		if (psession_entry->bssType == eSIR_IBSS_MODE)
			lim_ibss_heart_beat_handle(mac_ctx, psession_entry);

		if ((psession_entry->bssType == eSIR_INFRASTRUCTURE_MODE) &&
					(LIM_IS_STA_ROLE(psession_entry)))
			lim_handle_heart_beat_failure(mac_ctx, psession_entry);
	}
	/*
	 * In the function lim_handle_heart_beat_failure things can change
	 * so check for the session entry  valid and the other things
	 * again
	 */
	if ((psession_entry->valid == true) &&
		(psession_entry->bssType == eSIR_INFRASTRUCTURE_MODE) &&
			(LIM_IS_STA_ROLE(psession_entry)) &&
				(psession_entry->LimHBFailureStatus == true)) {
		tLimTimers *lim_timer  = &mac_ctx->lim.limTimers;
		/*
		 * Activate Probe After HeartBeat Timer incase HB
		 * Failure detected
		 */
		pe_debug("Sending Probe for Session: %d",
			psession_entry->bssIdx);
		lim_deactivate_and_change_timer(mac_ctx,
			eLIM_PROBE_AFTER_HB_TIMER);
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_TIMER_ACTIVATE, 0,
			eLIM_PROBE_AFTER_HB_TIMER));
		if (tx_timer_activate(&lim_timer->gLimProbeAfterHBTimer)
					!= TX_SUCCESS)
			pe_err("Fail to re-activate Probe-after-hb timer");
	}
}

uint8_t lim_get_current_operating_channel(tpAniSirGlobal pMac)
{
	uint8_t i;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		if (pMac->lim.gpSession[i].valid == true) {
			if ((pMac->lim.gpSession[i].bssType ==
			     eSIR_INFRASTRUCTURE_MODE)
			    && (pMac->lim.gpSession[i].limSystemRole ==
				eLIM_STA_ROLE)) {
				return pMac->lim.gpSession[i].
				       currentOperChannel;
			}
		}
	}
	return 0;
}

/**
 * lim_process_add_sta_rsp() - process WDA_ADD_STA_RSP from WMA
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: msg from WMA
 *
 * @Return: void
 */
void lim_process_add_sta_rsp(tpAniSirGlobal mac_ctx, struct scheduler_msg *msg)
{
	tpPESession session;
	tpAddStaParams add_sta_params;

	add_sta_params = (tpAddStaParams) msg->bodyptr;

	session = pe_find_session_by_session_id(mac_ctx,
			add_sta_params->sessionId);
	if (session == NULL) {
		pe_err("Session Does not exist for given sessionID");
		qdf_mem_free(add_sta_params);
		return;
	}
	session->csaOffloadEnable = add_sta_params->csaOffloadEnable;
	if (LIM_IS_IBSS_ROLE(session))
		(void)lim_ibss_add_sta_rsp(mac_ctx, msg->bodyptr, session);
	else if (LIM_IS_NDI_ROLE(session))
		lim_ndp_add_sta_rsp(mac_ctx, session, msg->bodyptr);
#ifdef FEATURE_WLAN_TDLS
	else if (add_sta_params->staType == STA_ENTRY_TDLS_PEER)
		lim_process_tdls_add_sta_rsp(mac_ctx, msg->bodyptr, session);
#endif
	else
		lim_process_mlm_add_sta_rsp(mac_ctx, msg, session);

}

/**
 * lim_update_beacon() - This function updates beacon
 * @mac_ctx: pointer to Global Mac Structure
 *
 * This Function is invoked to update the beacon
 *
 * Return: none
 */
void lim_update_beacon(tpAniSirGlobal mac_ctx)
{
	uint8_t i;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if (mac_ctx->lim.gpSession[i].valid != true)
			continue;
		if (((mac_ctx->lim.gpSession[i].limSystemRole == eLIM_AP_ROLE)
			|| (mac_ctx->lim.gpSession[i].limSystemRole ==
					eLIM_STA_IN_IBSS_ROLE))
			&& (eLIM_SME_NORMAL_STATE ==
				mac_ctx->lim.gpSession[i].limSmeState)) {

			sch_set_fixed_beacon_fields(mac_ctx,
						&mac_ctx->lim.gpSession[i]);

			if (false == mac_ctx->sap.SapDfsInfo.
					is_dfs_cac_timer_running)
				lim_send_beacon_ind(mac_ctx,
						&mac_ctx->lim.gpSession[i],
						REASON_DEFAULT);
		}
	}
}

/**
 * lim_handle_heart_beat_failure_timeout - handle heart beat failure
 * @mac_ctx: pointer to Global Mac Structure
 *
 * Function handle heart beat failure timeout
 *
 * Return: none
 */
void lim_handle_heart_beat_failure_timeout(tpAniSirGlobal mac_ctx)
{
	uint8_t i;
	tpPESession psession_entry;
	/*
	 * Probe response is not received  after HB failure.
	 * This is handled by LMM sub module.
	 */
	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if (mac_ctx->lim.gpSession[i].valid != true)
			continue;
		psession_entry = &mac_ctx->lim.gpSession[i];
		if (psession_entry->LimHBFailureStatus != true)
			continue;
		pe_debug("SME: %d MLME: %d HB-Count: %d",
				psession_entry->limSmeState,
				psession_entry->limMlmState,
				psession_entry->LimRxedBeaconCntDuringHB);

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_HB_FAILURE_TIMEOUT,
					psession_entry, 0, 0);
#endif
		if ((psession_entry->limMlmState ==
					eLIM_MLM_LINK_ESTABLISHED_STATE) &&
			(psession_entry->limSmeState !=
					eLIM_SME_WT_DISASSOC_STATE) &&
			(psession_entry->limSmeState !=
					eLIM_SME_WT_DEAUTH_STATE) &&
			((!LIM_IS_CONNECTION_ACTIVE(psession_entry)) ||
			/*
			 * Disconnect even if we have not received a single
			 * beacon after connection.
			 */
			 (psession_entry->currentBssBeaconCnt == 0))) {
			pe_nofl_info("HB fail session %d",
				     psession_entry->peSessionId);
			lim_send_deauth_mgmt_frame(mac_ctx,
				eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON,
				psession_entry->bssId, psession_entry, false);

			/*
			 * AP did not respond to Probe Request.
			 * Tear down link with it.
			 */
			lim_tear_down_link_with_ap(mac_ctx,
						psession_entry->peSessionId,
						eSIR_MAC_BEACON_MISSED,
						eLIM_LINK_MONITORING_DISASSOC);
			mac_ctx->lim.gLimProbeFailureAfterHBfailedCnt++;
		} else {
			pe_err("Unexpected wt-probe-timeout in state");
			lim_print_mlm_state(mac_ctx, LOGE,
				psession_entry->limMlmState);
			if (mac_ctx->sme.tx_queue_cb)
				mac_ctx->sme.tx_queue_cb(mac_ctx->hdd_handle,
						psession_entry->smeSessionId,
						WLAN_WAKE_ALL_NETIF_QUEUE,
						WLAN_CONTROL_PATH);
		}
	}
	/*
	 * Deactivate Timer ProbeAfterHB Timer -> As its a oneshot timer,
	 * need not deactivate the timer
	 * tx_timer_deactivate(&pMac->lim.limTimers.gLimProbeAfterHBTimer);
	 */
}

/*
 * This function assumes there will not be more than one IBSS session active at any time.
 */
tpPESession lim_is_ibss_session_active(tpAniSirGlobal pMac)
{
	uint8_t i;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		if ((pMac->lim.gpSession[i].valid) &&
		    (pMac->lim.gpSession[i].limSystemRole ==
		     eLIM_STA_IN_IBSS_ROLE))
			return &pMac->lim.gpSession[i];
	}

	return NULL;
}

tpPESession lim_is_ap_session_active(tpAniSirGlobal pMac)
{
	uint8_t i;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		if (pMac->lim.gpSession[i].valid &&
		    (pMac->lim.gpSession[i].limSystemRole == eLIM_AP_ROLE))
			return &pMac->lim.gpSession[i];
	}

	return NULL;
}

/**---------------------------------------------------------
   \fn        lim_handle_defer_msg_error
   \brief    handles error scenario, when the msg can not be deferred.
   \param pMac
   \param pLimMsg LIM msg, which could not be deferred.
   \return void
   -----------------------------------------------------------*/

void lim_handle_defer_msg_error(tpAniSirGlobal pMac,
				struct scheduler_msg *pLimMsg)
{
	if (SIR_BB_XPORT_MGMT_MSG == pLimMsg->type) {
		lim_decrement_pending_mgmt_count(pMac);
		cds_pkt_return_packet((cds_pkt_t *) pLimMsg->bodyptr);
		pLimMsg->bodyptr = NULL;
	} else if (pLimMsg->bodyptr != NULL) {
		qdf_mem_free(pLimMsg->bodyptr);
		pLimMsg->bodyptr = NULL;
	}

}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**---------------------------------------------------------
   \fn    lim_diag_event_report
   \brief This function reports Diag event
   \param pMac
   \param eventType
   \param bssid
   \param status
   \param reasonCode
   \return void
   -----------------------------------------------------------*/
void lim_diag_event_report(tpAniSirGlobal pMac, uint16_t eventType,
			   tpPESession pSessionEntry, uint16_t status,
			   uint16_t reasonCode)
{
	tSirMacAddr nullBssid = { 0, 0, 0, 0, 0, 0 };

	WLAN_HOST_DIAG_EVENT_DEF(peEvent, host_event_wlan_pe_payload_type);

	qdf_mem_zero(&peEvent, sizeof(host_event_wlan_pe_payload_type));

	if (NULL == pSessionEntry) {
		qdf_mem_copy(peEvent.bssid, nullBssid, sizeof(tSirMacAddr));
		peEvent.sme_state = (uint16_t) pMac->lim.gLimSmeState;
		peEvent.mlm_state = (uint16_t) pMac->lim.gLimMlmState;

	} else {
		qdf_mem_copy(peEvent.bssid, pSessionEntry->bssId,
			     sizeof(tSirMacAddr));
		peEvent.sme_state = (uint16_t) pSessionEntry->limSmeState;
		peEvent.mlm_state = (uint16_t) pSessionEntry->limMlmState;
	}
	peEvent.event_type = eventType;
	peEvent.status = status;
	peEvent.reason_code = reasonCode;

	WLAN_HOST_DIAG_EVENT_REPORT(&peEvent, EVENT_WLAN_PE);
	return;
}

static void lim_diag_fill_mgmt_event_report(tpAniSirGlobal mac_ctx,
			tpSirMacMgmtHdr mac_hdr,
			tpPESession session, uint16_t result_code,
			uint16_t reason_code,
			struct host_event_wlan_mgmt_payload_type *mgmt_event)
{
	uint8_t length;

	qdf_mem_zero(mgmt_event, sizeof(*mgmt_event));
	mgmt_event->mgmt_type = mac_hdr->fc.type;
	mgmt_event->mgmt_subtype = mac_hdr->fc.subType;
	qdf_mem_copy(mgmt_event->self_mac_addr, session->selfMacAddr,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(mgmt_event->bssid, session->bssId,
		     QDF_MAC_ADDR_SIZE);
	length = session->ssId.length;
	if (length > SIR_MAC_MAX_SSID_LENGTH)
		length = SIR_MAC_MAX_SSID_LENGTH;
	qdf_mem_copy(mgmt_event->ssid, session->ssId.ssId, length);
	mgmt_event->ssid_len = length;
	mgmt_event->operating_channel = session->currentOperChannel;
	mgmt_event->result_code = result_code;
	mgmt_event->reason_code = reason_code;
}

void lim_diag_mgmt_tx_event_report(tpAniSirGlobal mac_ctx, void *mgmt_hdr,
				   tpPESession session, uint16_t result_code,
				   uint16_t reason_code)
{
	tpSirMacMgmtHdr mac_hdr = mgmt_hdr;

	WLAN_HOST_DIAG_EVENT_DEF(mgmt_event,
				 struct host_event_wlan_mgmt_payload_type);
	if (!session || !mac_hdr) {
		pe_err("not valid input");
		return;
	}
	lim_diag_fill_mgmt_event_report(mac_ctx, mac_hdr, session,
					result_code, reason_code, &mgmt_event);

	pe_debug("TX frame: type:%d sub_type:%d seq_num:%d ssid:%.*s selfmacaddr:%pM bssid:%pM channel:%d",
		 mgmt_event.mgmt_type, mgmt_event.mgmt_subtype,
		 ((mac_hdr->seqControl.seqNumHi << 4) |
				mac_hdr->seqControl.seqNumLo),
		 mgmt_event.ssid_len, mgmt_event.ssid,
		 mgmt_event.self_mac_addr, mgmt_event.bssid,
		 mgmt_event.operating_channel);
	WLAN_HOST_DIAG_EVENT_REPORT(&mgmt_event, EVENT_WLAN_HOST_MGMT_TX_V2);
}

void lim_diag_mgmt_rx_event_report(tpAniSirGlobal mac_ctx, void *mgmt_hdr,
				   tpPESession session, uint16_t result_code,
				   uint16_t reason_code)
{
	tpSirMacMgmtHdr mac_hdr = mgmt_hdr;

	WLAN_HOST_DIAG_EVENT_DEF(mgmt_event,
				 struct host_event_wlan_mgmt_payload_type);
	if (!session || !mac_hdr) {
		pe_debug("not valid input");
		return;
	}
	lim_diag_fill_mgmt_event_report(mac_ctx, mac_hdr, session,
					result_code, reason_code, &mgmt_event);
	pe_debug("RX frame: type:%d sub_type:%d seq_num:%d ssid:%.*s selfmacaddr:%pM bssid:%pM channel:%d",
		 mgmt_event.mgmt_type, mgmt_event.mgmt_subtype,
		 ((mac_hdr->seqControl.seqNumHi << 4) |
				mac_hdr->seqControl.seqNumLo),
		 mgmt_event.ssid_len, mgmt_event.ssid,
		 mgmt_event.self_mac_addr, mgmt_event.bssid,
		 mgmt_event.operating_channel);
	WLAN_HOST_DIAG_EVENT_REPORT(&mgmt_event, EVENT_WLAN_HOST_MGMT_RX_V2);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

/* Returns length of P2P stream and Pointer ie passed to this function is filled with noa stream */

uint8_t lim_build_p2p_ie(tpAniSirGlobal pMac, uint8_t *ie, uint8_t *data,
			 uint8_t ie_len)
{
	int length = 0;
	uint8_t *ptr = ie;

	ptr[length++] = SIR_MAC_EID_VENDOR;
	ptr[length++] = ie_len + SIR_MAC_P2P_OUI_SIZE;
	qdf_mem_copy(&ptr[length], SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE);
	qdf_mem_copy(&ptr[length + SIR_MAC_P2P_OUI_SIZE], data, ie_len);
	return ie_len + SIR_P2P_IE_HEADER_LEN;
}

/* Returns length of NoA stream and Pointer pNoaStream passed to this function is filled with noa stream */

uint8_t lim_get_noa_attr_stream_in_mult_p2p_ies(tpAniSirGlobal pMac,
						uint8_t *noaStream, uint8_t noaLen,
						uint8_t overFlowLen)
{
	uint8_t overFlowP2pStream[SIR_MAX_NOA_ATTR_LEN];

	if ((noaLen <= (SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN)) &&
	    (noaLen >= overFlowLen) && (overFlowLen <= SIR_MAX_NOA_ATTR_LEN)) {
		qdf_mem_copy(overFlowP2pStream,
			     noaStream + noaLen - overFlowLen, overFlowLen);
		noaStream[noaLen - overFlowLen] = SIR_MAC_EID_VENDOR;
		noaStream[noaLen - overFlowLen + 1] =
			overFlowLen + SIR_MAC_P2P_OUI_SIZE;
		qdf_mem_copy(noaStream + noaLen - overFlowLen + 2,
			     SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE);
		qdf_mem_copy(noaStream + noaLen + 2 + SIR_MAC_P2P_OUI_SIZE -
			     overFlowLen, overFlowP2pStream, overFlowLen);
	}

	return noaLen + SIR_P2P_IE_HEADER_LEN;

}

/* Returns length of NoA stream and Pointer pNoaStream passed to this function is filled with noa stream */
uint8_t lim_get_noa_attr_stream(tpAniSirGlobal pMac, uint8_t *pNoaStream,
				tpPESession psessionEntry)
{
	uint8_t len = 0;

	uint8_t *pBody = pNoaStream;

	if ((psessionEntry != NULL) && (psessionEntry->valid) &&
	    (psessionEntry->pePersona == QDF_P2P_GO_MODE)) {
		if ((!(psessionEntry->p2pGoPsUpdate.uNoa1Duration))
		    && (!(psessionEntry->p2pGoPsUpdate.uNoa2Duration))
		    && (!psessionEntry->p2pGoPsUpdate.oppPsFlag)
		    )
			return 0;  /* No NoA Descriptor then return 0 */

		pBody[0] = SIR_P2P_NOA_ATTR;

		pBody[3] = psessionEntry->p2pGoPsUpdate.index;
		pBody[4] =
			psessionEntry->p2pGoPsUpdate.ctWin | (psessionEntry->
							      p2pGoPsUpdate.
							      oppPsFlag << 7);
		len = 5;
		pBody += len;

		if (psessionEntry->p2pGoPsUpdate.uNoa1Duration) {
			*pBody = psessionEntry->p2pGoPsUpdate.uNoa1IntervalCnt;
			pBody += 1;
			len += 1;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(psessionEntry->p2pGoPsUpdate.
						      uNoa1Duration);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(psessionEntry->p2pGoPsUpdate.
						      uNoa1Interval);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(psessionEntry->p2pGoPsUpdate.
						      uNoa1StartTime);
			pBody += sizeof(uint32_t);
			len += 4;

		}

		if (psessionEntry->p2pGoPsUpdate.uNoa2Duration) {
			*pBody = psessionEntry->p2pGoPsUpdate.uNoa2IntervalCnt;
			pBody += 1;
			len += 1;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(psessionEntry->p2pGoPsUpdate.
						      uNoa2Duration);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(psessionEntry->p2pGoPsUpdate.
						      uNoa2Interval);
			pBody += sizeof(uint32_t);
			len += 4;

			*((uint32_t *) (pBody)) =
				sir_swap_u32if_needed(psessionEntry->p2pGoPsUpdate.
						      uNoa2StartTime);
			pBody += sizeof(uint32_t);
			len += 4;

		}

		pBody = pNoaStream + 1;
		*((uint16_t *) (pBody)) = sir_swap_u16if_needed(len - 3); /*one byte for Attr and 2 bytes for length */

		return len;

	}
	return 0;

}

void pe_set_resume_channel(tpAniSirGlobal pMac, uint16_t channel,
			   ePhyChanBondState phyCbState)
{

	pMac->lim.gResumeChannel = channel;
	pMac->lim.gResumePhyCbState = phyCbState;
}

bool lim_is_noa_insert_reqd(tpAniSirGlobal pMac)
{
	uint8_t i;

	for (i = 0; i < pMac->lim.maxBssId; i++) {
		if (pMac->lim.gpSession[i].valid == true) {
			if ((eLIM_AP_ROLE ==
			     pMac->lim.gpSession[i].limSystemRole)
			    && (QDF_P2P_GO_MODE ==
				pMac->lim.gpSession[i].pePersona)
			    ) {
				return true;
			}
		}
	}
	return false;
}

bool lim_isconnected_on_dfs_channel(tpAniSirGlobal mac_ctx,
		uint8_t currentChannel)
{
	if (CHANNEL_STATE_DFS ==
	    wlan_reg_get_channel_state(mac_ctx->pdev, currentChannel)) {
		return true;
	} else {
		return false;
	}
}

#ifdef WLAN_FEATURE_11W
void lim_pmf_sa_query_timer_handler(void *pMacGlobal, uint32_t param)
{
	tpAniSirGlobal pMac = (tpAniSirGlobal) pMacGlobal;
	tPmfSaQueryTimerId timerId;
	tpPESession psessionEntry;
	tpDphHashNode pSta;
	uint32_t maxRetries;

	pe_debug("SA Query timer fires");
	timerId.value = param;

	/* Check that SA Query is in progress */
	psessionEntry = pe_find_session_by_session_id(pMac,
			timerId.fields.sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session does not exist for given session ID: %d",
			timerId.fields.sessionId);
		return;
	}
	pSta = dph_get_hash_entry(pMac, timerId.fields.peerIdx,
			       &psessionEntry->dph.dphHashTable);
	if (pSta == NULL) {
		pe_err("Entry does not exist for given peer index: %d",
			timerId.fields.peerIdx);
		return;
	}
	if (DPH_SA_QUERY_IN_PROGRESS != pSta->pmfSaQueryState)
		return;

	/* Increment the retry count, check if reached maximum */
	if (wlan_cfg_get_int(pMac, WNI_CFG_PMF_SA_QUERY_MAX_RETRIES,
			     &maxRetries) != QDF_STATUS_SUCCESS) {
		pe_err("Could not retrieve PMF SA Query maximum retries value");
		pSta->pmfSaQueryState = DPH_SA_QUERY_NOT_IN_PROGRESS;
		return;
	}
	pSta->pmfSaQueryRetryCount++;
	if (pSta->pmfSaQueryRetryCount >= maxRetries) {
		pe_err("SA Query timed out,Deleting STA");
		lim_print_mac_addr(pMac, pSta->staAddr, LOGE);
		lim_send_disassoc_mgmt_frame(pMac,
			eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON,
			pSta->staAddr, psessionEntry, false);
		lim_trigger_sta_deletion(pMac, pSta, psessionEntry);
		pSta->pmfSaQueryState = DPH_SA_QUERY_TIMED_OUT;
		return;
	}
	/* Retry SA Query */
	lim_send_sa_query_request_frame(pMac,
					(uint8_t *) &(pSta->
						      pmfSaQueryCurrentTransId),
					pSta->staAddr, psessionEntry);
	pSta->pmfSaQueryCurrentTransId++;
	pe_debug("Starting SA Query retry: %d", pSta->pmfSaQueryRetryCount);
	if (tx_timer_activate(&pSta->pmfSaQueryTimer) != TX_SUCCESS) {
		pe_err("PMF SA Query timer activation failed!");
		pSta->pmfSaQueryState = DPH_SA_QUERY_NOT_IN_PROGRESS;
	}
}
#endif

bool lim_check_vht_op_mode_change(tpAniSirGlobal pMac, tpPESession psessionEntry,
				  uint8_t chanWidth, uint8_t staId,
				  uint8_t *peerMac)
{
	tUpdateVHTOpMode tempParam;

	tempParam.opMode = chanWidth;
	tempParam.staId = staId;
	tempParam.smesessionId = psessionEntry->smeSessionId;
	qdf_mem_copy(tempParam.peer_mac, peerMac, sizeof(tSirMacAddr));

	lim_send_mode_update(pMac, &tempParam, psessionEntry);

	return true;
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
bool lim_send_he_ie_update(tpAniSirGlobal mac_ctx, tpPESession pe_session)
{
	QDF_STATUS status;

	status = wma_update_he_ops_ie(cds_get_context(QDF_MODULE_ID_WMA),
				      pe_session->smeSessionId,
				      &pe_session->he_op);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	return true;
}
#endif

bool lim_set_nss_change(tpAniSirGlobal pMac, tpPESession psessionEntry,
			uint8_t rxNss, uint8_t staId, uint8_t *peerMac)
{
	tUpdateRxNss tempParam;

	if (!rxNss) {
		pe_err("Invalid rxNss value: %u", rxNss);
		cds_trigger_recovery(QDF_REASON_UNSPECIFIED);
	}

	tempParam.rxNss = rxNss;
	tempParam.staId = staId;
	tempParam.smesessionId = psessionEntry->smeSessionId;
	qdf_mem_copy(tempParam.peer_mac, peerMac, sizeof(tSirMacAddr));

	lim_send_rx_nss_update(pMac, &tempParam, psessionEntry);

	return true;
}

bool lim_check_membership_user_position(tpAniSirGlobal pMac,
					tpPESession psessionEntry,
					uint32_t membership, uint32_t userPosition,
					uint8_t staId)
{
	tUpdateMembership tempParamMembership;
	tUpdateUserPos tempParamUserPosition;

	tempParamMembership.membership = membership;
	tempParamMembership.staId = staId;
	tempParamMembership.smesessionId = psessionEntry->smeSessionId;
	qdf_mem_copy(tempParamMembership.peer_mac, psessionEntry->bssId,
		     sizeof(tSirMacAddr));

	lim_set_membership(pMac, &tempParamMembership, psessionEntry);

	tempParamUserPosition.userPos = userPosition;
	tempParamUserPosition.staId = staId;
	tempParamUserPosition.smesessionId = psessionEntry->smeSessionId;
	qdf_mem_copy(tempParamUserPosition.peer_mac, psessionEntry->bssId,
		     sizeof(tSirMacAddr));

	lim_set_user_pos(pMac, &tempParamUserPosition, psessionEntry);

	return true;
}

void lim_get_short_slot_from_phy_mode(tpAniSirGlobal pMac, tpPESession psessionEntry,
				      uint32_t phyMode, uint8_t *pShortSlotEnabled)
{
	uint8_t val = 0;

	/* only 2.4G band should have short slot enable, rest it should be default */
	if (phyMode == WNI_CFG_PHY_MODE_11G) {
		/* short slot is default in all other modes */
		if ((psessionEntry->pePersona == QDF_SAP_MODE) ||
		    (psessionEntry->pePersona == QDF_IBSS_MODE) ||
		    (psessionEntry->pePersona == QDF_P2P_GO_MODE)) {
			val = true;
		}
		/* Program Polaris based on AP capability */
		if (psessionEntry->limMlmState == eLIM_MLM_WT_JOIN_BEACON_STATE) {
			/* Joining BSS. */
			val =
				SIR_MAC_GET_SHORT_SLOT_TIME(psessionEntry->
							    limCurrentBssCaps);
		} else if (psessionEntry->limMlmState ==
			   eLIM_MLM_WT_REASSOC_RSP_STATE) {
			/* Reassociating with AP. */
			val =
				SIR_MAC_GET_SHORT_SLOT_TIME(psessionEntry->
							    limReassocBssCaps);
		}
	} else {
		/*
		 * 11B does not short slot and short slot is default
		 * for 11A mode. Hence, not need to set this bit
		 */
		val = false;
	}

	pe_debug("phyMode: %u shortslotsupported: %u", phyMode, val);
	*pShortSlotEnabled = val;
}

#ifdef WLAN_FEATURE_11W
/**
 *
 * \brief This function is called by various LIM modules to correctly set
 * the Protected bit in the Frame Control Field of the 802.11 frame MAC header
 *
 *
 * \param  pMac Pointer to Global MAC structure
 *
 * \param psessionEntry Pointer to session corresponding to the connection
 *
 * \param peer Peer address of the STA to which the frame is to be sent
 *
 * \param pMacHdr Pointer to the frame MAC header
 *
 * \return nothing
 *
 *
 */
void
lim_set_protected_bit(tpAniSirGlobal pMac,
		      tpPESession psessionEntry,
		      tSirMacAddr peer, tpSirMacMgmtHdr pMacHdr)
{
	uint16_t aid;
	tpDphHashNode pStaDs;

	if (LIM_IS_AP_ROLE(psessionEntry)) {

		pStaDs = dph_lookup_hash_entry(pMac, peer, &aid,
					       &psessionEntry->dph.dphHashTable);
		if (pStaDs != NULL) {
			/* rmfenabled will be set at the time of addbss.
			 * but sometimes EAP auth fails and keys are not
			 * installed then if we send any management frame
			 * like deauth/disassoc with this bit set then
			 * firmware crashes. so check for keys are
			 * installed or not also before setting the bit
			 */
			if (pStaDs->rmfEnabled && pStaDs->is_key_installed)
				pMacHdr->fc.wep = 1;
		}
	} else if (psessionEntry->limRmfEnabled &&
			psessionEntry->is_key_installed) {
		pMacHdr->fc.wep = 1;
	}
} /*** end lim_set_protected_bit() ***/
#endif

void lim_set_ht_caps(tpAniSirGlobal p_mac, tpPESession p_session_entry,
		uint8_t *p_ie_start, uint32_t num_bytes)
{
	const uint8_t *p_ie = NULL;
	tDot11fIEHTCaps dot11_ht_cap = {0,};

	populate_dot11f_ht_caps(p_mac, p_session_entry, &dot11_ht_cap);
	p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_HTCAPS,
					p_ie_start, num_bytes);
	pe_debug("p_ie: %pK dot11_ht_cap.supportedMCSSet[0]: 0x%x",
		p_ie, dot11_ht_cap.supportedMCSSet[0]);
	if (p_ie) {
		/* convert from unpacked to packed structure */
		tHtCaps *p_ht_cap = (tHtCaps *) &p_ie[2];

		p_ht_cap->advCodingCap = dot11_ht_cap.advCodingCap;
		p_ht_cap->supportedChannelWidthSet =
			dot11_ht_cap.supportedChannelWidthSet;
		p_ht_cap->mimoPowerSave = dot11_ht_cap.mimoPowerSave;
		p_ht_cap->greenField = dot11_ht_cap.greenField;
		p_ht_cap->shortGI20MHz = dot11_ht_cap.shortGI20MHz;
		p_ht_cap->shortGI40MHz = dot11_ht_cap.shortGI40MHz;
		p_ht_cap->txSTBC = dot11_ht_cap.txSTBC;
		p_ht_cap->rxSTBC = dot11_ht_cap.rxSTBC;
		p_ht_cap->delayedBA = dot11_ht_cap.delayedBA;
		p_ht_cap->maximalAMSDUsize = dot11_ht_cap.maximalAMSDUsize;
		p_ht_cap->dsssCckMode40MHz = dot11_ht_cap.dsssCckMode40MHz;
		p_ht_cap->psmp = dot11_ht_cap.psmp;
		p_ht_cap->stbcControlFrame = dot11_ht_cap.stbcControlFrame;
		p_ht_cap->lsigTXOPProtection = dot11_ht_cap.lsigTXOPProtection;
		p_ht_cap->maxRxAMPDUFactor = dot11_ht_cap.maxRxAMPDUFactor;
		p_ht_cap->mpduDensity = dot11_ht_cap.mpduDensity;
		qdf_mem_copy((void *)p_ht_cap->supportedMCSSet,
			(void *)(dot11_ht_cap.supportedMCSSet),
			sizeof(p_ht_cap->supportedMCSSet));
		p_ht_cap->pco = dot11_ht_cap.pco;
		p_ht_cap->transitionTime = dot11_ht_cap.transitionTime;
		p_ht_cap->mcsFeedback = dot11_ht_cap.mcsFeedback;
		p_ht_cap->txBF = dot11_ht_cap.txBF;
		p_ht_cap->rxStaggeredSounding =
			dot11_ht_cap.rxStaggeredSounding;
		p_ht_cap->txStaggeredSounding =
			dot11_ht_cap.txStaggeredSounding;
		p_ht_cap->rxZLF = dot11_ht_cap.rxZLF;
		p_ht_cap->txZLF = dot11_ht_cap.txZLF;
		p_ht_cap->implicitTxBF = dot11_ht_cap.implicitTxBF;
		p_ht_cap->calibration = dot11_ht_cap.calibration;
		p_ht_cap->explicitCSITxBF = dot11_ht_cap.explicitCSITxBF;
		p_ht_cap->explicitUncompressedSteeringMatrix =
			dot11_ht_cap.explicitUncompressedSteeringMatrix;
		p_ht_cap->explicitBFCSIFeedback =
			dot11_ht_cap.explicitBFCSIFeedback;
		p_ht_cap->explicitUncompressedSteeringMatrixFeedback =
			dot11_ht_cap.explicitUncompressedSteeringMatrixFeedback;
		p_ht_cap->explicitCompressedSteeringMatrixFeedback =
			dot11_ht_cap.explicitCompressedSteeringMatrixFeedback;
		p_ht_cap->csiNumBFAntennae = dot11_ht_cap.csiNumBFAntennae;
		p_ht_cap->uncompressedSteeringMatrixBFAntennae =
			dot11_ht_cap.uncompressedSteeringMatrixBFAntennae;
		p_ht_cap->compressedSteeringMatrixBFAntennae =
			dot11_ht_cap.compressedSteeringMatrixBFAntennae;
		p_ht_cap->antennaSelection = dot11_ht_cap.antennaSelection;
		p_ht_cap->explicitCSIFeedbackTx =
			dot11_ht_cap.explicitCSIFeedbackTx;
		p_ht_cap->antennaIndicesFeedbackTx =
			dot11_ht_cap.antennaIndicesFeedbackTx;
		p_ht_cap->explicitCSIFeedback =
			dot11_ht_cap.explicitCSIFeedback;
		p_ht_cap->antennaIndicesFeedback =
			dot11_ht_cap.antennaIndicesFeedback;
		p_ht_cap->rxAS = dot11_ht_cap.rxAS;
		p_ht_cap->txSoundingPPDUs = dot11_ht_cap.txSoundingPPDUs;
	}
}

void lim_set_vht_caps(tpAniSirGlobal p_mac, tpPESession p_session_entry,
		      uint8_t *p_ie_start, uint32_t num_bytes)
{
	const uint8_t       *p_ie = NULL;
	tDot11fIEVHTCaps     dot11_vht_cap;

	populate_dot11f_vht_caps(p_mac, p_session_entry, &dot11_vht_cap);
	p_ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_VHTCAPS, p_ie_start,
					num_bytes);
	if (p_ie) {
		tSirMacVHTCapabilityInfo *vht_cap =
					(tSirMacVHTCapabilityInfo *) &p_ie[2];
		tSirVhtMcsInfo *vht_mcs = (tSirVhtMcsInfo *) &p_ie[2 +
					  sizeof(tSirMacVHTCapabilityInfo)];

		union {
			uint16_t                       u_value;
			tSirMacVHTRxSupDataRateInfo    vht_rx_supp_rate;
			tSirMacVHTTxSupDataRateInfo    vht_tx_supp_rate;
		} u_vht_data_rate_info;

		vht_cap->maxMPDULen = dot11_vht_cap.maxMPDULen;
		vht_cap->supportedChannelWidthSet =
					dot11_vht_cap.supportedChannelWidthSet;
		vht_cap->ldpcCodingCap = dot11_vht_cap.ldpcCodingCap;
		vht_cap->shortGI80MHz = dot11_vht_cap.shortGI80MHz;
		vht_cap->shortGI160and80plus80MHz =
					dot11_vht_cap.shortGI160and80plus80MHz;
		vht_cap->txSTBC = dot11_vht_cap.txSTBC;
		vht_cap->rxSTBC = dot11_vht_cap.rxSTBC;
		vht_cap->suBeamFormerCap = dot11_vht_cap.suBeamFormerCap;
		vht_cap->suBeamformeeCap = dot11_vht_cap.suBeamformeeCap;
		vht_cap->csnofBeamformerAntSup =
					dot11_vht_cap.csnofBeamformerAntSup;
		vht_cap->numSoundingDim = dot11_vht_cap.numSoundingDim;
		vht_cap->muBeamformerCap = dot11_vht_cap.muBeamformerCap;
		vht_cap->muBeamformeeCap = dot11_vht_cap.muBeamformeeCap;
		vht_cap->vhtTXOPPS = dot11_vht_cap.vhtTXOPPS;
		vht_cap->htcVHTCap = dot11_vht_cap.htcVHTCap;
		vht_cap->maxAMPDULenExp = dot11_vht_cap.maxAMPDULenExp;
		vht_cap->vhtLinkAdaptCap = dot11_vht_cap.vhtLinkAdaptCap;
		vht_cap->rxAntPattern = dot11_vht_cap.rxAntPattern;
		vht_cap->txAntPattern = dot11_vht_cap.txAntPattern;
		vht_cap->reserved1 = dot11_vht_cap.reserved1;

		/* Populate VHT MCS Information */
		vht_mcs->rxMcsMap = dot11_vht_cap.rxMCSMap;
		u_vht_data_rate_info.vht_rx_supp_rate.rxSupDataRate =
					dot11_vht_cap.rxHighSupDataRate;
		u_vht_data_rate_info.vht_rx_supp_rate.reserved =
					dot11_vht_cap.reserved2;
		vht_mcs->rxHighest = u_vht_data_rate_info.u_value;

		vht_mcs->txMcsMap = dot11_vht_cap.txMCSMap;
		u_vht_data_rate_info.vht_tx_supp_rate.txSupDataRate =
					dot11_vht_cap.txSupDataRate;
		u_vht_data_rate_info.vht_tx_supp_rate.reserved =
					dot11_vht_cap.reserved3;
		vht_mcs->txHighest = u_vht_data_rate_info.u_value;
	}
}

/**
 * lim_validate_received_frame_a1_addr() - To validate received frame's A1 addr
 * @mac_ctx: pointer to mac context
 * @a1: received frame's a1 address which is nothing but our self address
 * @session: PE session pointer
 *
 * This routine will validate, A1 address of the received frame
 *
 * Return: true or false
 */
bool lim_validate_received_frame_a1_addr(tpAniSirGlobal mac_ctx,
		tSirMacAddr a1, tpPESession session)
{
	if (mac_ctx == NULL || session == NULL) {
		pe_err("mac or session context is null");
		/* let main routine handle it */
		return true;
	}
	if (lim_is_group_addr(a1) || lim_is_addr_bc(a1)) {
		/* just for fail safe, don't handle MC/BC a1 in this routine */
		return true;
	}
	if (qdf_mem_cmp(a1, session->selfMacAddr, 6)) {
		pe_err("Invalid A1 address in received frame");
		return false;
	}
	return true;
}

/**
 * lim_check_and_reset_protection_params() - reset protection related parameters
 *
 * @mac_ctx: pointer to global mac structure
 *
 * resets protection related global parameters if the pe active session count
 * is zero.
 *
 * Return: None
 */
void lim_check_and_reset_protection_params(tpAniSirGlobal mac_ctx)
{
	if (!pe_get_active_session_count(mac_ctx)) {
		mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
	}
}

/**
 * lim_set_stads_rtt_cap() - update station node RTT capability
 * @sta_ds: Station hash node
 * @ext_cap: Pointer to extended capability
 * @mac_ctx: global MAC context
 *
 * This funciton update hash node's RTT capability based on received
 * Extended capability IE.
 *
 * Return: None
 */
void lim_set_stads_rtt_cap(tpDphHashNode sta_ds, struct s_ext_cap *ext_cap,
			   tpAniSirGlobal mac_ctx)
{
	sta_ds->timingMeasCap = 0;
	sta_ds->timingMeasCap |= (ext_cap->timing_meas) ?
				  RTT_TIMING_MEAS_CAPABILITY :
				  RTT_INVALID;
	sta_ds->timingMeasCap |= (ext_cap->fine_time_meas_initiator) ?
				  RTT_FINE_TIME_MEAS_INITIATOR_CAPABILITY :
				  RTT_INVALID;
	sta_ds->timingMeasCap |= (ext_cap->fine_time_meas_responder) ?
				  RTT_FINE_TIME_MEAS_RESPONDER_CAPABILITY :
				  RTT_INVALID;

	pe_debug("ExtCap present, timingMeas: %d Initiator: %d Responder: %d",
	    ext_cap->timing_meas, ext_cap->fine_time_meas_initiator,
	    ext_cap->fine_time_meas_responder);
}

#ifdef WLAN_SUPPORT_TWT
void lim_set_peer_twt_cap(tpPESession session, struct s_ext_cap *ext_cap)
{
	if (session->enable_session_twt_support) {
		session->peer_twt_requestor = ext_cap->twt_requestor_support;
		session->peer_twt_responder = ext_cap->twt_responder_support;
	}

	pe_debug("Ext Cap peer TWT requestor: %d, responder: %d, enable_twt %d",
		 ext_cap->twt_requestor_support,
		 ext_cap->twt_responder_support,
		 session->enable_session_twt_support);
}
#endif

/**
 * lim_send_ie() - sends IE to wma
 * @mac_ctx: global MAC context
 * @sme_session_id: sme session id
 * @eid: IE id
 * @band: band for which IE is intended
 * @buf: buffer containing IE
 * @len: length of buffer
 *
 * This funciton sends the IE data to WMA.
 *
 * Return: status of operation
 */
static QDF_STATUS lim_send_ie(tpAniSirGlobal mac_ctx, uint32_t sme_session_id,
			      uint8_t eid, enum cds_band_type band,
			      uint8_t *buf, uint32_t len)
{
	struct vdev_ie_info *ie_msg;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	/* Allocate memory for the WMI request */
	ie_msg = qdf_mem_malloc(sizeof(*ie_msg) + len);
	if (!ie_msg) {
		pe_err("Failed to allocate memory");
		return QDF_STATUS_E_NOMEM;
	}

	ie_msg->vdev_id = sme_session_id;
	ie_msg->ie_id = eid;
	ie_msg->length = len;
	ie_msg->band = band;
	/* IE data buffer starts at end of the struct */
	ie_msg->data = (uint8_t *)&ie_msg[1];

	qdf_mem_copy(ie_msg->data, buf, len);
	msg.type = WMA_SET_IE_INFO;
	msg.bodyptr = ie_msg;
	msg.reserved = 0;

	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Not able to post WMA_SET_IE_INFO to WMA");
		qdf_mem_free(ie_msg);
		return status;
	}

	return status;
}

/**
 * lim_get_rx_ldpc() - gets ldpc setting for given channel(band)
 * @mac_ctx: global mac context
 * @ch: channel enum for which ldpc setting is required
 *      Note: ch param is not absolute channel number rather it is
 *            channel number enum.
 *
 * Return: true if enabled and false otherwise
 */
static inline bool lim_get_rx_ldpc(tpAniSirGlobal mac_ctx, enum channel_enum ch)
{
	if (mac_ctx->roam.configParam.rx_ldpc_enable &&
		wma_is_rx_ldpc_supported_for_channel(WLAN_REG_CH_NUM(ch)))
		return true;
	else
		return false;
}

/**
 * lim_populate_mcs_set_ht_per_vdev() - update the MCS set according to vdev nss
 * @mac_ctx: global mac context
 * @ht_cap: pointer to ht caps
 * @vdev_id: vdev for which IE is targeted
 * @band: band for which the MCS set has to be updated
 *
 * This function updates the MCS set according to vdev nss
 *
 * Return: None
 */
static void lim_populate_mcs_set_ht_per_vdev(tpAniSirGlobal mac_ctx,
					     struct sHtCaps *ht_cap,
					     uint8_t vdev_id,
					     uint8_t band)
{
	struct mlme_nss_chains *nss_chains_ini_cfg;
	struct wlan_objmgr_vdev *vdev =
			wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							     vdev_id,
							     WLAN_MLME_SB_ID);
	if (!vdev) {
		pe_err("Got NULL vdev obj, returning");
		return;
	}
	if (!ht_cap->supportedMCSSet[1])
		goto end;
	nss_chains_ini_cfg = mlme_get_ini_vdev_config(vdev);
	if (!nss_chains_ini_cfg) {
		pe_err("nss chain dynamic config NULL");
		goto end;
	}

	/* convert from unpacked to packed structure */
	if (nss_chains_ini_cfg->rx_nss[band] == 1)
		ht_cap->supportedMCSSet[1] = 0;

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

/**
 * lim_populate_mcs_set_vht_per_vdev() - update MCS set according to vdev nss
 * @mac_ctx: global mac context
 * @vht_caps: pointer to vht_caps
 * @vdev_id: vdev for which IE is targeted
 * @band: band for which the MCS set has to be updated
 *
 * This function updates the MCS set according to vdev nss
 *
 * Return: None
 */
static void lim_populate_mcs_set_vht_per_vdev(tpAniSirGlobal mac_ctx,
					      uint8_t *vht_caps,
					      uint8_t vdev_id,
					      uint8_t band)
{
	struct mlme_nss_chains *nss_chains_ini_cfg;
	tSirVhtMcsInfo *vht_mcs;
	struct wlan_objmgr_vdev *vdev =
			wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							     vdev_id,
							     WLAN_MLME_SB_ID);
	if (!vdev) {
		pe_err("Got NULL vdev obj, returning");
		return;
	}

	nss_chains_ini_cfg = mlme_get_ini_vdev_config(vdev);
	if (!nss_chains_ini_cfg) {
		pe_err("nss chain dynamic config NULL");
		goto end;
	}

	vht_mcs = (tSirVhtMcsInfo *)&vht_caps[2 +
					sizeof(tSirMacVHTCapabilityInfo)];
	if (nss_chains_ini_cfg->tx_nss[band] == 1) {
	/* Populate VHT MCS Information */
		vht_mcs->txMcsMap |= DISABLE_NSS2_MCS;
		vht_mcs->txHighest =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
	}

	if (nss_chains_ini_cfg->rx_nss[band] == 1) {
	/* Populate VHT MCS Information */
		vht_mcs->rxMcsMap |= DISABLE_NSS2_MCS;
		vht_mcs->rxHighest =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
	}

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

/**
 * is_dot11mode_support_ht_cap() - Check dot11mode supports HT capability
 * @dot11mode: dot11mode
 *
 * This function checks whether dot11mode support HT capability or not
 *
 * Return: True, if supports. False otherwise
 */
static bool is_dot11mode_support_ht_cap(enum csr_cfgdot11mode dot11mode)
{
	if ((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11N) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11N_ONLY) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY)) {
		return true;
	}

	return false;
}

/**
 * is_dot11mode_support_vht_cap() - Check dot11mode supports VHT capability
 * @dot11mode: dot11mode
 *
 * This function checks whether dot11mode support VHT capability or not
 *
 * Return: True, if supports. False otherwise
 */
static bool is_dot11mode_support_vht_cap(enum csr_cfgdot11mode dot11mode)
{
	if ((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY)) {
		return true;
	}

	return false;
}

/**
 * is_dot11mode_support_he_cap() - Check dot11mode supports HE capability
 * @dot11mode: dot11mode
 *
 * This function checks whether dot11mode support HE capability or not
 *
 * Return: True, if supports. False otherwise
 */
static bool is_dot11mode_support_he_cap(enum csr_cfgdot11mode dot11mode)
{
	if ((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY)) {
		return true;
	}

	return false;
}

/**
 * lim_send_ht_caps_ie() - gets HT capability and send to firmware via wma
 * @mac_ctx: global mac context
 * @session: pe session. This can be NULL. In that case self cap will be sent
 * @device_mode: VDEV op mode
 * @vdev_id: vdev for which IE is targeted
 *
 * This function gets HT capability and send to firmware via wma
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_send_ht_caps_ie(tpAniSirGlobal mac_ctx,
				      tpPESession session,
				      enum QDF_OPMODE device_mode,
				      uint8_t vdev_id)
{
	uint8_t ht_caps[DOT11F_IE_HTCAPS_MIN_LEN + 2] = {0};
	tHtCaps *p_ht_cap = (tHtCaps *)(&ht_caps[2]);
	QDF_STATUS status_5g, status_2g;

	ht_caps[0] = DOT11F_EID_HTCAPS;
	ht_caps[1] = DOT11F_IE_HTCAPS_MIN_LEN;
	lim_set_ht_caps(mac_ctx, session, ht_caps,
			DOT11F_IE_HTCAPS_MIN_LEN + 2);
	/* Get LDPC and over write for 2G */
	p_ht_cap->advCodingCap = lim_get_rx_ldpc(mac_ctx,
						 CHAN_ENUM_6);
	/* Get self cap for HT40 support in 2G */
	if (mac_ctx->roam.configParam.channelBondingMode24GHz) {
		p_ht_cap->supportedChannelWidthSet = 1;
		p_ht_cap->shortGI40MHz = 1;
	} else {
		p_ht_cap->supportedChannelWidthSet = 0;
		p_ht_cap->shortGI40MHz = 0;
	}

	lim_populate_mcs_set_ht_per_vdev(mac_ctx, p_ht_cap, vdev_id,
					 NSS_CHAINS_BAND_2GHZ);
	if (device_mode == QDF_NDI_MODE) {
		p_ht_cap->txBF = 0;
		p_ht_cap->implicitTxBF = 0;
		p_ht_cap->explicitCSITxBF = 0;
	}

	status_2g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HTCAPS,
				CDS_BAND_2GHZ, &ht_caps[2],
				DOT11F_IE_HTCAPS_MIN_LEN);
	/*
	 * Get LDPC and over write for 5G - using channel 64 because it
	 * is available in all reg domains.
	 */
	p_ht_cap->advCodingCap = lim_get_rx_ldpc(mac_ctx, CHAN_ENUM_64);
	/* Get self cap for HT40 support in 5G */
	if (mac_ctx->roam.configParam.channelBondingMode5GHz) {
		p_ht_cap->supportedChannelWidthSet = 1;
		p_ht_cap->shortGI40MHz = 1;
	} else {
		p_ht_cap->supportedChannelWidthSet = 0;
		p_ht_cap->shortGI40MHz = 0;
	}
	lim_populate_mcs_set_ht_per_vdev(mac_ctx, p_ht_cap, vdev_id,
					 NSS_CHAINS_BAND_5GHZ);
	status_5g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HTCAPS,
				CDS_BAND_5GHZ, &ht_caps[2],
				DOT11F_IE_HTCAPS_MIN_LEN);

	if (QDF_IS_STATUS_SUCCESS(status_2g) &&
	    QDF_IS_STATUS_SUCCESS(status_5g))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_send_vht_caps_ie() - gets VHT capability and send to firmware via wma
 * @mac_ctx: global mac context
 * @session: pe session. This can be NULL. In that case self cap will be sent
 * @device_mode: VDEV op mode
 * @vdev_id: vdev for which IE is targeted
 *
 * This function gets VHT capability and send to firmware via wma
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_send_vht_caps_ie(tpAniSirGlobal mac_ctx,
				       tpPESession session,
				       enum QDF_OPMODE device_mode,
				       uint8_t vdev_id)
{
	uint8_t vht_caps[DOT11F_IE_VHTCAPS_MAX_LEN + 2] = {0};
	tSirMacVHTCapabilityInfo *p_vht_cap =
			(tSirMacVHTCapabilityInfo *)(&vht_caps[2]);
	QDF_STATUS status_5g, status_2g;

	vht_caps[0] = DOT11F_EID_VHTCAPS;
	vht_caps[1] = DOT11F_IE_VHTCAPS_MAX_LEN;
	lim_set_vht_caps(mac_ctx, session, vht_caps,
			DOT11F_IE_VHTCAPS_MIN_LEN + 2);
	/*
	 * Get LDPC and over write for 5G - using channel 64 because it
	 * is available in all reg domains.
	 */
	p_vht_cap->ldpcCodingCap = lim_get_rx_ldpc(mac_ctx, CHAN_ENUM_64);
	lim_populate_mcs_set_vht_per_vdev(mac_ctx, vht_caps, vdev_id,
					  NSS_CHAINS_BAND_5GHZ);

	if (device_mode == QDF_NDI_MODE) {
		p_vht_cap->muBeamformeeCap = 0;
		p_vht_cap->muBeamformerCap = 0;
		p_vht_cap->suBeamformeeCap = 0;
		p_vht_cap->suBeamFormerCap = 0;
	}
	/*
	 * Self VHT channel width for 5G is already negotiated
	 * with FW
	 */
	status_5g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_VHTCAPS,
				CDS_BAND_5GHZ, &vht_caps[2],
				DOT11F_IE_VHTCAPS_MIN_LEN);

	/* Get LDPC and over write for 2G */
	p_vht_cap->ldpcCodingCap = lim_get_rx_ldpc(mac_ctx, CHAN_ENUM_6);
	/* Self VHT 80/160/80+80 channel width for 2G is 0 */
	p_vht_cap->supportedChannelWidthSet = 0;
	p_vht_cap->shortGI80MHz = 0;
	p_vht_cap->shortGI160and80plus80MHz = 0;
	lim_populate_mcs_set_vht_per_vdev(mac_ctx, vht_caps, vdev_id,
					  NSS_CHAINS_BAND_2GHZ);
	status_2g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_VHTCAPS,
				CDS_BAND_2GHZ, &vht_caps[2],
				DOT11F_IE_VHTCAPS_MIN_LEN);

	if (QDF_IS_STATUS_SUCCESS(status_2g) &&
	    QDF_IS_STATUS_SUCCESS(status_5g))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_send_ies_per_band() - gets ht and vht capability and send to firmware via
 * wma
 * @mac_ctx: global mac context
 * @session: pe session. This can be NULL. In that case self cap will be sent
 * @vdev_id: vdev for which IE is targeted
 *
 * This funciton gets ht and vht capability and send to firmware via wma
 *
 * Return: status of operation
 */
QDF_STATUS lim_send_ies_per_band(tpAniSirGlobal mac_ctx,
				 tpPESession session,
				 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE device_mode;
	QDF_STATUS status_ht = QDF_STATUS_SUCCESS;
	QDF_STATUS status_vht = QDF_STATUS_SUCCESS;
	QDF_STATUS status_he = QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
			mac_ctx->psoc, vdev_id,
			WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		pe_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	device_mode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	/*
	 * Note: Do not use Dot11f VHT structure, since 1 byte present flag in
	 * it is causing weird padding errors. Instead use Sir Mac VHT struct
	 * to send IE to wma.
	 */
	if (is_dot11mode_support_ht_cap(
				mac_ctx->roam.configParam.uCfgDot11Mode))
		status_ht = lim_send_ht_caps_ie(mac_ctx, session,
						device_mode, vdev_id);

	if (is_dot11mode_support_vht_cap(
				mac_ctx->roam.configParam.uCfgDot11Mode))
		status_vht = lim_send_vht_caps_ie(mac_ctx, session,
						  device_mode, vdev_id);

	if (is_dot11mode_support_he_cap(
				mac_ctx->roam.configParam.uCfgDot11Mode))
		status_he = lim_send_he_caps_ie(mac_ctx, session,
						device_mode, vdev_id);

	if (QDF_IS_STATUS_SUCCESS(status_ht) &&
	    QDF_IS_STATUS_SUCCESS(status_vht) &&
	    QDF_IS_STATUS_SUCCESS(status_he))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_send_ext_cap_ie() - send ext cap IE to FW
 * @mac_ctx: global MAC context
 * @session_entry: PE session
 * @extra_extcap: extracted ext cap
 * @merge: merge extra ext cap
 *
 * This function is invoked after VDEV is created to update firmware
 * about the extended capabilities that the corresponding VDEV is capable
 * of. Since STA/SAP can have different Extended capabilities set, this function
 * is called per vdev creation.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_send_ext_cap_ie(tpAniSirGlobal mac_ctx,
			       uint32_t session_id,
			       tDot11fIEExtCap *extra_extcap, bool merge)
{
	tDot11fIEExtCap ext_cap_data = {0};
	uint32_t dot11mode, num_bytes;
	bool vht_enabled = false;
	struct vdev_ie_info *vdev_ie;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	wlan_cfg_get_int(mac_ctx, WNI_CFG_DOT11_MODE, &dot11mode);
	if (IS_DOT11_MODE_VHT(dot11mode))
		vht_enabled = true;

	status = populate_dot11f_ext_cap(mac_ctx, vht_enabled, &ext_cap_data,
					 NULL);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Failed to populate ext cap IE");
		return QDF_STATUS_E_FAILURE;
	}

	num_bytes = ext_cap_data.num_bytes;

	if (merge && NULL != extra_extcap && extra_extcap->num_bytes > 0) {
		if (extra_extcap->num_bytes > ext_cap_data.num_bytes)
			num_bytes = extra_extcap->num_bytes;
		lim_merge_extcap_struct(&ext_cap_data, extra_extcap, true);
	}

	/* Allocate memory for the WMI request, and copy the parameter */
	vdev_ie = qdf_mem_malloc(sizeof(*vdev_ie) + num_bytes);
	if (!vdev_ie) {
		pe_err("Failed to allocate memory");
		return QDF_STATUS_E_NOMEM;
	}

	vdev_ie->vdev_id = session_id;
	vdev_ie->ie_id = DOT11F_EID_EXTCAP;
	vdev_ie->length = num_bytes;
	vdev_ie->band = 0;

	vdev_ie->data = (uint8_t *)vdev_ie + sizeof(*vdev_ie);
	qdf_mem_copy(vdev_ie->data, ext_cap_data.bytes, num_bytes);

	msg.type = WMA_SET_IE_INFO;
	msg.bodyptr = vdev_ie;
	msg.reserved = 0;

	if (QDF_STATUS_SUCCESS !=
		scheduler_post_message(QDF_MODULE_ID_PE,
				       QDF_MODULE_ID_WMA,
				       QDF_MODULE_ID_WMA, &msg)) {
		pe_err("Not able to post WMA_SET_IE_INFO to WDA");
		qdf_mem_free(vdev_ie);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_strip_ie() - strip requested IE from IE buffer
 * @mac_ctx: global MAC context
 * @addn_ie: Additional IE buffer
 * @addn_ielen: Length of additional IE
 * @eid: EID of IE to strip
 * @size_of_len_field: length of IE length field
 * @oui: if present matches OUI also
 * @oui_length: if previous present, this is length of oui
 * @extracted_ie: if not NULL, copy the stripped IE to this buffer
 *
 * This utility function is used to strip of the requested IE if present
 * in IE buffer.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_strip_ie(tpAniSirGlobal mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen,
		uint8_t eid, eSizeOfLenField size_of_len_field,
		uint8_t *oui, uint8_t oui_length, uint8_t *extracted_ie,
		uint32_t eid_max_len)
{
	uint8_t *tempbuf = NULL;
	uint16_t templen = 0;
	int left = *addn_ielen;
	uint8_t *ptr = addn_ie;
	uint8_t elem_id;
	uint16_t elem_len, ie_len, extracted_ie_len = 0;

	if (NULL == addn_ie) {
		pe_debug("NULL addn_ie pointer");
		return QDF_STATUS_E_INVAL;
	}

	tempbuf = qdf_mem_malloc(left);
	if (NULL == tempbuf) {
		pe_err("Unable to allocate memory");
		return QDF_STATUS_E_NOMEM;
	}

	if (extracted_ie)
		qdf_mem_zero(extracted_ie, eid_max_len + size_of_len_field + 1);

	while (left >= 2) {
		elem_id  = ptr[0];
		left -= 1;
		if (size_of_len_field == TWO_BYTE) {
			elem_len = *((uint16_t *)&ptr[1]);
			left -= 2;
		} else {
			elem_len = ptr[1];
			left -= 1;
		}
		if (elem_len > left) {
			pe_err("Invalid IEs eid: %d elem_len: %d left: %d",
				elem_id, elem_len, left);
			qdf_mem_free(tempbuf);
			return QDF_STATUS_E_FAILURE;
		}

		if (eid != elem_id ||
				(oui && qdf_mem_cmp(oui,
						&ptr[size_of_len_field + 1],
						oui_length))) {
			qdf_mem_copy(tempbuf + templen, &ptr[0],
				     elem_len + size_of_len_field + 1);
			templen += (elem_len + size_of_len_field + 1);
		} else {
			/*
			 * eid matched and if provided OUI also matched
			 * take oui IE and store in provided buffer.
			 */
			if (NULL != extracted_ie) {
				ie_len = elem_len + size_of_len_field + 1;
				if (ie_len <= eid_max_len - extracted_ie_len) {
					qdf_mem_copy(
					extracted_ie + extracted_ie_len,
					&ptr[0], ie_len);
					extracted_ie_len += ie_len;
				}
			}
		}
		left -= elem_len;
		ptr += (elem_len + size_of_len_field + 1);
	}
	qdf_mem_copy(addn_ie, tempbuf, templen);

	*addn_ielen = templen;
	qdf_mem_free(tempbuf);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11W
void lim_del_pmf_sa_query_timer(tpAniSirGlobal mac_ctx, tpPESession pe_session)
{
	uint32_t associated_sta;
	tpDphHashNode sta_ds = NULL;

	for (associated_sta = 1;
			associated_sta < mac_ctx->lim.gLimAssocStaLimit;
			associated_sta++) {
		sta_ds = dph_get_hash_entry(mac_ctx, associated_sta,
				&pe_session->dph.dphHashTable);
		if (NULL == sta_ds)
			continue;
		if (!sta_ds->rmfEnabled) {
			pe_debug("no PMF timer for sta-idx:%d assoc-id:%d",
				 sta_ds->staIndex, sta_ds->assocId);
			continue;
		}

		pe_debug("Deleting pmfSaQueryTimer for sta-idx:%d assoc-id:%d",
			sta_ds->staIndex, sta_ds->assocId);
		tx_timer_deactivate(&sta_ds->pmfSaQueryTimer);
		tx_timer_delete(&sta_ds->pmfSaQueryTimer);
	}
}
#endif

QDF_STATUS lim_strip_supp_op_class_update_struct(tpAniSirGlobal mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen,
		tDot11fIESuppOperatingClasses *dst)
{
	uint8_t extracted_buff[DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN + 2];
	QDF_STATUS status;

	qdf_mem_zero((uint8_t *)&extracted_buff[0],
		    DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN + 2);
	status = lim_strip_ie(mac_ctx, addn_ie, addn_ielen,
			      DOT11F_EID_SUPPOPERATINGCLASSES, ONE_BYTE,
			      NULL, 0, extracted_buff,
			      DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_warn("Failed to strip supp_op_mode IE status: %d",
		       status);
		return status;
	}

	if (DOT11F_EID_SUPPOPERATINGCLASSES != extracted_buff[0] ||
	    extracted_buff[1] > DOT11F_IE_SUPPOPERATINGCLASSES_MAX_LEN) {
		pe_warn("Invalid IEs eid: %d elem_len: %d",
			extracted_buff[0], extracted_buff[1]);
		return QDF_STATUS_E_FAILURE;
	}

	/* update the extracted supp op class to struct*/
	if (DOT11F_PARSE_SUCCESS != dot11f_unpack_ie_supp_operating_classes(
	    mac_ctx, &extracted_buff[2], extracted_buff[1], dst, false)) {
		pe_err("dot11f_unpack Parse Error");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_update_extcap_struct() - poputlate the dot11f structure
 * @mac_ctx: global MAC context
 * @buf: extracted IE buffer
 * @dst: extended capability IE structure to be updated
 *
 * This function is used to update the extended capability structure
 * with @buf.
 *
 * Return: None
 */
void lim_update_extcap_struct(tpAniSirGlobal mac_ctx,
	uint8_t *buf, tDot11fIEExtCap *dst)
{
	uint8_t out[DOT11F_IE_EXTCAP_MAX_LEN];
	uint32_t status;

	if (NULL == buf) {
		pe_err("Invalid Buffer Address");
		return;
	}

	if (NULL == dst) {
		pe_err("NULL dst pointer");
		return;
	}

	if (DOT11F_EID_EXTCAP != buf[0] || buf[1] > DOT11F_IE_EXTCAP_MAX_LEN) {
		pe_debug_rl("Invalid IEs eid: %d elem_len: %d", buf[0], buf[1]);
		return;
	}

	qdf_mem_zero((uint8_t *)&out[0], DOT11F_IE_EXTCAP_MAX_LEN);
	qdf_mem_copy(&out[0], &buf[2], buf[1]);

	status = dot11f_unpack_ie_ext_cap(mac_ctx, &out[0],
					buf[1], dst, false);
	if (DOT11F_PARSE_SUCCESS != status)
		pe_err("dot11f_unpack Parse Error %d", status);
}

/**
 * lim_strip_extcap_update_struct - strip extended capability IE and populate
 *				  the dot11f structure
 * @mac_ctx: global MAC context
 * @addn_ie: Additional IE buffer
 * @addn_ielen: Length of additional IE
 * @dst: extended capability IE structure to be updated
 *
 * This function is used to strip extended capability IE from IE buffer and
 * update the passed structure.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_strip_extcap_update_struct(tpAniSirGlobal mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen, tDot11fIEExtCap *dst)
{
	uint8_t extracted_buff[DOT11F_IE_EXTCAP_MAX_LEN + 2];
	QDF_STATUS status;

	qdf_mem_zero((uint8_t *)&extracted_buff[0],
		      DOT11F_IE_EXTCAP_MAX_LEN + 2);
	status = lim_strip_ie(mac_ctx, addn_ie, addn_ielen,
			      DOT11F_EID_EXTCAP, ONE_BYTE,
			      NULL, 0, extracted_buff,
			      DOT11F_IE_EXTCAP_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_debug("Failed to strip extcap IE status: %d", status);
		return status;
	}

	/* update the extracted ExtCap to struct*/
	lim_update_extcap_struct(mac_ctx, extracted_buff, dst);
	return status;
}

/**
 * lim_merge_extcap_struct() - merge extended capabilities info
 * @dst: destination extended capabilities
 * @src: source extended capabilities
 * @add: true if add the capabilities, false if strip the capabilities.
 *
 * This function is used to take @src info and add/strip it to/from
 * @dst extended capabilities info.
 *
 * Return: None
 */
void lim_merge_extcap_struct(tDot11fIEExtCap *dst,
			     tDot11fIEExtCap *src,
			     bool add)
{
	uint8_t *tempdst = (uint8_t *)dst->bytes;
	uint8_t *tempsrc = (uint8_t *)src->bytes;
	uint8_t structlen = member_size(tDot11fIEExtCap, bytes);

	/* Return if @src not present */
	if (!src->present)
		return;

	/* Return if strip the capabilities from @dst which not present */
	if (!dst->present && !add)
		return;

	/* Merge the capabilities info in other cases */
	while (tempdst && tempsrc && structlen--) {
		if (add)
			*tempdst |= *tempsrc;
		else
			*tempdst &= *tempsrc;
		tempdst++;
		tempsrc++;
	}
	dst->num_bytes = lim_compute_ext_cap_ie_length(dst);
	if (dst->num_bytes == 0)
		dst->present = 0;
	else
		dst->present = 1;
}

/**
 * lim_get_80Mhz_center_channel - finds 80 Mhz center channel
 *
 * @primary_channel:   Primary channel for given 80 MHz band
 *
 * There are fixed 80MHz band and for each fixed band there is only one center
 * valid channel. Also location of primary channel decides what 80 MHz band will
 * it use, hence it decides what center channel will be used. This function
 * does thus calculation and returns the center channel.
 *
 * Return: center channel
 */
uint8_t
lim_get_80Mhz_center_channel(uint8_t primary_channel)
{
	if (primary_channel >= 36 && primary_channel <= 48)
		return (36+48)/2;
	if (primary_channel >= 52 && primary_channel <= 64)
		return (52+64)/2;
	if (primary_channel >= 100 && primary_channel <= 112)
		return (100+112)/2;
	if (primary_channel >= 116 && primary_channel <= 128)
		return (116+128)/2;
	if (primary_channel >= 132 && primary_channel <= 144)
		return (132+144)/2;
	if (primary_channel >= 149 && primary_channel <= 161)
		return (149+161)/2;

	return INVALID_CHANNEL_ID;
}

/**
 * lim_bss_type_to_string(): converts bss type enum to string.
 * @bss_type: enum value of bss_type.
 *
 * Return: Printable string for bss_type
 */
const char *lim_bss_type_to_string(const uint16_t bss_type)
{
	switch (bss_type) {
	CASE_RETURN_STRING(eSIR_INFRASTRUCTURE_MODE);
	CASE_RETURN_STRING(eSIR_INFRA_AP_MODE);
	CASE_RETURN_STRING(eSIR_IBSS_MODE);
	CASE_RETURN_STRING(eSIR_AUTO_MODE);
	CASE_RETURN_STRING(eSIR_NDI_MODE);
	default:
		return "Unknown bss_type";
	}
}

/**
 * lim_init_obss_params(): Initializes the OBSS Scan Parameters
 * @sesssion: LIM session
 * @mac_ctx: Mac context
 *
 * Return: None
 */
void lim_init_obss_params(tpAniSirGlobal mac_ctx, tpPESession session)
{
	uint32_t  cfg_value;

	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME,
		&cfg_value) !=  QDF_STATUS_SUCCESS) {
		pe_err("Fail to retrieve: %x value",
			WNI_CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME);
		return;
	}
	session->obss_ht40_scanparam.obss_active_dwelltime = cfg_value;

	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME,
		&cfg_value) != QDF_STATUS_SUCCESS) {
		pe_err("Fail to retrieve: %x value",
			WNI_CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME);
		return;
	}
	session->obss_ht40_scanparam.obss_passive_dwelltime = cfg_value;

	if (wlan_cfg_get_int(mac_ctx,
		WNI_CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL,
		&cfg_value) != QDF_STATUS_SUCCESS) {
		pe_err("Fail to retrieve: %x value",
			WNI_CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL);
		return;
	}
	session->obss_ht40_scanparam.obss_width_trigger_interval = cfg_value;
	if (wlan_cfg_get_int(mac_ctx,
		WNI_CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL,
		&cfg_value) != QDF_STATUS_SUCCESS) {
		pe_err("Fail to retrieve: %x value",
			WNI_CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL);
		return;
	}
	session->obss_ht40_scanparam.obss_active_total_per_channel = cfg_value;
	if (wlan_cfg_get_int(mac_ctx,
		WNI_CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL, &cfg_value)
		!= QDF_STATUS_SUCCESS) {
		pe_err("Fail to retrieve: %x value",
			WNI_CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL);
		return;
	}
	session->obss_ht40_scanparam.obss_passive_total_per_channel = cfg_value;

	if (wlan_cfg_get_int(mac_ctx,
		WNI_CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY, &cfg_value)
		!= QDF_STATUS_SUCCESS) {
		pe_err("Fail to retrieve: %x value",
			WNI_CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY);
		return;
	}
	session->obss_ht40_scanparam.bsswidth_ch_trans_delay =
								cfg_value;

	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD,
		&cfg_value) != QDF_STATUS_SUCCESS) {
		pe_err("Fail to retrieve: %x value",
			WNI_CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD);
		return;
	}
	session->obss_ht40_scanparam.obss_activity_threshold = cfg_value;
}

/**
 * lim_update_obss_scanparams(): Updates OBSS SCAN IE parameters to session
 * @sesssion: LIM session
 * @scan_params: Scan parameters
 *
 * Return: None
 */
void lim_update_obss_scanparams(tpPESession session,
			tDot11fIEOBSSScanParameters *scan_params)
{
	/*
	 * If the received value is not in the range specified
	 * by the Specification then it will be the default value
	 * configured through cfg
	 */
	if ((scan_params->obssScanActiveDwell >
		WNI_CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_STAMIN) &&
		(scan_params->obssScanActiveDwell <
		WNI_CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME_STAMAX))
		session->obss_ht40_scanparam.obss_active_dwelltime =
			scan_params->obssScanActiveDwell;

	if ((scan_params->obssScanPassiveDwell >
		WNI_CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_STAMIN) &&
		(scan_params->obssScanPassiveDwell <
		WNI_CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME_STAMAX))
		session->obss_ht40_scanparam.obss_passive_dwelltime =
			scan_params->obssScanPassiveDwell;

	if ((scan_params->bssWidthChannelTransitionDelayFactor >
		WNI_CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY_STAMIN) &&
		(scan_params->bssWidthChannelTransitionDelayFactor <
		WNI_CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY_STAMAX))
		session->obss_ht40_scanparam.bsswidth_ch_trans_delay =
			scan_params->bssWidthChannelTransitionDelayFactor;

	if ((scan_params->obssScanActiveTotalPerChannel >
		WNI_CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL_STAMIN) &&
		(scan_params->obssScanActiveTotalPerChannel <
		WNI_CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL_STAMAX))
		session->obss_ht40_scanparam.obss_active_total_per_channel =
			scan_params->obssScanActiveTotalPerChannel;

	if ((scan_params->obssScanPassiveTotalPerChannel >
		WNI_CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL_STAMIN) &&
		(scan_params->obssScanPassiveTotalPerChannel <
		WNI_CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL_STAMAX))
		session->obss_ht40_scanparam.obss_passive_total_per_channel =
			scan_params->obssScanPassiveTotalPerChannel;

	if ((scan_params->bssChannelWidthTriggerScanInterval >
		WNI_CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_STAMIN) &&
		(scan_params->bssChannelWidthTriggerScanInterval <
		WNI_CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL_STAMAX))
		session->obss_ht40_scanparam.obss_width_trigger_interval =
			scan_params->bssChannelWidthTriggerScanInterval;

	if ((scan_params->obssScanActivityThreshold >
		WNI_CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD_STAMIN) &&
		(scan_params->obssScanActivityThreshold <
		WNI_CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD_STAMAX))
		session->obss_ht40_scanparam.obss_activity_threshold =
			scan_params->obssScanActivityThreshold;
	return;
}

/**
 * lim_is_robust_mgmt_action_frame() - Check if action category is
 * robust action frame
 * @action_category: Action frame category.
 *
 * This function is used to check if given action category is robust
 * action frame.
 *
 * Return: bool
 */
bool lim_is_robust_mgmt_action_frame(uint8_t action_category)
{
	switch (action_category) {
	/*
	 * NOTE: This function doesn't take care of the DMG
	 * (Directional Multi-Gigatbit) BSS case as 8011ad
	 * support is not yet added. In future, if the support
	 * is required then this function need few more arguments
	 * and little change in logic.
	 */
	case SIR_MAC_ACTION_SPECTRUM_MGMT:
	case SIR_MAC_ACTION_QOS_MGMT:
	case SIR_MAC_ACTION_DLP:
	case SIR_MAC_ACTION_BLKACK:
	case SIR_MAC_ACTION_RRM:
	case SIR_MAC_ACTION_FAST_BSS_TRNST:
	case SIR_MAC_ACTION_SA_QUERY:
	case SIR_MAC_ACTION_PROT_DUAL_PUB:
	case SIR_MAC_ACTION_WNM:
	case SIR_MAC_ACITON_MESH:
	case SIR_MAC_ACTION_MHF:
	case SIR_MAC_ACTION_FST:
		return true;
	default:
		pe_debug("non-PMF action category: %d", action_category);
		break;
	}
	return false;
}

/**
 * lim_compute_ext_cap_ie_length - compute the length of ext cap ie
 * based on the bits set
 * @ext_cap: extended IEs structure
 *
 * Return: length of the ext cap ie, 0 means should not present
 */
uint8_t lim_compute_ext_cap_ie_length(tDot11fIEExtCap *ext_cap)
{
	uint8_t i = DOT11F_IE_EXTCAP_MAX_LEN;

	while (i) {
		if (ext_cap->bytes[i-1])
			break;
		i--;
	}

	return i;
}

/**
 * lim_update_caps_info_for_bss - Update capability info for this BSS
 *
 * @mac_ctx: mac context
 * @caps: Pointer to capability info to be updated
 * @bss_caps: Capability info of the BSS
 *
 * Update the capability info in Assoc/Reassoc request frames and reset
 * the spectrum management, short preamble, immediate block ack bits
 * if the BSS doesnot support it
 *
 * Return: None
 */
void lim_update_caps_info_for_bss(tpAniSirGlobal mac_ctx,
		uint16_t *caps, uint16_t bss_caps)
{
	if (!(bss_caps & LIM_SPECTRUM_MANAGEMENT_BIT_MASK)) {
		*caps &= (~LIM_SPECTRUM_MANAGEMENT_BIT_MASK);
		pe_debug("Clearing spectrum management:no AP support");
	}

	if (!(bss_caps & LIM_SHORT_PREAMBLE_BIT_MASK)) {
		*caps &= (~LIM_SHORT_PREAMBLE_BIT_MASK);
		pe_debug("Clearing short preamble:no AP support");
	}

	if (!(bss_caps & LIM_IMMEDIATE_BLOCK_ACK_MASK)) {
		*caps &= (~LIM_IMMEDIATE_BLOCK_ACK_MASK);
		pe_debug("Clearing Immed Blk Ack:no AP support");
	}
}
/**
 * lim_send_set_dtim_period(): Send SIR_HAL_SET_DTIM_PERIOD message
 * to set dtim period.
 *
 * @sesssion: LIM session
 * @dtim_period: dtim value
 * @mac_ctx: Mac context
 * @return None
 */
void lim_send_set_dtim_period(tpAniSirGlobal mac_ctx, uint8_t dtim_period,
			      tpPESession session)
{
	struct set_dtim_params *dtim_params = NULL;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};

	if (!session) {
		pe_err("Inavalid parameters");
		return;
	}
	dtim_params = qdf_mem_malloc(sizeof(*dtim_params));
	if (!dtim_params) {
		pe_err("Unable to allocate memory");
		return;
	}
	dtim_params->dtim_period = dtim_period;
	dtim_params->session_id = session->smeSessionId;
	msg.type = WMA_SET_DTIM_PERIOD;
	msg.bodyptr = dtim_params;
	msg.bodyval = 0;
	pe_debug("Post WMA_SET_DTIM_PERIOD to WMA");
	ret = wma_post_ctrl_msg(mac_ctx, &msg);
	if (QDF_STATUS_SUCCESS != ret) {
		pe_err("wma_post_ctrl_msg() failed");
		qdf_mem_free(dtim_params);
	}
}

/**
 * lim_is_valid_frame(): validate RX frame using last processed frame details
 * to find if it is duplicate frame.
 *
 * @last_processed_frm: last processed frame pointer.
 * @pRxPacketInfo: RX packet.
 *
 * Frame treat as duplicate:
 * if retry bit is set and
 *	 if source address and seq number matches with the last processed frame
 *
 * Return: false if duplicate frame, else true.
 */
bool lim_is_valid_frame(last_processed_msg *last_processed_frm,
		uint8_t *pRxPacketInfo)
{
	uint16_t seq_num;
	tpSirMacMgmtHdr pHdr;

	if (!pRxPacketInfo) {
		pe_err("Invalid RX frame");
		return false;
	}

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);

	if (pHdr->fc.retry == 0)
		return true;

	seq_num = (((pHdr->seqControl.seqNumHi <<
			HIGH_SEQ_NUM_OFFSET) |
			pHdr->seqControl.seqNumLo));

	if (last_processed_frm->seq_num == seq_num &&
		qdf_mem_cmp(last_processed_frm->sa, pHdr->sa, ETH_ALEN) == 0) {
		pe_err("Duplicate frame from "MAC_ADDRESS_STR " Seq Number %d",
		MAC_ADDR_ARRAY(pHdr->sa), seq_num);
		return false;
	}
	return true;
}

/**
 * lim_update_last_processed_frame(): update new processed frame info to cache.
 *
 * @last_processed_frm: last processed frame pointer.
 * @pRxPacketInfo: Successfully processed RX packet.
 *
 * Return: None.
 */
void lim_update_last_processed_frame(last_processed_msg *last_processed_frm,
		uint8_t *pRxPacketInfo)
{
	uint16_t seq_num;
	tpSirMacMgmtHdr pHdr;

	if (!pRxPacketInfo) {
		pe_err("Invalid RX frame");
		return;
	}

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	seq_num = (((pHdr->seqControl.seqNumHi <<
			HIGH_SEQ_NUM_OFFSET) |
			pHdr->seqControl.seqNumLo));

	qdf_mem_copy(last_processed_frm->sa, pHdr->sa, ETH_ALEN);
	last_processed_frm->seq_num = seq_num;
}

#ifdef WLAN_FEATURE_11AX
void lim_add_he_cap(tpAddStaParams add_sta_params, tpSirAssocReq assoc_req)
{
	if (!add_sta_params->he_capable || !assoc_req)
		return;

	qdf_mem_copy(&add_sta_params->he_config, &assoc_req->he_cap,
		     sizeof(add_sta_params->he_config));
}

void lim_add_self_he_cap(tpAddStaParams add_sta_params, tpPESession session)
{
	if (!session)
		return;

	add_sta_params->he_capable = true;

	qdf_mem_copy(&add_sta_params->he_config, &session->he_config,
		     sizeof(add_sta_params->he_config));
	qdf_mem_copy(&add_sta_params->he_op, &session->he_op,
		     sizeof(add_sta_params->he_op));
}

/**
 * lim_intersect_he_caps() - Intersect peer capability and self capability
 * @rcvd_he: pointer to received peer capability
 * @session_he: pointer to self capability
 * @peer_he: pointer to Intersected capability
 *
 * Return: None
 */
static void lim_intersect_he_caps(tDot11fIEhe_cap *rcvd_he,
			tDot11fIEhe_cap *session_he,
			tDot11fIEhe_cap *peer_he)
{
	uint8_t val;

	qdf_mem_copy(peer_he, rcvd_he, sizeof(*peer_he));

	peer_he->fragmentation &= session_he->fragmentation;

	if (session_he->tx_stbc_lt_80mhz && peer_he->rx_stbc_lt_80mhz)
		peer_he->rx_stbc_lt_80mhz = 1;
	else
		peer_he->rx_stbc_lt_80mhz = 0;

	if (session_he->rx_stbc_lt_80mhz && peer_he->tx_stbc_lt_80mhz)
		peer_he->tx_stbc_lt_80mhz = 1;
	else
		peer_he->tx_stbc_lt_80mhz = 0;

	if (session_he->tx_stbc_gt_80mhz && peer_he->rx_stbc_gt_80mhz)
		peer_he->rx_stbc_gt_80mhz = 1;
	else
		peer_he->rx_stbc_gt_80mhz = 0;

	if (session_he->rx_stbc_gt_80mhz && peer_he->tx_stbc_gt_80mhz)
		peer_he->tx_stbc_gt_80mhz = 1;
	else
		peer_he->tx_stbc_gt_80mhz = 0;

	/* Tx Doppler is first bit and Rx Doppler is second bit */
	if (session_he->doppler) {
		val = 0;
		if ((session_he->doppler & 0x1) && (peer_he->doppler & 0x10))
			val |= (1 << 1);
		if ((session_he->doppler & 0x10) && (peer_he->doppler & 0x1))
			val |= (1 << 0);
		peer_he->doppler = val;
	}

	peer_he->su_beamformer = session_he->su_beamformee ?
					peer_he->su_beamformer : 0;
	peer_he->su_beamformee = (session_he->su_beamformer ||
				  session_he->mu_beamformer) ?
					peer_he->su_beamformee : 0;
	peer_he->mu_beamformer = session_he->su_beamformee ?
					peer_he->mu_beamformer : 0;

	peer_he->twt_request = session_he->twt_responder ?
					peer_he->twt_request : 0;
	peer_he->twt_responder = session_he->twt_request ?
					peer_he->twt_responder : 0;
}

void lim_intersect_sta_he_caps(tpSirAssocReq assoc_req, tpPESession session,
		tpDphHashNode sta_ds)
{
	tDot11fIEhe_cap *rcvd_he = &assoc_req->he_cap;
	tDot11fIEhe_cap *session_he = &session->he_config;
	tDot11fIEhe_cap *peer_he = &sta_ds->he_config;

	if (sta_ds->mlmStaContext.he_capable)
		lim_intersect_he_caps(rcvd_he, session_he, peer_he);
}

void lim_intersect_ap_he_caps(tpPESession session, tpAddBssParams add_bss,
		tSchBeaconStruct *beacon, tpSirAssocRsp assoc_rsp)
{
	tDot11fIEhe_cap *rcvd_he;
	tDot11fIEhe_cap *session_he = &session->he_config;
	tDot11fIEhe_cap *peer_he = &add_bss->staContext.he_config;

	if (beacon)
		rcvd_he = &beacon->he_cap;
	else
		rcvd_he = &assoc_rsp->he_cap;

	lim_intersect_he_caps(rcvd_he, session_he, peer_he);
	add_bss->staContext.he_capable = true;
}

void lim_add_bss_he_cap(tpAddBssParams add_bss, tpSirAssocRsp assoc_rsp)
{
	tDot11fIEhe_cap *he_cap;
	tDot11fIEhe_op *he_op;

	he_cap = &assoc_rsp->he_cap;
	he_op = &assoc_rsp->he_op;
	add_bss->he_capable = he_cap->present;
	if (he_cap)
		qdf_mem_copy(&add_bss->staContext.he_config,
			     he_cap, sizeof(*he_cap));
	if (he_op)
		qdf_mem_copy(&add_bss->staContext.he_op,
			     he_op, sizeof(*he_op));
}

void lim_add_bss_he_cfg(tpAddBssParams add_bss, tpPESession session)
{
	add_bss->he_sta_obsspd = session->he_sta_obsspd;
}

void lim_update_stads_he_caps(tpDphHashNode sta_ds, tpSirAssocRsp assoc_rsp,
			      tpPESession session_entry)
{
	tDot11fIEhe_cap *he_cap;

	he_cap = &assoc_rsp->he_cap;
	sta_ds->mlmStaContext.he_capable = he_cap->present;

	if (!he_cap->present)
		return;

	qdf_mem_copy(&sta_ds->he_config, he_cap, sizeof(*he_cap));

}

void lim_update_usr_he_cap(tpAniSirGlobal mac_ctx, tpPESession session)
{
	tSirAddIeParams *add_ie = &session->addIeParams;
	tDot11fIEhe_cap *he_cap = &session->he_config;
	struct he_cap_network_endian *he_cap_from_ie;
	uint8_t extracted_buff[DOT11F_IE_HE_CAP_MAX_LEN + 2];
	QDF_STATUS status;
	qdf_mem_zero(extracted_buff, sizeof(extracted_buff));
	status = lim_strip_ie(mac_ctx, add_ie->probeRespBCNData_buff,
			&add_ie->probeRespBCNDataLen,
			DOT11F_EID_HE_CAP, ONE_BYTE,
			HE_CAP_OUI_TYPE, (uint8_t)HE_CAP_OUI_SIZE,
			extracted_buff, DOT11F_IE_HE_CAP_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_debug("Failed to strip HE cap IE status: %d", status);
		return;
	}

	pe_debug("Before update: su_beamformer: %d, su_beamformee: %d, mu_beamformer: %d",
		he_cap->su_beamformer, he_cap->su_beamformee, he_cap->mu_beamformer);

	he_cap_from_ie = (struct he_cap_network_endian *)
					&extracted_buff[HE_CAP_OUI_SIZE + 2];

	he_cap->su_beamformer =
		he_cap->su_beamformer & he_cap_from_ie->su_beamformer;
	he_cap->su_beamformee =
		he_cap->su_beamformee & he_cap_from_ie->su_beamformee;
	he_cap->mu_beamformer =
		he_cap->mu_beamformer & he_cap_from_ie->mu_beamformer;

	pe_debug("After update: su_beamformer: %d, su_beamformee: %d, mu_beamformer: %d",
		he_cap->su_beamformer, he_cap->su_beamformee, he_cap->mu_beamformer);
}

void lim_decide_he_op(tpAniSirGlobal mac_ctx, tpAddBssParams add_bss,
		      tpPESession session)
{
	uint32_t val;
	uint8_t color;
	struct he_ops_network_endian *he_ops_from_ie;
	tDot11fIEhe_op *he_ops = &add_bss->he_op;
	tSirAddIeParams *add_ie = &session->addIeParams;
	uint8_t extracted_buff[DOT11F_IE_HE_OP_MAX_LEN + 2];
	QDF_STATUS status;

	qdf_mem_zero(extracted_buff, sizeof(extracted_buff));
	status = lim_strip_ie(mac_ctx, add_ie->probeRespBCNData_buff,
			&add_ie->probeRespBCNDataLen,
			DOT11F_EID_HE_OP, ONE_BYTE,
			HE_OP_OUI_TYPE, (uint8_t)HE_OP_OUI_SIZE,
			extracted_buff, DOT11F_IE_HE_OP_MAX_LEN);
	if (QDF_STATUS_SUCCESS != status) {
		pe_debug("Failed to strip HE OP IE status: %d", status);
		return;
	}
	he_ops_from_ie = (struct he_ops_network_endian *)
					&extracted_buff[HE_OP_OUI_SIZE + 2];

	if (he_ops_from_ie->bss_color) {
		he_ops->bss_color = he_ops_from_ie->bss_color;
	} else {
		cds_rand_get_bytes(0, &color, 1);
		/* make sure color is within 1-63*/
		he_ops->bss_color = (color % WNI_CFG_HE_OPS_BSS_COLOR_MAX) + 1;
	}
	he_ops->default_pe = he_ops_from_ie->default_pe;
	he_ops->twt_required = he_ops_from_ie->twt_required;
	he_ops->rts_threshold = he_ops_from_ie->rts_threshold;
	he_ops->partial_bss_col = he_ops_from_ie->partial_bss_col;
	he_ops->tx_bssid_ind = he_ops_from_ie->tx_bssid_ind;
	he_ops->bss_col_disabled = he_ops_from_ie->bss_col_disabled;

	if (QDF_STATUS_SUCCESS != wlan_cfg_get_int(mac_ctx,
			WNI_CFG_HE_OPS_BASIC_MCS_NSS, &val))
		val = WNI_CFG_HE_OPS_BASIC_MCS_NSS_DEF;
	*((uint16_t *)he_ops->basic_mcs_nss) = (uint16_t)val;

	qdf_mem_copy(&session->he_op, he_ops, sizeof(*he_ops));

	pe_debug("HE Op: bss_color: 0x%0x, default_pe_duration: 0x%0x",
		he_ops->bss_color, he_ops->default_pe);
	pe_debug("He Op: twt_required: 0x%0x, rts_threshold: 0x%0x",
		he_ops->twt_required, he_ops->rts_threshold);
	pe_debug("HE Op: partial_bss_color: 0x%0x, Tx BSSID Indicator: 0x%0x",
		he_ops->partial_bss_col, he_ops->tx_bssid_ind);
	pe_debug("HE Op: BSS color disabled: 0x%0x",
		he_ops->bss_col_disabled);
	pe_debug("HE Op: Basic MCS NSS: 0x%04x",
		*((uint16_t *)he_ops->basic_mcs_nss));
}

void lim_copy_bss_he_cap(tpPESession session,
			 tpSirSmeStartBssReq sme_start_bss_req)
{
	qdf_mem_copy(&(session->he_config), &(sme_start_bss_req->he_config),
		     sizeof(session->he_config));
}

void lim_copy_join_req_he_cap(tpPESession session,
			      tpSirSmeJoinReq sme_join_req)
{
	qdf_mem_copy(&(session->he_config), &(sme_join_req->he_config),
		     sizeof(session->he_config));

	if (session->ch_width <= CH_WIDTH_80MHZ) {
		*(uint16_t *)session->he_config.rx_he_mcs_map_160 =
							HE_MCS_ALL_DISABLED;
		*(uint16_t *)session->he_config.tx_he_mcs_map_160 =
							HE_MCS_ALL_DISABLED;
		*(uint16_t *)session->he_config.rx_he_mcs_map_80_80 =
							HE_MCS_ALL_DISABLED;
		*(uint16_t *)session->he_config.tx_he_mcs_map_80_80 =
							HE_MCS_ALL_DISABLED;
	}
}

void lim_log_he_cap(tpAniSirGlobal mac, tDot11fIEhe_cap *he_cap)
{
	uint8_t chan_width;
	struct ppet_hdr *hdr;

	if (!he_cap->present)
		return;

	pe_nofl_debug("HE Capabilities: htc_he 0x%x twt_req 0x%x twt_res 0x%x fragmentation 0x%x max frag msdu amsdu 0x%x min frag 0x%x",
		      he_cap->htc_he, he_cap->twt_request,
		      he_cap->twt_responder, he_cap->fragmentation,
		      he_cap->max_num_frag_msdu,
		      he_cap->min_frag_size);
	pe_nofl_debug("\ttrig frm mac pad 0x%x multi tid aggr supp 0x%x link adaptaion 0x%x all ack 0x%x trigd_rsp_sched 0x%x a_bsr 0x%x",
		      he_cap->trigger_frm_mac_pad,
		      he_cap->multi_tid_aggr,
		      he_cap->he_link_adaptation, he_cap->all_ack,
		      he_cap->ul_mu_rsp_sched, he_cap->a_bsr);
	pe_nofl_debug("\tBC twt 0x%x ba_32bit_bitmap supp 0x%x mu_cascade 0x%x ack_enabled_multitid 0x%x omi_a_ctrl 0x%x ofdma_ra 0x%x",
		      he_cap->broadcast_twt, he_cap->ba_32bit_bitmap,
		      he_cap->mu_cascade, he_cap->ack_enabled_multitid,
		      he_cap->omi_a_ctrl, he_cap->ofdma_ra);
	pe_nofl_debug("\tmax_ampdu_len exp ext 0x%x amsdu_frag 0x%x flex_twt_sched 0x%x rx_ctrl frm 0x%x bsrp_ampdu_aggr 0x%x qtp 0x%x a_bqr 0x%x",
		      he_cap->max_ampdu_len, he_cap->amsdu_frag,
		      he_cap->flex_twt_sched, he_cap->rx_ctrl_frame,
		      he_cap->bsrp_ampdu_aggr, he_cap->qtp, he_cap->a_bqr);
	pe_nofl_debug("\tSR Reponder 0x%x ndp_feedback 0x%x ops_supp 0x%x amsdu_in_ampdu 0x%x",
		      he_cap->sr_responder,
		      he_cap->ndp_feedback_supp,
		      he_cap->ops_supp, he_cap->amsdu_in_ampdu);

	chan_width = HE_CH_WIDTH_COMBINE(he_cap->chan_width_0,
			he_cap->chan_width_1, he_cap->chan_width_2,
			he_cap->chan_width_3, he_cap->chan_width_4,
			he_cap->chan_width_5, he_cap->chan_width_6);

	pe_nofl_debug("\tchan width %d rx_pream_puncturing 0x%x device_class 0x%x ldpc_coding 0x%x 1x_ltf_800_gi_ppdu 0x%x midamble_tx_rx_max_nsts 0x%x",
		      chan_width, he_cap->rx_pream_puncturing,
		      he_cap->device_class,
		      he_cap->ldpc_coding, he_cap->he_1x_ltf_800_gi_ppdu,
		      he_cap->midamble_rx_max_nsts);

	pe_nofl_debug("\t4x_ltf_3200_gi_ndp 0x%x tb_ppdu_tx_stbc_lt_80mhz 0x%x rx_stbc_lt_80mhz 0x%x doppler 0x%x ul_mu 0x%x dcm_enc_tx 0x%x dcm_enc_rx 0x%x",
		      he_cap->he_4x_ltf_3200_gi_ndp,
		      he_cap->tx_stbc_lt_80mhz,
		      he_cap->rx_stbc_lt_80mhz, he_cap->doppler, he_cap->ul_mu,
		      he_cap->dcm_enc_tx, he_cap->dcm_enc_rx);

	pe_nofl_debug("\tul_he_mu 0x%x su_bfer 0x%x su_fee 0x%x mu_bfer 0x%x bfee_sts_lt_80 0x%x bfee_sts_gt_80 0x%x num_sd_lt_80 0x%x num_sd_gt_80 0x%x",
		      he_cap->ul_he_mu, he_cap->su_beamformer,
		      he_cap->su_beamformee,
		      he_cap->mu_beamformer, he_cap->bfee_sts_lt_80,
		      he_cap->bfee_sts_gt_80, he_cap->num_sounding_lt_80,
		      he_cap->num_sounding_gt_80);

	pe_nofl_debug("\tsu_fb_tone16 0x%x mu_fb_tone16 0x%x codebook_su 0x%x codebook_mu 0x%x bforming_feedback 0x%x he_er_su_ppdu 0x%x dl_mu_mimo_part_bw 0x%x",
		      he_cap->su_feedback_tone16, he_cap->mu_feedback_tone16,
		      he_cap->codebook_su, he_cap->codebook_mu,
		      he_cap->beamforming_feedback, he_cap->he_er_su_ppdu,
		      he_cap->dl_mu_mimo_part_bw);

	pe_nofl_debug("\tppet_present 0x%x srp 0x%x power_boost 0x%x ltf_800_gi_4x 0x%x max_nc 0x%x",
		      he_cap->ppet_present, he_cap->srp,
		      he_cap->power_boost, he_cap->he_ltf_800_gi_4x,
		      he_cap->max_nc);

	pe_nofl_debug("\ter_ltf_800_gi_4x 0x%x ppdu_20_in_40Mhz_2G 0x%x ppdu_20_in_160_80p80Mhz 0x%x ppdu_80_in_160_80p80Mhz 0x%x er_1x_ltf_gi 0x%x",
		      he_cap->er_he_ltf_800_gi_4x,
		      he_cap->he_ppdu_20_in_40Mhz_2G,
		      he_cap->he_ppdu_20_in_160_80p80Mhz,
		      he_cap->he_ppdu_80_in_160_80p80Mhz,
		      he_cap->er_1x_he_ltf_gi);

	pe_nofl_debug("\tmidamble_rx_1x_he_ltf 0x%x rx_he_mcs_map_lt_80 0x%x tx_he_mcs_map_lt_80 0x%x",
		      he_cap->midamble_rx_1x_he_ltf,
		      he_cap->rx_he_mcs_map_lt_80,
		      he_cap->tx_he_mcs_map_lt_80);

	hdr = (struct ppet_hdr *)&he_cap->ppet;
	pe_nofl_debug("\tRx MCS map 160 Mhz: 0x%x Tx MCS map 160 Mhz: 0x%x Rx MCS map 80+80 Mhz: 0x%x Tx MCS map 80+80 Mhz: 0x%x ppe_th:: nss_count: %d, ru_idx_msk: %d",
		      *((uint16_t *)he_cap->rx_he_mcs_map_160),
		      *((uint16_t *)he_cap->tx_he_mcs_map_160),
		      *((uint16_t *)he_cap->rx_he_mcs_map_80_80),
		      *((uint16_t *)he_cap->tx_he_mcs_map_80_80),
		      hdr->nss, hdr->ru_idx_mask);

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
		&he_cap->ppet, HE_MAX_PPET_SIZE);
}

void lim_log_he_op(tpAniSirGlobal mac, tDot11fIEhe_op *he_ops)
{
	pe_debug("bss_color 0x%x, default_pe_dur 0x%x, twt_req  0x%x, txop_rts_thres  0x%x, vht_op 0x%x",
		 he_ops->bss_color, he_ops->default_pe,
		 he_ops->twt_required, he_ops->rts_threshold,
		 he_ops->vht_oper_present);
	pe_debug("\tpart_bss_color 0x%x, mbssid_ap 0x%x, BSS color dis 0x%x basic mcs nss: 0x%x",
		 he_ops->partial_bss_col, he_ops->mbssid_ap,
		 he_ops->bss_col_disabled,
		 *((uint16_t *)he_ops->basic_mcs_nss));

	if (he_ops->vht_oper_present)
		pe_debug("VHT Info not present in HE Operation");
	else
		pe_debug("VHT Info: chan_width: %d, center_freq0: %d, center_freq1: %d",
			he_ops->vht_oper.info.chan_width,
			he_ops->vht_oper.info.center_freq_seg0,
			he_ops->vht_oper.info.center_freq_seg1);
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
void lim_log_he_bss_color(tpAniSirGlobal mac,
				tDot11fIEbss_color_change *he_bss_color)
{
	pe_debug("countdown: %d, new_color: %d",
			he_bss_color->countdown, he_bss_color->new_color);
}
#endif

void lim_update_sta_he_capable(tpAniSirGlobal mac,
	tpAddStaParams add_sta_params, tpDphHashNode sta_ds,
	tpPESession session_entry)
{
	if (LIM_IS_AP_ROLE(session_entry) || LIM_IS_IBSS_ROLE(session_entry))
		add_sta_params->he_capable = sta_ds->mlmStaContext.he_capable;
	else
		add_sta_params->he_capable = session_entry->he_capable;

	pe_debug("he_capable: %d", add_sta_params->he_capable);
}

void lim_update_bss_he_capable(tpAniSirGlobal mac, tpAddBssParams add_bss)
{
	add_bss->he_capable = true;
	pe_debug("he_capable: %d", add_bss->he_capable);
}

void lim_update_stads_he_capable(tpDphHashNode sta_ds, tpSirAssocReq assoc_req)
{
	sta_ds->mlmStaContext.he_capable = assoc_req->he_cap.present;
}

void lim_update_session_he_capable(tpAniSirGlobal mac, tpPESession session)
{
	session->he_capable = true;
	pe_debug("he_capable: %d", session->he_capable);
}

void lim_update_chan_he_capable(tpAniSirGlobal mac, tpSwitchChannelParams chan)
{
	chan->he_capable = true;
	pe_debug("he_capable: %d", chan->he_capable);
}

void lim_set_he_caps(tpAniSirGlobal mac, tpPESession session, uint8_t *ie_start,
		     uint32_t num_bytes)
{
	const uint8_t *ie = NULL;
	tDot11fIEhe_cap dot11_cap;
	struct he_capability_info *he_cap;

	populate_dot11f_he_caps(mac, session, &dot11_cap);
	lim_log_he_cap(mac, &dot11_cap);
	ie = wlan_get_ext_ie_ptr_from_ext_id(HE_CAP_OUI_TYPE,
			HE_CAP_OUI_SIZE, ie_start, num_bytes);
	if (ie) {
		/* convert from unpacked to packed structure */
		he_cap = (struct he_capability_info *) &ie[2 + HE_CAP_OUI_SIZE];

		he_cap->htc_he = dot11_cap.htc_he;
		he_cap->twt_request = dot11_cap.twt_request;
		he_cap->twt_responder = dot11_cap.twt_responder;
		he_cap->fragmentation = dot11_cap.fragmentation;
		he_cap->max_num_frag_msdu = dot11_cap.max_num_frag_msdu;
		he_cap->min_frag_size = dot11_cap.min_frag_size;
		he_cap->trigger_frm_mac_pad = dot11_cap.trigger_frm_mac_pad;
		he_cap->multi_tid_aggr = dot11_cap.multi_tid_aggr;
		he_cap->he_link_adaptation = dot11_cap.he_link_adaptation;
		he_cap->all_ack = dot11_cap.all_ack;
		he_cap->ul_mu_rsp_sched = dot11_cap.ul_mu_rsp_sched;
		he_cap->a_bsr = dot11_cap.a_bsr;
		he_cap->broadcast_twt = dot11_cap.broadcast_twt;
		he_cap->ba_32bit_bitmap = dot11_cap.ba_32bit_bitmap;
		he_cap->mu_cascade = dot11_cap.mu_cascade;
		he_cap->ack_enabled_multitid = dot11_cap.ack_enabled_multitid;
		he_cap->dl_mu_ba = dot11_cap.dl_mu_ba;
		he_cap->omi_a_ctrl = dot11_cap.omi_a_ctrl;
		he_cap->ofdma_ra = dot11_cap.ofdma_ra;
		he_cap->max_ampdu_len = dot11_cap.max_ampdu_len;
		he_cap->amsdu_frag = dot11_cap.amsdu_frag;
		he_cap->flex_twt_sched = dot11_cap.flex_twt_sched;
		he_cap->rx_ctrl_frame = dot11_cap.rx_ctrl_frame;

		he_cap->bsrp_ampdu_aggr = dot11_cap.bsrp_ampdu_aggr;
		he_cap->qtp = dot11_cap.qtp;
		he_cap->a_bqr = dot11_cap.a_bqr;
		he_cap->sr_responder = dot11_cap.sr_responder;
		he_cap->ops_supp = dot11_cap.ops_supp;
		he_cap->ndp_feedback_supp = dot11_cap.ndp_feedback_supp;
		he_cap->amsdu_in_ampdu = dot11_cap.amsdu_in_ampdu;
		he_cap->reserved1 = dot11_cap.reserved1;

		he_cap->dual_band = dot11_cap.dual_band;

		he_cap->chan_width = HE_CH_WIDTH_COMBINE(dot11_cap.chan_width_0,
				dot11_cap.chan_width_1, dot11_cap.chan_width_2,
				dot11_cap.chan_width_3, dot11_cap.chan_width_4,
				dot11_cap.chan_width_5, dot11_cap.chan_width_6);

		he_cap->rx_pream_puncturing = dot11_cap.rx_pream_puncturing;
		he_cap->device_class = dot11_cap.device_class;
		he_cap->ldpc_coding = dot11_cap.ldpc_coding;
		he_cap->he_1x_ltf_800_gi_ppdu = dot11_cap.he_1x_ltf_800_gi_ppdu;
		he_cap->midamble_rx_max_nsts = dot11_cap.midamble_rx_max_nsts;
		he_cap->he_4x_ltf_3200_gi_ndp = dot11_cap.he_4x_ltf_3200_gi_ndp;
		he_cap->tx_stbc_lt_80mhz = dot11_cap.tx_stbc_lt_80mhz;
		he_cap->rx_stbc_lt_80mhz = dot11_cap.rx_stbc_lt_80mhz;
		he_cap->tx_stbc_gt_80mhz = dot11_cap.tx_stbc_gt_80mhz;
		he_cap->rx_stbc_gt_80mhz = dot11_cap.rx_stbc_gt_80mhz;
		he_cap->doppler = dot11_cap.doppler;
		he_cap->ul_mu = dot11_cap.ul_mu;
		he_cap->dcm_enc_tx = dot11_cap.dcm_enc_tx;
		he_cap->dcm_enc_rx = dot11_cap.dcm_enc_rx;
		he_cap->ul_he_mu = dot11_cap.ul_he_mu;
		he_cap->su_beamformer = dot11_cap.su_beamformer;

		he_cap->su_beamformee = dot11_cap.su_beamformee;
		he_cap->mu_beamformer = dot11_cap.mu_beamformer;
		he_cap->bfee_sts_lt_80 = dot11_cap.bfee_sts_lt_80;
		he_cap->bfee_sts_gt_80 = dot11_cap.bfee_sts_gt_80;
		he_cap->num_sounding_lt_80 = dot11_cap.num_sounding_lt_80;
		he_cap->num_sounding_gt_80 = dot11_cap.num_sounding_gt_80;
		he_cap->su_feedback_tone16 = dot11_cap.su_feedback_tone16;
		he_cap->mu_feedback_tone16 = dot11_cap.mu_feedback_tone16;
		he_cap->codebook_su = dot11_cap.codebook_su;
		he_cap->codebook_mu = dot11_cap.codebook_mu;
		he_cap->beamforming_feedback = dot11_cap.beamforming_feedback;
		he_cap->he_er_su_ppdu = dot11_cap.he_er_su_ppdu;
		he_cap->dl_mu_mimo_part_bw = dot11_cap.dl_mu_mimo_part_bw;
		he_cap->ppet_present = dot11_cap.ppet_present;
		he_cap->srp = dot11_cap.srp;
		he_cap->power_boost = dot11_cap.power_boost;

		he_cap->he_ltf_800_gi_4x = dot11_cap.he_ltf_800_gi_4x;
		he_cap->max_nc = dot11_cap.max_nc;
		he_cap->er_he_ltf_800_gi_4x = dot11_cap.er_he_ltf_800_gi_4x;
		he_cap->he_ppdu_20_in_40Mhz_2G =
					dot11_cap.he_ppdu_20_in_40Mhz_2G;
		he_cap->he_ppdu_20_in_160_80p80Mhz =
					dot11_cap.he_ppdu_20_in_160_80p80Mhz;
		he_cap->he_ppdu_80_in_160_80p80Mhz =
					dot11_cap.he_ppdu_80_in_160_80p80Mhz;
		he_cap->er_1x_he_ltf_gi =
					dot11_cap.er_1x_he_ltf_gi;
		he_cap->midamble_rx_1x_he_ltf =
					dot11_cap.midamble_rx_1x_he_ltf;
		he_cap->reserved2 = dot11_cap.reserved2;

		he_cap->rx_he_mcs_map_lt_80 = dot11_cap.rx_he_mcs_map_lt_80;
		he_cap->tx_he_mcs_map_lt_80 = dot11_cap.tx_he_mcs_map_lt_80;
		he_cap->rx_he_mcs_map_160 =
				*((uint16_t *)dot11_cap.rx_he_mcs_map_160);
		he_cap->tx_he_mcs_map_160 =
				*((uint16_t *)dot11_cap.tx_he_mcs_map_160);
		he_cap->rx_he_mcs_map_80_80 =
				*((uint16_t *)dot11_cap.rx_he_mcs_map_80_80);
		he_cap->tx_he_mcs_map_80_80 =
				*((uint16_t *)dot11_cap.tx_he_mcs_map_80_80);
	}
}

QDF_STATUS lim_send_he_caps_ie(tpAniSirGlobal mac_ctx,
			       tpPESession session,
			       enum QDF_OPMODE device_mode,
			       uint8_t vdev_id)
{
	uint8_t he_caps[SIR_MAC_HE_CAP_MIN_LEN + 3];
	struct he_capability_info *he_cap;
	QDF_STATUS status_5g, status_2g;
	uint8_t he_cap_total_len = SIR_MAC_HE_CAP_MIN_LEN + HE_CAP_OUI_LEN +
				   HE_CAP_160M_MCS_MAP_LEN +
				   HE_CAP_80P80_MCS_MAP_LEN;

	/* Sending only minimal info(no PPET) to FW now, update if required */
	qdf_mem_zero(he_caps, SIR_MAC_HE_CAP_MIN_LEN + 3);
	he_caps[0] = DOT11F_EID_HE_CAP;
	he_caps[1] = SIR_MAC_HE_CAP_MIN_LEN;
	qdf_mem_copy(&he_caps[2], HE_CAP_OUI_TYPE, HE_CAP_OUI_SIZE);
	lim_set_he_caps(mac_ctx, session, he_caps,
			SIR_MAC_HE_CAP_MIN_LEN + 3);
	he_cap = (struct he_capability_info *) (&he_caps[2 + HE_CAP_OUI_SIZE]);
	he_cap->ppet_present = 0;
	if (device_mode == QDF_NDI_MODE) {
		he_cap->su_beamformee = 0;
		he_cap->su_beamformer = 0;
		he_cap->mu_beamformer = 0;
		he_cap->bfee_sts_gt_80 = 0;
		he_cap->bfee_sts_lt_80 = 0;
		he_cap->num_sounding_gt_80 = 0;
		he_cap->num_sounding_lt_80 = 0;
		he_cap->su_feedback_tone16 = 0;
		he_cap->mu_feedback_tone16 = 0;
	}
	status_5g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HE_CAP,
			CDS_BAND_5GHZ, &he_caps[2],
			SIR_MAC_HE_CAP_MIN_LEN + 1);
	if (QDF_IS_STATUS_ERROR(status_5g))
		pe_err("Unable send HE Cap IE for 5GHZ band, status: %d",
			status_5g);

	status_2g = lim_send_ie(mac_ctx, vdev_id, DOT11F_EID_HE_CAP,
			CDS_BAND_2GHZ, &he_caps[2],
			SIR_MAC_HE_CAP_MIN_LEN + 1);
	if (QDF_IS_STATUS_ERROR(status_2g))
		pe_err("Unable send HE Cap IE for 2GHZ band, status: %d",
			status_2g);

	if (QDF_IS_STATUS_SUCCESS(status_2g) &&
		QDF_IS_STATUS_SUCCESS(status_5g))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_populate_he_mcs_per_bw() - pouldate HE mcs set per BW (le 80, 160, 80+80)
 * @mac_ctx: Global MAC context
 * @self_rx: self rx mcs set
 * @self_tx: self tx mcs set
 * @peer_rx: peer rx mcs set
 * @peer_tx: peer tx mcs set
 * @nss: nss
 * @cfg_rx_param: rx wni param to read
 * @cfg_tx_param: tx wni param to read
 *
 * MCS values are interpreted as in IEEE 11ax-D1.4 spec onwards
 * +-----------------------------------------------------+
 * |  SS8  |  SS7  |  SS6  | SS5 | SS4 | SS3 | SS2 | SS1 |
 * +-----------------------------------------------------+
 * | 15-14 | 13-12 | 11-10 | 9-8 | 7-6 | 5-4 | 3-2 | 1-0 |
 * +-----------------------------------------------------+
 *
 * Return: status of operation
 */
static QDF_STATUS lim_populate_he_mcs_per_bw(tpAniSirGlobal mac_ctx,
				uint16_t *self_rx, uint16_t *self_tx,
				uint16_t peer_rx, uint16_t peer_tx, uint8_t nss,
				uint32_t cfg_rx_param, uint32_t cfg_tx_param)
{
	uint32_t val;

	pe_debug("peer rates: rx_mcs - 0x%04x tx_mcs - 0x%04x",
		 peer_rx, peer_tx);
	if (wlan_cfg_get_int(mac_ctx, cfg_rx_param, &val) != QDF_STATUS_SUCCESS) {
		pe_err("could not retrieve HE_MCS");
		return QDF_STATUS_E_FAILURE;
	}
	*self_rx = (uint16_t) val;
	if (wlan_cfg_get_int(mac_ctx, cfg_tx_param, &val) != QDF_STATUS_SUCCESS) {
		pe_err("could not retrieve HE_MCS");
		return QDF_STATUS_E_FAILURE;
	}
	*self_tx = (uint16_t) val;

	*self_rx = HE_INTERSECT_MCS(*self_rx, peer_tx);
	*self_tx = HE_INTERSECT_MCS(*self_tx, peer_rx);

	if (nss == NSS_1x1_MODE) {
		*self_rx |= HE_MCS_INV_MSK_4_NSS(1);
		*self_tx |= HE_MCS_INV_MSK_4_NSS(1);
	}
	/* if nss is 2, disable higher NSS */
	if (nss == NSS_2x2_MODE) {
		*self_rx |= (HE_MCS_INV_MSK_4_NSS(1) & HE_MCS_INV_MSK_4_NSS(2));
		*self_tx |= (HE_MCS_INV_MSK_4_NSS(1) & HE_MCS_INV_MSK_4_NSS(2));
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_populate_he_mcs_set(tpAniSirGlobal mac_ctx,
				   tpSirSupportedRates rates,
				   tDot11fIEhe_cap *peer_he_caps,
				   tpPESession session_entry, uint8_t nss)
{
	bool support_2x2 = false;
	uint32_t self_sta_dot11mode = 0;

	wlan_cfg_get_int(mac_ctx, WNI_CFG_DOT11_MODE, &self_sta_dot11mode);

	if (!IS_DOT11_MODE_HE(self_sta_dot11mode))
		return QDF_STATUS_SUCCESS;

	if ((peer_he_caps == NULL) || (!peer_he_caps->present)) {
		pe_debug("peer not he capable or he_caps NULL");
		return QDF_STATUS_SUCCESS;
	}

	pe_debug("PEER: lt 80: rx 0x%04x tx 0x%04x, 160: rx 0x%04x tx 0x%04x, 80+80: rx 0x%04x tx 0x%04x",
		peer_he_caps->rx_he_mcs_map_lt_80,
		peer_he_caps->tx_he_mcs_map_lt_80,
		(*(uint16_t *)peer_he_caps->rx_he_mcs_map_160),
		(*(uint16_t *)peer_he_caps->tx_he_mcs_map_160),
		(*(uint16_t *)peer_he_caps->rx_he_mcs_map_80_80),
		(*(uint16_t *)peer_he_caps->tx_he_mcs_map_80_80));

	if (session_entry && session_entry->nss == NSS_2x2_MODE) {
		if (mac_ctx->lteCoexAntShare &&
			IS_24G_CH(session_entry->currentOperChannel)) {
			if (IS_2X2_CHAIN(session_entry->chainMask))
				support_2x2 = true;
			else
				pe_err("2x2 not enabled %d",
					session_entry->chainMask);
		} else {
			support_2x2 = true;
		}
	}

	lim_populate_he_mcs_per_bw(mac_ctx,
		&rates->rx_he_mcs_map_lt_80, &rates->tx_he_mcs_map_lt_80,
		peer_he_caps->rx_he_mcs_map_lt_80,
		peer_he_caps->tx_he_mcs_map_lt_80, nss,
		WNI_CFG_HE_RX_MCS_MAP_LT_80, WNI_CFG_HE_TX_MCS_MAP_LT_80);
	lim_populate_he_mcs_per_bw(mac_ctx,
		&rates->rx_he_mcs_map_160, &rates->tx_he_mcs_map_160,
		*((uint16_t *)peer_he_caps->rx_he_mcs_map_160),
		*((uint16_t *)peer_he_caps->tx_he_mcs_map_160), nss,
		WNI_CFG_HE_RX_MCS_MAP_160, WNI_CFG_HE_TX_MCS_MAP_160);
	lim_populate_he_mcs_per_bw(mac_ctx,
		&rates->rx_he_mcs_map_80_80, &rates->tx_he_mcs_map_80_80,
		*((uint16_t *)peer_he_caps->rx_he_mcs_map_80_80),
		*((uint16_t *)peer_he_caps->tx_he_mcs_map_80_80), nss,
		WNI_CFG_HE_RX_MCS_MAP_80_80, WNI_CFG_HE_TX_MCS_MAP_80_80);
	if (!support_2x2) {
		/* disable 2 and higher NSS MCS sets */
		rates->rx_he_mcs_map_lt_80 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->tx_he_mcs_map_lt_80 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->rx_he_mcs_map_160 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->tx_he_mcs_map_160 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->rx_he_mcs_map_80_80 |= HE_MCS_INV_MSK_4_NSS(1);
		rates->tx_he_mcs_map_80_80 |= HE_MCS_INV_MSK_4_NSS(1);
	}

	pe_debug("lt 80: rx 0x%x tx 0x%x, 160: rx 0x%x tx 0x%x, 80_80: rx 0x%x tx 0x%x",
		 rates->rx_he_mcs_map_lt_80, rates->tx_he_mcs_map_lt_80,
		 rates->rx_he_mcs_map_160, rates->tx_he_mcs_map_160,
		 rates->rx_he_mcs_map_80_80, rates->tx_he_mcs_map_80_80);

	return QDF_STATUS_SUCCESS;
}
#endif

int
lim_assoc_rej_get_remaining_delta(struct sir_rssi_disallow_lst *node)
{
	qdf_time_t cur_time;
	uint32_t time_diff;

	cur_time = qdf_do_div(qdf_get_monotonic_boottime(),
				QDF_MC_TIMER_TO_MS_UNIT);
	time_diff = cur_time - node->time_during_rejection;

	return node->retry_delay - time_diff;
}

QDF_STATUS
lim_rem_blacklist_entry_with_lowest_delta(qdf_list_t *list)
{
	struct sir_rssi_disallow_lst *oldest_node = NULL;
	struct sir_rssi_disallow_lst *cur_node;
	qdf_list_node_t *cur_list = NULL;
	qdf_list_node_t *next_list = NULL;

	qdf_list_peek_front(list, &cur_list);
	while (cur_list) {
		cur_node = qdf_container_of(cur_list,
			struct sir_rssi_disallow_lst, node);
		if (!oldest_node ||
		   (lim_assoc_rej_get_remaining_delta(oldest_node) >
		    lim_assoc_rej_get_remaining_delta(cur_node)))
			oldest_node = cur_node;

		qdf_list_peek_next(list, cur_list, &next_list);
		cur_list = next_list;
		next_list = NULL;
	}

	if (oldest_node) {
		pe_debug("remove node %pM with lowest delta %d",
			oldest_node->bssid.bytes,
			lim_assoc_rej_get_remaining_delta(oldest_node));
		qdf_list_remove_node(list, &oldest_node->node);
		qdf_mem_free(oldest_node);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_INVAL;
}

void lim_assoc_rej_add_to_rssi_based_reject_list(tpAniSirGlobal mac_ctx,
					struct sir_rssi_disallow_lst *ap_info)
{
	struct sir_rssi_disallow_lst *entry;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	entry = qdf_mem_malloc(sizeof(*entry));
	if (!entry) {
		pe_err("malloc failed for bssid entry");
		return;
	}

	pe_debug("%pM: assoc resp, expected rssi %d retry delay %d sec and list size %d",
		 ap_info->bssid.bytes, ap_info->expected_rssi,
		 ap_info->retry_delay,
		 qdf_list_size(&mac_ctx->roam.rssi_disallow_bssid));

	*entry = *ap_info;
	entry->time_during_rejection =
		qdf_do_div(qdf_get_monotonic_boottime(),
		QDF_MC_TIMER_TO_MS_UNIT);

	qdf_mutex_acquire(&mac_ctx->roam.rssi_disallow_bssid_lock);
	if (qdf_list_size(&mac_ctx->roam.rssi_disallow_bssid) >=
		MAX_RSSI_AVOID_BSSID_LIST) {
		status = lim_rem_blacklist_entry_with_lowest_delta(
					&mac_ctx->roam.rssi_disallow_bssid);
		if (QDF_IS_STATUS_ERROR(status))
			pe_err("Failed to remove entry with lowest delta");
	}

	if (QDF_IS_STATUS_SUCCESS(status))
		status = qdf_list_insert_back(
				&mac_ctx->roam.rssi_disallow_bssid,
				&entry->node);
	qdf_mutex_release(&mac_ctx->roam.rssi_disallow_bssid_lock);

	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to enqueue bssid entry");
		qdf_mem_free(entry);
	}
}

void lim_decrement_pending_mgmt_count(tpAniSirGlobal mac_ctx)
{
	qdf_spin_lock(&mac_ctx->sys.bbt_mgmt_lock);
	if (!mac_ctx->sys.sys_bbt_pending_mgmt_count) {
		qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
		return;
	}
	mac_ctx->sys.sys_bbt_pending_mgmt_count--;
	qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
}

struct csr_roam_session *lim_get_session_by_macaddr(tpAniSirGlobal mac_ctx,
		tSirMacAddr self_mac)
{
	int i = 0;
	struct csr_roam_session *session;

	if (!mac_ctx || !self_mac) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid arguments"));
		return NULL;
	}

	for (i = 0; i < mac_ctx->sme.max_intf_count; i++) {
		session = CSR_GET_SESSION(mac_ctx, i);
		if (!session)
			continue;
		else if (!qdf_mem_cmp(&session->selfMacAddr,
			 self_mac, sizeof(tSirMacAddr))) {

			QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_INFO,
				  FL("session %d exists with mac address "
				  MAC_ADDRESS_STR), session->sessionId,
				  MAC_ADDR_ARRAY(self_mac));

			return session;
		}
	}

	return NULL;
}

bool lim_check_if_vendor_oui_match(tpAniSirGlobal mac_ctx,
					uint8_t *oui, uint8_t oui_len,
			       uint8_t *ie, uint8_t ie_len)
{
	uint8_t *ptr = ie;
	uint8_t elem_id;

	if (NULL == ie || 0 == ie_len) {
		pe_err("IE Null or ie len zero %d", ie_len);
		return false;
	}

	elem_id = *ie;

	if (elem_id == IE_EID_VENDOR &&
		!qdf_mem_cmp(&ptr[2], oui, oui_len))
		return true;
	else
		return false;
}

QDF_STATUS lim_util_get_type_subtype(void *pkt, uint8_t *type,
						uint8_t *subtype)
{
	cds_pkt_t *cds_pkt;
	QDF_STATUS status;
	tpSirMacMgmtHdr hdr;
	uint8_t *rxpktinfor;

	cds_pkt = (cds_pkt_t *) pkt;
	if (!cds_pkt) {
		pe_err("NULL packet received");
		return QDF_STATUS_E_FAILURE;
	}
	status =
		wma_ds_peek_rx_packet_info(cds_pkt, (void *)&rxpktinfor, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("Failed extract cds packet. status %d", status);
		return QDF_STATUS_E_FAILURE;
	}

	hdr = WMA_GET_RX_MAC_HEADER(rxpktinfor);
	if (hdr->fc.type == SIR_MAC_MGMT_FRAME) {
		pe_debug("RxBd: %pK mHdr: %pK Type: %d Subtype: %d SizeFC: %zu",
				rxpktinfor, hdr, hdr->fc.type, hdr->fc.subType,
				sizeof(tSirMacFrameCtl));
		*type = hdr->fc.type;
		*subtype = hdr->fc.subType;
	} else {
		pe_err("Not a management packet type %d", hdr->fc.type);
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

enum rateid lim_get_min_session_txrate(tpPESession session)
{
	enum rateid rid = RATEID_DEFAULT;
	uint8_t min_rate = SIR_MAC_RATE_54, curr_rate, i;
	tSirMacRateSet *rateset = &session->rateSet;

	if (!session)
		return rid;

	for (i = 0; i < rateset->numRates; i++) {
		/* Ignore MSB - set to indicate basic rate */
		curr_rate = rateset->rate[i] & 0x7F;
		min_rate =  (curr_rate < min_rate) ? curr_rate : min_rate;
	}
	pe_debug("supported min_rate: %0x(%d)", min_rate, min_rate);

	switch (min_rate) {
	case SIR_MAC_RATE_1:
		rid = RATEID_1MBPS;
		break;
	case SIR_MAC_RATE_2:
		rid = RATEID_2MBPS;
		break;
	case SIR_MAC_RATE_5_5:
		rid = RATEID_5_5MBPS;
		break;
	case SIR_MAC_RATE_11:
		rid = RATEID_11MBPS;
		break;
	case SIR_MAC_RATE_6:
		rid = RATEID_6MBPS;
		break;
	case SIR_MAC_RATE_9:
		rid = RATEID_9MBPS;
		break;
	case SIR_MAC_RATE_12:
		rid = RATEID_12MBPS;
		break;
	case SIR_MAC_RATE_18:
		rid = RATEID_18MBPS;
		break;
	case SIR_MAC_RATE_24:
		rid = RATEID_24MBPS;
		break;
	case SIR_MAC_RATE_36:
		rid = RATEID_36MBPS;
		break;
	case SIR_MAC_RATE_48:
		rid = RATEID_48MBPS;
		break;
	case SIR_MAC_RATE_54:
		rid = RATEID_54MBPS;
		break;
	default:
		rid = RATEID_DEFAULT;
		break;
	}

	return rid;
}

void lim_send_sme_mgmt_frame_ind(tpAniSirGlobal mac_ctx, uint8_t frame_type,
				 uint8_t *frame, uint32_t frame_len,
				 uint16_t session_id, uint32_t rx_channel,
				 tpPESession psession_entry, int8_t rx_rssi,
				 enum rxmgmt_flags rx_flags)
{
	tpSirSmeMgmtFrameInd sme_mgmt_frame = NULL;
	uint16_t length;

	length = sizeof(tSirSmeMgmtFrameInd) + frame_len;

	sme_mgmt_frame = qdf_mem_malloc(length);
	if (!sme_mgmt_frame) {
		pe_err("AllocateMemory failed for eWNI_SME_LISTEN_RSP");
		return;
	}

	if (qdf_is_macaddr_broadcast(
		(struct qdf_mac_addr *)(frame + 4)) &&
		!session_id) {
		pe_debug("Broadcast action frame");
		session_id = SME_SESSION_ID_BROADCAST;
	}

	sme_mgmt_frame->frame_len = frame_len;
	sme_mgmt_frame->sessionId = session_id;
	sme_mgmt_frame->frameType = frame_type;
	sme_mgmt_frame->rxRssi = rx_rssi;
	sme_mgmt_frame->rxChan = rx_channel;
	sme_mgmt_frame->rx_flags = rx_flags;

	qdf_mem_zero(sme_mgmt_frame->frameBuf, frame_len);
	qdf_mem_copy(sme_mgmt_frame->frameBuf, frame, frame_len);

	if (mac_ctx->mgmt_frame_ind_cb)
		mac_ctx->mgmt_frame_ind_cb(sme_mgmt_frame);
	else
		pe_debug_rl("Management indication callback not registered!!");
	qdf_mem_free(sme_mgmt_frame);
	return;
}

void
lim_send_dfs_chan_sw_ie_update(tpAniSirGlobal mac_ctx, tpPESession session)
{
	/* Update the beacon template and send to FW */
	if (sch_set_fixed_beacon_fields(mac_ctx, session) !=
					QDF_STATUS_SUCCESS) {
		pe_err("Unable to set CSA IE in beacon");
		return;
	}

	/* Send update beacon template message */
	lim_send_beacon_ind(mac_ctx, session, REASON_CHANNEL_SWITCH);
	pe_debug("Updated CSA IE, IE COUNT: %d",
		 session->gLimChannelSwitch.switchCount);
}

void lim_process_ap_ecsa_timeout(void *data)
{
	tpPESession session = (tpPESession)data;
	tpAniSirGlobal mac_ctx;
	uint8_t bcn_int, ch, ch_width;
	QDF_STATUS status;

	if (!session || !session->valid) {
		pe_err("Session is not valid");
		return;
	}

	mac_ctx = (tpAniSirGlobal)session->mac_ctx;

	if (!session->dfsIncludeChanSwIe) {
		pe_debug("session->dfsIncludeChanSwIe not set");
		return;
	}

	if (session->gLimChannelSwitch.switchCount)
		/* Decrement the beacon switch count */
		session->gLimChannelSwitch.switchCount--;

	/*
	 * Send only g_sap_chanswitch_beacon_cnt beacons with CSA IE Set in
	 * when a radar is detected
	 */
	if (session->gLimChannelSwitch.switchCount > 0) {
		/* Send the next beacon with updated CSA IE count */
		lim_send_dfs_chan_sw_ie_update(mac_ctx, session);

		ch = session->gLimChannelSwitch.primaryChannel;
		ch_width = session->gLimChannelSwitch.ch_width;
		if (mac_ctx->sap.SapDfsInfo.dfs_beacon_tx_enhanced)
			/* Send Action frame after updating beacon */
			lim_send_chan_switch_action_frame(mac_ctx, ch, ch_width,
							  session);

		/* Restart the timer */
		if (session->beaconParams.beaconInterval)
			bcn_int = session->beaconParams.beaconInterval;
		else
			bcn_int = WNI_CFG_BEACON_INTERVAL_STADEF;

		status = qdf_mc_timer_start(&session->ap_ecsa_timer,
					    bcn_int);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("cannot start ap_ecsa_timer");
			lim_process_ap_ecsa_timeout(session);
		}
	} else {
		tSirSmeCSAIeTxCompleteRsp *chan_switch_tx_rsp;
		struct scheduler_msg msg = {0};
		uint8_t length = sizeof(*chan_switch_tx_rsp);

		/* Done with CSA IE update, send response back to SME */
		session->gLimChannelSwitch.switchCount = 0;
		if (mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch == false)
			session->gLimChannelSwitch.switchMode = 0;
		session->dfsIncludeChanSwIe = false;
		session->dfsIncludeChanWrapperIe = false;

		chan_switch_tx_rsp = qdf_mem_malloc(length);
		if (!chan_switch_tx_rsp) {
			pe_err("AllocateMemory failed for tSirSmeCSAIeTxCompleteRsp");
			return;
		}

		chan_switch_tx_rsp->sessionId = session->smeSessionId;
		chan_switch_tx_rsp->chanSwIeTxStatus = QDF_STATUS_SUCCESS;

		msg.type = eWNI_SME_DFS_CSAIE_TX_COMPLETE_IND;
		msg.bodyptr = chan_switch_tx_rsp;

		status = scheduler_post_message(QDF_MODULE_ID_PE,
						QDF_MODULE_ID_SME,
						QDF_MODULE_ID_SME, &msg);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("Failed to post eWNI_SME_DFS_CSAIE_TX_COMPLETE_IND");
			qdf_mem_free(chan_switch_tx_rsp);
		}
	}
}

struct wlan_ies *
hdd_get_self_disconnect_ies(tpAniSirGlobal mac_ctx, uint8_t vdev_id)
{
	struct wlan_ies *discon_ie = NULL;
	struct wlan_objmgr_vdev *vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						     vdev_id,
						     WLAN_MLME_SB_ID);
	if (!vdev) {
		pe_err("Got NULL vdev obj, returning");
		return NULL;
	}
	discon_ie = mlme_get_self_disconnect_ies(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

	return discon_ie;
}

void hdd_free_self_disconnect_ies(tpAniSirGlobal mac_ctx, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						     vdev_id,
						     WLAN_MLME_SB_ID);
	if (!vdev) {
		pe_err("Got NULL vdev obj, returning");
		return;
	}
	mlme_free_self_disconnect_ies(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

	return;
}

bool lim_is_sha384_akm(enum ani_akm_type akm)
{
	switch (akm) {
	case ANI_AKM_TYPE_FILS_SHA384:
	case ANI_AKM_TYPE_FT_FILS_SHA384:
	case ANI_AKM_TYPE_SUITEB_EAP_SHA384:
	case ANI_AKM_TYPE_FT_SUITEB_EAP_SHA384:
		return true;
	default:
		return false;
	}
}
