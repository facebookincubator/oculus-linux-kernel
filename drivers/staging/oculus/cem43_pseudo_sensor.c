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
#define COEFFICIENT_SCALAR 10000

/*
 * We expect that the user would not be touching or carrying a hot
 * device longer than this.
 */
#define EXPECTED_HOT_TOUCH_MINUTES 5

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
	/* CEM horizon in minutes */
	int cem_horizon_minutes;
	/* CEM target temperature in millidegrees Celsius */
	int cem_target_mdeg;
	/* Project temperature forward by this many seconds */
	int projection_seconds;

	/* Last temperature is assumed if the new one cannot be read */
	int last_raw_temp;
	/* Last temperature after running averaging */
	int last_avg_temp;
	/* Exposure in COEFFICIENT_SCALAR units matching last_avg_temp */
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

	/* Accumulator state, all sums are in exposure seconds scaled by COEFFICIENT_SCALAR */
	/*
	 * For the last minute we carry only the sum, and reset it when
	 * we collect the whole minute, at time (cem43_ktime_boottime_s() % 60 == 0).
	 */
	s64 sum_last_minute;
	/* Timestamp of the most recent accumulated second */
	time64_t tstamp_last_second;
	/*
	 * The debugging allows to emulate skipping the seconds, and the
	 * total of skipped seconds is accumulated here.
	 */
	time64_t debug_skipped_seconds;
	/*
	 * The full horizon circular buffer, sliced in minutes, dynamically sized
	 * to contain cem_horizon_minutes elements, the index is
	 *   cem43_idx_in_horizon(drv, cem43_time_sec_to_min(cem43_ktime_boottime_s())
	 *   = (cem43_time_sec_to_min(cem43_ktime_boottime_s()) % cem_horizon_minutes).
	 * The current point is determined by tstamp_last_second.
	 */
	s64 *accum_horizon;
	s64 sum_horizon;
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
 * Compute the CEM multiplier in units of (1/COEFFICIENT_SCALAR)
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
	 * to units of (1/COEFFICIENT_SCALAR).
	 */
	s64 exp = (temp - drv->cem_target_mdeg) * (COEFFICIENT_SCALAR / 1000);

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
		s64 whole_exp = exp / COEFFICIENT_SCALAR;
		/* (1 + frac), scaled */
		s64 frac1 = COEFFICIENT_SCALAR +
			    (exp - whole_exp * COEFFICIENT_SCALAR);
		/* Avoid overflow on very large values. */
		if (whole_exp > 15) {
			whole_exp = 15;
			frac1 = COEFFICIENT_SCALAR * 2 - 1;
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
	s64 whole_nexp = nexp / COEFFICIENT_SCALAR;
	/* (1 - frac/2), scaled */
	s64 frac1 = COEFFICIENT_SCALAR -
				((nexp - whole_nexp * COEFFICIENT_SCALAR) >> 1);
	/* Large values of nexp produce 0. */
	if (whole_nexp > 12)
		return 0;
	return frac1 >> whole_nexp;
}

/* Get the monotonic boot time in seconds */
static inline time64_t cem43_ktime_boottime_s(void)
{
	const s64 cur_ms = ktime_to_ms(ktime_get_boottime());

	return div64_s64(cur_ms, 1000);
}

static inline time64_t cem43_time_sec_to_min(time64_t sec)
{
	return div64_s64(sec, 60);
}

static int cem43_idx_in_minute(int idx)
{
	idx %= 60;
	if (idx < 0)
		idx += 60;
	return idx;
}

static int cem43_idx_in_horizon(struct cem43_drvdata *drv, int idx)
{
	int mins = drv->cem_horizon_minutes;

	idx %= mins;
	if (idx < 0)
		idx += mins;
	return idx;
}

/*
 * This returns the exposure adjusted for the last fractional
 * minute and the projection into the future.
 * It's measured in CEM-seconds and scaled with COEFFICIENT_SCALAR.
 */
