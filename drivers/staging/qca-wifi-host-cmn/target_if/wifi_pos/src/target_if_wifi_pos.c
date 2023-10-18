/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 * DOC: target_if_wifi_pos.c
 * This file defines the functions pertinent to wifi positioning component's
 * target if layer.
 */
#include "wifi_pos_utils_pub.h"

#include "wmi_unified_api.h"
#include "wlan_lmac_if_def.h"
#include "target_if_wifi_pos.h"
#include "../../../../umac/wifi_pos/src/wifi_pos_main_i.h"
#include "wifi_pos_utils_i.h"
#include "target_if.h"
#ifdef WLAN_FEATURE_CIF_CFR
#include "hal_api.h"

#define RING_BASE_ALIGN 8

static void *target_if_wifi_pos_vaddr_lookup(
				struct wifi_pos_psoc_priv_obj *priv,
				void *paddr, uint8_t ring_num, uint32_t cookie)
{
	if (priv->dma_buf_pool[ring_num][cookie].paddr == paddr) {
		return priv->dma_buf_pool[ring_num][cookie].vaddr +
				priv->dma_buf_pool[ring_num][cookie].offset;
	} else {
		target_if_err("incorrect paddr found on cookie slot");
		return NULL;
	}
}

QDF_STATUS
target_if_wifi_pos_replenish_ring(struct wifi_pos_psoc_priv_obj *priv,
				  uint8_t ring_idx,
				  void *aligned_vaddr, uint32_t cookie)
{
	uint64_t *ring_entry;
	uint32_t dw_lo, dw_hi = 0, map_status;
	void *hal_soc = priv->hal_soc;
	void *srng = priv->dma_cfg[ring_idx].srng;
	void *paddr;

	if (!aligned_vaddr) {
		target_if_debug("NULL aligned_vaddr provided");
		return QDF_STATUS_SUCCESS;
	}

	map_status = qdf_mem_map_nbytes_single(NULL, aligned_vaddr,
			QDF_DMA_FROM_DEVICE,
			priv->dma_cap[ring_idx].min_buf_size,
			(qdf_dma_addr_t *)&paddr);
	if (map_status) {
		target_if_err("mem map failed status: %d", map_status);
		return QDF_STATUS_E_FAILURE;
	}
	QDF_ASSERT(!((uint64_t)paddr % priv->dma_cap[ring_idx].min_buf_align));
	priv->dma_buf_pool[ring_idx][cookie].paddr = paddr;

	hal_srng_access_start(hal_soc, srng);
	ring_entry = hal_srng_src_get_next(hal_soc, srng);
	dw_lo = (uint64_t)paddr & 0xFFFFFFFF;
	WMI_OEM_DMA_DATA_ADDR_HI_SET(dw_hi, (uint64_t)paddr >> 32);
	WMI_OEM_DMA_DATA_ADDR_HI_HOST_DATA_SET(dw_hi, cookie);
	*ring_entry = (uint64_t)dw_hi << 32 | dw_lo;
	hal_srng_access_end(hal_soc, srng);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS target_if_wifi_pos_get_indirect_data(
		struct wifi_pos_psoc_priv_obj *priv_obj,
		struct wmi_host_oem_indirect_data *indirect,
		struct oem_data_rsp *rsp, uint32_t *cookie)
{
	void *paddr = NULL;
	uint32_t addr_hi;
	uint8_t ring_idx = 0, num_rings;
	uint32_t allocated_len;

	if (!indirect) {
		target_if_debug("no indirect data. regular event received");
		return QDF_STATUS_SUCCESS;
	}

	ring_idx = indirect->pdev_id - 1;
	num_rings = priv_obj->num_rings;
	if (ring_idx >= num_rings) {
		target_if_err("incorrect pdev_id: %d", indirect->pdev_id);
		return QDF_STATUS_E_INVAL;
	}

	allocated_len = priv_obj->dma_cap[ring_idx].min_buf_size +
				(priv_obj->dma_cap[ring_idx].min_buf_align - 1);
	if (indirect->len > allocated_len ||
	    indirect->len > OEM_DATA_DMA_BUFF_SIZE) {
		target_if_err("Invalid indirect len: %d, allocated_len:%d",
			      indirect->len, allocated_len);
		return QDF_STATUS_E_INVAL;
	}

	addr_hi = (uint64_t)WMI_OEM_DMA_DATA_ADDR_HI_GET(
						indirect->addr_hi);
	paddr = (void *)((uint64_t)addr_hi << 32 | indirect->addr_lo);
	*cookie = WMI_OEM_DMA_DATA_ADDR_HI_HOST_DATA_GET(
						indirect->addr_hi);
	rsp->vaddr = target_if_wifi_pos_vaddr_lookup(priv_obj,
					paddr, ring_idx, *cookie);
	rsp->dma_len = indirect->len;
	qdf_mem_unmap_nbytes_single(NULL, (qdf_dma_addr_t)paddr,
			QDF_DMA_FROM_DEVICE,
			priv_obj->dma_cap[ring_idx].min_buf_size);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS target_if_wifi_pos_get_vht_ch_width(struct wlan_objmgr_psoc *psoc,
					       enum phy_ch_width *ch_width)
{
	struct target_psoc_info *tgt_hdl;
	int vht_cap_info;

	*ch_width = CH_WIDTH_INVALID;

	if (!psoc)
		return QDF_STATUS_E_INVAL;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_hdl)
		return QDF_STATUS_E_INVAL;

	*ch_width = CH_WIDTH_80MHZ;

	vht_cap_info = target_if_get_vht_cap_info(tgt_hdl);

	if (vht_cap_info & WLAN_VHTCAP_SUP_CHAN_WIDTH_80_160)
		*ch_width = CH_WIDTH_80P80MHZ;
	else if (vht_cap_info & WLAN_VHTCAP_SUP_CHAN_WIDTH_160)
		*ch_width = CH_WIDTH_160MHZ;

	return QDF_STATUS_SUCCESS;
}

#ifndef CNSS_GENL
QDF_STATUS target_if_wifi_pos_convert_pdev_id_host_to_target(
		struct wlan_objmgr_psoc *psoc, uint32_t host_pdev_id,
		uint32_t *target_pdev_id)
{
	wmi_unified_t wmi_hdl = GET_WMI_HDL_FROM_PSOC(psoc);

	if (!wmi_hdl) {
		target_if_err("null wmi_hdl");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return wmi_convert_pdev_id_host_to_target(wmi_hdl, host_pdev_id,
						  target_pdev_id);
}

QDF_STATUS target_if_wifi_pos_convert_pdev_id_target_to_host(
		struct wlan_objmgr_psoc *psoc, uint32_t target_pdev_id,
		uint32_t *host_pdev_id)
{
	wmi_unified_t wmi_hdl = GET_WMI_HDL_FROM_PSOC(psoc);

	if (!wmi_hdl) {
		target_if_err("null wmi_hdl");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return wmi_convert_pdev_id_target_to_host(wmi_hdl, target_pdev_id,
						  host_pdev_id);
}
#endif /* CNSS_GENL */

#ifdef WLAN_FEATURE_CIF_CFR
static QDF_STATUS target_if_wifi_pos_fill_ring(uint8_t ring_idx,
					struct hal_srng *srng,
					struct wifi_pos_psoc_priv_obj *priv)
{
	uint32_t i;
	void *buf, *buf_aligned;

	for (i = 0; i < priv->dma_cfg[ring_idx].num_ptr; i++) {
		buf = qdf_mem_malloc(priv->dma_cap[ring_idx].min_buf_size +
				priv->dma_cap[ring_idx].min_buf_align - 1);
		if (!buf)
			return QDF_STATUS_E_NOMEM;

		priv->dma_buf_pool[ring_idx][i].vaddr = buf;
		buf_aligned = (void *)qdf_roundup((uint64_t)buf,
				priv->dma_cap[ring_idx].min_buf_align);
		priv->dma_buf_pool[ring_idx][i].offset = buf_aligned - buf;
		priv->dma_buf_pool[ring_idx][i].cookie = i;
		target_if_wifi_pos_replenish_ring(priv, ring_idx,
						  buf_aligned, i);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_wifi_pos_empty_ring(uint8_t ring_idx,
					struct wifi_pos_psoc_priv_obj *priv)
{
	uint32_t i;

	for (i = 0; i < priv->dma_cfg[ring_idx].num_ptr; i++) {
		qdf_mem_unmap_nbytes_single(NULL,
			(qdf_dma_addr_t)priv->dma_buf_pool[ring_idx][i].vaddr,
			QDF_DMA_FROM_DEVICE,
			priv->dma_cap[ring_idx].min_buf_size);
		qdf_mem_free(priv->dma_buf_pool[ring_idx][i].vaddr);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_wifi_pos_init_ring(uint8_t ring_idx,
					struct wifi_pos_psoc_priv_obj *priv)
{
	void *srng;
	uint32_t num_entries;
	qdf_dma_addr_t paddr;
	uint32_t ring_alloc_size;
	void *hal_soc = priv->hal_soc;
	struct hal_srng_params ring_params = {0};
	uint32_t max_entries = hal_srng_max_entries(hal_soc, WIFI_POS_SRC);
	uint32_t entry_size = hal_srng_get_entrysize(hal_soc, WIFI_POS_SRC);

	num_entries = priv->dma_cap[ring_idx].min_num_ptr > max_entries ?
			max_entries : priv->dma_cap[ring_idx].min_num_ptr;
	priv->dma_cfg[ring_idx].num_ptr = num_entries;
	priv->dma_buf_pool[ring_idx] = qdf_mem_malloc(num_entries *
					sizeof(struct wifi_pos_dma_buf_info));
	if (!priv->dma_buf_pool[ring_idx])
		return QDF_STATUS_E_NOMEM;

	ring_alloc_size = (num_entries * entry_size) + RING_BASE_ALIGN - 1;
	priv->dma_cfg[ring_idx].ring_alloc_size = ring_alloc_size;
	priv->dma_cfg[ring_idx].base_vaddr_unaligned =
		qdf_mem_alloc_consistent(NULL, NULL, ring_alloc_size, &paddr);
	priv->dma_cfg[ring_idx].base_paddr_unaligned = (void *)paddr;
	if (!priv->dma_cfg[ring_idx].base_vaddr_unaligned) {
		target_if_err("malloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	priv->dma_cfg[ring_idx].base_vaddr_aligned = (void *)qdf_roundup(
		(uint64_t)priv->dma_cfg[ring_idx].base_vaddr_unaligned,
		RING_BASE_ALIGN);
	ring_params.ring_base_vaddr =
		priv->dma_cfg[ring_idx].base_vaddr_aligned;
	priv->dma_cfg[ring_idx].base_paddr_aligned = (void *)qdf_roundup(
		(uint64_t)priv->dma_cfg[ring_idx].base_paddr_unaligned,
		RING_BASE_ALIGN);
	ring_params.ring_base_paddr =
		(qdf_dma_addr_t)priv->dma_cfg[ring_idx].base_paddr_aligned;
	ring_params.num_entries = num_entries;
	srng = hal_srng_setup(hal_soc, WIFI_POS_SRC, 0,
			      priv->dma_cap[ring_idx].pdev_id, &ring_params, 0);
	if (!srng) {
		target_if_err("srng setup failed");
		return QDF_STATUS_E_FAILURE;
	}
	priv->dma_cfg[ring_idx].srng = srng;
	priv->dma_cfg[ring_idx].tail_idx_addr =
			(void *)hal_srng_get_tp_addr(hal_soc, srng);
	priv->dma_cfg[ring_idx].head_idx_addr =
			(void *)hal_srng_get_tp_addr(hal_soc, srng);

	return target_if_wifi_pos_fill_ring(ring_idx, srng, priv);
}

static QDF_STATUS target_if_wifi_pos_deinit_ring(uint8_t ring_idx,
					struct wifi_pos_psoc_priv_obj *priv)
{
	target_if_wifi_pos_empty_ring(ring_idx, priv);
	priv->dma_buf_pool[ring_idx] = NULL;
	hal_srng_cleanup(priv->hal_soc, priv->dma_cfg[ring_idx].srng);
	qdf_mem_free_consistent(NULL, NULL,
		priv->dma_cfg[ring_idx].ring_alloc_size,
		priv->dma_cfg[ring_idx].base_vaddr_unaligned,
		(qdf_dma_addr_t)priv->dma_cfg[ring_idx].base_paddr_unaligned,
		0);
	qdf_mem_free(priv->dma_buf_pool[ring_idx]);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_wifi_pos_init_srngs(
					struct wifi_pos_psoc_priv_obj *priv)
{
	uint8_t i;
	QDF_STATUS status;

	/* allocate memory for num_rings pointers */
	priv->dma_cfg = qdf_mem_malloc(priv->num_rings *
				sizeof(struct wifi_pos_dma_rings_cap));
	if (!priv->dma_cfg)
		return QDF_STATUS_E_NOMEM;

	priv->dma_buf_pool = qdf_mem_malloc(priv->num_rings *
				sizeof(struct wifi_pos_dma_buf_info *));
	if (!priv->dma_buf_pool)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < priv->num_rings; i++) {
		status = target_if_wifi_pos_init_ring(i, priv);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("init for ring[%d] failed", i);
			return status;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_wifi_pos_deinit_srngs(
					struct wifi_pos_psoc_priv_obj *priv)
{
	uint8_t i;

	for (i = 0; i < priv->num_rings; i++)
		target_if_wifi_pos_deinit_ring(i, priv);

	qdf_mem_free(priv->dma_buf_pool);
	priv->dma_buf_pool = NULL;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_wifi_pos_cfg_fw(struct wlan_objmgr_psoc *psoc,
					struct wifi_pos_psoc_priv_obj *priv)
{
	uint8_t i;
	QDF_STATUS status;
	wmi_unified_t wmi_hdl = GET_WMI_HDL_FROM_PSOC(psoc);
	wmi_oem_dma_ring_cfg_req_fixed_param cfg = {0};

	if (!wmi_hdl) {
		target_if_err("WMA closed, can't send oem data req cmd");
		return QDF_STATUS_E_INVAL;
	}

	target_if_debug("Sending oem dma ring cfg to target");

	for (i = 0; i < priv->num_rings; i++) {
		cfg.pdev_id = priv->dma_cfg[i].pdev_id;
		cfg.base_addr_lo = (uint64_t)priv->dma_cfg[i].base_paddr_aligned
						& 0xFFFFFFFF;
		cfg.base_addr_hi = (uint64_t)priv->dma_cfg[i].base_paddr_aligned
						& 0xFFFFFFFF00000000;
		cfg.head_idx_addr_lo = (uint64_t)priv->dma_cfg[i].head_idx_addr
						& 0xFFFFFFFF;
		cfg.head_idx_addr_hi = (uint64_t)priv->dma_cfg[i].head_idx_addr
						& 0xFFFFFFFF00000000;
		cfg.tail_idx_addr_lo = (uint64_t)priv->dma_cfg[i].tail_idx_addr
						& 0xFFFFFFFF;
		cfg.tail_idx_addr_hi = (uint64_t)priv->dma_cfg[i].tail_idx_addr
						& 0xFFFFFFFF00000000;
		cfg.num_ptr = priv->dma_cfg[i].num_ptr;
		status = wmi_unified_oem_dma_ring_cfg(wmi_hdl, &cfg);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			target_if_err("wmi cmd send failed");
			return status;
		}
	}

	return status;
}

QDF_STATUS target_if_wifi_pos_deinit_dma_rings(struct wlan_objmgr_psoc *psoc)
{
	struct wifi_pos_psoc_priv_obj *priv = wifi_pos_get_psoc_priv_obj(psoc);

	target_if_wifi_pos_deinit_srngs(priv);
	qdf_mem_free(priv->dma_cap);
	priv->dma_cap = NULL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS target_if_wifi_pos_init_cir_cfr_rings(struct wlan_objmgr_psoc *psoc,
					     void *hal_soc, uint8_t num_mac,
					     void *buf)
{
	uint8_t i;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	WMI_OEM_DMA_RING_CAPABILITIES *dma_cap = buf;
	struct wifi_pos_psoc_priv_obj *priv = wifi_pos_get_psoc_priv_obj(psoc);

	if (!priv) {
		target_if_err("unable to get wifi_pos psoc obj");
		return QDF_STATUS_E_NULL_VALUE;
	}

	priv->hal_soc = hal_soc;
	priv->num_rings = num_mac;
	priv->dma_cap = qdf_mem_malloc(priv->num_rings *
					sizeof(struct wifi_pos_dma_rings_cap));
	if (!priv->dma_cap)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < num_mac; i++) {
		priv->dma_cap[i].pdev_id = dma_cap[i].pdev_id;
		priv->dma_cap[i].min_num_ptr = dma_cap[i].min_num_ptr;
		priv->dma_cap[i].min_buf_size = dma_cap[i].min_buf_size;
		priv->dma_cap[i].min_buf_align = dma_cap[i].min_buf_align;
	}

	/* initialize DMA rings now */
	status = target_if_wifi_pos_init_srngs(priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("dma init failed: %d", status);
		goto dma_init_failed;
	}

	/* send cfg req cmd to firmware */
	status = target_if_wifi_pos_cfg_fw(psoc, priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("configure to FW failed: %d", status);
		goto dma_init_failed;
	}

	return QDF_STATUS_SUCCESS;

dma_init_failed:
	target_if_wifi_pos_deinit_dma_rings(psoc);
	return status;
}

#endif
