#ifndef SCHED_ORCHESTRATOR_H
#define SCHED_ORCHESTRATOR_H

#ifdef CONFIG_ORCHESTRATOR_AGENT
void orchestrator_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask);
void orchestrator_select_task_rq_fair(struct task_struct *p, int prev_cpu,
									  int sd_flag, int wake_flags,
									  int *target_cpu);
void orchestrator_enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
void orchestrator_dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
#else
static inline void orchestrator_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask) {}
static inline void orchestrator_select_task_rq_fair(struct task_struct *p, int prev_cpu,
									  int sd_flag, int wake_flags,
									  int *target_cpu) {}
static inline void orchestrator_enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se) {}
static inline void orchestrator_dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se) {}
#endif

#endif /* SCHED_ORCHESTRATOR_H */
