/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VIRTUAL_SENSOR_UTILS_H_
#define _VIRTUAL_SENSOR_UTILS_H_

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>

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

	bool was_charging;
	struct power_supply *batt_psy;
};

bool is_charging(struct power_supply *batt_psy);

int virtual_sensor_calculate_tz_temp(struct device *dev,
		struct virtual_sensor_common_data *data, s64 *temperature);

void virtual_sensor_reset_history(struct virtual_sensor_common_data *data);

int virtual_sensor_parse_dt(struct device *dev,
		struct virtual_sensor_common_data *data);

ssize_t tz_coefficients_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t tz_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t tz_slope_coefficients_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t tz_slope_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t tz_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t tz_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t tz_slope_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t tz_slope_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t intercept_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t intercept_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t intercept_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t intercept_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t fallback_tolerance_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t fallback_tolerance_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
#endif /* _VIRTUAL_SENSOR_UTILS_H_ */
