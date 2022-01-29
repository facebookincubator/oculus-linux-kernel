/*
 * ak4331.c  --  audio driver for AK4331
 *
 * Copyright (C) 2018-2019 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision      DS ver.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      18/01/16	    1.0           00
 *                      19/01/10	    1.0           00
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  kernel version: 4.9 & 4.4 & 3.18
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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
#include <linux/regmap.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/regulator/consumer.h>

#include "ak4331.h"

//#define AK4331_DEBUG

#define REG_26_VAL	0x02
#define REG_27_VAL	0xC0

#ifdef AK4331_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

/* AK4331 Codec Private Data */
struct ak4331_priv {
	unsigned int priv_pdn_en;	/* PDN GPIO pin */
	int pdn1;			/* PDN control, 0:Off, 1:On, 2:No use(assume always On) */
	int pdn2;			/* PDN control for kcontrol */
	int fs1;
	int fs2;
	int rclk;			/* Master Clock */
	int selDain;			/* 0:Bypass, 1:SRC (Dependent on a register bit) */
	int bickFreq;			/* 0:32fs, 1:48fs, 2:64fs */
	int SRCOutFs;			/* 0:48(44.1)kHz, 1:96(88.2)kHz, 2:192(176.4)kHz */
	int PLLMode;			/* 0:PLL OFF, 1:BICK PLL (Slave), 2:MCKI PLL (Master) */
	int mckFreq;			/* 0:9.6MHz, 1:11.2896MHz, 2:12.288MHz, 3:19.2MHz */
	int xtalmode;
	int nDeviceID;			/* 0:AK4375, 1:AK4375A, 2:AK4376, 3:AK4377, 7:AK4331 */
	int nDACOn;
	int pmsm;			/* Soft Mute 0: Disable, 1: Enable */

	struct i2c_client *i2c;
	struct regmap *regmap;

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

/* AK4331 register cache & default register settings */
static const struct reg_default ak4331_reg[] = {
	{ 0x00, 0x00 },	/*	0x00	AK4331_00_POWER_MANAGEMENT1		*/
	{ 0x01, 0x00 },	/*	0x01	AK4331_01_POWER_MANAGEMENT2		*/
	{ 0x02, 0x00 },	/*	0x02	AK4331_02_POWER_MANAGEMENT3		*/
	{ 0x03, 0x00 },	/*	0x03	AK4331_03_POWER_MANAGEMENT4		*/
	{ 0x04, 0x00 },	/*	0x04	AK4331_04_OUTPUT_MODE_SETTING		*/
	{ 0x05, 0x00 },	/*	0x05	AK4331_05_CLOCK_MODE_SELECT		*/
	{ 0x06, 0x00 },	/*	0x06	AK4331_06_DIGITAL_FILTER_SELECT		*/
	{ 0x07, 0x00 },	/*	0x07	AK4331_07_DAC_MONO_MIXING		*/
	{ 0x08, 0x00 },	/*	0x08	AK4331_08_JITTER_CLEANER_SETTING1	*/
	{ 0x09, 0x00 },	/*	0x09	AK4331_09_JITTER_CLEANER_SETTING2	*/
	{ 0x0A, 0x00 },	/*	0x0A	AK4331_0A_JITTER_CLEANER_SETTING3	*/
	{ 0x0B, 0x19 },	/*	0x0B	AK4331_0B_LCH_OUTPUT_VOLUME		*/
	{ 0x0C, 0x19 },	/*	0x0C	AK4331_0C_RCH_OUTPUT_VOLUME		*/
	{ 0x0D, 0x75 },	/*	0x0D	AK4331_0D_HP_VOLUME_CONTROL		*/
	{ 0x0E, 0x00 },	/*	0x0E	AK4331_0E_PLL_CLK_SOURCE_SELECT		*/
	{ 0x0F, 0x00 },	/*	0x0F	AK4331_0F_PLL_REF_CLK_DIVIDER1		*/
	{ 0x10, 0x00 },	/*	0x10	AK4331_10_PLL_REF_CLK_DIVIDER2		*/
	{ 0x11, 0x00 },	/*	0x11	AK4331_11_PLL_FB_CLK_DIVIDER1		*/
	{ 0x12, 0x00 },	/*	0x12	AK4331_12_PLL_FB_CLK_DIVIDER2		*/
	{ 0x13, 0x00 },	/*	0x13	AK4331_13_DAC_CLK_SOURCE		*/
	{ 0x14, 0x00 },	/*	0x14	AK4331_14_DAC_CLK_DIVIDER		*/
	{ 0x15, 0x70 },	/*	0x15	AK4331_15_AUDIO_IF_FORMAT		*/
	{ 0x16, 0x00 },	/*	0x16	AK4331_16_DIGITAL_MIC			*/
	{ 0x17, 0x00 },	/*	0x17	AK4331_17_SIDE_TONE_VOLUME_CONTROL	*/
	{ 0x26, 0x6C },	/*	0x26	AK4331_26_DAC_ADJUSTMENT1		*/
	{ 0x27, 0x40 },	/*	0x27	AK4331_27_DAC_ADJUSTMENT2		*/
	{ 0x29, 0x00 },	/*	0x19	AK4331_29_MODE_COMTROL			*/
};

static bool ak4331_readable(struct device *dev, unsigned int reg)
{
	bool ret = false;

	if (reg <= AK4331_17_SIDE_TONE_VOLUME_CONTROL)
		ret = true;
	else if (reg < AK4331_26_DAC_ADJUSTMENT1)
		ret = false;
	else if (reg <= AK4331_27_DAC_ADJUSTMENT2)
		ret = true;
	else if (reg == AK4331_29_MODE_COMTROL)
		ret = true;

	return ret;

}

bool ak4331_volatile(struct device *dev, unsigned int reg)
{
	int ret = false;

#if 0
	switch (reg) {
	case AK4331_15_AUDIO_IF_FORMAT:
		ret = true;
	default:
		break;
	}
#endif

	return ret;
}

static bool ak4331_writeable(struct device *dev, unsigned int reg)
{
	bool ret = false;

	if (reg <= AK4331_17_SIDE_TONE_VOLUME_CONTROL)
		ret = true;
	else if (reg < AK4331_26_DAC_ADJUSTMENT1)
		ret = false;
	else if (reg <= AK4331_27_DAC_ADJUSTMENT2)
		ret = true;
	else if (reg == AK4331_29_MODE_COMTROL)
		ret = true;

	return ret;
}

static int ak4331_i2c_write(struct snd_soc_component *component,
		unsigned int reg, unsigned int value)
{
	int ret;
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	unsigned char tx[2];
	size_t len = 2;

	tx[0] = (unsigned char)(0xFF & reg);
	tx[1] = (unsigned char)value;

	ret = i2c_master_send(ak4331->i2c, tx, len);

	if (ret < 0) {
		pr_err("\t[ak4331] %s error ret = %d\n", __func__, ret);
		ret = -EIO;
	} else {
		akdbgprt("*** %s addr, data =(0x%X, 0x%X)\n",
				__func__, reg, (int)value);
		ret = 0;
	}

	return(ret);

}

unsigned int ak4331_i2c_read(struct snd_soc_component *component,
		unsigned int reg)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	int ret = -1;
	unsigned char tx[1], rx[1];

	struct i2c_msg xfer[2];
	struct i2c_client *client = ak4331->i2c;

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
		akdbgprt("\t[AK4331] %s error ret = %d\n", __func__, ret);

	return (unsigned int)rx[0];
}

static int ak4331_pdn_control(struct snd_soc_component *component, int pdn)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4331] %s(%d) pdn=%d\n", __func__, __LINE__, pdn);

	if (ak4331->pdn1 == 0 && pdn == 1) {
		gpio_direction_output(ak4331->priv_pdn_en, 1);
		akdbgprt("\t[AK4331] %s(%d) Turn on priv_pdn_en\n",
				__func__, __LINE__);
		ak4331->pdn1 = 1;
		ak4331->pdn2 = 1;
		usleep_range(800, 1000);
		regcache_sync(ak4331->regmap);
		ak4331_i2c_write(component, AK4331_26_DAC_ADJUSTMENT1,
				REG_26_VAL);
		ak4331_i2c_write(component, AK4331_27_DAC_ADJUSTMENT2,
				REG_27_VAL);
	} else if (ak4331->pdn1 == 1 && pdn == 0) {
		gpio_direction_output(ak4331->priv_pdn_en, 0);
		akdbgprt("\t[AK4331] %s(%d) Turn off priv_pdn_en\n",
				__func__, __LINE__);
		ak4331->pdn1 = 0;
		ak4331->pdn2 = 0;
		usleep_range(800, 1000);
		regcache_mark_dirty(ak4331->regmap);
	}

	return 0;
}

