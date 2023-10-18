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
 * DOC: This file contains mcc quota event processing south bound interface
 * API implementation
 */

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_lmac_if_def.h>
#include "wlan_p2p_public_struct.h"
#include "../../core/src/wlan_p2p_main.h"
#include "../../core/src/wlan_p2p_mcc_quota.h"
#include "wlan_p2p_mcc_quota_tgt_api.h"

QDF_STATUS tgt_p2p_mcc_quota_event_cb(struct wlan_objmgr_psoc *psoc,
				      struct mcc_quota_info *event_info)
{
	if (!event_info) {
		p2p_err("invalid mcc quota event information");
		return QDF_STATUS_E_INVAL;
	}

	if (!psoc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return p2p_mcc_quota_event_process(psoc, event_info);
}

QDF_STATUS
tgt_p2p_register_mcc_quota_ev_handler(struct wlan_objmgr_psoc *psoc,
				      bool reg)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	struct wlan_lmac_if_p2p_tx_ops *p2p_tx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		p2p_err("invalid lmac if tx ops");
		return QDF_STATUS_E_FAILURE;
	}
	p2p_tx_ops = &tx_ops->p2p;
	if (p2p_tx_ops->reg_mcc_quota_ev_handler)
		status = p2p_tx_ops->reg_mcc_quota_ev_handler(psoc, reg);

	p2p_debug("register %d mcc quota event, status:%d",
		  reg, status);

	return status;
}
