/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_CSIPHY_REMOTE_DEV_H_
#define _CAM_CSIPHY_REMOTE_DEV_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_defs.h>
#include <cam_sensor_lite_ext_headers.h>
#include <cam_req_mgr_interface.h>
#include <cam_subdev.h>
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_context.h"

#define MAX_LANES                   5
#define MAX_SETTINGS_PER_LANE       50

#define CAMX_CSIPHY_REMOTE_DEV_NAME "cam-csiphy-remote-driver"

#define CSIPHY_REMOTE_DEFAULT_PARAMS            0
#define CSIPHY_REMOTE_LANE_ENABLE               1
#define CSIPHY_REMOTE_DNP_PARAMS                2
#define CSIPHY_REMOTE_2PH_REGS                  3
#define CSIPHY_REMOTE_3PH_REGS                  4
#define CSIPHY_REMOTE_2PH_COMBO_REGS            5
#define CSIPHY_REMOTE_3PH_COMBO_REGS            6

#define CSIPHY_MAX_INSTANCES_PER_PHY     3

#define CAM_CSIPHY_MAX_DPHY_LANES    4
#define CAM_CSIPHY_MAX_CPHY_LANES    3

#define DPHY_LANE_0    BIT(0)
#define CPHY_LANE_0    BIT(1)
#define DPHY_LANE_1    BIT(2)
#define CPHY_LANE_1    BIT(3)
#define DPHY_LANE_2    BIT(4)
#define CPHY_LANE_2    BIT(5)
#define DPHY_LANE_3    BIT(6)
#define DPHY_CLK_LN    BIT(7)

enum cam_csiphy_remote_state {
	CAM_CSIPHY_REMOTE_INIT,
	CAM_CSIPHY_REMOTE_ACQUIRE,
	CAM_CSIPHY_REMOTE_START,
};

/**
 * struct csiphy_remote_reg_parms_t
 * @csiphy_common_array_size          : CSIPhy common array size
 * @csiphy_reset_array_size           : CSIPhy reset array size
 * @csiphy_2ph_config_array_size      : 2ph settings size
 * @csiphy_3ph_config_array_size      : 3ph settings size
 */
struct csiphy_remote_reg_parms_t {
/*MIPI CSI PHY registers*/
	uint32_t csiphy_version;
	uint32_t csiphy_common_array_size;
	uint32_t csiphy_reset_array_size;
	uint32_t csiphy_2ph_config_array_size;
	uint32_t csiphy_3ph_config_array_size;
};

/**
 * struct csiphy_remote_hdl_tbl
 * @device_hdl     : Device Handle
 * @session_hdl    : Session Handle
 */
struct csiphy_remote_hdl_tbl {
	int32_t device_hdl;
	int32_t session_hdl;
};

/**
 * struct csiphy_remote_reg_t
 * @reg_addr              : Register address
 * @reg_data              : Register data
 * @delay                 : Delay in us
 * @csiphy_param_type     : CSIPhy parameter type
 */
struct csiphy_remote_reg_t {
	int32_t  reg_addr;
	int32_t  reg_data;
	int32_t  delay;
	uint32_t csiphy_param_type;
};

struct csiphy_remote_device;

/**
 * struct csiphy_remote_ctrl_t
 * @csiphy_reg                : Register address
 * @csiphy_common_reg         : Common register set
 * @csiphy_reset_regs         : Reset registers
 * @csiphy_2ph_reg            : 2phase register set
 * @csiphy_2ph_combo_mode_reg : 2phase combo register set
 * @csiphy_3ph_reg            : 3phase register set
 */
struct csiphy_remote_ctrl_t {
	struct csiphy_remote_reg_parms_t csiphy_reg;
	struct csiphy_remote_reg_t *csiphy_common_reg;
	struct csiphy_remote_reg_t *csiphy_reset_regs;
	struct csiphy_remote_reg_t (*csiphy_2ph_reg)[MAX_SETTINGS_PER_LANE];
	struct csiphy_remote_reg_t (*csiphy_2ph_combo_mode_reg)[MAX_SETTINGS_PER_LANE];
	struct csiphy_remote_reg_t (*csiphy_3ph_reg)[MAX_SETTINGS_PER_LANE];
	struct csiphy_remote_reg_t (*csiphy_3ph_combo_reg)[MAX_SETTINGS_PER_LANE];
};

/**
 * cam_csiphy_remote_param
 * @hdl_data                   :  CSIPHY handle table
 * @phy_id                     :  Phy ID
 * @sensor_physical_id         :  Sensor physical ID
 */
struct cam_csiphy_remote_param {
	struct csiphy_remote_hdl_tbl      hdl_data;
	uint32_t                          phy_id;
	uint32_t                          sensor_physical_id;
};

/**
 * struct csiphy_remote_device
 * @hw_version                 : Hardware Version
 * @device_name                : Device name
 * @mutex                      : ioctl operation mutex
 * @acquire_count              : Acquire device count
 * @session_max_device_support : Max number of devices supported in a session
 * @csiphy_state               : HW state
 * @ctrl_reg                   : CSIPhy control registers
 * @v4l2_dev_str               : V4L2 related data
 * @csiphy_info                : Sensor-specific data
 * @soc_info                   : SOC information
 * @ops                        : KMD operations
 * @crm_cb                     : Callback API pointers
 */
struct csiphy_remote_device {
	uint32_t                       hw_version;
	char                           device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct mutex                   mutex;
	uint32_t                       acquire_count;
	uint32_t                       start_dev_count;
	enum cam_csiphy_remote_state   csiphy_state;
	struct csiphy_remote_ctrl_t    *ctrl_reg;
	struct cam_subdev              v4l2_dev_str;
	struct cam_csiphy_remote_param csiphy_info[CSIPHY_MAX_INSTANCES_PER_PHY];
	struct cam_hw_soc_info         soc_info;
	struct cam_req_mgr_kmd_ops     ops;
	struct cam_req_mgr_crm_cb      *crm_cb;
};

/**
 * @brief : API to register CSIPHY hw to platform framework.
 * @return struct platform_device pointer on success, or ERR_PTR() on error.
 */
int32_t cam_csiphy_remote_init_module(void);

/**
 * @brief : API to remove CSIPHY Hw from platform framework.
 */
void cam_csiphy_remote_exit_module(void);
#endif /* _CAM_CSIPHY_REMOTE_DEV_H_ */
