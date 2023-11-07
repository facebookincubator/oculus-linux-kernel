// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <dt-bindings/msm-camera.h>
#include "cam_compat.h"
#include "cam_sensor_lite_core.h"
#include "cam_sensor_lite_dev.h"
#include "cam_sensor_lite_soc.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "cam_mem_mgr.h"
#include "cam_cpas_api.h"
#include "cam_compat.h"
#include "cam_rpmsg.h"
#include "cam_sensor_lite_pkt_utils.h"

#define WITH_CRM_MASK  0x1
#define HOST_DEST_CAM  0x1
#define SLAVE_DEST_CAM 0x2
#define SENSOR_LITE_DEFAULT_PD 2

/* This should be same as pipeline delay + 1*/
#define MAX_APPLIED_QUEUE_DEPTH 0x2
#define MAX_WAITING_QUEUE_DEPTH 0x4

static int free_request_object(
	struct sensor_lite_device         *sensor_lite_dev,
	struct sensor_lite_request        *req)
{
	int i = 0;

	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid sensor lite device");
		return -EINVAL;
	}

	if (!req) {
		CAM_ERR(CAM_SENSOR_LITE,
			"SENSOR_LITE[%d] cannot free invalid request object",
			sensor_lite_dev->soc_info.index);
		return -EINVAL;
	}

	for (i = 0; i < req->num_cmds; i++)
		kfree(req->payload[i]);

	kfree(req);
	return 0;
}

static int __cam_sensor_lite_handle_perframe(
	struct sensor_lite_device         *sensor_lite_dev,
	struct sensor_lite_perframe_cmd   *cmd,
	uint64_t                          request_id)
{
	int rc = 0;

	/* Send packet regardless of whether CRM is enabled */
	rc = __send_pkt(sensor_lite_dev,
			(struct sensor_lite_header *)cmd);
	return rc;
}

static int cam_sensor_lite_request_queue_cmd(
	struct sensor_lite_device  *dev,
	struct sensor_lite_request *req,
	struct sensor_lite_header  *cmd)
{

	if (!dev || !req || !cmd) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid input ");
		return -EINVAL;
	}

	if (req->num_cmds < MAX_PAYLOAD_CMDS) {
		/*
		 * Alloc and memcpy
		 * needed as we are applying after the call returns
		 **/
		void *payload = kzalloc(cmd->size, GFP_KERNEL);

		if (!payload) {
			CAM_ERR(CAM_SENSOR_LITE, "[%d] alloc failed",
					dev->soc_info.index);
			return -ENOMEM;
		}
		memcpy(payload, cmd, cmd->size);
		req->payload[req->num_cmds++]    = payload;
	} else {
		CAM_ERR(CAM_SENSOR_LITE,
				"[%d] Queue[%d] overflow for request[%llu]",
				dev->soc_info.index,
				req->num_cmds,
				req->request_id);
		return -EINVAL;
	}
	return 0;
}

static struct sensor_lite_request *sensor_lite_create_request_object(
	struct sensor_lite_device         *sensor_lite_dev,
	uint64_t                           request_id,
	uint32_t                           type)
{
	int i = 0;
	struct sensor_lite_request *req = NULL;

	req = kzalloc(sizeof(struct sensor_lite_request), GFP_KERNEL);
	if (!req) {
		CAM_ERR(CAM_SENSOR_LITE,
			"SENSOR_LITE[%d] Cannot create request object for req[%lld]",
			sensor_lite_dev->soc_info.index,
			request_id);
		goto end;
	}
	req->type       = type;
	req->request_id = request_id;
	for (i = 0; i < MAX_PAYLOAD_CMDS; i++)
		req->payload[i]    = NULL;
	req->num_cmds  = 0;

	INIT_LIST_HEAD(&(req->list));
end:
	return req;
}

void cam_sensor_lite_shutdown(
	struct sensor_lite_device *sensor_lite_dev)
{

	struct list_head *pos = NULL, *pos_next = NULL;
	struct sensor_lite_request *entry = NULL;

	CAM_INFO(CAM_SENSOR_LITE, "Shutdown[%d] called",
		sensor_lite_dev->soc_info.index);
	if (sensor_lite_dev->state == CAM_SENSOR_LITE_STATE_INIT) {
		CAM_INFO(CAM_SENSOR_LITE, "Shutdown[%d] called in Init State",
				sensor_lite_dev->soc_info.index);
		return;
	}

	if (sensor_lite_dev->state == CAM_SENSOR_LITE_STATE_START) {
		__send_pkt(sensor_lite_dev,
			&(sensor_lite_dev->stop_cmd->header));
		sensor_lite_dev->state = CAM_SENSOR_LITE_STATE_ACQUIRE;
	}

	if (sensor_lite_dev->state == CAM_SENSOR_LITE_STATE_ACQUIRE) {
		__send_pkt(sensor_lite_dev,
			&(sensor_lite_dev->release_cmd->header));
		sensor_lite_dev->state = CAM_SENSOR_LITE_STATE_INIT;

		cam_destroy_device_hdl(sensor_lite_dev->crm_intf.device_hdl);
		sensor_lite_dev->crm_intf.device_hdl  = -1;
		sensor_lite_dev->crm_intf.session_hdl = -1;
		sensor_lite_dev->crm_intf.enable_crm  = 0;
	}

	/* Free acquire and release */
	kfree(sensor_lite_dev->acquire_cmd);
	sensor_lite_dev->acquire_cmd = NULL;
	kfree(sensor_lite_dev->release_cmd);
	sensor_lite_dev->release_cmd = NULL;
	kfree(sensor_lite_dev->start_cmd);
	sensor_lite_dev->start_cmd = NULL;
	kfree(sensor_lite_dev->stop_cmd);
	sensor_lite_dev->stop_cmd = NULL;

	/* free the queues */
	list_for_each_safe(pos,
		pos_next, &sensor_lite_dev->applied_request_q) {
		entry = list_entry(pos, struct sensor_lite_request, list);
		list_del(&entry->list);
		free_request_object(sensor_lite_dev, entry);
		sensor_lite_dev->applied_request_q_depth--;
	}

	list_for_each_safe(pos,
		pos_next, &sensor_lite_dev->waiting_request_q) {
		entry = list_entry(pos, struct sensor_lite_request, list);
		list_del(&entry->list);
		sensor_lite_dev->waiting_request_q_depth--;
		free_request_object(sensor_lite_dev, entry);
	}
}

int cam_sensor_lite_publish_dev_info(
	struct cam_req_mgr_device_info *info)
{
	int rc = 0;
	struct sensor_lite_device *sensor_lite_dev = NULL;

	if (!info) {
		CAM_ERR(CAM_SENSOR_LITE,
				"Invalid crm info handle");
		return -EINVAL;
	}

