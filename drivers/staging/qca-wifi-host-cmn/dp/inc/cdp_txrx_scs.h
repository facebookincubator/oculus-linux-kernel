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

#ifndef _CDP_TXRX_SCS_H_
#define _CDP_TXRX_SCS_H_

#ifdef WLAN_SUPPORT_SCS
#include "cdp_txrx_handle.h"

/**
 * cdp_scs_peer_lookup_n_rule_match() - Find peer and check if SCS rule
 *  is applicable for the peer or not
 *
 * @soc: soc handle
 * @rule_id: scs rule id
 * @dst_mac_addr: destination mac addr for peer lookup
 *
 * Return: bool true on success and false on failure
 */
static inline
bool cdp_scs_peer_lookup_n_rule_match(ol_txrx_soc_handle soc,
				      uint32_t rule_id, uint8_t *dst_mac_addr)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return false;
	}

	if (!soc->ops->scs_ops ||
	    !soc->ops->scs_ops->scs_peer_lookup_n_rule_match)
		return false;

	return soc->ops->scs_ops->scs_peer_lookup_n_rule_match(soc, rule_id,
							       dst_mac_addr);
}
#endif
#endif
