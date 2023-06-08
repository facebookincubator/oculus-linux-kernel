/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VIRTUAL_SENSOR_UTILS_H_
#define _VIRTUAL_SENSOR_UTILS_H_

#include <linux/iio/consumer.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>

#define THERMAL_MAX_VIRT_SENSORS 10

/**
 * Data for an individual logical sensor, e.g. charging or discharging.
 */
struct virtual_sensor_common_data {
	void *parent;

	/* Matched in device tree */
	const char *name;

	struct thermal_zone_device *tzs[THERMAL_MAX_VIRT_SENSORS];
	struct iio_channel *iios[THERMAL_MAX_VIRT_SENSORS];

	/* Accumulate temperature samples as part of the formula */
	s64 tz_samples;
	s64 tz_accum_temperatures[THERMAL_MAX_VIRT_SENSORS];
	s64 iio_samples;
	s64 iio_accum_temperatures[THERMAL_MAX_VIRT_SENSORS];

	/* Store last temperatures as part of the formula */
	s64 tz_last_temperatures[THERMAL_MAX_VIRT_SENSORS];
	s64 iio_last_temperatures[THERMAL_MAX_VIRT_SENSORS];

	/* Rate limit temperature calculations */
	unsigned long tz_last_jiffies;
	unsigned long iio_last_jiffies;
	s64 tz_temperature;
	s64 iio_temperature;

	/* Scaling factor scales to millidegrees */
	int tz_scaling_factors[THERMAL_MAX_VIRT_SENSORS];
	int iio_scaling_factors[THERMAL_MAX_VIRT_SENSORS];

	/* scaled by COEFFICIENT_SCALAR */
	int tz_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int tz_slope_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int iio_coefficients[THERMAL_MAX_VIRT_SENSORS];
	int iio_slope_coefficients[THERMAL_MAX_VIRT_SENSORS];

	int tz_count;
	int iio_count;

	int intercept;

	/* virtual thermal sensor info to run workqueue */
	int internal_polling_delay;
	struct thermal_zone_device *tzd;
	struct delayed_work poll_queue;
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

	struct power_supply *batt_psy;	/* For determining charge status */
	void *data;						/* Driver-specific data */
};

int is_charging(struct power_supply *batt_psy);

int virtual_sensor_calculate_tz_temp(struct device *dev,
		struct virtual_sensor_common_data *data, s64 *temperature);

int virtual_sensor_calculate_iio_temp(struct device *dev,
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

ssize_t iio_coefficients_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t iio_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t iio_slope_coefficients_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t iio_slope_coefficients_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t tz_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t tz_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t tz_slope_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t tz_slope_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t iio_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t iio_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t iio_slope_coefficients_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t iio_slope_coefficients_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t intercept_charging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t intercept_charging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

ssize_t intercept_discharging_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t intercept_discharging_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

void virtual_sensor_workqueue_register(struct virtual_sensor_common_data *data);
void virtual_sensor_workqueue_unregister(struct virtual_sensor_common_data *data);
#endif /* _VIRTUAL_SENSOR_UTILS_H_ */
