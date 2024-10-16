/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_wifi_pos_pasn.h
 *
 * WLAN Host Device Driver WIFI POSITION module PASN authentication
 * feature interface definitions
 *
 */
#ifndef __WLAN_HDD_WIFI_POS_PASN_H__
#define __WLAN_HDD_WIFI_POS_PASN_H__
#include "qdf_types.h"
#include "qdf_status.h"
#include "qca_vendor.h"
#include <net/cfg80211.h>
#include "wlan_hdd_object_manager.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)

extern const struct nla_policy
wifi_pos_pasn_auth_status_policy[QCA_WLAN_VENDOR_ATTR_MAX + 1];

extern const struct nla_policy
wifi_pos_pasn_auth_policy[QCA_WLAN_VENDOR_ATTR_PASN_PEER_MAX + 1];

extern const struct nla_policy
wifi_pos_pasn_set_ranging_ctx_policy[QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX + 1];

int wlan_hdd_wifi_pos_send_pasn_auth_status(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len);

#define FEATURE_WIFI_POS_11AZ_AUTH_COMMANDS                                  \
{                                                                            \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                             \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PASN,                       \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,  \
	.doit = wlan_hdd_wifi_pos_send_pasn_auth_status,                     \
	vendor_command_policy(wifi_pos_pasn_auth_status_policy,              \
			      QCA_WLAN_VENDOR_ATTR_MAX)                      \
},

int
wlan_hdd_cfg80211_set_secure_ranging_context(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len);

#define FEATURE_WIFI_POS_SET_SECURE_RANGING_CONTEXT_COMMANDS                 \
{                                                                            \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                             \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SECURE_RANGING_CONTEXT,     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,  \
	.doit = wlan_hdd_cfg80211_set_secure_ranging_context,                \
	vendor_command_policy(wifi_pos_pasn_set_ranging_ctx_policy,          \
			      QCA_WLAN_VENDOR_ATTR_SECURE_RANGING_CTX_MAX)   \
},
#else
#define FEATURE_WIFI_POS_11AZ_AUTH_COMMANDS
#define FEATURE_WIFI_POS_SET_SECURE_RANGING_CONTEXT_COMMANDS
#endif /* WIFI_POS_CONVERGED && WLAN_FEATURE_RTT_11AZ_SUPPORT */
#endif /* __WLAN_HDD_WIFI_POS_PASN_H__ */
