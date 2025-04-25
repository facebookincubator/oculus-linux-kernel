/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains interface prototypes for son api
 */

#include <son_api.h>
#include <wlan_reg_services_api.h>
#include <wlan_mlme_api.h>
#include <ieee80211_external.h>
#include <wlan_cfg80211_scan.h>
#include <wlan_mlme_main.h>

/**
 * struct son_mlme_deliver_cbs - son mlme deliver callbacks
 * @deliver_opmode: cb to deliver opmode
 * @deliver_smps: cb to deliver smps
 */
struct son_mlme_deliver_cbs {
	mlme_deliver_cb deliver_opmode;
	mlme_deliver_cb deliver_smps;
};

static struct son_mlme_deliver_cbs g_son_mlme_deliver_cbs;

static struct son_cbs *g_son_cbs[WLAN_MAX_VDEVS];
static qdf_spinlock_t g_cbs_lock;

QDF_STATUS
wlan_son_register_mlme_deliver_cb(struct wlan_objmgr_psoc *psoc,
				  mlme_deliver_cb cb,
				  enum SON_MLME_DELIVER_CB_TYPE type)
{
	if (!psoc) {
		son_err("invalid psoc");
		return QDF_STATUS_E_INVAL;
	}

	switch (type) {
	case SON_MLME_DELIVER_CB_TYPE_OPMODE:
		g_son_mlme_deliver_cbs.deliver_opmode = cb;
		break;
	case SON_MLME_DELIVER_CB_TYPE_SMPS:
		g_son_mlme_deliver_cbs.deliver_smps = cb;
		break;
	default:
		son_err("invalid type");
		break;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_son_is_he_supported() - is he supported or not
 * @psoc: pointer to psoc
 *
 * Return: true if supports, false otherwise
 */
#ifdef WLAN_FEATURE_11AX
static bool wlan_son_is_he_supported(struct wlan_objmgr_psoc *psoc)
{
	tDot11fIEhe_cap he_cap = {0};

	mlme_cfg_get_he_caps(psoc, &he_cap);
	return !!he_cap.present;
}
#else
static bool wlan_son_is_he_supported(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif /*WLAN_FEATURE_11AX*/

QDF_STATUS wlan_son_peer_ext_stat_enable(struct wlan_objmgr_pdev *pdev,
					 uint8_t *mac_addr,
					 struct wlan_objmgr_vdev *vdev,
					 uint32_t stats_count,
					 uint32_t enable)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		son_err("invalid pdev");
		return QDF_STATUS_E_NULL_VALUE;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		son_err("invalid psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}
	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		son_err("invalid tx_ops");
		return QDF_STATUS_E_NULL_VALUE;
	}
	if (tx_ops->son_tx_ops.peer_ext_stats_enable)
		return tx_ops->son_tx_ops.peer_ext_stats_enable(pdev,
								mac_addr, vdev,
								stats_count,
								enable);

	return QDF_STATUS_E_NULL_VALUE;
}

QDF_STATUS wlan_son_peer_req_inst_stats(struct wlan_objmgr_pdev *pdev,
					uint8_t *mac_addr,
					struct wlan_objmgr_vdev *vdev)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		son_err("invalid pdev");
		return QDF_STATUS_E_NULL_VALUE;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		son_err("invalid psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}
	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		son_err("invalid tx_ops");
		return QDF_STATUS_E_NULL_VALUE;
	}
	if (tx_ops->son_tx_ops.son_send_null)
		return tx_ops->son_tx_ops.son_send_null(pdev, mac_addr, vdev);

	return QDF_STATUS_E_NULL_VALUE;
}

uint32_t wlan_son_get_chan_flag(struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq, bool flag_160,
				struct ch_params *chan_params)
{
	uint32_t flags = 0;
	qdf_freq_t sec_freq;
	struct ch_params ch_width40_ch_params;
	uint8_t sub_20_channel_width = 0;
	enum phy_ch_width bandwidth = mlme_get_vht_ch_width();
	struct wlan_objmgr_psoc *psoc;
	bool is_he_enabled;
	struct ch_params ch_params;

	if (!pdev) {
		son_err("invalid pdev");
		return flags;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		son_err("invalid psoc");
		return flags;
	}

	is_he_enabled = wlan_son_is_he_supported(psoc);
	wlan_mlme_get_sub_20_chan_width(wlan_pdev_get_psoc(pdev),
					&sub_20_channel_width);

	qdf_mem_zero(chan_params, sizeof(*chan_params));
	qdf_mem_zero(&ch_params, sizeof(ch_params));
	qdf_mem_zero(&ch_width40_ch_params, sizeof(ch_width40_ch_params));
	if (wlan_reg_is_24ghz_ch_freq(freq)) {
		if (bandwidth == CH_WIDTH_80P80MHZ ||
		    bandwidth == CH_WIDTH_160MHZ ||
		    bandwidth == CH_WIDTH_80MHZ)
			bandwidth = CH_WIDTH_40MHZ;
	}

