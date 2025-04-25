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

#include "os_if_qmi_wifi_driver_service_v01.h"
#include <linux/module.h>

struct qmi_elem_info wfds_gen_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wfds_gen_resp_msg_v01,
					   resp),
		.ei_array       = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wifi_drv_qmi_srng_information_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_srng_information_v01,
					   ring_id),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wifi_drv_qmi_srng_direction_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_srng_information_v01,
					   dir),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_srng_information_v01,
					   num_entries),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_srng_information_v01,
					   entry_size),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_srng_information_v01,
					   ring_base_paddr),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_srng_information_v01,
					   hp_paddr),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_srng_information_v01,
					   tp_paddr),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wifi_drv_qmi_ce_information_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_ce_information_v01,
					   ce_id),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wifi_drv_qmi_pipe_dir_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_ce_information_v01,
					   ce_dir),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wifi_drv_qmi_srng_information_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_ce_information_v01,
					   srng_info),
		.ei_array       = wifi_drv_qmi_srng_information_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_config_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   ce_info_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = WFDS_CE_MAX_SRNG_V01,
		.elem_size      = sizeof(struct wifi_drv_qmi_ce_information_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   ce_info),
		.ei_array       = wifi_drv_qmi_ce_information_v01_ei,
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wifi_drv_qmi_srng_information_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   rx_refill_ring),
		.ei_array       = wifi_drv_qmi_srng_information_v01_ei,
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   shadow_rdptr_mem_paddr),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x04,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   shadow_rdptr_mem_size),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x05,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   shadow_wrptr_mem_paddr),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x06,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   shadow_wrptr_mem_size),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x07,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   rx_pkt_tlv_len),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x08,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   rx_rbm),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x09,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   pcie_bar_pa),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x0A,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   pci_slot),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x0B,
		.offset         = offsetof(struct wfds_config_req_msg_v01,
					   lpass_ep_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wifi_drv_qmi_mem_arena_information_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_mem_arena_information_v01,
					   entry_size),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_mem_arena_information_v01,
					   num_entries),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_mem_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_mem_ind_msg_v01,
					   mem_arena_info_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = WFDS_MEM_ARENA_MAX_V01,
		.elem_size      = sizeof(struct wifi_drv_qmi_mem_arena_information_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_mem_ind_msg_v01,
					   mem_arena_info),
		.ei_array       = wifi_drv_qmi_mem_arena_information_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wifi_drv_qmi_mem_arena_page_information_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_mem_arena_page_information_v01,
					   num_entries_per_page),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_mem_arena_page_information_v01,
					   page_dma_addr_len),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = WFDS_PAGE_INFO_MAX_ARRAY_SIZE_V01,
		.elem_size      = sizeof(u64),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_mem_arena_page_information_v01,
					   page_dma_addr),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_mem_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_mem_req_msg_v01,
					   mem_arena_page_info_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = WFDS_MEM_ARENA_MAX_V01,
		.elem_size      = sizeof(struct wifi_drv_qmi_mem_arena_page_information_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_mem_req_msg_v01,
					   mem_arena_page_info),
		.ei_array       = wifi_drv_qmi_mem_arena_page_information_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wifi_drv_qmi_ipcc_information_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_ipcc_information_v01,
					   ce_id),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_ipcc_information_v01,
					   ipcc_trig_addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wifi_drv_qmi_ipcc_information_v01,
					   ipcc_trig_data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_ipcc_map_n_cfg_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_ipcc_map_n_cfg_ind_msg_v01,
					   ipcc_ce_info_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = WFDS_CE_MAX_SRNG_V01,
		.elem_size      = sizeof(struct wifi_drv_qmi_ipcc_information_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_ipcc_map_n_cfg_ind_msg_v01,
					   ipcc_ce_info),
		.ei_array       = wifi_drv_qmi_ipcc_information_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_ipcc_map_n_cfg_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_ipcc_map_n_cfg_req_msg_v01,
					   status),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_misc_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wifi_drv_qmi_event_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_misc_req_msg_v01,
					   event),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_misc_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wifi_drv_qmi_event_v01),
		.array_type     = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wfds_misc_ind_msg_v01,
					   event),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type     = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wfds_ut_cmd_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wifi_drv_qmi_ut_cmd_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   cmd),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   duration),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   flush_period),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x04,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   num_pkts),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x05,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   buf_size),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x06,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   ether_type),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 6,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x07,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   src_mac),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 6,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x08,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   dest_mac),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x09,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   ip_ver),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 16,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x0A,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   src_ip_addr),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 16,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x0B,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   dest_ip_addr),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x0C,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   dest_port),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 256,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x0D,
		.offset         = offsetof(struct
					   wfds_ut_cmd_req_msg_v01,
					   misc),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
