// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/notifier.h>
#include <linux/pid.h>
#include <linux/fdtable.h>
#include <linux/rcupdate.h>
#include <linux/fs.h>
#include <linux/version.h>

#include "cam_rpmsg.h"
#include "cam_debug_util.h"
#include "cam_req_mgr_dev.h"
#include "cam_common_util.h"
#include "cam_jpeg_hw_mgr.h"
#include "cam_fastrpc.h"
#include "cam_trace.h"

#define CAM_SLAVE_CHANNEL_NAME "AH_CAM"

static int state;

static struct cam_rpmsg_instance_data cam_rpdev_idata[CAM_RPMSG_HANDLE_MAX];
struct cam_rpmsg_slave_pvt slave_private;
static struct cam_rpmsg_jpeg_pvt jpeg_private;

struct cam_rpmsg_jpeg_payload {
	struct rpmsg_device *rpdev;
	struct cam_jpeg_dsp2cpu_cmd_msg *rsp;
	ktime_t worker_scheduled_ts;
	struct work_struct work;
};

#define CAM_RPMSG_WORKQ_NUM_TASK 10

struct cam_rpmsg_system_data {
	struct completion complete;
	struct cam_req_mgr_core_worker *worker;
};

struct cam_rpmsg_system_data system_data;

const char *cam_rpmsg_slave_pl_type_to_string(unsigned int val)
{
	switch (val) {
	case CAM_RPMSG_SLAVE_PACKET_BASE_SYSTEM:
		return "SLAVE_BASE_SYSTEM";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_PING:
		return "SLAVE_SYSTEM_PING";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_UNUSED:
		return "SLAVE_ISP_UNUSED";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_ACQUIRE:
		return "SLAVE_ISP_ACQUIRE";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_RELEASE:
		return "SLAVE_ISP_RELEASE";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_INIT_CONFIG:
		return "SLAVE_ISP_INIT_CONFIG";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_START_DEV:
		return "SLAVE_ISP_START_DEV";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_STOP_DEV:
		return "SLAVE_ISP_STOP_DEV";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_ERROR:
		return "SLAVE_ISP_ERROR";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_MAX:
		return "SLAVE_ISP_MAX";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_SENSOR_UNUSED:
		return "SLAVE_SENSOR_UNUSED";
	case HCM_PKT_OPCODE_SENSOR_PROBE:
		return "HCM_SENSOR_PROBE";
	case HCM_PKT_OPCODE_SENSOR_PROBE_RESPONSE:
		return "HCM_SENSOR_PROBE_RESPONSE";
	case HCM_PKT_OPCODE_SENSOR_ACQUIRE:
		return "HCM_SENSOR_ACQUIRE";
	case HCM_PKT_OPCODE_SENSOR_RELEASE:
		return "HCM_SENSOR_RELEASE";
	case HCM_PKT_OPCODE_SENSOR_INIT:
		return "HCM_SENSOR_INIT";
	case HCM_PKT_OPCODE_SENSOR_CONFIG:
		return "HCM_SENSOR_CONFIG";
	case HCM_PKT_OPCODE_SENSOR_START_DEV:
		return "HCM_SENSOR_START_DEV";
	case HCM_PKT_OPCODE_SENSOR_STOP_DEV:
		return "HCM_SENSOR_STOP_DEV";
	case HCM_PKT_OPCODE_SENSOR_ERROR:
		return "HCM_SENSOR_ERROR";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_SENSOR_MAX:
		return "SLAVE_SENSOR_MAX";
	case HCM_PKT_OPCODE_PHY_ACQUIRE:
		return "HCM_PHY_ACQUIRE";
	case HCM_PKT_OPCODE_PHY_RELEASE:
		return "HCM_PHY_RELEASE";
	case HCM_PKT_OPCODE_PHY_INIT_CONFIG:
		return "HCM_PHY_INIT_CONFIG";
	case HCM_PKT_OPCODE_PHY_START_DEV:
		return "HCM_PHY_START_DEV";
	case HCM_PKT_OPCODE_PHY_STOP_DEV:
		return "HCM_PHY_STOP_DEV";
	case HCM_PKT_OPCODE_PHY_ERROR:
		return "HCM_PHY_ERROR";
	case CAM_RPMSG_SLAVE_PACKET_TYPE_PHY_MAX:
		return "SLAVE_PHY_MAX";
	case CAM_RPMSG_SLAVE_PACKET_BASE_DEBUG:
		return "SLAVE_BASE_DEBUG";
	default:
		return "UNKNOWN";
	}
}

const char *cam_rpmsg_dev_hdl_to_string(unsigned int val)
{
	switch (val) {
	case CAM_RPMSG_HANDLE_SLAVE: return "SLAVE";
	case CAM_RPMSG_HANDLE_JPEG: return "JPEG";
	default: return "UNKNOWN";
	}
}

static int cam_rpmsg_system_recv_worker(void *priv, void *data)
{
	struct cam_rpmsg_slave_payload_desc *pkt;

	pkt = (struct cam_rpmsg_slave_payload_desc *)(data);

	switch(CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(pkt)) {
		case CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_PING:
			CAM_DBG(CAM_RPMSG, "ping ack recv");
			complete(&system_data.complete);
			break;
		default:
			CAM_ERR(CAM_RPMSG, "Unexpected pkt type %x, len %d",
				CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(pkt),
				CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(pkt));

	}

	kfree(data);

	return 0;
}

static int cam_rpmsg_system_recv_irq_cb(void *cookie, void *data, int len)
{
	struct crm_worker_task *task;
	void *payload;
	int rc = 0;

	/* called in interrupt context */
	payload = kzalloc(len, GFP_ATOMIC);
	if (!payload) {
		CAM_ERR(CAM_RPMSG, "Failed to alloc payload");
		return -ENOMEM;
	}
	CAM_DBG(CAM_RPMSG, "Send %d bytes to worker", len);
	memcpy(payload, data, len);

	task = cam_req_mgr_worker_get_task(system_data.worker);
	if (IS_ERR_OR_NULL(task)) {
		CAM_ERR(CAM_RPMSG, "Failed to dequeue task = %d", PTR_ERR(task));
		return -EINVAL;
	}
	task->payload = payload;
	task->process_cb = cam_rpmsg_system_recv_worker;
	rc = cam_req_mgr_worker_enqueue_task(task, NULL, CRM_TASK_PRIORITY_0);
	if (rc) {
		CAM_ERR(CAM_RPMSG, "failed to enqueue task rc %d", rc);
	}

	return rc;
}

