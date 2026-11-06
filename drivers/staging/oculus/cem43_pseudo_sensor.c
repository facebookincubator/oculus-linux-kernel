// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved.
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

/* Internal precision for fixed-point computations */
#define COEFFICIENT_SCALAR ((s64)10000)

/* Internal exposure units per CEM second */
#define EXPOSURE_PER_SECOND COEFFICIENT_SCALAR

/* Internal exposure units per CEM minute */
#define EXPOSURE_PER_MINUTE (60 * EXPOSURE_PER_SECOND)

/*
 * Internal exposure units (EXPOSURE_PER_SECOND-scaled seconds) per external
 * units of 1/1000 of a CEM minute.
 */
#define EXPOSURE_PER_MILLIMINUTE (EXPOSURE_PER_MINUTE / 1000)

/*
 * The largest usable cap, in CEM minutes. Leaves room to add up two values
 * without overflowing.
 */
#define MAX_EXPOSURE_CAP_MINUTES (S64_MAX / (2 * EXPOSURE_PER_MINUTE))

/**
 * Data for the CEM43 pseudo sensor driver, accessed with dev_[get/set]_drvdata
 */
struct cem43_drvdata {
	/* Access synchronization. */
	struct mutex lock;

	struct device *dev;
	/*
	 * Virtual class device owning the cem43 sysfs attribute group, exposed at
	 * /sys/devices/virtual/cem43/<node-name>/ (device-independent path).
	 */
	struct device *hwdev;
	struct thermal_zone_device *tzd;

	/*
	 * Input thermal zone name from DT. When hotpluggable, this is resolved on
	 * every get_temp() call instead of cached at probe — the underlying
	 * thermal_zone_device may come and go (e.g. tether/HMD pluggable
	 * sensors) and a cached pointer would dangle after unregister.
	 */
	const char *input_tz_name;
	bool hotpluggable;

	/* Input thermal zone, cached here if not hotpluggable. */
	struct thermal_zone_device *tz;

	/* If available, boundaries outside which the temperature is assumed faulty */
	bool fault_handling;
	int fault_lb;
	int fault_ub;

	/* Scaling factor scales to millidegrees */
	int tz_scaling_factor;
	/* CEM target temperature in millidegrees Celsius */
	int cem_target_mdeg;
	/* Cap on CEM exposure, in same units as sum_cem43 */
	s64 cem_cap;
	/* Project temperature forward by this many seconds */
	int projection_seconds;

	/* Last temperature is assumed if the new one cannot be read */
	int last_raw_temp;
	/* Last temperature after running averaging */
	int last_avg_temp;
	/* Exposure in EXPOSURE_PER_SECOND units matching last_avg_temp */
	s64 last_exposure;

	/* Input time averaging logic */
	/* delay between averaged samples, in milliseconds */
	int averaging_delay;
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

	/* CEM43 accumulator state */
	/* Timestamp of the most recent accumulated second */
	time64_t tstamp_last_second;
	/* Timestamp of when the suspension started */
	time64_t tstamp_suspend;
	/* The CEM43 total sum in exposure seconds scaled by EXPOSURE_PER_SECOND */
	s64 sum_cem43;
	/*
	 * We allow the history to be reloaded only once (unless explicitly
	 * cleared), to avoid adding the same history multiple times.
	 */
	bool history_reloaded;
};

static struct class *cem43_class;

static int cem43_thermal_zone_get_temp_scaled(struct thermal_zone_device *tzd,
					      int scaling_factor, int *temp)
{
	int ret = thermal_zone_get_temp(tzd, temp);

	if (ret)
		return ret;

	*temp *= scaling_factor;
	return 0;
}

static inline bool cem43_tz_is_faulty(const struct cem43_drvdata *drv, int temp)
{
	return drv->fault_handling &&
	       (temp < drv->fault_lb || temp > drv->fault_ub);
}

