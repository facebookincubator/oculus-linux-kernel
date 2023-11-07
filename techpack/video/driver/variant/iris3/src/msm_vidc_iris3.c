// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "msm_vidc_iris3.h"
#include "msm_vidc_buffer_iris3.h"
#include "msm_vidc_power_iris3.h"
#include "venus_hfi.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_core.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_control.h"
#include "msm_vidc_dt.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_buffer.h"
#include "msm_vidc_debug.h"

#define VIDEO_ARCH_LX 1

#define VCODEC_BASE_OFFS_IRIS3                 0x00000000
#define AON_MVP_NOC_RESET                      0x0001F000
#define CPU_BASE_OFFS_IRIS3                    0x000A0000
#define AON_BASE_OFFS			               0x000E0000
#define CPU_CS_BASE_OFFS_IRIS3		           (CPU_BASE_OFFS_IRIS3)
#define CPU_IC_BASE_OFFS_IRIS3		           (CPU_BASE_OFFS_IRIS3)

#define CPU_CS_A2HSOFTINTCLR_IRIS3             (CPU_CS_BASE_OFFS_IRIS3 + 0x1C)
#define CPU_CS_VCICMD_IRIS3                    (CPU_CS_BASE_OFFS_IRIS3 + 0x20)
#define CPU_CS_VCICMDARG0_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x24)
#define CPU_CS_VCICMDARG1_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x28)
#define CPU_CS_VCICMDARG2_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x2C)
#define CPU_CS_VCICMDARG3_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x30)
#define CPU_CS_VMIMSG_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x34)
#define CPU_CS_VMIMSGAG0_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x38)
#define CPU_CS_VMIMSGAG1_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x3C)
#define CPU_CS_SCIACMD_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x48)
#define CPU_CS_H2XSOFTINTEN_IRIS3	(CPU_CS_BASE_OFFS_IRIS3 + 0x148)

/* HFI_CTRL_STATUS */
#define CPU_CS_SCIACMDARG0_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x4C)
#define CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK_IRIS3	0xfe
#define CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY_IRIS3           0x100
#define CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK_IRIS3     0x40000000

/* HFI_QTBL_INFO */
#define CPU_CS_SCIACMDARG1_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x50)

/* HFI_QTBL_ADDR */
#define CPU_CS_SCIACMDARG2_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x54)

/* HFI_VERSION_INFO */
#define CPU_CS_SCIACMDARG3_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x58)

/* SFR_ADDR */
#define CPU_CS_SCIBCMD_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x5C)

/* MMAP_ADDR */
#define CPU_CS_SCIBCMDARG0_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x60)

/* UC_REGION_ADDR */
#define CPU_CS_SCIBARG1_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x64)

/* UC_REGION_ADDR */
#define CPU_CS_SCIBARG2_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x68)

#define CPU_CS_AHB_BRIDGE_SYNC_RESET            (CPU_CS_BASE_OFFS_IRIS3 + 0x160)
#define CPU_CS_AHB_BRIDGE_SYNC_RESET_STATUS     (CPU_CS_BASE_OFFS_IRIS3 + 0x164)

/* FAL10 Feature Control */
#define CPU_CS_X2RPMh_IRIS3		(CPU_CS_BASE_OFFS_IRIS3 + 0x168)
#define CPU_CS_X2RPMh_MASK0_BMSK_IRIS3	0x1
#define CPU_CS_X2RPMh_MASK0_SHFT_IRIS3	0x0
#define CPU_CS_X2RPMh_MASK1_BMSK_IRIS3	0x2
#define CPU_CS_X2RPMh_MASK1_SHFT_IRIS3	0x1
#define CPU_CS_X2RPMh_SWOVERRIDE_BMSK_IRIS3	0x4
#define CPU_CS_X2RPMh_SWOVERRIDE_SHFT_IRIS3	0x3

#define CPU_IC_SOFTINT_IRIS3		(CPU_IC_BASE_OFFS_IRIS3 + 0x150)
#define CPU_IC_SOFTINT_H2A_SHFT_IRIS3	0x0

/*
 * --------------------------------------------------------------------------
 * MODULE: AON_MVP_NOC_RESET_REGISTERS
 * --------------------------------------------------------------------------
 */
#define AON_WRAPPER_MVP_NOC_RESET_REQ   (AON_MVP_NOC_RESET + 0x000)
#define AON_WRAPPER_MVP_NOC_RESET_ACK   (AON_MVP_NOC_RESET + 0x004)

/*
 * --------------------------------------------------------------------------
 * MODULE: wrapper
 * --------------------------------------------------------------------------
 */
#define WRAPPER_BASE_OFFS_IRIS3		0x000B0000
#define WRAPPER_INTR_STATUS_IRIS3	(WRAPPER_BASE_OFFS_IRIS3 + 0x0C)
#define WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS3	0x8
#define WRAPPER_INTR_STATUS_A2H_BMSK_IRIS3	0x4

#define WRAPPER_INTR_MASK_IRIS3		(WRAPPER_BASE_OFFS_IRIS3 + 0x10)
#define WRAPPER_INTR_MASK_A2HWD_BMSK_IRIS3	0x8
#define WRAPPER_INTR_MASK_A2HCPU_BMSK_IRIS3	0x4

