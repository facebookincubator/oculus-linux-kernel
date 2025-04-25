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
/*
 * This file contains the API definitions for the Unified Wireless Module
 * Interface (WMI) specific to 11be.
 */

#ifndef _WMI_UNIFIED_11BE_API_H_
#define _WMI_UNIFIED_11BE_API_H_

#include <wmi_unified_api.h>
#include <wmi_unified_priv.h>

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wmi_extract_mlo_link_set_active_resp() - extract mlo link set active
 *  response event
 * @wmi: wmi handle
 * @evt_buf: pointer to event buffer
 * @evt: Pointer to hold mlo link set active response event
 *
 * This function gets called to extract mlo link set active response event
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_mlo_link_set_active_resp(wmi_unified_t wmi,
				     void *evt_buf,
				     struct mlo_link_set_active_resp *evt);

/**
 * wmi_send_mlo_link_set_active_cmd() - send mlo link set active command
 * @wmi_handle: WMI handle for this pdev
 * @param: Pointer to mlo link set active param
 *
 * Return: QDF_STATUS code
 */
QDF_STATUS
wmi_send_mlo_link_set_active_cmd(wmi_unified_t wmi_handle,
				 struct mlo_link_set_active_param *param);

/**
 * wmi_extract_mgmt_rx_ml_cu_params() - extract mlo cu params from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @cu_params: Pointer to mlo CU params
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_mgmt_rx_ml_cu_params(wmi_unified_t wmi_handle, void *evt_buf,
				 struct mlo_mgmt_ml_info *cu_params);

/**
 * wmi_send_mlo_link_removal_cmd() - Send WMI command for MLO link removal
 * @wmi: wmi handle
 * @param: MLO link removal command parameters
 *
 * Return: QDF_STATUS_SUCCESS of operation
 */
QDF_STATUS wmi_send_mlo_link_removal_cmd(
		wmi_unified_t wmi,
		const struct mlo_link_removal_cmd_params *param);

/**
 * wmi_send_mlo_vdev_pause() - Send WMI command for MLO vdev pause
 * @wmi: wmi handle
 * @info: MLO vdev pause information
 *
 * Return: QDF_STATUS_SUCCESS of operation
 */
QDF_STATUS wmi_send_mlo_vdev_pause(wmi_unified_t wmi,
				   struct mlo_vdev_pause *info);

/**
 * wmi_extract_mlo_link_removal_evt_fixed_param() - Extract fixed parameters TLV
 * from the MLO link removal WMI  event
 * @wmi_handle: wmi handle
 * @buf: pointer to event buffer
 * @params: MLO link removal event parameters
 *
 * Return: QDF_STATUS_SUCCESS of operation
 */
QDF_STATUS wmi_extract_mlo_link_removal_evt_fixed_param(
		struct wmi_unified *wmi_handle,
		void *buf,
		struct mlo_link_removal_evt_params *params);

/**
 * wmi_extract_mlo_link_removal_tbtt_update() - Extract TBTT update TLV
 * from the MLO link removal WMI  event
 * @wmi_handle: wmi handle
 * @buf: pointer to event buffer
 * @tbtt_info: TBTT information
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS wmi_extract_mlo_link_removal_tbtt_update(
		struct wmi_unified *wmi_handle,
		void *buf,
		struct mlo_link_removal_tbtt_info *tbtt_info);

/**
 * wmi_extract_mgmt_rx_mlo_link_removal_info() - Extract MLO link removal info
 * from MGMT Rx event
 * @wmi: wmi handle
 * @buf: event buffer
 * @link_removal_info: link removal information array to be populated
 * @num_link_removal_info: Number of elements in @link_removal_info
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS wmi_extract_mgmt_rx_mlo_link_removal_info(
		struct wmi_unified *wmi,
		void *buf,
		struct mgmt_rx_mlo_link_removal_info *link_removal_info,
		int num_link_removal_info);
#endif /*WLAN_FEATURE_11BE_MLO*/

#ifdef WLAN_FEATURE_11BE
/**
 * wmi_send_mlo_peer_tid_to_link_map_cmd() - send TID-to-link mapping command
 * @wmi: WMI handle for this pdev
 * @params: Pointer to TID-to-link mapping params
 * @t2lm_info: T2LM info presence flag
 */
QDF_STATUS wmi_send_mlo_peer_tid_to_link_map_cmd(
		wmi_unified_t wmi,
		struct wmi_host_tid_to_link_map_params *params,
		bool t2lm_info);

/**
 * wmi_send_mlo_vdev_tid_to_link_map_cmd() - send TID-to-link mapping command
 *                                           per vdev
 * @wmi: WMI handle for this pdev
 * @params: Pointer to TID-to-link mapping params
 */
QDF_STATUS wmi_send_mlo_vdev_tid_to_link_map_cmd(
		wmi_unified_t wmi,
		struct wmi_host_tid_to_link_map_ap_params *params);
/**
 * wmi_send_mlo_link_state_request_cmd - send mlo link status command
 * @wmi: wmi handle
 * @params: Pointer to link state params
 */
QDF_STATUS wmi_send_mlo_link_state_request_cmd(
		wmi_unified_t wmi,
		struct wmi_host_link_state_params *params);