static s64 cem43_compute_cem(struct cem43_drvdata *drv)
{
	int horiz_idx;
	s64 cem = drv->sum_horizon;
	/* Whole minutes in timestamp */
	s64 tstamp_min = cem43_time_sec_to_min(drv->tstamp_last_second);
	/* How many seconds will be pushed out from the horizon */
	int pushout_sec =
		drv->tstamp_last_second - (tstamp_min * 60) + drv->projection_seconds;

	/*
	 * The minutes at the end of horizon are getting pushed out,
	 * and the earliest minute is in the slot where the current minute
	 * will be written after it completes, (tstamp_min + 1).
	 */
	horiz_idx = cem43_idx_in_horizon(drv, tstamp_min + 1);
	for (; pushout_sec > 60; pushout_sec -= 60) {
		cem -= drv->accum_horizon[horiz_idx];
		horiz_idx = cem43_idx_in_horizon(drv, horiz_idx + 1);
	}
	/* Push out the last few seconds in proportion to their minute */
	cem -= div64_s64(drv->accum_horizon[horiz_idx] * pushout_sec, 60);

	/* Push in the last minute */
	cem += drv->sum_last_minute;
	/* Push in the projection */
	cem += drv->projection_seconds * drv->last_exposure;
	return cem;
}

static void cem43_accumulate_exposure(struct cem43_drvdata *drv, s64 exposure)
{
	int interval, interval_mins, left_in_minute, horiz_idx;

	time64_t now_sec = cem43_ktime_boottime_s() + drv->debug_skipped_seconds;

	drv->last_exposure = exposure;

	if (now_sec == drv->tstamp_last_second)
		return; /* a second hasn't passed yet */

	/*
	 * Fill the intermediate time with the current exposure.
	 * Even if the device was sleeping, the temperature at wake-up
	 * might be a good indication of temperature in between.
	 */
	interval = (int)(now_sec - drv->tstamp_last_second);

	/* First, sum in the current minute */
	left_in_minute = 60 - cem43_idx_in_minute(drv->tstamp_last_second);
	if (interval < left_in_minute) {
		/* The current minute is not closed yet, add to it */
		drv->sum_last_minute += interval * exposure;
		drv->tstamp_last_second += interval;
		return;
	}

	/* Close the current minute */
	drv->sum_last_minute += left_in_minute * exposure;
	drv->tstamp_last_second += left_in_minute;
	interval -= left_in_minute;

	/* Insert the current minute into the horizon storage */
	horiz_idx = cem43_idx_in_horizon(drv, cem43_time_sec_to_min(drv->tstamp_last_second));
	drv->sum_horizon +=
		drv->sum_last_minute - drv->accum_horizon[horiz_idx];
	drv->accum_horizon[horiz_idx] = drv->sum_last_minute;

	/* See if there are whole minnutes to be filled in the horizon */
	interval_mins = interval / 60;
	if (interval_mins >= drv->cem_horizon_minutes) {
		/* Uh-oh, it's been a very long time, so reset the accumulator to 0 */
		memset(drv->accum_horizon, 0,
		       sizeof(*drv->accum_horizon) * drv->cem_horizon_minutes);
		drv->sum_horizon = 0;
	} else {
		for (; interval_mins > 0; interval_mins--) {
			horiz_idx = cem43_idx_in_horizon(drv, horiz_idx + 1);
			/*
			 * Could potentially fill with the current exposure, but
			 * that would probably be overly pessimistic on long intervals.
			 * Fill only the last few minutes with the current exposure.
			 */
			if (interval_mins <= EXPECTED_HOT_TOUCH_MINUTES) {
				drv->sum_horizon +=
					exposure * 60 -
					drv->accum_horizon[horiz_idx];
				drv->accum_horizon[horiz_idx] = exposure * 60;
			} else {
				drv->sum_horizon -=
					drv->accum_horizon[horiz_idx];
				drv->accum_horizon[horiz_idx] = 0;
			}
		}
	}

	/* Fill the start of the new second */
	interval = interval % 60;
	drv->sum_last_minute = exposure * interval;
	drv->tstamp_last_second = now_sec;
}

static void cem43_clear_history(struct cem43_drvdata *drv)
{
	drv->sum_last_minute = 0;
	drv->tstamp_last_second = cem43_ktime_boottime_s();
	drv->debug_skipped_seconds = 0;
	memset(drv->accum_horizon, 0,
	       sizeof(*drv->accum_horizon) * drv->cem_horizon_minutes);
	drv->sum_horizon = 0;
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
				      60 * (COEFFICIENT_SCALAR / 1000));

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

/*
 * Find the index of earliest non-0 minute in the horizon buffer,
 * to skip the tail zeroes. If all the data is 0, will return the
 * index of the most recent minute.
 */
