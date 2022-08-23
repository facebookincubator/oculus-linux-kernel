// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "CAM-REQ-MGR_UTIL %s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <media/cam_req_mgr.h>
#include "cam_req_mgr_util.h"
#include "cam_debug_util.h"
#include "cam_subdev.h"

static struct cam_req_mgr_util_hdl_tbl *hdl_tbl;
static DEFINE_SPINLOCK(hdl_tbl_lock);

int cam_req_mgr_util_init(void)
{
	int rc = 0;
	int bitmap_size;
	static struct cam_req_mgr_util_hdl_tbl *hdl_tbl_local;

	if (hdl_tbl) {
		rc = -EINVAL;
		CAM_ERR(CAM_CRM, "Hdl_tbl is already present");
		goto hdl_tbl_check_failed;
	}

	hdl_tbl_local = kzalloc(sizeof(*hdl_tbl), GFP_KERNEL);
	if (!hdl_tbl_local) {
		rc = -ENOMEM;
		goto hdl_tbl_alloc_failed;
	}
	spin_lock_bh(&hdl_tbl_lock);
	if (hdl_tbl) {
		spin_unlock_bh(&hdl_tbl_lock);
		rc = -EEXIST;
		kfree(hdl_tbl_local);
		goto hdl_tbl_check_failed;
	}
	hdl_tbl = hdl_tbl_local;
	spin_unlock_bh(&hdl_tbl_lock);

	bitmap_size = BITS_TO_LONGS(CAM_REQ_MGR_MAX_HANDLES_V2) * sizeof(long);
	hdl_tbl->bitmap_dev = kzalloc(bitmap_size, GFP_KERNEL);
	if (!hdl_tbl->bitmap_dev) {
		rc = -ENOMEM;
		goto bitmap_dev_alloc_fail;
	}

	bitmap_size = BITS_TO_LONGS(CAM_REQ_MGR_MAX_SESSION_HANDLES) *
		sizeof(long);
	hdl_tbl->bitmap_session = kzalloc(bitmap_size, GFP_KERNEL);
	if (!hdl_tbl->bitmap_session) {
		rc = -ENOMEM;
		goto bitmap_session_alloc_fail;
	}

	return rc;

bitmap_session_alloc_fail:
	kfree(hdl_tbl->bitmap_dev);
	hdl_tbl->bitmap_dev = NULL;
bitmap_dev_alloc_fail:
	kfree(hdl_tbl);
	hdl_tbl = NULL;
hdl_tbl_alloc_failed:
hdl_tbl_check_failed:
	return rc;
}

int cam_req_mgr_util_deinit(void)
{
	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	kfree(hdl_tbl->bitmap_dev);
	hdl_tbl->bitmap_dev = NULL;
	kfree(hdl_tbl->bitmap_session);
	hdl_tbl->bitmap_session = NULL;
	kfree(hdl_tbl);
	hdl_tbl = NULL;
	spin_unlock_bh(&hdl_tbl_lock);

	return 0;
}

int cam_req_mgr_util_free_hdls(void)
{
	int i = 0;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	for (i = 0; i < CAM_REQ_MGR_MAX_HANDLES_V2; i++) {
		if (hdl_tbl->hdl_dev[i].state == HDL_ACTIVE) {
			CAM_WARN(CAM_CRM, "Dev handle = %x session_handle = %x",
				hdl_tbl->hdl_dev[i].hdl_value,
				hdl_tbl->hdl_dev[i].session_hdl);
			hdl_tbl->hdl_dev[i].state = HDL_FREE;
			clear_bit(i, hdl_tbl->bitmap_dev);
		}
	}
	for (i = 0; i < CAM_REQ_MGR_MAX_SESSION_HANDLES; i++) {
		if (hdl_tbl->hdl_session[i].state == HDL_ACTIVE) {
			CAM_WARN(CAM_CRM, "Dev handle = %x session_handle = %x",
				hdl_tbl->hdl_session[i].hdl_value,
				hdl_tbl->hdl_session[i].session_hdl);
			hdl_tbl->hdl_session[i].state = HDL_FREE;
			clear_bit(i, hdl_tbl->bitmap_session);
		}
	}
	bitmap_zero(hdl_tbl->bitmap_dev, CAM_REQ_MGR_MAX_HANDLES_V2);
	bitmap_zero(hdl_tbl->bitmap_session, CAM_REQ_MGR_MAX_SESSION_HANDLES);
	spin_unlock_bh(&hdl_tbl_lock);

	return 0;
}

