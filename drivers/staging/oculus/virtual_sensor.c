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

static int get_fallback_temp(struct virtual_sensor_drvdata *vs, s64 *tz_temp)
{
	int temp;
	int ret;

	ret = thermal_zone_get_temp(vs->fallback_tzd, &temp);
	if (ret != 0)
		return ret;

	*tz_temp = (s64)temp * (s64)vs->fallback_tz_scaling_factor;
	return 0;
}

static int virtual_sensor_get_temp(void *data, int *temperature)
{
	struct virtual_sensor_drvdata *vs = data;
	struct virtual_sensor_common_data *coeff_data;
	s64 tz_temp = 0, iio_temp = 0;
	s64 temp, fallback_temp = 0;
	const bool charging = is_charging(vs->batt_psy);
	int ret;

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&vs->lock);

	if (charging != vs->was_charging) {
		/* Zero out history upon disconnection to avoid sudden jumps */
		virtual_sensor_reset_history(&vs->data_charging);
		virtual_sensor_reset_history(&vs->data_discharging);
	}
	vs->was_charging = charging;

	coeff_data = charging ? &vs->data_charging : &vs->data_discharging;

	ret = virtual_sensor_calculate_tz_temp(vs->dev, coeff_data, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	ret = virtual_sensor_calculate_iio_temp(vs->dev, coeff_data, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	temp = div64_s64(tz_temp + iio_temp, COEFFICIENT_SCALAR);
	temp += coeff_data->intercept;

	*temperature = (int)temp;
	ret = 0;

	if (!vs->fallback_tzd)
		goto get_temp_unlock;

	ret = get_fallback_temp(vs, &fallback_temp);
	if (ret != 0)
		goto get_temp_unlock;

	/* If virtual sensor temp greatly differs from fallback, use fallback */
	if (abs(temp - fallback_temp) > vs->fallback_tolerance)
		*temperature = (int)fallback_temp;

get_temp_unlock:
	mutex_unlock(&vs->lock);
	return ret;
}

static const struct thermal_zone_of_device_ops virtual_sensor_thermal_ops = {
	.get_temp = virtual_sensor_get_temp,
};

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
static DEVICE_ATTR_RW(fallback_tolerance);

static struct attribute *virtual_sensor_attrs[] = {
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
	&dev_attr_fallback_tolerance.attr,
	NULL,
};
ATTRIBUTE_GROUPS(virtual_sensor);

static int virtual_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct virtual_sensor_drvdata *vs;
	struct thermal_zone_device *tzd = NULL;
	const char *fallback_tz_name;

	dev_dbg(&pdev->dev, "probing");

	vs = devm_kzalloc(&pdev->dev, sizeof(*vs), GFP_KERNEL);
	if (!vs)
		return -ENOMEM;

	vs->dev = &pdev->dev;

	mutex_init(&vs->lock);

	vs->batt_psy = power_supply_get_by_name("battery");
	if (!vs->batt_psy)
		return -EPROBE_DEFER;

	ret = of_property_read_string(pdev->dev.of_node,
			"fallback-thermal-zone", &fallback_tz_name);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "No fallback tz specified, won't use one");
		goto no_fallback;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"fallback-thermal-zone-scaling-factor",
			&vs->fallback_tz_scaling_factor);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "No fallback tz scaling factor specified, using 1");
		vs->fallback_tz_scaling_factor = 1;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"fallback-tolerance", &vs->fallback_tolerance);
	if (ret < 0) {
		dev_err(&pdev->dev, "must specify a fallback tolerance if using one");
		return ret;
	}

	vs->fallback_tzd = thermal_zone_get_zone_by_name(fallback_tz_name);
	if (IS_ERR(vs->fallback_tzd)) {
		dev_dbg(&pdev->dev, "failed getting fallback tz \'%s\': rc=%ld",
				fallback_tz_name, PTR_ERR(vs->fallback_tzd));
		return -EPROBE_DEFER;
	}

no_fallback:
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

	ret = sysfs_create_groups(&pdev->dev.kobj, virtual_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct virtual_sensor_drvdata *vs =
			(struct virtual_sensor_drvdata *) platform_get_drvdata(pdev);

	virtual_sensor_workqueue_unregister(&vs->data_charging);
	virtual_sensor_workqueue_unregister(&vs->data_discharging);

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	power_supply_put(vs->batt_psy);

	sysfs_remove_groups(&pdev->dev.kobj, virtual_sensor_groups);

	mutex_destroy(&vs->lock);

	return 0;
}

static const struct of_device_id virtual_sensor_table[] = {
	{ .compatible = "oculus,virtual-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, virtual_sensor_table);

static struct platform_driver virtual_sensor_driver = {
	.probe = virtual_sensor_probe,
	.remove = virtual_sensor_remove,
	.driver = {
		.name = "virtual-sensor",
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

MODULE_ALIAS("virtual_sensor");
MODULE_LICENSE("GPL v2");
