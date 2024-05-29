/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _LINUX_SCHED_WALT_H
#define _LINUX_SCHED_WALT_H

#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/cpumask.h>

#if IS_ENABLED(CONFIG_SCHED_WALT)

#define MAX_CPUS_PER_CLUSTER 6
#define MAX_CLUSTERS 3

struct core_ctl_notif_data {
	unsigned int nr_big;
	unsigned int coloc_load_pct;
	unsigned int ta_util_pct[MAX_CLUSTERS];
	unsigned int cur_cap_pct[MAX_CLUSTERS];
};

enum task_boost_type {
	TASK_BOOST_NONE = 0,
	TASK_BOOST_ON_MID,
	TASK_BOOST_ON_MAX,
	TASK_BOOST_STRICT_MAX,
	TASK_BOOST_END,
};

#define WALT_NR_CPUS 8
#define RAVG_HIST_SIZE_MAX 5
#define NUM_BUSY_BUCKETS 10

struct walt_related_thread_group {
	int			id;
	raw_spinlock_t		lock;
	struct list_head	tasks;
	struct list_head	list;
	bool			skip_min;
	struct rcu_head		rcu;
	u64			last_update;
	u64			downmigrate_ts;
	u64			start_ktime_ts;
};

struct walt_task_struct {
	/*
	 * 'mark_start' marks the beginning of an event (task waking up, task
	 * starting to execute, task being preempted) within a window
	 *
	 * 'sum' represents how runnable a task has been within current
	 * window. It incorporates both running time and wait time and is
	 * frequency scaled.
	 *
	 * 'sum_history' keeps track of history of 'sum' seen over previous
	 * RAVG_HIST_SIZE windows. Windows where task was entirely sleeping are
	 * ignored.
	 *
	 * 'demand' represents maximum sum seen over previous
	 * sysctl_sched_ravg_hist_size windows. 'demand' could drive frequency
	 * demand for tasks.
	 *
	 * 'curr_window_cpu' represents task's contribution to cpu busy time on
	 * various CPUs in the current window
	 *
	 * 'prev_window_cpu' represents task's contribution to cpu busy time on
	 * various CPUs in the previous window
	 *
	 * 'curr_window' represents the sum of all entries in curr_window_cpu
	 *
	 * 'prev_window' represents the sum of all entries in prev_window_cpu
	 *
	 * 'pred_demand' represents task's current predicted cpu busy time
	 *
	 * 'busy_buckets' groups historical busy time into different buckets
	 * used for prediction
	 *
	 * 'demand_scaled' represents task's demand scaled to 1024
	 *
	 * 'prev_on_rq' tracks enqueue/dequeue of a task for error conditions
	 * 0 = nothing, 1 = enqueued, 2 = dequeued
	 */
	u64				mark_start;
	u32				sum, demand;
	u32				coloc_demand;
	u32				sum_history[RAVG_HIST_SIZE_MAX];
	u32				curr_window_cpu[WALT_NR_CPUS];
	u32				prev_window_cpu[WALT_NR_CPUS];
	u32				curr_window, prev_window;
	u32				pred_demand;
	u8				busy_buckets[NUM_BUSY_BUCKETS];
	u16				demand_scaled;
	u16				pred_demand_scaled;
	u64				active_time;
	u64				last_win_size;
	int				boost;
	bool				wake_up_idle;
	bool				misfit;
	bool				rtg_high_prio;
	u8				low_latency;
	u64				boost_period;
	u64				boost_expires;
	u64				last_sleep_ts;
	u32				init_load_pct;
	u32				unfilter;
	u64				last_wake_ts;
	u64				last_enqueued_ts;
	struct walt_related_thread_group __rcu	*grp;
	struct list_head		grp_list;
	u64				cpu_cycles;
	cpumask_t			cpus_requested;
	bool				iowaited;
	int				prev_on_rq;
	int				prev_on_rq_cpu;
	struct list_head		mvp_list;
	u64				sum_exec_snapshot;
	u64				total_exec;
	int				mvp_prio;
	u8				hung_detect_status;
};

#define wts_to_ts(wts) ({ \
		void *__mptr = (void *)(wts); \
		((struct task_struct *)(__mptr - \
			offsetof(struct task_struct, android_vendor_data1))); })

static inline bool sched_get_wake_up_idle(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	return wts->wake_up_idle;
}

static inline int sched_set_wake_up_idle(struct task_struct *p, bool wake_up_idle)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wts->wake_up_idle = wake_up_idle;
	return 0;
}

static inline void set_wake_up_idle(bool wake_up_idle)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) current->android_vendor_data1;

	wts->wake_up_idle = wake_up_idle;
}

extern int sched_lpm_disallowed_time(int cpu, u64 *timeout);
extern int set_task_boost(int boost, u64 period);

struct notifier_block;
extern void core_ctl_notifier_register(struct notifier_block *n);
extern void core_ctl_notifier_unregister(struct notifier_block *n);
extern int core_ctl_set_boost(bool boost);
extern int walt_pause_cpus(struct cpumask *cpus);
extern int walt_resume_cpus(struct cpumask *cpus);
#else
static inline int sched_lpm_disallowed_time(int cpu, u64 *timeout)
{
	return INT_MAX;
}
static inline int set_task_boost(int boost, u64 period)
{
	return 0;
}

static inline bool sched_get_wake_up_idle(struct task_struct *p)
{
	return false;
}

static inline int sched_set_wake_up_idle(struct task_struct *p, bool wake_up_idle)
{
	return 0;
}

static inline void set_wake_up_idle(bool wake_up_idle)
{
}

static inline int core_ctl_set_boost(bool boost)
{
	return 0;
}

static inline void core_ctl_notifier_register(struct notifier_block *n)
{
}

static inline void core_ctl_notifier_unregister(struct notifier_block *n)
{
}

inline int walt_pause_cpus(struct cpumask *cpus)
{
	return 0;
}
inline int walt_resume_cpus(struct cpumask *cpus)
{
	return 0;
}
#endif

#endif /* _LINUX_SCHED_WALT_H */
