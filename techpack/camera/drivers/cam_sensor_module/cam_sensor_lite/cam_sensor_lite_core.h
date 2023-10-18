// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018,2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __CAM_SENSOR_CORE_H__
#define __CAM_SENSOR_CORE_H__

#include "cam_sensor_lite_dev.h"
#include "cam_sensor_lite_soc.h"

void cam_sensor_lite_shutdown(
		struct sensor_lite_device *sensor_lite_dev);

int __cam_sensor_lite_handle_start_dev(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_start_stop_dev_cmd *start);

int cam_sensor_lite_core_cfg(
		struct sensor_lite_device *sensor_lite_dev,
		void *arg);

int __dump_perframe_cmd(
	struct sensor_lite_perframe_cmd *pf_packet);

int sensor_lite_crm_intf_init(
	struct sensor_lite_device *sensor_lite_dev);

#endif
