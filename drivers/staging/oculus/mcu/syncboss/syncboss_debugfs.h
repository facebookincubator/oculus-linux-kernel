/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_DEBUGFS_H
#define _SYNCBOSS_DEBUGFS_H

#include <linux/device.h>
#include <linux/fs.h>

struct syncboss_devfs_client;
struct syncboss_seq;

struct syncboss_debugfs {
	struct device *dev;
	struct syncboss_seq *seq;
	const char *name;
	struct dentry *dentry;
	struct dentry *clients_dentry;
};

#ifdef CONFIG_DEBUG_FS
int syncboss_debugfs_init(struct syncboss_debugfs *debugfs, struct device *dev,
	struct syncboss_seq *seq, const char *name);
void syncboss_debugfs_deinit(struct syncboss_debugfs *debugfs);

int syncboss_debugfs_devfs_client_add_locked(struct syncboss_debugfs *debugfs,
	struct syncboss_devfs_client *client_data);
void syncboss_debugfs_devfs_client_remove_locked(struct syncboss_debugfs *debugfs,
	struct syncboss_devfs_client *client_data);
#else

#include <linux/err.h>

static inline int syncboss_debugfs_init(struct syncboss_debugfs *debugfs,
	struct device *dev, struct syncboss_seq *seq, const char *name)
{
	return -ENODEV;
}

static inline void syncboss_debugfs_deinit(struct syncboss_debugfs *debugfs)
{
}

static inline int syncboss_debugfs_devfs_client_add_locked(
	struct syncboss_debugfs *debugfs, struct syncboss_devfs_client *client_data)
{
	return -ENODEV;
}

static inline void syncboss_debugfs_devfs_client_remove_locked(
	struct syncboss_debugfs *debugfs, struct syncboss_devfs_client *client_data)
{
}

#endif /* CONFIG_DEBUG_FS */

#endif  /* _SYNCBOSS_DEBUGFS_H */
