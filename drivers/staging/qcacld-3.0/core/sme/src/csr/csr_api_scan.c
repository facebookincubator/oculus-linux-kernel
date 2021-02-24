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
 * DOC: csr_api_scan.c
 *
 * Implementation for the Common Scan interfaces.
 */

#include "ani_global.h"

#include "csr_inside_api.h"
#include "sme_inside.h"

#include "csr_support.h"

#include "host_diag_core_log.h"
#include "host_diag_core_event.h"

#include "cds_reg_service.h"
#include "wma_types.h"
#include "cds_utils.h"
#include "wma.h"

#include "wlan_policy_mgr_api.h"
#include "wlan_hdd_main.h"
#include "pld_common.h"
#include "csr_internal.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_scan_api.h>
#include <wlan_scan_utils_api.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_utility.h>
#include "wlan_reg_services_api.h"
#include "sch_api.h"
#include "wlan_blm_api.h"

static void csr_set_cfg_valid_channel_list(struct mac_context *mac,
					   uint8_t *pChannelList,
					   uint8_t NumChannels);

static void csr_save_tx_power_to_cfg(struct mac_context *mac,
				     tDblLinkList *pList,
				     uint32_t cfgId);

static void csr_set_cfg_country_code(struct mac_context *mac,
				     uint8_t *countryCode);

static void csr_purge_channel_power(struct mac_context *mac,
				    tDblLinkList *pChannelList);

static bool csr_roam_is_valid_channel(struct mac_context *mac, uint8_t channel);

/* pResult is invalid calling this function. */
void csr_free_scan_result_entry(struct mac_context *mac,
				struct tag_csrscan_result *pResult)
{
	if (pResult->Result.pvIes)
		qdf_mem_free(pResult->Result.pvIes);

	qdf_mem_free(pResult);
}

static QDF_STATUS csr_ll_scan_purge_result(struct mac_context *mac,
					   tDblLinkList *pList)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tListElem *pEntry;
	struct tag_csrscan_result *bss_desc;

	while ((pEntry = csr_ll_remove_head(pList, LL_ACCESS_NOLOCK)) != NULL) {
		bss_desc = GET_BASE_ADDR(pEntry, struct tag_csrscan_result,
					 Link);
		csr_free_scan_result_entry(mac, bss_desc);
	}

	return status;
}

QDF_STATUS csr_scan_open(struct mac_context *mac_ctx)
{
	csr_ll_open(&mac_ctx->scan.channelPowerInfoList24);
	csr_ll_open(&mac_ctx->scan.channelPowerInfoList5G);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_scan_close(struct mac_context *mac)
{
	csr_purge_channel_power(mac, &mac->scan.channelPowerInfoList24);
	csr_purge_channel_power(mac, &mac->scan.channelPowerInfoList5G);
	csr_ll_close(&mac->scan.channelPowerInfoList24);
	csr_ll_close(&mac->scan.channelPowerInfoList5G);
	ucfg_scan_psoc_set_disable(mac->psoc, REASON_SYSTEM_DOWN);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_scan_handle_search_for_ssid(struct mac_context *mac_ctx,
					   uint32_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tScanResultHandle hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
	tCsrScanResultFilter *pScanFilter = NULL;
	struct csr_roam_profile *profile;
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("session %d not found", session_id);
		return QDF_STATUS_E_FAILURE;
	}
	profile = session->scan_info.profile;
	sme_debug("session %d", session_id);
	do {
		/* If this scan is for HDD reassociate */
		if (mac_ctx->roam.neighborRoamInfo[session_id].
				uOsRequestedHandoff) {
			/* notify LFR state m/c */
			status = csr_neighbor_roam_sssid_scan_done
				(mac_ctx, session_id, QDF_STATUS_SUCCESS);
			if (!QDF_IS_STATUS_SUCCESS(status))
				csr_neighbor_roam_start_lfr_scan(mac_ctx,
								 session_id);
			status = QDF_STATUS_SUCCESS;
			break;
		}
		/*
		 * If there is roam command waiting, ignore this roam because
		 * the newer roam command is the one to execute
		 */
		if (csr_is_roam_command_waiting_for_session(mac_ctx, session_id)) {
			sme_warn("aborts because roam command waiting");
			break;
		}
		if (!profile)
			break;
		pScanFilter = qdf_mem_malloc(sizeof(tCsrScanResultFilter));
		if (!pScanFilter) {
			status = QDF_STATUS_E_NOMEM;
			break;
		}
		status = csr_roam_prepare_filter_from_profile(mac_ctx, profile,
							      pScanFilter);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		status = csr_scan_get_result(mac_ctx, pScanFilter, &hBSSList);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		if (mac_ctx->roam.roamSession[session_id].connectState ==
				eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING) {
			sme_err("upper layer issued disconnetion");
			status = QDF_STATUS_E_FAILURE;
			break;
		}
		status = csr_roam_issue_connect(mac_ctx, session_id, profile,
						hBSSList, eCsrHddIssued,
						session->scan_info.roam_id,
						true, true);
		hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
	} while (0);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		if (CSR_INVALID_SCANRESULT_HANDLE != hBSSList) {
			csr_scan_result_purge(mac_ctx, hBSSList);
		}
		/* We haven't done anything to this profile */
		csr_roam_call_callback(mac_ctx, session_id, NULL,
				       session->scan_info.roam_id,
				       eCSR_ROAM_ASSOCIATION_FAILURE,
				       eCSR_ROAM_RESULT_SCAN_FOR_SSID_FAILURE);
	}
	if (pScanFilter) {
		csr_free_scan_filter(mac_ctx, pScanFilter);
		qdf_mem_free(pScanFilter);
	}
	return status;
}

/**
 * csr_handle_fils_scan_for_ssid_failure() - Checks and fills FILS seq number
 * in roam_info structure to send to hdd
 *
 * @roam_profile: Pointer to current roam_profile structure
 * @roam_info: Pointer to roam_info strucure to be filled
 *
 * Return: true for FILS connection else false
 */
#ifdef WLAN_FEATURE_FILS_SK
static bool
csr_handle_fils_scan_for_ssid_failure(struct csr_roam_profile *roam_profile,
				      struct csr_roam_info *roam_info)
{
	if (roam_profile && roam_profile->fils_con_info &&
	    roam_profile->fils_con_info->is_fils_connection) {
		sme_debug("send roam_info for FILS connection failure, seq %d",
			  roam_profile->fils_con_info->sequence_number);
		roam_info->is_fils_connection = true;
		roam_info->fils_seq_num =
				roam_profile->fils_con_info->sequence_number;
		return true;
	}

	return false;
}
#else
static bool
csr_handle_fils_scan_for_ssid_failure(struct csr_roam_profile *roam_profile,
				      struct csr_roam_info *roam_info)
{
	return false;
}
#endif

QDF_STATUS csr_scan_handle_search_for_ssid_failure(struct mac_context *mac_ctx,
						   uint32_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_profile *profile;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	eCsrRoamResult roam_result;
	struct csr_roam_info *roam_info = NULL;
	struct tag_csrscan_result *scan_result;

	if (!session) {
		sme_err("session %d not found", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* If this scan is for HDD reassociate */
	if (mac_ctx->roam.neighborRoamInfo[session_id].uOsRequestedHandoff) {
		/* notify LFR state m/c */
		status = csr_neighbor_roam_sssid_scan_done
				(mac_ctx, session_id, QDF_STATUS_E_FAILURE);
		if (!QDF_IS_STATUS_SUCCESS(status))
			csr_neighbor_roam_start_lfr_scan(mac_ctx, session_id);
		return QDF_STATUS_SUCCESS;
	}

	profile = session->scan_info.profile;

	/*
	 * Check whether it is for start ibss. No need to do anything if it
	 * is a JOIN request
	 */
	if (profile && CSR_IS_START_IBSS(profile)) {
		status = csr_roam_issue_connect(mac_ctx, session_id, profile, NULL,
				eCsrHddIssued, session->scan_info.roam_id,
				true, true);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("failed to issue startIBSS, session_id %d status: 0x%08X roam id %d",
				session_id, status, session->scan_info.roam_id);
			csr_roam_call_callback(mac_ctx, session_id, NULL,
				session->scan_info.roam_id, eCSR_ROAM_FAILED,
				eCSR_ROAM_RESULT_FAILURE);
		}
		return status;
	}
	roam_result = eCSR_ROAM_RESULT_FAILURE;
	if (profile && csr_is_bss_type_ibss(profile->BSSType)) {
		roam_result = eCSR_ROAM_RESULT_IBSS_START_FAILED;
		goto roam_completion;
	}

	roam_info = qdf_mem_malloc(sizeof(struct csr_roam_info));
	if (!roam_info)
		goto roam_completion;

	if (session->scan_info.roambssentry) {
		scan_result = GET_BASE_ADDR(session->scan_info.roambssentry,
				struct tag_csrscan_result, Link);
		roam_info->bss_desc = &scan_result->Result.BssDescriptor;
	}
	roam_info->status_code = session->joinFailStatusCode.status_code;
	roam_info->reasonCode = session->joinFailStatusCode.reasonCode;

	/* Only indicate assoc_completion if we indicate assoc_start. */
	if (session->bRefAssocStartCnt > 0) {
		session->bRefAssocStartCnt--;
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       session->scan_info.roam_id,
				       eCSR_ROAM_ASSOCIATION_COMPLETION,
				       eCSR_ROAM_RESULT_SCAN_FOR_SSID_FAILURE);
	} else {
		if (!csr_handle_fils_scan_for_ssid_failure(
		    profile, roam_info)) {
			qdf_mem_free(roam_info);
			roam_info = NULL;
		}
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       session->scan_info.roam_id,
				       eCSR_ROAM_ASSOCIATION_FAILURE,
				       eCSR_ROAM_RESULT_SCAN_FOR_SSID_FAILURE);
	}

	if (roam_info)
		qdf_mem_free(roam_info);
roam_completion:
	csr_roam_completion(mac_ctx, session_id, NULL, NULL, roam_result,
			    false);
	return status;
}

QDF_STATUS csr_scan_result_purge(struct mac_context *mac,
				 tScanResultHandle hScanList)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct scan_result_list *pScanList =
				(struct scan_result_list *) hScanList;

	if (pScanList) {
		status = csr_ll_scan_purge_result(mac, &pScanList->List);
		csr_ll_close(&pScanList->List);
		qdf_mem_free(pScanList);
	}
	return status;
}

/* Add the channel to the occupiedChannels array */
static void csr_add_to_occupied_channels(struct mac_context *mac,
					 uint8_t ch,
					 uint8_t sessionId,
					 struct csr_channel *occupied_ch,
					 bool is_init_list)
{
	QDF_STATUS status;
	uint8_t num_occupied_ch = occupied_ch->numChannels;
	uint8_t *occupied_ch_lst = occupied_ch->channelList;

	if (is_init_list)
		mac->scan.roam_candidate_count[sessionId]++;

	if (csr_is_channel_present_in_list(occupied_ch_lst,
					   num_occupied_ch, ch))
		return;

	status = csr_add_to_channel_list_front(occupied_ch_lst,
					       num_occupied_ch, ch);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		occupied_ch->numChannels++;
		if (occupied_ch->numChannels >
		    CSR_BG_SCAN_OCCUPIED_CHANNEL_LIST_LEN)
			occupied_ch->numChannels =
				CSR_BG_SCAN_OCCUPIED_CHANNEL_LIST_LEN;
	}
}

/* Put the BSS into the scan result list */
/* pIes can not be NULL */
static void csr_scan_add_result(struct mac_context *mac_ctx,
				struct tag_csrscan_result *pResult)
{
	qdf_nbuf_t buf;
	uint8_t *data;
	struct mgmt_rx_event_params rx_param = {0};
	struct wlan_frame_hdr *hdr;
	struct wlan_bcn_frame *fixed_frame;
	uint32_t buf_len, i;
	struct bss_description *bss_desc;
	enum mgmt_frame_type frm_type = MGMT_BEACON;

	if (!pResult) {
		sme_err("pResult is null");
		return;
	}

	bss_desc = &pResult->Result.BssDescriptor;
	if (bss_desc->fProbeRsp)
		frm_type = MGMT_PROBE_RESP;

	rx_param.pdev_id = 0;
	rx_param.channel = bss_desc->channelId;
	rx_param.rssi = bss_desc->rssi;
	rx_param.tsf_delta = bss_desc->tsf_delta;

	/* Set all per chain rssi as invalid */
	for (i = 0; i < WLAN_MGMT_TXRX_HOST_MAX_ANTENNA; i++)
		rx_param.rssi_ctl[i] = WLAN_INVALID_PER_CHAIN_RSSI;

	buf_len = GET_IE_LEN_IN_BSS(bss_desc->length) +
		+ offsetof(struct wlan_bcn_frame, ie) + sizeof(*hdr);

	buf = qdf_nbuf_alloc(NULL, qdf_roundup(buf_len, 4),
				0, 4, false);
	if (!buf)
		return;

	qdf_nbuf_put_tail(buf, buf_len);
	qdf_nbuf_set_protocol(buf, ETH_P_CONTROL);

