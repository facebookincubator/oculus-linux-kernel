// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <soc/snd_event.h>
#include <linux/pm_runtime.h>
#include <soc/swr-common.h>
#include <dsp/digital-cdc-rsc-mgr.h>
#include "lpass-cdc.h"
#include "internal.h"
#include "lpass-cdc-clk-rsc.h"
#include <linux/qti-regmap-debugfs.h>

#define DRV_NAME "lpass-cdc"

#define LPASS_CDC_VERSION_ENTRY_SIZE 32
#define LPASS_CDC_STRING_LEN 80

static const struct snd_soc_component_driver lpass_cdc;

/* pm runtime auto suspend timer in msecs */
#define LPASS_CDC_AUTO_SUSPEND_DELAY          100 /* delay in msec */

/* MCLK_MUX table for all macros */
static u16 lpass_cdc_mclk_mux_tbl[MAX_MACRO][MCLK_MUX_MAX] = {
	{TX_MACRO, VA_MACRO},
	{TX_MACRO, RX_MACRO},
	{TX_MACRO, WSA_MACRO},
	{TX_MACRO, VA_MACRO},
};

static bool lpass_cdc_is_valid_codec_dev(struct device *dev);

int lpass_cdc_set_port_map(struct snd_soc_component *component,
			u32 size, void *data)
{
	struct lpass_cdc_priv *priv = NULL;
	struct swr_mstr_port_map *map = NULL;
	u16 idx;

	if (!component || (size == 0) || !data)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (!priv)
		return -EINVAL;

	if (!lpass_cdc_is_valid_codec_dev(priv->dev)) {
		dev_err(priv->dev, "%s: invalid codec\n", __func__);
		return -EINVAL;
	}
	map = (struct swr_mstr_port_map *)data;

	for (idx = 0; idx < size; idx++) {
		if (priv->macro_params[map->id].set_port_map)
			priv->macro_params[map->id].set_port_map(component,
						map->uc,
						SWR_MSTR_PORT_LEN,
						map->swr_port_params);
		map += 1;
	}

	return 0;
}
EXPORT_SYMBOL(lpass_cdc_set_port_map);

static void lpass_cdc_ahb_write_device(char __iomem *io_base,
				    u16 reg, u8 value)
{
	u32 temp = (u32)(value) & 0x000000FF;

	iowrite32(temp, io_base + reg);
}

static void lpass_cdc_ahb_read_device(char __iomem *io_base,
				   u16 reg, u8 *value)
{
	u32 temp;

	temp = ioread32(io_base + reg);
	*value = (u8)temp;
}

static int __lpass_cdc_reg_read(struct lpass_cdc_priv *priv,
			     u16 macro_id, u16 reg, u8 *val)
{
	int ret = 0;

	mutex_lock(&priv->clk_lock);
	if (!priv->dev_up) {
		dev_dbg_ratelimited(priv->dev,
			"%s: SSR in progress, exit\n", __func__);
		ret = -EINVAL;
		goto ssr_err;
	}

	if (priv->macro_params[VA_MACRO].dev) {
		pm_runtime_get_sync(priv->macro_params[VA_MACRO].dev);
		mutex_lock(&priv->vote_lock);
		if (((priv->lpass_core_hw_vote && !priv->core_hw_vote_count) ||
			(priv->lpass_audio_hw_vote && !priv->core_audio_vote_count))) {
			goto vote_err;
		}
	}
	lpass_cdc_ahb_read_device(
		priv->macro_params[macro_id].io_base, reg, val);

vote_err:
	if (priv->macro_params[VA_MACRO].dev) {
		mutex_unlock(&priv->vote_lock);
		pm_runtime_mark_last_busy(priv->macro_params[VA_MACRO].dev);
		pm_runtime_put_autosuspend(priv->macro_params[VA_MACRO].dev);
	}
ssr_err:
	mutex_unlock(&priv->clk_lock);
	return ret;
}

static int __lpass_cdc_reg_write(struct lpass_cdc_priv *priv,
			      u16 macro_id, u16 reg, u8 val)
{
	int ret = 0;

	mutex_lock(&priv->clk_lock);
	if (!priv->dev_up) {
		dev_dbg_ratelimited(priv->dev,
			"%s: SSR in progress, exit\n", __func__);
		ret = -EINVAL;
		goto ssr_err;
	}
	if (priv->macro_params[VA_MACRO].dev) {
		pm_runtime_get_sync(priv->macro_params[VA_MACRO].dev);
		mutex_lock(&priv->vote_lock);
		if (((priv->lpass_core_hw_vote && !priv->core_hw_vote_count) ||
			(priv->lpass_audio_hw_vote && !priv->core_audio_vote_count))) {
			goto vote_err;
		}
	}
	lpass_cdc_ahb_write_device(
		priv->macro_params[macro_id].io_base, reg, val);

vote_err:
	if (priv->macro_params[VA_MACRO].dev) {
		mutex_unlock(&priv->vote_lock);
		pm_runtime_mark_last_busy(priv->macro_params[VA_MACRO].dev);
		pm_runtime_put_autosuspend(priv->macro_params[VA_MACRO].dev);
	}
ssr_err:
	mutex_unlock(&priv->clk_lock);
	return ret;
}

static int lpass_cdc_update_wcd_event(void *handle, u16 event, u32 data)
{
	struct lpass_cdc_priv *priv = (struct lpass_cdc_priv *)handle;

	if (!priv) {
		pr_err("%s:Invalid lpass_cdc priv handle\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case WCD_LPASS_CDC_EVT_RX_MUTE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_RX_MUTE, data);
		break;
	case WCD_LPASS_CDC_EVT_IMPED_TRUE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_IMPED_TRUE, data);
		break;
	case WCD_LPASS_CDC_EVT_IMPED_FALSE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_IMPED_FALSE, data);
		break;
	case WCD_LPASS_CDC_EVT_RX_COMPANDER_SOFT_RST:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_RX_COMPANDER_SOFT_RST, data);
		break;
	case WCD_LPASS_CDC_EVT_BCS_CLK_OFF:
		if (priv->macro_params[TX_MACRO].event_handler)
			priv->macro_params[TX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_BCS_CLK_OFF, data);
		break;
	case WCD_LPASS_CDC_EVT_RX_PA_GAIN_UPDATE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_RX_PA_GAIN_UPDATE,
				data);
		break;
	case WCD_LPASS_CDC_EVT_HPHL_HD2_ENABLE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_HPHL_HD2_ENABLE, data);
		break;
	case WCD_LPASS_CDC_EVT_HPHR_HD2_ENABLE:
		if (priv->macro_params[RX_MACRO].event_handler)
			priv->macro_params[RX_MACRO].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_HPHR_HD2_ENABLE, data);
		break;
	default:
		dev_err(priv->dev, "%s: Invalid event %d trigger from wcd\n",
			__func__, event);
		return -EINVAL;
	}
	return 0;
}