static const char *__dsp_cmd_to_string(uint32_t val)
{
	switch (val) {
	case CAM_CPU2DSP_SUSPEND: return "CPU2DSP_SUSPEND";
	case CAM_CPU2DSP_RESUME: return "CPU2DSP_RESUME";
	case CAM_CPU2DSP_SHUTDOWN: return "CPU2DSP_SHUTDOWN";
	case CAM_CPU2DSP_REGISTER_BUFFER: return "CPU2DSP_REGISTER_BUFFER";
	case CAM_CPU2DSP_DEREGISTER_BUFFER: return "CPU2DSP_DEREGISTER_BUFFER";
	case CAM_CPU2DSP_INIT: return "CPU2DSP_INIT";
	case CAM_CPU2DSP_SET_DEBUG_LEVEL: return "CPU2DSP_SET_DEBUG_LEVEL";
	case CAM_CPU2DSP_NOTIFY_ERROR: return "CPU2DSP_NOTIFY_ERROR";
	case CAM_CPU2DSP_MAX_CMD: return "CPU2DSP_MAX_CMD";
	case CAM_DSP2CPU_POWERON: return "DSP2CPU_POWERON";
	case CAM_DSP2CPU_POWEROFF: return "DSP2CPU_POWEROFF";
	case CAM_DSP2CPU_START: return "DSP2CPU_START";
	case CAM_DSP2CPU_DETELE_SESSION: return "DSP2CPU_DETELE_SESSION";
	case CAM_DSP2CPU_POWER_REQUEST: return "DSP2CPU_POWER_REQUEST";
	case CAM_DSP2CPU_POWER_CANCEL: return "DSP2CPU_POWER_CANCEL";
	case CAM_DSP2CPU_REGISTER_BUFFER: return "DSP2CPU_REGISTER_BUFFER";
	case CAM_DSP2CPU_DEREGISTER_BUFFER: return "DSP2CPU_DEREGISTER_BUFFER";
	case CAM_DSP2CPU_MEM_ALLOC: return "DSP2CPU_MEM_ALLOC";
	case CAM_DSP2CPU_MEM_FREE: return "DSP2CPU_MEM_FREE";
	case CAM_JPEG_DSP_MAX_CMD: return "JPEG_DSP_MAX_CMD";
	default: return "Unknown";
	}
}

static int cam_jpeg_rpmsg_send(void *data)
{
	int ret = 0;
	struct rpmsg_device *rpdev;
	struct cam_jpeg_cmd_msg *cmd_msg = data;
	struct cam_rpmsg_instance_data *idata;

	if (!cmd_msg) {
		CAM_ERR(CAM_RPMSG, "data is null");
		return -EINVAL;
	}

	idata = &cam_rpdev_idata[CAM_RPMSG_HANDLE_JPEG];

	mutex_lock(&idata->rpmsg_mutex);
	rpdev = cam_rpdev_idata[CAM_RPMSG_HANDLE_JPEG].rpdev;
	if (!rpdev) {
		mutex_unlock(&idata->rpmsg_mutex);
		CAM_ERR(CAM_RPMSG, "Send in disconnect");
		return -EBUSY;
	}


	trace_cam_rpmsg("JPEG", CAM_RPMSG_TRACE_BEGIN_TX,
		sizeof(*cmd_msg), __dsp_cmd_to_string(cmd_msg->cmd_msg_type));
	ret = rpmsg_send(rpdev->ept, cmd_msg, sizeof(*cmd_msg));
	trace_cam_rpmsg("JPEG", CAM_RPMSG_TRACE_END_TX,
		sizeof(*cmd_msg), __dsp_cmd_to_string(cmd_msg->cmd_msg_type));
	if (ret) {
		mutex_unlock(&idata->rpmsg_mutex);
		CAM_ERR(CAM_RPMSG, "rpmsg_send failed dev %d, rc %d",
			ret);
		return ret;
	}
	mutex_unlock(&idata->rpmsg_mutex);
	CAM_DBG(CAM_RPMSG, "sz %d rc %d", sizeof(*cmd_msg), ret);

	return ret;
}

static int cam_rpmsg_handle_jpeg_poweroff(struct cam_rpmsg_jpeg_payload *payload)
{
	struct cam_jpeg_cmd_msg cmd_msg = {0};
	struct cam_jpeg_dsp2cpu_cmd_msg *rsp = NULL;

	int rc;

	if (payload)
		rsp = payload->rsp;

	mutex_lock(&jpeg_private.jpeg_mutex);
	if (jpeg_private.status == CAM_JPEG_DSP_POWEROFF)  {
		mutex_unlock(&jpeg_private.jpeg_mutex);
		if (!rsp)
			return 0;
		CAM_INFO(CAM_RPMSG, "JPEG DSP already powered off");
		cmd_msg.cmd_msg_type = CAM_DSP2CPU_POWEROFF;
		rc = cam_jpeg_rpmsg_send(&cmd_msg);
		return rc;
	}

	rc = cam_mem_mgr_release_nsp_buf();
	if (rc)
		CAM_ERR(CAM_RPMSG, "failed to release nsp buf, rc=%d", rc);

	if (jpeg_private.pid != -1) {
		CAM_INFO(CAM_RPMSG, "JPEG DSP fastrpc unregister %x", jpeg_private.pid);
		rc = cam_fastrpc_driver_unregister(jpeg_private.pid);
		jpeg_private.pid = -1;
	}
	cam_jpeg_mgr_nsp_release_hw();

	jpeg_private.dmabuf_f_op = NULL;
	jpeg_private.status = CAM_JPEG_DSP_POWEROFF;
	mutex_unlock(&jpeg_private.jpeg_mutex);
	if (rsp) {
		cmd_msg.cmd_msg_type = CAM_DSP2CPU_POWEROFF;
		rc = cam_jpeg_rpmsg_send(&cmd_msg);
	}

	return rc;
}

int cam_rpmsg_send_cpu2dsp_error(int error_type, int core_id, void *data)
{
	int ret = 0;
	unsigned long rem_jiffies;
	struct cam_jpeg_cmd_msg cmd_msg = {0};

	cmd_msg.cmd_msg_type = CAM_CPU2DSP_NOTIFY_ERROR;
	cmd_msg.error_info.error_type = error_type;
	cmd_msg.error_info.core_id = core_id;
	if (data)
		cmd_msg.error_info.data.far = *((uint64_t *)data);

	mutex_lock(&jpeg_private.jpeg_mutex);
	if (error_type == CAM_JPEG_DSP_PC_ERROR) {
		if (jpeg_private.status == CAM_JPEG_DSP_POWEROFF) {
			mutex_unlock(&jpeg_private.jpeg_mutex);
			return 0;
		}
		reinit_completion(&jpeg_private.error_data.complete);
	}
	mutex_unlock(&jpeg_private.jpeg_mutex);

	ret = cam_jpeg_rpmsg_send(&cmd_msg);

	if (error_type == CAM_JPEG_DSP_PC_ERROR) {
		CAM_DBG(CAM_RPMSG, "Waiting for error ACK %d", error_type);
		/* wait for ping ack */
		rem_jiffies = cam_common_wait_for_completion_timeout(
				&jpeg_private.error_data.complete, msecs_to_jiffies(100));
		if (!rem_jiffies) {
			ret = -ETIMEDOUT;
			CAM_ERR(CAM_RPMSG, "Jpeg response timed out %d\n", rem_jiffies);
			goto err;
		}
		CAM_DBG(CAM_RPMSG, "received ACK");
		cam_rpmsg_handle_jpeg_poweroff(NULL);
	}

err:
	return ret;
}