	ch_params.ch_width = bandwidth;
	switch (bandwidth) {
	case CH_WIDTH_80P80MHZ:
		ch_params.ch_width = CH_WIDTH_80P80MHZ;
		if (wlan_reg_get_5g_bonded_channel_state_for_pwrmode(
					pdev, freq,
					&ch_params, REG_CURRENT_PWR_MODE) !=
		    CHANNEL_STATE_INVALID) {
			if (!flag_160) {
				chan_params->ch_width = CH_WIDTH_80P80MHZ;
				wlan_reg_set_channel_params_for_pwrmode(
					pdev, freq, 0, chan_params,
					REG_CURRENT_PWR_MODE);
			}
			if (is_he_enabled)
				flags |= VENDOR_CHAN_FLAG2(
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE80_80);
			flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT80_80;
		}
		bandwidth = CH_WIDTH_160MHZ;
		fallthrough;
	case CH_WIDTH_160MHZ:
		ch_params.ch_width = CH_WIDTH_160MHZ;
		if (wlan_reg_get_5g_bonded_channel_state_for_pwrmode(
					pdev, freq,
					&ch_params, REG_CURRENT_PWR_MODE) !=
		    CHANNEL_STATE_INVALID) {
			if (flag_160) {
				chan_params->ch_width = CH_WIDTH_160MHZ;
				wlan_reg_set_channel_params_for_pwrmode(
					pdev, freq, 0, chan_params,
					REG_CURRENT_PWR_MODE);
			}
			if (is_he_enabled)
				flags |= VENDOR_CHAN_FLAG2(
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE160);
			flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT160;
		}
		bandwidth = CH_WIDTH_80MHZ;
		fallthrough;
	case CH_WIDTH_80MHZ:
		ch_params.ch_width = CH_WIDTH_80MHZ;
		if (wlan_reg_get_5g_bonded_channel_state_for_pwrmode(
					pdev, freq,
					&ch_params, REG_CURRENT_PWR_MODE) !=
		    CHANNEL_STATE_INVALID) {
			if (!flag_160 &&
			    chan_params->ch_width != CH_WIDTH_80P80MHZ) {
				chan_params->ch_width = CH_WIDTH_80MHZ;
				wlan_reg_set_channel_params_for_pwrmode(
					pdev, freq, 0, chan_params,
					REG_CURRENT_PWR_MODE);
			}
			if (is_he_enabled)
				flags |= VENDOR_CHAN_FLAG2(
					QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE80);
			flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT80;
		}
		bandwidth = CH_WIDTH_40MHZ;
		fallthrough;
	case CH_WIDTH_40MHZ:
		ch_width40_ch_params.ch_width = bandwidth;
		wlan_reg_set_channel_params_for_pwrmode(pdev, freq, 0,
							&ch_width40_ch_params,
							REG_CURRENT_PWR_MODE);

		if (ch_width40_ch_params.sec_ch_offset == LOW_PRIMARY_CH)
			sec_freq = freq + 20;
		else if (ch_width40_ch_params.sec_ch_offset == HIGH_PRIMARY_CH)
			sec_freq = freq - 20;
		else
			sec_freq = 0;

		if (wlan_reg_get_bonded_channel_state_for_pwrmode(
							pdev, freq,
							bandwidth, sec_freq,
							REG_CURRENT_PWR_MODE) !=
		    CHANNEL_STATE_INVALID) {
			if (ch_width40_ch_params.sec_ch_offset ==
			    LOW_PRIMARY_CH) {
				if (is_he_enabled)
				  flags |=
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE40PLUS;
				flags |=
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT40PLUS;
				flags |=
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT40PLUS;
			} else if (ch_width40_ch_params.sec_ch_offset ==
				   HIGH_PRIMARY_CH) {
				if (is_he_enabled)
				  flags |=
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE40MINUS;
				flags |=
				   QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT40MINUS;
				flags |=
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT40PLUS;
			}
		}
		bandwidth = CH_WIDTH_20MHZ;
		fallthrough;
	case CH_WIDTH_20MHZ:
		flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HT20;
		flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_VHT20;
		if (is_he_enabled)
			flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HE20;
		bandwidth = CH_WIDTH_10MHZ;
		fallthrough;
	case CH_WIDTH_10MHZ:
		if (wlan_reg_get_bonded_channel_state_for_pwrmode(
							pdev, freq,
							bandwidth, 0,
							REG_CURRENT_PWR_MODE) !=
		     CHANNEL_STATE_INVALID &&
		     sub_20_channel_width == WLAN_SUB_20_CH_WIDTH_10)
			flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HALF;
		bandwidth = CH_WIDTH_5MHZ;
		fallthrough;
	case CH_WIDTH_5MHZ:
		if (wlan_reg_get_bonded_channel_state_for_pwrmode(
							pdev, freq,
							bandwidth, 0,
							REG_CURRENT_PWR_MODE) !=
		    CHANNEL_STATE_INVALID &&
		    sub_20_channel_width == WLAN_SUB_20_CH_WIDTH_5)
			flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_QUARTER;
		break;
	default:
		son_info("invalid channel width value %d", bandwidth);
	}

	return flags;
}

QDF_STATUS wlan_son_peer_set_kickout_allow(struct wlan_objmgr_vdev *vdev,
					   struct wlan_objmgr_peer *peer,
					   bool kickout_allow)
{
	struct peer_mlme_priv_obj *peer_priv;

	if (!peer) {
		son_err("invalid peer");
		return QDF_STATUS_E_INVAL;
	}
	if (!vdev) {
		son_err("invalid vdev");
		return QDF_STATUS_E_INVAL;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		son_err("invalid vdev");
		return QDF_STATUS_E_INVAL;
	}

	peer_priv->allow_kickout = kickout_allow;

	return QDF_STATUS_SUCCESS;
}

bool wlan_son_peer_is_kickout_allow(struct wlan_objmgr_vdev *vdev,
				    uint8_t *macaddr)
{
	bool kickout_allow = true;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc;
	struct peer_mlme_priv_obj *peer_priv;

	if (!vdev) {
		son_err("invalid vdev");
		return kickout_allow;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		son_err("invalid psoc");
		return kickout_allow;
	}
	peer = wlan_objmgr_get_peer_by_mac(psoc, macaddr,
					   WLAN_SON_ID);

	if (!peer) {
		son_err("peer is null");
		return kickout_allow;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		son_err("invalid vdev");
		wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
		return kickout_allow;
	}
	kickout_allow = peer_priv->allow_kickout;
	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return kickout_allow;
}

void wlan_son_ind_assoc_req_frm(struct wlan_objmgr_vdev *vdev,
				uint8_t *macaddr, bool is_reassoc,
				uint8_t *frame, uint16_t frame_len,
				QDF_STATUS status)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_lmac_if_rx_ops *rx_ops;
	struct wlan_objmgr_psoc *psoc;
	uint16_t assocstatus = STATUS_UNSPECIFIED_FAILURE;
	uint16_t sub_type = IEEE80211_FC0_SUBTYPE_ASSOC_REQ;

