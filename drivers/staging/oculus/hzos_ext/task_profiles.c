// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Meta Platforms, Inc. and affiliates
 */

#include <linux/capability.h>
#include <linux/hash.h>
#include <linux/hzos_ext.h>
#include <linux/hzos_ext_tag.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/user_namespace.h>
#include <linux/wait.h>

#define CREATE_TRACE_POINTS
#include "task_profiles_trace.h"

/*
 * Watchers park on a hashed table of wait queues owned by this module rather
 * than on a wait queue embedded in the task struct.
 */
#define TASK_PROFILES_WQ_BITS	6
#define TASK_PROFILES_WQ_SIZE	(1 << TASK_PROFILES_WQ_BITS)

static wait_queue_head_t task_profiles_wq[TASK_PROFILES_WQ_SIZE];

static wait_queue_head_t *task_profiles_waitqueue(struct hzos_ext_tag *slot)
{
	return &task_profiles_wq[hash_ptr(slot, TASK_PROFILES_WQ_BITS)];
}

/*
 * Pin the task holding @task's tag of @kind. Release with put_task_struct().
 * Returns NULL once @task has been reaped.
 *
 * The caller's reference keeps @task itself valid, but not its group leader:
 * a reaped thread can still point at a leader that has since been released.
 * While @task is hashed (pid_alive()), its leader cannot be released without
 * a grace period that starts after this read, so the leader stays valid for
 * the rest of the RCU read-side section.
 */
static struct task_struct *tag_owner_get(struct task_struct *task,
					 enum hzos_ext_tag_kind kind)
{
	struct task_struct *owner;

	rcu_read_lock();
	owner = pid_alive(task) ? hzos_ext_tag_owner_of(task, kind) : NULL;
	if (owner)
		get_task_struct(owner);
	rcu_read_unlock();

	return owner;
}

bool hzos_ext_tag_write_allowed(void)
{
	return ns_capable_noaudit(&init_user_ns, CAP_SYS_NICE) ||
	       ns_capable_noaudit(&init_user_ns, CAP_SYS_ADMIN);
}

void hzos_ext_task_profiles_init(void)
{
	int i;

	for (i = 0; i < TASK_PROFILES_WQ_SIZE; i++)
		init_waitqueue_head(&task_profiles_wq[i]);
}

static ssize_t tag_read(struct task_struct *task, enum hzos_ext_tag_kind kind,
			char __user *ubuf, size_t count, loff_t *ppos,
			u32 *seen_gen)
{
	char tmpbuf[HZOS_EXT_TASK_PROFILES_LEN + 1];
	struct task_struct *owner;
	struct hzos_ext_tag *slot;
	int len;
	u32 gen;

	owner = tag_owner_get(task, kind);
	if (!owner)
		return -ESRCH;
	slot = hzos_ext_tag_slot(owner, kind);
	task_lock(owner);
	gen = slot->gen;
	len = scnprintf(tmpbuf, sizeof(tmpbuf), "%s\n", slot->buf);
	task_unlock(owner);
	put_task_struct(owner);

	WRITE_ONCE(*seen_gen, gen);

	return simple_read_from_buffer(ubuf, count, ppos, tmpbuf, len);
}

static ssize_t tag_write(struct task_struct *task, enum hzos_ext_tag_kind kind,
			 const char __user *ubuf, size_t count, loff_t *ppos)
{
	char tmpbuf[HZOS_EXT_TASK_PROFILES_LEN];
	struct task_struct *owner;
	struct hzos_ext_tag *slot;
	bool changed;
	u32 gen;

	if (!hzos_ext_tag_write_allowed())
		return -EPERM;

	/* No partial writes. */
	if (*ppos != 0)
		return -EINVAL;

	if (count >= sizeof(tmpbuf))
		return -EINVAL;

	memset(tmpbuf, 0, sizeof(tmpbuf));
	if (copy_from_user(tmpbuf, ubuf, count))
		return -EFAULT;

	owner = tag_owner_get(task, kind);
	if (!owner)
		return -ESRCH;
	slot = hzos_ext_tag_slot(owner, kind);
	task_lock(owner);
	changed = hzos_ext_tag_store(slot, tmpbuf);
	gen = slot->gen;
	task_unlock(owner);

	if (changed) {
		trace_hzos_ext_task_profiles_changed(kind, owner->tgid,
						     owner->pid, gen);
		wake_up_interruptible_poll(task_profiles_waitqueue(slot),
					   EPOLLPRI);
	}
	put_task_struct(owner);

	return count;
}

static __poll_t tag_poll(struct task_struct *task, enum hzos_ext_tag_kind kind,
			 struct file *file, struct poll_table_struct *wait,
			 u32 *seen_gen)
{
	struct task_struct *owner;
	struct hzos_ext_tag *slot;
	u32 gen;

	owner = tag_owner_get(task, kind);
	if (!owner)
		return EPOLLERR;
	slot = hzos_ext_tag_slot(owner, kind);
	/* After a non-leader exec, a parked poller stays on the old slot's queue. */
	poll_wait(file, task_profiles_waitqueue(slot), wait);
	gen = READ_ONCE(slot->gen);
	put_task_struct(owner);

	if (READ_ONCE(*seen_gen) == gen)
		return 0;

	return EPOLLPRI;
}

ssize_t hzos_ext_task_profiles_read_procfs(struct task_struct *task,
					   char __user *ubuf,
					   size_t count, loff_t *ppos,
					   u32 *seen_gen)
{
	return tag_read(task, HZOS_EXT_TAG_PROCESS, ubuf, count, ppos, seen_gen);
}

ssize_t hzos_ext_task_profiles_write_procfs(struct task_struct *task,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos)
{
	return tag_write(task, HZOS_EXT_TAG_PROCESS, ubuf, count, ppos);
}

__poll_t hzos_ext_task_profiles_poll_procfs(struct task_struct *task,
					    struct file *file,
					    struct poll_table_struct *wait,
					    u32 *seen_gen)
{
	return tag_poll(task, HZOS_EXT_TAG_PROCESS, file, wait, seen_gen);
}

ssize_t hzos_ext_thread_profiles_read_procfs(struct task_struct *task,
					     char __user *ubuf,
					     size_t count, loff_t *ppos,
					     u32 *seen_gen)
{
	return tag_read(task, HZOS_EXT_TAG_THREAD, ubuf, count, ppos, seen_gen);
}

ssize_t hzos_ext_thread_profiles_write_procfs(struct task_struct *task,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos)
{
	return tag_write(task, HZOS_EXT_TAG_THREAD, ubuf, count, ppos);
}

__poll_t hzos_ext_thread_profiles_poll_procfs(struct task_struct *task,
					      struct file *file,
					      struct poll_table_struct *wait,
					      u32 *seen_gen)
{
	return tag_poll(task, HZOS_EXT_TAG_THREAD, file, wait, seen_gen);
}