	data = qdf_nbuf_data(buf);
	hdr = (struct wlan_frame_hdr *) data;
	qdf_mem_copy(hdr->i_addr3, bss_desc->bssId, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(hdr->i_addr2, bss_desc->bssId, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(hdr->i_seq,
		&bss_desc->seq_ctrl, sizeof(uint16_t));

	data += sizeof(*hdr);
	fixed_frame = (struct wlan_bcn_frame *)data;
	qdf_mem_copy(fixed_frame->timestamp,
		bss_desc->timeStamp, 8);
	fixed_frame->beacon_interval = bss_desc->beaconInterval;
	fixed_frame->capability.value = bss_desc->capabilityInfo;
	data += offsetof(struct wlan_bcn_frame, ie);

	qdf_mem_copy(data, bss_desc->ieFields,
		GET_IE_LEN_IN_BSS(bss_desc->length));
	wlan_scan_process_bcn_probe_rx_sync(mac_ctx->psoc, buf, &rx_param,
					    frm_type);
}

static bool
csr_scan_save_bss_description(struct mac_context *mac,
			      struct bss_description *pBSSDescription)
{
	struct tag_csrscan_result *pCsrBssDescription = NULL;
	uint32_t cbBSSDesc;
	uint32_t cbAllocated;

	/* figure out how big the BSS description is (the BSSDesc->length does
	 * NOT include the size of the length field itself).
	 */
	cbBSSDesc = pBSSDescription->length + sizeof(pBSSDescription->length);

	cbAllocated = sizeof(struct tag_csrscan_result) + cbBSSDesc;

	pCsrBssDescription = qdf_mem_malloc(cbAllocated);
	if (!pCsrBssDescription)
		return false;

	pCsrBssDescription->AgingCount =
		(int32_t) mac->roam.configParam.agingCount;
	sme_debug(
		"Set Aging Count = %d for BSS " QDF_MAC_ADDR_STR " ",
		pCsrBssDescription->AgingCount,
		QDF_MAC_ADDR_ARRAY(pCsrBssDescription->Result.BssDescriptor.
			       bssId));
	qdf_mem_copy(&pCsrBssDescription->Result.BssDescriptor,
		     pBSSDescription, cbBSSDesc);
	csr_scan_add_result(mac, pCsrBssDescription);
	csr_free_scan_result_entry(mac, pCsrBssDescription);

	return true;
}

/* Append a Bss Description... */
bool csr_scan_append_bss_description(struct mac_context *mac,
				     struct bss_description *pSirBssDescription)
{
	return csr_scan_save_bss_description(mac, pSirBssDescription);
}

static void csr_purge_channel_power(struct mac_context *mac,
				    tDblLinkList *pChannelList)
{
	struct csr_channel_powerinfo *pChannelSet;
	tListElem *pEntry;

	/*
	 * Remove the channel sets from the learned list and put them
	 * in the free list
	 */
	csr_ll_lock(pChannelList);
	while ((pEntry = csr_ll_remove_head(pChannelList,
					    LL_ACCESS_NOLOCK)) != NULL) {
		pChannelSet = GET_BASE_ADDR(pEntry,
					struct csr_channel_powerinfo, link);
		if (pChannelSet)
			qdf_mem_free(pChannelSet);
	}
	csr_ll_unlock(pChannelList);
}

/*
 * Save the channelList into the ultimate storage as the final stage of channel
 * Input: pCountryInfo -- the country code (e.g. "USI"), channel list, and power
 * limit are all stored inside this data structure
 */
QDF_STATUS csr_save_to_channel_power2_g_5_g(struct mac_context *mac,
					    uint32_t tableSize,
					    tSirMacChanInfo *channelTable)
{
	uint32_t i = tableSize / sizeof(tSirMacChanInfo);
	tSirMacChanInfo *pChannelInfo;
	struct csr_channel_powerinfo *pChannelSet;
	bool f2GHzInfoFound = false;
	bool f2GListPurged = false, f5GListPurged = false;

	pChannelInfo = channelTable;
	/* atleast 3 bytes have to be remaining  -- from "countryString" */
	while (i--) {
	pChannelSet = qdf_mem_malloc(sizeof(struct csr_channel_powerinfo));
		if (!pChannelSet) {
			pChannelInfo++;
			continue;
		}
		pChannelSet->firstChannel = pChannelInfo->firstChanNum;
		pChannelSet->numChannels = pChannelInfo->numChannels;
		/*
		 * Now set the inter-channel offset based on the frequency band
		 * the channel set lies in
		 */
		if ((WLAN_REG_IS_24GHZ_CH(pChannelSet->firstChannel)) &&
		    ((pChannelSet->firstChannel +
		      (pChannelSet->numChannels - 1)) <=
		     WLAN_REG_MAX_24GHZ_CH_NUM)) {
			pChannelSet->interChannelOffset = 1;
			f2GHzInfoFound = true;
		} else if ((WLAN_REG_IS_5GHZ_CH(pChannelSet->firstChannel))
		    && ((pChannelSet->firstChannel +
		      ((pChannelSet->numChannels - 1) * 4)) <=
		     WLAN_REG_MAX_5GHZ_CH_NUM)) {
			pChannelSet->interChannelOffset = 4;
			f2GHzInfoFound = false;
		} else {
			sme_warn("Invalid Channel %d Present in Country IE",
				pChannelSet->firstChannel);
			qdf_mem_free(pChannelSet);
			return QDF_STATUS_E_FAILURE;
		}
		pChannelSet->txPower = QDF_MIN(pChannelInfo->maxTxPower,
					mac->mlme_cfg->power.max_tx_power);
		if (f2GHzInfoFound) {
			if (!f2GListPurged) {
				/* purge previous results if found new */
				csr_purge_channel_power(mac,
							&mac->scan.
							channelPowerInfoList24);
				f2GListPurged = true;
			}
			if (CSR_IS_OPERATING_BG_BAND(mac)) {
				/* add to the list of 2.4 GHz channel sets */
				csr_ll_insert_tail(&mac->scan.
						   channelPowerInfoList24,
						   &pChannelSet->link,
						   LL_ACCESS_LOCK);
			} else {
				sme_debug(
					"Adding 11B/G ch in 11A. 1st ch %d",
					pChannelSet->firstChannel);
				qdf_mem_free(pChannelSet);
			}
		} else {
			/* 5GHz info found */
			if (!f5GListPurged) {
				/* purge previous results if found new */
				csr_purge_channel_power(mac,
							&mac->scan.
							channelPowerInfoList5G);
				f5GListPurged = true;
			}
			if (CSR_IS_OPERATING_A_BAND(mac)) {
				/* add to the list of 5GHz channel sets */
				csr_ll_insert_tail(&mac->scan.
						   channelPowerInfoList5G,
						   &pChannelSet->link,
						   LL_ACCESS_LOCK);
			} else {
				sme_debug(
					"Adding 11A ch in B/G. 1st ch %d",
					pChannelSet->firstChannel);
				qdf_mem_free(pChannelSet);
			}
		}
		pChannelInfo++; /* move to next entry */
	}
	return QDF_STATUS_SUCCESS;
}

void csr_apply_power2_current(struct mac_context *mac)
{
	sme_debug("Updating Cfg with power settings");
	csr_save_tx_power_to_cfg(mac, &mac->scan.channelPowerInfoList24,
				 BAND_2G);
	csr_save_tx_power_to_cfg(mac, &mac->scan.channelPowerInfoList5G,
				 BAND_5G);
}

void csr_apply_channel_power_info_to_fw(struct mac_context *mac_ctx,
					struct csr_channel *ch_lst,
					uint8_t *countryCode)
{
	int i;
	uint8_t num_ch = 0;
	uint8_t tempNumChannels = 0;
	struct csr_channel tmp_ch_lst;

	if (ch_lst->numChannels) {
		tempNumChannels = QDF_MIN(ch_lst->numChannels,
					  CFG_VALID_CHANNEL_LIST_LEN);
		for (i = 0; i < tempNumChannels; i++) {
			tmp_ch_lst.channelList[num_ch] = ch_lst->channelList[i];
			num_ch++;
		}
		tmp_ch_lst.numChannels = num_ch;
		/* Store the channel+power info in the global place: Cfg */
		csr_apply_power2_current(mac_ctx);
		csr_set_cfg_valid_channel_list(mac_ctx, tmp_ch_lst.channelList,
					       tmp_ch_lst.numChannels);
	} else {
		sme_err("11D channel list is empty");
	}
	csr_set_cfg_country_code(mac_ctx, countryCode);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void csr_diag_reset_country_information(struct mac_context *mac)
{

	host_log_802_11d_pkt_type *p11dLog;
	int Index;

	WLAN_HOST_DIAG_LOG_ALLOC(p11dLog, host_log_802_11d_pkt_type,
				 LOG_WLAN_80211D_C);
	if (!p11dLog)
		return;

	p11dLog->eventId = WLAN_80211D_EVENT_RESET;
	qdf_mem_copy(p11dLog->countryCode, mac->scan.countryCodeCurrent, 3);
	p11dLog->numChannel = mac->scan.base_channels.numChannels;
	if (p11dLog->numChannel <= HOST_LOG_MAX_NUM_CHANNEL) {
		qdf_mem_copy(p11dLog->Channels,
			     mac->scan.base_channels.channelList,
			     p11dLog->numChannel);
		for (Index = 0;
		     Index < mac->scan.base_channels.numChannels;
		     Index++) {
			p11dLog->TxPwr[Index] = QDF_MIN(
				mac->scan.defaultPowerTable[Index].tx_power,
				mac->mlme_cfg->power.max_tx_power);
		}
	}
	if (!mac->mlme_cfg->gen.enabled_11d)
		p11dLog->supportMultipleDomain = WLAN_80211D_DISABLED;
	else
		p11dLog->supportMultipleDomain =
			WLAN_80211D_SUPPORT_MULTI_DOMAIN;
	WLAN_HOST_DIAG_LOG_REPORT(p11dLog);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

/**
 * csr_apply_channel_power_info_wrapper() - sends channel info to fw
 * @mac: main MAC data structure
 *
 * This function sends the channel power info to firmware
 *
 * Return: none
 */
void csr_apply_channel_power_info_wrapper(struct mac_context *mac)
{

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_diag_reset_country_information(mac);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	csr_prune_channel_list_for_mode(mac, &mac->scan.base_channels);
	csr_save_channel_power_for_band(mac, false);
	csr_save_channel_power_for_band(mac, true);
	/* apply the channel list, power settings, and the country code. */
	csr_apply_channel_power_info_to_fw(mac,
		&mac->scan.base_channels, mac->scan.countryCodeCurrent);
	/* clear the 11d channel list */
	qdf_mem_zero(&mac->scan.channels11d, sizeof(mac->scan.channels11d));
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
/* caller allocated memory for pNumChn and pChnPowerInfo */
/* As input, *pNumChn has the size of the array of pChnPowerInfo */
/* Upon return, *pNumChn has the number of channels assigned. */
static void csr_get_channel_power_info(struct mac_context *mac,
				       tDblLinkList *list,
				       uint32_t *num_ch,
				       struct channel_power *chn_pwr_info)
{
	tListElem *entry;
	uint32_t chn_idx = 0, idx;
	struct csr_channel_powerinfo *ch_set;

	/* Get 2.4Ghz first */
	csr_ll_lock(list);
	entry = csr_ll_peek_head(list, LL_ACCESS_NOLOCK);
	while (entry && (chn_idx < *num_ch)) {
		ch_set = GET_BASE_ADDR(entry,
				struct csr_channel_powerinfo, link);
		for (idx = 0; (idx < ch_set->numChannels)
				&& (chn_idx < *num_ch); idx++) {
			chn_pwr_info[chn_idx].chan_num =
				(uint8_t) (ch_set->firstChannel
				 + (idx * ch_set->interChannelOffset));
			chn_pwr_info[chn_idx++].tx_power = ch_set->txPower;
		}
		entry = csr_ll_next(list, entry, LL_ACCESS_NOLOCK);
	}
	csr_ll_unlock(list);
	*num_ch = chn_idx;
}

static void csr_diag_apply_country_info(struct mac_context *mac_ctx)
{
	host_log_802_11d_pkt_type *p11dLog;
	struct channel_power chnPwrInfo[CFG_VALID_CHANNEL_LIST_LEN];
	uint32_t nChnInfo = CFG_VALID_CHANNEL_LIST_LEN, nTmp;

	WLAN_HOST_DIAG_LOG_ALLOC(p11dLog, host_log_802_11d_pkt_type,
				 LOG_WLAN_80211D_C);
	if (!p11dLog)
		return;

	p11dLog->eventId = WLAN_80211D_EVENT_COUNTRY_SET;
	qdf_mem_copy(p11dLog->countryCode, mac_ctx->scan.countryCode11d, 3);
	p11dLog->numChannel = mac_ctx->scan.channels11d.numChannels;
	if (p11dLog->numChannel > HOST_LOG_MAX_NUM_CHANNEL)
		goto diag_end;

	qdf_mem_copy(p11dLog->Channels,
		     mac_ctx->scan.channels11d.channelList,
		     p11dLog->numChannel);
	csr_get_channel_power_info(mac_ctx,
				&mac_ctx->scan.channelPowerInfoList24,
				&nChnInfo, chnPwrInfo);
	nTmp = nChnInfo;
	nChnInfo = CFG_VALID_CHANNEL_LIST_LEN - nTmp;
	csr_get_channel_power_info(mac_ctx,
				&mac_ctx->scan.channelPowerInfoList5G,
				&nChnInfo, &chnPwrInfo[nTmp]);
	for (nTmp = 0; nTmp < p11dLog->numChannel; nTmp++) {
		for (nChnInfo = 0;
		     nChnInfo < CFG_VALID_CHANNEL_LIST_LEN;
		     nChnInfo++) {
			if (p11dLog->Channels[nTmp] ==
			    chnPwrInfo[nChnInfo].chan_num) {
				p11dLog->TxPwr[nTmp] =
					chnPwrInfo[nChnInfo].tx_power;
				break;
			}
		}
	}
diag_end:
	if (!mac_ctx->mlme_cfg->gen.enabled_11d)
		p11dLog->supportMultipleDomain = WLAN_80211D_DISABLED;
	else
		p11dLog->supportMultipleDomain =
				WLAN_80211D_SUPPORT_MULTI_DOMAIN;
	WLAN_HOST_DIAG_LOG_REPORT(p11dLog);
}
#endif /* #ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR */

/**
 * csr_apply_country_information() - apply country code information
 * @mac: core MAC data structure
 *
 * This function programs the new country code
 *
 * Return: none
 */
void csr_apply_country_information(struct mac_context *mac)
{
	v_REGDOMAIN_t domainId;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!wlan_reg_11d_enabled_on_host(mac->psoc))
		return;
	status = csr_get_regulatory_domain_for_country(mac,
			mac->scan.countryCode11d, &domainId, SOURCE_QUERY);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return;
	/* Check whether we need to enforce default domain */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_diag_apply_country_info(mac);
#endif /* #ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR */

	if (mac->scan.domainIdCurrent != domainId)
		return;
	if (mac->scan.domainIdCurrent != domainId) {
		sme_debug("Domain Changed Old %d, new %d",
			mac->scan.domainIdCurrent, domainId);
		if (domainId >= REGDOMAIN_COUNT)
			sme_err("fail to set regId %d", domainId);
	}
	mac->scan.domainIdCurrent = domainId;
	/* switch to active scans using this new channel list */
	mac->scan.curScanType = eSIR_ACTIVE_SCAN;
}

void csr_save_channel_power_for_band(struct mac_context *mac, bool fill_5f)
{
	uint32_t idx, count = 0;
	tSirMacChanInfo *chan_info;
	tSirMacChanInfo *ch_info_start;
	int32_t max_ch_idx;
	bool tmp_bool;
	uint8_t ch = 0;

	max_ch_idx =
		(mac->scan.base_channels.numChannels <
		CFG_VALID_CHANNEL_LIST_LEN) ?
		mac->scan.base_channels.numChannels :
		CFG_VALID_CHANNEL_LIST_LEN;

	chan_info = qdf_mem_malloc(sizeof(tSirMacChanInfo) *
				   CFG_VALID_CHANNEL_LIST_LEN);
	if (!chan_info)
		return;

	ch_info_start = chan_info;
	for (idx = 0; idx < max_ch_idx; idx++) {
		ch = mac->scan.defaultPowerTable[idx].chan_num;
		tmp_bool = (fill_5f && WLAN_REG_IS_5GHZ_CH(ch)) ||
			(!fill_5f && WLAN_REG_IS_24GHZ_CH(ch));
		if (!tmp_bool)
			continue;

		if (count >= CFG_VALID_CHANNEL_LIST_LEN) {
			sme_warn("count: %d exceeded", count);
			break;
		}

		chan_info->firstChanNum =
			mac->scan.defaultPowerTable[idx].chan_num;
		chan_info->numChannels = 1;
		chan_info->maxTxPower =
			QDF_MIN(mac->scan.defaultPowerTable[idx].tx_power,
				mac->mlme_cfg->power.max_tx_power);
		chan_info++;
		count++;
	}
	if (count) {
		csr_save_to_channel_power2_g_5_g(mac,
				count * sizeof(tSirMacChanInfo), ch_info_start);
	}
	qdf_mem_free(ch_info_start);
}

bool csr_is_supported_channel(struct mac_context *mac, uint8_t channelId)
{
	bool fRet = false;
	uint32_t i;

	for (i = 0; i < mac->scan.base_channels.numChannels; i++) {
		if (channelId ==
		    mac->scan.base_channels.channelList[i]) {
			fRet = true;
			break;
		}
	}

	return fRet;
}

/*
 * 802.11D only: Gather 11d IE via beacon or Probe response and store them in
 * pAdapter->channels11d
 */
bool csr_learn_11dcountry_information(struct mac_context *mac,
				      struct bss_description *pSirBssDesc,
				      tDot11fBeaconIEs *pIes, bool fForce)
{
	QDF_STATUS status;
	uint8_t *pCountryCodeSelected;
	bool fRet = false;
	v_REGDOMAIN_t domainId;
	tDot11fBeaconIEs *pIesLocal = pIes;
	bool useVoting = false;

	if ((!pSirBssDesc) && (!pIes))
		useVoting = true;

	/* check if .11d support is enabled */
	if (!wlan_reg_11d_enabled_on_host(mac->psoc))
		goto free_ie;

	if (false == useVoting) {
		if (!pIesLocal &&
		   (!QDF_IS_STATUS_SUCCESS(
			csr_get_parsed_bss_description_ies(
				mac, pSirBssDesc, &pIesLocal))))
			goto free_ie;
		/* check if country information element is present */
		if (!pIesLocal->Country.present)
			/* No country info */
			goto free_ie;
		status = csr_get_regulatory_domain_for_country(mac,
				pIesLocal->Country.country, &domainId,
				SOURCE_QUERY);
		if (QDF_IS_STATUS_SUCCESS(status)
		    && (domainId == REGDOMAIN_WORLD))
			goto free_ie;
	} /* useVoting == false */

	if (false == useVoting)
		pCountryCodeSelected = pIesLocal->Country.country;
	else
		pCountryCodeSelected = mac->scan.countryCodeElected;

	if (qdf_mem_cmp(pCountryCodeSelected, mac->scan.countryCodeCurrent,
			CDS_COUNTRY_CODE_LEN) == 0) {
		qdf_mem_copy(mac->scan.countryCode11d,
			     mac->scan.countryCodeCurrent,
			     CDS_COUNTRY_CODE_LEN);
		goto free_ie;
	}

	mac->reg_hint_src = SOURCE_11D;
	status = csr_get_regulatory_domain_for_country(mac,
				pCountryCodeSelected, &domainId, SOURCE_11D);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("fail to get regId %d", domainId);
		fRet = false;
		goto free_ie;
	}

	fRet = true;
free_ie:
	if (!pIes && pIesLocal) {
		/* locally allocated */
		qdf_mem_free(pIesLocal);
	}
	return fRet;
}

static enum csr_scancomplete_nextcommand
csr_scan_get_next_command_state(struct mac_context *mac_ctx,
				uint32_t session_id,
				eCsrScanStatus scan_status,
				uint8_t *chan)
{
	enum csr_scancomplete_nextcommand NextCommand = eCsrNextScanNothing;
	int8_t channel;
	struct csr_roam_session *session;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("session %d is invalid", session_id);
		return NextCommand;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);
	switch (session->scan_info.scan_reason) {
	case eCsrScanForSsid:
		sme_debug("Resp for Scan For Ssid");
		channel = csr_scan_get_channel_for_hw_mode_change(
				mac_ctx,
				session_id,
				session->scan_info.profile);
		if ((!channel) || scan_status) {
			NextCommand = eCsrNexteScanForSsidFailure;
			sme_err("next Scan For Ssid Failure %d %d",
				channel, scan_status);
		} else {
			NextCommand = eCsrNextCheckAllowConc;
			*chan = channel;
			sme_debug("next CheckAllowConc");
		}
		break;
	default:
		NextCommand = eCsrNextScanNothing;
		break;
	}
	sme_debug("Next Command %d", NextCommand);
	return NextCommand;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_diag_scan_complete(struct mac_context *mac_ctx,
		       eCsrScanStatus scan_status)
{
	host_log_scan_pkt_type *pScanLog = NULL;
	qdf_list_t *list = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct scan_cache_node *cur_node = NULL;
	struct scan_cache_node *next_node = NULL;
	int n = 0, c = 0;

	WLAN_HOST_DIAG_LOG_ALLOC(pScanLog,
				 host_log_scan_pkt_type,
				 LOG_WLAN_SCAN_C);
	if (!pScanLog)
		return;

	pScanLog->eventId = WLAN_SCAN_EVENT_ACTIVE_SCAN_RSP;

	if (eCSR_SCAN_SUCCESS != scan_status) {
		pScanLog->status = WLAN_SCAN_STATUS_FAILURE;
		WLAN_HOST_DIAG_LOG_REPORT(pScanLog);
		return;
	}
	pdev = wlan_objmgr_get_pdev_by_id(mac_ctx->psoc,
		0, WLAN_LEGACY_MAC_ID);

	if (!pdev) {
		sme_err("pdev is NULL");
		return;
	}

	list = ucfg_scan_get_result(pdev, NULL);
	if (list)
		sme_debug("num_entries %d", qdf_list_size(list));
	if (!list || (list && !qdf_list_size(list))) {
		sme_err("get scan result failed");
		WLAN_HOST_DIAG_LOG_REPORT(pScanLog);
		wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);
		if (list)
			ucfg_scan_purge_results(list);
		return;
	}

	qdf_list_peek_front(list,
		   (qdf_list_node_t **) &cur_node);
	while (cur_node) {
		if (n < HOST_LOG_MAX_NUM_BSSID) {
			qdf_mem_copy(pScanLog->bssid[n],
				cur_node->entry->bssid.bytes,
				QDF_MAC_ADDR_SIZE);
			if (cur_node->entry->ssid.length >
			   WLAN_SSID_MAX_LEN)
				cur_node->entry->ssid.length =
				  WLAN_SSID_MAX_LEN;
			qdf_mem_copy(pScanLog->ssid[n],
				cur_node->entry->ssid.ssid,
				cur_node->entry->ssid.length);
			n++;
		}
		c++;
		qdf_list_peek_next(
		  list,
		  (qdf_list_node_t *) cur_node,
		  (qdf_list_node_t **) &next_node);
		cur_node = next_node;
		next_node = NULL;
	}
	pScanLog->numSsid = (uint8_t) n;
	pScanLog->totalSsid = (uint8_t) c;
	ucfg_scan_purge_results(list);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);
	WLAN_HOST_DIAG_LOG_REPORT(pScanLog);
}
#endif /* #ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR */

/**
 * csr_saved_scan_cmd_free_fields() - Free internal fields of scan command
 *
 * @mac_ctx: Global MAC context
 * @session: sme session
 *
 * Frees data structures allocated inside saved_scan_cmd and releases
 * the profile.
 * Return: None
 */

void csr_saved_scan_cmd_free_fields(struct mac_context *mac_ctx,
				    struct csr_roam_session *session)
{
	if (session->scan_info.profile) {
		sme_debug("Free profile for session %d", session->sessionId);
		csr_release_profile(mac_ctx,
				    session->scan_info.profile);
		qdf_mem_free(session->scan_info.profile);
		session->scan_info.profile = NULL;
	}

