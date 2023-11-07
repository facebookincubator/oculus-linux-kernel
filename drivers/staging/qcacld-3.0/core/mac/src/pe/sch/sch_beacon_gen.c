/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * This file sch_beacon_gen.cc contains beacon generation related
 * functions
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "cds_api.h"
#include "wni_cfg.h"
#include "ani_global.h"
#include "sir_mac_prot_def.h"

#include "lim_utils.h"
#include "lim_api.h"

#include "wma_if.h"
#include "sch_api.h"

#include "parser_api.h"
#include "wlan_utility.h"
#include "lim_mlo.h"

/* Offset of Channel Switch count field in CSA/ECSA IE */
#define SCH_CSA_SWITCH_COUNT_OFFSET 2
#define SCH_ECSA_SWITCH_COUNT_OFFSET 3

const uint8_t p2p_oui[] = { 0x50, 0x6F, 0x9A, 0x9 };

/**
 * sch_get_csa_ecsa_count_offset() - get the offset of Switch count field
 * @ie: pointer to the beginning of IEs in the beacon frame buffer
 * @ie_len: length of the IEs in the buffer
 * @csa_count_offset: pointer to the csa_count_offset variable in the caller
 * @ecsa_count_offset: pointer to the ecsa_count_offset variable in the caller
 *
 * Gets the offset of the switch count field in the CSA/ECSA IEs from the start
 * of the IEs buffer.
 *
 * Return: None
 */
static void sch_get_csa_ecsa_count_offset(const uint8_t *ie, uint32_t ie_len,
					  uint32_t *csa_count_offset,
					  uint32_t *ecsa_count_offset)
{
	const uint8_t *ptr = ie;
	uint8_t elem_id;
	uint16_t elem_len;
	uint32_t offset = 0;

	/* IE is not present */
	if (!ie_len)
		return;

	while (ie_len >= 2) {
		elem_id = ptr[0];
		elem_len = ptr[1];
		ie_len -= 2;
		offset += 2;

		if (elem_id == DOT11F_EID_CHANSWITCHANN &&
		    elem_len == 3)
			*csa_count_offset = offset +
					SCH_CSA_SWITCH_COUNT_OFFSET;

		if (elem_id == DOT11F_EID_EXT_CHAN_SWITCH_ANN &&
		    elem_len == 4)
			*ecsa_count_offset = offset +
					SCH_ECSA_SWITCH_COUNT_OFFSET;

		if (ie_len < elem_len)
			return;

		ie_len -= elem_len;
		offset += elem_len;
		ptr += (elem_len + 2);
	}
}

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * lim_update_link_info() - update mlo_link_info
 * @mac_ctx: mac context
 * @session: pe session
 * @bcn_1: pointer to tDot11fBeacon1
 * @bcn_2: pointer to tDot11fBeacon2
 *
 * Return: void
 */
static void lim_update_link_info(struct mac_context *mac_ctx,
				 struct pe_session *session,
				 tDot11fBeacon1 *bcn_1,
				 tDot11fBeacon2 *bcn_2)
{
	struct mlo_link_ie *link_ie = &session->mlo_link_info.link_ie;
	uint16_t offset;
	uint8_t *ptr;
	uint32_t n_bytes;

	session->mlo_link_info.upt_bcn_mlo_ie = false;
	session->mlo_link_info.bss_param_change = false;

	if (qdf_mem_cmp(&link_ie->link_ds, &bcn_1->DSParams,
			sizeof(bcn_1->DSParams))) {
		qdf_mem_copy(&link_ie->link_ds, &bcn_1->DSParams,
			     sizeof(bcn_1->DSParams));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d DSParams changed, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	qdf_mem_copy(&link_ie->link_wmm_params, &bcn_2->WMMParams,
		     sizeof(bcn_2->WMMParams));

	qdf_mem_copy(&link_ie->link_wmm_caps, &bcn_2->WMMCaps,
		     sizeof(bcn_2->WMMCaps));

	if (qdf_mem_cmp(&link_ie->link_edca, &bcn_2->EDCAParamSet,
			sizeof(bcn_2->EDCAParamSet))) {
		qdf_mem_copy(&link_ie->link_edca, &bcn_2->EDCAParamSet,
			     sizeof(bcn_2->EDCAParamSet));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d EDCAParamSet changed, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_csa, &bcn_2->ChanSwitchAnn,
			sizeof(bcn_2->ChanSwitchAnn))) {
		session->mlo_link_info.upt_bcn_mlo_ie = true;
		qdf_mem_copy(&link_ie->link_csa, &bcn_2->ChanSwitchAnn,
			     sizeof(bcn_2->ChanSwitchAnn));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d csa added, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_ecsa, &bcn_2->ext_chan_switch_ann,
			sizeof(bcn_2->ext_chan_switch_ann))) {
		session->mlo_link_info.upt_bcn_mlo_ie = true;
		qdf_mem_copy(&link_ie->link_ecsa, &bcn_2->ext_chan_switch_ann,
			     sizeof(bcn_2->ext_chan_switch_ann));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d ecsa added, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_swt_time, &bcn_2->max_chan_switch_time,
			sizeof(bcn_2->max_chan_switch_time))) {
		session->mlo_link_info.upt_bcn_mlo_ie = true;
		qdf_mem_copy(&link_ie->link_swt_time,
			     &bcn_2->max_chan_switch_time,
			     sizeof(bcn_2->max_chan_switch_time));
		pe_debug("vdev id %d max channel switch time added",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_quiet, &bcn_2->Quiet,
			sizeof(bcn_2->Quiet))) {
		session->mlo_link_info.upt_bcn_mlo_ie = true;
		qdf_mem_copy(&link_ie->link_quiet, &bcn_2->Quiet,
			     sizeof(bcn_2->Quiet));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d quiet added, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_ht_info, &bcn_2->HTInfo,
			sizeof(bcn_2->HTInfo))) {
		qdf_mem_copy(&link_ie->link_ht_info, &bcn_2->HTInfo,
			     sizeof(bcn_2->HTInfo));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d HTInfo changed, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_vht_op, &bcn_2->VHTOperation,
			sizeof(bcn_2->VHTOperation))) {
		qdf_mem_copy(&link_ie->link_vht_op, &bcn_2->VHTOperation,
			     sizeof(bcn_2->VHTOperation));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d VHTOperation changed, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_he_op, &bcn_2->he_op,
			sizeof(bcn_2->he_op))) {
		qdf_mem_copy(&link_ie->link_he_op, &bcn_2->he_op,
			     sizeof(bcn_2->he_op));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d he_op changed, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	if (qdf_mem_cmp(&link_ie->link_eht_op, &bcn_2->eht_op,
			sizeof(bcn_2->eht_op))) {
		qdf_mem_copy(&link_ie->link_eht_op, &bcn_2->eht_op,
			     sizeof(bcn_2->eht_op));
		session->mlo_link_info.bss_param_change = true;
		pe_debug("vdev id %d eht_op changed, critical update",
			 wlan_vdev_get_id(session->vdev));
	}

	/*
	 * MLOTD
	 * If max channel switch time is not exist, calculate one for partner
	 * link, if current link enters CAC
	 */

	if (session->mlo_link_info.bcn_tmpl_exist) {
		if (bcn_2->ChanSwitchAnn.present ||
		    bcn_2->ext_chan_switch_ann.present ||
		    bcn_2->Quiet.present ||
		    bcn_2->WiderBWChanSwitchAnn.present ||
		    bcn_2->ChannelSwitchWrapper.present ||
		    bcn_2->OperatingMode.present ||
		    bcn_2->bss_color_change.present)
			session->mlo_link_info.bss_param_change = true;
		if (session->mlo_link_info.bss_param_change) {
			link_ie->bss_param_change_cnt++;
			offset = sizeof(tAniBeaconStruct);
			bcn_1->Capabilities.criticalUpdateFlag = 1;
			ptr = session->pSchBeaconFrameBegin + offset;
			dot11f_pack_beacon1(mac_ctx, bcn_1, ptr,
					    SIR_MAX_BEACON_SIZE - offset,
					    &n_bytes);
			bcn_1->Capabilities.criticalUpdateFlag = 0;
			mlme_set_notify_co_located_ap_update_rnr(session->vdev,
								 true);
		}
	} else {
		//save one time
		session->mlo_link_info.bcn_tmpl_exist = true;
		session->mlo_link_info.link_ie.bss_param_change_cnt = 0;
		qdf_mem_copy(&link_ie->link_cap, &bcn_1->Capabilities,
			     sizeof(bcn_1->Capabilities));
		qdf_mem_copy(&link_ie->link_qcn_ie, &bcn_2->qcn_ie,
			     sizeof(bcn_2->qcn_ie));
		qdf_mem_copy(&link_ie->link_ht_cap, &bcn_2->HTCaps,
			     sizeof(bcn_2->HTCaps));
		qdf_mem_copy(&link_ie->link_ext_cap, &bcn_2->ExtCap,
			     sizeof(bcn_2->ExtCap));
		qdf_mem_copy(&link_ie->link_vht_cap, &bcn_2->VHTCaps,
			     sizeof(bcn_2->VHTCaps));
		qdf_mem_copy(&link_ie->link_he_cap, &bcn_2->he_cap,
			     sizeof(bcn_2->he_cap));
		qdf_mem_copy(&link_ie->link_he_6ghz_band_cap,
			     &bcn_2->he_6ghz_band_cap,
			     sizeof(bcn_2->he_6ghz_band_cap));
		qdf_mem_copy(&link_ie->link_eht_cap, &bcn_2->eht_cap,
			     sizeof(bcn_2->eht_cap));
	}
}