	if (!vdev) {
		son_err("invalid vdev");
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		son_err("invalid psoc");
		return;
	}
	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops || !rx_ops->son_rx_ops.process_mgmt_frame) {
		son_err("invalid rx ops");
		return;
	}
	peer = wlan_objmgr_get_peer_by_mac(psoc, macaddr,
					   WLAN_SON_ID);
	if (!peer) {
		son_err("peer is null");
		return;
	}

	if (is_reassoc)
		sub_type = IEEE80211_FC0_SUBTYPE_REASSOC_REQ;
	if (QDF_IS_STATUS_SUCCESS(status))
		assocstatus = STATUS_SUCCESS;
	son_debug("subtype %u frame_len %u assocstatus %u",
		  sub_type, frame_len, assocstatus);
	rx_ops->son_rx_ops.process_mgmt_frame(vdev, peer, sub_type,
					      frame, frame_len,
					      &assocstatus);
	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
}

static int wlan_son_deliver_mlme_event(struct wlan_objmgr_vdev *vdev,
				       struct wlan_objmgr_peer *peer,
				       uint32_t event,
				       void *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_rx_ops *rx_ops;
	int ret;

	if (!vdev)
		return -EINVAL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return -EINVAL;

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (rx_ops && rx_ops->son_rx_ops.deliver_event) {
		son_debug("deliver mlme event %d", event);
		ret = rx_ops->son_rx_ops.deliver_event(vdev,
						       peer,
						       event,
						       event_data);
	} else {
		return -EINVAL;
	}

	return ret;
}

int wlan_son_deliver_tx_power(struct wlan_objmgr_vdev *vdev,
			      int32_t max_pwr)
{
	int ret;

	son_debug("tx power %d", max_pwr);
	ret = wlan_son_deliver_mlme_event(vdev,
					  NULL,
					  MLME_EVENT_TX_PWR_CHANGE,
					  &max_pwr);

	return ret;
}

int wlan_son_deliver_vdev_stop(struct wlan_objmgr_vdev *vdev)
{
	int ret;

	struct wlan_vdev_state_event event;

	event.state = VDEV_STATE_STOPPED;
	son_debug("state %d", event.state);
	ret = wlan_son_deliver_mlme_event(vdev,
					  NULL,
					  MLME_EVENT_VDEV_STATE,
					  &event);

	return ret;
}

int wlan_son_deliver_inst_rssi(struct wlan_objmgr_vdev *vdev,
			       struct wlan_objmgr_peer *peer,
			       uint32_t irssi)
{
	struct wlan_peer_inst_rssi event;
	int ret;

	if (irssi > 0 && irssi <= 127) {
		event.iRSSI = irssi;
		event.valid = true;
		son_debug("irssi %d", event.iRSSI);
	} else {
		event.valid = false;
		son_debug("irssi invalid");
	}

	ret = wlan_son_deliver_mlme_event(vdev,
					  peer,
					  MLME_EVENT_INST_RSSI,
					  &event);

	return ret;
}

int wlan_son_deliver_opmode(struct wlan_objmgr_vdev *vdev,
			    uint8_t bw,
			    uint8_t nss,
			    uint8_t *addr)
{
	struct wlan_objmgr_psoc *psoc;
	struct ieee80211_opmode_update_data opmode;

	if (!vdev)
		return -EINVAL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return -EINVAL;

	opmode.max_chwidth = bw;
	opmode.num_streams = nss;
	qdf_mem_copy(opmode.macaddr, addr, QDF_MAC_ADDR_SIZE);

	son_debug("bw %d, nss %d, addr " QDF_MAC_ADDR_FMT,
		  bw, nss, QDF_MAC_ADDR_REF(addr));

	if (!g_son_mlme_deliver_cbs.deliver_opmode) {
		son_err("invalid deliver opmode cb");
		return -EINVAL;
	}

	g_son_mlme_deliver_cbs.deliver_opmode(vdev,
					      sizeof(opmode),
					      (uint8_t *)&opmode);

	return 0;
}

int wlan_son_deliver_smps(struct wlan_objmgr_vdev *vdev,
			  uint8_t is_static,
			  uint8_t *addr)
{
	struct wlan_objmgr_psoc *psoc;
	struct ieee80211_smps_update_data smps;

	if (!vdev)
		return -EINVAL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return -EINVAL;

	smps.is_static = is_static;
	qdf_mem_copy(smps.macaddr, addr, QDF_MAC_ADDR_SIZE);

	son_debug("is_static %d, addr" QDF_MAC_ADDR_FMT,
		  is_static, QDF_MAC_ADDR_REF(addr));

	if (!g_son_mlme_deliver_cbs.deliver_smps) {
		son_err("invalid deliver smps cb");
		return -EINVAL;
	}

	g_son_mlme_deliver_cbs.deliver_smps(vdev,
					    sizeof(smps),
					    (uint8_t *)&smps);

	return 0;
}

