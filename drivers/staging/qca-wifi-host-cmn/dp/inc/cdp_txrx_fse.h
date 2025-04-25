/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _CDP_TXRX_FSE_H_
#define _CDP_TXRX_FSE_H_

#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
static inline QDF_STATUS
cdp_fse_flow_add(ol_txrx_soc_handle soc,
		 uint32_t *src_ip, uint32_t src_port,
		 uint32_t *dest_ip, uint32_t dest_port,
		 uint8_t protocol, uint8_t version)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->fse_ops ||
	    !soc->ops->fse_ops->fse_rule_add) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->fse_ops->fse_rule_add(soc,
					       src_ip, src_port,
					       dest_ip, dest_port,
					       protocol, version);
}

static inline QDF_STATUS
cdp_fse_flow_delete(ol_txrx_soc_handle soc,
		    uint32_t *src_ip, uint32_t src_port,
		    uint32_t *dest_ip, uint32_t dest_port,
		    uint8_t protocol, uint8_t version)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->fse_ops ||
	    !soc->ops->fse_ops->fse_rule_delete) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->fse_ops->fse_rule_delete(soc,
						  src_ip, src_port,
						  dest_ip, dest_port,
						  protocol, version);
}

#endif /* WLAN_SUPPORT_RX_FLOW_TAG */
#endif /* _CDP_TXRX_FSE_H_ */