	sensor_lite_dev = (struct sensor_lite_device *)
		cam_get_device_priv(info->dev_hdl);

	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE,
				"Device data is NULL");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_SENSOR_LITE;
	strlcpy(info->name, CAM_SENSOR_LITE_NAME, sizeof(info->name));
	/* Hard code for now, piline delay should come from umd */
	info->p_delay = 2;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	CAM_DBG(CAM_SENSOR_LITE, "SENSOR_LITE PD delay:%d", info->p_delay);
	return rc;
}

int cam_sensor_lite_no_sync_handshake(
	struct cam_req_mgr_no_crm_handshake_data *info)
{
	int rc = 0;
	struct sensor_lite_device *sensor_lite_dev = NULL;

	if (!info) {
		CAM_ERR(CAM_SENSOR_LITE,
				"Invalid crm info handle");
		return -EINVAL;
	}

	sensor_lite_dev = (struct sensor_lite_device *)
		cam_get_device_no_crm_priv(info->dev_hdl);
	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE,
				"Device data is NULL");
		return -EINVAL;
	}

	info->pipeline_delay = SENSOR_LITE_DEFAULT_PD;
	info->trigger        = CAM_TRIGGER_POINT_SOF;
	sensor_lite_dev->crm_intf.frame_skip_cb = info->frame_skip_cb;
	sensor_lite_dev->anchor_pd = info->anchor_pd;

	CAM_DBG(CAM_SENSOR_LITE, "SENSOR_LITE PD delay:%d", info->pipeline_delay);
	return rc;
}

static int cam_sensor_lite_no_crm_apply_req(
	struct cam_req_mgr_no_crm_apply_request *apply)
{
	int i = 0, rc = 0;
	uint64_t req_id = 0;
	int      self_pd = 2;
	struct sensor_lite_request *req                   = NULL;
	struct sensor_lite_device  *dev                   = NULL;

	if (!apply) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid arguments ");
		return -EINVAL;
	}

	dev = (struct sensor_lite_device *)
		cam_get_device_no_crm_priv(apply->dev_hdl);
	if (!dev) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid sensor lite dev ctx");
		return -EINVAL;
	}

	req_id = apply->anchor_req_id - dev->anchor_pd - self_pd;
	CAM_DBG(CAM_SENSOR_LITE,
				"[%d] Got notify for frame [%lld]",
				dev->soc_info.index,
				apply->anchor_req_id - (self_pd - dev->anchor_pd));

	mutex_lock(&dev->mutex);

	if (!list_empty(&(dev->waiting_request_q))) {
		req = list_first_entry(&(dev->waiting_request_q),
				struct sensor_lite_request, list);
		list_del(&req->list);
		for (i = 0; i < req->num_cmds; i++) {
			rc = __send_pkt(dev,
					req->payload[i]);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR_LITE,
							"[%d] Apply failed req[%lld] payload[%d]",
							dev->soc_info.index,
							req->request_id,
							i);
				break;
			}
		}
		dev->waiting_request_q_depth--;
		free_request_object(
			dev,
			req);
	} else {
		CAM_INFO(CAM_SENSOR_LITE,
				"[%d] Delay in adding request to waiting queue[%lld]",
				dev->soc_info.index,
				apply->anchor_req_id - dev->anchor_pd - self_pd);
	}
	mutex_unlock(&dev->mutex);

	return rc;
}

int cam_sensor_lite_setup_link(
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct sensor_lite_device *sensor_lite_dev = NULL;

	if (!link) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid link.");
		return -EINVAL;
	}

	sensor_lite_dev = (struct sensor_lite_device *)
		cam_get_device_priv(link->dev_hdl);
	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&sensor_lite_dev->mutex);
	if (link->link_enable) {
		sensor_lite_dev->crm_intf.link_hdl = link->link_hdl;
		sensor_lite_dev->crm_intf.crm_cb = link->crm_cb;
		CAM_INFO(CAM_SENSOR_LITE, "SENSOR_LITE[%d] CRM enable link done",
				sensor_lite_dev->soc_info.index);
	} else {
		sensor_lite_dev->crm_intf.link_hdl = -1;
		sensor_lite_dev->crm_intf.crm_cb = NULL;
		CAM_INFO(CAM_SENSOR_LITE, "SENSOR_LITE[%d] CRM disable link done",
				sensor_lite_dev->soc_info.index);
	}
	mutex_unlock(&sensor_lite_dev->mutex);
	return 0;
}

static int apply_active_req_unsafe(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_req_mgr_apply_request *apply)
{
	int i = 0, rc = 0;
	struct sensor_lite_request *reapply_request = NULL;
	struct list_head *pos = NULL, *pos_next = NULL;
	struct sensor_lite_request *entry = NULL;

	if (!apply || !sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid argument");
		return -EINVAL;
	}

	if (list_empty(&(sensor_lite_dev->applied_request_q))) {
		CAM_ERR(CAM_SENSOR_LITE, "applied queue is empty");
		return -EINVAL;
	}

	list_for_each_safe(pos,
				pos_next, &sensor_lite_dev->applied_request_q) {
		entry = list_entry(pos, struct sensor_lite_request, list);
		/* This is a re apply case */
		if (entry->request_id == apply->request_id) {
			reapply_request = entry;
			break;
		}
	}

	/* if reapply needs to be done */
	if (reapply_request) {
		CAM_DBG(CAM_SENSOR_LITE,
			"SENSOR_LITE[%d] reapply request[%lld] matched",
			sensor_lite_dev->soc_info.index,
			apply->request_id);
		/* send  packet */
		for (i = 0; i < reapply_request->num_cmds; i++) {
			__send_pkt(sensor_lite_dev,
				reapply_request->payload[i]);
		}
	} else {
		CAM_ERR(CAM_SENSOR_LITE,
				"[%d] could not find re apply request[%lld] in applied queue",
				sensor_lite_dev->soc_info.index,
				apply->request_id);
		rc = -EINVAL;
	}
	return rc;
}

static int cam_sensor_lite_apply_req(
	struct cam_req_mgr_apply_request *apply)
{
	int rc = 0, i = 0;
	struct sensor_lite_device *sensor_lite_dev        = NULL;
	struct sensor_lite_request *oldest_active_req     = NULL;
	struct sensor_lite_request *req                   = NULL;

