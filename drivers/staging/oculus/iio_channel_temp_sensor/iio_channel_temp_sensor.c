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

/**
 * struct iio_channel_temp_map_pt - Map the graph representation for ADC channel
 * @x: Represent the ADC digitized code.
 * @y: Represent the physical data which can be temperature, voltage,
 *     resistance.
 */
struct iio_channel_temp_map_pt {
	s32 x;
	s32 y;
};

/*
 * Resistor to temperature table for 100k pull up for NCP03WF104F05RL.
 */
static const struct iio_channel_temp_map_pt adcmap_104f_Rref[] = {
	{4397119, -40000},	/* ohm, milliCelsius */
	{3088599, -35000},
	{2197225, -30000},
	{1581881, -25000},
	{1151037, -20000},
	{846579, -15000},
	{628988, -10000},
	{471632, -5000},
	{357012, 0},
	{272500, 5000},
	{209710, 10000},
	{162651, 15000},
	{127080, 20000},
	{100000, 25000},
	{79222, 30000},
	{63167, 35000},
	{50677, 40000},
	{40904, 45000},
	{33195, 50000},
	{27091, 55000},
	{22224, 60000},
	{18323, 65000},
	{15184, 70000},
	{12635, 75000},
	{10566, 80000},
	{8873, 85000},
	{7481, 90000},
	{6337, 95000},
	{5384, 100000},
	{4594, 105000},
	{3934, 110000},
	{3380, 115000},
	{2916, 120000},
	{2522, 125000}
};

struct iio_channel_temp_sensor_data {
	struct device *dev;
	struct thermal_zone_device *tzd;
	struct iio_channel *iio;
	struct mutex lock;
	enum iio_chan_type type;
	int pull_up_res;
	int voltage_ref;
};

static int iio_channel_temp_map_resistor_temp(const struct iio_channel_temp_map_pt *pts,
				u32 tablesize, s32 input, int *output)
{
	bool descending = 1;
	u32 i = 0;

	if (!pts)
		return -EINVAL;

	if (tablesize > 1) {
		if (pts[0].x < pts[1].x)
			descending = 0;
	}

	while (i < tablesize) {
		if ((descending) && (pts[i].x < input)) {
			break;
		} else if ((!descending) &&
				(pts[i].x > input)) {
			break;
		}
		i++;
	}

	if (i == 0) {
		*output = pts[0].y;
	} else if (i == tablesize) {
		*output = pts[tablesize - 1].y;
	} else {
		*output = (((s32)((pts[i].y - pts[i - 1].y) *
			(input - pts[i - 1].x)) /
			(pts[i].x - pts[i - 1].x)) +
			pts[i - 1].y);
	}

	return 0;
}

static int iio_channel_temp_get_temp(void *data, int *temperature)
{
	struct iio_channel_temp_sensor_data *d = data;
	int iio_temp, ret;
	int r_ntc;
	int iio_value;

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&d->lock);

	ret = iio_read_channel_processed(d->iio, &iio_value);
	if (ret < 0) {
		dev_warn(d->dev, "%s: error getting iio value: %d",
					d->iio->indio_dev->name, ret);
		goto get_temp_unlock;
	}
	if (d->type == IIO_TEMP) {
		*temperature = iio_value;
		ret = 0;
		goto get_temp_unlock;
	}

	/*
	 * Rpullup and Rntc connected in series,
	 *
	 * iio_voltage_adc       Vref
	 * --------------- = ------------
	 *       Rntc        Rpullup+Rntc
	 *
	 * Rntc = Rpullup*iio_voltage_adc/(Vref-iio_voltage_adc)
	 */

	r_ntc = div64_s64(d->pull_up_res * iio_value,
			(d->voltage_ref - iio_value));
	ret = iio_channel_temp_map_resistor_temp(adcmap_104f_Rref,
			ARRAY_SIZE(adcmap_104f_Rref),r_ntc, &iio_temp);

	*temperature = iio_temp;
	ret = 0;

