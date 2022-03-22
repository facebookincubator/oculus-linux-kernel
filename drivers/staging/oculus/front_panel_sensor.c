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
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/types.h>

#include "virtual_sensor_utils.h"

#define COEFFICIENT_SCALAR 10000

struct front_panel_sensor_data {
	struct device *dev;

	struct power_supply *usb_psy;

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

	int intercept_constant_charging;
	int intercept_constant_discharging;

	struct mutex lock;
};

static int front_panel_get_temp(void *data,
					int *temperature)
{
	int i, ret, tz_temp = 0, iio_temp = 0;
	int tz_scaling_factors[THERMAL_MAX_VIRT_SENSORS];
	int iio_scaling_factors[THERMAL_MAX_VIRT_SENSORS];
	struct front_panel_sensor_data *fp = data;

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&fp->lock);

	for (i = 0; i < fp->tz_count; i++)
		tz_scaling_factors[i] = 1;

	ret = virtual_sensor_calculate_tz_temp(fp->dev, fp->tzs,
			fp->tz_coefficients, fp->tz_slope_coefficients,
			tz_scaling_factors, fp->tz_count,
			fp->tz_last_temperatures, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	for (i = 0; i < fp->iio_count; i++)
		iio_scaling_factors[i] = 1;

	ret = virtual_sensor_calculate_iio_temp(fp->dev, fp->iios,
			fp->iio_coefficients, fp->iio_slope_coefficients,
			iio_scaling_factors, fp->iio_count,
			fp->iio_last_temperatures, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	*temperature += tz_temp;
	*temperature += iio_temp;

	*temperature /= COEFFICIENT_SCALAR;

	/* Account for charging */
	*temperature += (is_charger_connected(fp->usb_psy) ?
		fp->intercept_constant_charging :
		fp->intercept_constant_discharging);

	ret = 0;

get_temp_unlock:
	mutex_unlock(&fp->lock);
	return ret;
}

static const struct thermal_zone_of_device_ops front_panel_thermal_ops = {
	.get_temp = front_panel_get_temp,
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
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(fp->tz_coefficients, fp->tz_count, buf);
	mutex_unlock(&fp->lock);

	return ret;
}

static ssize_t tz_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(fp->tz_coefficients, fp->tz_count, buf);
	mutex_unlock(&fp->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tz_coefficients);

static ssize_t tz_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(fp->tz_slope_coefficients, fp->tz_count, buf);
	mutex_unlock(&fp->lock);

	return ret;
}

static ssize_t tz_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(fp->tz_slope_coefficients, fp->tz_count, buf);
	mutex_unlock(&fp->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(tz_slope_coefficients);

static ssize_t iio_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(fp->iio_coefficients, fp->iio_count, buf);
	mutex_unlock(&fp->lock);

	return ret;
}

static ssize_t iio_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(fp->iio_slope_coefficients, fp->iio_count, buf);
	mutex_unlock(&fp->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(iio_coefficients);

static ssize_t iio_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(fp->iio_coefficients, fp->iio_count, buf);
	mutex_unlock(&fp->lock);

	return ret;
}

static ssize_t iio_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(fp->iio_slope_coefficients, fp->iio_count, buf);
	mutex_unlock(&fp->lock);
	if (ret < 0)
		return ret;

	return count;
}
static DEVICE_ATTR_RW(iio_slope_coefficients);

static ssize_t charging_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", fp->intercept_constant_charging);
	mutex_unlock(&fp->lock);

	return ret;
}

static ssize_t charging_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	int ret, constant;

	ret = kstrtoint_from_user(buf, count, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	fp->intercept_constant_charging = constant;
	mutex_unlock(&fp->lock);

	return count;
}
static DEVICE_ATTR_RW(charging_constant);

static ssize_t discharging_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", fp->intercept_constant_discharging);
	mutex_unlock(&fp->lock);

	return ret;
}

static ssize_t discharging_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct front_panel_sensor_data *fp =
		(struct front_panel_sensor_data *) dev_get_drvdata(dev);
	int ret, constant;

	ret = kstrtoint_from_user(buf, count, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&fp->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	fp->intercept_constant_discharging = constant;
	mutex_unlock(&fp->lock);

	return count;
}
static DEVICE_ATTR_RW(discharging_constant);

