/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: wlan_hdd_tx_power.c
 *
 * WLAN tx power setting functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wma_api.h>
#include <wlan_hdd_tx_power.h>
#include "wlan_hdd_object_manager.h"

#define MAX_TXPOWER_SCALE 4

const struct nla_policy
txpower_scale_policy[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE] = { .type = NLA_U8 },
};

/**
 * __wlan_hdd_cfg80211_txpower_scale () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_txpower_scale(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	int ret;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX + 1];
	uint8_t scale_value;
	QDF_STATUS status;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX,
				    data, data_len, txpower_scale_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE]) {
		hdd_err("attr tx power scale failed");
		return -EINVAL;
	}

	scale_value = nla_get_u8(tb
		    [QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE]);

	if (scale_value > MAX_TXPOWER_SCALE) {
		hdd_err("Invalid tx power scale level");
		return -EINVAL;
	}

	status = wma_set_tx_power_scale(adapter->deflink->vdev_id, scale_value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Set tx power scale failed");
		return -EINVAL;
	}

	return 0;
}

int wlan_hdd_cfg80211_txpower_scale(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_txpower_scale(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

const struct nla_policy txpower_scale_decr_db_policy
[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB] = { .type = NLA_U8 },
};

/**
 * __wlan_hdd_cfg80211_txpower_scale_decr_db () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_txpower_scale_decr_db(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data,
					  int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	int ret;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX + 1];
	uint8_t scale_value;
	QDF_STATUS status;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if (wlan_cfg80211_nla_parse(tb,
				QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX,
				data, data_len,
				txpower_scale_decr_db_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB]) {
		hdd_err("attr tx power decrease db value failed");
		return -EINVAL;
	}

	scale_value = nla_get_u8(tb
		    [QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB]);

	status = wma_set_tx_power_scale_decr_db(adapter->deflink->vdev_id,
						scale_value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Set tx power decrease db failed");
		return -EINVAL;
	}

	return 0;
}

int wlan_hdd_cfg80211_txpower_scale_decr_db(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_txpower_scale_decr_db(wiphy, wdev,
							  data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static uint32_t get_default_tpc_info_vendor_sbk_len(void)
{
	uint32_t skb_len;

	/* QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_FREQUENCY */
	skb_len = nla_total_size(sizeof(u32));
	/* QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_TX_POWER */
	skb_len += nla_total_size(sizeof(s8));
	skb_len = nla_total_size(skb_len) * MAX_NUM_PWR_LEVEL;
	skb_len = nla_total_size(skb_len);

	/* QCA_WLAN_VENDOR_ATTR_TPC_BSSID */
	skb_len += nla_total_size(QDF_MAC_ADDR_SIZE);
	/* QCA_WLAN_VENDOR_ATTR_TPC_PSD_POWER */
	skb_len += nla_total_size(sizeof(u8));
	/* QCA_WLAN_VENDOR_ATTR_TPC_EIRP_POWER */
	skb_len += nla_total_size(sizeof(s8));
	/* QCA_WLAN_VENDOR_ATTR_TPC_POWER_TYPE_6GHZ */
	skb_len += nla_total_size(sizeof(u8));
	/* QCA_WLAN_VENDOR_ATTR_TPC_AP_CONSTRAINT_POWER */
	skb_len += nla_total_size(sizeof(u8));

	skb_len = nla_total_size(skb_len) * WLAN_MAX_ML_BSS_LINKS;
	skb_len = nla_total_size(skb_len) + NLMSG_HDRLEN;

	return skb_len;
}

static int
__wlan_hdd_cfg80211_get_reg_tpc_info(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	int ret;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	int num_of_links = 0;
	struct qdf_mac_addr link_bssid[WLAN_MAX_ML_BSS_LINKS];
	struct reg_tpc_power_info reg_tpc_info[WLAN_MAX_ML_BSS_LINKS];
	struct sk_buff *reply_skb = NULL;
	uint32_t skb_len, i, j;
	struct wlan_hdd_link_info *link_info;
	struct nlattr *tpc_links_attr, *tpc_attr, *levels_attr, *tpc_level;
	int attr_id;
	struct chan_power_info *chan_power_info;

	hdd_enter_dev(dev);

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if (!adapter) {
		hdd_err("adapter null");
		return -EPERM;
	}

	if (adapter->device_mode != QDF_STA_MODE) {
		hdd_err("device mode %d not support", adapter->device_mode);
		return -EPERM;
	}

	hdd_adapter_for_each_link_info(adapter, link_info) {
		if (num_of_links >= WLAN_MAX_ML_BSS_LINKS)
			break;
		if (link_info->vdev_id == WLAN_INVALID_VDEV_ID)
			continue;
		if (!hdd_cm_is_vdev_connected(link_info))
			continue;

		vdev = hdd_objmgr_get_vdev_by_user(link_info,
						   WLAN_OSIF_POWER_ID);
		if (!vdev)
			continue;

		status = wlan_vdev_get_bss_peer_mac(vdev,
						    &link_bssid[num_of_links]);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("failed to get bssid for vdev %d",
				link_info->vdev_id);
			goto next_link;
		}

		status =
		ucfg_wlan_mlme_get_reg_tpc_info(vdev,
						&reg_tpc_info[num_of_links]);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("failed to get tpc info for vdev %d",
				link_info->vdev_id);
			goto next_link;
		}

		num_of_links++;
