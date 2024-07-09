/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifdef CONFIG_SCHED_WALT
struct rq;
struct group_cpu_time;
extern const char *task_event_names[];

#if defined(CREATE_TRACE_POINTS) && defined(CONFIG_SCHED_WALT)
static inline void __window_data(u32 *dst, u32 *src)
{
	if (src)
		memcpy(dst, src, nr_cpu_ids * sizeof(u32));
	else
		memset(dst, 0, nr_cpu_ids * sizeof(u32));
}

struct trace_seq;
const char *__window_print(struct trace_seq *p, const u32 *buf, int buf_len)
{
	int i;
	const char *ret = p->buffer + seq_buf_used(&p->seq);

	for (i = 0; i < buf_len; i++)
		trace_seq_printf(p, "%u ", buf[i]);

	trace_seq_putc(p, 0);

	return ret;
}

static inline s64 __rq_update_sum(struct rq *rq, bool curr, bool new)
{
	if (curr)
		if (new)
			return rq->nt_curr_runnable_sum;
		else
			return rq->curr_runnable_sum;
	else
		if (new)
			return rq->nt_prev_runnable_sum;
		else
			return rq->prev_runnable_sum;
}

static inline s64 __grp_update_sum(struct rq *rq, bool curr, bool new)
{
	if (curr)
		if (new)
			return rq->grp_time.nt_curr_runnable_sum;
		else
			return rq->grp_time.curr_runnable_sum;
	else
		if (new)
			return rq->grp_time.nt_prev_runnable_sum;
		else
			return rq->grp_time.prev_runnable_sum;
}

static inline s64
__get_update_sum(struct rq *rq, enum migrate_types migrate_type,
		 bool src, bool new, bool curr)
{
	switch (migrate_type) {
	case RQ_TO_GROUP:
		if (src)
			return __rq_update_sum(rq, curr, new);
		else
			return __grp_update_sum(rq, curr, new);
	case GROUP_TO_RQ:
		if (src)
			return __grp_update_sum(rq, curr, new);
		else
			return __rq_update_sum(rq, curr, new);
	default:
		WARN_ON_ONCE(1);
		return -1;
	}
}
#endif

TRACE_EVENT(sched_update_pred_demand,

	TP_PROTO(struct task_struct *p, u32 runtime, int pct,
		 unsigned int pred_demand),

	TP_ARGS(p, runtime, pct, pred_demand),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned int,	runtime)
		__field(int,		pct)
		__field(unsigned int,	pred_demand)
		__array(u8,		bucket, NUM_BUSY_BUCKETS)
		__field(int,		cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->runtime        = runtime;
		__entry->pct            = pct;
		__entry->pred_demand     = pred_demand;
		memcpy(__entry->bucket, p->ravg.busy_buckets,
					NUM_BUSY_BUCKETS * sizeof(u8));
		__entry->cpu            = task_cpu(p);
	),

	TP_printk("%d (%s): runtime %u pct %d cpu %d pred_demand %u (buckets: %u %u %u %u %u %u %u %u %u %u)",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->pct, __entry->cpu,
		__entry->pred_demand, __entry->bucket[0], __entry->bucket[1],
		__entry->bucket[2], __entry->bucket[3], __entry->bucket[4],
		__entry->bucket[5], __entry->bucket[6], __entry->bucket[7],
		__entry->bucket[8], __entry->bucket[9])
);

TRACE_EVENT(sched_update_history,

	TP_PROTO(struct rq *rq, struct task_struct *p, u32 runtime, int samples,
			enum task_event evt),

	TP_ARGS(rq, p, runtime, samples, evt),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(unsigned int,		runtime)
		__field(int,			samples)
		__field(enum task_event,	evt)
		__field(unsigned int,		demand)
		__field(unsigned int,		coloc_demand)
		__field(unsigned int,		pred_demand)
		__array(u32,			hist, RAVG_HIST_SIZE_MAX)
		__field(unsigned int,		nr_big_tasks)
		__field(int,			cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->runtime        = runtime;
		__entry->samples        = samples;
		__entry->evt            = evt;
		__entry->demand         = p->ravg.demand;
		__entry->coloc_demand   = p->ravg.coloc_demand;
		__entry->pred_demand     = p->ravg.pred_demand;
		memcpy(__entry->hist, p->ravg.sum_history,
					RAVG_HIST_SIZE_MAX * sizeof(u32));
		__entry->nr_big_tasks   = rq->walt_stats.nr_big_tasks;
		__entry->cpu            = rq->cpu;
	),

	TP_printk("%d (%s): runtime %u samples %d event %s demand %u coloc_demand %u pred_demand %u (hist: %u %u %u %u %u) cpu %d nr_big %u",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->samples,
		task_event_names[__entry->evt],
		__entry->demand, __entry->coloc_demand, __entry->pred_demand,
		__entry->hist[0], __entry->hist[1],
		__entry->hist[2], __entry->hist[3],
		__entry->hist[4], __entry->cpu, __entry->nr_big_tasks)
);

