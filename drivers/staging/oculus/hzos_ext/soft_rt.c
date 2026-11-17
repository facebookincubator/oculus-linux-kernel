// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Meta Platforms, Inc. and affiliates
 */

/*
 * SOFT_RT: soft real-time CPU priority for individual threads on cgroup v2.
 *
 * On cgroup v1, soft real-time threads joined the system-wide soft-rt cpu
 * cgroup and took precedence over other processes' threads. On a cgroup v2 cpu
 * controller a thread's CFS weight only competes inside its own process cgroup,
 * so no per-thread weight can do that. Instead, runnable threads flagged
 * SOFT_RT are kept on a per-CPU queue and picked ahead of the CFS choice,
 * across groups. Flagged threads are bounded so they cannot starve the rest of
 * the system:
 *
 * - slice: a flagged thread runs for at most one slice before the next flagged
 *   thread queued on the CPU gets a turn.
 * - limit: once a thread has run this long since it woke, it drops back to
 *   plain CFS until it sleeps again.
 * - rq window / rq share: the runtime of queued flagged threads is charged to
 *   the CPU in fixed windows. Once it exceeds the share of the current window,
 *   the CPU ignores its queue until the window ends. This holds however briefly
 *   the threads sleep between activations.
 *
 * All state is protected by the runqueue lock.
 */

#include <linux/list.h>
#include <linux/math64.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/time64.h>
#include <linux/version.h>

#include "features.h"
#include "flags.h"
#include "soft_rt.h"

static u64 srt_slice_ns = 3 * NSEC_PER_MSEC;
static u64 srt_limit_ns = 12 * NSEC_PER_MSEC;
static u64 srt_rq_window_ns = 100 * NSEC_PER_MSEC;
static u64 srt_rq_share_pct = 80;

static ssize_t srt_set(u64 *knob, s64 value)
{
	if (value <= 0)
		return -EINVAL;

	WRITE_ONCE(*knob, value);
	return 0;
}

ssize_t soft_rt_slice_write(s64 value)
{
	return srt_set(&srt_slice_ns, value);
}

s64 soft_rt_slice_get(void)
{
	return READ_ONCE(srt_slice_ns);
}

ssize_t soft_rt_limit_write(s64 value)
{
	return srt_set(&srt_limit_ns, value);
}

s64 soft_rt_limit_get(void)
{
	return READ_ONCE(srt_limit_ns);
}

ssize_t soft_rt_rq_window_write(s64 value)
{
	/* srt_charge() multiplies the window by the share percentage. */
	if (value > S64_MAX / 100)
		return -EINVAL;

	return srt_set(&srt_rq_window_ns, value);
}

s64 soft_rt_rq_window_get(void)
{
	return READ_ONCE(srt_rq_window_ns);
}

ssize_t soft_rt_rq_share_write(s64 value)
{
	if (value > 100)
		return -EINVAL;

	return srt_set(&srt_rq_share_pct, value);
}

s64 soft_rt_rq_share_get(void)
{
	return READ_ONCE(srt_rq_share_pct);
}

static void srt_clear_flag(struct task_struct *p)
{
	mutex_lock(&p->hzos_ext.lock);
	WRITE_ONCE(p->hzos_ext.flags,
		   p->hzos_ext.flags & ~HZOS_EXT_FLAG_SOFT_RT);
	mutex_unlock(&p->hzos_ext.lock);
}

#if defined(CONFIG_ANDROID_VENDOR_HOOKS) && defined(CONFIG_FAIR_GROUP_SCHED)

#include <trace/hooks/sched.h>

#include <kernel/sched/sched.h>

/* Exported by fair.c for vendor pick_next_task_fair() replacements. */
extern void set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static inline bool srt_task_on_cpu(struct rq *rq, struct task_struct *p)
{
	return task_on_cpu(rq, p);
}

static inline void srt_rq_lock(struct rq *rq)
{
	raw_spin_rq_lock(rq);
}

static inline void srt_rq_unlock(struct rq *rq)
{
	raw_spin_rq_unlock(rq);
}