/**
 * wmi_send_link_set_bss_params_cmd - send link set bss cmd
 * @wmi: wmi handler
 * @params: pointer to link bss param
 */
QDF_STATUS wmi_send_link_set_bss_params_cmd(
		wmi_unified_t wmi,
		struct wmi_host_link_bss_params *params);

/**
 * wmi_extract_mlo_vdev_tid_to_link_map_event() - extract mlo t2lm info for vdev
 * @wmi: wmi handle
 * @evt_buf: pointer to event buffer
 * @resp: Pointer to host structure to get the t2lm info
 *
 * This function gets called to extract mlo t2lm info for particular pdev
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_mlo_vdev_tid_to_link_map_event(
		wmi_unified_t wmi, void *evt_buf,
		struct mlo_vdev_host_tid_to_link_map_resp *resp);

/**
 * wmi_extract_mlo_vdev_bcast_tid_to_link_map_event() - extract bcast mlo t2lm
 *                                                      info for vdev
 * @wmi: wmi handle
 * @evt_buf: pointer to event buffer
 * @bcast: Pointer to host structure to get the t2lm bcast info
 *
 * This function gets called to extract bcast mlo t2lm info for particular pdev
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_mlo_vdev_bcast_tid_to_link_map_event(
				     wmi_unified_t wmi,
				     void *evt_buf,
				     struct mlo_bcast_t2lm_info *bcast);

/**
 * wmi_extract_mlo_link_state_info_event - extract mlo link status info
 * @wmi: wmi handle
 * @evt_buf: pointer to event buffer
 * @params: pointer to host struct to get mlo link state
 */
QDF_STATUS wmi_extract_mlo_link_state_info_event(
			wmi_unified_t wmi,
			void *evt_buf,
			struct ml_link_state_info_event *params);

#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
/**
 * wmi_send_mlo_link_switch_req_cnf_cmd() - Send WMI command to FW on
 * status of Link switch request received.
 * @wmi: wmi handle
 * @params: Params to send to FW.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wmi_send_mlo_link_switch_req_cnf_cmd(wmi_unified_t wmi,
				     struct wlan_mlo_link_switch_cnf *params);

/**
 * wmi_extract_mlo_link_switch_request_evt() - Extract fixed params TLV
 * from the MLO link switch request WMI event.
 * @wmi: wmi handle
 * @buf: pointer to event buffer
 * @req: MLO link switch request event params.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wmi_extract_mlo_link_switch_request_evt(struct wmi_unified *wmi, void *buf,
					struct wlan_mlo_link_switch_req *req);
#else
static inline QDF_STATUS
wmi_send_mlo_link_switch_req_cnf_cmd(wmi_unified_t wmi,
				     struct wlan_mlo_link_switch_cnf *params)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wmi_extract_mlo_link_switch_request_evt(struct wmi_unified *wmi, void *buf,
					struct wlan_mlo_link_switch_req *req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wmi_extract_mlo_link_disable_request_evt() - Extract fixed parameters TLV
 * from the MLO link disable request WMI event
 * @wmi: wmi handle
 * @buf: pointer to event buffer
 * @params: MLO link disable request event parameters
 *
 * Return: QDF_STATUS_SUCCESS of operation
 */
QDF_STATUS wmi_extract_mlo_link_disable_request_evt(
		struct wmi_unified *wmi,
		void *buf,
		struct mlo_link_disable_request_evt_params *params);

/**
 * wmi_extract_mlo_link_state_switch_evt() - Extract the MLO link switch state
 * event parameters
 * @wmi: wmi handle
 * @buf: pointer to event buffer
 * @len: event data length
 * @info: Info on link switch state change event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wmi_extract_mlo_link_state_switch_evt(struct wmi_unified *wmi, void *buf,
				      uint8_t len,
				      struct mlo_link_switch_state_info *info);
#endif /* WLAN_FEATURE_11BE */

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
/**
 * wmi_unified_peer_ptqm_migrate_send() - send PEER ptqm migrate command to fw
 * @wmi_hdl: wmi handle
 * @param: pointer to hold peer ptqm migrate parameters
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_peer_ptqm_migrate_send(
					wmi_unified_t wmi_hdl,
					struct peer_ptqm_migrate_params *param);

/**
 * wmi_extract_peer_ptqm_migrate_event() - extract peer ptqm migrate event params
 * @wmi: wmi handle
 * @evt_buf: pointer to event buffer
 * @resp: Pointer to host structure to get the event params
 *
 * This function gets called to extract peer ptqm migrate event params
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_peer_ptqm_migrate_event(
		wmi_unified_t wmi, void *evt_buf,
		struct peer_ptqm_migrate_event_params *resp);

/**
 * wmi_extract_peer_ptqm_entry_param() - extract peer entry ptqm migrate param
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @index: Index into pdev stats
 * @entry: Pointer to peer entry params
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_peer_ptqm_entry_param(
		wmi_unified_t wmi_handle, void *evt_buf,
		uint32_t index,
		struct peer_entry_ptqm_migrate_event_params *entry);
#endif /* QCA_SUPPORT_PRIMARY_LINK_MIGRATE */
#endif /*_WMI_UNIFIED_11BE_API_H_*/