static void lim_upt_mlo_partner_info(struct mac_context *mac,
				     struct pe_session *session,
				     uint8_t *ie, uint32_t ie_len,
				     uint16_t ie_offset)
{
	const uint8_t *mlo_ie;
	uint16_t subie_len;
	const uint8_t *subie_sta_prof;
	uint16_t subie_sta_prof_len;
	int link;
	struct ml_sch_partner_info *sch_info;
	uint16_t per_sta_ofst = mac->sch.sch_mlo_partner.mlo_ie_link_info_ofst;

	mlo_ie = wlan_get_ext_ie_ptr_from_ext_id(MLO_IE_OUI_TYPE,
						 MLO_IE_OUI_SIZE,
						 ie, ie_len);
	/* IE is not present */
	if (!mlo_ie) {
		pe_err("no mlo ie in mlo ap vdev id %d", session->vdev_id);
		return;
	}
	for (link = 0; link < mac->sch.sch_mlo_partner.num_links; link++) {
		sch_info = &mac->sch.sch_mlo_partner.partner_info[link];
		if (!sch_info->link_info_sta_prof_ofst)
			continue;
		if (!sch_info->csa_ext_csa_exist) {
			per_sta_ofst += 1; /* subelement ID */
			subie_len = mlo_ie[per_sta_ofst];
			per_sta_ofst += 1; /* length */
			per_sta_ofst += subie_len; /* payload of per sta info */
			continue;
		}
		subie_sta_prof = mlo_ie + per_sta_ofst +
					sch_info->link_info_sta_prof_ofst;
		per_sta_ofst += 1; /* subelement ID */
		subie_len = mlo_ie[per_sta_ofst];
		per_sta_ofst += 1; /* length */
		per_sta_ofst += subie_len; /* payload of per sta info */
		subie_sta_prof_len = subie_len + 2 -
					sch_info->link_info_sta_prof_ofst;
		sch_get_csa_ecsa_count_offset(subie_sta_prof,
					      subie_sta_prof_len,
					      &sch_info->bcn_csa_cnt_ofst,
					      &sch_info->bcn_ext_csa_cnt_ofst);
		/* plus offset from IE of sta prof to ie */
		if (sch_info->bcn_csa_cnt_ofst)
			sch_info->bcn_csa_cnt_ofst += ie_offset +
						subie_sta_prof - ie;
		if (sch_info->bcn_ext_csa_cnt_ofst)
			sch_info->bcn_ext_csa_cnt_ofst += ie_offset +
						subie_sta_prof - ie;
		pe_debug("vdev %d mlo csa_count_offset %d ecsa_count_offset %d",
			 sch_info->vdev_id, sch_info->bcn_csa_cnt_ofst,
			 sch_info->bcn_ext_csa_cnt_ofst);
	}
}
#else
static void lim_update_link_info(struct mac_context *mac_ctx,
				 struct pe_session *session,
				 tDot11fBeacon1 *bcn_1,
				 tDot11fBeacon2 *bcn_2)
{
}

static void lim_upt_mlo_partner_info(struct mac_context *mac,
				     struct pe_session *session,
				     uint8_t *ie, uint32_t ie_len,
				     uint16_t ie_offset)
{
}
#endif

static QDF_STATUS sch_get_p2p_ie_offset(uint8_t *pextra_ie,
					uint32_t extra_ie_len,
					uint16_t *pie_offset)
{
	uint8_t elem_id;
	uint8_t elem_len;
	uint8_t *ie_ptr = pextra_ie;
	uint8_t oui_size = sizeof(p2p_oui);
	uint32_t p2p_ie_offset = 0;
	uint32_t left_len = extra_ie_len;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	*pie_offset = 0;
	while (left_len > 2) {
		elem_id  = ie_ptr[0];
		elem_len = ie_ptr[1];
		left_len -= 2;

		if (elem_len > left_len)
			return status;

		if ((elem_id == 0xDD) && (elem_len >= oui_size)) {
			if (!qdf_mem_cmp(&ie_ptr[2], &p2p_oui, oui_size)) {
				*pie_offset = p2p_ie_offset;
				return QDF_STATUS_SUCCESS;
			}
		}

		left_len -= elem_len;
		ie_ptr += (elem_len + 2);
		p2p_ie_offset += (elem_len + 2);
	};

	return status;
}

/**
 * sch_append_addn_ie() - adds additional IEs to frame
 * @mac_ctx:       mac global context
 * @session:       pe session pointer
 * @frm:           frame where additional IE is to be added
 * @bcn_size_left: beacon size left
 * @num_bytes:     final size
 * @addn_ie:       pointer to additional IE
 * @addn_ielen:    length of additional IE
 *
 * Return: status of operation
 */