/*
 * Read the input time zone, scaling to millidegrees and ignoring the faulty
 * values.
 *
 * For hotpluggable sensors, the zone pointer is resolved freshly on each
 * call. Returns -ENODATA if the zone cannot be read.
 */
static int cem43_read_tz_temp(struct cem43_drvdata *drv, int *temperature)
{
	int ret;
	struct thermal_zone_device *tz;

	/*
	 * Resolve zone pointer freshly each call for hotpluggable
	 * sensors so an unregister doesn't leave a dangling pointer.
	 */
	if (drv->hotpluggable) {
		tz = thermal_zone_get_zone_by_name(drv->input_tz_name);
		if (IS_ERR(tz))
			tz = NULL;
	} else {
		tz = drv->tz;
	}

	if (!tz) {
		dev_dbg_ratelimited(drv->dev, "%s: zone %s unavailable", __func__,
				    drv->input_tz_name ? drv->input_tz_name : "?");
		return -ENODATA;
	}

	ret = cem43_thermal_zone_get_temp_scaled(tz, drv->tz_scaling_factor,
						 temperature);
	if (ret) {
		dev_err_ratelimited(drv->dev, "%s: error getting temp: %d", tz->type,
				    ret);
	} else if (cem43_tz_is_faulty(drv, *temperature)) {
		dev_err_ratelimited(drv->dev, "%s: got faulty temp %d", tz->type,
				    *temperature);
		ret = -EINVAL;
	}

	return ret;
}

static int cem43_averaging(struct cem43_drvdata *drv, int temp, int temp_err)
{
	/*
	 * There is no guarantee that temperatures get read exactly at
	 * averaging intervals, the reading might be slightly delayed or
	 * there might be additional reads in between. So be fuzzy and
	 * accept whatever was the first sample within each period.
	 */
	const s64 cur_ms = ktime_to_ms(ktime_get_boottime());

	if (cur_ms - drv->hist_ms >=
	    (s64)drv->averaging_delay * drv->averaging_count) {
		/* uh-oh, polling broke, maybe the system was sleeping, reset the history */
		drv->history_size = 0;
	}
	if (drv->history_size == 0) {
		if (temp_err) /* no new valid temperature */
			return 0;
		/*
		 * Initialize the history by pushing in one value.
		 */
		drv->history_size = 1;
		drv->history_idx = 0;
		drv->historic_temps[0] = temp;
		drv->sum_temp = temp;
		drv->hist_ms = cur_ms;
		return temp;
	}

	/*
	 * Advance time in whole averaging delay slices, so if less than
	 * one whole slice had passed yet, no advancing would be done,
	 * and the last value would be returned.
	 */
	while (cur_ms - drv->hist_ms >= drv->averaging_delay) {
		drv->hist_ms += drv->averaging_delay;
		int new_temp = temp;

		if (temp_err) {
			/* fill in temperature with the last value */
			new_temp = drv->historic_temps[drv->history_idx];
		}

		/* insert a new data point */
		if (++drv->history_idx >= drv->averaging_count)
			drv->history_idx = 0; /* wrap on the circular buffer */
		if (drv->history_size < drv->averaging_count) {
			++drv->history_size;
		} else {
			/* drop the oldest value */
			drv->sum_temp -= drv->historic_temps[drv->history_idx];
		}
		drv->historic_temps[drv->history_idx] = new_temp;
		drv->sum_temp += new_temp;
	}

	/* the average */
	return drv->sum_temp / drv->history_size;
}

/*
 * Read the temperature, filter it for validity (substituting
 * some reasonable guess if not valid), and do the time averaging
 * on it.
 */
