// SPDX-License-Identifier: GPL-2.0-only
/*
 * Meta Orchestrator Agent Module
 *
 * Copyright (c) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string_helpers.h>
#include <linux/uaccess.h>
#include <linux/orchestrator.h>

#include <uapi/linux/sched/types.h>

#include "flags.h"

#define MAX_FLAGS_LEN 64

ssize_t orchestrator_flags_read_procfs(struct task_struct *task,
				       char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	size_t remaining = MAX_FLAGS_LEN;
	int i;
	bool had_prior = false;
	char tmpbuf[MAX_FLAGS_LEN];
	char *kbuf = tmpbuf;

	for (i = 0; i < ORCHESTRATOR_NUM_FLAGS && remaining > 0; i++) {
		ssize_t flag_str_len;
		u64 flag = orchestrator_idx_to_flag(i);
		const char *prefix, *prior;

		if (READ_ONCE(task->orchestrator_flags) & flag)
			prefix = "";
		else
			prefix = "NO_";

		if (had_prior)
			prior = " ";
		else
			prior = "";

		flag_str_len = scnprintf(kbuf, remaining, "%s%s%s",
					 prior, prefix, orchestrator_flag_to_str(flag));

		kbuf += flag_str_len;
		remaining -= flag_str_len;
		had_prior = true;
	}

	return simple_read_from_buffer(ubuf, count, ppos, tmpbuf, MAX_FLAGS_LEN);
}

static DEFINE_MUTEX(flags_write_mutex);
ssize_t orchestrator_flags_write_procfs(struct task_struct *task,
				        const char __user *ubuf,
				        size_t count, loff_t *ppos)
{
	char tmpbuf[MAX_FLAGS_LEN];
	enum orchestrator_flags flag;
	bool off;
	char *kbuf;
	size_t len = MAX_FLAGS_LEN;

	/* No partial writes. */
	if (*ppos != 0)
		return -EINVAL;

	/* Shouldn't exceed maxmum flag length */
	if (count > MAX_FLAGS_LEN || count <= 3)
		return -EINVAL;

	/* If invalid user buffer, return */
	memset(tmpbuf, 0, MAX_FLAGS_LEN);
	if (copy_from_user(tmpbuf, ubuf, count))
		return -EFAULT;
	kbuf = strstrip(tmpbuf);

	off = !strncmp(kbuf, "NO_", 3);
	if (off) {
		kbuf += 3;
		len -= 3;
	}

	flag = orchestrator_str_to_flag(kbuf, len);
	if (!flag)
		return -ENOENT;

	/* Ensure writes are consistent */
	mutex_lock(&flags_write_mutex);
	if (off)
		WRITE_ONCE(task->orchestrator_flags,
			   (task->orchestrator_flags & ~flag));
	else
		WRITE_ONCE(task->orchestrator_flags,
			   (task->orchestrator_flags | flag));
	mutex_unlock(&flags_write_mutex);
	return count;
}

int orchestrator_task_setscheduler(struct task_struct *task,
			           const struct sched_attr *attr)
{
	int policy = attr->sched_policy;

	if ((policy == SCHED_FIFO || policy == SCHED_RR) &&
	    !orchestrator_task_has_flag(task, ORCHESTRATOR_FLAG_ALLOW_RT)) {
		bool is_service = orchestrator_task_has_flag(task, ORCHESTRATOR_FLAG_IS_SERVICE);
		const char *str = is_service ? "rejecting" : "but allowing";

		pr_info("%s[%d] ALLOW_RT permission denied, %s",
			task->comm, task->pid, str);
		/*
		 * For now, only prevent services from using RT as we only have
		 * the init.rc files to configure whether they can or can't use
		 * RT. Once the Orchestrator service is up and running, we can
		 * tighten this restriction (and possibly remove the IS_SERVICE
		 * flag).
		 */
		if (is_service)
			return -EPERM;
		else
			return 0;
	}

	return 0;
}

void orchestrator_post_clone(struct task_struct *new, struct task_struct *orig)
{
	/* Clear flags for new processes */
	if (new->pid == new->tgid)
		WRITE_ONCE(new->orchestrator_flags, 0);
}

static int __init init_orchestrator(void)
{
	pr_info("Orchestrator Agent starting");

	return 0;
}

subsys_initcall(init_orchestrator);
