/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.

 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_dp_main.h>
#include <wlan_dp_priv.h>
#include <wlan_dp_prealloc.h>
#include <dp_types.h>
#include <dp_internal.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_misc.h>
#include <dp_tx_desc.h>
#include <dp_rx.h>
#include <ce_api.h>
#include <ce_internal.h>
#include <wlan_cfg.h>
#include "wlan_dp_prealloc.h"
#ifdef WIFI_MONITOR_SUPPORT
#include <dp_mon.h>
#endif
#ifdef WLAN_PKT_CAPTURE_TX_2_0
#include "mon_ingress_ring.h"
#include "mon_destination_ring.h"
#include "dp_mon_2.0.h"
#endif

#ifdef DP_MEM_PRE_ALLOC

/* Max entries in FISA Flow table */
#define FISA_RX_FT_SIZE 256

/* Num elements in REO ring */
#define REO_DST_RING_SIZE 1024

/* Num elements in TCL Data ring */
#define TCL_DATA_RING_SIZE 5120

/* Num elements in WBM2SW ring */
#define WBM2SW_RELEASE_RING_SIZE 8192

/* Num elements in WBM Idle Link */
#define WBM_IDLE_LINK_RING_SIZE (32 * 1024)

/* Num TX desc in TX desc pool */
#define DP_TX_DESC_POOL_SIZE 6144

#define DP_TX_RX_DESC_MAX_NUM \
		(WLAN_CFG_NUM_TX_DESC_MAX * MAX_TXDESC_POOLS + \
			WLAN_CFG_RX_SW_DESC_NUM_SIZE_MAX * MAX_RXDESC_POOLS)

/**
 * struct dp_consistent_prealloc - element representing DP pre-alloc memory
 * @ring_type: HAL ring type
 * @size: size of pre-alloc memory
 * @in_use: whether this element is in use (occupied)
 * @va_unaligned: Unaligned virtual address
 * @va_aligned: aligned virtual address.
 * @pa_unaligned: Unaligned physical address.
 * @pa_aligned: Aligned physical address.
 */

struct dp_consistent_prealloc {
	enum hal_ring_type ring_type;
	uint32_t size;
	uint8_t in_use;
	void *va_unaligned;
	void *va_aligned;
	qdf_dma_addr_t pa_unaligned;
	qdf_dma_addr_t pa_aligned;
};

/**
 * struct dp_multi_page_prealloc -  element representing DP pre-alloc multiple
 *				    pages memory
 * @desc_type: source descriptor type for memory allocation
 * @element_size: single element size
 * @element_num: total number of elements should be allocated
 * @in_use: whether this element is in use (occupied)
 * @cacheable: coherent memory or cacheable memory
 * @pages: multi page information storage
 */
struct dp_multi_page_prealloc {
	enum qdf_dp_desc_type desc_type;
	qdf_size_t element_size;
	uint16_t element_num;
	bool in_use;
	bool cacheable;
	struct qdf_mem_multi_page_t pages;
};

/**
 * struct dp_consistent_prealloc_unaligned - element representing DP pre-alloc
 *					     unaligned memory
 * @ring_type: HAL ring type
 * @size: size of pre-alloc memory
 * @in_use: whether this element is in use (occupied)
 * @va_unaligned: unaligned virtual address
 * @pa_unaligned: unaligned physical address
 */
struct dp_consistent_prealloc_unaligned {
	enum hal_ring_type ring_type;
	uint32_t size;
	bool in_use;
	void *va_unaligned;
	qdf_dma_addr_t pa_unaligned;
};

/**
 * struct dp_prealloc_context - element representing DP prealloc context memory
 * @ctxt_type: DP context type
 * @size: size of pre-alloc memory
 * @in_use: check if element is being used
 * @is_critical: critical prealloc failure would cause prealloc_init to fail
 * @addr: address of memory allocated
 */
struct dp_prealloc_context {
	enum dp_ctxt_type ctxt_type;
	uint32_t size;
	bool in_use;
	bool is_critical;
	void *addr;
};

