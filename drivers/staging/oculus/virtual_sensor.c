// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <linux/version.h>

/*
 * How long it takes to fully transition between charging/discharging
 * calculations, in seconds
*/
#define CHARGING_SMOOTHING_PERIOD_SEC 10
#define CHARGING_SMOOTHING_FACTOR_MAX 100

#define COEFFICIENT_SCALAR 10000
#define THERMAL_MAX_VIRT_SENSORS 10

/**
 * Stores a component thermal zone as well as an optional estimator for the
 * the case where the primary thermistor reading is faulty.
 *
 * Example:
 *	Given: temp(tz) is outside the range of (fault_lb, fault_ub)
 *	==> Replace temp(tz) by temp(tz_estimator) * coefficient + intercept
 */
struct thermal_zone_info {
	struct thermal_zone_device *tz;

	bool fault_handling;
	int fault_lb;
	int fault_ub;

	struct thermal_zone_device *tz_estimator;
	int tz_estimator_scaling_factor;
	int tz_estimator_coeff;
	int tz_estimator_int;
};

/**
 * Data for an individual logical sensor, e.g. charging or discharging.
 */
struct virtual_sensor_common_data {
	void *parent;

	/* Matched in device tree */
	const char *name;

	struct thermal_zone_info tzs[THERMAL_MAX_VIRT_SENSORS];

	/*
	 * Thermal zone names from DT. When hotpluggable, these are resolved on
	 * every get_temp() call instead of cached at probe — the underlying
	 * thermal_zone_device may come and go (e.g. tether/HMD pluggable
	 * sensors) and a cached pointer would dangle after unregister.
	 */
	const char *tz_names[THERMAL_MAX_VIRT_SENSORS];
	bool hotpluggable;

	/* Accumulate temperature samples as part of the formula */
	s64 tz_samples;
	s64 tz_accum_temperatures[THERMAL_MAX_VIRT_SENSORS];

	/* Store last temperatures as part of the formula */
	s64 tz_last_temperatures[THERMAL_MAX_VIRT_SENSORS];

	/* Rate limit temperature calculations */
	unsigned long tz_last_jiffies;
	s64 tz_temperature;

	/* Scaling factor scales to millidegrees */
	int tz_scaling_factors[THERMAL_MAX_VIRT_SENSORS];

	/* scaled by COEFFICIENT_SCALAR */
	int tz_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int tz_slope_coefficients[THERMAL_MAX_VIRT_SENSORS];

	int tz_count;

	int intercept;

	struct thermal_zone_device *tzd;
};

/**
 * Data for the virtual sensor driver, accessed with dev_[get/set]_drvdata
 *
 * Contains two logical sensors, one for charging and one for discharging.
 */
struct virtual_sensor_drvdata {
	struct mutex lock;

	struct device *dev;
	struct thermal_zone_device *tzd;
	struct virtual_sensor_common_data data_charging;
	struct virtual_sensor_common_data data_discharging;

	/* A fallback thermal zone can be specified in the device tree. If the
	 * difference between the virtual sensor's calculated temp and the temp of
	 * the fallback thermal zone exceeds the specified tolerance, the virtual
	 * sensor will report the temperature of the fallback thermal zone as its
	 * own thermal zone's temperature.
	 */
	struct thermal_zone_device *fallback_tzd;
	int fallback_tz_scaling_factor;
	int fallback_tolerance;

	int charging_smoothing_factor;
	ktime_t last_inc_smoothing_factor_time;
	s64 last_temperature;
	bool was_charging;
	struct power_supply *batt_psy;

	/* time averaging logic */
	/* delay between averaged samples, in milliseconds */
	s64 averaging_delay;
	/* over how many polls to average */
	int averaging_count;
	/* place to save the history, as a circular buffer */
	int *historic_temps;
	/* the size of collected history, up to averaging_count */
	int history_size;
	/* index of last collected value in the circular buffer, unless history_size==0 */
	int history_idx;
	/* sum of all values in history, a precursor for averaging */
	int sum_temp;
	/* time of the last historic value, in milliseconds */
	s64 hist_ms;
};

