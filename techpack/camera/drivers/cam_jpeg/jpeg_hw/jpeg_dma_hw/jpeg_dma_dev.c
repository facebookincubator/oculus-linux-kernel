// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/timer.h>

#include "jpeg_dma_core.h"
#include "jpeg_dma_soc.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_io_util.h"
#include "cam_jpeg_hw_intf.h"
#include "cam_jpeg_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"

static struct cam_jpeg_dma_device_hw_info cam_jpeg_dma_hw_info = {
	.reserved = 0,
};
EXPORT_SYMBOL(cam_jpeg_dma_hw_info);

static int cam_jpeg_dma_register_cpas(struct cam_hw_soc_info *soc_info,
	struct cam_jpeg_dma_device_core_info *core_info,
	uint32_t hw_idx)
{
	struct cam_cpas_register_params cpas_register_params;
	int rc;

	cpas_register_params.dev = soc_info->dev;
	memcpy(cpas_register_params.identifier, "jpeg-dma",
		sizeof("jpeg-dma"));
	cpas_register_params.cam_cpas_client_cb = NULL;
	cpas_register_params.cell_index = hw_idx;
	cpas_register_params.userdata = NULL;

	rc = cam_cpas_register_client(&cpas_register_params);
	if (rc) {
		CAM_ERR(CAM_JPEG, "cpas_register failed: %d", rc);
		return rc;
	}
	core_info->cpas_handle = cpas_register_params.client_handle;

	return rc;
}

static int cam_jpeg_dma_unregister_cpas(
	struct cam_jpeg_dma_device_core_info *core_info)
{
	int rc;

	rc = cam_cpas_unregister_client(core_info->cpas_handle);
	if (rc)
		CAM_ERR(CAM_JPEG, "cpas unregister failed: %d", rc);
	core_info->cpas_handle = 0;

	return rc;
}

static int cam_jpeg_dma_remove(struct platform_device *pdev)
{
	struct cam_hw_info *jpeg_dma_dev = NULL;
	struct cam_hw_intf *jpeg_dma_dev_intf = NULL;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	int rc;

	jpeg_dma_dev_intf = platform_get_drvdata(pdev);
	if (!jpeg_dma_dev_intf) {
		CAM_ERR(CAM_JPEG, "error No data in pdev");
		return -EINVAL;
	}

	jpeg_dma_dev = jpeg_dma_dev_intf->hw_priv;
	if (!jpeg_dma_dev) {
		CAM_ERR(CAM_JPEG, "error HW data is NULL");
		rc = -ENODEV;
		goto free_jpeg_hw_intf;
	}

	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;
	if (!core_info) {
		CAM_ERR(CAM_JPEG, "error core data NULL");
		goto deinit_soc;
	}

	rc = cam_jpeg_dma_unregister_cpas(core_info);
	if (rc)
		CAM_ERR(CAM_JPEG, " unreg failed to reg cpas %d", rc);

	mutex_destroy(&core_info->core_mutex);
	kfree(core_info);

deinit_soc:
	rc = cam_soc_util_release_platform_resource(&jpeg_dma_dev->soc_info);
	if (rc)
		CAM_ERR(CAM_JPEG, "Failed to deinit soc rc=%d", rc);

	mutex_destroy(&jpeg_dma_dev->hw_mutex);
	kfree(jpeg_dma_dev);

free_jpeg_hw_intf:
	kfree(jpeg_dma_dev_intf);
	return rc;
}

