/* SPDX-License-Identifier: GPL-2.0-only */
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
#ifndef __KGSL_THREADSTATS_H
#define __KGSL_THREADSTATS_H

enum {
	KGSL_THREADSTATS_SUBMITTED = 0,
	KGSL_THREADSTATS_SUBMITTED_ID,
	KGSL_THREADSTATS_SUBMITTED_COUNT,
	KGSL_THREADSTATS_RETIRED,
	KGSL_THREADSTATS_RETIRED_ID,
	KGSL_THREADSTATS_RETIRED_COUNT,
	KGSL_THREADSTATS_QUEUED,
	KGSL_THREADSTATS_QUEUED_ID,
	KGSL_THREADSTATS_QUEUED_COUNT,
	KGSL_THREADSTATS_ACTIVE_TIME,
	KGSL_THREADSTATS_MAX
};

enum {
	KGSL_THREADSTATS_SUBMITTED_EVENT = 0,
	KGSL_THREADSTATS_RETIRED_EVENT,
	KGSL_THREADSTATS_QUEUED_EVENT,
	KGSL_THREADSTATS_ACTIVE_TIME_EVENT,
	KGSL_THREADSTATS_EVENT_MAX
};

struct kgsl_device;
struct kgsl_thread_private;

int kgsl_thread_private_get(struct kgsl_thread_private *thread);
void kgsl_thread_private_put(struct kgsl_thread_private *thread);
struct kgsl_thread_private *kgsl_thread_private_find(pid_t tid);

void kgsl_thread_private_close(struct kgsl_thread_private *private);
struct kgsl_thread_private *
kgsl_thread_private_open(struct kgsl_device *device);

#endif /* __KGSL_THREADSTATS_H */
