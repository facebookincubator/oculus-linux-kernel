/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CPASTOP_V636_100_H_
#define _CPASTOP_V636_100_H_

#define TEST_IRQ_ENABLE 0

static struct cam_camnoc_irq_sbm cam_cpas_v636_100_irq_sbm = {
	.sbm_enable = {
		.access_type = CAM_REG_TYPE_READ_WRITE,
		.enable = true,
		.offset = 0x240, /* CAM_NOC_SBM_FAULTINEN0_LOW */
		.value = 0x2 |    /* SBM_FAULTINEN0_LOW_PORT1_MASK */
			0x04 |     /* SBM_FAULTINEN0_LOW_PORT2_MASK */
			0x10 |    /* SBM_FAULTINEN0_LOW_PORT4_MASK */
			0x20 |    /* SBM_FAULTINEN0_LOW_PORT5_MASK */
			(TEST_IRQ_ENABLE ?
			0x80 :    /* SBM_FAULTINEN0_LOW_PORT7_MASK */
			0x0),
	},
	.sbm_status = {
		.access_type = CAM_REG_TYPE_READ,
		.enable = true,
		.offset = 0x248, /* CAM_NOC_SBM_FAULTINSTATUS0_LOW */
	},
	.sbm_clear = {
		.access_type = CAM_REG_TYPE_WRITE,
		.enable = true,
		.offset = 0x280, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		.value = TEST_IRQ_ENABLE ? 0x5 : 0x1,
	}
};

static struct cam_camnoc_irq_err
	cam_cpas_v636_100_irq_err[] = {
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_SLAVE_ERROR,
		.enable = false,
		.sbm_port = 0x1, /* SBM_FAULTINSTATUS0_LOW_PORT0_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x8, /* CAM_NOC_ERL_MAINCTL_LOW */
			.value = 1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x10, /* CAM_NOC_ERL_ERRVLD_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x18, /* CAM_NOC_ERL_ERRCLR_LOW */
			.value = 1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IFE0_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x20, /* SBM_FAULTINSTATUS0_LOW_PORT5_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x95A0, /* RT0_NIU_ENCERREN_LOW */
			.value = 0xF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x9590, /* RT0_NIU_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x9598, /* RT0_NIU_ENCERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x2, /* SBM_FAULTINSTATUS0_LOW_PORT1_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x97A0, /* cam_noc_ife_1_niu_EncErrEn_Low */
			.value = 0XF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x9790, /* cam_noc_ife_1_niu_EncErrStatus_Low */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x9798, /* cam_noc_ife_1_niu_EncErrClr_Low */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR,
		.enable = true,
		.sbm_port = 0x4, /* SBM_FAULTINSTATUS0_LOW_PORT2_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x9D20, /* CAM_NOC_IPE_RD_NIU_DECERREN_LOW */
			.value = 0xFF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x9D10, /* CAM_NOC_IPE_RD_NIU_DECERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x9D18, /* CAM_NOC_IPE_RD_NIU_DECERRCLR_LOW */
			.value = 0X1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_IPE_UBWC_ENCODE_ERROR,
		.enable = true,
		.sbm_port = 0x10, /* SBM_FAULTINSTATUS0_LOW_PORT4_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x9FA0, /* CAM_NOC_IPE_WR_NIU_ENCERREN_LOW */
			.value = 0XF,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x9F90, /* CAM_NOC_IPE_WR_NIU_ENCERRSTATUS_LOW */
		},
		.err_clear = {
			.access_type = CAM_REG_TYPE_WRITE,
			.enable = true,
			.offset = 0x9F98, /* CAM_NOC_IPE_WR_NIU_ENCERRCLR_LOW */
			.value = 0x1,
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT,
		.enable = false,
		.sbm_port = 0x40, /* SBM_FAULTINSTATUS0_LOW_PORT6_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x288, /* CAM_NOC_SBM_FLAGOUTSET0_LOW */
			.value = 0x1,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x290, /* CAM_NOC_SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		},
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_RESERVED1,
		.enable = false,
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_RESERVED2,
		.enable = false,
	},
	{
		.irq_type = CAM_CAMNOC_HW_IRQ_CAMNOC_TEST,
		.enable = TEST_IRQ_ENABLE ? true : false,
		.sbm_port = 0x80, /* SBM_FAULTINSTATUS0_LOW_PORT7_MASK */
		.err_enable = {
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.enable = true,
			.offset = 0x288, /* CAM_NOC_SBM_FLAGOUTSET0_LOW */
			.value = 0x5,
		},
		.err_status = {
			.access_type = CAM_REG_TYPE_READ,
			.enable = true,
			.offset = 0x290, /* CAM_NOC_SBM_FLAGOUTSTATUS0_LOW */
		},
		.err_clear = {
			.enable = false, /* CAM_NOC_SBM_FLAGOUTCLR0_LOW */
		},
	},
};

