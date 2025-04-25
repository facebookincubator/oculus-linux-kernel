/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DOC: contains EPCS (Emergency Preparedness Communications Service)
 * related functionality
 */
#include <wlan_cmn.h>
#include <wlan_cm_public_struct.h>
#include "wlan_epcs_api.h"
#include <wlan_mlo_epcs.h>
#include "wlan_cm_api.h"
#include "wlan_mlo_mgr_roam.h"
#include "wlan_cmn_ieee80211.h"
#include "dot11f.h"

#define EPCS_MIN_DIALOG_TOKEN         1
#define EPCS_MAX_DIALOG_TOKEN         0xFF

static struct ac_param_record default_epcs_edca[] = {
#ifndef ANI_LITTLE_BIT_ENDIAN
	/* The txop is multiple of 32us units */
	{0x07, 0x95, 79 /* 2.528ms */},
	{0x03, 0x95, 79 /* 2.528ms */},
	{0x02, 0x54, 128 /* 4.096ms */},
	{0x02, 0x43, 65 /* 2.080ms */}
#else
	{0x70, 0x59, 79 /* 2.528ms */},
	{0x30, 0x59, 79 /* 2.528ms */},
	{0x20, 0x45, 128 /* 4.096ms */},
	{0x20, 0x34, 65 /* 2.080ms */}
#endif
};

static
const char *epcs_get_event_str(enum wlan_epcs_evt event)
{
	if (event > WLAN_EPCS_EV_ACTION_FRAME_MAX)
		return "";

	switch (event) {
	CASE_RETURN_STRING(WLAN_EPCS_EV_ACTION_FRAME_RX_REQ);
	CASE_RETURN_STRING(WLAN_EPCS_EV_ACTION_FRAME_TX_RESP);
	CASE_RETURN_STRING(WLAN_EPCS_EV_ACTION_FRAME_TX_REQ);
	CASE_RETURN_STRING(WLAN_EPCS_EV_ACTION_FRAME_RX_RESP);
	CASE_RETURN_STRING(WLAN_EPCS_EV_ACTION_FRAME_RX_TEARDOWN);
	CASE_RETURN_STRING(WLAN_EPCS_EV_ACTION_FRAME_TX_TEARDOWN);
	default:
		return "Unknown";
	}
}

static uint8_t
epcs_gen_dialog_token(struct wlan_mlo_peer_epcs_info *epcs_info)
{
	if (!epcs_info)
		return 0;

	if (epcs_info->self_gen_dialog_token == EPCS_MAX_DIALOG_TOKEN)
		/* wrap is ok */
		epcs_info->self_gen_dialog_token = EPCS_MIN_DIALOG_TOKEN;
	else
		epcs_info->self_gen_dialog_token += 1;

	mlme_debug("gen dialog token %d", epcs_info->self_gen_dialog_token);
	return epcs_info->self_gen_dialog_token;
}

static void epcs_update_ac_value(tSirMacEdcaParamRecord *edca,
				 struct ac_param_record *epcs)
{
	edca->aci.rsvd = epcs->aci_aifsn >> RSVD_SHIFT_BIT & RSVD_MASK;
	edca->aci.aci = epcs->aci_aifsn >> ACI_SHIFT_BIT & ACI_MASK;
	edca->aci.acm = epcs->aci_aifsn >> ACM_SHIFT_BIT & ACM_MASK;
	edca->aci.aifsn = epcs->aci_aifsn >> AIFSN_SHIFT_BIT & AIFSN_MASK;

	edca->cw.max = epcs->ecw_min_max >> CWMAX_SHIFT_BIT & CWMAX_MASK;
	edca->cw.min = epcs->ecw_min_max >> CWMIN_SHIFT_BIT & CWMIN_MASK;

	edca->txoplimit = epcs->txop_limit;
	mlme_debug("edca rsvd %d, aci %d, acm %d, aifsn %d, cwmax %d, cwmin %d",
		   edca->aci.rsvd, edca->aci.aci, edca->aci.acm,
		   edca->aci.aifsn, edca->cw.max, edca->cw.min);
}

