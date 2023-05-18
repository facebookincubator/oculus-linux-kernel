// SPDX-License-Identifier: GPL-2.0-only

#include "virtual_sensor_utils.h"

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>

/*
 * drivers/thermal/of-thermal.c has actual of_thermal_handle_trip_temp.
 * This is to tell the compiler that linkage is extern.
 */
extern void of_thermal_handle_trip_temp(struct thermal_zone_device *tz,
					int trip_temp);

int is_charging(struct power_supply *batt_psy)
{
	union power_supply_propval val = {0};

	return (batt_psy && !power_supply_get_property(batt_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val) &&
			val.intval > 0);
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
		struct virtual_sensor_common_data *data, s64 *temperature)
{
	const unsigned long curr_jiffies = jiffies;
	const s64 delta_jiffies = (s64)(curr_jiffies - data->tz_last_jiffies);
	int i, ret, temp;
	const bool restart_sampling = data->tz_samples < 0 ||
			delta_jiffies > (2 * HZ);

	/*
	 * If 'tz_samples' is negative or it's been a long time since we last
	 * calculated the TZ temps then treat this as a special case and reset
	 * everything. Ignore the slope coefficients in calculating the temp.
	 */
	if (restart_sampling) {
		data->tz_samples = 0;
		memset(data->tz_accum_temperatures, 0, data->tz_count *
				sizeof(s64));
	}

	data->tz_samples++;
	for (i = 0; i < data->tz_count; i++) {
		ret = thermal_zone_get_temp(data->tzs[i], &temp);
		if (ret) {
			dev_warn(dev, "%s: error getting temp: %d",
					data->tzs[i]->type, ret);
			return ret;
		}

		temp *= data->tz_scaling_factors[i];

		data->tz_accum_temperatures[i] += (s64)temp;
	}

	/*
	 * Wait at least one second between calculations unless we need to
	 * restart sampling.
	 */
	if (delta_jiffies < HZ && !restart_sampling)
		goto not_ready;

	data->tz_last_jiffies = curr_jiffies;
	data->tz_temperature = 0;
	for (i = 0; i < data->tz_count; i++) {
		const s64 avg_temp = div64_s64(data->tz_accum_temperatures[i],
				data->tz_samples);

		data->tz_temperature += (s64)data->tz_coefficients[i] * avg_temp;

		if (!restart_sampling) {
			const s64 delta_temp = div64_s64((avg_temp -
					data->tz_last_temperatures[i]) *
					HZ, delta_jiffies);

			data->tz_temperature +=
					(s64)data->tz_slope_coefficients[i] *
					delta_temp;
		}

		data->tz_accum_temperatures[i] = 0;
		data->tz_last_temperatures[i] = avg_temp;
	}

	/* Reset our sample count. */
	data->tz_samples = 0;

not_ready:
	*temperature += data->tz_temperature;

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
		struct virtual_sensor_common_data *data, s64 *temperature)
{
	const unsigned long curr_jiffies = jiffies;
	const s64 delta_jiffies = (s64)(curr_jiffies - data->iio_last_jiffies);
	int i, ret, temp;
	const bool restart_sampling = data->iio_samples < 0 ||
			delta_jiffies > (2 * HZ);

	/*
	 * If 'iio_samples' is negative or it's been a long time since we last
	 * calculated the IIO temps then treat this as a special case and reset
	 * everything. Ignore the slope coefficients in calculating the temp.
	 */
	if (restart_sampling) {
		data->iio_samples = 0;
		memset(data->iio_accum_temperatures, 0, data->iio_count *
				sizeof(s64));
	}

	data->iio_samples++;
	for (i = 0; i < data->iio_count; i++) {
		ret = iio_read_channel_processed(data->iios[i], &temp);
		if (ret < 0) {
			dev_warn(dev, "%s: error getting temp: %d",
					data->iios[i]->indio_dev->name, ret);
			return ret;
		}

		temp *= data->iio_scaling_factors[i];

		data->iio_accum_temperatures[i] += (s64)temp;
	}

