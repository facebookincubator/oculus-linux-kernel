// SPDX-License-Identifier: GPL-2.0-only
/*
 *  kernel/sched/orchestrator.c
 *
 *  Meta-specific scheduler logic.
 *
 *  Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/cgroup.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <trace/hooks/sched.h>

#include "sched.h"
#include "orchestrator.h"

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

static DEFINE_MUTEX(preferred_mask_mutex);
static ssize_t sched_group_set_preferred_mask(struct task_group *tg,
					      const struct cpumask *preferred)
{
	struct task_struct *p;
	struct css_task_iter it;
	unsigned long flags;

	/* We can't change the preferred mask of the root cgroup */
	if (!tg->se[0])
		return -EINVAL;

	css_task_iter_start(&tg->css, 0, &it);
	mutex_lock(&preferred_mask_mutex);
	cpumask_copy(tg_preferred_mask(tg), preferred);
	while ((p = css_task_iter_next(&it))) {
		raw_spin_lock_irqsave(&p->pi_lock, flags);
		trace_android_vh_sched_tg_setpreferred(p, preferred);
		raw_spin_unlock_irqrestore(&p->pi_lock, flags);
	}
	mutex_unlock(&preferred_mask_mutex);
	css_task_iter_end(&it);

	return 0;
}

ssize_t preferred_mask_write(struct kernfs_open_file *of, char *buf,
			     size_t nbytes, loff_t off)
{
	struct task_group *tg;
	ssize_t err;
	cpumask_var_t new_mask;

	if (!alloc_cpumask_var(&new_mask, GFP_KERNEL))
		return -ENOMEM;

	tg = css_tg(of_css(of));
	err = cpumask_parse(buf, new_mask);
	if (err)
		goto free_mask;

	err = sched_group_set_preferred_mask(tg, new_mask);
free_mask:
	free_cpumask_var(new_mask);
	return err ?: nbytes;
}

int preferred_mask_read(struct seq_file *sf, void *v)
{
	char *kbuf = kmalloc(cpumask_size() + 1, GFP_KERNEL);
	struct task_group *tg;

	if (!kbuf)
		return -ENOMEM;

	tg = css_tg(seq_css(sf));
	cpumap_print_to_pagebuf(false, kbuf, tg_preferred_mask(tg));
	seq_printf(sf, "0x%s\n", kbuf);
	kfree(kbuf);

	return 0;
}

const struct cpumask *task_group_preferred_mask(struct task_struct *p)
{
	return tg_preferred_mask(task_group(p));
}
