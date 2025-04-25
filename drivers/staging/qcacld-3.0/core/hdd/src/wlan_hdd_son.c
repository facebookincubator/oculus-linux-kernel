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
 * DOC: contains son hdd API implementation
 */

#include <qdf_types.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_sta_info.h>
#include <wlan_hdd_regulatory.h>
#include <os_if_son.h>
#include <sap_internal.h>
#include <wma_api.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_reg_services_api.h>
#include <son_ucfg_api.h>
#include <wlan_hdd_son.h>
#include <wlan_hdd_object_manager.h>
#include <wlan_hdd_stats.h>
#include "wlan_cfg80211_mc_cp_stats.h"

static const struct son_chan_width {
	enum ieee80211_cwm_width son_chwidth;
	enum phy_ch_width phy_chwidth;
} son_chwidth_info[] = {
	{
		.son_chwidth = IEEE80211_CWM_WIDTH20,
		.phy_chwidth = CH_WIDTH_20MHZ,
	},
	{
		.son_chwidth = IEEE80211_CWM_WIDTH40,
		.phy_chwidth = CH_WIDTH_40MHZ,
	},
	{
		.son_chwidth = IEEE80211_CWM_WIDTH80,
		.phy_chwidth = CH_WIDTH_80MHZ,
	},
	{
		.son_chwidth = IEEE80211_CWM_WIDTH160,
		.phy_chwidth = CH_WIDTH_160MHZ,
	},
	{
		.son_chwidth = IEEE80211_CWM_WIDTH80_80,
		.phy_chwidth = CH_WIDTH_80P80MHZ,
	},
#ifdef WLAN_FEATURE_11BE
	{
		.son_chwidth = IEEE80211_CWM_WIDTH320,
		.phy_chwidth = CH_WIDTH_320MHZ,
	},
#endif
};

/**
 * hdd_son_is_acs_in_progress() - whether acs is in progress or not
 * @vdev: vdev
 *
 * Return: true if acs is in progress
 */
static uint32_t hdd_son_is_acs_in_progress(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_hdd_link_info *link_info;
	bool in_progress = false;

	if (!vdev) {
		hdd_err("null vdev");
		return in_progress;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return in_progress;
	}

	if (!hdd_adapter_is_ap(link_info->adapter)) {
		hdd_err("vdev id %d is not AP", link_info->vdev_id);
		return in_progress;
	}

	in_progress = qdf_atomic_read(&link_info->session.ap.acs_in_progress);

	return in_progress;
}

/**
 * hdd_son_chan_width_to_chan_width() - translate son chan width
 *                                      to mac chan width
 * @son_chwidth: son chan width
 *
 * Return: mac chan width
 */
static enum eSirMacHTChannelWidth hdd_son_chan_width_to_chan_width(
				enum ieee80211_cwm_width son_chwidth)
{
	enum eSirMacHTChannelWidth chwidth;

	switch (son_chwidth) {
	case IEEE80211_CWM_WIDTH20:
		chwidth = eHT_CHANNEL_WIDTH_20MHZ;
		break;
	case IEEE80211_CWM_WIDTH40:
		chwidth = eHT_CHANNEL_WIDTH_40MHZ;
		break;
	case IEEE80211_CWM_WIDTH80:
		chwidth = eHT_CHANNEL_WIDTH_80MHZ;
		break;
	case IEEE80211_CWM_WIDTH160:
		chwidth = eHT_CHANNEL_WIDTH_160MHZ;
		break;
	case IEEE80211_CWM_WIDTH80_80:
		chwidth = eHT_CHANNEL_WIDTH_80P80MHZ;
		break;
	default:
		chwidth = eHT_MAX_CHANNEL_WIDTH;
	}

	return chwidth;
}

/**
 * hdd_son_set_chwidth() - set son chan width
 * @vdev: vdev
 * @son_chwidth: son chan width
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_set_chwidth(struct wlan_objmgr_vdev *vdev,
			       enum ieee80211_cwm_width son_chwidth)
{
	enum eSirMacHTChannelWidth chwidth;
	struct wlan_hdd_link_info *link_info;
	uint8_t link_id = 0xFF;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter for %d", wlan_vdev_get_id(vdev));
		return -EINVAL;
	}

	chwidth = hdd_son_chan_width_to_chan_width(son_chwidth);

	return hdd_set_mac_chan_width(link_info, chwidth, link_id, false);
}

/**
 * hdd_chan_width_to_son_chwidth() - translate mac chan width
 *                                   to son chan width
 * @chwidth: mac chan width
 *
 * Return: son chan width
 */
static enum ieee80211_cwm_width hdd_chan_width_to_son_chwidth(
				enum eSirMacHTChannelWidth chwidth)
{
	enum ieee80211_cwm_width son_chwidth;

	switch (chwidth) {
	case eHT_CHANNEL_WIDTH_20MHZ:
		son_chwidth = IEEE80211_CWM_WIDTH20;
		break;
	case eHT_CHANNEL_WIDTH_40MHZ:
		son_chwidth = IEEE80211_CWM_WIDTH40;
		break;
	case eHT_CHANNEL_WIDTH_80MHZ:
		son_chwidth = IEEE80211_CWM_WIDTH80;
		break;
	case eHT_CHANNEL_WIDTH_160MHZ:
		son_chwidth = IEEE80211_CWM_WIDTH160;
		break;
	case eHT_CHANNEL_WIDTH_80P80MHZ:
		son_chwidth = IEEE80211_CWM_WIDTH80_80;
		break;
	default:
		son_chwidth = IEEE80211_CWM_WIDTHINVALID;
	}

	return son_chwidth;
}

/**
 * hdd_phy_chwidth_to_son_chwidth() - translate phy chan width
 *                                    to son chan width
 * @chwidth: phy chan width
 *
 * Return: son chan width
 */
static enum ieee80211_cwm_width
hdd_phy_chwidth_to_son_chwidth(enum phy_ch_width chwidth)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(son_chwidth_info); i++) {
		if (son_chwidth_info[i].phy_chwidth == chwidth)
			return son_chwidth_info[i].son_chwidth;
	}

	hdd_err("Unsupported channel width %d", chwidth);
	return IEEE80211_CWM_WIDTHINVALID;
}

/**
 * hdd_son_get_chwidth() - get chan width
 * @vdev: vdev
 *
 * Return: son chan width
 */
static enum ieee80211_cwm_width hdd_son_get_chwidth(
						struct wlan_objmgr_vdev *vdev)
{
	struct wlan_channel *des_chan;

	if (!vdev) {
		hdd_err("null vdev");
		return IEEE80211_CWM_WIDTHINVALID;
	}

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);

	return hdd_phy_chwidth_to_son_chwidth(des_chan->ch_width);
}

/**
 * hdd_son_chan_ext_offset_to_chan_type() - translate son chan extend offset
 *                                          to chan type
 * @son_chan_ext_offset: son chan ext offset
 *
 * Return: tSirMacHTChannelType
 */
static tSirMacHTChannelType hdd_son_chan_ext_offset_to_chan_type(
				enum sec20_chan_offset son_chan_ext_offset)
{
	tSirMacHTChannelType chan_type;

	switch (son_chan_ext_offset) {
	case EXT_CHAN_OFFSET_ABOVE:
		chan_type = eHT_CHAN_HT40PLUS;
		break;
	case EXT_CHAN_OFFSET_BELOW:
		chan_type = eHT_CHAN_HT40MINUS;
		break;
	default:
		chan_type = eHT_CHAN_HT20;
		break;
	}

	return chan_type;
}

/**
 * hdd_son_set_chan_ext_offset() - set son chan extend offset
 * @vdev: vdev
 * @son_chan_ext_offset: son chan extend offset
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_set_chan_ext_offset(
				struct wlan_objmgr_vdev *vdev,
				enum sec20_chan_offset son_chan_ext_offset)
{
	enum eSirMacHTChannelType chan_type;
	QDF_STATUS status;
	int retval = -EINVAL;
	struct hdd_adapter *adapter;

	if (!vdev) {
		hdd_err("null vdev");
		return retval;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return retval;
	}

	if (!hdd_adapter_is_ap(link_info->adapter)) {
		hdd_err("vdev id %d is not AP", link_info->vdev_id);
		return retval;
	}

	retval = 0;
	chan_type = hdd_son_chan_ext_offset_to_chan_type(son_chan_ext_offset);
	status = hdd_set_sap_ht2040_mode(link_info->adapter, chan_type);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Cannot set SAP HT20/40 mode!");
		retval = -EINVAL;
	}

	return retval;
}

/**
 * hdd_chan_type_to_son_chan_ext_offset() - translate tSirMacHTChannelType
 *                                          to son chan extend offset
 * @chan_type: tSirMacHTChannelType
 *
 * Return: son chan extend offset
 */
static enum sec20_chan_offset hdd_chan_type_to_son_chan_ext_offset(
				tSirMacHTChannelType chan_type)
{
	enum sec20_chan_offset son_chan_ext_offset;

	switch (chan_type) {
	case eHT_CHAN_HT40PLUS:
		son_chan_ext_offset = EXT_CHAN_OFFSET_ABOVE;
		break;
	case eHT_CHAN_HT40MINUS:
		son_chan_ext_offset = EXT_CHAN_OFFSET_BELOW;
		break;
	default:
		son_chan_ext_offset = EXT_CHAN_OFFSET_NA;
		break;
	}

	return son_chan_ext_offset;
}

/**
 * hdd_son_get_chan_ext_offset() - get chan extend offset
 * @vdev: vdev
 *
 * Return: enum sec20_chan_offset
 */
static enum sec20_chan_offset hdd_son_get_chan_ext_offset(
						struct wlan_objmgr_vdev *vdev)
{
	enum eSirMacHTChannelType chan_type;
	QDF_STATUS status;
	struct wlan_hdd_link_info *link_info;

	if (!vdev) {
		hdd_err("null vdev");
		return 0;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return 0;
	}

	if (!hdd_adapter_is_ap(link_info->adapter)) {
		hdd_err("vdev id %d is not AP", link_info->vdev_id);
		return 0;
	}

	status = hdd_get_sap_ht2040_mode(link_info->adapter, &chan_type);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Cannot set SAP HT20/40 mode!");
		return 0;
	}

	return hdd_chan_type_to_son_chan_ext_offset(chan_type);
}

/**
 * hdd_son_bandwidth_to_phymode() - get new eCsrPhyMode according
 *                                  to son band width
 * @son_bandwidth: son band width
 * @old_phymode: old eCsrPhyMode
 * @phymode: new eCsrPhyMode to get
 *
 * Return: void
 */
static void hdd_son_bandwidth_to_phymode(uint32_t son_bandwidth,
					 eCsrPhyMode old_phymode,
					 eCsrPhyMode *phymode)
{
	*phymode = old_phymode;

	switch (son_bandwidth) {
	case HT20:
	case HT40:
		*phymode = eCSR_DOT11_MODE_11n;
		break;
	case VHT20:
	case VHT40:
	case VHT80:
	case VHT160:
	case VHT80_80:
		*phymode = eCSR_DOT11_MODE_11ac;
		break;
	case HE20:
	case HE40:
	case HE80:
	case HE160:
	case HE80_80:
		*phymode = eCSR_DOT11_MODE_11ax;
		break;
	default:
		break;
	}
}

