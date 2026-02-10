/*
 * Meta HzOS Ext scheduler subsystem
 *
 * Copyright (c) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/cgroup.h>
#include <linux/cgroup-defs.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/lockdep.h>
#include <linux/mutex.h>
#include <linux/hzos_ext-kern.h>
#include <linux/percpu-defs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
#include <trace/hooks/cgroup.h>
#include <trace/hooks/cpuidle.h>
#include <trace/hooks/sched.h>
#endif /* CONFIG_ANDROID_VENDOR_HOOKS */

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

static const struct cpumask *task_cpumask(struct task_struct *p)
{
#ifdef CONFIG_ANDROID_VENDOR_HOOKS
	return p->cpus_ptr;
#else
	return &p->cpus_allowed;
#endif
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
		cpumask_and(&p->hzos_ext.preferred_mask, preferred, allowed);
}

static void set_allowed_mask_handler(void *unused, struct task_struct *p,
				     const struct cpumask *new_mask)
{
	task_update_allowed(p, new_mask);
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
	return cpumask_test_cpu(cpu, &p->hzos_ext.preferred_mask);
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

	if (!hzos_ext_feature_enabled(HZOS_EXT_FEATURE_SELECT_RQ_IDLE))
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
		hzos_ext_stat_event_inc(p, NR_SRQ_PREV);
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
			hzos_ext_stat_event_inc(p, NR_SRQ_SYNC);
			return;
		}
	}

	while (true) {
		cpu = cpumask_any_and(&p->hzos_ext.preferred_mask, idle_mask);

		if (cpu >= nr_cpu_ids)
			return;

		if (try_reserve_store_idle_cpu(cpu, target_cpu)) {
			hzos_ext_stat_event_inc(p, NR_SRQ_ANY);
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

void hzos_ext_task_setscheduler(void *unused, struct task_struct *task,
			        		    const struct sched_attr *attr, int *retval)
{
	int policy = attr->sched_policy;

	*retval = 0;

	if (!hzos_ext_feature_enabled(HZOS_EXT_FEATURE_ALLOW_RT))
		return;

	if ((policy == SCHED_FIFO || policy == SCHED_RR) &&
	    !hzos_ext_task_has_flag(task, HZOS_EXT_FLAG_ALLOW_RT)) {

		pr_info("%s[%d] ALLOW_RT permission denied", task->comm, task->pid);
		*retval = -EPERM;
	}

	return;
}

void hzos_ext_sched_post_clone(struct task_struct *new)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	unsigned long flags;

	/*
	 * task_update_allowed() can be called with interrupts disabled when
	 * updating its allowed cpumask. That should never happen here due to
	 * the fact that we're still on the clone path and the task is not yet
	 * runnable, but just to future proof let's do the safe thing and also
	 * disable interrupts here as this is a cold path and a small critical
	 * path.
	 */
	raw_spin_lock_irqsave(&new->pi_lock, flags);
	task_update_allowed(new, task_cpumask(new));
	raw_spin_unlock_irqrestore(&new->pi_lock, flags);
#endif
}

static DEFINE_MUTEX(preferred_mask_mutex);
static ssize_t sched_group_set_preferred_mask(struct cgroup_subsys_state *css,
											  const struct cpumask *preferred)
{
	struct task_struct *p;
	struct css_task_iter it;
	unsigned long flags;
	struct cpumask *tg_mask = css_tg_preferred_mask(css);

	/* We can't change the preferred mask of the root cgroup */
	if (!css->parent)
		return -EINVAL;

	mutex_lock(&preferred_mask_mutex);
	css_task_iter_start(css, 0, &it);
	cpumask_copy(tg_mask, preferred);
	while ((p = css_task_iter_next(&it))) {
		raw_spin_lock_irqsave(&p->pi_lock, flags);
		task_update_allowed(p, task_cpumask(p));
		raw_spin_unlock_irqrestore(&p->pi_lock, flags);
	}
	mutex_unlock(&preferred_mask_mutex);
	css_task_iter_end(&it);

	return 0;
}

ssize_t hzos_ext_preferred_mask_write(struct kernfs_open_file *of, char *buf,
			     size_t nbytes, loff_t off)
{
	ssize_t err;
	cpumask_var_t new_mask;

	if (!alloc_cpumask_var(&new_mask, GFP_KERNEL))
		return -ENOMEM;

	err = cpumask_parse(buf, new_mask);
	if (err)
		goto free_mask;

	err = sched_group_set_preferred_mask(of_css(of), new_mask);
free_mask:
	free_cpumask_var(new_mask);
	return err ?: nbytes;
}

int hzos_ext_preferred_mask_read(struct seq_file *sf, void *v)
{
	char *kbuf = kmalloc(cpumask_size() + 1, GFP_KERNEL);
	struct cpumask *tg_mask;

	if (!kbuf)
		return -ENOMEM;

	tg_mask = css_tg_preferred_mask(seq_css(sf));
	cpumap_print_to_pagebuf(false, kbuf, tg_mask);
	seq_printf(sf, "0x%s\n", kbuf);
	kfree(kbuf);

	return 0;
}

void hzos_ext_sched_init(void)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	BUG_ON(!alloc_cpumask_var(&idle_mask, GFP_KERNEL));

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
	register_trace_android_rvh_set_cpus_allowed_comm(set_allowed_mask_handler, NULL);
	register_trace_android_vh_cpu_idle_enter(record_idle_enter_handler, NULL);
	register_trace_android_vh_cpu_idle_exit(record_idle_exit_handler, NULL);
	register_trace_android_vh_task_setscheduler(hzos_ext_task_setscheduler, NULL);

	register_trace_android_rvh_select_task_rq_fair(select_task_rq_handler, NULL);
	register_trace_android_rvh_enqueue_entity(enqueue_entity_handler, NULL);
	register_trace_android_rvh_dequeue_entity(dequeue_entity_handler, NULL);
#endif /* CONFIG_ANDROID_VENDOR_HOOKS */
#endif /* CONFIG_FAIR_GROUP_SCHED */
}

#ifndef CONFIG_ANDROID_VENDOR_HOOKS
void hzos_ext_set_cpus_allowed(struct task_struct *p,
				     		   const struct cpumask *new_mask)
{
	set_allowed_mask_handler(NULL, p, new_mask);
}

void hzos_ext_cpu_idle_enter(int *state, struct cpuidle_device *dev)
{
	record_idle_enter_handler(NULL, state, dev);
}

void hzos_ext_cpu_idle_exit(int state, struct cpuidle_device *dev)
{
	record_idle_exit_handler(NULL, state, dev);
}

void hzos_ext_select_task_rq_fair(struct task_struct *p, int prev_cpu,
								  int sd_flag, int wake_flags,
								  int *target_cpu)
{
	select_task_rq_handler(NULL, p, prev_cpu, sd_flag, wake_flags, target_cpu);
}

void hzos_ext_enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	enqueue_entity_handler(NULL, cfs_rq, se);
}

void hzos_ext_dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	dequeue_entity_handler(NULL, cfs_rq, se);
}
#endif /* !CONFIG_ANDROID_VENDOR_HOOKS */
