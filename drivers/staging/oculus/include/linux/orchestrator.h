/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ORCHESTRATOR_H
#define _LINUX_ORCHESTRATOR_H
/*
 *  Custom Meta symbols and exports
 *
 *  Copyright (C) 2024 Meta Platforms, Inc. and affiliates
 */

#include <linux/fs.h>
#include <linux/sched.h>
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
 * @task: The task being updated.
 * @attr: The sched attribute being applied to the task.
 */
int orchestrator_task_setscheduler(struct task_struct *task,
				   const struct sched_attr *attr);

/**
 * orchestrator_post_clone - Notify the Orchestrator Agent that a new task has
 * been cloned.
 *
 * @new: The newly cloned task.
 * @orig: The original task.
 */
void orchestrator_post_clone(struct task_struct *new, struct task_struct *orig);

#endif /* CONFIG_ORCHESTRATOR_AGENT */

#endif /* _LINUX_ORCHESTRATOR_H */
