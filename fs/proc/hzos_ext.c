/*
 *  Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/hzos_ext.h>
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

const struct file_operations proc_hzos_ext_flag_ops = {
        .read           = proc_hzos_ext_flags_read,
        .write          = proc_hzos_ext_flags_write,
        .llseek         = generic_file_llseek,
};

static ssize_t proc_task_profiles_read(struct file *file, char __user *ubuf,
				       size_t count, loff_t *ppos)
{
	struct task_struct *task = get_proc_task(file_inode(file));
	ssize_t ret;

	if (!task)
		return -ESRCH;

	ret = hzos_ext_task_profiles_read_procfs(task, ubuf, count, ppos,
						 file->private_data);

	put_task_struct(task);
	return ret;
}

static ssize_t proc_task_profiles_write(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	struct task_struct *task = get_proc_task(file_inode(file));
	ssize_t ret;

	if (!task)
		return -ESRCH;

	ret = hzos_ext_task_profiles_write_procfs(task, ubuf, count, ppos);

	put_task_struct(task);
	return ret;
}

static int proc_task_profiles_open(struct inode *inode, struct file *file)
{
	/*
	 * Holds the tag generation this open has observed. Per-open rather
	 * than per-task so that each watcher is woken once per change.
	 */
	file->private_data = kzalloc(sizeof(u32), GFP_KERNEL);
	if (!file->private_data)
		return -ENOMEM;

	return 0;
}

static int proc_task_profiles_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static __poll_t proc_task_profiles_poll(struct file *file,
					struct poll_table_struct *wait)
{
	struct task_struct *task = get_proc_task(file_inode(file));
	__poll_t ret;

	if (!task)
		return EPOLLERR;

	ret = hzos_ext_task_profiles_poll_procfs(task, file, wait,
						 file->private_data);

	put_task_struct(task);
	return ret;
}

const struct file_operations proc_task_profiles_ops = {
	.open		= proc_task_profiles_open,
	.release	= proc_task_profiles_release,
	.read		= proc_task_profiles_read,
	.write		= proc_task_profiles_write,
	.poll		= proc_task_profiles_poll,
	.llseek		= generic_file_llseek,
};
