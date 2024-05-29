// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/syscore_ops.h>
#include <linux/cpufreq.h>
#include <linux/list_sort.h>
#include <linux/jiffies.h>
#include <linux/sched/stat.h>
#include <linux/module.h>
#include <linux/kmemleak.h>
#include <linux/ktime.h>
#include <linux/qcom-cpufreq-hw.h>
#include <linux/cpumask.h>

#include <trace/hooks/sched.h>
#include <trace/hooks/cpufreq.h>
#include <trace/hooks/topology.h>
#include <trace/events/power.h>

#include "walt.h"
#include "trace.h"

const char *task_event_names[] = {
	"PUT_PREV_TASK",
	"PICK_NEXT_TASK",
	"TASK_WAKE",
	"TASK_MIGRATE",
	"TASK_UPDATE",
	"IRQ_UPDATE"
};

const char *migrate_type_names[] = {
	"GROUP_TO_RQ",
	"RQ_TO_GROUP",
	"RQ_TO_RQ",
	"GROUP_TO_GROUP"
};

#define SCHED_FREQ_ACCOUNT_WAIT_TIME 0
#define SCHED_ACCOUNT_WAIT_TIME 1

#define EARLY_DETECTION_DURATION 9500000
#define MAX_NUM_CGROUP_COLOC_ID 20

#define MAX_NR_CLUSTERS			3

#define NEW_TASK_ACTIVE_TIME 100000000

unsigned int sysctl_sched_user_hint;

static ktime_t ktime_last;
static bool walt_ktime_suspended;

static bool use_cycle_counter;
static DEFINE_MUTEX(cluster_lock);
static u64 walt_load_reported_window;

static struct irq_work walt_cpufreq_irq_work;
struct irq_work walt_migration_irq_work;
unsigned int walt_rotation_enabled;
cpumask_t asym_cap_sibling_cpus = CPU_MASK_NONE;

unsigned int __read_mostly sched_ravg_window = 20000000;
unsigned int min_max_possible_capacity = 1024;
unsigned int max_possible_capacity = 1024; /* max(rq->max_possible_capacity) */
/* Initial task load. Newly created tasks are assigned this load. */
unsigned int __read_mostly sched_init_task_load_windows;
/*
 * Task load is categorized into buckets for the purpose of top task tracking.
 * The entire range of load from 0 to sched_ravg_window needs to be covered
 * in NUM_LOAD_INDICES number of buckets. Therefore the size of each bucket
 * is given by sched_ravg_window / NUM_LOAD_INDICES. Since the default value
 * of sched_ravg_window is DEFAULT_SCHED_RAVG_WINDOW, use that to compute
 * sched_load_granule.
 */
unsigned int __read_mostly sched_load_granule;

/*
 *@boost:should be 0,1,2.
 *@period:boost time based on ms units.
 */
int set_task_boost(int boost, u64 period)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) current->android_vendor_data1;

	if (boost < TASK_BOOST_NONE || boost >= TASK_BOOST_END)
		return -EINVAL;
	if (boost) {
		wts->boost = boost;
		wts->boost_period = (u64)period * 1000 * 1000;
		wts->boost_expires = sched_clock() + wts->boost_period;
	} else {
		wts->boost = 0;
		wts->boost_expires = 0;
		wts->boost_period = 0;
	}
	return 0;
}
EXPORT_SYMBOL(set_task_boost);

u64 walt_ktime_get_ns(void)
{
	if (unlikely(walt_ktime_suspended))
		return ktime_to_ns(ktime_last);
	return ktime_get_ns();
}

static void walt_resume(void)
{
	walt_ktime_suspended = false;
}

static int walt_suspend(void)
{
	ktime_last = ktime_get();
	walt_ktime_suspended = true;
	return 0;
}

static struct syscore_ops walt_syscore_ops = {
	.resume		= walt_resume,
	.suspend	= walt_suspend
};

static inline void acquire_rq_locks_irqsave(const cpumask_t *cpus,
				     unsigned long *flags)
{
	int cpu;
	int level = 0;

	local_irq_save(*flags);

	for_each_cpu(cpu, cpus) {
		if (level == 0)
			raw_spin_lock(&cpu_rq(cpu)->lock);
		else
			raw_spin_lock_nested(&cpu_rq(cpu)->lock, level);
		level++;
	}
}

static inline void release_rq_locks_irqrestore(const cpumask_t *cpus,
					unsigned long *flags)
{
	int cpu;

	for_each_cpu(cpu, cpus)
		raw_spin_unlock(&cpu_rq(cpu)->lock);
	local_irq_restore(*flags);
}

static unsigned int walt_cpu_high_irqload;

static __read_mostly unsigned int sched_ravg_hist_size = 5;

static __read_mostly unsigned int sched_io_is_busy = 1;

/* Window size (in ns) */
static __read_mostly unsigned int new_sched_ravg_window = DEFAULT_SCHED_RAVG_WINDOW;

static DEFINE_SPINLOCK(sched_ravg_window_lock);
static u64 sched_ravg_window_change_time;

static unsigned int __read_mostly sched_init_task_load_windows_scaled;
static unsigned int __read_mostly sysctl_sched_init_task_load_pct = 15;

/* Size of bitmaps maintained to track top tasks */
static const unsigned int top_tasks_bitmap_size =
		BITS_TO_LONGS(NUM_LOAD_INDICES + 1) * sizeof(unsigned long);

__read_mostly unsigned int walt_scale_demand_divisor;

#define SCHED_PRINT(arg)	printk_deferred("%s=%llu", #arg, arg)
#define STRG(arg)		#arg

void walt_task_dump(struct task_struct *p)
{
	char buff[WALT_NR_CPUS * 16];
	int i, j = 0;
	int buffsz = WALT_NR_CPUS * 16;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	bool is_32bit_thread = is_compat_thread(task_thread_info(p));

	printk_deferred("Task: %.16s-%d\n", p->comm, p->pid);
	SCHED_PRINT(p->state);
	SCHED_PRINT(p->cpu);
	SCHED_PRINT(p->policy);
	SCHED_PRINT(p->prio);
	SCHED_PRINT(wts->mark_start);
	SCHED_PRINT(wts->demand);
	SCHED_PRINT(wts->coloc_demand);
	SCHED_PRINT(sched_ravg_window);
	SCHED_PRINT(new_sched_ravg_window);

	for (i = 0 ; i < nr_cpu_ids; i++)
		j += scnprintf(buff + j, buffsz - j, "%u ",
				wts->curr_window_cpu[i]);
	printk_deferred("%s=%u (%s)\n", STRG(wts->curr_window),
			wts->curr_window, buff);

	for (i = 0, j = 0 ; i < nr_cpu_ids; i++)
		j += scnprintf(buff + j, buffsz - j, "%u ",
				wts->prev_window_cpu[i]);
	printk_deferred("%s=%u (%s)\n", STRG(wts->prev_window),
			wts->prev_window, buff);

	SCHED_PRINT(wts->last_wake_ts);
	SCHED_PRINT(wts->last_enqueued_ts);
	SCHED_PRINT(wts->misfit);
	SCHED_PRINT(wts->unfilter);
	SCHED_PRINT(is_32bit_thread);
	SCHED_PRINT(wts->grp);
	SCHED_PRINT(p->on_cpu);
	SCHED_PRINT(p->on_rq);
}

void walt_rq_dump(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *tsk = cpu_curr(cpu);
	int i;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	/*
	 * Increment the task reference so that it can't be
	 * freed on a remote CPU. Since we are going to
	 * enter panic, there is no need to decrement the
	 * task reference. Decrementing the task reference
	 * can't be done in atomic context, especially with
	 * rq locks held.
	 */
	get_task_struct(tsk);
	printk_deferred("CPU:%d nr_running:%u current: %d (%s)\n",
			cpu, rq->nr_running, tsk->pid, tsk->comm);

	printk_deferred("==========================================");
	SCHED_PRINT(wrq->window_start);
	SCHED_PRINT(wrq->prev_window_size);
	SCHED_PRINT(wrq->curr_runnable_sum);
	SCHED_PRINT(wrq->prev_runnable_sum);
	SCHED_PRINT(wrq->nt_curr_runnable_sum);
	SCHED_PRINT(wrq->nt_prev_runnable_sum);
	SCHED_PRINT(wrq->task_exec_scale);
	SCHED_PRINT(wrq->grp_time.curr_runnable_sum);
	SCHED_PRINT(wrq->grp_time.prev_runnable_sum);
	SCHED_PRINT(wrq->grp_time.nt_curr_runnable_sum);
	SCHED_PRINT(wrq->grp_time.nt_prev_runnable_sum);
	for (i = 0 ; i < NUM_TRACKED_WINDOWS; i++) {
		printk_deferred("wrq->load_subs[%d].window_start=%llu)\n", i,
				wrq->load_subs[i].window_start);
		printk_deferred("wrq->load_subs[%d].subs=%llu)\n", i,
				wrq->load_subs[i].subs);
		printk_deferred("wrq->load_subs[%d].new_subs=%llu)\n", i,
				wrq->load_subs[i].new_subs);
	}
	walt_task_dump(tsk);
	SCHED_PRINT(sched_capacity_margin_up[cpu]);
	SCHED_PRINT(sched_capacity_margin_down[cpu]);
}

void walt_dump(void)
{
	int cpu;

	printk_deferred("============ WALT RQ DUMP START ==============\n");
	printk_deferred("Sched ktime_get: %llu\n", walt_ktime_get_ns());
	printk_deferred("Time last window changed=%lu\n",
			sched_ravg_window_change_time);
	for_each_online_cpu(cpu)
		walt_rq_dump(cpu);
	SCHED_PRINT(max_possible_capacity);
	SCHED_PRINT(min_max_possible_capacity);
	printk_deferred("============ WALT RQ DUMP END ==============\n");
}

int in_sched_bug;

static inline void
fixup_cumulative_runnable_avg(struct rq *rq,
			      struct task_struct *p,
			      struct walt_sched_stats *stats,
			      s64 demand_scaled_delta,
			      s64 pred_demand_scaled_delta)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	s64 cumulative_runnable_avg_scaled =
		stats->cumulative_runnable_avg_scaled + demand_scaled_delta;
	s64 pred_demands_sum_scaled =
		stats->pred_demands_sum_scaled + pred_demand_scaled_delta;

	lockdep_assert_held(&rq->lock);

	if (task_rq(p) != rq)
		WALT_BUG(p, "on CPU %d task %s(%d) not on rq %d",
			 raw_smp_processor_id(), p->comm, p->pid, rq->cpu);

	if (cumulative_runnable_avg_scaled < 0) {
		WALT_BUG(p, "on CPU %d task ds=%llu is higher than cra=%llu\n",
			 raw_smp_processor_id(), wts->demand_scaled,
			 stats->cumulative_runnable_avg_scaled);
		cumulative_runnable_avg_scaled = 0;
	}
	stats->cumulative_runnable_avg_scaled = (u64)cumulative_runnable_avg_scaled;

	if (pred_demands_sum_scaled < 0) {
		WALT_BUG(p, "on CPU %d task pds=%llu is higher than pds_sum=%llu\n",
			 raw_smp_processor_id(), wts->pred_demand_scaled,
			 stats->pred_demands_sum_scaled);
		pred_demands_sum_scaled = 0;
	}
	stats->pred_demands_sum_scaled = (u64)pred_demands_sum_scaled;
}

static void fixup_walt_sched_stats_common(struct rq *rq, struct task_struct *p,
				   u16 updated_demand_scaled,
				   u16 updated_pred_demand_scaled)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	s64 task_load_delta = (s64)updated_demand_scaled -
			      wts->demand_scaled;
	s64 pred_demand_delta = (s64)updated_pred_demand_scaled -
				wts->pred_demand_scaled;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	fixup_cumulative_runnable_avg(rq, p, &wrq->walt_stats, task_load_delta,
				      pred_demand_delta);
}

/*
 * Demand aggregation for frequency purpose:
 *
 * CPU demand of tasks from various related groups is aggregated per-cluster and
 * added to the "max_busy_cpu" in that cluster, where max_busy_cpu is determined
 * by just wrq->prev_runnable_sum.
 *
 * Some examples follow, which assume:
 *	Cluster0 = CPU0-3, Cluster1 = CPU4-7
 *	One related thread group A that has tasks A0, A1, A2
 *
 *	A->cpu_time[X].curr/prev_sum = counters in which cpu execution stats of
 *	tasks belonging to group A are accumulated when they run on cpu X.
 *
 *	CX->curr/prev_sum = counters in which cpu execution stats of all tasks
 *	not belonging to group A are accumulated when they run on cpu X
 *
 * Lets say the stats for window M was as below:
 *
 *	C0->prev_sum = 1ms, A->cpu_time[0].prev_sum = 5ms
 *		Task A0 ran 5ms on CPU0
 *		Task B0 ran 1ms on CPU0
 *
 *	C1->prev_sum = 5ms, A->cpu_time[1].prev_sum = 6ms
 *		Task A1 ran 4ms on CPU1
 *		Task A2 ran 2ms on CPU1
 *		Task B1 ran 5ms on CPU1
 *
 *	C2->prev_sum = 0ms, A->cpu_time[2].prev_sum = 0
 *		CPU2 idle
 *
 *	C3->prev_sum = 0ms, A->cpu_time[3].prev_sum = 0
 *		CPU3 idle
 *
 * In this case, CPU1 was most busy going by just its prev_sum counter. Demand
 * from all group A tasks are added to CPU1. IOW, at end of window M, cpu busy
 * time reported to governor will be:
 *
 *
 *	C0 busy time = 1ms
 *	C1 busy time = 5 + 5 + 6 = 16ms
 *
 */
__read_mostly bool sched_freq_aggr_en;

static u64
update_window_start(struct rq *rq, u64 wallclock, int event)
{
	s64 delta;
	int nr_windows;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 old_window_start = wrq->window_start;

	delta = wallclock - wrq->window_start;
	if (delta < 0) {
		printk_deferred("WALT-BUG CPU%d; wallclock=%llu(0x%llx) is lesser than window_start=%llu(0x%llx)",
				rq->cpu, wallclock, wallclock,
				wrq->window_start, wrq->window_start);
		WALT_PANIC(1);
	}
	if (delta < sched_ravg_window)
		return old_window_start;

	nr_windows = div64_u64(delta, sched_ravg_window);
	wrq->window_start += (u64)nr_windows * (u64)sched_ravg_window;

	wrq->prev_window_size = sched_ravg_window;

	return old_window_start;
}

/*
 * Assumes rq_lock is held and wallclock was recorded in the same critical
 * section as this function's invocation.
 */
static inline u64 read_cycle_counter(int cpu, u64 wallclock)
{
	struct walt_rq *wrq = (struct walt_rq *) cpu_rq(cpu)->android_vendor_data1;

	if (wrq->last_cc_update != wallclock) {
		wrq->cycles = qcom_cpufreq_get_cpu_cycle_counter(cpu);
		wrq->last_cc_update = wallclock;
	}

	return wrq->cycles;
}

static void update_task_cpu_cycles(struct task_struct *p, int cpu,
				   u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (use_cycle_counter)
		wts->cpu_cycles = read_cycle_counter(cpu, wallclock);
}

static inline bool is_ed_enabled(void)
{
	return (walt_rotation_enabled || (boost_policy != SCHED_BOOST_NONE));
}

static inline bool is_ed_task(struct task_struct *p, u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	return (wallclock - wts->last_wake_ts >= EARLY_DETECTION_DURATION);
}

static bool is_ed_task_present(struct rq *rq, u64 wallclock, struct task_struct *deq_task)
{
	struct task_struct *p;
	int loop_max = 10;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	wrq->ed_task = NULL;

	if (!is_ed_enabled() || !rq->cfs.h_nr_running)
		return false;

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (!loop_max)
			break;

		if (p == deq_task)
			continue;

		if (is_ed_task(p, wallclock)) {
			wrq->ed_task = p;
			return true;
		}

		loop_max--;
	}

	return false;
}

static void walt_sched_account_irqstart(int cpu, struct task_struct *curr)
{
	struct rq *rq = cpu_rq(cpu);
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	if (!wrq->window_start)
		return;

	/* We're here without rq->lock held, IRQ disabled */
	raw_spin_lock(&rq->lock);
	update_task_cpu_cycles(curr, cpu, walt_ktime_get_ns());
	raw_spin_unlock(&rq->lock);
}

static void walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime);
static void walt_sched_account_irqend(int cpu, struct task_struct *curr, u64 delta)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

	raw_spin_lock_irqsave(&rq->lock, flags);
	walt_update_task_ravg(curr, rq, IRQ_UPDATE, walt_ktime_get_ns(), delta);
	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

/*
 * Return total number of tasks "eligible" to run on higher capacity cpus
 */
unsigned int walt_big_tasks(int cpu)
{
	struct walt_rq *wrq = (struct walt_rq *) cpu_rq(cpu)->android_vendor_data1;

	return wrq->walt_stats.nr_big_tasks;
}

static void clear_walt_request(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	clear_reserved(cpu);
	if (wrq->push_task) {
		struct task_struct *push_task = NULL;

		raw_spin_lock_irqsave(&rq->lock, flags);
		if (wrq->push_task) {
			clear_reserved(rq->push_cpu);
			push_task = wrq->push_task;
			wrq->push_task = NULL;
		}
		rq->active_balance = 0;
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		if (push_task)
			put_task_struct(push_task);
	}
}

