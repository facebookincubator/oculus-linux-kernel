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

struct battery_virtual_sensor_data {
	struct device *dev;

	struct power_supply *usb_psy;

	struct thermal_zone_device *tzd;

	struct thermal_zone_device *pcm_tz;
	int pcm_tz_scaling_factor;

	struct thermal_zone_device *tzs[THERMAL_MAX_VIRT_SENSORS];
	struct iio_channel *iios[THERMAL_MAX_VIRT_SENSORS];

	/* store last temperatures as part of the formula */
	int tz_last_temperatures[THERMAL_MAX_VIRT_SENSORS];
	int iio_last_temperatures[THERMAL_MAX_VIRT_SENSORS];

	/* Scaling factor scales to millidegrees */
	int tz_scaling_factors[THERMAL_MAX_VIRT_SENSORS];
	int iio_scaling_factors[THERMAL_MAX_VIRT_SENSORS];

	/* scaled by COEFFICIENT_SCALAR */
	int tz_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int tz_slope_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int iio_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int iio_slope_coefficients[THERMAL_MAX_VIRT_SENSORS];

	int tz_count;
	int iio_count;

	int intercept_constant_discharging;

	int max_differential;

	struct mutex lock;

	bool was_connected;
	bool use_pcm_therm_with_charger;
};

static int get_pcm_temp(struct battery_virtual_sensor_data *vs)
{
	int tz_temp;
	int ret;

	ret = thermal_zone_get_temp(vs->pcm_tz, &tz_temp);
	if (ret < 0) {
		dev_warn(vs->dev,
				"Error getting temperature: %s (%d)",
				vs->pcm_tz->type, ret);
		return ret;
	}

	return tz_temp * vs->pcm_tz_scaling_factor;
}

