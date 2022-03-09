/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/regmap.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/regulator/consumer.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6core.h>
#include <ipc/apr.h>
#include <soc/internal.h>
#include "sdm660-cdc-registers.h"
#include "msm-digital-cdc.h"
#include "msm-cdc-common.h"
#include "../../sdm660-common.h"

#define DRV_NAME "msm_digital_codec"
#define MCLK_RATE_9P6MHZ        9600000
#define MCLK_RATE_12P288MHZ     12288000
#define TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define CF_MIN_3DB_4HZ			0x0
#define CF_MIN_3DB_75HZ			0x1
#define CF_MIN_3DB_150HZ		0x2

#define DEC_SVA 5
#define MSM_DIG_CDC_VERSION_ENTRY_SIZE 32
#define MAX_ON_DEMAND_DIG_SUPPLY_NAME_LENGTH 64
#define CODEC_DT_MAX_PROP_SIZE 40

static unsigned long rx_digital_gain_reg[] = {
	MSM89XX_CDC_CORE_RX1_VOL_CTL_B2_CTL,
	MSM89XX_CDC_CORE_RX2_VOL_CTL_B2_CTL,
	MSM89XX_CDC_CORE_RX3_VOL_CTL_B2_CTL,
};

static unsigned long tx_digital_gain_reg[] = {
	MSM89XX_CDC_CORE_TX1_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX2_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX3_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX4_VOL_CTL_GAIN,
	MSM89XX_CDC_CORE_TX5_VOL_CTL_GAIN,
};

#define SDM660_TX_UNMUTE_DELAY_MS 40
static int tx_unmute_delay = SDM660_TX_UNMUTE_DELAY_MS;
module_param(tx_unmute_delay, int, 0664);
MODULE_PARM_DESC(tx_unmute_delay, "delay to unmute the tx path");

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);

struct snd_soc_codec *registered_digcodec;
struct hpf_work tx_hpf_work[NUM_DECIMATORS];

static int msm_dig_cdc_enable_on_demand_supply(
			struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event);
static char on_demand_supply_name[][MAX_ON_DEMAND_DIG_SUPPLY_NAME_LENGTH] = {
	"cdc-vdd-digital",
};

/* Codec supports 2 IIR filters */
enum {
	IIR1 = 0,
	IIR2,
	IIR_MAX,
};

/*
 * msm_digcdc_mclk_enable - add mclk support in digital codec
 * @codec: codec instance
 * @mclk_enable: mclk enable/disable
 * @dapm: check for dapm widget
 */
int msm_digcdc_mclk_enable(struct snd_soc_codec *codec,
			int mclk_enable, bool dapm)
{
	dev_dbg(codec->dev, "%s: mclk_enable = %u, dapm = %d\n",
			__func__, mclk_enable, dapm);
	if (mclk_enable) {
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_MCLK_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_TOP_CTL, 0x01, 0x01);
	} else {
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_TOP_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_MCLK_CTL, 0x01, 0x00);
	}

	return 0;
}
EXPORT_SYMBOL(msm_digcdc_mclk_enable);

static int msm_digcdc_clock_control(bool flag)
{
	int ret = -EINVAL;
	struct msm_asoc_mach_data *pdata = NULL;
	struct msm_dig_priv *msm_dig_cdc =
				snd_soc_codec_get_drvdata(registered_digcodec);

	pdata = snd_soc_card_get_drvdata(registered_digcodec->component.card);

	if (flag) {
		mutex_lock(&pdata->cdc_int_mclk0_mutex);
		if (atomic_read(&pdata->int_mclk0_enabled) == false) {
			if (msm_dig_cdc->regmap->cache_only == true)
				return ret;
			if (pdata->native_clk_set)
				pdata->digital_cdc_core_clk.clk_freq_in_hz =
							NATIVE_MCLK_RATE;
			else
				pdata->digital_cdc_core_clk.clk_freq_in_hz =
							DEFAULT_MCLK_RATE;
			pdata->digital_cdc_core_clk.enable = 1;
			ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_INT0_MI2S_RX,
						&pdata->digital_cdc_core_clk);
			if (ret < 0) {
				pr_err("%s:failed to enable the MCLK\n",
				       __func__);
				/*
				 * Avoid access to lpass register
				 * as clock enable failed during SSR/PDR.
				 */
				msm_dig_cdc->regmap->cache_only = true;
				return ret;
			}
			pr_debug("enabled digital codec core clk\n");
			atomic_set(&pdata->int_mclk0_enabled, true);
			schedule_delayed_work(&pdata->disable_int_mclk0_work,
					      50);
		}
	} else {
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		dev_dbg(registered_digcodec->dev,
			"disable MCLK, workq to disable set already\n");
	}
	return 0;
}

static void enable_digital_callback(void *flag)
{
	msm_digcdc_clock_control(true);
}

static void disable_digital_callback(void *flag)
{
	msm_digcdc_clock_control(false);
	pr_debug("disable mclk happens in workq\n");
}

static int msm_dig_cdc_put_dec_enum(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int dec_mux, decimator;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	u16 tx_mux_ctl_reg;
	u8 adc_dmic_sel = 0x0;
	int ret = 0;
	char *dec_num;

	if (ucontrol->value.enumerated.item[0] > e->items) {
		dev_err(codec->dev, "%s: Invalid enum value: %d\n",
			__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}
	dec_mux = ucontrol->value.enumerated.item[0];

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name) {
		dev_err(codec->dev, "%s: failed to copy string\n",
			__func__);
		return -ENOMEM;
	}
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	dec_num = strpbrk(dec_name, "12345");
	if (dec_num == NULL) {
		dev_err(codec->dev, "%s: Invalid DEC selected\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec_num, 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(w->dapm->dev, "%s(): widget = %s decimator = %u dec_mux = %u\n"
		, __func__, w->name, decimator, dec_mux);

	switch (decimator) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if ((dec_mux == 4) || (dec_mux == 5) ||
		    (dec_mux == 6) || (dec_mux == 7))
			adc_dmic_sel = 0x1;
		else
			adc_dmic_sel = 0x0;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid Decimator = %u\n",
			__func__, decimator);
		ret = -EINVAL;
		goto out;
	}

	tx_mux_ctl_reg =
		MSM89XX_CDC_CORE_TX1_MUX_CTL + 32 * (decimator - 1);

	if (decimator == DEC_SVA)
		tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX5_MUX_CTL;

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}


static int msm_dig_cdc_codec_config_compander(struct snd_soc_codec *codec,
					      int interp_n, int event)
{
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	int comp_ch_bits_set = 0x03;
	int comp_ch_value;

	dev_dbg(codec->dev, "%s: event %d shift %d, enabled %d\n",
		__func__, event, interp_n,
		dig_cdc->comp_enabled[interp_n]);

	/* compander is invalid */
	if (dig_cdc->comp_enabled[interp_n] != COMPANDER_1 &&
	    dig_cdc->comp_enabled[interp_n]) {
		dev_dbg(codec->dev, "%s: Invalid compander %d\n", __func__,
			dig_cdc->comp_enabled[interp_n]);
		return 0;
	}

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* compander is not enabled */
		if (!dig_cdc->comp_enabled[interp_n] &&
			dig_cdc->set_compander_mode) {
			dig_cdc->set_compander_mode(dig_cdc->handle, 0x00);
			return 0;
		};
		comp_ch_value = snd_soc_read(codec,
					     MSM89XX_CDC_CORE_COMP0_B1_CTL);
		if (interp_n == 0) {
			if (comp_ch_value & 0x02) {
				dev_dbg(codec->dev,
					"%s comp ch 1  already enabled\n",
					__func__);
				return 0;
			}
		}
		if (interp_n == 1) {
			if (comp_ch_value & 0x01) {
				dev_dbg(codec->dev,
					"%s comp ch 0 already enabled\n",
					__func__);
				return 0;
			}
		}
		if (dig_cdc->set_compander_mode)
			dig_cdc->set_compander_mode(dig_cdc->handle, 0x08);
		/* Enable Compander Clock */
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_COMP0_B2_CTL, 0x0F, 0x09);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_RX_B2_CTL, 0x01, 0x01);
		if (dig_cdc->comp_enabled[MSM89XX_RX1]) {
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B1_CTL,
				0x02, 0x02);
		}
		if (dig_cdc->comp_enabled[MSM89XX_RX2]) {
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B1_CTL,
				0x01, 0x01);
		}
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_COMP0_B3_CTL, 0xFF, 0x01);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_COMP0_B2_CTL, 0xF0, 0x50);
		/* add sleep for compander to settle */
		usleep_range(1000, 1100);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_COMP0_B3_CTL, 0xFF, 0x28);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_COMP0_B2_CTL, 0xF0, 0xB0);

		/* Enable Compander GPIO */
		if (dig_cdc->codec_hph_comp_gpio)
			dig_cdc->codec_hph_comp_gpio(1, codec);
	} else if (SND_SOC_DAPM_EVENT_OFF(event)) {
		/* Disable Compander GPIO */
		if (dig_cdc->codec_hph_comp_gpio)
			dig_cdc->codec_hph_comp_gpio(0, codec);

		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_COMP0_B1_CTL,
			1 << interp_n, 0);
		comp_ch_bits_set = snd_soc_read(codec,
					 MSM89XX_CDC_CORE_COMP0_B1_CTL);
		if ((comp_ch_bits_set & 0x03) == 0x00) {
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_COMP0_B2_CTL, 0x0F, 0x05);
			 snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_CLK_RX_B2_CTL, 0x01, 0x00);
		}
	}
	return 0;
}