static int cem43_read_averaged_temp(struct cem43_drvdata *drv)
{
	int temp = 0;
	int read_ret = cem43_read_tz_temp(drv, &temp);

	if (read_ret) {
		/*
		 * Hotpluggable sensors are expected to disappear (e.g. HMD
		 * unplugged). Surface the error to the thermal framework rather
		 * than reporting a stale "last temp" that would mask the loss
		 * of the underlying zones.
		 */
		if (drv->hotpluggable) {
			dev_dbg_ratelimited(drv->dev,
					    "%s: hotpluggable TZ unavailable: %d\n",
					    __func__, read_ret);
			/* If unplugged, assume that below the limit */
			drv->last_raw_temp = 0;
			temp = 0;
			read_ret = 0;
		} else {
			/*
			 * Unable to read new temp, use the last one so the function doesn't
			 * cause the thermal subsystem to error out.
			 */
			dev_warn_ratelimited(drv->dev,
					     "%s: Unable to read TZ temp, re-using last temp: %u\n",
					     __func__, drv->last_raw_temp);

			temp = drv->last_raw_temp;
		}
	} else {
		drv->last_raw_temp = temp;
	}

	if (drv->averaging_count > 1)
		temp = cem43_averaging(drv, temp, read_ret);

	drv->last_avg_temp = temp;

	return temp;
}

/*
 * Compute the CEM multiplier in units of EXPOSURE_PER_SECOND
 * from temperature in millidegrees.
 *
 */
static s64 cem43_temp_to_cem(struct cem43_drvdata *drv, s64 temp)
{
	/*
	 * See the long explanation of the logic in
	 * https://docs.google.com/document/d/1I22xKX_8VEqStUIcM5t9nOhSQW2O22LdvEP1XfcdOV0/edit
	 *
	 * The exponent, converted from milli-degrees
	 * to units of EXPOSURE_PER_SECOND.
	 */
	s64 exp = (temp - drv->cem_target_mdeg) * (EXPOSURE_PER_SECOND / 1000);

	if (exp >= 0) {
		/*
		 * Here we're computing an approximation of 2^exp.
		 * If exp were whole degrees, the answer would be
		 *   1 << whole_exp
		 * but there is also the fractional part
		 *   frac = exp - whole_exp
		 * We linearly approximate the exponentiation of this fractional part
		 * as values between (1 << whole_exp) and (1 << (whole_exp+1)) by
		 * computing
		 *   (1 + frac) << whole_exp
		 */
		/* The whole part, unscaled. */
		s64 whole_exp = exp / EXPOSURE_PER_SECOND;
		/* (1 + frac), scaled */
		s64 frac1 = EXPOSURE_PER_SECOND +
			    (exp - whole_exp * EXPOSURE_PER_SECOND);
		/* Avoid overflow on very large values. */
		if (whole_exp > 15) {
			whole_exp = 15;
			frac1 = EXPOSURE_PER_SECOND * 2 - 1;
		}
		return frac1 << whole_exp;
	}

	/*
	 * Here we're computing an approximation of 4^exp,
	 * with a negative exp. So we start by converting it
	 * to 2^(exp*2), and then making the exponent positive
	 * as (1/2)^(-exp*2).
	 */
	s64 nexp = -exp << 1;
	/*
	 * Here we're computing an approximation of (1/2)^nexp.
	 * If nexp were whole degrees, the answer would be
	 *   1 >> whole_nexp
	 * but there is also the fractional part
	 *   frac = nexp - whole_nexp
	 * We linearly approximate the exponentiation of this fractional part
	 * as values between (1 >> whole_nexp) and (1 >> (whole_nexp+1)) by
	 * computing
	 *   (1 - frac/2) >> whole_nexp
	 */
	/* The whole part, unscaled. */
	s64 whole_nexp = nexp / EXPOSURE_PER_SECOND;
	/* (1 - frac/2), scaled */
	s64 frac1 = EXPOSURE_PER_SECOND -
				((nexp - whole_nexp * EXPOSURE_PER_SECOND) >> 1);
	/* Large values of nexp produce 0. */
	if (whole_nexp >= 8)
		return 0;
	return frac1 >> whole_nexp;
}

/* Get the monotonic boot time in seconds */
static inline time64_t cem43_ktime_boottime_s(void)
{
	const s64 cur_ms = ktime_to_ms(ktime_get_boottime());

	return div64_s64(cur_ms, 1000);
}

