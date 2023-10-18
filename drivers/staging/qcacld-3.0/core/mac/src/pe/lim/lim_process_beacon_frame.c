/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * This file lim_process_beacon_frame.cc contains the code
 * for processing Received Beacon Frame.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wni_cfg.h"
#include "ani_global.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_ser_des_utils.h"
#include "wlan_mlo_t2lm.h"
#include "wlan_mlo_mgr_roam.h"
#include "lim_mlo.h"
#include "wlan_mlo_mgr_sta.h"
#ifdef WLAN_FEATURE_11BE_MLO
#include <cds_ieee80211_common.h>
#endif

#ifdef WLAN_FEATURE_11BE_MLO
void lim_process_bcn_prb_rsp_t2lm(struct mac_context *mac_ctx,
				  struct pe_session *session,
				  tpSirProbeRespBeacon bcn_ptr)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_t2lm_context *t2lm_ctx;

	if (!session || !bcn_ptr || !mac_ctx) {
		pe_err("invalid input parameters");
		return;
	}

	vdev = session->vdev;
	if (!vdev || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return;

	if (!mlo_check_if_all_links_up(vdev))
		return;

	if (bcn_ptr->t2lm_ctx.upcoming_t2lm.t2lm.direction ==
	    WLAN_T2LM_INVALID_DIRECTION &&
	    bcn_ptr->t2lm_ctx.established_t2lm.t2lm.direction ==
	    WLAN_T2LM_INVALID_DIRECTION) {
		pe_debug("No t2lm IE");
		return;
	}

	t2lm_ctx = &vdev->mlo_dev_ctx->t2lm_ctx;
	qdf_mem_copy((uint8_t *)&t2lm_ctx->tsf, (uint8_t *)bcn_ptr->timeStamp,
		     sizeof(uint64_t));
	wlan_process_bcn_prbrsp_t2lm_ie(vdev, &bcn_ptr->t2lm_ctx,
					t2lm_ctx->tsf);
}

void lim_process_beacon_mlo(struct mac_context *mac_ctx,
			    struct pe_session *session,
			    tSchBeaconStruct *bcn_ptr)
{
	struct csa_offload_params csa_param;
	int i;
	uint8_t link_id;
	uint8_t *per_sta_pro;
	uint32_t per_sta_pro_len;
	uint8_t *sta_pro;
	uint32_t sta_pro_len;
	uint16_t stacontrol;
	struct ieee80211_channelswitch_ie *csa_ie;
	struct ieee80211_extendedchannelswitch_ie *xcsa_ie;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_mlo_dev_context *mlo_ctx;

	if (!session || !bcn_ptr || !mac_ctx) {
		pe_err("invalid input parameters");
		return;
	}
	vdev = session->vdev;
	if (!vdev || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		pe_err("null pdev");
		return;
	}
	mlo_ctx = vdev->mlo_dev_ctx;
	if (!mlo_ctx) {
		pe_err("null mlo_dev_ctx");
		return;
	}

	for (i = 0; i < bcn_ptr->mlo_ie.mlo_ie.num_sta_profile; i++) {
		csa_ie = NULL;
		xcsa_ie = NULL;
		qdf_mem_zero(&csa_param, sizeof(csa_param));
		per_sta_pro = bcn_ptr->mlo_ie.mlo_ie.sta_profile[i].data;
		per_sta_pro_len =
			bcn_ptr->mlo_ie.mlo_ie.sta_profile[i].num_data;
		stacontrol = *(uint16_t *)per_sta_pro;
		/* IE ID + LEN + STA control */
		sta_pro = per_sta_pro + MIN_IE_LEN + 2;
		sta_pro_len = per_sta_pro_len - MIN_IE_LEN - 2;
		link_id = QDF_GET_BITS(
			    stacontrol,
			    WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX,
			    WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS);

		if (!mlo_is_sta_csa_synced(mlo_ctx, link_id)) {
			csa_ie = (struct ieee80211_channelswitch_ie *)
					wlan_get_ie_ptr_from_eid(
						DOT11F_EID_CHANSWITCHANN,
						sta_pro, sta_pro_len);
			xcsa_ie = (struct ieee80211_extendedchannelswitch_ie *)
					wlan_get_ie_ptr_from_eid(
						DOT11F_EID_EXT_CHAN_SWITCH_ANN,
						sta_pro, sta_pro_len);
		}
		if (csa_ie) {
			csa_param.channel = csa_ie->newchannel;
			csa_param.csa_chan_freq = wlan_reg_legacy_chan_to_freq(
						pdev, csa_ie->newchannel);
			csa_param.switch_mode = csa_ie->switchmode;
			csa_param.ies_present_flag |= MLME_CSA_IE_PRESENT;
			mlo_sta_csa_save_params(mlo_ctx, link_id, &csa_param);
		} else if (xcsa_ie) {
			csa_param.channel = xcsa_ie->newchannel;
			csa_param.switch_mode = xcsa_ie->switchmode;
			csa_param.new_op_class = xcsa_ie->newClass;
			if (wlan_reg_is_6ghz_op_class(pdev, xcsa_ie->newClass))
				csa_param.csa_chan_freq =
					wlan_reg_chan_band_to_freq(
						pdev, xcsa_ie->newchannel,
						BIT(REG_BAND_6G));
			else
				csa_param.csa_chan_freq =
					wlan_reg_legacy_chan_to_freq(
						pdev, xcsa_ie->newchannel);
			csa_param.ies_present_flag |= MLME_XCSA_IE_PRESENT;
			mlo_sta_csa_save_params(mlo_ctx, link_id, &csa_param);
		}
	}
}
#endif

