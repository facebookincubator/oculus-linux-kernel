/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_NODE_FASTPATH_H_
#define _CAM_NODE_FASTPATH_H_

#include <linux/mutex.h>
#include <media/cam_defs.h>

enum cam_node_fastpath_state {
	CAM_NODE_FP_STATE_INIT,
	CAM_NODE_FP_STATE_ACQUIRED_DEV,
	CAM_NODE_FP_STATE_ACQUIRED_HW,
	CAM_NODE_FP_STATE_STARTED,
	CAM_NODE_FP_STATE_STOPPED,
};

struct cam_node_fastpath {
	struct mutex mutex;
	enum cam_node_fastpath_state state;

	void *priv;
	const struct cam_node_fastpath_ops *ops;
};

struct cam_node_fastpath_ops {
	int (*set_power)(void *priv, int on);
	int (*query_cap)(void *priv, struct cam_query_cap_cmd *query);
	int (*acquire_hw)(void *priv, void *args);
	int (*release_hw)(void *priv, void *args);
	int (*flush_dev)(void *priv, struct cam_flush_dev_cmd *cmd);
	int (*config_dev)(void *priv, struct cam_config_dev_cmd *cmd);
	int (*start_dev)(void *priv, struct cam_start_stop_dev_cmd *cmd);
	int (*stop_dev)(void *priv, struct cam_start_stop_dev_cmd *cmd);
	int (*acquire_dev)(void *priv, struct cam_acquire_dev_cmd *cmd);
	int (*release_dev)(void *priv, struct cam_release_dev_cmd *cmd);
	int (*set_stream_mode)(void *priv, struct cam_set_stream_mode *cmd);
	int (*stream_mode_cmd)(void *priv, struct cam_stream_mode_cmd *cmd);
};

int cam_node_fastpath_ioctl(struct cam_node_fastpath *node,
			    struct cam_control *cmd);

int cam_node_fastpath_poweron(struct cam_node_fastpath *node);
void cam_node_fastpath_shutdown(struct cam_node_fastpath *node);

int cam_node_fastpath_deinit(struct cam_node_fastpath *node);

int cam_node_fastpath_init(struct cam_node_fastpath *node, void *priv,
			   const struct cam_node_fastpath_ops *ops);

#endif /* _CAM_NODE_FASTPATH_H_ */