static QDF_STATUS
sch_append_addn_ie(struct mac_context *mac_ctx, struct pe_session *session,
		   uint8_t *frm, uint32_t bcn_size_left,
		   uint32_t *num_bytes, uint8_t *addn_ie, uint16_t addn_ielen)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t add_ie[WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN];
	uint8_t *p2p_ie = NULL;
	uint8_t noa_len = 0;
	uint8_t noa_strm[SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN];
	uint8_t ext_p2p_ie[DOT11F_IE_P2PBEACON_MAX_LEN + 2];
	bool valid_ie;

	valid_ie = (addn_ielen <= WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN &&
		    addn_ielen && (addn_ielen <= bcn_size_left));
	if (!valid_ie) {
		pe_err("addn_ielen %d exceed left %d",
		       addn_ielen, bcn_size_left);
		return status;
	}

	qdf_mem_zero(&ext_p2p_ie[0], DOT11F_IE_P2PBEACON_MAX_LEN + 2);
	/*
	 * P2P IE extracted in wlan_hdd_add_hostapd_conf_vsie may not
	 * be at the end of additional IE buffer. The buffer sent to WMA
	 * expect P2P IE at the end of beacon buffer and will result in
	 * beacon corruption if P2P IE is not at end of beacon buffer.
	 */
	status = lim_strip_ie(mac_ctx, addn_ie, &addn_ielen, WLAN_ELEMID_VENDOR,
			      ONE_BYTE, SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE,
			      ext_p2p_ie, DOT11F_IE_P2PBEACON_MAX_LEN);

	qdf_mem_copy(&add_ie[0], addn_ie, addn_ielen);

	if (status == QDF_STATUS_SUCCESS &&
	    ext_p2p_ie[0] == WLAN_ELEMID_VENDOR &&
	    !qdf_mem_cmp(&ext_p2p_ie[2], SIR_MAC_P2P_OUI,
			 SIR_MAC_P2P_OUI_SIZE)) {
		qdf_mem_copy(&add_ie[addn_ielen], ext_p2p_ie,
			     ext_p2p_ie[1] + 2);
		addn_ielen += ext_p2p_ie[1] + 2;
	}

	p2p_ie = (uint8_t *)limGetP2pIEPtr(mac_ctx, &add_ie[0], addn_ielen);
	if ((p2p_ie) && !mac_ctx->beacon_offload) {
		/* get NoA attribute stream P2P IE */
		noa_len = lim_get_noa_attr_stream(mac_ctx, noa_strm, session);
		if (noa_len) {
			if ((noa_len + addn_ielen) <=
			    WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN) {
				qdf_mem_copy(&add_ie[addn_ielen], noa_strm,
					     noa_len);
				addn_ielen += noa_len;
				p2p_ie[1] += noa_len;
			} else {
				pe_err("Not able to insert NoA because of length constraint");
			}
		}
	}
	if (addn_ielen <= WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN) {
		qdf_mem_copy(frm, &add_ie[0], addn_ielen);
		*num_bytes = *num_bytes + addn_ielen;
	} else {
		pe_warn("Not able to insert because of len constraint %d",
			addn_ielen);
	}
	return status;
}

static void
populate_channel_switch_ann(struct mac_context *mac_ctx,
			    tDot11fBeacon2 *bcn,
			    struct pe_session *pe_session)
{
	populate_dot11f_chan_switch_ann(mac_ctx, &bcn->ChanSwitchAnn,
					pe_session);
	pe_debug("csa: mode:%d chan:%d count:%d",
		 bcn->ChanSwitchAnn.switchMode,
		 bcn->ChanSwitchAnn.newChannel,
		 bcn->ChanSwitchAnn.switchCount);

	if (!pe_session->dfsIncludeChanWrapperIe)
		return;

	populate_dot11f_chan_switch_wrapper(mac_ctx,
					    &bcn->ChannelSwitchWrapper,
					    pe_session);
	pe_debug("wrapper: width:%d f0:%d f1:%d",
		 bcn->ChannelSwitchWrapper.WiderBWChanSwitchAnn.newChanWidth,
		 bcn->ChannelSwitchWrapper.WiderBWChanSwitchAnn.newCenterChanFreq0,
		 bcn->ChannelSwitchWrapper.WiderBWChanSwitchAnn.newCenterChanFreq1);
}

/**
 * sch_get_tim_size() - Get TIM ie size
 * @max_aid: Max AID value
 *
 * Return: TIM ie size
 */
static uint16_t sch_get_tim_size(uint32_t max_aid)
{
	uint16_t tim_size;
	uint8_t N2;

	/**
	 * The TIM ie format:
	 * +----------+------+----------+-------------------------------------------------+
	 * |Element ID|Length|DTIM Count|DTIM Period|Bitmap Control|Partial Virtual Bitmap|
	 * +----------+------+----------+-----------+--------------+----------------------+
	 *   1 Byte    1 Byte  1Byte       1 Byte        1 Byte          0~255 Byte
	 *
	 * According to 80211 Spec, The Partial Virtual Bitmap field consists of octets
	 * numbered N1 to N2 of the traffic indication virtual bitmap, where N1 is the
	 * largest even number such that bits numbered 1 to (N1 * 8) – 1 in the traffic
	 * indication virtual bitmap are all 0, and N2 is the smallest number such that
	 * bits numbered (N2 + 1) * 8 to 2007 in the traffic indication virtual bitmap
	 * are all 0. In this case, the Bitmap Offset subfield value contains the number
	 * N1/2, and the Length field is set to (N2 – N1) + 4. Always start with AID 1 as
	 * minimum, N1 = (1 / 8) = 0. TIM size = length + 1Byte Element ID + 1Byte Length.
	 * The expression is reduced to (N2 - 0) + 4 + 2 = N2 + 6;
	 */
	N2 = max_aid / 8;
	tim_size = N2 + 6;

	return tim_size;
}

/**
 * sch_set_fixed_beacon_fields() - sets the fixed params in beacon frame
 * @mac_ctx:       mac global context
 * @session:       pe session entry
 * @band:          out param, band caclculated
 * @opr_ch:        operating channels
 *
 * Return: status of operation
 */

