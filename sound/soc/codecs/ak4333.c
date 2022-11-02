/*
 * ak4333.c  --  audio driver for AK4333
 *
 * Copyright (C) 2022 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision      DS ver.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                     22/03/11	    1.0	       00
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include "ak4333.h"

#ifdef AK4333_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

/* AK4333 Component Private Data */
struct ak4333_priv {
	struct i2c_client *i2c;
	struct regmap *regmap;
	int pdn_gpio;
	u32 rclk;
	u32 fs;
	int nDeviceID;
	int nDACOn;
	int PLLMode; // 0:PLL OFF, 1:BICK PLL (Slave), 2:MCKI PLL (Master)
	int mckFreq;
	int bickFreq;

	unsigned int hp_irq_gpio;  /* headset irq GPIO pin */
	int hp_irq;
	struct delayed_work hp_det_dwork;
	struct snd_soc_jack jack;
	struct snd_soc_card *card;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct regulator *avdd_core;
	u32 avdd_voltage;
	u32 avdd_current;
};

/* AK4333 register cache & default register settings */
static const struct reg_default ak4333_reg[] = {
	{0x00, 0x00}, /* 0x00 AK4333_00_POWER_MANAGEMENT1 */
	{0x01, 0x00}, /* 0x01 AK4333_01_POWER_MANAGEMENT2 */
	{0x02, 0x00}, /* 0x02 AK4333_02_POWER_MANAGEMENT3 */
	{0x03, 0x00}, /* 0x03 AK4333_03_POWER_MANAGEMENT4 */
	{0x04, 0x00}, /* 0x04 AK4333_04_OUTPUT_MODE_SETTING */
	{0x05, 0x00}, /* 0x05 AK4333_05_CLOCK_MODE_SELECT */
	{0x06, 0x00}, /* 0x06 AK4333_06_DIGITAL_FILTER_SELECT */
	{0x07, 0x00}, /* 0x07 AK4333_07_DAC_MONO_MIXING */
	{0x08, 0x00}, /* 0x08 AK4333_08_IMPEDANCE_DET_SETTING */
	{0x09, 0x00}, /* 0x09 AK4333_09_IMPEDANCE_DET_STATUS */
	{0x0A, 0x00}, /* 0x0A AK4333_0A_DAC_CLK_SOURCE_SELECT */
	{0x0B, 0x19}, /* 0x0B AK4333_0B_LCH_OUTPUT_VOLUME */
	{0x0C, 0x19}, /* 0x0C AK4333_0C_RCH_OUTPUT_VOLUME */
	{0x0D, 0x65}, /* 0x0D AK4333_0D_HP_VOLUME_CONTROL */
	{0x0E, 0x00}, /* 0x0E AK4333_0E_PLL_CLK_SOURCE_SELECT */
	{0x0F, 0x00}, /* 0x0F AK4333_0F_PLL_REF_CLK_DIVIDER1 */
	{0x10, 0x00}, /* 0x10 AK4333_10_PLL_REF_CLK_DIVIDER2 */
	{0x11, 0x00}, /* 0x11 AK4333_11_PLL_FB_CLK_DIVIDER1 */
	{0x12, 0x00}, /* 0x12 AK4333_12_PLL_FB_CLK_DIVIDER2 */
	{0x13, 0x00}, /* 0x13 AK4333_13_IMPEDANCE_DET_RESULT */
	{0x14, 0x00}, /* 0x14 AK4333_14_DAC_CLK_DIVIDER */
	{0x15, 0x80}, /* 0x15 AK4333_15_AUDIO_IF_FORMAT */
	{0x26, 0x00}, /* 0x26 AK4333_26_DAC_ADJUSTMENT1 */
	{0x27, 0x40}, /* 0x27 AK4333_27_DAC_ADJUSTMENT2 */
	{0x28, 0x00}, /* 0x28 AK4333_28_DAC_ADJUSTMENT3 */
};

