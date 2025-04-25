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
 * DOC: os_if_son.c
 *
 * WLAN Host Device Driver file for son (Self Organizing Network)
 * support.
 *
 */

#include <os_if_son.h>
#include <qdf_trace.h>
#include <qdf_module.h>
#include <wlan_cfg80211.h>
#include <son_ucfg_api.h>
#include <wlan_dfs_ucfg_api.h>
#include <wlan_reg_ucfg_api.h>
#include <wlan_vdev_mgr_ucfg_api.h>
#include <wlan_mlme_ucfg_api.h>
#include <wlan_reg_services_api.h>
#include <wlan_scan_ucfg_api.h>
#include <wlan_dcs_ucfg_api.h>

static struct son_callbacks g_son_os_if_cb;
static struct wlan_os_if_son_ops g_son_os_if_txrx_ops;
static void (*os_if_son_ops_cb)(struct wlan_os_if_son_ops *son_ops);

void os_if_son_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				      struct son_callbacks *cb_obj)
{
	g_son_os_if_cb = *cb_obj;
}

qdf_freq_t os_if_son_get_freq(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;
	qdf_freq_t freq;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		osif_err("null pdev");
		return 0;
	}

	freq = ucfg_son_get_operation_chan_freq_vdev_id(pdev,
							wlan_vdev_get_id(vdev));
	osif_debug("vdev %d get freq %d", wlan_vdev_get_id(vdev), freq);

	return freq;
}
qdf_export_symbol(os_if_son_get_freq);

uint32_t os_if_son_is_acs_in_progress(struct wlan_objmgr_vdev *vdev)
{
	uint32_t acs_in_progress;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	acs_in_progress = g_son_os_if_cb.os_if_is_acs_in_progress(vdev);
	osif_debug("vdev %d acs_in_progress %d",
		   wlan_vdev_get_id(vdev), acs_in_progress);

	return acs_in_progress;
}
qdf_export_symbol(os_if_son_is_acs_in_progress);

uint32_t os_if_son_is_cac_in_progress(struct wlan_objmgr_vdev *vdev)
{
	uint32_t cac_in_progress;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	cac_in_progress = ucfg_son_is_cac_in_progress(vdev);
	osif_debug("vdev %d cac_in_progress %d",
		   wlan_vdev_get_id(vdev), cac_in_progress);

	return cac_in_progress;
}
qdf_export_symbol(os_if_son_is_cac_in_progress);

int os_if_son_set_chan_ext_offset(struct wlan_objmgr_vdev *vdev,
				  enum sec20_chan_offset son_chan_ext_offset)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	ret = g_son_os_if_cb.os_if_set_chan_ext_offset(vdev,
						       son_chan_ext_offset);
	osif_debug("vdev %d set_chan_ext_offset %d, ret %d",
		   wlan_vdev_get_id(vdev), son_chan_ext_offset, ret);

	return ret;
}
qdf_export_symbol(os_if_son_set_chan_ext_offset);

enum sec20_chan_offset os_if_son_get_chan_ext_offset(
						struct wlan_objmgr_vdev *vdev)
{
	enum sec20_chan_offset chan_ext_offset;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	chan_ext_offset = g_son_os_if_cb.os_if_get_chan_ext_offset(vdev);
	osif_debug("vdev %d chan_ext_offset %d",
		   wlan_vdev_get_id(vdev), chan_ext_offset);

	return chan_ext_offset;
}
qdf_export_symbol(os_if_son_get_chan_ext_offset);

int os_if_son_set_bandwidth(struct wlan_objmgr_vdev *vdev,
			    uint32_t son_bandwidth)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}

	ret = g_son_os_if_cb.os_if_set_bandwidth(vdev, son_bandwidth);
	osif_debug("vdev %d son_bandwidth %d ret %d",
		   wlan_vdev_get_id(vdev), son_bandwidth, ret);

	return ret;
}
qdf_export_symbol(os_if_son_set_bandwidth);

uint32_t os_if_son_get_bandwidth(struct wlan_objmgr_vdev *vdev)
{
	uint32_t bandwidth;

	if (!vdev) {
		osif_err("null vdev");
		return NONHT;
	}

	bandwidth = g_son_os_if_cb.os_if_get_bandwidth(vdev);
	osif_debug("vdev %d son_bandwidth %d",
		   wlan_vdev_get_id(vdev), bandwidth);

	return bandwidth;
}
qdf_export_symbol(os_if_son_get_bandwidth);

static uint32_t os_if_band_bitmap_to_son_band_info(
					uint32_t reg_wifi_band_bitmap)
{
	uint32_t son_band_info = FULL_BAND_RADIO;

	if (!(reg_wifi_band_bitmap & BIT(REG_BAND_5G)) &&
	    !(reg_wifi_band_bitmap & BIT(REG_BAND_6G)))
		return NON_5G_RADIO;
	if (reg_wifi_band_bitmap & BIT(REG_BAND_6G) &&
	    !(reg_wifi_band_bitmap & BIT(REG_BAND_2G)) &&
	    !(reg_wifi_band_bitmap & BIT(REG_BAND_5G)))
		return BAND_6G_RADIO;

	return son_band_info;
}

uint32_t os_if_son_get_band_info(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_pdev *pdev;
	uint32_t reg_wifi_band_bitmap;
	uint32_t band_info;

	if (!vdev) {
		osif_err("null vdev");
		return NO_BAND_INFORMATION_AVAILABLE;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		osif_err("null pdev");
		return NO_BAND_INFORMATION_AVAILABLE;
	}

	status = ucfg_reg_get_band(pdev, &reg_wifi_band_bitmap);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		osif_err("failed to get band");
		return NO_BAND_INFORMATION_AVAILABLE;
	}

	band_info = os_if_band_bitmap_to_son_band_info(reg_wifi_band_bitmap);
	osif_debug("vdev %d band_info %d",
		   wlan_vdev_get_id(vdev), band_info);

	return band_info;
}
qdf_export_symbol(os_if_son_get_band_info);

#define BW_WITHIN(min, bw, max) ((min) <= (bw) && (bw) <= (max))
/**
 * os_if_son_fill_chan_info() - fill chan info
 * @chan_info: chan info to fill
 * @chan_num: chan number
 * @primary_freq: chan frequency
 * @ch_num_seg1: channel number for segment 1
 * @ch_num_seg2: channel number for segment 2
 *
 * Return: void
 */
static void os_if_son_fill_chan_info(struct ieee80211_channel_info *chan_info,
				     uint8_t chan_num, qdf_freq_t primary_freq,
				     uint8_t ch_num_seg1, uint8_t ch_num_seg2)
{
	chan_info->ieee = chan_num;
	chan_info->freq = primary_freq;
	chan_info->vhtop_ch_num_seg1 = ch_num_seg1;
	chan_info->vhtop_ch_num_seg2 = ch_num_seg2;
}

/**
 * os_if_son_update_chan_info() - update chan info
 * @pdev: pdev
 * @flag_160: flag indicating the API to fill the center frequencies of 160MHz.
 * @cur_chan_list: pointer to regulatory_channel
 * @chan_info: chan info to fill
 * @half_and_quarter_rate_flags: half and quarter rate flags
 *
 * Return: void
 */
