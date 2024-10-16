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
 * DOC: offload lmac interface APIs for P2P MCC quota event processing
 */

#ifndef _TARGET_IF_P2P_MCC_QUOTA_H_
#define _TARGET_IF_P2P_MCC_QUOTA_H_

struct wlan_lmac_if_tx_ops;

#ifdef WLAN_FEATURE_MCC_QUOTA
/**
 * target_if_mcc_quota_register_tx_ops() - Register mcc quota TX OPS
 * @tx_ops: lmac if transmit ops
 *
 * Return: None
 */
void
target_if_mcc_quota_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);
#else
static inline void
target_if_mcc_quota_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
}
#endif
#endif /* _TARGET_IF_P2P_MCC_QUOTA_H_ */