static struct dp_prealloc_context g_dp_context_allocs[] = {
	{DP_PDEV_TYPE, (sizeof(struct dp_pdev)), false,  true, NULL},
#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
	/* 4 Rx ring history */
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
#ifdef CONFIG_BERYLLIUM
	/* 4 extra Rx ring history */
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
#endif /* CONFIG_BERYLLIUM */
	/* 1 Rx error ring history */
	{DP_RX_ERR_RING_HIST_TYPE, sizeof(struct dp_rx_err_history),
	 false, false, NULL},
#ifndef RX_DEFRAG_DO_NOT_REINJECT
	/* 1 Rx reinject ring history */
	{DP_RX_REINJECT_RING_HIST_TYPE, sizeof(struct dp_rx_reinject_history),
	 false, false, NULL},
#endif	/* RX_DEFRAG_DO_NOT_REINJECT */
	/* 1 Rx refill ring history */
	{DP_RX_REFILL_RING_HIST_TYPE, sizeof(struct dp_rx_refill_history),
	false, false, NULL},
#endif	/* WLAN_FEATURE_DP_RX_RING_HISTORY */
#ifdef DP_TX_HW_DESC_HISTORY
	{DP_TX_HW_DESC_HIST_TYPE,
	DP_TX_HW_DESC_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_hw_desc_evt),
	false, false, NULL},
	{DP_TX_HW_DESC_HIST_TYPE,
	DP_TX_HW_DESC_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_hw_desc_evt),
	false, false, NULL},
	{DP_TX_HW_DESC_HIST_TYPE,
	DP_TX_HW_DESC_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_hw_desc_evt),
	false, false, NULL},
#endif
#ifdef WLAN_FEATURE_DP_TX_DESC_HISTORY
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_TCL_HIST_TYPE,
	DP_TX_TCL_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},

	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},
	{DP_TX_COMP_HIST_TYPE,
	DP_TX_COMP_HIST_PER_SLOT_MAX * sizeof(struct dp_tx_desc_event),
	false, false, NULL},

#endif	/* WLAN_FEATURE_DP_TX_DESC_HISTORY */
#ifdef WLAN_SUPPORT_RX_FISA
	{DP_FISA_RX_FT_TYPE, sizeof(struct dp_fisa_rx_sw_ft) * FISA_RX_FT_SIZE,
	 false, true, NULL},
#endif
#ifdef WLAN_FEATURE_DP_MON_STATUS_RING_HISTORY
	{DP_MON_STATUS_BUF_HIST_TYPE, sizeof(struct dp_mon_status_ring_history),
	 false, false, NULL},
#endif
#ifdef WIFI_MONITOR_SUPPORT
	{DP_MON_PDEV_TYPE, sizeof(struct dp_mon_pdev),
	 false, false, NULL},
#endif
#ifdef WLAN_FEATURE_DP_CFG_EVENT_HISTORY
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
	{DP_CFG_EVENT_HIST_TYPE,
	 DP_CFG_EVT_HIST_PER_SLOT_MAX * sizeof(struct dp_cfg_event),
	 false, false, NULL},
#endif
#ifdef WLAN_PKT_CAPTURE_TX_2_0
	{DP_MON_TX_DESC_POOL_TYPE, 0, false, false, NULL},
#endif
};

static struct  dp_consistent_prealloc g_dp_consistent_allocs[] = {
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
#ifdef CONFIG_BERYLLIUM
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0,
	NULL, NULL, 0, 0},
#endif
	/* 3 TCL data rings */
	{TCL_DATA, 0, 0, NULL, NULL, 0, 0},
	{TCL_DATA, 0, 0, NULL, NULL, 0, 0},
	{TCL_DATA, 0, 0, NULL, NULL, 0, 0},
	/* 4 WBM2SW rings */
	{WBM2SW_RELEASE, 0, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, 0, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, 0, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, 0, 0, NULL, 0, 0},
	/* SW2WBM link descriptor return ring */
	{SW2WBM_RELEASE, 0, 0, NULL, 0, 0},
	/* 1 WBM idle link desc ring */
	{WBM_IDLE_LINK, (sizeof(struct wbm_link_descriptor_ring)) *
	WBM_IDLE_LINK_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* 2 RXDMA DST ERR rings */
	{RXDMA_DST, 0, 0, NULL, NULL, 0, 0},
	{RXDMA_DST, 0, 0, NULL, NULL, 0, 0},
	/* REFILL ring 0 */
	{RXDMA_BUF, 0, 0, NULL, NULL, 0, 0},
	/* 2 RXDMA buffer rings */
	{RXDMA_BUF, 0, 0, NULL, NULL, 0, 0},
	{RXDMA_BUF, 0, 0, NULL, NULL, 0, 0},
	/* REO Exception ring */
	{REO_EXCEPTION, 0, 0, NULL, NULL, 0, 0},
	/* 1 REO status ring */
	{REO_STATUS, 0, 0, NULL, NULL, 0, 0},
	/* 2 monitor status rings */
	{RXDMA_MONITOR_STATUS, 0, 0, NULL, NULL, 0, 0},
	{RXDMA_MONITOR_STATUS, 0, 0, NULL, NULL, 0, 0},
#ifdef WLAN_PKT_CAPTURE_TX_2_0
	/* 2 MON2SW Tx monitor rings */
	{TX_MONITOR_DST, 0, 0, NULL, NULL, 0, 0},
	{TX_MONITOR_DST, 0, 0, NULL, NULL, 0, 0},
#endif
};