static int lpass_cdc_register_notifier(void *handle,
					struct notifier_block *nblock,
					bool enable)
{
	struct lpass_cdc_priv *priv = (struct lpass_cdc_priv *)handle;

	if (!priv) {
		pr_err("%s: lpass_cdc priv is null\n", __func__);
		return -EINVAL;
	}
	if (enable)
		return blocking_notifier_chain_register(&priv->notifier,
							nblock);

	return blocking_notifier_chain_unregister(&priv->notifier,
						  nblock);
}

static void lpass_cdc_notifier_call(struct lpass_cdc_priv *priv,
				     u32 data)
{
	dev_dbg(priv->dev, "%s: notifier call, data:%d\n", __func__, data);
	blocking_notifier_call_chain(&priv->notifier,
				     data, (void *)priv->wcd_dev);
}

static bool lpass_cdc_is_valid_child_dev(struct device *dev)
{
	if (of_device_is_compatible(dev->parent->of_node, "qcom,lpass-cdc"))
		return true;

	return false;
}

static bool lpass_cdc_is_valid_codec_dev(struct device *dev)
{
	if (of_device_is_compatible(dev->of_node, "qcom,lpass-cdc"))
		return true;

	return false;
}

/**
 * lpass_cdc_clear_amic_tx_hold - clears AMIC register on analog codec
 *
 * @dev: lpass_cdc device ptr.
 *
 */
void lpass_cdc_clear_amic_tx_hold(struct device *dev, u16 adc_n)
{
	struct lpass_cdc_priv *priv;
	u16 event;
	u16 amic = 0;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return;
	}

	if (!lpass_cdc_is_valid_codec_dev(dev)) {
		pr_err("%s: invalid codec\n", __func__);
		return;
	}
	priv = dev_get_drvdata(dev);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return;
	}
	event = LPASS_CDC_WCD_EVT_TX_CH_HOLD_CLEAR;
	if (adc_n == LPASS_CDC_ADC0)
		amic = 0x1;
	else if (adc_n == LPASS_CDC_ADC1)
		amic = 0x2;
	else if (adc_n == LPASS_CDC_ADC2)
		amic = 0x2;
	else if (adc_n == LPASS_CDC_ADC3)
		amic = 0x3;
	else
		return;

	lpass_cdc_notifier_call(priv, (amic << 0x10 | event));
}
EXPORT_SYMBOL(lpass_cdc_clear_amic_tx_hold);

/**
 * lpass_cdc_get_device_ptr - Get child or macro device ptr
 *
 * @dev: lpass_cdc device ptr.
 * @macro_id: ID of macro calling this API.
 *
 * Returns dev ptr on success or NULL on error.
 */
struct device *lpass_cdc_get_device_ptr(struct device *dev, u16 macro_id)
{
	struct lpass_cdc_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return NULL;
	}

	if (!lpass_cdc_is_valid_codec_dev(dev)) {
		pr_err("%s: invalid codec\n", __func__);
		return NULL;
	}
	priv = dev_get_drvdata(dev);
	if (!priv || (macro_id >= MAX_MACRO)) {
		dev_err(dev, "%s: priv is null or invalid macro\n", __func__);
		return NULL;
	}

	return priv->macro_params[macro_id].dev;
}
EXPORT_SYMBOL(lpass_cdc_get_device_ptr);

/**
 * lpass_cdc_get_rsc_clk_device_ptr - Get rsc clk device ptr
 *
 * @dev: lpass_cdc device ptr.
 *
 * Returns dev ptr on success or NULL on error.
 */
struct device *lpass_cdc_get_rsc_clk_device_ptr(struct device *dev)
{
	struct lpass_cdc_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return NULL;
	}

	if (!lpass_cdc_is_valid_codec_dev(dev)) {
		pr_err("%s: invalid codec\n", __func__);
		return NULL;
	}
	priv = dev_get_drvdata(dev);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return NULL;
	}

	return priv->clk_dev;
}
EXPORT_SYMBOL(lpass_cdc_get_rsc_clk_device_ptr);

static int lpass_cdc_copy_dais_from_macro(struct lpass_cdc_priv *priv)
{
	struct snd_soc_dai_driver *dai_ptr;
	u16 macro_idx;

	/* memcpy into lpass_cdc_dais all macro dais */
	if (!priv->lpass_cdc_dais)
		priv->lpass_cdc_dais = devm_kzalloc(priv->dev,
						priv->num_dais *
						sizeof(
						struct snd_soc_dai_driver),
						GFP_KERNEL);
	if (!priv->lpass_cdc_dais)
		return -ENOMEM;

	dai_ptr = priv->lpass_cdc_dais;

	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (priv->macro_params[macro_idx].dai_ptr) {
			memcpy(dai_ptr,
			       priv->macro_params[macro_idx].dai_ptr,
			       priv->macro_params[macro_idx].num_dais *
			       sizeof(struct snd_soc_dai_driver));
			dai_ptr += priv->macro_params[macro_idx].num_dais;
		}
	}
	return 0;
}

/**
 * lpass_cdc_register_res_clk - Registers rsc clk driver to lpass_cdc
 *
 * @dev: rsc clk device ptr.
 * @rsc_clk_cb: event handler callback for notifications like SSR
 *
 * Returns 0 on success or -EINVAL on error.
 */
