// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "common.h"
#include "qdsp6/q6afe.h"

#define DEFAULT_SAMPLE_RATE_48K		48000
#define DEFAULT_MCLK_RATE		24576000
#define DEFAULT_BCLK_RATE		12288000

struct sdm845_snd_data {
	struct snd_soc_card *card;
	uint32_t pri_mi2s_clk_count;
	uint32_t quat_tdm_clk_count;
};

static unsigned int tdm_slot_offset[8] = {0, 4, 8, 12, 16, 20, 24, 28};

static int sdm845_tdm_snd_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int channels, slot_width;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		slot_width = 32;
		break;
	default:
		dev_err(rtd->dev, "%s: invalid param format 0x%x\n",
				__func__, params_format(params));
		return -EINVAL;
	}

	channels = params_channels(params);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, 0x3,
				8, slot_width);
		if (ret < 0) {
			dev_err(rtd->dev, "%s: failed to set tdm slot, err:%d\n",
					__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, NULL,
				channels, tdm_slot_offset);
		if (ret < 0) {
			dev_err(rtd->dev, "%s: failed to set channel map, err:%d\n",
					__func__, ret);
			goto end;
		}
	} else {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0xf, 0,
				8, slot_width);
		if (ret < 0) {
			dev_err(rtd->dev, "%s: failed to set tdm slot, err:%d\n",
					__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, channels,
				tdm_slot_offset, 0, NULL);
		if (ret < 0) {
			dev_err(rtd->dev, "%s: failed to set channel map, err:%d\n",
					__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}

static int sdm845_snd_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	switch (cpu_dai->id) {
	case QUATERNARY_TDM_RX_0:
	case QUATERNARY_TDM_TX_0:
		ret = sdm845_tdm_snd_hw_params(substream, params);
		break;
	default:
		pr_err("%s: invalid dai id 0x%x\n", __func__, cpu_dai->id);
		break;
	}
	return ret;
}

static int sdm845_snd_startup(struct snd_pcm_substream *substream)
{
	unsigned int fmt = SND_SOC_DAIFMT_CBS_CFS;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct sdm845_snd_data *data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	switch (cpu_dai->id) {
	case PRIMARY_MI2S_RX:
	case PRIMARY_MI2S_TX:
		if (++(data->pri_mi2s_clk_count) == 1) {
			snd_soc_dai_set_sysclk(cpu_dai,
				Q6AFE_LPASS_CLK_ID_MCLK_1,
				DEFAULT_MCLK_RATE, SNDRV_PCM_STREAM_PLAYBACK);
			snd_soc_dai_set_sysclk(cpu_dai,
				Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
				DEFAULT_BCLK_RATE, SNDRV_PCM_STREAM_PLAYBACK);
		}
		snd_soc_dai_set_fmt(cpu_dai, fmt);
		break;

	case QUATERNARY_TDM_RX_0:
	case QUATERNARY_TDM_TX_0:
		if (++(data->quat_tdm_clk_count) == 1) {
			snd_soc_dai_set_sysclk(cpu_dai,
				Q6AFE_LPASS_CLK_ID_QUAD_TDM_IBIT,
				DEFAULT_BCLK_RATE, SNDRV_PCM_STREAM_PLAYBACK);
		}
		break;

	default:
		pr_err("%s: invalid dai id 0x%x\n", __func__, cpu_dai->id);
		break;
	}
	return 0;
}

static void  sdm845_snd_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct sdm845_snd_data *data = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	switch (cpu_dai->id) {
	case PRIMARY_MI2S_RX:
	case PRIMARY_MI2S_TX:
		if (--(data->pri_mi2s_clk_count) == 0) {
			snd_soc_dai_set_sysclk(cpu_dai,
				Q6AFE_LPASS_CLK_ID_MCLK_1,
				0, SNDRV_PCM_STREAM_PLAYBACK);
			snd_soc_dai_set_sysclk(cpu_dai,
				Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
				0, SNDRV_PCM_STREAM_PLAYBACK);
		};
		break;

	case QUATERNARY_TDM_RX_0:
	case QUATERNARY_TDM_TX_0:
		if (--(data->quat_tdm_clk_count) == 0) {
			snd_soc_dai_set_sysclk(cpu_dai,
				Q6AFE_LPASS_CLK_ID_QUAD_TDM_IBIT,
				0, SNDRV_PCM_STREAM_PLAYBACK);
		}
		break;

	default:
		pr_err("%s: invalid dai id 0x%x\n", __func__, cpu_dai->id);
		break;
	}
}

static struct snd_soc_ops sdm845_be_ops = {
	.hw_params = sdm845_snd_hw_params,
	.startup = sdm845_snd_startup,
	.shutdown = sdm845_snd_shutdown,
};

static int sdm845_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	rate->min = rate->max = DEFAULT_SAMPLE_RATE_48K;
	channels->min = channels->max = 2;
	snd_mask_set_format(fmt, SNDRV_PCM_FORMAT_S16_LE);

	return 0;
}

static void sdm845_add_be_ops(struct snd_soc_card *card)
{
	struct snd_soc_dai_link *link = card->dai_link;
	int i, num_links = card->num_links;

	for (i = 0; i < num_links; i++) {
		if (link->no_pcm == 1) {
			link->ops = &sdm845_be_ops;
			link->be_hw_params_fixup = sdm845_be_hw_params_fixup;
		}
		link++;
	}
}

static int sdm845_snd_platform_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct sdm845_snd_data *data;
	struct device *dev = &pdev->dev;
	int ret;

	card = kzalloc(sizeof(*card), GFP_KERNEL);
	if (!card)
		return -ENOMEM;

	/* Allocate the private data */
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto data_alloc_fail;
	}

	card->dev = dev;
	card->owner = THIS_MODULE;
	dev_set_drvdata(dev, card);
	ret = qcom_snd_parse_of(card);
	if (ret) {
		dev_err(dev, "Error parsing OF data\n");
		goto parse_dt_fail;
	}

	data->card = card;
	snd_soc_card_set_drvdata(card, data);

	sdm845_add_be_ops(card);
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(dev, "Sound card registration failed\n");
		goto register_card_fail;
	}
	return ret;

register_card_fail:
	kfree(card->dai_link);
parse_dt_fail:
	kfree(data);
data_alloc_fail:
	kfree(card);
	return ret;
}

static int sdm845_snd_platform_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = dev_get_drvdata(&pdev->dev);
	struct sdm845_snd_data *data = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	kfree(card->dai_link);
	kfree(data);
	kfree(card);
	return 0;
}

static const struct of_device_id sdm845_snd_device_id[]  = {
	{ .compatible = "qcom,sdm845-sndcard" },
	{},
};
MODULE_DEVICE_TABLE(of, sdm845_snd_device_id);

static struct platform_driver sdm845_snd_driver = {
	.probe = sdm845_snd_platform_probe,
	.remove = sdm845_snd_platform_remove,
	.driver = {
		.name = "msm-snd-sdm845",
		.of_match_table = sdm845_snd_device_id,
	},
};
module_platform_driver(sdm845_snd_driver);

MODULE_DESCRIPTION("sdm845 ASoC Machine Driver");
MODULE_LICENSE("GPL v2");
