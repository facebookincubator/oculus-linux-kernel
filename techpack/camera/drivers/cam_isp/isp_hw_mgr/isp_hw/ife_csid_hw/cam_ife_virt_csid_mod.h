/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_IFE_VIRT_CSID_HW_H_
#define _CAM_IFE_VIRT_CSID_HW_H_

#include "cam_ife_csid_common.h"

struct cam_ife_virt_csid {
	struct cam_hw_intf                    *hw_intf;
	struct cam_hw_info                    *hw_info;
	struct cam_ife_csid_core_info         *core_info;
	struct cam_ife_csid_cid_data           cid_data[CAM_IFE_CSID_CID_MAX];
	struct cam_ife_csid_hw_flags           flags;
	struct cam_ife_csid_rx_cfg             rx_cfg;
	struct cam_isp_resource_node           path_res[CAM_IFE_PIX_PATH_RES_MAX];
};

int cam_ife_virt_csid_init_module(void);
void cam_ife_virt_csid_exit_module(void);

#endif
