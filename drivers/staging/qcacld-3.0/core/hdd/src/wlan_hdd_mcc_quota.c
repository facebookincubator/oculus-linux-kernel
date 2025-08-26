/*
 * Copyright (c) 2022, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_mcc_quota.c
 *
 * WLAN Host Device Driver MCC quota feature cfg80211 APIs implementation
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <net/cfg80211.h>
#include "sme_api.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_hostapd.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_mcc_quota.h"
#include "wlan_hdd_trace.h"
#include "qdf_str.h"
#include "qdf_trace.h"
#include "qdf_types.h"
#include "wlan_policy_mgr_api.h"
#include <qca_vendor.h>
#include "wlan_utility.h"
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_hdd_object_manager.h"
#include "sme_api.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_osif_priv.h"
#include "wlan_p2p_mcc_quota_public_struct.h"

const struct nla_policy
set_mcc_quota_policy[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_TYPE] =	{ .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_ENTRIES] =
				VENDOR_NLA_POLICY_NESTED(set_mcc_quota_policy),
	[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_FREQ] =	{
					.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_TIME_PERCENTAGE] =	{
							.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_IFINDEX] = { .type = NLA_U32 },
};

int wlan_hdd_set_mcc_adaptive_sched(struct wlan_objmgr_psoc *psoc, bool enable)
{
	bool enable_mcc_adaptive_sch;

	hdd_debug("enable : %d", enable);
	ucfg_policy_mgr_get_mcc_adaptive_sch(psoc, &enable_mcc_adaptive_sch);
	if (enable_mcc_adaptive_sch) {
		ucfg_policy_mgr_set_dynamic_mcc_adaptive_sch(psoc, enable);
		if (QDF_IS_STATUS_ERROR(sme_set_mas(enable))) {
			hdd_err("Fail to config mcc adaptive sched.");
			return -EINVAL;
		}
	}

	return 0;
}

int wlan_hdd_cfg80211_set_mcc_quota(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *attr,
				    int attr_len)
{
	struct hdd_adapter *if_adapter;
	struct hdd_context *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX + 1];
	struct nlattr *quota_entries[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX + 1];
	struct nlattr *curr_attr;
	struct wlan_objmgr_psoc *psoc;
	uint32_t duty_cycle, cmd_id, quota_type, rem_bytes, entries, if_idx;
	struct wlan_user_mcc_quota mcc_quota;
	int att_id, rc;

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	psoc = hdd_ctx->psoc;
	if (!psoc)
		return -EINVAL;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX,
				    attr, attr_len, set_mcc_quota_policy)) {
		hdd_err("Error parsing attributes");
		return -EINVAL;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_TYPE;
	if (!tb[cmd_id]) {
		hdd_err("Quota type not specified");
		return -EINVAL;
	}
	quota_type = nla_get_u32(tb[cmd_id]);
	if (quota_type != QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_FIXED &&
	    quota_type != QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_CLEAR) {
		hdd_err("Quota type is not valid %u", quota_type);
		return -EINVAL;
	}

	if (quota_type == QCA_WLAN_VENDOR_MCC_QUOTA_TYPE_CLEAR) {
		/* Remove quota, enable MCC adaptive scheduling */
		if (wlan_hdd_set_mcc_adaptive_sched(hdd_ctx->psoc, true))
			return -EAGAIN;
		mcc_quota.op_mode = QDF_MAX_NO_OF_MODE;
		mcc_quota.vdev_id = WLAN_UMAC_VDEV_ID_MAX;
		ucfg_mlme_set_user_mcc_quota(psoc, &mcc_quota);
		return 0;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_ENTRIES;
	if (!tb[cmd_id]) {
		hdd_err("No entries present");
		return -EINVAL;
	}

	entries = 0;
	nla_for_each_nested(curr_attr, tb[cmd_id], rem_bytes) {
		if (entries > 0) {
			hdd_debug("Only one entry permitted");
			hdd_debug("Entry (%d) for (%u) is ignored",
				  entries, nla_type(curr_attr));
			entries++;
			continue;
		}
		rc = wlan_cfg80211_nla_parse_nested(quota_entries,
			       QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX,
			       curr_attr,
			       set_mcc_quota_policy);
		if (rc) {
			hdd_err("Entry parse error %d", rc);
			return -EINVAL;
		}

		att_id = QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_IFINDEX;
		if (!quota_entries[att_id]) {
			hdd_err("if_index not specified");
			return -EINVAL;
		}

		if_idx = nla_get_u32(quota_entries[att_id]);
		if (if_idx == 0) {
			hdd_debug("Invalid if_index");
			return -EINVAL;
		}
		if_adapter = hdd_get_adapter_by_ifindex(hdd_ctx, if_idx);

		if (!if_adapter) {
			hdd_err("interface (%u) not found", if_idx);
			return -EINVAL;
		}

		if (wlan_hdd_validate_vdev_id(if_adapter->vdev_id))
			return -EINVAL;

		att_id = QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_TIME_PERCENTAGE;
		if (!quota_entries[att_id]) {
			hdd_err("Quota not specified");
			return -EINVAL;
		}
		mcc_quota.quota = nla_get_u32(quota_entries[att_id]);
		mcc_quota.vdev_id = if_adapter->vdev_id;
		mcc_quota.op_mode = if_adapter->device_mode;

		entries++;
	}

	if (entries == 0) {
		hdd_err("No entries found");
		return -EINVAL;
	}

	if (mcc_quota.op_mode != QDF_P2P_GO_MODE &&
	    mcc_quota.op_mode != QDF_P2P_CLIENT_MODE) {
		hdd_debug("Support only P2P GO/GC mode now");
		return -EOPNOTSUPP;
	}

	ucfg_mlme_set_user_mcc_quota(psoc, &mcc_quota);

	duty_cycle = ucfg_mlme_get_user_mcc_quota_percentage(psoc);

	if (duty_cycle == 0) {
		hdd_debug("Quota will be configured when MCC scenario exists");
		return 0;
	}

	if (wlan_hdd_set_mcc_adaptive_sched(hdd_ctx->psoc, false))
		return -EAGAIN;

	if (wlan_hdd_send_mcc_vdev_quota(if_adapter, duty_cycle))
		return -EINVAL;

	return 0;
}

