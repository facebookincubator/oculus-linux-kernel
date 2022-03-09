/* Copyright (c) 2012-2015,2017-2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/idr.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/ipc_logging.h>

#include "ipc_logging_private.h"

static DEFINE_MUTEX(ipc_log_debugfs_init_lock);
static struct dentry *root_dent;

static int debug_log(struct ipc_log_context *ilctxt,
		     char *buff, int size, int cont)
{
	int i = 0;
	int ret;

	if (size < MAX_MSG_DECODED_SIZE) {
		pr_err("%s: buffer size %d < %d\n", __func__, size,
			MAX_MSG_DECODED_SIZE);
		return -ENOMEM;
	}
	do {
		i = ipc_log_extract(ilctxt, buff, size - 1);
		if (cont && i == 0) {
			ret = wait_for_completion_interruptible(
				&ilctxt->read_avail);
			if (ret < 0)
				return ret;
		}
	} while (cont && i == 0);

	return i;
}

/*
 * VFS Read operation helper which dispatches the call to the debugfs
 * read command stored in file->private_data.
 *
 * @file  File structure
 * @buff   user buffer
 * @count size of user buffer
 * @ppos  file position to read from (only a value of 0 is accepted)
 * @cont  1 = continuous mode (don't return 0 to signal end-of-file)
 *
 * @returns ==0 end of file
 *           >0 number of bytes read
 *           <0 error
 */
static ssize_t debug_read_helper(struct file *file, char __user *buff,
				 size_t count, loff_t *ppos, int cont)
{
	struct ipc_log_context *ilctxt;
	struct dentry *d = file->f_path.dentry;
	char *buffer;
	int bsize;
	int r;

	r = debugfs_file_get(d);
	if (!r) {
		ilctxt = file->private_data;
		r = kref_get_unless_zero(&ilctxt->refcount) ? 0 : -EIO;
	} else {
		return r;
	}
	debugfs_file_put(d);

	buffer = kmalloc(count, GFP_KERNEL);
	if (!buffer) {
		bsize = -ENOMEM;
		goto done;
	}

	bsize = debug_log(ilctxt, buffer, count, cont);

	if (bsize > 0) {
		if (copy_to_user(buff, buffer, bsize)) {
			bsize = -EFAULT;
			kfree(buffer);
			goto done;
		}
		*ppos += bsize;
	}
	kfree(buffer);

done:
	ipc_log_context_put(ilctxt);
	return bsize;
}

static ssize_t debug_write_ctrl(struct file *file, const char __user *buff,
				 size_t count, loff_t *ppos)
{
	struct ipc_log_context *ilctxt;
	struct dentry *d = file->f_path.dentry;
	int bsize = 1;
	int r;
	char local_buf[3];

	r = debugfs_file_get(d);
	if (!r) {
		ilctxt = file->private_data;
		r = kref_get_unless_zero(&ilctxt->refcount) ? 0 : -EIO;
	} else {
		return r;
	}
	debugfs_file_put(d);

	if (copy_from_user(local_buf, buff, bsize)) {
		count = -EFAULT;
		goto done;
	}

	if (*local_buf == '1') {
		ipc_log_string(ilctxt, "LOGGING DISABLED FOR THIS CLIENT!!\n");
		ilctxt->disabled = true;
	} else if (*local_buf == '0') {
		ilctxt->disabled = false;
		ipc_log_string(ilctxt, "LOGGING ENABLED FOR THIS CLIENT!!\n");
	}

done:
	ipc_log_context_put(ilctxt);
	return count;
}


static ssize_t debug_write_ctrl_all(struct file *file, const char __user *buff,
				 size_t count, loff_t *ppos)
{
	int bsize = 1;
	char local_buf[3];

	if (copy_from_user(local_buf, buff, bsize))
		return -EFAULT;
	if (*local_buf == '1')
		ipc_log_ctrl_all(true);
	else if (*local_buf == '0')
		ipc_log_ctrl_all(false);
	return count;
}

static ssize_t debug_read_ctrl(struct file *file, char __user *buff,
				 size_t count, loff_t *ppos)
{
	struct ipc_log_context *ilctxt;
	struct dentry *d = file->f_path.dentry;
	int bsize = 2;
	int r;

	r = debugfs_file_get(d);
	if (!r) {
		ilctxt = file->private_data;
		r = kref_get_unless_zero(&ilctxt->refcount) ? 0 : -EIO;
	} else {
		return r;
	}
	debugfs_file_put(d);

	bsize = simple_read_from_buffer(buff, count, ppos,
				ilctxt->disabled?"1\n":"0\n", bsize);
	ipc_log_context_put(ilctxt);
	return bsize;
}

static ssize_t debug_read(struct file *file, char __user *buff,
			  size_t count, loff_t *ppos)
{
	return debug_read_helper(file, buff, count, ppos, 0);
}

static ssize_t debug_read_cont(struct file *file, char __user *buff,
			       size_t count, loff_t *ppos)
{
	return debug_read_helper(file, buff, count, ppos, 1);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static const struct file_operations debug_ops_cont = {
	.read = debug_read_cont,
	.open = debug_open,
};

static const struct file_operations debug_ops_ctrl = {
	.read = debug_read_ctrl,
	.write = debug_write_ctrl,
	.open = debug_open,
};

static const struct file_operations debug_ops_ctrl_all = {
	.write = debug_write_ctrl_all,
};

static void debug_create(const char *name, mode_t mode,
			 struct dentry *dent,
			 struct ipc_log_context *ilctxt,
			 const struct file_operations *fops)
{
	debugfs_create_file_unsafe(name, mode, dent, ilctxt, fops);
}

static void dfunc_string(struct encode_context *ectxt,
			 struct decode_context *dctxt)
{
	tsv_timestamp_read(ectxt, dctxt, "");
	tsv_qtimer_read(ectxt, dctxt, " ");
	tsv_byte_array_read(ectxt, dctxt, "");

	/* add trailing \n if necessary */
	if (*(dctxt->buff - 1) != '\n') {
		if (dctxt->size) {
			++dctxt->buff;
			--dctxt->size;
		}
		*(dctxt->buff - 1) = '\n';
	}
}

void check_and_create_debugfs(void)
{
	mutex_lock(&ipc_log_debugfs_init_lock);
	if (!root_dent) {
		root_dent = debugfs_create_dir("ipc_logging", 0);

		if (IS_ERR(root_dent)) {
			pr_err("%s: unable to create debugfs %ld\n",
				__func__, PTR_ERR(root_dent));
			root_dent = NULL;
		} else {
			debug_create("ctrl_all", 0444, root_dent,
				NULL, &debug_ops_ctrl_all);
		}
	}
	mutex_unlock(&ipc_log_debugfs_init_lock);
}
EXPORT_SYMBOL(check_and_create_debugfs);

void create_ctx_debugfs(struct ipc_log_context *ctxt,
			const char *mod_name)
{
	if (!root_dent)
		check_and_create_debugfs();

	if (root_dent) {
		ctxt->dent = debugfs_create_dir(mod_name, root_dent);
		if (!IS_ERR(ctxt->dent)) {
			debug_create("log", 0444, ctxt->dent,
				     ctxt, &debug_ops);
			debug_create("log_cont", 0444, ctxt->dent,
				     ctxt, &debug_ops_cont);
			debug_create("log_disable", 0444, ctxt->dent,
				     ctxt, &debug_ops_ctrl);
		}
	}
	add_deserialization_func((void *)ctxt,
				 TSV_TYPE_STRING, dfunc_string);
}
EXPORT_SYMBOL(create_ctx_debugfs);