static void epcs_update_mu_ac_value(tSirMacEdcaParamRecord *edca,
				    struct muac_param_record *epcs)
{
	edca->aci.rsvd = epcs->aci_aifsn >> RSVD_SHIFT_BIT & RSVD_MASK;
	edca->aci.aci = epcs->aci_aifsn >> ACI_SHIFT_BIT & ACI_MASK;
	edca->aci.acm = epcs->aci_aifsn >> ACM_SHIFT_BIT & ACM_MASK;
	edca->aci.aifsn = epcs->aci_aifsn >> AIFSN_SHIFT_BIT & AIFSN_MASK;

	edca->cw.max = epcs->ecw_min_max >> CWMAX_SHIFT_BIT & CWMAX_MASK;
	edca->cw.min = epcs->ecw_min_max >> CWMIN_SHIFT_BIT & CWMIN_MASK;

	edca->txoplimit = epcs->mu_edca_timer;
	mlme_debug("muac rsvd %d, aci %d, acm %d, aifsn %d, cwmax %d, cwmin %d",
		   edca->aci.rsvd, edca->aci.aci, edca->aci.acm,
		   edca->aci.aifsn, edca->cw.max, edca->cw.min);
}

static QDF_STATUS
epcs_update_def_edca_param(struct wlan_objmgr_vdev *vdev)
{
	int i;
	struct mac_context *mac_ctx;
	tSirMacEdcaParamRecord edca[QCA_WLAN_AC_ALL] = {0};

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		epcs_update_ac_value(&edca[i], &default_epcs_edca[i]);
		edca[i].no_ack = mac_ctx->no_ack_policy_cfg[i];
	}

	mlme_debug("using default edca info");
	return lim_send_epcs_update_edca_params(vdev, edca, false);
}

static QDF_STATUS
epcs_update_edca_param(struct wlan_objmgr_vdev *vdev,
		       struct edca_ie *edca_ie)
{
	struct mac_context *mac_ctx;
	struct ac_param_record *ac_record;
	tSirMacEdcaParamRecord edca[QCA_WLAN_AC_ALL] = {0};
	int i;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	if (edca_ie->ie != DOT11F_EID_EDCAPARAMSET ||
	    edca_ie->len != DOT11F_IE_EDCAPARAMSET_MIN_LEN) {
		mlme_debug("edca info is not valid or not exist");
		return QDF_STATUS_E_INVAL;
	}

	ac_record = edca_ie->ac_record;
	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		epcs_update_ac_value(&edca[i], &ac_record[i]);
		edca[i].no_ack = mac_ctx->no_ack_policy_cfg[i];
	}

	return lim_send_epcs_update_edca_params(vdev, edca, false);
}