/**
 * hdd_son_bandwidth_to_bonding_mode() - son band with to bonding mode
 * @son_bandwidth: son band width
 * @bonding_mode: bonding mode to get
 *
 * Return: void
 */
static void hdd_son_bandwidth_to_bonding_mode(uint32_t son_bandwidth,
					      uint32_t *bonding_mode)
{
	switch (son_bandwidth) {
	case HT40:
	case VHT40:
	case VHT80:
	case VHT160:
	case VHT80_80:
	case HE40:
	case HE80:
	case HE160:
	case HE80_80:
		*bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;
		break;
	default:
		*bonding_mode = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	}
}

/**
 * hdd_son_set_bandwidth() - set band width
 * @vdev: vdev
 * @son_bandwidth: son band width
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_set_bandwidth(struct wlan_objmgr_vdev *vdev,
				 uint32_t son_bandwidth)
{
	eCsrPhyMode phymode;
	eCsrPhyMode old_phymode;
	uint8_t supported_band;
	uint32_t bonding_mode;
	struct wlan_hdd_link_info *link_info;
	struct hdd_context *hdd_ctx;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(link_info->adapter);
	if (!hdd_ctx) {
		hdd_err("null hdd ctx");
		return -EINVAL;
	}
	old_phymode = sme_get_phy_mode(hdd_ctx->mac_handle);

	hdd_son_bandwidth_to_phymode(son_bandwidth, old_phymode, &phymode);

	if (wlan_reg_is_6ghz_supported(hdd_ctx->psoc))
		supported_band = REG_BAND_MASK_ALL;
	else
		supported_band = BIT(REG_BAND_2G) | BIT(REG_BAND_5G);

	hdd_son_bandwidth_to_bonding_mode(son_bandwidth, &bonding_mode);

	return hdd_update_phymode(link_info->adapter, phymode,
				  supported_band, bonding_mode);
}

/**
 * hdd_phymode_chwidth_to_son_bandwidth() - get son bandwidth from
 *                                          phymode and chwidth
 * @phymode: eCsrPhyMode
 * @chwidth: eSirMacHTChannelWidth
 *
 * Return: son bandwidth
 */
static uint32_t hdd_phymode_chwidth_to_son_bandwidth(
					eCsrPhyMode phymode,
					enum eSirMacHTChannelWidth chwidth)
{
	uint32_t son_bandwidth = NONHT;

	switch (phymode) {
	case eCSR_DOT11_MODE_abg:
	case eCSR_DOT11_MODE_11a:
	case eCSR_DOT11_MODE_11b:
	case eCSR_DOT11_MODE_11g:
	case eCSR_DOT11_MODE_11g_ONLY:
	case eCSR_DOT11_MODE_11b_ONLY:
		son_bandwidth = NONHT;
		break;
	case eCSR_DOT11_MODE_11n:
	case eCSR_DOT11_MODE_11n_ONLY:
		son_bandwidth = HT20;
		if (chwidth == eHT_CHANNEL_WIDTH_40MHZ)
			son_bandwidth = HT40;
		break;
	case eCSR_DOT11_MODE_11ac:
	case eCSR_DOT11_MODE_11ac_ONLY:
		son_bandwidth = VHT20;
		if (chwidth == eHT_CHANNEL_WIDTH_40MHZ)
			son_bandwidth = VHT40;
		else if (chwidth == eHT_CHANNEL_WIDTH_80MHZ)
			son_bandwidth = VHT80;
		else if (chwidth == eHT_CHANNEL_WIDTH_160MHZ)
			son_bandwidth = VHT160;
		else if (chwidth == eHT_CHANNEL_WIDTH_80P80MHZ)
			son_bandwidth = VHT80_80;
		break;
	case eCSR_DOT11_MODE_11ax:
	case eCSR_DOT11_MODE_11ax_ONLY:
	case eCSR_DOT11_MODE_AUTO:
		son_bandwidth = HE20;
		if (chwidth == eHT_CHANNEL_WIDTH_40MHZ)
			son_bandwidth = HE40;
		else if (chwidth == eHT_CHANNEL_WIDTH_80MHZ)
			son_bandwidth = HE80;
		else if (chwidth == eHT_CHANNEL_WIDTH_160MHZ)
			son_bandwidth = HE160;
		else if (chwidth == eHT_CHANNEL_WIDTH_80P80MHZ)
			son_bandwidth = HE80_80;
		break;
	default:
		break;
	}

	return son_bandwidth;
}

/**
 * hdd_son_get_bandwidth() - get band width
 * @vdev: vdev
 *
 * Return: band width
 */
static uint32_t hdd_son_get_bandwidth(struct wlan_objmgr_vdev *vdev)
{
	enum eSirMacHTChannelWidth chwidth;
	eCsrPhyMode phymode;
	struct hdd_context *hdd_ctx;
	struct wlan_hdd_link_info *link_info;

	if (!vdev) {
		hdd_err("null vdev");
		return NONHT;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return NONHT;
	}

	chwidth = wma_cli_get_command(link_info->vdev_id,
				      wmi_vdev_param_chwidth, VDEV_CMD);

	if (chwidth < 0) {
		hdd_err("Failed to get chwidth");
		return NONHT;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(link_info->adapter);
	if (!hdd_ctx) {
		hdd_err("null hdd ctx");
		return -NONHT;
	}

	phymode = sme_get_phy_mode(hdd_ctx->mac_handle);

	return hdd_phymode_chwidth_to_son_bandwidth(phymode, chwidth);
}

/**
 * hdd_son_band_to_band() - translate SON band mode to reg_wifi_band
 * @band: given enum wlan_band_id
 *
 * Return: reg_wifi_band
 */
static enum reg_wifi_band hdd_son_band_to_band(enum wlan_band_id band)
{
	enum reg_wifi_band reg_band = REG_BAND_UNKNOWN;

	switch (band) {
	case WLAN_BAND_2GHZ:
		reg_band = REG_BAND_2G;
		break;
	case WLAN_BAND_5GHZ:
		reg_band = REG_BAND_5G;
		break;
	case WLAN_BAND_6GHZ:
		reg_band = REG_BAND_6G;
		break;
	default:
		break;
	}

	return reg_band;
}

/**
 * hdd_son_set_chan() - set chan
 * @vdev: vdev
 * @chan: given chan
 * @son_band: given enum wlan_band_id
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_set_chan(struct wlan_objmgr_vdev *vdev, int chan,
			    enum wlan_band_id son_band)
{
	struct wlan_hdd_link_info *link_info;
	enum reg_wifi_band band = hdd_son_band_to_band(son_band);
	bool status;
	qdf_freq_t freq;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}

	if (!hdd_adapter_is_ap(link_info->adapter)) {
		hdd_err("vdev id %d is not AP", link_info->vdev_id);
		return -ENOTSUPP;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		hdd_err("null pdev");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		hdd_err("null psoc");
		return -EINVAL;
	}

	freq = wlan_reg_chan_band_to_freq(pdev, chan, BIT(band));
	status = policy_mgr_is_sap_allowed_on_dfs_freq(pdev, link_info->vdev_id,
						       freq);
	if (!status) {
		hdd_err("sap_allowed_on_dfs_freq check fails");
		return -EINVAL;
	}
	wlan_hdd_set_sap_csa_reason(psoc, link_info->vdev_id,
				    CSA_REASON_USER_INITIATED);

	return hdd_softap_set_channel_change(link_info->adapter->dev, freq,
					     CH_WIDTH_MAX, false);
}

/**
 * hdd_son_set_country() - set country code
 * @vdev: vdev
 * @country_code:pointer to country code
 *
 * Return: 0 if country code is set successfully
 */
static int hdd_son_set_country(struct wlan_objmgr_vdev *vdev,
			       char *country_code)
{
	struct hdd_context *hdd_ctx;
	struct wlan_hdd_link_info *link_info;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(link_info->adapter);
	if (!hdd_ctx) {
		hdd_err("null hdd ctx");
		return -EINVAL;
	}

	return hdd_reg_set_country(hdd_ctx, country_code);
}

/**
 * hdd_son_set_candidate_freq() - set candidate freq. Switch to this freq
 *                                after radar is detected
 * @vdev: vdev
 * @freq: candidate frequency
 *
 * Return: 0 if candidate freq is set successfully.
 */
static int hdd_son_set_candidate_freq(struct wlan_objmgr_vdev *vdev,
				      qdf_freq_t freq)
{
	struct wlan_hdd_link_info *link_info;
	struct sap_context *sap_ctx;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}
	if (!hdd_adapter_is_ap(link_info->adapter)) {
		hdd_err("vdev id %d is not AP", link_info->vdev_id);
		return -EINVAL;
	}

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	if (!sap_ctx) {
		hdd_err("null sap_ctx");
		return -EINVAL;
	}

	sap_ctx->candidate_freq = freq;

	return 0;
}

/**
 * hdd_son_get_candidate_freq() - get candidate freq
 * @vdev: vdev
 *
 * Return: candidate freq
 */
static qdf_freq_t hdd_son_get_candidate_freq(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_hdd_link_info *link_info;
	struct sap_context *sap_ctx;
	qdf_freq_t freq = 0;

	if (!vdev) {
		hdd_err("null vdev");
		return freq;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return freq;
	}
	if (!hdd_adapter_is_ap(link_info->adapter)) {
		hdd_err("vdev id %d is not AP", link_info->vdev_id);
		return freq;
	}

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	if (!sap_ctx) {
		hdd_err("null sap_ctx");
		return freq;
	}
	freq = sap_ctx->candidate_freq;

	return freq;
}

/**
 * hdd_son_phy_mode_to_vendor_phy_mode() - translate son phy mode to
 *                                         vendor_phy_mode
 * @mode: son phy mode
 *
 * Return: qca_wlan_vendor_phy_mode
 */
static enum qca_wlan_vendor_phy_mode hdd_son_phy_mode_to_vendor_phy_mode(
						enum ieee80211_phymode mode)
{
	enum qca_wlan_vendor_phy_mode vendor_mode;

	switch (mode) {
	case IEEE80211_MODE_AUTO:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_AUTO;
		break;
	case IEEE80211_MODE_11A:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11A;
		break;
	case IEEE80211_MODE_11B:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11B;
		break;
	case IEEE80211_MODE_11G:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11G;
		break;
	case IEEE80211_MODE_11NA_HT20:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NA_HT20;
		break;
	case IEEE80211_MODE_11NG_HT20:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NG_HT20;
		break;
	case IEEE80211_MODE_11NA_HT40PLUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40PLUS;
		break;
	case IEEE80211_MODE_11NA_HT40MINUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40MINUS;
		break;
	case IEEE80211_MODE_11NG_HT40PLUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40PLUS;
		break;
	case IEEE80211_MODE_11NG_HT40MINUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40MINUS;
		break;
	case IEEE80211_MODE_11NG_HT40:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40;
		break;
	case IEEE80211_MODE_11NA_HT40:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40;
		break;
	case IEEE80211_MODE_11AC_VHT20:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT20;
		break;
	case IEEE80211_MODE_11AC_VHT40PLUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40PLUS;
		break;
	case IEEE80211_MODE_11AC_VHT40MINUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40MINUS;
		break;
	case IEEE80211_MODE_11AC_VHT40:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40;
		break;
	case IEEE80211_MODE_11AC_VHT80:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT80;
		break;
	case IEEE80211_MODE_11AC_VHT160:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT160;
		break;
	case IEEE80211_MODE_11AC_VHT80_80:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT80P80;
		break;
	case IEEE80211_MODE_11AXA_HE20:
	case IEEE80211_MODE_11AXG_HE20:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AX_HE20;
		break;
	case IEEE80211_MODE_11AXA_HE40PLUS:
	case IEEE80211_MODE_11AXG_HE40PLUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40PLUS;
		break;
	case IEEE80211_MODE_11AXA_HE40MINUS:
	case IEEE80211_MODE_11AXG_HE40MINUS:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40MINUS;
		break;
	case IEEE80211_MODE_11AXA_HE40:
	case IEEE80211_MODE_11AXG_HE40:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40;
		break;
	case IEEE80211_MODE_11AXA_HE80:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AX_HE80;
		break;
	case IEEE80211_MODE_11AXA_HE160:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AX_HE160;
		break;
	case IEEE80211_MODE_11AXA_HE80_80:
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_11AX_HE80P80;
		break;
	default:
		hdd_err("Invalid config phy mode %d, set it as auto", mode);
		vendor_mode = QCA_WLAN_VENDOR_PHY_MODE_AUTO;
		break;
	}

