// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * @file xrps_profiling.c
 * @brief Implementation of XRPS profiling interfaces
 *
 *****************************************************************************/

#include "xrps_profiling.h"
#include "xrps_osl.h"

#ifdef CONFIG_XRPS_PROFILING

XRPS_LOG_DECLARE();

#define MAX_NUM_PROFILING_EVENTS 64
static struct profiling_event profiling_events_buffer[MAX_NUM_PROFILING_EVENTS];

static xrps_osl_spinlock_t profiling_lock;

static struct xrps_osl_intf *osl;

void xrps_init_profiling(struct xrps_osl_intf *osl_intf,
			 struct profiling_event_array *events)
{
	if (osl == NULL)
		osl = osl_intf;

	events->buffer = profiling_events_buffer;
	events->cur_index = -1;
	osl->spin_lock_init(&profiling_lock);
}

void put_profiling_event(struct profiling_event_array *events,
			 enum profile_event_type type)
{
	xrps_osl_spinlock_flag_t flag = osl->spin_lock(&profiling_lock);
	int next = events->cur_index + 1;

	if (next < MAX_NUM_PROFILING_EVENTS) {
		struct profiling_event *e = &events->buffer[next];

		e->type = type;
		e->timestamp = osl->get_ktime();
		events->cur_index = next;
	}

	osl->spin_unlock(&profiling_lock, flag);
}

void dump_profiling_events(struct profiling_event_array *events)
{
	xrps_osl_spinlock_flag_t flag = osl->spin_lock(&profiling_lock);

	for (int i = 0; i <= events->cur_index; ++i) {
		struct profiling_event *e = &events->buffer[i];

		XRPS_LOG_ERR("XRPS event: %d ts: %llu\n", e->type,
			     osl->ktime_to_us(e->timestamp));
	}
	events->cur_index = -1;
	osl->spin_unlock(&profiling_lock, flag);
}

#endif /* CONFIG_XRPS_PROFILING */
