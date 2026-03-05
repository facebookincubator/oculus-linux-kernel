/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _SCMI_VENDOR_H
#define _SCMI_VENDOR_H
#include <linux/bitfield.h>
#include <linux/device.h>
#include <linux/types.h>

#define SCMI_PROTOCOL_BWPROF    0x8A
struct scmi_protocol_handle;
/**
 * struct scmi_bwprof_vendor_ops - represents the various operations provided
 *      by SCMI HW Bwprof Protocol
 */
struct scmi_bwprof_vendor_ops {
	int (*set_log_level)(const struct scmi_protocol_handle *ph, u8 val);
	int (*set_sample_ms)(const struct scmi_protocol_handle *ph,
			uint8_t  hist_enable, uint16_t ms_val);
	int (*set_masters_list)(const struct scmi_protocol_handle *ph,
			u8 cnt, u8 *master_list);
	int (*set_sampling_enable)(const struct scmi_protocol_handle *ph, u8 val);
	int (*set_hist_info)(const struct scmi_protocol_handle *ph,
			u32 *buckets_list);
};
#endif

