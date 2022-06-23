// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_req_mgr.h>
#include <media/cam_defs.h>
#include <media/cam_icp.h>
#include "cam_req_mgr_dev.h"
#include "cam_subdev.h"
#include "cam_node.h"
#include "cam_context.h"
#include "cam_icp_context.h"
#include "cam_hw_mgr_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"
#include "cam_icp_context_fastpath.h"
#include "cam_node_fastpath.h"
#include "cam_subdev_fastpath.h"

#define FP_CAM_ICP_CTX_MAX 6

#define CAM_ICP_DEV_NAME        "cam-icp"
#define CAM_ICP_FP_DEV_NAME     "cam-icp-fastpath"

struct cam_icp_subdev {
	struct cam_subdev sd;
	struct cam_node *node;
	struct cam_context ctx[CAM_ICP_CTX_MAX];
	struct cam_icp_context ctx_icp[CAM_ICP_CTX_MAX];
	struct mutex icp_lock;
	int32_t open_cnt;
	int32_t reserved;
};

struct cam_icp_subdev_fastpath {
	struct  cam_subdev sd;
	void    *ctx[FP_CAM_ICP_CTX_MAX];
	struct  mutex mutex;
	int32_t open_cnt;
};

static struct cam_icp_subdev g_icp_dev;
static struct cam_icp_subdev_fastpath g_icp_dev_fastpath;

static const struct of_device_id cam_icp_dt_match[] = {
	{.compatible = "qcom,cam-icp"},
	{}
};

static void cam_icp_dev_iommu_fault_handler(
	struct iommu_domain *domain, struct device *dev, unsigned long iova,
	int flags, void *token, uint32_t buf_info)
{
	int i = 0;
	struct cam_node *node = NULL;

	if (!token) {
		CAM_ERR(CAM_ICP, "invalid token in page handler cb");
		return;
	}

	node = (struct cam_node *)token;

	for (i = 0; i < node->ctx_size; i++)
		cam_context_dump_pf_info(&(node->ctx_list[i]), iova,
			buf_info);
}

static int cam_icp_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_hw_mgr_intf *hw_mgr_intf = NULL;
	struct cam_node *node = v4l2_get_subdevdata(sd);
	int rc = 0;

	mutex_lock(&g_icp_dev.icp_lock);
	if (g_icp_dev.open_cnt >= 1) {
		CAM_ERR(CAM_ICP, "ICP subdev is already opened");
		rc = -EALREADY;
		goto end;
	}

	if (!node) {
		CAM_ERR(CAM_ICP, "Invalid args");
		rc = -EINVAL;
		goto end;
	}

	hw_mgr_intf = &node->hw_mgr_intf;
	rc = hw_mgr_intf->hw_open(hw_mgr_intf->hw_mgr_priv, NULL);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "FW download failed");
		goto end;
	}
	g_icp_dev.open_cnt++;
end:
	mutex_unlock(&g_icp_dev.icp_lock);
	return rc;
}

static int cam_icp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_hw_mgr_intf *hw_mgr_intf = NULL;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	mutex_lock(&g_icp_dev.icp_lock);
	if (g_icp_dev.open_cnt <= 0) {
		CAM_DBG(CAM_ICP, "ICP subdev is already closed");
		rc = -EINVAL;
		goto end;
	}
	g_icp_dev.open_cnt--;
	if (!node) {
		CAM_ERR(CAM_ICP, "Invalid args");
		rc = -EINVAL;
		goto end;
	}

	hw_mgr_intf = &node->hw_mgr_intf;
	if (!hw_mgr_intf) {
		CAM_ERR(CAM_ICP, "hw_mgr_intf is not initialized");
		rc = -EINVAL;
		goto end;
	}

	rc = cam_node_shutdown(node);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "HW close failed");
		goto end;
	}

end:
	mutex_unlock(&g_icp_dev.icp_lock);
	return 0;
}

const struct v4l2_subdev_internal_ops cam_icp_subdev_internal_ops = {
	.open = cam_icp_subdev_open,
	.close = cam_icp_subdev_close,
};

