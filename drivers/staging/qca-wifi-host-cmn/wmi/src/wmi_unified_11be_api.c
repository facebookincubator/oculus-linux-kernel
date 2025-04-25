/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Implement API's specific to 11be.
 */

#include "wmi_unified_11be_api.h"

/**
 * wmi_extract_mlo_link_set_active_resp() - extract mlo link set active resp
 *  from event
 * @wmi: WMI handle for this pdev
 * @evt_buf: pointer to event buffer
 * @resp: Pointer to hold mlo link set active resp
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS
wmi_extract_mlo_link_set_active_resp(wmi_unified_t wmi,
				     void *evt_buf,
				     struct mlo_link_set_active_resp *resp)
{
	if (wmi->ops->extract_mlo_link_set_active_resp) {
		return wmi->ops->extract_mlo_link_set_active_resp(wmi,
								  evt_buf,
								  resp);
	}
	return QDF_STATUS_E_FAILURE;
}

/**
 * wmi_send_mlo_link_set_active_cmd() - send mlo link set active command
 * @wmi: WMI handle for this pdev
 * @param: Pointer to mlo link set active param
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS
wmi_send_mlo_link_set_active_cmd(wmi_unified_t wmi,
				 struct mlo_link_set_active_param *param)
{
	if (wmi->ops->send_mlo_link_set_active_cmd)
		return wmi->ops->send_mlo_link_set_active_cmd(wmi, param);

	return QDF_STATUS_E_FAILURE;
}

#ifdef WLAN_FEATURE_11BE
QDF_STATUS wmi_send_mlo_peer_tid_to_link_map_cmd(
		wmi_unified_t wmi,
		struct wmi_host_tid_to_link_map_params *params,
		bool t2lm_info)
{
	if (wmi->ops->send_mlo_peer_tid_to_link_map)
		return wmi->ops->send_mlo_peer_tid_to_link_map(wmi, params, t2lm_info);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_send_mlo_vdev_tid_to_link_map_cmd(
		wmi_unified_t wmi,
		struct wmi_host_tid_to_link_map_ap_params *params)
{
	if (wmi->ops->send_mlo_vdev_tid_to_link_map)
		return wmi->ops->send_mlo_vdev_tid_to_link_map(wmi, params);

	return QDF_STATUS_E_FAILURE;
}

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
QDF_STATUS
wmi_send_mlo_link_switch_req_cnf_cmd(wmi_unified_t wmi,
				     struct wlan_mlo_link_switch_cnf *params)
{
	if (wmi->ops->send_mlo_link_switch_req_cnf_cmd)
		return wmi->ops->send_mlo_link_switch_req_cnf_cmd(wmi, params);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_mlo_link_switch_request_evt(struct wmi_unified *wmi,
					void *buf,
					struct wlan_mlo_link_switch_req *req)
{
	if (wmi->ops->extract_mlo_link_switch_request_event)
		return wmi->ops->extract_mlo_link_switch_request_event(wmi,
								       buf,
								       req);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_mlo_link_state_switch_evt(struct wmi_unified *wmi,
				      void *buf, uint8_t len,
				      struct mlo_link_switch_state_info *info)
{
	if (wmi->ops->extract_mlo_link_state_switch_evt)
		return wmi->ops->extract_mlo_link_state_switch_evt(
							wmi, buf, len, info);

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */

QDF_STATUS wmi_send_mlo_link_state_request_cmd(
		wmi_unified_t wmi,
		struct wmi_host_link_state_params *params)
{
	if (wmi->ops->send_mlo_link_state_request)
		return wmi->ops->send_mlo_link_state_request(wmi, params);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_send_link_set_bss_params_cmd(
		wmi_unified_t wmi,
		struct wmi_host_link_bss_params *params)
{
	if (wmi->ops->send_link_set_bss_params_cmd)
		return wmi->ops->send_link_set_bss_params_cmd(wmi, params);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_mlo_vdev_tid_to_link_map_event(
		wmi_unified_t wmi, void *evt_buf,
		struct mlo_vdev_host_tid_to_link_map_resp *resp)
{
	if (wmi->ops->extract_mlo_vdev_tid_to_link_map_event) {
		return wmi->ops->extract_mlo_vdev_tid_to_link_map_event(wmi,
									evt_buf,
									resp);
	}
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_mlo_vdev_bcast_tid_to_link_map_event(
				     wmi_unified_t wmi,
				     void *evt_buf,
				     struct mlo_bcast_t2lm_info *bcast)
{
	if (wmi->ops->extract_mlo_vdev_bcast_tid_to_link_map_event) {
		return wmi->ops->extract_mlo_vdev_bcast_tid_to_link_map_event(
						wmi,
						evt_buf,
						bcast);
	}
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_mlo_link_state_info_event(
		wmi_unified_t wmi,
		void *evt_buf,
		struct ml_link_state_info_event *params)
{
	if (wmi->ops->extract_mlo_link_state_event)
		return wmi->ops->extract_mlo_link_state_event(
				wmi, evt_buf, params);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_mlo_link_disable_request_evt(
		struct wmi_unified *wmi,
		void *buf,
		struct mlo_link_disable_request_evt_params *params)
{
	if (wmi->ops->extract_mlo_link_disable_request_evt_param)
		return wmi->ops->extract_mlo_link_disable_request_evt_param(
							wmi, buf, params);