/* Number of HW link descriptors needed (rounded to power of 2) */
#define NUM_HW_LINK_DESCS (32 * 1024)

/* Size in bytes of HW LINK DESC */
#define HW_LINK_DESC_SIZE 128

/* Size in bytes of TX Desc (rounded to power of 2) */
#define TX_DESC_SIZE 128

/* Size in bytes of TX TSO Desc (rounded to power of 2) */
#define TX_TSO_DESC_SIZE 256

/* Size in bytes of TX TSO Num Seg Desc (rounded to power of 2) */
#define TX_TSO_NUM_SEG_DESC_SIZE 16

#define NON_CACHEABLE 0
#define CACHEABLE 1

#define DIRECT_LINK_CE_RX_BUF_SIZE  256
#define DIRECT_LINK_DEFAULT_BUF_SZ  2048
#define TX_DIRECT_LINK_BUF_NUM      380
#define TX_DIRECT_LINK_CE_BUF_NUM   8
#define RX_DIRECT_LINK_CE_BUF_NUM   30

static struct  dp_multi_page_prealloc g_dp_multi_page_allocs[] = {
	/* 4 TX DESC pools */
	{QDF_DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },

	/* 4 Tx EXT DESC NON Cacheable pools */
	{QDF_DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },
	{QDF_DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },
	{QDF_DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },
	{QDF_DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },

	/* 4 Tx EXT DESC Link Cacheable pools */
	{QDF_DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0,
	 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0,
	 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0,
	 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0,
	 0, CACHEABLE, { 0 } },

