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
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/firmware.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/ctype.h>
#include <linux/suspend.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "cm710x.h"
#include "cm710x-spi.h"

#define CM710X_FIRMWARE	"CM710X.bin"
#define CM710X_MIC_CALIBRATION_FIRMWARE	"mic"
enum {
	SPK_EQ_ON_OFF			= 0x01,
	MIC_EQ_ON_OFF,
	MIC_DSP_BYPASS_ON_OFF,
	MIC_DSP_PROCESS_ON_OFF,
	MIC_DSP_AEC_ON_OFF,
	MIC_EQ_AGC_ON_OFF,
	SPK_L_EQ_PRE_GAIN,
	SPK_R_EQ_PRE_GAIN,
	MIC_EQ_PRE_GAIN,
	SPK_L_EQ_LOWPASS_FC,
	SPK_L_EQ_BAND_01_FC,
	SPK_L_EQ_BAND_02_FC,
	SPK_L_EQ_BAND_03_FC,
	SPK_L_EQ_BAND_04_FC,
	SPK_L_EQ_BAND_05_FC,
	SPK_L_EQ_BAND_06_FC,
	SPK_L_EQ_HIGHPASS_FC,
	SPK_L_EQ_LOWPASS_GAIN,
	SPK_L_EQ_BAND_01_GAIN,
	SPK_L_EQ_BAND_02_GAIN,
	SPK_L_EQ_BAND_03_GAIN,
	SPK_L_EQ_BAND_04_GAIN,
	SPK_L_EQ_BAND_05_GAIN,
	SPK_L_EQ_BAND_06_GAIN,
	SPK_L_EQ_HIGHPASS_GAIN,
	SPK_L_EQ_BAND_01_Q,
	SPK_L_EQ_BAND_02_Q,
	SPK_L_EQ_BAND_03_Q,
	SPK_L_EQ_BAND_04_Q,
	SPK_L_EQ_BAND_05_Q,
	SPK_L_EQ_BAND_06_Q,
	SPK_R_EQ_LOWPASS_FC,
	SPK_R_EQ_BAND_01_FC,
	SPK_R_EQ_BAND_02_FC,
	SPK_R_EQ_BAND_03_FC,
	SPK_R_EQ_BAND_04_FC,
	SPK_R_EQ_BAND_05_FC,
	SPK_R_EQ_BAND_06_FC,
	SPK_R_EQ_HIGHPASS_FC,
	SPK_R_EQ_LOWPASS_GAIN,
	SPK_R_EQ_BAND_01_GAIN,
	SPK_R_EQ_BAND_02_GAIN,
	SPK_R_EQ_BAND_03_GAIN,
	SPK_R_EQ_BAND_04_GAIN,
	SPK_R_EQ_BAND_05_GAIN,
	SPK_R_EQ_BAND_06_GAIN,
	SPK_R_EQ_HIGHPASS_GAIN,
	SPK_R_EQ_BAND_01_Q,
	SPK_R_EQ_BAND_02_Q,
	SPK_R_EQ_BAND_03_Q,
	SPK_R_EQ_BAND_04_Q,
	SPK_R_EQ_BAND_05_Q,
	SPK_R_EQ_BAND_06_Q,
	MIC_EQ_LOWPASS_FC,
	MIC_EQ_BAND_01_FC,
	MIC_EQ_BAND_02_FC,
	MIC_EQ_BAND_03_FC,
	MIC_EQ_BAND_04_FC,
	MIC_EQ_BAND_05_FC,
	MIC_EQ_BAND_06_FC,
	MIC_EQ_HIGHPASS_FC,
	MIC_EQ_LOWPASS_GAIN,
	MIC_EQ_BAND_01_GAIN,
	MIC_EQ_BAND_02_GAIN,
	MIC_EQ_BAND_03_GAIN,
	MIC_EQ_BAND_04_GAIN,
	MIC_EQ_BAND_05_GAIN,
	MIC_EQ_BAND_06_GAIN,
	MIC_EQ_HIGHPASS_GAIN,
	MIC_EQ_BAND_01_Q,
	MIC_EQ_BAND_02_Q,
	MIC_EQ_BAND_03_Q,
	MIC_EQ_BAND_04_Q,
	MIC_EQ_BAND_05_Q,
	MIC_EQ_BAND_06_Q,
	MIC_DSP_MAXNR_AEC,
	MIC_DSP_NOISEGATE,
	MIC_DSP_TBRRLOW,
	MIC_DSP_SKIP_AEC,
	MIC_DSP_MAXNR_BF,
	MIC_DSP_MAXNR_BF_MODE,
};

#define DEFAULT_FW_PARSING  "spi"

#define PINCTRL_STATE_CODEC_DEFAULT	"cm710x_codec_default"
#define PINCTRL_STATE_CODEC_SUSPEND	"cm710x_codec_suspend"
#define PINCTRL_STATE_SPK_EN		"cm710x_codec_spk_en"
#define PINCTRL_STATE_SPK_SUSPEND	"cm710x_codec_spk_suspend"

/* Firmware load takes ~500ms, allow a 1 second timeout */
#define FW_DOWNLOAD_TIMEOUT (1*HZ)

static unsigned int PlaybackFrom;
static u32 DspAddr = 0x5ffc001C;
static u32 CodecAddr = 0x3C;
static const char hextext[] = "0123456789abcdefxABCDEFX";

static const struct reg_default cm710x_reg[] = {
	{CM710X_RESET,				0x0000},
	{CM710X_LOUT1,				0xa800},
	{CM710X_IN1,				0x0000},
	{CM710X_MICBIAS,			0x0000},
	{CM710X_SLIMBUS_PARAM,			0x0000},
	{CM710X_SLIMBUS_RX,			0x0000},
	{CM710X_SLIMBUS_CTRL,			0x0000},
	{CM710X_SIDETONE_CTRL,			0x000b},
	{CM710X_ANA_DAC1_2_3_SRC,		0x0000},
	{CM710X_IF_DSP_DAC3_4_MIXER,		0x1111},
	{CM710X_DAC4_DIG_VOL,			0xafaf},
	{CM710X_DAC3_DIG_VOL,			0xafaf},
	{CM710X_DAC1_DIG_VOL,			0xafaf},
	{CM710X_DAC2_DIG_VOL,			0xafaf},
	{CM710X_IF_DSP_DAC2_MIXER,		0x0011},
	{CM710X_STO1_ADC_DIG_VOL,		0x2f2f},
	{CM710X_MONO_ADC_DIG_VOL,		0x2f2f},
	{CM710X_STO1_2_ADC_BST,			0x0000},
	{CM710X_STO2_ADC_DIG_VOL,		0x2f2f},
	{CM710X_ADC_BST_CTRL2,			0x0000},
	{CM710X_STO3_4_ADC_BST,			0x0000},
	{CM710X_STO3_ADC_DIG_VOL,		0x2f2f},
	{CM710X_STO4_ADC_DIG_VOL,		0x2f2f},
	{CM710X_STO4_ADC_MIXER,			0xd4c0},
	{CM710X_STO3_ADC_MIXER,			0xd4c0},
	{CM710X_STO2_ADC_MIXER,			0xd4c0},
	{CM710X_STO1_ADC_MIXER,			0xd4c0},
	{CM710X_MONO_ADC_MIXER,			0xd4d1},
	{CM710X_ADC_IF_DSP_DAC1_MIXER,		0x8080},
	{CM710X_STO1_DAC_MIXER,			0xaaaa},
	{CM710X_MONO_DAC_MIXER,			0xaaaa},
	{CM710X_DD1_MIXER,			0xaaaa},
	{CM710X_DD2_MIXER,			0xaaaa},
	{CM710X_IF3_DATA,			0x0000},
	{CM710X_IF4_DATA,			0x0000},
	{CM710X_PDM_OUT_CTRL,			0x8888},
	{CM710X_PDM_DATA_CTRL1,			0x0000},
	{CM710X_PDM_DATA_CTRL2,			0x0000},
	{CM710X_PDM1_DATA_CTRL2,		0x0000},
	{CM710X_PDM1_DATA_CTRL3,		0x0000},
	{CM710X_PDM1_DATA_CTRL4,		0x0000},
	{CM710X_PDM2_DATA_CTRL2,		0x0000},
	{CM710X_PDM2_DATA_CTRL3,		0x0000},
	{CM710X_PDM2_DATA_CTRL4,		0x0000},
	{CM710X_TDM1_CTRL1,			0x0300},
	{CM710X_TDM1_CTRL2,			0x0000},
	{CM710X_TDM1_CTRL3,			0x4000},
	{CM710X_TDM1_CTRL4,			0x0123},
	{CM710X_TDM1_CTRL5,			0x4567},
	{CM710X_TDM2_CTRL1,			0x0300},
	{CM710X_TDM2_CTRL2,			0x0000},
	{CM710X_TDM2_CTRL3,			0x4000},
	{CM710X_TDM2_CTRL4,			0x0123},
	{CM710X_TDM2_CTRL5,			0x4567},
	{CM710X_I2C_MASTER_CTRL1,		0x0001},
	{CM710X_I2C_MASTER_CTRL2,		0x0000},
	{CM710X_I2C_MASTER_CTRL3,		0x0000},
	{CM710X_I2C_MASTER_CTRL4,		0x0000},
	{CM710X_I2C_MASTER_CTRL5,		0x0000},
	{CM710X_I2C_MASTER_CTRL6,		0x0000},
	{CM710X_I2C_MASTER_CTRL7,		0x0000},
	{CM710X_I2C_MASTER_CTRL8,		0x0000},
	{CM710X_DMIC_CTRL1,			0x1505},
	{CM710X_DMIC_CTRL2,			0x0055},
	{CM710X_HAP_GENE_CTRL1,			0x0111},
	{CM710X_HAP_GENE_CTRL2,			0x0064},
	{CM710X_HAP_GENE_CTRL3,			0xef0e},
	{CM710X_HAP_GENE_CTRL4,			0xf0f0},
	{CM710X_HAP_GENE_CTRL5,			0xef0e},
	{CM710X_HAP_GENE_CTRL6,			0xf0f0},
	{CM710X_HAP_GENE_CTRL7,			0xef0e},
	{CM710X_HAP_GENE_CTRL8,			0xf0f0},
	{CM710X_HAP_GENE_CTRL9,			0xf000},
	{CM710X_HAP_GENE_CTRL10,		0x0000},
	{CM710X_PWR_DIG1,			0x0000},
	{CM710X_PWR_DIG2,			0x0000},
	{CM710X_PWR_ANLG1,			0x0055},
	{CM710X_PWR_ANLG2,			0x0000},
	{CM710X_PWR_DSP1,			0x0001},
	{CM710X_PWR_DSP_ST,			0x0000},
	{CM710X_PWR_DSP2,			0x0000},
	{CM710X_ADC_DAC_HPF_CTRL1,		0x0e00},
	{CM710X_PRIV_INDEX,			0x0000},
	{CM710X_PRIV_DATA,			0x0000},
	{CM710X_I2S4_SDP,			0x8000},
	{CM710X_I2S1_SDP,			0x8000},
	{CM710X_I2S2_SDP,			0x8000},
	{CM710X_I2S3_SDP,			0x8000},
	{CM710X_CLK_TREE_CTRL1,			0x1111},
	{CM710X_CLK_TREE_CTRL2,			0x1111},
	{CM710X_CLK_TREE_CTRL3,			0x0000},
	{CM710X_PLL1_CTRL1,			0x0000},
	{CM710X_PLL1_CTRL2,			0x0000},
	{CM710X_PLL2_CTRL1,			0x0c60},
	{CM710X_PLL2_CTRL2,			0x2000},
	{CM710X_GLB_CLK1,			0x0000},
	{CM710X_GLB_CLK2,			0x0000},
	{CM710X_ASRC_1,				0x0000},
	{CM710X_ASRC_2,				0x0000},
	{CM710X_ASRC_3,				0x0000},
	{CM710X_ASRC_4,				0x0000},
	{CM710X_ASRC_5,				0x0000},
	{CM710X_ASRC_6,				0x0000},
	{CM710X_ASRC_7,				0x0000},
	{CM710X_ASRC_8,				0x0000},
	{CM710X_ASRC_9,				0x0000},
	{CM710X_ASRC_10,			0x0000},
	{CM710X_ASRC_11,			0x0000},
	{CM710X_ASRC_12,			0x0018},
	{CM710X_ASRC_13,			0x0000},
	{CM710X_ASRC_14,			0x0000},
	{CM710X_ASRC_15,			0x0000},
	{CM710X_ASRC_16,			0x0000},
	{CM710X_ASRC_17,			0x0000},
	{CM710X_ASRC_18,			0x0000},
	{CM710X_ASRC_19,			0x0000},
	{CM710X_ASRC_20,			0x0000},
	{CM710X_ASRC_21,			0x000c},
	{CM710X_ASRC_22,			0x0000},
	{CM710X_ASRC_23,			0x0000},
	{CM710X_VAD_CTRL1,			0x2184},
	{CM710X_VAD_CTRL2,			0x010a},
	{CM710X_VAD_CTRL3,			0x0aea},
	{CM710X_VAD_CTRL4,			0x000c},
	{CM710X_VAD_CTRL5,			0x0000},
	{CM710X_DSP_INB_CTRL1,			0x0000},
	{CM710X_DSP_INB_CTRL2,			0x0000},
	{CM710X_DSP_IN_OUTB_CTRL,		0x0000},
	{CM710X_DSP_OUTB0_1_DIG_VOL,		0x2f2f},
	{CM710X_DSP_OUTB2_3_DIG_VOL,		0x2f2f},
	{CM710X_DSP_OUTB4_5_DIG_VOL,		0x2f2f},
	{CM710X_DSP_OUTB6_7_DIG_VOL,		0x2f2f},
	{CM710X_ADC_EQ_CTRL1,			0x6000},
	{CM710X_ADC_EQ_CTRL2,			0x0000},
	{CM710X_EQ_CTRL1,			0xc000},
	{CM710X_EQ_CTRL2,			0x0000},
	{CM710X_EQ_CTRL3,			0x0000},
	{CM710X_SOFT_VOL_ZERO_CROSS1,		0x0009},
	{CM710X_JD_CTRL1,			0x0000},
	{CM710X_JD_CTRL2,			0x0000},
	{CM710X_JD_CTRL3,			0x0000},
	{CM710X_IRQ_CTRL1,			0x0000},
	{CM710X_IRQ_CTRL2,			0x0000},
	{CM710X_GPIO_ST,			0x0000},
	{CM710X_GPIO_CTRL1,			0x0000},
	{CM710X_GPIO_CTRL2,			0x0000},
	{CM710X_GPIO_CTRL3,			0x0000},
	{CM710X_STO1_ADC_HI_FILTER1,		0xb320},
	{CM710X_STO1_ADC_HI_FILTER2,		0x0000},
	{CM710X_MONO_ADC_HI_FILTER1,		0xb300},
	{CM710X_MONO_ADC_HI_FILTER2,		0x0000},
	{CM710X_STO2_ADC_HI_FILTER1,		0xb300},
	{CM710X_STO2_ADC_HI_FILTER2,		0x0000},
	{CM710X_STO3_ADC_HI_FILTER1,		0xb300},
	{CM710X_STO3_ADC_HI_FILTER2,		0x0000},
	{CM710X_STO4_ADC_HI_FILTER1,		0xb300},
	{CM710X_STO4_ADC_HI_FILTER2,		0x0000},
	{CM710X_MB_DRC_CTRL1,			0x0f20},
	{CM710X_DRC1_CTRL1,			0x001f},
	{CM710X_DRC1_CTRL2,			0x020c},
	{CM710X_DRC1_CTRL3,			0x1f00},
	{CM710X_DRC1_CTRL4,			0x0000},
	{CM710X_DRC1_CTRL5,			0x0000},
	{CM710X_DRC1_CTRL6,			0x0029},
	{CM710X_DRC2_CTRL1,			0x001f},
	{CM710X_DRC2_CTRL2,			0x020c},
	{CM710X_DRC2_CTRL3,			0x1f00},
	{CM710X_DRC2_CTRL4,			0x0000},
	{CM710X_DRC2_CTRL5,			0x0000},
	{CM710X_DRC2_CTRL6,			0x0029},
	{CM710X_DRC1_HL_CTRL1,			0x8000},
	{CM710X_DRC1_HL_CTRL2,			0x0200},
	{CM710X_DRC2_HL_CTRL1,			0x8000},
	{CM710X_DRC2_HL_CTRL2,			0x0200},
	{CM710X_DSP_INB1_SRC_CTRL1,		0x5800},
	{CM710X_DSP_INB1_SRC_CTRL2,		0x0000},
	{CM710X_DSP_INB1_SRC_CTRL3,		0x0000},
	{CM710X_DSP_INB1_SRC_CTRL4,		0x0800},
	{CM710X_DSP_INB2_SRC_CTRL1,		0x5800},
	{CM710X_DSP_INB2_SRC_CTRL2,		0x0000},
	{CM710X_DSP_INB2_SRC_CTRL3,		0x0000},
	{CM710X_DSP_INB2_SRC_CTRL4,		0x0800},
	{CM710X_DSP_INB3_SRC_CTRL1,		0x5800},
	{CM710X_DSP_INB3_SRC_CTRL2,		0x0000},
	{CM710X_DSP_INB3_SRC_CTRL3,		0x0000},
	{CM710X_DSP_INB3_SRC_CTRL4,		0x0800},
	{CM710X_DSP_OUTB1_SRC_CTRL1,		0x5800},
	{CM710X_DSP_OUTB1_SRC_CTRL2,		0x0000},
	{CM710X_DSP_OUTB1_SRC_CTRL3,		0x0000},
	{CM710X_DSP_OUTB1_SRC_CTRL4,		0x0800},
	{CM710X_DSP_OUTB2_SRC_CTRL1,		0x5800},
	{CM710X_DSP_OUTB2_SRC_CTRL2,		0x0000},
	{CM710X_DSP_OUTB2_SRC_CTRL3,		0x0000},
	{CM710X_DSP_OUTB2_SRC_CTRL4,		0x0800},
	{CM710X_DSP_OUTB_0123_MIXER_CTRL,	0xfefe},
	{CM710X_DSP_OUTB_45_MIXER_CTRL,		0xfefe},
	{CM710X_DSP_OUTB_67_MIXER_CTRL,		0xfefe},
	{CM710X_DIG_MISC,			0x0000},
	{CM710X_GEN_CTRL1,			0x0000},
	{CM710X_GEN_CTRL2,			0x0000},
	{CM710X_VENDOR_ID,			0x0000},
	{CM710X_VENDOR_ID1,			0x10ec},
	{CM710X_VENDOR_ID2,			0x6327},
};

