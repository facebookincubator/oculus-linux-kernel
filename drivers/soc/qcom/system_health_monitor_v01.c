/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/qmi_encdec.h>

#include <soc/qcom/msm_qmi_interface.h>

#include "system_health_monitor_v01.h"

struct elem_info hmon_register_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct hmon_register_req_msg_v01,
					   name_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = 255,
		.elem_size      = sizeof(char),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct hmon_register_req_msg_v01,
					   name),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct hmon_register_req_msg_v01,
					   timeout_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct hmon_register_req_msg_v01,
					   timeout),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info hmon_register_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct hmon_register_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info hmon_health_check_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info hmon_health_check_complete_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
				struct hmon_health_check_complete_req_msg_v01,
				result_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum hmon_check_result_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
				struct hmon_health_check_complete_req_msg_v01,
				result),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info hmon_health_check_complete_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(
				struct hmon_health_check_complete_resp_msg_v01,
				resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};
