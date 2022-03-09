/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_CSIPHY_H
#define MSM_CSIPHY_H

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <media/msm_cam_sensor.h>
#include "msm_sd.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"
#include "cam_soc_api.h"

#define MAX_CSIPHY 3
#define CSIPHY_NUM_CLK_MAX  16
#define MAX_CSIPHY_SETTINGS 120

struct csiphy_reg_t {
	uint32_t addr;
	uint32_t data;
};

struct csiphy_settings_t {
	struct csiphy_reg_t settings[MAX_CSIPHY_SETTINGS];
};

struct csiphy_reg_parms_t {
/*MIPI CSI PHY registers*/
	uint32_t mipi_csiphy_lnn_cfg1_addr;
	uint32_t mipi_csiphy_lnn_cfg2_addr;
	uint32_t mipi_csiphy_lnn_cfg3_addr;
	uint32_t mipi_csiphy_lnn_cfg4_addr;
	uint32_t mipi_csiphy_lnn_cfg5_addr;
	uint32_t mipi_csiphy_lnck_cfg1_addr;
	uint32_t mipi_csiphy_lnck_cfg2_addr;
	uint32_t mipi_csiphy_lnck_cfg3_addr;
	uint32_t mipi_csiphy_lnck_cfg4_addr;
	uint32_t mipi_csiphy_lnn_test_imp;
	uint32_t mipi_csiphy_lnn_misc1_addr;
	uint32_t mipi_csiphy_glbl_reset_addr;
	uint32_t mipi_csiphy_glbl_pwr_cfg_addr;
	uint32_t mipi_csiphy_glbl_irq_cmd_addr;
	uint32_t mipi_csiphy_hw_version_addr;
	uint32_t mipi_csiphy_interrupt_status0_addr;
	uint32_t mipi_csiphy_interrupt_mask0_addr;
	uint32_t mipi_csiphy_interrupt_mask_val;
	uint32_t mipi_csiphy_interrupt_mask_addr;
	uint32_t mipi_csiphy_interrupt_clear0_addr;
	uint32_t mipi_csiphy_interrupt_clear_addr;
	uint32_t mipi_csiphy_mode_config_shift;
	uint32_t mipi_csiphy_glbl_t_init_cfg0_addr;
	uint32_t mipi_csiphy_t_wakeup_cfg0_addr;
	uint32_t csiphy_version;
	uint32_t combo_clk_mask;
	uint32_t mipi_csiphy_interrupt_clk_status0_addr;
	uint32_t mipi_csiphy_interrupt_clk_clear0_addr;
};

struct csiphy_reg_snps_parms_t {
	/*MIPI CSI PHY registers*/
	struct csiphy_reg_t mipi_csiphy_sys_ctrl;
	struct csiphy_reg_t mipi_csiphy_sys_ctrl_1;
	struct csiphy_reg_t mipi_csiphy_ctrl_1;
	struct csiphy_reg_t mipi_csiphy_ctrl_2;
	struct csiphy_reg_t mipi_csiphy_ctrl_3;
	struct csiphy_reg_t mipi_csiphy_fifo_ctrl;
	struct csiphy_reg_t mipi_csiphy_enable;
	struct csiphy_reg_t mipi_csiphy_basedir;
	struct csiphy_reg_t mipi_csiphy_force_mode;
	struct csiphy_reg_t mipi_csiphy_enable_clk;
	struct csiphy_reg_t mipi_csiphy_irq_mask_ctrl_lane_0;
	struct csiphy_reg_t mipi_csiphy_irq_mask_ctrl_lane_clk_0;
	struct csiphy_reg_t mipi_csiphy_rx_sys_7_00;
	struct csiphy_reg_t mipi_csiphy_rx_startup_ovr_1_00;
	struct csiphy_reg_t mipi_csiphy_rx_startup_ovr_2_00;
	struct csiphy_reg_t mipi_csiphy_rx_startup_ovr_3_00;
	struct csiphy_reg_t mipi_csiphy_rx_startup_ovr_4_00;
	struct csiphy_reg_t mipi_csiphy_rx_startup_ovr_5_00;
	struct csiphy_reg_t mipi_csiphy_rx_startup_obs_2_00;
	struct csiphy_reg_t mipi_csiphy_rx_cb_2_00;
	struct csiphy_reg_t mipi_csiphy_rx_dual_phy_0_00;
	struct csiphy_reg_t mipi_csiphy_rx_clk_lane_3_00;
	struct csiphy_reg_t mipi_csiphy_rx_clk_lane_4_00;
	struct csiphy_reg_t mipi_csiphy_rx_clk_lane_6_00;
	struct csiphy_reg_t mipi_csiphy_rx_lane_0_7_00;
	struct csiphy_reg_t mipi_csiphy_rx_lane_1_7_00;
	struct csiphy_reg_t mipi_csiphy_rx_clk_lane_7_00;
	struct csiphy_reg_t mipi_csiphy_rx_lane0_ddl_2_00;
	struct csiphy_reg_t mipi_csiphy_rx_lane0_ddl_3_00;
	struct csiphy_reg_t mipi_csiphy_rx_lane_1_10_00;
	struct csiphy_reg_t mipi_csiphy_rx_lane_1_11_00;
};

