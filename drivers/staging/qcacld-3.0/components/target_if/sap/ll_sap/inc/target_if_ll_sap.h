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
 * DOC: contains ll_lt_sap declarations specific to the bearer
 * switch functionalities
 */
#include "wlan_ll_sap_public_structs.h"

/**
 * target_if_ll_sap_register_tx_ops() - register ll_lt_sap tx ops
 * @tx_ops: pointer to ll_sap tx ops
 *
 * Return: None
 */
void target_if_ll_sap_register_tx_ops(struct wlan_ll_sap_tx_ops *tx_ops);

/**
 * target_if_ll_sap_register_rx_ops() - register ll_lt_sap rx ops
 * @rx_ops: pointer to ll_sap rx ops
 *
 * Return: None
 */
void target_if_ll_sap_register_rx_ops(struct wlan_ll_sap_rx_ops *rx_ops);

/**
 * target_if_ll_sap_register_events() - register ll_lt_sap fw event handlers
 * @psoc: pointer to psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS target_if_ll_sap_register_events(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_ll_sap_deregister_events() - deregister ll_lt_sap fw event handlers
 * @psoc: pointer to psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_ll_sap_deregister_events(struct wlan_objmgr_psoc *psoc);

