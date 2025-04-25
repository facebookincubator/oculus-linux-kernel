/*
 * Copyright (c) 2016-2017, 2019 The Linux Foundation. All rights reserved.
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
 * DOC: cdp_txrx_wds.h
 * Define the host data path WDS API functions
 * called by the host control SW and the OS interface module
 */
#ifndef _CDP_TXRX_WDS_H_
#define _CDP_TXRX_WDS_H_
#include "cdp_txrx_handle.h"

/**
 * cdp_set_wds_rx_policy() - set the wds rx filter policy of the device
 * @soc: psoc object
 * @vdev_id: id of the data virtual device object
 * @val: the wds rx policy bitmask
 *
 * This flag sets the wds rx policy on the vdev. Rx frames not compliant
 * with the policy will be dropped.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_set_wds_rx_policy(ol_txrx_soc_handle soc,
	uint8_t vdev_id,
	u_int32_t val)
{
	if (!soc || !soc->ops || !soc->ops->wds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->wds_ops->txrx_set_wds_rx_policy)
		soc->ops->wds_ops->txrx_set_wds_rx_policy(soc, vdev_id, val);
	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_set_wds_tx_policy_update() - set the wds tx filter policy of the device
 * @soc: psoc object
 * @vdev_id: id of the data virtual device object
 * @peer_mac: peer mac address
 * @wds_tx_ucast: the wds unicast tx policy bitmask
 * @wds_tx_mcast: the wds multicast tx policy bitmask
 *
 * This flag sets the wds xx policy on the vdev. Tx frames not compliant
 * with the policy will be dropped.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_set_wds_tx_policy_update(ol_txrx_soc_handle soc,
	uint8_t vdev_id, uint8_t *peer_mac,
	int wds_tx_ucast, int wds_tx_mcast)
{
	if (!soc || !soc->ops || !soc->ops->wds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			"%s invalid instance", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (soc->ops->wds_ops->txrx_wds_peer_tx_policy_update)
		soc->ops->wds_ops->txrx_wds_peer_tx_policy_update(
				soc, vdev_id, peer_mac, wds_tx_ucast,
				wds_tx_mcast);
	return QDF_STATUS_SUCCESS;
}

/**
 * cdp_vdev_set_wds() - Set/unset wds_enable flag in vdev
 * @soc: data path soc handle
 * @vdev_id: id of data path vap handle
 * @val: value to be set in wds_en flag
 *
 *  This flag enables WDS source port learning feature on a vdev
 *
 * Return: 1 on success
 */
static inline int
cdp_vdev_set_wds(ol_txrx_soc_handle soc, uint8_t vdev_id, uint32_t val)
{
	if (soc->ops->wds_ops->vdev_set_wds)
		return soc->ops->wds_ops->vdev_set_wds(soc, vdev_id, val);
	return 0;
}
#endif
