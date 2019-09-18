/*
 * max98927.c  --  MAX98927 ALSA Soc Audio driver
 *
 * Copyright 2013-15 Maxim Integrated Products
 * Author: Ryan Lee <ryans.lee@maximintegrated.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/tlv.h>
#include "max98927.h"

static struct reg_default max98927_reg_map[] = {
/* modified */
	{0x0014,  0x78},	/* Meas ADC Thermal Warning Threshold, 124.6 degrees */
	{0x0015,  0xFF},	/* Meas ADC Thermal Shutdown Threshold, 150 degrees */
	{0x0043,  0x04},	/* Meas ADC Config, measurement input CH2 (Temperature) enable when ADC in automatic mode */
	{0x0017,  0x55},	/* Pin Config, ICC, LRCLK, BCLK, DOUT drive strength control normal driver mode */
	/* For mono driver we are just enabling one channel*/
	{0x0018,  0x03},	/* PCM Rx Enables A, PCM RX channel 0, 1 enabled */
	{0x001c,  0x00},	/* PCM Tx HiZ Control A, CH2-7 set HiZ on channel */
	{0x001d,  0x00},	/* PCM Tx HiZ Control B, CH8-15 set HiZ on channel */
	{0x001e,  0x10},	/* PCM Tx Channel Sources A, measure I PCM channel 0, measure V PCM channel 1 */
	{0x001f,  0x00},	/* PCM Tx Channel Sources B, I/V data is not interleaved, PCM channel 1 for amplifier to DAI feedback path */
	{0x003f,  0x07},	/* Measurement DSP Config, 3.7Hz, all enabled */
	{0x0025,  0x80},	/* PCM to Speaker Monomix A, output of monomix is (channel0 + channel1) / 2, monomix source select PCM RX channel 0 */
	{0x0026,  0x01},	/* PCM to Speaker Monomix B, monomix source select channel 1 gets PCM RX channel 1 */
	{0x0035,  0x00},	/* PDM Rx Enable, disabled */
	{0x0036,  0x40},	/* AMP volume control, set digital volume level output amplifier 0dB */
	{0x0037,  0x03},	/* AMP DSP Config, ??? */
	{0x0039,  0x01},	/* DRE Dynamic Range Enhancement Control, enabled */
	{0x003c,  0x01},	/* Speaker Gain, analog gain applied at speaker amplifier when using PDM / PCM interface, +12dB */
	{0x003d,  0x85},	/* SSM Configurarion, Spread-Spectrum clocking disable, speaker driver bias default */
	{0x0040,  0x00},	/* Boost Control 0, nominal output from the boost converter 8.5V */
	{0x0042,  0x3e},	/* Boost Control 1, peak input current limit for boost converter 4.1A */
	{0x0044,  0x00},	/* Meas ADC Base Divide MSByte 12.288 / 48KHz = 256 = 0xff */
	{0x0045,  0xff},	/* Meas ADC Base Divide LSByte 12.288 / 36 = 341.3KHz */
	{0x007f,  0x0e},	/* Brownout level 4 amp 1 control 1, -6dBFS */
	{0x0087,  0x1c},	/* Envelope Tracker Boost Vout ReadBack, 10.0V */
	{0x0089,  0x03},	/* ??? */
	{0x009f,  0x01},	/* ??? */
	{0x0020,  0x40},
/* added */
	{0x0005,  0xc4},
	{0x001a,  0x03},
	{0x0022,  0x44},
	{0x0023,  0x08},
	{0x0024,  0x88},
	{0x0034,  0x00},
              {0x003a,  0x01},
	{0x003e,  0x03},
	{0x003f,  0x07},
	{0x0041,  0x01},
	{0x0043,  0x04},
	{0x004e,  0x2a},
	{0x0082,  0x18},
	{0x00ff,  0x01},
};

void max98927_wrapper_write(struct max98927_priv *max98927,
	unsigned int reg, unsigned int val)
{
	if (max98927->regmap)
		regmap_write(max98927->regmap, reg, val);
	if (max98927->sub_regmap)
		regmap_write(max98927->sub_regmap, reg, val);
}

void max98927_wrap_update_bits(struct max98927_priv *max98927,
	unsigned int reg, unsigned int mask, unsigned int val)
{
	if (max98927->regmap)
		regmap_update_bits(max98927->regmap, reg, mask, val);
	if (max98927->sub_regmap)
		regmap_update_bits(max98927->sub_regmap, reg, mask, val);
}

static int max98927_reg_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data;

	regmap_read(max98927->regmap, reg, &data);
	ucontrol->value.integer.value[0] =
		(data & mask) >> shift;
	return 0;
}

