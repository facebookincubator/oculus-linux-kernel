// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#if IS_REACHABLE(CONFIG_MSM_GLOBAL_SYNX)
#include <synx_api.h>
#endif
#include "cam_sync_util.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_compat.h"
#include "camera_main.h"
#include "cam_req_mgr_worker_wrapper.h"

struct sync_device *sync_dev;
struct sync_uid_info sync_uid_access;

/*
 * Flag to determine whether to enqueue cb of a
 * signaled fence onto the worker or invoke it
 * directly in the same context
 */
static bool trigger_cb_without_switch;

static void cam_sync_print_fence_table(void)
{
	int idx;

	for (idx = 0; idx < CAM_SYNC_MAX_OBJS; idx++) {
		spin_lock_bh(&sync_dev->row_spinlocks[idx]);
		CAM_INFO(CAM_SYNC,
			"index[%u]: sync_id=%d, name=%s, type=%d, state=%d, ref_cnt=%d",
			idx,
			sync_dev->sync_table[idx].sync_id,
			sync_dev->sync_table[idx].name,
			sync_dev->sync_table[idx].type,
			sync_dev->sync_table[idx].state,
			atomic_read(&sync_dev->sync_table[idx].ref_cnt));
		spin_unlock_bh(&sync_dev->row_spinlocks[idx]);
	}
}

int cam_sync_create(int32_t *sync_obj, const char *name)
{
	int rc;
	long idx;
	bool bit;

	do {
		idx = find_first_zero_bit(sync_dev->bitmap, CAM_SYNC_MAX_OBJS);
		if (idx >= CAM_SYNC_MAX_OBJS) {
			CAM_ERR(CAM_SYNC,
				"Error: Unable to create sync idx = %d sync name = %s reached max!",
				idx, name);
			cam_sync_print_fence_table();
			return -ENOMEM;
		}
		CAM_DBG(CAM_SYNC, "Index location available at idx: %ld", idx);
		bit = test_and_set_bit(idx, sync_dev->bitmap);
	} while (bit);

	spin_lock_bh(&sync_dev->row_spinlocks[idx]);
	rc = cam_sync_init_row(sync_dev->sync_table, idx, name,
		CAM_SYNC_TYPE_INDV);
	if (rc) {
		CAM_ERR(CAM_SYNC, "Error: Unable to init row at idx = %ld",
			idx);
		clear_bit(idx, sync_dev->bitmap);
		spin_unlock_bh(&sync_dev->row_spinlocks[idx]);
		return -EINVAL;
	}

	*sync_obj = idx;
	CAM_DBG(CAM_SYNC, "sync_obj: %s[%i]", name, *sync_obj);
	spin_unlock_bh(&sync_dev->row_spinlocks[idx]);

	return rc;
}

int cam_sync_get_version(void) {

	return sync_dev->version;

}

int cam_sync_register_callback(sync_callback cb_func,
	void *userdata, int32_t sync_var)
{
	struct sync_callback_info *sync_cb;
	struct sync_table_row *row = NULL;
	struct crm_worker_task *task = NULL;
	int status = 0;
	uint32_t sync_obj, uid_validity;
	uint16_t sync_uid;
	int rc;

	sync_obj = (uint32_t)sync_var & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)sync_var >> sync_uid_access.uidShift;

	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0 || !cb_func)
		return -EINVAL;

	spin_lock_bh(&sync_dev->row_spinlocks[sync_obj]);
	row = sync_dev->sync_table + sync_obj;

	uid_validity = cam_sync_check_uid_valid(sync_var);

	if (uid_validity == SYNC_UID_NEW) {
		rc = cam_sync_reinit_object(sync_dev->sync_table, sync_var);
	} else if (uid_validity == SYNC_UID_OLD) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC, "Called for invalid fence, sync obj: %d, uid: %d",
			sync_obj,
			sync_uid);
		return -EINVAL;
	}

	if (row->state == CAM_SYNC_STATE_INVALID) {
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj %s[%d]",
			row->name,
			sync_obj);
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		return -EINVAL;
	}

	sync_cb = kzalloc(sizeof(*sync_cb), GFP_ATOMIC);
	if (!sync_cb) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		return -ENOMEM;
	}

	/* Trigger callback if sync object is already in SIGNALED state */
	if (((row->state == CAM_SYNC_STATE_SIGNALED_SUCCESS) ||
		(row->state == CAM_SYNC_STATE_SIGNALED_ERROR) ||
		(row->state == CAM_SYNC_STATE_SIGNALED_CANCEL)) &&
		(!row->remaining)) {
		if (trigger_cb_without_switch) {
			CAM_DBG(CAM_SYNC, "Invoke callback for sync object:%s[%d], sync_uid: %d",
				row->name,
				sync_obj,
				sync_uid);
			status = row->state;
			kfree(sync_cb);
			spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
			cb_func(sync_var, status, userdata);
		} else {
			sync_cb->callback_func = cb_func;
			sync_cb->cb_data = userdata;
			sync_cb->sync_obj = sync_var;
			sync_cb->status = row->state;
			CAM_DBG(CAM_SYNC, "Enqueue callback for sync object:%s[%d], sync_uid: %d",
				row->name,
				sync_cb->sync_obj,
				sync_uid);

			task = cam_req_mgr_worker_get_task(sync_dev->worker);
			if (IS_ERR_OR_NULL(task)) {
				CAM_ERR(CAM_RPMSG, "Failed to get task = %d", PTR_ERR(task));
				cb_func(sync_var, status, userdata);
				kfree(sync_cb);
			} else {
				task->payload = sync_cb;
				task->process_cb = cam_sync_util_cb_dispatch;
				cam_req_mgr_worker_enqueue_task(task, NULL, CRM_TASK_PRIORITY_0);
			}
			spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		}

		return 0;
	}

	sync_cb->callback_func = cb_func;
	sync_cb->cb_data = userdata;
	sync_cb->sync_obj = sync_var;
	list_add_tail(&sync_cb->list, &row->callback_list);
	spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);

	return 0;
}

