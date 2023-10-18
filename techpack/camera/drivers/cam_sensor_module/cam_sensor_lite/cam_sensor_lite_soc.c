// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */


#include "cam_sensor_lite_dev.h"
#include "cam_sensor_lite_core.h"
#include "cam_sensor_lite_soc.h"

int32_t cam_sensor_lite_parse_dt_info(struct platform_device *pdev,
	struct sensor_lite_device *sensor_lite_dev)
{
	int32_t   rc = 0;
	struct cam_hw_soc_info *soc_info;
	struct device_node     *of_node = NULL;

	if (!sensor_lite_dev)
		return -EINVAL;

	soc_info = &sensor_lite_dev->soc_info;

	if (!soc_info->dev)
		return -EINVAL;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_LITE, "parsing common soc dt(rc %d)", rc);
		return  rc;
	}

	of_node = soc_info->dev->of_node;

	rc = of_property_read_u32(of_node, "phy-id", &sensor_lite_dev->phy_id);
	if (rc) {
		CAM_ERR(CAM_SENSOR_LITE, "device %s failed to read phy-id",
			soc_info->dev_name);
		return rc;
	}

	if (of_property_read_bool(of_node, "hw_no_ops"))
		sensor_lite_dev->hw_no_ops = true;

	return rc;
}