static QDF_STATUS
epcs_update_ven_wmm_param(struct wlan_objmgr_vdev *vdev, uint8_t *ven_wme_ie)
{
	struct mac_context *mac_ctx;
	tDot11fIEWMMParams wmm_para = {0};
	tSirMacEdcaParamRecord edca[QCA_WLAN_AC_ALL] = {0};
	uint32_t status;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	status = dot11f_unpack_ie_wmm_params(mac_ctx,
					     ven_wme_ie + WMM_VENDOR_HEADER_LEN,
					     DOT11F_IE_WMMPARAMS_MIN_LEN,
					     &wmm_para, false);
	if (status != DOT11F_PARSE_SUCCESS) {
		mlme_debug("EPCS parsing wmm ie error");
		return QDF_STATUS_E_INVAL;
	}

	edca[QCA_WLAN_AC_BE].aci.rsvd = wmm_para.unused1;
	edca[QCA_WLAN_AC_BE].aci.aci = wmm_para.acbe_aci;
	edca[QCA_WLAN_AC_BE].aci.acm = wmm_para.acbe_acm;
	edca[QCA_WLAN_AC_BE].aci.aifsn = wmm_para.acbe_aifsn;
	edca[QCA_WLAN_AC_BE].cw.max = wmm_para.acbe_acwmax;
	edca[QCA_WLAN_AC_BE].cw.min = wmm_para.acbe_acwmin;
	edca[QCA_WLAN_AC_BE].txoplimit = wmm_para.acbe_txoplimit;
	edca[QCA_WLAN_AC_BE].no_ack =
				mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_BE];
	mlme_debug("WMM BE aci %d, acm %d, aifsn %d, cwmax %d, cwmin %d",
		   edca[QCA_WLAN_AC_BE].aci.aci,
		   edca[QCA_WLAN_AC_BE].aci.acm,
		   edca[QCA_WLAN_AC_BE].aci.aifsn,
		   edca[QCA_WLAN_AC_BE].cw.max,
		   edca[QCA_WLAN_AC_BE].cw.min);

	edca[QCA_WLAN_AC_BK].aci.rsvd = wmm_para.unused2;
	edca[QCA_WLAN_AC_BK].aci.aci = wmm_para.acbk_aci;
	edca[QCA_WLAN_AC_BK].aci.acm = wmm_para.acbk_acm;
	edca[QCA_WLAN_AC_BK].aci.aifsn = wmm_para.acbk_aifsn;
	edca[QCA_WLAN_AC_BK].cw.max = wmm_para.acbk_acwmax;
	edca[QCA_WLAN_AC_BK].cw.min = wmm_para.acbk_acwmin;
	edca[QCA_WLAN_AC_BK].txoplimit = wmm_para.acbk_txoplimit;
	edca[QCA_WLAN_AC_BK].no_ack =
				mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_BK];
	mlme_debug("WMM BK aci %d, acm %d, aifsn %d, cwmax %d, cwmin %d",
		   edca[QCA_WLAN_AC_BK].aci.aci,
		   edca[QCA_WLAN_AC_BK].aci.acm,
		   edca[QCA_WLAN_AC_BK].aci.aifsn,
		   edca[QCA_WLAN_AC_BK].cw.max,
		   edca[QCA_WLAN_AC_BK].cw.min);

	edca[QCA_WLAN_AC_VI].aci.rsvd = wmm_para.unused3;
	edca[QCA_WLAN_AC_VI].aci.aci = wmm_para.acvi_aci;
	edca[QCA_WLAN_AC_VI].aci.acm = wmm_para.acvi_acm;
	edca[QCA_WLAN_AC_VI].aci.aifsn = wmm_para.acvi_aifsn;
	edca[QCA_WLAN_AC_VI].cw.max = wmm_para.acvi_acwmax;
	edca[QCA_WLAN_AC_VI].cw.min = wmm_para.acvi_acwmin;
	edca[QCA_WLAN_AC_VI].txoplimit = wmm_para.acvi_txoplimit;
	edca[QCA_WLAN_AC_VI].no_ack =
				mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_VI];
	mlme_debug("WMM VI aci %d, acm %d, aifsn %d, cwmax %d, cwmin %d",
		   edca[QCA_WLAN_AC_VI].aci.aci,
		   edca[QCA_WLAN_AC_VI].aci.acm,
		   edca[QCA_WLAN_AC_VI].aci.aifsn,
		   edca[QCA_WLAN_AC_VI].cw.max,
		   edca[QCA_WLAN_AC_VI].cw.min);

	edca[QCA_WLAN_AC_VO].aci.rsvd = wmm_para.unused4;
	edca[QCA_WLAN_AC_VO].aci.aci = wmm_para.acvo_aci;
	edca[QCA_WLAN_AC_VO].aci.acm = wmm_para.acvo_acm;
	edca[QCA_WLAN_AC_VO].aci.aifsn = wmm_para.acvo_aifsn;
	edca[QCA_WLAN_AC_VO].cw.max = wmm_para.acvo_acwmax;
	edca[QCA_WLAN_AC_VO].cw.min = wmm_para.acvo_acwmin;
	edca[QCA_WLAN_AC_VO].txoplimit = wmm_para.acvo_txoplimit;
	edca[QCA_WLAN_AC_VO].no_ack =
				mac_ctx->no_ack_policy_cfg[QCA_WLAN_AC_VO];
	mlme_debug("WMM VO aci %d, acm %d, aifsn %d, cwmax %d, cwmin %d",
		   edca[QCA_WLAN_AC_VO].aci.aci,
		   edca[QCA_WLAN_AC_VO].aci.acm,
		   edca[QCA_WLAN_AC_VO].aci.aifsn,
		   edca[QCA_WLAN_AC_VO].cw.max,
		   edca[QCA_WLAN_AC_VO].cw.min);

	return lim_send_epcs_update_edca_params(vdev, edca, false);
}

static QDF_STATUS
epcs_update_mu_edca_param(struct wlan_objmgr_vdev *vdev,
			  struct muedca_ie *muedca)
{
	struct mac_context *mac_ctx;
	struct muac_param_record *mu_record;
	tSirMacEdcaParamRecord edca[QCA_WLAN_AC_ALL] = {0};
	int i;

