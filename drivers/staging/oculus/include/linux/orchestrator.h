/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ORCHESTRATOR_H
#define _LINUX_ORCHESTRATOR_H
/*
 *  Custom Meta symbols and exports
 *
 *  Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/fs.h>
#include <linux/kernfs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/types.h>

#include <uapi/linux/sched/types.h>

#ifdef CONFIG_ORCHESTRATOR_AGENT

/**
 * orchestrator_flags_read_procfs - Read the Orchestrator Agent
 * orchestrator_flags procfs file.
 *
 * @task: The task for the procfs read.
 * @ubuf: The user space buffer where the result will be written.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 */
ssize_t orchestrator_flags_read_procfs(struct task_struct *task,
				       char __user *ubuf,
				       size_t count, loff_t *ppos);

/**
 * orchestrator_flags_write_procfs - Consume a Orchestrator Agent flag input.
 *
 * @task: The task being updated.
 * @ubuf: The user space buffer containing the input data.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 */
ssize_t orchestrator_flags_write_procfs(struct task_struct *task,
				        const char __user *ubuf,
				        size_t count, loff_t *ppos);

/**
 * orchestrator_task_setscheduler - Call into the Orchestrator Agent subsystem
 * to see if a task may be set to the specified scheduler.
 *
 * When Orchestrator Agent is loaded, tasks may only be set to use the RT
 * scheduler if they have the ORCHESTRATOR_FLAG_ALLOW_RT flag set in their
 * procfs file. Otherwise, the call to sched_setscheduler() will be rejected.
 *
 * @unused: Unused but trace hook expects void * as first argument.
 * @task: The task being updated.
 * @attr: The sched attribute being applied to the task.
 * @retval: Trace hooks can only have void return values, so this is a pointer to the return value.
 */
void orchestrator_task_setscheduler(void *unused, struct task_struct *task,
				   const struct sched_attr *attr, int *retval);

/**
 * orchestrator_post_clone - Notify the Orchestrator Agent that a new task has
 * been cloned.
 *
 * @unused: Unused but trace hook expects void * as first argument.
 * @new: The newly cloned task.
 * @orig: The original task.
 */
void orchestrator_post_clone(void *unused, struct task_struct *new, struct task_struct *orig);

/**
 * orchestrator_preferred_mask_write - Write the preferred mask for a task.
 *
 * @of: The sysfs file for the preferred mask
 * @buf: The buffer containing the new mask
 * @nbytes: The size of the buffer
 * @off: The offset into the buffer
 */
ssize_t orchestrator_preferred_mask_write(struct kernfs_open_file *of, char *buf,
	size_t nbytes, loff_t off);

/**
 * orchestrator_preferred_mask_read - Read the preferred mask for a task.
 *
 * @sf: The sysfs file for the preferred mask
 * @v: Unused
 */
int orchestrator_preferred_mask_read(struct seq_file *sf, void *v);

#endif /* CONFIG_ORCHESTRATOR_AGENT */

#endif /* _LINUX_ORCHESTRATOR_H */
