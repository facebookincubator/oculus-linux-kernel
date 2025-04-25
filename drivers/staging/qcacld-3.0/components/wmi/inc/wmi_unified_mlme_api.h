/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 *
 */

/**
 * DOC: contains wmi mlme declarations
 */

#ifndef _WLAN_UNIFIED_MLME_API_H_
#define _WLAN_UNIFIED_MLME_API_H_

/*
 * struct csa_event_status_ind - structure for csa event status ind
 * @vdev_id: vdev id
 * @status: accept: 1 reject : 0
 */
struct csa_event_status_ind {
	uint8_t vdev_id;
	uint8_t status;
};

/**
 * wmi_send_csa_event_status_ind
 * @wmi_hdl: wmi handle
 * @params: csa params
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wmi_send_csa_event_status_ind(
					wmi_unified_t wmi_hdl,
					struct csa_event_status_ind params);
#endif
