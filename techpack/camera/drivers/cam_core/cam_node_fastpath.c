// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <media/cam_req_mgr.h>
#include "cam_debug_util.h"
#include "cam_node_fastpath.h"
#include "cam_isp_context_fastpath.h"

static inline void *cam_node_get_ctx_by_idx(struct cam_node_fastpath *node,
							int ctx_idx)
{
	if (ctx_idx >= node->num_ctx) {
		CAM_ERR(CAM_CORE, "[%s] Invalid context index %d max idx %d",
				ctx_idx, node->num_ctx);
		ctx_idx = 0;
	}
	return (*node->priv)[ctx_idx];
}

static inline void *cam_node_get_ctx(struct cam_node_fastpath *node,
					 int32_t dev_hdl)
{
	int ctx_idx = FP_DEV_GET_HDL_IDX(dev_hdl);

	return cam_node_get_ctx_by_idx(node, ctx_idx);
}

/* Helper to call node ops (with device handler) */
#define cam_node_call_op(node, dev_hdl, op, arg...)  \
	(!((node)->ops) ? -ENODEV : (((node)->ops->op) ?  \
	((node)->ops->op)(cam_node_get_ctx(node, dev_hdl), ##arg) : -ENODEV))

/* Helper to call node  ops (with context index) */
#define cam_ctx_call_op(node, idx, op, arg...)  \
	(!((node)->ops) ? -ENODEV : (((node)->ops->op) ?  \
	((node)->ops->op)(cam_node_get_ctx_by_idx(node, idx), ##arg) : -ENODEV))


#define CTX_STATE(node, ctx_idx)  ((*node->ctx_states)[ctx_idx])

static inline enum cam_node_fastpath_state*
cam_node_get_state_ptr(struct cam_node_fastpath *node, int32_t dev_hndl)
{
	unsigned int idx = FP_DEV_GET_HDL_IDX(dev_hndl);
    if (idx >= node->num_ctx) {
		CAM_ERR(CAM_CORE, "Invalid context index %d!!!", idx);
		idx = 0;
	}
	return &CTX_STATE(node, idx);
}

static bool
cam_node_fastpath_allow_state_change(enum cam_node_fastpath_state cur_state,
			enum cam_node_fastpath_state new_state)
{
	bool allowed_change = false;

	switch (cur_state) {
	case CAM_NODE_FP_STATE_INIT:
		switch (new_state) {
		case CAM_NODE_FP_STATE_ACQUIRED_DEV:
			allowed_change = true;
			break;
		default:
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
			break;
		}
		break;
	case CAM_NODE_FP_STATE_STARTED:
		switch (new_state) {
		case CAM_NODE_FP_STATE_STOPPED:
			allowed_change = true;
			break;
		default:
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
			break;
		}
		break;
	default:
		CAM_ERR(CAM_CORE, "Invalid state %d. This should not happen!",
			cur_state);
		break;
	};

	if (!allowed_change)
		CAM_ERR(CAM_CORE, "State change %d->%d not allowed!",
					cur_state, new_state);

	return allowed_change;
}

/* This is taken from the HW. Call isp fastpath */
static int cam_node_fastpath_query_cap(struct cam_node_fastpath *node,
				       struct cam_control *cmd)
{
	struct cam_query_cap_cmd query = {0};
	int rc = 0;

	if (copy_from_user(&query, u64_to_user_ptr(cmd->handle),
			   sizeof(query))) {
		rc = -EFAULT;
		goto end;
	}

	rc = cam_ctx_call_op(node, 0, query_cap, &query);
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
	int ctx_idx = 0;
	int rc = 0;

	if (copy_from_user(&acquire, u64_to_user_ptr(cmd->handle),
			   sizeof(acquire))) {
		rc = -EFAULT;
		goto end;
	}

	ctx_idx = find_first_zero_bit(&node->ctx_bitmap, node->num_ctx);
	if (ctx_idx < 0 || ctx_idx >= node->num_ctx) {
		CAM_ERR(CAM_ICP, "[%s] could not find free ctx. bitmap 0x%08x num %d", node->node_name,
					node->ctx_bitmap, node->num_ctx);
		rc = -EBADF;
		goto end;
	}

	if (!cam_node_fastpath_allow_state_change(CTX_STATE(node, ctx_idx),
				CAM_NODE_FP_STATE_ACQUIRED_DEV)) {
		rc = -EPERM;
		goto end;
	}

	rc = cam_ctx_call_op(node, ctx_idx, acquire_dev, &acquire);
	if (rc < 0)
		goto end;

	CTX_STATE(node, ctx_idx) = CAM_NODE_FP_STATE_ACQUIRED_DEV;

	if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&acquire, sizeof(acquire))) {
		rc = -EFAULT;
		goto end;
	}

	set_bit(ctx_idx, &node->ctx_bitmap);
end:
	return rc;
}

static int cam_node_fastpath_acquire_hw(struct cam_node_fastpath *node,
					struct cam_control *cmd)
{
	enum cam_node_fastpath_state *p_state;
	int32_t dev_handle;
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
		CAM_ERR(CAM_CORE, "[%s] Unsupported api version %d",
			node->node_name, api_version);
		return -EINVAL;
	}

	acquire_ptr = kzalloc(acquire_size, GFP_KERNEL);
	if (!acquire_ptr) {
		CAM_ERR(CAM_CORE, "[%s] No memory for acquire HW",
			node->node_name);
		return -ENOMEM;
	}

	if (copy_from_user(acquire_ptr, (void __user *)cmd->handle,
		acquire_size)) {
		rc = -EFAULT;
		goto end;
	}

	dev_handle = (api_version == 1) ?
		((struct cam_acquire_hw_cmd_v1 *)acquire_ptr)->dev_handle :
		((struct cam_acquire_hw_cmd_v2 *)acquire_ptr)->dev_handle;

	p_state = cam_node_get_state_ptr(node, dev_handle);

	if (!cam_node_fastpath_allow_state_change(*p_state,
			CAM_NODE_FP_STATE_ACQUIRED_HW)) {
		rc = -EPERM;
		goto end;
	}
	rc = cam_node_call_op(node, dev_handle, acquire_hw, acquire_ptr);
	if (rc < 0)
		goto end;

	*p_state = CAM_NODE_FP_STATE_ACQUIRED_HW;

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
	enum cam_node_fastpath_state *p_state;
	int rc = 0;

	if (copy_from_user(&start, u64_to_user_ptr(cmd->handle),
			sizeof(start)))
		return -EFAULT;

	p_state = cam_node_get_state_ptr(node, start.dev_handle);

	if (!cam_node_fastpath_allow_state_change(*p_state,
			CAM_NODE_FP_STATE_STARTED)) {
		rc = -EPERM;
		goto end;
	}

	rc = cam_node_call_op(node, start.dev_handle, start_dev, &start);

	if (rc == 0)
		*p_state = CAM_NODE_FP_STATE_STARTED;

end:
	return rc;
}

static int cam_node_fastpath_stop_dev(struct cam_node_fastpath *node,
				      struct cam_control *cmd)
{
	struct cam_start_stop_dev_cmd stop;
	enum cam_node_fastpath_state *p_state;
	int rc = 0;

	if (copy_from_user(&stop, u64_to_user_ptr(cmd->handle),
			sizeof(stop)))
		return -EFAULT;

	p_state = cam_node_get_state_ptr(node, stop.dev_handle);

	if (!cam_node_fastpath_allow_state_change(*p_state,
				CAM_NODE_FP_STATE_STOPPED)) {
		rc = -EPERM;
		goto end;
	}

	rc = cam_node_call_op(node, stop.dev_handle, stop_dev, &stop);

	if (rc == 0)
		*p_state = CAM_NODE_FP_STATE_STOPPED;

end:
	return rc;
}

static int cam_node_fastpath_config_dev(struct cam_node_fastpath *node,
					struct cam_control *cmd)
{
	struct cam_config_dev_cmd config;

	if (copy_from_user(&config, u64_to_user_ptr(cmd->handle),
			sizeof(config)))
		return -EFAULT;

	return cam_node_call_op(node, config.dev_handle, config_dev, &config);
}

static int cam_node_fastpath_set_stream_mode(struct cam_node_fastpath *node,
					     struct cam_control *cmd)
{
	struct cam_set_stream_mode stream_mode;

	if (copy_from_user(&stream_mode, u64_to_user_ptr(cmd->handle),
			sizeof(stream_mode)))
		return -EFAULT;

	return cam_node_call_op(node, stream_mode.dev_handle,
			set_stream_mode, &stream_mode);
}

static int cam_node_fastpath_stream_mode_cmd(struct cam_node_fastpath *node,
					     struct cam_control *cmd)
{
	struct cam_stream_mode_cmd stream_mode_cmd;

	if (copy_from_user(&stream_mode_cmd, u64_to_user_ptr(cmd->handle),
			sizeof(stream_mode_cmd)))
		return -EFAULT;

	return cam_node_call_op(node, stream_mode_cmd.dev_handle,
			stream_mode_cmd, &stream_mode_cmd);
}

static int cam_node_fastpath_release_dev(struct cam_node_fastpath *node,
					 struct cam_control *cmd)
{
	struct cam_release_dev_cmd release;
	enum cam_node_fastpath_state *p_state;
	int rc = 0;

	if (copy_from_user(&release, u64_to_user_ptr(cmd->handle),
			   sizeof(release)))
		return -EFAULT;

	p_state = cam_node_get_state_ptr(node, release.dev_handle);

	if (!cam_node_fastpath_allow_state_change(*p_state,
			CAM_NODE_FP_STATE_INIT)) {
		rc = -EPERM;
		goto end;
	}


	rc = cam_node_call_op(node, release.dev_handle, release_dev, &release);

	if (rc == 0) {
		*p_state = CAM_NODE_FP_STATE_INIT;
		clear_bit(FP_DEV_GET_HDL_IDX(release.dev_handle),
			&node->ctx_bitmap);
	}

end:
	return rc;
}

static int cam_node_fastpath_release_hw(struct cam_node_fastpath *node,
				struct cam_control *cmd)
{
	uint32_t api_version;
	struct cam_release_hw_cmd_v1 release_hw;
	enum cam_node_fastpath_state *p_state;
	int rc = 0;

	if (copy_from_user(&api_version, (void __user *)cmd->handle,
			   sizeof(api_version)))
		return -EFAULT;

	if (api_version != 1) {
		CAM_ERR(CAM_CORE, "[%s] Unsupported api version %d",
			node->node_name, api_version);
		return -EINVAL;
	}

	if (copy_from_user(&release_hw, (void __user *)cmd->handle,
				sizeof(struct cam_release_hw_cmd_v1))) {
		return -EFAULT;
	}

	p_state = cam_node_get_state_ptr(node, release_hw.dev_handle);

	if (!cam_node_fastpath_allow_state_change(*p_state,
		CAM_NODE_FP_STATE_ACQUIRED_DEV)) {
		rc = -EPERM;
		goto end;
	}

	rc = cam_node_call_op(node, release_hw.dev_handle, release_hw, &release_hw);

	if (rc == 0)
		*p_state = CAM_NODE_FP_STATE_ACQUIRED_DEV;
end:
	return rc;
}

static int cam_node_fastpath_flush_req(struct cam_node_fastpath *node,
				       struct cam_control *cmd)
{
	struct cam_flush_dev_cmd flush;

	if (copy_from_user(&flush, u64_to_user_ptr(cmd->handle),
			   sizeof(flush)))
		return -EFAULT;

	return cam_node_call_op(node, flush.dev_handle, flush_dev, &flush);
}


int cam_node_fastpath_poweron(struct cam_node_fastpath *node)
{
	int ret;

	/* Call set power only if supported. */
	ret = cam_node_call_op(node, 0, set_power, 1);
	if (ret == -ENODEV)
		ret = 0;

	return ret;
}

void cam_node_fastpath_shutdown(struct cam_node_fastpath *node)
{
	int ctx_idx = 0;

	for_each_set_bit(ctx_idx, &node->ctx_bitmap, node->num_ctx) {

		CAM_ERR(CAM_CORE, "Force Release [%s][%d]",
				node->node_name, ctx_idx);

		mutex_lock(&node->mutex);

		switch (CTX_STATE(node, ctx_idx)) {
		case CAM_NODE_FP_STATE_STARTED:
			cam_ctx_call_op(node, ctx_idx, stop_dev, NULL);
			/* fallthrough */
		case CAM_NODE_FP_STATE_STOPPED:
		case CAM_NODE_FP_STATE_ACQUIRED_HW:
			cam_ctx_call_op(node, ctx_idx, release_hw, NULL);
			/* fallthrough */
		case CAM_NODE_FP_STATE_ACQUIRED_DEV:
			cam_ctx_call_op(node, ctx_idx, release_dev, NULL);
			/* fallthrough */
		case CAM_NODE_FP_STATE_INIT:
		default:
			cam_ctx_call_op(node, ctx_idx, set_power, 0);
			break;
		};
		CTX_STATE(node, ctx_idx) = CAM_NODE_FP_STATE_INIT;
		clear_bit(ctx_idx, &node->ctx_bitmap);
		mutex_unlock(&node->mutex);
	}
}

int cam_node_fastpath_ioctl(struct cam_node_fastpath *node,
			    struct cam_control *cmd)
{
	int rc = 0;

	if (!cmd)
		return -EINVAL;

	mutex_lock(&node->mutex);

	switch (cmd->op_code) {
	case CAM_QUERY_CAP:
		rc = cam_node_fastpath_query_cap(node, cmd);
		break;
	case CAM_ACQUIRE_DEV:
		rc = cam_node_fastpath_acquire_dev(node, cmd);
		break;
	case CAM_ACQUIRE_HW:
		rc = cam_node_fastpath_acquire_hw(node, cmd);
		break;
	case CAM_START_DEV:
		rc = cam_node_fastpath_start_dev(node, cmd);
		break;
	case CAM_STOP_DEV:
		rc = cam_node_fastpath_stop_dev(node, cmd);
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
		rc = cam_node_fastpath_release_dev(node, cmd);
		break;
	case CAM_RELEASE_HW:
		rc = cam_node_fastpath_release_hw(node, cmd);
		break;
	case CAM_FLUSH_REQ:
		rc = cam_node_fastpath_flush_req(node, cmd);
		break;
	case CAM_DUMP_REQ:
		CAM_ERR(CAM_CORE, "[%s] Not implemented %d",
			node->node_name, cmd->op_code);
		break;
	default:
		CAM_ERR(CAM_CORE, "[%s] Unknown op code %d",
			node->node_name, cmd->op_code);
		rc = -EINVAL;
	}

	mutex_unlock(&node->mutex);

	CAM_DBG(CAM_CORE, "[%s] handle cmd %d done ret %d",
		node->node_name, cmd->op_code, rc);

	return rc;
}

int cam_node_fastpath_deinit(struct cam_node_fastpath *node)
{
	if (!node)
		return -EINVAL;

	mutex_destroy(&node->mutex);

	memset(node, 0, sizeof(*node));

	CAM_DBG(CAM_CORE, "[%s] deinit complete", node->node_name);

	return 0;
}

int cam_node_fastpath_init(struct cam_node_fastpath *node,
		const char *node_name,
		void* (*priv)[], int num_ctx,
		const struct cam_node_fastpath_ops *ops)
{
	int ctx_idx;
	int state_size;

	if (!node || !priv || !ops)
		return -EINVAL;

	node->priv = priv;
	node->ops = ops;

	node->ctx_bitmap = 0;
	node->num_ctx = num_ctx;
	strlcpy(node->node_name, node_name, FP_NODE_NAME_SIZE);
	mutex_init(&node->mutex);

	state_size = sizeof(enum cam_node_fastpath_state) * num_ctx;
	node->ctx_states = kzalloc(state_size, GFP_KERNEL);
	if (!node->ctx_states) {
		CAM_ERR(CAM_CORE, "[%s] No memory", node->node_name);
		return -ENOMEM;
	}

	for (ctx_idx = 0; ctx_idx < num_ctx; ctx_idx++)
		CTX_STATE(node, ctx_idx) = CAM_NODE_FP_STATE_INIT;

	return 0;
}