int cam_rpmsg_system_send_ping(void) {
	int rc = 0, handle;
	unsigned long rem_jiffies;
	struct cam_rpmsg_system_ping_payload pkt = {0};

	reinit_completion(&system_data.complete);

	/* send ping command */
	CAM_RPMSG_SLAVE_SET_PAYLOAD_TYPE(&pkt.phdr,
		CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_PING);
	CAM_RPMSG_SLAVE_SET_PAYLOAD_SIZE(&pkt.phdr,
			sizeof(struct cam_rpmsg_system_ping_payload) -
			SLAVE_PKT_HDR_SIZE);

	CAM_DBG(CAM_RPMSG, "sending ping to slave");
	handle = cam_rpmsg_get_handle("helios");
	rc = cam_rpmsg_send(handle, &pkt, sizeof(pkt));
	if (rc) {
		CAM_ERR(CAM_RPMSG, "Failed to send ping command");
		goto err;
	}

	CAM_DBG(CAM_RPMSG, "Waiting for ACK");
	/* wait for ping ack */
	rem_jiffies = cam_common_wait_for_completion_timeout(
			&system_data.complete, msecs_to_jiffies(50));
	if (!rem_jiffies) {
		rc = -ETIMEDOUT;
		CAM_ERR(CAM_RPMSG, "slave PING response timed out %d\n", rc);
		goto err;
	}

	CAM_DBG(CAM_RPMSG, "received PING ACK");

err:
	return rc;
}

int cam_rpmsg_system_send_sync(struct cam_req_mgr_sync_mode_v2 *sync_info)
{
	int rc = 0, handle, num_remote = 0, i, j;
	struct cam_rpmsg_system_sync_payload *pkt = NULL;
	size_t sz = 0;

	for (i = 0; i < sync_info->num_links; i++) {
		if (sync_info->links[i].remote_sensor)
			num_remote++;
	}

	CAM_DBG(CAM_RPMSG, "sync command num_cams:%d", num_remote);

	if (!num_remote)
		return 0;

	sz = sizeof(struct cam_rpmsg_system_sync_payload) + ((num_remote - 1) * sizeof(uint32_t));
	pkt = kzalloc(sz, GFP_KERNEL);
	if (!pkt) {
		CAM_ERR(CAM_RPMSG, "Failed to alloc %d bytes", sz);
		return -ENOMEM;
	}

	pkt->num_cams = num_remote;
	for (i = 0, j = 0; i < sync_info->num_links; i++) {
		if (sync_info->links[i].remote_sensor)
			pkt->camera_id[j++] = sync_info->links[i].sensor_id;
	}

	CAM_RPMSG_SLAVE_SET_PAYLOAD_TYPE(&pkt->phdr,
		CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_SYNC);
	CAM_RPMSG_SLAVE_SET_PAYLOAD_SIZE(&pkt->phdr, sz - SLAVE_PKT_HDR_SIZE);

	handle = cam_rpmsg_get_handle("helios");
	rc = cam_rpmsg_send(handle, pkt, sz);
	if (rc)
		CAM_ERR(CAM_RPMSG, "Failed to send sync command rc %d", rc);

	kfree(pkt);
	return rc;
}

int cam_rpmsg_isp_send_acq(uint32_t sensor_id)
{
	struct cam_rpmsg_isp_acq_payload pkt = {0};
	int rc = 0, handle;

	CAM_RPMSG_SLAVE_SET_PAYLOAD_TYPE(&pkt.phdr,
			CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_ACQUIRE);
	CAM_RPMSG_SLAVE_SET_PAYLOAD_SIZE(&pkt.phdr,
			sizeof(struct cam_rpmsg_isp_acq_payload) -
			SLAVE_PKT_HDR_SIZE);
	pkt.version = PACKET_VERSION_1;
	pkt.sensor_id = sensor_id;

	handle = cam_rpmsg_get_handle("helios");
	rc = cam_rpmsg_send(handle, &pkt, sizeof(pkt));

	return rc;
}

int cam_rpmsg_isp_send_rel(uint32_t sensor_id)
{
	struct cam_rpmsg_isp_rel_payload pkt = {0};
	int rc = 0, handle;

	CAM_RPMSG_SLAVE_SET_PAYLOAD_TYPE(&pkt.phdr,
			CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_RELEASE);
	CAM_RPMSG_SLAVE_SET_PAYLOAD_SIZE(&pkt.phdr,
			sizeof(struct cam_rpmsg_isp_rel_payload) -
			SLAVE_PKT_HDR_SIZE);
	pkt.version = PACKET_VERSION_1;
	pkt.sensor_id = sensor_id;

	handle = cam_rpmsg_get_handle("helios");
	rc = cam_rpmsg_send(handle, &pkt, sizeof(pkt));

	return rc;
}

int cam_rpmsg_isp_send_start(uint32_t sensor_id)
{
	struct cam_rpmsg_isp_start_payload pkt = {0};
	int rc = 0, handle;

	CAM_RPMSG_SLAVE_SET_PAYLOAD_TYPE(&pkt.phdr,
			CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_START_DEV);
	CAM_RPMSG_SLAVE_SET_PAYLOAD_SIZE(&pkt.phdr,
			sizeof(struct cam_rpmsg_isp_start_payload) -
			SLAVE_PKT_HDR_SIZE);
	pkt.version = PACKET_VERSION_1;
	pkt.sensor_id = sensor_id;

	handle = cam_rpmsg_get_handle("helios");
	rc = cam_rpmsg_send(handle, &pkt, sizeof(pkt));

	return rc;
}

int cam_rpmsg_isp_send_stop(uint32_t sensor_id)
{
	struct cam_rpmsg_isp_start_payload pkt = {0};
	int rc = 0, handle;

	CAM_RPMSG_SLAVE_SET_PAYLOAD_TYPE(&pkt.phdr,
			CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_STOP_DEV);
	CAM_RPMSG_SLAVE_SET_PAYLOAD_SIZE(&pkt.phdr,
			sizeof(struct cam_rpmsg_isp_stop_payload) -
			SLAVE_PKT_HDR_SIZE);
	pkt.version = PACKET_VERSION_1;
	pkt.sensor_id = sensor_id;

	handle = cam_rpmsg_get_handle("helios");
	rc = cam_rpmsg_send(handle, &pkt, sizeof(pkt));

	return rc;
}

unsigned int cam_rpmsg_get_handle(char *name)
{
	if (!strcmp("helios", name))
		return CAM_RPMSG_HANDLE_SLAVE;
	else if (!strcmp("jpeg", name))
		return CAM_RPMSG_HANDLE_JPEG;
	else {
		CAM_ERR(CAM_RPMSG, "Unknown dev name %s", name);
		return CAM_RPMSG_HANDLE_MAX;
	}
}

int cam_rpmsg_is_channel_connected(unsigned int handle)
{
	struct rpmsg_device *rpdev = NULL;

	if (handle >= CAM_RPMSG_HANDLE_MAX)
		return -EINVAL;

	rpdev = cam_rpdev_idata[handle].rpdev;

	return rpdev != NULL;
}

static inline unsigned int cam_rpmsg_get_handle_from_dev(
		struct rpmsg_device *rpdev)
{
	unsigned int handle;
	struct cam_rpmsg_instance_data *idata = &cam_rpdev_idata[0];

	for (handle = 0; handle < CAM_RPMSG_HANDLE_MAX; handle++) {
		if (idata[handle].rpdev == rpdev)
			return handle;
	}
	CAM_ERR(CAM_RPMSG, "Failed to find handle for device");

	return CAM_RPMSG_HANDLE_MAX;
}

