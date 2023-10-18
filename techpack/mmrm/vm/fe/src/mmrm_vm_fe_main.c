// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kthread.h>

#include <linux/module.h>
#include <linux/of.h>

#include "mmrm_vm_fe.h"
#include "mmrm_vm_msgq.h"
#include "mmrm_vm_interface.h"
#include "mmrm_vm_debug.h"

struct mmrm_vm_driver_data *drv_vm_fe = (void *) -EPROBE_DEFER;

static ssize_t dump_clk_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct mmrm_vm_fe_priv *fe_data = drv_vm_fe->vm_pvt_data;

	rc = mmrm_vm_fe_clk_print_info(&fe_data->clk_src_set, buf, MMRM_SYSFS_ENTRY_MAX_LEN);
	if (rc == 0)
		d_mpr_e("%s: failed to dump clk info\n", __func__);

	return rc;
}

ssize_t msgq_send_trigger_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct mmrm_vm_driver_data *priv = dev->driver_data;
	char send_buf[64] = "test msg";
	int ret;
	bool flag;

	ret = strtobool(buf, &flag);
	if (ret) {
		dev_err(dev, "invalid user input\n");
		return -1;
	}
	if (flag) {
		ret = mmrm_vm_msgq_send(priv, send_buf, sizeof(send_buf));
		if (ret)
			d_mpr_e("%s:send msgq failed\n", __func__);
		else
			d_mpr_e("%s:send msgq success\n", __func__);
	}
	return ret ? ret : count;
}

extern int mmrm_client_msgq_roundtrip_measure(u32 val);

ssize_t msgq_rt_test_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	long sz, n;
	struct mmrm_vm_fe_priv *fe_data;
	struct mmrm_vm_fe_msgq_rt_stats *trip_time;

	if (IS_ERR_OR_NULL(drv_vm_fe)) {
		return -1;
	}
	ret = kstrtol(buf, 10, &sz);

	if (ret) {
		dev_err(dev, "invalid user input\n");
		return -1;
	}
	if (sz) {
		d_mpr_w("%s: loop count:%d\n", __func__, sz);

		fe_data = drv_vm_fe->vm_pvt_data;
		trip_time = &fe_data->msgq_rt_stats;
		trip_time->looptest_total_us = 0;

		n = sz;
		while (n-- > 0) {
			ret = mmrm_client_msgq_roundtrip_measure(0);
			if (ret) {
				d_mpr_e("%s:send msgq failed\n", __func__);
				break;
			};
		}
		if (n <= 0)
			d_mpr_w("%s: aver: %d\n", __func__, trip_time->looptest_total_us / sz);
	}
	return ret ? ret : count;
}

static DEVICE_ATTR_RO(dump_clk_info);
static DEVICE_ATTR_WO(msgq_send_trigger);
static DEVICE_ATTR_WO(msgq_rt_test);

static struct attribute *mmrm_vm_fe_fs_attrs[] = {
	&dev_attr_dump_clk_info.attr,
	&dev_attr_msgq_send_trigger.attr,
	&dev_attr_msgq_rt_test.attr,
	NULL,
};

static struct attribute_group mmrm_vm_fe_fs_attrs_group = {
	.attrs = mmrm_vm_fe_fs_attrs,
};

static int mmrm_vm_fe_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mmrm_vm_fe_priv *fe_priv_data;
	int rc = 0;
	u32 clk_clients = 0;

	drv_vm_fe = devm_kzalloc(dev, sizeof(*drv_vm_fe), GFP_KERNEL);
	if (!drv_vm_fe)
		return -ENOMEM;

	fe_priv_data = devm_kzalloc(dev, sizeof(*fe_priv_data), GFP_KERNEL);
	if (!fe_priv_data) {
		rc = -ENOMEM;
		goto err_priv_data;
	}

	drv_vm_fe->vm_pvt_data = fe_priv_data;
	fe_priv_data->seq_no = 0;
	fe_priv_data->dev = dev;

	/* check for clk clients needing admission control */
	clk_clients = mmrm_vm_fe_count_clk_clients_frm_dt(pdev);
	if (clk_clients) {
		d_mpr_h("%s: %d clk clients managed for admission control\n",
			__func__, clk_clients);
		fe_priv_data->is_clk_scaling_supported = true;
	} else {
		d_mpr_h("%s: no clk clients managed for admission control\n",
			__func__);
		fe_priv_data->is_clk_scaling_supported = false;
		goto skip_mmrm;
	}

	mutex_init(&fe_priv_data->msg_send_lock);
	dev_set_drvdata(&pdev->dev, drv_vm_fe);

	INIT_LIST_HEAD(&fe_priv_data->resp_works);
	mutex_init(&fe_priv_data->resp_works_lock);

	mmrm_vm_fe_load_clk_rsrc(drv_vm_fe);
	rc = mmrm_vm_msgq_init(drv_vm_fe);
	if (rc != 0) {
		d_mpr_e("%s: failed to msgq init\n",
			__func__);
		goto err_msgq_init;
	}

	rc = mmrm_vm_fe_init_lookup_table(drv_vm_fe);
	if (rc == -1) {
		d_mpr_e("%s: failed to lookup table init\n",
			__func__);
		goto err_lookup_table;
	}

	if (sysfs_create_group(&pdev->dev.kobj, &mmrm_vm_fe_fs_attrs_group)) {
		d_mpr_e("%s: failed to create sysfs\n",
			__func__);
	}

	d_mpr_w("msgq probe success");
	return rc;

err_lookup_table:
	mmrm_vm_msgq_deinit(drv_vm_fe);

err_msgq_init:
err_priv_data:
	d_mpr_e("%s: failed to probe\n", __func__);

skip_mmrm:
	return rc;
}

static int mmrm_vm_fe_driver_remove(struct platform_device *pdev)
{
	struct mmrm_vm_driver_data *mmrm_vm = dev_get_drvdata(&pdev->dev);

	mmrm_vm_msgq_deinit(mmrm_vm);
	return 0;
}

static const struct of_device_id mmrm_vm_fe_match[] = {
	{ .compatible = "qcom,mmrm-vm-fe" },
	{},
};
MODULE_DEVICE_TABLE(of, mmrm_vm_fe_match);

static struct platform_driver mmrm_vm_fe_driver = {
	.probe = mmrm_vm_fe_driver_probe,
	.driver = {
		.name = "mmrm-vm-fe",
		.of_match_table = mmrm_vm_fe_match,
	},
	.remove = mmrm_vm_fe_driver_remove,
};

static int __init mmrm_vm_fe_module_init(void)
{
	d_mpr_e("%s:  init start\n", __func__);

	return platform_driver_register(&mmrm_vm_fe_driver);
}
subsys_initcall(mmrm_vm_fe_module_init);

static void __exit mmrm_vm_fe_module_exit(void)
{
	platform_driver_unregister(&mmrm_vm_fe_driver);
}
module_exit(mmrm_vm_fe_module_exit);

MODULE_SOFTDEP("pre: gunyah_transport");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Test MSGQ Driver");
MODULE_LICENSE("GPL v2");