TRACE_EVENT(sched_get_task_cpu_cycles,

	TP_PROTO(int cpu, int event, u64 cycles,
			u64 exec_time, struct task_struct *p),

	TP_ARGS(cpu, event, cycles, exec_time, p),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(int,	event)
		__field(u64,	cycles)
		__field(u64,	exec_time)
		__field(u32,	freq)
		__field(u32,	legacy_freq)
		__field(u32,	max_freq)
		__field(pid_t,	pid)
		__array(char,	comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->event		= event;
		__entry->cycles		= cycles;
		__entry->exec_time	= exec_time;
		__entry->freq		= cpu_cycles_to_freq(cycles, exec_time);
		__entry->legacy_freq	= sched_cpu_legacy_freq(cpu);
		__entry->max_freq	= cpu_max_freq(cpu);
		__entry->pid		= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
	),

	TP_printk("cpu=%d event=%d cycles=%llu exec_time=%llu freq=%u legacy_freq=%u max_freq=%u task=%d (%s)",
		  __entry->cpu, __entry->event, __entry->cycles,
		  __entry->exec_time, __entry->freq, __entry->legacy_freq,
		  __entry->max_freq, __entry->pid, __entry->comm)
);

TRACE_EVENT(sched_update_task_ravg,

	TP_PROTO(struct task_struct *p, struct rq *rq, enum task_event evt,
		 u64 wallclock, u64 irqtime,
		 struct group_cpu_time *cpu_time),

	TP_ARGS(p, rq, evt, wallclock, irqtime, cpu_time),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(pid_t,			cur_pid)
		__field(unsigned int,		cur_freq)
		__field(u64,			wallclock)
		__field(u64,			mark_start)
		__field(u64,			delta_m)
		__field(u64,			win_start)
		__field(u64,			delta)
		__field(u64,			irqtime)
		__field(enum task_event,	evt)
		__field(unsigned int,		demand)
		__field(unsigned int,		coloc_demand)
		__field(unsigned int,		sum)
		__field(int,			cpu)
		__field(unsigned int,		pred_demand)
		__field(u64,			rq_cs)
		__field(u64,			rq_ps)
		__field(u64,			grp_cs)
		__field(u64,			grp_ps)
		__field(u64,			grp_nt_cs)
		__field(u64,			grp_nt_ps)
		__field(u32,			curr_window)
		__field(u32,			prev_window)
		__dynamic_array(u32,		curr_sum, nr_cpu_ids)
		__dynamic_array(u32,		prev_sum, nr_cpu_ids)
		__field(u64,			nt_cs)
		__field(u64,			nt_ps)
		__field(u64,			active_time)
		__field(u32,			curr_top)
		__field(u32,			prev_top)
	),

	TP_fast_assign(
		__entry->wallclock      = wallclock;
		__entry->win_start      = rq->window_start;
		__entry->delta          = (wallclock - rq->window_start);
		__entry->evt            = evt;
		__entry->cpu            = rq->cpu;
		__entry->cur_pid        = rq->curr->pid;
		__entry->cur_freq       = rq->task_exec_scale;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->mark_start     = p->ravg.mark_start;
		__entry->delta_m        = (wallclock - p->ravg.mark_start);
		__entry->demand         = p->ravg.demand;
		__entry->coloc_demand	= p->ravg.coloc_demand;
		__entry->sum            = p->ravg.sum;
		__entry->irqtime        = irqtime;
		__entry->pred_demand     = p->ravg.pred_demand;
		__entry->rq_cs          = rq->curr_runnable_sum;
		__entry->rq_ps          = rq->prev_runnable_sum;
		__entry->grp_cs = cpu_time ? cpu_time->curr_runnable_sum : 0;
		__entry->grp_ps = cpu_time ? cpu_time->prev_runnable_sum : 0;
		__entry->grp_nt_cs = cpu_time ?
					cpu_time->nt_curr_runnable_sum : 0;
		__entry->grp_nt_ps = cpu_time ?
					cpu_time->nt_prev_runnable_sum : 0;
		__entry->curr_window	= p->ravg.curr_window;
		__entry->prev_window	= p->ravg.prev_window;
		__window_data(__get_dynamic_array(curr_sum),
						p->ravg.curr_window_cpu);
		__window_data(__get_dynamic_array(prev_sum),
						p->ravg.prev_window_cpu);
		__entry->nt_cs		= rq->nt_curr_runnable_sum;
		__entry->nt_ps		= rq->nt_prev_runnable_sum;
		__entry->active_time	= p->ravg.active_time;
		__entry->curr_top	= rq->curr_top;
		__entry->prev_top	= rq->prev_top;
	),

	    TP_printk("wc %llu ws %llu delta %llu event %s cpu %d cur_freq %u cur_pid %d task %d (%s) ms %llu delta %llu demand %u coloc_demand: %u sum %u irqtime %llu pred_demand %u rq_cs %llu rq_ps %llu cur_window %u (%s) prev_window %u (%s) nt_cs %llu nt_ps %llu active_time %u grp_cs %lld grp_ps %lld, grp_nt_cs %llu, grp_nt_ps: %llu curr_top %u prev_top %u",
		__entry->wallclock, __entry->win_start, __entry->delta,
		task_event_names[__entry->evt], __entry->cpu,
		__entry->cur_freq, __entry->cur_pid,
		__entry->pid, __entry->comm, __entry->mark_start,
		__entry->delta_m, __entry->demand, __entry->coloc_demand,
		__entry->sum, __entry->irqtime, __entry->pred_demand,
		__entry->rq_cs, __entry->rq_ps, __entry->curr_window,
		__window_print(p, __get_dynamic_array(curr_sum), nr_cpu_ids),
		__entry->prev_window,
		__window_print(p, __get_dynamic_array(prev_sum), nr_cpu_ids),
		__entry->nt_cs, __entry->nt_ps,
		__entry->active_time, __entry->grp_cs,
		__entry->grp_ps, __entry->grp_nt_cs, __entry->grp_nt_ps,
		__entry->curr_top, __entry->prev_top)
);