/* Limit the exposure to the cap. */
static inline s64 cem43_cap_exposure(const struct cem43_drvdata *drv, s64 cem)
{
	return min_t(s64, cem, drv->cem_cap);
}

/*
 * This returns the exposure adjusted for the projection into the future.
 * It's measured in CEM-seconds and scaled with EXPOSURE_PER_SECOND.
 */
static s64 cem43_compute_cem(struct cem43_drvdata *drv)
{
	s64 cem = drv->sum_cem43;
	/* Push in the projection, which may be above the cap. */
	cem += drv->projection_seconds * drv->last_exposure;
	return cem;
}

static void cem43_accumulate_exposure(struct cem43_drvdata *drv, s64 exposure)
{
	time64_t now_sec = cem43_ktime_boottime_s();

	drv->last_exposure = exposure;

	if (now_sec > drv->tstamp_last_second) {
		/* Fill the intermediate time with the current exposure. */
		drv->sum_cem43 += exposure * (now_sec - drv->tstamp_last_second);
		drv->sum_cem43 = cem43_cap_exposure(drv, drv->sum_cem43);
		drv->tstamp_last_second = now_sec;
	}
}

static void cem43_clear_history(struct cem43_drvdata *drv)
{
	drv->tstamp_last_second = cem43_ktime_boottime_s();
	drv->sum_cem43 = 0;
	drv->history_reloaded = false;
}

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
static int cem43_get_temp(struct thermal_zone_device *tz, int *temperature)
#else
static int cem43_get_temp(void *data, int *temperature)
#endif
{
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct cem43_drvdata *drv = tz->devdata;
#else
	struct cem43_drvdata *drv = data;
#endif

	mutex_lock(&drv->lock);

	s64 temp = cem43_read_averaged_temp(drv);
	s64 exposure = cem43_temp_to_cem(drv, temp);
	cem43_accumulate_exposure(drv, exposure);

	/* Convert the CEM into exposure minutes, with 1/1000 precision */
	*temperature = (int)div64_s64(cem43_compute_cem(drv),
				      EXPOSURE_PER_MILLIMINUTE);

	mutex_unlock(&drv->lock);
	return 0;
}

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
static const struct thermal_zone_device_ops cem43_thermal_ops = {
#else
static const struct thermal_zone_of_device_ops cem43_thermal_ops = {
#endif
	.get_temp = cem43_get_temp,
};

static ssize_t cem_history_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret, pos = 0;
	time64_t now, now_k;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	now = ktime_get_real_seconds();
	now_k = cem43_ktime_boottime_s();
	/* Adjust the system time to match the last collected second */
	now -= (now_k - drv->tstamp_last_second);

	/*
	 * The data format is:
	 * 1. Wall clock time in seconds.
	 * 2. The current time since boot in seconds (not really needed any more
	 *    but present historically).
	 * 3. The total sum of the exposure seconds, scaled by EXPOSURE_PER_SECOND.
	 * 4. Another copy of (3), as an easy verification against corruption.
	 */
	pos = scnprintf(buf, PAGE_SIZE, "%lld %lld %lld %lld\n", now, drv->tstamp_last_second,
			drv->sum_cem43, drv->sum_cem43);

	mutex_unlock(&drv->lock);

	return pos;
}

/*
 * Reduce the exposure by the pause in seconds.
 * A negative pause is ignored.
 */
static void reduce_exposure(struct cem43_drvdata *drv, s64 seconds)
{
	if (seconds <= 0)
		return;

	/*
	 * The exposure never exceeds the cap, so a pause longer than the cap
	 * in seconds reduces it to 0 in any case. Cutting it short
	 * here also keeps the scaling below from overflowing.
	 */
	if (seconds > div64_s64(drv->cem_cap, EXPOSURE_PER_SECOND)) {
		drv->sum_cem43 = 0;
		return;
	}

	drv->sum_cem43 -= seconds * EXPOSURE_PER_SECOND;
	if (drv->sum_cem43 < 0)
		drv->sum_cem43 = 0;
}