static QDF_STATUS
lim_validate_rsn_ie(const uint8_t *ie_ptr, uint16_t ie_len)
{
	QDF_STATUS status;
	const uint8_t *rsn_ie;
	struct wlan_crypto_params crypto_params;

	rsn_ie = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSN, ie_ptr, ie_len);
	if (!rsn_ie)
		return QDF_STATUS_SUCCESS;

	qdf_mem_zero(&crypto_params, sizeof(struct wlan_crypto_params));
	status = wlan_crypto_rsnie_check(&crypto_params, rsn_ie);
	if (status != QDF_STATUS_SUCCESS) {
		pe_debug_rl("RSN IE check failed %d", status);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE
/**
 * lim_get_update_eht_bw_puncture_allow() - whether bw and puncture can be
 *                                          sent to target directly
 * @session: pe session
 * @ori_bw: bandwdith from beacon
 * @new_bw: bandwidth intersection between reference AP and STA
 * @update_allow: return true if bw and puncture can be updated directly
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
lim_get_update_eht_bw_puncture_allow(struct pe_session *session,
				     enum phy_ch_width ori_bw,
				     enum phy_ch_width *new_bw,
				     bool *update_allow)
{
	enum phy_ch_width ch_width;
	struct wlan_objmgr_psoc *psoc;
	enum wlan_phymode phy_mode;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	*update_allow = false;

	psoc = wlan_vdev_get_psoc(session->vdev);
	if (!psoc) {
		pe_err("psoc object invalid");
		return QDF_STATUS_E_INVAL;
	}
	status = mlme_get_peer_phymode(psoc, session->bssId, &phy_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("failed to get phy_mode %d mac: " QDF_MAC_ADDR_FMT,
		       status, QDF_MAC_ADDR_REF(session->bssId));
		return QDF_STATUS_E_INVAL;
	}
	ch_width = wlan_mlme_get_ch_width_from_phymode(phy_mode);

	if (ori_bw <= ch_width) {
		*new_bw = ori_bw;
		*update_allow = true;
		return QDF_STATUS_SUCCESS;
	}

	if ((ori_bw == CH_WIDTH_320MHZ) &&
	    !session->eht_config.support_320mhz_6ghz) {
		if (ch_width == CH_WIDTH_160MHZ) {
			*new_bw = CH_WIDTH_160MHZ;
			*update_allow = true;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_SUCCESS;
}

void lim_process_beacon_eht_op(struct pe_session *session,
			       tDot11fIEeht_op *eht_op)
{
	uint16_t ori_punc = 0;
	enum phy_ch_width ori_bw = CH_WIDTH_INVALID;
	uint8_t cb_mode;
	enum phy_ch_width new_bw;
	bool update_allow;
	QDF_STATUS status;
	struct mac_context *mac_ctx;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_channel *des_chan;
	struct csa_offload_params *csa_param;

	if (!session || !eht_op || !session->mac_ctx || !session->vdev) {
		pe_err("invalid input parameters");
		return;
	}
	mac_ctx = session->mac_ctx;
	vdev = session->vdev;

	if (wlan_reg_is_24ghz_ch_freq(session->curr_op_freq)) {
		if (session->force_24ghz_in_ht20)
			cb_mode = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
		else
			cb_mode =
			   mac_ctx->roam.configParam.channelBondingMode24GHz;
	} else {
		cb_mode = mac_ctx->roam.configParam.channelBondingMode5GHz;
	}

	if (cb_mode == WNI_CFG_CHANNEL_BONDING_MODE_DISABLE) {
		/*
		 * if channel bonding is disabled from INI do not
		 * update the chan width
		 */
		pe_debug_rl("chan banding is disabled skip bw update");

		return;
	}
	/* handle beacon IE for 11be non-mlo case */
	if (eht_op->disabled_sub_chan_bitmap_present) {
		ori_punc = QDF_GET_BITS(
		    eht_op->disabled_sub_chan_bitmap[0][0], 0, 8);
		ori_punc |= QDF_GET_BITS(
		    eht_op->disabled_sub_chan_bitmap[0][1], 0, 8) << 8;
	}
	if (eht_op->eht_op_information_present) {
		ori_bw = wlan_mlme_convert_eht_op_bw_to_phy_ch_width(
						eht_op->channel_width);
		status = lim_get_update_eht_bw_puncture_allow(session, ori_bw,
							      &new_bw,
							      &update_allow);
		if (QDF_IS_STATUS_ERROR(status))
			return;
		if (update_allow) {
			wlan_cm_sta_update_bw_puncture(vdev, session->bssId,
						       ori_punc, ori_bw,
						       eht_op->ccfs0,
						       eht_op->ccfs1,
						       new_bw);
		} else {
			csa_param = qdf_mem_malloc(sizeof(*csa_param));
			if (!csa_param) {
				pe_err("csa_param allocation fails");
				return;
			}
			des_chan = wlan_vdev_mlme_get_des_chan(vdev);
			csa_param->channel = des_chan->ch_ieee;
			csa_param->csa_chan_freq = des_chan->ch_freq;
			csa_param->new_ch_width = ori_bw;
			csa_param->new_punct_bitmap = ori_punc;
			csa_param->new_ch_freq_seg1 = eht_op->ccfs0;
			csa_param->new_ch_freq_seg2 = eht_op->ccfs1;
			qdf_copy_macaddr(&csa_param->bssid,
					 (struct qdf_mac_addr *)session->bssId);
			lim_handle_sta_csa_param(session->mac_ctx, csa_param);
		}
	}
}