/* 6.1 skips its own pick on the simple path and leaves the hierarchy to us. */
#define SRT_SETS_SIMPLE_HIERARCHY true
#else
static inline bool srt_task_on_cpu(struct rq *rq, struct task_struct *p)
{
	return task_running(rq, p);
}

static inline void srt_rq_lock(struct rq *rq)
{
	raw_spin_lock(&rq->lock);
}

static inline void srt_rq_unlock(struct rq *rq)
{
	raw_spin_unlock(&rq->lock);
}

/* 5.10 sets the hierarchy of the replacement itself on the simple path. */
#define SRT_SETS_SIMPLE_HIERARCHY false
#endif

/**
 * struct srt_rq - Per-CPU SOFT_RT state.
 *
 * @tasks Runnable flagged tasks. The head runs next.
 * @window_start rq clock when the current window began.
 * @window_runtime Runtime charged by queued flagged tasks in the window.
 * @throttled The CPU ignores @tasks until the window ends.
 */
struct srt_rq {
	struct list_head tasks;
	u64 window_start;
	u64 window_runtime;
	bool throttled;
};

static DEFINE_PER_CPU(struct srt_rq, srt_rqs);

static inline struct srt_rq *srt_rq_of(struct rq *rq)
{
	return &per_cpu(srt_rqs, cpu_of(rq));
}

/* Tasks that existed before hzos_ext initialised have a zeroed node. */
static inline bool srt_queued(struct task_struct *p)
{
	return p->hzos_ext.srt_node.next && !list_empty(&p->hzos_ext.srt_node);
}

static inline bool srt_flagged(struct task_struct *p)
{
	return hzos_ext_task_has_flag(p, HZOS_EXT_FLAG_SOFT_RT);
}

static bool srt_over_limit(struct task_struct *p)
{
	return p->se.sum_exec_runtime - p->hzos_ext.srt_wake_runtime >
	       READ_ONCE(srt_limit_ns);
}

static void srt_window_roll(struct srt_rq *srtq, u64 now)
{
	if (now - srtq->window_start < READ_ONCE(srt_rq_window_ns))
		return;

	srtq->window_start = now;
	srtq->window_runtime = 0;
	srtq->throttled = false;
}

/* Charge the runtime @p accrued since its last charge to the CPU's window. */
static void srt_charge(struct srt_rq *srtq, struct task_struct *p)
{
	u64 runtime = p->se.sum_exec_runtime;
	u64 window = READ_ONCE(srt_rq_window_ns);

	srtq->window_runtime += runtime - p->hzos_ext.srt_charge_runtime;
	p->hzos_ext.srt_charge_runtime = runtime;

	if (srtq->window_runtime > div_u64(window * READ_ONCE(srt_rq_share_pct), 100))
		srtq->throttled = true;
}

static void srt_remove(struct srt_rq *srtq, struct task_struct *p)
{
	srt_charge(srtq, p);
	list_del_init(&p->hzos_ext.srt_node);
}

/* A throttled cfs_rq dequeues its group entity, leaving @se unreachable. */
static bool srt_hierarchy_queued(struct sched_entity *se)
{
	for (; se; se = se->parent) {
		if (!se->on_rq)
			return false;
	}

	return true;
}

/*
 * The first queued task that can run now, skipping tasks in a throttled
 * cgroup and tasks that yielded. Drops tasks whose flag was cleared.
 */
static struct task_struct *srt_next(struct srt_rq *srtq)
{
	struct task_struct *p, *tmp;

	list_for_each_entry_safe(p, tmp, &srtq->tasks, hzos_ext.srt_node) {
		if (!srt_flagged(p)) {
			srt_remove(srtq, p);
			continue;
		}

		if (srt_hierarchy_queued(&p->se) &&
		    p->se.cfs_rq->skip != &p->se)
			return p;
	}

	return NULL;
}

static void srt_enqueue_handler(void *unused, struct rq *rq,
			       struct task_struct *p, int flags)
{
	struct hzos_ext *ext = &p->hzos_ext;

