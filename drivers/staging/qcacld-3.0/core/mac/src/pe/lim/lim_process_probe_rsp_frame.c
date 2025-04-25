/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *
 * This file lim_process_probe_rsp_frame.cc contains the code
 * for processing Probe Response Frame.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wni_api.h"
#include "wni_cfg.h"
#include "ani_global.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_send_messages.h"
#include "lim_mlo.h"
#include "wlan_mlo_mgr_sta.h"
#include "parser_api.h"

/**
 * lim_validate_ie_information_in_probe_rsp_frame () - validates ie
 * information in probe response.
 * @mac_ctx: mac context
 * @pRxPacketInfo: Rx packet info
 *
 * Return: 0 on success, one on failure
 */
static QDF_STATUS
lim_validate_ie_information_in_probe_rsp_frame(struct mac_context *mac_ctx,
				uint8_t *pRxPacketInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t *pframe;
	uint32_t nframe;
	uint32_t missing_rsn_bytes;

	/*
	 * Validate a Probe response frame for malformed frame.
	 * If the frame is malformed then do not consider as it
	 * may cause problem fetching wrong IE values
	 */

	if (WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo) <
		(SIR_MAC_B_PR_SSID_OFFSET + SIR_MAC_MIN_IE_LEN))
		return QDF_STATUS_E_FAILURE;

	pframe = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	nframe = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
	missing_rsn_bytes = 0;

	status = sir_validate_and_rectify_ies(mac_ctx,
			pframe, nframe, &missing_rsn_bytes);

	if (status == QDF_STATUS_SUCCESS)
		WMA_GET_RX_MPDU_LEN(pRxPacketInfo) += missing_rsn_bytes;

	return status;
}

/**
 * lim_process_updated_ies_in_probe_rsp() -  process IEs of probe rsp frame
 * @mac_ctx: pointer to global mac context
 * @session_entry: pointer to pe session
 * @probe_rsp: pointer to structure tSirProbeRespBeacon
 *
 * Return: void
 */
static void
lim_process_updated_ies_in_probe_rsp(struct mac_context *mac_ctx,
				     struct pe_session *session_entry,
				     tSirProbeRespBeacon *probe_rsp)
{
	bool qos_enabled;
	bool wme_enabled;
	tpDphHashNode sta_ds;
	QDF_STATUS status;

	if (session_entry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE) {
		/*
		 * Now Process EDCA Parameters, if EDCAParamSet
		 * count is different.
		 * -- While processing beacons in link established
		 * state if it is determined that
		 * QoS Info IE has a different count for EDCA Params,
		 * and EDCA IE is not present in beacon,
		 * then probe req is sent out to get the EDCA params.
		 */
		sta_ds = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
					    &session_entry->dph.dphHashTable);

		limGetQosMode(session_entry, &qos_enabled);
		limGetWmeMode(session_entry, &wme_enabled);
		pe_debug("wmeEdcaPresent: %d wme_enabled: %d edcaPresent: %d, qos_enabled: %d edcaParams.qosInfo.count: %d schObject.gLimEdcaParamSetCount: %d",
			 probe_rsp->wmeEdcaPresent, wme_enabled,
			 probe_rsp->edcaPresent, qos_enabled,
			 probe_rsp->edcaParams.qosInfo.count,
			 session_entry->gLimEdcaParamSetCount);

		if (((probe_rsp->wmeEdcaPresent && wme_enabled) ||
		     (probe_rsp->edcaPresent && qos_enabled)) &&
		    (probe_rsp->edcaParams.qosInfo.count !=
		     session_entry->gLimEdcaParamSetCount)) {
			status = sch_beacon_edca_process(mac_ctx,
						    &probe_rsp->edcaParams,
						    session_entry);
			if (QDF_IS_STATUS_ERROR(status)) {
				pe_err("EDCA param process error");
			} else if (sta_ds) {
				qdf_mem_copy(&sta_ds->qos.peer_edca_params,
					     &probe_rsp->edcaParams,
					     sizeof(probe_rsp->edcaParams));
				/*
				 * If needed, downgrade the
				 * EDCA parameters
				 */
				lim_set_active_edca_params(mac_ctx,
						session_entry->gLimEdcaParams,
						session_entry);
				lim_send_edca_params(mac_ctx,
					session_entry->gLimEdcaParamsActive,
					session_entry->vdev_id, false);
				sch_qos_concurrency_update();
			} else {
				pe_err("SelfEntry missing in Hash");
			}
		}
		if (session_entry->fWaitForProbeRsp) {
			pe_warn("Check probe resp for caps change");
			lim_detect_change_in_ap_capabilities(mac_ctx, probe_rsp,
							     session_entry,
							     false);
		}
	}
}

