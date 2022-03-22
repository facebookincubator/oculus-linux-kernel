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

	struct thermal_zone_device *tzs[THERMAL_MAX_VIRT_SENSORS];
	struct iio_channel *iios[THERMAL_MAX_VIRT_SENSORS];

	/* store last temperatures as part of the formula */
	int tz_last_temperatures[THERMAL_MAX_VIRT_SENSORS];
	int iio_last_temperatures[THERMAL_MAX_VIRT_SENSORS];

	/* scaled by COEFFICIENT_SCALAR */
	int tz_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int tz_slope_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int iio_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int iio_slope_coefficients[THERMAL_MAX_VIRT_SENSORS];

	int tz_count;
	int iio_count;

	int intercept_constant;

	struct mutex lock;
};

static int tz_scaling_factors[THERMAL_MAX_VIRT_SENSORS];
static int iio_scaling_factors[THERMAL_MAX_VIRT_SENSORS];

static int get_temp(void *data, int *temperature)
{
	int ret, tz_temp = 0, iio_temp = 0;
	struct display_virtual_sensor_data *vs = data;

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&vs->lock);

	ret = virtual_sensor_calculate_tz_temp(vs->dev, vs->tzs,
			vs->tz_coefficients, vs->tz_slope_coefficients,
			tz_scaling_factors, vs->tz_count,
			vs->tz_last_temperatures, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	ret = virtual_sensor_calculate_iio_temp(vs->dev, vs->iios,
			vs->iio_coefficients, vs->iio_slope_coefficients,
			iio_scaling_factors, vs->iio_count,
			vs->iio_last_temperatures, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	*temperature += tz_temp;
	*temperature += iio_temp;

	*temperature /= COEFFICIENT_SCALAR;

	*temperature += vs->intercept_constant;

	ret = 0;

get_temp_unlock:
	mutex_unlock(&vs->lock);
	return ret;
}

static const struct thermal_zone_of_device_ops virtual_sensor_thermal_ops = {
	.get_temp = get_temp,
};

static ssize_t show_coefficients(int *coefficients, int num_coefficients,
		char *buf)
{
	int i, len = 0;

	for (i = 0; i < num_coefficients; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d",
				coefficients[i]);
		if (i == (num_coefficients - 1))
			len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
		else
			len += scnprintf(buf + len, PAGE_SIZE - len, " ");
	}

	return len;
}

static int store_coefficients(int *coefficients, int num_coefficients, const char *buf)
{
	int coeffs[THERMAL_MAX_VIRT_SENSORS];
	int ret;

	/* THERMAL_MAX_VIRT_SENSORS is 10, so allow up to 10 inputs */
	ret = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",
			&coeffs[0], &coeffs[1], &coeffs[2], &coeffs[3],
			&coeffs[4], &coeffs[5], &coeffs[6], &coeffs[7],
			&coeffs[8], &coeffs[9]);
	if (ret != num_coefficients)
		return -EINVAL;

	memcpy(coefficients, coeffs, sizeof(int) * num_coefficients);

	return 0;
}

static ssize_t tz_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(vs->tz_coefficients, vs->tz_count, buf);
	mutex_unlock(&vs->lock);

	return ret;
}

static ssize_t tz_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(vs->tz_coefficients, vs->tz_count, buf);
	mutex_unlock(&vs->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tz_coefficients);

static ssize_t tz_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(vs->tz_slope_coefficients, vs->tz_count, buf);
	mutex_unlock(&vs->lock);

	return ret;
}

static ssize_t tz_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(vs->tz_slope_coefficients, vs->tz_count, buf);
	mutex_unlock(&vs->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tz_slope_coefficients);

static ssize_t iio_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(vs->iio_coefficients, vs->iio_count, buf);
	mutex_unlock(&vs->lock);

	return ret;
}

static ssize_t iio_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(vs->iio_slope_coefficients, vs->iio_count, buf);
	mutex_unlock(&vs->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(iio_coefficients);

static ssize_t iio_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(vs->iio_coefficients, vs->iio_count, buf);
	mutex_unlock(&vs->lock);

	return ret;
}

static ssize_t iio_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(vs->iio_slope_coefficients, vs->iio_count, buf);
	mutex_unlock(&vs->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(iio_slope_coefficients);

static ssize_t intercept_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", vs->intercept_constant);
	mutex_unlock(&vs->lock);

	return ret;
}

static ssize_t intercept_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct display_virtual_sensor_data *vs =
		(struct display_virtual_sensor_data *) dev_get_drvdata(dev);
	int ret, constant;

	ret = kstrtoint_from_user(buf, count, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	vs->intercept_constant = constant;
	mutex_unlock(&vs->lock);

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
	int i;
	int ret = 0;

	dev_dbg(&pdev->dev, "probing");

	vs = devm_kzalloc(&pdev->dev, sizeof(*vs), GFP_KERNEL);
	if (!vs)
		return -ENOMEM;

	vs->dev = &pdev->dev;

	mutex_init(&vs->lock);

	for (i = 0; i < THERMAL_MAX_VIRT_SENSORS; i++) {
		tz_scaling_factors[i] = 1;
		iio_scaling_factors[i] = 1;
	}

	ret = virtual_sensor_parse_thermal_zones_dt(&pdev->dev, vs->tzs,
			&vs->tz_count);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node, "thermal-zone-coefficients",
			vs->tz_coefficients, vs->tz_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse thermal-zone-coefficients: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(
			pdev->dev.of_node, "thermal-zone-slope-coefficients",
			vs->tz_slope_coefficients, vs->tz_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse thermal-zone-slope-coefficients: %d",
				ret);
		return ret;
	}

	ret = virtual_sensor_parse_iio_channels_dt(&pdev->dev, vs->iios,
			&vs->iio_count);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node, "io-coefficients",
			vs->iio_coefficients, vs->iio_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse io-coefficients: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(
			pdev->dev.of_node, "io-slope-coefficients",
			vs->iio_slope_coefficients, vs->iio_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse io-slope-coefficients: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "intercept-constant",
			&vs->intercept_constant);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse intercept-constant: %d",
				ret);
		return ret;
	}

	tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, vs,
			&virtual_sensor_thermal_ops);
	if (IS_ERR(tzd)) {
		ret = PTR_ERR(tzd);
		dev_err(&pdev->dev, "Sensor register error: %d",
			ret);
		return ret;
	}
	vs->tzd = tzd;

	dev_set_drvdata(&pdev->dev, vs);

	ret = sysfs_create_groups(&pdev->dev.kobj, display_virtual_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct display_virtual_sensor_data *vs = platform_get_drvdata(pdev);

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, display_virtual_sensor_groups);

	mutex_destroy(&vs->lock);

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