	if (session->scan_info.roambssentry) {
		qdf_mem_free(session->scan_info.roambssentry);
		session->scan_info.roambssentry = NULL;
	}
}
/**
 * csr_save_profile() - Save the profile info from sme command
 * @mac_ctx: Global MAC context
 * @save_cmd: Pointer where the command will be saved
 * @command: Command from which the profile will be saved
 *
 * Saves the profile information from the SME's scan command
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_save_profile(struct mac_context *mac_ctx,
				   uint32_t session_id)
{
	struct tag_csrscan_result *scan_result;
	struct tag_csrscan_result *temp;
	uint32_t bss_len;
	struct csr_roam_session *session;

	/*
	 * check the session's validity first, if session itself
	 * is not valid then there is no point of releasing the memory
	 * for invalid session (i.e. "goto error" case)
	 */
	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("session %d is invalid", session_id);
		return QDF_STATUS_E_FAILURE;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session->scan_info.roambssentry)
		return QDF_STATUS_SUCCESS;

	scan_result = GET_BASE_ADDR(session->scan_info.roambssentry,
			struct tag_csrscan_result, Link);

	bss_len = scan_result->Result.BssDescriptor.length +
		sizeof(scan_result->Result.BssDescriptor.length);

	temp = qdf_mem_malloc(sizeof(struct tag_csrscan_result) + bss_len);
	if (!temp)
		goto error;

	temp->AgingCount = scan_result->AgingCount;
	temp->preferValue = scan_result->preferValue;
	temp->capValue = scan_result->capValue;
	temp->ucEncryptionType = scan_result->ucEncryptionType;
	temp->mcEncryptionType = scan_result->mcEncryptionType;
	temp->authType = scan_result->authType;
	/* pvIes is unsued in success/failure */
	temp->Result.pvIes = NULL;

	qdf_mem_copy(temp->Result.pvIes,
			scan_result->Result.pvIes,
			sizeof(*scan_result->Result.pvIes));
	temp->Result.ssId.length = scan_result->Result.ssId.length;
	qdf_mem_copy(temp->Result.ssId.ssId,
			scan_result->Result.ssId.ssId,
			sizeof(scan_result->Result.ssId.ssId));
	temp->Result.timer = scan_result->Result.timer;
	qdf_mem_copy(&temp->Result.BssDescriptor,
			&scan_result->Result.BssDescriptor,
			sizeof(temp->Result.BssDescriptor));
	temp->Link.last = temp->Link.next = NULL;
	session->scan_info.roambssentry = (tListElem *)temp;

	return QDF_STATUS_SUCCESS;
error:
	csr_scan_handle_search_for_ssid_failure(mac_ctx,
			session_id);
	csr_saved_scan_cmd_free_fields(mac_ctx, session);

	return QDF_STATUS_E_FAILURE;
}

static void csr_handle_nxt_cmd(struct mac_context *mac_ctx,
			       enum csr_scancomplete_nextcommand nxt_cmd,
			       uint32_t session_id,
			       uint8_t chan)
{
	QDF_STATUS status, ret;
	struct csr_roam_session *session;

	switch (nxt_cmd) {

	case eCsrNexteScanForSsidSuccess:
		csr_scan_handle_search_for_ssid(mac_ctx, session_id);
		break;
	case eCsrNexteScanForSsidFailure:
		csr_scan_handle_search_for_ssid_failure(mac_ctx, session_id);
		break;
	case eCsrNextCheckAllowConc:
		ret = policy_mgr_current_connections_update(mac_ctx->psoc,
					session_id, chan,
					POLICY_MGR_UPDATE_REASON_HIDDEN_STA);
		sme_debug("chan: %d session: %d status: %d",
					chan, session_id, ret);

		status = csr_save_profile(mac_ctx, session_id);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* csr_save_profile should report error */
			sme_err("profile save failed %d", status);
			break;
		}

		if (QDF_STATUS_E_FAILURE == ret) {
			sme_debug("conn update fail %d", chan);
			csr_scan_handle_search_for_ssid_failure(mac_ctx,
								session_id);
		} else if ((QDF_STATUS_E_NOSUPPORT == ret) ||
			(QDF_STATUS_E_ALREADY == ret)) {
			sme_err("conn update ret %d", ret);
			csr_scan_handle_search_for_ssid(mac_ctx, session_id);
		}
		/* Else: Set hw mode was issued and the saved connect would
		 * be issued after set hw mode response
		 */
		if (QDF_IS_STATUS_SUCCESS(ret))
			return;
	default:
		break;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (session)
		csr_saved_scan_cmd_free_fields(mac_ctx, session);
}

void csr_scan_callback(struct wlan_objmgr_vdev *vdev,
		       struct scan_event *event, void *arg)
{
	eCsrScanStatus scan_status = eCSR_SCAN_FAILURE;
	enum csr_scancomplete_nextcommand NextCommand = eCsrNextScanNothing;
	struct mac_context *mac_ctx;
	struct csr_roam_session *session;
	uint32_t session_id = 0;
	uint8_t chan = 0;
	QDF_STATUS status;
	bool success = false;

	mac_ctx = (struct mac_context *)arg;