/**
 * msm_dig_cdc_hph_comp_cb - registers callback to codec by machine driver.
 *
 * @codec_hph_comp_gpio: function pointer to set comp gpio at machine driver
 * @codec: codec pointer
 *
 */
void msm_dig_cdc_hph_comp_cb(
	int (*codec_hph_comp_gpio)(bool enable, struct snd_soc_codec *codec),
	struct snd_soc_codec *codec)
{
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: Enter\n", __func__);
	dig_cdc->codec_hph_comp_gpio = codec_hph_comp_gpio;
}
EXPORT_SYMBOL(msm_dig_cdc_hph_comp_cb);

static int msm_dig_cdc_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
						 struct snd_kcontrol *kcontrol,
						 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_dig_priv *msm_dig_cdc = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	if (w->shift >= MSM89XX_RX_MAX || w->shift < 0) {
		dev_err(codec->dev, "%s: wrong RX index: %d\n",
			__func__, w->shift);
		return -EINVAL;
	}
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msm_dig_cdc_codec_config_compander(codec, w->shift, event);
		/* apply the digital gain after the interpolator is enabled*/
		if ((w->shift) < ARRAY_SIZE(rx_digital_gain_reg))
			snd_soc_write(codec,
				  rx_digital_gain_reg[w->shift],
				  snd_soc_read(codec,
				  rx_digital_gain_reg[w->shift])
				  );
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm_dig_cdc_codec_config_compander(codec, w->shift, event);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		/*
		 * disable the mute enabled during the PMD of this device
		 */
		if ((w->shift == 0) &&
			(msm_dig_cdc->mute_mask & HPHL_PA_DISABLE)) {
			pr_debug("disabling HPHL mute\n");
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B6_CTL, 0x01, 0x00);
			msm_dig_cdc->mute_mask &= ~(HPHL_PA_DISABLE);
		} else if ((w->shift == 1) &&
				(msm_dig_cdc->mute_mask & HPHR_PA_DISABLE)) {
			pr_debug("disabling HPHR mute\n");
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B6_CTL, 0x01, 0x00);
			msm_dig_cdc->mute_mask &= ~(HPHR_PA_DISABLE);
		} else if ((w->shift == 2) &&
				(msm_dig_cdc->mute_mask & SPKR_PA_DISABLE)) {
			pr_debug("disabling SPKR mute\n");
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX3_B6_CTL, 0x01, 0x00);
			msm_dig_cdc->mute_mask &= ~(SPKR_PA_DISABLE);
		}
	}
	return 0;
}

static int msm_dig_cdc_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec,
			    (MSM89XX_CDC_CORE_IIR1_CTL + 64 * iir_idx)) &
		(1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_dig_cdc_put_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_update_bits(codec,
		(MSM89XX_CDC_CORE_IIR1_CTL + 64 * iir_idx),
			    (1 << band_idx), (value << band_idx));

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
	  iir_idx, band_idx,
		((snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_CTL + 64 * iir_idx)) &
	  (1 << band_idx)) != 0));

	return 0;
}

static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				   int iir_idx, int band_idx,
				   int coeff_idx)
{
	uint32_t value = 0;

	/* Address does not automatically update if reading */
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx));

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 8);

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 16);

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec, (MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL
		+ 64 * iir_idx)) & 0x3f) << 24);

	return value;

}

static void set_iir_band_coeff(struct snd_soc_codec *codec,
			       int iir_idx, int band_idx,
			       uint32_t value)
{
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 24) & 0x3F);

}

static int msm_dig_cdc_get_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 0);
	ucontrol->value.integer.value[1] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 1);
	ucontrol->value.integer.value[2] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 2);
	ucontrol->value.integer.value[3] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 3);
	ucontrol->value.integer.value[4] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 4);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[1],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[2],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[3],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[4]);
	return 0;
}

static int msm_dig_cdc_put_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	/* Mask top bit it is reserved */
	/* Updates addr automatically for each B2 write */
	snd_soc_write(codec,
		(MSM89XX_CDC_CORE_IIR1_COEF_B1_CTL + 64 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);


	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[0]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[1]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[2]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[3]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[4]);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 0),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 1),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 2),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 3),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 4));
	return 0;
}

static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct snd_soc_codec *codec;
	struct msm_dig_priv *msm_dig_cdc;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	codec = hpf_work->dig_cdc->codec;
	msm_dig_cdc = hpf_work->dig_cdc;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 32;

	if (hpf_work->decimator == DEC_SVA)
		tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX5_MUX_CTL;

	dev_dbg(codec->dev, "%s(): decimator %u hpf_cut_of_freq 0x%x\n",
		 __func__, hpf_work->decimator, (unsigned int)hpf_cut_of_freq);
	if (msm_dig_cdc->update_clkdiv)
		msm_dig_cdc->update_clkdiv(msm_dig_cdc->handle, 0x51);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
}

static int msm_dig_cdc_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int value = 0, reg;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (w->shift == 0)
			reg = MSM89XX_CDC_CORE_IIR1_GAIN_B1_CTL;
		else if (w->shift == 1)
			reg = MSM89XX_CDC_CORE_IIR2_GAIN_B1_CTL;
		else
			goto ret;
		value = snd_soc_read(codec, reg);
		snd_soc_write(codec, reg, value);
		break;
	default:
		pr_err("%s: event = %d not expected\n", __func__, event);
	}
ret:
	return 0;
}

static int msm_dig_cdc_compander_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	int comp_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int rx_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	dev_dbg(codec->dev, "%s: msm_dig_cdc->comp[%d]_enabled[%d] = %d\n",
			__func__, comp_idx, rx_idx,
			dig_cdc->comp_enabled[rx_idx]);

	ucontrol->value.integer.value[0] = dig_cdc->comp_enabled[rx_idx];

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_dig_cdc_compander_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	int comp_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int rx_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	if (dig_cdc->version >= DIANGU) {
		if (!value)
			dig_cdc->comp_enabled[rx_idx] = 0;
		else
			dig_cdc->comp_enabled[rx_idx] = comp_idx;
	}

	dev_dbg(codec->dev, "%s: msm_dig_cdc->comp[%d]_enabled[%d] = %d\n",
		__func__, comp_idx, rx_idx,
		dig_cdc->comp_enabled[rx_idx]);

	return 0;
}

static const struct snd_kcontrol_new compander_kcontrols[] = {
	SOC_SINGLE_EXT("COMP0 RX1", COMPANDER_1, MSM89XX_RX1, 1, 0,
	msm_dig_cdc_compander_get, msm_dig_cdc_compander_set),

	SOC_SINGLE_EXT("COMP0 RX2", COMPANDER_1, MSM89XX_RX2, 1, 0,
	msm_dig_cdc_compander_get, msm_dig_cdc_compander_set),

};

static int msm_dig_cdc_set_interpolator_rate(struct snd_soc_dai *dai,
					     u8 rx_fs_rate_reg_val,
					     u32 sample_rate)
{
	snd_soc_update_bits(dai->codec,
			MSM89XX_CDC_CORE_RX1_B5_CTL, 0xF0, rx_fs_rate_reg_val);
	snd_soc_update_bits(dai->codec,
			MSM89XX_CDC_CORE_RX2_B5_CTL, 0xF0, rx_fs_rate_reg_val);
	return 0;
}