	if (!hzos_ext_feature_enabled(HZOS_EXT_FEATURE_SOFT_RT) ||
	    p->sched_class != &fair_sched_class || !srt_flagged(p) ||
	    srt_queued(p))
		return;

	if (!ext->srt_awake) {
		ext->srt_awake = true;
		ext->srt_wake_runtime = p->se.sum_exec_runtime;
		ext->srt_slice_runtime = p->se.sum_exec_runtime;
	}

	/* Enqueues while runnable (migration, priority change) keep the limit. */
	if (srt_over_limit(p))
		return;

	ext->srt_charge_runtime = p->se.sum_exec_runtime;

	/* A running task re-enqueued for a priority change keeps its turn. */
	if (srt_task_on_cpu(rq, p))
		list_add(&ext->srt_node, &srt_rq_of(rq)->tasks);
	else
		list_add_tail(&ext->srt_node, &srt_rq_of(rq)->tasks);
}

/*
 * Runs after the class dequeue, so the runtime charged on removal includes
 * the update_curr() of a task going to sleep. Not gated on the feature so that
 * disabling it leaves no task queued after it stops being runnable.
 */
static void srt_dequeued(struct rq *rq, struct task_struct *p)
{
	if (srt_queued(p))
		srt_remove(srt_rq_of(rq), p);
}