static void os_if_son_update_chan_info(
			struct wlan_objmgr_pdev *pdev, bool flag_160,
			struct regulatory_channel *cur_chan_list,
			struct ieee80211_channel_info *chan_info,
			uint64_t half_and_quarter_rate_flags)
{
	qdf_freq_t primary_freq = cur_chan_list->center_freq;
	struct ch_params chan_params = {0};

	if (!chan_info) {
		osif_err("null chan info");
		return;
	}
	if (cur_chan_list->chan_flags & REGULATORY_CHAN_NO_OFDM)
		chan_info->flags |=
			VENDOR_CHAN_FLAG2(QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_B);
	else
		chan_info->flags |= ucfg_son_get_chan_flag(pdev, primary_freq,
							   flag_160,
							   &chan_params);
	if (cur_chan_list->chan_flags & REGULATORY_CHAN_RADAR) {
		chan_info->flags_ext |=
			QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_DFS;
		chan_info->flags_ext |=
			QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_DISALLOW_ADHOC;
		chan_info->flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_PASSIVE;
	} else if (cur_chan_list->chan_flags & REGULATORY_CHAN_NO_IR) {
		/* For 2Ghz passive channels. */
		chan_info->flags |= QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_PASSIVE;
	}

	if (WLAN_REG_IS_6GHZ_PSC_CHAN_FREQ(primary_freq))
		chan_info->flags_ext |=
			QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_EXT_PSC;

	os_if_son_fill_chan_info(chan_info, cur_chan_list->chan_num,
				 primary_freq,
				 chan_params.center_freq_seg0,
				 chan_params.center_freq_seg1);
}

int os_if_son_get_chan_list(struct wlan_objmgr_vdev *vdev,
			    struct ieee80211_ath_channel *chan_list,
			    struct ieee80211_channel_info *chan_info,
			    uint8_t *nchans, bool flag_160, bool flag_6ghz)
{
	struct regulatory_channel *cur_chan_list;
	int i;
	uint32_t phybitmap;
	uint32_t reg_wifi_band_bitmap;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct regulatory_channel *chan;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}

	if (!chan_info) {
		osif_err("null chan info");
		return -EINVAL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		osif_err("null pdev");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("null psoc");
		return -EINVAL;
	}

	status = ucfg_reg_get_band(pdev, &reg_wifi_band_bitmap);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		osif_err("failed to get band");
		return -EINVAL;
	}

	cur_chan_list = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(struct regulatory_channel));
	if (!cur_chan_list) {
		osif_err("cur_chan_list allocation fails");
		return -EINVAL;
	}

	if (wlan_reg_get_current_chan_list(
	    pdev, cur_chan_list) != QDF_STATUS_SUCCESS) {
		qdf_mem_free(cur_chan_list);
		osif_err("fail to get current chan list");
		return -EINVAL;
	}

	ucfg_reg_get_band(pdev, &phybitmap);

	for (i = 0; i < NUM_CHANNELS; i++) {
		uint64_t band_flags;
		qdf_freq_t primary_freq = cur_chan_list[i].center_freq;
		uint64_t half_and_quarter_rate_flags = 0;

		chan = &cur_chan_list[i];
		if ((chan->chan_flags & REGULATORY_CHAN_DISABLED) &&
		    chan->state == CHANNEL_STATE_DISABLE &&
		    !chan->nol_chan && !chan->nol_history)
			continue;
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(primary_freq)) {
			if (!flag_6ghz ||
			    !(reg_wifi_band_bitmap & BIT(REG_BAND_6G)))
				continue;
			band_flags = VENDOR_CHAN_FLAG2(
				QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_6GHZ);
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(primary_freq)) {
			if (!(reg_wifi_band_bitmap & BIT(REG_BAND_2G)))
				continue;
			band_flags = QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_2GHZ;
		} else if (WLAN_REG_IS_5GHZ_CH_FREQ(primary_freq)) {
			if (!(reg_wifi_band_bitmap & BIT(REG_BAND_5G)))
				continue;
			band_flags = QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_5GHZ;
		} else if (WLAN_REG_IS_49GHZ_FREQ(primary_freq)) {
			if (!(reg_wifi_band_bitmap & BIT(REG_BAND_5G)))
				continue;
			band_flags = QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_5GHZ;
			/**
			 * If 4.9G Half and Quarter rates are supported
			 * by the channel, update them as separate entries
			 * to the list
			 */
			if (BW_WITHIN(chan->min_bw, BW_10_MHZ, chan->max_bw)) {
				os_if_son_fill_chan_info(&chan_info[*nchans],
							 chan->chan_num,
							 primary_freq, 0, 0);
				chan_info[*nchans].flags |=
					QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_HALF;
				chan_info[*nchans].flags |=
					VENDOR_CHAN_FLAG2(
					QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_A);
				half_and_quarter_rate_flags =
					chan_info[*nchans].flags;
				if (++(*nchans) >= IEEE80211_CHAN_MAX)
					break;
			}
			if (BW_WITHIN(chan->min_bw, BW_5_MHZ, chan->max_bw)) {
				os_if_son_fill_chan_info(&chan_info[*nchans],
							 chan->chan_num,
							 primary_freq, 0, 0);
				chan_info[*nchans].flags |=
				    QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_QUARTER;
				chan_info[*nchans].flags |=
					VENDOR_CHAN_FLAG2(
					QCA_WLAN_VENDOR_CHANNEL_PROP_FLAG_A);
				half_and_quarter_rate_flags =
					chan_info[*nchans].flags;
				if (++(*nchans) >= IEEE80211_CHAN_MAX)
					break;
			}
		} else {
			continue;
		}

		os_if_son_update_chan_info(pdev, flag_160, chan,
					   &chan_info[*nchans],
					   half_and_quarter_rate_flags);

		if (++(*nchans) >= IEEE80211_CHAN_MAX)
			break;
	}

	qdf_mem_free(cur_chan_list);
	osif_debug("vdev %d channel_info exit", wlan_vdev_get_id(vdev));

	return 0;
}
qdf_export_symbol(os_if_son_get_chan_list);

uint32_t os_if_son_get_sta_count(struct wlan_objmgr_vdev *vdev)
{
	uint32_t sta_count;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	sta_count = ucfg_son_get_sta_count(vdev);
	osif_debug("vdev %d sta count %d", wlan_vdev_get_id(vdev), sta_count);

	return sta_count;
}
qdf_export_symbol(os_if_son_get_sta_count);

int os_if_son_get_bssid(struct wlan_objmgr_vdev *vdev,
			uint8_t bssid[QDF_MAC_ADDR_SIZE])
{
	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}

	ucfg_wlan_vdev_mgr_get_param_bssid(vdev, bssid);
	osif_debug("vdev %d bssid " QDF_MAC_ADDR_FMT,
		   wlan_vdev_get_id(vdev), QDF_MAC_ADDR_REF(bssid));

	return 0;
}
qdf_export_symbol(os_if_son_get_bssid);

int os_if_son_get_ssid(struct wlan_objmgr_vdev *vdev,
		       char ssid[WLAN_SSID_MAX_LEN + 1],
		       uint8_t *ssid_len)
{
	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}

	ucfg_wlan_vdev_mgr_get_param_ssid(vdev, ssid, ssid_len);
	osif_debug("vdev %d ssid " QDF_SSID_FMT,
		   wlan_vdev_get_id(vdev),
		   QDF_SSID_REF(*ssid_len, ssid));

	return 0;
}
qdf_export_symbol(os_if_son_get_ssid);