	if (!apply) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid parameters");
		return -EINVAL;
	}

	sensor_lite_dev = (struct sensor_lite_device *)
		cam_get_device_priv(apply->dev_hdl);

	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Device data is NULL");
		return -EINVAL;
	}

	CAM_DBG(CAM_SENSOR_LITE, "[%d] Got Apply request[%lld]",
			sensor_lite_dev->soc_info.index,
			apply->request_id);

	mutex_lock(&sensor_lite_dev->mutex);
	if (!list_empty(&(sensor_lite_dev->waiting_request_q))) {
		req = list_first_entry(&(sensor_lite_dev->waiting_request_q),
						struct sensor_lite_request, list);
		/* First check the waiting request Queue */
		if (req->request_id == apply->request_id) {
			list_del(&req->list);
			list_add_tail(&(req->list), &sensor_lite_dev->applied_request_q);
			sensor_lite_dev->applied_request_q_depth++;
			sensor_lite_dev->waiting_request_q_depth--;
			CAM_DBG(CAM_SENSOR_LITE,
					"SENSOR_LITE[%d] request[%lld] matched",
					sensor_lite_dev->soc_info.index,
					req->request_id);
			/* send  packet */
			for (i = 0; i < req->num_cmds; i++) {
				__send_pkt(sensor_lite_dev,
						req->payload[i]);
			}
		} else {
			/* There is some mismatch in apply */
			/* Check if this is a reapply call by iterating the active request q */
			rc = apply_active_req_unsafe(sensor_lite_dev, apply);
		}
	} else {
		CAM_DBG(CAM_SENSOR_LITE,
					"[%d] Got apply for request[%lld] when waiting queue is empty",
					sensor_lite_dev->soc_info.index,
					apply->request_id);
		rc = apply_active_req_unsafe(sensor_lite_dev, apply);
	}

	if (sensor_lite_dev->applied_request_q_depth >
				MAX_APPLIED_QUEUE_DEPTH) {
		oldest_active_req =
			list_first_entry(&(sensor_lite_dev->applied_request_q),
						struct sensor_lite_request, list);
		list_del(&oldest_active_req->list);
		sensor_lite_dev->applied_request_q_depth--;
		CAM_DBG(CAM_SENSOR_LITE,
					"[%d] Freeing oldest request[%lld] from applied request queue",
					sensor_lite_dev->soc_info.index,
					oldest_active_req->request_id);
		/* free the oldest request from the queue */
		free_request_object(
			sensor_lite_dev,
			oldest_active_req);
	}
	mutex_unlock(&sensor_lite_dev->mutex);

	return rc;
}

static int cam_sensor_lite_notify_frame_skip(
	struct cam_req_mgr_apply_request *apply)
{
	struct sensor_lite_device *sensor_lite_dev        = NULL;

	if (!apply) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid parameters");
		return -EINVAL;
	}

	sensor_lite_dev = (struct sensor_lite_device *)
		cam_get_device_priv(apply->dev_hdl);

	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Device data is NULL");
		return -EINVAL;
	}

	CAM_DBG(CAM_SENSOR_LITE,
					"[%d] Got Skip frame from crm",
					sensor_lite_dev->soc_info.index);
	return 0;
}

static int cam_sensor_lite_flush_waiting_q_unsafe(
	struct sensor_lite_device *dev,
	int flush_type,
	uint64_t req_id)
{
	int rc = 0;
	struct list_head *pos = NULL, *pos_next = NULL;
	struct sensor_lite_request *entry = NULL;

	if (!dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid Argument");
		return -EINVAL;
	}

	list_for_each_safe(pos,
		pos_next, &dev->waiting_request_q) {
		entry = list_entry(pos, struct sensor_lite_request, list);
		if ((flush_type == CAM_FLUSH_TYPE_ALL) ||
			(entry->request_id == req_id)) {
			list_del(&entry->list);
			dev->waiting_request_q_depth--;
			free_request_object(dev, entry);
			if (flush_type == CAM_FLUSH_TYPE_REQ)
				break;
		}
	}
	return rc;
}

static int cam_sensor_lite_flush_applied_q_unsafe(
	struct sensor_lite_device *dev,
	int flush_type,
	uint64_t req_id)
{
	int rc = 0;
	struct list_head *pos = NULL, *pos_next = NULL;
	struct sensor_lite_request *entry = NULL;

	if (!dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid Argument");
		return -EINVAL;
	}
	list_for_each_safe(pos,
		pos_next, &dev->applied_request_q) {
		entry = list_entry(pos, struct sensor_lite_request, list);
		if ((flush_type == CAM_FLUSH_TYPE_ALL) ||
			(entry->request_id == req_id)) {
			list_del(&entry->list);
			free_request_object(dev, entry);
			dev->applied_request_q_depth--;
			if (flush_type == CAM_FLUSH_TYPE_REQ)
				break;
		}
	}
	return rc;
}

static int cam_sensor_lite_flush_req_unsafe(
	struct sensor_lite_device *dev,
	int flush_type,
	uint64_t req_id)
{
	int rc = 0;

	if (!dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid Argument");
		return -EINVAL;
	}

	rc = cam_sensor_lite_flush_waiting_q_unsafe(
					dev,
					flush_type,
					req_id);

	if (rc == 0) {
		rc = cam_sensor_lite_flush_applied_q_unsafe(
					dev,
					flush_type,
					req_id);
	}

	return rc;
}

static int cam_sensor_lite_flush_req(
	struct cam_req_mgr_flush_request *flush)
{
	int rc = 0;
	int flush_type = 0;

	struct sensor_lite_device *sensor_lite_dev = NULL;

	if (!flush) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid argument !");
		return -EINVAL;
	}

	sensor_lite_dev = (struct sensor_lite_device *)
		cam_get_device_priv(flush->dev_hdl);

	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid dev !");
		return -EINVAL;
	}

	flush_type =
		(flush->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) ? CAM_FLUSH_TYPE_ALL :
			CAM_FLUSH_TYPE_REQ;

	mutex_lock(&sensor_lite_dev->mutex);
	rc = cam_sensor_lite_flush_req_unsafe(
					sensor_lite_dev,
					flush_type,
					flush->req_id);
	mutex_unlock(&sensor_lite_dev->mutex);
	return rc;
}

static int cam_sensor_lite_process_crm_evt(
	struct cam_req_mgr_link_evt_data *event)
{
	struct sensor_lite_device *sensor_lite_dev = NULL;

	if (!event) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid argument");
		return -EINVAL;
	}

	sensor_lite_dev = cam_get_device_priv(event->dev_hdl);
	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid dev_hdl");
		return -EINVAL;
	}

	switch (event->evt_type) {
	case CAM_REQ_MGR_LINK_EVT_SOF_FREEZE:
		CAM_DBG(CAM_SENSOR_LITE, "Sof freeze detected");
		break;
	default:
		CAM_DBG(CAM_SENSOR_LITE, "Got crm event notification: %d", event->evt_type);
		break;
	}
	return 0;
}

static int cam_sensor_lite_dump_req(
	struct cam_req_mgr_dump_info *dump_info)
{
	CAM_DBG(CAM_SENSOR_LITE, "Got dump request from CRM");
	return 0;
}

int sensor_lite_crm_intf_init(
	struct sensor_lite_device *sensor_lite_dev)
{
	if (sensor_lite_dev == NULL)
		return -EINVAL;