#define WRAPPER_CPU_CLOCK_CONFIG_IRIS3	(WRAPPER_BASE_OFFS_IRIS3 + 0x2000)
#define WRAPPER_CPU_CGC_DIS_IRIS3	(WRAPPER_BASE_OFFS_IRIS3 + 0x2010)
#define WRAPPER_CPU_STATUS_IRIS3	(WRAPPER_BASE_OFFS_IRIS3 + 0x2014)

#define WRAPPER_DEBUG_BRIDGE_LPI_CONTROL_IRIS3	(WRAPPER_BASE_OFFS_IRIS3 + 0x54)
#define WRAPPER_DEBUG_BRIDGE_LPI_STATUS_IRIS3	(WRAPPER_BASE_OFFS_IRIS3 + 0x58)
#define WRAPPER_IRIS_CPU_NOC_LPI_CONTROL	(WRAPPER_BASE_OFFS_IRIS3 + 0x5C)
#define WRAPPER_IRIS_CPU_NOC_LPI_STATUS		(WRAPPER_BASE_OFFS_IRIS3 + 0x60)
#define WRAPPER_CORE_POWER_STATUS		(WRAPPER_BASE_OFFS_IRIS3 + 0x80)
#define WRAPPER_CORE_CLOCK_CONFIG_IRIS3		(WRAPPER_BASE_OFFS_IRIS3 + 0x88)

/*
 * --------------------------------------------------------------------------
 * MODULE: tz_wrapper
 * --------------------------------------------------------------------------
 */
#define WRAPPER_TZ_BASE_OFFS	0x000C0000
#define WRAPPER_TZ_CPU_CLOCK_CONFIG	(WRAPPER_TZ_BASE_OFFS)
#define WRAPPER_TZ_CPU_STATUS	(WRAPPER_TZ_BASE_OFFS + 0x10)
#define WRAPPER_TZ_CTL_AXI_CLOCK_CONFIG	(WRAPPER_TZ_BASE_OFFS + 0x14)
#define WRAPPER_TZ_QNS4PDXFIFO_RESET	(WRAPPER_TZ_BASE_OFFS + 0x18)

#define CTRL_INIT_IRIS3		CPU_CS_SCIACMD_IRIS3

#define CTRL_STATUS_IRIS3	CPU_CS_SCIACMDARG0_IRIS3
#define CTRL_ERROR_STATUS__M_IRIS3 \
		CPU_CS_SCIACMDARG0_HFI_CTRL_ERROR_STATUS_BMSK_IRIS3
#define CTRL_INIT_IDLE_MSG_BMSK_IRIS3 \
		CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK_IRIS3
#define CTRL_STATUS_PC_READY_IRIS3 \
		CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY_IRIS3


#define QTBL_INFO_IRIS3		CPU_CS_SCIACMDARG1_IRIS3

#define QTBL_ADDR_IRIS3		CPU_CS_SCIACMDARG2_IRIS3

#define VERSION_INFO_IRIS3	    CPU_CS_SCIACMDARG3_IRIS3

#define SFR_ADDR_IRIS3		    CPU_CS_SCIBCMD_IRIS3
#define MMAP_ADDR_IRIS3		CPU_CS_SCIBCMDARG0_IRIS3
#define UC_REGION_ADDR_IRIS3	CPU_CS_SCIBARG1_IRIS3
#define UC_REGION_SIZE_IRIS3	CPU_CS_SCIBARG2_IRIS3

#define AON_WRAPPER_MVP_NOC_LPI_CONTROL	(AON_BASE_OFFS)
#define AON_WRAPPER_MVP_NOC_LPI_STATUS	(AON_BASE_OFFS + 0x4)

/*
 * --------------------------------------------------------------------------
 * MODULE: VCODEC_SS registers
 * --------------------------------------------------------------------------
 */
#define VCODEC_SS_IDLE_STATUSn           (VCODEC_BASE_OFFS_IRIS3 + 0x70)

/*
 * --------------------------------------------------------------------------
 * MODULE: vcodec noc error log registers (iris3)
 * --------------------------------------------------------------------------
 */
#define VCODEC_NOC_VIDEO_A_NOC_BASE_OFFS		0x00010000
#define VCODEC_NOC_ERL_MAIN_SWID_LOW			0x00011200
#define VCODEC_NOC_ERL_MAIN_SWID_HIGH			0x00011204
#define VCODEC_NOC_ERL_MAIN_MAINCTL_LOW			0x00011208
#define VCODEC_NOC_ERL_MAIN_ERRVLD_LOW			0x00011210
#define VCODEC_NOC_ERL_MAIN_ERRCLR_LOW			0x00011218
#define VCODEC_NOC_ERL_MAIN_ERRLOG0_LOW			0x00011220
#define VCODEC_NOC_ERL_MAIN_ERRLOG0_HIGH		0x00011224
#define VCODEC_NOC_ERL_MAIN_ERRLOG1_LOW			0x00011228
#define VCODEC_NOC_ERL_MAIN_ERRLOG1_HIGH		0x0001122C
#define VCODEC_NOC_ERL_MAIN_ERRLOG2_LOW			0x00011230
#define VCODEC_NOC_ERL_MAIN_ERRLOG2_HIGH		0x00011234
#define VCODEC_NOC_ERL_MAIN_ERRLOG3_LOW			0x00011238
#define VCODEC_NOC_ERL_MAIN_ERRLOG3_HIGH		0x0001123C


