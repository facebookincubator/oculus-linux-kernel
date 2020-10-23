/*
 * Driver for the CM710x codec
 *
 * Author:	Tzung-Dar Tsai <tdtsai@cmedia.com.tw>
 *		Copyright 2017
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __CM710X_H__
#define __CM710X_H__

#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <stdatomic.h>

/* Info */
#define CM710X_RESET				0x00
#define CM710X_VENDOR_ID			0xfd
#define CM710X_VENDOR_ID1			0xfe
#define CM710X_VENDOR_ID2			0xff
/*  I/O - Output */
#define CM710X_LOUT1				0x01
/* I/O - Input */
#define CM710X_IN1				0x03
#define CM710X_MICBIAS				0x04
/* I/O - SLIMBus */
#define CM710X_SLIMBUS_PARAM			0x07
#define CM710X_SLIMBUS_RX			0x08
#define CM710X_SLIMBUS_CTRL			0x09
/* I/O */
#define CM710X_SIDETONE_CTRL			0x13
/* I/O - ADC/DAC */
#define CM710X_ANA_DAC1_2_3_SRC			0x15
#define CM710X_IF_DSP_DAC3_4_MIXER		0x16
#define CM710X_DAC4_DIG_VOL			0x17
#define CM710X_DAC3_DIG_VOL			0x18
#define CM710X_DAC1_DIG_VOL			0x19
#define CM710X_DAC2_DIG_VOL			0x1a
#define CM710X_IF_DSP_DAC2_MIXER		0x1b
#define CM710X_STO1_ADC_DIG_VOL			0x1c
#define CM710X_MONO_ADC_DIG_VOL			0x1d
#define CM710X_STO1_2_ADC_BST			0x1e
#define CM710X_STO2_ADC_DIG_VOL			0x1f
/* Mixer - D-D */
#define CM710X_ADC_BST_CTRL2			0x20
#define CM710X_STO3_4_ADC_BST			0x21
#define CM710X_STO3_ADC_DIG_VOL			0x22
#define CM710X_STO4_ADC_DIG_VOL			0x23
#define CM710X_STO4_ADC_MIXER			0x24
#define CM710X_STO3_ADC_MIXER			0x25
#define CM710X_STO2_ADC_MIXER			0x26
#define CM710X_STO1_ADC_MIXER			0x27
#define CM710X_MONO_ADC_MIXER			0x28
#define CM710X_ADC_IF_DSP_DAC1_MIXER		0x29
#define CM710X_STO1_DAC_MIXER			0x2a
#define CM710X_MONO_DAC_MIXER			0x2b
#define CM710X_DD1_MIXER			0x2c
#define CM710X_DD2_MIXER			0x2d
#define CM710X_IF3_DATA				0x2f
#define CM710X_IF4_DATA				0x30
/* Mixer - PDM */
#define CM710X_PDM_OUT_CTRL			0x31
#define CM710X_PDM_DATA_CTRL1			0x32
#define CM710X_PDM_DATA_CTRL2			0x33
#define CM710X_PDM1_DATA_CTRL2			0x34
#define CM710X_PDM1_DATA_CTRL3			0x35
#define CM710X_PDM1_DATA_CTRL4			0x36
#define CM710X_PDM2_DATA_CTRL2			0x37
#define CM710X_PDM2_DATA_CTRL3			0x38
#define CM710X_PDM2_DATA_CTRL4			0x39
/* TDM */
#define CM710X_TDM1_CTRL1			0x3b
#define CM710X_TDM1_CTRL2			0x3c
#define CM710X_TDM1_CTRL3			0x3d
#define CM710X_TDM1_CTRL4			0x3e
#define CM710X_TDM1_CTRL5			0x3f
#define CM710X_TDM2_CTRL1			0x40
#define CM710X_TDM2_CTRL2			0x41
#define CM710X_TDM2_CTRL3			0x42
#define CM710X_TDM2_CTRL4			0x43
#define CM710X_TDM2_CTRL5			0x44
/* I2C_MASTER_CTRL */
#define CM710X_I2C_MASTER_CTRL1			0x47
#define CM710X_I2C_MASTER_CTRL2			0x48
#define CM710X_I2C_MASTER_CTRL3			0x49
#define CM710X_I2C_MASTER_CTRL4			0x4a
#define CM710X_I2C_MASTER_CTRL5			0x4b
#define CM710X_I2C_MASTER_CTRL6			0x4c
#define CM710X_I2C_MASTER_CTRL7			0x4d
#define CM710X_I2C_MASTER_CTRL8			0x4e
/* DMIC */
#define CM710X_DMIC_CTRL1			0x50
#define CM710X_DMIC_CTRL2			0x51
/* Haptic Generator */
#define CM710X_HAP_GENE_CTRL1			0x56
#define CM710X_HAP_GENE_CTRL2			0x57
#define CM710X_HAP_GENE_CTRL3			0x58
#define CM710X_HAP_GENE_CTRL4			0x59
#define CM710X_HAP_GENE_CTRL5			0x5a
#define CM710X_HAP_GENE_CTRL6			0x5b
#define CM710X_HAP_GENE_CTRL7			0x5c
#define CM710X_HAP_GENE_CTRL8			0x5d
#define CM710X_HAP_GENE_CTRL9			0x5e
#define CM710X_HAP_GENE_CTRL10			0x5f
/* Power */
#define CM710X_PWR_DIG1				0x61
#define CM710X_PWR_DIG2				0x62
#define CM710X_PWR_ANLG1			0x63
#define CM710X_PWR_ANLG2			0x64
#define CM710X_PWR_DSP1				0x65
#define CM710X_PWR_DSP_ST			0x66
#define CM710X_PWR_DSP2				0x67
#define CM710X_ADC_DAC_HPF_CTRL1		0x68
/* Private Register Control */
#define CM710X_PRIV_INDEX			0x6a
#define CM710X_PRIV_DATA			0x6c
/* Format - ADC/DAC */
#define CM710X_I2S4_SDP				0x6f
#define CM710X_I2S1_SDP				0x70
#define CM710X_I2S2_SDP				0x71
#define CM710X_I2S3_SDP				0x72
#define CM710X_CLK_TREE_CTRL1			0x73
#define CM710X_CLK_TREE_CTRL2			0x74
#define CM710X_CLK_TREE_CTRL3			0x75
/* Function - Analog */
#define CM710X_PLL1_CTRL1			0x7a
#define CM710X_PLL1_CTRL2			0x7b
#define CM710X_PLL2_CTRL1			0x7c
#define CM710X_PLL2_CTRL2			0x7d
#define CM710X_GLB_CLK1				0x80
#define CM710X_GLB_CLK2				0x81
#define CM710X_ASRC_1				0x83
#define CM710X_ASRC_2				0x84
#define CM710X_ASRC_3				0x85
#define CM710X_ASRC_4				0x86
#define CM710X_ASRC_5				0x87
#define CM710X_ASRC_6				0x88
#define CM710X_ASRC_7				0x89
#define CM710X_ASRC_8				0x8a
#define CM710X_ASRC_9				0x8b
#define CM710X_ASRC_10				0x8c
#define CM710X_ASRC_11				0x8d
#define CM710X_ASRC_12				0x8e
#define CM710X_ASRC_13				0x8f
#define CM710X_ASRC_14				0x90
#define CM710X_ASRC_15				0x91
#define CM710X_ASRC_16				0x92
#define CM710X_ASRC_17				0x93
#define CM710X_ASRC_18				0x94
#define CM710X_ASRC_19				0x95
#define CM710X_ASRC_20				0x97
#define CM710X_ASRC_21				0x98
#define CM710X_ASRC_22				0x99
#define CM710X_ASRC_23				0x9a
#define CM710X_VAD_CTRL1			0x9c
#define CM710X_VAD_CTRL2			0x9d
#define CM710X_VAD_CTRL3			0x9e
#define CM710X_VAD_CTRL4			0x9f
#define CM710X_VAD_CTRL5			0xa0
/* Function - Digital */
#define CM710X_DSP_INB_CTRL1			0xa3
#define CM710X_DSP_INB_CTRL2			0xa4
#define CM710X_DSP_IN_OUTB_CTRL			0xa5
#define CM710X_DSP_OUTB0_1_DIG_VOL		0xa6
#define CM710X_DSP_OUTB2_3_DIG_VOL		0xa7
#define CM710X_DSP_OUTB4_5_DIG_VOL		0xa8
#define CM710X_DSP_OUTB6_7_DIG_VOL		0xa9
#define CM710X_ADC_EQ_CTRL1			0xae
#define CM710X_ADC_EQ_CTRL2			0xaf
#define CM710X_EQ_CTRL1				0xb0
#define CM710X_EQ_CTRL2				0xb1
#define CM710X_EQ_CTRL3				0xb2
#define CM710X_SOFT_VOL_ZERO_CROSS1		0xb3
#define CM710X_JD_CTRL1				0xb5
#define CM710X_JD_CTRL2				0xb6
#define CM710X_JD_CTRL3				0xb8
#define CM710X_IRQ_CTRL1			0xbd
#define CM710X_IRQ_CTRL2			0xbe
#define CM710X_GPIO_ST				0xbf
#define CM710X_GPIO_CTRL1			0xc0
#define CM710X_GPIO_CTRL2			0xc1
#define CM710X_GPIO_CTRL3			0xc2
#define CM710X_STO1_ADC_HI_FILTER1		0xc5
#define CM710X_STO1_ADC_HI_FILTER2		0xc6
#define CM710X_MONO_ADC_HI_FILTER1		0xc7
#define CM710X_MONO_ADC_HI_FILTER2		0xc8
#define CM710X_STO2_ADC_HI_FILTER1		0xc9
#define CM710X_STO2_ADC_HI_FILTER2		0xca
#define CM710X_STO3_ADC_HI_FILTER1		0xcb
#define CM710X_STO3_ADC_HI_FILTER2		0xcc
#define CM710X_STO4_ADC_HI_FILTER1		0xcd
#define CM710X_STO4_ADC_HI_FILTER2		0xce
#define CM710X_MB_DRC_CTRL1			0xd0
#define CM710X_DRC1_CTRL1			0xd2
#define CM710X_DRC1_CTRL2			0xd3
#define CM710X_DRC1_CTRL3			0xd4
#define CM710X_DRC1_CTRL4			0xd5
#define CM710X_DRC1_CTRL5			0xd6
#define CM710X_DRC1_CTRL6			0xd7
#define CM710X_DRC2_CTRL1			0xd8
#define CM710X_DRC2_CTRL2			0xd9
#define CM710X_DRC2_CTRL3			0xda
#define CM710X_DRC2_CTRL4			0xdb
#define CM710X_DRC2_CTRL5			0xdc
#define CM710X_DRC2_CTRL6			0xdd
#define CM710X_DRC1_HL_CTRL1			0xde
#define CM710X_DRC1_HL_CTRL2			0xdf
#define CM710X_DRC2_HL_CTRL1			0xe0
#define CM710X_DRC2_HL_CTRL2			0xe1
#define CM710X_DSP_INB1_SRC_CTRL1		0xe3
#define CM710X_DSP_INB1_SRC_CTRL2		0xe4
#define CM710X_DSP_INB1_SRC_CTRL3		0xe5
#define CM710X_DSP_INB1_SRC_CTRL4		0xe6
#define CM710X_DSP_INB2_SRC_CTRL1		0xe7
#define CM710X_DSP_INB2_SRC_CTRL2		0xe8
#define CM710X_DSP_INB2_SRC_CTRL3		0xe9
#define CM710X_DSP_INB2_SRC_CTRL4		0xea
#define CM710X_DSP_INB3_SRC_CTRL1		0xeb
#define CM710X_DSP_INB3_SRC_CTRL2		0xec
#define CM710X_DSP_INB3_SRC_CTRL3		0xed
#define CM710X_DSP_INB3_SRC_CTRL4		0xee
#define CM710X_DSP_OUTB1_SRC_CTRL1		0xef
#define CM710X_DSP_OUTB1_SRC_CTRL2		0xf0
#define CM710X_DSP_OUTB1_SRC_CTRL3		0xf1
#define CM710X_DSP_OUTB1_SRC_CTRL4		0xf2
#define CM710X_DSP_OUTB2_SRC_CTRL1		0xf3
#define CM710X_DSP_OUTB2_SRC_CTRL2		0xf4
#define CM710X_DSP_OUTB2_SRC_CTRL3		0xf5
#define CM710X_DSP_OUTB2_SRC_CTRL4		0xf6

