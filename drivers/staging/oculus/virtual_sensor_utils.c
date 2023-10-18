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

bool is_charging(struct power_supply *batt_psy)
{
	union power_supply_propval batt_val = {0};
	const enum power_supply_property psy_prop = POWER_SUPPLY_PROP_CURRENT_NOW;

	return batt_psy &&
			!power_supply_get_property(batt_psy, psy_prop, &batt_val) &&
			batt_val.intval > 0;
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
		struct virtual_sensor_common_data *data)
{
	int count, i, ret = 0;
	const char **temp_string = NULL;

	count = of_property_count_strings(dev->of_node, "thermal-zones");
	if (count < 0) {
		dev_err(dev, "Empty or invalid thermal-zones property");
		return -ENODATA;
	}

	data->tz_count = count;

	temp_string = kcalloc(count, sizeof(char *),  GFP_KERNEL);
	if (!temp_string)
		return -ENOMEM;

	ret = of_property_read_string_array(dev->of_node, "thermal-zones",
				temp_string, data->tz_count);
	if (ret < 0) {
		dev_err(dev, "Failed to read thermal-zones: %d", ret);
		goto out;
	}

	for (i = 0; i < data->tz_count; i++) {
		data->tzs[i] = thermal_zone_get_zone_by_name(temp_string[i]);
		if (IS_ERR(data->tzs[i])) {
			ret = -EPROBE_DEFER;
			dev_dbg(dev, "sensor %s get_zone error: %ld",
				temp_string[i],
				PTR_ERR(data->tzs[i]));
			goto out;
		}
	}

	ret = of_property_read_u32_array(dev->of_node,
			"thermal-zone-scaling-factors",
			data->tz_scaling_factors, data->tz_count);
	if (ret < 0) {
		dev_dbg(dev, "No tz-scaling-factors specified, using 1");
		for (i = 0; i < data->tz_count; i++)
			data->tz_scaling_factors[i] = 1;
		ret = 0;
	}

out:
	kfree(temp_string);
	return ret;
}

static int virtual_sensor_parse_iio_channels_dt(struct device *dev,
		struct virtual_sensor_common_data *data)
{
	int count, i, ret = 0;
	const char **temp_string = NULL;

	count = of_property_count_strings(dev->of_node, "io-channel-names");
	if (count == -EINVAL) {
		/* io-channel-names does not exist, which is ok */
		data->iio_count = 0;
		return 0;
	} else if (count < 0) {
		dev_err(dev, "Invalid io-channel-names property");
		return count;
	}

	data->iio_count = count;

	temp_string = kcalloc(data->iio_count, sizeof(char *), GFP_KERNEL);
	if (!temp_string)
		return -ENOMEM;

	ret = of_property_read_string_array(dev->of_node, "io-channel-names",
			temp_string, data->iio_count);
	if (ret < 0) {
		dev_err(dev, "Failed to read io-channel-names: %d", ret);
		goto out;
	}

	for (i = 0; i < data->iio_count; i++) {
		data->iios[i] = iio_channel_get(dev, temp_string[i]);
		if (IS_ERR(data->iios[i])) {
			ret = -EPROBE_DEFER;
			dev_err(dev, "channel %s iio_channel_get error: %ld",
				temp_string[i],
				PTR_ERR(data->iios[i]));
			goto out;
		}
	}

	ret = of_property_read_u32_array(dev->of_node, "io-scaling-factors",
			data->iio_scaling_factors, data->iio_count);
	if (ret < 0) {
		for (i = 0; i < data->iio_count; i++)
			data->iio_scaling_factors[i] = 1;
		ret = 0;
	}

out:
	kfree(temp_string);
	return ret;
}

static int virtual_sensor_parse_tz_coefficients_dt(struct device *dev,
		struct virtual_sensor_common_data *data,
		struct device_node *sensor_node)
{
	int i, ret;

	ret = of_property_read_u32_array(sensor_node, "tz-coefficients",
			data->tz_coefficients, data->tz_count);
	if (ret < 0) {
		dev_err(dev, "Failed parsing tz-coefficients: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(sensor_node, "tz-slope-coefficients",
			data->tz_slope_coefficients, data->tz_count);
	if (ret < 0) {
		dev_dbg(dev, "No tz-slope-coefficients specified, using zeroes");
		for (i = 0; i < data->tz_count; i++)
			data->tz_slope_coefficients[i] = 0;
		ret = 0;
	}

	return 0;
}

static int virtual_sensor_parse_iio_coefficients_dt(struct device *dev,
		struct virtual_sensor_common_data *data,
		struct device_node *sensor_node)
{
	int ret;

	if (!data->iio_count)
		return 0;

	ret = of_property_read_u32_array(sensor_node, "io-coefficients",
			data->iio_coefficients, data->iio_count);
	if (ret < 0) {
		dev_err(dev, "Failed parsing io-coefficients: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(sensor_node, "io-slope-coefficients",
			data->iio_slope_coefficients, data->iio_count);
	if (ret < 0) {
		dev_err(dev, "Failed parsing io-slope-coefficients: %d", ret);
		return ret;
	}

	return 0;
}

static int virtual_sensor_parse_coefficients_dt(struct device *dev,
		struct virtual_sensor_common_data *data)
{
	struct device_node *child, *sensor_node = NULL;
	int ret;

