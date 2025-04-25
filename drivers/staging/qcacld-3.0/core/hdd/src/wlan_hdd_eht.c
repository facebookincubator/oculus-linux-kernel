/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: wlan_hdd_eht.c
 *
 * WLAN Host Device Driver file for 802.11be (Extremely High Throughput)
 * support.
 *
 */

#include "wlan_hdd_main.h"
#include "wlan_hdd_eht.h"
#include "osif_sync.h"
#include "wlan_utility.h"
#include "wlan_mlme_ucfg_api.h"
#include "qc_sap_ioctl.h"
#include "wma_api.h"
#include "wlan_osif_features.h"
#include "wlan_psoc_mlme_ucfg_api.h"

#if defined(WLAN_FEATURE_11BE) && defined(CFG80211_11BE_BASIC)
#define CHAN_WIDTH_SET_40MHZ_IN_2G \
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_IN_2G
#define CHAN_WIDTH_SET_40MHZ_80MHZ_IN_5G \
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G
#define CHAN_WIDTH_SET_160MHZ_IN_5G \
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G
#define CHAN_WIDTH_SET_80PLUS80_MHZ_IN_5G \
	IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G

void hdd_update_tgt_eht_cap(struct hdd_context *hdd_ctx,
			    struct wma_tgt_cfg *cfg)
{
	tDot11fIEeht_cap eht_cap_ini = {0};

	ucfg_mlme_update_tgt_eht_cap(hdd_ctx->psoc, cfg);
	sme_update_tgt_eht_cap(hdd_ctx->mac_handle, cfg, &eht_cap_ini);
}

/*
 * Typical 802.11 Multi-Link element
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Elem ID | Elem Len |Elem ID Extn | MLink Ctrl | Common Info | Link Info |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *      1          1           1           2        Variable Len  Variable Len
 */
void wlan_hdd_get_mlo_link_id(struct hdd_beacon_data *beacon,
			      uint8_t *link_id, uint8_t *num_link)
{
	const uint8_t *mlie, *cmn_info_ie, *link_info_ie;
	uint8_t total_len, cmn_info_len, link_info_len;
	uint8_t link_len;

	mlie = wlan_get_ext_ie_ptr_from_ext_id(MLO_IE_OUI_TYPE, MLO_IE_OUI_SIZE,
					       beacon->tail, beacon->tail_len);
	if (mlie) {
		hdd_debug("ML IE found in beacon data");
		*num_link = 1;

		mlie++; /* WLAN_MAC_EID_EXT */
		total_len = *mlie++; /* length */

		cmn_info_ie = mlie + 3;
		cmn_info_len = *cmn_info_ie;

		/* 802.11 Common info sub-element in Multi-link element
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 * |Cmn info Len |MLD MAC| Link ID | .....
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 *        1          6        0/1
		 */

		*link_id = *(cmn_info_ie + 1 + QDF_MAC_ADDR_SIZE);

		/* Length of link info equal total length minus below:
		 * 1-Byte Extn Ele ID
		 * 2-Byte Multi link control
		 * Length of Common info sub-element
		 */

		link_info_ie = cmn_info_ie + cmn_info_len;
		link_info_len = total_len - cmn_info_len - 3;
		while (link_info_len > 0) {
			link_info_ie++;
			link_info_len--;
			/* length of sub element ID */
			link_len = *link_info_ie++;
			link_info_len--;
			link_info_ie += link_len;
			link_info_len -= link_len;
			(*num_link)++;
		}
	} else {
		*num_link = 0;
		hdd_debug("ML IE not found in beacon data");
	}
}

void wlan_hdd_check_11be_support(struct hdd_beacon_data *beacon,
				 struct sap_config *config)
{
	const uint8_t *ie;

	ie = wlan_get_ext_ie_ptr_from_ext_id(EHT_CAP_OUI_TYPE, EHT_CAP_OUI_SIZE,
					     beacon->tail, beacon->tail_len);
	if (ie)
		config->SapHw_mode = eCSR_DOT11_MODE_11be;
}