static int32_t cam_get_free_handle_index(enum hdl_type type)
{
	int idx;
	int32_t bits_size;
	void *hdl_bitmap;

	if (type == HDL_TYPE_DEV) {
		idx = find_first_zero_bit(hdl_tbl->bitmap_dev,
			CAM_REQ_MGR_MAX_HANDLES_V2);
		bits_size = CAM_REQ_MGR_MAX_HANDLES_V2;
		hdl_bitmap = hdl_tbl->bitmap_dev;
	} else {
		idx = find_first_zero_bit(hdl_tbl->bitmap_session,
			CAM_REQ_MGR_MAX_SESSION_HANDLES);
		bits_size = CAM_REQ_MGR_MAX_SESSION_HANDLES;
		hdl_bitmap = hdl_tbl->bitmap_session;
	}

	if (idx >= bits_size || idx < 0)
		return -ENOSR;

	set_bit(idx, hdl_bitmap);

	return idx;
}

int32_t cam_create_session_hdl(void *priv)
{
	int idx;
	int rand = 0;
	int32_t handle = 0;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	idx = cam_get_free_handle_index(HDL_TYPE_SESSION);
	if (idx < 0) {
		CAM_ERR(CAM_CRM, "Unable to create session handle");
		spin_unlock_bh(&hdl_tbl_lock);
		return idx;
	}

	get_random_bytes(&rand, CAM_REQ_MGR_RND1_BYTES);
	handle = GET_DEV_HANDLE(rand, HDL_TYPE_SESSION, idx);
	hdl_tbl->hdl_session[idx].session_hdl = handle;
	hdl_tbl->hdl_session[idx].hdl_value = handle;
	hdl_tbl->hdl_session[idx].type = HDL_TYPE_SESSION;
	hdl_tbl->hdl_session[idx].state = HDL_ACTIVE;
	hdl_tbl->hdl_session[idx].priv = priv;
	hdl_tbl->hdl_session[idx].ops = NULL;
	spin_unlock_bh(&hdl_tbl_lock);

	return handle;
}

int32_t cam_create_device_hdl(struct cam_create_dev_hdl *hdl_data)
{
	int idx;
	int rand = 0;
	int32_t handle;
	bool crm_active;

	crm_active = cam_req_mgr_is_open();
	if (!crm_active) {
		CAM_ERR(CAM_ICP, "CRM is not ACTIVE");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		spin_unlock_bh(&hdl_tbl_lock);
		return -EINVAL;
	}

	idx = cam_get_free_handle_index(HDL_TYPE_DEV);
	if (idx < 0) {
		CAM_ERR(CAM_CRM, "Unable to create device handle");
		spin_unlock_bh(&hdl_tbl_lock);
		return idx;
	}

	get_random_bytes(&rand, CAM_REQ_MGR_RND1_BYTES);
	handle = GET_DEV_HANDLE(rand, HDL_TYPE_DEV, idx);
	hdl_tbl->hdl_dev[idx].session_hdl = hdl_data->session_hdl;
	hdl_tbl->hdl_dev[idx].hdl_value = handle;
	hdl_tbl->hdl_dev[idx].type = HDL_TYPE_DEV;
	hdl_tbl->hdl_dev[idx].state = HDL_ACTIVE;
	hdl_tbl->hdl_dev[idx].priv = hdl_data->priv;
	hdl_tbl->hdl_dev[idx].ops = hdl_data->ops;
	spin_unlock_bh(&hdl_tbl_lock);

	pr_debug("%s: handle = %x", __func__, handle);
	return handle;
}

void *cam_get_device_priv(int32_t dev_hdl)
{
	int idx;
	int type;
	void *priv;
	int32_t bits_size;
	struct handle *hdl_addr;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Hdl tbl is NULL");
		goto device_priv_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);
	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (HDL_TYPE_DEV != type && HDL_TYPE_SESSION != type) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid type");
		goto device_priv_fail;
	}

	if (type == HDL_TYPE_DEV) {
		bits_size = CAM_REQ_MGR_MAX_HANDLES_V2;
		hdl_addr = hdl_tbl->hdl_dev;
	} else {
		bits_size = CAM_REQ_MGR_MAX_SESSION_HANDLES;
		hdl_addr = hdl_tbl->hdl_session;
	}

	if (idx >= bits_size) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid idx");
		goto device_priv_fail;
	}

	if (hdl_addr[idx].state != HDL_ACTIVE) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid state");
		goto device_priv_fail;
	}

	if (hdl_addr[idx].hdl_value != dev_hdl) {
		CAM_ERR_RATE_LIMIT(CAM_CRM, "Invalid hdl");
		goto device_priv_fail;
	}

	priv = hdl_addr[idx].priv;
	spin_unlock_bh(&hdl_tbl_lock);

	return priv;