int wlan_hdd_apply_user_mcc_quota(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	uint32_t quota_val;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx)
		return -EINVAL;

	quota_val =
		ucfg_mlme_get_user_mcc_quota_percentage(hdd_ctx->psoc);

	if (quota_val == 0) {
		hdd_debug("no mcc/quota for mode %d, vdev_id : %u",
			  adapter->device_mode, adapter->vdev_id);
		return 0;
	}

	if (wlan_hdd_set_mcc_adaptive_sched(hdd_ctx->psoc, false))
		return 0;

	if (wlan_hdd_send_mcc_vdev_quota(adapter, quota_val)) {
		hdd_info("Could not send quota");
		wlan_hdd_set_mcc_adaptive_sched(hdd_ctx->psoc, true);
	}

	return 0;
}

/**
 * wlan_cfg80211_indicate_mcc_quota() - Callback to indicate mcc quota
 * event to upper layer
 * @psoc: pointer to soc object
 * @vdev: vdev object
 * @quota_info: quota info
 *
 * This callback will be used to indicate mcc quota info to upper layer
 *
 * Return: QDF_STATUS_SUCCESS if event is indicated to OS successfully.
 */
static QDF_STATUS
wlan_cfg80211_indicate_mcc_quota(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev,
				 struct mcc_quota_info *quota_info)
{
	uint32_t data_len;
	struct sk_buff *vendor_event;
	QDF_STATUS status;
	struct vdev_osif_priv *vdev_osif_priv;
	struct wireless_dev *wdev;
	struct pdev_osif_priv *pdev_osif_priv;
	struct wlan_objmgr_pdev *pdev;
	uint32_t idx;
	uint32_t vdev_id;
	struct nlattr *quota_attrs, *quota_element;