TRACE_EVENT(sched_update_task_ravg_mini,

	TP_PROTO(struct task_struct *p, struct rq *rq, enum task_event evt,
		 u64 wallclock, u64 irqtime,
		 struct group_cpu_time *cpu_time),

	TP_ARGS(p, rq, evt, wallclock, irqtime, cpu_time),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(u64,			wallclock)
		__field(u64,			mark_start)
		__field(u64,			delta_m)
		__field(u64,			win_start)
		__field(u64,			delta)
		__field(enum task_event,	evt)
		__field(unsigned int,		demand)
		__field(int,			cpu)
		__field(u64,			rq_cs)
		__field(u64,			rq_ps)
		__field(u64,			grp_cs)
		__field(u64,			grp_ps)
		__field(u32,			curr_window)
		__field(u32,			prev_window)
	),

	TP_fast_assign(
		__entry->wallclock      = wallclock;
		__entry->win_start      = rq->window_start;
		__entry->delta          = (wallclock - rq->window_start);
		__entry->evt            = evt;
		__entry->cpu            = rq->cpu;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->mark_start     = p->ravg.mark_start;
		__entry->delta_m        = (wallclock - p->ravg.mark_start);
		__entry->demand         = p->ravg.demand;
		__entry->rq_cs          = rq->curr_runnable_sum;
		__entry->rq_ps          = rq->prev_runnable_sum;
		__entry->grp_cs = cpu_time ? cpu_time->curr_runnable_sum : 0;
		__entry->grp_ps = cpu_time ? cpu_time->prev_runnable_sum : 0;
		__entry->curr_window	= p->ravg.curr_window;
		__entry->prev_window	= p->ravg.prev_window;
	),

	TP_printk("wc %llu ws %llu delta %llu event %s cpu %d task %d (%s) ms %llu delta %llu demand %u rq_cs %llu rq_ps %llu cur_window %u prev_window %u grp_cs %lld grp_ps %lld",
		__entry->wallclock, __entry->win_start, __entry->delta,
		task_event_names[__entry->evt], __entry->cpu,
		__entry->pid, __entry->comm, __entry->mark_start,
		__entry->delta_m, __entry->demand,
		__entry->rq_cs, __entry->rq_ps, __entry->curr_window,
		__entry->prev_window, __entry->grp_cs, __entry->grp_ps)
);

