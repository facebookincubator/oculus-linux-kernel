/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _DRM_TRACE_ATOMIC_H_
#define _DRM_TRACE_ATOMIC_H_

#if defined(CONFIG_DRM_ATRACE)
#include <linux/types.h>
#include <linux/sched.h>

/*
 * Use trace_puts for atrace-style events that Perfetto UI parses automatically.
 * trace_puts writes to the "print" ftrace event which Perfetto's ParsePrint() handles.
 */
static inline void drm_atrace_begin(const char *name)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "B|%d|%s", current->tgid, name);
	trace_puts(buf);
}

static inline void drm_atrace_end(const char *name)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "E|%d|%s", current->tgid, name);
	trace_puts(buf);
}

#define DRM_ATRACE_BEGIN(name) drm_atrace_begin(name)
#define DRM_ATRACE_END(name) drm_atrace_end(name)
#define DRM_ATRACE_FUNC_BEGIN() drm_atrace_begin(__func__)
#define DRM_ATRACE_FUNC_END() drm_atrace_end(__func__)
#else
#define DRM_ATRACE_BEGIN(name)
#define DRM_ATRACE_END(name)
#define DRM_ATRACE_FUNC_BEGIN()
#define DRM_ATRACE_FUNC_END()
#endif

#endif /* _DRM_TRACE_ATOMIC_H_ */