	return vendor_mode;
}

/**
 * hdd_son_set_phymode() - set son phy mode
 * @vdev: vdev
 * @mode: son phy mode to set
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_set_phymode(struct wlan_objmgr_vdev *vdev,
			       enum ieee80211_phymode mode)
{
	struct wlan_hdd_link_info *link_info;
	enum qca_wlan_vendor_phy_mode vendor_phy_mode;
	QDF_STATUS status;
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct sap_config *sap_config;
	struct wlan_hdd_link_info *link_info;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("Invalid VDEV %d", wlan_vdev_get_id(vdev));
		return -EINVAL;
	}

	if (!hdd_adapter_is_ap(link_info->adapter)) {
		hdd_err("vdev id %d is not AP", link_info->vdev_id);
		return -EINVAL;
	}

	vendor_phy_mode = hdd_son_phy_mode_to_vendor_phy_mode(mode);

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(link_info);
	sap_config = &hdd_ap_ctx->sap_config;
	status = wlansap_son_update_sap_config_phymode(vdev, sap_config,
						       vendor_phy_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("update son phy mode error");
		return -EINVAL;
	}

	hdd_restart_sap(link_info);

	return 0;
}

/**
 * hdd_wlan_phymode_to_son_phymode() - get son phymode from wlan_phymode phymode
 * @phymode: wlan_phymode phymode
 *
 * Return: ieee80211_phymode
 */
static enum ieee80211_phymode hdd_wlan_phymode_to_son_phymode(
					enum wlan_phymode phymode)
{
	enum ieee80211_phymode son_phymode;

	switch (phymode) {
	case WLAN_PHYMODE_AUTO:
		son_phymode = IEEE80211_MODE_AUTO;
		break;
	case WLAN_PHYMODE_11A:
		son_phymode = IEEE80211_MODE_11A;
		break;
	case WLAN_PHYMODE_11B:
		son_phymode = IEEE80211_MODE_11B;
		break;
	case WLAN_PHYMODE_11G:
	case WLAN_PHYMODE_11G_ONLY:
		son_phymode = IEEE80211_MODE_11G;
		break;
	case WLAN_PHYMODE_11NA_HT20:
		son_phymode = IEEE80211_MODE_11NA_HT20;
		break;
	case WLAN_PHYMODE_11NG_HT20:
		son_phymode = IEEE80211_MODE_11NG_HT20;
		break;
	case WLAN_PHYMODE_11NA_HT40:
		son_phymode = IEEE80211_MODE_11NA_HT40;
		break;
	case WLAN_PHYMODE_11NG_HT40PLUS:
		son_phymode = IEEE80211_MODE_11NG_HT40PLUS;
		break;
	case WLAN_PHYMODE_11NG_HT40MINUS:
		son_phymode = IEEE80211_MODE_11NG_HT40MINUS;
		break;
	case WLAN_PHYMODE_11NG_HT40:
		son_phymode = IEEE80211_MODE_11NG_HT40;
		break;
	case WLAN_PHYMODE_11AC_VHT20:
	case WLAN_PHYMODE_11AC_VHT20_2G:
		son_phymode = IEEE80211_MODE_11AC_VHT20;
		break;
	case WLAN_PHYMODE_11AC_VHT40:
	case WLAN_PHYMODE_11AC_VHT40_2G:
		son_phymode = IEEE80211_MODE_11AC_VHT40;
		break;
	case WLAN_PHYMODE_11AC_VHT40PLUS_2G:
		son_phymode = IEEE80211_MODE_11AC_VHT40PLUS;
		break;
	case WLAN_PHYMODE_11AC_VHT40MINUS_2G:
		son_phymode = IEEE80211_MODE_11AC_VHT40MINUS;
		break;
	case WLAN_PHYMODE_11AC_VHT80:
	case WLAN_PHYMODE_11AC_VHT80_2G:
		son_phymode = IEEE80211_MODE_11AC_VHT80;
		break;
	case WLAN_PHYMODE_11AC_VHT160:
		son_phymode = IEEE80211_MODE_11AC_VHT160;
		break;
	case WLAN_PHYMODE_11AC_VHT80_80:
		son_phymode = IEEE80211_MODE_11AC_VHT80_80;
		break;
	case WLAN_PHYMODE_11AXA_HE20:
		son_phymode = IEEE80211_MODE_11AXA_HE20;
		break;
	case WLAN_PHYMODE_11AXG_HE20:
		son_phymode = IEEE80211_MODE_11AXG_HE20;
		break;
	case WLAN_PHYMODE_11AXA_HE40:
		son_phymode = IEEE80211_MODE_11AXA_HE40;
		break;
	case WLAN_PHYMODE_11AXG_HE40PLUS:
		son_phymode = IEEE80211_MODE_11AXG_HE40PLUS;
		break;
	case WLAN_PHYMODE_11AXG_HE40MINUS:
		son_phymode = IEEE80211_MODE_11AXG_HE40MINUS;
		break;
	case WLAN_PHYMODE_11AXG_HE40:
	case WLAN_PHYMODE_11AXG_HE80:
		son_phymode = IEEE80211_MODE_11AXG_HE40;
		break;
	case WLAN_PHYMODE_11AXA_HE80:
		son_phymode = IEEE80211_MODE_11AXA_HE80;
		break;
	case WLAN_PHYMODE_11AXA_HE160:
		son_phymode = IEEE80211_MODE_11AXA_HE160;
		break;
	case WLAN_PHYMODE_11AXA_HE80_80:
		son_phymode = IEEE80211_MODE_11AXA_HE80_80;
		break;
	default:
		son_phymode = IEEE80211_MODE_AUTO;
		break;
	}

	return son_phymode;
}

/**
 * hdd_son_get_phymode() - get son phy mode
 * @vdev: vdev
 *
 * Return: enum ieee80211_phymode
 */
static enum ieee80211_phymode hdd_son_get_phymode(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_channel *des_chan;

	if (!vdev) {
		hdd_err("null vdev");
		return IEEE80211_MODE_AUTO;
	}
	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	if (!des_chan) {
		hdd_err("null des_chan");
		return IEEE80211_MODE_AUTO;
	}

	return hdd_wlan_phymode_to_son_phymode(des_chan->ch_phymode);
}

/**
 * hdd_son_per_sta_len() - get sta information length
 * @sta_info: pointer to hdd_station_info
 *
 * TD: Certain IE may be provided for sta info
 *
 * Return: the size which is needed for given sta info
 */
static uint32_t hdd_son_per_sta_len(struct hdd_station_info *sta_info)
{
	return qdf_roundup(sizeof(struct ieee80211req_sta_info),
			   sizeof(uint32_t));
}

/**
 * hdd_son_get_sta_space() - how many space are needed for given vdev
 * @vdev: vdev
 *
 * Return: space needed for given vdev to provide sta info
 */
static uint32_t hdd_son_get_sta_space(struct wlan_objmgr_vdev *vdev)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	struct hdd_station_info *sta_info, *tmp = NULL;
	uint32_t space = 0;

	if (!vdev) {
		hdd_err("null vdev");
		return space;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return space;
	}

	adapter = link_info->adapter;
	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_SOFTAP_GET_STA_INFO) {
		if (!qdf_is_macaddr_broadcast(&sta_info->sta_mac))
			space += hdd_son_per_sta_len(sta_info);

		hdd_put_sta_info_ref(&adapter->sta_info_list,
				     &sta_info, true,
				     STA_INFO_SOFTAP_GET_STA_INFO);
	}
	hdd_debug("sta list space %u", space);

	return space;
}

/**
 * hdd_son_get_sta_list() - get connected station list
 * @vdev: vdev
 * @si: pointer to ieee80211req_sta_info
 * @space: space left
 *
 * Return: void
 */
static void hdd_son_get_sta_list(struct wlan_objmgr_vdev *vdev,
				 struct ieee80211req_sta_info *si,
				 uint32_t *space)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	struct hdd_station_info *sta_info, *tmp = NULL;
	uint32_t len;
	qdf_time_t current_ts;

	if (!vdev) {
		hdd_err("null vdev");
		return;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return;
	}

	adapter = link_info->adapter;
	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_SOFTAP_GET_STA_INFO) {
		if (!qdf_is_macaddr_broadcast(&sta_info->sta_mac)) {
			len = hdd_son_per_sta_len(sta_info);

			if (len > *space) {
				/* no more space if left */
				hdd_put_sta_info_ref(
					&adapter->sta_info_list,
					&sta_info, true,
					STA_INFO_SOFTAP_GET_STA_INFO);

				if (tmp)
					hdd_put_sta_info_ref(
						&adapter->sta_info_list,
						&tmp, true,
						STA_INFO_SOFTAP_GET_STA_INFO);

				hdd_err("space %u, length %u", *space, len);

				return;
			}

			qdf_mem_copy(si->isi_macaddr, &sta_info->sta_mac,
				     QDF_MAC_ADDR_SIZE);
			si->isi_ext_cap = sta_info->ext_cap;
			si->isi_beacon_measurement_support =
					!!(sta_info->capability &
					   WLAN_CAPABILITY_RADIO_MEASURE);
			si->isi_operating_bands = sta_info->supported_band;
			si->isi_assoc_time = sta_info->assoc_ts;
			current_ts = qdf_system_ticks();
			jiffies_to_timespec(current_ts - sta_info->assoc_ts,
					    &si->isi_tr069_assoc_time);
			si->isi_rssi = sta_info->rssi;
			si->isi_len = len;
			si->isi_ie_len = 0;
			hdd_debug("sta " QDF_MAC_ADDR_FMT " ext_cap 0x%x op band %u rssi %d len %u, assoc ts %lu, curr ts %lu rrm %d",
				  QDF_MAC_ADDR_REF(si->isi_macaddr),
				  si->isi_ext_cap, si->isi_operating_bands,
				  si->isi_rssi, si->isi_len, sta_info->assoc_ts,
				  current_ts,
				  si->isi_beacon_measurement_support);
			si = (struct ieee80211req_sta_info *)(((uint8_t *)si) +
			     len);
			*space -= len;
		}
		hdd_put_sta_info_ref(&adapter->sta_info_list,
				     &sta_info, true,
				     STA_INFO_SOFTAP_GET_STA_INFO);
	}
}

