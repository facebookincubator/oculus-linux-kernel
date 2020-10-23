#include "fwupdate_driver.h"

#include "fwupdate_manager.h"

#include "syncboss_common.h"
#include "syncboss_swd.h"

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/of_gpio.h>

struct fwupdate_dev_data {
	/* Data from fwupdate_manager */
	struct fwupdate_data fwudata;

	/* workqueue associated with device */
	struct workqueue_struct *workqueue;

	/* Regulator associated with swd interface */
	struct regulator *swd_core;
};

static ssize_t show_update_firmware(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fwupdate_dev_data *devdata = dev_get_drvdata(dev);
	struct fwupdate_data *fwudata = &devdata->fwudata;

	return fwupdate_show_update_firmware(dev, fwudata, buf);
}


static ssize_t store_update_firmware(struct device *dev,
				    struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct fwupdate_dev_data *devdata = dev_get_drvdata(dev);
	struct fwupdate_data *fwudata = &devdata->fwudata;

	return fwupdate_store_update_firmware(devdata->workqueue, dev,
		fwudata, buf, count);
}

static int init_swd_dev_data(
	struct fwupdate_dev_data *devdata, struct device *dev)
{
	int status = 0;
	struct fwupdate_data *fwudata = &devdata->fwudata;
	struct device_node *node = dev->of_node;
	const char *swdflavor;

	devdata->workqueue = create_singlethread_workqueue(
		"fwupdate_workqueue");

	fwudata->dev = dev;

	mutex_init(&fwudata->state_mutex);

	status = of_property_read_string(node,
		"oculus,fwpath", &fwudata->fw_path);
	if (status < 0) {
		dev_err(dev, "Failed to get oculus,fwpath: %d", status);
		return status;
	}

	fwudata->gpio_swdclk = of_get_named_gpio(node, "oculus,swdclk",
						0);
	fwudata->gpio_swdio = of_get_named_gpio(node, "oculus,swdio",
						0);

	if ((fwudata->gpio_swdclk < 0) ||
	    (fwudata->gpio_swdio < 0)) {
		dev_err(dev, "Need swdclk, swdio GPIOs to update firmware");
		return -EINVAL;
	}

	status = of_property_read_string(node, "oculus,swdflavor", &swdflavor);
	if (status < 0) {
		dev_err(dev, "Failed to get oculus,swdflavor: %d", status);
		return status;
	}

	status = fwupdate_init_swd_ops(dev, fwudata, swdflavor);
	if (status < 0)
		return status;

	dev_set_drvdata(dev, devdata);
	return 0;
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
	.name = "oculus_swd",
	.attrs = swd_attr
};

static int swd_probe(
	struct platform_device *pdev,
	struct fwupdate_dev_data *devdata)
{
	struct device *dev = &pdev->dev;

	int rc;

	rc = init_swd_dev_data(devdata, dev);
	if (rc < 0)
		goto error_after_init_dev_data;

	/* Regulator is optional */
	init_syncboss_regulator(dev, &devdata->swd_core, "swd-core");

	rc = sysfs_create_group(&dev->kobj, &m_swd_gr);
	if (rc) {
		dev_err(dev, "device_create_file failed\n");
		goto error_after_create_group;
	}
	return 0;

error_after_create_group:
	destroy_workqueue(devdata->workqueue);
error_after_init_dev_data:
	return rc;
}

static int mod_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fwupdate_dev_data *devdata = dev_get_drvdata(dev);

	sysfs_remove_group(&dev->kobj, &m_swd_gr);

	destroy_workqueue(devdata->workqueue);
	return 0;
}

static int mod_probe(struct platform_device *pdev)
{
	static struct fwupdate_dev_data devdata;

	return swd_probe(pdev, &devdata);
}

#ifdef CONFIG_OF /*Open firmware must be defined for dts useage*/
static const struct of_device_id oculus_swd_table[] = {
	{ .compatible = "oculus,swd",}, /*Compatible node must match dts*/
	{ },
};
#else
#	 define oculus_swd_table NULL
#endif

struct platform_driver oculus_fwupdate_driver = {
	.driver = {
		.name = "oculus_fwupdate",
		.owner = THIS_MODULE,
		.of_match_table = oculus_swd_table
	},
	.probe = mod_probe,
	.remove = mod_remove,
};

