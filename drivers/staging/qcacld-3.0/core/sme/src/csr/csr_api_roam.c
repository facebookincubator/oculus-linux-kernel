/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: csr_api_roam.c
 *
 * Implementation for the Common Roaming interfaces.
 */
#include "ani_global.h"          /* for tpAniSirGlobal */
#include "wma_types.h"
#include "wma_if.h"          /* for STA_INVALID_IDX. */
#include "csr_inside_api.h"
#include "sme_trace.h"
#include "sme_qos_internal.h"
#include "sme_inside.h"
#include "host_diag_core_event.h"
#include "host_diag_core_log.h"
#include "csr_api.h"
#include "csr_internal.h"
#include "cds_reg_service.h"
#include "mac_trace.h"
#include "csr_neighbor_roam.h"
#include "cds_regdomain.h"
#include "cds_utils.h"
#include "sir_types.h"
#include "cfg_api.h"
#include "sme_power_save_api.h"
#include "wma.h"
#include "wlan_policy_mgr_api.h"
#include "sme_nan_datapath.h"
#include "pld_common.h"
#include "wlan_reg_services_api.h"
#include "qdf_crypto.h"
#include <wlan_logging_sock_svc.h>
#include "wlan_objmgr_psoc_obj.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_tdls_tgt_api.h>
#include <wlan_cfg80211_scan.h>
#include <wlan_scan_public_structs.h>
#include <wlan_action_oui_public_struct.h>
#include <wlan_action_oui_ucfg_api.h>
#include <wlan_utility.h>
#include "wlan_mlme_main.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_scan_utils_api.h"
#include <wlan_cp_stats_mc_ucfg_api.h>
#include "wlan_pkt_capture_ucfg_api.h"
#include "nan_ucfg_api.h"

#define RSN_AUTH_KEY_MGMT_SAE           WLAN_RSN_SEL(WLAN_AKM_SAE)
#define MAX_PWR_FCC_CHAN_12 8
#define MAX_PWR_FCC_CHAN_13 2

#define CSR_NUM_IBSS_START_CHAN_50      5
#define CSR_NUM_IBSS_START_CHANNELS_24      3
/* 70 seconds, for WPA, WPA2, CCKM */
#define CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD     \
	(SIR_INSTALL_KEY_TIMEOUT_SEC * QDF_MC_TIMER_TO_SEC_UNIT)
/* 120 seconds, for WPS */
#define CSR_WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD (120 * QDF_MC_TIMER_TO_SEC_UNIT)

/* OBIWAN recommends [8 10]% : pick 9% */
#define CSR_VCC_UL_MAC_LOSS_THRESHOLD 9
/* OBIWAN recommends -85dBm */
#define CSR_VCC_RSSI_THRESHOLD 80
#define CSR_MIN_GLOBAL_STAT_QUERY_PERIOD   500  /* ms */
#define CSR_MIN_GLOBAL_STAT_QUERY_PERIOD_IN_BMPS 2000   /* ms */
#define CSR_MIN_TL_STAT_QUERY_PERIOD       500  /* ms */

#define CSR_SINGLE_PMK_OUI               "\x00\x40\x96\x03"
#define CSR_SINGLE_PMK_OUI_SIZE          4

/* Flag to send/do not send disassoc frame over the air */
#define CSR_DONT_SEND_DISASSOC_OVER_THE_AIR 1
#define RSSI_HACK_BMPS (-40)
#define MAX_CB_VALUE_IN_INI (2)

#define MAX_SOCIAL_CHANNELS  3

/* packet dump timer duration of 60 secs */
#define PKT_DUMP_TIMER_DURATION 60

/* Choose the largest possible value that can be accommodated in 8 bit signed */
/* variable. */
#define SNR_HACK_BMPS                         (127)

/*
 * ROAMING_OFFLOAD_TIMER_START - Indicates the action to start the timer
 * ROAMING_OFFLOAD_TIMER_STOP - Indicates the action to stop the timer
 * CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD - Timeout value for roaming offload timer
 */
#define ROAMING_OFFLOAD_TIMER_START	1
#define ROAMING_OFFLOAD_TIMER_STOP	2
#define CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD    (5 * QDF_MC_TIMER_TO_SEC_UNIT)

/*
 * MAWC_ROAM_TRAFFIC_THRESHOLD_DEFAULT - Indicates the traffic thresold in kBps
 * MAWC_ROAM_AP_RSSI_THRESHOLD_DEFAULT - indicates the AP RSSI threshold
 * MAWC_ROAM_RSSI_HIGH_ADJUST_DEFAULT - Adjustable high value to suppress scan
 * MAWC_ROAM_RSSI_LOW_ADJUST_DEFAULT - Adjustable low value to suppress scan
 */
#define MAWC_ROAM_TRAFFIC_THRESHOLD_DEFAULT  300
#define MAWC_ROAM_AP_RSSI_THRESHOLD_DEFAULT  (-66)
#define MAWC_ROAM_RSSI_HIGH_ADJUST_DEFAULT   5
#define MAWC_ROAM_RSSI_LOW_ADJUST_DEFAULT    5

/*
 * Neighbor report offload needs to send 0xFFFFFFFF if a particular
 * parameter is disabled from the ini
 */
#define NEIGHBOR_REPORT_PARAM_INVALID (0xFFFFFFFFU)

/* Static Type declarations */
static struct csr_roam_session csr_roam_roam_session[CSR_ROAM_SESSION_MAX];

/*
 * To get roam reason from 0 to 3rd bit of roam_synch_data
 * received from firmware
 */
#define ROAM_REASON_MASK 0x0F
/**
 * csr_get_ielen_from_bss_description() - to get IE length
 *             from tSirBssDescription structure
 * @pBssDescr: pBssDescr
 *
 * This function is called in various places to get IE length
 * from tSirBssDescription structure
 *
 * @Return: total IE length
 */
static inline uint16_t
csr_get_ielen_from_bss_description(tpSirBssDescription pBssDescr)
{
	uint16_t ielen;

	if (!pBssDescr)
		return 0;

	/*
	 * Length of BSS desription is without length of
	 * length itself and length of pointer
	 * that holds ieFields
	 *
	 * <------------sizeof(tSirBssDescription)-------------------->
	 * +--------+---------------------------------+---------------+
	 * | length | other fields                    | pointer to IEs|
	 * +--------+---------------------------------+---------------+
	 *                                            ^
	 *                                            ieFields
	 */

	ielen = (uint16_t)(pBssDescr->length + sizeof(pBssDescr->length) -
			   GET_FIELD_OFFSET(tSirBssDescription, ieFields));

	return ielen;
}

#ifdef WLAN_FEATURE_SAE
/**
 * csr_sae_callback - Update SAE info to CSR roam session
 * @mac_ctx: MAC context
 * @msg_ptr: pointer to SAE message
 *
 * API to update SAE info to roam csr session
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_sae_callback(tpAniSirGlobal mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	struct csr_roam_info *roam_info;
	uint32_t session_id;
	struct sir_sae_info *sae_info;

	sae_info = (struct sir_sae_info *) msg_ptr;
	if (!sae_info) {
		sme_err("SAE info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("vdev_id %d "MAC_ADDRESS_STR"",
		sae_info->vdev_id,
		MAC_ADDR_ARRAY(sae_info->peer_mac_addr.bytes));

	session_id = sae_info->vdev_id;
	if (session_id == CSR_SESSION_ID_INVALID)
		return QDF_STATUS_E_INVAL;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info) {
		sme_err("qdf_mem_malloc failed for SAE");
		return QDF_STATUS_E_FAILURE;
	}

	roam_info->sae_info = sae_info;

	csr_roam_call_callback(mac_ctx, session_id, roam_info,
				   0, eCSR_ROAM_SAE_COMPUTE,
				   eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS csr_sae_callback(tpAniSirGlobal mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	return QDF_STATUS_SUCCESS;
}
#endif


#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
enum mgmt_auth_type diag_auth_type_from_csr_type(eCsrAuthType authtype)
{
	int n = AUTH_OPEN;

	switch (authtype) {
	case eCSR_AUTH_TYPE_SHARED_KEY:
		n = AUTH_SHARED;
		break;
	case eCSR_AUTH_TYPE_WPA:
		n = AUTH_WPA_EAP;
		break;
	case eCSR_AUTH_TYPE_WPA_PSK:
		n = AUTH_WPA_PSK;
		break;
	case eCSR_AUTH_TYPE_RSN:
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
#ifdef WLAN_FEATURE_11W
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
#endif
		n = AUTH_WPA2_EAP;
		break;
	case eCSR_AUTH_TYPE_RSN_PSK:
#ifdef WLAN_FEATURE_11W
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
#endif
		n = AUTH_WPA2_PSK;
		break;
#ifdef FEATURE_WLAN_WAPI
	case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
		n = AUTH_WAPI_CERT;
		break;
	case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
		n = AUTH_WAPI_PSK;
		break;
#endif /* FEATURE_WLAN_WAPI */
	default:
		break;
	}
	return n;
}

enum mgmt_encrypt_type diag_enc_type_from_csr_type(eCsrEncryptionType enctype)
{
	int n = ENC_MODE_OPEN;

	switch (enctype) {
	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP40:
		n = ENC_MODE_WEP40;
		break;
	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP104:
		n = ENC_MODE_WEP104;
		break;
	case eCSR_ENCRYPT_TYPE_TKIP:
		n = ENC_MODE_TKIP;
		break;
	case eCSR_ENCRYPT_TYPE_AES:
		n = ENC_MODE_AES;
		break;
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
		n = ENC_MODE_AES_GCMP;
		break;
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
		n = ENC_MODE_AES_GCMP_256;
		break;
#ifdef FEATURE_WLAN_WAPI
	case eCSR_ENCRYPT_TYPE_WPI:
		n = ENC_MODE_SMS4;
		break;
#endif /* FEATURE_WLAN_WAPI */
	default:
		break;
	}
	return n;
}

enum mgmt_dot11_mode
diag_dot11_mode_from_csr_type(enum csr_cfgdot11mode dot11mode)
{
	switch (dot11mode) {
	case eCSR_CFG_DOT11_MODE_ABG:
		return DOT11_MODE_ABG;
	case eCSR_CFG_DOT11_MODE_11A:
		return DOT11_MODE_11A;
	case eCSR_CFG_DOT11_MODE_11B:
		return DOT11_MODE_11B;
	case eCSR_CFG_DOT11_MODE_11G:
		return DOT11_MODE_11G;
	case eCSR_CFG_DOT11_MODE_11N:
		return DOT11_MODE_11N;
	case eCSR_CFG_DOT11_MODE_11AC:
		return DOT11_MODE_11AC;
	case eCSR_CFG_DOT11_MODE_11G_ONLY:
		return DOT11_MODE_11G_ONLY;
	case eCSR_CFG_DOT11_MODE_11N_ONLY:
		return DOT11_MODE_11N_ONLY;
	case eCSR_CFG_DOT11_MODE_11AC_ONLY:
		return DOT11_MODE_11AC_ONLY;
	case eCSR_CFG_DOT11_MODE_AUTO:
		return DOT11_MODE_AUTO;
	case eCSR_CFG_DOT11_MODE_11AX:
		return DOT11_MODE_11AX;
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		return DOT11_MODE_11AX_ONLY;
	default:
		return DOT11_MODE_MAX;
	}
}

enum mgmt_ch_width diag_ch_width_from_csr_type(enum phy_ch_width ch_width)
{
	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		return BW_20MHZ;
	case CH_WIDTH_40MHZ:
		return BW_40MHZ;
	case CH_WIDTH_80MHZ:
		return BW_80MHZ;
	case CH_WIDTH_160MHZ:
		return BW_160MHZ;
	case CH_WIDTH_80P80MHZ:
		return BW_80P80MHZ;
	case CH_WIDTH_5MHZ:
		return BW_5MHZ;
	case CH_WIDTH_10MHZ:
		return BW_10MHZ;
	default:
		return BW_MAX;
	}
}

enum mgmt_bss_type diag_persona_from_csr_type(enum QDF_OPMODE persona)
{
	switch (persona) {
	case QDF_STA_MODE:
		return STA_PERSONA;
	case QDF_SAP_MODE:
		return SAP_PERSONA;
	case QDF_P2P_CLIENT_MODE:
		return P2P_CLIENT_PERSONA;
	case QDF_P2P_GO_MODE:
		return P2P_GO_PERSONA;
	case QDF_FTM_MODE:
		return FTM_PERSONA;
	case QDF_IBSS_MODE:
		return IBSS_PERSONA;
	case QDF_MONITOR_MODE:
		return MONITOR_PERSONA;
	case QDF_P2P_DEVICE_MODE:
		return P2P_DEVICE_PERSONA;
	case QDF_OCB_MODE:
		return OCB_PERSONA;
	case QDF_EPPING_MODE:
		return EPPING_PERSONA;
	case QDF_QVIT_MODE:
		return QVIT_PERSONA;
	case QDF_NDI_MODE:
		return NDI_PERSONA;
	case QDF_WDS_MODE:
		return WDS_PERSONA;
	case QDF_BTAMP_MODE:
		return BTAMP_PERSONA;
	case QDF_AHDEMO_MODE:
		return AHDEMO_PERSONA;
	default:
		return MAX_PERSONA;
	}
}
#endif /* #ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR */

static const uint8_t
csr_start_ibss_channels50[CSR_NUM_IBSS_START_CHAN_50] = { 36, 44, 52, 56, 140 };
static const uint8_t
csr_start_ibss_channels24[CSR_NUM_IBSS_START_CHANNELS_24] = { 1, 6, 11 };

static const uint8_t
social_channel[MAX_SOCIAL_CHANNELS] = { 1, 6, 11 };

static void init_config_param(tpAniSirGlobal pMac);
static bool csr_roam_process_results(tpAniSirGlobal pMac, tSmeCmd *pCommand,
				     enum csr_roamcomplete_result Result,
				     void *Context);
static QDF_STATUS csr_roam_start_ibss(tpAniSirGlobal pMac, uint32_t sessionId,
				      struct csr_roam_profile *pProfile,
				      bool *pfSameIbss);
static void csr_roam_update_connected_profile_from_new_bss(tpAniSirGlobal pMac,
							   uint32_t sessionId,
							   tSirSmeNewBssInfo *
							   pNewBss);
static ePhyChanBondState csr_get_cb_mode_from_ies(tpAniSirGlobal pMac,
						  uint8_t primaryChn,
						  tDot11fBeaconIEs *pIes);

static void csr_roaming_state_config_cnf_processor(tpAniSirGlobal pMac,
			tSmeCmd *pCommand, uint32_t result, uint8_t session_id);
static QDF_STATUS csr_roam_open(tpAniSirGlobal pMac);
static QDF_STATUS csr_roam_close(tpAniSirGlobal pMac);
static bool csr_roam_is_same_profile_keys(tpAniSirGlobal pMac,
				   tCsrRoamConnectedProfile *pConnProfile,
				   struct csr_roam_profile *pProfile2);

static QDF_STATUS csr_roam_start_roaming_timer(tpAniSirGlobal pMac,
					       uint32_t vdev_id,
					       uint32_t interval);
static QDF_STATUS csr_roam_stop_roaming_timer(tpAniSirGlobal pMac,
					      uint32_t vdev_id);
static void csr_roam_roaming_timer_handler(void *pv);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_roam_roaming_offload_timer_action(tpAniSirGlobal mac_ctx,
		uint32_t interval, uint8_t session_id, uint8_t action);
#endif
static void csr_roam_roaming_offload_timeout_handler(void *timer_data);
static QDF_STATUS csr_roam_start_wait_for_key_timer(tpAniSirGlobal pMac,
						uint32_t interval);
static void csr_roam_wait_for_key_time_out_handler(void *pv);
static QDF_STATUS csr_init11d_info(tpAniSirGlobal pMac, tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_init_channel_power_list(tpAniSirGlobal pMac,
					      tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_roam_free_connected_info(tpAniSirGlobal pMac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo);
static QDF_STATUS csr_send_mb_set_context_req_msg(tpAniSirGlobal pMac,
						uint32_t sessionId,
					   struct qdf_mac_addr peer_macaddr,
					    uint8_t numKeys,
					   tAniEdType edType, bool fUnicast,
					   tAniKeyDirection aniKeyDirection,
					   uint8_t keyId, uint8_t keyLength,
					   uint8_t *pKey, uint8_t paeRole,
					   uint8_t *pKeyRsc);
static void csr_roam_link_up(tpAniSirGlobal pMac, struct qdf_mac_addr bssid);
static void csr_roam_link_down(tpAniSirGlobal pMac, uint32_t sessionId);
#ifndef QCA_SUPPORT_CP_STATS
static QDF_STATUS csr_send_mb_stats_req_msg(tpAniSirGlobal pMac,
					uint32_t statsMask, uint8_t staId,
					uint8_t sessionId);
/* pStaEntry is no longer invalid upon the return of this function. */
static void csr_roam_remove_stat_list_entry(tpAniSirGlobal pMac,
							tListElem *pEntry);
struct csr_statsclient_reqinfo *csr_roam_insert_entry_into_list(
			tpAniSirGlobal pMac, tDblLinkList *pStaList,
				struct csr_statsclient_reqinfo *
				pStaEntry);
static void csr_roam_report_statistics(tpAniSirGlobal pMac,
	uint32_t statsMask, tCsrStatsCallback callback, uint8_t staId,
	void *pContext);
tListElem *csr_roam_check_client_req_list(
	tpAniSirGlobal pMac, uint32_t statsMask);
static void csr_roam_remove_entry_from_pe_stats_req_list(
		tpAniSirGlobal pMac, struct csr_pestats_reqinfo *pPeStaEntry);
tListElem *csr_roam_find_in_pe_stats_req_list(
	tpAniSirGlobal pMac,
						uint32_t statsMask);
static QDF_STATUS csr_roam_dereg_statistics_req(tpAniSirGlobal pMac);
#else
static QDF_STATUS csr_roam_dereg_statistics_req(tpAniSirGlobal pMac)
{
	return QDF_STATUS_SUCCESS;
}
#endif
static enum csr_cfgdot11mode
csr_roam_get_phy_mode_band_for_bss(tpAniSirGlobal pMac,
				   struct csr_roam_profile *pProfile,
				   uint8_t operationChn,
				   enum band_info *pBand);
static QDF_STATUS csr_roam_get_qos_info_from_bss(
tpAniSirGlobal pMac, tSirBssDescription *pBssDesc);
static uint32_t csr_find_ibss_session(tpAniSirGlobal pMac);
static uint32_t csr_find_session_by_type(tpAniSirGlobal,
					enum QDF_OPMODE);
static bool csr_is_conn_allow_2g_band(tpAniSirGlobal pMac,
						uint32_t chnl);
static bool csr_is_conn_allow_5g_band(tpAniSirGlobal pMac,
						uint32_t chnl);
static QDF_STATUS csr_roam_start_wds(tpAniSirGlobal pMac,
						uint32_t sessionId,
				     struct csr_roam_profile *pProfile,
				     tSirBssDescription *pBssDesc);
static void csr_init_session(tpAniSirGlobal pMac, uint32_t sessionId);
static QDF_STATUS csr_roam_issue_set_key_command(tpAniSirGlobal pMac,
						 uint32_t sessionId,
						 tCsrRoamSetKey *pSetKey,
						 uint32_t roamId);
static QDF_STATUS csr_roam_get_qos_info_from_bss(tpAniSirGlobal pMac,
						 tSirBssDescription *pBssDesc);
static void csr_ser_des_unpack_diassoc_rsp(uint8_t *pBuf,
					   tSirSmeDisassocRsp *pRsp);
static void csr_init_operating_classes(tHalHandle hHal);

static void csr_add_len_of_social_channels(tpAniSirGlobal mac,
		uint8_t *num_chan);
static void csr_add_social_channels(tpAniSirGlobal mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan);

/* Initialize global variables */
static void csr_roam_init_globals(tpAniSirGlobal pMac)
{
	if (pMac) {
		qdf_mem_zero(&csr_roam_roam_session,
				sizeof(csr_roam_roam_session));
		pMac->roam.roamSession = csr_roam_roam_session;
	}
}

static void csr_roam_de_init_globals(tpAniSirGlobal pMac)
{
	uint8_t i;

	if (pMac) {
		for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
			if (pMac->roam.roamSession[i].pCurRoamProfile)
				csr_release_profile(pMac,
						    pMac->roam.roamSession[i].
						    pCurRoamProfile);
			csr_release_profile(pMac,
					    &pMac->roam.roamSession[i].
					    stored_roam_profile.profile);
		}
		pMac->roam.roamSession = NULL;
	}
}

#ifdef QCA_SUPPORT_CP_STATS
static QDF_STATUS csr_open_stats_ll(struct sAniSirGlobal *mac_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static void csr_close_stats_ll(struct sAniSirGlobal *mac_ctx) {}
#else
static QDF_STATUS csr_open_stats_ll(struct sAniSirGlobal *mac_ctx)
{
	QDF_STATUS status;

	status = csr_ll_open(&mac_ctx->roam.statsClientReqList);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return csr_ll_open(&mac_ctx->roam.peStatsReqList);
}

static void csr_close_stats_ll(struct sAniSirGlobal *mac_ctx)
{
	csr_ll_close(&mac_ctx->roam.statsClientReqList);
	csr_ll_close(&mac_ctx->roam.peStatsReqList);
}
#endif

/**
 * csr_assoc_rej_free_rssi_disallow_list() - Free the rssi disallowed
 * BSSID entries and destroy the list
 * @mac_ctx: MAC context
 *
 * Return: void
 */
void csr_assoc_rej_free_rssi_disallow_list(struct sAniSirGlobal *mac)
{
	QDF_STATUS status;
	struct sir_rssi_disallow_lst *cur_node;
	qdf_list_node_t *cur_lst = NULL, *next_lst = NULL;
	qdf_list_t *list = &mac->roam.rssi_disallow_bssid;

	qdf_mutex_acquire(&mac->roam.rssi_disallow_bssid_lock);
	qdf_list_peek_front(list, &cur_lst);
	while (cur_lst) {
		qdf_list_peek_next(list, cur_lst, &next_lst);
		cur_node = qdf_container_of(cur_lst,
					    struct sir_rssi_disallow_lst, node);
		status = qdf_list_remove_node(list, cur_lst);
		if (QDF_IS_STATUS_SUCCESS(status))
			qdf_mem_free(cur_node);
		cur_lst = next_lst;
		next_lst = NULL;
	}
	qdf_mutex_release(&mac->roam.rssi_disallow_bssid_lock);
}

/**
 * csr_roam_rssi_disallow_bssid_init() - Init the rssi disallowed
 * list and mutex
 * @mac_ctx: MAC context
 *
 * Return: QDF_STATUS enumeration
 */
static QDF_STATUS csr_roam_rssi_disallow_bssid_init(
					     struct sAniSirGlobal *mac_ctx)
{
	qdf_list_create(&mac_ctx->roam.rssi_disallow_bssid,
			MAX_RSSI_AVOID_BSSID_LIST);
	qdf_mutex_create(&mac_ctx->roam.rssi_disallow_bssid_lock);

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_roam_rssi_disallow_bssid_deinit() - Free the rssi diallowed
 * BSSID entries and destroy the list&mutex
 * @mac_ctx: MAC context
 *
 * Return: QDF_STATUS enumeration
 */
static QDF_STATUS csr_roam_rssi_disallow_bssid_deinit(
					     struct sAniSirGlobal *mac_ctx)
{
	csr_assoc_rej_free_rssi_disallow_list(mac_ctx);
	qdf_list_destroy(&mac_ctx->roam.rssi_disallow_bssid);
	qdf_mutex_destroy(&mac_ctx->roam.rssi_disallow_bssid_lock);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_open(tpAniSirGlobal pMac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		/* Initialize CSR Roam Globals */
		csr_roam_init_globals(pMac);
		for (i = 0; i < CSR_ROAM_SESSION_MAX; i++)
			csr_roam_state_change(pMac, eCSR_ROAMING_STATE_STOP, i);

		init_config_param(pMac);
		status = csr_scan_open(pMac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		status = csr_roam_open(pMac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMac->roam.nextRoamId = 1;      /* Must not be 0 */
		status = csr_open_stats_ll(pMac);
		if (QDF_IS_STATUS_ERROR(status))
			break;

		csr_roam_rssi_disallow_bssid_init(pMac);
	} while (0);

	return status;
}

QDF_STATUS csr_init_chan_list(tpAniSirGlobal mac, uint8_t *alpha2)
{
	QDF_STATUS status;

	mac->scan.countryCodeDefault[0] = alpha2[0];
	mac->scan.countryCodeDefault[1] = alpha2[1];
	mac->scan.countryCodeDefault[2] = alpha2[2];

	sme_debug("init time country code %.2s", mac->scan.countryCodeDefault);

	mac->scan.domainIdDefault = 0;
	mac->scan.domainIdCurrent = 0;

	qdf_mem_copy(mac->scan.countryCodeCurrent,
		     mac->scan.countryCodeDefault, WNI_CFG_COUNTRY_CODE_LEN);
	qdf_mem_copy(mac->scan.countryCodeElected,
		     mac->scan.countryCodeDefault, WNI_CFG_COUNTRY_CODE_LEN);
	status = csr_get_channel_and_power_list(mac);
	csr_clear_votes_for_country_info(mac);
	return status;
}

QDF_STATUS csr_set_channels(tpAniSirGlobal pMac, tCsrConfigParam *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t index = 0;

	qdf_mem_copy(pParam->Csr11dinfo.countryCode,
		     pMac->scan.countryCodeCurrent, WNI_CFG_COUNTRY_CODE_LEN);
	for (index = 0; index < pMac->scan.base_channels.numChannels;
	     index++) {
		pParam->Csr11dinfo.Channels.channelList[index] =
			pMac->scan.base_channels.channelList[index];
		pParam->Csr11dinfo.ChnPower[index].firstChannel =
			pMac->scan.base_channels.channelList[index];
		pParam->Csr11dinfo.ChnPower[index].numChannels = 1;
		pParam->Csr11dinfo.ChnPower[index].maxtxPower =
			pMac->scan.defaultPowerTable[index].tx_power;
	}
	pParam->Csr11dinfo.Channels.numChannels =
		pMac->scan.base_channels.numChannels;

	return status;
}

QDF_STATUS csr_close(tpAniSirGlobal pMac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_roam_close(pMac);
	csr_roam_rssi_disallow_bssid_deinit(pMac);
	csr_scan_close(pMac);
	csr_close_stats_ll(pMac);
	/* DeInit Globals */
	csr_roam_de_init_globals(pMac);
	return status;
}

static int8_t csr_find_channel_pwr(struct channel_power *
					     pdefaultPowerTable,
					     uint8_t ChannelNum)
{
	uint8_t i;
	/* TODO: if defaultPowerTable is guaranteed to be in ascending */
	/* order of channel numbers, we can employ binary search */
	for (i = 0; i < WNI_CFG_VALID_CHANNEL_LIST_LEN; i++) {
		if (pdefaultPowerTable[i].chan_num == ChannelNum)
			return pdefaultPowerTable[i].tx_power;
	}
	/* could not find the channel list in default list */
	/* this should not have occurred */
	QDF_ASSERT(0);
	return 0;
}

/**
 * csr_roam_arrange_ch_list() - Updates the channel list modified with greedy
 * order for 5 Ghz preference and DFS channels.
 * @mac_ctx: pointer to mac context.
 * @chan_list:    channel list updated with greedy channel order.
 * @num_channel:  Number of channels in list
 *
 * To allow Early Stop Roaming Scan feature to co-exist with 5G preference,
 * this function moves 5G channels ahead of 2G channels. This function can
 * also move 2G channels, ahead of DFS channel or vice versa. Order is
 * maintained among same category channels
 *
 * Return: None
 */
static void csr_roam_arrange_ch_list(tpAniSirGlobal mac_ctx,
			tSirUpdateChanParam *chan_list, uint8_t num_channel)
{
	bool prefer_5g = CSR_IS_ROAM_PREFER_5GHZ(mac_ctx);
	bool prefer_dfs = CSR_IS_DFS_CH_ROAM_ALLOWED(mac_ctx);
	int i, j = 0;
	tSirUpdateChanParam *tmp_list = NULL;

	if (!prefer_5g)
		return;

	tmp_list = (tSirUpdateChanParam *)
		qdf_mem_malloc(sizeof(tSirUpdateChanParam) * num_channel);
	if (tmp_list == NULL) {
		sme_err("Memory allocation failed");
		return;
	}

	/* Fist copy Non-DFS 5g channels */
	for (i = 0; i < num_channel; i++) {
		if (WLAN_REG_IS_5GHZ_CH(chan_list[i].chanId) &&
			!wlan_reg_is_dfs_ch(mac_ctx->pdev,
				chan_list[i].chanId)) {
			qdf_mem_copy(&tmp_list[j++],
				&chan_list[i], sizeof(tSirUpdateChanParam));
			chan_list[i].chanId = INVALID_CHANNEL_ID;
		}
	}
	if (prefer_dfs) {
		/* next copy DFS channels (remaining channels in 5G) */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_5GHZ_CH(chan_list[i].chanId)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].chanId = INVALID_CHANNEL_ID;
			}
		}
	} else {
		/* next copy 2G channels */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_24GHZ_CH(chan_list[i].chanId)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].chanId = INVALID_CHANNEL_ID;
			}
		}
	}
	/* copy rest of the channels in same order to tmp list */
	for (i = 0; i < num_channel; i++) {
		if (chan_list[i].chanId != INVALID_CHANNEL_ID) {
			qdf_mem_copy(&tmp_list[j++], &chan_list[i],
				sizeof(tSirUpdateChanParam));
			chan_list[i].chanId = INVALID_CHANNEL_ID;
		}
	}
	/* copy tmp list to original channel list buffer */
	qdf_mem_copy(chan_list, tmp_list,
				 sizeof(tSirUpdateChanParam) * num_channel);
	qdf_mem_free(tmp_list);
}

/**
 * csr_roam_sort_channel_for_early_stop() - Sort the channels
 * @mac_ctx:        mac global context
 * @chan_list:      Original channel list from the upper layers
 * @num_channel:    Number of original channels
 *
 * For Early stop scan feature, the channel list should be in an order,
 * where-in there is a maximum chance to detect an AP in the initial
 * channels in the list so that the scanning can be stopped early as the
 * feature demands.
 * Below fixed greedy channel list has been provided
 * based on most of the enterprise wifi installations across the globe.
 *
 * Identify all the greedy channels within the channel list from user space.
 * Identify all the non-greedy channels in the user space channel list.
 * Merge greedy channels followed by non-greedy channels back into the
 * chan_list.
 *
 * Return: None
 */
static void csr_roam_sort_channel_for_early_stop(tpAniSirGlobal mac_ctx,
			tSirUpdateChanList *chan_list, uint8_t num_channel)
{
	tSirUpdateChanList *chan_list_greedy, *chan_list_non_greedy;
	uint8_t i, j;
	static const uint8_t fixed_greedy_chan_list[] = {1, 6, 11, 36, 48, 40,
		44, 10, 2, 9, 149, 157, 161, 3, 4, 8, 153, 165, 7, 5, 136, 140,
		52, 116, 56, 104, 64, 60, 100, 120, 13, 14, 112, 132, 151, 155};
	uint8_t num_fixed_greedy_chan;
	uint8_t num_greedy_chan = 0;
	uint8_t num_non_greedy_chan = 0;
	uint8_t match_found = false;
	uint32_t buf_size;

	buf_size = sizeof(tSirUpdateChanList) +
		(sizeof(tSirUpdateChanParam) * num_channel);
	chan_list_greedy = qdf_mem_malloc(buf_size);
	chan_list_non_greedy = qdf_mem_malloc(buf_size);
	if (!chan_list_greedy || !chan_list_non_greedy) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "Failed to allocate memory for tSirUpdateChanList");
		goto scan_list_sort_error;
	}
	/*
	 * fixed_greedy_chan_list is an evaluated channel list based on most of
	 * the enterprise wifi deployments and the order of the channels
	 * determines the highest possibility of finding an AP.
	 * chan_list is the channel list provided by upper layers based on the
	 * regulatory domain.
	 */
	num_fixed_greedy_chan = sizeof(fixed_greedy_chan_list)/sizeof(uint8_t);
	/*
	 * Browse through the chan_list and put all the non-greedy channels
	 * into a separate list by name chan_list_non_greedy
	 */
	for (i = 0; i < num_channel; i++) {
		for (j = 0; j < num_fixed_greedy_chan; j++) {
			if (chan_list->chanParam[i].chanId ==
					fixed_greedy_chan_list[j]) {
				match_found = true;
				break;
			}
		}
		if (!match_found) {
			qdf_mem_copy(
			  &chan_list_non_greedy->chanParam[num_non_greedy_chan],
			  &chan_list->chanParam[i],
			  sizeof(tSirUpdateChanParam));
			num_non_greedy_chan++;
		} else {
			match_found = false;
		}
	}
	/*
	 * Browse through the fixed_greedy_chan_list and put all the greedy
	 * channels in the chan_list into a separate list by name
	 * chan_list_greedy
	 */
	for (i = 0; i < num_fixed_greedy_chan; i++) {
		for (j = 0; j < num_channel; j++) {
			if (fixed_greedy_chan_list[i] ==
					chan_list->chanParam[j].chanId) {
				qdf_mem_copy(
				  &chan_list_greedy->chanParam[num_greedy_chan],
				  &chan_list->chanParam[j],
				  sizeof(tSirUpdateChanParam));
				num_greedy_chan++;
				break;
			}
		}
	}
	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG,
		"greedy=%d, non-greedy=%d, tot=%d",
		num_greedy_chan, num_non_greedy_chan, num_channel);
	if ((num_greedy_chan + num_non_greedy_chan) != num_channel) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			"incorrect sorting of channels");
		goto scan_list_sort_error;
	}
	/* Copy the Greedy channels first */
	i = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_greedy->chanParam[i],
		num_greedy_chan * sizeof(tSirUpdateChanParam));
	/* Copy the remaining Non Greedy channels */
	i = num_greedy_chan;
	j = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_non_greedy->chanParam[j],
		num_non_greedy_chan * sizeof(tSirUpdateChanParam));

	/* Update channel list for 5g preference and allow DFS roam */
	csr_roam_arrange_ch_list(mac_ctx, chan_list->chanParam, num_channel);
scan_list_sort_error:
	qdf_mem_free(chan_list_greedy);
	qdf_mem_free(chan_list_non_greedy);
}

/**
 * csr_emu_chan_req() - update the required channel list for emulation
 * @channel: channel number to check
 *
 * To reduce scan time during emulation platforms, this function
 * restricts the scanning to be done on selected channels
 *
 * Return: QDF_STATUS enumeration
 */
#ifdef QCA_WIFI_NAPIER_EMULATION
#define SCAN_CHAN_LIST_5G_LEN 6
#define SCAN_CHAN_LIST_2G_LEN 3
static const uint8_t
csr_scan_chan_list_5g[SCAN_CHAN_LIST_5G_LEN] = { 36, 44, 52, 56, 140, 149 };
static const uint8_t
csr_scan_chan_list_2g[SCAN_CHAN_LIST_2G_LEN] = { 1, 6, 11 };
static QDF_STATUS csr_emu_chan_req(uint32_t channel)
{
	int i;

	if (WLAN_REG_IS_24GHZ_CH(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_2g); i++) {
			if (csr_scan_chan_list_2g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	} else if (WLAN_REG_IS_5GHZ_CH(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_5g); i++) {
			if (csr_scan_chan_list_5g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}
#else
static QDF_STATUS csr_emu_chan_req(uint32_t channel_num)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
static void csr_add_len_of_social_channels(tpAniSirGlobal mac,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add len of social channels, before adding - num_chan:%hu",
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state(
				mac->pdev, social_channel[i]) ==
					CHANNEL_STATE_ENABLE)
				no_chan++;
		}
	}
	*num_chan = no_chan;
	sme_debug("after adding - num_chan:%hu", *num_chan);
}

static void csr_add_social_channels(tpAniSirGlobal mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add social channels chan_list %pK, num_chan %hu", chan_list,
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state(mac->pdev,
				social_channel[i]) != CHANNEL_STATE_ENABLE)
				continue;
			chan_list->chanParam[no_chan].chanId =
				social_channel[i];
			chan_list->chanParam[no_chan].pwr =
				csr_find_channel_pwr(pScan->defaultPowerTable,
						social_channel[i]);
			chan_list->chanParam[no_chan].dfsSet = false;
			if (cds_is_5_mhz_enabled())
				chan_list->chanParam[no_chan].quarter_rate
					= 1;
			else if (cds_is_10_mhz_enabled())
				chan_list->chanParam[no_chan].half_rate = 1;
			no_chan++;
		}
		sme_debug("after adding -num_chan %hu", no_chan);
	}
	*num_chan = no_chan;
}
#else
static void csr_add_len_of_social_channels(tpAniSirGlobal mac,
		uint8_t *num_chan)
{
	sme_debug("skip adding len of social channels");
}
static void csr_add_social_channels(tpAniSirGlobal mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	sme_debug("skip social channels");
}
#endif

/**
 * csr_scan_event_handler() - callback for scan event
 * @vdev: wlan objmgr vdev pointer
 * @event: scan event
 * @arg: global mac context pointer
 *
 * Return: void
 */
static void csr_scan_event_handler(struct wlan_objmgr_vdev *vdev,
					    struct scan_event *event,
					    void *arg)
{
	bool success = false;
	QDF_STATUS lock_status;
	tpAniSirGlobal pMac = arg;

	if (!pMac)
		return;

	if (!util_is_scan_completed(event, &success))
		return;

	lock_status = sme_acquire_global_lock(&pMac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return;

	if (pMac->scan.pending_channel_list_req)
		csr_update_channel_list(pMac);
	sme_release_global_lock(&pMac->sme);
}

QDF_STATUS csr_update_channel_list(tpAniSirGlobal pMac)
{
	tSirUpdateChanList *pChanList;
	struct csr_scanstruct *pScan = &pMac->scan;
	uint8_t numChan = pScan->base_channels.numChannels;
	uint8_t num_channel = 0;
	uint32_t bufLen;
	struct scheduler_msg msg = {0};
	uint8_t i;
	uint8_t channel_state;
	uint16_t unsafe_chan[NUM_CHANNELS];
	uint16_t unsafe_chan_cnt = 0;
	uint16_t cnt = 0;
	uint8_t  channel;
	bool is_unsafe_chan;
	enum scm_scan_status scan_status;
	QDF_STATUS lock_status;
	bool is_5mhz_enabled;
	bool is_10mhz_enabled;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		sme_err("qdf_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	lock_status = sme_acquire_global_lock(&pMac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return lock_status;

	if (pMac->roam.configParam.enable_pending_list_req) {
		scan_status = ucfg_scan_get_pdev_status(pMac->pdev);
		if (scan_status == SCAN_IS_ACTIVE ||
		    scan_status == SCAN_IS_ACTIVE_AND_PENDING) {
			pMac->scan.pending_channel_list_req = true;
			sme_release_global_lock(&pMac->sme);
			sme_debug("scan in progress postpone channel list req ");
			return QDF_STATUS_SUCCESS;
		}
		pMac->scan.pending_channel_list_req = false;
	}
	sme_release_global_lock(&pMac->sme);

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
		    &unsafe_chan_cnt,
		    sizeof(unsafe_chan));

	csr_add_len_of_social_channels(pMac, &numChan);

	bufLen = sizeof(tSirUpdateChanList) +
		 (sizeof(tSirUpdateChanParam) * (numChan));

	csr_init_operating_classes((tHalHandle) pMac);
	pChanList = (tSirUpdateChanList *) qdf_mem_malloc(bufLen);
	if (!pChanList) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			  "Failed to allocate memory for tSirUpdateChanList");
		return QDF_STATUS_E_NOMEM;
	}

	is_5mhz_enabled = cds_is_5_mhz_enabled();
	if (is_5mhz_enabled)
		sme_nofl_debug("quarter_rate enabled");
	is_10mhz_enabled = cds_is_10_mhz_enabled();
	if (is_10mhz_enabled)
		sme_nofl_debug("half_rate enabled");

	for (i = 0; i < pScan->base_channels.numChannels; i++) {
		struct csr_sta_roam_policy_params *roam_policy =
			&pMac->roam.configParam.sta_roam_policy;
		if (QDF_STATUS_SUCCESS !=
			csr_emu_chan_req(pScan->base_channels.channelList[i]))
			continue;

		/* Scan is not performed on DSRC channels*/
		if (wlan_reg_is_dsrc_chan(pMac->pdev,
					  pScan->base_channels.channelList[i]))
			continue;

		channel = pScan->base_channels.channelList[i];

		channel_state = wlan_reg_get_channel_state(pMac->pdev,
				pScan->base_channels.channelList[i]);
		if ((CHANNEL_STATE_ENABLE == channel_state) ||
		    pMac->scan.fEnableDFSChnlScan) {
			if ((pMac->roam.configParam.sta_roam_policy.dfs_mode ==
				CSR_STA_ROAM_POLICY_DFS_DISABLED) &&
				(channel_state == CHANNEL_STATE_DFS)) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					QDF_TRACE_LEVEL_DEBUG,
					FL("skip dfs channel %d"),
					channel);
				continue;
			}
			if (pMac->roam.configParam.sta_roam_policy.
					skip_unsafe_channels &&
					unsafe_chan_cnt) {
				is_unsafe_chan = false;
				for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
					if (unsafe_chan[cnt] == channel) {
						is_unsafe_chan = true;
						break;
					}
				}
				if ((is_unsafe_chan) &&
				    ((WLAN_REG_IS_24GHZ_CH(channel) &&
				      roam_policy->sap_operating_band ==
					BAND_2G) ||
					(WLAN_REG_IS_5GHZ_CH(channel) &&
					 roam_policy->sap_operating_band ==
					BAND_5G))) {
					QDF_TRACE(QDF_MODULE_ID_SME,
					QDF_TRACE_LEVEL_DEBUG,
					FL("ignoring unsafe channel %d"),
					channel);
					continue;
				}
			}
			pChanList->chanParam[num_channel].chanId =
				pScan->base_channels.channelList[i];
			pChanList->chanParam[num_channel].pwr =
				csr_find_channel_pwr(pScan->defaultPowerTable,
				  pChanList->chanParam[num_channel].chanId);

			if (pScan->fcc_constraint) {
				if (12 == pChanList->chanParam[num_channel].
								chanId) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_12;
					QDF_TRACE(QDF_MODULE_ID_SME,
						  QDF_TRACE_LEVEL_DEBUG,
						  "txpow for channel 12 is %d",
						  MAX_PWR_FCC_CHAN_12);
				}
				if (13 == pChanList->chanParam[num_channel].
								chanId) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_13;
					QDF_TRACE(QDF_MODULE_ID_SME,
						  QDF_TRACE_LEVEL_DEBUG,
						  "txpow for channel 13 is %d",
						  MAX_PWR_FCC_CHAN_13);
				}
			}

			if (!ucfg_is_nan_allowed_on_chan(pMac->pdev,
				pChanList->chanParam[num_channel].chanId))
				pChanList->chanParam[num_channel].nan_disabled =
					true;

			if (CHANNEL_STATE_ENABLE != channel_state)
				pChanList->chanParam[num_channel].dfsSet =
					true;

			pChanList->chanParam[num_channel].quarter_rate =
					is_5mhz_enabled;
			pChanList->chanParam[num_channel].half_rate =
					is_10mhz_enabled;
			num_channel++;
		}
	}

	csr_add_social_channels(pMac, pChanList, pScan, &num_channel);

	if (pMac->roam.configParam.early_stop_scan_enable)
		csr_roam_sort_channel_for_early_stop(pMac, pChanList,
						     num_channel);
	else
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Early Stop Scan Feature not supported"));

	if ((pMac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(pMac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC) ||
			(pMac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC_ONLY)) {
		pChanList->vht_en = true;
		if (pMac->roam.configParam.enableVhtFor24GHz)
			pChanList->vht_24_en = true;
	}
	if ((pMac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(pMac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N) ||
			(pMac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N_ONLY)) {
		pChanList->ht_en = true;
	}
	msg.type = WMA_UPDATE_CHAN_LIST_REQ;
	msg.reserved = 0;
	msg.bodyptr = pChanList;
	pChanList->numChan = num_channel;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
			  "%s: Failed to post msg to WMA", __func__);
		qdf_mem_free(pChanList);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_CP_STATS
static void csr_init_tl_stats(struct sAniSirGlobal *mac_ctx) {}
#else
static void csr_init_tl_stats(struct sAniSirGlobal *mac_ctx)
{
	mac_ctx->roam.tlStatsReqInfo.numClient = 0;
}
#endif /* QCA_SUPPORT_CP_STATS */

QDF_STATUS csr_start(tpAniSirGlobal pMac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		for (i = 0; i < CSR_ROAM_SESSION_MAX; i++)
			csr_roam_state_change(pMac, eCSR_ROAMING_STATE_IDLE, i);

		status = csr_roam_start(pMac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		pMac->roam.sPendingCommands = 0;
		for (i = 0; i < CSR_ROAM_SESSION_MAX; i++)
			status = csr_neighbor_roam_init(pMac, i);
		csr_init_tl_stats(pMac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_warn("csr_start: Couldn't Init HO control blk ");
			break;
		}
		/* Register with scan component */
		pMac->scan.requester_id = ucfg_scan_register_requester(
						pMac->psoc,
						"CSR", csr_scan_callback, pMac);
		ucfg_scan_set_enable(pMac->psoc, true);

		if (pMac->roam.configParam.enable_pending_list_req) {
			status = ucfg_scan_register_event_handler(pMac->pdev,
					csr_scan_event_handler, pMac);

			if (QDF_IS_STATUS_ERROR(status))
				sme_err("scan event registration failed ");
		}
	} while (0);
	return status;
}

QDF_STATUS csr_stop(tpAniSirGlobal pMac)
{
	uint32_t sessionId;

	if (pMac->roam.configParam.enable_pending_list_req)
		ucfg_scan_unregister_event_handler(pMac->pdev,
						   csr_scan_event_handler,
						   pMac);
	ucfg_scan_set_enable(pMac->psoc, false);
	ucfg_scan_unregister_requester(pMac->psoc, pMac->scan.requester_id);

	/*
	 * purge all serialization commnad if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(pMac);
	for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
		csr_roam_close_session(pMac, sessionId, true);

	for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
		csr_neighbor_roam_close(pMac, sessionId);
	for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
		if (CSR_IS_SESSION_VALID(pMac, sessionId))
			csr_scan_flush_result(pMac);

	/* Reset the domain back to the deault */
	pMac->scan.domainIdCurrent = pMac->scan.domainIdDefault;

	for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++) {
		csr_roam_state_change(pMac, eCSR_ROAMING_STATE_STOP, sessionId);
		csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_ready(tpAniSirGlobal pMac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	/* If the gScanAgingTime is set to '0' then scan results aging timeout
	 * based  on timer feature is not enabled
	 */
	status = csr_apply_channel_and_power_list(pMac);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("csr_apply_channel_and_power_list failed during csr_ready with status: %d",
			status);

	return status;
}

void csr_set_default_dot11_mode(tpAniSirGlobal pMac)
{
	uint32_t wniDot11mode = 0;

	wniDot11mode = csr_translate_to_wni_cfg_dot11_mode(pMac,
					pMac->roam.configParam.uCfgDot11Mode);
	cfg_set_int(pMac, WNI_CFG_DOT11_MODE, wniDot11mode);
}

void csr_set_global_cfgs(tpAniSirGlobal pMac)
{

	cfg_set_int(pMac, WNI_CFG_FRAGMENTATION_THRESHOLD,
			csr_get_frag_thresh(pMac));
	cfg_set_int(pMac, WNI_CFG_RTS_THRESHOLD, csr_get_rts_thresh(pMac));
	cfg_set_int(pMac, WNI_CFG_11D_ENABLED,
			((pMac->roam.configParam.Is11hSupportEnabled) ?
			pMac->roam.configParam.Is11dSupportEnabled :
			pMac->roam.configParam.Is11dSupportEnabled));
	cfg_set_int(pMac, WNI_CFG_11H_ENABLED,
			pMac->roam.configParam.Is11hSupportEnabled);
	/* For now we will just use the 5GHz CB mode ini parameter to decide
	 * whether CB supported or not in Probes when there is no session
	 * Once session is established we will use the session related params
	 * stored in PE session for CB mode
	 */
	cfg_set_int(pMac, WNI_CFG_CHANNEL_BONDING_MODE,
			!!(pMac->roam.configParam.channelBondingMode5GHz));
	cfg_set_int(pMac, WNI_CFG_HEART_BEAT_THRESHOLD,
			pMac->roam.configParam.HeartbeatThresh24);

	/* Update the operating mode to configured value during
	 *  initialization, So that client can advertise full
	 *  capabilities in Probe request frame.
	 */
	csr_set_default_dot11_mode(pMac);
}

/**
 * csr_packetdump_timer_handler() - packet dump timer
 * handler
 * @pv: user data
 *
 * This function is used to handle packet dump timer
 *
 * Return: None
 *
 */
static void csr_packetdump_timer_handler(void *pv)
{
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"%s Invoking packetdump deregistration API", __func__);
	wlan_deregister_txrx_packetdump();
}

/**
 * csr_packetdump_timer_stop() - stops packet dump timer
 *
 * This function is used to stop packet dump timer
 *
 * Return: None
 *
 */
void csr_packetdump_timer_stop(void)
{
	QDF_STATUS status;
	tHalHandle hal;
	tpAniSirGlobal mac;

	hal = cds_get_context(QDF_MODULE_ID_SME);
	if (hal == NULL) {
		QDF_ASSERT(0);
		return;
	}

	mac = PMAC_STRUCT(hal);
	status = qdf_mc_timer_stop(&mac->roam.packetdump_timer);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("cannot stop packetdump timer");
}

static QDF_STATUS csr_roam_open(tpAniSirGlobal pMac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;
	struct csr_roam_session *pSession;

	do {
		for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
			pSession = CSR_GET_SESSION(pMac, i);
			pSession->roamingTimerInfo.pMac = pMac;
			pSession->roamingTimerInfo.vdev_id =
				CSR_SESSION_ID_INVALID;
		}
		pMac->roam.WaitForKeyTimerInfo.pMac = pMac;
		pMac->roam.WaitForKeyTimerInfo.vdev_id =
			CSR_SESSION_ID_INVALID;
		status = qdf_mc_timer_init(&pMac->roam.hTimerWaitForKey,
					  QDF_TIMER_TYPE_SW,
					 csr_roam_wait_for_key_time_out_handler,
					  &pMac->roam.WaitForKeyTimerInfo);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("cannot allocate memory for WaitForKey time out timer");
			break;
		}
		status = qdf_mc_timer_init(&pMac->roam.packetdump_timer,
				QDF_TIMER_TYPE_SW, csr_packetdump_timer_handler,
				pMac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("cannot allocate memory for packetdump timer");
			break;
		}
		spin_lock_init(&pMac->roam.roam_state_lock);
	} while (0);
	return status;
}

static QDF_STATUS csr_roam_close(tpAniSirGlobal pMac)
{
	uint32_t sessionId;

	/*
	 * purge all serialization commnad if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(pMac);
	for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
		csr_roam_close_session(pMac, sessionId, true);

	qdf_mc_timer_stop(&pMac->roam.hTimerWaitForKey);
	qdf_mc_timer_destroy(&pMac->roam.hTimerWaitForKey);
	qdf_mc_timer_stop(&pMac->roam.packetdump_timer);
	qdf_mc_timer_destroy(&pMac->roam.packetdump_timer);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_roam_start(tpAniSirGlobal pMac)
{
	(void)pMac;
	return QDF_STATUS_SUCCESS;
}

void csr_roam_stop(tpAniSirGlobal pMac, uint32_t sessionId)
{
	csr_roam_stop_roaming_timer(pMac, sessionId);
	/* deregister the clients requesting stats from PE/TL & also stop
	 * the corresponding timers
	 */
	csr_roam_dereg_statistics_req(pMac);
}

QDF_STATUS csr_roam_get_connect_state(tpAniSirGlobal pMac, uint32_t sessionId,
				      eCsrConnectState *pState)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (CSR_IS_SESSION_VALID(pMac, sessionId) && (NULL != pState)) {
		status = QDF_STATUS_SUCCESS;
		*pState = pMac->roam.roamSession[sessionId].connectState;
	}
	return status;
}

QDF_STATUS csr_roam_copy_connect_profile(tpAniSirGlobal pMac,
			uint32_t sessionId, tCsrRoamConnectedProfile *pProfile)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t size = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	tCsrRoamConnectedProfile *connected_prof;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	if (!pProfile) {
		sme_err("profile not found");
		return QDF_STATUS_E_FAILURE;
	}

	if (pSession->pConnectBssDesc) {
		size = pSession->pConnectBssDesc->length +
			sizeof(pSession->pConnectBssDesc->length);
		if (size) {
			pProfile->pBssDesc = qdf_mem_malloc(size);
			if (NULL != pProfile->pBssDesc) {
				qdf_mem_copy(pProfile->pBssDesc,
					pSession->pConnectBssDesc,
					size);
				status = QDF_STATUS_SUCCESS;
			} else {
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			pProfile->pBssDesc = NULL;
		}
		connected_prof = &(pSession->connectedProfile);
		pProfile->AuthType = connected_prof->AuthType;
		pProfile->akm_list = connected_prof->akm_list;
		pProfile->EncryptionType = connected_prof->EncryptionType;
		pProfile->mcEncryptionType = connected_prof->mcEncryptionType;
		pProfile->BSSType = connected_prof->BSSType;
		pProfile->operationChannel = connected_prof->operationChannel;
		qdf_mem_copy(&pProfile->bssid, &connected_prof->bssid,
			sizeof(struct qdf_mac_addr));
		qdf_mem_copy(&pProfile->SSID, &connected_prof->SSID,
			sizeof(tSirMacSSid));
		if (connected_prof->MDID.mdiePresent) {
			pProfile->MDID.mdiePresent = 1;
			pProfile->MDID.mobilityDomain =
				connected_prof->MDID.mobilityDomain;
		} else {
			pProfile->MDID.mdiePresent = 0;
			pProfile->MDID.mobilityDomain = 0;
		}
#ifdef FEATURE_WLAN_ESE
		pProfile->isESEAssoc = connected_prof->isESEAssoc;
		if (csr_is_auth_type_ese(connected_prof->AuthType)) {
			qdf_mem_copy(pProfile->eseCckmInfo.krk,
				connected_prof->eseCckmInfo.krk,
				SIR_KRK_KEY_LEN);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
			qdf_mem_copy(pProfile->eseCckmInfo.btk,
				connected_prof->eseCckmInfo.btk,
				SIR_BTK_KEY_LEN);
#endif
			pProfile->eseCckmInfo.reassoc_req_num =
				connected_prof->eseCckmInfo.reassoc_req_num;
			pProfile->eseCckmInfo.krk_plumbed =
				connected_prof->eseCckmInfo.krk_plumbed;
		}
#endif
#ifdef WLAN_FEATURE_11W
		pProfile->MFPEnabled = connected_prof->MFPEnabled;
		pProfile->MFPRequired = connected_prof->MFPRequired;
		pProfile->MFPCapable = connected_prof->MFPCapable;
#endif
	}
	return status;
}

QDF_STATUS csr_roam_get_connect_profile(tpAniSirGlobal pMac, uint32_t sessionId,
					tCsrRoamConnectedProfile *pProfile)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if ((csr_is_conn_state_connected(pMac, sessionId)) ||
	    (csr_is_conn_state_ibss(pMac, sessionId))) {
		if (pProfile) {
			status =
				csr_roam_copy_connect_profile(pMac, sessionId,
							      pProfile);
		}
	}
	return status;
}

void csr_roam_free_connect_profile(tCsrRoamConnectedProfile *profile)
{
	if (profile->pBssDesc)
		qdf_mem_free(profile->pBssDesc);
	if (profile->pAddIEAssoc)
		qdf_mem_free(profile->pAddIEAssoc);
	qdf_mem_zero(profile, sizeof(tCsrRoamConnectedProfile));
	profile->AuthType = eCSR_AUTH_TYPE_UNKNOWN;
}

static QDF_STATUS csr_roam_free_connected_info(tpAniSirGlobal pMac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (pConnectedInfo->pbFrames) {
		qdf_mem_free(pConnectedInfo->pbFrames);
		pConnectedInfo->pbFrames = NULL;
	}
	pConnectedInfo->nBeaconLength = 0;
	pConnectedInfo->nAssocReqLength = 0;
	pConnectedInfo->nAssocRspLength = 0;
	pConnectedInfo->staId = 0;
	pConnectedInfo->nRICRspLength = 0;
#ifdef FEATURE_WLAN_ESE
	pConnectedInfo->nTspecIeLength = 0;
#endif
	return status;
}

void csr_release_command_roam(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
	csr_reinit_roam_cmd(pMac, pCommand);
}

void csr_release_command_wm_status_change(tpAniSirGlobal pMac,
					tSmeCmd *pCommand)
{
	csr_reinit_wm_status_change_cmd(pMac, pCommand);
}

static void csr_release_command_set_hw_mode(tpAniSirGlobal mac,
					    tSmeCmd *cmd)
{
	struct csr_roam_session *session;
	uint32_t session_id;

	if (cmd->u.set_hw_mode_cmd.reason ==
	    POLICY_MGR_UPDATE_REASON_HIDDEN_STA) {
		session_id = cmd->u.set_hw_mode_cmd.session_id;
		session = CSR_GET_SESSION(mac, session_id);
		if (session)
			csr_saved_scan_cmd_free_fields(mac, session);
	}
}

void csr_roam_substate_change(tpAniSirGlobal pMac,
		enum csr_roam_substate NewSubstate, uint32_t sessionId)
{
	if (sessionId >= CSR_ROAM_SESSION_MAX) {
		sme_err("Invalid no of concurrent sessions %d",
			  sessionId);
		return;
	}
	if (pMac->roam.curSubState[sessionId] == NewSubstate)
		return;
	sme_nofl_debug("CSR RoamSubstate: [ %s <== %s ]",
		       mac_trace_getcsr_roam_sub_state(NewSubstate),
		       mac_trace_getcsr_roam_sub_state(pMac->roam.
		       curSubState[sessionId]));
	spin_lock(&pMac->roam.roam_state_lock);
	pMac->roam.curSubState[sessionId] = NewSubstate;
	spin_unlock(&pMac->roam.roam_state_lock);
}

enum csr_roam_state csr_roam_state_change(tpAniSirGlobal pMac,
				    enum csr_roam_state NewRoamState,
				uint8_t sessionId)
{
	enum csr_roam_state PreviousState;

	PreviousState = pMac->roam.curState[sessionId];
	if (NewRoamState == pMac->roam.curState[sessionId])
		 return PreviousState;

	sme_nofl_debug("CSR RoamState[%hu]: [ %s <== %s ]", sessionId,
		       mac_trace_getcsr_roam_state(NewRoamState),
		       mac_trace_getcsr_roam_state(pMac->roam.curState[sessionId]));
	/* Whenever we transition OUT of the Roaming state,
	 * clear the Roaming substate.
	 */
	if (CSR_IS_ROAM_JOINING(pMac, sessionId)) {
		csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	pMac->roam.curState[sessionId] = NewRoamState;

	return PreviousState;
}

void csr_assign_rssi_for_category(tpAniSirGlobal pMac, int8_t bestApRssi,
				  uint8_t catOffset)
{
	int i;

	if (catOffset) {
		pMac->roam.configParam.bCatRssiOffset = catOffset;
		for (i = 0; i < CSR_NUM_RSSI_CAT; i++) {
			pMac->roam.configParam.RSSICat[CSR_NUM_RSSI_CAT - i -
						       1] =
				(int)bestApRssi -
				pMac->roam.configParam.nSelect5GHzMargin -
				(int)(i * catOffset);
		}
	}
}

static void init_config_param(tpAniSirGlobal pMac)
{
	int i;

	pMac->roam.configParam.agingCount = CSR_AGING_COUNT;
	pMac->roam.configParam.channelBondingMode24GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	pMac->roam.configParam.channelBondingMode5GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;

	pMac->roam.configParam.phyMode = eCSR_DOT11_MODE_AUTO;
	pMac->roam.configParam.eBand = BAND_ALL;
	pMac->roam.configParam.uCfgDot11Mode = eCSR_CFG_DOT11_MODE_AUTO;
	pMac->roam.configParam.FragmentationThreshold =
		eCSR_DOT11_FRAG_THRESH_DEFAULT;
	pMac->roam.configParam.HeartbeatThresh24 = 40;
	pMac->roam.configParam.HeartbeatThresh50 = 40;
	pMac->roam.configParam.Is11dSupportEnabled = false;
	pMac->roam.configParam.Is11eSupportEnabled = true;
	pMac->roam.configParam.Is11hSupportEnabled = true;
	pMac->roam.configParam.RTSThreshold = 2346;
	pMac->roam.configParam.shortSlotTime = true;
	pMac->roam.configParam.WMMSupportMode = eCsrRoamWmmAuto;
	pMac->roam.configParam.ProprietaryRatesEnabled = true;
	for (i = 0; i < CSR_NUM_RSSI_CAT; i++)
		pMac->roam.configParam.BssPreferValue[i] = i;
	csr_assign_rssi_for_category(pMac, CSR_BEST_RSSI_VALUE,
			CSR_DEFAULT_RSSI_DB_GAP);
	pMac->roam.configParam.fSupplicantCountryCodeHasPriority = false;
	pMac->roam.configParam.nActiveMaxChnTime = CSR_ACTIVE_MAX_CHANNEL_TIME;
	pMac->roam.configParam.nActiveMinChnTime = CSR_ACTIVE_MIN_CHANNEL_TIME;
	pMac->roam.configParam.nPassiveMaxChnTime =
		CSR_PASSIVE_MAX_CHANNEL_TIME;
	pMac->roam.configParam.nPassiveMinChnTime =
		CSR_PASSIVE_MIN_CHANNEL_TIME;
	pMac->roam.configParam.nActiveMaxChnTimeConc =
		CSR_ACTIVE_MAX_CHANNEL_TIME_CONC;
	pMac->roam.configParam.nActiveMinChnTimeConc =
		CSR_ACTIVE_MIN_CHANNEL_TIME_CONC;
	pMac->roam.configParam.nPassiveMaxChnTimeConc =
		CSR_PASSIVE_MAX_CHANNEL_TIME_CONC;
	pMac->roam.configParam.nPassiveMinChnTimeConc =
		CSR_PASSIVE_MIN_CHANNEL_TIME_CONC;
	pMac->roam.configParam.nRestTimeConc = CSR_REST_TIME_CONC;
	pMac->roam.configParam.min_rest_time_conc =  CSR_MIN_REST_TIME_CONC;
	pMac->roam.configParam.idle_time_conc = CSR_IDLE_TIME_CONC;
	pMac->roam.configParam.nTxPowerCap = CSR_MAX_TX_POWER;
	pMac->roam.configParam.allow_tpc_from_ap = true;
	pMac->roam.configParam.statsReqPeriodicity =
		CSR_MIN_GLOBAL_STAT_QUERY_PERIOD;
	pMac->roam.configParam.statsReqPeriodicityInPS =
		CSR_MIN_GLOBAL_STAT_QUERY_PERIOD_IN_BMPS;
	pMac->roam.configParam.neighborRoamConfig.nMaxNeighborRetries = 3;
	pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold =
		120;
	pMac->roam.configParam.neighborRoamConfig.rssi_thresh_offset_5g = 0;
	pMac->roam.configParam.neighborRoamConfig.nOpportunisticThresholdDiff =
		30;
	pMac->roam.configParam.neighborRoamConfig.nRoamRescanRssiDiff = 5;
	pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime = 20;
	pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime = 40;
	pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod =
		200;
	pMac->roam.configParam.neighborRoamConfig.
		neighbor_scan_min_timer_period = 200;
	pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.
	numChannels = 3;
	pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.
	channelList[0] = 1;
	pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.
	channelList[1] = 6;
	pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.
	channelList[2] = 11;
	pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod
						= 20000;        /* 20 seconds */
	pMac->roam.configParam.neighborRoamConfig.nEmptyScanRefreshPeriod = 0;
	pMac->roam.configParam.neighborRoamConfig.nRoamBmissFirstBcnt = 10;
	pMac->roam.configParam.neighborRoamConfig.nRoamBmissFinalBcnt = 10;
	pMac->roam.configParam.neighborRoamConfig.nRoamBeaconRssiWeight = 14;
	pMac->roam.configParam.nVhtChannelWidth =
		WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ + 1;

	pMac->roam.configParam.addTSWhenACMIsOff = 0;
	pMac->roam.configParam.fScanTwice = false;

	/* Remove this code once SLM_Sessionization is supported */
	/* BMPS_WORKAROUND_NOT_NEEDED */
	pMac->roam.configParam.doBMPSWorkaround = 0;

	pMac->roam.configParam.nInitialDwellTime = 0;
	pMac->roam.configParam.initial_scan_no_dfs_chnl = 0;
	pMac->roam.configParam.csr_mawc_config.mawc_enabled = true;
	pMac->roam.configParam.csr_mawc_config.mawc_roam_enabled = true;
	pMac->roam.configParam.csr_mawc_config.mawc_roam_traffic_threshold =
		MAWC_ROAM_TRAFFIC_THRESHOLD_DEFAULT;
	pMac->roam.configParam.csr_mawc_config.mawc_roam_ap_rssi_threshold =
		MAWC_ROAM_AP_RSSI_THRESHOLD_DEFAULT;
	pMac->roam.configParam.csr_mawc_config.mawc_roam_rssi_high_adjust =
		MAWC_ROAM_RSSI_HIGH_ADJUST_DEFAULT;
	pMac->roam.configParam.csr_mawc_config.mawc_roam_rssi_low_adjust =
		MAWC_ROAM_RSSI_LOW_ADJUST_DEFAULT;

	qdf_mem_zero(&pMac->roam.configParam.bss_score_params,
		     sizeof(struct sir_score_config));
	pMac->roam.configParam.bss_score_params.weight_cfg.rssi_weightage =
		RSSI_WEIGHTAGE;
	pMac->roam.configParam.bss_score_params.weight_cfg.ht_caps_weightage =
		HT_CAPABILITY_WEIGHTAGE;
	pMac->roam.configParam.bss_score_params.weight_cfg.vht_caps_weightage =
		VHT_CAP_WEIGHTAGE;
	pMac->roam.configParam.bss_score_params.
		weight_cfg.chan_width_weightage = CHAN_WIDTH_WEIGHTAGE;
	pMac->roam.configParam.bss_score_params.
		weight_cfg.chan_band_weightage = CHAN_BAND_WEIGHTAGE;
	pMac->roam.configParam.bss_score_params.weight_cfg.nss_weightage =
		NSS_WEIGHTAGE;
	pMac->roam.configParam.bss_score_params.weight_cfg.
		beamforming_cap_weightage = BEAMFORMING_CAP_WEIGHTAGE;
	pMac->roam.configParam.bss_score_params.weight_cfg.pcl_weightage =
		PCL_WEIGHT;
	pMac->roam.configParam.bss_score_params.weight_cfg.
		channel_congestion_weightage = CHANNEL_CONGESTION_WEIGHTAGE;
}

enum band_info csr_get_current_band(tHalHandle hHal)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

	return pMac->roam.configParam.bandCapability;
}

void csr_flush_cfg_bg_scan_roam_channel_list(tCsrChannelInfo *channel_info)
{
	/* Free up the memory first (if required) */
	if (channel_info->ChannelList) {
		qdf_mem_free(channel_info->ChannelList);
		channel_info->ChannelList = NULL;
		channel_info->numOfChannels = 0;
	}
}

/**
 * csr_flush_roam_scan_chan_lists() - Flush the roam channel lists
 * @mac: Global MAC context
 * @vdev_id: vdev id
 *
 * Flush the roam channel lists pref_chan_info and specific_chan_info.
 *
 * Return: None
 */
static void
csr_flush_roam_scan_chan_lists(tpAniSirGlobal mac, uint8_t vdev_id)
{
	tCsrChannelInfo *chan_info;

	chan_info =
	&mac->roam.neighborRoamInfo[vdev_id].cfgParams.pref_chan_info;
	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);

	chan_info =
	&mac->roam.neighborRoamInfo[vdev_id].cfgParams.specific_chan_info;
	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);
}

QDF_STATUS csr_create_bg_scan_roam_channel_list(tpAniSirGlobal pMac,
						tCsrChannelInfo *channel_info,
						const uint8_t *pChannelList,
						const uint8_t numChannels)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	channel_info->ChannelList = qdf_mem_malloc(sizeof(uint32_t) * numChannels);
	if (!channel_info->ChannelList)
		return QDF_STATUS_E_NOMEM;

	channel_info->numOfChannels = numChannels;
	qdf_mem_copy(channel_info->ChannelList, pChannelList, numChannels);

	return status;
}

#ifdef FEATURE_WLAN_ESE
/**
 * csr_create_roam_scan_channel_list() - create roam scan channel list
 * @pMac: Global mac pointer
 * @sessionId: session id
 * @pChannelList: pointer to channel list
 * @numChannels: number of channels
 * @eBand: band enumeration
 *
 * This function modifies the roam scan channel list as per AP neighbor
 * report; AP neighbor report may be empty or may include only other AP
 * channels; in any case, we merge the channel list with the learned occupied
 * channels list.
 * if the band is 2.4G, then make sure channel list contains only 2.4G
 * valid channels if the band is 5G, then make sure channel list contains
 * only 5G valid channels
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_create_roam_scan_channel_list(tpAniSirGlobal pMac,
					     uint8_t sessionId,
					     uint8_t *pChannelList,
					     uint8_t numChannels,
					     const enum band_info eBand)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	tpCsrNeighborRoamControlInfo pNeighborRoamInfo
		= &pMac->roam.neighborRoamInfo[sessionId];
	uint8_t outNumChannels = 0;
	uint8_t inNumChannels = numChannels;
	uint8_t *inPtr = pChannelList;
	uint8_t i = 0;
	uint8_t ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t tmpChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t mergedOutputNumOfChannels = 0;

	tpCsrChannelInfo currChannelListInfo
		= &pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo;
	/*
	 * Create a Union of occupied channel list learnt by the DUT along
	 * with the Neighbor report Channels. This increases the chances of
	 * the DUT to get a candidate AP while roaming even if the Neighbor
	 * Report is not able to provide sufficient information.
	 */
	if (pMac->scan.occupiedChannels[sessionId].numChannels) {
		csr_neighbor_roam_merge_channel_lists(pMac, &pMac->scan.
						occupiedChannels[sessionId].
						channelList[0], pMac->scan.
						occupiedChannels[sessionId].
						numChannels, inPtr,
						inNumChannels,
						&mergedOutputNumOfChannels);
		inNumChannels = mergedOutputNumOfChannels;
	}
	if (BAND_2G == eBand) {
		for (i = 0; i < inNumChannels; i++) {
			if (WLAN_REG_IS_24GHZ_CH(inPtr[i])
			    && csr_roam_is_channel_valid(pMac, inPtr[i])) {
				ChannelList[outNumChannels++] = inPtr[i];
			}
		}
	} else if (BAND_5G == eBand) {
		for (i = 0; i < inNumChannels; i++) {
			/* Add 5G Non-DFS channel */
			if (WLAN_REG_IS_5GHZ_CH(inPtr[i]) &&
			    csr_roam_is_channel_valid(pMac, inPtr[i]) &&
			    !wlan_reg_is_dfs_ch(pMac->pdev, inPtr[i])) {
				ChannelList[outNumChannels++] = inPtr[i];
			}
		}
	} else if (BAND_ALL == eBand) {
		for (i = 0; i < inNumChannels; i++) {
			if (csr_roam_is_channel_valid(pMac, inPtr[i]) &&
			    !wlan_reg_is_dfs_ch(pMac->pdev, inPtr[i])) {
				ChannelList[outNumChannels++] = inPtr[i];
			}
		}
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_WARN,
			  "Invalid band, No operation carried out (Band %d)",
			  eBand);
		return QDF_STATUS_E_INVAL;
	}
	/*
	 * if roaming within band is enabled, then select only the
	 * in band channels .
	 * This is required only if the band capability is set to ALL,
	 * E.g., if band capability is only 2.4G then all the channels in the
	 * list are already filtered for 2.4G channels, hence ignore this check
	 */
	if ((BAND_ALL == eBand) && CSR_IS_ROAM_INTRA_BAND_ENABLED(pMac)) {
		csr_neighbor_roam_channels_filter_by_current_band(pMac,
								sessionId,
								ChannelList,
								outNumChannels,
								tmpChannelList,
							&outNumChannels);
		qdf_mem_copy(ChannelList, tmpChannelList, outNumChannels);
	}
	/* Prepare final roam scan channel list */
	if (outNumChannels) {
		/* Clear the channel list first */
		if (NULL != currChannelListInfo->ChannelList) {
			qdf_mem_free(currChannelListInfo->ChannelList);
			currChannelListInfo->ChannelList = NULL;
			currChannelListInfo->numOfChannels = 0;
		}
		currChannelListInfo->ChannelList
			= qdf_mem_malloc(outNumChannels * sizeof(uint8_t));
		if (NULL == currChannelListInfo->ChannelList) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
				  "Failed to allocate memory for roam scan channel list");
			currChannelListInfo->numOfChannels = 0;
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_copy(currChannelListInfo->ChannelList,
			     ChannelList, outNumChannels);
	}
	return status;
}

/**
 * csr_roam_is_ese_assoc() - is this ese association
 * @mac_ctx: Global MAC context
 * @session_id: session identifier
 *
 * Returns whether the current association is a ESE assoc or not.
 *
 * Return: true if ese association; false otherwise
 */
bool csr_roam_is_ese_assoc(tpAniSirGlobal mac_ctx, uint32_t session_id)
{
	return mac_ctx->roam.neighborRoamInfo[session_id].isESEAssoc;
}


/**
 * csr_roam_is_ese_ini_feature_enabled() - is ese feature enabled
 * @mac_ctx: Global MAC context
 *
 * Return: true if ese feature is enabled; false otherwise
 */
bool csr_roam_is_ese_ini_feature_enabled(tpAniSirGlobal pMac)
{
	return pMac->roam.configParam.isEseIniFeatureEnabled;
}

/**
 * csr_tsm_stats_rsp_processor() - tsm stats response processor
 * @pMac: Global MAC context
 * @pMsg: Message pointer
 *
 * Return: None
 */
static void csr_tsm_stats_rsp_processor(tpAniSirGlobal pMac, void *pMsg)
{
	tAniGetTsmStatsRsp *pTsmStatsRsp = (tAniGetTsmStatsRsp *) pMsg;

	if (NULL != pTsmStatsRsp) {
		/*
		 * Get roam Rssi request is backed up and passed back
		 * to the response, Extract the request message
		 * to fetch callback.
		 */
		tpAniGetTsmStatsReq reqBkp
			= (tAniGetTsmStatsReq *) pTsmStatsRsp->tsmStatsReq;

		if (NULL != reqBkp) {
			if (NULL != reqBkp->tsmStatsCallback) {
				((tCsrTsmStatsCallback)
				 (reqBkp->tsmStatsCallback))(pTsmStatsRsp->
							     tsmMetrics,
							     pTsmStatsRsp->
							     staId,
							     reqBkp->
							     pDevContext);
				reqBkp->tsmStatsCallback = NULL;
			}
			qdf_mem_free(reqBkp);
			pTsmStatsRsp->tsmStatsReq = NULL;
		} else {
			if (NULL != reqBkp) {
				qdf_mem_free(reqBkp);
				pTsmStatsRsp->tsmStatsReq = NULL;
			}
		}
	} else {
		sme_err("pTsmStatsRsp is NULL");
	}
}

/**
 * csr_send_ese_adjacent_ap_rep_ind() - ese send adjacent ap report
 * @pMac: Global MAC context
 * @pSession: Session pointer
 *
 * Return: None
 */
static void csr_send_ese_adjacent_ap_rep_ind(tpAniSirGlobal pMac,
					struct csr_roam_session *pSession)
{
	uint32_t roamTS2 = 0;
	tpPESession pSessionEntry = NULL;
	struct csr_roam_info *roam_info;
	uint8_t sessionId = CSR_SESSION_ID_INVALID;

	if (NULL == pSession) {
		sme_err("pSession is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roamTS2 = qdf_mc_timer_get_system_time();
	roam_info->tsmRoamDelay = roamTS2 - pSession->roamTS1;
	sme_debug("Bssid(" MAC_ADDRESS_STR ") Roaming Delay(%u ms)",
		MAC_ADDR_ARRAY(pSession->connectedProfile.bssid.bytes),
		roam_info->tsmRoamDelay);

	pSessionEntry = pe_find_session_by_bssid(pMac,
					 pSession->connectedProfile.bssid.bytes,
					 &sessionId);
	if (NULL == pSessionEntry) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}

	pSessionEntry->eseContext.tsm.tsmMetrics.RoamingDly
		= roam_info->tsmRoamDelay;

	csr_roam_call_callback(pMac, pSession->sessionId, roam_info,
			       0, eCSR_ROAM_ESE_ADJ_AP_REPORT_IND, 0);
	qdf_mem_free(roam_info);
}

/**
 * csr_get_tsm_stats() - get tsm stats
 * @pMac: Global MAC context
 * @callback: TSM stats callback
 * @staId: Station id
 * @bssId: bssid
 * @pContext: pointer to context
 * @tid: traffic id
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_get_tsm_stats(tpAniSirGlobal pMac,
			     tCsrTsmStatsCallback callback,
			     uint8_t staId,
			     struct qdf_mac_addr bssId,
			     void *pContext, uint8_t tid)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tAniGetTsmStatsReq *pMsg = NULL;

	pMsg = qdf_mem_malloc(sizeof(tAniGetTsmStatsReq));
	if (!pMsg) {
		sme_err(
			"csr_get_tsm_stats: failed to allocate mem for req");
		return QDF_STATUS_E_NOMEM;
	}
	/* need to initiate a stats request to PE */
	pMsg->msgType = eWNI_SME_GET_TSM_STATS_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetTsmStatsReq);
	pMsg->staId = staId;
	pMsg->tid = tid;
	qdf_copy_macaddr(&pMsg->bssId, &bssId);
	pMsg->tsmStatsCallback = callback;
	pMsg->pDevContext = pContext;
	status = umac_send_mb_message_to_mac(pMsg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("csr_get_tsm_stats: failed to send down the rssi req");
		/* pMsg is freed by cds_send_mb_message_to_mac */
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}


#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_fetch_ch_lst_from_received_list() - fetch channel list from received list
 * and update req msg
 * parameters
 * @mac_ctx:            global mac ctx
 * @roam_info:          roam info struct
 * @curr_ch_lst_info:   current channel list info
 * @req_buf:            out param, roam offload scan request packet
 *
 * Return: void
 */
static void
csr_fetch_ch_lst_from_received_list(tpAniSirGlobal mac_ctx,
				    tpCsrNeighborRoamControlInfo roam_info,
				    tpCsrChannelInfo curr_ch_lst_info,
				    tSirRoamOffloadScanReq *req_buf)
{
	uint8_t i = 0;
	uint8_t num_channels = 0;
	uint8_t *ch_lst = NULL;
	uint16_t  unsafe_chan[NUM_CHANNELS];
	uint16_t  unsafe_chan_cnt = 0;
	uint16_t  cnt = 0;
	bool      is_unsafe_chan;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		cds_err("qdf_ctx is NULL");
		return;
	}
	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
			&unsafe_chan_cnt,
			sizeof(unsafe_chan));

	if (curr_ch_lst_info->numOfChannels == 0)
		return;

	ch_lst = curr_ch_lst_info->ChannelList;
	for (i = 0; i < curr_ch_lst_info->numOfChannels; i++) {
		if ((!mac_ctx->roam.configParam.allowDFSChannelRoam ||
		    (mac_ctx->roam.configParam.sta_roam_policy.dfs_mode ==
			 CSR_STA_ROAM_POLICY_DFS_DISABLED)) &&
		     (wlan_reg_is_dfs_ch(mac_ctx->pdev, *ch_lst))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				("ignoring dfs channel %d"), *ch_lst);
			ch_lst++;
			continue;
		}

		if (mac_ctx->roam.configParam.sta_roam_policy.
				skip_unsafe_channels &&
				unsafe_chan_cnt) {
			is_unsafe_chan = false;
			for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
				if (unsafe_chan[cnt] == *ch_lst) {
					is_unsafe_chan = true;
					break;
				}
			}
			if (is_unsafe_chan) {
				QDF_TRACE(QDF_MODULE_ID_SME,
						QDF_TRACE_LEVEL_DEBUG,
					("ignoring unsafe channel %d"),
					*ch_lst);
				ch_lst++;
				continue;
			}
		}
		req_buf->ConnectedNetwork.ChannelCache[num_channels++] =
			*ch_lst;
		ch_lst++;
	}
	req_buf->ConnectedNetwork.ChannelCount = num_channels;
	req_buf->ChannelCacheType = CHANNEL_LIST_DYNAMIC;
}
#endif

/**
 * csr_set_cckm_ie() - set CCKM IE
 * @pMac: Global MAC context
 * @sessionId: session identifier
 * @pCckmIe: Pointer to input CCKM IE data
 * @ccKmIeLen: Length of @pCckmIe
 *
 * This function stores the CCKM IE passed by the supplicant
 * in a place holder data structure and this IE will be packed inside
 * reassociation request
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_set_cckm_ie(tpAniSirGlobal pMac, const uint8_t sessionId,
			   const uint8_t *pCckmIe, const uint8_t ccKmIeLen)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(pSession->suppCckmIeInfo.cckmIe, pCckmIe, ccKmIeLen);
	pSession->suppCckmIeInfo.cckmIeLen = ccKmIeLen;
	return status;
}

/**
 * csr_roam_read_tsf() - read TSF
 * @pMac: Global MAC context
 * @sessionId: session identifier
 * @pTimestamp: output TSF timestamp
 *
 * This function reads the TSF; and also add the time elapsed since
 * last beacon or probe response reception from the hand off AP to arrive at
 * the latest TSF value.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_roam_read_tsf(tpAniSirGlobal pMac, uint8_t *pTimestamp,
			     uint8_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrNeighborRoamBSSInfo handoffNode = {{0} };
	uint64_t timer_diff = 0;
	uint32_t timeStamp[2];
	tpSirBssDescription pBssDescription = NULL;

	csr_neighbor_roam_get_handoff_ap_info(pMac, &handoffNode, sessionId);
	if (!handoffNode.pBssDescription) {
		sme_err("Invalid BSS Description");
		return QDF_STATUS_E_INVAL;
	}
	pBssDescription = handoffNode.pBssDescription;
	/* Get the time diff in nano seconds */
	timer_diff = (qdf_get_monotonic_boottime_ns()  -
				pBssDescription->scansystimensec);
	/* Convert msec to micro sec timer */
	timer_diff = do_div(timer_diff, SYSTEM_TIME_NSEC_TO_USEC);
	timeStamp[0] = pBssDescription->timeStamp[0];
	timeStamp[1] = pBssDescription->timeStamp[1];
	update_cckmtsf(&(timeStamp[0]), &(timeStamp[1]), &timer_diff);
	qdf_mem_copy(pTimestamp, (void *)&timeStamp[0], sizeof(uint32_t) * 2);
	return status;
}

#endif /* FEATURE_WLAN_ESE */

/**
 * csr_roam_is_roam_offload_scan_enabled() - is roam offload enabled
 * @mac_ctx: Global MAC context
 *
 * Returns whether firmware based background scan is currently enabled or not.
 *
 * Return: true if roam offload scan enabled; false otherwise
 */
bool csr_roam_is_roam_offload_scan_enabled(tpAniSirGlobal mac_ctx)
{
	return mac_ctx->roam.configParam.isRoamOffloadScanEnabled;
}

QDF_STATUS csr_set_band(tHalHandle hHal, uint8_t sessionId,
			enum band_info eBand)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (CSR_IS_PHY_MODE_A_ONLY(pMac) && (eBand == BAND_2G)) {
		/* DOT11 mode configured to 11a only and received
		 * request to change the band to 2.4 GHz
		 */
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "failed to set band cfg80211 = %u, band = %u",
			  pMac->roam.configParam.uCfgDot11Mode, eBand);
		return QDF_STATUS_E_INVAL;
	}
	if ((CSR_IS_PHY_MODE_B_ONLY(pMac) ||
	     CSR_IS_PHY_MODE_G_ONLY(pMac)) && (eBand == BAND_5G)) {
		/* DOT11 mode configured to 11b/11g only and received
		 * request to change the band to 5 GHz
		 */
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "failed to set band dot11mode = %u, band = %u",
			  pMac->roam.configParam.uCfgDot11Mode, eBand);
		return QDF_STATUS_E_INVAL;
	}
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "Band changed to %u (0 - ALL, 1 - 2.4 GHZ, 2 - 5GHZ)", eBand);
	pMac->roam.configParam.eBand = eBand;
	pMac->roam.configParam.bandCapability = eBand;

	status = csr_get_channel_and_power_list(pMac);
	if (QDF_STATUS_SUCCESS == status)
		csr_apply_channel_and_power_list(pMac);
	return status;
}

/* The funcns csr_convert_cb_ini_value_to_phy_cb_state and
 * csr_convert_phy_cb_state_to_ini_value have been introduced
 * to convert the ini value to the ENUM used in csr and MAC for CB state
 * Ideally we should have kept the ini value and enum value same and
 * representing the same cb values as in 11n standard i.e.
 * Set to 1 (SCA) if the secondary channel is above the primary channel
 * Set to 3 (SCB) if the secondary channel is below the primary channel
 * Set to 0 (SCN) if no secondary channel is present
 * However, since our driver is already distributed we will keep the ini
 * definition as it is which is:
 * 0 - secondary none
 * 1 - secondary LOW
 * 2 - secondary HIGH
 * and convert to enum value used within the driver in
 * csr_change_default_config_param using this funcn
 * The enum values are as follows:
 * PHY_SINGLE_CHANNEL_CENTERED          = 0
 * PHY_DOUBLE_CHANNEL_LOW_PRIMARY   = 1
 * PHY_DOUBLE_CHANNEL_HIGH_PRIMARY  = 3
 */
ePhyChanBondState csr_convert_cb_ini_value_to_phy_cb_state(uint32_t cbIniValue)
{

	ePhyChanBondState phyCbState;

	switch (cbIniValue) {
	/* secondary none */
	case eCSR_INI_SINGLE_CHANNEL_CENTERED:
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		phyCbState =
			PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* If an invalid value is passed, disable CHANNEL BONDING */
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	}
	return phyCbState;
}

static
uint32_t csr_convert_phy_cb_state_to_ini_value(ePhyChanBondState phyCbState)
{
	uint32_t cbIniValue;

	switch (phyCbState) {
	/* secondary none */
	case PHY_SINGLE_CHANNEL_CENTERED:
		cbIniValue = eCSR_INI_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		cbIniValue =
		eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* return some invalid value */
		cbIniValue = eCSR_INI_CHANNEL_BONDING_STATE_MAX;
		break;
	}
	return cbIniValue;
}

#ifdef WLAN_FEATURE_11AX
/**
 * csr_update_he_config_param() - Update MAC context with HE config param
 * @mac_ctx: pointer to MAC context
 * @param: pointer to CSR config params
 *
 * Return: None
 */
static void csr_update_he_config_param(tpAniSirGlobal mac_ctx,
				       tCsrConfigParam *param)
{
	mac_ctx->roam.configParam.enable_ul_ofdma = param->enable_ul_ofdma;
	mac_ctx->roam.configParam.enable_ul_mimo = param->enable_ul_mimo;
}

/**
 * csr_get_he_config_param() - Get HE config param from MAC context
 * @param: pointer to CSR config params
 * @mac_ctx: pointer to MAC context
 *
 * Return: None
 */
static void csr_get_he_config_param(tCsrConfigParam *param,
				    tpAniSirGlobal mac_ctx)
{
	param->enable_ul_ofdma = mac_ctx->roam.configParam.enable_ul_ofdma;
	param->enable_ul_mimo = mac_ctx->roam.configParam.enable_ul_mimo;
}


/**
 * csr_join_req_copy_he_cap() - Copy HE cap into CSR Join Req
 * @csr_join_req: pointer to CSR Join Req
 * @session: pointer to CSR session
 *
 * Return: None
 */
static void csr_join_req_copy_he_cap(tSirSmeJoinReq *csr_join_req,
		struct csr_roam_session *session)
{
	qdf_mem_copy(&csr_join_req->he_config, &session->he_config,
		     sizeof(session->he_config));
}

/**
 * csr_start_bss_copy_he_cap() - Copy HE cap into CSR Join Req
 * @req: pointer to START BSS Req
 * @session: pointer to CSR session
 *
 * Return: None
 */
static void csr_start_bss_copy_he_cap(tSirSmeStartBssReq *req,
			struct csr_roam_session *session)
{
	qdf_mem_copy(&req->he_config, &session->he_config,
		     sizeof(session->he_config));
}

void csr_update_session_he_cap(tpAniSirGlobal mac_ctx,
			struct csr_roam_session *session)
{
	mac_handle_t mac_hdl = MAC_HANDLE(mac_ctx);
	uint32_t value = 0;
	tDot11fIEhe_cap *he_cap = &session->he_config;
	he_cap->present = true;

	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_CONTROL, &value);
	he_cap->htc_he = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TWT_REQUESTOR, &value);
	he_cap->twt_request = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TWT_RESPONDER, &value);
	he_cap->twt_responder = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_FRAGMENTATION, &value);
	he_cap->fragmentation = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MAX_FRAG_MSDU, &value);
	he_cap->max_num_frag_msdu = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MIN_FRAG_SIZE, &value);
	he_cap->min_frag_size = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TRIG_PAD, &value);
	he_cap->trigger_frm_mac_pad = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MTID_AGGR, &value);
	he_cap->multi_tid_aggr = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_LINK_ADAPTATION, &value);
	he_cap->he_link_adaptation = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_ALL_ACK, &value);
	he_cap->all_ack = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_UL_MU_RSP_SCHEDULING, &value);
	he_cap->ul_mu_rsp_sched = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_BUFFER_STATUS_RPT, &value);
	he_cap->a_bsr = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_BCAST_TWT, &value);
	he_cap->broadcast_twt = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_BA_32BIT, &value);
	he_cap->ba_32bit_bitmap = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MU_CASCADING, &value);
	he_cap->mu_cascade = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MULTI_TID, &value);
	he_cap->ack_enabled_multitid = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_DL_MU_BA, &value);
	he_cap->dl_mu_ba = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_OMI, &value);
	he_cap->omi_a_ctrl = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_OFDMA_RA, &value);
	he_cap->ofdma_ra = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MAX_AMPDU_LEN, &value);
	he_cap->max_ampdu_len = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_AMSDU_FRAG, &value);
	he_cap->amsdu_frag = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_FLEX_TWT_SCHED, &value);
	he_cap->flex_twt_sched = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_RX_CTRL, &value);
	he_cap->rx_ctrl_frame = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_BSRP_AMPDU_AGGR, &value);
	he_cap->bsrp_ampdu_aggr = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_QTP, &value);
	he_cap->qtp = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_A_BQR, &value);
	he_cap->a_bqr = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_SR_RESPONDER, &value);
	he_cap->sr_responder = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_NDP_FEEDBACK_SUPP, &value);
	he_cap->ndp_feedback_supp = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_OPS_SUPP, &value);
	he_cap->ops_supp = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_AMSDU_IN_AMPDU, &value);
	he_cap->amsdu_in_ampdu = value;

	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_DUAL_BAND, &value);
	he_cap->dual_band = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_CHAN_WIDTH, &value);
	he_cap->chan_width_0 = HE_CH_WIDTH_GET_BIT(value, 0);
	he_cap->chan_width_1 = HE_CH_WIDTH_GET_BIT(value, 1);
	he_cap->chan_width_2 = HE_CH_WIDTH_GET_BIT(value, 2);
	he_cap->chan_width_3 = HE_CH_WIDTH_GET_BIT(value, 3);
	he_cap->chan_width_4 = HE_CH_WIDTH_GET_BIT(value, 4);
	he_cap->chan_width_5 = HE_CH_WIDTH_GET_BIT(value, 5);
	he_cap->chan_width_6 = HE_CH_WIDTH_GET_BIT(value, 6);

	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_RX_PREAM_PUNC, &value);
	he_cap->rx_pream_puncturing = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_CLASS_OF_DEVICE, &value);
	he_cap->device_class = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_LDPC, &value);
	he_cap->ldpc_coding = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_LTF_PPDU, &value);
	he_cap->he_1x_ltf_800_gi_ppdu = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MIDAMBLE_RX_MAX_NSTS, &value);
	he_cap->midamble_rx_max_nsts = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_LTF_NDP, &value);
	he_cap->he_4x_ltf_3200_gi_ndp = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TX_STBC_LT80, &value);
	he_cap->tx_stbc_lt_80mhz = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_RX_STBC_LT80, &value);
	he_cap->rx_stbc_lt_80mhz = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_DOPPLER, &value);
	he_cap->doppler = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_UL_MUMIMO, &value);
	he_cap->ul_mu = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_DCM_TX, &value);
	he_cap->dcm_enc_tx = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_DCM_RX, &value);
	he_cap->dcm_enc_rx = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MU_PPDU, &value);
	he_cap->ul_he_mu = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_SU_BEAMFORMEE, &value);
	he_cap->su_beamformee = value;
	if (he_cap->su_beamformee) {
		sme_cfg_get_int(mac_hdl, WNI_CFG_HE_BFEE_STS_LT80, &value);
		he_cap->bfee_sts_lt_80 = value;
		sme_cfg_get_int(mac_hdl, WNI_CFG_HE_BFEE_STS_GT80, &value);
		he_cap->bfee_sts_gt_80 = value;
	} else {
		he_cap->bfee_sts_lt_80 = 0;
		he_cap->bfee_sts_gt_80 = 0;
	}
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_SU_BEAMFORMER, &value);
	he_cap->su_beamformer = value;
	if (he_cap->su_beamformer) {
		sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MU_BEAMFORMER, &value);
		he_cap->mu_beamformer = value;
		sme_cfg_get_int(mac_hdl, WNI_CFG_HE_NUM_SOUND_LT80, &value);
		he_cap->num_sounding_lt_80 = value;
		sme_cfg_get_int(mac_hdl, WNI_CFG_HE_NUM_SOUND_GT80, &value);
		he_cap->num_sounding_gt_80 = value;
	} else {
		he_cap->mu_beamformer = 0;
		he_cap->num_sounding_lt_80 = 0;
		he_cap->num_sounding_gt_80 = 0;
	}
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_SU_FEED_TONE16, &value);
	he_cap->su_feedback_tone16 = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MU_FEED_TONE16, &value);
	he_cap->mu_feedback_tone16 = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_CODEBOOK_SU, &value);
	he_cap->codebook_su = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_CODEBOOK_MU, &value);
	he_cap->codebook_mu = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_BFRM_FEED, &value);
	he_cap->beamforming_feedback = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_ER_SU_PPDU, &value);
	he_cap->he_er_su_ppdu = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_DL_PART_BW, &value);
	he_cap->dl_mu_mimo_part_bw = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_PPET_PRESENT, &value);
	he_cap->ppet_present = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_SRP, &value);
	he_cap->srp = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_POWER_BOOST, &value);
	he_cap->power_boost = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_4x_LTF_GI, &value);
	he_cap->he_ltf_800_gi_4x = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MAX_NC, &value);
	he_cap->max_nc = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TX_STBC_GT80, &value);
	he_cap->tx_stbc_gt_80mhz = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_RX_STBC_GT80, &value);
	he_cap->rx_stbc_gt_80mhz = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_ER_4x_LTF_GI, &value);
	he_cap->er_he_ltf_800_gi_4x = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_PPDU_20_IN_40MHZ_2G, &value);
	he_cap->he_ppdu_20_in_40Mhz_2G = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_PPDU_20_IN_160_80P80MHZ, &value);
	he_cap->he_ppdu_20_in_160_80p80Mhz = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_PPDU_80_IN_160_80P80MHZ, &value);
	he_cap->he_ppdu_80_in_160_80p80Mhz = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_ER_1X_HE_LTF_GI, &value);
	he_cap->er_1x_he_ltf_gi = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_MIDAMBLE_RX_1X_HE_LTF, &value);
	he_cap->midamble_rx_1x_he_ltf = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_RX_MCS_MAP_LT_80, &value);
	he_cap->rx_he_mcs_map_lt_80 = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TX_MCS_MAP_LT_80, &value);
	he_cap->tx_he_mcs_map_lt_80 = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_RX_MCS_MAP_160, &value);
	*((uint16_t *)he_cap->rx_he_mcs_map_160) = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TX_MCS_MAP_160, &value);
	*((uint16_t *)he_cap->tx_he_mcs_map_160) = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_RX_MCS_MAP_80_80, &value);
	*((uint16_t *)he_cap->rx_he_mcs_map_80_80) = value;
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_TX_MCS_MAP_80_80, &value);
	*((uint16_t *)he_cap->tx_he_mcs_map_80_80) = value;

	if (he_cap->ppet_present) {
		value = WNI_CFG_HE_PPET_LEN;
		/* till now operating channel is not decided yet, use 5g cap */
		sme_cfg_get_str(mac_hdl, WNI_CFG_HE_PPET_5G,
				he_cap->ppet.ppe_threshold.ppe_th, &value);
		he_cap->ppet.ppe_threshold.num_ppe_th =
			lim_truncate_ppet(he_cap->ppet.ppe_threshold.ppe_th,
					  value);
	} else {
		he_cap->ppet.ppe_threshold.num_ppe_th = 0;
	}
	sme_cfg_get_int(mac_hdl, WNI_CFG_HE_STA_OBSSPD, &value);
	session->he_sta_obsspd = value;
}

#else
static inline void csr_update_he_config_param(tpAniSirGlobal mac_ctx,
					      tCsrConfigParam *param)
{
}

static inline void csr_get_he_config_param(tCsrConfigParam *param,
					   tpAniSirGlobal mac_ctx)
{
}

static inline void csr_join_req_copy_he_cap(tSirSmeJoinReq *csr_join_req,
			struct csr_roam_session *session)
{
}

static inline void csr_start_bss_copy_he_cap(tSirSmeStartBssReq *req,
			struct csr_roam_session *session)
{
}

#endif

/**
 * csr_update_11k_offload_config_param() - Update 11k neighbor report config
 *
 * @csr_config: pointer to csr_config in MAC context
 * @pParam: pointer to config params from HDD
 *
 * Return: none
 */
static
void csr_update_11k_offload_config_param(struct csr_config *csr_config,
					tCsrConfigParam *param)
{
	csr_config->offload_11k_enable_bitmask =
		param->offload_11k_enable_bitmask;
	csr_config->neighbor_report_offload.params_bitmask =
		param->neighbor_report_offload.params_bitmask;
	csr_config->neighbor_report_offload.time_offset =
		param->neighbor_report_offload.time_offset;
	csr_config->neighbor_report_offload.low_rssi_offset =
		param->neighbor_report_offload.low_rssi_offset;
	csr_config->neighbor_report_offload.bmiss_count_trigger =
		param->neighbor_report_offload.bmiss_count_trigger;
	csr_config->neighbor_report_offload.per_threshold_offset =
		param->neighbor_report_offload.per_threshold_offset;
	csr_config->neighbor_report_offload.
		neighbor_report_cache_timeout =
		param->neighbor_report_offload.
		neighbor_report_cache_timeout;
	csr_config->neighbor_report_offload.
		max_neighbor_report_req_cap =
		param->neighbor_report_offload.
		max_neighbor_report_req_cap;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD

/**
 * csr_get_roam_preauth_config_param() - Get the roam preauth params
 *
 * @csr_config: pointer to csr_config in MAC context
 * @pParam: pointer to config params from HDD
 *
 * Return: none
 */

static void csr_get_roam_preauth_config_param(tCsrConfigParam *pparam,
					      struct csr_config *cfg_params)
{
	pparam->roam_preauth_no_ack_timeout =
		cfg_params->roam_preauth_no_ack_timeout;
	pparam->roam_preauth_retry_count = cfg_params->roam_preauth_retry_count;
}

/**
 * csr_change_default_roam_preauth_params() - Update roam preauth params
 *
 * @pmac: pointer to MAC context
 * @pParam: pointer to config params from HDD
 *
 * Return: none
 */
static void csr_change_default_roam_preauth_params(tpAniSirGlobal pmac,
						   tCsrConfigParam *pparam)
{
		pmac->roam.configParam.roam_preauth_retry_count =
			pparam->roam_preauth_retry_count;
		pmac->roam.configParam.roam_preauth_no_ack_timeout =
			pparam->roam_preauth_no_ack_timeout;
}
#else
static void csr_change_default_roam_preauth_params(tpAniSirGlobal pmac,
						   tCsrConfigParam *pparam)
{
}

static void csr_get_roam_preauth_config_param(tCsrConfigParam *pparam,
					      struct csr_config *cfg_params)
{
}
#endif

#ifdef WLAN_ADAPTIVE_11R
static void
csr_update_adaptive_11r_config_param(tpAniSirGlobal mac, tCsrConfigParam *param)
{
	mac->roam.configParam.enable_adaptive_11r =
		param->enable_adaptive_11r;
}
#else
static void
csr_update_adaptive_11r_config_param(tpAniSirGlobal mac, tCsrConfigParam *param)
{
}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void
csr_update_sae_single_pmk_cfg_param(tpAniSirGlobal mac, tCsrConfigParam *param)
{
	mac->roam.configParam.sae_single_pmk_feature_enabled =
		param->sae_single_pmk_feature_enabled;
}
#else
static void
csr_update_sae_single_pmk_cfg_param(tpAniSirGlobal mac, tCsrConfigParam *param)
{
}
#endif

QDF_STATUS csr_change_default_config_param(tpAniSirGlobal pMac,
					   tCsrConfigParam *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	int i;

	if (pParam) {
		pMac->roam.configParam.pkt_err_disconn_th =
			pParam->pkt_err_disconn_th;
		pMac->roam.configParam.is_force_1x1_enable =
			pParam->is_force_1x1_enable;
		pMac->roam.configParam.WMMSupportMode = pParam->WMMSupportMode;
		cfg_set_int(pMac, WNI_CFG_WME_ENABLED,
			(pParam->WMMSupportMode == eCsrRoamWmmNoQos) ? 0 : 1);
		pMac->roam.configParam.Is11eSupportEnabled =
			pParam->Is11eSupportEnabled;
		pMac->roam.configParam.FragmentationThreshold =
			pParam->FragmentationThreshold;
		pMac->roam.configParam.Is11dSupportEnabled =
			pParam->Is11dSupportEnabled;
		pMac->roam.configParam.Is11hSupportEnabled =
			pParam->Is11hSupportEnabled;

		pMac->roam.configParam.fenableMCCMode = pParam->fEnableMCCMode;
		pMac->roam.configParam.mcc_rts_cts_prot_enable =
						pParam->mcc_rts_cts_prot_enable;
		pMac->roam.configParam.mcc_bcast_prob_resp_enable =
					pParam->mcc_bcast_prob_resp_enable;
		pMac->roam.configParam.fAllowMCCGODiffBI =
			pParam->fAllowMCCGODiffBI;

		/* channelBondingMode5GHz plays a dual role right now
		 * INFRA STA will use this non zero value as CB enabled
		 * and SOFTAP will use this non-zero value to determine
		 * the secondary channel offset. This is how
		 * channelBondingMode5GHz works now and this is kept intact
		 * to avoid any cfg.ini change.
		 */
		if (pParam->channelBondingMode24GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 2.4GHz band %d, CB DISABLED",
				pParam->channelBondingMode24GHz);
		pMac->roam.configParam.channelBondingMode24GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
						channelBondingMode24GHz);
		if (pParam->channelBondingMode5GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 5GHz band %d, CB DISABLED",
				pParam->channelBondingMode5GHz);
		pMac->roam.configParam.channelBondingMode5GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
							channelBondingMode5GHz);
		pMac->roam.configParam.RTSThreshold = pParam->RTSThreshold;
		pMac->roam.configParam.phyMode = pParam->phyMode;
		pMac->roam.configParam.shortSlotTime = pParam->shortSlotTime;
		pMac->roam.configParam.HeartbeatThresh24 =
			pParam->HeartbeatThresh24;
		pMac->roam.configParam.HeartbeatThresh50 =
			pParam->HeartbeatThresh50;
		pMac->roam.configParam.ProprietaryRatesEnabled =
			pParam->ProprietaryRatesEnabled;
		pMac->roam.configParam.AdHocChannel24 = pParam->AdHocChannel24;
		pMac->roam.configParam.AdHocChannel5G = pParam->AdHocChannel5G;
		pMac->roam.configParam.bandCapability = pParam->bandCapability;
		pMac->roam.configParam.wep_tkip_in_he = pParam->wep_tkip_in_he;
		pMac->roam.configParam.neighborRoamConfig.
			delay_before_vdev_stop =
			pParam->neighborRoamConfig.delay_before_vdev_stop;

		/* if HDD passed down non zero values then only update, */
		/* otherwise keep using the defaults */
		if (pParam->initial_scan_no_dfs_chnl) {
			pMac->roam.configParam.initial_scan_no_dfs_chnl =
				pParam->initial_scan_no_dfs_chnl;
		}
		if (pParam->nInitialDwellTime) {
			pMac->roam.configParam.nInitialDwellTime =
				pParam->nInitialDwellTime;
		}
		if (pParam->nActiveMaxChnTime) {
			pMac->roam.configParam.nActiveMaxChnTime =
				pParam->nActiveMaxChnTime;
			cfg_set_int(pMac, WNI_CFG_ACTIVE_MAXIMUM_CHANNEL_TIME,
				    pParam->nActiveMaxChnTime);
		}
		if (pParam->nActiveMinChnTime) {
			pMac->roam.configParam.nActiveMinChnTime =
				pParam->nActiveMinChnTime;
			cfg_set_int(pMac, WNI_CFG_ACTIVE_MINIMUM_CHANNEL_TIME,
				    pParam->nActiveMinChnTime);
		}
		if (pParam->nPassiveMaxChnTime) {
			pMac->roam.configParam.nPassiveMaxChnTime =
				pParam->nPassiveMaxChnTime;
			cfg_set_int(pMac, WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME,
				    pParam->nPassiveMaxChnTime);
		}
		if (pParam->nPassiveMinChnTime) {
			pMac->roam.configParam.nPassiveMinChnTime =
				pParam->nPassiveMinChnTime;
			cfg_set_int(pMac, WNI_CFG_PASSIVE_MINIMUM_CHANNEL_TIME,
				    pParam->nPassiveMinChnTime);
		}
		if (pParam->nActiveMaxChnTimeConc) {
			pMac->roam.configParam.nActiveMaxChnTimeConc =
				pParam->nActiveMaxChnTimeConc;
		}
		if (pParam->nActiveMinChnTimeConc) {
			pMac->roam.configParam.nActiveMinChnTimeConc =
				pParam->nActiveMinChnTimeConc;
		}
		if (pParam->nPassiveMaxChnTimeConc) {
			pMac->roam.configParam.nPassiveMaxChnTimeConc =
				pParam->nPassiveMaxChnTimeConc;
		}
		if (pParam->nPassiveMinChnTimeConc) {
			pMac->roam.configParam.nPassiveMinChnTimeConc =
				pParam->nPassiveMinChnTimeConc;
		}
		pMac->roam.configParam.nRestTimeConc = pParam->nRestTimeConc;
		pMac->roam.configParam.min_rest_time_conc =
			pParam->min_rest_time_conc;
		pMac->roam.configParam.idle_time_conc = pParam->idle_time_conc;

		pMac->roam.configParam.eBand = pParam->eBand;
		pMac->roam.configParam.uCfgDot11Mode =
			csr_get_cfg_dot11_mode_from_csr_phy_mode(NULL,
							pMac->roam.configParam.
							phyMode,
							pMac->roam.configParam.
						ProprietaryRatesEnabled);
		/* if HDD passed down non zero values for age params,
		 * then only update, otherwise keep using the defaults
		 */
		if (pParam->nScanResultAgeCount) {
			pMac->roam.configParam.agingCount =
				pParam->nScanResultAgeCount;
		}
		if (pParam->obss_width_interval) {
			pMac->roam.configParam.obss_width_interval =
				pParam->obss_width_interval;
			cfg_set_int(pMac,
				WNI_CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL,
				pParam->obss_width_interval);
		}
		if (pParam->obss_active_dwelltime) {
			pMac->roam.configParam.obss_active_dwelltime =
				pParam->obss_active_dwelltime;
			cfg_set_int(pMac,
				WNI_CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME,
				pParam->obss_active_dwelltime);
		}
		if (pParam->obss_passive_dwelltime) {
			pMac->roam.configParam.obss_passive_dwelltime =
				pParam->obss_passive_dwelltime;
			cfg_set_int(pMac,
				WNI_CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME,
				pParam->obss_passive_dwelltime);
		}

		pMac->first_scan_bucket_threshold =
			pParam->first_scan_bucket_threshold;
		csr_assign_rssi_for_category(pMac,
			pMac->first_scan_bucket_threshold,
			pParam->bCatRssiOffset);
		pMac->roam.configParam.fSupplicantCountryCodeHasPriority =
			pParam->fSupplicantCountryCodeHasPriority;
		pMac->roam.configParam.vccRssiThreshold =
			pParam->vccRssiThreshold;
		pMac->roam.configParam.vccUlMacLossThreshold =
			pParam->vccUlMacLossThreshold;
		pMac->roam.configParam.statsReqPeriodicity =
			pParam->statsReqPeriodicity;
		pMac->roam.configParam.statsReqPeriodicityInPS =
			pParam->statsReqPeriodicityInPS;
		/* Assign this before calling csr_init11d_info */
		pMac->roam.configParam.nTxPowerCap = pParam->nTxPowerCap;
		pMac->roam.configParam.allow_tpc_from_ap =
				pParam->allow_tpc_from_ap;
		if (wlan_reg_11d_enabled_on_host(pMac->psoc))
			status = csr_init11d_info(pMac, &pParam->Csr11dinfo);
		else
			pMac->scan.curScanType = eSIR_ACTIVE_SCAN;

		/* Initialize the power + channel information if 11h is
		 * enabled. If 11d is enabled this information has already
		 * been initialized
		 */
		if (csr_is11h_supported(pMac) &&
				!wlan_reg_11d_enabled_on_host(pMac->psoc))
			csr_init_channel_power_list(pMac, &pParam->Csr11dinfo);

		pMac->roam.configParam.isFastTransitionEnabled =
			pParam->isFastTransitionEnabled;
		pMac->roam.configParam.RoamRssiDiff = pParam->RoamRssiDiff;
		pMac->roam.configParam.rssi_abs_thresh =
						pParam->rssi_abs_thresh;
		pMac->roam.configParam.nRoamPrefer5GHz =
			pParam->nRoamPrefer5GHz;
		pMac->roam.configParam.nRoamIntraBand = pParam->nRoamIntraBand;
		pMac->roam.configParam.isWESModeEnabled =
			pParam->isWESModeEnabled;
		pMac->roam.configParam.nProbes = pParam->nProbes;
		pMac->roam.configParam.nRoamScanHomeAwayTime =
			pParam->nRoamScanHomeAwayTime;
		pMac->roam.configParam.isRoamOffloadScanEnabled =
			pParam->isRoamOffloadScanEnabled;
		pMac->roam.configParam.bFastRoamInConIniFeatureEnabled =
			pParam->bFastRoamInConIniFeatureEnabled;
		pMac->roam.configParam.isFastRoamIniFeatureEnabled =
			pParam->isFastRoamIniFeatureEnabled;
		qdf_mem_copy(&pMac->roam.configParam.csr_mawc_config,
				&pParam->csr_mawc_config,
				sizeof(pParam->csr_mawc_config));
#ifdef FEATURE_WLAN_ESE
		pMac->roam.configParam.isEseIniFeatureEnabled =
			pParam->isEseIniFeatureEnabled;
#endif
		qdf_mem_copy(&pMac->roam.configParam.neighborRoamConfig,
			     &pParam->neighborRoamConfig,
			     sizeof(tCsrNeighborRoamConfigParams));

		pMac->roam.configParam.addTSWhenACMIsOff =
			pParam->addTSWhenACMIsOff;
		pMac->scan.fEnableBypass11d = pParam->fEnableBypass11d;
		pMac->scan.fEnableDFSChnlScan = pParam->fEnableDFSChnlScan;
		pMac->scan.scanResultCfgAgingTime = pParam->scanCfgAgingTime;
		pMac->roam.configParam.fScanTwice = pParam->fScanTwice;
		pMac->scan.fFirstScanOnly2GChnl = pParam->fFirstScanOnly2GChnl;
		pMac->scan.max_scan_count = pParam->max_scan_count;

		/* This parameter is not available in cfg and not passed from
		 * upper layers. Instead it is initialized here This parametere
		 * is used in concurrency to determine if there are concurrent
		 * active sessions. Is used as a temporary fix to disconnect
		 * all active sessions when BMPS enabled so the active session
		 * if Infra STA will automatically connect back and resume BMPS
		 * since resume BMPS is not working when moving from concurrent
		 * to single session
		 */
		/* Remove this code once SLM_Sessionization is supported */
		/* BMPS_WORKAROUND_NOT_NEEDED */
		pMac->roam.configParam.doBMPSWorkaround = 0;

		pMac->roam.configParam.nVhtChannelWidth =
			pParam->nVhtChannelWidth;
		pMac->roam.configParam.enable_subfee_vendor_vhtie =
					pParam->enable_subfee_vendor_vhtie;
		pMac->roam.configParam.enable_txbf_sap_mode =
			pParam->enable_txbf_sap_mode;
		pMac->roam.configParam.enable_vht20_mcs9 =
			pParam->enable_vht20_mcs9;
		pMac->roam.configParam.enable2x2 = pParam->enable2x2;
		pMac->roam.configParam.enableVhtFor24GHz =
			pParam->enableVhtFor24GHz;
		pMac->roam.configParam.enableVhtpAid = pParam->enableVhtpAid;
		pMac->roam.configParam.enableVhtGid = pParam->enableVhtGid;
		pMac->roam.configParam.enableAmpduPs = pParam->enableAmpduPs;
		pMac->roam.configParam.enableHtSmps = pParam->enableHtSmps;
		pMac->roam.configParam.htSmps = pParam->htSmps;
		pMac->roam.configParam.send_smps_action =
			pParam->send_smps_action;
		pMac->roam.configParam.tx_ldpc_enable = pParam->enable_tx_ldpc;
		pMac->roam.configParam.rx_ldpc_enable = pParam->enable_rx_ldpc;
		pMac->roam.configParam.disable_high_ht_mcs_2x2 =
					pParam->disable_high_ht_mcs_2x2;
		pMac->roam.configParam.ignore_peer_erp_info =
			pParam->ignore_peer_erp_info;
		pMac->roam.configParam.max_amsdu_num =
			pParam->max_amsdu_num;
		pMac->roam.configParam.nSelect5GHzMargin =
			pParam->nSelect5GHzMargin;
		pMac->roam.configParam.ho_delay_for_rx =
			pParam->ho_delay_for_rx;
		csr_change_default_roam_preauth_params(pMac, pParam);
		pMac->roam.configParam.min_delay_btw_roam_scans =
			pParam->min_delay_btw_roam_scans;
		pMac->roam.configParam.roam_trigger_reason_bitmask =
			pParam->roam_trigger_reason_bitmask;
		pMac->roam.configParam.roaming_scan_policy =
			pParam->roaming_scan_policy;
		pMac->roam.configParam.isCoalesingInIBSSAllowed =
			pParam->isCoalesingInIBSSAllowed;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		pMac->roam.configParam.cc_switch_mode = pParam->cc_switch_mode;
#endif
		pMac->roam.configParam.allowDFSChannelRoam =
			pParam->allowDFSChannelRoam;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		pMac->roam.configParam.isRoamOffloadEnabled =
			pParam->isRoamOffloadEnabled;
		pMac->roam.configParam.roam_triggers =
			pParam->roam_triggers;
		pMac->roam.configParam.enable_disconnect_roam_offload =
			pParam->enable_disconnect_roam_offload;
		pMac->roam.configParam.enable_idle_roam =
			pParam->enable_idle_roam;
		pMac->roam.configParam.idle_roam_rssi_delta =
			pParam->idle_roam_rssi_delta;
		pMac->roam.configParam.idle_roam_inactive_time =
			pParam->idle_roam_inactive_time;
		pMac->roam.configParam.idle_data_packet_count =
			pParam->idle_data_packet_count;
		pMac->roam.configParam.idle_roam_band =
			pParam->idle_roam_band;
		pMac->roam.configParam.idle_roam_min_rssi =
			pParam->idle_roam_min_rssi;
#endif
		pMac->roam.configParam.obssEnabled = pParam->obssEnabled;
		pMac->roam.configParam.vendor_vht_sap =
			pParam->vendor_vht_sap;
		pMac->roam.configParam.conc_custom_rule1 =
			pParam->conc_custom_rule1;
		pMac->roam.configParam.conc_custom_rule2 =
			pParam->conc_custom_rule2;
		pMac->roam.configParam.is_sta_connection_in_5gz_enabled =
			pParam->is_sta_connection_in_5gz_enabled;
		pMac->roam.configParam.send_deauth_before_con =
			pParam->send_deauth_before_con;

		pMac->enable_dot11p = pParam->enable_dot11p;
		pMac->roam.configParam.early_stop_scan_enable =
			pParam->early_stop_scan_enable;
		pMac->roam.configParam.early_stop_scan_min_threshold =
			pParam->early_stop_scan_min_threshold;
		pMac->roam.configParam.early_stop_scan_max_threshold =
			pParam->early_stop_scan_max_threshold;
		pMac->isCoalesingInIBSSAllowed =
			pParam->isCoalesingInIBSSAllowed;

		pMac->roam.configParam.roam_params.dense_rssi_thresh_offset =
			pParam->roam_dense_rssi_thresh_offset;
		pMac->roam.configParam.roam_params.dense_min_aps_cnt =
			pParam->roam_dense_min_aps;
		pMac->roam.configParam.roam_params.traffic_threshold =
			pParam->roam_dense_traffic_thresh;

		pMac->roam.configParam.roam_params.bg_scan_bad_rssi_thresh =
			pParam->roam_bg_scan_bad_rssi_thresh;
		pMac->roam.configParam.roam_params.bg_scan_client_bitmap =
			pParam->roam_bg_scan_client_bitmap;
		pMac->roam.configParam.roam_params.
			roam_bad_rssi_thresh_offset_2g =
			pParam->roam_bad_rssi_thresh_offset_2g;
		pMac->roam.configParam.roam_params.roam_data_rssi_threshold_triggers =
			pParam->roam_data_rssi_threshold_triggers;
		pMac->roam.configParam.roam_params.roam_data_rssi_threshold =
			pParam->roam_data_rssi_threshold;
		pMac->roam.configParam.roam_params.rx_data_inactivity_time =
			pParam->rx_data_inactivity_time;

		pMac->roam.configParam.enable_ftopen =
			pParam->enable_ftopen;
		pMac->roam.configParam.scan_adaptive_dwell_mode =
			pParam->scan_adaptive_dwell_mode;
		pMac->roam.configParam.scan_adaptive_dwell_mode_nc =
			pParam->scan_adaptive_dwell_mode_nc;
		pMac->roam.configParam.roamscan_adaptive_dwell_mode =
			pParam->roamscan_adaptive_dwell_mode;

		pMac->roam.configParam.per_roam_config.enable =
			pParam->per_roam_config.enable;
		pMac->roam.configParam.per_roam_config.tx_high_rate_thresh =
			pParam->per_roam_config.tx_high_rate_thresh;
		pMac->roam.configParam.per_roam_config.rx_high_rate_thresh =
			pParam->per_roam_config.rx_high_rate_thresh;
		pMac->roam.configParam.per_roam_config.tx_low_rate_thresh =
			pParam->per_roam_config.tx_low_rate_thresh;
		pMac->roam.configParam.per_roam_config.rx_low_rate_thresh =
			pParam->per_roam_config.rx_low_rate_thresh;
		pMac->roam.configParam.per_roam_config.tx_rate_thresh_percnt =
			pParam->per_roam_config.tx_rate_thresh_percnt;
		pMac->roam.configParam.per_roam_config.rx_rate_thresh_percnt =
			pParam->per_roam_config.rx_rate_thresh_percnt;
		pMac->roam.configParam.per_roam_config.per_rest_time =
			pParam->per_roam_config.per_rest_time;
		pMac->roam.configParam.per_roam_config.tx_per_mon_time =
			pParam->per_roam_config.tx_per_mon_time;
		pMac->roam.configParam.per_roam_config.rx_per_mon_time =
			pParam->per_roam_config.rx_per_mon_time;
		pMac->roam.configParam.per_roam_config.min_candidate_rssi =
			pParam->per_roam_config.min_candidate_rssi;

		pMac->fEnableDebugLog = pParam->fEnableDebugLog;

		/* update interface configuration */
		pMac->sme.max_intf_count = pParam->max_intf_count;

		pMac->enable5gEBT = pParam->enable5gEBT;
		pMac->sme.enableSelfRecovery = pParam->enableSelfRecovery;

		pMac->f_sta_miracast_mcc_rest_time_val =
			pParam->f_sta_miracast_mcc_rest_time_val;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		pMac->sap.sap_channel_avoidance =
			pParam->sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
		pMac->sap.acs_with_more_param =
			pParam->acs_with_more_param;

		pMac->f_prefer_non_dfs_on_radar =
			pParam->f_prefer_non_dfs_on_radar;

		pMac->sme.ps_global_info.ps_enabled =
			pParam->is_ps_enabled;
		pMac->sme.ps_global_info.auto_bmps_timer_val =
			pParam->auto_bmps_timer_val;
		pMac->roam.configParam.ignore_peer_ht_opmode =
			pParam->ignore_peer_ht_opmode;
		pMac->dual_mac_feature_disable =
			pParam->dual_mac_feature_disable;
		pMac->sta_sap_scc_on_dfs_chan =
			pParam->sta_sap_scc_on_dfs_chan;
		pMac->roam.configParam.early_stop_scan_enable =
			pParam->early_stop_scan_enable;
		pMac->roam.configParam.early_stop_scan_min_threshold =
			pParam->early_stop_scan_min_threshold;
		pMac->roam.configParam.early_stop_scan_max_threshold =
			pParam->early_stop_scan_max_threshold;
		pMac->roam.configParam.enable_edca_params =
			pParam->enable_edca_params;
		pMac->roam.configParam.edca_vo_cwmin = pParam->edca_vo_cwmin;
		pMac->roam.configParam.edca_vi_cwmin = pParam->edca_vi_cwmin;
		pMac->roam.configParam.edca_bk_cwmin = pParam->edca_bk_cwmin;
		pMac->roam.configParam.edca_be_cwmin = pParam->edca_be_cwmin;

		pMac->roam.configParam.edca_vo_cwmax = pParam->edca_vo_cwmax;
		pMac->roam.configParam.edca_vi_cwmax = pParam->edca_vi_cwmax;
		pMac->roam.configParam.edca_bk_cwmax = pParam->edca_bk_cwmax;
		pMac->roam.configParam.edca_be_cwmax = pParam->edca_be_cwmax;

		pMac->roam.configParam.edca_vo_aifs = pParam->edca_vo_aifs;
		pMac->roam.configParam.edca_vi_aifs = pParam->edca_vi_aifs;
		pMac->roam.configParam.edca_bk_aifs = pParam->edca_bk_aifs;
		pMac->roam.configParam.edca_be_aifs = pParam->edca_be_aifs;

		pMac->roam.configParam.enable_fatal_event =
			pParam->enable_fatal_event;
		pMac->roam.configParam.sta_roam_policy.dfs_mode =
			pParam->sta_roam_policy_params.dfs_mode;
		pMac->roam.configParam.sta_roam_policy.skip_unsafe_channels =
			pParam->sta_roam_policy_params.skip_unsafe_channels;
		pMac->roam.configParam.sta_roam_policy.sap_operating_band =
			pParam->sta_roam_policy_params.sap_operating_band;

		pMac->roam.configParam.tx_aggregation_size =
			pParam->tx_aggregation_size;
		pMac->roam.configParam.tx_aggregation_size_be =
			pParam->tx_aggregation_size_be;
		pMac->roam.configParam.tx_aggregation_size_bk =
			pParam->tx_aggregation_size_bk;
		pMac->roam.configParam.tx_aggregation_size_vi =
			pParam->tx_aggregation_size_vi;
		pMac->roam.configParam.tx_aggregation_size_vo =
			pParam->tx_aggregation_size_vo;
		pMac->roam.configParam.rx_aggregation_size =
			pParam->rx_aggregation_size;
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_be =
			pParam->tx_aggr_sw_retry_threshold_be;
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_bk =
			pParam->tx_aggr_sw_retry_threshold_bk;
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_vi =
			pParam->tx_aggr_sw_retry_threshold_vi;
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_vo =
			pParam->tx_aggr_sw_retry_threshold_vo;
		pMac->roam.configParam.tx_aggr_sw_retry_threshold =
			pParam->tx_aggr_sw_retry_threshold;
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_be =
			pParam->tx_non_aggr_sw_retry_threshold_be;
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_bk =
			pParam->tx_non_aggr_sw_retry_threshold_bk;
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_vi =
			pParam->tx_non_aggr_sw_retry_threshold_vi;
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_vo =
			pParam->tx_non_aggr_sw_retry_threshold_vo;
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold =
			pParam->tx_non_aggr_sw_retry_threshold;
		pMac->roam.configParam.enable_bcast_probe_rsp =
			pParam->enable_bcast_probe_rsp;
		pMac->roam.configParam.is_fils_enabled =
			pParam->is_fils_enabled;
		pMac->roam.configParam.qcn_ie_support =
			pParam->qcn_ie_support;
		pMac->roam.configParam.fils_max_chan_guard_time =
			pParam->fils_max_chan_guard_time;
		pMac->roam.configParam.disallow_duration =
			pParam->disallow_duration;
		pMac->roam.configParam.rssi_channel_penalization =
			pParam->rssi_channel_penalization;
		pMac->roam.configParam.num_disallowed_aps =
			pParam->num_disallowed_aps;
		pMac->roam.configParam.wlm_latency_enable =
			pParam->wlm_latency_enable;
		pMac->roam.configParam.wlm_latency_level =
			pParam->wlm_latency_level;
		for (i = 0; i < CSR_NUM_WLM_LATENCY_LEVEL; i++) {
			pMac->roam.configParam.wlm_latency_flags[i] =
				pParam->wlm_latency_flags[i];
		}
		pMac->roam.configParam.oce_feature_bitmap =
			pParam->oce_feature_bitmap;
		pMac->roam.configParam.roam_force_rssi_trigger =
			pParam->roam_force_rssi_trigger;

		pMac->roam.configParam.mbo_thresholds.
			mbo_candidate_rssi_thres =
			pParam->mbo_thresholds.mbo_candidate_rssi_thres;
		pMac->roam.configParam.mbo_thresholds.
			mbo_current_rssi_thres =
			pParam->mbo_thresholds.mbo_current_rssi_thres;
		pMac->roam.configParam.mbo_thresholds.
			mbo_current_rssi_mcc_thres =
			pParam->mbo_thresholds.mbo_current_rssi_mcc_thres;
		pMac->roam.configParam.mbo_thresholds.
			mbo_candidate_rssi_btc_thres =
			pParam->mbo_thresholds.mbo_candidate_rssi_btc_thres;

		qdf_mem_copy(&pMac->roam.configParam.bss_score_params,
			     &pParam->bss_score_params,
			     sizeof(struct sir_score_config));
		pMac->roam.configParam.btm_offload_config =
						     pParam->btm_offload_config;
		pMac->roam.configParam.pmkid_modes = pParam->pmkid_modes;
		pMac->roam.configParam.btm_solicited_timeout =
			pParam->btm_solicited_timeout;
		pMac->roam.configParam.btm_max_attempt_cnt =
			pParam->btm_max_attempt_cnt;
		pMac->roam.configParam.btm_sticky_time =
			pParam->btm_sticky_time;
		pMac->roam.configParam.btm_query_bitmask =
			pParam->btm_query_bitmask;
		pMac->roam.configParam.btm_trig_min_candidate_score =
				pParam->btm_trig_min_candidate_score;

		pMac->roam.configParam.btm_validity_timer =
				pParam->btm_validity_timer;
		pMac->roam.configParam.btm_disassoc_timer_threshold =
				pParam->btm_disassoc_timer_threshold;
		pMac->roam.configParam.enable_bss_load_roam_trigger =
				pParam->enable_bss_load_roam_trigger;
		pMac->roam.configParam.bss_load_threshold =
				pParam->bss_load_threshold;
		pMac->roam.configParam.bss_load_sample_time =
				pParam->bss_load_sample_time;
		pMac->roam.configParam.bss_load_trigger_rssi_threshold_5ghz =
				pParam->bss_load_trigger_rssi_threshold_5ghz;
		pMac->roam.configParam.bss_load_trigger_rssi_threshold_24ghz =
				pParam->bss_load_trigger_rssi_threshold_24ghz;

		pMac->roam.configParam.roam_scan_inactivity_time =
				pParam->roam_scan_inactivity_time;
		pMac->roam.configParam.roam_inactive_data_packet_count =
				pParam->roam_inactive_data_packet_count;
		pMac->roam.configParam.roam_scan_period_after_inactivity =
				pParam->roam_scan_period_after_inactivity;
		pMac->roam.configParam.disconnect_roam_min_rssi =
				pParam->disconnect_roam_min_rssi;
		pMac->roam.configParam.bmiss_roam_min_rssi =
				pParam->bmiss_roam_min_rssi;
		pMac->roam.configParam.btm_roam_score_delta =
				pParam->btm_roam_score_delta;
		pMac->roam.configParam.idle_roam_score_delta =
				pParam->idle_roam_score_delta;
		pMac->roam.configParam.disable_4way_hs_offload =
				pParam->disable_4way_hs_offload;

		csr_update_adaptive_11r_config_param(pMac, pParam);
		csr_update_sae_single_pmk_cfg_param(pMac, pParam);
		csr_update_he_config_param(pMac, pParam);
		csr_update_11k_offload_config_param(&pMac->roam.configParam,
						 pParam);
		pMac->roam.configParam.enable_pending_list_req =
					pParam->enable_pending_list_req;
		pMac->roam.configParam.sta_disable_roam =
					pParam->sta_disable_roam;
	}
	return status;
}

/**
 * csr_get_11k_offload_config_param() - Get 11k neighbor report config
 *
 * @csr_config: pointer to csr_config in MAC context
 * @pParam: pointer to config params from HDD
 *
 * Return: none
 */
static
void csr_get_11k_offload_config_param(struct csr_config *csr_config,
					tCsrConfigParam *param)
{
	param->offload_11k_enable_bitmask =
		csr_config->offload_11k_enable_bitmask;
	param->neighbor_report_offload.params_bitmask =
		csr_config->neighbor_report_offload.params_bitmask;
	param->neighbor_report_offload.time_offset =
		csr_config->neighbor_report_offload.time_offset;
	param->neighbor_report_offload.low_rssi_offset =
		csr_config->neighbor_report_offload.low_rssi_offset;
	param->neighbor_report_offload.bmiss_count_trigger =
		csr_config->neighbor_report_offload.bmiss_count_trigger;
	param->neighbor_report_offload.per_threshold_offset =
		csr_config->neighbor_report_offload.per_threshold_offset;
	param->neighbor_report_offload.neighbor_report_cache_timeout =
		csr_config->neighbor_report_offload.
		neighbor_report_cache_timeout;
	param->neighbor_report_offload.max_neighbor_report_req_cap =
		csr_config->neighbor_report_offload.
		max_neighbor_report_req_cap;
}

#ifdef WLAN_ADAPTIVE_11R
static void
csr_get_adaptive_11r_config(tpAniSirGlobal mac, tCsrConfigParam *param)
{
	param->enable_adaptive_11r = mac->roam.configParam.enable_adaptive_11r;
}
#else
static inline void
csr_get_adaptive_11r_config(tpAniSirGlobal mac, tCsrConfigParam *param)
{
}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void
csr_get_sae_single_pmk_config(tpAniSirGlobal mac, tCsrConfigParam *param)
{
	param->sae_single_pmk_feature_enabled =
			mac->roam.configParam.sae_single_pmk_feature_enabled;
}
#else
static inline void
csr_get_sae_single_pmk_config(tpAniSirGlobal mac, tCsrConfigParam *param)
{
}
#endif

QDF_STATUS csr_get_config_param(tpAniSirGlobal pMac, tCsrConfigParam *pParam)
{
	int i;
	struct csr_config *cfg_params = &pMac->roam.configParam;

	if (!pParam)
		return QDF_STATUS_E_INVAL;

	pParam->pkt_err_disconn_th = cfg_params->pkt_err_disconn_th;
	pParam->is_force_1x1_enable = cfg_params->is_force_1x1_enable;
	pParam->WMMSupportMode = cfg_params->WMMSupportMode;
	pParam->Is11eSupportEnabled = cfg_params->Is11eSupportEnabled;
	pParam->FragmentationThreshold = cfg_params->FragmentationThreshold;
	pParam->Is11dSupportEnabled = cfg_params->Is11dSupportEnabled;
	pParam->Is11hSupportEnabled = cfg_params->Is11hSupportEnabled;
	pParam->channelBondingMode24GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode24GHz);
	pParam->channelBondingMode5GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode5GHz);
	pParam->RTSThreshold = cfg_params->RTSThreshold;
	pParam->phyMode = cfg_params->phyMode;
	pParam->shortSlotTime = cfg_params->shortSlotTime;
	pParam->HeartbeatThresh24 = cfg_params->HeartbeatThresh24;
	pParam->HeartbeatThresh50 = cfg_params->HeartbeatThresh50;
	pParam->ProprietaryRatesEnabled = cfg_params->ProprietaryRatesEnabled;
	pParam->AdHocChannel24 = cfg_params->AdHocChannel24;
	pParam->AdHocChannel5G = cfg_params->AdHocChannel5G;
	pParam->bandCapability = cfg_params->bandCapability;
	pParam->nActiveMaxChnTime = cfg_params->nActiveMaxChnTime;
	pParam->nActiveMinChnTime = cfg_params->nActiveMinChnTime;
	pParam->nPassiveMaxChnTime = cfg_params->nPassiveMaxChnTime;
	pParam->nPassiveMinChnTime = cfg_params->nPassiveMinChnTime;
	pParam->nActiveMaxChnTimeConc = cfg_params->nActiveMaxChnTimeConc;
	pParam->nActiveMinChnTimeConc = cfg_params->nActiveMinChnTimeConc;
	pParam->nPassiveMaxChnTimeConc = cfg_params->nPassiveMaxChnTimeConc;
	pParam->nPassiveMinChnTimeConc = cfg_params->nPassiveMinChnTimeConc;
	pParam->nRestTimeConc = cfg_params->nRestTimeConc;
	pParam->min_rest_time_conc = cfg_params->min_rest_time_conc;
	pParam->idle_time_conc = cfg_params->idle_time_conc;
	pParam->eBand = cfg_params->eBand;
	pParam->nScanResultAgeCount = cfg_params->agingCount;
	pParam->bCatRssiOffset = cfg_params->bCatRssiOffset;
	pParam->fSupplicantCountryCodeHasPriority =
		cfg_params->fSupplicantCountryCodeHasPriority;
	pParam->vccRssiThreshold = cfg_params->vccRssiThreshold;
	pParam->vccUlMacLossThreshold = cfg_params->vccUlMacLossThreshold;
	pParam->nTxPowerCap = cfg_params->nTxPowerCap;
	pParam->allow_tpc_from_ap = cfg_params->allow_tpc_from_ap;
	pParam->statsReqPeriodicity = cfg_params->statsReqPeriodicity;
	pParam->statsReqPeriodicityInPS = cfg_params->statsReqPeriodicityInPS;
	pParam->addTSWhenACMIsOff = cfg_params->addTSWhenACMIsOff;
	pParam->fEnableBypass11d = pMac->scan.fEnableBypass11d;
	pParam->fEnableDFSChnlScan = pMac->scan.fEnableDFSChnlScan;
	pParam->fScanTwice = cfg_params->fScanTwice;
	pParam->fFirstScanOnly2GChnl = pMac->scan.fFirstScanOnly2GChnl;
	pParam->fEnableMCCMode = cfg_params->fenableMCCMode;
	pParam->fAllowMCCGODiffBI = cfg_params->fAllowMCCGODiffBI;
	pParam->scanCfgAgingTime = pMac->scan.scanResultCfgAgingTime;
	qdf_mem_copy(&pParam->neighborRoamConfig,
		     &cfg_params->neighborRoamConfig,
		     sizeof(tCsrNeighborRoamConfigParams));
	pParam->nVhtChannelWidth = cfg_params->nVhtChannelWidth;
	pParam->enable_subfee_vendor_vhtie =
				cfg_params->enable_subfee_vendor_vhtie;
	pParam->enable_txbf_sap_mode =
		cfg_params->enable_txbf_sap_mode;
	pParam->enable_vht20_mcs9 = cfg_params->enable_vht20_mcs9;
	pParam->enableVhtFor24GHz = cfg_params->enableVhtFor24GHz;
	pParam->ignore_peer_erp_info = cfg_params->ignore_peer_erp_info;
	pParam->enable2x2 = cfg_params->enable2x2;
	pParam->isFastTransitionEnabled = cfg_params->isFastTransitionEnabled;
	pParam->RoamRssiDiff = cfg_params->RoamRssiDiff;
	pParam->rssi_abs_thresh = cfg_params->rssi_abs_thresh;
	pParam->nRoamPrefer5GHz = cfg_params->nRoamPrefer5GHz;
	pParam->nRoamIntraBand = cfg_params->nRoamIntraBand;
	pParam->isWESModeEnabled = cfg_params->isWESModeEnabled;
	pParam->nProbes = cfg_params->nProbes;
	pParam->nRoamScanHomeAwayTime = cfg_params->nRoamScanHomeAwayTime;
	pParam->isRoamOffloadScanEnabled = cfg_params->isRoamOffloadScanEnabled;
	pParam->bFastRoamInConIniFeatureEnabled =
		cfg_params->bFastRoamInConIniFeatureEnabled;
	pParam->isFastRoamIniFeatureEnabled =
		cfg_params->isFastRoamIniFeatureEnabled;
#ifdef FEATURE_WLAN_ESE
	pParam->isEseIniFeatureEnabled = cfg_params->isEseIniFeatureEnabled;
#endif
	qdf_mem_copy(&pParam->neighborRoamConfig,
		     &cfg_params->neighborRoamConfig,
		     sizeof(tCsrNeighborRoamConfigParams));

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	pParam->cc_switch_mode = cfg_params->cc_switch_mode;
#endif
	pParam->enable_tx_ldpc = cfg_params->tx_ldpc_enable;
	pParam->enable_rx_ldpc = cfg_params->rx_ldpc_enable;
	pParam->wep_tkip_in_he = cfg_params->wep_tkip_in_he;
	pParam->disable_high_ht_mcs_2x2 = cfg_params->disable_high_ht_mcs_2x2;
	pParam->max_amsdu_num = cfg_params->max_amsdu_num;
	pParam->nSelect5GHzMargin = cfg_params->nSelect5GHzMargin;
	pParam->ho_delay_for_rx = cfg_params->ho_delay_for_rx;

	csr_get_roam_preauth_config_param(pParam, cfg_params);

	pParam->min_delay_btw_roam_scans = cfg_params->min_delay_btw_roam_scans;
	pParam->roam_trigger_reason_bitmask =
			cfg_params->roam_trigger_reason_bitmask;
	pParam->roaming_scan_policy =
			cfg_params->roaming_scan_policy;
	pParam->isCoalesingInIBSSAllowed = cfg_params->isCoalesingInIBSSAllowed;
	pParam->allowDFSChannelRoam = cfg_params->allowDFSChannelRoam;
	pParam->nInitialDwellTime = cfg_params->nInitialDwellTime;
	pParam->initial_scan_no_dfs_chnl = cfg_params->initial_scan_no_dfs_chnl;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	pParam->isRoamOffloadEnabled = cfg_params->isRoamOffloadEnabled;
	pParam->roam_triggers = cfg_params->roam_triggers;
	pParam->enable_disconnect_roam_offload =
		cfg_params->enable_disconnect_roam_offload;
	pParam->enable_idle_roam = cfg_params->enable_idle_roam;
	pParam->idle_roam_rssi_delta = cfg_params->idle_roam_rssi_delta;
	pParam->idle_roam_inactive_time = cfg_params->idle_roam_inactive_time;
	pParam->idle_data_packet_count = cfg_params->idle_data_packet_count;
	pParam->idle_roam_band = cfg_params->idle_roam_band;
	pParam->idle_roam_min_rssi = cfg_params->idle_roam_min_rssi;
#endif
	pParam->enable_dot11p = pMac->enable_dot11p;
	csr_set_channels(pMac, pParam);
	pParam->obssEnabled = cfg_params->obssEnabled;
	pParam->vendor_vht_sap =
		pMac->roam.configParam.vendor_vht_sap;
	pParam->roam_dense_rssi_thresh_offset =
		cfg_params->roam_params.dense_rssi_thresh_offset;
	pParam->roam_dense_min_aps =
			cfg_params->roam_params.dense_min_aps_cnt;
	pParam->roam_dense_traffic_thresh =
			cfg_params->roam_params.traffic_threshold;

	pParam->roam_bg_scan_bad_rssi_thresh =
		cfg_params->roam_params.bg_scan_bad_rssi_thresh;
	pParam->roam_bg_scan_client_bitmap =
		cfg_params->roam_params.bg_scan_client_bitmap;
	pParam->roam_bad_rssi_thresh_offset_2g =
		cfg_params->roam_params.roam_bad_rssi_thresh_offset_2g;
	pParam->roam_data_rssi_threshold_triggers =
		cfg_params->roam_params.roam_data_rssi_threshold_triggers;
	pParam->roam_data_rssi_threshold =
		cfg_params->roam_params.roam_data_rssi_threshold;
	pParam->rx_data_inactivity_time =
		cfg_params->roam_params.rx_data_inactivity_time;

	pParam->enable_ftopen = cfg_params->enable_ftopen;
	pParam->scan_adaptive_dwell_mode =
			cfg_params->scan_adaptive_dwell_mode;
	pParam->scan_adaptive_dwell_mode_nc =
			cfg_params->scan_adaptive_dwell_mode_nc;
	pParam->roamscan_adaptive_dwell_mode =
			cfg_params->roamscan_adaptive_dwell_mode;

	pParam->per_roam_config.enable = cfg_params->per_roam_config.enable;
	pParam->per_roam_config.tx_high_rate_thresh =
			cfg_params->per_roam_config.tx_high_rate_thresh;
	pParam->per_roam_config.rx_high_rate_thresh =
			cfg_params->per_roam_config.rx_high_rate_thresh;
	pParam->per_roam_config.tx_low_rate_thresh =
			cfg_params->per_roam_config.tx_low_rate_thresh;
	pParam->per_roam_config.rx_low_rate_thresh =
			cfg_params->per_roam_config.rx_low_rate_thresh;
	pParam->per_roam_config.tx_rate_thresh_percnt =
			cfg_params->per_roam_config.tx_rate_thresh_percnt;
	pParam->per_roam_config.rx_rate_thresh_percnt =
			cfg_params->per_roam_config.rx_rate_thresh_percnt;
	pParam->per_roam_config.per_rest_time =
			cfg_params->per_roam_config.per_rest_time;
	pParam->per_roam_config.tx_per_mon_time =
			cfg_params->per_roam_config.tx_per_mon_time;
	pParam->per_roam_config.rx_per_mon_time =
			cfg_params->per_roam_config.rx_per_mon_time;
	pParam->per_roam_config.min_candidate_rssi =
			cfg_params->per_roam_config.min_candidate_rssi;

	pParam->conc_custom_rule1 = cfg_params->conc_custom_rule1;
	pParam->conc_custom_rule2 = cfg_params->conc_custom_rule2;
	pParam->is_sta_connection_in_5gz_enabled =
		cfg_params->is_sta_connection_in_5gz_enabled;
	pParam->send_deauth_before_con =
		cfg_params->send_deauth_before_con;
	pParam->max_scan_count = pMac->scan.max_scan_count;
	pParam->first_scan_bucket_threshold =
		pMac->first_scan_bucket_threshold;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	pParam->sap_channel_avoidance = pMac->sap.sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	pParam->acs_with_more_param = pMac->sap.acs_with_more_param;
	pParam->max_intf_count = pMac->sme.max_intf_count;
	pParam->enableSelfRecovery = pMac->sme.enableSelfRecovery;
	pParam->f_prefer_non_dfs_on_radar =
		pMac->f_prefer_non_dfs_on_radar;
	pParam->dual_mac_feature_disable =
		pMac->dual_mac_feature_disable;
	pParam->sta_sap_scc_on_dfs_chan =
		pMac->sta_sap_scc_on_dfs_chan;
	pParam->is_ps_enabled = pMac->sme.ps_global_info.ps_enabled;
	pParam->auto_bmps_timer_val =
		pMac->sme.ps_global_info.auto_bmps_timer_val;
	pParam->fEnableDebugLog = pMac->fEnableDebugLog;
	pParam->enable5gEBT = pMac->enable5gEBT;
	pParam->f_sta_miracast_mcc_rest_time_val =
		pMac->f_sta_miracast_mcc_rest_time_val;
	pParam->early_stop_scan_enable =
		pMac->roam.configParam.early_stop_scan_enable;
	pParam->early_stop_scan_min_threshold =
		pMac->roam.configParam.early_stop_scan_min_threshold;
	pParam->early_stop_scan_max_threshold =
		pMac->roam.configParam.early_stop_scan_max_threshold;
	pParam->obss_width_interval =
		pMac->roam.configParam.obss_width_interval;
	pParam->obss_active_dwelltime =
		pMac->roam.configParam.obss_active_dwelltime;
	pParam->obss_passive_dwelltime =
		pMac->roam.configParam.obss_passive_dwelltime;
	pParam->ignore_peer_ht_opmode =
		pMac->roam.configParam.ignore_peer_ht_opmode;
	pParam->enableHtSmps = pMac->roam.configParam.enableHtSmps;
	pParam->htSmps = pMac->roam.configParam.htSmps;
	pParam->send_smps_action = pMac->roam.configParam.send_smps_action;

	pParam->enable_edca_params =
		pMac->roam.configParam.enable_edca_params;
	pParam->edca_vo_cwmin = pMac->roam.configParam.edca_vo_cwmin;
	pParam->edca_vi_cwmin = pMac->roam.configParam.edca_vi_cwmin;
	pParam->edca_bk_cwmin = pMac->roam.configParam.edca_bk_cwmin;
	pParam->edca_be_cwmin = pMac->roam.configParam.edca_be_cwmin;

	pParam->edca_vo_cwmax = pMac->roam.configParam.edca_vo_cwmax;
	pParam->edca_vi_cwmax = pMac->roam.configParam.edca_vi_cwmax;
	pParam->edca_bk_cwmax = pMac->roam.configParam.edca_bk_cwmax;
	pParam->edca_be_cwmax = pMac->roam.configParam.edca_be_cwmax;

	pParam->edca_vo_aifs = pMac->roam.configParam.edca_vo_aifs;
	pParam->edca_vi_aifs = pMac->roam.configParam.edca_vi_aifs;
	pParam->edca_bk_aifs = pMac->roam.configParam.edca_bk_aifs;
	pParam->edca_be_aifs = pMac->roam.configParam.edca_be_aifs;
	pParam->enable_fatal_event =
		pMac->roam.configParam.enable_fatal_event;
	pParam->sta_roam_policy_params.dfs_mode =
		pMac->roam.configParam.sta_roam_policy.dfs_mode;
	pParam->sta_roam_policy_params.skip_unsafe_channels =
		pMac->roam.configParam.sta_roam_policy.skip_unsafe_channels;
	pParam->tx_aggregation_size =
		pMac->roam.configParam.tx_aggregation_size;
	pParam->tx_aggregation_size_be =
		pMac->roam.configParam.tx_aggregation_size_be;
	pParam->tx_aggregation_size_bk =
		pMac->roam.configParam.tx_aggregation_size_bk;
	pParam->tx_aggregation_size_vi =
		pMac->roam.configParam.tx_aggregation_size_vi;
	pParam->tx_aggregation_size_vo =
		pMac->roam.configParam.tx_aggregation_size_vo;
	pParam->rx_aggregation_size =
		pMac->roam.configParam.rx_aggregation_size;
	pParam->enable_bcast_probe_rsp =
		pMac->roam.configParam.enable_bcast_probe_rsp;
	pParam->is_fils_enabled =
		pMac->roam.configParam.is_fils_enabled;
	pParam->qcn_ie_support =
		pMac->roam.configParam.qcn_ie_support;
	pParam->fils_max_chan_guard_time =
		pMac->roam.configParam.fils_max_chan_guard_time;
	pParam->disallow_duration =
		pMac->roam.configParam.disallow_duration;
	pParam->rssi_channel_penalization =
		pMac->roam.configParam.rssi_channel_penalization;
	pParam->num_disallowed_aps =
		pMac->roam.configParam.num_disallowed_aps;
	pParam->oce_feature_bitmap =
		pMac->roam.configParam.oce_feature_bitmap;
	pParam->roam_force_rssi_trigger = cfg_params->roam_force_rssi_trigger;
	qdf_mem_copy(&pParam->csr_mawc_config,
		&pMac->roam.configParam.csr_mawc_config,
		sizeof(pParam->csr_mawc_config));

	qdf_mem_copy(&pParam->bss_score_params,
		     &pMac->roam.configParam.bss_score_params,
		     sizeof(struct sir_score_config));
	pParam->btm_offload_config = pMac->roam.configParam.btm_offload_config;
	pParam->pmkid_modes = pMac->roam.configParam.pmkid_modes;
	pParam->btm_solicited_timeout =
		pMac->roam.configParam.btm_solicited_timeout;
	pParam->btm_max_attempt_cnt =
		pMac->roam.configParam.btm_max_attempt_cnt;
	pParam->btm_sticky_time = pMac->roam.configParam.btm_sticky_time;
	pParam->btm_query_bitmask =
		pMac->roam.configParam.btm_query_bitmask;
	pParam->btm_trig_min_candidate_score =
		pMac->roam.configParam.btm_trig_min_candidate_score;

	pParam->mbo_thresholds.mbo_candidate_rssi_thres =
		pMac->roam.configParam.mbo_thresholds.
		mbo_candidate_rssi_thres;
	pParam->mbo_thresholds.mbo_current_rssi_thres =
		pMac->roam.configParam.mbo_thresholds.
		mbo_current_rssi_thres;
	pParam->mbo_thresholds.mbo_current_rssi_mcc_thres =
		pMac->roam.configParam.mbo_thresholds.
		mbo_current_rssi_mcc_thres;
	pParam->mbo_thresholds.mbo_candidate_rssi_btc_thres =
		pMac->roam.configParam.mbo_thresholds.
		mbo_candidate_rssi_btc_thres;

	pParam->btm_validity_timer =
			pMac->roam.configParam.btm_validity_timer;
	pParam->btm_disassoc_timer_threshold =
			pMac->roam.configParam.btm_disassoc_timer_threshold;
	pParam->enable_bss_load_roam_trigger =
			pMac->roam.configParam.enable_bss_load_roam_trigger;
	pParam->bss_load_threshold =
			pMac->roam.configParam.bss_load_threshold;
	pParam->bss_load_sample_time =
			pMac->roam.configParam.bss_load_sample_time;
	pParam->bss_load_trigger_rssi_threshold_5ghz =
		pMac->roam.configParam.bss_load_trigger_rssi_threshold_5ghz;
	pParam->bss_load_trigger_rssi_threshold_24ghz =
		pMac->roam.configParam.bss_load_trigger_rssi_threshold_24ghz;

	pParam->roam_scan_inactivity_time =
		pMac->roam.configParam.roam_scan_inactivity_time;
	pParam->roam_inactive_data_packet_count =
		pMac->roam.configParam.roam_inactive_data_packet_count;
	pParam->roam_scan_period_after_inactivity =
		pMac->roam.configParam.roam_scan_period_after_inactivity;
	pParam->disconnect_roam_min_rssi =
		pMac->roam.configParam.disconnect_roam_min_rssi;
	pParam->bmiss_roam_min_rssi =
		pMac->roam.configParam.bmiss_roam_min_rssi;
	pParam->btm_roam_score_delta =
		pMac->roam.configParam.btm_roam_score_delta;
	pParam->idle_roam_score_delta =
		pMac->roam.configParam.idle_roam_score_delta;
	pParam->disable_4way_hs_offload =
		pMac->roam.configParam.disable_4way_hs_offload;

	csr_get_adaptive_11r_config(pMac, pParam);
	csr_get_sae_single_pmk_config(pMac, pParam);
	csr_get_he_config_param(pParam, pMac);

	csr_get_11k_offload_config_param(&pMac->roam.configParam, pParam);

	pParam->wlm_latency_enable = pMac->roam.configParam.wlm_latency_enable;
	pParam->wlm_latency_level = pMac->roam.configParam.wlm_latency_level;
	for (i = 0; i < CSR_NUM_WLM_LATENCY_LEVEL; i++) {
		pParam->wlm_latency_flags[i] =
			pMac->roam.configParam.wlm_latency_flags[i];
	}

	pParam->enable_pending_list_req =
				pMac->roam.configParam.enable_pending_list_req;
	pParam->sta_disable_roam = pMac->roam.configParam.sta_disable_roam;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_set_phy_mode(tHalHandle hHal, uint32_t phyMode,
			    enum band_info eBand, bool *pfRestartNeeded)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	bool fRestartNeeded = false;
	eCsrPhyMode newPhyMode = eCSR_DOT11_MODE_AUTO;

	if (BAND_2G == eBand) {
		if (CSR_IS_RADIO_A_ONLY(pMac))
			goto end;
		if (eCSR_DOT11_MODE_11a & phyMode)
			goto end;
	}
	if (BAND_5G == eBand) {
		if (CSR_IS_RADIO_BG_ONLY(pMac))
			goto end;
		if ((eCSR_DOT11_MODE_11b & phyMode)
			|| (eCSR_DOT11_MODE_11b_ONLY & phyMode)
			|| (eCSR_DOT11_MODE_11g & phyMode)
			|| (eCSR_DOT11_MODE_11g_ONLY & phyMode))
			goto end;
	}
	if (eCSR_DOT11_MODE_AUTO & phyMode)
		newPhyMode = eCSR_DOT11_MODE_AUTO;
	else {
		/* Check for dual band and higher capability first */
		if (eCSR_DOT11_MODE_11n_ONLY & phyMode) {
			if (eCSR_DOT11_MODE_11n_ONLY != phyMode)
				goto end;
			newPhyMode = eCSR_DOT11_MODE_11n_ONLY;
		} else if (eCSR_DOT11_MODE_11g_ONLY & phyMode) {
			if (eCSR_DOT11_MODE_11g_ONLY != phyMode)
				goto end;
			if (BAND_5G == eBand)
				goto end;
			newPhyMode = eCSR_DOT11_MODE_11g_ONLY;
			eBand = BAND_2G;
		} else if (eCSR_DOT11_MODE_11b_ONLY & phyMode) {
			if (eCSR_DOT11_MODE_11b_ONLY != phyMode)
				goto end;
			if (BAND_5G == eBand)
				goto end;
			newPhyMode = eCSR_DOT11_MODE_11b_ONLY;
			eBand = BAND_2G;
		} else if (eCSR_DOT11_MODE_11n & phyMode) {
			newPhyMode = eCSR_DOT11_MODE_11n;
		} else if (eCSR_DOT11_MODE_abg & phyMode) {
			newPhyMode = eCSR_DOT11_MODE_abg;
		} else if (eCSR_DOT11_MODE_11a & phyMode) {
			if ((eCSR_DOT11_MODE_11g & phyMode)
				|| (eCSR_DOT11_MODE_11b & phyMode)) {
				if (BAND_ALL == eBand)
					newPhyMode = eCSR_DOT11_MODE_abg;
				else
					goto end;
			} else {
				newPhyMode = eCSR_DOT11_MODE_11a;
				eBand = BAND_5G;
			}
		} else if (eCSR_DOT11_MODE_11g & phyMode) {
			newPhyMode = eCSR_DOT11_MODE_11g;
			eBand = BAND_2G;
		} else if (eCSR_DOT11_MODE_11b & phyMode) {
			newPhyMode = eCSR_DOT11_MODE_11b;
			eBand = BAND_2G;
		} else {
			sme_err("can't recognize phymode 0x%08X", phyMode);
			newPhyMode = eCSR_DOT11_MODE_AUTO;
		}
	}
	/* Done validating */
	status = QDF_STATUS_SUCCESS;
	/* Now we need to check whether a restart is needed. */
	if (eBand != pMac->roam.configParam.eBand) {
		fRestartNeeded = true;
		goto end;
	}
	if (newPhyMode != pMac->roam.configParam.phyMode) {
		fRestartNeeded = true;
		goto end;
	}
end:
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMac->roam.configParam.eBand = eBand;
		pMac->roam.configParam.phyMode = newPhyMode;
		if (pfRestartNeeded)
			*pfRestartNeeded = fRestartNeeded;
	}
	return status;
}

/**
 * csr_prune_ch_list() - prunes the channel list to keep only a type of channels
 * @ch_lst:        existing channel list
 * @is_24_GHz:     indicates if 2.5 GHz or 5 GHz channels are required
 *
 * Return: void
 */
static void csr_prune_ch_list(struct csr_channel *ch_lst, bool is_24_GHz)
{
	uint8_t idx = 0, num_channels = 0;

	for ( ; idx < ch_lst->numChannels; idx++) {
		if (is_24_GHz) {
			if (WLAN_REG_IS_24GHZ_CH(ch_lst->channelList[idx])) {
				ch_lst->channelList[num_channels] =
					ch_lst->channelList[idx];
				num_channels++;
			}
		} else {
			if (WLAN_REG_IS_5GHZ_CH(ch_lst->channelList[idx])) {
				ch_lst->channelList[num_channels] =
					ch_lst->channelList[idx];
				num_channels++;
			}
		}
	}
	/*
	 * Cleanup the rest of channels. Note we only need to clean up the
	 * channels if we had to trim the list. Calling qdf_mem_zero()
	 * is going to throw asserts on the debug builds so let's be a bit
	 * smarter about that. Zero out the reset of the channels only if we
	 * need to. The amount of memory to clear is the number of channesl that
	 * we trimmed (ch_lst->numChannels - num_channels) times the size of a
	 * channel in the structure.
	 */
	if (ch_lst->numChannels > num_channels) {
		qdf_mem_zero(&ch_lst->channelList[num_channels],
			    sizeof(ch_lst->channelList[0]) *
			    (ch_lst->numChannels - num_channels));
	}
	ch_lst->numChannels = num_channels;
}

/**
 * csr_prune_channel_list_for_mode() - prunes the channel list
 * @mac_ctx:       global mac context
 * @ch_lst:        existing channel list
 *
 * Prunes the channel list according to band stored in mac_ctx
 *
 * Return: void
 */
void csr_prune_channel_list_for_mode(tpAniSirGlobal mac_ctx,
				     struct csr_channel *ch_lst)
{
	/* for dual band NICs, don't need to trim the channel list.... */
	if (CSR_IS_OPEARTING_DUAL_BAND(mac_ctx))
		return;
	/*
	 * 2.4 GHz band operation requires the channel list to be trimmed to
	 * the 2.4 GHz channels only
	 */
	if (CSR_IS_24_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, true);
	else if (CSR_IS_5G_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, false);
}

#define INFRA_AP_DEFAULT_CHANNEL 6
QDF_STATUS csr_is_valid_channel(tpAniSirGlobal pMac, uint8_t chnNum)
{
	uint8_t index = 0;
	QDF_STATUS status = QDF_STATUS_E_NOSUPPORT;

	/* regulatory check */
	for (index = 0; index < pMac->scan.base_channels.numChannels;
	     index++) {
		if (pMac->scan.base_channels.channelList[index] == chnNum) {
			status = QDF_STATUS_SUCCESS;
			break;
		}
	}

	if (status == QDF_STATUS_SUCCESS) {
		/* dfs nol */
		for (index = 0;
		     index <
		     pMac->sap.SapDfsInfo.numCurrentRegDomainDfsChannels;
		     index++) {
			tSapDfsNolInfo *dfsChan = &pMac->sap.SapDfsInfo.
						sapDfsChannelNolList[index];
			if ((dfsChan->dfs_channel_number == chnNum)
			    && (dfsChan->radar_status_flag ==
				eSAP_DFS_CHANNEL_UNAVAILABLE)) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_ERROR,
					 FL("channel %d is in dfs nol"),
					  chnNum);
				status = QDF_STATUS_E_FAILURE;
				break;
			}
		}
	}

	if (QDF_STATUS_SUCCESS != status) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 FL("channel %d is not available"), chnNum);
	}

	return status;
}

QDF_STATUS csr_get_channel_and_power_list(tpAniSirGlobal pMac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t num20MHzChannelsFound = 0;
	QDF_STATUS qdf_status;
	uint8_t Index = 0;

	qdf_status = wlan_reg_get_channel_list_with_power(pMac->pdev,
				pMac->scan.defaultPowerTable,
				&num20MHzChannelsFound);

	if ((QDF_STATUS_SUCCESS != qdf_status) ||
	    (num20MHzChannelsFound == 0)) {
		sme_err("failed to get channels");
		status = QDF_STATUS_E_FAILURE;
	} else {
		if (num20MHzChannelsFound > WNI_CFG_VALID_CHANNEL_LIST_LEN)
			num20MHzChannelsFound = WNI_CFG_VALID_CHANNEL_LIST_LEN;
		pMac->scan.numChannelsDefault = num20MHzChannelsFound;
		/* Move the channel list to the global data */
		/* structure -- this will be used as the scan list */
		for (Index = 0; Index < num20MHzChannelsFound; Index++)
			pMac->scan.base_channels.channelList[Index] =
				pMac->scan.defaultPowerTable[Index].chan_num;
		pMac->scan.base_channels.numChannels =
			num20MHzChannelsFound;
	}
	return status;
}

QDF_STATUS csr_apply_channel_and_power_list(tpAniSirGlobal pMac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_prune_channel_list_for_mode(pMac, &pMac->scan.base_channels);
	csr_save_channel_power_for_band(pMac, false);
	csr_save_channel_power_for_band(pMac, true);
	csr_apply_channel_power_info_to_fw(pMac,
					   &pMac->scan.base_channels,
					   pMac->scan.countryCodeCurrent);

	csr_init_operating_classes((tHalHandle) pMac);
	return status;
}

static QDF_STATUS csr_init11d_info(tpAniSirGlobal pMac, tCsr11dinfo *ps11dinfo)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t index;
	uint32_t count = 0;
	tSirMacChanInfo *pChanInfo;
	tSirMacChanInfo *pChanInfoStart;
	bool applyConfig = true;

	if (!ps11dinfo)
		return status;

	if (ps11dinfo->Channels.numChannels
	    && (WNI_CFG_VALID_CHANNEL_LIST_LEN >=
		ps11dinfo->Channels.numChannels)) {
		pMac->scan.base_channels.numChannels =
			ps11dinfo->Channels.numChannels;
		qdf_mem_copy(pMac->scan.base_channels.channelList,
			     ps11dinfo->Channels.channelList,
			     ps11dinfo->Channels.numChannels);
	} else {
		/* No change */
		return QDF_STATUS_SUCCESS;
	}
	/* legacy maintenance */

	qdf_mem_copy(pMac->scan.countryCodeDefault, ps11dinfo->countryCode,
		     WNI_CFG_COUNTRY_CODE_LEN);

	/* Tush: at csropen get this initialized with default,
	 * during csr reset if this already set with some value
	 * no need initilaize with default again
	 */
	if (0 == pMac->scan.countryCodeCurrent[0]) {
		qdf_mem_copy(pMac->scan.countryCodeCurrent,
			     ps11dinfo->countryCode, WNI_CFG_COUNTRY_CODE_LEN);
	}
	/* need to add the max power channel list */
	pChanInfo =
		qdf_mem_malloc(sizeof(tSirMacChanInfo) *
			       WNI_CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo != NULL) {
		pChanInfoStart = pChanInfo;
		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->firstChanNum =
				ps11dinfo->ChnPower[index].firstChannel;
			pChanInfo->numChannels =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->maxTxPower =
				QDF_MIN(ps11dinfo->ChnPower[index].maxtxPower,
					pMac->roam.configParam.nTxPowerCap);
			pChanInfo++;
			count++;
		}
		if (count) {
			status = csr_save_to_channel_power2_g_5_g(pMac,
							 count *
							sizeof(tSirMacChanInfo),
							 pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}
	/* Only apply them to CFG when not in STOP state.
	 * Otherwise they will be applied later
	 */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		for (index = 0; index < CSR_ROAM_SESSION_MAX; index++) {
			if ((CSR_IS_SESSION_VALID(pMac, index))
			    && CSR_IS_ROAM_STOP(pMac, index)) {
				applyConfig = false;
			}
		}

		if (true == applyConfig) {
			/* Apply the base channel list, power info,
			 * and set the Country code.
			 */
			csr_apply_channel_power_info_to_fw(pMac,
							   &pMac->scan.
							   base_channels,
							   pMac->scan.
							   countryCodeCurrent);
		}
	}
	return status;
}

/* Initialize the Channel + Power List in the local cache and in the CFG */
QDF_STATUS csr_init_channel_power_list(tpAniSirGlobal pMac,
					tCsr11dinfo *ps11dinfo)
{
	uint8_t index;
	uint32_t count = 0;
	tSirMacChanInfo *pChanInfo;
	tSirMacChanInfo *pChanInfoStart;

	if (!ps11dinfo || !pMac)
		return QDF_STATUS_E_FAILURE;

	pChanInfo =
		qdf_mem_malloc(sizeof(tSirMacChanInfo) *
			       WNI_CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo != NULL) {
		pChanInfoStart = pChanInfo;

		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->firstChanNum =
				ps11dinfo->ChnPower[index].firstChannel;
			pChanInfo->numChannels =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->maxTxPower =
				QDF_MIN(ps11dinfo->ChnPower[index].maxtxPower,
					pMac->roam.configParam.nTxPowerCap);
			pChanInfo++;
			count++;
		}
		if (count) {
			csr_save_to_channel_power2_g_5_g(pMac,
							 count *
							sizeof(tSirMacChanInfo),
							 pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_roam_remove_duplicate_cmd_from_list()- Remove duplicate roam cmd from
 * list
 *
 * @mac_ctx: pointer to global mac
 * @session_id: session id for the cmd
 * @list: pending list from which cmd needs to be removed
 * @command: cmd to be removed, can be NULL
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the pending list.
 *
 * Return: void
 */
static void csr_roam_remove_duplicate_pending_cmd_from_list(
			tpAniSirGlobal mac_ctx,
			uint32_t session_id,
			tSmeCmd *command, enum csr_roam_reason roam_reason)
{
	tListElem *entry, *next_entry;
	tSmeCmd *dup_cmd;
	tDblLinkList local_list;

	qdf_mem_zero(&local_list, sizeof(tDblLinkList));
	if (!QDF_IS_STATUS_SUCCESS(csr_ll_open(&local_list))) {
		sme_err("failed to open list");
		return;
	}
	csr_nonscan_pending_ll_lock(mac_ctx);
	entry = csr_nonscan_pending_ll_peek_head(mac_ctx, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csr_nonscan_pending_ll_next(mac_ctx, entry,
						LL_ACCESS_NOLOCK);
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/*
		 * If command is not NULL remove the similar duplicate cmd for
		 * same reason as command. If command is NULL then check if
		 * roam_reason is eCsrForcedDisassoc (disconnect) and remove
		 * all roam command for the sessionId, else if roam_reason is
		 * eCsrHddIssued (connect) remove all connect (non disconenct)
		 * commands.
		 */
		if ((command && (command->sessionId == dup_cmd->sessionId) &&
			((command->command == dup_cmd->command) &&
			/*
			 * This peermac check is required for Softap/GO
			 * scenarios. for STA scenario below OR check will
			 * suffice as command will always be NULL for
			 * STA scenarios
			 */
			(!qdf_mem_cmp(dup_cmd->u.roamCmd.peerMac,
				command->u.roamCmd.peerMac,
					sizeof(QDF_MAC_ADDR_SIZE))) &&
				((command->u.roamCmd.roamReason ==
					dup_cmd->u.roamCmd.roamReason) ||
				(eCsrForcedDisassoc ==
					command->u.roamCmd.roamReason) ||
				(eCsrHddIssued ==
					command->u.roamCmd.roamReason)))) ||
			/* OR if pCommand is NULL */
			((session_id == dup_cmd->sessionId) &&
			(eSmeCommandRoam == dup_cmd->command) &&
			((eCsrForcedDisassoc == roam_reason) ||
			(eCsrHddIssued == roam_reason &&
			!CSR_IS_DISCONNECT_COMMAND(dup_cmd))))) {
			sme_debug("RoamReason: %d",
					dup_cmd->u.roamCmd.roamReason);
			/* Insert to local_list and remove later */
			csr_ll_insert_tail(&local_list, entry,
					   LL_ACCESS_NOLOCK);
		}
		entry = next_entry;
	}
	csr_nonscan_pending_ll_unlock(mac_ctx);

	while ((entry = csr_ll_remove_head(&local_list, LL_ACCESS_NOLOCK))) {
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/* Tell caller that the command is cancelled */
		csr_roam_call_callback(mac_ctx, dup_cmd->sessionId, NULL,
				dup_cmd->u.roamCmd.roamId,
				eCSR_ROAM_CANCELLED, eCSR_ROAM_RESULT_NONE);
		csr_release_command(mac_ctx, dup_cmd);
	}
	csr_ll_close(&local_list);
}

/**
 * csr_roam_remove_duplicate_command()- Remove duplicate roam cmd
 * from pending lists.
 *
 * @mac_ctx: pointer to global mac
 * @session_id: session id for the cmd
 * @command: cmd to be removed, can be null
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the sme and roam pending list.
 *
 * Return: void
 */
void csr_roam_remove_duplicate_command(tpAniSirGlobal mac_ctx,
			uint32_t session_id, tSmeCmd *command,
			enum csr_roam_reason roam_reason)
{
	/* Always lock active list before locking pending lists */
	csr_nonscan_active_ll_lock(mac_ctx);
	csr_roam_remove_duplicate_pending_cmd_from_list(mac_ctx,
		session_id, command, roam_reason);
	csr_nonscan_active_ll_unlock(mac_ctx);
}

/**
 * csr_roam_populate_channels() - Helper function to populate channels
 * @beacon_ies: pointer to beacon ie
 * @roam_info: Roaming related information
 * @chan1: center freq 1
 * @chan2: center freq2
 *
 * This function will issue populate chan1 and chan2 based on beacon ie
 *
 * Return: none.
 */
static void csr_roam_populate_channels(tDot11fBeaconIEs *beacon_ies,
			struct csr_roam_info *roam_info,
			uint8_t *chan1, uint8_t *chan2)
{
	ePhyChanBondState phy_state;

	if (beacon_ies->VHTOperation.present) {
		*chan1 = beacon_ies->VHTOperation.chanCenterFreqSeg1;
		*chan2 = beacon_ies->VHTOperation.chanCenterFreqSeg2;
		roam_info->chan_info.info = MODE_11AC_VHT80;
	} else if (beacon_ies->HTInfo.present) {
		if (beacon_ies->HTInfo.recommendedTxWidthSet ==
			eHT_CHANNEL_WIDTH_40MHZ) {
			phy_state = beacon_ies->HTInfo.secondaryChannelOffset;
			if (phy_state == PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
				*chan1 = beacon_ies->HTInfo.primaryChannel +
						CSR_CB_CENTER_CHANNEL_OFFSET;
			else if (phy_state == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
				*chan1 = beacon_ies->HTInfo.primaryChannel -
						CSR_CB_CENTER_CHANNEL_OFFSET;
			else
				*chan1 = beacon_ies->HTInfo.primaryChannel;

			roam_info->chan_info.info = MODE_11NA_HT40;
		} else {
			*chan1 = beacon_ies->HTInfo.primaryChannel;
			roam_info->chan_info.info = MODE_11NA_HT20;
		}
		*chan2 = 0;
	} else {
		*chan1 = 0;
		*chan2 = 0;
		roam_info->chan_info.info = MODE_11A;
	}
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
#ifdef WLAN_DEBUG
static const char *csr_get_ch_width_str(uint8_t ch_width)
{
	switch (ch_width) {
	CASE_RETURN_STRING(BW_20MHZ);
	CASE_RETURN_STRING(BW_40MHZ);
	CASE_RETURN_STRING(BW_80MHZ);
	CASE_RETURN_STRING(BW_160MHZ);
	CASE_RETURN_STRING(BW_80P80MHZ);
	CASE_RETURN_STRING(BW_5MHZ);
	CASE_RETURN_STRING(BW_10MHZ);
	default:
		return "Unknown";
	}
}

static const char *csr_get_dot11_mode_str(enum csr_cfgdot11mode dot11mode)
{
	switch (dot11mode) {
	CASE_RETURN_STRING(DOT11_MODE_AUTO);
	CASE_RETURN_STRING(DOT11_MODE_ABG);
	CASE_RETURN_STRING(DOT11_MODE_11A);
	CASE_RETURN_STRING(DOT11_MODE_11B);
	CASE_RETURN_STRING(DOT11_MODE_11G);
	CASE_RETURN_STRING(DOT11_MODE_11N);
	CASE_RETURN_STRING(DOT11_MODE_11AC);
	CASE_RETURN_STRING(DOT11_MODE_11G_ONLY);
	CASE_RETURN_STRING(DOT11_MODE_11N_ONLY);
	CASE_RETURN_STRING(DOT11_MODE_11AC_ONLY);
	CASE_RETURN_STRING(DOT11_MODE_11AX);
	CASE_RETURN_STRING(DOT11_MODE_11AX_ONLY);
	default:
		return "Unknown";
	}
}

static const char *csr_get_auth_type_str(uint8_t auth_type)
{
	switch (auth_type) {
	CASE_RETURN_STRING(AUTH_OPEN);
	CASE_RETURN_STRING(AUTH_SHARED);
	CASE_RETURN_STRING(AUTH_WPA_EAP);
	CASE_RETURN_STRING(AUTH_WPA_PSK);
	CASE_RETURN_STRING(AUTH_WPA2_EAP);
	CASE_RETURN_STRING(AUTH_WPA2_PSK);
	CASE_RETURN_STRING(AUTH_WAPI_CERT);
	CASE_RETURN_STRING(AUTH_WAPI_PSK);
	default:
		return "Unknown";
	}
}

static const char *csr_get_encr_type_str(uint8_t encr_type)
{
	switch (encr_type) {
	CASE_RETURN_STRING(ENC_MODE_OPEN);
	CASE_RETURN_STRING(ENC_MODE_WEP40);
	CASE_RETURN_STRING(ENC_MODE_WEP104);
	CASE_RETURN_STRING(ENC_MODE_TKIP);
	CASE_RETURN_STRING(ENC_MODE_AES);
	CASE_RETURN_STRING(ENC_MODE_AES_GCMP);
	CASE_RETURN_STRING(ENC_MODE_AES_GCMP_256);
	CASE_RETURN_STRING(ENC_MODE_SMS4);
	default:
		return "Unknown";
	}
}
#endif

static void csr_dump_connection_stats(tpAniSirGlobal mac_ctx,
		struct csr_roam_session *session,
		struct csr_roam_info *roam_info,
		eRoamCmdStatus u1, eCsrRoamResult u2)
{
	struct tagCsrRoamConnectedProfile *conn_profile;
	struct csr_roam_profile *profile;
	WLAN_HOST_DIAG_EVENT_DEF(conn_stats,
				 struct host_event_wlan_connection_stats);

	if (!session || !session->pCurRoamProfile || !roam_info)
		return;

	conn_profile = roam_info->u.pConnectedProfile;
	if (!conn_profile)
		return;
	profile = session->pCurRoamProfile;
	qdf_mem_zero(&conn_stats,
		    sizeof(struct host_event_wlan_connection_stats));
	qdf_mem_copy(conn_stats.bssid, conn_profile->bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	conn_stats.ssid_len = conn_profile->SSID.length;
	if (conn_stats.ssid_len > SIR_MAC_MAX_SSID_LENGTH)
		conn_stats.ssid_len = SIR_MAC_MAX_SSID_LENGTH;
	qdf_mem_copy(conn_stats.ssid, conn_profile->SSID.ssId,
		     conn_stats.ssid_len);
	sme_get_rssi_snr_by_bssid(MAC_HANDLE(mac_ctx),
				  session->pCurRoamProfile,
				  &conn_stats.bssid[0],
				  &conn_stats.rssi, NULL);
	conn_stats.est_link_speed = 0;
	conn_stats.chnl_bw =
		diag_ch_width_from_csr_type(conn_profile->vht_channel_width);
	conn_stats.dot11mode =
		diag_dot11_mode_from_csr_type(conn_profile->dot11Mode);
	conn_stats.bss_type =
	     diag_persona_from_csr_type(session->pCurRoamProfile->csrPersona);
	conn_stats.operating_channel = conn_profile->operationChannel;
	conn_stats.qos_capability = conn_profile->qosConnection;
	conn_stats.auth_type =
	     diag_auth_type_from_csr_type(conn_profile->AuthType);
	conn_stats.encryption_type =
	     diag_enc_type_from_csr_type(conn_profile->EncryptionType);
	conn_stats.result_code = (u2 == eCSR_ROAM_RESULT_ASSOCIATED) ? 1 : 0;
	conn_stats.reason_code = 0;
	sme_nofl_debug("+---------CONNECTION INFO START------------+");
	sme_nofl_debug("VDEV-ID: %d self_mac:%pM", session->sessionId,
		       session->selfMacAddr.bytes);
	sme_nofl_debug("ssid: %.*s bssid: %pM RSSI: %d dBm",
		       conn_stats.ssid_len, conn_stats.ssid,
		       conn_stats.bssid, conn_stats.rssi);
	sme_nofl_debug("Channel : %d channel_bw: %s dot11Mode: %s",
		       conn_stats.operating_channel,
		       csr_get_ch_width_str(conn_stats.chnl_bw),
		       csr_get_dot11_mode_str(conn_stats.dot11mode));
	sme_nofl_debug("AKM: %s Encry-type: %s",
		       csr_get_auth_type_str(conn_profile->AuthType),
		       csr_get_encr_type_str(conn_stats.encryption_type));
	sme_nofl_debug("Qos enable: %d | Associated: %s",
		       conn_stats.qos_capability,
		       (conn_stats.result_code ? "yes" : "no"));
	sme_nofl_debug("+---------CONNECTION INFO END------------+");

	WLAN_HOST_DIAG_EVENT_REPORT(&conn_stats, EVENT_WLAN_CONN_STATS_V2);
}
#else
static void csr_dump_connection_stats(tpAniSirGlobal mac_ctx,
		struct csr_roam_session *session,
		struct csr_roam_info *roam_info,
		eRoamCmdStatus u1, eCsrRoamResult u2)
{}

#endif

QDF_STATUS csr_roam_call_callback(tpAniSirGlobal pMac, uint32_t sessionId,
				  struct csr_roam_info *roam_info,
				  uint32_t roamId,
				  eRoamCmdStatus u1, eCsrRoamResult u2)
{
	QDF_STATUS ret, status = QDF_STATUS_SUCCESS;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	uint32_t rssi = 0;

	WLAN_HOST_DIAG_EVENT_DEF(connectionStatus,
			host_event_wlan_status_payload_type);
#endif
	struct csr_roam_session *pSession;
	tDot11fBeaconIEs *beacon_ies = NULL;
	uint8_t chan1, chan2;

	if (!CSR_IS_SESSION_VALID(pMac, sessionId)) {
		sme_err("Session ID: %d is not valid", sessionId);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
	pSession = CSR_GET_SESSION(pMac, sessionId);

	if (false == pSession->sessionActive) {
		sme_debug("Session is not Active");
		return QDF_STATUS_E_FAILURE;
	}

	if (eCSR_ROAM_ASSOCIATION_COMPLETION == u1 &&
			eCSR_ROAM_RESULT_ASSOCIATED == u2 && roam_info) {
		sme_debug("Assoc complete result: %d status: %d reason: %d",
			u2, roam_info->statusCode, roam_info->reasonCode);
		beacon_ies = qdf_mem_malloc(sizeof(tDot11fBeaconIEs));
		if ((NULL != beacon_ies) && (NULL != roam_info->pBssDesc)) {
			status = csr_parse_bss_description_ies(
					pMac, roam_info->pBssDesc,
					beacon_ies);
			csr_roam_populate_channels(beacon_ies, roam_info,
					&chan1, &chan2);
			if (0 != chan1)
				roam_info->chan_info.band_center_freq1 =
					cds_chan_to_freq(chan1);
			else
				roam_info->chan_info.band_center_freq1 = 0;
			if (0 != chan2)
				roam_info->chan_info.band_center_freq2 =
					cds_chan_to_freq(chan2);
			else
				roam_info->chan_info.band_center_freq2 = 0;
		} else {
			roam_info->chan_info.band_center_freq1 = 0;
			roam_info->chan_info.band_center_freq2 = 0;
			roam_info->chan_info.info = 0;
		}
		roam_info->chan_info.chan_id =
			roam_info->u.pConnectedProfile->operationChannel;
		roam_info->chan_info.mhz =
			cds_chan_to_freq(roam_info->chan_info.chan_id);
		roam_info->chan_info.reg_info_1 =
			(csr_get_cfg_max_tx_power(pMac,
				roam_info->chan_info.chan_id) << 16);
		roam_info->chan_info.reg_info_2 =
			(csr_get_cfg_max_tx_power(pMac,
				roam_info->chan_info.chan_id) << 8);
		qdf_mem_free(beacon_ies);
	} else if ((u1 == eCSR_ROAM_FT_REASSOC_FAILED)
			&& (pSession->bRefAssocStartCnt)) {
		/*
		 * Decrement bRefAssocStartCnt for FT reassoc failure.
		 * Reason: For FT reassoc failures, we first call
		 * csr_roam_call_callback before notifying a failed roam
		 * completion through csr_roam_complete. The latter in
		 * turn calls csr_roam_process_results which tries to
		 * once again call csr_roam_call_callback if bRefAssocStartCnt
		 * is non-zero. Since this is redundant for FT reassoc
		 * failure, decrement bRefAssocStartCnt.
		 */
		pSession->bRefAssocStartCnt--;
	} else if (roam_info && (u1 == eCSR_ROAM_SET_CHANNEL_RSP)
		   && (u2 == eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS)) {
		pSession->connectedProfile.operationChannel =
			roam_info->channelChangeRespEvent->newChannelNumber;
	} else if (u1 == eCSR_ROAM_SESSION_OPENED) {
		ret = (u2 == eCSR_ROAM_RESULT_SUCCESS) ?
		      QDF_STATUS_SUCCESS : QDF_STATUS_E_FAILURE;

		if (pSession->session_open_cb)
			pSession->session_open_cb(sessionId, ret);
		else
			sme_err("session_open_cb is not registered");
	}
	if (eCSR_ROAM_ASSOCIATION_COMPLETION == u1)
		csr_dump_connection_stats(pMac, pSession, roam_info, u1, u2);

	if (NULL != pSession->callback) {
		if (roam_info)
			roam_info->sessionId = (uint8_t) sessionId;
		status = pSession->callback(pSession->pContext, roam_info,
					    roamId, u1, u2);
	}
	/*
	 * EVENT_WLAN_STATUS_V2: eCSR_ROAM_ASSOCIATION_COMPLETION,
	 *                    eCSR_ROAM_LOSTLINK,
	 *                    eCSR_ROAM_DISASSOCIATED,
	 */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	qdf_mem_zero(&connectionStatus,
			sizeof(host_event_wlan_status_payload_type));

	if ((eCSR_ROAM_ASSOCIATION_COMPLETION == u1)
			&& (eCSR_ROAM_RESULT_ASSOCIATED == u2) && roam_info) {
		connectionStatus.eventId = eCSR_WLAN_STATUS_CONNECT;
		connectionStatus.bssType =
			roam_info->u.pConnectedProfile->BSSType;

		if (NULL != roam_info->pBssDesc) {
			connectionStatus.rssi =
				roam_info->pBssDesc->rssi * (-1);
			connectionStatus.channel =
				roam_info->pBssDesc->channelId;
		}
		if (cfg_set_int(pMac, WNI_CFG_CURRENT_RSSI,
				connectionStatus.rssi) == QDF_STATUS_E_FAILURE)
			sme_err("Can't pass WNI_CFG_CURRENT_RSSI to cfg");

		connectionStatus.qosCapability =
			roam_info->u.pConnectedProfile->qosConnection;
		connectionStatus.authType =
			(uint8_t) diag_auth_type_from_csr_type(
				roam_info->u.pConnectedProfile->AuthType);
		connectionStatus.encryptionType =
			(uint8_t) diag_enc_type_from_csr_type(
				roam_info->u.pConnectedProfile->EncryptionType);
		qdf_mem_copy(connectionStatus.ssid,
				roam_info->u.pConnectedProfile->SSID.ssId,
				roam_info->u.pConnectedProfile->SSID.length);

		connectionStatus.reason = eCSR_REASON_UNSPECIFIED;
		qdf_mem_copy(&pMac->sme.eventPayload, &connectionStatus,
				sizeof(host_event_wlan_status_payload_type));
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if ((eCSR_ROAM_MIC_ERROR_IND == u1)
			|| (eCSR_ROAM_RESULT_MIC_FAILURE == u2)) {
		qdf_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		if (QDF_IS_STATUS_SUCCESS(wlan_cfg_get_int(pMac,
				WNI_CFG_CURRENT_RSSI, &rssi)))
			connectionStatus.rssi = rssi;

		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_MIC_ERROR;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_FORCED == u2) {
		qdf_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		if (QDF_IS_STATUS_SUCCESS(wlan_cfg_get_int(pMac,
				WNI_CFG_CURRENT_RSSI, &rssi)))
			connectionStatus.rssi = rssi;

		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_USER_REQUESTED;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_DISASSOC_IND == u2) {
		qdf_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		if (QDF_IS_STATUS_SUCCESS(wlan_cfg_get_int(pMac,
				WNI_CFG_CURRENT_RSSI, &rssi)))
			connectionStatus.rssi = rssi;

		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_DISASSOC;
		if (roam_info)
			connectionStatus.reasonDisconnect =
				roam_info->reasonCode;

		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_DEAUTH_IND == u2) {
		qdf_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		if (QDF_IS_STATUS_SUCCESS(wlan_cfg_get_int(pMac,
				WNI_CFG_CURRENT_RSSI, &rssi)))
			connectionStatus.rssi = rssi;

		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_DEAUTH;
		if (roam_info)
			connectionStatus.reasonDisconnect =
				roam_info->reasonCode;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

	return status;
}

/* Returns whether handoff is currently in progress or not */
static
bool csr_roam_is_handoff_in_progress(tpAniSirGlobal pMac, uint8_t sessionId)
{
	return csr_neighbor_roam_is_handoff_in_progress(pMac, sessionId);
}

static
QDF_STATUS csr_roam_issue_disassociate(tpAniSirGlobal pMac, uint32_t sessionId,
				       enum csr_roam_substate NewSubstate,
				       bool fMICFailure)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssId = QDF_MAC_ADDR_BCAST_INIT;
	uint16_t reasonCode;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (fMICFailure) {
		reasonCode = eSIR_MAC_MIC_FAILURE_REASON;
	} else if (NewSubstate == eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF) {
		reasonCode = eSIR_MAC_DISASSOC_DUE_TO_FTHANDOFF_REASON;
	} else if (eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT == NewSubstate) {
		reasonCode = eSIR_MAC_DISASSOC_LEAVING_BSS_REASON;
		NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"set to reason code eSIR_MAC_DISASSOC_LEAVING_BSS_REASON and set back NewSubstate");
	} else {
		reasonCode = eSIR_MAC_UNSPEC_FAILURE_REASON;
	}
	if ((csr_roam_is_handoff_in_progress(pMac, sessionId)) &&
	    (NewSubstate != eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF)) {
		tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
			&pMac->roam.neighborRoamInfo[sessionId];
		qdf_copy_macaddr(&bssId,
			      pNeighborRoamInfo->csrNeighborRoamProfile.BSSIDs.
			      bssid);
	} else if (pSession->pConnectBssDesc) {
		qdf_mem_copy(&bssId.bytes, pSession->pConnectBssDesc->bssId,
			     sizeof(struct qdf_mac_addr));
	}

	sme_debug("Disassociate Bssid " MAC_ADDRESS_STR " subState: %s reason: %d",
		  MAC_ADDR_ARRAY(bssId.bytes),
		  mac_trace_getcsr_roam_sub_state(NewSubstate), reasonCode);

	csr_roam_substate_change(pMac, NewSubstate, sessionId);

	status = csr_send_mb_disassoc_req_msg(pMac, sessionId, bssId.bytes,
						reasonCode);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		csr_roam_link_down(pMac, sessionId);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		/* no need to tell QoS that we are disassociating, it will be
		 * taken care off in assoc req for HO
		 */
		if (eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF != NewSubstate) {
			/* notify QoS module that disassoc happening */
			sme_qos_csr_event_ind(pMac, (uint8_t) sessionId,
					      SME_QOS_CSR_DISCONNECT_REQ, NULL);
		}
#endif
	} else {
		sme_warn("csr_send_mb_disassoc_req_msg failed status: %d",
			status);
	}

	return status;
}

/**
 * csr_roam_issue_disassociate_sta_cmd() - disassociate a associated station
 * @sessionId:     Session Id for Soft AP
 * @p_del_sta_params: Pointer to parameters of the station to disassoc
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_* on error
 */
QDF_STATUS csr_roam_issue_disassociate_sta_cmd(tpAniSirGlobal pMac,
					       uint32_t sessionId,
					       struct csr_del_sta_params
					       *p_del_sta_params)

{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		pCommand = csr_get_command_buffer(pMac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->sessionId = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDisassocSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
				p_del_sta_params->peerMacAddr.bytes,
				sizeof(pCommand->u.roamCmd.peerMac));
		pCommand->u.roamCmd.reason =
			(tSirMacReasonCodes)p_del_sta_params->reason_code;
		status = csr_queue_sme_command(pMac, pCommand, true);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

/**
 * csr_roam_issue_deauthSta() - delete a associated station
 * @sessionId:     Session Id for Soft AP
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_** on error
 */
QDF_STATUS csr_roam_issue_deauth_sta_cmd(tpAniSirGlobal pMac,
		uint32_t sessionId,
		struct csr_del_sta_params *pDelStaParams)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		pCommand = csr_get_command_buffer(pMac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->sessionId = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDeauthSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
			     pDelStaParams->peerMacAddr.bytes,
			     sizeof(tSirMacAddr));
		pCommand->u.roamCmd.reason =
			(tSirMacReasonCodes)pDelStaParams->reason_code;
		status = csr_queue_sme_command(pMac, pCommand, true);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

QDF_STATUS
csr_roam_get_associated_stas(tpAniSirGlobal pMac, uint32_t sessionId,
			     QDF_MODULE_ID modId, void *pUsrContext,
			     void *pfnSapEventCallback, uint8_t *pAssocStasBuf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssId = QDF_MAC_ADDR_BCAST_INIT;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("csr_roam_get_associated_stas:CSR Session not found");
		return status;
	}
	if (pSession->pConnectBssDesc) {
		qdf_mem_copy(bssId.bytes, pSession->pConnectBssDesc->bssId,
			     sizeof(struct qdf_mac_addr));
	} else {
		sme_err("csr_roam_get_associated_stas:Connected BSS Description in CSR Session not found");
		return status;
	}
	sme_debug("CSR getting associated stations for Bssid: " MAC_ADDRESS_STR,
		  MAC_ADDR_ARRAY(bssId.bytes));
	status =
		csr_send_mb_get_associated_stas_req_msg(pMac, sessionId, modId,
							bssId,
							pUsrContext,
							pfnSapEventCallback,
							pAssocStasBuf);
	return status;
}

static
QDF_STATUS csr_roam_issue_deauth(tpAniSirGlobal pMac, uint32_t sessionId,
				 enum csr_roam_substate NewSubstate)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssId = QDF_MAC_ADDR_BCAST_INIT;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pSession->pConnectBssDesc) {
		qdf_mem_copy(bssId.bytes, pSession->pConnectBssDesc->bssId,
			     sizeof(struct qdf_mac_addr));
	}
	sme_debug("Deauth to Bssid " MAC_ADDRESS_STR,
		  MAC_ADDR_ARRAY(bssId.bytes));
	csr_roam_substate_change(pMac, NewSubstate, sessionId);

	status =
		csr_send_mb_deauth_req_msg(pMac, sessionId, bssId.bytes,
					   eSIR_MAC_DEAUTH_LEAVING_BSS_REASON);
	if (QDF_IS_STATUS_SUCCESS(status))
		csr_roam_link_down(pMac, sessionId);
	else {
		sme_err("csr_send_mb_deauth_req_msg failed with status %d Session ID: %d"
			MAC_ADDRESS_STR, status, sessionId,
			MAC_ADDR_ARRAY(bssId.bytes));
	}

	return status;
}

QDF_STATUS csr_roam_save_connected_bss_desc(tpAniSirGlobal pMac,
						uint32_t sessionId,
						tSirBssDescription *pBssDesc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	uint32_t size;

	if (!pSession) {
		sme_err("  session %d not found ", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* If no BSS description was found in this connection
	 * (happens with start IBSS), then nix the BSS description
	 * that we keep around for the connected BSS) and get out.
	 */
	if (NULL == pBssDesc) {
		csr_free_connect_bss_desc(pMac, sessionId);
	} else {
		size = pBssDesc->length + sizeof(pBssDesc->length);
		if (NULL != pSession->pConnectBssDesc) {
			if (((pSession->pConnectBssDesc->length) +
			     sizeof(pSession->pConnectBssDesc->length)) <
			    size) {
				/* not enough room for the new BSS,
				 * pMac->roam.pConnectBssDesc is freed inside
				 */
				csr_free_connect_bss_desc(pMac, sessionId);
			}
		}
		if (NULL == pSession->pConnectBssDesc)
			pSession->pConnectBssDesc = qdf_mem_malloc(size);

		if (NULL == pSession->pConnectBssDesc)
			status = QDF_STATUS_E_NOMEM;
		else
			qdf_mem_copy(pSession->pConnectBssDesc, pBssDesc, size);
	}
	return status;
}

static
QDF_STATUS csr_roam_prepare_bss_config(tpAniSirGlobal pMac,
				       struct csr_roam_profile *pProfile,
				       tSirBssDescription *pBssDesc,
				       struct bss_config_param *pBssConfig,
				       tDot11fBeaconIEs *pIes)
{
	enum csr_cfgdot11mode cfgDot11Mode;

	QDF_ASSERT(pIes != NULL);
	if (pIes == NULL)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(&pBssConfig->BssCap, &pBssDesc->capabilityInfo,
		     sizeof(tSirMacCapabilityInfo));
	/* get qos */
	pBssConfig->qosType = csr_get_qos_from_bss_desc(pMac, pBssDesc, pIes);
	/* Take SSID always from profile */
	qdf_mem_copy(&pBssConfig->SSID.ssId,
		     pProfile->SSIDs.SSIDList->SSID.ssId,
		     pProfile->SSIDs.SSIDList->SSID.length);
	pBssConfig->SSID.length = pProfile->SSIDs.SSIDList->SSID.length;

	if (csr_is_nullssid(pBssConfig->SSID.ssId, pBssConfig->SSID.length)) {
		sme_warn("BSS desc SSID is a wild card");
		/* Return failed if profile doesn't have an SSID either. */
		if (pProfile->SSIDs.numOfSSIDs == 0) {
			sme_warn("BSS desc and profile doesn't have SSID");
			return QDF_STATUS_E_FAILURE;
		}
	}
	if (WLAN_REG_IS_5GHZ_CH(pBssDesc->channelId))
		pBssConfig->eBand = BAND_5G;
	else
		pBssConfig->eBand = BAND_2G;
		/* phymode */
	if (csr_is_phy_mode_match(pMac, pProfile->phyMode, pBssDesc,
				  pProfile, &cfgDot11Mode, pIes)) {
		pBssConfig->uCfgDot11Mode = cfgDot11Mode;
	} else {
		/*
		 * No matching phy mode found, force to 11b/g based on INI for
		 * 2.4Ghz and to 11a mode for 5Ghz
		 */
		sme_warn("Can not find match phy mode");
		if (BAND_2G == pBssConfig->eBand) {
			if (pMac->roam.configParam.phyMode &
			    (eCSR_DOT11_MODE_11b | eCSR_DOT11_MODE_11b_ONLY)) {
				pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11B;
			} else {
				pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11G;
			}
		} else {
			pBssConfig->uCfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
		}
	}

	sme_debug("phyMode=%d, uCfgDot11Mode=%d negotiatedAuthType %d",
		   pProfile->phyMode, pBssConfig->uCfgDot11Mode,
		   pProfile->negotiatedAuthType);

	/* Qos */
	if ((pBssConfig->uCfgDot11Mode != eCSR_CFG_DOT11_MODE_11N) &&
	    (pMac->roam.configParam.WMMSupportMode == eCsrRoamWmmNoQos)) {
		/*
		 * Joining BSS is not 11n capable and WMM is disabled on client.
		 * Disable QoS and WMM
		 */
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
	}

	if (((pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N)
	    || (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC))
		&& ((pBssConfig->qosType != eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP)
		    || (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_HCF)
		    || (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_eDCF))) {
		/* Joining BSS is 11n capable and WMM is disabled on AP. */
		/* Assume all HT AP's are QOS AP's and enable WMM */
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
	}
	/* auth type */
	switch (pProfile->negotiatedAuthType) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pBssConfig->authType = eSIR_OPEN_SYSTEM;
		break;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		pBssConfig->authType = eSIR_SHARED_KEY;
		break;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pBssConfig->authType = eSIR_AUTO_SWITCH;
		break;
	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pBssConfig->authType = eSIR_AUTH_TYPE_SAE;
		break;
	}
	/* short slot time */
	if (eCSR_CFG_DOT11_MODE_11B != cfgDot11Mode)
		pBssConfig->uShortSlotTime =
			pMac->roam.configParam.shortSlotTime;
	else
		pBssConfig->uShortSlotTime = 0;

	if (pBssConfig->BssCap.ibss)
		/* We don't support 11h on IBSS */
		pBssConfig->f11hSupport = false;
	else
		pBssConfig->f11hSupport =
			pMac->roam.configParam.Is11hSupportEnabled;
	/* power constraint */
	pBssConfig->uPowerLimit =
		csr_get11h_power_constraint(pMac, &pIes->PowerConstraints);
	/* heartbeat */
	if (CSR_IS_11A_BSS(pBssDesc))
		pBssConfig->uHeartBeatThresh =
			pMac->roam.configParam.HeartbeatThresh50;
	else
		pBssConfig->uHeartBeatThresh =
			pMac->roam.configParam.HeartbeatThresh24;

	/*
	 * Join timeout: if we find a BeaconInterval in the BssDescription,
	 * then set the Join Timeout to be 10 x the BeaconInterval.
	 */
	if (pBssDesc->beaconInterval) {
		/* Make sure it is bigger than the minimal */
		pBssConfig->uJoinTimeOut =
			QDF_MAX(10 * pBssDesc->beaconInterval,
				CSR_JOIN_FAILURE_TIMEOUT_MIN);
		if (pBssConfig->uJoinTimeOut > CSR_JOIN_FAILURE_TIMEOUT_DEFAULT)
			pBssConfig->uJoinTimeOut =
					CSR_JOIN_FAILURE_TIMEOUT_DEFAULT;
	} else {
		pBssConfig->uJoinTimeOut =
			CSR_JOIN_FAILURE_TIMEOUT_DEFAULT;
	}
	/* validate CB */
	if ((pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N_ONLY) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AX_ONLY))
		pBssConfig->cbMode = csr_get_cb_mode_from_ies(pMac,
				pBssDesc->channelId, pIes);
	else
		pBssConfig->cbMode = PHY_SINGLE_CHANNEL_CENTERED;

	if (WLAN_REG_IS_24GHZ_CH(pBssDesc->channelId) &&
	    pProfile->force_24ghz_in_ht20) {
		pBssConfig->cbMode = PHY_SINGLE_CHANNEL_CENTERED;
		sme_debug("force_24ghz_in_ht20 is set so set cbmode to 0");
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_roam_prepare_bss_config_from_profile(
	tpAniSirGlobal pMac, struct csr_roam_profile *pProfile,
					struct bss_config_param *pBssConfig,
					tSirBssDescription *pBssDesc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t operationChannel = 0;
	uint8_t qAPisEnabled = false;
	/* SSID */
	pBssConfig->SSID.length = 0;
	if (pProfile->SSIDs.numOfSSIDs) {
		/* only use the first one */
		qdf_mem_copy(&pBssConfig->SSID,
			     &pProfile->SSIDs.SSIDList[0].SSID,
			     sizeof(tSirMacSSid));
	} else {
		/* SSID must present */
		return QDF_STATUS_E_FAILURE;
	}
	/* Settomg up the capabilities */
	if (csr_is_bss_type_ibss(pProfile->BSSType))
		pBssConfig->BssCap.ibss = 1;
	else
		pBssConfig->BssCap.ess = 1;

	if (eCSR_ENCRYPT_TYPE_NONE !=
	    pProfile->EncryptionType.encryptionType[0])
		pBssConfig->BssCap.privacy = 1;

	pBssConfig->eBand = pMac->roam.configParam.eBand;
	/* phymode */
	if (pProfile->ChannelInfo.ChannelList)
		operationChannel = pProfile->ChannelInfo.ChannelList[0];
	pBssConfig->uCfgDot11Mode = csr_roam_get_phy_mode_band_for_bss(pMac,
						pProfile, operationChannel,
						   &pBssConfig->eBand);
	/* QOS */
	/* Is this correct to always set to this // *** */
	if (pBssConfig->BssCap.ess == 1) {
		/*For Softap case enable WMM */
		if (CSR_IS_INFRA_AP(pProfile)
		    && (eCsrRoamWmmNoQos !=
			pMac->roam.configParam.WMMSupportMode)) {
			qAPisEnabled = true;
		} else
		if (csr_roam_get_qos_info_from_bss(pMac, pBssDesc) ==
		    QDF_STATUS_SUCCESS) {
			qAPisEnabled = true;
		} else {
			qAPisEnabled = false;
		}
	} else {
		qAPisEnabled = true;
	}
	if ((eCsrRoamWmmNoQos != pMac->roam.configParam.WMMSupportMode &&
	     qAPisEnabled) ||
	    ((eCSR_CFG_DOT11_MODE_11N == pBssConfig->uCfgDot11Mode &&
	      qAPisEnabled))) {
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
	} else {
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
	}

	/* auth type */
	/* Take the preferred Auth type. */
	switch (pProfile->AuthType.authType[0]) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pBssConfig->authType = eSIR_OPEN_SYSTEM;
		break;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		pBssConfig->authType = eSIR_SHARED_KEY;
		break;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pBssConfig->authType = eSIR_AUTO_SWITCH;
		break;
	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pBssConfig->authType = eSIR_AUTH_TYPE_SAE;
		break;
	}
	/* short slot time */
	if (WNI_CFG_PHY_MODE_11B != pBssConfig->uCfgDot11Mode) {
		pBssConfig->uShortSlotTime =
			pMac->roam.configParam.shortSlotTime;
	} else {
		pBssConfig->uShortSlotTime = 0;
	}
	/* power constraint. We don't support 11h on IBSS */
	pBssConfig->f11hSupport = false;
	pBssConfig->uPowerLimit = 0;
	/* heartbeat */
	if (BAND_5G == pBssConfig->eBand) {
		pBssConfig->uHeartBeatThresh =
			pMac->roam.configParam.HeartbeatThresh50;
	} else {
		pBssConfig->uHeartBeatThresh =
			pMac->roam.configParam.HeartbeatThresh24;
	}
	/* Join timeout */
	pBssConfig->uJoinTimeOut = CSR_JOIN_FAILURE_TIMEOUT_DEFAULT;

	return status;
}

static QDF_STATUS csr_roam_get_qos_info_from_bss(tpAniSirGlobal pMac,
						 tSirBssDescription *pBssDesc)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tDot11fBeaconIEs *pIes = NULL;

	do {
		if (!QDF_IS_STATUS_SUCCESS(
			csr_get_parsed_bss_description_ies(
				pMac, pBssDesc, &pIes))) {
			/* err msg */
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "csr_roam_get_qos_info_from_bss() failed");
			break;
		}
		/* check if the AP is QAP & it supports APSD */
		if (CSR_IS_QOS_BSS(pIes))
			status = QDF_STATUS_SUCCESS;
	} while (0);

	if (NULL != pIes)
		qdf_mem_free(pIes);

	return status;
}

static void csr_reset_cfg_privacy(tpAniSirGlobal mac)
{
	uint8_t Key0[WNI_CFG_WEP_DEFAULT_KEY_1_LEN] = {0};
	uint8_t Key1[WNI_CFG_WEP_DEFAULT_KEY_2_LEN] = {0};
	uint8_t Key2[WNI_CFG_WEP_DEFAULT_KEY_3_LEN] = {0};
	uint8_t Key3[WNI_CFG_WEP_DEFAULT_KEY_4_LEN] = {0};

	cfg_set_int(mac, WNI_CFG_PRIVACY_ENABLED, 0);
	cfg_set_int(mac, WNI_CFG_RSN_ENABLED, 0);
	cfg_set_str(mac, WNI_CFG_WEP_DEFAULT_KEY_1, Key0,
			 WNI_CFG_WEP_DEFAULT_KEY_1_LEN);
	cfg_set_str(mac, WNI_CFG_WEP_DEFAULT_KEY_2, Key1,
			 WNI_CFG_WEP_DEFAULT_KEY_2_LEN);
	cfg_set_str(mac, WNI_CFG_WEP_DEFAULT_KEY_3, Key2,
			 WNI_CFG_WEP_DEFAULT_KEY_3_LEN);
	cfg_set_str(mac, WNI_CFG_WEP_DEFAULT_KEY_4, Key3,
			 WNI_CFG_WEP_DEFAULT_KEY_4_LEN);
	cfg_set_int(mac, WNI_CFG_WEP_DEFAULT_KEYID, 0);
}

void csr_set_cfg_privacy(tpAniSirGlobal pMac, struct csr_roam_profile *pProfile,
			 bool fPrivacy)
{
	/*
	 * the only difference between this function and
	 * the csr_set_cfg_privacyFromProfile() is the setting of the privacy
	 * CFG based on the advertised privacy setting from the AP for WPA
	 * associations. See note below in this function...
	 */
	uint32_t PrivacyEnabled = 0, RsnEnabled = 0, WepDefaultKeyId = 0;
	uint32_t WepKeyLength = WNI_CFG_WEP_KEY_LENGTH_5;
	uint32_t Key0Length = 0, Key1Length = 0, Key2Length = 0, Key3Length = 0;

	/* Reserve for the biggest key */
	uint8_t Key0[WNI_CFG_WEP_DEFAULT_KEY_1_LEN];
	uint8_t Key1[WNI_CFG_WEP_DEFAULT_KEY_2_LEN];
	uint8_t Key2[WNI_CFG_WEP_DEFAULT_KEY_3_LEN];
	uint8_t Key3[WNI_CFG_WEP_DEFAULT_KEY_4_LEN];

	switch (pProfile->negotiatedUCEncryptionType) {
	case eCSR_ENCRYPT_TYPE_NONE:
		/* for NO encryption, turn off Privacy and Rsn. */
		PrivacyEnabled = 0;
		RsnEnabled = 0;
		/* clear out the WEP keys that may be hanging around. */
		Key0Length = 0;
		Key1Length = 0;
		Key2Length = 0;
		Key3Length = 0;
		break;

	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP40:

		/* Privacy is ON.  NO RSN for Wep40 static key. */
		PrivacyEnabled = 1;
		RsnEnabled = 0;
		/* Set the Wep default key ID. */
		WepDefaultKeyId = pProfile->Keys.defaultIndex;
		/* Wep key size if 5 bytes (40 bits). */
		WepKeyLength = WNI_CFG_WEP_KEY_LENGTH_5;
		/*
		 * set encryption keys in the CFG database or
		 * clear those that are not present in this profile.
		 */
		if (pProfile->Keys.KeyLength[0]) {
			qdf_mem_copy(Key0,
				pProfile->Keys.KeyMaterial[0],
				WNI_CFG_WEP_KEY_LENGTH_5);
			Key0Length = WNI_CFG_WEP_KEY_LENGTH_5;
		} else {
			Key0Length = 0;
		}

		if (pProfile->Keys.KeyLength[1]) {
			qdf_mem_copy(Key1,
				pProfile->Keys.KeyMaterial[1],
				WNI_CFG_WEP_KEY_LENGTH_5);
			Key1Length = WNI_CFG_WEP_KEY_LENGTH_5;
		} else {
			Key1Length = 0;
		}

		if (pProfile->Keys.KeyLength[2]) {
			qdf_mem_copy(Key2,
				pProfile->Keys.KeyMaterial[2],
				WNI_CFG_WEP_KEY_LENGTH_5);
			Key2Length = WNI_CFG_WEP_KEY_LENGTH_5;
		} else {
			Key2Length = 0;
		}

		if (pProfile->Keys.KeyLength[3]) {
			qdf_mem_copy(Key3,
				pProfile->Keys.KeyMaterial[3],
				WNI_CFG_WEP_KEY_LENGTH_5);
			Key3Length = WNI_CFG_WEP_KEY_LENGTH_5;
		} else {
			Key3Length = 0;
		}
		break;

	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP104:

		/* Privacy is ON.  NO RSN for Wep40 static key. */
		PrivacyEnabled = 1;
		RsnEnabled = 0;
		/* Set the Wep default key ID. */
		WepDefaultKeyId = pProfile->Keys.defaultIndex;
		/* Wep key size if 13 bytes (104 bits). */
		WepKeyLength = WNI_CFG_WEP_KEY_LENGTH_13;
		/*
		 * set encryption keys in the CFG database or clear
		 * those that are not present in this profile.
		 */
		if (pProfile->Keys.KeyLength[0]) {
			qdf_mem_copy(Key0,
				pProfile->Keys.KeyMaterial[0],
				WNI_CFG_WEP_KEY_LENGTH_13);
			Key0Length = WNI_CFG_WEP_KEY_LENGTH_13;
		} else {
			Key0Length = 0;
		}

		if (pProfile->Keys.KeyLength[1]) {
			qdf_mem_copy(Key1,
				pProfile->Keys.KeyMaterial[1],
				WNI_CFG_WEP_KEY_LENGTH_13);
			Key1Length = WNI_CFG_WEP_KEY_LENGTH_13;
		} else {
			Key1Length = 0;
		}

		if (pProfile->Keys.KeyLength[2]) {
			qdf_mem_copy(Key2,
				pProfile->Keys.KeyMaterial[2],
				WNI_CFG_WEP_KEY_LENGTH_13);
			Key2Length = WNI_CFG_WEP_KEY_LENGTH_13;
		} else {
			Key2Length = 0;
		}

		if (pProfile->Keys.KeyLength[3]) {
			qdf_mem_copy(Key3,
				pProfile->Keys.KeyMaterial[3],
				WNI_CFG_WEP_KEY_LENGTH_13);
			Key3Length = WNI_CFG_WEP_KEY_LENGTH_13;
		} else {
			Key3Length = 0;
		}
		break;

	case eCSR_ENCRYPT_TYPE_TKIP:
	case eCSR_ENCRYPT_TYPE_AES:
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
#ifdef FEATURE_WLAN_WAPI
	case eCSR_ENCRYPT_TYPE_WPI:
#endif /* FEATURE_WLAN_WAPI */
		/*
		 * this is the only difference between this function and
		 * the csr_set_cfg_privacyFromProfile().
		 * (setting of the privacy CFG based on the advertised
		 *  privacy setting from AP for WPA/WAPI associations).
		 */
		PrivacyEnabled = (0 != fPrivacy);
		/* turn on RSN enabled for WPA associations */
		RsnEnabled = 1;
		/* clear static WEP keys that may be hanging around. */
		Key0Length = 0;
		Key1Length = 0;
		Key2Length = 0;
		Key3Length = 0;
		break;
	default:
		PrivacyEnabled = 0;
		RsnEnabled = 0;
		break;
	}

	cfg_set_int(pMac, WNI_CFG_PRIVACY_ENABLED, PrivacyEnabled);
	cfg_set_int(pMac, WNI_CFG_RSN_ENABLED, RsnEnabled);
	cfg_set_str(pMac, WNI_CFG_WEP_DEFAULT_KEY_1, Key0, Key0Length);
	cfg_set_str(pMac, WNI_CFG_WEP_DEFAULT_KEY_2, Key1, Key1Length);
	cfg_set_str(pMac, WNI_CFG_WEP_DEFAULT_KEY_3, Key2, Key2Length);
	cfg_set_str(pMac, WNI_CFG_WEP_DEFAULT_KEY_4, Key3, Key3Length);
	cfg_set_int(pMac, WNI_CFG_WEP_DEFAULT_KEYID, WepDefaultKeyId);
}

static void csr_set_cfg_ssid(tpAniSirGlobal pMac, tSirMacSSid *pSSID)
{
	uint32_t len = 0;

	if (pSSID->length <= WNI_CFG_SSID_LEN)
		len = pSSID->length;
	cfg_set_str(pMac, WNI_CFG_SSID, (uint8_t *) pSSID->ssId, len);
}

static QDF_STATUS csr_set_qos_to_cfg(tpAniSirGlobal pMac, uint32_t sessionId,
				     eCsrMediaAccessType qosType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t QoSEnabled;
	uint32_t WmeEnabled;
	/* set the CFG enable/disable variables based on the
	 * qosType being configured.
	 */
	switch (qosType) {
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_802dot1p:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_NoClassify:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_11e_eDCF:
		QoSEnabled = true;
		WmeEnabled = false;
		break;
	case eCSR_MEDIUM_ACCESS_11e_HCF:
		QoSEnabled = true;
		WmeEnabled = false;
		break;
	default:
	case eCSR_MEDIUM_ACCESS_DCF:
		QoSEnabled = false;
		WmeEnabled = false;
		break;
	}
	/* save the WMM setting for later use */
	pMac->roam.roamSession[sessionId].fWMMConnection = (bool) WmeEnabled;
	pMac->roam.roamSession[sessionId].fQOSConnection = (bool) QoSEnabled;
	return status;
}

static QDF_STATUS csr_get_rate_set(tpAniSirGlobal pMac,
				   struct csr_roam_profile *pProfile,
				   eCsrPhyMode phyMode,
				   tSirBssDescription *pBssDesc,
				   tDot11fBeaconIEs *pIes,
				   tSirMacRateSet *pOpRateSet,
				   tSirMacRateSet *pExRateSet)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int i;
	enum csr_cfgdot11mode cfgDot11Mode;
	uint8_t *pDstRate;
	uint16_t rateBitmap = 0;

	qdf_mem_zero(pOpRateSet, sizeof(tSirMacRateSet));
	qdf_mem_zero(pExRateSet, sizeof(tSirMacRateSet));
	QDF_ASSERT(pIes != NULL);

	if (NULL == pIes) {
		sme_err("failed to parse BssDesc");
		return status;
	}

	csr_is_phy_mode_match(pMac, phyMode, pBssDesc, pProfile,
			      &cfgDot11Mode, pIes);
	/*
	 * Originally, we thought that for 11a networks, the 11a rates
	 * are always in the Operational Rate set & for 11b and 11g
	 * networks, the 11b rates appear in the Operational Rate set.
	 * Consequently, in either case, we would blindly put the rates
	 * we support into our Operational Rate set.
	 * (including the basic rates, which we've already verified are
	 * supported earlier in the roaming decision).
	 * However, it turns out that this is not always the case.
	 * Some AP's (e.g. D-Link DI-784) ram 11g rates into the
	 * Operational Rate set too.  Now, we're a little more careful.
	 */
	pDstRate = pOpRateSet->rate;
	if (pIes->SuppRates.present) {
		for (i = 0; i < pIes->SuppRates.num_rates; i++) {
			if (csr_rates_is_dot11_rate_supported(pMac,
				pIes->SuppRates.rates[i]) &&
				!csr_check_rate_bitmap(
					pIes->SuppRates.rates[i],
					rateBitmap)) {
				csr_add_rate_bitmap(pIes->SuppRates.
						    rates[i], &rateBitmap);
				*pDstRate++ = pIes->SuppRates.rates[i];
				pOpRateSet->numRates++;
			}
		}
	}
	/*
	 * If there are Extended Rates in the beacon, we will reflect the
	 * extended rates that we support in our Extended Operational Rate
	 * set.
	 */
	if (pIes->ExtSuppRates.present) {
		pDstRate = pExRateSet->rate;
		for (i = 0; i < pIes->ExtSuppRates.num_rates; i++) {
			if (csr_rates_is_dot11_rate_supported(pMac,
				pIes->ExtSuppRates.rates[i]) &&
				!csr_check_rate_bitmap(
					pIes->ExtSuppRates.rates[i],
					rateBitmap)) {
				*pDstRate++ = pIes->ExtSuppRates.rates[i];
				pExRateSet->numRates++;
			}
		}
	}
	if (pOpRateSet->numRates > 0 || pExRateSet->numRates > 0)
		status = QDF_STATUS_SUCCESS;
	return status;
}

static void csr_set_cfg_rate_set(tpAniSirGlobal pMac, eCsrPhyMode phyMode,
				 struct csr_roam_profile *pProfile,
				 tSirBssDescription *pBssDesc,
				 tDot11fBeaconIEs *pIes)
{
	int i;
	uint8_t *pDstRate;
	enum csr_cfgdot11mode cfgDot11Mode;
	/* leave enough room for the max number of rates */
	uint8_t OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
	uint32_t OperationalRatesLength = 0;
	/* leave enough room for the max number of rates */
	uint8_t ExtendedOperationalRates
				[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
	uint32_t ExtendedOperationalRatesLength = 0;
	uint8_t MCSRateIdxSet[SIZE_OF_SUPPORTED_MCS_SET];
	uint32_t MCSRateLength = 0;

	QDF_ASSERT(pIes != NULL);
	if (NULL != pIes) {
		csr_is_phy_mode_match(pMac, phyMode, pBssDesc, pProfile,
				      &cfgDot11Mode, pIes);
		/* Originally, we thought that for 11a networks, the 11a rates
		 * are always in the Operational Rate set & for 11b and 11g
		 * networks, the 11b rates appear in the Operational Rate set.
		 * Consequently, in either case, we would blindly put the rates
		 * we support into our Operational Rate set (including the basic
		 * rates, which we have already verified are supported earlier
		 * in the roaming decision). However, it turns out that this is
		 * not always the case.  Some AP's (e.g. D-Link DI-784) ram 11g
		 * rates into the Operational Rate set, too.  Now, we're a
		 * little more careful:
		 */
		pDstRate = OperationalRates;
		if (pIes->SuppRates.present) {
			for (i = 0; i < pIes->SuppRates.num_rates; i++) {
				if (csr_rates_is_dot11_rate_supported
					    (pMac, pIes->SuppRates.rates[i])
				    && (OperationalRatesLength <
					CSR_DOT11_SUPPORTED_RATES_MAX)) {
					*pDstRate++ = pIes->SuppRates.rates[i];
					OperationalRatesLength++;
				}
			}
		}
		if (eCSR_CFG_DOT11_MODE_11G == cfgDot11Mode ||
		    eCSR_CFG_DOT11_MODE_11N == cfgDot11Mode ||
		    eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode) {
			/* If there are Extended Rates in the beacon, we will
			 * reflect those extended rates that we support in out
			 * Extended Operational Rate set:
			 */
			pDstRate = ExtendedOperationalRates;
			if (pIes->ExtSuppRates.present) {
				for (i = 0; i < pIes->ExtSuppRates.num_rates;
				     i++) {
					if (csr_rates_is_dot11_rate_supported
						    (pMac, pIes->ExtSuppRates.
							rates[i])
					    && (ExtendedOperationalRatesLength <
						CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX)) {
						*pDstRate++ =
							pIes->ExtSuppRates.
							rates[i];
						ExtendedOperationalRatesLength++;
					}
				}
			}
		}
		/* Enable proprietary MAC features if peer node is Airgo node
		 * and STA user wants to use them For ANI network companions,
		 * we need to populate the proprietary rate set with any
		 * proprietary rates we found in the beacon, only if user allows
		 * them.
		 */
		/* No proprietary modes... */
		/* Get MCS Rate */
		pDstRate = MCSRateIdxSet;
		if (pIes->HTCaps.present) {
			for (i = 0; i < VALID_MAX_MCS_INDEX; i++) {
				if ((unsigned int)pIes->HTCaps.
				    supportedMCSSet[0] & (1 << i)) {
					MCSRateLength++;
					*pDstRate++ = i;
				}
			}
		}
		/* Set the operational rate set CFG variables... */
		cfg_set_str(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
				OperationalRates, OperationalRatesLength);
		cfg_set_str(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
				ExtendedOperationalRates,
				ExtendedOperationalRatesLength);
		cfg_set_str(pMac, WNI_CFG_CURRENT_MCS_SET, MCSRateIdxSet,
				MCSRateLength);
	} /* Parsing BSSDesc */
	else
		sme_err("failed to parse BssDesc");
}

static void csr_set_cfg_rate_set_from_profile(tpAniSirGlobal pMac,
					      struct csr_roam_profile *pProfile)
{
	tSirMacRateSetIE DefaultSupportedRates11a = { SIR_MAC_RATESET_EID,
						      {8,
						       {SIR_MAC_RATE_6,
							SIR_MAC_RATE_9,
							SIR_MAC_RATE_12,
							SIR_MAC_RATE_18,
							SIR_MAC_RATE_24,
							SIR_MAC_RATE_36,
							SIR_MAC_RATE_48,
							SIR_MAC_RATE_54} } };
	tSirMacRateSetIE DefaultSupportedRates11b = { SIR_MAC_RATESET_EID,
						      {4,
						       {SIR_MAC_RATE_1,
							SIR_MAC_RATE_2,
							SIR_MAC_RATE_5_5,
							SIR_MAC_RATE_11} } };
	enum csr_cfgdot11mode cfgDot11Mode;
	enum band_info eBand;
	/* leave enough room for the max number of rates */
	uint8_t OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
	uint32_t OperationalRatesLength = 0;
	/* leave enough room for the max number of rates */
	uint8_t ExtendedOperationalRates
				[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
	uint32_t ExtendedOperationalRatesLength = 0;
	uint8_t operationChannel = 0;

	if (pProfile->ChannelInfo.ChannelList)
		operationChannel = pProfile->ChannelInfo.ChannelList[0];
	cfgDot11Mode = csr_roam_get_phy_mode_band_for_bss(pMac, pProfile,
							operationChannel,
							&eBand);
	/* For 11a networks, the 11a rates go into the Operational Rate set.
	 * For 11b and 11g networks, the 11b rates appear in the Operational
	 * Rate set. In either case, we can blindly put the rates we support
	 * into our Operational Rate set (including the basic rates, which we
	 * have already verified are supported earlier in the roaming decision).
	 */
	if (BAND_5G == eBand) {
		/* 11a rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11a.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11a.supportedRateSet.rate,
			     OperationalRatesLength);

		/* Nothing in the Extended rate set. */
		ExtendedOperationalRatesLength = 0;
	} else if (eCSR_CFG_DOT11_MODE_11B == cfgDot11Mode) {
		/* 11b rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11b.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11b.supportedRateSet.rate,
			     OperationalRatesLength);
		/* Nothing in the Extended rate set. */
		ExtendedOperationalRatesLength = 0;
	} else {
		/* 11G */

		/* 11b rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11b.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11b.supportedRateSet.rate,
			     OperationalRatesLength);

		/* 11a rates go in the Extended rate set. */
		ExtendedOperationalRatesLength =
			DefaultSupportedRates11a.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
		qdf_mem_copy(ExtendedOperationalRates,
			     DefaultSupportedRates11a.supportedRateSet.rate,
			     ExtendedOperationalRatesLength);
	}

	/* Set the operational rate set CFG variables... */
	cfg_set_str(pMac, WNI_CFG_OPERATIONAL_RATE_SET, OperationalRates,
			OperationalRatesLength);
	cfg_set_str(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
			ExtendedOperationalRates,
			ExtendedOperationalRatesLength);
}

void csr_roam_ccm_cfg_set_callback(tpAniSirGlobal pMac, int32_t result,
					uint8_t session_id)
{
	tListElem *pEntry =
		csr_nonscan_active_ll_peek_head(pMac, LL_ACCESS_LOCK);
	uint32_t sessionId;
	tSmeCmd *pCommand = NULL;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	struct csr_roam_session *pSession = NULL;
#endif
	if (NULL == pEntry) {
		sme_err("CFG_CNF with active list empty");
		return;
	}
	pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
	sessionId = pCommand->sessionId;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	pSession = &pMac->roam.roamSession[sessionId];
	if (pSession->roam_synch_in_progress) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "LFR3:csr_roam_cfg_set_callback");
	}
#endif

	if (CSR_IS_ROAM_JOINING(pMac, sessionId)
	    && CSR_IS_ROAM_SUBSTATE_CONFIG(pMac, sessionId)) {
		csr_roaming_state_config_cnf_processor(pMac, pCommand,
				(uint32_t) result, session_id);
	}
}

/* pIes may be NULL */
QDF_STATUS csr_roam_set_bss_config_cfg(tpAniSirGlobal pMac, uint32_t sessionId,
				       struct csr_roam_profile *pProfile,
				       tSirBssDescription *pBssDesc,
				       struct bss_config_param *pBssConfig,
				       struct sDot11fBeaconIEs *pIes,
				       bool resetCountry)
{
	QDF_STATUS status;
	uint32_t cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	uint8_t channel = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Make sure we have the domain info for the BSS we try to connect to.
	 * Do we need to worry about sequence for OSs that are not Windows??
	 */
	if (pBssDesc) {
		if ((QDF_SAP_MODE !=
			csr_get_session_persona(pMac, sessionId)) &&
			csr_learn_11dcountry_information(
					pMac, pBssDesc, pIes, true)) {
			csr_apply_country_information(pMac);
		}
		if ((wlan_reg_11d_enabled_on_host(pMac->psoc)) && pIes) {
			if (!pIes->Country.present)
				csr_apply_channel_power_info_wrapper(pMac);
		}
	}
	/* Qos */
	csr_set_qos_to_cfg(pMac, sessionId, pBssConfig->qosType);
	/* SSID */
	csr_set_cfg_ssid(pMac, &pBssConfig->SSID);

	/* Auth type */
	cfg_set_int(pMac, WNI_CFG_AUTHENTICATION_TYPE, pBssConfig->authType);
	/* encryption type */
	csr_set_cfg_privacy(pMac, pProfile, (bool) pBssConfig->BssCap.privacy);
	/* short slot time */
	cfg_set_int(pMac, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED,
			pBssConfig->uShortSlotTime);
	/* 11d */
	cfg_set_int(pMac, WNI_CFG_11D_ENABLED,
			((pBssConfig->f11hSupport) ? pBssConfig->f11hSupport :
			 pProfile->ieee80211d));
	cfg_set_int(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT,
			pBssConfig->uPowerLimit);
	/* CB */

	if (CSR_IS_INFRA_AP(pProfile) || CSR_IS_IBSS(pProfile))
		channel = pProfile->operationChannel;
	else if (pBssDesc)
		channel = pBssDesc->channelId;
	if (0 != channel) {
		/* for now if we are on 2.4 Ghz, CB will be always disabled */
		if (WLAN_REG_IS_24GHZ_CH(channel))
			cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
		else
			cfgCb = pBssConfig->cbMode;
	}
	/* Rate */
	/* Fixed Rate */
	if (pBssDesc)
		csr_set_cfg_rate_set(pMac, (eCsrPhyMode) pProfile->phyMode,
				     pProfile, pBssDesc, pIes);
	else
		csr_set_cfg_rate_set_from_profile(pMac, pProfile);
	status = cfg_set_int(pMac, WNI_CFG_JOIN_FAILURE_TIMEOUT,
			pBssConfig->uJoinTimeOut);
	/* Any roaming related changes should be above this line */
	if (pSession && pSession->roam_synch_in_progress) {
		sme_debug("Roam synch is in progress Session_id: %d",
			  sessionId);
		return QDF_STATUS_SUCCESS;
	}
	/* Make this the last CFG to set. The callback will trigger a
	 * join_req Join time out
	 */
	csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_CONFIG, sessionId);

	csr_roam_ccm_cfg_set_callback(pMac, status, sessionId);
	return QDF_STATUS_SUCCESS;
}

/**
 * csr_check_for_hidden_ssid_match() - Check if the current connected SSID
 * is hidden ssid and if it matches with the roamed AP ssid.
 * @mac: Global mac context pointer
 * @session: csr session pointer
 * @roamed_bss_desc: pointer to bss descriptor of roamed bss
 * @roamed_bss_ies: Roamed AP beacon/probe IEs pointer
 *
 * Return: True if the SSID is hidden and matches with roamed SSID else false
 */
static bool
csr_check_for_hidden_ssid_match(tpAniSirGlobal mac,
				struct csr_roam_session *session,
				tSirBssDescription *roamed_bss_desc,
				tDot11fBeaconIEs *roamed_bss_ies)
{
	QDF_STATUS status;
	bool is_null_ssid_match = false;
	tDot11fBeaconIEs *connected_profile_ies = NULL;

	if (!session || !roamed_bss_ies)
		return false;

	status = csr_get_parsed_bss_description_ies(mac,
						    session->pConnectBssDesc,
						    &connected_profile_ies);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Unable to get IES");
		goto error;
	}

	if (!csr_is_nullssid(connected_profile_ies->SSID.ssid,
			     connected_profile_ies->SSID.num_ssid))
		goto error;

	/*
	 * After roam synch indication is received, the driver compares
	 * the SSID of the current AP and SSID of the roamed AP. If
	 * there is a mismatch, driver issues disassociate to current
	 * connected AP. This causes data path queues to be stopped and
	 * M2 to the roamed AP from userspace will fail if EAPOL is
	 * offloaded to userspace. The SSID of the current AP is
	 * parsed from the beacon IEs stored in the connected bss
	 * description. In hidden ssid case the SSID IE has 0 length
	 * and the host receives unicast probe with SSID of the
	 * AP in the roam synch indication. So SSID mismatch happens
	 * and validation fails. So fetch if the connected bss
	 * description has hidden ssid, fill the ssid from the
	 * csr_session connected_profile structure which will
	 * have the SSID.
	 */
	if (!roamed_bss_ies->SSID.present)
		goto error;

	if (roamed_bss_ies->SSID.num_ssid !=
	    session->connectedProfile.SSID.length)
		goto error;

	is_null_ssid_match = !qdf_mem_cmp(session->connectedProfile.SSID.ssId,
					  roamed_bss_ies->SSID.ssid,
					  roamed_bss_ies->SSID.num_ssid);
error:
	if (connected_profile_ies)
		qdf_mem_free(connected_profile_ies);

	return is_null_ssid_match;
}

static
QDF_STATUS csr_roam_stop_network(tpAniSirGlobal mac, uint32_t sessionId,
				 struct csr_roam_profile *roam_profile,
				 tSirBssDescription *pBssDesc,
				 tDot11fBeaconIEs *pIes)
{
	QDF_STATUS status;
	struct bss_config_param *pBssConfig;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool ssid_match;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pBssConfig = qdf_mem_malloc(sizeof(struct bss_config_param));
	if (NULL == pBssConfig)
		return QDF_STATUS_E_NOMEM;

	sme_debug("session id: %d", sessionId);

	status = csr_roam_prepare_bss_config(mac, roam_profile, pBssDesc,
					     pBssConfig, pIes);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		enum csr_roam_substate substate;

		substate = eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING;
		pSession->bssParams.uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
		/* This will allow to pass cbMode during join req */
		pSession->bssParams.cbMode = pBssConfig->cbMode;
		/* For IBSS, we need to prepare some more information */
		if (csr_is_bss_type_ibss(roam_profile->BSSType) ||
				CSR_IS_INFRA_AP(roam_profile))
			csr_roam_prepare_bss_params(mac, sessionId,
						    roam_profile, pBssDesc,
						    pBssConfig, pIes);
		/*
		 * If we are in an IBSS, then stop the IBSS...
		 * Not worry about WDS connection for now
		 */
		if (csr_is_conn_state_ibss(mac, sessionId)) {
			status = csr_roam_issue_stop_bss(mac, sessionId,
					substate);
		} else if (csr_is_conn_state_infra(mac, sessionId)) {
			/*
			 * the new Bss is an Ibss OR we are roaming from
			 * Infra to Infra across SSIDs
			 * (roaming to a new SSID)...
			 * Not worry about WDS connection for now
			 */
			ssid_match =
				csr_check_for_hidden_ssid_match(mac, pSession,
								pBssDesc, pIes);
			if (!ssid_match)
				ssid_match = csr_is_ssid_equal(
						mac, pSession->pConnectBssDesc,
						pBssDesc, pIes);

			if (pBssDesc &&
			    (csr_is_ibss_bss_desc(pBssDesc) || !ssid_match))
				status = csr_roam_issue_disassociate(mac,
						sessionId, substate, false);
			else if (pBssDesc)
				/*
				 * In an infra & going to an infra network with
				 * the same SSID.  This calls for a reassoc seq.
				 * So issue the CFG sets for this new AP. Set
				 * parameters for this Bss.
				 */
				status = csr_roam_set_bss_config_cfg(
						mac, sessionId, roam_profile,
						pBssDesc, pBssConfig, pIes,
						false);
		} else if (pBssDesc || CSR_IS_INFRA_AP(roam_profile)) {
			/*
			 * Neither in IBSS nor in Infra. We can go ahead and set
			 * the cfg for tne new network... nothing to stop.
			 */
			bool is_11r_roaming = false;

			is_11r_roaming = csr_roam_is11r_assoc(mac, sessionId);
			/* Set parameters for this Bss. */
			status = csr_roam_set_bss_config_cfg(mac, sessionId,
							     roam_profile,
							     pBssDesc,
							     pBssConfig, pIes,
							     is_11r_roaming);
		}
	} /* Success getting BSS config info */
	qdf_mem_free(pBssConfig);
	return status;
}

/**
 * csr_roam_state_for_same_profile() - Determine roam state for same profile
 * @mac_ctx: pointer to mac context
 * @profile: Roam profile
 * @session: Roam session
 * @session_id: session id
 * @ies_local: local ies
 * @bss_descr: bss description
 *
 * This function will determine the roam state for same profile
 *
 * Return: Roaming state.
 */
static enum csr_join_state csr_roam_state_for_same_profile(
	tpAniSirGlobal mac_ctx, struct csr_roam_profile *profile,
			struct csr_roam_session *session,
			uint32_t session_id, tDot11fBeaconIEs *ies_local,
			tSirBssDescription *bss_descr)
{
	QDF_STATUS status;
	struct bss_config_param bssConfig;

	if (csr_roam_is_same_profile_keys(mac_ctx, &session->connectedProfile,
				profile))
		return eCsrReassocToSelfNoCapChange;
	/* The key changes */
	qdf_mem_zero(&bssConfig, sizeof(bssConfig));
	status = csr_roam_prepare_bss_config(mac_ctx, profile, bss_descr,
				&bssConfig, ies_local);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		session->bssParams.uCfgDot11Mode =
				bssConfig.uCfgDot11Mode;
		session->bssParams.cbMode =
				bssConfig.cbMode;
		/* reapply the cfg including keys so reassoc happens. */
		status = csr_roam_set_bss_config_cfg(mac_ctx, session_id,
				profile, bss_descr, &bssConfig,
				ies_local, false);
		if (QDF_IS_STATUS_SUCCESS(status))
			return eCsrContinueRoaming;
	}

	return eCsrStopRoaming;

}

static enum csr_join_state csr_roam_join(tpAniSirGlobal pMac,
	uint32_t sessionId, tCsrScanResultInfo *pScanResult,
				   struct csr_roam_profile *pProfile)
{
	enum csr_join_state eRoamState = eCsrContinueRoaming;
	tSirBssDescription *pBssDesc = &pScanResult->BssDescriptor;
	tDot11fBeaconIEs *pIesLocal = (tDot11fBeaconIEs *) (pScanResult->pvIes);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return eCsrStopRoaming;
	}

	if (!pIesLocal &&
		!QDF_IS_STATUS_SUCCESS(csr_get_parsed_bss_description_ies(pMac,
				pBssDesc, &pIesLocal))) {
		sme_err("fail to parse IEs");
		return eCsrStopRoaming;
	}
	if (csr_is_infra_bss_desc(pBssDesc)) {
		/*
		 * If we are connected in infra mode and the join bss descr is
		 * for the same BssID, then we are attempting to join the AP we
		 * are already connected with.  In that case, see if the Bss or
		 * sta capabilities have changed and handle the changes
		 * without disturbing the current association
		 */

		if (csr_is_conn_state_connected_infra(pMac, sessionId) &&
			csr_is_bss_id_equal(pBssDesc,
					    pSession->pConnectBssDesc) &&
			csr_is_ssid_equal(pMac, pSession->pConnectBssDesc,
				pBssDesc, pIesLocal)) {
			/*
			 * Check to see if the Auth type has changed in the
			 * profile.  If so, we don't want to reassociate with
			 * authenticating first.  To force this, stop the
			 * current association (Disassociate) and then re 'Join'
			 * the AP, wihch will force an Authentication (with the
			 * new Auth type) followed by a new Association.
			 */
			if (csr_is_same_profile(pMac,
				&pSession->connectedProfile, pProfile)) {
				sme_warn("detect same profile");
				eRoamState =
					csr_roam_state_for_same_profile(pMac,
						pProfile, pSession, sessionId,
						pIesLocal, pBssDesc);
			} else if (!QDF_IS_STATUS_SUCCESS(
						csr_roam_issue_disassociate(
						pMac,
						sessionId,
						eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
						false))) {
				sme_err("fail disassoc session %d",
						sessionId);
				eRoamState = eCsrStopRoaming;
			}
		} else if (!QDF_IS_STATUS_SUCCESS(csr_roam_stop_network(pMac,
				sessionId, pProfile, pBssDesc, pIesLocal)))
			/* we used to pre-auth here with open auth
			 * networks but that wasn't working so well.
			 * stop the existing network before attempting
			 * to join the new network.
			 */
			eRoamState = eCsrStopRoaming;
	} else if (!QDF_IS_STATUS_SUCCESS(csr_roam_stop_network(pMac, sessionId,
						pProfile, pBssDesc,
						pIesLocal)))
		eRoamState = eCsrStopRoaming;

	if (pIesLocal && !pScanResult->pvIes)
		qdf_mem_free(pIesLocal);
	return eRoamState;
}

static
QDF_STATUS csr_roam_should_roam(tpAniSirGlobal pMac, uint32_t sessionId,
				tSirBssDescription *pBssDesc, uint32_t roamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;
	roam_info->pBssDesc = pBssDesc;
	status = csr_roam_call_callback(pMac, sessionId, roam_info, roamId,
					eCSR_ROAM_SHOULD_ROAM,
					eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);
	return status;
}

/* In case no matching BSS is found, use whatever default we can find */
static void csr_roam_assign_default_param(tpAniSirGlobal pMac,
					tSmeCmd *pCommand)
{
	/* Need to get all negotiated types in place first */
	/* auth type */
	/* Take the preferred Auth type. */
	switch (pCommand->u.roamCmd.roamProfile.AuthType.authType[0]) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_OPEN_SYSTEM;
		break;

	case eCSR_AUTH_TYPE_SHARED_KEY:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_SHARED_KEY;
		break;

	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_AUTOSWITCH;
		break;

	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_SAE;
		break;
	}
	pCommand->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
		pCommand->u.roamCmd.roamProfile.EncryptionType.
		encryptionType[0];
	/* In this case, the multicast encryption needs to follow the
	 * uncast ones.
	 */
	pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
		pCommand->u.roamCmd.roamProfile.EncryptionType.
		encryptionType[0];
}

/**
 * csr_roam_select_bss() - Handle join scenario based on profile
 * @mac_ctx:             Global MAC Context
 * @roam_bss_entry:      The next BSS to join
 * @csr_result_info:     Result of join
 * @csr_scan_result:     Global scan result
 * @vdev_id:             SME Session ID
 * @roam_id:             Roaming ID
 * @roam_state:          Current roaming state
 * @bss_list:            BSS List
 *
 * Return: true if the entire BSS list is done, false otherwise.
 */
static bool csr_roam_select_bss(tpAniSirGlobal mac_ctx,
		tListElem **roam_bss_entry, tCsrScanResultInfo **csr_result_info,
		struct tag_csrscan_result **csr_scan_result,
		uint32_t vdev_id, uint32_t roam_id,
		enum csr_join_state *roam_state,
		struct scan_result_list *bss_list)
{
	uint8_t conc_channel = 0;
	uint32_t temp_vdev_id;
	bool status = false;
	struct tag_csrscan_result *scan_result = NULL;
	tCsrScanResultInfo *result = NULL;
	enum QDF_OPMODE op_mode;
	struct wlan_objmgr_vdev *vdev;
	enum policy_mgr_con_mode mode;
	uint8_t chan_id;
	QDF_STATUS qdf_status;
	eCsrPhyMode self_phymode = mac_ctx->roam.configParam.phyMode;
	tDot11fBeaconIEs *bcn_ies;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac_ctx->pdev,
						    vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Vdev ref error");
		return true;
	}
	op_mode = wlan_vdev_mlme_get_opmode(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	while (*roam_bss_entry) {
		scan_result = GET_BASE_ADDR(*roam_bss_entry, struct
				tag_csrscan_result, Link);
		/*
		 * If concurrency enabled take the
		 * concurrent connected channel first.
		 * Valid multichannel concurrent
		 * sessions exempted
		 */
		result = &scan_result->Result;
		bcn_ies = result->pvIes;
		/*
		 * If phymode is configured to DOT11 Only profile.
		 * Don't connect to profile which is less than them.
		 */
		if (bcn_ies && ((self_phymode == eCSR_DOT11_MODE_11n_ONLY &&
		   !bcn_ies->HTCaps.present) ||
		   (self_phymode == eCSR_DOT11_MODE_11ac_ONLY &&
		   !bcn_ies->VHTCaps.present) ||
		   (self_phymode == eCSR_DOT11_MODE_11ax_ONLY &&
		   !bcn_ies->he_cap.present))) {
			sme_info("self_phymode %d mismatch HT %d VHT %d HE %d",
				self_phymode, bcn_ies->HTCaps.present,
				bcn_ies->VHTCaps.present,
				bcn_ies->he_cap.present);
			*roam_state = eCsrStopRoamingDueToConcurrency;
			status = true;
			*roam_bss_entry = csr_ll_next(&bss_list->List,
						      *roam_bss_entry,
						      LL_ACCESS_LOCK);
			continue;
		}

		/*
		 * Ignore the BSS if any other vdev is already connected
		 * to it.
		 */
		qdf_status = csr_roam_get_session_id_from_bssid(mac_ctx,
				(struct qdf_mac_addr *)
				&result->BssDescriptor.bssId, &temp_vdev_id);
		if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
			sme_info("vdev_id %d already connected to %pM. select next bss for vdev_id %d",
				 temp_vdev_id, result->BssDescriptor.bssId,
				 vdev_id);
			*roam_state = eCsrStopRoamingDueToConcurrency;
			status = true;
			*roam_bss_entry = csr_ll_next(&bss_list->List,
						      *roam_bss_entry,
						      LL_ACCESS_LOCK);
			continue;
		}

		chan_id = result->BssDescriptor.channelId;
		mode = policy_mgr_convert_device_mode_to_qdf_type(op_mode);
		/* If concurrency is not allowed select next bss */
		if (!policy_mgr_is_concurrency_allowed(mac_ctx->psoc,
						       mode,
						       chan_id,
						       HW_MODE_20_MHZ)) {
			sme_err("Concurrency not allowed for this channel %d bssid %pM, selecting next",
				chan_id, result->BssDescriptor.bssId);
			*roam_state = eCsrStopRoamingDueToConcurrency;
			status = true;
			*roam_bss_entry = csr_ll_next(&bss_list->List,
						      *roam_bss_entry,
						      LL_ACCESS_LOCK);
			continue;
		}

		/*
		 * check if channel is allowed for current hw mode, if not fetch
		 * next BSS.
		 */
		if (!policy_mgr_is_hwmode_set_for_given_chnl(mac_ctx->psoc,
					result->BssDescriptor.channelId)) {
			sme_err("HW mode is not properly set for channel %d BSSID %pM",
				result->BssDescriptor.channelId,
				result->BssDescriptor.bssId);
			*roam_state = eCsrStopRoamingDueToConcurrency;
			status = true;
			*roam_bss_entry = csr_ll_next(&bss_list->List,
						     *roam_bss_entry,
						     LL_ACCESS_LOCK);
			continue;
		}
		if (policy_mgr_concurrent_open_sessions_running(mac_ctx->psoc)
			&& !csr_is_valid_mc_concurrent_session(mac_ctx,
					vdev_id, &result->BssDescriptor)) {
			conc_channel = csr_get_concurrent_operation_channel(
					mac_ctx);
			sme_debug("csr Conc Channel: %d", conc_channel);
			if ((conc_channel) && (conc_channel ==
				result->BssDescriptor.channelId)) {
				/*
				 * make this 0 because we do not want the below
				 * check to pass as we don't want to connect on
				 * other channel
				 */
				sme_debug("Conc chnl match: %d", conc_channel);
				conc_channel = 0;
			}
		}

		/* Ok to roam this */
		if (!conc_channel &&
			QDF_IS_STATUS_SUCCESS(csr_roam_should_roam(mac_ctx,
				vdev_id, &result->BssDescriptor, roam_id))) {
			status = false;
			break;
		}
		*roam_state = eCsrStopRoamingDueToConcurrency;
		status = true;
		*roam_bss_entry = csr_ll_next(&bss_list->List, *roam_bss_entry,
					LL_ACCESS_LOCK);
	}
	*csr_result_info = result;
	*csr_scan_result = scan_result;
	return status;
}

/**
 * csr_roam_join_handle_profile() - Handle join scenario based on profile
 * @mac_ctx:             Global MAC Context
 * @session_id:          SME Session ID
 * @cmd:                 Command
 * @roam_info_ptr:       Pointed to the roaming info for join
 * @roam_state:          Current roaming state
 * @result:              Result of join
 * @scan_result:         Global scan result
 *
 * Return: None
 */
static void csr_roam_join_handle_profile(tpAniSirGlobal mac_ctx,
		uint32_t session_id, tSmeCmd *cmd,
		struct csr_roam_info *roam_info_ptr,
		enum csr_join_state *roam_state, tCsrScanResultInfo *result,
		struct tag_csrscan_result *scan_result)
{
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	uint8_t acm_mask = 0;
#endif
	QDF_STATUS status;
	struct csr_roam_session *session;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	tDot11fBeaconIEs *ies_local = NULL;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	/*
	 * We have something to roam, tell HDD when it is infra.
	 * For IBSS, the indication goes back to HDD via eCSR_ROAM_IBSS_IND
	 */
	if (CSR_IS_INFRASTRUCTURE(profile) && roam_info_ptr) {
		if (session->bRefAssocStartCnt) {
			session->bRefAssocStartCnt--;
			roam_info_ptr->pProfile = profile;
			/*
			 * Complete the last assoc attempt as a
			 * new one is about to be tried
			 */
			csr_roam_call_callback(mac_ctx, session_id,
				roam_info_ptr, cmd->u.roamCmd.roamId,
				eCSR_ROAM_ASSOCIATION_COMPLETION,
				eCSR_ROAM_RESULT_NOT_ASSOCIATED);
		}

		qdf_mem_zero(roam_info_ptr, sizeof(struct csr_roam_info));
		if (!scan_result)
			cmd->u.roamCmd.roamProfile.uapsd_mask = 0;
		else
			ies_local = scan_result->Result.pvIes;

		if (!result) {
			sme_err(" cannot parse IEs");
			*roam_state = eCsrStopRoaming;
			return;
		} else if (scan_result && !ies_local &&
				(!QDF_IS_STATUS_SUCCESS(
					csr_get_parsed_bss_description_ies(
						mac_ctx, &result->BssDescriptor,
						&ies_local)))) {
			sme_err(" cannot parse IEs");
			*roam_state = eCsrStopRoaming;
			return;
		}
		roam_info_ptr->pBssDesc = &result->BssDescriptor;
		cmd->u.roamCmd.pLastRoamBss = roam_info_ptr->pBssDesc;
		/* dont put uapsd_mask if BSS doesn't support uAPSD */
		if (scan_result && cmd->u.roamCmd.roamProfile.uapsd_mask
				&& CSR_IS_QOS_BSS(ies_local)
				&& CSR_IS_UAPSD_BSS(ies_local)) {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
			acm_mask = sme_qos_get_acm_mask(mac_ctx,
					&result->BssDescriptor, ies_local);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT */
		} else {
			cmd->u.roamCmd.roamProfile.uapsd_mask = 0;
		}
		if (ies_local && !scan_result->Result.pvIes)
			qdf_mem_free(ies_local);
		roam_info_ptr->pProfile = profile;
		session->bRefAssocStartCnt++;
		csr_roam_call_callback(mac_ctx, session_id, roam_info_ptr,
			cmd->u.roamCmd.roamId, eCSR_ROAM_ASSOCIATION_START,
			eCSR_ROAM_RESULT_NONE);
	}
	if (NULL != cmd->u.roamCmd.pRoamBssEntry) {
		/*
		 * We have BSS
		 * Need to assign these value because
		 * they are used in csr_is_same_profile
		 */
		scan_result = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
				struct tag_csrscan_result, Link);
		/*
		 * The OSEN IE doesn't provide the cipher suite.Therefore set
		 * to constant value of AES
		 */
		if (cmd->u.roamCmd.roamProfile.bOSENAssociation) {
			cmd->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
				eCSR_ENCRYPT_TYPE_AES;
			cmd->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
				eCSR_ENCRYPT_TYPE_AES;
		} else {
			/* Negotiated while building scan result. */
			cmd->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
				scan_result->ucEncryptionType;
			cmd->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
				scan_result->mcEncryptionType;
		}
		cmd->u.roamCmd.roamProfile.negotiatedAuthType =
			scan_result->authType;
		if (CSR_IS_START_IBSS(&cmd->u.roamCmd.roamProfile)) {
			if (csr_is_same_profile(mac_ctx,
				&session->connectedProfile, profile)) {
				*roam_state = eCsrStartIbssSameIbss;
				return;
			}
		}
		if (cmd->u.roamCmd.fReassocToSelfNoCapChange) {
			/* trying to connect to the one already connected */
			cmd->u.roamCmd.fReassocToSelfNoCapChange = false;
			*roam_state = eCsrReassocToSelfNoCapChange;
			return;
		}
		/* Attempt to Join this Bss... */
		*roam_state = csr_roam_join(mac_ctx, session_id,
				&scan_result->Result, profile);
		return;
	}

	/* For an IBSS profile, then we need to start the IBSS. */
	if (CSR_IS_START_IBSS(profile)) {
		bool same_ibss = false;
		/* Attempt to start this IBSS... */
		csr_roam_assign_default_param(mac_ctx, cmd);
		status = csr_roam_start_ibss(mac_ctx, session_id,
				profile, &same_ibss);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			if (same_ibss)
				*roam_state = eCsrStartIbssSameIbss;
			else
				*roam_state = eCsrContinueRoaming;
		} else {
			/* it somehow fail need to stop */
			*roam_state = eCsrStopRoaming;
		}
		return;
	} else if (CSR_IS_INFRA_AP(profile)) {
		/* Attempt to start this WDS... */
		csr_roam_assign_default_param(mac_ctx, cmd);
		/* For AP WDS, we dont have any BSSDescription */
		status = csr_roam_start_wds(mac_ctx, session_id, profile, NULL);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else if (CSR_IS_NDI(profile)) {
		csr_roam_assign_default_param(mac_ctx, cmd);
		status = csr_roam_start_ndi(mac_ctx, session_id, profile);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else {
		/* Nothing we can do */
		sme_warn("cannot continue without BSS list");
		*roam_state = eCsrStopRoaming;
		return;
	}

}
/**
 * csr_roam_join_next_bss() - Pick the next BSS for join
 * @mac_ctx:             Global MAC Context
 * @cmd:                 Command
 * @use_same_bss:        Use Same BSS to Join
 *
 * Return: The Join State
 */
static enum csr_join_state csr_roam_join_next_bss(tpAniSirGlobal mac_ctx,
		tSmeCmd *cmd, bool use_same_bss)
{
	struct tag_csrscan_result *scan_result = NULL;
	enum csr_join_state roam_state = eCsrStopRoaming;
	struct scan_result_list *bss_list =
		(struct scan_result_list *) cmd->u.roamCmd.hBSSList;
	bool done = false;
	struct csr_roam_info *roam_info = NULL;
	uint32_t session_id = cmd->sessionId;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_joinstatus *join_status;
	tCsrScanResultInfo *result = NULL;

	if (!session) {
		sme_err("session %d not found", session_id);
		return eCsrStopRoaming;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info) {
		sme_err("failed to allocate memory");
		return eCsrStopRoaming;
	}
	qdf_mem_copy(&roam_info->bssid, &session->joinFailStatusCode.bssId,
			sizeof(tSirMacAddr));
	/*
	 * When handling AP's capability change, continue to associate
	 * to same BSS and make sure pRoamBssEntry is not Null.
	 */
	if (bss_list) {
		if (!cmd->u.roamCmd.pRoamBssEntry) {
			/* Try the first BSS */
			cmd->u.roamCmd.pLastRoamBss = NULL;
			cmd->u.roamCmd.pRoamBssEntry =
				csr_ll_peek_head(&bss_list->List,
					LL_ACCESS_LOCK);
		} else if (!use_same_bss) {
			cmd->u.roamCmd.pRoamBssEntry =
				csr_ll_next(&bss_list->List,
					cmd->u.roamCmd.pRoamBssEntry,
					LL_ACCESS_LOCK);
			/*
			 * Done with all the BSSs.
			 * In this case, will tell HDD the
			 * completion
			 */
			if (NULL == cmd->u.roamCmd.pRoamBssEntry)
				goto end;
			/*
			 * We need to indicate to HDD that we
			 * are done with this one.
			 */
			roam_info->pBssDesc = cmd->u.roamCmd.pLastRoamBss;
			join_status = &session->joinFailStatusCode;
			roam_info->statusCode = join_status->statusCode;
			roam_info->reasonCode = join_status->reasonCode;
		} else {
			/*
			 * Try connect to same BSS again. Fill roam_info for the
			 * last attemp to indicate to HDD.
			 */
			roam_info->pBssDesc = cmd->u.roamCmd.pLastRoamBss;
			join_status = &session->joinFailStatusCode;
			roam_info->statusCode = join_status->statusCode;
			roam_info->reasonCode = join_status->reasonCode;
		}

		done = csr_roam_select_bss(mac_ctx,
					   &cmd->u.roamCmd.pRoamBssEntry,
					   &result, &scan_result, session_id,
					   cmd->u.roamCmd.roamId,
					   &roam_state, bss_list);
		if (done)
			goto end;
	}

	roam_info->u.pConnectedProfile = &session->connectedProfile;

	csr_roam_join_handle_profile(mac_ctx, session_id, cmd, roam_info,
		&roam_state, result, scan_result);
end:
	if ((eCsrStopRoaming == roam_state) && CSR_IS_INFRASTRUCTURE(profile) &&
		(session->bRefAssocStartCnt > 0)) {
		/*
		 * Need to indicate association_completion if association_start
		 * has been done
		 */
		session->bRefAssocStartCnt--;
		/*
		 * Complete the last assoc attempte as a
		 * new one is about to be tried
		 */
		roam_info->pProfile = profile;
		csr_roam_call_callback(mac_ctx, session_id,
			roam_info, cmd->u.roamCmd.roamId,
			eCSR_ROAM_ASSOCIATION_COMPLETION,
			eCSR_ROAM_RESULT_NOT_ASSOCIATED);
	}
	qdf_mem_free(roam_info);

	return roam_state;
}

static QDF_STATUS csr_roam(tpAniSirGlobal pMac, tSmeCmd *pCommand,
			   bool use_same_bss)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum csr_join_state RoamState;
	enum csr_roam_substate substate;
	uint32_t sessionId = pCommand->sessionId;

	/* Attept to join a Bss... */
	RoamState = csr_roam_join_next_bss(pMac, pCommand, use_same_bss);

	/* if nothing to join.. */
	if ((eCsrStopRoaming == RoamState) ||
		(eCsrStopRoamingDueToConcurrency == RoamState)) {
		bool fComplete = false;
		/* and if connected in Infrastructure mode... */
		if (csr_is_conn_state_infra(pMac, sessionId)) {
			/* ... then we need to issue a disassociation */
			substate = eCSR_ROAM_SUBSTATE_DISASSOC_NOTHING_TO_JOIN;
			status = csr_roam_issue_disassociate(pMac, sessionId,
					substate, false);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_warn("fail issuing disassoc status = %d",
					status);
				/*
				 * roam command is completed by caller in the
				 * failed case
				 */
				fComplete = true;
			}
		} else if (csr_is_conn_state_ibss(pMac, sessionId)) {
			status = csr_roam_issue_stop_bss(pMac, sessionId,
					eCSR_ROAM_SUBSTATE_STOP_BSS_REQ);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_warn("fail issuing stop bss status = %d",
					status);
				/*
				 * roam command is completed by caller in the
				 * failed case
				 */
				fComplete = true;
			}
		} else if (csr_is_conn_state_connected_infra_ap(pMac,
					sessionId)) {
			substate = eCSR_ROAM_SUBSTATE_STOP_BSS_REQ;
			status = csr_roam_issue_stop_bss(pMac, sessionId,
						substate);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_warn("fail issuing stop bss status = %d",
					status);
				/*
				 * roam command is completed by caller in the
				 * failed case
				 */
				fComplete = true;
			}
		} else {
			fComplete = true;
		}

		if (fComplete) {
			/* otherwise, we can complete the Roam command here. */
			if (eCsrStopRoamingDueToConcurrency == RoamState)
				csr_roam_complete(pMac,
					eCsrJoinFailureDueToConcurrency, NULL,
					sessionId);
			else
				csr_roam_complete(pMac,
					eCsrNothingToJoin, NULL, sessionId);
		}
	} else if (eCsrReassocToSelfNoCapChange == RoamState) {
		csr_roam_complete(pMac, eCsrSilentlyStopRoamingSaveState,
				NULL, sessionId);
	} else if (eCsrStartIbssSameIbss == RoamState) {
		csr_roam_complete(pMac, eCsrSilentlyStopRoaming, NULL,
				sessionId);
	}

	return status;
}

static
QDF_STATUS csr_process_ft_reassoc_roam_command(tpAniSirGlobal pMac,
					       tSmeCmd *pCommand)
{
	uint32_t sessionId;
	struct csr_roam_session *pSession;
	struct tag_csrscan_result *pScanResult = NULL;
	tSirBssDescription *pBssDesc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	sessionId = pCommand->sessionId;
	pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (CSR_IS_ROAMING(pSession) && pSession->fCancelRoaming) {
		/* the roaming is cancelled. Simply complete the command */
		sme_debug("Roam command canceled");
		csr_roam_complete(pMac, eCsrNothingToJoin, NULL, sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	if (pCommand->u.roamCmd.pRoamBssEntry) {
		pScanResult =
			GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry,
				      struct tag_csrscan_result, Link);
		pBssDesc = &pScanResult->Result.BssDescriptor;
	} else {
		/* the roaming is cancelled. Simply complete the command */
		sme_debug("Roam command canceled");
		csr_roam_complete(pMac, eCsrNothingToJoin, NULL, sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	status = csr_roam_issue_reassociate(pMac, sessionId, pBssDesc,
					    (tDot11fBeaconIEs *) (pScanResult->
								  Result.pvIes),
					    &pCommand->u.roamCmd.roamProfile);
	return status;
}

/**
 * csr_roam_trigger_reassociate() - Helper function to trigger reassociate
 * @mac_ctx: pointer to mac context
 * @cmd: sme command
 * @session_ptr: session pointer
 * @session_id: session id
 *
 * This function will trigger reassociate.
 *
 * Return: QDF_STATUS for success or failure.
 */
static QDF_STATUS
csr_roam_trigger_reassociate(tpAniSirGlobal mac_ctx, tSmeCmd *cmd,
			     struct csr_roam_session *session_ptr,
			     uint32_t session_id)
{
	tDot11fBeaconIEs *pIes = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	if (session_ptr->pConnectBssDesc) {
		status = csr_get_parsed_bss_description_ies(mac_ctx,
				session_ptr->pConnectBssDesc, &pIes);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to parse IEs");
		} else {
			roam_info->reasonCode =
					eCsrRoamReasonStaCapabilityChanged;
			csr_roam_call_callback(mac_ctx, session_ptr->sessionId,
					roam_info, 0, eCSR_ROAM_ROAMING_START,
					eCSR_ROAM_RESULT_NONE);
			session_ptr->roamingReason = eCsrReassocRoaming;
			roam_info->pBssDesc = session_ptr->pConnectBssDesc;
			roam_info->pProfile = &cmd->u.roamCmd.roamProfile;
			session_ptr->bRefAssocStartCnt++;
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
				cmd->u.roamCmd.roamId,
				eCSR_ROAM_ASSOCIATION_START,
				eCSR_ROAM_RESULT_NONE);

			sme_debug("calling csr_roam_issue_reassociate");
			status = csr_roam_issue_reassociate(mac_ctx, session_id,
					session_ptr->pConnectBssDesc, pIes,
					&cmd->u.roamCmd.roamProfile);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("failed status %d", status);
				csr_release_command(mac_ctx, cmd);
			} else {
				csr_neighbor_roam_state_transition(mac_ctx,
					eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING,
					session_id);
			}


			qdf_mem_free(pIes);
			pIes = NULL;
		}
	} else {
		sme_err("reassoc to same AP failed as connected BSS is NULL");
		status = QDF_STATUS_E_FAILURE;
	}
	qdf_mem_free(roam_info);
	return status;
}

/**
 * csr_allow_concurrent_sta_connections() - Wrapper for policy_mgr api
 * @mac: mac context
 * @vdev_id: vdev id
 *
 * This function invokes policy mgr api to check for support of
 * simultaneous connections on concurrent STA interfaces.
 *
 *  Return: If supports return true else false.
 */
static
bool csr_allow_concurrent_sta_connections(tpAniSirGlobal mac,
					  uint32_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE vdev_mode;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("vdev object not found for vdev_id %u", vdev_id);
		return false;
	}
	vdev_mode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	/* If vdev mode is STA then proceed further */
	if (vdev_mode != QDF_STA_MODE)
		return true;

	if (policy_mgr_allow_concurrency(mac->psoc, PM_STA_MODE, 0,
					 HW_MODE_20_MHZ))
		return true;

	return false;
}

#ifdef QCA_SUPPORT_CP_STATS
static void csr_get_peer_rssi_cb(struct stats_event *ev, void *cookie)
{
	tpAniSirGlobal mac = (tpAniSirGlobal)cookie;

	if (!mac)
		return;
	if (!ev->peer_stats) {
		sme_debug("%s no peer stats\n", __func__);
		return;
	}
	mac->peer_rssi = ev->peer_stats->peer_rssi;
	mac->peer_txrate = ev->peer_stats->tx_rate;
	mac->peer_rxrate = ev->peer_stats->rx_rate;
}

static void csr_get_peer_rssi(tpAniSirGlobal mac, uint32_t session_id,
			      struct qdf_mac_addr peer_mac)
{
	struct wlan_objmgr_vdev *vdev;
	struct request_info info = {0};
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						mac->psoc,
						session_id,
						WLAN_LEGACY_SME_ID);
	if (vdev) {
		info.cookie = mac;
		info.u.get_peer_rssi_cb = csr_get_peer_rssi_cb;
		info.vdev_id = wlan_vdev_get_id(vdev);
		info.pdev_id = wlan_objmgr_pdev_get_pdev_id(
					wlan_vdev_get_pdev(vdev));
		qdf_mem_copy(info.peer_mac_addr, &peer_mac, QDF_MAC_ADDR_SIZE);
		status = ucfg_mc_cp_stats_send_stats_request(
						vdev,
						TYPE_PEER_STATS,
						&info);

		sme_debug("peer_mac" QDF_MAC_ADDR_STR,
			  QDF_MAC_ADDR_ARRAY(peer_mac.bytes));
		if (QDF_IS_STATUS_ERROR(status))
			sme_err("stats req failed: %d", status);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	}
}
#else
static void csr_get_peer_rssi(tpAniSirGlobal mac, uint32_t session_id,
			      struct qdf_mac_addr peer_mac)
{
}
#endif

QDF_STATUS csr_roam_process_command(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
	QDF_STATUS lock_status, status = QDF_STATUS_SUCCESS;
	uint32_t sessionId = pCommand->sessionId;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	struct qdf_mac_addr peer_mac;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug("Roam Reason: %d sessionId: %d",
		pCommand->u.roamCmd.roamReason, sessionId);

	pSession->disconnect_reason = pCommand->u.roamCmd.disconnect_reason;

	switch (pCommand->u.roamCmd.roamReason) {
	case eCsrForcedDisassoc:
		status = csr_roam_process_disassoc_deauth(pMac, pCommand,
				true, false);
		lock_status = sme_acquire_global_lock(&pMac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(pMac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(pMac, sessionId);
		sme_release_global_lock(&pMac->sme);
		break;
	case eCsrSmeIssuedDisassocForHandoff:
		/* Not to free pMac->roam.pCurRoamProfile (via
		 * csr_free_roam_profile) because its needed after disconnect
		 */
		status = csr_roam_process_disassoc_deauth(pMac, pCommand,
				true, false);

		break;
	case eCsrForcedDisassocMICFailure:
		status = csr_roam_process_disassoc_deauth(pMac, pCommand,
				true, true);
		lock_status = sme_acquire_global_lock(&pMac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(pMac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(pMac, sessionId);
		sme_release_global_lock(&pMac->sme);
		break;
	case eCsrForcedDeauth:
		status = csr_roam_process_disassoc_deauth(pMac, pCommand,
				false, false);
		lock_status = sme_acquire_global_lock(&pMac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(pMac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(pMac, sessionId);
		sme_release_global_lock(&pMac->sme);
		break;
	case eCsrHddIssuedReassocToSameAP:
	case eCsrSmeIssuedReassocToSameAP:
		status = csr_roam_trigger_reassociate(pMac, pCommand,
						      pSession, sessionId);
		break;
	case eCsrCapsChange:
		sme_err("received eCsrCapsChange ");
		csr_roam_state_change(pMac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		status = csr_roam_issue_disassociate(pMac, sessionId,
				eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING,
				false);
		break;
	case eCsrSmeIssuedFTReassoc:
		sme_debug("received FT Reassoc Req");
		status = csr_process_ft_reassoc_roam_command(pMac, pCommand);
		break;

	case eCsrStopBss:
		csr_roam_state_change(pMac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		status = csr_roam_issue_stop_bss(pMac, sessionId,
				eCSR_ROAM_SUBSTATE_STOP_BSS_REQ);
		break;

	case eCsrForcedDisassocSta:
		csr_roam_state_change(pMac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
				sessionId);
		sme_debug("Disassociate issued with reason: %d",
			pCommand->u.roamCmd.reason);

		qdf_mem_copy(&peer_mac, &pCommand->u.roamCmd.peerMac,
			     QDF_MAC_ADDR_SIZE);
		csr_get_peer_rssi(pMac, sessionId, peer_mac);

		status = csr_send_mb_disassoc_req_msg(pMac, sessionId,
				pCommand->u.roamCmd.peerMac,
				pCommand->u.roamCmd.reason);
		break;

	case eCsrForcedDeauthSta:
		csr_roam_state_change(pMac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_DEAUTH_REQ,
				sessionId);

		sme_debug("Deauth issued with reason: %d",
			  pCommand->u.roamCmd.reason);

		qdf_mem_copy(&peer_mac, &pCommand->u.roamCmd.peerMac,
			     QDF_MAC_ADDR_SIZE);
		csr_get_peer_rssi(pMac, sessionId, peer_mac);

		status = csr_send_mb_deauth_req_msg(pMac, sessionId,
				pCommand->u.roamCmd.peerMac,
				pCommand->u.roamCmd.reason);
		break;

	case eCsrPerformPreauth:
		sme_debug("Attempting FT PreAuth Req");
		status = csr_roam_issue_ft_preauth_req(pMac, sessionId,
				pCommand->u.roamCmd.pLastRoamBss);
		break;

	case eCsrHddIssued:
		/*
		 * Check for simultaneous connection support on
		 * multiple STA interfaces.
		 */
		if (!csr_allow_concurrent_sta_connections(pMac, sessionId)) {
			sme_err("No support of conc STA con");
			csr_roam_complete(pMac, eCsrNothingToJoin, NULL,
					  sessionId);
			status = QDF_STATUS_E_FAILURE;
			break;
		}
		/* Fall through for success case */

	default:
		csr_roam_state_change(pMac, eCSR_ROAMING_STATE_JOINING,
				sessionId);

		if (pCommand->u.roamCmd.fUpdateCurRoamProfile) {
			/* Remember the roaming profile */
			lock_status = sme_acquire_global_lock(&pMac->sme);
			if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
				csr_roam_complete(pMac, eCsrNothingToJoin, NULL,
						  sessionId);
				return lock_status;
			}
			csr_free_roam_profile(pMac, sessionId);
			pSession->pCurRoamProfile =
				qdf_mem_malloc(sizeof(struct csr_roam_profile));
			if (NULL != pSession->pCurRoamProfile) {
				csr_roam_copy_profile(pMac,
					pSession->pCurRoamProfile,
					&pCommand->u.roamCmd.roamProfile);
			}
			sme_release_global_lock(&pMac->sme);
		}
		/*
		 * At this point original uapsd_mask is saved in
		 * pCurRoamProfile. uapsd_mask in the pCommand may change from
		 * this point on. Attempt to roam with the new scan results
		 * (if we need to..)
		 */
		status = csr_roam(pMac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_warn("csr_roam() failed with status = 0x%08X",
				status);
		break;
	}
	return status;
}

void csr_reinit_roam_cmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
	if (pCommand->u.roamCmd.fReleaseBssList) {
		csr_scan_result_purge(pMac, pCommand->u.roamCmd.hBSSList);
		pCommand->u.roamCmd.fReleaseBssList = false;
		pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
	}
	if (pCommand->u.roamCmd.fReleaseProfile) {
		csr_release_profile(pMac, &pCommand->u.roamCmd.roamProfile);
		pCommand->u.roamCmd.fReleaseProfile = false;
	}
	pCommand->u.roamCmd.pLastRoamBss = NULL;
	pCommand->u.roamCmd.pRoamBssEntry = NULL;
	/* Because u.roamCmd is union and share with scanCmd and StatusChange */
	qdf_mem_zero(&pCommand->u.roamCmd, sizeof(struct roam_cmd));
}

void csr_reinit_wm_status_change_cmd(tpAniSirGlobal pMac,
			tSmeCmd *pCommand)
{
	qdf_mem_zero(&pCommand->u.wmStatusChangeCmd,
		     sizeof(struct wmstatus_changecmd));
}

void csr_roam_complete(tpAniSirGlobal mac_ctx,
		       enum csr_roamcomplete_result Result,
		       void *Context, uint8_t session_id)
{
	tSmeCmd *sme_cmd;
	struct wlan_serialization_command *cmd;

	cmd = wlan_serialization_peek_head_active_cmd_using_psoc(
				mac_ctx->psoc, false);
	if (!cmd) {
		sme_err("Roam completion called but cmd is not active");
		return;
	}
	sme_cmd = cmd->umac_cmd;
	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	if (eSmeCommandRoam == sme_cmd->command) {
		csr_roam_process_results(mac_ctx, sme_cmd, Result, Context);
		csr_release_command(mac_ctx, sme_cmd);
	}
}


void csr_reset_pmkid_candidate_list(tpAniSirGlobal pMac,
						uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return;
	}
	qdf_mem_zero(&(pSession->PmkidCandidateInfo[0]),
		    sizeof(tPmkidCandidateInfo) * CSR_MAX_PMKID_ALLOWED);
	pSession->NumPmkidCandidate = 0;
}

#ifdef FEATURE_WLAN_WAPI
void csr_reset_bkid_candidate_list(tpAniSirGlobal pMac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return;
	}
	qdf_mem_zero(&(pSession->BkidCandidateInfo[0]),
		    sizeof(tBkidCandidateInfo) * CSR_MAX_BKID_ALLOWED);
	pSession->NumBkidCandidate = 0;
}
#endif /* FEATURE_WLAN_WAPI */

/**
 * csr_roam_save_params() - Helper function to save params
 * @mac_ctx: pointer to mac context
 * @session_ptr: Session pointer
 * @auth_type: auth type
 * @ie_ptr: pointer to ie
 * @ie_local: pointr to local ie
 *
 * This function will save params to session
 *
 * Return: none.
 */
static QDF_STATUS csr_roam_save_params(tpAniSirGlobal mac_ctx,
				struct csr_roam_session *session_ptr,
				eCsrAuthType auth_type,
				tDot11fBeaconIEs *ie_ptr,
				tDot11fBeaconIEs *ie_local)
{
	uint32_t nIeLen;
	uint8_t *pIeBuf;

	if ((eCSR_AUTH_TYPE_RSN == auth_type) ||
	    (eCSR_AUTH_TYPE_FT_RSN == auth_type) ||
	    (eCSR_AUTH_TYPE_FT_RSN_PSK == auth_type) ||
	    (eCSR_AUTH_TYPE_FT_SAE == auth_type) ||
	    (eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384 == auth_type) ||
	    (eCSR_AUTH_TYPE_SUITEB_EAP_SHA256 == auth_type) ||
	    (eCSR_AUTH_TYPE_SUITEB_EAP_SHA384 == auth_type) ||
#if defined WLAN_FEATURE_11W
	    (eCSR_AUTH_TYPE_RSN_PSK_SHA256 == auth_type) ||
	    (eCSR_AUTH_TYPE_RSN_8021X_SHA256 == auth_type) ||
#endif
	    (eCSR_AUTH_TYPE_RSN_PSK == auth_type)) {
		if (ie_local->RSN.present) {
			tDot11fIERSN *rsnie = &ie_local->RSN;
			/*
			 * Calculate the actual length
			 * version + gp_cipher_suite + pwise_cipher_suite_count
			 * + akm_suite_cnt + reserved + pwise_cipher_suites
			 */
			nIeLen = 8 + 2 + 2
				+ (rsnie->pwise_cipher_suite_count * 4)
				+ (rsnie->akm_suite_cnt * 4);
			if (rsnie->pmkid_count)
				/* pmkid */
				nIeLen += 2 + rsnie->pmkid_count * 4;

			/* nIeLen doesn't count EID and length fields */
			session_ptr->pWpaRsnRspIE = qdf_mem_malloc(nIeLen + 2);
			if (NULL == session_ptr->pWpaRsnRspIE)
				return QDF_STATUS_E_NOMEM;

			session_ptr->pWpaRsnRspIE[0] = DOT11F_EID_RSN;
			session_ptr->pWpaRsnRspIE[1] = (uint8_t) nIeLen;
			/* copy upto akm_suite */
			pIeBuf = session_ptr->pWpaRsnRspIE + 2;
			qdf_mem_copy(pIeBuf, &rsnie->version,
					sizeof(rsnie->version));
			pIeBuf += sizeof(rsnie->version);
			qdf_mem_copy(pIeBuf, &rsnie->gp_cipher_suite,
				sizeof(rsnie->gp_cipher_suite));
			pIeBuf += sizeof(rsnie->gp_cipher_suite);
			qdf_mem_copy(pIeBuf, &rsnie->pwise_cipher_suite_count,
				sizeof(rsnie->pwise_cipher_suite_count));
			pIeBuf += sizeof(rsnie->pwise_cipher_suite_count);
			if (rsnie->pwise_cipher_suite_count) {
				/* copy pwise_cipher_suites */
				qdf_mem_copy(pIeBuf, rsnie->pwise_cipher_suites,
					rsnie->pwise_cipher_suite_count * 4);
				pIeBuf += rsnie->pwise_cipher_suite_count * 4;
			}
			qdf_mem_copy(pIeBuf, &rsnie->akm_suite_cnt, 2);
			pIeBuf += 2;
			if (rsnie->akm_suite_cnt) {
				/* copy akm_suite */
				qdf_mem_copy(pIeBuf, rsnie->akm_suite,
					rsnie->akm_suite_cnt * 4);
				pIeBuf += rsnie->akm_suite_cnt * 4;
			}
			/* copy the rest */
			qdf_mem_copy(pIeBuf, rsnie->akm_suite +
				rsnie->akm_suite_cnt * 4,
				2 + rsnie->pmkid_count * 4);
			session_ptr->nWpaRsnRspIeLength = nIeLen + 2;
		}
	} else if ((eCSR_AUTH_TYPE_WPA == auth_type) ||
			(eCSR_AUTH_TYPE_WPA_PSK == auth_type)) {
		if (ie_local->WPA.present) {
			tDot11fIEWPA *wpaie = &ie_local->WPA;
			/* Calculate the actual length wpaie */
			nIeLen = 12 + 2 /* auth_suite_count */
				+ wpaie->unicast_cipher_count * 4
				+ wpaie->auth_suite_count * 4;

			/* The WPA capabilities follows the Auth Suite
			 * (two octects)-- this field is optional, and
			 * we always "send" zero, so just remove it.  This is
			 * consistent with our assumptions in the frames
			 * compiler; nIeLen doesn't count EID & length fields
			 */
			session_ptr->pWpaRsnRspIE = qdf_mem_malloc(nIeLen + 2);
			if (NULL == session_ptr->pWpaRsnRspIE)
				return QDF_STATUS_E_NOMEM;
			session_ptr->pWpaRsnRspIE[0] = DOT11F_EID_WPA;
			session_ptr->pWpaRsnRspIE[1] = (uint8_t) nIeLen;
			pIeBuf = session_ptr->pWpaRsnRspIE + 2;
			/* Copy WPA OUI */
			qdf_mem_copy(pIeBuf, &csr_wpa_oui[1], 4);
			pIeBuf += 4;
			qdf_mem_copy(pIeBuf, &wpaie->version,
				8 + wpaie->unicast_cipher_count * 4);
			pIeBuf += 8 + wpaie->unicast_cipher_count * 4;
			qdf_mem_copy(pIeBuf, &wpaie->auth_suite_count,
				2 + wpaie->auth_suite_count * 4);
			pIeBuf += wpaie->auth_suite_count * 4;
			session_ptr->nWpaRsnRspIeLength = nIeLen + 2;
		}
	}
#ifdef FEATURE_WLAN_WAPI
	else if ((eCSR_AUTH_TYPE_WAPI_WAI_PSK == auth_type) ||
			(eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE ==
			 auth_type)) {
		if (ie_local->WAPI.present) {
			tDot11fIEWAPI *wapi_ie = &ie_local->WAPI;
			/* Calculate the actual length of wapi ie*/
			nIeLen = 4 + 2 /* pwise_cipher_suite_count */
				+ wapi_ie->akm_suite_count * 4
				+ wapi_ie->unicast_cipher_suite_count * 4
				+ 6;  /* gp_cipher_suite + preauth + reserved */

			if (wapi_ie->bkid_count)
				nIeLen += 2 + wapi_ie->bkid_count * 4;

			/* nIeLen doesn't count EID and length fields */
			session_ptr->pWapiRspIE =
				qdf_mem_malloc(nIeLen + 2);
			if (NULL == session_ptr->pWapiRspIE)
				return QDF_STATUS_E_NOMEM;
			session_ptr->pWapiRspIE[0] = DOT11F_EID_WAPI;
			session_ptr->pWapiRspIE[1] = (uint8_t) nIeLen;
			pIeBuf = session_ptr->pWapiRspIE + 2;
			/* copy upto akm_suite_count */
			qdf_mem_copy(pIeBuf, &wapi_ie->version, 2);
			pIeBuf += 4;
			if (wapi_ie->akm_suite_count) {
				/* copy akm_suites */
				qdf_mem_copy(pIeBuf,
					wapi_ie->akm_suites,
					wapi_ie->akm_suite_count * 4);
				pIeBuf += wapi_ie->akm_suite_count * 4;
			}
			qdf_mem_copy(pIeBuf,
				&wapi_ie->unicast_cipher_suite_count, 2);
			pIeBuf += 2;
			if (wapi_ie->unicast_cipher_suite_count) {
				uint16_t suite_size =
					wapi_ie->unicast_cipher_suite_count * 4;
				/* copy pwise_cipher_suites */
				qdf_mem_copy(pIeBuf,
					wapi_ie->unicast_cipher_suites,
					suite_size);
				pIeBuf += suite_size;
			}
			/* gp_cipher_suite */
			qdf_mem_copy(pIeBuf,
				wapi_ie->multicast_cipher_suite, 4);
			pIeBuf += 4;
			/* preauth + reserved */
			qdf_mem_copy(pIeBuf,
				wapi_ie->multicast_cipher_suite + 4, 2);
			pIeBuf += 2;
			if (wapi_ie->bkid_count) {
				/* bkid_count */
				qdf_mem_copy(pIeBuf, &wapi_ie->bkid_count, 2);
				pIeBuf += 2;
				/* copy akm_suites */
				qdf_mem_copy(pIeBuf, wapi_ie->bkid,
					wapi_ie->bkid_count * 4);
				pIeBuf += wapi_ie->bkid_count * 4;
			}
			session_ptr->nWapiRspIeLength = nIeLen + 2;
		}
	}
#endif /* FEATURE_WLAN_WAPI */
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS csr_roam_save_security_rsp_ie(tpAniSirGlobal pMac,
						uint32_t sessionId,
						eCsrAuthType authType,
						tSirBssDescription *pSirBssDesc,
						tDot11fBeaconIEs *pIes)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	tDot11fBeaconIEs *pIesLocal = pIes;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("authType %d session %d", authType, sessionId);
	if ((eCSR_AUTH_TYPE_WPA == authType) ||
		(eCSR_AUTH_TYPE_WPA_PSK == authType) ||
		(eCSR_AUTH_TYPE_RSN == authType) ||
		(eCSR_AUTH_TYPE_RSN_PSK == authType)
		|| (eCSR_AUTH_TYPE_FT_RSN == authType) ||
		(eCSR_AUTH_TYPE_FT_RSN_PSK == authType)
		|| (eCSR_AUTH_TYPE_FT_SAE == authType)
		|| (eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384 == authType)
		|| (eCSR_AUTH_TYPE_SUITEB_EAP_SHA256 == authType)
		|| (eCSR_AUTH_TYPE_SUITEB_EAP_SHA384 == authType)
#ifdef FEATURE_WLAN_WAPI
		|| (eCSR_AUTH_TYPE_WAPI_WAI_PSK == authType) ||
		(eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE == authType)
#endif /* FEATURE_WLAN_WAPI */
#ifdef WLAN_FEATURE_11W
		|| (eCSR_AUTH_TYPE_RSN_PSK_SHA256 == authType) ||
		(eCSR_AUTH_TYPE_RSN_8021X_SHA256 == authType)
#endif /* FEATURE_WLAN_WAPI */
		|| (eCSR_AUTH_TYPE_SAE == authType)) {
		if (!pIesLocal && !QDF_IS_STATUS_SUCCESS
				(csr_get_parsed_bss_description_ies(pMac,
				pSirBssDesc, &pIesLocal)))
			sme_err(" cannot parse IEs");
		if (pIesLocal) {
			status = csr_roam_save_params(pMac, pSession, authType,
					pIes, pIesLocal);
			if (!pIes)
				/* locally allocated */
				qdf_mem_free(pIesLocal);
		}
	}
	return status;
}

/* Returns whether the current association is a 11r assoc or not */
bool csr_roam_is11r_assoc(tpAniSirGlobal pMac, uint8_t sessionId)
{
	return csr_neighbor_roam_is11r_assoc(pMac, sessionId);
}

/* Returns whether "Legacy Fast Roaming" is currently enabled...or not */
bool csr_roam_is_fast_roam_enabled(tpAniSirGlobal pMac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = NULL;

	if (CSR_IS_SESSION_VALID(pMac, sessionId)) {
		pSession = CSR_GET_SESSION(pMac, sessionId);
		if (NULL != pSession->pCurRoamProfile) {
			if (pSession->pCurRoamProfile->csrPersona !=
			    QDF_STA_MODE) {
				return false;
			}
		}
	}
	if (true == CSR_IS_FASTROAM_IN_CONCURRENCY_INI_FEATURE_ENABLED(pMac)) {
		return pMac->roam.configParam.isFastRoamIniFeatureEnabled;
	} else {
		return pMac->roam.configParam.isFastRoamIniFeatureEnabled &&
			(!csr_is_concurrent_session_running(pMac));
	}
}

static
void csr_update_scan_entry_associnfo(tpAniSirGlobal mac_ctx,
				     struct csr_roam_session *session,
				     enum scan_entry_connection_state state)
{
	QDF_STATUS status;
	struct mlme_info mlme;
	struct bss_info bss;
	tCsrRoamConnectedProfile *conn_profile;

	if (!session) {
		sme_debug("session is NULL");
		return;
	}
	conn_profile = &session->connectedProfile;
	if (!CSR_IS_INFRASTRUCTURE(conn_profile)) {
		sme_debug("not infra return");
		return;
	}

	qdf_copy_macaddr(&bss.bssid, &conn_profile->bssid);
	bss.chan = conn_profile->operationChannel;
	bss.ssid.length = conn_profile->SSID.length;
	qdf_mem_copy(&bss.ssid.ssid, &conn_profile->SSID.ssId,
		     bss.ssid.length);

	sme_debug("Update MLME info in scan database. bssid %pM ssid:%.*s chan %d state: %d",
		  bss.bssid.bytes, bss.ssid.length, bss.ssid.ssid, bss.chan,
		  state);
	mlme.assoc_state = state;
	status = ucfg_scan_update_mlme_by_bssinfo(mac_ctx->pdev, &bss, &mlme);
	if (QDF_IS_STATUS_ERROR(status))
		sme_debug("Failed to update the MLME info in scan entry");
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
static eCsrPhyMode csr_roamdot11mode_to_phymode(uint8_t dot11mode)
{
	eCsrPhyMode phymode = eCSR_DOT11_MODE_abg;

	switch (dot11mode) {
	case WNI_CFG_DOT11_MODE_ALL:
		phymode = eCSR_DOT11_MODE_abg;
		break;
	case WNI_CFG_DOT11_MODE_11A:
		phymode = eCSR_DOT11_MODE_11a;
		break;
	case WNI_CFG_DOT11_MODE_11B:
		phymode = eCSR_DOT11_MODE_11b;
		break;
	case WNI_CFG_DOT11_MODE_11G:
		phymode = eCSR_DOT11_MODE_11g;
		break;
	case WNI_CFG_DOT11_MODE_11N:
		phymode = eCSR_DOT11_MODE_11n;
		break;
	case WNI_CFG_DOT11_MODE_11G_ONLY:
		phymode = eCSR_DOT11_MODE_11g_ONLY;
		break;
	case WNI_CFG_DOT11_MODE_11N_ONLY:
		phymode = eCSR_DOT11_MODE_11n_ONLY;
		break;
	case WNI_CFG_DOT11_MODE_11AC:
		phymode = eCSR_DOT11_MODE_11ac;
		break;
	case WNI_CFG_DOT11_MODE_11AC_ONLY:
		phymode = eCSR_DOT11_MODE_11ac_ONLY;
		break;
	case WNI_CFG_DOT11_MODE_11AX:
		phymode = eCSR_DOT11_MODE_11ax;
		break;
	case WNI_CFG_DOT11_MODE_11AX_ONLY:
		phymode = eCSR_DOT11_MODE_11ax_ONLY;
		break;
	default:
		break;
	}

	return phymode;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_roam_synch_clean_up(tpAniSirGlobal mac, uint8_t session_id)
{
	struct scheduler_msg msg = {0};
	struct roam_offload_synch_fail *roam_offload_failed = NULL;
	struct csr_roam_session *session = &mac->roam.roamSession[session_id];

	/* Clean up the roam synch in progress for LFR3 */
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
		  "%s: Roam Synch Failed, Clean Up", __func__);
	session->roam_synch_in_progress = false;

	roam_offload_failed = qdf_mem_malloc(
				sizeof(struct roam_offload_synch_fail));
	if (NULL == roam_offload_failed) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: unable to allocate memory for roam synch fail",
			  __func__);
		return;
	}

	roam_offload_failed->session_id = session_id;
	msg.type     = WMA_ROAM_OFFLOAD_SYNCH_FAIL;
	msg.reserved = 0;
	msg.bodyptr  = roam_offload_failed;
	if (!QDF_IS_STATUS_SUCCESS(scheduler_post_message(QDF_MODULE_ID_SME,
							  QDF_MODULE_ID_WMA,
							  QDF_MODULE_ID_WMA,
							  &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"%s: Unable to post WMA_ROAM_OFFLOAD_SYNCH_FAIL to WMA",
			__func__);
		qdf_mem_free(roam_offload_failed);
	}
}
#endif

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * csr_roam_copy_ht_profile() - Copy from src to dst
 * @dst_profile:          Destination HT profile
 * @src_profile:          Source HT profile
 *
 * Copy the HT profile from the given source to destination
 *
 * Return: None
 */
static void csr_roam_copy_ht_profile(tCsrRoamHTProfile *dst_profile,
		tSirSmeHTProfile *src_profile)
{
	dst_profile->phymode =
		csr_roamdot11mode_to_phymode(src_profile->dot11mode);
	dst_profile->htCapability = src_profile->htCapability;
	dst_profile->htSupportedChannelWidthSet =
		src_profile->htSupportedChannelWidthSet;
	dst_profile->htRecommendedTxWidthSet =
		src_profile->htRecommendedTxWidthSet;
	dst_profile->htSecondaryChannelOffset =
		src_profile->htSecondaryChannelOffset;
	dst_profile->vhtCapability = src_profile->vhtCapability;
	dst_profile->apCenterChan = src_profile->apCenterChan;
	dst_profile->apChanWidth = src_profile->apChanWidth;
}
#endif

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * csr_update_fils_seq_number() - Copy FILS sequence number to roam info
 * @session: CSR Roam Session
 * @roam_info: Roam info
 *
 * Return: None
 */
static void csr_update_fils_seq_number(struct csr_roam_session *session,
					 struct csr_roam_info *roam_info)
{
	roam_info->is_fils_connection = true;
	roam_info->fils_seq_num = session->fils_seq_num;
	pe_debug("FILS sequence number %x", session->fils_seq_num);
}
#else
static inline void csr_update_fils_seq_number(struct csr_roam_session *session,
						struct csr_roam_info *roam_info)
{}
#endif

/**
 * csr_roam_process_results_default() - Process the result for start bss
 * @mac_ctx:          Global MAC Context
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Return: None
 */
static void csr_roam_process_results_default(tpAniSirGlobal mac_ctx,
		     tSmeCmd *cmd, void *context, enum csr_roamcomplete_result
			res)
{
	uint32_t session_id = cmd->sessionId;
	struct csr_roam_session *session;
	struct csr_roam_info *roam_info;
	QDF_STATUS status;
	struct csr_roam_connectedinfo *prev_connect_info = NULL;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	session = CSR_GET_SESSION(mac_ctx, session_id);

	prev_connect_info = &session->prev_assoc_ap_info;

	sme_debug("Assoc ref count: %d", session->bRefAssocStartCnt);

	/* Update AP's assoc info in scan before removing connectedProfile */
	switch (cmd->u.roamCmd.roamReason) {
	case eCsrSmeIssuedDisassocForHandoff:
	case eCsrForcedDisassoc:
	case eCsrForcedDeauth:
	case eCsrForcedDisassocMICFailure:
		csr_update_scan_entry_associnfo(mac_ctx, session,
						SCAN_ENTRY_CON_STATE_NONE);
		break;
	default:
		break;
	}
	if (CSR_IS_INFRASTRUCTURE(&session->connectedProfile)
		|| CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac_ctx, session_id)) {
		/*
		 * do not free for the other profiles as we need
		 * to send down stop BSS later
		 */
		csr_free_connect_bss_desc(mac_ctx, session_id);
		csr_roam_free_connect_profile(&session->connectedProfile);
		csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
		csr_set_default_dot11_mode(mac_ctx);
	}

	/* Copy FILS sequence number used to be updated to userspace */
	if (session->is_fils_connection)
		csr_update_fils_seq_number(session, roam_info);

	switch (cmd->u.roamCmd.roamReason) {
	/*
	 * If this transition is because of an 802.11 OID, then we
	 * transition back to INIT state so we sit waiting for more
	 * OIDs to be issued and we don't start the IDLE timer.
	 */
	case eCsrSmeIssuedFTReassoc:
	case eCsrSmeIssuedAssocToSimilarAP:
	case eCsrHddIssued:
	case eCsrSmeIssuedDisassocForHandoff:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);
		roam_info->pBssDesc = cmd->u.roamCmd.pLastRoamBss;
		roam_info->pProfile = &cmd->u.roamCmd.roamProfile;
		roam_info->statusCode = session->joinFailStatusCode.statusCode;
		roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
		qdf_mem_copy(&roam_info->bssid,
			     &session->joinFailStatusCode.bssId,
			     sizeof(struct qdf_mac_addr));

		if (prev_connect_info->pbFrames) {
			roam_info->nAssocReqLength =
					prev_connect_info->nAssocReqLength;
			roam_info->nAssocRspLength =
					prev_connect_info->nAssocRspLength;
			roam_info->nBeaconLength =
				prev_connect_info->nBeaconLength;
			roam_info->pbFrames = prev_connect_info->pbFrames;
		}
		/*
		 * If Join fails while Handoff is in progress, indicate
		 * disassociated event to supplicant to reconnect
		 */
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)) {
			csr_neighbor_roam_indicate_connect(mac_ctx,
				(uint8_t)session_id, QDF_STATUS_E_FAILURE);
		}
		if (session->bRefAssocStartCnt > 0) {
			session->bRefAssocStartCnt--;
			if (eCsrJoinFailureDueToConcurrency == res)
				csr_roam_call_callback(mac_ctx, session_id,
						       roam_info,
						       cmd->u.roamCmd.roamId,
				       eCSR_ROAM_ASSOCIATION_COMPLETION,
				       eCSR_ROAM_RESULT_ASSOC_FAIL_CON_CHANNEL);
			else
				csr_roam_call_callback(mac_ctx, session_id,
						       roam_info,
						       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_FAILURE);
		} else {
			/*
			 * bRefAssocStartCnt is not incremented when
			 * eRoamState == eCsrStopRoamingDueToConcurrency
			 * in csr_roam_join_next_bss API. so handle this in
			 * else case by sending assoc failure
			 */
			csr_roam_call_callback(mac_ctx, session_id,
					       roam_info,
					       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_FAILURE,
					       eCSR_ROAM_RESULT_FAILURE);
		}
		sme_debug("roam(reason %d) failed", cmd->u.roamCmd.roamReason);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_update_hand_off((uint8_t) session_id, false);
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
			eCSR_ROAM_RESULT_FAILURE, false);
		break;
	case eCsrHddIssuedReassocToSameAP:
	case eCsrSmeIssuedReassocToSameAP:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);

		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_FORCED);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
			eCSR_ROAM_RESULT_FAILURE, false);
		break;
	case eCsrForcedDisassoc:
	case eCsrForcedDeauth:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);
		csr_roam_call_callback(
			mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_FORCED);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
				SME_QOS_CSR_DISCONNECT_IND,
				NULL);
#endif
		csr_roam_link_down(mac_ctx, session_id);

		if (mac_ctx->roam.deauthRspStatus == eSIR_SME_DEAUTH_STATUS) {
			sme_warn("FW still in connected state");
			break;
		}
		break;
	case eCsrForcedIbssLeave:
		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_IBSS_LEAVE,
			eCSR_ROAM_RESULT_IBSS_STOP);
		session->connectState = eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED;
		break;
	case eCsrForcedDisassocMICFailure:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);

		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_MIC_FAILURE);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_REQ, NULL);
#endif
		break;
	case eCsrStopBss:
		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_INFRA_IND,
			eCSR_ROAM_RESULT_INFRA_STOPPED);
		break;
	case eCsrForcedDisassocSta:
	case eCsrForcedDeauthSta:
		roam_info->rssi = mac_ctx->peer_rssi;
		roam_info->tx_rate = mac_ctx->peer_txrate;
		roam_info->rx_rate = mac_ctx->peer_rxrate;

		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
			session_id);
		session = CSR_GET_SESSION(mac_ctx, session_id);
		if (CSR_IS_SESSION_VALID(mac_ctx, session_id) &&
			CSR_IS_INFRA_AP(&session->connectedProfile)) {
			roam_info->u.pConnectedProfile =
				&session->connectedProfile;
			qdf_mem_copy(roam_info->peerMac.bytes,
				     cmd->u.roamCmd.peerMac,
				     sizeof(tSirMacAddr));
			roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
			/* Update the MAC reason code */
			roam_info->disassoc_reason = cmd->u.roamCmd.reason;
			roam_info->statusCode = eSIR_SME_SUCCESS;
			status = csr_roam_call_callback(mac_ctx, session_id,
							roam_info,
							cmd->u.roamCmd.roamId,
							eCSR_ROAM_LOSTLINK,
						       eCSR_ROAM_RESULT_FORCED);
		}
		break;
	default:
		csr_roam_state_change(mac_ctx,
			eCSR_ROAMING_STATE_IDLE, session_id);
		break;
	}
	qdf_mem_free(roam_info);
}

/**
 * csr_roam_process_start_bss_success() - Process the result for start bss
 * @mac_ctx:          Global MAC Context
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Return: None
 */
static void csr_roam_process_start_bss_success(tpAniSirGlobal mac_ctx,
		     tSmeCmd *cmd, void *context)
{
	uint32_t session_id = cmd->sessionId;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_session *session;
	tSirBssDescription *bss_desc = NULL;
	struct csr_roam_info *roam_info;
	tSirSmeStartBssRsp *start_bss_rsp = NULL;
	eRoamCmdStatus roam_status;
	eCsrRoamResult roam_result;
	tDot11fBeaconIEs *ies_ptr = NULL;
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	QDF_STATUS status;
	host_log_ibss_pkt_type *ibss_log;
	uint32_t bi;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	tSirSmeHTProfile *src_profile = NULL;
	tCsrRoamHTProfile *dst_profile = NULL;
#endif

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	/*
	 * on the StartBss Response, LIM is returning the Bss Description that
	 * we are beaconing.  Add this Bss Description to our scan results and
	 * chain the Profile to this Bss Description.  On a Start BSS, there was
	 * no detected Bss description (no partner) so we issued the Start Bss
	 * to start the Ibss without any Bss description.  Lim was kind enough
	 * to return the Bss Description that we start beaconing for the newly
	 * started Ibss.
	 */
	sme_debug("receives start BSS ok indication");
	status = QDF_STATUS_E_FAILURE;
	start_bss_rsp = (tSirSmeStartBssRsp *) context;
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	if (CSR_IS_IBSS(profile))
		session->connectState = eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED;
	else if (CSR_IS_INFRA_AP(profile))
		session->connectState =
			eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
	else if (CSR_IS_NDI(profile))
		session->connectState = eCSR_CONNECT_STATE_TYPE_NDI_STARTED;
	else
		session->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;

	bss_desc = &start_bss_rsp->bssDescription;
	if (CSR_IS_NDI(profile)) {
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
			session_id);
		csr_roam_save_ndi_connected_info(mac_ctx, session_id, profile,
						bss_desc);
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
			     sizeof(struct qdf_mac_addr));
	} else {
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
				session_id);
		if (!QDF_IS_STATUS_SUCCESS
			(csr_get_parsed_bss_description_ies(mac_ctx, bss_desc,
							    &ies_ptr))) {
			sme_warn("cannot parse IBSS IEs");
			roam_info->pBssDesc = bss_desc;
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_IBSS_IND,
					    eCSR_ROAM_RESULT_IBSS_START_FAILED);
			qdf_mem_free(roam_info);
			return;
		}
	}
	if (!CSR_IS_INFRA_AP(profile) && !CSR_IS_NDI(profile)) {
		csr_scan_append_bss_description(mac_ctx, bss_desc);
	}
	csr_roam_save_connected_bss_desc(mac_ctx, session_id, bss_desc);
	csr_roam_free_connect_profile(&session->connectedProfile);
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	csr_roam_save_connected_information(mac_ctx, session_id,
			profile, bss_desc, ies_ptr);
	qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
		     sizeof(struct qdf_mac_addr));
	/* We are done with the IEs so free it */
	qdf_mem_free(ies_ptr);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	WLAN_HOST_DIAG_LOG_ALLOC(ibss_log,
		host_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
	if (ibss_log) {
		if (CSR_INVALID_SCANRESULT_HANDLE ==
				cmd->u.roamCmd.hBSSList) {
			/*
			 * We start the IBSS (didn't find any
			 * matched IBSS out there)
			 */
			ibss_log->eventId =
				WLAN_IBSS_EVENT_START_IBSS_RSP;
		} else {
			ibss_log->eventId =
				WLAN_IBSS_EVENT_JOIN_IBSS_RSP;
		}
		if (bss_desc) {
			qdf_mem_copy(ibss_log->bssid.bytes,
				bss_desc->bssId, QDF_MAC_ADDR_SIZE);
			ibss_log->operatingChannel =
				bss_desc->channelId;
		}
		if (QDF_IS_STATUS_SUCCESS(wlan_cfg_get_int(
					mac_ctx,
					WNI_CFG_BEACON_INTERVAL,
					&bi)))
			/* U8 is not enough for BI */
			ibss_log->beaconInterval = (uint8_t) bi;
		WLAN_HOST_DIAG_LOG_REPORT(ibss_log);
	}
#endif
	/*
	 * Only set context for non-WDS_STA. We don't even need it for
	 * WDS_AP. But since the encryption.
	 * is WPA2-PSK so it won't matter.
	 */
	if (session->pCurRoamProfile &&
	    !CSR_IS_INFRA_AP(session->pCurRoamProfile)) {
		if (CSR_IS_ENC_TYPE_STATIC(
				profile->negotiatedUCEncryptionType)) {
			/*
			 * Issue the set Context request to LIM to establish
			 * the Broadcast STA context for the Ibss. In Rome IBSS
			 * case, dummy key installation will break proper BSS
			 * key installation, so skip it.
			 */
			if (!CSR_IS_IBSS(session->pCurRoamProfile)) {
				/* NO keys. these key parameters don't matter */
				csr_roam_issue_set_context_req(mac_ctx,
					session_id,
					profile->negotiatedMCEncryptionType,
					bss_desc, &bcast_mac, false,
					false, eSIR_TX_RX, 0, 0, NULL, 0);
			}
		}
		if (CSR_IS_IBSS(session->pCurRoamProfile) &&
		    (eCSR_ENCRYPT_TYPE_WEP40_STATICKEY ==
				profile->negotiatedUCEncryptionType ||
		    eCSR_ENCRYPT_TYPE_WEP104_STATICKEY ==
				profile->negotiatedUCEncryptionType ||
		    eCSR_ENCRYPT_TYPE_TKIP ==
				profile->negotiatedUCEncryptionType ||
		    eCSR_ENCRYPT_TYPE_AES ==
				profile->negotiatedUCEncryptionType)) {
			roam_info->fAuthRequired = true;
		}
	}
	/*
	 * Only tell upper layer is we start the BSS because Vista doesn't like
	 * multiple connection indications. If we don't start the BSS ourself,
	 * handler of eSIR_SME_JOINED_NEW_BSS will trigger the connection start
	 * indication in Vista
	 */
	if (!CSR_IS_JOIN_TO_IBSS(profile)) {
		roam_status = eCSR_ROAM_IBSS_IND;
		roam_result = eCSR_ROAM_RESULT_IBSS_STARTED;
		if (CSR_IS_INFRA_AP(profile)) {
			roam_status = eCSR_ROAM_INFRA_IND;
			roam_result = eCSR_ROAM_RESULT_INFRA_STARTED;
		}
		roam_info->staId = (uint8_t)start_bss_rsp->staId;
		if (CSR_IS_NDI(profile)) {
			csr_roam_update_ndp_return_params(mac_ctx,
							  eCsrStartBssSuccess,
							  &roam_status,
							  &roam_result,
							  roam_info);
		}
		/*
		 * Only tell upper layer is we start the BSS because Vista
		 * doesn't like multiple connection indications. If we don't
		 * start the BSS ourself, handler of eSIR_SME_JOINED_NEW_BSS
		 * will trigger the connection start indication in Vista
		 */
		roam_info->statusCode = session->joinFailStatusCode.statusCode;
		roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
		/* We start the IBSS (didn't find any matched IBSS out there) */
		roam_info->pBssDesc = bss_desc;
		if (bss_desc)
			qdf_mem_copy(roam_info->bssid.bytes, bss_desc->bssId,
				     sizeof(struct qdf_mac_addr));
		if (!IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) &&
				(csr_is_concurrent_session_running(mac_ctx))) {
			mac_ctx->roam.configParam.doBMPSWorkaround = 1;
		}
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		dst_profile = &session->connectedProfile.HTProfile;
		src_profile = &start_bss_rsp->HTProfile;
		if (mac_ctx->roam.configParam.cc_switch_mode
				!= QDF_MCC_TO_SCC_SWITCH_DISABLE)
			csr_roam_copy_ht_profile(dst_profile, src_profile);
#endif
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId,
				       roam_status, roam_result);
	}
	qdf_mem_free(roam_info);
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * populate_fils_params_join_rsp() - Copy FILS params from JOIN rsp
 * @mac_ctx: Global MAC Context
 * @roam_info: CSR Roam Info
 * @join_rsp: SME Join response
 *
 * Copy the FILS params from the join results
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS populate_fils_params_join_rsp(tpAniSirGlobal mac_ctx,
						struct csr_roam_info *roam_info,
						tSirSmeJoinRsp *join_rsp)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct fils_join_rsp_params *roam_fils_info,
				    *fils_join_rsp = join_rsp->fils_join_rsp;

	if (!fils_join_rsp->fils_pmk_len ||
			!fils_join_rsp->fils_pmk || !fils_join_rsp->tk_len ||
			!fils_join_rsp->kek_len || !fils_join_rsp->gtk_len) {
		sme_err("fils join rsp err: pmk len %d tk len %d kek len %d gtk len %d",
			fils_join_rsp->fils_pmk_len,
			fils_join_rsp->tk_len,
			fils_join_rsp->kek_len,
			fils_join_rsp->gtk_len);
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_info->fils_join_rsp = qdf_mem_malloc(sizeof(*fils_join_rsp));
	if (!roam_info->fils_join_rsp) {
		sme_err("fils_join_rsp malloc fails!");
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_fils_info = roam_info->fils_join_rsp;
	roam_fils_info->fils_pmk = qdf_mem_malloc(fils_join_rsp->fils_pmk_len);
	if (!roam_fils_info->fils_pmk) {
		qdf_mem_free(roam_info->fils_join_rsp);
		roam_info->fils_join_rsp = NULL;
		sme_err("fils_pmk malloc fails!");
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_info->fils_seq_num = join_rsp->fils_seq_num;
	roam_fils_info->fils_pmk_len = fils_join_rsp->fils_pmk_len;
	qdf_mem_copy(roam_fils_info->fils_pmk,
		     fils_join_rsp->fils_pmk, roam_fils_info->fils_pmk_len);

	qdf_mem_copy(roam_fils_info->fils_pmkid,
		     fils_join_rsp->fils_pmkid, PMKID_LEN);

	roam_fils_info->kek_len = fils_join_rsp->kek_len;
	qdf_mem_copy(roam_fils_info->kek,
		     fils_join_rsp->kek, roam_fils_info->kek_len);

	roam_fils_info->tk_len = fils_join_rsp->tk_len;
	qdf_mem_copy(roam_fils_info->tk,
		     fils_join_rsp->tk, fils_join_rsp->tk_len);

	roam_fils_info->gtk_len = fils_join_rsp->gtk_len;
	qdf_mem_copy(roam_fils_info->gtk,
		     fils_join_rsp->gtk, roam_fils_info->gtk_len);

	cds_copy_hlp_info(&fils_join_rsp->dst_mac, &fils_join_rsp->src_mac,
			  fils_join_rsp->hlp_data_len, fils_join_rsp->hlp_data,
			  &roam_fils_info->dst_mac, &roam_fils_info->src_mac,
			  &roam_fils_info->hlp_data_len,
			  roam_fils_info->hlp_data);
	sme_debug("FILS connect params copied to CSR!");

free_fils_join_rsp:
	qdf_mem_free(fils_join_rsp->fils_pmk);
	qdf_mem_free(fils_join_rsp);
	return status;
}

/**
 * csr_process_fils_join_rsp() - Process join rsp for FILS connection
 * @mac_ctx: Global MAC Context
 * @profile: CSR Roam Profile
 * @session_id: Session ID
 * @roam_info: CSR Roam Info
 * @bss_desc: BSS description
 * @join_rsp: SME Join rsp
 *
 * Process SME join response for FILS connection
 *
 * Return: None
 */
static void csr_process_fils_join_rsp(tpAniSirGlobal mac_ctx,
					struct csr_roam_profile *profile,
					uint32_t session_id,
					struct csr_roam_info *roam_info,
					tSirBssDescription *bss_desc,
					tSirSmeJoinRsp *join_rsp)
{
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	QDF_STATUS status;

	if (!join_rsp || !join_rsp->fils_join_rsp) {
		sme_err("Join rsp doesn't have FILS info");
		goto process_fils_join_rsp_fail;
	}

	/* Copy FILS params */
	status = populate_fils_params_join_rsp(mac_ctx, roam_info, join_rsp);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Copy FILS params join rsp fails");
		goto process_fils_join_rsp_fail;
	}

	status = csr_roam_issue_set_context_req(mac_ctx, session_id,
					profile->negotiatedUCEncryptionType,
					bss_desc, &(bss_desc->bssId), true,
					true, eSIR_TX_RX, 0,
					roam_info->fils_join_rsp->tk_len,
					roam_info->fils_join_rsp->tk, 0);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Set context for unicast fail");
		goto process_fils_join_rsp_fail;
	}

	status = csr_roam_issue_set_context_req(mac_ctx, session_id,
					profile->negotiatedMCEncryptionType,
					bss_desc, &bcast_mac, true, false,
					eSIR_RX_ONLY, 2,
					roam_info->fils_join_rsp->gtk_len,
					roam_info->fils_join_rsp->gtk, 0);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Set context for bcast fail");
		goto process_fils_join_rsp_fail;
	}

	return;

process_fils_join_rsp_fail:
	csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE, session_id);
}
#else

static inline void csr_process_fils_join_rsp(tpAniSirGlobal mac_ctx,
					     struct csr_roam_profile *profile,
					     uint32_t session_id,
					     struct csr_roam_info *roam_info,
					     tSirBssDescription *bss_desc,
					     tSirSmeJoinRsp *join_rsp)
{}
#endif

/**
 * csr_roam_process_join_res() - Process the Join results
 * @mac_ctx:          Global MAC Context
 * @result:           Result after the command was processed
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Process the join results which are obtained in a successful join
 *
 * Return: None
 */
static void csr_roam_process_join_res(tpAniSirGlobal mac_ctx,
	enum csr_roamcomplete_result res, tSmeCmd *cmd, void *context)
{
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	sme_QosAssocInfo assoc_info;
	uint32_t key_timeout_interval = 0;
	uint8_t acm_mask = 0;   /* HDD needs ACM mask in assoc rsp callback */
	uint32_t session_id = cmd->sessionId;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_session *session;
	tSirBssDescription *bss_desc = NULL;
	struct tag_csrscan_result *scan_res = NULL;
	sme_qos_csr_event_indType ind_qos;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	tSirSmeHTProfile *src_profile = NULL;
	tCsrRoamHTProfile *dst_profile = NULL;
#endif
	tCsrRoamConnectedProfile *conn_profile = NULL;
	tDot11fBeaconIEs *ies_ptr = NULL;
	struct csr_roam_info *roam_info;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	tSirSmeJoinRsp *join_rsp = (tSirSmeJoinRsp *) context;
	uint32_t len;

	if (!join_rsp) {
		sme_err("join_rsp is NULL");
		return;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	conn_profile = &session->connectedProfile;
	sme_debug("receives association indication");
	/* always free the memory here */
	if (session->pWpaRsnRspIE) {
		session->nWpaRsnRspIeLength = 0;
		qdf_mem_free(session->pWpaRsnRspIE);
		session->pWpaRsnRspIE = NULL;
	}
#ifdef FEATURE_WLAN_WAPI
	if (session->pWapiRspIE) {
		session->nWapiRspIeLength = 0;
		qdf_mem_free(session->pWapiRspIE);
		session->pWapiRspIE = NULL;
	}
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_BTAMP_UT_RF
	session->maxRetryCount = 0;
	csr_roam_stop_join_retry_timer(mac_ctx, session_id);
#endif

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	if (eCsrReassocSuccess == res) {
		roam_info->reassoc = true;
		ind_qos = SME_QOS_CSR_REASSOC_COMPLETE;
	} else {
		roam_info->reassoc = false;
		ind_qos = SME_QOS_CSR_ASSOC_COMPLETE;
	}

	/*
	 * Reset remain_in_power_active_till_dhcp as
	 * it might have been set by last failed secured connection.
	 * It should be set only for secured connection.
	 */
	ps_global_info->remain_in_power_active_till_dhcp = false;
	if (CSR_IS_INFRASTRUCTURE(profile))
		session->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;
	else
		session->connectState = eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED;
	/*
	 * Use the last connected bssdesc for reassoc-ing to the same AP.
	 * NOTE: What to do when reassoc to a different AP???
	 */
	if ((eCsrHddIssuedReassocToSameAP == cmd->u.roamCmd.roamReason)
		|| (eCsrSmeIssuedReassocToSameAP ==
			cmd->u.roamCmd.roamReason)) {
		bss_desc = session->pConnectBssDesc;
		if (bss_desc)
			qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
				     sizeof(struct qdf_mac_addr));
	} else {
		if (cmd->u.roamCmd.pRoamBssEntry) {
			scan_res = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
					struct tag_csrscan_result, Link);
			if (scan_res != NULL) {
				bss_desc = &scan_res->Result.BssDescriptor;
				ies_ptr = (tDot11fBeaconIEs *)
					(scan_res->Result.pvIes);
				qdf_mem_copy(&roam_info->bssid,
					     &bss_desc->bssId,
					     sizeof(struct qdf_mac_addr));
			}
		}
	}
	if (bss_desc) {
		roam_info->staId = STA_INVALID_IDX;
		csr_roam_save_connected_information(mac_ctx, session_id,
			profile, bss_desc, ies_ptr);
		/* Save WPA/RSN IE */
		csr_roam_save_security_rsp_ie(mac_ctx, session_id,
			profile->negotiatedAuthType, bss_desc, ies_ptr);
#ifdef FEATURE_WLAN_ESE
		roam_info->isESEAssoc = conn_profile->isESEAssoc;
#endif

		/*
		 * csr_roam_state_change also affects sub-state.
		 * Hence, csr_roam_state_change happens first and then
		 * substate change.
		 * Moving even save profile above so that below
		 * mentioned conditon is also met.
		 * JEZ100225: Moved to after saving the profile.
		 * Fix needed in main/latest
		 */
		csr_roam_state_change(mac_ctx,
			eCSR_ROAMING_STATE_JOINED, session_id);

		/*
		 * Make sure the Set Context is issued before link
		 * indication to NDIS.  After link indication is
		 * made to NDIS, frames could start flowing.
		 * If we have not set context with LIM, the frames
		 * will be dropped for the security context may not
		 * be set properly.
		 *
		 * this was causing issues in the 2c_wlan_wep WHQL test
		 * when the SetContext was issued after the link
		 * indication. (Link Indication happens in the
		 * profFSMSetConnectedInfra call).
		 *
		 * this reordering was done on titan_prod_usb branch
		 * and is being replicated here.
		 */

		if (CSR_IS_ENC_TYPE_STATIC
			(profile->negotiatedUCEncryptionType) &&
			!profile->bWPSAssociation) {
			/*
			 * Issue the set Context request to LIM to establish
			 * the Unicast STA context
			 */
			if (!QDF_IS_STATUS_SUCCESS(
				csr_roam_issue_set_context_req(mac_ctx,
					session_id,
					profile->negotiatedUCEncryptionType,
					bss_desc, &(bss_desc->bssId),
					false, true,
					eSIR_TX_RX, 0, 0, NULL, 0))) {
				/* NO keys. these key parameters don't matter */
				sme_err("Set context for unicast fail");
				csr_roam_substate_change(mac_ctx,
					eCSR_ROAM_SUBSTATE_NONE, session_id);
			}
			/*
			 * Issue the set Context request to LIM
			 * to establish the Broadcast STA context
			 * NO keys. these key parameters don't matter
			 */
			csr_roam_issue_set_context_req(mac_ctx, session_id,
				profile->negotiatedMCEncryptionType,
				bss_desc, &bcast_mac, false, false,
				eSIR_TX_RX, 0, 0, NULL, 0);
		} else if (CSR_IS_AUTH_TYPE_FILS(profile->negotiatedAuthType)
				&& join_rsp->is_fils_connection) {
			roam_info->is_fils_connection = true;
			csr_process_fils_join_rsp(mac_ctx, profile, session_id,
						  roam_info, bss_desc,
						  join_rsp);
		} else {
			/* Need to wait for supplicant authtication */
			roam_info->fAuthRequired = true;
			/*
			 * Set the substate to WaitForKey in case
			 * authentiation is needed
			 */
			csr_roam_substate_change(mac_ctx,
					eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY,
					session_id);

			/*
			 * Set remain_in_power_active_till_dhcp to make
			 * sure we wait for until keys are set before
			 * going into BMPS.
			 */
			ps_global_info->remain_in_power_active_till_dhcp
				= true;

			if (profile->bWPSAssociation)
				key_timeout_interval =
					CSR_WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD;
			else
				key_timeout_interval =
					CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD;

			/* Save session_id in case of timeout */
			mac_ctx->roam.WaitForKeyTimerInfo.vdev_id =
				(uint8_t) session_id;
			/*
			 * This time should be long enough for the rest
			 * of the process plus setting key
			 */
			if (!QDF_IS_STATUS_SUCCESS
					(csr_roam_start_wait_for_key_timer(
					   mac_ctx, key_timeout_interval))
			   ) {
				/* Reset state so nothing is blocked. */
				sme_err("Failed preauth timer start");
				csr_roam_substate_change(mac_ctx,
						eCSR_ROAM_SUBSTATE_NONE,
						session_id);
			}
		}

		assoc_info.pBssDesc = bss_desc;       /* could be NULL */
		assoc_info.pProfile = profile;
		if (context) {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
			if (session->roam_synch_in_progress)
				QDF_TRACE(QDF_MODULE_ID_SME,
					QDF_TRACE_LEVEL_DEBUG,
					FL("LFR3:Clear Connected info"));
#endif
			csr_roam_free_connected_info(mac_ctx,
				&session->connectedInfo);
			len = join_rsp->assocReqLength +
				join_rsp->assocRspLength +
				join_rsp->beaconLength;
			len += join_rsp->parsedRicRspLen;
#ifdef FEATURE_WLAN_ESE
			len += join_rsp->tspecIeLen;
#endif
			if (len) {
				session->connectedInfo.pbFrames =
					qdf_mem_malloc(len);
				if (session->connectedInfo.pbFrames !=
						NULL) {
					qdf_mem_copy(
						session->connectedInfo.pbFrames,
						join_rsp->frames, len);
					session->connectedInfo.nAssocReqLength =
						join_rsp->assocReqLength;
					session->connectedInfo.nAssocRspLength =
						join_rsp->assocRspLength;
					session->connectedInfo.nBeaconLength =
						join_rsp->beaconLength;
					session->connectedInfo.nRICRspLength =
						join_rsp->parsedRicRspLen;
#ifdef FEATURE_WLAN_ESE
					session->connectedInfo.nTspecIeLength =
						join_rsp->tspecIeLen;
#endif
					roam_info->nAssocReqLength =
						join_rsp->assocReqLength;
					roam_info->nAssocRspLength =
						join_rsp->assocRspLength;
					roam_info->nBeaconLength =
						join_rsp->beaconLength;
					roam_info->pbFrames =
						session->connectedInfo.pbFrames;
				}
			}
			if (cmd->u.roamCmd.fReassoc)
				roam_info->fReassocReq =
					roam_info->fReassocRsp = true;
			conn_profile->vht_channel_width =
				join_rsp->vht_channel_width;
			session->connectedInfo.staId =
				(uint8_t)join_rsp->staId;
			roam_info->staId = (uint8_t)join_rsp->staId;
			roam_info->timingMeasCap = join_rsp->timingMeasCap;
			roam_info->chan_info.nss = join_rsp->nss;
			roam_info->chan_info.rate_flags =
				join_rsp->max_rate_flags;
			roam_info->chan_info.ch_width =
				join_rsp->vht_channel_width;
#ifdef FEATURE_WLAN_TDLS
			roam_info->tdls_prohibited = join_rsp->tdls_prohibited;
			roam_info->tdls_chan_swit_prohibited =
				join_rsp->tdls_chan_swit_prohibited;
			sme_debug("tdls:prohibit: %d chan_swit_prohibit: %d",
				roam_info->tdls_prohibited,
				roam_info->tdls_chan_swit_prohibited);
#endif
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
			src_profile = &join_rsp->HTProfile;
			dst_profile = &conn_profile->HTProfile;
			if (mac_ctx->roam.configParam.cc_switch_mode
				!= QDF_MCC_TO_SCC_SWITCH_DISABLE)
				csr_roam_copy_ht_profile(dst_profile,
						src_profile);
#endif
			roam_info->vht_caps = join_rsp->vht_caps;
			roam_info->ht_caps = join_rsp->ht_caps;
			roam_info->hs20vendor_ie = join_rsp->hs20vendor_ie;
			roam_info->ht_operation = join_rsp->ht_operation;
			roam_info->vht_operation = join_rsp->vht_operation;
		} else {
			if (cmd->u.roamCmd.fReassoc) {
				roam_info->fReassocReq =
					roam_info->fReassocRsp = true;
				roam_info->nAssocReqLength =
					session->connectedInfo.nAssocReqLength;
				roam_info->nAssocRspLength =
					session->connectedInfo.nAssocRspLength;
				roam_info->nBeaconLength =
					session->connectedInfo.nBeaconLength;
				roam_info->pbFrames =
					session->connectedInfo.pbFrames;
			}
		}
		/*
		 * Update the staId from the previous connected profile info
		 * as the reassociation is triggred at SME/HDD
		 */

		if ((eCsrHddIssuedReassocToSameAP ==
				cmd->u.roamCmd.roamReason) ||
			(eCsrSmeIssuedReassocToSameAP ==
				cmd->u.roamCmd.roamReason))
			roam_info->staId = session->connectedInfo.staId;

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		/*
		 * Indicate SME-QOS with reassoc success event,
		 * only after copying the frames
		 */
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id, ind_qos,
				&assoc_info);
#endif
		roam_info->pBssDesc = bss_desc;
		roam_info->statusCode =
			session->joinFailStatusCode.statusCode;
		roam_info->reasonCode =
			session->joinFailStatusCode.reasonCode;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		acm_mask = sme_qos_get_acm_mask(mac_ctx, bss_desc, NULL);
#endif
		conn_profile->acm_mask = acm_mask;
		conn_profile->modifyProfileFields.uapsd_mask =
						join_rsp->uapsd_mask;
		/*
		 * start UAPSD if uapsd_mask is not 0 because HDD will
		 * configure for trigger frame It may be better to let QoS do
		 * this????
		 */
		if (conn_profile->modifyProfileFields.uapsd_mask) {
			sme_err(
				" uapsd_mask (0x%X) set, request UAPSD now",
				conn_profile->modifyProfileFields.uapsd_mask);
			sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), session_id);
		}
		conn_profile->dot11Mode = session->bssParams.uCfgDot11Mode;
		roam_info->u.pConnectedProfile = conn_profile;

		if (session->bRefAssocStartCnt > 0) {
			session->bRefAssocStartCnt--;
			if (!IS_FEATURE_SUPPORTED_BY_FW
				(SLM_SESSIONIZATION) &&
				(csr_is_concurrent_session_running(mac_ctx))) {
				mac_ctx->roam.configParam.doBMPSWorkaround = 1;
			}
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_ASSOCIATED);
		}

		csr_update_scan_entry_associnfo(mac_ctx, session,
						SCAN_ENTRY_CON_STATE_ASSOC);
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
				eCSR_ROAM_RESULT_NONE, true);
		csr_reset_pmkid_candidate_list(mac_ctx, session_id);
#ifdef FEATURE_WLAN_WAPI
		csr_reset_bkid_candidate_list(mac_ctx, session_id);
#endif
	} else {
		sme_warn("Roam command doesn't have a BSS desc");
	}
	/* Not to signal link up because keys are yet to be set.
	 * The linkup function will overwrite the sub-state that
	 * we need to keep at this point.
	 */
	if (!CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id)) {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		if (session->roam_synch_in_progress) {
			QDF_TRACE(QDF_MODULE_ID_SME,
				QDF_TRACE_LEVEL_DEBUG,
				FL
				("NO CSR_IS_WAIT_FOR_KEY -> csr_roam_link_up"));
		}
#endif
		csr_roam_link_up(mac_ctx, conn_profile->bssid);
	}
	sme_free_join_rsp_fils_params(roam_info);
	qdf_mem_free(roam_info);
}

/**
 * csr_roam_process_results() - Process the Roam Results
 * @mac_ctx:      Global MAC Context
 * @cmd:          Command that has been processed
 * @res:          Results available after processing the command
 * @context:      Context
 *
 * Process the available results and make an appropriate decision
 *
 * Return: true if the command can be released, else not.
 */
static bool csr_roam_process_results(tpAniSirGlobal mac_ctx, tSmeCmd *cmd,
				     enum csr_roamcomplete_result res,
					void *context)
{
	bool release_cmd = true;
	tSirBssDescription *bss_desc = NULL;
	struct csr_roam_info *roam_info;
	uint32_t session_id = cmd->sessionId;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	eRoamCmdStatus roam_status;
	eCsrRoamResult roam_result;
	host_log_ibss_pkt_type *ibss_log;
	tSirSmeStartBssRsp  *start_bss_rsp = NULL;

	if (!session) {
		sme_err("session %d not found ", session_id);
		return false;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return false;

	sme_debug("Result %d", res);
	switch (res) {
	case eCsrJoinSuccess:
	case eCsrReassocSuccess:
		csr_roam_process_join_res(mac_ctx, res, cmd, context);
		break;
	case eCsrStartBssSuccess:
		csr_roam_process_start_bss_success(mac_ctx, cmd, context);
		break;
	case eCsrStartBssFailure:
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
		WLAN_HOST_DIAG_LOG_ALLOC(ibss_log,
			host_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
		if (ibss_log) {
			ibss_log->status = WLAN_IBSS_STATUS_FAILURE;
			WLAN_HOST_DIAG_LOG_REPORT(ibss_log);
		}
#endif
		start_bss_rsp = (tSirSmeStartBssRsp *)context;
		roam_status = eCSR_ROAM_IBSS_IND;
		roam_result = eCSR_ROAM_RESULT_IBSS_STARTED;
		if (CSR_IS_INFRA_AP(profile)) {
			roam_status = eCSR_ROAM_INFRA_IND;
			roam_result = eCSR_ROAM_RESULT_INFRA_START_FAILED;
		}
		if (CSR_IS_NDI(profile)) {
			csr_roam_update_ndp_return_params(mac_ctx,
							  eCsrStartBssFailure,
							  &roam_status,
							  &roam_result,
							  roam_info);
		}

		if (context)
			bss_desc = (tSirBssDescription *) context;
		else
			bss_desc = NULL;
		roam_info->pBssDesc = bss_desc;
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId, roam_status,
				       roam_result);
		csr_set_default_dot11_mode(mac_ctx);
		break;
	case eCsrSilentlyStopRoaming:
		/*
		 * We are here because we try to start the same IBSS.
		 * No message to PE. return the roaming state to Joined.
		 */
		sme_debug("receives silently stop roam ind");
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
			session_id);
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
			session_id);
		qdf_mem_zero(roam_info, sizeof(*roam_info));
		roam_info->pBssDesc = session->pConnectBssDesc;
		if (roam_info->pBssDesc)
			qdf_mem_copy(&roam_info->bssid,
				     &roam_info->pBssDesc->bssId,
				     sizeof(struct qdf_mac_addr));
		/*
		 * Since there is no change in the current state, simply pass
		 * back no result otherwise HDD may be mistakenly mark to
		 * disconnected state.
		 */
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId,
				       eCSR_ROAM_IBSS_IND,
				       eCSR_ROAM_RESULT_NONE);
		break;
	case eCsrSilentlyStopRoamingSaveState:
		/* We are here because we try to connect to the same AP */
		/* No message to PE */
		sme_debug("receives silently stop roaming indication");
		qdf_mem_zero(roam_info, sizeof(*roam_info));

		/* to aviod resetting the substate to NONE */
		mac_ctx->roam.curState[session_id] = eCSR_ROAMING_STATE_JOINED;
		/*
		 * No need to change substate to wai_for_key because there
		 * is no state change
		 */
		roam_info->pBssDesc = session->pConnectBssDesc;
		if (roam_info->pBssDesc)
			qdf_mem_copy(&roam_info->bssid,
				     &roam_info->pBssDesc->bssId,
				     sizeof(struct qdf_mac_addr));
		roam_info->statusCode = session->joinFailStatusCode.statusCode;
		roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
		roam_info->nBeaconLength = session->connectedInfo.nBeaconLength;
		roam_info->nAssocReqLength =
			session->connectedInfo.nAssocReqLength;
		roam_info->nAssocRspLength =
			session->connectedInfo.nAssocRspLength;
		roam_info->pbFrames = session->connectedInfo.pbFrames;
		roam_info->staId = session->connectedInfo.staId;
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		if (0 == roam_info->staId)
			QDF_ASSERT(0);

		session->bRefAssocStartCnt--;
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId,
				       eCSR_ROAM_ASSOCIATION_COMPLETION,
				       eCSR_ROAM_RESULT_ASSOCIATED);
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
				eCSR_ROAM_RESULT_ASSOCIATED, true);
		break;
	case eCsrReassocFailure:
		/*
		 * Currently Reassoc failure is handled through eCsrJoinFailure
		 * Need to revisit for eCsrReassocFailure handling
		 */
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
				SME_QOS_CSR_REASSOC_FAILURE, NULL);
#endif
		break;
	case eCsrStopBssSuccess:
		if (CSR_IS_NDI(profile)) {
			qdf_mem_zero(roam_info, sizeof(*roam_info));
			csr_roam_update_ndp_return_params(mac_ctx, res,
							  &roam_status,
							  &roam_result,
							  roam_info);
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       roam_status, roam_result);
		}
		break;
	case eCsrStopBssFailure:
		if (CSR_IS_NDI(profile)) {
			qdf_mem_zero(roam_info, sizeof(*roam_info));
			csr_roam_update_ndp_return_params(mac_ctx, res,
							  &roam_status,
							  &roam_result,
							  roam_info);
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       roam_status, roam_result);
		}
		break;
	case eCsrJoinFailure:
	case eCsrNothingToJoin:
	case eCsrJoinFailureDueToConcurrency:
	default:
		csr_roam_process_results_default(mac_ctx, cmd, context, res);
		break;
	}
	qdf_mem_free(roam_info);
	return release_cmd;
}

#ifdef WLAN_FEATURE_FILS_SK
/*
 * update_profile_fils_info: API to update FILS info from
 * source profile to destination profile.
 * @des_profile: pointer to destination profile
 * @src_profile: pointer to souce profile
 *
 * Return: None
 */
static void update_profile_fils_info(struct csr_roam_profile *des_profile,
				     struct csr_roam_profile *src_profile)
{
	if (!src_profile || !src_profile->fils_con_info)
		return;

	sme_debug("is fils %d", src_profile->fils_con_info->is_fils_connection);

	if (!src_profile->fils_con_info->is_fils_connection)
		return;

	des_profile->fils_con_info =
		qdf_mem_malloc(sizeof(struct cds_fils_connection_info));
	if (!des_profile->fils_con_info) {
		sme_err("failed to allocate memory");
		return;
	}

	qdf_mem_copy(des_profile->fils_con_info,
			src_profile->fils_con_info,
			sizeof(struct cds_fils_connection_info));

	des_profile->hlp_ie =
		qdf_mem_malloc(src_profile->hlp_ie_len);
	if (!des_profile->hlp_ie) {
		sme_err("failed to allocate memory for hlp ie");
		return;
	}

	qdf_mem_copy(des_profile->hlp_ie, src_profile->hlp_ie,
		     src_profile->hlp_ie_len);
	des_profile->hlp_ie_len = src_profile->hlp_ie_len;
}
#else
static inline
void update_profile_fils_info(struct csr_roam_profile *des_profile,
			      struct csr_roam_profile *src_profile)
{ }
#endif
QDF_STATUS csr_roam_copy_profile(tpAniSirGlobal pMac,
				 struct csr_roam_profile *pDstProfile,
				 struct csr_roam_profile *pSrcProfile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t size = 0;

	qdf_mem_zero(pDstProfile, sizeof(struct csr_roam_profile));
	if (pSrcProfile->BSSIDs.numOfBSSIDs) {
		size = sizeof(struct qdf_mac_addr) * pSrcProfile->BSSIDs.
								numOfBSSIDs;
		pDstProfile->BSSIDs.bssid = qdf_mem_malloc(size);
		if (NULL == pDstProfile->BSSIDs.bssid) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->BSSIDs.numOfBSSIDs =
			pSrcProfile->BSSIDs.numOfBSSIDs;
		qdf_mem_copy(pDstProfile->BSSIDs.bssid,
			pSrcProfile->BSSIDs.bssid, size);
	}
	if (pSrcProfile->SSIDs.numOfSSIDs) {
		size = sizeof(tCsrSSIDInfo) * pSrcProfile->SSIDs.numOfSSIDs;
		pDstProfile->SSIDs.SSIDList = qdf_mem_malloc(size);
		if (NULL == pDstProfile->SSIDs.SSIDList) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->SSIDs.numOfSSIDs =
			pSrcProfile->SSIDs.numOfSSIDs;
		qdf_mem_copy(pDstProfile->SSIDs.SSIDList,
			pSrcProfile->SSIDs.SSIDList, size);
	}
	if (pSrcProfile->nWPAReqIELength) {
		pDstProfile->pWPAReqIE =
			qdf_mem_malloc(pSrcProfile->nWPAReqIELength);
		if (NULL == pDstProfile->pWPAReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nWPAReqIELength =
			pSrcProfile->nWPAReqIELength;
		qdf_mem_copy(pDstProfile->pWPAReqIE, pSrcProfile->pWPAReqIE,
			pSrcProfile->nWPAReqIELength);
	}
	if (pSrcProfile->nRSNReqIELength) {
		pDstProfile->pRSNReqIE =
			qdf_mem_malloc(pSrcProfile->nRSNReqIELength);
		if (NULL == pDstProfile->pRSNReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nRSNReqIELength =
			pSrcProfile->nRSNReqIELength;
		qdf_mem_copy(pDstProfile->pRSNReqIE, pSrcProfile->pRSNReqIE,
			pSrcProfile->nRSNReqIELength);
	}
#ifdef FEATURE_WLAN_WAPI
	if (pSrcProfile->nWAPIReqIELength) {
		pDstProfile->pWAPIReqIE =
			qdf_mem_malloc(pSrcProfile->nWAPIReqIELength);
		if (NULL == pDstProfile->pWAPIReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nWAPIReqIELength =
			pSrcProfile->nWAPIReqIELength;
		qdf_mem_copy(pDstProfile->pWAPIReqIE, pSrcProfile->pWAPIReqIE,
			pSrcProfile->nWAPIReqIELength);
	}
#endif /* FEATURE_WLAN_WAPI */
	if (pSrcProfile->nAddIEScanLength) {
		pDstProfile->pAddIEScan =
			qdf_mem_malloc(pSrcProfile->nAddIEScanLength);
		if (NULL == pDstProfile->pAddIEScan) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nAddIEScanLength =
			pSrcProfile->nAddIEScanLength;
		qdf_mem_copy(pDstProfile->pAddIEScan, pSrcProfile->pAddIEScan,
			pSrcProfile->nAddIEScanLength);
	}
	if (pSrcProfile->nAddIEAssocLength) {
		pDstProfile->pAddIEAssoc =
			qdf_mem_malloc(pSrcProfile->nAddIEAssocLength);
		if (NULL == pDstProfile->pAddIEAssoc) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nAddIEAssocLength =
			pSrcProfile->nAddIEAssocLength;
		qdf_mem_copy(pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
			pSrcProfile->nAddIEAssocLength);
	}
	if (pSrcProfile->ChannelInfo.ChannelList) {
		pDstProfile->ChannelInfo.ChannelList =
			qdf_mem_malloc(pSrcProfile->ChannelInfo.
					numOfChannels);
		if (NULL == pDstProfile->ChannelInfo.ChannelList) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->ChannelInfo.numOfChannels =
			pSrcProfile->ChannelInfo.numOfChannels;
		qdf_mem_copy(pDstProfile->ChannelInfo.ChannelList,
			pSrcProfile->ChannelInfo.ChannelList,
			pSrcProfile->ChannelInfo.numOfChannels);
	}
	pDstProfile->AuthType = pSrcProfile->AuthType;
	pDstProfile->akm_list = pSrcProfile->akm_list;
	pDstProfile->EncryptionType = pSrcProfile->EncryptionType;
	pDstProfile->mcEncryptionType = pSrcProfile->mcEncryptionType;
	pDstProfile->negotiatedUCEncryptionType =
		pSrcProfile->negotiatedUCEncryptionType;
	pDstProfile->negotiatedMCEncryptionType =
		pSrcProfile->negotiatedMCEncryptionType;
	pDstProfile->negotiatedAuthType = pSrcProfile->negotiatedAuthType;
#ifdef WLAN_FEATURE_11W
	pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif
	pDstProfile->BSSType = pSrcProfile->BSSType;
	pDstProfile->phyMode = pSrcProfile->phyMode;
	pDstProfile->csrPersona = pSrcProfile->csrPersona;

#ifdef FEATURE_WLAN_WAPI
	if (csr_is_profile_wapi(pSrcProfile))
		if (pDstProfile->phyMode & eCSR_DOT11_MODE_11n)
			pDstProfile->phyMode &= ~eCSR_DOT11_MODE_11n;
#endif /* FEATURE_WLAN_WAPI */
	pDstProfile->ch_params.ch_width = pSrcProfile->ch_params.ch_width;
	pDstProfile->ch_params.center_freq_seg0 =
		pSrcProfile->ch_params.center_freq_seg0;
	pDstProfile->ch_params.center_freq_seg1 =
		pSrcProfile->ch_params.center_freq_seg1;
	pDstProfile->ch_params.sec_ch_offset =
		pSrcProfile->ch_params.sec_ch_offset;
	/*Save the WPS info */
	pDstProfile->bWPSAssociation = pSrcProfile->bWPSAssociation;
	pDstProfile->bOSENAssociation = pSrcProfile->bOSENAssociation;
	pDstProfile->force_24ghz_in_ht20 = pSrcProfile->force_24ghz_in_ht20;
	pDstProfile->uapsd_mask = pSrcProfile->uapsd_mask;
	pDstProfile->beaconInterval = pSrcProfile->beaconInterval;
	pDstProfile->privacy = pSrcProfile->privacy;
	pDstProfile->fwdWPSPBCProbeReq = pSrcProfile->fwdWPSPBCProbeReq;
	pDstProfile->csr80211AuthType = pSrcProfile->csr80211AuthType;
	pDstProfile->dtimPeriod = pSrcProfile->dtimPeriod;
	pDstProfile->ApUapsdEnable = pSrcProfile->ApUapsdEnable;
	pDstProfile->SSIDs.SSIDList[0].ssidHidden =
		pSrcProfile->SSIDs.SSIDList[0].ssidHidden;
	pDstProfile->protEnabled = pSrcProfile->protEnabled;
	pDstProfile->obssProtEnabled = pSrcProfile->obssProtEnabled;
	pDstProfile->cfg_protection = pSrcProfile->cfg_protection;
	pDstProfile->wps_state = pSrcProfile->wps_state;
	pDstProfile->ieee80211d = pSrcProfile->ieee80211d;
	pDstProfile->sap_dot11mc = pSrcProfile->sap_dot11mc;
	pDstProfile->supplicant_disabled_roaming =
		pSrcProfile->supplicant_disabled_roaming;
	qdf_mem_copy(&pDstProfile->Keys, &pSrcProfile->Keys,
		sizeof(pDstProfile->Keys));
#ifdef WLAN_FEATURE_11W
	pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif
	if (pSrcProfile->MDID.mdiePresent) {
		pDstProfile->MDID.mdiePresent = 1;
		pDstProfile->MDID.mobilityDomain =
			pSrcProfile->MDID.mobilityDomain;
	}
	qdf_mem_copy(&pDstProfile->addIeParams, &pSrcProfile->addIeParams,
			sizeof(tSirAddIeParams));

	update_profile_fils_info(pDstProfile, pSrcProfile);

	pDstProfile->beacon_tx_rate = pSrcProfile->beacon_tx_rate;

	if (pSrcProfile->supported_rates.numRates) {
		qdf_mem_copy(pDstProfile->supported_rates.rate,
				pSrcProfile->supported_rates.rate,
				pSrcProfile->supported_rates.numRates);
		pDstProfile->supported_rates.numRates =
			pSrcProfile->supported_rates.numRates;
	}
	if (pSrcProfile->extended_rates.numRates) {
		qdf_mem_copy(pDstProfile->extended_rates.rate,
				pSrcProfile->extended_rates.rate,
				pSrcProfile->extended_rates.numRates);
		pDstProfile->extended_rates.numRates =
			pSrcProfile->extended_rates.numRates;
	}
	pDstProfile->cac_duration_ms = pSrcProfile->cac_duration_ms;
	pDstProfile->dfs_regdomain   = pSrcProfile->dfs_regdomain;
	pDstProfile->chan_switch_hostapd_rate_enabled  =
		pSrcProfile->chan_switch_hostapd_rate_enabled;
	pDstProfile->force_rsne_override = pSrcProfile->force_rsne_override;
end:
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		csr_release_profile(pMac, pDstProfile);
		pDstProfile = NULL;
	}

	return status;
}

QDF_STATUS csr_roam_copy_connected_profile(tpAniSirGlobal pMac,
					   uint32_t sessionId,
					   struct csr_roam_profile *pDstProfile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrRoamConnectedProfile *pSrcProfile =
		&pMac->roam.roamSession[sessionId].connectedProfile;

	qdf_mem_zero(pDstProfile, sizeof(struct csr_roam_profile));

	pDstProfile->BSSIDs.bssid = qdf_mem_malloc(sizeof(struct qdf_mac_addr));
	if (NULL == pDstProfile->BSSIDs.bssid) {
		status = QDF_STATUS_E_NOMEM;
		sme_err("failed to allocate memory for BSSID "
			MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(pSrcProfile->bssid.bytes));
		goto end;
	}
	pDstProfile->BSSIDs.numOfBSSIDs = 1;
	qdf_copy_macaddr(pDstProfile->BSSIDs.bssid, &pSrcProfile->bssid);

	if (pSrcProfile->SSID.length > 0) {
		pDstProfile->SSIDs.SSIDList =
			qdf_mem_malloc(sizeof(tCsrSSIDInfo));
		if (NULL == pDstProfile->SSIDs.SSIDList) {
			status = QDF_STATUS_E_NOMEM;
			sme_err("failed to allocate memory for SSID "
				MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(pSrcProfile->bssid.bytes));
			goto end;
		}
		pDstProfile->SSIDs.numOfSSIDs = 1;
		pDstProfile->SSIDs.SSIDList[0].handoffPermitted =
			pSrcProfile->handoffPermitted;
		pDstProfile->SSIDs.SSIDList[0].ssidHidden =
			pSrcProfile->ssidHidden;
		qdf_mem_copy(&pDstProfile->SSIDs.SSIDList[0].SSID,
			&pSrcProfile->SSID, sizeof(tSirMacSSid));
	}
	if (pSrcProfile->nAddIEAssocLength) {
		pDstProfile->pAddIEAssoc =
			qdf_mem_malloc(pSrcProfile->nAddIEAssocLength);
		if (NULL == pDstProfile->pAddIEAssoc) {
			status = QDF_STATUS_E_NOMEM;
			sme_err("failed to allocate mem for additional ie");
			goto end;
		}
		pDstProfile->nAddIEAssocLength = pSrcProfile->nAddIEAssocLength;
		qdf_mem_copy(pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
			pSrcProfile->nAddIEAssocLength);
	}
	pDstProfile->ChannelInfo.ChannelList = qdf_mem_malloc(1);
	if (NULL == pDstProfile->ChannelInfo.ChannelList) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	pDstProfile->ChannelInfo.numOfChannels = 1;
	pDstProfile->ChannelInfo.ChannelList[0] = pSrcProfile->operationChannel;
	pDstProfile->AuthType.numEntries = 1;
	pDstProfile->AuthType.authType[0] = pSrcProfile->AuthType;
	pDstProfile->negotiatedAuthType = pSrcProfile->AuthType;
	pDstProfile->akm_list = pSrcProfile->akm_list;
	pDstProfile->EncryptionType.numEntries = 1;
	pDstProfile->EncryptionType.encryptionType[0] =
		pSrcProfile->EncryptionType;
	pDstProfile->negotiatedUCEncryptionType =
		pSrcProfile->EncryptionType;
	pDstProfile->mcEncryptionType.numEntries = 1;
	pDstProfile->mcEncryptionType.encryptionType[0] =
		pSrcProfile->mcEncryptionType;
	pDstProfile->negotiatedMCEncryptionType =
		pSrcProfile->mcEncryptionType;
	pDstProfile->BSSType = pSrcProfile->BSSType;
	qdf_mem_copy(&pDstProfile->Keys, &pSrcProfile->Keys,
		sizeof(pDstProfile->Keys));
	if (pSrcProfile->MDID.mdiePresent) {
		pDstProfile->MDID.mdiePresent = 1;
		pDstProfile->MDID.mobilityDomain =
			pSrcProfile->MDID.mobilityDomain;
	}
#ifdef WLAN_FEATURE_11W
	pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif

end:
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		csr_release_profile(pMac, pDstProfile);
		pDstProfile = NULL;
	}

	return status;
}

QDF_STATUS csr_roam_issue_connect(tpAniSirGlobal pMac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tScanResultHandle hBSSList,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate, bool fClearScan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(pMac);
	if (NULL == pCommand) {
		csr_scan_result_purge(pMac, hBSSList);
		sme_err(" fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	} else {
		if (fClearScan)
			csr_scan_abort_mac_scan(pMac, sessionId, INVAL_SCAN_ID);

		pCommand->u.roamCmd.fReleaseProfile = false;
		if (NULL == pProfile) {
			/* We can roam now
			 * Since pProfile is NULL, we need to build our own
			 * profile, set everything to default We can only
			 * support open and no encryption
			 */
			pCommand->u.roamCmd.roamProfile.AuthType.numEntries = 1;
			pCommand->u.roamCmd.roamProfile.AuthType.authType[0] =
				eCSR_AUTH_TYPE_OPEN_SYSTEM;
			pCommand->u.roamCmd.roamProfile.EncryptionType.
			numEntries = 1;
			pCommand->u.roamCmd.roamProfile.EncryptionType.
			encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
			pCommand->u.roamCmd.roamProfile.csrPersona =
				QDF_STA_MODE;
		} else {
			/* make a copy of the profile */
			status = csr_roam_copy_profile(pMac, &pCommand->u.
							roamCmd.roamProfile,
						      pProfile);
			if (QDF_IS_STATUS_SUCCESS(status))
				pCommand->u.roamCmd.fReleaseProfile = true;
		}

		pCommand->command = eSmeCommandRoam;
		pCommand->sessionId = (uint8_t) sessionId;
		pCommand->u.roamCmd.hBSSList = hBSSList;
		pCommand->u.roamCmd.roamId = roamId;
		pCommand->u.roamCmd.roamReason = reason;
		/* We need to free the BssList when the command is done */
		pCommand->u.roamCmd.fReleaseBssList = true;
		pCommand->u.roamCmd.fUpdateCurRoamProfile = true;

		status = csr_queue_sme_command(pMac, pCommand, fImediate);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to send message status: %d", status);
		}
	}

	return status;
}

QDF_STATUS csr_roam_issue_reassoc(tpAniSirGlobal pMac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tCsrRoamModifyProfileFields
				*pMmodProfileFields,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(pMac);
	if (NULL == pCommand) {
		sme_err("fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	} else {
		csr_scan_abort_mac_scan(pMac, sessionId, INVAL_SCAN_ID);
		if (pProfile) {
			/* This is likely trying to reassoc to
			 * different profile
			 */
			pCommand->u.roamCmd.fReleaseProfile = false;
			/* make a copy of the profile */
			status = csr_roam_copy_profile(pMac, &pCommand->u.
							roamCmd.roamProfile,
						      pProfile);
			pCommand->u.roamCmd.fUpdateCurRoamProfile = true;
		} else {
			status = csr_roam_copy_connected_profile(pMac,
							sessionId,
							&pCommand->u.roamCmd.
							roamProfile);
			/* how to update WPA/WPA2 info in roamProfile?? */
			pCommand->u.roamCmd.roamProfile.uapsd_mask =
				pMmodProfileFields->uapsd_mask;
		}
		if (QDF_IS_STATUS_SUCCESS(status))
			pCommand->u.roamCmd.fReleaseProfile = true;
		pCommand->command = eSmeCommandRoam;
		pCommand->sessionId = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamId = roamId;
		pCommand->u.roamCmd.roamReason = reason;
		/* We need to free the BssList when the command is done */
		/* For reassoc there is no BSS list, so the bool set to false */
		pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
		pCommand->u.roamCmd.fReleaseBssList = false;
		pCommand->u.roamCmd.fReassoc = true;
		csr_roam_remove_duplicate_command(pMac, sessionId, pCommand,
						  reason);
		status = csr_queue_sme_command(pMac, pCommand, fImediate);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to send message status = %d", status);
			csr_roam_completion(pMac, sessionId, NULL, NULL,
					    eCSR_ROAM_RESULT_FAILURE, false);
		}
	}
	return status;
}

QDF_STATUS csr_dequeue_roam_command(tpAniSirGlobal pMac,
			enum csr_roam_reason reason,
					uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;

	pEntry = csr_nonscan_active_ll_peek_head(pMac, LL_ACCESS_LOCK);

	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if ((eSmeCommandRoam == pCommand->command) &&
		    (eCsrPerformPreauth == reason)) {
			sme_debug("DQ-Command = %d, Reason = %d",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
			if (csr_nonscan_active_ll_remove_entry(pMac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(pMac, pCommand);
			}
		} else if ((eSmeCommandRoam == pCommand->command) &&
			   (eCsrSmeIssuedFTReassoc == reason)) {
			sme_debug("DQ-Command = %d, Reason = %d",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
			if (csr_nonscan_active_ll_remove_entry(pMac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(pMac, pCommand);
			}
		} else {
			sme_err("Command = %d, Reason = %d ",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
		}
	} else {
		sme_err("pEntry NULL for eWNI_SME_FT_PRE_AUTH_RSP");
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * csr_is_fils_connection() - API to check if FILS connection
 * @profile: CSR Roam Profile
 *
 * Return: true, if fils connection, false otherwise
 */
static bool csr_is_fils_connection(struct csr_roam_profile *profile)
{
	if (!profile->fils_con_info)
		return false;

	return profile->fils_con_info->is_fils_connection;
}
#else
static bool csr_is_fils_connection(struct csr_roam_profile *pProfile)
{
	return false;
}
#endif

/**
 * csr_roam_print_candidate_aps() - print all candidate AP in sorted
 * score.
 * @results: scan result
 *
 * Return : void
 */
static void csr_roam_print_candidate_aps(tScanResultHandle results)
{
	tListElem *entry;
	struct tag_csrscan_result *bss_desc = NULL;
	struct scan_result_list *bss_list = NULL;

	if (!results)
		return;
	bss_list = (struct scan_result_list *)results;
	entry = csr_ll_peek_head(&bss_list->List, LL_ACCESS_NOLOCK);
	while (entry) {
		bss_desc = GET_BASE_ADDR(entry,
				struct tag_csrscan_result, Link);
		sme_nofl_debug(QDF_MAC_ADDR_STR " score: %d",
			QDF_MAC_ADDR_ARRAY(bss_desc->Result.BssDescriptor.bssId),
			bss_desc->bss_score);

		entry = csr_ll_next(&bss_list->List, entry,
				LL_ACCESS_NOLOCK);
	}
}

QDF_STATUS csr_roam_connect(tpAniSirGlobal pMac, uint32_t sessionId,
		struct csr_roam_profile *pProfile,
		uint32_t *pRoamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tScanResultHandle hBSSList;
	tCsrScanResultFilter *pScanFilter;
	uint32_t roamId = 0;
	bool fCallCallback = false;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	tSirBssDescription *first_ap_profile;
	uint8_t channel_id = 0;

	if (NULL == pSession) {
		sme_err("session does not exist for given sessionId: %d",
			sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (NULL == pProfile) {
		sme_err("No profile specified");
		return QDF_STATUS_E_FAILURE;
	}

	first_ap_profile = qdf_mem_malloc(sizeof(*first_ap_profile));
	if (NULL == first_ap_profile) {
		sme_err("malloc fails for first_ap_profile");
		return QDF_STATUS_E_NOMEM;
	}

	/* Initialize the count before proceeding with the Join requests */
	pSession->join_bssid_count = 0;
	pSession->discon_in_progress = false;
	pSession->is_fils_connection = csr_is_fils_connection(pProfile);
	sme_debug("Persona %d authtype %d  encryType %d mc_encType %d",
		  pProfile->csrPersona, pProfile->AuthType.authType[0],
		  pProfile->EncryptionType.encryptionType[0],
		  pProfile->mcEncryptionType.encryptionType[0]);
	csr_roam_cancel_roaming(pMac, sessionId);
	csr_scan_abort_mac_scan(pMac, sessionId, INVAL_SCAN_ID);
	csr_roam_remove_duplicate_command(pMac, sessionId, NULL, eCsrHddIssued);
	/* Check whether ssid changes */
	if (csr_is_conn_state_connected(pMac, sessionId) &&
	    pProfile->SSIDs.numOfSSIDs &&
	    !csr_is_ssid_in_list(&pSession->connectedProfile.SSID,
				 &pProfile->SSIDs))
		csr_roam_issue_disassociate_cmd(pMac, sessionId,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					eSIR_MAC_UNSPEC_FAILURE_REASON);
	/*
	 * If roamSession.connectState is disconnecting that mean
	 * disconnect was received with scan for ssid in progress
	 * and dropped. This state will ensure that connect will
	 * not be issued from scan for ssid completion. Thus
	 * if this fresh connect also issue scan for ssid the connect
	 * command will be dropped assuming disconnect is in progress.
	 * Thus reset connectState here
	 */
	if (eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING ==
			pMac->roam.roamSession[sessionId].connectState)
		pMac->roam.roamSession[sessionId].connectState =
			eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
#ifdef FEATURE_WLAN_BTAMP_UT_RF
	pSession->maxRetryCount = CSR_JOIN_MAX_RETRY_COUNT;
#endif
	pScanFilter = qdf_mem_malloc(sizeof(tCsrScanResultFilter));
	if (NULL == pScanFilter) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}

	/* Try to connect to any BSS */
	if (NULL == pProfile) {
		/* No encryption */
		pScanFilter->EncryptionType.numEntries = 1;
		pScanFilter->EncryptionType.encryptionType[0] =
			eCSR_ENCRYPT_TYPE_NONE;
	} else {
		/* Here is the profile we need to connect to */
		status = csr_roam_prepare_filter_from_profile(pMac,
				pProfile, pScanFilter);
	}
	roamId = GET_NEXT_ROAM_ID(&pMac->roam);
	if (pRoamId)
		*pRoamId = roamId;
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(pScanFilter);
		goto end;
	}

	/*Save the WPS info */
	if (NULL != pProfile) {
		pScanFilter->bWPSAssociation =
			pProfile->bWPSAssociation;
		pScanFilter->bOSENAssociation =
			pProfile->bOSENAssociation;
	} else {
		pScanFilter->bWPSAssociation = 0;
		pScanFilter->bOSENAssociation = 0;
	}
	if (pProfile && CSR_IS_INFRA_AP(pProfile)) {
		/* This can be started right away */
		status = csr_roam_issue_connect(pMac, sessionId, pProfile, NULL,
				 eCsrHddIssued, roamId, false, false);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("CSR failed to issue start BSS cmd with status: 0x%08X",
				status);
			fCallCallback = true;
		} else
			sme_debug("Connect request to proceed for sap mode");

		csr_free_scan_filter(pMac, pScanFilter);
		qdf_mem_free(pScanFilter);
		goto end;
	}
	status = csr_scan_get_result(pMac, pScanFilter, &hBSSList);
	csr_roam_print_candidate_aps(hBSSList);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* check if set hw mode needs to be done */
		if ((pScanFilter->csrPersona == QDF_STA_MODE) ||
			 (pScanFilter->csrPersona == QDF_P2P_CLIENT_MODE)) {
			bool ok;

			csr_get_bssdescr_from_scan_handle(hBSSList,
					first_ap_profile);
			status = policy_mgr_is_chan_ok_for_dnbs(pMac->psoc,
					first_ap_profile->channelId, &ok);
			if (QDF_IS_STATUS_ERROR(status)) {
				sme_debug("policy_mgr_is_chan_ok_for_dnbs():error:%d",
					  status);
				csr_scan_result_purge(pMac, hBSSList);
				fCallCallback = true;
				goto error;
			}
			if (!ok) {
				sme_debug("chan:%d not ok for DNBS",
						first_ap_profile->channelId);
				csr_scan_result_purge(pMac, hBSSList);
				fCallCallback = true;
				status = QDF_STATUS_E_INVAL;
				goto error;
			}

			channel_id = csr_get_channel_for_hw_mode_change
					(pMac, hBSSList, sessionId);
			if (!channel_id)
				channel_id = first_ap_profile->channelId;

			status = policy_mgr_handle_conc_multiport(pMac->psoc,
					sessionId, channel_id);
			if ((QDF_IS_STATUS_SUCCESS(status)) &&
				(!csr_wait_for_connection_update(pMac, true))) {
					sme_debug("conn update error");
					csr_scan_result_purge(pMac, hBSSList);
					fCallCallback = true;
					status = QDF_STATUS_E_TIMEOUT;
					goto error;
			} else if (status == QDF_STATUS_E_FAILURE) {
				sme_debug("conn update error");
				csr_scan_result_purge(pMac, hBSSList);
				fCallCallback = true;
				goto error;
			}
		}

		status = csr_roam_issue_connect(pMac, sessionId, pProfile,
				hBSSList, eCsrHddIssued, roamId, false, false);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("CSR failed to issue connect cmd with status: 0x%08X",
				status);
			fCallCallback = true;
		}
	} else if (NULL != pProfile) {
		/* Check whether it is for start ibss */
		if (CSR_IS_START_IBSS(pProfile) ||
		    CSR_IS_NDI(pProfile)) {
			status = csr_roam_issue_connect(pMac, sessionId,
					pProfile, NULL, eCsrHddIssued,
					roamId, false, false);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("Failed with status = 0x%08X",
					status);
				fCallCallback = true;
			}
		} else {
			/* scan for this SSID */
			status = csr_scan_for_ssid(pMac, sessionId, pProfile,
						roamId, true);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("CSR failed to issue SSID scan cmd with status: 0x%08X",
					status);
				fCallCallback = true;
			}
		}
	} else {
		fCallCallback = true;
	}

error:
	if (NULL != pProfile)
		/*
		 * we need to free memory for filter
		 * if profile exists
		 */
		csr_free_scan_filter(pMac, pScanFilter);

	qdf_mem_free(pScanFilter);
end:
	/* tell the caller if we fail to trigger a join request */
	if (fCallCallback) {
		csr_roam_call_callback(pMac, sessionId, NULL, roamId,
				eCSR_ROAM_FAILED, eCSR_ROAM_RESULT_FAILURE);
	}
	qdf_mem_free(first_ap_profile);

	return status;
}

/**
 * csr_roam_reassoc() - process reassoc command
 * @mac_ctx:       mac global context
 * @session_id:    session id
 * @profile:       roam profile
 * @mod_fields:    AC info being modified in reassoc
 * @roam_id:       roam id to be populated
 *
 * Return: status of operation
 */
QDF_STATUS
csr_roam_reassoc(tpAniSirGlobal mac_ctx, uint32_t session_id,
		 struct csr_roam_profile *profile,
		 tCsrRoamModifyProfileFields mod_fields,
		 uint32_t *roam_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool fCallCallback = true;
	uint32_t roamId = 0;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (NULL == profile) {
		sme_err("No profile specified");
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug(
		"called  BSSType = %s (%d) authtype = %d  encryType = %d",
		sme_bss_type_to_string(profile->BSSType),
		profile->BSSType, profile->AuthType.authType[0],
		profile->EncryptionType.encryptionType[0]);
	csr_roam_cancel_roaming(mac_ctx, session_id);
	csr_scan_abort_mac_scan(mac_ctx, session_id, INVAL_SCAN_ID);
	csr_roam_remove_duplicate_command(mac_ctx, session_id, NULL,
					  eCsrHddIssuedReassocToSameAP);
	if (csr_is_conn_state_connected(mac_ctx, session_id)) {
		if (profile) {
			if (profile->SSIDs.numOfSSIDs &&
			    csr_is_ssid_in_list(&session->connectedProfile.SSID,
						&profile->SSIDs)) {
				fCallCallback = false;
			} else {
				/*
				 * Connected SSID did not match with what is
				 * asked in profile
				 */
				sme_debug("SSID mismatch");
			}
		} else if (qdf_mem_cmp(&mod_fields,
				&session->connectedProfile.modifyProfileFields,
				sizeof(tCsrRoamModifyProfileFields))) {
			fCallCallback = false;
		} else {
			sme_debug(
				/*
				 * Either the profile is NULL or none of the
				 * fields in tCsrRoamModifyProfileFields got
				 * modified
				 */
				"Profile NULL or nothing to modify");
		}
	} else {
		sme_debug("Not connected! No need to reassoc");
	}
	if (!fCallCallback) {
		roamId = GET_NEXT_ROAM_ID(&mac_ctx->roam);
		if (roam_id)
			*roam_id = roamId;
		status = csr_roam_issue_reassoc(mac_ctx, session_id, profile,
				&mod_fields, eCsrHddIssuedReassocToSameAP,
				roamId, false);
	} else {
		status = csr_roam_call_callback(mac_ctx, session_id, NULL,
						roamId, eCSR_ROAM_FAILED,
						eCSR_ROAM_RESULT_FAILURE);
	}
	return status;
}

static QDF_STATUS csr_roam_join_last_profile(tpAniSirGlobal pMac,
					     uint32_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tScanResultHandle hBSSList = NULL;
	tCsrScanResultFilter *pScanFilter = NULL;
	uint32_t roamId;
	struct csr_roam_profile *pProfile = NULL;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found ", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pSession->pCurRoamProfile) {
		csr_scan_abort_mac_scan(pMac, sessionId, INVAL_SCAN_ID);
		/* We have to make a copy of pCurRoamProfile because it
		 * will be free inside csr_roam_issue_connect
		 */
		pProfile = qdf_mem_malloc(sizeof(struct csr_roam_profile));
		if (NULL == pProfile) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		status = csr_roam_copy_profile(pMac, pProfile,
			pSession->pCurRoamProfile);
		if (!QDF_IS_STATUS_SUCCESS(status))
			goto end;
		pScanFilter = qdf_mem_malloc(sizeof(tCsrScanResultFilter));
		if (NULL == pScanFilter) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		status = csr_roam_prepare_filter_from_profile(pMac, pProfile,
					pScanFilter);
		if (!QDF_IS_STATUS_SUCCESS(status))
			goto end;
		roamId = GET_NEXT_ROAM_ID(&pMac->roam);
		status = csr_scan_get_result(pMac, pScanFilter, &hBSSList);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			/* we want to put the last connected BSS to the
			 * very beginning, if possible
			 */
			csr_move_bss_to_head_from_bssid(pMac,
				&pSession->connectedProfile.bssid, hBSSList);
			status = csr_roam_issue_connect(pMac, sessionId,
					pProfile, hBSSList, eCsrHddIssued,
					roamId, false, false);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				goto end;
			}
		} else {
			/* scan for this SSID only incase AP suppresses SSID */
			status = csr_scan_for_ssid(pMac, sessionId, pProfile,
					roamId, true);
			if (!QDF_IS_STATUS_SUCCESS(status))
				goto end;
		}
	} /* We have a profile */
	else {
		sme_warn("cannot find a roaming profile");
		goto end;
	}
end:
	if (pScanFilter) {
		csr_free_scan_filter(pMac, pScanFilter);
		qdf_mem_free(pScanFilter);
	}
	if (NULL != pProfile) {
		csr_release_profile(pMac, pProfile);
		qdf_mem_free(pProfile);
	}
	return status;
}

QDF_STATUS csr_roam_connect_to_last_profile(tpAniSirGlobal pMac,
						uint32_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	csr_roam_cancel_roaming(pMac, sessionId);
	csr_roam_remove_duplicate_command(pMac, sessionId, NULL, eCsrHddIssued);
	if (csr_is_conn_state_disconnected(pMac, sessionId))
		status = csr_roam_join_last_profile(pMac, sessionId);

	return status;
}

QDF_STATUS csr_roam_process_disassoc_deauth(tpAniSirGlobal pMac,
						tSmeCmd *pCommand,
					    bool fDisassoc, bool fMICFailure)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool fComplete = false;
	enum csr_roam_substate NewSubstate;
	uint32_t sessionId = pCommand->sessionId;

	if (CSR_IS_WAIT_FOR_KEY(pMac, sessionId)) {
		sme_debug("Stop Wait for key timer and change substate to eCSR_ROAM_SUBSTATE_NONE");
		csr_roam_stop_wait_for_key_timer(pMac);
		csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_NONE,
					sessionId);
	}
	/* change state to 'Roaming'... */
	csr_roam_state_change(pMac, eCSR_ROAMING_STATE_JOINING, sessionId);

	mlme_set_discon_reason_n_from_ap(pMac->psoc, pCommand->sessionId, false,
					 pCommand->u.roamCmd.disconnect_reason);
	if (csr_is_conn_state_ibss(pMac, sessionId)) {
		/* If we are in an IBSS, then stop the IBSS... */
		status =
			csr_roam_issue_stop_bss(pMac, sessionId,
					eCSR_ROAM_SUBSTATE_STOP_BSS_REQ);
		fComplete = (!QDF_IS_STATUS_SUCCESS(status));
	} else if (csr_is_conn_state_infra(pMac, sessionId)) {
		/*
		 * in Infrastructure, we need to disassociate from the
		 * Infrastructure network...
		 */
		NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
		if (eCsrSmeIssuedDisassocForHandoff ==
		    pCommand->u.roamCmd.roamReason) {
			NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF;
		} else
		if ((eCsrForcedDisassoc == pCommand->u.roamCmd.roamReason)
		    && (eSIR_MAC_DISASSOC_LEAVING_BSS_REASON ==
			pCommand->u.roamCmd.reason)) {
			NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT;
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
					 "set to substate eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT");
		}
		if (eCsrSmeIssuedDisassocForHandoff !=
				pCommand->u.roamCmd.roamReason) {
			/*
			 * If we are in neighbor preauth done state then
			 * on receiving disassoc or deauth we dont roam
			 * instead we just disassoc from current ap and
			 * then go to disconnected state.
			 * This happens for ESE and 11r FT connections ONLY.
			 */
			if (csr_roam_is11r_assoc(pMac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(pMac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							pMac, sessionId);
#ifdef FEATURE_WLAN_ESE
			if (csr_roam_is_ese_assoc(pMac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(pMac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							pMac, sessionId);
#endif
			if (csr_roam_is_fast_roam_enabled(pMac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(pMac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							pMac, sessionId);
		}
		if (fDisassoc)
			status = csr_roam_issue_disassociate(pMac, sessionId,
								NewSubstate,
								fMICFailure);
		else
			status = csr_roam_issue_deauth(pMac, sessionId,
						eCSR_ROAM_SUBSTATE_DEAUTH_REQ);
		fComplete = (!QDF_IS_STATUS_SUCCESS(status));
	} else {
		/* we got a dis-assoc request while not connected to any peer */
		/* just complete the command */
		fComplete = true;
		status = QDF_STATUS_E_FAILURE;
	}
	if (fComplete)
		csr_roam_complete(pMac, eCsrNothingToJoin, NULL, sessionId);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (csr_is_conn_state_infra(pMac, sessionId)) {
			/* Set the state to disconnect here */
			pMac->roam.roamSession[sessionId].connectState =
				eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
		}
	} else
		sme_warn(" failed with status %d", status);
	return status;
}

/**
 * csr_prepare_disconnect_command() - function to prepare disconnect command
 * @mac: pointer to global mac structure
 * @session_id: sme session index
 * @sme_cmd: pointer to sme command being prepared
 *
 * Function to prepare internal sme disconnect command
 * Return: QDF_STATUS_SUCCESS on success else QDF_STATUS_E_RESOURCES on failure
 */

QDF_STATUS csr_prepare_disconnect_command(tpAniSirGlobal mac,
			uint32_t session_id, tSmeCmd **sme_cmd)
{
	tSmeCmd *command;

	command = csr_get_command_buffer(mac);
	if (!command) {
		sme_err("fail to get command buffer");
		return QDF_STATUS_E_RESOURCES;
	}

	command->command = eSmeCommandRoam;
	command->sessionId = (uint8_t)session_id;
	command->u.roamCmd.roamReason = eCsrForcedDisassoc;

	*sme_cmd = command;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_roam_issue_disassociate_cmd(tpAniSirGlobal pMac,
					uint32_t sessionId,
					eCsrRoamDisconnectReason reason,
					tSirMacReasonCodes mac_reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		pCommand = csr_get_command_buffer(pMac);
		if (!pCommand) {
			sme_err(" fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		/* Change the substate in case it is wait-for-key */
		if (CSR_IS_WAIT_FOR_KEY(pMac, sessionId)) {
			csr_roam_stop_wait_for_key_timer(pMac);
			csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->sessionId = (uint8_t) sessionId;
		sme_debug("Disassociate reason: %d, vdev_id: %d",
			  reason, sessionId);
		switch (reason) {
		case eCSR_DISCONNECT_REASON_MIC_ERROR:
			pCommand->u.roamCmd.roamReason =
				eCsrForcedDisassocMICFailure;
			break;
		case eCSR_DISCONNECT_REASON_DEAUTH:
			pCommand->u.roamCmd.roamReason = eCsrForcedDeauth;
			break;
		case eCSR_DISCONNECT_REASON_HANDOFF:
			pCommand->u.roamCmd.roamReason =
				eCsrSmeIssuedDisassocForHandoff;
			break;
		case eCSR_DISCONNECT_REASON_UNSPECIFIED:
		case eCSR_DISCONNECT_REASON_DISASSOC:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			break;
		case eCSR_DISCONNECT_REASON_ROAM_HO_FAIL:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			break;
		case eCSR_DISCONNECT_REASON_IBSS_LEAVE:
			pCommand->u.roamCmd.roamReason = eCsrForcedIbssLeave;
			break;
		case eCSR_DISCONNECT_REASON_STA_HAS_LEFT:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			pCommand->u.roamCmd.reason =
				eSIR_MAC_DISASSOC_LEAVING_BSS_REASON;
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				 "SME convert to internal reason code eCsrStaHasLeft");
			break;
		case eCSR_DISCONNECT_REASON_NDI_DELETE:
			pCommand->u.roamCmd.roamReason = eCsrStopBss;
			pCommand->u.roamCmd.roamProfile.BSSType =
				eCSR_BSS_TYPE_NDI;
		default:
			break;
		}
		pCommand->u.roamCmd.disconnect_reason = mac_reason;
		status = csr_queue_sme_command(pMac, pCommand, true);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);
	return status;
}

QDF_STATUS csr_roam_issue_stop_bss_cmd(tpAniSirGlobal pMac, uint32_t sessionId,
				       bool fHighPriority)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(pMac);
	if (NULL != pCommand) {
		/* Change the substate in case it is wait-for-key */
		if (CSR_IS_WAIT_FOR_KEY(pMac, sessionId)) {
			csr_roam_stop_wait_for_key_timer(pMac);
			csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->sessionId = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrStopBss;
		status = csr_queue_sme_command(pMac, pCommand, fHighPriority);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} else {
		sme_err("fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	}
	return status;
}

QDF_STATUS csr_roam_disconnect_internal(tpAniSirGlobal pMac, uint32_t sessionId,
					eCsrRoamDisconnectReason reason,
					tSirMacReasonCodes mac_reason)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
#ifdef FEATURE_WLAN_BTAMP_UT_RF
	/* Stop the retry */
	pSession->maxRetryCount = 0;
	csr_roam_stop_join_retry_timer(pMac, sessionId);
#endif
	/* Not to call cancel roaming here */
	/* Only issue disconnect when necessary */
	if (csr_is_conn_state_connected(pMac, sessionId)
	    || csr_is_bss_type_ibss(pSession->connectedProfile.BSSType)
	    || csr_is_roam_command_waiting_for_session(pMac, sessionId)
	    || CSR_IS_CONN_NDI(&pSession->connectedProfile)) {
		status = csr_roam_issue_disassociate_cmd(pMac, sessionId,
							 reason, mac_reason);
	} else if (pSession->scan_info.profile) {
		pMac->roam.roamSession[sessionId].connectState =
			eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING;
		csr_scan_abort_mac_scan(pMac, sessionId, INVAL_SCAN_ID);
		status = QDF_STATUS_CMD_NOT_QUEUED;
		sme_debug("Disconnect cmd not queued, Roam command is not present return with status: %d",
			status);
	}

	return status;
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void
csr_disable_roaming_offload(tpAniSirGlobal mac_ctx, uint32_t vdev_id)
{
	sme_stop_roaming(MAC_HANDLE(mac_ctx), vdev_id, eCsrForcedDisassoc);
}
#else
static inline void
csr_disable_roaming_offload(tpAniSirGlobal mac_ctx, uint32_t session_id)
{}
#endif

QDF_STATUS csr_roam_disconnect(tpAniSirGlobal mac_ctx, uint32_t session_id,
			       eCsrRoamDisconnectReason reason,
			       tSirMacReasonCodes mac_reason)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("session: %d not found ", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	session->discon_in_progress = true;
	csr_roam_cancel_roaming(mac_ctx, session_id);
	csr_disable_roaming_offload(mac_ctx, session_id);

	csr_roam_remove_duplicate_command(mac_ctx, session_id, NULL,
					  eCsrForcedDisassoc);

	return csr_roam_disconnect_internal(mac_ctx, session_id, reason,
					    mac_reason);
}

QDF_STATUS csr_roam_save_connected_information(tpAniSirGlobal pMac,
					      uint32_t sessionId,
					      struct csr_roam_profile *pProfile,
					      tSirBssDescription *pSirBssDesc,
					      tDot11fBeaconIEs *pIes)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tDot11fBeaconIEs *pIesTemp = pIes;
	uint8_t index;
	struct csr_roam_session *pSession = NULL;
	tCsrRoamConnectedProfile *pConnectProfile = NULL;

	pSession = CSR_GET_SESSION(pMac, sessionId);
	if (NULL == pSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "session: %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pConnectProfile = &pSession->connectedProfile;
	if (pConnectProfile->pAddIEAssoc) {
		qdf_mem_free(pConnectProfile->pAddIEAssoc);
		pConnectProfile->pAddIEAssoc = NULL;
	}
	/*
	 * In case of LFR2.0, the connected profile is copied into a temporary
	 * profile and cleared and then is copied back. This is not needed for
	 * LFR3.0, since the profile is not cleared.
	 */
	if (!pSession->roam_synch_in_progress) {
		qdf_mem_zero(&pSession->connectedProfile,
				sizeof(tCsrRoamConnectedProfile));
		pConnectProfile->AuthType = pProfile->negotiatedAuthType;
		pConnectProfile->AuthInfo = pProfile->AuthType;
		pConnectProfile->akm_list = pProfile->akm_list;
		pConnectProfile->EncryptionType =
			pProfile->negotiatedUCEncryptionType;
		pConnectProfile->EncryptionInfo = pProfile->EncryptionType;
		pConnectProfile->mcEncryptionType =
			pProfile->negotiatedMCEncryptionType;
		pConnectProfile->mcEncryptionInfo = pProfile->mcEncryptionType;
		pConnectProfile->mgmt_encryption_type =
				pProfile->mgmt_encryption_type;
		pConnectProfile->BSSType = pProfile->BSSType;
		pConnectProfile->modifyProfileFields.uapsd_mask =
			pProfile->uapsd_mask;
		qdf_mem_copy(&pConnectProfile->Keys, &pProfile->Keys,
				sizeof(tCsrKeys));
		if (pProfile->nAddIEAssocLength) {
			pConnectProfile->pAddIEAssoc =
				qdf_mem_malloc(pProfile->nAddIEAssocLength);
			if (NULL == pConnectProfile->pAddIEAssoc)
				status = QDF_STATUS_E_NOMEM;
			else
				status = QDF_STATUS_SUCCESS;
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("Failed to allocate memory for IE");
				return QDF_STATUS_E_FAILURE;
			}
			pConnectProfile->nAddIEAssocLength =
				pProfile->nAddIEAssocLength;
			qdf_mem_copy(pConnectProfile->pAddIEAssoc,
					pProfile->pAddIEAssoc,
					pProfile->nAddIEAssocLength);
		}
#ifdef WLAN_FEATURE_11W
		pConnectProfile->MFPEnabled = pProfile->MFPEnabled;
		pConnectProfile->MFPRequired = pProfile->MFPRequired;
		pConnectProfile->MFPCapable = pProfile->MFPCapable;
#endif
	}
	/* Save bssid */
	pConnectProfile->operationChannel = pSirBssDesc->channelId;
	pConnectProfile->beaconInterval = pSirBssDesc->beaconInterval;
	if (!pConnectProfile->beaconInterval)
		sme_err("ERROR: Beacon interval is ZERO");
	csr_get_bss_id_bss_desc(pSirBssDesc, &pConnectProfile->bssid);
	if (pSirBssDesc->mdiePresent) {
		pConnectProfile->MDID.mdiePresent = 1;
		pConnectProfile->MDID.mobilityDomain =
			(pSirBssDesc->mdie[1] << 8) | (pSirBssDesc->mdie[0]);
	}
	if (NULL == pIesTemp)
		status = csr_get_parsed_bss_description_ies(pMac, pSirBssDesc,
							   &pIesTemp);
#ifdef FEATURE_WLAN_ESE
	if ((csr_is_profile_ese(pProfile) ||
	     (QDF_IS_STATUS_SUCCESS(status) && (pIesTemp->ESEVersion.present)
	      && (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM)))
	    && (pMac->roam.configParam.isEseIniFeatureEnabled)) {
		pConnectProfile->isESEAssoc = 1;
	}
#endif
	/* save ssid */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (pIesTemp->SSID.present &&
		    !csr_is_nullssid(pIesTemp->SSID.ssid,
				     pIesTemp->SSID.num_ssid)) {
			pConnectProfile->SSID.length = pIesTemp->SSID.num_ssid;
			qdf_mem_copy(pConnectProfile->SSID.ssId,
				     pIesTemp->SSID.ssid,
				     pIesTemp->SSID.num_ssid);
		} else if (pProfile->SSIDs.numOfSSIDs) {
			pConnectProfile->SSID.length =
					pProfile->SSIDs.SSIDList[0].SSID.length;
			qdf_mem_copy(pConnectProfile->SSID.ssId,
				     pProfile->SSIDs.SSIDList[0].SSID.ssId,
				     pConnectProfile->SSID.length);
		}
		/* Save the bss desc */
		status = csr_roam_save_connected_bss_desc(pMac, sessionId,
								pSirBssDesc);

		if (CSR_IS_QOS_BSS(pIesTemp) || pIesTemp->HTCaps.present)
			/* Some HT AP's dont send WMM IE so in that case we
			 * assume all HT Ap's are Qos Enabled AP's
			 */
			pConnectProfile->qap = true;
		else
			pConnectProfile->qap = false;

		if (pIesTemp->ExtCap.present) {
			struct s_ext_cap *p_ext_cap = (struct s_ext_cap *)
							pIesTemp->ExtCap.bytes;
			pConnectProfile->proxyARPService = p_ext_cap->
							    proxy_arp_service;
		}

		qdf_mem_zero(pConnectProfile->country_code,
			     WNI_CFG_COUNTRY_CODE_LEN);
		if (pIesTemp->Country.present) {
			qdf_mem_copy(pConnectProfile->country_code,
				     pIesTemp->Country.country,
				     WNI_CFG_COUNTRY_CODE_LEN);
			sme_debug("Storing country in connected info, %c%c 0x%x",
				  pConnectProfile->country_code[0],
				  pConnectProfile->country_code[1],
				  pConnectProfile->country_code[2]);
		}

		if (!pIes)
			/* Free memory if it allocated locally */
			qdf_mem_free(pIesTemp);
	}
	/* Save Qos connection */
	pConnectProfile->qosConnection =
		pMac->roam.roamSession[sessionId].fWMMConnection;

	if (!QDF_IS_STATUS_SUCCESS(status))
		csr_free_connect_bss_desc(pMac, sessionId);

	for (index = 0; index < pProfile->SSIDs.numOfSSIDs; index++) {
		if ((pProfile->SSIDs.SSIDList[index].SSID.length ==
		     pConnectProfile->SSID.length)
		    && (!qdf_mem_cmp(pProfile->SSIDs.SSIDList[index].SSID.
				       ssId, pConnectProfile->SSID.ssId,
				       pConnectProfile->SSID.length))) {
			pConnectProfile->handoffPermitted = pProfile->SSIDs.
					SSIDList[index].handoffPermitted;
			break;
		}
		pConnectProfile->handoffPermitted = false;
	}

	return status;
}


bool is_disconnect_pending(tpAniSirGlobal pmac,
				uint8_t sessionid)
{
	tListElem *entry = NULL;
	tListElem *next_entry = NULL;
	tSmeCmd *command = NULL;
	bool disconnect_cmd_exist = false;

	csr_nonscan_pending_ll_lock(pmac);
	entry = csr_nonscan_pending_ll_peek_head(pmac, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csr_nonscan_pending_ll_next(pmac,
					entry, LL_ACCESS_NOLOCK);

		command = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if (command && CSR_IS_DISCONNECT_COMMAND(command) &&
				command->sessionId == sessionid){
			disconnect_cmd_exist = true;
			break;
		}
		entry = next_entry;
	}
	csr_nonscan_pending_ll_unlock(pmac);
	return disconnect_cmd_exist;
}

static void csr_roam_join_rsp_processor(tpAniSirGlobal pMac,
					tSirSmeJoinRsp *pSmeJoinRsp)
{
	tListElem *pEntry = NULL;
	tSmeCmd *pCommand = NULL;
	mac_handle_t mac_handle = MAC_HANDLE(pMac);
	struct csr_roam_session *session_ptr;
	struct csr_roam_connectedinfo *prev_connect_info;
	tPmkidCacheInfo pmksa_entry;
	uint32_t len = 0, roamId = 0, reason_code = 0;
	bool is_dis_pending;
	bool use_same_bss = false;
	bool retry_same_bss = false;

	if (!pSmeJoinRsp) {
		sme_err("Sme Join Response is NULL");
		return;
	}

	session_ptr = CSR_GET_SESSION(pMac, pSmeJoinRsp->sessionId);
	if (!session_ptr) {
		sme_err("session %d not found", pSmeJoinRsp->sessionId);
		return;
	}

	prev_connect_info = &session_ptr->prev_assoc_ap_info;
	/* The head of the active list is the request we sent */
	pEntry = csr_nonscan_active_ll_peek_head(pMac, LL_ACCESS_LOCK);
	if (pEntry)
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);

	if (pSmeJoinRsp->is_fils_connection)
		sme_debug("Fils connection");
	/* Copy Sequence Number last used for FILS assoc failure case */
	if (session_ptr->is_fils_connection)
		session_ptr->fils_seq_num = pSmeJoinRsp->fils_seq_num;

	len = pSmeJoinRsp->assocReqLength +
	      pSmeJoinRsp->assocRspLength + pSmeJoinRsp->beaconLength;
	if (prev_connect_info->pbFrames)
		csr_roam_free_connected_info(pMac, prev_connect_info);

	prev_connect_info->pbFrames = qdf_mem_malloc(len);
	if (prev_connect_info->pbFrames) {
		qdf_mem_copy(prev_connect_info->pbFrames, pSmeJoinRsp->frames,
			     len);
		prev_connect_info->nAssocReqLength =
					pSmeJoinRsp->assocReqLength;
		prev_connect_info->nAssocRspLength =
					pSmeJoinRsp->assocRspLength;
		prev_connect_info->nBeaconLength = pSmeJoinRsp->beaconLength;
	}

	if (eSIR_SME_SUCCESS == pSmeJoinRsp->statusCode) {
		if (pCommand && eCsrSmeIssuedAssocToSimilarAP ==
		    pCommand->u.roamCmd.roamReason) {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
			sme_qos_csr_event_ind(pMac, pSmeJoinRsp->sessionId,
					    SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
#endif
		}

		session_ptr->supported_nss_1x1 = pSmeJoinRsp->supported_nss_1x1;

		/*
		 * The join bssid count can be reset as soon as
		 * we are done with the join requests and returning
		 * the response to upper layers
		 */
		session_ptr->join_bssid_count = 0;
		csr_roam_complete(pMac, eCsrJoinSuccess, (void *)pSmeJoinRsp,
				  pSmeJoinRsp->sessionId);

		return;
	}
	/* The head of the active list is the request we sent
	 * Try to get back the same profile and roam again
	 */
	if (pCommand)
		roamId = pCommand->u.roamCmd.roamId;
	reason_code = pSmeJoinRsp->protStatusCode;
	session_ptr->joinFailStatusCode.reasonCode = reason_code;
	session_ptr->joinFailStatusCode.statusCode = pSmeJoinRsp->statusCode;
	sme_warn("SmeJoinReq failed with status_code= 0x%08X [%d] reason:%d",
		 pSmeJoinRsp->statusCode, pSmeJoinRsp->statusCode,
		 reason_code);
	/*
	 * Delete the PMKID of the BSSID for which the assoc reject is
	 * received from the AP due to invalid PMKID reason.
	 * This will avoid the driver trying to connect to same AP with
	 * the same stale PMKID if multiple connection attempts to different
	 * bss fail and supplicant issues connect request back to the same
	 * AP.
	 */
	if (reason_code == eSIR_MAC_INVALID_PMKID) {
		sme_warn("Assoc reject from BSSID:%pM due to invalid PMKID",
			 session_ptr->joinFailStatusCode.bssId);
		qdf_mem_copy(&pmksa_entry.BSSID.bytes,
			     &session_ptr->joinFailStatusCode.bssId,
			     sizeof(tSirMacAddr));
		sme_roam_del_pmkid_from_cache(mac_handle, session_ptr->sessionId,
					      &pmksa_entry, false);
		retry_same_bss = true;
	}
	if (pSmeJoinRsp->messageType == eWNI_SME_JOIN_RSP &&
	    pSmeJoinRsp->statusCode == eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE &&
	    mlme_get_reconn_after_assoc_timeout_flag(pMac->psoc,
						     pSmeJoinRsp->sessionId))
		retry_same_bss = true;
	if (retry_same_bss && pCommand && pCommand->u.roamCmd.pRoamBssEntry) {
		struct tag_csrscan_result *scan_result;

		scan_result =
			GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry,
				      struct tag_csrscan_result, Link);
		/* Retry with same BSSID without PMKID */
		if (!scan_result->retry_count) {
			sme_info("Retry once with same BSSID, status %d reason %d",
				 pSmeJoinRsp->statusCode, reason_code);
			scan_result->retry_count = 1;
			use_same_bss = true;
		}
	}
	/* If Join fails while Handoff is in progress, indicate
	 * disassociated event to supplicant to reconnect
	 */
	if (csr_roam_is_handoff_in_progress(pMac, pSmeJoinRsp->sessionId)) {
		csr_roam_call_callback(pMac, pSmeJoinRsp->sessionId, NULL,
				       roamId, eCSR_ROAM_DISASSOCIATED,
				       eCSR_ROAM_RESULT_FORCED);
		/* Should indicate neighbor roam algorithm about the
		 * connect failure here
		 */
		csr_neighbor_roam_indicate_connect(pMac, pSmeJoinRsp->sessionId,
						   QDF_STATUS_E_FAILURE);
	}

	/*
	 * if userspace has issued disconnection, driver should not continue
	 * connecting
	 */
	is_dis_pending = is_disconnect_pending(pMac, session_ptr->sessionId);
	if (pCommand && !is_dis_pending &&
	    session_ptr->join_bssid_count < CSR_MAX_BSSID_COUNT) {
		csr_roam(pMac, pCommand, use_same_bss);
		return;
	}

	/*
	 * When the upper layers issue a connect command, there is a roam
	 * command with reason eCsrHddIssued that gets enqueued and an
	 * associated timer for the SME command timeout is started which is
	 * currently 120 seconds. This command would be dequeued only upon
	 * successful connections. In case of join failures, if there are too
	 * many BSS in the cache, and if we fail Join requests with all of them,
	 * there is a chance of timing out the above timer.
	 */
	if (session_ptr->join_bssid_count >= CSR_MAX_BSSID_COUNT)
		sme_err("Excessive Join Req Failures");

	if (is_dis_pending)
		sme_err("disconnect is pending, complete roam");

	if (session_ptr->bRefAssocStartCnt)
		session_ptr->bRefAssocStartCnt--;

	session_ptr->join_bssid_count = 0;
	csr_roam_call_callback(pMac, session_ptr->sessionId, NULL, roamId,
			       eCSR_ROAM_ASSOCIATION_COMPLETION,
			       eCSR_ROAM_RESULT_NOT_ASSOCIATED);
	csr_roam_complete(pMac, eCsrNothingToJoin, NULL, pSmeJoinRsp->sessionId);
}

static QDF_STATUS csr_roam_issue_join(tpAniSirGlobal pMac, uint32_t sessionId,
				      tSirBssDescription *pSirBssDesc,
				      tDot11fBeaconIEs *pIes,
				      struct csr_roam_profile *pProfile,
				      uint32_t roamId)
{
	QDF_STATUS status;

	/* Set the roaming substate to 'join attempt'... */
	csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_JOIN_REQ, sessionId);
	/* attempt to Join this BSS... */
	status = csr_send_join_req_msg(pMac, sessionId, pSirBssDesc, pProfile,
					pIes, eWNI_SME_JOIN_REQ);
	return status;
}

static void
csr_roam_reissue_roam_command(tpAniSirGlobal pMac, uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;
	struct csr_roam_info *roam_info;
	uint32_t sessionId;
	struct csr_roam_session *pSession;

	pEntry = csr_nonscan_active_ll_peek_head(pMac, LL_ACCESS_LOCK);
	if (NULL == pEntry) {
		sme_err("Disassoc rsp can't continue, no active CMD");
		return;
	}
	pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
	if (eSmeCommandRoam != pCommand->command) {
		sme_err("Active cmd, is not a roaming CMD");
		return;
	}
	sessionId = pCommand->sessionId;
	pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (!pCommand->u.roamCmd.fStopWds) {
		if (pSession->bRefAssocStartCnt > 0) {
			/*
			 * bRefAssocStartCnt was incremented in
			 * csr_roam_join_next_bss when the roam command issued
			 * previously. As part of reissuing the roam command
			 * again csr_roam_join_next_bss is going increment
			 * RefAssocStartCnt. So make sure to decrement the
			 * bRefAssocStartCnt
			 */
			pSession->bRefAssocStartCnt--;
		}
		if (eCsrStopRoaming == csr_roam_join_next_bss(pMac, pCommand,
							      true)) {
			sme_warn("Failed to reissue join command");
			csr_roam_complete(pMac, eCsrNothingToJoin, NULL,
					session_id);
		}
		return;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roam_info->pBssDesc = pCommand->u.roamCmd.pLastRoamBss;
	roam_info->statusCode = pSession->joinFailStatusCode.statusCode;
	roam_info->reasonCode = pSession->joinFailStatusCode.reasonCode;
	pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
	csr_roam_call_callback(pMac, sessionId, roam_info,
			       pCommand->u.roamCmd.roamId,
			       eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_INFRA_DISASSOCIATED);

	if (!QDF_IS_STATUS_SUCCESS(csr_roam_issue_stop_bss(pMac, sessionId,
					eCSR_ROAM_SUBSTATE_STOP_BSS_REQ))) {
		sme_err("Failed to reissue stop_bss command for WDS");
		csr_roam_complete(pMac, eCsrNothingToJoin, NULL, session_id);
	}
	qdf_mem_free(roam_info);
}

bool csr_is_roam_command_waiting_for_session(tpAniSirGlobal pMac,
						uint32_t sessionId)
{
	bool fRet = false;
	tListElem *pEntry;
	tSmeCmd *pCommand = NULL;

	/* alwasy lock active list before locking pending list */
	csr_nonscan_active_ll_lock(pMac);
	pEntry = csr_nonscan_active_ll_peek_head(pMac, LL_ACCESS_NOLOCK);
	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if ((eSmeCommandRoam == pCommand->command)
		    && (sessionId == pCommand->sessionId)) {
			fRet = true;
		}
	}
	if (false == fRet) {
		csr_nonscan_pending_ll_lock(pMac);
		pEntry = csr_nonscan_pending_ll_peek_head(pMac,
					 LL_ACCESS_NOLOCK);
		while (pEntry) {
			pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
			if ((eSmeCommandRoam == pCommand->command)
			    && (sessionId == pCommand->sessionId)) {
				fRet = true;
				break;
			}
			pEntry = csr_nonscan_pending_ll_next(pMac, pEntry,
							LL_ACCESS_NOLOCK);
		}
		csr_nonscan_pending_ll_unlock(pMac);
	}
	csr_nonscan_active_ll_unlock(pMac);

	return fRet;
}

static void
csr_roaming_state_config_cnf_processor(tpAniSirGlobal mac_ctx,
			tSmeCmd *cmd, uint32_t result, uint8_t sme_session_id)
{
	struct tag_csrscan_result *scan_result = NULL;
	tSirBssDescription *bss_desc = NULL;
	uint32_t session_id;
	struct csr_roam_session *session;
	tDot11fBeaconIEs *local_ies = NULL;
	bool is_ies_malloced = false;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (NULL == cmd) {
		sme_err("given sme cmd is null");
		return;
	}
	session_id = cmd->sessionId;
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("session %d not found", session_id);
		return;
	}

	if (CSR_IS_ROAMING(session) && session->fCancelRoaming) {
		/* the roaming is cancelled. Simply complete the command */
		sme_warn("Roam command canceled");
		csr_roam_complete(mac_ctx, eCsrNothingToJoin, NULL,
					sme_session_id);
		return;
	}

	if (!QDF_IS_STATUS_SUCCESS(result)) {
		/*
		 * In the event the configuration failed, for infra let the roam
		 * processor attempt to join something else...
		 */
		if (cmd->u.roamCmd.pRoamBssEntry
		    && CSR_IS_INFRASTRUCTURE(&cmd->u.roamCmd.roamProfile)) {
			csr_roam(mac_ctx, cmd, false);
		} else {
			/* We need to complete the command */
			if (csr_is_bss_type_ibss
				    (cmd->u.roamCmd.roamProfile.BSSType)) {
				csr_roam_complete(mac_ctx, eCsrStartBssFailure,
						  NULL, sme_session_id);
			} else {
				csr_roam_complete(mac_ctx, eCsrNothingToJoin,
						  NULL, sme_session_id);
			}
		}
		return;
	}

	/*
	 * Successfully set the configuration parameters for the new Bss.
	 * Attempt to join the roaming Bss
	 */
	if (cmd->u.roamCmd.pRoamBssEntry) {
		scan_result = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
					    struct tag_csrscan_result,
					    Link);
		bss_desc = &scan_result->Result.BssDescriptor;
	}
	if (csr_is_bss_type_ibss(cmd->u.roamCmd.roamProfile.BSSType)
	    || CSR_IS_INFRA_AP(&cmd->u.roamCmd.roamProfile)
	    || CSR_IS_NDI(&cmd->u.roamCmd.roamProfile)) {
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_issue_start_bss(mac_ctx,
						session_id, &session->bssParams,
						&cmd->u.roamCmd.roamProfile,
						bss_desc,
						cmd->u.roamCmd.roamId))) {
			sme_err("CSR start BSS failed");
			/* We need to complete the command */
			csr_roam_complete(mac_ctx, eCsrStartBssFailure, NULL,
					sme_session_id);
		}
		return;
	}

	if (!cmd->u.roamCmd.pRoamBssEntry) {
		sme_err("pRoamBssEntry is NULL");
		/* We need to complete the command */
		csr_roam_complete(mac_ctx, eCsrJoinFailure, NULL,
				sme_session_id);
		return;
	}

	if (NULL == scan_result) {
		/* If we are roaming TO an Infrastructure BSS... */
		QDF_ASSERT(scan_result != NULL);
		return;
	}

	if (!csr_is_infra_bss_desc(bss_desc)) {
		sme_warn("found BSSType mismatching the one in BSS descp");
		return;
	}

	local_ies = (tDot11fBeaconIEs *) scan_result->Result.pvIes;
	if (!local_ies) {
		status = csr_get_parsed_bss_description_ies(mac_ctx, bss_desc,
							    &local_ies);
		if (!QDF_IS_STATUS_SUCCESS(status))
			return;
		is_ies_malloced = true;
	}

	if (csr_is_conn_state_connected_infra(mac_ctx, session_id)) {
		if (csr_is_ssid_equal(mac_ctx, session->pConnectBssDesc,
				      bss_desc, local_ies)) {
			cmd->u.roamCmd.fReassoc = true;
			csr_roam_issue_reassociate(mac_ctx, session_id,
						   bss_desc, local_ies,
						   &cmd->u.roamCmd.roamProfile);
		} else {
			/*
			 * otherwise, we have to issue a new Join request to LIM
			 * because we disassociated from the previously
			 * associated AP.
			 */
			status = csr_roam_issue_join(mac_ctx, session_id,
					bss_desc, local_ies,
					&cmd->u.roamCmd.roamProfile,
					cmd->u.roamCmd.roamId);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				/* try something else */
				csr_roam(mac_ctx, cmd, false);
			}
		}
	} else {
		status = QDF_STATUS_SUCCESS;
		/*
		 * We need to come with other way to figure out that this is
		 * because of HO in BMP The below API will be only available for
		 * Android as it uses a different HO algorithm. Reassoc request
		 * will be used only for ESE and 11r handoff whereas other
		 * legacy roaming should use join request
		 */
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		    && csr_roam_is11r_assoc(mac_ctx, session_id)) {
			status = csr_roam_issue_reassociate(mac_ctx,
					session_id, bss_desc,
					local_ies,
					&cmd->u.roamCmd.roamProfile);
		} else
#ifdef FEATURE_WLAN_ESE
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		   && csr_roam_is_ese_assoc(mac_ctx, session_id)) {
			/* Now serialize the reassoc command. */
			status = csr_roam_issue_reassociate_cmd(mac_ctx,
								session_id);
		} else
#endif
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		   && csr_roam_is_fast_roam_enabled(mac_ctx, session_id)) {
			/* Now serialize the reassoc command. */
			status = csr_roam_issue_reassociate_cmd(mac_ctx,
								session_id);
		} else {
			/*
			 * else we are not connected and attempting to Join.
			 * Issue the Join request.
			 */
			status = csr_roam_issue_join(mac_ctx, session_id,
						    bss_desc,
						    local_ies,
						    &cmd->u.roamCmd.roamProfile,
						    cmd->u.roamCmd.roamId);
		}
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* try something else */
			csr_roam(mac_ctx, cmd, false);
		}
	}
	if (is_ies_malloced) {
		/* Locally allocated */
		qdf_mem_free(local_ies);
	}
}

static void csr_roam_roaming_state_reassoc_rsp_processor(tpAniSirGlobal pMac,
						tpSirSmeJoinRsp pSmeJoinRsp)
{
	enum csr_roamcomplete_result result;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	struct csr_roam_session *csr_session;

	if (pSmeJoinRsp->sessionId >= CSR_ROAM_SESSION_MAX) {
		sme_err("Invalid session ID received %d", pSmeJoinRsp->sessionId);
		return;
	}

	pNeighborRoamInfo =
		&pMac->roam.neighborRoamInfo[pSmeJoinRsp->sessionId];
	if (eSIR_SME_SUCCESS == pSmeJoinRsp->statusCode) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			 "CSR SmeReassocReq Successful");
		result = eCsrReassocSuccess;
		csr_session = CSR_GET_SESSION(pMac, pSmeJoinRsp->sessionId);
		if (NULL != csr_session) {
			csr_session->supported_nss_1x1 =
				pSmeJoinRsp->supported_nss_1x1;
			sme_debug("SME session supported nss: %d",
				csr_session->supported_nss_1x1);
		}
		/*
		 * Since the neighbor roam algorithm uses reassoc req for
		 * handoff instead of join, we need the response contents while
		 * processing the result in csr_roam_process_results()
		 */
		if (csr_roam_is_handoff_in_progress(pMac,
						pSmeJoinRsp->sessionId)) {
			/* Need to dig more on indicating events to
			 * SME QoS module
			 */
			sme_qos_csr_event_ind(pMac, pSmeJoinRsp->sessionId,
					    SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
			csr_roam_complete(pMac, result, pSmeJoinRsp,
					pSmeJoinRsp->sessionId);
		} else {
			csr_roam_complete(pMac, result, NULL,
					pSmeJoinRsp->sessionId);
		}
	}
	/* Should we handle this similar to handling the join failure? Is it ok
	 * to call csr_roam_complete() with state as CsrJoinFailure
	 */
	else {
		sme_warn(
			"CSR SmeReassocReq failed with statusCode= 0x%08X [%d]",
			pSmeJoinRsp->statusCode, pSmeJoinRsp->statusCode);
		result = eCsrReassocFailure;
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
			WLAN_LOG_INDICATOR_HOST_DRIVER,
			WLAN_LOG_REASON_ROAM_FAIL,
			false, false);
		if ((eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE ==
		     pSmeJoinRsp->statusCode)
		    || (eSIR_SME_FT_REASSOC_FAILURE ==
			pSmeJoinRsp->statusCode)
		    || (eSIR_SME_INVALID_PARAMETERS ==
			pSmeJoinRsp->statusCode)) {
			/* Inform HDD to turn off FT flag in HDD */
			if (pNeighborRoamInfo) {
				struct csr_roam_info *roam_info;
				uint32_t roam_id = 0;

				roam_info = qdf_mem_malloc(sizeof(*roam_info));
				if (!roam_info)
					return;
				mlme_set_discon_reason_n_from_ap(pMac->psoc,
					pSmeJoinRsp->sessionId, false,
					eSIR_MAC_HOST_TRIGGERED_ROAM_FAILURE);
				csr_roam_call_callback(pMac,
						       pSmeJoinRsp->sessionId,
						       roam_info, roam_id,
						       eCSR_ROAM_FT_REASSOC_FAILED,
						       eCSR_ROAM_RESULT_SUCCESS);
				/*
				 * Since the above callback sends a disconnect
				 * to HDD, we should clean-up our state
				 * machine as well to be in sync with the upper
				 * layers. There is no need to send a disassoc
				 * since: 1) we will never reassoc to the
				 * current AP in LFR, and 2) there is no need
				 * to issue a disassoc to the AP with which we
				 * were trying to reassoc.
				 */
				csr_roam_complete(pMac, eCsrJoinFailure, NULL,
						pSmeJoinRsp->sessionId);
				qdf_mem_free(roam_info);
				return;
			}
		}
		/* In the event that the Reassociation fails, then we need to
		 * Disassociate the current association and keep roaming. Note
		 * that we will attempt to Join the AP instead of a Reassoc
		 * since we may have attempted a 'Reassoc to self', which AP's
		 * that don't support Reassoc will force a Disassoc. The
		 * isassoc rsp message will remove the command from active list
		 */
		if (!QDF_IS_STATUS_SUCCESS
			    (csr_roam_issue_disassociate
				    (pMac, pSmeJoinRsp->sessionId,
				    eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE,
				false))) {
			csr_roam_complete(pMac, eCsrJoinFailure, NULL,
					pSmeJoinRsp->sessionId);
		}
	}
}

static void csr_roam_roaming_state_stop_bss_rsp_processor(tpAniSirGlobal pMac,
							  tSirSmeRsp *pSmeRsp)
{
	enum csr_roamcomplete_result result_code = eCsrNothingToJoin;
	struct csr_roam_profile *profile;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	{
		host_log_ibss_pkt_type *pIbssLog;

		WLAN_HOST_DIAG_LOG_ALLOC(pIbssLog, host_log_ibss_pkt_type,
					 LOG_WLAN_IBSS_C);
		if (pIbssLog) {
			pIbssLog->eventId = WLAN_IBSS_EVENT_STOP_RSP;
			if (eSIR_SME_SUCCESS != pSmeRsp->statusCode)
				pIbssLog->status = WLAN_IBSS_STATUS_FAILURE;
			WLAN_HOST_DIAG_LOG_REPORT(pIbssLog);
		}
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	pMac->roam.roamSession[pSmeRsp->sessionId].connectState =
		eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	if (CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(pMac, pSmeRsp->sessionId)) {
		profile =
		    pMac->roam.roamSession[pSmeRsp->sessionId].pCurRoamProfile;
		if (profile && CSR_IS_CONN_NDI(profile)) {
			result_code = eCsrStopBssSuccess;
			if (pSmeRsp->statusCode != eSIR_SME_SUCCESS)
				result_code = eCsrStopBssFailure;
		}
		csr_roam_complete(pMac, result_code, NULL, pSmeRsp->sessionId);
	} else if (CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE(pMac,
			pSmeRsp->sessionId)) {
		csr_roam_reissue_roam_command(pMac, pSmeRsp->sessionId);
	}
}

/**
 * csr_dequeue_command() - removes a command from active cmd list
 * @pMac:          mac global context
 *
 * Return: void
 */
static void
csr_dequeue_command(tpAniSirGlobal mac_ctx)
{
	bool fRemoveCmd;
	tSmeCmd *cmd = NULL;
	tListElem *entry = csr_nonscan_active_ll_peek_head(mac_ctx,
					    LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("NO commands are active");
		return;
	}

	cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
	/*
	 * If the head of the queue is Active and it is a given cmd type, remove
	 * and put this on the Free queue.
	 */
	if (eSmeCommandRoam != cmd->command) {
		sme_err("Roam command not active");
		return;
	}
	/*
	 * we need to process the result first before removing it from active
	 * list because state changes still happening insides
	 * roamQProcessRoamResults so no other roam command should be issued.
	 */
	fRemoveCmd = csr_nonscan_active_ll_remove_entry(mac_ctx, entry,
					 LL_ACCESS_LOCK);
	if (cmd->u.roamCmd.fReleaseProfile) {
		csr_release_profile(mac_ctx, &cmd->u.roamCmd.roamProfile);
		cmd->u.roamCmd.fReleaseProfile = false;
	}
	if (fRemoveCmd)
		csr_release_command(mac_ctx, cmd);
	else
		sme_err("fail to remove cmd reason %d",
			cmd->u.roamCmd.roamReason);
}

/**
 * csr_post_roam_failure() - post roam failure back to csr and issues a disassoc
 * @pMac:               mac global context
 * @session_id:         session id
 * @roam_info:          roam info struct
 * @scan_filter:        scan filter to free
 * @cur_roam_profile:   current csr roam profile
 *
 * Return: void
 */
static void
csr_post_roam_failure(tpAniSirGlobal mac_ctx,
		      uint32_t session_id,
		      struct csr_roam_info *roam_info,
		      tCsrScanResultFilter *scan_filter,
		      struct csr_roam_profile *cur_roam_profile)
{
	QDF_STATUS status;

	if (scan_filter) {
		csr_free_scan_filter(mac_ctx, scan_filter);
		qdf_mem_free(scan_filter);
	}
	if (cur_roam_profile)
		qdf_mem_free(cur_roam_profile);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		csr_roam_synch_clean_up(mac_ctx, session_id);
#endif
	/* Inform the upper layers that the reassoc failed */
	qdf_mem_zero(roam_info, sizeof(struct csr_roam_info));
	csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
			       eCSR_ROAM_FT_REASSOC_FAILED,
			       eCSR_ROAM_RESULT_SUCCESS);
	/*
	 * Issue a disassoc request so that PE/LIM uses this to clean-up the FT
	 * session. Upon success, we would re-enter this routine after receiving
	 * the disassoc response and will fall into the reassoc fail sub-state.
	 * And, eventually call csr_roam_complete which would remove the roam
	 * command from SME active queue.
	 */
	status = csr_roam_issue_disassociate(mac_ctx, session_id,
			eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"csr_roam_issue_disassociate failed, status %d",
			status);
		csr_roam_complete(mac_ctx, eCsrJoinFailure, NULL, session_id);
	}
}

/**
 * csr_check_profile_in_scan_cache() - finds if roam profile is present in scan
 * cache or not
 * @pMac:                  mac global context
 * @scan_filter:           out param, scan filter
 * @neighbor_roam_info:    roam info struct
 * @hBSSList:              scan result
 *
 * Return: true if found else false.
 */
static bool
csr_check_profile_in_scan_cache(tpAniSirGlobal mac_ctx,
				tCsrScanResultFilter **scan_filter,
				tpCsrNeighborRoamControlInfo neighbor_roam_info,
				tScanResultHandle *hBSSList)
{
	QDF_STATUS status;
	*scan_filter = qdf_mem_malloc(sizeof(tCsrScanResultFilter));
	if (NULL == *scan_filter) {
		sme_err("alloc for ScanFilter failed");
		return false;
	}
	(*scan_filter)->scan_filter_for_roam = 1;
	status = csr_roam_prepare_filter_from_profile(mac_ctx,
			&neighbor_roam_info->csrNeighborRoamProfile,
			*scan_filter);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"failed to prepare scan filter, status %d",
			status);
		return false;
	}
	status = csr_scan_get_result(mac_ctx, *scan_filter, hBSSList);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"csr_scan_get_result failed, status %d",
			status);
		return false;
	}
	return true;
}

static
void csr_roam_roaming_state_disassoc_rsp_processor(tpAniSirGlobal pMac,
						   tSirSmeDisassocRsp *pSmeRsp)
{
	tScanResultHandle hBSSList;
	struct csr_roam_info *roamInfo;
	tCsrScanResultFilter *pScanFilter = NULL;
	uint32_t roamId = 0;
	struct csr_roam_profile *pCurRoamProfile = NULL;
	QDF_STATUS status;
	uint32_t sessionId;
	struct csr_roam_session *pSession;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	tSirSmeDisassocRsp SmeDisassocRsp;

	csr_ser_des_unpack_diassoc_rsp((uint8_t *) pSmeRsp, &SmeDisassocRsp);
	sessionId = SmeDisassocRsp.sessionId;
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG, "sessionId %d",
		  sessionId);

	if (csr_is_conn_state_infra(pMac, sessionId)) {
		pMac->roam.roamSession[sessionId].connectState =
			eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	}

	pSession = CSR_GET_SESSION(pMac, sessionId);
	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	roamInfo = qdf_mem_malloc(sizeof(*roamInfo));

	if (!roamInfo) {
		sme_err("failed to allocate memory");
		return;
	}

	if (CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN(pMac, sessionId)) {
		sme_debug("***eCsrNothingToJoin***");
		csr_roam_complete(pMac, eCsrNothingToJoin, NULL, sessionId);
	} else if (CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED(pMac, sessionId) ||
		   CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(pMac, sessionId)) {
		if (eSIR_SME_SUCCESS == SmeDisassocRsp.statusCode) {
			sme_debug("CSR force disassociated successful");
			/*
			 * A callback to HDD will be issued from
			 * csr_roam_complete so no need to do anything here
			 */
		}
		csr_roam_complete(pMac, eCsrNothingToJoin, NULL, sessionId);
	} else if (CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(pMac, sessionId)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			 "CSR SmeDisassocReq due to HO on session %d",
			  sessionId);
		pNeighborRoamInfo = &pMac->roam.neighborRoamInfo[sessionId];
		/*
		 * First ensure if the roam profile is in the scan cache.
		 * If not, post a reassoc failure and disconnect.
		 */
		if (!csr_check_profile_in_scan_cache(pMac, &pScanFilter,
						pNeighborRoamInfo, &hBSSList))
			goto POST_ROAM_FAILURE;

		/* notify HDD about handoff and provide the BSSID too */
		roamInfo->reasonCode = eCsrRoamReasonBetterAP;

		qdf_copy_macaddr(&roamInfo->bssid,
			pNeighborRoamInfo->csrNeighborRoamProfile.BSSIDs.bssid);

		csr_roam_call_callback(pMac, sessionId, roamInfo, 0,
				       eCSR_ROAM_ROAMING_START,
				       eCSR_ROAM_RESULT_NONE);

		/*
		 * Copy the connected profile to apply the same for this
		 * connection as well
		 */
		pCurRoamProfile = qdf_mem_malloc(sizeof(*pCurRoamProfile));
		if (pCurRoamProfile != NULL) {
			/*
			 * notify sub-modules like QoS etc. that handoff
			 * happening
			 */
			sme_qos_csr_event_ind(pMac, sessionId,
					      SME_QOS_CSR_HANDOFF_ASSOC_REQ,
					      NULL);
			csr_roam_copy_profile(pMac, pCurRoamProfile,
					      pSession->pCurRoamProfile);
			/*
			 * After ensuring that the roam profile is in the scan
			 * result list, and pSession->pCurRoamProfile is saved,
			 * dequeue the command from the active list.
			 */
			csr_dequeue_command(pMac);
			/* make sure to put it at the head of the cmd queue */
			status = csr_roam_issue_connect(pMac, sessionId,
					pCurRoamProfile, hBSSList,
					eCsrSmeIssuedAssocToSimilarAP,
					roamId, true, false);
			if (!QDF_IS_STATUS_SUCCESS(status))
				sme_err(
					"issue_connect failed. status %d",
					status);

			csr_release_profile(pMac, pCurRoamProfile);
			qdf_mem_free(pCurRoamProfile);
			csr_free_scan_filter(pMac, pScanFilter);
			qdf_mem_free(pScanFilter);
			qdf_mem_free(roamInfo);
			return;
		} else {
			sme_err("pCurRoamProfile memory alloc failed");
			QDF_ASSERT(0);
			csr_dequeue_command(pMac);
		}
		csr_scan_result_purge(pMac, hBSSList);

POST_ROAM_FAILURE:
		mlme_set_discon_reason_n_from_ap(pMac->psoc, sessionId, false,
					eSIR_MAC_HOST_TRIGGERED_ROAM_FAILURE);
		csr_post_roam_failure(pMac, sessionId, roamInfo,
			      pScanFilter, pCurRoamProfile);
	} /* else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO( pMac ) ) */
	else if (CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL(pMac, sessionId)) {
		/* Disassoc due to Reassoc failure falls into this codepath */
		csr_roam_complete(pMac, eCsrJoinFailure, NULL, sessionId);
	} else {
		if (eSIR_SME_SUCCESS == SmeDisassocRsp.statusCode) {
			/*
			 * Successfully disassociated from the 'old' Bss.
			 * We get Disassociate response in three conditions.
			 * 1) The case where we are disasociating from an Infra
			 *    Bss to start an IBSS.
			 * 2) When we are disassociating from an Infra Bss to
			 *    join an IBSS or a new infra network.
			 * 3) Where we are doing an Infra to Infra roam between
			 *    networks with different SSIDs.
			 * In all cases, we set the new Bss configuration here
			 * and attempt to join
			 */
			sme_debug("Disassociated successfully");
		} else {
			sme_err(
				"DisassocReq failed, statusCode= 0x%08X",
				SmeDisassocRsp.statusCode);
		}
		/* We are not done yet. Get the data and continue roaming */
		csr_roam_reissue_roam_command(pMac, sessionId);
	}
	qdf_mem_free(roamInfo);
}

static void csr_roam_roaming_state_deauth_rsp_processor(tpAniSirGlobal pMac,
						tSirSmeDeauthRsp *pSmeRsp)
{
	tSirResultCodes statusCode;
	/* No one is sending eWNI_SME_DEAUTH_REQ to PE. */
	statusCode = csr_get_de_auth_rsp_status_code(pSmeRsp);
	pMac->roam.deauthRspStatus = statusCode;
	if (CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ(pMac, pSmeRsp->sessionId)) {
		csr_roam_complete(pMac, eCsrNothingToJoin, NULL,
				pSmeRsp->sessionId);
	} else {
		if (eSIR_SME_SUCCESS == statusCode) {
			/* Successfully deauth from the 'old' Bss... */
			/* */
			sme_debug(
				"CSR SmeDeauthReq disassociated Successfully");
		}
		/* We are not done yet. Get the data and continue roaming */
		csr_roam_reissue_roam_command(pMac, pSmeRsp->sessionId);
	}
}

static void csr_roam_roaming_state_start_bss_rsp_processor(tpAniSirGlobal pMac,
							   tSirSmeStartBssRsp *
							   pSmeStartBssRsp)
{
	enum csr_roamcomplete_result result;

	if (eSIR_SME_SUCCESS == pSmeStartBssRsp->statusCode) {
		sme_debug("SmeStartBssReq Successful");
		result = eCsrStartBssSuccess;
	} else {
		sme_warn("SmeStartBssReq failed with statusCode= 0x%08X",
			pSmeStartBssRsp->statusCode);
		/* Let csr_roam_complete decide what to do */
		result = eCsrStartBssFailure;
	}
	csr_roam_complete(pMac, result, pSmeStartBssRsp,
				pSmeStartBssRsp->sessionId);
}

/**
 * csr_roam_send_disconnect_done_indication() - Send disconnect ind to HDD.
 *
 * @mac_ctx: mac global context
 * @msg_ptr: incoming message
 *
 * This function gives final disconnect event to HDD after all cleanup in
 * lower layers is done.
 *
 * Return: None
 */
static void
csr_roam_send_disconnect_done_indication(tpAniSirGlobal mac_ctx, tSirSmeRsp
				     *msg_ptr)
{
	struct sir_sme_discon_done_ind *discon_ind =
				(struct sir_sme_discon_done_ind *)(msg_ptr);
	struct csr_roam_info roam_info;
	struct csr_roam_session *session;
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id;

	sme_debug("DISCONNECT_DONE_IND RC:%d", discon_ind->reason_code);
	vdev_id = discon_ind->session_id;

	if (CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		roam_info.reasonCode = discon_ind->reason_code;
		roam_info.statusCode = eSIR_SME_STA_NOT_ASSOCIATED;
		qdf_mem_copy(roam_info.peerMac.bytes, discon_ind->peer_mac,
			     ETH_ALEN);

		roam_info.rssi = mac_ctx->peer_rssi;
		roam_info.tx_rate = mac_ctx->peer_txrate;
		roam_info.rx_rate = mac_ctx->peer_rxrate;
		roam_info.disassoc_reason = discon_ind->reason_code;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							    vdev_id,
							    WLAN_LEGACY_SME_ID);
		if (vdev)
			roam_info.disconnect_ies =
				mlme_get_peer_disconnect_ies(vdev);

		csr_roam_call_callback(mac_ctx, vdev_id,
				       &roam_info, 0, eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_DISASSOC_IND);
		if (vdev) {
			mlme_free_peer_disconnect_ies(vdev);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		}
		session = CSR_GET_SESSION(mac_ctx, vdev_id);
		if (!CSR_IS_INFRA_AP(&session->connectedProfile))
			csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
					      vdev_id);

		if (CSR_IS_INFRASTRUCTURE(&session->connectedProfile)) {
			csr_free_roam_profile(mac_ctx, vdev_id);
			csr_free_connect_bss_desc(mac_ctx, vdev_id);
			csr_roam_free_connect_profile(
						&session->connectedProfile);
			csr_roam_free_connected_info(mac_ctx,
						     &session->connectedInfo);
		}

	} else {
		sme_err("Inactive vdev_id %d", vdev_id);
	}

	/*
	 * Release WM status change command as eWNI_SME_DISCONNECT_DONE_IND
	 * has been sent to HDD and there is nothing else left to do.
	 */
	csr_roam_wm_status_change_complete(mac_ctx, vdev_id);
}


/**
 * csr_roaming_state_msg_processor() - process roaming messages
 * @pMac:       mac global context
 * @pMsgBuf:    message buffer
 *
 * We need to be careful on whether to cast pMsgBuf (pSmeRsp) to other type of
 * strucutres. It depends on how the message is constructed. If the message is
 * sent by lim_send_sme_rsp, the pMsgBuf is only a generic response and can only
 * be used as pointer to tSirSmeRsp. For the messages where sender allocates
 * memory for specific structures, then it can be cast accordingly.
 *
 * Return: status of operation
 */
void csr_roaming_state_msg_processor(tpAniSirGlobal pMac, void *pMsgBuf)
{
	tSirSmeRsp *pSmeRsp;
	tSmeIbssPeerInd *pIbssPeerInd;
	struct csr_roam_info *roam_info;

	pSmeRsp = (tSirSmeRsp *) pMsgBuf;

	switch (pSmeRsp->messageType) {

	case eWNI_SME_JOIN_RSP:
		/* in Roaming state, process the Join response message... */
		if (CSR_IS_ROAM_SUBSTATE_JOIN_REQ(pMac, pSmeRsp->sessionId))
			/* We sent a JOIN_REQ */
			csr_roam_join_rsp_processor(pMac,
						    (tSirSmeJoinRsp *) pSmeRsp);
		break;
	case eWNI_SME_REASSOC_RSP:
		/* or the Reassociation response message... */
		if (CSR_IS_ROAM_SUBSTATE_REASSOC_REQ(pMac, pSmeRsp->sessionId))
			csr_roam_roaming_state_reassoc_rsp_processor(pMac,
						(tpSirSmeJoinRsp) pSmeRsp);
		break;
	case eWNI_SME_STOP_BSS_RSP:
		/* or the Stop Bss response message... */
		csr_roam_roaming_state_stop_bss_rsp_processor(pMac, pSmeRsp);
		break;
	case eWNI_SME_DISASSOC_RSP:
		/* or the Disassociate response message... */
		if (CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(pMac, pSmeRsp->sessionId)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN(pMac,
							pSmeRsp->sessionId)
		    || CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL(pMac,
							pSmeRsp->sessionId)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED(pMac,
							pSmeRsp->sessionId)
		    || CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE(pMac,
							pSmeRsp->sessionId)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(pMac,
							pSmeRsp->sessionId)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				 "eWNI_SME_DISASSOC_RSP subState = %s",
				  mac_trace_getcsr_roam_sub_state(
				  pMac->roam.curSubState[pSmeRsp->sessionId]));
			csr_roam_roaming_state_disassoc_rsp_processor(pMac,
						(tSirSmeDisassocRsp *) pSmeRsp);
		}
		break;
	case eWNI_SME_DEAUTH_RSP:
		/* or the Deauthentication response message... */
		if (CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ(pMac, pSmeRsp->sessionId)) {
			csr_remove_nonscan_cmd_from_pending_list(pMac,
					pSmeRsp->sessionId,
					eSmeCommandWmStatusChange);
			csr_roam_roaming_state_deauth_rsp_processor(pMac,
						(tSirSmeDeauthRsp *) pSmeRsp);
		}
		break;
	case eWNI_SME_START_BSS_RSP:
		/* or the Start BSS response message... */
		if (CSR_IS_ROAM_SUBSTATE_START_BSS_REQ(pMac,
						       pSmeRsp->sessionId))
			csr_roam_roaming_state_start_bss_rsp_processor(pMac,
						(tSirSmeStartBssRsp *) pSmeRsp);
		break;
	/* In case CSR issues STOP_BSS, we need to tell HDD about peer departed
	 * because PE is removing them
	 */
	case eWNI_SME_IBSS_PEER_DEPARTED_IND:
		pIbssPeerInd = (tSmeIbssPeerInd *) pSmeRsp;
		sme_err("Peer departed ntf from LIM in joining state");
		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info) {
			sme_err("failed to allocate memory for roam_info");
			break;
		}

		roam_info->staId = (uint8_t) pIbssPeerInd->staId;
		qdf_copy_macaddr(&roam_info->peerMac, &pIbssPeerInd->peer_addr);
		csr_roam_call_callback(pMac, pSmeRsp->sessionId, roam_info, 0,
				       eCSR_ROAM_CONNECT_STATUS_UPDATE,
				       eCSR_ROAM_RESULT_IBSS_PEER_DEPARTED);
		qdf_mem_free(roam_info);
		roam_info = NULL;
		break;
	case eWNI_SME_GET_RSSI_REQ:
	{
		tAniGetRssiReq *pGetRssiReq = (tAniGetRssiReq *) pMsgBuf;

		if (NULL != pGetRssiReq->rssiCallback)
			((tCsrRssiCallback) pGetRssiReq->rssiCallback)
				(pGetRssiReq->lastRSSI, pGetRssiReq->staId,
				pGetRssiReq->pDevContext);
		else
			sme_err("pGetRssiReq->rssiCallback is NULL");
	}
	break;
	case eWNI_SME_TRIGGER_SAE:
		sme_debug("Invoke SAE callback");
		csr_sae_callback(pMac, pSmeRsp);
		break;

	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_check_for_link_status_change(pMac, pSmeRsp);
		break;

	case eWNI_SME_PURGE_ALL_PDEV_CMDS_REQ:
		csr_purge_pdev_all_ser_cmd_list_sync(pMac, pMsgBuf);
		break;

	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(pMac, pSmeRsp);
		break;
	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
		csr_roam_joined_state_msg_processor(pMac, pSmeRsp);
		break;
	default:
		sme_debug("Unexpected message type: %d[0x%X] received in substate %s",
			pSmeRsp->messageType, pSmeRsp->messageType,
			mac_trace_getcsr_roam_sub_state(
				pMac->roam.curSubState[pSmeRsp->sessionId]));
		/* If we are connected, check the link status change */
		if (!csr_is_conn_state_disconnected(pMac, pSmeRsp->sessionId))
			csr_roam_check_for_link_status_change(pMac, pSmeRsp);
		break;
	}
}

void csr_roam_joined_state_msg_processor(tpAniSirGlobal pMac, void *pMsgBuf)
{
	tSirSmeRsp *pSirMsg = (tSirSmeRsp *) pMsgBuf;

	switch (pSirMsg->messageType) {
	case eWNI_SME_GET_STATISTICS_RSP:
		sme_debug("Stats rsp from PE");
		csr_roam_stats_rsp_processor(pMac, pSirMsg);
		break;
	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
	{
		struct csr_roam_session *pSession;
		tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf;
		struct csr_roam_info *roam_info;
		uint32_t sessionId;
		QDF_STATUS status;

		sme_debug("ASSOCIATION confirmation can be given to upper layer ");
		pUpperLayerAssocCnf =
			(tSirSmeAssocIndToUpperLayerCnf *) pMsgBuf;
		status = csr_roam_get_session_id_from_bssid(pMac,
							(struct qdf_mac_addr *)
							   pUpperLayerAssocCnf->
							   bssId, &sessionId);
		pSession = CSR_GET_SESSION(pMac, sessionId);

		if (!pSession) {
			sme_err("session %d not found", sessionId);
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}

		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info) {
			sme_err("roam_info not allocated");
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}
		/* send the status code as Success */
		roam_info->statusCode = eSIR_SME_SUCCESS;
		roam_info->u.pConnectedProfile =
			&pSession->connectedProfile;
		roam_info->staId = (uint8_t) pUpperLayerAssocCnf->aid;
		roam_info->rsnIELen =
			(uint8_t) pUpperLayerAssocCnf->rsnIE.length;
		roam_info->prsnIE =
			pUpperLayerAssocCnf->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
		roam_info->wapiIELen =
			(uint8_t) pUpperLayerAssocCnf->wapiIE.length;
		roam_info->pwapiIE =
			pUpperLayerAssocCnf->wapiIE.wapiIEdata;
#endif
		roam_info->addIELen =
			(uint8_t) pUpperLayerAssocCnf->addIE.length;
		roam_info->paddIE =
			pUpperLayerAssocCnf->addIE.addIEdata;
		qdf_mem_copy(roam_info->peerMac.bytes,
			     pUpperLayerAssocCnf->peerMacAddr,
			     sizeof(tSirMacAddr));
		qdf_mem_copy(&roam_info->bssid,
			     pUpperLayerAssocCnf->bssId,
			     sizeof(struct qdf_mac_addr));
		roam_info->wmmEnabledSta =
			pUpperLayerAssocCnf->wmmEnabledSta;
		roam_info->timingMeasCap =
			pUpperLayerAssocCnf->timingMeasCap;
		qdf_mem_copy(&roam_info->chan_info,
			     &pUpperLayerAssocCnf->chan_info,
			     sizeof(tSirSmeChanInfo));
		roam_info->ampdu = pUpperLayerAssocCnf->ampdu;
		roam_info->sgi_enable = pUpperLayerAssocCnf->sgi_enable;
		roam_info->tx_stbc = pUpperLayerAssocCnf->tx_stbc;
		roam_info->rx_stbc = pUpperLayerAssocCnf->rx_stbc;
		roam_info->ch_width = pUpperLayerAssocCnf->ch_width;
		roam_info->mode = pUpperLayerAssocCnf->mode;
		roam_info->max_supp_idx = pUpperLayerAssocCnf->max_supp_idx;
		roam_info->max_ext_idx = pUpperLayerAssocCnf->max_ext_idx;
		roam_info->max_mcs_idx = pUpperLayerAssocCnf->max_mcs_idx;
		roam_info->rx_mcs_map = pUpperLayerAssocCnf->rx_mcs_map;
		roam_info->tx_mcs_map = pUpperLayerAssocCnf->tx_mcs_map;
		roam_info->ecsa_capable = pUpperLayerAssocCnf->ecsa_capable;
		if (pUpperLayerAssocCnf->ht_caps.present)
			roam_info->ht_caps = pUpperLayerAssocCnf->ht_caps;
		if (pUpperLayerAssocCnf->vht_caps.present)
			roam_info->vht_caps = pUpperLayerAssocCnf->vht_caps;
		roam_info->capability_info =
					pUpperLayerAssocCnf->capability_info;
		roam_info->he_caps_present =
					pUpperLayerAssocCnf->he_caps_present;

		if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile)) {
			if (pUpperLayerAssocCnf->ies_len > 0) {
				roam_info->assocReqLength =
						pUpperLayerAssocCnf->ies_len;
				roam_info->assocReqPtr =
						pUpperLayerAssocCnf->ies;
			}

			pMac->roam.roamSession[sessionId].connectState =
				eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED;
			roam_info->fReassocReq =
				pUpperLayerAssocCnf->reassocReq;
			status = csr_roam_call_callback(pMac, sessionId,
						       roam_info, 0,
						       eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF);
		}
		if (pUpperLayerAssocCnf->ies)
			qdf_mem_free(pUpperLayerAssocCnf->ies);
		qdf_mem_free(roam_info);
	}
	break;
	default:
		csr_roam_check_for_link_status_change(pMac, pSirMsg);
		break;
	}
}

QDF_STATUS csr_roam_issue_set_context_req(tpAniSirGlobal pMac,
					  uint32_t sessionId,
					  eCsrEncryptionType EncryptType,
					  tSirBssDescription *pBssDescription,
					  tSirMacAddr *bssId, bool addKey,
					  bool fUnicast,
					  tAniKeyDirection aniKeyDirection,
					  uint8_t keyId, uint16_t keyLength,
					  uint8_t *pKey, uint8_t paeRole)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tAniEdType edType;

	sme_debug("sessionId: %d EncryptType: %d", sessionId, EncryptType);

	if (eCSR_ENCRYPT_TYPE_UNKNOWN == EncryptType)
		EncryptType = eCSR_ENCRYPT_TYPE_NONE;

	edType = csr_translate_encrypt_type_to_ed_type(EncryptType);

	/*
	 * Allow 0 keys to be set for the non-WPA encrypt types. For WPA encrypt
	 * types, the num keys must be non-zero or LIM will reject the set
	 * context (assumes the SET_CONTEXT does not occur until the keys are
	 * distrubuted).
	 */
	if (CSR_IS_ENC_TYPE_STATIC(EncryptType) || addKey) {
		tCsrRoamSetKey setKey;

		setKey.encType = EncryptType;
		setKey.keyDirection = aniKeyDirection;
		qdf_mem_copy(&setKey.peerMac, bssId, sizeof(struct
							qdf_mac_addr));
		/* 0 for supplicant */
		setKey.paeRole = paeRole;
		/* Key index */
		setKey.keyId = keyId;
		setKey.keyLength = keyLength;
		if (keyLength)
			qdf_mem_copy(setKey.Key, pKey, keyLength);
		status = csr_roam_issue_set_key_command(pMac, sessionId,
							&setKey, 0);
	}
	return status;
}

/**
 * csr_update_key_cmd() - update key info in set key command
 * @mac_ctx:         mac global context
 * @session:         roam session
 * @set_key:         input set key command
 * @set_key_cmd:     set key command to update
 * @is_key_valid:    indicates if key is valid
 *
 * This function will validate the key length, adjust if too long. It will
 * update is_key_valid flag to false if some error has occurred key are local.
 *
 * Return: status of operation
 */
static QDF_STATUS
csr_update_key_cmd(tpAniSirGlobal mac_ctx, struct csr_roam_session *session,
		   tCsrRoamSetKey *set_key, struct setkey_cmd *set_key_cmd,
		   bool *is_key_valid)
{
	switch (set_key->encType) {
	case eCSR_ENCRYPT_TYPE_WEP40:
	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
		/* KeyLength maybe 0 for static WEP */
		if (set_key->keyLength) {
			if (set_key->keyLength < CSR_WEP40_KEY_LEN) {
				sme_warn("Invalid WEP40 keylength [= %d]",
					set_key->keyLength);
				*is_key_valid = false;
				return QDF_STATUS_E_INVAL;
			}

			set_key_cmd->keyLength = CSR_WEP40_KEY_LEN;
			qdf_mem_copy(set_key_cmd->Key, set_key->Key,
				     CSR_WEP40_KEY_LEN);
		}
		*is_key_valid = true;
		break;
	case eCSR_ENCRYPT_TYPE_WEP104:
	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
		/* KeyLength maybe 0 for static WEP */
		if (set_key->keyLength) {
			if (set_key->keyLength < CSR_WEP104_KEY_LEN) {
				sme_warn("Invalid WEP104 keylength [= %d]",
					set_key->keyLength);
				*is_key_valid = false;
				return QDF_STATUS_E_INVAL;
			}

			set_key_cmd->keyLength = CSR_WEP104_KEY_LEN;
			qdf_mem_copy(set_key_cmd->Key, set_key->Key,
				     CSR_WEP104_KEY_LEN);
		}
		*is_key_valid = true;
		break;
	case eCSR_ENCRYPT_TYPE_TKIP:
		if (set_key->keyLength < CSR_TKIP_KEY_LEN) {
			sme_warn("Invalid TKIP keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_TKIP_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_TKIP_KEY_LEN);
		*is_key_valid = true;
		break;
	case eCSR_ENCRYPT_TYPE_AES:
		if (set_key->keyLength < CSR_AES_KEY_LEN) {
			sme_warn("Invalid AES/CCMP keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_AES_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_AES_KEY_LEN);
		*is_key_valid = true;
		break;
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
		if (set_key->keyLength < CSR_AES_GCMP_KEY_LEN) {
			sme_warn(
				"Invalid AES_GCMP keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_AES_GCMP_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_AES_GCMP_KEY_LEN);
		*is_key_valid = true;
		break;
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
		if (set_key->keyLength < CSR_AES_GCMP_256_KEY_LEN) {
			sme_warn(
				"Invalid AES_GCMP_256 keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_AES_GCMP_256_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_AES_GCMP_256_KEY_LEN);
		*is_key_valid = true;
		break;
#ifdef FEATURE_WLAN_WAPI
	case eCSR_ENCRYPT_TYPE_WPI:
		if (set_key->keyLength < CSR_WAPI_KEY_LEN) {
			sme_warn("Invalid WAPI keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_WAPI_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_WAPI_KEY_LEN);
		if (session->pCurRoamProfile) {
			session->pCurRoamProfile->negotiatedUCEncryptionType =
				eCSR_ENCRYPT_TYPE_WPI;
		} else {
			sme_err("pCurRoamProfile is NULL");
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		*is_key_valid = true;
		break;
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_ESE
	case eCSR_ENCRYPT_TYPE_KRK:
		/* no need to enqueue KRK key request, since they are local */
		*is_key_valid = false;
		if (set_key->keyLength < CSR_KRK_KEY_LEN) {
			sme_warn("Invalid KRK keylength [= %d]",
				set_key->keyLength);
			return QDF_STATUS_E_INVAL;
		}
		qdf_mem_copy(session->eseCckmInfo.krk, set_key->Key,
			     CSR_KRK_KEY_LEN);
		session->eseCckmInfo.reassoc_req_num = 1;
		session->eseCckmInfo.krk_plumbed = true;
		break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case eCSR_ENCRYPT_TYPE_BTK:
		/* no need to enqueue KRK key request, since they are local */
		*is_key_valid = false;
		if (set_key->keyLength < SIR_BTK_KEY_LEN) {
			sme_warn("LFR3:Invalid BTK keylength [= %d]",
				set_key->keyLength);
			return QDF_STATUS_E_INVAL;
		}
		qdf_mem_copy(session->eseCckmInfo.btk, set_key->Key,
			     SIR_BTK_KEY_LEN);
		/*
		 * KRK and BTK are updated by upper layer back to back. Send
		 * updated KRK and BTK together to FW here.
		 */
		csr_roam_offload_scan(mac_ctx, session->sessionId,
				      ROAM_SCAN_OFFLOAD_UPDATE_CFG,
				      REASON_ROAM_PSK_PMK_CHANGED);
		break;
#endif
#endif /* FEATURE_WLAN_ESE */
#ifdef WLAN_FEATURE_11W
	/* Check for 11w BIP */
	case eCSR_ENCRYPT_TYPE_AES_CMAC:
		if (set_key->keyLength < CSR_AES_KEY_LEN) {
			sme_warn("Invalid AES/CCMP keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_AES_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_AES_KEY_LEN);
		*is_key_valid = true;
		break;

	case eCSR_ENCRYPT_TYPE_AES_GMAC_128:
		if (set_key->keyLength < CSR_AES_GMAC_128_KEY_LEN) {
			sme_warn("Invalid AES GMAC 128 keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_AES_GMAC_128_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_AES_GMAC_128_KEY_LEN);
		*is_key_valid = true;
		break;

	case eCSR_ENCRYPT_TYPE_AES_GMAC_256:
		if (set_key->keyLength < CSR_AES_GMAC_256_KEY_LEN) {
			sme_warn("Invalid AES GMAC 256 keylength [= %d]",
				set_key->keyLength);
			*is_key_valid = false;
			return QDF_STATUS_E_INVAL;
		}
		set_key_cmd->keyLength = CSR_AES_GMAC_256_KEY_LEN;
		qdf_mem_copy(set_key_cmd->Key, set_key->Key,
			     CSR_AES_GMAC_256_KEY_LEN);
		*is_key_valid = true;
		break;

#endif /* WLAN_FEATURE_11W */
	default:
		/* for open security also we want to enqueue command */
		*is_key_valid = true;
		return QDF_STATUS_SUCCESS;
	} /* end of switch */
	return QDF_STATUS_SUCCESS;
}


static QDF_STATUS csr_roam_issue_set_key_command(
tpAniSirGlobal mac_ctx, uint32_t session_id,
		 tCsrRoamSetKey *set_key,
		 uint32_t roam_id)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	bool is_key_valid = true;
	struct setkey_cmd set_key_cmd;
#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_WAPI)
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (NULL == session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "session %d not found", session_id);
		return QDF_STATUS_E_FAILURE;
	}
#endif /* FEATURE_WLAN_ESE */

	qdf_mem_zero(&set_key_cmd, sizeof(struct setkey_cmd));
	/*
	 * following function will validate the key length, Adjust if too long.
	 * for static WEP the keys are not set thru' SetContextReq
	 *
	 * it will update bool is_key_valid, to false if some error has occurred
	 * key are local. enqueue sme command only if is_key_valid is true
	 * status is indication of success or failure and will be returned to
	 * called of current function if command is not enqueued due to key req
	 * being local
	 */
	status = csr_update_key_cmd(mac_ctx, session, set_key,
				    &set_key_cmd, &is_key_valid);
	if (is_key_valid) {
		set_key_cmd.roamId = roam_id;
		set_key_cmd.encType = set_key->encType;
		set_key_cmd.keyDirection = set_key->keyDirection;
		qdf_copy_macaddr(&set_key_cmd.peermac,
				 &set_key->peerMac);
		/* 0 for supplicant */
		set_key_cmd.paeRole = set_key->paeRole;
		set_key_cmd.keyId = set_key->keyId;
		qdf_mem_copy(set_key_cmd.keyRsc, set_key->keyRsc,
			     CSR_MAX_RSC_LEN);
		/*
		 * Always put set key to the head of the Q because it is the
		 * only thing to get executed in case of WT_KEY state
		 */
		sme_debug("set key req for session-%d authtype-%d",
			session_id, set_key->encType);
		status = csr_roam_send_set_key_cmd(mac_ctx, session_id,
						&set_key_cmd);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status = %d", status);
	}
	return status;
}

QDF_STATUS csr_roam_send_set_key_cmd(tpAniSirGlobal mac_ctx,
				uint32_t session_id,
				struct setkey_cmd *set_key_cmd)
{
	QDF_STATUS status;
	uint8_t num_keys = (set_key_cmd->keyLength) ? 1 : 0;
	tAniEdType ed_type = csr_translate_encrypt_type_to_ed_type(
						set_key_cmd->encType);
	bool unicast = (set_key_cmd->peermac.bytes[0] == 0xFF) ? false : true;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	WLAN_HOST_DIAG_EVENT_DEF(setKeyEvent,
				 host_event_wlan_security_payload_type);

	if (NULL == session) {
		sme_err("session %d not found", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (eSIR_ED_NONE != ed_type) {
		qdf_mem_zero(&setKeyEvent,
			sizeof(host_event_wlan_security_payload_type));
		if (qdf_is_macaddr_group(&set_key_cmd->peermac)) {
			setKeyEvent.eventId = WLAN_SECURITY_EVENT_SET_BCAST_REQ;
			setKeyEvent.encryptionModeMulticast =
				(uint8_t) diag_enc_type_from_csr_type(
					set_key_cmd->encType);
			setKeyEvent.encryptionModeUnicast =
				(uint8_t) diag_enc_type_from_csr_type(session->
							connectedProfile.
							EncryptionType);
		} else {
			setKeyEvent.eventId =
				WLAN_SECURITY_EVENT_SET_UNICAST_REQ;
			setKeyEvent.encryptionModeUnicast =
				(uint8_t) diag_enc_type_from_csr_type(
					set_key_cmd->encType);
			setKeyEvent.encryptionModeMulticast =
				(uint8_t) diag_enc_type_from_csr_type(session->
							connectedProfile.
							mcEncryptionType);
		}
		qdf_mem_copy(setKeyEvent.bssid,
			     session->connectedProfile.bssid.bytes,
			     QDF_MAC_ADDR_SIZE);
		if (CSR_IS_ENC_TYPE_STATIC(set_key_cmd->encType)) {
			uint32_t defKeyId;
			/* It has to be static WEP here */
			if (QDF_IS_STATUS_SUCCESS(wlan_cfg_get_int(mac_ctx,
					WNI_CFG_WEP_DEFAULT_KEYID,
					&defKeyId))) {
				setKeyEvent.keyId = (uint8_t) defKeyId;
			}
		} else {
			setKeyEvent.keyId = set_key_cmd->keyId;
		}
		setKeyEvent.authMode =
			(uint8_t) diag_auth_type_from_csr_type(session->
							       connectedProfile.
							       AuthType);
		WLAN_HOST_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	if (csr_is_set_key_allowed(mac_ctx, session_id)) {
		status = csr_send_mb_set_context_req_msg(mac_ctx, session_id,
					set_key_cmd->peermac,
					num_keys, ed_type, unicast,
					set_key_cmd->keyDirection,
					set_key_cmd->keyId,
					set_key_cmd->keyLength,
					set_key_cmd->Key,
					set_key_cmd->paeRole,
					set_key_cmd->keyRsc);
	} else {
		sme_warn(" cannot process not connected");
		/* Set this status so the error handling take
		 * care of the case.
		 */
		status = QDF_STATUS_CSR_WRONG_STATE;
	}
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("  error status %d", status);
		csr_roam_call_callback(mac_ctx, session_id, NULL,
				       set_key_cmd->roamId,
				       eCSR_ROAM_SET_KEY_COMPLETE,
				       eCSR_ROAM_RESULT_FAILURE);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
		if (eSIR_ED_NONE != ed_type) {
			if (qdf_is_macaddr_group(&set_key_cmd->peermac))
				setKeyEvent.eventId =
					WLAN_SECURITY_EVENT_SET_BCAST_RSP;
			else
				setKeyEvent.eventId =
					WLAN_SECURITY_EVENT_SET_UNICAST_RSP;
			setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
			WLAN_HOST_DIAG_EVENT_REPORT(&setKeyEvent,
						    EVENT_WLAN_SECURITY);
		}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	}
	return status;
}

QDF_STATUS csr_roam_set_key(tpAniSirGlobal pMac, uint32_t sessionId,
			    tCsrRoamSetKey *pSetKey, uint32_t roamId)
{
	QDF_STATUS status;

	if (!csr_is_set_key_allowed(pMac, sessionId)) {
		status = QDF_STATUS_CSR_WRONG_STATE;
	} else {
		status = csr_roam_issue_set_key_command(pMac, sessionId,
							pSetKey, roamId);
	}
	return status;
}

#ifdef WLAN_FEATURE_FILS_SK
/*
 * csr_create_fils_realm_hash: API to create hash using realm
 * @fils_con_info: fils connection info obtained from supplicant
 * @tmp_hash: pointer to new hash
 *
 * Return: None
 */
static bool
csr_create_fils_realm_hash(struct cds_fils_connection_info *fils_con_info,
			   uint8_t *tmp_hash)
{
	uint8_t *hash;
	uint8_t *data[1];

	if (!fils_con_info->realm_len)
		return false;

	hash = qdf_mem_malloc(SHA256_DIGEST_SIZE);
	if (!hash) {
		sme_err("malloc fails in fils realm");
		return false;
	}

	data[0] = fils_con_info->realm;
	qdf_get_hash(SHA256_CRYPTO_TYPE, 1, data,
			&fils_con_info->realm_len, hash);
	qdf_trace_hex_dump(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG,
				   hash, SHA256_DIGEST_SIZE);
	qdf_mem_copy(tmp_hash, hash, 2);
	qdf_mem_free(hash);
	return true;
}

/*
 * csr_update_fils_scan_filter: update scan filter in case of fils session
 * @scan_fltr: pointer to scan filer
 * @profile: csr profile pointer
 *
 * Return: None
 */
static void csr_update_fils_scan_filter(tCsrScanResultFilter *scan_fltr,
				struct csr_roam_profile *profile)
{
	if (profile->fils_con_info &&
	    profile->fils_con_info->is_fils_connection) {
		uint8_t realm_hash[2];

		sme_debug("creating realm based on fils info %d",
			profile->fils_con_info->is_fils_connection);
		scan_fltr->realm_check =  csr_create_fils_realm_hash(
				profile->fils_con_info, realm_hash);
		memcpy(scan_fltr->fils_realm, realm_hash,
			sizeof(uint8_t) * 2);
	}

}
#else
static void csr_update_fils_scan_filter(tCsrScanResultFilter *scan_fltr,
				struct csr_roam_profile *profile)
{ }
#endif

/*
 * Prepare a filter base on a profile for parsing the scan results.
 * Upon successful return, caller MUST call csr_free_scan_filter on
 *pScanFilter when it is done with the filter.
 */
QDF_STATUS
csr_roam_prepare_filter_from_profile(tpAniSirGlobal mac_ctx,
				     struct csr_roam_profile *profile,
				     tCsrScanResultFilter *scan_fltr)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t size = 0;
	uint8_t idx = 0;
	tCsrChannelInfo *fltr_ch_info = &scan_fltr->ChannelInfo;
	tCsrChannelInfo *profile_ch_info = &profile->ChannelInfo;
	struct roam_ext_params *roam_params;
	uint8_t i;

	roam_params = &mac_ctx->roam.configParam.roam_params;

	if (profile->BSSIDs.numOfBSSIDs) {
		size = sizeof(struct qdf_mac_addr) * profile->BSSIDs.
							numOfBSSIDs;
		scan_fltr->BSSIDs.bssid = qdf_mem_malloc(size);
		if (NULL == scan_fltr->BSSIDs.bssid) {
			status = QDF_STATUS_E_NOMEM;
			goto free_filter;
		}
		scan_fltr->BSSIDs.numOfBSSIDs = profile->BSSIDs.numOfBSSIDs;
		qdf_mem_copy(scan_fltr->BSSIDs.bssid,
			     profile->BSSIDs.bssid, size);
	}

	if (profile->SSIDs.numOfSSIDs) {
		scan_fltr->SSIDs.numOfSSIDs = profile->SSIDs.numOfSSIDs;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("No of Allowed List:%d"),
			roam_params->num_ssid_allowed_list);
		if (scan_fltr->scan_filter_for_roam
			&& roam_params->num_ssid_allowed_list) {
			scan_fltr->SSIDs.numOfSSIDs =
				roam_params->num_ssid_allowed_list;
			size = sizeof(tCsrSSIDInfo) *
				scan_fltr->SSIDs.numOfSSIDs;
			scan_fltr->SSIDs.SSIDList = qdf_mem_malloc(size);
			if (NULL == scan_fltr->SSIDs.SSIDList)
				status = QDF_STATUS_E_FAILURE;
			else
				status = QDF_STATUS_SUCCESS;
			if (!QDF_IS_STATUS_SUCCESS(status))
				goto free_filter;
			for  (i = 0;
				i < roam_params->num_ssid_allowed_list;
				i++) {
				qdf_mem_copy((void *)
				    scan_fltr->SSIDs.SSIDList[i].SSID.ssId,
				    roam_params->ssid_allowed_list[i].ssId,
				    roam_params->ssid_allowed_list[i].length);
				scan_fltr->SSIDs.SSIDList[i].SSID.length =
				    roam_params->ssid_allowed_list[i].length;
				scan_fltr->SSIDs.SSIDList[i].handoffPermitted =
					1;
				scan_fltr->SSIDs.SSIDList[i].ssidHidden = 0;
			}
		} else {
			size = sizeof(tCsrSSIDInfo) *
				profile->SSIDs.numOfSSIDs;
			scan_fltr->SSIDs.SSIDList = qdf_mem_malloc(size);
			if (NULL == scan_fltr->SSIDs.SSIDList) {
				status = QDF_STATUS_E_NOMEM;
				goto free_filter;
			}
			qdf_mem_copy(scan_fltr->SSIDs.SSIDList,
					profile->SSIDs.SSIDList, size);
		}
	}

	if (!profile_ch_info->ChannelList
	    || (profile_ch_info->ChannelList[0] == 0)) {
		fltr_ch_info->numOfChannels = 0;
		fltr_ch_info->ChannelList = NULL;
	} else if (profile_ch_info->numOfChannels) {
		fltr_ch_info->numOfChannels = 0;
		fltr_ch_info->ChannelList =
			qdf_mem_malloc(sizeof(*(fltr_ch_info->ChannelList)) *
				       profile_ch_info->numOfChannels);
		if (NULL == fltr_ch_info->ChannelList) {
			status = QDF_STATUS_E_NOMEM;
			goto free_filter;
		}

		for (idx = 0; idx < profile_ch_info->numOfChannels; idx++) {
			if (csr_roam_is_channel_valid(mac_ctx,
				profile_ch_info->ChannelList[idx])) {
				fltr_ch_info->
				ChannelList[fltr_ch_info->numOfChannels]
					= profile_ch_info->ChannelList[idx];
				fltr_ch_info->numOfChannels++;
			} else {
				sme_debug(
					"Channel (%d) is invalid",
					profile_ch_info->ChannelList[idx]);
			}
		}
	} else {
		sme_err("Channel list empty");
		status = QDF_STATUS_E_FAILURE;
		goto free_filter;
	}
	scan_fltr->uapsd_mask = profile->uapsd_mask;
	scan_fltr->authType = profile->AuthType;
	scan_fltr->EncryptionType = profile->EncryptionType;
	scan_fltr->mcEncryptionType = profile->mcEncryptionType;
	scan_fltr->BSSType = profile->BSSType;
	scan_fltr->phyMode = profile->phyMode;
#ifdef FEATURE_WLAN_WAPI
	/*
	 * check if user asked for WAPI with 11n or auto mode, in that
	 * case modify the phymode to 11g
	 */
	if (csr_is_profile_wapi(profile)) {
		if (scan_fltr->phyMode & eCSR_DOT11_MODE_11n)
			scan_fltr->phyMode &= ~eCSR_DOT11_MODE_11n;
		if (scan_fltr->phyMode & eCSR_DOT11_MODE_AUTO)
			scan_fltr->phyMode &= ~eCSR_DOT11_MODE_AUTO;
		if (!scan_fltr->phyMode)
			scan_fltr->phyMode = eCSR_DOT11_MODE_11g;
	}
#endif /* FEATURE_WLAN_WAPI */
	/*Save the WPS info */
	scan_fltr->bWPSAssociation = profile->bWPSAssociation;
	scan_fltr->bOSENAssociation = profile->bOSENAssociation;
	if (profile->countryCode[0]) {
		/*
		 * This causes the matching function to use countryCode as one
		 * of the criteria.
		 */
		qdf_mem_copy(scan_fltr->countryCode, profile->countryCode,
			     WNI_CFG_COUNTRY_CODE_LEN);
	}
	if (profile->MDID.mdiePresent) {
		scan_fltr->MDID.mdiePresent = 1;
		scan_fltr->MDID.mobilityDomain = profile->MDID.mobilityDomain;
	}
	qdf_mem_copy(scan_fltr->bssid_hint.bytes,
		profile->bssid_hint.bytes, QDF_MAC_ADDR_SIZE);

#ifdef WLAN_FEATURE_11W
	/* Management Frame Protection */
	scan_fltr->MFPEnabled = profile->MFPEnabled;
	scan_fltr->MFPRequired = profile->MFPRequired;
	scan_fltr->MFPCapable = profile->MFPCapable;
#endif
	scan_fltr->csrPersona = profile->csrPersona;
	csr_update_fils_scan_filter(scan_fltr, profile);
	scan_fltr->force_rsne_override = profile->force_rsne_override;

free_filter:
	if (!QDF_IS_STATUS_SUCCESS(status))
		csr_free_scan_filter(mac_ctx, scan_fltr);

	return status;
}

static
bool csr_roam_issue_wm_status_change(tpAniSirGlobal pMac, uint32_t sessionId,
				     enum csr_roam_wmstatus_changetypes Type,
				     tSirSmeRsp *pSmeRsp)
{
	bool fCommandQueued = false;
	tSmeCmd *pCommand;

	do {
		/* Validate the type is ok... */
		if ((eCsrDisassociated != Type)
		    && (eCsrDeauthenticated != Type))
			break;
		pCommand = csr_get_command_buffer(pMac);
		if (!pCommand) {
			sme_err(" fail to get command buffer");
			break;
		}
		/* Change the substate in case it is waiting for key */
		if (CSR_IS_WAIT_FOR_KEY(pMac, sessionId)) {
			csr_roam_stop_wait_for_key_timer(pMac);
			csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
		pCommand->command = eSmeCommandWmStatusChange;
		pCommand->sessionId = (uint8_t) sessionId;
		pCommand->u.wmStatusChangeCmd.Type = Type;
		if (eCsrDisassociated == Type) {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DisassocIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DisassocIndMsg));
		} else {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DeauthIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DeauthIndMsg));
		}
		if (QDF_IS_STATUS_SUCCESS
			    (csr_queue_sme_command(pMac, pCommand, false)))
			fCommandQueued = true;
		else
			sme_err("fail to send message");

		/* AP has issued Dissac/Deauth, Set the operating mode
		 * value to configured value
		 */
		csr_set_default_dot11_mode(pMac);
	} while (0);
	return fCommandQueued;
}

static QDF_STATUS csr_send_snr_request(void *pGetRssiReq)
{
	void *wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_STATUS_SUCCESS !=
		wma_send_snr_request(wma_handle, pGetRssiReq)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"Failed to Trigger wma stats request");
		return QDF_STATUS_E_FAILURE;
	}

	/* dont send success, otherwise call back
	 * will released with out values
	 */
	return QDF_STATUS_E_BUSY;
}

static void csr_update_rssi(tpAniSirGlobal pMac, void *pMsg)
{
	int8_t rssi = 0;
	tAniGetRssiReq *pGetRssiReq = (tAniGetRssiReq *) pMsg;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (pGetRssiReq) {
		qdf_status = csr_send_snr_request(pGetRssiReq);

		if (NULL != pGetRssiReq->rssiCallback) {
			if (qdf_status != QDF_STATUS_E_BUSY)
				((tCsrRssiCallback) (pGetRssiReq->rssiCallback))
					(rssi, pGetRssiReq->staId,
					pGetRssiReq->pDevContext);
			else
				sme_debug("rssi request is posted. waiting for reply");
		} else {
			sme_err("GetRssiReq->rssiCallback is NULL");
			return;
		}
	} else
		sme_err("pGetRssiReq is NULL");

}

static void csr_update_snr(tpAniSirGlobal pMac, void *pMsg)
{
	tAniGetSnrReq *pGetSnrReq = (tAniGetSnrReq *) pMsg;

	if (pGetSnrReq) {
		if (QDF_STATUS_SUCCESS != wma_get_snr(pGetSnrReq)) {
			sme_err("Error in wma_get_snr");
			return;
		}

	} else
		sme_err("pGetSnrReq is NULL");
}

static QDF_STATUS csr_send_reset_ap_caps_changed(tpAniSirGlobal pMac,
				struct qdf_mac_addr *bssId)
{
	tpSirResetAPCapsChange pMsg;
	uint16_t len;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Create the message and send to lim */
	len = sizeof(tSirResetAPCapsChange);
	pMsg = qdf_mem_malloc(len);
	if (NULL == pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;

	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMsg->messageType = eWNI_SME_RESET_AP_CAPS_CHANGED;
		pMsg->length = len;
		qdf_copy_macaddr(&pMsg->bssId, bssId);
		sme_debug(
			"CSR reset caps change for Bssid= " MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(pMsg->bssId.bytes));
		status = umac_send_mb_message_to_mac(pMsg);
	} else {
		sme_err("Memory allocation failed");
	}
	return status;
}

#ifdef FEATURE_WLAN_ESE
/**
 * csr_convert_ese_akm_to_ani() - Convert eCsrAuthType ESE akm value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type csr_convert_ese_akm_to_ani(eCsrAuthType akm_type)
{
	switch (akm_type) {
	case eCSR_AUTH_TYPE_CCKM_RSN:
		return ANI_AKM_TYPE_CCKM;
	default:
		return ANI_AKM_TYPE_UNKNOWN;
	}
}
#else
static inline enum
ani_akm_type csr_convert_ese_akm_to_ani(eCsrAuthType akm_type)
{
	return ANI_AKM_TYPE_UNKNOWN;
}
#endif

#ifdef WLAN_FEATURE_SAE
/**
 * csr_convert_sae_akm_to_ani() - Convert eCsrAuthType SAE akm value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type csr_convert_sae_akm_to_ani(eCsrAuthType akm_type)
{
	switch (akm_type) {
	case eCSR_AUTH_TYPE_SAE:
		return ANI_AKM_TYPE_SAE;
	case eCSR_AUTH_TYPE_FT_SAE:
		return ANI_AKM_TYPE_FT_SAE;
	default:
		return ANI_AKM_TYPE_UNKNOWN;
	}
}
#else
static inline
enum ani_akm_type csr_convert_sae_akm_to_ani(eCsrAuthType akm_type)
{
	return ANI_AKM_TYPE_UNKNOWN;
}
#endif

/**
 * csr_convert_csr_to_ani_akm_type() - Convert eCsrAuthType value to equivalent
 * enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type csr_convert_csr_to_ani_akm_type(eCsrAuthType akm_type)
{
	enum ani_akm_type ani_akm;

	switch (akm_type) {
	case eCSR_AUTH_TYPE_NONE:
		return ANI_AKM_TYPE_NONE;
	case eCSR_AUTH_TYPE_WPA:
		return ANI_AKM_TYPE_WPA;
	case eCSR_AUTH_TYPE_WPA_PSK:
		return ANI_AKM_TYPE_WPA_PSK;
	case eCSR_AUTH_TYPE_RSN:
		return ANI_AKM_TYPE_RSN;
	case eCSR_AUTH_TYPE_RSN_PSK:
		return ANI_AKM_TYPE_RSN_PSK;
	case eCSR_AUTH_TYPE_FT_RSN:
		return ANI_AKM_TYPE_FT_RSN;
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		return ANI_AKM_TYPE_FT_RSN_PSK;
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		return ANI_AKM_TYPE_RSN_PSK_SHA256;
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		return ANI_AKM_TYPE_RSN_8021X_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA256:
		return ANI_AKM_TYPE_FILS_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA384:
		return ANI_AKM_TYPE_FILS_SHA384;
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		return ANI_AKM_TYPE_FT_FILS_SHA256;
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		return ANI_AKM_TYPE_FT_FILS_SHA384;
	case eCSR_AUTH_TYPE_DPP_RSN:
		return ANI_AKM_TYPE_DPP_RSN;
	case eCSR_AUTH_TYPE_OWE:
		return ANI_AKM_TYPE_OWE;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		return ANI_AKM_TYPE_SUITEB_EAP_SHA256;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		return ANI_AKM_TYPE_SUITEB_EAP_SHA384;
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return ANI_AKM_TYPE_FT_SUITEB_EAP_SHA384;
	default:
		ani_akm = ANI_AKM_TYPE_UNKNOWN;
	}

	if (ani_akm == ANI_AKM_TYPE_UNKNOWN)
		ani_akm = csr_convert_sae_akm_to_ani(akm_type);

	if (ani_akm == ANI_AKM_TYPE_UNKNOWN)
		ani_akm = csr_convert_ese_akm_to_ani(akm_type);

	return ani_akm;
}

static eCsrAuthType csr_translate_akm_type(enum ani_akm_type akm_type)
{
	eCsrAuthType csr_akm_type;

	switch (akm_type) {
	case ANI_AKM_TYPE_NONE:
		csr_akm_type = eCSR_AUTH_TYPE_NONE;
		break;
#ifdef WLAN_FEATURE_SAE
	case ANI_AKM_TYPE_SAE:
		csr_akm_type = eCSR_AUTH_TYPE_SAE;
		break;
#endif
	case ANI_AKM_TYPE_WPA:
		csr_akm_type = eCSR_AUTH_TYPE_WPA;
		break;
	case ANI_AKM_TYPE_WPA_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_WPA_PSK;
		break;
	case ANI_AKM_TYPE_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_RSN;
		break;
	case ANI_AKM_TYPE_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK;
		break;
	case ANI_AKM_TYPE_FT_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN;
		break;
	case ANI_AKM_TYPE_FT_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN_PSK;
		break;
#ifdef FEATURE_WLAN_ESE
	case ANI_AKM_TYPE_CCKM:
		csr_akm_type = eCSR_AUTH_TYPE_CCKM_RSN;
		break;
#endif
	case ANI_AKM_TYPE_RSN_PSK_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK_SHA256;
		break;
	case ANI_AKM_TYPE_RSN_8021X_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_8021X_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_DPP_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_DPP_RSN;
		break;
	case ANI_AKM_TYPE_OWE:
		csr_akm_type = eCSR_AUTH_TYPE_OWE;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA256;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA384;
		break;
	default:
		csr_akm_type = eCSR_AUTH_TYPE_UNKNOWN;
	}

	return csr_akm_type;
}

static bool csr_is_sae_akm_present(tDot11fIERSN * const rsn_ie)
{
	uint16_t i;

	if (rsn_ie->akm_suite_cnt > 6) {
		sme_debug("Invalid akm_suite_cnt in Rx RSN IE");
		return false;
	}

	for (i = 0; i < rsn_ie->akm_suite_cnt; i++) {
		if (LE_READ_4(rsn_ie->akm_suite[i]) == RSN_AUTH_KEY_MGMT_SAE) {
			sme_debug("SAE AKM present");
			return true;
		}
	}
	return false;
}

static bool csr_is_sae_peer_allowed(tpAniSirGlobal mac_ctx,
				    tSirSmeAssocInd *assoc_ind,
				    struct csr_roam_session *session,
				    tSirMacAddr peer_mac_addr,
				    tDot11fIERSN *rsn_ie,
				    tSirMacStatusCodes *mac_status_code)
{
	bool is_allowed = false;

	/* Allow the peer if it's SAE authenticated */
	if (assoc_ind->is_sae_authenticated)
		return true;

	/* Allow the peer with valid PMKID */
	if (!rsn_ie->pmkid_count) {
		*mac_status_code = eSIR_MAC_AUTH_ALGO_NOT_SUPPORTED_STATUS;
		sme_debug("No PMKID present in RSNIE; Tried to use SAE AKM after non-SAE authentication");
	} else if (csr_is_pmkid_found_for_peer(mac_ctx, session, peer_mac_addr,
					       &rsn_ie->pmkid[0][0],
					       rsn_ie->pmkid_count)) {
		sme_debug("Valid PMKID found for SAE peer");
		is_allowed = true;
	} else {
		*mac_status_code = eSIR_MAC_INVALID_PMKID;
		sme_debug("No valid PMKID found for SAE peer");
	}

	return is_allowed;
}

static void
csr_roam_chk_lnk_assoc_ind(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tSirSmeAssocInd *pAssocInd;
	tSirMacStatusCodes mac_status_code = eSIR_MAC_SUCCESS_STATUS;
	eCsrAuthType csr_akm_type;

	sme_debug("Receive WNI_SME_ASSOC_IND from SME");
	pAssocInd = (tSirSmeAssocInd *) msg_ptr;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				(struct qdf_mac_addr *) pAssocInd->bssId,
				&sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	csr_akm_type = csr_translate_akm_type(pAssocInd->akm_type);

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	/* Required for indicating the frames to upper layer */
	roam_info->assocReqLength = pAssocInd->assocReqLength;
	roam_info->assocReqPtr = pAssocInd->assocReqPtr;
	roam_info->beaconPtr = pAssocInd->beaconPtr;
	roam_info->beaconLength = pAssocInd->beaconLength;
	roam_info->statusCode = eSIR_SME_SUCCESS;
	roam_info->u.pConnectedProfile = &session->connectedProfile;
	roam_info->staId = (uint8_t)pAssocInd->staId;
	roam_info->rsnIELen = (uint8_t)pAssocInd->rsnIE.length;
	roam_info->prsnIE = pAssocInd->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
	roam_info->wapiIELen = (uint8_t)pAssocInd->wapiIE.length;
	roam_info->pwapiIE = pAssocInd->wapiIE.wapiIEdata;
#endif
	roam_info->addIELen = (uint8_t)pAssocInd->addIE.length;
	roam_info->paddIE = pAssocInd->addIE.addIEdata;
	qdf_mem_copy(roam_info->peerMac.bytes,
		     pAssocInd->peerMacAddr,
		     sizeof(tSirMacAddr));
	qdf_mem_copy(roam_info->bssid.bytes,
		     pAssocInd->bssId,
		     sizeof(struct qdf_mac_addr));
	roam_info->wmmEnabledSta = pAssocInd->wmmEnabledSta;
	roam_info->timingMeasCap = pAssocInd->timingMeasCap;
	roam_info->ecsa_capable = pAssocInd->ecsa_capable;
	qdf_mem_copy(&roam_info->chan_info,
		     &pAssocInd->chan_info,
		     sizeof(tSirSmeChanInfo));

	if (pAssocInd->HTCaps.present)
		qdf_mem_copy(&roam_info->ht_caps,
			     &pAssocInd->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	if (pAssocInd->VHTCaps.present)
		qdf_mem_copy(&roam_info->vht_caps,
			     &pAssocInd->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));

	roam_info->capability_info = pAssocInd->capability_info;
	roam_info->he_caps_present = pAssocInd->he_caps_present;

	if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile)) {
		if (session->pCurRoamProfile &&
		    CSR_IS_ENC_TYPE_STATIC(
			session->pCurRoamProfile->negotiatedUCEncryptionType)) {
			/* NO keys... these key parameters don't matter. */
			csr_roam_issue_set_context_req(mac_ctx,
				sessionId,
				session->pCurRoamProfile->
				negotiatedUCEncryptionType,
				session->pConnectBssDesc,
				&roam_info->peerMac.bytes,
				false, true, eSIR_TX_RX, 0, 0, NULL, 0);
			roam_info->fAuthRequired = false;
		} else {
			roam_info->fAuthRequired = true;
		}
		if (csr_akm_type == eCSR_AUTH_TYPE_OWE) {
			roam_info->owe_pending_assoc_ind = qdf_mem_malloc(
							    sizeof(*pAssocInd));
			if (roam_info->owe_pending_assoc_ind)
				qdf_mem_copy(roam_info->owe_pending_assoc_ind,
					     pAssocInd, sizeof(*pAssocInd));
		}
		status = csr_roam_call_callback(mac_ctx, sessionId,
					roam_info, 0, eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* Refused due to Mac filtering */
			roam_info->statusCode = eSIR_SME_ASSOC_REFUSED;
		} else if (pAssocInd->rsnIE.length && SIR_MAC_RSN_EID ==
			   pAssocInd->rsnIE.rsnIEdata[0]) {
			tDot11fIERSN rsn_ie = {0};

			if (dot11f_unpack_ie_rsn(mac_ctx,
						 pAssocInd->rsnIE.rsnIEdata + 2,
						 pAssocInd->rsnIE.length - 2,
						 &rsn_ie, false)
			    != DOT11F_PARSE_SUCCESS ||
			    (csr_is_sae_akm_present(&rsn_ie) &&
			     !csr_is_sae_peer_allowed(mac_ctx, pAssocInd,
						      session,
						      pAssocInd->peerMacAddr,
						      &rsn_ie,
						      &mac_status_code))) {
				status = QDF_STATUS_E_INVAL;
				roam_info->statusCode =
						eSIR_SME_ASSOC_REFUSED;
				sme_debug("SAE peer not allowed: Status: %d",
					  mac_status_code);
			}
		}
	}

	if (csr_akm_type != eCSR_AUTH_TYPE_OWE) {
		/* Send Association completion message to PE */
		status = csr_send_assoc_cnf_msg(mac_ctx, pAssocInd, status,
						mac_status_code);
		/*
		 * send a message to CSR itself just to avoid the EAPOL frames
		 * going OTA before association response
		 */
		if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile) &&
		    roam_info->statusCode != eSIR_SME_ASSOC_REFUSED) {
			roam_info->fReassocReq = pAssocInd->reassocReq;
			status = csr_send_assoc_ind_to_upper_layer_cnf_msg(
					mac_ctx, pAssocInd, status, sessionId);
		}
	}

	qdf_mem_free(roam_info);
}

/*
 * csr_is_deauth_disassoc_already_active() - Function to check if deauth or
 *  disassoc is already in progress.
 * @mac_ctx: Global MAC context
 * @session_id: session id
 * @peer_macaddr: Peer MAC address
 *
 * Return: True if deauth/disassoc indication can be dropped
 *  else false
 */
static bool csr_is_deauth_disassoc_already_active(tpAniSirGlobal mac_ctx,
					       uint8_t session_id,
					       struct qdf_mac_addr peer_macaddr)
{
	bool ret = false;
	tSmeCmd *sme_cmd;

	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, session_id,
						 WLAN_SER_CMD_FORCE_DEAUTH_STA);
	if (!sme_cmd) {
		sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc,
					       session_id,
					       WLAN_SER_CMD_FORCE_DISASSOC_STA);
		if (!sme_cmd)
			return ret;
	}

	if ((mac_ctx->roam.curSubState[session_id] ==
	     eCSR_ROAM_SUBSTATE_DEAUTH_REQ ||
	     mac_ctx->roam.curSubState[session_id] ==
	     eCSR_ROAM_SUBSTATE_DISASSOC_REQ) &&
	    !qdf_mem_cmp(peer_macaddr.bytes, sme_cmd->u.roamCmd.peerMac,
			 QDF_MAC_ADDR_SIZE)) {
		sme_debug("Ignore DEAUTH_IND/DIASSOC_IND as Deauth/Disassoc already in progress");
		ret = true;
	}

	return ret;
}

static void
csr_roam_chk_lnk_disassoc_ind(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	tSirSmeDisassocInd *pDisassocInd;
	tSmeCmd *cmd;

	cmd = qdf_mem_malloc(sizeof(*cmd));
	if (NULL == cmd) {
		sme_err("memory allocation failed for size: %zu", sizeof(*cmd));
		return;
	}

	/*
	 * Check if AP dis-associated us because of MIC failure. If so,
	 * then we need to take action immediately and not wait till the
	 * the WmStatusChange requests is pushed and processed
	 */
	pDisassocInd = (tSirSmeDisassocInd *) msg_ptr;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				&pDisassocInd->bssid, &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Session Id not found for BSSID "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(pDisassocInd->bssid.bytes));
		qdf_mem_free(cmd);
		return;
	}

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDisassocInd->peer_macaddr)) {
		qdf_mem_free(cmd);
		return;
	}

	sme_nofl_info("disassoc from peer " MAC_ADDRESS_STR
		      "reason: %d status: %d vid: %d",
		      MAC_ADDR_ARRAY(pDisassocInd->peer_macaddr.bytes),
		      pDisassocInd->reasonCode,
		      pDisassocInd->statusCode, sessionId);
	/*
	 * If we are in neighbor preauth done state then on receiving
	 * disassoc or deauth we dont roam instead we just disassoc
	 * from current ap and then go to disconnected state
	 * This happens for ESE and 11r FT connections ONLY.
	 */
	if (csr_roam_is11r_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#ifdef FEATURE_WLAN_ESE
	if (csr_roam_is_ese_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#endif
	if (csr_roam_is_fast_roam_enabled(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session: %d not found", sessionId);
		qdf_mem_free(cmd);
		return;
	}

	csr_update_scan_entry_associnfo(mac_ctx,
					session, SCAN_ENTRY_CON_STATE_NONE);

	/* Update the disconnect stats */
	session->disconnect_stats.disconnection_cnt++;
	session->disconnect_stats.disassoc_by_peer++;

	if (csr_is_conn_state_infra(mac_ctx, sessionId))
		session->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	sme_qos_csr_event_ind(mac_ctx, (uint8_t) sessionId,
			      SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
	csr_roam_link_down(mac_ctx, sessionId);
	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDisassociated, msg_ptr);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		/*
		 * STA/P2P client got  disassociated so remove any pending
		 * deauth commands in sme pending list
		 */
		cmd->command = eSmeCommandRoam;
		cmd->sessionId = (uint8_t) sessionId;
		cmd->u.roamCmd.roamReason = eCsrForcedDeauthSta;
		qdf_mem_copy(cmd->u.roamCmd.peerMac,
			     pDisassocInd->peer_macaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		csr_roam_remove_duplicate_command(mac_ctx, sessionId, cmd,
						  eCsrForcedDeauthSta);
	}
	qdf_mem_free(cmd);
}

static void
csr_roam_chk_lnk_deauth_ind(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	tSirSmeDeauthInd *pDeauthInd;

	pDeauthInd = (tpSirSmeDeauthInd) msg_ptr;
	sme_debug("DEAUTH Indication from MAC for vdev_id %d bssid %pM",
		  pDeauthInd->sessionId, pDeauthInd->bssid.bytes);
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
						   &pDeauthInd->bssid,
						   &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return;

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDeauthInd->peer_macaddr))
		return;
	/* If we are in neighbor preauth done state then on receiving
	 * disassoc or deauth we dont roam instead we just disassoc
	 * from current ap and then go to disconnected state
	 * This happens for ESE and 11r FT connections ONLY.
	 */
	if (csr_roam_is11r_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#ifdef FEATURE_WLAN_ESE
	if (csr_roam_is_ese_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#endif
	if (csr_roam_is_fast_roam_enabled(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}

	csr_update_scan_entry_associnfo(mac_ctx,
					session, SCAN_ENTRY_CON_STATE_NONE);
	/* Update the disconnect stats */
	switch (pDeauthInd->reasonCode) {
	case eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.peer_kickout++;
		break;
	case eSIR_MAC_UNSPEC_FAILURE_REASON:
	case eSIR_MAC_PREV_AUTH_NOT_VALID_REASON:
	case eSIR_MAC_DEAUTH_LEAVING_BSS_REASON:
	case eSIR_MAC_CLASS2_FRAME_FROM_NON_AUTH_STA_REASON:
	case eSIR_MAC_CLASS3_FRAME_FROM_NON_ASSOC_STA_REASON:
	case eSIR_MAC_STA_NOT_PRE_AUTHENTICATED_REASON:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.deauth_by_peer++;
		break;
	case eSIR_MAC_BEACON_MISSED:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.bmiss++;
		break;
	default:
		/* Unknown reason code */
		break;
	}

	if (csr_is_conn_state_infra(mac_ctx, sessionId))
		session->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	sme_qos_csr_event_ind(mac_ctx, (uint8_t) sessionId,
			      SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
	csr_roam_link_down(mac_ctx, sessionId);
	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDeauthenticated,
					msg_ptr);
}

static void
csr_roam_chk_lnk_swt_ch_ind(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	uint16_t ie_len;
	QDF_STATUS status;
	tpSirSmeSwitchChannelInd pSwitchChnInd;
	struct csr_roam_info *roam_info;
	tSirMacDsParamSetIE *ds_params_ie;
	tDot11fIEHTInfo *ht_info_ie;

	/* in case of STA, the SWITCH_CHANNEL originates from its AP */
	sme_debug("eWNI_SME_SWITCH_CHL_IND from SME");
	pSwitchChnInd = (tpSirSmeSwitchChannelInd) msg_ptr;
	/* Update with the new channel id. The channel id is hidden in the
	 * statusCode.
	 */
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pSwitchChnInd->bssid, &sessionId);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	session->connectedProfile.operationChannel =
			(uint8_t) pSwitchChnInd->newChannelId;

	/* Update the occupied channel list with the new switched channel */
	csr_init_occupied_channels_list(mac_ctx, sessionId);

	if (session->pConnectBssDesc) {
		session->pConnectBssDesc->channelId =
				(uint8_t) pSwitchChnInd->newChannelId;

		ie_len = csr_get_ielen_from_bss_description(
						session->pConnectBssDesc);
		ds_params_ie = (tSirMacDsParamSetIE *)wlan_get_ie_ptr_from_eid(
				DOT11F_EID_DSPARAMS,
				(uint8_t *)session->pConnectBssDesc->ieFields,
				ie_len);
		if (ds_params_ie)
			ds_params_ie->channelNumber =
				(uint8_t)pSwitchChnInd->newChannelId;

		ht_info_ie = (tDot11fIEHTInfo *)wlan_get_ie_ptr_from_eid(
				DOT11F_EID_HTINFO,
				(uint8_t *)session->pConnectBssDesc->ieFields,
				ie_len);
		if (ht_info_ie) {
			ht_info_ie->primaryChannel =
				(uint8_t)pSwitchChnInd->newChannelId;
			ht_info_ie->secondaryChannelOffset =
				pSwitchChnInd->chan_params.sec_ch_offset;
		}
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roam_info->chan_info.chan_id = pSwitchChnInd->newChannelId;
	roam_info->chan_info.ch_width = pSwitchChnInd->chan_params.ch_width;
	roam_info->chan_info.sec_ch_offset =
				pSwitchChnInd->chan_params.sec_ch_offset;
	roam_info->chan_info.band_center_freq1 =
				pSwitchChnInd->chan_params.center_freq_seg0;
	roam_info->chan_info.band_center_freq2 =
				pSwitchChnInd->chan_params.center_freq_seg1;

	if (CSR_IS_PHY_MODE_11ac(mac_ctx->roam.configParam.phyMode))
		roam_info->mode = SIR_SME_PHY_MODE_VHT;
	else if (CSR_IS_PHY_MODE_11n(mac_ctx->roam.configParam.phyMode))
		roam_info->mode = SIR_SME_PHY_MODE_HT;
	else
		roam_info->mode = SIR_SME_PHY_MODE_LEGACY;

	status = csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
					eCSR_ROAM_STA_CHANNEL_SWITCH,
					eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_deauth_rsp(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tSirSmeDeauthRsp *pDeauthRsp = (tSirSmeDeauthRsp *) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DEAUTH_RSP from SME");
	sessionId = pDeauthRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->statusCode = pDeauthRsp->statusCode;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info, 0,
						eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_disassoc_rsp(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	/*
	 * session id is invalid here so cant use it to access the array
	 * curSubstate as index
	 */
	tSirSmeDisassocRsp *pDisassocRsp = (tSirSmeDisassocRsp *) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DISASSOC_RSP from SME ");
	sessionId = pDisassocRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->statusCode = pDisassocRsp->statusCode;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info, 0,
						eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_mic_fail(tpAniSirGlobal mac_ctx, uint32_t sessionId)
{
	WLAN_HOST_DIAG_EVENT_DEF(secEvent,
				 host_event_wlan_security_payload_type);
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, sessionId);

	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	qdf_mem_zero(&secEvent, sizeof(host_event_wlan_security_payload_type));
	secEvent.eventId = WLAN_SECURITY_EVENT_MIC_ERROR;
	secEvent.encryptionModeMulticast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.mcEncryptionType);
	secEvent.encryptionModeUnicast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.EncryptionType);
	secEvent.authMode =
		(uint8_t) diag_auth_type_from_csr_type(
				session->connectedProfile.AuthType);
	qdf_mem_copy(secEvent.bssid, session->connectedProfile.bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	WLAN_HOST_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_mic_fail_ind(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tpSirSmeMicFailureInd pMicInd = (tpSirSmeMicFailureInd) msg_ptr;
	eCsrRoamResult result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				&pMicInd->bssId, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pMICFailureInfo = &pMicInd->info;
		if (pMicInd->info.multicast)
			result = eCSR_ROAM_RESULT_MIC_ERROR_GROUP;
		else
			result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
				       eCSR_ROAM_MIC_ERROR_IND, result);
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_roam_diag_mic_fail(mac_ctx, sessionId);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_pbs_probe_req_ind(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tpSirSmeProbeReqInd pProbeReqInd = (tpSirSmeProbeReqInd) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("WPS PBC Probe request Indication from SME");

	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pProbeReqInd->bssid, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pWPSPBCProbeReq = &pProbeReqInd->WPSPBCProbeReq;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info,
				       0, eCSR_ROAM_WPS_PBC_PROBE_REQ_IND,
				       eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND);
	}
	qdf_mem_free(roam_info);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_joined_new_bss(tpAniSirGlobal mac_ctx,
				     tSirSmeNewBssInfo *pNewBss)
{
	host_log_ibss_pkt_type *pIbssLog;
	uint32_t bi;

	WLAN_HOST_DIAG_LOG_ALLOC(pIbssLog, host_log_ibss_pkt_type,
				 LOG_WLAN_IBSS_C);
	if (!pIbssLog)
		return;
	pIbssLog->eventId = WLAN_IBSS_EVENT_COALESCING;
	if (pNewBss) {
		qdf_copy_macaddr(&pIbssLog->bssid, &pNewBss->bssId);
		if (pNewBss->ssId.length > HOST_LOG_MAX_SSID_SIZE)
			pNewBss->ssId.length = HOST_LOG_MAX_SSID_SIZE;
		qdf_mem_copy(pIbssLog->ssid, pNewBss->ssId.ssId,
			     pNewBss->ssId.length);
		pIbssLog->operatingChannel = pNewBss->channelNumber;
	}
	if (QDF_IS_STATUS_SUCCESS(wlan_cfg_get_int(mac_ctx,
						   WNI_CFG_BEACON_INTERVAL,
						   &bi)))
		/* U8 is not enough for beacon interval */
		pIbssLog->beaconInterval = (uint8_t) bi;
	WLAN_HOST_DIAG_LOG_REPORT(pIbssLog);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_wm_status_change_ntf(tpAniSirGlobal mac_ctx,
				      tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tSirSmeWmStatusChangeNtf *pStatusChangeMsg;
	tSirSmeApNewCaps *pApNewCaps;
	eCsrRoamResult result = eCSR_ROAM_RESULT_NONE;
	tSirMacAddr Broadcastaddr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	tSirSmeNewBssInfo *pNewBss;
	eRoamCmdStatus roamStatus = eCSR_ROAM_FAILED;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	pStatusChangeMsg = (tSirSmeWmStatusChangeNtf *) msg_ptr;
	switch (pStatusChangeMsg->statusChangeCode) {
	case eSIR_SME_IBSS_ACTIVE:
		sessionId = csr_find_ibss_session(mac_ctx);
		if (CSR_SESSION_ID_INVALID == sessionId)
			break;
		session = CSR_GET_SESSION(mac_ctx, sessionId);
		if (!session) {
			sme_err("session %d not found",
				sessionId);
			goto out;
		}
		session->connectState = eCSR_ASSOC_STATE_TYPE_IBSS_CONNECTED;
		if (session->pConnectBssDesc) {
			qdf_mem_copy(&roam_info->bssid,
				     session->pConnectBssDesc->bssId,
				     sizeof(struct qdf_mac_addr));
			roam_info->u.pConnectedProfile =
				&session->connectedProfile;
		} else {
			sme_err("CSR: connected BSS is empty");
		}
		result = eCSR_ROAM_RESULT_IBSS_CONNECT;
		roamStatus = eCSR_ROAM_CONNECT_STATUS_UPDATE;
		break;

	case eSIR_SME_IBSS_INACTIVE:
		sessionId = csr_find_ibss_session(mac_ctx);
		if (CSR_SESSION_ID_INVALID != sessionId) {
			session = CSR_GET_SESSION(mac_ctx, sessionId);
			if (!session) {
				sme_err("session %d not found", sessionId);
				goto out;
			}
			session->connectState =
				eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED;
			result = eCSR_ROAM_RESULT_IBSS_INACTIVE;
			roamStatus = eCSR_ROAM_CONNECT_STATUS_UPDATE;
		}
		break;

	case eSIR_SME_JOINED_NEW_BSS:
		/* IBSS coalescing. */
		sme_debug("CSR: eSIR_SME_JOINED_NEW_BSS received from PE");
		sessionId = csr_find_ibss_session(mac_ctx);
		if (CSR_SESSION_ID_INVALID == sessionId)
			break;
		session = CSR_GET_SESSION(mac_ctx, sessionId);
		if (!session) {
			sme_err("session %d not found",
				sessionId);
			goto out;
		}
		/* update the connection state information */
		pNewBss = &pStatusChangeMsg->statusChangeInfo.newBssInfo;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
		csr_roam_diag_joined_new_bss(mac_ctx, pNewBss);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
		csr_roam_update_connected_profile_from_new_bss(mac_ctx,
							       sessionId,
							       pNewBss);

		if ((eCSR_ENCRYPT_TYPE_NONE ==
		     session->connectedProfile.EncryptionType)) {
			csr_roam_issue_set_context_req(mac_ctx,
			    sessionId,
			    session->connectedProfile.EncryptionType,
			    session->pConnectBssDesc,
			    &Broadcastaddr, false, false,
			    eSIR_TX_RX, 0, 0, NULL, 0);
		}
		result = eCSR_ROAM_RESULT_IBSS_COALESCED;
		roamStatus = eCSR_ROAM_IBSS_IND;
		qdf_mem_copy(&roam_info->bssid, &pNewBss->bssId,
			     sizeof(struct qdf_mac_addr));
		/* This BSSID is the real BSSID, save it */
		if (session->pConnectBssDesc)
			qdf_mem_copy(session->pConnectBssDesc->bssId,
				&pNewBss->bssId, sizeof(struct qdf_mac_addr));
		break;

	/*
	 * detection by LIM that the capabilities of the associated
	 * AP have changed.
	 */
	case eSIR_SME_AP_CAPS_CHANGED:
		pApNewCaps = &pStatusChangeMsg->statusChangeInfo.apNewCaps;
		sme_debug("CSR handling eSIR_SME_AP_CAPS_CHANGED");
		status = csr_roam_get_session_id_from_bssid(mac_ctx,
					&pApNewCaps->bssId, &sessionId);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		if (eCSR_ROAMING_STATE_JOINED ==
		    sme_get_current_roam_state(MAC_HANDLE(mac_ctx), sessionId)
		    && ((eCSR_ROAM_SUBSTATE_JOINED_REALTIME_TRAFFIC
			== mac_ctx->roam.curSubState[sessionId])
		    || (eCSR_ROAM_SUBSTATE_NONE ==
			mac_ctx->roam.curSubState[sessionId])
		    || (eCSR_ROAM_SUBSTATE_JOINED_NON_REALTIME_TRAFFIC
			== mac_ctx->roam.curSubState[sessionId])
		    || (eCSR_ROAM_SUBSTATE_JOINED_NO_TRAFFIC ==
			 mac_ctx->roam.curSubState[sessionId]))) {
			sme_warn("Calling csr_roam_disconnect_internal");
			csr_roam_disconnect_internal(mac_ctx, sessionId,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					eSIR_MAC_UNSPEC_FAILURE_REASON);
		} else {
			sme_warn("Skipping the new scan as CSR is in state: %s and sub-state: %s",
				mac_trace_getcsr_roam_state(
					mac_ctx->roam.curState[sessionId]),
				mac_trace_getcsr_roam_sub_state(
					mac_ctx->roam.curSubState[sessionId]));
			/* We ignore the caps change event if CSR is not in full
			 * connected state. Send one event to PE to reset
			 * limSentCapsChangeNtf Once limSentCapsChangeNtf set
			 * 0, lim can send sub sequent CAPS change event
			 * otherwise lim cannot send any CAPS change events to
			 * SME
			 */
			csr_send_reset_ap_caps_changed(mac_ctx,
						       &pApNewCaps->bssId);
		}
		break;

	default:
		roamStatus = eCSR_ROAM_FAILED;
		result = eCSR_ROAM_RESULT_NONE;
		break;
	} /* end switch on statusChangeCode */
	if (eCSR_ROAM_RESULT_NONE != result) {
		csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
				       roamStatus, result);
	}
out:
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_ibss_new_peer_ind(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info = NULL;
	tSmeIbssPeerInd *pIbssPeerInd = (tSmeIbssPeerInd *) msg_ptr;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	host_log_ibss_pkt_type *pIbssLog;

	WLAN_HOST_DIAG_LOG_ALLOC(pIbssLog, host_log_ibss_pkt_type,
				 LOG_WLAN_IBSS_C);
	if (pIbssLog) {
		pIbssLog->eventId = WLAN_IBSS_EVENT_PEER_JOIN;
		qdf_copy_macaddr(&pIbssLog->peer_macaddr,
				 &pIbssPeerInd->peer_addr);
		WLAN_HOST_DIAG_LOG_REPORT(pIbssLog);
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

	sessionId = csr_find_ibss_session(mac_ctx);
	if (CSR_SESSION_ID_INVALID == sessionId)
		return;
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	/*
	 * Issue the set Context request to LIM to establish the Unicast STA
	 * context for the new peer...
	 */
	if (!session->pConnectBssDesc) {
		sme_warn("CSR: connected BSS is empty");
		goto callback_and_free;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		goto callback_and_free;
	qdf_copy_macaddr(&roam_info->peerMac, &pIbssPeerInd->peer_addr);
	qdf_mem_copy(&roam_info->bssid, session->pConnectBssDesc->bssId,
		     sizeof(struct qdf_mac_addr));
	if (pIbssPeerInd->mesgLen > sizeof(tSmeIbssPeerInd)) {
		roam_info->pbFrames = qdf_mem_malloc((pIbssPeerInd->mesgLen -
					sizeof(tSmeIbssPeerInd)));
		if (!roam_info->pbFrames) {
			status = QDF_STATUS_E_NOMEM;
		} else {
			status = QDF_STATUS_SUCCESS;
			roam_info->nBeaconLength = pIbssPeerInd->mesgLen -
							sizeof(tSmeIbssPeerInd);
			qdf_mem_copy(roam_info->pbFrames,
				     ((uint8_t *)pIbssPeerInd) +
				     sizeof(tSmeIbssPeerInd),
				     roam_info->nBeaconLength);
		}
		roam_info->staId = (uint8_t)pIbssPeerInd->staId;
		roam_info->pBssDesc = qdf_mem_malloc(
					session->pConnectBssDesc->length);
		if (!roam_info->pBssDesc) {
			status = QDF_STATUS_E_NOMEM;
		} else {
			status = QDF_STATUS_SUCCESS;
			qdf_mem_copy(roam_info->pBssDesc,
				     session->pConnectBssDesc,
				     session->pConnectBssDesc->length);
		}
	}
	if ((eCSR_ENCRYPT_TYPE_NONE ==
		session->connectedProfile.EncryptionType)) {
		/* NO keys. these key parameters don't matter */
		csr_roam_issue_set_context_req(mac_ctx, sessionId,
			session->connectedProfile.EncryptionType,
			session->pConnectBssDesc,
			&pIbssPeerInd->peer_addr.bytes,
			false, true, eSIR_TX_RX, 0, 0, NULL, 0);
	}

callback_and_free:
	/* send up the sec type for the new peer */
	if (roam_info)
		roam_info->u.pConnectedProfile = &session->connectedProfile;
	csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
			       eCSR_ROAM_CONNECT_STATUS_UPDATE,
			       eCSR_ROAM_RESULT_IBSS_NEW_PEER);
	if (roam_info) {
		qdf_mem_free(roam_info->pbFrames);
		qdf_mem_free(roam_info->pBssDesc);
		qdf_mem_free(roam_info);
	}
}

static void
csr_roam_chk_lnk_ibss_peer_departed_ind(tpAniSirGlobal mac_ctx,
					tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	struct csr_roam_info *roam_info;
	tSmeIbssPeerInd *pIbssPeerInd;

	if (NULL == msg_ptr) {
		sme_err("IBSS peer ind. message is NULL");
		return;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	pIbssPeerInd = (tSmeIbssPeerInd *) msg_ptr;
	sessionId = csr_find_ibss_session(mac_ctx);
	if (CSR_SESSION_ID_INVALID != sessionId) {
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
		host_log_ibss_pkt_type *pIbssLog;

		WLAN_HOST_DIAG_LOG_ALLOC(pIbssLog, host_log_ibss_pkt_type,
					 LOG_WLAN_IBSS_C);
		if (pIbssLog) {
			pIbssLog->eventId = WLAN_IBSS_EVENT_PEER_LEAVE;
			if (pIbssPeerInd) {
				qdf_copy_macaddr(&pIbssLog->peer_macaddr,
						 &pIbssPeerInd->peer_addr);
			}
			WLAN_HOST_DIAG_LOG_REPORT(pIbssLog);
		}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
		sme_debug("CSR: Peer departed notification from LIM");
		roam_info->staId = (uint8_t)pIbssPeerInd->staId;
		qdf_copy_macaddr(&roam_info->peerMac, &pIbssPeerInd->peer_addr);
		csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
				       eCSR_ROAM_CONNECT_STATUS_UPDATE,
				       eCSR_ROAM_RESULT_IBSS_PEER_DEPARTED);
	}
	qdf_mem_free(roam_info);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_set_ctx_rsp(tpAniSirGlobal mac_ctx,
			  struct csr_roam_session *session,
			  tSirSmeSetContextRsp *pRsp)
{
	WLAN_HOST_DIAG_EVENT_DEF(setKeyEvent,
				 host_event_wlan_security_payload_type);
	if (eCSR_ENCRYPT_TYPE_NONE ==
		session->connectedProfile.EncryptionType)
		return;
	qdf_mem_zero(&setKeyEvent,
		    sizeof(host_event_wlan_security_payload_type));
	if (qdf_is_macaddr_group(&pRsp->peer_macaddr))
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_BCAST_RSP;
	else
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_UNICAST_RSP;
	setKeyEvent.encryptionModeMulticast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.mcEncryptionType);
	setKeyEvent.encryptionModeUnicast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.EncryptionType);
	qdf_mem_copy(setKeyEvent.bssid, session->connectedProfile.bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	setKeyEvent.authMode =
		(uint8_t) diag_auth_type_from_csr_type(
					session->connectedProfile.AuthType);
	if (eSIR_SME_SUCCESS != pRsp->statusCode)
		setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
	WLAN_HOST_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_set_ctx_rsp(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	eCsrRoamResult result = eCSR_ROAM_RESULT_NONE;
	tSirSmeSetContextRsp *pRsp = (tSirSmeSetContextRsp *) msg_ptr;


	if (!pRsp) {
		sme_err("set key response is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sessionId = pRsp->sessionId;
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_roam_diag_set_ctx_rsp(mac_ctx, session, pRsp);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, sessionId)) {
		csr_roam_stop_wait_for_key_timer(mac_ctx);
		/* We are done with authentication, whethere succeed or not */
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
		/* We do it here because this linkup function is not called
		 * after association  when a key needs to be set.
		 */
		if (csr_is_conn_state_connected_infra(mac_ctx, sessionId))
			csr_roam_link_up(mac_ctx,
					 session->connectedProfile.bssid);
	}
	if (eSIR_SME_SUCCESS == pRsp->statusCode) {
		qdf_copy_macaddr(&roam_info->peerMac, &pRsp->peer_macaddr);
		/* Make sure we install the GTK before indicating to HDD as
		 * authenticated. This is to prevent broadcast packets go out
		 * after PTK and before GTK.
		 */
		if (qdf_is_macaddr_broadcast(&pRsp->peer_macaddr)) {
			/*
			 * OBSS SCAN Indication will be sent to Firmware
			 * to start OBSS Scan
			 */
			if (CSR_IS_CHANNEL_24GHZ(
				session->connectedProfile.operationChannel)
				&& (session->connectState ==
					eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED)
				&& session->pCurRoamProfile
				&& ((QDF_P2P_CLIENT_MODE ==
				     session->pCurRoamProfile->csrPersona)
				|| (QDF_STA_MODE ==
				     session->pCurRoamProfile->csrPersona))) {
				struct sme_obss_ht40_scanind_msg *msg;

				msg = qdf_mem_malloc(sizeof(
					struct sme_obss_ht40_scanind_msg));
				if (!msg) {
					qdf_mem_free(roam_info);
					return;
				}

				msg->msg_type = eWNI_SME_HT40_OBSS_SCAN_IND;
				msg->length =
				      sizeof(struct sme_obss_ht40_scanind_msg);
				qdf_copy_macaddr(&msg->mac_addr,
					&session->connectedProfile.bssid);
				status = umac_send_mb_message_to_mac(msg);
			}
			result = eCSR_ROAM_RESULT_AUTHENTICATED;
		} else {
			result = eCSR_ROAM_RESULT_NONE;
		}
	} else {
		result = eCSR_ROAM_RESULT_FAILURE;
		sme_err(
			"CSR: setkey command failed(err=%d) PeerMac "
			MAC_ADDRESS_STR,
			pRsp->statusCode,
			MAC_ADDR_ARRAY(pRsp->peer_macaddr.bytes));
	}
	/* keeping roam_id = 0 as nobody is using roam_id for set_key */
	csr_roam_call_callback(mac_ctx, sessionId, roam_info,
			       0, eCSR_ROAM_SET_KEY_COMPLETE, result);
	/* Indicate SME_QOS that the SET_KEY is completed, so that SME_QOS
	 * can go ahead and initiate the TSPEC if any are pending
	 */
	sme_qos_csr_event_ind(mac_ctx, (uint8_t) sessionId,
			      SME_QOS_CSR_SET_KEY_SUCCESS_IND, NULL);
#ifdef FEATURE_WLAN_ESE
	/* Send Adjacent AP repot to new AP. */
	if (result == eCSR_ROAM_RESULT_AUTHENTICATED
	    && session->isPrevApInfoValid
	    && session->connectedProfile.isESEAssoc) {
		csr_send_ese_adjacent_ap_rep_ind(mac_ctx, session);
		session->isPrevApInfoValid = false;
	}
#endif
	qdf_mem_free(roam_info);
	return;
}


static void
csr_roam_chk_lnk_max_assoc_exceeded(tpAniSirGlobal mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	tSmeMaxAssocInd *pSmeMaxAssocInd;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	pSmeMaxAssocInd = (tSmeMaxAssocInd *) msg_ptr;
	sme_debug(
		"max assoc have been reached, new peer cannot be accepted");
	sessionId = pSmeMaxAssocInd->sessionId;
	roam_info->sessionId = sessionId;
	qdf_copy_macaddr(&roam_info->peerMac, &pSmeMaxAssocInd->peer_mac);
	csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
			       eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED);
	qdf_mem_free(roam_info);
}

void csr_purge_pdev_all_ser_cmd_list_sync(tpAniSirGlobal mac_ctx,
					  struct sir_purge_pdev_cmd_req *req)
{
	csr_purge_pdev_all_ser_cmd_list(mac_ctx);

	if (req->purge_complete_cb)
		req->purge_complete_cb(mac_ctx->hdd_handle);
}

void csr_roam_check_for_link_status_change(tpAniSirGlobal pMac,
						tSirSmeRsp *pSirMsg)
{
	if (NULL == pSirMsg) {
		sme_err("pSirMsg is NULL");
		return;
	}
	switch (pSirMsg->messageType) {
	case eWNI_SME_ASSOC_IND:
		csr_roam_chk_lnk_assoc_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_IND:
		csr_roam_chk_lnk_disassoc_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(pMac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_IND:
		csr_roam_chk_lnk_deauth_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_SWITCH_CHL_IND:
		csr_roam_chk_lnk_swt_ch_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_RSP:
		csr_roam_chk_lnk_deauth_rsp(pMac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_RSP:
		csr_roam_chk_lnk_disassoc_rsp(pMac, pSirMsg);
		break;
	case eWNI_SME_MIC_FAILURE_IND:
		csr_roam_chk_lnk_mic_fail_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_WPS_PBC_PROBE_REQ_IND:
		csr_roam_chk_lnk_pbs_probe_req_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_WM_STATUS_CHANGE_NTF:
		csr_roam_chk_lnk_wm_status_change_ntf(pMac, pSirMsg);
		break;
	case eWNI_SME_IBSS_NEW_PEER_IND:
		csr_roam_chk_lnk_ibss_new_peer_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_IBSS_PEER_DEPARTED_IND:
		csr_roam_chk_lnk_ibss_peer_departed_ind(pMac, pSirMsg);
		break;
	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_chk_lnk_set_ctx_rsp(pMac, pSirMsg);
		break;
	case eWNI_SME_GET_STATISTICS_RSP:
		sme_debug("Stats rsp from PE");
		csr_roam_stats_rsp_processor(pMac, pSirMsg);
		break;
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_RSP:
		sme_debug("TSM Stats rsp from PE");
		csr_tsm_stats_rsp_processor(pMac, pSirMsg);
		break;
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_GET_RSSI_REQ:
		sme_debug("GetRssiReq from self");
		csr_update_rssi(pMac, pSirMsg);
		break;
	case eWNI_SME_GET_SNR_REQ:
		sme_debug("GetSnrReq from self");
		csr_update_snr(pMac, pSirMsg);
		break;
	case eWNI_SME_FT_PRE_AUTH_RSP:
		csr_roam_ft_pre_auth_rsp_processor(pMac,
						(tpSirFTPreAuthRsp) pSirMsg);
		break;
	case eWNI_SME_MAX_ASSOC_EXCEEDED:
		csr_roam_chk_lnk_max_assoc_exceeded(pMac, pSirMsg);
		break;
	case eWNI_SME_CANDIDATE_FOUND_IND:
		sme_debug("Candidate found indication from PE");
		csr_neighbor_roam_candidate_found_ind_hdlr(pMac, pSirMsg);
		break;
	case eWNI_SME_HANDOFF_REQ:
		sme_debug("Handoff Req from self");
		csr_neighbor_roam_handoff_req_hdlr(pMac, pSirMsg);
		break;
	case eWNI_SME_PURGE_ALL_PDEV_CMDS_REQ:
		csr_purge_pdev_all_ser_cmd_list_sync(pMac,
			(struct sir_purge_pdev_cmd_req *)pSirMsg);
		break;
	default:
		break;
	} /* end switch on message type */
}

void csr_call_roaming_completion_callback(tpAniSirGlobal pMac,
					  struct csr_roam_session *pSession,
					  struct csr_roam_info *roam_info,
					  uint32_t roamId,
					  eCsrRoamResult roamResult)
{
	if (pSession) {
		if (pSession->bRefAssocStartCnt) {
			pSession->bRefAssocStartCnt--;

			if (0 != pSession->bRefAssocStartCnt) {
				QDF_ASSERT(pSession->bRefAssocStartCnt == 0);
				return;
			}
			/* Need to call association_completion because there
			 * is an assoc_start pending.
			 */
			csr_roam_call_callback(pMac, pSession->sessionId, NULL,
					       roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_FAILURE);
		}
		csr_roam_call_callback(pMac, pSession->sessionId, roam_info,
				       roamId, eCSR_ROAM_ROAMING_COMPLETION,
				       roamResult);
	} else
		sme_err("pSession is NULL");
}

/* return a bool to indicate whether roaming completed or continue. */
bool csr_roam_complete_roaming(tpAniSirGlobal pMac, uint32_t sessionId,
			       bool fForce, eCsrRoamResult roamResult)
{
	bool fCompleted = true;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found ", sessionId);
		return false;
	}
	/* Check whether time is up */
	if (pSession->fCancelRoaming || fForce ||
	    eCsrReassocRoaming == pSession->roamingReason ||
	    eCsrDynamicRoaming == pSession->roamingReason) {
		sme_debug("indicates roaming completion");
		if (pSession->fCancelRoaming
		    && CSR_IS_LOSTLINK_ROAMING(pSession->roamingReason)) {
			/* roaming is cancelled, tell HDD to indicate disconnect
			 * Because LIM overload deauth_ind for both deauth frame
			 * and missed beacon we need to use this logic to
			 * detinguish it. For missed beacon, LIM set reason to
			 * be eSIR_BEACON_MISSED
			 */
			if (eSIR_MAC_BEACON_MISSED == pSession->roamingStatusCode) {
				roamResult = eCSR_ROAM_RESULT_LOSTLINK;
			} else if (eCsrLostlinkRoamingDisassoc ==
				   pSession->roamingReason) {
				roamResult = eCSR_ROAM_RESULT_DISASSOC_IND;
			} else if (eCsrLostlinkRoamingDeauth ==
				   pSession->roamingReason) {
				roamResult = eCSR_ROAM_RESULT_DEAUTH_IND;
			} else {
				roamResult = eCSR_ROAM_RESULT_LOSTLINK;
			}
		}
		csr_call_roaming_completion_callback(pMac, pSession, NULL, 0,
						     roamResult);
		pSession->roamingReason = eCsrNotRoaming;
	} else {
		pSession->roamResult = roamResult;
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_start_roaming_timer(pMac,
					sessionId, QDF_MC_TIMER_TO_SEC_UNIT))) {
			csr_call_roaming_completion_callback(pMac, pSession,
							NULL, 0, roamResult);
			pSession->roamingReason = eCsrNotRoaming;
		} else {
			fCompleted = false;
		}
	}
	return fCompleted;
}

void csr_roam_cancel_roaming(tpAniSirGlobal pMac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return;
	}

	if (CSR_IS_ROAMING(pSession)) {
		sme_debug("Cancel roaming");
		pSession->fCancelRoaming = true;
		if (CSR_IS_ROAM_JOINING(pMac, sessionId)
		    && CSR_IS_ROAM_SUBSTATE_CONFIG(pMac, sessionId)) {
			/* No need to do anything in here because the handler
			 * takes care of it
			 */
		} else {
			eCsrRoamResult roamResult =
				CSR_IS_LOSTLINK_ROAMING(pSession->
							roamingReason) ?
				eCSR_ROAM_RESULT_LOSTLINK :
							eCSR_ROAM_RESULT_NONE;
			/* Roaming is stopped after here */
			csr_roam_complete_roaming(pMac, sessionId, true,
						  roamResult);
			/* Since CSR may be in lostlink roaming situation,
			 * abort all roaming related activities
			 */
			csr_scan_abort_mac_scan(pMac, sessionId, INVAL_SCAN_ID);
			csr_roam_stop_roaming_timer(pMac, sessionId);
		}
	}
}

void csr_roam_roaming_timer_handler(void *pv)
{
	tCsrTimerInfo *pInfo = (tCsrTimerInfo *) pv;
	tpAniSirGlobal pMac = pInfo->pMac;
	uint32_t vdev_id = pInfo->vdev_id;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, vdev_id);

	if (!pSession) {
		sme_err("  session %d not found ", vdev_id);
		return;
	}

	if (false == pSession->fCancelRoaming) {
		csr_call_roaming_completion_callback(pMac, pSession,
						NULL, 0,
						pSession->roamResult);
		pSession->roamingReason = eCsrNotRoaming;
	}
}

/**
 * csr_roam_roaming_offload_timeout_handler() - Handler for roaming failure
 * @timer_data: Carries the mac_ctx and session info
 *
 * This function would be invoked when the roaming_offload_timer expires.
 * The timer is waiting in anticipation of a related roaming event from
 * the firmware after receiving the ROAM_START event.
 *
 * Return: None
 */
void csr_roam_roaming_offload_timeout_handler(void *timer_data)
{
	tCsrTimerInfo *timer_info = (tCsrTimerInfo *) timer_data;
	struct scheduler_msg wma_msg = {0};
	struct roam_sync_timeout_timer_info *req;
	QDF_STATUS status;

	if (timer_info) {
		sme_debug("LFR3:roaming offload timer expired, session: %d",
			  timer_info->vdev_id);

	} else {
		sme_err("Invalid Session");
		return;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return;

	req->vdev_id = timer_info->vdev_id;
	wma_msg.type = WMA_ROAM_SYNC_TIMEOUT;
	wma_msg.bodyptr = req;

	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &wma_msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Post roam offload timer fails, session id: %d",
			timer_info->vdev_id);
		qdf_mem_free(req);
		return;
	}
}

QDF_STATUS csr_roam_start_roaming_timer(tpAniSirGlobal pMac,
					uint32_t vdev_id,
					uint32_t interval)
{
	QDF_STATUS status;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, vdev_id);

	if (!pSession) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("csrScanStartRoamingTimer");
	pSession->roamingTimerInfo.vdev_id = (uint8_t)vdev_id;
	status = qdf_mc_timer_start(&pSession->hTimerRoaming,
				    interval / QDF_MC_TIMER_TO_MS_UNIT);

	return status;
}

QDF_STATUS csr_roam_stop_roaming_timer(tpAniSirGlobal pMac,
				       uint32_t vdev_id)
{
	return qdf_mc_timer_stop
			(&pMac->roam.roamSession[vdev_id].hTimerRoaming);
}

void csr_roam_wait_for_key_time_out_handler(void *pv)
{
	tCsrTimerInfo *pInfo = (tCsrTimerInfo *) pv;
	tpAniSirGlobal pMac = pInfo->pMac;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac,
				pInfo->vdev_id);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (pSession == NULL) {
		sme_err("session not found");
		return;
	}

	sme_debug("WaitForKey timer expired in state: %s sub-state: %s",
		mac_trace_get_neighbour_roam_state(pMac->roam.
					neighborRoamInfo[pInfo->vdev_id].
						   neighborRoamState),
		mac_trace_getcsr_roam_sub_state(pMac->roam.
						curSubState[pInfo->vdev_id]));
	spin_lock(&pMac->roam.roam_state_lock);
	if (CSR_IS_WAIT_FOR_KEY(pMac, pInfo->vdev_id)) {
		/* Change the substate so command queue is unblocked. */
		if (CSR_ROAM_SESSION_MAX > pInfo->vdev_id)
			pMac->roam.curSubState[pInfo->vdev_id] =
						eCSR_ROAM_SUBSTATE_NONE;
		spin_unlock(&pMac->roam.roam_state_lock);

		if (csr_neighbor_roam_is_handoff_in_progress(pMac,
						pInfo->vdev_id)) {
			/*
			 * Enable heartbeat timer when hand-off is in progress
			 * and Key Wait timer expired.
			 */
			sme_debug("Enabling HB timer after WaitKey expiry nHBCount: %d)",
				pMac->roam.configParam.HeartbeatThresh24);
			cfg_set_int(pMac, WNI_CFG_HEART_BEAT_THRESHOLD,
				pMac->roam.configParam.HeartbeatThresh24);
		}
		sme_debug("SME pre-auth state timeout");

		if (csr_is_conn_state_connected_infra(pMac, pInfo->vdev_id)) {
			csr_roam_link_up(pMac,
					 pSession->connectedProfile.bssid);
			status = sme_acquire_global_lock(&pMac->sme);
			if (QDF_IS_STATUS_SUCCESS(status)) {
				csr_roam_disconnect(pMac, pInfo->vdev_id,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					eSIR_MAC_KEY_TIMEOUT);
				sme_release_global_lock(&pMac->sme);
			}
		} else {
			sme_err("session not found");
		}
	} else {
		spin_unlock(&pMac->roam.roam_state_lock);
	}

}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS csr_fast_reassoc(tHalHandle hal, struct csr_roam_profile *profile,
			    const tSirMacAddr bssid, int channel,
			    uint8_t vdev_id, const tSirMacAddr connected_bssid)
{
	QDF_STATUS status;
	struct wma_roam_invoke_cmd *fastreassoc;
	struct scheduler_msg msg = {0};
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal);
	struct csr_roam_session *session;
	struct csr_roam_profile *roam_profile;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_invoke_entity_param *vdev_roam_params;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session || !session->pCurRoamProfile) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!csr_is_conn_state_connected(mac_ctx, vdev_id)) {
		sme_debug("Not in connected state, Roam Invoke not sent");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("vdev is NULL, aborting roam invoke");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);

	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting roam invoke");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (vdev_roam_params->roam_invoke_in_progress) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_FAILURE;
	}
	roam_profile = session->pCurRoamProfile;
	if (roam_profile->driver_disabled_roaming) {
		sme_debug("roaming status in driver %d",
			  roam_profile->driver_disabled_roaming);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_FAILURE;
	}
	fastreassoc = qdf_mem_malloc(sizeof(*fastreassoc));
	if (NULL == fastreassoc) {
		sme_err("qdf_mem_malloc failed for fastreassoc");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_NOMEM;
	}
	/* if both are same then set the flag */
	if (!qdf_mem_cmp(connected_bssid, bssid, ETH_ALEN)) {
		fastreassoc->is_same_bssid = true;
		sme_debug("bssid same, bssid[%pM]", bssid);
	}
	fastreassoc->vdev_id = vdev_id;
	fastreassoc->bssid[0] = bssid[0];
	fastreassoc->bssid[1] = bssid[1];
	fastreassoc->bssid[2] = bssid[2];
	fastreassoc->bssid[3] = bssid[3];
	fastreassoc->bssid[4] = bssid[4];
	fastreassoc->bssid[5] = bssid[5];

	status = sme_get_beacon_frm(hal, profile, bssid,
				    &fastreassoc->frame_buf,
				    &fastreassoc->frame_len,
				    &channel);

	if (!channel) {
		sme_err("channel retrieval from BSS desc fails!");
		qdf_mem_free(fastreassoc);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_FAULT;
	}

	fastreassoc->channel = channel;
	if (QDF_STATUS_SUCCESS != status) {
		sme_warn("sme_get_beacon_frm failed");
		fastreassoc->frame_buf = NULL;
		fastreassoc->frame_len = 0;
	}

	if (csr_is_auth_type_ese(mac_ctx->roam.roamSession[vdev_id].
				connectedProfile.AuthType)) {
		sme_debug("Beacon is not required for ESE");
		if (fastreassoc->frame_len) {
			qdf_mem_free(fastreassoc->frame_buf);
			fastreassoc->frame_buf = NULL;
			fastreassoc->frame_len = 0;
		}
	}

	msg.type = eWNI_SME_ROAM_INVOKE;
	msg.reserved = 0;
	msg.bodyptr = fastreassoc;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Not able to post ROAM_INVOKE_CMD message to PE");
		qdf_mem_free(fastreassoc);
	} else {
		vdev_roam_params->roam_invoke_in_progress = true;
		sme_debug("Trigger roaming for vdev id %d ",
			  session->sessionId);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return status;
}

/**
 * csr_roam_roaming_offload_timer_action() - API to start/stop the timer
 * @mac_ctx: MAC Context
 * @interval: Value to be set for the timer
 * @session_id: Session on which the timer should be operated
 * @action: Start/Stop action for the timer
 *
 * API to start/stop the roaming offload timer
 *
 * Return: None
 */
void csr_roam_roaming_offload_timer_action(
		tpAniSirGlobal mac_ctx, uint32_t interval, uint8_t vdev_id,
		uint8_t action)
{
	struct csr_roam_session *csr_session = CSR_GET_SESSION(mac_ctx,
				vdev_id);
	QDF_TIMER_STATE tstate;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("LFR3: timer action %d, vdev_id %d, intvl %d"),
			action, vdev_id, interval);
	if (mac_ctx) {
		csr_session = CSR_GET_SESSION(mac_ctx, vdev_id);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				("LFR3: Invalid MAC Context"));
		return;
	}
	if (!csr_session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				("LFR3: session %d not found"), vdev_id);
		return;
	}
	csr_session->roamingTimerInfo.vdev_id = (uint8_t)vdev_id;

	tstate =
	   qdf_mc_timer_get_current_state(&csr_session->roaming_offload_timer);

	if (action == ROAMING_OFFLOAD_TIMER_START) {
		/*
		 * If timer is already running then re-start timer in order to
		 * process new ROAM_START with fresh timer.
		 */
		if (tstate == QDF_TIMER_STATE_RUNNING)
			qdf_mc_timer_stop(&csr_session->roaming_offload_timer);
	qdf_mc_timer_start(&csr_session->roaming_offload_timer,
			   interval / QDF_MC_TIMER_TO_MS_UNIT);
	}

	if (action == ROAMING_OFFLOAD_TIMER_STOP)
		qdf_mc_timer_stop(&csr_session->roaming_offload_timer);

}
#endif

static QDF_STATUS csr_roam_start_wait_for_key_timer(
		tpAniSirGlobal pMac, uint32_t interval)
{
	QDF_STATUS status;
#ifdef WLAN_DEBUG
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&pMac->roam.neighborRoamInfo[pMac->roam.WaitForKeyTimerInfo.
					     vdev_id];
#endif
	if (csr_neighbor_roam_is_handoff_in_progress(pMac,
				     pMac->roam.WaitForKeyTimerInfo.
				     vdev_id)) {
		/* Disable heartbeat timer when hand-off is in progress */
		sme_debug("disabling HB timer in state: %s sub-state: %s",
			mac_trace_get_neighbour_roam_state(
				pNeighborRoamInfo->neighborRoamState),
			mac_trace_getcsr_roam_sub_state(
				pMac->roam.curSubState[pMac->roam.
					WaitForKeyTimerInfo.vdev_id]));
		cfg_set_int(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, 0);
	}
	sme_debug("csrScanStartWaitForKeyTimer");
	status = qdf_mc_timer_start(&pMac->roam.hTimerWaitForKey,
				    interval / QDF_MC_TIMER_TO_MS_UNIT);

	return status;
}

QDF_STATUS csr_roam_stop_wait_for_key_timer(tpAniSirGlobal pMac)
{
#ifdef WLAN_DEBUG
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&pMac->roam.neighborRoamInfo[pMac->roam.WaitForKeyTimerInfo.
					     vdev_id];
#endif

	sme_debug("WaitForKey timer stopped in state: %s sub-state: %s",
		mac_trace_get_neighbour_roam_state(pNeighborRoamInfo->
						   neighborRoamState),
		mac_trace_getcsr_roam_sub_state(pMac->roam.
						curSubState[pMac->roam.
							    WaitForKeyTimerInfo.
							    vdev_id]));
	if (csr_neighbor_roam_is_handoff_in_progress(pMac,
				pMac->roam.WaitForKeyTimerInfo.vdev_id)) {
		/*
		 * Enable heartbeat timer when hand-off is in progress
		 * and Key Wait timer got stopped for some reason
		 */
		sme_debug("Enabling HB timer after WaitKey stop nHBCount: %d",
			pMac->roam.configParam.HeartbeatThresh24);
		cfg_set_int(pMac, WNI_CFG_HEART_BEAT_THRESHOLD,
				pMac->roam.configParam.HeartbeatThresh24);
	}
	return qdf_mc_timer_stop(&pMac->roam.hTimerWaitForKey);
}

void csr_roam_completion(tpAniSirGlobal pMac, uint32_t sessionId,
			 struct csr_roam_info *roam_info, tSmeCmd *pCommand,
			 eCsrRoamResult roamResult, bool fSuccess)
{
	eRoamCmdStatus roamStatus = csr_get_roam_complete_status(pMac,
								sessionId);
	uint32_t roamId = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found ", sessionId);
		return;
	}

	if (pCommand) {
		roamId = pCommand->u.roamCmd.roamId;
		if (sessionId != pCommand->sessionId) {
			QDF_ASSERT(sessionId == pCommand->sessionId);
			return;
		}
	}
	if (eCSR_ROAM_ROAMING_COMPLETION == roamStatus)
		/* if success, force roaming completion */
		csr_roam_complete_roaming(pMac, sessionId, fSuccess,
								roamResult);
	else {
		if (pSession->bRefAssocStartCnt != 0) {
			QDF_ASSERT(pSession->bRefAssocStartCnt == 0);
			return;
		}
		sme_debug("indicates association completion roamResult: %d",
			roamResult);
		csr_roam_call_callback(pMac, sessionId, roam_info, roamId,
				       roamStatus, roamResult);
	}
}

static
QDF_STATUS csr_roam_lost_link(tpAniSirGlobal pMac, uint32_t sessionId,
			      uint32_t type, tSirSmeRsp *pSirMsg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirSmeDeauthInd *pDeauthIndMsg = NULL;
	tSirSmeDisassocInd *pDisassocIndMsg = NULL;
	eCsrRoamResult result = eCSR_ROAM_RESULT_LOSTLINK;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	struct csr_roam_info *roam_info;
	bool from_ap = false;

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	if (eWNI_SME_DISASSOC_IND == type) {
		result = eCSR_ROAM_RESULT_DISASSOC_IND;
		pDisassocIndMsg = (tSirSmeDisassocInd *) pSirMsg;
		pSession->roamingStatusCode = pDisassocIndMsg->statusCode;
		pSession->joinFailStatusCode.reasonCode =
			pDisassocIndMsg->reasonCode;
		from_ap = pDisassocIndMsg->from_ap;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocIndMsg->peer_macaddr);
	} else if (eWNI_SME_DEAUTH_IND == type) {
		result = eCSR_ROAM_RESULT_DEAUTH_IND;
		pDeauthIndMsg = (tSirSmeDeauthInd *) pSirMsg;
		pSession->roamingStatusCode = pDeauthIndMsg->statusCode;
		pSession->joinFailStatusCode.reasonCode =
			pDeauthIndMsg->reasonCode;
		from_ap = pDeauthIndMsg->from_ap;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthIndMsg->peer_macaddr);

	} else {
		sme_warn("gets an unknown type (%d)", type);
		result = eCSR_ROAM_RESULT_NONE;
		pSession->joinFailStatusCode.reasonCode = 1;
	}
	mlme_set_discon_reason_n_from_ap(pMac->psoc, sessionId, from_ap,
				      pSession->joinFailStatusCode.reasonCode);

	if (type == eWNI_SME_DISASSOC_IND || type == eWNI_SME_DEAUTH_IND)
		csr_get_peer_rssi(pMac, sessionId, roam_info->peerMac);

	csr_roam_call_callback(pMac, sessionId, NULL, 0,
			       eCSR_ROAM_LOSTLINK_DETECTED, result);

	if (eWNI_SME_DISASSOC_IND == type)
		status = csr_send_mb_disassoc_cnf_msg(pMac, pDisassocIndMsg);
	else if (eWNI_SME_DEAUTH_IND == type)
		status = csr_send_mb_deauth_cnf_msg(pMac, pDeauthIndMsg);

	/* prepare to tell HDD to disconnect */
	qdf_mem_zero(roam_info, sizeof(*roam_info));
	roam_info->statusCode = (tSirResultCodes) pSession->roamingStatusCode;
	roam_info->reasonCode = pSession->joinFailStatusCode.reasonCode;
	if (eWNI_SME_DISASSOC_IND == type) {
		/* staMacAddr */
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocIndMsg->peer_macaddr);
		roam_info->staId = (uint8_t)pDisassocIndMsg->staId;
		roam_info->reasonCode = pDisassocIndMsg->reasonCode;
	} else if (eWNI_SME_DEAUTH_IND == type) {
		/* staMacAddr */
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthIndMsg->peer_macaddr);
		roam_info->staId = (uint8_t)pDeauthIndMsg->staId;
		roam_info->reasonCode = pDeauthIndMsg->reasonCode;
		roam_info->rxRssi = pDeauthIndMsg->rssi;
	}
	sme_debug("roamInfo.staId: %d", roam_info->staId);
/* Dont initiate internal driver based roaming after disconnection*/
	qdf_mem_free(roam_info);
	return status;
}


void csr_roam_wm_status_change_complete(tpAniSirGlobal pMac,
					uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;

	pEntry = csr_nonscan_active_ll_peek_head(pMac, LL_ACCESS_LOCK);
	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if (eSmeCommandWmStatusChange == pCommand->command) {
			/* Nothing to process in a Lost Link completion....  It just kicks off a */
			/* roaming sequence. */
			if (csr_nonscan_active_ll_remove_entry(pMac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(pMac, pCommand);
			} else {
	sme_err(
	" ******csr_roam_wm_status_change_complete fail to release command");
			}

		} else {
	sme_warn(
"CSR: WmStatusChange Completion called but LOST LINK command is not ACTIVE ...");
		}
	} else {
	sme_warn(
	"CSR: WmStatusChange Completion called but NO commands are ACTIVE ...");
	}
}

void csr_roam_process_wm_status_change_command(
		tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tSirSmeRsp *pSirSmeMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac,
						pCommand->sessionId);

	if (!pSession) {
		sme_err("session %d not found", pCommand->sessionId);
		goto end;
	}
	sme_debug("session:%d, CmdType : %d",
		pCommand->sessionId, pCommand->u.wmStatusChangeCmd.Type);

	switch (pCommand->u.wmStatusChangeCmd.Type) {
	case eCsrDisassociated:
		pSirSmeMsg =
			(tSirSmeRsp *) &pCommand->u.wmStatusChangeCmd.u.
			DisassocIndMsg;
		status =
			csr_roam_lost_link(pMac, pCommand->sessionId,
					   eWNI_SME_DISASSOC_IND, pSirSmeMsg);
		break;
	case eCsrDeauthenticated:
		pSirSmeMsg =
			(tSirSmeRsp *) &pCommand->u.wmStatusChangeCmd.u.
			DeauthIndMsg;
		status =
			csr_roam_lost_link(pMac, pCommand->sessionId,
					   eWNI_SME_DEAUTH_IND, pSirSmeMsg);
		break;
	default:
		sme_warn("gets an unknown command %d",
			pCommand->u.wmStatusChangeCmd.Type);
		break;
	}

end:
	if (status != QDF_STATUS_SUCCESS) {
		/*
		 * As status returned is not success, there is nothing else
		 * left to do so release WM status change command here.
		 */
		csr_roam_wm_status_change_complete(pMac, pCommand->sessionId);
	}
}

QDF_STATUS csr_process_del_sta_session_command(tpAniSirGlobal mac_ctx,
					       tSmeCmd *sme_command)
{
	struct del_sta_self_params *del_sta_self_req;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	del_sta_self_req = qdf_mem_malloc(sizeof(struct del_sta_self_params));
	if (NULL == del_sta_self_req) {
		sme_err(" mem alloc failed for tDelStaSelfParams");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_copy(del_sta_self_req->self_mac_addr,
		     sme_command->u.delStaSessionCmd.selfMacAddr,
		     sizeof(tSirMacAddr));

	del_sta_self_req->session_id = sme_command->sessionId;
	del_sta_self_req->sme_callback =
		sme_command->u.delStaSessionCmd.session_close_cb;
	del_sta_self_req->sme_ctx = sme_command->u.delStaSessionCmd.context;
	msg.type = WMA_DEL_STA_SELF_REQ;
	msg.reserved = 0;
	msg.bodyptr = del_sta_self_req;
	msg.bodyval = 0;

	sme_debug("sending WMA_DEL_STA_SELF_REQ");
	status = wma_post_ctrl_msg(mac_ctx, &msg);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("wma_post_ctrl_msg failed");
		qdf_mem_free(del_sta_self_req);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * csr_compute_mode_and_band() - computes dot11mode
 * @pMac:          mac global context
 * @dot11_mode:    out param, do11 mode calculated
 * @band:          out param, band caclculated
 * @opr_ch:        operating channels
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes.
 *
 * Return: void
 */
static void
csr_compute_mode_and_band(tpAniSirGlobal mac_ctx,
			  enum csr_cfgdot11mode *dot11_mode,
			  enum band_info *band,
			  uint8_t opr_ch)
{
	bool vht_24_ghz = mac_ctx->roam.configParam.enableVhtFor24GHz;

	switch (mac_ctx->roam.configParam.uCfgDot11Mode) {
	case eCSR_CFG_DOT11_MODE_11A:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
		*band = BAND_5G;
		break;
	case eCSR_CFG_DOT11_MODE_11B:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
		*band = BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11G:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		*band = BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11N:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		*band = CSR_GET_BAND(opr_ch);
		break;
	case eCSR_CFG_DOT11_MODE_11AC:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH(opr_ch) && !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = CSR_GET_BAND(opr_ch);
		break;
	case eCSR_CFG_DOT11_MODE_11AC_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH(opr_ch) && !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC_ONLY;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = CSR_GET_BAND(opr_ch);
		break;
	case eCSR_CFG_DOT11_MODE_11AX:
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = mac_ctx->roam.configParam.uCfgDot11Mode;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH(opr_ch) && !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = CSR_GET_BAND(opr_ch);
		break;
	case eCSR_CFG_DOT11_MODE_AUTO:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11AX;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band,
			 * check for INI item to disable VHT operation
			 * in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH(opr_ch)
				&& !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = CSR_GET_BAND(opr_ch);
		break;
	default:
		/*
		 * Global dot11 Mode setting is 11a/b/g. use the channel number
		 * to determine the Mode setting.
		 */
		if (eCSR_OPERATING_CHANNEL_AUTO == opr_ch) {
			*band = mac_ctx->roam.configParam.eBand;
			if (BAND_2G == *band) {
				/*
				 * See reason in else if ( WLAN_REG_IS_24GHZ_CH
				 * (opr_ch) ) to pick 11B
				 */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			} else {
				/* prefer 5GHz */
				*band = BAND_5G;
				*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			}
		} else if (WLAN_REG_IS_24GHZ_CH(opr_ch)) {
			/*
			 * WiFi tests require IBSS networks to start in 11b mode
			 * without any change to the default parameter settings
			 * on the adapter. We use ACU to start an IBSS through
			 * creation of a startIBSS profile. This startIBSS
			 * profile has Auto MACProtocol and the adapter property
			 * setting for dot11Mode is also AUTO. So in this case,
			 * let's start the IBSS network in 11b mode instead of
			 * 11g mode. So this is for Auto=profile->MacProtocol &&
			 * Auto=Global. dot11Mode && profile->channel is < 14,
			 * then start the IBSS in b mode.
			 *
			 * Note: we used to have this start as an 11g IBSS for
			 * best performance. now to specify that the user will
			 * have to set the do11Mode in the property page to 11g
			 * to force it.
			 */
			*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			*band = BAND_2G;
		} else {
			/* else, it's a 5.0GHz channel.  Set mode to 11a. */
			*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			*band = BAND_5G;
		}
		break;
	} /* switch */
}

/**
 * csr_roam_get_phy_mode_band_for_bss() - This function returns band and mode
 * information.
 * @mac_ctx:       mac global context
 * @profile:       bss profile
 * @band:          out param, band caclculated
 * @opr_ch:        operating channels
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes. The only tricky part is that if phyMode is set to 11abg,
 * this function may return eCSR_CFG_DOT11_MODE_11B instead of
 * eCSR_CFG_DOT11_MODE_11G if everything is set to auto-pick.
 *
 * Return: dot11mode
 */
static enum csr_cfgdot11mode
csr_roam_get_phy_mode_band_for_bss(tpAniSirGlobal mac_ctx,
				   struct csr_roam_profile *profile,
				   uint8_t opr_chn,
				   enum band_info *p_band)
{
	enum band_info band;
enum csr_cfgdot11mode curr_mode = mac_ctx->roam.configParam.uCfgDot11Mode;
	enum csr_cfgdot11mode cfg_dot11_mode =
		csr_get_cfg_dot11_mode_from_csr_phy_mode(profile,
			(eCsrPhyMode) profile->phyMode,
			mac_ctx->roam.configParam.ProprietaryRatesEnabled);

	/*
	 * If the global setting for dot11Mode is set to auto/abg, we overwrite
	 * the setting in the profile.
	 */
	if ((!CSR_IS_INFRA_AP(profile)
	    && ((eCSR_CFG_DOT11_MODE_AUTO == curr_mode)
	    || (eCSR_CFG_DOT11_MODE_ABG == curr_mode)))
	    || (eCSR_CFG_DOT11_MODE_AUTO == cfg_dot11_mode)
	    || (eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode)) {
		csr_compute_mode_and_band(mac_ctx, &cfg_dot11_mode,
					  &band, opr_chn);
	} /* if( eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode ) */
	else {
		/* dot11 mode is set, lets pick the band */
		if (eCSR_OPERATING_CHANNEL_AUTO == opr_chn) {
			/* channel is Auto also. */
			band = mac_ctx->roam.configParam.eBand;
			if (BAND_ALL == band) {
				/* prefer 5GHz */
				band = BAND_5G;
			}
		} else{
			band = CSR_GET_BAND(opr_chn);
		}
	}
	if (p_band)
		*p_band = band;

	if (opr_chn == 14) {
		sme_err("Switching to Dot11B mode");
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11B;
	}

	if (IS_24G_CH(opr_chn) &&
	   (false == mac_ctx->roam.configParam.enableVhtFor24GHz) &&
	   (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode ||
	    eCSR_CFG_DOT11_MODE_11AC_ONLY == cfg_dot11_mode))
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11N;
	/*
	 * Incase of WEP Security encryption type is coming as part of add key.
	 * So while STart BSS dont have information
	 */
	if ((!CSR_IS_11n_ALLOWED(profile->EncryptionType.encryptionType[0])
	    || ((profile->privacy == 1)
		&& (profile->EncryptionType.encryptionType[0] ==
		eCSR_ENCRYPT_TYPE_NONE)))
		&& ((eCSR_CFG_DOT11_MODE_11N == cfg_dot11_mode) ||
		    (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode) ||
		    (eCSR_CFG_DOT11_MODE_11AX == cfg_dot11_mode))) {
		/* We cannot do 11n here */
		if (WLAN_REG_IS_24GHZ_CH(opr_chn))
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		else
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11A;
	}
	sme_debug("dot11mode: %d", cfg_dot11_mode);
	return cfg_dot11_mode;
}

QDF_STATUS csr_roam_issue_stop_bss(tpAniSirGlobal pMac,
		uint32_t sessionId, enum csr_roam_substate NewSubstate)
{
	QDF_STATUS status;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	{
		host_log_ibss_pkt_type *pIbssLog;

		WLAN_HOST_DIAG_LOG_ALLOC(pIbssLog, host_log_ibss_pkt_type,
					 LOG_WLAN_IBSS_C);
		if (pIbssLog) {
			pIbssLog->eventId = WLAN_IBSS_EVENT_STOP_REQ;
			WLAN_HOST_DIAG_LOG_REPORT(pIbssLog);
		}
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	/* Set the roaming substate to 'stop Bss request'... */
	csr_roam_substate_change(pMac, NewSubstate, sessionId);

	/* attempt to stop the Bss (reason code is ignored...) */
	status = csr_send_mb_stop_bss_req_msg(pMac, sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_warn(
			"csr_send_mb_stop_bss_req_msg failed with status %d",
			status);
	}
	return status;
}

/* pNumChan is a caller allocated space with the sizeof pChannels */
QDF_STATUS csr_get_cfg_valid_channels(tpAniSirGlobal pMac, uint8_t *pChannels,
				      uint32_t *pNumChan)
{
	uint8_t num_chan_temp = 0;
	int i;

	if (!QDF_IS_STATUS_SUCCESS(wlan_cfg_get_str(pMac,
					WNI_CFG_VALID_CHANNEL_LIST,
					(uint8_t *) pChannels, pNumChan)))
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < *pNumChan; i++) {
		if (!wlan_reg_is_dsrc_chan(pMac->pdev, pChannels[i])) {
			pChannels[num_chan_temp] = pChannels[i];
			num_chan_temp++;
		}
	}

	*pNumChan = num_chan_temp;
	return QDF_STATUS_SUCCESS;
}

int8_t csr_get_cfg_max_tx_power(tpAniSirGlobal pMac, uint8_t channel)
{
	uint32_t cfgLength = 0;
	uint16_t cfgId = 0;
	int8_t maxTxPwr = 0;
	uint8_t *pCountryInfo = NULL;
	QDF_STATUS status;
	uint8_t count = 0;
	uint8_t firstChannel;
	uint8_t maxChannels;

	if (WLAN_REG_IS_5GHZ_CH(channel)) {
		cfgId = WNI_CFG_MAX_TX_POWER_5;
		cfgLength = WNI_CFG_MAX_TX_POWER_5_LEN;
	} else if (WLAN_REG_IS_24GHZ_CH(channel)) {
		cfgId = WNI_CFG_MAX_TX_POWER_2_4;
		cfgLength = WNI_CFG_MAX_TX_POWER_2_4_LEN;
	} else
		return maxTxPwr;

	pCountryInfo = qdf_mem_malloc(cfgLength);
	if (NULL == pCountryInfo)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "%s: failed to allocate memory, status = %d",
			  __func__, status);
		goto error;
	}
	if (wlan_cfg_get_str(pMac, cfgId, (uint8_t *)pCountryInfo,
			&cfgLength) != QDF_STATUS_SUCCESS) {
		goto error;
	}
	/* Identify the channel and maxtxpower */
	while (count <= (cfgLength - (sizeof(tSirMacChanInfo)))) {
		firstChannel = pCountryInfo[count++];
		maxChannels = pCountryInfo[count++];
		maxTxPwr = pCountryInfo[count++];

		if ((channel >= firstChannel) &&
		    (channel < (firstChannel + maxChannels))) {
			break;
		}
	}

error:
	if (NULL != pCountryInfo)
		qdf_mem_free(pCountryInfo);

	return maxTxPwr;
}

bool csr_roam_is_channel_valid(tpAniSirGlobal pMac, uint8_t channel)
{
	bool fValid = false;
	uint32_t idxValidChannels;
	uint32_t len = sizeof(pMac->roam.validChannelList);

	if (QDF_IS_STATUS_SUCCESS(csr_get_cfg_valid_channels(pMac,
					pMac->roam.validChannelList, &len))) {
		for (idxValidChannels = 0; (idxValidChannels < len);
		     idxValidChannels++) {
			if (channel ==
			    pMac->roam.validChannelList[idxValidChannels]) {
				fValid = true;
				break;
			}
		}
	}
	pMac->roam.numValidChannels = len;
	return fValid;
}

/* This function check and validate whether the NIC can do CB (40MHz) */
static ePhyChanBondState csr_get_cb_mode_from_ies(tpAniSirGlobal pMac,
						  uint8_t chan,
						  tDot11fBeaconIEs *pIes)
{
	ePhyChanBondState eRet = PHY_SINGLE_CHANNEL_CENTERED;
	uint8_t sec_ch = 0;
	uint32_t ChannelBondingMode;
	struct ch_params ch_params = {0};

	if (WLAN_REG_IS_24GHZ_CH(chan)) {
		ChannelBondingMode =
			pMac->roam.configParam.channelBondingMode24GHz;
	} else {
		ChannelBondingMode =
			pMac->roam.configParam.channelBondingMode5GHz;
	}

	if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE == ChannelBondingMode)
		return PHY_SINGLE_CHANNEL_CENTERED;

	/* Figure what the other side's CB mode */
	if (!(pIes->HTCaps.present && (eHT_CHANNEL_WIDTH_40MHZ ==
		pIes->HTCaps.supportedChannelWidthSet))) {
		return PHY_SINGLE_CHANNEL_CENTERED;
	}

	/* In Case WPA2 and TKIP is the only one cipher suite in Pairwise */
	if ((pIes->RSN.present && (pIes->RSN.pwise_cipher_suite_count == 1) &&
		!memcmp(&(pIes->RSN.pwise_cipher_suites[0][0]),
					"\x00\x0f\xac\x02", 4))
		/* In Case only WPA1 is supported and TKIP is
		 * the only one cipher suite in Unicast.
		 */
		|| (!pIes->RSN.present && (pIes->WPA.present &&
			(pIes->WPA.unicast_cipher_count == 1) &&
			!memcmp(&(pIes->WPA.unicast_ciphers[0][0]),
					"\x00\x50\xf2\x02", 4)))) {
		sme_debug("No channel bonding in TKIP mode");
		return PHY_SINGLE_CHANNEL_CENTERED;
	}

	if (!pIes->HTInfo.present)
		return PHY_SINGLE_CHANNEL_CENTERED;

	/*
	 * This is called during INFRA STA/CLIENT and should use the merged
	 * value of supported channel width and recommended tx width as per
	 * standard
	 */
	sme_debug("chan %d scws %u rtws %u sco %u", chan,
		pIes->HTCaps.supportedChannelWidthSet,
		pIes->HTInfo.recommendedTxWidthSet,
		pIes->HTInfo.secondaryChannelOffset);

	if (pIes->HTInfo.recommendedTxWidthSet == eHT_CHANNEL_WIDTH_40MHZ)
		eRet = (ePhyChanBondState)pIes->HTInfo.secondaryChannelOffset;
	else
		eRet = PHY_SINGLE_CHANNEL_CENTERED;

	switch (eRet) {
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		sec_ch = chan + CSR_SEC_CHANNEL_OFFSET;
		break;
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		sec_ch = chan - CSR_SEC_CHANNEL_OFFSET;
		break;
	default:
		break;
	}

	if (eRet != PHY_SINGLE_CHANNEL_CENTERED) {
		ch_params.ch_width = CH_WIDTH_40MHZ;
		wlan_reg_set_channel_params(pMac->pdev, chan,
					    sec_ch, &ch_params);
		if (ch_params.ch_width == CH_WIDTH_20MHZ ||
		    ch_params.sec_ch_offset != eRet) {
			sme_err("chan %d :: Supported HT BW %d and cbmode %d, APs HT BW %d and cbmode %d, so switch to 20Mhz",
				chan, ch_params.ch_width,
				ch_params.sec_ch_offset,
				pIes->HTInfo.recommendedTxWidthSet, eRet);
			eRet = PHY_SINGLE_CHANNEL_CENTERED;
		}
	}

	return eRet;
}

static bool csr_is_encryption_in_list(tpAniSirGlobal pMac,
				      tCsrEncryptionList *pCipherList,
				      eCsrEncryptionType encryptionType)
{
	bool fFound = false;
	uint32_t idx;

	for (idx = 0; idx < pCipherList->numEntries; idx++) {
		if (pCipherList->encryptionType[idx] == encryptionType) {
			fFound = true;
			break;
		}
	}
	return fFound;
}

static bool csr_is_auth_in_list(tpAniSirGlobal pMac, tCsrAuthList *pAuthList,
				eCsrAuthType authType)
{
	bool fFound = false;
	uint32_t idx;

	for (idx = 0; idx < pAuthList->numEntries; idx++) {
		if (pAuthList->authType[idx] == authType) {
			fFound = true;
			break;
		}
	}
	return fFound;
}

bool csr_is_same_profile(tpAniSirGlobal pMac,
			 tCsrRoamConnectedProfile *pProfile1,
			 struct csr_roam_profile *pProfile2)
{
	uint32_t i;
	bool fCheck = false;
	tCsrScanResultFilter *pScanFilter = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!(pProfile1 && pProfile2))
		return fCheck;
	pScanFilter = qdf_mem_malloc(sizeof(tCsrScanResultFilter));
	if (NULL == pScanFilter)
		return fCheck;

	status = csr_roam_prepare_filter_from_profile(pMac, pProfile2,
						      pScanFilter);
	if (!(QDF_IS_STATUS_SUCCESS(status)))
		goto free_scan_filter;

	for (i = 0; i < pScanFilter->SSIDs.numOfSSIDs; i++) {
		fCheck = csr_is_ssid_match(pMac,
				pScanFilter->SSIDs.SSIDList[i].SSID.ssId,
				pScanFilter->SSIDs.SSIDList[i].SSID.length,
				pProfile1->SSID.ssId,
				pProfile1->SSID.length,
				false);
		if (fCheck)
			break;
	}
	if (!fCheck)
		goto free_scan_filter;

	if (!csr_is_auth_in_list(pMac, &pProfile2->AuthType,
				 pProfile1->AuthType)
	    || (pProfile2->BSSType != pProfile1->BSSType)
	    || !csr_is_encryption_in_list(pMac, &pProfile2->EncryptionType,
					  pProfile1->EncryptionType)) {
		fCheck = false;
		goto free_scan_filter;
	}
	if (pProfile1->MDID.mdiePresent || pProfile2->MDID.mdiePresent) {
		if (pProfile1->MDID.mobilityDomain
			!= pProfile2->MDID.mobilityDomain) {
			fCheck = false;
			goto free_scan_filter;
		}
	}
	/* Match found */
	fCheck = true;
free_scan_filter:
	csr_free_scan_filter(pMac, pScanFilter);
	qdf_mem_free(pScanFilter);
	return fCheck;
}

static bool csr_roam_is_same_profile_keys(tpAniSirGlobal pMac,
				   tCsrRoamConnectedProfile *pConnProfile,
				   struct csr_roam_profile *pProfile2)
{
	bool fCheck = false;
	int i;

	do {
		/* Only check for static WEP */
		if (!csr_is_encryption_in_list
			    (pMac, &pProfile2->EncryptionType,
			    eCSR_ENCRYPT_TYPE_WEP40_STATICKEY)
		    && !csr_is_encryption_in_list(pMac,
				&pProfile2->EncryptionType,
				eCSR_ENCRYPT_TYPE_WEP104_STATICKEY)) {
			fCheck = true;
			break;
		}
		if (!csr_is_encryption_in_list
			    (pMac, &pProfile2->EncryptionType,
			    pConnProfile->EncryptionType))
			break;
		if (pConnProfile->Keys.defaultIndex !=
		    pProfile2->Keys.defaultIndex)
			break;
		for (i = 0; i < CSR_MAX_NUM_KEY; i++) {
			if (pConnProfile->Keys.KeyLength[i] !=
			    pProfile2->Keys.KeyLength[i])
				break;
			if (qdf_mem_cmp(&pConnProfile->Keys.KeyMaterial[i],
					     &pProfile2->Keys.KeyMaterial[i],
					     pProfile2->Keys.KeyLength[i])) {
				break;
			}
		}
		if (i == CSR_MAX_NUM_KEY)
			fCheck = true;
	} while (0);
	return fCheck;
}

/* IBSS */

static uint8_t csr_roam_get_ibss_start_channel_number50(tpAniSirGlobal pMac)
{
	uint8_t channel = 0;
	uint32_t idx;
	uint32_t idxValidChannels;
	bool fFound = false;
	uint32_t len = sizeof(pMac->roam.validChannelList);

	if (eCSR_OPERATING_CHANNEL_ANY != pMac->roam.configParam.
							AdHocChannel5G) {
		channel = pMac->roam.configParam.AdHocChannel5G;
		if (!csr_roam_is_channel_valid(pMac, channel))
			channel = 0;
	}
	if (0 == channel
	    &&
	    QDF_IS_STATUS_SUCCESS(csr_get_cfg_valid_channels
					  (pMac, (uint8_t *) pMac->roam.
					validChannelList, &len))) {
		for (idx = 0; (idx < CSR_NUM_IBSS_START_CHAN_50) && !fFound;
		     idx++) {
			for (idxValidChannels = 0;
			     (idxValidChannels < len) && !fFound;
			     idxValidChannels++) {
				if (csr_start_ibss_channels50[idx] ==
				    pMac->roam.
				    validChannelList[idxValidChannels]) {
					fFound = true;
					channel =
						csr_start_ibss_channels50[idx];
				}
			}
		}
		/*
		 * this is rare, but if it does happen,
		 * we find anyone in 11a bandwidth and
		 * return the first 11a channel found!
		 */
		if (!fFound) {
			for (idxValidChannels = 0; idxValidChannels < len;
			     idxValidChannels++) {
				if (WLAN_REG_IS_5GHZ_CH(pMac->roam.
					validChannelList[idxValidChannels])) {
					/* the max channel# in 11g is 14 */
					if (idxValidChannels <
					    CSR_NUM_IBSS_START_CHAN_50) {
						channel =
						pMac->roam.validChannelList
						[idxValidChannels];
					}
					break;
				}
			}
		}
	} /* if */

	return channel;
}

static uint8_t csr_roam_get_ibss_start_channel_number24(tpAniSirGlobal pMac)
{
	uint8_t channel = 1;
	uint32_t idx;
	uint32_t idxValidChannels;
	bool fFound = false;
	uint32_t len = sizeof(pMac->roam.validChannelList);

	if (eCSR_OPERATING_CHANNEL_ANY != pMac->roam.configParam.
							AdHocChannel24) {
		channel = pMac->roam.configParam.AdHocChannel24;
		if (!csr_roam_is_channel_valid(pMac, channel))
			channel = 0;
	}

	if (0 == channel
	    &&
	    QDF_IS_STATUS_SUCCESS(csr_get_cfg_valid_channels(pMac,
					(uint8_t *) pMac->roam.validChannelList,
					  &len))) {
		for (idx = 0; (idx < CSR_NUM_IBSS_START_CHANNELS_24) && !fFound;
		     idx++) {
			for (idxValidChannels = 0;
			     (idxValidChannels < len) && !fFound;
			     idxValidChannels++) {
				if (csr_start_ibss_channels24[idx] ==
				    pMac->roam.
				    validChannelList[idxValidChannels]) {
					fFound = true;
					channel =
						csr_start_ibss_channels24[idx];
				}
			}
		}
	}

	return channel;
}
/**
 * csr_populate_basic_rates() - populates OFDM or CCK rates
 * @rates:         rate struct to populate
 * @is_ofdm_rates:          true: ofdm rates, false: cck rates
 * @is_basic_rates:        indicates if rates are to be masked with
 *                 CSR_DOT11_BASIC_RATE_MASK
 *
 * This function will populate OFDM or CCK rates
 *
 * Return: void
 */
static void
csr_populate_basic_rates(tSirMacRateSet *rate_set, bool is_ofdm_rates,
		bool is_basic_rates)
{
	int i = 0;
	uint8_t ofdm_rates[8] = {
		SIR_MAC_RATE_6,
		SIR_MAC_RATE_9,
		SIR_MAC_RATE_12,
		SIR_MAC_RATE_18,
		SIR_MAC_RATE_24,
		SIR_MAC_RATE_36,
		SIR_MAC_RATE_48,
		SIR_MAC_RATE_54
	};
	uint8_t cck_rates[4] = {
		SIR_MAC_RATE_1,
		SIR_MAC_RATE_2,
		SIR_MAC_RATE_5_5,
		SIR_MAC_RATE_11
	};

	if (is_ofdm_rates == true) {
		rate_set->numRates = 8;
		qdf_mem_copy(rate_set->rate, ofdm_rates, sizeof(ofdm_rates));
		if (is_basic_rates) {
			rate_set->rate[0] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[2] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[4] |= CSR_DOT11_BASIC_RATE_MASK;
		}
		for (i = 0; i < rate_set->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("Default OFDM rate is %2x"), rate_set->rate[i]);
	} else {
		rate_set->numRates = 4;
		qdf_mem_copy(rate_set->rate, cck_rates, sizeof(cck_rates));
		if (is_basic_rates) {
			rate_set->rate[0] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[1] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[2] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[3] |= CSR_DOT11_BASIC_RATE_MASK;
		}
		for (i = 0; i < rate_set->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("Default CCK rate is %2x"), rate_set->rate[i]);

	}
}

/**
 * csr_convert_mode_to_nw_type() - convert mode into network type
 * @dot11_mode:    dot11_mode
 * @band:          2.4 or 5 GHz
 *
 * Return: tSirNwType
 */
static tSirNwType
csr_convert_mode_to_nw_type(enum csr_cfgdot11mode dot11_mode,
			    enum band_info band)
{
	switch (dot11_mode) {
	case eCSR_CFG_DOT11_MODE_11G:
		return eSIR_11G_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11B:
		return eSIR_11B_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11A:
		return eSIR_11A_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11N:
	default:
		/*
		 * Because LIM only verifies it against 11a, 11b or 11g, set
		 * only 11g or 11a here
		 */
		if (BAND_2G == band)
			return eSIR_11G_NW_TYPE;
		else
			return eSIR_11A_NW_TYPE;
	}
	return eSIR_DONOT_USE_NW_TYPE;
}

/**
 * csr_populate_supported_rates_from_hostapd() - populates operational
 * and extended rates.
 * from hostapd.conf file
 * @opr_rates:		rate struct to populate operational rates
 * @ext_rates:		rate struct to populate extended rates
 * @profile:		bss profile
 *
 * Return: void
 */
static void csr_populate_supported_rates_from_hostapd(tSirMacRateSet *opr_rates,
		tSirMacRateSet *ext_rates,
		struct csr_roam_profile *profile)
{
	int i = 0;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("supported_rates: %d extended_rates: %d"),
			profile->supported_rates.numRates,
			profile->extended_rates.numRates);

	if (profile->supported_rates.numRates > SIR_MAC_RATESET_EID_MAX)
		profile->supported_rates.numRates = SIR_MAC_RATESET_EID_MAX;

	if (profile->extended_rates.numRates > SIR_MAC_RATESET_EID_MAX)
		profile->extended_rates.numRates = SIR_MAC_RATESET_EID_MAX;

	if (profile->supported_rates.numRates) {
		opr_rates->numRates = profile->supported_rates.numRates;
		qdf_mem_copy(opr_rates->rate,
				profile->supported_rates.rate,
				profile->supported_rates.numRates);
		for (i = 0; i < opr_rates->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Supported Rate is %2x"), opr_rates->rate[i]);
	}
	if (profile->extended_rates.numRates) {
		ext_rates->numRates =
			profile->extended_rates.numRates;
		qdf_mem_copy(ext_rates->rate,
				profile->extended_rates.rate,
				profile->extended_rates.numRates);
		for (i = 0; i < ext_rates->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Extended Rate is %2x"), ext_rates->rate[i]);
	}
}

/**
 * csr_roam_get_bss_start_parms() - get bss start param from profile
 * @pMac:          mac global context
 * @pProfile:      roam profile
 * @pParam:        out param, start bss params
 * @skip_hostapd_rate: to skip given hostapd's rate
 *
 * This function populates start bss param from roam profile
 *
 * Return: void
 */
static QDF_STATUS
csr_roam_get_bss_start_parms(tpAniSirGlobal pMac,
			     struct csr_roam_profile *pProfile,
			     struct csr_roamstart_bssparams *pParam,
			     bool skip_hostapd_rate)
{
	enum band_info band;
	uint8_t opr_ch = 0;
	tSirNwType nw_type;
	uint8_t tmp_opr_ch = 0;
	tSirMacRateSet *opr_rates = &pParam->operationalRateSet;
	tSirMacRateSet *ext_rates = &pParam->extendedRateSet;

	if (pProfile->ChannelInfo.numOfChannels
	    && pProfile->ChannelInfo.ChannelList)
		tmp_opr_ch = pProfile->ChannelInfo.ChannelList[0];

	pParam->uCfgDot11Mode = csr_roam_get_phy_mode_band_for_bss(pMac,
					 pProfile, tmp_opr_ch, &band);

	if (((pProfile->csrPersona == QDF_P2P_CLIENT_MODE)
	    || (pProfile->csrPersona == QDF_P2P_GO_MODE))
	    && (pParam->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11B)) {
		/* This should never happen */
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
			 "For P2P (persona %d) dot11_mode is 11B",
			  pProfile->csrPersona);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	nw_type = csr_convert_mode_to_nw_type(pParam->uCfgDot11Mode, band);
	ext_rates->numRates = 0;
	/*
	 * hostapd.conf will populate its basic and extended rates
	 * as per hw_mode, but if acs in ini is enabled driver should
	 * ignore basic and extended rates from hostapd.conf and should
	 * populate default rates.
	 */
	if (!cds_is_sub_20_mhz_enabled() && !skip_hostapd_rate &&
			(pProfile->supported_rates.numRates ||
			pProfile->extended_rates.numRates)) {
		csr_populate_supported_rates_from_hostapd(opr_rates,
				ext_rates, pProfile);
		pParam->operationChn = tmp_opr_ch;
	} else {
		switch (nw_type) {
		default:
			sme_err(
				"sees an unknown pSirNwType (%d)",
				nw_type);
			return QDF_STATUS_E_INVAL;
		case eSIR_11A_NW_TYPE:
			csr_populate_basic_rates(opr_rates, true, true);
			if (eCSR_OPERATING_CHANNEL_ANY != tmp_opr_ch) {
				opr_ch = tmp_opr_ch;
				break;
			}
			opr_ch = csr_roam_get_ibss_start_channel_number50(pMac);
			if (0 == opr_ch &&
				CSR_IS_PHY_MODE_DUAL_BAND(pProfile->phyMode) &&
				CSR_IS_PHY_MODE_DUAL_BAND(
					pMac->roam.configParam.phyMode)) {
				/*
				 * We could not find a 5G channel by auto pick,
				 * let's try 2.4G channels. We only do this here
				 * because csr_roam_get_phy_mode_band_for_bss
				 * always picks 11a  for AUTO
				 */
				nw_type = eSIR_11B_NW_TYPE;
				opr_ch =
				csr_roam_get_ibss_start_channel_number24(pMac);
				csr_populate_basic_rates(opr_rates, false,
								true);
			}
			break;
		case eSIR_11B_NW_TYPE:
			csr_populate_basic_rates(opr_rates, false, true);
			if (eCSR_OPERATING_CHANNEL_ANY == tmp_opr_ch)
				opr_ch =
				csr_roam_get_ibss_start_channel_number24(pMac);
			else
				opr_ch = tmp_opr_ch;
			break;
		case eSIR_11G_NW_TYPE:
			/* For P2P Client and P2P GO, disable 11b rates */
			if ((pProfile->csrPersona == QDF_P2P_CLIENT_MODE) ||
				(pProfile->csrPersona == QDF_P2P_GO_MODE) ||
				(eCSR_CFG_DOT11_MODE_11G_ONLY ==
					pParam->uCfgDot11Mode)) {
				csr_populate_basic_rates(opr_rates, true, true);
			} else {
				csr_populate_basic_rates(opr_rates, false,
								true);
				csr_populate_basic_rates(ext_rates, true,
								false);
			}
			if (eCSR_OPERATING_CHANNEL_ANY == tmp_opr_ch)
				opr_ch =
				csr_roam_get_ibss_start_channel_number24(pMac);
			else
				opr_ch = tmp_opr_ch;
			break;
		}
		pParam->operationChn = opr_ch;
	}

	pParam->sirNwType = nw_type;
	pParam->ch_params.ch_width = pProfile->ch_params.ch_width;
	pParam->ch_params.center_freq_seg0 =
		pProfile->ch_params.center_freq_seg0;
	pParam->ch_params.center_freq_seg1 =
		pProfile->ch_params.center_freq_seg1;
	pParam->ch_params.sec_ch_offset =
		pProfile->ch_params.sec_ch_offset;
	return QDF_STATUS_SUCCESS;
}

static void
csr_roam_get_bss_start_parms_from_bss_desc(
					tpAniSirGlobal pMac,
					tSirBssDescription *pBssDesc,
					tDot11fBeaconIEs *pIes,
					struct csr_roamstart_bssparams *pParam)
{
	if (!pParam) {
		sme_err("BSS param's pointer is NULL");
		return;
	}

	pParam->sirNwType = pBssDesc->nwType;
	pParam->cbMode = PHY_SINGLE_CHANNEL_CENTERED;
	pParam->operationChn = pBssDesc->channelId;
	qdf_mem_copy(&pParam->bssid, pBssDesc->bssId,
						sizeof(struct qdf_mac_addr));

	if (!pIes) {
		pParam->ssId.length = 0;
		pParam->operationalRateSet.numRates = 0;
		sme_err("IEs struct pointer is NULL");
		return;
	}

	if (pIes->SuppRates.present) {
		pParam->operationalRateSet.numRates = pIes->SuppRates.num_rates;
		if (pIes->SuppRates.num_rates > SIR_MAC_RATESET_EID_MAX) {
			sme_err(
				"num_rates: %d > max val, resetting",
				pIes->SuppRates.num_rates);
			pIes->SuppRates.num_rates = SIR_MAC_RATESET_EID_MAX;
		}
		qdf_mem_copy(pParam->operationalRateSet.rate,
			     pIes->SuppRates.rates,
			     sizeof(*pIes->SuppRates.rates) *
			     pIes->SuppRates.num_rates);
	}
	if (pIes->ExtSuppRates.present) {
		pParam->extendedRateSet.numRates = pIes->ExtSuppRates.num_rates;
		if (pIes->ExtSuppRates.num_rates > SIR_MAC_RATESET_EID_MAX) {
			sme_err(
				"num_rates: %d > max val, resetting",
				pIes->ExtSuppRates.num_rates);
			pIes->ExtSuppRates.num_rates = SIR_MAC_RATESET_EID_MAX;
		}
		qdf_mem_copy(pParam->extendedRateSet.rate,
			     pIes->ExtSuppRates.rates,
			     sizeof(*pIes->ExtSuppRates.rates) *
			     pIes->ExtSuppRates.num_rates);
	}
	if (pIes->SSID.present) {
		pParam->ssId.length = pIes->SSID.num_ssid;
		qdf_mem_copy(pParam->ssId.ssId, pIes->SSID.ssid,
			     pParam->ssId.length);
	}
	pParam->cbMode = csr_get_cb_mode_from_ies(pMac, pParam->operationChn,
						  pIes);
}

static void csr_roam_determine_max_rate_for_ad_hoc(tpAniSirGlobal pMac,
						   tSirMacRateSet *pSirRateSet)
{
	uint8_t MaxRate = 0;
	uint32_t i;
	uint8_t *pRate;

	pRate = pSirRateSet->rate;
	for (i = 0; i < pSirRateSet->numRates; i++) {
		MaxRate = CSR_MAX(MaxRate, (pRate[i] &
						(~CSR_DOT11_BASIC_RATE_MASK)));
	}

	/* Save the max rate in the connected state information.
	 * modify LastRates variable as well
	 */

}

QDF_STATUS csr_roam_issue_start_bss(tpAniSirGlobal pMac, uint32_t sessionId,
				    struct csr_roamstart_bssparams *pParam,
				    struct csr_roam_profile *pProfile,
				    tSirBssDescription *pBssDesc,
					uint32_t roamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum band_info eBand;
	/* Set the roaming substate to 'Start BSS attempt'... */
	csr_roam_substate_change(pMac, eCSR_ROAM_SUBSTATE_START_BSS_REQ,
				 sessionId);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	/* Need to figure out whether we need to log WDS??? */
	if (CSR_IS_IBSS(pProfile)) {
		host_log_ibss_pkt_type *pIbssLog;

		WLAN_HOST_DIAG_LOG_ALLOC(pIbssLog, host_log_ibss_pkt_type,
					 LOG_WLAN_IBSS_C);
		if (pIbssLog) {
			if (pBssDesc) {
				pIbssLog->eventId =
					WLAN_IBSS_EVENT_JOIN_IBSS_REQ;
				qdf_mem_copy(pIbssLog->bssid.bytes,
					pBssDesc->bssId, QDF_MAC_ADDR_SIZE);
			} else
				pIbssLog->eventId =
					WLAN_IBSS_EVENT_START_IBSS_REQ;

			qdf_mem_copy(pIbssLog->ssid, pParam->ssId.ssId,
				     pParam->ssId.length);
			if (pProfile->ChannelInfo.numOfChannels == 0)
				pIbssLog->channelSetting = AUTO_PICK;
			else
				pIbssLog->channelSetting = SPECIFIED;

			pIbssLog->operatingChannel = pParam->operationChn;
			WLAN_HOST_DIAG_LOG_REPORT(pIbssLog);
		}
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	/* Put RSN information in for Starting BSS */
	pParam->nRSNIELength = (uint16_t) pProfile->nRSNReqIELength;
	pParam->pRSNIE = pProfile->pRSNReqIE;

	pParam->privacy = pProfile->privacy;
	pParam->fwdWPSPBCProbeReq = pProfile->fwdWPSPBCProbeReq;
	pParam->authType = pProfile->csr80211AuthType;
	pParam->beaconInterval = pProfile->beaconInterval;
	pParam->dtimPeriod = pProfile->dtimPeriod;
	pParam->ApUapsdEnable = pProfile->ApUapsdEnable;
	pParam->ssidHidden = pProfile->SSIDs.SSIDList[0].ssidHidden;
	if (CSR_IS_INFRA_AP(pProfile) && (pParam->operationChn != 0)) {
		if (csr_is_valid_channel(pMac, pParam->operationChn) !=
		    QDF_STATUS_SUCCESS) {
			pParam->operationChn = INFRA_AP_DEFAULT_CHANNEL;
			pParam->ch_params.ch_width = CH_WIDTH_20MHZ;
		}
	}
	pParam->protEnabled = pProfile->protEnabled;
	pParam->obssProtEnabled = pProfile->obssProtEnabled;
	pParam->ht_protection = pProfile->cfg_protection;
	pParam->wps_state = pProfile->wps_state;

	pParam->uCfgDot11Mode =
		csr_roam_get_phy_mode_band_for_bss(pMac, pProfile,
						   pParam->
						   operationChn,
						   &eBand);
	pParam->bssPersona = pProfile->csrPersona;

#ifdef WLAN_FEATURE_11W
	pParam->mfpCapable = (0 != pProfile->MFPCapable);
	pParam->mfpRequired = (0 != pProfile->MFPRequired);
#endif

	pParam->addIeParams.probeRespDataLen =
		pProfile->addIeParams.probeRespDataLen;
	pParam->addIeParams.probeRespData_buff =
		pProfile->addIeParams.probeRespData_buff;

	pParam->addIeParams.assocRespDataLen =
		pProfile->addIeParams.assocRespDataLen;
	pParam->addIeParams.assocRespData_buff =
		pProfile->addIeParams.assocRespData_buff;

	if (CSR_IS_IBSS(pProfile)) {
		pParam->addIeParams.probeRespBCNDataLen =
			pProfile->nWPAReqIELength;
		pParam->addIeParams.probeRespBCNData_buff = pProfile->pWPAReqIE;
	} else {
		pParam->addIeParams.probeRespBCNDataLen =
			pProfile->addIeParams.probeRespBCNDataLen;
		pParam->addIeParams.probeRespBCNData_buff =
			pProfile->addIeParams.probeRespBCNData_buff;
	}
	pParam->sap_dot11mc = pProfile->sap_dot11mc;
	pParam->cac_duration_ms = pProfile->cac_duration_ms;
	pParam->dfs_regdomain = pProfile->dfs_regdomain;
	pParam->beacon_tx_rate = pProfile->beacon_tx_rate;

	/* When starting an IBSS, start on the channel from the Profile. */
	status = csr_send_mb_start_bss_req_msg(pMac, sessionId,
						pProfile->BSSType, pParam,
					      pBssDesc);
	return status;
}

void csr_roam_prepare_bss_params(tpAniSirGlobal pMac, uint32_t sessionId,
					struct csr_roam_profile *pProfile,
					tSirBssDescription *pBssDesc,
					struct bss_config_param *pBssConfig,
					tDot11fBeaconIEs *pIes)
{
	uint8_t Channel;
	ePhyChanBondState cbMode = PHY_SINGLE_CHANNEL_CENTERED;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	bool skip_hostapd_rate = !pProfile->chan_switch_hostapd_rate_enabled;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (pBssDesc) {
		csr_roam_get_bss_start_parms_from_bss_desc(pMac, pBssDesc, pIes,
							&pSession->bssParams);
		if (CSR_IS_NDI(pProfile)) {
			qdf_copy_macaddr(&pSession->bssParams.bssid,
				&pSession->selfMacAddr);
		}
	} else {
		csr_roam_get_bss_start_parms(pMac, pProfile,
					     &pSession->bssParams,
					     skip_hostapd_rate);
		/* Use the first SSID */
		if (pProfile->SSIDs.numOfSSIDs)
			qdf_mem_copy(&pSession->bssParams.ssId,
				     pProfile->SSIDs.SSIDList,
				     sizeof(tSirMacSSid));
		if (pProfile->BSSIDs.numOfBSSIDs)
			/* Use the first BSSID */
			qdf_mem_copy(&pSession->bssParams.bssid,
				     pProfile->BSSIDs.bssid,
				     sizeof(struct qdf_mac_addr));
		else
			qdf_mem_zero(&pSession->bssParams.bssid,
				    sizeof(struct qdf_mac_addr));
	}
	Channel = pSession->bssParams.operationChn;
	/* Set operating channel in pProfile which will be used */
	/* in csr_roam_set_bss_config_cfg() to determine channel bonding */
	/* mode and will be configured in CFG later */
	pProfile->operationChannel = Channel;

	if (Channel == 0)
		sme_err("CSR cannot find a channel to start IBSS");
	else {
		csr_roam_determine_max_rate_for_ad_hoc(pMac,
						       &pSession->bssParams.
						       operationalRateSet);
		if (CSR_IS_INFRA_AP(pProfile) || CSR_IS_START_IBSS(pProfile)) {
			if (WLAN_REG_IS_24GHZ_CH(Channel)) {
				cbMode =
					pMac->roam.configParam.
					channelBondingMode24GHz;
			} else {
				cbMode =
					pMac->roam.configParam.
					channelBondingMode5GHz;
			}
			sme_debug("## cbMode %d", cbMode);
			pBssConfig->cbMode = cbMode;
			pSession->bssParams.cbMode = cbMode;
		}
	}
}

static QDF_STATUS csr_roam_start_ibss(tpAniSirGlobal pMac, uint32_t sessionId,
				      struct csr_roam_profile *pProfile,
				      bool *pfSameIbss)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool fSameIbss = false;

	if (csr_is_conn_state_ibss(pMac, sessionId)) {
		/* Check if any profile parameter has changed ? If any profile
		 * parameter has changed then stop old BSS and start a new one
		 * with new parameters
		 */
		if (csr_is_same_profile(pMac,
				&pMac->roam.roamSession[sessionId].
				connectedProfile, pProfile))
			fSameIbss = true;
		else
			status = csr_roam_issue_stop_bss(pMac, sessionId,
				eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING);

	} else if (csr_is_conn_state_connected_infra(pMac, sessionId))
		/* Disassociate from the connected Infrastructure network... */
		status = csr_roam_issue_disassociate(pMac, sessionId,
				eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING,
						    false);
	else {
		struct bss_config_param *pBssConfig;

		pBssConfig = qdf_mem_malloc(sizeof(struct bss_config_param));
		if (NULL == pBssConfig)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (QDF_IS_STATUS_SUCCESS(status)) {
			/* there is no Bss description before we start an IBSS
			 * so we need to adopt all Bss configuration parameters
			 * from the Profile.
			 */
			status = csr_roam_prepare_bss_config_from_profile(pMac,
								pProfile,
								pBssConfig,
								NULL);
			if (QDF_IS_STATUS_SUCCESS(status)) {
				/* save dotMode */
				pMac->roam.roamSession[sessionId].bssParams.
				uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
				/* Prepare some more parameters for this IBSS */
				csr_roam_prepare_bss_params(pMac, sessionId,
							    pProfile, NULL,
							    pBssConfig, NULL);
				status = csr_roam_set_bss_config_cfg(pMac,
								sessionId,
								pProfile, NULL,
								pBssConfig,
								NULL, false);
			}

			qdf_mem_free(pBssConfig);
		} /* Allocate memory */
	}

	if (pfSameIbss)
		*pfSameIbss = fSameIbss;
	return status;
}

static void csr_roam_update_connected_profile_from_new_bss(tpAniSirGlobal pMac,
							   uint32_t sessionId,
						tSirSmeNewBssInfo *pNewBss)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (pNewBss) {
		/* Set the operating channel. */
		pSession->connectedProfile.operationChannel =
			pNewBss->channelNumber;
		/* move the BSSId from the BSS description into the connected
		 * state information.
		 */
		qdf_mem_copy(&pSession->connectedProfile.bssid.bytes,
			     &(pNewBss->bssId), sizeof(struct qdf_mac_addr));
	}
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void csr_get_pmk_info(tpAniSirGlobal mac_ctx, uint8_t session_id,
			  tPmkidCacheInfo *pmk_cache)
{
	struct csr_roam_session *session = NULL;

	if (!mac_ctx) {
		sme_err("Mac_ctx is NULL");
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("session %d not found", session_id);
		return;
	}
	qdf_mem_copy(pmk_cache->pmk, session->psk_pmk,
					sizeof(session->psk_pmk));
	pmk_cache->pmk_len = session->pmk_len;
}

QDF_STATUS csr_roam_set_psk_pmk(tpAniSirGlobal pMac, uint32_t sessionId,
				uint8_t *pPSK_PMK, size_t pmk_len)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(pSession->psk_pmk, pPSK_PMK, sizeof(pSession->psk_pmk));
	pSession->pmk_len = pmk_len;

	if (csr_is_auth_type_ese(pMac->roam.roamSession[sessionId].
				connectedProfile.AuthType)) {
		sme_debug("PMK update is not required for ESE");
		return QDF_STATUS_SUCCESS;
	}

	csr_roam_offload_scan(pMac, sessionId,
			      ROAM_SCAN_OFFLOAD_UPDATE_CFG,
			      REASON_ROAM_PSK_PMK_CHANGED);
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_set_pmkid(struct csr_roam_session *pSession)
{
	WLAN_HOST_DIAG_EVENT_DEF(secEvent,
				 host_event_wlan_security_payload_type);
	qdf_mem_zero(&secEvent,
	    sizeof(host_event_wlan_security_payload_type));
	secEvent.eventId = WLAN_SECURITY_EVENT_PMKID_UPDATE;
	secEvent.encryptionModeMulticast =
		(uint8_t) diag_enc_type_from_csr_type(
			pSession->connectedProfile.mcEncryptionType);
	secEvent.encryptionModeUnicast =
		(uint8_t) diag_enc_type_from_csr_type(
			pSession->connectedProfile.EncryptionType);
	qdf_mem_copy(secEvent.bssid,
		     pSession->connectedProfile.bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	secEvent.authMode = (uint8_t) diag_auth_type_from_csr_type(
				pSession->connectedProfile.AuthType);
	WLAN_HOST_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * csr_update_session_psk_pmk - API to update PMK in csr session
 * @pSession: pointer to session
 * @pmksa: pointer to PMKSA struct
 *
 * Return : None
 */
static void
csr_update_session_psk_pmk(struct csr_roam_session *session,
			   tPmkidCacheInfo *pmksa)
{
	/* For SAE authentication, pmk will be sent over the
	 * set PMKSA vendor command. The set PMKSA command is sent
	 * after SAE authentication is complete, before association
	 * completion itself. So csr_roam_session will not be having
	 * any parameters at this point. This pmk received is not
	 * updated to csr session and when RSO update command is sent,
	 * empty pmk will be sent, resulting in SAE roming failure. So
	 * copy the pmk into csr session so that correct pmk will be
	 * sent in RSO command.
	 */

	qdf_mem_copy(session->psk_pmk, pmksa->pmk, pmksa->pmk_len);
	session->pmk_len = pmksa->pmk_len;
}
#else
static inline void
csr_update_session_psk_pmk(struct csr_roam_session *session,
			   tPmkidCacheInfo *pmksa)
{}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_update_sae_single_pmk_info - API to update Single PMK Info in csr session
 * @pSession: pointer to session
 * @pmksa: pointer to PMKSA struct
 *
 * Return : None
 */
static void
csr_update_sae_single_pmk_info(struct csr_roam_session *session,
			       tPmkidCacheInfo *pmksa)
{
	if (!session || !session->pCurRoamProfile || !pmksa) {
		sme_debug("Invalid session, current profile or pmksa");
		return;
	}

	if (session->pCurRoamProfile->AuthType.authType[0] ==
	    eCSR_AUTH_TYPE_SAE && session->single_pmk_info.sae_single_pmk_ap) {
		qdf_mem_copy(session->single_pmk_info.pmk_info.pmk,
			     pmksa->pmk, pmksa->pmk_len);
		session->single_pmk_info.pmk_info.pmk_len = pmksa->pmk_len;
	}
}
#else
static inline void
csr_update_sae_single_pmk_info(struct csr_roam_session *session,
			       tPmkidCacheInfo *pmksa)
{
}
#endif
/**
 * csr_update_pmk_cache - API to update PMK cache
 * @pSession: pointer to session
 * @pmksa: pointer to PMKSA struct
 *
 * Return : None
 */
static void csr_update_pmk_cache(struct csr_roam_session *session,
			tPmkidCacheInfo *pmksa)
{
	uint16_t cache_idx = session->curr_cache_idx;

	/* Add entry to the cache */
	if (!pmksa->ssid_len) {
		qdf_copy_macaddr(
		    &session->PmkidCacheInfo[cache_idx].BSSID,
		    &pmksa->BSSID);
		session->PmkidCacheInfo[cache_idx].ssid_len = 0;
	} else {
		qdf_mem_copy(session->PmkidCacheInfo[cache_idx].ssid,
			pmksa->ssid, pmksa->ssid_len);
		session->PmkidCacheInfo[cache_idx].ssid_len =
			pmksa->ssid_len;
		qdf_mem_copy(session->PmkidCacheInfo[cache_idx].cache_id,
			pmksa->cache_id, CACHE_ID_LEN);

	}
	qdf_mem_copy(
	    session->PmkidCacheInfo[cache_idx].PMKID,
	    pmksa->PMKID, CSR_RSN_PMKID_SIZE);

	if (pmksa->pmk_len) {
		qdf_mem_copy(session->PmkidCacheInfo[cache_idx].pmk,
			     pmksa->pmk, pmksa->pmk_len);

		csr_update_session_psk_pmk(session, pmksa);
		csr_update_sae_single_pmk_info(session, pmksa);
	}

	session->PmkidCacheInfo[cache_idx].pmk_len = pmksa->pmk_len;

	/* Increment the CSR local cache index */
	if (cache_idx < (CSR_MAX_PMKID_ALLOWED - 1))
		session->curr_cache_idx++;
	else {
		sme_debug("max value reached, setting current index as 0");
		session->curr_cache_idx = 0;
	}

	session->NumPmkidCache++;
	if (session->NumPmkidCache > CSR_MAX_PMKID_ALLOWED) {
		sme_debug("setting num pmkid cache to %d",
			CSR_MAX_PMKID_ALLOWED);
		session->NumPmkidCache = CSR_MAX_PMKID_ALLOWED;
	}
}

QDF_STATUS
csr_roam_set_pmkid_cache(tpAniSirGlobal pMac, uint32_t sessionId,
			 tPmkidCacheInfo *pPMKIDCache, uint32_t numItems,
			 bool update_entire_cache)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	uint32_t i = 0;
	tPmkidCacheInfo *pmksa;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("numItems = %d", numItems);

	if (numItems > CSR_MAX_PMKID_ALLOWED)
		return QDF_STATUS_E_INVAL;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_roam_diag_set_pmkid(pSession);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

	if (update_entire_cache) {
		if (numItems && pPMKIDCache) {
			pSession->NumPmkidCache = (uint16_t) numItems;
			qdf_mem_copy(pSession->PmkidCacheInfo, pPMKIDCache,
				sizeof(tPmkidCacheInfo) * numItems);
			pSession->curr_cache_idx = (uint16_t)numItems;
		}
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < numItems; i++) {
		pmksa = &pPMKIDCache[i];

		/* Delete the entry if present */
		csr_roam_del_pmkid_from_cache(pMac, sessionId,
				pmksa, false);
		/* Update new entry */
		csr_update_pmk_cache(pSession, pmksa);

	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_mem_zero_psk_pmk(struct csr_roam_session *session)
{
	qdf_mem_zero(session->psk_pmk, sizeof(session->psk_pmk));
	session->pmk_len = 0;
}
#else
static void csr_mem_zero_psk_pmk(struct csr_roam_session *session)
{
}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
void csr_clear_sae_single_pmk(tpAniSirGlobal pMac, uint8_t vdev_id,
			      tPmkidCacheInfo *pmk_cache)
{
	struct csr_roam_session *session = CSR_GET_SESSION(pMac, vdev_id);
	tPmkidCacheInfo *pmk_to_del;
	uint8_t i;

	if (!session) {
		sme_err("session %d not found", vdev_id);
		return;
	}

	if (!pmk_cache) {
		sme_debug("Flush sae_single_pmk info");
		qdf_mem_zero(&session->single_pmk_info.pmk_info,
			sizeof(session->single_pmk_info.pmk_info));
		return;
	}

	for (i = 0; i < session->NumPmkidCache; i++) {
		pmk_to_del = &session->PmkidCacheInfo[i];
		if (qdf_is_macaddr_equal(&pmk_cache->BSSID,
					 &pmk_to_del->BSSID)) {
			sme_debug("Match found BSSID: " MAC_ADDRESS_STR " to cached BSSID:"
				  MAC_ADDRESS_STR,
				  MAC_ADDR_ARRAY(pmk_cache->BSSID.bytes),
				  MAC_ADDR_ARRAY(pmk_to_del->BSSID.bytes));
			if (pmk_to_del->pmk_len !=
				   session->single_pmk_info.pmk_info.pmk_len) {
				sme_debug("Invalid pmk len");
				return;
			} else if (!qdf_mem_cmp(
					&session->single_pmk_info.pmk_info.pmk,
					pmk_to_del->pmk, pmk_to_del->pmk_len)) {
				sme_debug("Clear sae_single_pmk info");
				qdf_mem_zero(&session->single_pmk_info.pmk_info,
				    sizeof(session->single_pmk_info.pmk_info));
			}
		}
	}
}
#endif

void csr_roam_del_pmk_cache_entry(struct csr_roam_session *session,
				  tPmkidCacheInfo *cached_pmksa, u32 del_idx)
{
	u32 curr_idx;
	u32 i;

	/* Clear this - matched entry */
	qdf_mem_zero(cached_pmksa, sizeof(tPmkidCacheInfo));
	/* Match Found, Readjust the other entries */
	curr_idx = session->curr_cache_idx;
	if (del_idx < curr_idx) {
		for (i = del_idx; i < (curr_idx - 1); i++) {
			qdf_mem_copy(&session->PmkidCacheInfo[i],
				     &session->PmkidCacheInfo[i + 1],
				     sizeof(tPmkidCacheInfo));
		}

		session->curr_cache_idx--;
		qdf_mem_zero(&session->PmkidCacheInfo[session->curr_cache_idx],
			     sizeof(tPmkidCacheInfo));
	} else if (del_idx > curr_idx) {
		for (i = del_idx; i > (curr_idx); i--) {
			qdf_mem_copy(&session->PmkidCacheInfo[i],
				     &session->PmkidCacheInfo[i - 1],
				     sizeof(tPmkidCacheInfo));
		}

		qdf_mem_zero(&session->PmkidCacheInfo[session->curr_cache_idx],
			     sizeof(tPmkidCacheInfo));
	}

	/* Decrement the count since an entry is been deleted */
	session->NumPmkidCache--;
	sme_debug("PMKID at index=%d deleted, current index=%d cache count=%d",
		  del_idx, session->curr_cache_idx, session->NumPmkidCache);
}

QDF_STATUS csr_roam_del_pmkid_from_cache(tpAniSirGlobal pMac,
					 uint32_t sessionId,
					 tPmkidCacheInfo *pmksa,
					 bool flush_cache)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	bool fMatchFound = false;
	uint32_t Index;
	tPmkidCacheInfo *cached_pmksa;
	u32 del_idx;
	u8 del_pmk[CSR_RSN_MAX_PMK_LEN] = {0};


	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Check if there are no entries to delete */
	if (0 == pSession->NumPmkidCache) {
		sme_debug("No entries to delete/Flush");
		return QDF_STATUS_SUCCESS;
	}

	if (flush_cache) {
		/* Flush the entire cache */
		qdf_mem_zero(pSession->PmkidCacheInfo,
			     sizeof(tPmkidCacheInfo) * CSR_MAX_PMKID_ALLOWED);
		csr_clear_sae_single_pmk(pMac, sessionId, NULL);
		pSession->NumPmkidCache = 0;
		pSession->curr_cache_idx = 0;
		csr_mem_zero_psk_pmk(pSession);
		return QDF_STATUS_SUCCESS;
	}

	csr_clear_sae_single_pmk(pMac, sessionId, pmksa);

	/* !flush_cache - so look up in the cache */
	for (Index = 0; Index < CSR_MAX_PMKID_ALLOWED; Index++) {
		cached_pmksa = &pSession->PmkidCacheInfo[Index];
		if ((!cached_pmksa->ssid_len) &&
			qdf_is_macaddr_equal(&cached_pmksa->BSSID,
				&pmksa->BSSID))
			fMatchFound = 1;

		else if (cached_pmksa->ssid_len &&
			(!qdf_mem_cmp(cached_pmksa->ssid,
			pmksa->ssid, pmksa->ssid_len)) &&
			(!qdf_mem_cmp(cached_pmksa->cache_id,
				pmksa->cache_id, CACHE_ID_LEN)))
			fMatchFound = 1;

		if (fMatchFound) {
			/* copy the PMK of matched BSSID */
			qdf_mem_copy(del_pmk, cached_pmksa->pmk,
				     cached_pmksa->pmk_len);

			/* Free the matched entry to address null pmk_len case*/
			csr_roam_del_pmk_cache_entry(pSession, cached_pmksa,
						     Index);

			/* Search for matching PMK in session PMK cache */
			for (del_idx = 0; del_idx != pSession->NumPmkidCache;
			     del_idx++) {
				cached_pmksa =
					&pSession->PmkidCacheInfo[del_idx];
				if (cached_pmksa->pmk_len && (!qdf_mem_cmp
				    (cached_pmksa->pmk, del_pmk,
				    cached_pmksa->pmk_len))) {
					/* Delete the matched PMK cache entry */
					csr_roam_del_pmk_cache_entry(
						pSession, cached_pmksa,
						del_idx);
					/* Search again from current index as we
					 * re-adjusted entries by one position
					 */
					del_idx--;
				}
			}
			/* reset stored pmk */
			qdf_mem_zero(del_pmk, CSR_RSN_MAX_PMK_LEN);

			break;
		}
	}

	if (Index == CSR_MAX_PMKID_ALLOWED && !fMatchFound)
		sme_debug("No such PMKSA entry exists");

	return QDF_STATUS_SUCCESS;
}

uint32_t csr_roam_get_num_pmkid_cache(tpAniSirGlobal pMac, uint32_t sessionId)
{
	return pMac->roam.roamSession[sessionId].NumPmkidCache;
}

QDF_STATUS csr_roam_get_pmkid_cache(tpAniSirGlobal pMac, uint32_t sessionId,
				   uint32_t *pNum, tPmkidCacheInfo *pPmkidCache)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	tPmkidCacheInfo *pmksa;
	uint16_t i, j;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (!pNum || !pPmkidCache) {
		sme_err("Either pNum or pPmkidCache is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (pSession->NumPmkidCache == 0) {
		*pNum = 0;
		return QDF_STATUS_SUCCESS;
	}

	if (*pNum < pSession->NumPmkidCache)
		return QDF_STATUS_E_FAILURE;

	if (pSession->NumPmkidCache > CSR_MAX_PMKID_ALLOWED) {
		sme_err("NumPmkidCache :%d is more than CSR_MAX_PMKID_ALLOWED, resetting to CSR_MAX_PMKID_ALLOWED",
			pSession->NumPmkidCache);
		pSession->NumPmkidCache = CSR_MAX_PMKID_ALLOWED;
	}

	for (i = 0, j = 0; ((j < pSession->NumPmkidCache) &&
		(i < CSR_MAX_PMKID_ALLOWED)); i++) {
		/* Fill the valid entries */
		pmksa = &pSession->PmkidCacheInfo[i];
		if (!qdf_is_macaddr_zero(&pmksa->BSSID)) {
			qdf_mem_copy(pPmkidCache, pmksa,
				     sizeof(tPmkidCacheInfo));
			pPmkidCache++;
			j++;
		}
	}

	*pNum = pSession->NumPmkidCache;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_roam_get_wpa_rsn_req_ie(tpAniSirGlobal pMac, uint32_t sessionId,
				       uint32_t *pLen, uint8_t *pBuf)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t len;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pLen) {
		len = *pLen;
		*pLen = pSession->nWpaRsnReqIeLength;
		if (pBuf) {
			if (len >= pSession->nWpaRsnReqIeLength) {
				qdf_mem_copy(pBuf, pSession->pWpaRsnReqIE,
					     pSession->nWpaRsnReqIeLength);
				status = QDF_STATUS_SUCCESS;
			}
		}
	}
	return status;
}

QDF_STATUS csr_roam_get_wpa_rsn_rsp_ie(tpAniSirGlobal pMac, uint32_t sessionId,
				       uint32_t *pLen, uint8_t *pBuf)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t len;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pLen) {
		len = *pLen;
		*pLen = pSession->nWpaRsnRspIeLength;
		if (pBuf) {
			if (len >= pSession->nWpaRsnRspIeLength) {
				qdf_mem_copy(pBuf, pSession->pWpaRsnRspIE,
					     pSession->nWpaRsnRspIeLength);
				status = QDF_STATUS_SUCCESS;
			}
		}
	}
	return status;
}

#ifdef FEATURE_WLAN_WAPI
QDF_STATUS csr_roam_get_wapi_req_ie(tpAniSirGlobal pMac, uint32_t sessionId,
				    uint32_t *pLen, uint8_t *pBuf)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t len;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pLen) {
		len = *pLen;
		*pLen = pSession->nWapiReqIeLength;
		if (pBuf) {
			if (len >= pSession->nWapiReqIeLength) {
				qdf_mem_copy(pBuf, pSession->pWapiReqIE,
					     pSession->nWapiReqIeLength);
				status = QDF_STATUS_SUCCESS;
			}
		}
	}
	return status;
}

QDF_STATUS csr_roam_get_wapi_rsp_ie(tpAniSirGlobal pMac, uint32_t sessionId,
				    uint32_t *pLen, uint8_t *pBuf)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t len;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pLen) {
		len = *pLen;
		*pLen = pSession->nWapiRspIeLength;
		if (pBuf) {
			if (len >= pSession->nWapiRspIeLength) {
				qdf_mem_copy(pBuf, pSession->pWapiRspIE,
					     pSession->nWapiRspIeLength);
				status = QDF_STATUS_SUCCESS;
			}
		}
	}
	return status;
}
#endif /* FEATURE_WLAN_WAPI */
eRoamCmdStatus csr_get_roam_complete_status(tpAniSirGlobal pMac,
						uint32_t sessionId)
{
	eRoamCmdStatus retStatus = eCSR_ROAM_CONNECT_COMPLETION;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return retStatus;
	}

	if (CSR_IS_ROAMING(pSession)) {
		retStatus = eCSR_ROAM_ROAMING_COMPLETION;
		pSession->fRoaming = false;
	}
	return retStatus;
}

static QDF_STATUS csr_roam_start_wds(tpAniSirGlobal pMac, uint32_t sessionId,
				     struct csr_roam_profile *pProfile,
				     tSirBssDescription *pBssDesc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	struct bss_config_param bssConfig;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (csr_is_conn_state_ibss(pMac, sessionId)) {
		status = csr_roam_issue_stop_bss(pMac, sessionId,
				eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING);
	} else if (csr_is_conn_state_connected_infra(pMac, sessionId)) {
		/* Disassociate from the connected Infrastructure network.*/
		status = csr_roam_issue_disassociate(pMac, sessionId,
				eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING,
						    false);
	} else {
		/* We don't expect Bt-AMP HDD not to disconnect the last
		 * connection first at this time. Otherwise we need to add
		 * code to handle the situation just like IBSS. Though for
		 * WDS station, we need to send disassoc to PE first then
		 * send stop_bss to PE, before we can continue.
		 */

		if (csr_is_conn_state_wds(pMac, sessionId)) {
			QDF_ASSERT(0);
			return QDF_STATUS_E_FAILURE;
		}
		qdf_mem_zero(&bssConfig, sizeof(struct bss_config_param));
		/* Assume HDD provide bssid in profile */
		qdf_copy_macaddr(&pSession->bssParams.bssid,
				 pProfile->BSSIDs.bssid);
		/* there is no Bss description before we start an WDS so we
		 * need to adopt all Bss configuration parameters from the
		 * Profile.
		 */
		status = csr_roam_prepare_bss_config_from_profile(pMac,
								pProfile,
								&bssConfig,
								pBssDesc);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			/* Save profile for late use */
			csr_free_roam_profile(pMac, sessionId);
			pSession->pCurRoamProfile =
				qdf_mem_malloc(sizeof(struct csr_roam_profile));
			if (pSession->pCurRoamProfile != NULL) {
				csr_roam_copy_profile(pMac,
						      pSession->pCurRoamProfile,
						      pProfile);
			}
			/* Prepare some more parameters for this WDS */
			csr_roam_prepare_bss_params(pMac, sessionId, pProfile,
						NULL, &bssConfig, NULL);
			status = csr_roam_set_bss_config_cfg(pMac, sessionId,
							pProfile, NULL,
							&bssConfig, NULL,
							false);
		}
	}

	return status;
}

/**
 * csr_add_supported_5Ghz_channels()- Add valid 5Ghz channels
 * in Join req.
 * @mac_ctx: pointer to global mac structure
 * @chan_list: Pointer to channel list buffer to populate
 * @num_chan: Pointer to number of channels value to update
 * @supp_chan_ie: Boolean to check if we need to populate as IE
 *
 * This function is called to update valid 5Ghz channels
 * in Join req. If @supp_chan_ie is true, supported channels IE
 * format[chan num 1, num of channels 1, chan num 2, num of
 * channels 2, ..] is populated. Else, @chan_list would be a list
 * of supported channels[chan num 1, chan num 2..]
 *
 * Return: void
 */
static void csr_add_supported_5Ghz_channels(tpAniSirGlobal mac_ctx,
						uint8_t *chan_list,
						uint8_t *num_chnl,
						bool supp_chan_ie)
{
	uint16_t i, j;
	uint32_t size = 0;

	if (!chan_list) {
		sme_err("chan_list buffer NULL");
		return;
	}

	size = sizeof(mac_ctx->roam.validChannelList);
	if (QDF_IS_STATUS_SUCCESS
		(csr_get_cfg_valid_channels(mac_ctx,
		(uint8_t *) mac_ctx->roam.validChannelList,
				&size))) {
		for (i = 0, j = 0; i < size; i++) {
			/* Only add 5ghz channels.*/
			if (WLAN_REG_IS_5GHZ_CH
					(mac_ctx->roam.validChannelList[i])) {
				chan_list[j]
					= mac_ctx->roam.validChannelList[i];
				j++;

				if (supp_chan_ie) {
					chan_list[j] = 1;
					j++;
				}
			}
		}
		*num_chnl = (uint8_t)j;
	} else {
		sme_err("can not find any valid channel");
		*num_chnl = 0;
	}
}

/**
 * csr_set_ldpc_exception() - to set allow any LDPC exception permitted
 * @mac_ctx: Pointer to mac context
 * @session: Pointer to SME/CSR session
 * @channel: Given channel number where connection will go
 * @usr_cfg_rx_ldpc: User provided RX LDPC setting
 *
 * This API will check if hardware allows LDPC to be enabled for provided
 * channel and user has enabled the RX LDPC selection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_set_ldpc_exception(tpAniSirGlobal mac_ctx,
			struct csr_roam_session *session, uint8_t channel,
			bool usr_cfg_rx_ldpc)
{
	if (!mac_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (usr_cfg_rx_ldpc && wma_is_rx_ldpc_supported_for_channel(channel)) {
		session->htConfig.ht_rx_ldpc = 1;
		session->vht_config.ldpc_coding = 1;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"LDPC enable for chnl[%d]", channel);
	} else {
		session->htConfig.ht_rx_ldpc = 0;
		session->vht_config.ldpc_coding = 0;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"LDPC disable for chnl[%d]", channel);
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11W
/**
 * csr_is_mfpc_capable() - is MFPC capable
 * @ies: AP information element
 *
 * Return: true if MFPC capable, false otherwise
 */
bool csr_is_mfpc_capable(struct sDot11fIERSN *rsn)
{
	bool mfpc_capable = false;

	if (rsn && rsn->present &&
	    ((rsn->RSN_Cap[0] >> 7) & 0x01))
		mfpc_capable = true;

	return mfpc_capable;
}

/**
 * csr_set_mgmt_enc_type() - set mgmt enc type for PMF
 * @profile: roam profile
 * @ies: AP ie
 * @csr_join_req: csr join req
 *
 * Return: void
 */
static void csr_set_mgmt_enc_type(struct csr_roam_profile *profile,
				  tDot11fBeaconIEs *ies,
				  tSirSmeJoinReq *csr_join_req)
{
	if (profile->MFPEnabled)
		csr_join_req->MgmtEncryptionType =
					profile->mgmt_encryption_type;
	else
		csr_join_req->MgmtEncryptionType = eSIR_ED_NONE;

	if (profile->MFPEnabled &&
	   !(profile->MFPRequired) &&
	   !csr_is_mfpc_capable(&ies->RSN))
		csr_join_req->MgmtEncryptionType = eSIR_ED_NONE;
}
#else
static inline void csr_set_mgmt_enc_type(struct csr_roam_profile *profile,
					 tDot11fBeaconIEs *pIes,
					 tSirSmeJoinReq *csr_join_req)
{
}
#endif

#ifdef WLAN_FEATURE_FILS_SK
/*
 * csr_update_fils_connection_info: Copy fils connection info to join request
 * @profile: pointer to profile
 * @csr_join_req: csr join request
 *
 * Return: None
 */
static void csr_update_fils_connection_info(struct csr_roam_profile *profile,
					    tSirSmeJoinReq *csr_join_req)
{
	if (!profile->fils_con_info)
		return;

	if (profile->fils_con_info->is_fils_connection) {
		qdf_mem_copy(&csr_join_req->fils_con_info,
			     profile->fils_con_info,
			     sizeof(struct cds_fils_connection_info));
	} else {
		qdf_mem_zero(&csr_join_req->fils_con_info,
			     sizeof(struct cds_fils_connection_info));
	}
}
#else
static void csr_update_fils_connection_info(struct csr_roam_profile *profile,
					    tSirSmeJoinReq *csr_join_req)
{ }
#endif

#ifdef WLAN_FEATURE_SAE
/*
 * csr_update_sae_config() - Copy SAE info to join request
 * @csr_join_req: csr join request
 * @mac: mac context
 * @session: sme session
 *
 * Return: None
 */
static void csr_update_sae_config(tSirSmeJoinReq *csr_join_req,
	tpAniSirGlobal mac, struct csr_roam_session *session)
{
	tPmkidCacheInfo pmkid_cache;
	uint32_t index;

	qdf_mem_copy(pmkid_cache.BSSID.bytes,
		csr_join_req->bssDescription.bssId, QDF_MAC_ADDR_SIZE);

	csr_join_req->sae_pmk_cached =
	       csr_lookup_pmkid_using_bssid(mac, session, &pmkid_cache, &index);

	if (!csr_join_req->sae_pmk_cached)
		return;

	sme_debug("Found for BSSID=" QDF_MAC_ADDR_STR,
		  QDF_MAC_ADDR_ARRAY(csr_join_req->bssDescription.bssId));
}
#else
static void csr_update_sae_config(tSirSmeJoinReq *csr_join_req,
	tpAniSirGlobal mac, struct csr_roam_session *session)
{ }
#endif

/**
 * csr_get_nss_supported_by_sta_and_ap() - finds out nss from session
 * and beacon from AP
 * @vht_caps: VHT capabilities
 * @ht_caps: HT capabilities
 * @dot11_mode: dot11 mode
 *
 * Return: number of nss advertised by beacon
 */
static uint8_t csr_get_nss_supported_by_sta_and_ap(tDot11fIEVHTCaps *vht_caps,
						   tDot11fIEHTCaps *ht_caps,
						   uint32_t dot11_mode)
{
	bool vht_capability, ht_capability;

	vht_capability = IS_DOT11_MODE_VHT(dot11_mode);
	ht_capability = IS_DOT11_MODE_HT(dot11_mode);

	if (vht_capability && vht_caps->present) {
		if ((vht_caps->rxMCSMap & 0xC0) != 0xC0)
			return 4;

		if ((vht_caps->rxMCSMap & 0x30) != 0x30)
			return 3;

		if ((vht_caps->rxMCSMap & 0x0C) != 0x0C)
			return 2;
	} else if (ht_capability && ht_caps->present) {
		if (ht_caps->supportedMCSSet[3])
			return 4;

		if (ht_caps->supportedMCSSet[2])
			return 3;

		if (ht_caps->supportedMCSSet[1])
			return 2;
	}

	return 1;
}

/**
 * csr_check_vendor_ap_3_present() - Check if Vendor AP 3 is present
 * @mac_ctx: Pointer to Global MAC structure
 * @ie: Pointer to starting IE in Beacon/Probe Response
 * @ie_len: Length of all IEs combined
 *
 * For Vendor AP 3, the condition is that Vendor AP 3 IE should be present
 * and Vendor AP 4 IE should not be present.
 * If Vendor AP 3 IE is present and Vendor AP 4 IE is also present,
 * return false, else return true.
 *
 * Return: true or false
 */
static bool
csr_check_vendor_ap_3_present(tpAniSirGlobal mac_ctx, uint8_t *ie,
			      uint16_t ie_len)
{
	bool ret = true;

	if ((wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_VENDOR_AP_3_OUI,
	    SIR_MAC_VENDOR_AP_3_OUI_LEN, ie, ie_len)) &&
	    (wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_VENDOR_AP_4_OUI,
	    SIR_MAC_VENDOR_AP_4_OUI_LEN, ie, ie_len))) {
		sme_debug("Vendor OUI 3 and Vendor OUI 4 found");
		ret = false;
	}

	return ret;
}

/**
 * csr_enable_twt() - Check if its allowed to enable twt for this session
 * @ie: pointer to beacon/probe resp ie's
 *
 * TWT is allowed only if device is in 11ax mode and peer supports
 * TWT responder or if QCN ie present.
 *
 * Return: true or flase
 */
static bool csr_enable_twt(tpAniSirGlobal mac_ctx, tDot11fBeaconIEs *ie)
{
	uint32_t value;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	CFG_GET_INT(status, mac_ctx, WNI_CFG_TWT_REQUESTOR, value);
	if ((IS_FEATURE_SUPPORTED_BY_FW(DOT11AX) || value) && ie &&
	    (ie->QCN_IE.present || ie->he_cap.twt_responder)) {
		sme_debug("TWT is supported, hence disable UAPSD; twt_requestor: %d, twt respon supp: %d, QCN_IE: %d",
			  value, ie->he_cap.twt_responder, ie->QCN_IE.present);
		return true;
	}

	return false;
}

#ifdef WLAN_ADAPTIVE_11R
/**
 * csr_get_adaptive_11r_enabled() - Function to check if adaptive 11r
 * ini is enabled or disabled
 * @mac: pointer to mac context
 *
 * Return: true if adaptive 11r is enabled
 */
static bool
csr_get_adaptive_11r_enabled(tpAniSirGlobal mac)
{
	return mac->roam.configParam.enable_adaptive_11r;
}
#else
static inline bool
csr_get_adaptive_11r_enabled(tpAniSirGlobal mac)
{
	return false;
}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_update_sae_single_pmk_ap_cap() - Function to update sae single pmk ap ie
 * @session: scr session
 * @akm: AKM type
 * @config_param: csr config
 * @bss_description: bss description
 *
 * Return: true if sae single pmk feature is enabled
 */
static void
csr_update_sae_single_pmk_ap_cap(struct csr_roam_session *session,
				 eCsrAuthType akm,
				 struct csr_config *config_param,
				 tSirBssDescription *bss_description)
{
	if (akm == eCSR_AUTH_TYPE_SAE &&
	    config_param->sae_single_pmk_feature_enabled)
		session->single_pmk_info.sae_single_pmk_ap =
				bss_description->sae_single_pmk_ap;
}
#else
static inline void
csr_update_sae_single_pmk_ap_cap(struct csr_roam_session *session,
				 eCsrAuthType akm,
				 struct csr_config *config_param,
				 tSirBssDescription *bss_description)
{
}
#endif

/**
 * The communication between HDD and LIM is thru mailbox (MB).
 * Both sides will access the data structure "tSirSmeJoinReq".
 * The rule is, while the components of "tSirSmeJoinReq" can be accessed in the
 * regular way like tSirSmeJoinReq.assocType, this guideline stops at component
 * tSirRSNie;
 * any acces to the components after tSirRSNie is forbidden because the space
 * from tSirRSNie is squeezed with the component "tSirBssDescription" and since
 * the size of actual 'tSirBssDescription' varies, the receiving side should
 * keep in mind not to access the components DIRECTLY after tSirRSNie.
 */
QDF_STATUS csr_send_join_req_msg(tpAniSirGlobal pMac, uint32_t sessionId,
				 tSirBssDescription *pBssDescription,
				 struct csr_roam_profile *pProfile,
				 tDot11fBeaconIEs *pIes, uint16_t messageType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t acm_mask = 0, uapsd_mask;
	uint16_t msgLen, ieLen;
	tSirMacRateSet OpRateSet;
	tSirMacRateSet ExRateSet;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	uint32_t dwTmp, ucDot11Mode = 0;
	uint8_t *wpaRsnIE = NULL;
	uint8_t txBFCsnValue = 0;
	tSirSmeJoinReq *csr_join_req;
	tSirMacCapabilityInfo *pAP_capabilityInfo;
	bool fTmp;
	int8_t pwrLimit = 0;
	struct ps_global_info *ps_global_info = &pMac->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[sessionId];
	uint8_t ese_config = 0;
	tpCsrNeighborRoamControlInfo neigh_roam_info;
	uint32_t value = 0, value1 = 0;
	QDF_STATUS packetdump_timer_status;
	bool is_vendor_ap_present;
	struct vdev_type_nss *vdev_type_nss;
	struct action_oui_search_attr vendor_ap_search_attr;
	tDot11fIEVHTCaps *vht_caps = NULL;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mlme_priv_obj *vdev_mlme;
	eCsrAuthType akm;
	bool reconn_after_assoc_timeout = false;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* To satisfy klockworks */
	if (NULL == pBssDescription) {
		sme_err(" pBssDescription is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	neigh_roam_info = &pMac->roam.neighborRoamInfo[sessionId];
	if ((eWNI_SME_REASSOC_REQ == messageType) ||
	    WLAN_REG_IS_5GHZ_CH(pBssDescription->channelId)) {
		pSession->disable_hi_rssi = true;
		sme_debug("Disabling HI_RSSI, AP channel=%d, rssi=%d",
			  pBssDescription->channelId, pBssDescription->rssi);
	} else {
		pSession->disable_hi_rssi = false;
	}

	do {
		pSession->joinFailStatusCode.statusCode = eSIR_SME_SUCCESS;
		pSession->joinFailStatusCode.reasonCode = 0;
		qdf_mem_copy(&pSession->joinFailStatusCode.bssId,
		       &pBssDescription->bssId, sizeof(tSirMacAddr));
		/*
		 * the tSirSmeJoinReq which includes a single
		 * bssDescription. it includes a single uint32_t for the
		 * IE fields, but the length field in the bssDescription
		 * needs to be interpreted to determine length of IE fields
		 * So, take the size of the tSirSmeJoinReq, subtract  size of
		 * bssDescription, add the number of bytes indicated by the
		 * length field of the bssDescription, add the size of length
		 * field because it not included in the length field.
		 */
		msgLen = sizeof(tSirSmeJoinReq) - sizeof(*pBssDescription) +
				pBssDescription->length +
				sizeof(pBssDescription->length) +
				/*
				 * add in the size of the WPA IE that
				 * we may build.
				 */
				sizeof(tCsrWpaIe) + sizeof(tCsrWpaAuthIe) +
				sizeof(uint16_t);
		csr_join_req = qdf_mem_malloc(msgLen);
		if (NULL == csr_join_req)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		wpaRsnIE = qdf_mem_malloc(DOT11F_IE_RSN_MAX_LEN);
		if (NULL == wpaRsnIE)
			status = QDF_STATUS_E_NOMEM;

		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		csr_join_req->messageType = messageType;
		csr_join_req->length = msgLen;
		csr_join_req->sessionId = (uint8_t) sessionId;
		csr_join_req->transactionId = 0;
		if (pIes->SSID.present &&
		    !csr_is_nullssid(pIes->SSID.ssid,
				     pIes->SSID.num_ssid)) {
			csr_join_req->ssId.length = pIes->SSID.num_ssid;
			qdf_mem_copy(&csr_join_req->ssId.ssId, pIes->SSID.ssid,
				     pIes->SSID.num_ssid);
		} else if (pProfile->SSIDs.numOfSSIDs) {
			csr_join_req->ssId.length =
					pProfile->SSIDs.SSIDList[0].SSID.length;
			qdf_mem_copy(&csr_join_req->ssId.ssId,
				     pProfile->SSIDs.SSIDList[0].SSID.ssId,
				     csr_join_req->ssId.length);
		} else {
			csr_join_req->ssId.length = 0;
		}
		qdf_mem_copy(&csr_join_req->selfMacAddr, &pSession->selfMacAddr,
			     sizeof(tSirMacAddr));
		sme_nofl_info("vdev-%d: Connecting to %.*s " QDF_MAC_ADDR_STR
			      " rssi: %d channel: %d akm %d cipher: uc %d mc %d, CC: %c%c",
			      sessionId, csr_join_req->ssId.length,
			      csr_join_req->ssId.ssId,
			      MAC_ADDR_ARRAY(pBssDescription->bssId),
			      pBssDescription->rssi, pBssDescription->channelId,
			      pProfile->negotiatedAuthType,
			      pProfile->negotiatedUCEncryptionType,
			      pProfile->negotiatedMCEncryptionType,
			      pMac->scan.countryCodeCurrent[0],
			      pMac->scan.countryCodeCurrent[1]);
		/* bsstype */
		dwTmp = csr_translate_bsstype_to_mac_type
						(pProfile->BSSType);
		csr_join_req->bsstype = dwTmp;
		/* dot11mode */
		ucDot11Mode =
			csr_translate_to_wni_cfg_dot11_mode(pMac,
							    pSession->bssParams.
							    uCfgDot11Mode);
		akm = pProfile->negotiatedAuthType;
		csr_join_req->akm = csr_convert_csr_to_ani_akm_type(akm);

		csr_update_sae_single_pmk_ap_cap(pSession, akm,
						 &pMac->roam.configParam,
						 pBssDescription);

		if (pBssDescription->channelId <= 14
		    && false == pMac->roam.configParam.enableVhtFor24GHz
		    && WNI_CFG_DOT11_MODE_11AC == ucDot11Mode) {
			/* Need to disable VHT operation in 2.4 GHz band */
			ucDot11Mode = WNI_CFG_DOT11_MODE_11N;
		}

		if (IS_5G_CH(pBssDescription->channelId))
			vdev_type_nss = &pMac->vdev_type_nss_5g;
		else
			vdev_type_nss = &pMac->vdev_type_nss_2g;
		if (pSession->pCurRoamProfile->csrPersona ==
		    QDF_P2P_CLIENT_MODE)
			pSession->vdev_nss = vdev_type_nss->p2p_cli;
		else
			pSession->vdev_nss = vdev_type_nss->sta;
		pSession->nss = pSession->vdev_nss;

		if (pSession->nss > csr_get_nss_supported_by_sta_and_ap(
						&pIes->VHTCaps,
						&pIes->HTCaps, ucDot11Mode)) {
			pSession->nss = csr_get_nss_supported_by_sta_and_ap(
						&pIes->VHTCaps, &pIes->HTCaps,
						ucDot11Mode);
			pSession->vdev_nss = pSession->nss;
		}

		if (!pMac->roam.configParam.enable2x2)
			pSession->nss = 1;

		if (pSession->nss == 1)
			pSession->supported_nss_1x1 = true;

		ieLen = csr_get_ielen_from_bss_description(pBssDescription);

		/* Fill the Vendor AP search params */
		vendor_ap_search_attr.ie_data =
				(uint8_t *)&pBssDescription->ieFields[0];
		vendor_ap_search_attr.ie_length = ieLen;
		vendor_ap_search_attr.mac_addr = &pBssDescription->bssId[0];
		vendor_ap_search_attr.nss = csr_get_nss_supported_by_sta_and_ap(
						&pIes->VHTCaps, &pIes->HTCaps,
						ucDot11Mode);
		vendor_ap_search_attr.ht_cap = pIes->HTCaps.present;
		vendor_ap_search_attr.vht_cap = pIes->VHTCaps.present;
		vendor_ap_search_attr.enable_2g =
					IS_24G_CH(pBssDescription->channelId);
		vendor_ap_search_attr.enable_5g =
					IS_5G_CH(pBssDescription->channelId);

		is_vendor_ap_present = ucfg_action_oui_search(pMac->psoc,
					   &vendor_ap_search_attr,
					   ACTION_OUI_DISABLE_AGGRESSIVE_EDCA);

		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(pMac->psoc,
							    sessionId,
							    WLAN_LEGACY_MAC_ID);
		if (messageType == eWNI_SME_JOIN_REQ &&
		    ucfg_action_oui_search(pMac->psoc, &vendor_ap_search_attr,
					   ACTION_OUI_HOST_RECONN))
			reconn_after_assoc_timeout = true;
		mlme_set_reconn_after_assoc_timeout_flag(
				pMac->psoc, sessionId,
				reconn_after_assoc_timeout);
		if (vdev) {
			vdev_mlme = wlan_vdev_mlme_get_priv_obj(vdev);
			if (vdev_mlme) {
				if (is_vendor_ap_present)
					vdev_mlme->follow_ap_edca = true;
				else
					vdev_mlme->follow_ap_edca = false;
			}
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
		}

		is_vendor_ap_present =
				ucfg_action_oui_search(pMac->psoc,
						       &vendor_ap_search_attr,
						       ACTION_OUI_CONNECT_1X1);

		if (is_vendor_ap_present) {
			is_vendor_ap_present = csr_check_vendor_ap_3_present(
						pMac,
						vendor_ap_search_attr.ie_data,
						ieLen);
		}

		/*
		 * For WMI_ACTION_OUI_CONNECT_1x1_WITH_1_CHAIN, the host
		 * sends the NSS as 1 to the FW and the FW then decides
		 * after receiving the first beacon after connection to
		 * switch to 1 Tx/Rx Chain.
		 */

		if (!is_vendor_ap_present) {
			is_vendor_ap_present =
				ucfg_action_oui_search(pMac->psoc,
					&vendor_ap_search_attr,
					ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN);
			if (is_vendor_ap_present)
				sme_debug("1x1 with 1 Chain AP");
		}

		if ((pMac->roam.configParam.is_force_1x1_enable ==
					FORCE_1X1_ENABLED_FORCED ||
		     (pMac->roam.configParam.is_force_1x1_enable ==
					FORCE_1X1_ENABLED_FOR_AS &&
		     pMac->lteCoexAntShare)) && is_vendor_ap_present) {
			pSession->supported_nss_1x1 = true;
			pSession->vdev_nss = 1;
			pSession->nss = 1;
			pSession->nss_forced_1x1 = true;
			sme_debug("For special ap, NSS: %d force 1x1 %d",
				  pSession->nss,
				  pMac->roam.configParam.is_force_1x1_enable);
		}

		/*
		 * If CCK WAR is set for current AP, update to firmware via
		 * WMI_VDEV_PARAM_ABG_MODE_TX_CHAIN_NUM
		 */
		is_vendor_ap_present =
				ucfg_action_oui_search(pMac->psoc,
						       &vendor_ap_search_attr,
						       ACTION_OUI_CCKM_1X1);
		if (is_vendor_ap_present) {
			sme_debug("vdev: %d WMI_VDEV_PARAM_ABG_MODE_TX_CHAIN_NUM 1",
				 pSession->sessionId);
			wma_cli_set_command(
				pSession->sessionId,
				(int)WMI_VDEV_PARAM_ABG_MODE_TX_CHAIN_NUM, 1,
				VDEV_CMD);
		}

		/*
		 * If Switch to 11N WAR is set for current AP, change dot11
		 * mode to 11N.
		 */
		is_vendor_ap_present =
			ucfg_action_oui_search(pMac->psoc,
					       &vendor_ap_search_attr,
					       ACTION_OUI_SWITCH_TO_11N_MODE);
		if (pMac->roam.configParam.is_force_1x1_enable &&
		    pMac->lteCoexAntShare &&
		    is_vendor_ap_present &&
		    (ucDot11Mode == WNI_CFG_DOT11_MODE_ALL ||
		     ucDot11Mode == WNI_CFG_DOT11_MODE_11AC ||
		     ucDot11Mode == WNI_CFG_DOT11_MODE_11AC_ONLY))
			ucDot11Mode = WNI_CFG_DOT11_MODE_11N;

		csr_join_req->supported_nss_1x1 = pSession->supported_nss_1x1;
		csr_join_req->vdev_nss = pSession->vdev_nss;
		csr_join_req->nss = pSession->nss;
		csr_join_req->nss_forced_1x1 = pSession->nss_forced_1x1;
		csr_join_req->dot11mode = (uint8_t) ucDot11Mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		csr_join_req->cc_switch_mode =
			pMac->roam.configParam.cc_switch_mode;
#endif
		csr_join_req->staPersona = (uint8_t) pProfile->csrPersona;
		csr_join_req->wps_registration = pProfile->bWPSAssociation;
		csr_join_req->cbMode = (uint8_t) pSession->bssParams.cbMode;
		csr_join_req->force_24ghz_in_ht20 =
			pProfile->force_24ghz_in_ht20;
		pSession->uapsd_mask = pProfile->uapsd_mask;
		status =
			csr_get_rate_set(pMac, pProfile,
					 (eCsrPhyMode) pProfile->phyMode,
					 pBssDescription, pIes, &OpRateSet,
					 &ExRateSet);
		if (!csr_enable_twt(pMac, pIes))
			ps_param->uapsd_per_ac_bit_mask = pProfile->uapsd_mask;
		if (QDF_IS_STATUS_SUCCESS(status)) {
			/* OperationalRateSet */
			if (OpRateSet.numRates) {
				csr_join_req->operationalRateSet.numRates =
					OpRateSet.numRates;
				qdf_mem_copy(&csr_join_req->operationalRateSet.
						rate, OpRateSet.rate,
						OpRateSet.numRates);
			} else
				csr_join_req->operationalRateSet.numRates = 0;

			/* ExtendedRateSet */
			if (ExRateSet.numRates) {
				csr_join_req->extendedRateSet.numRates =
					ExRateSet.numRates;
				qdf_mem_copy(&csr_join_req->extendedRateSet.
						rate, ExRateSet.rate,
						ExRateSet.numRates);
			} else
				csr_join_req->extendedRateSet.numRates = 0;
		} else {
			csr_join_req->operationalRateSet.numRates = 0;
			csr_join_req->extendedRateSet.numRates = 0;
		}

		if (pBssDescription->adaptive_11r_ap)
			pSession->is_adaptive_11r_connection =
				csr_get_adaptive_11r_enabled(pMac);
		else
			pSession->is_adaptive_11r_connection = false;

		csr_join_req->is_adaptive_11r_connection =
				pSession->is_adaptive_11r_connection;

		/* rsnIE */
		if (csr_is_profile_wpa(pProfile)) {
			/* Insert the Wpa IE into the join request */
			ieLen =
				csr_retrieve_wpa_ie(pMac, pProfile,
						pBssDescription, pIes,
						(tCsrWpaIe *) (wpaRsnIE));
		} else if (csr_is_profile_rsn(pProfile)) {
			/* Insert the RSN IE into the join request */
			ieLen =
				csr_retrieve_rsn_ie(pMac, sessionId, pProfile,
						    pBssDescription, pIes,
						    (tCsrRSNIe *) (wpaRsnIE));
			csr_join_req->force_rsne_override =
						pProfile->force_rsne_override;
		}
#ifdef FEATURE_WLAN_WAPI
		else if (csr_is_profile_wapi(pProfile)) {
			/* Insert the WAPI IE into the join request */
			ieLen =
				csr_retrieve_wapi_ie(pMac, sessionId, pProfile,
						     pBssDescription, pIes,
						     (tCsrWapiIe *) (wpaRsnIE));
		}
#endif /* FEATURE_WLAN_WAPI */
		else
			ieLen = 0;
		/* remember the IE for future use */
		if (ieLen) {
			if (ieLen > DOT11F_IE_RSN_MAX_LEN) {
				sme_err("WPA RSN IE length :%d is more than DOT11F_IE_RSN_MAX_LEN, resetting to %d",
					ieLen, DOT11F_IE_RSN_MAX_LEN);
				ieLen = DOT11F_IE_RSN_MAX_LEN;
			}
#ifdef FEATURE_WLAN_WAPI
			if (csr_is_profile_wapi(pProfile)) {
				/* Check whether we need to allocate more mem */
				if (ieLen > pSession->nWapiReqIeLength) {
					if (pSession->pWapiReqIE
					    && pSession->nWapiReqIeLength) {
						qdf_mem_free(pSession->
							     pWapiReqIE);
					}
					pSession->pWapiReqIE =
						qdf_mem_malloc(ieLen);
					if (NULL == pSession->pWapiReqIE)
						status = QDF_STATUS_E_NOMEM;
					else
						status = QDF_STATUS_SUCCESS;
					if (!QDF_IS_STATUS_SUCCESS(status))
						break;
				}
				pSession->nWapiReqIeLength = ieLen;
				qdf_mem_copy(pSession->pWapiReqIE, wpaRsnIE,
					     ieLen);
				csr_join_req->rsnIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->rsnIE.rsnIEdata,
						 wpaRsnIE, ieLen);
			} else  /* should be WPA/WPA2 otherwise */
#endif /* FEATURE_WLAN_WAPI */
			{
				/* Check whether we need to allocate more mem */
				if (ieLen > pSession->nWpaRsnReqIeLength) {
					if (pSession->pWpaRsnReqIE
					    && pSession->nWpaRsnReqIeLength) {
						qdf_mem_free(pSession->
							     pWpaRsnReqIE);
					}
					pSession->pWpaRsnReqIE =
						qdf_mem_malloc(ieLen);
					if (NULL == pSession->pWpaRsnReqIE)
						status = QDF_STATUS_E_NOMEM;
					else
						status = QDF_STATUS_SUCCESS;
					if (!QDF_IS_STATUS_SUCCESS(status))
						break;
				}
				pSession->nWpaRsnReqIeLength = ieLen;
				qdf_mem_copy(pSession->pWpaRsnReqIE, wpaRsnIE,
					     ieLen);
				csr_join_req->rsnIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->rsnIE.rsnIEdata,
						 wpaRsnIE, ieLen);
			}
		} else {
			/* free whatever old info */
			pSession->nWpaRsnReqIeLength = 0;
			if (pSession->pWpaRsnReqIE) {
				qdf_mem_free(pSession->pWpaRsnReqIE);
				pSession->pWpaRsnReqIE = NULL;
			}
#ifdef FEATURE_WLAN_WAPI
			pSession->nWapiReqIeLength = 0;
			if (pSession->pWapiReqIE) {
				qdf_mem_free(pSession->pWapiReqIE);
				pSession->pWapiReqIE = NULL;
			}
#endif /* FEATURE_WLAN_WAPI */
			csr_join_req->rsnIE.length = 0;
		}
#ifdef FEATURE_WLAN_ESE
		if (eWNI_SME_JOIN_REQ == messageType)
			csr_join_req->cckmIE.length = 0;
		else if (eWNI_SME_REASSOC_REQ == messageType) {
			/* cckmIE */
			if (csr_is_profile_ese(pProfile)) {
				/* Insert the CCKM IE into the join request */
				ieLen = pSession->suppCckmIeInfo.cckmIeLen;
				qdf_mem_copy((void *)(wpaRsnIE),
						pSession->suppCckmIeInfo.cckmIe,
						ieLen);
			} else
				ieLen = 0;
			/*
			 * If present, copy the IE into the
			 * eWNI_SME_REASSOC_REQ message buffer
			 */
			if (ieLen) {
				/*
				 * Copy the CCKM IE over from the temp
				 * buffer (wpaRsnIE)
				 */
				csr_join_req->cckmIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->cckmIE.cckmIEdata,
						wpaRsnIE, ieLen);
			} else
				csr_join_req->cckmIE.length = 0;
		}
#endif /* FEATURE_WLAN_ESE */
		/* addIEScan */
		if (pProfile->nAddIEScanLength && pProfile->pAddIEScan) {
			ieLen = pProfile->nAddIEScanLength;
			if (ieLen > pSession->nAddIEScanLength) {
				if (pSession->pAddIEScan
					&& pSession->nAddIEScanLength) {
					qdf_mem_free(pSession->pAddIEScan);
				}
				pSession->pAddIEScan = qdf_mem_malloc(ieLen);
				if (NULL == pSession->pAddIEScan)
					status = QDF_STATUS_E_NOMEM;
				else
					status = QDF_STATUS_SUCCESS;
				if (!QDF_IS_STATUS_SUCCESS(status))
					break;
			}
			pSession->nAddIEScanLength = ieLen;
			qdf_mem_copy(pSession->pAddIEScan, pProfile->pAddIEScan,
					ieLen);
			csr_join_req->addIEScan.length = ieLen;
			qdf_mem_copy(&csr_join_req->addIEScan.addIEdata,
					pProfile->pAddIEScan, ieLen);
		} else {
			pSession->nAddIEScanLength = 0;
			if (pSession->pAddIEScan) {
				qdf_mem_free(pSession->pAddIEScan);
				pSession->pAddIEScan = NULL;
			}
			csr_join_req->addIEScan.length = 0;
		}
		/* addIEAssoc */
		if (pProfile->nAddIEAssocLength && pProfile->pAddIEAssoc) {
			ieLen = pProfile->nAddIEAssocLength;
			if (ieLen > pSession->nAddIEAssocLength) {
				if (pSession->pAddIEAssoc
				    && pSession->nAddIEAssocLength) {
					qdf_mem_free(pSession->pAddIEAssoc);
				}
				pSession->pAddIEAssoc = qdf_mem_malloc(ieLen);
				if (NULL == pSession->pAddIEAssoc)
					status = QDF_STATUS_E_NOMEM;
				else
					status = QDF_STATUS_SUCCESS;
				if (!QDF_IS_STATUS_SUCCESS(status))
					break;
			}
			pSession->nAddIEAssocLength = ieLen;
			qdf_mem_copy(pSession->pAddIEAssoc,
				     pProfile->pAddIEAssoc, ieLen);
			csr_join_req->addIEAssoc.length = ieLen;
			qdf_mem_copy(&csr_join_req->addIEAssoc.addIEdata,
					 pProfile->pAddIEAssoc, ieLen);
		} else {
			pSession->nAddIEAssocLength = 0;
			if (pSession->pAddIEAssoc) {
				qdf_mem_free(pSession->pAddIEAssoc);
				pSession->pAddIEAssoc = NULL;
			}
			csr_join_req->addIEAssoc.length = 0;
		}

		if (eWNI_SME_REASSOC_REQ == messageType) {
			/* Unmask any AC in reassoc that is ACM-set */
			uapsd_mask = (uint8_t) pProfile->uapsd_mask;
			if (uapsd_mask && (NULL != pBssDescription)) {
				if (CSR_IS_QOS_BSS(pIes)
						&& CSR_IS_UAPSD_BSS(pIes))
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
					acm_mask =
						sme_qos_get_acm_mask(pMac,
								pBssDescription,
								pIes);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT */
				else
					uapsd_mask = 0;
			}
		}

		if (!CSR_IS_11n_ALLOWED(pProfile->negotiatedUCEncryptionType))
			csr_join_req->he_with_wep_tkip =
				pMac->roam.configParam.wep_tkip_in_he;

		csr_join_req->UCEncryptionType =
				csr_translate_encrypt_type_to_ed_type
					(pProfile->negotiatedUCEncryptionType);

		csr_join_req->MCEncryptionType =
				csr_translate_encrypt_type_to_ed_type
					(pProfile->negotiatedMCEncryptionType);
	csr_set_mgmt_enc_type(pProfile, pIes, csr_join_req);
#ifdef FEATURE_WLAN_ESE
		ese_config =  pMac->roam.configParam.isEseIniFeatureEnabled;
#endif
		pProfile->MDID.mdiePresent = pBssDescription->mdiePresent;
		if (csr_is_profile11r(pMac, pProfile)
#ifdef FEATURE_WLAN_ESE
		    &&
		    !((pProfile->negotiatedAuthType ==
		       eCSR_AUTH_TYPE_OPEN_SYSTEM) && (pIes->ESEVersion.present)
		      && (ese_config))
#endif
			)
			csr_join_req->is11Rconnection = true;
		else
			csr_join_req->is11Rconnection = false;
#ifdef FEATURE_WLAN_ESE
		if (true == ese_config)
			csr_join_req->isESEFeatureIniEnabled = true;
		else
			csr_join_req->isESEFeatureIniEnabled = false;

		/* A profile can not be both ESE and 11R. But an 802.11R AP
		 * may be advertising support for ESE as well. So if we are
		 * associating Open or explicitly ESE then we will get ESE.
		 * If we are associating explicitly 11R only then we will get
		 * 11R.
		 */
		if ((csr_is_profile_ese(pProfile) ||
			((pIes->ESEVersion.present) &&
			(pProfile->negotiatedAuthType ==
				eCSR_AUTH_TYPE_OPEN_SYSTEM)))
			&& (ese_config))
			csr_join_req->isESEconnection = true;
		else
			csr_join_req->isESEconnection = false;

		if (eWNI_SME_JOIN_REQ == messageType) {
			tESETspecInfo eseTspec;
			/*
			 * ESE-Tspec IEs in the ASSOC request is presently not
			 * supported. so nullify the TSPEC parameters
			 */
			qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
			qdf_mem_copy(&csr_join_req->eseTspecInfo,
					&eseTspec, sizeof(tESETspecInfo));
		} else if (eWNI_SME_REASSOC_REQ == messageType) {
			if ((csr_is_profile_ese(pProfile) ||
				((pIes->ESEVersion.present)
				&& (pProfile->negotiatedAuthType ==
					eCSR_AUTH_TYPE_OPEN_SYSTEM))) &&
				(ese_config)) {
				tESETspecInfo eseTspec;

				qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
				eseTspec.numTspecs =
					sme_qos_ese_retrieve_tspec_info(pMac,
						sessionId,
						(tTspecInfo *) &eseTspec.
							tspec[0]);
				csr_join_req->eseTspecInfo.numTspecs =
					eseTspec.numTspecs;
				if (eseTspec.numTspecs) {
					qdf_mem_copy(&csr_join_req->eseTspecInfo
						.tspec[0],
						&eseTspec.tspec[0],
						(eseTspec.numTspecs *
							sizeof(tTspecInfo)));
				}
			} else {
				tESETspecInfo eseTspec;
				/*
				 * ESE-Tspec IEs in the ASSOC request is
				 * presently not supported. so nullify the TSPEC
				 * parameters
				 */
				qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
				qdf_mem_copy(&csr_join_req->eseTspecInfo,
						&eseTspec,
						sizeof(tESETspecInfo));
			}
		}
#endif /* FEATURE_WLAN_ESE */
		if (ese_config
		    || csr_roam_is_fast_roam_enabled(pMac, sessionId))
			csr_join_req->isFastTransitionEnabled = true;
		else
			csr_join_req->isFastTransitionEnabled = false;

		if (csr_roam_is_fast_roam_enabled(pMac, sessionId))
			csr_join_req->isFastRoamIniFeatureEnabled = true;
		else
			csr_join_req->isFastRoamIniFeatureEnabled = false;

		csr_join_req->txLdpcIniFeatureEnabled =
			(uint8_t) pMac->roam.configParam.tx_ldpc_enable;

		if ((csr_is11h_supported(pMac)) &&
			(WLAN_REG_IS_5GHZ_CH(pBssDescription->channelId)) &&
			(pIes->Country.present) &&
			(!pMac->roam.configParam.
			 fSupplicantCountryCodeHasPriority)) {
			csr_save_to_channel_power2_g_5_g(pMac,
				pIes->Country.num_triplets *
				sizeof(tSirMacChanInfo),
				(tSirMacChanInfo *)
				(&pIes->Country.triplets[0]));
			csr_apply_power2_current(pMac);
		}
		/*
		 * If RX LDPC has been disabled for 2.4GHz channels and enabled
		 * for 5Ghz for STA like persona then here is how to handle
		 * those cases (by now channel has been decided).
		 */
		if (eSIR_INFRASTRUCTURE_MODE == csr_join_req->bsstype ||
		    !policy_mgr_is_dbs_enable(pMac->psoc))
			csr_set_ldpc_exception(pMac, pSession,
					pBssDescription->channelId,
					pMac->roam.configParam.rx_ldpc_enable);
		qdf_mem_copy(&csr_join_req->htConfig,
				&pSession->htConfig, sizeof(tSirHTConfig));
		qdf_mem_copy(&csr_join_req->vht_config, &pSession->vht_config,
				sizeof(pSession->vht_config));
		sme_debug("ht capability 0x%x VHT capability 0x%x",
			(unsigned int)(*(uint32_t *) &csr_join_req->htConfig),
			(unsigned int)(*(uint32_t *) &csr_join_req->
			vht_config));

		if (IS_DOT11_MODE_HE(csr_join_req->dot11mode))
			csr_join_req_copy_he_cap(csr_join_req, pSession);

		if (wlan_cfg_get_int(pMac, WNI_CFG_VHT_SU_BEAMFORMEE_CAP,
				     &value) != QDF_STATUS_SUCCESS)
			QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
				("Failed to get SU beamformee capability"));
		if (wlan_cfg_get_int(pMac,
				WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED,
				&value1) != QDF_STATUS_SUCCESS)
			QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
				("Failed to get CSN beamformee capability"));

		csr_join_req->vht_config.su_beam_formee = value;

		if (pIes->VHTCaps.present)
			vht_caps = &pIes->VHTCaps;
		else if (pIes->vendor_vht_ie.VHTCaps.present)
			vht_caps = &pIes->vendor_vht_ie.VHTCaps;
		/* Set BF CSN value only if SU Bformee is enabled */
		if (vht_caps && csr_join_req->vht_config.su_beam_formee) {
			txBFCsnValue = (uint8_t)value1;
			/*
			 * Certain commercial AP display a bad behavior when
			 * CSN value in  assoc request is more than AP's CSN.
			 * Sending absolute self CSN value with such AP leads to
			 * IOT issues. However this issue is observed only with
			 * CSN cap of less than 4. To avoid such issues, take a
			 * min of self and peer CSN while sending ASSOC request.
			 */
			if (pIes->Vendor1IE.present &&
					vht_caps->csnofBeamformerAntSup < 4) {
				if (vht_caps->csnofBeamformerAntSup)
					txBFCsnValue = QDF_MIN(txBFCsnValue,
					  vht_caps->csnofBeamformerAntSup);
			}
		}
		csr_join_req->vht_config.csnof_beamformer_antSup = txBFCsnValue;

		if (wlan_cfg_get_int(pMac,
		   WNI_CFG_VHT_SU_BEAMFORMER_CAP, &value)
		   != QDF_STATUS_SUCCESS)
			sme_err("Failed to get SU beamformer capability");

		/*
		 * Set SU Bformer only if SU Bformer is enabled in INI
		 * and AP is SU Bformee capable
		 */
		if (value && !((IS_BSS_VHT_CAPABLE(pIes->VHTCaps) &&
		   pIes->VHTCaps.suBeamformeeCap) ||
		   (IS_BSS_VHT_CAPABLE(
		   pIes->vendor_vht_ie.VHTCaps)
		   && pIes->vendor_vht_ie.VHTCaps.
		   suBeamformeeCap)))
			value = 0;

		csr_join_req->vht_config.su_beam_former = value;

		/* Set num soundingdim value to 0 if SU Bformer is disabled */
		if (!csr_join_req->vht_config.su_beam_former)
			csr_join_req->vht_config.num_soundingdim = 0;

		if (wlan_cfg_get_int(pMac,
		   WNI_CFG_VHT_MU_BEAMFORMEE_CAP, &value)
		   != QDF_STATUS_SUCCESS)
			sme_err("Failed to get CSN beamformee capability");
		/*
		 * Set MU Bformee only if SU Bformee is enabled and
		 * MU Bformee is enabled in INI
		 */
		if (value && csr_join_req->vht_config.su_beam_formee &&
				pIes->VHTCaps.muBeamformerCap)
			csr_join_req->vht_config.mu_beam_formee = 1;
		else
			csr_join_req->vht_config.mu_beam_formee = 0;

		csr_join_req->enableVhtpAid =
			(uint8_t) pMac->roam.configParam.enableVhtpAid;

		csr_join_req->enableVhtGid =
			(uint8_t) pMac->roam.configParam.enableVhtGid;

		csr_join_req->enableAmpduPs =
			(uint8_t) pMac->roam.configParam.enableAmpduPs;

		csr_join_req->enableHtSmps =
			(uint8_t) pMac->roam.configParam.enableHtSmps;

		csr_join_req->htSmps = (uint8_t) pMac->roam.configParam.htSmps;
		csr_join_req->send_smps_action =
			pMac->roam.configParam.send_smps_action;

		csr_join_req->max_amsdu_num =
			(uint8_t) pMac->roam.configParam.max_amsdu_num;

		if (pMac->roam.roamSession[sessionId].fWMMConnection)
			csr_join_req->isWMEenabled = true;
		else
			csr_join_req->isWMEenabled = false;

		if (pMac->roam.roamSession[sessionId].fQOSConnection)
			csr_join_req->isQosEnabled = true;
		else
			csr_join_req->isQosEnabled = false;

		if (pProfile->bOSENAssociation)
			csr_join_req->isOSENConnection = true;
		else
			csr_join_req->isOSENConnection = false;

		/* Fill rrm config parameters */
		qdf_mem_copy(&csr_join_req->rrm_config,
			     &pMac->rrm.rrmConfig,
			     sizeof(struct rrm_config_param));

		pAP_capabilityInfo =
			(tSirMacCapabilityInfo *)
				&pBssDescription->capabilityInfo;
		/*
		 * tell the target AP my 11H capability only if both AP and STA
		 * support
		 * 11H and the channel being used is 11a
		 */
		if (csr_is11h_supported(pMac) && pAP_capabilityInfo->spectrumMgt
			&& eSIR_11A_NW_TYPE == pBssDescription->nwType) {
			fTmp = true;
		} else
			fTmp = false;

		csr_join_req->spectrumMgtIndicator = fTmp;
		csr_join_req->powerCap.minTxPower = MIN_TX_PWR_CAP;
		/*
		 * This is required for 11k test VoWiFi Ent: Test 2.
		 * We need the power capabilities for Assoc Req.
		 * This macro is provided by the halPhyCfg.h. We pick our
		 * max and min capability by the halPhy provided macros
		 * Any change in this power cap IE should also be done
		 * in csr_update_driver_assoc_ies() which would send
		 * assoc IE's to FW which is used for LFR3 roaming
		 * ie. used in reassociation requests from FW.
		 */
		pwrLimit = csr_get_cfg_max_tx_power(pMac,
					pBssDescription->channelId);
		if (0 != pwrLimit && pwrLimit < MAX_TX_PWR_CAP)
			csr_join_req->powerCap.maxTxPower = pwrLimit;
		else
			csr_join_req->powerCap.maxTxPower = MAX_TX_PWR_CAP;

		csr_add_supported_5Ghz_channels(pMac,
				csr_join_req->supportedChannels.channelList,
				&csr_join_req->supportedChannels.numChnl,
				false);
		/* Enable UAPSD only if TWT is not supported */
		if (!csr_enable_twt(pMac, pIes))
			csr_join_req->uapsdPerAcBitmask = pProfile->uapsd_mask;
		/* Move the entire BssDescription into the join request. */
		qdf_mem_copy(&csr_join_req->bssDescription, pBssDescription,
				pBssDescription->length +
				sizeof(pBssDescription->length));
		csr_update_fils_connection_info(pProfile, csr_join_req);
		csr_update_sae_config(csr_join_req, pMac, pSession);
		/*
		 * conc_custom_rule1:
		 * If SAP comes up first and STA comes up later then SAP
		 * need to follow STA's channel in 2.4Ghz. In following if
		 * condition we are adding sanity check, just to make sure that
		 * if this rule is enabled then don't allow STA to connect on
		 * 5gz channel and also by this time SAP's channel should be the
		 * same as STA's channel.
		 */
		if (pMac->roam.configParam.conc_custom_rule1) {
			if ((0 == pMac->roam.configParam.
				is_sta_connection_in_5gz_enabled) &&
				WLAN_REG_IS_5GHZ_CH(pBssDescription->
					channelId)) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_ERROR,
					 "STA-conn on 5G isn't allowed");
				status = QDF_STATUS_E_FAILURE;
				break;
			}
			if (!WLAN_REG_IS_5GHZ_CH(pBssDescription->channelId) &&
				(false == csr_is_conn_allow_2g_band(pMac,
						pBssDescription->channelId))) {
				status = QDF_STATUS_E_FAILURE;
				break;
			}
		}

		/*
		 * conc_custom_rule2:
		 * If P2PGO comes up first and STA comes up later then P2PGO
		 * need to follow STA's channel in 5Ghz. In following if
		 * condition we are just adding sanity check to make sure that
		 * by this time P2PGO's channel is same as STA's channel.
		 */
		if (pMac->roam.configParam.conc_custom_rule2 &&
			!WLAN_REG_IS_24GHZ_CH(pBssDescription->channelId) &&
			(!csr_is_conn_allow_5g_band(pMac,
						pBssDescription->channelId))) {
			status = QDF_STATUS_E_FAILURE;
			break;
		}

		if (pSession->pCurRoamProfile->csrPersona == QDF_STA_MODE)
			csr_join_req->enable_bcast_probe_rsp =
				pMac->roam.configParam.enable_bcast_probe_rsp;

		csr_join_req->enable_session_twt_support = csr_enable_twt(pMac,
									  pIes);
		status = umac_send_mb_message_to_mac(csr_join_req);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/*
			 * umac_send_mb_message_to_mac would've released the mem
			 * allocated by csr_join_req. Let's make it defensive by
			 * assigning NULL to the pointer.
			 */
			csr_join_req = NULL;
			break;
		}

		if (pProfile->csrPersona == QDF_STA_MODE) {
			wlan_register_txrx_packetdump();
			packetdump_timer_status = qdf_mc_timer_start(
						&pMac->roam.packetdump_timer,
						(PKT_DUMP_TIMER_DURATION *
						QDF_MC_TIMER_TO_SEC_UNIT)/
						QDF_MC_TIMER_TO_MS_UNIT);
			if (!QDF_IS_STATUS_SUCCESS(packetdump_timer_status))
				sme_debug("cannot start packetdump timer status: %d",
					  packetdump_timer_status);
		}
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		if (eWNI_SME_JOIN_REQ == messageType) {
			/* Notify QoS module that join happening */
			pSession->join_bssid_count++;
			sme_qos_csr_event_ind(pMac, (uint8_t) sessionId,
						SME_QOS_CSR_JOIN_REQ, NULL);
		} else if (eWNI_SME_REASSOC_REQ == messageType)
			/* Notify QoS module that reassoc happening */
			sme_qos_csr_event_ind(pMac, (uint8_t) sessionId,
						SME_QOS_CSR_REASSOC_REQ,
						NULL);
#endif
	} while (0);

	/* Clean up the memory in case of any failure */
	if (!QDF_IS_STATUS_SUCCESS(status) && (NULL != csr_join_req))
		qdf_mem_free(csr_join_req);

	if (wpaRsnIE)
		qdf_mem_free(wpaRsnIE);

	return status;
}

/* */
QDF_STATUS csr_send_mb_disassoc_req_msg(tpAniSirGlobal pMac, uint32_t sessionId,
					tSirMacAddr bssId, uint16_t reasonCode)
{
	tSirSmeDisassocReq *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!CSR_IS_SESSION_VALID(pMac, sessionId))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(tSirSmeDisassocReq));
	if (NULL == pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_DISASSOC_REQ;
	pMsg->length = sizeof(tSirSmeDisassocReq);
	pMsg->sessionId = sessionId;
	pMsg->transactionId = 0;
	if ((pSession->pCurRoamProfile != NULL)
		&& (CSR_IS_INFRA_AP(pSession->pCurRoamProfile))) {
		qdf_mem_copy(&pMsg->bssid.bytes,
			     &pSession->selfMacAddr,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(&pMsg->peer_macaddr.bytes,
			     bssId,
			     QDF_MAC_ADDR_SIZE);
	} else {
		qdf_mem_copy(&pMsg->bssid.bytes,
			     bssId, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(&pMsg->peer_macaddr.bytes,
			     bssId, QDF_MAC_ADDR_SIZE);
	}
	pMsg->reasonCode = reasonCode;
	pMsg->process_ho_fail = (pSession->disconnect_reason ==
		eSIR_MAC_FW_TRIGGERED_ROAM_FAILURE) ? true : false;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

	/*
	 * The state will be DISASSOC_HANDOFF only when we are doing
	 * handoff. Here we should not send the disassoc over the air
	 * to the AP
	 */
	if ((CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(pMac, sessionId)
			&& csr_roam_is11r_assoc(pMac, sessionId)) ||
						pMsg->process_ho_fail) {
		/* Set DoNotSendOverTheAir flag to 1 only for handoff case */
		pMsg->doNotSendOverTheAir = CSR_DONT_SEND_DISASSOC_OVER_THE_AIR;
	}
	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS
csr_send_mb_get_associated_stas_req_msg(tpAniSirGlobal pMac, uint32_t sessionId,
					QDF_MODULE_ID modId,
					struct qdf_mac_addr bssid,
					void *pUsrContext,
					void *pfnSapEventCallback,
					uint8_t *pAssocStasBuf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirSmeGetAssocSTAsReq *pMsg;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (NULL == pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_GET_ASSOC_STAS_REQ;
	qdf_copy_macaddr(&pMsg->bssid, &bssid);
	pMsg->modId = modId;
	qdf_mem_copy(pMsg->pUsrContext, pUsrContext, sizeof(void *));
	qdf_mem_copy(pMsg->pSapEventCallback,
			pfnSapEventCallback, sizeof(void *));
	qdf_mem_copy(pMsg->pAssocStasArray, pAssocStasBuf, sizeof(void *));
	pMsg->length = sizeof(*pMsg);
	status = umac_send_mb_message_to_mac(pMsg);

	return status;
}

QDF_STATUS csr_send_chng_mcc_beacon_interval(tpAniSirGlobal pMac,
						uint32_t sessionId)
{
	tpSirChangeBIParams pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* NO need to update the Beacon Params if update beacon parameter flag
	 * is not set
	 */
	if (!pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval)
		return QDF_STATUS_SUCCESS;

	pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval =
		false;

	/* Create the message and send to lim */
	len = sizeof(tSirChangeBIParams);
	pMsg = qdf_mem_malloc(len);
	if (NULL == pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMsg->messageType = eWNI_SME_CHNG_MCC_BEACON_INTERVAL;
		pMsg->length = len;

		qdf_copy_macaddr(&pMsg->bssid, &pSession->selfMacAddr);
		sme_debug(
			"CSR Attempting to change BI for Bssid= "
			   MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pMsg->bssid.bytes));
		pMsg->sessionId = sessionId;
		sme_debug("session %d BeaconInterval %d",
			sessionId,
			pMac->roam.roamSession[sessionId].bssParams.
			beaconInterval);
		pMsg->beaconInterval =
			pMac->roam.roamSession[sessionId].bssParams.beaconInterval;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}

#ifdef QCA_HT_2040_COEX
QDF_STATUS csr_set_ht2040_mode(tpAniSirGlobal pMac, uint32_t sessionId,
			       ePhyChanBondState cbMode, bool obssEnabled)
{
	tpSirSetHT2040Mode pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Create the message and send to lim */
	len = sizeof(tSirSetHT2040Mode);
	pMsg = qdf_mem_malloc(len);
	if (NULL == pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_zero(pMsg, sizeof(tSirSetHT2040Mode));
		pMsg->messageType = eWNI_SME_SET_HT_2040_MODE;
		pMsg->length = len;

		qdf_copy_macaddr(&pMsg->bssid, &pSession->selfMacAddr);
		sme_debug(
			"CSR Attempting to set HT20/40 mode for Bssid= "
			   MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pMsg->bssid.bytes));
		pMsg->sessionId = sessionId;
		sme_debug("  session %d HT20/40 mode %d",
			sessionId, cbMode);
		pMsg->cbMode = cbMode;
		pMsg->obssEnabled = obssEnabled;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}
#endif

QDF_STATUS csr_send_mb_deauth_req_msg(tpAniSirGlobal pMac, uint32_t sessionId,
				      tSirMacAddr bssId, uint16_t reasonCode)
{
	tSirSmeDeauthReq *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!CSR_IS_SESSION_VALID(pMac, sessionId))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(tSirSmeDeauthReq));
	if (NULL == pMsg)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(pMsg, sizeof(tSirSmeDeauthReq));
	pMsg->messageType = eWNI_SME_DEAUTH_REQ;
	pMsg->length = sizeof(tSirSmeDeauthReq);
	pMsg->sessionId = sessionId;
	pMsg->transactionId = 0;

	if ((pSession->pCurRoamProfile != NULL)
	     && (CSR_IS_INFRA_AP(pSession->pCurRoamProfile))) {
		qdf_mem_copy(&pMsg->bssid,
			     &pSession->selfMacAddr,
			     QDF_MAC_ADDR_SIZE);
	} else {
		qdf_mem_copy(&pMsg->bssid,
			     bssId, QDF_MAC_ADDR_SIZE);
	}

	/* Set the peer MAC address before sending the message to LIM */
	qdf_mem_copy(&pMsg->peer_macaddr.bytes, bssId, QDF_MAC_ADDR_SIZE);
	pMsg->reasonCode = reasonCode;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_mb_disassoc_cnf_msg(tpAniSirGlobal pMac,
					tpSirSmeDisassocInd pDisassocInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirSmeDisassocCnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(tSirSmeDisassocCnf));
		if (NULL == pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DISASSOC_CNF;
		pMsg->statusCode = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(tSirSmeDisassocCnf);
		pMsg->sme_session_id = pDisassocInd->sessionId;
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDisassocInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		qdf_copy_macaddr(&pMsg->bssid, &pDisassocInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_mb_deauth_cnf_msg(tpAniSirGlobal pMac,
				      tpSirSmeDeauthInd pDeauthInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirSmeDeauthCnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(tSirSmeDeauthCnf));
		if (NULL == pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DEAUTH_CNF;
		pMsg->statusCode = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(tSirSmeDeauthCnf);
		pMsg->sme_session_id = pDeauthInd->sessionId;
		qdf_copy_macaddr(&pMsg->bssid, &pDeauthInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDeauthInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_assoc_cnf_msg(tpAniSirGlobal pMac,
				  tpSirSmeAssocInd pAssocInd,
				  QDF_STATUS Halstatus,
				  tSirMacStatusCodes mac_status_code)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirSmeAssocCnf *pMsg;
	struct scheduler_msg msg = { 0 };

	sme_debug("HalStatus: %d, mac_status_code %d",
		  Halstatus, mac_status_code);
	do {
		pMsg = qdf_mem_malloc(sizeof(tSirSmeAssocCnf));
		if (NULL == pMsg)
			return QDF_STATUS_E_NOMEM;
		pMsg->messageType = eWNI_SME_ASSOC_CNF;
		pMsg->length = sizeof(*pMsg);
		if (QDF_IS_STATUS_SUCCESS(Halstatus)) {
			pMsg->statusCode = eSIR_SME_SUCCESS;
		} else {
			pMsg->statusCode = eSIR_SME_ASSOC_REFUSED;
			pMsg->mac_status_code = mac_status_code;
		}
		/* bssId */
		qdf_mem_copy(pMsg->bssid.bytes, pAssocInd->bssId,
			     QDF_MAC_ADDR_SIZE);
		/* peerMacAddr */
		qdf_mem_copy(pMsg->peer_macaddr.bytes, pAssocInd->peerMacAddr,
			     QDF_MAC_ADDR_SIZE);
		/* aid */
		pMsg->aid = pAssocInd->aid;
		/* alternateBssId */
		qdf_mem_copy(pMsg->alternate_bssid.bytes, pAssocInd->bssId,
			     QDF_MAC_ADDR_SIZE);
		/* alternateChannelId */
		pMsg->alternateChannelId = 11;
		/* OWE IE */
		if (pAssocInd->owe_ie_len) {
			pMsg->owe_ie = qdf_mem_malloc(pAssocInd->owe_ie_len);
			if (!pMsg->owe_ie)
				return QDF_STATUS_E_NOMEM;
			qdf_mem_copy(pMsg->owe_ie, pAssocInd->owe_ie,
				     pAssocInd->owe_ie_len);
			pMsg->owe_ie_len = pAssocInd->owe_ie_len;
		}

		msg.type = pMsg->messageType;
		msg.bodyval = 0;
		msg.bodyptr = pMsg;
		/* pMsg is freed by umac_send_mb_message_to_mac in anycase*/
		status = scheduler_post_msg_by_priority(QDF_MODULE_ID_PE, &msg,
							true);
	} while (0);
	return status;
}

QDF_STATUS csr_send_assoc_ind_to_upper_layer_cnf_msg(tpAniSirGlobal pMac,
						     tpSirSmeAssocInd pAssocInd,
						     QDF_STATUS Halstatus,
						     uint8_t sessionId)
{
	struct scheduler_msg msgQ = {0};
	tSirSmeAssocIndToUpperLayerCnf *pMsg;
	uint8_t *pBuf;
	tSirResultCodes statusCode;
	uint16_t wTmp;

	do {
		pMsg = qdf_mem_malloc(sizeof(tSirSmeAssocIndToUpperLayerCnf));
		if (NULL == pMsg)
			return QDF_STATUS_E_NOMEM;

		pMsg->messageType = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
		pMsg->length = sizeof(tSirSmeAssocIndToUpperLayerCnf);

		pMsg->sessionId = sessionId;

		pBuf = (uint8_t *) &pMsg->statusCode;
		if (QDF_IS_STATUS_SUCCESS(Halstatus))
			statusCode = eSIR_SME_SUCCESS;
		else
			statusCode = eSIR_SME_ASSOC_REFUSED;
		qdf_mem_copy(pBuf, &statusCode, sizeof(tSirResultCodes));
		/* bssId */
		pBuf = (uint8_t *)&pMsg->bssId;
		qdf_mem_copy((tSirMacAddr *)pBuf, pAssocInd->bssId,
			sizeof(tSirMacAddr));
		/* peerMacAddr */
		pBuf = (uint8_t *)&pMsg->peerMacAddr;
		qdf_mem_copy((tSirMacAddr *)pBuf, pAssocInd->peerMacAddr,
			sizeof(tSirMacAddr));
		/* StaId */
		pBuf = (uint8_t *)&pMsg->aid;
		wTmp = pAssocInd->staId;
		qdf_mem_copy(pBuf, &wTmp, sizeof(uint16_t));
		/* alternateBssId */
		pBuf = (uint8_t *)&pMsg->alternateBssId;
		qdf_mem_copy((tSirMacAddr *)pBuf, pAssocInd->bssId,
			sizeof(tSirMacAddr));
		/* alternateChannelId */
		pBuf = (uint8_t *)&pMsg->alternateChannelId;
		*pBuf = 11;
		/*
		 * Instead of copying roam Info,just copy WmmEnabled,
		 * RsnIE information.
		 * Wmm
		 */
		pBuf = (uint8_t *)&pMsg->wmmEnabledSta;
		*pBuf = pAssocInd->wmmEnabledSta;
		/* RSN IE */
		pBuf = (uint8_t *)&pMsg->rsnIE;
		qdf_mem_copy((tSirRSNie *)pBuf, &pAssocInd->rsnIE,
			sizeof(tSirRSNie));
#ifdef FEATURE_WLAN_WAPI
		/* WAPI IE */
		pBuf = (uint8_t *)&pMsg->wapiIE;
		qdf_mem_copy((tSirWAPIie *)pBuf, &pAssocInd->wapiIE,
			sizeof(tSirWAPIie));
#endif
		/* Additional IE */
		pBuf = (uint8_t *)&pMsg->addIE;
		qdf_mem_copy((tSirAddie *)pBuf, &pAssocInd->addIE,
			sizeof(tSirAddie));
		/* reassocReq */
		pBuf = (uint8_t *)&pMsg->reassocReq;
		*pBuf = pAssocInd->reassocReq;
		/* timingMeasCap */
		pBuf = (uint8_t *)&pMsg->timingMeasCap;
		*pBuf = pAssocInd->timingMeasCap;
		/* chan_info */
		pBuf = (uint8_t *)&pMsg->chan_info;
		qdf_mem_copy((void *)pBuf, &pAssocInd->chan_info,
			sizeof(tSirSmeChanInfo));
		/* ampdu */
		pBuf = (uint8_t *)&pMsg->ampdu;
		*((bool *)pBuf) = pAssocInd->ampdu;
		/* sgi_enable */
		pBuf = (uint8_t *)&pMsg->sgi_enable;
		*((bool *)pBuf) = pAssocInd->sgi_enable;
		/* tx stbc */
		pBuf = (uint8_t *)&pMsg->tx_stbc;
		*((bool *)pBuf) = pAssocInd->tx_stbc;
		/* ch_width */
		pBuf = (uint8_t *)&pMsg->ch_width;
		*((tSirMacHTChannelWidth *)pBuf) = pAssocInd->ch_width;
		/* mode */
		pBuf = (uint8_t *)&pMsg->mode;
		*((enum sir_sme_phy_mode *)pBuf) = pAssocInd->mode;
		/* rx stbc */
		pBuf = (uint8_t *)&pMsg->rx_stbc;
		*((bool *)pBuf) = pAssocInd->rx_stbc;
		/* max supported idx */
		pBuf = (uint8_t *)&pMsg->max_supp_idx;
		*pBuf = pAssocInd->max_supp_idx;
		/* max extended idx */
		pBuf = (uint8_t *)&pMsg->max_ext_idx;
		*pBuf = pAssocInd->max_ext_idx;
		/* max ht mcs idx */
		pBuf = (uint8_t *)&pMsg->max_mcs_idx;
		*pBuf = pAssocInd->max_mcs_idx;
		/* vht rx mcs map */
		pBuf = (uint8_t *)&pMsg->rx_mcs_map;
		*pBuf = pAssocInd->rx_mcs_map;
		/* vht tx mcs map */
		pBuf = (uint8_t *)&pMsg->tx_mcs_map;
		*pBuf = pAssocInd->tx_mcs_map;

		pBuf = (uint8_t *)&pMsg->ecsa_capable;
		*pBuf = pAssocInd->ecsa_capable;

		if (pAssocInd->HTCaps.present)
			pMsg->ht_caps = pAssocInd->HTCaps;
		if (pAssocInd->VHTCaps.present)
			pMsg->vht_caps = pAssocInd->VHTCaps;
		pMsg->capability_info = pAssocInd->capability_info;
		pMsg->he_caps_present = pAssocInd->he_caps_present;
		if (pAssocInd->assocReqPtr) {
			if (pAssocInd->assocReqLength < MAX_ASSOC_REQ_IE_LEN) {
				pMsg->ies = qdf_mem_malloc(
						pAssocInd->assocReqLength);
				if (pMsg->ies == NULL) {
					qdf_mem_free(pMsg);
					return QDF_STATUS_E_NOMEM;
				}
				pMsg->ies_len = pAssocInd->assocReqLength;
				qdf_mem_copy(pMsg->ies , pAssocInd->assocReqPtr,
					     pMsg->ies_len);
			} else {
				sme_err("Assoc Ie length is too long");
			}
		}
		msgQ.type = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
		msgQ.bodyptr = pMsg;
		msgQ.bodyval = 0;
		sys_process_mmh_msg(pMac, &msgQ);
	} while (0);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_send_mb_set_context_req_msg(tpAniSirGlobal pMac,
					   uint32_t sessionId,
					   struct qdf_mac_addr peer_macaddr,
					   uint8_t numKeys,
					   tAniEdType edType, bool fUnicast,
					   tAniKeyDirection aniKeyDirection,
					   uint8_t keyId, uint8_t keyLength,
					   uint8_t *pKey, uint8_t paeRole,
					   uint8_t *pKeyRsc)
{
	tSirSmeSetContextReq *pMsg;
	struct scheduler_msg msg = {0};
	uint16_t msgLen;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	sme_debug("keylength: %d Encry type: %d", keyLength, edType);
	do {
		if ((1 != numKeys) && (0 != numKeys))
			break;
		/*
		 * All of these fields appear in every SET_CONTEXT message.
		 * Below we'll add in the size for each key set. Since we only
		 * support up to one key, we always allocate memory for 1 key.
		 */
		msgLen = sizeof(struct sSirSmeSetContextReq);

		pMsg = qdf_mem_malloc(msgLen);
		if (NULL == pMsg)
			return QDF_STATUS_E_NOMEM;
		pMsg->messageType = eWNI_SME_SETCONTEXT_REQ;
		pMsg->length = msgLen;
		pMsg->sessionId = (uint8_t) sessionId;
		pMsg->transactionId = 0;
		qdf_copy_macaddr(&pMsg->peer_macaddr, &peer_macaddr);
		qdf_copy_macaddr(&pMsg->bssid,
				 &pSession->connectedProfile.bssid);

		/**
		 * Set the pMsg->keyMaterial.length field
		 * (this length is defined as all data that follows the
		 * edType field in the tSirKeyMaterial keyMaterial; field).
		 *
		 * NOTE:  This keyMaterial.length contains the length of a
		 * MAX size key, though the keyLength can be shorter than this
		 * max size.  Is LIM interpreting this ok ?
		 */
		pMsg->keyMaterial.length =
				sizeof(pMsg->keyMaterial.numKeys) +
				(numKeys * sizeof(pMsg->keyMaterial.key));
		pMsg->keyMaterial.edType = edType;
		pMsg->keyMaterial.numKeys = numKeys;
		pMsg->keyMaterial.key[0].keyId = keyId;
		pMsg->keyMaterial.key[0].unicast = fUnicast;
		pMsg->keyMaterial.key[0].keyDirection = aniKeyDirection;
		qdf_mem_copy(pMsg->keyMaterial.key[0].keyRsc,
				pKeyRsc, CSR_MAX_RSC_LEN);
		/* 0 is Supplicant */
		pMsg->keyMaterial.key[0].paeRole = paeRole;
		pMsg->keyMaterial.key[0].keyLength = keyLength;
		if (keyLength && pKey)
			qdf_mem_copy(pMsg->keyMaterial.key[0].key,
					pKey, keyLength);

		msg.type = eWNI_SME_SETCONTEXT_REQ;
		msg.bodyptr = pMsg;
		status = scheduler_post_message(QDF_MODULE_ID_SME,
						QDF_MODULE_ID_PE,
						QDF_MODULE_ID_PE, &msg);
		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mem_zero(pMsg, msgLen);
			qdf_mem_free(pMsg);
		}
	} while (0);
	return status;
}

QDF_STATUS csr_send_mb_start_bss_req_msg(tpAniSirGlobal pMac, uint32_t
					sessionId, eCsrRoamBssType bssType,
					 struct csr_roamstart_bssparams *pParam,
					 tSirBssDescription *pBssDesc)
{
	tSirSmeStartBssReq *pMsg;
	uint16_t wTmp;
	uint32_t value = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pSession->joinFailStatusCode.statusCode = eSIR_SME_SUCCESS;
	pSession->joinFailStatusCode.reasonCode = 0;
	pMsg = qdf_mem_malloc(sizeof(tSirSmeStartBssReq));
	if (NULL == pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_START_BSS_REQ;
	pMsg->sessionId = sessionId;
	pMsg->length = sizeof(tSirSmeStartBssReq);
	pMsg->transactionId = 0;
	qdf_copy_macaddr(&pMsg->bssid, &pParam->bssid);
	/* selfMacAddr */
	qdf_copy_macaddr(&pMsg->self_macaddr, &pSession->selfMacAddr);
	/* beaconInterval */
	if (pBssDesc && pBssDesc->beaconInterval)
		wTmp = pBssDesc->beaconInterval;
	else if (pParam->beaconInterval)
		wTmp = pParam->beaconInterval;
	else
		wTmp = WNI_CFG_BEACON_INTERVAL_STADEF;

	csr_validate_mcc_beacon_interval(pMac, pParam->operationChn,
					 &wTmp, sessionId, pParam->bssPersona);
	/* Update the beacon Interval */
	pParam->beaconInterval = wTmp;
	pMsg->beaconInterval = wTmp;
	pMsg->dot11mode =
		csr_translate_to_wni_cfg_dot11_mode(pMac,
						    pParam->uCfgDot11Mode);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	pMsg->cc_switch_mode = pMac->roam.configParam.cc_switch_mode;
#endif
	pMsg->bssType = csr_translate_bsstype_to_mac_type(bssType);
	qdf_mem_copy(&pMsg->ssId, &pParam->ssId, sizeof(pParam->ssId));
	pMsg->channelId = pParam->operationChn;
	/* What should we really do for the cbmode. */
	pMsg->cbMode = (ePhyChanBondState) pParam->cbMode;
	pMsg->vht_channel_width = pParam->ch_params.ch_width;
	pMsg->center_freq_seg0 = pParam->ch_params.center_freq_seg0;
	pMsg->center_freq_seg1 = pParam->ch_params.center_freq_seg1;
	pMsg->sec_ch_offset = pParam->ch_params.sec_ch_offset;
	pMsg->privacy = pParam->privacy;
	pMsg->apUapsdEnable = pParam->ApUapsdEnable;
	pMsg->ssidHidden = pParam->ssidHidden;
	pMsg->fwdWPSPBCProbeReq = (uint8_t) pParam->fwdWPSPBCProbeReq;
	pMsg->protEnabled = (uint8_t) pParam->protEnabled;
	pMsg->obssProtEnabled = (uint8_t) pParam->obssProtEnabled;
	/* set cfg related to protection */
	pMsg->ht_capab = pParam->ht_protection;
	pMsg->authType = pParam->authType;
	pMsg->dtimPeriod = pParam->dtimPeriod;
	pMsg->wps_state = pParam->wps_state;
	pMsg->isCoalesingInIBSSAllowed = pMac->isCoalesingInIBSSAllowed;
	pMsg->bssPersona = pParam->bssPersona;
	pMsg->txLdpcIniFeatureEnabled = pMac->roam.configParam.tx_ldpc_enable;

	/*
	 * If RX LDPC has been disabled for 2.4GHz channels and enabled
	 * for 5Ghz for STA like persona then here is how to handle
	 * those cases (by now channel has been decided).
	 */
	if (eSIR_IBSS_MODE == pMsg->bssType ||
		!policy_mgr_is_dbs_enable(pMac->psoc))
		csr_set_ldpc_exception(pMac, pSession,
				pMsg->channelId,
				pMac->roam.configParam.rx_ldpc_enable);

	qdf_mem_copy(&pMsg->vht_config,
		     &pSession->vht_config,
		     sizeof(pSession->vht_config));
	qdf_mem_copy(&pMsg->htConfig,
		     &pSession->htConfig,
		     sizeof(tSirHTConfig));

	if (wlan_cfg_get_int(pMac, WNI_CFG_VHT_SU_BEAMFORMEE_CAP, &value)
					!= QDF_STATUS_SUCCESS)
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			 "could not get SU beam formee capability");
	pMsg->vht_config.su_beam_formee =
		(uint8_t)value &&
		(uint8_t)pMac->roam.configParam.enable_txbf_sap_mode;

	value = WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF;
	pMsg->vht_config.csnof_beamformer_antSup = (uint8_t)value;
	pMsg->vht_config.mu_beam_formee = 0;

	sme_debug("ht capability 0x%x VHT capability 0x%x",
			 (uint32_t)(*(uint32_t *) &pMsg->htConfig),
			 (uint32_t)(*(uint32_t *) &pMsg->vht_config));
#ifdef WLAN_FEATURE_11W
	pMsg->pmfCapable = pParam->mfpCapable;
	pMsg->pmfRequired = pParam->mfpRequired;
#endif

	if (pParam->nRSNIELength > sizeof(pMsg->rsnIE.rsnIEdata)) {
		qdf_mem_free(pMsg);
		return QDF_STATUS_E_INVAL;
	}
	pMsg->rsnIE.length = pParam->nRSNIELength;
	qdf_mem_copy(pMsg->rsnIE.rsnIEdata,
		     pParam->pRSNIE,
		     pParam->nRSNIELength);
	pMsg->nwType = (tSirNwType)pParam->sirNwType;
	qdf_mem_copy(&pMsg->operationalRateSet,
		     &pParam->operationalRateSet,
		     sizeof(tSirMacRateSet));
	qdf_mem_copy(&pMsg->extendedRateSet,
		     &pParam->extendedRateSet,
		     sizeof(tSirMacRateSet));

	if (IS_DOT11_MODE_HE(pMsg->dot11mode))
		csr_start_bss_copy_he_cap(pMsg, pSession);

	qdf_mem_copy(&pMsg->addIeParams,
		     &pParam->addIeParams,
		     sizeof(pParam->addIeParams));
	pMsg->obssEnabled = pMac->roam.configParam.obssEnabled;
	pMsg->sap_dot11mc = pParam->sap_dot11mc;
	pMsg->vendor_vht_sap =
			pMac->roam.configParam.vendor_vht_sap;
	pMsg->cac_duration_ms = pParam->cac_duration_ms;
	pMsg->dfs_regdomain = pParam->dfs_regdomain;
	pMsg->beacon_tx_rate = pParam->beacon_tx_rate;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_mb_stop_bss_req_msg(tpAniSirGlobal pMac, uint32_t sessionId)
{
	tSirSmeStopBssReq *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pMsg = qdf_mem_malloc(sizeof(tSirSmeStopBssReq));
	if (NULL == pMsg)
		return QDF_STATUS_E_NOMEM;
	pMsg->messageType = eWNI_SME_STOP_BSS_REQ;
	pMsg->sessionId = sessionId;
	pMsg->length = sizeof(tSirSmeStopBssReq);
	pMsg->transactionId = 0;
	pMsg->reasonCode = 0;
	qdf_copy_macaddr(&pMsg->bssid, &pSession->connectedProfile.bssid);
	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_reassoc(tpAniSirGlobal pMac, uint32_t sessionId,
		       tCsrRoamModifyProfileFields *pModProfileFields,
		       uint32_t *pRoamId, bool fForce)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t roamId = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if ((csr_is_conn_state_connected(pMac, sessionId)) &&
	    (fForce || (qdf_mem_cmp(&pModProfileFields,
				     &pSession->connectedProfile.
				     modifyProfileFields,
				     sizeof(tCsrRoamModifyProfileFields))))) {
		roamId = GET_NEXT_ROAM_ID(&pMac->roam);
		if (pRoamId)
			*pRoamId = roamId;

		status =
			csr_roam_issue_reassoc(pMac, sessionId, NULL,
					       pModProfileFields,
					       eCsrSmeIssuedReassocToSameAP,
					       roamId, false);
	}
	return status;
}

static QDF_STATUS csr_roam_session_opened(tpAniSirGlobal pMac,
					  QDF_STATUS qdf_status,
					  uint32_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		status = csr_roam_call_callback(pMac, sessionId, roam_info, 0,
						eCSR_ROAM_SESSION_OPENED,
						eCSR_ROAM_RESULT_FAILURE);
	} else {
		status = csr_roam_call_callback(pMac, sessionId, roam_info, 0,
						eCSR_ROAM_SESSION_OPENED,
						eCSR_ROAM_RESULT_SUCCESS);
	}

	qdf_mem_free(roam_info);
	return status;
}

/**
 * csr_store_oce_cfg_flags_in_vdev() - fill OCE flags from ini
 * @mac: mac_context.
 * @vdev: Pointer to pdev obj
 * @vdev_id: vdev_id
 *
 * This API will store the oce flags in vdev mlme priv object
 *
 * Return: none
 */
static void csr_store_oce_cfg_flags_in_vdev(tpAniSirGlobal pMac,
					    struct wlan_objmgr_pdev *pdev,
					    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev =
	wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_LEGACY_MAC_ID);
	struct vdev_mlme_priv_obj *vdev_mlme;

	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	vdev_mlme = wlan_vdev_mlme_get_priv_obj(vdev);
	if (!vdev_mlme) {
		sme_err("vdev_mlme is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
		return;
	}

	vdev_mlme->sta_dynamic_oce_value =
	pMac->roam.configParam.oce_feature_bitmap;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

QDF_STATUS csr_process_add_sta_session_rsp(tpAniSirGlobal pMac, uint8_t *pMsg)
{
	struct add_sta_self_params *rsp;
	struct send_extcap_ie *msg;
	QDF_STATUS status;

	if (pMsg == NULL) {
		sme_err("in %s msg ptr is NULL", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	rsp = (struct add_sta_self_params *) pMsg;
	sme_debug("Add Sta self rsp status = %d", rsp->status);

	if (QDF_IS_STATUS_SUCCESS(rsp->status)) {
		if ((WMI_VDEV_TYPE_STA == rsp->type ||
		    (WMI_VDEV_TYPE_AP == rsp->type &&
		     WMI_UNIFIED_VDEV_SUBTYPE_P2P_DEVICE == rsp->sub_type))) {
			sme_debug("send SET IE msg to PE");
			msg = qdf_mem_malloc(sizeof(*msg));
			if (NULL == msg) {
				sme_err("Memory allocation failed");
				return QDF_STATUS_E_NOMEM;
			}

			msg->msg_type = eWNI_SME_SET_IE_REQ;
			msg->session_id = rsp->session_id;
			msg->length = sizeof(*msg);
			status = umac_send_mb_message_to_mac(msg);
			if (!QDF_IS_STATUS_SUCCESS(status))
				sme_err("Failed to send down the set IE req ");
		}
	}

	csr_roam_session_opened(pMac, rsp->status, rsp->session_id);

	if (QDF_IS_STATUS_SUCCESS(rsp->status) &&
	    rsp->type == WMI_VDEV_TYPE_STA) {
		csr_store_oce_cfg_flags_in_vdev(pMac, pMac->pdev,
						rsp->session_id);

		wlan_mlme_update_oce_flags(pMac->pdev,
					   pMac->roam.configParam.oce_feature_bitmap);
	}
	if (QDF_IS_STATUS_ERROR(rsp->status))
		csr_cleanup_session(pMac, rsp->session_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_get_vdev_type_nss() - gets the nss value based on vdev type
 * @mac_ctx: Pointer to Global MAC structure
 * @dev_mode: current device operating mode.
 * @nss2g: Pointer to the 2G Nss parameter.
 * @nss5g: Pointer to the 5G Nss parameter.
 *
 * Fills the 2G and 5G Nss values based on device mode.
 *
 * Return: None
 */
void csr_get_vdev_type_nss(tpAniSirGlobal mac_ctx,
		enum QDF_OPMODE dev_mode,
		uint8_t *nss_2g, uint8_t *nss_5g)
{
	switch (dev_mode) {
	case QDF_STA_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sta;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sta;
		break;
	case QDF_SAP_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sap;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sap;
		break;
	case QDF_P2P_CLIENT_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_cli;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_cli;
		break;
	case QDF_P2P_GO_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_go;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_go;
		break;
	case QDF_P2P_DEVICE_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_dev;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_dev;
		break;
	case QDF_IBSS_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ibss;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ibss;
		break;
	case QDF_OCB_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ocb;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ocb;
		break;
	default:
		*nss_2g = 1;
		*nss_5g = 1;
		sme_err("Unknown device mode");
		break;
	}
	sme_debug("mode - %d: nss_2g - %d, 5g - %d",
			dev_mode, *nss_2g, *nss_5g);
}

static
QDF_STATUS csr_issue_add_sta_for_session_req(tpAniSirGlobal pMac,
					     uint32_t sessionId,
					     tSirMacAddr sessionMacAddr,
					     uint32_t type, uint32_t subType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct add_sta_self_params *add_sta_self_req;
	uint8_t nss_2g;
	uint8_t nss_5g;
	struct scheduler_msg msg = {0};

	add_sta_self_req = qdf_mem_malloc(sizeof(struct add_sta_self_params));
	if (NULL == add_sta_self_req) {
		sme_err("Unable to allocate memory for tAddSelfStaParams");
		return QDF_STATUS_E_NOMEM;
	}

	csr_get_vdev_type_nss(pMac, pMac->sme.currDeviceMode,
			&nss_2g, &nss_5g);
	qdf_mem_copy(add_sta_self_req->self_mac_addr, sessionMacAddr,
			sizeof(tSirMacAddr));
	add_sta_self_req->curr_device_mode = pMac->sme.currDeviceMode;
	add_sta_self_req->session_id = sessionId;
	add_sta_self_req->type = type;
	add_sta_self_req->sub_type = subType;
	add_sta_self_req->nss_2g = nss_2g;
	add_sta_self_req->nss_5g = nss_5g;
	add_sta_self_req->tx_aggregation_size =
		pMac->roam.configParam.tx_aggregation_size;
	add_sta_self_req->tx_aggregation_size_be =
		pMac->roam.configParam.tx_aggregation_size_be;
	add_sta_self_req->tx_aggregation_size_bk =
		pMac->roam.configParam.tx_aggregation_size_bk;
	add_sta_self_req->tx_aggregation_size_vi =
		pMac->roam.configParam.tx_aggregation_size_vi;
	add_sta_self_req->tx_aggregation_size_vo =
		pMac->roam.configParam.tx_aggregation_size_vo;
	add_sta_self_req->rx_aggregation_size =
		pMac->roam.configParam.rx_aggregation_size;
	add_sta_self_req->enable_bcast_probe_rsp =
		pMac->roam.configParam.enable_bcast_probe_rsp;
	add_sta_self_req->fils_max_chan_guard_time =
		pMac->roam.configParam.fils_max_chan_guard_time;
	add_sta_self_req->pkt_err_disconn_th =
		pMac->roam.configParam.pkt_err_disconn_th;
	add_sta_self_req->oce_feature_bitmap =
		pMac->roam.configParam.oce_feature_bitmap;
	add_sta_self_req->tx_aggr_sw_retry_threshold_be =
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_be;
	add_sta_self_req->tx_aggr_sw_retry_threshold_bk =
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_bk;
	add_sta_self_req->tx_aggr_sw_retry_threshold_vi =
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_vi;
	add_sta_self_req->tx_aggr_sw_retry_threshold_vo =
		pMac->roam.configParam.tx_aggr_sw_retry_threshold_vo;
	add_sta_self_req->tx_aggr_sw_retry_threshold =
		pMac->roam.configParam.tx_aggr_sw_retry_threshold;
	add_sta_self_req->tx_non_aggr_sw_retry_threshold_be =
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_be;
	add_sta_self_req->tx_non_aggr_sw_retry_threshold_bk =
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_bk;
	add_sta_self_req->tx_non_aggr_sw_retry_threshold_vi =
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_vi;
	add_sta_self_req->tx_non_aggr_sw_retry_threshold_vo =
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold_vo;
	add_sta_self_req->tx_non_aggr_sw_retry_threshold =
		pMac->roam.configParam.tx_non_aggr_sw_retry_threshold;

	msg.type = WMA_ADD_STA_SELF_REQ;
	msg.reserved = 0;
	msg.bodyptr = add_sta_self_req;
	msg.bodyval = 0;

	sme_debug(
		"Send WMA_ADD_STA_SELF_REQ for selfMac=" MAC_ADDRESS_STR,
		 MAC_ADDR_ARRAY(add_sta_self_req->self_mac_addr));
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);

	if (status != QDF_STATUS_SUCCESS) {
		sme_err("wma_post_ctrl_msg failed");
		qdf_mem_free(add_sta_self_req);
		add_sta_self_req = NULL;
	}
	return status;
}

QDF_STATUS csr_roam_open_session(tpAniSirGlobal mac_ctx,
				 struct sme_session_params *session_param)
{
	QDF_STATUS status;
	uint32_t existing_session_id;
	union {
		uint16_t nCfgValue16;
		tSirMacHTCapabilityInfo htCapInfo;
	} uHTCapabilityInfo;
	uint32_t nCfgValue;
	struct csr_roam_session *session;

	/* check to see if the mac address already belongs to a session */
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			(struct qdf_mac_addr *)session_param->self_mac_addr,
			&existing_session_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Session %d exists with mac address " MAC_ADDRESS_STR,
			existing_session_id,
			MAC_ADDR_ARRAY(session_param->self_mac_addr));
		return QDF_STATUS_E_FAILURE;
	}

	/* attempt to retrieve session for Id */
	session = CSR_GET_SESSION(mac_ctx, session_param->sme_session_id);
	if (!session) {
		sme_err("Session does not exist for interface %d",
			session_param->sme_session_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* check to see if the session is already active */
	if (session->sessionActive) {
		sme_err("Cannot re-open active session with Id %d",
			session_param->sme_session_id);
		return QDF_STATUS_E_FAILURE;
	}

	session->sessionActive = true;
	session->sessionId = session_param->sme_session_id;

	/* Initialize FT related data structures only in STA mode */
	sme_ft_open(MAC_HANDLE(mac_ctx), session->sessionId);

	session->session_open_cb = session_param->session_open_cb;
	session->session_close_cb = session_param->session_close_cb;
	session->callback = session_param->callback;
	session->pContext = session_param->callback_ctx;

	qdf_mem_copy(&session->selfMacAddr, session_param->self_mac_addr,
		     sizeof(struct qdf_mac_addr));
	status = qdf_mc_timer_init(&session->hTimerRoaming,
				   QDF_TIMER_TYPE_SW,
				   csr_roam_roaming_timer_handler,
				   &session->roamingTimerInfo);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("cannot allocate memory for Roaming timer");
		return status;
	}

	status = qdf_mc_timer_init(&session->roaming_offload_timer,
				   QDF_TIMER_TYPE_SW,
				   csr_roam_roaming_offload_timeout_handler,
				   &session->roamingTimerInfo);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("mem fail for roaming timer");
		return status;
	}

	/* get the HT capability info */
	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_HT_CAP_INFO, &nCfgValue) !=
	    QDF_STATUS_SUCCESS) {
		sme_err("could not get HT capability info");
		return QDF_STATUS_SUCCESS;
	}

	uHTCapabilityInfo.nCfgValue16 = 0xFFFF & nCfgValue;
	session->htConfig.ht_rx_ldpc = uHTCapabilityInfo.htCapInfo.advCodingCap;
	session->htConfig.ht_tx_stbc = uHTCapabilityInfo.htCapInfo.txSTBC;
	session->htConfig.ht_rx_stbc = uHTCapabilityInfo.htCapInfo.rxSTBC;
	session->htConfig.ht_sgi20 = uHTCapabilityInfo.htCapInfo.shortGI20MHz;
	session->htConfig.ht_sgi40 = uHTCapabilityInfo.htCapInfo.shortGI40MHz;

#ifdef FEATURE_WLAN_BTAMP_UT_RF
	status = qdf_mc_timer_init(&session->hTimerJoinRetry, QDF_TIMER_TYPE_SW,
				   csr_roam_join_retry_timer_handler,
				   &session->joinRetryTimerInfo);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("cannot allocate memory for join retry timer");
		return status;
	}
#endif /* FEATURE_WLAN_BTAMP_UT_RF */

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_MAX_MPDU_LENGTH, &nCfgValue);
	session->vht_config.max_mpdu_len = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_SUPPORTED_CHAN_WIDTH_SET,
			 &nCfgValue);
	session->vht_config.supported_channel_widthset = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_LDPC_CODING_CAP, &nCfgValue);
	session->vht_config.ldpc_coding = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_SHORT_GI_80MHZ, &nCfgValue);
	session->vht_config.shortgi80 = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_SHORT_GI_160_AND_80_PLUS_80MHZ,
			 &nCfgValue);
	session->vht_config.shortgi160and80plus80 = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_TXSTBC, &nCfgValue);
	session->vht_config.tx_stbc = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_RXSTBC, &nCfgValue);
	session->vht_config.rx_stbc = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_SU_BEAMFORMER_CAP, &nCfgValue);
	session->vht_config.su_beam_former = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_SU_BEAMFORMEE_CAP, &nCfgValue);
	session->vht_config.su_beam_formee = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED,
			 &nCfgValue);
	session->vht_config.csnof_beamformer_antSup = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_NUM_SOUNDING_DIMENSIONS,
			 &nCfgValue);
	session->vht_config.num_soundingdim = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_MU_BEAMFORMER_CAP, &nCfgValue);
	session->vht_config.mu_beam_former = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_MU_BEAMFORMEE_CAP, &nCfgValue);
	session->vht_config.mu_beam_formee = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_TXOP_PS, &nCfgValue);
	session->vht_config.vht_txops = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_HTC_VHTC_CAP, &nCfgValue);
	session->vht_config.htc_vhtcap = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_RX_ANT_PATTERN, &nCfgValue);
	session->vht_config.rx_antpattern = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_TX_ANT_PATTERN, &nCfgValue);
	session->vht_config.tx_antpattern = nCfgValue;

	nCfgValue = 0;
	wlan_cfg_get_int(mac_ctx, WNI_CFG_VHT_AMPDU_LEN_EXPONENT, &nCfgValue);
	session->vht_config.max_ampdu_lenexp = nCfgValue;

	csr_update_session_he_cap(mac_ctx, session);

	return csr_issue_add_sta_for_session_req(mac_ctx,
				session_param->sme_session_id,
				session_param->self_mac_addr,
				session_param->type_of_persona,
				session_param->subtype_of_persona);
}

QDF_STATUS csr_process_del_sta_session_rsp(tpAniSirGlobal mac_ctx,
					   uint8_t *pMsg)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct del_sta_self_params *rsp;
	uint8_t sessionId;
	tListElem *entry;
	tSmeCmd *sme_command;

	if (pMsg == NULL) {
		sme_err("msg ptr is NULL");
		return status;
	}

	entry = csr_nonscan_active_ll_peek_head(mac_ctx, LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("NO commands are ACTIVE");
		return status;
	}

	sme_command = GET_BASE_ADDR(entry, tSmeCmd, Link);
	if (e_sme_command_del_sta_session != sme_command->command) {
		sme_err("No Del sta session command ACTIVE");
		return status;
	}

	rsp = (struct del_sta_self_params *) pMsg;
	sessionId = rsp->session_id;
	sme_debug("Del Sta rsp status = %d", rsp->status);

	/*
	 * This session is done. This will also flush all the pending command
	 * for this vdev, as vdev is deleted and no command should be sent
	 * for this vdev. Active cmnd is e_sme_command_del_sta_session and will
	 * be removed anyway next.
	 */
	csr_cleanup_session(mac_ctx, sessionId);

	/* Remove this command out of the non scan active list */
	if (csr_nonscan_active_ll_remove_entry(mac_ctx, entry,
					       LL_ACCESS_LOCK)) {
		csr_release_command(mac_ctx, sme_command);
	}

	if (rsp->sme_callback) {
		status = sme_release_global_lock(&mac_ctx->sme);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_debug("Failed to Release Lock");
		else {
			rsp->sme_callback(rsp->session_id);
			status = sme_acquire_global_lock(&mac_ctx->sme);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_debug("Failed to get Lock");
				return status;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}


static QDF_STATUS
csr_issue_del_sta_for_session_req(tpAniSirGlobal mac_ctx, uint32_t session_id,
				  tSirMacAddr session_mac_addr,
				  csr_session_close_cb callback,
				  void *context)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *sme_command;

	sme_command = csr_get_command_buffer(mac_ctx);
	if (NULL == sme_command) {
		status = QDF_STATUS_E_RESOURCES;
	} else {
		sme_command->command = e_sme_command_del_sta_session;
		sme_command->sessionId = (uint8_t)session_id;
		sme_command->u.delStaSessionCmd.session_close_cb = callback;
		sme_command->u.delStaSessionCmd.context = context;
		qdf_mem_copy(sme_command->u.delStaSessionCmd.selfMacAddr,
			     session_mac_addr, sizeof(tSirMacAddr));
		status = csr_queue_sme_command(mac_ctx, sme_command, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status = %d", status);
	}
	return status;
}

void csr_cleanup_session(tpAniSirGlobal pMac, uint32_t sessionId)
{
	if (CSR_IS_SESSION_VALID(pMac, sessionId)) {
		struct csr_roam_session *pSession = CSR_GET_SESSION(pMac,
								sessionId);

		csr_roam_stop(pMac, sessionId);

		/* Clean up FT related data structures */
		sme_ft_close(MAC_HANDLE(pMac), sessionId);
		csr_free_connect_bss_desc(pMac, sessionId);

		sme_reset_key(MAC_HANDLE(pMac), sessionId);
		csr_reset_cfg_privacy(pMac);
		csr_flush_roam_scan_chan_lists(pMac, sessionId);
		csr_roam_free_connect_profile(&pSession->connectedProfile);
		csr_roam_free_connected_info(pMac, &pSession->connectedInfo);
		csr_roam_free_connected_info(pMac,
					     &pSession->prev_assoc_ap_info);
		qdf_mc_timer_destroy(&pSession->hTimerRoaming);
		qdf_mc_timer_destroy(&pSession->roaming_offload_timer);
#ifdef FEATURE_WLAN_BTAMP_UT_RF
		qdf_mc_timer_destroy(&pSession->hTimerJoinRetry);
#endif
		csr_purge_vdev_pending_ser_cmd_list(pMac, sessionId);
		csr_init_session(pMac, sessionId);
	}
}

QDF_STATUS csr_roam_close_session(tpAniSirGlobal mac_ctx,
				  uint32_t session_id, bool sync)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id))
		return QDF_STATUS_E_INVAL;

	session = CSR_GET_SESSION(mac_ctx, session_id);
	/* Vdev going down stop roaming */
	session->fCancelRoaming = true;
	if (sync) {
		csr_cleanup_session(mac_ctx, session_id);
		return status;
	}

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id)) {
		sme_debug("Stop Wait for key timer and change substate to eCSR_ROAM_SUBSTATE_NONE");
		csr_roam_stop_wait_for_key_timer(mac_ctx);
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 session_id);
	}

	if (!session->session_close_cb) {
		sme_err("no close session callback registered");
		return QDF_STATUS_E_FAILURE;
	}
	status = csr_issue_del_sta_for_session_req(mac_ctx,
			session_id, session->selfMacAddr.bytes,
			session->session_close_cb, NULL);
	return status;
}

static void csr_init_session(tpAniSirGlobal pMac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession)
		return;

	pSession->sessionActive = false;
	pSession->sessionId = CSR_SESSION_ID_INVALID;
	pSession->callback = NULL;
	pSession->pContext = NULL;
	pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	csr_saved_scan_cmd_free_fields(pMac, pSession);
	csr_free_roam_profile(pMac, sessionId);
	csr_roam_free_connect_profile(&pSession->connectedProfile);
	csr_roam_free_connected_info(pMac, &pSession->connectedInfo);
	csr_roam_free_connected_info(pMac,
				     &pSession->prev_assoc_ap_info);
	csr_free_connect_bss_desc(pMac, sessionId);
	qdf_mem_zero(&pSession->selfMacAddr, sizeof(struct qdf_mac_addr));
	if (pSession->pWpaRsnReqIE) {
		qdf_mem_free(pSession->pWpaRsnReqIE);
		pSession->pWpaRsnReqIE = NULL;
	}
	pSession->nWpaRsnReqIeLength = 0;
	if (pSession->pWpaRsnRspIE) {
		qdf_mem_free(pSession->pWpaRsnRspIE);
		pSession->pWpaRsnRspIE = NULL;
	}
	pSession->nWpaRsnRspIeLength = 0;
#ifdef FEATURE_WLAN_WAPI
	if (pSession->pWapiReqIE) {
		qdf_mem_free(pSession->pWapiReqIE);
		pSession->pWapiReqIE = NULL;
	}
	pSession->nWapiReqIeLength = 0;
	if (pSession->pWapiRspIE) {
		qdf_mem_free(pSession->pWapiRspIE);
		pSession->pWapiRspIE = NULL;
	}
	pSession->nWapiRspIeLength = 0;
#endif /* FEATURE_WLAN_WAPI */
	if (pSession->pAddIEScan) {
		qdf_mem_free(pSession->pAddIEScan);
		pSession->pAddIEScan = NULL;
	}
	pSession->nAddIEScanLength = 0;
	if (pSession->pAddIEAssoc) {
		qdf_mem_free(pSession->pAddIEAssoc);
		pSession->pAddIEAssoc = NULL;
	}
	pSession->nAddIEAssocLength = 0;
}

QDF_STATUS csr_roam_get_session_id_from_bssid(tpAniSirGlobal pMac,
					      struct qdf_mac_addr *bssid,
					      uint32_t *pSessionId)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t i;

	for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
		if (CSR_IS_SESSION_VALID(pMac, i)) {
			if (qdf_is_macaddr_equal(bssid,
				    &pMac->roam.roamSession[i].connectedProfile.
				    bssid)) {
				/* Found it */
				status = QDF_STATUS_SUCCESS;
				*pSessionId = i;
				break;
			}
		}
	}
	return status;
}

/* This function assumes that we only support one IBSS session.
 * We cannot use BSSID to identify session because for IBSS,
 * the bssid changes.
 */
static uint32_t csr_find_ibss_session(tpAniSirGlobal pMac)
{
	uint32_t i, nRet = CSR_SESSION_ID_INVALID;
	struct csr_roam_session *pSession;

	for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
		if (CSR_IS_SESSION_VALID(pMac, i)) {
			pSession = CSR_GET_SESSION(pMac, i);
			if (pSession->pCurRoamProfile
			    &&
			    (csr_is_bss_type_ibss
				     (pSession->connectedProfile.BSSType))) {
				/* Found it */
				nRet = i;
				break;
			}
		}
	}
	return nRet;
}

static void csr_roam_link_up(tpAniSirGlobal pMac, struct qdf_mac_addr bssid)
{
	uint32_t sessionId = 0;

	/*
	 * Update the current BSS info in ho control block based on connected
	 * profile info from pmac global structure
	 */

	sme_debug(
		" csr_roam_link_up: WLAN link UP with AP= " MAC_ADDRESS_STR,
		MAC_ADDR_ARRAY(bssid.bytes));
	/* Check for user misconfig of RSSI trigger threshold */
	pMac->roam.configParam.vccRssiThreshold =
		(0 == pMac->roam.configParam.vccRssiThreshold) ?
		CSR_VCC_RSSI_THRESHOLD :
		pMac->roam.configParam.vccRssiThreshold;
	/* Check for user misconfig of UL MAC Loss trigger threshold */
	pMac->roam.configParam.vccUlMacLossThreshold =
		(0 == pMac->roam.configParam.vccUlMacLossThreshold) ?
		CSR_VCC_UL_MAC_LOSS_THRESHOLD : pMac->roam.configParam.
		vccUlMacLossThreshold;
	/* Indicate the neighbor roal algorithm about the connect indication */
	csr_roam_get_session_id_from_bssid(pMac, &bssid,
					   &sessionId);
	csr_neighbor_roam_indicate_connect(pMac, sessionId,
					   QDF_STATUS_SUCCESS);
}

static void csr_roam_link_down(tpAniSirGlobal pMac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}
	/* Only to handle the case for Handover on infra link */
	if (eCSR_BSS_TYPE_INFRASTRUCTURE != pSession->connectedProfile.BSSType)
		return;
	/*
	 * Incase of station mode, immediately stop data transmission whenever
	 * link down is detected.
	 */
	if (csr_roam_is_sta_mode(pMac, sessionId)
	    && !CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(pMac, sessionId)
	    && !csr_roam_is11r_assoc(pMac, sessionId)) {
		sme_debug("Inform Link lost for session %d",
			sessionId);
		csr_roam_call_callback(pMac, sessionId, NULL, 0,
				       eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_LOSTLINK);
	}
	/* deregister the clients requesting stats from PE/TL & also stop the
	 * corresponding timers
	 */
	csr_roam_dereg_statistics_req(pMac);
	/* Indicate the neighbor roal algorithm about the disconnect
	 * indication
	 */
	csr_neighbor_roam_indicate_disconnect(pMac, sessionId);

	/* Remove this code once SLM_Sessionization is supported */
	/* BMPS_WORKAROUND_NOT_NEEDED */
	if (!IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) &&
	    csr_is_infra_ap_started(pMac) &&
	    pMac->roam.configParam.doBMPSWorkaround) {
		pMac->roam.configParam.doBMPSWorkaround = 0;
	}

}

#ifndef QCA_SUPPORT_CP_STATS
QDF_STATUS csr_send_mb_stats_req_msg(tpAniSirGlobal pMac, uint32_t statsMask,
				     uint8_t staId, uint8_t sessionId)
{
	tAniGetPEStatsReq *pMsg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pMsg = qdf_mem_malloc(sizeof(tAniGetPEStatsReq));
	if (NULL == pMsg) {
		sme_err("Failed to allocate mem for stats req ");
		return QDF_STATUS_E_NOMEM;
	}
	/* need to initiate a stats request to PE */
	pMsg->msgType = eWNI_SME_GET_STATISTICS_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetPEStatsReq);
	pMsg->staId = staId;
	pMsg->statsMask = statsMask;
	pMsg->sessionId = sessionId;
	status = umac_send_mb_message_to_mac(pMsg);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_debug("Failed to send down the stats req ");

	return status;
}

/**
 * csr_update_stats() - updates correct stats struct in mac_ctx
 * @mac:             mac global context
 * @stats_type:      stats type
 * @sme_stats_rsp:   stats rsp msg packet
 * @stats:           input stats data buffer to fill in mac_ctx struct
 * @length:          out param - stats length
 *
 * This function fills corresponding stats struct in mac_cts based on stats type
 * passed
 *
 * Return: void
 */
static void
csr_update_stats(tpAniSirGlobal mac, uint8_t stats_type,
		 tAniGetPEStatsRsp *sme_stats_rsp,
		 uint8_t **stats, uint32_t *length)
{
	switch (stats_type) {
	case eCsrSummaryStats:
		sme_debug("summary stats");
		qdf_mem_copy((uint8_t *) &mac->roam.summaryStatsInfo, *stats,
			     sizeof(tCsrSummaryStatsInfo));
		*stats += sizeof(tCsrSummaryStatsInfo);
		*length -= sizeof(tCsrSummaryStatsInfo);
		break;
	case eCsrGlobalClassAStats:
		sme_debug("ClassA stats");
		qdf_mem_copy((uint8_t *) &mac->roam.classAStatsInfo, *stats,
			     sizeof(tCsrGlobalClassAStatsInfo));
		*stats += sizeof(tCsrGlobalClassAStatsInfo);
		*length -= sizeof(tCsrGlobalClassAStatsInfo);
		break;
	case csr_per_chain_rssi_stats:
		sme_debug("csrRoamStatsRspProcessor:Per Chain RSSI stats");
		qdf_mem_copy((uint8_t *)&mac->roam.per_chain_rssi_stats,
			*stats, sizeof(struct csr_per_chain_rssi_stats_info));
		*stats += sizeof(struct csr_per_chain_rssi_stats_info);
		*length -= sizeof(struct csr_per_chain_rssi_stats_info);
		break;
	default:
		sme_warn("unknown stats type");
		break;
	}
}

/**
 * csr_roam_stats_rsp_processor() - processes stats rsp msg
 * @pMac             mac global context
 * @pSirMsg:         incoming message
 *
 * Return: void
 */
void csr_roam_stats_rsp_processor(tpAniSirGlobal pMac, tSirSmeRsp *pSirMsg)
{
	tAniGetPEStatsRsp *pSmeStatsRsp;
	tListElem *pEntry = NULL;
	struct csr_statsclient_reqinfo *pTempStaEntry = NULL;
	struct csr_pestats_reqinfo *pPeStaEntry = NULL;
	uint32_t tempMask = 0;
	uint8_t counter = 0;
	uint8_t *pStats = NULL;
	uint32_t length = 0;
	int8_t rssi = 0, snr = 0;
	uint32_t *pRssi = NULL, *pSnr = NULL;
	uint32_t linkCapacity;

	pSmeStatsRsp = (tAniGetPEStatsRsp *) pSirMsg;
	if (pSmeStatsRsp->rc) {
		sme_warn("stats rsp from PE shows failure");
		goto post_update;
	}
	tempMask = pSmeStatsRsp->statsMask;
	pStats = ((uint8_t *) &pSmeStatsRsp->statsMask) +
		sizeof(pSmeStatsRsp->statsMask);
	/*
	 * subtract all statistics from this length, and after processing the
	 * entire 'stat' part of the message, if the length is not zero, then
	 * rssi is piggy packed in this 'stats' message.
	 */
	length = pSmeStatsRsp->msgLen - sizeof(tAniGetPEStatsRsp);
	/* new stats info from PE, fill up the stats strucutres in PMAC */
	while (tempMask) {
		if (tempMask & 1)
			csr_update_stats(pMac, counter, pSmeStatsRsp,
					 &pStats, &length);
		tempMask >>= 1;
		counter++;
	}
	if (length != 0) {
		pRssi = (uint32_t *) pStats;
		rssi = (int8_t) *pRssi;
		pStats += sizeof(uint32_t);
		length -= sizeof(uint32_t);
	} else
		/* If riva is not sending rssi, continue to use the hack */
		rssi = RSSI_HACK_BMPS;

	if (length != 0) {
		linkCapacity = *(uint32_t *) pStats;
		pStats += sizeof(uint32_t);
		length -= sizeof(uint32_t);
	} else
		linkCapacity = 0;

	if (length != 0) {
		pSnr = (uint32_t *) pStats;
		snr = (int8_t) *pSnr;
	} else
		snr = SNR_HACK_BMPS;

post_update:
	/* make sure to update the pe stats req list */
	pEntry = csr_roam_find_in_pe_stats_req_list(pMac,
						pSmeStatsRsp->statsMask);
	if (pEntry) {
		pPeStaEntry = GET_BASE_ADDR(pEntry,
					struct csr_pestats_reqinfo, link);
		pPeStaEntry->rspPending = false;

	}
	/* check the one timer cases */
	pEntry = csr_roam_check_client_req_list(pMac, pSmeStatsRsp->statsMask);
	if (pEntry) {
		pTempStaEntry =
			GET_BASE_ADDR(pEntry,
					struct csr_statsclient_reqinfo, link);
		if (pTempStaEntry->timerExpired) {
			/* send up the stats report */
			csr_roam_report_statistics(pMac,
						pTempStaEntry->statsMask,
						   pTempStaEntry->callback,
						   pTempStaEntry->staId,
						   pTempStaEntry->pContext);
			/* also remove from the client list */
			csr_roam_remove_stat_list_entry(pMac, pEntry);
			pTempStaEntry = NULL;
		}
	}
}

tListElem *csr_roam_find_in_pe_stats_req_list(
	tpAniSirGlobal pMac, uint32_t statsMask)
{
	tListElem *pEntry = NULL;
	struct csr_pestats_reqinfo *pTempStaEntry = NULL;

	pEntry = csr_ll_peek_head(&pMac->roam.peStatsReqList, LL_ACCESS_LOCK);
	if (!pEntry) {
		/* list empty */
		sme_debug("csr_roam_find_in_pe_stats_req_list: List empty, no request to PE");
		return NULL;
	}
	while (pEntry) {
	pTempStaEntry = GET_BASE_ADDR(pEntry, struct csr_pestats_reqinfo, link);
		if (pTempStaEntry->statsMask == statsMask)
			break;
		pEntry =
			csr_ll_next(&pMac->roam.peStatsReqList, pEntry,
				    LL_ACCESS_NOLOCK);
	}
	return pEntry;
}

static
tListElem *csr_roam_checkn_update_client_req_list(
tpAniSirGlobal pMac, struct csr_statsclient_reqinfo *pStaEntry,
						  bool update)
{
	tListElem *pEntry;
	struct csr_statsclient_reqinfo *pTempStaEntry;

	pEntry = csr_ll_peek_head(&pMac->roam.statsClientReqList,
				LL_ACCESS_LOCK);
	if (!pEntry) {
		/* list empty */
		sme_debug("List empty, no request from upper layer client(s)");
		return NULL;
	}
	while (pEntry) {
		pTempStaEntry =
			GET_BASE_ADDR(pEntry,
				struct csr_statsclient_reqinfo, link);
		if ((pTempStaEntry->requesterId == pStaEntry->requesterId)
		    && (pTempStaEntry->statsMask == pStaEntry->statsMask)) {
			if (update) {
				pTempStaEntry->callback = pStaEntry->callback;
				pTempStaEntry->pContext = pStaEntry->pContext;
			}
			break;
		}
		pEntry =
			csr_ll_next(&pMac->roam.statsClientReqList, pEntry,
				    LL_ACCESS_NOLOCK);
	}
	return pEntry;
}

tListElem *csr_roam_check_client_req_list(tpAniSirGlobal pMac,
					uint32_t statsMask)
{
	tListElem *pEntry;
	struct csr_statsclient_reqinfo *pTempStaEntry;

	pEntry = csr_ll_peek_head(&pMac->roam.statsClientReqList,
						LL_ACCESS_LOCK);
	if (!pEntry) {
		/* list empty */
		sme_debug("List empty, no request from upper layer client(s)");
		return NULL;
	}
	while (pEntry) {
		pTempStaEntry =
			GET_BASE_ADDR(pEntry,
				      struct csr_statsclient_reqinfo, link);
		if ((pTempStaEntry->
		     statsMask & ~(1 << eCsrGlobalClassDStats)) == statsMask) {
			break;
		}
		pEntry =
			csr_ll_next(&pMac->roam.statsClientReqList, pEntry,
				    LL_ACCESS_NOLOCK);
	}
	return pEntry;
}

struct csr_statsclient_reqinfo *csr_roam_insert_entry_into_list(
	tpAniSirGlobal pMac, tDblLinkList *pStaList,
	struct csr_statsclient_reqinfo *
	pStaEntry)
{
	struct csr_statsclient_reqinfo *pNewStaEntry = NULL;
	/*
	 * if same entity requested for same set of stats with different
	 * callback update it
	 */
	if (NULL == csr_roam_checkn_update_client_req_list(pMac, pStaEntry,
								true)) {

	pNewStaEntry = qdf_mem_malloc(sizeof(struct csr_statsclient_reqinfo));
		if (NULL == pNewStaEntry) {
			sme_err("couldn't allocate memory for the entry");
			return NULL;
		}

		pNewStaEntry->callback = pStaEntry->callback;
		pNewStaEntry->pContext = pStaEntry->pContext;
		pNewStaEntry->requesterId = pStaEntry->requesterId;
		pNewStaEntry->statsMask = pStaEntry->statsMask;
		pNewStaEntry->pPeStaEntry = pStaEntry->pPeStaEntry;
		pNewStaEntry->pMac = pStaEntry->pMac;
		pNewStaEntry->staId = pStaEntry->staId;
		pNewStaEntry->timerExpired = pStaEntry->timerExpired;

		csr_ll_insert_tail(pStaList, &pNewStaEntry->link,
							LL_ACCESS_LOCK);
	}
	return pNewStaEntry;
}
#endif /* QCA_SUPPORT_CP_STATS */

QDF_STATUS csr_get_rssi(tpAniSirGlobal pMac,
			tCsrRssiCallback callback,
			uint8_t staId,
			struct qdf_mac_addr bssId,
			int8_t lastRSSI, void *pContext)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};
	uint32_t sessionId;
	tAniGetRssiReq *pMsg;

	status = csr_roam_get_session_id_from_bssid(pMac, &bssId, &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		callback(lastRSSI, staId, pContext);
		sme_err("Failed to get SessionId");
		return QDF_STATUS_E_FAILURE;
	}

	pMsg = qdf_mem_malloc(sizeof(tAniGetRssiReq));
	if (NULL == pMsg) {
		sme_err("csr_get_rssi: failed to allocate mem for req ");
		return QDF_STATUS_E_NOMEM;
	}

	pMsg->msgType = eWNI_SME_GET_RSSI_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetRssiReq);
	pMsg->sessionId = sessionId;
	pMsg->staId = staId;
	pMsg->rssiCallback = callback;
	pMsg->pDevContext = pContext;
	/*
	 * store RSSI at time of calling, so that if RSSI request cannot
	 * be sent to firmware, this value can be used to return immediately
	 */
	pMsg->lastRSSI = lastRSSI;
	msg.type = eWNI_SME_GET_RSSI_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		sme_err("scheduler_post_msg failed to post msg to self");
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}

QDF_STATUS csr_get_snr(tpAniSirGlobal pMac,
		       tCsrSnrCallback callback,
		       uint8_t staId, struct qdf_mac_addr bssId, void *pContext)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};
	uint32_t sessionId = CSR_SESSION_ID_INVALID;
	tAniGetSnrReq *pMsg;

	pMsg = (tAniGetSnrReq *) qdf_mem_malloc(sizeof(tAniGetSnrReq));
	if (NULL == pMsg) {
		sme_err("failed to allocate mem for req");
		return QDF_STATUS_E_NOMEM;
	}

	status = csr_roam_get_session_id_from_bssid(pMac, &bssId, &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(pMsg);
		sme_err("Couldn't find session_id for given BSSID");
		return status;
	}

	pMsg->msgType = eWNI_SME_GET_SNR_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetSnrReq);
	pMsg->sessionId = sessionId;
	pMsg->staId = staId;
	pMsg->snrCallback = callback;
	pMsg->pDevContext = pContext;
	msg.type = eWNI_SME_GET_SNR_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		sme_err("failed to post msg to self");
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

#ifndef QCA_SUPPORT_CP_STATS
/**
 * csr_deregister_client_request() - deregisters a get stats request
 * @mac_ctx:       mac global context
 * @sta_entry:     stats request entry
 *
 * Return: status of operation
 */
static QDF_STATUS
csr_deregister_client_request(tpAniSirGlobal mac_ctx,
			      struct csr_statsclient_reqinfo *sta_entry)
{
	QDF_STATUS status;
	tListElem *entry = NULL;
	struct csr_statsclient_reqinfo *ptr_sta_entry = NULL;

	entry = csr_roam_checkn_update_client_req_list(mac_ctx, sta_entry,
						      false);
	if (!entry) {
	sme_err("callback is empty in the request & couldn't find any existing request in statsClientReqList");
		return QDF_STATUS_E_FAILURE;
	}
	/* clean up & return */
	ptr_sta_entry = GET_BASE_ADDR(entry,
				      struct csr_statsclient_reqinfo, link);
	if (NULL != ptr_sta_entry->pPeStaEntry) {
		ptr_sta_entry->pPeStaEntry->numClient--;
		/* check if we need to delete the entry from peStatsReqList */
		if (!ptr_sta_entry->pPeStaEntry->numClient)
			csr_roam_remove_entry_from_pe_stats_req_list(mac_ctx,
						ptr_sta_entry->pPeStaEntry);
	}
	/* check if we need to stop the tl stats timer too */
	mac_ctx->roam.tlStatsReqInfo.numClient--;
	qdf_mc_timer_stop(&ptr_sta_entry->timer);
	/* Destroy the qdf timer... */
	status = qdf_mc_timer_destroy(&ptr_sta_entry->timer);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err(
			"failed to destroy Client req timer");

	csr_roam_remove_stat_list_entry(mac_ctx, entry);
	return QDF_STATUS_SUCCESS;
}

/**
 * csr_insert_stats_request_to_list() - inserts request to existing list
 * @mac_ctx:       mac global context
 * @sta_entry:     stats request entry
 *
 * Return: status of operation
 */
static QDF_STATUS
csr_insert_stats_request_to_list(tpAniSirGlobal mac_ctx,
				 struct csr_statsclient_reqinfo *sta_entry)
{
struct csr_statsclient_reqinfo *ptr_sta_entry = csr_roam_insert_entry_into_list(
				mac_ctx, &mac_ctx->roam.statsClientReqList,
				sta_entry);
	if (!ptr_sta_entry) {
		sme_err("Failed to insert req in statsClientReqList");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_get_statistics(tpAniSirGlobal pMac,
			      eCsrStatsRequesterType requesterId,
			      uint32_t statsMask,
			      tCsrStatsCallback callback,
			      uint8_t staId,
			      void *pContext,
			      uint8_t sessionId)
{
	struct csr_statsclient_reqinfo staEntry;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool insertInClientList = false;
	uint32_t temp_mask = 0;

	if (csr_is_all_session_disconnected(pMac))
		return QDF_STATUS_E_FAILURE;

	if (csr_neighbor_middle_of_roaming(pMac, sessionId)) {
		sme_debug("in the middle of roaming states");
		return QDF_STATUS_E_FAILURE;
	}

	if ((!statsMask) && (!callback)) {
		sme_err("statsMask & callback empty in the request");
		return QDF_STATUS_E_FAILURE;
	}
	/* for the search list method for deregister */
	staEntry.requesterId = requesterId;
	staEntry.statsMask = statsMask;
	/* requester wants to deregister or just an error */
	if ((statsMask) && (!callback))
		return csr_deregister_client_request(pMac, &staEntry);

	/* add the request in the client req list */
	staEntry.callback = callback;
	staEntry.pContext = pContext;
	staEntry.pPeStaEntry = NULL;
	staEntry.staId = staId;
	staEntry.pMac = pMac;
	staEntry.timerExpired = false;
	staEntry.sessionId = sessionId;

	temp_mask = statsMask & ~(1 << eCsrGlobalClassDStats);
	if (temp_mask) {
		/* send down a req */
		status = csr_send_mb_stats_req_msg(pMac,
					temp_mask, staId, sessionId);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("failed to send down stats req");
		/*
		 * so that when the stats rsp comes back from PE we
		 * respond to upper layer right away
		 */
		staEntry.timerExpired = true;
		insertInClientList = true;
	}
	/* if looking for stats from TL only */
	if (!insertInClientList) {
		/* return the stats */
		csr_roam_report_statistics(pMac, statsMask, callback,
					   staId, pContext);
		return QDF_STATUS_SUCCESS;
	}
	if (insertInClientList)
		return csr_insert_stats_request_to_list(pMac, &staEntry);

	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_SUPPORT_CP_STATS */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS csr_roam_send_rso_cmd(tpAniSirGlobal mac_ctx, uint8_t session_id,
				 tSirRoamOffloadScanReq *request_buf)
{
	QDF_STATUS status;

	request_buf->message_type = eWNI_SME_ROAM_SCAN_OFFLOAD_REQ;
	request_buf->length = sizeof(*request_buf);
	status = umac_send_mb_message_to_mac(request_buf);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Send RSO from CSR failed");
		return status;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * csr_roam_set_key_mgmt_offload() - enable/disable key mgmt offload
 * @mac_ctx: mac context.
 * @session_id: Session Identifier
 * @roam_key_mgmt_offload_enabled: key mgmt enable/disable flag
 * @pmkid_modes: PMKID modes of PMKSA caching and OKC
 *
 * Return: QDF_STATUS_SUCCESS - CSR updated config successfully.
 * Other status means CSR is failed to update.
 */

QDF_STATUS csr_roam_set_key_mgmt_offload(tpAniSirGlobal mac_ctx,
					 uint32_t session_id,
					 bool roam_key_mgmt_offload_enabled,
					 struct pmkid_mode_bits *pmkid_modes)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("session %d not found", session_id);
		return QDF_STATUS_E_FAILURE;
	}
	session->RoamKeyMgmtOffloadEnabled = roam_key_mgmt_offload_enabled;
	session->pmkid_modes.fw_okc = pmkid_modes->fw_okc;
	session->pmkid_modes.fw_pmksa_cache = pmkid_modes->fw_pmksa_cache;
	return QDF_STATUS_SUCCESS;
}

/**
 * csr_update_roam_scan_ese_params() - Update ESE related params in RSO request
 * @req_buf: Roam Scan Offload Request buffer
 * @session: Current Roam Session
 *
 * This API will set the KRK and BTK required in case of Auth Type is CCKM.
 * It will also clear the PMK Len as CCKM PMK Caching is not supported
 *
 * Return: None
 */
#ifdef FEATURE_WLAN_ESE
static
void csr_update_roam_scan_ese_params(tSirRoamOffloadScanReq *req_buf,
				     struct csr_roam_session *session)
{
	if (csr_is_auth_type_ese(req_buf->ConnectedNetwork.authentication)) {
		qdf_mem_copy(req_buf->KRK, session->eseCckmInfo.krk,
			     SIR_KRK_KEY_LEN);
		qdf_mem_copy(req_buf->BTK, session->eseCckmInfo.btk,
			     SIR_BTK_KEY_LEN);
		req_buf->pmkid_modes.fw_okc = 0;
		req_buf->pmkid_modes.fw_pmksa_cache = 0;
		req_buf->pmk_len = 0;
		qdf_mem_zero(&req_buf->PSK_PMK[0], sizeof(req_buf->PSK_PMK));
	}
}
#else
static inline
void csr_update_roam_scan_ese_params(tSirRoamOffloadScanReq *req_buf,
				     struct csr_roam_session *session)
{
}
#endif

#ifdef WLAN_ADAPTIVE_11R
static void
csr_update_roam_req_adaptive_11r(struct csr_roam_session *session,
				 tSirRoamOffloadScanReq *req_buf)
{
	req_buf->is_adaptive_11r_connection =
			session->is_adaptive_11r_connection;
}
#else
static void
csr_update_roam_req_adaptive_11r(struct csr_roam_session *session,
				 tSirRoamOffloadScanReq *req_buf)
{
	req_buf->is_adaptive_11r_connection = false;
}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_fill_sae_single_pmk_info() - updates req msg with sae single pmk info
 * @mac_ctx: Mac context
 * @req_buf: out param, roam offload scan request packet
 * @session: csr session
 *
 * Return: true in case of success
 */
static bool
csr_fill_sae_single_pmk_info(tpAniSirGlobal mac_ctx,
			     tSirRoamOffloadScanReq *req_buf,
			     struct csr_roam_session *session)
{
	if (!mac_ctx || !req_buf) {
		sme_debug("Invalid session or req buff");
		return false;
	}

	if (session->single_pmk_info.pmk_info.pmk_len &&
	    session->single_pmk_info.sae_single_pmk_ap &&
	    mac_ctx->roam.configParam.sae_single_pmk_feature_enabled) {
		/* Update sae same pmk info in rso */
		qdf_mem_copy(req_buf->PSK_PMK,
			     session->single_pmk_info.pmk_info.pmk,
			     sizeof(req_buf->PSK_PMK));
		req_buf->pmk_len = session->single_pmk_info.pmk_info.pmk_len;

		req_buf->is_sae_single_pmk =
				session->single_pmk_info.sae_single_pmk_ap;

		return true;
	}

	return false;
}
#else
static bool
csr_fill_sae_single_pmk_info(tpAniSirGlobal mac_ctx,
			     tSirRoamOffloadScanReq *req_buf,
			     struct csr_roam_session *session)
{
	return false;
}
#endif

/**
 * csr_update_roam_scan_offload_request() - updates req msg with roam offload
 * parameters
 * @pMac:          mac global context
 * @req_buf:       out param, roam offload scan request packet
 * @session:       roam session
 *
 * Return: void
 */
static void
csr_update_roam_scan_offload_request(tpAniSirGlobal mac_ctx,
				     tSirRoamOffloadScanReq *req_buf,
				     struct csr_roam_session *session)
{
	tCsrNeighborRoamControlInfo *neighbor_roam_info;
	uint32_t pmkid_modes = mac_ctx->roam.configParam.pmkid_modes;

	req_buf->RoamOffloadEnabled = csr_roamIsRoamOffloadEnabled(mac_ctx);
	/* Roam Offload piggybacks upon the Roam Scan offload command. */
	if (!req_buf->RoamOffloadEnabled) {
		sme_debug("LFR3: LFR3 INI is disabled");
		return;
	}

	neighbor_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session->sessionId];
	req_buf->roam_triggers.vdev_id = session->sessionId;
	req_buf->roam_triggers.trigger_bitmap =
		neighbor_roam_info->cfgParams.roam_trigger_bitmap;

	req_buf->RoamKeyMgmtOffloadEnabled = session->RoamKeyMgmtOffloadEnabled;
	req_buf->pmkid_modes.fw_okc =
		(pmkid_modes & CFG_PMKID_MODES_OKC) ? 1 : 0;
	req_buf->pmkid_modes.fw_pmksa_cache =
		(pmkid_modes & CFG_PMKID_MODES_PMKSA_CACHING) ? 1 : 0;

	qdf_mem_copy(&req_buf->roam_params,
		     &mac_ctx->roam.configParam.roam_params,
		     sizeof(req_buf->roam_params));

	/* Check whether to send psk_pmk or sae_single pmk info */
	if (!csr_fill_sae_single_pmk_info(mac_ctx, req_buf, session)) {
		qdf_mem_copy(req_buf->PSK_PMK, session->psk_pmk,
			     sizeof(req_buf->PSK_PMK));
		req_buf->pmk_len = session->pmk_len;
	}

	req_buf->R0KH_ID_Length = session->ftSmeContext.r0kh_id_len;
	qdf_mem_copy(req_buf->R0KH_ID,
		     session->ftSmeContext.r0kh_id,
		     req_buf->R0KH_ID_Length);
	req_buf->Prefer5GHz = mac_ctx->roam.configParam.nRoamPrefer5GHz;
	req_buf->RoamRssiCatGap = mac_ctx->roam.configParam.bCatRssiOffset;
	req_buf->Select5GHzMargin = mac_ctx->roam.configParam.nSelect5GHzMargin;
	req_buf->ho_delay_for_rx = mac_ctx->roam.configParam.ho_delay_for_rx;
	req_buf->roam_preauth_retry_count =
		mac_ctx->roam.configParam.roam_preauth_retry_count;
	req_buf->roam_preauth_no_ack_timeout =
		mac_ctx->roam.configParam.roam_preauth_no_ack_timeout;
	req_buf->min_delay_btw_roam_scans =
			mac_ctx->roam.configParam.min_delay_btw_roam_scans;
	req_buf->roam_trigger_reason_bitmask =
			mac_ctx->roam.configParam.roam_trigger_reason_bitmask;
	req_buf->roaming_scan_policy =
			mac_ctx->roam.configParam.roaming_scan_policy;
	/* Do not force RSSI triggers in case controlled roaming enable */
	req_buf->roam_force_rssi_trigger =
			(!neighbor_roam_info->roam_control_enable &&
			 mac_ctx->roam.configParam.roam_force_rssi_trigger);

	csr_update_roam_req_adaptive_11r(session, req_buf);

	/* fill bss load triggered roam related configs */
	req_buf->bss_load_trig_enabled =
			mac_ctx->roam.configParam.enable_bss_load_roam_trigger;
	req_buf->bss_load_config.bss_load_threshold =
			mac_ctx->roam.configParam.bss_load_threshold;
	req_buf->bss_load_config.bss_load_sample_time =
			mac_ctx->roam.configParam.bss_load_sample_time;
	req_buf->bss_load_config.vdev_id = session->sessionId;
	req_buf->bss_load_config.rssi_threshold_5ghz =
		(mac_ctx->roam.configParam.bss_load_trigger_rssi_threshold_5ghz -
		 WMA_NOISE_FLOOR_DBM_DEFAULT);
	req_buf->bss_load_config.rssi_threshold_5ghz &= 0x000000ff;
	req_buf->bss_load_config.rssi_threshold_24ghz =
		(mac_ctx->roam.configParam.bss_load_trigger_rssi_threshold_24ghz -
		 WMA_NOISE_FLOOR_DBM_DEFAULT);
	req_buf->bss_load_config.rssi_threshold_24ghz &= 0x000000ff;

	req_buf->min_rssi_params[DEAUTH_MIN_RSSI].min_rssi =
		(mac_ctx->roam.configParam.disconnect_roam_min_rssi -
		 WMA_NOISE_FLOOR_DBM_DEFAULT);
	req_buf->min_rssi_params[DEAUTH_MIN_RSSI].min_rssi &= 0x000000ff;
	req_buf->min_rssi_params[DEAUTH_MIN_RSSI].trigger_reason =
				ROAM_TRIGGER_REASON_DEAUTH;

	req_buf->min_rssi_params[BMISS_MIN_RSSI].min_rssi =
		(mac_ctx->roam.configParam.bmiss_roam_min_rssi -
		 WMA_NOISE_FLOOR_DBM_DEFAULT);
	req_buf->min_rssi_params[BMISS_MIN_RSSI].min_rssi &= 0x000000ff;
	req_buf->min_rssi_params[BMISS_MIN_RSSI].trigger_reason =
		ROAM_TRIGGER_REASON_BMISS;

	req_buf->score_delta_param[IDLE_ROAM_TRIGGER].roam_score_delta =
		mac_ctx->roam.configParam.idle_roam_score_delta;
	req_buf->score_delta_param[IDLE_ROAM_TRIGGER].trigger_reason =
		ROAM_TRIGGER_REASON_IDLE;

	req_buf->score_delta_param[BTM_ROAM_TRIGGER].roam_score_delta =
		mac_ctx->roam.configParam.btm_roam_score_delta;
	req_buf->score_delta_param[BTM_ROAM_TRIGGER].trigger_reason =
		ROAM_TRIGGER_REASON_BTM;

	req_buf->disconnect_roam_params.enable =
		mac_ctx->roam.configParam.enable_disconnect_roam_offload;
	req_buf->disconnect_roam_params.vdev_id = session->sessionId;

	req_buf->idle_roam_params.vdev_id = session->sessionId;
	req_buf->idle_roam_params.enable =
		mac_ctx->roam.configParam.enable_idle_roam;
	req_buf->idle_roam_params.conn_ap_rssi_delta =
		mac_ctx->roam.configParam.idle_roam_rssi_delta;
	req_buf->idle_roam_params.inactive_time =
		mac_ctx->roam.configParam.idle_roam_inactive_time;
	req_buf->idle_roam_params.data_pkt_count =
		mac_ctx->roam.configParam.idle_data_packet_count;
	req_buf->idle_roam_params.band =
		mac_ctx->roam.configParam.idle_roam_band;
	req_buf->idle_roam_params.conn_ap_min_rssi =
		(mac_ctx->roam.configParam.idle_roam_min_rssi -
		 WMA_NOISE_FLOOR_DBM_DEFAULT);
	req_buf->idle_roam_params.conn_ap_min_rssi &= 0x000000ff;

	if (wlan_cfg_get_int(mac_ctx, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
			     (uint32_t *) &req_buf->ReassocFailureTimeout)
	    != QDF_STATUS_SUCCESS) {
		sme_err(
			"could not retrieve ReassocFailureTimeout value");
		req_buf->ReassocFailureTimeout =
			DEFAULT_REASSOC_FAILURE_TIMEOUT;
	}

	csr_update_roam_scan_ese_params(req_buf, session);

	req_buf->AcUapsd.acbe_uapsd = SIR_UAPSD_GET(ACBE, session->uapsd_mask);
	req_buf->AcUapsd.acbk_uapsd = SIR_UAPSD_GET(ACBK, session->uapsd_mask);
	req_buf->AcUapsd.acvi_uapsd = SIR_UAPSD_GET(ACVI, session->uapsd_mask);
	req_buf->AcUapsd.acvo_uapsd = SIR_UAPSD_GET(ACVO, session->uapsd_mask);
}

#ifdef WLAN_FEATURE_FIPS
QDF_STATUS
csr_process_roam_pmkid_req_callback(tpAniSirGlobal mac_ctx,
				    uint8_t vdev_id,
				    struct roam_pmkid_req_event *src_lst)
{
	struct csr_roam_info *roam_info;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);
	struct qdf_mac_addr *dst_list;
	QDF_STATUS status;
	uint32_t num_entries, i;

	if (!session)
		return QDF_STATUS_E_NULL_VALUE;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	num_entries = src_lst->num_entries;
	for (i = 0; i < num_entries; i++) {
		dst_list = &src_lst->ap_bssid[i];
		qdf_copy_macaddr(&roam_info->bssid, dst_list);

		status = csr_roam_call_callback(mac_ctx, vdev_id, roam_info,
						0, eCSR_ROAM_FIPS_PMK_REQUEST,
						eCSR_ROAM_RESULT_NONE);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("%s: Trigger pmkid fallback failed", __func__);
			qdf_mem_free(roam_info);
			return status;
		}
	}
	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
csr_roam_pmkid_req_callback(uint8_t vdev_id,
			    struct roam_pmkid_req_event *src_lst)
{
	QDF_STATUS status;
	tpAniSirGlobal mac_ctx;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		sme_err("%s: NULL mac ptr", __func__);
		QDF_ASSERT(0);
		return -EINVAL;
	}

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("%s: Locking failed, bailing out", __func__);
		return status;
	}

	status = csr_process_roam_pmkid_req_callback(mac_ctx, vdev_id,
						     src_lst);
	sme_release_global_lock(&mac_ctx->sme);

	return status;
}
#endif /* WLAN_FEATURE_FIPS */
#else
static inline void
csr_update_roam_scan_offload_request(tpAniSirGlobal mac_ctx,
				     tSirRoamOffloadScanReq *req_buf,
				     struct csr_roam_session *session)
{}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_check_band_channel_match() - check if passed band and channel match
 * parameters
 * @band:       band to match with channel
 * @channel:    channel to match with band
 *
 * Return: bool if match else false
 */
static bool
csr_check_band_channel_match(enum band_info band, uint8_t channel)
{
	if (BAND_ALL == band)
		return true;

	if (BAND_2G == band && WLAN_REG_IS_24GHZ_CH(channel))
		return true;

	if (BAND_5G == band && WLAN_REG_IS_5GHZ_CH(channel))
		return true;

	return false;
}

/**
 * csr_populate_roam_chan_list()
 * parameters
 * @mac_ctx:      global mac ctx
 * @dst: Destination roam network to populate the roam chan list
 * @src: Source channel list
 *
 * Return: QDF_STATUS enumeration
 */
static QDF_STATUS
csr_populate_roam_chan_list(tpAniSirGlobal mac_ctx,
			    tSirRoamNetworkType *dst,
			    tCsrChannelInfo *src)
{
	enum band_info band;
	uint8_t i = 0;
	uint8_t num_channels = 0;
	uint8_t *ch_lst = src->ChannelList;
	uint16_t  unsafe_chan[NUM_CHANNELS];
	uint16_t  unsafe_chan_cnt = 0;
	uint16_t  cnt = 0;
	bool      is_unsafe_chan;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		cds_err("qdf_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
			&unsafe_chan_cnt,
			 sizeof(unsafe_chan));

	/*
	 * The INI channels need to be filtered with respect to the current band
	 * that is supported.
	 */
	band = mac_ctx->roam.configParam.bandCapability;
	if ((BAND_2G != band) && (BAND_5G != band)
	    && (BAND_ALL != band)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "Invalid band(%d), roam scan offload req aborted",
			  band);
		return QDF_STATUS_E_FAILURE;
	}

	num_channels = dst->ChannelCount;
	for (i = 0; i < src->numOfChannels; i++) {
		if (csr_is_channel_present_in_list(dst->ChannelCache,
						   num_channels,
						   *ch_lst)) {
			ch_lst++;
			continue;
		}
		if (!csr_check_band_channel_match(band, *ch_lst)) {
			ch_lst++;
			continue;
		}
		/* Allow DFS channels only if the DFS roaming is enabled */
		if ((!mac_ctx->roam.configParam.allowDFSChannelRoam ||
		    (mac_ctx->roam.configParam.sta_roam_policy.dfs_mode ==
			 CSR_STA_ROAM_POLICY_DFS_DISABLED)) &&
		     (wlan_reg_is_dfs_ch(mac_ctx->pdev, *ch_lst))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				("ignoring dfs channel %d"), *ch_lst);
			ch_lst++;
			continue;
		}

		if (mac_ctx->roam.configParam.sta_roam_policy.
				skip_unsafe_channels &&
				unsafe_chan_cnt) {
			is_unsafe_chan = false;
			for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
				if (unsafe_chan[cnt] == *ch_lst) {
					is_unsafe_chan = true;
					break;
				}
			}
			if (is_unsafe_chan) {
				QDF_TRACE(QDF_MODULE_ID_SME,
						QDF_TRACE_LEVEL_DEBUG,
					("ignoring unsafe channel %d"),
					*ch_lst);
				ch_lst++;
				continue;
			}
		}
		dst->ChannelCache[num_channels++] = *ch_lst;
		ch_lst++;
	}
	dst->ChannelCount = num_channels;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_fetch_ch_lst_from_ini() - fetch channel list from ini and update req msg
 * parameters
 * @mac_ctx:      global mac ctx
 * @roam_info:    roam info struct
 * @req_buf:      out param, roam offload scan request packet
 *
 * Return: result of operation
 */
static QDF_STATUS
csr_fetch_ch_lst_from_ini(tpAniSirGlobal mac_ctx,
			  tpCsrNeighborRoamControlInfo roam_info,
			  struct sSirRoamOffloadScanReq *req_buf)
{
	QDF_STATUS status;
	tCsrChannelInfo *specific_chan_info;

	specific_chan_info = &roam_info->cfgParams.specific_chan_info;

	status = csr_populate_roam_chan_list(mac_ctx,
					     &req_buf->ConnectedNetwork,
					     specific_chan_info);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Failed to copy channels to roam list");
		return status;
	}
	req_buf->ChannelCacheType = CHANNEL_LIST_STATIC;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_fetch_ch_lst_from_occupied_lst() - fetch channel list from occupied list
 * and update req msg
 * parameters
 * @mac_ctx:      global mac ctx
 * @session_id:   session id
 * @reason:       reason to roam
 * @req_buf:      out param, roam offload scan request packet
 * @roam_info:    roam info struct
 *
 * Return: void
 */
static void
csr_fetch_ch_lst_from_occupied_lst(tpAniSirGlobal mac_ctx,
				   uint8_t session_id,
				   uint8_t reason,
				   tSirRoamOffloadScanReq *req_buf,
				   tpCsrNeighborRoamControlInfo roam_info)
{
	uint8_t i = 0;
	uint8_t num_channels = 0;
	uint8_t *ch_lst =
		mac_ctx->scan.occupiedChannels[session_id].channelList;
	uint16_t  unsafe_chan[NUM_CHANNELS];
	uint16_t  unsafe_chan_cnt = 0;
	uint16_t  cnt = 0;
	bool      is_unsafe_chan;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		cds_err("qdf_ctx is NULL");
		return;
	}

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
			&unsafe_chan_cnt,
			 sizeof(unsafe_chan));
	for (i = 0; i < mac_ctx->scan.occupiedChannels[session_id].numChannels;
	     i++) {
		if ((!mac_ctx->roam.configParam.allowDFSChannelRoam ||
		    (mac_ctx->roam.configParam.sta_roam_policy.dfs_mode ==
			 CSR_STA_ROAM_POLICY_DFS_DISABLED)) &&
		     (wlan_reg_is_dfs_ch(mac_ctx->pdev, *ch_lst))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				("ignoring dfs channel %d"), *ch_lst);
			ch_lst++;
			continue;
		}

		if (mac_ctx->roam.configParam.sta_roam_policy.
				skip_unsafe_channels &&
				unsafe_chan_cnt) {
			is_unsafe_chan = false;
			for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
				if (unsafe_chan[cnt] == *ch_lst) {
					is_unsafe_chan = true;
					break;
				}
			}
			if (is_unsafe_chan) {
				QDF_TRACE(QDF_MODULE_ID_SME,
						QDF_TRACE_LEVEL_DEBUG,
					("ignoring unsafe channel %d"),
					*ch_lst);
				ch_lst++;
				continue;
			}
		}
		req_buf->ConnectedNetwork.ChannelCache[num_channels++] =
			*ch_lst;
		ch_lst++;
	}
	req_buf->ConnectedNetwork.ChannelCount = num_channels;
	req_buf->ChannelCacheType = CHANNEL_LIST_DYNAMIC;
}

/**
 * csr_fetch_valid_ch_lst() - fetch channel list from valid channel list and
 * update req msg
 * parameters
 * @mac_ctx:            global mac ctx
 * @req_buf:            out param, roam offload scan request packet
 *
 * Return: void
 */
static QDF_STATUS
csr_fetch_valid_ch_lst(tpAniSirGlobal mac_ctx,
		       tSirRoamOffloadScanReq *req_buf,
		       uint8_t session_id)
{
	QDF_STATUS status;
	uint32_t host_channels = 0;
	uint8_t *ch_lst = NULL;
	uint8_t i = 0, num_channels = 0;
	uint16_t  unsafe_chan[NUM_CHANNELS];
	uint16_t  unsafe_chan_cnt = 0;
	uint16_t  cnt = 0;
	bool      is_unsafe_chan;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	enum band_info band = BAND_ALL;

	if (!qdf_ctx) {
		cds_err("qdf_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
			&unsafe_chan_cnt,
			sizeof(unsafe_chan));

	host_channels = sizeof(mac_ctx->roam.validChannelList);
	status = csr_get_cfg_valid_channels(mac_ctx,
					    mac_ctx->roam.validChannelList,
					    &host_channels);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "Failed to get the valid channel list");
		return status;
	}

	if (CSR_IS_ROAM_INTRA_BAND_ENABLED(mac_ctx)) {
		band = csr_get_rf_band(mac_ctx->roam.roamSession[session_id].
				connectedProfile.operationChannel);
		sme_debug("updated band %d operational ch %d", band,
				mac_ctx->roam.roamSession[session_id].
				connectedProfile.operationChannel);
	}

	ch_lst = mac_ctx->roam.validChannelList;
	mac_ctx->roam.numValidChannels = host_channels;

	for (i = 0; i < mac_ctx->roam.numValidChannels; i++) {
		if (!csr_check_band_channel_match(band, *ch_lst)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				("ignoring non-intra band channel %d"),
				*ch_lst);
			ch_lst++;
			continue;
		}

		if ((!mac_ctx->roam.configParam.allowDFSChannelRoam ||
		    (mac_ctx->roam.configParam.sta_roam_policy.dfs_mode ==
			 CSR_STA_ROAM_POLICY_DFS_DISABLED)) &&
		     (wlan_reg_is_dfs_ch(mac_ctx->pdev, *ch_lst))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				("ignoring dfs channel %d"), *ch_lst);
			ch_lst++;
			continue;
		}

		if (mac_ctx->roam.configParam.
				sta_roam_policy.skip_unsafe_channels &&
				unsafe_chan_cnt) {
			is_unsafe_chan = false;
			for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
				if (unsafe_chan[cnt] == *ch_lst) {
					is_unsafe_chan = true;
					break;
				}
			}
			if (is_unsafe_chan) {
				QDF_TRACE(QDF_MODULE_ID_SME,
						QDF_TRACE_LEVEL_DEBUG,
					("ignoring unsafe channel %d"),
					*ch_lst);
				ch_lst++;
				continue;
			}
		}
		req_buf->ConnectedNetwork.ChannelCache[num_channels++] =
			*ch_lst;
		ch_lst++;
	}
	req_buf->ValidChannelCount = num_channels;

	req_buf->ChannelCacheType = CHANNEL_LIST_DYNAMIC;
	req_buf->ConnectedNetwork.ChannelCount = num_channels;
	return status;
}

/**
 * csr_add_ch_lst_from_roam_scan_list() - channel from roam scan chan list
 * parameters
 * @mac_ctx: Global mac ctx
 * @req_buf: out param, roam offload scan request packet
 * @roam_info: roam info struct
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
csr_add_ch_lst_from_roam_scan_list(tpAniSirGlobal mac_ctx,
				   tSirRoamOffloadScanReq *req_buf,
				   tpCsrNeighborRoamControlInfo roam_info)
{
	QDF_STATUS status;
	tCsrChannelInfo *pref_chan_info = &roam_info->cfgParams.pref_chan_info;

	if (!pref_chan_info->numOfChannels)
		return QDF_STATUS_SUCCESS;

	status = csr_populate_roam_chan_list(mac_ctx,
					     &req_buf->ConnectedNetwork,
					     pref_chan_info);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Failed to copy channels to roam list");
		return status;
	}
	sme_dump_chan_list(pref_chan_info);
	req_buf->ChannelCacheType = CHANNEL_LIST_DYNAMIC;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11W
/**
 * csr_rso_command_fill_11w_params() - Fill the 11w related parameters
 * into the roam buffer
 * @mac_ctx: Global mac context buffer
 * @rso_req: RSO command buffer to be filled
 *
 * Return: None
 */
static void
csr_rso_command_fill_11w_params(tpAniSirGlobal mac_ctx,
				uint8_t session_id,
				tSirRoamOffloadScanReq *rso_req)
{
	tSirRoamNetworkType *network_cfg = &rso_req->ConnectedNetwork;
	tCsrRoamConnectedProfile session_connected_profile =
			mac_ctx->roam.roamSession[session_id].connectedProfile;
	tAniEdType group_mgmt_cipher;

	network_cfg->mfp_enabled = session_connected_profile.MFPEnabled;
	group_mgmt_cipher = session_connected_profile.mgmt_encryption_type;
	if (network_cfg->mfp_enabled)
		network_cfg->gp_mgmt_cipher_suite = group_mgmt_cipher;
	else
		network_cfg->gp_mgmt_cipher_suite = eSIR_ED_NONE;
}

#else
static inline
void csr_rso_command_fill_11w_params(tpAniSirGlobal mac_ctx,
				     uint8_t session_id,
				     tSirRoamOffloadScanReq *rso_req)
{}
#endif

/**
 * csr_update_btm_offload_config() - Update btm config param to fw
 * @mac_ctx: Global mac ctx
 * @command: Roam offload command
 * @req_buf: roam offload scan request
 * @session: roam session
 *
 * Return: None
 */
static void csr_update_btm_offload_config(tpAniSirGlobal mac_ctx,
					  uint8_t command,
					  tSirRoamOffloadScanReq *req_buf,
					  struct csr_roam_session *session)
{
	struct wlan_objmgr_peer *peer;
	bool is_pmf_enabled;

	req_buf->btm_offload_config =
			mac_ctx->roam.configParam.btm_offload_config;

	/* Return if INI is disabled */
	if (!req_buf->btm_offload_config)
		return;

	/* For RSO Stop Disable BTM offload to firmware */
	if (command == ROAM_SCAN_OFFLOAD_STOP) {
		req_buf->btm_offload_config = 0;
		return;
	}

	if (!session->pConnectBssDesc) {
		sme_err("Connected Bss Desc is NULL");
		return;
	}

	peer = wlan_objmgr_get_peer(mac_ctx->psoc,
				    wlan_objmgr_pdev_get_pdev_id(mac_ctx->pdev),
				    session->pConnectBssDesc->bssId,
				    WLAN_LEGACY_SME_ID);
	if (!peer) {
		sme_debug("Peer of peer_mac %pM not found",
			  session->pConnectBssDesc->bssId);
		return;
	}

	is_pmf_enabled = mlme_get_peer_pmf_status(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_SME_ID);
	sme_debug("get is_pmf_enabled %d for %pM", is_pmf_enabled,
		  session->pConnectBssDesc->bssId);

	/* If peer does not support PMF in case of OCE/MBO
	 * Connection, Disable BTM offload to firmware.
	 */
	if (session->pConnectBssDesc->mbo_oce_enabled_ap &&
	    !is_pmf_enabled)
		req_buf->btm_offload_config = 0;
}

/**
 * csr_create_roam_scan_offload_request() - init roam offload scan request
 *
 * parameters
 * @mac_ctx:      global mac ctx
 * @command:      roam scan offload command input
 * @session_id:   session id
 * @reason:       reason to roam
 * @session:      roam session
 * @roam_info:    roam info struct
 *
 * Return: roam offload scan request packet buffer
 */
static tSirRoamOffloadScanReq *
csr_create_roam_scan_offload_request(tpAniSirGlobal mac_ctx,
				     uint8_t command,
				     uint8_t session_id,
				     uint8_t reason,
				     struct csr_roam_session *session,
				     tpCsrNeighborRoamControlInfo roam_info)
{
	QDF_STATUS status;
	uint8_t i, j, dot11_mode;
	bool ese_neighbor_list_recvd = false;
	uint8_t ch_cache_str[128] = { 0 };
	tSirRoamOffloadScanReq *req_buf = NULL;
	tpCsrChannelInfo curr_ch_lst_info =
		&roam_info->roamChannelInfo.currentChannelListInfo;
	tCsrChannelInfo *specific_chan_info;

	specific_chan_info = &roam_info->cfgParams.specific_chan_info;
#ifdef FEATURE_WLAN_ESE
	/*
	 * this flag will be true if connection is ESE and no neighbor
	 * list received or if the connection is not ESE
	 */
	ese_neighbor_list_recvd = ((roam_info->isESEAssoc)
		&& (roam_info->roamChannelInfo.IAPPNeighborListReceived
		    == false))
		|| (roam_info->isESEAssoc == false);
#endif /* FEATURE_WLAN_ESE */

	req_buf = qdf_mem_malloc(sizeof(tSirRoamOffloadScanReq));
	if (NULL == req_buf) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "Mem alloc for roam scan offload req failed");
		return NULL;
	}
	req_buf->Command = command;
	/*
	 * If command is STOP, then pass down ScanOffloadEnabled as Zero. This
	 * will handle the case of host driver reloads, but Riva still up and
	 * running
	 */
	if (command == ROAM_SCAN_OFFLOAD_STOP) {
		/*
		 * clear the roaming parameters that are per connection.
		 * For a new connection, they have to be programmed again.
		 */
		if (csr_neighbor_middle_of_roaming(mac_ctx,
						   session_id))
			req_buf->middle_of_roaming = 1;
		else
			csr_roam_reset_roam_params(mac_ctx);
		req_buf->RoamScanOffloadEnabled = 0;
	} else if (command == ROAM_SCAN_OFFLOAD_UPDATE_CFG) {
		req_buf->RoamScanOffloadEnabled =
			roam_info->b_roam_scan_offload_started;
	} else {
		req_buf->RoamScanOffloadEnabled =
			mac_ctx->roam.configParam.isRoamOffloadScanEnabled;
	}
	qdf_mem_copy(req_buf->ConnectedNetwork.currAPbssid,
		     roam_info->currAPbssid.bytes, sizeof(struct qdf_mac_addr));
	req_buf->ConnectedNetwork.ssId.length =
		mac_ctx->roam.roamSession[session_id].
		connectedProfile.SSID.length;
	qdf_mem_copy(req_buf->ConnectedNetwork.ssId.ssId,
		mac_ctx->roam.roamSession[session_id].
		connectedProfile.SSID.ssId,
		req_buf->ConnectedNetwork.ssId.length);
	req_buf->ConnectedNetwork.authentication =
		mac_ctx->roam.roamSession[session_id].connectedProfile.AuthType;
	req_buf->ConnectedNetwork.encryption =
		mac_ctx->roam.roamSession[session_id].
		connectedProfile.EncryptionType;
	req_buf->ConnectedNetwork.mcencryption =
		mac_ctx->roam.roamSession[session_id].
		connectedProfile.mcEncryptionType;
	/* Copy the RSN capabilities in roam offload request from session*/
	req_buf->rsn_caps = session->rsn_caps;

	csr_rso_command_fill_11w_params(mac_ctx, session_id, req_buf);

	req_buf->delay_before_vdev_stop =
		roam_info->cfgParams.delay_before_vdev_stop;
	req_buf->OpportunisticScanThresholdDiff =
		roam_info->cfgParams.nOpportunisticThresholdDiff;
	req_buf->RoamRescanRssiDiff =
		roam_info->cfgParams.nRoamRescanRssiDiff;
	req_buf->RoamRssiDiff = roam_info->cfgParams.roam_rssi_diff;
	req_buf->rssi_abs_thresh = mac_ctx->roam.configParam.rssi_abs_thresh;
	req_buf->reason = reason;
	req_buf->NeighborScanTimerPeriod =
		roam_info->cfgParams.neighborScanPeriod;
	req_buf->neighbor_scan_min_timer_period =
		roam_info->cfgParams.neighbor_scan_min_period;
	req_buf->NeighborRoamScanRefreshPeriod =
		roam_info->cfgParams.neighborResultsRefreshPeriod;
	req_buf->NeighborScanChannelMinTime =
		roam_info->cfgParams.minChannelScanTime;
	req_buf->NeighborScanChannelMaxTime =
		roam_info->cfgParams.maxChannelScanTime;
	req_buf->EmptyRefreshScanPeriod =
		roam_info->cfgParams.emptyScanRefreshPeriod;
	req_buf->roam_scan_inactivity_time =
		roam_info->cfgParams.roam_scan_inactivity_time;
	req_buf->roam_inactive_data_packet_count =
		roam_info->cfgParams.roam_inactive_data_packet_count;
	req_buf->roam_scan_period_after_inactivity =
		roam_info->cfgParams.roam_scan_period_after_inactivity;
	req_buf->full_roam_scan_period =
		roam_info->cfgParams.full_roam_scan_period;

	req_buf->RoamBmissFirstBcnt =
		roam_info->cfgParams.nRoamBmissFirstBcnt;
	req_buf->RoamBmissFinalBcnt =
		roam_info->cfgParams.nRoamBmissFinalBcnt;
	req_buf->RoamBeaconRssiWeight =
		roam_info->cfgParams.nRoamBeaconRssiWeight;
	qdf_mem_copy(&req_buf->mawc_roam_params,
		&mac_ctx->roam.configParam.csr_mawc_config,
		sizeof(req_buf->mawc_roam_params));
#ifdef FEATURE_WLAN_ESE
	req_buf->IsESEAssoc =
		csr_roam_is_ese_assoc(mac_ctx, session_id) &&
		((req_buf->ConnectedNetwork.authentication ==
			eCSR_AUTH_TYPE_OPEN_SYSTEM)  ||
		(csr_is_auth_type_ese(req_buf->
			ConnectedNetwork.authentication)));
	req_buf->is_11r_assoc = roam_info->is11rAssoc;

	if (req_buf->IsESEAssoc || req_buf->is_11r_assoc ||
	    req_buf->middle_of_roaming)
		sme_debug("IsEseAssoc: %d is_11r_assoc: %d middle of roaming: %d ese_neighbor_list_recvd: %d cur no of chan: %d",
			  req_buf->IsESEAssoc, req_buf->is_11r_assoc,
			  req_buf->middle_of_roaming, ese_neighbor_list_recvd,
			  curr_ch_lst_info->numOfChannels);
#endif

	if (!CSR_IS_ROAM_INTRA_BAND_ENABLED(mac_ctx)) {
		if (ese_neighbor_list_recvd ||
		    curr_ch_lst_info->numOfChannels == 0) {
			/*
			 * Retrieve the Channel Cache either from ini or from
			 * the occupied channels list along with preferred
			 * channel list configured by the client.
			 * Give Preference to INI Channels
			 */
			if (specific_chan_info->numOfChannels) {
				status = csr_fetch_ch_lst_from_ini(mac_ctx,
								   roam_info,
								   req_buf);
				if (!QDF_IS_STATUS_SUCCESS(status)) {
					QDF_TRACE(QDF_MODULE_ID_SME,
						  QDF_TRACE_LEVEL_DEBUG,
						  "Fetch channel list from ini failed");
					qdf_mem_free(req_buf);
					return NULL;
				}
			} else if (reason == REASON_FLUSH_CHANNEL_LIST) {
				req_buf->ChannelCacheType = CHANNEL_LIST_STATIC;
				req_buf->ConnectedNetwork.ChannelCount = 0;
			} else {
				csr_fetch_ch_lst_from_occupied_lst(mac_ctx,
						session_id, reason, req_buf,
						roam_info);
				/* Add the preferred channel list configured by
				 * client to the roam channel list along with
				 * occupied channel list.
				 */
				csr_add_ch_lst_from_roam_scan_list(mac_ctx,
								   req_buf,
								   roam_info);
			}
		}
#ifdef FEATURE_WLAN_ESE
		else {
			/*
			 * If ESE is enabled, and a neighbor Report is received,
			 * then Ignore the INI Channels or the Occupied Channel
			 * List. Consider the channels in the neighbor list sent
			 * by the ESE AP
			 */
			csr_fetch_ch_lst_from_received_list(mac_ctx, roam_info,
					curr_ch_lst_info, req_buf);
		}
#endif
	}
	if (req_buf->ConnectedNetwork.ChannelCount == 0 &&
	    reason != REASON_FLUSH_CHANNEL_LIST) {
		/* Maintain the Valid Channels List */
		status = csr_fetch_valid_ch_lst(mac_ctx, req_buf, session_id);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Fetch channel list fail");
			qdf_mem_free(req_buf);
			return NULL;
		}
	}

	for (i = 0, j = 0; i < req_buf->ConnectedNetwork.ChannelCount; i++) {
		if (j < sizeof(ch_cache_str)) {
			j += snprintf(ch_cache_str + j,
				      sizeof(ch_cache_str) - j, " %d",
				      req_buf->ConnectedNetwork.
				      ChannelCache[i]);
		} else
			break;
	}
	sme_debug("ChnlCacheType:%d, No of Chnls:%d,Channels: %s",
		  req_buf->ChannelCacheType,
		  req_buf->ConnectedNetwork.ChannelCount, ch_cache_str);

	req_buf->MDID.mdiePresent =
		mac_ctx->roam.roamSession[session_id].
		connectedProfile.MDID.mdiePresent;
	req_buf->MDID.mobilityDomain =
		mac_ctx->roam.roamSession[session_id].
		connectedProfile.MDID.mobilityDomain;
	req_buf->sessionId = session_id;
	req_buf->nProbes = roam_info->cfgParams.roam_scan_n_probes;
	req_buf->HomeAwayTime = roam_info->cfgParams.roam_scan_home_away_time;

	/*
	 * Home Away Time should be at least equal to (MaxDwell time + (2*RFS)),
	 * where RFS is the RF Switching time. It is twice RFS to consider the
	 * time to go off channel and return to the home channel.
	 */
	if (req_buf->HomeAwayTime <
	    (req_buf->NeighborScanChannelMaxTime +
	     (2 * CSR_ROAM_SCAN_CHANNEL_SWITCH_TIME))) {
		sme_debug("Disable Home away time(%d) as it is less than (2*RF switching time + channel max time)(%d)",
			  req_buf->HomeAwayTime,
			  (req_buf->NeighborScanChannelMaxTime +
			   (2 * CSR_ROAM_SCAN_CHANNEL_SWITCH_TIME)));
		req_buf->HomeAwayTime = 0;
	}

	/*Prepare a probe request for 2.4GHz band and one for 5GHz band */
	dot11_mode = (uint8_t) csr_translate_to_wni_cfg_dot11_mode(mac_ctx,
				csr_find_best_phy_mode(mac_ctx,
					mac_ctx->roam.configParam.phyMode));
	req_buf->allowDFSChannelRoam =
	mac_ctx->roam.configParam.allowDFSChannelRoam;
	req_buf->early_stop_scan_enable =
		mac_ctx->roam.configParam.early_stop_scan_enable;
	req_buf->early_stop_scan_min_threshold =
		mac_ctx->roam.configParam.early_stop_scan_min_threshold;
	req_buf->early_stop_scan_max_threshold =
		mac_ctx->roam.configParam.early_stop_scan_max_threshold;
	req_buf->roamscan_adaptive_dwell_mode =
		mac_ctx->roam.configParam.roamscan_adaptive_dwell_mode;
	req_buf->lca_config_params.disallow_duration =
		mac_ctx->roam.configParam.disallow_duration;
	req_buf->lca_config_params.rssi_channel_penalization =
		mac_ctx->roam.configParam.rssi_channel_penalization;
	req_buf->lca_config_params.num_disallowed_aps =
		mac_ctx->roam.configParam.num_disallowed_aps;

	csr_update_btm_offload_config(mac_ctx, command, req_buf, session);

	req_buf->btm_solicited_timeout =
		mac_ctx->roam.configParam.btm_solicited_timeout;
	req_buf->btm_max_attempt_cnt =
		mac_ctx->roam.configParam.btm_max_attempt_cnt;
	req_buf->btm_sticky_time =
		mac_ctx->roam.configParam.btm_sticky_time;
	req_buf->btm_query_bitmask =
		mac_ctx->roam.configParam.btm_query_bitmask;
	req_buf->btm_trig_min_candidate_score =
		mac_ctx->roam.configParam.btm_trig_min_candidate_score;

	req_buf->rct_validity_timer =
			mac_ctx->roam.configParam.btm_validity_timer;
	req_buf->disassoc_timer_threshold =
			mac_ctx->roam.configParam.btm_disassoc_timer_threshold;

	csr_update_roam_scan_offload_request(mac_ctx, req_buf, session);
	return req_buf;
}

/**
 * csr_update_11k_offload_params - Update 11K offload params
 * @mac_ctx: MAC context
 * @session: Pointer to the CSR Roam Session
 * @req_buffer: Pointer to the RSO Request buffer
 * @enabled: 11k offload enabled/disabled.
 *
 * API to update 11k offload params to Roam Scan Offload request buffer
 *
 * Return: none
 */
static void csr_update_11k_offload_params(tpAniSirGlobal mac_ctx,
					  struct csr_roam_session *session,
					  tSirRoamOffloadScanReq *req_buffer,
					  bool enabled)
{
	struct wmi_11k_offload_params *params = &req_buffer->offload_11k_params;
	struct csr_config *csr_config = &mac_ctx->roam.configParam;
	struct csr_neighbor_report_offload_params *neighbor_report_offload =
		&csr_config->neighbor_report_offload;

	params->vdev_id = session->sessionId;

	if (enabled) {
		params->offload_11k_bitmask =
				csr_config->offload_11k_enable_bitmask;
	} else {
		params->offload_11k_bitmask = 0;
		return;
	}

	/*
	 * If none of the parameters are enabled, then set the
	 * offload_11k_bitmask to 0, so that we don't send the command
	 * to the FW and drop it in WMA
	 */
	if ((neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_ALL) == 0) {
		sme_err("No valid neighbor report offload params %x",
			neighbor_report_offload->params_bitmask);
		params->offload_11k_bitmask = 0;
		return;
	}

	/*
	 * First initialize all params to NEIGHBOR_REPORT_PARAM_INVALID
	 * Then set the values that are enabled
	 */
	params->neighbor_report_params.time_offset =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.low_rssi_offset =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.bmiss_count_trigger =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.per_threshold_offset =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.neighbor_report_cache_timeout =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.max_neighbor_report_req_cap =
		NEIGHBOR_REPORT_PARAM_INVALID;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_TIME_OFFSET)
		params->neighbor_report_params.time_offset =
			neighbor_report_offload->time_offset;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_LOW_RSSI_OFFSET)
		params->neighbor_report_params.low_rssi_offset =
			neighbor_report_offload->low_rssi_offset;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_BMISS_COUNT_TRIGGER)
		params->neighbor_report_params.bmiss_count_trigger =
			neighbor_report_offload->bmiss_count_trigger;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_PER_THRESHOLD_OFFSET)
		params->neighbor_report_params.per_threshold_offset =
			neighbor_report_offload->per_threshold_offset;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_CACHE_TIMEOUT)
		params->neighbor_report_params.neighbor_report_cache_timeout =
			neighbor_report_offload->neighbor_report_cache_timeout;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_MAX_REQ_CAP)
		params->neighbor_report_params.max_neighbor_report_req_cap =
			neighbor_report_offload->max_neighbor_report_req_cap;

	params->neighbor_report_params.ssid.length =
		session->connectedProfile.SSID.length;
	qdf_mem_copy(params->neighbor_report_params.ssid.mac_ssid,
			session->connectedProfile.SSID.ssId,
			session->connectedProfile.SSID.length);
}

QDF_STATUS csr_invoke_neighbor_report_request(uint8_t session_id,
				struct sRrmNeighborReq *neighbor_report_req,
				bool send_resp_to_host)
{
	struct wmi_invoke_neighbor_report_params *invoke_params;
	struct scheduler_msg msg = {0};

	if (!neighbor_report_req) {
		sme_err("Invalid params");
		return QDF_STATUS_E_INVAL;
	}

	invoke_params = qdf_mem_malloc(sizeof(*invoke_params));
	if (!invoke_params) {
		sme_err("Memory allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	invoke_params->vdev_id = session_id;
	invoke_params->send_resp_to_host = send_resp_to_host;

	if (!neighbor_report_req->no_ssid) {
		invoke_params->ssid.length = neighbor_report_req->ssid.length;
		qdf_mem_copy(invoke_params->ssid.mac_ssid,
				neighbor_report_req->ssid.ssId,
				neighbor_report_req->ssid.length);
	} else {
		invoke_params->ssid.length = 0;
	}

	sme_debug("Sending SIR_HAL_INVOKE_NEIGHBOR_REPORT");

	msg.type = SIR_HAL_INVOKE_NEIGHBOR_REPORT;
	msg.reserved = 0;
	msg.bodyptr = invoke_params;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		sme_err("Not able to post message to WMA");
		qdf_mem_free(invoke_params);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * check_allowed_ssid_list() - Check the WhiteList
 * @req_buffer:      Buffer which contains the connected profile SSID.
 * @roam_params:     Buffer which contains the whitelist SSID's.
 *
 * Check if the connected profile SSID exists in the whitelist.
 * It is assumed that the framework provides this also in the whitelist.
 * If it exists there is no issue. Otherwise add it to the list.
 *
 * Return: None
 */
static void check_allowed_ssid_list(tSirRoamOffloadScanReq *req_buffer,
		struct roam_ext_params *roam_params)
{
	int i = 0;
	bool match = false;

	for (i = 0; i < roam_params->num_ssid_allowed_list; i++) {
		if ((roam_params->ssid_allowed_list[i].length ==
			req_buffer->ConnectedNetwork.ssId.length) &&
			(!qdf_mem_cmp(roam_params->ssid_allowed_list[i].ssId,
				req_buffer->ConnectedNetwork.ssId.ssId,
				roam_params->ssid_allowed_list[i].length))) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				"Whitelist contains connected profile SSID");
			match = true;
			break;
		}
	}
	if (!match) {
		if (roam_params->num_ssid_allowed_list >=
				MAX_SSID_ALLOWED_LIST) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				"Whitelist is FULL. Cannot Add another entry");
			return;
		}
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				"Adding Connected profile SSID to whitelist");
		/* i is the next available index to add the entry.*/
		i = roam_params->num_ssid_allowed_list;
		qdf_mem_copy(roam_params->ssid_allowed_list[i].ssId,
				req_buffer->ConnectedNetwork.ssId.ssId,
				req_buffer->ConnectedNetwork.ssId.length);
		roam_params->ssid_allowed_list[i].length =
			req_buffer->ConnectedNetwork.ssId.length;
		roam_params->num_ssid_allowed_list++;
	}
}

/**
 * csr_add_rssi_reject_ap_list() - add rssi reject AP list to the
 * roam params
 * @mac_ctx: mac ctx.
 * @roam_params: roam params in which reject AP list needs
 * to be populated.
 *
 * Return: None
 */
static void csr_add_rssi_reject_ap_list(tpAniSirGlobal mac_ctx,
	struct roam_ext_params *roam_params)
{
	int i = 0;
	struct sir_rssi_disallow_lst *cur_node;
	qdf_list_node_t *cur_list = NULL;
	qdf_list_node_t *next_list = NULL;
	struct rssi_disallow_bssid *rssi_rejection_ap;
	qdf_list_t *list = &mac_ctx->roam.rssi_disallow_bssid;
	qdf_time_t cur_time =
		qdf_do_div(qdf_get_monotonic_boottime(),
		QDF_MC_TIMER_TO_MS_UNIT);

	roam_params->num_rssi_rejection_ap = qdf_list_size(list);

	if (!qdf_list_size(list))
		return;

	if (roam_params->num_rssi_rejection_ap > MAX_RSSI_AVOID_BSSID_LIST)
		roam_params->num_rssi_rejection_ap = MAX_RSSI_AVOID_BSSID_LIST;

	qdf_mutex_acquire(&mac_ctx->roam.rssi_disallow_bssid_lock);
	qdf_list_peek_front(list, &cur_list);
	while (cur_list) {
		int32_t rem_time;

		rssi_rejection_ap = &roam_params->rssi_rejection_ap[i];
		cur_node = qdf_container_of(cur_list,
				struct sir_rssi_disallow_lst, node);
		rem_time = cur_node->retry_delay -
			(cur_time - cur_node->time_during_rejection);

		if (rem_time > 0) {
			qdf_copy_macaddr(&rssi_rejection_ap->bssid,
					&cur_node->bssid);
			rssi_rejection_ap->expected_rssi =
					cur_node->expected_rssi;
			rssi_rejection_ap->remaining_duration = rem_time;
			i++;
		}
		qdf_list_peek_next(list, cur_list, &next_list);
		cur_list = next_list;
		next_list = NULL;

		if (i >= MAX_RSSI_AVOID_BSSID_LIST)
			break;
	}
	qdf_mutex_release(&mac_ctx->roam.rssi_disallow_bssid_lock);

	for (i = 0; i < roam_params->num_rssi_rejection_ap; i++) {
		sme_debug("BSSID %pM expected rssi %d remaining duration %d",
			roam_params->rssi_rejection_ap[i].bssid.bytes,
			roam_params->rssi_rejection_ap[i].expected_rssi,
			roam_params->rssi_rejection_ap[i].remaining_duration);
	}
}

/*
 * Below Table describe whether RSO command can be send down to fimrware or not.
 * Host check it on the basis of previous RSO command sent down to firmware.
 * ||=========================================================================||
 * || New cmd        |            LAST SENT COMMAND --->                      ||
 * ||====|====================================================================||
 * ||    V           | START | STOP | RESTART | UPDATE_CFG| ABORT_SCAN        ||
 * || ------------------------------------------------------------------------||
 * || RSO_START      | NO    | YES  |  NO     | YES       | NO                ||
 * || RSO_STOP       | YES   | YES  |  YES    | YES       | YES               ||
 * || RSO_RESTART    | YES   | YES  |  NO     | YES       | YES               ||
 * || RSO_UPDATE_CFG | YES   | NO   |  YES    | YES       | YES               ||
 * || RSO_ABORT_SCAN | YES   | NO   |  YES    | YES       | YES               ||
 * ||=========================================================================||
 **/
#define RSO_START_BIT       (1<<ROAM_SCAN_OFFLOAD_START)
#define RSO_STOP_BIT        (1<<ROAM_SCAN_OFFLOAD_STOP)
#define RSO_RESTART_BIT     (1<<ROAM_SCAN_OFFLOAD_RESTART)
#define RSO_UPDATE_CFG_BIT  (1<<ROAM_SCAN_OFFLOAD_UPDATE_CFG)
#define RSO_ABORT_SCAN_BIT  (1<<ROAM_SCAN_OFFLOAD_ABORT_SCAN)
#define RSO_START_ALLOW_MASK   (RSO_START_BIT |RSO_STOP_BIT | RSO_UPDATE_CFG_BIT)
#define RSO_STOP_ALLOW_MASK    (RSO_UPDATE_CFG_BIT | RSO_RESTART_BIT | \
		RSO_STOP_BIT | RSO_START_BIT | RSO_ABORT_SCAN_BIT)
#define RSO_RESTART_ALLOW_MASK (RSO_UPDATE_CFG_BIT | RSO_START_BIT | \
		RSO_ABORT_SCAN_BIT | RSO_RESTART_BIT)
#define RSO_UPDATE_CFG_ALLOW_MASK  (RSO_UPDATE_CFG_BIT | RSO_STOP_BIT | \
		RSO_START_BIT | RSO_ABORT_SCAN_BIT)
#define RSO_ABORT_SCAN_ALLOW_MASK (RSO_START_BIT | RSO_RESTART_BIT | \
		RSO_UPDATE_CFG_BIT | RSO_ABORT_SCAN_BIT)

static bool csr_is_RSO_cmd_allowed(tpAniSirGlobal mac_ctx,
	uint8_t command, uint8_t session_id)
{
	tpCsrNeighborRoamControlInfo neigh_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	uint8_t desiredMask = 0;
	bool ret_val;

	switch (command) {
	case ROAM_SCAN_OFFLOAD_START:
		desiredMask = RSO_START_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_STOP:
		desiredMask = RSO_STOP_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_RESTART:
		desiredMask = RSO_RESTART_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_UPDATE_CFG:
		desiredMask = RSO_UPDATE_CFG_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_ABORT_SCAN:
		desiredMask = RSO_ABORT_SCAN_ALLOW_MASK;
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			("Wrong RSO command %d, not allowed"), command);
		return 0;/*Cmd Not allowed*/
	}
	ret_val = desiredMask & (1 << neigh_roam_info->last_sent_cmd);
	return ret_val;
}

/**
 * csr_append_assoc_ies() - Append specific IE to assoc IE's buffer
 * @mac_ctx: Pointer to global mac context
 * @req_buf: Pointer to Roam offload scan request
 * @ie_id: IE ID to be appended
 * @ie_len: IE length to be appended
 * @ie_data: IE data to be appended
 *
 * Return: None
 */
static void csr_append_assoc_ies(tpAniSirGlobal mac_ctx,
				tSirRoamOffloadScanReq *req_buf, uint8_t ie_id,
				uint8_t ie_len, uint8_t *ie_data)
{
	tSirAddie *assoc_ie = &req_buf->assoc_ie;

	if ((SIR_MAC_MAX_ADD_IE_LENGTH - assoc_ie->length) < ie_len) {
		sme_err("Appending IE id: %d fails", ie_id);
		return;
	}
	assoc_ie->addIEdata[assoc_ie->length] = ie_id;
	assoc_ie->addIEdata[assoc_ie->length + 1] = ie_len;
	qdf_mem_copy(&assoc_ie->addIEdata[assoc_ie->length + 2],
						ie_data, ie_len);
	assoc_ie->length += (ie_len + 2);
}

#ifdef FEATURE_WLAN_ESE
/**
 * ese_populate_addtional_ies() - add IEs to reassoc frame
 * @mac_ctx: Pointer to global mac structure
 * @session: pointer to CSR session
 * @req_buf: Pointer to Roam offload scan request
 *
 * This function populates the TSPEC ie and appends the info
 * to assoc buffer.
 *
 * Return: None
 */
static void ese_populate_addtional_ies(tpAniSirGlobal mac_ctx,
				struct csr_roam_session *session,
				tSirRoamOffloadScanReq *req_buf) {

	uint8_t tspec_ie_hdr[SIR_MAC_OUI_WME_HDR_MIN]
			= { 0x00, 0x50, 0xf2, 0x02, 0x02, 0x01 };
	uint8_t tspec_ie_buf[DOT11F_IE_WMMTSPEC_MAX_LEN], j;
	ese_wmm_tspec_ie *tspec_ie;
	tESETspecInfo ese_tspec;

	tspec_ie = (ese_wmm_tspec_ie *)(tspec_ie_buf + SIR_MAC_OUI_WME_HDR_MIN);
	if (csr_is_wmm_supported(mac_ctx) &&
		mac_ctx->roam.configParam.isEseIniFeatureEnabled &&
		csr_roam_is_ese_assoc(mac_ctx, session->sessionId)) {
		ese_tspec.numTspecs = sme_qos_ese_retrieve_tspec_info(mac_ctx,
					session->sessionId,
					(tTspecInfo *) &ese_tspec.tspec[0]);
		qdf_mem_copy(tspec_ie_buf, tspec_ie_hdr,
			SIR_MAC_OUI_WME_HDR_MIN);
		for (j = 0; j < ese_tspec.numTspecs; j++) {
			/* Populate the tspec_ie */
			ese_populate_wmm_tspec(&ese_tspec.tspec[j].tspec,
				tspec_ie);
			csr_append_assoc_ies(mac_ctx, req_buf,
					IEEE80211_ELEMID_VENDOR,
					DOT11F_IE_WMMTSPEC_MAX_LEN,
					tspec_ie_buf);
		}
	}

}
#else
static inline void ese_populate_addtional_ies(
	tpAniSirGlobal mac_ctx,
	struct csr_roam_session *session, tSirRoamOffloadScanReq *req_buf) {
}
#endif
/**
 * csr_update_driver_assoc_ies() - Append driver built IE's to assoc IE's
 * @mac_ctx: Pointer to global mac structure
 * @session: pointer to CSR session
 * @req_buf: Pointer to Roam offload scan request
 *
 * Return: None
 */
static void csr_update_driver_assoc_ies(tpAniSirGlobal mac_ctx,
					struct csr_roam_session *session,
					tSirRoamOffloadScanReq *req_buf)
{
	bool power_caps_populated = false;
	uint32_t csr_11henable = WNI_CFG_11H_ENABLED_STADEF;

	uint8_t *rrm_cap_ie_data
			= (uint8_t *) &mac_ctx->rrm.rrmPEContext.rrmEnabledCaps;
	uint8_t power_cap_ie_data[DOT11F_IE_POWERCAPS_MAX_LEN]
			= {MIN_TX_PWR_CAP, MAX_TX_PWR_CAP};
	uint8_t max_tx_pwr_cap = 0;
	uint8_t supp_chan_ie[DOT11F_IE_SUPPCHANNELS_MAX_LEN], supp_chan_ie_len;

#ifdef FEATURE_WLAN_ESE
	uint8_t ese_ie[DOT11F_IE_ESEVERSION_MAX_LEN]
			= { 0x0, 0x40, 0x96, 0x3, ESE_VERSION_SUPPORTED};
#endif
	uint8_t qcn_ie[DOT11F_IE_QCN_IE_MAX_LEN]
			= {0x8C, 0xFD, 0xF0, 0x1, QCN_IE_VERSION_SUBATTR_ID,
				QCN_IE_VERSION_SUBATTR_DATA_LEN,
				QCN_IE_VERSION_SUPPORTED,
				QCN_IE_SUBVERSION_SUPPORTED};

	if (session->pConnectBssDesc)
		max_tx_pwr_cap = csr_get_cfg_max_tx_power(mac_ctx,
				session->pConnectBssDesc->channelId);

	if (max_tx_pwr_cap && max_tx_pwr_cap < MAX_TX_PWR_CAP)
		power_cap_ie_data[1] = max_tx_pwr_cap;
	else
		power_cap_ie_data[1] = MAX_TX_PWR_CAP;

	wlan_cfg_get_int(mac_ctx, WNI_CFG_11H_ENABLED, &csr_11henable);

	if (csr_11henable && csr_is11h_supported(mac_ctx)) {
		/* Append power cap IE */
		csr_append_assoc_ies(mac_ctx, req_buf, IEEE80211_ELEMID_PWRCAP,
					DOT11F_IE_POWERCAPS_MAX_LEN,
					power_cap_ie_data);
		power_caps_populated = true;

		/* Append Supported channels IE */
		csr_add_supported_5Ghz_channels(mac_ctx, supp_chan_ie,
					&supp_chan_ie_len, true);

		csr_append_assoc_ies(mac_ctx, req_buf,
					IEEE80211_ELEMID_SUPPCHAN,
					supp_chan_ie_len, supp_chan_ie);
	}

#ifdef FEATURE_WLAN_ESE
	/* Append ESE version IE if isEseIniFeatureEnabled INI is enabled */
	if (mac_ctx->roam.configParam.isEseIniFeatureEnabled)
		csr_append_assoc_ies(mac_ctx, req_buf, IEEE80211_ELEMID_VENDOR,
					DOT11F_IE_ESEVERSION_MAX_LEN,
					ese_ie);
#endif

	if (mac_ctx->rrm.rrmPEContext.rrmEnable) {
		/* Append RRM IE */
		csr_append_assoc_ies(mac_ctx, req_buf, IEEE80211_ELEMID_RRM,
					DOT11F_IE_RRMENABLEDCAP_MAX_LEN,
					rrm_cap_ie_data);
		if (!power_caps_populated)
			/* Append Power cap IE if not appended already */
			csr_append_assoc_ies(mac_ctx, req_buf,
					IEEE80211_ELEMID_PWRCAP,
					DOT11F_IE_POWERCAPS_MAX_LEN,
					power_cap_ie_data);
	}
	ese_populate_addtional_ies(mac_ctx, session, req_buf);

	/* Append QCN IE if g_support_qcn_ie INI is enabled */
	if (mac_ctx->roam.configParam.qcn_ie_support)
		csr_append_assoc_ies(mac_ctx, req_buf, IEEE80211_ELEMID_VENDOR,
					DOT11F_IE_QCN_IE_MAX_LEN,
					qcn_ie);
}

/**
 * csr_create_per_roam_request() - create PER roam offload scan request
 *
 * parameters
 * @mac_ctx: global mac ctx
 * @session_id: session id
 *
 * Return: per roam config request packet buffer
 */
static struct wmi_per_roam_config_req *
csr_create_per_roam_request(tpAniSirGlobal mac_ctx,
		uint8_t session_id)
{
	struct wmi_per_roam_config_req *req_buf = NULL;

	req_buf = qdf_mem_malloc(sizeof(struct wmi_per_roam_config_req));
	if (!req_buf) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "Mem alloc for per roam req failed");
		return NULL;
	}
	req_buf->vdev_id = session_id;
	req_buf->per_config.enable =
		mac_ctx->roam.configParam.per_roam_config.enable;
	req_buf->per_config.tx_high_rate_thresh =
		mac_ctx->roam.configParam.per_roam_config.tx_high_rate_thresh;
	req_buf->per_config.rx_high_rate_thresh =
		mac_ctx->roam.configParam.per_roam_config.rx_high_rate_thresh;
	req_buf->per_config.tx_low_rate_thresh =
		mac_ctx->roam.configParam.per_roam_config.tx_low_rate_thresh;
	req_buf->per_config.rx_low_rate_thresh =
		mac_ctx->roam.configParam.per_roam_config.rx_low_rate_thresh;
	req_buf->per_config.per_rest_time =
		mac_ctx->roam.configParam.per_roam_config.per_rest_time;
	req_buf->per_config.tx_per_mon_time =
		mac_ctx->roam.configParam.per_roam_config.tx_per_mon_time;
	req_buf->per_config.rx_per_mon_time =
		mac_ctx->roam.configParam.per_roam_config.rx_per_mon_time;
	req_buf->per_config.tx_rate_thresh_percnt =
		mac_ctx->roam.configParam.per_roam_config.tx_rate_thresh_percnt;
	req_buf->per_config.rx_rate_thresh_percnt =
		mac_ctx->roam.configParam.per_roam_config.rx_rate_thresh_percnt;
	req_buf->per_config.min_candidate_rssi =
		mac_ctx->roam.configParam.per_roam_config.min_candidate_rssi;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		"PER based roaming configuaration enable: %d vdev: %d high_rate_thresh: %d low_rate_thresh: %d rate_thresh_percnt: %d per_rest_time: %d monitor_time: %d min cand rssi: %d",
			  req_buf->per_config.enable, session_id,
			  req_buf->per_config.tx_high_rate_thresh,
			  req_buf->per_config.tx_low_rate_thresh,
			  req_buf->per_config.tx_rate_thresh_percnt,
			  req_buf->per_config.per_rest_time,
			  req_buf->per_config.tx_per_mon_time,
			  req_buf->per_config.min_candidate_rssi);
	return req_buf;
}

/**
 * csr_roam_offload_per_scan() - populates roam offload scan request and sends
 * to WMA
 *
 * parameters
 * @mac_ctx:      global mac ctx
 * @session_id:   session id
 *
 * Return: result of operation
 */
static QDF_STATUS
csr_roam_offload_per_scan(tpAniSirGlobal mac_ctx, uint8_t session_id)
{
	struct wmi_per_roam_config_req *req_buf;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	if (!mac_ctx->roam.configParam.per_roam_config.enable) {
		sme_debug("PER based roaming ini is disabled");
		return QDF_STATUS_SUCCESS;
	}

	req_buf = csr_create_per_roam_request(mac_ctx, session_id);
	if (!req_buf)
		return QDF_STATUS_E_FAILURE;

	msg.type = eWNI_SME_ROAM_SEND_PER_REQ;
	msg.reserved = 0;
	msg.bodyptr = req_buf;

	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE,
					&msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_debug("%s: Unable to post WMA_SET_PER_ROAM_CONFIG_CMD to WMA",
			  __func__);
		qdf_mem_free(req_buf);
	}
	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_FILS_SK)
QDF_STATUS csr_update_fils_config(tpAniSirGlobal mac, uint8_t session_id,
				  struct csr_roam_profile *src_profile)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac, session_id);
	struct csr_roam_profile *dst_profile = NULL;

	if (!session) {
		sme_err("session NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dst_profile = session->pCurRoamProfile;

	if (!dst_profile) {
		sme_err("Current Roam profile of SME session NULL");
		return QDF_STATUS_E_FAILURE;
	}
	update_profile_fils_info(dst_profile, src_profile);
	return QDF_STATUS_SUCCESS;
}

/**
 * copy_all_before_char() - API to copy all character before a particular char
 * @str: Source string
 * @str_len: Source string legnth
 * @dst: Destination string
 * @dst_len: Destination string legnth
 * @c: Character before which all characters need to be copied
 *
 * Return: length of the copied string, if success. zero otherwise.
 */
static uint32_t copy_all_before_char(char *str, uint32_t str_len,
				     char *dst, uint32_t dst_len, char c)
{
	uint32_t len = 0;

	if (!str)
		return len;

	while ((len < str_len) && (len < dst_len) &&
	       (*str != '\0') && (*str != c)) {
		*dst++ = *str++;
		len++;
	}

	return len;
}

/**
 * csr_update_fils_params_rso() - API to update FILS params in RSO
 * @mac: Mac context
 * @session: CSR Roam Session
 * @req_buffer: RSO request buffer
 *
 * Return: None
 */
static void csr_update_fils_params_rso(tpAniSirGlobal mac,
				       struct csr_roam_session *session,
				       tSirRoamOffloadScanReq *req_buffer)
{
	struct roam_fils_params *roam_fils_params;
	struct cds_fils_connection_info *fils_info;
	uint32_t usr_name_len;

	if (!session->pCurRoamProfile)
		return;

	fils_info = session->pCurRoamProfile->fils_con_info;
	if (!fils_info || !req_buffer)
		return;

	if (!fils_info->key_nai_length) {
		sme_debug("key_nai_length is NULL");
		return;
	}

	roam_fils_params = &req_buffer->roam_fils_params;
	if ((fils_info->key_nai_length > FILS_MAX_KEYNAME_NAI_LENGTH) ||
			(fils_info->r_rk_length > FILS_MAX_RRK_LENGTH)) {
		sme_err("Fils info len error: keyname nai len(%d) rrk len(%d)",
			fils_info->key_nai_length, fils_info->r_rk_length);
		return;
	}

	usr_name_len = copy_all_before_char(fils_info->keyname_nai,
					    sizeof(fils_info->keyname_nai),
					    roam_fils_params->username,
					    sizeof(roam_fils_params->username),
					    '@');
	if (fils_info->key_nai_length <= usr_name_len) {
		sme_err("Fils info len error: key nai len %d, user name len %d",
			fils_info->key_nai_length, usr_name_len);
		return;
	}

	roam_fils_params->username_length = usr_name_len;
	req_buffer->is_fils_connection = true;

	roam_fils_params->next_erp_seq_num = fils_info->sequence_number;

	roam_fils_params->rrk_length = fils_info->r_rk_length;
	qdf_mem_copy(roam_fils_params->rrk, fils_info->r_rk,
			roam_fils_params->rrk_length);

	/* REALM info */
	roam_fils_params->realm_len = fils_info->key_nai_length
			- roam_fils_params->username_length - 1;
	qdf_mem_copy(roam_fils_params->realm, fils_info->keyname_nai
			+ roam_fils_params->username_length + 1,
			roam_fils_params->realm_len);
	sme_debug("Fils: next_erp_seq_num %d rrk_len %d realm_len:%d",
		  roam_fils_params->next_erp_seq_num,
		  roam_fils_params->rrk_length, roam_fils_params->realm_len);
}
#else
static inline
void csr_update_fils_params_rso(tpAniSirGlobal mac,
				struct csr_roam_session *session,
				tSirRoamOffloadScanReq *req_buffer)
{}
#endif

/**
 * csr_update_score_params() - API to update Score params in RSO
 * @mac_ctx: Mac context
 * @req_buffer: RSO request buffer
 *
 * Return: None
 */
static void csr_update_score_params(tpAniSirGlobal mac_ctx,
				    tSirRoamOffloadScanReq *req_buffer,
				    tpCsrNeighborRoamControlInfo roam_info)
{
	struct scoring_param *req_score_params;
	struct rssi_scoring *req_rssi_score;
	struct sir_score_config *bss_score_params;
	struct sir_weight_config *weight_config;
	struct sir_rssi_cfg_score *rssi_score;

	req_score_params = &req_buffer->score_params;
	req_rssi_score = &req_score_params->rssi_scoring;

	bss_score_params = &mac_ctx->roam.configParam.bss_score_params;
	weight_config = &bss_score_params->weight_cfg;
	rssi_score = &bss_score_params->rssi_score;

	if (!roam_info->cfgParams.enable_scoring_for_roam)
		req_score_params->disable_bitmap =
			WLAN_ROAM_SCORING_DISABLE_ALL;

	req_score_params->rssi_weightage = weight_config->rssi_weightage;
	req_score_params->ht_weightage = weight_config->ht_caps_weightage;
	req_score_params->vht_weightage = weight_config->vht_caps_weightage;
	req_score_params->he_weightage = weight_config->he_caps_weightage;
	req_score_params->bw_weightage = weight_config->chan_width_weightage;
	req_score_params->band_weightage = weight_config->chan_band_weightage;
	req_score_params->nss_weightage = weight_config->nss_weightage;
	req_score_params->esp_qbss_weightage =
		weight_config->channel_congestion_weightage;
	req_score_params->beamforming_weightage =
		weight_config->beamforming_cap_weightage;
	req_score_params->pcl_weightage =
		weight_config->pcl_weightage;
	req_score_params->oce_wan_weightage = weight_config->oce_wan_weightage;
	req_score_params->vendor_roam_score_algorithm =
		weight_config->vendor_roam_score_algorithm;

	req_score_params->bw_index_score =
		bss_score_params->bandwidth_weight_per_index;
	req_score_params->band_index_score =
		bss_score_params->band_weight_per_index;
	req_score_params->nss_index_score =
		bss_score_params->nss_weight_per_index;
	req_score_params->roam_score_delta =
		bss_score_params->roam_score_delta;
	req_score_params->roam_trigger_bitmap =
		bss_score_params->roam_score_delta_bitmap;
	req_score_params->cand_min_roam_score_delta =
		bss_score_params->cand_min_roam_score_delta;

	req_rssi_score->best_rssi_threshold = rssi_score->best_rssi_threshold;
	req_rssi_score->good_rssi_threshold = rssi_score->good_rssi_threshold;
	req_rssi_score->bad_rssi_threshold = rssi_score->bad_rssi_threshold;
	req_rssi_score->good_rssi_pcnt = rssi_score->good_rssi_pcnt;
	req_rssi_score->bad_rssi_pcnt = rssi_score->bad_rssi_pcnt;
	req_rssi_score->good_bucket_size = rssi_score->good_rssi_bucket_size;
	req_rssi_score->bad_bucket_size = rssi_score->bad_rssi_bucket_size;
	req_rssi_score->rssi_pref_5g_rssi_thresh =
			rssi_score->rssi_pref_5g_rssi_thresh;

	req_score_params->esp_qbss_scoring.num_slot =
		bss_score_params->esp_qbss_scoring.num_slot;
	req_score_params->esp_qbss_scoring.score_pcnt3_to_0 =
		bss_score_params->esp_qbss_scoring.score_pcnt3_to_0;
	req_score_params->esp_qbss_scoring.score_pcnt7_to_4 =
		bss_score_params->esp_qbss_scoring.score_pcnt7_to_4;
	req_score_params->esp_qbss_scoring.score_pcnt11_to_8 =
		bss_score_params->esp_qbss_scoring.score_pcnt11_to_8;
	req_score_params->esp_qbss_scoring.score_pcnt15_to_12 =
		bss_score_params->esp_qbss_scoring.score_pcnt15_to_12;

	req_score_params->oce_wan_scoring.num_slot =
		bss_score_params->oce_wan_scoring.num_slot;
	req_score_params->oce_wan_scoring.score_pcnt3_to_0 =
		bss_score_params->oce_wan_scoring.score_pcnt3_to_0;
	req_score_params->oce_wan_scoring.score_pcnt7_to_4 =
		bss_score_params->oce_wan_scoring.score_pcnt7_to_4;
	req_score_params->oce_wan_scoring.score_pcnt11_to_8 =
		bss_score_params->oce_wan_scoring.score_pcnt11_to_8;
	req_score_params->oce_wan_scoring.score_pcnt15_to_12 =
		bss_score_params->oce_wan_scoring.score_pcnt15_to_12;
}

uint8_t csr_get_roam_enabled_sta_sessionid(tpAniSirGlobal mac_ctx)
{
	struct csr_roam_session *session;
	tpCsrNeighborRoamControlInfo roam_info;
	uint8_t i;

	for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
		session = CSR_GET_SESSION(mac_ctx, i);
		if (!session || !CSR_IS_SESSION_VALID(mac_ctx, i))
			continue;
		if (!session->pCurRoamProfile ||
		    session->pCurRoamProfile->csrPersona != QDF_STA_MODE)
			continue;
		roam_info = &mac_ctx->roam.neighborRoamInfo[i];
		if (roam_info->b_roam_scan_offload_started)
			return i;
	}

	return CSR_SESSION_ID_INVALID;
}

#ifdef WLAN_ADAPTIVE_11R
static bool
csr_is_adaptive_11r_roam_supported(tpAniSirGlobal mac_ctx,
				   struct csr_roam_session *session)
{
	if (session->is_adaptive_11r_connection)
		return mac_ctx->is_adaptive_11r_roam_supported;

	return true;
}
#else
static bool
csr_is_adaptive_11r_roam_supported(tpAniSirGlobal mac_ctx,
				   struct csr_roam_session *session)

{
	return true;
}
#endif

/**
 * csr_roam_offload_scan() - populates roam offload scan request and sends to
 * WMA
 *
 * parameters
 * @mac_ctx:      global mac ctx
 * @session_id:   session id
 * @command:      roam scan offload command input
 * @reason:       reason to roam
 *
 * Return: result of operation
 */
QDF_STATUS
csr_roam_offload_scan(tpAniSirGlobal mac_ctx, uint8_t session_id,
		      uint8_t command, uint8_t reason)
{
	uint8_t *state = NULL;
	tSirRoamOffloadScanReq *req_buf;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	tpCsrNeighborRoamControlInfo roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	struct roam_ext_params *roam_params_dst;
	struct roam_ext_params *roam_params_src;
	uint8_t temp_session_id;
	uint8_t op_channel, active_ndp_cnt = 0;
	bool prev_roaming_state;
	eCsrAuthType roam_profile_akm = eCSR_AUTH_TYPE_UNKNOWN;
	uint32_t fw_akm_bitmap;
	bool p2p_disable_sta_roaming = 0, nan_disable_sta_roaming = 0;

	sme_debug("RSO Command %d, vdev %d, Reason %d", command,
		   session_id, reason);
	if (NULL == session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "session is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (roam_info->neighborRoamState !=
	    eCSR_NEIGHBOR_ROAM_STATE_CONNECTED &&
	    (command == ROAM_SCAN_OFFLOAD_UPDATE_CFG ||
	     command == ROAM_SCAN_OFFLOAD_START ||
	     command == ROAM_SCAN_OFFLOAD_RESTART)) {
		sme_debug("Session not in connected state, RSO not sent and state=%d ",
			  roam_info->neighborRoamState);
		return QDF_STATUS_E_FAILURE;
	}

	temp_session_id = csr_get_roam_enabled_sta_sessionid(mac_ctx);
	if ((temp_session_id != CSR_SESSION_ID_INVALID) &&
	    (session_id != temp_session_id)) {
		sme_debug("Roam cmd not for session %d on which roaming is enabled",
			   temp_session_id);
		return QDF_STATUS_E_FAILURE;
	}

	if ((command == ROAM_SCAN_OFFLOAD_START ||
	    command == ROAM_SCAN_OFFLOAD_UPDATE_CFG) &&
	    (session->pCurRoamProfile &&
	    session->pCurRoamProfile->driver_disabled_roaming)) {
		if (reason == REASON_DRIVER_ENABLED) {
			session->pCurRoamProfile->driver_disabled_roaming =
									false;
			sme_debug("driver_disabled_roaming reset for session %d",
				  session_id);
		} else {
			sme_debug("Roam start received for session %d on which driver has disabled roaming",
				  session_id);
			return QDF_STATUS_E_FAILURE;
		}
	}

	if ((ROAM_SCAN_OFFLOAD_START == command &&
	    REASON_CTX_INIT != reason) &&
	    (session->pCurRoamProfile &&
	    session->pCurRoamProfile->supplicant_disabled_roaming)) {
		sme_debug("Supplicant disabled driver roaming");
		return QDF_STATUS_E_FAILURE;
	}

	if (0 == csr_roam_is_roam_offload_scan_enabled(mac_ctx)) {
		sme_err("isRoamOffloadScanEnabled not set");
		return QDF_STATUS_E_FAILURE;
	}
	if (!csr_is_RSO_cmd_allowed(mac_ctx, command, session_id)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("RSO out-of-sync command %d lastSentCmd %d"),
			command, roam_info->last_sent_cmd);
		return QDF_STATUS_E_FAILURE;
	}

	if ((true == roam_info->b_roam_scan_offload_started)
	    && (ROAM_SCAN_OFFLOAD_START == command)) {
		sme_err("Roam Scan Offload is already started");
		return QDF_STATUS_E_FAILURE;
	}

	if (session->pCurRoamProfile)
		roam_profile_akm =
			session->pCurRoamProfile->AuthType.authType[0];
	else
		sme_info("Roam profile is NULL");

	if (CSR_IS_AKM_FILS(roam_profile_akm) &&
	    !mac_ctx->is_fils_roaming_supported) {
		sme_info("FILS Roaming not suppprted by fw");
		return QDF_STATUS_SUCCESS;
	}

	if (!csr_is_adaptive_11r_roam_supported(mac_ctx, session)) {
		sme_info("Adaptive 11r Roaming not suppprted by fw");
		return QDF_STATUS_SUCCESS;
	}

	fw_akm_bitmap = mac_ctx->akm_service_bitmap;

	/* Roaming is not supported currently for OWE akm */
	if (roam_profile_akm == eCSR_AUTH_TYPE_OWE &&
	    !CSR_IS_FW_OWE_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("OWE Roaming not suppprted by fw");
		return QDF_STATUS_SUCCESS;
	}

	/* Roaming is not supported for SAE authentication */
	if (CSR_IS_AUTH_TYPE_SAE(roam_profile_akm) &&
	    !CSR_IS_FW_SAE_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("Roaming not suppprted for SAE connection");
		return QDF_STATUS_SUCCESS;
	}

	if ((roam_profile_akm == eCSR_AUTH_TYPE_SUITEB_EAP_SHA256 ||
	    roam_profile_akm == eCSR_AUTH_TYPE_SUITEB_EAP_SHA384) &&
	    !CSR_IS_FW_SUITEB_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("Roaming not supported for SUITEB connection");
		return QDF_STATUS_SUCCESS;
	}

	/*
	 * If fw doesn't advertise FT SAE, FT-FILS or FT-Suite-B capability,
	 * don't support roaming to that profile
	 */
	if (CSR_IS_AKM_FT_SAE(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_SAE_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT SAE akm");
			return QDF_STATUS_SUCCESS;
		}
	}

	if (CSR_IS_AKM_FT_SUITEB_SHA384(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_SUITEB_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT Suite-B akm");
			return QDF_STATUS_SUCCESS;
		}
	}

	if (CSR_IS_AKM_FT_FILS(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_FILS_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT FILS akm");
			return QDF_STATUS_SUCCESS;
		}
	}

	ucfg_nan_get_active_ndp_cnt(mac_ctx->psoc, &active_ndp_cnt);
	nan_disable_sta_roaming = (active_ndp_cnt &&
			(mac_ctx->roam.configParam.sta_disable_roam &
			 LFR3_STA_ROAM_DISABLE_BY_NAN));

	p2p_disable_sta_roaming =
		((mac_ctx->roam.configParam.sta_disable_roam &
		 LFR3_STA_ROAM_DISABLE_BY_P2P) &&
		 (policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
						PM_P2P_CLIENT_MODE, NULL) ||
		 policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
						PM_P2P_GO_MODE, NULL)));

	if ((command == ROAM_SCAN_OFFLOAD_START ||
	     command == ROAM_SCAN_OFFLOAD_UPDATE_CFG) &&
	     (p2p_disable_sta_roaming || nan_disable_sta_roaming)) {
		sme_debug("sta roaming blocked for active %s connection",
			  p2p_disable_sta_roaming ? "p2p" : "ndp");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * The Dynamic Config Items Update may happen even if the state is in
	 * INIT. It is important to ensure that the command is passed down to
	 * the FW only if the Infra Station is in a connected state. A connected
	 * station could also be in a PREAUTH or REASSOC states.
	 * 1) Block all CMDs that are not STOP in INIT State. For STOP always
	 *    inform firmware irrespective of state.
	 * 2) Block update cfg CMD if its for REASON_ROAM_SET_BLACKLIST_BSSID,
	 *    because we need to inform firmware of blacklisted AP for PNO in
	 *    all states.
	 */

	if ((roam_info->neighborRoamState ==
	     eCSR_NEIGHBOR_ROAM_STATE_INIT) &&
	    (command != ROAM_SCAN_OFFLOAD_STOP) &&
	    (reason != REASON_ROAM_SET_BLACKLIST_BSSID)) {
		state = mac_trace_get_neighbour_roam_state(
				roam_info->neighborRoamState);
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			 FL("Scan Command not sent to FW state=%s and cmd=%d"),
			  state,  command);
		return QDF_STATUS_E_FAILURE;
	}

	req_buf = csr_create_roam_scan_offload_request(mac_ctx, command,
						       session_id, reason,
						       session, roam_info);
	if (!req_buf) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "Failed to create req packet");
		return QDF_STATUS_E_FAILURE;
	}
	roam_params_dst = &req_buf->roam_params;
	roam_params_src = &mac_ctx->roam.configParam.roam_params;
	if (reason == REASON_ROAM_SET_SSID_ALLOWED)
		check_allowed_ssid_list(req_buf, roam_params_src);

	/*
	 * If rssi disallow bssid list have any member
	 * fill it and send it to firmware so that firmware does not
	 * try to roam to these BSS until RSSI OR time condition are
	 * matched.
	 */
	csr_add_rssi_reject_ap_list(mac_ctx, roam_params_src);

	/*
	 * Configure the lookup threshold either from INI or from framework.
	 * If both are present, give higher priority to the one from framework.
	 */
	if (roam_params_src->alert_rssi_threshold)
		req_buf->LookupThreshold =
			roam_params_src->alert_rssi_threshold;
	else
		req_buf->LookupThreshold =
			(int8_t)roam_info->cfgParams.neighborLookupThreshold *
			(-1);
	req_buf->rssi_thresh_offset_5g =
		roam_info->cfgParams.rssi_thresh_offset_5g;
	qdf_mem_copy(roam_params_dst, roam_params_src,
		sizeof(*roam_params_dst));
	/*
	 * rssi_diff which is updated via framework is equivalent to the
	 * INI RoamRssiDiff parameter and hence should be updated.
	 */
	if (roam_params_src->rssi_diff)
		req_buf->RoamRssiDiff = roam_params_src->rssi_diff;

	/* Set initial dense roam status */
	if (mac_ctx->scan.roam_candidate_count[session_id] >
	    roam_params_dst->dense_min_aps_cnt)
		roam_params_dst->initial_dense_status = true;

	op_channel = session->connectedProfile.operationChannel;
	req_buf->hi_rssi_scan_max_count =
		roam_info->cfgParams.hi_rssi_scan_max_count;
	req_buf->hi_rssi_scan_delay =
		roam_info->cfgParams.hi_rssi_scan_delay;
	req_buf->hi_rssi_scan_rssi_ub =
		roam_info->cfgParams.hi_rssi_scan_rssi_ub;
	/*
	 * If the current operation channel is 5G frequency band, then
	 * there is no need to enable the HI_RSSI feature. This feature
	 * is useful only if we are connected to a 2.4 GHz AP and we wish
	 * to connect to a better 5GHz AP is available.
	 */
	if (session->disable_hi_rssi)
		req_buf->hi_rssi_scan_rssi_delta = 0;
	else
		req_buf->hi_rssi_scan_rssi_delta =
			roam_info->cfgParams.hi_rssi_scan_rssi_delta;

	if (command != ROAM_SCAN_OFFLOAD_STOP) {
		req_buf->assoc_ie.length = session->nAddIEAssocLength;
		qdf_mem_copy(req_buf->assoc_ie.addIEdata,
				session->pAddIEAssoc,
				session->nAddIEAssocLength);
		csr_update_driver_assoc_ies(mac_ctx, session, req_buf);
		csr_update_score_params(mac_ctx, req_buf, roam_info);
		csr_update_fils_params_rso(mac_ctx, session, req_buf);
	}

	/*
	 * 11k offload is enabled during RSO Start after connect indication and
	 * 11k offload is disabled during RSO Stop after disconnect indication
	 */
	if (command == ROAM_SCAN_OFFLOAD_START)
		csr_update_11k_offload_params(mac_ctx, session, req_buf, TRUE);
	else if (command == ROAM_SCAN_OFFLOAD_STOP)
		csr_update_11k_offload_params(mac_ctx, session, req_buf, FALSE);

	prev_roaming_state = roam_info->b_roam_scan_offload_started;
	if (ROAM_SCAN_OFFLOAD_START == command)
		roam_info->b_roam_scan_offload_started = true;
	else if (ROAM_SCAN_OFFLOAD_STOP == command)
		roam_info->b_roam_scan_offload_started = false;
	policy_mgr_set_pcl_for_existing_combo(mac_ctx->psoc, PM_STA_MODE);

	/* Update PER config to FW. No need to update in case of stop command,
	 * FW takes care of stopping this internally
	 */
	if (command != ROAM_SCAN_OFFLOAD_STOP)
		csr_roam_offload_per_scan(mac_ctx, session_id);

	if (!QDF_IS_STATUS_SUCCESS(
		csr_roam_send_rso_cmd(mac_ctx, session_id, req_buf))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Not able to post message to PE",
			  __func__);
		roam_info->b_roam_scan_offload_started = prev_roaming_state;
		policy_mgr_set_pcl_for_existing_combo(mac_ctx->psoc,
						      PM_STA_MODE);
		return QDF_STATUS_E_FAILURE;
	}
	/* update the last sent cmd */
	roam_info->last_sent_cmd = command;

	return status;
}

QDF_STATUS csr_roam_offload_scan_rsp_hdlr(tpAniSirGlobal pMac,
					  tpSirRoamOffloadScanRsp
						scanOffloadRsp)
{
	switch (scanOffloadRsp->reason) {
	case 0:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "Rsp for Roam Scan Offload with failure status");
		break;
	case REASON_OS_REQUESTED_ROAMING_NOW:
		csr_neighbor_roam_proceed_with_handoff_req(pMac,
						scanOffloadRsp->sessionId);
		break;

	default:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "Rsp for Roam Scan Offload with reason %d",
			  scanOffloadRsp->reason);
	}
	return QDF_STATUS_SUCCESS;
}
#endif

#ifndef QCA_SUPPORT_CP_STATS
/* pStaEntry is no longer invalid upon the return of this function. */
static void csr_roam_remove_stat_list_entry(tpAniSirGlobal pMac,
						tListElem *pEntry)
{
	if (pEntry) {
		if (csr_ll_remove_entry(&pMac->roam.statsClientReqList,
						pEntry, LL_ACCESS_LOCK))
			qdf_mem_free(GET_BASE_ADDR(pEntry,
					struct csr_statsclient_reqinfo, link));
	}
}

static void csr_roam_remove_entry_from_pe_stats_req_list(
tpAniSirGlobal pMac, struct csr_pestats_reqinfo *pPeStaEntry)
{
	tListElem *pEntry;
	struct csr_pestats_reqinfo *pTempStaEntry;

	pEntry = csr_ll_peek_head(&pMac->roam.peStatsReqList, LL_ACCESS_LOCK);
	if (!pEntry) {
		sme_err("List empty, no stats req for PE");
		return;
	}
	while (pEntry) {
		pTempStaEntry = GET_BASE_ADDR(pEntry,
				struct csr_pestats_reqinfo, link);
		if (NULL == pTempStaEntry
			|| (pTempStaEntry->statsMask !=
				pPeStaEntry->statsMask)) {
			pEntry = csr_ll_next(&pMac->roam.peStatsReqList, pEntry,
					LL_ACCESS_NOLOCK);
			continue;
		}
		sme_debug("Match found");
		if (csr_ll_remove_entry(&pMac->roam.peStatsReqList, pEntry,
					LL_ACCESS_LOCK)) {
			qdf_mem_free(pTempStaEntry);
			pTempStaEntry = NULL;
			break;
		}
		pEntry = csr_ll_next(&pMac->roam.peStatsReqList, pEntry,
				     LL_ACCESS_NOLOCK);
	} /* end of while loop */
}

static void csr_roam_report_statistics(tpAniSirGlobal pMac,
		uint32_t statsMask,
		tCsrStatsCallback callback, uint8_t staId,
		void *pContext)
{
	uint8_t stats[500];
	uint8_t *pStats = NULL;
	uint32_t tempMask = 0;
	uint8_t counter = 0;

	if (!callback) {
		sme_err("Cannot report callback NULL");
		return;
	}
	if (!statsMask) {
		sme_err("Cannot report statsMask is 0");
		return;
	}
	pStats = stats;
	tempMask = statsMask;
	while (tempMask) {
		if (tempMask & 1) {
			/* new stats info from PE, fill up the stats
			 * strucutres in PMAC
			 */
			switch (counter) {
			case eCsrSummaryStats:
				sme_debug("Summary stats");
				qdf_mem_copy(pStats,
					     (uint8_t *) &pMac->roam.
					     summaryStatsInfo,
					     sizeof(tCsrSummaryStatsInfo));
				pStats += sizeof(tCsrSummaryStatsInfo);
				break;
			case eCsrGlobalClassAStats:
				sme_debug("ClassA stats");
				qdf_mem_copy(pStats,
					     (uint8_t *) &pMac->roam.
					     classAStatsInfo,
					     sizeof(tCsrGlobalClassAStatsInfo));
				pStats += sizeof(tCsrGlobalClassAStatsInfo);
				break;
			case eCsrGlobalClassDStats:
				sme_debug("ClassD stats");
				qdf_mem_copy(pStats,
					     (uint8_t *) &pMac->roam.
					     classDStatsInfo,
					     sizeof(tCsrGlobalClassDStatsInfo));
				pStats += sizeof(tCsrGlobalClassDStatsInfo);
				break;
			case csr_per_chain_rssi_stats:
				sme_debug("Per Chain RSSI stats");
				qdf_mem_copy(pStats,
				  (uint8_t *)&pMac->roam.per_chain_rssi_stats,
				  sizeof(struct csr_per_chain_rssi_stats_info));
				pStats += sizeof(
					struct csr_per_chain_rssi_stats_info);
				break;
			default:
				sme_err(
					"Unknown stats type and counter %d",
					counter);
				break;
			}
		}
		tempMask >>= 1;
		counter++;
	}
	callback(stats, pContext);
}

static QDF_STATUS csr_roam_dereg_statistics_req(
	tpAniSirGlobal pMac)
{
	tListElem *pEntry = NULL;
	tListElem *pPrevEntry = NULL;
	struct csr_statsclient_reqinfo *pTempStaEntry = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pEntry = csr_ll_peek_head(&pMac->roam.statsClientReqList,
							LL_ACCESS_LOCK);
	if (!pEntry) {
		/* list empty */
		sme_debug("List empty, no request from upper layer client(s)");
		return status;
	}
	while (pEntry) {
		if (pPrevEntry) {
			pTempStaEntry =
				GET_BASE_ADDR(pPrevEntry,
					struct csr_statsclient_reqinfo, link);
			/* send up the stats report */
			csr_roam_report_statistics(pMac,
						pTempStaEntry->statsMask,
						   pTempStaEntry->callback,
						   pTempStaEntry->staId,
						   pTempStaEntry->pContext);
			csr_roam_remove_stat_list_entry(pMac, pPrevEntry);
		}
		pTempStaEntry =
		GET_BASE_ADDR(pEntry, struct csr_statsclient_reqinfo, link);
		if (pTempStaEntry->pPeStaEntry) {
			/* pPeStaEntry can be NULL */
			pTempStaEntry->pPeStaEntry->numClient--;
			/* check if we need to delete the entry from
			 * peStatsReqList too
			 */
			if (!pTempStaEntry->pPeStaEntry->numClient) {
				csr_roam_remove_entry_from_pe_stats_req_list(
								pMac,
								pTempStaEntry->
								pPeStaEntry);
			}
		}
		/* check if we need to stop the tl stats timer too */
		pMac->roam.tlStatsReqInfo.numClient--;
		pPrevEntry = pEntry;
		pEntry = csr_ll_next(&pMac->roam.statsClientReqList, pEntry,
				     LL_ACCESS_NOLOCK);
	}
	/* the last one */
	if (pPrevEntry) {
		pTempStaEntry =
		GET_BASE_ADDR(pPrevEntry, struct csr_statsclient_reqinfo, link);
		/* send up the stats report */
		csr_roam_report_statistics(pMac, pTempStaEntry->statsMask,
					   pTempStaEntry->callback,
					   pTempStaEntry->staId,
					   pTempStaEntry->pContext);
		csr_roam_remove_stat_list_entry(pMac, pPrevEntry);
	}
	return status;

}
#endif /* QCA_SUPPORT_CP_STATS */

tSmeCmd *csr_get_command_buffer(tpAniSirGlobal pMac)
{
	tSmeCmd *pCmd = sme_get_command_buffer(pMac);

	if (pCmd)
		pMac->roam.sPendingCommands++;

	return pCmd;
}

static void csr_free_cmd_memory(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
	if (!pCommand) {
		sme_err("pCommand is NULL");
		return;
	}
	switch (pCommand->command) {
	case eSmeCommandRoam:
		csr_release_command_roam(pMac, pCommand);
		break;
	case eSmeCommandWmStatusChange:
		csr_release_command_wm_status_change(pMac, pCommand);
		break;
	case e_sme_command_set_hw_mode:
		csr_release_command_set_hw_mode(pMac, pCommand);
	default:
		break;
	}
}

void csr_release_command_buffer(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
	if (pMac->roam.sPendingCommands > 0) {
		/*
		 * All command allocated through csr_get_command_buffer
		 * need to decrement the pending count when releasing
		 */
		pMac->roam.sPendingCommands--;
		csr_free_cmd_memory(pMac, pCommand);
		sme_release_command(pMac, pCommand);
	} else {
		sme_err("no pending commands");
		QDF_ASSERT(0);
	}
}

void csr_release_command(tpAniSirGlobal mac_ctx, tSmeCmd *sme_cmd)
{
	struct wlan_serialization_queued_cmd_info cmd_info;
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev;

	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
			sme_cmd->sessionId, WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}
	qdf_mem_zero(&cmd_info,
			sizeof(struct wlan_serialization_queued_cmd_info));

	cmd_info.cmd_id = sme_cmd->cmd_id;
	cmd_info.req_type = WLAN_SER_CANCEL_NON_SCAN_CMD;
	cmd_info.cmd_type = csr_get_cmd_type(sme_cmd);
	cmd_info.vdev = vdev;
	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	cmd.cmd_id = cmd_info.cmd_id;
	cmd.cmd_type = cmd_info.cmd_type;
	cmd.vdev = cmd_info.vdev;
	if (wlan_serialization_is_cmd_present_in_active_queue(
				mac_ctx->psoc, &cmd))
		wlan_serialization_remove_cmd(&cmd_info);
	else if (wlan_serialization_is_cmd_present_in_pending_queue(
				mac_ctx->psoc, &cmd))
		wlan_serialization_cancel_request(&cmd_info);
	else
		sme_debug("can't find cmd_id[%d] cmd_type[%d]",
			  cmd_info.cmd_id, cmd_info.cmd_type);

	if (cmd_info.vdev)
		wlan_objmgr_vdev_release_ref(cmd_info.vdev, WLAN_LEGACY_SME_ID);
}


static enum wlan_serialization_cmd_type csr_get_roam_cmd_type(
		tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->u.roamCmd.roamReason) {
	case eCsrForcedDisassoc:
		cmd_type = WLAN_SER_CMD_FORCE_DISASSOC;
		break;
	case eCsrHddIssued:
		cmd_type = WLAN_SER_CMD_HDD_ISSUED;
		break;
	case eCsrForcedDisassocMICFailure:
		cmd_type = WLAN_SER_CMD_FORCE_DISASSOC_MIC_FAIL;
		break;
	case eCsrHddIssuedReassocToSameAP:
		cmd_type = WLAN_SER_CMD_HDD_ISSUE_REASSOC_SAME_AP;
		break;
	case eCsrSmeIssuedReassocToSameAP:
		cmd_type = WLAN_SER_CMD_SME_ISSUE_REASSOC_SAME_AP;
		break;
	case eCsrForcedDeauth:
		cmd_type = WLAN_SER_CMD_FORCE_DEAUTH;
		break;
	case eCsrSmeIssuedDisassocForHandoff:
		cmd_type =
			WLAN_SER_CMD_SME_ISSUE_DISASSOC_FOR_HANDOFF;
		break;
	case eCsrSmeIssuedAssocToSimilarAP:
		cmd_type =
			WLAN_SER_CMD_SME_ISSUE_ASSOC_TO_SIMILAR_AP;
		break;
	case eCsrForcedIbssLeave:
		cmd_type = WLAN_SER_CMD_FORCE_IBSS_LEAVE;
		break;
	case eCsrStopBss:
		cmd_type = WLAN_SER_CMD_STOP_BSS;
		break;
	case eCsrSmeIssuedFTReassoc:
		cmd_type = WLAN_SER_CMD_SME_ISSUE_FT_REASSOC;
		break;
	case eCsrForcedDisassocSta:
		cmd_type = WLAN_SER_CMD_FORCE_DISASSOC_STA;
		break;
	case eCsrForcedDeauthSta:
		cmd_type = WLAN_SER_CMD_FORCE_DEAUTH_STA;
		break;
	case eCsrPerformPreauth:
		cmd_type = WLAN_SER_CMD_PERFORM_PRE_AUTH;
		break;
	default:
		break;
	}

	return cmd_type;
}

enum wlan_serialization_cmd_type csr_get_cmd_type(tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->command) {
	case eSmeCommandRoam:
		cmd_type = csr_get_roam_cmd_type(sme_cmd);
		break;
	case eSmeCommandWmStatusChange:
		cmd_type = WLAN_SER_CMD_WM_STATUS_CHANGE;
		break;
	case e_sme_command_del_sta_session:
		cmd_type = WLAN_SER_CMD_DEL_STA_SESSION;
		break;
	case eSmeCommandAddTs:
		cmd_type = WLAN_SER_CMD_ADDTS;
		break;
	case eSmeCommandDelTs:
		cmd_type = WLAN_SER_CMD_DELTS;
		break;
	case e_sme_command_set_hw_mode:
		cmd_type = WLAN_SER_CMD_SET_HW_MODE;
		break;
	case e_sme_command_nss_update:
		cmd_type = WLAN_SER_CMD_NSS_UPDATE;
		break;
	case e_sme_command_set_dual_mac_config:
		cmd_type = WLAN_SER_CMD_SET_DUAL_MAC_CONFIG;
		break;
	case e_sme_command_set_antenna_mode:
		cmd_type = WLAN_SER_CMD_SET_ANTENNA_MODE;
		break;
	default:
		break;
	}

	return cmd_type;
}

static uint32_t csr_get_monotonous_number(tpAniSirGlobal mac_ctx)
{
	uint32_t cmd_id;
	uint32_t mask = 0x00FFFFFF, prefix = 0x0D000000;

	cmd_id = qdf_atomic_inc_return(&mac_ctx->global_cmd_id);
	cmd_id = (cmd_id & mask);
	cmd_id = (cmd_id | prefix);

	return cmd_id;
}

QDF_STATUS csr_set_serialization_params_to_cmd(tpAniSirGlobal mac_ctx,
		tSmeCmd *sme_cmd, struct wlan_serialization_command *cmd,
		uint8_t high_priority)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!sme_cmd) {
		sme_err("Invalid sme_cmd");
		return status;
	}
	if (!cmd) {
		sme_err("Invalid serialization_cmd");
		return status;
	}

	/*
	 * no need to fill command id for non-scan as they will be
	 * zero always
	 */
	sme_cmd->cmd_id = csr_get_monotonous_number(mac_ctx);
	cmd->cmd_id = sme_cmd->cmd_id;

	cmd->cmd_type = csr_get_cmd_type(sme_cmd);
	if (cmd->cmd_type == WLAN_SER_CMD_MAX) {
		sme_err("serialization enum not found for sme_cmd type %d",
			sme_cmd->command);
		return status;
	}
	cmd->vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
				sme_cmd->sessionId, WLAN_LEGACY_SME_ID);
	if (!cmd->vdev) {
		sme_err("vdev is NULL for sme_session:%d", sme_cmd->sessionId);
		return status;
	}
	cmd->umac_cmd = sme_cmd;

	/*
	 * For START BSS and STOP BSS commands for SAP, the command timeout
	 * is set to 10 seconds. For all other commands its 30 seconds
	 */
	if ((cmd->vdev->vdev_mlme.vdev_opmode == QDF_SAP_MODE) &&
	    ((cmd->cmd_type == WLAN_SER_CMD_HDD_ISSUED) ||
	    (cmd->cmd_type == WLAN_SER_CMD_STOP_BSS)))
		cmd->cmd_timeout_duration = SME_START_STOP_BSS_CMD_TIMEOUT;
	else if (cmd->cmd_type == WLAN_SER_CMD_DEL_STA_SESSION)
		cmd->cmd_timeout_duration = SME_VDEV_DELETE_CMD_TIMEOUT;
	else
		cmd->cmd_timeout_duration = SME_DEFAULT_CMD_TIMEOUT;

	cmd->cmd_cb = sme_ser_cmd_callback;
	cmd->is_high_priority = high_priority;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_queue_sme_command(tpAniSirGlobal mac_ctx, tSmeCmd *sme_cmd,
				 bool high_priority)
{
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev = NULL;
	enum wlan_serialization_status ser_cmd_status;
	QDF_STATUS status;

	if (!SME_IS_START(mac_ctx)) {
		sme_err("Sme in stop state");
		QDF_ASSERT(0);
		goto error;
	}

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, sme_cmd->sessionId)) {
		if (!CSR_IS_DISCONNECT_COMMAND(sme_cmd)) {
			sme_err("Can't process cmd(%d), waiting for key",
				sme_cmd->command);
			goto error;
		}
	}

	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	status = csr_set_serialization_params_to_cmd(mac_ctx, sme_cmd,
					&cmd, high_priority);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to set ser params");
		goto error;
	}

	vdev = cmd.vdev;
	ser_cmd_status = wlan_serialization_request(&cmd);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
	case WLAN_SER_CMD_ACTIVE:
		/* Command posted to active/pending list */
		status = QDF_STATUS_SUCCESS;
		break;
	default:
		sme_err("Failed to queue command %d with status:%d",
			sme_cmd->command, ser_cmd_status);
		status = QDF_STATUS_E_FAILURE;
		goto error;
	}

	return status;

error:
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	csr_release_command_buffer(mac_ctx, sme_cmd);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS csr_roam_update_config(tpAniSirGlobal mac_ctx, uint8_t session_id,
				  uint16_t capab, uint32_t value)
{
	struct update_config *msg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	sme_debug("update HT config requested");
	if (NULL == session) {
		sme_err("Session does not exist for session id %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (NULL == msg) {
		sme_err("malloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	msg->messageType = eWNI_SME_UPDATE_CONFIG;
	msg->sme_session_id = session_id;
	msg->capab = capab;
	msg->value = value;
	msg->length = sizeof(*msg);
	status = umac_send_mb_message_to_mac(msg);

	return status;
}

/*
 * pBuf points to the beginning of the message
 * LIM packs disassoc rsp as below,
 * messageType - 2 bytes
 * messageLength - 2 bytes
 * sessionId - 1 byte
 * transactionId - 2 bytes (uint16_t)
 * reasonCode - 4 bytes (sizeof(tSirResultCodes))
 * peerMacAddr - 6 bytes
 * The rest is conditionally defined of (WNI_POLARIS_FW_PRODUCT == AP)
 * and not used
 */
static void csr_ser_des_unpack_diassoc_rsp(uint8_t *pBuf, tSirSmeDisassocRsp
									*pRsp)
{
	if (pBuf && pRsp) {
		pBuf += 4;      /* skip type and length */
		pRsp->sessionId = *pBuf++;
		qdf_get_u16(pBuf, (uint16_t *) &pRsp->transactionId);
		pBuf += 2;
		qdf_get_u32(pBuf, (uint32_t *) &pRsp->statusCode);
		pBuf += 4;
		qdf_mem_copy(pRsp->peer_macaddr.bytes, pBuf, QDF_MAC_ADDR_SIZE);
	}
}

/* Returns whether a session is in QDF_STA_MODE...or not */
bool csr_roam_is_sta_mode(tpAniSirGlobal pMac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = NULL;

	pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("session %d not found",	sessionId);
		return false;
	}
	if (!CSR_IS_SESSION_VALID(pMac, sessionId)) {
		sme_err("Inactive session_id: %d", sessionId);
		return false;
	}
	if (eCSR_BSS_TYPE_INFRASTRUCTURE != pSession->connectedProfile.BSSType)
		return false;
	/* There is a possibility that the above check may fail,because
	 * P2P CLI also uses the same BSSType (eCSR_BSS_TYPE_INFRASTRUCTURE)
	 * when it is connected.So,we may sneak through the above check even
	 * if we are not a STA mode INFRA station. So, if we sneak through
	 * the above condition, we can use the following check if we are
	 * really in STA Mode.
	 */

	if (NULL != pSession->pCurRoamProfile) {
		if (pSession->pCurRoamProfile->csrPersona == QDF_STA_MODE)
			return true;
		else
			return false;
	}

	return false;
}

QDF_STATUS csr_handoff_request(tpAniSirGlobal pMac,
			       uint8_t sessionId,
			       tCsrHandoffRequest *pHandoffInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};

	tAniHandoffReq *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tAniHandoffReq));
	if (NULL == pMsg) {
		sme_err("csr_handoff_request: failed to allocate mem for req ");
		return QDF_STATUS_E_NOMEM;
	}
	pMsg->msgType = eWNI_SME_HANDOFF_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniHandoffReq);
	pMsg->sessionId = sessionId;
	pMsg->channel = pHandoffInfo->channel;
	pMsg->handoff_src = pHandoffInfo->src;
	qdf_mem_copy(pMsg->bssid, pHandoffInfo->bssid.bytes, QDF_MAC_ADDR_SIZE);
	msg.type = eWNI_SME_HANDOFF_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		sme_err("scheduler_post_msg failed to post msg to self");
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}

/**
 * csr_roam_channel_change_req() - Post channel change request to LIM
 * @pMac: mac context
 * @bssid: SAP bssid
 * @ch_params: channel information
 * @profile: CSR profile
 *
 * This API is primarily used to post Channel Change Req for SAP
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_roam_channel_change_req(tpAniSirGlobal pMac,
				       struct qdf_mac_addr bssid,
				       struct ch_params *ch_params,
				       struct csr_roam_profile *profile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirChanChangeRequest *pMsg;
	struct csr_roamstart_bssparams param;
	bool skip_hostapd_rate = !profile->chan_switch_hostapd_rate_enabled;

	/*
	 * while changing the channel, use basic rates given by driver
	 * and not by hostapd as there is a chance that hostapd might
	 * give us rates based on original channel which may not be
	 * suitable for new channel
	 */
	qdf_mem_zero(&param, sizeof(struct csr_roamstart_bssparams));

	status = csr_roam_get_bss_start_parms(pMac, profile, &param,
					      skip_hostapd_rate);

	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Failed to get bss parameters");
		return status;
	}

	pMsg = qdf_mem_malloc(sizeof(tSirChanChangeRequest));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_CHANNEL_CHANGE_REQ;
	pMsg->messageLen = sizeof(tSirChanChangeRequest);
	pMsg->targetChannel = profile->ChannelInfo.ChannelList[0];
	pMsg->sec_ch_offset = ch_params->sec_ch_offset;
	pMsg->ch_width = profile->ch_params.ch_width;
	pMsg->dot11mode = csr_translate_to_wni_cfg_dot11_mode(pMac,
					param.uCfgDot11Mode);
	if (IS_24G_CH(pMsg->targetChannel) &&
	   (false == pMac->roam.configParam.enableVhtFor24GHz) &&
	   (WNI_CFG_DOT11_MODE_11AC == pMsg->dot11mode ||
	    WNI_CFG_DOT11_MODE_11AC_ONLY == pMsg->dot11mode))
		pMsg->dot11mode = WNI_CFG_DOT11_MODE_11N;
	pMsg->nw_type = param.sirNwType;
	pMsg->center_freq_seg_0 = ch_params->center_freq_seg0;
	pMsg->center_freq_seg_1 = ch_params->center_freq_seg1;
	pMsg->cac_duration_ms = profile->cac_duration_ms;
	pMsg->dfs_regdomain = profile->dfs_regdomain;
	qdf_mem_copy(pMsg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&pMsg->operational_rateset,
		&param.operationalRateSet, sizeof(pMsg->operational_rateset));
	qdf_mem_copy(&pMsg->extended_rateset,
		&param.extendedRateSet, sizeof(pMsg->extended_rateset));

	status = umac_send_mb_message_to_mac(pMsg);

	return status;
}

/*
 * Post Beacon Tx Start request to LIM
 * immediately after SAP CAC WAIT is
 * completed without any RADAR indications.
 */
QDF_STATUS csr_roam_start_beacon_req(tpAniSirGlobal pMac,
				     struct qdf_mac_addr bssid,
				     uint8_t dfsCacWaitStatus)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirStartBeaconIndication *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tSirStartBeaconIndication));

	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_START_BEACON_REQ;
	pMsg->messageLen = sizeof(tSirStartBeaconIndication);
	pMsg->beaconStartStatus = dfsCacWaitStatus;
	qdf_mem_copy(pMsg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);

	status = umac_send_mb_message_to_mac(pMsg);

	return status;
}

/*
 * csr_roam_modify_add_ies -
 * This function sends msg to modify the additional IE buffers in PE
 *
 * @pMac: pMac global structure
 * @pModifyIE: pointer to tSirModifyIE structure
 * @updateType: Type of buffer
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_modify_add_ies(tpAniSirGlobal pMac,
			 tSirModifyIE *pModifyIE, eUpdateIEsType updateType)
{
	tpSirModifyIEsInd pModifyAddIEInd = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	/* following buffer will be freed by consumer (PE) */
	pLocalBuffer = qdf_mem_malloc(pModifyIE->ieBufferlength);

	if (NULL == pLocalBuffer) {
		sme_err("Memory Allocation Failure!!!");
		return QDF_STATUS_E_NOMEM;
	}

	pModifyAddIEInd = qdf_mem_malloc(sizeof(tSirModifyIEsInd));
	if (NULL == pModifyAddIEInd) {
		sme_err("Memory Allocation Failure!!!");
		qdf_mem_free(pLocalBuffer);
		return QDF_STATUS_E_NOMEM;
	}

	/*copy the IE buffer */
	qdf_mem_copy(pLocalBuffer, pModifyIE->pIEBuffer,
		     pModifyIE->ieBufferlength);
	qdf_mem_zero(pModifyAddIEInd, sizeof(tSirModifyIEsInd));

	pModifyAddIEInd->msgType = eWNI_SME_MODIFY_ADDITIONAL_IES;
	pModifyAddIEInd->msgLen = sizeof(tSirModifyIEsInd);

	qdf_copy_macaddr(&pModifyAddIEInd->modifyIE.bssid, &pModifyIE->bssid);

	pModifyAddIEInd->modifyIE.smeSessionId = pModifyIE->smeSessionId;
	pModifyAddIEInd->modifyIE.notify = pModifyIE->notify;
	pModifyAddIEInd->modifyIE.ieID = pModifyIE->ieID;
	pModifyAddIEInd->modifyIE.ieIDLen = pModifyIE->ieIDLen;
	pModifyAddIEInd->modifyIE.pIEBuffer = pLocalBuffer;
	pModifyAddIEInd->modifyIE.ieBufferlength = pModifyIE->ieBufferlength;
	pModifyAddIEInd->modifyIE.oui_length = pModifyIE->oui_length;

	pModifyAddIEInd->updateType = updateType;

	status = umac_send_mb_message_to_mac(pModifyAddIEInd);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/*
 * csr_roam_update_add_ies -
 * This function sends msg to updates the additional IE buffers in PE
 *
 * @pMac: pMac global structure
 * @sessionId: SME session id
 * @bssid: BSSID
 * @additionIEBuffer: buffer containing addition IE from hostapd
 * @length: length of buffer
 * @updateType: Type of buffer
 * @append: append or replace completely
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_update_add_ies(tpAniSirGlobal pMac,
			 tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType)
{
	tpSirUpdateIEsInd pUpdateAddIEs = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	if (pUpdateIE->ieBufferlength != 0) {
		/* Following buffer will be freed by consumer (PE) */
		pLocalBuffer = qdf_mem_malloc(pUpdateIE->ieBufferlength);
		if (NULL == pLocalBuffer) {
			sme_err("Memory Allocation Failure!!!");
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_copy(pLocalBuffer, pUpdateIE->pAdditionIEBuffer,
			     pUpdateIE->ieBufferlength);
	}

	pUpdateAddIEs = qdf_mem_malloc(sizeof(tSirUpdateIEsInd));
	if (NULL == pUpdateAddIEs) {
		sme_err("Memory Allocation Failure!!!");
		if (pLocalBuffer != NULL)
			qdf_mem_free(pLocalBuffer);

		return QDF_STATUS_E_NOMEM;
	}

	pUpdateAddIEs->msgType = eWNI_SME_UPDATE_ADDITIONAL_IES;
	pUpdateAddIEs->msgLen = sizeof(tSirUpdateIEsInd);

	qdf_copy_macaddr(&pUpdateAddIEs->updateIE.bssid, &pUpdateIE->bssid);

	pUpdateAddIEs->updateIE.smeSessionId = pUpdateIE->smeSessionId;
	pUpdateAddIEs->updateIE.append = pUpdateIE->append;
	pUpdateAddIEs->updateIE.notify = pUpdateIE->notify;
	pUpdateAddIEs->updateIE.ieBufferlength = pUpdateIE->ieBufferlength;
	pUpdateAddIEs->updateIE.pAdditionIEBuffer = pLocalBuffer;

	pUpdateAddIEs->updateType = updateType;

	status = umac_send_mb_message_to_mac(pUpdateAddIEs);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/**
 * csr_send_ext_change_channel()- function to post send ECSA
 * action frame to lim.
 * @mac_ctx: pointer to global mac structure
 * @channel: new channel to switch
 * @session_id: senssion it should be sent on.
 *
 * This function is called to post ECSA frame to lim.
 *
 * Return: success if msg posted to LIM else return failure
 */
QDF_STATUS csr_send_ext_change_channel(tpAniSirGlobal mac_ctx, uint32_t channel,
					uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct sir_sme_ext_cng_chan_req *msg;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (NULL == msg)
		return QDF_STATUS_E_NOMEM;

	msg->message_type = eWNI_SME_EXT_CHANGE_CHANNEL;
	msg->length = sizeof(*msg);
	msg->new_channel = channel;
	msg->session_id = session_id;
	status = umac_send_mb_message_to_mac(msg);
	return status;
}

/**
 * csr_roam_send_chan_sw_ie_request() - Request to transmit CSA IE
 * @mac_ctx:        Global MAC context
 * @bssid:          BSSID
 * @target_channel: Channel on which to send the IE
 * @csa_ie_reqd:    Include/Exclude CSA IE.
 * @ch_params:  operating Channel related information
 *
 * This function sends request to transmit channel switch announcement
 * IE to lower layers
 *
 * Return: success or failure
 **/
QDF_STATUS csr_roam_send_chan_sw_ie_request(tpAniSirGlobal mac_ctx,
					    struct qdf_mac_addr bssid,
					    uint8_t target_channel,
					    uint8_t csa_ie_reqd,
					    struct ch_params *ch_params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirDfsCsaIeRequest *msg;

	msg = qdf_mem_malloc(sizeof(tSirDfsCsaIeRequest));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->msgType = eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ;
	msg->msgLen = sizeof(tSirDfsCsaIeRequest);

	msg->targetChannel = target_channel;
	msg->csaIeRequired = csa_ie_reqd;
	msg->ch_switch_beacon_cnt =
		 mac_ctx->sap.SapDfsInfo.sap_ch_switch_beacon_cnt;
	msg->ch_switch_mode = mac_ctx->sap.SapDfsInfo.sap_ch_switch_mode;
	msg->dfs_ch_switch_disable =
		mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch;
	qdf_mem_copy(msg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&msg->ch_params, ch_params, sizeof(struct ch_params));

	status = umac_send_mb_message_to_mac(msg);

	return status;
}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
/**
 * csr_roaming_report_diag_event() - Diag events for LFR3
 * @mac_ctx:              MAC context
 * @roam_synch_ind_ptr:   Roam Synch Indication Pointer
 * @reason:               Reason for this event to happen
 *
 * The major events in the host for LFR3 roaming such as
 * roam synch indication, roam synch completion and
 * roam synch handoff fail will be indicated to the
 * diag framework using this API.
 *
 * Return: None
 */
void csr_roaming_report_diag_event(tpAniSirGlobal mac_ctx,
		roam_offload_synch_ind *roam_synch_ind_ptr,
		enum csr_diagwlan_status_eventreason reason)
{
	WLAN_HOST_DIAG_EVENT_DEF(roam_connection,
		host_event_wlan_status_payload_type);
	qdf_mem_zero(&roam_connection,
		sizeof(host_event_wlan_status_payload_type));
	switch (reason) {
	case eCSR_REASON_ROAM_SYNCH_IND:
		roam_connection.eventId = eCSR_WLAN_STATUS_CONNECT;
		if (roam_synch_ind_ptr) {
			roam_connection.rssi = roam_synch_ind_ptr->rssi;
			roam_connection.channel =
				cds_freq_to_chan(roam_synch_ind_ptr->chan_freq);
		}
		break;
	case eCSR_REASON_ROAM_SYNCH_CNF:
		roam_connection.eventId = eCSR_WLAN_STATUS_CONNECT;
		break;
	case eCSR_REASON_ROAM_HO_FAIL:
		roam_connection.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		break;
	default:
		sme_err("LFR3: Unsupported reason %d", reason);
		return;
	}
	roam_connection.reason = reason;
	WLAN_HOST_DIAG_EVENT_REPORT(&roam_connection, EVENT_WLAN_STATUS_V2);
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*
 * fn csr_process_ho_fail_ind
 * brief  This function will process the Hand Off Failure indication
 *        received from the firmware. It will trigger a disconnect on
 *        the session which the firmware reported a hand off failure
 * param  pMac global structure
 * param  pMsgBuf - Contains the session ID for which the handler should apply
 */
void csr_process_ho_fail_ind(tpAniSirGlobal mac_ctx, void *pMsgBuf)
{
	tSirSmeHOFailureInd *pSmeHOFailInd = (tSirSmeHOFailureInd *) pMsgBuf;
	uint32_t sessionId;

	if (pSmeHOFailInd)
		sessionId = pSmeHOFailInd->sessionId;
	else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "LFR3: Hand-Off Failure Ind is NULL");
		return;
	}
	/* Roaming is supported only on Infra STA Mode. */
	if (!csr_roam_is_sta_mode(mac_ctx, sessionId)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "LFR3:HO Fail cannot be handled for session %d",
			  sessionId);
		return;
	}
	mac_ctx->sme.set_connection_info_cb(false);
	csr_roam_roaming_offload_timer_action(mac_ctx, 0, sessionId,
			ROAMING_OFFLOAD_TIMER_STOP);
	csr_roam_call_callback(mac_ctx, sessionId, NULL, 0,
			eCSR_ROAM_NAPI_OFF, eCSR_ROAM_RESULT_FAILURE);
	csr_roam_synch_clean_up(mac_ctx, sessionId);
	csr_roaming_report_diag_event(mac_ctx, NULL,
			eCSR_REASON_ROAM_HO_FAIL);
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
		  "LFR3:Issue Disconnect on session %d", sessionId);
	csr_roam_disconnect(mac_ctx, sessionId,
			eCSR_DISCONNECT_REASON_ROAM_HO_FAIL,
			eSIR_MAC_FW_TRIGGERED_ROAM_FAILURE);
	if (mac_ctx->roam.configParam.enable_fatal_event)
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_ROAM_HO_FAILURE,
				false, false);
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

/**
 * csr_update_op_class_array() - update op class for each band
 * @mac_ctx:          mac global context
 * @op_classes:       out param, operating class array to update
 * @channel_info:     channel info
 * @ch_name:          channel band name to display in debug messages
 * @i:                out param, stores number of operating classes
 *
 * Return: void
 */
static void
csr_update_op_class_array(tpAniSirGlobal mac_ctx,
			  uint8_t *op_classes,
			  struct csr_channel *channel_info,
			  char *ch_name,
			  uint8_t *i)
{
	uint8_t j = 0, idx = 0, class = 0;
	bool found = false;
	uint8_t num_channels = channel_info->numChannels;
	uint8_t ch_bandwidth;

	sme_debug("Num %s channels,  %d", ch_name, num_channels);

	for (idx = 0; idx < num_channels &&
			*i < (REG_MAX_SUPP_OPER_CLASSES - 1); idx++) {
		for (ch_bandwidth = BW20; ch_bandwidth < BWALL;
			ch_bandwidth++) {
			class = wlan_reg_dmn_get_opclass_from_channel(
					mac_ctx->scan.countryCodeCurrent,
					channel_info->channelList[idx],
					ch_bandwidth);

			found = false;
			for (j = 0; j < REG_MAX_SUPP_OPER_CLASSES - 1;
				j++) {
				if (op_classes[j] == class) {
					found = true;
					break;
				}
			}

			if (!found) {
				op_classes[*i] = class;
				*i = *i + 1;
			}
		}
	}
}

/**
 * csr_update_op_class_array() - update op class for all bands
 * @hHal:          global hal context
 *
 * Return: void
 */
static void csr_init_operating_classes(tHalHandle hHal)
{
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t swap = 0;
	uint8_t numClasses = 0;
	uint8_t opClasses[REG_MAX_SUPP_OPER_CLASSES] = {0,};
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

	sme_debug("Current Country = %c%c",
		pMac->scan.countryCodeCurrent[0],
		pMac->scan.countryCodeCurrent[1]);

	csr_update_op_class_array(pMac, opClasses,
				  &pMac->scan.base_channels, "20MHz", &i);
	numClasses = i;

	/* As per spec the operating classes should be in ascending order.
	 * Bubble sort is fine since we don't have many classes
	 */
	for (i = 0; i < (numClasses - 1); i++) {
		for (j = 0; j < (numClasses - i - 1); j++) {
			/* For decreasing order use < */
			if (opClasses[j] > opClasses[j + 1]) {
				swap = opClasses[j];
				opClasses[j] = opClasses[j + 1];
				opClasses[j + 1] = swap;
			}
		}
	}

	/* Set the ordered list of op classes in regdomain
	 * for use by other modules
	 */
	wlan_reg_dmn_set_curr_opclasses(numClasses, &opClasses[0]);
}

/**
 * csr_find_session_by_type() - This function will find given session type from
 * all sessions.
 * @mac_ctx: pointer to mac context.
 * @type:    session type
 *
 * Return: session id for give session type.
 **/
static uint32_t
csr_find_session_by_type(tpAniSirGlobal mac_ctx, enum QDF_OPMODE type)
{
	uint32_t i, session_id = CSR_SESSION_ID_INVALID;
	struct csr_roam_session *session_ptr;

	for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
		if (!CSR_IS_SESSION_VALID(mac_ctx, i))
			continue;

		session_ptr = CSR_GET_SESSION(mac_ctx, i);
		if (type == session_ptr->bssParams.bssPersona) {
			session_id = i;
			break;
		}
	}
	return session_id;
}
/**
 * csr_is_conn_allow_2g_band() - This function will check if station's conn
 * is allowed in 2.4Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with SAP's operating channel. If SAP's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 **/
static bool csr_is_conn_allow_2g_band(tpAniSirGlobal mac_ctx, uint32_t chnl)
{
	uint32_t sap_session_id;
	struct csr_roam_session *sap_session;

	if (0 == chnl) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("channel is zero, connection not allowed"));

		return false;
	}

	sap_session_id = csr_find_session_by_type(mac_ctx, QDF_SAP_MODE);
	if (CSR_SESSION_ID_INVALID != sap_session_id) {
		sap_session = CSR_GET_SESSION(mac_ctx, sap_session_id);
		if ((0 != sap_session->bssParams.operationChn) &&
				(sap_session->bssParams.operationChn != chnl)) {

			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"Can't allow STA to connect, chnls not same");
			return false;
		}
	}
	return true;
}

/**
 * csr_is_conn_allow_5g_band() - This function will check if station's conn
 * is allowed in 5Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with P2PGO's operating channel. If P2PGO's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 **/
static bool csr_is_conn_allow_5g_band(tpAniSirGlobal mac_ctx, uint32_t chnl)
{
	uint32_t p2pgo_session_id;
	struct csr_roam_session *p2pgo_session;

	if (0 == chnl) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("channel is zero, connection not allowed"));
		return false;
	}

	p2pgo_session_id = csr_find_session_by_type(mac_ctx, QDF_P2P_GO_MODE);
	if (CSR_SESSION_ID_INVALID != p2pgo_session_id) {
		p2pgo_session = CSR_GET_SESSION(mac_ctx, p2pgo_session_id);
		if ((0 != p2pgo_session->bssParams.operationChn) &&
				(eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED !=
				 p2pgo_session->connectState) &&
				(p2pgo_session->bssParams.operationChn !=
				 chnl)) {

			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"Can't allow STA to connect, chnls not same");
			return false;
		}
	}
	return true;
}

/**
 * csr_clear_joinreq_param() - This function will clear station's params
 * for stored join request to csr.
 * @hal_handle: pointer to hal context.
 * @session_id: station's session id.
 *
 * This function will clear station's allocated memory for cached join
 * request.
 *
 * Return: true or false based on function's overall success.
 **/
bool csr_clear_joinreq_param(tpAniSirGlobal mac_ctx,
		uint32_t session_id)
{
	struct csr_roam_session *sta_session;
	struct scan_result_list *bss_list;

	if (NULL == mac_ctx)
		return false;

	sta_session = CSR_GET_SESSION(mac_ctx, session_id);
	if (NULL == sta_session)
		return false;

	/* Release the memory allocated by previous join request */
	bss_list =
		(struct scan_result_list *)&sta_session->stored_roam_profile.
		bsslist_handle;
	if (NULL != bss_list) {
		csr_scan_result_purge(mac_ctx,
			sta_session->stored_roam_profile.bsslist_handle);
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("bss list is released for session %d"), session_id);
		sta_session->stored_roam_profile.bsslist_handle = NULL;
	}
	sta_session->stored_roam_profile.bsslist_handle = NULL;
	csr_release_profile(mac_ctx, &sta_session->stored_roam_profile.profile);
	sta_session->stored_roam_profile.reason = 0;
	sta_session->stored_roam_profile.roam_id = 0;
	sta_session->stored_roam_profile.imediate_flag = false;
	sta_session->stored_roam_profile.clear_flag = false;
	return true;
}

/**
 * csr_store_joinreq_param() - This function will store station's join
 * request to that station's session.
 * @mac_ctx: pointer to mac context.
 * @profile: pointer to station's roam profile.
 * @scan_cache: pointer to station's scan cache.
 * @roam_id: reference to roam_id variable being passed.
 * @session_id: station's session id.
 *
 * This function will store station's join request to one of the
 * csr structure and add it to station's session.
 *
 * Return: true or false based on function's overall success.
 **/
bool csr_store_joinreq_param(tpAniSirGlobal mac_ctx,
		struct csr_roam_profile *profile,
		tScanResultHandle scan_cache,
		uint32_t *roam_id,
		uint32_t session_id)
{
	struct csr_roam_session *sta_session;

	if (NULL == mac_ctx)
		return false;

	sta_session = CSR_GET_SESSION(mac_ctx, session_id);
	if (NULL == sta_session)
		return false;

	sta_session->stored_roam_profile.session_id = session_id;
	csr_roam_copy_profile(mac_ctx,
			&sta_session->stored_roam_profile.profile, profile);
	/* new bsslist_handle's memory will be relased later */
	sta_session->stored_roam_profile.bsslist_handle = scan_cache;
	sta_session->stored_roam_profile.reason = eCsrHddIssued;
	sta_session->stored_roam_profile.roam_id = *roam_id;
	sta_session->stored_roam_profile.imediate_flag = false;
	sta_session->stored_roam_profile.clear_flag = false;

	return true;
}

/**
 * csr_issue_stored_joinreq() - This function will issues station's stored
 * the join request.
 * @mac_ctx: pointer to mac context.
 * @roam_id: reference to roam_id variable being passed.
 * @session_id: station's session id.
 *
 * This function will issue station's stored join request, from this point
 * onwards the flow will be just like normal connect request.
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE.
 **/
QDF_STATUS csr_issue_stored_joinreq(tpAniSirGlobal mac_ctx,
		uint32_t *roam_id,
		uint32_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *sta_session;
	uint32_t new_roam_id;

	sta_session = CSR_GET_SESSION(mac_ctx, session_id);
	if (NULL == sta_session)
		return QDF_STATUS_E_FAILURE;
	new_roam_id = GET_NEXT_ROAM_ID(&mac_ctx->roam);
	*roam_id = new_roam_id;
	status = csr_roam_issue_connect(mac_ctx,
			sta_session->stored_roam_profile.session_id,
			&sta_session->stored_roam_profile.profile,
			sta_session->stored_roam_profile.bsslist_handle,
			sta_session->stored_roam_profile.reason,
			new_roam_id,
			sta_session->stored_roam_profile.imediate_flag,
			sta_session->stored_roam_profile.clear_flag);

	sta_session->stored_roam_profile.bsslist_handle =
					CSR_INVALID_SCANRESULT_HANDLE;

	if (QDF_STATUS_SUCCESS != status) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL
			("CSR failed issuing connect cmd with status = 0x%08X"),
				status);
		csr_clear_joinreq_param(mac_ctx, session_id);
	}
	return status;
}

/**
 * csr_process_set_hw_mode() - Set HW mode command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the set HW mode command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_set_hw_mode(tpAniSirGlobal mac, tSmeCmd *command)
{
	uint32_t len;
	struct s_sir_set_hw_mode *cmd = NULL;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_set_hw_mode_resp *param;
	enum policy_mgr_hw_mode_change hw_mode;

	/* Setting HW mode is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd) {
		sme_err("Memory allocation failed");
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;
	}

	/* For hidden SSID case, if there is any scan command pending
	 * it needs to be cleared before issuing set HW mode
	 */
	if (command->u.set_hw_mode_cmd.reason ==
		POLICY_MGR_UPDATE_REASON_HIDDEN_STA) {
		sme_err("clear any pending scan command");
		status = csr_scan_abort_mac_scan(mac,
			command->u.set_hw_mode_cmd.session_id, INVAL_SCAN_ID);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Failed to clear scan cmd");
			goto fail;
		}
	}

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
		command->u.set_hw_mode_cmd.reason) &&
		(true == mac->sme.get_connection_info_cb(NULL, NULL))) {
		sme_err("Set HW mode refused: conn in progress");
		policy_mgr_restart_opportunistic_timer(mac->psoc, false);
		goto fail;
	}

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
		command->u.set_hw_mode_cmd.reason) &&
		(!command->u.set_hw_mode_cmd.hw_mode_index &&
		!policy_mgr_need_opportunistic_upgrade(mac->psoc))) {
		sme_err("Set HW mode to SMM not needed anymore");
		goto fail;
	}

	hw_mode = policy_mgr_get_hw_mode_change_from_hw_mode_index(
			mac->psoc, command->u.set_hw_mode_cmd.hw_mode_index);

	if (POLICY_MGR_HW_MODE_NOT_IN_PROGRESS == hw_mode) {
		sme_err("hw_mode %d, failing", hw_mode);
		goto fail;
	}

	policy_mgr_set_hw_mode_change_in_progress(mac->psoc, hw_mode);

	cmd->messageType = eWNI_SME_SET_HW_MODE_REQ;
	cmd->length = len;
	cmd->set_hw.hw_mode_index = command->u.set_hw_mode_cmd.hw_mode_index;
	cmd->set_hw.reason = command->u.set_hw_mode_cmd.reason;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same s_sir_set_hw_mode format.
	 */
	cmd->set_hw.set_hw_mode_cb = command->u.set_hw_mode_cmd.set_hw_mode_cb;

	sme_debug(
		"Posting set hw mode req to PE session:%d reason:%d",
		command->u.set_hw_mode_cmd.session_id,
		command->u.set_hw_mode_cmd.reason);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		policy_mgr_set_hw_mode_change_in_progress(mac->psoc,
			POLICY_MGR_HW_MODE_NOT_IN_PROGRESS);
		sme_err("Posting to PE failed");
		cmd = NULL;
		goto fail;
	}
	return;
fail:
	if (cmd)
		qdf_mem_free(cmd);
	param = qdf_mem_malloc(sizeof(*param));
	if (!param) {
		sme_err(
			"Malloc fail: Fail to send response to SME");
		return;
	}
	sme_debug("Sending set HW fail response to SME");
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	param->cfgd_hw_mode_index = 0;
	param->num_vdev_mac_entries = 0;
	msg.type = eWNI_SME_SET_HW_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_dual_mac_config() - Set HW mode command to PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_dual_mac_config(tpAniSirGlobal mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_dual_mac_cfg *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_dual_mac_config_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd) {
		sme_err("Memory allocation failed");
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;
	}

	cmd->message_type = eWNI_SME_SET_DUAL_MAC_CFG_REQ;
	cmd->length = len;
	cmd->set_dual_mac.scan_config = command->u.set_dual_mac_cmd.scan_config;
	cmd->set_dual_mac.fw_mode_config =
		command->u.set_dual_mac_cmd.fw_mode_config;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same sir_set_dual_mac_cfg format.
	 */
	cmd->set_dual_mac.set_dual_mac_cb =
		command->u.set_dual_mac_cmd.set_dual_mac_cb;

	sme_debug("Posting eWNI_SME_SET_DUAL_MAC_CFG_REQ to PE: %x %x",
		  cmd->set_dual_mac.scan_config,
		  cmd->set_dual_mac.fw_mode_config);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Posting to PE failed");
		goto fail;
	}
	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param) {
		sme_err(
			"Malloc fail: Fail to send response to SME");
		return;
	}
	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_antenna_mode() - Set antenna mode command to
 * PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_antenna_mode(tpAniSirGlobal mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_antenna_mode *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_antenna_mode_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set antenna mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd) {
		sme_err("Memory allocation failed");
		goto fail;
	}

	cmd->message_type = eWNI_SME_SET_ANTENNA_MODE_REQ;
	cmd->length = len;
	cmd->set_antenna_mode = command->u.set_antenna_mode_cmd;

	sme_debug(
		"Posting eWNI_SME_SET_ANTENNA_MODE_REQ to PE: %d %d",
		cmd->set_antenna_mode.num_rx_chains,
		cmd->set_antenna_mode.num_tx_chains);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Posting to PE failed");
		/*
		 * umac_send_mb_message_to_mac would've released the mem
		 * allocated by cmd.
		 */
		goto fail;
	}

	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param) {
		sme_err(
			"Malloc fail: Fail to send response to SME");
		return;
	}
	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_ANTENNA_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_ANTENNA_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_nss_update_req() - Update nss command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the nss update command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_nss_update_req(tpAniSirGlobal mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_nss_update_request *msg;
	QDF_STATUS status;
	struct scheduler_msg msg_return = {0};
	struct sir_bcn_update_rsp *param;
	struct csr_roam_session *session;


	if (!CSR_IS_SESSION_VALID(mac, command->sessionId)) {
		sme_err("Invalid session id %d", command->sessionId);
		goto fail;
	}
	session = CSR_GET_SESSION(mac, command->sessionId);

	len = sizeof(*msg);
	msg = qdf_mem_malloc(len);
	if (!msg) {
		sme_err("Memory allocation failed");
		/* Probably the fail response is also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;
	}

	msg->msgType = eWNI_SME_NSS_UPDATE_REQ;
	msg->msgLen = sizeof(*msg);

	msg->new_nss = command->u.nss_update_cmd.new_nss;
	msg->vdev_id = command->u.nss_update_cmd.session_id;

	sme_debug("Posting eWNI_SME_NSS_UPDATE_REQ to PE");

	status = umac_send_mb_message_to_mac(msg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	sme_err("Posting to PE failed");
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param) {
		sme_err(
			"Malloc fail: Fail to send response to SME");
		return;
	}
	sme_err("Sending nss update fail response to SME");
	param->status = QDF_STATUS_E_FAILURE;
	param->vdev_id = command->u.nss_update_cmd.session_id;
	param->reason = REASON_NSS_UPDATE;
	msg_return.type = eWNI_SME_NSS_UPDATE_RSP;
	msg_return.bodyptr = param;
	msg_return.bodyval = 0;
	sys_process_mmh_msg(mac, &msg_return);
}
#ifdef FEATURE_WLAN_TDLS
/**
 * csr_roam_fill_tdls_info() - Fill TDLS information
 * @roam_info: Roaming information buffer
 * @join_rsp: Join response which has TDLS info
 *
 * Return: None
 */
void csr_roam_fill_tdls_info(tpAniSirGlobal mac_ctx,
			     struct csr_roam_info *roam_info,
			     tpSirSmeJoinRsp join_rsp)
{
	roam_info->tdls_prohibited = join_rsp->tdls_prohibited;
	roam_info->tdls_chan_swit_prohibited =
		join_rsp->tdls_chan_swit_prohibited;
	sme_debug(
		"tdls:prohibit: %d, chan_swit_prohibit: %d",
		roam_info->tdls_prohibited,
		roam_info->tdls_chan_swit_prohibited);
}
#endif

#if defined(WLAN_FEATURE_FILS_SK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void csr_copy_fils_join_rsp_roam_info(struct csr_roam_info *roam_info,
				      roam_offload_synch_ind *roam_synch_data)
{
	struct fils_join_rsp_params *roam_fils_info;

	roam_info->fils_join_rsp = qdf_mem_malloc(sizeof(*roam_fils_info));
	if (!roam_info->fils_join_rsp) {
		sme_err("fils_join_rsp malloc fails!");
		return;
	}

	roam_fils_info = roam_info->fils_join_rsp;
	cds_copy_hlp_info(&roam_synch_data->dst_mac,
			&roam_synch_data->src_mac,
			roam_synch_data->hlp_data_len,
			roam_synch_data->hlp_data,
			&roam_fils_info->dst_mac,
			&roam_fils_info->src_mac,
			&roam_fils_info->hlp_data_len,
			roam_fils_info->hlp_data);
}

static void
csr_update_fils_erp_seq_num(struct csr_roam_profile *roam_info,
			    uint16_t erp_next_seq_num)
{
	if (roam_info->fils_con_info)
		roam_info->fils_con_info->sequence_number = erp_next_seq_num;
}
#else
static inline
void csr_copy_fils_join_rsp_roam_info(struct csr_roam_info *roam_info,
				      roam_offload_synch_ind *roam_synch_data)
{}

static inline
void csr_update_fils_erp_seq_num(struct csr_roam_profile *roam_info,
				 uint16_t erp_next_seq_num)
{}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_is_sae_single_pmk_vsie_ap() - check if the Roamed AP supports sae
 * roaming using single pmk connection
 * with same pmk or not
 * @bss_des: bss descriptor
 *
 * Return: True if same pmk IE is present
 */
static bool csr_is_sae_single_pmk_vsie_ap(struct bss_description *bss_des)
{
	uint16_t ie_len;
	const uint8_t *vendor_ie;

	if (!bss_des) {
		sme_debug("Invalid bss description");
		return false;
	}
	ie_len = csr_get_ielen_from_bss_description(bss_des);

	vendor_ie =
		wlan_get_vendor_ie_ptr_from_oui(CSR_SINGLE_PMK_OUI,
						CSR_SINGLE_PMK_OUI_SIZE,
						(uint8_t *)bss_des->ieFields,
						ie_len);

	if (!vendor_ie)
		return false;

	sme_debug("AP supports sae single pmk feature");

	return true;
}

/**
 * csr_check_and_set_sae_single_pmk_cap() - check if the Roamed AP support
 * roaming using single pmk with single pmk or not
 * @session: Session
 *
 * Return: True if same pmk IE is present
 */
static void
csr_check_and_set_sae_single_pmk_cap(struct csr_roam_session *session)
{
	session->single_pmk_info.sae_single_pmk_ap =
		csr_is_sae_single_pmk_vsie_ap(session->pConnectBssDesc);
}
#else
static inline void
csr_check_and_set_sae_single_pmk_cap(struct csr_roam_session *session)
{
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static QDF_STATUS csr_process_roam_sync_callback(tpAniSirGlobal mac_ctx,
		roam_offload_synch_ind *roam_synch_data,
		tpSirBssDescription bss_desc, enum sir_roam_op_code reason)
{
	uint8_t session_id = roam_synch_data->roamedVdevId;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	tDot11fBeaconIEs *ies_local = NULL;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct csr_roam_info *roam_info;
	tCsrRoamConnectedProfile *conn_profile = NULL;
	sme_QosAssocInfo assoc_info;
	tpAddBssParams add_bss_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tPmkidCacheInfo pmkid_cache;
	uint32_t pmkid_index;
	uint16_t len;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	tSirSmeHTProfile *src_profile = NULL;
	tCsrRoamHTProfile *dst_profile = NULL;
#endif
	tpCsrNeighborRoamControlInfo neigh_roam_info =
				&mac_ctx->roam.neighborRoamInfo[session_id];
	uint32_t chan_id;
	bool abort_host_scan_cap = false;
	wlan_scan_id scan_id;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_invoke_entity_param *vdev_roam_params;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("vdev is NULL, aborting roam invoke");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);

	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting roam invoke");
		status = QDF_STATUS_E_NULL_VALUE;
		goto end;
	}

	if (!session) {
		sme_err("LFR3: Session not found");
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	sme_debug("LFR3: reason: %d roaming in progress %d", reason,
		  vdev_roam_params->roam_invoke_in_progress);
	switch (reason) {
	case SIR_ROAMING_DEREGISTER_STA:
		/*
		 * The following is the first thing done in CSR
		 * after receiving RSI. Hence stopping the timer here.
		 */
		csr_roam_roaming_offload_timer_action(mac_ctx,
				0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		if (session->discon_in_progress ||
		    (neigh_roam_info->last_sent_cmd ==
		     ROAM_SCAN_OFFLOAD_STOP &&
		     !vdev_roam_params->roam_invoke_in_progress) ||
		    !CSR_IS_ROAM_JOINED(mac_ctx, session_id)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				FL("LFR3: Session not in connected state or disconnect is in progress %d"),
				session->discon_in_progress);
			status = QDF_STATUS_E_FAILURE;
			goto end;
		}
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_FT_START, eCSR_ROAM_RESULT_SUCCESS);
		goto end;
	case SIR_ROAMING_START:
		csr_roam_roaming_offload_timer_action(mac_ctx,
				CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD, session_id,
				ROAMING_OFFLOAD_TIMER_START);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_START, eCSR_ROAM_RESULT_SUCCESS);
		/*
		 * For emergency deauth roaming, firmware sends ROAM start
		 * instead of ROAM scan start notification as data path queues
		 * will be stopped only during roam start notification.
		 * This is because, for deauth/disassoc triggered roam, the
		 * AP has sent deauth, and packets shouldn't be sent to AP
		 * after that. Since firmware is sending roam start directly
		 * host sends scan abort during roam scan, but in other
		 * triggers, the host receives roam start after candidate
		 * selection and roam scan is complete. So when host sends
		 * roam abort for emergency deauth roam trigger, the firmware
		 * roam scan is also aborted. This results in roaming failure.
		 * So send scan_id as CANCEL_HOST_SCAN_ID to scan module to
		 * abort only host triggered scans.
		 */
		abort_host_scan_cap = mac_ctx->stop_all_host_scan_support;
		if (abort_host_scan_cap)
			scan_id = CANCEL_HOST_SCAN_ID;
		else
			scan_id = INVAL_SCAN_ID;

		wlan_abort_scan(mac_ctx->pdev, INVAL_PDEV_ID,
				session_id, scan_id, false);
		goto end;
	case SIR_ROAMING_ABORT:
		csr_roam_roaming_offload_timer_action(mac_ctx,
				0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_ABORT, eCSR_ROAM_RESULT_SUCCESS);
		vdev_roam_params->roam_invoke_in_progress = false;
		goto end;
	case SIR_ROAM_SYNCH_NAPI_OFF:
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_NAPI_OFF, eCSR_ROAM_RESULT_SUCCESS);
		goto end;
	case SIR_ROAMING_INVOKE_FAIL:
		/* Userspace roam request failed, disconnect with current AP */
		sme_debug("LFR3: roam invoke from user-space fail, dis cur AP");
		csr_roam_disconnect(mac_ctx, session_id,
				    eCSR_DISCONNECT_REASON_DEAUTH,
				    eSIR_MAC_USER_TRIGGERED_ROAM_FAILURE);
		vdev_roam_params->roam_invoke_in_progress = false;
		goto end;
	case SIR_ROAM_SYNCH_PROPAGATION:
		break;
	case SIR_ROAM_SYNCH_COMPLETE:
		/* Handle one race condition that if candidate is already
		 *selected & FW has gone ahead with roaming or about to go
		 * ahead when set_band comes, it will be complicated for FW
		 * to stop the current roaming. Instead, host will check the
		 * roam sync to make sure the new AP is on 2G, or disconnect
		 * the AP.
		 */
		chan_id = wlan_reg_freq_to_chan(mac_ctx->pdev,
						roam_synch_data->chan_freq);
		if (wlan_reg_is_disable_ch(mac_ctx->pdev, chan_id)) {
			csr_roam_disconnect(mac_ctx, session_id,
					    eCSR_DISCONNECT_REASON_DEAUTH,
					    eSIR_MAC_OPER_CHANNEL_BAND_CHANGE);
			sme_debug("Roaming Failed for disabled channel or band");
			vdev_roam_params->roam_invoke_in_progress = false;
			goto end;
		}
		/*
		 * Following operations need to be done once roam sync
		 * completion is sent to FW, hence called here:
		 * 1) Firmware has already updated DBS policy. Update connection
		 *    table in the host driver.
		 * 2) Force SCC switch if needed
		 * 3) Set connection in progress = false
		 */
		/* first update connection info from wma interface */
		policy_mgr_update_connection_info(mac_ctx->psoc, session_id);
		/* then update remaining parameters from roam sync ctx */
		sme_debug("Update DBS hw mode");
		policy_mgr_hw_mode_transition_cb(
			roam_synch_data->hw_mode_trans_ind.old_hw_mode_index,
			roam_synch_data->hw_mode_trans_ind.new_hw_mode_index,
			roam_synch_data->hw_mode_trans_ind.num_vdev_mac_entries,
			roam_synch_data->hw_mode_trans_ind.vdev_mac_map,
			mac_ctx->psoc);
		mac_ctx->sme.set_connection_info_cb(false);
		session->roam_synch_in_progress = false;

		if (ucfg_pkt_capture_get_pktcap_mode())
			ucfg_pkt_capture_record_channel();

		csr_check_and_set_sae_single_pmk_cap(session);

		if (WLAN_REG_IS_5GHZ_CH(bss_desc->channelId)) {
			session->disable_hi_rssi = true;
			sme_debug("Disabling HI_RSSI, AP channel=%d, rssi=%d",
				  bss_desc->channelId, bss_desc->rssi);
		} else {
			session->disable_hi_rssi = false;
		}

		policy_mgr_check_concurrent_intf_and_restart_sap(mac_ctx->psoc);
		if (roam_synch_data->authStatus ==
		    CSR_ROAM_AUTH_STATUS_AUTHENTICATED)
			csr_roam_offload_scan(mac_ctx, session_id,
					      ROAM_SCAN_OFFLOAD_UPDATE_CFG,
					      REASON_CONNECT);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				       eCSR_ROAM_SYNCH_COMPLETE,
				       eCSR_ROAM_RESULT_SUCCESS);
		vdev_roam_params->roam_invoke_in_progress = false;
		goto end;
	case SIR_ROAMING_DEAUTH:
		sme_debug("LFR3: callback reason %d", reason);
		csr_roam_roaming_offload_timer_action(
			mac_ctx, 0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		goto end;
	default:
		sme_debug("LFR3: callback reason %d", reason);
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}
	session->roam_synch_in_progress = true;
	session->roam_synch_data = roam_synch_data;
	status = csr_get_parsed_bss_description_ies(
			mac_ctx, bss_desc, &ies_local);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("LFR3: fail to parse IEs");
		session->roam_synch_in_progress = false;
		vdev_roam_params->roam_invoke_in_progress = false;
		goto end;
	}

	conn_profile = &session->connectedProfile;
	csr_roam_stop_network(mac_ctx, session_id, session->pCurRoamProfile,
			      bss_desc, ies_local);
	ps_global_info->remain_in_power_active_till_dhcp = false;
	session->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;

	/* Remove old BSSID mlme info from scan cache */
	csr_update_scan_entry_associnfo(mac_ctx, session,
					SCAN_ENTRY_CON_STATE_NONE);
	roam_info = qdf_mem_malloc(sizeof(struct csr_roam_info));
	if (NULL == roam_info) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("LFR3: Mem Alloc failed for roam info"));
		session->roam_synch_in_progress = false;
		vdev_roam_params->roam_invoke_in_progress = false;
		qdf_mem_free(ies_local);
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	csr_scan_save_roam_offload_ap_to_scan_cache(mac_ctx, roam_synch_data,
						    bss_desc);
	roam_info->sessionId = session_id;

	qdf_mem_copy(&roam_info->bssid.bytes, &bss_desc->bssId,
			sizeof(struct qdf_mac_addr));
	csr_roam_save_connected_information(mac_ctx, session_id,
			session->pCurRoamProfile,
			bss_desc,
			ies_local);
	/* Add new mlme info to new BSSID after upting connectedProfile */
	csr_update_scan_entry_associnfo(mac_ctx, session,
					SCAN_ENTRY_CON_STATE_ASSOC);
	csr_roam_save_security_rsp_ie(mac_ctx, session_id,
			session->pCurRoamProfile->negotiatedAuthType,
			bss_desc, ies_local);

#ifdef FEATURE_WLAN_ESE
	roam_info->isESEAssoc = conn_profile->isESEAssoc;
#endif

	/*
	 * Encryption keys for new connection are obtained as follows:
	 * authStatus = CSR_ROAM_AUTH_STATUS_AUTHENTICATED
	 * Open - No keys required.
	 * Static WEP - Firmware copies keys from old AP to new AP.
	 * Fast roaming authentications e.g. PSK, FT, CCKM - firmware
	 *      supplicant obtains them through 4-way handshake.
	 *
	 * authStatus = CSR_ROAM_AUTH_STATUS_CONNECTED
	 * All other authentications - Host supplicant performs EAPOL
	 *      with AP after this point and sends new keys to the driver.
	 *      Driver starts wait_for_key timer for that purpose.
	 */
	if (roam_synch_data->authStatus
				== CSR_ROAM_AUTH_STATUS_AUTHENTICATED) {
		QDF_TRACE(QDF_MODULE_ID_SME,
				QDF_TRACE_LEVEL_DEBUG,
				FL("LFR3:Don't start waitforkey timer"));
		csr_roam_substate_change(mac_ctx,
				eCSR_ROAM_SUBSTATE_NONE, session_id);
		/*
		 * If authStatus is AUTHENTICATED, then we have done successful
		 * 4 way handshake in FW using the cached PMKID.
		 * However, the session->psk_pmk has the PMK of the older AP
		 * as set_key is not received from supplicant.
		 * When any RSO command is sent for the current AP, the older
		 * AP's PMK is sent to the FW which leads to incorrect PMK and
		 * leads to 4 way handshake failure when roaming happens to
		 * this AP again.
		 * Check if a PMK cache exists for the roamed AP and update
		 * it into the session pmk.
		 */
		qdf_mem_zero(&pmkid_cache, sizeof(pmkid_cache));
		qdf_copy_macaddr(&pmkid_cache.BSSID,
				 &session->connectedProfile.bssid);
		sme_debug("Trying to find PMKID for " QDF_MAC_ADDR_STR,
			  QDF_MAC_ADDR_ARRAY(pmkid_cache.BSSID.bytes));
		if (csr_lookup_pmkid_using_bssid(mac_ctx, session,
						 &pmkid_cache,
						 &pmkid_index)) {
			session->pmk_len =
				session->PmkidCacheInfo[pmkid_index].pmk_len;
			qdf_mem_zero(session->psk_pmk,
				     sizeof(session->psk_pmk));
			qdf_mem_copy(session->psk_pmk,
				     session->PmkidCacheInfo[pmkid_index].pmk,
				     session->pmk_len);
			sme_debug("pmkid found for " QDF_MAC_ADDR_STR " at %d len %d",
				  QDF_MAC_ADDR_ARRAY(pmkid_cache.BSSID.bytes),
				  pmkid_index, (uint32_t)session->pmk_len);
		} else {
			sme_debug("PMKID Not found in cache for " QDF_MAC_ADDR_STR,
				  QDF_MAC_ADDR_ARRAY(pmkid_cache.BSSID.bytes));
			if (roam_synch_data->pmk_len) {
				qdf_mem_copy(&pmkid_cache.PMKID,
					     roam_synch_data->pmkid,
					     CSR_RSN_PMKID_SIZE);
				qdf_mem_copy(&pmkid_cache.pmk,
					     roam_synch_data->pmk,
					     roam_synch_data->pmk_len);
				pmkid_cache.pmk_len = roam_synch_data->pmk_len;

				csr_update_pmk_cache(session, &pmkid_cache);
			}
		}
	} else {
		roam_info->fAuthRequired = true;
		csr_roam_substate_change(mac_ctx,
				eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY,
				session_id);

		ps_global_info->remain_in_power_active_till_dhcp = true;
		mac_ctx->roam.WaitForKeyTimerInfo.vdev_id = session_id;
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_start_wait_for_key_timer(
				mac_ctx, CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD))
		   ) {
			sme_err("Failed wait for key timer start");
			csr_roam_substate_change(mac_ctx,
					eCSR_ROAM_SUBSTATE_NONE,
					session_id);
		}
		csr_neighbor_roam_state_transition(mac_ctx,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, session_id);
	}
	roam_info->nBeaconLength = 0;
	roam_info->nAssocReqLength = roam_synch_data->reassoc_req_length -
		SIR_MAC_HDR_LEN_3A - SIR_MAC_REASSOC_SSID_OFFSET;
	roam_info->nAssocRspLength = roam_synch_data->reassocRespLength -
		SIR_MAC_HDR_LEN_3A;
	roam_info->pbFrames = qdf_mem_malloc(roam_info->nBeaconLength +
		roam_info->nAssocReqLength + roam_info->nAssocRspLength);
	if (NULL == roam_info->pbFrames) {
		sme_err("no memory available");
		session->roam_synch_in_progress = false;
		vdev_roam_params->roam_invoke_in_progress = false;
		if (roam_info)
			qdf_mem_free(roam_info);
		qdf_mem_free(ies_local);
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	qdf_mem_copy(roam_info->pbFrames,
			(uint8_t *)roam_synch_data +
			roam_synch_data->reassoc_req_offset +
			SIR_MAC_HDR_LEN_3A + SIR_MAC_REASSOC_SSID_OFFSET,
			roam_info->nAssocReqLength);
	qdf_mem_copy(roam_info->pbFrames + roam_info->nAssocReqLength,
			(uint8_t *)roam_synch_data +
			roam_synch_data->reassocRespOffset +
			SIR_MAC_HDR_LEN_3A,
			roam_info->nAssocRspLength);

	QDF_TRACE(QDF_MODULE_ID_SME,
			QDF_TRACE_LEVEL_DEBUG,
			FL("LFR3:Clear Connected info"));
	csr_roam_free_connected_info(mac_ctx,
			&session->connectedInfo);
	len = roam_synch_data->join_rsp->parsedRicRspLen;

#ifdef FEATURE_WLAN_ESE
	len += roam_synch_data->join_rsp->tspecIeLen;
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("LFR3: tspecLen %d"),
		roam_synch_data->join_rsp->tspecIeLen);
#endif

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("LFR3: RIC length - %d"),
		roam_synch_data->join_rsp->parsedRicRspLen);
	if (len) {
		session->connectedInfo.pbFrames =
			qdf_mem_malloc(len);
		if (session->connectedInfo.pbFrames != NULL) {
			qdf_mem_copy(session->connectedInfo.pbFrames,
				roam_synch_data->join_rsp->frames, len);
			session->connectedInfo.nRICRspLength =
				roam_synch_data->join_rsp->parsedRicRspLen;

#ifdef FEATURE_WLAN_ESE
			session->connectedInfo.nTspecIeLength =
				roam_synch_data->join_rsp->tspecIeLen;
#endif
		}
	}
	conn_profile->vht_channel_width =
		roam_synch_data->join_rsp->vht_channel_width;
	add_bss_params = (tpAddBssParams)roam_synch_data->add_bss_params;
	session->connectedInfo.staId = add_bss_params->staContext.staIdx;
	roam_info->staId = session->connectedInfo.staId;
	roam_info->timingMeasCap =
		roam_synch_data->join_rsp->timingMeasCap;
	roam_info->chan_info.nss = roam_synch_data->join_rsp->nss;
	roam_info->chan_info.rate_flags =
		roam_synch_data->join_rsp->max_rate_flags;
	roam_info->chan_info.ch_width =
		roam_synch_data->join_rsp->vht_channel_width;
	csr_roam_fill_tdls_info(mac_ctx, roam_info, roam_synch_data->join_rsp);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	src_profile = &roam_synch_data->join_rsp->HTProfile;
	dst_profile = &conn_profile->HTProfile;
	if (mac_ctx->roam.configParam.cc_switch_mode
			!= QDF_MCC_TO_SCC_SWITCH_DISABLE)
		csr_roam_copy_ht_profile(dst_profile,
				src_profile);
#endif
	assoc_info.pBssDesc = bss_desc;
	roam_info->statusCode = eSIR_SME_SUCCESS;
	roam_info->reasonCode = eSIR_SME_SUCCESS;
	assoc_info.pProfile = session->pCurRoamProfile;
	mac_ctx->roam.roamSession[session_id].connectState =
		eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_HANDOFF_ASSOC_REQ, NULL);
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_REASSOC_REQ, NULL);
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
	mac_ctx->roam.roamSession[session_id].connectState =
		eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_REASSOC_COMPLETE, &assoc_info);
	roam_info->pBssDesc = bss_desc;
	conn_profile->acm_mask = sme_qos_get_acm_mask(mac_ctx,
			bss_desc, NULL);
	if (conn_profile->modifyProfileFields.uapsd_mask) {
		sme_debug(
				" uapsd_mask (0x%X) set, request UAPSD now",
				conn_profile->modifyProfileFields.uapsd_mask);
		sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), session_id);
	}
	conn_profile->dot11Mode = session->bssParams.uCfgDot11Mode;
	roam_info->u.pConnectedProfile = conn_profile;

	sme_debug(
		"vht ch width %d staId %d nss %d rate_flag %d dot11Mode %d",
		conn_profile->vht_channel_width,
		roam_info->staId,
		roam_info->chan_info.nss,
		roam_info->chan_info.rate_flags,
		conn_profile->dot11Mode);

	if (!IS_FEATURE_SUPPORTED_BY_FW
			(SLM_SESSIONIZATION) &&
			(csr_is_concurrent_session_running(mac_ctx))) {
		mac_ctx->roam.configParam.doBMPSWorkaround = 1;
	}
	roam_info->roamSynchInProgress = true;
	roam_info->synchAuthStatus = roam_synch_data->authStatus;
	roam_info->kck_len = roam_synch_data->kck_len;
	roam_info->kek_len = roam_synch_data->kek_len;
	roam_info->pmk_len = roam_synch_data->pmk_len;
	qdf_mem_copy(roam_info->kck, roam_synch_data->kck, roam_info->kck_len);
	qdf_mem_copy(roam_info->kek, roam_synch_data->kek, roam_info->kek_len);

	if (roam_synch_data->pmk_len)
		qdf_mem_copy(roam_info->pmk, roam_synch_data->pmk,
			     roam_synch_data->pmk_len);

	qdf_mem_copy(roam_info->pmkid, roam_synch_data->pmkid, SIR_PMKID_LEN);
	roam_info->update_erp_next_seq_num =
			roam_synch_data->update_erp_next_seq_num;
	roam_info->next_erp_seq_num = roam_synch_data->next_erp_seq_num;
	csr_update_fils_erp_seq_num(session->pCurRoamProfile,
				    roam_info->next_erp_seq_num);
	sme_debug("Update ERP Seq Num : %d, Next ERP Seq Num : %d",
			roam_info->update_erp_next_seq_num,
			roam_info->next_erp_seq_num);
	qdf_mem_copy(roam_info->replay_ctr, roam_synch_data->replay_ctr,
			SIR_REPLAY_CTR_LEN);
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("LFR3: Copy KCK, KEK(len %d) and Replay Ctr"),
		roam_info->kek_len);
	/* bit-4 and bit-5 indicate the subnet status */
	roam_info->subnet_change_status =
		CSR_GET_SUBNET_STATUS(roam_synch_data->roamReason);

	/* fetch 4 LSB to get roam reason */
	roam_info->roam_reason = roam_synch_data->roamReason &
				 ROAM_REASON_MASK;
	sme_debug("Update roam reason : %d", roam_info->roam_reason);
	csr_copy_fils_join_rsp_roam_info(roam_info, roam_synch_data);

	csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
		eCSR_ROAM_ASSOCIATION_COMPLETION, eCSR_ROAM_RESULT_ASSOCIATED);
	csr_reset_pmkid_candidate_list(mac_ctx, session_id);
#ifdef FEATURE_WLAN_WAPI
	csr_reset_bkid_candidate_list(mac_ctx, session_id);
#endif
	if (!CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id)) {
		QDF_TRACE(QDF_MODULE_ID_SME,
				QDF_TRACE_LEVEL_DEBUG,
				FL
				("NO CSR_IS_WAIT_FOR_KEY -> csr_roam_link_up"));
		csr_roam_link_up(mac_ctx, conn_profile->bssid);
	}

	session->fRoaming = false;
	session->roam_synch_in_progress = false;
	vdev_roam_params->roam_invoke_in_progress = false;
	sme_free_join_rsp_fils_params(roam_info);
	qdf_mem_free(roam_info->pbFrames);
	qdf_mem_free(roam_info);
	qdf_mem_free(ies_local);

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	return status;
}

/**
 * csr_roam_synch_callback() - SME level callback for roam synch propagation
 * @mac_ctx: MAC Context
 * @roam_synch_data: Roam synch data buffer pointer
 * @bss_desc: BSS descriptor pointer
 * @reason: Reason for calling the callback
 *
 * This callback is registered with WMA and used after roaming happens in
 * firmware and the call to this routine completes the roam synch
 * propagation at both CSR and HDD levels. The HDD level propagation
 * is achieved through the already defined callback for assoc completion
 * handler.
 *
 * Return: Success or Failure.
 */
QDF_STATUS csr_roam_synch_callback(tpAniSirGlobal mac_ctx,
		roam_offload_synch_ind *roam_synch_data,
		tpSirBssDescription  bss_desc, enum sir_roam_op_code reason)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("LFR3: Locking failed, bailing out");
		return status;
	}

	status = csr_process_roam_sync_callback(mac_ctx, roam_synch_data,
					    bss_desc, reason);

	sme_release_global_lock(&mac_ctx->sme);

	return status;
}

QDF_STATUS
csr_process_roam_auth_offload_callback(tpAniSirGlobal mac_ctx,
				       uint8_t vdev_id,
				       struct qdf_mac_addr roam_bssid)
{
	struct csr_roam_info *roam_info;
	struct sir_sae_info sae_info;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (!session) {
		sme_err("WPA3 Preauth event with invalid session id:%d",
			vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_FAILURE;

	sae_info.msg_len = sizeof(sae_info);
	sae_info.vdev_id = vdev_id;

	sae_info.ssid.length = session->connectedProfile.SSID.length;
	qdf_mem_copy(sae_info.ssid.ssId, session->connectedProfile.SSID.ssId,
		     sae_info.ssid.length);

	qdf_mem_copy(sae_info.peer_mac_addr.bytes,
		     roam_bssid.bytes, QDF_MAC_ADDR_SIZE);

	roam_info->sae_info = &sae_info;

	csr_roam_call_callback(mac_ctx, vdev_id, roam_info, 0,
			       eCSR_ROAM_SAE_COMPUTE, eCSR_ROAM_RESULT_NONE);

	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
csr_roam_auth_offload_callback(tpAniSirGlobal mac_ctx,
			       uint8_t vdev_id, struct qdf_mac_addr bssid)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = csr_process_roam_auth_offload_callback(mac_ctx, vdev_id, bssid);

	sme_release_global_lock(&mac_ctx->sme);

	return status;

}
#endif

QDF_STATUS csr_update_owe_info(tpAniSirGlobal mac,
			       tSirSmeAssocInd *assoc_ind)
{
	uint32_t session_id = CSR_SESSION_ID_INVALID;
	QDF_STATUS status;

	status = csr_roam_get_session_id_from_bssid(mac,
					(struct qdf_mac_addr *)assoc_ind->bssId,
					&session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		return QDF_STATUS_E_FAILURE;
	}

	/* Send Association completion message to PE */
	if (assoc_ind->owe_status)
		status = QDF_STATUS_E_INVAL;
	status = csr_send_assoc_cnf_msg(mac, assoc_ind, status,
					assoc_ind->owe_status);
	/*
	 * send a message to CSR itself just to avoid the EAPOL frames
	 * going OTA before association response
	 */
	if (assoc_ind->owe_status == 0)
		status = csr_send_assoc_ind_to_upper_layer_cnf_msg(mac,
								   assoc_ind,
								   status,
								   session_id);

	return status;
}