static void srt_slept(struct task_struct *p, int flags)
{
	if (flags & DEQUEUE_SLEEP)
		p->hzos_ext.srt_awake = false;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void srt_after_dequeue_handler(void *unused, struct rq *rq,
				     struct task_struct *p, int flags)
{
	srt_dequeued(rq, p);
	srt_slept(p, flags);
}

static int srt_register_dequeue(void)
{
	return register_trace_android_rvh_after_dequeue_task(srt_after_dequeue_handler,
							     NULL);
}
#else
/* 5.10's after_dequeue hook has no flags, so sleeps are seen at dequeue. */
static void srt_dequeue_handler(void *unused, struct rq *rq,
			       struct task_struct *p, int flags)
{
	srt_slept(p, flags);
}

static void srt_after_dequeue_handler(void *unused, struct rq *rq,
				     struct task_struct *p)
{
	srt_dequeued(rq, p);
}

static int srt_register_dequeue(void)
{
	return register_trace_android_rvh_dequeue_task(srt_dequeue_handler, NULL) ?:
	       register_trace_android_rvh_after_dequeue_task(srt_after_dequeue_handler,
							     NULL);
}
#endif

/*
 * Charge @curr's runtime, enforce its slice and limit, and roll the CPU's
 * window. Returns true if @curr should give up the CPU: the throttle state
 * changed, or another flagged task should run now.
 */
static bool srt_account(struct rq *rq, struct srt_rq *srtq,
		       struct task_struct *curr)
{
	struct hzos_ext *ext = &curr->hzos_ext;
	bool was_throttled = srtq->throttled;
	struct task_struct *next;

	srt_window_roll(srtq, rq->clock);

	if (srt_queued(curr)) {
		if (!srt_flagged(curr) || srt_over_limit(curr)) {
			srt_remove(srtq, curr);
		} else {
			srt_charge(srtq, curr);
			if (curr->se.sum_exec_runtime - ext->srt_slice_runtime >=
			    READ_ONCE(srt_slice_ns)) {
				ext->srt_slice_runtime = curr->se.sum_exec_runtime;
				list_move_tail(&ext->srt_node, &srtq->tasks);
			}
		}
	}

	if (was_throttled != srtq->throttled)
		return true;
	if (srtq->throttled)
		return false;

	next = srt_next(srtq);
	return next && next != curr;
}

static void srt_replace_next_handler(void *unused, struct rq *rq,
				    struct task_struct **p,
				    struct sched_entity **se, bool *repick,
				    bool simple, struct task_struct *prev)
{
	struct srt_rq *srtq = srt_rq_of(rq);
	struct sched_entity *next_se;
	struct task_struct *next;

	if (!hzos_ext_feature_enabled(HZOS_EXT_FEATURE_SOFT_RT))
		return;

	srt_window_roll(srtq, rq->clock);
	if (srtq->throttled)
		return;

	next = srt_next(srtq);
	if (!next)
		return;

	*p = next;
	*se = &next->se;
	*repick = true;

	/*
	 * On the non-simple path pick_next_task_fair() moves from prev to *se
	 * itself. On the simple path prev has already been put, and depending on
	 * the kernel the hierarchy is set either here or by the caller.
	 */
	if (simple && SRT_SETS_SIMPLE_HIERARCHY) {
		for (next_se = &next->se; next_se; next_se = next_se->parent)
			set_next_entity(next_se->cfs_rq, next_se);
	}
}

/*
 * Leaving both @preempt and @nopreempt false leaves the decision to CFS.
 */
static void srt_check_preempt_wakeup_handler(void *unused, struct rq *rq,
					    struct task_struct *p,
					    bool *preempt, bool *nopreempt,
					    int wake_flags,
					    struct sched_entity *se,
					    struct sched_entity *pse,
					    int next_buddy_marked,
					    unsigned int granularity)
{
	struct srt_rq *srtq = srt_rq_of(rq);
	struct task_struct *curr = rq->curr;

	if (!hzos_ext_feature_enabled(HZOS_EXT_FEATURE_SOFT_RT))
		return;

	srt_window_roll(srtq, rq->clock);

	if (srt_queued(curr)) {
		if (srt_account(rq, srtq, curr)) {
			*preempt = true;
			return;
		}
		if (srt_queued(curr) && !srtq->throttled) {
			*nopreempt = true;
			return;
		}
	}

	if (srt_queued(p) && !srtq->throttled)
		*preempt = true;
}

/* Runs after task_tick(), so curr's runtime is up to date. */
static void srt_tick_handler(void *unused, struct rq *rq)
{
	struct srt_rq *srtq = srt_rq_of(rq);

	if (!hzos_ext_feature_enabled(HZOS_EXT_FEATURE_SOFT_RT))
		return;

	/* Unlocked peek; a missed update is picked up on the next tick. */
	if (list_empty(&srtq->tasks) && !READ_ONCE(srtq->throttled))
		return;

	srt_rq_lock(rq);
	if (!(rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(rq);

	if (srt_account(rq, srtq, rq->curr) &&
	    rq->curr->sched_class == &fair_sched_class &&
	    rq->cfs.h_nr_running > 1)
		resched_curr(rq);
	srt_rq_unlock(rq);
}

void hzos_ext_soft_rt_post_clone(struct task_struct *p)
{
	INIT_LIST_HEAD(&p->hzos_ext.srt_node);
	p->hzos_ext.srt_awake = false;
	/* Like v2 thread tags, the flag does not pass to spawned threads. */
	srt_clear_flag(p);
}

void hzos_ext_soft_rt_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		INIT_LIST_HEAD(&per_cpu(srt_rqs, cpu).tasks);

	/*
	 * Restricted hooks cannot be unregistered. Register the consumer of the
	 * queues first and the hook that fills them last, so that a failure
	 * leaves either no queue or a queue that is still emptied on dequeue.
	 */
	if (register_trace_android_rvh_replace_next_task_fair(srt_replace_next_handler,
							      NULL) ||
	    srt_register_dequeue() ||
	    register_trace_android_rvh_enqueue_task(srt_enqueue_handler, NULL)) {
		pr_err("hzos_ext: SOFT_RT hooks unavailable");
		return;
	}

	/* Without these, flagged tasks are only favoured at the next pick. */
	if (register_trace_android_rvh_check_preempt_wakeup(srt_check_preempt_wakeup_handler,
							    NULL) ||
	    register_trace_android_vh_scheduler_tick(srt_tick_handler, NULL))
		pr_err("hzos_ext: SOFT_RT preemption hooks unavailable");
}

#else

void hzos_ext_soft_rt_post_clone(struct task_struct *p)
{
	srt_clear_flag(p);
}

void hzos_ext_soft_rt_init(void)
{
}

#endif /* CONFIG_ANDROID_VENDOR_HOOKS && CONFIG_FAIR_GROUP_SCHED */
