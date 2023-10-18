/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_LITE_636_H_
#define _CAM_IFE_CSID_LITE_636_H_

#include "cam_ife_csid_common.h"
#include "cam_ife_csid_dev.h"
#include "cam_ife_csid_hw_ver2.h"
#include "cam_irq_controller.h"
#include "cam_isp_hw_mgr_intf.h"

static struct cam_ife_csid_ver2_reg_info cam_ife_csid_lite_636_reg_info = {
	.irq_reg_info                         = &cam_ife_csid_lite_650_irq_reg_info,
	.cmn_reg                              = &cam_ife_csid_lite_650_cmn_reg_info,
	.csi2_reg                             = &cam_ife_csid_lite_650_csi2_reg_info,
	.buf_done_irq_reg_info                =
		&cam_ife_csid_lite_650_buf_done_irq_reg_info,
	.path_reg[CAM_IFE_PIX_PATH_RES_IPP]   = &cam_ife_csid_lite_650_ipp_reg_info,
	.path_reg[CAM_IFE_PIX_PATH_RES_PPP]   = NULL,
	.path_reg[CAM_IFE_PIX_PATH_RES_RDI_0] = &cam_ife_csid_lite_650_rdi_0_reg_info,
	.path_reg[CAM_IFE_PIX_PATH_RES_RDI_1] = &cam_ife_csid_lite_650_rdi_1_reg_info,
	.path_reg[CAM_IFE_PIX_PATH_RES_RDI_2] = &cam_ife_csid_lite_650_rdi_2_reg_info,
	.path_reg[CAM_IFE_PIX_PATH_RES_RDI_3] = &cam_ife_csid_lite_650_rdi_3_reg_info,
	.need_top_cfg = 0,
	.rx_irq_desc        = cam_ife_csid_lite_650_rx_irq_desc,
	.path_irq_desc      = cam_ife_csid_lite_650_path_irq_desc,
};
#endif /* _CAM_IFE_CSID_LITE_636_H_ */
