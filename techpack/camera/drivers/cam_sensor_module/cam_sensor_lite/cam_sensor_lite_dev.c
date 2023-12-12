// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>

#include "cam_sensor_lite_dev.h"
#include "cam_sensor_lite_core.h"
#include "cam_sensor_lite_soc.h"
#include "cam_req_mgr_dev.h"
#include <media/cam_sensor.h>
#include "camera_main.h"
#include "cam_rpmsg.h"
#include <dt-bindings/msm-camera.h>
#include <cam_sensor_lite_ext_headers.h>

#define SENSOR_LITE_DEBUG_FILE_NAME_SIZE     27
#define CAM_SENSOR_LITE_RPMSG_WORKQ_NUM_TASK 10

static struct dentry *debugfs_root;

static void cam_sensor_lite_subdev_handle_message(
		struct v4l2_subdev *sd,
		enum cam_subdev_message_type_t message_type,
		void *data)
{
	struct sensor_lite_device *sensor_lite_dev = v4l2_get_subdevdata(sd);
	uint32_t data_idx;

	switch (message_type) {
	case CAM_SUBDEV_MESSAGE_REG_DUMP:
		data_idx = *(uint32_t *)data;
		CAM_INFO(CAM_SENSOR_LITE, "subdev index : %d Sensor Lite index: %d",
				sensor_lite_dev->soc_info.index, data_idx);
		break;
	case CAM_SUBDEV_MESSAGE_START_SENSORLITE: {
		data_idx = *(uint32_t *)data;
		CAM_INFO(CAM_SENSOR_LITE, "SENSOR_LITE[%d] Start Sensorlite: phy_idx : %d",
		sensor_lite_dev->soc_info.index, data_idx);

		__cam_sensor_lite_handle_start_dev(sensor_lite_dev, NULL);
		break;
	}
	case CAM_SUBDEV_MESSAGE_PROBE_RES: {
		struct probe_response_packet *pkt =
			(struct probe_response_packet *)data;
		uint32_t sensor_id =  pkt->probe_response.sensor_id;

		if (sensor_lite_dev->soc_info.index == sensor_id) {
			CAM_INFO(CAM_SENSOR_LITE, "probe response received for : %d", sensor_id);
			memcpy(&sensor_lite_dev->probe_info,
					&(pkt->probe_response),
					sizeof(struct sensor_probe_response));
			complete(&sensor_lite_dev->complete);
		}
		break;
	}
	default:
		break;
	}
}