	qdf_mtrace(QDF_MODULE_ID_SCAN, QDF_MODULE_ID_SME, event->type,
		   event->vdev_id, event->scan_id);

	if (!util_is_scan_completed(event, &success))
		return;

	if (success)
		scan_status = eCSR_SCAN_SUCCESS;

	session_id = wlan_vdev_get_id(vdev);
	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("session %d is invalid", session_id);
		sme_release_global_lock(&mac_ctx->sme);
		return;
	}

	session = CSR_GET_SESSION(mac_ctx, session_id);

	sme_debug("Scan Completion: status %d session %d scan_id %d",
			scan_status, session_id, event->scan_id);

	/* verify whether scan event is related to scan interested by CSR */
	if (session->scan_info.scan_id != event->scan_id) {
		sme_debug("Scan Completion on wrong scan_id %d, expected %d",
			session->scan_info.scan_id, event->scan_id);
		sme_release_global_lock(&mac_ctx->sme);
		return;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_diag_scan_complete(mac_ctx, scan_status);
#endif
	NextCommand = csr_scan_get_next_command_state(mac_ctx,
					session_id, scan_status,
					&chan);
	/* We reuse the command here instead reissue a new command */
	csr_handle_nxt_cmd(mac_ctx, NextCommand,
			   session_id, chan);

	sme_release_global_lock(&mac_ctx->sme);
}

tCsrScanResultInfo *csr_scan_result_get_first(struct mac_context *mac,
					      tScanResultHandle hScanResult)
{
	tListElem *pEntry;
	struct tag_csrscan_result *pResult;
	tCsrScanResultInfo *pRet = NULL;
	struct scan_result_list *pResultList =
				(struct scan_result_list *) hScanResult;

	if (pResultList) {
		pEntry = csr_ll_peek_head(&pResultList->List, LL_ACCESS_NOLOCK);
		if (pEntry) {
			pResult = GET_BASE_ADDR(pEntry, struct
						tag_csrscan_result, Link);
			pRet = &pResult->Result;
		}
		pResultList->pCurEntry = pEntry;
	}

	return pRet;
}

tCsrScanResultInfo *csr_scan_result_get_next(struct mac_context *mac,
					     tScanResultHandle hScanResult)
{
	tListElem *pEntry = NULL;
	struct tag_csrscan_result *pResult = NULL;
	tCsrScanResultInfo *pRet = NULL;
	struct scan_result_list *pResultList =
				(struct scan_result_list *) hScanResult;

	if (!pResultList)
		return NULL;

	if (!pResultList->pCurEntry)
		pEntry = csr_ll_peek_head(&pResultList->List, LL_ACCESS_NOLOCK);
	else
		pEntry = csr_ll_next(&pResultList->List, pResultList->pCurEntry,
				     LL_ACCESS_NOLOCK);

	if (pEntry) {
		pResult = GET_BASE_ADDR(pEntry, struct tag_csrscan_result,
					Link);
		pRet = &pResult->Result;
	}
	pResultList->pCurEntry = pEntry;

	return pRet;
}

/**
 * csr_scan_for_ssid() -  Function usually used for BSSs that suppresses SSID
 * @mac_ctx: Pointer to Global Mac structure
 * @profile: pointer to struct csr_roam_profile
 * @roam_id: variable representing roam id
 * @notify: boolean variable
 *
 * Function is usually used for BSSs that suppresses SSID so the profile
 * shall have one and only one SSID.
 *
 * Return: Success - QDF_STATUS_SUCCESS, Failure - error number
 */
QDF_STATUS csr_scan_for_ssid(struct mac_context *mac_ctx, uint32_t session_id,
			     struct csr_roam_profile *profile, uint32_t roam_id,
			     bool notify)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t num_ssid = profile->SSIDs.numOfSSIDs;
	struct scan_start_request *req;
	struct wlan_objmgr_vdev *vdev;
	uint8_t i, chan, num_chan = 0;
	uint8_t pdev_id;
	wlan_scan_id scan_id;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("session %d is invalid", session_id);
		return status;
	}

	if (num_ssid != 1) {
		sme_err("numSSID (%d) is invalid", profile->SSIDs.numOfSSIDs);
		return status;
	}

	if (!mac_ctx->pdev) {
		sme_err("pdev ctx is NULL");
		return status;
	}
	pdev_id = wlan_objmgr_pdev_get_pdev_id(mac_ctx->pdev);

	/* Free old memory if any before its overwritten */
	csr_saved_scan_cmd_free_fields(mac_ctx, session);
	session->scan_info.profile =
			qdf_mem_malloc(sizeof(struct csr_roam_profile));
	if (!session->scan_info.profile)
		status = QDF_STATUS_E_NOMEM;
	else
		status = csr_roam_copy_profile(mac_ctx,
					session->scan_info.profile,
					profile);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;
	scan_id = ucfg_scan_get_scan_id(mac_ctx->psoc);
	session->scan_info.scan_id = scan_id;
	session->scan_info.scan_reason = eCsrScanForSsid;
	session->scan_info.roam_id = roam_id;
	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		status = QDF_STATUS_E_NOMEM;
		goto error;
	}

	vdev = wlan_objmgr_get_vdev_by_macaddr_from_psoc(mac_ctx->psoc,
				pdev_id,
				session->self_mac_addr.bytes,
				WLAN_LEGACY_SME_ID);
	ucfg_scan_init_default_params(vdev, req);
	req->scan_req.scan_id = scan_id;
	req->scan_req.vdev_id = session_id;
	req->scan_req.scan_req_id = mac_ctx->scan.requester_id;
	req->scan_req.scan_f_passive = false;
	req->scan_req.scan_f_bcast_probe = false;

	if (QDF_P2P_CLIENT_MODE == profile->csrPersona)
		req->scan_req.scan_priority = SCAN_PRIORITY_HIGH;

	/* Allocate memory for IE field */
	if (profile->pAddIEScan) {
		req->scan_req.extraie.ptr =
			qdf_mem_malloc(profile->nAddIEScanLength);

		if (!req->scan_req.extraie.ptr)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;

		if (QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_copy(req->scan_req.extraie.ptr,
					profile->pAddIEScan,
					profile->nAddIEScanLength);
			req->scan_req.extraie.len = profile->nAddIEScanLength;
		} else {
			sme_err("No memory for scanning IE fields");
		}
	}

	req->scan_req.num_bssid = 1;
	if (profile->BSSIDs.numOfBSSIDs == 1)
		qdf_copy_macaddr(&req->scan_req.bssid_list[0],
					profile->BSSIDs.bssid);
	else
		qdf_set_macaddr_broadcast(&req->scan_req.bssid_list[0]);

	if (profile->ChannelInfo.numOfChannels) {
		for (i = 0; i < profile->ChannelInfo.numOfChannels; i++) {
			if (csr_roam_is_valid_channel(mac_ctx,
				profile->ChannelInfo.ChannelList[i])) {
				chan = profile->ChannelInfo.ChannelList[i];
				req->scan_req.chan_list.chan[num_chan].freq =
						wlan_chan_to_freq(chan);
				num_chan++;
			}
		}
		req->scan_req.chan_list.num_chan = num_chan;
	}

	/* Extend it for multiple SSID */
	if (profile->SSIDs.numOfSSIDs) {
		if (profile->SSIDs.SSIDList[0].SSID.length > WLAN_SSID_MAX_LEN) {
			sme_debug("wrong ssid length = %d",
					profile->SSIDs.SSIDList[0].SSID.length);
			status = QDF_STATUS_E_INVAL;
			goto error;
		}
		req->scan_req.num_ssids = 1;
		qdf_mem_copy(&req->scan_req.ssid[0].ssid,
				&profile->SSIDs.SSIDList[0].SSID.ssId,
				profile->SSIDs.SSIDList[0].SSID.length);
		req->scan_req.ssid[0].length =
				profile->SSIDs.SSIDList[0].SSID.length;
		sme_debug("scan for SSID = %.*s", req->scan_req.ssid[0].length,
			  req->scan_req.ssid[0].ssid);
	}
	status = ucfg_scan_start(req);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
error:
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to initiate scan with status: %d", status);
		csr_release_profile(mac_ctx, session->scan_info.profile);
		qdf_mem_free(session->scan_info.profile);
		session->scan_info.profile = NULL;
		if (notify)
			csr_roam_call_callback(mac_ctx, session_id, NULL,
					roam_id, eCSR_ROAM_FAILED,
					eCSR_ROAM_RESULT_FAILURE);
	}
	return status;
}

static void csr_set_cfg_valid_channel_list(struct mac_context *mac,
					   uint8_t *pChannelList,
					   uint8_t NumChannels)
{
	QDF_STATUS status;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "%s: dump valid channel list(NumChannels(%d))",
		  __func__, NumChannels);
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			   pChannelList, NumChannels);
	qdf_mem_copy(mac->mlme_cfg->reg.valid_channel_list, pChannelList,
		     NumChannels);
	mac->mlme_cfg->reg.valid_channel_list_num = NumChannels;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "Scan offload is enabled, update default chan list");
	/*
	 * disable fcc constraint since new country code
	 * is being set
	 */
	mac->scan.fcc_constraint = false;
	status = csr_update_channel_list(mac);
	if (QDF_STATUS_SUCCESS != status) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "failed to update the supported channel list");
	}
}

/*
 * The Tx power limits are saved in the cfg for future usage.
 */
static void csr_save_tx_power_to_cfg(struct mac_context *mac,
				     tDblLinkList *pList,
				     enum band_info band)
{
	tListElem *pEntry;
	uint32_t cbLen = 0, dataLen, tmp_len;
	struct csr_channel_powerinfo *ch_set;
	uint32_t idx, count = 0;
	tSirMacChanInfo *ch_pwr_set;
	uint8_t *p_buf = NULL;

	/* allocate maximum space for all channels */
	dataLen = CFG_VALID_CHANNEL_LIST_LEN * sizeof(tSirMacChanInfo);
	p_buf = qdf_mem_malloc(dataLen);
	if (!p_buf)
		return;

	ch_pwr_set = (tSirMacChanInfo *)(p_buf);
	csr_ll_lock(pList);
	pEntry = csr_ll_peek_head(pList, LL_ACCESS_NOLOCK);
	/*
	 * write the tuples (startChan, numChan, txPower) for each channel found
	 * in the channel power list.
	 */
	while (pEntry) {
		ch_set = GET_BASE_ADDR(pEntry,
				struct csr_channel_powerinfo, link);
		if (1 != ch_set->interChannelOffset) {
			/*
			 * we keep the 5G channel sets internally with an
			 * interchannel offset of 4. Expand these to the right
			 * format. (inter channel offset of 1 is the only option
			 * for the triplets that 11d advertises.
			 */
			tmp_len = cbLen + (ch_set->numChannels *
						sizeof(tSirMacChanInfo));
			if (tmp_len >= dataLen) {
				/*
				 * expanding this entry will overflow our
				 * allocation
				 */
				sme_err(
					"Buffer overflow, start %d, num %d, offset %d",
					ch_set->firstChannel,
					ch_set->numChannels,
					ch_set->interChannelOffset);
				break;
			}

			for (idx = 0; idx < ch_set->numChannels; idx++) {
				ch_pwr_set->firstChanNum = (tSirMacChanNum)
					(ch_set->firstChannel + (idx *
						ch_set->interChannelOffset));
				ch_pwr_set->numChannels = 1;
				ch_pwr_set->maxTxPower =
					QDF_MIN(ch_set->txPower,
					mac->mlme_cfg->power.max_tx_power);
				cbLen += sizeof(tSirMacChanInfo);
				ch_pwr_set++;
				count++;
			}
		} else {
			if (cbLen + sizeof(tSirMacChanInfo) >= dataLen) {
				/* this entry will overflow our allocation */
				sme_err(
					"Buffer overflow, start %d, num %d, offset %d",
					ch_set->firstChannel,
					ch_set->numChannels,
					ch_set->interChannelOffset);
				break;
			}
			ch_pwr_set->firstChanNum = ch_set->firstChannel;
			ch_pwr_set->numChannels = ch_set->numChannels;
			ch_pwr_set->maxTxPower = QDF_MIN(ch_set->txPower,
					mac->mlme_cfg->power.max_tx_power);
			cbLen += sizeof(tSirMacChanInfo);
			ch_pwr_set++;
			count++;
		}
		pEntry = csr_ll_next(pList, pEntry, LL_ACCESS_NOLOCK);
	}
	csr_ll_unlock(pList);
	if (band == BAND_2G) {
		mac->mlme_cfg->power.max_tx_power_24.len = 3 * count;
		qdf_mem_copy(mac->mlme_cfg->power.max_tx_power_24.data,
			     (uint8_t *)p_buf,
			     mac->mlme_cfg->power.max_tx_power_24.len);
	}
	if (band == BAND_5G) {
		mac->mlme_cfg->power.max_tx_power_5.len = 3 * count;
		qdf_mem_copy(mac->mlme_cfg->power.max_tx_power_5.data,
			     (uint8_t *)p_buf,
			     mac->mlme_cfg->power.max_tx_power_5.len);
	}
	qdf_mem_free(p_buf);
}

static void csr_set_cfg_country_code(struct mac_context *mac,
				     uint8_t *countryCode)
{
	uint8_t cc[CFG_COUNTRY_CODE_LEN];
	/* v_REGDOMAIN_t DomainId */

	sme_debug("Set Country in Cfg %s", countryCode);
	qdf_mem_copy(cc, countryCode, CFG_COUNTRY_CODE_LEN);

	/*
	 * Don't program the bogus country codes that we created for Korea in
	 * the MAC. if we see the bogus country codes, program the MAC with
	 * the right country code.
	 */
	if (('K' == countryCode[0] && '1' == countryCode[1]) ||
	    ('K' == countryCode[0] && '2' == countryCode[1]) ||
	    ('K' == countryCode[0] && '3' == countryCode[1]) ||
	    ('K' == countryCode[0] && '4' == countryCode[1])) {
		/*
		 * replace the alternate Korea country codes, 'K1', 'K2', ..
		 * with 'KR' for Korea
		 */
		cc[1] = 'R';
	}

	/*
	 * country code is moved to mlme component, and it is limited to call
	 * legacy api, so required to call sch_edca_profile_update_all if
	 * overwrite country code in mlme component
	 */
	qdf_mem_copy(mac->mlme_cfg->reg.country_code, cc, CFG_COUNTRY_CODE_LEN);
	mac->mlme_cfg->reg.country_code_len = CFG_COUNTRY_CODE_LEN;
	sch_edca_profile_update_all(mac);
	/*
	 * Need to let HALPHY know about the current domain so it can apply some
	 * domain-specific settings (TX filter...)
	 */
}

