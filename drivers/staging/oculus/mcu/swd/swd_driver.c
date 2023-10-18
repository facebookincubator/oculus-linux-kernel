// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "fwupdate_debug.h"
#include "swd.h"

static ssize_t swd_driver_update_firmware_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return fwupdate_update_firmware_show(dev, buf);
}

static ssize_t swd_driver_update_firmware_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return fwupdate_update_firmware_store(dev, buf, count);
}

static int swd_driver_init_flash_parameters(struct device *dev, struct swd_mcu_data *mcudata, struct device_node *node)
{
	int ret;

	ret = of_property_read_u32(node, "oculus,flash-block-size",
				   &mcudata->flash_info.block_size);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-block-size: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "oculus,flash-page-size",
				   &mcudata->flash_info.page_size);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-page-size: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "oculus,flash-page-count",
				   &mcudata->flash_info.num_pages);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-page-count: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "oculus,flash-page-retained-count",
				   &mcudata->flash_info.num_retained_pages);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-page-retained-count: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "oculus,flash-page-bootloader-protected-count",
					&mcudata->flash_info.num_protected_bootloader_pages);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-page-bootloader-protected-count: %d\n", ret);
		return ret;
	}

	/* bank count is optional and will be initialized to 1 if not found */
	ret = of_property_read_u32(node, "oculus,flash-bank-count", &mcudata->flash_info.bank_count);
	if (ret < 0) {
		mcudata->flash_info.bank_count = 1;
		ret = 0;
	}

	return 0;
}

static int swd_driver_init_single_target(struct device *dev, struct swd_mcu_data *mcudata, struct device_node *node, bool flash_params_required)
{
	int ret;

	ret = of_property_read_string(node, "oculus,swd-flavor", &mcudata->target_flavor);
	if (ret < 0) {
		dev_err(dev, "Failed to get swdflavor: %d", ret);
		return ret;
	}

	if (flash_params_required)
		ret = swd_driver_init_flash_parameters(dev, mcudata, node);

	if (ret < 0)
		return ret;

	ret = of_property_read_u32(node, "oculus,swd-mem-ap", &mcudata->mem_ap);
	if (ret < 0) {
		/* If no MEMAP is specified, we will use the first one as fallback. */
		mcudata->mem_ap = 0;
	}

	return fwupdate_init_swd_ops(dev, mcudata);
}

