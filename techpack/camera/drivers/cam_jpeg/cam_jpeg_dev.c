// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_node.h"
#include "cam_hw_mgr_intf.h"
#include "cam_jpeg_hw_mgr_intf.h"
#include "cam_jpeg_dev.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"

#define CAM_JPEG_DEV_NAME "cam-jpeg"

static struct cam_jpeg_dev g_jpeg_dev;

static void cam_jpeg_dev_iommu_fault_handler(
	struct iommu_domain *domain, struct device *dev, unsigned long iova,
	int flags, void *token, uint32_t buf_info)
{
	int i = 0;
	struct cam_node *node = NULL;

	if (!token) {
		CAM_ERR(CAM_JPEG, "invalid token in page handler cb");
		return;
	}

	node = (struct cam_node *)token;

	for (i = 0; i < node->ctx_size; i++)
		cam_context_dump_pf_info(&(node->ctx_list[i]), iova,
			buf_info);
}

static const struct of_device_id cam_jpeg_dt_match[] = {
	{
		.compatible = "qcom,cam-jpeg"
	},
	{ }
};

static int cam_jpeg_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{

	mutex_lock(&g_jpeg_dev.jpeg_mutex);
	g_jpeg_dev.open_cnt++;
	mutex_unlock(&g_jpeg_dev.jpeg_mutex);

	return 0;
}

static int cam_jpeg_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_node *node = v4l2_get_subdevdata(sd);


	mutex_lock(&g_jpeg_dev.jpeg_mutex);
	if (g_jpeg_dev.open_cnt <= 0) {
		CAM_DBG(CAM_JPEG, "JPEG subdev is already closed");
		rc = -EINVAL;
		goto end;
	}

	g_jpeg_dev.open_cnt--;

	if (!node) {
		CAM_ERR(CAM_JPEG, "Node ptr is NULL");
		rc = -EINVAL;
		goto end;
	}

	if (g_jpeg_dev.open_cnt == 0)
		cam_node_shutdown(node);

end:
	mutex_unlock(&g_jpeg_dev.jpeg_mutex);
	return rc;
}

static const struct v4l2_subdev_internal_ops cam_jpeg_subdev_internal_ops = {
	.close = cam_jpeg_subdev_close,
	.open = cam_jpeg_subdev_open,
};

static int cam_jpeg_dev_remove(struct platform_device *pdev)
{
	int rc;
	int i;

	for (i = 0; i < CAM_JPEG_CTX_MAX; i++) {
		rc = cam_jpeg_context_deinit(&g_jpeg_dev.ctx_jpeg[i]);
		if (rc)
			CAM_ERR(CAM_JPEG, "JPEG context %d deinit failed %d",
				i, rc);
	}

	rc = cam_subdev_remove(&g_jpeg_dev.sd);
	if (rc)
		CAM_ERR(CAM_JPEG, "Unregister failed %d", rc);

	return rc;
}

static int cam_jpeg_dev_probe(struct platform_device *pdev)
{
	int rc;
	int i;
	struct cam_hw_mgr_intf hw_mgr_intf;
	struct cam_node *node;
	int iommu_hdl = -1;

	g_jpeg_dev.sd.internal_ops = &cam_jpeg_subdev_internal_ops;
	rc = cam_subdev_probe(&g_jpeg_dev.sd, pdev, CAM_JPEG_DEV_NAME,
		CAM_JPEG_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_JPEG, "JPEG cam_subdev_probe failed %d", rc);
		goto err;
	}
	node = (struct cam_node *)g_jpeg_dev.sd.token;

	rc = cam_jpeg_hw_mgr_init(pdev->dev.of_node,
		(uint64_t *)&hw_mgr_intf, &iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_JPEG, "Can not initialize JPEG HWmanager %d", rc);
		goto unregister;
	}

	for (i = 0; i < CAM_JPEG_CTX_MAX; i++) {
		rc = cam_jpeg_context_init(&g_jpeg_dev.ctx_jpeg[i],
			&g_jpeg_dev.ctx[i],
			&node->hw_mgr_intf,
			i);
		if (rc) {
			CAM_ERR(CAM_JPEG, "JPEG context init failed %d %d",
				i, rc);
			goto ctx_init_fail;
		}
	}

	rc = cam_node_init(node, &hw_mgr_intf, g_jpeg_dev.ctx, CAM_JPEG_CTX_MAX,
		CAM_JPEG_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_JPEG, "JPEG node init failed %d", rc);
		goto ctx_init_fail;
	}

	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_jpeg_dev_iommu_fault_handler, node);

	mutex_init(&g_jpeg_dev.jpeg_mutex);

	CAM_INFO(CAM_JPEG, "Camera JPEG probe complete");

	return rc;

ctx_init_fail:
	for (--i; i >= 0; i--)
		if (cam_jpeg_context_deinit(&g_jpeg_dev.ctx_jpeg[i]))
			CAM_ERR(CAM_JPEG, "deinit fail %d %d", i, rc);
unregister:
	if (cam_subdev_remove(&g_jpeg_dev.sd))
		CAM_ERR(CAM_JPEG, "remove fail %d", rc);
err:
	return rc;
}

static struct platform_driver jpeg_driver = {
	.probe = cam_jpeg_dev_probe,
	.remove = cam_jpeg_dev_remove,
	.driver = {
		.name = "cam_jpeg",
		.owner = THIS_MODULE,
		.of_match_table = cam_jpeg_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_jpeg_dev_init_module(void)
{
	return platform_driver_register(&jpeg_driver);
}

static void __exit cam_jpeg_dev_exit_module(void)
{
	platform_driver_unregister(&jpeg_driver);
}

module_init(cam_jpeg_dev_init_module);
module_exit(cam_jpeg_dev_exit_module);
MODULE_DESCRIPTION("MSM JPEG driver");
MODULE_LICENSE("GPL v2");