void lim_process_beacon_eht(struct mac_context *mac_ctx,
			    struct pe_session *session,
			    tSchBeaconStruct *bcn_ptr)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_channel *des_chan;

	if (!session || !bcn_ptr || !mac_ctx) {
		pe_err("invalid input parameters");
		return;
	}
	vdev = session->vdev;
	if (!vdev || wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE ||
	    !qdf_is_macaddr_equal((struct qdf_mac_addr *)session->bssId,
				  (struct qdf_mac_addr *)bcn_ptr->bssid))
		return;
	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	if (!des_chan || !IS_WLAN_PHYMODE_EHT(des_chan->ch_phymode))
		return;

	lim_process_beacon_eht_op(session, &bcn_ptr->eht_op);

	if (mlo_is_mld_sta(vdev))
		/* handle beacon IE for 802.11be mlo case */
		lim_process_beacon_mlo(mac_ctx, session, bcn_ptr);
}

void
lim_process_ml_reconfig(struct mac_context *mac_ctx,
			struct pe_session *session,
			uint8_t *rx_pkt_info)
{
	uint8_t *frame;
	uint16_t frame_len;

	if (!session->vdev)
		return;

	frame = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
	if (frame_len < SIR_MAC_B_PR_SSID_OFFSET)
		return;

	mlo_process_ml_reconfig_ie(session->vdev, NULL,
				   frame + SIR_MAC_B_PR_SSID_OFFSET,
				   frame_len - SIR_MAC_B_PR_SSID_OFFSET, NULL);
}
#endif

/**
 * lim_process_beacon_frame() - to process beacon frames
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_pkt_info: A pointer to RX packet info structure
 * @session: A pointer to session
 *
 * This function is called by limProcessMessageQueue() upon Beacon
 * frame reception.
 * Note:
 * 1. Beacons received in 'normal' state in IBSS are handled by
 *    Beacon Processing module.
 *
 * Return: none
 */

void
lim_process_beacon_frame(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
			 struct pe_session *session)
{
	tpSirMacMgmtHdr mac_hdr;
	tSchBeaconStruct *bcn_ptr;
	uint8_t *frame;
	const uint8_t *owe_transition_ie;
	uint16_t frame_len;
	uint8_t bpcc;
	bool cu_flag = true;
	QDF_STATUS status;

	mac_ctx->lim.gLimNumBeaconsRcvd++;

