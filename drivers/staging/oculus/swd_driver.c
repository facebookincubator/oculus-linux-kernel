#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#include "fw_helpers.h"
#include "swd.h"

static ssize_t show_update_firmware(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return fwupdate_show_update_firmware(dev, buf);
}

static ssize_t store_update_firmware(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return fwupdate_store_update_firmware(dev, buf, count);
}

static int init_swd_dev_data(struct swd_dev_data *devdata, struct device *dev)
{
	int ret;
	struct device_node *node = dev->of_node;
	struct device_node *parent_node = of_get_parent(node);
	const char *swdflavor;

	devdata->workqueue = create_singlethread_workqueue(
		"fwupdate_workqueue");

	/*
	 * If this SWD device is associated with syncboss, then we must make
	 * sure reset syncboss following an SWD firmware update. Set up a
	 * callback into the syncboss device to accomplish this.
	 */
	if (parent_node &&
	    of_device_is_compatible(parent_node, "oculus,syncboss")) {
		struct swd_ops *ops = dev_get_drvdata(dev->parent);

		devdata->on_firmware_update_complete = ops->fw_update_cb;
	}

	mutex_init(&devdata->state_mutex);

	ret = of_property_read_string(node,
		"oculus,fw-path", &devdata->fw_path);
	if (ret < 0) {
		dev_err(dev, "Failed to get fw-path: %d\n", ret);
		return ret;
	}

	devdata->gpio_swdclk = of_get_named_gpio(node, "oculus,swd-clk", 0);
	devdata->gpio_swdio = of_get_named_gpio(node, "oculus,swd-io", 0);
	if ((devdata->gpio_swdclk < 0) || (devdata->gpio_swdio < 0)) {
		dev_err(dev, "Need swdclk, swdio GPIOs to update firmware\n");
		return -EINVAL;
	}

	ret = of_property_read_string(node, "oculus,swd-flavor", &swdflavor);
	if (ret < 0) {
		dev_err(dev, "Failed to get swdflavor: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "oculus,flash-block-size",
				   &devdata->flash_info.block_size);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-block-size: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "oculus,flash-page-size",
				   &devdata->flash_info.page_size);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-page-size: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(node, "oculus,flash-page-count",
				   &devdata->flash_info.num_pages);
	if (ret < 0) {
		dev_err(dev, "Failed to get flash-page-count: %d\n", ret);
		return ret;
	}

	/* Regulator is optional and will be initialized to NULL if not found */
	fw_init_regulator(dev, &devdata->swd_core, "swd-core");

	ret = fwupdate_init_swd_ops(dev, swdflavor);
	if (ret < 0)
		return ret;

	return ret;
}

static DEVICE_ATTR(update_status, S_IRUGO,
	show_update_firmware, NULL);
static DEVICE_ATTR(update_firmware, S_IRUGO | S_IWUSR,
	show_update_firmware, store_update_firmware);

static struct attribute *swd_attr[] = {
	&dev_attr_update_status.attr,
	&dev_attr_update_firmware.attr,
	NULL
};

static const struct attribute_group m_swd_gr = {
	.attrs = swd_attr
};

static int mod_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct swd_dev_data *devdata;
	int rc;

	devdata = kzalloc(sizeof(struct swd_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);

	rc = init_swd_dev_data(devdata, dev);
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

static int mod_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct swd_dev_data *devdata = dev_get_drvdata(dev);

	sysfs_remove_group(&dev->kobj, &m_swd_gr);

	destroy_workqueue(devdata->workqueue);

	regulator_put(devdata->swd_core);

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
	.probe = mod_probe,
	.remove = mod_remove,
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
