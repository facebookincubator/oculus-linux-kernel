// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/fs.h>
#include <linux/hzos_ext.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
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