static struct cam_camnoc_specific
	cam_cpas_v636_100_camnoc_specific[] = {
	{
		.port_type = CAM_CAMNOC_IFE02,
		.port_name = "IFE0",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9430, /* IFE_0_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9434, /* IFE_0_NIU_PRIORITYLUT_HIGH */
			.value = 0x77777777,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9438, /* IFE_0_NIU_URGENCY_LOW */
			.value = 0x1E30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9440, /* IFE_0_NIU_DANGERLUT_LOW */
			.value = 0xff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9448, /* IFE_0_NIU_SAFELUT_LOW */
			.value = 0xff0f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA508, /* IFE_0_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA520, /* IFE_0_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA524, /* IFE_0_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x9420, /* IFE_0_NIU_MAXWR_LOW */
			.value = 0x0,
		},
		.dynattr_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7108, /* IFE_0_DYNATTR_MAINCTL_LOW */
			.value = 0x100,
		},
		.dynattr_tr_type_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7138, /* IFE_0_DYNATTR_TRTYPELUT_LOW */
			.value = 0x43222,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE13,
		.port_name = "IFE1",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9630, /* IFE_1_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9634, /* IFE_1_NIU_PRIORITYLUT_HIGH */
			.value = 0x77777777,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9638, /* IFE_1_NIU_URGENCY_LOW */
			.value = 0x1E30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9640, /* IFE_1_NIU_DANGERLUT_LOW */
			.value = 0xff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9648, /* IFE_1_NIU_SAFELUT_LOW */
			.value = 0xff0f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA588, /* IFE_1_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA5A0, /* IFE_1_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA5A4, /* IFE_1_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x9620, /* IFE_1_NIU_MAXWR_LOW */
			.value = 0x0,
		},
		.dynattr_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7188, /* IFE_1_DYNATTR_MAINCTL_LOW */
			.value = 0x100,
		},
		.dynattr_tr_type_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x71B8, /* IFE_1_DYNATTR_TRTYPELUT_LOW */
			.value = 0x43222,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE_LITE,
		.port_name = "IFE-LITE_0",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9830, /* IFE_LITE_0_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9834, /* IFE_LITE_0_NIU_PRIORITYLUT_HIGH */
			.value = 0x77777777,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9838, /* IFE_LITE_0_NIU_URGENCY_LOW */
			.value = 0x1E30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9840, /* IFE_LITE_0_NIU_DANGERLUT_LOW */
			.value = 0xff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9848, /* IFE_LITE_0_NIU_SAFELUT_LOW */
			.value = 0xff0f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA608, /* IFE_LITE_0_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA620, /* IFE_LITE_0_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA624, /* IFE_LITE_0_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x9820, /* IFE_LITE_0_NIU_MAXWR_LOW */
			.value = 0x0,
		},
		.dynattr_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7208, /* IFE_LITE_0_DYNATTR_MAINCTL_LOW */
			.value = 0x100,
		},
		.dynattr_tr_type_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x7238, /* IFE_LITE_0_DYNATTR_TRTYPELUT_LOW */
			.value = 0x43222,
		},
	},
	{
		.port_type = CAM_CAMNOC_IFE_LITE_1,
		.port_name = "IFE-LITE_1",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9A30, /* IFE_LITE_1_NIU_PRIORITYLUT_LOW */
			.value = 0x66665433,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9A34, /* IFE_LITE_1_NIU_PRIORITYLUT_HIGH */
			.value = 0x77777777,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9A38, /* IFE_LITE_1_NIU_URGENCY_LOW */
			.value = 0x1E30,
		},
		.danger_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9A40, /* IFE_LITE_1_NIU_DANGERLUT_LOW */
			.value = 0xff00,
		},
		.safe_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9A48, /* IFE_LITE_1_NIU_SAFELUT_LOW */
			.value = 0xff0f,
		},
		.ubwc_ctl = {
			/*
			 * Do not explicitly set ubwc config register.
			 * Power on default values are taking care of required
			 * register settings.
			 */
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA688, /* IFE_LITE_1_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA6A0, /* IFE_LITE_1_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA6A4, /* IFE_LITE_1_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x9A20, /* IFE_LITE_1_NIU_MAXWR_LOW */
			.value = 0x0,
		},
		.dynattr_mainctl = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x7288, /* IFE_LITE_1_DYNATTR_MAINCTL_LOW */
			.value = 0x100,
		},
		.dynattr_tr_type_lut = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x72B8, /* IFE_LITE_1_DYNATTR_TRTYPELUT_LOW */
			.value = 0x43222,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE_WR,
		.port_name = "IPE_WR",
		.enable = true,
		.priority_lut_low = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9E30, /* IPE_WR_NIU_PRIORITYLUT_LOW */
			.value = 0x33333333,
		},
		.priority_lut_high = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9E34, /* IPE_WR_NIU_PRIORITYLUT_HIGH */
			.value = 0x33333333,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9E38, /* IPE_WR_NIU_URGENCY_LOW */
			.value = 0x30,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9E40, /* IPE_WR_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9E48, /* IPE_WR_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA788, /* IPE_WR_QOSGEN_MAINCTL */
			.value = 0x0,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA7A0, /* IPE_WR_QOSGEN_SHAPING_LOW */
			.value = 0x0,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA7A4, /* IPE_WR_QOSGEN_SHAPING_HIGH */
			.value = 0x0,
		},
		.maxwr_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ,
			.masked_value = 0,
			.offset = 0x9E20, /* IPE_WR_NIU_MAXWR_LOW */
			.value = 0x0,
		},
	},
	{
		.port_type = CAM_CAMNOC_IPE0_RD,
		.port_name = "IPE_RD",
		.enable = true,
		.priority_lut_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9C30, /* IPE_RD_NIU_PRIORITYLUT_LOW */
			.value = 0x0,
		},
		.priority_lut_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9C34, /* IPE_RD_NIU_PRIORITYLUT_HIGH */
			.value = 0x0,
		},
		.urgency = {
			.enable = true,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9C38, /* IPE_RD_NIU_URGENCY_LOW */
			.value = 0x3,
		},
		.danger_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9C40, /* IPE_RD_NIU_DANGERLUT_LOW */
			.value = 0x0,
		},
		.safe_lut = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9C48, /* IPE_RD_NIU_SAFELUT_LOW */
			.value = 0xffff,
		},
		.ubwc_ctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0x9D08, /* IPE_RD_NIU_DECCTL_LOW */
			.value = 1,
		},
		.qosgen_mainctl = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA708, /* IPE_RD_QOSGEN_MAINCTL */
			.value = 0x2,
		},
		.qosgen_shaping_low = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA720, /* IPE_RD_QOSGEN_SHAPING_LOW */
			.value = 0x29292929,
		},
		.qosgen_shaping_high = {
			.enable = false,
			.access_type = CAM_REG_TYPE_READ_WRITE,
			.masked_value = 0,
			.offset = 0xA724, /* IPE_RD_QOSGEN_SHAPING_HIGH */
			.value = 0x29292929,
		},
	}
};