static int cam_sensor_lite_subdev_close_internal(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct sensor_lite_device *sensor_lite_dev =
		v4l2_get_subdevdata(sd);

	if (!sensor_lite_dev) {
		CAM_ERR(CAM_SENSOR_LITE, "sensor_lite_dev ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&sensor_lite_dev->mutex);
	cam_sensor_lite_shutdown(sensor_lite_dev);
	mutex_unlock(&sensor_lite_dev->mutex);

	return 0;
}

static int cam_sensor_lite_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open();

	if (crm_active) {
		CAM_DBG(CAM_SENSOR_LITE, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return cam_sensor_lite_subdev_close_internal(sd, fh);
}

static long cam_sensor_lite_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct sensor_lite_device *sensor_lite_dev = v4l2_get_subdevdata(sd);
	int rc = 0;

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_sensor_lite_core_cfg(sensor_lite_dev, arg);
		if (rc)
			CAM_ERR(CAM_SENSOR_LITE,
				"Failed in configuring the device: %d", rc);
		break;
	case CAM_SD_SHUTDOWN:
		if (!cam_req_mgr_is_shutdown()) {
			CAM_ERR(CAM_SENSOR_LITE, "SD shouldn't come from user space");
			return 0;
		}

		rc = cam_sensor_lite_subdev_close_internal(sd, NULL);
		break;
	default:
		CAM_ERR(CAM_SENSOR_LITE, "Wrong ioctl : %d", cmd);
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_sensor_lite_subdev_compat_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	int32_t rc = 0;
	struct cam_control cmd_data;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_SENSOR_LITE, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	/* All the arguments converted to 64 bit here
	 * Passed to the api in core.c
	 */
	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_sensor_lite_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc)
			CAM_ERR(CAM_SENSOR_LITE,
				"Failed in subdev_ioctl: %d", rc);
		break;
	default:
		CAM_ERR(CAM_SENSOR_LITE, "Invalid compat ioctl cmd: %d", cmd);
		rc = -ENOIOCTLCMD;
		break;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_SENSOR_LITE,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static struct v4l2_subdev_core_ops sensor_lite_subdev_core_ops = {
	.ioctl = cam_sensor_lite_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_sensor_lite_subdev_compat_ioctl,
#endif
};

static const struct v4l2_subdev_ops sensor_lite_subdev_ops = {
	.core = &sensor_lite_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops sensor_lite_subdev_intern_ops = {
	.close = cam_sensor_lite_subdev_close,
};

static int cam_sensor_lite_get_debug(void *data, u64 *val)
{
	struct sensor_lite_device *sensor_lite_dev =
		(struct sensor_lite_device *)data;

	if (val == NULL || sensor_lite_dev == NULL) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid argument");
		return -EINVAL;
	}
	*val = sensor_lite_dev->dump_en;

	return 0;
}

static int cam_sensor_lite_set_debug(void *data, u64 val)
{
	struct sensor_lite_device *sensor_lite_dev =
		(struct sensor_lite_device *)data;

	if (sensor_lite_dev == NULL) {
		CAM_ERR(CAM_SENSOR_LITE, "invalid argument");
		return -EINVAL;
	}
	sensor_lite_dev->dump_en = val;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_sensor_lite_debug,
	cam_sensor_lite_get_debug,
	cam_sensor_lite_set_debug, "%16llu\n");

static int cam_sensor_lite_create_debugfs_entry(
	struct sensor_lite_device *sensor_lite_dev)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;
	char   debug_file[SENSOR_LITE_DEBUG_FILE_NAME_SIZE] = {0};

	if (!debugfs_root) {
		dbgfileptr = debugfs_create_dir("cam_sensor_lite", NULL);
		if (!dbgfileptr) {
			CAM_ERR(CAM_SENSOR_LITE, "debugfs directory creation fail");
			rc = -ENOENT;
			goto end;
		}
		debugfs_root = dbgfileptr;
	}
	sensor_lite_dev->dump_en = 0;

	snprintf(debug_file,
			SENSOR_LITE_DEBUG_FILE_NAME_SIZE,
			"sensorlite%d_enable_dump",
			sensor_lite_dev->soc_info.index);

	dbgfileptr = debugfs_create_file(debug_file, 0644,
			debugfs_root, sensor_lite_dev, &cam_sensor_lite_debug);
	if (IS_ERR(dbgfileptr)) {
		if (PTR_ERR(dbgfileptr) == -ENODEV)
			CAM_WARN(CAM_SENSOR_LITE, "DebugFS not enabled");
		else {
			rc = PTR_ERR(dbgfileptr);
			goto end;
		}
	}
end:
	return rc;
}

static int cam_sensor_lite_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct sensor_lite_device *sensor_lite_dev = NULL;
	int32_t               rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	sensor_lite_dev = devm_kzalloc(&pdev->dev,
		sizeof(struct sensor_lite_device), GFP_KERNEL);
	if (!sensor_lite_dev)
		return -ENOMEM;

	mutex_init(&sensor_lite_dev->mutex);
	sensor_lite_dev->v4l2_dev_str.pdev = pdev;

	sensor_lite_dev->soc_info.pdev = pdev;
	sensor_lite_dev->soc_info.dev = &pdev->dev;
	sensor_lite_dev->soc_info.dev_name = pdev->name;
	sensor_lite_dev->ref_count = 0;

	rc = cam_sensor_lite_parse_dt_info(pdev, sensor_lite_dev);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR_LITE,  "DT parsing failed: %d", rc);

	sensor_lite_dev->v4l2_dev_str.internal_ops =
		&sensor_lite_subdev_intern_ops;
	sensor_lite_dev->v4l2_dev_str.ops =
		&sensor_lite_subdev_ops;
	snprintf(sensor_lite_dev->device_name,
		CAM_CTX_DEV_NAME_MAX_LENGTH,
		"%s%d", CAMX_SENOSR_LITE_DEV_NAME,
		sensor_lite_dev->soc_info.index);
	sensor_lite_dev->v4l2_dev_str.name =
		sensor_lite_dev->device_name;
	sensor_lite_dev->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	sensor_lite_dev->v4l2_dev_str.ent_function =
		CAM_SENSORLITE_DEVICE_TYPE;
	sensor_lite_dev->v4l2_dev_str.msg_cb =
		cam_sensor_lite_subdev_handle_message;
	sensor_lite_dev->v4l2_dev_str.token =
		sensor_lite_dev;
	sensor_lite_dev->v4l2_dev_str.close_seq_prior =
		CAM_SD_CLOSE_MEDIUM_PRIORITY;

	rc = cam_register_subdev(&(sensor_lite_dev->v4l2_dev_str));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_LITE, "cam_register_subdev Failed rc: %d", rc);
		goto err_exit_1;
	}

	platform_set_drvdata(pdev, &(sensor_lite_dev->v4l2_dev_str.sd));
	sensor_lite_crm_intf_init(sensor_lite_dev);

	/* Non real time device */
	sensor_lite_dev->acquire_cmd      = NULL;
	sensor_lite_dev->release_cmd      = NULL;
	sensor_lite_dev->start_cmd        = NULL;
	sensor_lite_dev->stop_cmd         = NULL;

	init_completion(&(sensor_lite_dev->complete));

	INIT_LIST_HEAD(&(sensor_lite_dev->waiting_request_q));
	INIT_LIST_HEAD(&(sensor_lite_dev->applied_request_q));
	sensor_lite_dev->applied_request_q_depth = 0;
	sensor_lite_dev->waiting_request_q_depth = 0;

	rc = cam_sensor_lite_create_debugfs_entry(sensor_lite_dev);
	if (rc < 0)
		goto err_exit_1;

	return rc;