static int max98927_reg_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	max98927_wrap_update_bits(max98927, reg, mask, sel << shift);
	dev_dbg(codec->dev, "%s: register 0x%02X, value 0x%02X\n",
				__func__, reg, sel);
	return 0;
}

static int max98927_dai_set_fmt(struct snd_soc_dai *codec_dai,
	unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int invert = 0;

	dev_dbg(codec->dev, "%s: fmt 0x%08X\n", __func__, fmt);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
		MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
		MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_SLAVE);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		max98927->master = true;
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
		MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
		MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_MASTER);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
		MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
		MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_HYBRID);
	default:
		dev_err(codec->dev, "DAI clock mode unsupported");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		invert = MAX98927_PCM_Mode_Config_PCM_BCLKEDGE;
		break;
	default:
		dev_err(codec->dev, "DAI invert mode unsupported");
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		max98927->iface |= SND_SOC_DAIFMT_I2S;
		max98927_wrap_update_bits(max98927,
			MAX98927_PCM_Mode_Config,
		max98927->iface, max98927->iface);
	break;
	case SND_SOC_DAIFMT_LEFT_J:
		max98927->iface |= SND_SOC_DAIFMT_LEFT_J;
		max98927_wrap_update_bits(max98927,
			MAX98927_PCM_Mode_Config,
		max98927->iface, max98927->iface);
	break;
	default:
		return -EINVAL;
	}

	/* pcm channel configuration */
	if (max98927->iface & (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_LEFT_J)) {
		max98927_wrapper_write(max98927,
			MAX98927_PCM_Rx_Enables_A,
			MAX98927_PCM_Rx_Enables_A_PCM_RX_CH0_EN|
			MAX98927_PCM_Rx_Enables_A_PCM_RX_CH1_EN);
	}
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
		MAX98927_PCM_Mode_Config_PCM_BCLKEDGE, invert);
	return 0;
}

/* codec MCLK rate in master mode */
static const int rate_table[] = {
	5644800, 6000000, 6144000, 6500000,
	9600000, 11289600, 12000000, 12288000,
	13000000, 19200000,
};

static int max98927_set_clock(struct max98927_priv *max98927,
	struct snd_pcm_hw_params *params)
{
	/* BCLK/LRCLK ratio calculation */
	int blr_clk_ratio = params_channels(params) * max98927->ch_size;
	int reg = MAX98927_PCM_Clock_setup;
	int mask = MAX98927_PCM_Clock_setup_PCM_BSEL_Mask;
	int value;

	if (max98927->master) {
		int i;
		/* match rate to closest value */
		for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
			if (rate_table[i] >= max98927->sysclk)
				break;
		}
		if (i == ARRAY_SIZE(rate_table)) {
			pr_err("%s couldn't get the MCLK to match codec\n",
				__func__);
			return -EINVAL;
		}
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
			MAX98927_PCM_Master_Mode_PCM_MCLK_RATE_Mask,
			i << MAX98927_PCM_Master_Mode_PCM_MCLK_RATE_SHIFT);
	}

	switch (blr_clk_ratio) {
	case 32:
		value = 2;
		break;
	case 48:
		value = 3;
		break;
	case 64:
		value = 4;
		break;
	default:
		return -EINVAL;
	}
	max98927_wrap_update_bits(max98927,
	reg, mask, value);
	return 0;
}

static int max98927_dai_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int sampling_rate = 0;

	/* pcm mode configuration */
	switch (snd_pcm_format_width(params_format(params))) {
	case 16:
		max98927_wrap_update_bits(max98927,
			MAX98927_PCM_Mode_Config,
			MAX98927_PCM_Mode_Config_PCM_CHANSZ_16,
			MAX98927_PCM_Mode_Config_PCM_CHANSZ_16);
		max98927->ch_size = 16;
		break;
	case 24:
		max98927_wrap_update_bits(max98927,
			MAX98927_PCM_Mode_Config,
			MAX98927_PCM_Mode_Config_PCM_CHANSZ_24,
			MAX98927_PCM_Mode_Config_PCM_CHANSZ_24);
		max98927->ch_size = 24;
		break;
	case 32:
		max98927_wrap_update_bits(max98927,
			MAX98927_PCM_Mode_Config,
			MAX98927_PCM_Mode_Config_PCM_CHANSZ_32,
			MAX98927_PCM_Mode_Config_PCM_CHANSZ_32);
		max98927->ch_size = 32;
		break;
	default:
		pr_err("%s: format unsupported %d",
			__func__, params_format(params));
		goto err;
	}
	dev_dbg(codec->dev, "%s: format supported %d",
		__func__, params_format(params));

	/* sampling rate configuration */
	switch (params_rate(params)) {
	case 8000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_8000;
		break;
	case 11025:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_11025;
		break;
	case 12000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_12000;
		break;
	case 16000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_16000;
		break;
	case 22050:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_22050;
		break;
	case 24000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_24000;
		break;
	case 32000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_32000;
		break;
	case 44100:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_44100;
		break;
	case 48000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_48000;
		break;
	default:
		pr_err("%s rate %d not supported\n",
			__func__, params_rate(params));
		goto err;
	}
	/* set DAI_SR to correct LRCLK frequency */
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_1,
		MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_Mask, sampling_rate);
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_2,
		MAX98927_PCM_Sample_rate_setup_2_SPK_SR_Mask, sampling_rate<<4);
	if (max98927->interleave_mode &&
	    sampling_rate > MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_16000)
		max98927_wrap_update_bits(max98927,
			MAX98927_PCM_Sample_rate_setup_2,
			MAX98927_PCM_Sample_rate_setup_2_IVADC_SR_Mask,
			sampling_rate - 3);
	else
		max98927_wrap_update_bits(max98927,
			MAX98927_PCM_Sample_rate_setup_2,
			MAX98927_PCM_Sample_rate_setup_2_IVADC_SR_Mask,
			sampling_rate);

	return max98927_set_clock(max98927, params);
