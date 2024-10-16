/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_SENSOR_LITE_DEV_H_
#define _CAM_SENSOR_LITE_DEV_H_

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/irqreturn.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_defs.h>
#include <cam_sensor_lite_ext_headers.h>
#include <cam_req_mgr_interface.h>
#include <cam_subdev.h>
#include <cam_io_util.h>
#include <cam_cpas_api.h>
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_context.h"

#define CAMX_SENOSR_LITE_DEV_NAME "cam-sensor-lite-driver"

#define MAX_PAYLOAD_CMDS 10

/**
 * struct sensor_lite_crm_intf_params
 * @device_hdl: Device Handle
 * @session_hdl: Session Handle
 * @link_hdl: Link Handle
 */
struct sensor_lite_crm_intf_params {
	int32_t device_hdl;
	int32_t session_hdl;
	int32_t link_hdl;
	struct cam_req_mgr_kmd_ops ops;
	struct cam_req_mgr_no_crm_kmd_ops no_crm_ops;
	struct cam_req_mgr_crm_cb *crm_cb;
	cam_req_mgr_no_crm_frame_skip_notify  frame_skip_cb;
	uint32_t enable_crm;
};

enum cam_sensor_lite_state {
	CAM_SENSOR_LITE_STATE_INIT,
	CAM_SENSOR_LITE_STATE_ACQUIRE,
	CAM_SENSOR_LITE_STATE_START,
	CAM_SENSOR_LITE_STATE_STATE_MAX
};

struct sensor_lite_request {
	uint64_t request_id;
	uint32_t type;

	/* This is allocated based on the request type*/
	void     *payload[MAX_PAYLOAD_CMDS];
	uint32_t num_cmds;

	/* Add members as needed */
	struct list_head  list;
};

struct sensor_lite_device {
	char                                  device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct mutex                          mutex;
	uint32_t                              hw_version;
	struct cam_subdev                     v4l2_dev_str;
	struct cam_hw_soc_info                soc_info;
	struct cam_req_mgr_kmd_ops            ops;
	struct cam_req_mgr_crm_cb             *crm_cb;
	int32_t                               ref_count;
	int                                   state;
	struct sensor_lite_crm_intf_params    crm_intf;
	uint32_t                              probe_success;

	/* allocate these during probe and free during shutdown*/
	struct sensor_lite_acquire_cmd        *acquire_cmd;
	struct sensor_lite_release_cmd        *release_cmd;

	/* allocate when packet is received and free during rlease shutodown */
	struct sensor_lite_start_stop_cmd     *start_cmd;
	struct sensor_lite_start_stop_cmd     *stop_cmd;

	struct completion                     complete;
	struct cam_req_mgr_core_worker        *worker;
	struct sensor_probe_response          probe_info;
	uint32_t                              phy_id;
	uint32_t                              dump_en;
	uint32_t                              type;
	bool                                  hw_no_ops;
	bool                                  is_trigger_mode;
	int                                   anchor_pd;

	/* Request Queue */
	struct list_head waiting_request_q;
	struct list_head applied_request_q;
	int    applied_request_q_depth;
	int    waiting_request_q_depth;
};

/**
 * @brief : API to register sensor lite hw to platform framework.
 * @return struct platform_device pointer on success, or ERR_PTR() on error.
 */
int32_t cam_sensor_lite_init_module(void);

/**
 * @brief : API to remove senor lite Hw from platform framework.
 */
void cam_sensor_lite_exit_module(void);
#endif /* _CAM_SENSOR_LITE_DEV_H_ */