/* Virtual DSP Mixer Control */
#define CM710X_DSP_OUTB_0123_MIXER_CTRL		0xf7
#define CM710X_DSP_OUTB_45_MIXER_CTRL		0xf8
#define CM710X_DSP_OUTB_67_MIXER_CTRL		0xf9

/* General Control */
#define CM710X_DIG_MISC				0xfa
#define CM710X_GEN_CTRL1			0xfb
#define CM710X_GEN_CTRL2			0xfc

/* DSP Mode I2C Control*/
#define CM710X_DSP_I2C_OP_CODE			0x00
#define CM710X_DSP_I2C_ADDR_LSB			0x01
#define CM710X_DSP_I2C_ADDR_MSB			0x02
#define CM710X_DSP_I2C_DATA_LSB			0x03
#define CM710X_DSP_I2C_DATA_MSB			0x04

/* global definition */
#define CM710X_L_MUTE				(0x1 << 15)
#define CM710X_L_MUTE_SFT			15
#define CM710X_VOL_L_MUTE			(0x1 << 14)
#define CM710X_VOL_L_SFT			14
#define CM710X_R_MUTE				(0x1 << 7)
#define CM710X_R_MUTE_SFT			7
#define CM710X_VOL_R_MUTE			(0x1 << 6)
#define CM710X_VOL_R_SFT			6
#define CM710X_L_VOL_MASK			(0x7f << 9)
#define CM710X_L_VOL_SFT			9
#define CM710X_R_VOL_MASK			(0x7f << 1)
#define CM710X_R_VOL_SFT			1

/* LOUT1 Control (0x01) */
#define CM710X_LOUT1_L_MUTE			(0x1 << 15)
#define CM710X_LOUT1_L_MUTE_SFT			(15)
#define CM710X_LOUT1_L_DF			(0x1 << 14)
#define CM710X_LOUT1_L_DF_SFT			(14)
#define CM710X_LOUT2_L_MUTE			(0x1 << 13)
#define CM710X_LOUT2_L_MUTE_SFT			(13)
#define CM710X_LOUT2_L_DF			(0x1 << 12)
#define CM710X_LOUT2_L_DF_SFT			(12)
#define CM710X_LOUT3_L_MUTE			(0x1 << 11)
#define CM710X_LOUT3_L_MUTE_SFT			(11)
#define CM710X_LOUT3_L_DF			(0x1 << 10)
#define CM710X_LOUT3_L_DF_SFT			(10)
#define CM710X_LOUT1_ENH_DRV			(0x1 << 9)
#define CM710X_LOUT1_ENH_DRV_SFT		(9)
#define CM710X_LOUT2_ENH_DRV			(0x1 << 8)
#define CM710X_LOUT2_ENH_DRV_SFT		(8)
#define CM710X_LOUT3_ENH_DRV			(0x1 << 7)
#define CM710X_LOUT3_ENH_DRV_SFT		(7)

/* IN1 Control (0x03) */
#define CM710X_BST_MASK1			(0xf << 12)
#define CM710X_BST_SFT1				12
#define CM710X_BST_MASK2			(0xf << 8)
#define CM710X_BST_SFT2				8
#define CM710X_IN_DF1				(0x1 << 7)
#define CM710X_IN_DF1_SFT			7
#define CM710X_IN_DF2				(0x1 << 6)
#define CM710X_IN_DF2_SFT			6

/* Micbias Control (0x04) */
#define CM710X_MICBIAS1_OUTVOLT_MASK		(0x1 << 15)
#define CM710X_MICBIAS1_OUTVOLT_SFT		(15)
#define CM710X_MICBIAS1_OUTVOLT_2_7V		(0x0 << 15)
#define CM710X_MICBIAS1_OUTVOLT_2_25V		(0x1 << 15)
#define CM710X_MICBIAS1_CTRL_VDD_MASK		(0x1 << 14)
#define CM710X_MICBIAS1_CTRL_VDD_SFT		(14)
#define CM710X_MICBIAS1_CTRL_VDD_1_8V		(0x0 << 14)
#define CM710X_MICBIAS1_CTRL_VDD_3_3V		(0x1 << 14)
#define CM710X_MICBIAS1_OVCD_MASK		(0x1 << 11)
#define CM710X_MICBIAS1_OVCD_SHIFT		(11)
#define CM710X_MICBIAS1_OVCD_DIS		(0x0 << 11)
#define CM710X_MICBIAS1_OVCD_EN			(0x1 << 11)
#define CM710X_MICBIAS1_OVTH_MASK		(0x3 << 9)
#define CM710X_MICBIAS1_OVTH_SFT		9
#define CM710X_MICBIAS1_OVTH_640UA		(0x0 << 9)
#define CM710X_MICBIAS1_OVTH_1280UA		(0x1 << 9)
#define CM710X_MICBIAS1_OVTH_1920UA		(0x2 << 9)

/* Sidetone Control (0x13) */
#define CM710X_ST_HPF_SEL_MASK			(0x7 << 13)
#define CM710X_ST_HPF_SEL_SFT			13
#define CM710X_ST_HPF_PATH			(0x1 << 12)
#define CM710X_ST_HPF_PATH_SFT			12
#define CM710X_ST_SEL_MASK			(0x7 << 9)
#define CM710X_ST_SEL_SFT			9
#define CM710X_ST_EN				(0x1 << 6)
#define CM710X_ST_EN_SFT			6
#define CM710X_ST_GAIN				(0x1 << 5)
#define CM710X_ST_GAIN_SFT			5
#define CM710X_ST_VOL_MASK			(0x1f << 0)
#define CM710X_ST_VOL_SFT			0

/* Analog DAC1/2/3 Source Control (0x15) */
#define CM710X_ANA_DAC3_SRC_SEL_MASK		(0x3 << 4)
#define CM710X_ANA_DAC3_SRC_SEL_SFT		4
#define CM710X_ANA_DAC1_2_SRC_SEL_MASK		(0x3 << 0)
#define CM710X_ANA_DAC1_2_SRC_SEL_SFT		0

/* IF/DSP to DAC3/4 Mixer Control (0x16) */
#define CM710X_M_DAC4_L_VOL			(0x1 << 15)
#define CM710X_M_DAC4_L_VOL_SFT			15
#define CM710X_SEL_DAC4_L_SRC_MASK		(0x7 << 12)
#define CM710X_SEL_DAC4_L_SRC_SFT		12
#define CM710X_M_DAC4_R_VOL			(0x1 << 11)
#define CM710X_M_DAC4_R_VOL_SFT			11
#define CM710X_SEL_DAC4_R_SRC_MASK		(0x7 << 8)
#define CM710X_SEL_DAC4_R_SRC_SFT		8
#define CM710X_M_DAC3_L_VOL			(0x1 << 7)
#define CM710X_M_DAC3_L_VOL_SFT			7
#define CM710X_SEL_DAC3_L_SRC_MASK		(0x7 << 4)
#define CM710X_SEL_DAC3_L_SRC_SFT		4
#define CM710X_M_DAC3_R_VOL			(0x1 << 3)
#define CM710X_M_DAC3_R_VOL_SFT			3
#define CM710X_SEL_DAC3_R_SRC_MASK		(0x7 << 0)
#define CM710X_SEL_DAC3_R_SRC_SFT		0

/* DAC4 Digital Volume (0x17) */
#define CM710X_DAC4_L_VOL_MASK			(0xff << 8)
#define CM710X_DAC4_L_VOL_SFT			8
#define CM710X_DAC4_R_VOL_MASK			(0xff)
#define CM710X_DAC4_R_VOL_SFT			0

/* DAC3 Digital Volume (0x18) */
#define CM710X_DAC3_L_VOL_MASK			(0xff << 8)
#define CM710X_DAC3_L_VOL_SFT			8
#define CM710X_DAC3_R_VOL_MASK			(0xff)
#define CM710X_DAC3_R_VOL_SFT			0

/* DAC3 Digital Volume (0x19) */
#define CM710X_DAC1_L_VOL_MASK			(0xff << 8)
#define CM710X_DAC1_L_VOL_SFT			8
#define CM710X_DAC1_R_VOL_MASK			(0xff)
#define CM710X_DAC1_R_VOL_SFT			0

/* DAC2 Digital Volume (0x1a) */
#define CM710X_DAC2_L_VOL_MASK			(0xff << 8)
#define CM710X_DAC2_L_VOL_SFT			8
#define CM710X_DAC2_R_VOL_MASK			(0xff)
#define CM710X_DAC2_R_VOL_SFT			0