next_link:
		hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_POWER_ID);
	}

	if (!num_of_links) {
		hdd_err("get tpc info failed - nun of links 0");
		return -EINVAL;
	}

	skb_len = get_default_tpc_info_vendor_sbk_len();

	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(
						wiphy,
						skb_len);
	if (!reply_skb) {
		hdd_err("alloc reply_skb failed");
		status = QDF_STATUS_E_NOMEM;
		goto free_skb;
	}

	tpc_links_attr = nla_nest_start(reply_skb,
					QCA_WLAN_VENDOR_ATTR_TPC_LINKS);
	if (!tpc_links_attr) {
		hdd_err("failed to add QCA_WLAN_VENDOR_ATTR_TPC_LINKS");
		status = QDF_STATUS_E_INVAL;
		goto free_skb;
	}

	for (i = 0; i < num_of_links; i++) {
		tpc_attr = nla_nest_start(reply_skb, i);
		if (!tpc_attr) {
			hdd_err("tpc_attr null, %d", i);
			continue;
		}
		if (nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_TPC_BSSID,
			    QDF_MAC_ADDR_SIZE, &link_bssid[i])) {
			hdd_err("failed to put mac_addr");
			status = QDF_STATUS_E_INVAL;
			goto free_skb;
		}

		if (reg_tpc_info[i].is_psd_power &&
		    nla_put_flag(reply_skb,
				 QCA_WLAN_VENDOR_ATTR_TPC_PSD_POWER)) {
			osif_err("failed to put psd flag");
			return QDF_STATUS_E_INVAL;
		}

		if (nla_put_s8(reply_skb,
			       QCA_WLAN_VENDOR_ATTR_TPC_EIRP_POWER,
			       reg_tpc_info[i].reg_max[0])) {
			hdd_err("failed to put eirp_power");
			status = QDF_STATUS_E_INVAL;
			goto free_skb;
		}

		chan_power_info = &reg_tpc_info[i].chan_power_info[0];
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_power_info->chan_cfreq) &&
		    nla_put_u8(reply_skb,
			       QCA_WLAN_VENDOR_ATTR_TPC_POWER_TYPE_6GHZ,
			       reg_tpc_info[i].power_type_6g)) {
			hdd_err("failed to put power_type_6g");
			status = QDF_STATUS_E_INVAL;
			goto free_skb;
		}

		if (nla_put_u8(reply_skb,
			       QCA_WLAN_VENDOR_ATTR_TPC_AP_CONSTRAINT_POWER,
			       reg_tpc_info[i].ap_constraint_power)) {
			hdd_err("failed to put ap_constraint_power");
			status = QDF_STATUS_E_INVAL;
			goto free_skb;
		}

		hdd_debug("%d tpc for bssid "QDF_MAC_ADDR_FMT" is_psd %d reg power %d 6ghz pwr type %d ap_constraint_power %d",
			  i, QDF_MAC_ADDR_REF(link_bssid[i].bytes),
			  reg_tpc_info[i].is_psd_power,
			  reg_tpc_info[i].reg_max[0],
			  reg_tpc_info[i].power_type_6g,
			  reg_tpc_info[i].ap_constraint_power);

		levels_attr = nla_nest_start(
			reply_skb, QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL);
		if (!levels_attr) {
			hdd_err("failed to add QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL");
			status = QDF_STATUS_E_INVAL;
			goto free_skb;
		}
		for (j = 0; j < reg_tpc_info[i].num_pwr_levels; j++) {
			tpc_level = nla_nest_start(reply_skb, j);
			if (!tpc_level) {
				hdd_err("tpc_level null. level %d", j);
				continue;
			}
			chan_power_info = &reg_tpc_info[i].chan_power_info[j];

			attr_id = QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_FREQUENCY;
			if (nla_put_u32(reply_skb,
					attr_id,
					chan_power_info->chan_cfreq)) {
				hdd_err("failed to put chan_cfreq %d",
					chan_power_info->chan_cfreq);
				status = QDF_STATUS_E_INVAL;
				goto free_skb;
			}

			attr_id = QCA_WLAN_VENDOR_ATTR_TPC_PWR_LEVEL_TX_POWER;
			if (nla_put_s8(reply_skb,
				       attr_id,
				       chan_power_info->tx_power)) {
				hdd_err("failed to put tx_power %d",
					chan_power_info->tx_power);
				status = QDF_STATUS_E_INVAL;
				goto free_skb;
			}

			hdd_debug("%d cfreq %d tx_power %d", j,
				  chan_power_info->chan_cfreq,
				  chan_power_info->tx_power);

			nla_nest_end(reply_skb, tpc_level);
		}
		nla_nest_end(reply_skb, levels_attr);

		nla_nest_end(reply_skb, tpc_attr);
	}

	nla_nest_end(reply_skb, tpc_links_attr);

	ret = wlan_cfg80211_vendor_cmd_reply(reply_skb);

	hdd_exit();

	return ret;
free_skb:
	if (reply_skb)
		wlan_cfg80211_vendor_free_skb(reply_skb);
	hdd_exit();
	return qdf_status_to_os_return(status);
}

int wlan_hdd_cfg80211_get_reg_tpc_info(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_get_reg_tpc_info(wiphy, wdev,
						     data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
