// SPDX-License-Identifier: GPL-2.0
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/stat.h>

#include "syncboss_debugfs.h"

#define ROOT_DIR_NAME "syncboss"

static ssize_t eperm_fop_write(struct file *filp, const char *buff, size_t len,
	loff_t *off)
{
	return -EPERM;
}

static ssize_t seq_num_mode_fop_read(struct file *filp, char *buff, size_t len,
	loff_t *off)
{
	bool *has_seq_num_ioctl = (bool *)filp->f_inode->i_private;
	const char *seq_num_mode_str;

	if (*has_seq_num_ioctl)
		seq_num_mode_str = "ioctl\n";
	else
		seq_num_mode_str = "sysfs\n";

	return simple_read_from_buffer(buff, len, off, seq_num_mode_str,
		strlen(seq_num_mode_str) + 1);
}

const struct file_operations seq_num_mode_fops = {
	.read = seq_num_mode_fop_read,
	.write = eperm_fop_write,
};

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

int syncboss_debugfs_client_add_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data)
{
	struct device *dev = &devdata->spi->dev;
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
	client_data->dentry = debugfs_create_dir(i_str, devdata->clients_dentry);
	if (IS_ERR_OR_NULL(client_data->dentry)) {
		dev_err(dev, "failed to create debugfs " ROOT_DIR_NAME
			"/clients/%s dir: %ld", i_str,
			PTR_ERR(devdata->clients_dentry));
		return PTR_ERR(client_data->dentry);
	}

	/* Assumes pid is never negative */
	if (sizeof(client_data->task->pid) == sizeof(u32)) {
		debugfs_create_u32("pid",
			0444, client_data->dentry,
			&client_data->task->pid);
	} else {
		dev_err(dev, "failed to create debugfs " ROOT_DIR_NAME
			"/clients/%s/pid: unhandled pid size: %zu", i_str,
			sizeof(client_data->task->pid));
			return -EIO;
	}

	debugfs_create_u64("sequence_number_allocation_count",
		0444, client_data->dentry,
		&client_data->seq_num_allocation_count);

	dentry = create_allocated_sequence_numbers_dentry(client_data->dentry,
		client_data->allocated_seq_num);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(dev,
			"failed to create debugfs " ROOT_DIR_NAME
			"/clients/%s/allocated_sequence_numbers: %ld", i_str,
			PTR_ERR(dentry));
		return PTR_ERR(dentry);
	}

	return 0;
}

void syncboss_debugfs_client_remove_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data)
{
	if (!IS_ERR_OR_NULL(client_data->dentry)) {
		debugfs_remove_recursive(client_data->dentry);
		client_data->dentry = NULL;
	}
}

int syncboss_debugfs_init(struct syncboss_dev_data *devdata)
{
	struct device *dev = &devdata->spi->dev;
	struct dentry *dentry;

	devdata->dentry = debugfs_create_dir(ROOT_DIR_NAME, NULL);
	if (IS_ERR_OR_NULL(devdata->dentry)) {
		dev_err(dev, "failed to create debugfs " ROOT_DIR_NAME
			" dir: %ld", PTR_ERR(devdata->dentry));
		return PTR_ERR(devdata->dentry);
	}

	devdata->clients_dentry = debugfs_create_dir("clients", devdata->dentry);
	if (IS_ERR_OR_NULL(devdata->clients_dentry)) {
		dev_err(dev, "failed to create debugfs " ROOT_DIR_NAME
			"/clients dir: %ld", PTR_ERR(devdata->clients_dentry));
		return PTR_ERR(devdata->clients_dentry);
	}

	dentry = debugfs_create_file("sequence_number_mode", 0444,
		devdata->dentry, &devdata->has_seq_num_ioctl, &seq_num_mode_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(dev, "failed to create debugfs " ROOT_DIR_NAME
			"/sequence_number_mode: %ld", PTR_ERR(dentry));
		return PTR_ERR(dentry);
	}

	debugfs_create_u64("sequence_number_allocation_count",
		0444, devdata->dentry,
		&devdata->seq_num_allocation_count);

	dentry = create_allocated_sequence_numbers_dentry(devdata->dentry,
		devdata->allocated_seq_num);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(dev, "failed to create debugfs " ROOT_DIR_NAME
			"/allocated_sequence_numbers: %ld", PTR_ERR(dentry));
		return PTR_ERR(dentry);
	}

	return 0;
}

void syncboss_debugfs_deinit(struct syncboss_dev_data *devdata)
{
	if (!IS_ERR_OR_NULL(devdata->dentry)) {
		debugfs_remove_recursive(devdata->dentry);
		devdata->dentry = NULL;
	}
}
