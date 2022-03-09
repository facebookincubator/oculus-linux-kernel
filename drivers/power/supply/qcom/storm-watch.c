/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "storm-watch.h"

/**
 * is_storming(): Check if an event is storming
 *
 * @data: Data for tracking an event storm
 *
 * The return value will be true if a storm has been detected and
 * false if a storm was not detected.
 */
bool is_storming(struct storm_watch *data)
{
	ktime_t curr_kt, delta_kt;
	bool is_storming = false;

	if (!data)
		return false;

	if (!data->enabled)
		return false;

	/* max storm count must be greater than 0 */
	if (data->max_storm_count <= 0)
		return false;

	/* the period threshold must be greater than 0ms */
	if (data->storm_period_ms <= 0)
		return false;

	mutex_lock(&data->storm_lock);
	curr_kt = ktime_get_boottime();
	delta_kt = ktime_sub(curr_kt, data->last_kt);

	if (ktime_to_ms(delta_kt) < data->storm_period_ms)
		data->storm_count++;
	else
		data->storm_count = 0;

	if (data->storm_count > data->max_storm_count) {
		is_storming = true;
		data->storm_count = 0;
	}

	data->last_kt = curr_kt;
	mutex_unlock(&data->storm_lock);
	return is_storming;
}

void reset_storm_count(struct storm_watch *data)
{
	mutex_lock(&data->storm_lock);
	data->storm_count = 0;
	mutex_unlock(&data->storm_lock);
}

void update_storm_count(struct storm_watch *data, int max_count)
{
	if (!data)
		return;

	mutex_lock(&data->storm_lock);
	data->max_storm_count = max_count;
	mutex_unlock(&data->storm_lock);
}
