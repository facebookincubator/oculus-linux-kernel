/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_dp_api.h
 *
 */
#include <cdp_txrx_cmn_struct.h>

#if !defined(_WLAN_DP_API_H_)
#define _WLAN_DP_API_H_

#ifdef WLAN_SUPPORT_FLOW_PRIORTIZATION
/**
 * wlan_dp_fpm_is_tid_override() - check network buffer marked with tid override
 * @nbuf: pointer to network buffer
 * @tid: buffer to get tid id skb marked with tid override tag
 *
 * Return: True if skb marked with tid override tag
 */
bool wlan_dp_fpm_is_tid_override(qdf_nbuf_t nbuf, uint8_t *tid);
#endif

#endif