/**
 * hdd_son_set_acl_policy() - set son acl policy
 * @vdev: vdev
 * @son_acl_policy: enum ieee80211_acl_cmd
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS hdd_son_set_acl_policy(struct wlan_objmgr_vdev *vdev,
					 ieee80211_acl_cmd son_acl_policy)
{
	struct wlan_hdd_link_info *link_info;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct sap_context *sap_context;

	if (!vdev) {
		hdd_err("null vdev");
		return status;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return status;
	}

	sap_context = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	switch (son_acl_policy) {
	case IEEE80211_MACCMD_POLICY_OPEN:
		status = wlansap_set_acl_mode(sap_context, eSAP_ALLOW_ALL);
		break;
	case IEEE80211_MACCMD_POLICY_ALLOW:
		status = wlansap_set_acl_mode(sap_context,
					      eSAP_DENY_UNLESS_ACCEPTED);
		break;
	case IEEE80211_MACCMD_POLICY_DENY:
		status = wlansap_set_acl_mode(sap_context,
					      eSAP_ACCEPT_UNLESS_DENIED);
		break;
	case IEEE80211_MACCMD_FLUSH:
	case IEEE80211_MACCMD_DETACH:
		status = wlansap_clear_acl(sap_context);
		break;
	default:
		hdd_err("invalid son acl policy %d", son_acl_policy);
		break;
	}

	return status;
}

/**
 * hdd_acl_policy_to_son_acl_policy() - convert acl policy to son acl policy
 * @acl_policy: acl policy
 *
 * Return: son acl policy. enum ieee80211_acl_cmd
 */
static ieee80211_acl_cmd hdd_acl_policy_to_son_acl_policy(
						eSapMacAddrACL acl_policy)
{
	ieee80211_acl_cmd son_acl_policy = IEEE80211_MACCMD_DETACH;

	switch (acl_policy) {
	case eSAP_ACCEPT_UNLESS_DENIED:
		son_acl_policy = IEEE80211_MACCMD_POLICY_DENY;
		break;
	case eSAP_DENY_UNLESS_ACCEPTED:
		son_acl_policy = IEEE80211_MACCMD_POLICY_ALLOW;
		break;
	case eSAP_ALLOW_ALL:
		son_acl_policy = IEEE80211_MACCMD_POLICY_OPEN;
		break;
	default:
		hdd_err("invalid acl policy %d", acl_policy);
		break;
	}

	return son_acl_policy;
}

/**
 * hdd_son_get_acl_policy() - get son acl policy
 * @vdev: vdev
 *
 * Return: son acl policy. enum ieee80211_acl_cmd
 */
static ieee80211_acl_cmd hdd_son_get_acl_policy(struct wlan_objmgr_vdev *vdev)
{
	eSapMacAddrACL acl_policy;
	struct wlan_hdd_link_info *link_info;
	ieee80211_acl_cmd son_acl_policy = IEEE80211_MACCMD_DETACH;

	if (!vdev) {
		hdd_err("null vdev");
		return son_acl_policy;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return son_acl_policy;
	}

	wlansap_get_acl_mode(WLAN_HDD_GET_SAP_CTX_PTR(link_info), &acl_policy);

	son_acl_policy = hdd_acl_policy_to_son_acl_policy(acl_policy);

	return son_acl_policy;
}

/**
 * hdd_son_add_acl_mac() - add mac to access control list(ACL)
 * @vdev: vdev
 * @acl_mac: mac address to add
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_add_acl_mac(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr *acl_mac)
{
	eSapACLType list_type;
	QDF_STATUS qdf_status;
	eSapMacAddrACL acl_policy;
	struct wlan_hdd_link_info *link_info;
	struct sap_context *sap_context;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}
	if (!acl_mac) {
		hdd_err("null acl_mac");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}

	sap_context = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	wlansap_get_acl_mode(sap_context, &acl_policy);

	if (acl_policy == eSAP_ACCEPT_UNLESS_DENIED) {
		list_type = SAP_DENY_LIST;
	} else if (acl_policy == eSAP_DENY_UNLESS_ACCEPTED) {
		list_type = SAP_ALLOW_LIST;
	} else {
		hdd_err("Invalid ACL policy %d.", acl_policy);
		return -EINVAL;
	}
	qdf_status = wlansap_modify_acl(sap_context, acl_mac->bytes, list_type,
					ADD_STA_TO_ACL_NO_DEAUTH);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		hdd_err("Modify ACL failed");
		return -EIO;
	}

	return 0;
}

/**
 * hdd_son_del_acl_mac() - delete mac from acl
 * @vdev: vdev
 * @acl_mac: mac to remove
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_del_acl_mac(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr *acl_mac)
{
	eSapACLType list_type;
	QDF_STATUS qdf_status;
	eSapMacAddrACL acl_policy;
	struct wlan_hdd_link_info *link_info;
	struct sap_context *sap_ctx;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}
	if (!acl_mac) {
		hdd_err("null acl_mac");
		return -EINVAL;
	}

	lin_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	if (!sap_ctx) {
		hdd_err("null sap ctx");
		return -EINVAL;
	}

	wlansap_get_acl_mode(sap_ctx, &acl_policy);

	if (acl_policy == eSAP_ACCEPT_UNLESS_DENIED) {
		list_type = SAP_DENY_LIST;
	} else if (acl_policy == eSAP_DENY_UNLESS_ACCEPTED) {
		list_type = SAP_ALLOW_LIST;
	} else {
		hdd_err("Invalid ACL policy %d.", acl_policy);
		return -EINVAL;
	}
	qdf_status = wlansap_modify_acl(sap_ctx, acl_mac->bytes, list_type,
					DELETE_STA_FROM_ACL_NO_DEAUTH);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		hdd_err("Modify ACL failed");
		return -EIO;
	}

	return 0;
}

/**
 * hdd_son_kickout_mac() - kickout sta with given mac
 * @vdev: vdev
 * @mac: sta mac to kickout
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_kickout_mac(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr *mac)
{
	struct wlan_hdd_link_info *link_info;

	if (!vdev) {
		hdd_err("null vdev");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}

	if (mac)
		return wlan_hdd_del_station(link_info->adapter, mac->bytes);
	else
		return wlan_hdd_del_station(link_info->adapter, NULL);
}

static uint8_t hdd_son_get_rx_nss(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_hdd_link_info *link_info;
	uint8_t rx_nss = 0;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return 0;
	}

	hdd_get_rx_nss(link_info->adapter, &rx_nss);
	return rx_nss;
}

static void hdd_son_deauth_sta(struct wlan_objmgr_vdev *vdev,
			       uint8_t *peer_mac,
			       bool ignore_frame)
{
	struct wlan_hdd_link_info *link_info;
	struct csr_del_sta_params param;
	QDF_STATUS status;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return;
	}

	qdf_mem_copy(param.peerMacAddr.bytes, peer_mac, QDF_MAC_ADDR_SIZE);
	param.subtype = SIR_MAC_MGMT_DEAUTH;
	param.reason_code = ignore_frame ? REASON_HOST_TRIGGERED_SILENT_DEAUTH
					 : REASON_UNSPEC_FAILURE;
	hdd_debug("Peer - "QDF_MAC_ADDR_FMT" Ignore Frame - %u",
		  QDF_MAC_ADDR_REF(peer_mac), ignore_frame);

	status = hdd_softap_sta_deauth(link_info->adapter, &param);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Error in deauthenticating peer");
}

static void hdd_son_modify_acl(struct wlan_objmgr_vdev *vdev,
			       uint8_t *peer_mac,
			       bool allow_auth)
{
	QDF_STATUS status;
	struct sap_context *sap_context;
	struct wlan_hdd_link_info *link_info;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return;
	}

	hdd_debug("Peer - " QDF_MAC_ADDR_FMT " Allow Auth - %u",
		  QDF_MAC_ADDR_REF(peer_mac), allow_auth);

	sap_context = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	if (allow_auth) {
		status = wlansap_modify_acl(sap_context, peer_mac,
					    SAP_DENY_LIST, DELETE_STA_FROM_ACL);
		status = wlansap_modify_acl(sap_context, peer_mac,
					    SAP_ALLOW_LIST, ADD_STA_TO_ACL);
	} else {
		status = wlansap_modify_acl(sap_context, peer_mac,
					    SAP_ALLOW_LIST,
					    DELETE_STA_FROM_ACL);
		status = wlansap_modify_acl(sap_context, peer_mac,
					    SAP_DENY_LIST, ADD_STA_TO_ACL);
	}
}

static int hdd_son_send_cfg_event(struct wlan_objmgr_vdev *vdev,
				  uint32_t event_id,
				  uint32_t event_len,
				  const uint8_t *event_buf)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	uint32_t len;
	uint32_t idx;
	struct sk_buff *skb;

	if (!event_buf) {
		hdd_err("invalid event buf");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return -EINVAL;
	}

	adapter = link_info->adapter;
	len = nla_total_size(sizeof(event_id)) +
			nla_total_size(event_len) +
			NLMSG_HDRLEN;
	idx = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION_INDEX;
	skb = wlan_cfg80211_vendor_event_alloc(adapter->hdd_ctx->wiphy,
					       &adapter->wdev,
					       len, idx, GFP_KERNEL);
	if (!skb) {
		hdd_err("failed to alloc cfg80211 vendor event");
		return -EINVAL;
	}

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
			event_id)) {
		hdd_err("failed to put attr config generic command");
		wlan_cfg80211_vendor_free_skb(skb);
		return -EINVAL;
	}

	if (nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
		    event_len,
		    event_buf)) {
		hdd_err("failed to put attr config generic data");
		wlan_cfg80211_vendor_free_skb(skb);
		return -EINVAL;
	}

	wlan_cfg80211_vendor_event(skb, GFP_KERNEL);

	return 0;
}

static int hdd_son_deliver_opmode(struct wlan_objmgr_vdev *vdev,
				  uint32_t event_len,
				  const uint8_t *event_buf)
{
	return hdd_son_send_cfg_event(vdev,
				      QCA_NL80211_VENDOR_SUBCMD_OPMODE_UPDATE,
				      event_len,
				      event_buf);
}

static int hdd_son_deliver_smps(struct wlan_objmgr_vdev *vdev,
				uint32_t event_len,
				const uint8_t *event_buf)
{
	return hdd_son_send_cfg_event(vdev,
				      QCA_NL80211_VENDOR_SUBCMD_SMPS_UPDATE,
				      event_len,
				      event_buf);
}

/**
 * hdd_son_get_vdev_by_netdev() - get vdev from net device
 * @dev: struct net_device dev
 *
 * Return: vdev on success, NULL on failure
 */
static struct wlan_objmgr_vdev *
hdd_son_get_vdev_by_netdev(struct net_device *dev)
{
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;

	if (!dev)
		return NULL;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if (!adapter || (adapter && adapter->delete_in_progress))
		return NULL;

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_SON_ID);
	if (!vdev)
		return NULL;

	return vdev;
}

/**
 * son_trigger_vdev_obj_creation() - Trigger vdev object creation
 * @psoc: psoc object
 * @object: vdev object
 * @arg: component id
 *
 * Return: void
 */