int os_if_son_set_chan(struct wlan_objmgr_vdev *vdev,
		       int chan, enum wlan_band_id son_band)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}

	ret = g_son_os_if_cb.os_if_set_chan(vdev, chan, son_band);
	osif_debug("vdev %d chan %d son_band %d", wlan_vdev_get_id(vdev),
		   chan, son_band);

	return ret;
}
qdf_export_symbol(os_if_son_set_chan);

int os_if_son_set_cac_timeout(struct wlan_objmgr_vdev *vdev,
			      int cac_timeout)
{
	struct wlan_objmgr_pdev *pdev;
	int status;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		osif_err("null pdev");
		return -EINVAL;
	}

	if (QDF_IS_STATUS_ERROR(ucfg_dfs_override_cac_timeout(
		pdev, cac_timeout, &status))) {
		osif_err("cac timeout override fails");
		return -EINVAL;
	}
	osif_debug("vdev %d cac_timeout %d status %d",
		   wlan_vdev_get_id(vdev), cac_timeout, status);

	return status;
}
qdf_export_symbol(os_if_son_set_cac_timeout);

int os_if_son_get_cac_timeout(struct wlan_objmgr_vdev *vdev,
			      int *cac_timeout)
{
	struct wlan_objmgr_pdev *pdev;
	int status;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		osif_err("null pdev");
		return -EINVAL;
	}

	if (QDF_IS_STATUS_ERROR(ucfg_dfs_get_override_cac_timeout(
		pdev, cac_timeout, &status))) {
		osif_err("fails to get cac timeout");
		return -EINVAL;
	}
	osif_debug("vdev %d cac_timeout %d status %d",
		   wlan_vdev_get_id(vdev), *cac_timeout, status);

	return status;
}
qdf_export_symbol(os_if_son_get_cac_timeout);

int os_if_son_set_country_code(struct wlan_objmgr_vdev *vdev,
			       char *country_code)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	ret = g_son_os_if_cb.os_if_set_country_code(vdev, country_code);
	osif_debug("vdev %d country_code %s ret %d",
		   wlan_vdev_get_id(vdev), country_code, ret);

	return ret;
}
qdf_export_symbol(os_if_son_set_country_code);

int os_if_son_get_country_code(struct wlan_objmgr_vdev *vdev,
			       char *country_code)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("null psoc");
		return -EINVAL;
	}
	status = ucfg_reg_get_current_country(psoc, country_code);
	osif_debug("vdev %d country_code %s status %d",
		   wlan_vdev_get_id(vdev), country_code, status);

	return qdf_status_to_os_return(status);
}
qdf_export_symbol(os_if_son_get_country_code);

int os_if_son_set_candidate_freq(struct wlan_objmgr_vdev *vdev,
				 qdf_freq_t freq)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}

	ret = g_son_os_if_cb.os_if_set_candidate_freq(vdev, freq);
	osif_debug("vdev %d set_candidate_freq %d ret %d",
		   wlan_vdev_get_id(vdev), freq, ret);

	return ret;
}
qdf_export_symbol(os_if_son_set_candidate_freq);

qdf_freq_t os_if_son_get_candidate_freq(struct wlan_objmgr_vdev *vdev)
{
	qdf_freq_t freq;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	freq = g_son_os_if_cb.os_if_get_candidate_freq(vdev);
	osif_debug("vdev %d candidate_freq %d",
		   wlan_vdev_get_id(vdev), freq);

	return freq;
}
qdf_export_symbol(os_if_son_get_candidate_freq);

QDF_STATUS os_if_son_set_acl_policy(struct wlan_objmgr_vdev *vdev,
				    ieee80211_acl_cmd son_acl_policy)
{
	QDF_STATUS ret;

	if (!vdev) {
		osif_err("null vdev");
		return QDF_STATUS_E_INVAL;
	}

	ret = g_son_os_if_cb.os_if_set_acl_policy(vdev, son_acl_policy);
	osif_debug("set acl policy %d status %d", son_acl_policy, ret);

	return ret;
}
qdf_export_symbol(os_if_son_set_acl_policy);

ieee80211_acl_cmd os_if_son_get_acl_policy(struct wlan_objmgr_vdev *vdev)
{
	ieee80211_acl_cmd son_acl_policy;

	if (!vdev) {
		osif_err("null vdev");
		return IEEE80211_MACCMD_DETACH;
	}
	son_acl_policy = g_son_os_if_cb.os_if_get_acl_policy(vdev);
	osif_debug("get acl policy %d", son_acl_policy);

	return son_acl_policy;
}
qdf_export_symbol(os_if_son_get_acl_policy);

int os_if_son_add_acl_mac(struct wlan_objmgr_vdev *vdev,
			  struct qdf_mac_addr *acl_mac)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	ret = g_son_os_if_cb.os_if_add_acl_mac(vdev, acl_mac);
	osif_debug("add_acl_mac " QDF_MAC_ADDR_FMT " ret %d",
		   QDF_MAC_ADDR_REF(acl_mac->bytes), ret);

	return ret;
}
qdf_export_symbol(os_if_son_add_acl_mac);

int os_if_son_del_acl_mac(struct wlan_objmgr_vdev *vdev,
			  struct qdf_mac_addr *acl_mac)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	ret = g_son_os_if_cb.os_if_del_acl_mac(vdev, acl_mac);
	osif_debug("del_acl_mac " QDF_MAC_ADDR_FMT " ret %d",
		   QDF_MAC_ADDR_REF(acl_mac->bytes), ret);

	return ret;
}
qdf_export_symbol(os_if_son_del_acl_mac);

int os_if_son_kickout_mac(struct wlan_objmgr_vdev *vdev,
			  struct qdf_mac_addr *mac)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	ret = g_son_os_if_cb.os_if_kickout_mac(vdev, mac);
	osif_debug("kickout mac " QDF_MAC_ADDR_FMT " ret %d",
		   QDF_MAC_ADDR_REF(mac->bytes), ret);

	return ret;
}
qdf_export_symbol(os_if_son_kickout_mac);

uint8_t os_if_son_get_chan_util(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_host_dcs_ch_util_stats dcs_son_stats = {};
	struct wlan_objmgr_psoc *psoc;
	uint8_t mac_id;
	QDF_STATUS status;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("null psoc");
		return 0;
	}
	status = policy_mgr_get_mac_id_by_session_id(psoc,
						     wlan_vdev_get_id(vdev),
						     &mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to get mac_id");
		return 0;
	}

	ucfg_dcs_get_ch_util(psoc, mac_id, &dcs_son_stats);
	osif_debug("get_chan_util %d", dcs_son_stats.total_cu);

	return dcs_son_stats.total_cu;
}
qdf_export_symbol(os_if_son_get_chan_util);

void os_if_son_get_phy_stats(struct wlan_objmgr_vdev *vdev,
			     struct ol_ath_radiostats *phy_stats)
{
	struct wlan_host_dcs_ch_util_stats dcs_son_stats = {};
	struct wlan_objmgr_psoc *psoc;
	uint8_t mac_id;
	QDF_STATUS status;

	if (!vdev) {
		osif_err("null vdev");
		return;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("null psoc");
		return;
	}
	status = policy_mgr_get_mac_id_by_session_id(psoc,
						     wlan_vdev_get_id(vdev),
						     &mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Failed to get mac_id");
		return;
	}

