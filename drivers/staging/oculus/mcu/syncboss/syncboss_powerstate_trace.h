/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM syncboss

#if !defined(_SYNCBOSS_POWERSTATE_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _SYNCBOSS_POWERSTATE_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(syncboss_powerstate_rx,
	TP_PROTO(unsigned long type, int evt),
	TP_ARGS(type, evt),
	TP_STRUCT__entry(
		__field(unsigned long, type)
		__field(int, evt)
	),
	TP_fast_assign(
		__entry->type = type;
		__entry->evt = evt;
	),
	TP_printk("type=%lu evt=%d", __entry->type, __entry->evt)
);

TRACE_EVENT(syncboss_powerstate_signal,
	TP_PROTO(int evt),
	TP_ARGS(evt),
	TP_STRUCT__entry(
		__field(int, evt)
	),
	TP_fast_assign(
		__entry->evt = evt;
	),
	TP_printk("evt=%d", __entry->evt)
);

#endif /* _SYNCBOSS_POWERSTATE_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE syncboss_powerstate_trace
#include <trace/define_trace.h>