static int cam_jpeg_dma_probe(struct platform_device *pdev)
{
	struct cam_hw_info *jpeg_dma_dev = NULL;
	struct cam_hw_intf *jpeg_dma_dev_intf = NULL;
	const struct of_device_id *match_dev = NULL;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	struct cam_jpeg_dma_device_hw_info *hw_info = NULL;
	int rc;

	jpeg_dma_dev_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!jpeg_dma_dev_intf)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &jpeg_dma_dev_intf->hw_idx);

	jpeg_dma_dev = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!jpeg_dma_dev) {
		rc = -ENOMEM;
		goto error_alloc_dev;
	}
	jpeg_dma_dev->soc_info.pdev = pdev;
	jpeg_dma_dev->soc_info.dev = &pdev->dev;
	jpeg_dma_dev->soc_info.dev_name = pdev->name;
	jpeg_dma_dev_intf->hw_priv = jpeg_dma_dev;
	jpeg_dma_dev_intf->hw_ops.init = cam_jpeg_dma_init_hw;
	jpeg_dma_dev_intf->hw_ops.deinit = cam_jpeg_dma_deinit_hw;
	jpeg_dma_dev_intf->hw_ops.process_cmd = cam_jpeg_dma_process_cmd;
	jpeg_dma_dev_intf->hw_type = CAM_JPEG_DEV_DMA;

	platform_set_drvdata(pdev, jpeg_dma_dev_intf);
	jpeg_dma_dev->core_info =
		kzalloc(sizeof(struct cam_jpeg_dma_device_core_info),
			GFP_KERNEL);
	if (!jpeg_dma_dev->core_info) {
		rc = -ENOMEM;
		goto error_alloc_core;
	}
	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_JPEG, " No jpeg_dma hardware info");
		rc = -EINVAL;
		goto error_match_dev;
	}
	hw_info = (struct cam_jpeg_dma_device_hw_info *)match_dev->data;
	core_info->jpeg_dma_hw_info = hw_info;
	core_info->core_state = CAM_JPEG_DMA_CORE_NOT_READY;
	mutex_init(&core_info->core_mutex);

	rc = cam_jpeg_dma_init_soc_resources(&jpeg_dma_dev->soc_info,
		cam_jpeg_dma_irq,
		jpeg_dma_dev);
	if (rc) {
		CAM_ERR(CAM_JPEG, "failed to init_soc %d", rc);
		goto error_init_soc;
	}

	rc = cam_jpeg_dma_register_cpas(&jpeg_dma_dev->soc_info,
		core_info, jpeg_dma_dev_intf->hw_idx);
	if (rc) {
		CAM_ERR(CAM_JPEG, " failed to reg cpas %d", rc);
		goto error_reg_cpas;
	}
	jpeg_dma_dev->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&jpeg_dma_dev->hw_mutex);
	spin_lock_init(&jpeg_dma_dev->hw_lock);
	init_completion(&jpeg_dma_dev->hw_complete);

	CAM_DBG(CAM_JPEG, " hwidx %d", jpeg_dma_dev_intf->hw_idx);

	return rc;

error_reg_cpas:
	rc = cam_soc_util_release_platform_resource(&jpeg_dma_dev->soc_info);
error_init_soc:
	mutex_destroy(&core_info->core_mutex);
error_match_dev:
	kfree(jpeg_dma_dev->core_info);
error_alloc_core:
	kfree(jpeg_dma_dev);
error_alloc_dev:
	kfree(jpeg_dma_dev_intf);
	return rc;
}

static const struct of_device_id cam_jpeg_dma_dt_match[] = {
	{
		.compatible = "qcom,cam_jpeg_dma",
		.data = &cam_jpeg_dma_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_jpeg_dma_dt_match);

static struct platform_driver cam_jpeg_dma_driver = {
	.probe = cam_jpeg_dma_probe,
	.remove = cam_jpeg_dma_remove,
	.driver = {
		.name = "cam-jpeg-dma",
		.owner = THIS_MODULE,
		.of_match_table = cam_jpeg_dma_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_jpeg_dma_init_module(void)
{
	return platform_driver_register(&cam_jpeg_dma_driver);
}

static void __exit cam_jpeg_dma_exit_module(void)
{
	platform_driver_unregister(&cam_jpeg_dma_driver);
}

module_init(cam_jpeg_dma_init_module);
module_exit(cam_jpeg_dma_exit_module);
MODULE_DESCRIPTION("CAM JPEG_DMA driver");
MODULE_LICENSE("GPL v2");
