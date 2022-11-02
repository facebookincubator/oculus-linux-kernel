/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VIRTUAL_SENSOR_UTILS_H_
#define _VIRTUAL_SENSOR_UTILS_H_

#include <linux/iio/consumer.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>

struct virtual_sensor_common_data {
	struct mutex lock;

	void *parent;

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

	int intercept_constant_charging;
	int intercept_constant_discharging;
};

int is_charger_connected(struct power_supply *usb_psy);

int virtual_sensor_calculate_tz_temp(struct device *dev,
		struct virtual_sensor_common_data *data, s64 *temperature);

int virtual_sensor_calculate_iio_temp(struct device *dev,
		struct virtual_sensor_common_data *data, s64 *temperature);

void virtual_sensor_reset_history(struct virtual_sensor_common_data *data);

int virtual_sensor_parse_common_dt(struct device *dev,
		struct virtual_sensor_common_data *data);

ssize_t tz_coefficients_show(struct device *dev, struct device_attribute *attr,
		char *buf);
ssize_t tz_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR_RW(tz_coefficients);

ssize_t tz_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t tz_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR_RW(tz_slope_coefficients);

ssize_t iio_coefficients_show(struct device *dev, struct device_attribute *attr,
		char *buf);
ssize_t iio_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR_RW(iio_coefficients);

ssize_t iio_slope_coefficients_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t iio_slope_coefficients_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR_RW(iio_slope_coefficients);

ssize_t charging_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t charging_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR_RW(charging_constant);

ssize_t discharging_constant_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t discharging_constant_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR_RW(discharging_constant);

#endif /* _VIRTUAL_SENSOR_UTILS_H_ */