static void
hdd_update_wiphy_eht_caps_6ghz(struct hdd_context *hdd_ctx,
			       tDot11fIEeht_cap eht_cap)
{
	struct ieee80211_supported_band *band_6g =
		   hdd_ctx->wiphy->bands[HDD_NL80211_BAND_6GHZ];
	uint8_t *phy_info =
		    hdd_ctx->iftype_data_6g->eht_cap.eht_cap_elem.phy_cap_info;
	struct ieee80211_sband_iftype_data *iftype_sta;
	struct ieee80211_sband_iftype_data *iftype_ap;

	if (!band_6g || !phy_info) {
		hdd_debug("6ghz not supported in wiphy");
		return;
	}

	hdd_ctx->iftype_data_6g->types_mask =
		(BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP));
	band_6g->n_iftype_data = EHT_OPMODE_SUPPORTED;
	band_6g->iftype_data = hdd_ctx->iftype_data_6g;
	iftype_sta = hdd_ctx->iftype_data_6g;
	iftype_ap = hdd_ctx->iftype_data_6g + 1;


	hdd_ctx->iftype_data_6g->eht_cap.has_eht = eht_cap.present;
	if (hdd_ctx->iftype_data_6g->eht_cap.has_eht &&
	    !hdd_ctx->iftype_data_6g->he_cap.has_he) {
		hdd_debug("6 GHz HE caps not present");
		hdd_ctx->iftype_data_6g->eht_cap.has_eht = false;
		band_6g->n_iftype_data = 1;
		return;
	}

	if (eht_cap.support_320mhz_6ghz)
		phy_info[0] |= IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ;

	if (eht_cap.su_beamformer)
		phy_info[0] |= IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER;

	if (eht_cap.su_beamformee)
		phy_info[0] |= IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE;

	qdf_mem_copy(iftype_ap, hdd_ctx->iftype_data_6g,
		     sizeof(struct ieee80211_supported_band));

	iftype_sta->types_mask = BIT(NL80211_IFTYPE_STATION);
	iftype_ap->types_mask = BIT(NL80211_IFTYPE_AP);
}

void hdd_update_wiphy_eht_cap(struct hdd_context *hdd_ctx)
{
	tDot11fIEeht_cap eht_cap_cfg;
	struct ieee80211_supported_band *band_2g =
			hdd_ctx->wiphy->bands[HDD_NL80211_BAND_2GHZ];
	struct ieee80211_supported_band *band_5g =
			hdd_ctx->wiphy->bands[HDD_NL80211_BAND_5GHZ];
	QDF_STATUS status;
	uint8_t *phy_info_5g =
		    hdd_ctx->iftype_data_5g->eht_cap.eht_cap_elem.phy_cap_info;
	uint8_t *phy_info_2g =
		    hdd_ctx->iftype_data_2g->eht_cap.eht_cap_elem.phy_cap_info;
	bool eht_capab;
	struct ieee80211_sband_iftype_data *iftype_sta;
	struct ieee80211_sband_iftype_data *iftype_ap;

	hdd_enter();

	ucfg_psoc_mlme_get_11be_capab(hdd_ctx->psoc, &eht_capab);
	if (!eht_capab)
		return;

	status = ucfg_mlme_cfg_get_eht_caps(hdd_ctx->psoc, &eht_cap_cfg);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	if (band_2g) {
		iftype_sta = hdd_ctx->iftype_data_2g;
		iftype_ap = hdd_ctx->iftype_data_2g + 1;
		hdd_ctx->iftype_data_2g->types_mask =
			(BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP));
		band_2g->n_iftype_data = EHT_OPMODE_SUPPORTED;
		band_2g->iftype_data = hdd_ctx->iftype_data_2g;

		hdd_ctx->iftype_data_2g->eht_cap.has_eht = eht_cap_cfg.present;
		if (hdd_ctx->iftype_data_2g->eht_cap.has_eht &&
		    !hdd_ctx->iftype_data_2g->he_cap.has_he) {
			hdd_debug("2.4 GHz HE caps not present");
			hdd_ctx->iftype_data_2g->eht_cap.has_eht = false;
			band_2g->n_iftype_data = 1;
			goto band_5ghz;
		}

		if (eht_cap_cfg.su_beamformer)
			phy_info_2g[0] |= IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER;

		if (eht_cap_cfg.su_beamformee)
			phy_info_2g[0] |= IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE;

		qdf_mem_copy(iftype_ap, hdd_ctx->iftype_data_2g,
			     sizeof(struct ieee80211_supported_band));

		iftype_sta->types_mask = BIT(NL80211_IFTYPE_STATION);
		iftype_ap->types_mask = BIT(NL80211_IFTYPE_AP);
	}

band_5ghz:
	if (band_5g) {
		iftype_sta = hdd_ctx->iftype_data_5g;
		iftype_ap = hdd_ctx->iftype_data_5g + 1;
		hdd_ctx->iftype_data_5g->types_mask =
			(BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP));
		band_5g->n_iftype_data = EHT_OPMODE_SUPPORTED;
		band_5g->iftype_data = hdd_ctx->iftype_data_5g;

		hdd_ctx->iftype_data_5g->eht_cap.has_eht = eht_cap_cfg.present;
		if (hdd_ctx->iftype_data_5g->eht_cap.has_eht &&
		    !hdd_ctx->iftype_data_5g->he_cap.has_he) {
			hdd_debug("5 GHz HE caps not present");
			hdd_ctx->iftype_data_5g->eht_cap.has_eht = false;
			band_5g->n_iftype_data = 1;
			goto band_6ghz;
		}

		if (eht_cap_cfg.su_beamformer)
			phy_info_5g[0] |= IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER;

		if (eht_cap_cfg.su_beamformee)
			phy_info_5g[0] |= IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE;

		qdf_mem_copy(iftype_ap, hdd_ctx->iftype_data_5g,
			     sizeof(struct ieee80211_supported_band));

		iftype_sta->types_mask = BIT(NL80211_IFTYPE_STATION);
		iftype_ap->types_mask = BIT(NL80211_IFTYPE_AP);
	}

