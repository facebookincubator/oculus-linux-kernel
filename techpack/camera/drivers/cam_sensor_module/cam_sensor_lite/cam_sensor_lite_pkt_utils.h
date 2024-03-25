// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __CAM_SENSOR_LITE_PKT_UTILS_H__
#define __CAM_SENSOR_LITE_PKT_UTILS_H__

#include <linux/module.h>
#include "cam_debug_util.h"
#include "cam_sensor_lite_dev.h"
#include "cam_rpmsg.h"
#include <cam_sensor_lite_ext_headers.h>

int __dump_probe_cmd(
	struct probe_payload_v2 *probe);

int __dump_probe_cmd_hex(
	struct probe_payload_v2 *probe);

int __dump_host_dest_init_cmd(
	struct host_dest_camera_init_payload_v2 *init);

int __dump_slave_dest_init_cmd(
	struct slave_dest_camera_init_payload *init);

int __copy_pwr_settings(
	void     *dst_ptr,
	void     *base,
	off_t    b_offset,
	uint32_t count,
	ssize_t  max_size);

int __set_slave_pkt_headers(
	struct sensor_lite_header *header,
	uint32_t                   opcode);

int __dump_acquire_cmd(
	struct sensor_lite_acquire_cmd *acquire);

int __dump_release_cmd(
	struct sensor_lite_release_cmd *release);

void __dump_slave_pkt_headers(
		struct sensor_lite_header *header);

void __dump_remote_flush_cmd(
		struct sensorlite_sys_cmd *cmd);

int __send_pkt(
	struct sensor_lite_device *sensor_lite_dev,
	struct sensor_lite_header *header);

int __send_probe_pkt(
	struct sensor_lite_device *sensor_lite_dev,
	struct sensor_lite_header *header);

#endif