get_temp_unlock:
	mutex_unlock(&d->lock);
	return ret;
}

static const struct thermal_zone_of_device_ops iio_channel_temp_thermal_ops = {
	.get_temp = iio_channel_temp_get_temp,
};

static struct attribute *iio_channel_temp_sensor_attrs[] = {
	NULL,
};
ATTRIBUTE_GROUPS(iio_channel_temp_sensor);

static int iio_control_parse_dt(struct iio_channel_temp_sensor_data *data)
{
	int ret = 0;
	const char *temp_string = NULL;

	ret = of_property_read_string(data->dev->of_node, "io-channel-names",
			&temp_string);
	if (ret < 0) {
		dev_err(data->dev, "Failed to read io-channel-names: %d",
				ret);
		return ret;
	}

	data->iio = iio_channel_get(data->dev, temp_string);
	if (IS_ERR(data->iio)) {
		ret = -EPROBE_DEFER;
		dev_err(data->dev, "channel %s iio_channel_temp get error: %ld",
			temp_string,
			PTR_ERR(data->iio));
		return ret;
	}

	data->type = IIO_VOLTAGE;
	if (of_property_read_bool(data->dev->of_node, "iio-temp")) {
		data->type = IIO_TEMP;
		return 0;
	}

	ret = of_property_read_u32(data->dev->of_node, "pull-up-res",
			&data->pull_up_res);
	if (ret) {
		dev_err(data->dev, "Failed to read pull-up-res: %d\n",
				ret);
		return ret;
	}
	ret = of_property_read_u32(data->dev->of_node, "voltage-ref",
                        &data->voltage_ref);
        if (ret) {
                dev_err(data->dev, "Failed to read voltage-ref: %d\n",
                                ret);
                return ret;
        }

	return 0;
}

static int iio_channel_temp_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct iio_channel_temp_sensor_data *data;
	struct thermal_zone_device *tzd = NULL;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &pdev->dev;

	mutex_init(&data->lock);

	ret = iio_control_parse_dt(data);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to parse dt: %d", ret);
		return ret;
        }
	tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, data,
			&iio_channel_temp_thermal_ops);
	if (IS_ERR(tzd)) {
		ret = PTR_ERR(tzd);
		dev_err(&pdev->dev, "Sensor register error: %d\n",
			ret);
		return ret;
	}
	data->tzd = tzd;

	dev_set_drvdata(&pdev->dev, data);

	ret = sysfs_create_groups(&pdev->dev.kobj, iio_channel_temp_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files\n");

	dev_info(&pdev->dev, "iio_channel_temp_sensor probe done");
	return ret;
}

static int iio_channel_temp_sensor_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct iio_channel_temp_sensor_data *vs = dev_get_drvdata(dev);

	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);

	sysfs_remove_groups(&pdev->dev.kobj, iio_channel_temp_sensor_groups);

	mutex_destroy(&vs->lock);

	return 0;
}

static const struct of_device_id iio_channel_temp_sensor_table[] = {
	{ .compatible = "meta,iio-channel-temp-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, iio_channel_temp_sensor_table);

static struct platform_driver iio_channel_temp_sensor_driver = {
	.probe = iio_channel_temp_sensor_probe,
	.remove = iio_channel_temp_sensor_remove,
	.driver = {
		.name = "iio-channel-temp-sensor",
		.of_match_table = iio_channel_temp_sensor_table,
	},
};

static int __init iio_channel_temp_sensor_init(void)
{
	return platform_driver_register(&iio_channel_temp_sensor_driver);
}
late_initcall(iio_channel_temp_sensor_init);

static void __exit iio_channel_temp_sensor_deinit(void)
{
	platform_driver_unregister(&iio_channel_temp_sensor_driver);
}
module_exit(iio_channel_temp_sensor_deinit);

MODULE_ALIAS("iio_channel_temp_sensor");
MODULE_LICENSE("GPL v2");