	/* 4 TX TSO DESC pools */
	{QDF_DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{QDF_DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },

	/* 4 TX TSO NUM SEG DESC pools */
	{QDF_DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },
	{QDF_DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },
	{QDF_DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },
	{QDF_DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },

	/* DP RX DESCs BUF pools */
	{QDF_DP_RX_DESC_BUF_TYPE, sizeof(union dp_rx_desc_list_elem_t),
	 0, 0, CACHEABLE, { 0 } },

#ifdef DISABLE_MON_CONFIG
	/* no op */
#else
	/* 2 DP RX DESCs Status pools */
	{QDF_DP_RX_DESC_STATUS_TYPE, sizeof(union dp_rx_desc_list_elem_t),
	 WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE + 1, 0, CACHEABLE, { 0 } },
	{QDF_DP_RX_DESC_STATUS_TYPE, sizeof(union dp_rx_desc_list_elem_t),
	 WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE + 1, 0, CACHEABLE, { 0 } },
#endif
	/* DP HW Link DESCs pools */
	{QDF_DP_HW_LINK_DESC_TYPE, HW_LINK_DESC_SIZE, NUM_HW_LINK_DESCS, 0,
	 NON_CACHEABLE, { 0 } },
#ifdef CONFIG_BERYLLIUM
	{QDF_DP_HW_CC_SPT_PAGE_TYPE, qdf_page_size,
	 ((DP_TX_RX_DESC_MAX_NUM * sizeof(uint64_t)) / qdf_page_size),
	 0, NON_CACHEABLE, { 0 } },
#endif
#ifdef FEATURE_DIRECT_LINK
	{QDF_DP_TX_DIRECT_LINK_CE_BUF_TYPE, DIRECT_LINK_DEFAULT_BUF_SZ,
	 TX_DIRECT_LINK_CE_BUF_NUM, 0, NON_CACHEABLE, { 0 } },
	{QDF_DP_TX_DIRECT_LINK_BUF_TYPE, DIRECT_LINK_DEFAULT_BUF_SZ,
	 TX_DIRECT_LINK_BUF_NUM, 0, NON_CACHEABLE, { 0 } },
	{QDF_DP_RX_DIRECT_LINK_CE_BUF_TYPE, DIRECT_LINK_CE_RX_BUF_SIZE,
	 RX_DIRECT_LINK_CE_BUF_NUM, 0, NON_CACHEABLE, { 0 } },
#endif
};

static struct dp_consistent_prealloc_unaligned
		g_dp_consistent_unaligned_allocs[] = {
	/* CE-0 */
	{CE_SRC, (sizeof(struct ce_srng_src_desc) * 16 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	/* CE-1 */
	{CE_DST, (sizeof(struct ce_srng_dest_desc) * 512 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	{CE_DST_STATUS, (sizeof(struct ce_srng_dest_status_desc) * 512
	 + CE_DESC_RING_ALIGN), false, NULL, 0},
	/* CE-2 */
	{CE_DST, (sizeof(struct ce_srng_dest_desc) * 32 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	{CE_DST_STATUS, (sizeof(struct ce_srng_dest_status_desc) * 32
	 + CE_DESC_RING_ALIGN), false, NULL, 0},
	/* CE-3 */
	{CE_SRC, (sizeof(struct ce_srng_src_desc) * 32 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	/* CE-4 */
	{CE_SRC, (sizeof(struct ce_srng_src_desc) * 256 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	/* CE-5 */
	{CE_DST, (sizeof(struct ce_srng_dest_desc) * 512 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	{CE_DST_STATUS, (sizeof(struct ce_srng_dest_status_desc) * 512
	 + CE_DESC_RING_ALIGN), false, NULL, 0},
};

void dp_prealloc_deinit(void)
{
	int i;
	struct dp_prealloc_context *cp;
	struct dp_consistent_prealloc *p;
	struct dp_multi_page_prealloc *mp;
	struct dp_consistent_prealloc_unaligned *up;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx)
		return;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];

		if (p->in_use)
			dp_warn("i %d: consistent_mem in use while free", i);

		if (p->va_aligned) {
			dp_debug("i %d: va aligned %pK pa aligned %pK size %d",
				 i, p->va_aligned, (void *)p->pa_aligned,
				 p->size);
			qdf_mem_free_consistent(qdf_ctx, qdf_ctx->dev,
						p->size,
						p->va_unaligned,
						p->pa_unaligned, 0);
			p->in_use = false;
			p->va_unaligned = NULL;
			p->va_aligned = NULL;
			p->pa_unaligned = 0;
			p->pa_aligned = 0;
		}
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (mp->in_use)
			dp_warn("i %d: multi-page mem in use while free", i);

		if (mp->pages.num_pages) {
			dp_info("i %d: type %d cacheable_pages %pK dma_pages %pK num_pages %d",
				i, mp->desc_type,
				mp->pages.cacheable_pages,
				mp->pages.dma_pages,
				mp->pages.num_pages);
			qdf_mem_multi_pages_free(qdf_ctx, &mp->pages,
						 0, mp->cacheable);
			mp->in_use = false;
			qdf_mem_zero(&mp->pages, sizeof(mp->pages));
		}
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];

		if (qdf_unlikely(up->in_use))
			dp_info("i %d: unaligned mem in use while free", i);

		if (up->va_unaligned) {
			dp_info("i %d: va unalign %pK pa unalign %pK size %d",
				i, up->va_unaligned,
				(void *)up->pa_unaligned, up->size);
			qdf_mem_free_consistent(qdf_ctx, qdf_ctx->dev,
						up->size,
						up->va_unaligned,
						up->pa_unaligned, 0);
			up->in_use = false;
			up->va_unaligned = NULL;
			up->pa_unaligned = 0;
		}
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];
		if (qdf_unlikely(cp->in_use))
			dp_warn("i %d: context in use while free", i);

		if (cp->addr) {
			qdf_mem_free(cp->addr);
			cp->addr = NULL;
		}
	}
}

#ifdef CONFIG_BERYLLIUM
/**
 * dp_get_tcl_data_srng_entrysize() - Get the tcl data srng entry
 *  size
 *
 * Return: TCL data srng entry size
 */
static inline uint32_t dp_get_tcl_data_srng_entrysize(void)
{
	return sizeof(struct tcl_data_cmd);
}

#ifdef WLAN_PKT_CAPTURE_TX_2_0
/**
 * dp_get_tx_mon_mem_size() - Get tx mon ring memory size
 * @cfg: prealloc config
 * @ring_type: ring type
 *
 * Return: Tx mon ring memory size
 */
static inline
uint32_t dp_get_tx_mon_mem_size(struct wlan_dp_prealloc_cfg *cfg,
				enum hal_ring_type ring_type)
{
	uint32_t mem_size = 0;

	if (!cfg)
		return mem_size;

	if (ring_type == TX_MONITOR_BUF) {
		mem_size = (sizeof(struct mon_ingress_ring)) *
			    cfg->num_tx_mon_buf_ring_entries;
	} else if (ring_type == TX_MONITOR_DST) {
		mem_size = (sizeof(struct mon_destination_ring)) *
			    cfg->num_tx_mon_dst_ring_entries;
	}

	return mem_size;
}

/**
 * dp_get_tx_mon_desc_pool_mem_size() - Get tx mon desc pool memory size
 * @cfg: prealloc config
 *
 * Return : TX mon desc pool memory size
 */
static inline
uint32_t dp_get_tx_mon_desc_pool_mem_size(struct wlan_dp_prealloc_cfg *cfg)
{
	return (sizeof(union dp_mon_desc_list_elem_t)) *
		cfg->num_tx_mon_buf_ring_entries;
}
#else
static inline
uint32_t dp_get_tx_mon_mem_size(struct wlan_dp_prealloc_cfg *cfg,
				enum hal_ring_type ring_type)
{
	return 0;
}

static inline
uint32_t dp_get_tx_mon_desc_pool_mem_size(struct wlan_dp_prealloc_cfg *cfg)
{
	return 0;
}
#endif /* WLAN_PKT_CAPTURE_TX_2_0 */
#else
static inline uint32_t dp_get_tcl_data_srng_entrysize(void)
{
	return (sizeof(struct tlv_32_hdr) + sizeof(struct tcl_data_cmd));
}

static inline
uint32_t dp_get_tx_mon_mem_size(struct wlan_dp_prealloc_cfg *cfg,
				enum hal_ring_type ring_type)
{
	return 0;
}

static inline
uint32_t dp_get_tx_mon_desc_pool_mem_size(struct wlan_dp_prealloc_cfg *cfg)
{
	return 0;
}
#endif

/**
 * dp_update_mem_size_by_ctx_type() - Update dp context memory size
 *                                    based on context type
 * @cfg: prealloc related cfg params
 * @ctx_type: DP context type
 * @mem_size: memory size to be updated
 *
 * Return: none
 */
static void
dp_update_mem_size_by_ctx_type(struct wlan_dp_prealloc_cfg *cfg,
			       enum dp_ctxt_type ctx_type,
			       uint32_t *mem_size)
{
	switch (ctx_type) {
	case DP_MON_TX_DESC_POOL_TYPE:
		*mem_size = dp_get_tx_mon_desc_pool_mem_size(cfg);
		break;
	default:
		return;
	}
}

/**
 * dp_update_mem_size_by_ring_type() - Update srng memory size based
 *  on ring type and the corresponding ini configuration
 * @cfg: prealloc related cfg params
 * @ring_type: srng type
 * @mem_size: memory size to be updated
 *
 * Return: None
 */
static void
dp_update_mem_size_by_ring_type(struct wlan_dp_prealloc_cfg *cfg,
				enum hal_ring_type ring_type,
				uint32_t *mem_size)
{
	switch (ring_type) {
	case TCL_DATA:
		*mem_size = dp_get_tcl_data_srng_entrysize() *
			    cfg->num_tx_ring_entries;
		return;
	case WBM2SW_RELEASE:
		*mem_size = (sizeof(struct wbm_release_ring)) *
			    cfg->num_tx_comp_ring_entries;
		return;
	case SW2WBM_RELEASE:
		*mem_size = (sizeof(struct wbm_release_ring)) *
			    cfg->num_wbm_rel_ring_entries;
		return;
	case RXDMA_DST:
		*mem_size = (sizeof(struct reo_entrance_ring)) *
			    cfg->num_rxdma_err_dst_ring_entries;
		return;
	case REO_EXCEPTION:
		*mem_size = (sizeof(struct reo_destination_ring)) *
			    cfg->num_reo_exception_ring_entries;
		return;
	case REO_DST:
		*mem_size = (sizeof(struct reo_destination_ring)) *
			    cfg->num_reo_dst_ring_entries;
		return;
	case RXDMA_BUF:
		*mem_size = (sizeof(struct wbm_buffer_ring)) *
			    cfg->num_rxdma_refill_ring_entries;
		return;
	case REO_STATUS:
		*mem_size = (sizeof(struct tlv_32_hdr) +
			     sizeof(struct reo_get_queue_stats_status)) *
			    cfg->num_reo_status_ring_entries;
		return;
	case RXDMA_MONITOR_STATUS:
		*mem_size = (sizeof(struct wbm_buffer_ring)) *
			    cfg->num_mon_status_ring_entries;
		return;
	case TX_MONITOR_BUF:
	case TX_MONITOR_DST:
		*mem_size = dp_get_tx_mon_mem_size(cfg, ring_type);
		return;
	default:
		return;
	}
}

/**
 * dp_update_num_elements_by_desc_type() - Update num of descriptors based
 *  on type and the corresponding ini configuration
 * @cfg: prealloc related cfg params
 * @desc_type: descriptor type
 * @num_elements: num of descriptor elements
 *
 * Return: None
 */
static void
dp_update_num_elements_by_desc_type(struct wlan_dp_prealloc_cfg *cfg,
				    enum qdf_dp_desc_type desc_type,
				    uint16_t *num_elements)
{
	switch (desc_type) {
	case QDF_DP_TX_DESC_TYPE:
		*num_elements = cfg->num_tx_desc;
		return;
	case QDF_DP_TX_EXT_DESC_TYPE:
	case QDF_DP_TX_EXT_DESC_LINK_TYPE:
	case QDF_DP_TX_TSO_DESC_TYPE:
	case QDF_DP_TX_TSO_NUM_SEG_TYPE:
		*num_elements = cfg->num_tx_ext_desc;
		return;
	case QDF_DP_RX_DESC_BUF_TYPE:
		*num_elements = cfg->num_rx_sw_desc * WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE;
		return;
	default:
		return;
	}
}

#ifdef WLAN_DP_PROFILE_SUPPORT
static void
wlan_dp_sync_prealloc_with_profile_cfg(struct wlan_dp_prealloc_cfg *cfg)
{
	struct wlan_dp_memory_profile_info *profile_info;
	struct wlan_dp_memory_profile_ctx *profile_ctx;
	int i;

	profile_info = wlan_dp_get_profile_info();
	if (!profile_info->is_selected)
		return;

	for (i = 0; i < profile_info->size; i++) {
		profile_ctx = &profile_info->ctx[i];

		switch (profile_ctx->param_type) {
		case DP_TX_DESC_NUM_CFG:
			cfg->num_tx_desc = profile_ctx->size;
			break;
		case DP_TX_EXT_DESC_NUM_CFG:
			cfg->num_tx_ext_desc = profile_ctx->size;
			break;
		case DP_TX_RING_SIZE_CFG:
			cfg->num_tx_ring_entries = profile_ctx->size;
			break;
		case DP_TX_COMPL_RING_SIZE_CFG:
			cfg->num_tx_comp_ring_entries = profile_ctx->size;
			break;
		case DP_RX_SW_DESC_NUM_CFG:
			cfg->num_rx_sw_desc = profile_ctx->size;
			break;
		case DP_REO_DST_RING_SIZE_CFG:
			cfg->num_reo_dst_ring_entries = profile_ctx->size;
			break;
		case DP_RXDMA_BUF_RING_SIZE_CFG:
			cfg->num_rxdma_buf_ring_entries = profile_ctx->size;
			break;
		case DP_RXDMA_REFILL_RING_SIZE_CFG:
			cfg->num_rxdma_refill_ring_entries = profile_ctx->size;
			break;
		default:
			break;
		}
	}
}
#else

static inline void
wlan_dp_sync_prealloc_with_profile_cfg(struct wlan_dp_prealloc_cfg *cfg) {}
#endif

QDF_STATUS dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc)
{
	int i;
	struct dp_prealloc_context *cp;
	struct dp_consistent_prealloc *p;
	struct dp_multi_page_prealloc *mp;
	struct dp_consistent_prealloc_unaligned *up;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	struct wlan_dp_prealloc_cfg cfg;

	if (!qdf_ctx || !ctrl_psoc) {
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	wlan_cfg_get_prealloc_cfg(ctrl_psoc, &cfg);
	wlan_dp_sync_prealloc_with_profile_cfg(&cfg);

	/*Context pre-alloc*/
	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];
		dp_update_mem_size_by_ctx_type(&cfg, cp->ctxt_type,
					       &cp->size);
		cp->addr = qdf_mem_malloc(cp->size);

		if (qdf_unlikely(!cp->addr) && cp->is_critical) {
			dp_warn("i %d: unable to preallocate %d bytes memory!",
				i, cp->size);
			break;
		}
	}

	if (i != QDF_ARRAY_SIZE(g_dp_context_allocs)) {
		dp_err("unable to allocate context memory!");
		goto deinit;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		p->in_use = 0;
		dp_update_mem_size_by_ring_type(&cfg, p->ring_type, &p->size);
		p->va_aligned =
			qdf_aligned_mem_alloc_consistent(qdf_ctx,
							 &p->size,
							 &p->va_unaligned,
							 &p->pa_unaligned,
							 &p->pa_aligned,
							 DP_RING_BASE_ALIGN);
		if (qdf_unlikely(!p->va_unaligned)) {
			dp_warn("i %d: unable to preallocate %d bytes memory!",
				i, p->size);
			break;
		}
		dp_debug("i %d: va aligned %pK pa aligned %pK size %d",
			 i, p->va_aligned, (void *)p->pa_aligned, p->size);
	}

	if (i != QDF_ARRAY_SIZE(g_dp_consistent_allocs)) {
		dp_err("unable to allocate consistent memory!");
		goto deinit;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];
		mp->in_use = false;
		dp_update_num_elements_by_desc_type(&cfg, mp->desc_type,
						    &mp->element_num);
		if (mp->cacheable)
			mp->pages.page_size = DP_BLOCKMEM_SIZE;

		qdf_mem_multi_pages_alloc(qdf_ctx, &mp->pages,
					  mp->element_size,
					  mp->element_num,
					  0, mp->cacheable);
		if (qdf_unlikely(!mp->pages.num_pages)) {
			dp_warn("i %d: preallocate %d bytes multi-pages failed!",
				i, (int)(mp->element_size * mp->element_num));
			break;
		}

		mp->pages.is_mem_prealloc = true;
		dp_info("i %d: cacheable_pages %pK dma_pages %pK num_pages %d",
			i, mp->pages.cacheable_pages,
			mp->pages.dma_pages,
			mp->pages.num_pages);
	}

	if (i != QDF_ARRAY_SIZE(g_dp_multi_page_allocs)) {
		dp_err("unable to allocate multi-pages memory!");
		goto deinit;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];
		up->in_use = 0;
		up->va_unaligned = qdf_mem_alloc_consistent(qdf_ctx,
							    qdf_ctx->dev,
							    up->size,
							    &up->pa_unaligned);
		if (qdf_unlikely(!up->va_unaligned)) {
			dp_warn("i %d: fail to prealloc unaligned %d bytes!",
				i, up->size);
			break;
		}
		dp_info("i %d: va unalign %pK pa unalign %pK size %d",
			i, up->va_unaligned,
			(void *)up->pa_unaligned, up->size);
	}

	if (i != QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs)) {
		dp_info("unable to allocate unaligned memory!");
		/*
		 * Only if unaligned memory prealloc fail, is deinit
		 * necessary for all other DP srng/multi-pages memory?
		 */
		goto deinit;
	}

	return QDF_STATUS_SUCCESS;
deinit:
	dp_prealloc_deinit();
	return QDF_STATUS_E_FAILURE;
}

void *dp_prealloc_get_context_memory(uint32_t ctxt_type, qdf_size_t ctxt_size)
{
	int i;
	struct dp_prealloc_context *cp;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];

		if ((ctxt_type == cp->ctxt_type) && !cp->in_use &&
		    cp->addr && ctxt_size <= cp->size) {
			cp->in_use = true;
			return cp->addr;
		}
	}

	return NULL;
}

QDF_STATUS dp_prealloc_put_context_memory(uint32_t ctxt_type, void *vaddr)
{
	int i;
	struct dp_prealloc_context *cp;

	if (!vaddr)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];