QDF_STATUS
sch_set_fixed_beacon_fields(struct mac_context *mac_ctx, struct pe_session *session)
{
	tpAniBeaconStruct bcn_struct = (tpAniBeaconStruct)
						session->pSchBeaconFrameBegin;
	tpSirMacMgmtHdr mac;
	uint16_t offset, bcn_size_left;
	uint8_t *ptr;
	tDot11fBeacon1 *bcn_1;
	tDot11fBeacon2 *bcn_2;
	uint32_t i, n_status, n_bytes;
	bool wps_ap_enable = 0;
	tDot11fIEWscProbeRes *wsc_prb_res;
	uint8_t *extra_ie = NULL;
	uint32_t extra_ie_len = 0;
	uint16_t extra_ie_offset = 0;
	uint16_t p2p_ie_offset = 0;
	uint32_t csa_count_offset = 0;
	uint32_t ecsa_count_offset = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool is_vht_enabled = false;
	uint16_t addn_ielen = 0;
	uint8_t *addn_ie = NULL;
	tDot11fIEExtCap extracted_extcap;
	bool extcap_present = true, addnie_present = false;
	bool is_6ghz_chsw;
	uint8_t *eht_op_ie = NULL, eht_op_ie_len = 0;
	uint8_t *eht_cap_ie = NULL, eht_cap_ie_len = 0;
	bool is_band_2g;
	uint16_t ie_buf_size;
	uint16_t mlo_ie_len = 0;
	uint16_t tim_size;

	tim_size = sch_get_tim_size(HAL_NUM_STA);

	bcn_1 = qdf_mem_malloc(sizeof(tDot11fBeacon1));
	if (!bcn_1)
		return QDF_STATUS_E_NOMEM;

	bcn_2 = qdf_mem_malloc(sizeof(tDot11fBeacon2));
	if (!bcn_2) {
		qdf_mem_free(bcn_1);
		return QDF_STATUS_E_NOMEM;
	}

	wsc_prb_res = qdf_mem_malloc(sizeof(tDot11fIEWscProbeRes));
	if (!wsc_prb_res) {
		qdf_mem_free(bcn_1);
		qdf_mem_free(bcn_2);
		return QDF_STATUS_E_NOMEM;
	}
	/*
	 * First set the fixed fields:
	 * set the TFP headers, set the mac header
	 */
	qdf_mem_zero((uint8_t *) &bcn_struct->macHdr, sizeof(tSirMacMgmtHdr));
	mac = (tpSirMacMgmtHdr) &bcn_struct->macHdr;
	mac->fc.type = SIR_MAC_MGMT_FRAME;
	mac->fc.subType = SIR_MAC_MGMT_BEACON;

	for (i = 0; i < 6; i++)
		mac->da[i] = 0xff;

	qdf_mem_copy(mac->sa, session->self_mac_addr,
		     sizeof(session->self_mac_addr));
	qdf_mem_copy(mac->bssId, session->bssId, sizeof(session->bssId));

	mac->fc.fromDS = 0;
	mac->fc.toDS = 0;

	/* Skip over the timestamp (it'll be updated later). */
	bcn_1->BeaconInterval.interval =
		session->beaconParams.beaconInterval;
	populate_dot11f_capabilities(mac_ctx, &bcn_1->Capabilities, session);
	if (session->ssidHidden) {
		bcn_1->SSID.present = 1;
		/* rest of the fields are 0 for hidden ssid */
		if ((session->ssId.length) &&
		    (session->ssidHidden == eHIDDEN_SSID_ZERO_CONTENTS))
			bcn_1->SSID.num_ssid = session->ssId.length;
	} else {
		populate_dot11f_ssid(mac_ctx, &session->ssId, &bcn_1->SSID);
	}

	populate_dot11f_supp_rates(mac_ctx, POPULATE_DOT11F_RATES_OPERATIONAL,
				   &bcn_1->SuppRates, session);
	populate_dot11f_ds_params(mac_ctx, &bcn_1->DSParams,
				  session->curr_op_freq);

	offset = sizeof(tAniBeaconStruct);
	ptr = session->pSchBeaconFrameBegin + offset;

	if (LIM_IS_AP_ROLE(session)) {
		/* Initialize the default IE bitmap to zero */
		qdf_mem_zero((uint8_t *) &(session->DefProbeRspIeBitmap),
			    (sizeof(uint32_t) * 8));

		/* Initialize the default IE bitmap to zero */
		qdf_mem_zero((uint8_t *) &(session->probeRespFrame),
			    sizeof(session->probeRespFrame));

		/*
		 * Can be efficiently updated whenever new IE added in Probe
		 * response in future
		 */
		if (lim_update_probe_rsp_template_ie_bitmap_beacon1(mac_ctx,
					bcn_1, session) != QDF_STATUS_SUCCESS)
			pe_err("Failed to build ProbeRsp template");
	}

	n_status = dot11f_pack_beacon1(mac_ctx, bcn_1, ptr,
				       SIR_MAX_BEACON_SIZE - offset, &n_bytes);
	if (DOT11F_FAILED(n_status)) {
		pe_err("Failed to packed a tDot11fBeacon1 (0x%08x)",
			n_status);
		qdf_mem_free(bcn_1);
		qdf_mem_free(bcn_2);
		qdf_mem_free(wsc_prb_res);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(n_status)) {
		pe_warn("Warnings while packing a tDot11fBeacon1(0x%08x)",
			n_status);
	}
	session->schBeaconOffsetBegin = offset + (uint16_t) n_bytes;
	/* Initialize the 'new' fields at the end of the beacon */
	is_6ghz_chsw =
		WLAN_REG_IS_6GHZ_CHAN_FREQ(session->curr_op_freq) ||
		WLAN_REG_IS_6GHZ_CHAN_FREQ
			(session->gLimChannelSwitch.sw_target_freq);
	if (session->limSystemRole == eLIM_AP_ROLE &&
	    session->dfsIncludeChanSwIe == true) {
		if (!CHAN_HOP_ALL_BANDS_ENABLE ||
		    session->lim_non_ecsa_cap_num == 0 || is_6ghz_chsw) {
			tDot11fIEext_chan_switch_ann *ext_csa =
						&bcn_2->ext_chan_switch_ann;
			populate_dot_11_f_ext_chann_switch_ann(mac_ctx,
							       ext_csa,
							       session);
			pe_debug("ecsa: mode:%d reg:%d chan:%d count:%d",
				 ext_csa->switch_mode,
				 ext_csa->new_reg_class,
				 ext_csa->new_channel,
				 ext_csa->switch_count);
		}

		if (session->lim_non_ecsa_cap_num &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(session->curr_op_freq) &&
		    !is_6ghz_chsw)
			populate_channel_switch_ann(mac_ctx, bcn_2, session);
	}

	populate_dot11_supp_operating_classes(mac_ctx,
		&bcn_2->SuppOperatingClasses, session);
	populate_dot11f_country(mac_ctx, &bcn_2->Country, session);
	if (bcn_1->Capabilities.qos)
		populate_dot11f_edca_param_set(mac_ctx, &bcn_2->EDCAParamSet,
					       session);

	if (session->lim11hEnable) {
		populate_dot11f_power_constraints(mac_ctx,
						  &bcn_2->PowerConstraints);
		populate_dot11f_tpc_report(mac_ctx, &bcn_2->TPCReport, session);
		/* Need to insert channel switch announcement here */
		if ((LIM_IS_AP_ROLE(session) ||
		     LIM_IS_P2P_DEVICE_GO(session)) &&
		    session->dfsIncludeChanSwIe && !is_6ghz_chsw) {
			populate_channel_switch_ann(mac_ctx, bcn_2, session);
		}
	}

	if (bcn_2->ext_chan_switch_ann.present || bcn_2->ChanSwitchAnn.present)
		populate_dot11f_max_chan_switch_time(
			mac_ctx, &bcn_2->max_chan_switch_time, session);

	if (mac_ctx->rrm.rrmConfig.sap_rrm_enabled)
		populate_dot11f_rrm_ie(mac_ctx, &bcn_2->RRMEnabledCap,
			session);

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	/* populate proprietary IE for MDM device operating in AP-MCC */
	populate_dot11f_avoid_channel_ie(mac_ctx, &bcn_2->QComVendorIE,
					 session);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

	if (session->dot11mode != MLME_DOT11_MODE_11B)
		populate_dot11f_erp_info(mac_ctx, &bcn_2->ERPInfo, session);

	if (session->htCapability) {
		populate_dot11f_ht_caps(mac_ctx, session, &bcn_2->HTCaps);
		populate_dot11f_ht_info(mac_ctx, &bcn_2->HTInfo, session);
	}
	if (session->vhtCapability) {
		populate_dot11f_vht_caps(mac_ctx, session, &bcn_2->VHTCaps);
		populate_dot11f_vht_operation(mac_ctx, session,
					      &bcn_2->VHTOperation);
		is_vht_enabled = true;
		/* following is for MU MIMO: we do not support it yet */
		/*
		populate_dot11f_vht_ext_bss_load( mac_ctx, &bcn2.VHTExtBssLoad);
		*/
		populate_dot11f_tx_power_env(mac_ctx,
					     &bcn_2->transmit_power_env[0],
					     session->ch_width,
					     session->curr_op_freq,
					     &bcn_2->num_transmit_power_env,
					     false);
		populate_dot11f_qcn_ie(mac_ctx, session, &bcn_2->qcn_ie,
				       QCN_IE_ATTR_ID_ALL);
	}

	if (wlan_reg_is_6ghz_chan_freq(session->curr_op_freq)) {
		populate_dot11f_tx_power_env(mac_ctx,
					     &bcn_2->transmit_power_env[0],
					     session->ch_width,
					     session->curr_op_freq,
					     &bcn_2->num_transmit_power_env,
					     false);
		populate_dot11f_qcn_ie(mac_ctx, session, &bcn_2->qcn_ie,
				       QCN_IE_ATTR_ID_ALL);
	}

	if (lim_is_session_he_capable(session)) {
		pe_debug("Populate HE IEs");
		populate_dot11f_he_caps(mac_ctx, session,
					&bcn_2->he_cap);
		populate_dot11f_he_operation(mac_ctx, session,
					&bcn_2->he_op);
		populate_dot11f_sr_info(mac_ctx, session,
					&bcn_2->spatial_reuse);
		populate_dot11f_he_6ghz_cap(mac_ctx, session,
					    &bcn_2->he_6ghz_band_cap);
		populate_dot11f_he_bss_color_change(mac_ctx, session,
					&bcn_2->bss_color_change);
	}

	if (lim_is_session_eht_capable(session)) {
		pe_debug("Populate EHT IEs");
		populate_dot11f_eht_caps(mac_ctx, session, &bcn_2->eht_cap);
		populate_dot11f_eht_operation(mac_ctx, session, &bcn_2->eht_op);
	}

	populate_dot11f_ext_cap(mac_ctx, is_vht_enabled, &bcn_2->ExtCap,
				session);

	populate_dot11f_ext_supp_rates(mac_ctx,
				POPULATE_DOT11F_RATES_OPERATIONAL,
				&bcn_2->ExtSuppRates, session);

	if (session->pLimStartBssReq) {
		populate_dot11f_wpa(mac_ctx, &session->pLimStartBssReq->rsnIE,
				    &bcn_2->WPA);
		populate_dot11f_rsn_opaque(mac_ctx,
					   &session->pLimStartBssReq->rsnIE,
					   &bcn_2->RSNOpaque);
		populate_dot11f_wapi(mac_ctx, &session->pLimStartBssReq->rsnIE,
				     &bcn_2->WAPI);
	}

	if (session->limWmeEnabled)
		populate_dot11f_wmm(mac_ctx, &bcn_2->WMMInfoAp,
				&bcn_2->WMMParams, &bcn_2->WMMCaps, session);

	if (LIM_IS_AP_ROLE(session)) {
		if (session->wps_state != SAP_WPS_DISABLED) {
			populate_dot11f_beacon_wpsi_es(mac_ctx,
						&bcn_2->WscBeacon, session);
		}
	} else {
		wps_ap_enable = mac_ctx->mlme_cfg->wps_params.enable_wps &
					    WNI_CFG_WPS_ENABLE_AP;
		if (wps_ap_enable)
			populate_dot11f_wsc(mac_ctx, &bcn_2->WscBeacon);

		if (mac_ctx->lim.wscIeInfo.wscEnrollmentState ==
						eLIM_WSC_ENROLL_BEGIN) {
			populate_dot11f_wsc_registrar_info(mac_ctx,
						&bcn_2->WscBeacon);
			mac_ctx->lim.wscIeInfo.wscEnrollmentState =
						eLIM_WSC_ENROLL_IN_PROGRESS;
		}

		if (mac_ctx->lim.wscIeInfo.wscEnrollmentState ==
						eLIM_WSC_ENROLL_END) {
			de_populate_dot11f_wsc_registrar_info(mac_ctx,
							&bcn_2->WscBeacon);
			mac_ctx->lim.wscIeInfo.wscEnrollmentState =
							eLIM_WSC_ENROLL_NOOP;
		}
	}

	if (LIM_IS_AP_ROLE(session)) {
		if (wlan_vdev_mlme_is_mlo_ap(session->vdev)) {
			lim_update_link_info(mac_ctx, session, bcn_1, bcn_2);
			mlo_ie_len = lim_send_bcn_frame_mlo(mac_ctx, session);
			populate_dot11f_mlo_rnr(
				mac_ctx, session,
				&bcn_2->reduced_neighbor_report);
		} else if (!wlan_reg_is_6ghz_chan_freq(session->curr_op_freq)) {
			/*
			 * TD: If current AP is MLO, RNR IE is already populated
			 *     More effor to populate RNR IE for
			 *     MLO SAP + 6G legacy SAP
			 */
			populate_dot11f_6g_rnr(mac_ctx, session,
					       &bcn_2->reduced_neighbor_report);
		}
		/*
		 * Can be efficiently updated whenever new IE added  in Probe
		 * response in future
		 */
		lim_update_probe_rsp_template_ie_bitmap_beacon2(mac_ctx, bcn_2,
					&session->DefProbeRspIeBitmap[0],
					&session->probeRespFrame);

		/* update probe response WPS IE instead of beacon WPS IE */
		if (session->wps_state != SAP_WPS_DISABLED) {
			if (session->APWPSIEs.SirWPSProbeRspIE.FieldPresent)
				populate_dot11f_probe_res_wpsi_es(mac_ctx,
							wsc_prb_res, session);
			else
				wsc_prb_res->present = 0;
			if (wsc_prb_res->present) {
				set_probe_rsp_ie_bitmap(
					&session->DefProbeRspIeBitmap[0],
					SIR_MAC_WPA_EID);
				qdf_mem_copy((void *)
					&session->probeRespFrame.WscProbeRes,
					(void *)wsc_prb_res,
					sizeof(tDot11fIEWscProbeRes));
			}
		}
	}

	addnie_present = (session->add_ie_params.probeRespBCNDataLen != 0);
	if (addnie_present) {
		/*
		 * Strip HE cap/op from additional IE buffer if any, as they
		 * should be populated already.
		 */
		lim_strip_he_ies_from_add_ies(mac_ctx, session);
		lim_strip_eht_ies_from_add_ies(mac_ctx, session);

		addn_ielen = session->add_ie_params.probeRespBCNDataLen;
		addn_ie = qdf_mem_malloc(addn_ielen);
		if (!addn_ie) {
			qdf_mem_free(bcn_1);
			qdf_mem_free(bcn_2);
			qdf_mem_free(wsc_prb_res);
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_copy(addn_ie,
			session->add_ie_params.probeRespBCNData_buff,
			addn_ielen);

		qdf_mem_zero((uint8_t *)&extracted_extcap,
			     sizeof(tDot11fIEExtCap));
		status = lim_strip_extcap_update_struct(mac_ctx, addn_ie,
				&addn_ielen, &extracted_extcap);
		if (QDF_STATUS_SUCCESS != status) {
			extcap_present = false;
			pe_debug("extcap not extracted");
		}
		/* merge extcap IE */
		if (extcap_present) {
			lim_merge_extcap_struct(&bcn_2->ExtCap,
						&extracted_extcap,
						true);
			populate_dot11f_bcn_prot_extcaps(mac_ctx, session,
							 &bcn_2->ExtCap);
		}
	}

	if (session->vhtCapability && session->gLimOperatingMode.present) {
		populate_dot11f_operating_mode(mac_ctx, &bcn_2->OperatingMode,
					       session);
		lim_strip_ie(mac_ctx, addn_ie, &addn_ielen,
			     WLAN_ELEMID_OP_MODE_NOTIFY, ONE_BYTE, NULL, 0,
			     NULL, SIR_MAC_VHT_OPMODE_SIZE - 2);
	}

	n_status = dot11f_pack_beacon2(mac_ctx, bcn_2,
				       session->pSchBeaconFrameEnd,
				       SIR_MAX_BEACON_SIZE, &n_bytes);
	if (DOT11F_FAILED(n_status)) {
		pe_err("Failed to packed a tDot11fBeacon2 (0x%08x)",
			n_status);
		status = QDF_STATUS_E_FAILURE;
		goto free_and_exit;
	} else if (DOT11F_WARNED(n_status)) {
		pe_err("Warnings while packing a tDot11fBeacon2(0x%08x)",
			n_status);
	}

	/* Strip EHT capabilities IE */
	if (lim_is_session_eht_capable(session)) {
		ie_buf_size = n_bytes;

		eht_op_ie = qdf_mem_malloc(WLAN_MAX_IE_LEN + 2);
		if (!eht_op_ie) {
			pe_err("malloc failed for eht_op_ie");
			status = QDF_STATUS_E_FAILURE;
			goto free_and_exit;
		}

		status = lim_strip_eht_op_ie(mac_ctx,
					     session->pSchBeaconFrameEnd,
					     &ie_buf_size, eht_op_ie);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("Failed to strip EHT op IE");
			qdf_mem_free(eht_op_ie);
			status = QDF_STATUS_E_FAILURE;
			goto free_and_exit;
		}

		lim_ieee80211_pack_ehtop(eht_op_ie, bcn_2->eht_op,
					 bcn_2->VHTOperation,
					 bcn_2->he_op,
					 bcn_2->HTInfo);
		eht_op_ie_len = eht_op_ie[1] + 2;

		/* Copy the EHT operation IE to the end of the frame */
		qdf_mem_copy(session->pSchBeaconFrameEnd + ie_buf_size,
			     eht_op_ie, eht_op_ie_len);
		qdf_mem_free(eht_op_ie);
		n_bytes = ie_buf_size + eht_op_ie_len;

		ie_buf_size = n_bytes;
		eht_cap_ie = qdf_mem_malloc(WLAN_MAX_IE_LEN + 2);
		if (!eht_cap_ie) {
			pe_err("malloc failed for eht_cap_ie");
			status = QDF_STATUS_E_FAILURE;
			goto free_and_exit;
		}
		status = lim_strip_eht_cap_ie(mac_ctx,
					      session->pSchBeaconFrameEnd,
					      &ie_buf_size, eht_cap_ie);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("Failed to strip EHT cap IE");
			qdf_mem_free(eht_cap_ie);
			status = QDF_STATUS_E_FAILURE;
			goto free_and_exit;
		}

		is_band_2g =
			WLAN_REG_IS_24GHZ_CH_FREQ(session->curr_op_freq);

		lim_ieee80211_pack_ehtcap(eht_cap_ie, bcn_2->eht_cap,
					  bcn_2->he_cap, is_band_2g);
		eht_cap_ie_len = eht_cap_ie[1] + 2;

		/* Copy the EHT cap IE to the end of the frame */
		qdf_mem_copy(session->pSchBeaconFrameEnd + ie_buf_size,
			     eht_cap_ie, eht_cap_ie_len);

		qdf_mem_free(eht_cap_ie);
		n_bytes = ie_buf_size + eht_cap_ie_len;
	}

	if (mlo_ie_len) {
		status = lim_fill_complete_mlo_ie(session, mlo_ie_len,
					 session->pSchBeaconFrameEnd + n_bytes);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_debug("assemble ml ie error");
			mlo_ie_len = 0;
		}
		n_bytes += mlo_ie_len;
	}

	/* Fill the CSA/ECSA count offsets if the IEs are present */
	mac_ctx->sch.ecsa_count_offset = 0;
	mac_ctx->sch.csa_count_offset = 0;
	if (session->dfsIncludeChanSwIe)
		sch_get_csa_ecsa_count_offset(session->pSchBeaconFrameEnd,
					      n_bytes,
					      &csa_count_offset,
					      &ecsa_count_offset);

	if (csa_count_offset)
		mac_ctx->sch.csa_count_offset =
				session->schBeaconOffsetBegin + tim_size +
				csa_count_offset;
	if (ecsa_count_offset)
		mac_ctx->sch.ecsa_count_offset =
				session->schBeaconOffsetBegin + tim_size +
				ecsa_count_offset;

	if (wlan_vdev_mlme_is_mlo_ap(session->vdev))
		lim_upt_mlo_partner_info(mac_ctx, session,
					 session->pSchBeaconFrameEnd, n_bytes,
					 session->schBeaconOffsetBegin +
					 tim_size);

	extra_ie = session->pSchBeaconFrameEnd + n_bytes;
	extra_ie_offset = n_bytes;

	/*
	 * Max size left to append additional IE.= (MAX beacon size - TIM IE -
	 * beacon fix size (bcn_1 + header) - beacon variable size (bcn_1).
	 */
	bcn_size_left = SIR_MAX_BEACON_SIZE - tim_size -
				session->schBeaconOffsetBegin -
				(uint16_t)n_bytes;

	/* TODO: Append additional IE here. */
	if (addn_ielen > 0)
		sch_append_addn_ie(mac_ctx, session,
				   session->pSchBeaconFrameEnd + n_bytes,
				   bcn_size_left, &n_bytes,
				   addn_ie, addn_ielen);

	session->schBeaconOffsetEnd = (uint16_t) n_bytes;
	extra_ie_len = n_bytes - extra_ie_offset;
	/* Get the p2p Ie Offset */
	status = sch_get_p2p_ie_offset(extra_ie, extra_ie_len, &p2p_ie_offset);
	if (QDF_STATUS_SUCCESS == status)
		/* Update the P2P Ie Offset */
		mac_ctx->sch.p2p_ie_offset =
			session->schBeaconOffsetBegin + tim_size +
			extra_ie_offset + p2p_ie_offset;
	else
		mac_ctx->sch.p2p_ie_offset = 0;

	pe_debug("vdev %d: beacon begin offset %d fixed size %d csa_count_offset %d ecsa_count_offset %d max_bcn_size_left %d addn_ielen %d beacon end offset %d",
		 session->vdev_id, offset, session->schBeaconOffsetBegin,
		 mac_ctx->sch.csa_count_offset, mac_ctx->sch.ecsa_count_offset,
		 bcn_size_left, addn_ielen, session->schBeaconOffsetEnd);
	mac_ctx->sch.beacon_changed = 1;
	status = QDF_STATUS_SUCCESS;