err:
	return -EINVAL;
}

#define MAX98927_RATES SNDRV_PCM_RATE_8000_48000

#define MAX98927_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static int max98927_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	max98927->sysclk = freq;
	return 0;
}

static const struct snd_soc_dai_ops max98927_dai_ops = {
	.set_sysclk = max98927_dai_set_sysclk,
	.set_fmt = max98927_dai_set_fmt,
	.hw_params = max98927_dai_hw_params,
};

static void max98927_handle_pdata(struct snd_soc_codec *codec)
{
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	struct max98927_reg_default *regInfo;
	int cfg_size = 0;
	int x;

	if (max98927->regcfg != NULL)
		cfg_size = max98927->regcfg_sz / sizeof(uint32_t);

	if (cfg_size <= 0) {
		dev_dbg(codec->dev,
			"Register configuration is not required.\n");
		return;
	}

	/* direct configuration from device tree */
	for (x = 0; x < cfg_size; x += 3) {
		regInfo = (struct max98927_reg_default *)&max98927->regcfg[x];
		dev_info(codec->dev, "CH:%d, reg:0x%02x, value:0x%02x\n",
			be32_to_cpu(regInfo->ch),
			be32_to_cpu(regInfo->reg),
			be32_to_cpu(regInfo->def));
		if (be32_to_cpu(regInfo->ch) == PRI_MAX98927
			&& max98927->regmap)
			regmap_write(max98927->regmap,
				be32_to_cpu(regInfo->reg),
				be32_to_cpu(regInfo->def));
		else if (be32_to_cpu(regInfo->ch) == SEC_MAX98927
			&& max98927->sub_regmap)
			regmap_write(max98927->sub_regmap,
				be32_to_cpu(regInfo->reg),
				be32_to_cpu(regInfo->def));
	}
}

static int max98927_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		max98927_wrap_update_bits(max98927,
			MAX98927_AMP_enables, 1, 1);
		/* enable the v and i for vi feedback */
		max98927_wrap_update_bits(max98927,
			MAX98927_Measurement_enables,
			MAX98927_Measurement_enables_IVADC_V_EN,
			MAX98927_Measurement_enables_IVADC_V_EN);
		max98927_wrap_update_bits(max98927,
			MAX98927_Measurement_enables,
			MAX98927_Measurement_enables_IVADC_I_EN,
			MAX98927_Measurement_enables_IVADC_I_EN);
		max98927_wrap_update_bits(max98927,
			MAX98927_Global_Enable, 1, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		max98927_wrap_update_bits(max98927,
			MAX98927_Global_Enable, 1, 0);
		max98927_wrap_update_bits(max98927,
			MAX98927_AMP_enables, 1, 0);
		/* disable the v and i for vi feedback */
		max98927_wrap_update_bits(max98927,
			MAX98927_Measurement_enables,
			MAX98927_Measurement_enables_IVADC_V_EN,
			0);
		max98927_wrap_update_bits(max98927,
			MAX98927_Measurement_enables,
			MAX98927_Measurement_enables_IVADC_I_EN,
			0);
		break;
	default:
		return 0;
	}
	return 0;
}

static const struct snd_soc_dapm_widget max98927_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAI_OUT", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("Amp Enable", "HiFi Playback", MAX98927_AMP_enables,
		0, 0, max98927_dac_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
};

static DECLARE_TLV_DB_SCALE(max98927_spk_tlv, 300, 300, 0);
static DECLARE_TLV_DB_SCALE(max98927_digital_tlv, -1600, 25, 0);