/* IF/DSP to DAC2 Mixer Control (0x1b) */
#define CM710X_M_DAC2_L_VOL			(0x1 << 7)
#define CM710X_M_DAC2_L_VOL_SFT			7
#define CM710X_SEL_DAC2_L_SRC_MASK		(0x7 << 4)
#define CM710X_SEL_DAC2_L_SRC_SFT		4
#define CM710X_M_DAC2_R_VOL			(0x1 << 3)
#define CM710X_M_DAC2_R_VOL_SFT			3
#define CM710X_SEL_DAC2_R_SRC_MASK		(0x7 << 0)
#define CM710X_SEL_DAC2_R_SRC_SFT		0

/* Stereo1 ADC Digital Volume Control (0x1c) */
#define CM710X_STO1_ADC_L_VOL_MASK		(0x3f << 9)
#define CM710X_STO1_ADC_L_VOL_SFT		9
#define CM710X_STO1_ADC_R_VOL_MASK		(0x3f << 1)
#define CM710X_STO1_ADC_R_VOL_SFT		1

/* Mono ADC Digital Volume Control (0x1d) */
#define CM710X_MONO_ADC_L_VOL_MASK		(0x3f << 9)
#define CM710X_MONO_ADC_L_VOL_SFT		9
#define CM710X_MONO_ADC_R_VOL_MASK		(0x3f << 1)
#define CM710X_MONO_ADC_R_VOL_SFT		1

/* Stereo 1/2 ADC Boost Gain Control (0x1e) */
#define CM710X_STO1_ADC_L_BST_MASK		(0x3 << 14)
#define CM710X_STO1_ADC_L_BST_SFT		14
#define CM710X_STO1_ADC_R_BST_MASK		(0x3 << 12)
#define CM710X_STO1_ADC_R_BST_SFT		12
#define CM710X_STO1_ADC_COMP_MASK		(0x3 << 10)
#define CM710X_STO1_ADC_COMP_SFT		10
#define CM710X_STO2_ADC_L_BST_MASK		(0x3 << 8)
#define CM710X_STO2_ADC_L_BST_SFT		8
#define CM710X_STO2_ADC_R_BST_MASK		(0x3 << 6)
#define CM710X_STO2_ADC_R_BST_SFT		6
#define CM710X_STO2_ADC_COMP_MASK		(0x3 << 4)
#define CM710X_STO2_ADC_COMP_SFT		4

/* Stereo2 ADC Digital Volume Control (0x1f) */
#define CM710X_STO2_ADC_L_VOL_MASK		(0x7f << 8)
#define CM710X_STO2_ADC_L_VOL_SFT		8
#define CM710X_STO2_ADC_R_VOL_MASK		(0x7f)
#define CM710X_STO2_ADC_R_VOL_SFT		0

/* ADC Boost Gain Control 2 (0x20) */
#define CM710X_MONO_ADC_L_BST_MASK		(0x3 << 14)
#define CM710X_MONO_ADC_L_BST_SFT		14
#define CM710X_MONO_ADC_R_BST_MASK		(0x3 << 12)
#define CM710X_MONO_ADC_R_BST_SFT		12
#define CM710X_MONO_ADC_COMP_MASK		(0x3 << 10)
#define CM710X_MONO_ADC_COMP_SFT		10

/* Stereo 3/4 ADC Boost Gain Control (0x21) */
#define CM710X_STO3_ADC_L_BST_MASK		(0x3 << 14)
#define CM710X_STO3_ADC_L_BST_SFT		14
#define CM710X_STO3_ADC_R_BST_MASK		(0x3 << 12)
#define CM710X_STO3_ADC_R_BST_SFT		12
#define CM710X_STO3_ADC_COMP_MASK		(0x3 << 10)
#define CM710X_STO3_ADC_COMP_SFT		10
#define CM710X_STO4_ADC_L_BST_MASK		(0x3 << 8)
#define CM710X_STO4_ADC_L_BST_SFT		8
#define CM710X_STO4_ADC_R_BST_MASK		(0x3 << 6)
#define CM710X_STO4_ADC_R_BST_SFT		6
#define CM710X_STO4_ADC_COMP_MASK		(0x3 << 4)
#define CM710X_STO4_ADC_COMP_SFT		4

/* Stereo3 ADC Digital Volume Control (0x22) */
#define CM710X_STO3_ADC_L_VOL_MASK		(0x7f << 8)
#define CM710X_STO3_ADC_L_VOL_SFT		8
#define CM710X_STO3_ADC_R_VOL_MASK		(0x7f)
#define CM710X_STO3_ADC_R_VOL_SFT		0

/* Stereo4 ADC Digital Volume Control (0x23) */
#define CM710X_STO4_ADC_L_VOL_MASK		(0x7f << 8)
#define CM710X_STO4_ADC_L_VOL_SFT		8
#define CM710X_STO4_ADC_R_VOL_MASK		(0x7f)
#define CM710X_STO4_ADC_R_VOL_SFT		0

/* Stereo4 ADC Mixer control (0x24) */
#define CM710X_M_STO4_ADC_L2			(0x1 << 15)
#define CM710X_M_STO4_ADC_L2_SFT		15
#define CM710X_M_STO4_ADC_L1			(0x1 << 14)
#define CM710X_M_STO4_ADC_L1_SFT		14
#define CM710X_SEL_STO4_ADC1_MASK		(0x3 << 12)
#define CM710X_SEL_STO4_ADC1_SFT		12
#define CM710X_SEL_STO4_ADC2_MASK		(0x3 << 10)
#define CM710X_SEL_STO4_ADC2_SFT		10
#define CM710X_SEL_STO4_DMIC_MASK		(0x3 << 8)
#define CM710X_SEL_STO4_DMIC_SFT		8
#define CM710X_M_STO4_ADC_R1			(0x1 << 7)
#define CM710X_M_STO4_ADC_R1_SFT		7
#define CM710X_M_STO4_ADC_R2			(0x1 << 6)
#define CM710X_M_STO4_ADC_R2_SFT		6

/* Stereo3 ADC Mixer control (0x25) */
#define CM710X_M_STO3_ADC_L2			(0x1 << 15)
#define CM710X_M_STO3_ADC_L2_SFT		15
#define CM710X_M_STO3_ADC_L1			(0x1 << 14)
#define CM710X_M_STO3_ADC_L1_SFT		14
#define CM710X_SEL_STO3_ADC1_MASK		(0x3 << 12)
#define CM710X_SEL_STO3_ADC1_SFT		12
#define CM710X_SEL_STO3_ADC2_MASK		(0x3 << 10)
#define CM710X_SEL_STO3_ADC2_SFT		10
#define CM710X_SEL_STO3_DMIC_MASK		(0x3 << 8)
#define CM710X_SEL_STO3_DMIC_SFT		8
#define CM710X_M_STO3_ADC_R1			(0x1 << 7)
#define CM710X_M_STO3_ADC_R1_SFT		7
#define CM710X_M_STO3_ADC_R2			(0x1 << 6)
#define CM710X_M_STO3_ADC_R2_SFT		6

/* Stereo2 ADC Mixer Control (0x26) */
#define CM710X_M_STO2_ADC_L2			(0x1 << 15)
#define CM710X_M_STO2_ADC_L2_SFT		15
#define CM710X_M_STO2_ADC_L1			(0x1 << 14)
#define CM710X_M_STO2_ADC_L1_SFT		14
#define CM710X_SEL_STO2_ADC1_MASK		(0x3 << 12)
#define CM710X_SEL_STO2_ADC1_SFT		12
#define CM710X_SEL_STO2_ADC2_MASK		(0x3 << 10)
#define CM710X_SEL_STO2_ADC2_SFT		10
#define CM710X_SEL_STO2_DMIC_MASK		(0x3 << 8)
#define CM710X_SEL_STO2_DMIC_SFT		8
#define CM710X_M_STO2_ADC_R1			(0x1 << 7)
#define CM710X_M_STO2_ADC_R1_SFT		7
#define CM710X_M_STO2_ADC_R2			(0x1 << 6)
#define CM710X_M_STO2_ADC_R2_SFT		6
#define CM710X_SEL_STO2_LR_MIX_MASK		(0x1 << 0)
#define CM710X_SEL_STO2_LR_MIX_SFT		0
#define CM710X_SEL_STO2_LR_MIX_L		(0x0 << 0)
#define CM710X_SEL_STO2_LR_MIX_LR		(0x1 << 0)

/* Stereo1 ADC Mixer control (0x27) */
#define CM710X_M_STO1_ADC_L2			(0x1 << 15)
#define CM710X_M_STO1_ADC_L2_SFT		15
#define CM710X_M_STO1_ADC_L1			(0x1 << 14)
#define CM710X_M_STO1_ADC_L1_SFT		14
#define CM710X_SEL_STO1_ADC1_MASK		(0x3 << 12)
#define CM710X_SEL_STO1_ADC1_SFT		12
#define CM710X_SEL_STO1_ADC2_MASK		(0x3 << 10)
#define CM710X_SEL_STO1_ADC2_SFT		10
#define CM710X_SEL_STO1_DMIC_MASK		(0x3 << 8)
#define CM710X_SEL_STO1_DMIC_SFT		8
#define CM710X_M_STO1_ADC_R1			(0x1 << 7)
#define CM710X_M_STO1_ADC_R1_SFT		7
#define CM710X_M_STO1_ADC_R2			(0x1 << 6)
#define CM710X_M_STO1_ADC_R2_SFT		6

/* Mono ADC Mixer control (0x28) */
#define CM710X_M_MONO_ADC_L2			(0x1 << 15)
#define CM710X_M_MONO_ADC_L2_SFT		15
#define CM710X_M_MONO_ADC_L1			(0x1 << 14)
#define CM710X_M_MONO_ADC_L1_SFT		14
#define CM710X_SEL_MONO_ADC_L1_MASK		(0x3 << 12)
#define CM710X_SEL_MONO_ADC_L1_SFT		12
#define CM710X_SEL_MONO_ADC_L2_MASK		(0x3 << 10)
#define CM710X_SEL_MONO_ADC_L2_SFT		10
#define CM710X_SEL_MONO_DMIC_L_MASK		(0x3 << 8)
#define CM710X_SEL_MONO_DMIC_L_SFT		8
#define CM710X_M_MONO_ADC_R1			(0x1 << 7)
#define CM710X_M_MONO_ADC_R1_SFT		7
#define CM710X_M_MONO_ADC_R2			(0x1 << 6)
#define CM710X_M_MONO_ADC_R2_SFT		6
#define CM710X_SEL_MONO_ADC_R1_MASK		(0x3 << 4)
#define CM710X_SEL_MONO_ADC_R1_SFT		4
#define CM710X_SEL_MONO_ADC_R2_MASK		(0x3 << 2)
#define CM710X_SEL_MONO_ADC_R2_SFT		2
#define CM710X_SEL_MONO_DMIC_R_MASK		(0x3 << 0)
#define CM710X_SEL_MONO_DMIC_R_SFT		0

