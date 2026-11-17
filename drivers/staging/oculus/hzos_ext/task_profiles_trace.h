/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Meta Platforms, Inc. and affiliates
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hzos_ext

#if !defined(_HZOS_EXT_TASK_PROFILES_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _HZOS_EXT_TASK_PROFILES_TRACE_H

#include <linux/hzos_ext_types.h>
#include <linux/tracepoint.h>
#include <linux/types.h>

TRACE_DEFINE_ENUM(HZOS_EXT_TAG_PROCESS);
TRACE_DEFINE_ENUM(HZOS_EXT_TAG_THREAD);

TRACE_EVENT(hzos_ext_task_profiles_changed,
	TP_PROTO(u8 kind, pid_t tgid, pid_t tid, u32 gen),
	TP_ARGS(kind, tgid, tid, gen),
	TP_STRUCT__entry(
		__field(u8, kind)
		__field(pid_t, tgid)
		__field(pid_t, tid)
		__field(u32, gen)
	),
	TP_fast_assign(
		__entry->kind = kind;
		__entry->tgid = tgid;
		__entry->tid = tid;
		__entry->gen = gen;
	),
	TP_printk("kind=%s tgid=%d tid=%d gen=%u",
		  __print_symbolic(__entry->kind,
				   { HZOS_EXT_TAG_PROCESS, "process" },
				   { HZOS_EXT_TAG_THREAD, "thread" }),
		  __entry->tgid, __entry->tid, __entry->gen)
);

#endif /* _HZOS_EXT_TASK_PROFILES_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE task_profiles_trace
#include <trace/define_trace.h>