static int cam_icp_dev_fp_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_node_fastpath *fp_node =
		(struct cam_node_fastpath *) v4l2_get_subdevdata(sd);

	if (!fp_node) {
		CAM_ERR(CAM_ISP, "Node ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&g_icp_dev_fastpath.mutex);

	g_icp_dev_fastpath.open_cnt++;
	if (g_icp_dev_fastpath.open_cnt == 1)
		cam_node_fastpath_poweron(fp_node);

	mutex_unlock(&g_icp_dev_fastpath.mutex);

	return 0;
}

static int cam_icp_dev_fp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_node_fastpath *fp_node =
		(struct cam_node_fastpath *) v4l2_get_subdevdata(sd);

	if (!fp_node) {
		CAM_ERR(CAM_ISP, "Node ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&g_icp_dev_fastpath.mutex);
	if (!g_icp_dev_fastpath.open_cnt) {
		mutex_unlock(&g_icp_dev_fastpath.mutex);
		return -EINVAL;
	}

	g_icp_dev_fastpath.open_cnt--;
	if (!g_icp_dev_fastpath.open_cnt)
		cam_node_fastpath_shutdown(fp_node);

	mutex_unlock(&g_icp_dev_fastpath.mutex);

	return 0;
}

const struct v4l2_subdev_internal_ops cam_icp_subdev_fastpath_ops = {
	.open = cam_icp_dev_fp_subdev_open,
	.close = cam_icp_dev_fp_subdev_close,
};

/* Fastpath ops */
static const struct cam_node_fastpath_ops fastpath_ctx_ops = {
	.set_power       = cam_icp_fastpath_set_power,
	.query_cap       = cam_icp_fastpath_query_cap,
	.acquire_hw      = cam_icp_fastpath_acquire_hw,
	.release_hw      = cam_icp_fastpath_release_hw,
	.flush_dev       = cam_icp_fastpath_flush_dev,
	.config_dev      = cam_icp_fastpath_config_dev,
	.start_dev       = cam_icp_fastpath_start_dev,
	.stop_dev        = cam_icp_fastpath_stop_dev,
	.acquire_dev     = cam_icp_fastpath_acquire_dev,
	.release_dev     = cam_icp_fastpath_release_dev,
	.set_stream_mode = cam_icp_fastpath_set_stream_mode,
	.stream_mode_cmd = cam_isp_fastpath_stream_mode_cmd,
};

static int cam_icp_probe(struct platform_device *pdev)
{
	int rc = 0, i = 0;
	struct cam_node *node;
	struct cam_hw_mgr_intf *hw_mgr_intf;
	struct cam_node_fastpath *fastpath_node;
	int iommu_hdl = -1;

	if (!pdev) {
		CAM_ERR(CAM_ICP, "pdev is NULL");
		return -EINVAL;
	}

	g_icp_dev.sd.pdev = pdev;
	g_icp_dev.sd.internal_ops = &cam_icp_subdev_internal_ops;
	rc = cam_subdev_probe(&g_icp_dev.sd, pdev, CAM_ICP_DEV_NAME,
		CAM_ICP_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_ICP, "ICP cam_subdev_probe failed");
		goto probe_fail;
	}

	node = (struct cam_node *) g_icp_dev.sd.token;

	hw_mgr_intf = kzalloc(sizeof(*hw_mgr_intf), GFP_KERNEL);
	if (!hw_mgr_intf) {
		rc = -EINVAL;
		goto hw_alloc_fail;
	}

	rc = cam_icp_hw_mgr_init(pdev->dev.of_node, (uint64_t *)hw_mgr_intf,
		&iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_ICP, "ICP HW manager init failed: %d", rc);
		goto hw_init_fail;
	}

	for (i = 0; i < CAM_ICP_CTX_MAX; i++) {
		g_icp_dev.ctx_icp[i].base = &g_icp_dev.ctx[i];
		rc = cam_icp_context_init(&g_icp_dev.ctx_icp[i],
					hw_mgr_intf, i);
		if (rc) {
			CAM_ERR(CAM_ICP, "ICP context init failed");
			goto ctx_fail;
		}
	}

	rc = cam_node_init(node, hw_mgr_intf, g_icp_dev.ctx,
				CAM_ICP_CTX_MAX, CAM_ICP_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_ICP, "ICP node init failed");
		goto ctx_fail;
	}

	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_icp_dev_iommu_fault_handler, node);

	g_icp_dev.open_cnt = 0;
	mutex_init(&g_icp_dev.icp_lock);

	/* Init fastpath Context and node */
	g_icp_dev_fastpath.sd.internal_ops = &cam_icp_subdev_fastpath_ops;

	/* Initialize the v4l2 subdevice first. (create cam_node) */
	rc = cam_subdev_fastpath_probe(&g_icp_dev_fastpath.sd, pdev,
				       CAM_ICP_FP_DEV_NAME,
				       CAM_ICP_FASTPATH_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_ICP, "ICP cam_subdev_probe failed");
		goto ctx_fail;
	}
	fastpath_node = (struct cam_node_fastpath *)
		g_icp_dev_fastpath.sd.token;

	for (i = 0; i < FP_CAM_ICP_CTX_MAX; i++) {

		g_icp_dev_fastpath.ctx[i] = cam_icp_fastpath_context_create(
				hw_mgr_intf, i);

		if (!g_icp_dev_fastpath.ctx[i]) {
			CAM_ERR(CAM_ISP, "ISP context fastpah init failed!");
			rc = -ENOMEM;
			goto remove_fp_subdevice;
		}
	}

	rc = cam_node_fastpath_init(fastpath_node, "ICP Node",
				    &g_icp_dev_fastpath.ctx, FP_CAM_ICP_CTX_MAX,
				    &fastpath_ctx_ops);
	if (rc) {
		CAM_ERR(CAM_ISP, "ISP fastpath node init failed!");
		goto destroy_fp_context;
	}
	mutex_init(&g_icp_dev_fastpath.mutex);

	CAM_DBG(CAM_ICP, "ICP probe complete Fastpath");

	return rc;

