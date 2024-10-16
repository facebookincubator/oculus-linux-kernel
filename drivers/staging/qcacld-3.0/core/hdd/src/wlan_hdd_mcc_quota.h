/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_mcc_quota.h
 *
 * WLAN Host Device Driver MCC quota feature interface definitions
 *
 */
#ifndef WLAN_HDD_MCC_QUOTA_H
#define WLAN_HDD_MCC_QUOTA_H
#include "qdf_types.h"
#include "qdf_status.h"
#include "qca_vendor.h"
#include <net/cfg80211.h>

struct hdd_context;
struct hdd_adapter;
struct wlan_objmgr_psoc;

#ifdef WLAN_FEATURE_MCC_QUOTA
extern const struct nla_policy
set_mcc_quota_policy[QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX + 1];

/**
 * wlan_hdd_set_mcc_adaptive_sched() - Enable or disable MCC adaptive scheduling
 * @psoc: psoc context
 * @enable: Enable (true) or disable (false)
 *
 * Return: 0 for success, Non zero failure code for errors
 */
int wlan_hdd_set_mcc_adaptive_sched(struct wlan_objmgr_psoc *psoc,
				    bool enable);

/**
 * wlan_hdd_cfg80211_set_mcc_quota() - Set user MCC quota to the target
 * @wiphy: Wireless info object
 * @wdev: Wireless dev object
 * @attr: Command attributes
 * @attr_len: Length of attributes
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_set_mcc_quota(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *attr,
				    int attr_len);

#define	FEATURE_MCC_QUOTA_VENDOR_COMMANDS				   \
{									   \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			   \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MCC_QUOTA,		   \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,\
	.doit = wlan_hdd_cfg80211_set_mcc_quota,			   \
	vendor_command_policy(set_mcc_quota_policy,			   \
			      QCA_WLAN_VENDOR_ATTR_MCC_QUOTA_MAX)	   \
},

#define FEATURE_MCC_QUOTA_VENDOR_EVENTS                         \
[QCA_NL80211_VENDOR_SUBCMD_MCC_QUOTA_INDEX] = {                 \
	.vendor_id = QCA_NL80211_VENDOR_ID,                     \
	.subcmd = QCA_NL80211_VENDOR_SUBCMD_MCC_QUOTA,          \
},

/**
 * wlan_hdd_apply_user_mcc_quota() - Apply the user MCC quota to the target
 * @adapter: pointer to HDD adapter object
 *
 * Return: 0 on success, errno for error
 */
int wlan_hdd_apply_user_mcc_quota(struct hdd_adapter *adapter);

/**
 * wlan_hdd_register_mcc_quota_event_callback() - Register hdd callback to get
 * mcc quota event.
 * @hdd_ctx: pointer to hdd context
 *
 * Return: void
 */
void wlan_hdd_register_mcc_quota_event_callback(struct hdd_context *hdd_ctx);
#else /* WLAN_FEATURE_MCC_QUOTA */
#define	FEATURE_MCC_QUOTA_VENDOR_COMMANDS
#define FEATURE_MCC_QUOTA_VENDOR_EVENTS

static inline int wlan_hdd_apply_user_mcc_quota(struct hdd_adapter *adapter)
{
	return 0;
}

static inline int wlan_hdd_set_mcc_adaptive_sched(struct wlan_objmgr_psoc *psoc,
						  bool enable)
{
	return 0;
}

static inline void
wlan_hdd_register_mcc_quota_event_callback(struct hdd_context *hdd_ctx)
{
}
#endif /* WLAN_FEATURE_MCC_QUOTA */
#endif /* WLAN_HDD_MCC_QUOTA_H */
