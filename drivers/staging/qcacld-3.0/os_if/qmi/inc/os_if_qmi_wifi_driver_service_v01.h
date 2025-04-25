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

#ifndef WIFI_DRIVER_SERVICE_V01_H
#define WIFI_DRIVER_SERVICE_V01_H

#include <linux/soc/qcom/qmi.h>

#define WFDS_SERVICE_ID_V01 0x043C
#define WFDS_SERVICE_VERS_V01 0x01

#define QMI_WFDS_IPCC_MAP_N_CFG_RESP_V01 0x0003
#define QMI_WFDS_UT_CMD_RESP_V01 0x0005
#define QMI_WFDS_MISC_REQ_V01 0x0004
#define QMI_WFDS_MISC_RESP_V01 0x0004
#define QMI_WFDS_MEM_RESP_V01 0x0002
#define QMI_WFDS_IPCC_MAP_N_CFG_REQ_V01 0x0003
#define QMI_WFDS_MISC_IND_V01 0x0004
#define QMI_WFDS_UT_CMD_REQ_V01 0x0005
#define QMI_WFDS_CONFIG_REQ_V01 0x0001
#define QMI_WFDS_IPCC_MAP_N_CFG_IND_V01 0x0003
#define QMI_WFDS_CONFIG_RESP_V01 0x0001
#define QMI_WFDS_MEM_REQ_V01 0x0002
#define QMI_WFDS_MEM_IND_V01 0x0002

#define WFDS_CE_MAX_SRNG_V01 3
#define WFDS_MEM_ARENA_MAX_V01 8
#define WFDS_PAGE_INFO_MAX_ARRAY_SIZE_V01 255

/**
 * struct wfds_gen_resp_msg_v01 - Generic QMI response message
 * @resp: QMI result code
 */
struct wfds_gen_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define WFDS_GEN_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info wfds_gen_resp_msg_v01_ei[];

/**
 * enum wifi_drv_qmi_srng_direction_v01 - SRNG direction
 * @WIFI_DRV_QMI_SRNG_DIRECTION_MIN_VAL_V01: SRNG direction enum min value
 * @WFDS_SRNG_SOURCE_RING_V01: SRNG source ring
 * @WFDS_SRNG_DESTINATION_RING_V01: SRNG destination ring
 * @WIFI_DRV_QMI_SRNG_DIRECTION_MAX_VAL_V01: SRNG direction enum max value
 */
enum wifi_drv_qmi_srng_direction_v01 {
	WIFI_DRV_QMI_SRNG_DIRECTION_MIN_VAL_V01 = INT_MIN,
	WFDS_SRNG_SOURCE_RING_V01 = 0,
	WFDS_SRNG_DESTINATION_RING_V01 = 1,
	WIFI_DRV_QMI_SRNG_DIRECTION_MAX_VAL_V01 = INT_MAX,
};

/**
 * struct wifi_drv_qmi_srng_information_v01 - SRNG information
 * @ring_id: SRNG ring id
 * @dir: SRNG direction
 * @num_entries: number of entries in SRNG
 * @entry_size: size of SRNG descriptor
 * @ring_base_paddr: ring base physical address of SRNG
 * @hp_paddr: HP physical address of SRNG
 * @tp_paddr: TP physical address of SRNG
 */
struct wifi_drv_qmi_srng_information_v01 {
	u8 ring_id;
	enum wifi_drv_qmi_srng_direction_v01 dir;
	u32 num_entries;
	u32 entry_size;
	u64 ring_base_paddr;
	u64 hp_paddr;
	u64 tp_paddr;
};

/**
 * enum wifi_drv_qmi_pipe_dir_v01 - pipe direction
 * @WIFI_DRV_QMI_PIPE_DIR_MIN_VAL_V01: pipe direction enum min value
 * @WFDS_PIPEDIR_NONE_V01: none pipe direction
 * @WFDS_PIPEDIR_IN_V01: target to host pipe direction
 * @WFDS_PIPEDIR_OUT_V01:  host to target pipe direction
 * @WIFI_DRV_QMI_PIPE_DIR_MAX_VAL_V01: pipe direction enum max value
 */
enum wifi_drv_qmi_pipe_dir_v01 {
	WIFI_DRV_QMI_PIPE_DIR_MIN_VAL_V01 = INT_MIN,
	WFDS_PIPEDIR_NONE_V01 = 0,
	WFDS_PIPEDIR_IN_V01 = 1,
	WFDS_PIPEDIR_OUT_V01 = 2,
	WIFI_DRV_QMI_PIPE_DIR_MAX_VAL_V01 = INT_MAX,
};

/**
 * struct wifi_drv_qmi_ce_information_v01 - CE information
 * @ce_id: CE id
 * @ce_dir: CE direction
 * @srng_info: SRNG information
 */
struct wifi_drv_qmi_ce_information_v01 {
	u8 ce_id;
	enum wifi_drv_qmi_pipe_dir_v01 ce_dir;
	struct wifi_drv_qmi_srng_information_v01 srng_info;
};