QDF_STATUS csr_get_country_code(struct mac_context *mac, uint8_t *pBuf,
				uint8_t *pbLen)
{
	if (pBuf && pbLen && (*pbLen >= CFG_COUNTRY_CODE_LEN)) {
		*pbLen = mac->mlme_cfg->reg.country_code_len;
		qdf_mem_copy(pBuf, mac->mlme_cfg->reg.country_code,
			     (uint32_t)*pbLen);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_INVAL;
}

QDF_STATUS csr_scan_abort_mac_scan(struct mac_context *mac_ctx,
				   uint32_t vdev_id,
				   uint32_t scan_id)
{
	struct scan_cancel_request *req;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	/* Get NL global context from objmgr*/
	if (vdev_id == INVAL_VDEV_ID)
		vdev = wlan_objmgr_pdev_get_first_vdev(mac_ctx->pdev,
						       WLAN_LEGACY_SME_ID);
	else
		vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac_ctx->pdev,
				vdev_id, WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_debug("Failed get vdev");
		qdf_mem_free(req);
		return QDF_STATUS_E_INVAL;
	}

	req->vdev = vdev;
	req->cancel_req.scan_id = scan_id;
	req->cancel_req.pdev_id = wlan_objmgr_pdev_get_pdev_id(mac_ctx->pdev);
	req->cancel_req.vdev_id = vdev_id;
	if (scan_id != INVAL_SCAN_ID)
		req->cancel_req.req_type = WLAN_SCAN_CANCEL_SINGLE;
	if (vdev_id == INVAL_VDEV_ID)
		req->cancel_req.req_type = WLAN_SCAN_CANCEL_PDEV_ALL;
	else
		req->cancel_req.req_type = WLAN_SCAN_CANCEL_VDEV_ALL;

	status = ucfg_scan_cancel(req);
	if (QDF_IS_STATUS_ERROR(status))
		sme_err("Cancel scan request failed");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return status;
}

bool csr_roam_is_valid_channel(struct mac_context *mac, uint8_t channel)
{
	bool fValid = false;
	uint32_t idx_valid_ch;
	uint32_t len = mac->roam.numValidChannels;

	for (idx_valid_ch = 0; (idx_valid_ch < len); idx_valid_ch++) {
		if (channel == mac->roam.validChannelList[idx_valid_ch]) {
			fValid = true;
			break;
		}
	}
	return fValid;
}

QDF_STATUS csr_scan_create_entry_in_scan_cache(struct mac_context *mac,
					       uint32_t sessionId,
					       struct qdf_mac_addr bssid,
					       uint8_t channel)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	struct bss_description *pNewBssDescriptor = NULL;
	uint32_t size = 0;

	if (!pSession) {
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug("Current bssid::"QDF_MAC_ADDR_STR,
		QDF_MAC_ADDR_ARRAY(pSession->pConnectBssDesc->bssId));
	sme_debug("My bssid::"QDF_MAC_ADDR_STR" channel %d",
		QDF_MAC_ADDR_ARRAY(bssid.bytes), channel);

	size = pSession->pConnectBssDesc->length +
		sizeof(pSession->pConnectBssDesc->length);
	if (!size) {
		sme_err("length of bss descriptor is 0");
		return QDF_STATUS_E_FAILURE;
	}
	pNewBssDescriptor = qdf_mem_malloc(size);
	if (pNewBssDescriptor)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(pNewBssDescriptor, pSession->pConnectBssDesc, size);
	/* change the BSSID & channel as passed */
	qdf_mem_copy(pNewBssDescriptor->bssId, bssid.bytes,
			sizeof(tSirMacAddr));
	pNewBssDescriptor->channelId = channel;
	if (!csr_scan_append_bss_description(mac, pNewBssDescriptor)) {
		sme_err("csr_scan_append_bss_description failed");
		status = QDF_STATUS_E_FAILURE;
		goto free_mem;
	}
	sme_err("entry successfully added in scan cache");

free_mem:
	if (pNewBssDescriptor)
		qdf_mem_free(pNewBssDescriptor);

	return status;
}

#ifdef FEATURE_WLAN_ESE
/*  Update the TSF with the difference in system time */
void update_cckmtsf(uint32_t *timeStamp0, uint32_t *timeStamp1,
		    uint64_t *incr)
{
	uint64_t timeStamp64 = ((uint64_t) *timeStamp1 << 32) | (*timeStamp0);

	timeStamp64 = (uint64_t)(timeStamp64 + (*incr));
	*timeStamp0 = (uint32_t) (timeStamp64 & 0xffffffff);
	*timeStamp1 = (uint32_t) ((timeStamp64 >> 32) & 0xffffffff);
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
csr_rso_save_ap_to_scan_cache(struct mac_context *mac,
			      struct roam_offload_synch_ind *roam_sync_ind_ptr,
			      struct bss_description *bss_desc_ptr)
{
	uint32_t length = 0;
	struct tag_csrscan_result *scan_res_ptr = NULL;

	length = roam_sync_ind_ptr->beaconProbeRespLength -
		(SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET);
	scan_res_ptr = qdf_mem_malloc(sizeof(struct tag_csrscan_result) +
				length);
	if (!scan_res_ptr)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(&scan_res_ptr->Result.BssDescriptor,
			bss_desc_ptr,
			(sizeof(struct bss_description) + length));

	sme_debug("LFR3:Add BSSID to scan cache" QDF_MAC_ADDR_STR,
		QDF_MAC_ADDR_ARRAY(scan_res_ptr->Result.BssDescriptor.bssId));
	csr_scan_add_result(mac, scan_res_ptr);
	csr_free_scan_result_entry(mac, scan_res_ptr);
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * csr_get_fst_bssdescr_ptr() - This function returns the pointer to first bss
 * description from scan handle
 * @result_handle: an object for the result.
 *
 * Return: first bss descriptor from the scan handle.
 */
struct bss_description*
csr_get_fst_bssdescr_ptr(tScanResultHandle result_handle)
{
	tListElem *first_element = NULL;
	struct tag_csrscan_result *scan_result = NULL;
	struct scan_result_list *bss_list =
				(struct scan_result_list *)result_handle;

	if (!bss_list) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("Empty bss_list"));
		return NULL;
	}
	if (csr_ll_is_list_empty(&bss_list->List, LL_ACCESS_NOLOCK)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("bss_list->List is empty"));
		return NULL;
	}
	first_element = csr_ll_peek_head(&bss_list->List, LL_ACCESS_NOLOCK);
	if (!first_element) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("peer head return NULL"));
		return NULL;
	}

	scan_result = GET_BASE_ADDR(first_element, struct tag_csrscan_result,
					Link);

	return &scan_result->Result.BssDescriptor;
}

/**
 * csr_get_bssdescr_from_scan_handle() - This function to extract
 *                                       first bss description from scan handle
 * @result_handle: an object for the result.
 *
 * This function is written to extract first bss from scan handle.
 *
 * Return: first bss descriptor from the scan handle.
 */
struct bss_description *
csr_get_bssdescr_from_scan_handle(tScanResultHandle result_handle,
				  struct bss_description *bss_descr)
{
	tListElem *first_element = NULL;
	struct tag_csrscan_result *scan_result = NULL;
	struct scan_result_list *bss_list =
				(struct scan_result_list *)result_handle;

	if (!bss_list) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("Empty bss_list"));
		return NULL;
	}
	if (csr_ll_is_list_empty(&bss_list->List, LL_ACCESS_NOLOCK)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("bss_list->List is empty"));
		qdf_mem_free(bss_list);
		return NULL;
	}
	first_element = csr_ll_peek_head(&bss_list->List, LL_ACCESS_NOLOCK);
	if (first_element) {
		scan_result = GET_BASE_ADDR(first_element,
				struct tag_csrscan_result,
				Link);
		qdf_mem_copy(bss_descr,
				&scan_result->Result.BssDescriptor,
				sizeof(struct bss_description));
	}
	return bss_descr;
}

uint8_t
csr_get_channel_for_hw_mode_change(struct mac_context *mac_ctx,
				   tScanResultHandle result_handle,
				   uint32_t session_id)
{
	tListElem *next_element = NULL;
	struct tag_csrscan_result *scan_result = NULL;
	struct scan_result_list *bss_list =
				(struct scan_result_list *)result_handle;
	uint8_t channel_id = 0;

	if (!bss_list) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("Empty bss_list"));
		goto end;
	}

	if (policy_mgr_is_hw_dbs_capable(mac_ctx->psoc) == false) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  FL("driver isn't dbs capable"));
		goto end;
	}

	if (policy_mgr_is_current_hwmode_dbs(mac_ctx->psoc)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  FL("driver is already in DBS"));
		goto end;
	}

	if (!policy_mgr_is_dbs_allowed_for_concurrency(mac_ctx->psoc,
						       QDF_STA_MODE)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  FL("DBS is not allowed for this concurrency combo"));
		goto end;
	}

	if (!policy_mgr_is_hw_dbs_2x2_capable(mac_ctx->psoc) &&
	    !policy_mgr_is_hw_dbs_required_for_band(mac_ctx->psoc,
		HW_MODE_MAC_BAND_2G) &&
	    !policy_mgr_get_connection_count(mac_ctx->psoc)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  FL("1x1 DBS HW with no prior connection"));
		goto end;
	}

	if (csr_ll_is_list_empty(&bss_list->List, LL_ACCESS_NOLOCK)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("bss_list->List is empty"));
		qdf_mem_free(bss_list);
		goto end;
	}

	next_element = csr_ll_peek_head(&bss_list->List, LL_ACCESS_NOLOCK);
	while (next_element) {
		scan_result = GET_BASE_ADDR(next_element,
					    struct tag_csrscan_result,
					    Link);
		if (policy_mgr_is_hw_dbs_required_for_band(
				mac_ctx->psoc,
				HW_MODE_MAC_BAND_2G)) {
			if (WLAN_REG_IS_24GHZ_CH
				(scan_result->Result.BssDescriptor.channelId)) {
				channel_id =
					scan_result->
					Result.BssDescriptor.channelId;
				break;
			}
		} else {
			if (WLAN_REG_IS_24GHZ_CH
				(scan_result->Result.BssDescriptor.channelId) &&
			    policy_mgr_is_any_mode_active_on_band_along_with_session
				(mac_ctx->psoc,
				 session_id, POLICY_MGR_BAND_5)) {
				channel_id =
					scan_result->
					Result.BssDescriptor.channelId;
				break;
			}
			if (WLAN_REG_IS_5GHZ_CH
				(scan_result->Result.BssDescriptor.channelId) &&
			    policy_mgr_is_any_mode_active_on_band_along_with_session
				(mac_ctx->psoc,
				 session_id, POLICY_MGR_BAND_24)) {
				channel_id =
					scan_result->
					Result.BssDescriptor.channelId;
				break;
			}
		}
		next_element = csr_ll_next(&bss_list->List, next_element,
					   LL_ACCESS_NOLOCK);
	}
end:
	return channel_id;
}

uint8_t
csr_scan_get_channel_for_hw_mode_change(struct mac_context *mac_ctx,
					uint32_t session_id,
					struct csr_roam_profile *profile)
{
	tScanResultHandle result_handle = NULL;
	QDF_STATUS status;
	uint8_t first_ap_ch = 0;
	uint8_t candidate_chan;

	status = sme_get_ap_channel_from_scan_cache(profile, &result_handle,
						    &first_ap_ch);
	if (status != QDF_STATUS_SUCCESS || !result_handle || !first_ap_ch) {
		if (result_handle)
			sme_scan_result_purge(result_handle);
		sme_err("fail get scan result: status %d first ap ch %d",
			status, first_ap_ch);
		return 0;
	}
	if (!policy_mgr_check_for_session_conc(mac_ctx->psoc, session_id,
					       first_ap_ch)) {
		sme_scan_result_purge(result_handle);
		sme_err("Conc not allowed for the session %d ch %d",
			session_id, first_ap_ch);
		return 0;
	}

	candidate_chan = csr_get_channel_for_hw_mode_change(mac_ctx,
							    result_handle,
							    session_id);
	sme_scan_result_purge(result_handle);
	if (!candidate_chan)
		candidate_chan = first_ap_ch;
	sme_debug("session %d hw mode check candidate_chan %d", session_id,
		  candidate_chan);

	return candidate_chan;
}

static enum wlan_auth_type csr_covert_auth_type_new(enum csr_akm_type auth)
{
	switch (auth) {
	case eCSR_AUTH_TYPE_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		return WLAN_AUTH_TYPE_OPEN_SYSTEM;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		return WLAN_AUTH_TYPE_SHARED_KEY;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		return WLAN_AUTH_TYPE_AUTOSWITCH;
	case eCSR_AUTH_TYPE_WPA:
		return WLAN_AUTH_TYPE_WPA;
	case eCSR_AUTH_TYPE_WPA_PSK:
		return WLAN_AUTH_TYPE_WPA_PSK;
	case eCSR_AUTH_TYPE_WPA_NONE:
		return WLAN_AUTH_TYPE_WPA_NONE;
	case eCSR_AUTH_TYPE_RSN:
		return WLAN_AUTH_TYPE_RSN;
	case eCSR_AUTH_TYPE_RSN_PSK:
		return WLAN_AUTH_TYPE_RSN_PSK;
	case eCSR_AUTH_TYPE_FT_RSN:
		return WLAN_AUTH_TYPE_FT_RSN;
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		return WLAN_AUTH_TYPE_FT_RSN_PSK;
	case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
		return WLAN_AUTH_TYPE_WAPI_WAI_CERTIFICATE;
	case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
		return WLAN_AUTH_TYPE_WAPI_WAI_PSK;
	case eCSR_AUTH_TYPE_CCKM_WPA:
		return WLAN_AUTH_TYPE_CCKM_WPA;
	case eCSR_AUTH_TYPE_CCKM_RSN:
		return WLAN_AUTH_TYPE_CCKM_RSN;
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		return WLAN_AUTH_TYPE_RSN_PSK_SHA256;
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		return WLAN_AUTH_TYPE_RSN_8021X_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA256:
		return WLAN_AUTH_TYPE_FILS_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA384:
		return WLAN_AUTH_TYPE_FILS_SHA384;
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		return WLAN_AUTH_TYPE_FT_FILS_SHA256;
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		return WLAN_AUTH_TYPE_FT_FILS_SHA384;
	case eCSR_AUTH_TYPE_DPP_RSN:
		return WLAN_AUTH_TYPE_DPP_RSN;
	case eCSR_AUTH_TYPE_OWE:
		return WLAN_AUTH_TYPE_OWE;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		return WLAN_AUTH_TYPE_SUITEB_EAP_SHA256;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		return WLAN_AUTH_TYPE_SUITEB_EAP_SHA384;
	case eCSR_AUTH_TYPE_SAE:
		return WLAN_AUTH_TYPE_SAE;
	case eCSR_AUTH_TYPE_OSEN:
		return WLAN_AUTH_TYPE_OSEN;
	case eCSR_AUTH_TYPE_FT_SAE:
		return WLAN_AUTH_TYPE_FT_SAE;
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return WLAN_AUTH_TYPE_FT_SUITEB_EAP_SHA384;
	case eCSR_NUM_OF_SUPPORT_AUTH_TYPE:
	default:
		return WLAN_AUTH_TYPE_OPEN_SYSTEM;
	}
}