	child = of_get_child_by_name(dev->of_node, "coefficients");
	if (!child) {
		ret = -EINVAL;
		goto error;
	}

	sensor_node = of_get_child_by_name(child, data->name);
	if (!sensor_node) {
		ret = -EINVAL;
		dev_err(dev, "Failed to get sensor node \"%s\": %d", data->name, ret);
		goto error;
	}

	ret = virtual_sensor_parse_tz_coefficients_dt(dev, data, sensor_node);
	if (ret < 0)
		goto error;

	ret = virtual_sensor_parse_iio_coefficients_dt(dev, data, sensor_node);
	if (ret < 0)
		goto error;

	ret = of_property_read_u32(sensor_node, "intercept", &data->intercept);
	if (ret < 0) {
		data->intercept = 0;
		ret = 0;
	}

error:
	of_node_put(child);
	of_node_put(sensor_node);

	return ret;
}

int virtual_sensor_parse_dt(struct device *dev,
		struct virtual_sensor_common_data *data)
{
	int ret;

	ret = virtual_sensor_parse_thermal_zones_dt(dev, data);
	if (ret < 0)
		return ret;

	ret = virtual_sensor_parse_iio_channels_dt(dev, data);
	if (ret < 0 && ret != -EINVAL)
		return ret;

	ret = virtual_sensor_parse_coefficients_dt(dev, data);
	if (ret < 0)
		return ret;

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

ssize_t tz_coefficients_discharging_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_discharging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.tz_coefficients, data.tz_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t tz_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_discharging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->tz_coefficients, data->tz_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t tz_slope_coefficients_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_discharging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.tz_slope_coefficients, data.tz_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t tz_slope_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_discharging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->tz_slope_coefficients, data->tz_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t iio_coefficients_discharging_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_discharging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.iio_coefficients, data.iio_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t iio_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_discharging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->iio_coefficients, data->iio_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t iio_slope_coefficients_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_discharging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.iio_slope_coefficients, data.iio_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t iio_slope_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_discharging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->iio_slope_coefficients, data->iio_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t tz_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_charging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.tz_coefficients, data.tz_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t tz_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_charging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->tz_coefficients, data->tz_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t tz_slope_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_charging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.tz_slope_coefficients,	data.tz_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t tz_slope_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_charging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->tz_slope_coefficients, data->tz_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t iio_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_charging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.iio_coefficients, data.iio_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t iio_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_charging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->iio_coefficients, data->iio_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t iio_slope_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_charging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = show_coefficients(data.iio_slope_coefficients, data.iio_count, buf);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t iio_slope_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_charging;
	int ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	ret = store_coefficients(data->iio_slope_coefficients, data->iio_count, buf);
	mutex_unlock(&drvdata->lock);
	if (ret < 0)
		return ret;

	return count;
}

ssize_t intercept_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_charging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", data.intercept);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t intercept_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_charging;
	int ret, constant;

	ret = kstrtoint(buf, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	data->intercept = constant;
	mutex_unlock(&drvdata->lock);

	return count;
}

ssize_t intercept_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data data = drvdata->data_discharging;
	ssize_t ret;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}
	ret = sprintf(buf, "%d\n", data.intercept);
	mutex_unlock(&drvdata->lock);

	return ret;
}

ssize_t intercept_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *drvdata =
		(struct virtual_sensor_drvdata *) dev_get_drvdata(dev);
	struct virtual_sensor_common_data *data = &drvdata->data_discharging;
	int ret, constant;

	ret = kstrtoint(buf, 10, &constant);
	if (ret < 0)
		return -EINVAL;

	ret = mutex_lock_interruptible(&drvdata->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, ret);
		return ret;
	}
	data->intercept = constant;
	mutex_unlock(&drvdata->lock);

	return count;
}

ssize_t fallback_tolerance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virtual_sensor_drvdata *vs = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", vs->fallback_tolerance);
	mutex_unlock(&vs->lock);

	return ret;
}

ssize_t fallback_tolerance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct virtual_sensor_drvdata *vs = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&vs->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, (int)ret);
		return ret;
	}

	if (kstrtoint(buf, 10, &vs->fallback_tolerance))
		return -EINVAL;
	mutex_unlock(&vs->lock);

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

#ifdef CONFIG_ARCH_KONA
extern void of_thermal_handle_trip_temp(struct thermal_zone_device *tz, int trip_temp);
#endif

static void virtual_sensor_check(struct work_struct *work)
{
	struct virtual_sensor_common_data *data = container_of(work, struct virtual_sensor_common_data, poll_queue.work);
	struct thermal_zone_device *tzd = data->tzd;

#ifdef CONFIG_ARCH_KONA
	int ret, temp;

	ret = thermal_zone_get_temp(tzd, &temp);
	if (ret) {
		if (ret != -EAGAIN)
			dev_warn(&tzd->device,
				 "failed to read out thermal zone (%d)\n",
				 ret);
		return;
	}
	of_thermal_handle_trip_temp(tzd, temp);
#else
	thermal_zone_device_update(tzd, THERMAL_EVENT_UNSPECIFIED);
#endif

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
