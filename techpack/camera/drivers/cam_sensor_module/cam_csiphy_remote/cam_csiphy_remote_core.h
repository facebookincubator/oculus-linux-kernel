/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_CSIPHY_REMOTE_CORE_H_
#define _CAM_CSIPHY_REMOTE_CORE_H_

#include <linux/irqreturn.h>
#include "cam_csiphy_remote_dev.h"
#include <cam_req_mgr_util.h>


/**
 * @csiphy_dev: Remote PHY device structure
 * @arg: Camera control command argument
 *
 * This API handles the camera control argument reached to CSIPhy
 */
int cam_csiphy_remote_core_cfg(void *csiphy_dev, void *arg);

/**
 * @csiphy_dev: Remote PHY device structure
 *
 * This API handles the CSIPhy close
 */
void cam_csiphy_remote_shutdown(struct csiphy_remote_device *csiphy_dev);

#endif /* _CAM_CSIPHY_REMOTE_CORE_H_ */