free_and_exit:
	qdf_mem_free(bcn_1);
	qdf_mem_free(bcn_2);
	qdf_mem_free(wsc_prb_res);
	qdf_mem_free(addn_ie);
	return status;
}

QDF_STATUS
lim_update_probe_rsp_template_ie_bitmap_beacon1(struct mac_context *mac,
						tDot11fBeacon1 *beacon1,
						struct pe_session *pe_session)
{
	uint32_t *DefProbeRspIeBitmap;
	tDot11fProbeResponse *prb_rsp;

	if (!pe_session) {
		pe_debug("PESession is null!");
		return QDF_STATUS_E_FAILURE;
	}
	DefProbeRspIeBitmap = &pe_session->DefProbeRspIeBitmap[0];
	prb_rsp = &pe_session->probeRespFrame;
	prb_rsp->BeaconInterval = beacon1->BeaconInterval;
	qdf_mem_copy((void *)&prb_rsp->Capabilities,
		     (void *)&beacon1->Capabilities,
		     sizeof(beacon1->Capabilities));

	/* SSID */
	if (beacon1->SSID.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, WLAN_ELEMID_SSID);
		/* populating it, because probe response has to go with SSID even in hidden case */
		populate_dot11f_ssid(mac, &pe_session->ssId, &prb_rsp->SSID);
	}
	/* supported rates */
	if (beacon1->SuppRates.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, WLAN_ELEMID_RATES);
		qdf_mem_copy((void *)&prb_rsp->SuppRates,
			     (void *)&beacon1->SuppRates,
			     sizeof(beacon1->SuppRates));

	}
	/* DS Parameter set */
	if (beacon1->DSParams.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_DSPARMS);
		qdf_mem_copy((void *)&prb_rsp->DSParams,
			     (void *)&beacon1->DSParams,
			     sizeof(beacon1->DSParams));

	}

	return QDF_STATUS_SUCCESS;
}

