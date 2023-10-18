// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __CAM_SENSOR_LITE_SOC_H__
#define __CAM_SENSOR_LITE_SOC_H__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include "cam_sensor_lite_dev.h"
#include "cam_sensor_lite_core.h"

int32_t cam_sensor_lite_parse_dt_info(struct platform_device *pdev,
	struct sensor_lite_device *sensor_lite_dev);

#endif