static int max98927_spk_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98927->spk_gain;
	dev_dbg(codec->dev, "%s: spk_gain setting returned %d\n", __func__,
		(int) ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_spk_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (sel < ((1 << MAX98927_Speaker_Gain_Width) - 1)) {
		max98927_wrap_update_bits(max98927, MAX98927_Speaker_Gain,
			MAX98927_Speaker_Gain_SPK_PCM_GAIN_Mask, sel);
		max98927->spk_gain = sel;
	}
	return 0;
}

static int max98927_digital_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98927->digital_gain;
	dev_dbg(codec->dev, "%s: spk_gain setting returned %d\n", __func__,
		(int) ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_digital_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (sel < ((1 << MAX98927_AMP_VOL_WIDTH) - 1)) {
		max98927_wrap_update_bits(max98927, MAX98927_AMP_volume_control,
			MAX98927_AMP_volume_control_AMP_VOL_Mask, sel);
		max98927->digital_gain = sel;
	}
	return 0;
}

static int max98927_boost_voltage_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Boost_Control_0,
		MAX98927_Boost_Control_0_BST_VOUT_Mask, 0);
}

static int max98927_boost_voltage_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Boost_Control_0,
		MAX98927_Boost_Control_0_BST_VOUT_Mask, 0);
}

static int max98927_amp_vol_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Boost_Control_0,
		MAX98927_Boost_Control_0_BST_VOUT_Mask,
		MAX98927_AMP_VOL_LOCATION_SHIFT);
}

static int max98927_amp_dsp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Brownout_enables,
		MAX98927_Brownout_enables_AMP_DSP_EN, MAX98927_BDE_DSP_SHIFT);
}

static int max98927_amp_dsp_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Brownout_enables,
		MAX98927_Brownout_enables_AMP_DSP_EN, MAX98927_BDE_DSP_SHIFT);
}

static int max98927_ramp_switch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_AMP_DSP_Config,
		MAX98927_AMP_DSP_Config_AMP_VOL_RMP_BYPASS,
		MAX98927_SPK_RMP_EN_SHIFT);
}
static int max98927_ramp_switch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_AMP_DSP_Config,
		MAX98927_AMP_DSP_Config_AMP_VOL_RMP_BYPASS,
		MAX98927_SPK_RMP_EN_SHIFT);
}

static int max98927_dre_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_DRE_Control,
		MAX98927_DRE_Control_DRE_EN, 0);
}
static int max98927_dre_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_DRE_Control,
		MAX98927_DRE_Control_DRE_EN, 0);
}
static int max98927_amp_vol_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol,
		MAX98927_AMP_volume_control,
		MAX98927_AMP_volume_control_AMP_VOL_SEL,
		MAX98927_AMP_VOL_LOCATION_SHIFT);
}
static int max98927_spk_src_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol,
		MAX98927_Speaker_source_select,
		MAX98927_Speaker_source_select_SPK_SOURCE_Mask, 0);
}

static int max98927_spk_src_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol,
		MAX98927_Speaker_source_select,
		MAX98927_Speaker_source_select_SPK_SOURCE_Mask, 0);
}

static int max98927_mono_out_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol,
		MAX98927_PCM_to_speaker_monomix_A,
		MAX98927_PCM_to_spkmonomix_A_DMONOMIX_CFG_Mask,
		MAX98927_PCM_to_speaker_monomix_A_SHIFT);
}

static int max98927_mono_out_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol,
		MAX98927_PCM_to_speaker_monomix_A,
		MAX98927_PCM_to_spkmonomix_A_DMONOMIX_CFG_Mask,
		MAX98927_PCM_to_speaker_monomix_A_SHIFT);
}

static bool max98927_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x0001 ... 0x0028:
	case 0x002B ... 0x004E:
	case 0x0051 ... 0x0055:
	case 0x005A ... 0x0061:
	case 0x0072 ... 0x0087:
	case 0x00FF:
	case 0x0100:
	case 0x01FF:
		return true;
	}
	return false;
};

static const char * const max98927_boost_voltage_text[] = {
	"6.5V", "6.625V", "6.75V", "6.875V", "7V", "7.125V", "7.25V", "7.375V",
	"7.5V", "7.625V", "7.75V", "7.875V", "8V", "8.125V", "8.25V", "8.375V",
	"8.5V", "8.625V", "8.75V", "8.875V", "9V", "9.125V", "9.25V", "9.375V",
	"9.5V", "9.625V", "9.75V", "9.875V", "10V"
};

static const char * const max98927_speaker_source_text[] = {
	"i2s", "reserved", "tone", "pdm"
};