void lim_update_probe_rsp_template_ie_bitmap_beacon2(struct mac_context *mac,
						     tDot11fBeacon2 *beacon2,
						     uint32_t *DefProbeRspIeBitmap,
						     tDot11fProbeResponse *prb_rsp)
{
	uint8_t i;
	uint16_t num_tpe = beacon2->num_transmit_power_env;

	if (beacon2->Country.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, WLAN_ELEMID_COUNTRY);
		qdf_mem_copy((void *)&prb_rsp->Country,
			     (void *)&beacon2->Country,
			     sizeof(beacon2->Country));

	}
	/* Power constraint */
	if (beacon2->PowerConstraints.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_PWRCNSTR);
		qdf_mem_copy((void *)&prb_rsp->PowerConstraints,
			     (void *)&beacon2->PowerConstraints,
			     sizeof(beacon2->PowerConstraints));

	}
	/* Channel Switch Annoouncement WLAN_ELEMID_CHANSWITCHANN */
	if (beacon2->ChanSwitchAnn.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_CHANSWITCHANN);
		qdf_mem_copy((void *)&prb_rsp->ChanSwitchAnn,
			     (void *)&beacon2->ChanSwitchAnn,
			     sizeof(beacon2->ChanSwitchAnn));

	}

	/* EXT Channel Switch Announcement CHNL_EXTENDED_SWITCH_ANN_EID*/
	if (beacon2->ext_chan_switch_ann.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
			WLAN_ELEMID_EXTCHANSWITCHANN);
		qdf_mem_copy((void *)&prb_rsp->ext_chan_switch_ann,
			(void *)&beacon2->ext_chan_switch_ann,
			sizeof(beacon2->ext_chan_switch_ann));
	}

	/* Supported operating class */
	if (beacon2->SuppOperatingClasses.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_SUPP_OP_CLASS);
		qdf_mem_copy((void *)&prb_rsp->SuppOperatingClasses,
				(void *)&beacon2->SuppOperatingClasses,
				sizeof(beacon2->SuppOperatingClasses));
	}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	if (beacon2->QComVendorIE.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					SIR_MAC_QCOM_VENDOR_EID);
		qdf_mem_copy((void *)&prb_rsp->QComVendorIE,
			     (void *)&beacon2->QComVendorIE,
			     sizeof(beacon2->QComVendorIE));
	}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

	/* ERP information */
	if (beacon2->ERPInfo.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, WLAN_ELEMID_ERP);
		qdf_mem_copy((void *)&prb_rsp->ERPInfo,
			     (void *)&beacon2->ERPInfo,
			     sizeof(beacon2->ERPInfo));

	}
	/* Extended supported rates */
	if (beacon2->ExtSuppRates.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_XRATES);
		qdf_mem_copy((void *)&prb_rsp->ExtSuppRates,
			     (void *)&beacon2->ExtSuppRates,
			     sizeof(beacon2->ExtSuppRates));

	}

	/* WPA */
	if (beacon2->WPA.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, SIR_MAC_WPA_EID);
		qdf_mem_copy((void *)&prb_rsp->WPA, (void *)&beacon2->WPA,
			     sizeof(beacon2->WPA));

	}

	/* RSN */
	if (beacon2->RSNOpaque.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, WLAN_ELEMID_RSN);
		qdf_mem_copy((void *)&prb_rsp->RSNOpaque,
			     (void *)&beacon2->RSNOpaque,
			     sizeof(beacon2->RSNOpaque));
	}

	/* WAPI */
	if (beacon2->WAPI.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, WLAN_ELEMID_WAPI);
		qdf_mem_copy((void *)&prb_rsp->WAPI,
			     (void *)&beacon2->WAPI,
			     sizeof(beacon2->WAPI));
	}

	/* EDCA Parameter set */
	if (beacon2->EDCAParamSet.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_EDCAPARMS);
		qdf_mem_copy((void *)&prb_rsp->EDCAParamSet,
			     (void *)&beacon2->EDCAParamSet,
			     sizeof(beacon2->EDCAParamSet));

	}
	/* Vendor specific - currently no vendor specific IEs added */
	/* Requested IEs - currently we are not processing this will be added later */
	/* HT capability IE */
	if (beacon2->HTCaps.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_HTCAP_ANA);
		qdf_mem_copy((void *)&prb_rsp->HTCaps, (void *)&beacon2->HTCaps,
			     sizeof(beacon2->HTCaps));
	}
	/* HT Info IE */
	if (beacon2->HTInfo.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, WLAN_ELEMID_HTINFO_ANA);
		qdf_mem_copy((void *)&prb_rsp->HTInfo, (void *)&beacon2->HTInfo,
			     sizeof(beacon2->HTInfo));
	}
	if (beacon2->VHTCaps.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_VHTCAP);
		qdf_mem_copy((void *)&prb_rsp->VHTCaps,
			     (void *)&beacon2->VHTCaps,
			     sizeof(beacon2->VHTCaps));
	}
	if (beacon2->VHTOperation.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_VHTOP);
		qdf_mem_copy((void *)&prb_rsp->VHTOperation,
			     (void *)&beacon2->VHTOperation,
			     sizeof(beacon2->VHTOperation));
	}

	for (i = 0; i < num_tpe; i++) {
		if (beacon2->transmit_power_env[i].present) {
			set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
						WLAN_ELEMID_VHT_TX_PWR_ENVLP);
			qdf_mem_copy((void *)&prb_rsp->transmit_power_env[i],
				     (void *)&beacon2->transmit_power_env[i],
				     sizeof(beacon2->transmit_power_env[i]));
		}
	}
	prb_rsp->num_transmit_power_env = num_tpe;

	if (beacon2->VHTExtBssLoad.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_EXT_BSS_LOAD);
		qdf_mem_copy((void *)&prb_rsp->VHTExtBssLoad,
			     (void *)&beacon2->VHTExtBssLoad,
			     sizeof(beacon2->VHTExtBssLoad));
	}
	/* WMM IE */
	if (beacon2->WMMParams.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, SIR_MAC_WPA_EID);
		qdf_mem_copy((void *)&prb_rsp->WMMParams,
			     (void *)&beacon2->WMMParams,
			     sizeof(beacon2->WMMParams));
	}
	/* WMM capability - most of the case won't be present */
	if (beacon2->WMMCaps.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, SIR_MAC_WPA_EID);
		qdf_mem_copy((void *)&prb_rsp->WMMCaps,
			     (void *)&beacon2->WMMCaps,
			     sizeof(beacon2->WMMCaps));
	}

	/* QCN IE - only for ll sap */
	if (beacon2->qcn_ie.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_VENDOR);
		qdf_mem_copy((void *)&prb_rsp->qcn_ie,
			     (void *)&beacon2->qcn_ie,
			     sizeof(beacon2->qcn_ie));
	}

	/* Extended Capability */
	if (beacon2->ExtCap.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap, DOT11F_EID_EXTCAP);
		qdf_mem_copy((void *)&prb_rsp->ExtCap,
			     (void *)&beacon2->ExtCap,
			     sizeof(beacon2->ExtCap));
	}

	if (beacon2->he_cap.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_HE_CAP);
		qdf_mem_copy((void *)&prb_rsp->he_cap,
			     (void *)&beacon2->he_cap,
			     sizeof(beacon2->he_cap));
	}
	if (beacon2->he_op.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_HE_OP);
		qdf_mem_copy((void *)&prb_rsp->he_op,
			     (void *)&beacon2->he_op,
			     sizeof(beacon2->he_op));
	}

	if (beacon2->spatial_reuse.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_SPATIAL_REUSE);
		qdf_mem_copy((void *)&prb_rsp->spatial_reuse,
			     (void *)&beacon2->spatial_reuse,
			     sizeof(beacon2->spatial_reuse));
	}

	if (beacon2->he_6ghz_band_cap.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_HE_6GHZ_BAND_CAP);
		qdf_mem_copy((void *)&prb_rsp->he_6ghz_band_cap,
			     (void *)&beacon2->he_6ghz_band_cap,
			     sizeof(beacon2->he_6ghz_band_cap));
	}

	if (beacon2->eht_cap.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_EHT_CAP);
		qdf_mem_copy((void *)&prb_rsp->eht_cap,
			     (void *)&beacon2->eht_cap,
			     sizeof(beacon2->eht_cap));
	}

	if (beacon2->eht_op.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_EHT_OP);
		qdf_mem_copy((void *)&prb_rsp->eht_op,
			     (void *)&beacon2->eht_op,
			     sizeof(beacon2->eht_op));
	}

	if (beacon2->mlo_ie.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_MLO_IE);
		qdf_mem_copy((void *)&prb_rsp->mlo_ie,
			     (void *)&beacon2->mlo_ie,
			     sizeof(beacon2->mlo_ie));
	}

	if (beacon2->reduced_neighbor_report.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					DOT11F_EID_REDUCED_NEIGHBOR_REPORT);
		qdf_mem_copy((void *)&prb_rsp->reduced_neighbor_report,
			     (void *)&beacon2->reduced_neighbor_report,
			     sizeof(beacon2->reduced_neighbor_report));
	}

	if (beacon2->TPCReport.present) {
		set_probe_rsp_ie_bitmap(DefProbeRspIeBitmap,
					WLAN_ELEMID_TPCREP);
		qdf_mem_copy((void *)&prb_rsp->TPCReport,
			     (void *)&beacon2->TPCReport,
			     sizeof(beacon2->TPCReport));
	}

}