static int swd_driver_init_dev_data(struct swd_dev_data *devdata, struct device *dev)
{
	int ret;
	int child_count;
	int child_index;
	struct device_node *child = NULL;
	struct device_node *mcu_node = NULL;
	struct device_node *node = dev->of_node;
	struct device_node *parent_node = of_get_parent(node);
	const char *swdflavor;



	if (parent_node &&
	    of_device_is_compatible(parent_node, "oculus,syncboss")) {
		struct swd_ops *ops = dev_get_drvdata(dev->parent);

		devdata->is_busy = ops->is_busy;
	}

	mutex_init(&devdata->state_mutex);

	ret = of_property_read_string(node, "oculus,swd-flavor", &swdflavor);
	if (ret < 0) {
		dev_err(dev, "Failed to get swdflavor: %d\n", ret);
		return ret;
	}

	devdata->gpio_reset = of_get_named_gpio(node, "oculus,pin-reset", 0);
	if (devdata->gpio_reset == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	devdata->gpio_swdclk = of_get_named_gpio(node, "oculus,swd-clk", 0);
	if (devdata->gpio_swdclk == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	devdata->gpio_swdio = of_get_named_gpio(node, "oculus,swd-io", 0);
	if (devdata->gpio_swdio == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if ((devdata->gpio_swdclk < 0) || (devdata->gpio_swdio < 0)) {
		dev_err(dev, "Need swdclk, swdio GPIOs to update firmware\n");
		return -EINVAL;
	}

	mcu_node = of_get_child_by_name(node, "oculus,mcus");
	ret = of_property_read_string(node, "oculus,fw-path", &devdata->mcu_data.fw_path);
	if (ret < 0 && !mcu_node) {
		dev_err(dev, "Failed to get fw-path: %d\n", ret);
		return ret;
	}

	if (ret == 0 && mcu_node) {
		dev_err(dev, "Both single and multi-core schemas specified.");
		return -EINVAL;
	}

	devdata->erase_all = of_property_read_bool(node, "oculus,flash-erase-all");
	devdata->swd_provisioning = of_property_read_bool(node, "oculus,swd-provisioning");

	if (mcu_node) {
		child_count = of_get_child_count(mcu_node);
		devdata->num_children = child_count;
		dev_info(dev, "Creating %d child dev nodes", child_count);
		devdata->child_mcu_data = devm_kcalloc(dev, child_count, sizeof(*(devdata->child_mcu_data)), GFP_KERNEL);
		if (!(devdata->child_mcu_data))
			return -ENOMEM;

		child_index = 0;
		for_each_child_of_node(mcu_node, child) {
			dev_err(dev, "Name: %s", child->name);
			of_property_read_string(child, "oculus,fw-path", &devdata->child_mcu_data[child_index].fw_path);
			ret = swd_driver_init_single_target(dev, &devdata->child_mcu_data[child_index], child, true);
			if (ret < 0)
				return ret;
			child_index++;
		}
	}

	/* Regulator is optional and will be initialized to NULL if not found */
	devm_fw_init_regulator(dev, &devdata->swd_core, "swd-core");

	ret = swd_driver_init_single_target(dev, &devdata->mcu_data, node, !mcu_node);
	if (ret < 0)
		return ret;

#ifdef CONFIG_OCULUS_SWD_DEBUG
	ret = fwupdate_create_debugfs(dev, swdflavor);
	if (ret < 0)
		return ret;
#endif

	devdata->workqueue = create_singlethread_workqueue(
		"fwupdate_workqueue");
	if (!devdata->workqueue) {
		dev_err(dev, "Could not create work queue");
		return -ENOMEM;
	}

	return 0;
}

static DEVICE_ATTR(update_status, S_IRUGO,
	swd_driver_update_firmware_show, NULL);
static DEVICE_ATTR(update_firmware, S_IRUGO | S_IWUSR,
	swd_driver_update_firmware_show, swd_driver_update_firmware_store);

static struct attribute *swd_attr[] = {
	&dev_attr_update_status.attr,
	&dev_attr_update_firmware.attr,
	NULL
};

static const struct attribute_group m_swd_gr = {
	.attrs = swd_attr
};

static int swd_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct swd_dev_data *devdata;
	int rc;

	devdata = kzalloc(sizeof(struct swd_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);

	rc = swd_driver_init_dev_data(devdata, dev);
	if (rc < 0)
		goto error_after_init_dev_data;

	rc = sysfs_create_group(&dev->kobj, &m_swd_gr);
	if (rc) {
		dev_err(dev, "device_create_file failed\n");
		goto error_after_create_group;
	}
	return 0;

error_after_create_group:
	destroy_workqueue(devdata->workqueue);
error_after_init_dev_data:
	kfree(devdata);

	return rc;
}

static int swd_driver_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	sysfs_remove_group(&dev->kobj, &m_swd_gr);
#ifdef CONFIG_OCULUS_SWD_DEBUG
	if (fwupdate_remove_debugfs(dev))
		dev_err(dev, "Error removing debugfs nodes");
#endif

	destroy_workqueue(devdata->workqueue);

	kfree(devdata);

	return 0;
}


#ifdef CONFIG_OF /*Open firmware must be defined for dts useage*/
static const struct of_device_id oculus_swd_table[] = {
	{ .compatible = "oculus,swd",}, /*Compatible node must match dts*/
	{ },
};
#else
#	 define oculus_swd_table NULL
#endif

struct platform_driver oculus_swd_driver = {
	.driver = {
		.name = "oculus_swd",
		.owner = THIS_MODULE,
		.of_match_table = oculus_swd_table
	},
	.probe = swd_driver_probe,
	.remove = swd_driver_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&oculus_swd_driver,
};

static int __init swd_driver_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit swd_driver_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(swd_driver_init);
module_exit(swd_driver_exit);
MODULE_DESCRIPTION("Oculus SWD FW-Update driver");
MODULE_LICENSE("GPL v2");