static enum csr_akm_type csr_covert_auth_type_old(enum wlan_auth_type auth)
{
	switch (auth) {
	case WLAN_AUTH_TYPE_OPEN_SYSTEM:
		return eCSR_AUTH_TYPE_OPEN_SYSTEM;
	case WLAN_AUTH_TYPE_SHARED_KEY:
		return eCSR_AUTH_TYPE_SHARED_KEY;
	case WLAN_AUTH_TYPE_AUTOSWITCH:
		return eCSR_AUTH_TYPE_AUTOSWITCH;
	case WLAN_AUTH_TYPE_WPA:
		return eCSR_AUTH_TYPE_WPA;
	case WLAN_AUTH_TYPE_WPA_PSK:
		return eCSR_AUTH_TYPE_WPA_PSK;
	case WLAN_AUTH_TYPE_WPA_NONE:
		return eCSR_AUTH_TYPE_WPA_NONE;
	case WLAN_AUTH_TYPE_RSN:
		return eCSR_AUTH_TYPE_RSN;
	case WLAN_AUTH_TYPE_RSN_PSK:
		return eCSR_AUTH_TYPE_RSN_PSK;
	case WLAN_AUTH_TYPE_FT_RSN:
		return eCSR_AUTH_TYPE_FT_RSN;
	case WLAN_AUTH_TYPE_FT_RSN_PSK:
		return eCSR_AUTH_TYPE_FT_RSN_PSK;
	case WLAN_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
		return eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE;
	case WLAN_AUTH_TYPE_WAPI_WAI_PSK:
		return eCSR_AUTH_TYPE_WAPI_WAI_PSK;
	case WLAN_AUTH_TYPE_CCKM_WPA:
		return eCSR_AUTH_TYPE_CCKM_WPA;
	case WLAN_AUTH_TYPE_CCKM_RSN:
		return eCSR_AUTH_TYPE_CCKM_RSN;
	case WLAN_AUTH_TYPE_RSN_PSK_SHA256:
		return eCSR_AUTH_TYPE_RSN_PSK_SHA256;
	case WLAN_AUTH_TYPE_RSN_8021X_SHA256:
		return eCSR_AUTH_TYPE_RSN_8021X_SHA256;
	case WLAN_AUTH_TYPE_FILS_SHA256:
		return eCSR_AUTH_TYPE_FILS_SHA256;
	case WLAN_AUTH_TYPE_FILS_SHA384:
		return eCSR_AUTH_TYPE_FILS_SHA384;
	case WLAN_AUTH_TYPE_FT_FILS_SHA256:
		return eCSR_AUTH_TYPE_FT_FILS_SHA256;
	case WLAN_AUTH_TYPE_FT_FILS_SHA384:
		return eCSR_AUTH_TYPE_FT_FILS_SHA384;
	case WLAN_AUTH_TYPE_DPP_RSN:
		return eCSR_AUTH_TYPE_DPP_RSN;
	case WLAN_AUTH_TYPE_OWE:
		return eCSR_AUTH_TYPE_OWE;
	case WLAN_AUTH_TYPE_SUITEB_EAP_SHA256:
		return eCSR_AUTH_TYPE_SUITEB_EAP_SHA256;
	case WLAN_AUTH_TYPE_SUITEB_EAP_SHA384:
		return eCSR_AUTH_TYPE_SUITEB_EAP_SHA384;
	case WLAN_AUTH_TYPE_SAE:
		return eCSR_AUTH_TYPE_SAE;
	case WLAN_AUTH_TYPE_OSEN:
		return eCSR_AUTH_TYPE_OSEN;
	case WLAN_AUTH_TYPE_FT_SAE:
		return eCSR_AUTH_TYPE_FT_SAE;
	case WLAN_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384;
	case WLAN_NUM_OF_SUPPORT_AUTH_TYPE:
	default:
		return eCSR_AUTH_TYPE_OPEN_SYSTEM;
	}
}

static enum wlan_enc_type csr_covert_enc_type_new(eCsrEncryptionType enc)
{
	switch (enc) {
	case eCSR_ENCRYPT_TYPE_NONE:
		return WLAN_ENCRYPT_TYPE_NONE;
	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
		return WLAN_ENCRYPT_TYPE_WEP40_STATICKEY;
	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
		return WLAN_ENCRYPT_TYPE_WEP104_STATICKEY;
	case eCSR_ENCRYPT_TYPE_WEP40:
		return WLAN_ENCRYPT_TYPE_WEP40;
	case eCSR_ENCRYPT_TYPE_WEP104:
		return WLAN_ENCRYPT_TYPE_WEP104;
	case eCSR_ENCRYPT_TYPE_TKIP:
		return WLAN_ENCRYPT_TYPE_TKIP;
	case eCSR_ENCRYPT_TYPE_AES:
		return WLAN_ENCRYPT_TYPE_AES;
	case eCSR_ENCRYPT_TYPE_WPI:
		return WLAN_ENCRYPT_TYPE_WPI;
	case eCSR_ENCRYPT_TYPE_KRK:
		return WLAN_ENCRYPT_TYPE_KRK;
	case eCSR_ENCRYPT_TYPE_BTK:
		return WLAN_ENCRYPT_TYPE_BTK;
	case eCSR_ENCRYPT_TYPE_AES_CMAC:
		return WLAN_ENCRYPT_TYPE_AES_CMAC;
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
		return WLAN_ENCRYPT_TYPE_AES_GCMP;
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
		return WLAN_ENCRYPT_TYPE_AES_GCMP_256;
	case eCSR_ENCRYPT_TYPE_ANY:
	default:
		return WLAN_ENCRYPT_TYPE_NONE;
	}
}

static eCsrEncryptionType csr_covert_enc_type_old(enum wlan_enc_type enc)
{
	switch (enc) {
	case WLAN_ENCRYPT_TYPE_NONE:
		return eCSR_ENCRYPT_TYPE_NONE;
	case WLAN_ENCRYPT_TYPE_WEP40_STATICKEY:
		return eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
	case WLAN_ENCRYPT_TYPE_WEP104_STATICKEY:
		return eCSR_ENCRYPT_TYPE_WEP104_STATICKEY;
	case WLAN_ENCRYPT_TYPE_WEP40:
		return eCSR_ENCRYPT_TYPE_WEP40;
	case WLAN_ENCRYPT_TYPE_WEP104:
		return eCSR_ENCRYPT_TYPE_WEP104;
	case WLAN_ENCRYPT_TYPE_TKIP:
		return eCSR_ENCRYPT_TYPE_TKIP;
	case WLAN_ENCRYPT_TYPE_AES:
		return eCSR_ENCRYPT_TYPE_AES;
	case WLAN_ENCRYPT_TYPE_WPI:
		return eCSR_ENCRYPT_TYPE_WPI;
	case WLAN_ENCRYPT_TYPE_KRK:
		return eCSR_ENCRYPT_TYPE_KRK;
	case WLAN_ENCRYPT_TYPE_BTK:
		return eCSR_ENCRYPT_TYPE_BTK;
	case WLAN_ENCRYPT_TYPE_AES_CMAC:
		return eCSR_ENCRYPT_TYPE_AES_CMAC;
	case WLAN_ENCRYPT_TYPE_AES_GCMP:
		return eCSR_ENCRYPT_TYPE_AES_GCMP;
	case WLAN_ENCRYPT_TYPE_AES_GCMP_256:
		return eCSR_ENCRYPT_TYPE_AES_GCMP_256;
	case WLAN_ENCRYPT_TYPE_ANY:
	default:
		return eCSR_ENCRYPT_TYPE_NONE;
	}
}

#ifdef WLAN_FEATURE_11W
/**
 * csr_update_pmf_cap: Updates PMF cap
 * @src_filter: Source filter
 * @dst_filter: Destination filter
 *
 * Return: None
 */
static void csr_update_pmf_cap(tCsrScanResultFilter *src_filter,
			       struct scan_filter *dst_filter) {

	if (src_filter->MFPCapable || src_filter->MFPEnabled)
		dst_filter->pmf_cap = WLAN_PMF_CAPABLE;
	if (src_filter->MFPRequired)
		dst_filter->pmf_cap = WLAN_PMF_REQUIRED;
}
#else
static inline void csr_update_pmf_cap(tCsrScanResultFilter *src_filter,
				      struct scan_filter *dst_filter)
{}
#endif

/**
 * csr_convert_dotllmod_phymode: Convert eCsrPhyMode to wlan_phymode
 * @dotllmode: phy mode
 *
 * Return: returns enum wlan_phymode
 */
static enum wlan_phymode csr_convert_dotllmod_phymode(eCsrPhyMode dotllmode)
{

	enum wlan_phymode con_phy_mode;

	switch (dotllmode) {
	case eCSR_DOT11_MODE_abg:
		con_phy_mode = WLAN_PHYMODE_AUTO;
		break;
	case eCSR_DOT11_MODE_11a:
		con_phy_mode = WLAN_PHYMODE_11A;
		break;
	case eCSR_DOT11_MODE_11b:
		con_phy_mode = WLAN_PHYMODE_11B;
		break;
	case eCSR_DOT11_MODE_11g:
		con_phy_mode = WLAN_PHYMODE_11G;
		break;
	case eCSR_DOT11_MODE_11n:
		con_phy_mode = WLAN_PHYMODE_11NA_HT20;
		break;
	case eCSR_DOT11_MODE_11g_ONLY:
		con_phy_mode = WLAN_PHYMODE_11G;
		break;
	case eCSR_DOT11_MODE_11n_ONLY:
		con_phy_mode = WLAN_PHYMODE_11NA_HT20;
		break;
	case eCSR_DOT11_MODE_11b_ONLY:
		con_phy_mode = WLAN_PHYMODE_11B;
		break;
	case eCSR_DOT11_MODE_11ac:
		con_phy_mode = WLAN_PHYMODE_11AC_VHT160;
		break;
	case eCSR_DOT11_MODE_11ac_ONLY:
		con_phy_mode = WLAN_PHYMODE_11AC_VHT160;
		break;
	case eCSR_DOT11_MODE_AUTO:
		con_phy_mode = WLAN_PHYMODE_AUTO;
		break;
	case eCSR_DOT11_MODE_11ax:
		con_phy_mode = WLAN_PHYMODE_11AXA_HE160;
		break;
	case eCSR_DOT11_MODE_11ax_ONLY:
		con_phy_mode = WLAN_PHYMODE_11AXA_HE160;
		break;
	default:
		con_phy_mode = WLAN_PHYMODE_AUTO;
		break;
	}

	return con_phy_mode;
}

#ifdef WLAN_ADAPTIVE_11R
/**
 * csr_update_adaptive_11r_scan_filter - Copy adaptive 11r ini to scan
 * module
 * @mac_ctx: Pointer to mac_context
 * @scan_filter: Scan filter to be sent to scan module
 *
 * Return: None
 */
static void
csr_update_adaptive_11r_scan_filter(struct mac_context *mac_ctx,
				    struct scan_filter *filter)
{
	filter->enable_adaptive_11r =
		   mac_ctx->mlme_cfg->lfr.enable_adaptive_11r;
}
#else
static inline void
csr_update_adaptive_11r_scan_filter(struct mac_context *mac_ctx,
				    struct scan_filter *filter)
{
	filter->enable_adaptive_11r = false;
}
#endif

static QDF_STATUS csr_prepare_scan_filter(struct mac_context *mac_ctx,
					  tCsrScanResultFilter *pFilter,
					  struct scan_filter *filter)
{
	int i;
	uint32_t len = 0;
	QDF_STATUS status;
	enum policy_mgr_con_mode new_mode;

	filter->num_of_bssid = pFilter->BSSIDs.numOfBSSIDs;
	if (filter->num_of_bssid > WLAN_SCAN_FILTER_NUM_BSSID)
		filter->num_of_bssid = WLAN_SCAN_FILTER_NUM_BSSID;
	for (i = 0; i < filter->num_of_bssid; i++)
		qdf_mem_copy(filter->bssid_list[i].bytes,
			pFilter->BSSIDs.bssid[i].bytes,
			QDF_MAC_ADDR_SIZE);

	filter->age_threshold = pFilter->age_threshold;

	filter->num_of_ssid = pFilter->SSIDs.numOfSSIDs;
	if (filter->num_of_ssid > WLAN_SCAN_FILTER_NUM_SSID)
		filter->num_of_ssid = WLAN_SCAN_FILTER_NUM_SSID;
	for (i = 0; i < filter->num_of_ssid; i++) {
		filter->ssid_list[i].length =
			pFilter->SSIDs.SSIDList[i].SSID.length;
		qdf_mem_copy(filter->ssid_list[i].ssid,
			pFilter->SSIDs.SSIDList[i].SSID.ssId,
			filter->ssid_list[i].length);
	}