void set_probe_rsp_ie_bitmap(uint32_t *IeBitmap, uint32_t pos)
{
	uint32_t index, temp;

	index = pos >> 5;
	if (index >= 8) {
		return;
	}
	temp = IeBitmap[index];

	temp |= 1 << (pos & 0x1F);

	IeBitmap[index] = temp;
}

/**
 * write_beacon_to_memory() - send the beacon to the wma
 * @mac: pointer to mac structure
 * @size: Size of the beacon to write to memory
 * @length: Length field of the beacon to write to memory
 * @pe_session: pe session
 * @reason: beacon update reason
 *
 * return: success: QDF_STATUS_SUCCESS failure: QDF_STATUS_E_FAILURE
 */
static QDF_STATUS write_beacon_to_memory(struct mac_context *mac, uint16_t size,
					 uint16_t length,
					 struct pe_session *pe_session,
					 enum sir_bcn_update_reason reason)
{
	uint16_t i;
	tpAniBeaconStruct pBeacon;
	QDF_STATUS status;

	/* copy end of beacon only if length > 0 */
	if (length > 0) {
		if (size + pe_session->schBeaconOffsetEnd >
		    SIR_MAX_BEACON_SIZE) {
			pe_err("beacon tmp fail size %d BeaconOffsetEnd %d",
			       size, pe_session->schBeaconOffsetEnd);
			return QDF_STATUS_E_FAILURE;
		}
		for (i = 0; i < pe_session->schBeaconOffsetEnd; i++)
			pe_session->pSchBeaconFrameBegin[size++] =
				pe_session->pSchBeaconFrameEnd[i];
	}
	/* Update the beacon length */
	pBeacon = (tpAniBeaconStruct) pe_session->pSchBeaconFrameBegin;
	/* Do not include the beaconLength indicator itself */
	if (length == 0) {
		pBeacon->beaconLength = 0;
		/* Dont copy entire beacon, Copy length field alone */
		size = 4;
	} else
		pBeacon->beaconLength = (uint32_t) size - sizeof(uint32_t);