static int msm_dig_cdc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	u8 tx_fs_rate, rx_fs_rate, rx_clk_fs_rate;
	int ret;

	dev_dbg(dai->codec->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d format %d\n",
		__func__, dai->name, dai->id, params_rate(params),
		params_channels(params), params_format(params));

	switch (params_rate(params)) {
	case 8000:
		tx_fs_rate = 0x00;
		rx_fs_rate = 0x00;
		rx_clk_fs_rate = 0x00;
		break;
	case 16000:
		tx_fs_rate = 0x20;
		rx_fs_rate = 0x20;
		rx_clk_fs_rate = 0x01;
		break;
	case 32000:
		tx_fs_rate = 0x40;
		rx_fs_rate = 0x40;
		rx_clk_fs_rate = 0x02;
		break;
	case 44100:
	case 48000:
		tx_fs_rate = 0x60;
		rx_fs_rate = 0x60;
		rx_clk_fs_rate = 0x03;
		break;
	case 96000:
		tx_fs_rate = 0x80;
		rx_fs_rate = 0x80;
		rx_clk_fs_rate = 0x04;
		break;
	case 192000:
		tx_fs_rate = 0xA0;
		rx_fs_rate = 0xA0;
		rx_clk_fs_rate = 0x05;
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid sampling rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	snd_soc_update_bits(dai->codec,
			MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 0x0F, rx_clk_fs_rate);

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_CAPTURE:
		break;
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = msm_dig_cdc_set_interpolator_rate(dai, rx_fs_rate,
						  params_rate(params));
		if (ret < 0) {
			dev_err(dai->codec->dev,
				"%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	}
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(dai->codec,
				MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 0x20, 0x20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		snd_soc_update_bits(dai->codec,
				MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 0x20, 0x00);
		break;
	default:
		dev_err(dai->codec->dev, "%s: wrong format selected\n",
				__func__);
		return -EINVAL;
	}
	return 0;
}

static int msm_dig_cdc_codec_enable_dmic(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_dig_priv *dig_cdc = snd_soc_codec_get_drvdata(codec);
	u8  dmic_clk_en;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	unsigned int dmic;
	int ret;
	char *dmic_num = strpbrk(w->name, "1234");

	if (dmic_num == NULL) {
		dev_err(codec->dev, "%s: Invalid DMIC\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(dmic_num, 10, &dmic);
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Invalid DMIC line on the codec\n", __func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 1:
	case 2:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(dig_cdc->dmic_1_2_clk_cnt);
		dmic_clk_reg = MSM89XX_CDC_CORE_CLK_DMIC_B1_CTL;
		dev_dbg(codec->dev,
			"%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;
	case 3:
	case 4:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(dig_cdc->dmic_3_4_clk_cnt);
		dmic_clk_reg = MSM89XX_CDC_CORE_CLK_DMIC_B2_CTL;
		dev_dbg(codec->dev,
			"%s() event %d DMIC%d dmic_3_4_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;
	default:
		dev_err(codec->dev, "%s: Invalid DMIC Selection\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1) {
			snd_soc_update_bits(codec, dmic_clk_reg,
					0x0E, 0x04);
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);
		}
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_TX1_DMIC_CTL + (dmic - 1) * 0x20,
			0x07, 0x02);
		break;
	case SND_SOC_DAPM_POST_PMD:
		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0)
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, 0);
		break;
	}
	return 0;
}

static int msm_dig_cdc_codec_enable_dec(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_asoc_mach_data *pdata = NULL;
	unsigned int decimator;
	struct msm_dig_priv *msm_dig_cdc = snd_soc_codec_get_drvdata(codec);
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0, i;
	u16 dec_reset_reg, tx_vol_ctl_reg, tx_mux_ctl_reg;
	u8 dec_hpf_cut_of_freq;
	int offset;
	char *dec_num;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, w->name);
		ret = -EINVAL;
		goto out;
	}

	dec_num = strpbrk(dec_name, "12345");
	if (dec_num == NULL) {
		dev_err(codec->dev, "%s: Invalid Decimator\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec_num, 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, dec_name);
		ret = -EINVAL;
		goto out;
	}

	dev_dbg(codec->dev,
		"%s(): widget = %s dec_name = %s decimator = %u\n", __func__,
		w->name, dec_name, decimator);

	if (w->reg == MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = MSM89XX_CDC_CORE_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else {
		dev_err(codec->dev, "%s: Error, incorrect dec\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	tx_vol_ctl_reg = MSM89XX_CDC_CORE_TX1_VOL_CTL_CFG +
			 32 * (decimator - 1);
	tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX1_MUX_CTL +
			  32 * (decimator - 1);
	if (decimator == DEC_SVA) {
		tx_vol_ctl_reg = MSM89XX_CDC_CORE_TX5_VOL_CTL_CFG;
		tx_mux_ctl_reg = MSM89XX_CDC_CORE_TX5_MUX_CTL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enableable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				msm_dig_cdc->dec_active[i] = true;
		}

		dec_hpf_cut_of_freq = snd_soc_read(codec, tx_mux_ctl_reg);

		dec_hpf_cut_of_freq = (dec_hpf_cut_of_freq & 0x30) >> 4;

		tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq =
			dec_hpf_cut_of_freq;

		if (dec_hpf_cut_of_freq != CF_MIN_3DB_150HZ) {

			/* set cut of freq to CF_MIN_3DB_150HZ (0x1); */
			snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
					    CF_MIN_3DB_150HZ << 4);
		}
		if (msm_dig_cdc->update_clkdiv)
			msm_dig_cdc->update_clkdiv(msm_dig_cdc->handle, 0x42);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* enable HPF */
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x00);

		schedule_delayed_work(
			    &msm_dig_cdc->tx_mute_dwork[decimator - 1].dwork,
			    msecs_to_jiffies(tx_unmute_delay));
		if (tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq !=
				CF_MIN_3DB_150HZ) {

			schedule_delayed_work(&tx_hpf_work[decimator - 1].dwork,
					msecs_to_jiffies(300));
		}
		/* apply the digital gain after the decimator is enabled*/
		if ((w->shift) < ARRAY_SIZE(tx_digital_gain_reg))
			snd_soc_write(codec,
				  tx_digital_gain_reg[w->shift + offset],
				  snd_soc_read(codec,
				  tx_digital_gain_reg[w->shift + offset])
				  );
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		msleep(20);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		cancel_delayed_work_sync(&tx_hpf_work[decimator - 1].dwork);
		cancel_delayed_work_sync(
			&msm_dig_cdc->tx_mute_dwork[decimator - 1].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
			(tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq) << 4);
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				msm_dig_cdc->dec_active[i] = false;
		}
		break;
	}
out:
	kfree(widget_name);
	return ret;
}

