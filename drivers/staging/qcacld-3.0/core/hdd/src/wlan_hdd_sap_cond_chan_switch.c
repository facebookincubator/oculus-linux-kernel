/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_sap_cond_chan_switch.c
 *
 * WLAN SAP conditional channel switch functions
 *
 */

#include <cds_utils.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include "osif_sync.h"
#include <qdf_str.h>
#include <wlan_hdd_includes.h>
#include <wlan_hdd_sap_cond_chan_switch.h>
#include "wlan_hdd_pre_cac.h"

const struct nla_policy conditional_chan_switch_policy[
		QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST] = {
				.type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_STATUS] = {
				.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_conditional_chan_switch() - Conditional channel switch
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Processes the conditional channel switch request and invokes the helper
 * APIs to process the channel switch request.
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_conditional_chan_switch(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct nlattr
		*tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_MAX + 1];
	uint32_t freq_len, i;
	uint32_t *freq;
	bool is_dfs_mode_enabled = false;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_STATUS_SUCCESS != ucfg_mlme_get_dfs_master_capability(
				hdd_ctx->psoc, &is_dfs_mode_enabled)) {
		hdd_err("Failed to get dfs master capability");
		return -EINVAL;
	}

	if (!is_dfs_mode_enabled) {
		hdd_err("DFS master capability is not present in the driver");
		return -EINVAL;
	}

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if (adapter->device_mode != QDF_SAP_MODE) {
		hdd_err("Invalid device mode %d", adapter->device_mode);
		return -EINVAL;
	}

	/*
	 * audit note: it is ok to pass a NULL policy here since only
	 * one attribute is parsed which is array of frequencies and
	 * it is explicitly validated for both under read and over read
	 */
	if (wlan_cfg80211_nla_parse(tb,
			   QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_MAX,
			   data, data_len, conditional_chan_switch_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST]) {
		hdd_err("Frequency list is missing");
		return -EINVAL;
	}

	freq_len = nla_len(
		tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST])/
		sizeof(uint32_t);

	if (freq_len > NUM_CHANNELS) {
		hdd_err("insufficient space to hold channels");
		return -ENOMEM;
	}

	hdd_debug("freq_len=%d", freq_len);

	freq = nla_data(
		tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST]);

	for (i = 0; i < freq_len; i++)
		hdd_debug("freq[%d]=%d", i, freq[i]);

	/*
	 * The input frequency list from user space is designed to be a
	 * priority based frequency list. This is only to accommodate any
	 * future request. But, current requirement is only to perform CAC
	 * on a single channel. So, the first entry from the list is picked.
	 *
	 * If channel is zero, any channel in the available outdoor regulatory
	 * domain will be selected.
	 */
	ret = wlan_hdd_request_pre_cac(hdd_ctx, freq[0]);
	if (ret) {
		hdd_err("pre cac request failed with reason:%d", ret);
		return ret;
	}

	return 0;
}

int wlan_hdd_cfg80211_conditional_chan_switch(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_conditional_chan_switch(wiphy, wdev,
							    data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

