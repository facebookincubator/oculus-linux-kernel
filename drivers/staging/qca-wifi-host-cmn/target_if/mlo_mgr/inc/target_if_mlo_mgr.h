/*
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
 * DOC: target_if_mlo_mgr.h
 *
 * This header file provide declarations required for Rx and Tx events from
 * firmware
 */

#ifndef __TARGET_IF_MLO_MGR_H__
#define __TARGET_IF_MLO_MGR_H__

#include <target_if.h>
#include <wlan_lmac_if_def.h>

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * target_if_mlo_get_rx_ops() - get rx ops
 * @psoc: pointer to soc object
 *
 * API to retrieve the MLO rx ops from the psoc context
 *
 * Return: pointer to rx ops
 */
static inline struct wlan_lmac_if_mlo_rx_ops *
target_if_mlo_get_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		target_if_err("rx_ops is NULL");
		return NULL;
	}

	return &rx_ops->mlo_rx_ops;
}

/**
 * target_if_mlo_get_tx_ops() - get tx ops
 * @psoc: pointer to soc object
 *
 * API to retrieve the MLO tx ops from the psoc context
 *
 * Return: pointer to tx ops
 */
static inline struct wlan_lmac_if_mlo_tx_ops *
target_if_mlo_get_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		target_if_err("tx_ops is NULL");
		return NULL;
	}

	return &tx_ops->mlo_ops;
}

/**
 * target_if_mlo_register_tx_ops() - lmac handler to register mlo tx ops
 *  callback functions
 * @tx_ops: wlan_lmac_if_tx_ops object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_mlo_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);

/**
 * target_if_mlo_send_link_removal_cmd() - Send WMI command for MLO link removal
 * @psoc: psoc pointer
 * @param: MLO link removal command parameters
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS target_if_mlo_send_link_removal_cmd(
		struct wlan_objmgr_psoc *psoc,
		const struct mlo_link_removal_cmd_params *param);

/**
 * target_if_extract_mlo_link_removal_info_mgmt_rx() - Extract MLO link removal
 * information from MGMT Rx event
 * @wmi_handle: WMI handle
 * @evt_buf: Event buffer
 * @rx_event: MGMT Rx event parameters
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS
target_if_extract_mlo_link_removal_info_mgmt_rx(
		wmi_unified_t wmi_handle,
		void *evt_buf,
		struct mgmt_rx_event_params *rx_event);

/**
 * target_if_mlo_register_mlo_link_state_info_event -
 *  Register mlo link state event
 * @wmi_handle: WMI handle
 */
void target_if_mlo_register_mlo_link_state_info_event(
		struct wmi_unified *wmi_handle);

/**
 * target_if_mlo_unregister_mlo_link_state_info_event -
 *  Unregister mlo link state event
 * @wmi_handle: WMI handle
 */
void target_if_mlo_unregister_mlo_link_state_info_event(
		struct wmi_unified *wmi_handle);

/**
 * target_if_mlo_register_vdev_tid_to_link_map_event() - Register T2LM event
 * handler.
 * @wmi_handle: WMI handle
 *
 * Return: None
 */
void target_if_mlo_register_vdev_tid_to_link_map_event(
		struct wmi_unified *wmi_handle);

/**
 * target_if_mlo_unregister_vdev_tid_to_link_map_event() - Unregister T2LM event
 * handler.
 * @wmi_handle: WMI handle
 *
 * Return: None
 */
void target_if_mlo_unregister_vdev_tid_to_link_map_event(
		struct wmi_unified *wmi_handle);
#else
static inline QDF_STATUS
target_if_extract_mlo_link_removal_info_mgmt_rx(
		wmi_unified_t wmi_handle,
		void *evt_buf,
		struct mgmt_rx_event_params *rx_event)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void target_if_mlo_register_vdev_tid_to_link_map_event(
		struct wmi_unified *wmi_handle)
{
}

static inline
void target_if_mlo_unregister_vdev_tid_to_link_map_event(
		struct wmi_unified *wmi_handle)
{
}
#endif
#endif /* __TARGET_IF_MLO_MGR_H__ */