int cam_rpmsg_subscribe_slave_callback(unsigned int module_id,
	struct cam_rpmsg_slave_cbs cbs)
{
	struct cam_rpmsg_instance_data *idata =
		&cam_rpdev_idata[CAM_RPMSG_HANDLE_SLAVE];
	struct cam_rpmsg_slave_pvt *pvt =
		(struct cam_rpmsg_slave_pvt *)idata->pvt;
	unsigned long flag;

	if (module_id >= CAM_RPMSG_SLAVE_CLIENT_MAX) {
		CAM_ERR(CAM_RPMSG, "Invalid module_id %d", module_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&idata->sp_lock, flag);
	if (pvt->cbs[module_id].registered) {
		spin_unlock_irqrestore(&idata->sp_lock, flag);
		CAM_ERR(CAM_RPMSG, "cb already subscribed for mid %d",
			  module_id);
		return -EALREADY;
	}

	pvt->cbs[module_id] = cbs;
	pvt->cbs[module_id].registered = 1;
	spin_unlock_irqrestore(&idata->sp_lock, flag);

	CAM_DBG(CAM_RPMSG, "rpmsg callback subscribed for mid %d", module_id);

	return 0;
}

int cam_rpmsg_unsubscribe_slave_callback(int module_id)
{
	struct cam_rpmsg_instance_data *idata =
		&cam_rpdev_idata[CAM_RPMSG_HANDLE_SLAVE];
	struct cam_rpmsg_slave_pvt *pvt =
		(struct cam_rpmsg_slave_pvt *)idata->pvt;
	unsigned long flag;

	if (module_id >= CAM_RPMSG_SLAVE_CLIENT_MAX) {
		CAM_ERR(CAM_RPMSG, "Invalid module_id %d", module_id);
		return -EINVAL;
	}

	spin_lock_irqsave(&idata->sp_lock, flag);
	if (!pvt->cbs[module_id].registered) {
		spin_unlock_irqrestore(&idata->sp_lock, flag);
		CAM_ERR(CAM_RPMSG, "cb already unsubscribed for mid %d",
			  module_id);
		return -EALREADY;
	}

	pvt->cbs[module_id].registered = 0;
	pvt->cbs[module_id].cookie = NULL;
	pvt->cbs[module_id].recv = NULL;
	spin_unlock_irqrestore(&idata->sp_lock, flag);

	CAM_DBG(CAM_RPMSG, "rpmsg callback unsubscribed for mid %d", module_id);

	return 0;
}

int cam_rpmsg_register_status_change_event(unsigned int handle,
		struct notifier_block *nb)
{
	struct cam_rpmsg_instance_data *idata;

	if (handle >= CAM_RPMSG_HANDLE_MAX)
		return -EINVAL;

	if (!nb->notifier_call) {
		CAM_ERR(CAM_RPMSG, "Null notifier");
		return -EINVAL;
	}

	idata = &cam_rpdev_idata[handle];
	blocking_notifier_chain_register(&idata->status_change_notify, nb);

	return 0;
}

int cam_rpmsg_unregister_status_change_event(unsigned int handle,
		struct notifier_block *nb)
{
	struct cam_rpmsg_instance_data *idata;

	if (handle >= CAM_RPMSG_HANDLE_MAX)
		return -EINVAL;

	idata = &cam_rpdev_idata[handle];
	blocking_notifier_chain_unregister(&idata->status_change_notify, nb);

	return 0;
}

static void cam_rpmsg_notify_slave_status_change(
		struct cam_rpmsg_instance_data *idata, int status)
{
	if (!state) {
		CAM_DBG(CAM_RPMSG, "skip notify in deinit");
		return;
	}

