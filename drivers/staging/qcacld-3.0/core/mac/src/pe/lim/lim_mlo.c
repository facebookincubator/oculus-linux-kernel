/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC : lim_mlo.c
 *
 * WLAN Host Device Driver file for 802.11be (Extremely High Throughput)
 * support.
 *
 */

#include "lim_mlo.h"
#include "sch_api.h"
#include "lim_types.h"
#include "wlan_mlo_mgr_ap.h"
#include "wlan_mlo_mgr_op.h"
#include <wlan_mlo_mgr_peer.h>
#include <lim_assoc_utils.h>
#include <wlan_mlo_mgr_peer.h>
#include <lim_utils.h>
#include <utils_mlo.h>

QDF_STATUS lim_cu_info_from_rnr_per_link_id(const uint8_t *rnr,
					    uint8_t linkid, uint8_t *bpcc,
					    uint8_t *aui)
{
	const uint8_t *data, *rnr_end;
	struct neighbor_ap_info_field *neighbor_ap_info;
	uint8_t tbtt_type, tbtt_len, tbtt_count;
	uint8_t mld_pos, mld_id, link_id;
	struct rnr_mld_info *mld_param;
	int32_t i, len;
	uint8_t nbr_ap_info_len = sizeof(struct neighbor_ap_info_field);

	if (!rnr)
		return QDF_STATUS_E_INVAL;

	rnr_end = rnr + rnr[TAG_LEN_POS] + MIN_IE_LEN;
	data = rnr + PAYLOAD_START_POS;
	while ((data + sizeof(struct neighbor_ap_info_field)) <= rnr_end) {
		neighbor_ap_info = (struct neighbor_ap_info_field *)data;
		tbtt_count = neighbor_ap_info->tbtt_header.tbtt_info_count;
		tbtt_len = neighbor_ap_info->tbtt_header.tbtt_info_length;
		tbtt_type = neighbor_ap_info->tbtt_header.tbbt_info_fieldtype;
		len = tbtt_len * (tbtt_count + 1) + nbr_ap_info_len;
		if (data + len > rnr_end) {
			pe_debug("error about rnr length");
			return QDF_STATUS_E_INVAL;
		}

		if (tbtt_len >=
		    TBTT_NEIGHBOR_AP_BSSID_S_SSID_BSS_PARAM_20MHZ_PSD_MLD_PARAM)
			mld_pos =
			      TBTT_NEIGHBOR_AP_BSSID_S_SSID_BSS_PARAM_20MHZ_PSD;
		else
			mld_pos = 0;

		if (mld_pos == 0 || tbtt_type != 0) {
			data += len;
			continue;
		}

		data += nbr_ap_info_len;
		for (i = 0; i < tbtt_count + 1; i++) {
			mld_param = (struct rnr_mld_info *)&data[mld_pos];
			mld_id = mld_param->mld_id;
			if (mld_id == 0) {
				link_id = mld_param->link_id;
				if (linkid == link_id) {
					*bpcc = mld_param->bss_param_change_cnt;
					*aui = mld_param->all_updates_included;
					pe_debug("rnr bpcc %d, aui %d, linkid %d",
						 *bpcc, *aui, linkid);
					return QDF_STATUS_SUCCESS;
				}
			}
			data += tbtt_len;
		}
	}

	return QDF_STATUS_E_INVAL;
}

QDF_STATUS lim_get_bpcc_from_mlo_ie(tSchBeaconStruct *bcn, uint8_t *bpcc)
{
	struct sir_multi_link_ie *mlo_ie;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	if (!bcn)
		return status;

	mlo_ie = &bcn->mlo_ie;
	if (mlo_ie->mlo_ie_present &&
	    mlo_ie->mlo_ie.bss_param_change_cnt_present) {
		*bpcc = mlo_ie->mlo_ie.bss_param_change_count;
		pe_debug("mlie bpcc %d", *bpcc);
		status = QDF_STATUS_SUCCESS;
	} else {
		*bpcc = 0;
	}

	return status;
}

bool lim_check_cu_happens(struct wlan_objmgr_vdev *vdev, uint8_t new_bpcc)
{
	uint8_t bpcc;
	uint8_t vdev_id;
	QDF_STATUS status;

	if (!vdev || !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return false;

	vdev_id = wlan_vdev_get_id(vdev);

	status = wlan_mlo_get_cu_bpcc(vdev, &bpcc);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	if (new_bpcc == 0 && bpcc == 0)
		return false;

	pe_debug_rl("vdev id %d new bpcc %d, old bpcc %d",
		    vdev_id, new_bpcc, bpcc);
	if (new_bpcc && new_bpcc < bpcc)
		return false;

	wlan_mlo_set_cu_bpcc(vdev, new_bpcc);

	return true;
}

/**
 * lim_send_mlo_ie_update() - mlo ie is changed, populate new beacon template
 * @session: pe session
 *
 * Return: void
 */
static void lim_send_mlo_ie_update(struct mac_context *mac_ctx,
				   struct pe_session *session)
{
	if (QDF_IS_STATUS_ERROR(
		sch_set_fixed_beacon_fields(mac_ctx, session))) {
		pe_err("Unable to update mlo IE in beacon");
		return;
	}

	lim_send_beacon_ind(mac_ctx, session, REASON_MLO_IE_UPDATE);
}

QDF_STATUS lim_partner_link_info_change(struct wlan_objmgr_vdev *vdev)
{
	struct pe_session *session;
	struct mac_context *mac;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		pe_err("mac ctx is null");
		return QDF_STATUS_E_INVAL;
	}
	if (!vdev) {
		pe_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}
	session = pe_find_session_by_vdev_id(
			mac, vdev->vdev_objmgr.vdev_id);
	if (!session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (session->mlo_link_info.bcn_tmpl_exist)
		lim_send_mlo_ie_update(mac, session);

	return QDF_STATUS_SUCCESS;
}

void lim_mlo_release_vdev_ref(struct wlan_objmgr_vdev *vdev)
{
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
}

struct pe_session *pe_find_partner_session_by_link_id(
			struct pe_session *session, uint8_t link_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mac_context *mac;
	struct pe_session *partner_session;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		pe_err("mac ctx is null");
		return NULL;
	}

	if (!session) {
		pe_err("session is null");
		return NULL;
	}

	vdev = mlo_get_vdev_by_link_id(session->vdev, link_id,
				       WLAN_LEGACY_MAC_ID);

	if (!vdev) {
		pe_err("vdev is null");
		return NULL;
	}

	partner_session = pe_find_session_by_vdev_id(
			mac, vdev->vdev_objmgr.vdev_id);

	if (!partner_session)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	return partner_session;
}