static bool cm710x_volatile_register(struct device *dev, unsigned int reg)
{

	switch (reg) {
	case CM710X_RESET:
	case CM710X_SLIMBUS_PARAM:
	case CM710X_PDM_DATA_CTRL1:
	case CM710X_PDM_DATA_CTRL2:
	case CM710X_PDM1_DATA_CTRL4:
	case CM710X_PDM2_DATA_CTRL4:
	case CM710X_I2C_MASTER_CTRL1:
	case CM710X_I2C_MASTER_CTRL7:
	case CM710X_I2C_MASTER_CTRL8:
	case CM710X_HAP_GENE_CTRL2:
	case CM710X_PWR_DSP_ST:
	case CM710X_PRIV_DATA:
	case CM710X_ASRC_22:
	case CM710X_ASRC_23:
	case CM710X_VAD_CTRL5:
	case CM710X_ADC_EQ_CTRL1:
	case CM710X_EQ_CTRL1:
	case CM710X_IRQ_CTRL1:
	case CM710X_IRQ_CTRL2:
	case CM710X_GPIO_ST:
	case CM710X_DSP_INB1_SRC_CTRL4:
	case CM710X_DSP_INB2_SRC_CTRL4:
	case CM710X_DSP_INB3_SRC_CTRL4:
	case CM710X_DSP_OUTB1_SRC_CTRL4:
	case CM710X_DSP_OUTB2_SRC_CTRL4:
	case CM710X_VENDOR_ID:
	case CM710X_VENDOR_ID1:
	case CM710X_VENDOR_ID2:
		return true;
	default:
		return false;
	}
}

static bool cm710x_readable_register(struct device *dev, unsigned int reg)
{

	switch (reg) {
	case CM710X_RESET:
	case CM710X_LOUT1:
	case CM710X_IN1:
	case CM710X_MICBIAS:
	case CM710X_SLIMBUS_PARAM:
	case CM710X_SLIMBUS_RX:
	case CM710X_SLIMBUS_CTRL:
	case CM710X_SIDETONE_CTRL:
	case CM710X_ANA_DAC1_2_3_SRC:
	case CM710X_IF_DSP_DAC3_4_MIXER:
	case CM710X_DAC4_DIG_VOL:
	case CM710X_DAC3_DIG_VOL:
	case CM710X_DAC1_DIG_VOL:
	case CM710X_DAC2_DIG_VOL:
	case CM710X_IF_DSP_DAC2_MIXER:
	case CM710X_STO1_ADC_DIG_VOL:
	case CM710X_MONO_ADC_DIG_VOL:
	case CM710X_STO1_2_ADC_BST:
	case CM710X_STO2_ADC_DIG_VOL:
	case CM710X_ADC_BST_CTRL2:
	case CM710X_STO3_4_ADC_BST:
	case CM710X_STO3_ADC_DIG_VOL:
	case CM710X_STO4_ADC_DIG_VOL:
	case CM710X_STO4_ADC_MIXER:
	case CM710X_STO3_ADC_MIXER:
	case CM710X_STO2_ADC_MIXER:
	case CM710X_STO1_ADC_MIXER:
	case CM710X_MONO_ADC_MIXER:
	case CM710X_ADC_IF_DSP_DAC1_MIXER:
	case CM710X_STO1_DAC_MIXER:
	case CM710X_MONO_DAC_MIXER:
	case CM710X_DD1_MIXER:
	case CM710X_DD2_MIXER:
	case CM710X_IF3_DATA:
	case CM710X_IF4_DATA:
	case CM710X_PDM_OUT_CTRL:
	case CM710X_PDM_DATA_CTRL1:
	case CM710X_PDM_DATA_CTRL2:
	case CM710X_PDM1_DATA_CTRL2:
	case CM710X_PDM1_DATA_CTRL3:
	case CM710X_PDM1_DATA_CTRL4:
	case CM710X_PDM2_DATA_CTRL2:
	case CM710X_PDM2_DATA_CTRL3:
	case CM710X_PDM2_DATA_CTRL4:
	case CM710X_TDM1_CTRL1:
	case CM710X_TDM1_CTRL2:
	case CM710X_TDM1_CTRL3:
	case CM710X_TDM1_CTRL4:
	case CM710X_TDM1_CTRL5:
	case CM710X_TDM2_CTRL1:
	case CM710X_TDM2_CTRL2:
	case CM710X_TDM2_CTRL3:
	case CM710X_TDM2_CTRL4:
	case CM710X_TDM2_CTRL5:
	case CM710X_I2C_MASTER_CTRL1:
	case CM710X_I2C_MASTER_CTRL2:
	case CM710X_I2C_MASTER_CTRL3:
	case CM710X_I2C_MASTER_CTRL4:
	case CM710X_I2C_MASTER_CTRL5:
	case CM710X_I2C_MASTER_CTRL6:
	case CM710X_I2C_MASTER_CTRL7:
	case CM710X_I2C_MASTER_CTRL8:
	case CM710X_DMIC_CTRL1:
	case CM710X_DMIC_CTRL2:
	case CM710X_HAP_GENE_CTRL1:
	case CM710X_HAP_GENE_CTRL2:
	case CM710X_HAP_GENE_CTRL3:
	case CM710X_HAP_GENE_CTRL4:
	case CM710X_HAP_GENE_CTRL5:
	case CM710X_HAP_GENE_CTRL6:
	case CM710X_HAP_GENE_CTRL7:
	case CM710X_HAP_GENE_CTRL8:
	case CM710X_HAP_GENE_CTRL9:
	case CM710X_HAP_GENE_CTRL10:
	case CM710X_PWR_DIG1:
	case CM710X_PWR_DIG2:
	case CM710X_PWR_ANLG1:
	case CM710X_PWR_ANLG2:
	case CM710X_PWR_DSP1:
	case CM710X_PWR_DSP_ST:
	case CM710X_PWR_DSP2:
	case CM710X_ADC_DAC_HPF_CTRL1:
	case CM710X_PRIV_INDEX:
	case CM710X_PRIV_DATA:
	case CM710X_I2S4_SDP:
	case CM710X_I2S1_SDP:
	case CM710X_I2S2_SDP:
	case CM710X_I2S3_SDP:
	case CM710X_CLK_TREE_CTRL1:
	case CM710X_CLK_TREE_CTRL2:
	case CM710X_CLK_TREE_CTRL3:
	case CM710X_PLL1_CTRL1:
	case CM710X_PLL1_CTRL2:
	case CM710X_PLL2_CTRL1:
	case CM710X_PLL2_CTRL2:
	case CM710X_GLB_CLK1:
	case CM710X_GLB_CLK2:
	case CM710X_ASRC_1:
	case CM710X_ASRC_2:
	case CM710X_ASRC_3:
	case CM710X_ASRC_4:
	case CM710X_ASRC_5:
	case CM710X_ASRC_6:
	case CM710X_ASRC_7:
	case CM710X_ASRC_8:
	case CM710X_ASRC_9:
	case CM710X_ASRC_10:
	case CM710X_ASRC_11:
	case CM710X_ASRC_12:
	case CM710X_ASRC_13:
	case CM710X_ASRC_14:
	case CM710X_ASRC_15:
	case CM710X_ASRC_16:
	case CM710X_ASRC_17:
	case CM710X_ASRC_18:
	case CM710X_ASRC_19:
	case CM710X_ASRC_20:
	case CM710X_ASRC_21:
	case CM710X_ASRC_22:
	case CM710X_ASRC_23:
	case CM710X_VAD_CTRL1:
	case CM710X_VAD_CTRL2:
	case CM710X_VAD_CTRL3:
	case CM710X_VAD_CTRL4:
	case CM710X_VAD_CTRL5:
	case CM710X_DSP_INB_CTRL1:
	case CM710X_DSP_INB_CTRL2:
	case CM710X_DSP_IN_OUTB_CTRL:
	case CM710X_DSP_OUTB0_1_DIG_VOL:
	case CM710X_DSP_OUTB2_3_DIG_VOL:
	case CM710X_DSP_OUTB4_5_DIG_VOL:
	case CM710X_DSP_OUTB6_7_DIG_VOL:
	case CM710X_ADC_EQ_CTRL1:
	case CM710X_ADC_EQ_CTRL2:
	case CM710X_EQ_CTRL1:
	case CM710X_EQ_CTRL2:
	case CM710X_EQ_CTRL3:
	case CM710X_SOFT_VOL_ZERO_CROSS1:
	case CM710X_JD_CTRL1:
	case CM710X_JD_CTRL2:
	case CM710X_JD_CTRL3:
	case CM710X_IRQ_CTRL1:
	case CM710X_IRQ_CTRL2:
	case CM710X_GPIO_ST:
	case CM710X_GPIO_CTRL1:
	case CM710X_GPIO_CTRL2:
	case CM710X_GPIO_CTRL3:
	case CM710X_STO1_ADC_HI_FILTER1:
	case CM710X_STO1_ADC_HI_FILTER2:
	case CM710X_MONO_ADC_HI_FILTER1:
	case CM710X_MONO_ADC_HI_FILTER2:
	case CM710X_STO2_ADC_HI_FILTER1:
	case CM710X_STO2_ADC_HI_FILTER2:
	case CM710X_STO3_ADC_HI_FILTER1:
	case CM710X_STO3_ADC_HI_FILTER2:
	case CM710X_STO4_ADC_HI_FILTER1:
	case CM710X_STO4_ADC_HI_FILTER2:
	case CM710X_MB_DRC_CTRL1:
	case CM710X_DRC1_CTRL1:
	case CM710X_DRC1_CTRL2:
	case CM710X_DRC1_CTRL3:
	case CM710X_DRC1_CTRL4:
	case CM710X_DRC1_CTRL5:
	case CM710X_DRC1_CTRL6:
	case CM710X_DRC2_CTRL1:
	case CM710X_DRC2_CTRL2:
	case CM710X_DRC2_CTRL3:
	case CM710X_DRC2_CTRL4:
	case CM710X_DRC2_CTRL5:
	case CM710X_DRC2_CTRL6:
	case CM710X_DRC1_HL_CTRL1:
	case CM710X_DRC1_HL_CTRL2:
	case CM710X_DRC2_HL_CTRL1:
	case CM710X_DRC2_HL_CTRL2:
	case CM710X_DSP_INB1_SRC_CTRL1:
	case CM710X_DSP_INB1_SRC_CTRL2:
	case CM710X_DSP_INB1_SRC_CTRL3:
	case CM710X_DSP_INB1_SRC_CTRL4:
	case CM710X_DSP_INB2_SRC_CTRL1:
	case CM710X_DSP_INB2_SRC_CTRL2:
	case CM710X_DSP_INB2_SRC_CTRL3:
	case CM710X_DSP_INB2_SRC_CTRL4:
	case CM710X_DSP_INB3_SRC_CTRL1:
	case CM710X_DSP_INB3_SRC_CTRL2:
	case CM710X_DSP_INB3_SRC_CTRL3:
	case CM710X_DSP_INB3_SRC_CTRL4:
	case CM710X_DSP_OUTB1_SRC_CTRL1:
	case CM710X_DSP_OUTB1_SRC_CTRL2:
	case CM710X_DSP_OUTB1_SRC_CTRL3:
	case CM710X_DSP_OUTB1_SRC_CTRL4:
	case CM710X_DSP_OUTB2_SRC_CTRL1:
	case CM710X_DSP_OUTB2_SRC_CTRL2:
	case CM710X_DSP_OUTB2_SRC_CTRL3:
	case CM710X_DSP_OUTB2_SRC_CTRL4:
	case CM710X_DSP_OUTB_0123_MIXER_CTRL:
	case CM710X_DSP_OUTB_45_MIXER_CTRL:
	case CM710X_DSP_OUTB_67_MIXER_CTRL:
	case CM710X_DIG_MISC:
	case CM710X_GEN_CTRL1:
	case CM710X_GEN_CTRL2:
	case CM710X_VENDOR_ID:
	case CM710X_VENDOR_ID1:
	case CM710X_VENDOR_ID2:
		return true;
	default:
		return false;
	}
}

static void cm710x_update_DSP_Effect_Param(
	struct cm710x_codec_priv *cm710x_codec,
	char type,
	long value
);

