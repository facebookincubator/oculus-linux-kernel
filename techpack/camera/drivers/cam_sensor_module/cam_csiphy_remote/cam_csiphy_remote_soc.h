/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __CAM_CSIPHY_REMOTE_SOC_H__
#define __CAM_CSIPHY_REMOTE_SOC_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include "cam_csiphy_remote_dev.h"
#include "cam_csiphy_remote_core.h"

#define CSIPHY_VERSION_V121                       0x121

/**
 * @csiphy_dev: Remote PHY device structure
 *
 * This API releases SOC related parameters
 */
int cam_csiphy_remote_soc_release(struct csiphy_remote_device *csiphy_dev);

/**
 * @pdev: Platform device
 * @csiphy_dev: Remote PHY device
 *
 * This API parses csiphy device tree information
 */
int32_t cam_csiphy_remote_parse_dt_info(struct platform_device *pdev,
	struct csiphy_remote_device *csiphy_dev);

#endif /* _CAM_CSIPHY_REMOTE_SOC_H_ */