/*
 * Special case the last index and provide a fast path for index = 0.
 * Note that sched_load_granule can change underneath us if we are not
 * holding any runqueue locks while calling the two functions below.
 */
static u32 top_task_load(struct rq *rq)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	int index = wrq->prev_top;
	u8 prev = 1 - wrq->curr_table;

	if (!index) {
		int msb = NUM_LOAD_INDICES - 1;

		if (!test_bit(msb, wrq->top_tasks_bitmap[prev]))
			return 0;
		else
			return sched_load_granule;
	} else if (index == NUM_LOAD_INDICES - 1) {
		return sched_ravg_window;
	} else {
		return (index + 1) * sched_load_granule;
	}
}

unsigned long sched_user_hint_reset_time;
static bool is_cluster_hosting_top_app(struct walt_sched_cluster *cluster);

static inline bool
should_apply_suh_freq_boost(struct walt_sched_cluster *cluster)
{
	if (sched_freq_aggr_en || !sysctl_sched_user_hint ||
				  !cluster->aggr_grp_load)
		return false;

	return is_cluster_hosting_top_app(cluster);
}

static inline u64 freq_policy_load(struct rq *rq)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_sched_cluster *cluster = wrq->cluster;
	u64 aggr_grp_load = cluster->aggr_grp_load;
	u64 load, tt_load = 0;
	struct task_struct *cpu_ksoftirqd = per_cpu(ksoftirqd, cpu_of(rq));

	if (wrq->ed_task != NULL) {
		load = sched_ravg_window;
		goto done;
	}

	if (sched_freq_aggr_en)
		load = wrq->prev_runnable_sum + aggr_grp_load;
	else
		load = wrq->prev_runnable_sum +
					wrq->grp_time.prev_runnable_sum;

	if (cpu_ksoftirqd && cpu_ksoftirqd->state == TASK_RUNNING)
		load = max_t(u64, load, task_load(cpu_ksoftirqd));

	tt_load = top_task_load(rq);
	load = max_t(u64, load, tt_load);

	if (should_apply_suh_freq_boost(cluster)) {
		if (is_suh_max())
			load = sched_ravg_window;
		else
			load = div64_u64(load * sysctl_sched_user_hint,
					 (u64)100);
	}

done:
	trace_sched_load_to_gov(rq, aggr_grp_load, tt_load, sched_freq_aggr_en,
				load, 0, walt_rotation_enabled,
				sysctl_sched_user_hint, wrq);
	return load;
}

static bool rtgb_active;

static inline unsigned long
__cpu_util_freq_walt(int cpu, struct walt_cpu_load *walt_load)
{
	u64 util;
	struct rq *rq = cpu_rq(cpu);
	unsigned long capacity = capacity_orig_of(cpu);
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	util = div64_u64(freq_policy_load(rq),
			sched_ravg_window >> SCHED_CAPACITY_SHIFT);

	if (walt_load) {
		u64 nl = wrq->nt_prev_runnable_sum +
				wrq->grp_time.nt_prev_runnable_sum;
		u64 pl = wrq->walt_stats.pred_demands_sum_scaled;

		wrq->old_busy_time = util;
		wrq->old_estimated_time = pl;

		nl = div64_u64(nl, sched_ravg_window >> SCHED_CAPACITY_SHIFT);
		walt_load->nl = nl;
		walt_load->pl = pl;
		walt_load->ws = walt_load_reported_window;
		walt_load->rtgb_active = rtgb_active;
	}

	return (util >= capacity) ? capacity : util;
}

#define ADJUSTED_ASYM_CAP_CPU_UTIL(orig, other, x)	\
			(max(orig, mult_frac(other, x, 100)))

unsigned long
cpu_util_freq_walt(int cpu, struct walt_cpu_load *walt_load)
{
	struct walt_cpu_load wl_other = {0};
	unsigned long util = 0, util_other = 0;
	unsigned long capacity = capacity_orig_of(cpu);
	int i, mpct = sysctl_sched_asym_cap_sibling_freq_match_pct;

	if (!cpumask_test_cpu(cpu, &asym_cap_sibling_cpus))
		return __cpu_util_freq_walt(cpu, walt_load);

	for_each_cpu(i, &asym_cap_sibling_cpus) {
		if (i == cpu)
			util = __cpu_util_freq_walt(cpu, walt_load);
		else
			util_other = __cpu_util_freq_walt(i, &wl_other);
	}

	if (cpu == cpumask_last(&asym_cap_sibling_cpus))
		mpct = 100;

	util = ADJUSTED_ASYM_CAP_CPU_UTIL(util, util_other, mpct);

	walt_load->nl = ADJUSTED_ASYM_CAP_CPU_UTIL(walt_load->nl, wl_other.nl,
						   mpct);
	walt_load->pl = ADJUSTED_ASYM_CAP_CPU_UTIL(walt_load->pl, wl_other.pl,
						   mpct);

	return (util >= capacity) ? capacity : util;
}

/*
 * In this function we match the accumulated subtractions with the current
 * and previous windows we are operating with. Ignore any entries where
 * the window start in the load_subtraction struct does not match either
 * the curent or the previous window. This could happen whenever CPUs
 * become idle or busy with interrupts disabled for an extended period.
 */
static inline void account_load_subtractions(struct rq *rq)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 ws = wrq->window_start;
	u64 prev_ws = ws - wrq->prev_window_size;
	struct load_subtractions *ls = wrq->load_subs;
	int i;

	for (i = 0; i < NUM_TRACKED_WINDOWS; i++) {
		if (ls[i].window_start == ws) {
			wrq->curr_runnable_sum -= ls[i].subs;
			wrq->nt_curr_runnable_sum -= ls[i].new_subs;
		} else if (ls[i].window_start == prev_ws) {
			wrq->prev_runnable_sum -= ls[i].subs;
			wrq->nt_prev_runnable_sum -= ls[i].new_subs;
		}

		ls[i].subs = 0;
		ls[i].new_subs = 0;
	}

	if ((s64)wrq->prev_runnable_sum < 0) {
		WALT_BUG(NULL, "wrq->prev_runnable_sum=%llu < 0",
				(s64)wrq->prev_runnable_sum);
		wrq->prev_runnable_sum = 0;
	}
	if ((s64)wrq->curr_runnable_sum < 0) {
		WALT_BUG(NULL, "wrq->curr_runnable_sum=%llu < 0",
				(s64)wrq->curr_runnable_sum);
		wrq->curr_runnable_sum = 0;
	}
	if ((s64)wrq->nt_prev_runnable_sum < 0) {
		WALT_BUG(NULL, "wrq->nt_prev_runnable_sum=%llu < 0",
				(s64)wrq->nt_prev_runnable_sum);
		wrq->nt_prev_runnable_sum = 0;
	}
	if ((s64)wrq->nt_curr_runnable_sum < 0) {
		WALT_BUG(NULL, "wrq->nt_curr_runnable_sum=%llu < 0",
				(s64)wrq->nt_curr_runnable_sum);
		wrq->nt_curr_runnable_sum = 0;
	}
}

static inline void create_subtraction_entry(struct rq *rq, u64 ws, int index)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	wrq->load_subs[index].window_start = ws;
	wrq->load_subs[index].subs = 0;
	wrq->load_subs[index].new_subs = 0;
}

static int get_top_index(unsigned long *bitmap, unsigned long old_top)
{
	int index = find_next_bit(bitmap, NUM_LOAD_INDICES, old_top);

	if (index == NUM_LOAD_INDICES)
		return 0;

	return NUM_LOAD_INDICES - 1 - index;
}

static bool get_subtraction_index(struct rq *rq, u64 ws)
{
	int i;
	u64 oldest = ULLONG_MAX;
	int oldest_index = 0;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	for (i = 0; i < NUM_TRACKED_WINDOWS; i++) {
		u64 entry_ws = wrq->load_subs[i].window_start;

		if (ws == entry_ws)
			return i;

		if (entry_ws < oldest) {
			oldest = entry_ws;
			oldest_index = i;
		}
	}

	create_subtraction_entry(rq, ws, oldest_index);
	return oldest_index;
}

static void update_rq_load_subtractions(int index, struct rq *rq,
					u32 sub_load, bool new_task)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	wrq->load_subs[index].subs += sub_load;
	if (new_task)
		wrq->load_subs[index].new_subs += sub_load;
}

static void update_cluster_load_subtractions(struct task_struct *p,
					int cpu, u64 ws, bool new_task)
{
	struct walt_sched_cluster *cluster = cpu_cluster(cpu);
	struct cpumask cluster_cpus = cluster->cpus;
	struct walt_rq *wrq = (struct walt_rq *) cpu_rq(cpu)->android_vendor_data1;
	u64 prev_ws = ws - wrq->prev_window_size;
	int i;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	cpumask_clear_cpu(cpu, &cluster_cpus);
	raw_spin_lock(&cluster->load_lock);

	for_each_cpu(i, &cluster_cpus) {
		struct rq *rq = cpu_rq(i);
		int index;

		if (wts->curr_window_cpu[i]) {
			index = get_subtraction_index(rq, ws);
			update_rq_load_subtractions(index, rq,
				wts->curr_window_cpu[i], new_task);
			wts->curr_window_cpu[i] = 0;
		}

		if (wts->prev_window_cpu[i]) {
			index = get_subtraction_index(rq, prev_ws);
			update_rq_load_subtractions(index, rq,
				wts->prev_window_cpu[i], new_task);
			wts->prev_window_cpu[i] = 0;
		}
	}

	raw_spin_unlock(&cluster->load_lock);
}

static inline void inter_cluster_migration_fixup
	(struct task_struct *p, int new_cpu, int task_cpu, bool new_task)
{
	struct rq *dest_rq = cpu_rq(new_cpu);
	struct rq *src_rq = cpu_rq(task_cpu);
	struct walt_rq *dest_wrq = (struct walt_rq *) dest_rq->android_vendor_data1;
	struct walt_rq *src_wrq = (struct walt_rq *) src_rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (same_freq_domain(new_cpu, task_cpu))
		return;

	wts->curr_window_cpu[new_cpu] = wts->curr_window;
	wts->prev_window_cpu[new_cpu] = wts->prev_window;

	dest_wrq->curr_runnable_sum += wts->curr_window;
	dest_wrq->prev_runnable_sum += wts->prev_window;

	if (src_wrq->curr_runnable_sum < wts->curr_window_cpu[task_cpu]) {
		WALT_BUG(p, "pid=%u CPU%d -> CPU%d src_crs=%llu is lesser than task_contrib=%llu",
			 p->pid, src_rq->cpu, dest_rq->cpu,
			 src_wrq->curr_runnable_sum,
			 wts->curr_window_cpu[task_cpu]);
		src_wrq->curr_runnable_sum = wts->curr_window_cpu[task_cpu];
	}
	src_wrq->curr_runnable_sum -= wts->curr_window_cpu[task_cpu];

	if (src_wrq->prev_runnable_sum < wts->prev_window_cpu[task_cpu]) {
		WALT_BUG(p, "pid=%u CPU%d -> CPU%d src_prs=%llu is lesser than task_contrib=%llu",
			 p->pid, src_rq->cpu, dest_rq->cpu,
			 src_wrq->prev_runnable_sum,
			 wts->prev_window_cpu[task_cpu]);
		 src_wrq->prev_runnable_sum = wts->prev_window_cpu[task_cpu];
	}
	src_wrq->prev_runnable_sum -= wts->prev_window_cpu[task_cpu];

	if (new_task) {
		dest_wrq->nt_curr_runnable_sum += wts->curr_window;
		dest_wrq->nt_prev_runnable_sum += wts->prev_window;

		if (src_wrq->nt_curr_runnable_sum < wts->curr_window_cpu[task_cpu]) {
			WALT_BUG(p, "pid=%u CPU%d -> CPU%d src_nt_crs=%llu is lesser than task_contrib=%llu",
				 p->pid, src_rq->cpu, dest_rq->cpu,
				 src_wrq->nt_curr_runnable_sum,
				 wts->curr_window_cpu[task_cpu]);
			src_wrq->nt_curr_runnable_sum = wts->curr_window_cpu[task_cpu];
		}
		src_wrq->nt_curr_runnable_sum -=
				wts->curr_window_cpu[task_cpu];

		if (src_wrq->nt_prev_runnable_sum < wts->prev_window_cpu[task_cpu]) {
			WALT_BUG(p, "pid=%u CPU%d -> CPU%d src_nt_prs=%llu is lesser than task_contrib=%llu",
				 p->pid, src_rq->cpu, dest_rq->cpu,
				 src_wrq->nt_prev_runnable_sum,
				 wts->prev_window_cpu[task_cpu]);
			src_wrq->nt_prev_runnable_sum = wts->prev_window_cpu[task_cpu];
		}
		src_wrq->nt_prev_runnable_sum -=
				wts->prev_window_cpu[task_cpu];
	}

	wts->curr_window_cpu[task_cpu] = 0;
	wts->prev_window_cpu[task_cpu] = 0;

	update_cluster_load_subtractions(p, task_cpu,
			src_wrq->window_start, new_task);
}

static u32 load_to_index(u32 load)
{
	u32 index = load / sched_load_granule;

	return min(index, (u32)(NUM_LOAD_INDICES - 1));
}

static void
migrate_top_tasks(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	int index;
	int top_index;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u32 curr_window = wts->curr_window;
	u32 prev_window = wts->prev_window;
	struct walt_rq *dst_wrq = (struct walt_rq *) dst_rq->android_vendor_data1;
	struct walt_rq *src_wrq = (struct walt_rq *) src_rq->android_vendor_data1;
	u8 src = src_wrq->curr_table;
	u8 dst = dst_wrq->curr_table;
	u8 *src_table;
	u8 *dst_table;

	if (curr_window) {
		src_table = src_wrq->top_tasks[src];
		dst_table = dst_wrq->top_tasks[dst];
		index = load_to_index(curr_window);
		src_table[index] -= 1;
		dst_table[index] += 1;

		if (!src_table[index])
			__clear_bit(NUM_LOAD_INDICES - index - 1,
				src_wrq->top_tasks_bitmap[src]);

		if (dst_table[index] == 1)
			__set_bit(NUM_LOAD_INDICES - index - 1,
				dst_wrq->top_tasks_bitmap[dst]);

		if (index > dst_wrq->curr_top)
			dst_wrq->curr_top = index;

		top_index = src_wrq->curr_top;
		if (index == top_index && !src_table[index])
			src_wrq->curr_top = get_top_index(
				src_wrq->top_tasks_bitmap[src], top_index);
	}

	if (prev_window) {
		src = 1 - src;
		dst = 1 - dst;
		src_table = src_wrq->top_tasks[src];
		dst_table = dst_wrq->top_tasks[dst];
		index = load_to_index(prev_window);
		src_table[index] -= 1;
		dst_table[index] += 1;

		if (!src_table[index])
			__clear_bit(NUM_LOAD_INDICES - index - 1,
				src_wrq->top_tasks_bitmap[src]);

		if (dst_table[index] == 1)
			__set_bit(NUM_LOAD_INDICES - index - 1,
				dst_wrq->top_tasks_bitmap[dst]);

		if (index > dst_wrq->prev_top)
			dst_wrq->prev_top = index;

		top_index = src_wrq->prev_top;
		if (index == top_index && !src_table[index])
			src_wrq->prev_top = get_top_index(
				src_wrq->top_tasks_bitmap[src], top_index);
	}
}

static inline bool is_new_task(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	return wts->active_time < NEW_TASK_ACTIVE_TIME;
}