static int __disable_unprepare_clock_iris3(struct msm_vidc_core *core,
		const char *clk_name)
{
	int rc = 0;
	struct clock_info *cl;
	bool found;

	if (!core || !clk_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_clock(core, cl) {
		if (!cl->clk) {
			d_vpr_e("%s: invalid clock %s\n", __func__, cl->name);
			return -EINVAL;
		}
		if (strcmp(cl->name, clk_name))
			continue;
		found = true;
		clk_disable_unprepare(cl->clk);
		if (cl->has_scaling)
			__set_clk_rate(core, cl, 0);
		cl->prev = 0;
		d_vpr_h("%s: clock %s disable unprepared\n", __func__, cl->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: clock %s not found\n", __func__, clk_name);
		return -EINVAL;
	}

	return rc;
}

static int __prepare_enable_clock_iris3(struct msm_vidc_core *core,
		const char *clk_name)
{
	int rc = 0, src_clk_scale_ratio = 1;
	struct clock_info *cl;
	bool found;
	u64 rate = 0;

	if (!core || !clk_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_clock(core, cl) {
		if (!cl->clk) {
			d_vpr_e("%s: invalid clock\n", __func__);
			return -EINVAL;
		}
		if (strcmp(cl->name, clk_name))
			continue;
		found = true;
		/*
		 * For the clocks we control, set the rate prior to preparing
		 * them.  Since we don't really have a load at this point, scale
		 * it to the lowest frequency possible
		 */
		if (cl->has_scaling) {
			rate = clk_round_rate(cl->clk, 0);
			/**
			 * source clock is already multipled with scaling ratio and __set_clk_rate
			 * attempts to multiply again. So divide scaling ratio before calling
			 * __set_clk_rate.
			 */
			src_clk_scale_ratio = msm_vidc_get_src_clk_scaling_ratio(core);
			rate = rate / src_clk_scale_ratio;
			__set_clk_rate(core, cl, rate);
		}

		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			d_vpr_e("%s: failed to enable clock %s\n",
				__func__, cl->name);
			return rc;
		}
		if (!__clk_is_enabled(cl->clk)) {
			d_vpr_e("%s: clock %s not enabled\n",
				__func__, cl->name);
			clk_disable_unprepare(cl->clk);
			if (cl->has_scaling)
				__set_clk_rate(core, cl, 0);
			return -EINVAL;
		}
		d_vpr_h("%s: clock %s prepare enabled\n", __func__, cl->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: clock %s not found\n", __func__, clk_name);
		return -EINVAL;
	}

	return rc;
}

static int __disable_regulator_iris3(struct msm_vidc_core *core,
		const char *reg_name)
{
	int rc = 0;
	struct regulator_info *rinfo;
	bool found;

	if (!core || !reg_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_regulator(core, rinfo) {
		if (!rinfo->regulator) {
			d_vpr_e("%s: invalid regulator %s\n",
				__func__, rinfo->name);
			return -EINVAL;
		}
		if (strcmp(rinfo->name, reg_name))
			continue;
		found = true;

		rc = __acquire_regulator(core, rinfo);
		if (rc) {
			d_vpr_e("%s: failed to acquire %s, rc = %d\n",
				__func__, rinfo->name, rc);
			/* Bring attention to this issue */
			WARN_ON(true);
			return rc;
		}
		core->handoff_done = false;

		rc = regulator_disable(rinfo->regulator);
		if (rc) {
			d_vpr_e("%s: failed to disable %s, rc = %d\n",
				__func__, rinfo->name, rc);
			return rc;
		}
		d_vpr_h("%s: disabled regulator %s\n", __func__, rinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: regulator %s not found\n", __func__, reg_name);
		return -EINVAL;
	}

	return rc;
}

static int __enable_regulator_iris3(struct msm_vidc_core *core,
		const char *reg_name)
{
	int rc = 0;
	struct regulator_info *rinfo;
	bool found;

	if (!core || !reg_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_regulator(core, rinfo) {
		if (!rinfo->regulator) {
			d_vpr_e("%s: invalid regulator %s\n",
				__func__, rinfo->name);
			return -EINVAL;
		}
		if (strcmp(rinfo->name, reg_name))
			continue;
		found = true;

		rc = regulator_enable(rinfo->regulator);
		if (rc) {
			d_vpr_e("%s: failed to enable %s, rc = %d\n",
				__func__, rinfo->name, rc);
			return rc;
		}
		if (!regulator_is_enabled(rinfo->regulator)) {
			d_vpr_e("%s: regulator %s not enabled\n",
				__func__, rinfo->name);
			regulator_disable(rinfo->regulator);
			return -EINVAL;
		}
		d_vpr_h("%s: enabled regulator %s\n", __func__, rinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: regulator %s not found\n", __func__, reg_name);
		return -EINVAL;
	}

	return rc;
}

static int __interrupt_init_iris3(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	u32 mask_val = 0;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* All interrupts should be disabled initially 0x1F6 : Reset value */
	rc = __read_register(core, WRAPPER_INTR_MASK_IRIS3, &mask_val);
	if (rc)
		return rc;

	/* Write 0 to unmask CPU and WD interrupts */
	mask_val &= ~(WRAPPER_INTR_MASK_A2HWD_BMSK_IRIS3|
			WRAPPER_INTR_MASK_A2HCPU_BMSK_IRIS3);
	rc = __write_register(core, WRAPPER_INTR_MASK_IRIS3, mask_val);
	if (rc)
		return rc;

	return 0;
}

static int __setup_ucregion_memory_map_iris3(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	u32 value;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	value = (u32)core->iface_q_table.align_device_addr;
	rc = __write_register(core, UC_REGION_ADDR_IRIS3, value);
	if (rc)
		return rc;

	value = SHARED_QSIZE;
	rc = __write_register(core, UC_REGION_SIZE_IRIS3, value);
	if (rc)
		return rc;

	value = (u32)core->iface_q_table.align_device_addr;
	rc = __write_register(core, QTBL_ADDR_IRIS3, value);
	if (rc)
		return rc;

	rc = __write_register(core, QTBL_INFO_IRIS3, 0x01);
	if (rc)
		return rc;

	/* update queues vaddr for debug purpose */
	value = (u32)((u64)core->iface_q_table.align_virtual_addr);
	rc = __write_register(core, CPU_CS_VCICMDARG0_IRIS3, value);
	if (rc)
		return rc;

	value = (u32)((u64)core->iface_q_table.align_virtual_addr >> 32);
	rc = __write_register(core, CPU_CS_VCICMDARG1_IRIS3, value);
	if (rc)
		return rc;

	if (core->sfr.align_device_addr) {
		value = (u32)core->sfr.align_device_addr + VIDEO_ARCH_LX;
		rc = __write_register(core, SFR_ADDR_IRIS3, value);
		if (rc)
			return rc;
	}

	return 0;
}

static bool is_iris3_hw_power_collapsed(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 value = 0, pwr_status = 0;

	rc = __read_register(core, WRAPPER_CORE_POWER_STATUS, &value);
	if (rc)
		return false;

	/* if BIT(1) is 1 then video hw power is on else off */
	pwr_status = value & BIT(1);
	return pwr_status ? false : true;
}

static int __power_off_iris3_hardware(struct msm_vidc_core *core)
{
	int rc = 0, i;
	u32 value = 0;
	bool pwr_collapsed = false;

	/*
	 * Incase hw power control is enabled, for both CPU WD, video
	 * hw unresponsive cases, check for power status to decide on
	 * executing NOC reset sequence before disabling power. If there
	 * is no CPU WD and hw_power_control is enabled, fw is expected
	 * to power collapse video hw always.
	 */
	if (core->hw_power_control) {
		pwr_collapsed = is_iris3_hw_power_collapsed(core);
		if (core->cpu_watchdog || core->video_unresponsive) {
			if (pwr_collapsed) {
				d_vpr_e("%s: video hw power collapsed %d, %d\n",
					__func__, core->cpu_watchdog, core->video_unresponsive);
				goto disable_power;
			} else {
				d_vpr_e("%s: video hw is power ON %d, %d\n",
					__func__, core->cpu_watchdog, core->video_unresponsive);
			}
		} else {
			if (!pwr_collapsed)
				d_vpr_e("%s: video hw is not power collapsed\n", __func__);

			goto disable_power;
		}
	}

	/*
	 * check to make sure core clock branch enabled else
	 * we cannot read vcodec top idle register
	 */
	rc = __read_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS3, &value);
	if (rc)
		return rc;

	if (value) {
		d_vpr_h("%s: core clock config not enabled, enabling it to read vcodec registers\n",
			__func__);
		rc = __write_register(core, WRAPPER_CORE_CLOCK_CONFIG_IRIS3, 0);
		if (rc)
			return rc;
	}

	/*
	 * add MNoC idle check before collapsing MVS0 per HPG update
	 * poll for NoC DMA idle -> HPG 6.1.1
	 */
	for (i = 0; i < core->capabilities[NUM_VPP_PIPE].value; i++) {
		rc = __read_register_with_poll_timeout(core, VCODEC_SS_IDLE_STATUSn + 4*i,
				0x400000, 0x400000, 2000, 20000);
		if (rc)
			d_vpr_h("%s: VCODEC_SS_IDLE_STATUSn (%d) is not idle (%#x)\n",
				__func__, i, value);
	}

	/* Apply partial reset on MSF interface and wait for ACK */
	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ, 0x3);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK,
			0x3, 0x3, 200, 2000);
	if (rc)
		d_vpr_h("%s: AON_WRAPPER_MVP_NOC_RESET assert failed\n", __func__);

	/* De-assert partial reset on MSF interface and wait for ACK */
	rc = __write_register(core, AON_WRAPPER_MVP_NOC_RESET_REQ, 0x0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_RESET_ACK,
			0x3, 0x0, 200, 2000);
	if (rc)
		d_vpr_h("%s: AON_WRAPPER_MVP_NOC_RESET de-assert failed\n", __func__);

	/*
	 * Reset both sides of 2 ahb2ahb_bridges (TZ and non-TZ)
	 * do we need to check status register here?
	 */
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET, 0x3);
	if (rc)
		return rc;
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET, 0x2);
	if (rc)
		return rc;
	rc = __write_register(core, CPU_CS_AHB_BRIDGE_SYNC_RESET, 0x0);
	if (rc)
		return rc;

disable_power:
	/* power down process */
	rc = __disable_regulator_iris3(core, "vcodec");
	if (rc) {
		d_vpr_e("%s: disable regulator vcodec failed\n", __func__);
		rc = 0;
	}
	rc = __disable_unprepare_clock_iris3(core, "vcodec_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare vcodec_clk failed\n", __func__);
		rc = 0;
	}

	return rc;
}

static int __power_off_iris3_controller(struct msm_vidc_core *core)
{
	int rc = 0;

	/*
	 * mask fal10_veto QLPAC error since fal10_veto can go 1
	 * when pwwait == 0 and clamped to 0 -> HPG 6.1.2
	 */
	rc = __write_register(core, CPU_CS_X2RPMh_IRIS3, 0x3);
	if (rc)
		return rc;

	/* set MNoC to low power, set PD_NOC_QREQ (bit 0) */
	rc = __write_register_masked(core, AON_WRAPPER_MVP_NOC_LPI_CONTROL,
			0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, AON_WRAPPER_MVP_NOC_LPI_STATUS,
			0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_h("%s: AON_WRAPPER_MVP_NOC_LPI_CONTROL failed\n", __func__);

	/* Set Iris CPU NoC to Low power */
	rc = __write_register_masked(core, WRAPPER_IRIS_CPU_NOC_LPI_CONTROL,
			0x1, BIT(0));
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, WRAPPER_IRIS_CPU_NOC_LPI_STATUS,
			0x1, 0x1, 200, 2000);
	if (rc)
		d_vpr_h("%s: WRAPPER_IRIS_CPU_NOC_LPI_CONTROL failed\n", __func__);

	/* Debug bridge LPI release */
	rc = __write_register(core, WRAPPER_DEBUG_BRIDGE_LPI_CONTROL_IRIS3, 0x0);
	if (rc)
		return rc;

	rc = __read_register_with_poll_timeout(core, WRAPPER_DEBUG_BRIDGE_LPI_STATUS_IRIS3,
			0xffffffff, 0x0, 200, 2000);
	if (rc)
		d_vpr_h("%s: debug bridge release failed\n", __func__);

	/* Reset MVP QNS4PDXFIFO */
	rc = __write_register(core, WRAPPER_TZ_CTL_AXI_CLOCK_CONFIG, 0x3);
	if (rc)
		return rc;

	rc = __write_register(core, WRAPPER_TZ_QNS4PDXFIFO_RESET, 0x1);
	if (rc)
		return rc;

	rc = __write_register(core, WRAPPER_TZ_QNS4PDXFIFO_RESET, 0x0);
	if (rc)
		return rc;

	rc = __write_register(core, WRAPPER_TZ_CTL_AXI_CLOCK_CONFIG, 0x0);
	if (rc)
		return rc;

	/* Turn off MVP MVS0C core clock */
	rc = __disable_unprepare_clock_iris3(core, "core_clk");
	if (rc) {
		d_vpr_e("%s: disable unprepare core_clk failed\n", __func__);
		rc = 0;
	}

	/* power down process */
	rc = __disable_regulator_iris3(core, "iris-ctl");
	if (rc) {
		d_vpr_e("%s: disable regulator iris-ctl failed\n", __func__);
		rc = 0;
	}

	return rc;
}

static int __power_off_iris3(struct msm_vidc_core *core)
{
	int rc = 0;

	if (!core || !core->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!core->power_enabled)
		return 0;

	/**
	 * Reset video_cc_mvs0_clk_src value to resolve MMRM high video
	 * clock projection issue.
	 */
	rc = __set_clocks(core, 0);
	if (rc)
		d_vpr_e("%s: resetting clocks failed\n", __func__);

	if (__power_off_iris3_hardware(core))
		d_vpr_e("%s: failed to power off hardware\n", __func__);

	if (__power_off_iris3_controller(core))
		d_vpr_e("%s: failed to power off controller\n", __func__);

	if (__unvote_buses(core))
		d_vpr_e("%s: failed to unvote buses\n", __func__);

	if (!(core->intr_status & WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS3))
		disable_irq_nosync(core->dt->irq);
	core->intr_status = 0;

	core->power_enabled = false;

	return rc;
}

static int __power_on_iris3_controller(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = __enable_regulator_iris3(core, "iris-ctl");
	if (rc)
		goto fail_regulator;

	rc = call_venus_op(core, reset_ahb2axi_bridge, core);
	if (rc)
		goto fail_reset_ahb2axi;

	rc = __prepare_enable_clock_iris3(core, "gcc_video_axi0");
	if (rc)
		goto fail_clk_axi;

	rc = __prepare_enable_clock_iris3(core, "core_clk");
	if (rc)
		goto fail_clk_controller;

	return 0;

fail_clk_controller:
	__disable_unprepare_clock_iris3(core, "gcc_video_axi0");
fail_clk_axi:
fail_reset_ahb2axi:
	__disable_regulator_iris3(core, "iris-ctl");
fail_regulator:
	return rc;
}

static int __power_on_iris3_hardware(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = __enable_regulator_iris3(core, "vcodec");
	if (rc)
		goto fail_regulator;

	rc = __prepare_enable_clock_iris3(core, "vcodec_clk");
	if (rc)
		goto fail_clk_controller;

	return 0;

fail_clk_controller:
	__disable_regulator_iris3(core, "vcodec");
fail_regulator:
	return rc;
}

static int __power_on_iris3(struct msm_vidc_core *core)
{
	int rc = 0;

	if (core->power_enabled)
		return 0;

	/* Vote for all hardware resources */
	rc = __vote_buses(core, INT_MAX, INT_MAX);
	if (rc) {
		d_vpr_e("%s: failed to vote buses, rc %d\n", __func__, rc);
		goto fail_vote_buses;
	}

	rc = __power_on_iris3_controller(core);
	if (rc) {
		d_vpr_e("%s: failed to power on iris3 controller\n", __func__);
		goto fail_power_on_controller;
	}

	rc = __power_on_iris3_hardware(core);
	if (rc) {
		d_vpr_e("%s: failed to power on iris3 hardware\n", __func__);
		goto fail_power_on_hardware;
	}
	/* video controller and hardware powered on successfully */
	core->power_enabled = true;

	rc = __scale_clocks(core);
	if (rc) {
		d_vpr_e("%s: failed to scale clocks\n", __func__);
		rc = 0;
	}
	/*
	 * Re-program all of the registers that get reset as a result of
	 * regulator_disable() and _enable()
	 */
	__set_registers(core);

	call_venus_op(core, interrupt_init, core);
	core->intr_status = 0;
	enable_irq(core->dt->irq);

	return rc;

fail_power_on_hardware:
	__power_off_iris3_controller(core);
fail_power_on_controller:
	__unvote_buses(core);
fail_vote_buses:
	core->power_enabled = false;
	return rc;
}

static int __prepare_pc_iris3(struct msm_vidc_core *vidc_core)
{
	int rc = 0;
	u32 wfi_status = 0, idle_status = 0, pc_ready = 0;
	u32 ctrl_status = 0;
	struct msm_vidc_core *core = vidc_core;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = __read_register(core, CTRL_STATUS_IRIS3, &ctrl_status);
	if (rc)
		return rc;

	pc_ready = ctrl_status & CTRL_STATUS_PC_READY_IRIS3;
	idle_status = ctrl_status & BIT(30);

	if (pc_ready) {
		d_vpr_h("Already in pc_ready state\n");
		return 0;
	}
	rc = __read_register(core, WRAPPER_TZ_CPU_STATUS, &wfi_status);
	if (rc)
		return rc;

	wfi_status &= BIT(0);
	if (!wfi_status || !idle_status) {
		d_vpr_e("Skipping PC, wfi status not set\n");
		goto skip_power_off;
	}

	rc = __prepare_pc(core);
	if (rc) {
		d_vpr_e("Failed __prepare_pc %d\n", rc);
		goto skip_power_off;
	}

	rc = __read_register_with_poll_timeout(core, CTRL_STATUS_IRIS3,
			CTRL_STATUS_PC_READY_IRIS3, CTRL_STATUS_PC_READY_IRIS3, 250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Ctrl status not set\n", __func__);
		goto skip_power_off;
	}

	rc = __read_register_with_poll_timeout(core, WRAPPER_TZ_CPU_STATUS,
			BIT(0), 0x1, 250, 2500);
	if (rc) {
		d_vpr_e("%s: Skip PC. Wfi status not set\n", __func__);
		goto skip_power_off;
	}
	return rc;

skip_power_off:
	rc = __read_register(core, CTRL_STATUS_IRIS3, &ctrl_status);
	if (rc)
		return rc;
	rc = __read_register(core, WRAPPER_TZ_CPU_STATUS, &wfi_status);
	if (rc)
		return rc;
	wfi_status &= BIT(0);
	d_vpr_e("Skip PC, wfi=%#x, idle=%#x, pcr=%#x, ctrl=%#x)\n",
		wfi_status, idle_status, pc_ready, ctrl_status);
	return -EAGAIN;
}

static int __raise_interrupt_iris3(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = __write_register(core, CPU_IC_SOFTINT_IRIS3, 1 << CPU_IC_SOFTINT_H2A_SHFT_IRIS3);
	if (rc)
		return rc;

	return 0;
}

static int __watchdog_iris3(struct msm_vidc_core *vidc_core, u32 intr_status)
{
	int rc = 0;
	struct msm_vidc_core *core = vidc_core;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (intr_status & WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS3) {
		d_vpr_e("%s: received watchdog interrupt\n", __func__);
		rc = 1;
	}

	return rc;
}

static int __noc_error_info_iris3(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/*
	 * we are not supposed to access vcodec subsystem registers
	 * unless vcodec core clock WRAPPER_CORE_CLOCK_CONFIG_IRIS3 is enabled.
	 * core clock might have been disabled by video firmware as part of
	 * inter frame power collapse (power plane control feature).
	 */

	/*
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_SWID_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_SWID_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_SWID_HIGH);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_SWID_HIGH:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_MAINCTL_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_MAINCTL_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRVLD_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRVLD_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRCLR_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRCLR_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG0_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG0_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG0_HIGH);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG0_HIGH:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG1_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG1_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG1_HIGH);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG1_HIGH:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG2_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG2_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG2_HIGH);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG2_HIGH:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG3_LOW);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG3_LOW:     %#x\n", val);
	val = __read_register(core, VCODEC_NOC_ERL_MAIN_ERRLOG3_HIGH);
	d_vpr_e("VCODEC_NOC_ERL_MAIN_ERRLOG3_HIGH:     %#x\n", val);
	*/

	return 0;
}

static int __clear_interrupt_iris3(struct msm_vidc_core *vidc_core)
{
	struct msm_vidc_core *core = vidc_core;
	u32 intr_status = 0, mask = 0;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: NULL core\n", __func__);
		return 0;
	}

	rc = __read_register(core, WRAPPER_INTR_STATUS_IRIS3, &intr_status);
	if (rc)
		return rc;

	mask = (WRAPPER_INTR_STATUS_A2H_BMSK_IRIS3|
		WRAPPER_INTR_STATUS_A2HWD_BMSK_IRIS3|
		CTRL_INIT_IDLE_MSG_BMSK_IRIS3);

	if (intr_status & mask) {
		core->intr_status |= intr_status;
		core->reg_count++;
		d_vpr_l("INTERRUPT: times: %d interrupt_status: %d\n",
			core->reg_count, intr_status);
	} else {
		core->spur_count++;
	}

	rc = __write_register(core, CPU_CS_A2HSOFTINTCLR_IRIS3, 1);
	if (rc)
		return rc;

	return 0;
}