static const char * const max98927_monomix_output_text[] = {
	"ch_0", "ch_1", "ch_1_2_div"
};

static const struct soc_enum max98927_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_monomix_output_text),
		max98927_monomix_output_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_speaker_source_text),
		max98927_speaker_source_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_boost_voltage_text),
		max98927_boost_voltage_text),
};

static const struct snd_kcontrol_new max98927_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Speaker Volume", MAX98927_Speaker_Gain,
		0, (1<<MAX98927_Speaker_Gain_Width)-1, 0,
		max98927_spk_gain_get, max98927_spk_gain_put,
		max98927_spk_tlv),
	SOC_SINGLE_EXT_TLV("Digital Gain", MAX98927_AMP_volume_control,
		0, (1<<MAX98927_AMP_VOL_WIDTH)-1, 0,
		max98927_digital_gain_get, max98927_digital_gain_put,
		max98927_digital_tlv),
	SOC_SINGLE_EXT("Amp DSP Enable", MAX98927_Brownout_enables,
		MAX98927_BDE_DSP_SHIFT, 1, 0,
		max98927_amp_dsp_get, max98927_amp_dsp_put),
	SOC_SINGLE_EXT("Ramp Switch", MAX98927_AMP_DSP_Config,
		MAX98927_SPK_RMP_EN_SHIFT, 1, 1,
		max98927_ramp_switch_get, max98927_ramp_switch_put),
	SOC_SINGLE_EXT("DRE EN", MAX98927_DRE_Control,
		MAX98927_DRE_Control_DRE_SHIFT, 1, 0,
		max98927_dre_en_get, max98927_dre_en_put),
	SOC_SINGLE_EXT("Amp Volume Location", MAX98927_AMP_volume_control,
		MAX98927_AMP_VOL_LOCATION_SHIFT, 1, 0,
		max98927_amp_vol_get, max98927_amp_vol_put),

	SOC_ENUM_EXT("Boost Output Voltage", max98927_enum[2],
	max98927_boost_voltage_get, max98927_boost_voltage_put),
	SOC_ENUM_EXT("Speaker Source", max98927_enum[1],
	max98927_spk_src_get, max98927_spk_src_put),
	SOC_ENUM_EXT("Monomix Output", max98927_enum[0],
	max98927_mono_out_get, max98927_mono_out_put),
};

static const struct snd_soc_dapm_route max98927_audio_map[] = {
	{"BE_OUT", NULL, "Amp Enable"},
};

static struct snd_soc_dai_driver max98927_dai[] = {
	{
		.name = "max98927-aif1",
		.playback = {
			.stream_name = "HiFi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98927_RATES,
			.formats = MAX98927_FORMATS,
		},
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98927_RATES,
			.formats = MAX98927_FORMATS,
		},
		.ops = &max98927_dai_ops,
	}
};

static int max98927_probe(struct snd_soc_codec *codec)
{
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int ret = 0, reg = 0, i;

	max98927->codec = codec;
	codec->control_data = max98927->regmap;
	codec->cache_bypass = 1;

	/* Software Reset */
	max98927_wrapper_write(max98927,
		MAX98927_Software_Reset, MAX98927_Software_Reset_RST);

	/* Check Revision ID for the primary MAX98927*/
	ret = regmap_read(max98927->regmap, MAX98927_REV_ID, &reg);
	if (ret < 0)
		dev_err(codec->dev,
			"Failed to read: 0x%02X\n", MAX98927_REV_ID);
	else
		dev_info(codec->dev,
			"MAX98927 revisionID: 0x%02X\n", reg);

	/* Check Revision ID for the secondary MAX98927*/
	if (max98927->sub_regmap) {
		ret = regmap_read(max98927->sub_regmap, MAX98927_REV_ID, &reg);
		if (ret < 0)
			dev_err(codec->dev,
				"Failed to read: 0x%02X from secodnary device\n"
				, MAX98927_REV_ID);
		else
			dev_info(codec->dev,
				"Secondary device revisionID: 0x%02X\n", reg);
	}

	/* Register initialization */
	for (i = 0; i < sizeof(max98927_reg_map)/
			sizeof(max98927_reg_map[0]); i++)
             {		
                     if(max98927_reg_map[i].reg == 0x0025)
                     {
                         if (max98927->regmap)
                         {
                                 regmap_write(max98927->regmap, 0x0025, 0x40); 
                         }
                        if (max98927->sub_regmap)
                        {
                                 regmap_write(max98927->sub_regmap, 0x0025, 0x00); 
                         }    
                     }
                    else 
		max98927_wrapper_write(max98927,
			max98927_reg_map[i].reg,
			max98927_reg_map[i].def);
             }

	if (max98927->regmap)
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_Channel_Sources_A,
			(max98927->i_l_slot
				<<MAX98927_PCM_Tx_Ch_Sources_A_I_SHIFT|
			max98927->v_l_slot)&0xFF);
	if (max98927->sub_regmap)
		regmap_write(max98927->sub_regmap,
			MAX98927_PCM_Tx_Channel_Sources_A,
			(max98927->i_r_slot
				<<MAX98927_PCM_Tx_Ch_Sources_A_I_SHIFT|
			max98927->v_r_slot)&0xFF);

	max98927_handle_pdata(codec);

	return ret;
}