	sensor_lite_dev->crm_intf.device_hdl = -1;
	sensor_lite_dev->crm_intf.link_hdl = -1;
	sensor_lite_dev->crm_intf.ops.get_dev_info = cam_sensor_lite_publish_dev_info;
	sensor_lite_dev->crm_intf.ops.link_setup = cam_sensor_lite_setup_link;
	sensor_lite_dev->crm_intf.ops.apply_req = cam_sensor_lite_apply_req;
	sensor_lite_dev->crm_intf.ops.notify_frame_skip =
		cam_sensor_lite_notify_frame_skip;
	sensor_lite_dev->crm_intf.ops.flush_req = cam_sensor_lite_flush_req;
	sensor_lite_dev->crm_intf.ops.process_evt = cam_sensor_lite_process_crm_evt;
	sensor_lite_dev->crm_intf.ops.dump_req = cam_sensor_lite_dump_req;
	sensor_lite_dev->crm_intf.enable_crm = 0;
	sensor_lite_dev->crm_intf.no_crm_ops.handshake = cam_sensor_lite_no_sync_handshake;
	sensor_lite_dev->crm_intf.no_crm_ops.apply_req = cam_sensor_lite_no_crm_apply_req;

	return 0;
}

static int __cam_sensor_lite_handle_acquire_dev(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_sensorlite_acquire_dev *acquire)
{
	int rc = 0;
	struct cam_create_dev_hdl crm_intf_params;

	if (!sensor_lite_dev || !acquire) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid input ");
		rc = -EINVAL;
		goto exit;
	}

	if (sensor_lite_dev->state != CAM_SENSOR_LITE_STATE_INIT) {
		CAM_ERR(CAM_SENSOR_LITE,
				"SENSOR_LITE[%d] not in right state[%d] to acquire",
				sensor_lite_dev->soc_info.index, sensor_lite_dev->state);
		rc = -EINVAL;
		goto exit;
	}

	crm_intf_params.session_hdl = acquire->session_handle;

	/* crm ops should be assined in no crm case as well for error handling */
	crm_intf_params.ops = &sensor_lite_dev->crm_intf.ops;
	crm_intf_params.v4l2_sub_dev_flag = 0;
	crm_intf_params.media_entity_flag = 0;
	crm_intf_params.priv = sensor_lite_dev;
	crm_intf_params.no_crm_priv = NULL;
	crm_intf_params.dev_id = CAM_SENSOR_LITE;

	/* add crm callbacks only in case of with crm is enabled */
	if (acquire->info_handle & WITH_CRM_MASK) {
		sensor_lite_dev->crm_intf.enable_crm = 1;
		crm_intf_params.ops = &sensor_lite_dev->crm_intf.ops;
	} else {
		sensor_lite_dev->crm_intf.enable_crm = 0;
		crm_intf_params.no_crm_ops = &sensor_lite_dev->crm_intf.no_crm_ops;
		crm_intf_params.no_crm_priv = sensor_lite_dev;
	}

	sensor_lite_dev->type = HOST_DEST_CAM;
	acquire->device_handle =
		cam_create_device_hdl(&crm_intf_params);
	sensor_lite_dev->crm_intf.device_hdl  = acquire->device_handle;
	sensor_lite_dev->crm_intf.session_hdl = acquire->session_handle;
	sensor_lite_dev->state = CAM_SENSOR_LITE_STATE_ACQUIRE;
	CAM_INFO(CAM_SENSOR_LITE,
			"SENSOR_LITE[%d] Acquire Device done",
			sensor_lite_dev->soc_info.index);
	__send_pkt(sensor_lite_dev,
			&(sensor_lite_dev->acquire_cmd->header));
exit:
	return rc;
}

static int __cam_sensor_lite_handle_query_cap(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_sensorlite_query_cap *query)
{
	struct cam_hw_soc_info *soc_info = NULL;

	if (!sensor_lite_dev || !query) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid argument");
		return -EINVAL;
	}

	soc_info = &sensor_lite_dev->soc_info;
	CAM_INFO(CAM_SENSOR_LITE, "Handling  query capability for %d ",
			soc_info->index);
	query->slot_info = soc_info->index;
	query->version = 0x0;
	query->csiphy_slot_id = sensor_lite_dev->phy_id;

	return 0;
}

static int __cam_sensor_lite_handle_query_cap_v2(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_sensorlite_query_cap_v2 *query)
{
	struct cam_hw_soc_info *soc_info = NULL;

	if (!sensor_lite_dev || !query) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid argument");
		return -EINVAL;
	}

	soc_info = &sensor_lite_dev->soc_info;
	CAM_DBG(CAM_SENSOR_LITE, "Handling  query capability for %d ",
			soc_info->index);
	query->slot_info = soc_info->index;
	query->version = 0x0;
	query->csiphy_slot_id = sensor_lite_dev->phy_id;
	query->queue_depth = MAX_WAITING_QUEUE_DEPTH;

	return 0;
}

static int __cam_sensor_lite_handle_release_dev(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_release_dev_cmd *release)
{
	int rc = 0;

	if (!release || !sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid params");
		return -EINVAL;
	}

	if (release->dev_handle <= 0) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid device handle for context");
		return -EINVAL;
	}

	if (release->session_handle <= 0) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid session handle for context");
		return -EINVAL;
	}
	if (sensor_lite_dev->state == CAM_SENSOR_LITE_STATE_INIT) {
		CAM_WARN(CAM_SENSOR_LITE, "SENSOR_LITE[%d] not in right state[%d] to release",
				sensor_lite_dev->soc_info.index, sensor_lite_dev->state);
		return 0;
	}

	if (sensor_lite_dev->state == CAM_SENSOR_LITE_STATE_START) {
		CAM_DBG(CAM_SENSOR_LITE, "SENSOR_LITE[%d] release from start state",
						sensor_lite_dev->soc_info.index);
	} else {
		cam_destroy_device_hdl(sensor_lite_dev->crm_intf.device_hdl);
		sensor_lite_dev->crm_intf.device_hdl  = -1;
		sensor_lite_dev->crm_intf.session_hdl = -1;
		sensor_lite_dev->crm_intf.enable_crm  = 0;
		kfree(sensor_lite_dev->start_cmd);
		sensor_lite_dev->start_cmd = NULL;
		kfree(sensor_lite_dev->stop_cmd);
		sensor_lite_dev->stop_cmd = NULL;
		cam_sensor_lite_flush_req_unsafe(
				sensor_lite_dev,
				CAM_FLUSH_TYPE_ALL,
				0);
		CAM_INFO(CAM_SENSOR_LITE, "SENSOR_LITE[%d] Release Done.",
				sensor_lite_dev->soc_info.index);
		sensor_lite_dev->state = CAM_SENSOR_LITE_STATE_INIT;
	}

	__send_pkt(sensor_lite_dev,
		&(sensor_lite_dev->release_cmd->header));
	return rc;
}

static int __cam_sensor_lite_handle_start_cmd(
	struct sensor_lite_device *sensor_lite_dev,
	struct sensor_lite_start_stop_cmd *cmd)
{
	int rc = 0;