/* ADC/IF/DSP to DAC1 Mixer control (0x29) */
#define CM710X_M_ADDA_MIXER1_L			(0x1 << 15)
#define CM710X_M_ADDA_MIXER1_L_SFT		15
#define CM710X_M_DAC1_L				(0x1 << 14)
#define CM710X_M_DAC1_L_SFT			14
#define CM710X_DAC1_L_SEL_MASK			(0x7 << 8)
#define CM710X_DAC1_L_SEL_SFT			8
#define CM710X_M_ADDA_MIXER1_R			(0x1 << 7)
#define CM710X_M_ADDA_MIXER1_R_SFT		7
#define CM710X_M_DAC1_R				(0x1 << 6)
#define CM710X_M_DAC1_R_SFT			6
#define CM710X_ADDA1_SEL_MASK			(0x3 << 0)
#define CM710X_ADDA1_SEL_SFT			0

/* Stereo1 DAC Mixer L/R Control (0x2a) */
#define CM710X_M_ST_DAC1_L			(0x1 << 15)
#define CM710X_M_ST_DAC1_L_SFT			15
#define CM710X_M_DAC1_L_STO_L			(0x1 << 13)
#define CM710X_M_DAC1_L_STO_L_SFT		13
#define CM710X_DAC1_L_STO_L_VOL_MASK		(0x1 << 12)
#define CM710X_DAC1_L_STO_L_VOL_SFT		12
#define CM710X_M_DAC2_L_STO_L			(0x1 << 11)
#define CM710X_M_DAC2_L_STO_L_SFT		11
#define CM710X_DAC2_L_STO_L_VOL_MASK		(0x1 << 10)
#define CM710X_DAC2_L_STO_L_VOL_SFT		10
#define CM710X_M_DAC1_R_STO_L			(0x1 << 9)
#define CM710X_M_DAC1_R_STO_L_SFT		9
#define CM710X_DAC1_R_STO_L_VOL_MASK		(0x1 << 8)
#define CM710X_DAC1_R_STO_L_VOL_SFT		8
#define CM710X_M_ST_DAC1_R			(0x1 << 7)
#define CM710X_M_ST_DAC1_R_SFT			7
#define CM710X_M_DAC1_R_STO_R			(0x1 << 5)
#define CM710X_M_DAC1_R_STO_R_SFT		5
#define CM710X_DAC1_R_STO_R_VOL_MASK		(0x1 << 4)
#define CM710X_DAC1_R_STO_R_VOL_SFT		4
#define CM710X_M_DAC2_R_STO_R			(0x1 << 3)
#define CM710X_M_DAC2_R_STO_R_SFT		3
#define CM710X_DAC2_R_STO_R_VOL_MASK		(0x1 << 2)
#define CM710X_DAC2_R_STO_R_VOL_SFT		2
#define CM710X_M_DAC1_L_STO_R			(0x1 << 1)
#define CM710X_M_DAC1_L_STO_R_SFT		1
#define CM710X_DAC1_L_STO_R_VOL_MASK		(0x1 << 0)
#define CM710X_DAC1_L_STO_R_VOL_SFT		0

/* Mono DAC Mixer L/R Control (0x2b) */
#define CM710X_M_ST_DAC2_L			(0x1 << 15)
#define CM710X_M_ST_DAC2_L_SFT			15
#define CM710X_M_DAC2_L_MONO_L			(0x1 << 13)
#define CM710X_M_DAC2_L_MONO_L_SFT		13
#define CM710X_DAC2_L_MONO_L_VOL_MASK		(0x1 << 12)
#define CM710X_DAC2_L_MONO_L_VOL_SFT		12
#define CM710X_M_DAC2_R_MONO_L			(0x1 << 11)
#define CM710X_M_DAC2_R_MONO_L_SFT		11
#define CM710X_DAC2_R_MONO_L_VOL_MASK		(0x1 << 10)
#define CM710X_DAC2_R_MONO_L_VOL_SFT		10
#define CM710X_M_DAC1_L_MONO_L			(0x1 << 9)
#define CM710X_M_DAC1_L_MONO_L_SFT		9
#define CM710X_DAC1_L_MONO_L_VOL_MASK		(0x1 << 8)
#define CM710X_DAC1_L_MONO_L_VOL_SFT		8
#define CM710X_M_ST_DAC2_R			(0x1 << 7)
#define CM710X_M_ST_DAC2_R_SFT			7
#define CM710X_M_DAC2_R_MONO_R			(0x1 << 5)
#define CM710X_M_DAC2_R_MONO_R_SFT		5
#define CM710X_DAC2_R_MONO_R_VOL_MASK		(0x1 << 4)
#define CM710X_DAC2_R_MONO_R_VOL_SFT		4
#define CM710X_M_DAC1_R_MONO_R			(0x1 << 3)
#define CM710X_M_DAC1_R_MONO_R_SFT		3
#define CM710X_DAC1_R_MONO_R_VOL_MASK		(0x1 << 2)
#define CM710X_DAC1_R_MONO_R_VOL_SFT		2
#define CM710X_M_DAC2_L_MONO_R			(0x1 << 1)
#define CM710X_M_DAC2_L_MONO_R_SFT		1
#define CM710X_DAC2_L_MONO_R_VOL_MASK		(0x1 << 0)
#define CM710X_DAC2_L_MONO_R_VOL_SFT		0

/* DD Mixer 1 Control (0x2c) */
#define CM710X_M_STO_L_DD1_L			(0x1 << 15)
#define CM710X_M_STO_L_DD1_L_SFT		15
#define CM710X_STO_L_DD1_L_VOL_MASK		(0x1 << 14)
#define CM710X_STO_L_DD1_L_VOL_SFT		14
#define CM710X_M_MONO_L_DD1_L			(0x1 << 13)
#define CM710X_M_MONO_L_DD1_L_SFT		13
#define CM710X_MONO_L_DD1_L_VOL_MASK		(0x1 << 12)
#define CM710X_MONO_L_DD1_L_VOL_SFT		12
#define CM710X_M_DAC3_L_DD1_L			(0x1 << 11)
#define CM710X_M_DAC3_L_DD1_L_SFT		11
#define CM710X_DAC3_L_DD1_L_VOL_MASK		(0x1 << 10)
#define CM710X_DAC3_L_DD1_L_VOL_SFT		10
#define CM710X_M_DAC3_R_DD1_L			(0x1 << 9)
#define CM710X_M_DAC3_R_DD1_L_SFT		9
#define CM710X_DAC3_R_DD1_L_VOL_MASK		(0x1 << 8)
#define CM710X_DAC3_R_DD1_L_VOL_SFT		8
#define CM710X_M_STO_R_DD1_R			(0x1 << 7)
#define CM710X_M_STO_R_DD1_R_SFT		7
#define CM710X_STO_R_DD1_R_VOL_MASK		(0x1 << 6)
#define CM710X_STO_R_DD1_R_VOL_SFT		6
#define CM710X_M_MONO_R_DD1_R			(0x1 << 5)
#define CM710X_M_MONO_R_DD1_R_SFT		5
#define CM710X_MONO_R_DD1_R_VOL_MASK		(0x1 << 4)
#define CM710X_MONO_R_DD1_R_VOL_SFT		4
#define CM710X_M_DAC3_R_DD1_R			(0x1 << 3)
#define CM710X_M_DAC3_R_DD1_R_SFT		3
#define CM710X_DAC3_R_DD1_R_VOL_MASK		(0x1 << 2)
#define CM710X_DAC3_R_DD1_R_VOL_SFT		2
#define CM710X_M_DAC3_L_DD1_R			(0x1 << 1)
#define CM710X_M_DAC3_L_DD1_R_SFT		1
#define CM710X_DAC3_L_DD1_R_VOL_MASK		(0x1 << 0)
#define CM710X_DAC3_L_DD1_R_VOL_SFT		0

/* DD Mixer 2 Control (0x2d) */
#define CM710X_M_STO_L_DD2_L			(0x1 << 15)
#define CM710X_M_STO_L_DD2_L_SFT		15
#define CM710X_STO_L_DD2_L_VOL_MASK		(0x1 << 14)
#define CM710X_STO_L_DD2_L_VOL_SFT		14
#define CM710X_M_MONO_L_DD2_L			(0x1 << 13)
#define CM710X_M_MONO_L_DD2_L_SFT		13
#define CM710X_MONO_L_DD2_L_VOL_MASK		(0x1 << 12)
#define CM710X_MONO_L_DD2_L_VOL_SFT		12
#define CM710X_M_DAC4_L_DD2_L			(0x1 << 11)
#define CM710X_M_DAC4_L_DD2_L_SFT		11
#define CM710X_DAC4_L_DD2_L_VOL_MASK		(0x1 << 10)
#define CM710X_DAC4_L_DD2_L_VOL_SFT		10
#define CM710X_M_DAC4_R_DD2_L			(0x1 << 9)
#define CM710X_M_DAC4_R_DD2_L_SFT		9
#define CM710X_DAC4_R_DD2_L_VOL_MASK		(0x1 << 8)
#define CM710X_DAC4_R_DD2_L_VOL_SFT		8
#define CM710X_M_STO_R_DD2_R			(0x1 << 7)
#define CM710X_M_STO_R_DD2_R_SFT		7
#define CM710X_STO_R_DD2_R_VOL_MASK		(0x1 << 6)
#define CM710X_STO_R_DD2_R_VOL_SFT		6
#define CM710X_M_MONO_R_DD2_R			(0x1 << 5)
#define CM710X_M_MONO_R_DD2_R_SFT		5
#define CM710X_MONO_R_DD2_R_VOL_MASK		(0x1 << 4)
#define CM710X_MONO_R_DD2_R_VOL_SFT		4
#define CM710X_M_DAC4_R_DD2_R			(0x1 << 3)
#define CM710X_M_DAC4_R_DD2_R_SFT		3
#define CM710X_DAC4_R_DD2_R_VOL_MASK		(0x1 << 2)
#define CM710X_DAC4_R_DD2_R_VOL_SFT		2
#define CM710X_M_DAC4_L_DD2_R			(0x1 << 1)
#define CM710X_M_DAC4_L_DD2_R_SFT		1
#define CM710X_DAC4_L_DD2_R_VOL_MASK		(0x1 << 0)
#define CM710X_DAC4_L_DD2_R_VOL_SFT		0

