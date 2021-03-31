/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/of_device.h>
#include <linux/mfd/msm-cdc-pinctrl.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <device_event.h>
#include <linux/qdsp6v2/audio_notifier.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/wcd9335.h"
#include "../codecs/wcd934x/wcd934x.h"
#include "../codecs/wcd934x/wcd934x-mbhc.h"
#include "../codecs/wsa881x.h"

#define DRV_NAME "msm8998-asoc-snd"

#define __CHIPSET__ "MSM8998 "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_11P025KHZ 11025
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_22P05KHZ  22050
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_44P1KHZ   44100
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_88P2KHZ   88200
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_176P4KHZ  176400
#define SAMPLING_RATE_192KHZ    192000
#define SAMPLING_RATE_352P8KHZ  352800
#define SAMPLING_RATE_384KHZ    384000

#define CODEC_EXT_CLK_RATE          9600000
#define CM710X_CODEC_EXT_CLK_RATE  19200000
#define ADSP_STATE_READY_TIMEOUT_MS 3000
#define DEV_NAME_STR_LEN            32

#define WCN_CDC_SLIM_RX_CH_MAX 2
#define WCN_CDC_SLIM_TX_CH_MAX 3

#define TDM_CHANNEL_MAX 8

#define MSM_HIFI_ON 1

enum {
	SLIM_RX_0 = 0,
	SLIM_RX_1,
	SLIM_RX_2,
	SLIM_RX_3,
	SLIM_RX_4,
	SLIM_RX_5,
	SLIM_RX_6,
	SLIM_RX_7,
	SLIM_RX_MAX,
};

enum {
	SLIM_TX_0 = 0,
	SLIM_TX_1,
	SLIM_TX_2,
	SLIM_TX_3,
	SLIM_TX_4,
	SLIM_TX_5,
	SLIM_TX_6,
	SLIM_TX_7,
	SLIM_TX_8,
	SLIM_TX_MAX,
};

enum {
	PRI_MI2S = 0,
	SEC_MI2S,
	TERT_MI2S,
	QUAT_MI2S,
	MI2S_MAX,
};

enum {
	PRIM_AUX_PCM = 0,
	SEC_AUX_PCM,
	TERT_AUX_PCM,
	QUAT_AUX_PCM,
	AUX_PCM_MAX,
};

enum {
	PCM_I2S_SEL_PRIM = 0,
	PCM_I2S_SEL_SEC,
	PCM_I2S_SEL_TERT,
	PCM_I2S_SEL_QUAT,
	PCM_I2S_SEL_MAX,
};

struct mi2s_aux_pcm_common_conf {
	struct mutex lock;
	void *pcm_i2s_sel_vt_addr;
};

struct mi2s_conf {
	struct mutex lock;
	u32 ref_cnt;
	u32 msm_is_mi2s_master;
};

struct auxpcm_conf {
	struct mutex lock;
	u32 ref_cnt;
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

struct msm_asoc_mach_data {
	u32 mclk_freq;
	int us_euro_gpio; /* used by gpio driver API */
	struct device_node *us_euro_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en1_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en0_gpio_p; /* used by pinctrl API */
	struct snd_info_entry *codec_root;
	int left_jack_detect_gpio;
	int right_jack_detect_gpio;
};

struct msm_asoc_wcd93xx_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
	void (*mbhc_hs_detect_exit)(struct snd_soc_codec *codec);
};

enum {
	TDM_0 = 0,
	TDM_1,
	TDM_2,
	TDM_3,
	TDM_4,
	TDM_5,
	TDM_6,
	TDM_7,
	TDM_PORT_MAX,
};

enum {
	TDM_PRI = 0,
	TDM_SEC,
	TDM_TERT,
	TDM_QUAT,
	TDM_INTERFACE_MAX,
};

struct tdm_port {
	u32 mode;
	u32 channel;
};

/* TDM default config */
static struct dev_config tdm_rx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	}
};

/* TDM default config */
static struct dev_config tdm_tx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	}
};

/* Default configuration of slimbus channels */
static struct dev_config slim_rx_cfg[] = {
	[SLIM_RX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_6] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config slim_tx_cfg[] = {
	[SLIM_TX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_6] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_8] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config usb_rx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

static struct dev_config usb_tx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 1,
};

static struct dev_config proxy_rx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

/* Default configuration of MI2S channels */
static struct dev_config mi2s_rx_cfg[] = {
	[PRI_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[SEC_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TERT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[QUAT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config mi2s_tx_cfg[] = {
	[PRI_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config aux_pcm_rx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config aux_pcm_tx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static int msm_vi_feed_tx_ch = 2;
static const char *const slim_rx_ch_text[] = {"One", "Two"};
static const char *const slim_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					  "S32_LE"};
static char const *slim_sample_rate_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static char const *bt_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_48"};
static const char *const usb_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};
static char const *ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *usb_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
					"KHZ_16", "KHZ_22P05",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four",
				    "Five", "Six", "Seven", "Eight"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S32_LE"};
static char const *tdm_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_32",
					     "KHZ_44P1", "KHZ_48", "KHZ_96",
					     "KHZ_192", "KHZ_352P8", "KHZ_384"};
static const char *const auxpcm_rate_text[] = {"KHZ_8", "KHZ_16"};
static char const *mi2s_rate_text[] = {"KHZ_8", "KHZ_16",
				      "KHZ_32", "KHZ_44P1", "KHZ_48",
				      "KHZ_96", "KHZ_192"};
static const char *const mi2s_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};
static const char *const hifi_text[] = {"Off", "On"};

static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_2_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_chs, slim_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_1_tx_chs, slim_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(vi_feed_tx_chs, vi_feed_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(proxy_rx_chs, ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_2_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate, bt_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_sample_rate, usb_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_sample_rate, usb_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_chs, tdm_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_format, tdm_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_sample_rate, tdm_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_chs, tdm_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_format, tdm_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_sample_rate, tdm_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(hifi_function, hifi_text);

static struct platform_device *spdev;
static int msm_hifi_control;

static bool is_initial_boot;
static bool codec_reg_done;
static struct msm_asoc_wcd93xx_codec msm_codec_fn;

static struct afe_clk_set mi2s_clk[MI2S_MAX] = {
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_SEC_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	}
};

static struct mi2s_aux_pcm_common_conf mi2s_auxpcm_conf[PCM_I2S_SEL_MAX];
static struct mi2s_conf mi2s_intf_conf[MI2S_MAX];
static struct auxpcm_conf auxpcm_intf_conf[AUX_PCM_MAX];

static int slim_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 10;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int slim_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int slim_get_bit_format_val(int bit_format)
{
	int val = 0;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		val = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		val = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		val = 0;
		break;
	}
	return val;
}

static int slim_get_bit_format(int val)
{
	int bit_fmt = SNDRV_PCM_FORMAT_S16_LE;

	switch (val) {
	case 0:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		bit_fmt = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		bit_fmt = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 3:
		bit_fmt = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return bit_fmt;
}

static int slim_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int port_id = 0;

	if (strnstr(kcontrol->id.name, "SLIM_0_RX", sizeof("SLIM_0_RX")))
		port_id = SLIM_RX_0;
	else if (strnstr(kcontrol->id.name, "SLIM_2_RX", sizeof("SLIM_2_RX")))
		port_id = SLIM_RX_2;
	else if (strnstr(kcontrol->id.name, "SLIM_5_RX", sizeof("SLIM_5_RX")))
		port_id = SLIM_RX_5;
	else if (strnstr(kcontrol->id.name, "SLIM_6_RX", sizeof("SLIM_6_RX")))
		port_id = SLIM_RX_6;
	else if (strnstr(kcontrol->id.name, "SLIM_0_TX", sizeof("SLIM_0_TX")))
		port_id = SLIM_TX_0;
	else if (strnstr(kcontrol->id.name, "SLIM_1_TX", sizeof("SLIM_1_TX")))
		port_id = SLIM_TX_1;
	else {
		pr_err("%s: unsupported channel: %s",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}

	return port_id;
}

static int slim_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
		slim_get_sample_rate_val(slim_rx_cfg[ch_num].sample_rate);

	pr_debug("%s: slim[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].sample_rate =
		slim_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
		slim_get_sample_rate_val(slim_tx_cfg[ch_num].sample_rate);

	pr_debug("%s: slim[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate = 0;
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	sample_rate = slim_get_sample_rate(ucontrol->value.enumerated.item[0]);
	if (sample_rate == SAMPLING_RATE_44P1KHZ) {
		pr_err("%s: Unsupported sample rate %d: for Tx path\n",
			__func__, sample_rate);
		return -EINVAL;
	}
	slim_tx_cfg[ch_num].sample_rate = sample_rate;

	pr_debug("%s: slim[%d]_tx_sample_rate = %d, value = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
			slim_get_bit_format_val(slim_rx_cfg[ch_num].bit_format);

	pr_debug("%s: slim[%d]_rx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_rx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].bit_format =
		slim_get_bit_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_rx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_rx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
			slim_get_bit_format_val(slim_tx_cfg[ch_num].bit_format);

	pr_debug("%s: slim[%d]_tx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_tx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_tx_cfg[ch_num].bit_format =
		slim_get_bit_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_tx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_tx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_slim_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	pr_debug("%s: msm_slim_[%d]_rx_ch  = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].channels);
	ucontrol->value.enumerated.item[0] = slim_rx_cfg[ch_num].channels - 1;

	return 0;
}

static int msm_slim_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_slim_[%d]_rx_ch  = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].channels);

	return 1;
}

static int msm_slim_tx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	pr_debug("%s: msm_slim_[%d]_tx_ch  = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].channels);
	ucontrol->value.enumerated.item[0] = slim_tx_cfg[ch_num].channels - 1;

	return 0;
}

static int msm_slim_tx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_tx_cfg[ch_num].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_slim_[%d]_tx_ch = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].channels);

	return 1;
}

static int msm_vi_feed_tx_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_vi_feed_tx_ch - 1;
	pr_debug("%s: msm_vi_feed_tx_ch = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_vi_feed_tx_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	msm_vi_feed_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_vi_feed_tx_ch = %d\n", __func__, msm_vi_feed_tx_ch);
	return 1;
}

static int msm_bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	/*
	 * Slimbus_7_Rx/Tx sample rate values should always be in sync (same)
	 * when used for BT_SCO use case. Return either Rx or Tx sample rate
	 * value.
	 */
	switch (slim_rx_cfg[SLIM_RX_7].sample_rate) {
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate = %d", __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_16KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 0:
	default:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_8KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rates: slim7_rx = %d, slim7_tx = %d, value = %d\n",
		 __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate,
		 slim_tx_cfg[SLIM_TX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int usb_audio_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: usb_audio_rx_ch  = %d\n", __func__,
		 usb_rx_cfg.channels);
	ucontrol->value.integer.value[0] = usb_rx_cfg.channels - 1;
	return 0;
}

static int usb_audio_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_rx_cfg.channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: usb_audio_rx_ch = %d\n", __func__, usb_rx_cfg.channels);
	return 1;
}