static int get_pin_control(struct cm710x_codec_priv *cm710x)
{
	int rc = 0;

	if (!cm710x)
		return -EINVAL;

	cm710x->pinctrl = devm_pinctrl_get(cm710x->dev);
	if (IS_ERR_OR_NULL(cm710x->pinctrl)) {
		rc = -EINVAL;
		dev_err(cm710x->dev, "failed to look up pinctrl");
		goto error;
	}

	cm710x->pin_default = pinctrl_lookup_state(cm710x->pinctrl,
			PINCTRL_STATE_CODEC_DEFAULT);
	if (IS_ERR_OR_NULL(cm710x->pin_default)) {
		rc = -EINVAL;
		dev_err(cm710x->dev, "failed to look up default pin state");
		goto free_pinctrl;
	}

	cm710x->pin_suspend = pinctrl_lookup_state(cm710x->pinctrl,
			PINCTRL_STATE_CODEC_SUSPEND);
	if (IS_ERR_OR_NULL(cm710x->pin_suspend)) {
		rc = -EINVAL;
		dev_err(cm710x->dev, "failed to look up suspend pin state");
		goto free_pinctrl;
	}

	cm710x->pin_spk_en = pinctrl_lookup_state(cm710x->pinctrl,
			PINCTRL_STATE_SPK_EN);
	if (IS_ERR_OR_NULL(cm710x->pin_spk_en))
		dev_warn(cm710x->dev, "failed to look up spk_en pin state");

	cm710x->pin_spk_suspend = pinctrl_lookup_state(cm710x->pinctrl,
			PINCTRL_STATE_SPK_SUSPEND);
	if (IS_ERR_OR_NULL(cm710x->pin_spk_suspend))
		dev_warn(cm710x->dev, "failed to look up spk_suspend pin state");

	return rc;

free_pinctrl:
	devm_pinctrl_put(cm710x->pinctrl);
error:
	return rc;
}

static int cm710x_dsp_mode_i2c_write_addr(struct cm710x_codec_priv *cm710x,
		unsigned int addr, unsigned int value, unsigned int opcode)
{
	int ret;

	mutex_lock(&cm710x->Dsp_Access_Lock);

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_ADDR_MSB,
			addr >> 16);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set addr msb value: %d\n", ret);
		goto err;
	}

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_ADDR_LSB,
			addr & 0xffff);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set addr lsb value: %d\n", ret);
		goto err;
	}

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_DATA_MSB,
			value >> 16);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set data msb value: %d\n", ret);
		goto err;
	}

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_DATA_LSB,
			value & 0xffff);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set data lsb value: %d\n", ret);
		goto err;
	}

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_OP_CODE, opcode);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set op code value: %d\n", ret);
		goto err;
	}

err:
	mutex_unlock(&cm710x->Dsp_Access_Lock);

	return ret;
}

/**
 * cm710x_dsp_mode_i2c_read_addr - Read value from address on DSP mode.
 * cm710x: Private Data.
 * @addr: Address index.
 * @value: Address data.
 *
 *
 * Returns 0 for success or negative error code.
 */
static int cm710x_dsp_mode_i2c_read_addr(
		struct cm710x_codec_priv *cm710x, unsigned int addr,
		unsigned int *value)
{
	int ret;
	unsigned int msb, lsb;

	mutex_lock(&cm710x->Dsp_Access_Lock);

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_ADDR_MSB,
			addr >> 16);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set addr msb value: %d\n", ret);
		goto err;
	}

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_ADDR_LSB,
			addr & 0xffff);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set addr lsb value: %d\n", ret);
		goto err;
	}

	ret = regmap_write(cm710x->real_regmap, CM710X_DSP_I2C_OP_CODE, 0x0002);
	if (ret < 0) {
		dev_err(cm710x->dev, "Failed to set op code value: %d\n", ret);
		goto err;
	}

	regmap_read(cm710x->real_regmap, CM710X_DSP_I2C_DATA_MSB, &msb);
	regmap_read(cm710x->real_regmap, CM710X_DSP_I2C_DATA_LSB, &lsb);
	*value = (msb << 16) | lsb;

err:
	mutex_unlock(&cm710x->Dsp_Access_Lock);

	return ret;
}

static int cm710x_dsp_mode_i2c_read(struct cm710x_codec_priv *cm710x,
		unsigned int reg, unsigned int *value)
{
	return cm710x_dsp_mode_i2c_read_addr(cm710x, 0x18020000 + reg * 2,
			value);
}

static int cm710x_dsp_mode_i2c_write(struct cm710x_codec_priv *cm710x,
		unsigned int reg, unsigned int value)
{
	return cm710x_dsp_mode_i2c_write_addr(cm710x, 0x18020000 + reg * 2,
			value, 0x0001);
}

static int cm710x_dsp_mode_i2c_read_mem(struct regmap *regmap, u32 uAddr,
		u32 *Data)
{
	u32 AddrRegL;
	u32 AddrRegH;
	u32 DataReg;
	int ret = -EIO;

	if (uAddr & 0x03)
		return ret;

	ret = 0;
	AddrRegL = uAddr & 0xffff;
	AddrRegH = uAddr >> 16;
	regmap_write(regmap, 0x01, AddrRegL);
	regmap_write(regmap, 0x02, AddrRegH);
	regmap_write(regmap, 0x00, 0x02);
	regmap_read(regmap, 0x03, &DataReg);
	*Data = DataReg & 0xFFFF;
	regmap_read(regmap, 0x04, &DataReg);
	*Data |= (DataReg & 0xFFFF) << 16;

	return ret;
}

static int cm710x_dsp_mode_i2c_write_mem(struct regmap *regmap, u32 uAddr,
		u8 *Data, int len)
{
	int count, idx;
	u32 AddrRegL;
	u32 AddrRegH;
	u32 DataRegL;
	u32 DataRegH;
	int ret = 0;

	if (0 == (uAddr & 0x03)) {
		if (0 != (len & 0x03)) {
			count = len >> 2;
			for (idx = 0; idx < count; idx++) {
				AddrRegL = uAddr & 0xffff;
				AddrRegH = uAddr >> 16;
				DataRegL = Data[0] | (Data[1] << 8);
				DataRegH = Data[2] | (Data[3] << 8);
				regmap_write(regmap, 0x01, AddrRegL);
				regmap_write(regmap, 0x02, AddrRegH);
				regmap_write(regmap, 0x03, DataRegL);
				regmap_write(regmap, 0x04, DataRegH);
				regmap_write(regmap, 0x00, 0x03);
				uAddr += 4;
				Data += 4;
			}
		} else {
			count = len >> 2;
			for (idx = 0; idx < count; idx++) {
				AddrRegL = uAddr & 0xffff;
				AddrRegH = uAddr >> 16;
				DataRegL = Data[0] | (Data[1] << 8);
				DataRegH = Data[2] | (Data[3] << 8);
				regmap_write(regmap, 0x01, AddrRegL);
				regmap_write(regmap, 0x02, AddrRegH);
				regmap_write(regmap, 0x03, DataRegL);
				regmap_write(regmap, 0x04, DataRegH);
				regmap_write(regmap, 0x00, 0x03);
				uAddr += 4;
				Data += 4;
			}
			if (1 == (len & 0x03)) {
				AddrRegL = uAddr & 0xffff;
				AddrRegH = uAddr >> 16;
				DataRegL = Data[0];
				DataRegH = 0;
				regmap_write(regmap, 0x01, AddrRegL);
				regmap_write(regmap, 0x02, AddrRegH);
				regmap_write(regmap, 0x03, DataRegL);
				regmap_write(regmap, 0x04, DataRegH);
				regmap_write(regmap, 0x00, 0x03);
			} else if (2 == (len & 0x03)) {
				AddrRegL = uAddr & 0xffff;
				AddrRegH = uAddr >> 16;
				DataRegL = Data[0] | (Data[1] << 8);
				DataRegH = 0;
				regmap_write(regmap, 0x01, AddrRegL);
				regmap_write(regmap, 0x02, AddrRegH);
				regmap_write(regmap, 0x03, DataRegL);
				regmap_write(regmap, 0x04, DataRegH);
				regmap_write(regmap, 0x00, 0x03);
			} else if (3 == (len & 0x03)) {
				AddrRegL = uAddr & 0xffff;
				AddrRegH = uAddr >> 16;
				DataRegL = Data[0] | (Data[1] << 8);
				DataRegH = Data[2];
				regmap_write(regmap, 0x01, AddrRegL);
				regmap_write(regmap, 0x02, AddrRegH);
				regmap_write(regmap, 0x03, DataRegL);
				regmap_write(regmap, 0x04, DataRegH);
				regmap_write(regmap, 0x00, 0x03);
			}
		}
	} else
		ret = -EIO;

	return ret;
}

static int cm710x_write_firmware_Codec_CMD(
		struct cm710x_codec_priv *cm710x_codec,
		u8 *I2CCommand, int Count)
{
	int retSize = 0;
	int idx;
	u32 reg;
	u32 data;
	int ret = 0;

	for (idx = 0; idx < Count; idx++) {
		if (cm710x_codec->bSuspendRequestPending)
			return -ECANCELED;

		reg = I2CCommand[idx * 4];
		data = I2CCommand[(idx * 4) + 2] |
			I2CCommand[(idx * 4) + 3] << 8;
		if (reg != 0xFE)
			ret = regmap_write(cm710x_codec->virt_regmap,
					reg, data);
		else
			if (data > 20)
				msleep(data);
			else
				usleep_range(data * 1000, data * 1000 + 500);

		if (ret < 0) {
			pr_err("%s: Codec Init Failed in I2C failed in Command(%d)\n",
					__func__, idx);
			return ret;
		}

		retSize += 4;
	}

	return retSize;
}

static int cm710x_write_chunk_SPI(struct cm710x_codec_priv *cm710x_codec,
		u32 uAddr, u8 *fwData, int len)
{
	int iWriteSize = 0;
	int ret = 0;

	do {
		if (cm710x_codec->bSuspendRequestPending)
			return -ECANCELED;

		if (len > 16 * 1024)
			iWriteSize = 16 * 1024;
		else
			iWriteSize = len;
		len -= iWriteSize;
		dev_info(cm710x_codec->dev,
			"SPI Write chunk Addr 0x%08x Len = %d\n",
			uAddr, iWriteSize);
		ret = cm710x_write_SPI_Dsp(uAddr, fwData, iWriteSize);
		if (ret != 0) {
			pr_err("%s: Firmware Load Chunk SPI interface failed\n",
					__func__);
			ret = -ECANCELED;
			break;
		}
		uAddr += iWriteSize;
		fwData += iWriteSize;
	} while (len != 0);

	return ret;
}

static int cm710x_firmware_checksum(void *FirmwareData, u8 checksum, size_t len)
{
	u8  *fwData = FirmwareData;
	u8  uSum = 0;
	int i;

	for (i = 0; i < len; i++)
		uSum += fwData[i];
	checksum = checksum - 1 + uSum;
	checksum = ~checksum;

	return (checksum == 0);
}

static int cm710x_firmware_parsing(struct cm710x_codec_priv *cm710x_codec,
		void *FirmwareData, size_t len)
{
	u32 ulFirmwareSize = len;
	u8  *fwData = FirmwareData;
	int iI2cCmdCount;
	int iDspBlockCount;
	u32 uAddr;
	int idx;
	int lSize = 0;
	int ret = 0;
	u32 stardsp;
	u8  cCheckSumCodec, cCheckSumDsp;
	u8  *fwTmp;

	if ('C' != fwData[0] || 'M' != fwData[1]) {
		ret = -EINVAL;
		pr_err("%s: firmware image not correct\n", __func__);
		goto __EXIT;
	}

	iI2cCmdCount = fwData[2];
	iDspBlockCount = fwData[3];

	uAddr = fwData[4] | (fwData[5] << 8);
	cCheckSumCodec = fwData[6];
	cCheckSumDsp = fwData[7];
	if (uAddr != 0x08) {
		ret = -EINVAL;
		pr_err("%s: firmware image corrupted\n", __func__);
		goto __EXIT;
	}

	if (uAddr > ulFirmwareSize) {
		ret = -EINVAL;
		pr_err("%s: firmware image size incorrect %d\n",
				__func__, ulFirmwareSize);
		goto __EXIT;
	}

	fwData += uAddr;
	if (!cm710x_firmware_checksum(fwData, cCheckSumCodec, iI2cCmdCount*4)) {
		ret = -EINVAL;
		pr_err("%s: firmware image dsp setting checksum failed\n",
				__func__);
		goto __EXIT;
	}

	fwTmp = fwData + iI2cCmdCount * 4;
	uAddr = 0;
	for (idx = 0; idx < iDspBlockCount; idx++) {
		lSize = fwTmp[4] | (fwTmp[5] << 8) | (fwTmp[6] << 16) |
			(fwTmp[7] << 24);
		uAddr += 8 + lSize;
		fwTmp += 8 + lSize;
	}

	if (!cm710x_firmware_checksum(fwData + iI2cCmdCount * 4,
				cCheckSumDsp, uAddr)) {
		ret = -EINVAL;
		pr_err("%s: firmware image dsp code checksum failed\n",
				__func__);
		goto __EXIT;
	}

	lSize = cm710x_write_firmware_Codec_CMD(cm710x_codec,
			fwData, iI2cCmdCount);
	if (lSize < 0) {
		ret = lSize;
		pr_err("%s: firmware int codec failed\n", __func__);
		goto __EXIT;
	}

	if (lSize == iI2cCmdCount * 4)
		cm710x_codec->bDspMode = true;

	fwData += lSize;

	cm710x_codec->bDelayForPopNoise = true;

	idx = 0;
	do {
		if (cm710x_codec->bSuspendRequestPending)
			return -ECANCELED;

		ret = regmap_read(cm710x_codec->virt_regmap, CM710X_PWR_DSP_ST,
				&stardsp);
		if (ret < 0)
			pr_err("%s: regmap_read from CM710X_PWR_DSP_ST Failed\n",
				__func__);

		idx++;
		stardsp &= 0xFFFF;
		dev_info(cm710x_codec->dev, "stardsp = 0x%04x\n", stardsp);
		if (stardsp != 0x3ff)
			usleep_range(10000, 15000);
	} while (stardsp != 0x3ff && idx < 1000);

	if (stardsp != 0x3ff) {
		ret = -ENXIO;
		pr_err("%s: I2C command start DSP Failed\n", __func__);
		goto __EXIT;
	}

	for (idx = 0; idx < iDspBlockCount; idx++) {
		if (cm710x_codec->bSuspendRequestPending)
			return -ECANCELED;

		uAddr = fwData[0] | (fwData[1] << 8) | (fwData[2] << 16) |
			(fwData[3] << 24);
		lSize = fwData[4] | (fwData[5] << 8) | (fwData[6] << 16) |
			(fwData[7] << 24);
		fwData += 8;

		if (!strcmp(cm710x_codec->fw_parsing, "i2c")) {
			dev_info(cm710x_codec->dev,
					"I2C Load Block %d Addr 0x%08x Len = %d\n",
					idx, uAddr, lSize);
			ret = cm710x_dsp_mode_i2c_write_mem(
					cm710x_codec->real_regmap,
					uAddr, fwData, lSize);
		} else if (!strcmp(cm710x_codec->fw_parsing, "spi")) {
			dev_info(cm710x_codec->dev,
					"SPI Load Block %d Addr 0x%08x Len = %d\n",
					idx, uAddr, lSize);
			ret = cm710x_write_chunk_SPI(cm710x_codec,
					uAddr, fwData, lSize);
		} else
			dev_err(cm710x_codec->dev, "%s (%d) : cannot get fw parsing\n",
					__func__, __LINE__);

		if (ret != 0) {
			pr_err("%s: Load Firmware Failed in Block %d\n",
					__func__, idx);
			break;
		}

		fwData += lSize;
	}

	if (ret == 0) {
		usleep_range(1000, 1500);
		ret = regmap_write(cm710x_codec->virt_regmap, CM710X_PWR_DSP1,
				0x07FE);
	}

	usleep_range(1000, 1500);

__EXIT:
	return ret;
}