	/*
	 * here is it required to increment session specific heartBeat
	 * beacon counter
	 */
	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	frame = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	pe_debug("Beacon (len %d): " QDF_MAC_ADDR_FMT " RSSI %d",
		 WMA_GET_RX_MPDU_LEN(rx_pkt_info),
		 QDF_MAC_ADDR_REF(mac_hdr->sa),
		 (uint)abs((int8_t)
		 WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info)));
	if (frame_len < SIR_MAC_B_PR_SSID_OFFSET) {
		pe_debug_rl("payload invalid len %d", frame_len);
		return;
	}
	if (lim_validate_rsn_ie(frame + SIR_MAC_B_PR_SSID_OFFSET,
				frame_len - SIR_MAC_B_PR_SSID_OFFSET) !=
			QDF_STATUS_SUCCESS)
		return;
	/* Expect Beacon in any state as Scan is independent of LIM state */
	bcn_ptr = qdf_mem_malloc(sizeof(*bcn_ptr));
	if (!bcn_ptr)
		return;

	/* Parse received Beacon */
	if (sir_convert_beacon_frame2_struct(mac_ctx,
			rx_pkt_info, bcn_ptr) !=
			QDF_STATUS_SUCCESS) {
		/*
		 * Received wrongly formatted/invalid Beacon.
		 * Ignore it and move on.
		 */
		pe_warn("Received invalid Beacon in state: %X",
			session->limMlmState);
		lim_print_mlm_state(mac_ctx, LOGW,
			session->limMlmState);
		qdf_mem_free(bcn_ptr);
		return;
	}

	if (mlo_is_mld_sta(session->vdev)) {
		cu_flag = false;
		status = lim_get_bpcc_from_mlo_ie(bcn_ptr, &bpcc);
		if (QDF_IS_STATUS_SUCCESS(status))
			cu_flag = lim_check_cu_happens(session->vdev, bpcc);
		lim_process_ml_reconfig(mac_ctx, session, rx_pkt_info);
	}

	lim_process_bcn_prb_rsp_t2lm(mac_ctx, session, bcn_ptr);
	if (QDF_IS_STATUS_SUCCESS(lim_check_for_ml_probe_req(session)))
		goto end;

	/*
	 * during scanning, when any session is active, and
	 * beacon/Pr belongs to one of the session, fill up the
	 * following, TBD - HB counter
	 */
	if (sir_compare_mac_addr(session->bssId,
				bcn_ptr->bssid)) {
		qdf_mem_copy((uint8_t *)&session->lastBeaconTimeStamp,
			(uint8_t *) bcn_ptr->timeStamp,
			sizeof(uint64_t));
		session->currentBssBeaconCnt++;
	}
	MTRACE(mac_trace(mac_ctx,
		TRACE_CODE_RX_MGMT_TSF, 0, bcn_ptr->timeStamp[0]));
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_RX_MGMT_TSF, 0,
		bcn_ptr->timeStamp[1]));

	if (session->limMlmState ==
			eLIM_MLM_WT_JOIN_BEACON_STATE) {
		owe_transition_ie = wlan_get_vendor_ie_ptr_from_oui(
					OWE_TRANSITION_OUI_TYPE,
					OWE_TRANSITION_OUI_SIZE,
					frame + SIR_MAC_B_PR_SSID_OFFSET,
					frame_len - SIR_MAC_B_PR_SSID_OFFSET);
		if (session->connected_akm == ANI_AKM_TYPE_OWE &&
		    owe_transition_ie) {
			pe_debug("vdev:%d Drop OWE rx beacon. Wait for probe for join success",
				 session->vdev_id);
			qdf_mem_free(bcn_ptr);
			return;
		}

		if (session->beacon) {
			qdf_mem_free(session->beacon);
			session->beacon = NULL;
			session->bcnLen = 0;
		}

		mac_ctx->lim.bss_rssi =
			(int8_t)WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info);
		session->bcnLen = WMA_GET_RX_MPDU_LEN(rx_pkt_info);
		session->beacon = qdf_mem_malloc(session->bcnLen);
		if (session->beacon)
			/*
			 * Store the whole Beacon frame. This is sent to
			 * csr/hdd in join cnf response.
			 */
			qdf_mem_copy(session->beacon,
				WMA_GET_RX_MAC_HEADER(rx_pkt_info),
				session->bcnLen);

		lim_check_and_announce_join_success(mac_ctx, bcn_ptr,
				mac_hdr, session);
	}

	if (cu_flag)
		lim_process_beacon_eht(mac_ctx, session, bcn_ptr);
end:
	qdf_mem_free(bcn_ptr);
	return;
}