		if ((ctxt_type == cp->ctxt_type) && vaddr == cp->addr) {
			qdf_mem_zero(cp->addr, cp->size);
			cp->in_use = false;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

void *dp_prealloc_get_coherent(uint32_t *size, void **base_vaddr_unaligned,
			       qdf_dma_addr_t *paddr_unaligned,
			       qdf_dma_addr_t *paddr_aligned,
			       uint32_t align,
			       uint32_t ring_type)
{
	int i;
	struct dp_consistent_prealloc *p;
	void *va_aligned = NULL;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		if (p->ring_type == ring_type && !p->in_use &&
		    p->va_unaligned && *size <= p->size) {
			p->in_use = 1;
			*base_vaddr_unaligned = p->va_unaligned;
			*paddr_unaligned = p->pa_unaligned;
			*paddr_aligned = p->pa_aligned;
			va_aligned = p->va_aligned;
			*size = p->size;
			dp_debug("index %i -> ring type %s va-aligned %pK", i,
				 dp_srng_get_str_from_hal_ring_type(ring_type),
				 va_aligned);
			break;
		}
	}

	if (i == QDF_ARRAY_SIZE(g_dp_consistent_allocs))
		dp_info("unable to allocate memory for ring type %s (%d) size %d",
			dp_srng_get_str_from_hal_ring_type(ring_type),
			ring_type, *size);
	return va_aligned;
}

void dp_prealloc_put_coherent(qdf_size_t size, void *vaddr_unligned,
			      qdf_dma_addr_t paddr)
{
	int i;
	struct dp_consistent_prealloc *p;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		if (p->va_unaligned == vaddr_unligned) {
			dp_debug("index %d, returned", i);
			p->in_use = 0;
			qdf_mem_zero(p->va_unaligned, p->size);
			break;
		}
	}

