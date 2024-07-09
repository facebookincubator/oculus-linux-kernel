/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CSID_PPI_100_H_
#define _CAM_CSID_PPI_100_H_

#include "cam_csid_ppi_core.h"

static struct cam_csid_ppi_reg_offset cam_csid_ppi_100_reg_offset = {
	.ppi_hw_version_addr    = 0,
	.ppi_module_cfg_addr    = 0x60,
	.ppi_irq_status_addr    = 0x68,
	.ppi_irq_mask_addr      = 0x6c,
	.ppi_irq_set_addr       = 0x70,
	.ppi_irq_clear_addr     = 0x74,
	.ppi_irq_cmd_addr       = 0x78,
	.ppi_rst_cmd_addr       = 0x7c,
	.ppi_test_bus_ctrl_addr = 0x1f4,
	.ppi_debug_addr         = 0x1f8,
	.ppi_spare_addr         = 0x1fc,
};

#endif /*_CAM_CSID_PPI_100_H_ */
