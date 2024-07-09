// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/types.h>

struct iio_channel_sensor_data {
	struct device *dev;
	struct thermal_zone_device *tzd;
	struct iio_channel *iio;
	struct mutex lock;
};

static int iio_channel_get_temp(void *data, int *temperature)
{
	struct iio_channel_sensor_data *d = data;
	int iio_temp = 0, ret;

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&d->lock);
	ret = iio_read_channel_processed(d->iio, &iio_temp);
	if (ret < 0) {
		dev_warn(d->dev, "%s: error getting temp: %d",
					d->iio->indio_dev->name, ret);
		goto get_temp_unlock;
	}
	*temperature = iio_temp;
	ret = 0;

get_temp_unlock:
	mutex_unlock(&d->lock);
	return ret;
}

static const struct thermal_zone_of_device_ops iio_channel_thermal_ops = {
	.get_temp = iio_channel_get_temp,
};


static struct attribute *iio_channel_sensor_attrs[] = {
	NULL,
};
ATTRIBUTE_GROUPS(iio_channel_sensor);

static int parse_iio_channel_dt(struct device *dev,
		struct iio_channel **channel_ptr)
{
	int ret = 0;
	const char *temp_string = NULL;

	ret = of_property_read_string(dev->of_node, "io-channel-names",
			&temp_string);
	if (ret < 0) {
		dev_err(dev, "Failed to read io-channel-names: %d",
				ret);
		return ret;
	}

	*channel_ptr = iio_channel_get(dev, temp_string);
	if (IS_ERR(*channel_ptr)) {
		ret = -EPROBE_DEFER;
		dev_err(dev, "channel %s iio_channel_get error: %ld",
			temp_string,
			PTR_ERR(*channel_ptr));
		return ret;
	}

	return 0;
}

static int iio_channel_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct iio_channel_sensor_data *d;
	struct thermal_zone_device *tzd = NULL;

	dev_dbg(&pdev->dev, "probing");

	d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	d->dev = &pdev->dev;

	mutex_init(&d->lock);

	ret = parse_iio_channel_dt(d->dev, &d->iio);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse iio channel data: %d", ret);
		return ret;
	}

	tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, d,
			&iio_channel_thermal_ops);
	if (IS_ERR(tzd)) {
		ret = PTR_ERR(tzd);
		dev_err(&pdev->dev, "Sensor register error: %d\n",
			ret);
		return ret;
	}
	d->tzd = tzd;

	dev_set_drvdata(&pdev->dev, d);

	ret = sysfs_create_groups(&pdev->dev.kobj, iio_channel_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int iio_channel_sensor_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct iio_channel_sensor_data *vs = dev_get_drvdata(dev);

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, iio_channel_sensor_groups);

	mutex_destroy(&vs->lock);

	return 0;
}

static const struct of_device_id iio_channel_sensor_table[] = {
	{ .compatible = "oculus,iio-channel-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, iio_channel_sensor_table);

static struct platform_driver iio_channel_sensor_driver = {
	.probe = iio_channel_sensor_probe,
	.remove = iio_channel_sensor_remove,
	.driver = {
		.name = "iio-channel-sensor",
		.of_match_table = iio_channel_sensor_table,
	},
};

static int __init iio_channel_sensor_init(void)
{
	return platform_driver_register(&iio_channel_sensor_driver);
}
late_initcall(iio_channel_sensor_init);

static void __exit iio_channel_sensor_deinit(void)
{
	platform_driver_unregister(&iio_channel_sensor_driver);
}
module_exit(iio_channel_sensor_deinit);

MODULE_ALIAS("iio_channel_sensor");
MODULE_LICENSE("GPL v2");
