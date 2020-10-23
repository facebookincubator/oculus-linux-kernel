// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/types.h>

#define COEFFICIENT_SCALAR 10000

struct front_panel_sensor_data {
	struct device *dev;

	struct power_supply *batt_psy;

	struct thermal_zone_device *tzd;
	struct thermal_zone_device *zones[THERMAL_MAX_VIRT_SENSORS];

	/* Due to tz limitations, soc thermistor is read from iio */
	struct iio_channel *soc_adc;

	/* store last temperatures as part of the formula */
	int last_temperatures[THERMAL_MAX_VIRT_SENSORS];

	/* scaled by COEFFICIENT_SCALAR */
	int coefficients[THERMAL_MAX_VIRT_SENSORS];
	int slope_coefficients[THERMAL_MAX_VIRT_SENSORS];

	int sensor_count;
	int intercept_constant_charging;
	int intercept_constant_discharging;
};

static int get_intercept_term(struct front_panel_sensor_data *fp)
{
	union power_supply_propval val = {0};

	if (fp->batt_psy &&
			!power_supply_get_property(fp->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &val) &&
			val.intval == POWER_SUPPLY_STATUS_CHARGING)
		return fp->intercept_constant_charging;

	return fp->intercept_constant_discharging;
}

/*
 * The front panel temperature at time t is a linear combination of
 * the intercept constant, sensor temperatures at time t, and the
 * slope of sensor temperatures. Where the slope is defined as
 * sensor(t) - sensor(t-1)
 *
 * VirtualSensor(t) = intercept + sensor1_coefficient*sensor1(t) + ...
 *   + sensorN_coefficient*(t) +
 *   sensor1_slopeCoefficient*( sensor1(t) - sensor1(t-1) ) + ...
 *   + sensorN_slopeCoefficient*( sensorN(t) - sensorN(t-1) )
 */
static int front_panel_get_temp(void *data,
					int *temperature)
{
	int idx = 0, ret, temp;
	struct front_panel_sensor_data *fp = data;

	*temperature = 0;

	/* Thermal zones */
	for (idx = 0; idx < fp->sensor_count - 1; idx++) {
		ret = thermal_zone_get_temp(fp->zones[idx], &temp);
		if (ret) {
			dev_warn(fp->dev, "Error getting temperature: %s (%d)",
					fp->zones[idx]->type, ret);
			return ret;
		}

		/* Combine terms for each sensor reading */
		*temperature += fp->coefficients[idx] * temp;
		*temperature += fp->slope_coefficients[idx] *
			(temp - fp->last_temperatures[idx]);

		fp->last_temperatures[idx] = temp;
	}

	/* iio channels */
	ret = iio_read_channel_processed(fp->soc_adc, &temp);
	if (ret < 0) {
		dev_warn(fp->dev, "Error getting temperature: soc (%d", ret);
		return ret;
	}

	*temperature += fp->coefficients[idx] * temp;
	*temperature += fp->slope_coefficients[idx] *
		(temp - fp->last_temperatures[idx]);

	fp->last_temperatures[idx] = temp;

	*temperature /= COEFFICIENT_SCALAR;

	/* Account for charging */
	*temperature += get_intercept_term(fp);

	return 0;
}

static const struct thermal_zone_of_device_ops front_panel_thermal_ops = {
	  .get_temp = front_panel_get_temp,
};

static int front_panel_sensor_parse_thermal_dt(struct platform_device *pdev,
		struct front_panel_sensor_data *fp)
{
	int count, idx, ret = 0;
	const char **temp_string = NULL;

	count = of_property_count_strings(pdev->dev.of_node, "thermal-zones");
	if (count < 0) {
		dev_err(&pdev->dev, "No sensors configured, thermal-zones");
		return -ENODATA;
	}
	fp->sensor_count = count;

	temp_string = kcalloc(count, sizeof(char *),  GFP_KERNEL);
	if (!temp_string)
		return -ENOMEM;

	ret = of_property_read_string_array(pdev->dev.of_node, "thermal-zones",
				temp_string, fp->sensor_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to read thermal-zones: %d", ret);
		goto out;
	}

	for (idx = 0; idx < fp->sensor_count; idx++) {
		fp->zones[idx] =
			thermal_zone_get_zone_by_name(temp_string[idx]);
		if (IS_ERR(fp->zones[idx])) {
			ret = -EPROBE_DEFER;
			dev_err(&pdev->dev, "sensor %s get_zone error: %ld\n",
				temp_string[idx],
				PTR_ERR(fp->zones[idx]));
			goto out;
		}
	}

out:
	kfree(temp_string);
	return ret;
}

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

	ret = front_panel_sensor_parse_thermal_dt(pdev, fp);
	if (ret < 0)
		return ret;

	fp->soc_adc = iio_channel_get(&pdev->dev, "soc_temp");
	if (IS_ERR(fp->soc_adc)) {
		dev_err(&pdev->dev, "Sensor soc_temp channel_get error: %ld\n",
				PTR_ERR(fp->soc_adc));
		return -EPROBE_DEFER;
	}
	fp->sensor_count += 1;

	ret = of_property_read_u32_array(pdev->dev.of_node, "coefficients",
			fp->coefficients, fp->sensor_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse coefficients: %d", ret);
		return ret;
	}
	ret = of_property_read_u32_array(
			pdev->dev.of_node, "slope-coefficients",
			fp->slope_coefficients, fp->sensor_count);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse slope-coefficients: %d",
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
			&fp->intercept_constant_charging);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse intercept-discharging: %d",
				ret);
		return ret;
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

	fp->batt_psy = power_supply_get_by_name("battery");
	if (!fp->batt_psy) {
		dev_warn(&pdev->dev, "Unable to get battery power_supply\n");
		ret = -EPROBE_DEFER;
	}

	return ret;
}

static int front_panel_sensor_remove(struct platform_device *pdev)
{
	struct front_panel_sensor_data *fp = platform_get_drvdata(pdev);

	thermal_zone_of_sensor_unregister(&pdev->dev, fp->tzd);

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
subsys_initcall(front_panel_sensor_init);

static void __exit front_panel_sensor_deinit(void)
{
	platform_driver_unregister(&front_panel_sensor_driver);
}
module_exit(front_panel_sensor_deinit);

MODULE_ALIAS("front_panel_sensor");
MODULE_LICENSE("GPL v2");
