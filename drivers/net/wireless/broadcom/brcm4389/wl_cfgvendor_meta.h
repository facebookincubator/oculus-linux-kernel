// Copyright (c) Meta Platforms, Inc. and affiliates.

#ifndef _wl_cfgvendor_meta__h_
#define _wl_cfgvendor_meta__h_

#include "wl_cfgvendor.h"

#define OUI_META  0x2C2617

/* @META_SUBCMD_SET_TXCHAIN: Set Tx chain for WLAN device
 *	For P2PGO and STA interfaces, META_ATTR_MAC can be specified
 */
enum meta_vendor_subcmd {
	META_SUBCMD_SET_WIFI_CONFIG = META_NL80211_SUBCMD_RANGE_START,
};

enum meta_vendor_attr_config {
	META_VENDOR_ATTR_CONFIG_INVALID     = 0,
	META_VENDOR_ATTR_CONFIG_TXCHAIN     = 1,
	META_VENDOR_ATTR_CONFIG_ELNA_BYPASS = 2,
	META_VENDOR_ATTR_CONFIG_SIZE,
	META_VENDOR_ATTR_CONFIG_MAX = META_VENDOR_ATTR_CONFIG_SIZE - 1
};

int
wl_cfgvendor_meta_set_wifi_config(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data,
				int len);

#endif /* _wl_cfgvendor_meta__h_ */
