// SPDX-License-Identifier: GPL-2.0
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/stat.h>

#include "syncboss_debugfs.h"

#include "syncboss_devfs_clients.h"
#include "syncboss_sequence_number.h"

/* This needs to be large enough to hold the path from the mount point */
#define SAFE_PATH_FOR_DENTRY_BUFFER_LEN 60

static char *safe_path_for_dentry(struct dentry *dentry, char *buffer, size_t buffer_len)
{
	char *path = dentry_path_raw(dentry, buffer, buffer_len);
	return IS_ERR_OR_NULL(path) ? "?" : path;
}

static struct dentry *create_debugfs_dir(struct syncboss_debugfs *debugfs,
	const char *name, struct dentry *parent)
{
	struct device *dev = debugfs->dev;
	char buffer[SAFE_PATH_FOR_DENTRY_BUFFER_LEN];
	struct dentry *dentry;
	const char *parent_path;

	dentry = debugfs_create_dir(name, parent);
	if (IS_ERR_OR_NULL(dentry)) {
		parent_path = parent ? safe_path_for_dentry(parent, buffer, sizeof(buffer)) : "";

		dev_err(dev, "failed to create debugfs dir %s/%s: %ld",
			parent_path, name, PTR_ERR(dentry));
	}

	return dentry;
}

static struct dentry *create_debugfs_file(struct syncboss_debugfs *debugfs,
	const char *name, mode_t mode, struct dentry *parent, void *data,
	const struct file_operations *fops)
{
	struct device *dev = debugfs->dev;
	char buffer[SAFE_PATH_FOR_DENTRY_BUFFER_LEN];
	struct dentry *dentry;
	const char *parent_path;

	dentry = debugfs_create_file(name, mode, parent, data, fops);
	if (IS_ERR_OR_NULL(dentry)) {
		parent_path = parent ? safe_path_for_dentry(parent, buffer, sizeof(buffer)) : "";

		dev_err(dev, "failed to create debugfs file %s/%s: %ld",
			parent_path, name, PTR_ERR(dentry));
	}

	return dentry;
}

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

static struct dentry *create_allocated_sequence_numbers_bitmap_dentry(
	struct syncboss_debugfs *debugfs, struct dentry *parent,
	unsigned long *allocated_seq_num)
{
	return create_debugfs_file(debugfs, "allocation_bitmap", 0444, parent,
		allocated_seq_num, &allocated_seq_num_fops);
}

int syncboss_debugfs_devfs_client_add_locked(struct syncboss_debugfs *debugfs,
	struct syncboss_devfs_client *client_data)
{
	struct device *dev = debugfs->dev;
	char i_str[sizeof(client_data->index) * 3 + 1];
	int status;
	char buffer[SAFE_PATH_FOR_DENTRY_BUFFER_LEN];
	struct dentry *dentry;

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
	client_data->dentry = create_debugfs_dir(debugfs, i_str, debugfs->clients_dentry);
	if (IS_ERR_OR_NULL(client_data->dentry))
		return PTR_ERR(client_data->dentry);

	/* Assumes pid is never negative */
	if (sizeof(client_data->task->pid) == sizeof(u32)) {
		debugfs_create_u32("pid", 0444, client_data->dentry,
			&client_data->task->pid);
	} else {
		dev_err(dev, "failed to create debugfs file %s/pid: unhandled pid size: %zu",
			safe_path_for_dentry(client_data->dentry, buffer, sizeof(buffer)),
			sizeof(client_data->task->pid));
		return -EIO;
	}

	dentry = syncboss_debugfs_create_seq_allocations_dir(debugfs, client_data->dentry,
		&client_data->seq->allocations, "sequence_numbers");
	if (IS_ERR_OR_NULL(dentry))
		return PTR_ERR(dentry);

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

struct dentry *syncboss_debugfs_create_seq_allocations_dir(
	struct syncboss_debugfs *debugfs, struct dentry *parent_dentry,
	struct syncboss_seq_allocations *seq_allocations, const char *name)
{
	struct device *dev = debugfs->dev;
	struct dentry *dir_dentry;
	struct dentry *dentry;

	dir_dentry = create_debugfs_dir(debugfs, name, parent_dentry);
	if (IS_ERR_OR_NULL(dir_dentry)) {
		return dir_dentry;
	}

	debugfs_create_u64("allocation_count", 0444, dir_dentry,
		&seq_allocations->count);

	dentry = create_allocated_sequence_numbers_bitmap_dentry(debugfs, dir_dentry,
		seq_allocations->bitmap);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(dev, "failed to create debugfs %s/%s/allocated_bitmap: %ld",
			debugfs->name, name, PTR_ERR(dentry));
		return dentry;
	}

	return dir_dentry;
}

int syncboss_debugfs_init(struct syncboss_debugfs *debugfs, struct device *dev,
	struct syncboss_seq *seq, const char *name)
{
	struct dentry *dentry;

	debugfs->dev = dev;
	debugfs->seq = seq;
	debugfs->name = name;

	debugfs->dentry = create_debugfs_dir(debugfs, debugfs->name, NULL);
	if (IS_ERR_OR_NULL(debugfs->dentry))
		return PTR_ERR(debugfs->dentry);

	debugfs->clients_dentry = create_debugfs_dir(debugfs, "clients", debugfs->dentry);
	if (IS_ERR_OR_NULL(debugfs->clients_dentry))
		return PTR_ERR(debugfs->clients_dentry);

	dentry = syncboss_debugfs_create_seq_allocations_dir(debugfs,
		debugfs->dentry, &seq->allocations, "sequence_numbers");
	if (IS_ERR_OR_NULL(debugfs->dentry))
		return PTR_ERR(dentry);

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