static int get_temp(void *data, int *temperature)
{
	int ret, pcm_temp = 0, tz_temp = 0, iio_temp = 0;
	struct battery_virtual_sensor_data *vs = data;

	*temperature = 0;

	pcm_temp = get_pcm_temp(vs);
	if (pcm_temp < 0)
		return pcm_temp;

	/* Use PCM thermistor if charger is present */
	if (vs->use_pcm_therm_with_charger &&
	    is_charger_connected(vs->usb_psy)) {
		vs->was_connected = true;

		*temperature = pcm_temp;

		return 0;
	}

	if (vs->was_connected) {
		/* Zero out history upon disconnection to avoid sudden jumps */
		memset(vs->tz_last_temperatures, 0,
				sizeof(vs->tz_last_temperatures));
		memset(vs->iio_last_temperatures, 0,
				sizeof(vs->iio_last_temperatures));
		vs->was_connected = false;
	}

	mutex_lock(&vs->lock);

	ret = virtual_sensor_calculate_tz_temp(vs->dev, vs->tzs,
			vs->tz_coefficients, vs->tz_slope_coefficients,
			vs->tz_scaling_factors, vs->tz_count,
			vs->tz_last_temperatures, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	ret = virtual_sensor_calculate_iio_temp(vs->dev, vs->iios,
			vs->iio_coefficients, vs->iio_slope_coefficients,
			vs->iio_scaling_factors, vs->iio_count,
			vs->iio_last_temperatures, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	*temperature += tz_temp;
	*temperature += iio_temp;

	*temperature /= COEFFICIENT_SCALAR;

	/* Account for charging */
	*temperature += vs->intercept_constant_discharging;

	/* If virtual sensor temp greatly differs from PCM, use PCM */
	if (abs(*temperature - pcm_temp) > vs->max_differential)
		*temperature = pcm_temp;

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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
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

static ssize_t discharging_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", vs->intercept_constant_discharging);
	mutex_unlock(&vs->lock);

	return ret;
}

static ssize_t discharging_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct battery_virtual_sensor_data *vs =
		(struct battery_virtual_sensor_data *) dev_get_drvdata(dev);
	int ret, constant;

	ret = kstrtoint_from_user(buf, count, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	vs->intercept_constant_discharging = constant;
	mutex_unlock(&vs->lock);

	return count;
}
static DEVICE_ATTR_RW(discharging_constant);

static struct attribute *battery_virtual_sensor_attrs[] = {
	&dev_attr_tz_coefficients.attr,
	&dev_attr_tz_slope_coefficients.attr,
	&dev_attr_iio_coefficients.attr,
	&dev_attr_iio_slope_coefficients.attr,
	&dev_attr_discharging_constant.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_virtual_sensor);

static int virtual_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct battery_virtual_sensor_data *vs;
	struct thermal_zone_device *tzd = NULL;
	const char *pcm_tz_name;

	dev_dbg(&pdev->dev, "probing");

	vs = devm_kzalloc(&pdev->dev, sizeof(*vs), GFP_KERNEL);
	if (!vs)
		return -ENOMEM;

	vs->dev = &pdev->dev;

	mutex_init(&vs->lock);

	ret = of_property_read_string(pdev->dev.of_node, "pcm-thermal-zone",
			&pcm_tz_name);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to read pcm-thermal-zone: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"pcm-thermal-zone-scaling-factor",
			&vs->pcm_tz_scaling_factor);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to read pcm-thermal-zone-scaling-factor: %d",
			ret);
		return ret;
	}

	vs->pcm_tz = thermal_zone_get_zone_by_name(pcm_tz_name);
	if (IS_ERR(vs->pcm_tz)) {
		dev_err(&pdev->dev, "sensor %s get_zone error: %d",
				pcm_tz_name, ret);
		return -EPROBE_DEFER;
	}

	ret = virtual_sensor_parse_thermal_zones_dt(&pdev->dev, vs->tzs,
			&vs->tz_count);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node,
			"thermal-zone-scaling-factors", vs->tz_scaling_factors,
			vs->tz_count);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to parse thermal-zone-scaling-factors: %d", ret);
		return ret;
	}

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
	if (ret < 0 && ret != -EINVAL)
		return ret;

	ret = of_property_read_u32_array(pdev->dev.of_node,
			"io-scaling-factors", vs->iio_scaling_factors,
			vs->iio_count);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(&pdev->dev,
			"Failed to parse io-scaling-factors: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(pdev->dev.of_node, "io-coefficients",
			vs->iio_coefficients, vs->iio_count);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(&pdev->dev, "Failed to parse io-coefficients: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(
			pdev->dev.of_node, "io-slope-coefficients",
			vs->iio_slope_coefficients, vs->iio_count);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(&pdev->dev, "Failed to parse io-slope-coefficients: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "intercept-discharging",
			&vs->intercept_constant_discharging);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse intercept-discharging: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "max-differential",
			&vs->max_differential);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse max-differential: %d",
				ret);
		return ret;
	}

	vs->use_pcm_therm_with_charger =
		!of_property_read_bool(pdev->dev.of_node,
			"dont-use-pcm-with-charger");

	vs->usb_psy = power_supply_get_by_name("usb");
	if (!vs->usb_psy) {
		dev_warn(&pdev->dev, "Unable to get charger power_supply");
		return -EPROBE_DEFER;
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

	ret = sysfs_create_groups(&pdev->dev.kobj, battery_virtual_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct battery_virtual_sensor_data *vs = platform_get_drvdata(pdev);

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, battery_virtual_sensor_groups);

	mutex_destroy(&vs->lock);

	return 0;
}

static const struct of_device_id virtual_sensor_table[] = {
	{ .compatible = "oculus,battery-virtual-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, virtual_sensor_table);

static struct platform_driver virtual_sensor_driver = {
	.probe = virtual_sensor_probe,
	.remove = virtual_sensor_remove,
	.driver = {
		.name = "battery-virtual-sensor",
		.of_match_table = virtual_sensor_table,
	},
};

static int __init virtual_sensor_init(void)
{
	pr_debug("%s: Initializing\n", __func__);
	return platform_driver_register(&virtual_sensor_driver);
}
subsys_initcall(virtual_sensor_init);

static void __exit virtual_sensor_deinit(void)
{
	platform_driver_unregister(&virtual_sensor_driver);
}
module_exit(virtual_sensor_deinit);

MODULE_ALIAS("battery_virtual_sensor");
MODULE_LICENSE("GPL v2");