	if (sensor_lite_dev->start_cmd) {
		CAM_ERR(CAM_SENSOR_LITE, "Previous instance is not cleared");
		rc = -EINVAL;
	} else {
		sensor_lite_dev->start_cmd = kzalloc(cmd->header.size, GFP_KERNEL);
		if (sensor_lite_dev->start_cmd == NULL)
			return -ENOMEM;

		__set_slave_pkt_headers(&cmd->header, HCM_PKT_OPCODE_SENSOR_START_DEV);
		memcpy(sensor_lite_dev->start_cmd, cmd, cmd->header.size);

		CAM_DBG(CAM_SENSOR_LITE, "SENSOR_LITE[%d] start settings size: %d trigger mode:%d",
			sensor_lite_dev->soc_info.index,
			sensor_lite_dev->start_cmd->start_stop_settings_size,
			(sensor_lite_dev->start_cmd->start_stop_settings_size == 0));
	}
	return rc;
}

static int __cam_sensor_lite_handle_stop_cmd(
	struct sensor_lite_device *sensor_lite_dev,
	struct sensor_lite_start_stop_cmd *cmd)
{
	int rc = 0;

	if (sensor_lite_dev->stop_cmd) {
		CAM_ERR(CAM_SENSOR_LITE, "Previous instance is not cleared");
		rc = -EINVAL;
	} else {
		sensor_lite_dev->stop_cmd = kzalloc(cmd->header.size, GFP_KERNEL);
		if (sensor_lite_dev->stop_cmd == NULL)
			return -ENOMEM;
		__set_slave_pkt_headers(&cmd->header, HCM_PKT_OPCODE_SENSOR_STOP_DEV);
		memcpy(sensor_lite_dev->stop_cmd, cmd, cmd->header.size);
	}
	return rc;
}

static int __cam_sensor_lite_add_crm_req(
	struct sensor_lite_device *sensor_lite_dev,
	struct sensor_lite_request *req)
{
	int rc = 0;
	struct cam_req_mgr_add_request add_req = {0};

	if (sensor_lite_dev->waiting_request_q_depth < MAX_WAITING_QUEUE_DEPTH) {
		list_add_tail(&(req->list), &sensor_lite_dev->waiting_request_q);
	} else {
		CAM_ERR(CAM_SENSOR_LITE, "[%d] Wait queue full");
		return -EINVAL;
	}

	/* Add request only if crm is enabled*/
	if ((sensor_lite_dev->crm_intf.link_hdl != -1) &&
			(sensor_lite_dev->crm_intf.device_hdl != -1) &&
			(sensor_lite_dev->crm_intf.crm_cb != NULL) &&
			(sensor_lite_dev->crm_intf.enable_crm)) {
		add_req.link_hdl = sensor_lite_dev->crm_intf.link_hdl;
		add_req.dev_hdl  = sensor_lite_dev->crm_intf.device_hdl;
		add_req.req_id   = req->request_id;
		rc = sensor_lite_dev->crm_intf.crm_cb->add_req(&add_req);
	}
	return rc;
}

static int cam_sensor_lite_validate_cmd_descriptor(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_cmd_buf_desc *cmd_desc,
	uint32_t *cmd_type,
	uintptr_t *cmd_addr)
{
	int rc = 0;
	uintptr_t generic_ptr;
	size_t len_of_buff = 0;
	uint32_t                  *cmd_buf    = NULL;
	struct sensor_lite_header *cmd_header = NULL;

	if (!cmd_desc || !cmd_type || !cmd_addr)
		return -EINVAL;

	rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
		&generic_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_LITE,
			"Failed to get cmd buf Mem address : %d", rc);
		return rc;
	}

	cmd_buf = (uint32_t *)generic_ptr;
	cmd_buf += cmd_desc->offset / 4;
	cmd_header = (struct sensor_lite_header *)cmd_buf;

	if (len_of_buff < sizeof(struct sensor_lite_header)) {
		CAM_ERR(CAM_SENSOR_LITE,
				"Got invalid command descriptor of invalid cmd buffer size");
		rc = -EINVAL;
		goto end;
	}

	if (len_of_buff < cmd_header->size) {
		CAM_ERR(CAM_SENSOR_LITE, "Cmd header size mismatch");
		rc = -EINVAL;
		goto end;
	}

	*cmd_type = cmd_header->tag;
	*cmd_addr = (uintptr_t)cmd_header;

	if ((cmd_header->tag <= SENSORLITE_CMD_TYPE_INVALID) ||
			(cmd_header->tag >= SENSORLITE_CMD_TYPE_MAX)) {
		rc = -EINVAL;
	}
end:
	return rc;
}

static int cam_sensor_lite_cmd_buf_parse(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_packet *packet)
{
	int rc = 0, i = 0;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct sensor_lite_request *req = NULL;

	if (!sensor_lite_dev || !packet)
		return -EINVAL;

	if ((((packet->header.op_code & 0xFF)
			== CAM_SENSOR_LITE_PACKET_OPCODE_UPDATE) ||
		(packet->header.op_code & 0xFF)
			== CAM_SENSOR_LITE_PACKET_OPCODE_NOP) &&
		(packet->header.request_id >= 1) &&
		(sensor_lite_dev->start_cmd->start_stop_settings_size)) {
		req = sensor_lite_create_request_object(
				sensor_lite_dev,
				packet->header.request_id,
				CAM_SENSOR_LITE_PACKET_OPCODE_UPDATE);
	}

	CAM_DBG(CAM_SENSOR_LITE, "[%d] Req[%lld] opcode: %d",
					sensor_lite_dev->soc_info.index,
					packet->header.request_id,
					(packet->header.op_code & 0xFF));

	for (i = 0; i < packet->num_cmd_buf; i++) {
		uint32_t  cmd_type = -1;
		uintptr_t cmd_addr;
		cmd_desc = (struct cam_cmd_buf_desc *)
			((uint32_t *)&packet->payload +
			(packet->cmd_buf_offset / 4) +
			(i * (sizeof(struct cam_cmd_buf_desc)/4)));
		rc = cam_sensor_lite_validate_cmd_descriptor(
				sensor_lite_dev,
				cmd_desc,
				&cmd_type,
				&cmd_addr);
		if (rc < 0)
			goto end;

		switch (cmd_type) {
		case SENSORLITE_CMD_TYPE_SLAVEDESTINIT:
			sensor_lite_dev->type = SLAVE_DEST_CAM;
		case SENSORLITE_CMD_TYPE_HOSTDESTINIT:
		case SENSORLITE_CMD_TYPE_RESOLUTIONINFO:
			__send_pkt(sensor_lite_dev,
				(struct sensor_lite_header *)cmd_addr);
			break;
		case SENSORLITE_CMD_TYPE_START:
			__cam_sensor_lite_handle_start_cmd(sensor_lite_dev,
				(struct sensor_lite_start_stop_cmd *)cmd_addr);
			break;
		case SENSORLITE_CMD_TYPE_STOP:
			__cam_sensor_lite_handle_stop_cmd(sensor_lite_dev,
				(struct sensor_lite_start_stop_cmd *)cmd_addr);
			break;
		case SENSORLITE_CMD_TYPE_PERFRAME:
			/* In FSIN mode, start settings should be empty */
			if (!sensor_lite_dev->start_cmd->start_stop_settings_size) {
				__cam_sensor_lite_handle_perframe(sensor_lite_dev,
					(struct sensor_lite_perframe_cmd *)cmd_addr,
					packet->header.request_id);
			} else {
				/* Add request to the cmd buffers */
				rc = cam_sensor_lite_request_queue_cmd(
						sensor_lite_dev,
						req,
						(struct sensor_lite_header *)cmd_addr);
			}
			break;
		case SENSORLITE_CMD_TYPE_EXPOSUREUPDATE:
			/* Add request to the cmd buffers */
			rc = cam_sensor_lite_request_queue_cmd(
						sensor_lite_dev,
						req,
						(struct sensor_lite_header *)cmd_addr);
			break;
		default:
			CAM_INFO(CAM_SENSOR_LITE,
				"invalid packet tag received ignore for now %d",
				((struct sensor_lite_header *)cmd_addr)->tag);
			break;
		}
	}

	/* Add request */
	if ((sensor_lite_dev->start_cmd->start_stop_settings_size) &&
		(((packet->header.op_code & 0xFF)
			== CAM_SENSOR_LITE_PACKET_OPCODE_UPDATE) ||
		(packet->header.op_code & 0xFF)
			== CAM_SENSOR_LITE_PACKET_OPCODE_NOP) &&
		(packet->header.request_id >= 1) &&
		(req != NULL)) {
		rc = __cam_sensor_lite_add_crm_req(
					sensor_lite_dev,
					req);
	}
end:
	return rc;
}

