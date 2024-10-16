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

/**
 * DOC: wlan_qmi_public_struct.h
 *
 * Contains QMI public data structure definitions.
 */

#ifndef _WLAN_QMI_PUBLIC_STRUCT_H_
#define _WLAN_QMI_PUBLIC_STRUCT_H_

#include "qdf_status.h"
#include <qdf_types.h>

#ifdef QMI_WFDS
#define QMI_WFDS_CE_MAX_SRNG 3
#define QMI_WFDS_MEM_ARENA_MAX 8
#define QMI_WFDS_PAGE_INFO_ARRAY_MAX_SIZE 255

/**
 * enum wlan_qmi_wfds_srng_dir - SRNG direction
 * @QMI_WFDS_SRNG_SOURCE_RING: SRNG source ring
 * @QMI_WFDS_SRNG_DESTINATION_RING: SRNG destination ring
 */
enum wlan_qmi_wfds_srng_dir {
	QMI_WFDS_SRNG_SOURCE_RING = 0,
	QMI_WFDS_SRNG_DESTINATION_RING = 1,
};

/**
 * struct wlan_qmi_wfds_srng_info - SRNG information
 * @ring_id: SRNG ring id
 * @dir: SRNG direction
 * @num_entries: number of entries in SRNG
 * @entry_size: size of SRNG descriptor
 * @ring_base_paddr: ring base physical address of SRNG
 * @hp_paddr: HP physical address of SRNG
 * @tp_paddr: TP physical address of SRNG
 */
struct wlan_qmi_wfds_srng_info {
	uint8_t ring_id;
	enum wlan_qmi_wfds_srng_dir dir;
	uint32_t num_entries;
	uint32_t entry_size;
	uint64_t ring_base_paddr;
	uint64_t hp_paddr;
	uint64_t tp_paddr;
};

/**
 * enum wlan_qmi_wfds_pipe_dir - pipe direction
 * @QMI_WFDS_PIPEDIR_NONE: none pipe direction
 * @QMI_WFDS_PIPEDIR_IN: target to host pipe direction
 * @QMI_WFDS_PIPEDIR_OUT:  host to target pipe direction
 */
enum wlan_qmi_wfds_pipe_dir {
	QMI_WFDS_PIPEDIR_NONE = 0,
	QMI_WFDS_PIPEDIR_IN = 1,
	QMI_WFDS_PIPEDIR_OUT = 2,
};

/**
 * struct wlan_qmi_wfds_ce_info - CE information
 * @ce_id: CE id
 * @ce_dir: CE direction
 * @srng_info: SRNG information
 */
struct wlan_qmi_wfds_ce_info {
	uint8_t ce_id;
	enum wlan_qmi_wfds_pipe_dir ce_dir;
	struct wlan_qmi_wfds_srng_info srng_info;
};

/**
 * struct wlan_qmi_wfds_config_req_msg - WFDS config request message
 * @ce_info_len: size of ce_info with valid entries
 * @ce_info: CE information array
 * @rx_refill_ring: refill SRNG information
 * @shadow_rdptr_mem_paddr: shadow read memory physical address
 * @shadow_rdptr_mem_size:  shadow read memory size
 * @shadow_wrptr_mem_paddr: shadow write memory physical address
 * @shadow_wrptr_mem_size: shadow write memory size
 * @rx_pkt_tlv_len: rx packet tlvs length
 * @rx_rbm: return buffer manager for rx buffers
 * @pcie_bar_pa: PCIe BAR physical address
 * @pci_slot: PCIe slot
 * @lpass_ep_id: LPASS data message service endpoint id
 */
struct wlan_qmi_wfds_config_req_msg {
	uint32_t ce_info_len;
	struct wlan_qmi_wfds_ce_info ce_info[QMI_WFDS_CE_MAX_SRNG];
	struct wlan_qmi_wfds_srng_info rx_refill_ring;
	uint64_t shadow_rdptr_mem_paddr;
	uint64_t shadow_rdptr_mem_size;
	uint64_t shadow_wrptr_mem_paddr;
	uint64_t shadow_wrptr_mem_size;
	uint32_t rx_pkt_tlv_len;
	uint32_t rx_rbm;
	uint64_t pcie_bar_pa;
	uint32_t pci_slot;
	uint32_t lpass_ep_id;
};

/**
 * enum wlan_qmi_wfds_mem_arenas - Memory arenas
 * @QMI_WFDS_MEM_ARENA_TX_BUFFERS: tx buffers memory arena
 * @QMI_WFDS_MEM_ARENA_CE_TX_MSG_BUFFERS: ce tx message buffers memory arena
 * @QMI_WFDS_MEM_ARENA_CE_RX_MSG_BUFFERS: ce rx message buffers memory arena
 */