struct migration_sum_data;
extern const char *migrate_type_names[];

TRACE_EVENT(sched_set_preferred_cluster,

	TP_PROTO(struct related_thread_group *grp, u64 total_demand),

	TP_ARGS(grp, total_demand),

	TP_STRUCT__entry(
		__field(int,		id)
		__field(u64,		total_demand)
		__field(bool,		skip_min)
	),

	TP_fast_assign(
		__entry->id			= grp->id;
		__entry->total_demand		= total_demand;
		__entry->skip_min		= grp->skip_min;
	),

	TP_printk("group_id %d total_demand %llu skip_min %d",
			__entry->id, __entry->total_demand,
			__entry->skip_min)
);

TRACE_EVENT(sched_migration_update_sum,

	TP_PROTO(struct task_struct *p, enum migrate_types migrate_type,
							struct rq *rq),

	TP_ARGS(p, migrate_type, rq),

	TP_STRUCT__entry(
		__field(int,			tcpu)
		__field(int,			pid)
		__field(enum migrate_types,	migrate_type)
		__field(s64,			src_cs)
		__field(s64,			src_ps)
		__field(s64,			dst_cs)
		__field(s64,			dst_ps)
		__field(s64,			src_nt_cs)
		__field(s64,			src_nt_ps)
		__field(s64,			dst_nt_cs)
		__field(s64,			dst_nt_ps)
	),

	TP_fast_assign(
		__entry->tcpu		= task_cpu(p);
		__entry->pid		= p->pid;
		__entry->migrate_type	= migrate_type;
		__entry->src_cs		= __get_update_sum(rq, migrate_type,
							   true, false, true);
		__entry->src_ps		= __get_update_sum(rq, migrate_type,
							   true, false, false);
		__entry->dst_cs		= __get_update_sum(rq, migrate_type,
							   false, false, true);
		__entry->dst_ps		= __get_update_sum(rq, migrate_type,
							   false, false, false);
		__entry->src_nt_cs	= __get_update_sum(rq, migrate_type,
							   true, true, true);
		__entry->src_nt_ps	= __get_update_sum(rq, migrate_type,
							   true, true, false);
		__entry->dst_nt_cs	= __get_update_sum(rq, migrate_type,
							   false, true, true);
		__entry->dst_nt_ps	= __get_update_sum(rq, migrate_type,
							   false, true, false);
	),

	TP_printk("pid %d task_cpu %d migrate_type %s src_cs %llu src_ps %llu dst_cs %lld dst_ps %lld src_nt_cs %llu src_nt_ps %llu dst_nt_cs %lld dst_nt_ps %lld",
		__entry->pid, __entry->tcpu,
		migrate_type_names[__entry->migrate_type],
		__entry->src_cs, __entry->src_ps, __entry->dst_cs,
		__entry->dst_ps, __entry->src_nt_cs, __entry->src_nt_ps,
		__entry->dst_nt_cs, __entry->dst_nt_ps)
);

TRACE_EVENT(sched_set_boost,

	TP_PROTO(int type),

	TP_ARGS(type),

	TP_STRUCT__entry(
		__field(int, type)
	),

	TP_fast_assign(
		__entry->type = type;
	),

	TP_printk("type %d", __entry->type)
);