static int cm710x_compose_integer_value_from_string(
		char *float_str, s32 *calib_data)
{
	s32 before_decimal = 0;
	s32 after_decimal = 0;
	int temp_val = 0;
	char *temp;
	int ret;

	/* Extract the integer and decimal parts of a floating point
	 * value in the string as two integer values.
	 */
	ret = sscanf(float_str, "%d.%d", &before_decimal,
			&after_decimal);
	if (ret != 2) {
		pr_err("%s get mic cal data failed ret = %d\n",
				__func__, ret);
		return -EINVAL;
	}

	/* The number of digits after the decimal point
	 * cannot exceed 3
	 */
	temp =  strnstr(float_str, ".", strlen(float_str));
	if (temp == NULL) {
		pr_err("%s find '.' failed\n", __func__);
		return -EINVAL;
	}

	temp_val = strlen(++temp);
	if (temp_val > 3) {
		pr_err("%s get mic cal data format error\n",
				__func__);
		return -EINVAL;
	}

	/* The integer before the decimal point and the integer
	 * after the decimal point form a new integer, which is
	 * used to calculate the calibration step. For example,
	 * -23.125 is converted to -23125
	 * -23.07 is converted to -23070
	 * -23 is converted to -23000
	 */
	if (temp_val == 1)
		after_decimal *= 100;
	else if (temp_val == 2)
		after_decimal *= 10;

	if (before_decimal < 0)
		*calib_data = before_decimal * 1000 - after_decimal;
	else
		*calib_data = before_decimal * 1000 + after_decimal;

	return 0;
}

static s32 cm710x_calculate_volume_step_count(
		s32 mic_calib_data, s32 mic_calib_target)
{
	s32 mic_delta;
	s32 volume_step_size = 375; /*0.375dB*/
	s32 volume_step_count;
	s32 volume_step_count_mod;


	/* Calculate the difference between MIC and the target value,
	 * and then calculate how many steps are required, each step is
	 * 0.375dB. During calibration, the register value will be adjusted
	 * according to this step value, and the volume will eventually be
	 * adjusted to the target within the azimuth of plus or minus
	 * 0.187dB.
	 */
	mic_delta = mic_calib_data - mic_calib_target;
	volume_step_count = mic_delta / volume_step_size;
	volume_step_count_mod = abs(mic_delta -
		(mic_delta / volume_step_size) * volume_step_size);
	pr_debug("%s: mic volume step count=%d, mod=%d, step/2=%d\n",
			__func__, volume_step_count,
			volume_step_count_mod, volume_step_size / 2);
	if (volume_step_count_mod > volume_step_size / 2) {
		if (mic_delta >= 0)
			volume_step_count += 1;
		else
			volume_step_count -= 1;
	}

	return -volume_step_count;
}

static int cm710x_mic_calibration_firmware_parsing(
		struct cm710x_codec_priv *cm710x_codec,
		void *firmware_data, size_t len)
{
	u32 fw_size = len;
	u8  *fw_data = firmware_data;
	char *temp;
	char mic_temp_str[100] = {0};
	s32 calib_data[3]; /*[mic1 mic2 target]*/
	s32 mic1_sensitivity;
	s32 mic2_sensitivity;
	s32 target_sensitivity;
	int i, j = 0;
	int ret;

	if (fw_size >= sizeof(mic_temp_str)) {
		pr_err("%s Unexpected firmware size %d\n", __func__, fw_size);
		return -EINVAL;
	}
	memcpy(mic_temp_str, fw_data, fw_size);

	/* The content format of the MIC calibration file is as follows:
	 * "mic1_sensitivity_1kHz"=-23.457
	 * "mic2_sensitivity_1kHz"=-22.627
	 * "target"=-23.000
	 * We generate a string for each line to process.
	 */
	mic_temp_str[fw_size] = '\0';
	for (i = 0; i < fw_size; i++) {
		if (mic_temp_str[i] == '\x0a')
			mic_temp_str[i] = '\0';
	}

	/* Convert the string after the symbol "=" into the calibration
	 * data we need.
	 */
	for (i = 0; i < fw_size; i++) {
		if (mic_temp_str[i] == '=') {
			temp = &mic_temp_str[i] + 1;
			ret = cm710x_compose_integer_value_from_string(temp,
					&calib_data[j]);
			if (ret < 0) {
				pr_err("%s get mic cal data failed ret = %d\n",
						__func__, ret);
				return -EINVAL;
			}
			pr_debug("%s calibration data %d: %d\n", __func__,
						j, calib_data[j]);
			if (++j == 3)
				break;
		}
	}
	mic1_sensitivity = calib_data[0];
	mic2_sensitivity = calib_data[1];
	target_sensitivity = calib_data[2];

	cm710x_codec->mic_calib.mic1_volume_step_count =
		cm710x_calculate_volume_step_count(mic1_sensitivity,
				target_sensitivity);

	cm710x_codec->mic_calib.mic2_volume_step_count =
		cm710x_calculate_volume_step_count(mic2_sensitivity,
				target_sensitivity);

	return 0;
}

static void cm710x_mic_recalibration(struct cm710x_codec_priv *cm710x_codec)
{
	/* The firmware will be downloaded again after system resume,
	 * and the saved calibrated MIC volume will be written into
	 * the register again.
	 */
	if (cm710x_codec->mic_calib.mic_calibrated)
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_STO2_ADC_DIG_VOL,
			0x7f7f, cm710x_codec->mic_calib.mic_calibrated_reg_val);
}

static int cm710x_mic_calibration(struct cm710x_codec_priv *cm710x_codec)
{
	u32 mic_reg_val;
	s32 mic1_new_volume;
	s32 mic2_new_volume;
	int ret;

	if (cm710x_codec->mic_calib.mic_calibrated)
		return 0;

	/* Get the default mic volume from the register */
	ret = regmap_read(cm710x_codec->virt_regmap, CM710X_STO2_ADC_DIG_VOL,
				&mic_reg_val);
	if (ret < 0) {
		pr_err("%s: regmap_read from CM710X_STO2_ADC_DIG_VOL Failed\n",
			__func__);
		return ret;
	}

	/* Use the default volume plus the step value to get the
	 * calibrated volume.
	 */
	mic1_new_volume = ((mic_reg_val & 0x7f00) >> 8) +
				cm710x_codec->mic_calib.mic1_volume_step_count;
	mic2_new_volume = (mic_reg_val & 0x007f) +
				cm710x_codec->mic_calib.mic2_volume_step_count;
	pr_debug("%s: mic1 and mic2 new volume = %d, %d\n", __func__,
				mic1_new_volume, mic2_new_volume);
	if (mic1_new_volume < 0 || mic1_new_volume > 0x7f
			|| mic2_new_volume < 0 || mic2_new_volume > 0x7f){
		pr_err("%s: mic values not correct.\n",	__func__);
		return -EINVAL;
	}
	mic_reg_val = mic1_new_volume << 8 | mic2_new_volume;

	/* Write the calibrated MIC volume to the register to complete
	 * the calibration.
	 */
	regmap_update_bits(cm710x_codec->virt_regmap, CM710X_STO2_ADC_DIG_VOL,
				0x7f7f, mic_reg_val);
	cm710x_codec->mic_calib.mic_calibrated_reg_val = (mic_reg_val & 0x7f7f);
	pr_debug("%s success, new calibrated reg value = 0x%x\n",
		__func__, cm710x_codec->mic_calib.mic_calibrated_reg_val);
	cm710x_codec->mic_calib.mic_calibrated = true;

	return 0;
}

static int cm710x_download_calibration_firmware(
		struct cm710x_codec_priv *cm710x_codec)
{
	int ret = request_firmware(&cm710x_codec->mic_calib.mic_cal_fw,
			CM710X_MIC_CALIBRATION_FIRMWARE, cm710x_codec->dev);
	if (ret) {
		pr_err("%s: request_firmware failed: %d\n", __func__, ret);
		return ret;
	}
	ret = cm710x_mic_calibration_firmware_parsing(cm710x_codec,
			(void *)cm710x_codec->mic_calib.mic_cal_fw->data,
			cm710x_codec->mic_calib.mic_cal_fw->size);
	release_firmware(cm710x_codec->mic_calib.mic_cal_fw);
	if (ret) {
		pr_err("%s: cm710x_firmware_parsing failed: %d\n",
				__func__, ret);
		return ret;
	}
	ret = cm710x_mic_calibration(cm710x_codec);

	return ret;
}

static int cm710x_get_EQ_Default(struct cm710x_codec_priv *cm710x_codec)
{
	u32 i = 0, uData = 0;

	mutex_lock(&cm710x_codec->Dsp_Access_Lock);
	cm710x_dsp_mode_i2c_read_mem(cm710x_codec->real_regmap,
			0x5FFC0020, &uData);
	mutex_unlock(&cm710x_codec->Dsp_Access_Lock);

	cm710x_codec->bEnableSpkEQ = uData & 0x01;
	cm710x_codec->bEnableMicEQ = (uData >> 1) & 0x01;
	cm710x_codec->cSpkLeftChPreGain = (uData >> 2) & 0x0F;
	cm710x_codec->cSpkRightChPreGain = (uData >> 6) & 0x0F;
	cm710x_codec->cMicPreGain = (uData >> 10) & 0x0F;

	for (i = 0; i < 8; i++) {
		mutex_lock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_dsp_mode_i2c_read_mem(cm710x_codec->real_regmap,
				0x5FFC0038 + i * 4, &uData);
		mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_codec->SPKLEQParam[i].Enable = uData & 0x01;
		cm710x_codec->SPKLEQParam[i].CenterFreq = (uData >> 1) & 0x7FFF;
		cm710x_codec->SPKLEQParam[i].Gain = (uData >> 16) & 0x3F;
		cm710x_codec->SPKLEQParam[i].Q = (uData >> 22) & 0x1FF;
		cm710x_codec->SPKLEQParam[i].HPF = (uData >> 31) & 0x01;
	}

	for (i = 0; i < 8; i++) {
		mutex_lock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_dsp_mode_i2c_read_mem(cm710x_codec->real_regmap,
				0x5FFC0058 + i * 4, &uData);
		mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_codec->SPKREQParam[i].Enable = uData & 0x01;
		cm710x_codec->SPKREQParam[i].CenterFreq = (uData >> 1) & 0x7FFF;
		cm710x_codec->SPKREQParam[i].Gain = (uData >> 16) & 0x3F;
		cm710x_codec->SPKREQParam[i].Q = (uData >> 22) & 0x1FF;
		cm710x_codec->SPKREQParam[i].HPF = (uData >> 31) & 0x01;
		dev_info(cm710x_codec->dev, "%s SPK_R_EQ Param = 0x%08x\n",
						__func__, uData);
	}

	for (i = 0; i < 8; i++) {
		mutex_lock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_dsp_mode_i2c_read_mem(cm710x_codec->real_regmap,
				0x5FFC0078 + i * 4, &uData);
		mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_codec->MICEQParam[i].Enable = uData & 0x01;
		cm710x_codec->MICEQParam[i].CenterFreq = (uData >> 1) & 0x7FFF;
		cm710x_codec->MICEQParam[i].Gain = (uData >> 16) & 0x3F;
		cm710x_codec->MICEQParam[i].Q = (uData >> 22) & 0x1FF;
		cm710x_codec->MICEQParam[i].HPF = (uData >> 31) & 0x01;
		dev_info(cm710x_codec->dev, "%s MIC_EQ Param = 0x%08x\n",
						__func__, uData);
	}

	return 0;
}

static void cm710x_mic_bypass_dsp(struct cm710x_codec_priv *cm710x_codec,
					bool enable)
{
	const u32 DSPElement = 0x80000000;

	if (enable) {
		/* MIC bypass DSP */
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_TDM1_CTRL2, 0xff, 0x31);
		/* trigger DSP CPU stop */
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_PWR_DSP1, 0x1, 0x1);
		cm710x_codec->bEnableMicDspBypass = true;
	} else {
		/* MIC do not bypass DSP */
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_TDM1_CTRL2, 0xff, 0x50);
		/* trigger DSP CPU run */
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_PWR_DSP1, 0x1, 0x0);
		usleep_range(50000, 50100);
		cm710x_codec->bEnableMicDspBypass = false;

		mutex_lock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_dsp_mode_i2c_write_mem(cm710x_codec->real_regmap,
				0x5FFC0004, (u8 *)&DSPElement, 4);
		mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
	}
}

static int cm710x_download_firmware(struct cm710x_codec_priv *cm710x_codec)
{
	u32 version = 0;

	int ret = request_firmware(&cm710x_codec->fw, CM710X_FIRMWARE,
			cm710x_codec->dev);
	if (ret) {
		pr_err("%s: request_firmware failed: %d\n", __func__, ret);
		return ret;
	}

	ret = cm710x_firmware_parsing(cm710x_codec,
			(void *)cm710x_codec->fw->data,
			cm710x_codec->fw->size);
	release_firmware(cm710x_codec->fw);
	if (ret) {
		pr_err("%s: cm710x_firmware_parsing failed: %d\n",
				__func__, ret);
		return ret;
	}

	msleep(20);
	mutex_lock(&cm710x_codec->Dsp_Access_Lock);
	cm710x_dsp_mode_i2c_read_mem(cm710x_codec->real_regmap,
			0x5FFC001C, &version);
	mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
	pr_debug("%s (%d): Version = %08X\n", __func__, __LINE__, version);

	cm710x_mic_bypass_dsp(cm710x_codec, true);

	ret = cm710x_get_EQ_Default(cm710x_codec);

	return 0;
}

static void cm710x_set_eq_value(struct cm710x_codec_priv *cm710x_codec,
					struct EQParam *pEQParam, u32 uAddr);
static void cm710x_update_eq_pregain_enable(
					struct cm710x_codec_priv *cm710x_codec);

static void cm710x_firmware_download_work(struct work_struct *work)
{
	struct cm710x_codec_priv *cm710x_codec = container_of(work,
			struct cm710x_codec_priv, fw_download_work);
	int ret = 0;
	u8 idx;

	ret = cm710x_download_firmware(cm710x_codec);
	if (ret == 0) {
		usleep_range(10000, 15000);
		for (idx = 0; idx < 8; idx++)
			cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->SPKLEQParam[idx], 0x5FFC0038 + 4 * idx);

		for (idx = 0; idx < 8; idx++)
			cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->SPKREQParam[idx], 0x5FFC0058 + 4 * idx);

		for (idx = 0; idx < 8; idx++)
			cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->MICEQParam[idx], 0x5FFC0078 + 4 * idx);

		cm710x_update_eq_pregain_enable(cm710x_codec);
		cm710x_mic_recalibration(cm710x_codec);

		complete_all(&cm710x_codec->fw_download_complete);
	}
}

static void cm710x_pop_noise_work(struct work_struct *work)
{
	u32 lineout_reg = 0;
	struct delayed_work *pDelay = container_of(work,
			struct delayed_work, work);
	struct cm710x_codec_priv *cm710x_codec = container_of(pDelay,
			struct cm710x_codec_priv, avoid_pop_noise);

	int rc = wait_for_completion_timeout(
			&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return;
	}

	/* Read LINEOUT Control register (MX-01h), if
	 * LOUT1 and LOUT2 are unmute, enable audio AMP.
	 */
	regmap_read(cm710x_codec->virt_regmap, CM710X_LOUT1,
			&lineout_reg);
	if ((lineout_reg & 0xa000) == 0) {
		dev_dbg(cm710x_codec->dev, "SPK gpio to high\n");
		rc = pinctrl_select_state(cm710x_codec->pinctrl,
				cm710x_codec->pin_spk_en);
		if (rc)
			dev_err(cm710x_codec->dev,
				"pinctrl_select_state error");
	}
}

static int cm710x_pm_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	struct cm710x_codec_priv *cm710x_codec =
			container_of(nb, struct cm710x_codec_priv, pm_nb);

	switch (mode) {
	case PM_SUSPEND_PREPARE:
		cm710x_codec->bSuspendRequestPending = true;
		cancel_work_sync(&cm710x_codec->fw_download_work);
		cm710x_codec->bSuspendRequestPending = false;
		break;
	case PM_POST_SUSPEND:
		schedule_work(&cm710x_codec->fw_download_work);
		break;
	default:
		break;
	}
	return 0;
}