static struct cam_camnoc_err_logger_info cam636_cpas100_err_logger_offsets = {
	.mainctrl     =  0x8, /* ERL_MAINCTL_LOW */
	.errvld       =  0x10, /* ERL_ERRVLD_LOW */
	.errlog0_low  =  0x20, /* ERL_ERRLOG0_LOW */
	.errlog0_high =  0x24, /* ERL_ERRLOG0_HIGH */
	.errlog1_low  =  0x28, /* ERL_ERRLOG1_LOW */
	.errlog1_high =  0x2c, /* ERL_ERRLOG1_HIGH */
	.errlog2_low  =  0x30, /* ERL_ERRLOG2_LOW */
	.errlog2_high =  0x34, /* ERL_ERRLOG2_HIGH */
	.errlog3_low  =  0x38, /* ERL_ERRLOG3_LOW */
	.errlog3_high =  0x3c, /* ERL_ERRLOG3_HIGH */
};

static struct cam_cpas_hw_errata_wa_list cam636_cpas100_errata_wa_list = {
	.camnoc_flush_slave_pending_trans = {
		.enable = false,
		.data.reg_info = {
			.access_type = CAM_REG_TYPE_READ,
			.offset = 0x300, /* sbm_SenseIn0_Low */
			.mask = 0xE0000, /* Bits 17, 18, 19 */
			.value = 0, /* expected to be 0 */
		},
	},
	.enable_icp_clk_for_qchannel = {
		.enable = true,
	},
};

static struct cam_camnoc_info cam636_cpas100_camnoc_info = {
	.specific = &cam_cpas_v636_100_camnoc_specific[0],
	.specific_size = ARRAY_SIZE(cam_cpas_v636_100_camnoc_specific),
	.irq_sbm = &cam_cpas_v636_100_irq_sbm,
	.irq_err = &cam_cpas_v636_100_irq_err[0],
	.irq_err_size = ARRAY_SIZE(cam_cpas_v636_100_irq_err),
	.err_logger = &cam636_cpas100_err_logger_offsets,
	.errata_wa_list = &cam636_cpas100_errata_wa_list,
};

static struct cam_cpas_camnoc_qchannel cam636_cpas100_qchannel_info = {
	.qchannel_ctrl   = 0x5C,
	.qchannel_status = 0x60,
};
#endif /* _CPASTOP_V636_100_H_ */