	if (!mac->sch.beacon_changed)
		return QDF_STATUS_E_FAILURE;

	status = sch_send_beacon_req(mac, pe_session->pSchBeaconFrameBegin,
				     size, pe_session, reason);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("sch_send_beacon_req() returned an error %d, size %d",
		       status, size);
	mac->sch.beacon_changed = 0;

	return status;
}

/**
 * sch_generate_tim
 *
 * FUNCTION:
 * Generate TIM
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param mac pointer to global mac structure
 * @param **pPtr pointer to the buffer, where the TIM bit is to be written.
 * @param *timLength pointer to limLength, which needs to be returned.
 * @return None
 */
void sch_generate_tim(struct mac_context *mac, uint8_t **pPtr, uint16_t *timLength,
		      uint8_t dtimPeriod)
{
	uint8_t *ptr = *pPtr;
	uint32_t val = 0;
	uint32_t minAid = 1;    /* Always start with AID 1 as minimum */
	uint32_t maxAid = HAL_NUM_STA;
	/* Generate partial virtual bitmap */
	uint8_t N1 = minAid / 8;
	uint8_t N2 = maxAid / 8;

	if (N1 & 1)
		N1--;

	*timLength = N2 - N1 + 4;
	val = dtimPeriod;

	/*
	 * Write 0xFF to firmware's field to detect firmware's mal-function
	 * early. DTIM count and bitmap control usually cannot be 0xFF, so it
	 * is easy to know that firmware never updated DTIM count/bitmap control
	 * field after host driver downloaded beacon template if end-user complaints
	 * that DTIM count and bitmapControl is 0xFF.
	 */
	*ptr++ = WLAN_ELEMID_TIM;
	*ptr++ = (uint8_t) (*timLength);
	/* location for dtimCount. will be filled in by FW. */
	*ptr++ = 0xFF;
	*ptr++ = (uint8_t) val;
	/* location for bitmap control. will be filled in by FW. */
	*ptr++ = 0xFF;
	ptr += (N2 - N1 + 1);

	*pPtr = ptr;
}

QDF_STATUS sch_process_pre_beacon_ind(struct mac_context *mac,
				      struct scheduler_msg *limMsg,
				      enum sir_bcn_update_reason reason)
{
	struct beacon_gen_params *pMsg = limMsg->bodyptr;
	uint32_t beaconSize;
	struct pe_session *pe_session;
	uint8_t sessionId;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	pe_session = pe_find_session_by_bssid(mac, pMsg->bssid, &sessionId);
	if (!pe_session) {
		pe_err("session lookup fails");
		goto end;
	}

	beaconSize = pe_session->schBeaconOffsetBegin;

	/* If SME is not in normal mode, no need to generate beacon */
	if (pe_session->limSmeState != eLIM_SME_NORMAL_STATE) {
		pe_debug("PreBeaconInd received in invalid state: %d",
			 pe_session->limSmeState);
		goto end;
	}

	switch (GET_LIM_SYSTEM_ROLE(pe_session)) {
	case eLIM_AP_ROLE: {
		uint8_t *ptr =
			&pe_session->pSchBeaconFrameBegin[pe_session->
							     schBeaconOffsetBegin];
		uint16_t timLength = 0;

		if (pe_session->statypeForBss == STA_ENTRY_SELF) {
			sch_generate_tim(mac, &ptr, &timLength,
					 pe_session->dtimPeriod);
			beaconSize += 2 + timLength;
			status =
			    write_beacon_to_memory(mac, (uint16_t) beaconSize,
						   (uint16_t) beaconSize,
						   pe_session, reason);
		} else
			pe_err("can not send beacon for PEER session entry");
			}
			break;

	default:
		pe_err("Error-PE has Receive PreBeconGenIndication when System is in %d role",
		       GET_LIM_SYSTEM_ROLE(pe_session));
	}

end:
	qdf_mem_free(pMsg);

	return status;
}