static bool ak4333_readable(struct device *dev, unsigned int reg)
{
	bool ret;

	switch (reg) {
	case AK4333_00_POWER_MANAGEMENT1:
	case AK4333_01_POWER_MANAGEMENT2:
	case AK4333_02_POWER_MANAGEMENT3:
	case AK4333_03_POWER_MANAGEMENT4:
	case AK4333_04_OUTPUT_MODE_SETTING:
	case AK4333_05_CLOCK_MODE_SELECT:
	case AK4333_06_DIGITAL_FILTER_SELECT:
	case AK4333_07_DAC_MONO_MIXING:
	case AK4333_08_IMPEDANCE_DET_SETTING:
	case AK4333_09_IMPEDANCE_DET_STATUS:
	case AK4333_0A_DAC_CLK_SOURCE_SELECT:
	case AK4333_0B_LCH_OUTPUT_VOLUME:
	case AK4333_0C_RCH_OUTPUT_VOLUME:
	case AK4333_0D_HP_VOLUME_CONTROL:
	case AK4333_0E_PLL_CLK_SOURCE_SELECT:
	case AK4333_0F_PLL_REF_CLK_DIVIDER1:
	case AK4333_10_PLL_REF_CLK_DIVIDER2:
	case AK4333_11_PLL_FB_CLK_DIVIDER1:
	case AK4333_12_PLL_FB_CLK_DIVIDER2:
	case AK4333_13_IMPEDANCE_DET_RESULT:
	case AK4333_14_DAC_CLK_DIVIDER:
	case AK4333_15_AUDIO_IF_FORMAT:
	case AK4333_26_DAC_ADJUSTMENT1:
	case AK4333_27_DAC_ADJUSTMENT2:
	case AK4333_28_DAC_ADJUSTMENT3:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

bool ak4333_volatile(struct device *dev, unsigned int reg)
{
	bool ret;

	switch (reg) {
	case AK4333_08_IMPEDANCE_DET_SETTING:
	case AK4333_09_IMPEDANCE_DET_STATUS:
	case AK4333_13_IMPEDANCE_DET_RESULT:
	case AK4333_15_AUDIO_IF_FORMAT:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool ak4333_writeable(struct device *dev, unsigned int reg)
{
	bool ret;

	switch (reg) {
	case AK4333_00_POWER_MANAGEMENT1:
	case AK4333_01_POWER_MANAGEMENT2:
	case AK4333_02_POWER_MANAGEMENT3:
	case AK4333_03_POWER_MANAGEMENT4:
	case AK4333_04_OUTPUT_MODE_SETTING:
	case AK4333_05_CLOCK_MODE_SELECT:
	case AK4333_06_DIGITAL_FILTER_SELECT:
	case AK4333_07_DAC_MONO_MIXING:
	case AK4333_08_IMPEDANCE_DET_SETTING:
	case AK4333_09_IMPEDANCE_DET_STATUS:
	case AK4333_0A_DAC_CLK_SOURCE_SELECT:
	case AK4333_0B_LCH_OUTPUT_VOLUME:
	case AK4333_0C_RCH_OUTPUT_VOLUME:
	case AK4333_0D_HP_VOLUME_CONTROL:
	case AK4333_0E_PLL_CLK_SOURCE_SELECT:
	case AK4333_0F_PLL_REF_CLK_DIVIDER1:
	case AK4333_10_PLL_REF_CLK_DIVIDER2:
	case AK4333_11_PLL_FB_CLK_DIVIDER1:
	case AK4333_12_PLL_FB_CLK_DIVIDER2:
	case AK4333_14_DAC_CLK_DIVIDER:
	case AK4333_15_AUDIO_IF_FORMAT:
	case AK4333_26_DAC_ADJUSTMENT1:
	case AK4333_27_DAC_ADJUSTMENT2:
	case AK4333_28_DAC_ADJUSTMENT3:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

unsigned int ak4333_i2c_read(
	struct snd_soc_component *component,
	unsigned int reg
)
{
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	int ret = -1;
	unsigned char tx[1], rx[1];

	struct i2c_msg xfer[2];
	struct i2c_client *client = ak4333->i2c;

	tx[0] = reg;
	rx[0] = 0;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = tx;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 1;
	xfer[1].buf = rx;

	ret = i2c_transfer(client->adapter, xfer, 2);
	if (ret != 2)
		akdbgprt("\t[AK4333] %s error ret = %d\n", __func__, ret);

	return (unsigned int) rx[0];
}

/* Output Digital volume control:
 * from -12.5 to 3 dB in 0.5 dB steps (mute instead of -12.5 dB)
 */
static DECLARE_TLV_DB_SCALE(ovl_tlv, -1250, 50, 0);
static DECLARE_TLV_DB_SCALE(ovr_tlv, -1250, 50, 0);

/* HP-Amp Analog volume control:
 * from -10 to 4 dB in 2 dB steps
 */
static DECLARE_TLV_DB_SCALE(hpg_tlv, -1000, 200, 0);

static const char * const ak4333_ovolcn_select_texts[] = {
	"Dependent", "Independent",
};
static const char * const ak4333_mdacl_select_texts[] = {
	"x1", "x1/2",
};
static const char * const ak4333_mdacr_select_texts[] = {
	"x1", "x1/2",
};
static const char * const ak4333_invl_select_texts[] = {
	"Normal", "Inverting",
};
static const char * const ak4333_invr_select_texts[] = {
	"Normal", "Inverting",
};
static const char * const ak4333_hphl_select_texts[] = {
	"4ohm", "95kohm",
};
static const char * const ak4333_hphr_select_texts[] = {
	"4ohm", "95kohm",
};
static const char * const ak4333_dacfil_select_texts[] = {
	"Sharp Roll-Off",
	"Slow Roll-Off",
	"Short Delay Sharp Roll-Off",
	"Short Delay Slow Roll-Off",
};
static const char * const ak4333_bcko_select_texts[] = {
	"64fs", "32fs",
};
static const char * const ak4333_hptm_select_texts[] = {
	"128/fs", "256/fs", "512/fs", "1024/fs", "2048/fs",
};

static const struct soc_enum ak4333_dac_enum[] = {
	SOC_ENUM_SINGLE(AK4333_0B_LCH_OUTPUT_VOLUME, 7,
		ARRAY_SIZE(ak4333_ovolcn_select_texts),
		ak4333_ovolcn_select_texts),
	SOC_ENUM_SINGLE(AK4333_07_DAC_MONO_MIXING, 2,
		ARRAY_SIZE(ak4333_mdacl_select_texts),
		ak4333_mdacl_select_texts),
	SOC_ENUM_SINGLE(AK4333_07_DAC_MONO_MIXING, 6,
		ARRAY_SIZE(ak4333_mdacr_select_texts),
		ak4333_mdacr_select_texts),
	SOC_ENUM_SINGLE(AK4333_07_DAC_MONO_MIXING, 3,
		ARRAY_SIZE(ak4333_invl_select_texts),
		ak4333_invl_select_texts),
	SOC_ENUM_SINGLE(AK4333_07_DAC_MONO_MIXING, 7,
		ARRAY_SIZE(ak4333_invr_select_texts),
		ak4333_invr_select_texts),

	SOC_ENUM_SINGLE(AK4333_04_OUTPUT_MODE_SETTING, 0,
		ARRAY_SIZE(ak4333_hphl_select_texts),
		ak4333_hphl_select_texts),
	SOC_ENUM_SINGLE(AK4333_04_OUTPUT_MODE_SETTING, 1,
		ARRAY_SIZE(ak4333_hphr_select_texts),
		ak4333_hphr_select_texts),
	SOC_ENUM_SINGLE(AK4333_06_DIGITAL_FILTER_SELECT, 6,
		ARRAY_SIZE(ak4333_dacfil_select_texts),
		ak4333_dacfil_select_texts),
	SOC_ENUM_SINGLE(AK4333_15_AUDIO_IF_FORMAT, 3,
		ARRAY_SIZE(ak4333_bcko_select_texts),
		ak4333_bcko_select_texts),
	SOC_ENUM_SINGLE(AK4333_0D_HP_VOLUME_CONTROL, 5,
		ARRAY_SIZE(ak4333_hptm_select_texts),
		ak4333_hptm_select_texts),
};

static const char * const bickfreq_on_select[] = {
	"32fs", "48fs", "64fs",
};
static const char * const pllmcki_on_select[] = {
	"9.6MHz", "11.2896MHz", "12.288MHz", "19.2MHz", "12MHz", "24MHz"
};
static int mcktab[] = {
	9600000, 11289600, 12288000, 19200000, 12000000, 24000000
};
static const char * const pllmode_on_select[] = {
	"PLL_OFF", "PLL_BICK_MODE", "PLL_MCKI_MODE",
};
static const char * const detected_impedance_table[] = {
	"< 4", "< 4", "< 4", "< 4", "4", "5", "6", "7",
	"8", "9", "10", "11", "12", "13", "14", "15",
	"16", "17", "18", "19", "20", "21", "22", "23",
	"24", "25", "26", "27", "28", "29", "30", "31",
	"32", "33", "34", "35", "36", "37", "38", "39",
	"40", "41", "42", "43", "44", "45", "46", "47",
	"48", "49", "50", "51", "52", "53", "54", "55",
	"56", "57", "58", "59", "60", "61", "62", "63",
	"64", "65", "66", "67", "68", "69", "70", "71",
	"72", "73", "74", "75", "76", "77", "78", "79",
	"80", "81", "82", "83", "84", "85", "86", "87",
	"88", "89", "90", "91", "92", "93", "94", "95",
	"96", "97", "98", "99", "100", "100 <", "100 <", "100 <",
	"100 <", "100 <", "100 <", "100 <", "100 <", "100 <", "100 <", "100 <",
	"100 <", "100 <", "100 <", "100 <", "100 <", "100 <", "100 <", "100 <",
	"100 <", "100 <", "100 <", "100 <", "100 <", "100 <", "100 <", "100 <",
	"Error",
};

static const struct soc_enum ak4333_bitset_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(bickfreq_on_select), bickfreq_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllmcki_on_select), pllmcki_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllmode_on_select), pllmode_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(detected_impedance_table),
		detected_impedance_table),
};

static int get_bickfs(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4333->bickFreq;

	return 0;
}

static int set_bickfs(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	ak4333->bickFreq = ucontrol->value.enumerated.item[0];

	return 0;
}

static int get_pllmcki(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4333->mckFreq;

	return 0;
}

static int set_pllmcki(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	ak4333->mckFreq = ucontrol->value.enumerated.item[0];

	return 0;
}

static int get_pllmode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4333] %s(%d)\n", __func__, __LINE__);

	ucontrol->value.enumerated.item[0] = ak4333->PLLMode;

	return 0;
}