err_exit_1:
	kfree(sensor_lite_dev);
	return rc;
}

static void cam_sensor_lite_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct platform_device *pdev               =
		to_platform_device(dev);
	struct sensor_lite_device *sensor_lite_dev =
		platform_get_drvdata(pdev);
	int    rc                                  = 0;

	rc = cam_unregister_subdev(&(sensor_lite_dev->v4l2_dev_str));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_LITE, "sensor_lite[%d] unregister failed",
				sensor_lite_dev->soc_info.index);
	}

	kfree(sensor_lite_dev);
	return;
}

const static struct component_ops cam_sensor_lite_component_ops = {
	.bind = cam_sensor_lite_component_bind,
	.unbind = cam_sensor_lite_component_unbind,
};

static int32_t cam_sensor_lite_platform_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_SENSOR_LITE, "Adding SENSOR LITE component");
	rc = component_add(&pdev->dev, &cam_sensor_lite_component_ops);
	if (rc)
		CAM_ERR(CAM_SENSOR_LITE, "failed to add component rc: %d", rc);
	return rc;
}


static int32_t cam_sensor_lite_device_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_sensor_lite_component_ops);
	return 0;
}

static const struct of_device_id cam_sensor_lite_dt_match[] = {
	{.compatible = "qcom,cam-sensor-lite"},
	{}
};

MODULE_DEVICE_TABLE(of, cam_sensor_lite_dt_match);

