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
 * DOC: Contains p2p mcc quota event south bound interface definitions
 */

#ifndef _WLAN_P2P_MCC_QUOTA_TGT_API_H_
#define _WLAN_P2P_MCC_QUOTA_TGT_API_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;

#ifdef WLAN_FEATURE_MCC_QUOTA
/**
 * tgt_p2p_register_mcc_quota_ev_handler() - register/unregister mcc quota
 * wmi event handler
 * @psoc: psoc object
 * @reg: true for register, false for unregister
 *
 * This function will register or unregister mcc quota event handler
 * in target if.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_register_mcc_quota_ev_handler(struct wlan_objmgr_psoc *psoc,
						 bool reg);

/**
 * tgt_p2p_mcc_quota_event_cb() - mcc quota event callback handler
 * @psoc: psoc object
 * @event_info: mcc quota event
 *
 * This function will be called by target if to indicate mcc quota event
 * to stack.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS  tgt_p2p_mcc_quota_event_cb(struct wlan_objmgr_psoc *psoc,
				       struct mcc_quota_info *event_info);
#else
static inline
QDF_STATUS tgt_p2p_register_mcc_quota_ev_handler(struct wlan_objmgr_psoc *psoc,
						 bool reg)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif /* _WLAN_P2P_MCC_QUOTA_TGT_API_H_ */