static int cam_sensor_lite_packet_parse(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_config_dev_cmd *config)
{
	int rc = 0;
	uintptr_t generic_ptr;
	size_t len_of_buff = 0, remain_len = 0;
	struct cam_packet *csl_packet = NULL;

	rc = cam_mem_get_cpu_buf(config->packet_handle,
		&generic_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_LITE, "Failed in getting the packet: %d", rc);
		return rc;
	}

	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config->offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_SENSOR_LITE,
			"Inval cam_packet struct size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto end;
	}
	remain_len = len_of_buff;
	remain_len -= (size_t)config->offset;
	csl_packet = (struct cam_packet *)(generic_ptr +
		(uint32_t)config->offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid packet params");
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_SENSOR_LITE,
		"SENSOR_LITE[%d] Packet opcode = %d num_cmds: %d num_ios: %d num_patch: %d",
			sensor_lite_dev->soc_info.index,
			(csl_packet->header.op_code & 0xFF),
			csl_packet->num_cmd_buf,
			csl_packet->num_io_configs,
			csl_packet->num_patches);
	switch ((csl_packet->header.op_code & 0xFF)) {
	case CAM_SENSOR_LITE_PACKET_OPCODE_INITIAL_CONFIG:
		if (csl_packet->num_cmd_buf <= 0) {
			CAM_ERR(CAM_SENSOR_LITE, "Expecting atleast one command in Init packet");
			rc = -EINVAL;
			goto end;
		}
	case CAM_SENSOR_LITE_PACKET_OPCODE_UPDATE:
	case CAM_SENSOR_LITE_PACKET_OPCODE_NOP: {
		rc = cam_sensor_lite_cmd_buf_parse(sensor_lite_dev, csl_packet);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR_LITE, "CMD buffer parse failed");
			goto end;
		}
		break;
	}
	default:
		CAM_ERR(CAM_SENSOR_LITE, "SENSOR_LITE[%d] Invalid packet %x",
					sensor_lite_dev->soc_info.index,
					(csl_packet->header.op_code & 0xFF));
		rc = -EINVAL;
		break;
	}
end:
	return rc;
}

int __cam_sensor_lite_handle_start_dev(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_start_stop_dev_cmd *start)
{
	int rc = 0;

	if (sensor_lite_dev->state != CAM_SENSOR_LITE_STATE_ACQUIRE) {
		CAM_ERR(CAM_SENSOR_LITE, "SENSOR_LITE[%d] not in right state[%d] to start",
				sensor_lite_dev->soc_info.index, sensor_lite_dev->state);
		return -EINVAL;
	}

	sensor_lite_dev->state = CAM_SENSOR_LITE_STATE_START;
	CAM_INFO(CAM_SENSOR_LITE,
			"SENSOR_LITE[%d] START_DEV done.",
			sensor_lite_dev->soc_info.index);

	if (sensor_lite_dev->start_cmd != NULL) {
		__send_pkt(sensor_lite_dev,
			&(sensor_lite_dev->start_cmd->header));
	} else {
		CAM_ERR(CAM_SENSOR_LITE,
				"START CMD not received from user space");
		rc = -EINVAL;
	}

	return rc;
}

static int __cam_sensor_lite_handle_stop_dev(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_start_stop_dev_cmd *stop)
{
	int rc = 0;

	if (!stop || !sensor_lite_dev)
		return -EINVAL;

	if (stop->dev_handle <= 0) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid device handle for context");
		return -EINVAL;
	}

	if (stop->session_handle <= 0) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid session handle for context");
		return -EINVAL;
	}
	if (sensor_lite_dev->state != CAM_SENSOR_LITE_STATE_START) {
		CAM_WARN(CAM_SENSOR_LITE, "SENSOR_LITE[%d] not in right state[%d] to stop",
				sensor_lite_dev->soc_info.index, sensor_lite_dev->state);
	}
	if (rc) {
		CAM_ERR(CAM_SENSOR_LITE,
				"SENSOR_LITE[%d] STOP_DEV failed",
				sensor_lite_dev->soc_info.index);
	} else {
		/* Free all allocated streams during stop dev */
		sensor_lite_dev->state = CAM_SENSOR_LITE_STATE_ACQUIRE;
		CAM_INFO(CAM_SENSOR_LITE,
				"SENSOR_LITE[%d] STOP_DEV done.",
				sensor_lite_dev->soc_info.index);
	}

	if (sensor_lite_dev->stop_cmd != NULL) {
		__send_pkt(sensor_lite_dev,
			&(sensor_lite_dev->stop_cmd->header));
	} else {
		CAM_ERR(CAM_SENSOR_LITE, "stop cmd not received from UMD");
		rc = -EINVAL;
	}


	return rc;
}

static int __cam_sensor_lite_handle_config_dev(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_config_dev_cmd *config)
{
	int rc = 0;

	if (!config || !sensor_lite_dev)
		return -EINVAL;

	if (config->dev_handle <= 0) {
		CAM_ERR(CAM_SENSOR_LITE, "SENSOR_LITE[%d] Invalid device handle",
				sensor_lite_dev->soc_info.index);
		return -EINVAL;
	}

	if (config->session_handle <= 0) {
		CAM_ERR(CAM_SENSOR_LITE, "SENSOR_LITE[%d] Invalid session handle",
				sensor_lite_dev->soc_info.index);
		return -EINVAL;
	}

	if (sensor_lite_dev->state < CAM_SENSOR_LITE_STATE_ACQUIRE) {
		CAM_ERR(CAM_SENSOR_LITE, "SENSOR_LITE[%d] not in right state[%d] to configure",
				sensor_lite_dev->soc_info.index, sensor_lite_dev->state);
	}

	// Handle Config Dev
	rc = cam_sensor_lite_packet_parse(sensor_lite_dev, config);

	return rc;
}


