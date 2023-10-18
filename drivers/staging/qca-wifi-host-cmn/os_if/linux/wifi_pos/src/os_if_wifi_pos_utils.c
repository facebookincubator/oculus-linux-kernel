/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: os_if_wifi_pos_utils.c
 * This file defines the important functions pertinent to wifi positioning
 * component's os_if layer.
 */

#include "wlan_objmgr_psoc_obj.h"
#include "os_if_wifi_pos_utils.h"
#include "wifi_pos_ucfg_i.h"
#include "os_if_wifi_pos.h"

#if defined(WIFI_POS_CONVERGED) && defined(WLAN_FEATURE_RTT_11AZ_SUPPORT)
static struct wifi_pos_osif_ops osif_ops = {
	.osif_initiate_pasn_cb = os_if_wifi_pos_initiate_pasn_auth,
};

QDF_STATUS
osif_wifi_pos_register_ops(struct wlan_objmgr_psoc *psoc)
{
	return ucfg_wifi_pos_register_osif_callbacks(psoc, &osif_ops);
}

QDF_STATUS
osif_wifi_pos_deregister_ops(struct wlan_objmgr_psoc *psoc)
{
	return ucfg_wifi_pos_deregister_osif_callbacks(psoc);
}
#endif
