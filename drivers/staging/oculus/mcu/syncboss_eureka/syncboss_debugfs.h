/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_DEBUGFS_H
#define _SYNCBOSS_DEBUGFS_H

#include "syncboss.h"

#ifdef CONFIG_DEBUG_FS
int syncboss_debugfs_init(struct syncboss_dev_data *devdata);
void syncboss_debugfs_deinit(struct syncboss_dev_data *devdata);

int syncboss_debugfs_client_add_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data);
void syncboss_debugfs_client_remove_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data);
#else

#include <linux/err.h>

static inline int syncboss_debugfs_init(struct syncboss_dev_data *devdata)
{
	return -ENODEV;
}

static inline void syncboss_debugfs_deinit(struct syncboss_dev_data *devdata)
{
}

static inline int syncboss_debugfs_client_add_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data)
	{
	return -ENODEV;
}

static inline void syncboss_debugfs_client_remove_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data)
	{
}

#endif

#endif
