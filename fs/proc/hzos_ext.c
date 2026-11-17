// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/fs.h>
#include <linux/hzos_ext.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "internal.h"

static ssize_t proc_hzos_ext_flags_op(struct file *file,
				          const char __user *ubuf,
                          size_t count, loff_t *ppos,
				          bool write)
{
	struct inode * inode = file_inode(file);
	struct task_struct *task = get_proc_task(inode);
	ssize_t ret;

	if (!task)
		return -ESRCH;

	if (write)
		ret = hzos_ext_flags_write_procfs(task, ubuf, count, ppos);
	else
		ret = hzos_ext_flags_read_procfs(task, (char *)((uintptr_t)ubuf),
					             count, ppos);

	put_task_struct(task);
	return ret;
}

static ssize_t proc_hzos_ext_flags_read(struct file *file,
					    char __user *ubuf,
                                            size_t count, loff_t *ppos)
{
	return proc_hzos_ext_flags_op(file, ubuf, count, ppos, false);
}

static ssize_t proc_hzos_ext_flags_write(struct file *file,
					     const char __user *ubuf,
                                             size_t count, loff_t *ppos)
{
	return proc_hzos_ext_flags_op(file, ubuf, count, ppos, true);
}

static int proc_hzos_ext_flags_open(struct inode *inode, struct file *file)
{
	/*
	 * Writes are checked again against the writer, but checking the
	 * opener as well stops an unprivileged task from handing a writable
	 * fd to a privileged one and having it write on its behalf.
	 */
	if ((file->f_mode & FMODE_WRITE) && !hzos_ext_flags_write_allowed())
		return -EPERM;

	return 0;
}

const struct file_operations proc_hzos_ext_flag_ops = {
	.open		= proc_hzos_ext_flags_open,
        .read           = proc_hzos_ext_flags_read,
        .write          = proc_hzos_ext_flags_write,
        .llseek         = generic_file_llseek,
};

/*
 * task_profiles tags the process and thread_profiles tags a single thread.
 * Both files share one implementation and differ only in these helpers.
 */
struct proc_profiles_helpers {
	ssize_t (*read)(struct task_struct *task, char __user *ubuf,
			size_t count, loff_t *ppos, u32 *seen_gen);
	ssize_t (*write)(struct task_struct *task, const char __user *ubuf,
			 size_t count, loff_t *ppos);
	__poll_t (*poll)(struct task_struct *task, struct file *file,
			 struct poll_table_struct *wait, u32 *seen_gen);
};

static const struct proc_profiles_helpers task_profiles_helpers = {
	.read	= hzos_ext_task_profiles_read_procfs,
	.write	= hzos_ext_task_profiles_write_procfs,
	.poll	= hzos_ext_task_profiles_poll_procfs,
};

static const struct proc_profiles_helpers thread_profiles_helpers = {
	.read	= hzos_ext_thread_profiles_read_procfs,
	.write	= hzos_ext_thread_profiles_write_procfs,
	.poll	= hzos_ext_thread_profiles_poll_procfs,
};

static ssize_t proc_profiles_read(struct file *file, char __user *ubuf,
				  size_t count, loff_t *ppos,
				  const struct proc_profiles_helpers *helpers)
{
	struct task_struct *task = get_proc_task(file_inode(file));
	ssize_t ret;

	if (!task)
		return -ESRCH;

	ret = helpers->read(task, ubuf, count, ppos, file->private_data);

	put_task_struct(task);
	return ret;
}

static ssize_t proc_profiles_write(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos,
				   const struct proc_profiles_helpers *helpers)
{
	struct task_struct *task = get_proc_task(file_inode(file));
	ssize_t ret;

	if (!task)
		return -ESRCH;

	ret = helpers->write(task, ubuf, count, ppos);

	put_task_struct(task);
	return ret;
}

static __poll_t proc_profiles_poll(struct file *file,
				   struct poll_table_struct *wait,
				   const struct proc_profiles_helpers *helpers)
{
	struct task_struct *task = get_proc_task(file_inode(file));
	__poll_t ret;

	if (!task)
		return EPOLLERR;

	ret = helpers->poll(task, file, wait, file->private_data);

	put_task_struct(task);
	return ret;
}

static int proc_profiles_open(struct inode *inode, struct file *file)
{
	/*
	 * Writes are checked again against the writer, but checking the
	 * opener as well stops an unprivileged task from handing a writable
	 * fd to a privileged one and having it write on its behalf.
	 */
	if ((file->f_mode & FMODE_WRITE) && !hzos_ext_tag_write_allowed())
		return -EPERM;

	/*
	 * Holds the tag generation this open has observed. Per-open rather
	 * than per-task so that each watcher is woken once per change.
	 */
	file->private_data = kzalloc(sizeof(u32), GFP_KERNEL);
	if (!file->private_data)
		return -ENOMEM;

	return 0;
}

static int proc_profiles_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static ssize_t proc_task_profiles_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	return proc_profiles_read(file, ubuf, count, ppos,
				  &task_profiles_helpers);
}

static ssize_t proc_task_profiles_write(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	return proc_profiles_write(file, ubuf, count, ppos,
				   &task_profiles_helpers);
}

static __poll_t proc_task_profiles_poll(struct file *file,
					struct poll_table_struct *wait)
{
	return proc_profiles_poll(file, wait, &task_profiles_helpers);
}

const struct file_operations proc_task_profiles_ops = {
	.open		= proc_profiles_open,
	.release	= proc_profiles_release,
	.read		= proc_task_profiles_read,
	.write		= proc_task_profiles_write,
	.poll		= proc_task_profiles_poll,
	.llseek		= generic_file_llseek,
};

static ssize_t proc_thread_profiles_read(struct file *file, char __user *ubuf,
					 size_t count, loff_t *ppos)
{
	return proc_profiles_read(file, ubuf, count, ppos,
				  &thread_profiles_helpers);
}

static ssize_t proc_thread_profiles_write(struct file *file,
					  const char __user *ubuf,
					  size_t count, loff_t *ppos)
{
	return proc_profiles_write(file, ubuf, count, ppos,
				   &thread_profiles_helpers);
}

static __poll_t proc_thread_profiles_poll(struct file *file,
					  struct poll_table_struct *wait)
{
	return proc_profiles_poll(file, wait, &thread_profiles_helpers);
}

const struct file_operations proc_thread_profiles_ops = {
	.open		= proc_profiles_open,
	.release	= proc_profiles_release,
	.read		= proc_thread_profiles_read,
	.write		= proc_thread_profiles_write,
	.poll		= proc_thread_profiles_poll,
	.llseek		= generic_file_llseek,
};
