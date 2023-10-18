// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include "cam_vfe170.h"
#include "cam_vfe170_150.h"
#include "cam_vfe175.h"
#include "cam_vfe175_130.h"
#include "cam_vfe480.h"
#include "cam_vfe570.h"
#include "cam_vfe580.h"
#include "cam_vfe650.h"
#include "cam_vfe680.h"
#include "cam_vfe680_110.h"
#include "cam_vfe780.h"
#include "cam_vfe636.h"
#include "cam_vfe_lite17x.h"
#include "cam_vfe_lite48x.h"
#include "cam_vfe_lite65x.h"
#include "cam_vfe_lite63x.h"
#include "cam_vfe_lite68x.h"
#include "cam_vfe_lite78x.h"
#include "cam_vfe_hw_intf.h"
#include "cam_vfe_core.h"
#include "cam_vfe_dev.h"
#include "camera_main.h"

static const struct of_device_id cam_vfe_dt_match[] = {
	{
		.compatible = "qcom,vfe170",
		.data = &cam_vfe170_hw_info,
	},
	{
		.compatible = "qcom,vfe170_150",
		.data = &cam_vfe170_150_hw_info,
	},
	{
		.compatible = "qcom,vfe175",
		.data = &cam_vfe175_hw_info,
	},
	{
		.compatible = "qcom,vfe175_130",
		.data = &cam_vfe175_130_hw_info,
	},
	{
		.compatible = "qcom,vfe480",
		.data = &cam_vfe480_hw_info,
	},
	{
		.compatible = "qcom,vfe570",
		.data = &cam_vfe570_hw_info,
	},
	{
		.compatible = "qcom,vfe580",
		.data = &cam_vfe580_hw_info,
	},
	{
		.compatible = "qcom,vfe650",
		.data = &cam_vfe650_hw_info,
	},
	{
		.compatible = "qcom,vfe680",
		.data = &cam_vfe680_hw_info,
	},
	{
		.compatible = "qcom,vfe680_110",
		.data = &cam_vfe680_110_hw_info,
	},
	{
		.compatible = "qcom,vfe780",
		.data = &cam_vfe780_hw_info,
	},
	{
		.compatible = "qcom,vfe636",
		.data = &cam_vfe636_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite170",
		.data = &cam_vfe_lite17x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite175",
		.data = &cam_vfe_lite17x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite480",
		.data = &cam_vfe_lite48x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite570",
		.data = &cam_vfe_lite48x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite580",
		.data = &cam_vfe_lite48x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite650",
		.data = &cam_vfe_lite65x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite680",
		.data = &cam_vfe_lite68x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite680_110",
		.data = &cam_vfe_lite68x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite780",
		.data = &cam_vfe_lite78x_hw_info,
	},
	{
		.compatible = "qcom,vfe-lite636",
		.data = &cam_vfe_lite63x_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_vfe_dt_match);

struct platform_driver cam_vfe_driver = {
	.probe = cam_vfe_probe,
	.remove = cam_vfe_remove,
	.driver = {
		.name = "cam_vfe",
		.owner = THIS_MODULE,
		.of_match_table = cam_vfe_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_vfe_init_module(void)
{
	return platform_driver_register(&cam_vfe_driver);
}


void cam_vfe_exit_module(void)
{
	platform_driver_unregister(&cam_vfe_driver);
}

MODULE_DESCRIPTION("CAM VFE driver");
MODULE_LICENSE("GPL v2");