/* IF3 data control (0x2f) */
#define CM710X_IF3_DAC_SEL_MASK			(0x3 << 6)
#define CM710X_IF3_DAC_SEL_SFT			6
#define CM710X_IF3_ADC_SEL_MASK			(0x3 << 4)
#define CM710X_IF3_ADC_SEL_SFT			4
#define CM710X_IF3_ADC_IN_MASK			(0xf << 0)
#define CM710X_IF3_ADC_IN_SFT			0

/* IF4 data control (0x30) */
#define CM710X_IF4_ADC_IN_MASK			(0xf << 4)
#define CM710X_IF4_ADC_IN_SFT			4
#define CM710X_IF4_DAC_SEL_MASK			(0x3 << 2)
#define CM710X_IF4_DAC_SEL_SFT			2
#define CM710X_IF4_ADC_SEL_MASK			(0x3 << 0)
#define CM710X_IF4_ADC_SEL_SFT			0

/* PDM Output Control (0x31) */
#define CM710X_M_PDM1_L				(0x1 << 15)
#define CM710X_M_PDM1_L_SFT			15
#define CM710X_SEL_PDM1_L_MASK			(0x3 << 12)
#define CM710X_SEL_PDM1_L_SFT			12
#define CM710X_M_PDM1_R				(0x1 << 11)
#define CM710X_M_PDM1_R_SFT			11
#define CM710X_SEL_PDM1_R_MASK			(0x3 << 8)
#define CM710X_SEL_PDM1_R_SFT			8
#define CM710X_M_PDM2_L				(0x1 << 7)
#define CM710X_M_PDM2_L_SFT			7
#define CM710X_SEL_PDM2_L_MASK			(0x3 << 4)
#define CM710X_SEL_PDM2_L_SFT			4
#define CM710X_M_PDM2_R				(0x1 << 3)
#define CM710X_M_PDM2_R_SFT			3
#define CM710X_SEL_PDM2_R_MASK			(0x3 << 0)
#define CM710X_SEL_PDM2_R_SFT			0

/* PDM I2C / Data Control 1 (0x32) */
#define CM710X_PDM2_PW_DOWN			(0x1 << 7)
#define CM710X_PDM1_PW_DOWN			(0x1 << 6)
#define CM710X_PDM2_BUSY			(0x1 << 5)
#define CM710X_PDM1_BUSY			(0x1 << 4)
#define CM710X_PDM_PATTERN			(0x1 << 3)
#define CM710X_PDM_GAIN				(0x1 << 2)
#define CM710X_PDM_DIV_MASK			(0x3 << 0)

/* PDM I2C / Data Control 2 (0x33) */
#define CM710X_PDM1_I2C_ID			(0xf << 12)
#define CM710X_PDM1_EXE				(0x1 << 11)
#define CM710X_PDM1_I2C_CMD			(0x1 << 10)
#define CM710X_PDM1_I2C_EXE			(0x1 << 9)
#define CM710X_PDM1_I2C_BUSY			(0x1 << 8)
#define CM710X_PDM2_I2C_ID			(0xf << 4)
#define CM710X_PDM2_EXE				(0x1 << 3)
#define CM710X_PDM2_I2C_CMD			(0x1 << 2)
#define CM710X_PDM2_I2C_EXE			(0x1 << 1)
#define CM710X_PDM2_I2C_BUSY			(0x1 << 0)

/* TDM1 control 1 (0x3b) */
#define CM710X_IF1_ADC_MODE_MASK		(0x1 << 12)
#define CM710X_IF1_ADC_MODE_SFT			12
#define CM710X_IF1_ADC_MODE_I2S			(0x0 << 12)
#define CM710X_IF1_ADC_MODE_TDM			(0x1 << 12)
#define CM710X_IF1_ADC1_SWAP_MASK		(0x3 << 6)
#define CM710X_IF1_ADC1_SWAP_SFT		6
#define CM710X_IF1_ADC2_SWAP_MASK		(0x3 << 4)
#define CM710X_IF1_ADC2_SWAP_SFT		4
#define CM710X_IF1_ADC3_SWAP_MASK		(0x3 << 2)
#define CM710X_IF1_ADC3_SWAP_SFT		2
#define CM710X_IF1_ADC4_SWAP_MASK		(0x3 << 0)
#define CM710X_IF1_ADC4_SWAP_SFT		0

/* TDM1 control 2 (0x3c) */
#define CM710X_IF1_ADC4_MASK			(0x3 << 10)
#define CM710X_IF1_ADC4_SFT			10
#define CM710X_IF1_ADC3_MASK			(0x3 << 8)
#define CM710X_IF1_ADC3_SFT			8
#define CM710X_IF1_ADC2_MASK			(0x3 << 6)
#define CM710X_IF1_ADC2_SFT			6
#define CM710X_IF1_ADC1_MASK			(0x3 << 4)
#define CM710X_IF1_ADC1_SFT			4
#define CM710X_IF1_ADC_CTRL_MASK		(0x7 << 0)
#define CM710X_IF1_ADC_CTRL_SFT			0

/* TDM1 control 4 (0x3e) */
#define CM710X_IF1_DAC0_MASK			(0x7 << 12)
#define CM710X_IF1_DAC0_SFT			12
#define CM710X_IF1_DAC1_MASK			(0x7 << 8)
#define CM710X_IF1_DAC1_SFT			8
#define CM710X_IF1_DAC2_MASK			(0x7 << 4)
#define CM710X_IF1_DAC2_SFT			4
#define CM710X_IF1_DAC3_MASK			(0x7 << 0)
#define CM710X_IF1_DAC3_SFT			0

/* TDM1 control 5 (0x3f) */
#define CM710X_IF1_DAC4_MASK			(0x7 << 12)
#define CM710X_IF1_DAC4_SFT			12
#define CM710X_IF1_DAC5_MASK			(0x7 << 8)
#define CM710X_IF1_DAC5_SFT			8
#define CM710X_IF1_DAC6_MASK			(0x7 << 4)
#define CM710X_IF1_DAC6_SFT			4
#define CM710X_IF1_DAC7_MASK			(0x7 << 0)
#define CM710X_IF1_DAC7_SFT			0

/* TDM2 control 1 (0x40) */
#define CM710X_IF2_ADC_MODE_MASK		(0x1 << 12)
#define CM710X_IF2_ADC_MODE_SFT			12
#define CM710X_IF2_ADC_MODE_I2S			(0x0 << 12)
#define CM710X_IF2_ADC_MODE_TDM			(0x1 << 12)
#define CM710X_IF2_ADC1_SWAP_MASK		(0x3 << 6)
#define CM710X_IF2_ADC1_SWAP_SFT		6
#define CM710X_IF2_ADC2_SWAP_MASK		(0x3 << 4)
#define CM710X_IF2_ADC2_SWAP_SFT		4
#define CM710X_IF2_ADC3_SWAP_MASK		(0x3 << 2)
#define CM710X_IF2_ADC3_SWAP_SFT		2
#define CM710X_IF2_ADC4_SWAP_MASK		(0x3 << 0)
#define CM710X_IF2_ADC4_SWAP_SFT		0

/* TDM2 control 2 (0x41) */
#define CM710X_IF2_ADC4_MASK			(0x3 << 10)
#define CM710X_IF2_ADC4_SFT			10
#define CM710X_IF2_ADC3_MASK			(0x3 << 8)
#define CM710X_IF2_ADC3_SFT			8
#define CM710X_IF2_ADC2_MASK			(0x3 << 6)
#define CM710X_IF2_ADC2_SFT			6
#define CM710X_IF2_ADC1_MASK			(0x3 << 4)
#define CM710X_IF2_ADC1_SFT			4
#define CM710X_IF2_ADC_CTRL_MASK		(0x7 << 0)
#define CM710X_IF2_ADC_CTRL_SFT			0

/* TDM2 control 4 (0x43) */
#define CM710X_IF2_DAC0_MASK			(0x7 << 12)
#define CM710X_IF2_DAC0_SFT			12
#define CM710X_IF2_DAC1_MASK			(0x7 << 8)
#define CM710X_IF2_DAC1_SFT			8
#define CM710X_IF2_DAC2_MASK			(0x7 << 4)
#define CM710X_IF2_DAC2_SFT			4
#define CM710X_IF2_DAC3_MASK			(0x7 << 0)
#define CM710X_IF2_DAC3_SFT			0

/* TDM2 control 5 (0x44) */
#define CM710X_IF2_DAC4_MASK			(0x7 << 12)
#define CM710X_IF2_DAC4_SFT			12
#define CM710X_IF2_DAC5_MASK			(0x7 << 8)
#define CM710X_IF2_DAC5_SFT			8
#define CM710X_IF2_DAC6_MASK			(0x7 << 4)
#define CM710X_IF2_DAC6_SFT			4
#define CM710X_IF2_DAC7_MASK			(0x7 << 0)
#define CM710X_IF2_DAC7_SFT			0