static ssize_t cem_history_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	time64_t dump_systime, dump_ktime;
	time64_t now, boot_systime;
	s64 dump_sum_cem43, copy_dump_sum_cem43;
	s64 current_exposure;
	int ret;

	if (!strncmp(buf, "clear", 5)) {
		/* A special case, clear the history */
		ret = mutex_lock_interruptible(&drv->lock);
		if (ret < 0) {
			dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
				 ret);
			return ret;
		}

		cem43_clear_history(drv);

		mutex_unlock(&drv->lock);
		return count;
	}

	if (sscanf(buf, "%lld %lld %lld %lld", &dump_systime, &dump_ktime,
		   &dump_sum_cem43, &copy_dump_sum_cem43) != 4)
		return -EINVAL;
	if (dump_sum_cem43 < 0)
		return -EINVAL;

	if (dump_sum_cem43 != copy_dump_sum_cem43) {
		dev_warn_ratelimited(drv->dev, "Mismatch in two copies of cem43 sum: %lld vs %lld",
				dump_sum_cem43, copy_dump_sum_cem43);
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 ret);
		return ret;
	}

	/* The system time at which the current boot started */
	now = ktime_get_real_seconds();
	boot_systime = now - cem43_ktime_boottime_s();

	if (drv->history_reloaded) {
		dev_warn_ratelimited(drv->dev, "Ignored the repeated history load");
		goto unlock;
	}
	if (dump_systime > now) {
		dev_warn_ratelimited(drv->dev, "Ignored the history load from %lld after boot time %lld",
			dump_systime, now);
		goto unlock;
	}
	if (dump_sum_cem43 > drv->cem_cap) {
		dev_warn_ratelimited(drv->dev, "Rejecting the reload of exposure %lld above the cap of %lld minutes",
			div64_s64(dump_sum_cem43, EXPOSURE_PER_MINUTE),
			div64_s64(drv->cem_cap, EXPOSURE_PER_MINUTE));
		goto unlock;
	}

	/* Preserve the exposure accumulated since boot against reduction. */
	current_exposure = drv->sum_cem43;

	cem43_clear_history(drv);

	drv->sum_cem43 = dump_sum_cem43;
	/* Reduce the exposure for the time the device had been powered off. */
	reduce_exposure(drv, boot_systime - dump_systime);

	drv->sum_cem43 = cem43_cap_exposure(drv, drv->sum_cem43 + current_exposure);

	drv->history_reloaded = true;

unlock:
	mutex_unlock(&drv->lock);

	return count;
}

static ssize_t cem_target_mdeg_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", drv->cem_target_mdeg);
	mutex_unlock(&drv->lock);

	return ret;
}

static ssize_t cem_cap_minutes_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret;
	s64 minutes;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	minutes = div64_s64(drv->cem_cap, EXPOSURE_PER_MINUTE);
	/* Capping at the maximum is the same as not capping at all */
	if (minutes >= MAX_EXPOSURE_CAP_MINUTES)
		minutes = 0;

	ret = scnprintf(buf, PAGE_SIZE, "%lld\n", minutes);
	mutex_unlock(&drv->lock);

	return ret;
}

static ssize_t cem_cap_minutes_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	s64 minutes;
	int ret;

	ret = kstrtos64(buf, 10, &minutes);
	if (ret < 0)
		return ret;

	if (minutes < 0) {
		dev_warn(dev, "%s: the cap must be non-negative", __func__);
		return -EINVAL;
	}

	/* 0 means no cap, and so does anything above the maximum */
	if (minutes == 0 || minutes > MAX_EXPOSURE_CAP_MINUTES)
		minutes = MAX_EXPOSURE_CAP_MINUTES;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 ret);
		return ret;
	}

	drv->cem_cap = minutes * EXPOSURE_PER_MINUTE;
	/* Apply the new cap to what has been accumulated already */
	drv->sum_cem43 = cem43_cap_exposure(drv, drv->sum_cem43);

	mutex_unlock(&drv->lock);

	return count;
}

