/*
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
 * DOC: This file contains pkt_capture public API's exposed.
 */

#include "wlan_pkt_capture_api.h"
#include "wlan_pkt_capture_main.h"

bool wlan_pkt_capture_is_tx_mgmt_enable(struct wlan_objmgr_pdev *pdev)
{
	return pkt_capture_is_tx_mgmt_enable(pdev);
}

QDF_STATUS
wlan_pkt_capture_register_callbacks(struct wlan_objmgr_vdev *vdev,
				    QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t),
				    void *context)
{
	return pkt_capture_register_callbacks(vdev, mon_cb, context);
}
