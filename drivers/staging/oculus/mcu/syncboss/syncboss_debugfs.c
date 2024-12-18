// SPDX-License-Identifier: GPL-2.0
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/stat.h>

#include "syncboss_debugfs.h"

#include "syncboss_devfs_clients.h"
#include "syncboss_sequence_number.h"

static ssize_t eperm_fop_write(struct file *filp, const char *buff, size_t len,
	loff_t *off)
{
	return -EPERM;
}

static ssize_t allocated_seq_num_fop_read(struct file *filp, char *buff,
	size_t len, loff_t *off)
{
	unsigned long *allocated_seq_num =
		(unsigned long *)filp->f_inode->i_private;

	return simple_read_from_buffer(buff, len, off, allocated_seq_num,
		BITS_TO_BYTES(SYNCBOSS_SEQ_NUM_BITS));
}

const struct file_operations allocated_seq_num_fops = {
	.read = allocated_seq_num_fop_read,
	.write = eperm_fop_write,
};

static struct dentry *create_allocated_sequence_numbers_dentry(
	struct dentry *parent, unsigned long *allocated_seq_num)
{
	return debugfs_create_file("allocated_sequence_numbers", 0444,
		parent, allocated_seq_num, &allocated_seq_num_fops);
}

int syncboss_debugfs_devfs_client_add_locked(struct syncboss_debugfs *debugfs,
	struct syncboss_devfs_client *client_data)
{
	struct device *dev = debugfs->dev;
	struct dentry *dentry;
	char i_str[sizeof(client_data->index) * 3 + 1];
	int status;

	status = snprintf(i_str, sizeof(i_str), "%llu", client_data->index);
	if (status < 0 || status >= sizeof(i_str)) {
		dev_err(dev, "failed to convert %llu to string: %d", client_data->index,
			status);
			return status;
	}

	/*
	 * pid is not unique if a process opens more than one handle. pid.fd would
	 * be nice but getting the fd out of the struct file seems more complicated
	 * than I want to deal with right now. Use an incrementing index for the dir
	 * name and make the pid available within.
	 */
	client_data->dentry = debugfs_create_dir(i_str, debugfs->clients_dentry);
	if (IS_ERR_OR_NULL(client_data->dentry)) {
		dev_err(dev, "failed to create debugfs %s/clients/%s dir: %ld", 
			debugfs->name, i_str, PTR_ERR(debugfs->clients_dentry));
		return PTR_ERR(client_data->dentry);
	}

	/* Assumes pid is never negative */
	if (sizeof(client_data->task->pid) == sizeof(u32)) {
		debugfs_create_u32("pid",
			0444, client_data->dentry,
			&client_data->task->pid);
	} else {
		dev_err(dev, "failed to create debugfs %s/clients/%s/pid: unhandled pid size: %zu", 
			debugfs->name, i_str, sizeof(client_data->task->pid));
			return -EIO;
	}

	debugfs_create_u64("sequence_number_allocation_count",
		0444, client_data->dentry,
		&client_data->seq->seq_num_allocation_count);

	dentry = create_allocated_sequence_numbers_dentry(client_data->dentry,
		client_data->seq->allocated_seq_num);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(dev,
			"failed to create debugfs %s/clients/%s/allocated_sequence_numbers: %ld", 
				debugfs->name, i_str, PTR_ERR(dentry));
		return PTR_ERR(dentry);
	}

	return 0;
}

void syncboss_debugfs_devfs_client_remove_locked(struct syncboss_debugfs *debugfs,
	struct syncboss_devfs_client *client_data)
{
	if (!IS_ERR_OR_NULL(client_data->dentry)) {
		debugfs_remove_recursive(client_data->dentry);
		client_data->dentry = NULL;
	}
}

int syncboss_debugfs_init(struct syncboss_debugfs *debugfs, struct device *dev,
	struct syncboss_seq *seq, const char *name)
{
	struct dentry *dentry;

	debugfs->dev = dev;
	debugfs->seq = seq;
	debugfs->name = name;

	debugfs->dentry = debugfs_create_dir(debugfs->name, NULL);
	if (IS_ERR_OR_NULL(debugfs->dentry)) {
		dev_err(dev, "failed to create debugfs %s dir: %ld", 
			debugfs->name, PTR_ERR(debugfs->dentry));
		return PTR_ERR(debugfs->dentry);
	}

	debugfs->clients_dentry = debugfs_create_dir("clients", debugfs->dentry);
	if (IS_ERR_OR_NULL(debugfs->clients_dentry)) {
		dev_err(dev, "failed to create debugfs %s/clients dir: %ld", 
			debugfs->name, PTR_ERR(debugfs->clients_dentry));
		return PTR_ERR(debugfs->clients_dentry);
	}

	debugfs_create_u64("sequence_number_allocation_count",
		0444, debugfs->dentry,
		&debugfs->seq->seq_num_allocation_count);

	dentry = create_allocated_sequence_numbers_dentry(debugfs->dentry,
		debugfs->seq->allocated_seq_num);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(dev, "failed to create debugfs %s/allocated_sequence_numbers: %ld", 
			debugfs->name, PTR_ERR(dentry));
		return PTR_ERR(dentry);
	}

	return 0;
}

void syncboss_debugfs_deinit(struct syncboss_debugfs *debugfs)
{
	if (!IS_ERR_OR_NULL(debugfs->dentry)) {
		debugfs_remove_recursive(debugfs->dentry);
		debugfs->dentry = NULL;
		debugfs->clients_dentry = NULL;
	}
}
