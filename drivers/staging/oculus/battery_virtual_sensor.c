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
	struct thermal_zone_device *tzd;
	struct virtual_sensor_common_data data;

	struct power_supply *usb_psy;

	struct thermal_zone_device *pcm_tz;
	int pcm_tz_scaling_factor;

	int max_differential;

	bool was_connected;
	bool use_pcm_therm_with_charger;
};

static int get_pcm_temp(struct battery_virtual_sensor_data *vs, s64 *tz_temp)
{
	int ret;
	int temp;

	ret = thermal_zone_get_temp(vs->pcm_tz, &temp);
	if (ret != 0) {
		dev_warn(vs->dev,
				"Error getting temperature: %s (%d)",
				vs->pcm_tz->type, ret);
		return ret;
	}

	*tz_temp = (s64)temp * (s64)vs->pcm_tz_scaling_factor;
	return 0;
}

static int get_temp(void *data, int *temperature)
{
	struct battery_virtual_sensor_data *vs = data;
	s64 tz_temp = 0, iio_temp = 0, temp, pcm_temp = 0;
	int ret;

	*temperature = 0;

	ret = get_pcm_temp(vs, &pcm_temp);
	if (ret != 0)
		return ret;

	mutex_lock(&vs->data.lock);

	/* Use PCM thermistor if charger is present */
	if (vs->use_pcm_therm_with_charger && is_charger_connected(vs->usb_psy)) {
		vs->was_connected = true;

		*temperature = (int)pcm_temp;
		ret = 0;
		goto get_temp_unlock;
	}

	if (vs->was_connected) {
		/* Zero out history upon disconnection to avoid sudden jumps */
		virtual_sensor_reset_history(&vs->data);

		vs->was_connected = false;
	}

	ret = virtual_sensor_calculate_tz_temp(vs->dev, &vs->data, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	ret = virtual_sensor_calculate_iio_temp(vs->dev, &vs->data, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	temp = div64_s64(tz_temp + iio_temp, COEFFICIENT_SCALAR);

	/* Account for charging */
	temp += (s64)vs->data.intercept_constant_discharging;

	/* If virtual sensor temp greatly differs from PCM, use PCM */
	if (abs(temp - pcm_temp) > vs->max_differential)
		*temperature = (int)pcm_temp;
	else
		*temperature = (int)temp;

	ret = 0;

get_temp_unlock:
	mutex_unlock(&vs->data.lock);
	return ret;
}

static const struct thermal_zone_of_device_ops virtual_sensor_thermal_ops = {
	.get_temp = get_temp,
};

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

	mutex_init(&vs->data.lock);

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

	ret = virtual_sensor_parse_common_dt(vs->dev, &vs->data);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse common data: %d", ret);
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

	ret = sysfs_create_groups(&pdev->dev.kobj, battery_virtual_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct virtual_sensor_common_data *data =
			(struct virtual_sensor_common_data *) platform_get_drvdata(pdev);
	struct battery_virtual_sensor_data *vs =
			(struct battery_virtual_sensor_data *) data->parent;

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, battery_virtual_sensor_groups);

	mutex_destroy(&data->lock);

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