int cam_sync_deregister_callback(sync_callback cb_func,
	void *userdata, int32_t sync_var)
{
	struct sync_table_row *row = NULL;
	struct sync_callback_info *sync_cb, *temp;
	bool found = false;
	uint32_t sync_obj;
	uint16_t sync_uid;

	sync_obj = (uint32_t)sync_var & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)sync_var >> sync_uid_access.uidShift;

	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0)
		return -EINVAL;

	spin_lock_bh(&sync_dev->row_spinlocks[sync_obj]);
	row = sync_dev->sync_table + sync_obj;

	if (row->uid != sync_uid) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC, "deregister called for invalid fence, sync obj: %d, uid: %d",
			sync_obj, sync_uid);
		return -EINVAL;
	}

	if (row->state == CAM_SYNC_STATE_INVALID) {
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj = %s[%d]",
			row->name,
			sync_obj);
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		return -EINVAL;
	}

	CAM_DBG(CAM_SYNC, "deregistered callback for sync object:%s[%d], uid: %d",
		row->name,
		sync_obj,
		sync_uid);
	list_for_each_entry_safe(sync_cb, temp, &row->callback_list, list) {
		if (sync_cb->callback_func == cb_func &&
			sync_cb->cb_data == userdata) {
			list_del_init(&sync_cb->list);
			kfree(sync_cb);
			found = true;
		}
	}

	spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
	return found ? 0 : -ENOENT;
}

int cam_sync_signal(struct cam_sync_signal_param *param,
	struct cam_sync_timestamp *time_stamp)
{
	struct sync_table_row *row = NULL;
	struct sync_table_row *parent_row = NULL;
	struct sync_parent_info *parent_info, *temp_parent_info;
	struct list_head parents_list;
	int rc = 0;
	uint32_t sync_obj, psync_obj, uid_validity;
	int16_t sync_uid;

	sync_obj = param->sync_obj & sync_uid_access.fenceIdMask;
	sync_uid = param->sync_obj >> sync_uid_access.uidShift;

	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0) {
		CAM_ERR(CAM_SYNC, "Error: Out of range sync obj (0 <= %d < %d)",
			param->sync_obj, CAM_SYNC_MAX_OBJS);
		return -EINVAL;
	}
	row = sync_dev->sync_table + sync_obj;
	spin_lock_bh(&sync_dev->row_spinlocks[sync_obj]);

	/* Check if it is a valid fence, i.e., either current fence uid or newer */
	uid_validity = cam_sync_check_uid_valid(param->sync_obj);
	if (uid_validity == SYNC_UID_NEW) {
		rc = cam_sync_reinit_object(sync_dev->sync_table, param->sync_obj);
	} else if (uid_validity == SYNC_UID_OLD) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC, "Signaling an old fence, sync obj: %d, uid: %d, state: %d",
			sync_obj,
			sync_uid,
			param->status);
		return -EINVAL;
	}

	if (row->state == CAM_SYNC_STATE_INVALID) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj = %s[%d], uid = %d",
			row->name,
			sync_obj,
			sync_uid);
		return -EINVAL;
	}

	if (row->type == CAM_SYNC_TYPE_GROUP) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC,
			"Error: Signaling a GROUP sync object = %s[%d], uid = %d",
			row->name,
			sync_obj,
			sync_uid);
		return -EINVAL;
	}

	if (row->state != CAM_SYNC_STATE_ACTIVE) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC,
			"Error: Sync object already signaled sync_obj = %s[%d], uid = %d",
			row->name,
			sync_obj,
			sync_uid);
		return -EALREADY;
	}

	if ((param->status != CAM_SYNC_STATE_SIGNALED_SUCCESS) &&
		(param->status != CAM_SYNC_STATE_SIGNALED_ERROR) &&
		(param->status != CAM_SYNC_STATE_SIGNALED_CANCEL)) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC,
			"Error: signaling with undefined status = %d event reason = %u",
			param->status, param->event_cause);
		return -EINVAL;
	}

	if (!atomic_dec_and_test(&row->ref_cnt)) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		return 0;
	}

	row->state = param->status;
	cam_sync_util_dispatch_signaled_cb(param, time_stamp);

	/* copy parent list to local and release child lock */
	INIT_LIST_HEAD(&parents_list);
	list_splice_init(&row->parents_list, &parents_list);
	spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);

	if (list_empty(&parents_list))
		goto signal_success_exit;

	/*
	 * Now iterate over all parents of this object and if they too need to
	 * be signaled dispatch cb's
	 */
	list_for_each_entry_safe(parent_info,
		temp_parent_info,
		&parents_list,
		list) {
		psync_obj = parent_info->sync_id & sync_uid_access.fenceIdMask;
		parent_row = sync_dev->sync_table + psync_obj;
		spin_lock_bh(&sync_dev->row_spinlocks[psync_obj]);
		parent_row->remaining--;

		rc = cam_sync_util_update_parent_state(
			parent_row,
			param->status);
		if (rc) {
			CAM_ERR(CAM_SYNC, "Invalid parent state %d",
				parent_row->state);
			spin_unlock_bh(
				&sync_dev->row_spinlocks[psync_obj]);
			kfree(parent_info);
			continue;
		}
		param->sync_obj = parent_info->sync_id;
		param->status = parent_row->state;
		if (!parent_row->remaining)
			cam_sync_util_dispatch_signaled_cb(param, time_stamp);

		spin_unlock_bh(&sync_dev->row_spinlocks[psync_obj]);
		list_del_init(&parent_info->list);
		kfree(parent_info);
	}

signal_success_exit:
	return 0;
}

