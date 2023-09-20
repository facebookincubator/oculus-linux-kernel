/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * @file xrps_profiling.h
 * @brief XRPS profiling interfaces and type definitions
 *
 * @details The client is expected to instantiate a buffer in which events will
 * be stored. Events contain user-specific data such as event type and
 * timestamp. The client can then dump the buffer which will also print its
 * contents to the console.
 *
 *****************************************************************************/

#ifndef XRPS_PROFILING_H
#define XRPS_PROFILING_H

#ifdef CONFIG_XRPS_PROFILING

#ifndef __linux__
// Redefinition in include/linux/types.h
#include <stdint.h>
#endif
#include "xrps_osl.h"

enum profile_event_type {
	UNHOLD_START = 0,
	UNHOLD_END = 1,
	FIRST_RX_CB = 2,
	EOT_SUBMIT = 3,
	EOT_TX = 4,
	EOT_RX = 5,
	MB_RING = 6,
};

struct profiling_event {
	enum profile_event_type type;
	uint64_t timestamp;
} __aligned(4);

struct profiling_event_array {
	struct profiling_event *buffer;
	int cur_index;
};

void xrps_init_profiling(struct xrps_osl_intf *osl_intf,
			 struct profiling_event_array *events);
void put_profiling_event(struct profiling_event_array *events,
			 enum profile_event_type type);
void dump_profiling_events(struct profiling_event_array *events);

#endif /* CONFIG_XRPS_PROFILING */

#endif // XRPS_PROFILING_H
