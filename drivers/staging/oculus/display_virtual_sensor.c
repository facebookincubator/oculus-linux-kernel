// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/types.h>

#include "virtual_sensor_utils.h"

#define COEFFICIENT_SCALAR 10000

struct display_virtual_sensor_data {
	struct device *dev;
	struct thermal_zone_device *tzd;
	struct virtual_sensor_common_data data;

	int intercept_constant;
};

static int get_temp(void *data, int *temperature)
{
	struct display_virtual_sensor_data *vs = data;
	s64 tz_temp = 0, iio_temp = 0, temp;
	int ret;

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&vs->data.lock);

	ret = virtual_sensor_calculate_tz_temp(vs->dev, &vs->data, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	ret = virtual_sensor_calculate_iio_temp(vs->dev, &vs->data, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	temp = div64_s64(tz_temp + iio_temp, COEFFICIENT_SCALAR);
	temp += (s64)vs->intercept_constant;

	*temperature = (int)temp;
	ret = 0;

get_temp_unlock:
	mutex_unlock(&vs->data.lock);
	return ret;
}

static const struct thermal_zone_of_device_ops virtual_sensor_thermal_ops = {
	.get_temp = get_temp,
};

static ssize_t intercept_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) data->parent;
	ssize_t ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", vs->intercept_constant);
	mutex_unlock(&data->lock);

	return ret;
}

static ssize_t intercept_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) data->parent;
	int ret, constant;

	ret = kstrtoint_from_user(buf, count, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	vs->intercept_constant = constant;
	mutex_unlock(&data->lock);

	return count;
}
static DEVICE_ATTR_RW(intercept_constant);

static struct attribute *display_virtual_sensor_attrs[] = {
	&dev_attr_tz_coefficients.attr,
	&dev_attr_tz_slope_coefficients.attr,
	&dev_attr_iio_coefficients.attr,
	&dev_attr_iio_slope_coefficients.attr,
	&dev_attr_intercept_constant.attr,
	NULL,
};
ATTRIBUTE_GROUPS(display_virtual_sensor);

static int virtual_sensor_probe(struct platform_device *pdev)
{
	struct display_virtual_sensor_data *vs;
	struct thermal_zone_device *tzd = NULL;
	int ret = 0;

	dev_dbg(&pdev->dev, "probing");

	vs = devm_kzalloc(&pdev->dev, sizeof(*vs), GFP_KERNEL);
	if (!vs)
		return -ENOMEM;

	vs->dev = &pdev->dev;

	mutex_init(&vs->data.lock);

	ret = virtual_sensor_parse_common_dt(vs->dev, &vs->data);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse common data: %d", ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "intercept-constant",
			&vs->intercept_constant);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse intercept-constant: %d",
				ret);
		return ret;
	}

	virtual_sensor_reset_history(&vs->data);

	tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, vs,
			&virtual_sensor_thermal_ops);
	if (IS_ERR(tzd)) {
		ret = PTR_ERR(tzd);
		dev_err(&pdev->dev, "Sensor register error: %d",
			ret);
		return ret;
	}
	vs->tzd = tzd;

	dev_set_drvdata(&pdev->dev, &vs->data);

	vs->data.tzd = vs->tzd;
	virtual_sensor_workqueue_register(&vs->data);

	ret = sysfs_create_groups(&pdev->dev.kobj, display_virtual_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct virtual_sensor_common_data *data =
			(struct virtual_sensor_common_data *) platform_get_drvdata(pdev);
	struct display_virtual_sensor_data *vs =
			(struct display_virtual_sensor_data *) data->parent;

	virtual_sensor_workqueue_unregister(&vs->data);

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, display_virtual_sensor_groups);

	mutex_destroy(&data->lock);

	return 0;
}

static const struct of_device_id virtual_sensor_table[] = {
	{ .compatible = "oculus,display-virtual-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, virtual_sensor_table);

static struct platform_driver virtual_sensor_driver = {
	.probe = virtual_sensor_probe,
	.remove = virtual_sensor_remove,
	.driver = {
		.name = "display-virtual-sensor",
		.of_match_table = virtual_sensor_table,
	},
};

static int __init display_virtual_sensor_init(void)
{
	pr_debug("%s: Initializing\n", __func__);
	return platform_driver_register(&virtual_sensor_driver);
}
late_initcall(display_virtual_sensor_init);

static void __exit display_virtual_sensor_deinit(void)
{
	platform_driver_unregister(&virtual_sensor_driver);
}
module_exit(display_virtual_sensor_deinit);

MODULE_ALIAS("display_virtual_sensor");
MODULE_LICENSE("GPL v2");
