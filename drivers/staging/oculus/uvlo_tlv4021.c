// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2023, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/sysfs.h>
#include <linux/atomic.h>

MODULE_DESCRIPTION("uvlo tlv4021 driver");

struct tlv4021_info {
	struct  device  *dev;
	int tlv4021_irq_gpio;
	atomic_t uvlo_count;
};

static ssize_t uvlo_count_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tlv4021_info *data = dev_get_drvdata(dev);

	if (!data) {
		dev_err(dev, "tlv4021_info is NULL\n");
		return -EINVAL;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->uvlo_count));
}

static DEVICE_ATTR(uvlo_count, 0644, uvlo_count_show, NULL);

static struct attribute *tlv4021_attrs[] = {
	&dev_attr_uvlo_count.attr,
	NULL
};

ATTRIBUTE_GROUPS(tlv4021);

static irqreturn_t tlv4021_irq_handler(int irq, void *dev_id)
{
	struct tlv4021_info *data = (struct tlv4021_info *)dev_id;

	atomic_inc(&data->uvlo_count);
	return IRQ_HANDLED;
}

static int tlv4021_probe(struct platform_device *pdev)
{
	struct tlv4021_info *data;
	int rc = 0;

	if (!pdev) {
		dev_err(&pdev->dev, "Platform device pointer is NULL\n");
		return -ENOMEM;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "Failed to allocate device: %d\n", -ENOMEM);
		return -ENOMEM;
	}

	data->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, data);
	atomic_set(&data->uvlo_count, 0);

	data->tlv4021_irq_gpio = of_get_named_gpio(data->dev->of_node,
						"tlv4021,irq-gpio", 0);
	if (!gpio_is_valid(data->tlv4021_irq_gpio)) {
		dev_err(data->dev, "tlv4021 irq gpio is invalid\n");
		return -EINVAL;
	}
	rc = devm_gpio_request_one(data->dev, data->tlv4021_irq_gpio,
				GPIOF_IN, "uvlo-tlv4021-irq");
	if (rc) {
		dev_err(data->dev, "tlv4021 irq gpio failed\n");
		return rc;
	}
	rc = devm_request_threaded_irq(data->dev,
				gpio_to_irq(data->tlv4021_irq_gpio),
				NULL, tlv4021_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"uvlo_tlv4021_irq", data);
	if (rc) {
		dev_err(data->dev, "tlv4021 failed to request hpi interrupt\n");
		return rc;
	}

	rc = sysfs_create_groups(&data->dev->kobj, tlv4021_groups);
	if (rc) {
		dev_err(data->dev, "Could not create tlv4021_uvlo_count sysfs, error: %d\n", rc);
		return rc;
	}

	return rc;
}

static int tlv4021_remove(struct platform_device *pdev)
{
	struct tlv4021_info *data;

	if (!pdev) {
		dev_err(&pdev->dev, "NULL platform device pointer\n");
		return -EINVAL;
	}

	data = (struct tlv4021_info *)platform_get_drvdata(pdev);

	if (!data) {
		dev_err(&pdev->dev, "Null platform device pointer\n");
		return -EINVAL;
	}

	sysfs_remove_groups(&data->dev->kobj, tlv4021_groups);

	return 0;
}

static const struct of_device_id uvlo_tlv4021_table[] = {
	{ .compatible = "meta,uvlo-tlv4021" },
	{}
};

MODULE_DEVICE_TABLE(of, uvlo_tlv4021_table);

static struct platform_driver tlv4021_uvlo_driver = {
	.probe = tlv4021_probe,
	.remove = tlv4021_remove,
	.driver = {
		.name = "uvlo-tlv4021",
		.of_match_table = uvlo_tlv4021_table,
	},
};

static int __init tlv4021_init(void)
{
	return platform_driver_register(&tlv4021_uvlo_driver);
}

static void __exit tlv4021_deinit(void)
{
	platform_driver_unregister(&tlv4021_uvlo_driver);
}

module_init(tlv4021_init);
module_exit(tlv4021_deinit);

MODULE_ALIAS("platform:uvlo-tlv4021");
MODULE_LICENSE("GPL v2");