static void fixup_busy_time(struct task_struct *p, int new_cpu)
{
	struct rq *src_rq = task_rq(p);
	struct rq *dest_rq = cpu_rq(new_cpu);
	u64 wallclock;
	u64 *src_curr_runnable_sum, *dst_curr_runnable_sum;
	u64 *src_prev_runnable_sum, *dst_prev_runnable_sum;
	u64 *src_nt_curr_runnable_sum, *dst_nt_curr_runnable_sum;
	u64 *src_nt_prev_runnable_sum, *dst_nt_prev_runnable_sum;
	bool new_task;
	struct walt_related_thread_group *grp;
	long pstate;
	struct walt_rq *dest_wrq = (struct walt_rq *) dest_rq->android_vendor_data1;
	struct walt_rq *src_wrq = (struct walt_rq *) src_rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (!p->on_rq && p->state != TASK_WAKING)
		return;

	pstate = p->state;

	if (pstate == TASK_WAKING)
		double_rq_lock(src_rq, dest_rq);

	wallclock = walt_ktime_get_ns();

	lockdep_assert_held(&src_rq->lock);
	lockdep_assert_held(&dest_rq->lock);

	if (task_rq(p) != src_rq)
		WALT_BUG(p, "on CPU %d task %s(%d) not on src_rq %d",
				raw_smp_processor_id(), p->comm, p->pid, src_rq->cpu);

	walt_update_task_ravg(task_rq(p)->curr, task_rq(p),
			 TASK_UPDATE,
			 wallclock, 0);
	walt_update_task_ravg(dest_rq->curr, dest_rq,
			 TASK_UPDATE, wallclock, 0);

	walt_update_task_ravg(p, task_rq(p), TASK_MIGRATE,
			 wallclock, 0);

	update_task_cpu_cycles(p, new_cpu, wallclock);

	new_task = is_new_task(p);
	/* Protected by rq_lock */
	grp = wts->grp;

	/*
	 * For frequency aggregation, we continue to do migration fixups
	 * even for intra cluster migrations. This is because, the aggregated
	 * load has to reported on a single CPU regardless.
	 */
	if (grp) {
		struct group_cpu_time *cpu_time;

		cpu_time = &src_wrq->grp_time;
		src_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		src_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		src_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		cpu_time = &dest_wrq->grp_time;
		dst_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		dst_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		dst_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		dst_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		if (wts->curr_window) {
			*src_curr_runnable_sum -= wts->curr_window;
			*dst_curr_runnable_sum += wts->curr_window;
			if (new_task) {
				*src_nt_curr_runnable_sum -= wts->curr_window;
				*dst_nt_curr_runnable_sum += wts->curr_window;
			}
		}

		if (wts->prev_window) {
			*src_prev_runnable_sum -= wts->prev_window;
			*dst_prev_runnable_sum += wts->prev_window;
			if (new_task) {
				*src_nt_prev_runnable_sum -= wts->prev_window;
				*dst_nt_prev_runnable_sum += wts->prev_window;
			}
		}
	} else {
		inter_cluster_migration_fixup(p, new_cpu,
						task_cpu(p), new_task);
	}

	migrate_top_tasks(p, src_rq, dest_rq);

	if (!same_freq_domain(new_cpu, task_cpu(p))) {
		src_wrq->notif_pending = true;
		dest_wrq->notif_pending = true;
		walt_irq_work_queue(&walt_migration_irq_work);
	}

	if (is_ed_enabled()) {
		if (p == src_wrq->ed_task) {
			src_wrq->ed_task = NULL;
			dest_wrq->ed_task = p;
		} else if (is_ed_task(p, wallclock)) {
			dest_wrq->ed_task = p;
		}
	}

	if (pstate == TASK_WAKING)
		double_rq_unlock(src_rq, dest_rq);
}

static void set_window_start(struct rq *rq)
{
	static int sync_cpu_available;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_rq *sync_wrq;
	struct walt_task_struct *wts = (struct walt_task_struct *) rq->curr->android_vendor_data1;

	if (likely(wrq->window_start))
		return;

	if (!sync_cpu_available) {
		wrq->window_start = 1;
		sync_cpu_available = 1;
		atomic64_set(&walt_irq_work_lastq_ws, wrq->window_start);
		walt_load_reported_window =
					atomic64_read(&walt_irq_work_lastq_ws);

	} else {
		struct rq *sync_rq = cpu_rq(cpumask_any(cpu_online_mask));

		sync_wrq = (struct walt_rq *) sync_rq->android_vendor_data1;
		raw_spin_unlock(&rq->lock);
		double_rq_lock(rq, sync_rq);
		wrq->window_start = sync_wrq->window_start;
		wrq->curr_runnable_sum = wrq->prev_runnable_sum = 0;
		wrq->nt_curr_runnable_sum = wrq->nt_prev_runnable_sum = 0;
		raw_spin_unlock(&sync_rq->lock);
	}

	wts->mark_start = wrq->window_start;
}

#define INC_STEP 8
#define DEC_STEP 2
#define CONSISTENT_THRES 16
#define INC_STEP_BIG 16
/*
 * bucket_increase - update the count of all buckets
 *
 * @buckets: array of buckets tracking busy time of a task
 * @idx: the index of bucket to be incremented
 *
 * Each time a complete window finishes, count of bucket that runtime
 * falls in (@idx) is incremented. Counts of all other buckets are
 * decayed. The rate of increase and decay could be different based
 * on current count in the bucket.
 */
static inline void bucket_increase(u8 *buckets, int idx)
{
	int i, step;

	for (i = 0; i < NUM_BUSY_BUCKETS; i++) {
		if (idx != i) {
			if (buckets[i] > DEC_STEP)
				buckets[i] -= DEC_STEP;
			else
				buckets[i] = 0;
		} else {
			step = buckets[i] >= CONSISTENT_THRES ?
						INC_STEP_BIG : INC_STEP;
			if (buckets[i] > U8_MAX - step)
				buckets[i] = U8_MAX;
			else
				buckets[i] += step;
		}
	}
}

static inline int busy_to_bucket(u32 normalized_rt)
{
	int bidx;

	bidx = mult_frac(normalized_rt, NUM_BUSY_BUCKETS, max_task_load());
	bidx = min(bidx, NUM_BUSY_BUCKETS - 1);

	/*
	 * Combine lowest two buckets. The lowest frequency falls into
	 * 2nd bucket and thus keep predicting lowest bucket is not
	 * useful.
	 */
	if (!bidx)
		bidx++;

	return bidx;
}

/*
 * get_pred_busy - calculate predicted demand for a task on runqueue
 *
 * @p: task whose prediction is being updated
 * @start: starting bucket. returned prediction should not be lower than
 *         this bucket.
 * @runtime: runtime of the task. returned prediction should not be lower
 *           than this runtime.
 * Note: @start can be derived from @runtime. It's passed in only to
 * avoid duplicated calculation in some cases.
 *
 * A new predicted busy time is returned for task @p based on @runtime
 * passed in. The function searches through buckets that represent busy
 * time equal to or bigger than @runtime and attempts to find the bucket
 * to use for prediction. Once found, it searches through historical busy
 * time and returns the latest that falls into the bucket. If no such busy
 * time exists, it returns the medium of that bucket.
 */
static u32 get_pred_busy(struct task_struct *p,
				int start, u32 runtime)
{
	int i;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u8 *buckets = wts->busy_buckets;
	u32 *hist = wts->sum_history;
	u32 dmin, dmax;
	u64 cur_freq_runtime = 0;
	int first = NUM_BUSY_BUCKETS, final;
	u32 ret = runtime;

	/* skip prediction for new tasks due to lack of history */
	if (unlikely(is_new_task(p)))
		goto out;

	/* find minimal bucket index to pick */
	for (i = start; i < NUM_BUSY_BUCKETS; i++) {
		if (buckets[i]) {
			first = i;
			break;
		}
	}
	/* if no higher buckets are filled, predict runtime */
	if (first >= NUM_BUSY_BUCKETS)
		goto out;

	/* compute the bucket for prediction */
	final = first;

	/* determine demand range for the predicted bucket */
	if (final < 2) {
		/* lowest two buckets are combined */
		dmin = 0;
		final = 1;
	} else {
		dmin = mult_frac(final, max_task_load(), NUM_BUSY_BUCKETS);
	}
	dmax = mult_frac(final + 1, max_task_load(), NUM_BUSY_BUCKETS);

	/*
	 * search through runtime history and return first runtime that falls
	 * into the range of predicted bucket.
	 */
	for (i = 0; i < sched_ravg_hist_size; i++) {
		if (hist[i] >= dmin && hist[i] < dmax) {
			ret = hist[i];
			break;
		}
	}
	/* no historical runtime within bucket found, use average of the bin */
	if (ret < dmin)
		ret = (dmin + dmax) / 2;
	/*
	 * when updating in middle of a window, runtime could be higher
	 * than all recorded history. Always predict at least runtime.
	 */
	ret = max(runtime, ret);
out:
	trace_sched_update_pred_demand(p, runtime,
		mult_frac((unsigned int)cur_freq_runtime, 100,
			  sched_ravg_window), ret, wts);
	return ret;
}

static inline u32 calc_pred_demand(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (wts->pred_demand >= wts->curr_window)
		return wts->pred_demand;

	return get_pred_busy(p, busy_to_bucket(wts->curr_window),
			     wts->curr_window);
}

/*
 * predictive demand of a task is calculated at the window roll-over.
 * if the task current window busy time exceeds the predicted
 * demand, update it here to reflect the task needs.
 */
static void update_task_pred_demand(struct rq *rq, struct task_struct *p, int event)
{
	u32 new, old;
	u16 new_scaled;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (is_idle_task(p))
		return;

	if (event != PUT_PREV_TASK && event != TASK_UPDATE &&
			(!SCHED_FREQ_ACCOUNT_WAIT_TIME ||
			 (event != TASK_MIGRATE &&
			 event != PICK_NEXT_TASK)))
		return;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (!p->on_rq && !SCHED_FREQ_ACCOUNT_WAIT_TIME)
			return;
	}

	new = calc_pred_demand(p);
	old = wts->pred_demand;

	if (old >= new)
		return;

	new_scaled = scale_demand(new);
	if (task_on_rq_queued(p) && (!task_has_dl_policy(p) ||
				!p->dl.dl_throttled))
		fixup_walt_sched_stats_common(rq, p,
				wts->demand_scaled,
				new_scaled);

	wts->pred_demand = new;
	wts->pred_demand_scaled = new_scaled;
}

static void clear_top_tasks_bitmap(unsigned long *bitmap)
{
	memset(bitmap, 0, top_tasks_bitmap_size);
	__set_bit(NUM_LOAD_INDICES, bitmap);
}

static inline void clear_top_tasks_table(u8 *table)
{
	memset(table, 0, NUM_LOAD_INDICES * sizeof(u8));
}

static void update_top_tasks(struct task_struct *p, struct rq *rq,
		u32 old_curr_window, int new_window, bool full_window)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u8 curr = wrq->curr_table;
	u8 prev = 1 - curr;
	u8 *curr_table = wrq->top_tasks[curr];
	u8 *prev_table = wrq->top_tasks[prev];
	int old_index, new_index, update_index;
	u32 curr_window = wts->curr_window;
	u32 prev_window = wts->prev_window;
	bool zero_index_update;

	if (old_curr_window == curr_window && !new_window)
		return;

	old_index = load_to_index(old_curr_window);
	new_index = load_to_index(curr_window);

	if (!new_window) {
		zero_index_update = !old_curr_window && curr_window;
		if (old_index != new_index || zero_index_update) {
			if (old_curr_window)
				curr_table[old_index] -= 1;
			if (curr_window)
				curr_table[new_index] += 1;
			if (new_index > wrq->curr_top)
				wrq->curr_top = new_index;
		}

		if (!curr_table[old_index])
			__clear_bit(NUM_LOAD_INDICES - old_index - 1,
				wrq->top_tasks_bitmap[curr]);

		if (curr_table[new_index] == 1)
			__set_bit(NUM_LOAD_INDICES - new_index - 1,
				wrq->top_tasks_bitmap[curr]);

		return;
	}

	/*
	 * The window has rolled over for this task. By the time we get
	 * here, curr/prev swaps would has already occurred. So we need
	 * to use prev_window for the new index.
	 */
	update_index = load_to_index(prev_window);

	if (full_window) {
		/*
		 * Two cases here. Either 'p' ran for the entire window or
		 * it didn't run at all. In either case there is no entry
		 * in the prev table. If 'p' ran the entire window, we just
		 * need to create a new entry in the prev table. In this case
		 * update_index will be correspond to sched_ravg_window
		 * so we can unconditionally update the top index.
		 */
		if (prev_window) {
			prev_table[update_index] += 1;
			wrq->prev_top = update_index;
		}

		if (prev_table[update_index] == 1)
			__set_bit(NUM_LOAD_INDICES - update_index - 1,
				wrq->top_tasks_bitmap[prev]);
	} else {
		zero_index_update = !old_curr_window && prev_window;
		if (old_index != update_index || zero_index_update) {
			if (old_curr_window)
				prev_table[old_index] -= 1;

			prev_table[update_index] += 1;

			if (update_index > wrq->prev_top)
				wrq->prev_top = update_index;

			if (!prev_table[old_index])
				__clear_bit(NUM_LOAD_INDICES - old_index - 1,
						wrq->top_tasks_bitmap[prev]);

			if (prev_table[update_index] == 1)
				__set_bit(NUM_LOAD_INDICES - update_index - 1,
						wrq->top_tasks_bitmap[prev]);
		}
	}

	if (curr_window) {
		curr_table[new_index] += 1;

		if (new_index > wrq->curr_top)
			wrq->curr_top = new_index;

		if (curr_table[new_index] == 1)
			__set_bit(NUM_LOAD_INDICES - new_index - 1,
				wrq->top_tasks_bitmap[curr]);
	}
}

static void rollover_top_tasks(struct rq *rq, bool full_window)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u8 curr_table = wrq->curr_table;
	u8 prev_table = 1 - curr_table;
	int curr_top = wrq->curr_top;

	clear_top_tasks_table(wrq->top_tasks[prev_table]);
	clear_top_tasks_bitmap(wrq->top_tasks_bitmap[prev_table]);

	if (full_window) {
		curr_top = 0;
		clear_top_tasks_table(wrq->top_tasks[curr_table]);
		clear_top_tasks_bitmap(wrq->top_tasks_bitmap[curr_table]);
	}

	wrq->curr_table = prev_table;
	wrq->prev_top = curr_top;
	wrq->curr_top = 0;
}

static u32 empty_windows[WALT_NR_CPUS];

static void rollover_task_window(struct task_struct *p, bool full_window)
{
	u32 *curr_cpu_windows = empty_windows;
	u32 curr_window;
	int i;
	struct walt_rq *wrq = (struct walt_rq *) task_rq(p)->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	/* Rollover the sum */
	curr_window = 0;

	if (!full_window) {
		curr_window = wts->curr_window;
		curr_cpu_windows = wts->curr_window_cpu;
	}

	wts->prev_window = curr_window;
	wts->curr_window = 0;

	/* Roll over individual CPU contributions */
	for (i = 0; i < nr_cpu_ids; i++) {
		wts->prev_window_cpu[i] = curr_cpu_windows[i];
		wts->curr_window_cpu[i] = 0;
	}

	if (is_new_task(p))
		wts->active_time += wrq->prev_window_size;
}

static inline int cpu_is_waiting_on_io(struct rq *rq)
{
	if (!sched_io_is_busy)
		return 0;

	return atomic_read(&rq->nr_iowait);
}

static int account_busy_for_cpu_time(struct rq *rq, struct task_struct *p,
				     u64 irqtime, int event)
{
	if (is_idle_task(p)) {
		/* TASK_WAKE && TASK_MIGRATE is not possible on idle task! */
		if (event == PICK_NEXT_TASK)
			return 0;

		/* PUT_PREV_TASK, TASK_UPDATE && IRQ_UPDATE are left */
		return irqtime || cpu_is_waiting_on_io(rq);
	}

	if (event == TASK_WAKE)
		return 0;

	if (event == PUT_PREV_TASK || event == IRQ_UPDATE)
		return 1;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_FREQ_ACCOUNT_WAIT_TIME : 0;
	}

	/* TASK_MIGRATE, PICK_NEXT_TASK left */
	return SCHED_FREQ_ACCOUNT_WAIT_TIME;
}

#define DIV64_U64_ROUNDUP(X, Y) div64_u64((X) + (Y - 1), Y)

static inline u64 scale_exec_time(u64 delta, struct rq *rq)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	return (delta * wrq->task_exec_scale) >> 10;
}

/* Convert busy time to frequency equivalent
 * Assumes load is scaled to 1024
 */
static inline unsigned int load_to_freq(struct rq *rq, unsigned int load)
{
	return mult_frac(cpu_max_possible_freq(cpu_of(rq)), load,
		 (unsigned int)arch_scale_cpu_capacity(cpu_of(rq)));
}

static bool do_pl_notif(struct rq *rq)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 prev = wrq->old_busy_time;
	u64 pl = wrq->walt_stats.pred_demands_sum_scaled;
	int cpu = cpu_of(rq);

	/* If already at max freq, bail out */
	if (capacity_orig_of(cpu) == capacity_curr_of(cpu))
		return false;

	prev = max(prev, wrq->old_estimated_time);

	/* 400 MHz filter. */
	return (pl > prev) && (load_to_freq(rq, pl - prev) > 400000);
}

static void rollover_cpu_window(struct rq *rq, bool full_window)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 curr_sum = wrq->curr_runnable_sum;
	u64 nt_curr_sum = wrq->nt_curr_runnable_sum;
	u64 grp_curr_sum = wrq->grp_time.curr_runnable_sum;
	u64 grp_nt_curr_sum = wrq->grp_time.nt_curr_runnable_sum;

	if (unlikely(full_window)) {
		curr_sum = 0;
		nt_curr_sum = 0;
		grp_curr_sum = 0;
		grp_nt_curr_sum = 0;
	}

	wrq->prev_runnable_sum = curr_sum;
	wrq->nt_prev_runnable_sum = nt_curr_sum;
	wrq->grp_time.prev_runnable_sum = grp_curr_sum;
	wrq->grp_time.nt_prev_runnable_sum = grp_nt_curr_sum;

	wrq->curr_runnable_sum = 0;
	wrq->nt_curr_runnable_sum = 0;
	wrq->grp_time.curr_runnable_sum = 0;
	wrq->grp_time.nt_curr_runnable_sum = 0;
}

/*
 * Account cpu activity in its
 * busy time counters(wrq->curr/prev_runnable_sum)
 */