band_6ghz:
	hdd_update_wiphy_eht_caps_6ghz(hdd_ctx, eht_cap_cfg);

	hdd_exit();
}

int hdd_set_11be_rate_code(struct hdd_adapter *adapter, uint16_t rate_code)
{
	uint8_t preamble = 0, nss = 0, rix = 0;
	int ret;
	struct sap_config *sap_config = NULL;

	if (adapter->device_mode == QDF_SAP_MODE)
		sap_config = &adapter->deflink->session.ap.sap_config;

	if (!sap_config) {
		if (!sme_is_feature_supported_by_fw(DOT11BE)) {
			hdd_err_rl("Target does not support 11be");
			return -EIO;
		}
	} else if (sap_config->SapHw_mode != eCSR_DOT11_MODE_11be &&
		   sap_config->SapHw_mode != eCSR_DOT11_MODE_11be_ONLY) {
		hdd_err_rl("Invalid hw mode, SAP hw_mode= 0x%x, ch_freq = %d",
			   sap_config->SapHw_mode, sap_config->chan_freq);
		return -EIO;
	}

	if ((rate_code >> 8) != WMI_RATE_PREAMBLE_EHT) {
		hdd_err_rl("Invalid input: %x", rate_code);
		return -EIO;
	}

	rix = RC_2_RATE_IDX_11BE(rate_code);
	preamble = rate_code >> 8;
	nss = HT_RC_2_STREAMS_11BE(rate_code) + 1;

	hdd_debug("SET_11BE_RATE rate_code %d rix %d preamble %x nss %d",
		  rate_code, rix, preamble, nss);

	ret = wma_cli_set_command(adapter->deflink->vdev_id,
				  wmi_vdev_param_fixed_rate,
				  rate_code, VDEV_CMD);

	return ret;
}

/**
 * hdd_map_eht_gi_to_os() - map txrate_gi to os guard interval
 * @guard_interval: guard interval get from fw rate
 *
 * Return: os guard interval value
 */
static inline uint8_t hdd_map_eht_gi_to_os(enum txrate_gi guard_interval)
{
	switch (guard_interval) {
	case TXRATE_GI_0_8_US:
		return NL80211_RATE_INFO_EHT_GI_0_8;
	case TXRATE_GI_1_6_US:
		return NL80211_RATE_INFO_EHT_GI_1_6;
	case TXRATE_GI_3_2_US:
		return NL80211_RATE_INFO_EHT_GI_3_2;
	default:
		return NL80211_RATE_INFO_EHT_GI_0_8;
	}
}

/**
 * wlan_hdd_fill_os_eht_rateflags() - Fill EHT related rate_info
 * @os_rate: rate info for os
 * @rate_flags: rate flags
 * @dcm: dcm from rate
 * @guard_interval: guard interval from rate
 *
 * Return: none
 */
void wlan_hdd_fill_os_eht_rateflags(struct rate_info *os_rate,
				    enum tx_rate_info rate_flags,
				    uint8_t dcm,
				    enum txrate_gi guard_interval)
{
	/* as fw not yet report ofdma to host, so don't
	 * fill RATE_INFO_BW_EHT_RU.
	 */
	if (rate_flags & (TX_RATE_EHT80 | TX_RATE_EHT40 |
	    TX_RATE_EHT20 | TX_RATE_EHT160 | TX_RATE_EHT320)) {
		if (rate_flags & TX_RATE_EHT320)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_320);
		else if (rate_flags & TX_RATE_EHT160)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_160);
		else if (rate_flags & TX_RATE_EHT80)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_80);
		else if (rate_flags & TX_RATE_EHT40)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_40);

		os_rate->flags |= RATE_INFO_FLAGS_EHT_MCS;
	}
}

#ifdef FEATURE_RX_LINKSPEED_ROAM_TRIGGER
void
wlan_hdd_refill_os_eht_rateflags(struct rate_info *os_rate, uint8_t preamble)
{
	if (preamble == DOT11_BE)
		os_rate->flags |= RATE_INFO_FLAGS_EHT_MCS;
}

void
wlan_hdd_refill_os_eht_bw(struct rate_info *os_rate, enum rx_tlv_bw bw)
{
	if (bw == RX_TLV_BW_320MHZ)
		os_rate->bw = RATE_INFO_BW_320;
	else
		os_rate->bw = RATE_INFO_BW_20; /* Invalid bw: set 20M */
}
#endif
#endif
