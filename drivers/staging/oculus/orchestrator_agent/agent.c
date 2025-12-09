// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta Orchestrator Agent Module
 *
 * Copyright (c) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "features.h"
#include "flags.h"
#include "sched.h"
#include "mem.h"

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
#include <trace/hooks/sched.h>
#endif /* CONFIG_ANDROID_VENDOR_HOOKS */

void orchestrator_post_clone(void *unused, struct task_struct *new, struct task_struct *orig)
{
	/* Clear orchestrator object for new processes */
	if (new->pid == new->tgid)
		memset(&new->orchestrator, 0, sizeof(new->orchestrator));

	mutex_init(&new->orchestrator.lock);
	orchestrator_sched_post_clone(new);
}

static int __init init_orchestrator(void)
{
	pr_info("Orchestrator Agent starting");

	orchestrator_features_init();
	orchestrator_sched_init();
	orchestrator_mem_init();

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
	register_trace_android_vh_post_clone(orchestrator_post_clone, NULL);
#endif /* CONFIG_ANDROID_VENDOR_HOOKS */

	return 0;
}

subsys_initcall(init_orchestrator);
