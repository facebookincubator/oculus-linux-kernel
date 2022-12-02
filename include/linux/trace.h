/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TRACE_H
#define _LINUX_TRACE_H

#ifdef CONFIG_TRACING
/*
 * The trace export - an export of Ftrace output. The trace_export
 * can process traces and export them to a registered destination as
 * an addition to the current only output of Ftrace - i.e. ring buffer.
 *
 * If you want traces to be sent to some other place rather than ring
 * buffer only, just need to register a new trace_export and implement
 * its own .write() function for writing traces to the storage.
 *
 * next		- pointer to the next trace_export
 * write	- copy traces which have been delt with ->commit() to
 *		  the destination
 */
struct trace_export {
	struct trace_export __rcu	*next;
	void (*write)(struct trace_export *, const void *, unsigned int);
};

int register_ftrace_export(struct trace_export *export);
int unregister_ftrace_export(struct trace_export *export);

#endif	/* CONFIG_TRACING */

/* Functions mimicking the behavior of Android's "atrace". */
#ifdef CONFIG_KERNEL_ATRACE
struct task_struct;
extern bool __atrace_is_enabled(void);
extern int __atrace_begin(const struct task_struct *task, const char *name);
extern int __atrace_end(const struct task_struct *task);
extern int __atrace_int(const struct task_struct *task, const char *name,
		int value);
#else
#define __atrace_is_enabled() (false)
#define __atrace_begin(task, name) (0)
#define __atrace_end(task) (0)
#define __atrace_int(task, name, value) (0)
#endif  /* !CONFIG_KERNEL_ATRACE */

#define ATRACE_BEGIN(name) __atrace_begin(current, name)
#define ATRACE_END() __atrace_end(current)
#define ATRACE_INT(name, value) __atrace_int(current, name, value)

#endif	/* _LINUX_TRACE_H */