static int cm710x_codec_probe(struct snd_soc_component *component)
{
	int ret;
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);

	ret = pinctrl_select_state(cm710x_codec->pinctrl,
			cm710x_codec->pin_default);
	if (ret != 0) {
		devm_pinctrl_put(cm710x_codec->pinctrl);
		dev_err(cm710x_codec->dev, "Failed to set codec cm710x pin state\n");
		return ret;
	}

	cm710x_codec->mclk = clk_get(cm710x_codec->dev, "cm7104_mclk");
	if (IS_ERR(cm710x_codec->mclk)) {
		dev_err(cm710x_codec->dev, "Get CM710X MCLK error\n");
		return -EPROBE_DEFER;
	}

	ret = clk_prepare_enable(cm710x_codec->mclk);
	if (ret < 0) {
		dev_err(cm710x_codec->dev, "Enable CM710x MCLK error\n");
		clk_put(cm710x_codec->mclk);
		return -EPROBE_DEFER;
	}
	dev_info(cm710x_codec->dev, "Enable CM710X MCLK success\n");

	INIT_DELAYED_WORK(&cm710x_codec->avoid_pop_noise,
		cm710x_pop_noise_work);
	cm710x_codec->pm_nb.notifier_call = cm710x_pm_notify;
	register_pm_notifier(&cm710x_codec->pm_nb);
	ret = cm710x_download_firmware(cm710x_codec);
	cm710x_download_calibration_firmware(cm710x_codec);

	complete_all(&cm710x_codec->fw_download_complete);

	return ret;
}

static void cm710x_codec_remove(struct snd_soc_component *component)
{
	int ret;
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);

	cancel_delayed_work_sync(&cm710x_codec->avoid_pop_noise);

	ret = regmap_write(cm710x_codec->virt_regmap, CM710X_RESET, 0x10ec);

	if (!IS_ERR_OR_NULL(cm710x_codec->pin_suspend)) {
		ret = pinctrl_select_state(cm710x_codec->pinctrl,
				cm710x_codec->pin_suspend);
		if (ret)
			dev_err(cm710x_codec->dev,
				"pinctrl_select_state error");
	}
	unregister_pm_notifier(&cm710x_codec->pm_nb);
}

static void cm710x_update_DSP_Effect_enable(
				struct cm710x_codec_priv *cm710x_codec)
{
	u32 DSPElement = 0;

	if (cm710x_codec->bEnableMicDspProcess)
		DSPElement = 0x1;

	dev_info(cm710x_codec->dev, "%s DSPElement = 0x%08x\n",
		__func__, DSPElement);
	mutex_lock(&cm710x_codec->Dsp_Access_Lock);
	cm710x_dsp_mode_i2c_write_mem(cm710x_codec->real_regmap,
				0x5FFC0030, (u8 *)&DSPElement, 4);
	mutex_unlock(&cm710x_codec->Dsp_Access_Lock);

}

static void cm710x_update_eq_pregain_enable(
				struct cm710x_codec_priv *cm710x_codec)
{
	u32 EQElement = 0;

	EQElement = cm710x_codec->bEnableSpkEQ |
					(cm710x_codec->bEnableMicEQ << 1) |
					(cm710x_codec->cSpkLeftChPreGain << 2) |
					(cm710x_codec->cSpkLeftChPreGain << 6) |
					(cm710x_codec->cMicPreGain << 10);

	dev_info(cm710x_codec->dev, "%s EQElement = 0x%08x\n",
		__func__, EQElement);
	mutex_lock(&cm710x_codec->Dsp_Access_Lock);
	cm710x_dsp_mode_i2c_write_mem(cm710x_codec->real_regmap,
				0x5FFC0020, (u8 *)&EQElement, 4);
	mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
}