	filter->num_of_channels =
		pFilter->ChannelInfo.numOfChannels;
	if (filter->num_of_channels > NUM_CHANNELS)
		filter->num_of_channels = NUM_CHANNELS;
	qdf_mem_copy(filter->channel_list,
			pFilter->ChannelInfo.ChannelList,
			filter->num_of_channels);

	if (pFilter->realm_check) {
		filter->fils_scan_filter.realm_check = true;
		qdf_mem_copy(filter->fils_scan_filter.fils_realm,
			pFilter->fils_realm, REAM_HASH_LEN);
	}

	if (pFilter->force_rsne_override) {

		sme_debug("force_rsne_override set auth type and enctype to any and ignore pmf cap");

		filter->num_of_auth = 1;
		filter->auth_type[0] = WLAN_AUTH_TYPE_ANY;
		filter->num_of_enc_type = 1;
		filter->enc_type[0] = WLAN_ENCRYPT_TYPE_ANY;
		filter->num_of_mc_enc_type = 1;
		filter->mc_enc_type[0] = WLAN_ENCRYPT_TYPE_ANY;

		filter->ignore_pmf_cap = true;
	} else {
		filter->num_of_auth =
			pFilter->authType.numEntries;
		if (filter->num_of_auth > WLAN_NUM_OF_SUPPORT_AUTH_TYPE)
			filter->num_of_auth = WLAN_NUM_OF_SUPPORT_AUTH_TYPE;
		for (i = 0; i < filter->num_of_auth; i++)
			filter->auth_type[i] =
			  csr_covert_auth_type_new(
			  pFilter->authType.authType[i]);
		filter->num_of_enc_type =
			pFilter->EncryptionType.numEntries;
		if (filter->num_of_enc_type > WLAN_NUM_OF_ENCRYPT_TYPE)
			filter->num_of_enc_type = WLAN_NUM_OF_ENCRYPT_TYPE;
		for (i = 0; i < filter->num_of_enc_type; i++)
			filter->enc_type[i] =
			  csr_covert_enc_type_new(
			  pFilter->EncryptionType.encryptionType[i]);
		filter->num_of_mc_enc_type =
				pFilter->mcEncryptionType.numEntries;
		if (filter->num_of_mc_enc_type > WLAN_NUM_OF_ENCRYPT_TYPE)
			filter->num_of_mc_enc_type = WLAN_NUM_OF_ENCRYPT_TYPE;
		for (i = 0; i < filter->num_of_mc_enc_type; i++)
			filter->mc_enc_type[i] =
			  csr_covert_enc_type_new(
			  pFilter->mcEncryptionType.encryptionType[i]);
	}
	qdf_mem_copy(filter->country, pFilter->countryCode,
		     CFG_COUNTRY_CODE_LEN);

	if (pFilter->bWPSAssociation || pFilter->bOSENAssociation)
		filter->ignore_auth_enc_type = true;

	filter->rrm_measurement_filter = pFilter->fMeasurement;

	filter->mobility_domain = pFilter->mdid.mobility_domain;

	filter->p2p_results = pFilter->p2pResult;

	csr_update_pmf_cap(pFilter, filter);

	if (pFilter->BSSType == eCSR_BSS_TYPE_INFRASTRUCTURE)
		filter->bss_type = WLAN_TYPE_BSS;
	else if (pFilter->BSSType == eCSR_BSS_TYPE_IBSS ||
		pFilter->BSSType == eCSR_BSS_TYPE_START_IBSS)
		filter->bss_type = WLAN_TYPE_IBSS;
	else
		filter->bss_type = WLAN_TYPE_ANY;

	filter->dot11_mode = csr_convert_dotllmod_phymode(pFilter->phyMode);

	// enable bss scoring for only STA mode
	if (pFilter->csrPersona == QDF_STA_MODE)
		filter->bss_scoring_required = true;
	else
		filter->bss_scoring_required = false;

	csr_update_adaptive_11r_scan_filter(mac_ctx, filter);

	if (!pFilter->BSSIDs.numOfBSSIDs) {
		if (policy_mgr_map_concurrency_mode(
		   &pFilter->csrPersona, &new_mode)) {
			status = policy_mgr_get_pcl(mac_ctx->psoc, new_mode,
				filter->pcl_channel_list, &len,
				filter->pcl_weight_list, NUM_CHANNELS);
			filter->num_of_pcl_channels = (uint8_t)len;
		}
	}
	qdf_mem_copy(filter->bssid_hint.bytes,
			pFilter->bssid_hint.bytes,
			QDF_MAC_ADDR_SIZE);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * csr_update_bss_with_fils_data: Fill FILS params in bss desc from scan entry
 * @mac_ctx: mac context
 * @scan_entry: scan entry
 * @bss_descr: bss description
 */
static void csr_update_bss_with_fils_data(struct mac_context *mac_ctx,
					  struct scan_cache_entry *scan_entry,
					  struct bss_description *bss_descr)
{
	int ret;
	tDot11fIEfils_indication fils_indication = {0};
	struct sir_fils_indication fils_ind;

	if (!scan_entry->ie_list.fils_indication)
		return;

	ret = dot11f_unpack_ie_fils_indication(mac_ctx,
				scan_entry->ie_list.fils_indication +
				SIR_FILS_IND_ELEM_OFFSET,
				*(scan_entry->ie_list.fils_indication + 1),
				&fils_indication, false);
	if (DOT11F_FAILED(ret)) {
		sme_err("unpack failed ret: 0x%x", ret);
		return;
	}

	update_fils_data(&fils_ind, &fils_indication);
	if (fils_ind.realm_identifier.realm_cnt > SIR_MAX_REALM_COUNT)
		fils_ind.realm_identifier.realm_cnt = SIR_MAX_REALM_COUNT;

	bss_descr->fils_info_element.realm_cnt =
		fils_ind.realm_identifier.realm_cnt;
	qdf_mem_copy(bss_descr->fils_info_element.realm,
			fils_ind.realm_identifier.realm,
			bss_descr->fils_info_element.realm_cnt * SIR_REALM_LEN);
	if (fils_ind.cache_identifier.is_present) {
		bss_descr->fils_info_element.is_cache_id_present = true;
		qdf_mem_copy(bss_descr->fils_info_element.cache_id,
			fils_ind.cache_identifier.identifier, CACHE_ID_LEN);
	}
	if (fils_ind.is_fils_sk_auth_supported)
		bss_descr->fils_info_element.is_fils_sk_supported = true;
}
#else
static void csr_update_bss_with_fils_data(struct mac_context *mac_ctx,
					  struct scan_cache_entry *scan_entry,
					  struct bss_description *bss_descr)
{ }
#endif

/**
 * csr_is_assoc_disallowed() - Find if assoc disallowed
 * bit is set in AP's beacon or probe response
 * @mac_ctx: mac context
 * @scan_entry: scan entry
 *
 * Return: True if assoc disallowed is set else false
 */
static bool csr_is_assoc_disallowed(struct mac_context *mac_ctx,
				    struct scan_cache_entry *scan_entry)
{
	int ret;
	tDot11fIEMBO_IE mbo_ie = {0};
	uint8_t *mbo_oce;

	mbo_oce = util_scan_entry_mbo_oce(scan_entry);

	if (!mbo_oce)
		return false;

	ret = dot11f_unpack_ie_MBO_IE(mac_ctx, mbo_oce + SIR_MBO_ELEM_OFFSET,
				      *(mbo_oce + 1) - SIR_MAC_MBO_OUI_SIZE,
				      &mbo_ie, false);

	if (DOT11F_FAILED(ret)) {
		sme_err("unpack failed ret: 0x%x", ret);
		return false;
	}

	return mbo_ie.assoc_disallowed.present;
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_fill_single_pmk_ap_cap_from_scan_entry() - WAP3_SPMK VSIE from scan
 * entry
 * @bss_desc: BSS Descriptor
 * @scan_entry: scan entry
 *
 * Return: None
 */
static void
csr_fill_single_pmk_ap_cap_from_scan_entry(struct bss_description *bss_desc,
					   struct scan_cache_entry *scan_entry)
{
	bss_desc->is_single_pmk = util_scan_entry_single_pmk(scan_entry);
}

#else
static inline void
csr_fill_single_pmk_ap_cap_from_scan_entry(struct bss_description *bss_desc,
					   struct scan_cache_entry *scan_entry)
{
}
#endif
static QDF_STATUS csr_fill_bss_from_scan_entry(struct mac_context *mac_ctx,
					struct scan_cache_entry *scan_entry,
					struct tag_csrscan_result **p_result)
{
	tDot11fBeaconIEs *bcn_ies;
	struct bss_description *bss_desc;
	tCsrScanResultInfo *result_info;
	tpSirMacMgmtHdr hdr;
	uint8_t *ie_ptr;
	struct tag_csrscan_result *bss;
	uint32_t bss_len, alloc_len, ie_len;
	QDF_STATUS status;
	enum channel_state ap_channel_state;

	ap_channel_state =
		wlan_reg_get_channel_state(mac_ctx->pdev,
					   scan_entry->channel.chan_idx);
	if (ap_channel_state == CHANNEL_STATE_DISABLE ||
	    ap_channel_state == CHANNEL_STATE_INVALID) {
		sme_err("BSS %pM channel %d invalid, not populating this BSSID",
			scan_entry->bssid.bytes, scan_entry->channel.chan_idx);
		return QDF_STATUS_E_INVAL;
	}

	ie_len = util_scan_entry_ie_len(scan_entry);
	ie_ptr = util_scan_entry_ie_data(scan_entry);

	hdr = (tpSirMacMgmtHdr)scan_entry->raw_frame.ptr;

	bss_len = (uint16_t)(offsetof(struct bss_description,
			   ieFields[0]) + ie_len);
	alloc_len = sizeof(struct tag_csrscan_result) + bss_len;
	bss = qdf_mem_malloc(alloc_len);
	if (!bss)
		return QDF_STATUS_E_NOMEM;

	bss->AgingCount =
		(int32_t) mac_ctx->roam.configParam.agingCount;
	bss->ucEncryptionType =
		csr_covert_enc_type_old(scan_entry->neg_sec_info.uc_enc);
	bss->mcEncryptionType =
		csr_covert_enc_type_old(scan_entry->neg_sec_info.mc_enc);
	bss->authType =
		csr_covert_auth_type_old(scan_entry->neg_sec_info.auth_type);
	bss->bss_score = scan_entry->bss_score;

	result_info = &bss->Result;
	result_info->ssId.length = scan_entry->ssid.length;
	qdf_mem_copy(result_info->ssId.ssId,
		scan_entry->ssid.ssid,
		result_info->ssId.length);
	result_info->timer = scan_entry->hidden_ssid_timestamp;

	bss_desc = &result_info->BssDescriptor;

	bss_desc->length = (uint16_t) (offsetof(struct bss_description,
			   ieFields[0]) - sizeof(bss_desc->length) + ie_len);

	qdf_mem_copy(bss_desc->bssId,
			scan_entry->bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	bss_desc->scansystimensec = scan_entry->scan_entry_time;
	qdf_mem_copy(bss_desc->timeStamp,
		scan_entry->tsf_info.data, 8);

	bss_desc->beaconInterval = scan_entry->bcn_int;
	bss_desc->capabilityInfo = scan_entry->cap_info.value;

	if (WLAN_REG_IS_5GHZ_CH(scan_entry->channel.chan_idx))
		bss_desc->nwType = eSIR_11A_NW_TYPE;
	else if (scan_entry->phy_mode == WLAN_PHYMODE_11B)
		bss_desc->nwType = eSIR_11B_NW_TYPE;
	else
		bss_desc->nwType = eSIR_11G_NW_TYPE;

	bss_desc->rssi = scan_entry->rssi_raw;
	bss_desc->rssi_raw = scan_entry->rssi_raw;

	/* channelId what peer sent in beacon/probersp. */
	bss_desc->channelId =
		scan_entry->channel.chan_idx;
	/* channelId on which we are parked at. */
	/* used only in scan case. */
	bss_desc->channelIdSelf =
		scan_entry->channel.chan_idx;
	bss_desc->rx_channel = bss_desc->channelIdSelf;
	bss_desc->received_time =
		scan_entry->scan_entry_time;
	bss_desc->startTSF[0] =
		mac_ctx->rrm.rrmPEContext.startTSF[0];
	bss_desc->startTSF[1] =
		mac_ctx->rrm.rrmPEContext.startTSF[1];
	bss_desc->parentTSF =
		scan_entry->rrm_parent_tsf;
	bss_desc->fProbeRsp = (scan_entry->frm_subtype ==
			  MGMT_SUBTYPE_PROBE_RESP);
	bss_desc->seq_ctrl = hdr->seqControl;
	bss_desc->tsf_delta = scan_entry->tsf_delta;
	bss_desc->assoc_disallowed = csr_is_assoc_disallowed(mac_ctx,
							     scan_entry);
	bss_desc->adaptive_11r_ap = scan_entry->adaptive_11r_ap;

	bss_desc->mbo_oce_enabled_ap =
			util_scan_entry_mbo_oce(scan_entry) ? true : false;

	csr_fill_single_pmk_ap_cap_from_scan_entry(bss_desc, scan_entry);

	qdf_mem_copy(&bss_desc->mbssid_info, &scan_entry->mbssid_info,
		     sizeof(struct scan_mbssid_info));

	qdf_mem_copy((uint8_t *) &bss_desc->ieFields,
		ie_ptr, ie_len);

	status = csr_get_parsed_bss_description_ies(mac_ctx,
			  bss_desc, &bcn_ies);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(bss);
		return status;
	}
	result_info->pvIes = bcn_ies;

	if (bcn_ies->MobilityDomain.present) {
		bss_desc->mdiePresent = true;
		qdf_mem_copy((uint8_t *)&(bss_desc->mdie[0]),
			     (uint8_t *)&(bcn_ies->MobilityDomain.MDID),
			     sizeof(uint16_t));
		bss_desc->mdie[2] =
			((bcn_ies->MobilityDomain.overDSCap << 0) |
			(bcn_ies->MobilityDomain.resourceReqCap << 1));
	}
#ifdef FEATURE_WLAN_ESE
	if (bcn_ies->QBSSLoad.present) {
		bss_desc->QBSSLoad_present = true;
		bss_desc->QBSSLoad_avail =
			bcn_ies->QBSSLoad.avail;
	}
#endif
	csr_update_bss_with_fils_data(mac_ctx, scan_entry, bss_desc);
	if (scan_entry->alt_wcn_ie.ptr) {
		bss_desc->WscIeLen = scan_entry->alt_wcn_ie.len;
		qdf_mem_copy(bss_desc->WscIeProbeRsp,
			scan_entry->alt_wcn_ie.ptr,
			scan_entry->alt_wcn_ie.len);
	}