	ucfg_dcs_get_ch_util(psoc, mac_id, &dcs_son_stats);

	phy_stats->ap_rx_util = dcs_son_stats.rx_cu;
	phy_stats->ap_tx_util = dcs_son_stats.tx_cu;
	phy_stats->obss_rx_util = dcs_son_stats.obss_rx_cu;
	if (dcs_son_stats.total_cu < 100)
		phy_stats->free_medium = 100 - dcs_son_stats.total_cu;
	else
		phy_stats->free_medium = 0;
	phy_stats->chan_nf = dcs_son_stats.chan_nf;
	osif_debug("rx_util %d tx_util %d obss_rx_util %d free_medium %d noise floor %d",
		   phy_stats->ap_rx_util, phy_stats->ap_tx_util,
		   phy_stats->obss_rx_util, phy_stats->free_medium,
		   phy_stats->chan_nf);
}
qdf_export_symbol(os_if_son_get_phy_stats);

int os_if_son_cbs_init(void)
{
	int ret;

	ret = ucfg_son_cbs_init();

	return ret;
}

qdf_export_symbol(os_if_son_cbs_init);

int os_if_son_cbs_deinit(void)
{
	int ret;

	ret = ucfg_son_cbs_deinit();

	return ret;
}

qdf_export_symbol(os_if_son_cbs_deinit);

int os_if_son_set_cbs(struct wlan_objmgr_vdev *vdev,
		      bool enable)
{
	int ret;

	ret = ucfg_son_set_cbs(vdev, enable);

	return ret;
}

qdf_export_symbol(os_if_son_set_cbs);

int os_if_son_set_cbs_wait_time(struct wlan_objmgr_vdev *vdev,
				uint32_t val)
{
	int ret;

	ret = ucfg_son_set_cbs_wait_time(vdev, val);

	return ret;
}

qdf_export_symbol(os_if_son_set_cbs_wait_time);

int os_if_son_set_cbs_dwell_split_time(struct wlan_objmgr_vdev *vdev,
				       uint32_t val)
{
	int ret;

	ret = ucfg_son_set_cbs_dwell_split_time(vdev, val);

	return ret;
}

qdf_export_symbol(os_if_son_set_cbs_dwell_split_time);

int os_if_son_set_phymode(struct wlan_objmgr_vdev *vdev,
			  enum ieee80211_phymode mode)
{
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	ret = g_son_os_if_cb.os_if_set_phymode(vdev, mode);
	osif_debug("vdev %d phymode %d ret %d",
		   wlan_vdev_get_id(vdev), mode, ret);

	return ret;
}
qdf_export_symbol(os_if_son_set_phymode);

enum ieee80211_phymode os_if_son_get_phymode(struct wlan_objmgr_vdev *vdev)
{
	enum ieee80211_phymode phymode;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	phymode = g_son_os_if_cb.os_if_get_phymode(vdev);
	osif_debug("vdev %d phymode %d",
		   wlan_vdev_get_id(vdev), phymode);

	return phymode;
}
qdf_export_symbol(os_if_son_get_phymode);

