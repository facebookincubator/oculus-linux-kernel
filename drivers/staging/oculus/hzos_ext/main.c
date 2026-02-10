// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta HzOS Ext Module
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

void hzos_ext_post_clone(void *unused, struct task_struct *new, struct task_struct *orig)
{
	/* Clear hzos_ext object for new processes */
	if (new->pid == new->tgid)
		memset(&new->hzos_ext, 0, sizeof(new->hzos_ext));

	mutex_init(&new->hzos_ext.lock);
	hzos_ext_sched_post_clone(new);
}

static int __init init_hzos_ext(void)
{
	pr_info("hzos_ext starting");

	hzos_ext_features_init();
	hzos_ext_sched_init();
	hzos_ext_mem_init();

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
	register_trace_android_vh_post_clone(hzos_ext_post_clone, NULL);
#endif /* CONFIG_ANDROID_VENDOR_HOOKS */

	return 0;
}

subsys_initcall(init_hzos_ext);