static int cem43_find_earliest_non0_minute(struct cem43_drvdata *drv)
{
	int hidx, hidx_end;

	/* hidx_end is the last written minute */
	hidx_end = cem43_idx_in_horizon(drv, cem43_time_sec_to_min(drv->tstamp_last_second));
	for (hidx = cem43_idx_in_horizon(drv, hidx_end + 1); hidx != hidx_end;
	     hidx = cem43_idx_in_horizon(drv, hidx + 1)) {
		if (drv->accum_horizon[hidx] != 0)
			break;
	}
	return hidx;
}

static ssize_t cem_history_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret, pos = 0;
	time64_t now, now_k;
	int hidx, hidx_end;

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
	 * The data contains:
	 * 1. Wall clock time in seconds.
	 * 2. The current time since boot, since the minutes are expressed in
	 * multiples of 60 from this.
	 * 3. The length of horizon in minutes.
	 * 4. The total sum of the horizon.
	 * 5. The sum of the last minute.
	 * 6. The values of sums for every minute, staring from the most recently completed one
	 * and going backwards (the tail 0s are skipped). The tail minutes may get skipped
	 * if the buffer runs out, then the reader is supposed to fill the rest from the
	 * total sum and length of the horizon.
	 */
	pos = scnprintf(buf, PAGE_SIZE, "%lld %lld %d %lld %lld\n", now, drv->tstamp_last_second,
			drv->cem_horizon_minutes, drv->sum_horizon, drv->sum_last_minute);

	/*
	 * Now fill as much as possible of the buffer with per-minute data,
	 * going backwards to the earliest non-0 minute.
	 */
	hidx = cem43_idx_in_horizon(drv, cem43_time_sec_to_min(drv->tstamp_last_second));
	hidx_end = cem43_find_earliest_non0_minute(drv);
	if (hidx != hidx_end || drv->accum_horizon[hidx] != 0) {
		for (; ; hidx = cem43_idx_in_horizon(drv, hidx - 1)) {
			if (pos >= PAGE_SIZE - 25)
				break; /* The buffer would overflow */
			pos += scnprintf(buf + pos, PAGE_SIZE - pos, " %lld",
					 drv->accum_horizon[hidx]);

			/* Loop condition is a postcondition. */
			if (hidx == hidx_end)
				break;
		}
	}
	if (pos <= PAGE_SIZE - 2)
		pos += scnprintf(buf + pos, PAGE_SIZE - pos, "\n");

	mutex_unlock(&drv->lock);

	return pos;
}

/*
 * Remix a sum of per-second CEM43 values in a minute to an offset
 * minute boundary.
 * offset_sec - how many seconds from the current minute get carried into
 *   the next processed minute, and replaced with the same number of seconds
 *   carried from the previous processed minute.
 * carry - on input the sum carried in from the previous processed minute,
 *   on output the sum carried from the current minute to the next processed one.
 * sum_minute - the sum of current minute that gets updated in place.
 * collected_sec - how many seconds were collected in sum_minute (would be 60
 *   for a full minute). Must not be 0, a minute with no seconds collected
 *   cannot be used for remixing because it doesn't have any data to
 *   contribute to the mix (skip it and move on to the next minute).
 */
static inline void cem43_remix_minute(s64 offset_sec, s64 *carry,
				      s64 *sum_minute, s64 collected_sec)
{
	s64 new_carry = div64_s64(*sum_minute * offset_sec, collected_sec);

	*sum_minute = *sum_minute - new_carry + *carry;
	*carry = new_carry;
}

/*
 * If a minute's buffer was unfilled in the horizon buffer,
 * fill a new value into it.
 * hidx - index of the minute in the horizon buffer
 * sum_minute - new sum for this minute
 */
static inline void cem43_maybe_overwrite(struct cem43_drvdata *drv, int hidx, s64 sum_minute)
{
	if (drv->accum_horizon[hidx] == 0) {
		drv->accum_horizon[hidx] = sum_minute;
		drv->sum_horizon += sum_minute;
	}
}