/* Digital Microphone Control 1 (0x50) */
#define CM710X_DMIC_1_EN_MASK			(0x1 << 15)
#define CM710X_DMIC_1_EN_SFT			15
#define CM710X_DMIC_1_DIS			(0x0 << 15)
#define CM710X_DMIC_1_EN			(0x1 << 15)
#define CM710X_DMIC_2_EN_MASK			(0x1 << 14)
#define CM710X_DMIC_2_EN_SFT			14
#define CM710X_DMIC_2_DIS			(0x0 << 14)
#define CM710X_DMIC_2_EN			(0x1 << 14)
#define CM710X_DMIC_L_STO1_LH_MASK		(0x1 << 13)
#define CM710X_DMIC_L_STO1_LH_SFT		13
#define CM710X_DMIC_L_STO1_LH_FALLING		(0x0 << 13)
#define CM710X_DMIC_L_STO1_LH_RISING		(0x1 << 13)
#define CM710X_DMIC_R_STO1_LH_MASK		(0x1 << 12)
#define CM710X_DMIC_R_STO1_LH_SFT		12
#define CM710X_DMIC_R_STO1_LH_FALLING		(0x0 << 12)
#define CM710X_DMIC_R_STO1_LH_RISING		(0x1 << 12)
#define CM710X_DMIC_L_STO3_LH_MASK		(0x1 << 11)
#define CM710X_DMIC_L_STO3_LH_SFT		11
#define CM710X_DMIC_L_STO3_LH_FALLING		(0x0 << 11)
#define CM710X_DMIC_L_STO3_LH_RISING		(0x1 << 11)
#define CM710X_DMIC_R_STO3_LH_MASK		(0x1 << 10)
#define CM710X_DMIC_R_STO3_LH_SFT		10
#define CM710X_DMIC_R_STO3_LH_FALLING		(0x0 << 10)
#define CM710X_DMIC_R_STO3_LH_RISING		(0x1 << 10)
#define CM710X_DMIC_L_STO2_LH_MASK		(0x1 << 9)
#define CM710X_DMIC_L_STO2_LH_SFT		9
#define CM710X_DMIC_L_STO2_LH_FALLING		(0x0 << 9)
#define CM710X_DMIC_L_STO2_LH_RISING		(0x1 << 9)
#define CM710X_DMIC_R_STO2_LH_MASK		(0x1 << 8)
#define CM710X_DMIC_R_STO2_LH_SFT		8
#define CM710X_DMIC_R_STO2_LH_FALLING		(0x0 << 8)
#define CM710X_DMIC_R_STO2_LH_RISING		(0x1 << 8)
#define CM710X_DMIC_CLK_MASK			(0x7 << 5)
#define CM710X_DMIC_CLK_SFT			5
#define CM710X_DMIC_3_EN_MASK			(0x1 << 4)
#define CM710X_DMIC_3_EN_SFT			4
#define CM710X_DMIC_3_DIS			(0x0 << 4)
#define CM710X_DMIC_3_EN			(0x1 << 4)
#define CM710X_DMIC_R_MONO_LH_MASK		(0x1 << 2)
#define CM710X_DMIC_R_MONO_LH_SFT		2
#define CM710X_DMIC_R_MONO_LH_FALLING		(0x0 << 2)
#define CM710X_DMIC_R_MONO_LH_RISING		(0x1 << 2)
#define CM710X_DMIC_L_STO4_LH_MASK		(0x1 << 1)
#define CM710X_DMIC_L_STO4_LH_SFT		1
#define CM710X_DMIC_L_STO4_LH_FALLING		(0x0 << 1)
#define CM710X_DMIC_L_STO4_LH_RISING		(0x1 << 1)
#define CM710X_DMIC_R_STO4_LH_MASK		(0x1 << 0)
#define CM710X_DMIC_R_STO4_LH_SFT		0
#define CM710X_DMIC_R_STO4_LH_FALLING		(0x0 << 0)
#define CM710X_DMIC_R_STO4_LH_RISING		(0x1 << 0)

/* Digital Microphone Control 2 (0x51) */
#define CM710X_DMIC_4_EN_MASK			(0x1 << 15)
#define CM710X_DMIC_4_EN_SFT			15
#define CM710X_DMIC_4_DIS			(0x0 << 15)
#define CM710X_DMIC_4_EN			(0x1 << 15)
#define CM710X_DMIC_4L_LH_MASK			(0x1 << 7)
#define CM710X_DMIC_4L_LH_SFT			7
#define CM710X_DMIC_4L_LH_FALLING		(0x0 << 7)
#define CM710X_DMIC_4L_LH_RISING		(0x1 << 7)
#define CM710X_DMIC_4R_LH_MASK			(0x1 << 6)
#define CM710X_DMIC_4R_LH_SFT			6
#define CM710X_DMIC_4R_LH_FALLING		(0x0 << 6)
#define CM710X_DMIC_4R_LH_RISING		(0x1 << 6)
#define CM710X_DMIC_3L_LH_MASK			(0x1 << 5)
#define CM710X_DMIC_3L_LH_SFT			5
#define CM710X_DMIC_3L_LH_FALLING		(0x0 << 5)
#define CM710X_DMIC_3L_LH_RISING		(0x1 << 5)
#define CM710X_DMIC_3R_LH_MASK			(0x1 << 4)
#define CM710X_DMIC_3R_LH_SFT			4
#define CM710X_DMIC_3R_LH_FALLING		(0x0 << 4)
#define CM710X_DMIC_3R_LH_RISING		(0x1 << 4)
#define CM710X_DMIC_2L_LH_MASK			(0x1 << 3)
#define CM710X_DMIC_2L_LH_SFT			3
#define CM710X_DMIC_2L_LH_FALLING		(0x0 << 3)
#define CM710X_DMIC_2L_LH_RISING		(0x1 << 3)
#define CM710X_DMIC_2R_LH_MASK			(0x1 << 2)
#define CM710X_DMIC_2R_LH_SFT			2
#define CM710X_DMIC_2R_LH_FALLING		(0x0 << 2)
#define CM710X_DMIC_2R_LH_RISING		(0x1 << 2)
#define CM710X_DMIC_1L_LH_MASK			(0x1 << 1)
#define CM710X_DMIC_1L_LH_SFT			1
#define CM710X_DMIC_1L_LH_FALLING		(0x0 << 1)
#define CM710X_DMIC_1L_LH_RISING		(0x1 << 1)
#define CM710X_DMIC_1R_LH_MASK			(0x1 << 0)
#define CM710X_DMIC_1R_LH_SFT			0
#define CM710X_DMIC_1R_LH_FALLING		(0x0 << 0)
#define CM710X_DMIC_1R_LH_RISING		(0x1 << 0)

/* Power Management for Digital 1 (0x61) */
#define CM710X_PWR_I2S1				(0x1 << 15)
#define CM710X_PWR_I2S1_BIT			15
#define CM710X_PWR_I2S2				(0x1 << 14)
#define CM710X_PWR_I2S2_BIT			14
#define CM710X_PWR_I2S3				(0x1 << 13)
#define CM710X_PWR_I2S3_BIT			13
#define CM710X_PWR_DAC1				(0x1 << 12)
#define CM710X_PWR_DAC1_BIT			12
#define CM710X_PWR_DAC2				(0x1 << 11)
#define CM710X_PWR_DAC2_BIT			11
#define CM710X_PWR_I2S4				(0x1 << 10)
#define CM710X_PWR_I2S4_BIT			10
#define CM710X_PWR_SLB				(0x1 << 9)
#define CM710X_PWR_SLB_BIT			9
#define CM710X_PWR_DAC3				(0x1 << 7)
#define CM710X_PWR_DAC3_BIT			7
#define CM710X_PWR_ADCFED2			(0x1 << 4)
#define CM710X_PWR_ADCFED2_BIT			4
#define CM710X_PWR_ADCFED1			(0x1 << 3)
#define CM710X_PWR_ADCFED1_BIT			3
#define CM710X_PWR_ADC_L			(0x1 << 2)
#define CM710X_PWR_ADC_L_BIT			2
#define CM710X_PWR_ADC_R			(0x1 << 1)
#define CM710X_PWR_ADC_R_BIT			1
#define CM710X_PWR_I2C_MASTER			(0x1 << 0)
#define CM710X_PWR_I2C_MASTER_BIT		0

/* Power Management for Digital 2 (0x62) */
#define CM710X_PWR_ADC_S1F			(0x1 << 15)
#define CM710X_PWR_ADC_S1F_BIT			15
#define CM710X_PWR_ADC_MF_L			(0x1 << 14)
#define CM710X_PWR_ADC_MF_L_BIT			14
#define CM710X_PWR_ADC_MF_R			(0x1 << 13)
#define CM710X_PWR_ADC_MF_R_BIT			13
#define CM710X_PWR_DAC_S1F			(0x1 << 12)
#define CM710X_PWR_DAC_S1F_BIT			12
#define CM710X_PWR_DAC_M2F_L			(0x1 << 11)
#define CM710X_PWR_DAC_M2F_L_BIT		11
#define CM710X_PWR_DAC_M2F_R			(0x1 << 10)
#define CM710X_PWR_DAC_M2F_R_BIT		10
#define CM710X_PWR_DAC_M3F_L			(0x1 << 9)
#define CM710X_PWR_DAC_M3F_L_BIT		9
#define CM710X_PWR_DAC_M3F_R			(0x1 << 8)
#define CM710X_PWR_DAC_M3F_R_BIT		8
#define CM710X_PWR_DAC_M4F_L			(0x1 << 7)
#define CM710X_PWR_DAC_M4F_L_BIT		7
#define CM710X_PWR_DAC_M4F_R			(0x1 << 6)
#define CM710X_PWR_DAC_M4F_R_BIT		6
#define CM710X_PWR_ADC_S2F			(0x1 << 5)
#define CM710X_PWR_ADC_S2F_BIT			5
#define CM710X_PWR_ADC_S3F			(0x1 << 4)
#define CM710X_PWR_ADC_S3F_BIT			4
#define CM710X_PWR_ADC_S4F			(0x1 << 3)
#define CM710X_PWR_ADC_S4F_BIT			3
#define CM710X_PWR_PDM1				(0x1 << 2)
#define CM710X_PWR_PDM1_BIT			2
#define CM710X_PWR_PDM2				(0x1 << 1)
#define CM710X_PWR_PDM2_BIT			1

/* Power Management for Analog 1 (0x63) */
#define CM710X_PWR_VREF1			(0x1 << 15)
#define CM710X_PWR_VREF1_BIT			15
#define CM710X_PWR_FV1				(0x1 << 14)
#define CM710X_PWR_FV1_BIT			14
#define CM710X_PWR_MB				(0x1 << 13)
#define CM710X_PWR_MB_BIT			13
#define CM710X_PWR_LO1				(0x1 << 12)
#define CM710X_PWR_LO1_BIT			12
#define CM710X_PWR_BG				(0x1 << 11)
#define CM710X_PWR_BG_BIT			11
#define CM710X_PWR_LO2				(0x1 << 10)
#define CM710X_PWR_LO2_BIT			10
#define CM710X_PWR_LO3				(0x1 << 9)
#define CM710X_PWR_LO3_BIT			9
#define CM710X_PWR_VREF2			(0x1 << 8)
#define CM710X_PWR_VREF2_BIT			8
#define CM710X_PWR_FV2				(0x1 << 7)
#define CM710X_PWR_FV2_BIT			7
#define CM710X_LDO2_SEL_MASK			(0x7 << 4)
#define CM710X_LDO2_SEL_SFT			4
#define CM710X_LDO1_SEL_MASK			(0x7 << 0)
#define CM710X_LDO1_SEL_SFT			0

