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
	struct thermal_zone_device *pcm_tz;
	int pcm_tz_scaling_factor;

	int max_differential;

	bool was_charging;
};

static int get_pcm_temp(struct virtual_sensor_drvdata *vs, s64 *tz_temp)
{
	int ret;
	int temp;
	struct battery_virtual_sensor_data *battery_vs = vs->data;

	ret = thermal_zone_get_temp(battery_vs->pcm_tz, &temp);
	if (ret != 0) {
		dev_warn(vs->dev,
				"Error getting temperature: %s (%d)",
				battery_vs->pcm_tz->type, ret);
		return ret;
	}

	*tz_temp = (s64)temp * (s64)battery_vs->pcm_tz_scaling_factor;
	return 0;
}

static int get_temp(void *data, int *temperature)
{
	struct virtual_sensor_drvdata *vs = data;
	struct battery_virtual_sensor_data *battery_vs = vs->data;
	struct virtual_sensor_common_data *coeff_data;
	s64 tz_temp = 0, iio_temp = 0, temp, pcm_temp = 0;
	const bool charging = is_charging();
	int ret;

	*temperature = 0;

	ret = get_pcm_temp(vs, &pcm_temp);
	if (ret != 0)
		return ret;

	mutex_lock(&vs->lock);

	if (!charging && battery_vs->was_charging) {
		/* Zero out history upon disconnection to avoid sudden jumps */
		virtual_sensor_reset_history(&vs->data_charging);
		virtual_sensor_reset_history(&vs->data_discharging);
	}

	if (charging) {
		battery_vs->was_charging = true;
		coeff_data = &vs->data_charging;
	} else {
		battery_vs->was_charging = false;
		coeff_data = &vs->data_discharging;
	}

	ret = virtual_sensor_calculate_tz_temp(vs->dev, coeff_data, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	ret = virtual_sensor_calculate_iio_temp(vs->dev, coeff_data, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	temp = div64_s64(tz_temp + iio_temp, COEFFICIENT_SCALAR);
	temp += coeff_data->intercept;

	/* If virtual sensor temp greatly differs from PCM, use PCM */
	if (abs(temp - pcm_temp) > battery_vs->max_differential)
		*temperature = (int)pcm_temp;
	else
		*temperature = (int)temp;

	ret = 0;

get_temp_unlock:
	mutex_unlock(&vs->lock);
	return ret;
}

static const struct thermal_zone_of_device_ops virtual_sensor_thermal_ops = {
	.get_temp = get_temp,
};

ssize_t max_differential_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *vs = dev_get_drvdata(dev);
	struct battery_virtual_sensor_data *battery_vs = vs->data;
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", battery_vs->max_differential);
	mutex_unlock(&vs->lock);

	return ret;
}

ssize_t max_differential_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *vs = dev_get_drvdata(dev);
	struct battery_virtual_sensor_data *battery_vs = vs->data;
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}

	if (kstrtoint(buf, 10, &battery_vs->max_differential))
		return -EINVAL;
	mutex_unlock(&vs->lock);

	return count;
}

static DEVICE_ATTR_RW(tz_coefficients_discharging);
static DEVICE_ATTR_RW(tz_slope_coefficients_discharging);
static DEVICE_ATTR_RW(iio_coefficients_discharging);
static DEVICE_ATTR_RW(iio_slope_coefficients_discharging);
static DEVICE_ATTR_RW(tz_coefficients_charging);
static DEVICE_ATTR_RW(tz_slope_coefficients_charging);
static DEVICE_ATTR_RW(iio_coefficients_charging);
static DEVICE_ATTR_RW(iio_slope_coefficients_charging);
static DEVICE_ATTR_RW(intercept_charging);
static DEVICE_ATTR_RW(intercept_discharging);
static DEVICE_ATTR_RW(max_differential);

static struct attribute *battery_virtual_sensor_attrs[] = {
	&dev_attr_tz_coefficients_discharging.attr,
	&dev_attr_tz_slope_coefficients_discharging.attr,
	&dev_attr_iio_coefficients_discharging.attr,
	&dev_attr_iio_slope_coefficients_discharging.attr,
	&dev_attr_tz_coefficients_charging.attr,
	&dev_attr_tz_slope_coefficients_charging.attr,
	&dev_attr_iio_coefficients_charging.attr,
	&dev_attr_iio_slope_coefficients_charging.attr,
	&dev_attr_intercept_discharging.attr,
	&dev_attr_intercept_charging.attr,
	&dev_attr_max_differential.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_virtual_sensor);

static int virtual_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct virtual_sensor_drvdata *vs;
	struct battery_virtual_sensor_data *battery_vs;
	struct thermal_zone_device *tzd = NULL;
	const char *pcm_tz_name;

	dev_dbg(&pdev->dev, "probing");

	vs = devm_kzalloc(&pdev->dev, sizeof(*vs), GFP_KERNEL);
	if (!vs)
		return -ENOMEM;

	vs->dev = &pdev->dev;

	mutex_init(&vs->lock);

	battery_vs = devm_kzalloc(vs->dev, sizeof(*battery_vs), GFP_KERNEL);

	vs->data = battery_vs;
	if (!vs->data)
		return -ENOMEM;

	ret = of_property_read_string(pdev->dev.of_node, "pcm-thermal-zone",
			&pcm_tz_name);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to read pcm-thermal-zone: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"pcm-thermal-zone-scaling-factor",
			&battery_vs->pcm_tz_scaling_factor);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "No pcm-tz-scaling-factor specified, using 1");
		battery_vs->pcm_tz_scaling_factor = 1;
		ret = 0;
	}

	battery_vs->pcm_tz = thermal_zone_get_zone_by_name(pcm_tz_name);
	if (IS_ERR(battery_vs->pcm_tz)) {
		dev_dbg(&pdev->dev, "sensor %s get_zone error: %d",
				pcm_tz_name, ret);
		return -EPROBE_DEFER;
	}

	vs->data_charging.name = "charging";
	ret = virtual_sensor_parse_dt(vs->dev, &vs->data_charging);
	if (ret == -EPROBE_DEFER)
		return ret;
	else if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to parse data for sensor \"%s\": %d",
			vs->data_charging.name, ret);
		return ret;
	}

	vs->data_discharging.name = "discharging";
	ret = virtual_sensor_parse_dt(vs->dev, &vs->data_discharging);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to parse data for sensor \"%s\": %d",
			vs->data_charging.name, ret);
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "max-differential",
			&battery_vs->max_differential);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse max-differential: %d",
				ret);
		return ret;
	}

	virtual_sensor_reset_history(&vs->data_charging);
	virtual_sensor_reset_history(&vs->data_discharging);

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

	vs->data_charging.tzd = vs->tzd;
	vs->data_discharging.tzd = vs->tzd;
	virtual_sensor_workqueue_register(&vs->data_charging);
	virtual_sensor_workqueue_register(&vs->data_discharging);

	ret = sysfs_create_groups(&pdev->dev.kobj, battery_virtual_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct virtual_sensor_drvdata *vs =
			(struct virtual_sensor_drvdata *) platform_get_drvdata(pdev);

	virtual_sensor_workqueue_unregister(&vs->data_charging);
	virtual_sensor_workqueue_unregister(&vs->data_discharging);

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
late_initcall(virtual_sensor_init);

static void __exit virtual_sensor_deinit(void)
{
	platform_driver_unregister(&virtual_sensor_driver);
}
module_exit(virtual_sensor_deinit);

MODULE_ALIAS("battery_virtual_sensor");
MODULE_LICENSE("GPL v2");