static int msm_dig_cdc_event_notify(struct notifier_block *block,
				    unsigned long val,
				    void *data)
{
	enum dig_cdc_notify_event event = (enum dig_cdc_notify_event)val;
	struct snd_soc_codec *codec = registered_digcodec;
	struct msm_dig_priv *msm_dig_cdc = snd_soc_codec_get_drvdata(codec);
	struct msm_asoc_mach_data *pdata = NULL;
	struct msm_cap_mode *capmode;
	int ret = -EINVAL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);

	switch (event) {
	case DIG_CDC_EVENT_CLK_ON:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_CLK_PDM_CTL, 0x03, 0x03);
		if (pdata->mclk_freq == MCLK_RATE_12P288MHZ ||
		    pdata->native_clk_set)
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_TOP_CTL, 0x01, 0x00);
		else if (pdata->mclk_freq == MCLK_RATE_9P6MHZ)
			snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_TOP_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_MCLK_CTL, 0x01, 0x01);
		break;
	case DIG_CDC_EVENT_CLK_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_PDM_CTL, 0x03, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_CLK_MCLK_CTL, 0x01, 0x00);
		break;
	case DIG_CDC_EVENT_RX1_MUTE_ON:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX1_B6_CTL, 0x01, 0x01);
		msm_dig_cdc->mute_mask |= HPHL_PA_DISABLE;
		break;
	case DIG_CDC_EVENT_RX1_MUTE_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX1_B6_CTL, 0x01, 0x00);
		msm_dig_cdc->mute_mask &= (~HPHL_PA_DISABLE);
		break;
	case DIG_CDC_EVENT_RX2_MUTE_ON:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX2_B6_CTL, 0x01, 0x01);
		msm_dig_cdc->mute_mask |= HPHR_PA_DISABLE;
		break;
	case DIG_CDC_EVENT_RX2_MUTE_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX2_B6_CTL, 0x01, 0x00);
		msm_dig_cdc->mute_mask &= (~HPHR_PA_DISABLE);
		break;
	case DIG_CDC_EVENT_RX3_MUTE_ON:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX3_B6_CTL, 0x01, 0x01);
		msm_dig_cdc->mute_mask |= SPKR_PA_DISABLE;
		break;
	case DIG_CDC_EVENT_RX3_MUTE_OFF:
		snd_soc_update_bits(codec,
			MSM89XX_CDC_CORE_RX3_B6_CTL, 0x01, 0x00);
		msm_dig_cdc->mute_mask &= (~SPKR_PA_DISABLE);
		break;
	case DIG_CDC_EVENT_PRE_RX1_INT_ON:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x3C, 0x28);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B4_CTL, 0x18, 0x10);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x80, 0x80);
		break;
	case DIG_CDC_EVENT_PRE_RX2_INT_ON:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x3C, 0x28);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B4_CTL, 0x18, 0x10);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x80, 0x80);
		break;
	case DIG_CDC_EVENT_POST_RX1_INT_OFF:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x3C, 0x00);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B4_CTL, 0x18, 0xFF);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX1_B3_CTL, 0x80, 0x00);
		break;
	case DIG_CDC_EVENT_POST_RX2_INT_OFF:
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x3C, 0x00);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B4_CTL, 0x18, 0xFF);
		snd_soc_update_bits(codec,
				MSM89XX_CDC_CORE_RX2_B3_CTL, 0x80, 0x00);
		break;
	case DIG_CDC_EVENT_SSR_DOWN:
		regcache_cache_only(msm_dig_cdc->regmap, true);
		mutex_lock(&pdata->cdc_int_mclk0_mutex);
		atomic_set(&pdata->int_mclk0_enabled, false);
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		break;
	case DIG_CDC_EVENT_SSR_UP:
		regcache_cache_only(msm_dig_cdc->regmap, false);
		regcache_mark_dirty(msm_dig_cdc->regmap);

		mutex_lock(&pdata->cdc_int_mclk0_mutex);
		pdata->digital_cdc_core_clk.enable = 1;
		ret = afe_set_lpass_clock_v2(
					AFE_PORT_ID_INT0_MI2S_RX,
					&pdata->digital_cdc_core_clk);
		if (ret < 0) {
			pr_err("%s:failed to enable the MCLK\n",
			       __func__);
			mutex_unlock(&pdata->cdc_int_mclk0_mutex);
			break;
		}
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);

		regcache_sync(msm_dig_cdc->regmap);

		mutex_lock(&pdata->cdc_int_mclk0_mutex);
		pdata->digital_cdc_core_clk.enable = 0;
		afe_set_lpass_clock_v2(
				AFE_PORT_ID_INT0_MI2S_RX,
				&pdata->digital_cdc_core_clk);
		mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		break;
	case DIG_CDC_EVENT_CAP_CONFIGURE:
		capmode = (struct msm_cap_mode *)data;
		capmode->micbias1_cap_mode = pdata->micbias1_cap_mode;
		capmode->micbias2_cap_mode = pdata->micbias2_cap_mode;
		break;
	case DIG_CDC_EVENT_INVALID:
	default:
		break;
	}
	return 0;
}

static ssize_t msm_dig_codec_version_read(struct snd_info_entry *entry,
					  void *file_private_data,
					  struct file *file,
					  char __user *buf, size_t count,
					  loff_t pos)
{
	struct msm_dig_priv *msm_dig;
	char buffer[MSM_DIG_CDC_VERSION_ENTRY_SIZE];
	int len = 0;

	msm_dig = (struct msm_dig_priv *) entry->private_data;
	if (!msm_dig) {
		pr_err("%s: msm_dig priv is null\n", __func__);
		return -EINVAL;
	}

	switch (msm_dig->version) {
	case DRAX_CDC:
		len = snprintf(buffer, sizeof(buffer), "SDM660-CDC_1_0\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops msm_dig_codec_info_ops = {
	.read = msm_dig_codec_version_read,
};

/*
 * msm_dig_codec_info_create_codec_entry - creates msm_dig module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates msm_dig module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int msm_dig_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					  struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct msm_dig_priv *msm_dig;
	struct snd_soc_card *card;

	if (!codec_root || !codec)
		return -EINVAL;

	msm_dig = snd_soc_codec_get_drvdata(codec);
	card = codec->component.card;
	msm_dig->entry = snd_info_create_subdir(codec_root->module,
						  "msm_digital_codec",
						  codec_root);
	if (!msm_dig->entry) {
		dev_dbg(codec->dev, "%s: failed to create msm_digital entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   msm_dig->entry);
	if (!version_entry) {
		dev_dbg(codec->dev, "%s: failed to create msm_digital version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = msm_dig;
	version_entry->size = MSM_DIG_CDC_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &msm_dig_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	msm_dig->version_entry = version_entry;
	if (msm_dig->get_cdc_version)
		msm_dig->version = msm_dig->get_cdc_version(msm_dig->handle);
	else
		msm_dig->version = DRAX_CDC;

	return 0;
}
EXPORT_SYMBOL(msm_dig_codec_info_create_codec_entry);

static void msm_dig_cdc_update_micbias_regulator(
				const struct msm_dig_priv *dig_cdc,
				const char *name,
				struct on_demand_dig_supply *micbias_supply)
{
	int i;

	for (i = 0; i < dig_cdc->num_of_supplies; i++) {
		if (dig_cdc->supplies[i].supply &&
				!strcmp(dig_cdc->supplies[i].supply, name)) {
			micbias_supply->supply =
				dig_cdc->supplies[i].consumer;
			micbias_supply->min_uv = dig_cdc->regulator[i].min_uv;
			micbias_supply->max_uv = dig_cdc->regulator[i].max_uv;
			micbias_supply->optimum_ua =
				dig_cdc->regulator[i].optimum_ua;
			return;
		}
	}

	dev_dbg(dig_cdc->dev, "Error: regulator not found:%s\n", name);
}

static void sdm660_tx_mute_update_callback(struct work_struct *work)
{
	struct tx_mute_work *tx_mute_dwork;
	struct snd_soc_codec *codec = NULL;
	struct msm_dig_priv *dig_cdc;
	struct delayed_work *delayed_work;
	u16 tx_vol_ctl_reg = 0;
	u8 decimator = 0, i;

	delayed_work = to_delayed_work(work);
	tx_mute_dwork = container_of(delayed_work, struct tx_mute_work, dwork);
	dig_cdc = tx_mute_dwork->dig_cdc;
	codec = dig_cdc->codec;

	for (i = 0; i < NUM_DECIMATORS; i++) {
		if (dig_cdc->dec_active[i])
			decimator = i + 1;
		if (decimator && decimator <= NUM_DECIMATORS) {
			/* unmute decimators corresponding to Tx DAI's*/
			tx_vol_ctl_reg =
				MSM89XX_CDC_CORE_TX1_VOL_CTL_CFG +
					32 * (decimator - 1);
			if (decimator == DEC_SVA)
				tx_vol_ctl_reg =
					MSM89XX_CDC_CORE_TX5_VOL_CTL_CFG;

			snd_soc_update_bits(codec, tx_vol_ctl_reg,
					0x01, 0x00);
		}
		decimator = 0;
	}
}