static const struct snd_soc_codec_driver soc_codec_dev_max98927 = {
	.probe            = max98927_probe,
	.dapm_routes = max98927_audio_map,
	.num_dapm_routes = ARRAY_SIZE(max98927_audio_map),
	.dapm_widgets = max98927_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max98927_dapm_widgets),
	.controls = max98927_snd_controls,
	.num_controls = ARRAY_SIZE(max98927_snd_controls),
};

static const struct regmap_config max98927_regmap = {
	.reg_bits         = 16,
	.val_bits         = 8,
	.max_register     = MAX98927_REV_ID,
	.reg_defaults     = max98927_reg_map,
	.num_reg_defaults = ARRAY_SIZE(max98927_reg_map),
	.readable_reg	  = max98927_readable_register,
	.cache_type       = REGCACHE_RBTREE,
};

static struct i2c_board_info max98927_i2c_sub_board[] = {
	{
		I2C_BOARD_INFO("max98927_sub", 0x39),
	}
};

static struct i2c_driver max98927_i2c_sub_driver = {
	.driver = {
		.name = "max98927_sub",
		.owner = THIS_MODULE,
	},
};

struct i2c_client *max98927_add_sub_device(int bus_id, int slave_addr)
{
	struct i2c_client *i2c = NULL;
	struct i2c_adapter *adapter;

	max98927_i2c_sub_board[0].addr = slave_addr;

	adapter = i2c_get_adapter(bus_id);
	if (adapter) {
		i2c = i2c_new_device(adapter, max98927_i2c_sub_board);
		if (i2c)
			i2c->dev.driver = &max98927_i2c_sub_driver.driver;
	}

	return i2c;
}

int probe_common(struct i2c_client *i2c, struct max98927_priv *max98927)
{
	int ret = 0, value;

	if (!of_property_read_u32(i2c->dev.of_node, "vmon-l-slot", &value))
		max98927->v_l_slot = value & 0xF;
	else
		max98927->v_l_slot = 0;
	if (!of_property_read_u32(i2c->dev.of_node, "imon-l-slot", &value))
		max98927->i_l_slot = value & 0xF;
	else
		max98927->i_l_slot = 1;
	if (!of_property_read_u32(i2c->dev.of_node, "vmon-r-slot", &value))
		max98927->v_r_slot = value & 0xF;
	else
		max98927->v_r_slot = 2;
	if (!of_property_read_u32(i2c->dev.of_node, "imon-r-slot", &value))
		max98927->i_r_slot = value & 0xF;
	else
		max98927->i_r_slot = 3;

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_max98927,
		max98927_dai, ARRAY_SIZE(max98927_dai));
	if (ret < 0)
		dev_err(&i2c->dev,
		    "Failed to register codec: %d\n", ret);
	return ret;
}