static QDF_STATUS os_if_son_get_apcap(struct wlan_objmgr_vdev *vdev,
				      wlan_ap_cap *apcap)
{
	uint32_t num_rx_streams = 0;
	uint32_t num_tx_streams = 0;
	uint32_t  value;
	struct mlme_ht_capabilities_info ht_cap_info;
	struct wlan_objmgr_psoc *psoc;
	tDot11fIEhe_cap he_cap = {0};
	bool enabled;
	QDF_STATUS status;
	int32_t vht_caps = 0;

	/* Number of supported tx and rx streams */
	status = ucfg_son_vdev_get_supported_txrx_streams(vdev, &num_tx_streams,
							  &num_rx_streams);
	if (status != QDF_STATUS_SUCCESS) {
		osif_err("Could not get txrx streams");
		return status;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* Fetch HT CAP */
	status = ucfg_mlme_get_ht_cap_info(psoc, &ht_cap_info);
	if (status == QDF_STATUS_SUCCESS) {
		apcap->wlan_ap_ht_capabilities_valid = true;
		qdf_mem_copy(&apcap->htcap.htcap, &ht_cap_info,
			     sizeof(struct mlme_ht_capabilities_info));
		apcap->htcap.max_tx_nss = num_tx_streams;
		apcap->htcap.max_rx_nss = num_rx_streams;
	}

	/* Fetch VHT CAP */
	status = ucfg_mlme_get_vht_enable2x2(psoc, &enabled);
	if (enabled) {
		apcap->wlan_ap_vht_capabilities_valid = 1;
		ucfg_mlme_cfg_get_vht_tx_mcs_map(psoc, &value);
		apcap->vhtcap.supp_tx_mcs = value;
		ucfg_mlme_cfg_get_vht_rx_mcs_map(psoc, &value);
		apcap->vhtcap.supp_rx_mcs = value;
		apcap->vhtcap.max_tx_nss = num_tx_streams;
		apcap->vhtcap.max_rx_nss = num_rx_streams;
		if (ucfg_son_get_vht_cap(psoc, &vht_caps) == QDF_STATUS_SUCCESS)
			apcap->vhtcap.vhtcap = vht_caps;
	}

	/* Fetch HE CAP */
	ucfg_mlme_cfg_get_he_caps(psoc, &he_cap);
	if (he_cap.present) {
		apcap->wlan_ap_he_capabilities_valid = 1;
		apcap->hecap.num_mcs_entries = MAP_MAX_HE_MCS;
		apcap->hecap.max_tx_nss = num_tx_streams;
		apcap->hecap.max_rx_nss = num_rx_streams;
		apcap->hecap.he_su_ppdu_1x_ltf_800ns_gi =
					he_cap.he_1x_ltf_800_gi_ppdu;
		apcap->hecap.he_ndp_4x_ltf_3200ns_gi =
					he_cap.he_4x_ltf_3200_gi_ndp;
		apcap->hecap.he_su_bfer = he_cap.su_beamformer;
		apcap->hecap.he_su_bfee = he_cap.su_beamformee;
		apcap->hecap.he_mu_bfer = he_cap.mu_beamformer;
		apcap->hecap.supported_he_mcs[0] = he_cap.rx_he_mcs_map_lt_80;
		apcap->hecap.supported_he_mcs[1] = he_cap.tx_he_mcs_map_lt_80;
		apcap->hecap.supported_he_mcs[2] =
					he_cap.rx_he_mcs_map_160[0][0] |
					(he_cap.rx_he_mcs_map_160[0][1] << 8);
		apcap->hecap.supported_he_mcs[3] =
					he_cap.tx_he_mcs_map_160[0][0] |
					(he_cap.tx_he_mcs_map_160[0][1] << 8);
		apcap->hecap.supported_he_mcs[4] =
					he_cap.rx_he_mcs_map_80_80[0][0] |
					(he_cap.rx_he_mcs_map_80_80[0][1] << 8);
		apcap->hecap.supported_he_mcs[5] =
					he_cap.tx_he_mcs_map_80_80[0][0] |
					(he_cap.tx_he_mcs_map_80_80[0][1] << 8);
		apcap->hecap.he_ul_mumimo = QDF_GET_BITS(he_cap.ul_mu, 0, 1);
		apcap->hecap.he_ul_muofdma = QDF_GET_BITS(he_cap.ul_mu, 1, 1);
		apcap->hecap.he_dl_muofdma = he_cap.dl_mu_mimo_part_bw;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS os_if_son_vdev_ops(struct wlan_objmgr_vdev *vdev,
			      enum wlan_mlme_vdev_param type,
			      void *data, void *ret)
{
	union wlan_mlme_vdev_data *in = (union wlan_mlme_vdev_data *)data;
	union wlan_mlme_vdev_data *out = (union wlan_mlme_vdev_data *)ret;

	if (!vdev)
		return QDF_STATUS_E_INVAL;
	switch (type) {
	case VDEV_SET_IE:
		break;
	case VDEV_CLR_IE:
		break;
	case VDEV_SET_ACL:
		break;
	case VDEV_CLR_ACL:
		break;
	case VDEV_SET_ACL_TIMER:
		break;
	case VDEV_SET_PEER_ACT_STATS:
		break;
	case VDEV_SET_SEC_STA_WDS:
		break;
	case VDEV_SET_MEC:
		break;
	case VDEV_SET_MBO_IE_BSTM:
		break;
	case VDEV_SET_WPS_ACL_ENABLE:
		break;
	case VDEV_SET_WNM_BSS_PREF:
		break;
	case VDEV_GET_NSS:
		break;
	case VDEV_GET_CHAN:
		if (!out)
			return QDF_STATUS_E_INVAL;
		qdf_mem_copy(&out->chan,
			     wlan_vdev_get_active_channel(vdev),
			     sizeof(out->chan));
		break;
	case VDEV_GET_CHAN_WIDTH:
		break;
	case VDEV_GET_CHAN_UTIL:
		if (!out)
			return QDF_STATUS_E_INVAL;
		out->chan_util = os_if_son_get_chan_util(vdev);
		break;
	case VDEV_GET_APCAP:
		if (!out)
			return QDF_STATUS_E_INVAL;
		return os_if_son_get_apcap(vdev, &out->apcap);
		break;
	case VDEV_GET_CONNECT_N_TX:
		break;
	case VDEV_GET_SSID:
		break;
	case VDEV_GET_MAX_PHYRATE:
		break;
	case VDEV_GET_ACL:
		break;
	case VDEV_GET_ACL_RSSI_THRESHOLDS:
		break;
	case VDEV_GET_NODE_CAP:
		if (!out || !in)
			return QDF_STATUS_E_INVAL;
		os_if_son_get_node_datarate_info(vdev, in->mac, &out->nodeinfo);
		break;
	case VDEV_GET_WDS:
		break;
	default:
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(os_if_son_vdev_ops);

static QDF_STATUS os_if_son_get_peer_capability(struct wlan_objmgr_vdev *vdev,
						struct wlan_objmgr_peer *peer,
						wlan_peer_cap *peer_cap)
{
	if (g_son_os_if_cb.os_if_get_peer_capability)
		return g_son_os_if_cb.os_if_get_peer_capability(vdev, peer,
								peer_cap);
	return QDF_STATUS_E_INVAL;
}

QDF_STATUS os_if_son_peer_ops(struct wlan_objmgr_peer *peer,
			      enum wlan_mlme_peer_param type,
			      union wlan_mlme_peer_data *in,
			      union wlan_mlme_peer_data *out)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr mac;
	int ret_val;
	static uint32_t peer_ext_stats_count;

	if (!peer) {
		osif_err("null peer");
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		osif_err("null vdev");
		return QDF_STATUS_E_INVAL;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		osif_err("null pdev");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		osif_err("null psoc");
		return QDF_STATUS_E_INVAL;
	}
	osif_debug("type %d", type);
	/* All PEER MLME operations exported to SON component */
	switch (type) {
	/* SET/CLR API start */
	case PEER_SET_KICKOUT:
		qdf_mem_copy(&mac.bytes, peer->macaddr, QDF_MAC_ADDR_SIZE);
		ret_val =
		    g_son_os_if_cb.os_if_kickout_mac(vdev, &mac);
		if (ret_val) {
			osif_err("Failed to kickout peer " QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(peer->macaddr));
			return QDF_STATUS_E_INVAL;
		}
		break;
	case PEER_SET_KICKOUT_ALLOW:
		if (!in) {
			osif_err("invalid input parameter");
			return QDF_STATUS_E_INVAL;
		}
		status = ucfg_son_set_peer_kickout_allow(vdev, peer,
							 in->enable);
		osif_debug("kickout allow %d, status %d", in->enable, status);
		break;
	case PEER_SET_EXT_STATS:
		if (!in)
			return QDF_STATUS_E_INVAL;
		ret_val = wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_EXT_STATS);
		osif_debug("Enable: %d peer_ext_stats_count: %u ret_val: %d",
			   in->enable, peer_ext_stats_count, ret_val);
		if ((!!ret_val) != in->enable) {
			status =
			     wlan_son_peer_ext_stat_enable(pdev, peer->macaddr,
							   vdev,
							   peer_ext_stats_count,
							   in->enable);
			osif_debug("status: %u", status);
			if (status == QDF_STATUS_SUCCESS) {
				peer_ext_stats_count++;
				wlan_peer_mlme_flag_set(peer,
							WLAN_PEER_F_EXT_STATS);
			} else {
				if (peer_ext_stats_count)
					peer_ext_stats_count--;
				wlan_peer_mlme_flag_clear
						(peer, WLAN_PEER_F_EXT_STATS);
			}
		}
		break;
	case PEER_REQ_INST_STAT:
		status = wlan_son_peer_req_inst_stats(pdev, peer->macaddr,
						      vdev);
		if (status != QDF_STATUS_SUCCESS)
			osif_err("Type: %d is failed", type);
		break;
	case PEER_GET_CAPABILITY:
		if (!out)
			return QDF_STATUS_E_INVAL;
		status = os_if_son_get_peer_capability(vdev, peer,
						       &out->peercap);
		break;
	case PEER_GET_MAX_MCS:
		if (!out)
			return QDF_STATUS_E_INVAL;
		out->mcs = os_if_son_get_peer_max_mcs_idx(vdev, peer);
		break;
	default:
		osif_err("invalid type: %d", type);
		status = QDF_STATUS_E_INVAL;
	}

	return status;
}

qdf_export_symbol(os_if_son_peer_ops);

QDF_STATUS os_if_son_scan_db_iterate(struct wlan_objmgr_pdev *pdev,
				     scan_iterator_func handler, void *arg)
{
	return ucfg_scan_db_iterate(pdev, handler, arg);
}

qdf_export_symbol(os_if_son_scan_db_iterate);

bool os_if_son_acl_is_probe_wh_set(struct wlan_objmgr_vdev *vdev,
				   const uint8_t *mac_addr,
				   uint8_t probe_rssi)
{
	return false;
}

qdf_export_symbol(os_if_son_acl_is_probe_wh_set);

int os_if_son_set_chwidth(struct wlan_objmgr_vdev *vdev,
			  enum ieee80211_cwm_width son_chwidth)
{
	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}

	return g_son_os_if_cb.os_if_set_chwidth(vdev, son_chwidth);
}
qdf_export_symbol(os_if_son_set_chwidth);

enum ieee80211_cwm_width os_if_son_get_chwidth(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	return g_son_os_if_cb.os_if_get_chwidth(vdev);
}
qdf_export_symbol(os_if_son_get_chwidth);

u_int8_t os_if_son_get_rx_streams(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	return g_son_os_if_cb.os_if_get_rx_nss(vdev);
}
qdf_export_symbol(os_if_son_get_rx_streams);

QDF_STATUS os_if_son_cfg80211_reply(qdf_nbuf_t sk_buf)
{
	return wlan_cfg80211_qal_devcfg_send_response(sk_buf);
}

qdf_export_symbol(os_if_son_cfg80211_reply);

bool os_if_son_vdev_is_wds(struct wlan_objmgr_vdev *vdev)
{
	return true;
}

qdf_export_symbol(os_if_son_vdev_is_wds);

uint32_t os_if_son_get_sta_space(struct wlan_objmgr_vdev *vdev)
{
	uint32_t sta_space;

	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	sta_space = g_son_os_if_cb.os_if_get_sta_space(vdev);
	osif_debug("need space %u", sta_space);

	return sta_space;
}

qdf_export_symbol(os_if_son_get_sta_space);

void os_if_son_get_sta_list(struct wlan_objmgr_vdev *vdev,
			    struct ieee80211req_sta_info *si, uint32_t *space)
{
	if (!vdev) {
		osif_err("null vdev");
		return;
	}

	if (!si) {
		osif_err("null si");
		return;
	}
	if (!space || *space == 0) {
		osif_err("invalid input space");
		return;
	}

	g_son_os_if_cb.os_if_get_sta_list(vdev, si, space);

	osif_debug("left space %u", *space);
}

qdf_export_symbol(os_if_son_get_sta_list);

void os_if_son_deauth_peer_sta(struct wlan_objmgr_vdev *vdev,
			       uint8_t *peer_mac,
			       bool ignore_frame)
{
	if (!vdev || !peer_mac) {
		osif_err("null vdev / peer_mac");
		return;
	}
	if (g_son_os_if_cb.os_if_deauth_sta)
		g_son_os_if_cb.os_if_deauth_sta(vdev, peer_mac, ignore_frame);
}

qdf_export_symbol(os_if_son_deauth_peer_sta);

void os_if_son_modify_acl(struct wlan_objmgr_vdev *vdev,
			  uint8_t *peer_mac,
			  bool allow_auth)
{
	if (!vdev || !peer_mac) {
		osif_err("null vdev / peer_mac");
		return;
	}
	if (g_son_os_if_cb.os_if_modify_acl)
		g_son_os_if_cb.os_if_modify_acl(vdev, peer_mac, allow_auth);
}

qdf_export_symbol(os_if_son_modify_acl);

static
int os_if_son_reg_get_ap_hw_cap(struct wlan_objmgr_pdev *pdev,
				struct wlan_radio_basic_capabilities *hwcap,
				bool skip_6ghz)
{
	QDF_STATUS status;
	uint8_t idx;
	uint8_t max_supp_op_class = REG_MAX_SUPP_OPER_CLASSES;
	uint8_t n_opclasses = 0;
	/* nsoc = Number of supported operating classes */
	uint8_t nsoc = 0;
	struct regdmn_ap_cap_opclass_t *reg_ap_cap;

	if (!pdev || !hwcap)
		return nsoc;

	reg_ap_cap = qdf_mem_malloc(max_supp_op_class * sizeof(*reg_ap_cap));
	if (!reg_ap_cap) {
		osif_err("Memory allocation failure");
		return nsoc;
	}
	status = wlan_reg_get_opclass_details(pdev, reg_ap_cap, &n_opclasses,
					      max_supp_op_class, true,
					      REG_CURRENT_PWR_MODE);
	if (status == QDF_STATUS_E_FAILURE) {
		osif_err("Failed to get SAP regulatory capabilities");
		goto end_reg_get_ap_hw_cap;
	}
	osif_debug("n_opclasses: %u", n_opclasses);

	for (idx = 0; reg_ap_cap[idx].op_class && idx < n_opclasses; idx++) {
		osif_debug("idx: %d op_class: %u ch_width: %d  max_tx_pwr_dbm: %u",
			   idx, reg_ap_cap[idx].op_class,
			   reg_ap_cap[idx].ch_width,
			   reg_ap_cap[idx].max_tx_pwr_dbm);
		if (reg_ap_cap[idx].ch_width == BW_160_MHZ)
			continue;
		if (skip_6ghz &&
		    wlan_reg_is_6ghz_op_class(pdev, reg_ap_cap[idx].op_class)) {
			osif_debug("ignore 6 GHz op_class: %d to son",
				   reg_ap_cap[idx].op_class);
			continue;
		}
		hwcap->opclasses[nsoc].opclass = reg_ap_cap[idx].op_class;
		hwcap->opclasses[nsoc].max_tx_pwr_dbm =
					reg_ap_cap[idx].max_tx_pwr_dbm;
		hwcap->opclasses[nsoc].num_non_oper_chan =
					reg_ap_cap[idx].num_non_supported_chan;
		qdf_mem_copy(hwcap->opclasses[nsoc].non_oper_chan_num,
			     reg_ap_cap[idx].non_sup_chan_list,
			     reg_ap_cap[idx].num_non_supported_chan);
		hwcap->wlan_radio_basic_capabilities_valid = 1;
		nsoc++;
	}
	hwcap->num_supp_op_classes = nsoc;

end_reg_get_ap_hw_cap:

	qdf_mem_free(reg_ap_cap);
	return nsoc;
}

static void os_if_son_reg_get_op_channels(struct wlan_objmgr_pdev *pdev,
					  struct wlan_op_chan *op_chan,
					  bool dfs_required)
{
	QDF_STATUS status;
	uint8_t idx;
	uint8_t max_supp_op_class = REG_MAX_SUPP_OPER_CLASSES;
	uint8_t n_opclasses = 0;
	/* nsoc = Number of supported operating classes */
	uint8_t nsoc = 0;
	struct regdmn_ap_cap_opclass_t *reg_ap_cap;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev || !op_chan) {
		osif_err("invalid input parameters");
		return;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		osif_err("NULL psoc");
		return;
	}

	reg_ap_cap = qdf_mem_malloc(max_supp_op_class * sizeof(*reg_ap_cap));
	if (!reg_ap_cap) {
		osif_err("Memory allocation failure");
		return;
	}
	status = wlan_reg_get_opclass_details(pdev, reg_ap_cap, &n_opclasses,
					      max_supp_op_class, true,
					      REG_CURRENT_PWR_MODE);
	if (status == QDF_STATUS_E_FAILURE) {
		osif_err("Failed to get SAP regulatory capabilities");
		goto end_reg_get_op_channels;
	}
	osif_debug("n_opclasses: %u op_chan->opclass: %u",
		   n_opclasses, op_chan->opclass);
	for (idx = 0; reg_ap_cap[idx].op_class && idx < n_opclasses; idx++) {
		if ((reg_ap_cap[idx].ch_width == BW_160_MHZ) ||
		    (op_chan->opclass != reg_ap_cap[idx].op_class))
			continue;
		osif_debug("idx: %d op_class: %u ch_width: %d  max_tx_pwr_dbm: %u",
			   idx, reg_ap_cap[idx].op_class,
			   reg_ap_cap[idx].ch_width,
			   reg_ap_cap[idx].max_tx_pwr_dbm);
		if (reg_ap_cap[idx].op_class == op_chan->opclass) {
			switch (reg_ap_cap[idx].ch_width) {
			case BW_20_MHZ:
			case BW_25_MHZ:
				op_chan->ch_width = CH_WIDTH_20MHZ;
				break;
			case BW_40_MHZ:
				op_chan->ch_width = CH_WIDTH_40MHZ;
				break;
			case BW_80_MHZ:
				if (reg_ap_cap[idx].behav_limit == BIT(BEHAV_BW80_PLUS) &&
				    ucfg_mlme_get_restricted_80p80_bw_supp(psoc))
					op_chan->ch_width = CH_WIDTH_80P80MHZ;
				else
					op_chan->ch_width = CH_WIDTH_80MHZ;
				break;
			case BW_160_MHZ:
				op_chan->ch_width  = CH_WIDTH_160MHZ;
				break;
			default:
				op_chan->ch_width = INVALID_WIDTH;
				break;
			}
			op_chan->num_oper_chan =
					reg_ap_cap[idx].num_supported_chan;
			qdf_mem_copy(op_chan->oper_chan_num,
				     reg_ap_cap[idx].sup_chan_list,
				     reg_ap_cap[idx].num_supported_chan);
		}
	}
	osif_debug("num of supported channel: %u",
		   op_chan->num_oper_chan);
	/*
	 * TBD: DFS channel support needs to be added
	 * Variable nsoc will be update whenever we add DFS
	 * channel support for Easymesh.
	 */
	op_chan->num_supp_op_classes = nsoc;

end_reg_get_op_channels:

	qdf_mem_free(reg_ap_cap);
}

/* size of sec chan offset element */
#define IEEE80211_SEC_CHAN_OFFSET_BYTES             3
/* no secondary channel */
#define IEEE80211_SEC_CHAN_OFFSET_SCN               0
/* secondary channel above */
#define IEEE80211_SEC_CHAN_OFFSET_SCA               1
/* secondary channel below */
#define IEEE80211_SEC_CHAN_OFFSET_SCB               3

static void os_if_son_reg_get_opclass_details(struct wlan_objmgr_pdev *pdev,
					      struct wlan_op_class *op_class)
{
	QDF_STATUS status;
	uint8_t i;
	uint8_t idx;
	uint8_t n_opclasses = 0;
	uint8_t chan_idx;
	uint8_t max_supp_op_class = REG_MAX_SUPP_OPER_CLASSES;
	struct regdmn_ap_cap_opclass_t *reg_ap_cap =
			qdf_mem_malloc(max_supp_op_class * sizeof(*reg_ap_cap));

	if (!reg_ap_cap) {
		osif_err("Memory allocation failure");
		return;
	}
	status = wlan_reg_get_opclass_details(pdev, reg_ap_cap, &n_opclasses,
					      max_supp_op_class, true,
					      REG_CURRENT_PWR_MODE);
	if (status == QDF_STATUS_E_FAILURE) {
		osif_err("Failed to get SAP regulatory capabilities");
		goto end_reg_get_opclass_details;
	}
	osif_debug("n_opclasses: %u", n_opclasses);

	for (idx = 0; reg_ap_cap[idx].op_class && idx < n_opclasses; idx++) {
		osif_debug("idx: %d op_class: %u ch_width: %d",
			   idx, reg_ap_cap[idx].op_class,
			   reg_ap_cap[idx].ch_width);
		if ((op_class->opclass != reg_ap_cap[idx].op_class) ||
		    (reg_ap_cap[idx].ch_width == BW_160_MHZ))
			continue;
		switch (reg_ap_cap[idx].ch_width) {
		case BW_20_MHZ:
		case BW_25_MHZ:
			op_class->ch_width = CH_WIDTH_20MHZ;
			break;
		case BW_40_MHZ:
			op_class->ch_width = CH_WIDTH_40MHZ;
			break;
		case BW_80_MHZ:
			if (reg_ap_cap[idx].behav_limit == BIT(BEHAV_BW80_PLUS))
				op_class->ch_width = CH_WIDTH_80P80MHZ;
			else
				op_class->ch_width = CH_WIDTH_80MHZ;
			break;
		case BW_160_MHZ:
			op_class->ch_width  = CH_WIDTH_160MHZ;
			break;
		default:
			op_class->ch_width = CH_WIDTH_INVALID;
			break;
		}
		switch (reg_ap_cap[idx].behav_limit) {
		case BIT(BEHAV_NONE):
			op_class->sc_loc = IEEE80211_SEC_CHAN_OFFSET_SCN;
			break;
		case BIT(BEHAV_BW40_LOW_PRIMARY):
			op_class->sc_loc = IEEE80211_SEC_CHAN_OFFSET_SCA;
			break;
		case BIT(BEHAV_BW40_HIGH_PRIMARY):
			op_class->sc_loc = IEEE80211_SEC_CHAN_OFFSET_SCB;
			break;
		case BIT(BEHAV_BW80_PLUS):
			op_class->sc_loc = IEEE80211_SEC_CHAN_OFFSET_SCN;
			break;
		default:
			op_class->sc_loc = IEEE80211_SEC_CHAN_OFFSET_SCN;
			break;
		}
		osif_debug("num_supported_chan: %u num_non_supported_chan: %u",
			   reg_ap_cap[idx].num_supported_chan,
			   reg_ap_cap[idx].num_non_supported_chan);
		i = 0;
		chan_idx = 0;
		while ((i < reg_ap_cap[idx].num_supported_chan) &&
		       (chan_idx < MAX_CHANNELS_PER_OP_CLASS))
			op_class->channels[chan_idx++] =
				reg_ap_cap[idx].sup_chan_list[i++];
		i = 0;
		while ((i < reg_ap_cap[idx].num_non_supported_chan) &&
		       (chan_idx < MAX_CHANNELS_PER_OP_CLASS))
			op_class->channels[chan_idx++] =
				reg_ap_cap[idx].non_sup_chan_list[i++];

		 op_class->num_chan = chan_idx;
	}

end_reg_get_opclass_details:

	qdf_mem_free(reg_ap_cap);
}

QDF_STATUS os_if_son_pdev_ops(struct wlan_objmgr_pdev *pdev,
			      enum wlan_mlme_pdev_param type,
			      void *data, void *ret)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	union wlan_mlme_pdev_data *in = (union wlan_mlme_pdev_data *)data;
	union wlan_mlme_pdev_data *out = (union wlan_mlme_pdev_data *)ret;
	wlan_esp_data *esp_info;

	if (!out)
		return QDF_STATUS_E_INVAL;

	osif_debug("Type: %d", type);
	switch (type) {
	case PDEV_GET_ESP_INFO:
		esp_info = &out->esp_info;
		/* BA Window Size of 16 */
		esp_info->per_ac[WME_AC_BE].ba_window_size = ba_window_size_16;
		esp_info->per_ac[WME_AC_BE].est_air_time_fraction = 0;
		/* Default : 250us PPDU Duration in native format */
		esp_info->per_ac[WME_AC_BE].data_ppdu_dur_target =
			MAP_DEFAULT_PPDU_DURATION * MAP_PPDU_DURATION_UNITS;
		break;
	case PDEV_GET_CAPABILITY:
		os_if_son_reg_get_ap_hw_cap(pdev, &out->cap, in->skip_6ghz);
		break;
	case PDEV_GET_OPERABLE_CHAN:
		memcpy(&out->op_chan, &in->op_chan,
		       sizeof(struct wlan_op_chan));
		os_if_son_reg_get_op_channels(pdev, &out->op_chan,
					      in->op_chan.dfs_required);
		break;
	case PDEV_GET_OPERABLE_CLASS:
		memcpy(&out->op_class, &in->op_class,
		       sizeof(struct wlan_op_class));
		os_if_son_reg_get_opclass_details(pdev, &out->op_class);
		break;
	default:
		break;
	}

	return status;
}

