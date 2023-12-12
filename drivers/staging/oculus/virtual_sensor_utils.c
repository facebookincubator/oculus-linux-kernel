// SPDX-License-Identifier: GPL-2.0-only

#include "virtual_sensor_utils.h"

#include <linux/delay.h>
#include <linux/err.h>
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

static int virtual_sensor_thermal_zone_get_temp_scaled(
		struct thermal_zone_device *tzd, int scaling_factor, int *temp)
{
	int ret = thermal_zone_get_temp(tzd, temp);
	if (ret)
		return ret;

	*temp *= scaling_factor;

	return 0;
}

static int virtual_sensor_estimate_tz_if_faulty(
		const struct thermal_zone_info *tz_info,
		int *temp)
{
	int ret = 0;
	int tz_temp;

	if (!tz_info->tz_estimator)
		return -EINVAL;

	if (tz_info->fault_lb <= *temp && *temp <= tz_info->fault_ub)
		return 0;

	ret = virtual_sensor_thermal_zone_get_temp_scaled(tz_info->tz_estimator,
			tz_info->tz_estimator_scaling_factor, &tz_temp);
	if (ret)
		return ret;

	*temp = (int)div64_s64((s64)tz_temp * (s64)tz_info->tz_estimator_coeff,
			COEFFICIENT_SCALAR);
	*temp += tz_info->tz_estimator_int;

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
		ret = virtual_sensor_thermal_zone_get_temp_scaled(data->tzs[i].tz,
				data->tz_scaling_factors[i], &temp);
		if (ret) {
			dev_err(dev, "%s: error getting temp: %d", data->tzs[i].tz->type, ret);
			return ret;
		}

		if (data->tzs[i].fault_handling) {
			ret = virtual_sensor_estimate_tz_if_faulty(&data->tzs[i], &temp);
			if (ret) {
				dev_err(dev, "%s: couldn't estimate faulty tz %s: %d", __func__,
						data->tzs[i].tz->type, ret);
				return ret;
			}
		}


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

void virtual_sensor_reset_history(struct virtual_sensor_common_data *data)
{
	memset(data->tz_accum_temperatures, 0,
			sizeof(data->tz_accum_temperatures));
	memset(data->tz_last_temperatures, 0,
			sizeof(data->tz_last_temperatures));
	data->tz_temperature = 0;
	data->tz_last_jiffies = 0;

	/*
	 * Set the sample counts to -1 to signal that we should clear
	 * things out and restart the sampling.
	 */
	data->tz_samples = -1;
}

static int thermal_zone_parse_thermal_zone_fault_dt(struct device *dev,
		struct virtual_sensor_common_data *vs_data)
{
	int i, ret;
	u32 fault_bounds[2];
	u32 fault_scaling_factors[THERMAL_MAX_VIRT_SENSORS];
	u32 fault_vals[THERMAL_MAX_VIRT_SENSORS * 2];
	const char *fault_zones[THERMAL_MAX_VIRT_SENSORS];
	struct thermal_zone_info *tz_info;

	ret = of_property_read_u32_array(dev->of_node,
			"thermal-zone-fault-estimator-bounds",
			fault_bounds, 2);
	if (ret == -EINVAL)
		return 0;
	else if (ret < 0)
		return ret;

	ret = of_property_read_string_array(dev->of_node,
			"thermal-zone-fault-estimator-zones",
			fault_zones, vs_data->tz_count);
	if (ret < 0) {
		dev_err(dev, "Failed to read tz estimator zones: %d", ret);
		return ret;
	}

	ret = of_property_read_u32_array(dev->of_node,
			"thermal-zone-fault-estimator-scaling-factors",
			fault_scaling_factors, vs_data->tz_count);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(dev, "Failed to read tz estimator scaling factors: %d", ret);
		return ret;
	}

	for (i = 0; i < vs_data->tz_count; i++) {
		if (ret == -EINVAL)
			vs_data->tzs[i].tz_estimator_scaling_factor = 1;
		else
			vs_data->tzs[i].tz_estimator_scaling_factor = fault_scaling_factors[i];
	}

	ret = of_property_read_u32_array(dev->of_node,
			"thermal-zone-fault-estimator-values",
			fault_vals, vs_data->tz_count * 2);
	if (ret) {
		dev_err(dev, "Failed to read tz estimator vals: %d", ret);
		return ret;
	}

	for (i = 0; i < vs_data->tz_count; i++) {
		if (!strcmp(fault_zones[i], "NA"))
			continue;

		tz_info = &vs_data->tzs[i];

		tz_info->fault_handling = true;
		tz_info->fault_lb = fault_bounds[0];
		tz_info->fault_ub = fault_bounds[1];

		tz_info->tz_estimator = thermal_zone_get_zone_by_name(fault_zones[i]);
		if (IS_ERR(tz_info->tz_estimator)) {
			dev_dbg_ratelimited(dev, "sensor %s get_zone error: %ld",
					fault_zones[i],
					PTR_ERR(tz_info->tz));
			return -EPROBE_DEFER;
		}

		tz_info->tz_estimator_coeff = fault_vals[i * 2];
		tz_info->tz_estimator_int = fault_vals[(i * 2) + 1];
	}

	return 0;
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
		data->tzs[i].tz = thermal_zone_get_zone_by_name(temp_string[i]);
		if (IS_ERR(data->tzs[i].tz)) {
			ret = -EPROBE_DEFER;
			dev_dbg(dev, "sensor %s get_zone error: %ld",
				temp_string[i],
				PTR_ERR(data->tzs[i].tz));
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

	ret = thermal_zone_parse_thermal_zone_fault_dt(dev, data);
	if (ret < 0) {
		dev_dbg(dev, "Failed parsing tz fault configuration: %d", ret);
		goto out;
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

	ret = virtual_sensor_parse_coefficients_dt(dev, data);
	if (ret < 0)
		return ret;

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
