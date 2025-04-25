/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __WLAN_DP_FISA_RX_H__
#define __WLAN_DP_FISA_RX_H__

#ifdef WLAN_SUPPORT_RX_FISA
#include <dp_types.h>
#endif
#include <qdf_status.h>
#include <wlan_dp_priv.h>

//#define FISA_DEBUG_ENABLE

#ifdef FISA_DEBUG_ENABLE
#define dp_fisa_debug dp_info
#else
#define dp_fisa_debug(params...)
#endif

#if defined(WLAN_SUPPORT_RX_FISA)

/*
 * Below is different types of max MSDU aggregation supported in FISA.
 * Host should send one value less so that F.W will increment one
 * and program in RXOLE reg
 */
#define DP_RX_FISA_MAX_AGGR_COUNT_DEFAULT	(16 - 1)
#define	DP_RX_FISA_MAX_AGGR_COUNT_1		(32 - 1)

#define FSE_CACHE_FLUSH_TIME_OUT	5 /* milliSeconds */
#define FISA_UDP_MAX_DATA_LEN		1470 /* udp max data length */
#define FISA_UDP_HDR_LEN		8 /* udp header length */
/* single packet max cumulative ip length */
#define FISA_MAX_SINGLE_CUMULATIVE_IP_LEN \
	(FISA_UDP_MAX_DATA_LEN + FISA_UDP_HDR_LEN)
/* max flow cumulative ip length */
#define FISA_FLOW_MAX_CUMULATIVE_IP_LEN \
	(FISA_MAX_SINGLE_CUMULATIVE_IP_LEN * FISA_FLOW_MAX_AGGR_COUNT)

/* minimal pure UDP data length required for FISA */
#define FISA_MIN_UDP_DATA_LEN 64
/* minimal length without L2/L3 header required for FISA */
#define FISA_MIN_L4_AND_DATA_LEN \
	(FISA_UDP_HDR_LEN + FISA_MIN_UDP_DATA_LEN)

/* CMEM size for FISA FST 16K */
#define DP_CMEM_FST_SIZE 16384

#define IPSEC_PORT 500
#define IPSEC_NAT_PORT 4500
#define DNS_SERVER_PORT 53

#define DP_FT_LOCK_MAX_RECORDS 32

#define FISA_FT_ENTRY_AGING_US	1000000

struct dp_fisa_rx_fst_update_elem {
	/* Do not add new entries here */
	qdf_list_node_t node;
	struct cdp_rx_flow_tuple_info flow_tuple_info;
	struct dp_vdev *vdev;
	uint8_t vdev_id;
	uint32_t flow_idx;
	uint32_t reo_dest_indication;
	bool is_tcp_flow;
	bool is_udp_flow;
	u8 reo_id;
};

enum dp_ft_lock_event_type {
	DP_FT_LOCK_EVENT,
	DP_FT_UNLOCK_EVENT,
};

struct dp_ft_lock_record {
	const char *func;
	int cpu_id;
	uint64_t timestamp;
	enum dp_ft_lock_event_type type;
};

struct dp_ft_lock_history {
	uint32_t record_idx;
	struct dp_ft_lock_record ft_lock_rec[DP_FT_LOCK_MAX_RECORDS];
};

/**
 * dp_fisa_rx() - FISA Rx packet delivery entry function
 * @dp_ctx: DP component handle
 * @vdev: core txrx vdev
 * @nbuf_list: Delivery list of nbufs
 *
 * Return: Success on aggregation
 */
QDF_STATUS dp_fisa_rx(struct wlan_dp_psoc_context *dp_ctx,
		      struct dp_vdev *vdev,
		      qdf_nbuf_t nbuf_list);

/**
 * dp_rx_fisa_flush_by_ctx_id() - FISA Rx flush function to flush
 *				  aggregation at end of NAPI
 * @soc: core txrx main context
 * @napi_id: Flows which are rxed on the NAPI ID to be flushed
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_fisa_flush_by_ctx_id(struct dp_soc *soc, int napi_id);

/**
 * dp_rx_fisa_flush_by_vdev_id() - Flush fisa aggregates per vdev id
 * @soc: core txrx main context
 * @vdev_id: vdev ID
 *
 * Return: Success on flushing the flows for the vdev
 */