int32_t __cam_sensor_lite_handle_probe(
	struct sensor_lite_device *sensor_lite_dev,
	uint64_t handle,
	uint32_t cmd)
{
	int rc = 0, i;
	uint32_t *cmd_buf = NULL;
	void *ptr = NULL;
	size_t len;
	struct cam_packet *pkt = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uintptr_t cmd_buf1 = 0;
	uintptr_t packet = 0;
	size_t    remain_len = 0;
	uint32_t probe_ver = 0;

	rc = cam_mem_get_cpu_buf(handle,
		&packet, &len);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_LITE, "Failed to get the command Buffer");
		return -EINVAL;
	}

	pkt = (struct cam_packet *)packet;
	if (pkt == NULL) {
		CAM_ERR(CAM_SENSOR_LITE, "packet pos is invalid");
		rc = -EINVAL;
		goto end;
	}

	if ((len < sizeof(struct cam_packet)) ||
		(pkt->cmd_buf_offset >= (len - sizeof(struct cam_packet)))) {
		CAM_ERR(CAM_SENSOR_LITE, "Not enough buf provided");
		rc = -EINVAL;
		goto end;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		CAM_ERR(CAM_SENSOR_LITE, "command descriptor pos is invalid");
		rc = -EINVAL;
		goto end;
	}

	probe_ver = pkt->header.op_code & 0xFFFFFF;
	CAM_DBG(CAM_SENSOR_LITE, "Received Header opcode: %u", probe_ver);
	if (probe_ver != CAM_SENSOR_LITE_PACKET_OPCODE_PROBE_V2) {
		CAM_ERR(CAM_SENSOR_LITE, "Expecting probe packet opcode %x", probe_ver);
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < pkt->num_cmd_buf; i++) {
		struct sensor_power_setting *pwr_on  = NULL;
		struct sensor_power_setting *pwr_off = NULL;
		ssize_t pwr_on_cmd_size              = 0;
		ssize_t pwr_off_cmd_size             = 0;
		struct probe_payload_v2 *probe       = NULL;

		if (!(cmd_desc[i].length))
			continue;

		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cmd_buf1, &len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR_LITE,
				"Failed to parse the command Buffer Header");
			goto end;
		}

		if (cmd_desc[i].offset >= len) {
			CAM_ERR(CAM_SENSOR_LITE,
				"offset past length of buffer");
			rc = -EINVAL;
			goto end;
		}

		remain_len = len - cmd_desc[i].offset;
		if (cmd_desc[i].length > remain_len) {
			CAM_ERR(CAM_SENSOR_LITE,
				"Not enough buffer provided for cmd");
			rc = -EINVAL;
			goto end;
		}

		cmd_buf  = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr      = (void *) cmd_buf;
		probe    = (struct probe_payload_v2 *) ptr;

		if (probe->header.tag != SENSORLITE_CMD_TYPE_PROBE)
			return -EINVAL;
		pwr_on_cmd_size = sizeof(struct sensor_lite_acquire_cmd) +
					(sizeof(struct sensor_power_setting) *
					probe->power_up_settings_size);

		sensor_lite_dev->acquire_cmd = kzalloc(pwr_on_cmd_size, GFP_KERNEL);
		if (!sensor_lite_dev->acquire_cmd) {
			CAM_ERR(CAM_SENSOR_LITE,
					"Could not allocate the memory for acquire_cmd");
			return -ENOMEM;
		}

		sensor_lite_dev->acquire_cmd->sensor_id             =
			probe->sensor_physical_id;
		sensor_lite_dev->acquire_cmd->power_settings_offset =
			sizeof(struct sensor_lite_acquire_cmd);
		sensor_lite_dev->acquire_cmd->power_settings_size   =
			probe->power_up_settings_size;

		pwr_on = (struct sensor_power_setting *)(
				(uint8_t *)sensor_lite_dev->acquire_cmd +
				sizeof(struct sensor_lite_acquire_cmd));
		pwr_off_cmd_size = sizeof(struct sensor_lite_release_cmd) +
					(sizeof(struct sensor_power_setting) *
					probe->power_down_settings_size);
		/* free during RELEASE_DEV */
		sensor_lite_dev->release_cmd = kzalloc(pwr_off_cmd_size, GFP_KERNEL);
		if (!sensor_lite_dev->release_cmd) {
			CAM_ERR(CAM_SENSOR_LITE,
					"Could not allocate the memory for acquire_cmd");
			kfree(sensor_lite_dev->acquire_cmd);
			sensor_lite_dev->acquire_cmd = NULL;
			return -ENOMEM;
		}

		/* initialize headers for acquire command */
		sensor_lite_dev->acquire_cmd->header.version = 0x1;
		sensor_lite_dev->acquire_cmd->header.tag     =
			HCM_PKT_OPCODE_SENSOR_ACQUIRE;
		sensor_lite_dev->acquire_cmd->header.size    =
			pwr_on_cmd_size;
		__set_slave_pkt_headers(&(sensor_lite_dev->acquire_cmd->header),
				HCM_PKT_OPCODE_SENSOR_ACQUIRE);

		/* initialize headers for release command */
		sensor_lite_dev->release_cmd->header.version = 0x1;
		sensor_lite_dev->release_cmd->header.tag     =
			HCM_PKT_OPCODE_SENSOR_RELEASE;
		sensor_lite_dev->release_cmd->header.size    =
			pwr_off_cmd_size;
		__set_slave_pkt_headers(&(sensor_lite_dev->release_cmd->header),
				HCM_PKT_OPCODE_SENSOR_RELEASE);

		sensor_lite_dev->release_cmd->sensor_id             =
			probe->sensor_physical_id;
		sensor_lite_dev->release_cmd->power_settings_offset =
			sizeof(struct sensor_lite_release_cmd);
		sensor_lite_dev->release_cmd->power_settings_size   =
			probe->power_down_settings_size;
		pwr_off = (struct sensor_power_setting *)(
				(uint8_t *)sensor_lite_dev->release_cmd +
				sizeof(struct sensor_lite_release_cmd));
		__copy_pwr_settings(pwr_on,
				probe,
				probe->power_up_settings_offset,
				probe->power_up_settings_size,
				probe->header.size);

		__copy_pwr_settings(pwr_off,
				probe,
				probe->power_down_settings_offset,
				probe->power_down_settings_size,
				probe->header.size);

	}

	if (ptr != NULL) {
		rc = __send_probe_pkt(sensor_lite_dev, ptr);
	} else {
		CAM_ERR(CAM_SENSOR_LITE, "ptr is NULL");
		rc = -EINVAL;
	}