	return QDF_STATUS_E_FAILURE;
}
#endif /* WLAN_FEATURE_11BE */

QDF_STATUS
wmi_extract_mgmt_rx_ml_cu_params(wmi_unified_t wmi, void *evt_buf,
				 struct mlo_mgmt_ml_info *cu_params)
{
	if (wmi->ops->extract_mgmt_rx_ml_cu_params)
		return wmi->ops->extract_mgmt_rx_ml_cu_params(
				wmi, evt_buf, cu_params);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_send_mlo_link_removal_cmd(
		wmi_unified_t wmi,
		const struct mlo_link_removal_cmd_params *param)
{
	if (wmi->ops->send_mlo_link_removal_cmd)
		return wmi->ops->send_mlo_link_removal_cmd(wmi, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_send_mlo_vdev_pause(wmi_unified_t wmi,
				   struct mlo_vdev_pause *info)
{
	if (wmi->ops->send_mlo_vdev_pause)
		return wmi->ops->send_mlo_vdev_pause(wmi, info);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_mlo_link_removal_evt_fixed_param(
		struct wmi_unified *wmi,
		void *buf,
		struct mlo_link_removal_evt_params *params)
{
	if (wmi->ops->extract_mlo_link_removal_evt_fixed_param)
		return wmi->ops->extract_mlo_link_removal_evt_fixed_param(
							wmi, buf, params);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_mlo_link_removal_tbtt_update(
		struct wmi_unified *wmi,
		void *buf,
		struct mlo_link_removal_tbtt_info *tbtt_info)
{
	if (wmi->ops->extract_mlo_link_removal_tbtt_update)
		return wmi->ops->extract_mlo_link_removal_tbtt_update(
							wmi, buf, tbtt_info);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_mgmt_rx_mlo_link_removal_info(
		struct wmi_unified *wmi,
		void *buf,
		struct mgmt_rx_mlo_link_removal_info *link_removal_info,
		int num_link_removal_info)
{
	if (wmi->ops->extract_mgmt_rx_mlo_link_removal_info)
		return wmi->ops->extract_mgmt_rx_mlo_link_removal_info(
							wmi, buf,
							link_removal_info,
							num_link_removal_info);

	return QDF_STATUS_E_FAILURE;
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
QDF_STATUS wmi_unified_peer_ptqm_migrate_send(
					wmi_unified_t wmi_hdl,
					struct peer_ptqm_migrate_params *param)
{
	if (wmi_hdl->ops->send_peer_ptqm_migrate_cmd)
		return wmi_hdl->ops->send_peer_ptqm_migrate_cmd(wmi_hdl, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_peer_ptqm_migrate_event(
		wmi_unified_t wmi, void *evt_buf,
		struct peer_ptqm_migrate_event_params *resp)
{
	if (wmi->ops->extract_peer_ptqm_migrate_event) {
		return wmi->ops->extract_peer_ptqm_migrate_event(wmi,
								 evt_buf,
								 resp);
	}
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_peer_ptqm_entry_param(
		wmi_unified_t wmi_handle, void *evt_buf,
		uint32_t index,
		struct peer_entry_ptqm_migrate_event_params *entry)
{
	if (wmi_handle->ops->extract_peer_entry_ptqm_migrate_event)
		return wmi_handle->ops->extract_peer_entry_ptqm_migrate_event(
			wmi_handle, evt_buf,
			index, entry);

	return QDF_STATUS_E_FAILURE;
}
#endif /* QCA_SUPPORT_PRIMARY_LINK_MIGRATE */
