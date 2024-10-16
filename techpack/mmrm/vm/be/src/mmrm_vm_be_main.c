// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "mmrm_vm_be.h"
#include "mmrm_vm_msgq.h"
#include "mmrm_vm_interface.h"
#include "mmrm_debug.h"

#define MMRM_CLK_CLIENTS_NUM_MAX 35

struct mmrm_vm_driver_data *drv_vm_be = (void *) -EPROBE_DEFER;

int msm_mmrm_debug = MMRM_ERR | MMRM_WARN | MMRM_PRINTK;

int mmrm_client_get_clk_count(void);

static int mmrm_vm_be_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int   sz, clk_count, rc;

	drv_vm_be = devm_kzalloc(dev, sizeof(*drv_vm_be), GFP_KERNEL);
	if (!drv_vm_be)
		return -ENOMEM;

	clk_count = mmrm_client_get_clk_count();
	if (clk_count <= 0 || clk_count > MMRM_CLK_CLIENTS_NUM_MAX) {
		d_mpr_e("%s: clk count is not correct\n", __func__);
		goto clk_count_err;
	}
	sz = sizeof(struct mmrm_client *) * clk_count;
	drv_vm_be->clk_client_tbl = devm_kzalloc(dev, sz, GFP_KERNEL);
	if (!drv_vm_be->clk_client_tbl)
		goto client_tbl_err;

	drv_vm_be->debugfs_root = msm_mmrm_debugfs_init();
	if (!drv_vm_be->debugfs_root)
		d_mpr_e("%s: failed to create debugfs for mmrm\n", __func__);

	dev_set_drvdata(&pdev->dev, drv_vm_be);
	rc = mmrm_vm_msgq_init(drv_vm_be);
	if (rc != 0) {
		d_mpr_e("%s: failed to init msgq\n", __func__);
		goto msgq_init_err;
	}

	drv_vm_be->dev = dev;
	dev_err(dev, "msgq probe success");
	return 0;

msgq_init_err:
	dev_set_drvdata(&pdev->dev, NULL);
	msm_mmrm_debugfs_deinit(drv_vm_be->debugfs_root);
	return -EINVAL;
client_tbl_err:
	dev_err(dev, "msgq register alloc memory failed");
	return -ENOMEM;
clk_count_err:
	return -EINVAL;
}

static int mmrm_vm_be_driver_remove(struct platform_device *pdev)
{
	mmrm_vm_msgq_deinit(drv_vm_be);
	msm_mmrm_debugfs_deinit(drv_vm_be->debugfs_root);

	dev_set_drvdata(&pdev->dev, NULL);
	drv_vm_be = (void *) -EPROBE_DEFER;
	return 0;
}

static const struct of_device_id mmrm_vm_be_match[] = {
	{ .compatible = "qcom,mmrm-vm-be" },
	{},
};
MODULE_DEVICE_TABLE(of, mmrm_vm_be_match);

static struct platform_driver mmrm_vm_be_driver = {
	.probe = mmrm_vm_be_driver_probe,
	.driver = {
		.name = "mmrm-vm-be",
		.of_match_table = mmrm_vm_be_match,
	},
	.remove = mmrm_vm_be_driver_remove,
};

static int __init mmrm_vm_be_module_init(void)
{
	pr_info("%s:  init start\n", __func__);

	return platform_driver_register(&mmrm_vm_be_driver);
}
subsys_initcall(mmrm_vm_be_module_init);

static void __exit mmrm_vm_be_module_exit(void)
{
	platform_driver_unregister(&mmrm_vm_be_driver);
}
module_exit(mmrm_vm_be_module_exit);

MODULE_SOFTDEP("pre: gunyah_transport");
MODULE_SOFTDEP("pre: msm-mmrm");

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. MMRM BE Driver");
MODULE_LICENSE("GPL v2");
