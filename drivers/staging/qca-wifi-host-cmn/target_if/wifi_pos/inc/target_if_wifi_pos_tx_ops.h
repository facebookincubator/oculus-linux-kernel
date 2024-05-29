/*
 * Copyright (c) 2017, 2019-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: target_if_wifi_pos_tx_ops.h
 * This file declares the functions pertinent to wifi positioning component's
 * target if layer.
 */
#ifndef _WIFI_POS_TGT_IF_TX_OPS_H_
#define _WIFI_POS_TGT_IF_TX_OPS_H_

#include "qdf_types.h"
#include "qdf_status.h"
#include "wlan_cmn.h"
#include "target_if_wifi_pos_rx_ops.h"

#ifdef WIFI_POS_CONVERGED
/**
 * target_if_wifi_pos_register_tx_ops: function to register with lmac tx ops
 * @tx_ops: lmac tx ops struct object
 *
 * Return: none
 */
void target_if_wifi_pos_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);
#endif
#endif /* _WIFI_POS_TGT_IF_TX_OPS_H_ */