	if (muedca->elem_id != DOT11F_EID_MU_EDCA_PARAM_SET ||
	    muedca->elem_len != (DOT11F_IE_MU_EDCA_PARAM_SET_MIN_LEN + 1)) {
		mlme_debug("mu edca info for epcs is not valid or not exist");
		return QDF_STATUS_SUCCESS;
	}

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	mu_record = muedca->mu_record;
	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		epcs_update_mu_ac_value(&edca[i], &mu_record[i]);
		edca[i].no_ack = mac_ctx->no_ack_policy_cfg[i];
	}

	return lim_send_epcs_update_edca_params(vdev, edca, true);
}

static QDF_STATUS
epcs_restore_edca_param(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_vdev *link_vdev;
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	int i;

	if (!vdev)
		return QDF_STATUS_E_INVAL;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx)
		return QDF_STATUS_E_INVAL;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		link_vdev = mlo_dev_ctx->wlan_vdev_list[i];
		if (!link_vdev)
			continue;
		lim_send_epcs_restore_edca_params(link_vdev);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS epcs_handle_rx_req(struct wlan_objmgr_vdev *vdev,
				     struct wlan_objmgr_peer *peer,
				     void *event_data, uint32_t len)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_peer_epcs_info *epcs_info;
	struct wlan_epcs_info epcs_req = {0};
	struct wlan_action_frame_args args;
	struct ml_pa_info *edca_info;
	struct ml_pa_partner_link_info *link;
	struct wlan_objmgr_vdev *link_vdev;
	uint32_t i;
	QDF_STATUS status;

	if (!vdev || !peer)
		return QDF_STATUS_E_INVAL;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	epcs_info = &ml_peer->epcs_info;
	if (epcs_info->state == EPCS_ENABLE) {
		mlme_err("EPCS has been enable, ignore the req.");
		return QDF_STATUS_E_ALREADY;
	}

	status = wlan_mlo_parse_epcs_action_frame(&epcs_req, event_data, len);
	if (status != QDF_STATUS_SUCCESS) {
		mlme_err("Unable to parse EPCS request action frame");
		return QDF_STATUS_E_FAILURE;
	}

	epcs_info->self_gen_dialog_token = epcs_req.dialog_token;
	edca_info = &epcs_req.pa_info;
	for (i = 0; i < edca_info->num_links; i++) {
		link = &edca_info->link_info[i];
		link_vdev = mlo_get_vdev_by_link_id(vdev, link->link_id,
						    WLAN_MLO_MGR_ID);
		if (!link_vdev)
			continue;

		if (link->edca_ie_present)
			epcs_update_edca_param(link_vdev, &link->edca);
		else if (link->ven_wme_ie_present)
			epcs_update_ven_wmm_param(link_vdev,
						  &link->ven_wme_ie_bytes[0]);
		else
			epcs_update_def_edca_param(link_vdev);

		if (link->muedca_ie_present)
			epcs_update_mu_edca_param(link_vdev, &link->muedca);

		wlan_objmgr_vdev_release_ref(link_vdev, WLAN_MLO_MGR_ID);
	}

	args.category = ACTION_CATEGORY_PROTECTED_EHT;
	args.action = EHT_EPCS_RESPONSE;
	args.arg1 = epcs_info->self_gen_dialog_token;
	args.arg2 = QDF_STATUS_SUCCESS;

	status = lim_send_epcs_action_rsp_frame(vdev,
						wlan_peer_get_macaddr(peer),
						&args);
	if (status != QDF_STATUS_SUCCESS) {
		mlme_err("Send EPCS response frame error");
		epcs_restore_edca_param(vdev);
	} else {
		epcs_info->state = EPCS_ENABLE;
		mlme_debug("EPCS (responder) state: Teardown -> Enable");
	}

	return status;
}

