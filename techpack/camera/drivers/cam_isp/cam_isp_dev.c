// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include <media/cam_req_mgr.h>
#include "cam_isp_dev.h"
#include "cam_hw_mgr_intf.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_node.h"
#include "cam_node_fastpath.h"
#include "cam_subdev_fastpath.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"

static struct cam_isp_dev g_isp_dev;

/* fastpath isp device */
static struct cam_isp_fastpath_dev g_isp_dev_fastpath = {};

static void cam_isp_dev_iommu_fault_handler(
	struct iommu_domain *domain, struct device *dev, unsigned long iova,
	int flags, void *token, uint32_t buf_info)
{
	int i = 0;
	struct cam_node *node = NULL;

	if (!token) {
		CAM_ERR(CAM_ISP, "invalid token in page handler cb");
		return;
	}

	node = (struct cam_node *)token;

	for (i = 0; i < node->ctx_size; i++)
		cam_context_dump_pf_info(&(node->ctx_list[i]), iova,
			buf_info);
}

static const struct of_device_id cam_isp_dt_match[] = {
	{
		.compatible = "qcom,cam-isp"
	},
	{}
};

static int cam_isp_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	cam_req_mgr_rwsem_read_op(CAM_SUBDEV_LOCK);

	mutex_lock(&g_isp_dev.isp_mutex);
	g_isp_dev.open_cnt++;
	mutex_unlock(&g_isp_dev.isp_mutex);

	cam_req_mgr_rwsem_read_op(CAM_SUBDEV_UNLOCK);

	return 0;
}

static int cam_isp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	mutex_lock(&g_isp_dev.isp_mutex);
	if (g_isp_dev.open_cnt <= 0) {
		CAM_DBG(CAM_ISP, "ISP subdev is already closed");
		rc = -EINVAL;
		goto end;
	}

	g_isp_dev.open_cnt--;
	if (!node) {
		CAM_ERR(CAM_ISP, "Node ptr is NULL");
		rc = -EINVAL;
		goto end;
	}

	if (g_isp_dev.open_cnt == 0)
		cam_node_shutdown(node);

end:
	mutex_unlock(&g_isp_dev.isp_mutex);
	return rc;
}

static const struct v4l2_subdev_internal_ops cam_isp_subdev_internal_ops = {
	.close = cam_isp_subdev_close,
	.open = cam_isp_subdev_open,
};

static int cam_isp_dev_fp_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_node_fastpath *fp_node =
		(struct cam_node_fastpath *) v4l2_get_subdevdata(sd);

	if (!fp_node) {
		CAM_ERR(CAM_ISP, "Node ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&g_isp_dev_fastpath.mutex);

	g_isp_dev_fastpath.open_cnt++;
	if (g_isp_dev_fastpath.open_cnt == 1)
		cam_node_fastpath_poweron(fp_node);

	mutex_unlock(&g_isp_dev_fastpath.mutex);

	return 0;
}

static int cam_isp_dev_fp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_node_fastpath *fp_node =
		(struct cam_node_fastpath *) v4l2_get_subdevdata(sd);

	if (!fp_node) {
		CAM_ERR(CAM_ISP, "Node ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&g_isp_dev_fastpath.mutex);
	if (!g_isp_dev_fastpath.open_cnt) {
		mutex_unlock(&g_isp_dev_fastpath.mutex);
		return -EINVAL;
	}

	g_isp_dev_fastpath.open_cnt--;
	if (!g_isp_dev_fastpath.open_cnt)
		cam_node_fastpath_shutdown(fp_node);

	mutex_unlock(&g_isp_dev_fastpath.mutex);

	return 0;
}

static const struct v4l2_subdev_internal_ops cam_isp_subdev_fastpath_ops = {
	.close = cam_isp_dev_fp_subdev_close,
	.open = cam_isp_dev_fp_subdev_open,
};

/* Fastpath ops */
static const struct cam_node_fastpath_ops fastpath_ctx_ops = {
	.set_power       = NULL,
	.query_cap       = cam_isp_fastpath_query_cap,
	.acquire_hw      = cam_isp_fastpath_acquire_hw,
	.release_hw      = cam_isp_fastpath_release_hw,
	.flush_dev       = cam_isp_fastpath_flush_dev,
	.config_dev      = cam_isp_fastpath_config_dev,
	.start_dev       = cam_isp_fastpath_start_dev,
	.stop_dev        = cam_isp_fastpath_stop_dev,
	.acquire_dev     = cam_isp_fastpath_acquire_dev,
	.release_dev     = cam_isp_fastpath_release_dev,
	.set_stream_mode = cam_isp_fastpath_set_stream_mode,
	.stream_mode_cmd = cam_isp_fastpath_stream_mode_cmd,
};

static int cam_isp_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	int i;

	/* clean up resources */
	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_isp_context_deinit(&g_isp_dev.ctx_isp[i]);
		if (rc)
			CAM_ERR(CAM_ISP, "ISP context %d deinit failed",
				 i);
	}

	rc = cam_subdev_remove(&g_isp_dev.sd);
	if (rc)
		CAM_ERR(CAM_ISP, "Unregister failed");

	mutex_destroy(&g_isp_dev.isp_mutex);

	memset(&g_isp_dev, 0, sizeof(g_isp_dev));

	for (i = 0; i < CAM_CTX_MAX; i++) {
		/* Destroy fastpath resourcess */
		if (g_isp_dev_fastpath.ctx[i])
			cam_isp_fastpath_context_destroy(
						g_isp_dev_fastpath.ctx[i]);

		g_isp_dev_fastpath.ctx[i] = NULL;
	}

	rc = cam_subdev_fastpath_remove(&g_isp_dev_fastpath.sd);
	if (rc)
		CAM_ERR(CAM_ISP, "Unregister failed");

	mutex_destroy(&g_isp_dev_fastpath.mutex);

	memset(&g_isp_dev_fastpath, 0, sizeof(g_isp_dev_fastpath));

	return 0;
}