static int set_pllmode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	ak4333->PLLMode = ucontrol->value.enumerated.item[0];
	switch (ak4333->PLLMode) {
	case 0:
		/* PLL_OFF */
		/* CKCEL = 1 (PLLCLK) */
		snd_soc_component_update_bits(component,
			AK4333_0A_DAC_CLK_SOURCE_SELECT, 0x40, 0x40);
		/* PLS = 0 (MCKI pin) */
		snd_soc_component_update_bits(component,
			AK4333_0E_PLL_CLK_SOURCE_SELECT, 0x01, 0x00);
		break;
	case 1:
		/* PLL_BICK_MODE */
		/* CKCEL = 0 (MCKI) */
		snd_soc_component_update_bits(component,
			AK4333_0A_DAC_CLK_SOURCE_SELECT, 0x40, 0x00);
		/* PLS = 1 (BCLK pin) */
		snd_soc_component_update_bits(component,
			AK4333_0E_PLL_CLK_SOURCE_SELECT, 0x01, 0x01);
		break;
	case 2:
		/* PLL_MCKI_MODE */
		/* CKCEL = 0 (MCKI) */
		snd_soc_component_update_bits(component,
			AK4333_0A_DAC_CLK_SOURCE_SELECT, 0x40, 0x00);
		/* PLS = 0 (MCKI pin) */
		snd_soc_component_update_bits(component,
			AK4333_0E_PLL_CLK_SOURCE_SELECT, 0x01, 0x00);
		break;
	default:
		break;
	}

	akdbgprt("\t[AK4333] %s(%d) PLLMode=%d\n",
		__func__, __LINE__, ak4333->PLLMode);

	return 0;
}

static void ak4333_power_up(struct snd_soc_component *component,
	struct ak4333_priv *ak4333)
{
	int val = ((ak4333->PLLMode > 0) ? 0x01 : 0x00);

	/* PMPLL */
	snd_soc_component_update_bits(component,
		AK4333_00_POWER_MANAGEMENT1, 0x01, val);
	/* wait 2ms */
	msleep(2);
	/* PMTIM = 1 */
	snd_soc_component_update_bits(component,
		AK4333_00_POWER_MANAGEMENT1, 0x02, 0x02);
	/* PMLDOP = 1 */
	snd_soc_component_update_bits(component,
		AK4333_01_POWER_MANAGEMENT2, 0x10, 0x10);
	/* wait 1ms (>= 0.5ms) */
	msleep(1);
	/* PMCP = 1 */
	snd_soc_component_update_bits(component,
		AK4333_01_POWER_MANAGEMENT2, 0x01, 0x01);
	/* wait 7ms (>= 6.5ms) */
	msleep(7);
	/* PMLDON = 1 */
	snd_soc_component_update_bits(component,
		AK4333_01_POWER_MANAGEMENT2, 0x20, 0x20);
	/* wait 1ms (>= 0.5ms) */
	msleep(1);
}

static void ak4333_power_down(struct snd_soc_component *component)
{
	/* PMLDON = 0 */
	snd_soc_component_update_bits(component,
		AK4333_01_POWER_MANAGEMENT2, 0x20, 0x00);
	/* PMCP = 0 */
	snd_soc_component_update_bits(component,
		AK4333_01_POWER_MANAGEMENT2, 0x01, 0x00);
	/* PMLDOP = 0 */
	snd_soc_component_update_bits(component,
		AK4333_01_POWER_MANAGEMENT2, 0x10, 0x00);
	/* PMTIM = 0 */
	snd_soc_component_update_bits(component,
		AK4333_00_POWER_MANAGEMENT1, 0x02, 0x00);
	/* PMPLL = 0 */
	snd_soc_component_update_bits(component,
		AK4333_00_POWER_MANAGEMENT1, 0x01, 0x00);
}

static int get_detected_impedance(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	int ret;
	unsigned int i, val;

	ret = 0;

	if (!ak4333->nDACOn)
		ak4333_power_up(component, ak4333);

	/* IDST = 1 */
	snd_soc_component_update_bits(component,
		AK4333_08_IMPEDANCE_DET_SETTING, 0x02, 0x02);

	/* Wait to IDE to 1 */
	for (i = 0; i <= 500; i++) {
		ret |= snd_soc_component_read(component,
			AK4333_09_IMPEDANCE_DET_STATUS, &val);
		if (val & 0x08)
			ret = -EIO;
		if (ret || (val & 0x01))
			break;

		msleep(1);
	}

	if (i >= 500)
		ret = -EIO;

	if (!ret) {
		/* get IMP */
		snd_soc_component_read(component,
			AK4333_13_IMPEDANCE_DET_RESULT, &val);
		ucontrol->value.enumerated.item[0] = val & 0x7F;
	} else {
		ucontrol->value.enumerated.item[0] = 0x80;
	}

	/* CLR = 1 */
	snd_soc_component_update_bits(component,
		AK4333_08_IMPEDANCE_DET_SETTING, 0x01, 0x01);

	if (!ak4333->nDACOn)
		ak4333_power_down(component);

	return ret;
}

static int set_detected_impedance(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	return 0;
}

#ifdef AK4333_DEBUG
static const char * const test_reg_select[] = {
	"read ak4333 Reg 00:28",
};

static const struct soc_enum ak4333_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
};

static int nTestRegNo;

static int get_test_reg(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = nTestRegNo;

	return 0;
}