int cam_sync_merge(int32_t *sync_var, uint32_t num_objs, int32_t *merged_obj)
{
	int rc;
	long idx = 0;
	bool bit;
	int i = 0;
	uint32_t sync_obj;
	uint16_t sync_uid;

	if (!sync_var || !merged_obj) {
		CAM_ERR(CAM_SYNC, "Invalid pointer(s)");
		return -EINVAL;
	}

	if (num_objs <= 1) {
		CAM_ERR(CAM_SYNC, "Single object merge is not allowed");
		return -EINVAL;
	}

	if (cam_common_util_remove_duplicate_arr(sync_var, num_objs)
		!= num_objs) {
		CAM_ERR(CAM_SYNC, "The obj list has duplicate fence");
		return -EINVAL;
	}

	for (i = 0; i < num_objs; i++) {
		sync_obj = (uint32_t)(sync_var[i]) & sync_uid_access.fenceIdMask;
		sync_uid = (uint32_t)(sync_var[i]) >> sync_uid_access.uidShift;
		rc = cam_sync_check_valid(sync_obj);
		if (rc) {
			CAM_ERR(CAM_SYNC, "Sync_obj[%d] %d valid check fail",
				i, sync_obj);
			return rc;
		}
	}
	do {
		idx = find_first_zero_bit(sync_dev->bitmap, CAM_SYNC_MAX_OBJS);
		if (idx >= CAM_SYNC_MAX_OBJS)
			return -ENOMEM;
		bit = test_and_set_bit(idx, sync_dev->bitmap);
	} while (bit);

	spin_lock_bh(&sync_dev->row_spinlocks[idx]);
	rc = cam_sync_init_group_object(sync_dev->sync_table,
		idx, sync_var,
		num_objs);
	if (rc < 0) {
		CAM_ERR(CAM_SYNC, "Error: Unable to init row at idx = %ld",
			idx);
		clear_bit(idx, sync_dev->bitmap);
		spin_unlock_bh(&sync_dev->row_spinlocks[idx]);
		return -EINVAL;
	}
	CAM_DBG(CAM_SYNC, "Init row at idx:%ld to merge objects", idx);
	*merged_obj = idx;
	spin_unlock_bh(&sync_dev->row_spinlocks[idx]);

	return 0;
}

int cam_sync_get_obj_ref(int32_t sync_var)
{
	struct sync_table_row *row = NULL;
	uint32_t sync_obj;
	uint16_t sync_uid;

	sync_obj = (uint32_t)sync_var & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)sync_var >> sync_uid_access.uidShift;

	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0)
		return -EINVAL;

	row = sync_dev->sync_table + sync_obj;

	spin_lock(&sync_dev->row_spinlocks[sync_obj]);

	if (row->state != CAM_SYNC_STATE_ACTIVE) {
		spin_unlock(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj = %s[%d]",
			row->name,
			sync_obj);
		return -EINVAL;
	}

	atomic_inc(&row->ref_cnt);
	spin_unlock(&sync_dev->row_spinlocks[sync_obj]);
	CAM_DBG(CAM_SYNC, "get ref for obj %d, uid: %d", sync_obj, sync_uid);

	return 0;
}

int cam_sync_put_obj_ref(int32_t sync_var)
{
	struct sync_table_row *row = NULL;
	uint32_t sync_obj;
	uint16_t sync_uid;

	sync_obj = (uint32_t)sync_var & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)sync_var >> sync_uid_access.uidShift;

	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0)
		return -EINVAL;

	row = sync_dev->sync_table + sync_obj;
	atomic_dec(&row->ref_cnt);
	CAM_DBG(CAM_SYNC, "put ref for obj %d, row_state:%u, uid: %d",
		sync_obj,
		row->state,
		sync_uid);

	return 0;
}

int cam_sync_destroy(int32_t sync_var)
{
	return cam_sync_deinit_object(sync_dev->sync_table, sync_var);
}

int cam_sync_check_valid(int32_t sync_var)
{
	struct sync_table_row *row = NULL;
	uint32_t sync_obj;
	uint16_t sync_uid;

	sync_obj = (uint32_t)sync_var & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)sync_var >> sync_uid_access.uidShift;

	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0)
		return -EINVAL;

	row = sync_dev->sync_table + sync_obj;

	if (!test_bit(sync_obj, sync_dev->bitmap)) {
		CAM_ERR(CAM_SYNC, "Error: Released sync obj received %s[%d]",
			row->name,
			sync_obj);
		return -EINVAL;
	}

	if (row->state == CAM_SYNC_STATE_INVALID) {
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj = %s[%d]",
			row->name,
			sync_obj);
		return -EINVAL;
	}
	return 0;
}

int cam_sync_wait(int32_t sync_var, uint64_t timeout_ms)
{
	unsigned long timeleft;
	int rc = -EINVAL;
	struct sync_table_row *row = NULL;
	uint32_t sync_obj;
	uint16_t sync_uid;

	sync_obj = (uint32_t)sync_var & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)sync_var >> sync_uid_access.uidShift;

	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0)
		return -EINVAL;

	row = sync_dev->sync_table + sync_obj;

	if (row->state == CAM_SYNC_STATE_INVALID) {
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj = %s[%d], uid = %d",
			row->name,
			sync_obj,
			sync_uid);
		return -EINVAL;
	}

	timeleft = cam_common_wait_for_completion_timeout(&row->signaled,
		msecs_to_jiffies(timeout_ms));

	if (!timeleft) {
		CAM_ERR(CAM_SYNC,
			"Error: timed out for sync obj = %s[%d], uid = %d",
			row->name,
			sync_obj,
			sync_uid);
		rc = -ETIMEDOUT;
	} else {
		switch (row->state) {
		case CAM_SYNC_STATE_INVALID:
		case CAM_SYNC_STATE_ACTIVE:
		case CAM_SYNC_STATE_SIGNALED_ERROR:
		case CAM_SYNC_STATE_SIGNALED_CANCEL:
			CAM_ERR(CAM_SYNC,
				"Error: Wait on invalid state = %d, obj = %d, name = %s, uid = %d",
				row->state, sync_obj, row->name, sync_uid);
			rc = -EINVAL;
			break;
		case CAM_SYNC_STATE_SIGNALED_SUCCESS:
			rc = 0;
			break;
		default:
			rc = -EINVAL;
			break;
		}
	}

	return rc;
}

