/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mmrm_test: " fmt

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/qcom/msm_mmrm.h>

#include <soc/qcom/socinfo.h>

#include "mmrm_vm_fe_test_internal.h"
#include "mmrm_vm_debug.h"

#define MODULE_NAME "mmrm_vm_fe_test"

enum supported_soc_ids {
	SOC_KALAMA_ID = 519			/* KAILUA */
};

struct mmrm_test_platform_resources {
	struct platform_device *pdev;
	struct clock_rate *clk_rate_tbl;
	u32 count;
};

struct mmrm_test_driver_data {
	struct mmrm_test_platform_resources clk_res;
};

static struct mmrm_test_driver_data *test_drv_data = (void *) -EPROBE_DEFER;

int mmrm_vm_debug = MMRM_VM_ERR | MMRM_VM_WARN | MMRM_VM_PRINTK;

int mmrm_vm_fe_load_mmrm_test_table(
	struct mmrm_test_platform_resources *dt_res)
{
	int rc = 0, num_clock_names = 0, c = 0;
	struct platform_device *pdev = dt_res->pdev;
	int   entry_offset = 0;
	struct clock_rate *clk_rate;

	num_clock_names = of_property_count_strings(pdev->dev.of_node,
			"clock-names");
	if (num_clock_names <= 0) {
		dt_res->count = 0;
		goto err_load_corner_tbl;
	}
	d_mpr_h("%s: count =%d\n", __func__, num_clock_names);

	dt_res->clk_rate_tbl = devm_kzalloc(&pdev->dev,
		sizeof(*dt_res->clk_rate_tbl) * num_clock_names, GFP_KERNEL);

	if (!dt_res->clk_rate_tbl) {
		rc = -ENOMEM;
		goto err_load_corner_tbl;
	}
	dt_res->count = num_clock_names;

	clk_rate = dt_res->clk_rate_tbl;
	for (c = 0; c < num_clock_names; c++, clk_rate++) {
		of_property_read_string_index(pdev->dev.of_node,
			"clock-names", c, &clk_rate->name);
	}

	clk_rate = dt_res->clk_rate_tbl;
	for (c = 0; c < num_clock_names; c++, entry_offset += 7, clk_rate++) {
		of_property_read_u32_index(pdev->dev.of_node,
			"clock_rates", entry_offset, &clk_rate->domain);

		of_property_read_u32_index(pdev->dev.of_node,
			"clock_rates", entry_offset+1, &clk_rate->id);

		of_property_read_u32_index(pdev->dev.of_node,
			"clock_rates", entry_offset+2, &clk_rate->clk_rates[0]);

		of_property_read_u32_index(pdev->dev.of_node,
			"clock_rates", entry_offset+3, &clk_rate->clk_rates[1]);

		of_property_read_u32_index(pdev->dev.of_node,
			"clock_rates", entry_offset+4, &clk_rate->clk_rates[2]);

		of_property_read_u32_index(pdev->dev.of_node,
			"clock_rates", entry_offset+5, &clk_rate->clk_rates[3]);

		of_property_read_u32_index(pdev->dev.of_node,
			"clock_rates", entry_offset+6, &clk_rate->clk_rates[4]);
	}

	/* print clock rate tables */

	clk_rate = dt_res->clk_rate_tbl;
	for (c = 0; c < num_clock_names; c++, clk_rate++) {
		d_mpr_h("clock name:%s, %d, %d, %d, %d, %d, %d, %d\n", clk_rate->name,
			clk_rate->domain, clk_rate->id,
			clk_rate->clk_rates[0], clk_rate->clk_rates[1],
			clk_rate->clk_rates[2], clk_rate->clk_rates[3],
			clk_rate->clk_rates[4]);
	}

	return 0;

err_load_corner_tbl:
	return rc;
}