TRACE_EVENT(sched_load_balance_skip_tasks,

	TP_PROTO(int scpu, int dcpu, int grp_type, int pid,
		unsigned long h_load, unsigned long task_util,
		unsigned long affinity),

	TP_ARGS(scpu, dcpu, grp_type, pid, h_load, task_util, affinity),

	TP_STRUCT__entry(
		__field(int,		scpu)
		__field(unsigned long,	src_util_cum)
		__field(int,		grp_type)
		__field(int,		dcpu)
		__field(unsigned long,	dst_util_cum)
		__field(int,		pid)
		__field(unsigned long,	affinity)
		__field(unsigned long,	task_util)
		__field(unsigned long,	h_load)
	),

	TP_fast_assign(
		__entry->scpu		= scpu;
		__entry->src_util_cum	=
					cpu_rq(scpu)->cum_window_demand_scaled;
		__entry->grp_type	= grp_type;
		__entry->dcpu		= dcpu;
		__entry->dst_util_cum	=
					cpu_rq(dcpu)->cum_window_demand_scaled;
		__entry->pid		= pid;
		__entry->affinity	= affinity;
		__entry->task_util	= task_util;
		__entry->h_load		= h_load;
	),

	TP_printk("source_cpu=%d util_cum=%lu group_type=%d dest_cpu=%d util_cum=%lu pid=%d affinity=%#lx task_util=%lu task_h_load=%lu",
		__entry->scpu, __entry->src_util_cum, __entry->grp_type,
		__entry->dcpu, __entry->dst_util_cum, __entry->pid,
		__entry->affinity, __entry->task_util, __entry->h_load)
);

TRACE_EVENT(sched_load_to_gov,

	TP_PROTO(struct rq *rq, u64 aggr_grp_load, u32 tt_load,
		int freq_aggr, u64 load, int policy,
		int big_task_rotation,
		unsigned int user_hint),
	TP_ARGS(rq, aggr_grp_load, tt_load, freq_aggr, load, policy,
		big_task_rotation, user_hint),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(int,    policy)
		__field(int,	ed_task_pid)
		__field(u64,    aggr_grp_load)
		__field(int,    freq_aggr)
		__field(u64,    tt_load)
		__field(u64,	rq_ps)
		__field(u64,	grp_rq_ps)
		__field(u64,	nt_ps)
		__field(u64,	grp_nt_ps)
		__field(u64,	pl)
		__field(u64,    load)
		__field(int,    big_task_rotation)
		__field(unsigned int, user_hint)
	),

	TP_fast_assign(
		__entry->cpu		= cpu_of(rq);
		__entry->policy		= policy;
		__entry->ed_task_pid	= rq->ed_task ? rq->ed_task->pid : -1;
		__entry->aggr_grp_load	= aggr_grp_load;
		__entry->freq_aggr	= freq_aggr;
		__entry->tt_load	= tt_load;
		__entry->rq_ps		= rq->prev_runnable_sum;
		__entry->grp_rq_ps	= rq->grp_time.prev_runnable_sum;
		__entry->nt_ps		= rq->nt_prev_runnable_sum;
		__entry->grp_nt_ps	= rq->grp_time.nt_prev_runnable_sum;
		__entry->pl		=
					rq->walt_stats.pred_demands_sum_scaled;
		__entry->load		= load;
		__entry->big_task_rotation = big_task_rotation;
		__entry->user_hint = user_hint;
	),

	TP_printk("cpu=%d policy=%d ed_task_pid=%d aggr_grp_load=%llu freq_aggr=%d tt_load=%llu rq_ps=%llu grp_rq_ps=%llu nt_ps=%llu grp_nt_ps=%llu pl=%llu load=%llu big_task_rotation=%d user_hint=%u",
		__entry->cpu, __entry->policy, __entry->ed_task_pid,
		__entry->aggr_grp_load, __entry->freq_aggr,
		__entry->tt_load, __entry->rq_ps, __entry->grp_rq_ps,
		__entry->nt_ps, __entry->grp_nt_ps, __entry->pl, __entry->load,
		__entry->big_task_rotation, __entry->user_hint)
);

TRACE_EVENT(walt_window_rollover,

	TP_PROTO(u64 window_start),

	TP_ARGS(window_start),

	TP_STRUCT__entry(
		__field(u64, window_start)
	),

	TP_fast_assign(
		__entry->window_start = window_start;
	),

	TP_printk("window_start=%llu", __entry->window_start)
);

#endif