	blocking_notifier_call_chain(&idata->status_change_notify,
		status, NULL);

}

void cam_rpmsg_slave_dump_pkt(void *pkt, size_t len);

int cam_rpmsg_send(unsigned int handle, void *data, int len)
{
	int ret = 0;
	struct rpmsg_device *rpdev;
	struct cam_slave_pkt_hdr *hdr = (struct cam_slave_pkt_hdr *)data;
	struct cam_rpmsg_slave_payload_desc *phdr;
	struct cam_rpmsg_instance_data *idata;

	if (len < (sizeof(struct cam_slave_pkt_hdr) +
		sizeof(struct cam_rpmsg_slave_payload_desc))) {
		CAM_ERR(CAM_RPMSG, "malformed packet, sz %d", len);
		return -EINVAL;
	}

	phdr = (struct cam_rpmsg_slave_payload_desc *)(PTR_TO_U64(data) +
		sizeof(struct cam_slave_pkt_hdr));

	if (handle >= CAM_RPMSG_HANDLE_MAX) {
		CAM_ERR(CAM_RPMSG, "Invalid handle %d", handle);
		return -EINVAL;
	}

	if(!data) {
		CAM_ERR(CAM_RPMSG, "data is null");
		return -EINVAL;
	}

	/* Data size should be multiple of 4 */
	if (len && len & 0x3) {
		CAM_ERR(CAM_RPMSG, "Invalid length %d", len);
		return -EINVAL;
	}

	CAM_RPMSG_SLAVE_SET_HDR_VERSION(hdr, 1);
	CAM_RPMSG_SLAVE_SET_HDR_DIRECTION(hdr, CAM_RPMSG_DIR_MASTER_TO_SLAVE);
	CAM_RPMSG_SLAVE_SET_HDR_NUM_PACKET(hdr, 1);
	CAM_RPMSG_SLAVE_SET_HDR_PACKET_SZ(hdr, len);

	if (slave_private.tx_dump) {
		cam_rpmsg_slave_dump_pkt(data, len);
		CAM_INFO(CAM_RPMSG, "hex_dump %d bytes, w %d", len, *(char *)data);
		print_hex_dump(KERN_INFO, "camera_dump:", DUMP_PREFIX_OFFSET, 16, 4, data, len, 0);
	}

	idata = &cam_rpdev_idata[CAM_RPMSG_HANDLE_SLAVE];
	mutex_lock(&idata->rpmsg_mutex);
	rpdev = cam_rpdev_idata[handle].rpdev;

	if (!rpdev) {
		mutex_unlock(&idata->rpmsg_mutex);
		CAM_ERR(CAM_RPMSG, "Send in disconnect");
		return -EBUSY;
	}

	trace_cam_rpmsg(cam_rpmsg_dev_hdl_to_string(handle), CAM_RPMSG_TRACE_BEGIN_TX,
		CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(phdr),
		cam_rpmsg_slave_pl_type_to_string(CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(phdr)));

	ret = rpmsg_send(rpdev->ept, data, len);

	trace_cam_rpmsg(cam_rpmsg_dev_hdl_to_string(handle), CAM_RPMSG_TRACE_END_TX,
		CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(phdr),
		cam_rpmsg_slave_pl_type_to_string(CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(phdr)));
	if (ret) {
		mutex_unlock(&idata->rpmsg_mutex);
		CAM_ERR(CAM_RPMSG, "rpmsg_send failed dev %d, rc %d",
			handle, ret);
		return ret;
	}
	mutex_unlock(&idata->rpmsg_mutex);
	CAM_DBG(CAM_RPMSG, "send hndl %d sz %d rc %d", handle, len, ret);

	return ret;
}

static void handle_jpeg_cb(struct work_struct *work) {
	struct cam_rpmsg_jpeg_payload *payload;
	struct cam_jpeg_cmd_msg   cmd_msg = {0};
	struct cam_mem_mgr_alloc_cmd alloc_cmd = {0};
	struct cam_mem_mgr_map_cmd map_cmd = {0};
	struct cam_mem_mgr_release_cmd release_cmd = {0};
	struct cam_jpeg_dsp2cpu_cmd_msg *rsp;
	struct rpmsg_device *rpdev;
	unsigned int handle;
	const char *dev_name = NULL;
	int rc = 0;
	int old_fd;
	struct pid *pid_s = NULL;
	struct files_struct *files;
	struct file *file;
	struct dma_buf *dbuf;

	payload = container_of(work, struct cam_rpmsg_jpeg_payload, work);
	if (!payload) {
		CAM_ERR(CAM_RPMSG, "NULL payload");
		return;
	}

	rsp = payload->rsp;
	rpdev = payload->rpdev;
	handle = cam_rpmsg_get_handle_from_dev(rpdev);
	dev_name = cam_rpmsg_dev_hdl_to_string(handle);
	CAM_DBG(CAM_RPMSG, "method %d %s", rsp->type, __dsp_cmd_to_string(rsp->type));
	trace_cam_rpmsg(dev_name, CAM_RPMSG_TRACE_RX, rsp->len, __dsp_cmd_to_string(rsp->type));

	switch(rsp->type) {
		case CAM_DSP2CPU_POWERON:
			mutex_lock(&jpeg_private.jpeg_mutex);
			if (CAM_JPEG_DSP_POWERON == jpeg_private.status) {
				CAM_INFO(CAM_RPMSG, "JPEG DSP already powered on");
				mutex_unlock(&jpeg_private.jpeg_mutex);
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_POWERON;
				rc = cam_jpeg_rpmsg_send(&cmd_msg);
				break;
			}
			cam_jpeg_mgr_nsp_acquire_hw(&jpeg_private.jpeg_iommu_hdl);
			rc = cam_fastrpc_driver_register(rsp->pid);
			if (rc) {
				jpeg_private.status = CAM_JPEG_DSP_POWEROFF;
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_POWERON;
				cmd_msg.ret_val = -1;
				mutex_unlock(&jpeg_private.jpeg_mutex);
				goto ack;
			}
			jpeg_private.pid = rsp->pid;
			cmd_msg.cmd_msg_type = CAM_DSP2CPU_POWERON;
			pid_s = find_get_pid(rsp->pid);
			if (pid_s == NULL) {
				CAM_ERR(CAM_RPMSG, "incorrect pid 0x%x", rsp->pid);
			}
			jpeg_private.jpeg_task = get_pid_task(pid_s, PIDTYPE_TGID);
			if (!jpeg_private.jpeg_task) {
				CAM_ERR(CAM_RPMSG, "task doesn't exist for pid 0x%x", pid_s);
			}
			jpeg_private.dmabuf_f_op = NULL;
			jpeg_private.status = CAM_JPEG_DSP_POWERON;
			mutex_unlock(&jpeg_private.jpeg_mutex);
ack:
			rc = cam_jpeg_rpmsg_send(&cmd_msg);
			reinit_completion(&jpeg_private.error_data.complete);
			break;

		case CAM_DSP2CPU_POWEROFF:
			cam_rpmsg_handle_jpeg_poweroff(payload);
			break;
		case CAM_DSP2CPU_MEM_ALLOC:
			alloc_cmd.flags = CAM_MEM_FLAG_NSP_ACCESS | CAM_MEM_FLAG_HW_READ_WRITE;
			alloc_cmd.len = rsp->buf_info.size;
			if (alloc_cmd.flags & CAM_MEM_FLAG_HW_READ_WRITE) {
				alloc_cmd.mmu_hdls[0] = jpeg_private.jpeg_iommu_hdl;
				alloc_cmd.num_hdl = 1;
			} else {
				alloc_cmd.num_hdl = 0;
			}

			mutex_lock(&jpeg_private.jpeg_mutex);
			if (jpeg_private.status == CAM_JPEG_DSP_POWEROFF) {
				CAM_WARN(CAM_RPMSG,
					"JPEG DSP powered off Cannot register/Alloc buffer");
				mutex_unlock(&jpeg_private.jpeg_mutex);
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_MEM_ALLOC;
				cmd_msg.buf_info.ipa_addr = 0;
				cmd_msg.ret_val = -1;
				goto send_ack;
			}
			mutex_unlock(&jpeg_private.jpeg_mutex);

			rc = cam_mem_mgr_alloc_and_map(&alloc_cmd);
			if (rc) {
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_MEM_ALLOC;
				cmd_msg.buf_info.ipa_addr = 0;
				cmd_msg.ret_val = -1;
				goto send_ack;
			}
			rc = cam_mem_get_io_buf(
				alloc_cmd.out.buf_handle,
				jpeg_private.jpeg_iommu_hdl,
				(dma_addr_t *)&cmd_msg.buf_info.iova, (size_t *)&cmd_msg.buf_info.size, NULL);
			if (rc) {
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_MEM_ALLOC;
				cmd_msg.buf_info.ipa_addr = 0;
				cmd_msg.ret_val = -1;
				goto send_ack;
			}
			cmd_msg.cmd_msg_type = CAM_DSP2CPU_MEM_ALLOC;
			cmd_msg.buf_info.fd = alloc_cmd.out.fd;
			cmd_msg.buf_info.ipa_addr = alloc_cmd.out.vaddr;
			cmd_msg.buf_info.buf_handle = alloc_cmd.out.buf_handle;
			if (!jpeg_private.dmabuf_f_op) {
				dbuf = dma_buf_get(alloc_cmd.out.fd);
				jpeg_private.dmabuf_f_op = (const struct file_operations *)dbuf->file->f_op;
			}
send_ack:
			CAM_DBG(CAM_RPMSG, "ALLOC_OUT fd %d ipa 0x%x iova 0x%x buf_handle %x",
				cmd_msg.buf_info.fd, cmd_msg.buf_info.ipa_addr,
				cmd_msg.buf_info.iova, cmd_msg.buf_info.buf_handle);

			rc = cam_jpeg_rpmsg_send(&cmd_msg);
			CAM_DBG(CAM_RPMSG, "closing dmabuf fd %d", cmd_msg.buf_info.fd);
			__close_fd(current->files, cmd_msg.buf_info.fd);
			break;
		case CAM_DSP2CPU_REGISTER_BUFFER:
			map_cmd.flags = CAM_MEM_FLAG_NSP_ACCESS | CAM_MEM_FLAG_HW_READ_WRITE;
			map_cmd.fd = rsp->buf_info.fd;
			map_cmd.mmu_hdls[0] = jpeg_private.jpeg_iommu_hdl;
			map_cmd.num_hdl = 1;

			mutex_lock(&jpeg_private.jpeg_mutex);
			if (jpeg_private.status == CAM_JPEG_DSP_POWEROFF) {
				mutex_unlock(&jpeg_private.jpeg_mutex);
				CAM_WARN(CAM_RPMSG,
					"JPEG DSP powered off Cannot register/Alloc buffer");
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_MEM_ALLOC;
				cmd_msg.buf_info.ipa_addr = 0;
				cmd_msg.ret_val = -1;
				goto send_ack;
			}
			mutex_unlock(&jpeg_private.jpeg_mutex);

			files = jpeg_private.jpeg_task->files;
			if (!files) {
				CAM_ERR(CAM_MEM, "null files");
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_REGISTER_BUFFER;
				cmd_msg.ret_val = -EINVAL;
				goto registerEnd;
			}
			rcu_read_lock();
			loop:
			// TODO: Things like this should be moved in cam_compat.
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0))
			file = fcheck_files(files, map_cmd.fd);
#else
			file = files_lookup_fd_rcu(files, map_cmd.fd);
#endif
			if (!file) {
				rcu_read_unlock();
				CAM_ERR(CAM_RPMSG, "null file");
				cmd_msg.cmd_msg_type = CAM_DSP2CPU_REGISTER_BUFFER;
				cmd_msg.ret_val = -EINVAL;
				goto registerEnd;
			} else {
				/* File object ref couldn't be taken.
				 * dup2() atomicity guarantee is the reason
				 * we loop to catch the new file (or NULL pointer)
				 */
				if (file->f_mode & FMODE_PATH) {
					CAM_ERR(CAM_RPMSG, "INCORRECT FMODE", file->f_mode);
					file = NULL;
					rcu_read_unlock();
					cmd_msg.cmd_msg_type = CAM_DSP2CPU_REGISTER_BUFFER;
					cmd_msg.ret_val = -EINVAL;
					goto registerEnd;
				}
				else if (!get_file_rcu_many(file, 1)) {
					CAM_ERR(CAM_RPMSG, "get_file_rcu_many 0");
					goto loop;
				}
			}
			rcu_read_unlock();

