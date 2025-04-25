/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_hdd_afc.h
 *
 * This file has the AFC osif interface with linux kernel.
 */

#ifndef __WLAN_HDD_AFC_H__
#define __WLAN_HDD_AFC_H__
#include <wlan_cfg80211.h>
#include <qca_vendor.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_cfg80211_afc.h>

#ifdef CONFIG_AFC_SUPPORT

#define FEATURE_AFC_VENDOR_COMMANDS \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_RESPONSE, \
	.doit = wlan_hdd_vendor_afc_response, \
	vendor_command_policy(wlan_cfg80211_afc_response_policy, \
			      QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX) \
},

#define FEATURE_AFC_VENDOR_EVENTS \
[QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT_INDEX] = {		   \
	.vendor_id = QCA_NL80211_VENDOR_ID,		   \
	.subcmd = QCA_NL80211_VENDOR_SUBCMD_AFC_EVENT,	   \
},

/**
 * wlan_hdd_vendor_afc_response() - OSIF callback function of NL vendor AFC
 * response command.
 * @wiphy: Pointer to WIPHY object
 * @wdev: Pointer to wireless device
 * @data: Pointer to NL vendor command of AFC response
 * @data_len: Length of NL vendor command of AFC response
 *
 * Return: 0 if success, otherwise error code
 */
int wlan_hdd_vendor_afc_response(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len);

/**
 * wlan_hdd_register_afc_pld_cb() - API to register AFC PLD function to pass
 * AFC response data to target.
 * @psoc: Pointer to PSOC object
 *
 * Return: None
 */
void wlan_hdd_register_afc_pld_cb(struct wlan_objmgr_psoc *psoc);
#else
#define FEATURE_AFC_VENDOR_COMMANDS
#define FEATURE_AFC_VENDOR_EVENTS

static inline void wlan_hdd_register_afc_pld_cb(struct wlan_objmgr_psoc *psoc)
{
}
#endif
#endif