static ssize_t projection_seconds_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", drv->projection_seconds);
	mutex_unlock(&drv->lock);

	return ret;
}

static ssize_t last_raw_temp_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", drv->last_raw_temp);
	mutex_unlock(&drv->lock);

	return ret;
}

static ssize_t last_avg_temp_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", drv->last_avg_temp);
	mutex_unlock(&drv->lock);

	return ret;
}

static ssize_t last_exposure_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%lld\n", drv->last_exposure);
	mutex_unlock(&drv->lock);

	return ret;
}

#if IS_ENABLED(CONFIG_THERMAL_EMULATION)
static ssize_t emul_skip_sec_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	int ret;
	int seconds;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 ret);
		return ret;
	}

	ret = kstrtoint(buf, 10, &seconds);
	if (ret < 0) {
		mutex_unlock(&drv->lock);
		return ret;
	}

	if (seconds < 0) {
		dev_warn(dev, "%s: skip seconds must be non-negative", __func__);
		mutex_unlock(&drv->lock);
		return -EINVAL;
	}

	/*
	 * This simulates skipping a number of seconds, just as if the
	 * device were sleeping.
	 */
	reduce_exposure(drv, seconds);

	mutex_unlock(&drv->lock);

	return count;
}
#endif /* CONFIG_THERMAL_EMULATION */

static DEVICE_ATTR_RW(cem_history);
static DEVICE_ATTR_RO(cem_target_mdeg);
static DEVICE_ATTR_RW(cem_cap_minutes);
static DEVICE_ATTR_RO(projection_seconds);
static DEVICE_ATTR_RO(last_raw_temp);
static DEVICE_ATTR_RO(last_avg_temp);
static DEVICE_ATTR_RO(last_exposure);
#if IS_ENABLED(CONFIG_THERMAL_EMULATION)
static DEVICE_ATTR_WO(emul_skip_sec);
#endif

static struct attribute *cem43_attrs[] = {
	&dev_attr_cem_history.attr,
	&dev_attr_cem_target_mdeg.attr,
	&dev_attr_cem_cap_minutes.attr,
	&dev_attr_projection_seconds.attr,
	&dev_attr_last_raw_temp.attr,
	&dev_attr_last_avg_temp.attr,
	&dev_attr_last_exposure.attr,
#if IS_ENABLED(CONFIG_THERMAL_EMULATION)
	&dev_attr_emul_skip_sec.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(cem43);

static int cem43_parse_thermal_zone_fault_dt(struct cem43_drvdata *drv)
{
	int ret;
	u32 fault_bounds[2];

	ret = of_property_read_u32_array(drv->dev->of_node,
					 "thermal-zone-fault-estimator-bounds",
					 fault_bounds, 2);
	if (ret == -EINVAL)
		return 0;
	else if (ret < 0)
		return ret;

	drv->fault_handling = true;
	drv->fault_lb = fault_bounds[0];
	drv->fault_ub = fault_bounds[1];

	return 0;
}

static int cem43_parse_thermal_zones_dt(struct cem43_drvdata *drv)
{
	int ret = 0;
	const char *param_string;

	drv->hotpluggable =
		of_property_read_bool(drv->dev->of_node, "meta,hotpluggable");

	ret = of_property_read_string(drv->dev->of_node, "input-thermal-zone",
				      &param_string);
	if (ret < 0) {
		dev_err(drv->dev, "Missing required input-thermal-zone property");
		return -ENODATA;
	}

	/*
	 * Persist the zone name. For hotpluggable sensors we re-lookup
	 * by name on every get_temp() call; for non-hotpluggable ones
	 * the name is just kept for diagnostics.
	 */
	drv->input_tz_name = devm_kstrdup(drv->dev, param_string, GFP_KERNEL);
	if (!drv->input_tz_name)
		return -ENOMEM;

	drv->tz = thermal_zone_get_zone_by_name(drv->input_tz_name);
	if (IS_ERR(drv->tz)) {
		if (drv->hotpluggable) {
			/*
			 * Zone may not be registered yet — that's fine
			 * for hotpluggable sensors; we'll resolve at
			 * read time.
			 */
			dev_dbg(drv->dev,
				"hotpluggable zone %s not present at probe",
				drv->input_tz_name);
			drv->tz = NULL;
		} else {
			ret = -EPROBE_DEFER;
			dev_dbg(drv->dev, "sensor %s get_zone error: %ld",
				drv->input_tz_name, PTR_ERR(drv->tz));
			return ret;
		}
	}

	ret = of_property_read_u32(drv->dev->of_node, "input-scaling-factor",
				   &drv->tz_scaling_factor);
	if (ret < 0) {
		dev_dbg(drv->dev, "No input-scaling-factor specified, using 1");
		drv->tz_scaling_factor = 1;
		ret = 0;
	}

	ret = cem43_parse_thermal_zone_fault_dt(drv);
	if (ret < 0) {
		dev_dbg(drv->dev, "Failed parsing tz fault configuration: %d", ret);
		goto out;
	}

out:

	return ret;
}

static int cem43_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 cem_cap_mins;
	struct cem43_drvdata *drv;
	struct thermal_zone_device *tzd = NULL;