/* Power Management for Analog 2 (0x64) */
#define CM710X_PWR_BST1				(0x1 << 15)
#define CM710X_PWR_BST1_BIT			15
#define CM710X_PWR_BST2				(0x1 << 14)
#define CM710X_PWR_BST2_BIT			14
#define CM710X_PWR_CLK_MB1			(0x1 << 13)
#define CM710X_PWR_CLK_MB1_BIT			13
#define CM710X_PWR_SLIM				(0x1 << 12)
#define CM710X_PWR_SLIM_BIT			12
#define CM710X_PWR_MB1				(0x1 << 11)
#define CM710X_PWR_MB1_BIT			11
#define CM710X_PWR_PP_MB1			(0x1 << 10)
#define CM710X_PWR_PP_MB1_BIT			10
#define CM710X_PWR_PLL1				(0x1 << 9)
#define CM710X_PWR_PLL1_BIT			9
#define CM710X_PWR_PLL2				(0x1 << 8)
#define CM710X_PWR_PLL2_BIT			8
#define CM710X_PWR_CORE				(0x1 << 7)
#define CM710X_PWR_CORE_BIT			7
#define CM710X_PWR_CLK_MB			(0x1 << 6)
#define CM710X_PWR_CLK_MB_BIT			6
#define CM710X_PWR_BST1_P			(0x1 << 5)
#define CM710X_PWR_BST1_P_BIT			5
#define CM710X_PWR_BST2_P			(0x1 << 4)
#define CM710X_PWR_BST2_P_BIT			4
#define CM710X_PWR_IPTV				(0x1 << 3)
#define CM710X_PWR_IPTV_BIT			3
#define CM710X_PWR_25M_CLK			(0x1 << 1)
#define CM710X_PWR_25M_CLK_BIT			1
#define CM710X_PWR_LDO1				(0x1 << 0)
#define CM710X_PWR_LDO1_BIT			0

/* Power Management for DSP (0x65) */
#define CM710X_PWR_SR7				(0x1 << 10)
#define CM710X_PWR_SR7_BIT			10
#define CM710X_PWR_SR6				(0x1 << 9)
#define CM710X_PWR_SR6_BIT			9
#define CM710X_PWR_SR5				(0x1 << 8)
#define CM710X_PWR_SR5_BIT			8
#define CM710X_PWR_SR4				(0x1 << 7)
#define CM710X_PWR_SR4_BIT			7
#define CM710X_PWR_SR3				(0x1 << 6)
#define CM710X_PWR_SR3_BIT			6
#define CM710X_PWR_SR2				(0x1 << 5)
#define CM710X_PWR_SR2_BIT			5
#define CM710X_PWR_SR1				(0x1 << 4)
#define CM710X_PWR_SR1_BIT			4
#define CM710X_PWR_SR0				(0x1 << 3)
#define CM710X_PWR_SR0_BIT			3
#define CM710X_PWR_MLT				(0x1 << 2)
#define CM710X_PWR_MLT_BIT			2
#define CM710X_PWR_DSP				(0x1 << 1)
#define CM710X_PWR_DSP_BIT			1
#define CM710X_PWR_DSP_CPU			(0x1 << 0)
#define CM710X_PWR_DSP_CPU_BIT			0

/* Power Status for DSP (0x66) */
#define CM710X_PWR_SR7_RDY			(0x1 << 9)
#define CM710X_PWR_SR7_RDY_BIT			9
#define CM710X_PWR_SR6_RDY			(0x1 << 8)
#define CM710X_PWR_SR6_RDY_BIT			8
#define CM710X_PWR_SR5_RDY			(0x1 << 7)
#define CM710X_PWR_SR5_RDY_BIT			7
#define CM710X_PWR_SR4_RDY			(0x1 << 6)
#define CM710X_PWR_SR4_RDY_BIT			6
#define CM710X_PWR_SR3_RDY			(0x1 << 5)
#define CM710X_PWR_SR3_RDY_BIT			5
#define CM710X_PWR_SR2_RDY			(0x1 << 4)
#define CM710X_PWR_SR2_RDY_BIT			4
#define CM710X_PWR_SR1_RDY			(0x1 << 3)
#define CM710X_PWR_SR1_RDY_BIT			3
#define CM710X_PWR_SR0_RDY			(0x1 << 2)
#define CM710X_PWR_SR0_RDY_BIT			2
#define CM710X_PWR_MLT_RDY			(0x1 << 1)
#define CM710X_PWR_MLT_RDY_BIT			1
#define CM710X_PWR_DSP_RDY			(0x1 << 0)
#define CM710X_PWR_DSP_RDY_BIT			0

/* Power Management for DSP (0x67) */
#define CM710X_PWR_SLIM_ISO			(0x1 << 11)
#define CM710X_PWR_SLIM_ISO_BIT			11
#define CM710X_PWR_CORE_ISO			(0x1 << 10)
#define CM710X_PWR_CORE_ISO_BIT			10
#define CM710X_PWR_DSP_ISO			(0x1 << 9)
#define CM710X_PWR_DSP_ISO_BIT			9
#define CM710X_PWR_SR7_ISO			(0x1 << 8)
#define CM710X_PWR_SR7_ISO_BIT			8
#define CM710X_PWR_SR6_ISO			(0x1 << 7)
#define CM710X_PWR_SR6_ISO_BIT			7
#define CM710X_PWR_SR5_ISO			(0x1 << 6)
#define CM710X_PWR_SR5_ISO_BIT			6
#define CM710X_PWR_SR4_ISO			(0x1 << 5)
#define CM710X_PWR_SR4_ISO_BIT			5
#define CM710X_PWR_SR3_ISO			(0x1 << 4)
#define CM710X_PWR_SR3_ISO_BIT			4
#define CM710X_PWR_SR2_ISO			(0x1 << 3)
#define CM710X_PWR_SR2_ISO_BIT			3
#define CM710X_PWR_SR1_ISO			(0x1 << 2)
#define CM710X_PWR_SR1_ISO_BIT			2
#define CM710X_PWR_SR0_ISO			(0x1 << 1)
#define CM710X_PWR_SR0_ISO_BIT			1
#define CM710X_PWR_MLT_ISO			(0x1 << 0)
#define CM710X_PWR_MLT_ISO_BIT			0

/* I2S1/2/3/4 Audio Serial Data Port Control (0x6f 0x70 0x71 0x72) */
#define CM710X_I2S_MS_MASK			(0x1 << 15)
#define CM710X_I2S_MS_SFT			15
#define CM710X_I2S_MS_M				(0x0 << 15)
#define CM710X_I2S_MS_S				(0x1 << 15)
#define CM710X_I2S_O_CP_MASK			(0x3 << 10)
#define CM710X_I2S_O_CP_SFT			10
#define CM710X_I2S_O_CP_OFF			(0x0 << 10)
#define CM710X_I2S_O_CP_U_LAW			(0x1 << 10)
#define CM710X_I2S_O_CP_A_LAW			(0x2 << 10)
#define CM710X_I2S_I_CP_MASK			(0x3 << 8)
#define CM710X_I2S_I_CP_SFT			8
#define CM710X_I2S_I_CP_OFF			(0x0 << 8)
#define CM710X_I2S_I_CP_U_LAW			(0x1 << 8)
#define CM710X_I2S_I_CP_A_LAW			(0x2 << 8)
#define CM710X_I2S_BP_MASK			(0x1 << 7)
#define CM710X_I2S_BP_SFT			7
#define CM710X_I2S_BP_NOR			(0x0 << 7)
#define CM710X_I2S_BP_INV			(0x1 << 7)
#define CM710X_I2S_DL_MASK			(0x3 << 2)
#define CM710X_I2S_DL_SFT			2
#define CM710X_I2S_DL_16			(0x0 << 2)
#define CM710X_I2S_DL_20			(0x1 << 2)
#define CM710X_I2S_DL_24			(0x2 << 2)
#define CM710X_I2S_DL_8				(0x3 << 2)
#define CM710X_I2S_DF_MASK			(0x3 << 0)
#define CM710X_I2S_DF_SFT			0
#define CM710X_I2S_DF_I2S			(0x0 << 0)
#define CM710X_I2S_DF_LEFT			(0x1 << 0)
#define CM710X_I2S_DF_PCM_A			(0x2 << 0)
#define CM710X_I2S_DF_PCM_B			(0x3 << 0)

/* Clock Tree Control 1 (0x73) */
#define CM710X_I2S_PD1_MASK			(0x7 << 12)
#define CM710X_I2S_PD1_SFT			12
#define CM710X_I2S_PD1_1			(0x0 << 12)
#define CM710X_I2S_PD1_2			(0x1 << 12)
#define CM710X_I2S_PD1_3			(0x2 << 12)
#define CM710X_I2S_PD1_4			(0x3 << 12)
#define CM710X_I2S_PD1_6			(0x4 << 12)
#define CM710X_I2S_PD1_8			(0x5 << 12)
#define CM710X_I2S_PD1_12			(0x6 << 12)
#define CM710X_I2S_PD1_16			(0x7 << 12)
#define CM710X_I2S_BCLK_MS2_MASK		(0x1 << 11)
#define CM710X_I2S_BCLK_MS2_SFT			11
#define CM710X_I2S_BCLK_MS2_32			(0x0 << 11)
#define CM710X_I2S_BCLK_MS2_64			(0x1 << 11)
#define CM710X_I2S_PD2_MASK			(0x7 << 8)
#define CM710X_I2S_PD2_SFT			8
#define CM710X_I2S_PD2_1			(0x0 << 8)
#define CM710X_I2S_PD2_2			(0x1 << 8)
#define CM710X_I2S_PD2_3			(0x2 << 8)
#define CM710X_I2S_PD2_4			(0x3 << 8)
#define CM710X_I2S_PD2_6			(0x4 << 8)
#define CM710X_I2S_PD2_8			(0x5 << 8)
#define CM710X_I2S_PD2_12			(0x6 << 8)
#define CM710X_I2S_PD2_16			(0x7 << 8)
#define CM710X_I2S_BCLK_MS3_MASK		(0x1 << 7)
#define CM710X_I2S_BCLK_MS3_SFT			7
#define CM710X_I2S_BCLK_MS3_32			(0x0 << 7)
#define CM710X_I2S_BCLK_MS3_64			(0x1 << 7)
#define CM710X_I2S_PD3_MASK			(0x7 << 4)
#define CM710X_I2S_PD3_SFT			4
#define CM710X_I2S_PD3_1			(0x0 << 4)
#define CM710X_I2S_PD3_2			(0x1 << 4)
#define CM710X_I2S_PD3_3			(0x2 << 4)
#define CM710X_I2S_PD3_4			(0x3 << 4)
#define CM710X_I2S_PD3_6			(0x4 << 4)
#define CM710X_I2S_PD3_8			(0x5 << 4)
#define CM710X_I2S_PD3_12			(0x6 << 4)
#define CM710X_I2S_PD3_16			(0x7 << 4)
#define CM710X_I2S_BCLK_MS4_MASK		(0x1 << 3)
#define CM710X_I2S_BCLK_MS4_SFT			3
#define CM710X_I2S_BCLK_MS4_32			(0x0 << 3)
#define CM710X_I2S_BCLK_MS4_64			(0x1 << 3)
#define CM710X_I2S_PD4_MASK			(0x7 << 0)
#define CM710X_I2S_PD4_SFT			0
#define CM710X_I2S_PD4_1			(0x0 << 0)
#define CM710X_I2S_PD4_2			(0x1 << 0)
#define CM710X_I2S_PD4_3			(0x2 << 0)
#define CM710X_I2S_PD4_4			(0x3 << 0)
#define CM710X_I2S_PD4_6			(0x4 << 0)
#define CM710X_I2S_PD4_8			(0x5 << 0)
#define CM710X_I2S_PD4_12			(0x6 << 0)
#define CM710X_I2S_PD4_16			(0x7 << 0)

