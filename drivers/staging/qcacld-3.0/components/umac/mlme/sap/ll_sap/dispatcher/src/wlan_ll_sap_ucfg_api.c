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

/**
 * DOC: This file contains ll_sap north bound interface definitions
 */
#include "../../core/src/wlan_ll_sap_main.h"
#include "../../core/src/wlan_ll_lt_sap_main.h"
#include "../../core/src/wlan_ll_lt_sap_bearer_switch.h"
#include <wlan_ll_sap_ucfg_api.h>

QDF_STATUS ucfg_ll_sap_init(void)
{
	return ll_sap_init();
}

QDF_STATUS ucfg_ll_sap_deinit(void)
{
	return ll_sap_deinit();
}

bool ucfg_is_ll_lt_sap_supported(struct wlan_objmgr_psoc *psoc)
{
	return ll_lt_sap_is_supported(psoc);
}

QDF_STATUS ucfg_ll_lt_sap_request_for_audio_transport_switch(
					struct wlan_objmgr_vdev *vdev,
					enum bearer_switch_req_type req_type)
{
	return ll_lt_sap_request_for_audio_transport_switch(vdev, req_type);
}

void ucfg_ll_lt_sap_deliver_audio_transport_switch_resp(
					struct wlan_objmgr_vdev *vdev,
					enum bearer_switch_req_type req_type,
					enum bearer_switch_status status)
{
	ll_lt_sap_deliver_audio_transport_switch_resp(vdev, req_type,
						      status);
}

void ucfg_ll_sap_register_cb(struct ll_sap_ops *ll_sap_global_ops)
{
	ll_sap_register_os_if_cb(ll_sap_global_ops);
}

void ucfg_ll_sap_unregister_cb(void)
{
	ll_sap_unregister_os_if_cb();
}

QDF_STATUS ucfg_ll_sap_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return ll_sap_psoc_enable(psoc);
}

QDF_STATUS ucfg_ll_sap_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return ll_sap_psoc_disable(psoc);
}