	/*
	 * Wait at least one second between calculations unless we need to
	 * restart sampling.
	 */
	if (delta_jiffies < HZ && !restart_sampling)
		goto not_ready;

	data->iio_last_jiffies = curr_jiffies;
	data->iio_temperature = 0;
	for (i = 0; i < data->iio_count; i++) {
		const s64 avg_temp = div64_s64(data->iio_accum_temperatures[i],
				data->iio_samples);

		data->iio_temperature += (s64)data->iio_coefficients[i] *
				avg_temp;

		if (!restart_sampling) {
			const s64 delta_temp = div64_s64((avg_temp -
					data->iio_last_temperatures[i]) *
					delta_jiffies, HZ);

			data->iio_temperature +=
					(s64)data->iio_slope_coefficients[i] *
					delta_temp;
		}

		data->iio_accum_temperatures[i] = 0;
		data->iio_last_temperatures[i] = avg_temp;
	}

	/* Reset our sample count. */
	data->iio_samples = 0;

not_ready:
	*temperature += data->iio_temperature;

	return 0;
}

void virtual_sensor_reset_history(struct virtual_sensor_common_data *data)
{
	memset(data->tz_accum_temperatures, 0,
			sizeof(data->tz_accum_temperatures));
	memset(data->tz_last_temperatures, 0,
			sizeof(data->tz_last_temperatures));
	data->tz_temperature = 0;
	data->tz_last_jiffies = 0;

	memset(data->iio_accum_temperatures, 0,
			sizeof(data->iio_accum_temperatures));
	memset(data->iio_last_temperatures, 0,
			sizeof(data->iio_last_temperatures));
	data->iio_temperature = 0;
	data->iio_last_jiffies = 0;

	/*
	 * Set the sample counts to -1 to signal that we should clear
	 * things out and restart the sampling.
	 */
	data->tz_samples = -1;
	data->iio_samples = -1;
}

static int virtual_sensor_parse_thermal_zones_dt(struct device *dev,
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

static int virtual_sensor_parse_iio_channels_dt(struct device *dev,
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

int virtual_sensor_parse_common_dt(struct device *dev,
		struct virtual_sensor_common_data *data)
{
	int i, ret;

	ret = virtual_sensor_parse_thermal_zones_dt(dev, data->tzs,
			&data->tz_count);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(dev->of_node,
			"thermal-zone-scaling-factors",
			data->tz_scaling_factors, data->tz_count);
	if (ret < 0) {
		for (i = 0; i < data->tz_count; i++)
			data->tz_scaling_factors[i] = 1;
	}

	ret = of_property_read_u32_array(dev->of_node, "thermal-zone-coefficients",
			data->tz_coefficients, data->tz_count);
	if (ret < 0) {
		dev_err(dev, "Failed to parse thermal-zone-coefficients: %d",
				ret);
		return ret;
	}

	ret = of_property_read_u32_array(dev->of_node,
			"thermal-zone-slope-coefficients",
			data->tz_slope_coefficients, data->tz_count);
	if (ret < 0) {
		dev_err(dev, "Failed to parse thermal-zone-slope-coefficients: %d",
				ret);
		return ret;
	}

	ret = virtual_sensor_parse_iio_channels_dt(dev, data->iios,
			&data->iio_count);
	if (ret < 0 && ret != -EINVAL)
		return ret;

	ret = of_property_read_u32_array(dev->of_node, "io-scaling-factors",
			data->iio_scaling_factors, data->iio_count);
	if (ret < 0) {
		for (i = 0; i < data->iio_count; i++)
			data->iio_scaling_factors[i] = 1;
	}

	ret = of_property_read_u32_array(dev->of_node, "io-coefficients",
			data->iio_coefficients, data->iio_count);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(dev, "Failed to parse io-coefficients: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(
			dev->of_node, "io-slope-coefficients",
			data->iio_slope_coefficients, data->iio_count);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(dev, "Failed to parse io-slope-coefficients: %d", ret);
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "intercept-charging",
			&data->intercept_constant_charging);
	if (ret < 0)
		data->intercept_constant_charging = 0;

	ret = of_property_read_u32(dev->of_node, "intercept-discharging",
			&data->intercept_constant_discharging);
	if (ret < 0)
		data->intercept_constant_discharging = 0;

	ret = of_property_read_u32(dev->of_node, "internal-polling-delay",
			&data->internal_polling_delay);
	if (ret < 0)
		data->internal_polling_delay = 0;

	return 0;
}

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

ssize_t tz_coefficients_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(data->tz_coefficients, data->tz_count, buf);
	mutex_unlock(&data->lock);

	return ret;
}