/* Clock Tree Control 2 (0x74) */
#define CM710X_I2S_PD5_MASK			(0x7 << 12)
#define CM710X_I2S_PD5_SFT			12
#define CM710X_I2S_PD5_1			(0x0 << 12)
#define CM710X_I2S_PD5_2			(0x1 << 12)
#define CM710X_I2S_PD5_3			(0x2 << 12)
#define CM710X_I2S_PD5_4			(0x3 << 12)
#define CM710X_I2S_PD5_6			(0x4 << 12)
#define CM710X_I2S_PD5_8			(0x5 << 12)
#define CM710X_I2S_PD5_12			(0x6 << 12)
#define CM710X_I2S_PD5_16			(0x7 << 12)
#define CM710X_I2S_PD6_MASK			(0x7 << 8)
#define CM710X_I2S_PD6_SFT			8
#define CM710X_I2S_PD6_1			(0x0 << 8)
#define CM710X_I2S_PD6_2			(0x1 << 8)
#define CM710X_I2S_PD6_3			(0x2 << 8)
#define CM710X_I2S_PD6_4			(0x3 << 8)
#define CM710X_I2S_PD6_6			(0x4 << 8)
#define CM710X_I2S_PD6_8			(0x5 << 8)
#define CM710X_I2S_PD6_12			(0x6 << 8)
#define CM710X_I2S_PD6_16			(0x7 << 8)
#define CM710X_I2S_PD7_MASK			(0x7 << 4)
#define CM710X_I2S_PD7_SFT			4
#define CM710X_I2S_PD7_1			(0x0 << 4)
#define CM710X_I2S_PD7_2			(0x1 << 4)
#define CM710X_I2S_PD7_3			(0x2 << 4)
#define CM710X_I2S_PD7_4			(0x3 << 4)
#define CM710X_I2S_PD7_6			(0x4 << 4)
#define CM710X_I2S_PD7_8			(0x5 << 4)
#define CM710X_I2S_PD7_12			(0x6 << 4)
#define CM710X_I2S_PD7_16			(0x7 << 4)
#define CM710X_I2S_PD8_MASK			(0x7 << 0)
#define CM710X_I2S_PD8_SFT			0
#define CM710X_I2S_PD8_1			(0x0 << 0)
#define CM710X_I2S_PD8_2			(0x1 << 0)
#define CM710X_I2S_PD8_3			(0x2 << 0)
#define CM710X_I2S_PD8_4			(0x3 << 0)
#define CM710X_I2S_PD8_6			(0x4 << 0)
#define CM710X_I2S_PD8_8			(0x5 << 0)
#define CM710X_I2S_PD8_12			(0x6 << 0)
#define CM710X_I2S_PD8_16			(0x7 << 0)

/* Clock Tree Control 3 (0x75) */
#define CM710X_DSP_ASRC_O_MASK			(0x3 << 6)
#define CM710X_DSP_ASRC_O_SFT			6
#define CM710X_DSP_ASRC_O_1_0			(0x0 << 6)
#define CM710X_DSP_ASRC_O_1_5			(0x1 << 6)
#define CM710X_DSP_ASRC_O_2_0			(0x2 << 6)
#define CM710X_DSP_ASRC_O_3_0			(0x3 << 6)
#define CM710X_DSP_ASRC_I_MASK			(0x3 << 4)
#define CM710X_DSP_ASRC_I_SFT			4
#define CM710X_DSP_ASRC_I_1_0			(0x0 << 4)
#define CM710X_DSP_ASRC_I_1_5			(0x1 << 4)
#define CM710X_DSP_ASRC_I_2_0			(0x2 << 4)
#define CM710X_DSP_ASRC_I_3_0			(0x3 << 4)
#define CM710X_DSP_BUS_PD_MASK			(0x7 << 0)
#define CM710X_DSP_BUS_PD_SFT			0
#define CM710X_DSP_BUS_PD_1			(0x0 << 0)
#define CM710X_DSP_BUS_PD_2			(0x1 << 0)
#define CM710X_DSP_BUS_PD_3			(0x2 << 0)
#define CM710X_DSP_BUS_PD_4			(0x3 << 0)
#define CM710X_DSP_BUS_PD_6			(0x4 << 0)
#define CM710X_DSP_BUS_PD_8			(0x5 << 0)
#define CM710X_DSP_BUS_PD_12			(0x6 << 0)
#define CM710X_DSP_BUS_PD_16			(0x7 << 0)

/* DSP InBound Control (0xa3) */
#define CM710X_IB01_SRC_MASK			(0x7 << 12)
#define CM710X_IB01_SRC_SFT			12
#define CM710X_IB23_SRC_MASK			(0x7 << 8)
#define CM710X_IB23_SRC_SFT			8
#define CM710X_IB45_SRC_MASK			(0x7 << 4)
#define CM710X_IB45_SRC_SFT			4
#define CM710X_IB6_SRC_MASK			(0x7 << 0)
#define CM710X_IB6_SRC_SFT			0

/* DSP InBound Control (0xa4) */
#define CM710X_IB7_SRC_MASK			(0x7 << 12)
#define CM710X_IB7_SRC_SFT			12
#define CM710X_IB8_SRC_MASK			(0x7 << 8)
#define CM710X_IB8_SRC_SFT			8
#define CM710X_IB9_SRC_MASK			(0x7 << 4)
#define CM710X_IB9_SRC_SFT			4

/* DSP In/OutBound Control (0xa5) */
#define CM710X_SEL_SRC_OB23			(0x1 << 4)
#define CM710X_SEL_SRC_OB23_SFT			4
#define CM710X_SEL_SRC_OB01			(0x1 << 3)
#define CM710X_SEL_SRC_OB01_SFT			3
#define CM710X_SEL_SRC_IB45			(0x1 << 2)
#define CM710X_SEL_SRC_IB45_SFT			2
#define CM710X_SEL_SRC_IB23			(0x1 << 1)
#define CM710X_SEL_SRC_IB23_SFT			1
#define CM710X_SEL_SRC_IB01			(0x1 << 0)
#define CM710X_SEL_SRC_IB01_SFT			0

/* Virtual DSP Mixer Control (0xf7 0xf8 0xf9) */
#define CM710X_DSP_IB_01_H			(0x1 << 15)
#define CM710X_DSP_IB_01_H_SFT			15
#define CM710X_DSP_IB_23_H			(0x1 << 14)
#define CM710X_DSP_IB_23_H_SFT			14
#define CM710X_DSP_IB_45_H			(0x1 << 13)
#define CM710X_DSP_IB_45_H_SFT			13
#define CM710X_DSP_IB_6_H			(0x1 << 12)
#define CM710X_DSP_IB_6_H_SFT			12
#define CM710X_DSP_IB_7_H			(0x1 << 11)
#define CM710X_DSP_IB_7_H_SFT			11
#define CM710X_DSP_IB_8_H			(0x1 << 10)
#define CM710X_DSP_IB_8_H_SFT			10
#define CM710X_DSP_IB_9_H			(0x1 << 9)
#define CM710X_DSP_IB_9_H_SFT			9
#define CM710X_DSP_IB_01_L			(0x1 << 7)
#define CM710X_DSP_IB_01_L_SFT			7
#define CM710X_DSP_IB_23_L			(0x1 << 6)
#define CM710X_DSP_IB_23_L_SFT			6
#define CM710X_DSP_IB_45_L			(0x1 << 5)
#define CM710X_DSP_IB_45_L_SFT			5
#define CM710X_DSP_IB_6_L			(0x1 << 4)
#define CM710X_DSP_IB_6_L_SFT			4
#define CM710X_DSP_IB_7_L			(0x1 << 3)
#define CM710X_DSP_IB_7_L_SFT			3
#define CM710X_DSP_IB_8_L			(0x1 << 2)
#define CM710X_DSP_IB_8_L_SFT			2
#define CM710X_DSP_IB_9_L			(0x1 << 1)
#define CM710X_DSP_IB_9_L_SFT			1

/* General Control2 (0xfc)*/
#define CM710X_GPIO5_FUNC_MASK			(0x1 << 9)
#define CM710X_GPIO5_FUNC_GPIO			(0x0 << 9)
#define CM710X_GPIO5_FUNC_DMIC			(0x1 << 9)

enum {
	CM710X_AIF1,
	CM710X_AIF2,
	CM710X_AIF3,
	CM710X_AIF4,
	CM710X_AIF5,
	CM710X_AIFS,
};

struct cm710x_codec_priv {
	struct regmap *virt_regmap, *real_regmap;
	struct device *dev;
	const struct firmware *fw;

	struct regulator *avdd_core;
	struct regulator *dvdd_core;
	struct regulator *amic_core;

	struct clk *mclk;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_suspend;
	struct pinctrl_state *pin_spk_en;
	struct pinctrl_state *pin_hp_en;

	struct work_struct fw_download_work;
	struct completion fw_download_complete;

	struct delayed_work avoid_pop_noise;

	bool bDelayForPopNoise;
	bool bDspMode;
	atomic_bool bSuspendRequestPending;

	struct mutex Dsp_Access_Lock;

	unsigned int EQ[20];

	const char *fw_parsing;
};

#endif /* __CM710X_H__ */