static int set_test_reg(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol
)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	u32 currMode = ucontrol->value.enumerated.item[0];
	int i, value;
	int regs, rege;

	nTestRegNo = currMode;

	regs = 0x00;
	rege = 0x17;

	for (i = regs; i <= rege; i++) {
		value = ak4333_i2c_read(component, i);
		akdbgprt("***ak4333 Addr,Reg=(%02X, %02X)\n", i, value);
	}

	regs = 0x26;
	rege = 0x28;

	for (i = regs; i <= rege; i++) {
		value = ak4333_i2c_read(component, i);
		akdbgprt("***ak4333 Addr,Reg=(%02X, %02X)\n", i, value);
	}

	return 0;
}

#endif

static const struct snd_kcontrol_new ak4333_snd_controls[] = {
	SOC_SINGLE_TLV("AK4333 Digital Output VolumeL",
		AK4333_0B_LCH_OUTPUT_VOLUME, 0, 0x1F, 0, ovl_tlv),
	SOC_SINGLE_TLV("AK4333 Digital Output VolumeR",
		AK4333_0C_RCH_OUTPUT_VOLUME, 0, 0x1F, 0, ovr_tlv),
	SOC_SINGLE_TLV("AK4333 HP-Amp Analog Volume",
		AK4333_0D_HP_VOLUME_CONTROL, 0, 0x7, 0, hpg_tlv),

	SOC_ENUM("AK4333 Digital Volume Control", ak4333_dac_enum[0]),
	SOC_ENUM("AK4333 DACL Signal Level", ak4333_dac_enum[1]),
	SOC_ENUM("AK4333 DACR Signal Level", ak4333_dac_enum[2]),
	SOC_ENUM("AK4333 DACL Signal Invert", ak4333_dac_enum[3]),
	SOC_ENUM("AK4333 DACR Signal Invert", ak4333_dac_enum[4]),
	SOC_ENUM("AK4333 HPL Power-down Resistor", ak4333_dac_enum[5]),
	SOC_ENUM("AK4333 HPR Power-down Resistor", ak4333_dac_enum[6]),
	SOC_ENUM("AK4333 DAC Digital Filter Mode", ak4333_dac_enum[7]),
	SOC_ENUM("AK4333 BCLK Output Frequency", ak4333_dac_enum[8]),
	SOC_ENUM("AK4333 HPG Zero Cross Timeout", ak4333_dac_enum[9]),

	SOC_ENUM_EXT("AK4333 BCLK Frequency",
		ak4333_bitset_enum[0], get_bickfs, set_bickfs),
	SOC_ENUM_EXT("AK4333 PLL MCKI Frequency",
		ak4333_bitset_enum[1], get_pllmcki, set_pllmcki),
	SOC_ENUM_EXT("AK4333 PLL Mode",
		ak4333_bitset_enum[2], get_pllmode, set_pllmode),
	SOC_ENUM_EXT("AK4333 Impedance Detection",
		ak4333_bitset_enum[3], get_detected_impedance,
		set_detected_impedance),

#ifdef AK4333_DEBUG
	SOC_ENUM_EXT("AK4333 Reg Read",
		ak4333_enum[0], get_test_reg, set_test_reg),
#endif

};

static int ak4333_dac_event(
	struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol,
	int event
)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[ak4333] %s(%d)\n", __func__, __LINE__);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* before widget power up */
		ak4333->nDACOn = 1;
		ak4333_power_up(component, ak4333);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* after widget power down */
		ak4333_power_down(component);
		ak4333->nDACOn = 0;
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new ak4333_hpl_mixer_controls[] = {
	SOC_DAPM_SINGLE("LDACL", AK4333_07_DAC_MONO_MIXING, 0, 1, 0),
	SOC_DAPM_SINGLE("RDACL", AK4333_07_DAC_MONO_MIXING, 1, 1, 0),
};

static const struct snd_kcontrol_new ak4333_hpr_mixer_controls[] = {
	SOC_DAPM_SINGLE("LDACR", AK4333_07_DAC_MONO_MIXING, 4, 1, 0),
	SOC_DAPM_SINGLE("RDACR", AK4333_07_DAC_MONO_MIXING, 5, 1, 0),
};

/* DMIC Switch */
static const char * const ak4333_dmic_lch_select_texts[] = {
	"Off", "On"
};

static SOC_ENUM_SINGLE_VIRT_DECL(ak4333_dmic_lch_enum,
	ak4333_dmic_lch_select_texts);

static const struct snd_kcontrol_new ak4333_dmic_lch_mux_control =
	SOC_DAPM_ENUM("DMICL Switch", ak4333_dmic_lch_enum);

static const char * const ak4333_dmic_rch_select_texts[] = {
	"Off", "On"
};

static SOC_ENUM_SINGLE_VIRT_DECL(ak4333_dmic_rch_enum,
	ak4333_dmic_rch_select_texts);

static const struct snd_kcontrol_new ak4333_dmic_rch_mux_control =
	SOC_DAPM_ENUM("DMICR Switch", ak4333_dmic_rch_enum);

static const struct snd_soc_dapm_widget ak4333_dapm_widgets[] = {
	/* DAC */
	SND_SOC_DAPM_DAC_E("AK4333 DAC",
		NULL, AK4333_02_POWER_MANAGEMENT3, 0, 0, ak4333_dac_event,
		(SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD)),

	/* Digital Input */
	SND_SOC_DAPM_AIF_IN("AK4333 SDTI", "Playback", 0, SND_SOC_NOPM, 0, 0),

	/* Analog Output */
	SND_SOC_DAPM_OUTPUT("AK4333 HPL"),
	SND_SOC_DAPM_OUTPUT("AK4333 HPR"),

	SND_SOC_DAPM_MIXER("AK4333 HPR Mixer",
		AK4333_03_POWER_MANAGEMENT4, 1, 0,
		&ak4333_hpr_mixer_controls[0],
		ARRAY_SIZE(ak4333_hpr_mixer_controls)),
	SND_SOC_DAPM_MIXER("AK4333 HPL Mixer",
		AK4333_03_POWER_MANAGEMENT4, 0, 0,
		&ak4333_hpl_mixer_controls[0],
		ARRAY_SIZE(ak4333_hpl_mixer_controls)),
};

static const struct snd_soc_dapm_route ak4333_intercon[] = {
	{"AK4333 DAC",       NULL,    "AK4333 SDTI"},

	{"AK4333 HPL Mixer", "LDACL", "AK4333 DAC"},
	{"AK4333 HPL Mixer", "RDACL", "AK4333 DAC"},
	{"AK4333 HPR Mixer", "LDACR", "AK4333 DAC"},
	{"AK4333 HPR Mixer", "RDACR", "AK4333 DAC"},

	{"AK4333 HPL",       NULL,    "AK4333 HPL Mixer"},
	{"AK4333 HPR",       NULL,    "AK4333 HPR Mixer"},
};