static struct attribute *front_panel_sensor_attrs[] = {
	&dev_attr_tz_coefficients.attr,
	&dev_attr_tz_slope_coefficients.attr,
	&dev_attr_iio_coefficients.attr,
	&dev_attr_iio_slope_coefficients.attr,
	&dev_attr_charging_constant.attr,
	&dev_attr_discharging_constant.attr,
	NULL,
};
ATTRIBUTE_GROUPS(front_panel_sensor);

static int front_panel_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct front_panel_sensor_data *fp;
	struct thermal_zone_device *tzd = NULL;

	dev_dbg(&pdev->dev, "probing");

	fp = devm_kzalloc(&pdev->dev, sizeof(*fp), GFP_KERNEL);
	if (!fp)
		return -ENOMEM;

	fp->dev = &pdev->dev;

	mutex_init(&fp->lock);

	ret = virtual_sensor_parse_thermal_zones_dt(&pdev->dev, fp->tzs,
			&fp->tz_count);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(
			pdev->dev.of_node, "thermal-zone-coefficients",
			fp->tz_coefficients, fp->tz_count);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to parse thermal-zone-coefficients: %d",
			ret);
		return ret;
	}

	ret = of_property_read_u32_array(
			pdev->dev.of_node, "thermal-zone-slope-coefficients",
			fp->tz_slope_coefficients, fp->tz_count);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to parse thermal-zone-slope-coefficients: %d",
			ret);
		return ret;
	}

	ret = virtual_sensor_parse_iio_channels_dt(&pdev->dev, fp->iios,
			&fp->iio_count);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node, "io-coefficients",
			fp->iio_coefficients, fp->iio_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse io-coefficients: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32_array(
			pdev->dev.of_node, "io-slope-coefficients",
			fp->iio_slope_coefficients, fp->iio_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse io-slope-coefficients: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "intercept-charging",
			&fp->intercept_constant_charging);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse intercept-charging: %d",
				ret);
		return ret;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "intercept-discharging",
			&fp->intercept_constant_discharging);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse intercept-discharging: %d",
				ret);
		return ret;
	}

	fp->usb_psy = power_supply_get_by_name("usb");
	if (!fp->usb_psy) {
		dev_warn(&pdev->dev, "Unable to get charger power_supply\n");
		return -EPROBE_DEFER;
	}

	tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, fp,
			&front_panel_thermal_ops);
	if (IS_ERR(tzd)) {
		ret = PTR_ERR(tzd);
		dev_err(&pdev->dev, "Sensor register error: %d\n",
			ret);
		return ret;
	}
	fp->tzd = tzd;

	dev_set_drvdata(&pdev->dev, fp);

	ret = sysfs_create_groups(&pdev->dev.kobj, front_panel_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int front_panel_sensor_remove(struct platform_device *pdev)
{
	struct front_panel_sensor_data *fp = platform_get_drvdata(pdev);

	thermal_zone_of_sensor_unregister(&pdev->dev, fp->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, front_panel_sensor_groups);

	mutex_destroy(&fp->lock);

	return 0;
}

static const struct of_device_id front_panel_sensor_table[] = {
	{ .compatible = "oculus,front-panel-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, front_panel_sensor_table);

static struct platform_driver front_panel_sensor_driver = {
	.probe = front_panel_sensor_probe,
	.remove = front_panel_sensor_remove,
	.driver = {
		.name = "front-panel-sensor",
		.of_match_table = front_panel_sensor_table,
	},
};

static int __init front_panel_sensor_init(void)
{
	pr_info("%s: Initializing\n", __func__);
	return platform_driver_register(&front_panel_sensor_driver);
}
late_initcall(front_panel_sensor_init);

static void __exit front_panel_sensor_deinit(void)
{
	platform_driver_unregister(&front_panel_sensor_driver);
}
module_exit(front_panel_sensor_deinit);

MODULE_ALIAS("front_panel_sensor");
MODULE_LICENSE("GPL v2");