static ssize_t cem_history_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	int dump_h_minutes;
	time64_t dump_systime, dump_ktime;
	time64_t now, now_k, now_kmin, now_ksec;
	s64 dump_sum_horizon, dump_sum_minute;
	s64 dump_kmin, dump_ksec, dump_offset, dump_offset_min, dump_offset_sec;
	s64 sum_carry = 0;
	int hidx, hidx_end;
	int pos = 0;
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

	if (sscanf(buf, "%lld %lld %d %lld %lld%n", &dump_systime, &dump_ktime,
		   &dump_h_minutes, &dump_sum_horizon, &dump_sum_minute,
		   &pos) != 5)
		return -EINVAL;
	if (dump_ktime < 0 || dump_h_minutes <= 0 || dump_sum_horizon < 0 || dump_sum_minute < 0)
		return -EINVAL;
	buf += pos;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 ret);
		return ret;
	}

	now = ktime_get_real_seconds();
	now_k = cem43_ktime_boottime_s();
	/* Adjust the system time to match the last collected second */
	now -= (now_k - drv->tstamp_last_second);

	/* The current minute boundaries from boot time */
	now_kmin = cem43_time_sec_to_min(drv->tstamp_last_second);
	now_ksec = drv->tstamp_last_second - 60 * now_kmin;

	/* The minute boundaries from boot time in the dump */
	dump_kmin = cem43_time_sec_to_min(dump_ktime);
	dump_ksec = dump_ktime - 60 * dump_kmin;

	/* How much time has passed since the dump between the ends of last minutes */
	dump_offset = (now - now_ksec) - (dump_systime - dump_ksec);
	/* Silently ignore the dumps from the future. */
	if (dump_offset < 0) {
		dev_warn_ratelimited(drv->dev, "Ignored the history load from the future");
		goto success;
	}

	dump_offset_min = cem43_time_sec_to_min(dump_offset);
	/* Silently ignore the dumps from the distant past. */
	if (dump_offset_min > drv->cem_horizon_minutes - 2)
		goto success;

	/*
	 * The minute boundaries are misaligned by this amount, which
	 * means that when converting from dump minutes to our minutes
	 * we need to fill in this much time from the next minute (and
	 * correspondingly, drop this much time from the current minute).
	 */
	dump_offset_sec = dump_offset - 60 * dump_offset_min;

	/*
	 * Convert the last, possibly partial minute
	 */
	if (dump_ksec > 0 && dump_ksec >= dump_offset_sec) {
		cem43_remix_minute(dump_offset_sec, &sum_carry, &dump_sum_minute, dump_ksec);
	} else {
		/* Carry the whole remaining partial minute over */
		sum_carry = dump_sum_minute;
		dump_sum_minute = 0;
	}

	/*
	 * This prevents the very last minute of the horizon from being
	 * filled from the history, but it makes the loop termination
	 * condition much more convenient, and this minute will be
	 * overwritten soon by the new data anyway.
	 */
	hidx_end = cem43_idx_in_horizon(drv, now_kmin + 1);

	/* dump_sum_minute contains data for the next minute after dump_kmin */
	hidx = cem43_idx_in_horizon(drv, now_kmin - dump_offset_min + 1);

	/*
	 * Avoid updating the current minute, i.e. ignore dump_offset_min == 0. This
	 * should never happen in reality because the minute boundary starts with the
	 * current boot time, and the pre-reboot dump could not have happened after
	 * reboot.
	 */
	if (dump_offset_min > 0)
		cem43_maybe_overwrite(drv, hidx, dump_sum_minute);
	dump_h_minutes--;

	/* Fill in the whole minutes, remixing them for the new boundary */
	for (hidx = cem43_idx_in_horizon(drv, hidx - 1);
	     hidx != hidx_end && dump_sum_horizon + sum_carry > 0;
	     hidx = cem43_idx_in_horizon(drv, hidx - 1)) {
		if (sscanf(buf, " %lld%n", &dump_sum_minute, &pos) < 1)
			break; /* We're out of data. */
		buf += pos;
		dump_sum_horizon -= dump_sum_minute;
		dump_h_minutes--;

		cem43_remix_minute(dump_offset_sec, &sum_carry, &dump_sum_minute, 60);
		cem43_maybe_overwrite(drv, hidx, dump_sum_minute);
	}
	if (hidx != hidx_end && dump_sum_horizon + sum_carry > 0) {
		/* Ran out of data, fill assuming the remaining horizon is flat */
		if (dump_sum_horizon > 0 && dump_h_minutes > 0)
			dump_sum_horizon = div64_s64(dump_sum_horizon, dump_h_minutes);
		else
			dump_sum_horizon = 0;
		/* Now dump_sum_horizon contains the flat per-minute filling */

		/* Remix and write the last potentially uneven minute */
		dump_h_minutes--;
		dump_sum_minute = dump_sum_horizon;
		cem43_remix_minute(dump_offset_sec, &sum_carry, &dump_sum_minute, 60);
		cem43_maybe_overwrite(drv, hidx, dump_sum_minute);

		if (dump_sum_horizon > 0) {
			dump_sum_minute = dump_sum_horizon;
			/* Write the even-leveled tail */
			for (hidx = cem43_idx_in_horizon(drv, hidx - 1);
			     hidx != hidx_end && dump_h_minutes > 0;
			     hidx = cem43_idx_in_horizon(drv, hidx - 1)) {
				dump_h_minutes--;
				cem43_maybe_overwrite(drv, hidx, dump_sum_minute);
			}
		}
	}

