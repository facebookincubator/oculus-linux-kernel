// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include "cam_isp_hw_mgr_intf.h"
#include "cam_ife_hw_mgr.h"
#include "cam_debug_util.h"


int cam_isp_hw_mgr_init(struct device_node *of_node,
	struct cam_hw_mgr_intf *hw_mgr, int *iommu_hdl)
{
	int rc = 0;
	const char *compat_str = NULL;

	rc = of_property_read_string_index(of_node, "arch-compat", 0,
		(const char **)&compat_str);

	if (strnstr(compat_str, "ife", strlen(compat_str)))
		rc = cam_ife_hw_mgr_init(hw_mgr, iommu_hdl);
	else {
		CAM_ERR(CAM_ISP, "Invalid ISP hw type");
		rc = -EINVAL;
	}

	return rc;
}