void lim_get_mlo_vdev_list(struct pe_session *session, uint16_t *vdev_count,
			   struct wlan_objmgr_vdev **wlan_vdev_list)
{
	mlo_ap_get_vdev_list(session->vdev, vdev_count,
			     wlan_vdev_list);
}

/**
 * lim_mlo_get_assoc_link_session_sta_ds() - get assoc link session and sta ds
 * @session: pe session
 * @partner_peer_idx: aid
 * @assoc_session: assoc link session
 * @assoc_sta: assoc sta ds
 *
 * Return: void
 */
static void lim_mlo_get_assoc_link_session_sta_ds(
				struct pe_session *session,
				uint16_t partner_peer_idx,
				struct pe_session **assoc_session,
				tpDphHashNode *assoc_sta)
{
	struct wlan_mlo_peer_context *mlo_peer_ctx;
	struct wlan_objmgr_peer *peer;
	uint16_t aid = 0;
	struct mac_context *mac;
	struct wlan_objmgr_vdev *vdev;
	struct pe_session *partner_session;

	*assoc_session = NULL;
	*assoc_sta = NULL;
	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		pe_err("mac ctx is null");
		return;
	}
	if (!session) {
		pe_err("session is NULL");
		return;
	}

	mlo_peer_ctx = wlan_mlo_get_mlpeer_by_aid(session->vdev->mlo_dev_ctx,
						  partner_peer_idx);
	if (!mlo_peer_ctx) {
		pe_err("mlo peer ctx is null");
		return;
	}
	peer = wlan_mlo_peer_get_assoc_peer(mlo_peer_ctx);
	if (!peer) {
		pe_err("peer is null");
		return;
	}
	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		pe_err("vdev is null");
		return;
	}
	partner_session = pe_find_session_by_vdev_id(
				mac, vdev->vdev_objmgr.vdev_id);

	if (!partner_session) {
		pe_err("assoc session is null");
		return;
	}
	*assoc_sta = dph_lookup_hash_entry(mac, peer->macaddr, &aid,
					   &partner_session->dph.dphHashTable);
	*assoc_session = partner_session;
}

/**
 * lim_mlo_update_cleanup_trigger () - update clean up trigger
 * @session: pointer to session
 * @sta_ds: sta ds
 * @clnup_tri: clean up trigger
 *
 * Return: Void
 */
static void lim_mlo_update_cleanup_trigger(struct pe_session *session,
					   tpDphHashNode sta_ds,
					   uint16_t clnup_tri)
{
	tpDphHashNode assoc_sta = NULL;
	struct pe_session *link_session;
	struct pe_session *assoc_session = NULL;
	struct mac_context *mac_ctx;
	tpDphHashNode link_sta;
	uint8_t link_id;
	int link;
	uint8_t *sta_addr;
	uint16_t assoc_id;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac ctx is null");
		return;
	}
	if (!session) {
		pe_err("session is null");
		return;
	}
	if (!sta_ds) {
		pe_err("sta ds is null");
		return;
	}

	if (lim_is_mlo_recv_assoc(sta_ds)) {
		assoc_sta = sta_ds;
	} else {
		lim_mlo_get_assoc_link_session_sta_ds(session, sta_ds->assocId,
						      &assoc_session,
						      &assoc_sta);
		if (!assoc_sta) {
			pe_err("assoc link sta ds is null");
			return;
		}

		assoc_sta->mlmStaContext.cleanupTrigger = clnup_tri;
	}
	for (link = 0; link < assoc_sta->mlo_info.num_partner_links; link++) {
		link_id = assoc_sta->mlo_info.partner_link_info[link].link_id;
		link_session = pe_find_partner_session_by_link_id(session,
								  link_id);
		if (!link_session)
			continue;
		sta_addr =
		    assoc_sta->mlo_info.partner_link_info[link].link_addr.bytes;
		link_sta = dph_lookup_hash_entry(
				mac_ctx,
				sta_addr,
				&assoc_id,
				&link_session->dph.dphHashTable);
		if (!link_sta || link_sta == sta_ds) {
			lim_mlo_release_vdev_ref(link_session->vdev);
			continue;
		}
		link_sta->mlmStaContext.cleanupTrigger = clnup_tri;
		lim_mlo_release_vdev_ref(link_session->vdev);
	}
}