static int usb_audio_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val;

	switch (usb_rx_cfg.sample_rate) {
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: usb_audio_rx_sample_rate = %d\n", __func__,
		 usb_rx_cfg.sample_rate);
	return 0;
}

static int usb_audio_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 12:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, usb_audio_rx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		usb_rx_cfg.sample_rate);
	return 0;
}

static int usb_audio_rx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (usb_rx_cfg.bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: usb_audio_rx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_rx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int usb_audio_rx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: usb_audio_rx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_rx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}

static int usb_audio_tx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: usb_audio_tx_ch  = %d\n", __func__,
		 usb_tx_cfg.channels);
	ucontrol->value.integer.value[0] = usb_tx_cfg.channels - 1;
	return 0;
}

static int usb_audio_tx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_tx_cfg.channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: usb_audio_tx_ch = %d\n", __func__, usb_tx_cfg.channels);
	return 1;
}

static int usb_audio_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val;

	switch (usb_tx_cfg.sample_rate) {
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	default:
		sample_rate_val = 6;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: usb_audio_tx_sample_rate = %d\n", __func__,
		 usb_tx_cfg.sample_rate);
	return 0;
}

static int usb_audio_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 12:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, usb_audio_tx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		usb_tx_cfg.sample_rate);
	return 0;
}

static int usb_audio_tx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (usb_tx_cfg.bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: usb_audio_tx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_tx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int usb_audio_tx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: usb_audio_tx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_tx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}

static int proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: proxy_rx channels = %d\n",
		 __func__, proxy_rx_cfg.channels);
	ucontrol->value.integer.value[0] = proxy_rx_cfg.channels - 2;

	return 0;
}

static int proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	proxy_rx_cfg.channels = ucontrol->value.integer.value[0] + 2;
	pr_debug("%s: proxy_rx channels = %d\n",
		 __func__, proxy_rx_cfg.channels);

	return 1;
}

static int tdm_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int aux_pcm_get_sample_rate(int value)
{
	int sample_rate;

	switch (value) {
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 0:
	default:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	return sample_rate;
}

static int tdm_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 8;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int aux_pcm_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val;

	switch (sample_rate) {
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		sample_rate_val = 0;
		break;
	}
	return sample_rate_val;
}

