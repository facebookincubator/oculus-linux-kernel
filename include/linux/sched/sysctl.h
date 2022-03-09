#ifndef _SCHED_SYSCTL_H
#define _SCHED_SYSCTL_H

#ifdef CONFIG_DETECT_HUNG_TASK
extern int	     sysctl_hung_task_check_count;
extern unsigned int  sysctl_hung_task_panic;
extern unsigned long sysctl_hung_task_timeout_secs;
extern int sysctl_hung_task_warnings;
extern int sysctl_hung_task_selective_monitoring;
extern int proc_dohung_task_timeout_secs(struct ctl_table *table, int write,
					 void __user *buffer,
					 size_t *lenp, loff_t *ppos);
#else
/* Avoid need for ifdefs elsewhere in the code */
enum { sysctl_hung_task_timeout_secs = 0 };
#endif

extern unsigned int sysctl_sched_latency;
extern unsigned int sysctl_sched_min_granularity;
extern unsigned int sysctl_sched_wakeup_granularity;
extern unsigned int sysctl_sched_child_runs_first;
extern unsigned int sysctl_sched_is_big_little;
extern unsigned int sysctl_sched_sync_hint_enable;
extern unsigned int sysctl_sched_cstate_aware;
extern unsigned int sysctl_sched_capacity_margin;
extern unsigned int sysctl_sched_capacity_margin_down;

#ifdef CONFIG_SCHED_WALT
extern unsigned int sysctl_sched_init_task_load_pct;
extern unsigned int sysctl_sched_cpu_high_irqload;
extern unsigned int sysctl_sched_use_walt_cpu_util;
extern unsigned int sysctl_sched_use_walt_task_util;
extern unsigned int sysctl_sched_boost;
extern unsigned int sysctl_sched_group_upmigrate_pct;
extern unsigned int sysctl_sched_group_downmigrate_pct;
extern unsigned int sysctl_sched_walt_rotate_big_tasks;
extern unsigned int sysctl_sched_min_task_util_for_boost_colocation;
extern unsigned int sysctl_sched_little_cluster_coloc_fmin_khz;

extern int
walt_proc_update_handler(struct ctl_table *table, int write,
			 void __user *buffer, size_t *lenp,
			 loff_t *ppos);

#endif /* CONFIG_SCHED_WALT */

#if defined(CONFIG_PREEMPT_TRACER) || defined(CONFIG_IRQSOFF_TRACER)
extern unsigned int sysctl_preemptoff_tracing_threshold_ns;
extern unsigned int sysctl_irqsoff_tracing_threshold_ns;
#endif

enum sched_tunable_scaling {
	SCHED_TUNABLESCALING_NONE,
	SCHED_TUNABLESCALING_LOG,
	SCHED_TUNABLESCALING_LINEAR,
	SCHED_TUNABLESCALING_END,
};
extern enum sched_tunable_scaling sysctl_sched_tunable_scaling;

extern unsigned int sysctl_numa_balancing_scan_delay;
extern unsigned int sysctl_numa_balancing_scan_period_min;
extern unsigned int sysctl_numa_balancing_scan_period_max;
extern unsigned int sysctl_numa_balancing_scan_size;

#ifdef CONFIG_SCHED_DEBUG
extern __read_mostly unsigned int sysctl_sched_migration_cost;
extern __read_mostly unsigned int sysctl_sched_nr_migrate;
extern __read_mostly unsigned int sysctl_sched_time_avg;
extern unsigned int sysctl_sched_shares_window;

int sched_proc_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *length,
		loff_t *ppos);
#endif

extern int sched_migrate_notify_proc_handler(struct ctl_table *table,
		int write, void __user *buffer, size_t *lenp, loff_t *ppos);

extern int sched_boost_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos);

extern int sched_window_update_handler(struct ctl_table *table,
		 int write, void __user *buffer, size_t *lenp, loff_t *ppos);

/*
 *  control realtime throttling:
 *
 *  /proc/sys/kernel/sched_rt_period_us
 *  /proc/sys/kernel/sched_rt_runtime_us
 */
extern unsigned int sysctl_sched_rt_period;
extern int sysctl_sched_rt_runtime;

#ifdef CONFIG_CFS_BANDWIDTH
extern unsigned int sysctl_sched_cfs_bandwidth_slice;
#endif

#ifdef CONFIG_SCHED_TUNE
extern unsigned int sysctl_sched_cfs_boost;
int sysctl_sched_cfs_boost_handler(struct ctl_table *table, int write,
				   void __user *buffer, size_t *length,
				   loff_t *ppos);
static inline unsigned int get_sysctl_sched_cfs_boost(void)
{
	return sysctl_sched_cfs_boost;
}
#else
static inline unsigned int get_sysctl_sched_cfs_boost(void)
{
	return 0;
}
#endif

#ifdef CONFIG_SCHED_AUTOGROUP
extern unsigned int sysctl_sched_autogroup_enabled;
#endif

extern int sched_rr_timeslice;

extern int sched_rr_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos);

extern int sched_rt_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos);

extern int sched_updown_migrate_handler(struct ctl_table *table,
					int write, void __user *buffer,
					size_t *lenp, loff_t *ppos);

extern int sysctl_numa_balancing(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos);

extern int sysctl_schedstats(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos);

#ifdef CONFIG_SCHED_WALT
extern int sched_little_cluster_coloc_fmin_khz_handler(struct ctl_table *table,
					int write, void __user *buffer,
					size_t *lenp, loff_t *ppos);
#endif
#endif /* _SCHED_SYSCTL_H */