static QDF_STATUS epcs_handle_rx_resp(struct wlan_objmgr_vdev *vdev,
				      struct wlan_objmgr_peer *peer,
				      void *event_data, uint32_t len)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_peer_epcs_info *epcs_info;
	struct wlan_epcs_info epcs_rsp = {0};
	struct ml_pa_info *edca_info;
	struct ml_pa_partner_link_info *link;
	struct wlan_objmgr_vdev *link_vdev;
	uint32_t i;
	QDF_STATUS status;

	if (!vdev || !peer)
		return QDF_STATUS_E_NULL_VALUE;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return QDF_STATUS_E_NULL_VALUE;

	epcs_info = &ml_peer->epcs_info;
	if (epcs_info->state == EPCS_ENABLE) {
		mlme_err("EPCS has been enable, ignore the rsp.");
		return QDF_STATUS_E_ALREADY;
	}

	status = wlan_mlo_parse_epcs_action_frame(&epcs_rsp, event_data, len);
	if (status != QDF_STATUS_SUCCESS) {
		mlme_err("Unable to parse EPCS response action frame");
		return QDF_STATUS_E_FAILURE;
	}

	if (epcs_info->self_gen_dialog_token != epcs_rsp.dialog_token) {
		mlme_err("epcs rsp dialog token %d does not match",
			 epcs_rsp.dialog_token);
		return QDF_STATUS_E_FAILURE;
	}

	if (epcs_rsp.status) {
		mlme_err("epcs rsp status error %d", epcs_rsp.status);
		return QDF_STATUS_E_FAILURE;
	}

	edca_info = &epcs_rsp.pa_info;
	for (i = 0; i < edca_info->num_links; i++) {
		link = &edca_info->link_info[i];
		link_vdev = mlo_get_vdev_by_link_id(vdev, link->link_id,
						    WLAN_MLO_MGR_ID);
		if (!link_vdev)
			continue;

		if (link->edca_ie_present)
			epcs_update_edca_param(link_vdev, &link->edca);
		else if (link->ven_wme_ie_present)
			epcs_update_ven_wmm_param(link_vdev,
						  &link->ven_wme_ie_bytes[0]);
		else
			epcs_update_def_edca_param(link_vdev);

		if (link->muedca_ie_present)
			epcs_update_mu_edca_param(link_vdev, &link->muedca);

		wlan_objmgr_vdev_release_ref(link_vdev, WLAN_MLO_MGR_ID);
	}

	epcs_info->state = EPCS_ENABLE;
	mlme_debug("EPCS (initiator) state: Teardown -> Enable");

	return status;
}

static QDF_STATUS epcs_handle_rx_teardown(struct wlan_objmgr_vdev *vdev,
					  struct wlan_objmgr_peer *peer,
					  void *event_data, uint32_t len)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_mlo_peer_epcs_info *epcs_info;
	struct wlan_epcs_info epcs_req = {0};
	struct mac_context *mac_ctx;
	QDF_STATUS status;

	if (!vdev || !peer)
		return QDF_STATUS_E_INVAL;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer)
		return QDF_STATUS_E_FAILURE;

	epcs_info = &ml_peer->epcs_info;
	if (epcs_info->state == EPCS_DOWN) {
		mlme_err("EPCS has been down, ignore the teardown req.");
		return QDF_STATUS_E_ALREADY;
	}

	status = wlan_mlo_parse_epcs_action_frame(&epcs_req, event_data, len);
	if (status != QDF_STATUS_SUCCESS) {
		mlme_err("Unable to parse EPCS teardown action frame");
		return QDF_STATUS_E_FAILURE;
	}

	epcs_restore_edca_param(vdev);

	epcs_info->state = EPCS_DOWN;
	mlme_debug("EPCS state: Enale -> Teardown.");

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS epcs_handle_tx_req(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_objmgr_peer *peer;
	struct wlan_action_frame_args args;
	struct wlan_mlo_peer_epcs_info *epcs_info;
	QDF_STATUS status;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer)
		return QDF_STATUS_E_NULL_VALUE;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_peer;
	}

	epcs_info = &ml_peer->epcs_info;
	if (epcs_info->state == EPCS_ENABLE) {
		mlme_err("EPCS has been enable, ignore the req cmd.");
		status = QDF_STATUS_E_ALREADY;
		goto release_peer;
	}

	args.category = ACTION_CATEGORY_PROTECTED_EHT;
	args.action = EHT_EPCS_REQUEST;
	args.arg1 = epcs_gen_dialog_token(epcs_info);

	status = lim_send_epcs_action_req_frame(vdev,
						wlan_peer_get_macaddr(peer),
						&args);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("Failed to send EPCS action request frame");

release_peer:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);

	return status;
}

