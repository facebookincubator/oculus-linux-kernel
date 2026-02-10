#ifndef _LINUX_HZOS_EXT_KERN_H
#define _LINUX_HZOS_EXT_KERN_H

#include <linux/cpumask.h>
#include <linux/cgroup-defs.h>
#include <linux/sched.h>

/**
 * tg_preferred_mask - Get the preferred mask that is stashed in a task group.
 *
 * @tg: The task group to get the preferred mask for.
 */
struct cpumask *tg_preferred_mask(struct task_group *tg);

/**
 * task_group_preferred_mask - Get the preferred mask for the task_group of the specified task.
 *
 * @p: The task to get the preferred mask for.
 */
const struct cpumask *task_group_preferred_mask(struct task_struct *p);

/**
 * css_tg_preferred_mask - Get the preferred mask for the task_group of the specified cgroup.
 *
 * @css: The cgroup to get the preferred mask for.
 */
struct cpumask *css_tg_preferred_mask(struct cgroup_subsys_state *css);

#endif /* _LINUX_HZOS_EXT_KERN_H */
