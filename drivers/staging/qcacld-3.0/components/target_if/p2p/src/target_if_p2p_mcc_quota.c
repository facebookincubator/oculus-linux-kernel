/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: offload lmac interface APIs implementation for P2P mcc quota event
 * processing
 */

#include <wmi_unified_api.h>
#include "wlan_p2p_mcc_quota_public_struct.h"
#include "target_if.h"
#include "target_if_p2p_mcc_quota.h"

/**
 * target_if_mcc_quota_event_handler() - WMI callback for mcc_quota
 * @scn:       pointer to scn
 * @data:      event buffer
 * @datalen:   buffer length
 *
 * This function gets called from WMI when triggered WMI event
 * WMI_RESMGR_CHAN_TIME_QUOTA_CHANGED_EVENTID
 *
 * Return: 0 - success, others - failure
 */
static int target_if_mcc_quota_event_handler(ol_scn_t scn, uint8_t *data,
					     uint32_t datalen)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct mcc_quota_info *event_info;
	struct wlan_lmac_if_rx_ops *rx_ops;
	struct wlan_lmac_if_p2p_rx_ops *p2p_rx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	target_if_debug("scn:%pK, data:%pK, datalen:%d", scn, data, datalen);

	if (!scn || !data) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, data);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("null wmi handle");
		return -EINVAL;
	}

	event_info = qdf_mem_malloc(sizeof(*event_info));
	if (!event_info)
		return -ENOMEM;

	if (wmi_extract_mcc_quota_ev_param(wmi_handle, data,
					   event_info)) {
		target_if_err("failed to extract mcc quota event");
		qdf_mem_free(event_info);
		return -EINVAL;
	}
	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		target_if_err("failed to get soc rx ops");
		qdf_mem_free(event_info);
		return -EINVAL;
	}
	p2p_rx_ops = &rx_ops->p2p;
	if (p2p_rx_ops->mcc_quota_ev_handler) {
		status = p2p_rx_ops->mcc_quota_ev_handler(psoc, event_info);
		if (QDF_IS_STATUS_ERROR(status))
			target_if_debug("quota event handler, status:%d",
					status);
	} else {
		target_if_debug("no valid mcc quota event handler");
	}
	qdf_mem_free(event_info);

	return qdf_status_to_os_return(status);
}

/**
 * target_if_register_mcc_quota_event_handler() - Register or unregister
 * mcc quota wmi event handler
 * @psoc: psoc object
 * @reg: register or unregister flag
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS
target_if_register_mcc_quota_event_handler(struct wlan_objmgr_psoc *psoc,
					   bool reg)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}
	if (reg) {
		status = wmi_unified_register_event_handler(wmi_handle,
							    wmi_resmgr_chan_time_quota_changed_eventid,
							    target_if_mcc_quota_event_handler,
							    WMI_RX_SERIALIZER_CTX);

		target_if_debug("wmi register mcc_quota event handle, status:%d",
				status);
	} else {
		status = wmi_unified_unregister_event_handler(wmi_handle,
							      wmi_resmgr_chan_time_quota_changed_eventid);

		target_if_debug("wmi unregister mcc_quota event handle, status:%d",
				status);
	}

	return status;
}

void target_if_mcc_quota_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_p2p_tx_ops *p2p_tx_ops = &tx_ops->p2p;

	p2p_tx_ops->reg_mcc_quota_ev_handler =
			target_if_register_mcc_quota_event_handler;
}