destroy_fp_context:
	for (i = 0; i < FP_CAM_ICP_CTX_MAX; i++) {
		if (g_icp_dev_fastpath.ctx[i])
			cam_icp_fastpath_context_destroy(
						g_icp_dev_fastpath.ctx[i]);

		g_icp_dev_fastpath.ctx[i] = NULL;
	}
remove_fp_subdevice:
	cam_subdev_remove(&g_icp_dev_fastpath.sd);
ctx_fail:
	for (--i; i >= 0; i--)
		cam_icp_context_deinit(&g_icp_dev.ctx_icp[i]);
hw_init_fail:
	kfree(hw_mgr_intf);
hw_alloc_fail:
	cam_subdev_remove(&g_icp_dev.sd);
probe_fail:
	return rc;
}

static int cam_icp_remove(struct platform_device *pdev)
{
	int i;
	struct v4l2_subdev *sd;
	struct cam_subdev *subdev;

	if (!pdev) {
		CAM_ERR(CAM_ICP, "pdev is NULL");
		return -ENODEV;
	}

	sd = platform_get_drvdata(pdev);
	if (!sd) {
		CAM_ERR(CAM_ICP, "V4l2 subdev is NULL");
		return -ENODEV;
	}

	subdev = v4l2_get_subdevdata(sd);
	if (!subdev) {
		CAM_ERR(CAM_ICP, "cam subdev is NULL");
		return -ENODEV;
	}

	for (i = 0; i < CAM_ICP_CTX_MAX; i++)
		cam_icp_context_deinit(&g_icp_dev.ctx_icp[i]);
	cam_node_deinit(g_icp_dev.node);
	cam_subdev_remove(&g_icp_dev.sd);
	mutex_destroy(&g_icp_dev.icp_lock);

	/* Deinit fastpath resourcess */
	for (i = 0; i < FP_CAM_ICP_CTX_MAX; i++) {

		if (g_icp_dev_fastpath.ctx[i])
			cam_icp_fastpath_context_destroy(
				g_icp_dev_fastpath.ctx[i]);

		g_icp_dev_fastpath.ctx[i] = NULL;
	}

	cam_subdev_remove(&g_icp_dev_fastpath.sd);

	mutex_destroy(&g_icp_dev_fastpath.mutex);

	memset(&g_icp_dev_fastpath, 0, sizeof(g_icp_dev_fastpath));

	return 0;
}

static struct platform_driver cam_icp_driver = {
	.probe = cam_icp_probe,
	.remove = cam_icp_remove,
	.driver = {
		.name = "cam_icp",
		.owner = THIS_MODULE,
		.of_match_table = cam_icp_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_icp_init_module(void)
{
	return platform_driver_register(&cam_icp_driver);
}

static void __exit cam_icp_exit_module(void)
{
	platform_driver_unregister(&cam_icp_driver);
}
module_init(cam_icp_init_module);
module_exit(cam_icp_exit_module);
MODULE_DESCRIPTION("MSM ICP driver");
MODULE_LICENSE("GPL v2");