static void update_cpu_busy_time(struct task_struct *p, struct rq *rq,
				 int event, u64 wallclock, u64 irqtime)
{
	int new_window, full_window = 0;
	int p_is_curr_task = (p == rq->curr);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u64 mark_start = wts->mark_start;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 window_start = wrq->window_start;
	u32 window_size = wrq->prev_window_size;
	u64 delta;
	u64 *curr_runnable_sum = &wrq->curr_runnable_sum;
	u64 *prev_runnable_sum = &wrq->prev_runnable_sum;
	u64 *nt_curr_runnable_sum = &wrq->nt_curr_runnable_sum;
	u64 *nt_prev_runnable_sum = &wrq->nt_prev_runnable_sum;
	bool new_task;
	struct walt_related_thread_group *grp;
	int cpu = rq->cpu;
	u32 old_curr_window = wts->curr_window;

	new_window = mark_start < window_start;
	if (new_window)
		full_window = (window_start - mark_start) >= window_size;

	/*
	 * Handle per-task window rollover. We don't care about the
	 * idle task.
	 */
	if (!is_idle_task(p)) {
		if (new_window)
			rollover_task_window(p, full_window);
	}

	new_task = is_new_task(p);

	if (p_is_curr_task && new_window) {
		rollover_cpu_window(rq, full_window);
		rollover_top_tasks(rq, full_window);
	}

	if (!account_busy_for_cpu_time(rq, p, irqtime, event))
		goto done;

	grp = wts->grp;
	if (grp) {
		struct group_cpu_time *cpu_time = &wrq->grp_time;

		curr_runnable_sum = &cpu_time->curr_runnable_sum;
		prev_runnable_sum = &cpu_time->prev_runnable_sum;

		nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;
	}

	if (!new_window) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. No rollover
		 * since we didn't start a new window. An example of this is
		 * when a task starts execution and then sleeps within the
		 * same window.
		 */

		if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq))
			delta = wallclock - mark_start;
		else
			delta = irqtime;
		delta = scale_exec_time(delta, rq);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!is_idle_task(p)) {
			wts->curr_window += delta;
			wts->curr_window_cpu[cpu] += delta;
		}

		goto done;
	}

	if (!p_is_curr_task) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has also started, but p is not the current task, so the
		 * window is not rolled over - just split up and account
		 * as necessary into curr and prev. The window is only
		 * rolled over when a new window is processed for the current
		 * task.
		 *
		 * Irqtime can't be accounted by a task that isn't the
		 * currently running task.
		 */

		if (!full_window) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			wts->prev_window += delta;
			wts->prev_window_cpu[cpu] += delta;
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			wts->prev_window = delta;
			wts->prev_window_cpu[cpu] = delta;
		}

		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		wts->curr_window = delta;
		wts->curr_window_cpu[cpu] = delta;

		goto done;
	}

	if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq)) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. If any of these three above conditions are true
		 * then this busy time can't be accounted as irqtime.
		 *
		 * Busy time for the idle task need not be accounted.
		 *
		 * An example of this would be a task that starts execution
		 * and then sleeps once a new window has begun.
		 */

		if (!full_window) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!is_idle_task(p)) {
				wts->prev_window += delta;
				wts->prev_window_cpu[cpu] += delta;
			}
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			if (!is_idle_task(p)) {
				wts->prev_window = delta;
				wts->prev_window_cpu[cpu] = delta;
			}
		}

		/*
		 * Rollover is done here by overwriting the values in
		 * prev_runnable_sum and curr_runnable_sum.
		 */
		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!is_idle_task(p)) {
			wts->curr_window = delta;
			wts->curr_window_cpu[cpu] = delta;
		}

		goto done;
	}

	if (irqtime) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. The current task must be the idle task because
		 * irqtime is not accounted for any other task.
		 *
		 * Irqtime will be accounted each time we process IRQ activity
		 * after a period of idleness, so we know the IRQ busy time
		 * started at wallclock - irqtime.
		 */

		WALT_PANIC(!is_idle_task(p));
		mark_start = wallclock - irqtime;

		/*
		 * Roll window over. If IRQ busy time was just in the current
		 * window then that is all that need be accounted.
		 */
		if (mark_start > window_start) {
			*curr_runnable_sum = scale_exec_time(irqtime, rq);
			return;
		}

		/*
		 * The IRQ busy time spanned multiple windows. Process the
		 * busy time preceding the current window start first.
		 */
		delta = window_start - mark_start;
		if (delta > window_size)
			delta = window_size;
		delta = scale_exec_time(delta, rq);
		*prev_runnable_sum += delta;

		/* Process the remaining IRQ busy time in the current window. */
		delta = wallclock - window_start;
		wrq->curr_runnable_sum = scale_exec_time(delta, rq);

		return;
	}

done:
	if (!is_idle_task(p))
		update_top_tasks(p, rq, old_curr_window,
					new_window, full_window);
}

static inline u32 predict_and_update_buckets(
			struct task_struct *p, u32 runtime) {
	int bidx;
	u32 pred_demand;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	bidx = busy_to_bucket(runtime);
	pred_demand = get_pred_busy(p, bidx, runtime);
	bucket_increase(wts->busy_buckets, bidx);

	return pred_demand;
}

static int
account_busy_for_task_demand(struct rq *rq, struct task_struct *p, int event)
{
	/*
	 * No need to bother updating task demand for the idle task.
	 */
	if (is_idle_task(p))
		return 0;

	/*
	 * When a task is waking up it is completing a segment of non-busy
	 * time. Likewise, if wait time is not treated as busy time, then
	 * when a task begins to run or is migrated, it is not running and
	 * is completing a segment of non-busy time.
	 */
	if (event == TASK_WAKE || (!SCHED_ACCOUNT_WAIT_TIME &&
			 (event == PICK_NEXT_TASK || event == TASK_MIGRATE)))
		return 0;

	/*
	 * The idle exit time is not accounted for the first task _picked_ up to
	 * run on the idle CPU.
	 */
	if (event == PICK_NEXT_TASK && rq->curr == rq->idle)
		return 0;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_ACCOUNT_WAIT_TIME : 0;
	}

	return 1;
}

/*
 * Called when new window is starting for a task, to record cpu usage over
 * recently concluded window(s). Normally 'samples' should be 1. It can be > 1
 * when, say, a real-time task runs without preemption for several windows at a
 * stretch.
 */
static void update_history(struct rq *rq, struct task_struct *p,
			 u32 runtime, int samples, int event)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u32 *hist = &wts->sum_history[0];
	int ridx, widx;
	u32 max = 0, avg, demand, pred_demand;
	u64 sum = 0;
	u16 demand_scaled, pred_demand_scaled;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	/* Ignore windows where task had no activity */
	if (!runtime || is_idle_task(p) || !samples)
		goto done;

	/* Push new 'runtime' value onto stack */
	widx = sched_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		hist[widx] = hist[ridx];
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	for (widx = 0; widx < samples && widx < sched_ravg_hist_size; widx++) {
		hist[widx] = runtime;
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	wts->sum = 0;

	if (sysctl_sched_window_stats_policy == WINDOW_STATS_RECENT) {
		demand = runtime;
	} else if (sysctl_sched_window_stats_policy == WINDOW_STATS_MAX) {
		demand = max;
	} else {
		avg = div64_u64(sum, sched_ravg_hist_size);
		if (sysctl_sched_window_stats_policy == WINDOW_STATS_AVG)
			demand = avg;
		else
			demand = max(avg, runtime);
	}
	pred_demand = predict_and_update_buckets(p, runtime);
	demand_scaled = scale_demand(demand);
	pred_demand_scaled = scale_demand(pred_demand);

	/*
	 * A throttled deadline sched class task gets dequeued without
	 * changing p->on_rq. Since the dequeue decrements walt stats
	 * avoid decrementing it here again.
	 *
	 * When window is rolled over, the cumulative window demand
	 * is reset to the cumulative runnable average (contribution from
	 * the tasks on the runqueue). If the current task is dequeued
	 * already, it's demand is not included in the cumulative runnable
	 * average. So add the task demand separately to cumulative window
	 * demand.
	 */
	if (!task_has_dl_policy(p) || !p->dl.dl_throttled) {
		if (task_on_rq_queued(p))
			fixup_walt_sched_stats_common(rq, p,
					demand_scaled, pred_demand_scaled);
	}

	wts->demand = demand;
	wts->demand_scaled = demand_scaled;
	wts->coloc_demand = div64_u64(sum, sched_ravg_hist_size);
	wts->pred_demand = pred_demand;
	wts->pred_demand_scaled = pred_demand_scaled;

	if (demand_scaled > sysctl_sched_min_task_util_for_colocation)
		wts->unfilter = sysctl_sched_task_unfilter_period;
	else
		if (wts->unfilter)
			wts->unfilter = max_t(int, 0,
				wts->unfilter - wrq->prev_window_size);

done:
	trace_sched_update_history(rq, p, runtime, samples, event, wrq, wts);
}

static u64 add_to_task_demand(struct rq *rq, struct task_struct *p, u64 delta)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	delta = scale_exec_time(delta, rq);
	wts->sum += delta;
	if (unlikely(wts->sum > sched_ravg_window))
		wts->sum = sched_ravg_window;

	return delta;
}

/*
 * Account cpu demand of task and/or update task's cpu demand history
 *
 * ms = wts->mark_start;
 * wc = wallclock
 * ws = wrq->window_start
 *
 * Three possibilities:
 *
 *	a) Task event is contained within one window.
 *		window_start < mark_start < wallclock
 *
 *		ws   ms  wc
 *		|    |   |
 *		V    V   V
 *		|---------------|
 *
 *	In this case, wts->sum is updated *iff* event is appropriate
 *	(ex: event == PUT_PREV_TASK)
 *
 *	b) Task event spans two windows.
 *		mark_start < window_start < wallclock
 *
 *		ms   ws   wc
 *		|    |    |
 *		V    V    V
 *		-----|-------------------
 *
 *	In this case, wts->sum is updated with (ws - ms) *iff* event
 *	is appropriate, then a new window sample is recorded followed
 *	by wts->sum being set to (wc - ws) *iff* event is appropriate.
 *
 *	c) Task event spans more than two windows.
 *
 *		ms ws_tmp			   ws  wc
 *		|  |				   |   |
 *		V  V				   V   V
 *		---|-------|-------|-------|-------|------
 *		   |				   |
 *		   |<------ nr_full_windows ------>|
 *
 *	In this case, wts->sum is updated with (ws_tmp - ms) first *iff*
 *	event is appropriate, window sample of wts->sum is recorded,
 *	'nr_full_window' samples of window_size is also recorded *iff*
 *	event is appropriate and finally wts->sum is set to (wc - ws)
 *	*iff* event is appropriate.
 *
 * IMPORTANT : Leave wts->mark_start unchanged, as update_cpu_busy_time()
 * depends on it!
 */
static u64 update_task_demand(struct task_struct *p, struct rq *rq,
			       int event, u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	u64 mark_start = wts->mark_start;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 delta, window_start = wrq->window_start;
	int new_window, nr_full_windows;
	u32 window_size = sched_ravg_window;
	u64 runtime;

	new_window = mark_start < window_start;
	if (!account_busy_for_task_demand(rq, p, event)) {
		if (new_window)
			/*
			 * If the time accounted isn't being accounted as
			 * busy time, and a new window started, only the
			 * previous window need be closed out with the
			 * pre-existing demand. Multiple windows may have
			 * elapsed, but since empty windows are dropped,
			 * it is not necessary to account those.
			 */
			update_history(rq, p, wts->sum, 1, event);
		return 0;
	}

	if (!new_window) {
		/*
		 * The simple case - busy time contained within the existing
		 * window.
		 */
		return add_to_task_demand(rq, p, wallclock - mark_start);
	}

	/*
	 * Busy time spans at least two windows. Temporarily rewind
	 * window_start to first window boundary after mark_start.
	 */
	delta = window_start - mark_start;
	nr_full_windows = div64_u64(delta, window_size);
	window_start -= (u64)nr_full_windows * (u64)window_size;

	/* Process (window_start - mark_start) first */
	runtime = add_to_task_demand(rq, p, window_start - mark_start);

	/* Push new sample(s) into task's demand history */
	update_history(rq, p, wts->sum, 1, event);
	if (nr_full_windows) {
		u64 scaled_window = scale_exec_time(window_size, rq);

		update_history(rq, p, scaled_window, nr_full_windows, event);
		runtime += nr_full_windows * scaled_window;
	}

	/*
	 * Roll window_start back to current to process any remainder
	 * in current window.
	 */
	window_start += (u64)nr_full_windows * (u64)window_size;

	/* Process (wallclock - window_start) next */
	mark_start = window_start;
	runtime += add_to_task_demand(rq, p, wallclock - mark_start);

	return runtime;
}

static inline unsigned int cpu_cur_freq(int cpu)
{
	struct walt_rq *wrq = (struct walt_rq *) cpu_rq(cpu)->android_vendor_data1;

	return wrq->cluster->cur_freq;
}

static void
update_task_rq_cpu_cycles(struct task_struct *p, struct rq *rq, int event,
			  u64 wallclock, u64 irqtime)
{
	u64 cur_cycles;
	u64 cycles_delta;
	u64 time_delta;
	int cpu = cpu_of(rq);
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	lockdep_assert_held(&rq->lock);

	if (!use_cycle_counter) {
		wrq->task_exec_scale = DIV64_U64_ROUNDUP(cpu_cur_freq(cpu) *
				arch_scale_cpu_capacity(cpu),
				wrq->cluster->max_possible_freq);
		return;
	}

	cur_cycles = read_cycle_counter(cpu, wallclock);

	/*
	 * If current task is idle task and irqtime == 0 CPU was
	 * indeed idle and probably its cycle counter was not
	 * increasing.  We still need estimatied CPU frequency
	 * for IO wait time accounting.  Use the previously
	 * calculated frequency in such a case.
	 */
	if (!is_idle_task(rq->curr) || irqtime) {
		if (unlikely(cur_cycles < wts->cpu_cycles))
			cycles_delta = cur_cycles + (U64_MAX -
				wts->cpu_cycles);
		else
			cycles_delta = cur_cycles - wts->cpu_cycles;
		cycles_delta = cycles_delta * NSEC_PER_MSEC;

		if (event == IRQ_UPDATE && is_idle_task(p))
			/*
			 * Time between mark_start of idle task and IRQ handler
			 * entry time is CPU cycle counter stall period.
			 * Upon IRQ handler entry walt_sched_account_irqstart()
			 * replenishes idle task's cpu cycle counter so
			 * cycles_delta now represents increased cycles during
			 * IRQ handler rather than time between idle entry and
			 * IRQ exit.  Thus use irqtime as time delta.
			 */
			time_delta = irqtime;
		else
			time_delta = wallclock - wts->mark_start;

		if ((s64)time_delta < 0) {
			printk_deferred("WALT-BUG pid=%u CPU%d wallclock=%llu(0x%llx) < mark_start=%llu(0x%llx) event=%d irqtime=%llu",
					 p->pid, rq->cpu, wallclock, wallclock,
					 wts->mark_start, wts->mark_start, event, irqtime);
			WALT_PANIC((s64)time_delta < 0);
		}

		wrq->task_exec_scale = DIV64_U64_ROUNDUP(cycles_delta *
				arch_scale_cpu_capacity(cpu),
				time_delta *
					wrq->cluster->max_possible_freq);

		trace_sched_get_task_cpu_cycles(cpu, event,
				cycles_delta, time_delta, p);
	}

	wts->cpu_cycles = cur_cycles;
}

static inline void run_walt_irq_work(u64 old_window_start, struct rq *rq)
{
	u64 result;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	if (old_window_start == wrq->window_start)
		return;

	result = atomic64_cmpxchg(&walt_irq_work_lastq_ws, old_window_start,
				   wrq->window_start);
	if (result == old_window_start) {
		walt_irq_work_queue(&walt_cpufreq_irq_work);
		trace_walt_window_rollover(wrq->window_start);
	}
}

/* Reflect task activity on its demand and cpu's busy time statistics */
static void walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime)
{
	u64 old_window_start;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (!wrq->window_start || wts->mark_start == wallclock)
		return;

	lockdep_assert_held(&rq->lock);

	old_window_start = update_window_start(rq, wallclock, event);

	if (!wts->mark_start) {
		update_task_cpu_cycles(p, cpu_of(rq), wallclock);
		goto done;
	}

	update_task_rq_cpu_cycles(p, rq, event, wallclock, irqtime);
	update_task_demand(p, rq, event, wallclock);
	update_cpu_busy_time(p, rq, event, wallclock, irqtime);
	update_task_pred_demand(rq, p, event);
	if (event == PUT_PREV_TASK && p->state)
		wts->iowaited = p->in_iowait;

	trace_sched_update_task_ravg(p, rq, event, wallclock, irqtime,
				&wrq->grp_time, wrq, wts);
	trace_sched_update_task_ravg_mini(p, rq, event, wallclock, irqtime,
				&wrq->grp_time, wrq, wts);

done:
	wts->mark_start = wallclock;

	run_walt_irq_work(old_window_start, rq);
}

static inline void __sched_fork_init(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wts->last_sleep_ts	= 0;
	wts->wake_up_idle	= false;
	wts->boost		= 0;
	wts->boost_expires	= 0;
	wts->boost_period	= false;
	wts->low_latency	= false;
	wts->iowaited		= false;
}