end:
	return rc;
}

static int validate_ioctl_params(
	struct sensor_lite_device *sensor_lite_dev,
	struct cam_control *cmd)
{
	int rc = 0;

	if (!sensor_lite_dev || !cmd) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid input args");
		rc = -EINVAL;
		goto exit;
	}

	if ((cmd->op_code != CAM_SENSOR_PROBE_CMD) &&
			(cmd->handle_type != CAM_HANDLE_USER_POINTER)) {
		CAM_ERR(CAM_SENSOR_LITE,
			"SENSOR_LITE[%d] Invalid handle type: %d",
			sensor_lite_dev->soc_info.index,
			cmd->handle_type);
		rc = -EINVAL;
	}
	CAM_DBG(CAM_SENSOR_LITE,
			"SENSOR_LITE [%d] Opcode: %d",
			sensor_lite_dev->soc_info.index,
			cmd->op_code);
exit:
	return rc;
}

int cam_sensor_lite_core_cfg(
	struct sensor_lite_device *sensor_lite_dev,
	void *arg)
{
	int rc = 0;
	struct cam_control   *cmd = (struct cam_control *)arg;

	rc = validate_ioctl_params(sensor_lite_dev, cmd);
	if (rc < 0)
		return rc;

	mutex_lock(&sensor_lite_dev->mutex);
	switch (cmd->op_code) {
	case CAM_SENSOR_PROBE_CMD: {
		CAM_INFO(CAM_SENSOR_LITE, "SENSOR_LITE[%d] probe cmd received",
				sensor_lite_dev->soc_info.index);
		rc = __cam_sensor_lite_handle_probe(sensor_lite_dev, cmd->handle, cmd->op_code);
		break;
	}
	case CAM_ACQUIRE_DEV: {
		struct cam_sensorlite_acquire_dev acquire = {0};

		if (copy_from_user(&acquire, u64_to_user_ptr(cmd->handle),
			sizeof(acquire))) {
			rc = -EFAULT;
			break;
		}
		rc = __cam_sensor_lite_handle_acquire_dev(sensor_lite_dev, &acquire);
		if (rc) {
			CAM_ERR(CAM_SENSOR_LITE, "SENSOR_LITE[%d] acquire device failed(rc = %d)",
				sensor_lite_dev->soc_info.index,
				rc);
			break;
		}
		if (copy_to_user(u64_to_user_ptr(cmd->handle), &acquire,
			sizeof(acquire)))
			rc = -EFAULT;
		break;
	}
	case CAM_QUERY_CAP: {
		struct cam_sensorlite_query_cap query = {0};

		if (copy_from_user(&query, u64_to_user_ptr(cmd->handle),
			sizeof(query))) {
			rc = -EFAULT;
			break;
		}

		rc = __cam_sensor_lite_handle_query_cap(sensor_lite_dev, &query);
		if (rc) {
			CAM_ERR(CAM_SENSOR_LITE, "SENSOR LITE[%d] querycap is failed(rc = %d)",
				sensor_lite_dev->soc_info.index,
				rc);
			break;
		}

		if (copy_to_user(u64_to_user_ptr(cmd->handle), &query,
			sizeof(query)))
			rc = -EFAULT;

		break;
	}
	case CAM_QUERY_CAP_V2: {
		struct cam_sensorlite_query_cap_v2 query = {0};

		if (copy_from_user(&query, u64_to_user_ptr(cmd->handle),
			sizeof(query))) {
			rc = -EFAULT;
			break;
		}

		rc = __cam_sensor_lite_handle_query_cap_v2(sensor_lite_dev, &query);
		if (rc) {
			CAM_ERR(CAM_SENSOR_LITE, "SENSOR LITE[%d] querycap is failed(rc = %d)",
				sensor_lite_dev->soc_info.index,
				rc);
			break;
		}

		if (copy_to_user(u64_to_user_ptr(cmd->handle), &query,
			sizeof(query)))
			rc = -EFAULT;

		break;
	}
	case CAM_STOP_DEV: {
		struct cam_start_stop_dev_cmd stop;

		if (copy_from_user(&stop, u64_to_user_ptr(cmd->handle),
			sizeof(stop)))
			rc = -EFAULT;
		else {
			rc = __cam_sensor_lite_handle_stop_dev(sensor_lite_dev, &stop);
			if (rc)
				CAM_ERR(CAM_SENSOR_LITE,
					"SENSOR_LITE[%d] stop device failed(rc = %d)",
					sensor_lite_dev->soc_info.index,
					rc);
		}
		break;
	}
	case CAM_RELEASE_DEV: {
		struct cam_release_dev_cmd release;

		if (copy_from_user(&release, u64_to_user_ptr(cmd->handle),
			sizeof(release)))
			rc = -EFAULT;
		else {
			rc = __cam_sensor_lite_handle_release_dev(sensor_lite_dev, &release);
			if (rc)
				CAM_ERR(CAM_SENSOR_LITE,
					"SENSOR_LITE[%d] release device failed(rc = %d)",
					sensor_lite_dev->soc_info.index,
					rc);
		}
		break;
	}
	case CAM_CONFIG_DEV: {
		struct cam_config_dev_cmd config;

		if (copy_from_user(&config, u64_to_user_ptr(cmd->handle),
			sizeof(config)))
			rc = -EFAULT;
		else {
			rc = __cam_sensor_lite_handle_config_dev(sensor_lite_dev, &config);
			if (rc)
				CAM_ERR(CAM_SENSOR_LITE,
					"SENSOR_LITE[%d] config device failed(rc = %d)",
					sensor_lite_dev->soc_info.index,
					rc);
		}
		break;
	}
	case CAM_START_DEV: {
			/* As all sensors are connnected to Co-Processer For Single camera     */
			/* and multicamrea usecases, all Sensors start_dev should happen after */
			/* Aggregator PHY Start, The present code doesn't do that, to fix that */
			/* implemented a subdev notify from PHY to Sensor lite node to stream  */
			/* line the sensor start dev                                           */
		break;
	}
	case CAM_FLUSH_REQ: {
		struct cam_flush_dev_cmd flush;

		if (copy_from_user(&flush, u64_to_user_ptr(cmd->handle),
					sizeof(flush)))
			rc = -EFAULT;

		else {
			/* Flush the requests from the queue */
			rc = cam_sensor_lite_flush_req_unsafe(
					sensor_lite_dev,
					flush.flush_type,
					flush.req_id);
		}
		break;
	}
	default:
		CAM_ERR(CAM_SENSOR_LITE,
			"Invalid Opcode: %d",
			cmd->op_code);
		rc = -EINVAL;
		goto release_mutex;
	}
release_mutex:
	mutex_unlock(&sensor_lite_dev->mutex);
	return rc;
}
