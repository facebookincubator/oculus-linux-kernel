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

#ifndef _OS_IF_DP_SVC_H_
#define _OS_IF_DP_SVC_H_

#include "qdf_types.h"
#include "qca_vendor.h"
#include "wlan_cfg80211.h"

#ifdef WLAN_SUPPORT_SERVICE_CLASS

extern const struct nla_policy
set_service_class_cmd_policy[QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX + 1];

#define FEATURE_SERVICE_CLASS_COMMANDS				           \
	{								   \
		.info.vendor_id = QCA_NL80211_VENDOR_ID,		   \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SDWF_PHY_OPS,	   \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |			   \
			WIPHY_VENDOR_CMD_NEED_NETDEV,			   \
		.doit = wlan_hdd_cfg80211_service_class_cmd,		   \
		vendor_command_policy(set_service_class_cmd_policy,	   \
				      QCA_WLAN_VENDOR_ATTR_SDWF_PHY_MAX)   \
	},								   \

QDF_STATUS os_if_dp_service_class_cmd(struct wiphy *wiphy,
				      const void *data, int data_len);

#else

#define FEATURE_SERVICE_CLASS_COMMANDS

static inline
QDF_STATUS os_if_dp_service_class_cmd(struct wiphy *wiphy,
				      const void *data, int data_len)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#endif
