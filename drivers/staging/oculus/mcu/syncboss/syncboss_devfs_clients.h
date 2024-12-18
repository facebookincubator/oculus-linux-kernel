/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_DEVFS_CLIENTS_H
#define _SYNCBOSS_DEVFS_CLIENTS_H

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/types.h>

struct syncboss_debugfs;
struct syncboss_seq_client;

struct syncboss_devfs_client {
	struct list_head list_entry;
	struct file *file;
	struct task_struct *task;
	struct dentry *dentry;
	u64 index;
	struct syncboss_seq_client *seq;
};

struct syncboss_devfs_clients {
	struct device *dev;
	struct syncboss_seq *seq;
	struct syncboss_debugfs *debugfs;
	u64 client_data_index;
	struct list_head client_data_list;
};

void syncboss_devfs_clients_init(struct syncboss_devfs_clients *clients,
	struct device *dev, struct syncboss_seq *seq,
	struct syncboss_debugfs *debugfs);

int syncboss_devfs_client_create_locked(struct syncboss_devfs_clients *clients,
		struct file *file, struct task_struct *task,
		struct syncboss_devfs_client **client);
struct syncboss_devfs_client *syncboss_devfs_client_get_locked(
	struct syncboss_devfs_clients *clients, struct file *file);
void syncboss_devfs_client_destroy_locked(struct syncboss_devfs_clients *clients,
	struct syncboss_devfs_client *client);

#endif /* _SYNCBOSS_DEVFS_CLIENTS_H */