int wlan_son_deliver_rrm_rpt(struct wlan_objmgr_vdev *vdev,
			     uint8_t *mac_addr,
			     uint8_t *frm,
			     uint32_t flen)
{
	struct wlan_act_frm_info rrm_info;
	struct wlan_lmac_if_rx_ops *rx_ops;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer;
	uint8_t sub_type = IEEE80211_FC0_SUBTYPE_ACTION;
	struct ieee80211_action ia;
	const uint8_t *ie, *pos, *end;
	uint8_t total_bcnrpt_count = 0;

	if (!vdev) {
		son_err("invalid vdev");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		son_err("invalid psoc");
		return -EINVAL;
	}

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops || !rx_ops->son_rx_ops.process_mgmt_frame) {
		son_err("invalid rx ops");
		return -EINVAL;
	}

	peer = wlan_objmgr_get_peer_by_mac(psoc, mac_addr, WLAN_SON_ID);
	if (!peer) {
		son_err("peer is null");
		return -EINVAL;
	}

	ia.ia_category = ACTION_CATEGORY_RRM;
	ia.ia_action = RRM_RADIO_MEASURE_RPT;
	qdf_mem_zero(&rrm_info, sizeof(rrm_info));
	rrm_info.ia = &ia;
	rrm_info.ald_info = 0;
	qdf_mem_copy(rrm_info.data.rrm_data.macaddr,
		     mac_addr,
		     QDF_MAC_ADDR_SIZE);
	/* IEEE80211_ACTION_RM_TOKEN */
	rrm_info.data.rrm_data.dialog_token = *frm;

	/* Points to Measurement Report Element */
	++frm;
	--flen;
	pos = frm;
	end = pos + flen;

	while ((ie = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_MEASREP,
					      pos, end - pos))) {
		if (ie[1] < 3) {
			son_err("Bad Measurement Report element");
			wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
			return -EINVAL;
		}
		if (ie[4] == SIR_MAC_RRM_BEACON_TYPE)
			++total_bcnrpt_count;
		pos = ie + ie[1] + 2;
	}

	rrm_info.data.rrm_data.num_meas_rpts = total_bcnrpt_count;

	son_debug("Sta: " QDF_MAC_ADDR_FMT
		  "Category %d Action %d Num_Report %d Rptlen %d",
		  QDF_MAC_ADDR_REF(mac_addr),
		  ACTION_CATEGORY_RRM,
		  RRM_RADIO_MEASURE_RPT,
		  total_bcnrpt_count,
		  flen);

	rx_ops->son_rx_ops.process_mgmt_frame(vdev, peer, sub_type,
					      frm, flen, &rrm_info);

	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return 0;
}

int wlan_son_anqp_frame(struct wlan_objmgr_vdev *vdev, int subtype,
			uint8_t *frame, uint16_t frame_len, void *action_hdr,
			uint8_t *macaddr)
{
	struct son_act_frm_info info;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_rx_ops *rx_ops;
	int ret;

	if (!vdev)
		return -EINVAL;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return -EINVAL;

	qdf_mem_zero(&info, sizeof(info));
	info.ia = (struct ieee80211_action *)action_hdr;
	info.ald_info = 1;
	qdf_mem_copy(info.data.macaddr, macaddr, sizeof(tSirMacAddr));

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (rx_ops && rx_ops->son_rx_ops.process_mgmt_frame)
		ret = rx_ops->son_rx_ops.process_mgmt_frame(vdev, NULL,
							    subtype, frame,
							    frame_len, &info);
	else
		return -EINVAL;
	return ret;
}

static int wlan_son_deliver_cbs(struct wlan_objmgr_vdev *vdev,
				wlan_cbs_event_type type)
{
	int ret;

	ret = wlan_son_deliver_mlme_event(vdev,
					  NULL,
					  MLME_EVENT_CBS_STATUS,
					  &type);

	return ret;
}

static int wlan_son_deliver_cbs_completed(struct wlan_objmgr_vdev *vdev)
{
	return wlan_son_deliver_cbs(vdev, CBS_COMPLETE);
}

static int wlan_son_deliver_cbs_cancelled(struct wlan_objmgr_vdev *vdev)
{
	return wlan_son_deliver_cbs(vdev, CBS_CANCELLED);
}

static void
wlan_son_cbs_set_state(struct son_cbs *cbs, enum son_cbs_state state)
{
	son_debug("Change State CBS OLD[%d] --> NEW[%d]",
		  cbs->cbs_state, state);
	cbs->cbs_state = state;
}

static enum
son_cbs_state wlan_son_cbs_get_state(struct son_cbs *cbs)
{
	return cbs->cbs_state;
}