int lpass_cdc_register_res_clk(struct device *dev, rsc_clk_cb_t rsc_clk_cb)
{
	struct lpass_cdc_priv *priv;

	if (!dev || !rsc_clk_cb) {
		pr_err("%s: dev or rsc_clk_cb is null\n", __func__);
		return -EINVAL;
	}
	if (!lpass_cdc_is_valid_child_dev(dev)) {
		dev_err(dev, "%s: child device :%pK not added yet\n",
			__func__, dev);
		return -EINVAL;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return -EINVAL;
	}

	priv->clk_dev = dev;
	priv->rsc_clk_cb = rsc_clk_cb;

	return 0;
}
EXPORT_SYMBOL(lpass_cdc_register_res_clk);

/**
 * lpass_cdc_unregister_res_clk - Unregisters rsc clk driver from lpass_cdc
 *
 * @dev: resource clk device ptr.
 */
void lpass_cdc_unregister_res_clk(struct device *dev)
{
	struct lpass_cdc_priv *priv;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return;
	}
	if (!lpass_cdc_is_valid_child_dev(dev)) {
		dev_err(dev, "%s: child device :%pK not added\n",
			__func__, dev);
		return;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return;
	}

	priv->clk_dev = NULL;
	priv->rsc_clk_cb = NULL;
}
EXPORT_SYMBOL(lpass_cdc_unregister_res_clk);

static u8 lpass_cdc_dmic_clk_div_get(struct snd_soc_component *component,
				   int mode)
{
	struct lpass_cdc_priv* priv = snd_soc_component_get_drvdata(component);
	int macro = (mode ? VA_MACRO : TX_MACRO);
	int ret = 0;

	if (priv->macro_params[macro].clk_div_get) {
		ret = priv->macro_params[macro].clk_div_get(component);
		if (ret >= 0)
			return ret;
	}

	return 1;
}

int lpass_cdc_dmic_clk_enable(struct snd_soc_component *component,
			   u32 dmic, u32 tx_mode, bool enable)
{
	struct lpass_cdc_priv* priv = snd_soc_component_get_drvdata(component);
	u8  dmic_clk_en = 0x01;
	u16 dmic_clk_reg = 0;
	s32 *dmic_clk_cnt = NULL;
	u8 *dmic_clk_div = NULL;
	u8 freq_change_mask = 0;
	u8 clk_div = 0;

	dev_dbg(component->dev, "%s: enable: %d, tx_mode:%d, dmic: %d\n",
		__func__, enable, tx_mode, dmic);

	switch (dmic) {
	case 0:
	case 1:
		dmic_clk_cnt = &(priv->dmic_0_1_clk_cnt);
		dmic_clk_div = &(priv->dmic_0_1_clk_div);
		dmic_clk_reg = LPASS_CDC_VA_TOP_CSR_DMIC0_CTL;
		freq_change_mask = 0x01;
		break;
	case 2:
	case 3:
		dmic_clk_cnt = &(priv->dmic_2_3_clk_cnt);
		dmic_clk_div = &(priv->dmic_2_3_clk_div);
		dmic_clk_reg = LPASS_CDC_VA_TOP_CSR_DMIC1_CTL;
		freq_change_mask = 0x02;
		break;
	case 4:
	case 5:
		dmic_clk_cnt = &(priv->dmic_4_5_clk_cnt);
		dmic_clk_div = &(priv->dmic_4_5_clk_div);
		dmic_clk_reg = LPASS_CDC_VA_TOP_CSR_DMIC2_CTL;
		freq_change_mask = 0x04;
		break;
	case 6:
	case 7:
		dmic_clk_cnt = &(priv->dmic_6_7_clk_cnt);
		dmic_clk_div = &(priv->dmic_6_7_clk_div);
		dmic_clk_reg = LPASS_CDC_VA_TOP_CSR_DMIC3_CTL;
		freq_change_mask = 0x08;
		break;
	default:
		dev_err(component->dev, "%s: Invalid DMIC Selection\n",
			__func__);
		return -EINVAL;
	}
	dev_dbg(component->dev, "%s: DMIC%d dmic_clk_cnt %d\n",
			__func__, dmic, *dmic_clk_cnt);
	if (enable) {
		clk_div = lpass_cdc_dmic_clk_div_get(component, tx_mode);
		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1) {
			snd_soc_component_update_bits(component,
					LPASS_CDC_VA_TOP_CSR_DMIC_CFG,
					0x80, 0x00);
			snd_soc_component_update_bits(component, dmic_clk_reg,
						0x0E, clk_div << 0x1);
			snd_soc_component_update_bits(component, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);
		} else {
			if (*dmic_clk_div > clk_div) {
				snd_soc_component_update_bits(component,
						LPASS_CDC_VA_TOP_CSR_DMIC_CFG,
						freq_change_mask, freq_change_mask);
				snd_soc_component_update_bits(component, dmic_clk_reg,
						0x0E, clk_div << 0x1);
				snd_soc_component_update_bits(component,
						LPASS_CDC_VA_TOP_CSR_DMIC_CFG,
						freq_change_mask, 0x00);
			} else {
				clk_div = *dmic_clk_div;
			}
		}
		*dmic_clk_div = clk_div;
	} else {
		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0) {
			snd_soc_component_update_bits(component, dmic_clk_reg,
					dmic_clk_en, 0);
			clk_div = 0;
			snd_soc_component_update_bits(component, dmic_clk_reg,
							0x0E, clk_div << 0x1);
		} else {
			clk_div = lpass_cdc_dmic_clk_div_get(component, tx_mode);
			if (*dmic_clk_div > clk_div) {
				clk_div = lpass_cdc_dmic_clk_div_get(component, !tx_mode);
				snd_soc_component_update_bits(component,
							LPASS_CDC_VA_TOP_CSR_DMIC_CFG,
							freq_change_mask, freq_change_mask);
				snd_soc_component_update_bits(component, dmic_clk_reg,
								0x0E, clk_div << 0x1);
				snd_soc_component_update_bits(component,
							LPASS_CDC_VA_TOP_CSR_DMIC_CFG,
							freq_change_mask, 0x00);
			} else {
				clk_div = *dmic_clk_div;
			}
		}
		*dmic_clk_div = clk_div;
	}

	return 0;
}
EXPORT_SYMBOL(lpass_cdc_dmic_clk_enable);

