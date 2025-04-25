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
 * DOC: wlan_cfg80211_afc.h
 *
 * This Header file provide declaration for cfg80211 vendor command handler API
 * and structures.
 */

#ifndef __WLAN_CFG80211_AFC_H__
#define __WLAN_CFG80211_AFC_H__

#include <wlan_objmgr_cmn.h>
#include <qdf_types.h>
#include <net/cfg80211.h>
#include <qca_vendor.h>
#include <wlan_reg_ucfg_api.h>

#ifdef CONFIG_AFC_SUPPORT

extern const struct nla_policy
wlan_cfg80211_afc_response_policy[QCA_WLAN_VENDOR_ATTR_AFC_RESP_MAX + 1];

/**
 * wlan_cfg80211_vendor_afc_response() - To handle vendor AFC response command
 * @psoc: Pointer to PSOC object
 * @pdev: Pointer to PDEV object
 * @data: Pointer to the data received via the vendor interface
 * @data_len: Length of the data to be passed
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_vendor_afc_response(struct wlan_objmgr_psoc *psoc,
				      struct wlan_objmgr_pdev *pdev,
				      const void *data,
				      int data_len);

/**
 * wlan_cfg80211_afc_send_request() - To handle AFC request from
 * regulatory AFC component.
 * @pdev: Pointer to PDEV object
 * @afc_req: Pointer to AFC request
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_cfg80211_afc_send_request(struct wlan_objmgr_pdev *pdev,
				   struct wlan_afc_host_request *afc_req);

/**
 * wlan_cfg80211_afc_send_update_complete() - To handle AFC update complete
 * event from regulatory AFC component.
 * @pdev: Pointer to PDEV object
 * @afc_evt: Pointer to AFC power update complete event
 *
 * Return: 0 on success, negative errno on failure
 */
int
wlan_cfg80211_afc_send_update_complete(struct wlan_objmgr_pdev *pdev,
				       struct reg_fw_afc_power_event *afc_evt);
#endif
#endif