			/*
			 * TODO: If register_buf is called before MEM_ALLOC, then will it fail?
			 * we are assigning dmabuf_f_op in MEM_ALLOC.
			 */
			if (jpeg_private.dmabuf_f_op && file->f_op != jpeg_private.dmabuf_f_op) {
				CAM_ERR(CAM_RPMSG, "fd doesn't refer to dma_buf");
				break;
			} else {
				old_fd = map_cmd.fd;
				map_cmd.fd = dma_buf_fd((struct dma_buf *)(file->private_data), O_CLOEXEC);
				if (map_cmd.fd < 0) {
					CAM_ERR(CAM_RPMSG, "get fd fail, *fd=%d", map_cmd.fd);
					break;
				}
			}

			cam_mem_mgr_map(&map_cmd);
			rc = cam_mem_get_io_buf(
				map_cmd.out.buf_handle,
				jpeg_private.jpeg_iommu_hdl,
				(dma_addr_t *)&cmd_msg.buf_info.iova, (size_t *)&cmd_msg.buf_info.size, NULL);
			cmd_msg.cmd_msg_type = CAM_DSP2CPU_REGISTER_BUFFER;
			cmd_msg.buf_info.buf_handle = map_cmd.out.buf_handle;
			cmd_msg.buf_info.fd = old_fd;
			cmd_msg.buf_info.ipa_addr = alloc_cmd.out.vaddr;
			CAM_DBG(CAM_RPMSG, "MAP_OUT fd %d iova 0x%x size %d ipa %lx buf_handle %x",
				cmd_msg.buf_info.fd, cmd_msg.buf_info.iova, cmd_msg.buf_info.size,
				cmd_msg.buf_info.ipa_addr, cmd_msg.buf_info.buf_handle);
			CAM_DBG(CAM_RPMSG, "closing dmabuf fd %d", map_cmd.fd);
			__close_fd(current->files, map_cmd.fd);
registerEnd:
			rc = cam_jpeg_rpmsg_send(&cmd_msg);
			break;
		case CAM_DSP2CPU_DEREGISTER_BUFFER:
		case CAM_DSP2CPU_MEM_FREE:
			release_cmd.buf_handle = rsp->buf_info.buf_handle;
			mutex_lock(&jpeg_private.jpeg_mutex);
			rc = cam_mem_mgr_release(&release_cmd);
			mutex_unlock(&jpeg_private.jpeg_mutex);
			if (rc) {
				CAM_ERR(CAM_RPMSG, "Failed to release buffer for handle %d", rsp->buf_info.buf_handle);
				cmd_msg.cmd_msg_type = rsp->type;
				cmd_msg.ret_val = rc;
			} else {
				cmd_msg.cmd_msg_type = rsp->type;
				cmd_msg.buf_info.fd =  rsp->buf_info.fd;
				cmd_msg.buf_info.buf_handle = rsp->buf_info.buf_handle;
			}

			rc = cam_jpeg_rpmsg_send(&cmd_msg);
			break;
		case CAM_CPU2DSP_NOTIFY_ERROR:
			complete(&jpeg_private.error_data.complete);
			break;
		default:
			CAM_ERR(CAM_MEM, "Invalid command %d", rsp->type);
			break;
	}


}

static int cam_rpmsg_jpeg_cb(struct rpmsg_device *rpdev, void *data, int len,
	void *priv, u32 src)
{
	struct cam_jpeg_dsp2cpu_cmd_msg *rsp = (struct cam_jpeg_dsp2cpu_cmd_msg *)data;
	struct cam_rpmsg_jpeg_payload *payload = 0;
	bool work_status;

	CAM_DBG(CAM_RPMSG, "Received message %d", rsp->type);
	if (rsp->type < CAM_JPEG_DSP_MAX_CMD &&
		len == sizeof(struct cam_jpeg_dsp2cpu_cmd_msg)) {
		payload = kzalloc(sizeof(struct cam_rpmsg_jpeg_payload),
			GFP_ATOMIC);
		if (!payload) {
			CAM_ERR(CAM_MEM, "failed to allocate mem for payload");
			return -ENOMEM;
		}
		payload->rpdev = rpdev;
		payload->rsp   = rsp;

		INIT_WORK((struct work_struct *)&payload->work,
			handle_jpeg_cb);
		payload->worker_scheduled_ts = ktime_get();

		work_status = queue_work(
			jpeg_private.jpeg_work_queue,
			&payload->work);

		if (work_status == false) {
			CAM_ERR(CAM_CDM,
				"Failed to queue work for");
			kfree(payload);
			payload = NULL;
		}
	}
	return 0;
}