/*
 * Output Digital volume control:
 * from -12.5 to 3 dB in 0.5 dB steps (mute instead of -12.5 dB)
 */
static DECLARE_TLV_DB_SCALE(ovl_tlv, -1250, 50, 0);
static DECLARE_TLV_DB_SCALE(ovr_tlv, -1250, 50, 0);

/*
 * HP-Amp Analog volume control:
 * from -12 to 4 dB in 2 dB steps (mute instead of -12 dB)
 */
static DECLARE_TLV_DB_SCALE(hpg_tlv, -1200, 200, 0);

/*
 * Digital MIC Side Tone Volume:
 * from -24 to 0 dB in 6 dB steps
 */
static DECLARE_TLV_DB_SCALE(sv_tlv, -2400, 600, 0);

static const char * const ak4331_ovolcn_select_texts[] = {
	"Dependent", "Independent"
};

static const char * const ak4331_mdacl_select_texts[] = {
	"x1", "x1/2"
};

static const char * const ak4331_mdacr_select_texts[] = {
	"x1", "x1/2"
};

static const char * const ak4331_invl_select_texts[] = {
	"Normal", "Inverting"
};

static const char * const ak4331_invr_select_texts[] = {
	"Normal", "Inverting"
};

static const char * const ak4331_cpmod_select_texts[] = {
	"Automatic Switching", "+-VDD Operation", "+-1/2VDD Operation"
};

static const char * const ak4331_hphl_select_texts[] = {
	"4ohm", "95kohm"
};

static const char * const ak4331_hphr_select_texts[] = {
	"4ohm", "95kohm"
};

static const char * const ak4331_dacfil_select_texts[] = {
	"Sharp Roll-Off", "Slow Roll-Off", "Short Delay Sharp Roll-Off",
	"Short Delay Slow Roll-Off"
};

static const char * const ak4331_bcko_select_texts[] = {
	"64fs", "32fs"
};

static const char * const ak4331_smtcycle_on_select[] = {
	"1024/FSO", "2048/FSO", "4096/FSO", "8192/FSO"
};

static const struct soc_enum ak4331_dac_enum[] = {
	SOC_ENUM_SINGLE(AK4331_0B_LCH_OUTPUT_VOLUME, 7,
			ARRAY_SIZE(ak4331_ovolcn_select_texts),
			ak4331_ovolcn_select_texts),
	SOC_ENUM_SINGLE(AK4331_07_DAC_MONO_MIXING, 2,
			ARRAY_SIZE(ak4331_mdacl_select_texts),
			ak4331_mdacl_select_texts),
	SOC_ENUM_SINGLE(AK4331_07_DAC_MONO_MIXING, 6,
			ARRAY_SIZE(ak4331_mdacr_select_texts),
			ak4331_mdacr_select_texts),
	SOC_ENUM_SINGLE(AK4331_07_DAC_MONO_MIXING, 3,
			ARRAY_SIZE(ak4331_invl_select_texts),
			ak4331_invl_select_texts),
	SOC_ENUM_SINGLE(AK4331_07_DAC_MONO_MIXING, 7,
			ARRAY_SIZE(ak4331_invr_select_texts),
			ak4331_invr_select_texts),
	SOC_ENUM_SINGLE(AK4331_03_POWER_MANAGEMENT4, 2,
			ARRAY_SIZE(ak4331_cpmod_select_texts),
			ak4331_cpmod_select_texts),
	SOC_ENUM_SINGLE(AK4331_04_OUTPUT_MODE_SETTING, 0,
			ARRAY_SIZE(ak4331_hphl_select_texts),
			ak4331_hphl_select_texts),
	SOC_ENUM_SINGLE(AK4331_04_OUTPUT_MODE_SETTING, 1,
			ARRAY_SIZE(ak4331_hphr_select_texts),
			ak4331_hphr_select_texts),
	SOC_ENUM_SINGLE(AK4331_06_DIGITAL_FILTER_SELECT, 6,
			ARRAY_SIZE(ak4331_dacfil_select_texts),
			ak4331_dacfil_select_texts),
	SOC_ENUM_SINGLE(AK4331_15_AUDIO_IF_FORMAT, 3,
			ARRAY_SIZE(ak4331_bcko_select_texts),
			ak4331_bcko_select_texts),
	SOC_ENUM_SINGLE(AK4331_09_JITTER_CLEANER_SETTING2, 2,
			ARRAY_SIZE(ak4331_smtcycle_on_select),
			ak4331_smtcycle_on_select),
};

static const char * const bickfreq_on_select[] = {
	"48fs", "32fs", "64fs"
};

static const char * const srcoutfs_on_select[] = {
#ifdef SRC_OUT_FS_48K
	"48kHz", "96kHz", "192kHz"
#else
	"44.1kHz", "88.2kHz", "176.4kHz"
#endif
};

static const char * const pllmcki_on_select[] = {
	"9.6MHz", "11.2896MHz", "12.288MHz", "19.2MHz"
};

static int mcktab[] = {
	9600000, 11289600, 12288000, 19200000
};

static const char * const xtal_on_select[] = {
	"Off", "On"
};

static const char * const pllmode_on_select[] = {
	"PLL_OFF", "PLL_BICK_MODE", "PLL_MCKI_MODE"
};

static const char * const src_on_select[] = {
	"Bypass", "On"
};

static const struct soc_enum ak4331_bitset_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(bickfreq_on_select), bickfreq_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(srcoutfs_on_select), srcoutfs_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllmcki_on_select), pllmcki_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(xtal_on_select), xtal_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllmode_on_select), pllmode_on_select),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(src_on_select), src_on_select),
};

static const char * const ak4331_dmic_hpf_fc[] = {
	"30Hz", "15Hz", "120Hz", "240Hz"
};

static const char * const ak4331_dmic_clock[] = {
	"Lch", "Rch"
};

static const char * const ak4331_dmic_init_cycle[] = {
	"1059/fs", "267/fs", "2115/fs", "531/fs"
};

static const struct soc_enum ak4331_dmic_enum[] = {
	SOC_ENUM_SINGLE(AK4331_17_SIDE_TONE_VOLUME_CONTROL, 1,
			ARRAY_SIZE(ak4331_dmic_hpf_fc), ak4331_dmic_hpf_fc),
	SOC_ENUM_SINGLE(AK4331_16_DIGITAL_MIC, 4,
			ARRAY_SIZE(ak4331_dmic_clock), ak4331_dmic_clock),
	SOC_ENUM_SINGLE(AK4331_16_DIGITAL_MIC, 0,
			ARRAY_SIZE(ak4331_dmic_init_cycle),
			ak4331_dmic_init_cycle),
};


static int get_bickfs(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4331->bickFreq;

	return 0;
}

static int ak4331_set_bickfs(struct snd_soc_component *component)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	if (ak4331->bickFreq == 0) {
		/* 48fs */
		/* DL1-0=00(24bit, >= 48fs) */
		snd_soc_component_update_bits(component,
				AK4331_15_AUDIO_IF_FORMAT, 0x03, 0x00);
	} else if (ak4331->bickFreq == 1) {
		/* 32fs */
		/* DL1-0=01(16bit, >= 32fs) */
		snd_soc_component_update_bits(component,
				AK4331_15_AUDIO_IF_FORMAT, 0x03, 0x01);
	} else {
		/* 64fs */
		/* DL1-0=1x(32bit, >= 64fs) */
		snd_soc_component_update_bits(component,
				AK4331_15_AUDIO_IF_FORMAT, 0x02, 0x02);
	}

	return 0;
}