static int tdm_get_port_idx(struct snd_kcontrol *kcontrol,
			    struct tdm_port *port)
{
	if (port) {
		if (strnstr(kcontrol->id.name, "PRI",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_PRI;
		} else if (strnstr(kcontrol->id.name, "SEC",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_SEC;
		} else if (strnstr(kcontrol->id.name, "TERT",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_TERT;
		} else if (strnstr(kcontrol->id.name, "QUAT",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_QUAT;
		} else {
			pr_err("%s: unsupported mode in: %s",
				__func__, kcontrol->id.name);
			return -EINVAL;
		}

		if (strnstr(kcontrol->id.name, "RX_0",
		    sizeof(kcontrol->id.name)) ||
		    strnstr(kcontrol->id.name, "TX_0",
		    sizeof(kcontrol->id.name))) {
			port->channel = TDM_0;
		} else if (strnstr(kcontrol->id.name, "RX_1",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_1",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_1;
		} else if (strnstr(kcontrol->id.name, "RX_2",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_2",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_2;
		} else if (strnstr(kcontrol->id.name, "RX_3",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_3",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_3;
		} else if (strnstr(kcontrol->id.name, "RX_4",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_4",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_4;
		} else if (strnstr(kcontrol->id.name, "RX_5",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_5",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_5;
		} else if (strnstr(kcontrol->id.name, "RX_6",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_6",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_6;
		} else if (strnstr(kcontrol->id.name, "RX_7",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_7",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_7;
		} else {
			pr_err("%s: unsupported channel in: %s",
				__func__, kcontrol->id.name);
			return -EINVAL;
		}
	} else
		return -EINVAL;
	return 0;
}

static int tdm_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_sample_rate_val(
			tdm_rx_cfg[port.mode][port.channel].sample_rate);

		pr_debug("%s: tdm_rx_sample_rate = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].sample_rate =
			tdm_get_sample_rate(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_rx_sample_rate = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_sample_rate_val(
			tdm_tx_cfg[port.mode][port.channel].sample_rate);

		pr_debug("%s: tdm_tx_sample_rate = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].sample_rate =
			tdm_get_sample_rate(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_tx_sample_rate = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_get_format(int value)
{
	int format = 0;

	switch (value) {
	case 0:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return format;
}

static int tdm_get_format_val(int format)
{
	int value = 0;

	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = 0;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		value = 1;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = 2;
		break;
	default:
		value = 0;
		break;
	}
	return value;
}

static int tdm_rx_format_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_format_val(
				tdm_rx_cfg[port.mode][port.channel].bit_format);

		pr_debug("%s: tdm_rx_bit_format = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_format_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].bit_format =
			tdm_get_format(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_rx_bit_format = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_format_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_format_val(
				tdm_tx_cfg[port.mode][port.channel].bit_format);

		pr_debug("%s: tdm_tx_bit_format = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_format_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].bit_format =
			tdm_get_format(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_tx_bit_format = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_ch_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {

		ucontrol->value.enumerated.item[0] =
			tdm_rx_cfg[port.mode][port.channel].channels - 1;

		pr_debug("%s: tdm_rx_ch = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].channels - 1,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_ch_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].channels =
			ucontrol->value.enumerated.item[0] + 1;

		pr_debug("%s: tdm_rx_ch = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].channels,
			 ucontrol->value.enumerated.item[0] + 1);
	}
	return ret;
}

static int tdm_tx_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] =
			tdm_tx_cfg[port.mode][port.channel].channels - 1;

		pr_debug("%s: tdm_tx_ch = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].channels - 1,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].channels =
			ucontrol->value.enumerated.item[0] + 1;

		pr_debug("%s: tdm_tx_ch = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].channels,
			 ucontrol->value.enumerated.item[0] + 1);
	}
	return ret;
}

static int aux_pcm_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx;

	if (strnstr(kcontrol->id.name, "PRIM_AUX_PCM",
		    sizeof("PRIM_AUX_PCM")))
		idx = PRIM_AUX_PCM;
	else if (strnstr(kcontrol->id.name, "SEC_AUX_PCM",
			 sizeof("SEC_AUX_PCM")))
		idx = SEC_AUX_PCM;
	else if (strnstr(kcontrol->id.name, "TERT_AUX_PCM",
			 sizeof("TERT_AUX_PCM")))
		idx = TERT_AUX_PCM;
	else if (strnstr(kcontrol->id.name, "QUAT_AUX_PCM",
			 sizeof("QUAT_AUX_PCM")))
		idx = QUAT_AUX_PCM;
	else {
		pr_err("%s: unsupported port: %s",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int aux_pcm_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_rx_cfg[idx].sample_rate =
		aux_pcm_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
	     aux_pcm_get_sample_rate_val(aux_pcm_rx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_tx_cfg[idx].sample_rate =
		aux_pcm_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
	     aux_pcm_get_sample_rate_val(aux_pcm_tx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx;

	if (strnstr(kcontrol->id.name, "PRI_MI2S_RX",
	    sizeof("PRI_MI2S_RX")))
		idx = PRI_MI2S;
	else if (strnstr(kcontrol->id.name, "SEC_MI2S_RX",
		 sizeof("SEC_MI2S_RX")))
		idx = SEC_MI2S;
	else if (strnstr(kcontrol->id.name, "TERT_MI2S_RX",
		 sizeof("TERT_MI2S_RX")))
		idx = TERT_MI2S;
	else if (strnstr(kcontrol->id.name, "QUAT_MI2S_RX",
		 sizeof("QUAT_MI2S_RX")))
		idx = QUAT_MI2S;
	else if (strnstr(kcontrol->id.name, "PRI_MI2S_TX",
		 sizeof("PRI_MI2S_TX")))
		idx = PRI_MI2S;
	else if (strnstr(kcontrol->id.name, "SEC_MI2S_TX",
		 sizeof("SEC_MI2S_TX")))
		idx = SEC_MI2S;
	else if (strnstr(kcontrol->id.name, "TERT_MI2S_TX",
		 sizeof("TERT_MI2S_TX")))
		idx = TERT_MI2S;
	else if (strnstr(kcontrol->id.name, "QUAT_MI2S_TX",
		 sizeof("QUAT_MI2S_TX")))
		idx = QUAT_MI2S;
	else {
		pr_err("%s: unsupported channel: %s",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int mi2s_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 6;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int mi2s_get_sample_rate(int value)
{
	int sample_rate;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int mi2s_get_format(int value)
{
	int format;

	switch (value) {
	case 0:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 3:
		format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return format;
}

static int mi2s_get_format_value(int format)
{
	int value;

	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = 0;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		value = 1;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		value = 2;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = 3;
		break;
	default:
		value = 0;
		break;
	}
	return value;
}

static int mi2s_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].sample_rate =
		mi2s_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_sample_rate_val(mi2s_rx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].sample_rate =
		mi2s_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_sample_rate_val(mi2s_tx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	pr_debug("%s: msm_mi2s_[%d]_rx_ch  = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].channels);
	ucontrol->value.enumerated.item[0] = mi2s_rx_cfg[idx].channels - 1;

	return 0;
}

static int msm_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_mi2s_[%d]_rx_ch  = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].channels);

	return 1;
}

static int msm_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	pr_debug("%s: msm_mi2s_[%d]_tx_ch  = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].channels);
	ucontrol->value.enumerated.item[0] = mi2s_tx_cfg[idx].channels - 1;

	return 0;
}

static int msm_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_mi2s_[%d]_tx_ch  = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].channels);

	return 1;
}

static int msm_mi2s_rx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_format_value(mi2s_rx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		idx, mi2s_rx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_rx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].bit_format =
		mi2s_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		  idx, mi2s_rx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_tx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_format_value(mi2s_tx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		idx, mi2s_tx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_tx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].bit_format =
		mi2s_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		  idx, mi2s_tx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_hifi_ctrl(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	pr_debug("%s: msm_hifi_control = %d", __func__,
		 msm_hifi_control);

	if (!pdata || !pdata->hph_en1_gpio_p) {
		pr_err("%s: hph_en1_gpio is invalid\n", __func__);
		return -EINVAL;
	}
	if (msm_hifi_control == MSM_HIFI_ON) {
		msm_cdc_pinctrl_select_active_state(pdata->hph_en1_gpio_p);
		/* 5msec delay needed as per HW requirement */
		usleep_range(5000, 5010);
	} else {
		msm_cdc_pinctrl_select_sleep_state(pdata->hph_en1_gpio_p);
	}
	snd_soc_dapm_sync(dapm);

	return 0;
}

static int msm_hifi_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hifi_control = %d\n",
		 __func__, msm_hifi_control);
	ucontrol->value.integer.value[0] = msm_hifi_control;

	return 0;
}

static int msm_hifi_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pr_debug("%s() ucontrol->value.integer.value[0] = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);

	msm_hifi_control = ucontrol->value.integer.value[0];
	msm_hifi_ctrl(codec);

	return 0;
}

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("SLIM_0_RX Channels", slim_0_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_2_RX Channels", slim_2_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", slim_0_tx_chs,
			msm_slim_tx_ch_get, msm_slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_1_TX Channels", slim_1_tx_chs,
			msm_slim_tx_ch_get, msm_slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_5_RX Channels", slim_5_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_6_RX Channels", slim_6_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", vi_feed_tx_chs,
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Channels", usb_rx_chs,
			usb_audio_rx_ch_get, usb_audio_rx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Channels", usb_tx_chs,
			usb_audio_tx_ch_get, usb_audio_tx_ch_put),
	SOC_ENUM_EXT("PROXY_RX Channels", proxy_rx_chs,
			proxy_rx_ch_get, proxy_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", slim_0_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_5_RX Format", slim_5_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_6_RX Format", slim_6_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_TX Format", slim_0_tx_format,
			slim_tx_bit_format_get, slim_tx_bit_format_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Format", usb_rx_format,
			usb_audio_rx_format_get, usb_audio_rx_format_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Format", usb_tx_format,
			usb_audio_tx_format_get, usb_audio_tx_format_put),
	SOC_ENUM_EXT("SLIM_0_RX SampleRate", slim_0_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_2_RX SampleRate", slim_2_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_0_TX SampleRate", slim_0_tx_sample_rate,
			slim_tx_sample_rate_get, slim_tx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_5_RX SampleRate", slim_5_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_6_RX SampleRate", slim_6_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("BT SampleRate", bt_sample_rate,
			msm_bt_sample_rate_get,
			msm_bt_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_RX SampleRate", usb_rx_sample_rate,
			usb_audio_rx_sample_rate_get,
			usb_audio_rx_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_TX SampleRate", usb_tx_sample_rate,
			usb_audio_tx_sample_rate_get,
			usb_audio_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_RX SampleRate", prim_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_RX SampleRate", sec_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_RX SampleRate", tert_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_RX SampleRate", quat_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_TX SampleRate", prim_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_TX SampleRate", sec_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_TX SampleRate", tert_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_TX SampleRate", quat_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_MI2S_RX SampleRate", prim_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_MI2S_RX SampleRate", sec_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_MI2S_RX SampleRate", tert_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX SampleRate", quat_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_MI2S_TX SampleRate", prim_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_MI2S_TX SampleRate", sec_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_MI2S_TX SampleRate", tert_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX SampleRate", quat_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_MI2S_RX Channels", prim_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("PRI_MI2S_TX Channels", prim_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S_RX Channels", sec_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S_TX Channels", sec_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("TERT_MI2S_RX Channels", tert_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("TERT_MI2S_TX Channels", tert_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Channels", quat_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Channels", quat_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("PRI_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("PRI_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("SEC_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("SEC_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("TERT_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("TERT_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("HiFi Function", hifi_function, msm_hifi_get,
			msm_hifi_put),
};

static const struct snd_soc_dapm_widget msm_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Speaker Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p,
					     int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);

		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}

static int msm_slim_get_ch_from_beid(int32_t be_id)
{
	int ch_id = 0;

	switch (be_id) {
	case MSM_BACKEND_DAI_SLIMBUS_0_RX:
		ch_id = SLIM_RX_0;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_1_RX:
		ch_id = SLIM_RX_1;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_2_RX:
		ch_id = SLIM_RX_2;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_3_RX:
		ch_id = SLIM_RX_3;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_4_RX:
		ch_id = SLIM_RX_4;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_6_RX:
		ch_id = SLIM_RX_6;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_0_TX:
		ch_id = SLIM_TX_0;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_3_TX:
		ch_id = SLIM_TX_3;
		break;
	default:
		ch_id = SLIM_RX_0;
		break;
	}

	return ch_id;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	int rc = 0;
	int idx;
	void *config = NULL;
	struct snd_soc_codec *codec = NULL;

	pr_debug("%s: format = %d, rate = %d\n",
		  __func__, params_format(params), params_rate(params));

	switch (dai_link->be_id) {
	case MSM_BACKEND_DAI_SLIMBUS_0_RX:
	case MSM_BACKEND_DAI_SLIMBUS_1_RX:
	case MSM_BACKEND_DAI_SLIMBUS_2_RX:
	case MSM_BACKEND_DAI_SLIMBUS_3_RX:
	case MSM_BACKEND_DAI_SLIMBUS_4_RX:
	case MSM_BACKEND_DAI_SLIMBUS_6_RX:
		idx = msm_slim_get_ch_from_beid(dai_link->be_id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[idx].bit_format);
		rate->min = rate->max = slim_rx_cfg[idx].sample_rate;
		channels->min = channels->max = slim_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_0_TX:
	case MSM_BACKEND_DAI_SLIMBUS_3_TX:
		idx = msm_slim_get_ch_from_beid(dai_link->be_id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_tx_cfg[idx].bit_format);
		rate->min = rate->max = slim_tx_cfg[idx].sample_rate;
		channels->min = channels->max = slim_tx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_1_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_tx_cfg[1].bit_format);
		rate->min = rate->max = slim_tx_cfg[1].sample_rate;
		channels->min = channels->max = slim_tx_cfg[1].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_4_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       SNDRV_PCM_FORMAT_S32_LE);
		rate->min = rate->max = SAMPLING_RATE_8KHZ;
		channels->min = channels->max = msm_vi_feed_tx_ch;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_5_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[5].bit_format);
		rate->min = rate->max = slim_rx_cfg[5].sample_rate;
		channels->min = channels->max = slim_rx_cfg[5].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_5_TX:
		codec = rtd->codec;
		rate->min = rate->max = SAMPLING_RATE_16KHZ;
		channels->min = channels->max = 1;

		config = msm_codec_fn.get_afe_config_fn(codec,
					AFE_SLIMBUS_SLAVE_PORT_CONFIG);
		if (config) {
			rc = afe_set_config(AFE_SLIMBUS_SLAVE_PORT_CONFIG,
					    config, SLIMBUS_5_TX);
			if (rc)
				pr_err("%s: Failed to set slimbus slave port config %d\n",
					__func__, rc);
		}
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[SLIM_RX_7].bit_format);
		rate->min = rate->max = slim_rx_cfg[SLIM_RX_7].sample_rate;
		channels->min = channels->max =
			slim_rx_cfg[SLIM_RX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_TX:
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_7].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_8_TX:
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_8].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_8].channels;
		break;

	case MSM_BACKEND_DAI_USB_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				usb_rx_cfg.bit_format);
		rate->min = rate->max = usb_rx_cfg.sample_rate;
		channels->min = channels->max = usb_rx_cfg.channels;
		break;

	case MSM_BACKEND_DAI_USB_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				usb_tx_cfg.bit_format);
		rate->min = rate->max = usb_tx_cfg.sample_rate;
		channels->min = channels->max = usb_tx_cfg.channels;
		break;

	case MSM_BACKEND_DAI_AFE_PCM_RX:
		channels->min = channels->max = proxy_rx_cfg.channels;
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;

	case MSM_BACKEND_DAI_PRI_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_PRI_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEC_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEC_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_TERT_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_TERT_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUAT_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUAT_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[PRI_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[PRI_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[PRI_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_PRI_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[PRI_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[PRI_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[PRI_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[SEC_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[SEC_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[SEC_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SECONDARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[SEC_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[SEC_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[SEC_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_TERTIARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[TERT_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[TERT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[TERT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[TERT_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[TERT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[TERT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[QUAT_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[QUAT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[QUAT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[QUAT_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[QUAT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[QUAT_MI2S].channels;
		break;

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}

	return rc;
}

static int msm_afe_set_config(struct snd_soc_codec *codec)
{
	int ret = 0;
	void *config_data = NULL;

	if (!msm_codec_fn.get_afe_config_fn) {
		dev_err(codec->dev, "%s: codec get afe config not init'ed\n",
			__func__);
		return -EINVAL;
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTERS_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
		if (ret) {
			dev_err(codec->dev,
				"%s: Failed to set codec registers config %d\n",
				__func__, ret);
			return ret;
		}
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTER_PAGE_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_CDC_REGISTER_PAGE_CONFIG, config_data,
				    0);
		if (ret)
			dev_err(codec->dev,
				"%s: Failed to set cdc register page config\n",
				__func__);
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_SLIMBUS_SLAVE_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
		if (ret) {
			dev_err(codec->dev,
				"%s: Failed to set slimbus slave config %d\n",
				__func__, ret);
			return ret;
		}
	}

	return 0;
}

static void msm_afe_clear_config(void)
{
	afe_clear_config(AFE_CDC_REGISTERS_CONFIG);
	afe_clear_config(AFE_SLIMBUS_SLAVE_CONFIG);
}

static int msm_adsp_power_up_config(struct snd_soc_codec *codec)
{
	int ret = 0;
	unsigned long timeout;
	int adsp_ready = 0;

	timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);

	do {
		if (q6core_is_adsp_ready()) {
			pr_debug("%s: ADSP Audio is ready\n", __func__);
			adsp_ready = 1;
			break;
		} else {
			/*
			* ADSP will be coming up after subsystem restart and
			* it might not be fully up when the control reaches
			* here. So, wait for 50msec before checking ADSP state
			*/
			msleep(50);
		}
	} while (time_after(timeout, jiffies));

	if (!adsp_ready) {
		pr_err("%s: timed out waiting for ADSP Audio\n", __func__);
		ret = -ETIMEDOUT;
		goto err_fail;
	}

	ret = msm_afe_set_config(codec);
	if (ret)
		pr_err("%s: Failed to set AFE config. err %d\n",
			__func__, ret);

	return 0;

err_fail:
	return ret;
}

static int msm8998_notifier_service_cb(struct notifier_block *this,
					 unsigned long opcode, void *ptr)
{
	int ret;
	struct snd_soc_card *card = NULL;
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_codec *codec;

	pr_debug("%s: Service opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		/*
		 * Use flag to ignore initial boot notifications
		 * On initial boot msm_adsp_power_up_config is
		 * called on init. There is no need to clear
		 * and set the config again on initial boot.
		 */
		if (is_initial_boot)
			break;
		msm_afe_clear_config();
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (is_initial_boot) {
			is_initial_boot = false;
			break;
		}
		if (!spdev)
			return -EINVAL;

		card = platform_get_drvdata(spdev);
		rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
		if (!rtd) {
			dev_err(card->dev,
				"%s: snd_soc_get_pcm_runtime for %s failed!\n",
				__func__, be_dl_name);
			ret = -EINVAL;
			goto done;
		}
		codec = rtd->codec;

		ret = msm_adsp_power_up_config(codec);
		if (ret < 0) {
			dev_err(card->dev,
				"%s: msm_adsp_power_up_config failed ret = %d!\n",
				__func__, ret);
			goto done;
		}
		break;
	default:
		break;
	}
done:
	return NOTIFY_OK;
}

static struct notifier_block service_nb = {
	.notifier_call  = msm8998_notifier_service_cb,
	.priority = -INT_MAX,
};

static int msm_wcn_init(struct snd_soc_pcm_runtime *rtd)
{
	unsigned int rx_ch[WCN_CDC_SLIM_RX_CH_MAX] = {157, 158};
	unsigned int tx_ch[WCN_CDC_SLIM_TX_CH_MAX]  = {159, 160, 161};
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	return snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
					   tx_ch, ARRAY_SIZE(rx_ch), rx_ch);
}

static int msm_wcn_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	u32 rx_ch[WCN_CDC_SLIM_RX_CH_MAX], tx_ch[WCN_CDC_SLIM_TX_CH_MAX];
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	int ret;

	dev_dbg(rtd->dev, "%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
	if (ret) {
		dev_err(rtd->dev,
			"%s: failed to get BTFM codec chan map\n, err:%d\n",
			__func__, ret);
		goto exit;
	}

	dev_dbg(rtd->dev, "%s: tx_ch_cnt(%d) be_id %d\n",
		__func__, tx_ch_cnt, dai_link->be_id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, rx_ch_cnt, rx_ch);
	if (ret)
		dev_err(rtd->dev, "%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);

exit:
	return ret;
}

static int msm_aux_pcm_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int index = cpu_dai->id - 1;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__, substream->name, substream->stream,
		cpu_dai->name, cpu_dai->id);

	if (index < PRIM_AUX_PCM || index > QUAT_AUX_PCM) {
		ret = -EINVAL;
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, cpu_dai->id);
		goto done;
	}

	mutex_lock(&auxpcm_intf_conf[index].lock);
	if (++auxpcm_intf_conf[index].ref_cnt == 1) {
		if (mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr != NULL) {
			mutex_lock(&mi2s_auxpcm_conf[index].lock);
			iowrite32(1,
				mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr);
			mutex_unlock(&mi2s_auxpcm_conf[index].lock);
		} else {
			dev_err(rtd->card->dev,
				"%s lpaif_tert_muxsel_virt_addr is NULL\n",
				__func__);
			ret = -EINVAL;
		}
	}
	if (IS_ERR_VALUE(ret))
		auxpcm_intf_conf[index].ref_cnt--;

	mutex_unlock(&auxpcm_intf_conf[index].lock);

done:
	return ret;
}

static void msm_aux_pcm_snd_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int index = rtd->cpu_dai->id - 1;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__,
		substream->name, substream->stream,
		rtd->cpu_dai->name, rtd->cpu_dai->id);

	if (index < PRIM_AUX_PCM || index > QUAT_AUX_PCM) {
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, rtd->cpu_dai->id);
		return;
	}

	mutex_lock(&auxpcm_intf_conf[index].lock);
	if (--auxpcm_intf_conf[index].ref_cnt == 0) {
		if (mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr != NULL) {
			mutex_lock(&mi2s_auxpcm_conf[index].lock);
			iowrite32(0,
				mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr);
			mutex_unlock(&mi2s_auxpcm_conf[index].lock);
		} else {
			dev_err(rtd->card->dev,
				"%s lpaif_tert_muxsel_virt_addr is NULL\n",
				__func__);
		}
	}
	mutex_unlock(&auxpcm_intf_conf[index].lock);
}

static int msm_get_port_id(int be_id)
{
	int afe_port_id;

	switch (be_id) {
	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_PRI_MI2S_TX:
		afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_SECONDARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_SECONDARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_TERTIARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
		break;
	default:
		pr_err("%s: Invalid be_id: %d\n", __func__, be_id);
		afe_port_id = -EINVAL;
	}

	return afe_port_id;
}

static u32 get_mi2s_bits_per_sample(u32 bit_format)
{
	u32 bit_per_sample;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		bit_per_sample = 32;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bit_per_sample = 16;
		break;
	}

	return bit_per_sample;
}

static void update_mi2s_clk_val(int dai_id, int stream)
{
	u32 bit_per_sample;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		bit_per_sample =
		    get_mi2s_bits_per_sample(mi2s_rx_cfg[dai_id].bit_format);
		mi2s_clk[dai_id].clk_freq_in_hz =
		    mi2s_rx_cfg[dai_id].sample_rate * 2 * bit_per_sample;
	} else {
		bit_per_sample =
		    get_mi2s_bits_per_sample(mi2s_tx_cfg[dai_id].bit_format);
		mi2s_clk[dai_id].clk_freq_in_hz =
		    mi2s_tx_cfg[dai_id].sample_rate * 2 * bit_per_sample;
	}

	if (!mi2s_intf_conf[dai_id].msm_is_mi2s_master)
		mi2s_clk[dai_id].clk_freq_in_hz = 0;
}

static int msm_mi2s_set_sclk(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int port_id = 0;
	int index = cpu_dai->id;

	port_id = msm_get_port_id(rtd->dai_link->be_id);
	if (IS_ERR_VALUE(port_id)) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		ret = port_id;
		goto done;
	}

	if (enable) {
		update_mi2s_clk_val(index, substream->stream);
		dev_dbg(rtd->card->dev, "%s: clock rate %ul\n", __func__,
			mi2s_clk[index].clk_freq_in_hz);
	}

	mi2s_clk[index].enable = enable;
	ret = afe_set_lpass_clock_v2(port_id,
				     &mi2s_clk[index]);
	if (ret < 0) {
		dev_err(rtd->card->dev,
			"%s: afe lpass clock failed for port 0x%x , err:%d\n",
			__func__, port_id, ret);
		goto done;
	}

done:
	return ret;
}

static struct snd_soc_jack msm_cm710x_hp_jack;

static struct snd_soc_jack_pin msm_cm710x_hp_jack_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};

static struct snd_soc_jack_gpio msm_cm710x_hp_jack_gpios[] = {
	[0] = {
		.name = "Left headphone detection",
		.report = SND_JACK_HEADPHONE,
		.debounce_time = 150,
		.invert = 1,
	},
	[1] = {
		.name = "Right headphone detection",
		.report = SND_JACK_HEADPHONE,
		.debounce_time = 150,
		.invert = 1,
	},
};

static int msm_cm710x_jack_status_check(void *data)
{
	int left_jack_enabled = !gpio_get_value(
			msm_cm710x_hp_jack_gpios[0].gpio);
	int right_jack_enabled = !gpio_get_value(
			msm_cm710x_hp_jack_gpios[1].gpio);

	return left_jack_enabled | right_jack_enabled;
}

static int msm_cm710x_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(rtd->card);

	dev_dbg(codec->dev, "%s: Entry\n", __func__);

	ret = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (ret < 0) {
		dev_err(codec->dev, "%s: add_codec_controls failed, err%d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_dapm_widgets,
				ARRAY_SIZE(msm_dapm_widgets));

	/* headphone jack detection */
	ret = snd_soc_card_jack_new(rtd->card, "Headphone Jack",
		SND_JACK_HEADPHONE, &msm_cm710x_hp_jack,
		msm_cm710x_hp_jack_pins, ARRAY_SIZE(msm_cm710x_hp_jack_pins));
	if (ret < 0) {
		dev_err(codec->dev, "%s: card_jack_new failed, err = %d\n",
			__func__, ret);
		return ret;
	}

	if (gpio_is_valid(pdata->left_jack_detect_gpio)) {
		msm_cm710x_hp_jack_gpios[0].gpio =
		    pdata->left_jack_detect_gpio;

		msm_cm710x_hp_jack_gpios[0].jack_status_check =
			msm_cm710x_jack_status_check;
	}

	if (gpio_is_valid(pdata->right_jack_detect_gpio)) {
		msm_cm710x_hp_jack_gpios[1].gpio =
		    pdata->right_jack_detect_gpio;

		msm_cm710x_hp_jack_gpios[1].jack_status_check =
			msm_cm710x_jack_status_check;
	}

	ret = snd_soc_jack_add_gpios(&msm_cm710x_hp_jack,
		ARRAY_SIZE(msm_cm710x_hp_jack_gpios),
		msm_cm710x_hp_jack_gpios);

	if (ret < 0)
		dev_err(codec->dev, "%s: jack_add_gpios failed, err = %d\n",
			__func__, ret);


	snd_soc_dapm_enable_pin(dapm, "Speaker Jack");
	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
	snd_soc_dapm_enable_pin(dapm, "Mic Jack");
	snd_soc_dapm_ignore_suspend(dapm, "OUTL");
	snd_soc_dapm_ignore_suspend(dapm, "OUTR");
	snd_soc_dapm_ignore_suspend(dapm, "INL");
	snd_soc_dapm_ignore_suspend(dapm, "INR");

	snd_soc_dapm_sync(dapm);

	return 0;
}

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int index = cpu_dai->id;
	unsigned int fmt = SND_SOC_DAIFMT_CBS_CFS;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__, substream->name, substream->stream,
		cpu_dai->name, cpu_dai->id);

	if (index < PRI_MI2S || index > QUAT_MI2S) {
		ret = -EINVAL;
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, cpu_dai->id);
		goto done;
	}

	/*
	 * Muxtex protection in case the same MI2S
	 * interface using for both TX and RX  so
	 * that the same clock won't be enable twice.
	 */
	mutex_lock(&mi2s_intf_conf[index].lock);
	if (++mi2s_intf_conf[index].ref_cnt == 1) {
		ret = msm_mi2s_set_sclk(substream, true);
		if (IS_ERR_VALUE(ret)) {
			dev_err(rtd->card->dev,
				"%s: afe lpass clock failed to enable MI2S clock, err:%d\n",
				__func__, ret);
			goto clean_up;
		}
		if (mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr != NULL) {
			mutex_lock(&mi2s_auxpcm_conf[index].lock);
			iowrite32(0,
				mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr);
			mutex_unlock(&mi2s_auxpcm_conf[index].lock);
		} else {
			dev_err(rtd->card->dev,
				"%s lpaif_muxsel_virt_addr is NULL for dai %d\n",
				__func__, index);
			ret = -EINVAL;
			goto clk_off;
		}
		/* Check if msm needs to provide the clock to the interface */
		if (!mi2s_intf_conf[index].msm_is_mi2s_master)
			fmt = SND_SOC_DAIFMT_CBM_CFM;
		ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s: set fmt cpu dai failed for MI2S (%d), err:%d\n",
				__func__, index, ret);
			goto clk_off;
		}
	}
clk_off:
	if (IS_ERR_VALUE(ret))
		msm_mi2s_set_sclk(substream, false);
clean_up:
	if (IS_ERR_VALUE(ret))
		mi2s_intf_conf[index].ref_cnt--;
	mutex_unlock(&mi2s_intf_conf[index].lock);
done:
	return ret;
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int index = rtd->cpu_dai->id;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	if (index < PRI_MI2S || index > QUAT_MI2S) {
		pr_err("%s:invalid MI2S DAI(%d)\n", __func__, index);
		return;
	}

	mutex_lock(&mi2s_intf_conf[index].lock);
	if (--mi2s_intf_conf[index].ref_cnt == 0) {
		ret = msm_mi2s_set_sclk(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed for MI2S (%d); ret=%d\n",
				__func__, index, ret);
	}
	mutex_unlock(&mi2s_intf_conf[index].lock);
}

static struct snd_soc_ops msm_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_aux_pcm_be_ops = {
	.startup = msm_aux_pcm_snd_startup,
	.shutdown = msm_aux_pcm_snd_shutdown,
};

static struct snd_soc_ops msm_wcn_ops = {
	.hw_params = msm_wcn_hw_params,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = MSM_DAILINK_NAME(Media1),
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = MSM_DAILINK_NAME(Media2),
		.stream_name = "MultiMedia2",
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name = "VoiceMMode1",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name = "VoIP",
		.platform_name = "msm-voip-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = MSM_DAILINK_NAME(ULL),
		.stream_name = "MultiMedia3",
		.cpu_dai_name = "MultiMedia3",
		.platform_name = "msm-pcm-dsp.2",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name = "SLIMBUS0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name = "msm-pcm-afe",
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = MSM_DAILINK_NAME(Compress1),
		.stream_name = "Compress1",
		.cpu_dai_name = "MultiMedia4",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name = "AUXPCM_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_1 Hostless",
		.stream_name = "SLIMBUS_1 Hostless",
		.cpu_dai_name = "SLIMBUS1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_3 Hostless",
		.stream_name = "SLIMBUS_3 Hostless",
		.cpu_dai_name = "SLIMBUS3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_4 Hostless",
		.stream_name = "SLIMBUS_4 Hostless",
		.cpu_dai_name = "SLIMBUS4_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = MSM_DAILINK_NAME(LowLatency),
		.stream_name = "MultiMedia5",
		.cpu_dai_name = "MultiMedia5",
		.platform_name = "msm-pcm-dsp.1",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	{
		.name = "Listen 1 Audio Service",
		.stream_name = "Listen 1 Audio Service",
		.cpu_dai_name = "LSM1",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM1,
	},
	/* Multiple Tunnel instances */
	{
		.name = MSM_DAILINK_NAME(Compress2),
		.stream_name = "Compress2",
		.cpu_dai_name = "MultiMedia7",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{
		.name = MSM_DAILINK_NAME(Compress3),
		.stream_name = "Compress3",
		.cpu_dai_name = "MultiMedia10",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{
		.name = MSM_DAILINK_NAME(ULL_NOIRQ),
		.stream_name = "MM_NOIRQ",
		.cpu_dai_name = "MultiMedia8",
		.platform_name = "msm-pcm-dsp-noirq",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
	},
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name = "VoiceMMode2",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	/* LSM FE */
	{
		.name = "Listen 2 Audio Service",
		.stream_name = "Listen 2 Audio Service",
		.cpu_dai_name = "LSM2",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM2,
	},
	{
		.name = "Listen 3 Audio Service",
		.stream_name = "Listen 3 Audio Service",
		.cpu_dai_name = "LSM3",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM3,
	},
	{
		.name = "Listen 4 Audio Service",
		.stream_name = "Listen 4 Audio Service",
		.cpu_dai_name = "LSM4",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM4,
	},
	{
		.name = "Listen 5 Audio Service",
		.stream_name = "Listen 5 Audio Service",
		.cpu_dai_name = "LSM5",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM5,
	},
	{
		.name = "Listen 6 Audio Service",
		.stream_name = "Listen 6 Audio Service",
		.cpu_dai_name = "LSM6",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM6,
	},
	{
		.name = "Listen 7 Audio Service",
		.stream_name = "Listen 7 Audio Service",
		.cpu_dai_name = "LSM7",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM7,
	},
	{
		.name = "Listen 8 Audio Service",
		.stream_name = "Listen 8 Audio Service",
		.cpu_dai_name = "LSM8",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM8,
	},
	{
		.name = MSM_DAILINK_NAME(Media9),
		.stream_name = "MultiMedia9",
		.cpu_dai_name = "MultiMedia9",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
	{
		.name = MSM_DAILINK_NAME(Compress4),
		.stream_name = "Compress4",
		.cpu_dai_name = "MultiMedia11",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{
		.name = MSM_DAILINK_NAME(Compress5),
		.stream_name = "Compress5",
		.cpu_dai_name = "MultiMedia12",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{
		.name = MSM_DAILINK_NAME(Compress6),
		.stream_name = "Compress6",
		.cpu_dai_name = "MultiMedia13",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{
		.name = MSM_DAILINK_NAME(Compress7),
		.stream_name = "Compress7",
		.cpu_dai_name = "MultiMedia14",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{
		.name = MSM_DAILINK_NAME(Compress8),
		.stream_name = "Compress8",
		.cpu_dai_name = "MultiMedia15",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{
		.name = MSM_DAILINK_NAME(ULL_NOIRQ_2),
		.stream_name = "MM_NOIRQ_2",
		.cpu_dai_name = "MultiMedia16",
		.platform_name = "msm-pcm-dsp-noirq",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	{
		.name = "SLIMBUS_8 Hostless",
		.stream_name = "SLIMBUS8_HOSTLESS Capture",
		.cpu_dai_name = "SLIMBUS8_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
};

static struct snd_soc_dai_link msm_common_be_dai_links[] = {
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6-dev.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_wcn_be_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_7_RX,
		.stream_name = "Slimbus7 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16398",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		/* BT codec driver determines capabilities based on
		 * dai name, bt codecdai name should always contains
		 * supported usecase information
		 */
		.codec_dai_name = "btfm_bt_sco_a2dp_slim_rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_7_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_wcn_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_7_TX,
		.stream_name = "Slimbus7 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16399",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		.codec_dai_name = "btfm_bt_sco_slim_tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_7_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_8_TX,
		.stream_name = "Slimbus8 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16401",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		.codec_dai_name = "btfm_fm_slim_tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_8_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.init = &msm_wcn_init,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link ext_disp_be_dai_link[] = {
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-ext-disp-audio-codec-rx",
		.codec_dai_name = "msm_hdmi_audio_codec_rx_dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* DISP PORT BACK END DAI Link */
	{
		.name = LPASS_BE_DISPLAY_PORT,
		.stream_name = "Display Port Playback",
		.cpu_dai_name = "msm-dai-q6-dp.24608",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-ext-disp-audio-codec-rx",
		.codec_dai_name = "msm_dp_audio_codec_rx_dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_DISPLAY_PORT_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_mi2s_be_dai_links[] = {
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "cm710x.8-002c",
		.codec_dai_name = "cm710x-aif1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init = &msm_cm710x_init,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "cm710x.8-002c",
		.codec_dai_name = "cm710x-aif1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_MI2S_RX,
		.stream_name = "Secondary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_SEC_MI2S_TX,
		.stream_name = "Secondary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_RX,
		.stream_name = "Tertiary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_auxpcm_be_dai_links[] = {
	/* Primary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Secondary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_SEC_AUXPCM_RX,
		.stream_name = "Sec AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_TX,
		.stream_name = "Sec AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Tertiary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_TERT_AUXPCM_RX,
		.stream_name = "Tert AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_TERT_AUXPCM_TX,
		.stream_name = "Tert AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Quaternary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_AUXPCM_RX,
		.stream_name = "Quat AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_QUAT_AUXPCM_TX,
		.stream_name = "Quat AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
};

static int msm_populate_dai_link_component_of_node(
					struct snd_soc_card *card)
{
	int i, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *np;

	if (!cdev) {
		pr_err("%s: Sound card device memory NULL\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < card->num_links; i++) {
		if (dai_link[i].platform_of_node && dai_link[i].cpu_of_node)
			continue;

		/* populate platform_of_node for snd card dai links */
		if (dai_link[i].platform_name &&
		    !dai_link[i].platform_of_node) {
			index = of_property_match_string(cdev->of_node,
						"asoc-platform-names",
						dai_link[i].platform_name);
			if (index < 0) {
				pr_err("%s: No match found for platform name: %s\n",
					__func__, dai_link[i].platform_name);
				ret = index;
				goto err;
			}
			np = of_parse_phandle(cdev->of_node, "asoc-platform",
					      index);
			if (!np) {
				pr_err("%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platform_name,
					index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platform_of_node = np;
			dai_link[i].platform_name = NULL;
		}

		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpu_dai_name && !dai_link[i].cpu_of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-cpu-names",
						 dai_link[i].cpu_dai_name);
			if (index >= 0) {
				np = of_parse_phandle(cdev->of_node, "asoc-cpu",
						index);
				if (!np) {
					pr_err("%s: retrieving phandle for cpu dai %s failed\n",
						__func__,
						dai_link[i].cpu_dai_name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].cpu_of_node = np;
				dai_link[i].cpu_dai_name = NULL;
			}
		}

		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].codec_name && !dai_link[i].codec_of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-codec-names",
						 dai_link[i].codec_name);
			if (index < 0)
				continue;
			np = of_parse_phandle(cdev->of_node, "asoc-codec",
					      index);
			if (!np) {
				pr_err("%s: retrieving phandle for codec %s failed\n",
					__func__, dai_link[i].codec_name);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].codec_of_node = np;
			dai_link[i].codec_name = NULL;
		}
	}

err:
	return ret;
}

static int msm_audrx_stub_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	ret = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (ret < 0) {
		dev_err(codec->dev, "%s: add_codec_controls failed, err%d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_dapm_widgets,
				ARRAY_SIZE(msm_dapm_widgets));

	return 0;
}

static int msm_snd_stub_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	int ret = 0;
	unsigned int rx_ch[] = {144, 145, 146, 147, 148, 149, 150,
				151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[] = {128, 129, 130, 131, 132, 133,
				134, 135, 136, 137, 138, 139,
				140, 141, 142, 143};

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  slim_rx_cfg[0].channels,
						  rx_ch);
		if (ret < 0)
			pr_err("%s: RX failed to set cpu chan map error %d\n",
				__func__, ret);
	} else {
		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  slim_tx_cfg[0].channels,
						  tx_ch, 0, 0);
		if (ret < 0)
			pr_err("%s: TX failed to set cpu chan map error %d\n",
				__func__, ret);
	}

	return ret;
}

static struct snd_soc_ops msm_stub_be_ops = {
	.hw_params = msm_snd_stub_hw_params,
};

static struct snd_soc_dai_link msm_stub_fe_dai_links[] = {

	/* FrontEnd DAI Links */
	{
		.name = "MSMSTUB Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
};

static struct snd_soc_dai_link msm_stub_be_dai_links[] = {

	/* Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_stub_init,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
		.ops = &msm_stub_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_stub_be_ops,
	},
};

static struct snd_soc_dai_link msm_stub_dai_links[
			 ARRAY_SIZE(msm_stub_fe_dai_links) +
			 ARRAY_SIZE(msm_stub_be_dai_links) +
			 ARRAY_SIZE(ext_disp_be_dai_link) +
			 ARRAY_SIZE(msm_mi2s_be_dai_links) +
			 ARRAY_SIZE(msm_auxpcm_be_dai_links)];

static struct snd_soc_dai_link msm_cm710x_dai_links[
			 ARRAY_SIZE(msm_common_dai_links) +
			 ARRAY_SIZE(msm_common_be_dai_links) +
			 ARRAY_SIZE(msm_wcn_be_dai_links) +
			 ARRAY_SIZE(msm_mi2s_be_dai_links) +
			 ARRAY_SIZE(msm_auxpcm_be_dai_links)];

struct snd_soc_card snd_soc_card_stub_msm = {
	.name		= "msm8998-stub-snd-card",
};

struct snd_soc_card snd_soc_card_cm710x_msm = {
	.name		= "msm8998-cm710x-snd-card",
};

static const struct of_device_id msm8998_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8998-asoc-snd-stub",
	  .data = "stub_codec"},
	{ .compatible = "qcom,msm8998-asoc-snd-cm710x",
	  .data = "cm710x_codec"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int len_1, len_2;
	int total_links;
	const struct of_device_id *match;

	match = of_match_node(msm8998_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "stub_codec")) {
		card = &snd_soc_card_stub_msm;
		len_1 = ARRAY_SIZE(msm_stub_fe_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm_stub_be_dai_links);

		memcpy(msm_stub_dai_links,
		       msm_stub_fe_dai_links,
		       sizeof(msm_stub_fe_dai_links));
		memcpy(msm_stub_dai_links + len_1,
		       msm_stub_be_dai_links,
		       sizeof(msm_stub_be_dai_links));

		total_links = len_2;
		if (of_property_read_bool(dev->of_node,
			    "qcom,ext-disp-audio-rx")) {
			dev_dbg(dev, "%s(): External display audio support present\n",
				__func__);
			memcpy(msm_stub_dai_links + total_links,
				ext_disp_be_dai_link,
				sizeof(ext_disp_be_dai_link));
			total_links += ARRAY_SIZE(ext_disp_be_dai_link);
		}
		if (of_property_read_bool(dev->of_node,
			    "qcom,mi2s-audio-intf")) {
			memcpy(msm_stub_dai_links + total_links,
				msm_mi2s_be_dai_links,
				sizeof(msm_mi2s_be_dai_links));
			total_links += ARRAY_SIZE(msm_mi2s_be_dai_links);
		}
		if (of_property_read_bool(dev->of_node,
			    "qcom,auxpcm-audio-intf")) {
			memcpy(msm_stub_dai_links + total_links,
				msm_auxpcm_be_dai_links,
				sizeof(msm_auxpcm_be_dai_links));
			total_links += ARRAY_SIZE(msm_auxpcm_be_dai_links);
		}
		dailink = msm_stub_dai_links;
	}

	if (!strcmp(match->data, "cm710x_codec")) {
		card = &snd_soc_card_cm710x_msm;
		len_1 = ARRAY_SIZE(msm_common_dai_links);

		memcpy(msm_cm710x_dai_links,
			msm_common_dai_links,
			sizeof(msm_common_dai_links));

		len_2 = ARRAY_SIZE(msm_common_be_dai_links);

		memcpy(msm_cm710x_dai_links + len_1,
			msm_common_be_dai_links,
			sizeof(msm_common_be_dai_links));

		total_links = len_1 + len_2;

		if (of_property_read_bool(dev->of_node, "qcom,wcn-btfm")) {
			dev_dbg(dev, "%s(): WCN BTFM support present\n",
				__func__);
			memcpy(msm_cm710x_dai_links + total_links,
			       msm_wcn_be_dai_links,
			       sizeof(msm_wcn_be_dai_links));
			total_links += ARRAY_SIZE(msm_wcn_be_dai_links);
		}

		if (of_property_read_bool(dev->of_node,
					  "qcom,mi2s-audio-intf")) {
			memcpy(msm_cm710x_dai_links + total_links,
			       msm_mi2s_be_dai_links,
			       sizeof(msm_mi2s_be_dai_links));
			total_links += ARRAY_SIZE(msm_mi2s_be_dai_links);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,auxpcm-audio-intf")) {
			memcpy(msm_cm710x_dai_links + total_links,
			msm_auxpcm_be_dai_links,
			sizeof(msm_auxpcm_be_dai_links));
			total_links += ARRAY_SIZE(msm_auxpcm_be_dai_links);
		}
		dailink = msm_cm710x_dai_links;
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = total_links;
	}

	return card;
}

static void i2s_auxpcm_init(struct platform_device *pdev)
{
	struct resource *muxsel;
	int count;
	u32 mi2s_master_slave[MI2S_MAX];
	int ret;
	char *str[PCM_I2S_SEL_MAX] = {
		"lpaif_pri_mode_muxsel",
		"lpaif_sec_mode_muxsel",
		"lpaif_tert_mode_muxsel",
		"lpaif_quat_mode_muxsel"
	};

	for (count = 0; count < MI2S_MAX; count++) {
		mutex_init(&mi2s_intf_conf[count].lock);
		mi2s_intf_conf[count].ref_cnt = 0;
	}

	for (count = 0; count < AUX_PCM_MAX; count++) {
		mutex_init(&auxpcm_intf_conf[count].lock);
		auxpcm_intf_conf[count].ref_cnt = 0;
	}

	for (count = 0; count < PCM_I2S_SEL_MAX; count++) {
		mutex_init(&mi2s_auxpcm_conf[count].lock);
		mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr = NULL;
	}

	for (count = 0; count < PCM_I2S_SEL_MAX; count++) {
		muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						      str[count]);
		if (muxsel) {
			mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr
				= ioremap(muxsel->start, resource_size(muxsel));
		}
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-mi2s-master",
			mi2s_master_slave, MI2S_MAX);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: no qcom,msm-mi2s-master in DT node\n",
			__func__);
	} else {
		for (count = 0; count < MI2S_MAX; count++) {
			mi2s_intf_conf[count].msm_is_mi2s_master =
				mi2s_master_slave[count];
		}
	}
}

static void i2s_auxpcm_deinit(void)
{
	int count;

	for (count = 0; count < PCM_I2S_SEL_MAX; count++)
		if (mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr !=
			NULL)
			iounmap(
			mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr);
}

static int msm_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;
	char *mclk_freq_prop_name;
	const struct of_device_id *match;
	struct pinctrl *pinctrl = NULL;
	struct pinctrl_state *pins_default = NULL;

	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	/* headphone jack detection */
	pdata->left_jack_detect_gpio = of_get_named_gpio(pdev->dev.of_node,
					"cm710x,left-jack-detect-gpio", 0);
	if (pdata->left_jack_detect_gpio < 0) {
		dev_err(&pdev->dev, "%s: failed to get %s\n", __func__,
					"cm710x,left-jack-detect-gpio");
	}
	pdata->right_jack_detect_gpio = of_get_named_gpio(pdev->dev.of_node,
					"cm710x,right-jack-detect-gpio", 0);
	if (pdata->right_jack_detect_gpio < 0) {
		dev_err(&pdev->dev, "%s: failed to get %s\n", __func__,
					"cm710x,right-jack-detect-gpio");
	}

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(&pdev->dev, "%s: Failed to get pin ctrl\n", __func__);
		ret = PTR_ERR(pinctrl);
		goto err;
	}
	pins_default = pinctrl_lookup_state(pinctrl,
			"headphone_jack_detect_default");
	if (IS_ERR_OR_NULL(pins_default)) {
		dev_err(&pdev->dev,
			"%s: Failed to lookup pinctrl headphone jack default state\n",
			__func__);
		ret = PTR_ERR(pins_default);
		goto pinctrl_err;
	}
	ret = pinctrl_select_state(pinctrl, pins_default);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"%s: Failed to set headphone jack pin state\n",
			__func__);
		goto pinctrl_err;
	}

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(&pdev->dev, "parse card name failed, err:%d\n",
			ret);
		goto err;
	}

	ret = snd_soc_of_parse_audio_routing(card, "qcom,audio-routing");
	if (ret) {
		dev_err(&pdev->dev, "parse audio routing failed, err:%d\n",
			ret);
		goto err;
	}

	match = of_match_node(msm8998_asoc_machine_of_match,
			pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "%s: no matched codec is found.\n",
			__func__);
		goto err;
	}

	if (!strcmp(match->data, "cm710x_codec"))
		mclk_freq_prop_name = "qcom,cm710x-mclk-clk-freq";
	else
		mclk_freq_prop_name = "qcom,stub-mclk-clk-freq";

	ret = of_property_read_u32(pdev->dev.of_node,
			mclk_freq_prop_name, &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed, err%d\n",
			mclk_freq_prop_name,
			pdev->dev.of_node->full_name, ret);
		goto err;
	}

	if ((pdata->mclk_freq != CODEC_EXT_CLK_RATE) &&
		(pdata->mclk_freq != CM710X_CODEC_EXT_CLK_RATE)) {
		dev_err(&pdev->dev, "unsupported mclk freq %u\n",
			pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}

	ret = msm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret == -EPROBE_DEFER) {
		if (codec_reg_done)
			ret = -EINVAL;
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);
	spdev = pdev;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: failed to add child nodes, ret=%d\n",
			__func__, ret);
	} else {
		pdata->hph_en1_gpio_p = of_parse_phandle(pdev->dev.of_node,
							"qcom,hph-en1-gpio", 0);
		if (!pdata->hph_en1_gpio_p) {
			dev_dbg(&pdev->dev, "property %s not detected in node %s",
				"qcom,hph-en1-gpio",
				pdev->dev.of_node->full_name);
		}

		pdata->hph_en0_gpio_p = of_parse_phandle(pdev->dev.of_node,
							"qcom,hph-en0-gpio", 0);
		if (!pdata->hph_en0_gpio_p) {
			dev_dbg(&pdev->dev, "property %s not detected in node %s",
				"qcom,hph-en0-gpio",
				pdev->dev.of_node->full_name);
		}
	}

	i2s_auxpcm_init(pdev);

	is_initial_boot = true;
	ret = audio_notifier_register("msm8998", AUDIO_NOTIFIER_ADSP_DOMAIN,
				      &service_nb);
	if (ret < 0)
		pr_err("%s: Audio notifier register failed ret = %d\n",
			__func__, ret);

	return 0;

pinctrl_err:
	devm_pinctrl_put(pinctrl);
err:
	if (pdata->us_euro_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free us_euro gpio %d\n",
			__func__, pdata->us_euro_gpio);
		gpio_free(pdata->us_euro_gpio);
		pdata->us_euro_gpio = 0;
	}
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	gpio_free(pdata->us_euro_gpio);
	i2s_auxpcm_deinit();

	snd_soc_unregister_card(card);
	return 0;
}

static struct platform_driver msm8998_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8998_asoc_machine_of_match,
	},
	.probe = msm_asoc_machine_probe,
	.remove = msm_asoc_machine_remove,
};
module_platform_driver(msm8998_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8998_asoc_machine_of_match);