static int cam_sync_handle_create(struct cam_private_ioctl_arg *k_ioctl)
{
	struct cam_sync_info sync_create;
	int result;

	if (k_ioctl->size != sizeof(struct cam_sync_info))
		return -EINVAL;

	if (!k_ioctl->ioctl_ptr)
		return -EINVAL;

	if (copy_from_user(&sync_create,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;
	sync_create.name[SYNC_DEBUG_NAME_LEN] = '\0';

	result = cam_sync_create(&sync_create.sync_obj,
		sync_create.name);

	if (!result)
		if (copy_to_user(
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&sync_create,
			k_ioctl->size))
			return -EFAULT;

	return result;
}

static int cam_sync_handle_signal(struct cam_private_ioctl_arg *k_ioctl)
{
	int rc = 0;
	struct cam_sync_signal sync_signal;
	struct cam_sync_signal_param param;
	uint32_t uid_validity;

	if (k_ioctl->size != sizeof(struct cam_sync_signal))
		return -EINVAL;

	if (!k_ioctl->ioctl_ptr)
		return -EINVAL;

	if (copy_from_user(&sync_signal,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	uid_validity = cam_sync_check_uid_valid(sync_signal.sync_obj);
	if (uid_validity == SYNC_UID_NEW) {
		rc = cam_sync_reinit_object(sync_dev->sync_table, sync_signal.sync_obj);
	} else if (uid_validity == SYNC_UID_OLD) {
		CAM_ERR(CAM_SYNC, "Signaling an old fence, sync : %d (0x%x)",
			sync_signal.sync_obj, sync_signal.sync_obj);
		return -EINVAL;
	}

	/* need to get ref for UMD signaled fences */
	rc = cam_sync_get_obj_ref(sync_signal.sync_obj);
	if (rc) {
		CAM_DBG(CAM_SYNC,
			"Error: cannot signal an uninitialized sync obj = %d",
			sync_signal.sync_obj);
		return rc;
	}
	memset(&param, 0, sizeof(param));
	param.sync_obj = sync_signal.sync_obj;
	param.status = sync_signal.sync_state;
	param.event_cause = CAM_SYNC_COMMON_SYNC_SIGNAL_EVENT;
	return cam_sync_signal(&param, NULL);
}

static int cam_sync_handle_merge(struct cam_private_ioctl_arg *k_ioctl)
{
	struct cam_sync_merge sync_merge;
	uint32_t *sync_objs;
	uint32_t num_objs;
	uint32_t size;
	int result;

	if (k_ioctl->size != sizeof(struct cam_sync_merge))
		return -EINVAL;

	if (!k_ioctl->ioctl_ptr)
		return -EINVAL;

	if (copy_from_user(&sync_merge,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	if (sync_merge.num_objs >= CAM_SYNC_MAX_OBJS)
		return -EINVAL;

	size = sizeof(uint32_t) * sync_merge.num_objs;
	sync_objs = kzalloc(size, GFP_ATOMIC);

	if (!sync_objs)
		return -ENOMEM;

	if (copy_from_user(sync_objs,
		u64_to_user_ptr(sync_merge.sync_objs),
		sizeof(uint32_t) * sync_merge.num_objs)) {
		kfree(sync_objs);
		return -EFAULT;
	}

	num_objs = sync_merge.num_objs;

	result = cam_sync_merge(sync_objs,
		num_objs,
		&sync_merge.merged);

	if (!result)
		if (copy_to_user(
			u64_to_user_ptr(k_ioctl->ioctl_ptr),
			&sync_merge,
			k_ioctl->size)) {
			kfree(sync_objs);
			return -EFAULT;
	}

	kfree(sync_objs);

	return result;
}

static int cam_sync_handle_wait(struct cam_private_ioctl_arg *k_ioctl)
{
	struct cam_sync_wait sync_wait;

	if (k_ioctl->size != sizeof(struct cam_sync_wait))
		return -EINVAL;

	if (!k_ioctl->ioctl_ptr)
		return -EINVAL;

	if (copy_from_user(&sync_wait,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	k_ioctl->result = cam_sync_wait(sync_wait.sync_obj,
		sync_wait.timeout_ms);

	return 0;
}

static inline int cam_sync_handle_exit_poll(void)
{
	return cam_sync_util_send_exit_poll_event();
}

static int cam_sync_handle_destroy(struct cam_private_ioctl_arg *k_ioctl)
{
	struct cam_sync_info sync_create;

	if (k_ioctl->size != sizeof(struct cam_sync_info))
		return -EINVAL;

	if (!k_ioctl->ioctl_ptr)
		return -EINVAL;

	if (copy_from_user(&sync_create,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	return cam_sync_destroy(sync_create.sync_obj);
}

static int cam_sync_handle_register_user_payload(
	struct cam_private_ioctl_arg *k_ioctl)
{
	struct cam_sync_userpayload_info userpayload_info;
	struct sync_user_payload *user_payload_kernel;
	struct sync_user_payload *user_payload_iter;
	struct sync_user_payload *temp_upayload_kernel;
	uint32_t sync_obj;
	struct sync_table_row *row = NULL;
	uint16_t sync_uid;
	uint32_t rc, uid_validity;

	if (k_ioctl->size != sizeof(struct cam_sync_userpayload_info))
		return -EINVAL;

	if (!k_ioctl->ioctl_ptr)
		return -EINVAL;

	if (copy_from_user(&userpayload_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	sync_obj = (uint32_t)userpayload_info.sync_obj & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)userpayload_info.sync_obj >> sync_uid_access.uidShift;
	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0)
		return -EINVAL;

	user_payload_kernel = kzalloc(sizeof(*user_payload_kernel), GFP_KERNEL);
	if (!user_payload_kernel)
		return -ENOMEM;

	memcpy(user_payload_kernel->payload_data,
		userpayload_info.payload,
		CAM_SYNC_PAYLOAD_WORDS * sizeof(__u64));

	spin_lock_bh(&sync_dev->row_spinlocks[sync_obj]);
	row =  sync_dev->sync_table + sync_obj;

	uid_validity = cam_sync_check_uid_valid(userpayload_info.sync_obj);

	if (uid_validity == SYNC_UID_NEW) {
		rc = cam_sync_reinit_object(sync_dev->sync_table, userpayload_info.sync_obj);
	} else if (uid_validity == SYNC_UID_OLD) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC, "Called for invalid fence, sync obj: %d, uid: %d",
			sync_obj,
			sync_uid);
		return -EINVAL;
	}

	if (row->state == CAM_SYNC_STATE_INVALID) {
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj = %s[%d]",
			row->name,
			sync_obj);
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		kfree(user_payload_kernel);
		return -EINVAL;
	}

	if ((row->state == CAM_SYNC_STATE_SIGNALED_SUCCESS) ||
		(row->state == CAM_SYNC_STATE_SIGNALED_ERROR) ||
		(row->state == CAM_SYNC_STATE_SIGNALED_CANCEL)) {

		cam_sync_util_send_v4l2_event(CAM_SYNC_V4L_EVENT_ID_CB_TRIG,
			sync_obj,
			row->state,
			user_payload_kernel->payload_data,
			CAM_SYNC_USER_PAYLOAD_SIZE * sizeof(__u64),
			CAM_SYNC_COMMON_REG_PAYLOAD_EVENT, NULL);

		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		kfree(user_payload_kernel);
		return 0;
	}

	list_for_each_entry_safe(user_payload_iter,
		temp_upayload_kernel,
		&row->user_payload_list,
		list) {
		if (user_payload_iter->payload_data[0] ==
				user_payload_kernel->payload_data[0] &&
			user_payload_iter->payload_data[1] ==
				user_payload_kernel->payload_data[1]) {

			spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
			kfree(user_payload_kernel);
			return -EALREADY;
		}
	}

	list_add_tail(&user_payload_kernel->list, &row->user_payload_list);
	spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
	return 0;
}

static int cam_sync_handle_deregister_user_payload(
	struct cam_private_ioctl_arg *k_ioctl)
{
	struct cam_sync_userpayload_info userpayload_info;
	struct sync_user_payload *user_payload_kernel, *temp;
	uint32_t sync_obj;
	struct sync_table_row *row = NULL;
	uint16_t sync_uid;

	if (k_ioctl->size != sizeof(struct cam_sync_userpayload_info)) {
		CAM_ERR(CAM_SYNC, "Incorrect ioctl size");
		return -EINVAL;
	}

	if (!k_ioctl->ioctl_ptr) {
		CAM_ERR(CAM_SYNC, "Invalid embedded ioctl ptr");
		return -EINVAL;
	}

	if (copy_from_user(&userpayload_info,
		u64_to_user_ptr(k_ioctl->ioctl_ptr),
		k_ioctl->size))
		return -EFAULT;

	sync_obj = (uint32_t)userpayload_info.sync_obj & sync_uid_access.fenceIdMask;
	sync_uid = (uint32_t)userpayload_info.sync_obj >> sync_uid_access.uidShift;
	if (sync_obj >= CAM_SYNC_MAX_OBJS || sync_obj <= 0)
		return -EINVAL;

	spin_lock_bh(&sync_dev->row_spinlocks[sync_obj]);
	row = sync_dev->sync_table + sync_obj;

	if (row->uid != sync_uid) {
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		CAM_ERR(CAM_SYNC, "deregister called for invalid fence, sync obj: %d, uid: %d",
			sync_obj,
			sync_uid);
		return -EINVAL;
	}

	if (row->state == CAM_SYNC_STATE_INVALID) {
		CAM_ERR(CAM_SYNC,
			"Error: accessing an uninitialized sync obj = %s[%d]",
			row->name,
			sync_obj);
		spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
		return -EINVAL;
	}

	list_for_each_entry_safe(user_payload_kernel, temp,
				&row->user_payload_list, list) {
		if (user_payload_kernel->payload_data[0] ==
				userpayload_info.payload[0] &&
				user_payload_kernel->payload_data[1] ==
				userpayload_info.payload[1]) {
			list_del_init(&user_payload_kernel->list);
			kfree(user_payload_kernel);
		}
	}

	spin_unlock_bh(&sync_dev->row_spinlocks[sync_obj]);
	return 0;
}

static long cam_sync_dev_ioctl(struct file *filep, void *fh,
		bool valid_prio, unsigned int cmd, void *arg)
{
	int32_t rc;
	struct sync_device *sync_dev = video_drvdata(filep);
	struct cam_private_ioctl_arg k_ioctl;

	if (!sync_dev) {
		CAM_ERR(CAM_SYNC, "sync_dev NULL");
		return -EINVAL;
	}

	if (!arg)
		return -EINVAL;

	if (cmd != CAM_PRIVATE_IOCTL_CMD)
		return -ENOIOCTLCMD;

	k_ioctl = *(struct cam_private_ioctl_arg *)arg;

	switch (k_ioctl.id) {
	case CAM_SYNC_CREATE:
		rc = cam_sync_handle_create(&k_ioctl);
		break;
	case CAM_SYNC_DESTROY:
		rc = cam_sync_handle_destroy(&k_ioctl);
		break;
	case CAM_SYNC_REGISTER_PAYLOAD:
		rc = cam_sync_handle_register_user_payload(
			&k_ioctl);
		break;
	case CAM_SYNC_DEREGISTER_PAYLOAD:
		rc = cam_sync_handle_deregister_user_payload(
			&k_ioctl);
		break;
	case CAM_SYNC_SIGNAL:
		rc = cam_sync_handle_signal(&k_ioctl);
		break;
	case CAM_SYNC_MERGE:
		rc = cam_sync_handle_merge(&k_ioctl);
		break;
	case CAM_SYNC_WAIT:
		rc = cam_sync_handle_wait(&k_ioctl);
		((struct cam_private_ioctl_arg *)arg)->result =
			k_ioctl.result;
		break;
	case CAM_SYNC_EXIT_DQ_THREAD:
		rc = cam_sync_handle_exit_poll();
		break;
	default:
		rc = -ENOIOCTLCMD;
	}

	return rc;
}

static unsigned int cam_sync_poll(struct file *f,
	struct poll_table_struct *pll_table)
{
	int rc = 0;
	struct v4l2_fh *eventq = f->private_data;

	if (!eventq)
		return -EINVAL;

	poll_wait(f, &eventq->wait, pll_table);

	if (v4l2_event_pending(eventq))
		rc = POLLPRI;

	return rc;
}

static int cam_sync_open(struct file *filep)
{
	int rc;
	struct sync_device *sync_dev = video_drvdata(filep);

	if (!sync_dev) {
		CAM_ERR(CAM_SYNC, "Sync device NULL");
		return -ENODEV;
	}

	mutex_lock(&sync_dev->table_lock);
	if (sync_dev->open_cnt >= 1) {
		mutex_unlock(&sync_dev->table_lock);
		return -EALREADY;
	}

	rc = v4l2_fh_open(filep);
	if (!rc) {
		sync_dev->open_cnt++;
		spin_lock_bh(&sync_dev->cam_sync_eventq_lock);
		sync_dev->cam_sync_eventq = filep->private_data;
		spin_unlock_bh(&sync_dev->cam_sync_eventq_lock);
	} else {
		CAM_ERR(CAM_SYNC, "v4l2_fh_open failed : %d", rc);
	}
	mutex_unlock(&sync_dev->table_lock);

	return rc;
}

static int cam_sync_close(struct file *filep)
{
	int rc = 0;
	int i;
	struct sync_device *sync_dev = video_drvdata(filep);
	struct cam_sync_signal_param param;

	if (!sync_dev) {
		CAM_ERR(CAM_SYNC, "Sync device NULL");
		rc = -ENODEV;
		return rc;
	}
	mutex_lock(&sync_dev->table_lock);
	sync_dev->open_cnt--;
	if (!sync_dev->open_cnt) {
		for (i = 1; i < CAM_SYNC_MAX_OBJS; i++) {
			struct sync_table_row *row =
			sync_dev->sync_table + i;

			/*
			 * Signal all ACTIVE objects as ERR, but we don't
			 * care about the return status here apart from logging
			 * it.
			 */
			if (row->state == CAM_SYNC_STATE_ACTIVE) {
				memset(&param, 0, sizeof(param));
				param.sync_obj = i;
				param.status = CAM_SYNC_STATE_SIGNALED_ERROR;
				param.event_cause = CAM_SYNC_COMMON_RELEASE_EVENT;
				rc = cam_sync_signal(&param, NULL);
				if (rc < 0)
					CAM_ERR(CAM_SYNC,
					  "Cleanup signal fail idx:%d\n",
					  i);
			}
		}

		/*
		 * Flush the work queue to wait for pending signal callbacks to
		 * finish
		 */
		cam_req_mgr_worker_flush(sync_dev->worker);

		/*
		 * Now that all callbacks worker threads have finished,
		 * destroy the sync objects
		 */
		for (i = 1; i < CAM_SYNC_MAX_OBJS; i++) {
			struct sync_table_row *row =
			sync_dev->sync_table + i;

			if (row->state != CAM_SYNC_STATE_INVALID) {
				rc = cam_sync_destroy(i);
				if (rc < 0)
					CAM_ERR(CAM_SYNC,
					  "Cleanup destroy fail:idx:%d\n",
					  i);
			}
		}
	}
	mutex_unlock(&sync_dev->table_lock);
	spin_lock_bh(&sync_dev->cam_sync_eventq_lock);
	sync_dev->cam_sync_eventq = NULL;
	spin_unlock_bh(&sync_dev->cam_sync_eventq_lock);
	v4l2_fh_release(filep);

	return rc;
}

static void cam_sync_event_queue_notify_error(const struct v4l2_event *old,
	struct v4l2_event *new)
{
	if (sync_dev->version == CAM_SYNC_V4L_EVENT_V2) {
		struct cam_sync_ev_header_v2 *ev_header;

		ev_header = CAM_SYNC_GET_HEADER_PTR_V2((*old));
		CAM_ERR(CAM_CRM,
			"Failed to notify event id %d fence %d statue %d reason %u %u %u %u",
			old->id, ev_header->sync_obj, ev_header->status,
			ev_header->evt_param[0], ev_header->evt_param[1],
			ev_header->evt_param[2], ev_header->evt_param[3]);

	} else if (sync_dev->version == CAM_SYNC_V4L_EVENT_V3) {
		struct cam_sync_ev_header_v3 *ev_header;

		ev_header = CAM_SYNC_GET_HEADER_PTR_V3((*old));
		CAM_ERR(CAM_CRM,
			"Fail to notify event id %d fence %d status %d reason %u",
			old->id, ev_header->sync_obj, ev_header->status,
			ev_header->evt_param[0]);
	} else {
		struct cam_sync_ev_header *ev_header;

		ev_header = CAM_SYNC_GET_HEADER_PTR((*old));
		CAM_ERR(CAM_CRM,
			"Failed to notify event id %d fence %d statue %d",
			old->id, ev_header->sync_obj, ev_header->status);
	}
}

static struct v4l2_subscribed_event_ops cam_sync_v4l2_ops = {
	.merge = cam_sync_event_queue_notify_error,
};

int cam_sync_subscribe_event(struct v4l2_fh *fh,
		const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case CAM_SYNC_V4L_EVENT:
	case CAM_SYNC_V4L_EVENT_V2:
	case CAM_SYNC_V4L_EVENT_V3:
		break;
	default:
		CAM_ERR(CAM_SYNC, "Non supported event type 0x%x", sub->type);
		return -EINVAL;
	}

	switch (sub->id) {
	case CAM_SYNC_V4L_EVENT_ID_CB_TRIG:
	case CAM_SYNC_V4L_EVENT_ID_EXIT:
		break;
	default:
		CAM_ERR(CAM_SYNC, "Non supported event id 0x%x", sub->id);
		return -EINVAL;
	}

	sync_dev->version = sub->type;
	CAM_DBG(CAM_SYNC, "Sync event verion type 0x%x", sync_dev->version);
	return v4l2_event_subscribe(fh, sub, CAM_SYNC_MAX_V4L2_EVENTS,
		&cam_sync_v4l2_ops);
}

int cam_sync_unsubscribe_event(struct v4l2_fh *fh,
		const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case CAM_SYNC_V4L_EVENT:
	case CAM_SYNC_V4L_EVENT_V2:
	case CAM_SYNC_V4L_EVENT_V3:
		break;
	default:
		CAM_ERR(CAM_SYNC, "Non supported event type 0x%x", sub->type);
		return -EINVAL;
	}

	return v4l2_event_unsubscribe(fh, sub);
}

static const struct v4l2_ioctl_ops g_cam_sync_ioctl_ops = {
	.vidioc_subscribe_event = cam_sync_subscribe_event,
	.vidioc_unsubscribe_event = cam_sync_unsubscribe_event,
	.vidioc_default = cam_sync_dev_ioctl,
};

static struct v4l2_file_operations cam_sync_v4l2_fops = {
	.owner = THIS_MODULE,
	.open  = cam_sync_open,
	.release = cam_sync_close,
	.poll = cam_sync_poll,
	.unlocked_ioctl   = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

#if IS_REACHABLE(CONFIG_MEDIA_CONTROLLER)
static int cam_sync_media_controller_init(struct sync_device *sync_dev,
	struct platform_device *pdev)
{
	int rc;

	sync_dev->v4l2_dev.mdev = kzalloc(sizeof(struct media_device),
		GFP_KERNEL);
	if (!sync_dev->v4l2_dev.mdev)
		return -ENOMEM;

	media_device_init(sync_dev->v4l2_dev.mdev);
	strlcpy(sync_dev->v4l2_dev.mdev->model, CAM_SYNC_DEVICE_NAME,
			sizeof(sync_dev->v4l2_dev.mdev->model));
	sync_dev->v4l2_dev.mdev->dev = &(pdev->dev);

	rc = media_device_register(sync_dev->v4l2_dev.mdev);
	if (rc < 0)
		goto register_fail;

	rc = media_entity_pads_init(&sync_dev->vdev->entity, 0, NULL);
	if (rc < 0)
		goto entity_fail;

	return 0;

entity_fail:
	media_device_unregister(sync_dev->v4l2_dev.mdev);
register_fail:
	media_device_cleanup(sync_dev->v4l2_dev.mdev);
	return rc;
}

static void cam_sync_media_controller_cleanup(struct sync_device *sync_dev)
{
	media_entity_cleanup(&sync_dev->vdev->entity);
	media_device_unregister(sync_dev->v4l2_dev.mdev);
	media_device_cleanup(sync_dev->v4l2_dev.mdev);
	kfree(sync_dev->v4l2_dev.mdev);
}

static void cam_sync_init_entity(struct sync_device *sync_dev)
{
	sync_dev->vdev->entity.function = CAM_SYNC_DEVICE_TYPE;
	sync_dev->vdev->entity.name =
				video_device_node_name(sync_dev->vdev);
}
#else
static int cam_sync_media_controller_init(struct sync_device *sync_dev,
	struct platform_device *pdev)
{
	return 0;
}

static void cam_sync_media_controller_cleanup(struct sync_device *sync_dev)
{
}

static void cam_sync_init_entity(struct sync_device *sync_dev)
{
}
#endif

static int cam_sync_create_debugfs(void)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;

	dbgfileptr = debugfs_create_dir("camera_sync", NULL);
	if (!dbgfileptr) {
		CAM_ERR(CAM_SYNC,"DebugFS could not create directory!");
		rc = -ENOENT;
		goto end;
	}
	/* Store parent inode for cleanup in caller */
	sync_dev->dentry = dbgfileptr;

	debugfs_create_bool("trigger_cb_without_switch", 0644,
		sync_dev->dentry, &trigger_cb_without_switch);

end:
	return rc;
}

#if IS_REACHABLE(CONFIG_MSM_GLOBAL_SYNX)
int cam_synx_sync_signal(int32_t sync_obj, uint32_t synx_status)
{
	int rc = 0;
	uint32_t sync_status = synx_status;
	struct cam_sync_signal_param param;

	switch (synx_status) {
	case SYNX_STATE_ACTIVE:
		sync_status = CAM_SYNC_STATE_ACTIVE;
		break;
	case SYNX_STATE_SIGNALED_SUCCESS:
		sync_status = CAM_SYNC_STATE_SIGNALED_SUCCESS;
		break;
	case SYNX_STATE_SIGNALED_ERROR:
		sync_status = CAM_SYNC_STATE_SIGNALED_ERROR;
		break;
	case 4: /* SYNX_STATE_SIGNALED_CANCEL: */
		sync_status = CAM_SYNC_STATE_SIGNALED_CANCEL;
		break;
	default:
		CAM_ERR(CAM_SYNC, "Invalid synx status %d for obj %d",
			synx_status, sync_obj);
		sync_status = CAM_SYNC_STATE_SIGNALED_ERROR;
		break;
	}
	memset(&param, 0, sizeof(param));
	param.sync_obj = sync_obj;
	param.status = sync_status;
	param.event_cause = CAM_SYNC_COMMON_EVENT_SYNX;
	rc = cam_sync_signal(&param, NULL);
	if (rc) {
		CAM_ERR(CAM_SYNC,
			"synx signal failed with %d, sync_obj=%d, synx_status=%d, sync_status=%d",
			sync_obj, synx_status, sync_status, rc);
	}

	return rc;
}

static int cam_sync_register_synx_bind_ops(
	struct synx_register_params *object)
{
	int rc = 0;

	rc = synx_register_ops(object);
	if (rc)
		CAM_ERR(CAM_SYNC, "synx registration fail with rc=%d", rc);

	return rc;
}

static void cam_sync_unregister_synx_bind_ops(
	struct synx_register_params *object)
{
	int rc = 0;

	rc = synx_deregister_ops(object);
	if (rc)
		CAM_ERR(CAM_SYNC, "sync unregistration fail with %d", rc);
}

static void cam_sync_configure_synx_obj(struct synx_register_params *object)
{
	struct synx_register_params *params = object;

	params->name = CAM_SYNC_NAME;
	params->type = SYNX_TYPE_CSL;
	params->ops.register_callback = cam_sync_register_callback;
	params->ops.deregister_callback = cam_sync_deregister_callback;
	params->ops.enable_signaling = cam_sync_get_obj_ref;
	params->ops.signal = cam_synx_sync_signal;
}
#endif

static int cam_sync_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc;
	int idx;
	struct platform_device *pdev = to_platform_device(dev);
	uint32_t num_uid_bits;
	const unsigned long max_uid = CAM_SYNC_MAX_UID;

	sync_dev = kzalloc(sizeof(*sync_dev), GFP_KERNEL);
	if (!sync_dev)
		return -ENOMEM;

	mutex_init(&sync_dev->table_lock);
	spin_lock_init(&sync_dev->cam_sync_eventq_lock);

	for (idx = 0; idx < CAM_SYNC_MAX_OBJS; idx++)
		spin_lock_init(&sync_dev->row_spinlocks[idx]);

	sync_dev->vdev = video_device_alloc();
	if (!sync_dev->vdev) {
		rc = -ENOMEM;
		goto vdev_fail;
	}

	rc = cam_sync_media_controller_init(sync_dev, pdev);
	if (rc < 0)
		goto mcinit_fail;

	sync_dev->vdev->v4l2_dev = &sync_dev->v4l2_dev;

	rc = v4l2_device_register(&(pdev->dev), sync_dev->vdev->v4l2_dev);
	if (rc < 0)
		goto register_fail;

	strlcpy(sync_dev->vdev->name, CAM_SYNC_NAME,
				sizeof(sync_dev->vdev->name));
	sync_dev->vdev->release  = video_device_release_empty;
	sync_dev->vdev->fops     = &cam_sync_v4l2_fops;
	sync_dev->vdev->ioctl_ops = &g_cam_sync_ioctl_ops;
	sync_dev->vdev->minor     = -1;
	sync_dev->vdev->device_caps |= V4L2_CAP_VIDEO_CAPTURE;
	sync_dev->vdev->vfl_type  = VFL_TYPE_VIDEO;
	rc = video_register_device(sync_dev->vdev, VFL_TYPE_VIDEO, -1);
	if (rc < 0) {
		CAM_ERR(CAM_SYNC,
			"video device registration failure rc = %d, name = %s, device_caps = %d",
			rc, sync_dev->vdev->name, sync_dev->vdev->device_caps);
		goto v4l2_fail;
	}

	cam_sync_init_entity(sync_dev);
	video_set_drvdata(sync_dev->vdev, sync_dev);
	memset(&sync_dev->sync_table, 0, sizeof(sync_dev->sync_table));
	memset(&sync_dev->bitmap, 0, sizeof(sync_dev->bitmap));
	bitmap_zero(sync_dev->bitmap, CAM_SYNC_MAX_OBJS);

	/*
	 * We treat zero as invalid handle, so we will keep the 0th bit set
	 * always
	 */
	set_bit(0, sync_dev->bitmap);

	cam_req_mgr_worker_create("sync_worker", 30,
		&sync_dev->worker, CRM_WORKER_USAGE_IRQ, WQ_HIGHPRI | WQ_UNBOUND);

	if (!sync_dev->worker) {
		CAM_ERR(CAM_SYNC,
			"Error: high priority work queue creation failed");
		rc = -ENOMEM;
		goto v4l2_fail;
	}

	trigger_cb_without_switch = false;
	cam_sync_create_debugfs();
	num_uid_bits = bitmap_weight(&max_uid, sizeof(sync_dev->sync_table->sync_id));
	sync_uid_access.fenceIdMask =
		(1 << ((sizeof(sync_dev->sync_table->sync_id) * 8) - num_uid_bits)) - 1;
	sync_uid_access.uidShift = (sizeof(sync_dev->sync_table->sync_id) * 8) - num_uid_bits;
	sync_uid_access.init_uid_val = max_uid << sync_uid_access.uidShift;
#if IS_REACHABLE(CONFIG_MSM_GLOBAL_SYNX)
	CAM_DBG(CAM_SYNC, "Registering with synx driver");
	cam_sync_configure_synx_obj(&sync_dev->params);
	rc = cam_sync_register_synx_bind_ops(&sync_dev->params);
	if (rc)
		goto v4l2_fail;
#endif
	CAM_DBG(CAM_SYNC, "Component bound successfully");
	return rc;

v4l2_fail:
	v4l2_device_unregister(sync_dev->vdev->v4l2_dev);
register_fail:
	cam_sync_media_controller_cleanup(sync_dev);
mcinit_fail:
	video_unregister_device(sync_dev->vdev);
	video_device_release(sync_dev->vdev);
vdev_fail:
	mutex_destroy(&sync_dev->table_lock);
	kfree(sync_dev);
	return rc;
}

static void cam_sync_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int i;

	v4l2_device_unregister(sync_dev->vdev->v4l2_dev);
	cam_sync_media_controller_cleanup(sync_dev);
#if IS_REACHABLE(CONFIG_MSM_GLOBAL_SYNX)
	cam_sync_unregister_synx_bind_ops(&sync_dev->params);
#endif
	video_unregister_device(sync_dev->vdev);
	video_device_release(sync_dev->vdev);
	debugfs_remove_recursive(sync_dev->dentry);
	sync_dev->dentry = NULL;

	for (i = 0; i < CAM_SYNC_MAX_OBJS; i++)
		spin_lock_init(&sync_dev->row_spinlocks[i]);

	kfree(sync_dev);
	sync_dev = NULL;
}

const static struct component_ops cam_sync_component_ops = {
	.bind = cam_sync_component_bind,
	.unbind = cam_sync_component_unbind,
};

static int cam_sync_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_SYNC, "Adding Sync component");
	rc = component_add(&pdev->dev, &cam_sync_component_ops);
	if (rc)
		CAM_ERR(CAM_SYNC, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_sync_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_sync_component_ops);
	return 0;
}

static const struct of_device_id cam_sync_dt_match[] = {
	{.compatible = "qcom,cam-sync"},
	{}
};

MODULE_DEVICE_TABLE(of, cam_sync_dt_match);

struct platform_driver cam_sync_driver = {
	.probe = cam_sync_probe,
	.remove = cam_sync_remove,
	.driver = {
		.name = "cam_sync",
		.owner = THIS_MODULE,
		.of_match_table = cam_sync_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_sync_init(void)
{
	return platform_driver_register(&cam_sync_driver);
}

void cam_sync_exit(void)
{
	platform_driver_unregister(&cam_sync_driver);
}

MODULE_DESCRIPTION("Camera sync driver");
MODULE_LICENSE("GPL v2");