static int msm_dig_cdc_soc_probe(struct snd_soc_codec *codec)
{
	struct msm_dig_priv *msm_dig_cdc = dev_get_drvdata(codec->dev);
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int i, ret;

	msm_dig_cdc->codec = codec;

	snd_soc_add_codec_controls(codec, compander_kcontrols,
			ARRAY_SIZE(compander_kcontrols));

	for (i = 0; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].dig_cdc = msm_dig_cdc;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
		msm_dig_cdc->tx_mute_dwork[i].dig_cdc = msm_dig_cdc;
		msm_dig_cdc->tx_mute_dwork[i].decimator = i + 1;
		INIT_DELAYED_WORK(&msm_dig_cdc->tx_mute_dwork[i].dwork,
			sdm660_tx_mute_update_callback);
	}

	for (i = 0; i < MSM89XX_RX_MAX; i++)
		msm_dig_cdc->comp_enabled[i] = COMPANDER_NONE;

	/* Register event notifier */
	msm_dig_cdc->nblock.notifier_call = msm_dig_cdc_event_notify;
	if (msm_dig_cdc->register_notifier) {
		ret = msm_dig_cdc->register_notifier(msm_dig_cdc->handle,
						     &msm_dig_cdc->nblock,
						     true);
		if (ret) {
			pr_err("%s: Failed to register notifier %d\n",
				__func__, ret);
			return ret;
		}
	}

	msm_dig_cdc_update_micbias_regulator(
			msm_dig_cdc,
			on_demand_supply_name[ON_DEMAND_DIGITAL],
			&msm_dig_cdc->on_demand_list[ON_DEMAND_DIGITAL]);
	atomic_set(&msm_dig_cdc->on_demand_list[ON_DEMAND_DIGITAL].ref, 0);

	registered_digcodec = codec;
	if (msm_dig_cdc->no_analog_codec) {
		msm_dig_cdc->fw_data = devm_kzalloc(codec->dev,
					    sizeof(*(msm_dig_cdc->fw_data)),
					    GFP_KERNEL);
		if (!msm_dig_cdc->fw_data)
			return -ENOMEM;

		ret = wcd_cal_create_hwdep(msm_dig_cdc->fw_data,
			WCD9XXX_CODEC_HWDEP_NODE, codec);
		if (ret < 0) {
			dev_err(codec->dev, "%s hwdep failed %d\n", __func__,
				ret);
			return ret;
		}
	}
	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "ADC1_IN");
	snd_soc_dapm_ignore_suspend(dapm, "ADC2_IN");
	snd_soc_dapm_ignore_suspend(dapm, "ADC3_IN");
	snd_soc_dapm_ignore_suspend(dapm, "PDM_OUT_RX1");
	snd_soc_dapm_ignore_suspend(dapm, "PDM_OUT_RX2");
	snd_soc_dapm_ignore_suspend(dapm, "PDM_OUT_RX3");

	snd_soc_dapm_sync(dapm);

	return 0;
}

static int msm_dig_cdc_soc_remove(struct snd_soc_codec *codec)
{
	struct msm_dig_priv *msm_dig_cdc = dev_get_drvdata(codec->dev);

	if (msm_dig_cdc->register_notifier)
		msm_dig_cdc->register_notifier(msm_dig_cdc->handle,
					       &msm_dig_cdc->nblock,
					       false);
	iounmap(msm_dig_cdc->dig_base);
	return 0;
}

static const struct snd_soc_dapm_route audio_dig_map[] = {
	{"RX_I2S_CLK", NULL, "CDC_CONN"},
	{"I2S RX1", NULL, "RX_I2S_CLK"},
	{"I2S RX2", NULL, "RX_I2S_CLK"},
	{"I2S RX3", NULL, "RX_I2S_CLK"},

	{"I2S TX1", NULL, "TX_I2S_CLK"},
	{"I2S TX2", NULL, "TX_I2S_CLK"},
	{"I2S TX3", NULL, "TX_I2S_CLK"},
	{"I2S TX4", NULL, "TX_I2S_CLK"},
	{"I2S TX5", NULL, "TX_I2S_CLK"},
	{"I2S TX6", NULL, "TX_I2S_CLK"},

	{"I2S TX1", NULL, "DEC1 MUX"},
	{"I2S TX2", NULL, "DEC2 MUX"},
	{"I2S TX3", NULL, "I2S TX2 INP1"},
	{"I2S TX4", NULL, "I2S TX2 INP2"},
	{"I2S TX5", NULL, "DEC3 MUX"},
	{"I2S TX6", NULL, "I2S TX3 INP2"},

	{"I2S TX2 INP1", "RX_MIX1", "RX1 MIX2"},
	{"I2S TX2 INP1", "DEC3", "DEC3 MUX"},
	{"I2S TX2 INP2", "RX_MIX2", "RX2 MIX2"},
	{"I2S TX2 INP2", "RX_MIX3", "RX3 MIX1"},
	{"I2S TX2 INP2", "DEC4", "DEC4 MUX"},
	{"I2S TX3 INP2", "DEC4", "DEC4 MUX"},
	{"I2S TX3 INP2", "DEC5", "DEC5 MUX"},

	{"PDM_OUT_RX1", NULL, "RX1 CHAIN"},
	{"PDM_OUT_RX2", NULL, "RX2 CHAIN"},
	{"PDM_OUT_RX3", NULL, "RX3 CHAIN"},

	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},
	{"RX3 CHAIN", NULL, "RX3 MIX1"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX1 MIX2", NULL, "RX1 MIX1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
	{"RX2 MIX2", NULL, "RX2 MIX1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP1"},

	{"RX1 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP1", "IIR2", "IIR2"},
	{"RX1 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "IIR2", "IIR2"},
	{"RX1 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX2 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP1", "IIR2", "IIR2"},
	{"RX2 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "IIR2", "IIR2"},
	{"RX2 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX3 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "IIR2", "IIR2"},
	{"RX3 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "IIR2", "IIR2"},
	{"RX3 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX1 MIX2 INP1", "IIR1", "IIR1"},
	{"RX2 MIX2 INP1", "IIR1", "IIR1"},
	{"RX1 MIX2 INP1", "IIR2", "IIR2"},
	{"RX2 MIX2 INP1", "IIR2", "IIR2"},

		/* Decimator Inputs */
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "DMIC2", "DMIC2"},
	{"DEC1 MUX", "DMIC3", "DMIC3"},
	{"DEC1 MUX", "DMIC4", "DMIC4"},
	{"DEC1 MUX", "ADC1", "ADC1_IN"},
	{"DEC1 MUX", "ADC2", "ADC2_IN"},
	{"DEC1 MUX", "ADC3", "ADC3_IN"},
	{"DEC1 MUX", NULL, "CDC_CONN"},

	{"DEC2 MUX", "DMIC1", "DMIC1"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "DMIC3", "DMIC3"},
	{"DEC2 MUX", "DMIC4", "DMIC4"},
	{"DEC2 MUX", "ADC1", "ADC1_IN"},
	{"DEC2 MUX", "ADC2", "ADC2_IN"},
	{"DEC2 MUX", "ADC3", "ADC3_IN"},
	{"DEC2 MUX", NULL, "CDC_CONN"},

	{"DEC3 MUX", "DMIC1", "DMIC1"},
	{"DEC3 MUX", "DMIC2", "DMIC2"},
	{"DEC3 MUX", "DMIC3", "DMIC3"},
	{"DEC3 MUX", "DMIC4", "DMIC4"},
	{"DEC3 MUX", "ADC1", "ADC1_IN"},
	{"DEC3 MUX", "ADC2", "ADC2_IN"},
	{"DEC3 MUX", "ADC3", "ADC3_IN"},
	{"DEC3 MUX", NULL, "CDC_CONN"},

	{"DEC4 MUX", "DMIC1", "DMIC1"},
	{"DEC4 MUX", "DMIC2", "DMIC2"},
	{"DEC4 MUX", "DMIC3", "DMIC3"},
	{"DEC4 MUX", "DMIC4", "DMIC4"},
	{"DEC4 MUX", "ADC1", "ADC1_IN"},
	{"DEC4 MUX", "ADC2", "ADC2_IN"},
	{"DEC4 MUX", "ADC3", "ADC3_IN"},
	{"DEC4 MUX", NULL, "CDC_CONN"},

	{"DEC5 MUX", "DMIC1", "DMIC1"},
	{"DEC5 MUX", "DMIC2", "DMIC2"},
	{"DEC5 MUX", "DMIC3", "DMIC3"},
	{"DEC5 MUX", "DMIC4", "DMIC4"},
	{"DEC5 MUX", "ADC1", "ADC1_IN"},
	{"DEC5 MUX", "ADC2", "ADC2_IN"},
	{"DEC5 MUX", "ADC3", "ADC3_IN"},
	{"DEC5 MUX", NULL, "CDC_CONN"},

	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP1 MUX", "DEC4", "DEC4 MUX"},
	{"IIR2", NULL, "IIR2 INP1 MUX"},
	{"IIR2 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP1 MUX", "DEC4", "DEC4 MUX"},
};


static const char * const i2s_tx2_inp1_text[] = {
	"ZERO", "RX_MIX1", "DEC3"
};

static const char * const i2s_tx2_inp2_text[] = {
	"ZERO", "RX_MIX2", "RX_MIX3", "DEC4"
};

static const char * const i2s_tx3_inp2_text[] = {
	"DEC4", "DEC5"
};

static const char * const rx_mix1_text[] = {
	"ZERO", "IIR1", "IIR2", "RX1", "RX2", "RX3"
};

static const char * const rx_mix2_text[] = {
	"ZERO", "IIR1", "IIR2"
};

static const char * const dec_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "DMIC1", "DMIC2", "DMIC3", "DMIC4"
};

static const char * const iir_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "RX1", "RX2", "RX3", "DEC3", "DEC4"
};

/* I2S TX MUXes */
static const struct soc_enum i2s_tx2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_I2S_SD1_CTL,
		2, 3, i2s_tx2_inp1_text);

