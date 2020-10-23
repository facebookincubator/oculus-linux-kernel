/* Copyright (c) 2018, Facebook Technologies, LLC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __VIDC_PROFILING__
#define __VIDC_PROFILING__

#include <linux/types.h>
#include <linux/list.h>
#include "vidc_hfi.h"

struct hal_session;

struct vidc_buffer_entry {
	struct list_head list;
	u64 ktime;
	u64 timestamp;
};

int vidc_profile_start(struct hal_session *s, u64 timestamp);
int vidc_profile_end(struct hal_session *s, u64 timestamp, bool is_decoder);

void vidc_profile_init(void);

ssize_t vidc_profile_decoder_active_time_show(
		struct device *dev, struct device_attribute *attr, char *buf);
ssize_t vidc_profile_encoder_active_time_show(
		struct device *dev, struct device_attribute *attr, char *buf);

#endif