static void
son_trigger_vdev_obj_creation(struct wlan_objmgr_psoc *psoc,
			      void *object, void *arg)
{
	QDF_STATUS ret;
	struct wlan_objmgr_vdev *vdev;
	enum wlan_umac_comp_id *id;

	vdev = object;
	id = arg;

	ret = wlan_objmgr_trigger_vdev_comp_priv_object_creation(vdev, *id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("vdev obj creation trigger failed");
}

/**
 * son_trigger_pdev_obj_creation() - Trigger pdev object creation
 * @psoc: psoc object
 * @object: pdev object
 * @arg: component id
 *
 * Return: void
 */
static void
son_trigger_pdev_obj_creation(struct wlan_objmgr_psoc *psoc,
			      void *object, void *arg)
{
	QDF_STATUS ret;
	struct wlan_objmgr_pdev *pdev;
	enum wlan_umac_comp_id *id;

	pdev = object;
	id = arg;

	ret = wlan_objmgr_trigger_pdev_comp_priv_object_creation(pdev, *id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("pdev obj creation trigger failed");
}

/**
 * son_trigger_pdev_obj_deletion() - Trigger pdev object deletion
 * @psoc: psoc object
 * @object: pdev object
 * @arg: component id
 *
 * Return: void
 */
static void
son_trigger_pdev_obj_deletion(struct wlan_objmgr_psoc *psoc,
			      void *object, void *arg)
{
	QDF_STATUS ret;
	struct wlan_objmgr_pdev *pdev;
	enum wlan_umac_comp_id *id;

	pdev = object;
	id = arg;

	ret = wlan_objmgr_trigger_pdev_comp_priv_object_deletion(pdev, *id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("pdev obj delete trigger failed");
}

/**
 * hdd_son_trigger_objmgr_object_creation() - Trigger objmgr object creation
 * @id: umac component id
 *
 * Return: QDF_STATUS_SUCCESS on success otherwise failure
 */
static QDF_STATUS
hdd_son_trigger_objmgr_object_creation(enum wlan_umac_comp_id id)
{
	QDF_STATUS ret;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_objmgr_get_psoc_by_id(0, WLAN_SON_ID);
	if (!psoc) {
		hdd_err("psoc null");
		return QDF_STATUS_E_INVAL;
	}

	ret = wlan_objmgr_trigger_psoc_comp_priv_object_creation(psoc, id);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("psoc object create trigger failed");
		goto out;
	}

	ret = wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
					   son_trigger_pdev_obj_creation,
					   &id, 0, id);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("pdev object create trigger failed");
		goto fail;
	}

	ret = wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
					   son_trigger_vdev_obj_creation,
					   &id, 0, id);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("vdev object create trigger failed");
		goto fail1;
	}
	wlan_objmgr_psoc_release_ref(psoc, WLAN_SON_ID);
	return ret;

fail1:
	ret = wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
					   son_trigger_pdev_obj_deletion,
					   &id, 0, id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("pdev object delete trigger failed");
fail:
	ret = wlan_objmgr_trigger_psoc_comp_priv_object_deletion(psoc, id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("psoc object delete trigger failed");
out:
	wlan_objmgr_psoc_release_ref(psoc, WLAN_SON_ID);
	return ret;
}

/**
 * son_trigger_peer_obj_deletion() - Trigger peer object deletion
 * @psoc: psoc object
 * @object: peer object
 * @arg: component id
 *
 * Return: void
 */
static void
son_trigger_peer_obj_deletion(struct wlan_objmgr_psoc *psoc,
			      void *object, void *arg)
{
	QDF_STATUS ret;
	struct wlan_objmgr_peer *peer;
	enum wlan_umac_comp_id *id;

	peer = object;
	id = arg;

	ret = wlan_objmgr_trigger_peer_comp_priv_object_deletion(peer, *id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("peer obj delete trigger failed");
}

/**
 * son_trigger_vdev_obj_deletion() - Trigger vdev object deletion
 * @psoc: psoc object
 * @object: vdev object
 * @arg: component id
 *
 * Return: void
 */
static void
son_trigger_vdev_obj_deletion(struct wlan_objmgr_psoc *psoc,
			      void *object, void *arg)
{
	QDF_STATUS ret;
	struct wlan_objmgr_vdev *vdev;
	enum wlan_umac_comp_id *id;

	vdev = object;
	id = arg;

	ret = wlan_objmgr_trigger_vdev_comp_priv_object_deletion(vdev, *id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("vdev obj deletion trigger failed");
}

/**
 * hdd_son_trigger_objmgr_object_deletion() - Trigger objmgr object deletion
 * @id: umac component id
 *
 * Return: QDF_STATUS_SUCCESS on success otherwise failure
 */
static QDF_STATUS
hdd_son_trigger_objmgr_object_deletion(enum wlan_umac_comp_id id)
{
	QDF_STATUS ret;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_objmgr_get_psoc_by_id(0, WLAN_SON_ID);
	if (!psoc) {
		hdd_err("psoc null");
		return QDF_STATUS_E_INVAL;
	}

	ret = wlan_objmgr_iterate_obj_list(psoc, WLAN_PEER_OP,
					   son_trigger_peer_obj_deletion,
					   &id, 0, id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("peer object deletion trigger failed");

	ret = wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
					   son_trigger_vdev_obj_deletion,
					   &id, 0, id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("vdev object deletion trigger failed");

	ret = wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
					   son_trigger_pdev_obj_deletion,
					   &id, 0, id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("pdev object delete trigger failed");

	ret = wlan_objmgr_trigger_psoc_comp_priv_object_deletion(psoc, id);
	if (QDF_IS_STATUS_ERROR(ret))
		hdd_err("psoc object delete trigger failed");

	wlan_objmgr_psoc_release_ref(psoc, WLAN_SON_ID);
	return ret;
}

/*
 * hdd_son_init_acs_channels() -initializes acs configs
 *
 * @adapter: pointer to hdd adapter
 * @hdd_ctx: pointer to hdd context
 * @acs_cfg: pointer to acs configs
 *
 * Return: QDF_STATUS_SUCCESS if ACS configuration is initialized,
 */
static QDF_STATUS hdd_son_init_acs_channels(struct hdd_adapter *adapter,
					    struct hdd_context *hdd_ctx,
					    struct sap_acs_cfg *acs_cfg)
{
	enum policy_mgr_con_mode pm_mode;
	uint32_t freq_list[NUM_CHANNELS], num_channels, i;

	if (!hdd_ctx || !acs_cfg) {
		hdd_err("Null pointer!!! hdd_ctx or acs_cfg");
		return QDF_STATUS_E_INVAL;
	}
	if (acs_cfg->freq_list) {
		hdd_debug("ACS config is already there, no need to init again");
		return QDF_STATUS_SUCCESS;
	}
	/* Setting ACS config */
	qdf_mem_zero(acs_cfg, sizeof(*acs_cfg));
	acs_cfg->ch_width = CH_WIDTH_20MHZ;
	policy_mgr_get_valid_chans(hdd_ctx->psoc, freq_list, &num_channels);
	/* convert and store channel to freq */
	if (!num_channels) {
		hdd_err("No Valid channel for ACS");
		return QDF_STATUS_E_INVAL;
	}
	acs_cfg->freq_list = qdf_mem_malloc(sizeof(*acs_cfg->freq_list) *
					    num_channels);
	if (!acs_cfg->freq_list) {
		hdd_err("Mem-alloc failed for acs_cfg->freq_list");
		return QDF_STATUS_E_NOMEM;
	}
	acs_cfg->master_freq_list =
			qdf_mem_malloc(sizeof(*acs_cfg->master_freq_list) *
				       num_channels);
	if (!acs_cfg->master_freq_list) {
		hdd_err("Mem-alloc failed for acs_cfg->master_freq_list");
		qdf_mem_free(acs_cfg->freq_list);
		acs_cfg->freq_list = NULL;
		return QDF_STATUS_E_NOMEM;
	}

	pm_mode =
	      policy_mgr_qdf_opmode_to_pm_con_mode(hdd_ctx->psoc,
						   adapter->device_mode,
						   adapter->deflink->vdev_id);
	/* convert channel to freq */
	for (i = 0; i < num_channels; i++) {
		acs_cfg->freq_list[i] = freq_list[i];
		acs_cfg->master_freq_list[i] = freq_list[i];
	}
	acs_cfg->ch_list_count = num_channels;
	acs_cfg->master_ch_list_count = num_channels;
	if (policy_mgr_is_force_scc(hdd_ctx->psoc) &&
	    policy_mgr_get_connection_count(hdd_ctx->psoc)) {
		policy_mgr_get_pcl(hdd_ctx->psoc, pm_mode,
				   acs_cfg->pcl_chan_freq,
				   &acs_cfg->pcl_ch_count,
				   acs_cfg->pcl_channels_weight_list,
				   NUM_CHANNELS, adapter->deflink->vdev_id);
		wlan_hdd_trim_acs_channel_list(acs_cfg->pcl_chan_freq,
					       acs_cfg->pcl_ch_count,
					       acs_cfg->freq_list,
					       &acs_cfg->ch_list_count);
		if (!acs_cfg->ch_list_count && acs_cfg->master_ch_list_count)
			wlan_hdd_handle_zero_acs_list
					       (hdd_ctx,
						acs_cfg->freq_list,
						&acs_cfg->ch_list_count,
						acs_cfg->master_freq_list,
						acs_cfg->master_ch_list_count);
	}
	acs_cfg->start_ch_freq = acs_cfg->freq_list[0];
	acs_cfg->end_ch_freq = acs_cfg->freq_list[acs_cfg->ch_list_count - 1];
	acs_cfg->hw_mode = eCSR_DOT11_MODE_abg;

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_son_start_acs() -Trigers ACS
 *
 * @vdev: pointer to object mgr vdev
 * @enable: True to trigger ACS
 *
 * Return: 0 on success
 */
static int hdd_son_start_acs(struct wlan_objmgr_vdev *vdev, uint8_t enable)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	struct sap_config *sap_config;
	struct hdd_context *hdd_ctx;

	if (!enable) {
		hdd_err("ACS Start report with disabled flag");
		return -EINVAL;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("Invalid VDEV %d", wlan_vdev_get_id(vdev));
		return -EINVAL;
	}

	adapter = link_info->adapter;
	if (adapter->device_mode != QDF_SAP_MODE) {
		hdd_err("Invalid device mode %d", adapter->device_mode);
		return -EINVAL;
	}
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("null hdd_ctx");
		return -EINVAL;
	}
	if (qdf_atomic_read(&link_info->session.ap.acs_in_progress)) {
		hdd_err("ACS is in-progress");
		return -EAGAIN;
	}
	wlan_hdd_undo_acs(link_info);
	sap_config = &link_info->session.ap.sap_config;
	hdd_debug("ACS Config country %s hw_mode %d ACS_BW: %d START_CH: %d END_CH: %d band %d",
		  hdd_ctx->reg.alpha2, sap_config->acs_cfg.hw_mode,
		  sap_config->acs_cfg.ch_width,
		  sap_config->acs_cfg.start_ch_freq,
		  sap_config->acs_cfg.end_ch_freq, sap_config->acs_cfg.band);
	sap_dump_acs_channel(&sap_config->acs_cfg);

	wlan_hdd_cfg80211_start_acs(link_info);

	return 0;
}

#define ACS_SET_CHAN_LIST_APPEND 0xFF
#define ACS_SNR_NEAR_RANGE_MIN 60
#define ACS_SNR_MID_RANGE_MIN 30
#define ACS_SNR_FAR_RANGE_MIN 0

/**
 * hdd_son_set_acs_channels() - Sets Channels for ACS
 *
 * @vdev: pointer to object mgr vdev
 * @req: target channels
 *
 * Return: 0 on success
 */
static int hdd_son_set_acs_channels(struct wlan_objmgr_vdev *vdev,
				    struct ieee80211req_athdbg *req)
{
	struct sap_config *sap_config;
	/* Append the new channels with existing channel list */
	bool append;
	/* Duplicate */
	bool dup;
	uint32_t freq_list[ACS_MAX_CHANNEL_COUNT];
	uint32_t num_channels;
	uint32_t chan_idx = 0;
	uint32_t tmp;
	uint16_t chan_start = 0;
	uint16_t i, j;
	uint16_t acs_chan_count = 0;
	uint32_t *prev_acs_list;
	struct ieee80211_chan_def *chans = req->data.user_chanlist.chans;
	uint16_t nchans = req->data.user_chanlist.n_chan;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	struct hdd_context *hdd_ctx;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info || !req) {
		hdd_err("null adapter or req");
		return -EINVAL;
	}

	adapter = link_info->adapter;
	if (adapter->device_mode != QDF_SAP_MODE) {
		hdd_err("Invalid device mode %d", adapter->device_mode);
		return -EINVAL;
	}
	if (!nchans) {
		hdd_err("No channels are sent to be set");
		return -EINVAL;
	}
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("null hdd_ctx");
		return -EINVAL;
	}
	sap_config = &link_info->session.ap.sap_config;
	/* initialize with default channels */
	if (hdd_son_init_acs_channels(adapter, hdd_ctx, &sap_config->acs_cfg)
						       != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to start the ACS");
		return -EAGAIN;
	}
	append = (chans[0].chan == ACS_SET_CHAN_LIST_APPEND);
	if (append) {
		chan_start = 1;
		acs_chan_count = sap_config->acs_cfg.ch_list_count;
	}
	prev_acs_list = sap_config->acs_cfg.freq_list;
	for (i = chan_start; i < nchans; i++) {
		tmp = wlan_reg_legacy_chan_to_freq(pdev, chans[i].chan);
		if (append) {
			for (j = 0; j < acs_chan_count; j++) {
				if (prev_acs_list[j] == tmp) {
					dup = true;
					break;
				}
			}
		}
		/* Remove duplicate */
		if (!dup) {
			freq_list[chan_idx] = tmp;
			chan_idx++;
		} else {
			dup = false;
		}
	}
	num_channels = chan_idx + acs_chan_count;
	sap_config->acs_cfg.ch_list_count = num_channels;
	sap_config->acs_cfg.freq_list =
			qdf_mem_malloc(num_channels *
				       sizeof(*sap_config->acs_cfg.freq_list));
	if (!sap_config->acs_cfg.freq_list) {
		hdd_err("Error in allocating memory, failed to set channels");
		sap_config->acs_cfg.freq_list = prev_acs_list;
		sap_config->acs_cfg.ch_list_count = acs_chan_count;
		return -ENOMEM;
	}
	if (append)
		qdf_mem_copy(sap_config->acs_cfg.freq_list, prev_acs_list,
			     sizeof(uint32_t) * acs_chan_count);
	qdf_mem_copy(&sap_config->acs_cfg.freq_list[acs_chan_count], freq_list,
		     sizeof(uint32_t) * chan_idx);
	qdf_mem_free(prev_acs_list);

	return 0;
}