qdf_export_symbol(os_if_son_pdev_ops);

int os_if_son_deliver_ald_event(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_peer *peer,
				enum ieee80211_event_type event,
				void *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_rx_ops *rx_ops;
	int ret;

	if (!vdev) {
		osif_err("null vdev");
		return -EINVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("null posc");
		return -EINVAL;
	}
	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (rx_ops && rx_ops->son_rx_ops.deliver_event)
		ret = rx_ops->son_rx_ops.deliver_event(vdev, peer, event,
						       event_data);
	else
		ret = -EINVAL;

	return ret;
}

qdf_export_symbol(os_if_son_deliver_ald_event);

struct wlan_objmgr_vdev *
os_if_son_get_vdev_by_netdev(struct net_device *dev)
{
	return g_son_os_if_cb.os_if_get_vdev_by_netdev(dev);
}

qdf_export_symbol(os_if_son_get_vdev_by_netdev);

QDF_STATUS os_if_son_trigger_objmgr_object_creation(enum wlan_umac_comp_id id)
{
	return g_son_os_if_cb.os_if_trigger_objmgr_object_creation(id);
}

qdf_export_symbol(os_if_son_trigger_objmgr_object_creation);

QDF_STATUS os_if_son_trigger_objmgr_object_deletion(enum wlan_umac_comp_id id)
{
	return g_son_os_if_cb.os_if_trigger_objmgr_object_deletion(id);
}

