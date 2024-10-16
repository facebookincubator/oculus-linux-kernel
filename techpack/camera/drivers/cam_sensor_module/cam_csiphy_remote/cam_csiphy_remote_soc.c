// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_csiphy_remote_soc.h"
#include "cam_csiphy_remote_core.h"
#include "include/cam_csiphy_remote_1_2_1_hwreg.h"

int32_t cam_csiphy_remote_parse_dt_info(struct platform_device *pdev,
	struct csiphy_remote_device *csiphy_dev)
{
	int32_t   rc = 0;
	struct cam_hw_soc_info   *soc_info;

	soc_info = &csiphy_dev->soc_info;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY_REMOTE, "DT parse error: %d", rc);
		return  rc;
	}

	if (of_device_is_compatible(soc_info->dev->of_node,
		"qcom,csiphy-remote-v1.2.1")) {
		csiphy_dev->ctrl_reg->csiphy_2ph_reg = csiphy_remote_2ph_v1_2_1_reg;
		csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg =
			csiphy_remote_2ph_v1_2_1_combo_mode_reg;
		csiphy_dev->ctrl_reg->csiphy_3ph_reg = csiphy_remote_3ph_v1_2_1_reg;
		csiphy_dev->ctrl_reg->csiphy_common_reg = csiphy_remote_common_reg_1_2_1;
		csiphy_dev->ctrl_reg->csiphy_reset_regs = csiphy_remote_reset_reg_1_2_1;
		csiphy_dev->ctrl_reg->csiphy_reg = csiphy_remote_v1_2_1;
		csiphy_dev->hw_version = CSIPHY_VERSION_V121;
	} else {
		CAM_ERR(CAM_CSIPHY_REMOTE, "invalid hw version: 0x%x",
			csiphy_dev->hw_version);
		rc =  -EINVAL;
		return rc;
	}

	rc = cam_soc_util_request_platform_resource(&csiphy_dev->soc_info,
		NULL, csiphy_dev);

	return rc;
}

int32_t cam_csiphy_remote_soc_release(struct csiphy_remote_device *csiphy_dev)
{
	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "csiphy dev NULL");
		return 0;
	}

	cam_soc_util_release_platform_resource(&csiphy_dev->soc_info);

	return 0;
}