success:
	mutex_unlock(&drv->lock);

	return count;
}

static ssize_t cem_horizon_minutes_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct cem43_drvdata *drv = dev_get_drvdata(dev);
	ssize_t ret;

	ret = mutex_lock_interruptible(&drv->lock);
	if (ret < 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__,
			 (int)ret);
		return ret;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", drv->cem_horizon_minutes);
	mutex_unlock(&drv->lock);

	return ret;
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
	drv->debug_skipped_seconds += seconds;

	mutex_unlock(&drv->lock);

	return count;
}
#endif /* CONFIG_THERMAL_EMULATION */

static DEVICE_ATTR_RW(cem_history);
static DEVICE_ATTR_RO(cem_horizon_minutes);
static DEVICE_ATTR_RO(cem_target_mdeg);
static DEVICE_ATTR_RO(projection_seconds);
static DEVICE_ATTR_RO(last_raw_temp);
static DEVICE_ATTR_RO(last_avg_temp);
static DEVICE_ATTR_RO(last_exposure);
#if IS_ENABLED(CONFIG_THERMAL_EMULATION)
static DEVICE_ATTR_WO(emul_skip_sec);
#endif

static struct attribute *cem43_attrs[] = {
	&dev_attr_cem_history.attr,
	&dev_attr_cem_horizon_minutes.attr,
	&dev_attr_cem_target_mdeg.attr,
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

	ret = of_property_read_u32(pdev->dev.of_node, "cem-horizon-minutes",
				   &drv->cem_horizon_minutes);
	if (ret < 0)
		drv->cem_horizon_minutes = 180;

	if (drv->cem_horizon_minutes <= 0) {
		dev_err(&pdev->dev, "cem-horizon-minutes must be positive");
		return -EINVAL;
	}

	/* Parse cem-target-mdeg, the CEM target temperature */
	ret = of_property_read_u32(pdev->dev.of_node, "cem-target-mdeg",
				   &drv->cem_target_mdeg);
	if (ret < 0) {
		/* the default is CEM43, 43 degrees */
		drv->cem_target_mdeg = 43000;
	}

	/* Parse projection-seconds with default of 0 */
	ret = of_property_read_u32(pdev->dev.of_node, "projection-seconds",
				   &drv->projection_seconds);
	if (ret < 0)
		drv->projection_seconds = 0;

	if (drv->projection_seconds < 0) {
		dev_err(&pdev->dev, "projection-seconds must be non-negative");
		return -EINVAL;
	}

	/*
	 * Limit the projection so cem43_compute_cem() cannot push out more than half
	 * the horizon, since exceeding the horizon would wrap the ring buffer and
	 * subtract slots more than once.
	 */
	if (drv->projection_seconds > drv->cem_horizon_minutes * 60 / 2) {
		dev_err(&pdev->dev,
			"projection-seconds must not exceed half the horizon (%d)",
			drv->cem_horizon_minutes * 60 / 2);
		return -EINVAL;
	}

	ret = cem43_parse_thermal_zones_dt(drv);
	if (ret == -EPROBE_DEFER) {
		return ret;
	} else if (ret < 0) {
		dev_err(&pdev->dev, "Failed to parse data for sensor: %d", ret);
		return ret;
	}

	/* Allocate horizon accumulator, zeroed */
	drv->accum_horizon = devm_kcalloc(&pdev->dev, drv->cem_horizon_minutes,
					  sizeof(*drv->accum_horizon),
					  GFP_KERNEL);
	if (!drv->accum_horizon)
		return -ENOMEM;

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