static int ak4333_set_mcki(struct snd_soc_component *component,
	int fs, int rclk)
{
	u8 mode;
	int mcki_rate;

	akdbgprt("\t[ak4333] %s fs=%d rclk=%d\n", __func__, fs, rclk);

	if ((fs != 0) && (rclk != 0)) {
		if (rclk > 24576000)
			return -EINVAL;

		mcki_rate = rclk / fs;
		switch (mcki_rate) {
		case 128:
			mode = AK4333_CM_3;
			break;
		case 256:
			mode = AK4333_CM_0;
			break;
		case 512:
			mode = AK4333_CM_1;
			break;
		case 1024:
			mode = AK4333_CM_2;
			break;
		default:
			return -EINVAL;
		}
		snd_soc_component_update_bits(component,
			AK4333_05_CLOCK_MODE_SELECT, AK4333_CM, mode);
	}

	return 0;
}

static int ak4333_set_pllblock(struct snd_soc_component *component, int nfs)
{
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	const int pllRefClockMin = 76800;
	const int pllRefClockMax = 768000;
	const int pllRefClockMid = 256000;
	const int pldMax = 65536;
	int pllClock, dacClk;
	int pllInFreq;
	int pldRefFreq;
	int mdiv, rest;
	int plm, pld;
	int val;
	u8 mode;

	if (ak4333->PLLMode == 0)
		return 0;

	if (nfs <= 48000) {
		mode = AK4333_CM_1;
		dacClk = 512 * nfs;
	} else if (nfs <= 96000) {
		mode = AK4333_CM_0;
		dacClk = 256 * nfs;
	} else {
		mode = AK4333_CM_3;
		dacClk = 128 * nfs;
	}
	snd_soc_component_update_bits(component,
		AK4333_05_CLOCK_MODE_SELECT, AK4333_CM, mode);

	if ((nfs % 8000) == 0)
		pllClock = 24576000;
	else
		pllClock = 22579200;

	/* BICK_PLL (Slave) */
	if (ak4333->PLLMode == 1) {
		if (ak4333->bickFreq == 0) {
			 /* 32fs */
			pllInFreq = 32 * nfs;
		} else if (ak4333->bickFreq == 1) {
			/* 48fs */
			pllInFreq = 48 * nfs;
		} else {
			pllInFreq = 64 * nfs;
		}
	} else {
		/* MCKI PLL (Master) */
		pllInFreq = mcktab[ak4333->mckFreq];
	}

	mdiv = pllClock / dacClk;
	rest = (pllClock % dacClk);

	/* See Note 65 in the data sheet */
	if ((mode == AK4333_CM_2 && nfs == 16000) ||
		(mode == AK4333_CM_1 && nfs == 32000)) {
		mdiv = 1;
		rest = 0;
	}

	if (rest != 0) {
		pr_err("%s: Error PLL Setting\n", __func__);
		return -EINVAL;
	}

	plm = 1;
	pld = 1;

	do {
		pldRefFreq = pllInFreq / pld;
		if (pldRefFreq < pllRefClockMin)
			break;

		rest = (pllInFreq % pld);
		if ((pldRefFreq > pllRefClockMax) || (rest != 0)) {
			pld++;
			continue;
		}
		rest = (pllClock % pldRefFreq);
		if (rest == 0) {
			plm = pllClock / pldRefFreq;
			break;
		}
		pld++;
	} while (pld < pldMax);

	if ((pldRefFreq < pllRefClockMin) || (pld > pldMax)) {
		pr_err("%s: Error PLL Setting 2\n", __func__);
		return -EINVAL;
	}

	akdbgprt("\t[ak4333] %s dacClk=%dHz, pllInFreq=%dHz, pldRefFreq=%dHz\n",
		__func__, dacClk, pllInFreq, pldRefFreq);
	akdbgprt("\t[ak4333] %s pld=%d, plm=%d mdiv=%d\n",
		__func__, pld, plm, mdiv);

	pld--;
	plm--;
	mdiv--;

	/* PLD15-0 */
	snd_soc_component_update_bits(component,
		AK4333_0F_PLL_REF_CLK_DIVIDER1, 0xFF, ((pld & 0xFF00) >> 8));
	snd_soc_component_update_bits(component,
		AK4333_10_PLL_REF_CLK_DIVIDER2, 0xFF, ((pld & 0x00FF) >> 0));

	/* PLM15-0 */
	snd_soc_component_update_bits(component,
		AK4333_11_PLL_FB_CLK_DIVIDER1, 0xFF, ((plm & 0xFF00) >> 8));
	snd_soc_component_update_bits(component,
		AK4333_12_PLL_FB_CLK_DIVIDER2, 0xFF, ((plm & 0x00FF) >> 0));

	snd_soc_component_update_bits(component,
		AK4333_14_DAC_CLK_DIVIDER, 0xFF, mdiv);

	val = ((pldRefFreq < pllRefClockMid) ? 0x10 : 0x00);

	snd_soc_component_update_bits(component,
		AK4333_0E_PLL_CLK_SOURCE_SELECT, 0x10, val);

	return 0;
}

static int ak4333_hw_params_set(struct snd_soc_component *component, int nfs1)
{
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	u8 fs;

	akdbgprt("\t[ak4333] %s(%d)\n", __func__, __LINE__);

	ak4333->fs = nfs1;

	switch (nfs1) {
	case 8000:
		fs = AK4333_FS_8KHZ;
		break;
	case 11025:
		fs = AK4333_FS_11_025KHZ;
		break;
	case 16000:
		fs = AK4333_FS_16KHZ;
		break;
	case 22050:
		fs = AK4333_FS_22_05KHZ;
		break;
	case 32000:
		fs = AK4333_FS_32KHZ;
		break;
	case 44100:
		fs = AK4333_FS_44_1KHZ;
		break;
	case 48000:
		fs = AK4333_FS_48KHZ;
		break;
	case 88200:
		fs = AK4333_FS_88_2KHZ;
		break;
	case 96000:
		fs = AK4333_FS_96KHZ;
		break;
	case 176400:
		fs = AK4333_FS_176_4KHZ;
		break;
	case 192000:
		fs = AK4333_FS_192KHZ;
		break;
	default:
		return -EINVAL;
	}
	snd_soc_component_update_bits(component,
		AK4333_05_CLOCK_MODE_SELECT, 0x1F, fs);

	if (ak4333->PLLMode == 0) {
		/* Not PLL mode */
		ak4333_set_mcki(component, nfs1, ak4333->rclk);
	} else {
		/* PLL mode */
		ak4333_set_pllblock(component, nfs1);
	}

	return 0;
}