ssize_t tz_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(data->tz_coefficients, data->tz_count, buf);
	mutex_unlock(&data->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t tz_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(data->tz_slope_coefficients, data->tz_count, buf);
	mutex_unlock(&data->lock);

	return ret;
}

ssize_t tz_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(data->tz_slope_coefficients, data->tz_count, buf);
	mutex_unlock(&data->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t iio_coefficients_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(data->iio_coefficients, data->iio_count, buf);
	mutex_unlock(&data->lock);

	return ret;
}

ssize_t iio_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(data->iio_coefficients, data->iio_count, buf);
	mutex_unlock(&data->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t iio_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = show_coefficients(data->iio_slope_coefficients, data->iio_count, buf);
	mutex_unlock(&data->lock);

	return ret;
}

ssize_t iio_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	int ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	ret = store_coefficients(data->iio_slope_coefficients, data->iio_count, buf);
	mutex_unlock(&data->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t charging_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", data->intercept_constant_charging);
	mutex_unlock(&data->lock);

	return ret;
}

ssize_t charging_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	int ret, constant;

	ret = kstrtoint(buf, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	data->intercept_constant_charging = constant;
	mutex_unlock(&data->lock);

	return count;
}

ssize_t discharging_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %zd\n", ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", data->intercept_constant_discharging);
	mutex_unlock(&data->lock);

	return ret;
}

ssize_t discharging_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_common_data *data =
		(struct virtual_sensor_common_data *) dev_get_drvdata(dev);
	int ret, constant;

	ret = kstrtoint(buf, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret < 0) {
		dev_err(dev, "Failed to obtain mutex: %d\n", ret);
		return ret;
	}
	data->intercept_constant_discharging = constant;
	mutex_unlock(&data->lock);

	return count;
}

static void virtual_sensor_workqueue_set_polling(struct workqueue_struct *queue, struct virtual_sensor_common_data *data)
{
	if (data->internal_polling_delay > 1000)
		mod_delayed_work(queue, &data->poll_queue,
				 round_jiffies(msecs_to_jiffies(data->internal_polling_delay)));
	else if (data->internal_polling_delay)
		mod_delayed_work(queue, &data->poll_queue,
				 msecs_to_jiffies(data->internal_polling_delay));
	else
		cancel_delayed_work(&data->poll_queue);
}

static void virtual_sensor_check(struct work_struct *work)
{
	int ret = 0;
	int temp;
	struct virtual_sensor_common_data *data = container_of(work, struct virtual_sensor_common_data, poll_queue.work);
	struct thermal_zone_device *tzd = data->tzd;

	ret = thermal_zone_get_temp(tzd, &temp);
	if (ret) {
		if (ret != -EAGAIN)
			dev_warn(&tzd->device,
				 "failed to read out thermal zone (%d)\n",
				 ret);
		return;
	}
	of_thermal_handle_trip_temp(tzd, temp);

	virtual_sensor_workqueue_set_polling(system_freezable_power_efficient_wq, data);
}

void virtual_sensor_workqueue_register(struct virtual_sensor_common_data *data)
{
	INIT_DEFERRABLE_WORK(&data->poll_queue, virtual_sensor_check);
	virtual_sensor_workqueue_set_polling(system_freezable_power_efficient_wq, data);
}

void virtual_sensor_workqueue_unregister(struct virtual_sensor_common_data *data)
{
	cancel_delayed_work_sync(&data->poll_queue);
}
