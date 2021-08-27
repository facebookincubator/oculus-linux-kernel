// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "cam_debug_util.h"
#include "cam_node_fastpath.h"
#include "cam_isp_context_fastpath.h"

/* Helper to call node ops  */
#define cam_node_call_op(node, op, arg...)             \
	(!((node)->ops) ? -ENODEV : (((node)->ops->op) ?  \
	((node)->ops->op)((node->priv), ##arg) : -ENODEV))

/* This is taken from the HW. Call isp fastpath */
static int cam_node_fastpath_query_cap(struct cam_node_fastpath *node,
				       struct cam_control *cmd)
{
	struct cam_query_cap_cmd query;
	int rc = 0;

	if (copy_from_user(&query, u64_to_user_ptr(cmd->handle),
			   sizeof(query))) {
		rc = -EFAULT;
		goto end;
	}

	rc = cam_node_call_op(node, query_cap, &query);
	if (rc < 0)
		goto end;

	if (copy_to_user(u64_to_user_ptr(cmd->handle), &query,
			 sizeof(query)))
		rc = -EFAULT;

end:
	return rc;
}

static int cam_node_fastpath_acquire_dev(struct cam_node_fastpath *node,
					 struct cam_control *cmd)
{
	struct cam_acquire_dev_cmd acquire;
	int rc = 0;

	if (copy_from_user(&acquire, u64_to_user_ptr(cmd->handle),
			   sizeof(acquire))) {
		rc = -EFAULT;
		goto end;
	}

	rc = cam_node_call_op(node, acquire_dev, &acquire);
	if (rc < 0)
		goto end;

	if (copy_to_user(u64_to_user_ptr(cmd->handle), &acquire,
		sizeof(acquire)))
		rc = -EFAULT;
end:
	return rc;
}

static int cam_node_fastpath_acquire_hw(struct cam_node_fastpath *node,
					struct cam_control *cmd)
{
	uint32_t api_version;
	void *acquire_ptr = NULL;
	size_t acquire_size;
	int rc = 0;

	if (copy_from_user(&api_version, (void __user *)cmd->handle,
			sizeof(api_version))) {
		return -EFAULT;
	}

	if (api_version == 1)
		acquire_size = sizeof(struct cam_acquire_hw_cmd_v1);
	else if (api_version == 2)
		acquire_size = sizeof(struct cam_acquire_hw_cmd_v2);
	else {
		CAM_ERR(CAM_CORE, "Unsupported api version %d",
			api_version);
		return -EINVAL;
	}

	acquire_ptr = kzalloc(acquire_size, GFP_KERNEL);
	if (!acquire_ptr) {
		CAM_ERR(CAM_CORE, "No memory for acquire HW");
		return -ENOMEM;
	}

	if (copy_from_user(acquire_ptr, (void __user *)cmd->handle,
		acquire_size)) {
		rc = -EFAULT;
		goto end;
	}

	rc = cam_node_call_op(node, acquire_hw, acquire_ptr);
	if (rc < 0)
		goto end;

	if (copy_to_user((void __user *)cmd->handle, acquire_ptr,
		acquire_size))
		rc = -EFAULT;

end:
	kfree(acquire_ptr);
	return rc;
}

static int cam_node_fastpath_start_dev(struct cam_node_fastpath *node,
				struct cam_control *cmd)
{
	struct cam_start_stop_dev_cmd start;

	if (copy_from_user(&start, u64_to_user_ptr(cmd->handle),
			sizeof(start)))
		return -EFAULT;

	return cam_node_call_op(node, start_dev, &start);
}

static int cam_node_fastpath_stop_dev(struct cam_node_fastpath *node,
				      struct cam_control *cmd)
{
	struct cam_start_stop_dev_cmd stop;

	if (copy_from_user(&stop, u64_to_user_ptr(cmd->handle),
			sizeof(stop)))
		return -EFAULT;

	return cam_node_call_op(node, stop_dev, &stop);
}

static int cam_node_fastpath_config_dev(struct cam_node_fastpath *node,
					struct cam_control *cmd)
{
	struct cam_config_dev_cmd config;

	if (copy_from_user(&config, u64_to_user_ptr(cmd->handle),
			sizeof(config)))
		return -EFAULT;

	return cam_node_call_op(node, config_dev, &config);
}

static int cam_node_fastpath_set_stream_mode(struct cam_node_fastpath *node,
					     struct cam_control *cmd)
{
	struct cam_set_stream_mode stream_mode;

	if (copy_from_user(&stream_mode, u64_to_user_ptr(cmd->handle),
			sizeof(stream_mode)))
		return -EFAULT;

	return cam_node_call_op(node, set_stream_mode, &stream_mode);
}

static int cam_node_fastpath_stream_mode_cmd(struct cam_node_fastpath *node,
					     struct cam_control *cmd)
{
	struct cam_stream_mode_cmd stream_mode_cmd;

	if (copy_from_user(&stream_mode_cmd, u64_to_user_ptr(cmd->handle),
			sizeof(stream_mode_cmd)))
		return -EFAULT;

	return cam_node_call_op(node, stream_mode_cmd, &stream_mode_cmd);
}

static int cam_node_fastpath_release_dev(struct cam_node_fastpath *node,
					 struct cam_control *cmd)
{
	struct cam_release_dev_cmd release;

	if (copy_from_user(&release, u64_to_user_ptr(cmd->handle),
			   sizeof(release)))
		return -EFAULT;

	return cam_node_call_op(node, release_dev, &release);
}

static int cam_node_fastpath_release_hw(struct cam_node_fastpath *node,
					struct cam_control *cmd)
{
	uint32_t api_version;
	size_t release_size;
	void *release_ptr = NULL;
	int rc = 0;

	if (copy_from_user(&api_version, (void __user *)cmd->handle,
			   sizeof(api_version)))
		return -EFAULT;

	if (api_version == 1)
		release_size = sizeof(struct cam_release_hw_cmd_v1);
	else {
		CAM_ERR(CAM_CORE, "Unsupported api version %d",
			api_version);
		return -EINVAL;
	}

	release_ptr = kzalloc(release_size, GFP_KERNEL);
	if (!release_ptr) {
		CAM_ERR(CAM_CORE, "No memory for release HW");
		return -ENOMEM;
	}

	if (copy_from_user(release_ptr, (void __user *)cmd->handle,
			   release_size)) {
		rc = -EFAULT;
		goto release_kfree;
	}


	rc = cam_node_call_op(node, release_hw, release_ptr);

release_kfree:
	kfree(release_ptr);
	return rc;
}

static int cam_node_fastpath_flush_req(struct cam_node_fastpath *node,
				       struct cam_control *cmd)
{
	struct cam_flush_dev_cmd flush;

	if (copy_from_user(&flush, u64_to_user_ptr(cmd->handle),
			   sizeof(flush)))
		return -EFAULT;

	return cam_node_call_op(node, flush_dev, &flush);
}

static bool
cam_node_fastpath_allow_state_change(struct cam_node_fastpath *node,
				     enum cam_node_fastpath_state new_state)
{
	bool allowed_change = false;

	switch (node->state) {
	case CAM_NODE_FP_STATE_INIT:
		switch (new_state) {
		case CAM_NODE_FP_STATE_ACQUIRED_DEV:
			allowed_change = true;
			break;
		default:
			allowed_change = false;
			break;
		}
		break;
	case CAM_NODE_FP_STATE_ACQUIRED_DEV:
		switch (new_state) {
		case CAM_NODE_FP_STATE_INIT:
		case CAM_NODE_FP_STATE_ACQUIRED_HW:
		case CAM_NODE_FP_STATE_STARTED:
			allowed_change = true;
			break;
		default:
			allowed_change = false;
			break;
		}
		break;
	case CAM_NODE_FP_STATE_ACQUIRED_HW:
		switch (new_state) {
		case CAM_NODE_FP_STATE_ACQUIRED_DEV:
		case CAM_NODE_FP_STATE_STARTED:
			allowed_change = true;
			break;
		default:
			allowed_change = false;
			break;
		}
		break;
	case CAM_NODE_FP_STATE_STARTED:
		switch (new_state) {
		case CAM_NODE_FP_STATE_STOPPED:
			allowed_change = true;
			break;
		default:
			allowed_change = false;
			break;
		}
		break;
	case CAM_NODE_FP_STATE_STOPPED:
		switch (new_state) {
		case CAM_NODE_FP_STATE_STARTED:
		case CAM_NODE_FP_STATE_ACQUIRED_DEV:
			allowed_change = true;
			break;
		default:
			allowed_change = false;
			break;
		}
		break;
	default:
		CAM_ERR(CAM_CORE, "Invalid state this should not happen!");
		break;
	};

	if (!allowed_change)
		CAM_ERR(CAM_CORE, "State change %d->%d not allowed!",
			node->state, new_state);

	return allowed_change;
}

int cam_node_fastpath_poweron(struct cam_node_fastpath *node)
{
	int ret;

	/* Call set power only if supported */
	ret = cam_node_call_op(node, set_power, 1);
	if (ret == -ENODEV)
		ret = 0;

	return ret;
}

void cam_node_fastpath_shutdown(struct cam_node_fastpath *node)
{
	mutex_lock(&node->mutex);

	switch (node->state) {
	case CAM_NODE_FP_STATE_STARTED:
		cam_node_call_op(node, stop_dev, NULL);
		/* fallthrough */
	case CAM_NODE_FP_STATE_STOPPED:
	case CAM_NODE_FP_STATE_ACQUIRED_HW:
		cam_node_call_op(node, release_hw, NULL);
		/* fallthrough */
	case CAM_NODE_FP_STATE_ACQUIRED_DEV:
		cam_node_call_op(node, release_dev, NULL);
		/* fallthrough */
	case CAM_NODE_FP_STATE_INIT:
	default:
		cam_node_call_op(node, set_power, 0);
		node->state = CAM_NODE_FP_STATE_INIT;
		break;
	};

	mutex_unlock(&node->mutex);
}

int cam_node_fastpath_ioctl(struct cam_node_fastpath *node,
			    struct cam_control *cmd)
{
	int rc = 0;
	bool allow;

	if (!cmd)
		return -EINVAL;

	CAM_DBG(CAM_CORE, "handle cmd %d", cmd->op_code);

	mutex_lock(&node->mutex);

	switch (cmd->op_code) {
	case CAM_QUERY_CAP:
		rc = cam_node_fastpath_query_cap(node, cmd);
		break;
	case CAM_ACQUIRE_DEV:
		allow = cam_node_fastpath_allow_state_change(node,
				CAM_NODE_FP_STATE_ACQUIRED_DEV);
		if (!allow)
			goto done;

		rc = cam_node_fastpath_acquire_dev(node, cmd);
		if (!rc)
			node->state = CAM_NODE_FP_STATE_ACQUIRED_DEV;
		break;
	case CAM_ACQUIRE_HW:
		allow = cam_node_fastpath_allow_state_change(node,
				CAM_NODE_FP_STATE_ACQUIRED_HW);
		if (!allow)
			goto done;

		rc = cam_node_fastpath_acquire_hw(node, cmd);
		if (!rc)
			node->state = CAM_NODE_FP_STATE_ACQUIRED_HW;
		break;
	case CAM_START_DEV:
		allow = cam_node_fastpath_allow_state_change(node,
				CAM_NODE_FP_STATE_STARTED);
		if (!allow)
			goto done;

		rc = cam_node_fastpath_start_dev(node, cmd);
		if (!rc)
			node->state = CAM_NODE_FP_STATE_STARTED;
		break;
	case CAM_STOP_DEV:
		allow = cam_node_fastpath_allow_state_change(node,
				CAM_NODE_FP_STATE_STOPPED);
		if (!allow)
			goto done;

		rc = cam_node_fastpath_stop_dev(node, cmd);

		if (!rc)
			node->state = CAM_NODE_FP_STATE_STOPPED;
		break;
	case CAM_CONFIG_DEV:
		rc = cam_node_fastpath_config_dev(node, cmd);
		break;
	case CAM_SET_STREAM_MODE:
		rc = cam_node_fastpath_set_stream_mode(node, cmd);
		break;
	case CAM_STREAM_MODE_CMD:
		rc = cam_node_fastpath_stream_mode_cmd(node, cmd);
		break;
	case CAM_RELEASE_DEV:
		allow = cam_node_fastpath_allow_state_change(node,
				CAM_NODE_FP_STATE_INIT);

		rc = cam_node_fastpath_release_dev(node, cmd);
		if (!rc)
			node->state = CAM_NODE_FP_STATE_INIT;
		break;
	case CAM_RELEASE_HW:
		allow = cam_node_fastpath_allow_state_change(node,
				CAM_NODE_FP_STATE_ACQUIRED_DEV);

		rc = cam_node_fastpath_release_hw(node, cmd);
		if (!rc)
			node->state = CAM_NODE_FP_STATE_ACQUIRED_DEV;
		break;
	case CAM_FLUSH_REQ:
		rc = cam_node_fastpath_flush_req(node, cmd);
		break;
	case CAM_DUMP_REQ:
		CAM_ERR(CAM_CORE, "Not implemented %d", cmd->op_code);
		break;
	default:
		CAM_ERR(CAM_CORE, "Unknown op code %d", cmd->op_code);
		rc = -EINVAL;
	}

done:
	mutex_unlock(&node->mutex);

	CAM_DBG(CAM_CORE, "handle cmd %d done ret %d", cmd->op_code, rc);
	return rc;
}

int cam_node_fastpath_deinit(struct cam_node_fastpath *node)
{
	if (!node)
		return -EINVAL;

	mutex_destroy(&node->mutex);

	memset(node, 0, sizeof(*node));

	CAM_DBG(CAM_CORE, "deinit complete");

	return 0;
}

int cam_node_fastpath_init(struct cam_node_fastpath *node, void *priv,
			   const struct cam_node_fastpath_ops *ops)
{
	if (!node || !priv || !ops)
		return -EINVAL;

	node->priv = priv;
	node->ops = ops;

	mutex_init(&node->mutex);
	node->state = CAM_NODE_FP_STATE_INIT;

	return 0;
}