static int cam_rpmsg_create_slave_debug_fs(void)
{
	int rc = 0;

	slave_private.dentry = debugfs_create_dir("camera_rpmsg", NULL);
	if (!slave_private.dentry) {
		CAM_ERR(CAM_SMMU,"DebugFS could not create directory!");
		rc = -ENOENT;
		goto end;
	}

	debugfs_create_bool("slave_tx_dump_en", 0644,
		slave_private.dentry, &slave_private.tx_dump);
	debugfs_create_bool("slave_rx_dump_en", 0644,
		slave_private.dentry, &slave_private.rx_dump);
end:
	return rc;
}
static int cam_rpmsg_slave_cb(struct rpmsg_device *rpdev, void *data, int len,
	void *priv, u32 src)
{
	int ret = 0, processed = 0, client;
	int hdr_version, payload_type, payload_len, hdr_len;
	unsigned long flag;
	struct cam_rpmsg_instance_data *idata = dev_get_drvdata(&rpdev->dev);
	unsigned int handle = cam_rpmsg_get_handle_from_dev(rpdev);
	struct cam_slave_pkt_hdr *hdr = data;
	struct cam_rpmsg_slave_payload_desc *payload = NULL;
	struct cam_rpmsg_slave_pvt *pvt =
		(struct cam_rpmsg_slave_pvt *)idata->pvt;
	struct cam_rpmsg_slave_cbs *cb = NULL;
	int rc;


	if (len < (sizeof(struct cam_slave_pkt_hdr) +
				sizeof(struct cam_rpmsg_slave_payload_desc))) {
		CAM_ERR(CAM_RPMSG, "malformed packet, sz %d", len);
		return 0;
	}

	hdr_version = CAM_RPMSG_SLAVE_GET_HDR_VERSION(hdr);
	hdr_len = CAM_RPMSG_SLAVE_GET_HDR_PACKET_SZ(hdr);
	processed = sizeof(struct cam_slave_pkt_hdr);

	if (hdr_version != CAM_RPMSG_V1) {
		CAM_ERR(CAM_RPMSG, "Unsupported packet version %d",
				hdr_version);
		return 0;
	}
	if (slave_private.rx_dump) {
		print_hex_dump(KERN_INFO, "cam_rx_dump:", DUMP_PREFIX_OFFSET, 16, 4,
				data, len, 0);
	}

	CAM_DBG(CAM_RPMSG, "pkt_len %d, hdr_len %d", len, hdr_len);
	while (processed < len) {
		payload = (struct cam_rpmsg_slave_payload_desc *)
			((uintptr_t)data + processed);
		cb = NULL;
		rc = 0;
		client = CAM_RPMSG_SLAVE_CLIENT_MAX;
		payload_type = CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(payload);
		payload_len = CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(payload);

		CAM_DBG(CAM_RPMSG, "pld_type %x, pld_len %d", payload_type, payload_len);
		trace_cam_rpmsg(cam_rpmsg_dev_hdl_to_string(handle),
			CAM_RPMSG_TRACE_RX, payload_len,
			cam_rpmsg_slave_pl_type_to_string(payload_type));

		if (!payload_len) {
			CAM_ERR(CAM_RPMSG, "zero length payload, type %d", payload_type);
			return 0;
		}

		if (payload_type >= CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_UNUSED &&
			payload_type <= CAM_RPMSG_SLAVE_PACKET_TYPE_SYSTEM_MAX) {
			/* Hand off to system pkt handler */
			client = CAM_RPMSG_SLAVE_CLIENT_SYSTEM;
		} else if (payload_type >= CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_UNUSED &&
			payload_type <= CAM_RPMSG_SLAVE_PACKET_TYPE_ISP_MAX) {
			/* Hand off to ISP pkt handler */
			client = CAM_RPMSG_SLAVE_CLIENT_ISP;
		} else if (payload_type >= CAM_RPMSG_SLAVE_PACKET_TYPE_SENSOR_UNUSED &&
			payload_type <= CAM_RPMSG_SLAVE_PACKET_TYPE_SENSOR_MAX) {
			/* Hand off to SENSOR pkt handler */
			client = CAM_RPMSG_SLAVE_CLIENT_SENSOR;
		}else if (payload_type == CAM_RPMSG_SLAVE_PACKET_TYPE_DEBUG_LOG) {
			char *log_str = (char *)&payload[1];
			log_str[len - 8 - 1] = '\0';
			CAM_INFO(CAM_RPMSG, "%s: %s", "HELIOS", log_str);
			goto pkt_processed;
		} else {
			CAM_ERR(CAM_RPMSG, "Unexpected packet type %x len %d",
					payload_type, payload_len);
			print_hex_dump(KERN_INFO, "cam_recv_dump:", DUMP_PREFIX_OFFSET, 16, 4,
					payload, payload_len, 0);
		}

		spin_lock_irqsave(&idata->sp_lock, flag);
		if (client < CAM_RPMSG_SLAVE_CLIENT_MAX)
			cb = &pvt->cbs[client];

		if (!cb) {
			CAM_ERR(CAM_RPMSG, "cb is null for client %d", client);
		}else if(!cb->registered) {
			CAM_ERR(CAM_RPMSG, "cbs not registered for client %d",
					client);
		} else if (!cb->recv) {
			CAM_ERR(CAM_RPMSG, "recv not set for client %d, type %d",
					client, payload_type);
		} else {
			rc = cb->recv(cb->cookie, payload, payload_len);
			if (rc) {
				CAM_ERR(CAM_RPMSG, "recv_cb failed rc %d", rc);
				print_hex_dump(KERN_INFO, "cam_rx_fail:", DUMP_PREFIX_OFFSET, 16, 4,
					payload, payload_len, 0);
			}
		}
		spin_unlock_irqrestore(&idata->sp_lock, flag);

pkt_processed:
		processed += payload_len;
		trace_cam_rpmsg(cam_rpmsg_dev_hdl_to_string(handle), CAM_RPMSG_TRACE_RX,
				payload_len, cam_rpmsg_slave_pl_type_to_string(payload_type));
	}

	if (processed != len) {
		CAM_INFO(CAM_RPMSG, "%d bytes are left unprocessed len %d",
				len - processed, len);
	}

	return ret;
}

static int cam_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
						void *priv, u32 src)
{
	int rc = 0;
	unsigned int handle = cam_rpmsg_get_handle_from_dev(rpdev);
	unsigned long flag;
	struct cam_rpmsg_instance_data *idata = dev_get_drvdata(&rpdev->dev);
	cam_rpmsg_recv_cb cb;

	spin_lock_irqsave(&idata->sp_lock, flag);
	cb = idata->recv_cb;
	spin_unlock_irqrestore(&idata->sp_lock, flag);

	if (cb)
		rc = cb(rpdev, data, len, priv, src);
	else
		CAM_ERR(CAM_RPMSG, "No recv_cb for handle %d", handle);

	return rc;
}

int cam_rpmsg_set_recv_cb(unsigned int handle, cam_rpmsg_recv_cb cb)
{
	struct cam_rpmsg_instance_data *idata;
	unsigned long flags;

	if (handle >= CAM_RPMSG_HANDLE_MAX) {
		CAM_ERR(CAM_RPMSG, "Invalid handle %d", handle);
		return -EINVAL;
	}

	idata = &cam_rpdev_idata[handle];

	spin_lock_irqsave(&idata->sp_lock, flags);
	idata->recv_cb = cb;
	spin_unlock_irqrestore(&idata->sp_lock, flags);

	CAM_DBG(CAM_RPMSG, "handle %d cb %pS", handle, cb);
	return 0;
}