void lim_mlo_notify_peer_disconn(struct pe_session *pe_session,
				 tpDphHashNode sta_ds)
{
	struct wlan_objmgr_peer *peer;
	struct mac_context *mac_ctx;

	if (!pe_session) {
		pe_err("pe session is null");
		return;
	}
	if (!sta_ds) {
		pe_err("sta ds is null");
		return;
	}
	mac_ctx = pe_session->mac_ctx;
	if (!mac_ctx) {
		pe_err("mac context is null");
		return;
	}

	peer = wlan_objmgr_get_peer_by_mac(mac_ctx->psoc,
					   sta_ds->staAddr,
					   WLAN_LEGACY_MAC_ID);
	if (!peer) {
		pe_err("peer is null");
		return;
	}

	if (wlan_peer_mlme_flag_ext_get(peer, WLAN_PEER_FEXT_MLO)) {
		if (wlan_vdev_mlme_is_mlo_ap(pe_session->vdev))
			lim_mlo_update_cleanup_trigger(
					pe_session, sta_ds,
					sta_ds->mlmStaContext.cleanupTrigger);
		wlan_mlo_partner_peer_disconnect_notify(peer);
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
}

void lim_mlo_sta_notify_peer_disconn(struct pe_session *pe_session)
{
	struct wlan_objmgr_peer *peer;
	struct mac_context *mac_ctx;

	if (!pe_session) {
		pe_err("pe session is null");
		return;
	}
	mac_ctx = pe_session->mac_ctx;
	if (!mac_ctx) {
		pe_err("mac context is null");
		return;
	}

	peer = wlan_objmgr_get_peer_by_mac(mac_ctx->psoc,
					   pe_session->bssId,
					   WLAN_LEGACY_MAC_ID);
	if (!peer) {
		pe_err("peer is null");
		return;
	}

	if (wlan_peer_mlme_flag_ext_get(peer, WLAN_PEER_FEXT_MLO))
		wlan_mlo_partner_peer_disconnect_notify(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
}

void lim_mlo_roam_peer_disconn_del(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssid;

	if (!vdev) {
		pe_err("vdev is null");
		return;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pe_err("psoc is null");
		return;
	}

	status = wlan_vdev_get_bss_peer_mac(vdev, &bssid);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_debug("vdev id %d : failed to get bssid",
			 wlan_vdev_get_id(vdev));
		return;
	}

	peer = wlan_objmgr_get_peer_by_mac(psoc,
					   bssid.bytes,
					   WLAN_LEGACY_MAC_ID);
	if (!peer) {
		pe_err("peer is null");
		return;
	}

	if (wlan_peer_mlme_flag_ext_get(peer, WLAN_PEER_FEXT_MLO)) {
		pe_debug("vdev id %d disconn del peer", wlan_vdev_get_id(vdev));
		wlan_mlo_partner_peer_disconnect_notify(peer);
		wlan_mlo_link_peer_delete(peer);
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
}

void lim_mlo_cleanup_partner_peer(struct wlan_objmgr_peer *peer)
{
	struct mac_context *mac_ctx;
	uint16_t aid;
	tpDphHashNode sta_ds;
	struct pe_session *pe_session;
	tpSirAssocReq tmp_assoc_req;
	struct wlan_objmgr_vdev *vdev;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac ctx is null");
		return;
	}

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		pe_err("vdev is null");
		return;
	}

	pe_session = pe_find_session_by_vdev_id(
			mac_ctx, vdev->vdev_objmgr.vdev_id);
	if (!pe_session) {
		pe_err("pe session is null");
		return;
	}

	sta_ds = dph_lookup_hash_entry(mac_ctx, peer->macaddr, &aid,
				       &pe_session->dph.dphHashTable);

	if (!sta_ds) {
		pe_err("sta ds is null");
		return;
	}

	lim_cleanup_rx_path(mac_ctx, sta_ds, pe_session, true);

	if (pe_session->parsedAssocReq) {
		tmp_assoc_req = pe_session->parsedAssocReq[sta_ds->assocId];
		if (tmp_assoc_req) {
			lim_free_assoc_req_frm_buf(tmp_assoc_req);
			qdf_mem_free(tmp_assoc_req);
			tmp_assoc_req = NULL;
		}

		pe_session->parsedAssocReq[sta_ds->assocId] = NULL;
	}
}

void lim_mlo_set_mld_mac_peer(tpDphHashNode sta_ds,
			      uint8_t peer_mld_addr[QDF_MAC_ADDR_SIZE])
{
	WLAN_ADDR_COPY(sta_ds->mld_addr, peer_mld_addr);
}

bool lim_is_mlo_conn(struct pe_session *session, tpDphHashNode sta_ds)
{
	bool mlo_conn = false;

	if (!sta_ds) {
		pe_err("sta ds is null");
		return mlo_conn;
	}

	if (!session) {
		pe_err("session is null");
		return mlo_conn;
	}

	if (wlan_vdev_mlme_is_mlo_vdev(session->vdev) &&
	    !qdf_is_macaddr_zero((struct qdf_mac_addr *)sta_ds->mld_addr))
		mlo_conn = true;

	return mlo_conn;
}

void lim_set_mlo_recv_assoc(tpDphHashNode sta_ds, bool mlo_recv_assoc_frm)
{
	if (!sta_ds) {
		pe_err("sta ds is null");
		return;
	}

	sta_ds->recv_assoc_frm = mlo_recv_assoc_frm;
}

bool lim_is_mlo_recv_assoc(tpDphHashNode sta_ds)
{
	if (!sta_ds) {
		pe_err("sta ds is null");
		return false;
	}

	return sta_ds->recv_assoc_frm;
}