/**
 * struct wfds_config_req_msg_v01 - WFDS config request message
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
struct wfds_config_req_msg_v01 {
	u32 ce_info_len;
	struct wifi_drv_qmi_ce_information_v01 ce_info[WFDS_CE_MAX_SRNG_V01];
	struct wifi_drv_qmi_srng_information_v01 rx_refill_ring;
	u64 shadow_rdptr_mem_paddr;
	u64 shadow_rdptr_mem_size;
	u64 shadow_wrptr_mem_paddr;
	u64 shadow_wrptr_mem_size;
	u32 rx_pkt_tlv_len;
	u32 rx_rbm;
	u64 pcie_bar_pa;
	u32 pci_slot;
	u32 lpass_ep_id;
};

#define WFDS_CONFIG_REQ_MSG_V01_MAX_MSG_LEN 253
extern struct qmi_elem_info wfds_config_req_msg_v01_ei[];

/**
 * enum wifi_drv_qmi_mem_arenas_v01 - Memory arena
 * @WIFI_DRV_QMI_MEM_ARENAS_MIN_VAL_V01: memory arena enum min value
 * @WFDS_MEM_ARENA_TX_BUFFERS_V01: TX buffers memory arena
 * @WFDS_MEM_ARENA_CE_TX_MSG_BUFFERS_V01: CE TX message buffers memory arena
 * @WFDS_MEM_ARENA_CE_RX_MSG_BUFFERS_V01: CE RX message buffers memory arena
 * @WIFI_DRV_QMI_MEM_ARENAS_MAX_VAL_V01: memory arena enum max value
 */
enum wifi_drv_qmi_mem_arenas_v01 {
	WIFI_DRV_QMI_MEM_ARENAS_MIN_VAL_V01 = INT_MIN,
	WFDS_MEM_ARENA_TX_BUFFERS_V01 = 0,
	WFDS_MEM_ARENA_CE_TX_MSG_BUFFERS_V01 = 1,
	WFDS_MEM_ARENA_CE_RX_MSG_BUFFERS_V01 = 2,
	WIFI_DRV_QMI_MEM_ARENAS_MAX_VAL_V01 = INT_MAX,
};

/**
 * struct wifi_drv_qmi_mem_arena_information_v01 - Memory arena information
 * @entry_size: entry size
 * @num_entries: total number of entries required
 */
struct wifi_drv_qmi_mem_arena_information_v01 {
	u16 entry_size;
	u16 num_entries;
};

/**
 * struct wfds_mem_ind_msg_v01 - Memory indication message
 * @mem_arena_info_len: number of valid entries in mem_arena_info array
 * @mem_arena_info: memory arena information array
 */
struct wfds_mem_ind_msg_v01 {
	u32 mem_arena_info_len;
	struct wifi_drv_qmi_mem_arena_information_v01 mem_arena_info[WFDS_MEM_ARENA_MAX_V01];
};

#define WFDS_MEM_IND_MSG_V01_MAX_MSG_LEN 36
extern struct qmi_elem_info wfds_mem_ind_msg_v01_ei[];

/**
 * struct wifi_drv_qmi_mem_arena_page_information_v01 - Memory arena
 *  page information
 * @num_entries_per_page: number of entries per page
 * @page_dma_addr_len: number of valid entries in page_dma_addr array
 * @page_dma_addr: page dma address array
 */
struct wifi_drv_qmi_mem_arena_page_information_v01 {
	u16 num_entries_per_page;
	u32 page_dma_addr_len;
	u64 page_dma_addr[WFDS_PAGE_INFO_MAX_ARRAY_SIZE_V01];
};

/**
 * struct wfds_mem_req_msg_v01 - Memory request message
 *  page information
 * @mem_arena_page_info_len: number of valid entries in
 *  mem_arena_page_info array
 * @mem_arena_page_info: memory arena information
 */
struct wfds_mem_req_msg_v01 {
	u32 mem_arena_page_info_len;
	struct wifi_drv_qmi_mem_arena_page_information_v01 mem_arena_page_info[WFDS_MEM_ARENA_MAX_V01];
};

#define WFDS_MEM_REQ_MSG_V01_MAX_MSG_LEN 16348
extern struct qmi_elem_info wfds_mem_req_msg_v01_ei[];

/**
 * struct wifi_drv_qmi_ipcc_information_v01 - IPCC information
 * @ce_id: CE id
 * @ipcc_trig_addr: IPCC trigger address
 * @ipcc_trig_data: IPCC trigger data
 */
struct wifi_drv_qmi_ipcc_information_v01 {
	u8 ce_id;
	u64 ipcc_trig_addr;
	u32 ipcc_trig_data;
};

/**
 * struct wfds_ipcc_map_n_cfg_ind_msg_v01 - IPCC map and configure
 *  indication message
 * @ipcc_ce_info_len: number of valid entries in ipcc_ce_info array
 * @ipcc_ce_info: IPCC information for CE
 */
struct wfds_ipcc_map_n_cfg_ind_msg_v01 {
	u32 ipcc_ce_info_len;
	struct wifi_drv_qmi_ipcc_information_v01 ipcc_ce_info[WFDS_CE_MAX_SRNG_V01];
};