static const struct soc_enum i2s_tx2_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_I2S_SD1_CTL,
		0, 4, i2s_tx2_inp2_text);

static const struct soc_enum i2s_tx3_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_I2S_SD1_CTL,
		4, 2, i2s_tx3_inp2_text);

/* RX1 MIX1 */
static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B2_CTL,
		0, 6, rx_mix1_text);

/* RX1 MIX2 */
static const struct soc_enum rx_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX1_B3_CTL,
		0, 3, rx_mix2_text);

/* RX2 MIX1 */
static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

/* RX2 MIX2 */
static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX2_B3_CTL,
		0, 3, rx_mix2_text);

/* RX3 MIX1 */
static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX3_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

/* DEC */
static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B1_CTL,
		0, 8, dec_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B1_CTL,
		3, 8, dec_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B2_CTL,
		0, 8, dec_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B2_CTL,
		3, 8, dec_mux_text);

static const struct soc_enum decsva_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_TX_B3_CTL,
		0, 8, dec_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_EQ1_B1_CTL,
		0, 8, iir_inp1_text);

static const struct soc_enum iir2_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_CONN_EQ2_B1_CTL,
		0, 8, iir_inp1_text);

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_RX1_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_RX2_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_RX3_B4_CTL, 0, 3, cf_text);

static const struct snd_kcontrol_new rx3_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP1 Mux", rx3_mix1_inp1_chain_enum);

#define MSM89XX_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = msm_dig_cdc_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	MSM89XX_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	MSM89XX_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	MSM89XX_DEC_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	MSM89XX_DEC_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new decsva_mux =
	MSM89XX_DEC_ENUM("DEC5 MUX Mux", decsva_mux_enum);

static const struct snd_kcontrol_new i2s_tx2_inp1_mux =
	SOC_DAPM_ENUM("I2S TX2 INP1 Mux", i2s_tx2_inp1_chain_enum);

static const struct snd_kcontrol_new i2s_tx2_inp2_mux =
	SOC_DAPM_ENUM("I2S TX2 INP2 Mux", i2s_tx2_inp2_chain_enum);

static const struct snd_kcontrol_new i2s_tx3_inp2_mux =
	SOC_DAPM_ENUM("I2S TX3 INP2 Mux", i2s_tx3_inp2_chain_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new iir2_inp1_mux =
	SOC_DAPM_ENUM("IIR2 INP1 Mux", iir2_inp1_mux_enum);

static const struct snd_kcontrol_new rx_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP1 Mux", rx_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP2 Mux", rx_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP3 Mux", rx_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP1 Mux", rx2_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP2 Mux", rx2_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP3 Mux", rx2_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP2 Mux", rx3_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP3 Mux", rx3_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP1 Mux", rx_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP1 Mux", rx2_mix2_inp1_chain_enum);