QDF_STATUS lim_mlo_proc_assoc_req_frm(struct wlan_objmgr_vdev *vdev,
				      struct wlan_mlo_peer_context *ml_peer,
				      struct qdf_mac_addr *link_addr,
				      qdf_nbuf_t buf)
{
	struct mac_context *mac_ctx;
	struct pe_session *session;
	tSirMacAddr sa;
	uint8_t sub_type;
	uint32_t frame_len;
	uint8_t *frm_body;
	tpSirMacMgmtHdr pHdr;
	tSirMacFrameCtl fc;
	tpSirAssocReq assoc_req;
	QDF_STATUS status;
	qdf_size_t link_frame_len = 0;
	struct qdf_mac_addr link_bssid;

	if (!vdev) {
		pe_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!ml_peer) {
		pe_err("ml_peer is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!link_addr) {
		pe_err("link addr is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!buf) {
		pe_err("assoq req buf is null");
		return QDF_STATUS_E_INVAL;
	}

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac ctx is null");
		return QDF_STATUS_E_INVAL;
	}

	session = pe_find_session_by_vdev_id(
			mac_ctx, vdev->vdev_objmgr.vdev_id);
	if (!session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_nbuf_len(buf) <= sizeof(*pHdr)) {
		pe_err("invalid buf");
		return QDF_STATUS_E_INVAL;
	}

	frame_len = qdf_nbuf_len(buf) - sizeof(*pHdr);
	frm_body = qdf_nbuf_data(buf) + sizeof(*pHdr);
	pHdr = (tpSirMacMgmtHdr)qdf_nbuf_data(buf);
	fc = pHdr->fc;

	if (fc.type == SIR_MAC_MGMT_FRAME) {
		if (fc.subType == SIR_MAC_MGMT_ASSOC_REQ) {
			sub_type = LIM_ASSOC;
		} else if (fc.subType == SIR_MAC_MGMT_REASSOC_REQ) {
			sub_type = LIM_REASSOC;
		} else {
			pe_err("invalid mgt_type %d, sub_type %d",
			       fc.type, fc.subType);
			return QDF_STATUS_E_INVAL;
		}
	} else {
		pe_err("invalid mgt_type %d, sub_type %d",
		       fc.type, fc.subType);
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_copy(sa, link_addr->bytes, QDF_MAC_ADDR_SIZE);
	status = lim_check_assoc_req(mac_ctx, sub_type, sa, session);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	/* Allocate memory for the Assoc Request frame */
	assoc_req = qdf_mem_malloc(sizeof(*assoc_req));
	if (!assoc_req)
		return QDF_STATUS_E_NOMEM;

	assoc_req->assoc_req_buf = qdf_nbuf_copy(buf);
	if (!assoc_req->assoc_req_buf) {
		pe_err("partner link assoc request buf clone failed");
		qdf_mem_free(assoc_req);
		return QDF_STATUS_E_NOMEM;
	}
	qdf_copy_macaddr(&link_bssid, (struct qdf_mac_addr *)session->bssId);
	status = util_gen_link_assoc_req(
				frm_body, frame_len, sub_type == LIM_REASSOC,
				0,
				link_bssid,
				qdf_nbuf_data(assoc_req->assoc_req_buf),
				qdf_nbuf_len(assoc_req->assoc_req_buf),
				&link_frame_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_warn("Partner Assoc Req frame gen error. source addr:"
			QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(sa));
		lim_free_assoc_req_frm_buf(assoc_req);
		qdf_mem_free(assoc_req);
		return status;
	}

	qdf_nbuf_set_len(assoc_req->assoc_req_buf, link_frame_len);
	assoc_req->assocReqFrame = qdf_nbuf_data(assoc_req->assoc_req_buf) +
				   sizeof(*pHdr);
	assoc_req->assocReqFrameLength = link_frame_len - sizeof(*pHdr);

	qdf_copy_macaddr((struct qdf_mac_addr *)assoc_req->mld_mac,
			 &ml_peer->peer_mld_addr);
	return lim_proc_assoc_req_frm_cmn(mac_ctx, sub_type, session, sa,
					  assoc_req, ml_peer->assoc_id);
}

void lim_mlo_ap_sta_assoc_suc(struct wlan_objmgr_peer *peer)
{
	struct mac_context *mac;
	tpDphHashNode sta;
	struct pe_session *pe_session;
	struct wlan_objmgr_vdev *vdev;
	uint16_t aid = 0;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		pe_err("mac ctx is null");
		return;
	}
	if (!peer) {
		pe_err("peer is null");
		return;
	}
	vdev = wlan_peer_get_vdev(peer);

	pe_session = pe_find_session_by_vdev_id(
			mac, vdev->vdev_objmgr.vdev_id);

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}
	sta = dph_lookup_hash_entry(mac, peer->macaddr, &aid,
				    &pe_session->dph.dphHashTable);
	if (!sta) {
		pe_err("sta ds is null");
		return;
	}
	if (lim_send_mlm_assoc_ind(mac, sta, pe_session) != QDF_STATUS_SUCCESS)
		lim_reject_association(mac, sta->staAddr,
				       sta->mlmStaContext.subType,
				       true, sta->mlmStaContext.authType,
				       sta->assocId, true,
				       STATUS_UNSPECIFIED_FAILURE,
				       pe_session);
}

void lim_ap_mlo_sta_peer_ind(struct mac_context *mac,
			     struct pe_session *pe_session,
			     tpDphHashNode sta,
			     bool add_sta_rsp_status)
{
	tpSirAssocReq assoc_req;
	struct wlan_mlo_peer_context *ml_peer;
	struct wlan_objmgr_peer *peer;
	struct mlo_partner_info info;
	struct mlo_link_info *linfo;

	if (!sta) {
		pe_err("sta ds is null");
		return;
	}
	if (add_sta_rsp_status) {
		peer = wlan_objmgr_get_peer_by_mac(mac->psoc,
						   sta->staAddr,
						   WLAN_LEGACY_MAC_ID);
		if (!peer) {
			pe_err("peer is null");
			return;
		}

		if (lim_is_mlo_recv_assoc(sta)) {
			assoc_req = pe_session->parsedAssocReq[sta->assocId];
			if (assoc_req->mlo_info.num_partner_links <
			    QDF_ARRAY_SIZE(
				assoc_req->mlo_info.partner_link_info)) {
				qdf_mem_copy(&info, &assoc_req->mlo_info,
					     sizeof(info));
				linfo =
				&info.partner_link_info[info.num_partner_links];
				linfo->link_id = wlan_vdev_get_link_id(
							pe_session->vdev);
				qdf_mem_copy(linfo->link_addr.bytes,
					     sta->staAddr, QDF_MAC_ADDR_SIZE);
				info.num_partner_links++;
				wlan_mlo_peer_create(pe_session->vdev, peer,
						     &info,
						     assoc_req->assoc_req_buf,
						     sta->assocId);
			} else {
				pe_err("invalid partner link number %d",
				       assoc_req->mlo_info.num_partner_links);
			}
		} else {
			ml_peer = wlan_mlo_get_mlpeer_by_aid(
					pe_session->vdev->mlo_dev_ctx,
					sta->assocId);
			if (ml_peer)
				wlan_mlo_link_peer_attach(ml_peer, peer, NULL);
		}
		wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
	} else {
		if (!lim_is_mlo_recv_assoc(sta)) {
			ml_peer = wlan_mlo_get_mlpeer_by_aid(
					pe_session->vdev->mlo_dev_ctx,
					sta->assocId);
			if (ml_peer)
				wlan_mlo_partner_peer_create_failed_notify(
								ml_peer);
		}
	}
}

