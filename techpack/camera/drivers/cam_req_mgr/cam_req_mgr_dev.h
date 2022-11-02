/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_REQ_MGR_DEV_H_
#define _CAM_REQ_MGR_DEV_H_

/**
 * struct cam_req_mgr_device - a camera request manager device
 *
 * @video: pointer to struct video device.
 * @v4l2_dev: pointer to struct v4l2 device.
 * @subdev_nodes_created: flag to check the created state.
 * @count: number of subdevices registered.
 * @dev_lock: lock for the subdevice count.
 * @state: state of the root device.
 * @open_cnt: open count of subdev
 * @cam_lock: per file handle lock
 * @cam_eventq: event queue
 * @cam_eventq_lock: lock for event queue
 * @shutdown_state: shutdown state
 */
struct cam_req_mgr_device {
	struct video_device *video;
	struct v4l2_device *v4l2_dev;
	bool subdev_nodes_created;
	int count;
	struct mutex dev_lock;
	bool state;
	int32_t open_cnt;
	struct mutex cam_lock;
	struct v4l2_fh  *cam_eventq;
	spinlock_t cam_eventq_lock;
	bool shutdown_state;
};

#define CAM_REQ_MGR_GET_PAYLOAD_PTR(ev, type)        \
	(type *)((char *)ev.u.data)

int cam_req_mgr_notify_message(struct cam_req_mgr_message *msg,
	uint32_t id,
	uint32_t type);

#endif /* _CAM_REQ_MGR_DEV_H_ */