static void init_new_task_load(struct task_struct *p)
{
	int i;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_task_struct *cur_wts =
		(struct walt_task_struct *) current->android_vendor_data1;
	u32 init_load_windows = sched_init_task_load_windows;
	u32 init_load_windows_scaled = sched_init_task_load_windows_scaled;
	u32 init_load_pct = cur_wts->init_load_pct;

	wts->init_load_pct = 0;
	rcu_assign_pointer(wts->grp, NULL);
	INIT_LIST_HEAD(&wts->grp_list);

	wts->mark_start = 0;
	wts->sum = 0;
	wts->curr_window = 0;
	wts->prev_window = 0;
	wts->active_time = 0;
	wts->prev_on_rq = 0;
	wts->prev_on_rq_cpu = -1;

	for (i = 0; i < NUM_BUSY_BUCKETS; ++i)
		wts->busy_buckets[i] = 0;

	wts->cpu_cycles = 0;

	memset(wts->curr_window_cpu, 0, sizeof(u32) * WALT_NR_CPUS);
	memset(wts->prev_window_cpu, 0, sizeof(u32) * WALT_NR_CPUS);

	if (init_load_pct) {
		init_load_windows = div64_u64((u64)init_load_pct *
			  (u64)sched_ravg_window, 100);
		init_load_windows_scaled = scale_demand(init_load_windows);
	}

	wts->demand = init_load_windows;
	wts->demand_scaled = init_load_windows_scaled;
	wts->coloc_demand = init_load_windows;
	wts->pred_demand = 0;
	wts->pred_demand_scaled = 0;
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		wts->sum_history[i] = init_load_windows;
	wts->misfit = false;
	wts->rtg_high_prio = false;
	wts->unfilter = sysctl_sched_task_unfilter_period;

	INIT_LIST_HEAD(&wts->mvp_list);
	wts->sum_exec_snapshot = 0;
	wts->total_exec = 0;
	wts->mvp_prio = WALT_NOT_MVP;
	__sched_fork_init(p);
}

static void init_existing_task_load(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	init_new_task_load(p);
	cpumask_copy(&wts->cpus_requested, &p->cpus_mask);
}

static void walt_task_dead(struct task_struct *p)
{
	sched_set_group_id(p, 0);
}

static void mark_task_starting(struct task_struct *p)
{
	u64 wallclock;
	struct rq *rq = task_rq(p);
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wallclock = walt_ktime_get_ns();
	wts->mark_start = wts->last_wake_ts = wallclock;
	wts->last_enqueued_ts = wallclock;
	update_task_cpu_cycles(p, cpu_of(rq), wallclock);
}

/*
 * Task groups whose aggregate demand on a cpu is more than
 * sched_group_upmigrate need to be up-migrated if possible.
 */
static unsigned int __read_mostly sched_group_upmigrate = 20000000;

/*
 * Task groups, once up-migrated, will need to drop their aggregate
 * demand to less than sched_group_downmigrate before they are "down"
 * migrated.
 */
static unsigned int __read_mostly sched_group_downmigrate = 19000000;

void walt_update_group_thresholds(void)
{
	unsigned int min_scale = arch_scale_cpu_capacity(
				cluster_first_cpu(sched_cluster[0]));
	u64 min_ms = min_scale * (sched_ravg_window >> SCHED_CAPACITY_SHIFT);

	sched_group_upmigrate = div64_ul(min_ms *
				sysctl_sched_group_upmigrate_pct, 100);
	sched_group_downmigrate = div64_ul(min_ms *
				sysctl_sched_group_downmigrate_pct, 100);
}

struct walt_sched_cluster *sched_cluster[WALT_NR_CPUS];
__read_mostly int num_sched_clusters;

struct list_head cluster_head;

static struct walt_sched_cluster init_cluster = {
	.list			= LIST_HEAD_INIT(init_cluster.list),
	.id			= 0,
	.cur_freq		= 1,
	.max_possible_freq	= 1,
	.aggr_grp_load		= 0,
};

static void init_clusters(void)
{
	init_cluster.cpus = *cpu_possible_mask;
	raw_spin_lock_init(&init_cluster.load_lock);
	INIT_LIST_HEAD(&cluster_head);
	list_add(&init_cluster.list, &cluster_head);
}

static void
insert_cluster(struct walt_sched_cluster *cluster, struct list_head *head)
{
	struct walt_sched_cluster *tmp;
	struct list_head *iter = head;

	list_for_each_entry(tmp, head, list) {
		if (arch_scale_cpu_capacity(cluster_first_cpu(cluster))
			< arch_scale_cpu_capacity(cluster_first_cpu(tmp)))
			break;
		iter = &tmp->list;
	}

	list_add(&cluster->list, iter);
}

static struct walt_sched_cluster *alloc_new_cluster(const struct cpumask *cpus)
{
	struct walt_sched_cluster *cluster = NULL;

	cluster = kzalloc(sizeof(struct walt_sched_cluster), GFP_ATOMIC);
	BUG_ON(!cluster);

	INIT_LIST_HEAD(&cluster->list);
	cluster->cur_freq		=	1;
	cluster->max_freq		=	1;
	cluster->max_possible_freq	=	1;

	raw_spin_lock_init(&cluster->load_lock);
	cluster->cpus = *cpus;

	return cluster;
}

static void add_cluster(const struct cpumask *cpus, struct list_head *head)
{
	struct walt_sched_cluster *cluster = alloc_new_cluster(cpus);
	int i;
	struct walt_rq *wrq;

	for_each_cpu(i, cpus) {
		wrq = (struct walt_rq *) cpu_rq(i)->android_vendor_data1;
		wrq->cluster = cluster;
	}

	insert_cluster(cluster, head);
	num_sched_clusters++;
}

static void cleanup_clusters(struct list_head *head)
{
	struct walt_sched_cluster *cluster, *tmp;
	int i;
	struct walt_rq *wrq;

	list_for_each_entry_safe(cluster, tmp, head, list) {
		for_each_cpu(i, &cluster->cpus) {
			wrq = (struct walt_rq *) cpu_rq(i)->android_vendor_data1;
			wrq->cluster = &init_cluster;
		}
		list_del(&cluster->list);
		num_sched_clusters--;
		kfree(cluster);
	}
}

static inline void assign_cluster_ids(struct list_head *head)
{
	struct walt_sched_cluster *cluster;
	int pos = 0;

	list_for_each_entry(cluster, head, list) {
		cluster->id = pos;
		sched_cluster[pos++] = cluster;
	}

	WARN_ON(pos > MAX_NR_CLUSTERS);
}

static inline void
move_list(struct list_head *dst, struct list_head *src, bool sync_rcu)
{
	struct list_head *first, *last;

	first = src->next;
	last = src->prev;

	if (sync_rcu) {
		INIT_LIST_HEAD_RCU(src);
		synchronize_rcu();
	}

	first->prev = dst;
	dst->prev = last;
	last->next = dst;

	/* Ensure list sanity before making the head visible to all CPUs. */
	smp_mb();
	dst->next = first;
}

static void update_all_clusters_stats(void)
{
	struct walt_sched_cluster *cluster;
	u64 highest_mpc = 0, lowest_mpc = U64_MAX;

	for_each_sched_cluster(cluster) {
		u64 mpc = arch_scale_cpu_capacity(
				cluster_first_cpu(cluster));

		if (mpc > highest_mpc)
			highest_mpc = mpc;

		if (mpc < lowest_mpc)
			lowest_mpc = mpc;
	}

	max_possible_capacity = highest_mpc;
	min_max_possible_capacity = lowest_mpc;
	walt_update_group_thresholds();
}

static bool walt_clusters_parsed;
cpumask_t __read_mostly **cpu_array;

static void init_cpu_array(void)
{
	int i;

	cpu_array = kcalloc(num_sched_clusters, sizeof(cpumask_t *),
			GFP_ATOMIC | __GFP_NOFAIL);
	if (!cpu_array)
		WALT_PANIC(1);

	for (i = 0; i < num_sched_clusters; i++) {
		cpu_array[i] = kcalloc(num_sched_clusters, sizeof(cpumask_t),
			GFP_ATOMIC | __GFP_NOFAIL);
		if (!cpu_array[i])
			WALT_PANIC(1);
	}
}

static void build_cpu_array(void)
{
	int i;

	if (!cpu_array)
		WALT_PANIC(1);
	/* Construct cpu_array row by row */
	for (i = 0; i < num_sched_clusters; i++) {
		int j, k = 1;

		/* Fill out first column with appropriate cpu arrays */
		cpumask_copy(&cpu_array[i][0], &sched_cluster[i]->cpus);
		/*
		 * k starts from column 1 because 0 is filled
		 * Fill clusters for the rest of the row,
		 * above i in ascending order
		 */
		for (j = i + 1; j < num_sched_clusters; j++) {
			cpumask_copy(&cpu_array[i][k],
					&sched_cluster[j]->cpus);
			k++;
		}

		/*
		 * k starts from where we left off above.
		 * Fill clusters below i in descending order.
		 */
		for (j = i - 1; j >= 0; j--) {
			cpumask_copy(&cpu_array[i][k],
					&sched_cluster[j]->cpus);
			k++;
		}
	}
}

static void walt_get_possible_siblings(int cpuid, struct cpumask *cluster_cpus)
{
	int cpu;
	struct cpu_topology *cpu_topo, *cpuid_topo = &cpu_topology[cpuid];

	if (cpuid_topo->package_id == -1)
		return;

	for_each_possible_cpu(cpu) {
		cpu_topo = &cpu_topology[cpu];

		if (cpuid_topo->package_id != cpu_topo->package_id)
			continue;
		cpumask_set_cpu(cpu, cluster_cpus);
	}
}

int cpu_l2_sibling[WALT_NR_CPUS] = {[0 ... WALT_NR_CPUS-1] = -1};
static void find_cache_siblings(void)
{
	int cpu, cpu2;
	struct device_node *cpu_dev, *cpu_dev2, *cpu_l2_cache_node, *cpu_l2_cache_node2;

	for_each_possible_cpu(cpu) {
		cpu_dev = of_get_cpu_node(cpu, NULL);
		if (!cpu_dev)
			continue;

		cpu_l2_cache_node = of_parse_phandle(cpu_dev, "next-level-cache", 0);
		if (!cpu_l2_cache_node)
			continue;

		for_each_possible_cpu(cpu2) {
			if (cpu == cpu2)
				continue;

			cpu_dev2 = of_get_cpu_node(cpu2, NULL);
			if (!cpu_dev2)
				continue;

			cpu_l2_cache_node2 = of_parse_phandle(cpu_dev2, "next-level-cache", 0);
			if (!cpu_l2_cache_node2)
				continue;

			if (cpu_l2_cache_node == cpu_l2_cache_node2) {
				cpu_l2_sibling[cpu] = cpu2;
				break;
			}
		}
	}
}

static void walt_update_cluster_topology(void)
{
	struct cpumask cpus = *cpu_possible_mask;
	struct cpumask cluster_cpus;
	struct walt_sched_cluster *cluster;
	struct list_head new_head;
	int i;
	struct walt_rq *wrq;

	INIT_LIST_HEAD(&new_head);

	for_each_cpu(i, &cpus) {
		cpumask_clear(&cluster_cpus);
		walt_get_possible_siblings(i, &cluster_cpus);
		if (cpumask_empty(&cluster_cpus)) {
			WARN(1, "WALT: Invalid cpu topology!!");
			cleanup_clusters(&new_head);
			return;
		}
		cpumask_andnot(&cpus, &cpus, &cluster_cpus);
		add_cluster(&cluster_cpus, &new_head);
	}

	assign_cluster_ids(&new_head);

	list_for_each_entry(cluster, &new_head, list) {
		struct cpufreq_policy *policy;

		policy = cpufreq_cpu_get_raw(cluster_first_cpu(cluster));
		/*
		 * walt_update_cluster_topology() must be called AFTER policies
		 * for all cpus are initialized. If not, simply BUG().
		 */
		WALT_PANIC(!policy);

		if (policy) {
			cluster->max_possible_freq = policy->cpuinfo.max_freq;
			cluster->max_freq = policy->max;
			for_each_cpu(i, &cluster->cpus) {
				wrq = (struct walt_rq *) cpu_rq(i)->android_vendor_data1;
				cpumask_copy(&wrq->freq_domain_cpumask,
					     policy->related_cpus);
			}
			cpuinfo_max_freq_cached = (cpuinfo_max_freq_cached >
			policy->cpuinfo.max_freq) ?: policy->cpuinfo.max_freq;
		}
	}

	/*
	 * Ensure cluster ids are visible to all CPUs before making
	 * cluster_head visible.
	 */
	move_list(&cluster_head, &new_head, false);
	update_all_clusters_stats();
	cluster = NULL;

	for_each_sched_cluster(cluster) {
		if (cpumask_weight(&cluster->cpus) == 1)
			cpumask_or(&asym_cap_sibling_cpus,
				   &asym_cap_sibling_cpus, &cluster->cpus);
	}

	if (cpumask_weight(&asym_cap_sibling_cpus) == 1)
		cpumask_clear(&asym_cap_sibling_cpus);

	init_cpu_array();
	build_cpu_array();
	find_cache_siblings();

	create_util_to_cost();
	walt_clusters_parsed = true;
}

static int cpufreq_notifier_trans(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = (struct cpufreq_freqs *)data;
	unsigned int cpu = freq->policy->cpu, new_freq = freq->new;
	unsigned long flags;
	struct walt_sched_cluster *cluster;
	struct walt_rq *wrq = (struct walt_rq *) cpu_rq(cpu)->android_vendor_data1;
	struct cpumask policy_cpus = wrq->freq_domain_cpumask;
	int i, j;

	if (use_cycle_counter)
		return NOTIFY_DONE;
	wrq = (struct walt_rq *) cpu_rq(cpumask_first(&policy_cpus))->android_vendor_data1;
	if (wrq->cluster == &init_cluster)
		return NOTIFY_DONE;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_DONE;

	if (cpu_cur_freq(cpu) == new_freq)
		return NOTIFY_OK;

	for_each_cpu(i, &policy_cpus) {
		wrq = (struct walt_rq *) cpu_rq(i)->android_vendor_data1;
		cluster = wrq->cluster;

		for_each_cpu(j, &cluster->cpus) {
			struct rq *rq = cpu_rq(j);

			raw_spin_lock_irqsave(&rq->lock, flags);
			walt_update_task_ravg(rq->curr, rq, TASK_UPDATE,
					 walt_ktime_get_ns(), 0);
			raw_spin_unlock_irqrestore(&rq->lock, flags);
		}

		cluster->cur_freq = new_freq;
		cpumask_andnot(&policy_cpus, &policy_cpus, &cluster->cpus);
	}

	return NOTIFY_OK;
}

static struct notifier_block notifier_trans_block = {
	.notifier_call = cpufreq_notifier_trans
};

static void walt_init_cycle_counter(void)
{
	if (qcom_cpufreq_get_cpu_cycle_counter(smp_processor_id()) != U64_MAX) {
		use_cycle_counter = true;
		return;
	}

	cpufreq_register_notifier(&notifier_trans_block,
				  CPUFREQ_TRANSITION_NOTIFIER);
}

static void transfer_busy_time(struct rq *rq,
				struct walt_related_thread_group *grp,
					struct task_struct *p, int event);

/*
 * Enable colocation and frequency aggregation for all threads in a process.
 * The children inherits the group id from the parent.
 */

static struct walt_related_thread_group
			*related_thread_groups[MAX_NUM_CGROUP_COLOC_ID];
static LIST_HEAD(active_related_thread_groups);
static DEFINE_RWLOCK(related_thread_group_lock);

static inline
void update_best_cluster(struct walt_related_thread_group *grp,
				   u64 combined_demand, bool boost)
{
	if (boost) {
		/*
		 * since we are in boost, we can keep grp on min, the boosts
		 * will ensure tasks get to bigs
		 */
		grp->skip_min = false;
		return;
	}

	if (is_suh_max())
		combined_demand = sched_group_upmigrate;

	if (!grp->skip_min) {
		if (combined_demand >= sched_group_upmigrate)
			grp->skip_min = true;
		return;
	}
	if (combined_demand < sched_group_downmigrate) {
		if (!sysctl_sched_coloc_downmigrate_ns ||
				(grp->last_update - grp->start_ktime_ts) <
				sysctl_sched_hyst_min_coloc_ns) {
			grp->downmigrate_ts = 0;
			grp->skip_min = false;
			return;
		}
		if (!grp->downmigrate_ts) {
			grp->downmigrate_ts = grp->last_update;
			return;
		}
		if (grp->last_update - grp->downmigrate_ts >
				sysctl_sched_coloc_downmigrate_ns) {
			grp->downmigrate_ts = 0;
			grp->skip_min = false;
		}
	} else if (grp->downmigrate_ts)
		grp->downmigrate_ts = 0;
}