bool lim_mlo_partner_auth_type(struct pe_session *session,
			       uint16_t partner_peer_idx,
			       tAniAuthType *auth_type)
{
	bool status = false;
	struct pe_session *assoc_link_session = NULL;

	tpDphHashNode sta_ds = NULL;

	lim_mlo_get_assoc_link_session_sta_ds(session, partner_peer_idx,
					      &assoc_link_session, &sta_ds);

	if (sta_ds) {
		*auth_type = sta_ds->mlmStaContext.authType;
		status = true;
	} else {
		pe_err("sta ds is null");
	}

	return status;
}

void lim_mlo_ap_sta_assoc_fail(struct wlan_objmgr_peer *peer)
{
	struct mac_context *mac;
	struct wlan_objmgr_vdev *vdev;
	tpDphHashNode sta;
	struct pe_session *pe_session;
	uint16_t aid = 0;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		pe_err("mac ctx is null");
		return;
	}
	if (!peer) {
		pe_err("peer is null");
		return;
	}
	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		pe_err("vdev is null");
		return;
	}
	pe_session = pe_find_session_by_vdev_id(
			mac, vdev->vdev_objmgr.vdev_id);

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}
	sta = dph_lookup_hash_entry(mac, peer->macaddr, &aid,
				    &pe_session->dph.dphHashTable);
	if (!sta) {
		pe_err("sta ds is null");
		return;
	}
	lim_reject_association(mac, sta->staAddr,
			       sta->mlmStaContext.subType,
			       true, sta->mlmStaContext.authType,
			       sta->assocId, true,
			       STATUS_UNSPECIFIED_FAILURE,
			       pe_session);
}

void lim_mlo_delete_link_peer(struct pe_session *pe_session,
			      tpDphHashNode sta_ds)
{
	struct wlan_objmgr_peer *peer;
	struct mac_context *mac;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		pe_err("mac ctx is null");
		return;
	}
	if (!pe_session) {
		pe_err("pe session is null");
		return;
	}
	if (!sta_ds) {
		pe_err("sta ds is null");
		return;
	}
	if (!lim_is_mlo_conn(pe_session, sta_ds))
		return;

	peer = wlan_objmgr_get_peer_by_mac(mac->psoc,
					   sta_ds->staAddr,
					   WLAN_LEGACY_MAC_ID);
	if (!peer) {
		pe_err("peer is null");
		return;
	}

	wlan_mlo_link_peer_delete(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
}

#if defined(SAP_MULTI_LINK_EMULATION)
QDF_STATUS lim_mlo_assoc_ind_upper_layer(struct mac_context *mac,
					 struct pe_session *pe_session,
					 struct mlo_partner_info *mlo_info)
{
	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS lim_mlo_assoc_ind_upper_layer(struct mac_context *mac,
					 struct pe_session *pe_session,
					 struct mlo_partner_info *mlo_info)
{
	int link;
	uint8_t link_id;
	struct qdf_mac_addr *link_addr;
	struct pe_session *lk_session;
	tpDphHashNode sta;
	uint16_t aid;
	struct assoc_ind *sme_assoc_ind;
	struct scheduler_msg msg;
	tpLimMlmAssocInd lim_assoc_ind;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!mac) {
		pe_err("mac is NULL");
		return status;
	}

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return status;
	}

	if (!mlo_info) {
		pe_err("mlo_info is NULL");
		return status;
	}

	status = QDF_STATUS_SUCCESS;
	for (link = 0; link < mlo_info->num_partner_links; link++) {
		link_id = mlo_info->partner_link_info[link].link_id;
		link_addr = &mlo_info->partner_link_info[link].link_addr;
		lk_session = pe_find_partner_session_by_link_id(pe_session,
								link_id);
		if (!lk_session) {
			pe_err("link_session is NULL");
			status = QDF_STATUS_E_FAILURE;
			break;
		}
		sta = dph_lookup_hash_entry(mac, link_addr->bytes, &aid,
					    &lk_session->dph.dphHashTable);
		if (!sta) {
			pe_err("sta_ds is NULL");
			status = QDF_STATUS_E_FAILURE;
			lim_mlo_release_vdev_ref(lk_session->vdev);
			break;
		}
		lim_assoc_ind = qdf_mem_malloc(sizeof(tLimMlmAssocInd));
		if (!lim_assoc_ind) {
			pe_err("lim assoc ind allocate error");
			qdf_mem_free(lk_session->parsedAssocReq[sta->assocId]);
			lk_session->parsedAssocReq[sta->assocId] = NULL;
			status = QDF_STATUS_E_FAILURE;
			lim_mlo_release_vdev_ref(lk_session->vdev);
			break;
		}

		if (!lim_fill_lim_assoc_ind_params(lim_assoc_ind, mac,
						   sta, lk_session)) {
			pe_err("lim assoc ind fill error");
			qdf_mem_free(lim_assoc_ind);
			qdf_mem_free(lk_session->parsedAssocReq[sta->assocId]);
			lk_session->parsedAssocReq[sta->assocId] = NULL;
			status = QDF_STATUS_E_FAILURE;
			lim_mlo_release_vdev_ref(lk_session->vdev);
			break;
		}
		sme_assoc_ind = qdf_mem_malloc(sizeof(struct assoc_ind));
		if (!sme_assoc_ind) {
			pe_err("sme assoc ind allocate error");
			qdf_mem_free(lim_assoc_ind);
			qdf_mem_free(lk_session->parsedAssocReq[sta->assocId]);
			lk_session->parsedAssocReq[sta->assocId] = NULL;
			status = QDF_STATUS_E_FAILURE;
			lim_mlo_release_vdev_ref(lk_session->vdev);
			break;
		}

		sme_assoc_ind->messageType = eWNI_SME_ASSOC_IND_UPPER_LAYER;
		lim_fill_sme_assoc_ind_params(mac, lim_assoc_ind, sme_assoc_ind,
					      lk_session, true);

		qdf_mem_zero(&msg, sizeof(struct scheduler_msg));
		msg.type = eWNI_SME_ASSOC_IND_UPPER_LAYER;
		msg.bodyptr = sme_assoc_ind;
		msg.bodyval = 0;
		sme_assoc_ind->reassocReq = sta->mlmStaContext.subType;
		sme_assoc_ind->timingMeasCap = sta->timingMeasCap;
		MTRACE(mac_trace_msg_tx(mac, lk_session->peSessionId,
					msg.type));
		lim_sys_process_mmh_msg_api(mac, &msg);

		qdf_mem_free(lim_assoc_ind);
		lim_free_assoc_req_frm_buf(
				lk_session->parsedAssocReq[sta->assocId]);
		qdf_mem_free(lk_session->parsedAssocReq[sta->assocId]);
		lk_session->parsedAssocReq[sta->assocId] = NULL;
		lim_mlo_release_vdev_ref(lk_session->vdev);
	}

	return status;
}
#endif

