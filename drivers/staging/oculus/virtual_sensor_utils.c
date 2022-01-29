// SPDX-License-Identifier: GPL-2.0-only

#include "virtual_sensor_utils.h"

#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/types.h>

int is_charger_connected(struct power_supply *usb_psy)
{
	union power_supply_propval val = {0};

	/*
	 * Check presence of USB charger, since it is possible to be connected
	 * to USB without charging but still draw power from the charger to run
	 * the system.
	 */
	return (usb_psy && !power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &val) &&
			val.intval != 0);
}

/*
 * The virtual sensor temperature at time t is a linear combination of
 * the intercept constant, sensor temperatures at time t, and the
 * slope of sensor temperatures. Where the slope is defined as
 * sensor(t) - sensor(t-1)
 *
 * VirtualSensor(t) = intercept + sensor1_coefficient*sensor1(t) + ...
 *   + sensorN_coefficient*(t) +
 *   sensor1_slopeCoefficient*( sensor1(t) - sensor1(t-1) ) + ...
 *   + sensorN_slopeCoefficient*( sensorN(t) - sensorN(t-1) )
 */
int virtual_sensor_calculate_tz_temp(struct device *dev,
		struct thermal_zone_device **zones, int *zone_coeffs,
		int *zone_slope_coeffs, int *zone_scaling_factors,
		int zone_count, int *last_temps, int *temperature)
{
	int i, ret, temp;

	for (i = 0; i < zone_count; i++) {
		ret = thermal_zone_get_temp(zones[i], &temp);
		if (ret) {
			dev_warn(dev, "%s: error getting temp: %d",
					zones[i]->type, ret);
			return ret;
		}

		temp *= zone_scaling_factors[i];

		*temperature += zone_coeffs[i] * temp;
		*temperature += zone_slope_coeffs[i] *
			(temp - last_temps[i]);

		last_temps[i] = temp;
	}

	return 0;
}

/*
 * The virtual sensor temperature at time t is a linear combination of
 * the intercept constant, sensor temperatures at time t, and the
 * slope of sensor temperatures. Where the slope is defined as
 * sensor(t) - sensor(t-1)
 *
 * VirtualSensor(t) = intercept + sensor1_coefficient*sensor1(t) + ...
 *   + sensorN_coefficient*(t) +
 *   sensor1_slopeCoefficient*( sensor1(t) - sensor1(t-1) ) + ...
 *   + sensorN_slopeCoefficient*( sensorN(t) - sensorN(t-1) )
 */
int virtual_sensor_calculate_iio_temp(struct device *dev,
		struct iio_channel **chans, int *chan_coeffs,
		int *chan_slope_coeffs, int *chan_scaling_factors,
		int chan_count, int *last_temps, int *temperature)
{
	int i, ret, temp;

	for (i = 0; i < chan_count; i++) {
		ret = iio_read_channel_processed(chans[i], &temp);
		if (ret < 0) {
			dev_warn(dev, "%s: error getting temp: %d",
					chans[i]->indio_dev->name, ret);
			return ret;
		}

		temp *= chan_scaling_factors[i];

		*temperature += chan_coeffs[i] * temp;
		*temperature += chan_slope_coeffs[i] *
			(temp - last_temps[i]);

		last_temps[i] = temp;
	}

	return 0;
}

int virtual_sensor_parse_thermal_zones_dt(struct device *dev,
		struct thermal_zone_device **zones, int *zone_count)
{
	int count, i, ret = 0;
	const char **temp_string = NULL;

	count = of_property_count_strings(dev->of_node, "thermal-zones");
	if (count < 0) {
		dev_err(dev, "Empty or invalid thermal-zones property");
		return -ENODATA;
	}

	*zone_count = count;

	temp_string = kcalloc(count, sizeof(char *),  GFP_KERNEL);
	if (!temp_string)
		return -ENOMEM;

	ret = of_property_read_string_array(dev->of_node, "thermal-zones",
				temp_string, *zone_count);
	if (ret < 0) {
		dev_err(dev, "Failed to read thermal-zones: %d", ret);
		goto out;
	}

	for (i = 0; i < *zone_count; i++) {
		zones[i] = thermal_zone_get_zone_by_name(temp_string[i]);
		if (IS_ERR(zones[i])) {
			ret = -EPROBE_DEFER;
			dev_err(dev, "sensor %s get_zone error: %ld",
				temp_string[i],
				PTR_ERR(zones[i]));
			goto out;
		}
	}

out:
	kfree(temp_string);
	return ret;
}

int virtual_sensor_parse_iio_channels_dt(struct device *dev,
		struct iio_channel **chans, int *chan_count)
{
	int count, i, ret = 0;
	const char **temp_string = NULL;

	count = of_property_count_strings(dev->of_node, "io-channel-names");
	if (count < 0) {
		dev_err(dev, "Empty or invalid io-channel-names property");
		return count;
	}

	*chan_count = count;

	temp_string = kcalloc(count, sizeof(char *), GFP_KERNEL);
	if (!temp_string)
		return -ENOMEM;

	ret = of_property_read_string_array(dev->of_node, "io-channel-names",
			temp_string, *chan_count);
	if (ret < 0) {
		dev_err(dev, "Failed to read io-channel-names: %d",
				ret);
		goto out;
	}

	for (i = 0; i < *chan_count; i++) {
		chans[i] = iio_channel_get(dev, temp_string[i]);
		if (IS_ERR(chans[i])) {
			ret = -EPROBE_DEFER;
			dev_err(dev, "channel %s iio_channel_get error: %ld",
				temp_string[i],
				PTR_ERR(chans[i]));
			goto out;
		}
	}

out:
	kfree(temp_string);
	return ret;
}