bool lpass_cdc_is_va_macro_registered(struct device *dev)
{
	struct lpass_cdc_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return false;
	}
	if (!lpass_cdc_is_valid_child_dev(dev)) {
		dev_err(dev, "%s: child device calling is not added yet\n",
			__func__);
		return false;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return false;
	}
	return priv->macros_supported[VA_MACRO];
}
EXPORT_SYMBOL(lpass_cdc_is_va_macro_registered);

/**
 * lpass_cdc_register_macro - Registers macro to lpass_cdc
 *
 * @dev: macro device ptr.
 * @macro_id: ID of macro calling this API.
 * @ops: macro params to register.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int lpass_cdc_register_macro(struct device *dev, u16 macro_id,
			  struct macro_ops *ops)
{
	struct lpass_cdc_priv *priv;
	int ret = -EINVAL;

	if (!dev || !ops) {
		pr_err("%s: dev or ops is null\n", __func__);
		return -EINVAL;
	}
	if (!lpass_cdc_is_valid_child_dev(dev)) {
		dev_err(dev, "%s: child device for macro:%d not added yet\n",
			__func__, macro_id);
		return -EINVAL;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv || (macro_id >= MAX_MACRO)) {
		dev_err(dev, "%s: priv is null or invalid macro\n", __func__);
		return -EINVAL;
	}

	priv->macro_params[macro_id].clk_id_req = ops->clk_id_req;
	priv->macro_params[macro_id].default_clk_id = ops->default_clk_id;
	priv->macro_params[macro_id].init = ops->init;
	priv->macro_params[macro_id].exit = ops->exit;
	priv->macro_params[macro_id].io_base = ops->io_base;
	priv->macro_params[macro_id].num_dais = ops->num_dais;
	priv->macro_params[macro_id].dai_ptr = ops->dai_ptr;
	priv->macro_params[macro_id].event_handler = ops->event_handler;
	priv->macro_params[macro_id].set_port_map = ops->set_port_map;
	priv->macro_params[macro_id].dev = dev;
	priv->current_mclk_mux_macro[macro_id] =
				lpass_cdc_mclk_mux_tbl[macro_id][MCLK_MUX0];
	if (macro_id == TX_MACRO) {
		priv->macro_params[macro_id].reg_wake_irq = ops->reg_wake_irq;
		priv->macro_params[macro_id].reg_evt_listener =
							ops->reg_evt_listener;
		priv->macro_params[macro_id].clk_enable = ops->clk_enable;
	}
	if (macro_id == TX_MACRO || macro_id == VA_MACRO)
		priv->macro_params[macro_id].clk_div_get = ops->clk_div_get;

	if (macro_id == VA_MACRO)
		priv->macro_params[macro_id].reg_wake_irq =
						ops->reg_wake_irq;
	priv->num_dais += ops->num_dais;
	priv->num_macros_registered++;
	priv->macros_supported[macro_id] = true;

	dev_info(dev, "%s: register macro successful:%d\n", __func__, macro_id);

	if (priv->num_macros_registered == priv->num_macros) {
		ret = lpass_cdc_copy_dais_from_macro(priv);
		if (ret < 0) {
			dev_err(dev, "%s: copy_dais failed\n", __func__);
			return ret;
		}
		if (priv->macros_supported[TX_MACRO] == false) {
			lpass_cdc_mclk_mux_tbl[WSA_MACRO][MCLK_MUX0] = WSA_MACRO;
			priv->current_mclk_mux_macro[WSA_MACRO] = WSA_MACRO;
			lpass_cdc_mclk_mux_tbl[VA_MACRO][MCLK_MUX0] = VA_MACRO;
			priv->current_mclk_mux_macro[VA_MACRO] = VA_MACRO;
		}
		ret = snd_soc_register_component(dev->parent, &lpass_cdc,
				priv->lpass_cdc_dais, priv->num_dais);
		if (ret < 0) {
			dev_err(dev, "%s: register codec failed\n", __func__);
			return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL(lpass_cdc_register_macro);

/**
 * lpass_cdc_unregister_macro - De-Register macro from lpass_cdc
 *
 * @dev: macro device ptr.
 * @macro_id: ID of macro calling this API.
 *
 */
void lpass_cdc_unregister_macro(struct device *dev, u16 macro_id)
{
	struct lpass_cdc_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return;
	}
	if (!lpass_cdc_is_valid_child_dev(dev)) {
		dev_err(dev, "%s: macro:%d not in valid registered macro-list\n",
			__func__, macro_id);
		return;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv || (macro_id >= MAX_MACRO)) {
		dev_err(dev, "%s: priv is null or invalid macro\n", __func__);
		return;
	}

	priv->macro_params[macro_id].init = NULL;
	priv->macro_params[macro_id].num_dais = 0;
	priv->macro_params[macro_id].dai_ptr = NULL;
	priv->macro_params[macro_id].event_handler = NULL;
	priv->macro_params[macro_id].dev = NULL;
	if (macro_id == TX_MACRO) {
		priv->macro_params[macro_id].reg_wake_irq = NULL;
		priv->macro_params[macro_id].reg_evt_listener = NULL;
		priv->macro_params[macro_id].clk_enable = NULL;
	}
	if (macro_id == TX_MACRO || macro_id == VA_MACRO)
		priv->macro_params[macro_id].clk_div_get = NULL;

	priv->num_dais -= priv->macro_params[macro_id].num_dais;
	priv->num_macros_registered--;

	/* UNREGISTER CODEC HERE */
	if (priv->num_macros - 1 == priv->num_macros_registered)
		snd_soc_unregister_component(dev->parent);
}
EXPORT_SYMBOL(lpass_cdc_unregister_macro);

void lpass_cdc_wsa_pa_on(struct device *dev, bool adie_lb)
{
	struct lpass_cdc_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return;
	}
	if (!lpass_cdc_is_valid_child_dev(dev)) {
		dev_err(dev, "%s: not a valid child dev\n",
			__func__);
		return;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return;
	}
	if (adie_lb)
		lpass_cdc_notifier_call(priv,
			LPASS_CDC_WCD_EVT_PA_ON_POST_FSCLK_ADIE_LB);
	else
		lpass_cdc_notifier_call(priv,
			LPASS_CDC_WCD_EVT_PA_ON_POST_FSCLK);
}
EXPORT_SYMBOL(lpass_cdc_wsa_pa_on);