static int set_bickfs(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->bickFreq = ucontrol->value.enumerated.item[0];

	ak4331_set_bickfs(component);

	return 0;
}

static int get_srcfs(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4331->SRCOutFs;

	return 0;
}

static int set_srcfs(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->SRCOutFs = ucontrol->value.enumerated.item[0];

	return 0;
}

static int get_xtalmode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4331->xtalmode;

	return 0;
}

static int set_xtalmode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->xtalmode = ucontrol->value.enumerated.item[0];

	return 0;
}


static int get_pllmcki(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4331->mckFreq;

	return 0;
}

static int set_pllmcki(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->mckFreq = ucontrol->value.enumerated.item[0];

	return 0;
}

static int get_pllmode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4331] %s(%d)\n", __func__, __LINE__);

	ucontrol->value.enumerated.item[0] = ak4331->PLLMode;

	return 0;
}

static int set_pllmode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->PLLMode = ucontrol->value.enumerated.item[0];

	akdbgprt("\t[AK4331] %s(%d) PLLMode=%d\n",
			__func__, __LINE__, ak4331->PLLMode);

	return 0;
}

static int get_srcmode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4331->selDain;

	return 0;
}

static int set_srcmode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->selDain = ucontrol->value.enumerated.item[0];

	return 0;
}

static const char * const smute_enable[] = {
	"Off",
	"On"
};

static const struct soc_enum ak4331_smute_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(smute_enable), smute_enable),
};

static int get_smute_enable(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4331->pmsm;

	return 0;
}

static int set_smute_enable(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->pmsm = ucontrol->value.enumerated.item[0];

	if (ak4331->pmsm) {
		snd_soc_component_update_bits(component,
				AK4331_02_POWER_MANAGEMENT3, 0x30, 0x30);
	} else {
		snd_soc_component_update_bits(component,
				AK4331_02_POWER_MANAGEMENT3, 0x30, 0x00);
		snd_soc_component_update_bits(component,
				AK4331_09_JITTER_CLEANER_SETTING2, 0x01, 0x00);
	}

	return 0;
}

static const char * const pdn_on_select[] = {
	"Off",
	"On"
};

static const struct soc_enum ak4331_pdn_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pdn_on_select), pdn_on_select),
};

static int get_pdn(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4331] %s(%d)\n", __func__, __LINE__);

	ucontrol->value.enumerated.item[0] = ak4331->pdn2;

	return 0;
}

static int set_pdn(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->pdn2 = ucontrol->value.enumerated.item[0];

	akdbgprt("\t[AK4331] %s(%d) pdn2=%d\n",
			__func__, __LINE__, ak4331->pdn2);

	if (ak4331->pdn1 == 0)
		ak4331_pdn_control(component, ak4331->pdn2);

	return 0;
}

#ifdef AK4331_DEBUG
static const char * const test_reg_select[] = {
	"read ak4331 Reg 00:27",
};

static const struct soc_enum ak4331_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
};

static int nTestRegNo;

static int get_test_reg(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = nTestRegNo;

	return 0;
}

static int set_test_reg(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	u32 currMode = ucontrol->value.enumerated.item[0];
	int i, value;
	int regs, rege;

	nTestRegNo = currMode;

	regs = 0x00;
	rege = 0x17;

	for (i = regs ; i <= rege ; i++) {
		value = ak4331_i2c_read(component, i);
		pr_info("***ak4331 Addr,Reg={%x, %x)\n", i, value);
	}
	value = ak4331_i2c_read(component, 0x26);
	pr_info("***ak4331 Addr,Reg=(%x, %x)\n", 0x26, value);
	value = ak4331_i2c_read(component, 0x27);
	pr_info("***ak4331 Addr,Reg=(%x, %x)\n", 0x27, value);

	return 0;
}
#endif

static const struct snd_kcontrol_new ak4331_snd_controls[] = {
	SOC_SINGLE_TLV("AK4331 Digital Output VolumeL",
			AK4331_0B_LCH_OUTPUT_VOLUME, 0, 0x1F, 0, ovl_tlv),
	SOC_SINGLE_TLV("AK4331 Digital Output VolumeR",
			AK4331_0C_RCH_OUTPUT_VOLUME, 0, 0x1F, 0, ovr_tlv),
	SOC_SINGLE_TLV("AK4331 HP-Amp Analog Volume",
			AK4331_0D_HP_VOLUME_CONTROL, 0, 0x7, 0, hpg_tlv),
	SOC_SINGLE_TLV("AK4331 Side Tone Volume",
			AK4331_17_SIDE_TONE_VOLUME_CONTROL, 5, 0x4, 1, sv_tlv),

	SOC_ENUM("AK4331 Digital Volume Control", ak4331_dac_enum[0]),
	SOC_ENUM("AK4331 DACL Signal Level", ak4331_dac_enum[1]),
	SOC_ENUM("AK4331 DACR Signal Level", ak4331_dac_enum[2]),
	SOC_ENUM("AK4331 DACL Signal Invert", ak4331_dac_enum[3]),
	SOC_ENUM("AK4331 DACR Signal Invert", ak4331_dac_enum[4]),
	SOC_ENUM("AK4331 Charge Pump Mode", ak4331_dac_enum[5]),
	SOC_ENUM("AK4331 HPL Power-down Resistor", ak4331_dac_enum[6]),
	SOC_ENUM("AK4331 HPR Power-down Resistor", ak4331_dac_enum[7]),
	SOC_ENUM("AK4331 DAC Digital Filter Mode", ak4331_dac_enum[8]),
	SOC_ENUM("AK4331 BICK Output Frequency", ak4331_dac_enum[9]),
	SOC_ENUM("AK4331 Soft Mute Cycle", ak4331_dac_enum[10]),

	SOC_ENUM_EXT("AK4331 BICK Frequency Select", ak4331_bitset_enum[0],
			get_bickfs, set_bickfs),
	SOC_ENUM_EXT("AK4331 SRC Output FS", ak4331_bitset_enum[1], get_srcfs,
			set_srcfs),
	SOC_ENUM_EXT("AK4331 MCKI/XTI Frequency", ak4331_bitset_enum[2],
			get_pllmcki, set_pllmcki),
	SOC_ENUM_EXT("AK4331 Xtal Mode", ak4331_bitset_enum[3], get_xtalmode,
			set_xtalmode),
	SOC_ENUM_EXT("AK4331 PLL Mode", ak4331_bitset_enum[4], get_pllmode,
			set_pllmode),

	SOC_ENUM_EXT("AK4331 SRC Mode", ak4331_bitset_enum[5], get_srcmode,
			set_srcmode),

	SOC_SINGLE("AK4331 Soft Mute Semi-Auto Mode",
			AK4331_09_JITTER_CLEANER_SETTING2, 1, 1, 0),
	SOC_SINGLE("AK4331 Soft Mute Control",
			AK4331_09_JITTER_CLEANER_SETTING2, 0, 1, 0),

	SOC_ENUM_EXT("AK4331 Soft Mute Enable", ak4331_smute_enum[0],
			get_smute_enable, set_smute_enable),

	SOC_ENUM("AK4331 Digital Mic HPF Cut Off Frequency",
			ak4331_dmic_enum[0]),
	SOC_ENUM("AK4331 Digital Mic Data at Clock Low", ak4331_dmic_enum[1]),
	SOC_ENUM("AK4331 Digital Mic Initial Cycle", ak4331_dmic_enum[2]),

	SOC_ENUM_EXT("AK4331 PDN Control", ak4331_pdn_enum[0], get_pdn,
			set_pdn),

#ifdef AK4331_DEBUG
	SOC_ENUM_EXT("AK4331 Reg Read", ak4331_enum[0], get_test_reg,
			set_test_reg),
#endif
};