qdf_export_symbol(os_if_son_trigger_objmgr_object_deletion);

int os_if_son_start_acs(struct wlan_objmgr_vdev *vdev, uint8_t enable)
{
	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	return g_son_os_if_cb.os_if_start_acs(vdev, enable);
}

qdf_export_symbol(os_if_son_start_acs);

int os_if_son_set_acs_chan(struct wlan_objmgr_vdev *vdev,
			   struct ieee80211req_athdbg *req)
{
	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	return g_son_os_if_cb.os_if_set_acs_channels(vdev, req);
}

qdf_export_symbol(os_if_son_set_acs_chan);

int os_if_son_get_acs_report(struct wlan_objmgr_vdev *vdev,
			     struct ieee80211_acs_dbg *acs_r)
{
	if (!vdev) {
		osif_err("null vdev");
		return 0;
	}

	return g_son_os_if_cb.os_if_get_acs_report(vdev, acs_r);
}

qdf_export_symbol(os_if_son_get_acs_report);

void
wlan_os_if_son_ops_register_cb(void (*handler)(struct wlan_os_if_son_ops *))
{
	os_if_son_ops_cb = handler;
}

qdf_export_symbol(wlan_os_if_son_ops_register_cb);

static void wlan_son_register_os_if_ops(struct wlan_os_if_son_ops *son_ops)
{
	if (os_if_son_ops_cb)
		os_if_son_ops_cb(son_ops);
	else
		osif_err("\n***** OS_IF: SON MODULE NOT LOADED *****\n");
}