static const struct snd_soc_dapm_widget msm_dig_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("I2S RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("I2S RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("I2S RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("I2S TX1", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX2", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX3", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX4", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX5", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX6", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MIXER_E("RX1 MIX2", MSM89XX_CDC_CORE_CLK_RX_B1_CTL,
			     MSM89XX_RX1, 0, NULL, 0,
			     msm_dig_cdc_codec_enable_interpolator,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 MIX2", MSM89XX_CDC_CORE_CLK_RX_B1_CTL,
			     MSM89XX_RX2, 0, NULL, 0,
			     msm_dig_cdc_codec_enable_interpolator,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1", MSM89XX_CDC_CORE_CLK_RX_B1_CTL,
			     MSM89XX_RX3, 0, NULL, 0,
			     msm_dig_cdc_codec_enable_interpolator,
			     SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("RX1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX1 CHAIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 CHAIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX3 CHAIN", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("RX1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX2 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX3 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX1 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp1_mux),

	SND_SOC_DAPM_SUPPLY_S("CDC_CONN", -2, MSM89XX_CDC_CORE_CLK_OTHR_CTL,
		2, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("DEC1 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC3 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC4 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC5 MUX",
		MSM89XX_CDC_CORE_CLK_TX_CLK_EN_B1_CTL, 4, 0,
		&decsva_mux, msm_dig_cdc_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA_E("IIR1", MSM89XX_CDC_CORE_CLK_SD_CTL, 0, 0, NULL, 0,
		msm_dig_cdc_codec_set_iir_gain, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("IIR2 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir2_inp1_mux),
	SND_SOC_DAPM_PGA_E("IIR2", MSM89XX_CDC_CORE_CLK_SD_CTL, 1, 0, NULL, 0,
		msm_dig_cdc_codec_set_iir_gain, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK",
		MSM89XX_CDC_CORE_CLK_RX_I2S_CTL, 4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK",
		MSM89XX_CDC_CORE_CLK_TX_I2S_CTL, 4, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS_E("DIGITAL_REGULATOR", SND_SOC_NOPM,
		ON_DEMAND_DIGITAL, 0,
		msm_dig_cdc_enable_on_demand_supply,
		SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("I2S TX2 INP1", SND_SOC_NOPM, 0, 0,
			&i2s_tx2_inp1_mux),
	SND_SOC_DAPM_MUX("I2S TX2 INP2", SND_SOC_NOPM, 0, 0,
			&i2s_tx2_inp2_mux),
	SND_SOC_DAPM_MUX("I2S TX3 INP2", SND_SOC_NOPM, 0, 0,
			&i2s_tx3_inp2_mux),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		msm_dig_cdc_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("ADC1_IN"),
	SND_SOC_DAPM_INPUT("ADC2_IN"),
	SND_SOC_DAPM_INPUT("ADC3_IN"),
	SND_SOC_DAPM_OUTPUT("PDM_OUT_RX1"),
	SND_SOC_DAPM_OUTPUT("PDM_OUT_RX2"),
	SND_SOC_DAPM_OUTPUT("PDM_OUT_RX3"),
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec3_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX3_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec4_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX4_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_decsva_enum =
	SOC_ENUM_SINGLE(MSM89XX_CDC_CORE_TX5_MUX_CTL, 4, 3, cf_text);

static const struct snd_kcontrol_new msm_dig_snd_controls[] = {
	SOC_SINGLE_SX_TLV("DEC1 Volume",
		MSM89XX_CDC_CORE_TX1_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume",
		  MSM89XX_CDC_CORE_TX2_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC3 Volume",
		  MSM89XX_CDC_CORE_TX3_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC4 Volume",
		  MSM89XX_CDC_CORE_TX4_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC5 Volume",
		  MSM89XX_CDC_CORE_TX5_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B1_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B3_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP4 Volume",
			  MSM89XX_CDC_CORE_IIR1_GAIN_B4_CTL,
			0,  -84,	40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR2 INP1 Volume",
			  MSM89XX_CDC_CORE_IIR2_GAIN_B1_CTL,
			0,  -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("RX1 Digital Volume",
		MSM89XX_CDC_CORE_RX1_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume",
		MSM89XX_CDC_CORE_RX2_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume",
		MSM89XX_CDC_CORE_RX3_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),

	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
		msm_dig_cdc_get_iir_enable_audio_mixer,
		msm_dig_cdc_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
		msm_dig_cdc_get_iir_band_audio_mixer,
		msm_dig_cdc_put_iir_band_audio_mixer),

	SOC_SINGLE("RX1 HPF Switch",
		MSM89XX_CDC_CORE_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch",
		MSM89XX_CDC_CORE_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch",
		MSM89XX_CDC_CORE_RX3_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),
	SOC_ENUM("TX3 HPF cut off", cf_dec3_enum),
	SOC_ENUM("TX4 HPF cut off", cf_dec4_enum),
	SOC_ENUM("TX5 HPF cut off", cf_decsva_enum),
	SOC_SINGLE("TX1 HPF Switch",
		MSM89XX_CDC_CORE_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch",
		MSM89XX_CDC_CORE_TX2_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX3 HPF Switch",
		MSM89XX_CDC_CORE_TX3_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX4 HPF Switch",
		MSM89XX_CDC_CORE_TX4_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX5 HPF Switch",
		MSM89XX_CDC_CORE_TX5_MUX_CTL, 3, 1, 0),
};

static int msm_dig_cdc_enable_on_demand_supply(
		struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct msm_dig_priv *msm_dig_cdc = snd_soc_codec_get_drvdata(codec);
	struct on_demand_dig_supply *supply;

	if (w->shift >= ON_DEMAND_DIG_SUPPLIES_MAX) {
		dev_err(codec->dev, "%s: error index >= MAX on demand supplies",
			__func__);
		ret = -EINVAL;
		goto out;
	}
	dev_dbg(codec->dev, "%s: supply: %s event: %d ref: %d\n",
		__func__, on_demand_supply_name[w->shift], event,
		atomic_read(&msm_dig_cdc->on_demand_list[w->shift].ref));

	supply = &msm_dig_cdc->on_demand_list[w->shift];
	if (!supply->supply) {
		dev_err(codec->dev, "%s: err supply not present ond for %d",
			__func__, w->shift);
		goto out;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (atomic_inc_return(&supply->ref) == 1) {
			ret = regulator_set_voltage(supply->supply,
						supply->min_uv,
						supply->max_uv);
			if (ret) {
				dev_err(codec->dev,
					"Setting regulator voltage(en) for micbias with err = %d\n",
					ret);
				goto out;
			}
			ret = regulator_set_load(supply->supply,
						supply->optimum_ua);
			if (ret < 0) {
				dev_err(codec->dev,
					"Setting regulator optimum mode(en) failed for micbias with err = %d\n",
					ret);
				goto out;
			}
			ret = regulator_enable(supply->supply);
			if (ret)
				dev_err(codec->dev, "%s: Failed to enable %s\n",
					__func__,
					on_demand_supply_name[w->shift]);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_read(&supply->ref) == 0) {
			dev_dbg(codec->dev, "%s: %s supply has been disabled.\n",
				__func__, on_demand_supply_name[w->shift]);
			goto out;
		}
		if (atomic_dec_return(&supply->ref) == 0) {
			ret = regulator_disable(supply->supply);
			if (ret)
				dev_err(codec->dev, "%s: Failed to disable %s\n",
					__func__,
					on_demand_supply_name[w->shift]);
			ret = regulator_set_voltage(supply->supply,
					0,
					supply->max_uv);
			if (ret) {
				dev_err(codec->dev,
					"Setting regulator voltage(dis) failed for micbias with err = %d\n",
					ret);
				goto out;
			}
			ret = regulator_set_load(supply->supply, 0);
			if (ret < 0)
				dev_err(codec->dev,
					"Setting regulator optimum mode(dis) failed for micbias with err = %d\n",
					ret);
		}
		break;
	default:
		break;
	}
out:
	return ret;
}

static int msm_digital_cdc_init_supplies(struct msm_dig_priv *msm_cdc)
{
	int ret;
	int i;

	msm_cdc->supplies = devm_kzalloc(msm_cdc->dev,
					sizeof(struct regulator_bulk_data) *
					ARRAY_SIZE(msm_cdc->regulator),
					GFP_KERNEL);
	if (!msm_cdc->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	msm_cdc->num_of_supplies = 0;

	if (ARRAY_SIZE(msm_cdc->regulator) > MAX_REGULATOR) {
		dev_err(msm_cdc->dev, "%s: Array Size out of bound\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(msm_cdc->regulator); i++) {
		if (msm_cdc->regulator[i].name) {
			msm_cdc->supplies[i].supply =
						msm_cdc->regulator[i].name;
			msm_cdc->num_of_supplies++;
		}
	}

	ret = devm_regulator_bulk_get(msm_cdc->dev,
				      msm_cdc->num_of_supplies,
				      msm_cdc->supplies);
	if (ret != 0) {
		dev_err(msm_cdc->dev,
			"Failed to get supplies: err = %d\n",
			ret);
		goto err_supplies;
	}

	for (i = 0; i < msm_cdc->num_of_supplies; i++) {
		if (regulator_count_voltages(
			msm_cdc->supplies[i].consumer) <= 0)
			continue;
		ret = regulator_set_voltage(
					msm_cdc->supplies[i].consumer,
					msm_cdc->regulator[i].min_uv,
					msm_cdc->regulator[i].max_uv);
		if (ret) {
			dev_err(msm_cdc->dev,
				"Setting regulator voltage failed for regulator %s err = %d\n",
				msm_cdc->supplies[i].supply, ret);
			goto err_supplies;
		}
		ret = regulator_set_load(msm_cdc->supplies[i].consumer,
				msm_cdc->regulator[i].optimum_ua);
		if (ret < 0) {
			dev_err(msm_cdc->dev,
				"Setting regulator optimum mode failed for regulator %s err = %d\n",
				msm_cdc->supplies[i].supply, ret);
			goto err_supplies;
		} else {
			ret = 0;
		}
	}

	return ret;

err_supplies:
err:
	return ret;
}

static int msm_digital_cdc_dt_parse_vreg_info(struct device *dev,
	struct dig_cdc_regulator *vreg, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[CODEC_DT_MAX_PROP_SIZE];
	struct device_node *regnode = NULL;
	u32 prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE, "%s-supply",
			vreg_name);
	regnode = of_parse_phandle(dev->of_node, prop_name, 0);

	if (!regnode) {
		dev_err(dev, "Looking up %s property in node %s failed\n",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}

	dev_dbg(dev, "Looking up %s property in node %s\n",
		prop_name, dev->of_node->full_name);

	vreg->name = vreg_name;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-voltage", vreg_name);
	prop = of_get_property(dev->of_node, prop_name, &len);

	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_err(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
		return -EINVAL;
	}
	vreg->min_uv = be32_to_cpup(&prop[0]);
	vreg->max_uv = be32_to_cpup(&prop[1]);

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-current", vreg_name);

	ret = of_property_read_u32(dev->of_node, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -EFAULT;
	}
	vreg->optimum_ua = prop_val;
	dev_dbg(dev, "%s: vol=[%d %d]uV, curr=[%d]uA\n", vreg->name,
		vreg->min_uv, vreg->max_uv, vreg->optimum_ua);
	return 0;
}

static struct snd_soc_dai_ops msm_dig_dai_ops = {
	.hw_params = msm_dig_cdc_hw_params,
};


static struct snd_soc_dai_driver msm_codec_dais[] = {
	{
		.name = "msm_dig_cdc_dai_rx1",
		.id = AIF1_PB,
		.playback = { /* Support maximum range */
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.rate_max = 192000,
			.rate_min = 8000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
	{
		.name = "msm_dig_cdc_dai_tx1",
		.id = AIF1_CAP,
		.capture = { /* Support maximum range */
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
	{
		.name = "msm_dig_cdc_dai_tx2",
		.id = AIF3_SVA,
		.capture = { /* Support maximum range */
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
	{
		.name = "msm_dig_cdc_dai_vifeed",
		.id = AIF2_VIFEED,
		.capture = { /* Support maximum range */
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		 .ops = &msm_dig_dai_ops,
	},
};

static struct regmap *msm_digital_get_regmap(struct device *dev)
{
	struct msm_dig_priv *msm_dig_cdc = dev_get_drvdata(dev);

	return msm_dig_cdc->regmap;
}

static int msm_dig_cdc_suspend(struct snd_soc_codec *codec)
{
	struct msm_dig_priv *msm_dig_cdc = dev_get_drvdata(codec->dev);

	msm_dig_cdc->dapm_bias_off = 1;
	return 0;
}

static int msm_dig_cdc_resume(struct snd_soc_codec *codec)
{
	struct msm_dig_priv *msm_dig_cdc = dev_get_drvdata(codec->dev);

	msm_dig_cdc->dapm_bias_off = 0;
	return 0;
}

static struct snd_soc_codec_driver soc_msm_dig_codec = {
	.probe  = msm_dig_cdc_soc_probe,
	.remove = msm_dig_cdc_soc_remove,
	.suspend = msm_dig_cdc_suspend,
	.resume = msm_dig_cdc_resume,
	.get_regmap = msm_digital_get_regmap,
	.component_driver = {
		.controls = msm_dig_snd_controls,
		.num_controls = ARRAY_SIZE(msm_dig_snd_controls),
		.dapm_widgets = msm_dig_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(msm_dig_dapm_widgets),
		.dapm_routes = audio_dig_map,
		.num_dapm_routes = ARRAY_SIZE(audio_dig_map),
	},
};

const struct regmap_config msm_digital_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 8,
	.lock = enable_digital_callback,
	.unlock = disable_digital_callback,
	.cache_type = REGCACHE_FLAT,
	.reg_defaults = msm89xx_cdc_core_defaults,
	.num_reg_defaults = MSM89XX_CDC_CORE_MAX_REGISTER,
	.writeable_reg = msm89xx_cdc_core_writeable_reg,
	.readable_reg = msm89xx_cdc_core_readable_reg,
	.volatile_reg = msm89xx_cdc_core_volatile_reg,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.max_register = MSM89XX_CDC_CORE_MAX_REGISTER,
};

static bool msm_digital_cdc_populate_dt_pdata(
					struct device *dev,
					struct msm_dig_priv *dig_priv)
{
	int ret, ond_cnt, i, idx = 0;
	const char *name = NULL;
	const char *ond_prop_name = "qcom,cdc-on-demand-supplies";

	ond_cnt = of_property_count_strings(dev->of_node, ond_prop_name);
	if (ond_cnt < 0)
		ond_cnt = 0;

	if (ond_cnt > ARRAY_SIZE(dig_priv->regulator)) {
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < ond_cnt; i++, idx++) {
		ret = of_property_read_string_index(dev->of_node, ond_prop_name,
							i, &name);
		if (ret) {
			dev_err(dev, "%s: err parsing on_demand for %s idx %d\n",
				__func__, ond_prop_name, i);
			goto err;
		}

		dev_dbg(dev, "%s: Found on-demand cdc supply %s\n", __func__,
			name);
		ret = msm_digital_cdc_dt_parse_vreg_info(dev,
						&dig_priv->regulator[idx],
						name);
		if (ret) {
			dev_err(dev, "%s: err parsing vreg on_demand for %s idx %d\n",
				__func__, name, idx);
			goto err;
		}
	}

	return true;
err:
	dev_err(dev, "%s: Failed to populate DT data ret = %d\n",
		__func__, ret);
	return false;
}

static void msm_digital_cdc_disable_supplies(struct msm_dig_priv *msm_cdc)
{
	int i;

	if (!msm_cdc->supplies)
		return;

	regulator_bulk_disable(msm_cdc->num_of_supplies,
			       msm_cdc->supplies);
	for (i = 0; i < msm_cdc->num_of_supplies; i++) {
		if (regulator_count_voltages(
				msm_cdc->supplies[i].consumer) <= 0)
			continue;
		regulator_set_voltage(msm_cdc->supplies[i].consumer, 0,
				msm_cdc->regulator[i].max_uv);
		regulator_set_load(msm_cdc->supplies[i].consumer, 0);
	}
	regulator_bulk_free(msm_cdc->num_of_supplies,
			    msm_cdc->supplies);
	devm_kfree(msm_cdc->dev, msm_cdc->supplies);
}

static int msm_dig_cdc_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	u32 dig_cdc_addr;
	struct msm_dig_priv *msm_dig_cdc;
	struct dig_ctrl_platform_data *pdata = NULL;
	int adsp_state;

	adsp_state = apr_get_subsys_state();
	if ((adsp_state != APR_SUBSYS_LOADED) || (!q6core_is_adsp_ready())) {
		dev_err(&pdev->dev, "Adsp is not loaded yet %d\n",
			adsp_state);
		return -EPROBE_DEFER;
	}

	device_init_wakeup(&pdev->dev, true);

	msm_dig_cdc = devm_kzalloc(&pdev->dev, sizeof(struct msm_dig_priv),
			      GFP_KERNEL);
	if (!msm_dig_cdc)
		return -ENOMEM;

	msm_dig_cdc->dev = &pdev->dev;
	if (pdev->dev.of_node == NULL)
		return -EINVAL;

	if (pdev->dev.of_node)
		msm_dig_cdc->no_analog_codec = of_property_read_bool(
					pdev->dev.of_node,
					"qcom,no-analog-codec");

	if (msm_dig_cdc->no_analog_codec) {
		dev_dbg(&pdev->dev, "%s:Platform data from device tree\n",
				__func__);
		if (msm_digital_cdc_populate_dt_pdata(&pdev->dev,
							msm_dig_cdc)) {
			ret = msm_digital_cdc_init_supplies(
							msm_dig_cdc);
			if (ret) {
				dev_err(&pdev->dev,
				"%s: Fail to enable Codec supplies\n",
				__func__);
				goto rtn;
			}
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		if (!pdata) {
			dev_err(&pdev->dev, "%s: pdata from parent is NULL\n",
				__func__);
			ret = -EINVAL;
			goto err_supplies;
		}
		msm_dig_cdc->update_clkdiv = pdata->update_clkdiv;
		msm_dig_cdc->set_compander_mode = pdata->set_compander_mode;
		msm_dig_cdc->get_cdc_version = pdata->get_cdc_version;
		msm_dig_cdc->handle = pdata->handle;
		msm_dig_cdc->register_notifier = pdata->register_notifier;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "reg",
					&dig_cdc_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		goto err_supplies;
	}

	msm_dig_cdc->dig_base = ioremap(dig_cdc_addr,
					MSM89XX_CDC_CORE_MAX_REGISTER);
	if (msm_dig_cdc->dig_base == NULL) {
		dev_err(&pdev->dev, "%s ioremap failed\n", __func__);
		ret = -ENOMEM;
		goto err_supplies;
	}
	msm_dig_cdc->regmap =
		devm_regmap_init_mmio_clk(&pdev->dev, NULL,
			msm_dig_cdc->dig_base, &msm_digital_regmap_config);

	dev_set_drvdata(&pdev->dev, msm_dig_cdc);
	snd_soc_register_codec(&pdev->dev, &soc_msm_dig_codec,
				msm_codec_dais, ARRAY_SIZE(msm_codec_dais));
	dev_dbg(&pdev->dev, "%s: registered DIG CODEC 0x%x\n",
			__func__, dig_cdc_addr);
	return ret;
err_supplies:
	if (msm_dig_cdc->no_analog_codec)
		msm_digital_cdc_disable_supplies(msm_dig_cdc);
rtn:
	return ret;
}

static int msm_dig_cdc_remove(struct platform_device *pdev)
{
	struct msm_dig_priv *msm_cdc = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);
	msm_digital_cdc_disable_supplies(msm_cdc);
	return 0;
}

#ifdef CONFIG_PM
static int msm_dig_suspend(struct device *dev)
{
	struct msm_asoc_mach_data *pdata;
	struct msm_dig_priv *msm_dig_cdc = dev_get_drvdata(dev);

	if (!registered_digcodec || !msm_dig_cdc) {
		pr_debug("%s:digcodec not initialized, return\n", __func__);
		return 0;
	}
	if (!registered_digcodec->component.card) {
		pr_debug("%s:component not initialized, return\n", __func__);
		return 0;
	}
	pdata = snd_soc_card_get_drvdata(registered_digcodec->component.card);
	if (!pdata) {
		pr_debug("%s:card not initialized, return\n", __func__);
		return 0;
	}
	if (msm_dig_cdc->dapm_bias_off) {
		pr_debug("%s: mclk cnt = %d, mclk_enabled = %d\n",
			__func__, atomic_read(&pdata->int_mclk0_rsc_ref),
			atomic_read(&pdata->int_mclk0_enabled));

		if (atomic_read(&pdata->int_mclk0_enabled) == true) {
			cancel_delayed_work_sync(
				&pdata->disable_int_mclk0_work);
			mutex_lock(&pdata->cdc_int_mclk0_mutex);
			pdata->digital_cdc_core_clk.enable = 0;
			afe_set_lpass_clock_v2(AFE_PORT_ID_INT0_MI2S_RX,
						&pdata->digital_cdc_core_clk);
			atomic_set(&pdata->int_mclk0_enabled, false);
			mutex_unlock(&pdata->cdc_int_mclk0_mutex);
		}
	}

	return 0;
}

static int msm_dig_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops msm_dig_pm_ops = {
	.suspend_late = msm_dig_suspend,
	.resume_early = msm_dig_resume,
};
#endif

static const struct of_device_id msm_dig_cdc_of_match[] = {
	{.compatible = "qcom,msm-digital-codec"},
	{},
};

static struct platform_driver msm_digcodec_driver = {
	.driver                 = {
		.owner          = THIS_MODULE,
		.name           = DRV_NAME,
		.of_match_table = msm_dig_cdc_of_match,
#ifdef CONFIG_PM
	.pm = &msm_dig_pm_ops,
#endif
	},
	.probe                  = msm_dig_cdc_probe,
	.remove                 = msm_dig_cdc_remove,
};
module_platform_driver(msm_digcodec_driver);

MODULE_DESCRIPTION("MSM Audio Digital codec driver");
MODULE_LICENSE("GPL v2");
