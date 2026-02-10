#ifndef SCHED_HZOS_EXT_H
#define SCHED_HZOS_EXT_H

#ifdef CONFIG_HZOS_EXT
void hzos_ext_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask);
void hzos_ext_select_task_rq_fair(struct task_struct *p, int prev_cpu,
									  int sd_flag, int wake_flags,
									  int *target_cpu);
void hzos_ext_enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
void hzos_ext_dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
#else
static inline void hzos_ext_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask) {}
static inline void hzos_ext_select_task_rq_fair(struct task_struct *p, int prev_cpu,
									  int sd_flag, int wake_flags,
									  int *target_cpu) {}
static inline void hzos_ext_enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se) {}
static inline void hzos_ext_dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se) {}
#endif

#endif /* SCHED_HZOS_EXT_H */