void lim_process_gen_probe_rsp_frame(struct mac_context *mac_ctx,
				     struct pe_session *session_entry,
				     uint8_t *bcn_probe, uint32_t len)
{
	tSirProbeRespBeacon *probe_rsp;
	struct wlan_frame_hdr *header;
	QDF_STATUS status;

	if (!bcn_probe || !len) {
		pe_err("bcn_probe is null or invalid len %d", len);
		return;
	}

	if (!session_entry) {
		pe_err("session_entry is NULL");
		return;
	}

	probe_rsp = qdf_mem_malloc(sizeof(tSirProbeRespBeacon));
	if (!probe_rsp) {
		pe_err("Unable to allocate memory");
		return;
	}

	header = (struct wlan_frame_hdr *)(bcn_probe);
	pe_debug("Generate Probe Resp for cu (len %d): " QDF_MAC_ADDR_FMT,
		 len, QDF_MAC_ADDR_REF(header->i_addr3));

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   bcn_probe, len);

	bcn_probe = (uint8_t *)(bcn_probe + sizeof(*header));
	len -= sizeof(*header);

	status = sir_convert_probe_frame2_struct(mac_ctx,
						 bcn_probe, len, probe_rsp);
	if (QDF_IS_STATUS_ERROR(status) || !probe_rsp->ssidPresent) {
		pe_err("Parse error ProbeResponse, length=%d", len);
		qdf_mem_free(probe_rsp);
		return;
	}

	lim_process_updated_ies_in_probe_rsp(mac_ctx, session_entry, probe_rsp);
	qdf_mem_free(probe_rsp);
}

#ifdef WLAN_FEATURE_11BE_MLO
static
void lim_update_mlo_mgr_prb_info(struct mac_context *mac_ctx,
				 struct pe_session *session_entry,
				 struct qdf_mac_addr *mac_addr,
				 tpSirProbeRespBeacon probe_rsp)
{
	if (!(session_entry->lim_join_req &&
	      session_entry->lim_join_req->is_ml_probe_req_sent &&
	      probe_rsp->mlo_ie.mlo_ie_present))
		return;

	lim_update_mlo_mgr_info(mac_ctx, session_entry->vdev, mac_addr,
				session_entry->lim_join_req->assoc_link_id,
				probe_rsp->chan_freq);
}
#else
static inline
void lim_update_mlo_mgr_prb_info(struct mac_context *mac_ctx,
				 struct pe_session *session_entry,
				 struct qdf_mac_addr *mac_addr,
				 tpSirProbeRespBeacon probe_rsp)
{
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO
static bool
lim_validate_probe_rsp_mld_addr(struct pe_session *session,
				tpSirProbeRespBeacon probe_rsp)
{
	QDF_STATUS status;
	struct wlan_mlo_ie *mlo_ie;
	struct qdf_mac_addr curr_bss_mld;
	struct qdf_mac_addr *probe_rsp_mld;

	/* If ML-IE is not present or if the VDEV is not MLO return success */
	if (!probe_rsp->mlo_ie.mlo_ie_present ||
	    !wlan_vdev_mlme_is_mlo_vdev(session->vdev))
		return true;

	status = wlan_vdev_get_bss_peer_mld_mac(session->vdev, &curr_bss_mld);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to fetch MLD address for ML VDEV");
		return false;
	}

	mlo_ie = &probe_rsp->mlo_ie.mlo_ie;
	probe_rsp_mld =	(struct qdf_mac_addr *)mlo_ie->mld_mac_addr;
	if (qdf_is_macaddr_zero(probe_rsp_mld) ||
	    !qdf_is_macaddr_equal(probe_rsp_mld, &curr_bss_mld)) {
		pe_err("prb rsp MLD " QDF_MAC_ADDR_FMT ", bss peer MLD " QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(probe_rsp_mld->bytes),
		       QDF_MAC_ADDR_REF(curr_bss_mld.bytes));
		return false;
	}

	return true;
}
#else
static inline bool
lim_validate_probe_rsp_mld_addr(struct pe_session *session,
				tpSirProbeRespBeacon probe_rsp)
{
	return true;
}
#endif
/**
 * lim_process_probe_rsp_frame() - processes received Probe Response frame
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_Packet_info: A pointer to Buffer descriptor + associated PDUs
 * @session_entry: Handle to the session.
 *
 * This function processes received Probe Response frame.
 * Frames with out-of-order IEs are dropped.
 * In case of IBSS, join 'success' makes MLM state machine
 * transition into 'BSS started' state. This may have to change
 * depending on supporting what kinda Authentication in IBSS.
 *
 * Return: None
 */
void
lim_process_probe_rsp_frame(struct mac_context *mac_ctx, uint8_t *rx_Packet_info,
			    struct pe_session *session_entry)
{
	uint8_t *body;
	uint32_t frame_len = 0;
	tSirMacAddr current_bssid;
	tpSirMacMgmtHdr header;
	tSirProbeRespBeacon *probe_rsp;
	uint32_t chan_freq = 0;
	uint8_t bpcc;
	bool cu_flag = true;
	QDF_STATUS status;