static int __boot_firmware_iris3(struct msm_vidc_core *vidc_core)
{
	int rc = 0;
	u32 ctrl_init_val = 0, ctrl_status = 0, count = 0, max_tries = 1000;
	struct msm_vidc_core *core = vidc_core;

	if (!core) {
		d_vpr_e("%s: NULL core\n", __func__);
		return 0;
	}

	ctrl_init_val = BIT(0);

	rc = __write_register(core, CTRL_INIT_IRIS3, ctrl_init_val);
	if (rc)
		return rc;

	while (!ctrl_status && count < max_tries) {
		rc = __read_register(core, CTRL_STATUS_IRIS3, &ctrl_status);
		if (rc)
			return rc;

		if ((ctrl_status & CTRL_ERROR_STATUS__M_IRIS3) == 0x4) {
			d_vpr_e("invalid setting for UC_REGION\n");
			break;
		}

		usleep_range(50, 100);
		count++;
	}

	if (count >= max_tries) {
		d_vpr_e("Error booting up vidc firmware\n");
		return -ETIME;
	}

	/* Enable interrupt before sending commands to venus */
	rc = __write_register(core, CPU_CS_H2XSOFTINTEN_IRIS3, 0x1);
	if (rc)
		return rc;

	rc = __write_register(core, CPU_CS_X2RPMh_IRIS3, 0x0);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_decide_work_mode_iris3(struct msm_vidc_inst* inst)
{
	u32 work_mode;
	struct v4l2_format *inp_f;
	u32 width, height;
	bool res_ok = false;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	work_mode = MSM_VIDC_STAGE_2;
	inp_f = &inst->fmts[INPUT_PORT];

	if (is_image_decode_session(inst))
		work_mode = MSM_VIDC_STAGE_1;

	if (is_image_session(inst))
		goto exit;

	if (is_decode_session(inst)) {
		height = inp_f->fmt.pix_mp.height;
		width = inp_f->fmt.pix_mp.width;
		res_ok = res_is_less_than(width, height, 1280, 720);
		if (inst->capabilities->cap[CODED_FRAMES].value ==
				CODED_FRAMES_INTERLACE ||
			inst->capabilities->cap[LOWLATENCY_MODE].value ||
			res_ok) {
			work_mode = MSM_VIDC_STAGE_1;
		}
	} else if (is_encode_session(inst)) {
		height = inst->crop.height;
		width = inst->crop.width;
		res_ok = !res_is_greater_than(width, height, 4096, 2160);
		if (res_ok &&
			(inst->capabilities->cap[LOWLATENCY_MODE].value)) {
			work_mode = MSM_VIDC_STAGE_1;
		}
		if (inst->capabilities->cap[SLICE_MODE].value ==
			V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES) {
			work_mode = MSM_VIDC_STAGE_1;
		}
		if (inst->capabilities->cap[LOSSLESS].value)
			work_mode = MSM_VIDC_STAGE_2;

		if (!inst->capabilities->cap[GOP_SIZE].value)
			work_mode = MSM_VIDC_STAGE_2;
	} else {
		i_vpr_e(inst, "%s: invalid session type\n", __func__);
		return -EINVAL;
	}

exit:
	i_vpr_h(inst, "Configuring work mode = %u low latency = %u, gop size = %u\n",
		work_mode, inst->capabilities->cap[LOWLATENCY_MODE].value,
		inst->capabilities->cap[GOP_SIZE].value);
	msm_vidc_update_cap_value(inst, STAGE, work_mode, __func__);

	return 0;
}

int msm_vidc_decide_work_route_iris3(struct msm_vidc_inst* inst)
{
	u32 work_route;
	struct msm_vidc_core* core;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	work_route = core->capabilities[NUM_VPP_PIPE].value;

	if (is_image_session(inst))
		goto exit;

	if (is_decode_session(inst)) {
		if (inst->capabilities->cap[CODED_FRAMES].value ==
				CODED_FRAMES_INTERLACE)
			work_route = MSM_VIDC_PIPE_1;
	} else if (is_encode_session(inst)) {
		u32 slice_mode;

		slice_mode = inst->capabilities->cap[SLICE_MODE].value;

		/*TODO Pipe=1 for legacy CBR*/
		if (slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES)
			work_route = MSM_VIDC_PIPE_1;

	} else {
		i_vpr_e(inst, "%s: invalid session type\n", __func__);
		return -EINVAL;
	}

exit:
	i_vpr_h(inst, "Configuring work route = %u", work_route);
	msm_vidc_update_cap_value(inst, PIPE, work_route, __func__);

	return 0;
}

int msm_vidc_decide_quality_mode_iris3(struct msm_vidc_inst* inst)
{
	struct msm_vidc_inst_capability* capability = NULL;
	struct msm_vidc_core *core;
	u32 mbpf, mbps, max_hq_mbpf, max_hq_mbps;
	u32 mode = MSM_VIDC_POWER_SAVE_MODE;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	capability = inst->capabilities;

	if (!is_encode_session(inst))
		return 0;

	/* image or lossless or all intra runs at quality mode */
	if (is_image_session(inst) || capability->cap[LOSSLESS].value ||
		capability->cap[ALL_INTRA].value) {
		mode = MSM_VIDC_MAX_QUALITY_MODE;
		goto decision_done;
	}

	/* for lesser complexity, make LP for all resolution */
	if (capability->cap[COMPLEXITY].value < DEFAULT_COMPLEXITY) {
		mode = MSM_VIDC_POWER_SAVE_MODE;
		goto decision_done;
	}

	mbpf = msm_vidc_get_mbs_per_frame(inst);
	mbps = mbpf * msm_vidc_get_fps(inst);
	core = inst->core;
	max_hq_mbpf = core->capabilities[MAX_MBPF_HQ].value;;
	max_hq_mbps = core->capabilities[MAX_MBPS_HQ].value;;

	if (!is_realtime_session(inst)) {
		if (((capability->cap[COMPLEXITY].flags & CAP_FLAG_CLIENT_SET) &&
			(capability->cap[COMPLEXITY].value >= DEFAULT_COMPLEXITY)) ||
			mbpf <= max_hq_mbpf) {
			mode = MSM_VIDC_MAX_QUALITY_MODE;
			goto decision_done;
		}
	}

	if (mbpf <= max_hq_mbpf && mbps <= max_hq_mbps)
		mode = MSM_VIDC_MAX_QUALITY_MODE;

decision_done:
	msm_vidc_update_cap_value(inst, QUALITY_MODE, mode, __func__);

	return 0;
}

int msm_vidc_adjust_bitrate_boost_iris3(void* instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability* capability = NULL;
	s32 adjusted_value;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	s32 rc_type = -1;
	u32 width, height, frame_rate;
	struct v4l2_format *f;
	u32 max_bitrate = 0, bitrate = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val :
		capability->cap[BITRATE_BOOST].value;

	if (inst->bufq[OUTPUT_PORT].vb2q->streaming)
		return 0;

	if (msm_vidc_get_parent_value(inst, BITRATE_BOOST,
		BITRATE_MODE, &rc_type, __func__))
		return -EINVAL;

	/*
	 * Bitrate Boost are supported only for VBR rc type.
	 * Hence, do not adjust or set to firmware for non VBR rc's
	 */
	if (rc_type != HFI_RC_VBR_CFR) {
		adjusted_value = 0;
		goto adjust;
	}

	frame_rate = inst->capabilities->cap[FRAME_RATE].value >> 16;
	f= &inst->fmts[OUTPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;

	/*
	 * honor client set bitrate boost
	 * if client did not set, keep max bitrate boost upto 4k@60fps
	 * and remove bitrate boost after 4k@60fps
	*/
	if (capability->cap[BITRATE_BOOST].flags & CAP_FLAG_CLIENT_SET) {
		/* accept client set bitrate boost value as is */
	} else {
		if (res_is_less_than_or_equal_to(width, height, 4096, 2176) &&
			frame_rate <= 60)
			adjusted_value = MAX_BITRATE_BOOST;
		else
			adjusted_value = 0;
	}

	max_bitrate = msm_vidc_get_max_bitrate(inst);
	bitrate = inst->capabilities->cap[BIT_RATE].value;
	if (adjusted_value) {
		if ((bitrate + bitrate / (100 / adjusted_value)) > max_bitrate) {
			i_vpr_h(inst,
				"%s: bitrate %d is beyond max bitrate %d, remove bitrate boost\n",
				__func__, max_bitrate, bitrate);
			adjusted_value = 0;
		}
	}
adjust:
	msm_vidc_update_cap_value(inst, BITRATE_BOOST, adjusted_value, __func__);

	return 0;
}



static struct msm_vidc_venus_ops iris3_ops = {
	.boot_firmware = __boot_firmware_iris3,
	.interrupt_init = __interrupt_init_iris3,
	.raise_interrupt = __raise_interrupt_iris3,
	.clear_interrupt = __clear_interrupt_iris3,
	.setup_ucregion_memmap = __setup_ucregion_memory_map_iris3,
	.clock_config_on_enable = NULL,
	.reset_ahb2axi_bridge = __reset_ahb2axi_bridge,
	.power_on = __power_on_iris3,
	.power_off = __power_off_iris3,
	.prepare_pc = __prepare_pc_iris3,
	.watchdog = __watchdog_iris3,
	.noc_error_info = __noc_error_info_iris3,
};

static struct msm_vidc_session_ops msm_session_ops = {
	.buffer_size = msm_buffer_size_iris3,
	.min_count = msm_buffer_min_count_iris3,
	.extra_count = msm_buffer_extra_count_iris3,
	.calc_freq = msm_vidc_calc_freq_iris3,
	.calc_bw = msm_vidc_calc_bw_iris3,
	.decide_work_route = msm_vidc_decide_work_route_iris3,
	.decide_work_mode = msm_vidc_decide_work_mode_iris3,
	.decide_quality_mode = msm_vidc_decide_quality_mode_iris3,
};

int msm_vidc_init_iris3(struct msm_vidc_core *core)
{
	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	d_vpr_h("%s()\n", __func__);
	core->venus_ops = &iris3_ops;
	core->session_ops = &msm_session_ops;

	return 0;
}

int msm_vidc_deinit_iris3(struct msm_vidc_core *core)
{
	/* do nothing */
	return 0;
}
