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
 * DOC: wlan_afc_ucfg_api.h
 *
 * This file has the prototypes of AFC dispatcher API which is exposed
 * to outside of AFC component.
 */

#ifndef __WLAN_AFC_UCFG_API_H__
#define __WLAN_AFC_UCFG_API_H__

#include <wlan_afc_main.h>

#ifdef CONFIG_AFC_SUPPORT
/**
 * ucfg_afc_register_data_send_cb() - UCFG API to register AFC data send
 * callback function to pass AFC response data to target.
 * @psoc: Pointer to PSOC object
 * @func: Pointer to PLD AFC function to pass AFC response data to target
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_afc_register_data_send_cb(struct wlan_objmgr_psoc *psoc,
					  send_response_to_afcmem func);
/**
 * ucfg_afc_data_send() - UCFG API to send AFC response data to target
 * @psoc: Pointer to PSOC object
 * @pdev: Pointer to PDEV object
 * @data: Pointer to AFC response data which pass to target
 * @len: Length of AFC response data
 *
 * Return: 0 if success, otherwise error code
 */
int ucfg_afc_data_send(struct wlan_objmgr_psoc *psoc,
		       struct wlan_objmgr_pdev *pdev,
		       struct wlan_afc_host_resp *data,
		       uint32_t len);
/**
 * ucfg_afc_init() - UCFG API to initialize AFC component
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_afc_init(void);

/**
 * ucfg_afc_deinit() - UCFG API to deinitialize AFC component
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_afc_deinit(void);
#else
static inline QDF_STATUS ucfg_afc_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_afc_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif
