/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM walt
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_WALT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_WALT_H
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_walt_sched_lpm_disallowed_time,
	TP_PROTO(int cpu, u64* bias_time, int *ret),
	TP_ARGS(cpu, bias_time, ret));

#endif /* _TRACE_HOOK_WALT_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