static int max98927_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{

	int ret = 0, value, i;
	struct max98927_priv *max98927 = NULL;

	dev_info(&i2c->dev, "%s(%d) entry point\n", __func__, __LINE__);

	max98927 = devm_kzalloc(&i2c->dev,
		sizeof(*max98927), GFP_KERNEL);

	if (!max98927) {
		ret = -ENOMEM;
		goto err;
	}

	/* regulator DVDD */
	max98927->dvdd_core = regulator_get(&i2c->dev, "max98927,dvdd-core");
	if (IS_ERR(max98927->dvdd_core)) {
		dev_err(&i2c->dev, "DVDD regulator_get error\n");
	}
	ret = regulator_set_voltage(max98927->dvdd_core, 1800000, 1800000);
	if (ret < 0) {
		dev_err(&i2c->dev, "DVDD set_voltage failed\n");
	}
	ret = regulator_set_load(max98927->dvdd_core, 300000);
	if (ret < 0) {
		dev_err(&i2c->dev, "DVDD set_load failed\n");
	}
	ret = regulator_enable(max98927->dvdd_core);
	if (ret < 0) {
		dev_err(&i2c->dev, "DVDD regulator_enable failed\n");
	}

        /* AMP1 GPIO RESET */
	max98927->spkamp1_reset_gpio = of_get_named_gpio(i2c->dev.of_node, "max98927,spkamp1_reset_gpio", 0);
	if (gpio_is_valid(max98927->spkamp1_reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, max98927->spkamp1_reset_gpio, GPIOF_OUT_INIT_HIGH,
			"MAX98927 SPKAMP1 RESET");
		if (ret < 0) { 
			dev_err(&i2c->dev, "Failed to request SPKAMP1 RESET %d: %d\n",
				max98927->spkamp1_reset_gpio, ret);
		}
	} else {
		dev_err(&i2c->dev, "error requesting spkamp1_reset_gpio: %d\n", max98927->spkamp1_reset_gpio);
		return -EIO;
	}

        /* AMP2 GPIO RESET */
	max98927->spkamp2_reset_gpio = of_get_named_gpio(i2c->dev.of_node, "max98927,spkamp2_reset_gpio", 0); 
	if (gpio_is_valid(max98927->spkamp2_reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, max98927->spkamp2_reset_gpio, GPIOF_OUT_INIT_HIGH,
			"MAX98927 SPKAMP2 RESET");
		if (ret < 0) { 
			dev_err(&i2c->dev, "Failed to request SPKAMP2 RESET %d: %d\n",
			max98927->spkamp2_reset_gpio, ret);	
		}
	} else {
		dev_err(&i2c->dev, "error requesting spkamp2_reset_gpio: %d\n", max98927->spkamp2_reset_gpio);
		return -EIO;
	}

	mdelay(100);
	i2c_set_clientdata(i2c, max98927);

	/* update interleave mode info */
	if (!of_property_read_u32(i2c->dev.of_node,
		"interleave_mode", &value)) {
		if (value > 0)
			max98927->interleave_mode = 1;
		else
			max98927->interleave_mode = 0;
	} else
		max98927->interleave_mode = 0;

	/* update direct configuration info */
	max98927->regcfg = of_get_property(i2c->dev.of_node,
			"maxim,regcfg", &max98927->regcfg_sz);

	/* check for secondary MAX98927 */
	ret = of_property_read_u32(i2c->dev.of_node,
			"maxim,sub_reg", &max98927->sub_reg);
	if (ret) {
		dev_err(&i2c->dev, "Sub-device slave address was not found.\n");
		max98927->sub_reg = -1;
	}

	ret = of_property_read_u32(i2c->dev.of_node,
			"maxim,sub_bus", &max98927->sub_bus);
	if (ret) {
		dev_err(&i2c->dev, "Sub-device bus information was not found.\n");
		max98927->sub_bus = i2c->adapter->nr;
	}

	/* regmap initialization for primary device */
	max98927->regmap
		= devm_regmap_init_i2c(i2c, &max98927_regmap);
	if (IS_ERR(max98927->regmap)) {
		ret = PTR_ERR(max98927->regmap);
		dev_err(&i2c->dev,
			"Failed to allocate regmap: %d\n", ret);
		goto err;
	}

	/* regmap initialization for secondary device */
	if (max98927->sub_reg > 0)	{
		max98927->sub_i2c = max98927_add_sub_device(max98927->sub_bus,
			max98927->sub_reg);
		if (IS_ERR(max98927->sub_i2c)) {
			dev_err(&max98927->sub_i2c->dev,
					"Second MAX98927 was not found\n");
			ret = PTR_ERR(max98927->regmap);
			goto err;
		} else {
			max98927->sub_regmap = regmap_init_i2c(
					max98927->sub_i2c, &max98927_regmap);
			if (IS_ERR(max98927->sub_regmap)) {
				ret = PTR_ERR(max98927->sub_regmap);
				dev_err(&max98927->sub_i2c->dev,
					"Failed to allocate sub_regmap: %d\n",
					ret);
				goto err;
			} else
				dev_info(&max98927->sub_i2c->dev,
					"sub_regmap init_i2c OK\n");
		}
	}

/* add register map update */
#if 1

	/* Software Reset */
	max98927_wrapper_write(max98927,
		MAX98927_Software_Reset, MAX98927_Software_Reset_RST);
	mdelay(5);

	/* Check Revision ID for the primary MAX98927*/
	ret = regmap_read(max98927->regmap, MAX98927_REV_ID, &value);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed regmap_read ret = %d\n", ret);
		dev_err(&i2c->dev,
			"Failed to read: 0x%02X\n", MAX98927_REV_ID);
	} else
		dev_info(&i2c->dev,
			"MAX98927 revisionID: 0x%02X\n", value);

	/* Check Revision ID for the secondary MAX98927*/
	if (max98927->sub_regmap) {
		ret = regmap_read(max98927->sub_regmap, MAX98927_REV_ID, &value);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed regmap_read ret = %d\n", ret);
			dev_err(&i2c->dev,
				"Failed to read: 0x%02X from secodnary device\n"
				, MAX98927_REV_ID);
		} else
			dev_info(&i2c->dev,
				"Secondary device revisionID: 0x%02X\n", value);
	}

	/* Register initialization */
	for (i = 0; i < sizeof(max98927_reg_map)/
			sizeof(max98927_reg_map[0]); i++)
	{
		if(max98927_reg_map[i].reg == 0x0025) {
			if (max98927->regmap) {
				regmap_write(max98927->regmap, 0x0025, 0x40);
			}
			if (max98927->sub_regmap) {
				regmap_write(max98927->sub_regmap, 0x0025, 0x00);
			}
		}
		else {
		max98927_wrapper_write(max98927,
			max98927_reg_map[i].reg,
			max98927_reg_map[i].def);
		}
	}

	if (max98927->regmap)
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_Channel_Sources_A,
			(max98927->i_l_slot
				<<MAX98927_PCM_Tx_Ch_Sources_A_I_SHIFT|
			max98927->v_l_slot)&0xFF);
	if (max98927->sub_regmap)
		regmap_write(max98927->sub_regmap,
			MAX98927_PCM_Tx_Channel_Sources_A,
			(max98927->i_r_slot
				<<MAX98927_PCM_Tx_Ch_Sources_A_I_SHIFT|
			max98927->v_r_slot)&0xFF);

	max98927_wrapper_write(max98927,
			MAX98927_PCM_Tx_HiZ_Control_B,
			0xFF);
	/* Set interleave mode */
	if (max98927->interleave_mode) {
		max98927_wrap_update_bits(max98927,
				MAX98927_PCM_Tx_Channel_Sources_B,	/* 1F */
				MAX98927_PCM_Tx_Channel_Src_INTERLEAVE_Mask,
				MAX98927_PCM_Tx_Channel_Src_INTERLEAVE_Mask);
		/* left */
		regmap_write(max98927->sub_regmap,	/* 1E */
			MAX98927_PCM_Tx_Channel_Sources_A,
			0x0);
		regmap_write(max98927->sub_regmap,
			MAX98927_PCM_Tx_Enables_A,	/* 1A */
			0x01);
		regmap_write(max98927->sub_regmap,
			MAX98927_PCM_Tx_HiZ_Control_A,	/* 1C */
			0xFE);
		/* right */
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_Channel_Sources_A,	/* 1E */
			0x11);
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_Enables_A,	/* 1A */
			0x02);
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_HiZ_Control_A,	/* 1C */
			0xFD);
	} else {
		max98927_wrap_update_bits(max98927,
				MAX98927_PCM_Tx_Channel_Sources_B,	/* 1F */
				MAX98927_PCM_Tx_Channel_Src_INTERLEAVE_Mask,
				0);
		/* left */
		regmap_write(max98927->sub_regmap,
			MAX98927_PCM_Tx_Enables_A,	/* 1A */
			0x03);
		regmap_write(max98927->sub_regmap,
			MAX98927_PCM_Tx_HiZ_Control_A,	/* 1C */
			0xFC);
		regmap_write(max98927->sub_regmap,	/* 1E */
			MAX98927_PCM_Tx_Channel_Sources_A,
			0x10);
		/* right */
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_Enables_A,	/* 1A */
			0x0C);
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_HiZ_Control_A,	/* 1C */
			0xF3);
		regmap_write(max98927->regmap,
			MAX98927_PCM_Tx_Channel_Sources_A,	/* 1E */
			0x32);
	}
#endif

	/* codec registeration */
	ret = probe_common(i2c, max98927);

	dev_info(&i2c->dev, "%s(%d) leave point\n", __func__, __LINE__);

	return ret;

err:
	if (max98927) 
		devm_kfree(&i2c->dev, max98927);
	dev_err(&i2c->dev, "%s probe error\n", __func__);
	return ret;
}

static int max98927_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id max98927_i2c_id[] = {
	{ "max98927", 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, max98927_i2c_id);

static const struct of_device_id max98927_of_match[] = {
	{ .compatible = "maxim,max98927", },
	{ }
};
MODULE_DEVICE_TABLE(of, max98927_of_match);

static struct i2c_driver max98927_i2c_driver = {
	.driver = {
		.name = "max98927",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max98927_of_match),
		.pm = NULL,
	},
	.probe  = max98927_i2c_probe,
	.remove = max98927_i2c_remove,
	.id_table = max98927_i2c_id,
};

module_i2c_driver(max98927_i2c_driver)

MODULE_DESCRIPTION("ALSA SoC MAX98927 driver");
MODULE_AUTHOR("Ryan Lee <ryans.lee@maximintegrated.com>");
MODULE_LICENSE("GPL");