static void
wlan_son_cbs_init_dwell_params(struct son_cbs *cbs,
			       int dwell_split_time,
			       int dwell_rest_time)
{
	int i;

	if (!cbs || !cbs->vdev)
		return;
	son_debug("dwell_split_time %d, dwell_rest_time %d",
		  dwell_split_time, dwell_rest_time);
	son_debug("vdev_id: %d\n", wlan_vdev_get_id(cbs->vdev));

	switch (dwell_split_time) {
	case CBS_DWELL_TIME_10MS:
		cbs->max_arr_size_used = 10;
		cbs->dwell_split_cnt = cbs->max_arr_size_used - 1;
		cbs->max_dwell_split_cnt = cbs->max_arr_size_used - 1;
		for (i = 0; i < cbs->max_arr_size_used; i++)
			cbs->scan_dwell_rest[i] = dwell_rest_time;
		for (i = 0; i < cbs->max_arr_size_used; i++)
			cbs->scan_offset[i] = i * dwell_split_time;
		break;
	case CBS_DWELL_TIME_25MS:
		cbs->max_arr_size_used = 8;
		cbs->dwell_split_cnt = cbs->max_arr_size_used - 1;
		cbs->max_dwell_split_cnt = cbs->max_arr_size_used - 1;
		if (dwell_rest_time % TOTAL_DWELL_TIME == 0) {
			cbs->scan_dwell_rest[0] = dwell_rest_time;
			cbs->scan_dwell_rest[1] = dwell_rest_time;
			cbs->scan_dwell_rest[2] = dwell_rest_time;
			cbs->scan_dwell_rest[3] = dwell_rest_time;
			cbs->scan_dwell_rest[4] = dwell_rest_time +
							TOTAL_DWELL_TIME -
							DEFAULT_BEACON_INTERVAL;
			cbs->scan_dwell_rest[5] = dwell_rest_time +
							TOTAL_DWELL_TIME -
							DEFAULT_BEACON_INTERVAL;
			cbs->scan_dwell_rest[6] = dwell_rest_time;
			cbs->scan_dwell_rest[7] = dwell_rest_time;
			cbs->scan_dwell_rest[8] = 0;
			cbs->scan_dwell_rest[9] = 0;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = 0;
			cbs->scan_offset[2] = dwell_split_time;
			cbs->scan_offset[3] = dwell_split_time;
			cbs->scan_offset[4] = 2 * dwell_split_time;
			cbs->scan_offset[5] = 2 * dwell_split_time;
			cbs->scan_offset[6] = 3 * dwell_split_time;
			cbs->scan_offset[7] = 3 * dwell_split_time;
			cbs->scan_offset[8] = 0;
			cbs->scan_offset[9] = 0;
		} else {
			for (i = 0; i < cbs->max_arr_size_used - 1; i++)
				cbs->scan_dwell_rest[i] = dwell_rest_time;

			cbs->scan_dwell_rest[8] = 0;
			cbs->scan_dwell_rest[9] = 0;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = dwell_split_time;
			cbs->scan_offset[2] = 2 * dwell_split_time;
			cbs->scan_offset[3] = 3 * dwell_split_time;
			cbs->scan_offset[4] = 0;
			cbs->scan_offset[5] = dwell_split_time;
			cbs->scan_offset[6] = 2 * dwell_split_time;
			cbs->scan_offset[7] = 3 * dwell_split_time;
			cbs->scan_offset[8] = 0;
			cbs->scan_offset[9] = 0;
		}
		break;
	case CBS_DWELL_TIME_50MS:
		cbs->max_arr_size_used = 4;
		cbs->dwell_split_cnt = cbs->max_arr_size_used - 1;
		cbs->max_dwell_split_cnt = cbs->max_arr_size_used - 1;
		if (dwell_rest_time % TOTAL_DWELL_TIME == 0) {
			cbs->scan_dwell_rest[0] = dwell_rest_time;
			cbs->scan_dwell_rest[1] = dwell_rest_time;
			cbs->scan_dwell_rest[2] = dwell_rest_time +
							TOTAL_DWELL_TIME -
							DEFAULT_BEACON_INTERVAL;
			cbs->scan_dwell_rest[3] = dwell_rest_time +
							TOTAL_DWELL_TIME -
							DEFAULT_BEACON_INTERVAL;
			cbs->scan_dwell_rest[4] = 0;
			cbs->scan_dwell_rest[5] = 0;
			cbs->scan_dwell_rest[6] = 0;
			cbs->scan_dwell_rest[7] = 0;
			cbs->scan_dwell_rest[8] = 0;
			cbs->scan_dwell_rest[9] = 0;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = 0;
			cbs->scan_offset[2] = dwell_split_time;
			cbs->scan_offset[3] = dwell_split_time;
			cbs->scan_offset[4] = 0;
			cbs->scan_offset[5] = 0;
			cbs->scan_offset[6] = 0;
			cbs->scan_offset[7] = 0;
			cbs->scan_offset[8] = 0;
			cbs->scan_offset[9] = 0;
		} else {
			cbs->scan_dwell_rest[0] = dwell_rest_time;
			cbs->scan_dwell_rest[1] = dwell_rest_time;
			cbs->scan_dwell_rest[2] = dwell_rest_time;
			cbs->scan_dwell_rest[3] = dwell_rest_time;
			cbs->scan_dwell_rest[4] = 0;
			cbs->scan_dwell_rest[5] = 0;
			cbs->scan_dwell_rest[6] = 0;
			cbs->scan_dwell_rest[7] = 0;
			cbs->scan_dwell_rest[8] = 0;
			cbs->scan_dwell_rest[9] = 0;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = dwell_split_time;
			cbs->scan_offset[2] = 0;
			cbs->scan_offset[3] = dwell_split_time;
			cbs->scan_offset[4] = 0;
			cbs->scan_offset[5] = 0;
			cbs->scan_offset[6] = 0;
			cbs->scan_offset[7] = 0;
			cbs->scan_offset[8] = 0;
			cbs->scan_offset[9] = 0;
		}
		break;
	case CBS_DWELL_TIME_75MS:
		cbs->max_arr_size_used = 4;
		cbs->dwell_split_cnt = cbs->max_arr_size_used - 1;
		cbs->max_dwell_split_cnt = cbs->max_arr_size_used - 1;
		if (dwell_rest_time % TOTAL_DWELL_TIME == 0) {
			cbs->scan_dwell_rest[0] = dwell_rest_time;
			cbs->scan_dwell_rest[1] = dwell_rest_time;
			cbs->scan_dwell_rest[2] = dwell_rest_time +
							TOTAL_DWELL_TIME -
							DEFAULT_BEACON_INTERVAL;
			cbs->scan_dwell_rest[3] = dwell_rest_time +
							TOTAL_DWELL_TIME -
							DEFAULT_BEACON_INTERVAL;
			cbs->scan_dwell_rest[4] = 0;
			cbs->scan_dwell_rest[5] = 0;
			cbs->scan_dwell_rest[6] = 0;
			cbs->scan_dwell_rest[7] = 0;
			cbs->scan_dwell_rest[8] = 0;
			cbs->scan_dwell_rest[9] = 0;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = 0;
			cbs->scan_offset[2] = DEFAULT_BEACON_INTERVAL -
							dwell_split_time;
			cbs->scan_offset[3] = DEFAULT_BEACON_INTERVAL -
							dwell_split_time;
			cbs->scan_offset[4] = 0;
			cbs->scan_offset[5] = 0;
			cbs->scan_offset[6] = 0;
			cbs->scan_offset[7] = 0;
			cbs->scan_offset[8] = 0;
			cbs->scan_offset[9] = 0;
		} else {
			cbs->scan_dwell_rest[0] = dwell_rest_time;
			cbs->scan_dwell_rest[1] = dwell_rest_time;
			cbs->scan_dwell_rest[2] = dwell_rest_time;
			cbs->scan_dwell_rest[3] = dwell_rest_time;
			cbs->scan_dwell_rest[4] = 0;
			cbs->scan_dwell_rest[5] = 0;
			cbs->scan_dwell_rest[6] = 0;
			cbs->scan_dwell_rest[7] = 0;
			cbs->scan_dwell_rest[8] = 0;
			cbs->scan_dwell_rest[9] = 0;
			cbs->scan_offset[0] = 0;
			cbs->scan_offset[1] = DEFAULT_BEACON_INTERVAL -
							dwell_split_time;
			cbs->scan_offset[2] = 0;
			cbs->scan_offset[3] = DEFAULT_BEACON_INTERVAL -
							dwell_split_time;
			cbs->scan_offset[4] = 0;
			cbs->scan_offset[5] = 0;
			cbs->scan_offset[6] = 0;
			cbs->scan_offset[7] = 0;
			cbs->scan_offset[8] = 0;
			cbs->scan_offset[9] = 0;
		}
		break;
	default:
		son_err("Dwell time not supported\n");
		break;
	}
}