static int ak4333_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai
)
{
	struct snd_soc_component *component = dai->component;
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	ak4333->fs = params_rate(params);

	ak4333_hw_params_set(component, ak4333->fs);

	switch (params_width(params)) {
	case 16:
		snd_soc_component_update_bits(component,
			AK4333_15_AUDIO_IF_FORMAT, 0x03, 0x01);
		break;
	case 24:
		snd_soc_component_update_bits(component,
			AK4333_15_AUDIO_IF_FORMAT, 0x03, 0x00);
		break;
	case 32:
		snd_soc_component_update_bits(component,
			AK4333_15_AUDIO_IF_FORMAT, 0x02, 0x02);
		break;
	}

	return 0;
}

static int ak4333_set_dai_sysclk(
	struct snd_soc_dai *dai,
	int clk_id,
	unsigned int freq,
	int dir
)
{
	struct snd_soc_component *component = dai->component;
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[ak4333] %s(%d)\n", __func__, __LINE__);

	ak4333->rclk = freq;

	if (ak4333->PLLMode == 0) {
		/* Not PLL mode */
		ak4333_set_mcki(component, ak4333->fs, freq);
	}

	return 0;
}

static int ak4333_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	unsigned int format;

	akdbgprt("\t[ak4333] %s(%d)\n", __func__, __LINE__);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		format = AK4333_SLAVE_MODE;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		format = AK4333_MASTER_MODE;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(component->dev, "Clock mode unsupported");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format |= AK4333_DIF_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format |= AK4333_DIF_MSB_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* set format */
	snd_soc_component_update_bits(component,
		AK4333_15_AUDIO_IF_FORMAT, AK4333_DIF, format);

	return 0;
}

static int ak4333_set_bias_level(
	struct snd_soc_component *component,
	enum snd_soc_bias_level level
)
{
	akdbgprt("\t[AK4333] %s(%d)\n", __func__, __LINE__);

	component->dapm.bias_level = level;

	return 0;
}

static int ak4333_set_dai_mute(struct snd_soc_dai *dai, int mute)
{
	akdbgprt("\t[ak4333] %s mute[%s]\n", __func__, mute ? "ON" : "OFF");

	/* wait 24ms (>= 23.9ms) */
	if (mute == 0)
		msleep(24);

	return 0;
}

#define AK4333_RATES ( \
	SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | \
	SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | \
	SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 | \
	SNDRV_PCM_RATE_192000 \
)

#define AK4333_FORMATS ( \
	SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE \
)

static struct snd_soc_dai_ops ak4333_dai_ops = {
	.hw_params = ak4333_hw_params,
	.set_sysclk = ak4333_set_dai_sysclk,
	.set_fmt = ak4333_set_dai_fmt,
	.digital_mute = ak4333_set_dai_mute,
};

struct snd_soc_dai_driver ak4333_dai[] = {
	{
		.name = "ak4333-aif",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AK4333_RATES,
			.formats = AK4333_FORMATS,
		},
		.ops = &ak4333_dai_ops,
	},
};

static int ak4333_init_reg(struct snd_soc_component *component)
{
	unsigned int DeviceID;
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4333] %s(%d)\n", __func__, __LINE__);

	if (ak4333->pdn_gpio > 0) {
		gpio_set_value(ak4333->pdn_gpio, GPO_PDN_LOW);
		msleep(1);
		gpio_set_value(ak4333->pdn_gpio, GPO_PDN_HIGH);
		msleep(1);
	}

	snd_soc_component_read(component, AK4333_15_AUDIO_IF_FORMAT, &DeviceID);

	switch (DeviceID >> 5) {
	case 0:
		/* 0:AK4375 */
		ak4333->nDeviceID = 0;
		akdbgprt("AK4375 is connecting.\n");
		break;
	case 1:
		/* 1:AK4375A */
		ak4333->nDeviceID = 1;
		akdbgprt("AK4375A is connecting.\n");
		break;
	case 2:
		/* 2:AK4376 */
		ak4333->nDeviceID = 2;
		akdbgprt("AK4376 is connecting.\n");
		break;
	case 3:
		/* 3:AK4377 */
		ak4333->nDeviceID = 3;
		akdbgprt("AK4377 is connecting.\n");
		break;
	case 4:
		/* 4:AK4333 */
		ak4333->nDeviceID = 4;
		akdbgprt("AK4333 is connecting.\n");
		break;
	case 7:
		/* 7:AK4333 */
		ak4333->nDeviceID = 7;
		akdbgprt("AK4333 is connecting.\n");
		break;
	default:
		/* 8:Other IC */
		ak4333->nDeviceID = 8;
		akdbgprt("AK43XX DAC is not connected.\n");
	}

	snd_soc_component_write(component,
		AK4333_26_DAC_ADJUSTMENT1, AK4333_REG_26_VAL);
	snd_soc_component_write(component,
		AK4333_27_DAC_ADJUSTMENT2, AK4333_REG_27_VAL);
	snd_soc_component_write(component,
		AK4333_28_DAC_ADJUSTMENT3, AK4333_REG_28_VAL);

	return 0;
}

static int ak4333_parse_dt(struct ak4333_priv *ak4333)
{
	struct device *dev;
	struct device_node *np;

	dev = &(ak4333->i2c->dev);
	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ak4333->pdn_gpio = of_get_named_gpio(np, "ak4333,pdn-gpio", 0);
	if (ak4333->pdn_gpio < 0) {
		ak4333->pdn_gpio = -1;
		return -EINVAL;
	}

	return 0;
}

static int ak4333_set_pinctrl(struct ak4333_priv *ak4333)
{
	struct device *dev;
	struct device_node *np;
	int ret;

	dev = &(ak4333->i2c->dev);

	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ak4333->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(ak4333->pinctrl)) {
		dev_err(dev, "failed to look up pinctrl");
		return -EINVAL;
	}

	ak4333->pin_default = pinctrl_lookup_state(ak4333->pinctrl,
			"default");
	if (IS_ERR_OR_NULL(ak4333->pin_default)) {
		dev_err(dev, "failed to look up default pin state");
		devm_pinctrl_put(ak4333->pinctrl);
		return -EINVAL;
	}

	ret = pinctrl_select_state(ak4333->pinctrl,
			ak4333->pin_default);
	if (ret != 0) {
		dev_err(dev, "Failed to set default pin state\n");
		devm_pinctrl_put(ak4333->pinctrl);
		return ret;
	}

	return 0;
}