/* DAC control */
static int ak4331_dac_event2(struct snd_soc_component *component, int event)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	int val, val2;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* before widget power up */
		ak4331->nDACOn = 1;
		val = ((ak4331->xtalmode == 1) ? 0x10 : 0);
		val |= ((ak4331->PLLMode > 0) ? 0x1 : 0);
		/* PMPSC, PMPLL */
		snd_soc_component_update_bits(component,
				AK4331_00_POWER_MANAGEMENT1, 0x11, val);
		if (ak4331->PLLMode > 0)
			msleep(2);

		/* PMTIM=1 */
		snd_soc_component_update_bits(component,
				AK4331_00_POWER_MANAGEMENT1, 0x02, 0x02);

		/* PMCP1=1 */
		snd_soc_component_update_bits(component,
				AK4331_01_POWER_MANAGEMENT2, 0x01, 0x01);
		/* wait 7ms */
		msleep(7);

		/* PMLDO1P/N=1 */
		snd_soc_component_update_bits(component,
				AK4331_01_POWER_MANAGEMENT2, 0x30, 0x30);
		/* wait 1ms */
		msleep(1);

		if (ak4331->selDain == 0) {
			val2 = 0x0;
			val = 0x0;
		} else {
			val2 = 0x20;
			val = 0x80;
		}
		/* DADFSSEL */
		snd_soc_component_update_bits(component,
				AK4331_06_DIGITAL_FILTER_SELECT, 0x20, val2);
		/* PMSRC */
		snd_soc_component_update_bits(component,
				AK4331_02_POWER_MANAGEMENT3, 0x80, val);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* after widget power up */
		/* PMCP2=1 */
		snd_soc_component_update_bits(component,
				AK4331_01_POWER_MANAGEMENT2, 0x02, 0x02);
		/* wait 5ms */
		msleep(5);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* before widget power down */
		/* PMCP2=0 */
		snd_soc_component_update_bits(component,
				AK4331_01_POWER_MANAGEMENT2, 0x02, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* after widget power down */
		/* PMSRC */
		snd_soc_component_update_bits(component,
				AK4331_02_POWER_MANAGEMENT3, 0x80, 0x00);
		/* PMLDO1P/N=0 */
		snd_soc_component_update_bits(component,
				AK4331_01_POWER_MANAGEMENT2, 0x30, 0x00);
		/* PMCP1=0 */
		snd_soc_component_update_bits(component,
				AK4331_01_POWER_MANAGEMENT2, 0x01, 0x00);
		/*PMTIM,PMPSC, PMPLL */
		snd_soc_component_update_bits(component,
				AK4331_00_POWER_MANAGEMENT1, 0x13, 0x00);
		ak4331->nDACOn = 0;
		break;
	}
	return 0;
}

static int ak4331_dac_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event) //CONFIG_LINF
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	ak4331_dac_event2(component, event);

	return 0;
}

/* HPL Mixer */
static const struct snd_kcontrol_new ak4331_hpl_mixer_controls[] = {
	SOC_DAPM_SINGLE("LDACL", AK4331_07_DAC_MONO_MIXING, 0, 1, 0),
	SOC_DAPM_SINGLE("RDACL", AK4331_07_DAC_MONO_MIXING, 1, 1, 0),
};

/* HPR Mixer */
static const struct snd_kcontrol_new ak4331_hpr_mixer_controls[] = {
	SOC_DAPM_SINGLE("LDACR", AK4331_07_DAC_MONO_MIXING, 4, 1, 0),
	SOC_DAPM_SINGLE("RDACR", AK4331_07_DAC_MONO_MIXING, 5, 1, 0),
};

/* DMIC Side Tone */
static const struct snd_kcontrol_new ak4331_sidetone_control =
	SOC_DAPM_SINGLE("Switch", AK4331_17_SIDE_TONE_VOLUME_CONTROL, 4, 1, 0);

/* DMIC Switch */
static const char * const ak4331_dmic_lch_select_texts[] = {
	"Off",
	"On"
};

static SOC_ENUM_SINGLE_VIRT_DECL(ak4331_dmic_lch_enum,
		ak4331_dmic_lch_select_texts);

static const struct snd_kcontrol_new ak4331_dmic_lch_mux_control =
	SOC_DAPM_ENUM("DMICL Switch", ak4331_dmic_lch_enum);

static const char * const ak4331_dmic_rch_select_texts[] = {
	"Off",
	"On"
};

static SOC_ENUM_SINGLE_VIRT_DECL(ak4331_dmic_rch_enum,
		ak4331_dmic_rch_select_texts);

static const struct snd_kcontrol_new ak4331_dmic_rch_mux_control =
	SOC_DAPM_ENUM("DMICR Switch", ak4331_dmic_rch_enum);

static const struct snd_soc_dapm_widget ak4331_dapm_widgets[] = {
	/* DAC */
	SND_SOC_DAPM_DAC_E("AK4331 DAC", NULL, AK4331_02_POWER_MANAGEMENT3,
			0, 0, ak4331_dac_event, (SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD)),

	/* Digital Input/Output */
	SND_SOC_DAPM_AIF_IN("AK4331 SDTI", "Playback", 0, SND_SOC_NOPM, 0, 0),

	/* Analog Output */
	SND_SOC_DAPM_OUTPUT("AK4331 HPL"),
	SND_SOC_DAPM_OUTPUT("AK4331 HPR"),

	SND_SOC_DAPM_MIXER("AK4331 HPR Mixer", AK4331_03_POWER_MANAGEMENT4,
			1, 0, &ak4331_hpr_mixer_controls[0],
			ARRAY_SIZE(ak4331_hpr_mixer_controls)),

	SND_SOC_DAPM_MIXER("AK4331 HPL Mixer", AK4331_03_POWER_MANAGEMENT4,
			0, 0, &ak4331_hpl_mixer_controls[0],
			ARRAY_SIZE(ak4331_hpl_mixer_controls)),

	/* Digital MIC */
	SND_SOC_DAPM_INPUT("AK4331 DMIC Input"),

	SND_SOC_DAPM_MUX("AK4331 DMIC Lch", AK4331_16_DIGITAL_MIC, 6, 0,
			&ak4331_dmic_lch_mux_control),
	SND_SOC_DAPM_MUX("AK4331 DMIC Rch", AK4331_16_DIGITAL_MIC, 7, 0,
			&ak4331_dmic_rch_mux_control),

	SND_SOC_DAPM_ADC("AK4331 DMIC", NULL, AK4331_16_DIGITAL_MIC, 5, 0),

	SND_SOC_DAPM_SWITCH("AK4331 Side Tone", SND_SOC_NOPM, 0, 0,
			&ak4331_sidetone_control),

	SND_SOC_DAPM_AIF_OUT("AK4331 SDOUT", "Capture", 0, SND_SOC_NOPM, 0, 0),

};

static const struct snd_soc_dapm_route ak4331_intercon[] = {
	{"AK4331 DAC", NULL, "AK4331 SDTI"},

	{"AK4331 HPL Mixer", "LDACL", "AK4331 DAC"},
	{"AK4331 HPL Mixer", "RDACL", "AK4331 DAC"},
	{"AK4331 HPR Mixer", "LDACR", "AK4331 DAC"},
	{"AK4331 HPR Mixer", "RDACR", "AK4331 DAC"},

	{"AK4331 HPL", NULL, "AK4331 HPL Mixer"},
	{"AK4331 HPR", NULL, "AK4331 HPR Mixer"},

	{"AK4331 DMIC Lch", "On", "AK4331 DMIC Input"},
	{"AK4331 DMIC Rch", "On", "AK4331 DMIC Input"},

	{"AK4331 DMIC", NULL, "AK4331 DMIC Lch"},
	{"AK4331 DMIC", NULL, "AK4331 DMIC Rch"},

	{"AK4331 SDOUT", NULL, "AK4331 DMIC"},

	{"AK4331 Side Tone", "Switch", "AK4331 DMIC"},
	{"AK4331 DAC", NULL, "AK4331 Side Tone"},
};

static int ak4331_set_mcki(struct snd_soc_component *component, int fs, int rclk)
{
	u8 mode;
	int mcki_rate;

	akdbgprt("\t[ak4331] %s fs=%d rclk=%d\n", __func__, fs, rclk);

	if (fs != 0 && rclk != 0) {
		if (rclk > 24576000)
			return -EINVAL;

		mcki_rate = rclk / fs;

		switch (mcki_rate) {
		case 128:
			mode = AK4331_CM_3;
			break;
		case 256:
			mode = AK4331_CM_0;
			break;
		case 512:
			mode = AK4331_CM_1;
			break;
		case 1024:
			mode = AK4331_CM_2;
			break;
		default:
			return -EINVAL;
		}
		snd_soc_component_update_bits(component,
				AK4331_05_CLOCK_MODE_SELECT, 0x60, mode);
	}

	return 0;
}