void lim_mlo_save_mlo_info(tpDphHashNode sta_ds,
			   struct mlo_partner_info *mlo_info)
{
	if (!sta_ds) {
		pe_err("sta ds is null");
		return;
	}

	qdf_mem_copy(&sta_ds->mlo_info, mlo_info, sizeof(sta_ds->mlo_info));
}

QDF_STATUS lim_fill_complete_mlo_ie(struct pe_session *session,
				    uint16_t total_len, uint8_t *target)
{
	struct wlan_mlo_sta_profile *sta_prof;
	uint16_t mlo_ie_total_len;
	uint8_t *buf, *pbuf;
	uint16_t i;
	uint16_t consumed = 0;
	uint16_t index = 0;
	struct wlan_mlo_ie *mlo_ie;

	if (!session)
		return QDF_STATUS_E_INVAL;

	mlo_ie = &session->mlo_ie;
	if (total_len > WLAN_MAX_IE_LEN + MIN_IE_LEN)
		mlo_ie->data[TAG_LEN_POS] = WLAN_MAX_IE_LEN;
	else
		mlo_ie->data[TAG_LEN_POS] = total_len - MIN_IE_LEN;

	buf = qdf_mem_malloc(total_len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	pbuf = buf;
	qdf_mem_copy(pbuf, mlo_ie->data, mlo_ie->num_data);
	pbuf += mlo_ie->num_data;

	for (i = 0; i < mlo_ie->num_sta_profile; i++) {
		sta_prof = &mlo_ie->sta_profile[i];
		qdf_mem_copy(pbuf, sta_prof->data, sta_prof->num_data);
		pbuf += sta_prof->num_data;
	}

	target[consumed++] = buf[index++];
	target[consumed++] = buf[index++];
	mlo_ie_total_len = pbuf - buf - MIN_IE_LEN;
	if (mlo_ie_total_len > total_len - MIN_IE_LEN) {
		pe_err("Invalid len: %u, %u", mlo_ie_total_len, total_len);
		qdf_mem_free(buf);
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < mlo_ie_total_len; i++) {
		if (i && (i % WLAN_MAX_IE_LEN) == 0) {
			/* add fragmentation IE and length */
			target[consumed++] = WLAN_ELEMID_FRAGMENT;
			if ((mlo_ie_total_len - i) > WLAN_MAX_IE_LEN)
				target[consumed++] = WLAN_MAX_IE_LEN;
			else
				target[consumed++] = mlo_ie_total_len - i;
		}
		target[consumed++] = buf[index++];
	}
	qdf_mem_free(buf);
	pe_debug("pack mlo ie %d bytes, expected to copy %d bytes",
		 consumed, total_len);
	qdf_trace_hex_dump(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   target, consumed);

	return QDF_STATUS_SUCCESS;
}

uint16_t lim_caculate_mlo_ie_length(struct wlan_mlo_ie *mlo_ie)
{
	struct wlan_mlo_sta_profile *sta_prof;
	uint16_t total_len;
	uint16_t i, tmp;

	total_len = mlo_ie->num_data;
	for (i = 0; i < mlo_ie->num_sta_profile; i++) {
		sta_prof = &mlo_ie->sta_profile[i];
		total_len += sta_prof->num_data;
	}

	if (total_len > WLAN_MAX_IE_LEN + MIN_IE_LEN) {
		/* ML IE max length  WLAN_MAX_IE_LEN + MIN_IE_LEN */
		tmp = total_len - (WLAN_MAX_IE_LEN + MIN_IE_LEN);
		while (tmp > WLAN_MAX_IE_LEN) {
			/* add one flagmentation IE */
			total_len += MIN_IE_LEN;
			tmp -= WLAN_MAX_IE_LEN;
		}
		/* add one flagmentation IE */
		total_len += MIN_IE_LEN;
	}
	return total_len;
}

QDF_STATUS lim_store_mlo_ie_raw_info(uint8_t *ie, uint8_t *sta_prof_ie,
				     uint32_t total_len,
				     struct wlan_mlo_ie *mlo_ie)
{
	uint32_t i, frag_num = 0, sta_index;
	/* ml_ie_len = total_len - 2 * frag_num, does not include
	 * WLAN_ELEMID_FRAGMENT IE and LEN
	 */
	uint32_t ml_ie_len;
	uint32_t index, copied;
	uint8_t *pfrm;
	uint8_t *buf;
	struct wlan_mlo_sta_profile *sta_prof;
	uint8_t *sta_data;
	/* Per STA profile frag or not */
	bool frag = FALSE;

	if (!ie)
		return QDF_STATUS_E_INVAL;

	qdf_mem_zero(mlo_ie, sizeof(*mlo_ie));

	/* assume element ID + LEN + extension element ID + multi-link control +
	 * common info length always less than WLAN_MAX_IE_LEN
	 */
	mlo_ie->num_data = sta_prof_ie - ie;
	if (mlo_ie->num_data > WLAN_MLO_IE_COM_MAX_LEN) {
		mlo_ie->num_data = 0;
		return QDF_STATUS_E_INVAL;
	}
	qdf_mem_copy(mlo_ie->data, ie, mlo_ie->num_data);

	/* Count how many frag IE */
	pfrm = ie;
	ml_ie_len = pfrm[TAG_LEN_POS] + MIN_IE_LEN;
	while (ml_ie_len < total_len) {
		frag_num++;
		pfrm += MIN_IE_LEN + pfrm[TAG_LEN_POS];
		ml_ie_len += pfrm[TAG_LEN_POS] + MIN_IE_LEN;
	}
	ml_ie_len = total_len - frag_num * MIN_IE_LEN;

	pe_debug_rl("ml_ie_len: %d, total_len: %d, frag_num: %d", ml_ie_len,
		    total_len, frag_num);

	buf = qdf_mem_malloc(total_len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	/* Copy the raw info and skip frag IE */
	index = 0;
	copied = 0;
	buf[index++] = ie[copied++];
	buf[index++] = ie[copied++];
	for (i = 0; i < ml_ie_len - MIN_IE_LEN; i++) {
		/* skip the frag IE */
		if (i && (i % WLAN_MAX_IE_LEN) == 0)
			copied += MIN_IE_LEN;
		buf[index++] = ie[copied++];
	}

	/* copy sta profile from buf, it has copied the common info */
	sta_index = 0;
	copied = mlo_ie->num_data;
	pfrm = buf + copied;
	while (copied < ml_ie_len && sta_index < WLAN_MLO_MAX_VDEVS &&
	       pfrm[ID_POS] == WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE) {
		sta_prof = &mlo_ie->sta_profile[sta_index];
		sta_data = sta_prof->data;
		index = 0;

		sta_data[index++] = buf[copied++];
		sta_data[index++] = buf[copied++];
		do {
			if (index + pfrm[TAG_LEN_POS] >
						WLAN_STA_PROFILE_MAX_LEN) {
				qdf_mem_free(buf);
				pe_debug("no enough buf to store sta prof");
				return QDF_STATUS_E_INVAL;
			}

			for (i = 0; i < pfrm[TAG_LEN_POS]; i++) {
				if (copied > ml_ie_len) {
					pe_debug("Buf length exceeded, copied %d ml_ie_len %d",
						 copied, ml_ie_len);
					qdf_mem_free(buf);
					return QDF_STATUS_E_INVAL;
				}
				sta_data[index++] = buf[copied++];
			}
			sta_prof->num_data = index;

			if (copied < ml_ie_len &&
			    pfrm[TAG_LEN_POS] == WLAN_MAX_IE_LEN &&
			    pfrm[WLAN_MAX_IE_LEN + MIN_IE_LEN] ==
					WLAN_ML_LINFO_SUBELEMID_FRAGMENT) {
				frag = TRUE;
				/* skip sta profile frag IE */
				copied += MIN_IE_LEN;
			} else {
				frag = FALSE;
			}
			pfrm += pfrm[TAG_LEN_POS] + MIN_IE_LEN;
		} while (frag);
		pe_debug_rl("sta index: %d, sta_data len: %d, copied: %d",
			    sta_index, index, copied);
		sta_index++;
	}

	mlo_ie->num_sta_profile = sta_index;
	qdf_mem_free(buf);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_add_frag_ie_for_sta_profile(uint8_t *data, uint16_t *len)
{
	uint16_t total_len;
	uint16_t tmp, i;
	uint8_t *buf;
	uint16_t consumed = 0;
	uint16_t index = 0;

	total_len = *len;
	buf = qdf_mem_malloc(total_len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(buf, data, total_len);

	if (total_len > WLAN_MAX_IE_LEN + MIN_IE_LEN) {
		/* ML IE max length  WLAN_MAX_IE_LEN + MIN_IE_LEN */
		tmp = total_len - (WLAN_MAX_IE_LEN + MIN_IE_LEN);
		while (tmp > WLAN_MAX_IE_LEN) {
			/* add one flagmentation IE */
			total_len += MIN_IE_LEN;
			tmp -= WLAN_MAX_IE_LEN;
		}
		/* add one flagmentation IE */
		total_len += MIN_IE_LEN;
	}

	data[consumed++] = buf[index++];
	data[consumed++] = buf[index++];
	for (i = 0; i < (*len - MIN_IE_LEN); i++) {
		data[consumed++] = buf[index++];
		if (i && (i % WLAN_MAX_IE_LEN) == 0) {
			data[consumed++] = WLAN_ML_LINFO_SUBELEMID_FRAGMENT;
			if ((*len - MIN_IE_LEN - i) > WLAN_MAX_IE_LEN)
				data[consumed++] = WLAN_MAX_IE_LEN;
			else
				data[consumed++] = *len - MIN_IE_LEN - i;
		}
	}

	*len = total_len;
	qdf_mem_free(buf);

	return QDF_STATUS_SUCCESS;
}

uint16_t
lim_fill_assoc_req_mlo_ie(struct mac_context *mac_ctx,
			  struct pe_session *session,
			  tDot11fAssocRequest *frm)
{
	QDF_STATUS status;

	session->mlo_ie_total_len = 0;
	qdf_mem_zero(&session->mlo_ie, sizeof(session->mlo_ie));
	if ((wlan_vdev_mlme_get_opmode(session->vdev) == QDF_STA_MODE) &&
	    wlan_vdev_mlme_is_mlo_vdev(session->vdev)) {
		status =
			populate_dot11f_assoc_req_mlo_ie(mac_ctx, session, frm);
		if (QDF_IS_STATUS_SUCCESS(status))
			session->mlo_ie_total_len =
				lim_caculate_mlo_ie_length(&session->mlo_ie);
	}

	return session->mlo_ie_total_len;
}

uint16_t
lim_send_assoc_rsp_mgmt_frame_mlo(struct mac_context *mac_ctx,
				  struct pe_session *session,
				  tpDphHashNode sta,
				  tDot11fAssocResponse *frm)
{
	QDF_STATUS status;

	session->mlo_ie_total_len = 0;
	qdf_mem_zero(&session->mlo_ie, sizeof(session->mlo_ie));
	status = populate_dot11f_assoc_rsp_mlo_ie(mac_ctx, session, sta, frm);
	if (QDF_IS_STATUS_SUCCESS(status))
		session->mlo_ie_total_len =
				lim_caculate_mlo_ie_length(&session->mlo_ie);

	return session->mlo_ie_total_len;
}

uint16_t
lim_send_bcn_frame_mlo(struct mac_context *mac_ctx,
		       struct pe_session *session)
{
	QDF_STATUS status;

	session->mlo_ie_total_len = 0;
	qdf_mem_zero(&session->mlo_ie, sizeof(session->mlo_ie));
	status = populate_dot11f_bcn_mlo_ie(mac_ctx, session);
	if (QDF_IS_STATUS_SUCCESS(status))
		session->mlo_ie_total_len =
				lim_caculate_mlo_ie_length(&session->mlo_ie);

	return session->mlo_ie_total_len;
}

uint16_t
lim_send_probe_req_frame_mlo(struct mac_context *mac_ctx,
			     struct pe_session *session)
{
	QDF_STATUS status;

	session->mlo_ie_total_len = 0;
	qdf_mem_zero(&session->mlo_ie, sizeof(session->mlo_ie));
	status = populate_dot11f_probe_req_mlo_ie(mac_ctx, session);
	if (QDF_IS_STATUS_SUCCESS(status))
		session->mlo_ie_total_len =
				lim_caculate_mlo_ie_length(&session->mlo_ie);

	return session->mlo_ie_total_len;
}

uint16_t
lim_send_tdls_mgmt_frame_mlo(struct mac_context *mac_ctx,
			     struct pe_session *session)
{
	QDF_STATUS status;

	session->mlo_ie_total_len = 0;
	qdf_mem_zero(&session->mlo_ie, sizeof(session->mlo_ie));
	status = populate_dot11f_tdls_mgmt_mlo_ie(mac_ctx, session);
	if (QDF_IS_STATUS_SUCCESS(status))
		session->mlo_ie_total_len =
				lim_caculate_mlo_ie_length(&session->mlo_ie);

	return session->mlo_ie_total_len;
}

uint16_t
lim_get_frame_mlo_ie_len(struct pe_session *session)
{
	if (session)
		return session->mlo_ie_total_len;
	else
		return 0;
}

bool
lim_is_ml_peer_state_disconn(struct mac_context *mac_ctx,
			     struct pe_session *session,
			     uint8_t *mac_addr)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_mlo_peer_context *ml_peer = NULL;
	bool is_ml_peer_disconn = false;

	peer = wlan_objmgr_get_peer_by_mac(mac_ctx->psoc, mac_addr,
					   WLAN_LEGACY_MAC_ID);

	if (!peer) {
		pe_err("peer is NULL");
		return is_ml_peer_disconn;
	}

	if ((session->opmode == QDF_STA_MODE) &&
	     wlan_vdev_mlme_is_mlo_vdev(session->vdev))
		ml_peer = peer->mlo_peer_ctx;

	if (!ml_peer) {
		pe_err("ML peer ctx not found");
		goto end;
	}

	if (QDF_IS_STATUS_SUCCESS(wlan_mlo_peer_is_disconnect_progress(ml_peer)))
		is_ml_peer_disconn = true;

end:
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
	return is_ml_peer_disconn;
}

bool lim_is_emlsr_band_supported(struct pe_session *session)
{
	uint8_t i;
	uint32_t freq;
	struct mlo_partner_info *partner_info;

	if (!session->lim_join_req) {
		/* Initial connection */
		partner_info = &session->ml_partner_info;
	} else {
		/* Roaming */
		partner_info = &session->lim_join_req->partner_info;
	}

	if (wlan_reg_is_24ghz_ch_freq(session->curr_op_freq))
		return false;

	for (i = 0; i < partner_info->num_partner_links; i++) {
		freq = partner_info->partner_link_info[i].chan_freq;
		if (wlan_reg_is_24ghz_ch_freq(freq))
			return false;
	}

	return true;
}