static int cam_isp_dev_probe(struct platform_device *pdev)
{
	int rc = -1;
	int i;
	int cam_ctx_initialized = 0;
	struct cam_hw_mgr_intf         hw_mgr_intf;
	struct cam_node               *node;
	struct cam_node_fastpath      *fastpath_node;
	int iommu_hdl = -1;

	g_isp_dev.sd.internal_ops = &cam_isp_subdev_internal_ops;
	/* Initialize the v4l2 subdevice first. (create cam_node) */
	rc = cam_subdev_probe(&g_isp_dev.sd, pdev, CAM_ISP_DEV_NAME,
		CAM_IFE_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_ISP, "ISP cam_subdev_probe failed!");
		goto err;
	}
	node = (struct cam_node *) g_isp_dev.sd.token;

	memset(&hw_mgr_intf, 0, sizeof(hw_mgr_intf));
	rc = cam_isp_hw_mgr_init(pdev->dev.of_node, &hw_mgr_intf, &iommu_hdl);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Can not initialized ISP HW manager!");
		goto unregister;
	}

	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_isp_context_init(&g_isp_dev.ctx_isp[i],
			&g_isp_dev.ctx[i],
			&node->crm_node_intf,
			&node->hw_mgr_intf,
			i);
		if (rc) {
			CAM_ERR(CAM_ISP, "ISP context init failed!");
			goto destroy_context;
		}
		cam_ctx_initialized++;
	}

	rc = cam_node_init(node, &hw_mgr_intf, g_isp_dev.ctx, CAM_CTX_MAX,
		CAM_ISP_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_ISP, "ISP node init failed!");
		goto destroy_context;
	}

	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_isp_dev_iommu_fault_handler, node);

	mutex_init(&g_isp_dev.isp_mutex);

	/* Init fastpath Context and node */
	g_isp_dev_fastpath.sd.internal_ops = &cam_isp_subdev_fastpath_ops;

	/* Initialize the v4l2 subdevice first. (create cam_node) */
	rc = cam_subdev_fastpath_probe(&g_isp_dev_fastpath.sd, pdev,
				       CAM_ISP_DEV_FASTPATH_NAME,
				       CAM_IFE_FASTPATH_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_ISP, "ISP cam_subdev_probe fastpath failed!");
		goto destroy_context;
	}
	fastpath_node =
		(struct cam_node_fastpath *) g_isp_dev_fastpath.sd.token;

	for (i = 0; i < CAM_CTX_MAX; i++) {

		g_isp_dev_fastpath.ctx[i] =
			cam_isp_fastpath_context_create(&hw_mgr_intf, i);

		if (!g_isp_dev_fastpath.ctx[i]) {
			CAM_ERR(CAM_ISP, "ISP context fastpah init failed!");
			rc = -ENOMEM;
			goto error_fastpath_subdev_remove;
		}
	}

	rc = cam_node_fastpath_init(fastpath_node, "ISP Node",
				    &g_isp_dev_fastpath.ctx, CAM_CTX_MAX,
				    &fastpath_ctx_ops);
	if (rc) {
		CAM_ERR(CAM_ISP, "ISP fastpath node init failed!");
		goto error_fp_context_destroy;
	}
	mutex_init(&g_isp_dev_fastpath.mutex);

	CAM_INFO(CAM_ISP, "Camera ISP probe complete with fastpath complete");

	return 0;

error_fp_context_destroy:
	for (i = 0; i < CAM_CTX_MAX; i++) {

		if (g_isp_dev_fastpath.ctx[i])
			cam_isp_fastpath_context_destroy(
						g_isp_dev_fastpath.ctx[i]);

		g_isp_dev_fastpath.ctx[i] = NULL;
	}
error_fastpath_subdev_remove:
	cam_subdev_remove(&g_isp_dev_fastpath.sd);
	mutex_destroy(&g_isp_dev.isp_mutex);
destroy_context:
	for (i = 0; i < cam_ctx_initialized; i++)
		cam_isp_context_deinit(&g_isp_dev.ctx_isp[i]);
unregister:
	rc = cam_subdev_remove(&g_isp_dev.sd);
err:
	return rc;
}


static struct platform_driver isp_driver = {
	.probe = cam_isp_dev_probe,
	.remove = cam_isp_dev_remove,
	.driver = {
		.name = "cam_isp",
		.owner = THIS_MODULE,
		.of_match_table = cam_isp_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_isp_dev_init_module(void)
{
	return platform_driver_register(&isp_driver);
}

static void __exit cam_isp_dev_exit_module(void)
{
	platform_driver_unregister(&isp_driver);
}

module_init(cam_isp_dev_init_module);
module_exit(cam_isp_dev_exit_module);
MODULE_DESCRIPTION("MSM ISP driver");
MODULE_LICENSE("GPL v2");
