/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include "cam_ife_csid_lite170.h"
#include "cam_ife_csid_core.h"
#include "cam_ife_csid_dev.h"

#define CAM_CSID_LITE_DRV_NAME                    "csid_lite_170"
#define CAM_CSID_LITE_VERSION_V170                 0x10070000

static struct cam_ife_csid_hw_info cam_ife_csid_lite170_hw_info = {
	.csid_reg = &cam_ife_csid_lite_170_reg_offset,
	.hw_dts_version = CAM_CSID_LITE_VERSION_V170,
};

static const struct of_device_id cam_ife_csid_lite170_dt_match[] = {
	{
		.compatible = "qcom,csid-lite170",
		.data = &cam_ife_csid_lite170_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_ife_csid_lite170_dt_match);

static struct platform_driver cam_ife_csid_lite170_driver = {
	.probe = cam_ife_csid_probe,
	.remove = cam_ife_csid_remove,
	.driver = {
		.name = CAM_CSID_LITE_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_ife_csid_lite170_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_ife_csid_lite170_init_module(void)
{
	return platform_driver_register(&cam_ife_csid_lite170_driver);
}

static void __exit cam_ife_csid_lite170_exit_module(void)
{
	platform_driver_unregister(&cam_ife_csid_lite170_driver);
}

module_init(cam_ife_csid_lite170_init_module);
module_exit(cam_ife_csid_lite170_exit_module);
MODULE_DESCRIPTION("CAM IFE_CSID_LITE170 driver");
MODULE_LICENSE("GPL v2");
