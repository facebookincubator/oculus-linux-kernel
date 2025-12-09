#include <linux/cgroup.h>
#include <linux/cgroup-defs.h>
#include <linux/sched.h>

#include "sched.h"

static struct cpumask *tg_preferred_mask(struct task_group *tg)
{
	/*
	 * Cgroups may set a "preferred" cpumask that the CPU scheduler may use
	 * to inform how it chooses to schedule the task. For instance, tasks
	 * in a system-service cgroup may prefer to run on silver cores.
	 *
	 * In CPU schedulers, a task is generally preferred to run on either:
	 *
	 * 1. The "domain" (L3 cache, NUMA node, etc) it's currently *scheduled* in
	 * 2. The "domain" (L3 cache, NUMA node, etc) it's currently *assigned* to.
	 *
	 * In the default fair.c scheduler, the above two are the same. A task
	 * becomes "assigned" to a domain as soon as its load balanced.
	 * However, "scheduled in" and "assigned to" need not mean the same
	 * thing. Consider for instance that a task may want to *temporarily*
	 * run in another domain if that target domain has no work to do if it
	 * would otherwise suffer runqueue latency. Perhaps an application
	 * thread would like to run on a silver core rather than have to wait
	 * for a gold core. However, once it's run on the silver core and the
	 * traffic dies down on the gold core, it wants to go back to the gold
	 * core to take advantage of higher frequencies.
	 *
	 * This is what the preferred mask accomplishes. It's a hint to the
	 * fair scheduler, especially during task wakeup, for where a task
	 * should be migrated to and enqueued on.
	 *
	 * Note that we follow the mm philosophy here in that we don't validate
	 * parent cgroup preferred masks. If a user sets a preferred mask for a
	 * cgroup that conflicts (meaning it is not a subset) of the preferred
	 * mask of its parent, we simply let it happen. The preferred mask is a
	 * _hint_ to the scheduler, so we leave it to user space to provide
	 * logical hints.
	 */
	return &tg->android_vendor_data1;
}

static struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

struct cpumask *css_tg_preferred_mask(struct cgroup_subsys_state *css)
{
	return tg_preferred_mask(css_tg(css));
}

const struct cpumask *task_group_preferred_mask(struct task_struct *p)
{
	return tg_preferred_mask(task_group(p));
}
