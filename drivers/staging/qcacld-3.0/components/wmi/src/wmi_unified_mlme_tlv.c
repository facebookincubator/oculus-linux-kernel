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

#include <wmi_unified_priv.h>
#include "wmi.h"
#include "wlan_mlme_api.h"
#include "wmi_unified_ll_sap_tlv.h"

static QDF_STATUS csa_event_status_ind_tlv(wmi_unified_t wmi_handle,
					   struct csa_event_status_ind params)
{
	wmi_csa_event_status_ind_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS status;

	buf = wmi_buf_alloc(wmi_handle, sizeof(*cmd));
	if (!buf)
		return QDF_STATUS_E_FAILURE;

	cmd = (wmi_csa_event_status_ind_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_csa_event_status_ind_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_csa_event_status_ind_fixed_param));

	cmd->vdev_id = params.vdev_id;
	cmd->status = params.status;

	wmi_debug("vdev_id: %d status: %d ", cmd->vdev_id, cmd->status);

	status = wmi_unified_cmd_send(wmi_handle, buf, sizeof(*cmd),
				      WMI_CSA_EVENT_STATUS_INDICATION_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

#ifdef WLAN_FEATURE_LL_LT_SAP
/**
 * wmi_mlme_attach_ll_lt_sap_tlv() - attach ll_lt_sap tlv handlers
 * @ops: wmi ops
 *
 * Return: void
 */
static void wmi_mlme_attach_ll_lt_sap_tlv(struct wmi_ops *ops)
{
	ops->send_audio_transport_switch_resp = audio_transport_switch_resp_tlv;
	ops->extract_audio_transport_switch_req_event =
				extract_audio_transport_switch_req_event_tlv;
}
#else
static inline void wmi_mlme_attach_ll_lt_sap_tlv(struct wmi_ops *ops)
{
}
#endif

/**
 * wmi_mlme_attach_tlv() - attach MLME tlv handlers
 * @wmi_handle: wmi handle
 *
 * Return: void
 */
void wmi_mlme_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_csa_event_status_ind = csa_event_status_ind_tlv;
	wmi_mlme_attach_ll_lt_sap_tlv(ops);
}
