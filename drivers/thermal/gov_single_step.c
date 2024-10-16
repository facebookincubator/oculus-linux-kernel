/*
 *  single_step.c - A simple single step thermal throttling governor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 */

#include <linux/thermal.h>

#include "thermal_core.h"

static void thermal_zone_trip_update(struct thermal_zone_device *tz, int trip)
{
	int trip_temp, trip_hyst;
	struct thermal_instance *instance;

	tz->ops->get_trip_temp(tz, trip, &trip_temp);

	if (!tz->ops->get_trip_hyst) {
		pr_warn_once("Undefined get_trip_hyst for thermal zone %s - "
				"running with default hysteresis zero\n",
				tz->type);
		trip_hyst = 0;
	} else
		tz->ops->get_trip_hyst(tz, trip, &trip_hyst);

	dev_dbg(&tz->device, "Trip%d[temp=%d]:temp=%d:hyst=%d\n",
				trip, trip_temp, tz->temperature,
				trip_hyst);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip)
			continue;

		/* in case device is in initial state, set lower limit */
		if (instance->target == THERMAL_NO_TARGET)
			instance->target = instance->lower;

		dev_dbg(&tz->device, "%s: upper: %d, lower: %d, current target: %d\n",
			instance->name, instance->upper, instance->lower,
			instance->target);

		/*
		 * in case device is neither lower or upper limit assume
		 * control is handled by another step so ignore it
		 */
		if (instance->target != instance->lower &&
				instance->target != instance->upper) {
			dev_dbg(&instance->cdev->device,
				"Thermal instance %s controlled by single_step has unexpected state: %ld\n",
				instance->name, instance->target);
			goto skip_instance;
		}

		/*
		 * set upper limit when temperature exceeds trip_temp and set
		 * lower limit in case it falls below trip_temp minus hysteresis
		 */
		if (instance->target == instance->lower &&
				tz->temperature >= trip_temp)
			instance->target = instance->upper;
		else if (instance->target == instance->upper &&
				tz->temperature <= trip_temp - trip_hyst)
			instance->target = instance->lower;

		dev_dbg(&instance->cdev->device, "target=%d\n",
					(int)instance->target);

skip_instance:
		mutex_lock(&instance->cdev->lock);
		instance->cdev->updated = false; /* cdev needs update */
		mutex_unlock(&instance->cdev->lock);
	}

	mutex_unlock(&tz->lock);
}

/**
 * single_step_control - controls devices associated with the given zone
 * @tz - thermal_zone_device
 * @trip - the trip point
 *
 * Regulation Logic: a two point regulation, deliver cooling state depending
 * on the previous state shown in this diagram:
 *
 *          cooling_dev:  LOW   HIGH
 *
 *                              |
 *                              |
 *          trip_temp:    +---->+
 *                        |     |        ^
 *                        |     |        |
 *                        |     |   Temperature
 * (trip_temp - hyst):    +<----+
 *                        |
 *                        |
 *                        |
 *
 *   * If below trip_temp, cooling device is set to low. If trip_temp is
 *     exceeded, cooling device is set to high.
 *   * In case the trip_temp is set to high, temperature must fall below
 *     (trip_temp - hyst) so that the cooling device goes to low again.
 *
 */
static int single_step_control(struct thermal_zone_device *tz, int trip)
{
	struct thermal_instance *instance;

	thermal_zone_trip_update(tz, trip);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node)
		thermal_cdev_update(instance->cdev);

	mutex_unlock(&tz->lock);

	return 0;
}

static struct thermal_governor thermal_gov_single_step = {
	.name		= "single_step",
	.throttle	= single_step_control,
};
THERMAL_GOVERNOR_DECLARE(thermal_gov_single_step);
