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

#include <linux/ktime.h>
#include "msm_vidc_internal.h"
#include "venus_hfi.h"
#include "vidc_profile.h"

static u64 encoder_active_time;
static u32 encoder_frames;
static u64 decoder_active_time;
static u32 decoder_frames;

int vidc_profile_start(struct hal_session *s, u64 timestamp)
{
	struct vidc_buffer_entry *entry;

	entry = kmalloc(sizeof(struct vidc_buffer_entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->timestamp = timestamp;
	entry->ktime = ktime_to_ns(ktime_get());

	list_add_tail(&entry->list, &s->profile_head);

	return 0;
}

int vidc_profile_end(struct hal_session *s, u64 timestamp, bool is_decoder)
{
	struct vidc_buffer_entry *buf_entry, *next;

	list_for_each_entry_safe(buf_entry, next, &s->profile_head, list) {
		if (buf_entry->timestamp == timestamp) {
			u64 active_time =
				ktime_to_ns(ktime_get()) - buf_entry->ktime;

			list_del(&buf_entry->list);
			kfree(buf_entry);

			if (is_decoder) {
				decoder_active_time += active_time;
				decoder_frames++;
			} else {
				encoder_active_time += active_time;
				encoder_frames++;
			}

			return 0;
		}
	}

	return -EINVAL;
}

void vidc_profile_init(void)
{
	encoder_active_time = 0;
	encoder_frames = 0;
	decoder_active_time = 0;
	decoder_frames = 0;
}

ssize_t vidc_profile_decoder_active_time_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%llu,%u\n",
		decoder_active_time, decoder_frames);
}

ssize_t vidc_profile_encoder_active_time_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%llu,%u\n",
		encoder_active_time, encoder_frames);
}