static enum wlan_band_id
reg_wifi_band_to_wlan_band_id(enum reg_wifi_band reg_wifi_band)
{
	enum wlan_band_id wlan_band;
	const uint32_t reg_wifi_band_to_wlan_band_id_map[] = {
		[REG_BAND_2G] = WLAN_BAND_2GHZ,
		[REG_BAND_5G] = WLAN_BAND_5GHZ,
		[REG_BAND_6G] = WLAN_BAND_6GHZ,
		[REG_BAND_UNKNOWN] = WLAN_BAND_MAX,};

	wlan_band = reg_wifi_band_to_wlan_band_id_map[reg_wifi_band];
	if (wlan_band == WLAN_BAND_MAX) {
		hdd_err("Invalid wlan_band_id %d, reg_wifi_band: %d",
			wlan_band, reg_wifi_band);
		return -EINVAL;
	}

	return wlan_band;
}

/**
 * get_son_acs_report_values() - Gets ACS report for target channel
 *
 * @vdev: pointer to object mgr vdev
 * @acs_r: pointer to acs_dbg
 * @mac_handle: Handle to MAC
 * @chan_freq: Channel frequency
 *
 * Return: void
 */
static void get_son_acs_report_values(struct wlan_objmgr_vdev *vdev,
				      struct ieee80211_acs_dbg *acs_r,
				      mac_handle_t mac_handle,
				      uint16_t chan_freq)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct scan_filter *filter = qdf_mem_malloc(sizeof(*filter));
	struct scan_cache_node *cur_node;
	struct scan_cache_entry *se;
	enum ieee80211_phymode phymode_se;
	struct ieee80211_ie_hecap *hecap_ie;
	struct ieee80211_ie_srp_extie *srp_ie;
	qdf_list_node_t *cur_lst = NULL, *next_lst = NULL;
	uint32_t srps = 0;
	qdf_list_t *scan_list = NULL;
	uint8_t snr_se, *hecap_phy_ie;

	if (!filter)
		return;
	filter->num_of_channels = 1;
	filter->chan_freq_list[0] = chan_freq;
	scan_list = ucfg_scan_get_result(pdev, filter);
	acs_r->chan_nbss = qdf_list_size(scan_list);

	acs_r->chan_maxrssi = 0;
	acs_r->chan_minrssi = 0;
	acs_r->chan_nbss_near = 0;
	acs_r->chan_nbss_mid = 0;
	acs_r->chan_nbss_far = 0;
	acs_r->chan_nbss_srp = 0;
	qdf_list_peek_front(scan_list, &cur_lst);
	while (cur_lst) {
		qdf_list_peek_next(scan_list, cur_lst, &next_lst);
		cur_node = qdf_container_of(cur_lst,
					    struct scan_cache_node, node);
		se = cur_node->entry;
		snr_se = util_scan_entry_snr(se);
		hecap_ie = (struct ieee80211_ie_hecap *)
			   util_scan_entry_hecap(se);
		srp_ie = (struct ieee80211_ie_srp_extie *)
			 util_scan_entry_spatial_reuse_parameter(se);
		phymode_se = util_scan_entry_phymode(se);

		if (hecap_ie) {
			hecap_phy_ie = &hecap_ie->hecap_phyinfo[0];
			srps = hecap_phy_ie[HECAP_PHYBYTE_IDX7] &
			       HECAP_PHY_SRP_SR_BITS;
		}

		if (acs_r->chan_maxrssi < snr_se)
			acs_r->chan_maxrssi = snr_se;
		else if (acs_r->chan_minrssi > snr_se)
			acs_r->chan_minrssi = snr_se;
		if (snr_se > ACS_SNR_NEAR_RANGE_MIN)
			acs_r->chan_nbss_near += 1;
		else if (snr_se > ACS_SNR_MID_RANGE_MIN)
			acs_r->chan_nbss_mid += 1;
		else
			acs_r->chan_nbss_far += 1;
		if (srp_ie &&
		    (!(srp_ie->sr_control &
		       IEEE80211_SRP_SRCTRL_OBSS_PD_DISALLOWED_MASK) || srps))
			acs_r->chan_nbss_srp++;

		cur_lst = next_lst;
		next_lst = NULL;
	}
	acs_r->chan_80211_b_duration = sme_get_11b_data_duration(mac_handle,
								 chan_freq);
	acs_r->chan_nbss_eff = 100 + (acs_r->chan_nbss_near * 50)
				   + (acs_r->chan_nbss_mid * 50)
				   + (acs_r->chan_nbss_far * 25);
	acs_r->chan_srp_load = acs_r->chan_nbss_srp * 4;
	acs_r->chan_efficiency = (1000 + acs_r->chan_grade) /
				  acs_r->chan_nbss_eff;
	ucfg_scan_purge_results(scan_list);

	qdf_mem_free(filter);
}

/**
 * hdd_son_get_acs_report() - Gets ACS report
 *
 * @vdev: pointer to object mgr vdev
 * @acs_report: pointer to acs_dbg
 *
 * Return: 0 on success
 */
static int hdd_son_get_acs_report(struct wlan_objmgr_vdev *vdev,
				  struct ieee80211_acs_dbg *acs_report)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	uint8_t  acs_entry_id = 0;
	ACS_LIST_TYPE acs_type = 0;
	int ret = 0, i = 0;
	struct sap_acs_cfg *acs_cfg;
	struct hdd_context *hdd_ctx;
	struct ieee80211_acs_dbg *acs_r = NULL;
	struct sap_context *sap_ctx;
	qdf_size_t not_copied;

	if (!acs_report) {
		hdd_err("null acs_report");
		ret = -EINVAL;
		goto end;
	}

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		ret = -EINVAL;
		goto end;
	}

	adapter = link_info->adapter;
	if (adapter->device_mode != QDF_SAP_MODE) {
		hdd_err("Invalid device mode %d", adapter->device_mode);
		ret = -EINVAL;
		goto end;
	}
	if (hdd_son_is_acs_in_progress(vdev)) {
		acs_report->nchans = 0;
		hdd_err("ACS is in-progress");
		ret = -EAGAIN;
		goto end;
	}
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("null hdd_ctx");
		ret = -EINVAL;
		goto end;
	}
	acs_r = qdf_mem_malloc(sizeof(*acs_r));
	if (!acs_r) {
		hdd_err("Failed to allocate memory");
		ret = -ENOMEM;
		goto end;
	}
	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(link_info);
	acs_cfg = &link_info->session.ap.sap_config.acs_cfg;
	if (!acs_cfg->freq_list &&
	    (hdd_son_init_acs_channels(adapter, hdd_ctx,
				       acs_cfg) != QDF_STATUS_SUCCESS)) {
		hdd_err("Failed to start the ACS");
		ret = -EAGAIN;
		goto end_acs_r_free;
	}
	acs_r->nchans = acs_cfg->ch_list_count;
	ret = copy_from_user(&acs_entry_id, &acs_report->entry_id,
			     sizeof(acs_report->entry_id));
	hdd_debug("acs entry id: %u num of channels: %u",
		  acs_entry_id, acs_r->nchans);
	if (acs_entry_id > acs_r->nchans) {
		ret = -EINVAL;
		goto end_acs_r_free;
	}
	ret = copy_from_user(&acs_type, &acs_report->acs_type,
			     sizeof(acs_report->acs_type));

	acs_r->acs_status = ACS_DEFAULT;
	acs_r->chan_freq = acs_cfg->freq_list[acs_entry_id];
	acs_r->chan_band = reg_wifi_band_to_wlan_band_id
				(wlan_reg_freq_to_band(acs_r->chan_freq));
	hdd_debug("acs type: %d", acs_type);
	if (acs_type == ACS_CHAN_STATS) {
		acs_r->ieee_chan = wlan_reg_freq_to_chan(hdd_ctx->pdev,
							 acs_r->chan_freq);
		acs_r->chan_width = IEEE80211_CWM_WIDTH20;
		acs_r->channel_loading = 0;
		acs_r->chan_availability = 100;
		acs_r->chan_grade = 100; /* as hw_chan_grade is 100 in WIN 8 */
		acs_r->sec_chan = false;
		acs_r->chan_radar_noise =
		    wlansap_is_channel_in_nol_list(sap_ctx, acs_r->chan_freq,
						   PHY_SINGLE_CHANNEL_CENTERED);
		get_son_acs_report_values(vdev, acs_r, hdd_ctx->mac_handle,
					  acs_r->chan_freq);
		acs_r->chan_load = 0;
		acs_r->noisefloor = -254; /* NF_INVALID */
		for (i = 0; i < SIR_MAX_NUM_CHANNELS; i++) {
			if (hdd_ctx->chan_info[i].freq == acs_r->chan_freq) {
				acs_r->noisefloor =
					hdd_ctx->chan_info[i].noise_floor;
				acs_r->chan_load =
					hdd_ctx->chan_info[i].rx_clear_count;
				break;
			}
		}
		not_copied = copy_to_user(acs_report, acs_r, sizeof(*acs_r));
		if (not_copied)
			hdd_debug("%ul is not copied", not_copied);
	} else if (acs_type == ACS_CHAN_NF_STATS) {
	} else if (acs_type == ACS_NEIGHBOUR_GET_LIST_COUNT ||
		   acs_type == ACS_NEIGHBOUR_GET_LIST) {
	}
