/*
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

#ifndef _WLAN_DP_WFDS_H_
#define _WLAN_DP_WFDS_H_

#ifdef FEATURE_DIRECT_LINK

#include "qdf_atomic.h"
#include "wlan_dp_priv.h"
#include "wlan_qmi_public_struct.h"
#include "wlan_qmi_wfds_api.h"

#define DP_WFDS_CE_MAX_SRNG QMI_WFDS_CE_MAX_SRNG

/**
 * enum dp_wfds_msg - WFDS message type
 * @DP_WFDS_REQ_MEM_IND_MSG: Memory request indication message
 * @DP_WFDS_IPCC_MAP_N_CFG_IND_MSG: IPCC map and configure indication message
 * @DP_WFDS_MSG_MAX: not a real value just a placeholder for max
 */
enum dp_wfds_msg {
	DP_WFDS_REQ_MEM_IND_MSG,
	DP_WFDS_IPCC_MAP_N_CFG_IND_MSG,
	DP_WFDS_MSG_MAX,
};

/**
 * enum dp_wfds_state - Datapath WFDS state
 * @DP_WFDS_SVC_DISCONNECTED: service disconnected
 * @DP_WFDS_SVC_CONNECTED: service connected
 * @DP_WFDS_SVC_CONFIG_DONE: configuration msg handshake done
 * @DP_WFDS_SVC_MEM_CONFIG_DONE: memory handshake with server done
 * @DP_WFDS_SVC_IPCC_MAP_N_CFG_DONE: IPCC map and cfg handshake completed
 */
enum dp_wfds_state {
	DP_WFDS_SVC_DISCONNECTED,
	DP_WFDS_SVC_CONNECTED,
	DP_WFDS_SVC_CONFIG_DONE,
	DP_WFDS_SVC_MEM_CONFIG_DONE,
	DP_WFDS_SVC_IPCC_MAP_N_CFG_DONE,
};

/**
 * enum dp_wfds_event_type  - Datapath WFDS event type
 * @DP_WFDS_NEW_SERVER: QMI new server event
 * @DP_WFDS_MEM_REQ: QMI memory request event
 * @DP_WFDS_IPCC_MAP_N_CFG: QMI IPCC map and configure event
 */
enum dp_wfds_event_type {
	DP_WFDS_NEW_SERVER,
	DP_WFDS_MEM_REQ,
	DP_WFDS_IPCC_MAP_N_CFG,
};

/**
 * struct dp_wfds_event - DP QMI event structure
 * @list_node: node used for adding/deleting to a list
 * @wfds_evt_type: QMI event type
 * @data: Pointer to event data
 */
struct dp_wfds_event {
	qdf_list_node_t list_node;
	enum dp_wfds_event_type wfds_evt_type;
	void *data;
};

/**
 * struct dp_direct_link_iommu_config - Direct link related IOMMU configuration
 * @shadow_rdptr_paddr: shadow read pointer dma address
 * @shadow_rdptr_map_size: shadow read pointer memory size
 * @shadow_wrptr_paddr: shadow write pointer dma address
 * @shadow_wrptr_map_size: shadow write pointer memory size
 * @direct_link_srng_ring_base_paddr: SRNG ring base dma address
 * @direct_link_srng_ring_map_size: SRNG ring memory size
 * @direct_link_refill_ring_base_paddr: refill SRNG ring base dma address
 * @direct_link_refill_ring_map_size: refill SRNG ring memory size
 */
struct dp_direct_link_iommu_config {
	qdf_dma_addr_t shadow_rdptr_paddr;
	uint16_t shadow_rdptr_map_size;
	qdf_dma_addr_t shadow_wrptr_paddr;
	uint16_t shadow_wrptr_map_size;
	qdf_dma_addr_t direct_link_srng_ring_base_paddr[QMI_WFDS_CE_MAX_SRNG];
	uint16_t direct_link_srng_ring_map_size[QMI_WFDS_CE_MAX_SRNG];
	qdf_dma_addr_t direct_link_refill_ring_base_paddr;
	uint16_t direct_link_refill_ring_map_size;
};

/**
 * struct dp_direct_link_wfds_context - DP Direct Link WFDS context structure
 * @direct_link_ctx: direct link context
 * @wfds_work: work to be scheduled on QMI event
 * @wfds_wq: QMI workqueue
 * @wfds_event_list_lock: spinlock for event list access
 * @wfds_event_list: QMI event list
 * @wfds_state: QMI state
 * @num_mem_arenas: Number of memory arenas requested by QMI server
 * @mem_arena_pages: Pointer to array of mem multi page structure for arenas
 * @ipcc_dma_addr: ipcc dma address
 * @ipcc_ce_id: ids of CEs that are configured with IPCC MSI info
 * @ipcc_ce_id_len: number of valid entries in ipcc_ce_id array
 * @iommu_cfg: direct link iommu configuration
 */
struct dp_direct_link_wfds_context {
	struct dp_direct_link_context *direct_link_ctx;
	qdf_work_t wfds_work;
	qdf_workqueue_t *wfds_wq;
	qdf_spinlock_t wfds_event_list_lock;
	qdf_list_t wfds_event_list;
	qdf_atomic_t wfds_state;
	uint32_t num_mem_arenas;
	struct qdf_mem_multi_page_t *mem_arena_pages;
	uint32_t ipcc_dma_addr;
	uint8_t ipcc_ce_id[DP_WFDS_CE_MAX_SRNG];
	uint8_t ipcc_ce_id_len;
	struct dp_direct_link_iommu_config iommu_cfg;
};

/**
 * dp_wfds_handle_request_mem_ind() - Process request memory indication received
 *  from QMI server
 * @mem_msg: pointer to memory request indication message
 *
 * Return: None
 */
void
dp_wfds_handle_request_mem_ind(struct wlan_qmi_wfds_mem_ind_msg *mem_msg);

/**
 * dp_wfds_handle_ipcc_map_n_cfg_ind() - Process IPCC map and configure
 *  indication received from QMI server
 * @ipcc_msg: pointer to IPCC map and configure indication message
 *
 * Return: None
 */
void
dp_wfds_handle_ipcc_map_n_cfg_ind(struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg *ipcc_msg);

/**
 * dp_wfds_new_server() - New server callback triggered when service is up.
 *  Connect to the service as part of this call.
 *
 * Return: QDF status
 */
QDF_STATUS dp_wfds_new_server(void);

/**
 * dp_wfds_del_server() - Del server callback triggered when service is
 *  down.
 *
 * Return: None
 */
void dp_wfds_del_server(void);

/**
 * dp_wfds_init() - Initialize DP WFDS context
 * @direct_link_ctx: DP Direct Link context
 *
 * Return: QDF status
 */
QDF_STATUS dp_wfds_init(struct dp_direct_link_context *direct_link_ctx);

/**
 * dp_wfds_deinit() - Deinitialize DP WFDS context
 * @direct_link_ctx: DP Direct Link context
 * @is_ssr: true if SSR is in progress else false
 *
 * Return: None
 */
void dp_wfds_deinit(struct dp_direct_link_context *direct_link_ctx,
		    bool is_ssr);
#endif
#endif