static int ak4331_set_src_mcki(struct snd_soc_component *component)
{
	struct ak4331_priv *ak4331 =
		snd_soc_component_get_drvdata(component);
	u8 rate = 0;
	int oclk_rate;
	int srcoutfs;

#ifdef SRC_OUT_FS_48K
	srcoutfs = 48000 * (1 << (ak4331->SRCOutFs));
#else
	srcoutfs = 44100 * (1 << (ak4331->SRCOutFs));
#endif

	switch (srcoutfs) {
	case 44100:
		rate |= AK4331_FS_44_1KHZ;
		break;
	case 48000:
		rate |= AK4331_FS_48KHZ;
		break;
	case 88200:
		rate |= AK4331_FS_88_2KHZ;
		break;
	case 96000:
		rate |= AK4331_FS_96KHZ;
		break;
	case 176400:
		rate |= AK4331_FS_176_4KHZ;
		break;
	case 192000:
		rate |= AK4331_FS_192KHZ;
		break;
	default:
		return -EINVAL;
	}

	oclk_rate = mcktab[ak4331->mckFreq]/srcoutfs;
	switch (oclk_rate) {
	case 128:
		rate |= AK4331_CM_3;
		break;
	case 256:
		rate |= AK4331_CM_0;
		break;
	case 512:
		rate |= AK4331_CM_1;
		break;
	case 1024:
		rate |= AK4331_CM_2;
		break;
	default:
		return -EINVAL;
	}

	akdbgprt("\t[ak4331] %s srcfs=%dHz, mck=%d\n",
			__func__, srcoutfs, mcktab[ak4331->mckFreq]);

	ak4331->fs2 = srcoutfs;
	snd_soc_component_update_bits(component,
			AK4331_08_JITTER_CLEANER_SETTING1, 0x7F, rate);

	return 0;
}

static int ak4331_set_pllblock(struct snd_soc_component *component, int fs)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	const int pllRefClockMin = 76800;
	const int pllRefClockMax = 768000;
	const int pllRefClockMid = 256000;
	const int pldMax = 65536;
	int pllClock, dacClk;
	int pllInFreq;
	int pldRefFreq;
	int mdiv, rest;
	int pls, plm, pld;
	int val;
	u8 mode;

	mode = snd_soc_component_read32(component, AK4331_05_CLOCK_MODE_SELECT);
	mode &= ~AK4331_CM;

	if (fs <= 24000) {
		mode |= AK4331_CM_1;
		dacClk = 512 * fs;
	} else if (fs <= 96000) {
		mode |= AK4331_CM_0;
		dacClk = 256 * fs;
	} else {
		mode |= AK4331_CM_3;
		dacClk = 128 * fs;
	}
	snd_soc_component_write(component, AK4331_05_CLOCK_MODE_SELECT, mode);

	if ((fs % 8000) == 0)
		pllClock = 24576000;
	else
		pllClock = 11289600;

	if (ak4331->PLLMode == 1) {
		/* BICK_PLL (Slave) */
		pls = 1;
		if (ak4331->bickFreq == 0) {
			/* 48fs */
			pllInFreq = 48 * fs;
		} else if (ak4331->bickFreq == 1) {
			/* 32fs */
			pllInFreq = 32 * fs;
		} else {
			pllInFreq = 64 * fs;
		}
	} else {
		/* MCKI PLL (Master) */
		pls = 0;
		pllInFreq = mcktab[ak4331->mckFreq];
	}

	mdiv = pllClock / dacClk;
	rest = (pllClock % dacClk);

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
		rest = (pldRefFreq % pld);
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

	akdbgprt("\t[ak4331] %s dacClk=%dHz, pllInFreq=%dHz, pldRefFreq=%dHz\n",
			__func__, dacClk, pllInFreq, pldRefFreq);
	akdbgprt("\t[ak4331] %s pld=%d, plm=%d mdiv=%d\n",
			__func__, pld, plm, mdiv);

	pld--;
	plm--;
	mdiv--;

	/* PLD15-0 */
	snd_soc_component_update_bits(component,
			AK4331_0F_PLL_REF_CLK_DIVIDER1,
			0xFF,
			((pld & 0xFF00) >> 8));
	snd_soc_component_update_bits(component,
			AK4331_10_PLL_REF_CLK_DIVIDER2,
			0xFF,
			((pld & 0x00FF) >> 0));

	/* PLM15-0 */
	snd_soc_component_update_bits(component,
			AK4331_11_PLL_FB_CLK_DIVIDER1,
			0xFF,
			((plm & 0xFF00) >> 8));
	snd_soc_component_update_bits(component,
			AK4331_12_PLL_FB_CLK_DIVIDER2,
			0xFF,
			((plm & 0x00FF) >> 0));

	snd_soc_component_update_bits(component,
			AK4331_14_DAC_CLK_DIVIDER, 0xFF, mdiv);

	val = ((pldRefFreq < pllRefClockMid) ? 0x10 : 0x00);
	val += pls;

	snd_soc_component_update_bits(component,
			AK4331_0E_PLL_CLK_SOURCE_SELECT, 0x11, val);

	return 0;
}

static int ak4331_set_timer(struct snd_soc_component *component)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	int count, tm, nfs;
	int lvdtm, vddtm, hptm;

	lvdtm = 0;
	vddtm = 0;
	hptm = 0;

	if (ak4331->selDain == 1)
		nfs = ak4331->fs2;
	else
		nfs = ak4331->fs1;

	/* LVDTM2-0 bits set */
	do {
		count = 1000 * (64 << lvdtm);
		tm = count / nfs;
		if (tm > LVDTM_HOLD_TIME)
			break;
		lvdtm++;
	} while (lvdtm < 7);
	/* LVDTM2-0 = 0~7 */
	snd_soc_component_update_bits(component, AK4331_03_POWER_MANAGEMENT4,
			0x70, (lvdtm << 4));

	/* VDDTM3-0 bits set */
	do {
		count = 1000 * (1024 << vddtm);
		tm = count / nfs;
		if (tm > VDDTM_HOLD_TIME)
			break;
		vddtm++;
	} while (vddtm < 8);
	/* VDDTM3-0 = 0~8 */
	snd_soc_component_update_bits(component, AK4331_04_OUTPUT_MODE_SETTING,
			0x3C, (vddtm << 2));

	/* HPTM2-0 bits set */
	do {
		count = 1000 * (128 << hptm);
		tm = count / nfs;
		if (tm > HPTM_HOLD_TIME)
			break;
		hptm++;
	} while (hptm < 4);
	/* HPTM2-0 = 0~4 */
	snd_soc_component_update_bits(component, AK4331_0D_HP_VOLUME_CONTROL,
			0xE0, (hptm << 5));

	return 0;
}

