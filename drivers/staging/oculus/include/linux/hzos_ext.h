/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_HZOS_EXT_H
#define _LINUX_HZOS_EXT_H
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

#ifdef CONFIG_HZOS_EXT

/**
 * hzos_ext_flags_read_procfs - Read the HzOS Ext
 * hzos_ext_flags procfs file.
 *
 * @task: The task for the procfs read.
 * @ubuf: The user space buffer where the result will be written.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 */
ssize_t hzos_ext_flags_read_procfs(struct task_struct *task,
				       char __user *ubuf,
				       size_t count, loff_t *ppos);

/**
 * hzos_ext_flags_write_procfs - Consume a HzOS Ext flag input.
 *
 * @task: The task being updated.
 * @ubuf: The user space buffer containing the input data.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 */
ssize_t hzos_ext_flags_write_procfs(struct task_struct *task,
				        const char __user *ubuf,
				        size_t count, loff_t *ppos);

/**
 * hzos_ext_task_setscheduler - Call into the HzOS Ext subsystem
 * to see if a task may be set to the specified scheduler.
 *
 * When HzOS Ext is loaded, tasks may only be set to use the RT
 * scheduler if they have the HZOS_EXT_FLAG_ALLOW_RT flag set in their
 * procfs file. Otherwise, the call to sched_setscheduler() will be rejected.
 *
 * @unused: Unused but trace hook expects void * as first argument.
 * @task: The task being updated.
 * @attr: The sched attribute being applied to the task.
 * @retval: Trace hooks can only have void return values, so this is a pointer to the return value.
 */
void hzos_ext_task_setscheduler(void *unused, struct task_struct *task,
				   const struct sched_attr *attr, int *retval);

/**
 * hzos_ext_post_clone - Notify the HzOS Ext that a new task has
 * been cloned.
 *
 * @unused: Unused but trace hook expects void * as first argument.
 * @new: The newly cloned task.
 * @orig: The original task.
 */
void hzos_ext_post_clone(void *unused, struct task_struct *new, struct task_struct *orig);

/**
 * hzos_ext_preferred_mask_write - Write the preferred mask for a task.
 *
 * @of: The sysfs file for the preferred mask
 * @buf: The buffer containing the new mask
 * @nbytes: The size of the buffer
 * @off: The offset into the buffer
 */
ssize_t hzos_ext_preferred_mask_write(struct kernfs_open_file *of, char *buf,
	size_t nbytes, loff_t off);

/**
 * hzos_ext_preferred_mask_read - Read the preferred mask for a task.
 *
 * @sf: The sysfs file for the preferred mask
 * @v: Unused
 */
int hzos_ext_preferred_mask_read(struct seq_file *sf, void *v);

/**
 * hzos_ext_check_bpf_helper - Check if a BPF helper should be blocked.
 *
 * @func_id: The BPF helper function ID to check.
 * @ret: Set to -EPERM if the helper should be blocked.
 */
void hzos_ext_check_bpf_helper(int func_id, int *ret);

#else

static inline void hzos_ext_check_bpf_helper(int func_id, int *ret) { }

#endif /* CONFIG_HZOS_EXT */

#endif /* _LINUX_HZOS_EXT_H */