int lpass_cdc_get_version(struct device *dev)
{
	struct lpass_cdc_priv *priv;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return -EINVAL;
	}
	if (!lpass_cdc_is_valid_child_dev(dev)) {
		dev_err(dev, "%s: child device for macro not added yet\n",
			__func__);
		return -EINVAL;
	}
	priv = dev_get_drvdata(dev->parent);
	if (!priv) {
		dev_err(dev, "%s: priv is null\n", __func__);
		return -EINVAL;
	}
	return priv->version;
}
EXPORT_SYMBOL(lpass_cdc_get_version);

static ssize_t lpass_cdc_version_read(struct snd_info_entry *entry,
				   void *file_private_data,
				   struct file *file,
				   char __user *buf, size_t count,
				   loff_t pos)
{
	struct lpass_cdc_priv *priv;
	char buffer[LPASS_CDC_VERSION_ENTRY_SIZE];
	int len = 0;

	priv = (struct lpass_cdc_priv *) entry->private_data;
	if (!priv) {
		pr_err("%s: lpass_cdc priv is null\n", __func__);
		return -EINVAL;
	}

	switch (priv->version) {
	case LPASS_CDC_VERSION_1_0:
		len = snprintf(buffer, sizeof(buffer), "LPASS-CDC_1_0\n");
		break;
	case LPASS_CDC_VERSION_1_1:
		len = snprintf(buffer, sizeof(buffer), "LPASS-CDC_1_1\n");
		break;
	case LPASS_CDC_VERSION_1_2:
		len = snprintf(buffer, sizeof(buffer), "LPASS-CDC_1_2\n");
		break;
	case LPASS_CDC_VERSION_2_1:
		len = snprintf(buffer, sizeof(buffer), "LPASS-CDC_2_1\n");
		break;
	case LPASS_CDC_VERSION_2_5:
		len = snprintf(buffer, sizeof(buffer), "LPASS-CDC_2_5\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static int lpass_cdc_ssr_enable(struct device *dev, void *data)
{
	struct lpass_cdc_priv *priv = data;
	int macro_idx;

	if (priv->initial_boot) {
		priv->initial_boot = false;
		return 0;
	}

	if (priv->rsc_clk_cb)
		priv->rsc_clk_cb(priv->clk_dev, LPASS_CDC_MACRO_EVT_SSR_UP);

	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (priv->macro_params[macro_idx].event_handler)
			priv->macro_params[macro_idx].event_handler(
				priv->component,
				LPASS_CDC_MACRO_EVT_CLK_RESET, 0x0);
	}
	trace_printk("%s: clk count reset\n", __func__);

	mutex_lock(&priv->clk_lock);
	priv->pre_dev_up = true;
	mutex_unlock(&priv->clk_lock);

	if (priv->rsc_clk_cb)
		priv->rsc_clk_cb(priv->clk_dev, LPASS_CDC_MACRO_EVT_SSR_GFMUX_UP);

	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (!priv->macro_params[macro_idx].event_handler)
			continue;
		priv->macro_params[macro_idx].event_handler(
			priv->component,
			LPASS_CDC_MACRO_EVT_PRE_SSR_UP, 0x0);
	}

	regcache_cache_only(priv->regmap, false);
	mutex_lock(&priv->clk_lock);
	priv->dev_up = true;
	mutex_unlock(&priv->clk_lock);
	regcache_mark_dirty(priv->regmap);
	lpass_cdc_clk_rsc_enable_all_clocks(priv->clk_dev, true);
	regcache_sync(priv->regmap);
	/* Add a 100usec sleep to ensure last register write is done */
	usleep_range(100,110);
	lpass_cdc_clk_rsc_enable_all_clocks(priv->clk_dev, false);
	trace_printk("%s: regcache_sync done\n", __func__);
	/* call ssr event for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (!priv->macro_params[macro_idx].event_handler)
			continue;
		priv->macro_params[macro_idx].event_handler(
			priv->component,
			LPASS_CDC_MACRO_EVT_SSR_UP, 0x0);
	}
	trace_printk("%s: SSR up events processed by all macros\n", __func__);
	lpass_cdc_notifier_call(priv, LPASS_CDC_WCD_EVT_SSR_UP);
	return 0;
}

static void lpass_cdc_ssr_disable(struct device *dev, void *data)
{
	struct lpass_cdc_priv *priv = data;
	int macro_idx;

	if (!priv->dev_up) {
		dev_err_ratelimited(priv->dev,
				    "%s: already disabled\n", __func__);
		return;
	}

	lpass_cdc_notifier_call(priv, LPASS_CDC_WCD_EVT_PA_OFF_PRE_SSR);
	regcache_cache_only(priv->regmap, true);

	mutex_lock(&priv->clk_lock);
	priv->dev_up = false;
	priv->pre_dev_up = false;
	mutex_unlock(&priv->clk_lock);
	if (priv->rsc_clk_cb)
		priv->rsc_clk_cb(priv->clk_dev, LPASS_CDC_MACRO_EVT_SSR_DOWN);
	/* call ssr event for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (!priv->macro_params[macro_idx].event_handler)
			continue;
		priv->macro_params[macro_idx].event_handler(
			priv->component,
			LPASS_CDC_MACRO_EVT_SSR_DOWN, 0x0);
	}
	lpass_cdc_notifier_call(priv, LPASS_CDC_WCD_EVT_SSR_DOWN);
}

static struct snd_info_entry_ops lpass_cdc_info_ops = {
	.read = lpass_cdc_version_read,
};

static const struct snd_event_ops lpass_cdc_ssr_ops = {
	.enable = lpass_cdc_ssr_enable,
	.disable = lpass_cdc_ssr_disable,
};

/*
 * lpass_cdc_info_create_codec_entry - creates lpass_cdc module
 * @codec_root: The parent directory
 * @component: Codec component instance
 *
 * Creates lpass_cdc module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int lpass_cdc_info_create_codec_entry(struct snd_info_entry *codec_root,
				   struct snd_soc_component *component)
{
	struct snd_info_entry *version_entry;
	struct lpass_cdc_priv *priv;
	struct snd_soc_card *card;

	if (!codec_root || !component)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (priv->entry) {
		dev_dbg(priv->dev,
			"%s:lpass_cdc module already created\n", __func__);
		return 0;
	}
	card = component->card;
	priv->entry = snd_info_create_module_entry(codec_root->module,
					     "lpass-cdc", codec_root);
	if (!priv->entry) {
		dev_dbg(component->dev, "%s: failed to create lpass_cdc entry\n",
			__func__);
		return -ENOMEM;
	}
	priv->entry->mode = S_IFDIR | 0555;
	if (snd_info_register(priv->entry) < 0) {
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   priv->entry);
	if (!version_entry) {
		dev_err(component->dev, "%s: failed to create lpass_cdc version entry\n",
			__func__);
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}

	version_entry->private_data = priv;
	version_entry->size = LPASS_CDC_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &lpass_cdc_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}
	priv->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(lpass_cdc_info_create_codec_entry);

/**
 * lpass_cdc_register_wake_irq - Register wake irq of Tx macro
 *
 * @component: codec component ptr.
 * @ipc_wakeup: bool to identify ipc_wakeup to be used or HW interrupt line.
 *
 * Return: 0 on success or negative error code on failure.
 */
int lpass_cdc_register_wake_irq(struct snd_soc_component *component,
			     u32 ipc_wakeup)
{
	struct lpass_cdc_priv *priv = NULL;

	if (!component)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (!priv)
		return -EINVAL;

	if (!lpass_cdc_is_valid_codec_dev(priv->dev)) {
		dev_err(component->dev, "%s: invalid codec\n", __func__);
		return -EINVAL;
	}

	if (priv->macro_params[VA_MACRO].reg_wake_irq)
		priv->macro_params[VA_MACRO].reg_wake_irq(
				component, ipc_wakeup);

	return 0;
}
EXPORT_SYMBOL(lpass_cdc_register_wake_irq);

/**
 * lpass_cdc_tx_mclk_enable - Enable/Disable TX Macro mclk
 *
 * @component: pointer to codec component instance.
 * @enable: set true to enable, otherwise false.
 *
 * Returns 0 on success or -EINVAL on error.
 */
int lpass_cdc_tx_mclk_enable(struct snd_soc_component *component,
			  bool enable)
{
	struct lpass_cdc_priv *priv = NULL;
	int ret = 0;

	if (!component)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (!priv)
		return -EINVAL;

	if (!lpass_cdc_is_valid_codec_dev(priv->dev)) {
		dev_err(component->dev, "%s: invalid codec\n", __func__);
		return -EINVAL;
	}

	if (priv->macro_params[TX_MACRO].clk_enable)
		ret = priv->macro_params[TX_MACRO].clk_enable(component,
								enable);

	return ret;
}
EXPORT_SYMBOL(lpass_cdc_tx_mclk_enable);

/**
 * lpass_cdc_register_event_listener - Register/Deregister to event listener
 *
 * @component: pointer to codec component instance.
 * @enable: when set to 1 registers to event listener otherwise, derigisters
 *          from the event listener
 *
 * Returns 0 on success or -EINVAL on error.
 */
int lpass_cdc_register_event_listener(struct snd_soc_component *component,
				   bool enable)
{
	struct lpass_cdc_priv *priv = NULL;
	int ret = 0;

	if (!component)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (!priv)
		return -EINVAL;

	if (!lpass_cdc_is_valid_codec_dev(priv->dev)) {
		dev_err(component->dev, "%s: invalid codec\n", __func__);
		return -EINVAL;
	}

	if (priv->macro_params[TX_MACRO].reg_evt_listener)
		ret = priv->macro_params[TX_MACRO].reg_evt_listener(component,
								    enable);

	return ret;
}
EXPORT_SYMBOL(lpass_cdc_register_event_listener);

static int lpass_cdc_soc_codec_probe(struct snd_soc_component *component)
{
	struct lpass_cdc_priv *priv = dev_get_drvdata(component->dev);
	int macro_idx, ret = 0;
	u8 core_id_0 = 0, core_id_1 = 0;

	snd_soc_component_init_regmap(component, priv->regmap);

	if (!priv->version) {
		/*
		 * In order for the ADIE RTC to differentiate between targets
		 * version info is used.
		 * Assign 1.0 for target with only one macro
		 * Assign 1.1 for target with two macros
		 * Assign 1.2 for target with more than two macros
		 */
		if (priv->num_macros_registered == 1)
			priv->version = LPASS_CDC_VERSION_1_0;
		else if (priv->num_macros_registered == 2)
			priv->version = LPASS_CDC_VERSION_1_1;
		else if (priv->num_macros_registered > 2)
			priv->version = LPASS_CDC_VERSION_1_2;
	}

	/* Assign lpass_cdc version */
	core_id_0 = snd_soc_component_read(component,
					LPASS_CDC_VA_TOP_CSR_CORE_ID_0);
	core_id_1 = snd_soc_component_read(component,
					LPASS_CDC_VA_TOP_CSR_CORE_ID_1);
	if ((core_id_0 == 0x01) && (core_id_1 == 0x0F))
		priv->version = LPASS_CDC_VERSION_2_0;
	if ((core_id_0 == 0x02) && (core_id_1 == 0x0E))
		priv->version = LPASS_CDC_VERSION_2_1;
	if ((core_id_0 == 0x02) && (core_id_1 == 0x0F))
		priv->version = LPASS_CDC_VERSION_2_5;

	/* call init for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++) {
		if (priv->macro_params[macro_idx].init) {
			ret = priv->macro_params[macro_idx].init(component);
			if (ret < 0) {
				dev_err(component->dev,
					"%s: init for macro %d failed\n",
					__func__, macro_idx);
				goto err;
			}
		}
	}
	priv->component = component;

	ret = snd_event_client_register(priv->dev, &lpass_cdc_ssr_ops, priv);
	if (!ret) {
		snd_event_notify(priv->dev, SND_EVENT_UP);
	} else {
		dev_err(component->dev,
			"%s: Registration with SND event FWK failed ret = %d\n",
			__func__, ret);
		goto err;
	}

	dev_dbg(component->dev, "%s: lpass_cdc soc codec probe success\n",
		__func__);
err:
	return ret;
}

static void lpass_cdc_soc_codec_remove(struct snd_soc_component *component)
{
	struct lpass_cdc_priv *priv = dev_get_drvdata(component->dev);
	int macro_idx;

	snd_event_client_deregister(priv->dev);
	/* call exit for supported macros */
	for (macro_idx = START_MACRO; macro_idx < MAX_MACRO; macro_idx++)
		if (priv->macro_params[macro_idx].exit)
			priv->macro_params[macro_idx].exit(component);

	return;
}

static const struct snd_soc_component_driver lpass_cdc = {
	.name = DRV_NAME,
	.probe = lpass_cdc_soc_codec_probe,
	.remove = lpass_cdc_soc_codec_remove,
};

static void lpass_cdc_add_child_devices(struct work_struct *work)
{
	struct lpass_cdc_priv *priv;
	bool split_codec = false;
	struct platform_device *pdev;
	struct device_node *node;
	int ret = 0, count = 0;
	struct wcd_ctrl_platform_data *platdata = NULL;
	char plat_dev_name[LPASS_CDC_STRING_LEN] = "";

	priv = container_of(work, struct lpass_cdc_priv,
			    lpass_cdc_add_child_devices_work);
	if (!priv) {
		pr_err("%s: Memory for lpass_cdc priv does not exist\n",
			__func__);
		return;
	}
	if (!priv->dev || !priv->dev->of_node) {
		dev_err(priv->dev, "%s: DT node for lpass_cdc does not exist\n",
			__func__);
		return;
	}

	platdata = &priv->plat_data;
	priv->child_count = 0;

	for_each_available_child_of_node(priv->dev->of_node, node) {
		split_codec = false;
		if (of_find_property(node, "qcom,split-codec", NULL)) {
			split_codec = true;
			dev_dbg(priv->dev, "%s: split codec slave exists\n",
				__func__);
		}

		strlcpy(plat_dev_name, node->name,
				(LPASS_CDC_STRING_LEN - 1));

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(priv->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = priv->dev;
		pdev->dev.of_node = node;

		priv->dev->platform_data = platdata;
		if (split_codec)
			priv->wcd_dev = &pdev->dev;

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			platform_device_put(pdev);
			goto fail_pdev_add;
		}

		if (priv->child_count < LPASS_CDC_CHILD_DEVICES_MAX)
			priv->pdev_child_devices[priv->child_count++] = pdev;
		else
			goto err;
	}
	return;
fail_pdev_add:
	for (count = 0; count < priv->child_count; count++)
		platform_device_put(priv->pdev_child_devices[count]);
err:
	return;
}

static int lpass_cdc_probe(struct platform_device *pdev)
{
	struct lpass_cdc_priv *priv;
	u32 num_macros = 0;
	int ret;
	struct clk *lpass_core_hw_vote = NULL;
	struct clk *lpass_audio_hw_vote = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct lpass_cdc_priv),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,num-macros",
				   &num_macros);
	if (ret) {
		dev_err(&pdev->dev,
			"%s:num-macros property not found\n",
			__func__);
		return ret;
	}
	priv->num_macros = num_macros;
	if (priv->num_macros > MAX_MACRO) {
		dev_err(&pdev->dev,
			"%s:num_macros(%d) > MAX_MACRO(%d) than supported\n",
			__func__, priv->num_macros, MAX_MACRO);
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,lpass-cdc-version", &priv->version);
	if (ret) {
		dev_dbg(&pdev->dev, "%s:lpass_cdc version not specified\n",
			__func__);
		ret = 0;
	}

	BLOCKING_INIT_NOTIFIER_HEAD(&priv->notifier);
	priv->dev = &pdev->dev;
	priv->dev_up = true;
	priv->pre_dev_up = true;
	priv->initial_boot = true;
	priv->regmap = lpass_cdc_regmap_init(priv->dev,
					  &lpass_cdc_regmap_config);
	if (IS_ERR_OR_NULL((void *)(priv->regmap))) {
		dev_err(&pdev->dev, "%s:regmap init failed\n", __func__);
		return -EINVAL;
	}

	devm_regmap_qti_debugfs_register(priv->dev, priv->regmap);

	priv->read_dev = __lpass_cdc_reg_read;
	priv->write_dev = __lpass_cdc_reg_write;

	priv->plat_data.handle = (void *) priv;
	priv->plat_data.update_wcd_event = lpass_cdc_update_wcd_event;
	priv->plat_data.register_notifier = lpass_cdc_register_notifier;

	priv->core_hw_vote_count = 0;
	priv->core_audio_vote_count = 0;

	dev_set_drvdata(&pdev->dev, priv);
	mutex_init(&priv->io_lock);
	mutex_init(&priv->clk_lock);
	mutex_init(&priv->vote_lock);
	INIT_WORK(&priv->lpass_cdc_add_child_devices_work,
		  lpass_cdc_add_child_devices);

	/* Register LPASS core hw vote */
	lpass_core_hw_vote = devm_clk_get(&pdev->dev, "lpass_core_hw_vote");
	if (IS_ERR(lpass_core_hw_vote)) {
		ret = PTR_ERR(lpass_core_hw_vote);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_core_hw_vote", ret);
		lpass_core_hw_vote = NULL;
		ret = 0;
	}
	priv->lpass_core_hw_vote = lpass_core_hw_vote;

	/* Register LPASS audio hw vote */
	lpass_audio_hw_vote = devm_clk_get(&pdev->dev, "lpass_audio_hw_vote");
	if (IS_ERR(lpass_audio_hw_vote)) {
		ret = PTR_ERR(lpass_audio_hw_vote);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_audio_hw_vote", ret);
		lpass_audio_hw_vote = NULL;
		ret = 0;
	}
	priv->lpass_audio_hw_vote = lpass_audio_hw_vote;
	schedule_work(&priv->lpass_cdc_add_child_devices_work);

	return 0;
}

static int lpass_cdc_remove(struct platform_device *pdev)
{
	struct lpass_cdc_priv *priv = dev_get_drvdata(&pdev->dev);

	if (!priv)
		return -EINVAL;

	of_platform_depopulate(&pdev->dev);
	mutex_destroy(&priv->io_lock);
	mutex_destroy(&priv->clk_lock);
	mutex_destroy(&priv->vote_lock);
	return 0;
}

#ifdef CONFIG_PM
int lpass_cdc_runtime_resume(struct device *dev)
{
	struct lpass_cdc_priv *priv = dev_get_drvdata(dev->parent);
	int ret = 0;

	trace_printk("%s, enter\n", __func__);
	dev_dbg(dev,"%s, enter\n", __func__);
	mutex_lock(&priv->vote_lock);
	if (priv->lpass_core_hw_vote == NULL) {
		dev_dbg(dev, "%s: Invalid lpass core hw node\n", __func__);
		goto audio_vote;
	}

	if (priv->core_hw_vote_count == 0) {
		ret = digital_cdc_rsc_mgr_hw_vote_enable(priv->lpass_core_hw_vote, dev);
		if (ret < 0) {
			dev_err(dev, "%s:lpass core hw enable failed\n",
				__func__);
			goto audio_vote;
		}
	}
	priv->core_hw_vote_count++;
	trace_printk("%s: hw vote count %d\n",
		__func__, priv->core_hw_vote_count);

audio_vote:
	if (priv->lpass_audio_hw_vote == NULL) {
		dev_dbg(dev, "%s: Invalid lpass audio hw node\n", __func__);
		goto done;
	}

	if (priv->core_audio_vote_count == 0) {
		ret = digital_cdc_rsc_mgr_hw_vote_enable(priv->lpass_audio_hw_vote, dev);
		if (ret < 0) {
			dev_err(dev, "%s:lpass audio hw enable failed\n",
				__func__);
			goto done;
		}
	}
	priv->core_audio_vote_count++;
	trace_printk("%s: audio vote count %d\n",
		__func__, priv->core_audio_vote_count);

done:
	mutex_unlock(&priv->vote_lock);
	trace_printk("%s, leave\n", __func__);
	dev_dbg(dev,"%s, leave, hw_vote %d, audio_vote %d\n", __func__,
			priv->core_hw_vote_count, priv->core_audio_vote_count);
	pm_runtime_set_autosuspend_delay(priv->dev, LPASS_CDC_AUTO_SUSPEND_DELAY);
	return 0;
}
EXPORT_SYMBOL(lpass_cdc_runtime_resume);

int lpass_cdc_runtime_suspend(struct device *dev)
{
	struct lpass_cdc_priv *priv = dev_get_drvdata(dev->parent);

	trace_printk("%s, enter\n", __func__);
	dev_dbg(dev,"%s, enter\n", __func__);
	mutex_lock(&priv->vote_lock);
	if (priv->lpass_core_hw_vote != NULL) {
		if (--priv->core_hw_vote_count == 0)
			digital_cdc_rsc_mgr_hw_vote_disable(
					priv->lpass_core_hw_vote, dev);
		if (priv->core_hw_vote_count < 0)
			priv->core_hw_vote_count = 0;
	} else {
		dev_dbg(dev, "%s: Invalid lpass core hw node\n",
			__func__);
	}
	trace_printk("%s: hw vote count %d\n",
		__func__, priv->core_hw_vote_count);

	if (priv->lpass_audio_hw_vote != NULL) {
		if (--priv->core_audio_vote_count == 0)
			digital_cdc_rsc_mgr_hw_vote_disable(
					priv->lpass_audio_hw_vote, dev);
		if (priv->core_audio_vote_count < 0)
			priv->core_audio_vote_count = 0;
	} else {
		dev_dbg(dev, "%s: Invalid lpass audio hw node\n",
			__func__);
	}
	trace_printk("%s: audio vote count %d\n",
		__func__, priv->core_audio_vote_count);

	mutex_unlock(&priv->vote_lock);
	trace_printk("%s, leave\n", __func__);
	dev_dbg(dev,"%s, leave, hw_vote %d, audio_vote %d\n", __func__,
		priv->core_hw_vote_count, priv->core_audio_vote_count);
	return 0;
}
EXPORT_SYMBOL(lpass_cdc_runtime_suspend);
#endif /* CONFIG_PM */

bool lpass_cdc_check_core_votes(struct device *dev)
{
	struct lpass_cdc_priv *priv = dev_get_drvdata(dev->parent);
	bool ret = true;
	trace_printk("%s, enter\n", __func__);
	mutex_lock(&priv->vote_lock);
	if (!priv->pre_dev_up ||
		(priv->lpass_core_hw_vote && !priv->core_hw_vote_count) ||
		(priv->lpass_audio_hw_vote && !priv->core_audio_vote_count))
		ret = false;
	mutex_unlock(&priv->vote_lock);
	trace_printk("%s, leave\n", __func__);

	return ret;
}
EXPORT_SYMBOL(lpass_cdc_check_core_votes);

static const struct of_device_id lpass_cdc_dt_match[] = {
	{.compatible = "qcom,lpass-cdc"},
	{}
};
MODULE_DEVICE_TABLE(of, lpass_cdc_dt_match);

static struct platform_driver lpass_cdc_drv = {
	.driver = {
		.name = "lpass-cdc",
		.owner = THIS_MODULE,
		.of_match_table = lpass_cdc_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = lpass_cdc_probe,
	.remove = lpass_cdc_remove,
};

static int lpass_cdc_drv_init(void)
{
	return platform_driver_register(&lpass_cdc_drv);
}

static void lpass_cdc_drv_exit(void)
{
	platform_driver_unregister(&lpass_cdc_drv);
}

static int __init lpass_cdc_init(void)
{
	lpass_cdc_drv_init();
	lpass_cdc_clk_rsc_mgr_init();
	return 0;
}
module_init(lpass_cdc_init);

static void __exit lpass_cdc_exit(void)
{
	lpass_cdc_clk_rsc_mgr_exit();
	lpass_cdc_drv_exit();
}
module_exit(lpass_cdc_exit);

MODULE_DESCRIPTION("LPASS Codec driver");
MODULE_LICENSE("GPL v2");
