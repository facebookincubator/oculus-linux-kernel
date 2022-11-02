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
	struct thermal_zone_device *tzd;
	struct virtual_sensor_common_data data;

	struct power_supply *usb_psy;
};

static int front_panel_get_temp(void *data, int *temperature)
{
	struct front_panel_sensor_data *fp = data;
	s64 tz_temp = 0, iio_temp = 0, temp;
	int ret;

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&fp->data.lock);

	ret = virtual_sensor_calculate_tz_temp(fp->dev, &fp->data, &tz_temp);
	if (ret)
		goto get_temp_unlock;

	ret = virtual_sensor_calculate_iio_temp(fp->dev, &fp->data, &iio_temp);
	if (ret)
		goto get_temp_unlock;

	temp = div64_s64(tz_temp + iio_temp, COEFFICIENT_SCALAR);

	/* Account for charging */
	temp += (s64)(is_charger_connected(fp->usb_psy) ?
		fp->data.intercept_constant_charging :
		fp->data.intercept_constant_discharging);

	*temperature = (int)temp;
	ret = 0;

get_temp_unlock:
	mutex_unlock(&fp->data.lock);
	return ret;
}

static const struct thermal_zone_of_device_ops front_panel_thermal_ops = {
	.get_temp = front_panel_get_temp,
};

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

	mutex_init(&fp->data.lock);

	ret = virtual_sensor_parse_common_dt(fp->dev, &fp->data);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse common data: %d", ret);
		return ret;
	}

	fp->usb_psy = power_supply_get_by_name("usb");
	if (!fp->usb_psy) {
		dev_warn(&pdev->dev, "Unable to get charger power_supply\n");
		return -EPROBE_DEFER;
	}

	virtual_sensor_reset_history(&fp->data);

	tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, fp,
			&front_panel_thermal_ops);
	if (IS_ERR(tzd)) {
		ret = PTR_ERR(tzd);
		dev_err(&pdev->dev, "Sensor register error: %d\n",
			ret);
		return ret;
	}
	fp->tzd = tzd;

	dev_set_drvdata(&pdev->dev, &fp->data);

	ret = sysfs_create_groups(&pdev->dev.kobj, front_panel_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	return ret;
}

static int front_panel_sensor_remove(struct platform_device *pdev)
{
	struct virtual_sensor_common_data *data =
			(struct virtual_sensor_common_data *) platform_get_drvdata(pdev);
	struct front_panel_sensor_data *vs =
			(struct front_panel_sensor_data *) data->parent;

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, front_panel_sensor_groups);

	mutex_destroy(&data->lock);

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
