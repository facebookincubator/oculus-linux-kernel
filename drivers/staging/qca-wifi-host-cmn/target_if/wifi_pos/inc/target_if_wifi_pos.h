/*
 * Copyright (c) 2017, 2019-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: target_if_wifi_pos.h
 * This file declares the functions pertinent to wifi positioning component's
 * target if layer.
 */
#ifndef _WIFI_POS_TGT_IF_H_
#define _WIFI_POS_TGT_IF_H_

#include "qdf_types.h"
#include "qdf_status.h"
#include "wlan_cmn.h"
#include "wmi_unified_api.h"
#include "wifi_pos_utils_i.h"

struct oem_data_req;
struct oem_data_rsp;
struct wlan_objmgr_psoc;
struct wlan_soc_southbound_cb;
struct wlan_lmac_if_tx_ops;
struct wlan_lmac_if_rx_ops;

#ifdef WLAN_FEATURE_CIF_CFR
/**
 * target_if_wifi_pos_get_indirect_data  - Get the indirect data from the
 * response
 * @priv_obj: Wifi Pos private object
 * @indirect: Indirect data
 * @rsp: Oem data response
 * @cookie: Cookie
 *
 * Return: QDF_STATUS
 */
QDF_STATUS target_if_wifi_pos_get_indirect_data(
			struct wifi_pos_psoc_priv_obj *priv_obj,
			struct wmi_host_oem_indirect_data *indirect,
			struct oem_data_rsp *rsp, uint32_t *cookie);
/**
 * target_if_wifi_pos_replenish_ring() - Replenish the CFR DMA ring
 * @priv: Pointer to Wifi Pos psoc private object
 * @ring_idx: Ring Index
 * @vaddr: Virtual address
 * @cookie: Cookie
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_wifi_pos_replenish_ring(struct wifi_pos_psoc_priv_obj *priv,
				  uint8_t ring_idx,
				  void *vaddr, uint32_t cookie);
#else
static inline
QDF_STATUS target_if_wifi_pos_get_indirect_data(
			struct wifi_pos_psoc_priv_obj *priv_obj,
			struct wmi_host_oem_indirect_data *indirect,
			struct oem_data_rsp *rsp, uint32_t *cookie)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
target_if_wifi_pos_replenish_ring(struct wifi_pos_psoc_priv_obj *priv,
				  uint8_t ring_idx,
				  void *vaddr, uint32_t cookie)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WIFI_POS_CONVERGED
/**
 * target_if_wifi_pos_get_vht_ch_width: function to get vht channel width
 * @psoc: pointer to psoc object
 * @ch_width: pointer to the variable in which output value needs to be filled
 *
 * Return: status of operation
 */
QDF_STATUS target_if_wifi_pos_get_vht_ch_width(struct wlan_objmgr_psoc *psoc,
					       enum phy_ch_width *ch_width);
#endif

#if defined(WLAN_FEATURE_CIF_CFR) && defined(WIFI_POS_CONVERGED)
/**
 * target_if_wifi_pos_init_cir_cfr_rings: set DMA ring cap in psoc private
 * object
 * @psoc: pointer to psoc object
 * @hal_soc: pointer to hal soc
 * @num_mac: number of mac
 * @buf: buffer containing DMA ring cap
 *
 * Return: status of operation
 */
QDF_STATUS target_if_wifi_pos_init_cir_cfr_rings(struct wlan_objmgr_psoc *psoc,
					     void *hal_soc, uint8_t num_mac,
					     void *buf);
/**
 * target_if_wifi_pos_deinit_dma_rings: frees up DMA rings
 * @psoc: pointer to psoc
 *
 * Return: status of operation
 */
QDF_STATUS target_if_wifi_pos_deinit_dma_rings(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS target_if_wifi_pos_init_cir_cfr_rings(
				struct wlan_objmgr_psoc *psoc, void *hal_soc,
				uint8_t num_mac, void *buf)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS target_if_wifi_pos_deinit_dma_rings(
				struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifndef CNSS_GENL
/**
 * target_if_wifi_pos_convert_pdev_id_host_to_target: function to get target
 * pdev_id from host pdev_id
 * @psoc: pointer to psoc object
 * @host_pdev_id: host pdev id
 * @target_pdev_id: target pdev id
 *
 * Return: QDF_STATUS_SUCCESS in case of success, error codes in
 * case of failure
 */
QDF_STATUS target_if_wifi_pos_convert_pdev_id_host_to_target(
		struct wlan_objmgr_psoc *psoc, uint32_t host_pdev_id,
		uint32_t *target_pdev_id);

/**
 * target_if_wifi_pos_convert_pdev_id_target_to_host: function to get host
 * pdev_id from target pdev_id
 * @psoc: pointer to psoc object
 * @target_pdev_id: target pdev id
 * @host_pdev_id: host pdev id
 *
 * Return: QDF_STATUS_SUCCESS in case of success, error codes in
 * case of failure
 */
QDF_STATUS target_if_wifi_pos_convert_pdev_id_target_to_host(
		struct wlan_objmgr_psoc *psoc, uint32_t target_pdev_id,
		uint32_t *host_pdev_id);

#else

static inline QDF_STATUS target_if_wifi_pos_convert_pdev_id_host_to_target(
		struct wlan_objmgr_psoc *psoc, uint32_t host_pdev_id,
		uint32_t *target_pdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS target_if_wifi_pos_convert_pdev_id_target_to_host(
		struct wlan_objmgr_psoc *psoc, uint32_t target_pdev_id,
		uint32_t *host_pdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* CNSS_GENL */
#endif /* _WIFI_POS_TGT_IF_H_ */