static int wlan_son_cbs_start(struct son_cbs *cbs)
{
	struct scan_start_request *req;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	psoc = wlan_vdev_get_psoc(cbs->vdev);
	if (!psoc) {
		son_err("invalid psoc");
		return -EINVAL;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		son_err("failed to malloc");
		return -ENOMEM;
	}
	qdf_mem_copy(req, &cbs->scan_params, sizeof(*req));

	cbs->cbs_scan_id = ucfg_scan_get_scan_id(psoc);
	req->scan_req.scan_id = cbs->cbs_scan_id;
	son_debug("vdev_id: %d req->scan_req.scan_id: %u",
		  wlan_vdev_get_id(cbs->vdev), req->scan_req.scan_id);

	status = ucfg_scan_start(req);
	if (QDF_IS_STATUS_ERROR(status)) {
		son_err("failed to start cbs");
		wlan_son_deliver_cbs_cancelled(cbs->vdev);
		return -EINVAL;
	}

	son_debug("cbs start");

	return 0;
}

static int wlan_son_cbs_stop(struct son_cbs *cbs)
{
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(cbs->vdev);
	if (!pdev) {
		son_err("invalid pdev");
		return -EINVAL;
	}
	son_debug("vdev_id: %d", wlan_vdev_get_id(cbs->vdev));

	if (ucfg_scan_get_pdev_status(pdev) != SCAN_NOT_IN_PROGRESS) {
		son_info("cbs_scan_id: %u abort scan", cbs->cbs_scan_id);
		status = wlan_abort_scan(pdev,
					 wlan_objmgr_pdev_get_pdev_id(pdev),
					 cbs->vdev->vdev_objmgr.vdev_id,
					 cbs->cbs_scan_id,
					 true);
		if (QDF_IS_STATUS_ERROR(status)) {
			son_err("failed to abort cbs");
			return -EBUSY;
		}
	}

	return 0;
}

static void wlan_cbs_timer_handler(void *arg)
{
	struct son_cbs *cbs = (struct son_cbs *)arg;
	enum son_cbs_state state;

	state = wlan_son_cbs_get_state(cbs);
	son_debug("state: %d", state);
	if (state == CBS_REST) {
		son_debug("vdev_id: %d dwell_split_cnt: %d",
			  wlan_vdev_get_id(cbs->vdev),
			  cbs->dwell_split_cnt);
		qdf_spin_lock_bh(&g_cbs_lock);
		wlan_son_cbs_set_state(cbs, CBS_SCAN);
		cbs->dwell_split_cnt--;
		wlan_son_cbs_start(cbs);
		qdf_spin_unlock_bh(&g_cbs_lock);
	} else if (state == CBS_WAIT) {
		wlan_son_cbs_enable(cbs->vdev);
	}
}

static int wlan_cbs_iterate(struct son_cbs *cbs)
{
	int offset_array_idx;
	struct wlan_objmgr_psoc *psoc;

	qdf_spin_lock_bh(&g_cbs_lock);
	if (!cbs || !cbs->vdev) {
		qdf_spin_unlock_bh(&g_cbs_lock);
		return -EINVAL;
	}
	son_debug("dwell_split_cnt: %d", cbs->dwell_split_cnt);
	if (cbs->dwell_split_cnt < 0) {
		psoc = wlan_vdev_get_psoc(cbs->vdev);
		if (!psoc) {
			qdf_spin_unlock_bh(&g_cbs_lock);
			return -EINVAL;
		}
		wlan_son_deliver_cbs_completed(cbs->vdev);

		ucfg_scan_unregister_requester(psoc,
					       cbs->cbs_scan_requestor);
		son_debug("Unregister cbs_scan_requestor: %u",
			  cbs->cbs_scan_requestor);

		if (cbs->wait_time) {
			wlan_son_cbs_set_state(cbs, CBS_WAIT);
			qdf_timer_mod(&cbs->cbs_timer,
				      cbs->wait_time);
		} else {
			wlan_son_cbs_set_state(cbs, CBS_INIT);
		}
	} else {
		offset_array_idx = cbs->max_arr_size_used -
				   cbs->dwell_split_cnt - 1;
		if (offset_array_idx < MIN_SCAN_OFFSET_ARRAY_SIZE ||
		    offset_array_idx > MAX_SCAN_OFFSET_ARRAY_SIZE) {
			qdf_spin_unlock_bh(&g_cbs_lock);
			return -EINVAL;
		}
		if (cbs->scan_dwell_rest[offset_array_idx] == 0) {
			cbs->dwell_split_cnt--;
			wlan_son_cbs_start(cbs);
		} else {
			wlan_son_cbs_set_state(cbs, CBS_REST);
			qdf_timer_mod(&cbs->cbs_timer,
				      cbs->scan_dwell_rest[offset_array_idx]);
		}
	}
	qdf_spin_unlock_bh(&g_cbs_lock);

	return 0;
}

static void wlan_cbs_scan_event_cb(struct wlan_objmgr_vdev *vdev,
				   struct scan_event *event,
				   void *arg)
{
	son_debug("event type: %d", event->type);
	switch (event->type) {
	case SCAN_EVENT_TYPE_FOREIGN_CHANNEL:
	case SCAN_EVENT_TYPE_FOREIGN_CHANNEL_GET_NF:
		break;
	case SCAN_EVENT_TYPE_COMPLETED:
		wlan_cbs_iterate(arg);
		break;
	default:
		break;
	}
}