static int cm710x_put_vu(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	unsigned int reg = mc->reg;

	int rc = wait_for_completion_timeout(
			&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	switch (reg) {
	case MIC_DSP_BYPASS_ON_OFF:
		cm710x_mic_bypass_dsp(cm710x_codec,
				ucontrol->value.integer.value[0]);
		dev_info(cm710x_codec->dev,
			"%s MIC_DSP_BYPASS_ON_OFF set Value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case MIC_DSP_PROCESS_ON_OFF:
		if (ucontrol->value.integer.value[0] == 0)
			cm710x_codec->bEnableMicDspProcess = false;
		else
			cm710x_codec->bEnableMicDspProcess = true;

		cm710x_update_DSP_Effect_enable(cm710x_codec);
		dev_info(cm710x_codec->dev,
			"%s MIC_DSP_PROCESS_ON_OFF set Value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case MIC_DSP_AEC_ON_OFF:
		if (ucontrol->value.integer.value[0] == 0)
			cm710x_codec->bEnableAEC = false;
		else
			cm710x_codec->bEnableAEC = true;

		/* Skip AEC for headphone devices */
		if (cm710x_codec->bEnableAEC && !cm710x_codec->bHeadphoneEnable)
			cm710x_codec->bSkipAEC = false;
		else
			cm710x_codec->bSkipAEC = true;

		cm710x_update_DSP_Effect_Param(cm710x_codec,
			MIC_DSP_SKIP_AEC - MIC_DSP_MAXNR_AEC,
			cm710x_codec->bSkipAEC);

		dev_info(cm710x_codec->dev,
			"%s MIC_DSP_AEC_ON_OFF set Value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;

	case SPK_EQ_ON_OFF:
		if (ucontrol->value.integer.value[0] == 0)
			cm710x_codec->bEnableSpkEQ = false;
		else
			cm710x_codec->bEnableSpkEQ = true;

		cm710x_update_eq_pregain_enable(cm710x_codec);
		dev_info(cm710x_codec->dev,
			"%s SPK_EQ_ON_OFF set Value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case MIC_EQ_ON_OFF:
		if (ucontrol->value.integer.value[0] == 0)
			cm710x_codec->bEnableMicEQ = false;
		else
			cm710x_codec->bEnableMicEQ = true;

		cm710x_update_eq_pregain_enable(cm710x_codec);
		dev_info(cm710x_codec->dev,
			"%s MIC_EQ_ON_OFF set Value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

		break;
	case MIC_EQ_AGC_ON_OFF:
		if (ucontrol->value.integer.value[0] == 0)
			cm710x_codec->bEnableAGC = false;
		else
			cm710x_codec->bEnableAGC = true;

		cm710x_update_eq_pregain_enable(cm710x_codec);
		dev_info(cm710x_codec->dev,
			"%s MIC_EQ_AGC_ON_OFF set Value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	}

	return 0;
}

static int cm710x_get_vu(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	unsigned int reg = mc->reg;

	switch (reg) {
	case MIC_DSP_BYPASS_ON_OFF:
		if (cm710x_codec->bEnableMicDspBypass)
			ucontrol->value.integer.value[0] = true;
		else
			ucontrol->value.integer.value[0] = false;

		dev_info(cm710x_codec->dev,
			"%s MIC_DSP_BYPASS_ON_OFF get value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case MIC_DSP_PROCESS_ON_OFF:
		if (cm710x_codec->bEnableMicDspProcess)
			ucontrol->value.integer.value[0] = true;
		else
			ucontrol->value.integer.value[0] = false;

		dev_info(cm710x_codec->dev,
			"%s MIC_DSP_PROCESS_ON_OFF get value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case MIC_DSP_AEC_ON_OFF:
		if (cm710x_codec->bEnableAEC)
			ucontrol->value.integer.value[0] = true;
		else
			ucontrol->value.integer.value[0] = false;

		dev_info(cm710x_codec->dev,
			"%s MIC_DSP_AEC_ON_OFF get value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case SPK_EQ_ON_OFF:
		if (cm710x_codec->bEnableSpkEQ)
			ucontrol->value.integer.value[0] = true;
		else
			ucontrol->value.integer.value[0] = false;

		dev_info(cm710x_codec->dev,
			"%s SPK_EQ_ON_OFF get value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case MIC_EQ_ON_OFF:
		if (cm710x_codec->bEnableMicEQ)
			ucontrol->value.integer.value[0] = true;
		else
			ucontrol->value.integer.value[0] = false;

		dev_info(cm710x_codec->dev,
			"%s MIC_EQ_ON_OFF get value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	case MIC_EQ_AGC_ON_OFF:
		if (cm710x_codec->bEnableAGC)
			ucontrol->value.integer.value[0] = true;
		else
			ucontrol->value.integer.value[0] = false;

		dev_info(cm710x_codec->dev,
			"%s MIC_EQ_AGC_ON_OFF get value = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
		break;
	}

	return 0;
}

static void cm710x_set_eq_value(struct cm710x_codec_priv *cm710x_codec,
				struct EQParam *pEQParam, u32 uAddr)
{
	u32 EQElement = 0;

	EQElement = pEQParam->Enable | (pEQParam->CenterFreq << 1) |
				(pEQParam->Gain << 16) | (pEQParam->Q << 22) |
				(pEQParam->HPF << 31);
	dev_info(cm710x_codec->dev, "%s EQElement = 0x%08x Addr = 0x%08x\n",
		__func__, EQElement, uAddr);
	mutex_lock(&cm710x_codec->Dsp_Access_Lock);
	cm710x_dsp_mode_i2c_write_mem(cm710x_codec->real_regmap,
				uAddr, (u8 *)&EQElement, 4);
	mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
}

static int cm710x_put_eq_gain(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);

	int rc = 0;

	unsigned int reg = mc->reg;
	u8 idx;
	u32 gain;
	gain = ucontrol->value.integer.value[0];

	rc = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	switch (reg) {
	case SPK_L_EQ_PRE_GAIN:
		cm710x_codec->cSpkLeftChPreGain = (char)(12 -  gain);
		cm710x_update_eq_pregain_enable(cm710x_codec);
		break;
	case SPK_R_EQ_PRE_GAIN:
		cm710x_codec->cSpkRightChPreGain = (char)(12 -  gain);
		cm710x_update_eq_pregain_enable(cm710x_codec);
		break;
	case MIC_EQ_PRE_GAIN:
		cm710x_codec->cMicPreGain = (char)(12 -  gain);
		cm710x_update_eq_pregain_enable(cm710x_codec);
		break;
	case SPK_L_EQ_LOWPASS_GAIN:
	case SPK_L_EQ_BAND_01_GAIN:
	case SPK_L_EQ_BAND_02_GAIN:
	case SPK_L_EQ_BAND_03_GAIN:
	case SPK_L_EQ_BAND_04_GAIN:
	case SPK_L_EQ_BAND_05_GAIN:
	case SPK_L_EQ_BAND_06_GAIN:
	case SPK_L_EQ_HIGHPASS_GAIN:
		idx = reg - SPK_L_EQ_LOWPASS_GAIN;
		cm710x_codec->SPKLEQParam[idx].Gain = gain -  20;
		if (cm710x_codec->SPKLEQParam[idx].Gain != 0)
			cm710x_codec->SPKLEQParam[idx].Enable = true;
		else
			cm710x_codec->SPKLEQParam[idx].Enable = false;
			cm710x_set_eq_value(cm710x_codec,
				&cm710x_codec->SPKLEQParam[idx],
				0x5FFC0038 + 4 * idx);
		break;
	case SPK_R_EQ_LOWPASS_GAIN:
	case SPK_R_EQ_BAND_01_GAIN:
	case SPK_R_EQ_BAND_02_GAIN:
	case SPK_R_EQ_BAND_03_GAIN:
	case SPK_R_EQ_BAND_04_GAIN:
	case SPK_R_EQ_BAND_05_GAIN:
	case SPK_R_EQ_BAND_06_GAIN:
	case SPK_R_EQ_HIGHPASS_GAIN:
		idx = reg - SPK_R_EQ_LOWPASS_GAIN;
		cm710x_codec->SPKREQParam[idx].Gain = gain -  20;
		if (cm710x_codec->SPKREQParam[idx].Gain != 0)
			cm710x_codec->SPKREQParam[idx].Enable = true;
		else
			cm710x_codec->SPKREQParam[idx].Enable = false;
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->SPKREQParam[idx], 0x5FFC0058 + 4 * idx);
		break;
	case MIC_EQ_LOWPASS_GAIN:
	case MIC_EQ_BAND_01_GAIN:
	case MIC_EQ_BAND_02_GAIN:
	case MIC_EQ_BAND_03_GAIN:
	case MIC_EQ_BAND_04_GAIN:
	case MIC_EQ_BAND_05_GAIN:
	case MIC_EQ_BAND_06_GAIN:
	case MIC_EQ_HIGHPASS_GAIN:
		idx = reg - MIC_EQ_LOWPASS_GAIN;
		cm710x_codec->MICEQParam[idx].Gain = gain -  20;
		if (cm710x_codec->MICEQParam[idx].Gain != 0)
			cm710x_codec->MICEQParam[idx].Enable = true;
		else
			cm710x_codec->MICEQParam[idx].Enable = false;
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->MICEQParam[idx], 0x5FFC0078 + 4 * idx);
		break;
	default:
		break;
	}

	return 0;
}

static int cm710x_get_eq_gain(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	unsigned int reg = mc->reg;
	u8 idx;
	char gain;

	switch (reg) {
	case SPK_L_EQ_PRE_GAIN:
		ucontrol->value.integer.value[0] =
					12 - cm710x_codec->cSpkLeftChPreGain;
		break;
	case SPK_R_EQ_PRE_GAIN:
		ucontrol->value.integer.value[0] =
					12 - cm710x_codec->cSpkRightChPreGain;
		break;
	case MIC_EQ_PRE_GAIN:
		ucontrol->value.integer.value[0] =
					12 - cm710x_codec->cMicPreGain;
		break;
	case SPK_L_EQ_LOWPASS_GAIN:
	case SPK_L_EQ_BAND_01_GAIN:
	case SPK_L_EQ_BAND_02_GAIN:
	case SPK_L_EQ_BAND_03_GAIN:
	case SPK_L_EQ_BAND_04_GAIN:
	case SPK_L_EQ_BAND_05_GAIN:
	case SPK_L_EQ_BAND_06_GAIN:
	case SPK_L_EQ_HIGHPASS_GAIN:
		idx = reg - SPK_L_EQ_LOWPASS_GAIN;
		gain = cm710x_codec->SPKLEQParam[idx].Gain;
		if (gain & 0x20)
			gain = gain - 64;
		gain = gain + 20;
		ucontrol->value.integer.value[0] = gain;
		break;
	case SPK_R_EQ_LOWPASS_GAIN:
	case SPK_R_EQ_BAND_01_GAIN:
	case SPK_R_EQ_BAND_02_GAIN:
	case SPK_R_EQ_BAND_03_GAIN:
	case SPK_R_EQ_BAND_04_GAIN:
	case SPK_R_EQ_BAND_05_GAIN:
	case SPK_R_EQ_BAND_06_GAIN:
	case SPK_R_EQ_HIGHPASS_GAIN:
		idx = reg - SPK_R_EQ_LOWPASS_GAIN;
		gain = cm710x_codec->SPKREQParam[idx].Gain;
		if (gain & 0x20)
			gain = gain - 64;
		gain = gain + 20;
		ucontrol->value.integer.value[0] = gain;
		break;
	case MIC_EQ_LOWPASS_GAIN:
	case MIC_EQ_BAND_01_GAIN:
	case MIC_EQ_BAND_02_GAIN:
	case MIC_EQ_BAND_03_GAIN:
	case MIC_EQ_BAND_04_GAIN:
	case MIC_EQ_BAND_05_GAIN:
	case MIC_EQ_BAND_06_GAIN:
	case MIC_EQ_HIGHPASS_GAIN:
		idx = reg - MIC_EQ_LOWPASS_GAIN;
		gain = cm710x_codec->MICEQParam[idx].Gain;
		if (gain & 0x20)
			gain = gain - 64;
		gain = gain + 20;
		ucontrol->value.integer.value[0] = gain;
		break;
	default:
		break;
	}
	return 0;
}

static int cm710x_put_eq_fc(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);

	int rc = 0;
	unsigned int reg = mc->reg;
	u8 idx;
	u32 fc;

	fc = ucontrol->value.integer.value[0];

	rc = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	switch (reg) {
	case SPK_L_EQ_LOWPASS_FC:
	case SPK_L_EQ_BAND_01_FC:
	case SPK_L_EQ_BAND_02_FC:
	case SPK_L_EQ_BAND_03_FC:
	case SPK_L_EQ_BAND_04_FC:
	case SPK_L_EQ_BAND_05_FC:
	case SPK_L_EQ_BAND_06_FC:
	case SPK_L_EQ_HIGHPASS_FC:
		idx = reg - SPK_L_EQ_LOWPASS_FC;
		cm710x_codec->SPKLEQParam[idx].CenterFreq = fc;
		dev_info(cm710x_codec->dev, "%s idx = %d, Enable = %d\n",
			__func__, idx, cm710x_codec->SPKLEQParam[idx].Enable);
	    dev_info(cm710x_codec->dev, "%s CenterFreq = %d\n",
			__func__, cm710x_codec->SPKLEQParam[idx].CenterFreq);
		dev_info(cm710x_codec->dev, "%s Gain = %d\n",
			__func__, cm710x_codec->SPKLEQParam[idx].Gain);
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->SPKLEQParam[idx], 0x5FFC0038 + 4 * idx);
		break;
	case SPK_R_EQ_LOWPASS_FC:
	case SPK_R_EQ_BAND_01_FC:
	case SPK_R_EQ_BAND_02_FC:
	case SPK_R_EQ_BAND_03_FC:
	case SPK_R_EQ_BAND_04_FC:
	case SPK_R_EQ_BAND_05_FC:
	case SPK_R_EQ_BAND_06_FC:
	case SPK_R_EQ_HIGHPASS_FC:
		idx = reg - SPK_R_EQ_LOWPASS_FC;
		cm710x_codec->SPKREQParam[idx].CenterFreq = fc;
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->SPKREQParam[idx], 0x5FFC0058 + 4 * idx);
		break;
	case MIC_EQ_LOWPASS_FC:
	case MIC_EQ_BAND_01_FC:
	case MIC_EQ_BAND_02_FC:
	case MIC_EQ_BAND_03_FC:
	case MIC_EQ_BAND_04_FC:
	case MIC_EQ_BAND_05_FC:
	case MIC_EQ_BAND_06_FC:
	case MIC_EQ_HIGHPASS_FC:
		idx = reg - MIC_EQ_LOWPASS_FC;
		cm710x_codec->MICEQParam[idx].CenterFreq = fc;
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->MICEQParam[idx],	0x5FFC0078 + 4 * idx);
		break;
	default:
		break;
	}

	return 0;
}

static int cm710x_get_eq_fc(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	unsigned int reg = mc->reg;
	u8 idx;

	switch (reg) {
	case SPK_L_EQ_LOWPASS_FC:
	case SPK_L_EQ_BAND_01_FC:
	case SPK_L_EQ_BAND_02_FC:
	case SPK_L_EQ_BAND_03_FC:
	case SPK_L_EQ_BAND_04_FC:
	case SPK_L_EQ_BAND_05_FC:
	case SPK_L_EQ_BAND_06_FC:
	case SPK_L_EQ_HIGHPASS_FC:
		idx = reg - SPK_L_EQ_LOWPASS_FC;
		ucontrol->value.integer.value[0] =
			cm710x_codec->SPKLEQParam[idx].CenterFreq;
		break;
	case SPK_R_EQ_LOWPASS_FC:
	case SPK_R_EQ_BAND_01_FC:
	case SPK_R_EQ_BAND_02_FC:
	case SPK_R_EQ_BAND_03_FC:
	case SPK_R_EQ_BAND_04_FC:
	case SPK_R_EQ_BAND_05_FC:
	case SPK_R_EQ_BAND_06_FC:
	case SPK_R_EQ_HIGHPASS_FC:
		idx = reg - SPK_R_EQ_LOWPASS_FC;
		ucontrol->value.integer.value[0] =
			cm710x_codec->SPKREQParam[idx].CenterFreq;
		break;
	case MIC_EQ_LOWPASS_FC:
	case MIC_EQ_BAND_01_FC:
	case MIC_EQ_BAND_02_FC:
	case MIC_EQ_BAND_03_FC:
	case MIC_EQ_BAND_04_FC:
	case MIC_EQ_BAND_05_FC:
	case MIC_EQ_BAND_06_FC:
	case MIC_EQ_HIGHPASS_FC:
		idx = reg - MIC_EQ_LOWPASS_FC;
		ucontrol->value.integer.value[0] =
			cm710x_codec->MICEQParam[idx].CenterFreq;
		break;
	default:
		break;
	}
	return 0;
}

static int cm710x_put_eq_q(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);

	int rc = 0;

	unsigned int reg = mc->reg;
	u8 idx;
	u32 q;

	q = ucontrol->value.integer.value[0];
	rc = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	switch (reg) {
	case SPK_L_EQ_BAND_01_Q:
	case SPK_L_EQ_BAND_02_Q:
	case SPK_L_EQ_BAND_03_Q:
	case SPK_L_EQ_BAND_04_Q:
	case SPK_L_EQ_BAND_05_Q:
	case SPK_L_EQ_BAND_06_Q:
		idx = reg - SPK_L_EQ_BAND_01_Q;
		cm710x_codec->SPKLEQParam[idx+1].Q = q + 1;
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->SPKLEQParam[idx+1],
			0x5FFC003C + 4 * idx);
		break;
	case SPK_R_EQ_BAND_01_Q:
	case SPK_R_EQ_BAND_02_Q:
	case SPK_R_EQ_BAND_03_Q:
	case SPK_R_EQ_BAND_04_Q:
	case SPK_R_EQ_BAND_05_Q:
	case SPK_R_EQ_BAND_06_Q:
		idx = reg - SPK_R_EQ_BAND_01_Q;
		cm710x_codec->SPKREQParam[idx+1].Q = q + 1;
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->SPKREQParam[idx+1],
			0x5FFC005C + 4 * idx);
		break;
	case MIC_EQ_BAND_01_Q:
	case MIC_EQ_BAND_02_Q:
	case MIC_EQ_BAND_03_Q:
	case MIC_EQ_BAND_04_Q:
	case MIC_EQ_BAND_05_Q:
	case MIC_EQ_BAND_06_Q:
		idx = reg - MIC_EQ_BAND_01_Q;
		cm710x_codec->MICEQParam[idx+1].Q = q + 1;
		cm710x_set_eq_value(cm710x_codec,
			&cm710x_codec->MICEQParam[idx+1], 0x5FFC007C + 4 * idx);
		break;
	default:
		break;
	}

	return 0;
}

static int cm710x_get_eq_q(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	unsigned int reg = mc->reg;
	u8 idx;

	switch (reg) {
	case SPK_L_EQ_BAND_01_Q:
	case SPK_L_EQ_BAND_02_Q:
	case SPK_L_EQ_BAND_03_Q:
	case SPK_L_EQ_BAND_04_Q:
	case SPK_L_EQ_BAND_05_Q:
	case SPK_L_EQ_BAND_06_Q:
		idx = reg - SPK_L_EQ_BAND_01_Q;
		ucontrol->value.integer.value[0] =
			cm710x_codec->SPKLEQParam[idx+1].Q - 1;
		break;
	case SPK_R_EQ_BAND_01_Q:
	case SPK_R_EQ_BAND_02_Q:
	case SPK_R_EQ_BAND_03_Q:
	case SPK_R_EQ_BAND_04_Q:
	case SPK_R_EQ_BAND_05_Q:
	case SPK_R_EQ_BAND_06_Q:
		idx = reg - SPK_R_EQ_BAND_01_Q;
		ucontrol->value.integer.value[0] =
			cm710x_codec->SPKREQParam[idx+1].Q - 1;
		break;
	case MIC_EQ_BAND_01_Q:
	case MIC_EQ_BAND_02_Q:
	case MIC_EQ_BAND_03_Q:
	case MIC_EQ_BAND_04_Q:
	case MIC_EQ_BAND_05_Q:
	case MIC_EQ_BAND_06_Q:
		idx = reg - MIC_EQ_BAND_01_Q;
		ucontrol->value.integer.value[0] =
			cm710x_codec->MICEQParam[idx+1].Q - 1;
		break;
	default:
		break;
	}
	return 0;
}

static void cm710x_update_DSP_Effect_Param
(
	struct cm710x_codec_priv *cm710x_codec,
	char type,
	long value
)
{
	u32 DSPElement = 0;

	DSPElement = cm710x_codec->bEnableMicDspProcess | (type & 0x7) << 2 |
					(value & 0x7FFFFF) << 5 | 0x2;

	dev_info(cm710x_codec->dev, "%s DSPElement = 0x%08x\n",
					__func__, DSPElement);
	usleep_range(30000, 35000);
	mutex_lock(&cm710x_codec->Dsp_Access_Lock);
	cm710x_dsp_mode_i2c_write_mem(cm710x_codec->real_regmap,
				0x5FFC0030, (u8 *)&DSPElement, 4);
	mutex_unlock(&cm710x_codec->Dsp_Access_Lock);

}

static int  cm710x_put_dsp_param(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);

	int rc = 0;

	unsigned int reg = mc->reg;
	u32 q;

	q = ucontrol->value.integer.value[0];
	rc = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	switch (reg) {
	case MIC_DSP_MAXNR_AEC:
		q = q & 0x7FFF;
		cm710x_update_DSP_Effect_Param(cm710x_codec,
				MIC_DSP_MAXNR_AEC - MIC_DSP_MAXNR_AEC, q);
		cm710x_codec->sMaxNRAEC = (short)q;
		break;
	case MIC_DSP_NOISEGATE:
		q = q & 0x7FFFFF;
		cm710x_update_DSP_Effect_Param(cm710x_codec,
				MIC_DSP_NOISEGATE - MIC_DSP_MAXNR_AEC, q);
		cm710x_codec->NoiseGate = q;
		break;
	case MIC_DSP_TBRRLOW:
		cm710x_update_DSP_Effect_Param(cm710x_codec,
				MIC_DSP_TBRRLOW - MIC_DSP_MAXNR_AEC, q);
		cm710x_codec->cTbrrLowValue = (char)q;
		break;
	case MIC_DSP_SKIP_AEC:
		cm710x_update_DSP_Effect_Param(cm710x_codec,
				MIC_DSP_SKIP_AEC - MIC_DSP_MAXNR_AEC, q);
		cm710x_codec->bSkipAEC = (char)q;
		break;
	case MIC_DSP_MAXNR_BF:
		q = q & 0x7FFF;
		cm710x_update_DSP_Effect_Param(cm710x_codec,
				MIC_DSP_MAXNR_BF - MIC_DSP_MAXNR_AEC, q);
		cm710x_codec->sMaxNRBF = (short)q;
		break;
	case MIC_DSP_MAXNR_BF_MODE:
		cm710x_update_DSP_Effect_Param(cm710x_codec,
				MIC_DSP_MAXNR_BF_MODE - MIC_DSP_MAXNR_AEC, q);
		cm710x_codec->sMaxNRBFMode = (short)q;
		break;
	default:
		break;
	}

	return 0;
}

static int  cm710x_get_dsp_param(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	unsigned int reg = mc->reg;

	switch (reg) {
	case MIC_DSP_MAXNR_AEC:
		ucontrol->value.integer.value[0] = cm710x_codec->sMaxNRAEC;
		break;
	case MIC_DSP_NOISEGATE:
		ucontrol->value.integer.value[0] = cm710x_codec->NoiseGate;
		break;
	case MIC_DSP_TBRRLOW:
		ucontrol->value.integer.value[0] = cm710x_codec->cTbrrLowValue;
		break;
	case MIC_DSP_SKIP_AEC:
		ucontrol->value.integer.value[0] = cm710x_codec->bSkipAEC;
		break;
	case MIC_DSP_MAXNR_BF:
		ucontrol->value.integer.value[0] = cm710x_codec->sMaxNRBF;
		break;
	case MIC_DSP_MAXNR_BF_MODE:
		ucontrol->value.integer.value[0] = cm710x_codec->sMaxNRBFMode;
		break;
	default:
		break;
	}
	return 0;
}

static int cm710x_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	u16 i2s_len = 0;
	u16 tdm_len = 0;
	u32 resetvalue = 0x80000000;

	int rc = wait_for_completion_timeout(
			&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	switch (params_width(params)) {
	case 16:
		pr_debug("%s 16bits\n", __func__);
		break;
	case 24:
		pr_debug("%s 24bits\n", __func__);
		i2s_len |= 0x08;
		tdm_len |= 0x300;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(cm710x_codec->virt_regmap, CM710X_I2S1_SDP,
			CM710X_I2S_DL_MASK, i2s_len);
	regmap_update_bits(cm710x_codec->virt_regmap, CM710X_TDM1_CTRL1,
			0x0300, tdm_len);
	regmap_update_bits(cm710x_codec->virt_regmap, CM710X_I2S2_SDP,
			CM710X_I2S_DL_MASK, 0x08);
	regmap_update_bits(cm710x_codec->virt_regmap, CM710X_TDM2_CTRL1,
			0x0300, 0x300);

	/* clean inbound */
	mutex_lock(&cm710x_codec->Dsp_Access_Lock);
	cm710x_dsp_mode_i2c_write_mem(cm710x_codec->real_regmap, 0x5FFC0000,
			(u8 *)&resetvalue, 4);
	mutex_unlock(&cm710x_codec->Dsp_Access_Lock);

	return 0;
}

static int cm710x_stream_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		pr_debug("%s SNDRV_PCM_TRIGGER_START\n", __func__);
		if (cm710x_codec->bDelayForPopNoise &&
			substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			schedule_delayed_work(&cm710x_codec->avoid_pop_noise,
					HZ / 20);
			cm710x_codec->bDelayForPopNoise = false;
		}
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		pr_debug("%s SNDRV_PCM_TRIGGER_RESUME\n", __func__);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("%s SNDRV_PCM_TRIGGER_PAUSE_RELEASE\n", __func__);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("%s SNDRV_PCM_TRIGGER_STOP\n", __func__);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		pr_debug("%s SNDRV_PCM_TRIGGER_SUSPEND\n", __func__);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("%s SNDRV_PCM_TRIGGER_PAUSE_PUSH\n", __func__);
		break;
	}
	return 0;
}

static int cm710x_put_device_switch(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	const struct soc_mixer_control *mc =
		(const struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec = snd_soc_component_get_drvdata(component);

	int rc;
	int left = ucontrol->value.integer.value[0];
	int right = ucontrol->value.integer.value[1];
	bool enabled = left || right;
	int reg = mc->reg;

	rc = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	if (reg == CM710X_STO1_ADC_DIG_VOL)
		cm710x_codec->bHeadphoneEnable = enabled;

	if (cm710x_codec->bEnableMicDspProcess) {
		/* Skip AEC for headphone devices */
		if (cm710x_codec->bEnableAEC && !cm710x_codec->bHeadphoneEnable)
			cm710x_codec->bSkipAEC = false;
		else
			cm710x_codec->bSkipAEC = true;

		cm710x_update_DSP_Effect_Param(cm710x_codec,
			MIC_DSP_SKIP_AEC - MIC_DSP_MAXNR_AEC,
			cm710x_codec->bSkipAEC);
	}

	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static const DECLARE_TLV_DB_SCALE(eq_pre_tlv, -1200, 100, 0);
static const DECLARE_TLV_DB_SCALE(eq_vol_tlv, -2000, 100, 0);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -6525, 75, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -1725, 75, 0);

static const struct snd_kcontrol_new cm710x_snd_controls[] = {
	SOC_DOUBLE_TLV("Headphone Volume", CM710X_DAC3_DIG_VOL,
		CM710X_L_VOL_SFT, CM710X_R_VOL_SFT, 87, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("Speaker Volume", CM710X_DAC1_DIG_VOL,
		CM710X_L_VOL_SFT, CM710X_R_VOL_SFT, 87, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("MIC Volume", CM710X_STO2_ADC_DIG_VOL,
		8, 0, 127, 0, adc_vol_tlv),
	SOC_DOUBLE_EXT("Headphone Switch", CM710X_STO1_ADC_DIG_VOL,
		CM710X_L_MUTE_SFT, CM710X_R_MUTE_SFT, 1, 1,
		snd_soc_get_volsw, cm710x_put_device_switch),
	SOC_DOUBLE_EXT("Speaker Switch", CM710X_STO1_DAC_MIXER,
		CM710X_M_DAC1_L_STO_L_SFT, CM710X_M_DAC1_R_STO_R_SFT, 1, 1,
		snd_soc_get_volsw, cm710x_put_device_switch),
	SOC_DOUBLE_EXT("Mic Switch", CM710X_STO2_ADC_DIG_VOL,
		CM710X_L_MUTE_SFT, CM710X_R_MUTE_SFT, 1, 1,
		snd_soc_get_volsw, cm710x_put_device_switch),

	SOC_SINGLE_EXT("Mic DSP Bypass",
		MIC_DSP_BYPASS_ON_OFF, 0, 1, 0,
		cm710x_get_vu, cm710x_put_vu),
	SOC_SINGLE_EXT("Mic DSP Enable",
		MIC_DSP_PROCESS_ON_OFF, 0, 1, 0,
		cm710x_get_vu, cm710x_put_vu),
	SOC_SINGLE_EXT("Mic DSP AEC", MIC_DSP_AEC_ON_OFF, 0, 1, 0,
		cm710x_get_vu, cm710x_put_vu),

	SOC_SINGLE_EXT("MIC DSP MaxNR for AEC", MIC_DSP_MAXNR_AEC, 0, 32767, 0,
		cm710x_get_dsp_param, cm710x_put_dsp_param),
	SOC_SINGLE_EXT("MIC DSP NoiseGate", MIC_DSP_NOISEGATE, 0, 8388607, 0,
		cm710x_get_dsp_param, cm710x_put_dsp_param),
	SOC_SINGLE_EXT("MIC DSP TBrrLow", MIC_DSP_TBRRLOW, 0, 12, 0,
		cm710x_get_dsp_param, cm710x_put_dsp_param),
	SOC_SINGLE_EXT("MIC DSP Skip AEC", MIC_DSP_SKIP_AEC, 0, 1, 0,
		cm710x_get_dsp_param, cm710x_put_dsp_param),
	SOC_SINGLE_EXT("MIC DSP MaxNR for BeamForming",
				MIC_DSP_MAXNR_BF, 0, 32767, 0,
		cm710x_get_dsp_param, cm710x_put_dsp_param),
	SOC_SINGLE_EXT("MIC DSP BeamForming Beam width mode",
				MIC_DSP_MAXNR_BF_MODE, 0, 3, 0,
		cm710x_get_dsp_param, cm710x_put_dsp_param),

	SOC_SINGLE_EXT("SPK EQ Switch", SPK_EQ_ON_OFF, 0, 1, 0,
		cm710x_get_vu, cm710x_put_vu),
	SOC_SINGLE_EXT_TLV("SPK Left EQ Pre Gain", SPK_L_EQ_PRE_GAIN, 0, 12, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_pre_tlv),
	SOC_SINGLE_EXT("SPK Left Channel LowPass FC",
		SPK_L_EQ_LOWPASS_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Left BandPass01 FC",
		SPK_L_EQ_BAND_01_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Left BandPass02 FC",
		SPK_L_EQ_BAND_02_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Left BandPass03 FC",
		SPK_L_EQ_BAND_03_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Left BandPass04 FC",
		SPK_L_EQ_BAND_04_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Left BandPass05 FC",
		SPK_L_EQ_BAND_05_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Left BandPass06 FC",
		SPK_L_EQ_BAND_06_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Left Channel HighPass FC",
		SPK_L_EQ_HIGHPASS_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT_TLV("SPK Left Channel LowPass Gain",
		SPK_L_EQ_LOWPASS_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Left BandPass01 Gain",
		SPK_L_EQ_BAND_01_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Left BandPass02 Gain",
		SPK_L_EQ_BAND_02_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Left BandPass03 Gain",
		SPK_L_EQ_BAND_03_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Left BandPass04 Gain",
		SPK_L_EQ_BAND_04_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Left BandPass05 Gain",
		SPK_L_EQ_BAND_05_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Left BandPass06 Gain",
		SPK_L_EQ_BAND_06_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Left Channel HigPass Gain",
		SPK_L_EQ_HIGHPASS_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT("SPK Left BandPass01 Q",
		SPK_L_EQ_BAND_01_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Left BandPass02 Q", SPK_L_EQ_BAND_02_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Left BandPass03 Q", SPK_L_EQ_BAND_03_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Left BandPass04 Q", SPK_L_EQ_BAND_04_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Left BandPass05 Q", SPK_L_EQ_BAND_05_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Left BandPass06 Q", SPK_L_EQ_BAND_06_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT_TLV("SPK Right EQ Pre Gain", SPK_R_EQ_PRE_GAIN, 0, 12, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_pre_tlv),
	SOC_SINGLE_EXT("SPK Right Channel LowPass FC",
		SPK_R_EQ_LOWPASS_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Right BandPass01 FC",
		SPK_R_EQ_BAND_01_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Right BandPass02 FC",
		SPK_R_EQ_BAND_02_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Right BandPass03 FC",
		SPK_R_EQ_BAND_03_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Right BandPass04 FC",
		SPK_R_EQ_BAND_04_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Right BandPass05 FC",
		SPK_R_EQ_BAND_05_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Right BandPass06 FC",
		SPK_R_EQ_BAND_06_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("SPK Right Channel HighPass FC",
		SPK_R_EQ_HIGHPASS_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT_TLV("SPK Right Channel LowPass Gain",
		SPK_R_EQ_LOWPASS_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Right BandPass01 Gain",
		SPK_R_EQ_BAND_01_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Right BandPass02 Gain",
		SPK_R_EQ_BAND_02_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Right BandPass03 Gain",
		SPK_R_EQ_BAND_03_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Right BandPass04 Gain",
		SPK_R_EQ_BAND_04_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Right BandPass05 Gain",
		SPK_R_EQ_BAND_05_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Right BandPass06 Gain",
		SPK_R_EQ_BAND_06_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("SPK Right Channel HigPass Gain",
		SPK_R_EQ_HIGHPASS_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT("SPK Right BandPass01 Q", SPK_R_EQ_BAND_01_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Right BandPass02 Q", SPK_R_EQ_BAND_02_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Right BandPass03 Q", SPK_R_EQ_BAND_03_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Right BandPass04 Q", SPK_R_EQ_BAND_04_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Right BandPass05 Q", SPK_R_EQ_BAND_05_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("SPK Right BandPass06 Q", SPK_R_EQ_BAND_06_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),

	SOC_SINGLE_EXT("MIC EQ Switch", MIC_EQ_ON_OFF, 0, 1, 0,
		cm710x_get_vu, cm710x_put_vu),
	SOC_SINGLE_EXT("MIC EQ AGC Switch", MIC_EQ_AGC_ON_OFF, 0, 1, 0,
		cm710x_get_vu, cm710x_put_vu),
	SOC_SINGLE_EXT_TLV("MIC EQ Pre Gain", MIC_EQ_PRE_GAIN, 0, 12, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_pre_tlv),
	SOC_SINGLE_EXT("MIC LowPass FC", MIC_EQ_LOWPASS_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("MIC BandPass01 FC", MIC_EQ_BAND_01_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("MIC BandPass02 FC", MIC_EQ_BAND_02_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("MIC BandPass03 FC", MIC_EQ_BAND_03_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("MIC BandPass04 FC", MIC_EQ_BAND_04_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("MIC BandPass05 FC", MIC_EQ_BAND_05_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("MIC BandPass06 FC", MIC_EQ_BAND_06_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT("MIC HighPass FC", MIC_EQ_HIGHPASS_FC, 0, 32767, 0,
		cm710x_get_eq_fc, cm710x_put_eq_fc),
	SOC_SINGLE_EXT_TLV("MIC LowPass Gain", MIC_EQ_LOWPASS_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("MIC BandPass01 Gain", MIC_EQ_BAND_01_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("MIC BandPass02 Gain", MIC_EQ_BAND_02_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("MIC BandPass03 Gain", MIC_EQ_BAND_03_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("MIC BandPass04 Gain", MIC_EQ_BAND_04_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("MIC BandPass05 Gain", MIC_EQ_BAND_05_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("MIC BandPass06 Gain", MIC_EQ_BAND_06_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT_TLV("MIC HigPass Gain", MIC_EQ_HIGHPASS_GAIN, 0, 40, 0,
		cm710x_get_eq_gain, cm710x_put_eq_gain, eq_vol_tlv),
	SOC_SINGLE_EXT("MIC BandPass01 Q", MIC_EQ_BAND_01_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("MIC BandPass02 Q", MIC_EQ_BAND_02_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("MIC BandPass03 Q", MIC_EQ_BAND_03_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("MIC BandPass04 Q", MIC_EQ_BAND_04_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("MIC BandPass05 Q", MIC_EQ_BAND_05_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
	SOC_SINGLE_EXT("MIC BandPass06 Q", MIC_EQ_BAND_06_Q, 0, 510, 0,
		cm710x_get_eq_q, cm710x_put_eq_q),
};

static int cm710x_playback_in_enum_ext_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
			snd_soc_component_get_drvdata(component);

	int rc = wait_for_completion_timeout(
			&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	ucontrol->value.integer.value[0] = PlaybackFrom;
	dev_info(cm710x_codec->dev, "%s select PlaybackFrom = %d\n",
		__func__, PlaybackFrom);

	return 0;
}

static int cm710x_playback_in_enum_ext_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_dapm_kcontrol_component(kcontrol);
	struct cm710x_codec_priv *cm710x_codec =
			snd_soc_component_get_drvdata(component);
	int ret = 0;

	ret = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (ret == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	PlaybackFrom = ucontrol->value.integer.value[0];
	switch (PlaybackFrom) {
	/* AIF1 */
	case 0:
		dev_dbg(cm710x_codec->dev, "%s select 0\n", __func__);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ADC_IF_DSP_DAC1_MIXER,
				0xc0c3, 0x8080);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_DD1_MIXER,
				0xFFFF, 0xA2A2);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ASRC_3,
				0xFFFF, 0x1011);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ASRC_4,
				0xFFFF, 0x1111);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ASRC_5,
				0xFFFF, 0x2100);

		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ANA_DAC1_2_3_SRC,
				0xFFFF, 0x0000);
		break;
	/* AMIC */
	case 1:
		dev_dbg(cm710x_codec->dev, "%s select 1\n", __func__);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ADC_IF_DSP_DAC1_MIXER,
				0xc0c3, 0x4041);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_DD1_MIXER,
				0xFFFF, 0x2222);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ASRC_3,
				0xFFFF, 0x1011);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ASRC_4,
				0xFFFF, 0x1111);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ASRC_5,
				0xFFFF, 0x2100);
		regmap_update_bits(cm710x_codec->virt_regmap,
				CM710X_ANA_DAC1_2_3_SRC,
				0xFFFF, 0x0002);

		break;
	default:
		dev_err(cm710x_codec->dev, "%s EINVAL\n", __func__);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int HP_dapm_power_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	int rc;

	rc = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	if (!IS_ERR_OR_NULL(cm710x_codec->pin_spk_en)) {
		switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			break;
		case SND_SOC_DAPM_POST_PMD:
			break;
		default:
			dev_dbg(component->dev,
				"Unhandled dapm widget event %d from %s\n",
				event, w->name);
		}
	}
	return 0;
}

static int SPK_dapm_power_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	int rc;

	rc = wait_for_completion_timeout(&cm710x_codec->fw_download_complete,
			FW_DOWNLOAD_TIMEOUT);
	if (rc == 0) {
		pr_err("%s: Firmware download timed out!\n", __func__);
		return -ETIMEDOUT;
	}

	if (!IS_ERR_OR_NULL(cm710x_codec->pin_spk_en)) {
		switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			if (!cm710x_codec->bDelayForPopNoise) {
				dev_dbg(component->dev, "SPK gpio to high\n");
				rc = pinctrl_select_state(cm710x_codec->pinctrl,
						cm710x_codec->pin_spk_en);
				if (rc)
					dev_err(cm710x_codec->dev,
						"pinctrl_select_state error");
			}
			break;
		case SND_SOC_DAPM_POST_PMD:
			dev_dbg(component->dev, "SPK gpio to low\n");
			rc = pinctrl_select_state(cm710x_codec->pinctrl,
					cm710x_codec->pin_spk_suspend);
			if (rc)
				dev_err(cm710x_codec->dev,
					"pinctrl_select_state error");
			break;
		default:
			dev_dbg(component->dev,
				"Unhandled dapm widget event %d from %s\n",
				event, w->name);
		}
	}
	return 0;
}

/*Output Mixer*/
static const struct snd_kcontrol_new output_mix[] = {
	SOC_DAPM_SINGLE("HP Switch", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE("SPK Switch", SND_SOC_NOPM, 0, 1, 0),
};

/* Playback Input Source */
static const char * const cm710x_playback_in_src[] = {
	"AIF1", "AMIC"
};

static SOC_ENUM_SINGLE_EXT_DECL(
	cm710x_playback_in_enum, cm710x_playback_in_src);

static const struct snd_kcontrol_new cm710x_playback_in_mux =
	SOC_DAPM_ENUM_EXT("Playback From", cm710x_playback_in_enum,
			cm710x_playback_in_enum_ext_get,
			cm710x_playback_in_enum_ext_put);

static const struct snd_soc_dapm_widget cm710x_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_OUTPUT("OUTR"),
	SND_SOC_DAPM_INPUT("INL"),
	SND_SOC_DAPM_INPUT("INR"),

	SND_SOC_DAPM_ADC("ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("DAC1PGA", CM710X_LOUT1, 15, 1, NULL, 0),
	SND_SOC_DAPM_PGA("DAC2PGA", CM710X_LOUT1, 13, 1, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("HP Driver", SND_SOC_NOPM, 0, 0,
		NULL, 0, HP_dapm_power_event,
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_OUT_DRV_E("SPK Driver", SND_SOC_NOPM, 0, 0,
		NULL, 0, SPK_dapm_power_event,
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MIXER("OUTPUT MIX", SND_SOC_NOPM, 0, 0,
		output_mix, ARRAY_SIZE(output_mix)),
	SND_SOC_DAPM_MUX("Playback From Mux", SND_SOC_NOPM, 0, 0,
		&cm710x_playback_in_mux),

	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_MICBIAS("I2S2PWR", CM710X_PWR_DIG1,
		CM710X_PWR_I2S2_BIT, 0),

};

static const struct snd_soc_dai_ops cm710x_dai_ops = {
	.hw_params = cm710x_hw_params,
	.trigger = cm710x_stream_trigger,
};

static const struct snd_soc_dapm_route cm710x_dapm_routes[] = {
	{ "ADC", NULL, "INL"},
	{ "ADC", NULL, "INR"},
	{ "Playback From Mux", "AIF1", "AIF1RX"},
	{ "Playback From Mux", "AMIC", "ADC"},
	{ "DAC1PGA", NULL, "Playback From Mux" },
	{ "DAC2PGA", NULL, "Playback From Mux" },
	{ "I2S2PWR", NULL, "Playback From Mux" },
	{ "DAC1", NULL, "DAC1PGA" },
	{ "DAC2", NULL, "DAC2PGA" },
	{ "SPK Driver", NULL, "DAC1" },
	{ "SPK Driver", NULL, "DAC2" },
	{ "HP Driver", NULL, "I2S2PWR" },
	{ "OUTPUT MIX", "HP Switch", "HP Driver" },
	{ "OUTPUT MIX", "SPK Switch", "SPK Driver" },
	{ "OUTL", NULL, "OUTPUT MIX" },
	{ "OUTR", NULL, "OUTPUT MIX" },
	{ "AIF1TX", NULL, "ADC" },
};

static int cm710x_codec_suspend(struct snd_soc_component *component)
{
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	int ret;

	regmap_write(cm710x_codec->virt_regmap, CM710X_RESET, 0x10ec);
	cm710x_codec->bDspMode = false;

	if (!IS_ERR_OR_NULL(cm710x_codec->pin_suspend)) {
		ret = pinctrl_select_state(cm710x_codec->pinctrl,
				cm710x_codec->pin_suspend);
		if (ret)
			dev_err(cm710x_codec->dev,
				"pinctrl_select_state error");
	}

	clk_disable_unprepare(cm710x_codec->mclk);

	ret = regulator_disable(cm710x_codec->amic_core);
	if (ret < 0)
		dev_err(cm710x_codec->dev, "AMIC regulator_disable failed\n");

	ret = regulator_disable(cm710x_codec->avdd_core);
	if (ret < 0)
		dev_err(cm710x_codec->dev, "AVDD regulator_disable failed\n");

	ret = regulator_disable(cm710x_codec->dvdd_core);
	if (ret < 0)
		dev_err(cm710x_codec->dev, "DVDD regulator_disable failed\n");

	/*
	 * Reinitialize completion since powering off regulators requires
	 * a firmware reload before any audio playback function will work.
	 */
	reinit_completion(&cm710x_codec->fw_download_complete);

	return 0;
}

static int cm710x_codec_resume(struct snd_soc_component *component)
{
	struct cm710x_codec_priv *cm710x_codec =
		snd_soc_component_get_drvdata(component);
	int ret;

	ret = regulator_enable(cm710x_codec->dvdd_core);
	if (ret < 0)
		dev_err(cm710x_codec->dev, "DVDD regulator_enable failed\n");

	ret = regulator_enable(cm710x_codec->avdd_core);
	if (ret < 0)
		dev_err(cm710x_codec->dev, "AVDD regulator_enable failed\n");

	ret = regulator_enable(cm710x_codec->amic_core);
	if (ret < 0)
		dev_err(cm710x_codec->dev, "AMIC regulator_enable failed\n");

	ret = clk_prepare_enable(cm710x_codec->mclk);
	if (ret < 0)
		dev_err(cm710x_codec->dev, "enable CM710x MCLK error\n");

	if (!IS_ERR_OR_NULL(cm710x_codec->pin_default)) {
		ret = pinctrl_select_state(cm710x_codec->pinctrl,
				cm710x_codec->pin_default);
		if (ret)
			dev_err(cm710x_codec->dev,
				"pinctrl_select_state error");
	}

	return ret;
}

static int cm710x_read(void *context, unsigned int reg, unsigned int *val)
{
	struct i2c_client *client = context;
	struct cm710x_codec_priv *cm710x_codec = i2c_get_clientdata(client);

	if (cm710x_codec->bDspMode)
		cm710x_dsp_mode_i2c_read(cm710x_codec, reg, val);
	else
		regmap_read(cm710x_codec->real_regmap, reg, val);

	return 0;
}

static int cm710x_write(void *context, unsigned int reg, unsigned int val)
{
	struct i2c_client *client = context;
	struct cm710x_codec_priv *cm710x_codec = i2c_get_clientdata(client);

	if (cm710x_codec->bDspMode)
		cm710x_dsp_mode_i2c_write(cm710x_codec, reg, val);
	else
		regmap_write(cm710x_codec->real_regmap, reg, val);

	return 0;
}

static u32 param_parsing(char *buf, u32 Count, u32 *dwAddr, u32 *dwData)
{
	char *p = NULL, *ptr = NULL;
	char *Param[2] = {0};
	int err = 0;
	u32 dwIdx = 0, dwParamCount = 0;
	*dwAddr = 0;
	*dwData = 0;

	p = buf;
	do {
		for (; dwIdx < Count; dwIdx++) {
			ptr = strnchr(hextext, strlen(hextext), *p);
			if (ptr)
				break;
			p++;
		}

		if (dwIdx == Count)
			break;
		if (dwParamCount < 2)
			Param[dwParamCount] = p;

		dwParamCount++;
		for (; dwIdx < Count; dwIdx++) {
			ptr = strnchr(hextext, strlen(hextext), *p);
			if (ptr == NULL) {
				*p = 0;
				p++;
				dwIdx++;
				break;
			}
			*p = tolower(*p);
			p++;
		}
	} while (dwIdx < Count);

	if (Param[0]) {
		if (Param[0][0] == '0' && Param[0][1] == 'x')
			err = kstrtou32(Param[0], 16, dwAddr);
		else
			err = kstrtou32(Param[0], 10, dwAddr);

		if (err)
			return err;

		pr_debug("%s Param[0] = %s dwAddr = %d\n", __func__,
			Param[0], *dwAddr);
	}

	if (Param[1]) {
		if (Param[1][0] == '0' && Param[1][1] == 'x')
			err = kstrtou32(Param[1], 16, dwData);
		else
			err = kstrtou32(Param[1], 10, dwData);

		if (err)
			return err;

		pr_debug("%s Param[1] = %s dwData = %d\n", __func__,
			Param[1], *dwData);
	}

	return dwParamCount;
}

static ssize_t cm710x_Codec_Reg_Show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct cm710x_codec_priv *cm710x_codec = dev_get_drvdata(dev);
	u32 dwValue = 0;

	if (cm710x_codec->bDspMode)
		cm710x_dsp_mode_i2c_read(cm710x_codec, CodecAddr, &dwValue);
	else
		regmap_read(cm710x_codec->real_regmap, CodecAddr, &dwValue);

	dwValue = dwValue & 0xFFFF;
	return scnprintf(buf, PAGE_SIZE, "0x%04x\n", dwValue);
}

static ssize_t cm710x_Codec_Reg_Set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct cm710x_codec_priv *cm710x_codec = dev_get_drvdata(dev);
	u32 dwAddr = 0, dwData = 0;
	u32 dwParamCount;

	dwParamCount = param_parsing((char *)buf, (u32)count, &dwAddr, &dwData);
	dwAddr = dwAddr & 0xFF;
	dwData = dwData & 0xFFFF;

	if (dwParamCount == 1)
		CodecAddr = dwAddr;
	else if (dwParamCount == 2) {
		CodecAddr = dwAddr;
		if (cm710x_codec->bDspMode)
			cm710x_dsp_mode_i2c_write(cm710x_codec,
				CodecAddr, dwData);
		else
			regmap_write(cm710x_codec->real_regmap,
				CodecAddr, dwData);
	}

	pr_debug("%s %s BufLen=%zu dwParamCount = %d\n", __func__, buf,
		count, dwParamCount);

	return count;
}

static ssize_t cm710x_Dsp_Mem_Show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct cm710x_codec_priv *cm710x_codec = dev_get_drvdata(dev);
	u32 dwValue = 0;

	if (cm710x_codec->bDspMode) {
		mutex_lock(&cm710x_codec->Dsp_Access_Lock);
		cm710x_dsp_mode_i2c_read_mem(cm710x_codec->real_regmap,
			DspAddr, &dwValue);
		mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
	}

	return scnprintf(buf, PAGE_SIZE, "0x%08x\n", dwValue);
}

static ssize_t cm710x_Dsp_Mem_Set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct cm710x_codec_priv *cm710x_codec = dev_get_drvdata(dev);
	u32 dwAddr = 0, dwData = 0;
	u32 dwParamCount;

	dwParamCount = param_parsing((char *)buf, (u32)count, &dwAddr, &dwData);

	if (dwParamCount == 1) {
		DspAddr = dwAddr;
	} else if (dwParamCount == 2) {
		DspAddr = dwAddr;
		if (cm710x_codec->bDspMode) {
			mutex_lock(&cm710x_codec->Dsp_Access_Lock);
			cm710x_dsp_mode_i2c_write_mem(cm710x_codec->real_regmap,
				DspAddr, (u8 *)&dwData, 4);
			mutex_unlock(&cm710x_codec->Dsp_Access_Lock);
		}
	}

	pr_debug("%s %s BufLen=%zu dwParamCount = %d\n", __func__, buf,
		count, dwParamCount);

	return count;
}

static DEVICE_ATTR(cm710xcodec, 0644, cm710x_Codec_Reg_Show,
	cm710x_Codec_Reg_Set);
static DEVICE_ATTR(cm710xdsp, 0644, cm710x_Dsp_Mem_Show,
	cm710x_Dsp_Mem_Set);

static struct snd_soc_dai_driver cm710x_dai[] = {
	{
		.name = "cm710x-aif1",
		.id = 0,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S24_3LE,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S24_3LE,
		},
		.ops = &cm710x_dai_ops,
		.symmetric_rates = 1,
	}
};


static const struct snd_soc_component_driver soc_codec_dev_cm710x = {
	.name = "cm710x",
	.probe = cm710x_codec_probe,
	.remove = cm710x_codec_remove,
	.suspend = cm710x_codec_suspend,
	.resume = cm710x_codec_resume,
	.controls		= cm710x_snd_controls,
	.num_controls		= ARRAY_SIZE(cm710x_snd_controls),
	.dapm_widgets		= cm710x_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(cm710x_dapm_widgets),
	.dapm_routes		= cm710x_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(cm710x_dapm_routes),
};

static const struct regmap_config cm710x_real_regmap_config = {
	.name = "physical_regmap",
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = 0x100,
	.readable_reg = cm710x_readable_register,

	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config cm710x_virt_regmap_config = {
	.name = "virtual_regmap",
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = 0x100,

	.volatile_reg = cm710x_volatile_register,
	.readable_reg = cm710x_readable_register,
	.reg_read = cm710x_read,
	.reg_write = cm710x_write,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = cm710x_reg,
	.num_reg_defaults = ARRAY_SIZE(cm710x_reg),
};

int cm710x_probe(struct device *dev, struct regmap *virt_regmap,
		struct regmap *real_regmap)
{
	struct cm710x_codec_priv *cm710x_codec;
	int ret;

	dev_dbg(dev, "%s entry\n", __func__);

	cm710x_codec = devm_kzalloc(dev, sizeof(struct cm710x_codec_priv),
			GFP_KERNEL);
	if (cm710x_codec == NULL)
		return -ENOMEM;

	cm710x_codec->dev = dev;
	cm710x_codec->virt_regmap = virt_regmap;
	cm710x_codec->real_regmap = real_regmap;

	mutex_init(&cm710x_codec->Dsp_Access_Lock);

	dev_set_drvdata(dev, cm710x_codec);

	/* regulator DVDD */
	cm710x_codec->dvdd_core = regulator_get(dev, "cm710x,dvdd-core");
	if (IS_ERR(cm710x_codec->dvdd_core))
		dev_err(dev, "DVDD regulator_get error\n");

	ret = regulator_set_voltage(cm710x_codec->dvdd_core, 1800000, 1800000);
	if (ret < 0)
		dev_err(dev, "DVDD set_voltage failed\n");

	ret = regulator_set_load(cm710x_codec->dvdd_core, 300000);
	if (ret < 0)
		dev_err(dev, "DVDD set_load failed\n");

	ret = regulator_enable(cm710x_codec->dvdd_core);
	if (ret < 0)
		dev_err(dev, "DVDD regulator_enable failed\n");

	/* regulator AVDD */
	cm710x_codec->avdd_core = regulator_get(dev, "cm710x,avdd-core");
	if (IS_ERR(cm710x_codec->avdd_core))
		dev_err(dev, "AVDD regulator_get error\n");

	ret = regulator_set_voltage(cm710x_codec->avdd_core, 1800000, 1800000);
	if (ret < 0)
		dev_err(dev, "AVDD set_voltage failed\n");

	ret = regulator_set_load(cm710x_codec->avdd_core, 300000);
	if (ret < 0)
		dev_err(dev, "AVDD set_load failed\n");

	ret = regulator_enable(cm710x_codec->avdd_core);
	if (ret < 0)
		dev_err(dev, "AVDD regulator_enable failed\n");

	/* regulator AMIC */
	cm710x_codec->amic_core = regulator_get(dev, "cm710x,amic-core");
	if (IS_ERR(cm710x_codec->amic_core))
		dev_err(dev, "AMIC regulator_get error\n");

	ret = regulator_set_voltage(cm710x_codec->amic_core, 1800000, 1800000);
	if (ret < 0)
		dev_err(dev, "AMIC set_voltage failed\n");

	ret = regulator_set_load(cm710x_codec->amic_core, 300000);
	if (ret < 0)
		dev_err(dev, "AMIC set_load failed\n");

	ret = regulator_enable(cm710x_codec->amic_core);
	if (ret < 0)
		dev_err(dev, "AMIC regulator_enable failed\n");

	ret = get_pin_control(cm710x_codec);
	if (ret != 0) {
		dev_err(dev, "Failed to get pin control\n");
		return ret;
	}

	cm710x_codec->fw_parsing = of_get_property(dev->of_node,
			"cm710x,fw-parsing", &ret);
	if (cm710x_codec->fw_parsing != NULL) {
		dev_dbg(cm710x_codec->dev, "%s (%d) : fw_parsing is %s\n",
				__func__, __LINE__, cm710x_codec->fw_parsing);
	} else {
		dev_err(cm710x_codec->dev, "%s (%d) : cannot get dts model\n",
				__func__, __LINE__);
		cm710x_codec->fw_parsing = DEFAULT_FW_PARSING;
	}

	INIT_WORK(&cm710x_codec->fw_download_work,
			cm710x_firmware_download_work);

	init_completion(&cm710x_codec->fw_download_complete);

	usleep_range(10000, 15000);

	ret = snd_soc_register_component(dev, &soc_codec_dev_cm710x,
			cm710x_dai, ARRAY_SIZE(cm710x_dai));

	return ret;
}
EXPORT_SYMBOL_GPL(cm710x_probe);

static int cm710x_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct regmap_config virt_config, real_config;
	struct regmap *virt_map = NULL, *real_map = NULL;
	int ret;

	virt_config = cm710x_virt_regmap_config;
	real_config = cm710x_real_regmap_config;

	real_map = devm_regmap_init_i2c(client, &real_config);
	if (IS_ERR(real_map)) {
		ret = PTR_ERR(virt_map);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
				ret);
		return ret;
	}

	virt_map = devm_regmap_init(&client->dev, NULL, client, &virt_config);
	if (IS_ERR(virt_map)) {
		ret = PTR_ERR(virt_map);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
				ret);
		return ret;
	}

	ret = device_create_file(&client->dev, &dev_attr_cm710xcodec);
	if (ret)
		dev_info(&client->dev, "error creating sysfs files\n");

	ret = device_create_file(&client->dev, &dev_attr_cm710xdsp);
	if (ret)
		dev_info(&client->dev, "error creating sysfs files\n");

	return cm710x_probe(&client->dev, virt_map, real_map);
}

static int cm710x_i2c_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_cm710xdsp);
	device_remove_file(&client->dev, &dev_attr_cm710xcodec);
	snd_soc_unregister_component(&client->dev);

	return 0;
}

const struct of_device_id cm710x_of_match[] = {
	{ .compatible = "C-Media,cm710x", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, cm710x_of_match);

static const struct i2c_device_id cm710x_i2c_ids[] = {
	{ "cm7100", 1 },
	{ "cm7104", 2 },
	{ "cm7106", 3 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cm710x_i2c_ids);

static struct i2c_driver cm710x_i2c_driver = {
	.driver = {
		.name = "cm710x",
		.of_match_table = cm710x_of_match,
	},
	.probe = cm710x_i2c_probe,
	.remove = cm710x_i2c_remove,
	.id_table = cm710x_i2c_ids,
};
module_i2c_driver(cm710x_i2c_driver);

MODULE_DESCRIPTION("ASoC CM710X codec driver");
MODULE_AUTHOR("Tzung-Dar Tsai <tdtsai@cmedia.com.tw>");
MODULE_LICENSE("GPL v2");
