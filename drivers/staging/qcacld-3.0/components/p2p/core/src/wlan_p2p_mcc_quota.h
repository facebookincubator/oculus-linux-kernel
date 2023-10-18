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
 * DOC: Defines main MCC Quota core functions
 */

#ifndef _WLAN_P2P_MCC_QUOTA_H_
#define _WLAN_P2P_MCC_QUOTA_H_

struct wlan_objmgr_psoc;
struct mcc_quota_info;
/**
 * p2p_mcc_quota_event_process() - Process mcc quota event
 * @psoc: psoc object
 * @event_info: mcc quota info
 *
 * This function handles the target mcc quota event to indicate
 * the quota information to existing vdev's interface.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_mcc_quota_event_process(struct wlan_objmgr_psoc *psoc,
				       struct mcc_quota_info *event_info);
#endif