	*p_result = bss;
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS csr_parse_scan_list(struct mac_context *mac_ctx,
				      struct scan_result_list *ret_list,
				      qdf_list_t *scan_list)
{
	struct tag_csrscan_result *pResult = NULL;
	struct scan_cache_node *cur_node = NULL;
	struct scan_cache_node *next_node = NULL;

	qdf_list_peek_front(scan_list, (qdf_list_node_t **) &cur_node);

	while (cur_node) {
		qdf_list_peek_next(scan_list, (qdf_list_node_t *) cur_node,
				  (qdf_list_node_t **) &next_node);
		pResult = NULL;
		csr_fill_bss_from_scan_entry(mac_ctx,
					     cur_node->entry, &pResult);
		if (pResult)
			csr_ll_insert_tail(&ret_list->List, &pResult->Link,
					   LL_ACCESS_NOLOCK);
		cur_node = next_node;
		next_node = NULL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_remove_ap_with_assoc_disallowed() - Remove APs with assoc
 * disallowed bit set
 * @mac_ctx: mac context
 * @scan_list: candidate list for the connection
 *
 * Return: None
 */
static void csr_remove_ap_with_assoc_disallowed(struct mac_context *mac_ctx,
					     struct scan_result_list *scan_list)
{
	tListElem *cur_entry;
	tListElem *next_entry;
	struct tag_csrscan_result *scan_res;

	if (!scan_list)
		return;

	cur_entry = csr_ll_peek_head(&scan_list->List, LL_ACCESS_NOLOCK);
	while (cur_entry) {
		scan_res = GET_BASE_ADDR(cur_entry, struct tag_csrscan_result,
					 Link);
		next_entry = csr_ll_next(&scan_list->List, cur_entry,
					 LL_ACCESS_NOLOCK);

		if (!mac_ctx->ignore_assoc_disallowed &&
		    scan_res->Result.BssDescriptor.assoc_disallowed) {
			csr_ll_remove_entry(&scan_list->List, cur_entry,
					    LL_ACCESS_NOLOCK);
			csr_free_scan_result_entry(mac_ctx, scan_res);
		}
		cur_entry = next_entry;
		next_entry = NULL;
	}
}

QDF_STATUS csr_scan_get_result(struct mac_context *mac_ctx,
			       tCsrScanResultFilter *pFilter,
			       tScanResultHandle *results)
{
	QDF_STATUS status;
	struct scan_result_list *ret_list = NULL;
	qdf_list_t *list = NULL;
	struct scan_filter *filter = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	uint32_t num_bss = 0;

	if (results)
		*results = CSR_INVALID_SCANRESULT_HANDLE;

	pdev = wlan_objmgr_get_pdev_by_id(mac_ctx->psoc,
		0, WLAN_LEGACY_MAC_ID);
	if (!pdev) {
		sme_err("pdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (pFilter) {
		filter = qdf_mem_malloc(sizeof(*filter));
		if (!filter) {
			status = QDF_STATUS_E_NOMEM;
			goto error;
		}

		status = csr_prepare_scan_filter(mac_ctx, pFilter, filter);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("Prepare filter failed");
			goto error;
		}
	}

	list = ucfg_scan_get_result(pdev,
		    pFilter ? filter : NULL);
	if (list) {
		num_bss = qdf_list_size(list);
		sme_debug("num_entries %d", num_bss);
	}
	/* Filter the scan list with the blacklist, rssi reject, avoided APs */
	if (pFilter && pFilter->csrPersona == QDF_STA_MODE)
		wlan_blm_filter_bssid(pdev, list);

	if (!list || (list && !qdf_list_size(list))) {
		sme_debug("scan list empty");
		if (num_bss)
			status = QDF_STATUS_E_EXISTS;
		else
			status = QDF_STATUS_E_NULL_VALUE;
		goto error;
	}

	ret_list = qdf_mem_malloc(sizeof(struct scan_result_list));
	if (!ret_list) {
		status = QDF_STATUS_E_NOMEM;
		goto error;
	}

	csr_ll_open(&ret_list->List);
	ret_list->pCurEntry = NULL;
	status = csr_parse_scan_list(mac_ctx, ret_list, list);
	if (QDF_IS_STATUS_ERROR(status) || !results)
		/* Fail or No one wants the result. */
		csr_scan_result_purge(mac_ctx, (tScanResultHandle) ret_list);
	else {
		if (pFilter && pFilter->csrPersona == QDF_STA_MODE)
			csr_remove_ap_with_assoc_disallowed(mac_ctx, ret_list);

		if (!csr_ll_count(&ret_list->List)) {
			/* This mean that there is no match */
			csr_ll_close(&ret_list->List);
			qdf_mem_free(ret_list);
			/*
			 * Do not trigger scan for ssid if the scan entries
			 * are removed either due to rssi reject or assoc
			 * disallowed.
			 */
			if (num_bss)
				status = QDF_STATUS_E_EXISTS;
			else
				status = QDF_STATUS_E_NULL_VALUE;
		} else if (results) {
			*results = ret_list;
		}
	}

error:
	if (filter)
		qdf_mem_free(filter);
	if (list)
		ucfg_scan_purge_results(list);
	if (pdev)
		wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);

	return status;
}

QDF_STATUS csr_scan_get_result_for_bssid(struct mac_context *mac_ctx,
					 struct qdf_mac_addr *bssid,
					 tCsrScanResultInfo *res)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tCsrScanResultFilter *scan_filter = NULL;
	tScanResultHandle filtered_scan_result = NULL;
	tCsrScanResultInfo *scan_result;

	if (!mac_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				FL("mac_ctx is NULL"));
		return QDF_STATUS_E_FAILURE;
	}

	scan_filter = qdf_mem_malloc(sizeof(tCsrScanResultFilter));
	if (!scan_filter)
		return QDF_STATUS_E_NOMEM;

	scan_filter->BSSIDs.bssid = qdf_mem_malloc(sizeof(*bssid));
	if (!scan_filter->BSSIDs.bssid) {
		status = QDF_STATUS_E_FAILURE;
		goto free_filter;
	}

	scan_filter->BSSIDs.numOfBSSIDs = 1;
	qdf_mem_copy(scan_filter->BSSIDs.bssid, bssid, sizeof(*bssid));

	status = csr_scan_get_result(mac_ctx, scan_filter,
				&filtered_scan_result);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to get scan result");
		goto free_filter;
	}

	scan_result = csr_scan_result_get_first(mac_ctx, filtered_scan_result);

	if (scan_result) {
		res->pvIes = NULL;
		res->ssId.length = scan_result->ssId.length;
		qdf_mem_copy(&res->ssId.ssId, &scan_result->ssId.ssId,
			res->ssId.length);
		res->timer = scan_result->timer;
		qdf_mem_copy(&res->BssDescriptor, &scan_result->BssDescriptor,
			sizeof(struct bss_description));
		status = QDF_STATUS_SUCCESS;
	} else {
		status = QDF_STATUS_E_FAILURE;
	}

	csr_scan_result_purge(mac_ctx, filtered_scan_result);

free_filter:
	csr_free_scan_filter(mac_ctx, scan_filter);
	if (scan_filter)
		qdf_mem_free(scan_filter);

	return status;
}

static inline QDF_STATUS
csr_flush_scan_results(struct mac_context *mac_ctx,
		       struct scan_filter *filter)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	QDF_STATUS status;

	pdev = wlan_objmgr_get_pdev_by_id(mac_ctx->psoc,
		0, WLAN_LEGACY_MAC_ID);
	if (!pdev) {
		sme_err("pdev is NULL");
		return QDF_STATUS_E_INVAL;
	}
	status = ucfg_scan_flush_results(pdev, filter);

	wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);
	return status;
}

static inline void csr_flush_bssid(struct mac_context *mac_ctx,
				   uint8_t *bssid)
{
	struct scan_filter *filter;

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		return;

	filter->num_of_bssid = 1;
	qdf_mem_copy(filter->bssid_list[0].bytes,
		     bssid, QDF_MAC_ADDR_SIZE);

	csr_flush_scan_results(mac_ctx, filter);
	sme_debug("Removed BSS entry:%pM", bssid);
	if (filter)
		qdf_mem_free(filter);
}

void csr_remove_bssid_from_scan_list(struct mac_context *mac_ctx,
				     tSirMacAddr bssid)
{
	csr_flush_bssid(mac_ctx, bssid);
}

static void csr_dump_occupied_chan_list(struct csr_channel *occupied_ch)
{
	uint8_t idx;
	uint32_t buff_len;
	char *chan_buff;
	uint32_t len = 0;

	buff_len = (occupied_ch->numChannels * 5) + 1;
	chan_buff = qdf_mem_malloc(buff_len);
	if (!chan_buff)
		return;

	for (idx = 0; idx < occupied_ch->numChannels; idx++)
		len += qdf_scnprintf(chan_buff + len, buff_len - len, " %d",
				     occupied_ch->channelList[idx]);

	sme_nofl_debug("Occupied chan list[%d]:%s",
		       occupied_ch->numChannels, chan_buff);

	qdf_mem_free(chan_buff);
}
void csr_init_occupied_channels_list(struct mac_context *mac_ctx,
				     uint8_t sessionId)
{
	qdf_list_t *list = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	qdf_list_node_t *cur_lst = NULL;
	qdf_list_node_t *next_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	struct scan_filter *filter;
	tpCsrNeighborRoamControlInfo neighbor_roam_info =
		&mac_ctx->roam.neighborRoamInfo[sessionId];
	tCsrRoamConnectedProfile *profile = NULL;
	uint8_t ch;

	if (!(mac_ctx && mac_ctx->roam.roamSession &&
	      CSR_IS_SESSION_VALID(mac_ctx, sessionId))) {
		sme_debug("Invalid session");
		return;
	}
	if (neighbor_roam_info->cfgParams.specific_chan_info.numOfChannels) {
		/*
		 * Ini file contains neighbor scan channel list, hence NO need
		 * to build occupied channel list"
		 */
		sme_debug("Ini contains neighbor scan ch list");
		return;
	}

	profile = &mac_ctx->roam.roamSession[sessionId].connectedProfile;
	if (!profile)
		return;

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter) {
		sme_err("filter is NULL");
		return;
	}

	filter->num_of_auth = 1;
	filter->auth_type[0] = csr_covert_auth_type_new(profile->AuthType);
	filter->num_of_enc_type = 1;
	filter->enc_type[0] = csr_covert_enc_type_new(profile->EncryptionType);
	filter->num_of_mc_enc_type = 1;
	filter->mc_enc_type[0] =
		csr_covert_enc_type_new(profile->mcEncryptionType);
	filter->num_of_ssid = 1;
	filter->ssid_list[0].length = profile->SSID.length;
	qdf_mem_copy(filter->ssid_list[0].ssid, profile->SSID.ssId,
		     profile->SSID.length);
	csr_update_pmf_cap_from_connected_profile(profile, filter);

	pdev = wlan_objmgr_get_pdev_by_id(mac_ctx->psoc, 0, WLAN_LEGACY_MAC_ID);

	if (!pdev) {
		sme_err("pdev is NULL");
		qdf_mem_free(filter);
		return;
	}

	/* Empty occupied channels here */
	mac_ctx->scan.occupiedChannels[sessionId].numChannels = 0;
	mac_ctx->scan.roam_candidate_count[sessionId] = 0;

	csr_add_to_occupied_channels(
			mac_ctx, profile->operationChannel,
			sessionId,
			&mac_ctx->scan.occupiedChannels[sessionId],
			true);
	list = ucfg_scan_get_result(pdev, filter);
	if (list)
		sme_debug("num_entries %d", qdf_list_size(list));
	if (!list || (list && !qdf_list_size(list))) {
		wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);
		qdf_mem_free(filter);
		if (list)
			ucfg_scan_purge_results(list);
		csr_dump_occupied_chan_list(
			&mac_ctx->scan.occupiedChannels[sessionId]);
		return;
	}

	qdf_list_peek_front(list, &cur_lst);
	while (cur_lst) {
		cur_node = qdf_container_of(cur_lst, struct scan_cache_node,
					    node);
		ch = cur_node->entry->channel.chan_idx;
		csr_add_to_occupied_channels(
				mac_ctx, ch,
				sessionId,
				&mac_ctx->scan.occupiedChannels[sessionId],
				true);
		qdf_list_peek_next(list, cur_lst, &next_lst);
		cur_lst = next_lst;
		next_lst = NULL;
	}
	csr_dump_occupied_chan_list(&mac_ctx->scan.occupiedChannels[sessionId]);

	qdf_mem_free(filter);
	ucfg_scan_purge_results(list);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);
}

/**
 * csr_scan_filter_results: filter scan result based
 * on valid channel list number.
 * @mac_ctx: mac context
 *
 * Get scan result from scan list and Check Scan result channel number
 * with 11d channel list if channel number is found in 11d channel list
 * then do not remove scan result entry from scan list
 *
 * return: QDF Status
 */
QDF_STATUS csr_scan_filter_results(struct mac_context *mac_ctx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t len = sizeof(mac_ctx->roam.validChannelList);
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_objmgr_get_pdev_by_id(mac_ctx->psoc,
		0, WLAN_LEGACY_MAC_ID);
	if (!pdev) {
		sme_err("pdev is NULL");
		return QDF_STATUS_E_INVAL;
	}
	status = csr_get_cfg_valid_channels(mac_ctx,
			  mac_ctx->roam.validChannelList,
			  &len);

	/* Get valid channels list from CFG */
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);
		sme_err("Failed to get Channel list from CFG");
		return status;
	}
	sme_debug("No of valid channel %d", len);

	ucfg_scan_filter_valid_channel(pdev,
		mac_ctx->roam.validChannelList, len);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_LEGACY_MAC_ID);
	return QDF_STATUS_SUCCESS;
}