	if (i == QDF_ARRAY_SIZE(g_dp_consistent_allocs))
		dp_err("unable to find vaddr %pK", vaddr_unligned);
}

void dp_prealloc_get_multi_pages(uint32_t desc_type,
				 qdf_size_t element_size,
				 uint16_t element_num,
				 struct qdf_mem_multi_page_t *pages,
				 bool cacheable)
{
	int i;
	struct dp_multi_page_prealloc *mp;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (desc_type == mp->desc_type && !mp->in_use &&
		    mp->pages.num_pages && element_size == mp->element_size &&
		    element_num <= mp->element_num) {
			mp->in_use = true;
			*pages = mp->pages;

			dp_info("i %d: desc_type %d cacheable_pages %pK dma_pages %pK num_pages %d",
				i, desc_type,
				mp->pages.cacheable_pages,
				mp->pages.dma_pages,
				mp->pages.num_pages);
			break;
		}
	}
}

void dp_prealloc_put_multi_pages(uint32_t desc_type,
				 struct qdf_mem_multi_page_t *pages)
{
	int i;
	struct dp_multi_page_prealloc *mp;
	bool mp_found = false;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (desc_type == mp->desc_type) {
			/* compare different address by cacheable flag */
			mp_found = mp->cacheable ?
				(mp->pages.cacheable_pages ==
				 pages->cacheable_pages) :
				(mp->pages.dma_pages == pages->dma_pages);
			/* find it, put back to prealloc pool */
			if (mp_found) {
				dp_info("i %d: desc_type %d returned",
					i, desc_type);
				mp->in_use = false;
				qdf_mem_multi_pages_zero(&mp->pages,
							 mp->cacheable);
				break;
			}
		}
	}

	if (qdf_unlikely(!mp_found))
		dp_warn("Not prealloc pages %pK desc_type %d cacheable_pages %pK dma_pages %pK",
			pages,
			desc_type,
			pages->cacheable_pages,
			pages->dma_pages);
}

