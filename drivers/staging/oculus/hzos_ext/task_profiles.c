// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Meta Platforms, Inc. and affiliates
 */

#include <linux/hash.h>
#include <linux/hzos_ext.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

/*
 * Watchers park on a hashed table of wait queues owned by this module rather
 * than on a wait queue embedded in the task struct.
 */
#define TASK_PROFILES_WQ_BITS	6
#define TASK_PROFILES_WQ_SIZE	(1 << TASK_PROFILES_WQ_BITS)

static wait_queue_head_t task_profiles_wq[TASK_PROFILES_WQ_SIZE];

static wait_queue_head_t *task_profiles_waitqueue(struct task_struct *task)
{
	return &task_profiles_wq[hash_ptr(task, TASK_PROFILES_WQ_BITS)];
}

void hzos_ext_task_profiles_init(void)
{
	int i;

	for (i = 0; i < TASK_PROFILES_WQ_SIZE; i++)
		init_waitqueue_head(&task_profiles_wq[i]);
}

ssize_t hzos_ext_task_profiles_read_procfs(struct task_struct *task,
					   char __user *ubuf,
					   size_t count, loff_t *ppos,
					   u32 *seen_gen)
{
	char tmpbuf[HZOS_EXT_TASK_PROFILES_LEN + 1];
	int len;
	u32 gen;

	task_lock(task);
	gen = task->hzos_ext.task_profiles_gen;
	len = scnprintf(tmpbuf, sizeof(tmpbuf), "%s\n",
			task->hzos_ext.task_profiles);
	task_unlock(task);

	WRITE_ONCE(*seen_gen, gen);

	return simple_read_from_buffer(ubuf, count, ppos, tmpbuf, len);
}

ssize_t hzos_ext_task_profiles_write_procfs(struct task_struct *task,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	char tmpbuf[HZOS_EXT_TASK_PROFILES_LEN];
	const char *profiles;
	bool changed;

	/* No partial writes. */
	if (*ppos != 0)
		return -EINVAL;

	if (count >= sizeof(tmpbuf))
		return -EINVAL;

	memset(tmpbuf, 0, sizeof(tmpbuf));
	if (copy_from_user(tmpbuf, ubuf, count))
		return -EFAULT;

	profiles = strstrip(tmpbuf);

	task_lock(task);
	changed = strcmp(task->hzos_ext.task_profiles, profiles) != 0;
	if (changed) {
		strscpy(task->hzos_ext.task_profiles, profiles,
			sizeof(task->hzos_ext.task_profiles));
		WRITE_ONCE(task->hzos_ext.task_profiles_gen,
			   task->hzos_ext.task_profiles_gen + 1);
	}
	task_unlock(task);

	if (changed)
		wake_up_interruptible_poll(task_profiles_waitqueue(task),
					   EPOLLPRI);

	return count;
}

__poll_t hzos_ext_task_profiles_poll_procfs(struct task_struct *task,
					    struct file *file,
					    struct poll_table_struct *wait,
					    u32 *seen_gen)
{
	u32 gen;

	poll_wait(file, task_profiles_waitqueue(task), wait);

	gen = READ_ONCE(task->hzos_ext.task_profiles_gen);
	if (READ_ONCE(*seen_gen) == gen)
		return 0;

	return EPOLLPRI;
}