static void ak4333_hp_det_report(struct work_struct *work)
{
	int value;
	struct delayed_work *dwork;
	struct ak4333_priv *ak4333;

	dwork = to_delayed_work(work);
	ak4333 = container_of(dwork, struct ak4333_priv, hp_det_dwork);

	if (gpio_is_valid(ak4333->hp_irq_gpio)) {
		value = gpio_get_value(ak4333->hp_irq_gpio);
		akdbgprt("%s :gpio value = %d\n", __func__, value);
		snd_soc_jack_report(&ak4333->jack, value ? 0 :
				SND_JACK_HEADPHONE, AK4333_JACK_MASK);
	}
}

static irqreturn_t ak4333_headset_det_irq_thread(int irq, void *data)
{
	struct ak4333_priv *ak4333;

	ak4333 = (struct ak4333_priv *)data;
	cancel_delayed_work_sync(&ak4333->hp_det_dwork);
	schedule_delayed_work(&ak4333->hp_det_dwork, msecs_to_jiffies(500));

	return IRQ_HANDLED;
}

static int ak4333_parse_dt_hp_det(struct ak4333_priv *ak4333)
{
	struct device *dev;
	struct device_node *np;
	int rc;

	dev = &(ak4333->i2c->dev);
	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ak4333->hp_irq_gpio = of_get_named_gpio(np,
						"ak4333,headset-det-gpio", 0);

	rc = gpio_request(ak4333->hp_irq_gpio, "AK4333 hp-det");
	if (rc) {
		dev_err(dev, "\t[AK4333] %s(%d) AK4333 hp-det gpio request fail\n",
				__func__, __LINE__);
		ak4333->hp_irq_gpio = -1;
		return rc;
	}

	rc = gpio_direction_input(ak4333->hp_irq_gpio);
	if (rc) {
		akdbgprt("\t[AK4333] %s(%d) hp-det gpiog input fail\n",
				__func__, __LINE__);
		ak4333->hp_irq_gpio = -1;
		gpio_free(ak4333->hp_irq_gpio);
		return -EINVAL;
	}

	if (!gpio_is_valid(ak4333->hp_irq_gpio)) {
		dev_err(dev, "AK4333 pdn pin(%u) is invalid\n",
						ak4333->hp_irq_gpio);
		ak4333->hp_irq_gpio = -1;
		gpio_free(ak4333->hp_irq_gpio);
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&ak4333->hp_det_dwork, ak4333_hp_det_report);
	rc = snd_soc_card_jack_new(ak4333->card, "AK4333 Headphone",
		AK4333_JACK_MASK, &ak4333->jack, NULL, 0);
	if (rc < 0) {
		dev_err(dev, "Cannot create jack\n");
		goto error;
	}

	ak4333->hp_irq = gpio_to_irq(ak4333->hp_irq_gpio);
	rc = request_threaded_irq(ak4333->hp_irq, NULL,
		ak4333_headset_det_irq_thread,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"ak4333", ak4333);
	if (rc != 0) {
		dev_err(dev, "Failed to request IRQ %d: %d\n",
			ak4333->hp_irq, rc);
		goto error;
	}

	rc = enable_irq_wake(ak4333->hp_irq);
	if (rc) {
		dev_err(dev,
			"Failed to set wake interrupt on IRQ %d: %d\n",
			ak4333->hp_irq, rc);
		free_irq(ak4333->hp_irq, ak4333);
		goto error;
	}

	schedule_delayed_work(&ak4333->hp_det_dwork, msecs_to_jiffies(200));

error:
	return rc;
}

static int ak4333_probe(struct snd_soc_component *component)
{
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	akdbgprt("\t[AK4333] %s(%d)\n", __func__, __LINE__);

	ak4333->card = component->card;

	ret = ak4333_parse_dt(ak4333);

	ret = ak4333_set_pinctrl(ak4333);
	if (ret)
		akdbgprt("\t[AK4333] %s(%d) set_pinctrl ret=%d\n",
				__func__, __LINE__, ret);

	if (ak4333->pdn_gpio != -1) {
		ret = gpio_request(ak4333->pdn_gpio, "ak4333 pdn");
		akdbgprt("\t[AK4333] %s : gpio_request ret = %d\n",
			__func__, ret);
		gpio_direction_output(ak4333->pdn_gpio, 0);
	}

	ret = ak4333_parse_dt_hp_det(ak4333);
	if (ret != 0) {
		akdbgprt("\t[AK4333] %s(%d) cannot setup headphone detection\n",
				__func__, __LINE__);
		regulator_disable(ak4333->avdd_core);
		return ret;
	}

	ak4333->rclk = 12288000;
	ak4333->fs = 48000;
	ak4333->nDeviceID = 0;
	ak4333->nDACOn = 0;
	ak4333->PLLMode = 2;
	ak4333->mckFreq = 0;
	ak4333->bickFreq = 0;

	ak4333_init_reg(component);

	return ret;
}

static void ak4333_remove(struct snd_soc_component *component)
{
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	struct device *dev = &(ak4333->i2c->dev);
	int ret = 0;

	akdbgprt("\t[AK4333] %s(%d)\n", __func__, __LINE__);

	if (ak4333->pdn_gpio > 0) {
		gpio_set_value(ak4333->pdn_gpio, GPO_PDN_LOW);
		msleep(1);
		gpio_free(ak4333->pdn_gpio);
		msleep(1);
	}

	ret = regulator_disable(ak4333->avdd_core);
	if (ret < 0)
		dev_err(dev, "AVDD regulator_disable failed\n");
}

static int ak4333_suspend(struct snd_soc_component *component)
{
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	struct device *dev = &(ak4333->i2c->dev);
	int ret = 0;

	akdbgprt("\t[AK4333] %s(%d)\n", __func__, __LINE__);

	regcache_cache_only(ak4333->regmap, true);
	regcache_mark_dirty(ak4333->regmap);

	if (ak4333->pdn_gpio > 0) {
		gpio_set_value(ak4333->pdn_gpio, GPO_PDN_LOW);
		msleep(1);
	}

	ret = regulator_disable(ak4333->avdd_core);
	if (ret < 0) {
		dev_err(dev, "AVDD regulator_disable failed\n");
		return ret;
	}

	return 0;
}

