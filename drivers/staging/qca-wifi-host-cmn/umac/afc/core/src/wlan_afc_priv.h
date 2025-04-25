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
 * DOC: wlan_afc_priv.h
 *
 * Declare various struct, macros which are used private to AFC component.
 */

#ifndef __WLAN_AFC_PRIV_H__
#define __WLAN_AFC_PRIV_H__

#include "wlan_afc_main.h"

/**
 * struct wlan_afc_callbacks - AFC PSOC private callbacks
 * @afc_rsp_cp_func: AFC callback function to pass AFC response data to target
 * @afc_req_func: AFC callback function to send AFC request
 * @afc_updated_func: AFC callback function to send AFC update complete event
 */
struct wlan_afc_callbacks {
	send_response_to_afcmem afc_rsp_cp_func;
	osif_send_afc_request afc_req_func;
	osif_send_afc_power_update_complete afc_updated_func;
};

/**
 * struct wlan_afc_psoc_priv - AFC PSOC private data and callbacks
 * @psoc: Pointer to PSOC object
 * @cbs: AFC PSOC callbacks
 */
struct wlan_afc_psoc_priv {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_afc_callbacks cbs;
};
#endif