struct platform_driver cam_sensor_lite_driver = {
	.probe  = cam_sensor_lite_platform_probe,
	.remove = cam_sensor_lite_device_remove,
	.driver = {
		.name  = CAMX_SENOSR_LITE_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_sensor_lite_dt_match,
		.suppress_bind_attrs = true,
	},
};

static struct cam_req_mgr_core_workq *sensor_lite_rpmsg_workq;

static int sensor_lite_rpmsg_recv_worker(void *priv, void *data)
{
	struct cam_rpmsg_slave_payload_desc *pkt = NULL;

	if (!data) {
		CAM_ERR(CAM_SENSOR_LITE, "Invalid data pointer");
		return -EINVAL;
	}

	pkt = (struct cam_rpmsg_slave_payload_desc *)(data);
	switch (CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(pkt)) {
	case HCM_PKT_OPCODE_SENSOR_PROBE_RESPONSE:
	{
		if ((CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(pkt) - SLAVE_PKT_PLD_SIZE) !=
			      sizeof(struct probe_response_packet)) {
			CAM_ERR(CAM_SENSOR_LITE,
					"received probe response packet of invalid size");
			goto exit_1;
		}
		cam_subdev_notify_message(CAM_SENSORLITE_DEVICE_TYPE,
			CAM_SUBDEV_MESSAGE_PROBE_RES,
			(void *)((uint8_t *)data + SLAVE_PKT_PLD_SIZE));
		break;
	}
	default:
		CAM_ERR(CAM_RPMSG, "Unexpected pkt type %x, len %d",
			CAM_RPMSG_SLAVE_GET_PAYLOAD_TYPE(pkt),
			CAM_RPMSG_SLAVE_GET_PAYLOAD_SIZE(pkt));

	}
exit_1:
	kfree(data);

	return 0;
}


static int sensor_lite_recv_irq_cb(void *cookie, void *data, int len)
{
	struct crm_workq_task *task;
	void *payload;
	int rc = 0;

	/* called in interrupt context */
	payload = kzalloc(len, GFP_ATOMIC);
	if (!payload) {
		CAM_ERR(CAM_RPMSG, "Failed to alloc payload");
		return -ENOMEM;
	}
	memcpy(payload, data, len);

	task = cam_req_mgr_workq_get_task(sensor_lite_rpmsg_workq);
	if (task == NULL) {
		rc = -EINVAL;
		goto err_exit1;
	}

	task->payload = payload;
	task->process_cb = sensor_lite_rpmsg_recv_worker;
	rc = cam_req_mgr_workq_enqueue_task(task, NULL, CRM_TASK_PRIORITY_0);
	if (rc) {
		CAM_ERR(CAM_RPMSG, "failed to enqueue task rc %d", rc);
		goto err_exit1;
	}

	return rc;

err_exit1:
	kfree(payload);
	return rc;
}

int32_t cam_sensor_lite_init_module(void)
{

	struct cam_rpmsg_slave_cbs sensor_lite_rpmsg_cb;

	cam_req_mgr_workq_create("cam_rpmsg_sensor_wq",
			CAM_SENSOR_LITE_RPMSG_WORKQ_NUM_TASK,
			&(sensor_lite_rpmsg_workq), CRM_WORKQ_USAGE_IRQ,
			CAM_WORKQ_FLAG_HIGH_PRIORITY);

	sensor_lite_rpmsg_cb.cookie = NULL;
	sensor_lite_rpmsg_cb.recv   = sensor_lite_recv_irq_cb;
	cam_rpmsg_subscribe_slave_callback(CAM_RPMSG_SLAVE_CLIENT_SENSOR,
			sensor_lite_rpmsg_cb);
	return platform_driver_register(&cam_sensor_lite_driver);
}

void cam_sensor_lite_exit_module(void)
{
	platform_driver_unregister(&cam_sensor_lite_driver);
}

MODULE_DESCRIPTION("CAM Sensor Lite driver");
MODULE_LICENSE("GPL v2");