static void _set_preferred_cluster(struct walt_related_thread_group *grp)
{
	struct task_struct *p;
	u64 combined_demand = 0;
	bool group_boost = false;
	u64 wallclock;
	bool prev_skip_min = grp->skip_min;
	struct walt_task_struct *wts;

	if (list_empty(&grp->tasks)) {
		grp->skip_min = false;
		goto out;
	}

	if (!hmp_capable()) {
		grp->skip_min = false;
		goto out;
	}

	wallclock = walt_ktime_get_ns();

	/*
	 * wakeup of two or more related tasks could race with each other and
	 * could result in multiple calls to _set_preferred_cluster being issued
	 * at same time. Avoid overhead in such cases of rechecking preferred
	 * cluster
	 */
	if (wallclock - grp->last_update < sched_ravg_window / 10)
		return;

	list_for_each_entry(wts, &grp->tasks, grp_list) {
		p = wts_to_ts(wts);
		if (task_boost_policy(p) == SCHED_BOOST_ON_BIG) {
			group_boost = true;
			break;
		}

		if (wts->mark_start < wallclock -
		    (sched_ravg_window * sched_ravg_hist_size))
			continue;

		combined_demand += wts->coloc_demand;
		if (!trace_sched_set_preferred_cluster_enabled()) {
			if (combined_demand > sched_group_upmigrate)
				break;
		}
	}

	grp->last_update = wallclock;
	update_best_cluster(grp, combined_demand, group_boost);

out:
	trace_sched_set_preferred_cluster(grp, combined_demand, prev_skip_min);
	if (grp->id == DEFAULT_CGROUP_COLOC_ID
			&& grp->skip_min != prev_skip_min) {
		if (grp->skip_min)
			grp->start_ktime_ts = wallclock;
		else
			grp->start_ktime_ts = 0;
		sched_update_hyst_times();
	}
}

static void set_preferred_cluster(struct walt_related_thread_group *grp)
{
	raw_spin_lock(&grp->lock);
	_set_preferred_cluster(grp);
	raw_spin_unlock(&grp->lock);
}

static int update_preferred_cluster(struct walt_related_thread_group *grp,
		struct task_struct *p, u32 old_load, bool from_tick)
{
	u32 new_load = task_load(p);

	if (!grp)
		return 0;

	if (unlikely(from_tick && is_suh_max()))
		return 1;

	/*
	 * Update if task's load has changed significantly or a complete window
	 * has passed since we last updated preference
	 */
	if (abs(new_load - old_load) > sched_ravg_window / 4 ||
		walt_ktime_get_ns() - grp->last_update > sched_ravg_window)
		return 1;

	return 0;
}

#define ADD_TASK	0
#define REM_TASK	1

static inline struct walt_related_thread_group*
lookup_related_thread_group(unsigned int group_id)
{
	return related_thread_groups[group_id];
}

static int alloc_related_thread_groups(void)
{
	int i;
	struct walt_related_thread_group *grp;

	/* groupd_id = 0 is invalid as it's special id to remove group. */
	for (i = 1; i < MAX_NUM_CGROUP_COLOC_ID; i++) {
		grp = kzalloc(sizeof(*grp), GFP_ATOMIC | GFP_NOWAIT);
		BUG_ON(!grp);

		grp->id = i;
		INIT_LIST_HEAD(&grp->tasks);
		INIT_LIST_HEAD(&grp->list);
		raw_spin_lock_init(&grp->lock);

		related_thread_groups[i] = grp;
	}

	return 0;
}

static void remove_task_from_group(struct task_struct *p)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	struct walt_related_thread_group *grp = wts->grp;
	struct rq *rq;
	int empty_group = 1;
	struct rq_flags rf;

	raw_spin_lock(&grp->lock);

	rq = __task_rq_lock(p, &rf);
	transfer_busy_time(rq, wts->grp, p, REM_TASK);
	list_del_init(&wts->grp_list);
	rcu_assign_pointer(wts->grp, NULL);
	__task_rq_unlock(rq, &rf);

	if (!list_empty(&grp->tasks)) {
		empty_group = 0;
		_set_preferred_cluster(grp);
	}

	raw_spin_unlock(&grp->lock);

	/* Reserved groups cannot be destroyed */
	if (empty_group && grp->id != DEFAULT_CGROUP_COLOC_ID)
		 /*
		  * We test whether grp->list is attached with list_empty()
		  * hence re-init the list after deletion.
		  */
		list_del_init(&grp->list);
}

static int
add_task_to_group(struct task_struct *p, struct walt_related_thread_group *grp)
{
	struct rq *rq;
	struct rq_flags rf;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	raw_spin_lock(&grp->lock);

	/*
	 * Change wts->grp under rq->lock. Will prevent races with read-side
	 * reference of wts->grp in various hot-paths
	 */
	rq = __task_rq_lock(p, &rf);
	transfer_busy_time(rq, grp, p, ADD_TASK);
	list_add(&wts->grp_list, &grp->tasks);
	rcu_assign_pointer(wts->grp, grp);
	__task_rq_unlock(rq, &rf);

	_set_preferred_cluster(grp);

	raw_spin_unlock(&grp->lock);

	return 0;
}

#ifdef CONFIG_UCLAMP_TASK_GROUP
static inline bool uclamp_task_colocated(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	bool colocate;
	struct walt_task_group *wtg;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	if (!css) {
		rcu_read_unlock();
		return false;
	}
	tg = container_of(css, struct task_group, css);
	wtg = (struct walt_task_group *) tg->android_vendor_data1;
	colocate = wtg->colocate;
	rcu_read_unlock();

	return colocate;
}
#else
static inline bool uclamp_task_colocated(struct task_struct *p)
{
	return false;
}
#endif /* CONFIG_UCLAMP_TASK_GROUP */

static void add_new_task_to_grp(struct task_struct *new)
{
	unsigned long flags;
	struct walt_related_thread_group *grp;
	struct walt_task_struct *wts = (struct walt_task_struct *) new->android_vendor_data1;

	/*
	 * If the task does not belong to colocated schedtune
	 * cgroup, nothing to do. We are checking this without
	 * lock. Even if there is a race, it will be added
	 * to the co-located cgroup via cgroup attach.
	 */
	if (!uclamp_task_colocated(new))
		return;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	write_lock_irqsave(&related_thread_group_lock, flags);

	/*
	 * It's possible that someone already added the new task to the
	 * group. or it might have taken out from the colocated schedtune
	 * cgroup. check these conditions under lock.
	 */
	if (!uclamp_task_colocated(new) || wts->grp) {
		write_unlock_irqrestore(&related_thread_group_lock, flags);
		return;
	}

	raw_spin_lock(&grp->lock);

	rcu_assign_pointer(wts->grp, grp);
	list_add(&wts->grp_list, &grp->tasks);

	raw_spin_unlock(&grp->lock);
	write_unlock_irqrestore(&related_thread_group_lock, flags);
}

static int __sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	int rc = 0;
	unsigned long flags;
	struct walt_related_thread_group *grp = NULL;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (group_id >= MAX_NUM_CGROUP_COLOC_ID)
		return -EINVAL;

	raw_spin_lock_irqsave(&p->pi_lock, flags);
	write_lock(&related_thread_group_lock);

	/* Switching from one group to another directly is not permitted */
	if ((!wts->grp && !group_id) || (wts->grp && group_id))
		goto done;

	if (!group_id) {
		remove_task_from_group(p);
		goto done;
	}

	grp = lookup_related_thread_group(group_id);
	if (list_empty(&grp->list))
		list_add(&grp->list, &active_related_thread_groups);

	rc = add_task_to_group(p, grp);
done:
	write_unlock(&related_thread_group_lock);
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return rc;
}

int sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	/* DEFAULT_CGROUP_COLOC_ID is a reserved id */
	if (group_id == DEFAULT_CGROUP_COLOC_ID)
		return -EINVAL;

	return __sched_set_group_id(p, group_id);
}

unsigned int sched_get_group_id(struct task_struct *p)
{
	unsigned int group_id;
	struct walt_related_thread_group *grp;

	rcu_read_lock();
	grp = task_related_thread_group(p);
	group_id = grp ? grp->id : 0;
	rcu_read_unlock();

	return group_id;
}

/*
 * We create a default colocation group at boot. There is no need to
 * synchronize tasks between cgroups at creation time because the
 * correct cgroup hierarchy is not available at boot. Therefore cgroup
 * colocation is turned off by default even though the colocation group
 * itself has been allocated. Furthermore this colocation group cannot
 * be destroyted once it has been created. All of this has been as part
 * of runtime optimizations.
 *
 * The job of synchronizing tasks to the colocation group is done when
 * the colocation flag in the cgroup is turned on.
 */
static int create_default_coloc_group(void)
{
	struct walt_related_thread_group *grp = NULL;
	unsigned long flags;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	write_lock_irqsave(&related_thread_group_lock, flags);
	list_add(&grp->list, &active_related_thread_groups);
	write_unlock_irqrestore(&related_thread_group_lock, flags);
	return 0;
}

static void walt_update_tg_pointer(struct cgroup_subsys_state *css)
{
	if (!strcmp(css->cgroup->kn->name, "top-app"))
		walt_init_topapp_tg(css_tg(css));
	else if (!strcmp(css->cgroup->kn->name, "foreground"))
		walt_init_foreground_tg(css_tg(css));
	else
		walt_init_tg(css_tg(css));
}

static void android_rvh_cpu_cgroup_online(void *unused, struct cgroup_subsys_state *css)
{
	if (unlikely(walt_disabled))
		return;

	walt_update_tg_pointer(css);
}

static void android_rvh_cpu_cgroup_attach(void *unused,
						struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	struct walt_task_group *wtg;
	unsigned int grp_id;
	int ret;

	if (unlikely(walt_disabled))
		return;

	cgroup_taskset_first(tset, &css);
	if (!css)
		return;

	tg = container_of(css, struct task_group, css);
	wtg = (struct walt_task_group *) tg->android_vendor_data1;

	cgroup_taskset_for_each(task, css, tset) {
		grp_id = wtg->colocate ? DEFAULT_CGROUP_COLOC_ID : 0;
		ret = __sched_set_group_id(task, grp_id);
		trace_sched_cgroup_attach(task, grp_id, ret);
	}
}

static bool is_cluster_hosting_top_app(struct walt_sched_cluster *cluster)
{
	struct walt_related_thread_group *grp;
	bool grp_on_min;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);

	if (!grp)
		return false;

	grp_on_min = !grp->skip_min && (boost_policy != SCHED_BOOST_ON_BIG);

	return (is_min_capacity_cluster(cluster) == grp_on_min);
}

static void note_task_waking(struct task_struct *p, u64 wallclock)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wts->last_wake_ts = wallclock;
}

/*
 * Task's cpu usage is accounted in:
 *	wrq->curr/prev_runnable_sum,  when its ->grp is NULL
 *	grp->cpu_time[cpu]->curr/prev_runnable_sum, when its ->grp is !NULL
 *
 * Transfer task's cpu usage between those counters when transitioning between
 * groups
 */
static void transfer_busy_time(struct rq *rq,
				struct walt_related_thread_group *grp,
					struct task_struct *p, int event)
{
	u64 wallclock;
	struct group_cpu_time *cpu_time;
	u64 *src_curr_runnable_sum, *dst_curr_runnable_sum;
	u64 *src_prev_runnable_sum, *dst_prev_runnable_sum;
	u64 *src_nt_curr_runnable_sum, *dst_nt_curr_runnable_sum;
	u64 *src_nt_prev_runnable_sum, *dst_nt_prev_runnable_sum;
	int migrate_type;
	int cpu = cpu_of(rq);
	bool new_task;
	int i;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	wallclock = walt_ktime_get_ns();

	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(p, rq, TASK_UPDATE, wallclock, 0);
	new_task = is_new_task(p);

	cpu_time = &wrq->grp_time;
	if (event == ADD_TASK) {
		migrate_type = RQ_TO_GROUP;

		src_curr_runnable_sum = &wrq->curr_runnable_sum;
		dst_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		src_prev_runnable_sum = &wrq->prev_runnable_sum;
		dst_prev_runnable_sum = &cpu_time->prev_runnable_sum;

		src_nt_curr_runnable_sum = &wrq->nt_curr_runnable_sum;
		dst_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &wrq->nt_prev_runnable_sum;
		dst_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		if (*src_curr_runnable_sum < wts->curr_window_cpu[cpu]) {
			WALT_BUG(p, "pid=%u CPU=%d event=%d src_crs=%llu is lesser than task_contrib=%llu",
				 p->pid, cpu, event, *src_curr_runnable_sum,
				 wts->curr_window_cpu[cpu]);
			*src_curr_runnable_sum = wts->curr_window_cpu[cpu];
		}
		*src_curr_runnable_sum -= wts->curr_window_cpu[cpu];

		if (*src_prev_runnable_sum < wts->prev_window_cpu[cpu]) {
			WALT_BUG(p, "pid=%u CPU=%d event=%d src_prs=%llu is lesser than task_contrib=%llu",
				 p->pid, cpu, event, *src_prev_runnable_sum,
				 wts->prev_window_cpu[cpu]);
			*src_prev_runnable_sum = wts->prev_window_cpu[cpu];
		}
		*src_prev_runnable_sum -= wts->prev_window_cpu[cpu];

		if (new_task) {
			if (*src_nt_curr_runnable_sum < wts->curr_window_cpu[cpu]) {
				WALT_BUG(p, "pid=%u CPU=%d event=%d src_nt_crs=%llu is lesser than task_contrib=%llu",
					 p->pid, cpu, event,
					 *src_nt_curr_runnable_sum,
					 wts->curr_window_cpu[cpu]);
				*src_nt_curr_runnable_sum = wts->curr_window_cpu[cpu];
			}
			*src_nt_curr_runnable_sum -=
					wts->curr_window_cpu[cpu];

			if (*src_nt_prev_runnable_sum < wts->prev_window_cpu[cpu]) {
				WALT_BUG(p, "pid=%u CPU=%d event=%d src_nt_prs=%llu is lesser than task_contrib=%llu",
					 p->pid, cpu, event,
					 *src_nt_prev_runnable_sum,
					 wts->prev_window_cpu[cpu]);
				*src_nt_prev_runnable_sum = wts->prev_window_cpu[cpu];
			}
			*src_nt_prev_runnable_sum -=
					wts->prev_window_cpu[cpu];
		}

		update_cluster_load_subtractions(p, cpu,
				wrq->window_start, new_task);

	} else {
		migrate_type = GROUP_TO_RQ;

		src_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		dst_curr_runnable_sum = &wrq->curr_runnable_sum;
		src_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		dst_prev_runnable_sum = &wrq->prev_runnable_sum;

		src_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		dst_nt_curr_runnable_sum = &wrq->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;
		dst_nt_prev_runnable_sum = &wrq->nt_prev_runnable_sum;

		if (*src_curr_runnable_sum < wts->curr_window) {
			WALT_BUG(p, "WALT-UG pid=%u CPU=%d event=%d src_crs=%llu is lesser than task_contrib=%llu",
				 p->pid, cpu, event, *src_curr_runnable_sum,
				 wts->curr_window);
			*src_curr_runnable_sum = wts->curr_window;
		}
		*src_curr_runnable_sum -= wts->curr_window;

		if (*src_prev_runnable_sum < wts->prev_window) {
			WALT_BUG(p, "pid=%u CPU=%d event=%d src_prs=%llu is lesser than task_contrib=%llu",
				 p->pid, cpu, event, *src_prev_runnable_sum,
				 wts->prev_window);
			*src_prev_runnable_sum = wts->prev_window;
		}
		*src_prev_runnable_sum -= wts->prev_window;

		if (new_task) {
			if (*src_nt_curr_runnable_sum < wts->curr_window) {
				WALT_BUG(p, "pid=%u CPU=%d event=%d src_nt_crs=%llu is lesser than task_contrib=%llu",
						p->pid, cpu, event,
						*src_nt_curr_runnable_sum,
						wts->curr_window);
				*src_nt_curr_runnable_sum = wts->curr_window;
			}
			*src_nt_curr_runnable_sum -= wts->curr_window;

			if (*src_nt_prev_runnable_sum < wts->prev_window) {
				WALT_BUG(p, "pid=%u CPU=%d event=%d src_nt_prs=%llu is lesser than task_contrib=%llu",
					 p->pid, cpu, event,
					 *src_nt_prev_runnable_sum,
					 wts->prev_window);
				*src_nt_prev_runnable_sum = wts->prev_window;
			}
			*src_nt_prev_runnable_sum -= wts->prev_window;
		}

		/*
		 * Need to reset curr/prev windows for all CPUs, not just the
		 * ones in the same cluster. Since inter cluster migrations
		 * did not result in the appropriate book keeping, the values
		 * per CPU would be inaccurate.
		 */
		for_each_possible_cpu(i) {
			wts->curr_window_cpu[i] = 0;
			wts->prev_window_cpu[i] = 0;
		}
	}

	*dst_curr_runnable_sum += wts->curr_window;
	*dst_prev_runnable_sum += wts->prev_window;
	if (new_task) {
		*dst_nt_curr_runnable_sum += wts->curr_window;
		*dst_nt_prev_runnable_sum += wts->prev_window;
	}

	/*
	 * When a task enter or exits a group, it's curr and prev windows are
	 * moved to a single CPU. This behavior might be sub-optimal in the
	 * exit case, however, it saves us the overhead of handling inter
	 * cluster migration fixups while the task is part of a related group.
	 */
	wts->curr_window_cpu[cpu] = wts->curr_window;
	wts->prev_window_cpu[cpu] = wts->prev_window;

	trace_sched_migration_update_sum(p, migrate_type, rq);
}

