// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2025 Meta Platforms, Inc. and affiliates
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/orchestrator.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "internal.h"

static ssize_t proc_orchestrator_flags_op(struct file *file,
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
		ret = orchestrator_flags_write_procfs(task, ubuf, count, ppos);
	else
		ret = orchestrator_flags_read_procfs(task, (char *)((uintptr_t)ubuf),
					             count, ppos);

	put_task_struct(task);
	return ret;
}

static ssize_t proc_orchestrator_flags_read(struct file *file,
					    char __user *ubuf,
                                            size_t count, loff_t *ppos)
{
	return proc_orchestrator_flags_op(file, ubuf, count, ppos, false);
}

static ssize_t proc_orchestrator_flags_write(struct file *file,
					     const char __user *ubuf,
                                             size_t count, loff_t *ppos)
{
	return proc_orchestrator_flags_op(file, ubuf, count, ppos, true);
}

const struct file_operations proc_orchestrator_flag_ops = {
        .read           = proc_orchestrator_flags_read,
        .write          = proc_orchestrator_flags_write,
        .llseek         = generic_file_llseek,
};