static int cam_rpmsg_slave_probe(struct rpmsg_device *rpdev)
{
	struct cam_rpmsg_instance_data *idata;
	struct cam_rpmsg_slave_cbs system_cb;
	unsigned int handle = CAM_RPMSG_HANDLE_SLAVE;
	int rc = 0;

	CAM_INFO(CAM_RPMSG, "src 0x%x -> dst 0x%x", rpdev->src, rpdev->dst);

	idata = &cam_rpdev_idata[CAM_RPMSG_HANDLE_SLAVE];
	dev_set_drvdata(&rpdev->dev, idata);
	mutex_lock(&idata->rpmsg_mutex);
	idata->rpdev = rpdev;
	mutex_unlock(&idata->rpmsg_mutex);

	cam_rpmsg_set_recv_cb(handle, cam_rpmsg_slave_cb);

	rc = cam_req_mgr_worker_create("cam_rpmsg_system_wq",
			CAM_RPMSG_WORKQ_NUM_TASK,
			&system_data.worker, CRM_WORKER_USAGE_IRQ,
			CAM_WORKER_FLAG_HIGH_PRIORITY);
	if (rc) {
		CAM_ERR(CAM_RPMSG, "Failed to create worker rc %d", rc);
		return -EINVAL;
	}
	/* set system recv */
	init_completion(&system_data.complete);
	system_cb.cookie = &system_data;
	system_cb.recv = cam_rpmsg_system_recv_irq_cb;
	cam_rpmsg_subscribe_slave_callback(CAM_RPMSG_SLAVE_CLIENT_SYSTEM,
			system_cb);

	cam_rpmsg_system_send_ping();
	cam_rpmsg_notify_slave_status_change(idata, CAM_REQ_MGR_SLAVE_UP);
	return 0;
}

static int cam_rpmsg_jpeg_probe(struct rpmsg_device *rpdev)
{
	int rc = 0;
	struct cam_rpmsg_instance_data *idata;

	CAM_INFO(CAM_RPMSG, "src 0x%x -> dst 0x%x", rpdev->src, rpdev->dst);
	idata = &cam_rpdev_idata[CAM_RPMSG_HANDLE_JPEG];
	dev_set_drvdata(&rpdev->dev, idata);
	mutex_lock(&idata->rpmsg_mutex);
	idata->rpdev = rpdev;
	mutex_unlock(&idata->rpmsg_mutex);

	if (!jpeg_private.jpeg_work_queue)
		jpeg_private.jpeg_work_queue = alloc_workqueue("jpeg_glink_workq",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS,
			5);

	if (!jpeg_private.jpeg_work_queue)
		CAM_ERR(CAM_CDM,
			"Workqueue allocation failed");

	cam_rpmsg_set_recv_cb(CAM_RPMSG_HANDLE_JPEG, cam_rpmsg_jpeg_cb);
	init_completion(&jpeg_private.error_data.complete);
	mutex_init(&jpeg_private.jpeg_mutex);
	jpeg_private.pid = -1;

	CAM_DBG(CAM_RPMSG, "rpmsg probe success for jpeg");

	return rc;
}

static void cam_rpmsg_slave_remove(struct rpmsg_device *rpdev)
{
	struct cam_rpmsg_instance_data *idata = dev_get_drvdata(&rpdev->dev);

	CAM_INFO(CAM_RPMSG, "Possible SSR");

	mutex_lock(&idata->rpmsg_mutex);
	idata->rpdev = NULL;
	mutex_unlock(&idata->rpmsg_mutex);
	cam_rpmsg_notify_slave_status_change(idata, CAM_REQ_MGR_SLAVE_DOWN);
}

static void cam_rpmsg_jpeg_remove(struct rpmsg_device *rpdev)
{
	struct cam_rpmsg_instance_data *idata = dev_get_drvdata(&rpdev->dev);
	struct cam_rpmsg_jpeg_payload *payload = 0;

	CAM_INFO(CAM_RPMSG, "jpeg rpmsg remove");

	payload = kzalloc(sizeof(struct cam_rpmsg_jpeg_payload),
		GFP_KERNEL);
	if (!payload) {
		CAM_ERR(CAM_MEM, "failed to allocate mem for payload");
		return;
	}
	payload->rpdev = idata->rpdev;
	payload->rsp   = NULL;

	cam_rpmsg_handle_jpeg_poweroff(payload);
	complete(&jpeg_private.error_data.complete);
	mutex_lock(&idata->rpmsg_mutex);
	idata->rpdev = NULL;
	mutex_unlock(&idata->rpmsg_mutex);
}

/* Channel name */
static struct rpmsg_device_id cam_rpmsg_slave_id_table[] = {
	{ .name = CAM_SLAVE_CHANNEL_NAME},
	{},
};

/* device tree compatible string */
static const struct of_device_id cam_rpmsg_slave_of_match[] = {
	{ .compatible = "qcom,cam-slave-rpmsg" },
	{ },
};
MODULE_DEVICE_TABLE(of, cam_rpmsg_slave_of_match);

static struct rpmsg_driver cam_rpmsg_slave_client = {
	.id_table               = cam_rpmsg_slave_id_table,
	.probe                  = cam_rpmsg_slave_probe,
	.callback               = cam_rpmsg_cb,
	.remove                 = cam_rpmsg_slave_remove,
	.drv                    = {
		.name           = "qcom,msm_slave_rpmsg",
		.of_match_table = cam_rpmsg_slave_of_match,
	},
};

static struct rpmsg_device_id cam_rpmsg_cdsp_id_table[] = {
	{ .name = "cam-nsp-jpeg" },
	{ },
};

static const struct of_device_id cam_rpmsg_jpeg_of_match[] = {
	{ .compatible = "qcom,cam-jpeg-rpmsg" },
	{ },
};
MODULE_DEVICE_TABLE(of, cam_rpmsg_jpeg_of_match);

static struct rpmsg_driver cam_rpmsg_jpeg_client = {
	.id_table               = cam_rpmsg_cdsp_id_table,
	.probe                  = cam_rpmsg_jpeg_probe,
	.callback               = cam_rpmsg_cb,
	.remove                 = cam_rpmsg_jpeg_remove,
	.drv                    = {
		.name           = "qcom,cam-jpeg-rpmsg",
	},
};

int cam_rpmsg_init(void)
{
	int rc = 0, i;
	struct cam_rpmsg_instance_data *idata;

	CAM_INFO(CAM_RPMSG, "Registering RPMSG driver");

	idata = &cam_rpdev_idata[CAM_RPMSG_HANDLE_SLAVE];
	idata->pvt = &slave_private;

	for (i = 0; i < CAM_RPMSG_HANDLE_MAX; i++) {
		spin_lock_init(&cam_rpdev_idata[i].sp_lock);
		mutex_init(&cam_rpdev_idata[i].rpmsg_mutex);
		BLOCKING_INIT_NOTIFIER_HEAD(
				&cam_rpdev_idata[i].status_change_notify);
	}
	cam_rpmsg_create_slave_debug_fs();
	state = 1;

	rc = register_rpmsg_driver(&cam_rpmsg_slave_client);
	if (rc) {
		CAM_ERR(CAM_RPMSG, "Failed to register slave rpmsg");
		return rc;
	}
	CAM_DBG(CAM_RPMSG, "salve channel %s, rc %d", CAM_SLAVE_CHANNEL_NAME, rc);

	rc = register_rpmsg_driver(&cam_rpmsg_jpeg_client);
	if (rc) {
		CAM_ERR(CAM_RPMSG, "Failed to register jpeg rpmsg");
		return rc;
	}

	return rc;
}

void cam_rpmsg_exit(void)
{
	state = 0;
	debugfs_remove_recursive(slave_private.dentry);
	unregister_rpmsg_driver(&cam_rpmsg_slave_client);
	unregister_rpmsg_driver(&cam_rpmsg_jpeg_client);
}

MODULE_DESCRIPTION("CAM Remote processor messaging driver");
MODULE_LICENSE("GPL v2");
