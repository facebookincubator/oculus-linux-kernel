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

struct poll_table_struct;

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
 * hzos_ext_flags_write_allowed - Whether the current task may write HzOS Ext
 * flags. Check it when the flags file is opened for writing (FMODE_WRITE) and
 * on every write.
 */
bool hzos_ext_flags_write_allowed(void);

/**
 * hzos_ext_tag_write_allowed - Whether the current task may set a tag. Check
 * it when a tag file is opened for writing (FMODE_WRITE) and on every write.
 */
bool hzos_ext_tag_write_allowed(void);

/**
 * hzos_ext_task_profiles_read_procfs - Read the task_profiles procfs file.
 *
 * Reading marks the tag generation seen, which is what stops a poller from
 * being told about the same change again.
 *
 * @task: The task for the procfs read.
 * @ubuf: The user space buffer where the result will be written.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 * @seen_gen: The generation this open has observed, updated on return.
 */
ssize_t hzos_ext_task_profiles_read_procfs(struct task_struct *task,
					   char __user *ubuf,
					   size_t count, loff_t *ppos,
					   u32 *seen_gen);

/**
 * hzos_ext_task_profiles_write_procfs - Tag a task with the task profiles
 * that were applied to it.
 *
 * The tag is stored on @task's thread group leader, so writing through any
 * thread of the process tags the process.
 *
 * @task: The task being tagged.
 * @ubuf: The user space buffer containing the input data.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 */
ssize_t hzos_ext_task_profiles_write_procfs(struct task_struct *task,
					    const char __user *ubuf,
					    size_t count, loff_t *ppos);

/**
 * hzos_ext_task_profiles_poll_procfs - Report whether a task's task profiles
 * have changed since this open of the procfs file last observed them.
 *
 * Returns EPOLLPRI, following the convention that a regular file always reads
 * as ready so a change has to be signalled out of band. Keeps reporting until
 * the tag is read; polling deliberately does not consume the change, because
 * epoll polls a file both on registration and again when it reports.
 *
 * @task: The task being watched.
 * @file: The procfs file being polled.
 * @wait: The poll table to register on.
 * @seen_gen: The generation this open last observed.
 */
__poll_t hzos_ext_task_profiles_poll_procfs(struct task_struct *task,
					    struct file *file,
					    struct poll_table_struct *wait,
					    u32 *seen_gen);

/**
 * hzos_ext_thread_profiles_read_procfs - Read the thread_profiles procfs file.
 *
 * Like hzos_ext_task_profiles_read_procfs(), for @task's own thread tag.
 *
 * @task: The thread for the procfs read.
 * @ubuf: The user space buffer where the result will be written.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 * @seen_gen: The generation this open has observed, updated on return.
 */
ssize_t hzos_ext_thread_profiles_read_procfs(struct task_struct *task,
					     char __user *ubuf,
					     size_t count, loff_t *ppos,
					     u32 *seen_gen);

/**
 * hzos_ext_thread_profiles_write_procfs - Tag a thread with the profiles that
 * were applied to it.
 *
 * Like hzos_ext_task_profiles_write_procfs(), but the tag is stored on @task
 * itself. New threads inherit it; new processes start untagged.
 *
 * @task: The thread being tagged.
 * @ubuf: The user space buffer containing the input data.
 * @count: Size of user space buffer.
 * @ppos: Offset into buffer.
 */
ssize_t hzos_ext_thread_profiles_write_procfs(struct task_struct *task,
					      const char __user *ubuf,
					      size_t count, loff_t *ppos);

/**
 * hzos_ext_thread_profiles_poll_procfs - Report whether a thread's tag has
 * changed since this open of the procfs file last observed it.
 *
 * Same semantics as hzos_ext_task_profiles_poll_procfs().
 *
 * @task: The thread being watched.
 * @file: The procfs file being polled.
 * @wait: The poll table to register on.
 * @seen_gen: The generation this open last observed.
 */
__poll_t hzos_ext_thread_profiles_poll_procfs(struct task_struct *task,
					      struct file *file,
					      struct poll_table_struct *wait,
					      u32 *seen_gen);

/**
 * hzos_ext_task_profiles_init - Initialize the wait queue table that task
 * profile watchers park on. Called once from the hzos_ext initcall.
 */
void hzos_ext_task_profiles_init(void);

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

/* The thread group that FORK_TG_NORMAL threads create new threads in. */
#define HZOS_EXT_FORK_CGROUP_NAME "tg_normal"

/**
 * hzos_ext_fork_tg_normal - Whether a thread cloned by @parent should start in
 * the HZOS_EXT_FORK_CGROUP_NAME sibling of @parent's thread group rather than
 * in @parent's own cgroup.
 *
 * A thread tagged into a cgroup v2 thread group would otherwise pass its group
 * to every thread it spawns, since clone() places a new thread in its creator's
 * cgroup.
 *
 * @parent: The task calling clone().
 * @clone_flags: The clone flags of the new task.
 */
bool hzos_ext_fork_tg_normal(struct task_struct *parent, u64 clone_flags);

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

static inline bool hzos_ext_fork_tg_normal(struct task_struct *parent,
					   u64 clone_flags)
{
	return false;
}

#endif /* CONFIG_HZOS_EXT */

#endif /* _LINUX_HZOS_EXT_H */