	if (!session_entry) {
		pe_err("session_entry is NULL");
		return;
	}

	probe_rsp = qdf_mem_malloc(sizeof(tSirProbeRespBeacon));
	if (!probe_rsp) {
		pe_err("Unable to allocate memory");
		return;
	}

	probe_rsp->ssId.length = 0;
	probe_rsp->wpa.length = 0;

	header = WMA_GET_RX_MAC_HEADER(rx_Packet_info);

	mac_ctx->lim.bss_rssi = (int8_t)
				WMA_GET_RX_RSSI_NORMALIZED(rx_Packet_info);

	/* Validate IE information before processing Probe Response Frame */
	if (lim_validate_ie_information_in_probe_rsp_frame(mac_ctx,
				rx_Packet_info) !=
		QDF_STATUS_SUCCESS) {
		pe_err("Parse error ProbeResponse, length=%d", frame_len);
		goto mem_free;
	}

	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_Packet_info);
	/* Get pointer to Probe Response frame body */
	body = WMA_GET_RX_MPDU_DATA(rx_Packet_info);
		/* Enforce Mandatory IEs */
	if ((sir_convert_probe_frame2_struct(mac_ctx,
		body, frame_len, probe_rsp) == QDF_STATUS_E_FAILURE) ||
		!probe_rsp->ssidPresent) {
		pe_err("Parse error ProbeResponse, length=%d", frame_len);
		goto mem_free;
	}

	if (!lim_validate_probe_rsp_mld_addr(session_entry, probe_rsp))
		goto mem_free;

	qdf_trace_hex_dump(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG, body,
			   frame_len);

	lim_update_mlo_mgr_prb_info(mac_ctx, session_entry,
				    (struct qdf_mac_addr *)header->bssId,
				    probe_rsp);

	lim_process_bcn_prb_rsp_t2lm(mac_ctx, session_entry, probe_rsp);
	lim_gen_link_specific_probe_rsp(mac_ctx, session_entry,
					probe_rsp,
					body,
					frame_len,
					mac_ctx->lim.bss_rssi);

	if (mlo_is_mld_sta(session_entry->vdev)) {
		cu_flag = false;
		status = lim_get_bpcc_from_mlo_ie(probe_rsp, &bpcc);
		if (QDF_IS_STATUS_SUCCESS(status))
			cu_flag = lim_check_cu_happens(session_entry->vdev,
						       bpcc);
		lim_process_cu_for_probe_rsp(mac_ctx, session_entry,
					     body, frame_len);
	}

	if (session_entry->limMlmState ==
			eLIM_MLM_WT_JOIN_BEACON_STATE) {
		/*
		 * Either Beacon/probe response is required.
		 * Hence store it in same buffer.
		 */
		if (session_entry->beacon) {
			qdf_mem_free(session_entry->beacon);
			session_entry->beacon = NULL;
			session_entry->bcnLen = 0;
		}
		session_entry->bcnLen =
			WMA_GET_RX_MPDU_LEN(rx_Packet_info);
		session_entry->beacon =
			qdf_mem_malloc(session_entry->bcnLen);
		if (!session_entry->beacon) {
			pe_err("No Memory to store beacon");
		} else {
			/*
			 * Store the whole ProbeRsp frame.
			 * This is sent to csr/hdd in join cnf response.
			 */
			qdf_mem_copy(session_entry->beacon,
				     WMA_GET_RX_MAC_HEADER
					     (rx_Packet_info),
				     session_entry->bcnLen);
		}
			/* STA in WT_JOIN_BEACON_STATE */
		lim_check_and_announce_join_success(mac_ctx, probe_rsp,
						header,
						session_entry);

	} else if (session_entry->limMlmState ==
		   eLIM_MLM_LINK_ESTABLISHED_STATE) {
		/*
		 * Check if this Probe Response is for
		 * our Probe Request sent upon reaching
		 * heart beat threshold
		 */
		sir_copy_mac_addr(current_bssid, session_entry->bssId);
		if (qdf_mem_cmp(current_bssid, header->bssId,
				sizeof(tSirMacAddr))) {
			goto mem_free;
		}
		if (!LIM_IS_CONNECTION_ACTIVE(session_entry)) {
			pe_warn("Recved Probe Resp from AP,AP-alive");
			if (probe_rsp->HTInfo.present) {
				chan_freq =
				    wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
								 probe_rsp->HTInfo.primaryChannel);
				lim_received_hb_handler(mac_ctx, chan_freq,
							session_entry);
			} else
				lim_received_hb_handler(mac_ctx,
							probe_rsp->chan_freq,
							session_entry);
		}

		if (!cu_flag)
			goto mem_free;

		lim_process_updated_ies_in_probe_rsp(mac_ctx, session_entry,
						     probe_rsp);
	}

mem_free:
	qdf_mem_free(probe_rsp);

	/* Ignore Probe Response frame in all other states */
	return;
}
