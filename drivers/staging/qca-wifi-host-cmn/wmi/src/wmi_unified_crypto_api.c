/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Implement API's specific to crypto component.
 */

#include "wmi_unified_priv.h"
#include "wmi_unified_param.h"
#include "wmi_unified_crypto_api.h"

QDF_STATUS
wmi_extract_install_key_comp_event(wmi_unified_t wmi_handle, void *evt_buf,
				   uint32_t len,
				   struct wmi_install_key_comp_event *param)
{
	if (wmi_handle->ops->extract_install_key_comp_event)
		return wmi_handle->ops->extract_install_key_comp_event(
			wmi_handle, evt_buf, len, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_send_vdev_set_ltf_key_seed_cmd(wmi_unified_t wmi,
				   struct wlan_crypto_ltf_keyseed_data *data)
{
	if (wmi->ops->send_vdev_set_ltf_key_seed_cmd)
		return wmi->ops->send_vdev_set_ltf_key_seed_cmd(wmi, data);

	return QDF_STATUS_E_FAILURE;
}
