/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_HZOS_EXT_TYPES_H
#define _LINUX_HZOS_EXT_TYPES_H
/*
 *  Custom Meta symbols and exports
 *
 *  Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/cpumask.h>
#include <linux/mutex.h>
#include <linux/types.h>

/**
 * struct hzos_ext_stats - Struct containing hzos_ext-related
 * statistics
 *
 * @nr_srq_prev	Waking task was kept on its most recent CPU.
 * @nr_srq_sync	Waking task was migrated to the waker's CPU.
 * @nr_srq_any	Waking task was migrated to a random idle CPU.
 */
struct hzos_ext_stats {
	u64 nr_srq_prev;
	u64 nr_srq_sync;
	u64 nr_srq_any;
};

/* Maximum length, including the NUL terminator, of a task profiles tag. */
#define HZOS_EXT_TASK_PROFILES_LEN 256

/* Which hzos_ext_tag slot of a task: the per-process or the per-thread tag. */
enum hzos_ext_tag_kind {
	HZOS_EXT_TAG_PROCESS,
	HZOS_EXT_TAG_THREAD,
};

/**
 * struct hzos_ext_tag - A string tag set from user space via procfs.
 *
 * @buf The tag, NUL terminated.
 * @gen Bumped on every change to @buf, and nonzero exactly when a tag has
 *	been applied. Pollers of the procfs file compare it against the
 *	generation they last observed, so the tag behaves as state rather than
 *	an event stream: a reader that misses intermediate changes still
 *	converges on the current value.
 */
struct hzos_ext_tag {
	char buf[HZOS_EXT_TASK_PROFILES_LEN];
	u32 gen;
};

/**
 * struct hzos_ext - Wrapper struct for hzos_ext object.
 *
 * @flags Flags
 * @preferred_mask The cpumask that the task prefers to run on.
 * @srt_node Entry in the per-CPU queue of runnable SOFT_RT tasks.
 * @srt_wake_runtime se.sum_exec_runtime when the task last woke.
 * @srt_slice_runtime se.sum_exec_runtime when the current slice began.
 * @srt_charge_runtime se.sum_exec_runtime when the task was last charged to
 *		      its CPU's window.
 * @srt_awake Set from the first enqueue after a sleep until the next sleep.
 * @task_profiles The task profiles init applied to the task.
 * @thread_profiles The profile tag applied to this thread.
 * @lock The mutex synchronization object.
 */
struct hzos_ext {
	u64 flags;

#ifdef CONFIG_FAIR_GROUP_SCHED
	/*
	 * Tasks may use hzos_ext to set a "preferred" cpumask that the CPU scheduler
	 * may use to inform how it chooses to schedule the task. For instance, an
	 * application may prefer to run on gold cores, whereas system services may
	 * prefer to run on silver cores.
	 *
	 * In CPU schedulers, a task is generally preferred to run on either:
	 *
	 * 1. The "domain" (L3 cache, NUMA node, etc) it's currently *scheduled* in
	 * 2. The "domain" (L3 cache, NUMA node, etc) it's currently *assigned* to.
	 *
	 * In the default fair.c scheduler, the above two are the same. A task
	 * becomes "assigned" to a domain as soon as its load balanced.
	 * However, "scheduled in" and "assigned to" need not mean the same
	 * thing. Consider for instance that a task may want to *temporarily*
	 * run in another domain if that target domain has no work to do if it
	 * would otherwise suffer runqueue latency. Perhaps an application
	 * thread would like to run on a silver core rather than have to wait
	 * for a gold core. However, once it's run on the silver core and the
	 * traffic dies down on the gold core, it wants to go back to the gold
	 * core to take advantage of higher frequencies.
	 *
	 * This is what the preferred mask accomplishes. The hzos_ext
	 * portion of this is (thus far) very simple -- hzos_ext simply
	 * tracks the preferred mask, and exposes it to the main CPU scheduler to
	 * decide how it should be applied.
	 */
	cpumask_t preferred_mask;

	/*
	 * SOFT_RT scheduling state, protected by the lock of the
	 * runqueue the task is queued on. See hzos_ext/soft_rt.c.
	 */
	struct list_head srt_node;
	u64 srt_wake_runtime;
	u64 srt_slice_runtime;
	u64 srt_charge_runtime;
	bool srt_awake;
#endif

#ifdef CONFIG_HZOS_EXT_STATS
	struct hzos_ext_stats stats;
#endif

	/*
	 * The task profiles that init applied to the task, in the same space
	 * separated form as the task_profiles option of the service's init.rc
	 * entry. Purely informational: it lets user space attribute a process
	 * to its profiles without having to ask init, and it is never used to
	 * make scheduling or cgroup decisions in the kernel.
	 */
	struct hzos_ext_tag task_profiles;

	/*
	 * The profile tag applied to this individual thread. Purely
	 * informational, like task_profiles.
	 */
	struct hzos_ext_tag thread_profiles;

	struct mutex lock;
};

#endif /* _LINUX_HZOS_EXT_TYPES_H */
