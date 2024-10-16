/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
 * DOC: contains coex target if declarations
 */
#ifndef __TARGET_IF_COEX_H__
#define __TARGET_IF_COEX_H__

#include <target_if.h>
#include "wlan_coex_public_structs.h"

/**
 * target_if_coex_register_tx_ops() - Register coex target_if tx ops
 * @tx_ops: pointer to target if tx ops
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS
target_if_coex_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);

#ifdef WLAN_FEATURE_DBAM_CONFIG
/**
 * target_if_dbam_register_tx_ops() - Register dbam target_if tx ops
 * @tx_ops: pointer to target if tx ops
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS
target_if_dbam_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);

/**
 * target_if_dbam_process_event() - dbam response function handler
 * @psoc: pointer to psoc
 * @resp: response received from FW to dbam config command
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS
target_if_dbam_process_event(struct wlan_objmgr_psoc *psoc,
			     enum coex_dbam_comp_status resp);
#endif
#endif