int wlan_son_cbs_init(void)
{
	int i, j;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		if (g_son_cbs[i]) {
			qdf_mem_free(g_son_cbs[i]);
			g_son_cbs[i] = NULL;
		}
		g_son_cbs[i] = qdf_mem_malloc(sizeof(*g_son_cbs[i]));
		if (!g_son_cbs[i]) {
			for (j = i - 1; j >= 0; j--) {
				qdf_mem_free(g_son_cbs[j]);
				g_son_cbs[i] = NULL;
			}
			return -ENOMEM;
		}
		qdf_timer_init(NULL,
			       &g_son_cbs[i]->cbs_timer,
			       wlan_cbs_timer_handler,
			       g_son_cbs[i],
			       QDF_TIMER_TYPE_WAKE_APPS);

		g_son_cbs[i]->rest_time  = CBS_DEFAULT_RESTTIME;
		g_son_cbs[i]->dwell_time = CBS_DEFAULT_DWELL_TIME;
		g_son_cbs[i]->wait_time  = CBS_DEFAULT_WAIT_TIME;
		g_son_cbs[i]->dwell_split_time = CBS_DEFAULT_DWELL_SPLIT_TIME;
		g_son_cbs[i]->min_dwell_rest_time = CBS_DEFAULT_DWELL_REST_TIME;

		wlan_son_cbs_set_state(g_son_cbs[i], CBS_INIT);
	}
	qdf_spinlock_create(&g_cbs_lock);
	son_debug("cbs init");

	return 0;
}

int wlan_son_cbs_deinit(void)
{
	int i;

	qdf_spinlock_destroy(&g_cbs_lock);
	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		if (!g_son_cbs[i])
			return -EINVAL;
		if (g_son_cbs[i]->vdev) {
			wlan_objmgr_vdev_release_ref(g_son_cbs[i]->vdev,
						     WLAN_SON_ID);
			son_debug("vdev_id: %d dereferenced",
				  wlan_vdev_get_id(g_son_cbs[i]->vdev));
		}
		qdf_timer_free(&g_son_cbs[i]->cbs_timer);
		qdf_mem_free(g_son_cbs[i]);
		g_son_cbs[i] = NULL;
	}

	son_debug("cbs deinit");

	return 0;
}

int wlan_son_cbs_enable(struct wlan_objmgr_vdev *vdev)
{
	struct scan_start_request *req;
	struct wlan_objmgr_psoc *psoc;
	enum son_cbs_state state;
	struct son_cbs *cbs;
	QDF_STATUS status;

	cbs = g_son_cbs[wlan_vdev_get_id(vdev)];
	if (!cbs) {
		son_err("invalid cbs");
		return -EINVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		son_err("invalid psoc");
		return -EINVAL;
	}

	state = wlan_son_cbs_get_state(cbs);
	if (state != CBS_INIT &&
	    state != CBS_WAIT) {
		son_err("can't start scan in state %d", state);
		return -EINVAL;
	}
	son_debug("State: %d", state);

	qdf_spin_lock_bh(&g_cbs_lock);
	if (!cbs->vdev) {
		cbs->vdev = vdev;
		status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SON_ID);
		if (status != QDF_STATUS_SUCCESS) {
			qdf_spin_unlock_bh(&g_cbs_lock);
			son_err("Failed to get VDEV reference");
			return -EAGAIN;
		}
		son_debug("vdev_id: %d referenced",
			  wlan_vdev_get_id(vdev));
	}
	cbs->cbs_scan_requestor =
		ucfg_scan_register_requester(psoc,
					     (uint8_t *)"cbs",
					     wlan_cbs_scan_event_cb,
					     (void *)cbs);
	son_debug("cbs_scan_requestor: %u vdev_id: %d",
		  cbs->cbs_scan_requestor, wlan_vdev_get_id(vdev));

	if (!cbs->cbs_scan_requestor) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SON_ID);
		qdf_spin_unlock_bh(&g_cbs_lock);
		son_err("ucfg_scan_register_requestor failed");
		return -EINVAL;
	}

	req = &cbs->scan_params;
	ucfg_scan_init_default_params(vdev, req);
	req->scan_req.scan_req_id = cbs->cbs_scan_requestor;

	req->scan_req.vdev_id = wlan_vdev_get_id(vdev);
	req->scan_req.scan_priority = SCAN_PRIORITY_HIGH;
	req->scan_req.scan_f_bcast_probe = true;

	req->scan_req.scan_f_passive = true;
	req->scan_req.max_rest_time = DEFAULT_SCAN_MAX_REST_TIME;
	req->scan_req.scan_f_forced = true;

	req->scan_req.scan_flags = 0;
	req->scan_req.dwell_time_active = cbs->dwell_split_time;
	req->scan_req.dwell_time_passive = cbs->dwell_split_time + 5;
	req->scan_req.min_rest_time = CBS_DEFAULT_MIN_REST_TIME;
	req->scan_req.max_rest_time = CBS_DEFAULT_DWELL_REST_TIME;
	req->scan_req.scan_f_passive = false;
	req->scan_req.scan_f_2ghz = true;
	req->scan_req.scan_f_5ghz = true;
	req->scan_req.scan_f_offchan_mgmt_tx = true;
	req->scan_req.scan_f_offchan_data_tx = true;
	req->scan_req.scan_f_chan_stat_evnt = true;

	if (cbs->min_dwell_rest_time % DEFAULT_BEACON_INTERVAL) {
		cbs->min_dwell_rest_time =
			(cbs->min_dwell_rest_time /
			(2 * DEFAULT_BEACON_INTERVAL)) *
			(2 * DEFAULT_BEACON_INTERVAL) +
			(cbs->min_dwell_rest_time % 200 < 100) ? 100 : 200;
	}

	wlan_son_cbs_init_dwell_params(cbs,
				       cbs->dwell_split_time,
				       cbs->min_dwell_rest_time);

	cbs->dwell_split_cnt--;
	wlan_son_cbs_set_state(cbs, CBS_SCAN);

	wlan_son_cbs_start(cbs);
	qdf_spin_unlock_bh(&g_cbs_lock);

	son_debug("cbs enable");

	return 0;
}