end_acs_r_free:
	qdf_mem_free(acs_r);
end:
	return ret;
}

static const uint8_t wlanphymode2ieeephymode[WLAN_PHYMODE_MAX] = {
	[WLAN_PHYMODE_AUTO] = IEEE80211_MODE_AUTO,
	[WLAN_PHYMODE_11A] = IEEE80211_MODE_11A,
	[WLAN_PHYMODE_11B] = IEEE80211_MODE_11B,
	[WLAN_PHYMODE_11G] = IEEE80211_MODE_11G,
	[WLAN_PHYMODE_11G_ONLY] = IEEE80211_MODE_11G,
	[WLAN_PHYMODE_11NA_HT20] = IEEE80211_MODE_11NA_HT20,
	[WLAN_PHYMODE_11NG_HT20] = IEEE80211_MODE_11NG_HT20,
	[WLAN_PHYMODE_11NA_HT40] = IEEE80211_MODE_11NA_HT40,
	[WLAN_PHYMODE_11NG_HT40PLUS] = IEEE80211_MODE_11NG_HT40PLUS,
	[WLAN_PHYMODE_11NG_HT40MINUS] = IEEE80211_MODE_11NG_HT40MINUS,
	[WLAN_PHYMODE_11NG_HT40] = IEEE80211_MODE_11NG_HT40,
	[WLAN_PHYMODE_11AC_VHT20] = IEEE80211_MODE_11AC_VHT20,
	[WLAN_PHYMODE_11AC_VHT20_2G] = IEEE80211_MODE_11AC_VHT20,
	[WLAN_PHYMODE_11AC_VHT40] = IEEE80211_MODE_11AC_VHT40,
	[WLAN_PHYMODE_11AC_VHT40PLUS_2G] = IEEE80211_MODE_11AC_VHT40PLUS,
	[WLAN_PHYMODE_11AC_VHT40MINUS_2G] = IEEE80211_MODE_11AC_VHT40MINUS,
	[WLAN_PHYMODE_11AC_VHT40_2G] = IEEE80211_MODE_11AC_VHT40,
	[WLAN_PHYMODE_11AC_VHT80] = IEEE80211_MODE_11AC_VHT80,
	[WLAN_PHYMODE_11AC_VHT80_2G] = IEEE80211_MODE_11AC_VHT80,
	[WLAN_PHYMODE_11AC_VHT160] = IEEE80211_MODE_11AC_VHT160,
	[WLAN_PHYMODE_11AC_VHT80_80] = IEEE80211_MODE_11AC_VHT80_80,
	[WLAN_PHYMODE_11AXA_HE20] = IEEE80211_MODE_11AXA_HE20,
	[WLAN_PHYMODE_11AXG_HE20] = IEEE80211_MODE_11AXG_HE20,
	[WLAN_PHYMODE_11AXA_HE40] = IEEE80211_MODE_11AXA_HE40,
	[WLAN_PHYMODE_11AXG_HE40PLUS] = IEEE80211_MODE_11AXG_HE40PLUS,
	[WLAN_PHYMODE_11AXG_HE40MINUS] = IEEE80211_MODE_11AXG_HE40MINUS,
	[WLAN_PHYMODE_11AXG_HE40] = IEEE80211_MODE_11AXG_HE40,
	[WLAN_PHYMODE_11AXA_HE80] = IEEE80211_MODE_11AXA_HE80,
	[WLAN_PHYMODE_11AXA_HE160] = IEEE80211_MODE_11AXA_HE160,
	[WLAN_PHYMODE_11AXA_HE80_80] = IEEE80211_MODE_11AXA_HE80_80,
#ifdef WLAN_FEATURE_11BE
	[WLAN_PHYMODE_11BEA_EHT20] = IEEE80211_MODE_11BEA_EHT20,
	[WLAN_PHYMODE_11BEG_EHT20] = IEEE80211_MODE_11BEG_EHT20,
	[WLAN_PHYMODE_11BEA_EHT40MINUS] = IEEE80211_MODE_11BEA_EHT40,
	[WLAN_PHYMODE_11BEG_EHT40PLUS] = IEEE80211_MODE_11BEG_EHT40PLUS,
	[WLAN_PHYMODE_11BEG_EHT40MINUS] = IEEE80211_MODE_11BEG_EHT40MINUS,
	[WLAN_PHYMODE_11BEG_EHT40] = IEEE80211_MODE_11BEG_EHT40,
	[WLAN_PHYMODE_11BEA_EHT80] = IEEE80211_MODE_11BEA_EHT80,
	[WLAN_PHYMODE_11BEG_EHT80] = 0,
	[WLAN_PHYMODE_11BEA_EHT160] = IEEE80211_MODE_11BEA_EHT160,
	[WLAN_PHYMODE_11BEA_EHT320] = IEEE80211_MODE_11BEA_EHT320,
#endif /* WLAN_FEATURE_11BE */
};

static enum ieee80211_phymode
wlan_hdd_son_get_ieee_phymode(enum wlan_phymode wlan_phymode)
{
	if (wlan_phymode >= WLAN_PHYMODE_MAX)
		return IEEE80211_MODE_AUTO;

	return wlanphymode2ieeephymode[wlan_phymode];
}

/**
 * hdd_son_get_peer_tx_rate() - Get peer tx rate from FW
 * @vdev: pointer to object mgr vdev
 * @peer_macaddr: peer mac address
 *
 * Return: tx rate in Kbps
 */
static uint32_t hdd_son_get_peer_tx_rate(struct wlan_objmgr_vdev *vdev,
					 uint8_t *peer_macaddr)
{
	uint32_t tx_rate = 0;
	struct stats_event *stats;
	int retval = 0;

	stats = wlan_cfg80211_mc_cp_stats_get_peer_stats(vdev,
							 peer_macaddr,
							 &retval);
	if (retval || !stats) {
		if (stats)
			wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
		hdd_err("Unable to get peer tx rate from fw");
		return tx_rate;
	}

	tx_rate = stats->peer_stats_info_ext->tx_rate;
	wlan_cfg80211_mc_cp_stats_free_stats_event(stats);

	return tx_rate;
}

static QDF_STATUS hdd_son_get_node_info_sta(struct wlan_objmgr_vdev *vdev,
					    uint8_t *mac_addr,
					    wlan_node_info *node_info)
{
	struct wlan_hdd_link_info *link_info;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info ||
	    wlan_hdd_validate_context(link_info->adapter->hdd_ctx))
		return QDF_STATUS_E_FAILURE;

	if (!hdd_cm_is_vdev_associated(link_info)) {
		hdd_debug_rl("STA VDEV not connected");
		/* Still return success and framework will see default stats */
		return QDF_STATUS_SUCCESS;
	}

	node_info->tx_bitrate = hdd_son_get_peer_tx_rate(vdev, mac_addr);
	/* convert it to Mbps */
	node_info->tx_bitrate = qdf_do_div(node_info->tx_bitrate, 1000);
	hdd_debug_rl("tx_bitrate %u", node_info->tx_bitrate);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS hdd_son_get_node_info_sap(struct wlan_objmgr_vdev *vdev,
					    uint8_t *mac_addr,
					    wlan_node_info *node_info)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	struct hdd_station_info *sta_info;
	enum wlan_phymode peer_phymode;
	struct wlan_objmgr_psoc *psoc;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_debug("NULL adapter");
		return QDF_STATUS_E_FAILURE;
	}

	adapter = link_info->adapter;
	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list, mac_addr,
					   STA_INFO_SON_GET_DATRATE_INFO);
	if (!sta_info) {
		hdd_err("Sta info is null");
		return QDF_STATUS_E_FAILURE;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		hdd_err("null psoc");
		return QDF_STATUS_E_FAILURE;
	}

	node_info->tx_bitrate = hdd_son_get_peer_tx_rate(vdev, mac_addr);
	/* convert it to Mbps */
	node_info->tx_bitrate = qdf_do_div(node_info->tx_bitrate, 1000);
	node_info->max_chwidth =
			hdd_chan_width_to_son_chwidth(sta_info->ch_width);
	node_info->num_streams = sta_info->nss;
	ucfg_mlme_get_peer_phymode(psoc, mac_addr, &peer_phymode);
	node_info->phymode = wlan_hdd_son_get_ieee_phymode(peer_phymode);
	node_info->max_txpower = ucfg_son_get_tx_power(sta_info->assoc_req_ies);
	node_info->max_MCS = sta_info->max_real_mcs_idx;
	if (node_info->max_MCS == INVALID_MCS_NSS_INDEX) {
		hdd_err("invalid mcs");
		return QDF_STATUS_E_FAILURE;
	}
	if (sta_info->vht_present)
		node_info->is_mu_mimo_supported =
				sta_info->vht_caps.vht_cap_info
				& IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE;
	if (sta_info->ht_present)
		node_info->is_static_smps = ((sta_info->ht_caps.cap_info
				& IEEE80211_HTCAP_C_SM_MASK) ==
				IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC);
	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SON_GET_DATRATE_INFO);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS hdd_son_get_node_info(struct wlan_objmgr_vdev *vdev,
					uint8_t *mac_addr,
					wlan_node_info *node_info)
{
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_debug("NULL adapter");
		return QDF_STATUS_E_FAILURE;
	}

	adapter = link_info->adapter;
	if (adapter->device_mode == QDF_STA_MODE)
		return hdd_son_get_node_info_sta(vdev, mac_addr, node_info);
	else if (adapter->device_mode == QDF_SAP_MODE)
		return hdd_son_get_node_info_sap(vdev, mac_addr, node_info);
	else
		return QDF_STATUS_SUCCESS;
}

