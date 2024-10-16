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
 * DOC: wlan_cfg80211_wifi_pos.c
 * defines wifi-pos module related driver functions interfacing with linux
 * kernel
 */
#include "wlan_cfg80211.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_cfg80211_wifi_pos.h"
#include "wlan_cmn_ieee80211.h"
#include "wifi_pos_ucfg_i.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)

u8 wlan_extended_caps_iface[WLAN_EXTCAP_IE_MAX_LEN] = {0};
u8 wlan_extended_caps_iface_mask[WLAN_EXTCAP_IE_MAX_LEN] = {0};

struct wiphy_iftype_ext_capab iftype_ext_cap;

#define WLAN_EXT_RANGING_CAP_IDX  11
void
wlan_wifi_pos_cfg80211_set_wiphy_ext_feature(struct wiphy *wiphy,
					     struct wlan_objmgr_psoc *psoc)
{
	bool enable_rsta_11az_ranging;

	enable_rsta_11az_ranging = ucfg_wifi_pos_get_rsta_11az_ranging_cap();
	if (!enable_rsta_11az_ranging)
		return;

	if (wlan_psoc_nif_fw_ext_cap_get(psoc, WLAN_RTT_11AZ_NTB_SUPPORT)) {
		wlan_extended_caps_iface[WLAN_EXT_RANGING_CAP_IDX] |=
					WLAN_EXT_CAPA11_NTB_RANGING_RESPONDER;
		wlan_extended_caps_iface_mask[WLAN_EXT_RANGING_CAP_IDX] |=
					WLAN_EXT_CAPA11_NTB_RANGING_RESPONDER;
	}

	if (wlan_psoc_nif_fw_ext2_cap_get(psoc, WLAN_RTT_11AZ_TB_SUPPORT)) {
		wlan_extended_caps_iface[WLAN_EXT_RANGING_CAP_IDX] |=
					WLAN_EXT_CAPA11_TB_RANGING_RESPONDER;
		wlan_extended_caps_iface_mask[WLAN_EXT_RANGING_CAP_IDX] |=
					WLAN_EXT_CAPA11_TB_RANGING_RESPONDER;
	}

	iftype_ext_cap.iftype = NL80211_IFTYPE_AP;
	iftype_ext_cap.extended_capabilities =
				wlan_extended_caps_iface,
	iftype_ext_cap.extended_capabilities_mask =
				wlan_extended_caps_iface_mask,
	iftype_ext_cap.extended_capabilities_len =
				ARRAY_SIZE(wlan_extended_caps_iface),

	wiphy->num_iftype_ext_capab = 0;
	wiphy->iftype_ext_capab = &iftype_ext_cap;
	wiphy->num_iftype_ext_capab++;
}

#define NUM_BITS_IN_BYTE       8
static void
wlan_wifi_pos_set_feature_flags(uint8_t *feature_flags,
				enum qca_wlan_vendor_features feature)
{
	uint32_t index;
	uint8_t bit_mask;

	index = feature / NUM_BITS_IN_BYTE;
	bit_mask = 1 << (feature % NUM_BITS_IN_BYTE);
	feature_flags[index] |= bit_mask;
}

void wlan_wifi_pos_cfg80211_set_features(struct wlan_objmgr_psoc *psoc,
					 uint8_t *feature_flags)
{
	bool rsta_secure_ltf_support, enable_rsta_11az_ranging;

	enable_rsta_11az_ranging = ucfg_wifi_pos_get_rsta_11az_ranging_cap();
	rsta_secure_ltf_support = enable_rsta_11az_ranging &&
				wifi_pos_get_rsta_sec_ltf_cap();
	if (wlan_psoc_nif_fw_ext2_cap_get(psoc,
					  WLAN_RTT_11AZ_MAC_PHY_SEC_SUPPORT)) {
		wlan_wifi_pos_set_feature_flags(feature_flags,
						QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_STA);
		if (rsta_secure_ltf_support)
			wlan_wifi_pos_set_feature_flags(feature_flags,
							QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_AP);
	}

	if (wlan_psoc_nif_fw_ext2_cap_get(psoc,
					  WLAN_RTT_11AZ_MAC_SEC_SUPPORT)) {
		wlan_wifi_pos_set_feature_flags(feature_flags,
			QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_STA);
		if (rsta_secure_ltf_support)
			wlan_wifi_pos_set_feature_flags(feature_flags,
							QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_AP);
	}
}
#endif
