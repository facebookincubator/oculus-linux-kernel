// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/syncboss/consumer.h>

#include "syncboss_timesync.h"

static irqreturn_t timesync_irq_handler(int irq, void *p)
{
	const u64 timestamp_ns = ktime_to_ns(ktime_get());
	struct timesync_dev_data *devdata = (struct timesync_dev_data *) p;

	atomic64_set(&devdata->last_te_timestamp_ns, timestamp_ns);

	return IRQ_HANDLED;
}

static ssize_t te_timestamp_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct timesync_dev_data *devdata =
	(struct timesync_dev_data *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			(s64)atomic64_read(&devdata->last_te_timestamp_ns));
}
static DEVICE_ATTR_RO(te_timestamp);

static int syncboss_timesync_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct timesync_dev_data *devdata = dev_get_drvdata(dev);
	struct device_node *parent_node = of_get_parent(node);
	int ret = 0;

	if (!parent_node || !of_device_is_compatible(parent_node, "meta,syncboss-spi")) {
		dev_err(dev, "failed to find compatible parent device");
		return -ENODEV;
	}

	devdata = devm_kzalloc(dev, sizeof(struct timesync_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	dev_set_drvdata(dev, devdata);
	devdata->dev = dev;

	atomic64_set(&devdata->last_te_timestamp_ns, ktime_to_ns(ktime_get()));

	devdata->timesync_irq = platform_get_irq_byname(pdev, "timesync");
	if (devdata->timesync_irq < 0) {
		dev_err(dev, "No timesync IRQ specified");
		return -EINVAL;
	}

	ret = devm_request_irq(dev, devdata->timesync_irq,
			timesync_irq_handler,
			IRQF_TRIGGER_RISING,
			"timesync", devdata);
	if (ret < 0) {
		dev_err(dev, "failed to get timesync irq, error %d", ret);
		return ret;
	}

	ret = sysfs_create_file(&dev->kobj, &dev_attr_te_timestamp.attr);
	if (ret) {
		dev_err(dev, "failed to create sysfs file %d", ret);
		return ret;
	}

	return ret;
}

static int syncboss_timesync_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	return sysfs_create_file(&dev->kobj, &dev_attr_te_timestamp.attr);
}

#ifdef CONFIG_OF
static const struct of_device_id syncboss_timesync_match_table[] = {
	{ .compatible = "meta,syncboss-timesync", },
	{ },
};
#else
#define syncboss_timesync_match_table NULL
#endif

struct platform_driver syncboss_timesync_driver = {
	.driver = {
		.name = "syncboss_timesync",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_timesync_match_table
	},
	.probe = syncboss_timesync_probe,
	.remove = syncboss_timesync_remove,
};

static struct platform_driver * const platform_drivers[] = {
	&syncboss_timesync_driver,
};

static int __init syncboss_timesync_init(void)
{
	return platform_register_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

static void __exit syncboss_timesync_exit(void)
{
	platform_unregister_drivers(platform_drivers,
		ARRAY_SIZE(platform_drivers));
}

module_init(syncboss_timesync_init);
module_exit(syncboss_timesync_exit);
MODULE_DESCRIPTION("Syncboss Timesync Event Driver");
MODULE_LICENSE("GPL v2");