#define WFDS_IPCC_MAP_N_CFG_IND_MSG_V01_MAX_MSG_LEN 43
extern struct qmi_elem_info wfds_ipcc_map_n_cfg_ind_msg_v01_ei[];

/**
 * struct wfds_ipcc_map_n_cfg_req_msg_v01 - IPCC map and configure
 *  request message
 * @status: IPCC configuration status
 */
struct wfds_ipcc_map_n_cfg_req_msg_v01 {
	u8 status;
};

#define WFDS_IPCC_MAP_N_CFG_REQ_MSG_V01_MAX_MSG_LEN 4
extern struct qmi_elem_info wfds_ipcc_map_n_cfg_req_msg_v01_ei[];

/**
 * enum wifi_drv_qmi_event_v01 - driver event
 * @WIFI_DRV_QMI_EVENT_MIN_VAL_V01: event enum min value
 * @WFDS_EVENT_WLAN_HOST_RMMOD_V01: host driver rmmod event
 * @WFDS_EVENT_WLAN_SSR_V01: wlan SSR event
 * @WFDS_EVENT_LPASS_SSR_V01: LPASS SSR event
 * @WIFI_DRV_QMI_EVENT_MAX_VAL_V01: event enum max value
 */
enum wifi_drv_qmi_event_v01 {
	WIFI_DRV_QMI_EVENT_MIN_VAL_V01 = INT_MIN,
	WFDS_EVENT_WLAN_HOST_RMMOD_V01 = 0,
	WFDS_EVENT_WLAN_SSR_V01 = 1,
	WFDS_EVENT_LPASS_SSR_V01 = 2,
	WIFI_DRV_QMI_EVENT_MAX_VAL_V01 = INT_MAX,
};

/**
 * struct wfds_misc_req_msg_v01 - Miscellaneous request
 *  message
 * @event: driver event
 */
struct wfds_misc_req_msg_v01 {
	enum wifi_drv_qmi_event_v01 event;
};

#define WFDS_MISC_REQ_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info wfds_misc_req_msg_v01_ei[];

/**
 * struct wfds_misc_ind_msg_v01 - Miscellaneous indication
 *  message
 * @event: driver event
 */
struct wfds_misc_ind_msg_v01 {
	enum wifi_drv_qmi_event_v01 event;
};

#define WFDS_MISC_IND_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info wfds_misc_ind_msg_v01_ei[];

/**
 * enum wifi_drv_qmi_ut_cmd_v01 - driver event
 * @WIFI_DRV_QMI_UT_CMD_MIN_VAL_V01: event enum min value
 * @WFDS_UT_CMD_STOP_V01: Stop WFDS traffic
 * @WFDS_UT_CMD_START_V01: Start WFDS Traffic
 * @WFDS_UT_CMD_STATS_V01: Get WFDS traffic stats
 * @WFDS_UT_CMD_START_WHC_V01: Start WHC Traffic
 * @WFDS_UT_CMD_START_TSF_SYNC_V01: Start TSF handshake
 * @WFDS_UT_CMD_MISC_V01: Miscellaneous test
 * @WIFI_DRV_QMI_UT_CMD_MAX_VAL_V01: event enum max value
 */
enum wifi_drv_qmi_ut_cmd_v01 {
	WIFI_DRV_QMI_UT_CMD_MIN_VAL_V01 = INT_MIN,
	WFDS_UT_CMD_STOP_V01 = 0,
	WFDS_UT_CMD_START_V01 = 1,
	WFDS_UT_CMD_STATS_V01 = 2,
	WFDS_UT_CMD_START_WHC_V01 = 3,
	WFDS_UT_CMD_START_TSF_SYNC_V01 = 4,
	WFDS_UT_CMD_MISC_V01 = 5,
	WIFI_DRV_QMI_UT_CMD_MAX_VAL_V01 = INT_MAX,
};

/**
 * struct wfds_ut_cmd_req_msg_v01 - WFDS QMI UT cmd info structure
 * @cmd: Command type
 * @duration: Traffic duration
 * @flush_period: Buffer flushing periodicity
 * @num_pkts: Number of packets per flush
 * @buf_size: Buffer size
 * @ether_type: ether_type of packet
 * @src_mac: Source MAC address
 * @dest_mac: Destination MAC address
 * @ip_ver: IP address version
 * @src_ip_addr: source IP address
 * @dest_ip_addr: Destination IP address
 * @dest_port: Destination port
 * @misc: Misc input data
 */
struct wfds_ut_cmd_req_msg_v01 {
	enum wifi_drv_qmi_ut_cmd_v01 cmd;
	u32 duration;
	u32 flush_period;
	u32 num_pkts;
	u32 buf_size;
	u16 ether_type;
	u8 src_mac[6];
	u8 dest_mac[6];
	u8 ip_ver;
	u8 src_ip_addr[16];
	u8 dest_ip_addr[16];
	u16 dest_port;
	u8 misc[256];
};

#define WFDS_UT_CMD_REQ_MSG_V01_MAX_MSG_LEN 364
extern struct qmi_elem_info wfds_ut_cmd_req_msg_v01_ei[];

#endif