bool is_rtgb_active(void)
{
	struct walt_related_thread_group *grp;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	return grp && grp->skip_min;
}

u64 get_rtgb_active_time(void)
{
	struct walt_related_thread_group *grp;
	u64 now = walt_ktime_get_ns();

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);

	if (grp && grp->skip_min && grp->start_ktime_ts)
		return now - grp->start_ktime_ts;

	return 0;
}

static void walt_init_window_dep(void);
static void walt_tunables_fixup(void)
{
	if (likely(num_sched_clusters > 0))
		walt_update_group_thresholds();
	walt_init_window_dep();
}

static void walt_update_irqload(struct rq *rq)
{
	u64 irq_delta = 0;
	unsigned int nr_windows = 0;
	u64 cur_irq_time;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 last_irq_window = READ_ONCE(wrq->last_irq_window);

	if (wrq->window_start > last_irq_window)
		nr_windows = div64_u64(wrq->window_start - last_irq_window,
				       sched_ravg_window);

	/* Decay CPU's irqload by 3/4 for each window. */
	if (nr_windows < 10)
		wrq->avg_irqload = mult_frac(wrq->avg_irqload, 3, 4);
	else
		wrq->avg_irqload = 0;

	cur_irq_time = irq_time_read(cpu_of(rq));
	if (cur_irq_time > wrq->prev_irq_time)
		irq_delta = cur_irq_time - wrq->prev_irq_time;

	wrq->avg_irqload += irq_delta;
	wrq->prev_irq_time = cur_irq_time;

	if (nr_windows < SCHED_HIGH_IRQ_TIMEOUT)
		wrq->high_irqload = (wrq->avg_irqload >=
					walt_cpu_high_irqload);
	else
		wrq->high_irqload = false;
}

/*
 * Runs in hard-irq context. This should ideally run just after the latest
 * window roll-over.
 */
static void walt_irq_work(struct irq_work *irq_work)
{
	struct walt_sched_cluster *cluster;
	struct rq *rq;
	int cpu;
	u64 wc;
	bool is_migration = false, is_asym_migration = false;
	u64 total_grp_load = 0, min_cluster_grp_load = 0;
	int level = 0;
	unsigned long flags;
	struct walt_rq *wrq;

	/* Am I the window rollover work or the migration work? */
	if (irq_work == &walt_migration_irq_work)
		is_migration = true;

	for_each_cpu(cpu, cpu_possible_mask) {
		if (level == 0)
			raw_spin_lock(&cpu_rq(cpu)->lock);
		else
			raw_spin_lock_nested(&cpu_rq(cpu)->lock, level);
		level++;
	}

	wc = walt_ktime_get_ns();
	walt_load_reported_window = atomic64_read(&walt_irq_work_lastq_ws);
	for_each_sched_cluster(cluster) {
		u64 aggr_grp_load = 0;

		raw_spin_lock(&cluster->load_lock);

		for_each_cpu(cpu, &cluster->cpus) {
			rq = cpu_rq(cpu);
			wrq = (struct walt_rq *) rq->android_vendor_data1;
			if (rq->curr) {
				walt_update_task_ravg(rq->curr, rq,
						TASK_UPDATE, wc, 0);
				account_load_subtractions(rq);
				aggr_grp_load +=
					wrq->grp_time.prev_runnable_sum;
			}
			if (is_migration && wrq->notif_pending &&
			    cpumask_test_cpu(cpu, &asym_cap_sibling_cpus)) {
				is_asym_migration = true;
				wrq->notif_pending = false;
			}
		}

		cluster->aggr_grp_load = aggr_grp_load;
		total_grp_load += aggr_grp_load;

		if (is_min_capacity_cluster(cluster))
			min_cluster_grp_load = aggr_grp_load;
		raw_spin_unlock(&cluster->load_lock);
	}

	if (total_grp_load) {
		if (cpumask_weight(&asym_cap_sibling_cpus)) {
			u64 big_grp_load =
					  total_grp_load - min_cluster_grp_load;

			for_each_cpu(cpu, &asym_cap_sibling_cpus)
				cpu_cluster(cpu)->aggr_grp_load = big_grp_load;
		}
		rtgb_active = is_rtgb_active();
	} else {
		rtgb_active = false;
	}

	if (!is_migration && sysctl_sched_user_hint && time_after(jiffies,
						sched_user_hint_reset_time))
		sysctl_sched_user_hint = 0;

	for_each_sched_cluster(cluster) {
		cpumask_t cluster_online_cpus;
		unsigned int num_cpus, i = 1;

		cpumask_and(&cluster_online_cpus, &cluster->cpus,
						cpu_online_mask);
		num_cpus = cpumask_weight(&cluster_online_cpus);
		for_each_cpu(cpu, &cluster_online_cpus) {
			int wflag = 0;

			rq = cpu_rq(cpu);
			wrq = (struct walt_rq *) rq->android_vendor_data1;

			if (is_migration) {
				if (wrq->notif_pending) {
					wrq->notif_pending = false;

					wflag |= WALT_CPUFREQ_IC_MIGRATION;
				}
			} else {
				wflag |= WALT_CPUFREQ_ROLLOVER;
			}

			if (is_asym_migration && cpumask_test_cpu(cpu,
							&asym_cap_sibling_cpus)) {
				wflag |= WALT_CPUFREQ_IC_MIGRATION;
			}

			if (i == num_cpus)
				waltgov_run_callback(cpu_rq(cpu), wflag);
			else
				waltgov_run_callback(cpu_rq(cpu), wflag |
							WALT_CPUFREQ_CONTINUE);
			i++;

			if (!is_migration)
				walt_update_irqload(rq);
		}
	}

	/*
	 * If the window change request is in pending, good place to
	 * change sched_ravg_window since all rq locks are acquired.
	 *
	 * If the current window roll over is delayed such that the
	 * mark_start (current wallclock with which roll over is done)
	 * of the current task went past the window start with the
	 * updated new window size, delay the update to the next
	 * window roll over. Otherwise the CPU counters (prs and crs) are
	 * not rolled over properly as mark_start > window_start.
	 */
	if (!is_migration) {
		spin_lock_irqsave(&sched_ravg_window_lock, flags);
		wrq = (struct walt_rq *) this_rq()->android_vendor_data1;
		if ((sched_ravg_window != new_sched_ravg_window) &&
		    (wc < wrq->window_start + new_sched_ravg_window)) {
			sched_ravg_window_change_time = walt_ktime_get_ns();
			trace_sched_ravg_window_change(sched_ravg_window,
					new_sched_ravg_window,
					sched_ravg_window_change_time);
			sched_ravg_window = new_sched_ravg_window;
			walt_tunables_fixup();
		}
		spin_unlock_irqrestore(&sched_ravg_window_lock, flags);
	}

	for_each_cpu(cpu, cpu_possible_mask)
		raw_spin_unlock(&cpu_rq(cpu)->lock);

	if (!is_migration) {
		wrq = (struct walt_rq *) this_rq()->android_vendor_data1;
		core_ctl_check(wrq->window_start);
	}
}

void walt_rotation_checkpoint(int nr_big)
{
	if (!hmp_capable())
		return;

	if (!sysctl_sched_walt_rotate_big_tasks || sched_boost_type != NO_BOOST) {
		walt_rotation_enabled = 0;
		return;
	}

	walt_rotation_enabled = nr_big >= num_possible_cpus();
}

void walt_fill_ta_data(struct core_ctl_notif_data *data)
{
	struct walt_related_thread_group *grp;
	unsigned long flags;
	u64 total_demand = 0, wallclock;
	int min_cap_cpu, scale = 1024;
	struct walt_sched_cluster *cluster;
	int i = 0;
	struct walt_task_struct *wts;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);

	raw_spin_lock_irqsave(&grp->lock, flags);
	if (list_empty(&grp->tasks)) {
		raw_spin_unlock_irqrestore(&grp->lock, flags);
		goto fill_util;
	}

	wallclock = walt_ktime_get_ns();

	list_for_each_entry(wts, &grp->tasks, grp_list) {
		if (wts->mark_start < wallclock -
		    (sched_ravg_window * sched_ravg_hist_size))
			continue;

		total_demand += wts->coloc_demand;
	}

	raw_spin_unlock_irqrestore(&grp->lock, flags);

	/*
	 * Scale the total demand to the lowest capacity CPU and
	 * convert into percentage.
	 *
	 * P = total_demand/sched_ravg_window * 1024/scale * 100
	 */

	min_cap_cpu = cpumask_first(&cpu_array[0][0]);
	if (min_cap_cpu != -1)
		scale = arch_scale_cpu_capacity(min_cap_cpu);

	data->coloc_load_pct = div64_u64(total_demand * 1024 * 100,
			       (u64)sched_ravg_window * scale);

fill_util:
	for_each_sched_cluster(cluster) {
		int fcpu = cluster_first_cpu(cluster);

		if (i == MAX_CLUSTERS)
			break;

		scale = arch_scale_cpu_capacity(fcpu);
		data->ta_util_pct[i] = div64_u64(cluster->aggr_grp_load * 1024 *
				       100, (u64)sched_ravg_window * scale);

		scale = arch_scale_freq_capacity(fcpu);
		data->cur_cap_pct[i] = (scale * 100)/1024;
		i++;
	}
}

static void walt_init_window_dep(void)
{
	walt_scale_demand_divisor = sched_ravg_window >> SCHED_CAPACITY_SHIFT;

	sched_init_task_load_windows =
		div64_u64((u64)sysctl_sched_init_task_load_pct *
			  (u64)sched_ravg_window, 100);
	sched_init_task_load_windows_scaled =
		scale_demand(sched_init_task_load_windows);

	walt_cpu_high_irqload = div64_u64((u64)sched_ravg_window * 95, (u64) 100);
}

static void walt_init_once(void)
{
	init_irq_work(&walt_migration_irq_work, walt_irq_work);
	init_irq_work(&walt_cpufreq_irq_work, walt_irq_work);
	walt_init_window_dep();
}

static void walt_sched_init_rq(struct rq *rq)
{
	int j;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	if (cpu_of(rq) == 0)
		walt_init_once();

	cpumask_set_cpu(cpu_of(rq), &wrq->freq_domain_cpumask);

	wrq->walt_stats.cumulative_runnable_avg_scaled = 0;
	wrq->prev_window_size = sched_ravg_window;
	wrq->window_start = 0;
	wrq->walt_stats.nr_big_tasks = 0;
	wrq->walt_flags = 0;
	wrq->avg_irqload = 0;
	wrq->prev_irq_time = 0;
	wrq->last_irq_window = 0;
	wrq->high_irqload = false;
	wrq->task_exec_scale = 1024;
	wrq->push_task = NULL;

	/*
	 * All cpus part of same cluster by default. This avoids the
	 * need to check for wrq->cluster being non-NULL in hot-paths
	 * like select_best_cpu()
	 */
	wrq->cluster = &init_cluster;
	wrq->curr_runnable_sum = wrq->prev_runnable_sum = 0;
	wrq->nt_curr_runnable_sum = wrq->nt_prev_runnable_sum = 0;
	memset(&wrq->grp_time, 0, sizeof(struct group_cpu_time));
	wrq->old_busy_time = 0;
	wrq->old_estimated_time = 0;
	wrq->walt_stats.pred_demands_sum_scaled = 0;
	wrq->walt_stats.nr_rtg_high_prio_tasks = 0;
	wrq->ed_task = NULL;
	wrq->curr_table = 0;
	wrq->prev_top = 0;
	wrq->curr_top = 0;
	wrq->last_cc_update = 0;
	wrq->cycles = 0;
	for (j = 0; j < NUM_TRACKED_WINDOWS; j++) {
		memset(&wrq->load_subs[j], 0,
				sizeof(struct load_subtractions));
		wrq->top_tasks[j] = kcalloc(NUM_LOAD_INDICES,
				sizeof(u8), GFP_ATOMIC | GFP_NOWAIT);
		/* No other choice */
		BUG_ON(!wrq->top_tasks[j]);
		clear_top_tasks_bitmap(wrq->top_tasks_bitmap[j]);
	}
	wrq->notif_pending = false;

	wrq->num_mvp_tasks = 0;
	INIT_LIST_HEAD(&wrq->mvp_tasks);
}

void sched_window_nr_ticks_change(void)
{
	unsigned long flags;

	spin_lock_irqsave(&sched_ravg_window_lock, flags);
	new_sched_ravg_window = mult_frac(sysctl_sched_ravg_window_nr_ticks,
						NSEC_PER_SEC, HZ);
	spin_unlock_irqrestore(&sched_ravg_window_lock, flags);
}

static void
walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	fixup_cumulative_runnable_avg(rq, p, &wrq->walt_stats, wts->demand_scaled,
					wts->pred_demand_scaled);
}

static void
walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	fixup_cumulative_runnable_avg(rq, p, &wrq->walt_stats,
				      -(s64)wts->demand_scaled,
				      -(s64)wts->pred_demand_scaled);
}

static void inc_rq_walt_stats(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (wts->misfit)
		wrq->walt_stats.nr_big_tasks++;

	wts->rtg_high_prio = task_rtg_high_prio(p);
	if (wts->rtg_high_prio)
		wrq->walt_stats.nr_rtg_high_prio_tasks++;
}

static void dec_rq_walt_stats(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (wts->misfit)
		wrq->walt_stats.nr_big_tasks--;

	if (wts->rtg_high_prio)
		wrq->walt_stats.nr_rtg_high_prio_tasks--;

	BUG_ON(wrq->walt_stats.nr_big_tasks < 0);
}

static void android_rvh_wake_up_new_task(void *unused, struct task_struct *new)
{
	if (unlikely(walt_disabled))
		return;
	init_new_task_load(new);
	add_new_task_to_grp(new);
}

static void walt_cpu_frequency_limits(void *unused, struct cpufreq_policy *policy)
{
	if (unlikely(walt_disabled))
		return;

	cpu_cluster(policy->cpu)->max_freq = policy->max;
}

/*
 * The intention of this hook is to update cpu_capacity_orig as well as
 * (*capacity), otherwise we will end up capacity_of() > capacity_orig_of().
 */
static void android_rvh_update_cpu_capacity(void *unused, int cpu, unsigned long *capacity)
{
	unsigned long fmax_capacity = arch_scale_cpu_capacity(cpu);
	unsigned long thermal_pressure = arch_scale_thermal_pressure(cpu);
	unsigned long thermal_cap, old;
	unsigned long rt_pressure = fmax_capacity - *capacity;
	struct walt_sched_cluster *cluster;
	struct rq *rq = cpu_rq(cpu);

	if (unlikely(walt_disabled))
		return;

	/*
	 * thermal_pressure = cpu_scale - curr_cap_as_per_thermal.
	 * so,
	 * curr_cap_as_per_thermal = cpu_scale - thermal_pressure.
	 */

	thermal_cap = fmax_capacity - thermal_pressure;

	cluster = cpu_cluster(cpu);
	/* reduce the fmax_capacity under cpufreq constraints */
	if (cluster->max_freq != cluster->max_possible_freq)
		fmax_capacity = mult_frac(fmax_capacity, cluster->max_freq,
					 cluster->max_possible_freq);

	old = rq->cpu_capacity_orig;
	rq->cpu_capacity_orig = min(fmax_capacity, thermal_cap);

	if (old != rq->cpu_capacity_orig)
		trace_update_cpu_capacity(cpu, rt_pressure, *capacity);

	*capacity = max(rq->cpu_capacity_orig - rt_pressure, 1UL);
}

static void android_rvh_sched_cpu_starting(void *unused, int cpu)
{
	unsigned long flags;
	struct rq *rq = cpu_rq(cpu);

	if (unlikely(walt_disabled))
		return;
	raw_spin_lock_irqsave(&rq->lock, flags);
	set_window_start(rq);
	raw_spin_unlock_irqrestore(&rq->lock, flags);

	clear_walt_request(cpu);
}

static void android_rvh_sched_cpu_dying(void *unused, int cpu)
{
	if (unlikely(walt_disabled))
		return;
	clear_walt_request(cpu);
}

static void android_rvh_set_task_cpu(void *unused, struct task_struct *p, unsigned int new_cpu)
{
	if (unlikely(walt_disabled))
		return;
	fixup_busy_time(p, (int) new_cpu);
}

static void android_rvh_new_task_stats(void *unused, struct task_struct *p)
{
	if (unlikely(walt_disabled))
		return;
	mark_task_starting(p);
}

static void android_rvh_account_irq(void *unused, struct task_struct *curr, int cpu, s64 delta)
{
	struct walt_rq *wrq = (struct walt_rq *) cpu_rq(cpu)->android_vendor_data1;

	if (unlikely(walt_disabled))
		return;
	if (!!(curr->flags & PF_IDLE)) {
		if (hardirq_count() || in_serving_softirq())
			walt_sched_account_irqend(cpu, curr, delta);
		else
			walt_sched_account_irqstart(cpu, curr);
	}
	wrq->last_irq_window = wrq->window_start;
}

static void android_rvh_flush_task(void *unused, struct task_struct *p)
{
	if (unlikely(walt_disabled))
		return;
	walt_task_dead(p);
}