enum wlan_qmi_wfds_mem_arenas {
	QMI_WFDS_MEM_ARENA_TX_BUFFERS = 0,
	QMI_WFDS_MEM_ARENA_CE_TX_MSG_BUFFERS = 1,
	QMI_WFDS_MEM_ARENA_CE_RX_MSG_BUFFERS = 2,
};

/**
 * struct wlan_qmi_wfds_mem_arena_info - Memory arena information
 * @entry_size: entry size
 * @num_entries: total number of entries required
 */
struct wlan_qmi_wfds_mem_arena_info {
	uint16_t entry_size;
	uint16_t num_entries;
};

/**
 * struct wlan_qmi_wfds_mem_ind_msg - Memory indication message
 * @mem_arena_info_len: number of valid entries in mem_arena_info array
 * @mem_arena_info: memory arena information array
 */
struct wlan_qmi_wfds_mem_ind_msg {
	uint32_t mem_arena_info_len;
	struct wlan_qmi_wfds_mem_arena_info mem_arena_info[QMI_WFDS_MEM_ARENA_MAX];
};

/**
 * struct wlan_qmi_wfds_mem_arena_page_info - Memory arena
 *  page information
 * @num_entries_per_page: number of entries per page
 * @page_dma_addr_len: number of valid entries in page_dma_addr array
 * @page_dma_addr: page dma address array
 */
struct wlan_qmi_wfds_mem_arena_page_info {
	uint16_t num_entries_per_page;
	uint32_t page_dma_addr_len;
	uint64_t page_dma_addr[QMI_WFDS_PAGE_INFO_ARRAY_MAX_SIZE];
};

/**
 * struct wlan_qmi_wfds_mem_req_msg - Memory request message
 *  page information
 * @mem_arena_page_info_len: number of valid entries in
 *  mem_arena_page_info array
 * @mem_arena_page_info: memory arena information
 */
struct wlan_qmi_wfds_mem_req_msg {
	uint32_t mem_arena_page_info_len;
	struct wlan_qmi_wfds_mem_arena_page_info mem_arena_page_info[QMI_WFDS_MEM_ARENA_MAX];
};

/**
 * struct wlan_qmi_wfds_ipcc_info - IPCC information
 * @ce_id: CE id
 * @ipcc_trig_addr: IPCC trigger address
 * @ipcc_trig_data: IPCC trigger data
 */
struct wlan_qmi_wfds_ipcc_info {
	uint8_t ce_id;
	uint64_t ipcc_trig_addr;
	uint32_t ipcc_trig_data;
};

/**
 * struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg - IPCC map and configure
 *  indication message
 * @ipcc_ce_info_len: number of valid entries in ipcc_ce_info array
 * @ipcc_ce_info: IPCC information for CE
 */
struct wlan_qmi_wfds_ipcc_map_n_cfg_ind_msg {
	uint32_t ipcc_ce_info_len;
	struct wlan_qmi_wfds_ipcc_info ipcc_ce_info[QMI_WFDS_CE_MAX_SRNG];
};

/**
 * enum wlan_qmi_wfds_status - status
 * @QMI_WFDS_STATUS_SUCCESS: success status
 * @QMI_WFDS_STATUS_FAILURE: failure status
 */
enum wlan_qmi_wfds_status {
	QMI_WFDS_STATUS_SUCCESS = 0,
	QMI_WFDS_STATUS_FAILURE = 1,
};

/**
 * struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg - IPCC map and configure
 *  request message
 * @status: IPCC configuration status
 */
struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg {
	enum wlan_qmi_wfds_status status;
};
#endif

/**
 * struct wlan_qmi_psoc_callbacks - struct containing callbacks
 *  to osif QMI APIs
 * @qmi_wfds_init: Callback to initialize WFDS QMI handle
 * @qmi_wfds_deinit: Callback to deinitialize WFDS QMI handle
 * @qmi_wfds_send_config_msg: Callback to send WFDS configuration message
 * @qmi_wfds_send_req_mem_msg: Callback to send WFDS request memory message
 * @qmi_wfds_send_ipcc_map_n_cfg_msg: Callback to send WFDS IPCC map and
 *  configure message
 * @qmi_wfds_send_misc_req_msg: Callback to send WFDS misc request message
 */
struct wlan_qmi_psoc_callbacks {
#ifdef QMI_WFDS
	QDF_STATUS (*qmi_wfds_init)(void);
	void (*qmi_wfds_deinit)(void);
	QDF_STATUS (*qmi_wfds_send_config_msg)(
			struct wlan_qmi_wfds_config_req_msg *src_info);
	QDF_STATUS (*qmi_wfds_send_req_mem_msg)(
			struct wlan_qmi_wfds_mem_req_msg *src_info);
	QDF_STATUS (*qmi_wfds_send_ipcc_map_n_cfg_msg)(
			struct wlan_qmi_wfds_ipcc_map_n_cfg_req_msg *src_info);
	QDF_STATUS (*qmi_wfds_send_misc_req_msg)(bool is_ssr);
#endif
};
#endif