void *dp_prealloc_get_consistent_mem_unaligned(qdf_size_t size,
					       qdf_dma_addr_t *base_addr,
					       uint32_t ring_type)
{
	int i;
	struct dp_consistent_prealloc_unaligned *up;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];

		if (ring_type == up->ring_type && size == up->size &&
		    up->va_unaligned && !up->in_use) {
			up->in_use = true;
			*base_addr = up->pa_unaligned;
			dp_info("i %d: va unalign %pK pa unalign %pK size %d",
				i, up->va_unaligned,
				(void *)up->pa_unaligned, up->size);
			return up->va_unaligned;
		}
	}

	return NULL;
}

void dp_prealloc_put_consistent_mem_unaligned(void *va_unaligned)
{
	int i;
	struct dp_consistent_prealloc_unaligned *up;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];

		if (va_unaligned == up->va_unaligned) {
			dp_info("index %d, returned", i);
			up->in_use = false;
			qdf_mem_zero(up->va_unaligned, up->size);
			break;
		}
	}

	if (i == QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs))
		dp_err("unable to find vaddr %pK", va_unaligned);
}
#endif

#ifdef FEATURE_RUNTIME_PM
uint32_t dp_get_tx_inqueue(ol_txrx_soc_handle soc)
{
	struct dp_soc *dp_soc;

	dp_soc = cdp_soc_t_to_dp_soc(soc);

	return qdf_atomic_read(&dp_soc->tx_pending_rtpm);
}
#else
uint32_t dp_get_tx_inqueue(ol_txrx_soc_handle soc)
{
	return 0;
}
#endif