void os_if_son_register_lmac_if_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	if (!psoc) {
		osif_err("psoc is NULL");
		return;
	}

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		osif_err("rx_ops is null");
		return;
	}

	wlan_lmac_if_son_mod_register_rx_ops(rx_ops);
}

qdf_export_symbol(os_if_son_register_lmac_if_ops);

void os_if_son_register_osif_ops(void)
{
	wlan_son_register_os_if_ops(&g_son_os_if_txrx_ops);
}

qdf_export_symbol(os_if_son_register_osif_ops);

int os_if_son_parse_generic_nl_cmd(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   struct nlattr **tb,
				   enum os_if_son_vendor_cmd_type type)
{
	struct os_if_son_rx_ops *rx_ops = &g_son_os_if_txrx_ops.son_osif_rx_ops;
	struct wlan_cfg8011_genric_params param = {};

	if (!rx_ops->parse_generic_nl_cmd)
		return -EINVAL;

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND])
		param.command = nla_get_u32(tb
				[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE])
		param.value = nla_get_u32(tb
				[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]) {
		param.data = nla_data(tb
				[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
		param.data_len = nla_len(tb
				[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH])
		param.length = nla_get_u32(tb
				[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_LENGTH]);

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS])
		param.flags = nla_get_u32(tb
				[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_FLAGS]);

	return rx_ops->parse_generic_nl_cmd(wiphy, wdev, &param, type);
}

QDF_STATUS os_if_son_get_node_datarate_info(struct wlan_objmgr_vdev *vdev,
					    uint8_t *mac_addr,
					    wlan_node_info *node_info)
{
	int8_t max_tx_power;
	int8_t min_tx_power;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("null posc");
		return QDF_STATUS_E_INVAL;
	}

	if (WLAN_ADDR_EQ(wlan_vdev_mlme_get_macaddr(vdev), mac_addr) ==
							   QDF_STATUS_SUCCESS) {
		node_info->max_chwidth = os_if_son_get_chwidth(vdev);
		node_info->phymode = os_if_son_get_phymode(vdev);
		node_info->num_streams = os_if_son_get_rx_streams(vdev);
		ucfg_son_get_min_and_max_power(psoc, &max_tx_power,
					       &min_tx_power);
		node_info->max_txpower = max_tx_power;
		node_info->max_MCS = ucfg_mlme_get_vdev_max_mcs_idx(vdev);
		if (node_info->max_MCS == INVALID_MCS_NSS_INDEX) {
			osif_err("invalid mcs index");
			return QDF_STATUS_E_INVAL;
		}
		osif_debug("node info: max_chwidth: %u, phymode: %u, num_streams: %d, max_mcs: %d, max_txpower: %d",
			   node_info->max_chwidth, node_info->phymode,
			   node_info->num_streams, node_info->max_MCS,
			   node_info->max_txpower);
	} else {
		if (!g_son_os_if_cb.os_if_get_node_info) {
			osif_err("Callback not registered");
			return QDF_STATUS_E_INVAL;
		}
		status = g_son_os_if_cb.os_if_get_node_info(vdev, mac_addr,
							    node_info);
	}
	return status;
}

qdf_export_symbol(os_if_son_get_node_datarate_info);

uint32_t os_if_son_get_peer_max_mcs_idx(struct wlan_objmgr_vdev *vdev,
					struct wlan_objmgr_peer *peer)
{
	if (g_son_os_if_cb.os_if_get_peer_max_mcs_idx)
		return g_son_os_if_cb.os_if_get_peer_max_mcs_idx(vdev, peer);

	return 0;
}

int os_if_son_get_sta_stats(struct wlan_objmgr_vdev *vdev, uint8_t *mac_addr,
			    struct ieee80211_nodestats *stats)
{
	if (g_son_os_if_cb.os_if_get_sta_stats)
		return g_son_os_if_cb.os_if_get_sta_stats(vdev, mac_addr,
							  stats);

	return 0;
}
qdf_export_symbol(os_if_son_get_sta_stats);