static QDF_STATUS hdd_son_get_peer_capability(struct wlan_objmgr_vdev *vdev,
					      struct wlan_objmgr_peer *peer,
					      wlan_peer_cap *peer_cap)
{
	struct hdd_station_info *sta_info;
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;
	bool b_meas_supported;
	QDF_STATUS status;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return QDF_STATUS_E_FAILURE;
	}

	adapter = link_info->adapter;
	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   peer->macaddr,
					   STA_INFO_SOFTAP_GET_STA_INFO);
	if (!sta_info) {
		hdd_err("sta_info NULL");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_info("Getting peer capability from sta_info");
	qdf_mem_copy(peer_cap->bssid, vdev->vdev_mlme.macaddr,
		     QDF_MAC_ADDR_SIZE);
	peer_cap->is_BTM_Supported = !!(sta_info->ext_cap &
				   BIT(19/*BSS_TRANSITION*/));
	peer_cap->is_RRM_Supported = !!(sta_info->capability &
				   WLAN_CAPABILITY_RADIO_MEASURE);

	peer_cap->band_cap = sta_info->supported_band;
	if (sta_info->assoc_req_ies.len) {
		status = ucfg_son_get_peer_rrm_info(sta_info->assoc_req_ies,
						    peer_cap->rrmcaps,
						    &(b_meas_supported));
		if (status == QDF_STATUS_SUCCESS)
			peer_cap->is_beacon_meas_supported = b_meas_supported;
	}
	if (sta_info->ht_present)
		peer_cap->htcap = sta_info->ht_caps.cap_info;
	if (sta_info->vht_present)
		peer_cap->vhtcap = sta_info->vht_caps.vht_cap_info;

	qdf_mem_zero(&peer_cap->hecap, sizeof(wlan_client_he_capabilities));

	os_if_son_get_node_datarate_info(vdev, peer->macaddr,
					 &peer_cap->info);

	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_GET_STA_INFO);

	return QDF_STATUS_SUCCESS;
}

uint32_t hdd_son_get_peer_max_mcs_idx(struct wlan_objmgr_vdev *vdev,
				      struct wlan_objmgr_peer *peer)
{
	uint32_t ret = 0;
	struct hdd_station_info *sta_info = NULL;
	struct hdd_adapter *adapter;
	struct wlan_hdd_link_info *link_info;

	link_info = wlan_hdd_get_link_info_from_objmgr(vdev);
	if (!link_info) {
		hdd_err("null adapter");
		return ret;
	}

	adapter = link_info->adapter;
	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   peer->macaddr,
					   STA_INFO_SOFTAP_GET_STA_INFO);
	if (!sta_info) {
		hdd_err("sta_info NULL");
		return ret;
	}

	ret = sta_info->max_real_mcs_idx;

	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_GET_STA_INFO);
	hdd_debug("Peer " QDF_MAC_ADDR_FMT " max MCS index: %u",
		  QDF_MAC_ADDR_REF(peer->macaddr), ret);

	return ret;
}

/**
 * hdd_son_get_sta_stats() - get connected sta rssi and estimated data rate
 * @vdev: pointer to vdev
 * @mac_addr: connected sta mac addr
 * @stats: pointer to ieee80211_nodestats
 *
 * Return: 0 on success, negative errno on failure
 */
static int hdd_son_get_sta_stats(struct wlan_objmgr_vdev *vdev,
				 uint8_t *mac_addr,
				 struct ieee80211_nodestats *stats)
{
	struct stats_event *stats_info;
	int ret = 0;

	stats_info = wlan_cfg80211_mc_cp_stats_get_peer_rssi(
			vdev, mac_addr, &ret);
	if (ret || !stats_info) {
		hdd_err("get peer rssi fail");
		wlan_cfg80211_mc_cp_stats_free_stats_event(stats_info);
		return ret;
	}
	stats->ns_rssi = stats_info->peer_stats[0].peer_rssi;
	stats->ns_last_tx_rate = stats_info->peer_stats[0].tx_rate;
	stats->ns_last_rx_rate = stats_info->peer_stats[0].rx_rate;
	hdd_debug("sta " QDF_MAC_ADDR_FMT " rssi %d tx %u kbps, rx %u kbps",
		  QDF_MAC_ADDR_REF(mac_addr), stats->ns_rssi,
		  stats->ns_last_tx_rate,
		  stats->ns_last_rx_rate);
	wlan_cfg80211_mc_cp_stats_free_stats_event(stats_info);

	return ret;
}

void hdd_son_register_callbacks(struct hdd_context *hdd_ctx)
{
	struct son_callbacks cb_obj = {0};

	cb_obj.os_if_is_acs_in_progress = hdd_son_is_acs_in_progress;
	cb_obj.os_if_set_chan_ext_offset = hdd_son_set_chan_ext_offset;
	cb_obj.os_if_get_chan_ext_offset = hdd_son_get_chan_ext_offset;
	cb_obj.os_if_set_bandwidth = hdd_son_set_bandwidth;
	cb_obj.os_if_get_bandwidth = hdd_son_get_bandwidth;
	cb_obj.os_if_set_chan = hdd_son_set_chan;
	cb_obj.os_if_set_acl_policy = hdd_son_set_acl_policy;
	cb_obj.os_if_get_acl_policy = hdd_son_get_acl_policy;
	cb_obj.os_if_add_acl_mac = hdd_son_add_acl_mac;
	cb_obj.os_if_del_acl_mac = hdd_son_del_acl_mac;
	cb_obj.os_if_kickout_mac = hdd_son_kickout_mac;
	cb_obj.os_if_set_country_code = hdd_son_set_country;
	cb_obj.os_if_set_candidate_freq = hdd_son_set_candidate_freq;
	cb_obj.os_if_get_candidate_freq = hdd_son_get_candidate_freq;
	cb_obj.os_if_set_phymode = hdd_son_set_phymode;
	cb_obj.os_if_get_phymode = hdd_son_get_phymode;
	cb_obj.os_if_get_rx_nss = hdd_son_get_rx_nss;
	cb_obj.os_if_set_chwidth = hdd_son_set_chwidth;
	cb_obj.os_if_get_chwidth = hdd_son_get_chwidth;
	cb_obj.os_if_deauth_sta = hdd_son_deauth_sta;
	cb_obj.os_if_modify_acl = hdd_son_modify_acl;
	cb_obj.os_if_get_sta_list = hdd_son_get_sta_list;
	cb_obj.os_if_get_sta_space = hdd_son_get_sta_space;
	cb_obj.os_if_get_vdev_by_netdev = hdd_son_get_vdev_by_netdev;
	cb_obj.os_if_trigger_objmgr_object_creation =
				hdd_son_trigger_objmgr_object_creation;
	cb_obj.os_if_trigger_objmgr_object_deletion =
				hdd_son_trigger_objmgr_object_deletion;
	cb_obj.os_if_start_acs = hdd_son_start_acs;
	cb_obj.os_if_set_acs_channels = hdd_son_set_acs_channels;
	cb_obj.os_if_get_acs_report = hdd_son_get_acs_report;
	cb_obj.os_if_get_node_info = hdd_son_get_node_info;
	cb_obj.os_if_get_peer_capability = hdd_son_get_peer_capability;
	cb_obj.os_if_get_peer_max_mcs_idx = hdd_son_get_peer_max_mcs_idx;
	cb_obj.os_if_get_sta_stats = hdd_son_get_sta_stats;

	os_if_son_register_hdd_callbacks(hdd_ctx->psoc, &cb_obj);

	ucfg_son_register_deliver_opmode_cb(hdd_ctx->psoc,
					    hdd_son_deliver_opmode);
	ucfg_son_register_deliver_smps_cb(hdd_ctx->psoc,
					  hdd_son_deliver_smps);
}

int hdd_son_deliver_acs_complete_event(struct hdd_adapter *adapter)
{
	int ret = -EINVAL;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_SON_ID);
	if (!vdev) {
		hdd_err("null vdev");
		return ret;
	}
	ret = os_if_son_deliver_ald_event(vdev, NULL, MLME_EVENT_ACS_COMPLETE,
					  NULL);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);
	return ret;
}

int hdd_son_deliver_cac_status_event(struct hdd_adapter *adapter,
				     qdf_freq_t freq, bool radar_detected)
{
	int ret = -EINVAL;
	struct wlan_objmgr_vdev *vdev;
	struct son_ald_cac_info cac_info;

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_SON_ID);
	if (!vdev) {
		hdd_err("null vdev");
		return ret;
	}
	cac_info.freq = freq;
	cac_info.radar_detected = radar_detected;
	ret = os_if_son_deliver_ald_event(vdev, NULL, MLME_EVENT_CAC_STATUS,
					  &cac_info);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);

	return ret;
}

int hdd_son_deliver_assoc_disassoc_event(struct hdd_adapter *adapter,
					 struct qdf_mac_addr sta_mac,
					 uint32_t reason_code,
					 enum assoc_disassoc_event flag)
{
	int ret = -EINVAL;
	struct son_ald_assoc_event_info info;
	struct wlan_objmgr_vdev *vdev;

	qdf_mem_zero(&info, sizeof(info));
	memcpy(info.macaddr, &sta_mac.bytes, QDF_MAC_ADDR_SIZE);
	info.flag = flag;
	info.reason = reason_code;

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_SON_ID);
	if (!vdev) {
		hdd_err("null vdev");
		return ret;
	}
	ret = os_if_son_deliver_ald_event(vdev, NULL, MLME_EVENT_ASSOC_DISASSOC,
					  &info);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);
	return ret;
}

void hdd_son_deliver_peer_authorize_event(struct wlan_hdd_link_info *link_info,
					  uint8_t *peer_mac)
{
	struct wlan_objmgr_peer *peer;
	int ret;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;

	if (link_info->adapter->device_mode != QDF_SAP_MODE) {
		hdd_err("Non SAP vdev");
		return;
	}
	vdev = hdd_objmgr_get_vdev_by_user(link_info, WLAN_SON_ID);
	if (!vdev) {
		hdd_err("null vdev");
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		hdd_err("null psoc");
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);
		return;
	}
	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac, WLAN_UMAC_COMP_SON);
	if (!peer) {
		hdd_err("No peer object for sta" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_mac));
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);
		return;
	}

	ret = os_if_son_deliver_ald_event(vdev, peer,
					  MLME_EVENT_CLIENT_ASSOCIATED, NULL);
	if (ret)
		hdd_err("ALD ASSOCIATED Event failed for" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_mac));

	wlan_objmgr_peer_release_ref(peer, WLAN_UMAC_COMP_SON);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);
}

int hdd_son_deliver_chan_change_event(struct hdd_adapter *adapter,
				      qdf_freq_t freq)
{
	int ret = -EINVAL;
	struct wlan_objmgr_vdev *vdev;
	struct son_ald_chan_change_info chan_info;
	struct wlan_objmgr_pdev *pdev;

	if (!adapter) {
		hdd_err("null adapter");
		return ret;
	}
	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_SON_ID);
	if (!vdev) {
		hdd_err("null vdev");
		return ret;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		hdd_err("null pdev");
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);
		return ret;
	}
	chan_info.freq = freq;
	chan_info.chan_num = wlan_reg_freq_to_chan(pdev, freq);
	ret = os_if_son_deliver_ald_event(vdev, NULL,
					  MLME_EVENT_CHAN_CHANGE,
					  &chan_info);
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_SON_ID);

	return ret;
}

int hdd_son_send_set_wifi_generic_command(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct nlattr **tb)
{
	return os_if_son_parse_generic_nl_cmd(wiphy, wdev, tb,
					      OS_IF_SON_VENDOR_SET_CMD);
}

int hdd_son_send_get_wifi_generic_command(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  struct nlattr **tb)
{
	return os_if_son_parse_generic_nl_cmd(wiphy, wdev, tb,
					      OS_IF_SON_VENDOR_GET_CMD);
}
