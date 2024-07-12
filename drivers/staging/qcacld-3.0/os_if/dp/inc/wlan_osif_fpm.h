/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_osif_fpm.h
 *
 * WLAN Host Device Driver fpm related implementation
 *
 */

#if !defined(WLAN_OSIF_FPM_H)
#define WLAN_OSIF_FPM_H

#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION
#include "wlan_hdd_main.h"

int wlan_hdd_cfg80211_vendor_fpm(struct wiphy *wiphy,
				 struct wireless_dev *wdev, const void *data,
				 int data_len);

extern const
struct nla_policy fpm_policy[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_MAX + 1];

extern const
struct nla_policy fpm_policy_param[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_PARAM_MAX + 1];

extern const
struct nla_policy fpm_policy_config[QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_CONFIG_MAX + 1];

#define FEATURE_FLOW_POLICY_COMMANDS					\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_FLOW_POLICY,		\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
		WIPHY_VENDOR_CMD_NEED_NETDEV |				\
		WIPHY_VENDOR_CMD_NEED_RUNNING,				\
	.doit = wlan_hdd_cfg80211_vendor_fpm,				\
	vendor_command_policy(fpm_policy,				\
			      QCA_WLAN_VENDOR_ATTR_FLOW_POLICY_MAX)	\
},

#else
#define FEATURE_FLOW_POLICY_COMMANDS
#endif

#endif /* end #if !defined(WLAN_OSIF_FPM_H) */

