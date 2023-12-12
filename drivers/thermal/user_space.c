/*
 *  user_space.c - A simple user space Thermal events notifier
 *
 *  Copyright (C) 2012 Intel Corp
 *  Copyright (C) 2012 Durgadoss R <durgadoss.r@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/thermal.h>
#include <linux/slab.h>

#include "thermal_core.h"

struct user_space_params {
	int last_trip;
};

static int bind_user_space(struct thermal_zone_device *tz) {
	struct user_space_params *params;

	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	params->last_trip = -1;

	tz->governor_data = params;

	return 0;
}

static void unbind_user_space(struct thermal_zone_device *tz) {
	kfree(tz->governor_data);
	tz->governor_data = NULL;
}

/**
 * notify_user_space - Notifies user space about thermal events
 * @tz - thermal_zone_device
 * @trip - trip point index
 *
 * This function notifies the user space through UEvents.
 */
static int notify_user_space(struct thermal_zone_device *tz, int trip)
{
	struct user_space_params *params = tz->governor_data;
	char *thermal_prop[5];
	int i, trip_temp, trip_hyst = 0;
	bool was_tripped = false;

	mutex_lock(&tz->lock);

	tz->ops->get_trip_temp(tz, trip, &trip_temp);
	if (tz->ops->get_trip_hyst)
		tz->ops->get_trip_hyst(tz, trip, &trip_hyst);

	/*
	 * Trip crossed going up
	 */
	if (tz->temperature >= trip_temp &&
	    params->last_trip < trip) {
		params->last_trip = trip;
		was_tripped = true;
	}

	/*
	 * Trip crossed going down
	 */
	if (tz->temperature < (trip_temp - trip_hyst) &&
	    params->last_trip >= trip) {
		params->last_trip = trip - 1;
		was_tripped = true;
	}

	if (!was_tripped)
		goto notify_out;

	thermal_prop[0] = kasprintf(GFP_KERNEL, "NAME=%s", tz->type);
	thermal_prop[1] = kasprintf(GFP_KERNEL, "TEMP=%d", tz->temperature);
	thermal_prop[2] = kasprintf(GFP_KERNEL, "TRIP=%d", trip);
	thermal_prop[3] = kasprintf(GFP_KERNEL, "EVENT=%d", tz->notify_event);
	thermal_prop[4] = NULL;
	kobject_uevent_env(&tz->device.kobj, KOBJ_CHANGE, thermal_prop);
	for (i = 0; i < 4; ++i)
		kfree(thermal_prop[i]);
notify_out:
	mutex_unlock(&tz->lock);
	return 0;
}

static struct thermal_governor thermal_gov_user_space = {
	.name		= "user_space",
	.bind_to_tz	= bind_user_space,
	.unbind_from_tz	= unbind_user_space,
	.throttle	= notify_user_space,
};

int thermal_gov_user_space_register(void)
{
	return thermal_register_governor(&thermal_gov_user_space);
}

void thermal_gov_user_space_unregister(void)
{
	thermal_unregister_governor(&thermal_gov_user_space);
}

