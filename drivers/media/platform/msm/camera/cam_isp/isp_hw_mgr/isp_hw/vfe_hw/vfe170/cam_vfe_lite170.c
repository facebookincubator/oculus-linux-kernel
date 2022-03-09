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
#include "cam_vfe_lite170.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_core.h"
#include "cam_vfe_dev.h"

static const struct of_device_id cam_vfe170_dt_match[] = {
	{
		.compatible = "qcom,vfe-lite170",
		.data = &cam_vfe_lite170_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_vfe170_dt_match);

static struct platform_driver cam_vfe170_driver = {
	.probe = cam_vfe_probe,
	.remove = cam_vfe_remove,
	.driver = {
		.name = "cam_vfe_lite170",
		.owner = THIS_MODULE,
		.of_match_table = cam_vfe170_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_vfe170_init_module(void)
{
	return platform_driver_register(&cam_vfe170_driver);
}

static void __exit cam_vfe170_exit_module(void)
{
	platform_driver_unregister(&cam_vfe170_driver);
}

module_init(cam_vfe170_init_module);
module_exit(cam_vfe170_exit_module);
MODULE_DESCRIPTION("CAM VFE170 driver");
MODULE_LICENSE("GPL v2");