	dev_dbg(&pdev->dev, "probing");

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, drv);

	mutex_init(&drv->lock);

	drv->averaging_delay = 0;
	ret = of_property_read_u32(pdev->dev.of_node, "averaging-delay",
				   &drv->averaging_delay);
	if (ret == 0) {
		if (drv->averaging_delay < 1) {
			dev_err(&pdev->dev, "averaging-delay must be positive");
			return -EINVAL;
		}
	}

	drv->averaging_count = 0;
	ret = of_property_read_u32(pdev->dev.of_node, "averaging-count",
				   &drv->averaging_count);

	if ((drv->averaging_delay > 0) ^ (ret == 0)) {
		dev_err(&pdev->dev,
			"averaging-count and averaging-delay must be specified both or neither");
		return -EINVAL;
	}

	if (ret < 0) {
		/* The defaults average for 1 second every 1/10 seconds */
		drv->averaging_count = 10;
		drv->averaging_delay = 100;
	} else {
		if (drv->averaging_count < 1) {
			dev_err(&pdev->dev, "averaging-count must be positive");
			return -EINVAL;
		}
	}

	drv->historic_temps =
		devm_kcalloc(&pdev->dev, drv->averaging_count,
			     sizeof(*drv->historic_temps), GFP_KERNEL);
	if (!drv->historic_temps)
		return -ENOMEM;

	/* Parse cem-target-mdeg, the CEM target temperature */
	ret = of_property_read_u32(pdev->dev.of_node, "cem-target-mdeg",
				   &drv->cem_target_mdeg);
	if (ret < 0) {
		/* the default is CEM43, 43 degrees */
		drv->cem_target_mdeg = 43000;
	}

	/*
	 * Parse cem-cap-minutes, the upper bound on the accumulated exposure.
	 * By default, and for any larger value, use a bound that keeps the
	 * exposure arithmetic from overflowing.
	 */
	drv->cem_cap = MAX_EXPOSURE_CAP_MINUTES * EXPOSURE_PER_MINUTE;
	if (of_property_read_u32(pdev->dev.of_node, "cem-cap-minutes", &cem_cap_mins) == 0 &&
	    cem_cap_mins != 0 && (s64)cem_cap_mins < MAX_EXPOSURE_CAP_MINUTES)
		drv->cem_cap = (s64)cem_cap_mins * EXPOSURE_PER_MINUTE;

	/* Parse projection-seconds with default of 0 */
	ret = of_property_read_u32(pdev->dev.of_node, "projection-seconds",
				   &drv->projection_seconds);
	if (ret < 0)
		drv->projection_seconds = 0;

	if (drv->projection_seconds < 0) {
		dev_err(&pdev->dev, "projection-seconds must be non-negative");
		return -EINVAL;
	}

	ret = cem43_parse_thermal_zones_dt(drv);
	if (ret == -EPROBE_DEFER) {
		return ret;
	} else if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse data for sensor: %d", ret);
		return ret;
	}

	drv->tstamp_last_second = cem43_ktime_boottime_s();

	drv->hwdev = device_create_with_groups(cem43_class, NULL, MKDEV(0, 0), drv,
					       cem43_groups, "%s",
					       pdev->dev.of_node->name);
	if (IS_ERR(drv->hwdev)) {
		ret = PTR_ERR(drv->hwdev);
		dev_err(&pdev->dev, "Failed to create cem43 class device: %d", ret);
		return ret;
	}

	/*
	 * Register the thermal zone last: registration triggers a synchronous
	 * get_temp() call, so all state it reads (accum_horizon,
	 * tstamp_last_second) must already be initialized.
	 */
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	tzd = devm_thermal_of_zone_register(&pdev->dev, 0, drv,
					    &cem43_thermal_ops);