int mmrm_clk_print_info(
	struct mmrm_test_platform_resources *dt_res,
	char *buf, int max_len)
{
	struct clock_rate *clk_rate;
	int left_spaces = max_len;
	int len, c;
	u32 count;

	count = dt_res->count;
	/* print clock rate tables */

	clk_rate = dt_res->clk_rate_tbl;
	for (c = 0; c < count; c++, clk_rate++) {
		len = scnprintf(buf, left_spaces, "clock name:%s, %d, %d, %d, %d, %d, %d, %d\n",
			clk_rate->name, clk_rate->domain, clk_rate->id,
			clk_rate->clk_rates[0], clk_rate->clk_rates[1],
			clk_rate->clk_rates[2], clk_rate->clk_rates[3],
			clk_rate->clk_rates[4]);

		left_spaces -= len;
		buf += len;
	}
	return max_len - left_spaces;
}

struct clock_rate *find_clk_by_name(const char *name)
{
	int  i;
	struct mmrm_test_platform_resources  *res = &test_drv_data->clk_res;
	struct clock_rate *p = res->clk_rate_tbl;

	for (i=0; i < res->count; i++, p++)
	{
		if (strcmp(name, p->name) == 0)
			return p;
	}
	return NULL;
}

int get_clock_count(void)
{
	return test_drv_data->clk_res.count;
}

struct clock_rate *get_nth_clock(int nth)
{
	struct mmrm_test_platform_resources  *res = &test_drv_data->clk_res;

	return &(res->clk_rate_tbl[nth]);
}

int mmrm_vm_fe_test_read_platform_resources(struct platform_device *pdev)
{
	int rc = 0;

	d_mpr_h("%s: mmrm_test_read_platform_resources =%p test_drv_data:%p\n",
		__func__, pdev, test_drv_data);

	if (pdev == NULL || pdev->dev.of_node == NULL) {
		rc = -EINVAL;
		goto exit;
	}

	if (test_drv_data == (void *) -EPROBE_DEFER) {
		d_mpr_e("%s: mmrm_test_read_platform_resources\n", __func__);
		goto exit;
	}

	rc = mmrm_vm_fe_load_mmrm_test_table(&test_drv_data->clk_res);
	if (rc) {
		goto exit;
	}
exit:
	return rc;
}

static int mmrm_test(struct platform_device *pdev, int flags)
{
	int soc_id;

	// Get socid to get known mmrm configurations
	soc_id = socinfo_get_id();
	d_mpr_e("%s: soc id: %d flags=%x\n", __func__, soc_id, flags);
	soc_id = SOC_KALAMA_ID;
	d_mpr_e("%s: soc id: %d flags=%x\n", __func__, soc_id, flags);

	switch (soc_id) {
	case SOC_KALAMA_ID:
		if (flags & 1)
			mmrm_vm_fe_client_tests(pdev);
		if (flags & 2)
			test_mmrm_concurrent_client_cases(pdev, kalama_testcases, kalama_testcases_count);
		if (flags & 4)
			test_mmrm_switch_volt_corner_client_testcases(pdev, kalama_cornercase_testcases, kalama_cornercase_testcases_count);
		break;
	default:
		d_mpr_e("%s: Not supported for soc_id %d\n", __func__, soc_id);
		return -ENODEV;
	}
	return 0;
}

static ssize_t mmrm_vm_fe_sysfs_debug_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = scnprintf(buf, MMRM_SYSFS_ENTRY_MAX_LEN, "0x%x\n", mmrm_vm_debug);
	pr_info("%s: 0x%04X\n", __func__, mmrm_vm_debug);

	return ret;
}

static ssize_t mmrm_vm_fe_sysfs_debug_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long dbg_mask;

	ret = kstrtoul(buf, 16, &dbg_mask);
	if (ret == 0)
		mmrm_vm_debug = dbg_mask;

	return count;
}

static ssize_t dump_clk_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = mmrm_clk_print_info(&test_drv_data->clk_res, buf, MMRM_SYSFS_ENTRY_MAX_LEN);
	if (rc == 0)
		d_mpr_e("%s: failed to dump clk info\n", __func__);

	return rc;
}

ssize_t test_trigger_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	int ret;
	unsigned long flags;

	d_mpr_e("%s: start ...\n", __func__);

	ret = kstrtoul(buf, 16, &flags);
	if (ret) {
		d_mpr_e("invalid user input\n");
		return -1;
	}

	if  (flags & 0x80)
		mmrm_vm_fe_client_register_tests(pdev);
	else
		mmrm_test(pdev, flags);

	return count;
}