	if (!vdev) {
		hdd_debug("null vdev");
		return QDF_STATUS_E_INVAL;
	}

	vdev_osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!vdev_osif_priv || !vdev_osif_priv->wdev) {
		hdd_debug("null wdev");
		return QDF_STATUS_E_INVAL;
	}

	wdev = vdev_osif_priv->wdev;
	vdev_id = wlan_vdev_get_id(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		hdd_debug("null pdev");
		return QDF_STATUS_E_INVAL;
	}

	pdev_osif_priv = wlan_pdev_get_ospriv(pdev);
	if (!pdev_osif_priv || !pdev_osif_priv->wiphy) {
		hdd_debug("null wiphy");
		return QDF_STATUS_E_INVAL;
	}

	/* nested element of QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_FREQ and
	 * QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_TIME_PERCENTAGE
	 */
	data_len = nla_total_size(nla_total_size(sizeof(uint32_t)) +
				  nla_total_size(sizeof(uint32_t)));
	/* nested array of quota element */
	data_len = nla_total_size(data_len * quota_info->num_chan_quota);
	/* QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_TYPE and NL msg header */
	data_len += nla_total_size(sizeof(uint32_t)) + NLMSG_HDRLEN;

	vendor_event = wlan_cfg80211_vendor_event_alloc(pdev_osif_priv->wiphy,
							wdev, data_len,
							QCA_NL80211_VENDOR_SUBCMD_MCC_QUOTA_INDEX,
							GFP_KERNEL);
	if (!vendor_event) {
		hdd_debug("wlan_cfg80211_vendor_event_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_TYPE,
			quota_info->type)) {
		status = QDF_STATUS_E_NOMEM;
		hdd_debug("add QUOTA_TYPE failed");
		goto err;
	}

	quota_attrs = nla_nest_start(vendor_event,
				     QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_ENTRIES);
	if (!quota_attrs) {
		status = QDF_STATUS_E_NOMEM;
		hdd_debug("add QUOTA_ENTRIES failed");
		goto err;
	}
	hdd_debug("mcc quota vdev %d type %d num %d",
		  vdev_id, quota_info->type, quota_info->num_chan_quota);

	for (idx = 0; idx < quota_info->num_chan_quota; idx++) {
		quota_element = nla_nest_start(vendor_event, idx);
		if (!quota_element) {
			status = QDF_STATUS_E_NOMEM;
			hdd_debug("add quota idx failed");
			goto err;
		}

		if (nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_FREQ,
				quota_info->chan_quota[idx].chan_mhz)) {
			status = QDF_STATUS_E_NOMEM;
			hdd_debug("add QUOTA_CHAN_FREQ failed");
			goto err;
		}

		if (nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_CHAN_TIME_PERCENTAGE,
				quota_info->chan_quota[idx].channel_time_quota)) {
			status = QDF_STATUS_E_NOMEM;
			hdd_debug("add QUOTA_CHAN_TIME_PERCENTAGE failed");
			goto err;
		}

		nla_nest_end(vendor_event, quota_element);
		hdd_debug("mcc quota vdev %d [%d] %d quota %d",
			  vdev_id, idx, quota_info->chan_quota[idx].chan_mhz,
			  quota_info->chan_quota[idx].channel_time_quota);
	}
	nla_nest_end(vendor_event, quota_attrs);

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return QDF_STATUS_SUCCESS;
err:
	wlan_cfg80211_vendor_free_skb(vendor_event);

	return status;
}

/**
 * wlan_hdd_register_mcc_quota_event_callback() - Register hdd callback to get
 * mcc quota event to upper layer
 * @hdd_ctx: pointer to hdd context
 *
 * Return: void
 */
void wlan_hdd_register_mcc_quota_event_callback(struct hdd_context *hdd_ctx)
{
	ucfg_p2p_register_mcc_quota_event_os_if_cb(hdd_ctx->psoc,
						   wlan_cfg80211_indicate_mcc_quota);
}
