/* Copyright (c) 2019, Facebook Technologies, LLC. All rights reserved.
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
#ifndef __VIDC_THREADSTATS_H
#define __VIDC_THREADSTATS_H

enum {
	VIDC_THREADSTATS_ETB_TIMESTAMP = 0,
	VIDC_THREADSTATS_ETB_ID,
	VIDC_THREADSTATS_ETB_COUNT,
	VIDC_THREADSTATS_EBD_TIMESTAMP,
	VIDC_THREADSTATS_EBD_ID,
	VIDC_THREADSTATS_EBD_COUNT,
	VIDC_THREADSTATS_FTB_TIMESTAMP,
	VIDC_THREADSTATS_FTB_ID,
	VIDC_THREADSTATS_FTB_COUNT,
	VIDC_THREADSTATS_FBD_TIMESTAMP,
	VIDC_THREADSTATS_FBD_ID,
	VIDC_THREADSTATS_FBD_COUNT,
	VIDC_THREADSTATS_MAX
};

enum {
	VIDC_THREADSTATS_ETB_EVENT = 0,
	VIDC_THREADSTATS_EBD_EVENT,
	VIDC_THREADSTATS_FTB_EVENT,
	VIDC_THREADSTATS_FBD_EVENT,
	VIDC_THREADSTATS_EVENT_MAX
};

struct vidc_thread_private;

int vidc_thread_private_get(struct vidc_thread_private *thread);
void vidc_thread_private_put(struct vidc_thread_private *thread);
struct vidc_thread_private *vidc_thread_private_find(pid_t tid);

void vidc_thread_private_close(struct vidc_thread_private *private);
struct vidc_thread_private *vidc_thread_private_open(void);

#endif /* __VIDC_THREADSTATS_H */
