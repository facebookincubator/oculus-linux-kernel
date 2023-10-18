/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *  DOC: wlan_mgmt_txrx_rx_reo_tgt_api.h
 *  This file contains mgmt rx re-ordering tgt layer related APIs
 */

#ifndef _WLAN_MGMT_TXRX_RX_REO_TGT_API_H
#define _WLAN_MGMT_TXRX_RX_REO_TGT_API_H
#include <wlan_objmgr_pdev_obj.h>
#include <qdf_types.h>
#include <wlan_mgmt_txrx_rx_reo_public_structs.h>
#include <wlan_mgmt_txrx_rx_reo_utils_api.h>
#include <wlan_mgmt_txrx_tgt_api.h>
#include <wlan_lmac_if_def.h>

#ifdef WLAN_MGMT_RX_REO_SUPPORT
/**
 * wlan_pdev_get_mgmt_rx_reo_txops() - Get management rx-reorder txops from pdev
 * @pdev: Pointer to pdev object
 *
 * Return: Pointer to management rx-reorder txops in case of success, else NULL
 */
static inline struct wlan_lmac_if_mgmt_rx_reo_tx_ops *
wlan_pdev_get_mgmt_rx_reo_txops(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_lmac_if_mgmt_txrx_tx_ops *mgmt_txrx_tx_ops;

	mgmt_txrx_tx_ops = wlan_pdev_get_mgmt_txrx_txops(pdev);
	if (!mgmt_txrx_tx_ops) {
		mgmt_txrx_err("txops is null for mgmt txrx module");
		return NULL;
	}

	return &mgmt_txrx_tx_ops->mgmt_rx_reo_tx_ops;
}

/**
 * wlan_psoc_get_mgmt_rx_reo_txops() - Get management rx-reorder txops from psoc
 * @psoc: Pointer to psoc object
 *
 * Return: Pointer to management rx-reorder txops in case of success, else NULL
 */
static inline struct wlan_lmac_if_mgmt_rx_reo_tx_ops *
wlan_psoc_get_mgmt_rx_reo_txops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_mgmt_txrx_tx_ops *mgmt_txrx_tx_ops;

	mgmt_txrx_tx_ops = wlan_psoc_get_mgmt_txrx_txops(psoc);
	if (!mgmt_txrx_tx_ops) {
		mgmt_txrx_err("txops is null for mgmt txrx module");
		return NULL;
	}

	return &mgmt_txrx_tx_ops->mgmt_rx_reo_tx_ops;
}

/**
 * tgt_mgmt_rx_reo_get_num_active_hw_links() - Get number of active MLO HW
 * links
 * @psoc: Pointer to psoc object
 * @num_active_hw_links: pointer to number of active MLO HW links
 *
 * Get number of active MLO HW links from the MLO global shared memory arena.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_mgmt_rx_reo_get_num_active_hw_links(struct wlan_objmgr_psoc *psoc,
					int8_t *num_active_hw_links);

/**
 * tgt_mgmt_rx_reo_get_valid_hw_link_bitmap() - Get valid MLO HW link bitmap
 * @psoc: Pointer to psoc object
 * @valid_hw_link_bitmap: Pointer to valid MLO HW link bitmap
 *
 * Get valid MLO HW link bitmap from the MLO global shared memory arena.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_mgmt_rx_reo_get_valid_hw_link_bitmap(struct wlan_objmgr_psoc *psoc,
					 uint16_t *valid_hw_link_bitmap);

/**
 * tgt_mgmt_rx_reo_read_snapshot() - Read management rx-reorder snapshot
 * @pdev: Pointer to pdev object
 * @snapshot_info: Snapshot info
 * @id: Snapshot ID
 * @value: Pointer to the snapshot value where the snapshot
 * should be written
 * @raw_snapshot: Raw snapshot data
 *
 * Read management rx-reorder snapshots from target.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_mgmt_rx_reo_read_snapshot(
			struct wlan_objmgr_pdev *pdev,
			struct mgmt_rx_reo_snapshot_info *snapshot_info,
			enum mgmt_rx_reo_shared_snapshot_id id,
			struct mgmt_rx_reo_snapshot_params *value,
			struct mgmt_rx_reo_shared_snapshot (*raw_snapshot)
			[MGMT_RX_REO_SNAPSHOT_B2B_READ_SWAR_RETRY_LIMIT]);

/**
 * tgt_mgmt_rx_reo_fw_consumed_event_handler() - MGMT Rx REO FW consumed
 * event handler
 * @pdev: pdev for which this event is intended
 * @params: Pointer to MGMT Rx REO parameters
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS
tgt_mgmt_rx_reo_fw_consumed_event_handler(struct wlan_objmgr_pdev *pdev,
					  struct mgmt_rx_reo_params *params);

/**
 * tgt_mgmt_rx_reo_filter_config() - Configure MGMT Rx REO filter
 * @pdev: Pointer to pdev object
 * @filter: Pointer to MGMT Rx REO filter
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS tgt_mgmt_rx_reo_filter_config(struct wlan_objmgr_pdev *pdev,
					 struct mgmt_rx_reo_filter *filter);

/**
 * tgt_mgmt_rx_reo_get_snapshot_info() - Get information regarding management
 * rx-reorder snapshot
 * @pdev: Pointer to pdev object
 * @id: Snapshot ID
 * @snapshot_info: Pointer to snapshot info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_mgmt_rx_reo_get_snapshot_info
			(struct wlan_objmgr_pdev *pdev,
			 enum mgmt_rx_reo_shared_snapshot_id id,
			 struct mgmt_rx_reo_snapshot_info *snapshot_info);

/**
 * tgt_mgmt_rx_reo_frame_handler() - REO handler for management Rx frames.
 * @pdev: pdev for which this management frame is intended
 * @buf: buffer
 * @mgmt_rx_params: rx event params
 *
 * Return: QDF_STATUS of operation.
 */
QDF_STATUS tgt_mgmt_rx_reo_frame_handler(
			struct wlan_objmgr_pdev *pdev,
			qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params);

/**
 * tgt_mgmt_rx_reo_host_drop_handler() - MGMT Rx REO handler for the
 * management Rx frames that gets dropped in the Host before entering
 * MGMT Rx REO algorithm
 * @pdev: pdev for which this frame was intended
 * @params: MGMT Rx event parameters
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS
tgt_mgmt_rx_reo_host_drop_handler(struct wlan_objmgr_pdev *pdev,
				  struct mgmt_rx_reo_params *params);
#else
/**
 * tgt_mgmt_rx_reo_frame_handler() - REO handler for management Rx frames.
 * @pdev: pdev for which this management frame is intended
 * @buf: buffer
 * @mgmt_rx_params: rx event params
 *
 * Return: QDF_STATUS of operation.
 */
static inline QDF_STATUS tgt_mgmt_rx_reo_frame_handler(
			struct wlan_objmgr_pdev *pdev,
			qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params)
{
	/**
	 * If MGMT Rx REO feature is not compiled,
	 * process the frame right away.
	 */
	return tgt_mgmt_txrx_process_rx_frame(pdev, buf, mgmt_rx_params);
}
#endif /* WLAN_MGMT_RX_REO_SUPPORT */
#endif /* _WLAN_MGMT_TXRX_RX_REO_TGT_API_H */
