// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta Orchestrator Agent scheduler subsystem
 *
 * Copyright (c) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/cpumask.h>
#include <linux/errno.h>
#include <linux/lockdep.h>
#include <linux/percpu-defs.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <trace/hooks/cgroup.h>
#include <trace/hooks/cpuidle.h>
#include <trace/hooks/sched.h>

#include <uapi/linux/sched/types.h>

#include "features.h"
#include "flags.h"
#include "stats.h"

#ifdef CONFIG_FAIR_GROUP_SCHED
static cpumask_var_t idle_mask;

/*
 * wake flags copied from kernel/sched/sched.h
 */
#define WF_SYNC                 0x01            /* Waker goes to sleep after wakeup */
#define WF_FORK                 0x02            /* Child wakeup after fork */
#define WF_MIGRATED             0x04            /* Internal use, task got migrated */
#define WF_ON_CPU               0x08            /* Wakee is on_cpu */
#define WF_ANDROID_VENDOR       0x1000          /* Vendor specific for Android */

struct pcpu_ctx {
	/* The number of tasks enqueued on this CPU */
	u64 nr_enqueued;
};

static DEFINE_PER_CPU(struct pcpu_ctx, pcpu_ctxs);

static struct pcpu_ctx *get_curr_cpu_ctx(void)
{
	return this_cpu_ptr(&pcpu_ctxs);
}

static void task_update_allowed(struct task_struct *p,
				const struct cpumask *allowed)
{
	const struct cpumask *preferred;

	lockdep_assert_held(&p->pi_lock);

	/*
	 * A task's preferred mask is derived from the task's cgroup's
	 * preferred mask AND'ed with the task's cpuset affinity. This is done
	 * to avoid having to do cpumask ops on the scheduling path.
	 */
	preferred = task_group_preferred_mask(p);
	if (likely(preferred))
		cpumask_and(&p->orchestrator.preferred_mask, preferred, allowed);
}

static void set_allowed_mask_handler(void *unused, struct task_struct *p,
				     const struct cpumask *new_mask)
{
	task_update_allowed(p, new_mask);
}

static void set_preferred_mask_handler(void *unused, struct task_struct *p,
				       const struct cpumask *new_preferred)
{
	task_update_allowed(p, p->cpus_ptr);
}

static void record_idle_enter_handler(void *unused, int *state,
				      struct cpuidle_device *dev)
{
	cpumask_set_cpu(dev->cpu, idle_mask);
}

static void record_idle_exit_handler(void *unused, int state,
				     struct cpuidle_device *dev)
{
	cpumask_clear_cpu(dev->cpu, idle_mask);
}

static bool try_reserve_store_idle_cpu(int cpu, int *target_cpu)
{
	if (cpumask_test_and_clear_cpu(cpu, idle_mask)) {
		*target_cpu = cpu;
		return true;
	}

	return false;
}

static bool task_prefers_cpu(struct task_struct *p, int cpu)
{
	return cpumask_test_cpu(cpu, &p->orchestrator.preferred_mask);
}

static bool try_reserve_idle_if_preferred(struct task_struct *p, int cpu,
					  int *target_cpu)
{
	if (!task_prefers_cpu(p, cpu))
		return false;

	return try_reserve_store_idle_cpu(cpu, target_cpu);
}

static void select_task_rq_handler(void *unused, struct task_struct *p,
				   int prev_cpu, int sd_flag, int wake_flags,
				   int *target_cpu)
{
	int cpu, curr_cpu;
	struct pcpu_ctx *ctx;
	bool sync;

	if (!orchestrator_feature_enabled(ORCHESTRATOR_FEATURE_SELECT_RQ_IDLE))
		return;

	/*
	 * First try to wake up on the prev cpu for:
	 * - Locality: we assume that keeping the task on the same CPU it
	 *   blocked on will be correlated with locality.
	 * - Avoiding migrations: Though the task is not yet enqueued at this
	 *   point, keeping it on the same CPU avoids unnecessary logic that
	 *   migrates it to another rq.
	 */
	if (try_reserve_idle_if_preferred(p, prev_cpu, target_cpu)) {
		orchestrator_stat_event_inc(p, NR_SRQ_PREV);
		return;
	}

	if (p->nr_cpus_allowed == 1)
		return;

	sync = !!(wake_flags & WF_SYNC) && !(current->flags & PF_EXITING);
	ctx = get_curr_cpu_ctx();
	if (sync && ctx->nr_enqueued == 0) {
		curr_cpu = smp_processor_id();
		if (task_prefers_cpu(p, curr_cpu)) {
			*target_cpu = smp_processor_id();
			orchestrator_stat_event_inc(p, NR_SRQ_SYNC);
			return;
		}
	}

	while (true) {
		cpu = cpumask_any_and_distribute(&p->orchestrator.preferred_mask,
						 idle_mask);

		if (cpu >= nr_cpu_ids)
			return;

		if (try_reserve_store_idle_cpu(cpu, target_cpu)) {
			orchestrator_stat_event_inc(p, NR_SRQ_ANY);
			return;
		}
	}
}

static void enqueue_entity_handler(void *unused, struct cfs_rq *cfs_rq,
				   struct sched_entity *se)
{
	struct pcpu_ctx *ctx = get_curr_cpu_ctx();

	ctx->nr_enqueued++;
}

static void dequeue_entity_handler(void *unused, struct cfs_rq *cfs_rq,
				   struct sched_entity *se)
{
	struct pcpu_ctx *ctx = get_curr_cpu_ctx();

	ctx->nr_enqueued--;
}
#endif

int orchestrator_task_setscheduler(struct task_struct *task,
			           const struct sched_attr *attr)
{
	int policy = attr->sched_policy;

	if (!orchestrator_feature_enabled(ORCHESTRATOR_FEATURE_ALLOW_RT))
		return 0;

	if ((policy == SCHED_FIFO || policy == SCHED_RR) &&
	    !orchestrator_task_has_flag(task, ORCHESTRATOR_FLAG_ALLOW_RT)) {

		pr_info("%s[%d] ALLOW_RT permission denied", task->comm, task->pid);
		return -EPERM;
	}

	return 0;
}

void orchestrator_sched_post_clone(struct task_struct *new)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	unsigned long flags;

	/*
	 * task_update_allowed() can be called with interrupts disabled when
	 * updating its allowed cpus_ptr. That should never happen here due to
	 * the fact that we're still on the clone path and the task is not yet
	 * runnable, but just to future proof let's do the safe thing and also
	 * disable interrupts here as this is a cold path and a small critical
	 * path.
	 */
	raw_spin_lock_irqsave(&new->pi_lock, flags);
	task_update_allowed(new, new->cpus_ptr);
	raw_spin_unlock_irqrestore(&new->pi_lock, flags);
#endif
}

void orchestrator_sched_init(void)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	BUG_ON(!alloc_cpumask_var(&idle_mask, GFP_KERNEL));

	register_trace_android_rvh_set_cpus_allowed_comm(set_allowed_mask_handler, NULL);
	register_trace_android_vh_sched_tg_setpreferred(set_preferred_mask_handler, NULL);
	register_trace_android_vh_cpu_idle_enter(record_idle_enter_handler, NULL);
	register_trace_android_vh_cpu_idle_exit(record_idle_exit_handler, NULL);

	register_trace_android_rvh_select_task_rq_fair(select_task_rq_handler, NULL);
	register_trace_android_rvh_enqueue_entity(enqueue_entity_handler, NULL);
	register_trace_android_rvh_dequeue_entity(dequeue_entity_handler, NULL);
#endif
}
