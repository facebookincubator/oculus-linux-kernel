// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/types.h>
#include <linux/of_platform.h>

#include "mmrm_internal.h"
#include "mmrm_debug.h"

static struct mmrm_common_data common_pt_data[] = {
	{
		.key = "qcom,mmrm_clk_mgr_scheme",
		.value = CLK_MGR_SCHEME_SW,
	},
};

/*throttle client list is as per fdd & resource availability*/

static struct mmrm_throttle_clients_data common_pt_throttle_clients_data[] = {
	{
		.domain = MMRM_CLIENT_DOMAIN_DISPLAY,
		.id = 0x3d,
	},
	{
		.domain = MMRM_CLIENT_DOMAIN_VIDEO,
		.id = 0x03,
	},
	{
		.domain = MMRM_CLIENT_DOMAIN_CAMERA,
		.id = 0x46,
	},
	{
		.domain = MMRM_CLIENT_DOMAIN_CVP,
		.id = 0x08,
	},
	{
		.domain = MMRM_CLIENT_DOMAIN_CAMERA,
		.id = 0x02,
	},
};

static struct mmrm_platform_data commom_pt_platform_data = {
	.common_data = common_pt_data,
	.common_data_length = ARRAY_SIZE(common_pt_data),
	.throttle_clk_clients_data = common_pt_throttle_clients_data,
	.throttle_clk_clients_data_length = ARRAY_SIZE(common_pt_throttle_clients_data),
};

static const struct of_device_id mmrm_dt_match[] = {
	{
		.compatible = "qcom,waipio-mmrm",
		.data = &commom_pt_platform_data,
	},
	{
		.compatible = "qcom,kalama-mmrm",
		.data = &commom_pt_platform_data,
	},
	{},
};

struct mmrm_platform_data *mmrm_get_platform_data(struct device *dev)
{
	struct mmrm_platform_data *platform_data = NULL;
	const struct of_device_id *match;

	match = of_match_node(mmrm_dt_match, dev->of_node);
	if (match)
		platform_data = (struct mmrm_platform_data *)match->data;

	if (!platform_data)
		goto exit;

	/* add additional config checks for platform data */

exit:
	return platform_data;
}

int mmrm_init(struct mmrm_driver_data *drv_data)
{
	int rc = 0;

	/* get clk resource mgr ops */
	rc = mmrm_get_clk_mgr_ops(drv_data);
	if (rc) {
		d_mpr_e("%s: init clk mgr failed\n", __func__);
		goto err_get_clk_mgr_ops;
	}

	/* clock resource mgr */
	rc = drv_data->clk_mgr_ops->init_clk_mgr(drv_data);
	if (rc) {
		d_mpr_e("%s: init clk mgr failed\n", __func__);
		goto err_init_clk_mgr;
	}
	return rc;

err_init_clk_mgr:
err_get_clk_mgr_ops:
	return rc;
}

int mmrm_deinit(struct mmrm_driver_data *drv_data)
{
	int rc = 0;

	if (!drv_data || !drv_data->clk_mgr_ops ||
		!drv_data->clk_mgr_ops->destroy_clk_mgr) {
		d_mpr_e("%s: invalid driver data or clk mgr ops\n", __func__);
		return -EINVAL;
	}

	/* destroy clock resource mgr */
	rc = drv_data->clk_mgr_ops->destroy_clk_mgr(drv_data->clk_mgr);
	if (rc) {
		d_mpr_e("%s: destroy clk mgr failed\n", __func__);
		drv_data->clk_mgr = NULL;
	}

	return rc;
}