static int ak4331_hw_params_set(struct snd_soc_component *component, int nfs1)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	u8 fs;
	int val;

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	ak4331->fs1 = nfs1;

	switch (nfs1) {
	case 8000:
		fs = AK4331_FS_8KHZ;
		break;
	case 11025:
		fs = AK4331_FS_11_025KHZ;
		break;
	case 16000:
		fs = AK4331_FS_16KHZ;
		break;
	case 22050:
		fs = AK4331_FS_22_05KHZ;
		break;
	case 32000:
		fs = AK4331_FS_32KHZ;
		break;
	case 44100:
		fs = AK4331_FS_44_1KHZ;
		break;
	case 48000:
		fs = AK4331_FS_48KHZ;
		break;
	case 88200:
		fs = AK4331_FS_88_2KHZ;
		break;
	case 96000:
		fs = AK4331_FS_96KHZ;
		break;
	case 176400:
		fs = AK4331_FS_176_4KHZ;
		break;
	case 192000:
		fs = AK4331_FS_192KHZ;
		break;
	default:
		return -EINVAL;
	}
	snd_soc_component_update_bits(component, AK4331_05_CLOCK_MODE_SELECT,
			0x1F, fs);

	if (ak4331->PLLMode == 0) {
		/* Not PLL mode */
		ak4331_set_mcki(component, nfs1, ak4331->rclk);
	} else {
		/* PLL mode */
		ak4331_set_pllblock(component, nfs1);
	}

	if (ak4331->selDain == 1) {
		/* SRC mode */
		val = ((ak4331->PLLMode == 0) ? 0x1 : 0x0);
		snd_soc_component_update_bits(component,
				AK4331_13_SRC_CLK_SOURCE, 0x01, val);
		snd_soc_component_update_bits(component,
				AK4331_0A_JITTER_CLEANER_SETTING3, 0x42, 0x42);
		ak4331_set_src_mcki(component);
	} else {
		/* SRC Bypass mode */
		val = ((ak4331->PLLMode == 0) ? 0x1 : 0x0);
		snd_soc_component_update_bits(component,
				AK4331_13_SRC_CLK_SOURCE, 0x01, val);
		val <<= 6;
		snd_soc_component_update_bits(component,
				AK4331_0A_JITTER_CLEANER_SETTING3, 0x42, val);
		if (fs < 50000)
			snd_soc_component_update_bits(component,
					AK4331_29_MODE_COMTROL, 0x03, 0);
		else
			snd_soc_component_update_bits(component,
					AK4331_29_MODE_COMTROL, 0x03, 3);
	}

	ak4331_set_timer(component);

	return 0;
}

static int ak4331_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	ak4331->fs1 = params_rate(params);

	ak4331_hw_params_set(component, ak4331->fs1);

	return 0;
}

static int ak4331_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	struct snd_soc_component *component = dai->component;
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	ak4331_pdn_control(component, 1);

	ak4331->rclk = freq;

	if (ak4331->PLLMode == 0) {
		/* Not PLL mode */
		ak4331_set_mcki(component, ak4331->fs1, freq);
	}

	return 0;
}

static int ak4331_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u8 format;

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	ak4331_pdn_control(component, 1);

	/* set master/slave audio interface */
	format = snd_soc_component_read32(component, AK4331_15_AUDIO_IF_FORMAT);
	format &= ~AK4331_DIF;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		format |= AK4331_SLAVE_MODE;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		format |= AK4331_MASTER_MODE;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(component->dev, "Clock mode unsupported");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format |= AK4331_DIF_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format |= AK4331_DIF_MSB_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* set format */
	snd_soc_component_write(component, AK4331_15_AUDIO_IF_FORMAT, format);

	return 0;
}

static int ak4331_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *codec_dai)
{
	int ret = 0;

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	return ret;
}

static enum snd_soc_bias_level ak4331_get_bias_level(
		struct snd_soc_component *component)
{
	return snd_soc_component_get_bias_level(component);
}

static int ak4331_set_bias_level(struct snd_soc_component *component,
		enum snd_soc_bias_level level)
{
	int level1 = level;
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);

	akdbgprt("\t[ak4331] %s(%d) level=%d\n",
			__func__, __LINE__, level1);
	akdbgprt("\t[AK4331] %s(%d) ak4331_get_bias_level=%d\n",
			__func__, __LINE__, ak4331_get_bias_level(component));

	switch (level1) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		if (ak4331_get_bias_level(component) == SND_SOC_BIAS_STANDBY)
			akdbgprt("\t[AK4331] %s(%d) codec->dapm.bias_level == SND_SOC_BIAS_STANDBY\n",
					__func__, __LINE__);
		if (ak4331_get_bias_level(component) == SND_SOC_BIAS_ON)
			akdbgprt("\t[AK4331] %s(%d) codec->dapm.bias_level >= SND_SOC_BIAS_ON\n",
					__func__, __LINE__);
		break;
	case SND_SOC_BIAS_STANDBY:
		if (ak4331_get_bias_level(component) == SND_SOC_BIAS_PREPARE)
			akdbgprt("\t[AK4331] %s(%d) codec->dapm.bias_level == SND_SOC_BIAS_PREPARE\n",
					__func__, __LINE__);
		if (ak4331_get_bias_level(component) == SND_SOC_BIAS_OFF)
			akdbgprt("\t[AK4331] %s(%d) codec->dapm.bias_level == SND_SOC_BIAS_OFF\n",
					__func__, __LINE__);
		break;
	case SND_SOC_BIAS_OFF:
		if (ak4331_get_bias_level(component) == SND_SOC_BIAS_STANDBY)
			akdbgprt("\t[AK4331] %s(%d) codec->dapm.bias_level == SND_SOC_BIAS_STANDBY\n",
					__func__, __LINE__);
		break;
	}

	dapm->bias_level = level;

	return 0;
}

static int ak4331_set_dai_mute2(struct snd_soc_component *component, int mute)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	int ret = 0;
	int nfs, ndt, ndt2;
	int nSmt;

	nSmt = snd_soc_component_read32(component,
			AK4331_09_JITTER_CLEANER_SETTING2);
	nSmt &= 0xC;
	nSmt >>= 2;

	if (ak4331->selDain == 1)
		nfs = ak4331->fs2;
	else
		nfs = ak4331->fs1;

	akdbgprt("\t[ak4331] %s mute[%s]\n", __func__, mute ? "ON" : "OFF");

	if (mute) {
		/* SMUTE: 1, MUTE */
		if (ak4331->pmsm) {
			ret = snd_soc_component_update_bits(component,
					AK4331_09_JITTER_CLEANER_SETTING2,
					0x01, 0x01);
			ndt = (1024000 << nSmt) / nfs;
			msleep(ndt);
		}
	} else {
		/* SMUTE: 0, NORMAL operation */
		if (ak4331->pmsm) {
			ndt2 = (1024000 << nSmt) / nfs;
			ndt -= ndt2;
			if (ndt < 4)
				ndt = 4;
			msleep(ndt);
			ret = snd_soc_component_update_bits(component,
					AK4331_09_JITTER_CLEANER_SETTING2,
					0x01, 0x00);
			msleep(ndt2);
		}
	}
	return ret;
}

static int ak4331_set_dai_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_component *component = dai->component;

	ak4331_set_dai_mute2(component, mute);

	return 0;
}

#define AK4331_RATES		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
				SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
				SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
				SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400 |\
				SNDRV_PCM_RATE_192000)

#define AK4331_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
				SNDRV_PCM_FMTBIT_S24_LE |\
				SNDRV_PCM_FMTBIT_S32_LE)


static struct snd_soc_dai_ops ak4331_dai_ops = {
	.hw_params = ak4331_hw_params,
	.set_sysclk = ak4331_set_dai_sysclk,
	.set_fmt = ak4331_set_dai_fmt,
	.trigger = ak4331_trigger,
	.digital_mute = ak4331_set_dai_mute,
};

struct snd_soc_dai_driver ak4331_dai[] = {
	{
		.name = "ak4331-AIF1",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AK4331_RATES,
			.formats = AK4331_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AK4331_RATES,
			.formats = AK4331_FORMATS,
		},
		.ops = &ak4331_dai_ops,
	},
};

static int ak4331_init_reg(struct snd_soc_component *component)
{
	u8 DeviceID;
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4331] %s(%d)\n", __func__, __LINE__);

	ak4331_pdn_control(component, 1);

	DeviceID = ak4331_i2c_read(component, AK4331_15_AUDIO_IF_FORMAT);

	switch (DeviceID >> 5) {
	case 0:
		/* 0:AK4375 */
		ak4331->nDeviceID = 0;
		pr_info("AK4375 is connecting.\n");
		break;
	case 1:
		/* 1:AK4375A */
		ak4331->nDeviceID = 1;
		pr_info("AK4375A is connecting.\n");
		break;
	case 2:
		/* 2:AK4376 */
		ak4331->nDeviceID = 2;
		pr_info("AK4376 is connecting.\n");
		break;
	case 3:
		/* 3:AK4377 */
		ak4331->nDeviceID = 3;
		pr_info("AK4377 is connecting.\n");
		break;
	case 7:
		/* 3:AK4331 */
		ak4331->nDeviceID = 7;
		pr_info("AK4331 is connecting.\n");
		break;
	default:
		/* 4:Other IC */
		ak4331->nDeviceID = 4;
		pr_err("AK43XX DAC is not connected.\n");
	}

	return 0;
}