#else
	tzd = thermal_zone_of_sensor_register(&pdev->dev, 0, drv,
					      &cem43_thermal_ops);
#endif
	if (IS_ERR(tzd)) {
		ret = PTR_ERR(tzd);
		device_unregister(drv->hwdev);
		dev_err(&pdev->dev, "Sensor register error: %d", ret);
		return ret;
	}
	drv->tzd = tzd;

	return 0;
}

static int cem43_remove(struct platform_device *pdev)
{
	struct cem43_drvdata *drv =
		(struct cem43_drvdata *)platform_get_drvdata(pdev);

#if (KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE)
	thermal_zone_of_sensor_unregister(&pdev->dev, drv->tzd);
#endif

	device_unregister(drv->hwdev);

	mutex_destroy(&drv->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)

static int cem43_suspend(struct device *dev)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);

	mutex_lock(&drv->lock);

	s64 temp = cem43_read_averaged_temp(drv);
	s64 exposure = cem43_temp_to_cem(drv, temp);

	cem43_accumulate_exposure(drv, exposure);

	drv->tstamp_suspend = cem43_ktime_boottime_s();

	mutex_unlock(&drv->lock);

	return 0;
}

static int cem43_resume(struct device *dev)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);

	mutex_lock(&drv->lock);

	/* Restart accumulation from the current time */
	drv->tstamp_last_second = cem43_ktime_boottime_s();
	/*
	 * Assume that while suspended, the device was doffed, so
	 * reduce the exposure.
	 */
	reduce_exposure(drv, drv->tstamp_last_second - drv->tstamp_suspend);
	drv->tstamp_suspend = 0;

	mutex_unlock(&drv->lock);

	return 0;
}

#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops cem43_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cem43_suspend, cem43_resume)
};

static const struct of_device_id cem43_table[] = {
	{ .compatible = "oculus,cem43-pseudo-sensor" },
	{}
};
MODULE_DEVICE_TABLE(of, cem43_table);

static struct platform_driver cem43_driver = {
	.probe = cem43_probe,
	.remove = cem43_remove,
	.driver = {
		.name = "cem43-pseudo-sensor",
		.pm = &cem43_pm_ops,
		.of_match_table = cem43_table,
	},
};

static int __init cem43_init(void)
{
	int ret;

	cem43_class = class_create(THIS_MODULE, "cem43");
	if (IS_ERR(cem43_class))
		return PTR_ERR(cem43_class);

	ret = platform_driver_register(&cem43_driver);
	if (ret)
		class_destroy(cem43_class);

	return ret;
}
module_init(cem43_init);

static void __exit cem43_exit(void)
{
	platform_driver_unregister(&cem43_driver);
	class_destroy(cem43_class);
}
module_exit(cem43_exit);

MODULE_ALIAS("cem43_pseudo_sensor");
MODULE_LICENSE("GPL v2");
