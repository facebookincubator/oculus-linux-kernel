/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _VIRTUAL_SENSOR_UTILS_H_
#define _VIRTUAL_SENSOR_UTILS_H_

#include <linux/iio/consumer.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>

extern int is_charger_connected(struct power_supply *usb_psy);

extern int virtual_sensor_calculate_tz_temp(struct device *dev,
		struct thermal_zone_device **zones, int *zone_coeffs,
		int *zone_slope_coeffs, int *zone_scaling_factors,
		int zone_count, int *last_temps, int *temperature);

extern int virtual_sensor_calculate_iio_temp(struct device *dev,
		struct iio_channel **chans, int *chan_coeffs,
		int *chan_slope_coeffs, int *chan_scaling_factors,
		int chan_count, int *last_temps, int *temperature);

extern int virtual_sensor_parse_thermal_zones_dt(struct device *dev,
		struct thermal_zone_device **zones, int *zone_count);

extern int virtual_sensor_parse_iio_channels_dt(struct device *dev,
		struct iio_channel **chans, int *chan_count);

#endif /* _VIRTUAL_SENSOR_UTILS_H_ */