static int ak4331_parse_dt(struct ak4331_priv *ak4331)
{
	struct device *dev;
	struct device_node *np;

	dev = &(ak4331->i2c->dev);

	np = dev->of_node;

	if (!np)
		return -EINVAL;

	pr_info("Read PDN pin from device tree\n");

	ak4331->priv_pdn_en = of_get_named_gpio(np, "ak4331,pdn-gpio", 0);
	if (ak4331->priv_pdn_en < 0) {
		ak4331->priv_pdn_en = -1;
		return -EINVAL;
	}

	if (!gpio_is_valid(ak4331->priv_pdn_en)) {
		pr_err("AK4331 pdn pin(%u) is invalid\n",
				ak4331->priv_pdn_en);
		return -EINVAL;
	}

	return 0;
}

static int ak4331_set_pinctrl(struct ak4331_priv *ak4331)
{
	struct device *dev;
	struct device_node *np;
	int ret;

	dev = &(ak4331->i2c->dev);

	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ak4331->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(ak4331->pinctrl)) {
		dev_err(dev, "failed to look up pinctrl");
		return -EINVAL;
	}

	ak4331->pin_default = pinctrl_lookup_state(ak4331->pinctrl,
			"default");
	if (IS_ERR_OR_NULL(ak4331->pin_default)) {
		dev_err(dev, "failed to look up default pin state");
		devm_pinctrl_put(ak4331->pinctrl);
		return -EINVAL;
	}

	ret = pinctrl_select_state(ak4331->pinctrl,
			ak4331->pin_default);
	if (ret != 0) {
		dev_err(dev, "Failed to set default pin state\n");
		devm_pinctrl_put(ak4331->pinctrl);
		return ret;
	}

	return 0;
}

static void ak4331_hp_det_report(struct work_struct *work)
{
	int value;
	struct delayed_work *dwork;
	struct ak4331_priv *ak4331;

	dwork = to_delayed_work(work);
	ak4331 = container_of(dwork, struct ak4331_priv, hp_det_dwork);

	if (gpio_is_valid(ak4331->hp_irq_gpio)) {
		value = gpio_get_value(ak4331->hp_irq_gpio);
		akdbgprt("%s :gpio value = %d\n", __func__, value);
		snd_soc_jack_report(&ak4331->jack, value ? 0 :
				SND_JACK_HEADPHONE, AK4331_JACK_MASK);
	}
}

static irqreturn_t ak4331_headset_det_irq_thread(int irq, void *data)
{
	struct ak4331_priv *ak4331;

	ak4331 = (struct ak4331_priv *)data;
	cancel_delayed_work_sync(&ak4331->hp_det_dwork);
	schedule_delayed_work(&ak4331->hp_det_dwork, msecs_to_jiffies(500));

	return IRQ_HANDLED;
}

static int ak4331_parse_dt_hp_det(struct ak4331_priv *ak4331)
{
	struct device *dev;
	struct device_node *np;
	int rc;

	dev = &(ak4331->i2c->dev);
	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ak4331->hp_irq_gpio = of_get_named_gpio(np,
						"ak4331,headset-det-gpio", 0);

	rc = gpio_request(ak4331->hp_irq_gpio, "AK4331 hp-det");
	if (rc) {
		dev_err(dev, "\t[AK4331] %s(%d) AK4331 hp-det gpio request fail\n",
				__func__, __LINE__);
		ak4331->hp_irq_gpio = -1;
		return rc;
	}

	rc = gpio_direction_input(ak4331->hp_irq_gpio);
	if (rc) {
		akdbgprt("\t[AK4331] %s(%d) hp-det gpiog input fail\n",
				__func__, __LINE__);
		ak4331->hp_irq_gpio = -1;
		gpio_free(ak4331->hp_irq_gpio);
		return -EINVAL;
	}

	if (!gpio_is_valid(ak4331->hp_irq_gpio)) {
		dev_err(dev, "AK4331 pdn pin(%u) is invalid\n",
						ak4331->hp_irq_gpio);
		ak4331->hp_irq_gpio = -1;
		gpio_free(ak4331->hp_irq_gpio);
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&ak4331->hp_det_dwork, ak4331_hp_det_report);
	rc = snd_soc_card_jack_new(ak4331->card, "AK4331 Headphone",
		AK4331_JACK_MASK, &ak4331->jack, NULL, 0);
	if (rc < 0) {
		dev_err(dev, "Cannot create jack\n");
		goto error;
	}

	ak4331->hp_irq = gpio_to_irq(ak4331->hp_irq_gpio);
	rc = request_threaded_irq(ak4331->hp_irq, NULL,
		ak4331_headset_det_irq_thread,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"ak4331", ak4331);
	if (rc != 0) {
		dev_err(dev, "Failed to request IRQ %d: %d\n",
			ak4331->hp_irq, rc);
		goto error;
	}

	rc = enable_irq_wake(ak4331->hp_irq);
	if (rc) {
		dev_err(dev,
			"Failed to set wake interrupt on IRQ %d: %d\n",
			ak4331->hp_irq, rc);
		free_irq(ak4331->hp_irq, ak4331);
		goto error;
	}

	schedule_delayed_work(&ak4331->hp_det_dwork, msecs_to_jiffies(200));

error:
	return rc;
}


static int ak4331_probe(struct snd_soc_component *component)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	int ret = 0;
	struct device *dev = &(ak4331->i2c->dev);

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	ak4331->card = component->card;

	if (ak4331->avdd_voltage != 0) {
		ret = regulator_set_voltage(ak4331->avdd_core,
			ak4331->avdd_voltage, ak4331->avdd_voltage);
		if (ret < 0) {
			dev_err(dev, "AVDD regulator_set_voltage failed\n");
			return ret;
		}
	}

	if (ak4331->avdd_current != 0) {
		ret = regulator_set_load(ak4331->avdd_core,
			ak4331->avdd_current);
		if (ret < 0) {
			dev_err(dev, "AVDD regulator_set_load failed\n");
			return ret;
		}
	}

	ret = regulator_enable(ak4331->avdd_core);
	if (ret < 0) {
		dev_err(dev, "AVDD regulator_enable failed\n");
		return ret;
	}

	ret = ak4331_parse_dt(ak4331);
	akdbgprt("\t[AK4331] %s(%d) ret=%d\n", __func__, __LINE__, ret);
	if (ret) {
		/* No use GPIO control */
		ak4331->pdn1 = 2;
		goto parse_hp_detect;
	}

	ret = gpio_request(ak4331->priv_pdn_en, "AK4331 pdn");
	akdbgprt("\t[AK4331] %s : gpio_request ret = %d\n",
			__func__, ret);
	if (ret) {
		akdbgprt("\t[AK4331] %s(%d) cannot get AK4331 pdn gpio\n",
				__func__, __LINE__);
		/* No use GPIO control */
		ak4331->pdn1 = 2;
		goto parse_hp_detect;
	}

	ret = ak4331_set_pinctrl(ak4331);
	if (ret) {
		akdbgprt("\t[AK4331] %s(%d) set_pinctrl ret=%d\n",
				__func__, __LINE__, ret);
		gpio_free(ak4331->priv_pdn_en);
		ak4331->pdn1 = 2;
		goto parse_hp_detect;
	}

	ret = gpio_direction_output(ak4331->priv_pdn_en, 0);
	if (ret) {
		akdbgprt("\t[AK4331] %s(%d) pdn_en=0 fail\n",
				__func__, __LINE__);
		gpio_free(ak4331->priv_pdn_en);
		ak4331->pdn1 = 2;
		goto parse_hp_detect;
	}

	akdbgprt("\t[AK4331] %s(%d) pdn_en=0\n",
			__func__, __LINE__);