static QDF_STATUS epcs_handle_tx_teardown(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_objmgr_peer *peer;
	struct wlan_action_frame_args args;
	struct wlan_mlo_peer_epcs_info *epcs_info;
	QDF_STATUS status;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLO_MGR_ID);
	if (!peer)
		return QDF_STATUS_E_NULL_VALUE;

	ml_peer = peer->mlo_peer_ctx;
	if (!ml_peer) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_peer;
	}

	epcs_info = &ml_peer->epcs_info;
	if (epcs_info->state == EPCS_DOWN) {
		mlme_err("EPCS has been down, ignore the teardwon cmd.");
		status = QDF_STATUS_E_ALREADY;
		goto release_peer;
	}

	args.category = ACTION_CATEGORY_PROTECTED_EHT;
	args.action = EHT_EPCS_TEARDOWN;

	status =
	    lim_send_epcs_action_teardown_frame(vdev,
						wlan_peer_get_macaddr(peer),
						&args);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Failed to send EPCS tear down frame");
	} else {
		epcs_restore_edca_param(vdev);
		epcs_info->state = EPCS_DOWN;
		mlme_debug("EPCS state: Enale -> Teardown.");
	}

release_peer:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLO_MGR_ID);

	return status;
}

static QDF_STATUS epcs_deliver_event(struct wlan_objmgr_vdev *vdev,
				     struct wlan_objmgr_peer *peer,
				     enum wlan_epcs_evt event,
				     void *event_data, uint32_t len)
{
	QDF_STATUS status;

	mlme_debug("EPCS event received: %s(%d)",
		   epcs_get_event_str(event), event);

	switch (event) {
	case WLAN_EPCS_EV_ACTION_FRAME_RX_REQ:
		status = epcs_handle_rx_req(vdev, peer, event_data, len);
		break;
	case WLAN_EPCS_EV_ACTION_FRAME_RX_RESP:
		status = epcs_handle_rx_resp(vdev, peer, event_data, len);
		break;
	case WLAN_EPCS_EV_ACTION_FRAME_RX_TEARDOWN:
		status = epcs_handle_rx_teardown(vdev, peer, event_data, len);
		break;
	default:
		status = QDF_STATUS_E_FAILURE;
		mlme_err("Unhandled EPCS event");
	}

	return status;
}

QDF_STATUS wlan_epcs_deliver_event(struct wlan_objmgr_vdev *vdev,
				   struct wlan_objmgr_peer *peer,
				   enum wlan_epcs_evt event,
				   void *event_data, uint32_t len)
{
	return epcs_deliver_event(vdev, peer, event, event_data, len);
}

static QDF_STATUS epcs_deliver_cmd(struct wlan_objmgr_vdev *vdev,
				   enum wlan_epcs_evt event)
{
	QDF_STATUS status;

	mlme_debug("EPCS cmd received: %s(%d)",
		   epcs_get_event_str(event), event);

	switch (event) {
	case WLAN_EPCS_EV_ACTION_FRAME_TX_REQ:
		status = epcs_handle_tx_req(vdev);
		break;
	case WLAN_EPCS_EV_ACTION_FRAME_TX_TEARDOWN:
		status = epcs_handle_tx_teardown(vdev);
		break;
	default:
		status = QDF_STATUS_E_FAILURE;
		mlme_err("Unhandled EPCS cmd");
	}

	return status;
}

QDF_STATUS wlan_epcs_deliver_cmd(struct wlan_objmgr_vdev *vdev,
				 enum wlan_epcs_evt event)
{
	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	if (!wlan_mlme_get_epcs_capability(wlan_vdev_get_psoc(vdev))) {
		mlme_info("EPCS has been disabled");
		return QDF_STATUS_E_FAILURE;
	}

	return epcs_deliver_cmd(vdev, event);
}

QDF_STATUS wlan_epcs_set_config(struct wlan_objmgr_vdev *vdev, uint8_t flag)
{
	struct mac_context *mac_ctx;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	if (flag)
		wlan_mlme_set_epcs_capability(wlan_vdev_get_psoc(vdev), true);
	else
		wlan_mlme_set_epcs_capability(wlan_vdev_get_psoc(vdev), false);

	return lim_send_eht_caps_ie(mac_ctx, QDF_STA_MODE,
				    wlan_vdev_get_id(vdev));
}

bool wlan_epcs_get_config(struct wlan_objmgr_vdev *vdev)
{
	bool epcs_flag;

	if (!vdev)
		return false;

	epcs_flag = wlan_mlme_get_epcs_capability(wlan_vdev_get_psoc(vdev));
	mlme_debug("EPCS %s", epcs_flag ? "Enabled" : "Disabled");

	return epcs_flag;
}