static int ak4333_resume(struct snd_soc_component *component)
{
	struct ak4333_priv *ak4333 = snd_soc_component_get_drvdata(component);
	struct device *dev = &(ak4333->i2c->dev);
	int ret = 0;

	akdbgprt("\t[AK4333] %s(%d)\n", __func__, __LINE__);

	if (ak4333->avdd_voltage != 0) {
		ret = regulator_set_voltage(ak4333->avdd_core,
			ak4333->avdd_voltage, ak4333->avdd_voltage);
		if (ret < 0) {
			dev_err(dev, "AVDD regulator_set_voltage failed\n");
			return ret;
		}
	}

	if (ak4333->avdd_current != 0) {
		ret = regulator_set_load(ak4333->avdd_core,
			ak4333->avdd_current);
		if (ret < 0) {
			dev_err(dev, "AVDD regulator_set_load failed\n");
			return ret;
		}
	}

	ret = regulator_enable(ak4333->avdd_core);
	if (ret < 0) {
		dev_err(dev, "AVDD regulator_enable failed\n");
		return ret;
	}

	if (ak4333->pdn_gpio != -1) {
		gpio_set_value(ak4333->pdn_gpio, GPO_PDN_LOW);
		msleep(1);
		akdbgprt("\t[AK4333] %s External PDN[ON]\n", __func__);
		msleep(1);
	}
	regcache_cache_only(ak4333->regmap, false);
	regcache_sync(ak4333->regmap);

	return 0;
}


static const struct snd_soc_component_driver soc_component_dev_ak4333 = {
	.probe = ak4333_probe,
	.remove = ak4333_remove,
	.suspend = ak4333_suspend,
	.resume = ak4333_resume,

	.set_bias_level = ak4333_set_bias_level,

	.controls = ak4333_snd_controls,
	.num_controls = ARRAY_SIZE(ak4333_snd_controls),
	.dapm_widgets = ak4333_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak4333_dapm_widgets),
	.dapm_routes = ak4333_intercon,
	.num_dapm_routes = ARRAY_SIZE(ak4333_intercon),

	.idle_bias_on = 1,
	.endianness = 1,
	.non_legacy_dai_naming = 1,
};

static const struct regmap_config ak4333_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AK4333_MAX_REGISTER,
	.readable_reg = ak4333_readable,
	.volatile_reg = ak4333_volatile,
	.writeable_reg = ak4333_writeable,

	.reg_defaults = ak4333_reg,
	.num_reg_defaults = ARRAY_SIZE(ak4333_reg),
	.cache_type = REGCACHE_RBTREE,
};

static const struct of_device_id ak4333_i2c_dt_ids[] = {
	{.compatible = "akm,ak4333"},
	{}
};
MODULE_DEVICE_TABLE(of, ak4333_i2c_dt_ids
);

static int ak4333_i2c_probe(
	struct i2c_client *i2c,
	const struct i2c_device_id *id
)
{
	struct ak4333_priv *ak4333;
	struct device *dev;
	struct device_node *np;
	int ret = 0;

	akdbgprt("\t[ak4333] %s(%d)\n", __func__, __LINE__);

	dev = &i2c->dev;
	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ak4333 = devm_kzalloc(&i2c->dev,
		sizeof(struct ak4333_priv), GFP_KERNEL);
	if (ak4333 == NULL)
		return -ENOMEM;

	ak4333->avdd_core = devm_regulator_get(&i2c->dev, "ak4333,avdd-core");
	if (IS_ERR(ak4333->avdd_core)) {
		ret = PTR_ERR(ak4333->avdd_core);
		dev_err(&i2c->dev, "Unable to get avdd-core:%d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "ak4333,avdd-voltage",
				&ak4333->avdd_voltage);
	if (ret) {
		dev_err(&i2c->dev,
			"%s:Looking up %s property in node %s failed\n",
			__func__, "ak4333,avdd-voltage",
			np->full_name);
	}

	ret = of_property_read_u32(np, "ak4333,avdd-current",
				&ak4333->avdd_current);
	if (ret) {
		dev_err(&i2c->dev,
			"%s:Looking up %s property in node %s failed\n",
			__func__, "ak4333,avdd-current",
			np->full_name);
	}

	if (ak4333->avdd_voltage != 0) {
		ret = regulator_set_voltage(ak4333->avdd_core,
			ak4333->avdd_voltage, ak4333->avdd_voltage);
		if (ret < 0) {
			dev_err(&i2c->dev, "AVDD regulator_set_voltage failed\n");
			return ret;
		}
	}

	if (ak4333->avdd_current != 0) {
		ret = regulator_set_load(ak4333->avdd_core,
			ak4333->avdd_current);
		if (ret < 0) {
			dev_err(&i2c->dev, "AVDD regulator_set_load failed\n");
			return ret;
		}
	}

	ret = regulator_enable(ak4333->avdd_core);
	if (ret < 0) {
		dev_err(&i2c->dev, "AVDD regulator_enable failed\n");
		return ret;
	}

	ak4333->regmap = devm_regmap_init_i2c(i2c, &ak4333_regmap);
	if (IS_ERR(ak4333->regmap)) {
		akdbgprt("[*****AK4333*****] %s regmap error\n", __func__);
		devm_kfree(&i2c->dev, ak4333);
		return PTR_ERR(ak4333->regmap);
	}

	i2c_set_clientdata(i2c, ak4333);

	ak4333->i2c = i2c;

	/* Fix device name. This name should be consistent with the
	 * codec name specified in the DAI link of machine driver.
	 * In this way, the codec name no longer depends on the dynamic
	 * I2C bus-addr like "ak4333.1-0010".
	 */
	dev_set_name(&i2c->dev, "ak4333");

	ret = devm_snd_soc_register_component(&i2c->dev,
		&soc_component_dev_ak4333, &ak4333_dai[0],
		ARRAY_SIZE(ak4333_dai));
	if (ret < 0) {
		devm_kfree(&i2c->dev, ak4333);
		akdbgprt("\t[ak4333 Error!] %s(%d)\n", __func__, __LINE__);
	}

	akdbgprt("\t[ak4333] %s(%d) pdn1=%d\n, ret=%d",
		__func__, __LINE__, ret);
	return ret;
}

static int ak4333_i2c_remove(struct i2c_client *client)
{
	akdbgprt("\t[ak4333] %s(%d)\n", __func__, __LINE__);

	snd_soc_unregister_component(&client->dev);
	return 0;
}

static const struct i2c_device_id ak4333_i2c_id[] = {
	{"ak4333", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ak4333_i2c_id
);

static struct i2c_driver ak4333_i2c_driver = {
	.driver = {
		.name = "ak4333",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak4333_i2c_dt_ids),
	},
	.probe = ak4333_i2c_probe,
	.remove = ak4333_i2c_remove,
	.id_table = ak4333_i2c_id,
};

static int __init

ak4333_modinit(void)
{
	akdbgprt("\t[ak4333] %s(%d)\n", __func__, __LINE__);
	return i2c_add_driver(&ak4333_i2c_driver);
}

module_init(ak4333_modinit);

static void __exit

ak4333_exit(void)
{
	i2c_del_driver(&ak4333_i2c_driver);
}

module_exit(ak4333_exit);

MODULE_DESCRIPTION("ASoC ak4333 codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