device_priv_fail:
	spin_unlock_bh(&hdl_tbl_lock);
	return NULL;
}

void *cam_get_device_ops(int32_t dev_hdl)
{
	int idx;
	int type;
	void *ops;
	int32_t bits_size;
	struct handle *hdl_addr;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		goto device_ops_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (HDL_TYPE_DEV != type && HDL_TYPE_SESSION != type) {
		CAM_ERR(CAM_CRM, "Invalid type");
		goto device_ops_fail;
	}

	if (type == HDL_TYPE_DEV) {
		bits_size = CAM_REQ_MGR_MAX_HANDLES_V2;
		hdl_addr = hdl_tbl->hdl_dev;
	} else {
		bits_size = CAM_REQ_MGR_MAX_SESSION_HANDLES;
		hdl_addr = hdl_tbl->hdl_session;
	}

	if (idx >= bits_size) {
		CAM_ERR(CAM_CRM, "Invalid idx");
		goto device_ops_fail;
	}

	if (hdl_addr[idx].state != HDL_ACTIVE) {
		CAM_ERR(CAM_CRM, "Invalid state");
		goto device_ops_fail;
	}

	if (hdl_addr[idx].hdl_value != dev_hdl) {
		CAM_ERR(CAM_CRM, "Invalid hdl");
		goto device_ops_fail;
	}

	ops = hdl_addr[idx].ops;
	spin_unlock_bh(&hdl_tbl_lock);

	return ops;

device_ops_fail:
	spin_unlock_bh(&hdl_tbl_lock);
	return NULL;
}

static int cam_destroy_hdl(int32_t dev_hdl, int dev_hdl_type)
{
	int idx;
	int type;
	int32_t bits_size;
	struct handle *hdl_addr;
	void *hdl_bitmap;

	spin_lock_bh(&hdl_tbl_lock);
	if (!hdl_tbl) {
		CAM_ERR(CAM_CRM, "Hdl tbl is NULL");
		goto destroy_hdl_fail;
	}

	idx = CAM_REQ_MGR_GET_HDL_IDX(dev_hdl);

	type = CAM_REQ_MGR_GET_HDL_TYPE(dev_hdl);
	if (type != dev_hdl_type) {
		CAM_ERR(CAM_CRM, "Invalid type %d, %d", type, dev_hdl_type);
		goto destroy_hdl_fail;
	}

	if (type == HDL_TYPE_DEV) {
		bits_size = CAM_REQ_MGR_MAX_HANDLES_V2;
		hdl_addr = hdl_tbl->hdl_dev;
		hdl_bitmap = hdl_tbl->bitmap_dev;
	} else {
		bits_size = CAM_REQ_MGR_MAX_SESSION_HANDLES;
		hdl_addr = hdl_tbl->hdl_session;
		hdl_bitmap = hdl_tbl->bitmap_session;
	}

	if (idx >= bits_size) {
		CAM_ERR(CAM_CRM, "Invalid idx %d", idx);
		goto destroy_hdl_fail;
	}

	if (hdl_addr[idx].state != HDL_ACTIVE) {
		CAM_ERR(CAM_CRM, "Invalid state");
		goto destroy_hdl_fail;
	}

	if (hdl_addr[idx].hdl_value != dev_hdl) {
		CAM_ERR(CAM_CRM, "Invalid hdl");
		goto destroy_hdl_fail;
	}

	hdl_addr[idx].state = HDL_FREE;
	hdl_addr[idx].ops   = NULL;
	hdl_addr[idx].priv  = NULL;
	clear_bit(idx, hdl_bitmap);
	spin_unlock_bh(&hdl_tbl_lock);

	return 0;

destroy_hdl_fail:
	spin_unlock_bh(&hdl_tbl_lock);
	return -EINVAL;
}

int cam_destroy_device_hdl(int32_t dev_hdl)
{
	return cam_destroy_hdl(dev_hdl, HDL_TYPE_DEV);
}

int cam_destroy_session_hdl(int32_t dev_hdl)
{
	return cam_destroy_hdl(dev_hdl, HDL_TYPE_SESSION);
}