static void android_rvh_enqueue_task(void *unused, struct rq *rq, struct task_struct *p)
{
	u64 wallclock = walt_ktime_get_ns();
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	bool double_enqueue = false;

	if (unlikely(walt_disabled))
		return;

	lockdep_assert_held(&rq->lock);

	if (p->cpu != cpu_of(rq))
		WALT_BUG(p, "enqueuing on rq %d when task->cpu is %d\n",
				cpu_of(rq), p->cpu);

	/* catch double enqueue */
	if (wts->prev_on_rq == 1) {
		WALT_BUG(p, "double enqueue detected: task_cpu=%d new_cpu=%d\n",
			 task_cpu(p), cpu_of(rq));
		double_enqueue = true;
	}

	wts->prev_on_rq = 1;
	wts->prev_on_rq_cpu = cpu_of(rq);

	wts->last_enqueued_ts = wallclock;
	sched_update_nr_prod(rq->cpu, 1);

	if (walt_fair_task(p)) {
		wts->misfit = !task_fits_max(p, rq->cpu);
		if (!double_enqueue)
			inc_rq_walt_stats(rq, p);
		walt_cfs_enqueue_task(rq, p);
	}

	if (!double_enqueue)
		walt_inc_cumulative_runnable_avg(rq, p);
	trace_sched_enq_deq_task(p, 1, cpumask_bits(&p->cpus_mask)[0], is_mvp(wts));
}

static void android_rvh_dequeue_task(void *unused, struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;
	bool double_dequeue = false;

	if (unlikely(walt_disabled))
		return;

	lockdep_assert_held(&rq->lock);

	/*
	 * a task can be enqueued before walt is started, and dequeued after.
	 * therefore the check to ensure that prev_on_rq_cpu is needed to prevent
	 * an invalid failure.
	 */
	if (wts->prev_on_rq_cpu >= 0 && wts->prev_on_rq_cpu != cpu_of(rq))
		WALT_BUG(p, "dequeue cpu %d not same as enqueue %d\n",
			 cpu_of(rq), wts->prev_on_rq_cpu);

	/* no longer on a cpu */
	wts->prev_on_rq_cpu = -1;

	/* catch double deq */
	if (wts->prev_on_rq == 2) {
		WALT_BUG(p, "double dequeue detected: task_cpu=%d new_cpu=%d\n",
			 task_cpu(p), cpu_of(rq));
		double_dequeue = true;
	}

	wts->prev_on_rq = 2;
	if (p == wrq->ed_task)
		is_ed_task_present(rq, walt_ktime_get_ns(), p);

	sched_update_nr_prod(rq->cpu, -1);

	if (walt_fair_task(p)) {
		if (!double_dequeue)
			dec_rq_walt_stats(rq, p);
		walt_cfs_dequeue_task(rq, p);
	}

	if (!double_dequeue)
		walt_dec_cumulative_runnable_avg(rq, p);

	trace_sched_enq_deq_task(p, 0, cpumask_bits(&p->cpus_mask)[0], is_mvp(wts));
}

static void android_rvh_update_misfit_status(void *unused, struct task_struct *p,
		struct rq *rq, bool *need_update)
{
	struct walt_task_struct *wts;
	struct walt_rq *wrq;
	bool old_misfit, misfit;
	int change;

	if (unlikely(walt_disabled))
		return;
	*need_update = false;

	if (!p) {
		rq->misfit_task_load = 0;
		return;
	}

	wrq = (struct walt_rq *) rq->android_vendor_data1;
	wts = (struct walt_task_struct *) p->android_vendor_data1;
	old_misfit = wts->misfit;

	if (task_fits_max(p, rq->cpu))
		rq->misfit_task_load = 0;
	else
		rq->misfit_task_load = task_util(p);

	misfit = rq->misfit_task_load;

	change = misfit - old_misfit;
	if (change) {
		sched_update_nr_prod(rq->cpu, 0);
		wts->misfit = misfit;
		wrq->walt_stats.nr_big_tasks += change;
		BUG_ON(wrq->walt_stats.nr_big_tasks < 0);
	}
}

/* utility function to update walt signals at wakeup */
static void android_rvh_try_to_wake_up(void *unused, struct task_struct *p)
{
	struct rq *rq = cpu_rq(task_cpu(p));
	struct rq_flags rf;
	u64 wallclock;
	unsigned int old_load;
	struct walt_related_thread_group *grp = NULL;

	if (unlikely(walt_disabled))
		return;
	rq_lock_irqsave(rq, &rf);
	old_load = task_load(p);
	wallclock = walt_ktime_get_ns();
	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(p, rq, TASK_WAKE, wallclock, 0);
	note_task_waking(p, wallclock);
	rq_unlock_irqrestore(rq, &rf);

	rcu_read_lock();
	grp = task_related_thread_group(p);
	if (update_preferred_cluster(grp, p, old_load, false))
		set_preferred_cluster(grp);
	rcu_read_unlock();
}

static void android_rvh_try_to_wake_up_success(void *unused, struct task_struct *p)
{
	unsigned long flags;
	int cpu = p->cpu;

	if (unlikely(walt_disabled))
		return;

	raw_spin_lock_irqsave(&cpu_rq(cpu)->lock, flags);
	if (do_pl_notif(cpu_rq(cpu)))
		waltgov_run_callback(cpu_rq(cpu), WALT_CPUFREQ_PL);
	raw_spin_unlock_irqrestore(&cpu_rq(cpu)->lock, flags);
}

static void android_rvh_tick_entry(void *unused, struct rq *rq)
{
	u64 wallclock;

	lockdep_assert_held(&rq->lock);
	if (unlikely(walt_disabled))
		return;

	set_window_start(rq);
	wallclock = walt_ktime_get_ns();

	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);

	if (is_ed_task_present(rq, wallclock, NULL))
		waltgov_run_callback(rq, WALT_CPUFREQ_EARLY_DET);
}

static void android_vh_scheduler_tick(void *unused, struct rq *rq)
{
	struct walt_related_thread_group *grp;
	u32 old_load;

	if (unlikely(walt_disabled))
		return;

	old_load = task_load(rq->curr);
	rcu_read_lock();
	grp = task_related_thread_group(rq->curr);
	if (update_preferred_cluster(grp, rq->curr, old_load, true))
		set_preferred_cluster(grp);
	rcu_read_unlock();

	walt_lb_tick(rq);
}

static void android_rvh_schedule(void *unused, struct task_struct *prev,
		struct task_struct *next, struct rq *rq)
{
	u64 wallclock = walt_ktime_get_ns();
	struct walt_task_struct *wts = (struct walt_task_struct *) prev->android_vendor_data1;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	if (unlikely(walt_disabled))
		return;
	if (likely(prev != next)) {
		if (!prev->on_rq)
			wts->last_sleep_ts = wallclock;
		walt_update_task_ravg(prev, rq, PUT_PREV_TASK, wallclock, 0);
		walt_update_task_ravg(next, rq, PICK_NEXT_TASK, wallclock, 0);
		if (is_idle_task(next) && wrq->walt_stats.cumulative_runnable_avg_scaled != 0)
			WALT_BUG(next, "next=idle cra non zero=%d\n",
				 wrq->walt_stats.cumulative_runnable_avg_scaled);
	} else {
		walt_update_task_ravg(prev, rq, TASK_UPDATE, wallclock, 0);
	}
}

static void android_rvh_resume_cpus(void *unused, struct cpumask *resuming_cpus, int *err)
{
	int i;
	struct rq *rq;
	unsigned long flags;

	if (unlikely(walt_disabled))
		return;
	/*
	 * send a reschedule event  on all resumed CPUs
	 * which trigger newly idle load balance.
	 */
	for_each_cpu(i, resuming_cpus) {
		rq = cpu_rq(i);
		raw_spin_lock_irqsave(&rq->lock, flags);
		resched_curr(rq);
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	}

	*err = 0;
}

static void android_rvh_update_cpus_allowed(void *unused, struct task_struct *p,
						cpumask_var_t cpus_requested,
						const struct cpumask *new_mask, int *ret)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (unlikely(walt_disabled))
		return;
	if (cpumask_subset(&wts->cpus_requested, cpus_requested))
		*ret = set_cpus_allowed_ptr(p, &wts->cpus_requested);
}

static void android_rvh_sched_setaffinity(void *unused, struct task_struct *p,
					  const struct cpumask *in_mask,
					  int *retval)
{
	struct walt_task_struct *wts = (struct walt_task_struct *) p->android_vendor_data1;

	if (unlikely(walt_disabled))
		return;

	/* nothing to do if the affinity call failed */
	if (*retval)
		return;

	/*
	 * cache the affinity for user space tasks so that they
	 * can be restored during cpuset cgroup change.
	 */
	if (!(p->flags & PF_KTHREAD))
		cpumask_and(&wts->cpus_requested, in_mask, cpu_possible_mask);
}

static void android_rvh_sched_fork_init(void *unused, struct task_struct *p)
{
	if (unlikely(walt_disabled))
		return;

	__sched_fork_init(p);
}

static void android_rvh_ttwu_cond(void *unused, bool *cond)
{
	if (unlikely(walt_disabled))
		return;
	*cond = sysctl_sched_many_wakeup_threshold < WALT_MANY_WAKEUP_DEFAULT;
}

static void android_rvh_sched_exec(void *unused, bool *cond)
{
	if (unlikely(walt_disabled))
		return;
	*cond = true;
}

static void android_rvh_build_perf_domains(void *unused, bool *eas_check)
{
	*eas_check = true;
}

static void android_rvh_force_compatible_pre(void *unused, void *unused2)
{
	if (unlikely(walt_disabled))
		return;
	cpu_maps_update_begin();
}

static void android_rvh_force_compatible_post(void *unused, void *unused2)
{
	if (unlikely(walt_disabled))
		return;
	cpu_maps_update_done();
}

static void dump_throttled_rt_tasks(void *unused, int cpu, u64 clock,
		ktime_t rt_period, u64 rt_runtime, s64 rt_period_timer_expires)
{
	printk_deferred("sched: RT throttling activated for cpu %d\n", cpu);
	printk_deferred("rt_period_timer: expires=%lld now=%llu rt_time=%llu runtime=%llu period=%llu\n",
			rt_period_timer_expires, ktime_get_ns(),
			task_rq(current)->rt.rt_time, rt_runtime, rt_period);
	printk_deferred("potential CPU hogs:\n");
#ifdef CONFIG_SCHED_INFO
	if (sched_info_on())
		printk_deferred("current %s (%d) is running for %llu nsec\n",
				current->comm, current->pid,
				clock - current->sched_info.last_arrival);
#endif
	BUG_ON(sysctl_sched_bug_on_rt_throttle);
}

static void register_walt_hooks(void)
{
	register_trace_android_rvh_wake_up_new_task(android_rvh_wake_up_new_task, NULL);
	register_trace_android_rvh_update_cpu_capacity(android_rvh_update_cpu_capacity, NULL);
	register_trace_android_rvh_sched_cpu_starting(android_rvh_sched_cpu_starting, NULL);
	register_trace_android_rvh_sched_cpu_dying(android_rvh_sched_cpu_dying, NULL);
	register_trace_android_rvh_set_task_cpu(android_rvh_set_task_cpu, NULL);
	register_trace_android_rvh_new_task_stats(android_rvh_new_task_stats, NULL);
	register_trace_android_rvh_account_irq(android_rvh_account_irq, NULL);
	register_trace_android_rvh_flush_task(android_rvh_flush_task, NULL);
	register_trace_android_rvh_update_misfit_status(android_rvh_update_misfit_status, NULL);
	register_trace_android_rvh_after_enqueue_task(android_rvh_enqueue_task, NULL);
	register_trace_android_rvh_after_dequeue_task(android_rvh_dequeue_task, NULL);
	register_trace_android_rvh_try_to_wake_up(android_rvh_try_to_wake_up, NULL);
	register_trace_android_rvh_try_to_wake_up_success(android_rvh_try_to_wake_up_success, NULL);
	register_trace_android_rvh_tick_entry(android_rvh_tick_entry, NULL);
	register_trace_android_vh_scheduler_tick(android_vh_scheduler_tick, NULL);
	register_trace_android_rvh_schedule(android_rvh_schedule, NULL);
	register_trace_android_rvh_resume_cpus(android_rvh_resume_cpus, NULL);
	register_trace_android_rvh_cpu_cgroup_attach(android_rvh_cpu_cgroup_attach, NULL);
	register_trace_android_rvh_cpu_cgroup_online(android_rvh_cpu_cgroup_online, NULL);
	register_trace_android_rvh_update_cpus_allowed(android_rvh_update_cpus_allowed, NULL);
	register_trace_android_rvh_sched_setaffinity(android_rvh_sched_setaffinity, NULL);
	register_trace_android_rvh_sched_fork_init(android_rvh_sched_fork_init, NULL);
	register_trace_android_rvh_ttwu_cond(android_rvh_ttwu_cond, NULL);
	register_trace_android_rvh_sched_exec(android_rvh_sched_exec, NULL);
	register_trace_android_rvh_build_perf_domains(android_rvh_build_perf_domains, NULL);
	register_trace_cpu_frequency_limits(walt_cpu_frequency_limits, NULL);
	register_trace_android_rvh_force_compatible_pre(android_rvh_force_compatible_pre, NULL);
	register_trace_android_rvh_force_compatible_post(android_rvh_force_compatible_post, NULL);
	register_trace_android_vh_dump_throttled_rt_tasks(dump_throttled_rt_tasks, NULL);
}

atomic64_t walt_irq_work_lastq_ws;
bool walt_disabled = true;

static int walt_init_stop_handler(void *data)
{
	int cpu;
	struct task_struct *g, *p;
	u64 window_start_ns, nr_windows;
	struct walt_rq *wrq;
	int level = 0;

	read_lock(&tasklist_lock);
	for_each_possible_cpu(cpu) {
		if (level == 0)
			raw_spin_lock(&cpu_rq(cpu)->lock);
		else
			raw_spin_lock_nested(&cpu_rq(cpu)->lock, level);
		level++;
	}

	do_each_thread(g, p) {
		init_existing_task_load(p);
	} while_each_thread(g, p);

	window_start_ns = ktime_get_ns();
	nr_windows = div64_u64(window_start_ns, sched_ravg_window);
	window_start_ns = (u64)nr_windows * (u64)sched_ravg_window;

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		/* Create task members for idle thread */
		init_new_task_load(rq->idle);

		walt_sched_init_rq(rq);

		wrq = (struct walt_rq *) rq->android_vendor_data1;
		wrq->window_start = window_start_ns;
	}

	atomic64_set(&walt_irq_work_lastq_ws, window_start_ns);

	create_default_coloc_group();

	walt_update_cluster_topology();

	walt_disabled = false;

	for_each_possible_cpu(cpu) {
		raw_spin_unlock(&cpu_rq(cpu)->lock);
	}
	read_unlock(&tasklist_lock);
	return 0;
}

static void walt_init_tg_pointers(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		walt_update_tg_pointer(css);
	rcu_read_unlock();
}

static void walt_init(struct work_struct *work)
{
	struct ctl_table_header *hdr;
	static atomic_t already_inited = ATOMIC_INIT(0);
	int i;

	might_sleep();

	if (atomic_cmpxchg(&already_inited, 0, 1))
		return;

	walt_tunables();

	register_syscore_ops(&walt_syscore_ops);
	BUG_ON(alloc_related_thread_groups());
	walt_init_cycle_counter();
	init_clusters();
	walt_init_tg_pointers();

	register_walt_hooks();
	walt_fixup_init();
	walt_lb_init();
	walt_rt_init();
	walt_cfs_init();
	walt_pause_init();

	stop_machine(walt_init_stop_handler, NULL, NULL);

	hdr = register_sysctl_table(walt_base_table);
	kmemleak_not_leak(hdr);

	input_boost_init();
	core_ctl_init();
	walt_boost_init();
	waltgov_register();

	i = match_string(sched_feat_names, __SCHED_FEAT_NR, "TTWU_QUEUE");
	if (i >= 0) {
		static_key_disable(&sched_feat_keys[i]);
		sysctl_sched_features &= ~(1UL << i);
	}
}

static DECLARE_WORK(walt_init_work, walt_init);
static void android_vh_update_topology_flags_workfn(void *unused, void *unused2)
{
	schedule_work(&walt_init_work);
}

#define WALT_VENDOR_DATA_SIZE_TEST(wstruct, kstruct)		\
	BUILD_BUG_ON(sizeof(wstruct) > (sizeof(u64) *		\
		ARRAY_SIZE(((kstruct *)0)->android_vendor_data1)))

static int walt_module_init(void)
{
	/* compile time checks for vendor data size */
	WALT_VENDOR_DATA_SIZE_TEST(struct walt_task_struct, struct task_struct);
	WALT_VENDOR_DATA_SIZE_TEST(struct walt_rq, struct rq);
	WALT_VENDOR_DATA_SIZE_TEST(struct walt_task_group, struct task_group);

	register_trace_android_vh_update_topology_flags_workfn(
			android_vh_update_topology_flags_workfn, NULL);

	if (topology_update_done)
		schedule_work(&walt_init_work);

	return 0;
}

module_init(walt_module_init);
MODULE_LICENSE("GPL v2");

#if IS_ENABLED(CONFIG_SCHED_WALT_DEBUG)
MODULE_SOFTDEP("pre: sched-walt-debug");
#endif