parse_hp_detect:
	ret = ak4331_parse_dt_hp_det(ak4331);
	if (ret != 0) {
		akdbgprt("\t[AK4331] %s(%d) cannot setup headphone detection\n",
				__func__, __LINE__);
		regulator_disable(ak4331->avdd_core);
		return ret;
	}

	ak4331->fs1 = 48000;
	ak4331->fs2 = 48000;

	ak4331_init_reg(component);

	ak4331->rclk = 0;

	/* 0:Bypass, 1:SRC (Dependent on a register bit) */
	ak4331->selDain = 0;
	/* 0:48fs, 1:32fs, 2:64fs */
	ak4331->bickFreq = 0;
	/* 0:48(44.1)kHz, 1:96(88.2)kHz, 2:192(176.4)kHz */
	ak4331->SRCOutFs = 0;
	/* 0:PLL OFF, 1:BICK PLL (Slave), 2:MCKI PLL (Master) */
	ak4331->PLLMode = 0;
	/* 0:9.6MHz, 1:11.2896MHz, 2:12.288MHz, 3:19.2MHz */
	ak4331->mckFreq = 0;
	ak4331->xtalmode = 0;
	ak4331->pmsm = 0;

	return ret;
}

static void ak4331_remove(struct snd_soc_component *component)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	struct device *dev = &(ak4331->i2c->dev);
	int ret;
	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	ak4331_set_bias_level(component, SND_SOC_BIAS_OFF);
	ak4331_pdn_control(component, 0);
	ret = regulator_disable(ak4331->avdd_core);
	if (ret < 0)
		dev_err(dev, "AVDD regulator_disable failed\n");
}

static int ak4331_suspend(struct snd_soc_component *component)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	struct device *dev = &(ak4331->i2c->dev);
	int ret;
	akdbgprt("\t[AK4331] %s(%d)\n", __func__, __LINE__);

	ak4331_set_bias_level(component, SND_SOC_BIAS_OFF);
	ak4331_pdn_control(component, 0);
	ret = regulator_disable(ak4331->avdd_core);
	if (ret < 0) {
		dev_err(dev, "AVDD regulator_disable failed\n");
		return ret;
	}

	return 0;
}

static int ak4331_resume(struct snd_soc_component *component)
{
	struct ak4331_priv *ak4331 = snd_soc_component_get_drvdata(component);
	struct device *dev = &(ak4331->i2c->dev);
	int ret;
	akdbgprt("\t[AK4331] %s(%d)\n", __func__, __LINE__);

	if (ak4331->avdd_voltage != 0) {
		ret = regulator_set_voltage(ak4331->avdd_core,
			ak4331->avdd_voltage, ak4331->avdd_voltage);
		if (ret < 0) {
			dev_err(dev, "AVDD regulator_set_voltage failed\n");
			return ret;
		}
	}

	if (ak4331->avdd_current != 0) {
		ret = regulator_set_load(ak4331->avdd_core,
			ak4331->avdd_current);
		if (ret < 0) {
			dev_err(dev, "AVDD regulator_set_load failed\n");
			return ret;
		}
	}

	ret = regulator_enable(ak4331->avdd_core);
	if (ret < 0) {
		dev_err(dev, "AVDD regulator_enable failed\n");
		return ret;
	}

	ak4331_init_reg(component);

	return 0;
}


struct snd_soc_component_driver soc_component_dev_ak4331 = {
	.name = "ak4331",
	.probe = ak4331_probe,
	.remove = ak4331_remove,
	.suspend = ak4331_suspend,
	.resume = ak4331_resume,
	.idle_bias_on = false,
	.set_bias_level = ak4331_set_bias_level,
	.controls = ak4331_snd_controls,
	.num_controls = ARRAY_SIZE(ak4331_snd_controls),
	.dapm_widgets = ak4331_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak4331_dapm_widgets),
	.dapm_routes = ak4331_intercon,
	.num_dapm_routes = ARRAY_SIZE(ak4331_intercon),
};
EXPORT_SYMBOL_GPL(soc_component_dev_ak4331);

static const struct regmap_config ak4331_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AK4331_MAX_REGISTER,
	.readable_reg = ak4331_readable,
	.volatile_reg = ak4331_volatile,
	.writeable_reg = ak4331_writeable,

	.reg_defaults = ak4331_reg,
	.num_reg_defaults = ARRAY_SIZE(ak4331_reg),

	.cache_type = REGCACHE_RBTREE,
};

static const struct of_device_id ak4331_i2c_dt_ids[] = {
	{ .compatible = "akm,ak4331"},
	{}
};
MODULE_DEVICE_TABLE(of, ak4331_i2c_dt_ids);

static int ak4331_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct ak4331_priv *ak4331;
	struct device *dev;
	struct device_node *np;
	int ret = 0;

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	dev = &i2c->dev;
	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ak4331 = devm_kzalloc(&i2c->dev, sizeof(struct ak4331_priv),
			GFP_KERNEL);
	if (ak4331 == NULL)
		return -ENOMEM;

	ak4331->avdd_core = devm_regulator_get(&i2c->dev, "ak4331,avdd-core");
	if (IS_ERR(ak4331->avdd_core)) {
		ret = PTR_ERR(ak4331->avdd_core);
		dev_err(&i2c->dev, "Unable to get avdd-core:%d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "ak4331,avdd-voltage",
				&ak4331->avdd_voltage);
	if (ret) {
		dev_err(&i2c->dev,
			"%s:Looking up %s property in node %s failed\n",
			__func__, "ak4331,avdd-voltage",
			np->full_name);
	}
	ret = of_property_read_u32(np, "ak4331,avdd-current",
				&ak4331->avdd_current);
	if (ret) {
		dev_err(&i2c->dev,
			"%s:Looking up %s property in node %s failed\n",
			__func__, "ak4331,avdd-current",
			np->full_name);
	}

	ak4331->regmap = devm_regmap_init_i2c(i2c, &ak4331_regmap);
	if (IS_ERR(ak4331->regmap)) {
		akdbgprt("[*****AK4331*****] %s regmap error\n", __func__);
		devm_kfree(&i2c->dev, ak4331);
		return PTR_ERR(ak4331->regmap);
	}

#ifdef CONFIG_DEBUG_FS_CODEC
	ret = device_create_file(&i2c->dev, &dev_attr_reg_data);
	if (ret)
		pr_err("%s: Error to create reg_data\n", __func__);
#endif

	i2c_set_clientdata(i2c, ak4331);

	ak4331->i2c = i2c;
	ak4331->pdn1 = 0;
	ak4331->pdn2 = 0;
	ak4331->priv_pdn_en = 0;

	ret = snd_soc_register_component(&i2c->dev, &soc_component_dev_ak4331,
			&ak4331_dai[0], ARRAY_SIZE(ak4331_dai));
	if (ret < 0) {
		devm_kfree(&i2c->dev, ak4331);
		akdbgprt("\t[ak4331 Error!] %s(%d)\n", __func__, __LINE__);
	}

	akdbgprt("\t[ak4331] %s(%d) pdn1=%d\n, ret=%d",
			__func__, __LINE__, ak4331->pdn1, ret);

	return ret;
}

static int ak4331_i2c_remove(struct i2c_client *client)
{
	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

#ifdef CONFIG_DEBUG_FS_CODEC
	device_remove_file(&client->dev, &dev_attr_reg_data);
#endif

	snd_soc_unregister_component(&client->dev);
	return 0;
}

static const struct i2c_device_id ak4331_i2c_id[] = {
	{ "ak4331", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ak4331_i2c_id);

static struct i2c_driver ak4331_i2c_driver = {
	.driver = {
		.name = "ak4331",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak4331_i2c_dt_ids),
	},
	.probe = ak4331_i2c_probe,
	.remove = ak4331_i2c_remove,
	.id_table = ak4331_i2c_id,
};

static int __init ak4331_modinit(void)
{

	akdbgprt("\t[ak4331] %s(%d)\n", __func__, __LINE__);

	return i2c_add_driver(&ak4331_i2c_driver);
}

module_init(ak4331_modinit);

static void __exit ak4331_exit(void)
{
	i2c_del_driver(&ak4331_i2c_driver);
}
module_exit(ak4331_exit);

MODULE_DESCRIPTION("ASoC ak4331 codec driver");
MODULE_LICENSE("GPL v2");
