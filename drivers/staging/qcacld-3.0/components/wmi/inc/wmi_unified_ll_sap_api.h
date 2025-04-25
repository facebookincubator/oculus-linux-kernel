/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "qdf_status.h"
#include "wlan_ll_sap_public_structs.h"
#include "wmi_unified_param.h"

#ifdef WLAN_FEATURE_LL_LT_SAP
/**
 * wmi_unified_audio_transport_switch_resp_send() - Send audio transport switch
 * response to fw
 * @wmi_hdl: WMI handle
 * @req_type: Bearer switch request type
 * @status: Status of the bearer switch request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wmi_unified_audio_transport_switch_resp_send(
					wmi_unified_t wmi_hdl,
					enum bearer_switch_req_type req_type,
					enum bearer_switch_status status);

/**
 * wmi_extract_audio_transport_switch_req_event() - Extract audio transport
 * switch request from fw
 * @wmi_handle: WMI handle
 * @event: WMI event from fw
 * @len: Length of the event
 * @req_type: Type of the bearer switch request from fw
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wmi_extract_audio_transport_switch_req_event(
				wmi_unified_t wmi_handle,
				uint8_t *event, uint32_t len,
				enum bearer_switch_req_type *req_type);
#endif