static bool is_charging(struct power_supply *batt_psy)
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

static bool tz_is_faulty(
		const struct thermal_zone_info *tz_info,
		int temp)
{
	return (temp < tz_info->fault_lb) || (temp > tz_info->fault_ub);
}

static int virtual_sensor_estimate_faulty_tz(
		const struct thermal_zone_info *tz_info,
		int *temp)
{
	int ret = 0;
	int tz_temp;

	if (!tz_info->tz_estimator)
		return -EINVAL;

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
 *
 * For hotpluggable sensors, the zone pointer is resolved freshly on each
 * call. Returns -ENODATA if any zone cannot be read (unresolved or
 * thermal_zone_get_temp failure) — every zone must contribute for the
 * linear combination to be meaningful.
 */
static int virtual_sensor_calculate_tz_temp(struct device *dev,
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
		struct thermal_zone_device *tz;

		/*
		 * Resolve zone pointer freshly each call for hotpluggable
		 * sensors so an unregister doesn't leave a dangling pointer.
		 */
		if (data->hotpluggable) {
			tz = thermal_zone_get_zone_by_name(data->tz_names[i]);
			if (IS_ERR(tz))
				tz = NULL;
		} else {
			tz = data->tzs[i].tz;
		}

		if (!tz) {
			dev_dbg_ratelimited(dev,
				"%s: zone %s unavailable",
				__func__,
				data->tz_names[i] ? data->tz_names[i] : "?");
			return -ENODATA;
		}

		ret = virtual_sensor_thermal_zone_get_temp_scaled(tz,
				data->tz_scaling_factors[i], &temp);
		if (ret)
			dev_err_ratelimited(dev, "%s: error getting temp: %d",
					tz->type, ret);

		if (data->tzs[i].fault_handling &&
				(ret < 0 || tz_is_faulty(&data->tzs[i], temp))) {
			ret = virtual_sensor_estimate_faulty_tz(&data->tzs[i], &temp);
			if (ret)
				dev_err(dev, "%s: couldn't estimate faulty tz %s: %d",
						__func__, tz->type, ret);
		}

		if (ret)
			return ret;

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

static void virtual_sensor_reset_history(struct virtual_sensor_common_data *data)
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

static int virtual_sensor_calculate_temp_for_coeffs(
	struct virtual_sensor_drvdata *vs,
	struct virtual_sensor_common_data *coeff_data, s64 *tz_temp)
{
	int ret = 0;

	if (!vs || !vs->dev || !coeff_data || !tz_temp)
		return -EINVAL;

	ret = virtual_sensor_calculate_tz_temp(vs->dev, coeff_data, tz_temp);
	if (ret) {
		if (coeff_data->hotpluggable) {
			dev_dbg_ratelimited(vs->dev,
				"%s: hotpluggable zone unavailable, reporting 0",
				__func__);
			*tz_temp = 0;
			return 0;
		}
		return ret;
	}

	*tz_temp = div64_s64(*tz_temp, COEFFICIENT_SCALAR);
	*tz_temp += coeff_data->intercept;

	return 0;
}

static void virtual_sensor_averaging(
		struct virtual_sensor_drvdata *vs, int *temperature)
{
	/*
	 * There is no guarantee that temperatures get read exactly at
	 * averaging intervals, the reading might be slightly delayed or
	 * there might be additional reads in between. So be fuzzy and
	 * accept whatever was the first sample within each period.
	 */
	const s64 cur_ms = ktime_to_ms(ktime_get_boottime());

	if (cur_ms - vs->hist_ms >= vs->averaging_delay * vs->averaging_count) {
		/* uh-oh, polling broke, maybe the system was sleeping, reset the history */
		vs->history_size = 0;
	}
	if (vs->history_size == 0) {
		/*
		 * Initialize the history by pushing in one value.
		 */
		vs->history_size = 1;
		vs->history_idx = 0;
		vs->historic_temps[0] = *temperature;
		vs->sum_temp = *temperature;
		vs->hist_ms = cur_ms;
		return;
	}

	/*
	 * Advance time in whole averaging delay slices, so if less than
	 * one whole slice had passed yet, no advancing would be done,
	 * and the last value would be returned.
	 */
	while (cur_ms - vs->hist_ms >= vs->averaging_delay) {
		vs->hist_ms += vs->averaging_delay;

		/* insert a new data point */
		if (++vs->history_idx >= vs->averaging_count)
			vs->history_idx = 0; /* wrap on the circular buffer */
		if (vs->history_size < vs->averaging_count) {
			++vs->history_size;
		} else {
			/* drop the oldest value */
			vs->sum_temp -=
				vs->historic_temps[vs->history_idx];
		}
		vs->historic_temps[vs->history_idx] = *temperature;
		vs->sum_temp += *temperature;
	}

	/* replace the temperature with the average */
	*temperature = vs->sum_temp / vs->history_size;
}

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
static int virtual_sensor_get_temp(struct thermal_zone_device *tz, int *temperature)
#else
static int virtual_sensor_get_temp(void *data, int *temperature)
#endif
{
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct virtual_sensor_drvdata *vs = tz->devdata;
#else
	struct virtual_sensor_drvdata *vs = data;
#endif
	const ktime_t curr_ktime = ktime_get_boottime();
	const bool should_inc_smoothing_factor =
		ktime_ms_delta(curr_ktime, vs->last_inc_smoothing_factor_time) > (1 * MSEC_PER_SEC);
	s64 temp = 0, temp_charging = 0, temp_discharging = 0, fallback_temp = 0;
	int ret = 0;
	int calc_ret = 0;
	bool charging;

	/* Lazily acquire battery power supply if not available at probe */
	if (!vs->batt_psy)
		vs->batt_psy = power_supply_get_by_name("battery");

	charging = is_charging(vs->batt_psy);

	if (!temperature)
		return -EINVAL;

	*temperature = 0;

	mutex_lock(&vs->lock);

	if (charging != vs->was_charging) {
		/* Zero out history upon disconnection to avoid sudden jumps */
		virtual_sensor_reset_history(&vs->data_charging);
		virtual_sensor_reset_history(&vs->data_discharging);
	}

	if (should_inc_smoothing_factor) {
		if (charging) {
			vs->charging_smoothing_factor += (CHARGING_SMOOTHING_FACTOR_MAX / CHARGING_SMOOTHING_PERIOD_SEC);
			vs->charging_smoothing_factor = min(vs->charging_smoothing_factor, CHARGING_SMOOTHING_FACTOR_MAX);
		} else {
			vs->charging_smoothing_factor -= (CHARGING_SMOOTHING_FACTOR_MAX / CHARGING_SMOOTHING_PERIOD_SEC);
			vs->charging_smoothing_factor = max(vs->charging_smoothing_factor, 0);
		}

		vs->last_inc_smoothing_factor_time = curr_ktime;
	}
	vs->was_charging = charging;

	calc_ret = virtual_sensor_calculate_temp_for_coeffs(vs, &vs->data_charging, &temp_charging);
	calc_ret |= virtual_sensor_calculate_temp_for_coeffs(vs, &vs->data_discharging, &temp_discharging);
	if (calc_ret) {
		/*
		 * Hotpluggable sensors are expected to disappear (e.g. HMD
		 * unplugged). Surface the error to the thermal framework rather
		 * than reporting a stale "last temp" that would mask the loss
		 * of the underlying zones.
		 */
		if (vs->data_charging.hotpluggable) {
			dev_dbg_ratelimited(vs->dev,
				"%s: hotpluggable TZ unavailable: %d\n",
				__func__, calc_ret);
			mutex_unlock(&vs->lock);
			return -ENODATA;
		}

		/*
		 * Unable to calculate new temp, use the last one so the function doesn't
		 * cause the thermal subsystem to error out.
		 */
		dev_warn(vs->dev,
			"%s: Unable to calcualte TZ temp, re-using last temp: %llu\n",
			__func__, vs->last_temperature);

		*temperature = vs->last_temperature;
		goto get_temp_virtual;
	}

	temp = (temp_charging * vs->charging_smoothing_factor) +
	       (temp_discharging * (CHARGING_SMOOTHING_FACTOR_MAX - vs->charging_smoothing_factor));
	temp = div64_s64(temp, CHARGING_SMOOTHING_FACTOR_MAX);
	*temperature = (int)temp;

	dev_dbg(vs->dev,
		"[%s]: temp[%lld] = (temp_charging[%lld] * mult[%d/100]) + (temp_discharging[%lld] * (1-mult)[%d/100])",
		vs->tzd->type, temp, temp_charging, vs->charging_smoothing_factor,
		temp_discharging, (CHARGING_SMOOTHING_FACTOR_MAX - vs->charging_smoothing_factor));

	if (!vs->fallback_tzd)
		goto get_temp_virtual;

	ret = get_fallback_temp(vs, &fallback_temp);
	if (ret != 0) {
		/*
		 * Ignore fallback temp if it can't be read
		 */
		fallback_temp = temp;
	}

	/* If virtual sensor temp greatly differs from fallback, use fallback */
	if (abs(temp - fallback_temp) > vs->fallback_tolerance)
		*temperature = (int)fallback_temp;

get_temp_virtual:
	if (vs->averaging_count > 1) {
		if (calc_ret != 0) {
			/*
			 * The new point is invalid, can only return the last value if any,
			 * but dont pollute the averaging logic with the invalid guesses.
			 */
			if (vs->history_size > 0)
				*temperature = vs->sum_temp / vs->history_size;
		} else {
			virtual_sensor_averaging(vs, temperature);
		}
	}

	vs->last_temperature = *temperature;
	mutex_unlock(&vs->lock);
	return 0;
}


#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
static const struct thermal_zone_device_ops virtual_sensor_thermal_ops = {
#else
static const struct thermal_zone_of_device_ops virtual_sensor_thermal_ops = {
#endif
	.get_temp = virtual_sensor_get_temp,
};

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

static ssize_t tz_coefficients_discharging_show(struct device *dev, struct device_attribute *attr,
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

static ssize_t tz_coefficients_discharging_store(struct device *dev,
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

static ssize_t tz_slope_coefficients_discharging_show(struct device *dev,
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

static ssize_t tz_slope_coefficients_discharging_store(struct device *dev,
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

static ssize_t tz_coefficients_charging_show(struct device *dev,
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

static ssize_t tz_coefficients_charging_store(struct device *dev,
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

static ssize_t tz_slope_coefficients_charging_show(struct device *dev,
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

static ssize_t tz_slope_coefficients_charging_store(struct device *dev,
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

static ssize_t intercept_charging_show(struct device *dev,
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
	ret = sysfs_emit(buf, "%d\n", data.intercept);
	mutex_unlock(&drvdata->lock);

	return ret;
}

static ssize_t intercept_charging_store(struct device *dev,
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

static ssize_t intercept_discharging_show(struct device *dev,
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
	ret = sysfs_emit(buf, "%d\n", data.intercept);
	mutex_unlock(&drvdata->lock);

	return ret;
}

static ssize_t intercept_discharging_store(struct device *dev,
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

static ssize_t fallback_tolerance_show(struct device *dev,
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

static ssize_t fallback_tolerance_store(struct device *dev,
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

static DEVICE_ATTR_RW(tz_coefficients_discharging);
static DEVICE_ATTR_RW(tz_slope_coefficients_discharging);
static DEVICE_ATTR_RW(tz_coefficients_charging);
static DEVICE_ATTR_RW(tz_slope_coefficients_charging);
static DEVICE_ATTR_RW(intercept_charging);
static DEVICE_ATTR_RW(intercept_discharging);
static DEVICE_ATTR_RW(fallback_tolerance);

static struct attribute *virtual_sensor_attrs[] = {
	&dev_attr_tz_coefficients_discharging.attr,
	&dev_attr_tz_slope_coefficients_discharging.attr,
	&dev_attr_tz_coefficients_charging.attr,
	&dev_attr_tz_slope_coefficients_charging.attr,
	&dev_attr_intercept_discharging.attr,
	&dev_attr_intercept_charging.attr,
	&dev_attr_fallback_tolerance.attr,
	NULL,
};
ATTRIBUTE_GROUPS(virtual_sensor);


static int virtual_sensor_parse_thermal_zone_fault_dt(struct device *dev,
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

	if (count > THERMAL_MAX_VIRT_SENSORS) {
		dev_err(dev, "thermal-zones count %d exceeds max %d",
			count, THERMAL_MAX_VIRT_SENSORS);
		return -EINVAL;
	}

	data->tz_count = count;
	data->hotpluggable = of_property_read_bool(dev->of_node,
			"meta,hotpluggable");

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
		/*
		 * Persist the zone name. For hotpluggable sensors we re-lookup
		 * by name on every get_temp() call; for non-hotpluggable ones
		 * the name is just kept for diagnostics.
		 */
		data->tz_names[i] = devm_kstrdup(dev, temp_string[i],
				GFP_KERNEL);
		if (!data->tz_names[i]) {
			ret = -ENOMEM;
			goto out;
		}

		data->tzs[i].tz = thermal_zone_get_zone_by_name(temp_string[i]);
		if (IS_ERR(data->tzs[i].tz)) {
			if (data->hotpluggable) {
				/*
				 * Zone may not be registered yet — that's fine
				 * for hotpluggable sensors; we'll resolve at
				 * read time.
				 */
				dev_dbg(dev,
					"hotpluggable zone %s not present at probe",
					temp_string[i]);
				data->tzs[i].tz = NULL;
			} else {
				ret = -EPROBE_DEFER;
				dev_dbg(dev, "sensor %s get_zone error: %ld",
					temp_string[i],
					PTR_ERR(data->tzs[i].tz));
				goto out;
			}
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

	ret = virtual_sensor_parse_thermal_zone_fault_dt(dev, data);
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

static int virtual_sensor_parse_dt(struct device *dev,
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
		dev_dbg(&pdev->dev, "battery power supply not available yet, charging state will default to discharging\n");

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
	ret = of_property_read_u32(pdev->dev.of_node,
			"averaging-count", &vs->averaging_count);
	if (ret < 0 || vs->averaging_count <= 1)
		vs->averaging_count = 0;

	if (vs->averaging_count > 1) {
		u32 avg_delay;

		ret = of_property_read_u32(pdev->dev.of_node,
				"averaging-delay", &avg_delay);
		vs->averaging_delay = avg_delay;
		if (ret < 0 || vs->averaging_delay < 1) {
			dev_err(&pdev->dev,
				"averaging-count requires a positive averaging-delay");
			return -EINVAL;
		}

		vs->historic_temps = devm_kcalloc(&pdev->dev,
				vs->averaging_count,
				sizeof(*vs->historic_temps), GFP_KERNEL);
		if (!vs->historic_temps)
			return -ENOMEM;
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

	virtual_sensor_reset_history(&vs->data_charging);
	virtual_sensor_reset_history(&vs->data_discharging);

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	tzd = devm_thermal_of_zone_register
			(&pdev->dev, 0, vs,
			&virtual_sensor_thermal_ops);
#else
	tzd = thermal_zone_of_sensor_register
			(&pdev->dev, 0, vs,
			&virtual_sensor_thermal_ops);
#endif
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

	ret = sysfs_create_groups(&pdev->dev.kobj, virtual_sensor_groups);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create sysfs files");

	return ret;
}

static int virtual_sensor_remove(struct platform_device *pdev)
{
	struct virtual_sensor_drvdata *vs =
			(struct virtual_sensor_drvdata *) platform_get_drvdata(pdev);

#if (KERNEL_VERSION(6, 1, 0) >= LINUX_VERSION_CODE)
	thermal_zone_of_sensor_unregister(&pdev->dev, vs->tzd);
#endif
	if (vs->batt_psy)
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

module_platform_driver(virtual_sensor_driver);

MODULE_ALIAS("virtual_sensor");
MODULE_LICENSE("GPL v2");
