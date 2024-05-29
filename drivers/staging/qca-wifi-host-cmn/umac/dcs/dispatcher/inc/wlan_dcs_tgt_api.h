/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_dcs_tgt_api.h
 *
 * This header file provide with API declarations to interface with Southbound
 */
#ifndef __WLAN_DCS_TGT_API_H__
#define __WLAN_DCS_TGT_API_H__

#include <wlan_objmgr_cmn.h>
#include <wlan_dcs_public_structs.h>

/**
 * tgt_dcs_process_event(): dcs FW event process
 * @psoc: pointer to psoc object
 * @event: pointer to dcs event
 *
 * This function gets called to process dcs FW event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_dcs_process_event(struct wlan_objmgr_psoc *psoc,
				 struct wlan_host_dcs_event *event);
#ifdef CONFIG_AFC_SUPPORT
/**
 * tgt_afc_trigger_dcs() - AFC event DCS process
 * @pdev: pointer to pdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_afc_trigger_dcs(struct wlan_objmgr_pdev *pdev);
#endif
#endif /* __WLAN_DCS_TGT_API_H__ */