QDF_STATUS dp_rx_fisa_flush_by_vdev_id(struct dp_soc *soc, uint8_t vdev_id);

/**
 * dp_fisa_rx_fst_update_work() - Work functions for FST updates
 * @arg: argument passed to the work function
 *
 * Return: None
 */
void dp_fisa_rx_fst_update_work(void *arg);

/**
 * dp_suspend_fse_cache_flush() - Suspend FSE cache flush
 * @dp_ctx: DP component context
 *
 * Return: None
 */
void dp_suspend_fse_cache_flush(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_rx_fst_attach() - Initialize Rx FST and setup necessary parameters
 * @dp_ctx: DP component context
 *
 * Return: Handle to flow search table entry
 */
QDF_STATUS dp_rx_fst_attach(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_rx_fst_target_config() - Configure RX OLE FSE engine in HW
 * @dp_ctx: DP component context
 *
 * Return: Success
 */
QDF_STATUS dp_rx_fst_target_config(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_rx_fisa_config() - Configure FISA related settings
 * @dp_ctx: DP component context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_fisa_config(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_rx_fst_detach() - De-initialize Rx FST
 * @dp_ctx: DP component context
 *
 * Return: None
 */
void dp_rx_fst_detach(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_resume_fse_cache_flush() - Resume FSE cache flush
 * @dp_ctx: DP component context
 *
 * Return: None
 */
void dp_resume_fse_cache_flush(struct wlan_dp_psoc_context *dp_ctx);

/**
 * dp_rx_fst_update_pm_suspend_status() - Update Suspend status in FISA
 * @dp_ctx: DP component context
 * @suspended: Flag to indicate suspend or not
 *
 * Return: None
 */
void dp_rx_fst_update_pm_suspend_status(struct wlan_dp_psoc_context *dp_ctx,
					bool suspended);

/**
 * dp_rx_fst_requeue_wq() - Re-queue pending work queue tasks
 * @dp_ctx: DP component context
 *
 * Return: None
 */
void dp_rx_fst_requeue_wq(struct wlan_dp_psoc_context *dp_ctx);

void dp_print_fisa_rx_stats(enum cdp_fisa_stats_id stats_id);

/**
 * dp_fisa_cfg_init() - FISA INI items init
 * @config: SoC CFG config
 * @psoc: Objmgr PSoC handle
 *
 * Return: None
 */
void dp_fisa_cfg_init(struct wlan_dp_psoc_cfg *config,
		      struct wlan_objmgr_psoc *psoc);

/**
 * dp_set_fst_in_cmem() - Set flag to indicate FST is in CMEM
 * @fst_in_cmem: Flag to indicate FST is in CMEM
 *
 * Return: None
 */
void dp_set_fst_in_cmem(bool fst_in_cmem);

/**
 * dp_set_fisa_dynamic_aggr_size_support() - Set flag to indicate dynamic
 *					     aggregation size support
 * @dynamic_aggr_size_support: Flag to indicate dynamic aggregation support
 *
 * Return: None
 */
void dp_set_fisa_dynamic_aggr_size_support(bool dynamic_aggr_size_support);
#else
static inline void
dp_rx_fst_update_pm_suspend_status(struct wlan_dp_psoc_context *dp_ctx,
				   bool suspended)
{
}

static inline void dp_print_fisa_rx_stats(enum cdp_fisa_stats_id stats_id)
{
}

static inline void dp_fisa_cfg_init(struct wlan_dp_psoc_cfg *config,
				    struct wlan_objmgr_psoc *psoc)
{
}

static inline void dp_set_fst_in_cmem(bool fst_in_cmem)
{
}

static inline void
dp_set_fisa_dynamic_aggr_size_support(bool dynamic_aggr_size_support)
{
}
#endif
#endif