struct csiphy_reg_3ph_parms_t {
/*MIPI CSI PHY registers*/
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl5;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl6;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl34;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl35;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl36;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl1;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl2;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl3;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl5;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl6;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl7;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl8;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl9;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl10;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl11;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl12;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl13;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl14;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl15;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl16;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl17;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl18;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl19;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl21;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl23;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl24;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl25;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl26;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl27;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl28;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl29;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl30;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl31;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl32;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl33;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl51;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl7;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl11;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl12;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl13;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl14;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl15;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl16;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl17;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl18;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl19;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl20;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl21;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_misc1;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl0;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg1;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg2;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg3;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg4;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg5;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg6;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg7;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg8;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_cfg9;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl15;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_test_imp;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_test_force;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl5;
	struct csiphy_reg_t mipi_csiphy_3ph_lnck_cfg1;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl20;
	struct csiphy_reg_t mipi_csiphy_3ph_lnn_ctrl55;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl11;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl13;
	struct csiphy_reg_t mipi_csiphy_2ph_lnck_ctrl10;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl0;
	struct csiphy_reg_t mipi_csiphy_2ph_lnck_ctrl3;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl14;
	struct csiphy_reg_t mipi_csiphy_3ph_cmn_ctrl7_cphy;
	struct csiphy_reg_t mipi_csiphy_2ph_lnck_ctrl0;
	struct csiphy_reg_t mipi_csiphy_2ph_lnck_ctrl9;
	struct csiphy_reg_t mipi_csiphy_2ph_lnn_ctrl9;
};

struct csiphy_ctrl_t {
	struct csiphy_reg_parms_t csiphy_reg;
	struct csiphy_reg_3ph_parms_t csiphy_3ph_reg;
	struct csiphy_settings_t csiphy_combo_mode_settings;
	struct csiphy_reg_snps_parms_t csiphy_snps_reg;
};

enum msm_csiphy_state_t {
	CSIPHY_POWER_UP,
	CSIPHY_POWER_DOWN,
};

enum snps_csiphy_mode {
	AGGREGATE_MODE,
	TWO_LANE_PHY_A,
	TWO_LANE_PHY_B,
	INVALID_MODE,
};

enum snps_csiphy_state {
	NOT_CONFIGURED,
	CONFIGURED_AGGREGATE_MODE,
	CONFIGURED_TWO_LANE_PHY_A,
	CONFIGURED_TWO_LANE_PHY_B,
	CONFIGURED_COMBO_MODE,
};

struct snps_freq_value {
	uint32_t default_bit_rate;
	uint8_t hs_freq;
	uint16_t osc_freq;
};

struct csiphy_device {
	struct platform_device *pdev;
	struct msm_sd_subdev msm_sd;
	struct v4l2_subdev subdev;
	struct resource *irq;
	void __iomem *base;
	void __iomem *clk_mux_base;
	struct mutex mutex;
	uint32_t hw_version;
	uint32_t hw_dts_version;
	enum msm_csiphy_state_t csiphy_state;
	struct csiphy_ctrl_t *ctrl_reg;
	size_t num_all_clk;
	struct clk **csiphy_all_clk;
	struct msm_cam_clk_info *csiphy_all_clk_info;
	uint32_t num_clk;
	struct clk *csiphy_clk[CSIPHY_NUM_CLK_MAX];
	struct msm_cam_clk_info csiphy_clk_info[CSIPHY_NUM_CLK_MAX];
	struct clk *csiphy_3p_clk[2];
	struct msm_cam_clk_info csiphy_3p_clk_info[2];
	unsigned char csi_3phase;
	int32_t ref_count;
	uint16_t lane_mask[MAX_CSIPHY];
	uint32_t is_3_1_20nm_hw;
	uint32_t csiphy_clk_index;
	uint32_t csiphy_max_clk;
	uint8_t csiphy_3phase;
	uint8_t num_irq_registers;
	uint32_t csiphy_sof_debug;
	uint32_t csiphy_sof_debug_count;
	struct camera_vreg_t *csiphy_vreg;
	struct regulator *csiphy_reg_ptr[MAX_REGULATOR];
	int32_t regulator_count;
	uint8_t is_snps_phy;
	enum snps_csiphy_state snps_state;
	uint8_t num_clk_irq_registers;
	uint64_t snps_programmed_data_rate;
};

#define VIDIOC_MSM_CSIPHY_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, void *)
#endif
