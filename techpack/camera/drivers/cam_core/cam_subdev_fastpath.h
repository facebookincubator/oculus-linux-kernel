/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SUBDEV_FASTPATH_H_
#define _CAM_SUBDEV_FASTPATH_H_

#include "cam_subdev.h"

int cam_subdev_fastpath_probe(struct cam_subdev *sd,
			      struct platform_device *pdev,
			      char *name, uint32_t dev_type);
int cam_subdev_fastpath_remove(struct cam_subdev *sd);

#endif /* _CAM_SUBDEV_FASTPATH_H_ */
