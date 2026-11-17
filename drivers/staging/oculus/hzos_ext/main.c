// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta HzOS Ext Module
 *
 * Copyright (c) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/hzos_ext.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "bpf.h"
#include "features.h"
#include "flags.h"
#include "sched.h"
#include "mem.h"
#include "soft_rt.h"

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
#include <trace/hooks/sched.h>
#endif /* CONFIG_ANDROID_VENDOR_HOOKS */

/* After the vendor hooks, which define TRACE_INCLUDE_PATH without an #undef. */
#include "task_profiles_trace.h"

void hzos_ext_post_clone(void *unused, struct task_struct *new, struct task_struct *orig)
{
	/*
	 * Clear hzos_ext object for new processes. A new thread keeps its
	 * thread tag but not the process tag: that lives on the thread group
	 * leader, and a later exec from this thread must not promote a stale
	 * copy into the process tag. A new thread of a FORK_TG_NORMAL thread
	 * starts in tg_normal rather than its creator's thread group, so it
	 * drops the thread tag too.
	 */
	if (new->pid == new->tgid) {
		memset(&new->hzos_ext, 0, sizeof(new->hzos_ext));
	} else {
		struct hzos_ext_tag *tag = &new->hzos_ext.thread_profiles;

		memset(&new->hzos_ext.task_profiles, 0,
		       sizeof(new->hzos_ext.task_profiles));
		if (hzos_ext_task_has_flag(orig, HZOS_EXT_FLAG_FORK_TG_NORMAL))
			memset(tag, 0, sizeof(*tag));
		/*
		 * User space learns thread tags from this tracepoint, which a
		 * tag write only fires on a change, so report an inherited tag.
		 */
		if (tag->buf[0] != '\0')
			trace_hzos_ext_task_profiles_changed(HZOS_EXT_TAG_THREAD,
							     new->tgid, new->pid,
							     tag->gen);
	}

	mutex_init(&new->hzos_ext.lock);
	hzos_ext_sched_post_clone(new);
	hzos_ext_soft_rt_post_clone(new);
}

static int __init init_hzos_ext(void)
{
	pr_info("hzos_ext starting");

	hzos_ext_features_init();
	hzos_ext_bpf_init();
	hzos_ext_sched_init();
	hzos_ext_soft_rt_init();
	hzos_ext_mem_init();
	hzos_ext_task_profiles_init();

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
	register_trace_android_vh_post_clone(hzos_ext_post_clone, NULL);
#endif /* CONFIG_ANDROID_VENDOR_HOOKS */

	return 0;
}

subsys_initcall(init_hzos_ext);