int wlan_son_cbs_disable(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct son_cbs *cbs;

	if (!vdev) {
		son_err("invalid vdev");
		return -EINVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		son_err("invalid psoc");
		return -EINVAL;
	}
	cbs = g_son_cbs[wlan_vdev_get_id(vdev)];
	if (!cbs || !cbs->vdev) {
		son_err("vdev null");
		return -EINVAL;
	}
	wlan_son_deliver_cbs_cancelled(vdev);

	qdf_timer_sync_cancel(&cbs->cbs_timer);

	wlan_son_cbs_stop(cbs);

	son_debug("cbs_scan_requestor: %d vdev_id: %d",
		  cbs->cbs_scan_requestor, wlan_vdev_get_id(vdev));
	ucfg_scan_unregister_requester(psoc, cbs->cbs_scan_requestor);

	qdf_spin_lock_bh(&g_cbs_lock);
	wlan_son_cbs_set_state(cbs, CBS_INIT);
	if (vdev == cbs->vdev) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_SON_ID);
		son_debug("vdev_id: %d dereferenced",
			  vdev->vdev_objmgr.vdev_id);
	}
	cbs->vdev = NULL;
	qdf_spin_unlock_bh(&g_cbs_lock);

	son_debug("cbs disable");

	return 0;
}

int wlan_son_set_cbs(struct wlan_objmgr_vdev *vdev,
		     bool enable)
{
	son_debug("Enable: %u", enable);

	if (!vdev || !g_son_cbs[wlan_vdev_get_id(vdev)])
		return -EINVAL;

	if (enable)
		wlan_son_cbs_enable(vdev);
	else
		wlan_son_cbs_disable(vdev);

	return 0;
}

int wlan_son_set_cbs_wait_time(struct wlan_objmgr_vdev *vdev,
			       uint32_t val)
{
	if (!g_son_cbs[wlan_vdev_get_id(vdev)])
		return -EINVAL;

	son_debug("vdev_id: %d wait time %d", wlan_vdev_get_id(vdev), val);
	wlan_son_set_cbs(vdev, false);

	if (val % DEFAULT_BEACON_INTERVAL != 0) {
		val = (val / (2 * DEFAULT_BEACON_INTERVAL)) *
			(2 * DEFAULT_BEACON_INTERVAL) +
			(val % (2 * DEFAULT_BEACON_INTERVAL) <
				DEFAULT_BEACON_INTERVAL) ?
				DEFAULT_BEACON_INTERVAL :
				2 * DEFAULT_BEACON_INTERVAL;
	}
	qdf_spin_lock_bh(&g_cbs_lock);
	g_son_cbs[wlan_vdev_get_id(vdev)]->wait_time = val;
	qdf_spin_unlock_bh(&g_cbs_lock);

	wlan_son_set_cbs(vdev, true);

	return 0;
}

int wlan_son_set_cbs_dwell_split_time(struct wlan_objmgr_vdev *vdev,
				      uint32_t val)
{
	if (!g_son_cbs[wlan_vdev_get_id(vdev)])
		return -EINVAL;

	son_debug("vdev_id: %d dwell split time %d",
		  wlan_vdev_get_id(vdev), val);
	if (val != CBS_DWELL_TIME_10MS &&
	    val != CBS_DWELL_TIME_25MS &&
	    val != CBS_DWELL_TIME_50MS &&
	    val != CBS_DWELL_TIME_75MS) {
		son_err("dwell time not supported ");
		return -EINVAL;
	}

	wlan_son_set_cbs(vdev, false);

	qdf_spin_lock_bh(&g_cbs_lock);
	g_son_cbs[wlan_vdev_get_id(vdev)]->dwell_split_time = val;
	qdf_spin_unlock_bh(&g_cbs_lock);

	wlan_son_set_cbs(vdev, true);

	return 0;
}

uint8_t wlan_son_get_node_tx_power(struct element_info assoc_req_ies)
{
	const uint8_t *power_cap_ie_data;

	power_cap_ie_data = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_PWRCAP,
						     assoc_req_ies.ptr,
						     assoc_req_ies.len);
	if (power_cap_ie_data)
		return *(power_cap_ie_data + 3);
	else
		return 0;
}

QDF_STATUS wlan_son_get_peer_rrm_info(struct element_info assoc_req_ies,
				      uint8_t *rrmcaps,
				      bool *is_beacon_meas_supported)
{
	const uint8_t *eid;

	eid = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RRM,
				       assoc_req_ies.ptr,
				       assoc_req_ies.len);
	if (eid) {
		qdf_mem_copy(rrmcaps, &eid[2], eid[1]);
		if ((rrmcaps[0] &
		    IEEE80211_RRM_CAPS_BEACON_REPORT_PASSIVE) ||
		    (rrmcaps[0] &
		    IEEE80211_RRM_CAPS_BEACON_REPORT_ACTIVE))
			*is_beacon_meas_supported = true;
		return QDF_STATUS_SUCCESS;
	}
	return QDF_STATUS_E_RESOURCES;
}

QDF_STATUS
wlan_son_vdev_get_supported_txrx_streams(struct wlan_objmgr_vdev *vdev,
					 uint32_t *num_tx_streams,
					 uint32_t *num_rx_streams)
{
	struct wlan_mlme_nss_chains *nss_cfg;
	enum nss_chains_band_info band = NSS_CHAINS_BAND_MAX;
	struct wlan_channel *chan;
	qdf_freq_t chan_freq = 0;

	nss_cfg = mlme_get_dynamic_vdev_config(vdev);
	if (!nss_cfg)
		return QDF_STATUS_NOT_INITIALIZED;

	chan = wlan_vdev_get_active_channel(vdev);
	if (chan)
		chan_freq = chan->ch_freq;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
		band = NSS_CHAINS_BAND_2GHZ;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq))
		band = NSS_CHAINS_BAND_5GHZ;

	if (band == NSS_CHAINS_BAND_MAX)
		return QDF_STATUS_NOT_INITIALIZED;

	*num_tx_streams = nss_cfg->tx_nss[band];
	*num_rx_streams = nss_cfg->rx_nss[band];

	return QDF_STATUS_SUCCESS;
}