static DEVICE_ATTR_RO(dump_clk_info);

static DEVICE_ATTR(debug, 0644,
	mmrm_vm_fe_sysfs_debug_get,
	mmrm_vm_fe_sysfs_debug_set);

static DEVICE_ATTR_WO(test_trigger);

static struct attribute *mmrm_vm_fe_test_fs_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_dump_clk_info.attr,
	&dev_attr_test_trigger.attr,
	NULL,
};

static struct attribute_group mmrm_vm_fe_test_fs_attrs_group = {
	.attrs = mmrm_vm_fe_test_fs_attrs,
};

static int mmrm_vm_fe_test_probe(struct platform_device *pdev)
{
	bool is_mmrm_supported = false;
	int rc;

	// Check if of_node is found
	if (!of_device_is_compatible(pdev->dev.of_node, "qcom,mmrm-vm-fe-test")) {
		d_mpr_e("No compatible device node\n");
		return 1;
	}

	is_mmrm_supported = mmrm_client_check_scaling_supported(MMRM_CLIENT_CLOCK, 0);
	if (!is_mmrm_supported) {
		d_mpr_e("%s: MMRM not supported on %s\n", __func__, socinfo_get_id_string());
		return 0;
	}

	test_drv_data = kzalloc(sizeof(*test_drv_data), GFP_KERNEL);
	if (!test_drv_data) {
		rc = -ENOMEM;
		goto err_no_mem;
	}
	test_drv_data->clk_res.pdev = pdev;
	dev_set_drvdata(&pdev->dev, test_drv_data);

	rc = mmrm_vm_fe_test_read_platform_resources(pdev);
	if (rc) {
		d_mpr_e("%s: unable to read platform resources for mmrm\n",
			__func__);
		goto err_read;
	}
	d_mpr_e("%s: Validating mmrm on target\n", __func__);

	if (sysfs_create_group(&pdev->dev.kobj, &mmrm_vm_fe_test_fs_attrs_group)) {
		d_mpr_e("%s: failed to create sysfs\n",
			__func__);
	}

err_no_mem:
err_read:
	return 0;
}

int mmrm_vm_fe_test_remove(struct platform_device *pdev)
{
	int rc = 0;

	if (!pdev) {
		rc = -EINVAL;
		goto err_exit;
	}

	test_drv_data = dev_get_drvdata(&pdev->dev);
	if (!test_drv_data) {
		rc = -EINVAL;
		goto err_exit;
	}

	if (test_drv_data->clk_res.clk_rate_tbl)
		kfree(test_drv_data->clk_res.clk_rate_tbl);

	dev_set_drvdata(&pdev->dev, NULL);

	kfree(test_drv_data);
	test_drv_data = (void *) -EPROBE_DEFER;

err_exit:
	return rc;
}

static const struct of_device_id mmrm_vm_fe_test_dt_match[] = {
	{.compatible = "qcom,mmrm-vm-fe-test"}, {} // empty
};

static struct platform_driver mmrm_vm_fe_test_driver = {
	.probe = mmrm_vm_fe_test_probe,
	.remove = mmrm_vm_fe_test_remove,
	.driver = {
			.name = MODULE_NAME,
			.owner = THIS_MODULE,
			.of_match_table = mmrm_vm_fe_test_dt_match,
		},
};

static int __init mmrm_vm_fe_test_init(void)
{
	int rc = 0;
	rc = platform_driver_register(&mmrm_vm_fe_test_driver);
	if (rc) {
		pr_info("%s: failed to register platform driver\n", __func__);
	}
	return rc;
}
module_init(mmrm_vm_fe_test_init);

static void __exit mmrm_vm_fe_test_exit(void)
{
	platform_driver_unregister(&mmrm_vm_fe_test_driver);
}
module_exit(mmrm_vm_fe_test_exit);

MODULE_DESCRIPTION("MMRM VM FE TEST");
MODULE_LICENSE("GPL v2");
