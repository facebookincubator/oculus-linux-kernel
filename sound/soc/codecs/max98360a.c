// SPDX-License-Identifier: GPL-2.0
/*
 * max98360a.c -- MAX98360A ALSA SoC Codec driver
 * Copyright (c) 2010-2011,2013-2015 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>

struct max98360a_priv {
	struct device *dev;
	struct snd_soc_component *component;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_sleep;
};

static int max98360a_daiops_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct max98360a_priv *max98360a =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(max98360a->dev, "%s entry.\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = pinctrl_select_state(max98360a->pinctrl,
					max98360a->pin_active);
		if (ret)
			dev_err(max98360a->dev, "set pin_active error.\n");
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = pinctrl_select_state(max98360a->pinctrl,
					max98360a->pin_sleep);
		if (ret)
			dev_err(max98360a->dev, "set pin_sleep error.\n");
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget max98360a_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("Speaker"),
};

static const struct snd_soc_dapm_route max98360a_dapm_routes[] = {
	{"Speaker", NULL, "HiFi Playback"},
};

static int max98360a_component_probe(struct snd_soc_component *component)
{
	struct max98360a_priv *max98360a =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(max98360a->dev, "%s entry.\n", __func__);

	max98360a->pinctrl = devm_pinctrl_get(max98360a->dev);
	if (IS_ERR_OR_NULL(max98360a->pinctrl)) {
		dev_err(max98360a->dev, "failed to get pinctrl.\n");
		return -EINVAL;
	}

	max98360a->pin_active =
		pinctrl_lookup_state(max98360a->pinctrl, "active");
	if (IS_ERR_OR_NULL(max98360a->pin_active)) {
		dev_err(max98360a->dev, "failed to look up active pin state.\n");
		devm_pinctrl_put(max98360a->pinctrl);
		return -EINVAL;
	}

	max98360a->pin_sleep =
		pinctrl_lookup_state(max98360a->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(max98360a->pin_sleep)) {
		dev_err(max98360a->dev, "failed to look up sleep pin state.\n");
		devm_pinctrl_put(max98360a->pinctrl);
		return -EINVAL;
	}

	ret = pinctrl_select_state(max98360a->pinctrl, max98360a->pin_sleep);
	if (ret != 0) {
		dev_err(max98360a->dev, "failed to set pin_sleep, %d\n", ret);
		devm_pinctrl_put(max98360a->pinctrl);
		return ret;
	}

	dev_dbg(max98360a->dev, "%s exit.\n", __func__);

	return 0;
}

static const struct snd_soc_component_driver max98360a_component_driver = {
	.name			= "max98360a",
	.probe			= max98360a_component_probe,
	.dapm_widgets		= max98360a_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(max98360a_dapm_widgets),
	.dapm_routes		= max98360a_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(max98360a_dapm_routes),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static const struct snd_soc_dai_ops max98360a_dai_ops = {
	.trigger	= max98360a_daiops_trigger,
};

static struct snd_soc_dai_driver max98360a_dai_driver = {
	.name = "max98360a-hifi",
	.playback = {
		.stream_name	= "HiFi Playback",
		.formats	= SNDRV_PCM_FMTBIT_S16 |
					SNDRV_PCM_FMTBIT_S24 |
					SNDRV_PCM_FMTBIT_S32,
		.rates		= SNDRV_PCM_RATE_8000 |
					SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_48000 |
					SNDRV_PCM_RATE_96000,
		.rate_min	= 8000,
		.rate_max	= 96000,
		.channels_min	= 1,
		.channels_max	= 2,
	},
	.ops    = &max98360a_dai_ops,
};

static int max98360a_platform_probe(struct platform_device *pdev)
{
	struct max98360a_priv *max98360a;
	int ret;

	max98360a = devm_kzalloc(&pdev->dev, sizeof(struct max98360a_priv),
		GFP_KERNEL);
	if (!max98360a)
		return -ENOMEM;

	max98360a->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, max98360a);

	dev_set_name(&pdev->dev, "max98360a");
	ret = devm_snd_soc_register_component(&pdev->dev,
			&max98360a_component_driver,
			&max98360a_dai_driver, 1);
	if (ret)
		dev_err(&pdev->dev, "failed to register component: %d\n", ret);

	return ret;
}

static int max98360a_platform_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id max98360a_device_id[] = {
	{ .compatible = "maxim,max98360a" },
	{}
};
MODULE_DEVICE_TABLE(of, max98360a_device_id);

static struct platform_driver max98360a_platform_driver = {
	.driver = {
		.name = "max98360a",
		.of_match_table = of_match_ptr(max98360a_device_id),
	},
	.probe	= max98360a_platform_probe,
	.remove = max98360a_platform_remove,
};
module_platform_driver(max98360a_platform_driver);

MODULE_DESCRIPTION("Maxim MAX98360A Codec Driver");
MODULE_LICENSE("GPL v2");
