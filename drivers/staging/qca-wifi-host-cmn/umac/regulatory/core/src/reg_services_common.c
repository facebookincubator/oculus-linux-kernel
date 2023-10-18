/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: reg_services_common.c
 * This file defines regulatory component service functions
 */

#include <wlan_cmn.h>
#include <reg_services_public_struct.h>
#include <wlan_reg_services_api.h>
#ifdef CONFIG_AFC_SUPPORT
#include "reg_opclass.h"
#endif
#include <wlan_objmgr_psoc_obj.h>
#include <qdf_lock.h>
#include "reg_priv_objs.h"
#include "reg_utils.h"
#include "reg_callbacks.h"
#include "reg_services_common.h"
#include <wlan_objmgr_psoc_obj.h>
#include "reg_db.h"
#include "reg_db_parser.h"
#include "reg_build_chan_list.h"
#include <wlan_objmgr_pdev_obj.h>
#include <target_if.h>
#ifdef WLAN_FEATURE_GET_USABLE_CHAN_LIST
#include "wlan_mlme_ucfg_api.h"
#include "wlan_nan_api.h"
#endif
#ifndef CONFIG_REG_CLIENT
#include <wlan_reg_channel_api.h>
#endif

const struct chan_map *channel_map;
uint8_t g_reg_max_5g_chan_num;

#ifdef WLAN_FEATURE_11BE
static bool reg_is_chan_bit_punctured(uint16_t input_punc_bitmap,
				      uint8_t chan_idx)
{
	return input_punc_bitmap & BIT(chan_idx);
}
#else
static bool reg_is_chan_bit_punctured(uint16_t in_punc_bitmap,
				      uint8_t chan_idx)
{
	return false;
}
#endif

#ifdef CONFIG_CHAN_FREQ_API
/* bonded_chan_40mhz_list_freq - List of 40MHz bonnded channel frequencies */
static const struct bonded_channel_freq bonded_chan_40mhz_list_freq[] = {
	{5180, 5200},
	{5220, 5240},
	{5260, 5280},
	{5300, 5320},
	{5500, 5520},
	{5540, 5560},
	{5580, 5600},
	{5620, 5640},
	{5660, 5680},
	{5700, 5720},
	{5745, 5765},
	{5785, 5805},
	{5825, 5845},
	{5865, 5885},
#ifdef CONFIG_BAND_6GHZ
	{5955, 5975},
	{5995, 6015},
	{6035, 6055},
	{6075, 6095},
	{6115, 6135},
	{6155, 6175},
	{6195, 6215},
	{6235, 6255},
	{6275, 6295},
	{6315, 6335},
	{6355, 6375},
	{6395, 6415},
	{6435, 6455},
	{6475, 6495},
	{6515, 6535},
	{6555, 6575},
	{6595, 6615},
	{6635, 6655},
	{6675, 6695},
	{6715, 6735},
	{6755, 6775},
	{6795, 6815},
	{6835, 6855},
	{6875, 6895},
	{6915, 6935},
	{6955, 6975},
	{6995, 7015},
	{7035, 7055},
	{7075, 7095}
#endif /*CONFIG_BAND_6GHZ*/
};

/* bonded_chan_80mhz_list_freq - List of 80MHz bonnded channel frequencies */
static const struct bonded_channel_freq bonded_chan_80mhz_list_freq[] = {
	{5180, 5240},
	{5260, 5320},
	{5500, 5560},
	{5580, 5640},
	{5660, 5720},
	{5745, 5805},
	{5825, 5885},
#ifdef CONFIG_BAND_6GHZ
	{5955, 6015},
	{6035, 6095},
	{6115, 6175},
	{6195, 6255},
	{6275, 6335},
	{6355, 6415},
	{6435, 6495},
	{6515, 6575},
	{6595, 6655},
	{6675, 6735},
	{6755, 6815},
	{6835, 6895},
	{6915, 6975},
	{6995, 7055}
#endif /*CONFIG_BAND_6GHZ*/
};

/* bonded_chan_160mhz_list_freq - List of 160MHz bonnded channel frequencies */
static const struct bonded_channel_freq bonded_chan_160mhz_list_freq[] = {
	{5180, 5320},
	{5500, 5640},
	{5745, 5885},
#ifdef CONFIG_BAND_6GHZ
	{5955, 6095},
	{6115, 6255},
	{6275, 6415},
	{6435, 6575},
	{6595, 6735},
	{6755, 6895},
	{6915, 7055}
#endif /*CONFIG_BAND_6GHZ*/
};

#ifdef WLAN_FEATURE_11BE
/* bonded_chan_320mhz_list_freq - List of 320MHz bonnded channel frequencies */
static const struct bonded_channel_freq bonded_chan_320mhz_list_freq[] = {
	{5500, 5720}, /* center freq: 5650: The 5Ghz 240MHz chan */
#ifdef CONFIG_BAND_6GHZ
	{5955, 6255}, /* center freq: 6105 */
	{6115, 6415}, /* center freq: 6265 */
	{6275, 6575}, /* center freq: 6425 */
	{6435, 6735}, /* center freq: 6585 */
	{6595, 6895}, /* center freq: 6745 */
	{6755, 7055}  /* center freq: 6905 */
#endif /*CONFIG_BAND_6GHZ*/
};
#endif

/**
 * struct bw_bonded_array_pair - Structure containing bandwidth, bonded_array
 * corresponding to bandwidth and the size of the bonded array.
 * @chwidth: channel width
 * @bonded_chan_arr: bonded array corresponding to chwidth.
 * @array_size: size of the bonded_chan_arr.
 */
struct bw_bonded_array_pair {
	enum phy_ch_width chwidth;
	const struct bonded_channel_freq *bonded_chan_arr;
	uint16_t array_size;
};

/* Mapping of chwidth to bonded array and size of bonded array */
static const
struct bw_bonded_array_pair bw_bonded_array_pair_map[] = {
#ifdef WLAN_FEATURE_11BE
	{CH_WIDTH_320MHZ, bonded_chan_320mhz_list_freq,
		QDF_ARRAY_SIZE(bonded_chan_320mhz_list_freq)},
#endif
	{CH_WIDTH_160MHZ, bonded_chan_160mhz_list_freq,
		QDF_ARRAY_SIZE(bonded_chan_160mhz_list_freq)},
	{CH_WIDTH_80P80MHZ, bonded_chan_80mhz_list_freq,
		QDF_ARRAY_SIZE(bonded_chan_80mhz_list_freq)},
	{CH_WIDTH_80MHZ, bonded_chan_80mhz_list_freq,
		QDF_ARRAY_SIZE(bonded_chan_80mhz_list_freq)},
	{CH_WIDTH_40MHZ, bonded_chan_40mhz_list_freq,
		QDF_ARRAY_SIZE(bonded_chan_40mhz_list_freq)},
};

#ifdef WLAN_FEATURE_11BE
/** Binary bitmap pattern
 * 1: Punctured 20Mhz chan 0:non-Punctured 20Mhz Chan
 *
 * Band: 80MHz  Puncturing Unit: 20Mhz
 *  B0001 = 0x1  : BIT(0)
 *  B0010 = 0x2  : BIT(1)
 *  B0100 = 0x4  : BIT(2)
 *  B1000 = 0x8  : BIT(3)
 *
 * Band: 160MHz  Puncturing Unit: 20Mhz
 *  B0000_0001 = 0x01  : BIT(0)
 *  B0000_0010 = 0x02  : BIT(1)
 *  B0000_0100 = 0x04  : BIT(2)
 *  B0000_1000 = 0x08  : BIT(3)
 *  B0001_0000 = 0x10  : BIT(4)
 *  B0010_0000 = 0x20  : BIT(5)
 *  B0100_0000 = 0x40  : BIT(6)
 *  B1000_0000 = 0x80  : BIT(7)
 *
 * Band: 160MHz  Puncturing Unit: 40Mhz
 *  B0000_0011 = 0x03  : BIT(0) | BIT(1)
 *  B0000_1100 = 0x0C  : BIT(2) | BIT(3)
 *  B0011_0000 = 0x30  : BIT(4) | BIT(5)
 *  B1100_0000 = 0xC0  : BIT(6) | BIT(7)
 *
 * Band: 320MHz  Puncturing Unit: 40Mhz
 *  B0000_0000_0000_0011 = 0x0003  : BIT(0)  | BIT(1)
 *  B0000_0000_0000_1100 = 0x000C  : BIT(2)  | BIT(3)
 *  B0000_0000_0011_0000 = 0x0030  : BIT(4)  | BIT(5)
 *  B0000_0000_1100_0000 = 0x00C0  : BIT(6)  | BIT(7)
 *  B0000_0011_0000_0000 = 0x0300  : BIT(8)  | BIT(9)
 *  B0000_1100_0000_0000 = 0x0C00  : BIT(10) | BIT(11)
 *  B0011_0000_0000_0000 = 0x3000  : BIT(12) | BIT(13)
 *  B1100_0000_0000_0000 = 0xC000  : BIT(13) | BIT(15)
 *
 * Band: 320MHz  Puncturing Unit: 80Mhz
 *  B0000_0000_0000_1111 = 0x000F  : BIT(0)  | BIT(1) | BIT(2) | BIT(3)
 *  B0000_0000_1111_0000 = 0x00F0  : BIT(4)  | BIT(5) | BIT(6) | BIT(7)
 *  B0000_1111_0000_0000 = 0x0F00  : BIT(8)  | BIT(9) | BIT(10) | BIT(11)
 *  B1111_0000_0000_0000 = 0xF000  : BIT(12) | BIT(13) | BIT(14) | BIT(15)
 *
 * Band: 320MHz  Puncturing Unit: 80Mhz+40Mhz (Right 80Mhz punctured)
 *  B0000_0000_0011_1111 = 0x003F  : BIT(4)  | BIT(5)   [right 80MHz: BIT(0) | BIT(1) | BIT(2) | BIT(3)]
 *  B0000_0000_1100_1111 = 0x00CF  : BIT(6)  | BIT(7)   [right 80MHz: BIT(0) | BIT(1) | BIT(2) | BIT(3)]
 *  B0000_0011_0000_1111 = 0x030F  : BIT(8)  | BIT(9)   [right 80MHz: BIT(0) | BIT(1) | BIT(2) | BIT(3)]
 *  B0000_1100_0000_1111 = 0x0C0F  : BIT(10) | BIT(11)  [right 80MHz: BIT(0) | BIT(1) | BIT(2) | BIT(3)]
 *  B0011_0000_0000_1111 = 0x300F  : BIT(12) | BIT(13)  [right 80MHz: BIT(0) | BIT(1) | BIT(2) | BIT(3)]
 *  B1100_0000_0000_1111 = 0xC00F  : BIT(14) | BIT(15)  [right 80MHz: BIT(0) | BIT(1) | BIT(2) | BIT(3)]
 *
 * Band: 320MHz  Puncturing Unit: 80Mhz+40Mhz (Left 80Mhz punctured)
 *  B1111_0000_0000_0011 = 0xF003  : BIT(4)  | BIT(5)   [left 80MHz: BIT(12) | BIT(13) | BIT(14) | BIT(15)]
 *  B1111_0000_0000_1100 = 0xF00C  : BIT(6)  | BIT(7)   [left 80MHz: BIT(12) | BIT(13) | BIT(14) | BIT(15)]
 *  B1111_0000_0011_0000 = 0xF030  : BIT(8)  | BIT(9)   [left 80MHz: BIT(12) | BIT(13) | BIT(14) | BIT(15)]
 *  B1111_0000_1100_0000 = 0xF0C0  : BIT(10) | BIT(11)  [left 80MHz: BIT(12) | BIT(13) | BIT(14) | BIT(15)]
 *  B1111_0011_0000_0000 = 0xF300  : BIT(12) | BIT(13)  [left 80MHz: BIT(12) | BIT(13) | BIT(14) | BIT(15)]
 *  B1111_1100_0000_0000 = 0xFC00  : BIT(14) | BIT(15)  [left 80MHz: BIT(12) | BIT(13) | BIT(14) | BIT(15)]
 */
static const uint16_t chan_80mhz_puncture_bitmap[] = {
	/* 20Mhz puncturing pattern */
	0x1,
	0x2,
	0x4,
	0x8
};

static const uint16_t chan_160mhz_puncture_bitmap[] = {
	/* 20Mhz puncturing pattern */
	0x1,
	0x2,
	0x4,
	0x8,
	0x10,
	0x20,
	0x40,
	0x80,
	/* 40Mhz puncturing pattern */
	0x3,
	0xc,
	0x30,
	0xc0
};

static const uint16_t chan_320mhz_puncture_bitmap[] = {
	/* 40Mhz puncturing pattern */
	0x3,
	0xc,
	0x30,
	0xc0,
	0x300,
	0xc00,
	0x3000,
	0xc000,
	/* 80Mhz puncturing pattern */
	0xf,
	0xf0,
	0xf00,
	0xf000,
	/* 80+40Mhz puncturing pattern: Left 80MHz punctured */
	0x3f,
	0xcf,
	0x30f,
	0xc0f,
	0x300f,
	0xc00f,
	/* 80+40Mhz puncturing pattern: Right 80MHz punctured */
	0xf003,
	0xf00c,
	0xf030,
	0xf0c0,
	0xf300,
	0xfc00
};

struct bw_puncture_bitmap_pair {
	enum phy_ch_width chwidth;
	const uint16_t *puncture_bitmap_arr;
	uint16_t array_size;
};

static const
struct bw_puncture_bitmap_pair bw_puncture_bitmap_pair_map[] = {
	{CH_WIDTH_320MHZ, chan_320mhz_puncture_bitmap,
		QDF_ARRAY_SIZE(chan_320mhz_puncture_bitmap)},
	{CH_WIDTH_160MHZ, chan_160mhz_puncture_bitmap,
		QDF_ARRAY_SIZE(chan_160mhz_puncture_bitmap)},
	{CH_WIDTH_80MHZ, chan_80mhz_puncture_bitmap,
		QDF_ARRAY_SIZE(chan_80mhz_puncture_bitmap)},
};

static inline qdf_freq_t
reg_get_band_cen_from_bandstart(uint16_t bw, qdf_freq_t bandstart)
{
	return bandstart - BW_10_MHZ + bw / 2;
}

#ifdef WLAN_FEATURE_11BE
uint16_t
reg_fetch_punc_bitmap(struct ch_params *ch_params)
{
	if (!ch_params)
		return NO_SCHANS_PUNC;

	return ch_params->input_punc_bitmap;
}
#endif

#else /* WLAN_FEATURE_11BE */
static inline qdf_freq_t
reg_get_band_cen_from_bandstart(uint16_t bw, qdf_freq_t bandstart)
{
	return 0;
}

#endif /* WLAN_FEATURE_11BE */

static bool reg_is_freq_within_bonded_chan(
		qdf_freq_t freq,
		const struct bonded_channel_freq *bonded_chan_arr,
		enum phy_ch_width chwidth, qdf_freq_t cen320_freq)
{
	qdf_freq_t band_center;

	if (reg_is_ch_width_320(chwidth) && cen320_freq) {
		/*
		 * For the 5GHz 320/240 MHz channel, bonded pair ends are not
		 * symmetric around the center of the channel. Use the start
		 * frequency of the bonded channel to calculate the center
		 */
		if (REG_IS_5GHZ_FREQ(freq)) {
			qdf_freq_t start_freq = bonded_chan_arr->start_freq;
			uint16_t bw = reg_get_bw_value(chwidth);

			band_center =
				reg_get_band_cen_from_bandstart(bw,
								start_freq);
		} else
			band_center = (bonded_chan_arr->start_freq +
					bonded_chan_arr->end_freq) >> 1;
		if (band_center != cen320_freq)
			return false;
	}

	if (freq >= bonded_chan_arr->start_freq &&
	    freq <= bonded_chan_arr->end_freq)
		return true;

	return false;
}

const struct bonded_channel_freq *
reg_get_bonded_chan_entry(qdf_freq_t freq,
			  enum phy_ch_width chwidth,
			  qdf_freq_t cen320_freq)
{
	const struct bonded_channel_freq *bonded_chan_arr;
	uint16_t array_size, i, num_bws;

	num_bws = QDF_ARRAY_SIZE(bw_bonded_array_pair_map);
	for (i = 0; i < num_bws; i++) {
		if (chwidth == bw_bonded_array_pair_map[i].chwidth) {
			bonded_chan_arr =
				bw_bonded_array_pair_map[i].bonded_chan_arr;
			array_size = bw_bonded_array_pair_map[i].array_size;
			break;
		}
	}
	if (i == num_bws) {
		reg_debug("Could not find bonded_chan_array for chwidth %d",
			  chwidth);
		return NULL;
	}

	for (i = 0; i < array_size; i++) {
		if (reg_is_freq_within_bonded_chan(freq, &bonded_chan_arr[i],
						   chwidth, cen320_freq))
			return &bonded_chan_arr[i];
	}

	reg_debug("Could not find a bonded pair for freq %d and width %d",
		  freq, chwidth);
	return NULL;
}

#endif /*CONFIG_CHAN_FREQ_API*/

enum phy_ch_width get_next_lower_bandwidth(enum phy_ch_width ch_width)
{
	static const enum phy_ch_width get_next_lower_bw[] = {
    /* 80+80 mode not supported in chips that support 320 mode */
#ifdef WLAN_FEATURE_11BE
		[CH_WIDTH_320MHZ] = CH_WIDTH_160MHZ,
#endif
		[CH_WIDTH_80P80MHZ] = CH_WIDTH_160MHZ,
		[CH_WIDTH_160MHZ] = CH_WIDTH_80MHZ,
		[CH_WIDTH_80MHZ] = CH_WIDTH_40MHZ,
		[CH_WIDTH_40MHZ] = CH_WIDTH_20MHZ,
		[CH_WIDTH_20MHZ] = CH_WIDTH_10MHZ,
		[CH_WIDTH_10MHZ] = CH_WIDTH_5MHZ,
		[CH_WIDTH_5MHZ] = CH_WIDTH_INVALID
	};

	return get_next_lower_bw[ch_width];
}

const struct chan_map channel_map_us[NUM_CHANNELS] = {
	[CHAN_ENUM_2412] = {2412, 1, 20, 40},
	[CHAN_ENUM_2417] = {2417, 2, 20, 40},
	[CHAN_ENUM_2422] = {2422, 3, 20, 40},
	[CHAN_ENUM_2427] = {2427, 4, 20, 40},
	[CHAN_ENUM_2432] = {2432, 5, 20, 40},
	[CHAN_ENUM_2437] = {2437, 6, 20, 40},
	[CHAN_ENUM_2442] = {2442, 7, 20, 40},
	[CHAN_ENUM_2447] = {2447, 8, 20, 40},
	[CHAN_ENUM_2452] = {2452, 9, 20, 40},
	[CHAN_ENUM_2457] = {2457, 10, 20, 40},
	[CHAN_ENUM_2462] = {2462, 11, 20, 40},
	[CHAN_ENUM_2467] = {2467, 12, 20, 40},
	[CHAN_ENUM_2472] = {2472, 13, 20, 40},
	[CHAN_ENUM_2484] = {2484, 14, 20, 20},
#ifdef CONFIG_49GHZ_CHAN
	[CHAN_ENUM_4912] = {4912, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4915] = {4915, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4917] = {4917, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4920] = {4920, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4922] = {4922, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4925] = {4925, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4927] = {4927, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4932] = {4932, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4935] = {4935, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4937] = {4937, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4940] = {4940, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4942] = {4942, 1, 5, 5},
	[CHAN_ENUM_4945] = {4945, 11, 10, 10},
	[CHAN_ENUM_4947] = {4947, 2, 5, 5},
	[CHAN_ENUM_4950] = {4950, 20, 10, 20},
	[CHAN_ENUM_4952] = {4952, 3, 5, 5},
	[CHAN_ENUM_4955] = {4955, 21, 10, 20},
	[CHAN_ENUM_4957] = {4957, 4, 5, 5},
	[CHAN_ENUM_4960] = {4960, 22, 10, 20},
	[CHAN_ENUM_4962] = {4962, 5, 5, 5},
	[CHAN_ENUM_4965] = {4965, 23, 10, 20},
	[CHAN_ENUM_4967] = {4967, 6, 5, 5},
	[CHAN_ENUM_4970] = {4970, 24, 10, 20},
	[CHAN_ENUM_4972] = {4972, 7, 5, 5},
	[CHAN_ENUM_4975] = {4975, 25, 10, 20},
	[CHAN_ENUM_4977] = {4977, 8, 5, 5},
	[CHAN_ENUM_4980] = {4980, 26, 10, 20},
	[CHAN_ENUM_4982] = {4982, 9, 5, 5},
	[CHAN_ENUM_4985] = {4985, 19, 10, 10},
	[CHAN_ENUM_4987] = {4987, 10, 5, 5},
	[CHAN_ENUM_5032] = {5032, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5035] = {5035, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5037] = {5037, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5040] = {5040, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5042] = {5042, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5045] = {5045, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5047] = {5047, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5052] = {5052, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5055] = {5055, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5057] = {5057, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5060] = {5060, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5080] = {5080, INVALID_CHANNEL_NUM, 2, 20},
#endif /* CONFIG_49GHZ_CHAN */
	[CHAN_ENUM_5180] = {5180, 36, 2, 160},
	[CHAN_ENUM_5200] = {5200, 40, 2, 160},
	[CHAN_ENUM_5220] = {5220, 44, 2, 160},
	[CHAN_ENUM_5240] = {5240, 48, 2, 160},
	[CHAN_ENUM_5260] = {5260, 52, 2, 160},
	[CHAN_ENUM_5280] = {5280, 56, 2, 160},
	[CHAN_ENUM_5300] = {5300, 60, 2, 160},
	[CHAN_ENUM_5320] = {5320, 64, 2, 160},
	[CHAN_ENUM_5500] = {5500, 100, 2, 240},
	[CHAN_ENUM_5520] = {5520, 104, 2, 240},
	[CHAN_ENUM_5540] = {5540, 108, 2, 240},
	[CHAN_ENUM_5560] = {5560, 112, 2, 240},
	[CHAN_ENUM_5580] = {5580, 116, 2, 240},
	[CHAN_ENUM_5600] = {5600, 120, 2, 240},
	[CHAN_ENUM_5620] = {5620, 124, 2, 240},
	[CHAN_ENUM_5640] = {5640, 128, 2, 240},
	[CHAN_ENUM_5660] = {5660, 132, 2, 240},
	[CHAN_ENUM_5680] = {5680, 136, 2, 240},
	[CHAN_ENUM_5700] = {5700, 140, 2, 240},
	[CHAN_ENUM_5720] = {5720, 144, 2, 240},
	[CHAN_ENUM_5745] = {5745, 149, 2, 160},
	[CHAN_ENUM_5765] = {5765, 153, 2, 160},
	[CHAN_ENUM_5785] = {5785, 157, 2, 160},
	[CHAN_ENUM_5805] = {5805, 161, 2, 160},
	[CHAN_ENUM_5825] = {5825, 165, 2, 160},
	[CHAN_ENUM_5845] = {5845, 169, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5850] = {5850, 170, 2, 160},
	[CHAN_ENUM_5855] = {5855, 171, 2, 160},
	[CHAN_ENUM_5860] = {5860, 172, 2, 160},
#endif
	[CHAN_ENUM_5865] = {5865, 173, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5870] = {5870, 174, 2, 160},
	[CHAN_ENUM_5875] = {5875, 175, 2, 160},
	[CHAN_ENUM_5880] = {5880, 176, 2, 160},
#endif
	[CHAN_ENUM_5885] = {5885, 177, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5890] = {5890, 178, 2, 160},
	[CHAN_ENUM_5895] = {5895, 179, 2, 160},
	[CHAN_ENUM_5900] = {5900, 180, 2, 160},
	[CHAN_ENUM_5905] = {5905, 181, 2, 160},
	[CHAN_ENUM_5910] = {5910, 182, 2, 160},
	[CHAN_ENUM_5915] = {5915, 183, 2, 160},
	[CHAN_ENUM_5920] = {5920, 184, 2, 160},
#endif /* WLAN_FEATURE_DSRC */
#ifdef CONFIG_BAND_6GHZ
	[CHAN_ENUM_5935] = {5935, 2, 2, 20},
	[CHAN_ENUM_5955] = {5955, 1, 2, 320},
	[CHAN_ENUM_5975] = {5975, 5, 2, 320},
	[CHAN_ENUM_5995] = {5995, 9, 2, 320},
	[CHAN_ENUM_6015] = {6015, 13, 2, 320},
	[CHAN_ENUM_6035] = {6035, 17, 2, 320},
	[CHAN_ENUM_6055] = {6055, 21, 2, 320},
	[CHAN_ENUM_6075] = {6075, 25, 2, 320},
	[CHAN_ENUM_6095] = {6095, 29, 2, 320},
	[CHAN_ENUM_6115] = {6115, 33, 2, 320},
	[CHAN_ENUM_6135] = {6135, 37, 2, 320},
	[CHAN_ENUM_6155] = {6155, 41, 2, 320},
	[CHAN_ENUM_6175] = {6175, 45, 2, 320},
	[CHAN_ENUM_6195] = {6195, 49, 2, 320},
	[CHAN_ENUM_6215] = {6215, 53, 2, 320},
	[CHAN_ENUM_6235] = {6235, 57, 2, 320},
	[CHAN_ENUM_6255] = {6255, 61, 2, 320},
	[CHAN_ENUM_6275] = {6275, 65, 2, 320},
	[CHAN_ENUM_6295] = {6295, 69, 2, 320},
	[CHAN_ENUM_6315] = {6315, 73, 2, 320},
	[CHAN_ENUM_6335] = {6335, 77, 2, 320},
	[CHAN_ENUM_6355] = {6355, 81, 2, 320},
	[CHAN_ENUM_6375] = {6375, 85, 2, 320},
	[CHAN_ENUM_6395] = {6395, 89, 2, 320},
	[CHAN_ENUM_6415] = {6415, 93, 2, 320},
	[CHAN_ENUM_6435] = {6435, 97, 2, 320},
	[CHAN_ENUM_6455] = {6455, 101, 2, 320},
	[CHAN_ENUM_6475] = {6475, 105, 2, 320},
	[CHAN_ENUM_6495] = {6495, 109, 2, 320},
	[CHAN_ENUM_6515] = {6515, 113, 2, 320},
	[CHAN_ENUM_6535] = {6535, 117, 2, 320},
	[CHAN_ENUM_6555] = {6555, 121, 2, 320},
	[CHAN_ENUM_6575] = {6575, 125, 2, 320},
	[CHAN_ENUM_6595] = {6595, 129, 2, 320},
	[CHAN_ENUM_6615] = {6615, 133, 2, 320},
	[CHAN_ENUM_6635] = {6635, 137, 2, 320},
	[CHAN_ENUM_6655] = {6655, 141, 2, 320},
	[CHAN_ENUM_6675] = {6675, 145, 2, 320},
	[CHAN_ENUM_6695] = {6695, 149, 2, 320},
	[CHAN_ENUM_6715] = {6715, 153, 2, 320},
	[CHAN_ENUM_6735] = {6735, 157, 2, 320},
	[CHAN_ENUM_6755] = {6755, 161, 2, 320},
	[CHAN_ENUM_6775] = {6775, 165, 2, 320},
	[CHAN_ENUM_6795] = {6795, 169, 2, 320},
	[CHAN_ENUM_6815] = {6815, 173, 2, 320},
	[CHAN_ENUM_6835] = {6835, 177, 2, 320},
	[CHAN_ENUM_6855] = {6855, 181, 2, 320},
	[CHAN_ENUM_6875] = {6875, 185, 2, 320},
	[CHAN_ENUM_6895] = {6895, 189, 2, 320},
	[CHAN_ENUM_6915] = {6915, 193, 2, 320},
	[CHAN_ENUM_6935] = {6935, 197, 2, 320},
	[CHAN_ENUM_6955] = {6955, 201, 2, 320},
	[CHAN_ENUM_6975] = {6975, 205, 2, 320},
	[CHAN_ENUM_6995] = {6995, 209, 2, 320},
	[CHAN_ENUM_7015] = {7015, 213, 2, 320},
	[CHAN_ENUM_7035] = {7035, 217, 2, 320},
	[CHAN_ENUM_7055] = {7055, 221, 2, 320},
	[CHAN_ENUM_7075] = {7075, 225, 2, 160},
	[CHAN_ENUM_7095] = {7095, 229, 2, 160},
	[CHAN_ENUM_7115] = {7115, 233, 2, 160}
#endif /* CONFIG_BAND_6GHZ */
};

const struct chan_map channel_map_eu[NUM_CHANNELS] = {
	[CHAN_ENUM_2412] = {2412, 1, 20, 40},
	[CHAN_ENUM_2417] = {2417, 2, 20, 40},
	[CHAN_ENUM_2422] = {2422, 3, 20, 40},
	[CHAN_ENUM_2427] = {2427, 4, 20, 40},
	[CHAN_ENUM_2432] = {2432, 5, 20, 40},
	[CHAN_ENUM_2437] = {2437, 6, 20, 40},
	[CHAN_ENUM_2442] = {2442, 7, 20, 40},
	[CHAN_ENUM_2447] = {2447, 8, 20, 40},
	[CHAN_ENUM_2452] = {2452, 9, 20, 40},
	[CHAN_ENUM_2457] = {2457, 10, 20, 40},
	[CHAN_ENUM_2462] = {2462, 11, 20, 40},
	[CHAN_ENUM_2467] = {2467, 12, 20, 40},
	[CHAN_ENUM_2472] = {2472, 13, 20, 40},
	[CHAN_ENUM_2484] = {2484, 14, 20, 20},
#ifdef CONFIG_49GHZ_CHAN
	[CHAN_ENUM_4912] = {4912, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4915] = {4915, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4917] = {4917, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4920] = {4920, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4922] = {4922, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4925] = {4925, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4927] = {4927, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4932] = {4932, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4935] = {4935, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4937] = {4937, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4940] = {4940, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4942] = {4942, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4945] = {4945, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4947] = {4947, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4950] = {4950, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4952] = {4952, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4955] = {4955, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4957] = {4957, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4960] = {4960, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4962] = {4962, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4965] = {4965, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4967] = {4967, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4970] = {4970, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4972] = {4972, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4975] = {4975, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4977] = {4977, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4980] = {4980, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4982] = {4982, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4985] = {4985, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4987] = {4987, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5032] = {5032, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5035] = {5035, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5037] = {5037, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5040] = {5040, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5042] = {5042, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5045] = {5045, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5047] = {5047, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5052] = {5052, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5055] = {5055, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5057] = {5057, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5060] = {5060, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5080] = {5080, INVALID_CHANNEL_NUM, 2, 20},
#endif /* CONFIG_49GHZ_CHAN */
	[CHAN_ENUM_5180] = {5180, 36, 2, 160},
	[CHAN_ENUM_5200] = {5200, 40, 2, 160},
	[CHAN_ENUM_5220] = {5220, 44, 2, 160},
	[CHAN_ENUM_5240] = {5240, 48, 2, 160},
	[CHAN_ENUM_5260] = {5260, 52, 2, 160},
	[CHAN_ENUM_5280] = {5280, 56, 2, 160},
	[CHAN_ENUM_5300] = {5300, 60, 2, 160},
	[CHAN_ENUM_5320] = {5320, 64, 2, 160},
	[CHAN_ENUM_5500] = {5500, 100, 2, 240},
	[CHAN_ENUM_5520] = {5520, 104, 2, 240},
	[CHAN_ENUM_5540] = {5540, 108, 2, 240},
	[CHAN_ENUM_5560] = {5560, 112, 2, 240},
	[CHAN_ENUM_5580] = {5580, 116, 2, 240},
	[CHAN_ENUM_5600] = {5600, 120, 2, 240},
	[CHAN_ENUM_5620] = {5620, 124, 2, 240},
	[CHAN_ENUM_5640] = {5640, 128, 2, 240},
	[CHAN_ENUM_5660] = {5660, 132, 2, 240},
	[CHAN_ENUM_5680] = {5680, 136, 2, 240},
	[CHAN_ENUM_5700] = {5700, 140, 2, 240},
	[CHAN_ENUM_5720] = {5720, 144, 2, 240},
	[CHAN_ENUM_5745] = {5745, 149, 2, 160},
	[CHAN_ENUM_5765] = {5765, 153, 2, 160},
	[CHAN_ENUM_5785] = {5785, 157, 2, 160},
	[CHAN_ENUM_5805] = {5805, 161, 2, 160},
	[CHAN_ENUM_5825] = {5825, 165, 2, 160},
	[CHAN_ENUM_5845] = {5845, 169, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5850] = {5850, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5855] = {5855, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5860] = {5860, INVALID_CHANNEL_NUM, 2, 160},
#endif
	[CHAN_ENUM_5865] = {5865, 173, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5870] = {5870, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5875] = {5875, 175, 2, 160},
	[CHAN_ENUM_5880] = {5880, 176, 2, 160},
#endif
	[CHAN_ENUM_5885] = {5885, 177, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5890] = {5890, 178, 2, 160},
	[CHAN_ENUM_5895] = {5895, 179, 2, 160},
	[CHAN_ENUM_5900] = {5900, 180, 2, 160},
	[CHAN_ENUM_5905] = {5905, 181, 2, 160},
	[CHAN_ENUM_5910] = {5910, 182, 2, 160},
	[CHAN_ENUM_5915] = {5915, 183, 2, 160},
	[CHAN_ENUM_5920] = {5920, 184, 2, 160},
#endif /* WLAN_FEATURE_DSRC */
#ifdef CONFIG_BAND_6GHZ
	[CHAN_ENUM_5935] = {5935, 2, 2, 20},
	[CHAN_ENUM_5955] = {5955, 1, 2, 320},
	[CHAN_ENUM_5975] = {5975, 5, 2, 320},
	[CHAN_ENUM_5995] = {5995, 9, 2, 320},
	[CHAN_ENUM_6015] = {6015, 13, 2, 320},
	[CHAN_ENUM_6035] = {6035, 17, 2, 320},
	[CHAN_ENUM_6055] = {6055, 21, 2, 320},
	[CHAN_ENUM_6075] = {6075, 25, 2, 320},
	[CHAN_ENUM_6095] = {6095, 29, 2, 320},
	[CHAN_ENUM_6115] = {6115, 33, 2, 320},
	[CHAN_ENUM_6135] = {6135, 37, 2, 320},
	[CHAN_ENUM_6155] = {6155, 41, 2, 320},
	[CHAN_ENUM_6175] = {6175, 45, 2, 320},
	[CHAN_ENUM_6195] = {6195, 49, 2, 320},
	[CHAN_ENUM_6215] = {6215, 53, 2, 320},
	[CHAN_ENUM_6235] = {6235, 57, 2, 320},
	[CHAN_ENUM_6255] = {6255, 61, 2, 320},
	[CHAN_ENUM_6275] = {6275, 65, 2, 320},
	[CHAN_ENUM_6295] = {6295, 69, 2, 320},
	[CHAN_ENUM_6315] = {6315, 73, 2, 320},
	[CHAN_ENUM_6335] = {6335, 77, 2, 320},
	[CHAN_ENUM_6355] = {6355, 81, 2, 320},
	[CHAN_ENUM_6375] = {6375, 85, 2, 320},
	[CHAN_ENUM_6395] = {6395, 89, 2, 320},
	[CHAN_ENUM_6415] = {6415, 93, 2, 320},
	[CHAN_ENUM_6435] = {6435, 97, 2, 320},
	[CHAN_ENUM_6455] = {6455, 101, 2, 320},
	[CHAN_ENUM_6475] = {6475, 105, 2, 320},
	[CHAN_ENUM_6495] = {6495, 109, 2, 320},
	[CHAN_ENUM_6515] = {6515, 113, 2, 320},
	[CHAN_ENUM_6535] = {6535, 117, 2, 320},
	[CHAN_ENUM_6555] = {6555, 121, 2, 320},
	[CHAN_ENUM_6575] = {6575, 125, 2, 320},
	[CHAN_ENUM_6595] = {6595, 129, 2, 320},
	[CHAN_ENUM_6615] = {6615, 133, 2, 320},
	[CHAN_ENUM_6635] = {6635, 137, 2, 320},
	[CHAN_ENUM_6655] = {6655, 141, 2, 320},
	[CHAN_ENUM_6675] = {6675, 145, 2, 320},
	[CHAN_ENUM_6695] = {6695, 149, 2, 320},
	[CHAN_ENUM_6715] = {6715, 153, 2, 320},
	[CHAN_ENUM_6735] = {6735, 157, 2, 320},
	[CHAN_ENUM_6755] = {6755, 161, 2, 320},
	[CHAN_ENUM_6775] = {6775, 165, 2, 320},
	[CHAN_ENUM_6795] = {6795, 169, 2, 320},
	[CHAN_ENUM_6815] = {6815, 173, 2, 320},
	[CHAN_ENUM_6835] = {6835, 177, 2, 320},
	[CHAN_ENUM_6855] = {6855, 181, 2, 320},
	[CHAN_ENUM_6875] = {6875, 185, 2, 320},
	[CHAN_ENUM_6895] = {6895, 189, 2, 320},
	[CHAN_ENUM_6915] = {6915, 193, 2, 320},
	[CHAN_ENUM_6935] = {6935, 197, 2, 320},
	[CHAN_ENUM_6955] = {6955, 201, 2, 320},
	[CHAN_ENUM_6975] = {6975, 205, 2, 320},
	[CHAN_ENUM_6995] = {6995, 209, 2, 320},
	[CHAN_ENUM_7015] = {7015, 213, 2, 320},
	[CHAN_ENUM_7035] = {7035, 217, 2, 320},
	[CHAN_ENUM_7055] = {7055, 221, 2, 320},
	[CHAN_ENUM_7075] = {7075, 225, 2, 160},
	[CHAN_ENUM_7095] = {7095, 229, 2, 160},
	[CHAN_ENUM_7115] = {7115, 233, 2, 160}
#endif /* CONFIG_BAND_6GHZ */
};

const struct chan_map channel_map_jp[NUM_CHANNELS] = {
	[CHAN_ENUM_2412] = {2412, 1, 20, 40},
	[CHAN_ENUM_2417] = {2417, 2, 20, 40},
	[CHAN_ENUM_2422] = {2422, 3, 20, 40},
	[CHAN_ENUM_2427] = {2427, 4, 20, 40},
	[CHAN_ENUM_2432] = {2432, 5, 20, 40},
	[CHAN_ENUM_2437] = {2437, 6, 20, 40},
	[CHAN_ENUM_2442] = {2442, 7, 20, 40},
	[CHAN_ENUM_2447] = {2447, 8, 20, 40},
	[CHAN_ENUM_2452] = {2452, 9, 20, 40},
	[CHAN_ENUM_2457] = {2457, 10, 20, 40},
	[CHAN_ENUM_2462] = {2462, 11, 20, 40},
	[CHAN_ENUM_2467] = {2467, 12, 20, 40},
	[CHAN_ENUM_2472] = {2472, 13, 20, 40},
	[CHAN_ENUM_2484] = {2484, 14, 20, 20},
#ifdef CONFIG_49GHZ_CHAN
	[CHAN_ENUM_4912] = {4912, 182, 5, 5},
	[CHAN_ENUM_4915] = {4915, 183, 10, 10},
	[CHAN_ENUM_4917] = {4917, 183, 5, 5},
	[CHAN_ENUM_4920] = {4920, 184, 10, 20},
	[CHAN_ENUM_4922] = {4922, 184, 5, 5},
	[CHAN_ENUM_4925] = {4925, 185, 10, 10},
	[CHAN_ENUM_4927] = {4927, 185, 5, 5},
	[CHAN_ENUM_4932] = {4932, 186, 5, 5},
	[CHAN_ENUM_4935] = {4935, 187, 10, 10},
	[CHAN_ENUM_4937] = {4937, 187, 5, 5},
	[CHAN_ENUM_4940] = {4940, 188, 10, 20},
	[CHAN_ENUM_4942] = {4942, 188, 5, 5},
	[CHAN_ENUM_4945] = {4945, 189, 10, 10},
	[CHAN_ENUM_4947] = {4947, 189, 5, 5},
	[CHAN_ENUM_4950] = {4950, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4952] = {4952, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4955] = {4955, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4957] = {4957, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4960] = {4960, 192, 20, 20},
	[CHAN_ENUM_4962] = {4962, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4965] = {4965, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4967] = {4967, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4970] = {4970, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4972] = {4972, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4975] = {4975, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4977] = {4977, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4980] = {4980, 196, 20, 20},
	[CHAN_ENUM_4982] = {4982, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4985] = {4985, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4987] = {4987, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5032] = {5032, 6, 5, 5},
	[CHAN_ENUM_5035] = {5035, 7, 10, 10},
	[CHAN_ENUM_5037] = {5037, 7, 5, 5},
	[CHAN_ENUM_5040] = {5040, 8, 10, 20},
	[CHAN_ENUM_5042] = {5042, 8, 5, 5},
	[CHAN_ENUM_5045] = {5045, 9, 10, 10},
	[CHAN_ENUM_5047] = {5047, 9, 5, 5},
	[CHAN_ENUM_5052] = {5052, 10, 5, 5},
	[CHAN_ENUM_5055] = {5055, 11, 10, 10},
	[CHAN_ENUM_5057] = {5057, 11, 5, 5},
	[CHAN_ENUM_5060] = {5060, 12, 20, 20},
	[CHAN_ENUM_5080] = {5080, 16, 20, 20},
#endif /* CONFIG_49GHZ_CHAN */
	[CHAN_ENUM_5180] = {5180, 36, 2, 160},
	[CHAN_ENUM_5200] = {5200, 40, 2, 160},
	[CHAN_ENUM_5220] = {5220, 44, 2, 160},
	[CHAN_ENUM_5240] = {5240, 48, 2, 160},
	[CHAN_ENUM_5260] = {5260, 52, 2, 160},
	[CHAN_ENUM_5280] = {5280, 56, 2, 160},
	[CHAN_ENUM_5300] = {5300, 60, 2, 160},
	[CHAN_ENUM_5320] = {5320, 64, 2, 160},
	[CHAN_ENUM_5500] = {5500, 100, 2, 240},
	[CHAN_ENUM_5520] = {5520, 104, 2, 240},
	[CHAN_ENUM_5540] = {5540, 108, 2, 240},
	[CHAN_ENUM_5560] = {5560, 112, 2, 240},
	[CHAN_ENUM_5580] = {5580, 116, 2, 240},
	[CHAN_ENUM_5600] = {5600, 120, 2, 240},
	[CHAN_ENUM_5620] = {5620, 124, 2, 240},
	[CHAN_ENUM_5640] = {5640, 128, 2, 240},
	[CHAN_ENUM_5660] = {5660, 132, 2, 240},
	[CHAN_ENUM_5680] = {5680, 136, 2, 240},
	[CHAN_ENUM_5700] = {5700, 140, 2, 240},
	[CHAN_ENUM_5720] = {5720, 144, 2, 240},
	[CHAN_ENUM_5745] = {5745, 149, 2, 160},
	[CHAN_ENUM_5765] = {5765, 153, 2, 160},
	[CHAN_ENUM_5785] = {5785, 157, 2, 160},
	[CHAN_ENUM_5805] = {5805, 161, 2, 160},
	[CHAN_ENUM_5825] = {5825, 165, 2, 160},
	[CHAN_ENUM_5845] = {5845, 169, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5850] = {5850, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5855] = {5855, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5860] = {5860, INVALID_CHANNEL_NUM, 2, 160},
#endif
	[CHAN_ENUM_5865] = {5865, INVALID_CHANNEL_NUM, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5870] = {5870, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5875] = {5875, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5880] = {5880, INVALID_CHANNEL_NUM, 2, 160},
#endif
	[CHAN_ENUM_5885] = {5885, INVALID_CHANNEL_NUM, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5890] = {5890, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5895] = {5895, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5900] = {5900, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5905] = {5905, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5910] = {5910, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5915] = {5915, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5920] = {5920, INVALID_CHANNEL_NUM, 2, 160},
#endif /* WLAN_FEATURE_DSRC */
#ifdef CONFIG_BAND_6GHZ
	[CHAN_ENUM_5935] = {5935, 2, 2, 20},
	[CHAN_ENUM_5955] = {5955, 1, 2, 320},
	[CHAN_ENUM_5975] = {5975, 5, 2, 320},
	[CHAN_ENUM_5995] = {5995, 9, 2, 320},
	[CHAN_ENUM_6015] = {6015, 13, 2, 320},
	[CHAN_ENUM_6035] = {6035, 17, 2, 320},
	[CHAN_ENUM_6055] = {6055, 21, 2, 320},
	[CHAN_ENUM_6075] = {6075, 25, 2, 320},
	[CHAN_ENUM_6095] = {6095, 29, 2, 320},
	[CHAN_ENUM_6115] = {6115, 33, 2, 320},
	[CHAN_ENUM_6135] = {6135, 37, 2, 320},
	[CHAN_ENUM_6155] = {6155, 41, 2, 320},
	[CHAN_ENUM_6175] = {6175, 45, 2, 320},
	[CHAN_ENUM_6195] = {6195, 49, 2, 320},
	[CHAN_ENUM_6215] = {6215, 53, 2, 320},
	[CHAN_ENUM_6235] = {6235, 57, 2, 320},
	[CHAN_ENUM_6255] = {6255, 61, 2, 320},
	[CHAN_ENUM_6275] = {6275, 65, 2, 320},
	[CHAN_ENUM_6295] = {6295, 69, 2, 320},
	[CHAN_ENUM_6315] = {6315, 73, 2, 320},
	[CHAN_ENUM_6335] = {6335, 77, 2, 320},
	[CHAN_ENUM_6355] = {6355, 81, 2, 320},
	[CHAN_ENUM_6375] = {6375, 85, 2, 320},
	[CHAN_ENUM_6395] = {6395, 89, 2, 320},
	[CHAN_ENUM_6415] = {6415, 93, 2, 320},
	[CHAN_ENUM_6435] = {6435, 97, 2, 320},
	[CHAN_ENUM_6455] = {6455, 101, 2, 320},
	[CHAN_ENUM_6475] = {6475, 105, 2, 320},
	[CHAN_ENUM_6495] = {6495, 109, 2, 320},
	[CHAN_ENUM_6515] = {6515, 113, 2, 320},
	[CHAN_ENUM_6535] = {6535, 117, 2, 320},
	[CHAN_ENUM_6555] = {6555, 121, 2, 320},
	[CHAN_ENUM_6575] = {6575, 125, 2, 320},
	[CHAN_ENUM_6595] = {6595, 129, 2, 320},
	[CHAN_ENUM_6615] = {6615, 133, 2, 320},
	[CHAN_ENUM_6635] = {6635, 137, 2, 320},
	[CHAN_ENUM_6655] = {6655, 141, 2, 320},
	[CHAN_ENUM_6675] = {6675, 145, 2, 320},
	[CHAN_ENUM_6695] = {6695, 149, 2, 320},
	[CHAN_ENUM_6715] = {6715, 153, 2, 320},
	[CHAN_ENUM_6735] = {6735, 157, 2, 320},
	[CHAN_ENUM_6755] = {6755, 161, 2, 320},
	[CHAN_ENUM_6775] = {6775, 165, 2, 320},
	[CHAN_ENUM_6795] = {6795, 169, 2, 320},
	[CHAN_ENUM_6815] = {6815, 173, 2, 320},
	[CHAN_ENUM_6835] = {6835, 177, 2, 320},
	[CHAN_ENUM_6855] = {6855, 181, 2, 320},
	[CHAN_ENUM_6875] = {6875, 185, 2, 320},
	[CHAN_ENUM_6895] = {6895, 189, 2, 320},
	[CHAN_ENUM_6915] = {6915, 193, 2, 320},
	[CHAN_ENUM_6935] = {6935, 197, 2, 320},
	[CHAN_ENUM_6955] = {6955, 201, 2, 320},
	[CHAN_ENUM_6975] = {6975, 205, 2, 320},
	[CHAN_ENUM_6995] = {6995, 209, 2, 320},
	[CHAN_ENUM_7015] = {7015, 213, 2, 320},
	[CHAN_ENUM_7035] = {7035, 217, 2, 320},
	[CHAN_ENUM_7055] = {7055, 221, 2, 320},
	[CHAN_ENUM_7075] = {7075, 225, 2, 160},
	[CHAN_ENUM_7095] = {7095, 229, 2, 160},
	[CHAN_ENUM_7115] = {7115, 233, 2, 160}
#endif /* CONFIG_BAND_6GHZ */
};

const struct chan_map channel_map_global[NUM_CHANNELS] = {
	[CHAN_ENUM_2412] = {2412, 1, 20, 40},
	[CHAN_ENUM_2417] = {2417, 2, 20, 40},
	[CHAN_ENUM_2422] = {2422, 3, 20, 40},
	[CHAN_ENUM_2427] = {2427, 4, 20, 40},
	[CHAN_ENUM_2432] = {2432, 5, 20, 40},
	[CHAN_ENUM_2437] = {2437, 6, 20, 40},
	[CHAN_ENUM_2442] = {2442, 7, 20, 40},
	[CHAN_ENUM_2447] = {2447, 8, 20, 40},
	[CHAN_ENUM_2452] = {2452, 9, 20, 40},
	[CHAN_ENUM_2457] = {2457, 10, 20, 40},
	[CHAN_ENUM_2462] = {2462, 11, 20, 40},
	[CHAN_ENUM_2467] = {2467, 12, 20, 40},
	[CHAN_ENUM_2472] = {2472, 13, 20, 40},
	[CHAN_ENUM_2484] = {2484, 14, 20, 20},
#ifdef CONFIG_49GHZ_CHAN
	[CHAN_ENUM_4912] = {4912, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4915] = {4915, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4917] = {4917, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4920] = {4920, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4922] = {4922, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4925] = {4925, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4927] = {4927, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4932] = {4932, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4935] = {4935, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4937] = {4937, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4940] = {4940, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4942] = {4942, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4945] = {4945, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4947] = {4947, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4950] = {4950, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4952] = {4952, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4955] = {4955, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4957] = {4957, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4960] = {4960, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4962] = {4962, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4965] = {4965, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4967] = {4967, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4970] = {4970, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4972] = {4972, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4975] = {4975, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4977] = {4977, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4980] = {4980, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4982] = {4982, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4985] = {4985, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4987] = {4987, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5032] = {5032, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5035] = {5035, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5037] = {5037, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5040] = {5040, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5042] = {5042, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5045] = {5045, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5047] = {5047, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5052] = {5052, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5055] = {5055, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5057] = {5057, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5060] = {5060, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5080] = {5080, INVALID_CHANNEL_NUM, 2, 20},
#endif /* CONFIG_49GHZ_CHAN */
	[CHAN_ENUM_5180] = {5180, 36, 2, 160},
	[CHAN_ENUM_5200] = {5200, 40, 2, 160},
	[CHAN_ENUM_5220] = {5220, 44, 2, 160},
	[CHAN_ENUM_5240] = {5240, 48, 2, 160},
	[CHAN_ENUM_5260] = {5260, 52, 2, 160},
	[CHAN_ENUM_5280] = {5280, 56, 2, 160},
	[CHAN_ENUM_5300] = {5300, 60, 2, 160},
	[CHAN_ENUM_5320] = {5320, 64, 2, 160},
	[CHAN_ENUM_5500] = {5500, 100, 2, 240},
	[CHAN_ENUM_5520] = {5520, 104, 2, 240},
	[CHAN_ENUM_5540] = {5540, 108, 2, 240},
	[CHAN_ENUM_5560] = {5560, 112, 2, 240},
	[CHAN_ENUM_5580] = {5580, 116, 2, 240},
	[CHAN_ENUM_5600] = {5600, 120, 2, 240},
	[CHAN_ENUM_5620] = {5620, 124, 2, 240},
	[CHAN_ENUM_5640] = {5640, 128, 2, 240},
	[CHAN_ENUM_5660] = {5660, 132, 2, 240},
	[CHAN_ENUM_5680] = {5680, 136, 2, 240},
	[CHAN_ENUM_5700] = {5700, 140, 2, 240},
	[CHAN_ENUM_5720] = {5720, 144, 2, 240},
	[CHAN_ENUM_5745] = {5745, 149, 2, 160},
	[CHAN_ENUM_5765] = {5765, 153, 2, 160},
	[CHAN_ENUM_5785] = {5785, 157, 2, 160},
	[CHAN_ENUM_5805] = {5805, 161, 2, 160},
	[CHAN_ENUM_5825] = {5825, 165, 2, 160},
	[CHAN_ENUM_5845] = {5845, 169, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5850] = {5850, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5855] = {5855, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5860] = {5860, INVALID_CHANNEL_NUM, 2, 160},
#endif
	[CHAN_ENUM_5865] = {5865, 173, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5870] = {5870, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5875] = {5875, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5880] = {5880, INVALID_CHANNEL_NUM, 2, 160},
#endif
	[CHAN_ENUM_5885] = {5885, INVALID_CHANNEL_NUM, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5890] = {5890, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5895] = {5895, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5900] = {5900, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5905] = {5905, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5910] = {5910, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5915] = {5915, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5920] = {5920, INVALID_CHANNEL_NUM, 2, 160},
#endif /* WLAN_FEATURE_DSRC */
#ifdef CONFIG_BAND_6GHZ
	[CHAN_ENUM_5935] = {5935, 2, 2, 20},
	[CHAN_ENUM_5955] = {5955, 1, 2, 320},
	[CHAN_ENUM_5975] = {5975, 5, 2, 320},
	[CHAN_ENUM_5995] = {5995, 9, 2, 320},
	[CHAN_ENUM_6015] = {6015, 13, 2, 320},
	[CHAN_ENUM_6035] = {6035, 17, 2, 320},
	[CHAN_ENUM_6055] = {6055, 21, 2, 320},
	[CHAN_ENUM_6075] = {6075, 25, 2, 320},
	[CHAN_ENUM_6095] = {6095, 29, 2, 320},
	[CHAN_ENUM_6115] = {6115, 33, 2, 320},
	[CHAN_ENUM_6135] = {6135, 37, 2, 320},
	[CHAN_ENUM_6155] = {6155, 41, 2, 320},
	[CHAN_ENUM_6175] = {6175, 45, 2, 320},
	[CHAN_ENUM_6195] = {6195, 49, 2, 320},
	[CHAN_ENUM_6215] = {6215, 53, 2, 320},
	[CHAN_ENUM_6235] = {6235, 57, 2, 320},
	[CHAN_ENUM_6255] = {6255, 61, 2, 320},
	[CHAN_ENUM_6275] = {6275, 65, 2, 320},
	[CHAN_ENUM_6295] = {6295, 69, 2, 320},
	[CHAN_ENUM_6315] = {6315, 73, 2, 320},
	[CHAN_ENUM_6335] = {6335, 77, 2, 320},
	[CHAN_ENUM_6355] = {6355, 81, 2, 320},
	[CHAN_ENUM_6375] = {6375, 85, 2, 320},
	[CHAN_ENUM_6395] = {6395, 89, 2, 320},
	[CHAN_ENUM_6415] = {6415, 93, 2, 320},
	[CHAN_ENUM_6435] = {6435, 97, 2, 320},
	[CHAN_ENUM_6455] = {6455, 101, 2, 320},
	[CHAN_ENUM_6475] = {6475, 105, 2, 320},
	[CHAN_ENUM_6495] = {6495, 109, 2, 320},
	[CHAN_ENUM_6515] = {6515, 113, 2, 320},
	[CHAN_ENUM_6535] = {6535, 117, 2, 320},
	[CHAN_ENUM_6555] = {6555, 121, 2, 320},
	[CHAN_ENUM_6575] = {6575, 125, 2, 320},
	[CHAN_ENUM_6595] = {6595, 129, 2, 320},
	[CHAN_ENUM_6615] = {6615, 133, 2, 320},
	[CHAN_ENUM_6635] = {6635, 137, 2, 320},
	[CHAN_ENUM_6655] = {6655, 141, 2, 320},
	[CHAN_ENUM_6675] = {6675, 145, 2, 320},
	[CHAN_ENUM_6695] = {6695, 149, 2, 320},
	[CHAN_ENUM_6715] = {6715, 153, 2, 320},
	[CHAN_ENUM_6735] = {6735, 157, 2, 320},
	[CHAN_ENUM_6755] = {6755, 161, 2, 320},
	[CHAN_ENUM_6775] = {6775, 165, 2, 320},
	[CHAN_ENUM_6795] = {6795, 169, 2, 320},
	[CHAN_ENUM_6815] = {6815, 173, 2, 320},
	[CHAN_ENUM_6835] = {6835, 177, 2, 320},
	[CHAN_ENUM_6855] = {6855, 181, 2, 320},
	[CHAN_ENUM_6875] = {6875, 185, 2, 320},
	[CHAN_ENUM_6895] = {6895, 189, 2, 320},
	[CHAN_ENUM_6915] = {6915, 193, 2, 320},
	[CHAN_ENUM_6935] = {6935, 197, 2, 320},
	[CHAN_ENUM_6955] = {6955, 201, 2, 320},
	[CHAN_ENUM_6975] = {6975, 205, 2, 320},
	[CHAN_ENUM_6995] = {6995, 209, 2, 320},
	[CHAN_ENUM_7015] = {7015, 213, 2, 320},
	[CHAN_ENUM_7035] = {7035, 217, 2, 320},
	[CHAN_ENUM_7055] = {7055, 221, 2, 320},
	[CHAN_ENUM_7075] = {7075, 225, 2, 160},
	[CHAN_ENUM_7095] = {7095, 229, 2, 160},
	[CHAN_ENUM_7115] = {7115, 233, 2, 160}
#endif /* CONFIG_BAND_6GHZ */
};

const struct chan_map channel_map_china[NUM_CHANNELS] = {
	[CHAN_ENUM_2412] = {2412, 1, 20, 40},
	[CHAN_ENUM_2417] = {2417, 2, 20, 40},
	[CHAN_ENUM_2422] = {2422, 3, 20, 40},
	[CHAN_ENUM_2427] = {2427, 4, 20, 40},
	[CHAN_ENUM_2432] = {2432, 5, 20, 40},
	[CHAN_ENUM_2437] = {2437, 6, 20, 40},
	[CHAN_ENUM_2442] = {2442, 7, 20, 40},
	[CHAN_ENUM_2447] = {2447, 8, 20, 40},
	[CHAN_ENUM_2452] = {2452, 9, 20, 40},
	[CHAN_ENUM_2457] = {2457, 10, 20, 40},
	[CHAN_ENUM_2462] = {2462, 11, 20, 40},
	[CHAN_ENUM_2467] = {2467, 12, 20, 40},
	[CHAN_ENUM_2472] = {2472, 13, 20, 40},
	[CHAN_ENUM_2484] = {2484, 14, 20, 20},
#ifdef CONFIG_49GHZ_CHAN
	[CHAN_ENUM_4912] = {4912, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4915] = {4915, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4917] = {4917, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4920] = {4920, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4922] = {4922, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4925] = {4925, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4927] = {4927, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4932] = {4932, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4935] = {4935, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4937] = {4937, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4940] = {4940, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4942] = {4942, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4945] = {4945, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4947] = {4947, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4950] = {4950, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4952] = {4952, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4955] = {4955, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4957] = {4957, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4960] = {4960, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4962] = {4962, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4965] = {4965, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4967] = {4967, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4970] = {4970, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4972] = {4972, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4975] = {4975, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4977] = {4977, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4980] = {4980, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4982] = {4982, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4985] = {4985, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_4987] = {4987, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5032] = {5032, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5035] = {5035, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5037] = {5037, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5040] = {5040, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5042] = {5042, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5045] = {5045, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5047] = {5047, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5052] = {5052, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5055] = {5055, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5057] = {5057, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5060] = {5060, INVALID_CHANNEL_NUM, 2, 20},
	[CHAN_ENUM_5080] = {5080, INVALID_CHANNEL_NUM, 2, 20},
#endif /* CONFIG_49GHZ_CHAN */
	[CHAN_ENUM_5180] = {5180, 36, 2, 160},
	[CHAN_ENUM_5200] = {5200, 40, 2, 160},
	[CHAN_ENUM_5220] = {5220, 44, 2, 160},
	[CHAN_ENUM_5240] = {5240, 48, 2, 160},
	[CHAN_ENUM_5260] = {5260, 52, 2, 160},
	[CHAN_ENUM_5280] = {5280, 56, 2, 160},
	[CHAN_ENUM_5300] = {5300, 60, 2, 160},
	[CHAN_ENUM_5320] = {5320, 64, 2, 160},
	[CHAN_ENUM_5500] = {5500, 100, 2, 240},
	[CHAN_ENUM_5520] = {5520, 104, 2, 240},
	[CHAN_ENUM_5540] = {5540, 108, 2, 240},
	[CHAN_ENUM_5560] = {5560, 112, 2, 240},
	[CHAN_ENUM_5580] = {5580, 116, 2, 240},
	[CHAN_ENUM_5600] = {5600, 120, 2, 240},
	[CHAN_ENUM_5620] = {5620, 124, 2, 240},
	[CHAN_ENUM_5640] = {5640, 128, 2, 240},
	[CHAN_ENUM_5660] = {5660, 132, 2, 240},
	[CHAN_ENUM_5680] = {5680, 136, 2, 240},
	[CHAN_ENUM_5700] = {5700, 140, 2, 240},
	[CHAN_ENUM_5720] = {5720, 144, 2, 240},
	[CHAN_ENUM_5745] = {5745, 149, 2, 160},
	[CHAN_ENUM_5765] = {5765, 153, 2, 160},
	[CHAN_ENUM_5785] = {5785, 157, 2, 160},
	[CHAN_ENUM_5805] = {5805, 161, 2, 160},
	[CHAN_ENUM_5825] = {5825, 165, 2, 160},
	[CHAN_ENUM_5845] = {5845, 169, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5850] = {5850, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5855] = {5855, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5860] = {5860, INVALID_CHANNEL_NUM, 2, 160},
#endif
	[CHAN_ENUM_5865] = {5865, INVALID_CHANNEL_NUM, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5870] = {5870, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5875] = {5875, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5880] = {5880, INVALID_CHANNEL_NUM, 2, 160},
#endif
	[CHAN_ENUM_5885] = {5885, INVALID_CHANNEL_NUM, 2, 160},
#ifdef WLAN_FEATURE_DSRC
	[CHAN_ENUM_5890] = {5890, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5895] = {5895, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5900] = {5900, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5905] = {5905, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5910] = {5910, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5915] = {5915, INVALID_CHANNEL_NUM, 2, 160},
	[CHAN_ENUM_5920] = {5920, INVALID_CHANNEL_NUM, 2, 160},
#endif /* WLAN_FEATURE_DSRC */
#ifdef CONFIG_BAND_6GHZ
	[CHAN_ENUM_5935] = {5935, 2, 2, 20},
	[CHAN_ENUM_5955] = {5955, 1, 2, 320},
	[CHAN_ENUM_5975] = {5975, 5, 2, 320},
	[CHAN_ENUM_5995] = {5995, 9, 2, 320},
	[CHAN_ENUM_6015] = {6015, 13, 2, 320},
	[CHAN_ENUM_6035] = {6035, 17, 2, 320},
	[CHAN_ENUM_6055] = {6055, 21, 2, 320},
	[CHAN_ENUM_6075] = {6075, 25, 2, 320},
	[CHAN_ENUM_6095] = {6095, 29, 2, 320},
	[CHAN_ENUM_6115] = {6115, 33, 2, 320},
	[CHAN_ENUM_6135] = {6135, 37, 2, 320},
	[CHAN_ENUM_6155] = {6155, 41, 2, 320},
	[CHAN_ENUM_6175] = {6175, 45, 2, 320},
	[CHAN_ENUM_6195] = {6195, 49, 2, 320},
	[CHAN_ENUM_6215] = {6215, 53, 2, 320},
	[CHAN_ENUM_6235] = {6235, 57, 2, 320},
	[CHAN_ENUM_6255] = {6255, 61, 2, 320},
	[CHAN_ENUM_6275] = {6275, 65, 2, 320},
	[CHAN_ENUM_6295] = {6295, 69, 2, 320},
	[CHAN_ENUM_6315] = {6315, 73, 2, 320},
	[CHAN_ENUM_6335] = {6335, 77, 2, 320},
	[CHAN_ENUM_6355] = {6355, 81, 2, 320},
	[CHAN_ENUM_6375] = {6375, 85, 2, 320},
	[CHAN_ENUM_6395] = {6395, 89, 2, 320},
	[CHAN_ENUM_6415] = {6415, 93, 2, 320},
	[CHAN_ENUM_6435] = {6435, 97, 2, 320},
	[CHAN_ENUM_6455] = {6455, 101, 2, 320},
	[CHAN_ENUM_6475] = {6475, 105, 2, 320},
	[CHAN_ENUM_6495] = {6495, 109, 2, 320},
	[CHAN_ENUM_6515] = {6515, 113, 2, 320},
	[CHAN_ENUM_6535] = {6535, 117, 2, 320},
	[CHAN_ENUM_6555] = {6555, 121, 2, 320},
	[CHAN_ENUM_6575] = {6575, 125, 2, 320},
	[CHAN_ENUM_6595] = {6595, 129, 2, 320},
	[CHAN_ENUM_6615] = {6615, 133, 2, 320},
	[CHAN_ENUM_6635] = {6635, 137, 2, 320},
	[CHAN_ENUM_6655] = {6655, 141, 2, 320},
	[CHAN_ENUM_6675] = {6675, 145, 2, 320},
	[CHAN_ENUM_6695] = {6695, 149, 2, 320},
	[CHAN_ENUM_6715] = {6715, 153, 2, 320},
	[CHAN_ENUM_6735] = {6735, 157, 2, 320},
	[CHAN_ENUM_6755] = {6755, 161, 2, 320},
	[CHAN_ENUM_6775] = {6775, 165, 2, 320},
	[CHAN_ENUM_6795] = {6795, 169, 2, 320},
	[CHAN_ENUM_6815] = {6815, 173, 2, 320},
	[CHAN_ENUM_6835] = {6835, 177, 2, 320},
	[CHAN_ENUM_6855] = {6855, 181, 2, 320},
	[CHAN_ENUM_6875] = {6875, 185, 2, 320},
	[CHAN_ENUM_6895] = {6895, 189, 2, 320},
	[CHAN_ENUM_6915] = {6915, 193, 2, 320},
	[CHAN_ENUM_6935] = {6935, 197, 2, 320},
	[CHAN_ENUM_6955] = {6955, 201, 2, 320},
	[CHAN_ENUM_6975] = {6975, 205, 2, 320},
	[CHAN_ENUM_6995] = {6995, 209, 2, 320},
	[CHAN_ENUM_7015] = {7015, 213, 2, 320},
	[CHAN_ENUM_7035] = {7035, 217, 2, 320},
	[CHAN_ENUM_7055] = {7055, 221, 2, 320},
	[CHAN_ENUM_7075] = {7075, 225, 2, 160},
	[CHAN_ENUM_7095] = {7095, 229, 2, 160},
	[CHAN_ENUM_7115] = {7115, 233, 2, 160}
#endif /* CONFIG_BAND_6GHZ */
};

static uint8_t reg_calculate_max_5gh_enum(void)
{
	int16_t idx;
	uint8_t max_valid_ieee_chan = INVALID_CHANNEL_NUM;

	for (idx = MAX_5GHZ_CHANNEL; idx >= 0; idx--) {
		if (channel_map[idx].chan_num != INVALID_CHANNEL_NUM) {
			max_valid_ieee_chan = channel_map[idx].chan_num;
			break;
		}
	}

	return max_valid_ieee_chan;
}

void reg_init_channel_map(enum dfs_reg dfs_region)
{
	switch (dfs_region) {
	case DFS_UNINIT_REGION:
	case DFS_UNDEF_REGION:
		channel_map = channel_map_global;
		break;
	case DFS_FCC_REGION:
		channel_map = channel_map_us;
		break;
	case DFS_ETSI_REGION:
		channel_map = channel_map_eu;
		break;
	case DFS_MKK_REGION:
	case DFS_MKKN_REGION:
		channel_map = channel_map_jp;
		break;
	case DFS_CN_REGION:
		channel_map = channel_map_china;
		break;
	case DFS_KR_REGION:
		channel_map = channel_map_global;
		break;
	}

	g_reg_max_5g_chan_num = reg_calculate_max_5gh_enum();
}

#ifdef WLAN_FEATURE_11BE
uint16_t reg_get_bw_value(enum phy_ch_width bw)
{
	switch (bw) {
	case CH_WIDTH_20MHZ:
		return 20;
	case CH_WIDTH_40MHZ:
		return 40;
	case CH_WIDTH_80MHZ:
		return 80;
	case CH_WIDTH_160MHZ:
		return 160;
	case CH_WIDTH_80P80MHZ:
		return 160;
	case CH_WIDTH_INVALID:
		return 0;
	case CH_WIDTH_5MHZ:
		return 5;
	case CH_WIDTH_10MHZ:
		return 10;
	case CH_WIDTH_320MHZ:
	case CH_WIDTH_MAX:
		return 320;
	default:
		return 0;
	}
}
#else
uint16_t reg_get_bw_value(enum phy_ch_width bw)
{
	switch (bw) {
	case CH_WIDTH_20MHZ:
		return 20;
	case CH_WIDTH_40MHZ:
		return 40;
	case CH_WIDTH_80MHZ:
		return 80;
	case CH_WIDTH_160MHZ:
		return 160;
	case CH_WIDTH_80P80MHZ:
		return 160;
	case CH_WIDTH_INVALID:
		return 0;
	case CH_WIDTH_5MHZ:
		return 5;
	case CH_WIDTH_10MHZ:
		return 10;
	case CH_WIDTH_MAX:
		return 160;
	default:
		return 0;
	}
}
#endif

struct wlan_lmac_if_reg_tx_ops *reg_get_psoc_tx_ops(
		struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		reg_err("tx_ops is NULL");
		return NULL;
	}

	return &tx_ops->reg_ops;
}

/**
 * reg_combine_channel_states() - Get minimum of channel state1 and state2
 * @chan_state1: Channel state1
 * @chan_state2: Channel state2
 *
 * Return: Channel state
 */
enum channel_state reg_combine_channel_states(enum channel_state chan_state1,
					      enum channel_state chan_state2)
{
	if ((chan_state1 == CHANNEL_STATE_INVALID) ||
	    (chan_state2 == CHANNEL_STATE_INVALID))
		return CHANNEL_STATE_INVALID;
	else
		return min(chan_state1, chan_state2);
}

QDF_STATUS reg_read_default_country(struct wlan_objmgr_psoc *psoc,
				    uint8_t *country_code)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	if (!country_code) {
		reg_err("country_code is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_copy(country_code, psoc_priv_obj->def_country,
		     REG_ALPHA2_LEN + 1);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_REG_PARTIAL_OFFLOAD
QDF_STATUS reg_get_max_5g_bw_from_country_code(struct wlan_objmgr_pdev *pdev,
					       uint16_t cc,
					       uint16_t *max_bw_5g)
{
	uint16_t i;
	int num_countries;

	*max_bw_5g = 0;
	reg_get_num_countries(&num_countries);

	for (i = 0; i < num_countries; i++) {
		if (g_all_countries[i].country_code == cc)
			break;
	}

	if (i == num_countries)
		return QDF_STATUS_E_FAILURE;

	*max_bw_5g = g_all_countries[i].max_bw_5g;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_get_max_5g_bw_from_regdomain(struct wlan_objmgr_pdev *pdev,
					    uint16_t regdmn,
					    uint16_t *max_bw_5g)
{
	uint16_t i;
	int num_reg_dmn;

	*max_bw_5g = 0;
	reg_get_num_reg_dmn_pairs(&num_reg_dmn);

	for (i = 0; i < num_reg_dmn; i++) {
		if (g_reg_dmn_pairs[i].reg_dmn_pair_id == regdmn)
			break;
	}

	if (i == num_reg_dmn)
		return QDF_STATUS_E_FAILURE;

	*max_bw_5g = regdomains_5g[g_reg_dmn_pairs[i].dmn_id_5g].max_bw;

	return QDF_STATUS_SUCCESS;
}
#else

QDF_STATUS reg_get_max_5g_bw_from_country_code(struct wlan_objmgr_pdev *pdev,
					       uint16_t cc,
					       uint16_t *max_bw_5g)
{
	*max_bw_5g = reg_get_max_bw_5G_for_fo(pdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_get_max_5g_bw_from_regdomain(struct wlan_objmgr_pdev *pdev,
					    uint16_t regdmn,
					    uint16_t *max_bw_5g)
{
	*max_bw_5g = reg_get_max_bw_5G_for_fo(pdev);

	return QDF_STATUS_SUCCESS;
}
#endif

uint16_t reg_get_max_bw_5G_for_fo(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	struct wlan_regulatory_psoc_priv_obj *soc_reg;
	uint8_t pdev_id;
	uint8_t phy_id;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;

	soc_reg = reg_get_psoc_obj(psoc);
	if (!soc_reg) {
		reg_err("soc_reg is NULL");
		return 0;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	reg_tx_ops = reg_get_psoc_tx_ops(psoc);
	if (reg_tx_ops->get_phy_id_from_pdev_id)
		reg_tx_ops->get_phy_id_from_pdev_id(psoc, pdev_id, &phy_id);
	else
		phy_id = pdev_id;

	return soc_reg->mas_chan_params[phy_id].max_bw_5g;
}

void reg_get_current_dfs_region(struct wlan_objmgr_pdev *pdev,
				enum dfs_reg *dfs_reg)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg component pdev priv is NULL");
		return;
	}

	*dfs_reg = pdev_priv_obj->dfs_region;
}

void reg_set_dfs_region(struct wlan_objmgr_pdev *pdev,
			enum dfs_reg dfs_reg)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return;
	}

	pdev_priv_obj->dfs_region = dfs_reg;

	reg_init_channel_map(dfs_reg);
}

static uint8_t reg_freq_to_chan_direct(qdf_freq_t freq)
{
	if (freq >= TWOG_CHAN_1_IN_MHZ && freq <= TWOG_CHAN_13_IN_MHZ)
		return IEEE_2GHZ_CH1 +
			(freq - TWOG_CHAN_1_IN_MHZ) / IEEE_CH_SEP;

	if (freq == TWOG_CHAN_14_IN_MHZ)
		return IEEE_2GHZ_CH14;

	if (freq >= FIVEG_CHAN_36_IN_MHZ && freq <= FIVEG_CHAN_177_IN_MHZ)
		return IEEE_5GHZ_CH36 +
			(freq - FIVEG_CHAN_36_IN_MHZ) / IEEE_CH_SEP;

	if (freq == SIXG_CHAN_2_IN_MHZ)
		return IEEE_6GHZ_CH2;

	if (freq >= SIXG_CHAN_1_IN_MHZ && freq <= SIXG_CHAN_233_IN_MHZ)
		return IEEE_6GHZ_CH1 +
			(freq - SIXG_CHAN_1_IN_MHZ) / IEEE_CH_SEP;

	return 0;
}

static uint8_t
reg_freq_to_chan_for_chlist(struct regulatory_channel *chan_list,
			    qdf_freq_t freq,
			    enum channel_enum num_chans)
{
	uint32_t count;
	uint8_t chan_ieee;

	if (num_chans > NUM_CHANNELS) {
		reg_err_rl("invalid num_chans");
		return 0;
	}

	chan_ieee = reg_freq_to_chan_direct(freq);
	if (chan_ieee)
		return chan_ieee;

	for (count = 0; count < num_chans; count++) {
		if (chan_list[count].center_freq >= freq)
			break;
	}

	if (count == num_chans)
		goto end;

	if (chan_list[count].center_freq == freq)
		return chan_list[count].chan_num;

	if (count == 0)
		goto end;

	if ((chan_list[count - 1].chan_num == INVALID_CHANNEL_NUM) ||
	    (chan_list[count].chan_num == INVALID_CHANNEL_NUM)) {
		reg_err("Frequency %d invalid in current reg domain", freq);
		return 0;
	}

	return (chan_list[count - 1].chan_num +
		(freq - chan_list[count - 1].center_freq) / 5);

end:
	reg_err_rl("invalid frequency %d", freq);
	return 0;
}

uint8_t reg_freq_to_chan(struct wlan_objmgr_pdev *pdev,
			 qdf_freq_t freq)
{
	struct regulatory_channel *chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint8_t chan;
	enum supported_6g_pwr_types input_6g_pwr_mode = REG_AP_LPI;

	if (freq == 0) {
		reg_debug_rl("Invalid freq %d", freq);
		return 0;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return 0;
	}

	chan_list = pdev_priv_obj->mas_chan_list;
	chan = reg_freq_to_chan_for_chlist(chan_list, freq, NUM_CHANNELS);

	if (chan)
		return chan;

	if (!REG_IS_6GHZ_FREQ(freq))
		return chan;

	input_6g_pwr_mode = REG_AP_LPI;

	while (input_6g_pwr_mode < REG_INVALID_PWR_MODE) {
		chan_list = reg_get_reg_maschan_lst_frm_6g_pwr_mode(
							input_6g_pwr_mode,
							pdev_priv_obj, 0);
		if (!chan_list)
			return 0;

		chan = reg_freq_to_chan_for_chlist(chan_list, freq,
						   NUM_6GHZ_CHANNELS);
		if (chan)
			break;
		input_6g_pwr_mode++;
	}

	return chan;
}

static uint16_t
reg_compute_chan_to_freq_for_chlist(struct regulatory_channel *chan_list,
				    uint8_t chan_num,
				    enum channel_enum min_chan_range,
				    enum channel_enum max_chan_range)
{
	uint16_t count;

	if (reg_is_chan_enum_invalid(min_chan_range) ||
	    reg_is_chan_enum_invalid(max_chan_range)) {
		reg_debug_rl("Invalid channel range: min_chan_range: 0x%X max_chan_range: 0x%X",
			     min_chan_range,
			     max_chan_range);
		return 0;
	}

	for (count = min_chan_range; count <= max_chan_range; count++) {
		if ((chan_list[count].state != CHANNEL_STATE_DISABLE) &&
		    !(chan_list[count].chan_flags & REGULATORY_CHAN_DISABLED)) {
			if (REG_IS_49GHZ_FREQ(chan_list[count].center_freq)) {
				if (chan_list[count].chan_num == chan_num)
					break;
				continue;
			} else if ((chan_list[count].chan_num >= chan_num) &&
				   (chan_list[count].chan_num !=
							INVALID_CHANNEL_NUM))
				break;
		}
	}

	if (count == max_chan_range + 1)
		goto end;

	if (chan_list[count].chan_num == chan_num) {
		if (chan_list[count].chan_flags & REGULATORY_CHAN_DISABLED)
			reg_err("Channel %d disabled in current reg domain",
				chan_num);
		return chan_list[count].center_freq;
	}

	if (count == min_chan_range)
		goto end;

	if ((chan_list[count - 1].chan_num == INVALID_CHANNEL_NUM) ||
	    REG_IS_49GHZ_FREQ(chan_list[count - 1].center_freq) ||
	    (chan_list[count].chan_num == INVALID_CHANNEL_NUM)) {
		reg_err("Channel %d invalid in current reg domain",
			chan_num);
		return 0;
	}

	return (chan_list[count - 1].center_freq +
		(chan_num - chan_list[count - 1].chan_num) * 5);

end:

	reg_debug_rl("Invalid channel %d", chan_num);
	return 0;
}

static uint16_t reg_compute_chan_to_freq(struct wlan_objmgr_pdev *pdev,
					 uint8_t chan_num,
					 enum channel_enum min_chan_range,
					 enum channel_enum max_chan_range)
{
	struct regulatory_channel *chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint16_t freq;
	enum supported_6g_pwr_types input_6g_pwr_mode;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return 0;
	}

	if (min_chan_range < MIN_CHANNEL || max_chan_range > MAX_CHANNEL) {
		reg_err_rl("Channel range is invalid");
		return 0;
	}

	chan_list = pdev_priv_obj->mas_chan_list;

	freq = reg_compute_chan_to_freq_for_chlist(chan_list, chan_num,
						   min_chan_range,
						   max_chan_range);

	/* If the frequency is a 2G or 5G frequency, then it should be found
	 * in the regulatory mas_chan_list.
	 * If a valid 6G frequency has been returned with the current power mode
	 * itself, then return the freq computed.
	 */
	if (freq)
		return freq;

	min_chan_range = reg_convert_enum_to_6g_idx(min_chan_range);
	max_chan_range = reg_convert_enum_to_6g_idx(max_chan_range);
	if (reg_is_chan_enum_invalid(min_chan_range) ||
	    reg_is_chan_enum_invalid(max_chan_range))
		return freq;

	/* If a valid 6G frequency has not been found, then search in a
	 * power mode's master channel list.
	 */
	input_6g_pwr_mode = REG_AP_LPI;
	while (input_6g_pwr_mode <= REG_CLI_SUB_VLP) {
		chan_list = reg_get_reg_maschan_lst_frm_6g_pwr_mode(
							input_6g_pwr_mode,
							pdev_priv_obj, 0);
		if (!chan_list)
			return 0;

		freq = reg_compute_chan_to_freq_for_chlist(chan_list, chan_num,
							   min_chan_range,
							   max_chan_range);
		if (freq)
			break;
		input_6g_pwr_mode++;
	}

	return freq;
}

uint16_t reg_legacy_chan_to_freq(struct wlan_objmgr_pdev *pdev,
				 uint8_t chan_num)
{
	uint16_t min_chan_range = MIN_24GHZ_CHANNEL;
	uint16_t max_chan_range = MAX_5GHZ_CHANNEL;

	if (chan_num == 0) {
		reg_debug_rl("Invalid channel %d", chan_num);
		return 0;
	}

	return reg_compute_chan_to_freq(pdev, chan_num,
					min_chan_range,
					max_chan_range);
}

#ifdef WLAN_REG_PARTIAL_OFFLOAD
QDF_STATUS reg_program_default_cc(struct wlan_objmgr_pdev *pdev,
				  uint16_t regdmn)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct cur_regulatory_info *reg_info;
	uint16_t cc = -1;
	uint16_t country_index = -1, regdmn_pair = -1;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS err;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("reg soc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	reg_info = (struct cur_regulatory_info *)qdf_mem_malloc
		(sizeof(struct cur_regulatory_info));
	if (!reg_info)
		return QDF_STATUS_E_NOMEM;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	reg_info->psoc = psoc;
	reg_info->phy_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	reg_info->num_phy = 1;

	if (regdmn == 0) {
		reg_get_default_country(&regdmn);
		regdmn |= COUNTRY_ERD_FLAG;
	}

	if (regdmn & COUNTRY_ERD_FLAG) {
		cc = regdmn & ~COUNTRY_ERD_FLAG;

		reg_get_rdpair_from_country_code(cc,
						 &country_index,
						 &regdmn_pair);

		err = reg_get_cur_reginfo(reg_info, country_index, regdmn_pair);
		if (err == QDF_STATUS_E_FAILURE) {
			reg_err("Unable to set country code\n");
			qdf_mem_free(reg_info->reg_rules_2g_ptr);
			qdf_mem_free(reg_info->reg_rules_5g_ptr);
			qdf_mem_free(reg_info);
			return QDF_STATUS_E_FAILURE;
		}

		pdev_priv_obj->ctry_code = cc;

	} else {
		err = reg_get_rdpair_from_regdmn_id(regdmn, &regdmn_pair);
		if (err == QDF_STATUS_E_FAILURE) {
			reg_err("Failed to get regdmn idx for regdmn pair: %x",
				regdmn);
			qdf_mem_free(reg_info->reg_rules_2g_ptr);
			qdf_mem_free(reg_info->reg_rules_5g_ptr);
			qdf_mem_free(reg_info);
			return QDF_STATUS_E_FAILURE;
		}

		err = reg_get_cur_reginfo(reg_info, country_index, regdmn_pair);
		if (err == QDF_STATUS_E_FAILURE) {
			reg_err("Unable to set country code\n");
			qdf_mem_free(reg_info->reg_rules_2g_ptr);
			qdf_mem_free(reg_info->reg_rules_5g_ptr);
			qdf_mem_free(reg_info);
			return QDF_STATUS_E_FAILURE;
		}

		pdev_priv_obj->reg_dmn_pair = regdmn;
	}

	reg_info->offload_enabled = false;
	reg_process_master_chan_list(reg_info);

	qdf_mem_free(reg_info->reg_rules_2g_ptr);
	qdf_mem_free(reg_info->reg_rules_5g_ptr);
	qdf_mem_free(reg_info);

	return QDF_STATUS_SUCCESS;
}

/**
 * reg_program_chan_list_po() - API to program channel list in Partial Offload
 * @psoc: Pointer to psoc object manager
 * @pdev: Pointer to pdev object
 * @rd: Pointer to cc_regdmn_s structure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS reg_program_chan_list_po(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_pdev *pdev,
					   struct cc_regdmn_s *rd)
{
	struct cur_regulatory_info *reg_info;
	uint16_t country_index = -1, regdmn_pair = -1;
	QDF_STATUS err;

	reg_info = (struct cur_regulatory_info *)qdf_mem_malloc
		(sizeof(struct cur_regulatory_info));
	if (!reg_info)
		return QDF_STATUS_E_NOMEM;

	reg_info->psoc = psoc;
	reg_info->phy_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	if (rd->flags == CC_IS_SET) {
		reg_get_rdpair_from_country_code(rd->cc.country_code,
						 &country_index,
						 &regdmn_pair);
	} else if (rd->flags == ALPHA_IS_SET) {
		reg_get_rdpair_from_country_iso(rd->cc.alpha,
						&country_index,
						&regdmn_pair);
	} else if (rd->flags == REGDMN_IS_SET) {
		err = reg_get_rdpair_from_regdmn_id(
				rd->cc.regdmn.reg_2g_5g_pair_id,
				&regdmn_pair);
		if (err == QDF_STATUS_E_FAILURE) {
			reg_err("Failed to get regdmn idx for regdmn pair: %x",
				rd->cc.regdmn.reg_2g_5g_pair_id);
			qdf_mem_free(reg_info->reg_rules_2g_ptr);
			qdf_mem_free(reg_info->reg_rules_5g_ptr);
			qdf_mem_free(reg_info);
			return QDF_STATUS_E_FAILURE;
		}
	}

	err = reg_get_cur_reginfo(reg_info, country_index, regdmn_pair);
	if (err == QDF_STATUS_E_FAILURE) {
		reg_err("Unable to set country code\n");
		qdf_mem_free(reg_info->reg_rules_2g_ptr);
		qdf_mem_free(reg_info->reg_rules_5g_ptr);
		qdf_mem_free(reg_info);
		return QDF_STATUS_E_FAILURE;
	}

	reg_info->offload_enabled = false;
	reg_process_master_chan_list(reg_info);

	qdf_mem_free(reg_info->reg_rules_2g_ptr);
	qdf_mem_free(reg_info->reg_rules_5g_ptr);
	qdf_mem_free(reg_info);

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS reg_program_chan_list_po(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_pdev *pdev,
					   struct cc_regdmn_s *rd)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_REG_PARTIAL_OFFLOAD */

QDF_STATUS reg_program_chan_list(struct wlan_objmgr_pdev *pdev,
				 struct cc_regdmn_s *rd)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	uint8_t pdev_id;
	uint8_t phy_id;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err(" pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (psoc_priv_obj->offload_enabled) {
		if ((rd->flags == ALPHA_IS_SET) && (rd->cc.alpha[2] == 'O'))
			pdev_priv_obj->indoor_chan_enabled = false;
		else
			pdev_priv_obj->indoor_chan_enabled = true;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
		tx_ops = reg_get_psoc_tx_ops(psoc);

		if (tx_ops->get_phy_id_from_pdev_id)
			tx_ops->get_phy_id_from_pdev_id(psoc, pdev_id, &phy_id);
		else
			phy_id = pdev_id;

		if (tx_ops->set_user_country_code) {
			psoc_priv_obj->new_init_ctry_pending[phy_id] = true;
			return tx_ops->set_user_country_code(psoc, pdev_id, rd);
		}

		return QDF_STATUS_E_FAILURE;
	}

	return reg_program_chan_list_po(psoc, pdev, rd);
}

QDF_STATUS reg_get_current_cc(struct wlan_objmgr_pdev *pdev,
			      struct cc_regdmn_s *rd)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("reg pdev priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (rd->flags == CC_IS_SET) {
		rd->cc.country_code = pdev_priv_obj->ctry_code;
	} else if (rd->flags == ALPHA_IS_SET) {
		qdf_mem_copy(rd->cc.alpha, pdev_priv_obj->current_country,
			     sizeof(rd->cc.alpha));
	} else if (rd->flags == REGDMN_IS_SET) {
		rd->cc.regdmn.reg_2g_5g_pair_id = pdev_priv_obj->reg_dmn_pair;
		rd->cc.regdmn.sixg_superdmn_id = pdev_priv_obj->reg_6g_superid;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_set_regdb_offloaded(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->offload_enabled = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_get_curr_regdomain(struct wlan_objmgr_pdev *pdev,
				  struct cur_regdmn_info *cur_regdmn)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	uint16_t index;
	int num_reg_dmn;
	uint8_t phy_id;
	uint8_t pdev_id;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;

	psoc = wlan_pdev_get_psoc(pdev);
	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("soc reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	tx_ops = reg_get_psoc_tx_ops(psoc);
	if (tx_ops->get_phy_id_from_pdev_id)
		tx_ops->get_phy_id_from_pdev_id(psoc, pdev_id, &phy_id);
	else
		phy_id = pdev_id;

	cur_regdmn->regdmn_pair_id =
		psoc_priv_obj->mas_chan_params[phy_id].reg_dmn_pair;

	reg_get_num_reg_dmn_pairs(&num_reg_dmn);
	for (index = 0; index < num_reg_dmn; index++) {
		if (g_reg_dmn_pairs[index].reg_dmn_pair_id ==
				cur_regdmn->regdmn_pair_id)
			break;
	}

	if (index == num_reg_dmn) {
		reg_debug_rl("invalid regdomain");
		return QDF_STATUS_E_FAILURE;
	}

	cur_regdmn->dmn_id_2g = g_reg_dmn_pairs[index].dmn_id_2g;
	cur_regdmn->dmn_id_5g = g_reg_dmn_pairs[index].dmn_id_5g;
	cur_regdmn->ctl_2g = regdomains_2g[cur_regdmn->dmn_id_2g].ctl_val;
	cur_regdmn->ctl_5g = regdomains_5g[cur_regdmn->dmn_id_5g].ctl_val;
	cur_regdmn->dfs_region =
		regdomains_5g[cur_regdmn->dmn_id_5g].dfs_region;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_modify_chan_144(struct wlan_objmgr_pdev *pdev,
			       bool enable_ch_144)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;
	QDF_STATUS status;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (pdev_priv_obj->en_chan_144 == enable_ch_144) {
		reg_info("chan 144 is already  %d", enable_ch_144);
		return QDF_STATUS_SUCCESS;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	reg_debug("setting chan 144: %d", enable_ch_144);
	pdev_priv_obj->en_chan_144 = enable_ch_144;

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);
	if (reg_tx_ops->fill_umac_legacy_chanlist)
		reg_tx_ops->fill_umac_legacy_chanlist(pdev,
				pdev_priv_obj->cur_chan_list);

	status = reg_send_scheduler_msg_sb(psoc, pdev);

	return status;
}

bool reg_get_en_chan_144(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return false;
	}

	return pdev_priv_obj->en_chan_144;
}

struct wlan_psoc_host_hal_reg_capabilities_ext *reg_get_hal_reg_cap(
						struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return NULL;
	}

	return psoc_priv_obj->reg_cap;
}

QDF_STATUS reg_set_hal_reg_cap(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap,
		uint16_t phy_cnt)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (phy_cnt > PSOC_MAX_PHY_REG_CAP) {
		reg_err("phy cnt:%d is more than %d", phy_cnt,
			PSOC_MAX_PHY_REG_CAP);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(psoc_priv_obj->reg_cap, reg_cap,
		     phy_cnt *
		     sizeof(struct wlan_psoc_host_hal_reg_capabilities_ext));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_update_hal_reg_cap(struct wlan_objmgr_psoc *psoc,
				  uint64_t wireless_modes, uint8_t phy_id)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	if (!psoc) {
		reg_err("psoc is null");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->reg_cap[phy_id].wireless_modes |= wireless_modes;

	return QDF_STATUS_SUCCESS;
}

#if defined(CONFIG_BAND_6GHZ) && defined(CONFIG_AFC_SUPPORT)
bool reg_get_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return false;
	}

	return psoc_priv_obj->enable_6ghz_sp_pwrmode_supp;
}

void reg_set_enable_6ghz_sp_mode_support(struct wlan_objmgr_psoc *psoc,
					 bool value)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return;
	}

	psoc_priv_obj->enable_6ghz_sp_pwrmode_supp = value;
}

bool reg_get_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return false;
	}

	return psoc_priv_obj->afc_disable_timer_check;
}

void reg_set_afc_disable_timer_check(struct wlan_objmgr_psoc *psoc,
				     bool value)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return;
	}

	psoc_priv_obj->afc_disable_timer_check = value;
}

bool reg_get_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return false;
	}

	return psoc_priv_obj->afc_disable_request_id_check;
}

void reg_set_afc_disable_request_id_check(struct wlan_objmgr_psoc *psoc,
					  bool value)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return;
	}

	psoc_priv_obj->afc_disable_request_id_check = value;
}

bool reg_get_afc_noaction(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return false;
	}

	return psoc_priv_obj->is_afc_reg_noaction;
}

void reg_set_afc_noaction(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return;
	}

	psoc_priv_obj->is_afc_reg_noaction = value;
}
#endif

bool reg_chan_in_range(struct regulatory_channel *chan_list,
		       qdf_freq_t low_freq_2g, qdf_freq_t high_freq_2g,
		       qdf_freq_t low_freq_5g, qdf_freq_t high_freq_5g,
		       enum channel_enum ch_enum)
{
	uint32_t low_limit_2g = NUM_CHANNELS;
	uint32_t high_limit_2g = NUM_CHANNELS;
	uint32_t low_limit_5g = NUM_CHANNELS;
	uint32_t high_limit_5g = NUM_CHANNELS;
	bool chan_in_range;
	enum channel_enum chan_enum;
	uint16_t min_bw;
	qdf_freq_t center_freq;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		min_bw = chan_list[chan_enum].min_bw;
		center_freq = chan_list[chan_enum].center_freq;

		if ((center_freq - min_bw / 2) >= low_freq_2g) {
			low_limit_2g = chan_enum;
			break;
		}
	}

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
		min_bw = chan_list[chan_enum].min_bw;
		center_freq = chan_list[chan_enum].center_freq;

		if ((center_freq - min_bw / 2) >= low_freq_5g) {
			low_limit_5g = chan_enum;
			break;
		}
	}

	for (chan_enum = NUM_CHANNELS - 1; chan_enum >= 0; chan_enum--) {
		min_bw = chan_list[chan_enum].min_bw;
		center_freq = chan_list[chan_enum].center_freq;

		if (center_freq + min_bw / 2 <= high_freq_2g) {
			high_limit_2g = chan_enum;
			break;
		}
		if (chan_enum == 0)
			break;
	}

	for (chan_enum = NUM_CHANNELS - 1; chan_enum >= 0; chan_enum--) {
		min_bw = chan_list[chan_enum].min_bw;
		center_freq = chan_list[chan_enum].center_freq;

		if (center_freq + min_bw / 2 <= high_freq_5g) {
			high_limit_5g = chan_enum;
			break;
		}
		if (chan_enum == 0)
			break;
	}

	chan_in_range = false;
	if  ((low_limit_2g <= ch_enum) &&
	     (high_limit_2g >= ch_enum) &&
	     (low_limit_2g != NUM_CHANNELS) &&
	     (high_limit_2g != NUM_CHANNELS))
		chan_in_range = true;
	if  ((low_limit_5g <= ch_enum) &&
	     (high_limit_5g >= ch_enum) &&
	     (low_limit_5g != NUM_CHANNELS) &&
	     (high_limit_5g != NUM_CHANNELS))
		chan_in_range = true;

	if (chan_in_range)
		return true;
	else
		return false;
}

bool reg_is_24ghz_ch_freq(uint32_t freq)
{
	return REG_IS_24GHZ_CH_FREQ(freq);
}

bool reg_is_5ghz_ch_freq(uint32_t freq)
{
	return REG_IS_5GHZ_FREQ(freq);
}

/**
 * BAND_2G_PRESENT() - Check if REG_BAND_2G is set in the band_mask
 * @band_mask: Bitmask for bands
 *
 * Return: True if REG_BAND_2G is set in the band_mask, else false
 */
static inline bool BAND_2G_PRESENT(uint8_t band_mask)
{
	return !!(band_mask & (BIT(REG_BAND_2G)));
}

/**
 * BAND_5G_PRESENT() - Check if REG_BAND_5G is set in the band_mask
 * @band_mask: Bitmask for bands
 *
 * Return: True if REG_BAND_5G is set in the band_mask, else false
 */
static inline bool BAND_5G_PRESENT(uint8_t band_mask)
{
	return !!(band_mask & (BIT(REG_BAND_5G)));
}

/**
 * reg_is_freq_in_between() - Check whether freq falls within low_freq and
 * high_freq, inclusively.
 * @low_freq - Low frequency.
 * @high_freq - High frequency.
 * @freq - Frequency to be checked.
 *
 * Return: True if freq falls within low_freq and high_freq, else false.
 */
static bool reg_is_freq_in_between(qdf_freq_t low_freq, qdf_freq_t high_freq,
				   qdf_freq_t freq)
{
	return (low_freq <= freq && freq <= high_freq);
}

static bool reg_is_ranges_overlap(qdf_freq_t low_freq, qdf_freq_t high_freq,
				  qdf_freq_t start_edge_freq,
				  qdf_freq_t end_edge_freq)
{
	return (reg_is_freq_in_between(start_edge_freq,
				       end_edge_freq,
				       low_freq) ||
		reg_is_freq_in_between(start_edge_freq,
				       end_edge_freq,
				       high_freq) ||
		reg_is_freq_in_between(low_freq,
				       high_freq,
				       start_edge_freq) ||
		reg_is_freq_in_between(low_freq,
				       high_freq,
				       end_edge_freq));
}

bool reg_is_range_overlap_2g(qdf_freq_t low_freq, qdf_freq_t high_freq)
{
	return reg_is_ranges_overlap(low_freq, high_freq,
				     TWO_GIG_STARTING_EDGE_FREQ,
				     TWO_GIG_ENDING_EDGE_FREQ);
}

bool reg_is_range_overlap_5g(qdf_freq_t low_freq, qdf_freq_t high_freq)
{
	return reg_is_ranges_overlap(low_freq, high_freq,
				     FIVE_GIG_STARTING_EDGE_FREQ,
				     FIVE_GIG_ENDING_EDGE_FREQ);
}

static struct regulatory_channel *
reg_get_reg_chan(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct regulatory_channel *cur_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_enum chan_enum;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return NULL;
	}

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err_rl("Invalid chan enum %d", chan_enum);
		return NULL;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;
	if (cur_chan_list[chan_enum].state == CHANNEL_STATE_DISABLE) {
		reg_err("Channel %u is not enabled for this pdev", freq);
		return NULL;
	}

	return &cur_chan_list[chan_enum];
}

bool reg_is_freq_indoor(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct regulatory_channel *reg_chan;

	reg_chan = reg_get_reg_chan(pdev, freq);

	if (!reg_chan) {
		reg_err("reg channel is NULL");
		return false;
	}

	return (reg_chan->chan_flags &
		REGULATORY_CHAN_INDOOR_ONLY);
}

uint16_t reg_get_min_chwidth(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct regulatory_channel *reg_chan;

	reg_chan = reg_get_reg_chan(pdev, freq);

	if (!reg_chan) {
		reg_err("reg channel is NULL");
		return 0;
	}

	return reg_chan->min_bw;
}

uint16_t reg_get_max_chwidth(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct regulatory_channel *reg_chan;

	reg_chan = reg_get_reg_chan(pdev, freq);

	if (!reg_chan) {
		reg_err("reg channel is NULL");
		return 0;
	}

	return reg_chan->max_bw;
}

#ifdef CONFIG_REG_CLIENT
bool reg_is_freq_indoor_in_secondary_list(struct wlan_objmgr_pdev *pdev,
					  qdf_freq_t freq)
{
	struct regulatory_channel *secondary_cur_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_enum chan_enum;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return false;
	}

	chan_enum = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err_rl("Invalid chan enum %d", chan_enum);
		return false;
	}

	secondary_cur_chan_list = pdev_priv_obj->secondary_cur_chan_list;

	return (secondary_cur_chan_list[chan_enum].chan_flags &
		REGULATORY_CHAN_INDOOR_ONLY);
}
#endif

#ifdef CONFIG_BAND_6GHZ
bool reg_is_6ghz_chan_freq(uint16_t freq)
{
	return REG_IS_6GHZ_FREQ(freq);
}

#ifdef CONFIG_6G_FREQ_OVERLAP
bool reg_is_range_overlap_6g(qdf_freq_t low_freq, qdf_freq_t high_freq)
{
	return reg_is_ranges_overlap(low_freq, high_freq,
				     SIX_GIG_STARTING_EDGE_FREQ,
				     SIX_GIG_ENDING_EDGE_FREQ);
}

bool reg_is_range_only6g(qdf_freq_t low_freq, qdf_freq_t high_freq)
{
	if (low_freq >= high_freq) {
		reg_err_rl("Low freq is greater than or equal to high freq");
		return false;
	}

	if (reg_is_range_overlap_6g(low_freq, high_freq) &&
	    !reg_is_range_overlap_5g(low_freq, high_freq)) {
		reg_debug_rl("The device is 6G only");
		return true;
	}

	reg_debug_rl("The device is not 6G only");

	return false;
}
#endif

uint16_t reg_min_6ghz_chan_freq(void)
{
	return REG_MIN_6GHZ_CHAN_FREQ;
}

uint16_t reg_max_6ghz_chan_freq(void)
{
	return REG_MAX_6GHZ_CHAN_FREQ;
}

bool reg_is_6ghz_psc_chan_freq(uint16_t freq)
{
	if (!REG_IS_6GHZ_FREQ(freq)) {
		reg_debug(" Channel frequency is not a 6GHz frequency");
		return false;
	}

	if (!(((freq - SIX_GHZ_NON_ORPHAN_START_FREQ) + FREQ_LEFT_SHIFT) %
	      (FREQ_TO_CHAN_SCALE * NUM_80MHZ_BAND_IN_6G))) {
		return true;
	}

	reg_debug_rl("Channel freq %d MHz is not a 6GHz PSC frequency", freq);

	return false;
}

bool reg_is_6g_freq_indoor(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	return (REG_IS_6GHZ_FREQ(freq) && reg_is_freq_indoor(pdev, freq));
}

/**
 * reg_get_max_psd() - Get max PSD.
 * @freq: Channel frequency.
 * @bw: Channel bandwidth.
 * @reg_ap: Regulatory 6G AP type.
 * @reg_client: Regulatory 6G client type.
 * @tx_power: Pointer to tx-power.
 *
 * Return: Return QDF_STATUS_SUCCESS, if PSD is filled for 6G TPE IE
 * else return QDF_STATUS_E_FAILURE.
 */
static QDF_STATUS reg_get_max_psd(qdf_freq_t freq,
				  uint16_t bw,
				  enum reg_6g_ap_type reg_ap,
				  enum reg_6g_client_type reg_client,
				  uint8_t *tx_power)
{
	if (reg_ap == REG_INDOOR_AP ||
	    reg_ap == REG_VERY_LOW_POWER_AP) {
		switch (reg_client) {
		case REG_DEFAULT_CLIENT:
			*tx_power = REG_PSD_MAX_TXPOWER_FOR_DEFAULT_CLIENT;
			return QDF_STATUS_SUCCESS;
		case REG_SUBORDINATE_CLIENT:
			*tx_power = REG_PSD_MAX_TXPOWER_FOR_SUBORDINATE_CLIENT;
			return QDF_STATUS_SUCCESS;
		default:
			reg_err_rl("Invalid client type");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

/**
 * reg_get_max_txpower_for_eirp() - Get max EIRP.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency.
 * @bw: Channel bandwidth.
 * @reg_ap: Regulatory 6G AP type.
 * @reg_client: Regulatory client type.
 * @tx_power: Pointer to tx-power.
 *
 * Return: Return QDF_STATUS_SUCCESS, if EIRP is filled for 6G TPE IE
 * else return QDF_STATUS_E_FAILURE.
 */
static QDF_STATUS reg_get_max_eirp(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq,
				   uint16_t bw,
				   enum reg_6g_ap_type reg_ap,
				   enum reg_6g_client_type reg_client,
				   uint8_t *tx_power)
{
	if (reg_ap == REG_INDOOR_AP ||
	    reg_ap == REG_VERY_LOW_POWER_AP) {
		switch (reg_client) {
		case REG_DEFAULT_CLIENT:
			*tx_power = reg_get_channel_reg_power_for_freq(pdev,
								       freq);
			return QDF_STATUS_SUCCESS;
		case REG_SUBORDINATE_CLIENT:
			*tx_power = REG_EIRP_MAX_TXPOWER_FOR_SUBORDINATE_CLIENT;
			return QDF_STATUS_SUCCESS;
		default:
			reg_err_rl("Invalid client type");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS reg_get_max_txpower_for_6g_tpe(struct wlan_objmgr_pdev *pdev,
					  qdf_freq_t freq, uint8_t bw,
					  enum reg_6g_ap_type reg_ap,
					  enum reg_6g_client_type reg_client,
					  bool is_psd,
					  uint8_t *tx_power)
{
	if (!REG_IS_6GHZ_FREQ(freq)) {
		reg_err_rl("%d is not a 6G channel frequency", freq);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * For now, there is support only for Indoor AP and we have only
	 * LPI power values.
	 */
	if (is_psd)
		return reg_get_max_psd(freq, bw, reg_ap, reg_client, tx_power);

	return reg_get_max_eirp(pdev, freq, bw, reg_ap, reg_client, tx_power);
}

/**
 * BAND_6G_PRESENT() - Check if REG_BAND_6G is set in the band_mask
 * @band_mask: Bitmask for bands
 *
 * Return: True if REG_BAND_6G is set in the band_mask, else false
 */
static inline bool BAND_6G_PRESENT(uint8_t band_mask)
{
	return !!(band_mask & (BIT(REG_BAND_6G)));
}
#else
static inline bool BAND_6G_PRESENT(uint8_t band_mask)
{
	return false;
}
#endif /* CONFIG_BAND_6GHZ */

/**
 * reg_get_band_from_cur_chan_list() - Get channel list and number of channels
 * @pdev: pdev ptr
 * @band_mask: Input bitmap with band set
 * @channel_list: Pointer to Channel List
 * @cur_chan_list: Pointer to primary current channel list for non-beaconing
 * entities (STA, p2p client) and secondary channel list for beaconing entities
 * (SAP, p2p GO)
 *
 * Get the given channel list and number of channels from the current channel
 * list based on input band bitmap.
 *
 * Return: Number of channels, else 0 to indicate error
 */
static uint16_t
reg_get_band_from_cur_chan_list(struct wlan_objmgr_pdev *pdev,
				uint8_t band_mask,
				struct regulatory_channel *channel_list,
				struct regulatory_channel *cur_chan_list)
{
	uint16_t i, num_channels = 0;

	if (BAND_2G_PRESENT(band_mask)) {
		for (i = MIN_24GHZ_CHANNEL; i <= MAX_24GHZ_CHANNEL; i++) {
			if ((cur_chan_list[i].state != CHANNEL_STATE_DISABLE) &&
			    !(cur_chan_list[i].chan_flags &
			      REGULATORY_CHAN_DISABLED)) {
				channel_list[num_channels] = cur_chan_list[i];
				num_channels++;
			}
		}
	}
	if (BAND_5G_PRESENT(band_mask)) {
		for (i = BAND_5GHZ_START_CHANNEL; i <= MAX_5GHZ_CHANNEL; i++) {
			if ((cur_chan_list[i].state != CHANNEL_STATE_DISABLE) &&
			    !(cur_chan_list[i].chan_flags &
			      REGULATORY_CHAN_DISABLED)) {
				channel_list[num_channels] = cur_chan_list[i];
				num_channels++;
			}
		}
	}
	if (BAND_6G_PRESENT(band_mask)) {
		for (i = MIN_6GHZ_CHANNEL; i <= MAX_6GHZ_CHANNEL; i++) {
			if ((cur_chan_list[i].state != CHANNEL_STATE_DISABLE) &&
			    !(cur_chan_list[i].chan_flags &
			      REGULATORY_CHAN_DISABLED)) {
				channel_list[num_channels] = cur_chan_list[i];
				num_channels++;
			}
		}
	}

	if (!num_channels) {
		reg_err("Failed to retrieve the channel list");
		return 0;
	}

	return num_channels;
}

uint16_t
reg_get_band_channel_list(struct wlan_objmgr_pdev *pdev,
			  uint8_t band_mask,
			  struct regulatory_channel *channel_list)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return 0;
	}

	return reg_get_band_from_cur_chan_list(pdev, band_mask, channel_list,
					       pdev_priv_obj->cur_chan_list);
}

#ifdef CONFIG_REG_6G_PWRMODE
uint16_t
reg_get_band_channel_list_for_pwrmode(struct wlan_objmgr_pdev *pdev,
				      uint8_t band_mask,
				      struct regulatory_channel *channel_list,
				      enum supported_6g_pwr_types
				      in_6g_pwr_mode)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *reg_chan_list;
	uint16_t nchan = 0;

	reg_chan_list = qdf_mem_malloc(NUM_CHANNELS * sizeof(*reg_chan_list));

	if (!reg_chan_list)
		return 0;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		goto err;
	}

	if (reg_get_pwrmode_chan_list(pdev, reg_chan_list, in_6g_pwr_mode)) {
		reg_debug_rl("Unable to get powermode channel list");
		goto err;
	}

	nchan = reg_get_band_from_cur_chan_list(pdev, band_mask, channel_list,
						reg_chan_list);
err:
	qdf_mem_free(reg_chan_list);
	return nchan;
}
#endif

#ifdef CONFIG_REG_CLIENT
uint16_t
reg_get_secondary_band_channel_list(struct wlan_objmgr_pdev *pdev,
				    uint8_t band_mask,
				    struct regulatory_channel *channel_list)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return 0;
	}

	return reg_get_band_from_cur_chan_list(
				pdev, band_mask, channel_list,
				pdev_priv_obj->secondary_cur_chan_list);
}
#endif

qdf_freq_t reg_chan_band_to_freq(struct wlan_objmgr_pdev *pdev,
				 uint8_t chan_num,
				 uint8_t band_mask)
{
	enum channel_enum min_chan, max_chan;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint16_t freq;

	if (chan_num == 0) {
		reg_debug_rl("Invalid channel %d", chan_num);
		return 0;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return 0;
	}

	if (BAND_6G_PRESENT(band_mask)) {
		if (BAND_2G_PRESENT(band_mask) ||
		    BAND_5G_PRESENT(band_mask)) {
			reg_err_rl("Incorrect band_mask %x", band_mask);
				return 0;
		}

		/* Handle 6G channel 2 as a special case as it does not follow
		 * the regular increasing order of channel numbers
		 */
		if (chan_num == SIXG_CHAN_2) {
			struct regulatory_channel *mas_chan_list;

			mas_chan_list = pdev_priv_obj->mas_chan_list;
			/* Check if chan 2 is in the master list */
			if ((mas_chan_list[CHAN_ENUM_SIXG_2].state !=
			     CHANNEL_STATE_DISABLE) &&
			    !(mas_chan_list[CHAN_ENUM_SIXG_2].chan_flags &
			     REGULATORY_CHAN_DISABLED))
				return mas_chan_list[CHAN_ENUM_SIXG_2].
								center_freq;
			else
				return 0;
		}

		/* MIN_6GHZ_CHANNEL corresponds to CHAN_ENUM_5935
		 * ( a.k.a SIXG_CHAN_2). Skip it from the search space
		 */
		min_chan = MIN_6GHZ_CHANNEL + 1;
		max_chan = MAX_6GHZ_CHANNEL;
		return reg_compute_chan_to_freq(pdev, chan_num,
						min_chan,
						max_chan);
	} else {
		if (BAND_2G_PRESENT(band_mask)) {
			min_chan = MIN_24GHZ_CHANNEL;
			max_chan = MAX_24GHZ_CHANNEL;
			freq = reg_compute_chan_to_freq(pdev, chan_num,
							min_chan,
							max_chan);
			if (freq != 0)
				return freq;
		}

		if (BAND_5G_PRESENT(band_mask)) {
			min_chan = BAND_5GHZ_START_CHANNEL;
			max_chan = MAX_5GHZ_CHANNEL;

			return reg_compute_chan_to_freq(pdev, chan_num,
							min_chan,
							max_chan);
		}

		reg_err_rl("Incorrect band_mask %x", band_mask);
		return 0;
	}
}

#ifdef CONFIG_49GHZ_CHAN
bool reg_is_49ghz_freq(qdf_freq_t freq)
{
	return REG_IS_49GHZ_FREQ(freq);
}
#endif /* CONFIG_49GHZ_CHAN */

qdf_freq_t reg_ch_num(uint32_t ch_enum)
{
	return REG_CH_NUM(ch_enum);
}

qdf_freq_t reg_ch_to_freq(uint32_t ch_enum)
{
	return REG_CH_TO_FREQ(ch_enum);
}

uint8_t reg_max_5ghz_ch_num(void)
{
	return g_reg_max_5g_chan_num;
}

#ifdef CONFIG_CHAN_FREQ_API
qdf_freq_t reg_min_24ghz_chan_freq(void)
{
	return REG_MIN_24GHZ_CH_FREQ;
}

qdf_freq_t reg_max_24ghz_chan_freq(void)
{
	return REG_MAX_24GHZ_CH_FREQ;
}

qdf_freq_t reg_min_5ghz_chan_freq(void)
{
	return REG_MIN_5GHZ_CH_FREQ;
}

qdf_freq_t reg_max_5ghz_chan_freq(void)
{
	return REG_MAX_5GHZ_CH_FREQ;
}
#endif /* CONFIG_CHAN_FREQ_API */

QDF_STATUS reg_enable_dfs_channels(struct wlan_objmgr_pdev *pdev,
				   bool enable)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (pdev_priv_obj->dfs_enabled == enable) {
		reg_info("dfs_enabled is already set to %d", enable);
		return QDF_STATUS_SUCCESS;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	reg_info("set dfs_enabled: %d", enable);

	pdev_priv_obj->dfs_enabled = enable;

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);

	/* Fill the ic channel list with the updated current channel
	 * chan list.
	 */
	if (reg_tx_ops->fill_umac_legacy_chanlist)
		reg_tx_ops->fill_umac_legacy_chanlist(pdev,
				pdev_priv_obj->cur_chan_list);

	status = reg_send_scheduler_msg_sb(psoc, pdev);

	return status;
}

#ifdef WLAN_REG_PARTIAL_OFFLOAD
bool reg_is_regdmn_en302502_applicable(struct wlan_objmgr_pdev *pdev)
{
	struct cur_regdmn_info cur_reg_dmn;
	QDF_STATUS status;

	status = reg_get_curr_regdomain(pdev, &cur_reg_dmn);
	if (status != QDF_STATUS_SUCCESS) {
		reg_err("Failed to get reg domain");
		return false;
	}

	return reg_en302_502_regdmn(cur_reg_dmn.regdmn_pair_id);
}
#endif

QDF_STATUS reg_get_phybitmap(struct wlan_objmgr_pdev *pdev,
			     uint16_t *phybitmap)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!pdev_priv_obj) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAULT;
	}

	*phybitmap = pdev_priv_obj->phybitmap;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_update_channel_ranges(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;
	struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap_ptr;
	uint32_t cnt;
	uint8_t phy_id;
	uint8_t pdev_id;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);
	if (reg_tx_ops->get_phy_id_from_pdev_id)
		reg_tx_ops->get_phy_id_from_pdev_id(psoc, pdev_id, &phy_id);
	else
		phy_id = pdev_id;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	reg_cap_ptr = psoc_priv_obj->reg_cap;

	for (cnt = 0; cnt < PSOC_MAX_PHY_REG_CAP; cnt++) {
		if (!reg_cap_ptr) {
			qdf_mem_free(pdev_priv_obj);
			reg_err("reg cap ptr is NULL");
			return QDF_STATUS_E_FAULT;
		}

		if (reg_cap_ptr->phy_id == phy_id)
			break;
		reg_cap_ptr++;
	}

	if (cnt == PSOC_MAX_PHY_REG_CAP) {
		qdf_mem_free(pdev_priv_obj);
		reg_err("extended capabilities not found for pdev");
		return QDF_STATUS_E_FAULT;
	}
	pdev_priv_obj->range_2g_low = reg_cap_ptr->low_2ghz_chan;
	pdev_priv_obj->range_2g_high = reg_cap_ptr->high_2ghz_chan;
	pdev_priv_obj->range_5g_low = reg_cap_ptr->low_5ghz_chan;
	pdev_priv_obj->range_5g_high = reg_cap_ptr->high_5ghz_chan;
	pdev_priv_obj->wireless_modes = reg_cap_ptr->wireless_modes;

	return status;
}

QDF_STATUS reg_modify_pdev_chan_range(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;
	enum direction dir;
	QDF_STATUS status;

	status = reg_update_channel_ranges(pdev);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (psoc_priv_obj->offload_enabled) {
		dir = NORTHBOUND;
	} else {
		dir = SOUTHBOUND;
	}

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);

	/* Fill the ic channel list with the updated current channel
	 * chan list.
	 */
	if (reg_tx_ops->fill_umac_legacy_chanlist) {
	    reg_tx_ops->fill_umac_legacy_chanlist(pdev,
						  pdev_priv_obj->cur_chan_list);

	} else {
		if (dir == NORTHBOUND)
			status = reg_send_scheduler_msg_nb(psoc, pdev);
		else
			status = reg_send_scheduler_msg_sb(psoc, pdev);
	}

	return status;
}

QDF_STATUS reg_update_pdev_wireless_modes(struct wlan_objmgr_pdev *pdev,
					  uint64_t wireless_modes)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
							      WLAN_UMAC_COMP_REGULATORY);

	if (!pdev_priv_obj) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	pdev_priv_obj->wireless_modes = wireless_modes;

	return QDF_STATUS_SUCCESS;
}

#ifdef DISABLE_UNII_SHARED_BANDS
/**
 * reg_is_reg_unii_band_1_or_reg_unii_band_2a() - Check the input bitmap
 * @unii_5g_bitmap: 5G UNII band bitmap
 *
 * This function checks if either REG_UNII_BAND_1 or REG_UNII_BAND_2A,
 * are present in the 5G UNII band bitmap.
 *
 * Return: Return true if REG_UNII_BAND_1 or REG_UNII_BAND_2A, are present in
 * the UNII 5g bitmap else return false.
 */
static bool
reg_is_reg_unii_band_1_or_reg_unii_band_2a(uint8_t unii_5g_bitmap)
{
	if (!unii_5g_bitmap)
		return false;

	return ((unii_5g_bitmap & (BIT(REG_UNII_BAND_1) |
		 BIT(REG_UNII_BAND_2A))) ==  unii_5g_bitmap);
}

QDF_STATUS reg_disable_chan_coex(struct wlan_objmgr_pdev *pdev,
				 uint8_t unii_5g_bitmap)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (unii_5g_bitmap &&
	    !reg_is_reg_unii_band_1_or_reg_unii_band_2a(unii_5g_bitmap)) {
		reg_err_rl("Invalid unii_5g_bitmap =  %d", unii_5g_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	if (pdev_priv_obj->unii_5g_bitmap == unii_5g_bitmap) {
		reg_debug_rl("UNII bitmask for 5G channels is already set  %d",
			    unii_5g_bitmap);
		return QDF_STATUS_SUCCESS;
	}

	reg_debug_rl("Setting UNII bitmask for 5G: %d", unii_5g_bitmap);
	pdev_priv_obj->unii_5g_bitmap = unii_5g_bitmap;

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);

	if (reg_tx_ops->fill_umac_legacy_chanlist) {
		reg_tx_ops->fill_umac_legacy_chanlist(pdev,
				pdev_priv_obj->cur_chan_list);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

bool reg_is_chan_disabled(uint32_t chan_flags, enum channel_state chan_state)
{
	return (REGULATORY_CHAN_DISABLED & chan_flags ||
		chan_state == CHANNEL_STATE_DISABLE);
}

#ifdef CONFIG_REG_CLIENT
#ifdef CONFIG_BAND_6GHZ
static void reg_append_6g_channel_list_with_power(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			struct channel_power *ch_list,
			uint8_t *count,
			enum supported_6g_pwr_types in_6g_pwr_type)
{
	struct super_chan_info *sc_entry;
	enum supported_6g_pwr_types pwr_type;
	uint8_t i, count_6g = *count;

	pwr_type = in_6g_pwr_type;
	for (i = 0; i < NUM_6GHZ_CHANNELS; i++) {
		sc_entry = &pdev_priv_obj->super_chan_list[i];

		if (in_6g_pwr_type == REG_BEST_PWR_MODE)
			pwr_type = sc_entry->best_power_mode;

		if (reg_is_supp_pwr_mode_invalid(pwr_type))
			continue;

		if (!reg_is_chan_disabled(sc_entry->chan_flags_arr[pwr_type],
					  sc_entry->state_arr[pwr_type])) {
			ch_list[count_6g].center_freq =
					reg_ch_to_freq(i + MIN_6GHZ_CHANNEL);
			ch_list[count_6g].chan_num =
					reg_ch_num(i + MIN_6GHZ_CHANNEL);
			ch_list[count_6g++].tx_power =
				sc_entry->reg_chan_pwr[pwr_type].tx_power;
		}
	}
	*count = count_6g;
}
#else
static inline void reg_append_6g_channel_list_with_power(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			struct channel_power *ch_list,
			uint8_t *count,
			enum supported_6g_pwr_types in_6g_pwr_type)
{
}
#endif

QDF_STATUS reg_get_channel_list_with_power(
				struct wlan_objmgr_pdev *pdev,
				struct channel_power *ch_list,
				uint8_t *num_chan,
				enum supported_6g_pwr_types in_6g_pwr_type)
{
	uint8_t i, count;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint8_t max_curr_num_chan;

	if (!pdev) {
		reg_err_rl("invalid pdev");
		return QDF_STATUS_E_INVAL;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("reg pdev priv obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!num_chan || !ch_list) {
		reg_err("chan_list or num_ch is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*num_chan = 0;

	if (in_6g_pwr_type == REG_CURRENT_PWR_MODE)
		max_curr_num_chan = NUM_CHANNELS;
	else
		max_curr_num_chan = MAX_5GHZ_CHANNEL;

	for (i = 0, count = 0; i < max_curr_num_chan; i++) {
		if (!reg_is_chan_disabled(
				pdev_priv_obj->cur_chan_list[i].chan_flags,
				pdev_priv_obj->cur_chan_list[i].state)) {
			ch_list[count].center_freq =
				pdev_priv_obj->cur_chan_list[i].center_freq;
			ch_list[count].chan_num =
				pdev_priv_obj->cur_chan_list[i].chan_num;
			ch_list[count++].tx_power =
				pdev_priv_obj->cur_chan_list[i].tx_power;
		}
	}

	if (in_6g_pwr_type == REG_CURRENT_PWR_MODE) {
		*num_chan = count;
		return QDF_STATUS_SUCCESS;
	}

	reg_append_6g_channel_list_with_power(pdev_priv_obj, ch_list, &count,
					      in_6g_pwr_type);
	*num_chan = count;

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef CONFIG_CHAN_FREQ_API
QDF_STATUS reg_get_channel_list_with_power_for_freq(struct wlan_objmgr_pdev
						    *pdev,
						    struct channel_power
						    *ch_list,
						    uint8_t *num_chan)
{
	int i, count;
	struct regulatory_channel *reg_channels;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	if (!num_chan || !ch_list) {
		reg_err("chan_list or num_ch is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* set the current channel list */
	reg_channels = pdev_priv_obj->cur_chan_list;

	for (i = 0, count = 0; i < NUM_CHANNELS; i++) {
		if (reg_channels[i].state &&
		    !(reg_channels[i].chan_flags & REGULATORY_CHAN_DISABLED)) {
			ch_list[count].center_freq =
				reg_channels[i].center_freq;
			ch_list[count].chan_num = reg_channels[i].chan_num;
			ch_list[count++].tx_power =
				reg_channels[i].tx_power;
		}
	}

	*num_chan = count;

	return QDF_STATUS_SUCCESS;
}

enum channel_enum reg_get_chan_enum_for_freq(qdf_freq_t freq)
{
	int16_t start = 0;
	int16_t end = NUM_CHANNELS - 1;

	while (start <= end) {
		int16_t middle = (start + end) / 2;
		qdf_freq_t mid_freq_elem = channel_map[middle].center_freq;

		if (freq == mid_freq_elem)
			return middle;
		if (freq > mid_freq_elem)
			start = middle + 1;
		else
			end = middle - 1;
	}

	reg_debug_rl("invalid channel center frequency %d", freq);

	return INVALID_CHANNEL;
}

bool
reg_is_freq_present_in_cur_chan_list(struct wlan_objmgr_pdev *pdev,
				     qdf_freq_t freq)
{
	enum channel_enum chan_enum;
	struct regulatory_channel *cur_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("pdev reg obj is NULL");
		return false;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++)
		if (cur_chan_list[chan_enum].center_freq == freq)
			if ((cur_chan_list[chan_enum].state !=
			     CHANNEL_STATE_DISABLE) &&
			    !(cur_chan_list[chan_enum].chan_flags &
			      REGULATORY_CHAN_DISABLED))
				return true;

	reg_debug_rl("Channel center frequency %d not found", freq);

	return false;
}

#ifdef WLAN_FEATURE_GET_USABLE_CHAN_LIST
/**
 * is_freq_present_in_resp_list() - is freq present in resp list
 *
 * @pcl_ch: pcl ch
 * @res_msg: Response msg
 * @count: no of usable channels
 *
 * Return: void
 */
static bool
is_freq_present_in_resp_list(uint32_t pcl_ch,
			     struct get_usable_chan_res_params *res_msg,
			     int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (res_msg[i].freq == pcl_ch)
			return true;
	}
	return false;
}

/**
 * reg_update_usable_chan_resp() - Update response msg
 * @pdev: Pointer to pdev
 * @res_msg: Response msg
 * @pcl_ch: pcl channel
 * @len: calculated pcl len
 * @iface_mode_mask: interface type
 * @band_mask: requested band mask
 * @count: no of usable channels
 *
 * Return: void
 */
static void
reg_update_usable_chan_resp(struct wlan_objmgr_pdev *pdev,
			    struct get_usable_chan_res_params *res_msg,
			    uint32_t *pcl_ch, uint32_t len,
			    uint32_t iface_mode_mask,
			    uint32_t band_mask, int *count)
{
	int i;
	struct ch_params ch_params = {0};
	int index = *count;

	for (i = 0; i < len && index < NUM_CHANNELS; i++) {
		/* In case usable channels are required for multiple filter
		 * mask, Some frequencies may present in res_msg . To avoid
		 * frequency duplication, only mode mask is updated for
		 * existing frequency.
		 */
		if (is_freq_present_in_resp_list(pcl_ch[i], res_msg, *count))
			continue;

		if (!(band_mask & 1 << wlan_reg_freq_to_band(pcl_ch[i])))
			continue;

		ch_params.ch_width = CH_WIDTH_MAX;
		reg_set_channel_params_for_freq(
				pdev,
				pcl_ch[i],
				0, &ch_params, true);
		res_msg[index].freq = (qdf_freq_t)pcl_ch[i];
		res_msg[index].iface_mode_mask |= 1 << iface_mode_mask;
		res_msg[index].bw = ch_params.ch_width;
		if (ch_params.center_freq_seg0)
			res_msg[index].seg0_freq =
					ch_params.center_freq_seg0;
		if (ch_params.center_freq_seg1)
			res_msg[index].seg1_freq =
					ch_params.center_freq_seg1;
		index++;
	}

	*count = index;
}

/**
 * reg_update_conn_chan_list() - Get usable channels with conn filter
 *				 and policy mgr mask
 * @pdev: Pointer to pdev
 * @res_msg: Response msg
 * @policy_mgr_con_mode: policy mgr mode
 * @iftype: interface type
 * @band_mask: requested band mask
 * @count: no of usable channels
 *
 * Return: qdf status
 */
static QDF_STATUS
reg_update_conn_chan_list(struct wlan_objmgr_pdev *pdev,
			  struct get_usable_chan_res_params *res_msg,
			  enum policy_mgr_con_mode mode,
			  uint32_t iftype,
			  uint32_t band_mask,
			  uint32_t *count)
{
	uint32_t *pcl_ch;
	uint8_t *weight_list;
	uint32_t len;
	uint32_t weight_len;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pcl_ch = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(uint32_t));

	if (!pcl_ch) {
		reg_err("pcl_ch invalid");
		return QDF_STATUS_E_FAILURE;
	}

	weight_list = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(uint8_t));

	if (!weight_list) {
		reg_err("weight_list invalid");
		qdf_mem_free(pcl_ch);
		return QDF_STATUS_E_FAILURE;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("invalid psoc");
		status = QDF_STATUS_E_FAILURE;
		goto err;
	}

	len = NUM_CHANNELS;
	weight_len = NUM_CHANNELS;

	status = policy_mgr_get_pcl(psoc, mode, pcl_ch, &len,
				    weight_list, weight_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_err("get pcl failed for mode: %d", mode);
		goto err;
	}
	reg_update_usable_chan_resp(pdev, res_msg, pcl_ch, len,
				    iftype, band_mask, count);
err:
	qdf_mem_free(pcl_ch);
	qdf_mem_free(weight_list);
	return status;
}

/**
 * reg_get_usable_channel_con_filter() - Get usable channel with con filter mask
 * @pdev: Pointer to pdev
 * @req_msg: Request msg
 * @res_msg: Response msg
 * @chan_list: reg channel list
 * @count: no of usable channels
 *
 * Return: qdf status
 */
static QDF_STATUS
reg_get_usable_channel_con_filter(struct wlan_objmgr_pdev *pdev,
				  struct get_usable_chan_req_params req_msg,
				  struct get_usable_chan_res_params *res_msg,
				  int *count)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t iface_mode_mask = req_msg.iface_mode_mask;

	while (iface_mode_mask) {
		if (iface_mode_mask & 1 << IFTYPE_AP) {
			status =
			reg_update_conn_chan_list(pdev, res_msg, PM_SAP_MODE,
						  IFTYPE_AP, req_msg.band_mask,
						  count);
			iface_mode_mask &= ~(1 << IFTYPE_AP);
		} else if (iface_mode_mask & 1 << IFTYPE_STATION) {
			status =
			reg_update_conn_chan_list(pdev, res_msg, PM_STA_MODE,
						  IFTYPE_STATION,
						  req_msg.band_mask, count);
			iface_mode_mask &= ~(1 << IFTYPE_STATION);
		} else if (iface_mode_mask & 1 << IFTYPE_P2P_GO) {
			status =
			reg_update_conn_chan_list(pdev, res_msg, PM_P2P_GO_MODE,
						  IFTYPE_P2P_GO,
						  req_msg.band_mask, count);
			iface_mode_mask &= ~(1 << IFTYPE_P2P_GO);
		} else if (iface_mode_mask & 1 << IFTYPE_P2P_CLIENT) {
			status =
			reg_update_conn_chan_list(pdev, res_msg,
						  PM_P2P_CLIENT_MODE,
						  IFTYPE_P2P_CLIENT,
						  req_msg.band_mask, count);
			iface_mode_mask &= ~(1 << IFTYPE_P2P_CLIENT);
		} else if (iface_mode_mask & 1 << IFTYPE_NAN) {
			status =
			reg_update_conn_chan_list(pdev, res_msg,
						  PM_NAN_DISC_MODE, IFTYPE_NAN,
						  req_msg.band_mask, count);
			iface_mode_mask &= ~(1 << IFTYPE_NAN);
		} else {
			reg_err("invalid mode");
			break;
		}
	}
	return status;
}

/**
 * reg_remove_freq() - Remove invalid freq
 * @res_msg: Response msg
 * @index: index of freq that needs to be removed
 *
 * Return: void
 */
static void
reg_remove_freq(struct get_usable_chan_res_params *res_msg,
		int index)
{
	reg_debug("removing freq %d", res_msg[index].freq);
	qdf_mem_zero(&res_msg[index],
		     sizeof(struct get_usable_chan_res_params));
}

/**
 * reg_skip_invalid_chan_freq() - Remove invalid freq for SAP, P2P GO
 *				  and NAN
 * @pdev: Pointer to pdev
 * @res_msg: Response msg
 * @count: no of usable channels
 * @iface_mode_mask: interface mode mask
 *
 * Return: qdf status
 */
static QDF_STATUS
reg_skip_invalid_chan_freq(struct wlan_objmgr_pdev *pdev,
			   struct get_usable_chan_res_params *res_msg,
			   uint32_t *no_usable_channels,
			   uint32_t iface_mode_mask)
{
	uint32_t chan_enum, iface_mode = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool include_indoor_channel, dfs_master_capable;
	uint8_t enable_srd_chan, srd_mask = 0;
	struct wlan_objmgr_psoc *psoc;
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("invalid psoc");
		return QDF_STATUS_E_FAILURE;
	}

	status = ucfg_mlme_get_indoor_channel_support(psoc,
						      &include_indoor_channel);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_err("failed to get indoor channel skip info");
		return QDF_STATUS_E_FAILURE;
	}

	ucfg_mlme_get_etsi_srd_chan_in_master_mode(psoc,
						   &enable_srd_chan);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_err("failed to get srd chan info");
		return QDF_STATUS_E_FAILURE;
	}

	status = ucfg_mlme_get_dfs_master_capability(psoc, &dfs_master_capable);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_err("failed to get dfs master capable");
		return status;
	}

	while (iface_mode_mask) {
		if (iface_mode_mask & (1 << IFTYPE_AP)) {
			srd_mask = 1;
			iface_mode = 1 << IFTYPE_AP;
		} else if (iface_mode_mask & (1 << IFTYPE_P2P_GO)) {
			srd_mask = 2;
			iface_mode = 1 << IFTYPE_P2P_GO;
		} else if (iface_mode_mask & (1 << IFTYPE_NAN)) {
			iface_mode = 1 << IFTYPE_NAN;
		} else {
			break;
		}
		for (chan_enum = 0; chan_enum < *no_usable_channels;
		     chan_enum++) {
			if (iface_mode_mask & (1 << IFTYPE_NAN)) {
				if (!wlan_is_nan_allowed_on_freq(pdev,
				     res_msg[chan_enum].freq))
					res_msg[chan_enum].iface_mode_mask &=
						~(iface_mode);
				if (!res_msg[chan_enum].iface_mode_mask)
					reg_remove_freq(res_msg, chan_enum);
			} else {
				if (wlan_reg_is_freq_indoor(
					pdev, res_msg[chan_enum].freq) &&
					!include_indoor_channel) {
					res_msg[chan_enum].iface_mode_mask &=
							~(iface_mode);
					if (!res_msg[chan_enum].iface_mode_mask)
						reg_remove_freq(res_msg,
								chan_enum);
				}

				if (!(enable_srd_chan & srd_mask) &&
				    reg_is_etsi13_srd_chan_for_freq(
					pdev, res_msg[chan_enum].freq)) {
					res_msg[chan_enum].iface_mode_mask &=
						~(iface_mode);
					if (!res_msg[chan_enum].iface_mode_mask)
						reg_remove_freq(res_msg,
								chan_enum);
				}

				if (!dfs_master_capable &&
				    wlan_reg_is_dfs_for_freq(pdev,
				    res_msg[chan_enum].freq)) {
					res_msg[chan_enum].iface_mode_mask &=
						~(iface_mode);
					if (!res_msg[chan_enum].iface_mode_mask)
						reg_remove_freq(res_msg,
								chan_enum);
				}
			}
		}

		iface_mode_mask &= ~iface_mode;
	}

	return status;
}

/**
 * reg_get_usable_channel_no_filter() - Get usable channel with no filter mask
 * @pdev: Pointer to pdev
 * @req_msg: Request msg
 * @res_msg: Response msg
 * @chan_list: reg channel list
 * @count: no of usable channels
 *
 * Return: qdf status
 */
static QDF_STATUS
reg_get_usable_channel_no_filter(struct wlan_objmgr_pdev *pdev,
				 struct get_usable_chan_req_params req_msg,
				 struct get_usable_chan_res_params *res_msg,
				 struct regulatory_channel *chan_list,
				 int *count)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status =
	reg_skip_invalid_chan_freq(pdev, res_msg,
				   count, req_msg.iface_mode_mask);
	return status;
}

/**
 * reg_get_usable_channel_coex_filter() - Get usable channel with coex filter
 * @pdev: Pointer to pdev
 * @req_msg: Request msg
 * @res_msg: Response msg
 * @chan_list: reg channel list
 * @count: no of usable channels
 *
 * Return: qdf status
 */
static QDF_STATUS
reg_get_usable_channel_coex_filter(struct wlan_objmgr_pdev *pdev,
				   struct get_usable_chan_req_params req_msg,
				   struct get_usable_chan_res_params *res_msg,
				   struct regulatory_channel *chan_list,
				   int *count)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	enum channel_enum chan_enum;
	uint32_t i = 0;
	struct ch_avoid_freq_type freq_range;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("invalid psoc");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_alert("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	for (chan_enum = 0; chan_enum < *count; chan_enum++) {
		for (i = 0; i <
		    psoc_priv_obj->avoid_freq_list.ch_avoid_range_cnt; i++) {
			freq_range =
			psoc_priv_obj->avoid_freq_list.avoid_freq_range[i];

			if (freq_range.start_freq <=
			    chan_list[chan_enum].center_freq &&
			    freq_range.end_freq >=
			    chan_list[chan_enum].center_freq) {
				reg_debug("avoid freq %d",
					  chan_list[chan_enum].center_freq);
				reg_remove_freq(res_msg, chan_enum);
			}
		}
	}
	if (req_msg.iface_mode_mask & 1 << IFTYPE_AP ||
	    req_msg.iface_mode_mask & 1 << IFTYPE_P2P_GO ||
	    req_msg.iface_mode_mask & 1 << IFTYPE_NAN)
		status =
		reg_skip_invalid_chan_freq(pdev, res_msg, count,
					   req_msg.iface_mode_mask);
	return status;
}

/**
 * reg_calculate_mode_mask() - calculate valid mode mask
 * @iface_mode_mask: interface mode mask
 *
 * Return: Valid mode mask
 */
static uint32_t
reg_calculate_mode_mask(uint32_t iface_mode_mask)
{
	int mode_mask = 0;

	mode_mask = (iface_mode_mask & 1 << IFTYPE_STATION) |
		    (iface_mode_mask & 1 << IFTYPE_AP) |
		    (iface_mode_mask & 1 << IFTYPE_P2P_GO) |
		    (iface_mode_mask & 1 << IFTYPE_P2P_CLIENT) |
		    (iface_mode_mask & 1 << IFTYPE_P2P_DEVICE) |
		    (iface_mode_mask & 1 << IFTYPE_NAN);

	return mode_mask;
}

/**
 * reg_add_usable_channel_to_resp() - Add usable channels to resp structure
 * @pdev: Pointer to pdev
 * @res_msg: Response msg
 * @iface_mode_mask: interface mode mask
 * @chan_list: reg channel list
 * @count: no of usable channels
 *
 * Return: qdf status
 */
static QDF_STATUS
reg_add_usable_channel_to_resp(struct wlan_objmgr_pdev *pdev,
			       struct get_usable_chan_res_params *res_msg,
			       uint32_t iface_mode_mask,
			       struct regulatory_channel *chan_list,
			       int *count)
{
	enum channel_enum chan_enum;
	struct ch_params ch_params = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t mode_mask = 0;

	mode_mask = reg_calculate_mode_mask(iface_mode_mask);

	for (chan_enum = 0; chan_enum < *count &&
	     chan_enum < NUM_CHANNELS; chan_enum++) {
		ch_params.ch_width = CH_WIDTH_MAX;
		reg_set_channel_params_for_freq(
				pdev,
				chan_list[chan_enum].center_freq,
				chan_list[chan_enum].max_bw, &ch_params, true);

		res_msg[chan_enum].freq = chan_list[chan_enum].center_freq;
		res_msg[chan_enum].iface_mode_mask = mode_mask;
		if (!res_msg[chan_enum].iface_mode_mask) {
			reg_err("invalid iface mask");
			return QDF_STATUS_E_FAILURE;
		}
		res_msg[chan_enum].bw = ch_params.ch_width;
		res_msg[chan_enum].state = chan_list[chan_enum].state;
		if (ch_params.center_freq_seg0)
			res_msg[chan_enum].seg0_freq =
					ch_params.center_freq_seg0;
		if (ch_params.center_freq_seg1)
			res_msg[chan_enum].seg1_freq =
					ch_params.center_freq_seg1;
	}

	return status;
}

QDF_STATUS
wlan_reg_get_usable_channel(struct wlan_objmgr_pdev *pdev,
			    struct get_usable_chan_req_params req_msg,
			    struct get_usable_chan_res_params *res_msg,
			    uint32_t *usable_channels)
{
	struct regulatory_channel *chan_list;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	chan_list = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(*chan_list));

	if (!chan_list) {
		reg_err("chan_list invalid");
		return QDF_STATUS_E_FAILURE;
	}

	if ((req_msg.filter_mask & 1 << FILTER_CELLULAR_COEX) ||
	    (!(req_msg.filter_mask & 1 << FILTER_CELLULAR_COEX) &&
	     !(req_msg.filter_mask & 1 << FILTER_WLAN_CONCURRENCY))) {
		*usable_channels = reg_get_band_channel_list(pdev,
							     req_msg.band_mask,
							     chan_list);
		status =
		reg_add_usable_channel_to_resp(pdev, res_msg,
					       req_msg.iface_mode_mask,
					       chan_list, usable_channels);
		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mem_free(chan_list);
			return status;
		}
	}

	if (req_msg.filter_mask & 1 << FILTER_WLAN_CONCURRENCY)
		status =
		reg_get_usable_channel_con_filter(pdev, req_msg, res_msg,
						  usable_channels);

	if (req_msg.filter_mask & 1 << FILTER_CELLULAR_COEX)
		status =
		reg_get_usable_channel_coex_filter(pdev, req_msg, res_msg,
						   chan_list, usable_channels);
	if (!(req_msg.filter_mask & 1 << FILTER_CELLULAR_COEX) &&
	    !(req_msg.filter_mask & 1 << FILTER_WLAN_CONCURRENCY))
		status =
		reg_get_usable_channel_no_filter(pdev, req_msg, res_msg,
						 chan_list, usable_channels);
	reg_debug("usable chan count is %d", *usable_channels);

	qdf_mem_free(chan_list);
	return status;
}
#endif

/**
 * reg_get_nol_channel_state () - Get channel state from regulatory
 * and treat NOL channels as enabled channels
 * @pdev: Pointer to pdev
 * @freq: channel center frequency.
 *
 * Return: channel state
 */
static enum channel_state
reg_get_nol_channel_state(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum supported_6g_pwr_types in_6g_pwr_mode)
{
	enum channel_enum ch_idx;
	enum channel_state chan_state;

	ch_idx = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(ch_idx))
		return CHANNEL_STATE_INVALID;

	chan_state = reg_get_chan_state(pdev, ch_idx, in_6g_pwr_mode, false);

	return chan_state;
}

/**
 * reg_get_5g_bonded_chan_state()- Return the channel state for a
 * 5G or 6G channel frequency based on the bonded channel.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @bonded_chan_ptr: Pointer to bonded_channel_freq.
 * @input_punc_bitmap: input puncture bitmap
 *
 * Return: Channel State
 */
static enum channel_state
reg_get_5g_bonded_chan_state(struct wlan_objmgr_pdev *pdev,
			     uint16_t freq,
			     const struct bonded_channel_freq *bonded_chan_ptr,
			     enum supported_6g_pwr_types in_6g_pwr_mode,
			     uint16_t input_punc_bitmap)
{
	uint16_t chan_cfreq;
	enum channel_state chan_state = CHANNEL_STATE_INVALID;
	enum channel_state temp_chan_state;
	uint8_t i = 0;

	chan_cfreq =  bonded_chan_ptr->start_freq;
	while (chan_cfreq <= bonded_chan_ptr->end_freq) {
		if (!reg_is_chan_bit_punctured(input_punc_bitmap, i)) {
			temp_chan_state =
				reg_get_nol_channel_state(pdev, chan_cfreq,
							  in_6g_pwr_mode);
			if (temp_chan_state < chan_state)
				chan_state = temp_chan_state;
		}
		chan_cfreq = chan_cfreq + 20;
		i++;
	}

	return chan_state;
}

enum channel_state
reg_get_5g_chan_state(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
		      enum phy_ch_width bw,
		      enum supported_6g_pwr_types in_6g_pwr_mode,
		      uint16_t input_punc_bitmap)
{
	enum channel_enum ch_indx;
	enum channel_state chan_state;
	bool bw_enabled = false;
	const struct bonded_channel_freq *bonded_chan_ptr = NULL;
	uint16_t min_bw, max_bw;

	if (bw > CH_WIDTH_80P80MHZ) {
		reg_err_rl("bw passed is not good");
		return CHANNEL_STATE_INVALID;
	}

	if (bw == CH_WIDTH_20MHZ)
		return reg_get_nol_channel_state(pdev, freq, in_6g_pwr_mode);

	/* Fetch the bonded_chan_ptr for width greater than 20MHZ. */
	bonded_chan_ptr = reg_get_bonded_chan_entry(freq, bw, 0);

	if (!bonded_chan_ptr)
		return CHANNEL_STATE_INVALID;

	chan_state = reg_get_5g_bonded_chan_state(pdev, freq, bonded_chan_ptr,
						  in_6g_pwr_mode,
						  input_punc_bitmap);

	if ((chan_state == CHANNEL_STATE_INVALID) ||
	    (chan_state == CHANNEL_STATE_DISABLE))
		return chan_state;

	ch_indx = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(ch_indx))
		return CHANNEL_STATE_INVALID;

	if (reg_get_min_max_bw_reg_chan_list(pdev, ch_indx, in_6g_pwr_mode,
					     &min_bw, &max_bw)) {
		return CHANNEL_STATE_INVALID;
	}

	if (bw == CH_WIDTH_5MHZ)
		bw_enabled = true;
	else if (bw == CH_WIDTH_10MHZ)
		bw_enabled = (min_bw <= 10) &&
			(max_bw >= 10);
	else if (bw == CH_WIDTH_20MHZ)
		bw_enabled = (min_bw <= 20) &&
			(max_bw >= 20);
	else if (bw == CH_WIDTH_40MHZ)
		bw_enabled = (min_bw <= 40) &&
			(max_bw >= 40);
	else if (bw == CH_WIDTH_80MHZ)
		bw_enabled = (min_bw <= 80) &&
			(max_bw >= 80);
	else if (bw == CH_WIDTH_160MHZ)
		bw_enabled = (min_bw <= 160) &&
			(max_bw >= 160);
	else if (bw == CH_WIDTH_80P80MHZ)
		bw_enabled = (min_bw <= 80) &&
			(max_bw >= 80);

	if (bw_enabled)
		return chan_state;
	else
		return CHANNEL_STATE_DISABLE;
}

enum channel_state
reg_get_ch_state_based_on_nol_flag(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq,
				   struct ch_params *ch_params,
				   enum supported_6g_pwr_types
				   in_6g_pwr_mode,
				   bool treat_nol_chan_as_disabled)
{
	uint16_t input_punc_bitmap = reg_fetch_punc_bitmap(ch_params);

	if (treat_nol_chan_as_disabled)
		return wlan_reg_get_5g_bonded_channel_state_for_pwrmode(pdev,
									freq,
									ch_params,
									in_6g_pwr_mode);

	return reg_get_5g_chan_state(pdev, freq, ch_params->ch_width,
				     in_6g_pwr_mode,
				     input_punc_bitmap);
}

#ifdef WLAN_FEATURE_11BE
bool reg_is_ch_width_320(enum phy_ch_width ch_width)
{
	if (ch_width == CH_WIDTH_320MHZ)
		return true;
	return false;
}
#else
bool reg_is_ch_width_320(enum phy_ch_width ch_width)
{
	return false;
}
#endif

#ifdef CONFIG_REG_6G_PWRMODE
enum channel_state
reg_get_channel_state_for_pwrmode(struct wlan_objmgr_pdev *pdev,
				  qdf_freq_t freq,
				  enum supported_6g_pwr_types in_6g_pwr_type)
{
	enum channel_enum ch_idx;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_state state;

	ch_idx = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(ch_idx))
		return CHANNEL_STATE_INVALID;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return CHANNEL_STATE_INVALID;
	}

	state = reg_get_chan_state(pdev, ch_idx, in_6g_pwr_type, true);
	return state;
}
#endif

static uint32_t reg_get_channel_flags_for_freq(struct wlan_objmgr_pdev *pdev,
					       qdf_freq_t freq)
{
	enum channel_enum chan_enum;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	chan_enum = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return REGULATORY_CHAN_INVALID;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return REGULATORY_CHAN_INVALID;
	}

	return pdev_priv_obj->cur_chan_list[chan_enum].chan_flags;
}

#ifdef CONFIG_REG_CLIENT
enum channel_state reg_get_channel_state_from_secondary_list_for_freq(
						struct wlan_objmgr_pdev *pdev,
						qdf_freq_t freq)
{
	enum channel_enum ch_idx;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	ch_idx = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(ch_idx))
		return CHANNEL_STATE_INVALID;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return CHANNEL_STATE_INVALID;
	}

	return pdev_priv_obj->secondary_cur_chan_list[ch_idx].state;
}

static uint32_t reg_get_channel_flags_from_secondary_list_for_freq(
						struct wlan_objmgr_pdev *pdev,
						qdf_freq_t freq)
{
	enum channel_enum chan_enum;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	chan_enum = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err_rl("chan freq %u is not valid", freq);
		return REGULATORY_CHAN_INVALID;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return REGULATORY_CHAN_INVALID;
	}

	return pdev_priv_obj->secondary_cur_chan_list[chan_enum].chan_flags;
}

#ifdef CONFIG_BAND_6GHZ
/**
 * reg_get_psd_power() - Function to get PSD power for 6 GHz channel
 * @chan: Pointer to channel object
 * @is_psd: Pointer to whether it is PSD power
 *
 * Return: Channel PSD power value if it is PSD type.
 */
static uint16_t reg_get_psd_power(struct regulatory_channel *chan, bool *is_psd)
{
	if (is_psd)
		*is_psd = chan->psd_flag;
	return chan->psd_eirp;
}
#else
static uint16_t reg_get_psd_power(struct regulatory_channel *chan, bool *is_psd)
{
	if (is_psd)
		*is_psd = false;
	return 0;
}
#endif

QDF_STATUS
reg_get_channel_power_attr_from_secondary_list_for_freq(
		struct wlan_objmgr_pdev *pdev,
		qdf_freq_t freq, bool *is_psd,
		uint16_t *tx_power, uint16_t *psd_eirp, uint32_t *flags)
{
	enum channel_enum chan_enum;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *chan;

	if (!is_psd && !tx_power && !psd_eirp && !flags) {
		reg_err("all pointers null");
		return QDF_STATUS_E_FAILURE;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (chan_enum == INVALID_CHANNEL) {
		reg_err_rl("chan freq %u is not valid", freq);
		return QDF_STATUS_E_FAILURE;
	}

	chan = &pdev_priv_obj->secondary_cur_chan_list[chan_enum];

	if (chan->state == CHANNEL_STATE_DISABLE ||
	    chan->state == CHANNEL_STATE_INVALID) {
		reg_err_rl("invalid channel state %d", chan->state);
		return QDF_STATUS_E_FAILURE;
	}

	if (tx_power)
		*tx_power = chan->tx_power;
	if (psd_eirp)
		*psd_eirp = reg_get_psd_power(chan, is_psd);
	if (flags)
		*flags = chan->chan_flags;

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_BAND_6GHZ
QDF_STATUS
reg_decide_6ghz_power_within_bw_for_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t freq, enum phy_ch_width bw,
					 bool *is_psd, uint16_t *min_tx_power,
					 int16_t *min_psd_eirp,
					 enum reg_6g_ap_type *power_type)
{
	const struct bonded_channel_freq *bonded_chan_ptr = NULL;
	enum channel_state state;
	qdf_freq_t start_freq;
	uint16_t tx_power, psd_eirp;
	uint32_t chan_flags, min_chan_flags = 0;
	bool first_time = true;

	if (!reg_is_6ghz_chan_freq(freq))
		return QDF_STATUS_E_INVAL;

	if (!is_psd) {
		reg_err("is_psd pointer null");
		return QDF_STATUS_E_INVAL;
	}
	if (!min_tx_power) {
		reg_err("min_tx_power pointer null");
		return QDF_STATUS_E_INVAL;
	}
	if (!min_psd_eirp) {
		reg_err("min_psd_eirp pointer null");
		return QDF_STATUS_E_INVAL;
	}
	if (!power_type) {
		reg_err("power_type pointer null");
		return QDF_STATUS_E_INVAL;
	}

	state = reg_get_5g_bonded_channel_for_freq(pdev,
						   freq,
						   bw,
						   &bonded_chan_ptr);
	if (state != CHANNEL_STATE_ENABLE &&
	    state != CHANNEL_STATE_DFS) {
		reg_err("invalid channel state %d", state);
		return QDF_STATUS_E_INVAL;
	}

	if (bw <= CH_WIDTH_20MHZ) {
		if (reg_get_channel_power_attr_from_secondary_list_for_freq(
			pdev, freq, is_psd, &tx_power,
			&psd_eirp, &chan_flags) != QDF_STATUS_SUCCESS)
			return QDF_STATUS_E_INVAL;
		*min_psd_eirp = (int16_t)psd_eirp;
		*min_tx_power = tx_power;
		min_chan_flags = chan_flags;
		goto decide_power_type;
	}

	start_freq = bonded_chan_ptr->start_freq;
	while (start_freq <= bonded_chan_ptr->end_freq) {
		if (reg_get_channel_power_attr_from_secondary_list_for_freq(
			pdev, start_freq, is_psd, &tx_power,
			&psd_eirp, &chan_flags) != QDF_STATUS_SUCCESS)
			return QDF_STATUS_E_INVAL;

		if (first_time) {
			*min_psd_eirp = (int16_t)psd_eirp;
			*min_tx_power = tx_power;
			min_chan_flags = chan_flags;
			first_time = false;
		}
		if ((int16_t)psd_eirp < *min_psd_eirp)
			*min_psd_eirp = (int16_t)psd_eirp;
		if (tx_power < *min_tx_power)
			*min_tx_power = tx_power;
		min_chan_flags |= (chan_flags & REGULATORY_CHAN_AFC);
		min_chan_flags |= (chan_flags & REGULATORY_CHAN_INDOOR_ONLY);
		start_freq += 20;
	}

decide_power_type:
	if ((min_chan_flags & REGULATORY_CHAN_AFC) &&
	    (min_chan_flags & REGULATORY_CHAN_INDOOR_ONLY))
		*power_type = REG_INDOOR_AP;
	else if (min_chan_flags & REGULATORY_CHAN_AFC)
		*power_type = REG_STANDARD_POWER_AP;
	else if (min_chan_flags & REGULATORY_CHAN_INDOOR_ONLY)
		*power_type = REG_INDOOR_AP;
	else
		*power_type = REG_VERY_LOW_POWER_AP;

	return QDF_STATUS_SUCCESS;
}
#endif
#endif

/**
 * reg_get_5g_bonded_chan_array_for_freq()- Return the channel state for a
 * 5G or 6G channel frequency based on the bonded channel.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @bonded_chan_ptr: Pointer to bonded_channel_freq.
 *
 * Return: Channel State
 */
static enum channel_state
reg_get_5g_bonded_chan_array_for_freq(struct wlan_objmgr_pdev *pdev,
				      uint16_t freq,
				      const struct bonded_channel_freq *
				      bonded_chan_ptr)
{
	uint16_t chan_cfreq;
	enum channel_state chan_state = CHANNEL_STATE_INVALID;
	enum channel_state temp_chan_state;

	if (!bonded_chan_ptr) {
		reg_debug("bonded chan ptr is NULL");
		return chan_state;
	}

	chan_cfreq =  bonded_chan_ptr->start_freq;
	while (chan_cfreq <= bonded_chan_ptr->end_freq) {
		temp_chan_state = reg_get_channel_state_for_pwrmode(
						pdev, chan_cfreq,
						REG_CURRENT_PWR_MODE);
		if (temp_chan_state < chan_state)
			chan_state = temp_chan_state;
		chan_cfreq = chan_cfreq + 20;
	}

	return chan_state;
}

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * reg_get_5g_bonded_chan_array_for_pwrmode()- Return the channel state for a
 * 5G or 6G channel frequency based on the bonded channel.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @bonded_chan_ptr: Pointer to bonded_channel_freq.
 * @in_6g_pwr_type: Input 6g power mode which decides the which power mode based
 * channel list will be chosen.
 * @input_punc_bitmap: Input puncture bitmap
 *
 * Return: Channel State
 */
static enum channel_state
reg_get_5g_bonded_chan_array_for_pwrmode(struct wlan_objmgr_pdev *pdev,
					 uint16_t freq,
					 const struct bonded_channel_freq *
					 bonded_chan_ptr,
					 enum supported_6g_pwr_types
					 in_6g_pwr_type,
					 uint16_t input_punc_bitmap)
{
	uint16_t chan_cfreq;
	enum channel_state chan_state = CHANNEL_STATE_INVALID;
	enum channel_state temp_chan_state;
	uint8_t i = 0;

	if (!bonded_chan_ptr) {
		reg_debug("bonded chan ptr is NULL");
		return chan_state;
	}

	chan_cfreq =  bonded_chan_ptr->start_freq;
	while (chan_cfreq <= bonded_chan_ptr->end_freq) {
		if (!reg_is_chan_bit_punctured(input_punc_bitmap, i)) {
			temp_chan_state =
				reg_get_channel_state_for_pwrmode(pdev,
								  chan_cfreq,
								  in_6g_pwr_type);
			if (temp_chan_state < chan_state)
				chan_state = temp_chan_state;
		}
		chan_cfreq = chan_cfreq + 20;
		i++;
	}

	return chan_state;
}
#endif

#ifdef WLAN_FEATURE_11BE

QDF_STATUS reg_extract_puncture_by_bw(enum phy_ch_width ori_bw,
				      uint16_t ori_puncture_bitmap,
				      qdf_freq_t freq,
				      qdf_freq_t cen320_freq,
				      enum phy_ch_width new_bw,
				      uint16_t *new_puncture_bitmap)
{
	const struct bonded_channel_freq *ori_bonded_chan;
	const struct bonded_channel_freq *new_bonded_chan;
	uint16_t chan_cfreq;
	uint16_t new_bit;

	if (ori_bw < new_bw) {
		reg_err_rl("freq %d, ori bw %d can't be smaller than new bw %d",
			   freq, ori_bw, new_bw);
		return QDF_STATUS_E_FAILURE;
	}

	if (ori_bw == new_bw) {
		*new_puncture_bitmap = ori_puncture_bitmap;
		return QDF_STATUS_SUCCESS;
	}

	ori_bonded_chan = reg_get_bonded_chan_entry(freq, ori_bw, cen320_freq);
	new_bonded_chan = reg_get_bonded_chan_entry(freq, new_bw, 0);
	if (!ori_bonded_chan) {
		reg_err_rl("bonded chan fails, freq %d, ori bw %d, new bw %d",
			   freq, ori_bw, new_bw);
		return QDF_STATUS_E_FAILURE;
	}

	new_bit = 0;
	*new_puncture_bitmap = 0;
	chan_cfreq =  ori_bonded_chan->start_freq;
	while (chan_cfreq <= ori_bonded_chan->end_freq) {
		/*
		 * If the "new_bw" is 20, then new_bonded_chan = NULL and the
		 * output puncturing bitmap (*new_puncture_bitmap) as per spec
		 * should be 0. However, if the "ori_puncture_bitmap" has
		 * punctured the primary channel (the only channel in 20Mhz
		 * case), then the output "(*ori_puncture_bitmap) should contain
		 * the same so that the caller can recognize the error in the
		 * input pattern.
		 */
		if (freq == chan_cfreq ||
		    (new_bonded_chan &&
		     chan_cfreq >= new_bonded_chan->start_freq &&
		     chan_cfreq <= new_bonded_chan->end_freq)) {
			/* this frequency is in new bw */
			*new_puncture_bitmap |=
					(ori_puncture_bitmap & 1) << new_bit;
			new_bit++;
		}

		ori_puncture_bitmap >>= 1;
		chan_cfreq = chan_cfreq + BW_20_MHZ;
	}

	return QDF_STATUS_SUCCESS;
}

void reg_set_create_punc_bitmap(struct ch_params *ch_params,
				bool is_create_punc_bitmap)
{
	ch_params->is_create_punc_bitmap = is_create_punc_bitmap;
}

bool reg_is_punc_bitmap_valid(enum phy_ch_width bw, uint16_t puncture_bitmap)
{
	int i, num_bws;
	const uint16_t *bonded_puncture_bitmap = NULL;
	uint16_t array_size = 0;
	bool is_punc_bitmap_valid = false;

	num_bws = QDF_ARRAY_SIZE(bw_puncture_bitmap_pair_map);
	for (i = 0; i < num_bws; i++) {
		if (bw == bw_puncture_bitmap_pair_map[i].chwidth) {
			bonded_puncture_bitmap =
			    bw_puncture_bitmap_pair_map[i].puncture_bitmap_arr;
			array_size = bw_puncture_bitmap_pair_map[i].array_size;
			break;
		}
	}

	if (array_size && bonded_puncture_bitmap) {
		for (i = 0; i < array_size; i++) {
			if (puncture_bitmap == bonded_puncture_bitmap[i]) {
				is_punc_bitmap_valid = true;
				break;
			}
		}
	}

	return is_punc_bitmap_valid;
}

#ifdef QCA_DFS_BW_PUNCTURE
uint16_t reg_find_nearest_puncture_pattern(enum phy_ch_width bw,
					   uint16_t proposed_bitmap)
{
	int i, num_bws;
	const uint16_t *bonded_puncture_bitmap = NULL;
	uint16_t array_size;
	uint16_t final_bitmap;

	/* An input pattern = 0 will match any pattern
	 * Therefore, ignore '0' pattern and return '0', as '0' matches '0'.
	 */
	if (!proposed_bitmap)
		return 0;

	array_size = 0;
	final_bitmap = 0;

	num_bws = QDF_ARRAY_SIZE(bw_puncture_bitmap_pair_map);
	for (i = 0; i < num_bws; i++) {
		if (bw == bw_puncture_bitmap_pair_map[i].chwidth) {
			bonded_puncture_bitmap =
			    bw_puncture_bitmap_pair_map[i].puncture_bitmap_arr;
			array_size = bw_puncture_bitmap_pair_map[i].array_size;
			break;
		}
	}

	if (array_size && bonded_puncture_bitmap) {
		for (i = 0; i < array_size; i++) {
			uint16_t valid_bitmap = bonded_puncture_bitmap[i];

			if ((proposed_bitmap | valid_bitmap) == valid_bitmap) {
				final_bitmap = valid_bitmap;
				break;
			}
		}
	}

	return final_bitmap;
}
#endif /* QCA_DFS_BW_PUNCTURE */

/**
 * reg_update_5g_bonded_channel_state_punc_for_freq() - update channel state
 * with static puncturing feature
 * @pdev: pointer to pdev
 * @bonded_chan_ptr: Pointer to bonded_channel_freq.
 * @ch_params: pointer to ch_params
 * @chan_state: chan_state to be updated
 *
 * Return: void
 */
static void reg_update_5g_bonded_channel_state_punc_for_freq(
			struct wlan_objmgr_pdev *pdev,
			const struct bonded_channel_freq *bonded_chan_ptr,
			struct ch_params *ch_params,
			enum channel_state *chan_state)
{
	qdf_freq_t chan_cfreq;
	enum channel_state temp_chan_state;
	uint16_t puncture_bitmap = 0;
	int i = 0;
	enum channel_state update_state = CHANNEL_STATE_ENABLE;

	if (!pdev || !bonded_chan_ptr || !ch_params || !chan_state ||
	    !ch_params->is_create_punc_bitmap)
		return;

	chan_cfreq =  bonded_chan_ptr->start_freq;
	while (chan_cfreq <= bonded_chan_ptr->end_freq) {
		temp_chan_state = reg_get_channel_state_for_pwrmode(
							pdev,
							chan_cfreq,
							REG_CURRENT_PWR_MODE);
		if (!reg_is_state_allowed(temp_chan_state))
			puncture_bitmap |= BIT(i);
		/* Remember of any of the sub20 channel is a DFS channel */
		if (temp_chan_state == CHANNEL_STATE_DFS)
			update_state = CHANNEL_STATE_DFS;
		chan_cfreq = chan_cfreq + BW_20_MHZ;
		i++;
	}
	/* Validate puncture bitmap. Update channel state. */
	if (reg_is_punc_bitmap_valid(ch_params->ch_width, puncture_bitmap)) {
		*chan_state = update_state;
		ch_params->reg_punc_bitmap = puncture_bitmap;
	}
}

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * reg_update_5g_bonded_channel_state_punc_for_pwrmode() - update channel state
 * with static puncturing feature
 * @pdev: pointer to pdev
 * @bonded_chan_ptr: Pointer to bonded_channel_freq.
 * @ch_params: pointer to ch_params
 * @chan_state: chan_state to be updated
 *
 * Return: void
 */
static void reg_update_5g_bonded_channel_state_punc_for_pwrmode(
			struct wlan_objmgr_pdev *pdev,
			const struct bonded_channel_freq *bonded_chan_ptr,
			struct ch_params *ch_params,
			enum channel_state *chan_state,
			enum supported_6g_pwr_types in_6g_pwr_mode)
{
	qdf_freq_t chan_cfreq;
	enum channel_state temp_chan_state;
	uint16_t puncture_bitmap = 0;
	int i = 0;
	enum channel_state update_state = CHANNEL_STATE_ENABLE;

	if (!pdev || !bonded_chan_ptr || !ch_params || !chan_state ||
	    !ch_params->is_create_punc_bitmap)
		return;

	chan_cfreq =  bonded_chan_ptr->start_freq;
	while (chan_cfreq <= bonded_chan_ptr->end_freq) {
		temp_chan_state =
			reg_get_channel_state_for_pwrmode(pdev, chan_cfreq,
							  in_6g_pwr_mode);
		if (!reg_is_state_allowed(temp_chan_state))
			puncture_bitmap |= BIT(i);
		/* Remember of any of the sub20 channel is a DFS channel */
		if (temp_chan_state == CHANNEL_STATE_DFS)
			update_state = CHANNEL_STATE_DFS;
		chan_cfreq = chan_cfreq + BW_20_MHZ;
		i++;
	}
	/* Validate puncture bitmap. Update channel state. */
	if (reg_is_punc_bitmap_valid(ch_params->ch_width, puncture_bitmap)) {
		*chan_state = update_state;
		ch_params->reg_punc_bitmap = puncture_bitmap;
	}
}
#endif

#ifdef CONFIG_REG_CLIENT
QDF_STATUS reg_apply_puncture(struct wlan_objmgr_pdev *pdev,
			      uint16_t puncture_bitmap,
			      qdf_freq_t freq,
			      enum phy_ch_width bw,
			      qdf_freq_t cen320_freq)
{
	const struct bonded_channel_freq *bonded_chan;
	qdf_freq_t chan_cfreq;
	enum channel_enum chan_enum;
	struct regulatory_channel *mas_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	bool is_puncture;
	uint16_t i = 0;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("pdev reg obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mas_chan_list = pdev_priv_obj->mas_chan_list;
	if (!mas_chan_list) {
		reg_err_rl("mas chan_list is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	bonded_chan = reg_get_bonded_chan_entry(freq, bw, cen320_freq);
	if (!bonded_chan) {
		reg_err_rl("bonded chan fails, freq %d, bw %d, cen320_freq %d",
			   freq, bw, cen320_freq);
		return QDF_STATUS_E_FAILURE;
	}

	chan_cfreq = bonded_chan->start_freq;
	while (chan_cfreq <= bonded_chan->end_freq) {
		is_puncture = BIT(i) & puncture_bitmap;
		if (is_puncture) {
			chan_enum = reg_get_chan_enum_for_freq(chan_cfreq);
			if (reg_is_chan_enum_invalid(chan_enum)) {
				reg_debug_rl("Invalid chan enum %d", chan_enum);
				return QDF_STATUS_E_FAILURE;
			}
			mas_chan_list[chan_enum].is_static_punctured = true;
		}
		i++;
		chan_cfreq = chan_cfreq + BW_20_MHZ;
	}

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_remove_puncture(struct wlan_objmgr_pdev *pdev)
{
	enum channel_enum chan_enum;
	struct regulatory_channel *mas_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("pdev reg obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mas_chan_list = pdev_priv_obj->mas_chan_list;
	if (!mas_chan_list) {
		reg_err_rl("mas chan_list is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++)
		if (mas_chan_list[chan_enum].is_static_punctured)
			mas_chan_list[chan_enum].is_static_punctured = false;

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	return QDF_STATUS_SUCCESS;
}
#endif

#else
static void reg_update_5g_bonded_channel_state_punc_for_freq(
			struct wlan_objmgr_pdev *pdev,
			const struct bonded_channel_freq *bonded_chan_ptr,
			struct ch_params *ch_params,
			enum channel_state *chan_state)
{
}

static void reg_update_5g_bonded_channel_state_punc_for_pwrmode(
			struct wlan_objmgr_pdev *pdev,
			const struct bonded_channel_freq *bonded_chan_ptr,
			struct ch_params *ch_params,
			enum channel_state *chan_state,
			enum supported_6g_pwr_types in_6g_pwr_mode)
{
}
#endif

enum channel_state
reg_get_5g_bonded_channel_state_for_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t freq,
					 struct ch_params *ch_params)
{
	enum phy_ch_width bw;
	enum channel_enum ch_indx;
	enum channel_state chan_state;
	struct regulatory_channel *reg_channels;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	bool bw_enabled = false;
	const struct bonded_channel_freq *bonded_chan_ptr = NULL;

	if (!ch_params) {
		reg_err_rl("Invalid ch_params");
		return CHANNEL_STATE_INVALID;
	}
	bw = ch_params->ch_width;
	if (bw > CH_WIDTH_80P80MHZ) {
		reg_err_rl("bw (%d) passed is not good", bw);
		return CHANNEL_STATE_INVALID;
	}

	chan_state = reg_get_5g_bonded_channel_for_freq(pdev, freq, bw,
							&bonded_chan_ptr);

	reg_update_5g_bonded_channel_state_punc_for_freq(pdev,
							 bonded_chan_ptr,
							 ch_params,
							 &chan_state);

	if ((chan_state == CHANNEL_STATE_INVALID) ||
	    (chan_state == CHANNEL_STATE_DISABLE))
		return chan_state;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return CHANNEL_STATE_INVALID;
	}
	reg_channels = pdev_priv_obj->cur_chan_list;

	ch_indx = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(ch_indx))
		return CHANNEL_STATE_INVALID;
	if (bw == CH_WIDTH_5MHZ)
		bw_enabled = true;
	else if (bw == CH_WIDTH_10MHZ)
		bw_enabled = (reg_channels[ch_indx].min_bw <= 10) &&
			(reg_channels[ch_indx].max_bw >= 10);
	else if (bw == CH_WIDTH_20MHZ)
		bw_enabled = (reg_channels[ch_indx].min_bw <= 20) &&
			(reg_channels[ch_indx].max_bw >= 20);
	else if (bw == CH_WIDTH_40MHZ)
		bw_enabled = (reg_channels[ch_indx].min_bw <= 40) &&
			(reg_channels[ch_indx].max_bw >= 40);
	else if (bw == CH_WIDTH_80MHZ)
		bw_enabled = (reg_channels[ch_indx].min_bw <= 80) &&
			(reg_channels[ch_indx].max_bw >= 80);
	else if (bw == CH_WIDTH_160MHZ)
		bw_enabled = (reg_channels[ch_indx].min_bw <= 160) &&
			(reg_channels[ch_indx].max_bw >= 160);
	else if (bw == CH_WIDTH_80P80MHZ)
		bw_enabled = (reg_channels[ch_indx].min_bw <= 80) &&
			(reg_channels[ch_indx].max_bw >= 80);

	if (bw_enabled)
		return chan_state;
	return CHANNEL_STATE_DISABLE;
}

#ifdef CONFIG_REG_6G_PWRMODE
enum channel_state
reg_get_5g_bonded_channel_state_for_pwrmode(struct wlan_objmgr_pdev *pdev,
					    qdf_freq_t freq,
					    struct ch_params *ch_params,
					    enum supported_6g_pwr_types
					    in_6g_pwr_mode)
{
	enum phy_ch_width bw;
	enum channel_enum ch_indx;
	enum channel_state chan_state;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	bool bw_enabled = false;
	const struct bonded_channel_freq *bonded_chan_ptr = NULL;
	uint16_t min_bw, max_bw;
	uint16_t in_punc_bitmap = reg_fetch_punc_bitmap(ch_params);

	if (!ch_params) {
		reg_err_rl("Invalid ch_params");
		return CHANNEL_STATE_INVALID;
	}
	bw = ch_params->ch_width;
	if (bw > CH_WIDTH_80P80MHZ) {
		reg_err_rl("bw (%d) passed is not good", bw);
		return CHANNEL_STATE_INVALID;
	}

	chan_state = reg_get_5g_bonded_channel_for_pwrmode(pdev, freq, bw,
							   &bonded_chan_ptr,
							   in_6g_pwr_mode,
							   in_punc_bitmap);

	reg_update_5g_bonded_channel_state_punc_for_pwrmode(
						pdev, bonded_chan_ptr,
						ch_params, &chan_state,
						in_6g_pwr_mode);

	if ((chan_state == CHANNEL_STATE_INVALID) ||
	    (chan_state == CHANNEL_STATE_DISABLE))
		return chan_state;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg obj is NULL");
		return CHANNEL_STATE_INVALID;
	}

	ch_indx = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(ch_indx))
		return CHANNEL_STATE_INVALID;

	if (reg_get_min_max_bw_reg_chan_list(pdev, ch_indx, in_6g_pwr_mode,
					     &min_bw, &max_bw))
		return CHANNEL_STATE_INVALID;

	if (bw == CH_WIDTH_5MHZ)
		bw_enabled = true;
	else if (bw == CH_WIDTH_10MHZ)
		bw_enabled = (min_bw <= 10) &&
			(max_bw >= 10);
	else if (bw == CH_WIDTH_20MHZ)
		bw_enabled = (min_bw <= 20) &&
			(max_bw >= 20);
	else if (bw == CH_WIDTH_40MHZ)
		bw_enabled = (min_bw <= 40) &&
			(max_bw >= 40);
	else if (bw == CH_WIDTH_80MHZ)
		bw_enabled = (min_bw <= 80) &&
			(max_bw >= 80);
	else if (bw == CH_WIDTH_160MHZ)
		bw_enabled = (min_bw <= 160) &&
			(max_bw >= 160);
	else if (bw == CH_WIDTH_80P80MHZ)
		bw_enabled = (min_bw <= 80) &&
			(max_bw >= 80);

	if (bw_enabled)
		return chan_state;
	return CHANNEL_STATE_DISABLE;
}
#endif

enum channel_state
reg_get_2g_bonded_channel_state_for_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t oper_ch_freq,
					 qdf_freq_t sec_ch_freq,
					 enum phy_ch_width bw)
{
	enum channel_enum chan_idx;
	enum channel_state chan_state;
	struct regulatory_channel *reg_channels;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	bool bw_enabled = false;
	enum channel_state chan_state2 = CHANNEL_STATE_INVALID;

	if (bw > CH_WIDTH_40MHZ)
		return CHANNEL_STATE_INVALID;

	if (bw == CH_WIDTH_40MHZ) {
		if ((sec_ch_freq + 20 != oper_ch_freq) &&
		    (oper_ch_freq + 20 != sec_ch_freq))
			return CHANNEL_STATE_INVALID;
		chan_state2 =
		    reg_get_channel_state_for_pwrmode(pdev,
						      sec_ch_freq,
						      REG_CURRENT_PWR_MODE);
		if (chan_state2 == CHANNEL_STATE_INVALID)
			return chan_state2;
	}
	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return CHANNEL_STATE_INVALID;
	}

	reg_channels = pdev_priv_obj->cur_chan_list;

	chan_state = reg_get_channel_state_for_pwrmode(pdev,
						       oper_ch_freq,
						       REG_CURRENT_PWR_MODE);
	if (chan_state2 < chan_state)
		chan_state = chan_state2;

	if ((chan_state == CHANNEL_STATE_INVALID) ||
	    (chan_state == CHANNEL_STATE_DISABLE))
		return chan_state;

	chan_idx = reg_get_chan_enum_for_freq(oper_ch_freq);
	if (reg_is_chan_enum_invalid(chan_idx))
		return CHANNEL_STATE_INVALID;
	if (bw == CH_WIDTH_5MHZ)
		bw_enabled = true;
	else if (bw == CH_WIDTH_10MHZ)
		bw_enabled = (reg_channels[chan_idx].min_bw <= 10) &&
			(reg_channels[chan_idx].max_bw >= 10);
	else if (bw == CH_WIDTH_20MHZ)
		bw_enabled = (reg_channels[chan_idx].min_bw <= 20) &&
			(reg_channels[chan_idx].max_bw >= 20);
	else if (bw == CH_WIDTH_40MHZ)
		bw_enabled = (reg_channels[chan_idx].min_bw <= 40) &&
			(reg_channels[chan_idx].max_bw >= 40);

	if (bw_enabled)
		return chan_state;
	else
		return CHANNEL_STATE_DISABLE;

	return CHANNEL_STATE_ENABLE;
}

#ifdef WLAN_FEATURE_11BE

/**
 * reg_get_20mhz_channel_state_based_on_nol() - Get channel state of the
 * given 20MHZ channel. If the freq is in NOL/NOL history, it is considered
 * as enabled if "treat_nol_chan_as_disabled" is false, else the state is
 * considered as "disabled".
 * @pdev: Pointer to struct wlan_objmgr_pdev
 * @freq: Primary frequency
 * @treat_nol_chan_as_disabled: Flag to treat nol chan as enabled/disabled
 * @in_6g_pwr_type: Input 6g power type
 *
 * Return - Channel state
 */
static enum channel_state
reg_get_20mhz_channel_state_based_on_nol(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t freq,
					 bool treat_nol_chan_as_disabled,
					 enum supported_6g_pwr_types in_6g_pwr_type)
{
	if (treat_nol_chan_as_disabled)
		return  reg_get_channel_state_for_pwrmode(pdev, freq,
							  in_6g_pwr_type);
	return reg_get_nol_channel_state(pdev, freq,
					 in_6g_pwr_type);
}

/**
 * reg_get_320_bonded_chan_array() - Fetches a list of bonded channel pointers
 * for the given bonded channel array. If 320 band center is specified,
 * return the bonded channel pointer comprising of given band center else
 * return list of all available bonded channel pair.
 *
 * @pdev: Pointer to struct wlan_objmgr_pdev.
 * @freq: Input frequency in MHZ whose bonded channel pointer must be fetched.
 * @band_center_320: Channel center frequency of 320MHZ channel.
 * @bonded_chan_ar: Array of bonded channel list.
 * @array_size: Size of bonded channel array.
 * @bonded_chan_ptr: Pointer to hold the address of bonded_channel_freq index.
 *
 * Return: number of bonded channel arrays fetched.
 */

#define MAX_NUM_BONDED_PAIR 2
static uint8_t
reg_get_320_bonded_chan_array(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq,
			      qdf_freq_t band_center_320,
			      const struct bonded_channel_freq bonded_chan_ar[],
			      uint16_t array_size,
			      const struct bonded_channel_freq
			      *bonded_chan_ptr[])
{
	int i;
	uint8_t num_bonded_pairs = 0;

	/* Fetch all possible bonded channel pointers for the given freq */
	if (!band_center_320) {
		for (i = 0 ; i < array_size &&
		     num_bonded_pairs < MAX_NUM_BONDED_PAIR; i++) {
			if ((freq >= bonded_chan_ar[i].start_freq) &&
			    (freq <= bonded_chan_ar[i].end_freq)) {
				bonded_chan_ptr[num_bonded_pairs] =
					&bonded_chan_ar[i];
				num_bonded_pairs++;
			}
		}
	} else {
		/* Fetch the bonded channel pointer for the given band_center */
		for (i = 0; i < array_size; i++) {
			qdf_freq_t bandstart = bonded_chan_ar[i].start_freq;

			if (band_center_320 ==
			    reg_get_band_cen_from_bandstart(BW_320_MHZ,
							    bandstart)) {
				bonded_chan_ptr[num_bonded_pairs] =
					&bonded_chan_ar[i];
				num_bonded_pairs++;
				break;
			}
		}
	}
	return num_bonded_pairs;
}

#define SUB_CHAN_BW 20 /* 20 MHZ */
#define BW_160MHZ 160
#define  REG_IS_TOT_CHAN_BW_BELOW_160(_x, _y) \
	(reg_is_state_allowed((_x)) && (_y) < BW_160MHZ)

static inline qdf_freq_t
reg_get_endchan_cen_from_bandstart(qdf_freq_t band_start,
				   uint16_t bw)
{
	uint16_t left_edge_freq = band_start - BW_10_MHZ;

	return left_edge_freq + bw - BW_10_MHZ;
}
#endif

#ifdef WLAN_FEATURE_11BE
enum channel_state
reg_get_chan_state_for_320(struct wlan_objmgr_pdev *pdev,
			   uint16_t freq,
			   qdf_freq_t band_center_320,
			   enum phy_ch_width ch_width,
			   const struct bonded_channel_freq
			   **bonded_chan_ptr_ptr,
			   enum supported_6g_pwr_types in_6g_pwr_type,
			   bool treat_nol_chan_as_disabled,
			   uint16_t input_punc_bitmap)
{
	uint8_t num_bonded_pairs;
	uint16_t array_size =
		QDF_ARRAY_SIZE(bonded_chan_320mhz_list_freq);
	const struct bonded_channel_freq *bonded_ch_ptr[2] = {
		NULL, NULL};
	uint16_t punct_pattern;

	/* For now sending band center freq as 0 */
	num_bonded_pairs =
		reg_get_320_bonded_chan_array(pdev, freq, band_center_320,
					      bonded_chan_320mhz_list_freq,
					      array_size, bonded_ch_ptr);
	if (!num_bonded_pairs) {
		reg_info("No 320MHz bonded pair for freq %d", freq);
		return CHANNEL_STATE_INVALID;
	}
	/* Taking only first bonded pair */
	*bonded_chan_ptr_ptr = bonded_ch_ptr[0];

	return reg_get_320_bonded_channel_state_for_pwrmode(pdev, freq,
							    bonded_ch_ptr[0],
							    ch_width,
							    &punct_pattern,
							    in_6g_pwr_type,
							    treat_nol_chan_as_disabled,
							    input_punc_bitmap);
}
#endif

#ifdef WLAN_FEATURE_11BE
enum channel_state
reg_get_320_bonded_channel_state_for_pwrmode(struct wlan_objmgr_pdev *pdev,
					     qdf_freq_t freq,
					     const struct bonded_channel_freq
					     *bonded_chan_ptr,
					     enum phy_ch_width bw,
					     uint16_t *out_punc_bitmap,
					     enum supported_6g_pwr_types
					     in_6g_pwr_type,
					     bool treat_nol_chan_as_disabled,
					     uint16_t input_punc_bitmap)
{
	enum channel_state chan_state = CHANNEL_STATE_INVALID;
	enum channel_state temp_chan_state, prim_chan_state;
	uint16_t startchan_cfreq, endchan_cfreq;
	uint16_t max_cont_bw, i;
	enum channel_state update_state = CHANNEL_STATE_ENABLE;

	*out_punc_bitmap = ALL_SCHANS_PUNC;

	if (!bonded_chan_ptr)
		return chan_state;

	startchan_cfreq =  bonded_chan_ptr->start_freq;
	endchan_cfreq =
		reg_get_endchan_cen_from_bandstart(startchan_cfreq,
						   BW_320_MHZ);
	max_cont_bw = 0;
	i = 0;

	while (startchan_cfreq <= endchan_cfreq) {
		if (!reg_is_chan_bit_punctured(input_punc_bitmap, i)) {
			temp_chan_state =
				reg_get_20mhz_channel_state_based_on_nol(pdev,
									 startchan_cfreq,
									 treat_nol_chan_as_disabled,
									 in_6g_pwr_type);

			if (reg_is_state_allowed(temp_chan_state)) {
				max_cont_bw += SUB_CHAN_BW;
				*out_punc_bitmap &= ~BIT(i);
				/* Remember if sub20 channel is DFS channel */
				if (temp_chan_state == CHANNEL_STATE_DFS)
					update_state = CHANNEL_STATE_DFS;
			}

			if (temp_chan_state < chan_state)
				chan_state = temp_chan_state;
		}
		startchan_cfreq = startchan_cfreq + SUB_CHAN_BW;
		i++;
	}

	/* Validate puncture bitmap. Update channel state. */
	if (reg_is_punc_bitmap_valid(CH_WIDTH_320MHZ, *out_punc_bitmap)) {
		chan_state = update_state;
	}

	prim_chan_state =
		reg_get_20mhz_channel_state_based_on_nol(pdev, freq,
							 treat_nol_chan_as_disabled,
							 in_6g_pwr_type);

	/* After iterating through all the subchannels, if the final channel
	 * state is invalid/disable, it means all our subchannels are not
	 * valid and we could not find a 320 MHZ channel.
	 * If we have found a channel where the max width is:
	 * 1. Less than 160: there is no puncturing needed. Hence return
	 * the chan state as invalid. Or if the primary freq given is not
	 * supported by regulatory, the channel cannot be enabled as a
	 * punctured channel. So return channel state as invalid.
	 * 2. If greater than 160: Mark the invalid channels as punctured.
	 * and return channel state as ENABLE.
	 */
	if (REG_IS_TOT_CHAN_BW_BELOW_160(chan_state, max_cont_bw) ||
		!reg_is_state_allowed(prim_chan_state))
		return CHANNEL_STATE_INVALID;

	return chan_state;
}

static inline bool reg_is_pri_within_240mhz_chan(qdf_freq_t freq)
{
	return (freq >= CHAN_FREQ_5660 && freq <= CHAN_FREQ_5720);
}

/**
 * reg_fill_chan320mhz_seg0_center() - Fill the primary segment center
 * for a 320MHz channel in the given channel param. Primary segment center
 * of a 320MHZ is the 160MHZ segment center of the given freq.
 * @pdev: Pointer to struct wlan_objmgr_pdev.
 * @ch_param: channel params to be filled.
 * @freq: Input primary frequency in MHZ.
 *
 * Return: void.
 */
static void
reg_fill_chan320mhz_seg0_center(struct wlan_objmgr_pdev *pdev,
				struct ch_params *ch_param, qdf_freq_t freq)
{
	const struct bonded_channel_freq *t_bonded_ch_ptr;

	t_bonded_ch_ptr = reg_get_bonded_chan_entry(freq, CH_WIDTH_160MHZ, 0);
	if (t_bonded_ch_ptr) {
		ch_param->mhz_freq_seg0 =
			(t_bonded_ch_ptr->start_freq +
			 t_bonded_ch_ptr->end_freq) / 2;
		ch_param->center_freq_seg0 =
			reg_freq_to_chan(pdev,
					 ch_param->mhz_freq_seg0);
	} else {
		/**
		 * If we do not find a 160Mhz  bonded  pair, since it is
		 * for a 320Mhz channel we need to also see if we can find a
		 * pseudo 160Mhz channel for the special case of
		 * 5Ghz 240Mhz channel.
		 */
		if (reg_is_pri_within_240mhz_chan(freq)) {
			ch_param->mhz_freq_seg0 =
				PRIM_SEG_FREQ_CENTER_240MHZ_5G_CHAN;
			ch_param->center_freq_seg0 =
				PRIM_SEG_IEEE_CENTER_240MHZ_5G_CHAN;
		} else {
			ch_param->ch_width = CH_WIDTH_INVALID;
			reg_err("Cannot find 160Mhz centers for freq %d", freq);
		}
	}
}

/**
 * reg_fill_channel_list_for_320() - Fill 320MHZ channel list. If we
 * are unable to find a channel whose width is greater than 160MHZ and less
 * than 320 with the help of puncturing, using the given freq, set "update_bw"
 * variable to be true, lower the channel width and return to the caller.
 * The caller fetches a channel of reduced mode based on "update_bw" flag.
 *
 * If 320 band center is 0, return all the 320 channels
 * that match the primary frequency else return only channel
 * that matches 320 band center.
 *
 * @pdev: Pointer to struct wlan_objmgr_pdev.
 * @freq: Input frequency in MHZ.
 * @ch_width: Input channel width, if a channel of the given width is not
 * found, reduce the channel width to the next lower mode and pass it to the
 * caller.
 * @band_center_320: Center of 320MHZ channel.
 * @chan_list: Pointer to reg_channel_list to be filled.
 * @update_bw: Flag to hold if bw is updated.
 * @treat_nol_chan_as_disabled: Bool to treat NOL channels as disabled/enabled
 *
 * Return - None.
 */
static void
reg_fill_channel_list_for_320(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq,
			      enum phy_ch_width *in_ch_width,
			      qdf_freq_t band_center_320,
			      struct reg_channel_list *chan_list,
			      bool *update_bw,
			      bool treat_nol_chan_as_disabled)
{
	uint8_t num_bonded_pairs, i, num_ch_params;
	enum channel_state chan_state;
	uint16_t array_size = QDF_ARRAY_SIZE(bonded_chan_320mhz_list_freq);
	uint16_t out_punc_bitmap;
	uint16_t max_reg_bw;
	enum channel_enum chan_enum;
	const struct bonded_channel_freq *bonded_ch_ptr[2] = {NULL, NULL};
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	*update_bw = false;

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	/* Maximum bandwidth of the channel supported by regulatory for
	 * the given freq.
	 */
	max_reg_bw = pdev_priv_obj->cur_chan_list[chan_enum].max_bw;

	/* Regulatory does not support BW greater than 160.
	 * Try finding a channel in a lower mode.
	 */
	if (max_reg_bw <= BW_160MHZ) {
		*in_ch_width =  get_next_lower_bandwidth(*in_ch_width);
		*update_bw = true;
		return;
	}

	num_bonded_pairs =
		reg_get_320_bonded_chan_array(pdev, freq, band_center_320,
					      bonded_chan_320mhz_list_freq,
					      array_size,
					      bonded_ch_ptr);

	if (!num_bonded_pairs) {
		if (band_center_320) {
			reg_debug("No bonded pair for the given band_center\n");
			chan_list->num_ch_params = 0;
		} else {
			/* Could not find a 320 MHZ bonded channel pair,
			 * find a channel of lower BW.
			 */
			*in_ch_width =  get_next_lower_bandwidth(*in_ch_width);
			*update_bw = true;
		}
		return;
	}

	for (i = 0, num_ch_params = 0 ; i < num_bonded_pairs; i++) {
		/* Chan_state to hold the channel state of bonding
		 * pair of channels.
		 */
		uint16_t in_punc_bitmap =
			chan_list->chan_param[i].input_punc_bitmap;

		chan_state =
		    reg_get_320_bonded_channel_state_for_pwrmode(
						     pdev, freq,
						     bonded_ch_ptr[i],
						     *in_ch_width,
						     &out_punc_bitmap,
						     REG_CURRENT_PWR_MODE,
						     treat_nol_chan_as_disabled,
						     in_punc_bitmap);

		if (reg_is_state_allowed(chan_state)) {
			struct ch_params *t_chan_param =
			    &chan_list->chan_param[num_ch_params];

			t_chan_param->mhz_freq_seg1 =
				(bonded_ch_ptr[i]->start_freq +
				 bonded_ch_ptr[i]->end_freq) / 2;
			t_chan_param->center_freq_seg1 =
				reg_freq_to_chan(pdev,
						 t_chan_param->mhz_freq_seg1);
			t_chan_param->ch_width = *in_ch_width;
			t_chan_param->reg_punc_bitmap = out_punc_bitmap;

			reg_fill_chan320mhz_seg0_center(pdev,
							t_chan_param,
							freq);
			num_ch_params++;
			chan_list->num_ch_params = num_ch_params;
		}
	}

	/* The bonded pairs could not create any channels,
	 * lower the bandwidth to find a channel.
	 */
	if (!chan_list->num_ch_params) {
		*in_ch_width =  get_next_lower_bandwidth(*in_ch_width);
		*update_bw = true;
	}
}

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * reg_fill_channel_list_for_320_for_pwrmode() - Fill 320MHZ channel list. If we
 * are unable to find a channel whose width is greater than 160MHZ and less
 * than 320 with the help of puncturing, using the given freq, set "update_bw"
 * variable to be true, lower the channel width and return to the caller.
 * The caller fetches a channel of reduced mode based on "update_bw" flag.
 *
 * If 320 band center is 0, return all the 320 channels
 * that match the primary frequency else return only channel
 * that matches 320 band center.
 *
 * @pdev: Pointer to struct wlan_objmgr_pdev.
 * @freq: Input frequency in MHZ.
 * @ch_width: Input channel width, if a channel of the given width is not
 * found, reduce the channel width to the next lower mode and pass it to the
 * caller.
 * @band_center_320: Center of 320MHZ channel.
 * @chan_list: Pointer to reg_channel_list to be filled.
 * @update_bw: Flag to hold if bw is updated.
 * @in_6g_pwr_type: Input 6g power mode which decides the which power mode based
 * channel list will be chosen.
 * @treat_nol_chan_as_disabled: Bool to treat NOL channels as disabled/enabled
 *
 * Return - None.
 */
static void
reg_fill_channel_list_for_320_for_pwrmode(
			      struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq,
			      enum phy_ch_width *in_ch_width,
			      qdf_freq_t band_center_320,
			      struct reg_channel_list *chan_list,
			      bool *update_bw,
			      enum supported_6g_pwr_types in_6g_pwr_mode,
			      bool treat_nol_chan_as_disabled)
{
	uint8_t num_bonded_pairs, i, num_ch_params;
	enum channel_state chan_state;
	uint16_t array_size = QDF_ARRAY_SIZE(bonded_chan_320mhz_list_freq);
	uint16_t out_punc_bitmap;
	uint16_t max_reg_bw;
	enum channel_enum chan_enum;
	const struct bonded_channel_freq *bonded_ch_ptr[2] = {NULL, NULL};
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	*update_bw = false;

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	/* Maximum bandwidth of the channel supported by regulatory for
	 * the given freq.
	 */
	if (reg_get_min_max_bw_reg_chan_list(pdev, chan_enum, in_6g_pwr_mode,
					     NULL, &max_reg_bw))
		return;

	/* Regulatory does not support BW greater than 160.
	 * Try finding a channel in a lower mode.
	 */
	if (max_reg_bw <= BW_160MHZ) {
		*in_ch_width =  get_next_lower_bandwidth(*in_ch_width);
		*update_bw = true;
		return;
	}

	num_bonded_pairs =
		reg_get_320_bonded_chan_array(pdev, freq, band_center_320,
					      bonded_chan_320mhz_list_freq,
					      array_size,
					      bonded_ch_ptr);

	if (!num_bonded_pairs) {
		if (band_center_320) {
			reg_debug("No bonded pair for the given band_center\n");
			chan_list->num_ch_params = 0;
		} else {
			/* Could not find a 320 MHZ bonded channel pair,
			 * find a channel of lower BW.
			 */
			*in_ch_width =  get_next_lower_bandwidth(*in_ch_width);
			*update_bw = true;
		}
		return;
	}

	for (i = 0, num_ch_params = 0 ; i < num_bonded_pairs; i++) {
		uint16_t in_punc_bitmap =
			chan_list->chan_param[i].input_punc_bitmap;

		/* Chan_state to hold the channel state of bonding
		 * pair of channels.
		 */
		chan_state =
			reg_get_320_bonded_channel_state_for_pwrmode(pdev, freq,
								     bonded_ch_ptr[i],
								     *in_ch_width,
								     &out_punc_bitmap,
								     in_6g_pwr_mode,
								     treat_nol_chan_as_disabled,
								     in_punc_bitmap);

		if (reg_is_state_allowed(chan_state)) {
			struct ch_params *t_chan_param =
			    &chan_list->chan_param[num_ch_params];
			qdf_freq_t start_freq = bonded_ch_ptr[i]->start_freq;

			t_chan_param->mhz_freq_seg1 =
				reg_get_band_cen_from_bandstart(BW_320_MHZ,
								start_freq);
			t_chan_param->center_freq_seg1 =
				reg_freq_to_chan(pdev,
						 t_chan_param->mhz_freq_seg1);
			t_chan_param->ch_width = *in_ch_width;
			t_chan_param->reg_punc_bitmap = out_punc_bitmap;

			reg_fill_chan320mhz_seg0_center(pdev,
							t_chan_param,
							freq);
			num_ch_params++;
			chan_list->num_ch_params = num_ch_params;
		}
	}

	/* The bonded pairs could not create any channels,
	 * lower the bandwidth to find a channel.
	 */
	if (!chan_list->num_ch_params) {
		*in_ch_width =  get_next_lower_bandwidth(*in_ch_width);
		*update_bw = true;
	}
}
#endif

/**
 * reg_fill_pre320mhz_channel() - Fill channel params for channel width
 * less than 320.
 * @pdev: Pointer to struct wlan_objmgr_pdev
 * @chan_list: Pointer to struct reg_channel_list
 * @ch_width: Channel width
 * @freq: Center frequency of the primary channel in MHz
 * @sec_ch_2g_freq:  Secondary 2G channel frequency in MHZ
 * @treat_nol_chan_as_disabled: Bool to treat NOL channels as
 * disabled/enabled
 */
static void
reg_fill_pre320mhz_channel(struct wlan_objmgr_pdev *pdev,
			   struct reg_channel_list *chan_list,
			   enum phy_ch_width ch_width,
			   qdf_freq_t freq,
			   qdf_freq_t sec_ch_2g_freq,
			   bool treat_nol_chan_as_disabled)
{
	chan_list->num_ch_params = 1;
	chan_list->chan_param[0].ch_width = ch_width;
	chan_list->chan_param[0].reg_punc_bitmap = NO_SCHANS_PUNC;
	reg_set_channel_params_for_freq(pdev, freq, sec_ch_2g_freq,
					&chan_list->chan_param[0],
					treat_nol_chan_as_disabled);
}

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * reg_fill_pre320mhz_channel_for_pwrmode() - Fill channel params for channel
 * width less than 320.
 * @pdev: Pointer to struct wlan_objmgr_pdev
 * @chan_list: Pointer to struct reg_channel_list
 * @ch_width: Channel width
 * @freq: Center frequency of the primary channel in MHz
 * @sec_ch_2g_freq:  Secondary 2G channel frequency in MHZ
 * @in_6g_pwr_type: Input 6g power mode which decides the which power mode based
 * channel list will be chosen.
 * @treat_nol_chan_as_disabled: Bool to consider nol chan as enabled/disabled
 */
static void
reg_fill_pre320mhz_channel_for_pwrmode(
			   struct wlan_objmgr_pdev *pdev,
			   struct reg_channel_list *chan_list,
			   enum phy_ch_width ch_width,
			   qdf_freq_t freq,
			   qdf_freq_t sec_ch_2g_freq,
			   enum supported_6g_pwr_types in_6g_pwr_mode,
			   bool treat_nol_chan_as_disabled)
{
	chan_list->num_ch_params = 1;
	chan_list->chan_param[0].ch_width = ch_width;
	chan_list->chan_param[0].reg_punc_bitmap = NO_SCHANS_PUNC;
	reg_set_channel_params_for_pwrmode(pdev, freq, sec_ch_2g_freq,
					   &chan_list->chan_param[0],
					   in_6g_pwr_mode,
					   treat_nol_chan_as_disabled);
}
#endif

void
reg_fill_channel_list(struct wlan_objmgr_pdev *pdev,
		      qdf_freq_t freq,
		      qdf_freq_t sec_ch_2g_freq,
		      enum phy_ch_width in_ch_width,
		      qdf_freq_t band_center_320,
		      struct reg_channel_list *chan_list,
		      bool treat_nol_chan_as_disabled)
{
	bool update_bw;

	if (!chan_list) {
		reg_err("channel params is NULL");
		return;
	}

	if (in_ch_width >= CH_WIDTH_MAX)
		in_ch_width = CH_WIDTH_320MHZ;

	if (in_ch_width == CH_WIDTH_320MHZ) {
		update_bw = 0;
		reg_fill_channel_list_for_320(pdev, freq, &in_ch_width,
					      band_center_320, chan_list,
					      &update_bw,
					      treat_nol_chan_as_disabled);
		if (!update_bw)
			return;
	}

	/* A 320 channel is not available (or) user has not requested
	 * for a 320MHZ channel, look for channels in lower modes,
	 * reg_set_5g_channel_params_for_freq() finds for the
	 * next available mode and fills ch_params.
	 */
	reg_fill_pre320mhz_channel(pdev, chan_list, in_ch_width, freq,
				   sec_ch_2g_freq,
				   treat_nol_chan_as_disabled);
}

#ifdef CONFIG_REG_6G_PWRMODE
void
reg_fill_channel_list_for_pwrmode(struct wlan_objmgr_pdev *pdev,
				  qdf_freq_t freq,
				  qdf_freq_t sec_ch_2g_freq,
				  enum phy_ch_width in_ch_width,
				  qdf_freq_t band_center_320,
				  struct reg_channel_list *chan_list,
				  enum supported_6g_pwr_types in_6g_pwr_mode,
				  bool treat_nol_chan_as_disabled)
{
	bool update_bw;

	if (!chan_list) {
		reg_err("channel params is NULL");
		return;
	}

	if (in_ch_width >= CH_WIDTH_MAX)
		in_ch_width = CH_WIDTH_320MHZ;

	if (in_ch_width == CH_WIDTH_320MHZ) {
		update_bw = 0;
		reg_fill_channel_list_for_320_for_pwrmode(
					      pdev, freq, &in_ch_width,
					      band_center_320, chan_list,
					      &update_bw, in_6g_pwr_mode,
					      treat_nol_chan_as_disabled);
		if (!update_bw)
			return;
	}

	/* A 320 channel is not available (or) user has not requested
	 * for a 320MHZ channel, look for channels in lower modes,
	 * reg_set_5g_channel_params_for_freq() finds for the
	 * next available mode and fills ch_params.
	 */
	reg_fill_pre320mhz_channel_for_pwrmode(
				   pdev, chan_list, in_ch_width, freq,
				   sec_ch_2g_freq, in_6g_pwr_mode,
				   treat_nol_chan_as_disabled);
}
#endif
#endif

enum channel_state
reg_get_5g_bonded_channel_for_freq(struct wlan_objmgr_pdev *pdev,
				   uint16_t freq,
				   enum phy_ch_width ch_width,
				   const struct bonded_channel_freq
				   **bonded_chan_ptr_ptr)
{
	if (ch_width == CH_WIDTH_20MHZ)
		return reg_get_channel_state_for_pwrmode(pdev, freq,
							 REG_CURRENT_PWR_MODE);

	if (reg_is_ch_width_320(ch_width)) {
		return reg_get_chan_state_for_320(pdev, freq, 0,
						  ch_width,
						  bonded_chan_ptr_ptr,
						  REG_CURRENT_PWR_MODE,
						  true,
						  NO_SCHANS_PUNC);
	} else {
		*bonded_chan_ptr_ptr = reg_get_bonded_chan_entry(freq,
								 ch_width, 0);
		if (!(*bonded_chan_ptr_ptr))
			return CHANNEL_STATE_INVALID;

		return reg_get_5g_bonded_chan_array_for_freq(
							pdev, freq,
							*bonded_chan_ptr_ptr);
	}
}

#ifdef CONFIG_REG_6G_PWRMODE
enum channel_state
reg_get_5g_bonded_channel_for_pwrmode(struct wlan_objmgr_pdev *pdev,
				      uint16_t freq,
				      enum phy_ch_width ch_width,
				      const struct bonded_channel_freq
				      **bonded_chan_ptr_ptr,
				      enum supported_6g_pwr_types
				      in_6g_pwr_mode,
				      uint16_t input_punc_bitmap)
{
	if (ch_width == CH_WIDTH_20MHZ)
		return reg_get_channel_state_for_pwrmode(pdev, freq,
						      in_6g_pwr_mode);

	if (reg_is_ch_width_320(ch_width))
		return reg_get_chan_state_for_320(pdev, freq, 0,
						  ch_width,
						  bonded_chan_ptr_ptr,
						  in_6g_pwr_mode, true,
						  input_punc_bitmap);
	/* Fetch the bonded_chan_ptr for width greater than 20MHZ. */
	*bonded_chan_ptr_ptr = reg_get_bonded_chan_entry(freq, ch_width, 0);

	if (!(*bonded_chan_ptr_ptr)) {
		reg_debug_rl("bonded_chan_ptr_ptr is NULL");
		return CHANNEL_STATE_INVALID;
	}

	return reg_get_5g_bonded_chan_array_for_pwrmode(pdev, freq,
						     *bonded_chan_ptr_ptr,
						     in_6g_pwr_mode,
						     input_punc_bitmap);
}
#endif

#ifdef CONFIG_REG_6G_PWRMODE
/**
 * reg_set_5g_channel_params_for_pwrmode()- Set channel parameters like center
 * frequency for a bonded channel state. Also return the maximum bandwidth
 * supported by the channel.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * ch_params: Pointer to ch_params.
 * @in_6g_pwr_type: Input 6g power mode which decides the which power mode based
 * channel list will be chosen.
 * @treat_nol_chan_as_disabled: Bool to treat NOL channels as disabled/enabled
 *
 * Return: void
 */
static void reg_set_5g_channel_params_for_pwrmode(
					       struct wlan_objmgr_pdev *pdev,
					       uint16_t freq,
					       struct ch_params *ch_params,
					       enum supported_6g_pwr_types
					       in_6g_pwr_type,
					       bool treat_nol_chan_as_disabled)
{
	/*
	 * Set channel parameters like center frequency for a bonded channel
	 * state. Also return the maximum bandwidth supported by the channel.
	 */

	enum channel_state chan_state = CHANNEL_STATE_ENABLE;
	enum channel_state chan_state2 = CHANNEL_STATE_ENABLE;
	const struct bonded_channel_freq *bonded_chan_ptr = NULL;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_enum chan_enum, sec_5g_chan_enum;
	uint16_t bw_80 = 0;
	uint16_t max_bw, sec_5g_freq_max_bw = 0;
	uint16_t in_punc_bitmap = reg_fetch_punc_bitmap(ch_params);

	if (!ch_params) {
		reg_err("ch_params is NULL");
		return;
	}

	chan_enum = reg_get_chan_enum_for_freq(freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	if (ch_params->ch_width >= CH_WIDTH_MAX) {
		if (ch_params->mhz_freq_seg1 != 0)
			ch_params->ch_width = CH_WIDTH_80P80MHZ;
		else
			ch_params->ch_width = CH_WIDTH_160MHZ;
	}

	if (reg_get_min_max_bw_reg_chan_list(pdev, chan_enum, in_6g_pwr_type,
					     NULL, &max_bw))
		return;

	bw_80 = reg_get_bw_value(CH_WIDTH_80MHZ);

	if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
		sec_5g_chan_enum =
			reg_get_chan_enum_for_freq(ch_params->mhz_freq_seg1 -
					NEAREST_20MHZ_CHAN_FREQ_OFFSET);
		if (reg_is_chan_enum_invalid(sec_5g_chan_enum)) {
			reg_err("secondary channel freq is not valid");
			return;
		}

		if (reg_get_min_max_bw_reg_chan_list(pdev, sec_5g_chan_enum,
						     in_6g_pwr_type,
						     NULL, &sec_5g_freq_max_bw))
			return;
	}

	while (ch_params->ch_width != CH_WIDTH_INVALID) {
		if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
			if ((max_bw < bw_80) || (sec_5g_freq_max_bw < bw_80))
				goto update_bw;
		} else if (max_bw < reg_get_bw_value(ch_params->ch_width)) {
			goto update_bw;
		}

		bonded_chan_ptr = NULL;
		chan_state = reg_get_5g_bonded_channel_for_pwrmode(
				pdev, freq, ch_params->ch_width,
				&bonded_chan_ptr, in_6g_pwr_type,
				in_punc_bitmap);
		chan_state =
			reg_get_ch_state_based_on_nol_flag(pdev, freq,
							   ch_params,
							   in_6g_pwr_type,
							   treat_nol_chan_as_disabled);

		if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
			struct ch_params temp_ch_params = {0};

			temp_ch_params.ch_width = CH_WIDTH_80MHZ;
			/* Puncturing patter is not needed for 80+80 */
			reg_set_create_punc_bitmap(&temp_ch_params, false);
			chan_state2 =
				reg_get_ch_state_based_on_nol_flag(pdev,
								   ch_params->mhz_freq_seg1 -
								   NEAREST_20MHZ_CHAN_FREQ_OFFSET,
								   &temp_ch_params, in_6g_pwr_type,
								   treat_nol_chan_as_disabled);
			chan_state = reg_combine_channel_states(
					chan_state, chan_state2);
		}

		if ((chan_state != CHANNEL_STATE_ENABLE) &&
		    (chan_state != CHANNEL_STATE_DFS))
			goto update_bw;
		if (ch_params->ch_width <= CH_WIDTH_20MHZ) {
			ch_params->sec_ch_offset = NO_SEC_CH;
			ch_params->mhz_freq_seg0 = freq;
				ch_params->center_freq_seg0 =
				reg_freq_to_chan(pdev,
						 ch_params->mhz_freq_seg0);
			break;
		} else if (ch_params->ch_width >= CH_WIDTH_40MHZ) {
			const struct bonded_channel_freq *bonded_chan_ptr2;

			bonded_chan_ptr2 =
					reg_get_bonded_chan_entry(
								freq,
								CH_WIDTH_40MHZ,
								0);

			if (!bonded_chan_ptr || !bonded_chan_ptr2)
				goto update_bw;
			if (freq == bonded_chan_ptr2->start_freq)
				ch_params->sec_ch_offset = LOW_PRIMARY_CH;
			else
				ch_params->sec_ch_offset = HIGH_PRIMARY_CH;

			ch_params->mhz_freq_seg0 =
				(bonded_chan_ptr->start_freq +
				 bonded_chan_ptr->end_freq) / 2;
				ch_params->center_freq_seg0 =
				reg_freq_to_chan(pdev,
						 ch_params->mhz_freq_seg0);
			break;
		}
update_bw:
		ch_params->ch_width =
		    get_next_lower_bandwidth(ch_params->ch_width);
	}

	if (ch_params->ch_width == CH_WIDTH_160MHZ) {
		ch_params->mhz_freq_seg1 = ch_params->mhz_freq_seg0;
			ch_params->center_freq_seg1 =
				reg_freq_to_chan(pdev,
						 ch_params->mhz_freq_seg1);

		chan_state = reg_get_5g_bonded_channel_for_pwrmode(
				pdev, freq, CH_WIDTH_80MHZ, &bonded_chan_ptr,
				in_6g_pwr_type,
				in_punc_bitmap);
		if (bonded_chan_ptr) {
			ch_params->mhz_freq_seg0 =
				(bonded_chan_ptr->start_freq +
				 bonded_chan_ptr->end_freq) / 2;
				ch_params->center_freq_seg0 =
				reg_freq_to_chan(pdev,
						 ch_params->mhz_freq_seg0);
		}
	}

	/* Overwrite mhz_freq_seg1 to 0 for non 160 and 80+80 width */
	if (!(ch_params->ch_width == CH_WIDTH_160MHZ ||
	      ch_params->ch_width == CH_WIDTH_80P80MHZ)) {
		ch_params->mhz_freq_seg1 = 0;
		ch_params->center_freq_seg1 = 0;
	}
}
#endif

#ifdef CONFIG_REG_CLIENT
static qdf_freq_t reg_get_sec_ch_2g_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t primary_freq)
{
	qdf_freq_t sec_ch_2g_freq = 0;

	if (primary_freq >= TWOG_CHAN_1_IN_MHZ &&
	    primary_freq <= TWOG_CHAN_5_IN_MHZ)
		sec_ch_2g_freq = primary_freq + HT40_SEC_OFFSET;
	else if (primary_freq >= TWOG_CHAN_6_IN_MHZ &&
		 primary_freq <= TWOG_CHAN_13_IN_MHZ)
		sec_ch_2g_freq = primary_freq - HT40_SEC_OFFSET;

	return sec_ch_2g_freq;
}
#else
static qdf_freq_t reg_get_sec_ch_2g_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t primary_freq)
{
	qdf_freq_t sec_ch_2g_freq;

	if (primary_freq < TWOG_CHAN_1_IN_MHZ ||
	    primary_freq > TWOG_CHAN_13_IN_MHZ)
		return 0;

	sec_ch_2g_freq = primary_freq + HT40_SEC_OFFSET;

	/* For 2G primary frequencies > 2452 (IEEE9), return HT40-. */
	if (primary_freq > TWOG_CHAN_9_IN_MHZ)
		sec_ch_2g_freq = primary_freq - HT40_SEC_OFFSET;

	/*
	 * For 2G primary frequencies <= 2452 (IEEE9), return HT40+ if
	 * the secondary is available, else return HT40-.
	 */
	else if (!reg_is_freq_present_in_cur_chan_list(pdev, sec_ch_2g_freq))
		sec_ch_2g_freq = primary_freq - HT40_SEC_OFFSET;

	return sec_ch_2g_freq;
}
#endif

void reg_set_2g_channel_params_for_freq(struct wlan_objmgr_pdev *pdev,
					uint16_t oper_freq,
					struct ch_params *ch_params,
					uint16_t sec_ch_2g_freq)
{
	enum channel_state chan_state = CHANNEL_STATE_ENABLE;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_enum chan_enum;
	uint16_t max_bw;

	chan_enum = reg_get_chan_enum_for_freq(oper_freq);
	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("chan freq is not valid");
		return;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	if (ch_params->ch_width >= CH_WIDTH_MAX)
		ch_params->ch_width = CH_WIDTH_40MHZ;
	if ((reg_get_bw_value(ch_params->ch_width) > 20) && !sec_ch_2g_freq)
		sec_ch_2g_freq = reg_get_sec_ch_2g_freq(pdev, oper_freq);

	max_bw = pdev_priv_obj->cur_chan_list[chan_enum].max_bw;

	while (ch_params->ch_width != CH_WIDTH_INVALID) {
		if (max_bw < reg_get_bw_value(ch_params->ch_width))
			goto update_bw;

		chan_state =
		reg_get_2g_bonded_channel_state_for_freq(pdev, oper_freq,
							 sec_ch_2g_freq,
							 ch_params->ch_width);
		if ((chan_state == CHANNEL_STATE_ENABLE) ||
		    (chan_state == CHANNEL_STATE_DFS)) {
			if (ch_params->ch_width == CH_WIDTH_40MHZ) {
				if (oper_freq < sec_ch_2g_freq)
					ch_params->sec_ch_offset =
						LOW_PRIMARY_CH;
				else
					ch_params->sec_ch_offset =
						HIGH_PRIMARY_CH;
				ch_params->mhz_freq_seg0 =
					(oper_freq + sec_ch_2g_freq) / 2;
				if (ch_params->mhz_freq_seg0 ==
						TWOG_CHAN_14_IN_MHZ)
					ch_params->center_freq_seg0 = 14;
				else
					ch_params->center_freq_seg0 =
						(ch_params->mhz_freq_seg0 -
						 TWOG_STARTING_FREQ) /
						FREQ_TO_CHAN_SCALE;
			} else {
				ch_params->sec_ch_offset = NO_SEC_CH;
				ch_params->mhz_freq_seg0 = oper_freq;
				if (ch_params->mhz_freq_seg0 ==
						TWOG_CHAN_14_IN_MHZ)
					ch_params->center_freq_seg0 = 14;
				else
					ch_params->center_freq_seg0 =
						(ch_params->mhz_freq_seg0 -
						 TWOG_STARTING_FREQ) /
						FREQ_TO_CHAN_SCALE;
			}
			break;
		}
update_bw:
		ch_params->ch_width =
		    get_next_lower_bandwidth(ch_params->ch_width);
	}
	/* Overwrite mhz_freq_seg1 and center_freq_seg1 to 0 for 2.4 Ghz */
	ch_params->mhz_freq_seg1 = 0;
	ch_params->center_freq_seg1 = 0;
}

#ifdef WLAN_FEATURE_11BE
static void reg_copy_ch_params(struct ch_params *ch_params,
			       struct reg_channel_list chan_list)
{
	ch_params->center_freq_seg0 = chan_list.chan_param[0].center_freq_seg0;
	ch_params->center_freq_seg1 = chan_list.chan_param[0].center_freq_seg1;
	ch_params->mhz_freq_seg0 = chan_list.chan_param[0].mhz_freq_seg0;
	ch_params->mhz_freq_seg1 = chan_list.chan_param[0].mhz_freq_seg1;
	ch_params->ch_width = chan_list.chan_param[0].ch_width;
	ch_params->sec_ch_offset = chan_list.chan_param[0].sec_ch_offset;
	ch_params->reg_punc_bitmap = chan_list.chan_param[0].reg_punc_bitmap;
}

void reg_set_channel_params_for_freq(struct wlan_objmgr_pdev *pdev,
				     qdf_freq_t freq,
				     qdf_freq_t sec_ch_2g_freq,
				     struct ch_params *ch_params,
				     bool treat_nol_chan_as_disabled)
{
	if (reg_is_5ghz_ch_freq(freq) || reg_is_6ghz_chan_freq(freq)) {
		if (reg_is_ch_width_320(ch_params->ch_width)) {
			struct reg_channel_list chan_list;

			qdf_mem_zero(&chan_list, sizeof(chan_list));
			/* For now sending center freq as 0 */
			reg_fill_channel_list(pdev, freq, sec_ch_2g_freq,
					      ch_params->ch_width, 0,
					      &chan_list,
					      treat_nol_chan_as_disabled);
			reg_copy_ch_params(ch_params, chan_list);
		} else {
			reg_set_5g_channel_params_for_pwrmode(
						pdev, freq,
						ch_params,
						REG_CURRENT_PWR_MODE,
						treat_nol_chan_as_disabled);
		}
	} else if  (reg_is_24ghz_ch_freq(freq)) {
		reg_set_2g_channel_params_for_freq(pdev, freq, ch_params,
						   sec_ch_2g_freq);
	}
}
#else /* WLAN_FEATURE_11BE */
void reg_set_channel_params_for_freq(struct wlan_objmgr_pdev *pdev,
				     qdf_freq_t freq,
				     qdf_freq_t sec_ch_2g_freq,
				     struct ch_params *ch_params,
				     bool treat_nol_chan_as_disabled)
{
	if (reg_is_5ghz_ch_freq(freq) || reg_is_6ghz_chan_freq(freq))
		reg_set_5g_channel_params_for_pwrmode(
						pdev, freq, ch_params,
						REG_CURRENT_PWR_MODE,
						treat_nol_chan_as_disabled);
	else if  (reg_is_24ghz_ch_freq(freq))
		reg_set_2g_channel_params_for_freq(pdev, freq, ch_params,
						   sec_ch_2g_freq);
}
#endif /* WLAN_FEATURE_11BE */

#ifdef CONFIG_REG_6G_PWRMODE
#ifdef WLAN_FEATURE_11BE
void
reg_set_channel_params_for_pwrmode(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq,
				   qdf_freq_t sec_ch_2g_freq,
				   struct ch_params *ch_params,
				   enum supported_6g_pwr_types in_6g_pwr_mode,
				   bool is_treat_nol_dis)
{
	if (reg_is_5ghz_ch_freq(freq) || reg_is_6ghz_chan_freq(freq)) {
		if (reg_is_ch_width_320(ch_params->ch_width)) {
			struct reg_channel_list chan_list;
			uint8_t i;

			qdf_mem_zero(&chan_list, sizeof(chan_list));

			for (i = 0; i < MAX_NUM_CHAN_PARAM; i++) {
				chan_list.chan_param[i].input_punc_bitmap =
					ch_params->input_punc_bitmap;
			}
			reg_fill_channel_list_for_pwrmode(pdev, freq,
							  sec_ch_2g_freq,
							  ch_params->ch_width,
							  ch_params->mhz_freq_seg1,
							  &chan_list,
							  in_6g_pwr_mode,
							  is_treat_nol_dis);
			reg_copy_ch_params(ch_params, chan_list);
		} else {
			reg_set_5g_channel_params_for_pwrmode(pdev, freq,
							      ch_params,
							      in_6g_pwr_mode,
							      is_treat_nol_dis);
		}
	} else if  (reg_is_24ghz_ch_freq(freq)) {
		reg_set_2g_channel_params_for_freq(pdev, freq, ch_params,
						   sec_ch_2g_freq);
	}
}
#else
void
reg_set_channel_params_for_pwrmode(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq,
				   qdf_freq_t sec_ch_2g_freq,
				   struct ch_params *ch_params,
				   enum supported_6g_pwr_types in_6g_pwr_mode,
				   bool is_treat_nol_dis)
{
	if (reg_is_5ghz_ch_freq(freq) || reg_is_6ghz_chan_freq(freq))
		reg_set_5g_channel_params_for_pwrmode(pdev, freq, ch_params,
						      in_6g_pwr_mode,
						      is_treat_nol_dis);
	else if  (reg_is_24ghz_ch_freq(freq))
		reg_set_2g_channel_params_for_freq(pdev, freq, ch_params,
						   sec_ch_2g_freq);
}
#endif
#endif

uint8_t reg_get_channel_reg_power_for_freq(struct wlan_objmgr_pdev *pdev,
					   qdf_freq_t freq)
{
	enum channel_enum chan_enum;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *reg_channels;

	chan_enum = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(chan_enum)) {
		reg_err("channel is invalid");
		return REG_INVALID_TXPOWER;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return REG_INVALID_TXPOWER;
	}

	reg_channels = pdev_priv_obj->cur_chan_list;

	return reg_channels[chan_enum].tx_power;
}

bool reg_is_dfs_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	uint32_t chan_flags;

	chan_flags = reg_get_channel_flags_for_freq(pdev, freq);

	return chan_flags & REGULATORY_CHAN_RADAR;
}

#ifdef CONFIG_REG_CLIENT
bool reg_is_dfs_in_secondary_list_for_freq(struct wlan_objmgr_pdev *pdev,
					   qdf_freq_t freq)
{
	uint32_t chan_flags;

	chan_flags = reg_get_channel_flags_from_secondary_list_for_freq(pdev,
									freq);

	return chan_flags & REGULATORY_CHAN_RADAR;
}

/**
 * reg_get_psoc_mas_chan_list () - Get psoc master channel list
 * @pdev: pointer to pdev object
 * @psoc: pointer to psoc object
 *
 * Return: psoc master channel list
 */
static struct regulatory_channel *reg_get_psoc_mas_chan_list(
						struct wlan_objmgr_pdev *pdev,
						struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *soc_reg;
	uint8_t pdev_id;
	uint8_t phy_id;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;

	soc_reg = reg_get_psoc_obj(psoc);
	if (!soc_reg) {
		reg_err("reg psoc private obj is NULL");
		return NULL;
	}
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);
	if (reg_tx_ops->get_phy_id_from_pdev_id)
		reg_tx_ops->get_phy_id_from_pdev_id(psoc, pdev_id, &phy_id);
	else
		phy_id = pdev_id;

	return soc_reg->mas_chan_params[phy_id].mas_chan_list;
}
#else
static inline struct regulatory_channel *reg_get_psoc_mas_chan_list(
						struct wlan_objmgr_pdev *pdev,
						struct wlan_objmgr_psoc *psoc)
{
	return NULL;
}
#endif

void reg_update_nol_ch_for_freq(struct wlan_objmgr_pdev *pdev,
				uint16_t *chan_freq_list,
				uint8_t num_chan,
				bool nol_chan)
{
	enum channel_enum chan_enum;
	struct regulatory_channel *mas_chan_list = NULL, *psoc_mas_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_objmgr_psoc *psoc;
	uint16_t i;

	if (!num_chan || !chan_freq_list) {
		reg_err("chan_freq_list or num_ch is NULL");
		return;
	}

	psoc = wlan_pdev_get_psoc(pdev);


	psoc_mas_chan_list = reg_get_psoc_mas_chan_list(pdev, psoc);
	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (pdev_priv_obj)
		mas_chan_list = pdev_priv_obj->mas_chan_list;

	for (i = 0; i < num_chan; i++) {
		chan_enum = reg_get_chan_enum_for_freq(chan_freq_list[i]);
		if (reg_is_chan_enum_invalid(chan_enum)) {
			reg_err("Invalid freq in nol list, freq %d",
				chan_freq_list[i]);
			continue;
		}
		if (mas_chan_list)
			mas_chan_list[chan_enum].nol_chan = nol_chan;
		if (psoc_mas_chan_list)
			psoc_mas_chan_list[chan_enum].nol_chan = nol_chan;
	}

	if (!pdev_priv_obj) {
		reg_err("reg pdev private obj is NULL");
		return;
	}

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	reg_send_scheduler_msg_sb(psoc, pdev);
}

void reg_update_nol_history_ch_for_freq(struct wlan_objmgr_pdev *pdev,
					uint16_t *chan_list,
					uint8_t num_chan,
					bool nol_history_chan)
{
	enum channel_enum chan_enum;
	struct regulatory_channel *mas_chan_list;
	struct regulatory_channel *cur_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint16_t i;

	if (!num_chan || !chan_list) {
		reg_err("chan_list or num_ch is NULL");
		return;
	}

	pdev_priv_obj = wlan_objmgr_pdev_get_comp_private_obj(
			pdev, WLAN_UMAC_COMP_REGULATORY);

	if (!pdev_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return;
	}

	mas_chan_list = pdev_priv_obj->mas_chan_list;
	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < num_chan; i++) {
		chan_enum = reg_get_chan_enum_for_freq(chan_list[i]);
		if (reg_is_chan_enum_invalid(chan_enum)) {
			reg_err("Invalid ch in nol list, chan %d",
				chan_list[i]);
			continue;
		}
		mas_chan_list[chan_enum].nol_history = nol_history_chan;
		cur_chan_list[chan_enum].nol_history = nol_history_chan;
	}
}

qdf_freq_t reg_min_chan_freq(void)
{
	return channel_map[MIN_24GHZ_CHANNEL].center_freq;
}

qdf_freq_t reg_max_chan_freq(void)
{
	return channel_map[NUM_CHANNELS - 1].center_freq;
}

bool reg_is_same_band_freqs(qdf_freq_t freq1, qdf_freq_t freq2)
{
	return (freq1 && freq2 && ((REG_IS_6GHZ_FREQ(freq1) &&
				    REG_IS_6GHZ_FREQ(freq2)) ||
				   (REG_IS_5GHZ_FREQ(freq1) &&
				    REG_IS_5GHZ_FREQ(freq2)) ||
				   (REG_IS_24GHZ_CH_FREQ(freq1) &&
				    REG_IS_24GHZ_CH_FREQ(freq2))));
}

enum reg_wifi_band reg_freq_to_band(qdf_freq_t freq)
{
	if (REG_IS_24GHZ_CH_FREQ(freq))
		return REG_BAND_2G;
	else if (REG_IS_5GHZ_FREQ(freq) || REG_IS_49GHZ_FREQ(freq))
		return REG_BAND_5G;
	else if (REG_IS_6GHZ_FREQ(freq))
		return REG_BAND_6G;
	return REG_BAND_UNKNOWN;
}

#ifdef CONFIG_REG_6G_PWRMODE
bool reg_is_disable_for_pwrmode(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				enum supported_6g_pwr_types in_6g_pwr_mode)
{
	enum channel_state ch_state;

	ch_state = reg_get_channel_state_for_pwrmode(pdev,
						     freq,
						     in_6g_pwr_mode);

	return (ch_state == CHANNEL_STATE_DISABLE) ||
		(ch_state == CHANNEL_STATE_INVALID);
}
#endif

#ifdef CONFIG_REG_CLIENT
bool reg_is_disable_in_secondary_list_for_freq(struct wlan_objmgr_pdev *pdev,
					       qdf_freq_t freq)
{
	enum channel_state ch_state;

	ch_state = reg_get_channel_state_from_secondary_list_for_freq(pdev,
								      freq);

	return ch_state == CHANNEL_STATE_DISABLE;
}

bool reg_is_enable_in_secondary_list_for_freq(struct wlan_objmgr_pdev *pdev,
					      qdf_freq_t freq)
{
	enum channel_state ch_state;

	ch_state = reg_get_channel_state_from_secondary_list_for_freq(pdev,
								      freq);

	return ch_state == CHANNEL_STATE_ENABLE;
}

#ifdef CONFIG_BAND_6GHZ
static uint8_t reg_get_max_tx_power_from_super_chan_list(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			enum supported_6g_pwr_types in_6g_pwr_type)
{
	struct super_chan_info *sc_entry;
	enum supported_6g_pwr_types pwr_type;
	uint8_t i, max_tx_power = 0;

	pwr_type = in_6g_pwr_type;
	for (i = 0; i < NUM_6GHZ_CHANNELS; i++) {
		sc_entry = &pdev_priv_obj->super_chan_list[i];

		if (in_6g_pwr_type == REG_BEST_PWR_MODE)
			pwr_type = sc_entry->best_power_mode;

		if (reg_is_supp_pwr_mode_invalid(pwr_type))
			continue;

		if (!reg_is_chan_disabled(sc_entry->chan_flags_arr[pwr_type],
					  sc_entry->state_arr[pwr_type]) &&
		    (sc_entry->reg_chan_pwr[pwr_type].tx_power > max_tx_power))
			max_tx_power =
				sc_entry->reg_chan_pwr[pwr_type].tx_power;
	}
	return max_tx_power;
}
#else
static inline uint8_t reg_get_max_tx_power_from_super_chan_list(
			struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			enum supported_6g_pwr_types in_6g_pwr_type)
{
	return 0;
}
#endif

uint8_t reg_get_max_tx_power_for_pwr_mode(
				struct wlan_objmgr_pdev *pdev,
				enum supported_6g_pwr_types in_6g_pwr_type)
{
	uint8_t i, max_tx_power = 0, max_super_chan_power = 0;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint16_t max_curr_num_chan;

	if (!pdev) {
		reg_err_rl("invalid pdev");
		return QDF_STATUS_E_INVAL;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("reg pdev priv obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (in_6g_pwr_type == REG_CURRENT_PWR_MODE)
		max_curr_num_chan = NUM_CHANNELS;
	else
		max_curr_num_chan = MAX_5GHZ_CHANNEL;

	for (i = 0; i < max_curr_num_chan; i++) {
		if (!reg_is_chan_disabled(
			pdev_priv_obj->cur_chan_list[i].chan_flags,
			pdev_priv_obj->cur_chan_list[i].state) &&
		    (pdev_priv_obj->cur_chan_list[i].tx_power > max_tx_power))
			max_tx_power =
			pdev_priv_obj->cur_chan_list[i].tx_power;
	}

	if (in_6g_pwr_type == REG_CURRENT_PWR_MODE)
		goto return_max_tx_power;

	max_super_chan_power = reg_get_max_tx_power_from_super_chan_list(
								pdev_priv_obj,
								in_6g_pwr_type);

	if (max_super_chan_power > max_tx_power)
		max_tx_power = max_super_chan_power;

return_max_tx_power:

	if (!max_tx_power)
		reg_err_rl("max_tx_power is zero");

	return max_tx_power;
}
#endif

bool reg_is_passive_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	uint32_t chan_flags;

	chan_flags = reg_get_channel_flags_for_freq(pdev, freq);

	return chan_flags & REGULATORY_CHAN_NO_IR;
}
#endif /* CONFIG_CHAN_FREQ_API */

uint8_t  reg_get_max_tx_power(struct wlan_objmgr_pdev *pdev)
{
	struct regulatory_channel *cur_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint8_t i, max_tx_power = 0;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = 0; i < NUM_CHANNELS; i++) {
		if (cur_chan_list[i].state != CHANNEL_STATE_DISABLE &&
		    cur_chan_list[i].chan_flags != REGULATORY_CHAN_DISABLED) {
			if (cur_chan_list[i].tx_power > max_tx_power)
				max_tx_power = cur_chan_list[i].tx_power;
		}
	}

	if (!max_tx_power)
		reg_err_rl("max_tx_power is zero");

	return max_tx_power;
}

QDF_STATUS reg_set_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_reg;

	psoc_reg = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_reg)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc_reg->ignore_fw_reg_offload_ind = true;
	return QDF_STATUS_SUCCESS;
}

bool reg_get_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_reg;

	psoc_reg = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_reg))
		return false;

	return psoc_reg->ignore_fw_reg_offload_ind;
}

QDF_STATUS reg_set_6ghz_supported(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->six_ghz_supported = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_set_5dot9_ghz_supported(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->five_dot_nine_ghz_supported = val;

	return QDF_STATUS_SUCCESS;
}

#ifdef CONFIG_REG_CLIENT
bool reg_is_6ghz_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return  false;
	}

	return psoc_priv_obj->six_ghz_supported;
}
#endif

bool reg_is_5dot9_ghz_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return  false;
	}

	return psoc_priv_obj->five_dot_nine_ghz_supported;
}

bool reg_is_fcc_regdmn(struct wlan_objmgr_pdev *pdev)
{
	struct cur_regdmn_info cur_reg_dmn;
	QDF_STATUS status;

	status = reg_get_curr_regdomain(pdev, &cur_reg_dmn);
	if (status != QDF_STATUS_SUCCESS) {
		reg_debug_rl("Failed to get reg domain");
		return false;
	}

	return reg_fcc_regdmn(cur_reg_dmn.dmn_id_5g);
}

bool reg_is_5dot9_ghz_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return false;
	}

	return (freq >= channel_map_us[MIN_5DOT9_CHANNEL].center_freq &&
		freq <= channel_map_us[MAX_5DOT9_CHANNEL].center_freq);
}

bool reg_is_5dot9_ghz_chan_allowed_master_mode(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	if (!pdev) {
		reg_alert("pdev is NULL");
		return true;
	}
	psoc = wlan_pdev_get_psoc(pdev);

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_alert("psoc reg component is NULL");
		return true;
	}

	return psoc_priv_obj->enable_5dot9_ghz_chan_in_master_mode;
}

#ifdef DISABLE_UNII_SHARED_BANDS
QDF_STATUS
reg_get_unii_5g_bitmap(struct wlan_objmgr_pdev *pdev, uint8_t *bitmap)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*bitmap =  pdev_priv_obj->unii_5g_bitmap;

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_11BE
bool reg_is_phymode_unallowed(enum reg_phymode phy_in, uint32_t phymode_bitmap)
{
	if (!phymode_bitmap)
		return false;

	if (phy_in == REG_PHYMODE_11BE)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11BE;
	else if (phy_in == REG_PHYMODE_11AX)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11AX;
	else if (phy_in == REG_PHYMODE_11AC)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11AC;
	else if (phy_in == REG_PHYMODE_11N)
		return phymode_bitmap & REGULATORY_CHAN_NO11N;
	else if (phy_in == REG_PHYMODE_11G)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11G;
	else if (phy_in == REG_PHYMODE_11A)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11A;
	else if (phy_in == REG_PHYMODE_11B)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11B;
	else
		return true;
}
#else
bool reg_is_phymode_unallowed(enum reg_phymode phy_in, uint32_t phymode_bitmap)
{
	if (!phymode_bitmap)
		return false;

	if (phy_in == REG_PHYMODE_11AX)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11AX;
	else if (phy_in == REG_PHYMODE_11AC)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11AC;
	else if (phy_in == REG_PHYMODE_11N)
		return phymode_bitmap & REGULATORY_CHAN_NO11N;
	else if (phy_in == REG_PHYMODE_11G)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11G;
	else if (phy_in == REG_PHYMODE_11A)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11A;
	else if (phy_in == REG_PHYMODE_11B)
		return phymode_bitmap & REGULATORY_PHYMODE_NO11B;
	else
		return true;
}
#endif

#ifdef CHECK_REG_PHYMODE
enum reg_phymode reg_get_max_phymode(struct wlan_objmgr_pdev *pdev,
				     enum reg_phymode phy_in,
				     qdf_freq_t freq)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint32_t phymode_bitmap;
	enum reg_phymode current_phymode = phy_in;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return REG_PHYMODE_INVALID;
	}

	phymode_bitmap = pdev_priv_obj->phybitmap;

	while (1) {
		if (reg_is_phymode_unallowed(current_phymode, phymode_bitmap)) {
			if (current_phymode == REG_PHYMODE_11N) {
				if (REG_IS_24GHZ_CH_FREQ(freq))
					current_phymode = REG_PHYMODE_11G;
				else
					current_phymode = REG_PHYMODE_11A;
			} else if (current_phymode == REG_PHYMODE_11A ||
				   current_phymode == REG_PHYMODE_11B) {
				reg_err("Couldn't find a suitable phymode");
				return REG_PHYMODE_INVALID;
			} else if (current_phymode > REG_PHYMODE_MAX) {
				reg_err("Unknown phymode");
				return REG_PHYMODE_INVALID;
			} else {
				current_phymode--;
			}
		} else {
			return current_phymode;
		}
	}
}
#endif /* CHECK_REG_PHYMODE */

#ifdef CONFIG_REG_CLIENT
enum band_info reg_band_bitmap_to_band_info(uint32_t band_bitmap)
{
	if ((band_bitmap & BIT(REG_BAND_2G)) &&
	    (band_bitmap & BIT(REG_BAND_5G)) &&
	    (band_bitmap & BIT(REG_BAND_6G)))
		return BAND_ALL;
	else if ((band_bitmap & BIT(REG_BAND_5G)) &&
		 (band_bitmap & BIT(REG_BAND_6G)))
		return BAND_5G;
	else if ((band_bitmap & BIT(REG_BAND_2G)) &&
		 (band_bitmap & BIT(REG_BAND_6G)))
		return BAND_2G;
	else if ((band_bitmap & BIT(REG_BAND_2G)) &&
		 (band_bitmap & BIT(REG_BAND_5G)))
		return BAND_ALL;
	else if (band_bitmap & BIT(REG_BAND_2G))
		return BAND_2G;
	else if (band_bitmap & BIT(REG_BAND_5G))
		return BAND_5G;
	else if (band_bitmap & BIT(REG_BAND_6G))
		return BAND_2G;
	else
		return BAND_UNKNOWN;
}

QDF_STATUS
reg_update_tx_power_on_ctry_change(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	reg_ctry_change_callback callback = NULL;

	psoc = wlan_pdev_get_psoc(pdev);
	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&psoc_priv_obj->cbk_list_lock);
	if (psoc_priv_obj->cc_cbk.cbk)
		callback = psoc_priv_obj->cc_cbk.cbk;
	qdf_spin_unlock_bh(&psoc_priv_obj->cbk_list_lock);
	if (callback)
		callback(vdev_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_add_indoor_concurrency(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			   uint32_t freq, enum phy_ch_width width)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct indoor_concurrency_list *list;
	const struct bonded_channel_freq *range = NULL;
	uint8_t i = 0;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (width > CH_WIDTH_20MHZ)
		range = wlan_reg_get_bonded_chan_entry(freq, width, 0);

	list = &pdev_priv_obj->indoor_list[0];
	for (i = 0; i < MAX_INDOOR_LIST_SIZE; i++, list++) {
		if (list->freq == 0 && list->vdev_id == INVALID_VDEV_ID) {
			list->freq = freq;
			list->vdev_id = vdev_id;
			list->chan_range = range;
			reg_debug("Added freq %d vdev %d width %d at idx %d",
				  freq, vdev_id, width, i);
			return QDF_STATUS_SUCCESS;
		}
	}
	reg_err("Unable to add indoor concurrency for vdev %d freq %d width %d",
		vdev_id, freq, width);
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
reg_remove_indoor_concurrency(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			      uint32_t freq)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct indoor_concurrency_list *list;
	uint8_t i = 0;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	list = &pdev_priv_obj->indoor_list[0];
	for (i = 0; i < MAX_INDOOR_LIST_SIZE; i++, list++) {
		if (list->freq == freq ||
		    (vdev_id != INVALID_VDEV_ID && list->vdev_id == vdev_id)) {
			reg_debug("Removed freq %d from idx %d", list->freq, i);
			list->freq = 0;
			list->vdev_id = INVALID_VDEV_ID;
			list->chan_range = NULL;
			return QDF_STATUS_SUCCESS;
		}
		continue;
	}

	return QDF_STATUS_E_FAILURE;
}

void
reg_init_indoor_channel_list(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct indoor_concurrency_list *list;
	uint8_t i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	list = pdev_priv_obj->indoor_list;
	for (i = 0; i < MAX_INDOOR_LIST_SIZE; i++, list++) {
		list->freq = 0;
		list->vdev_id = INVALID_VDEV_ID;
		list->chan_range = NULL;
	}
}

QDF_STATUS
reg_compute_indoor_list_on_cc_change(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_channel *des_chan;
	enum channel_enum chan_enum;
	uint8_t vdev_id;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (pdev_priv_obj->indoor_chan_enabled ||
	    !pdev_priv_obj->sta_sap_scc_on_indoor_channel)
		return QDF_STATUS_SUCCESS;

	/* Iterate through VDEV list */
	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {
		vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						     WLAN_REGULATORY_SB_ID);
		if (!vdev)
			continue;

		if (vdev->vdev_mlme.vdev_opmode != QDF_STA_MODE &&
		    vdev->vdev_mlme.vdev_opmode != QDF_P2P_CLIENT_MODE)
			goto next;

		des_chan = vdev->vdev_mlme.des_chan;
		if (!des_chan)
			goto next;

		if (!reg_is_5ghz_ch_freq(des_chan->ch_freq))
			goto next;

		chan_enum = reg_get_chan_enum_for_freq(des_chan->ch_freq);
		if (reg_is_chan_enum_invalid(chan_enum)) {
			reg_err_rl("Invalid chan enum %d", chan_enum);
			goto next;
		}

		if (pdev_priv_obj->mas_chan_list[chan_enum].state !=
		    CHANNEL_STATE_DISABLE &&
		    pdev_priv_obj->mas_chan_list[chan_enum].chan_flags &
		    REGULATORY_CHAN_INDOOR_ONLY)
			reg_add_indoor_concurrency(pdev, vdev_id,
						   des_chan->ch_freq,
						   des_chan->ch_width);

next:
		wlan_objmgr_vdev_release_ref(vdev, WLAN_REGULATORY_SB_ID);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(CONFIG_BAND_6GHZ)
QDF_STATUS
reg_set_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
			   enum reg_6g_ap_type reg_cur_6g_ap_pwr_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (reg_cur_6g_ap_pwr_type > REG_MAX_SUPP_AP_TYPE) {
		reg_err("Unsupported 6G AP power type");
		return QDF_STATUS_E_FAILURE;
	}
	/* should we validate the input reg_cur_6g_ap_type? */
	pdev_priv_obj->reg_cur_6g_ap_pwr_type = reg_cur_6g_ap_pwr_type;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_get_cur_6g_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
			   enum reg_6g_ap_type *reg_cur_6g_ap_pwr_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (pdev_priv_obj->reg_cur_6g_ap_pwr_type >= REG_CURRENT_MAX_AP_TYPE)
		return QDF_STATUS_E_FAILURE;

	*reg_cur_6g_ap_pwr_type = pdev_priv_obj->reg_cur_6g_ap_pwr_type;

	return QDF_STATUS_SUCCESS;
}

/**
 * get_reg_rules_for_pdev() - Get the pointer to the reg rules for the pdev
 * @pdev: Pointer to pdev
 *
 * Return: Pointer to Standard Power regulatory rules
 */
static struct reg_rule_info *
reg_get_reg_rules_for_pdev(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_regulatory_psoc_priv_obj *psoc_reg_priv;
	uint8_t phy_id;
	struct reg_rule_info *psoc_reg_rules;

	psoc = wlan_pdev_get_psoc(pdev);
	psoc_reg_priv = reg_get_psoc_obj(psoc);

	if (!psoc_reg_priv) {
		reg_debug("Regulatory psoc private object is NULL");
		return NULL;
	}

	phy_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	psoc_reg_rules = &psoc_reg_priv->mas_chan_params[phy_id].reg_rules;

	return psoc_reg_rules;
}

uint8_t
reg_get_num_rules_of_ap_pwr_type(struct wlan_objmgr_pdev *pdev,
				 enum reg_6g_ap_type ap_pwr_type)
{
	struct reg_rule_info *psoc_reg_rules = reg_get_reg_rules_for_pdev(pdev);

	if (!psoc_reg_rules) {
		reg_debug("No psoc_reg_rules");
		return 0;
	}

	if (ap_pwr_type > REG_MAX_SUPP_AP_TYPE) {
		reg_err("Unsupported 6G AP power type");
		return 0;
	}

	return psoc_reg_rules->num_of_6g_ap_reg_rules[ap_pwr_type];
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * reg_is_empty_range() - If both left, right frquency edges in the input range
 * are zero then the range is empty, else not.
 * @in_range: Pointer to input range
 *
 * Return: True if the range is empty, else false
 */
static bool reg_is_empty_range(struct freq_range *in_range)
{
	return !in_range->left && !in_range->right;
}

struct freq_range
reg_init_freq_range(qdf_freq_t left, qdf_freq_t right)
{
	struct freq_range out_range;

	out_range.left = left;
	out_range.right = right;

	return out_range;
}

/**
 * reg_assign_vars_with_range_vals() - Assign input variables with the values of
 * the range variable values
 * @in_range: Pointer to input range object
 * @left: Pointer to the first variable to get the value of left frequency edge
 * @right: Pointer to the second variable to get the value of right frequency
 *         edge
 *
 * Return: void
 */
static void
reg_assign_vars_with_range_vals(struct freq_range *in_range,
				qdf_freq_t *left,
				qdf_freq_t *right)
{
	*left = in_range->left;
	*right = in_range->right;
}

/**
 * reg_intersect_ranges() - Intersect two ranges and return the intesected range
 * @first: Pointer to first input range
 * @second: Pointer to second input range
 *
 * Return: Intersected output range
 */
static struct freq_range
reg_intersect_ranges(struct freq_range *first_range,
		     struct freq_range *second_range)
{
	struct freq_range out_range;
	qdf_freq_t l_freq;
	qdf_freq_t r_freq;

	/* validate if the ranges are proper */

	l_freq = QDF_MAX(first_range->left, second_range->left);
	r_freq = QDF_MIN(first_range->right, second_range->right);

	if (l_freq > r_freq) {
		l_freq = 0;
		l_freq = 0;

		reg_debug("Ranges do not overlap first= [%u, %u], second = [%u, %u]",
			  first_range->left,
			  first_range->right,
			  second_range->left,
			  second_range->right);
	}

	out_range.left = l_freq;
	out_range.right = r_freq;

	return out_range;
}

/**
 * reg_act_sp_rule_cb -  A function pointer type that calculate something
 * from the input frequency range
 * @rule_fr: Pointer to frequency range
 * @arg: Pointer to generic argument (a.k.a. context)
 *
 * Return: Void
 */
typedef void (*reg_act_sp_rule_cb)(struct freq_range *rule_fr,
				   void *arg);

/**
 * reg_iterate_sp_rules() - Iterate through the Standard Power reg rules, for
 * every reg rule call the call back function to take some action or calculate
 * something
 * @pdev: Pointer to pdev
 * @pdev_priv_obj: Pointer to pdev private object
 * @action_on_sp_rule: A function pointer to take some action or calculate
 * something for every sp rule
 * @arg: Pointer to opque object (argument/context)
 *
 * Return: Void
 */
static void reg_iterate_sp_rules(struct wlan_objmgr_pdev *pdev,
				 struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
				 reg_act_sp_rule_cb sp_rule_action,
				 void *arg)
{
	struct cur_reg_rule *p_sp_reg_rule;
	struct reg_rule_info *psoc_reg_rules;
	uint8_t n_6g_sp_ap_reg_rules;
	qdf_freq_t low_5g;
	qdf_freq_t high_5g;
	uint8_t i;
	struct freq_range chip_range;

	psoc_reg_rules = reg_get_reg_rules_for_pdev(pdev);

	if (!psoc_reg_rules) {
		reg_debug("psoc reg rule pointer is NULL");
		return;
	}

	n_6g_sp_ap_reg_rules = psoc_reg_rules->num_of_6g_ap_reg_rules[REG_STANDARD_POWER_AP];
	p_sp_reg_rule = psoc_reg_rules->reg_rules_6g_ap[REG_STANDARD_POWER_AP];

	low_5g = pdev_priv_obj->range_5g_low;
	high_5g = pdev_priv_obj->range_5g_high;

	chip_range = reg_init_freq_range(low_5g, high_5g);

	reg_debug("chip_range = [%u, %u]", low_5g, high_5g);
	reg_debug("Num_6g_rules = %u", n_6g_sp_ap_reg_rules);

	for (i = 0; i < n_6g_sp_ap_reg_rules; i++) {
		struct freq_range sp_range;
		struct freq_range out_range;

		sp_range = reg_init_freq_range(p_sp_reg_rule->start_freq,
					       p_sp_reg_rule->end_freq);
		reg_debug("Rule:[%u, %u]",
			  p_sp_reg_rule->start_freq,
			  p_sp_reg_rule->end_freq);
		out_range = reg_intersect_ranges(&chip_range, &sp_range);

		if (sp_rule_action)
			sp_rule_action(&out_range, arg);

		p_sp_reg_rule++;
	}
}

/**
 * reg_afc_incr_num_ranges() - Increment the number of frequency ranges
 * @p_range: Pointer to frequency range
 * @num_freq_ranges: Pointer to number of frequency ranges. This needs to be
 * (Actual type: uint8_t *num_freq_ranges)
 * incremented by the function
 *
 * Return: Void
 */
static void reg_afc_incr_num_ranges(struct freq_range *p_range,
				    void *num_freq_ranges)
{
	if (!reg_is_empty_range(p_range))
		(*(uint8_t *)num_freq_ranges)++;
}

/**
 * reg_get_num_sp_freq_ranges() - Find the number of reg rules from the Standard
 * power regulatory rules
 * @pdev: Pointer to pdev
 *
 * Return: number of frequency ranges
 */
static uint8_t reg_get_num_sp_freq_ranges(struct wlan_objmgr_pdev *pdev,
					  struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	uint8_t num_freq_ranges;

	num_freq_ranges = 0;
	reg_iterate_sp_rules(pdev,
			     pdev_priv_obj,
			     reg_afc_incr_num_ranges,
			     &num_freq_ranges);

	reg_debug("Num_freq_ranges=%u", num_freq_ranges);
	return num_freq_ranges;
}

/**
 * reg_afc_get_intersected_ranges() - Get the intersected range into range obj
 * @rule_fr: Pointer to the rule for frequency range
 * @arg: Pointer to opaque object (argument/context)
 * (Actual type: struct wlan_afc_freq_range_obj **p_range_obj)
 * incremented by the function
 *
 * Return: Void
 */
static void reg_afc_get_intersected_ranges(struct freq_range *rule_fr,
					   void *arg)
{
	struct wlan_afc_freq_range_obj *p_range;
	struct wlan_afc_freq_range_obj **pp_range;
	qdf_freq_t low, high;

	pp_range = (struct wlan_afc_freq_range_obj **)arg;
	p_range = *pp_range;

	if (!reg_is_empty_range(rule_fr)) {
		reg_assign_vars_with_range_vals(rule_fr, &low, &high);
		p_range->lowfreq = (uint16_t)low;
		p_range->highfreq = (uint16_t)high;
		reg_debug("Range = [%u, %u]", p_range->lowfreq, p_range->highfreq);
		(*pp_range)++;
	}
}

/**
 * reg_cp_freq_ranges() - Copy frequency ranges  from the Standard power
 * regulatory rules
 * @pdev: Pointer to pdev
 * @pdev_priv_obj: Pointer to pdev private object
 * @num_freq_ranges: Number of frequency ranges
 * @p_range_obj: Pointer to range object
 *
 * Return: void
 */
static void reg_cp_freq_ranges(struct wlan_objmgr_pdev *pdev,
			       struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			       uint8_t num_freq_ranges,
			       struct wlan_afc_freq_range_obj *p_range_obj)
{
	struct wlan_afc_freq_range_obj *p_range;

	reg_debug("Num freq ranges = %u", num_freq_ranges);

	p_range = p_range_obj;
	reg_iterate_sp_rules(pdev,
			     pdev_priv_obj,
			     reg_afc_get_intersected_ranges,
			     &p_range);
}

/**
 * reg_get_frange_list_len() - Calculate the length of the list of the
 * frequency ranges
 * @num_freq_ranges: Number of frequency ranges
 *
 * Return: Length of the frequency range list
 */
static uint16_t reg_get_frange_list_len(uint8_t num_freq_ranges)
{
	uint16_t frange_lst_len;

	if (!num_freq_ranges)
		reg_err("AFC:There is no freq ranges");

	frange_lst_len =
		sizeof(struct wlan_afc_frange_list) +
		sizeof(struct wlan_afc_freq_range_obj) * num_freq_ranges;

	return frange_lst_len;
}

/**
 * reg_get_opclasses_array_len() - Calculate the length of the array of
 * opclasses objects
 * @num_opclasses: The number of opclasses
 * @chansize_lst: The array of sizes of channel lists
 *
 * Return: Length of the array of opclass object
 */
static uint16_t reg_get_opclasses_array_len(struct wlan_objmgr_pdev *pdev,
					    uint8_t num_opclasses,
					    uint8_t *chansize_lst)
{
	uint16_t opclasses_arr_len = 0;
	uint16_t i;

	for (i = 0; i < num_opclasses; i++) {
		opclasses_arr_len +=
			sizeof(struct wlan_afc_opclass_obj) +
			sizeof(uint8_t) * chansize_lst[i];
	}

	return opclasses_arr_len;
}

/**
 * reg_get_afc_req_length() - Calculate the length of the AFC partial request
 * @num_opclasses: The number of opclasses
 * @num_freq_ranges: The number of frequency ranges
 * @chansize_lst: The array of sizes of channel lists
 *
 * Return: Length of the partial AFC request
 */
static uint16_t reg_get_afc_req_length(struct wlan_objmgr_pdev *pdev,
				       uint8_t num_opclasses,
				       uint8_t num_freq_ranges,
				       uint8_t *chansize_lst)
{
	uint16_t afc_req_len;
	uint16_t frange_lst_len;
	uint16_t fixed_param_len;
	uint16_t num_opclasses_len;
	uint16_t opclasses_arr_len;
	uint16_t afc_location_len;

	fixed_param_len = sizeof(struct wlan_afc_host_req_fixed_params);
	frange_lst_len = reg_get_frange_list_len(num_freq_ranges);
	num_opclasses_len = sizeof(struct wlan_afc_num_opclasses);
	opclasses_arr_len = reg_get_opclasses_array_len(pdev,
							num_opclasses,
							chansize_lst);
	afc_location_len = sizeof(struct wlan_afc_location);

	afc_req_len =
		fixed_param_len +
		frange_lst_len +
		num_opclasses_len +
		opclasses_arr_len +
		afc_location_len;

	return afc_req_len;
}

/**
 * reg_fill_afc_fixed_params() - Fill the AFC fixed params
 * @p_fixed_params: Pointer to afc fixed params object
 * @afc_req_len: Length of the partial AFC request
 *
 * Return: Void
 */
static inline void
reg_fill_afc_fixed_params(struct wlan_afc_host_req_fixed_params *p_fixed_params,
			  uint16_t afc_req_len)
{
	p_fixed_params->req_length = afc_req_len;
	p_fixed_params->req_id = DEFAULT_REQ_ID;
	p_fixed_params->min_des_power = DEFAULT_MIN_POWER;
}

/**
 * reg_fill_afc_freq_ranges() - Fill the AFC fixed params
 * @pdev: Pointer to pdev
 * @pdev_priv_obj: Pointer to pdev private object
 * @p_frange_lst: Pointer to frequency range list
 * @num_freq_ranges: Number of frequency ranges
 *
 * Return: Void
 */
static inline void
reg_fill_afc_freq_ranges(struct wlan_objmgr_pdev *pdev,
			 struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			 struct wlan_afc_frange_list *p_frange_lst,
			 uint8_t num_freq_ranges)
{
	struct wlan_afc_freq_range_obj *p_range_obj;

	p_frange_lst->num_ranges = num_freq_ranges;

	p_range_obj = &p_frange_lst->range_objs[0];

	reg_cp_freq_ranges(pdev, pdev_priv_obj, num_freq_ranges, p_range_obj);
}

/**
 * reg_fill_afc_opclass_obj() - Fill the opclass object and return pointer to
 *                              next AFC opclass object
 * @p_obj_opclass_obj: Pointer to opclass object
 * @opclass: Operating class
 * @num_chans: Number of channels in the opclass
 * @p_chan_lst: Pointer to channel list
 *
 * Return: Pointer to the next AFC opclass object
 */
static struct wlan_afc_opclass_obj *
reg_fill_afc_opclass_obj(struct wlan_afc_opclass_obj *p_obj_opclass_obj,
			 uint8_t opclass,
			 uint8_t num_chans,
			 uint8_t *p_chan_lst)
{
	uint16_t len_obj;
	uint8_t *out_p;
	uint8_t *src, *dst;
	uint8_t copy_len;

	p_obj_opclass_obj->opclass_num_cfis = num_chans;
	p_obj_opclass_obj->opclass = opclass;
	src = p_chan_lst;
	dst = p_obj_opclass_obj->cfis;
	copy_len = num_chans * sizeof(uint8_t);

	qdf_mem_copy(dst, src, copy_len);

	len_obj = sizeof(struct wlan_afc_opclass_obj) + copy_len;
	out_p = (uint8_t *)p_obj_opclass_obj + len_obj;

	return (struct wlan_afc_opclass_obj *)out_p;
}

/**
 * reg_fill_afc_opclasses_arr() - Fill the array of opclass objects
 * @num_opclasses: The number of opclasses
 * @opclass_lst: The array of Operating classes
 * @chansize_lst: The array of sizes of channel lists
 * @channel_lists: The array of channel lists
 * @p_opclass_obj_arr: Pointer to the first opclass object
 *
 * Return: Pointer to the end of last opclass object
 */
static inline struct wlan_afc_opclass_obj *
reg_fill_afc_opclasses_arr(struct wlan_objmgr_pdev *pdev,
			   uint8_t num_opclasses,
			   uint8_t *opclass_lst,
			   uint8_t *chansize_lst,
			   uint8_t *channel_lists[],
			   struct wlan_afc_opclass_obj *p_opclass_obj_arr)
{
	uint16_t i;
	struct wlan_afc_opclass_obj *p_opclass_obj;

	p_opclass_obj = p_opclass_obj_arr;

	for (i = 0; i < num_opclasses; i++) {
		p_opclass_obj = reg_fill_afc_opclass_obj(p_opclass_obj,
							 opclass_lst[i],
							 chansize_lst[i],
							 channel_lists[i]);
	}
	return p_opclass_obj;
}

/**
 * reg_next_opcls_ptr() - Get the pointer to the next opclass object
 * @p_cur_opcls_obj: Pointer to the current operating class object
 * @num_cfis: number of center frequency indices
 *
 * Return: Pointer to next opclss object
 */
static struct wlan_afc_opclass_obj *
reg_next_opcls_ptr(struct wlan_afc_opclass_obj *p_cur_opcls_obj,
		   uint8_t num_cfis)
{
	uint8_t cur_obj_sz;
	uint8_t fixed_opcls_sz;
	struct wlan_afc_opclass_obj *p_next_opcls_obj;
	uint8_t *p_tmp_next;

	fixed_opcls_sz = sizeof(struct wlan_afc_opclass_obj);
	cur_obj_sz = fixed_opcls_sz + num_cfis * sizeof(uint8_t);
	p_tmp_next = (uint8_t *)p_cur_opcls_obj + cur_obj_sz;
	p_next_opcls_obj = (struct wlan_afc_opclass_obj *)p_tmp_next;

	return p_next_opcls_obj;
}

void reg_print_partial_afc_req_info(struct wlan_objmgr_pdev *pdev,
				    struct wlan_afc_host_partial_request *afc_req)
{
	struct wlan_afc_host_req_fixed_params *p_fixed_params;
	struct wlan_afc_frange_list *p_frange_lst;
	struct wlan_afc_num_opclasses *p_num_opclasses;
	uint8_t i;
	uint8_t j;
	uint16_t frange_lst_len;
	uint8_t num_opclasses;
	struct wlan_afc_opclass_obj *p_obj_opclass_arr;
	struct wlan_afc_opclass_obj *p_opclass_obj;
	uint8_t num_freq_ranges;
	uint8_t *p_temp;
	struct wlan_afc_location *p_afc_location;
	uint8_t *deployment_type_str;

	p_fixed_params = &afc_req->fixed_params;
	reg_debug("req_length=%hu", p_fixed_params->req_length);
	reg_debug("req_id=%llu", p_fixed_params->req_id);
	reg_debug("min_des_power=%hd", p_fixed_params->min_des_power);

	p_temp = (uint8_t *)p_fixed_params;
	p_temp += sizeof(*p_fixed_params);
	p_frange_lst = (struct wlan_afc_frange_list *)p_temp;
	reg_debug("num_ranges=%hhu", p_frange_lst->num_ranges);
	for (i = 0; i < p_frange_lst->num_ranges; i++) {
		struct wlan_afc_freq_range_obj *p_range_obj;

		p_range_obj = &p_frange_lst->range_objs[i];
		reg_debug("lowfreq=%hu", p_range_obj->lowfreq);
		reg_debug("highfreq=%hu", p_range_obj->highfreq);
	}

	num_freq_ranges = p_frange_lst->num_ranges;
	frange_lst_len = reg_get_frange_list_len(num_freq_ranges);
	p_temp += frange_lst_len;
	p_num_opclasses = (struct wlan_afc_num_opclasses *)p_temp;
	num_opclasses = p_num_opclasses->num_opclasses;
	reg_debug("num_opclasses=%hhu", num_opclasses);

	p_temp += sizeof(*p_num_opclasses);
	p_obj_opclass_arr = (struct wlan_afc_opclass_obj *)p_temp;
	p_opclass_obj = p_obj_opclass_arr;
	for (i = 0; i < num_opclasses; i++) {
		uint8_t opclass = p_opclass_obj->opclass;
		uint8_t num_cfis = p_opclass_obj->opclass_num_cfis;
		uint8_t *cfis = p_opclass_obj->cfis;

		reg_debug("opclass[%hhu]=%hhu", i, opclass);
		reg_debug("num_cfis[%hhu]=%hhu", i, num_cfis);
		reg_debug("[");
		for (j = 0; j < num_cfis; j++)
			reg_debug("%hhu,", cfis[j]);
		reg_debug("]");

		p_opclass_obj = reg_next_opcls_ptr(p_opclass_obj, num_cfis);
	}

	p_afc_location = (struct wlan_afc_location *)p_opclass_obj;
	switch (p_afc_location->deployment_type) {
	case AFC_DEPLOYMENT_INDOOR:
		deployment_type_str = "Indoor";
		break;
	case AFC_DEPLOYMENT_OUTDOOR:
		deployment_type_str = "Outdoor";
		break;
	default:
		deployment_type_str = "Unknown";
	}
	reg_debug("AFC location=%s", deployment_type_str);
}

/**
 * reg_get_frange_filled_buf() - Allocate and fill the frange buffer and return
 * the buffer. Also return the number of frequence ranges
 * @pdev: Pointer to pdev
 * @pdev_priv_obj: Pointer to pdev private object
 * @num_freq_ranges: Pointer to number of frequency ranges (output param)
 *
 * Return: Pointer to the frange buffer
 */
static struct wlan_afc_frange_list *
reg_get_frange_filled_buf(struct wlan_objmgr_pdev *pdev,
			  struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			  uint8_t *num_freq_ranges)
{
	uint16_t frange_lst_len;
	struct wlan_afc_frange_list *p_frange_lst_local;

	*num_freq_ranges =  reg_get_num_sp_freq_ranges(pdev, pdev_priv_obj);
	frange_lst_len = reg_get_frange_list_len(*num_freq_ranges);

	p_frange_lst_local = qdf_mem_malloc(frange_lst_len);
	if (!p_frange_lst_local)
		return NULL;

	reg_fill_afc_freq_ranges(pdev,
				 pdev_priv_obj,
				 p_frange_lst_local,
				 *num_freq_ranges);
	return p_frange_lst_local;
}

QDF_STATUS
reg_get_partial_afc_req_info(struct wlan_objmgr_pdev *pdev,
			     struct wlan_afc_host_partial_request **afc_req)
{
	/* allocate the memory for the partial request */
	struct wlan_afc_host_partial_request *temp_afc_req;
	struct wlan_afc_host_req_fixed_params *p_fixed_params;
	struct wlan_afc_frange_list *p_frange_lst_local;
	struct wlan_afc_frange_list *p_frange_lst_afc;
	struct wlan_afc_num_opclasses *p_num_opclasses;
	uint16_t afc_req_len;
	uint16_t frange_lst_len;
	uint8_t num_freq_ranges;
	uint8_t num_opclasses;
	struct wlan_afc_opclass_obj *p_obj_opclass_arr;
	struct wlan_afc_location *p_afc_location;

	uint8_t *opclass_lst;
	uint8_t *chansize_lst;
	uint8_t **channel_lists;
	QDF_STATUS status;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	if (!afc_req) {
		reg_err("afc_req is NULL");
		status = QDF_STATUS_E_INVAL;
		return status;
	}

	temp_afc_req = NULL;
	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		status = QDF_STATUS_E_INVAL;
		goto handle_invalid_priv_object;
	}

	p_frange_lst_local = reg_get_frange_filled_buf(pdev,
						       pdev_priv_obj,
						       &num_freq_ranges);
	if (!p_frange_lst_local) {
		reg_err("Frange lst not allocated");
		status = QDF_STATUS_E_NOMEM;
		goto handle_invalid_priv_object;
	}

	status = reg_dmn_get_6g_opclasses_and_channels(pdev,
						       p_frange_lst_local,
						       &num_opclasses,
						       &opclass_lst,
						       &chansize_lst,
						       &channel_lists);
	if (status != QDF_STATUS_SUCCESS) {
		reg_err("Opclasses and chans not allocated");
		status = QDF_STATUS_E_NOMEM;
		goto free_frange_lst_local;
	}

	afc_req_len = reg_get_afc_req_length(pdev,
					     num_opclasses,
					     num_freq_ranges,
					     chansize_lst);

	temp_afc_req = qdf_mem_malloc(afc_req_len);
	if (!temp_afc_req) {
		reg_err("AFC request not allocated");
		status = QDF_STATUS_E_NOMEM;
		goto free_opcls_chan_mem;
	}

	p_fixed_params = &temp_afc_req->fixed_params;
	reg_fill_afc_fixed_params(p_fixed_params, afc_req_len);

	/* frange list is already filled just copy it */
	frange_lst_len = reg_get_frange_list_len(num_freq_ranges);
	p_frange_lst_afc = (struct wlan_afc_frange_list *)&p_fixed_params[1];
	qdf_mem_copy(p_frange_lst_afc, p_frange_lst_local, frange_lst_len);

	p_num_opclasses = (struct wlan_afc_num_opclasses *)
	    ((char *)(p_frange_lst_afc) + frange_lst_len);
	p_num_opclasses->num_opclasses = num_opclasses;

	p_obj_opclass_arr = (struct wlan_afc_opclass_obj *)&p_num_opclasses[1];
	p_obj_opclass_arr = reg_fill_afc_opclasses_arr(pdev,
						       num_opclasses,
						       opclass_lst,
						       chansize_lst,
						       channel_lists,
						       p_obj_opclass_arr);

	p_afc_location = (struct wlan_afc_location *)p_obj_opclass_arr;
	p_afc_location->deployment_type =
				pdev_priv_obj->reg_afc_dev_deployment_type;
	p_afc_location->afc_elem_type = AFC_OBJ_LOCATION;
	p_afc_location->afc_elem_len =
				sizeof(*p_afc_location) -
				sizeof(p_afc_location->afc_elem_type) -
				sizeof(p_afc_location->afc_elem_len);
free_opcls_chan_mem:
	reg_dmn_free_6g_opclasses_and_channels(pdev,
					       num_opclasses,
					       opclass_lst,
					       chansize_lst,
					       channel_lists);

free_frange_lst_local:
	qdf_mem_free(p_frange_lst_local);

handle_invalid_priv_object:
	*afc_req = temp_afc_req;

	return status;
}

void reg_dmn_set_afc_req_id(struct wlan_afc_host_partial_request *afc_req,
			    uint64_t req_id)
{
	struct wlan_afc_host_req_fixed_params *p_fixed_params;

	p_fixed_params = &afc_req->fixed_params;
	p_fixed_params->req_id = req_id;
}

/**
 * reg_send_afc_partial_request() - Send AFC partial request to registered
 * recipient
 * @pdev: Pointer to pdev
 * @afc_req: Pointer to afc partial request
 *
 * Return: void
 */
static
void reg_send_afc_partial_request(struct wlan_objmgr_pdev *pdev,
				  struct wlan_afc_host_partial_request *afc_req)
{
	afc_req_rx_evt_handler cbf;
	void *arg;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return;
	}

	qdf_spin_lock_bh(&pdev_priv_obj->afc_cb_lock);
	cbf = pdev_priv_obj->afc_cb_obj.func;
	if (cbf) {
		arg = pdev_priv_obj->afc_cb_obj.arg;
		cbf(pdev, afc_req, arg);
	}
	qdf_spin_unlock_bh(&pdev_priv_obj->afc_cb_lock);
}

QDF_STATUS reg_afc_start(struct wlan_objmgr_pdev *pdev, uint64_t req_id)
{
	struct wlan_afc_host_partial_request *afc_req;
	QDF_STATUS status;

	status = reg_get_partial_afc_req_info(pdev, &afc_req);
	if (status != QDF_STATUS_SUCCESS) {
		reg_err("Creating AFC Request failed");
		return QDF_STATUS_E_FAILURE;
	}

	QDF_TRACE(QDF_MODULE_ID_AFC, QDF_TRACE_LEVEL_DEBUG,
		  "Processing AFC Start/Renew Expiry event");

	reg_dmn_set_afc_req_id(afc_req, req_id);

	reg_print_partial_afc_req_info(pdev, afc_req);

	reg_send_afc_partial_request(pdev, afc_req);

	qdf_mem_free(afc_req);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_send_afc_power_event(struct wlan_objmgr_pdev *pdev,
				    struct reg_fw_afc_power_event *power_info)
{
	afc_power_tx_evt_handler cbf;
	void *arg;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&pdev_priv_obj->afc_cb_lock);
	cbf = pdev_priv_obj->afc_pow_evt_cb_obj.func;
	if (cbf) {
		arg = pdev_priv_obj->afc_pow_evt_cb_obj.arg;
		cbf(pdev, power_info, arg);
	}

	qdf_spin_unlock_bh(&pdev_priv_obj->afc_cb_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_register_afc_req_rx_callback(struct wlan_objmgr_pdev *pdev,
					    afc_req_rx_evt_handler cbf,
					    void *arg)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&pdev_priv_obj->afc_cb_lock);
	pdev_priv_obj->afc_cb_obj.func = cbf;
	pdev_priv_obj->afc_cb_obj.arg = arg;
	qdf_spin_unlock_bh(&pdev_priv_obj->afc_cb_lock);
	reg_debug("afc_event_cb: 0x%pK, arg: 0x%pK", cbf, arg);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_unregister_afc_req_rx_callback(struct wlan_objmgr_pdev *pdev,
					      afc_req_rx_evt_handler cbf)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&pdev_priv_obj->afc_cb_lock);
	if (pdev_priv_obj->afc_cb_obj.func == cbf) {
		pdev_priv_obj->afc_cb_obj.func = NULL;
		pdev_priv_obj->afc_cb_obj.arg = NULL;
	} else {
		reg_err("cb function=0x%pK not found", cbf);
	}
	qdf_spin_unlock_bh(&pdev_priv_obj->afc_cb_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_register_afc_power_event_callback(struct wlan_objmgr_pdev *pdev,
				      afc_power_tx_evt_handler cbf,
				      void *arg)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&pdev_priv_obj->afc_cb_lock);
	pdev_priv_obj->afc_pow_evt_cb_obj.func = cbf;
	pdev_priv_obj->afc_pow_evt_cb_obj.arg = arg;
	qdf_spin_unlock_bh(&pdev_priv_obj->afc_cb_lock);
	reg_debug("afc_power_event_cb: 0x%pK, arg: 0x%pK", cbf, arg);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_unregister_afc_power_event_callback(struct wlan_objmgr_pdev *pdev,
					afc_power_tx_evt_handler cbf)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&pdev_priv_obj->afc_cb_lock);
	if (pdev_priv_obj->afc_pow_evt_cb_obj.func == cbf) {
		pdev_priv_obj->afc_pow_evt_cb_obj.func = NULL;
		pdev_priv_obj->afc_pow_evt_cb_obj.arg = NULL;
	} else {
		reg_err("cb function=0x%pK not found", cbf);
	}
	qdf_spin_unlock_bh(&pdev_priv_obj->afc_cb_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_get_afc_dev_deploy_type(struct wlan_objmgr_pdev *pdev,
			    enum reg_afc_dev_deploy_type *reg_afc_dev_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*reg_afc_dev_type = pdev_priv_obj->reg_afc_dev_deployment_type;

	return QDF_STATUS_SUCCESS;
}

bool
reg_is_sta_connect_allowed(struct wlan_objmgr_pdev *pdev,
			   enum reg_6g_ap_type root_ap_pwr_mode)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return false;
	}

	if (reg_get_num_rules_of_ap_pwr_type(pdev, REG_STANDARD_POWER_AP) &&
	    (pdev_priv_obj->reg_afc_dev_deployment_type == AFC_DEPLOYMENT_OUTDOOR)) {
		if (root_ap_pwr_mode == REG_STANDARD_POWER_AP)
			return true;
		else
			return false;
	}

	return true;
}

QDF_STATUS reg_set_afc_soc_dev_type(struct wlan_objmgr_psoc *psoc,
				    enum reg_afc_dev_deploy_type
				    reg_afc_dev_type)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->reg_afc_dev_type = reg_afc_dev_type;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_get_afc_soc_dev_type(struct wlan_objmgr_psoc *psoc,
			 enum reg_afc_dev_deploy_type *reg_afc_dev_type)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*reg_afc_dev_type = psoc_priv_obj->reg_afc_dev_type;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_set_eirp_preferred_support(struct wlan_objmgr_psoc *psoc,
			       bool reg_is_eirp_support_preferred)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->reg_is_eirp_support_preferred =
					reg_is_eirp_support_preferred;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_get_eirp_preferred_support(struct wlan_objmgr_psoc *psoc,
			       bool *reg_is_eirp_support_preferred)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*reg_is_eirp_support_preferred =
			psoc_priv_obj->reg_is_eirp_support_preferred;

	return QDF_STATUS_SUCCESS;
}

#endif /* CONFIG_AFC_SUPPORT */

QDF_STATUS
reg_get_cur_6g_client_type(struct wlan_objmgr_pdev *pdev,
			   enum reg_6g_client_type
			   *reg_cur_6g_client_mobility_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (pdev_priv_obj->reg_cur_6g_client_mobility_type >=
	    REG_MAX_CLIENT_TYPE)
		return QDF_STATUS_E_FAILURE;

	*reg_cur_6g_client_mobility_type =
	    pdev_priv_obj->reg_cur_6g_client_mobility_type;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_set_cur_6ghz_client_type(struct wlan_objmgr_pdev *pdev,
			     enum reg_6g_client_type in_6ghz_client_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (in_6ghz_client_type >= REG_MAX_CLIENT_TYPE)
		return QDF_STATUS_E_FAILURE;

	pdev_priv_obj->reg_cur_6g_client_mobility_type = in_6ghz_client_type;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_set_6ghz_client_type_from_target(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev_priv_obj->reg_cur_6g_client_mobility_type =
					pdev_priv_obj->reg_target_client_type;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_get_rnr_tpe_usable(struct wlan_objmgr_pdev *pdev,
				  bool *reg_rnr_tpe_usable)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*reg_rnr_tpe_usable = pdev_priv_obj->reg_rnr_tpe_usable;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS reg_get_unspecified_ap_usable(struct wlan_objmgr_pdev *pdev,
					 bool *reg_unspecified_ap_usable)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*reg_unspecified_ap_usable = pdev_priv_obj->reg_unspecified_ap_usable;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_find_txpower_from_6g_list(qdf_freq_t freq,
			      struct regulatory_channel *chan_list,
			      uint16_t *txpower)
{
	enum channel_enum chan_enum;

	*txpower = 0;

	for (chan_enum = 0; chan_enum < NUM_6GHZ_CHANNELS; chan_enum++) {
		if (chan_list[chan_enum].center_freq == freq) {
			*txpower = chan_list[chan_enum].tx_power;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

bool reg_is_6g_psd_power(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *cur_chan_list;
	enum channel_enum i;

	if (!pdev) {
		reg_err("pdev is NULL");
		return false;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return false;
	}

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	for (i = MIN_6GHZ_CHANNEL; i <= MAX_6GHZ_CHANNEL; i++) {
		if (!(cur_chan_list[i].chan_flags & REGULATORY_CHAN_DISABLED))
			return cur_chan_list[i].psd_flag;
	}

	return false;
}

QDF_STATUS
reg_get_6g_chan_psd_eirp_power(qdf_freq_t freq,
			       struct regulatory_channel *mas_chan_list,
			       uint16_t *eirp_psd_power)
{
	uint16_t i;

	if (!mas_chan_list) {
		reg_err_rl("mas_chan_list is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < NUM_6GHZ_CHANNELS; i++) {
		if (freq == mas_chan_list[i].center_freq) {
			*eirp_psd_power = mas_chan_list[i].psd_eirp;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS reg_get_6g_chan_ap_power(struct wlan_objmgr_pdev *pdev,
				    qdf_freq_t chan_freq, bool *is_psd,
				    uint16_t *tx_power,
				    uint16_t *eirp_psd_power)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *master_chan_list;
	enum reg_6g_ap_type ap_pwr_type;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = reg_get_cur_6g_ap_pwr_type(pdev, &ap_pwr_type);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	master_chan_list = pdev_priv_obj->mas_chan_list_6g_ap[ap_pwr_type];

	reg_find_txpower_from_6g_list(chan_freq, master_chan_list,
				      tx_power);

	*is_psd = reg_is_6g_psd_power(pdev);
	if (*is_psd)
		status = reg_get_6g_chan_psd_eirp_power(chan_freq,
							master_chan_list,
							eirp_psd_power);

	return status;
}

QDF_STATUS reg_get_client_power_for_connecting_ap(struct wlan_objmgr_pdev *pdev,
						  enum reg_6g_ap_type ap_type,
						  qdf_freq_t chan_freq,
						  bool is_psd,
						  uint16_t *tx_power,
						  uint16_t *eirp_psd_power)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum reg_6g_client_type client_type;
	struct regulatory_channel *master_chan_list;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	reg_get_cur_6g_client_type(pdev, &client_type);

	master_chan_list =
		pdev_priv_obj->mas_chan_list_6g_client[ap_type][client_type];

	reg_find_txpower_from_6g_list(chan_freq, master_chan_list,
				      tx_power);

	if (is_psd)
		status = reg_get_6g_chan_psd_eirp_power(chan_freq,
							master_chan_list,
							eirp_psd_power);

	return status;
}

QDF_STATUS reg_get_client_power_for_6ghz_ap(struct wlan_objmgr_pdev *pdev,
					    enum reg_6g_client_type client_type,
					    qdf_freq_t chan_freq,
					    bool *is_psd, uint16_t *tx_power,
					    uint16_t *eirp_psd_power)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum reg_6g_ap_type ap_pwr_type;
	struct regulatory_channel *master_chan_list;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = reg_get_cur_6g_ap_pwr_type(pdev, &ap_pwr_type);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	master_chan_list = pdev_priv_obj->
			mas_chan_list_6g_client[ap_pwr_type][client_type];

	reg_find_txpower_from_6g_list(chan_freq, master_chan_list,
				      tx_power);

	*is_psd = reg_is_6g_psd_power(pdev);
	if (*is_psd)
		status = reg_get_6g_chan_psd_eirp_power(chan_freq,
							master_chan_list,
							eirp_psd_power);

	return status;
}

QDF_STATUS reg_set_ap_pwr_and_update_chan_list(struct wlan_objmgr_pdev *pdev,
					       enum reg_6g_ap_type ap_pwr_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	QDF_STATUS status;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!reg_get_num_rules_of_ap_pwr_type(pdev, ap_pwr_type))
		return QDF_STATUS_E_FAILURE;

	status = reg_set_cur_6g_ap_pwr_type(pdev, ap_pwr_type);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_debug("failed to set AP power type to %d", ap_pwr_type);
		return status;
	}

	reg_compute_pdev_current_chan_list(pdev_priv_obj);

	return QDF_STATUS_SUCCESS;
}
#endif

bool reg_is_regdb_offloaded(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return false;
	}

	return psoc_priv_obj->offload_enabled;
}

QDF_STATUS
reg_set_ext_tpc_supported(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->is_ext_tpc_supported = val;

	return QDF_STATUS_SUCCESS;
}

bool reg_is_ext_tpc_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return  false;
	}

	return psoc_priv_obj->is_ext_tpc_supported;
}

#if defined(CONFIG_BAND_6GHZ)
QDF_STATUS
reg_set_lower_6g_edge_ch_supp(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->is_lower_6g_edge_ch_supported = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_set_disable_upper_6g_edge_ch_supp(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj->is_upper_6g_edge_ch_disabled = val;

	return QDF_STATUS_SUCCESS;
}

bool reg_is_lower_6g_edge_ch_supp(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return  false;
	}

	return psoc_priv_obj->is_lower_6g_edge_ch_supported;
}

bool reg_is_upper_6g_edge_ch_disabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);

	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("psoc reg component is NULL");
		return  false;
	}

	return psoc_priv_obj->is_upper_6g_edge_ch_disabled;
}

static inline bool reg_is_within_range_inclusive(enum channel_enum left,
						 enum channel_enum right,
						 enum channel_enum idx)
{
	return (idx >= left) && (idx <= right);
}

uint16_t reg_convert_enum_to_6g_idx(enum channel_enum ch_idx)
{
	if (!reg_is_within_range_inclusive(MIN_6GHZ_CHANNEL,
					   MAX_6GHZ_CHANNEL,
					   ch_idx))
		return INVALID_CHANNEL;

	return (ch_idx - MIN_6GHZ_CHANNEL);
}

QDF_STATUS
reg_get_superchan_entry(struct wlan_objmgr_pdev *pdev,
			enum channel_enum chan_enum,
			const struct super_chan_info **p_sup_chan_entry)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint16_t sup_idx;

	sup_idx = reg_convert_enum_to_6g_idx(chan_enum);

	if (reg_is_chan_enum_invalid(sup_idx)) {
		reg_debug("super channel idx is invalid for the chan_enum %d",
			  chan_enum);
		return QDF_STATUS_E_INVAL;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("pdev reg component is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (!p_sup_chan_entry) {
		reg_err_rl("p_sup_chan_entry is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (sup_idx >= NUM_6GHZ_CHANNELS) {
		reg_debug("sup_idx is out of bounds");
		return QDF_STATUS_E_INVAL;
	}

	*p_sup_chan_entry = &pdev_priv_obj->super_chan_list[sup_idx];

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef FEATURE_WLAN_CH_AVOID_EXT
/**
 * reg_process_ch_avoid_freq_ext() - Update extended avoid frequencies in
 * psoc_priv_obj
 * @psoc: Pointer to psoc structure
 * @pdev: pointer to pdev object
 *
 * Return: None
 */
static QDF_STATUS
reg_process_ch_avoid_freq_ext(struct wlan_objmgr_psoc *psoc,
			      struct wlan_objmgr_pdev *pdev)
{
	uint32_t i;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	uint8_t start_channel;
	uint8_t end_channel;
	int32_t txpower;
	bool is_valid_txpower;
	struct ch_avoid_freq_type *range;
	enum channel_enum ch_loop;
	enum channel_enum start_ch_idx;
	enum channel_enum end_ch_idx;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint32_t len;
	struct unsafe_ch_list *unsafe_ch_list;
	bool coex_unsafe_nb_user_prefer;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!pdev_priv_obj) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	unsafe_ch_list = &psoc_priv_obj->unsafe_chan_list;
	coex_unsafe_nb_user_prefer =
		psoc_priv_obj->coex_unsafe_chan_nb_user_prefer;

	if (pdev_priv_obj->avoid_chan_ext_list.chan_cnt > 0) {
		len = sizeof(pdev_priv_obj->avoid_chan_ext_list.chan_freq_list);
		pdev_priv_obj->avoid_chan_ext_list.chan_cnt = 0;
		qdf_mem_zero(&pdev_priv_obj->avoid_chan_ext_list.chan_freq_list,
			     len);
	}

	if (unsafe_ch_list->chan_cnt > 0) {
		len = sizeof(unsafe_ch_list->chan_freq_list);
		unsafe_ch_list->chan_cnt = 0;
		qdf_mem_zero(unsafe_ch_list->chan_freq_list, len);
	}

	for (i = 0; i < psoc_priv_obj->avoid_freq_ext_list.ch_avoid_range_cnt;
		i++) {
		if (pdev_priv_obj->avoid_chan_ext_list.chan_cnt >=
		    NUM_CHANNELS) {
			reg_debug("ext avoid channel list full");
			break;
		}

		if (unsafe_ch_list->chan_cnt >= NUM_CHANNELS) {
			reg_warn("LTE Coex unsafe channel list full");
			break;
		}

		start_ch_idx = INVALID_CHANNEL;
		end_ch_idx = INVALID_CHANNEL;
		range = &psoc_priv_obj->avoid_freq_ext_list.avoid_freq_range[i];

		start_channel = reg_freq_to_chan(pdev, range->start_freq);
		end_channel = reg_freq_to_chan(pdev, range->end_freq);
		txpower = range->txpower;
		is_valid_txpower = range->is_valid_txpower;

		reg_debug("start: freq %d, ch %d, end: freq %d, ch %d txpower %d",
			  range->start_freq, start_channel, range->end_freq,
			  end_channel, txpower);

		/* do not process frequency bands that are not mapped to
		 * predefined channels
		 */
		if (start_channel == 0 || end_channel == 0)
			continue;

		for (ch_loop = 0; ch_loop < NUM_CHANNELS;
			ch_loop++) {
			if (REG_CH_TO_FREQ(ch_loop) >= range->start_freq) {
				start_ch_idx = ch_loop;
				break;
			}
		}
		for (ch_loop = 0; ch_loop < NUM_CHANNELS;
			ch_loop++) {
			if (REG_CH_TO_FREQ(ch_loop) >= range->end_freq) {
				end_ch_idx = ch_loop;
				if (REG_CH_TO_FREQ(ch_loop) > range->end_freq)
					end_ch_idx--;
				break;
			}
		}

		if (reg_is_chan_enum_invalid(start_ch_idx) ||
		    reg_is_chan_enum_invalid(end_ch_idx))
			continue;

		for (ch_loop = start_ch_idx; ch_loop <= end_ch_idx;
			ch_loop++) {
			pdev_priv_obj->avoid_chan_ext_list.chan_freq_list
			[pdev_priv_obj->avoid_chan_ext_list.chan_cnt++] =
			REG_CH_TO_FREQ(ch_loop);

			if (coex_unsafe_nb_user_prefer) {
				if (unsafe_ch_list->chan_cnt >=
					NUM_CHANNELS) {
					reg_warn("LTECoex unsafe ch list full");
					break;
				}
				unsafe_ch_list->txpower[
				unsafe_ch_list->chan_cnt] =
					txpower;
				unsafe_ch_list->is_valid_txpower[
				unsafe_ch_list->chan_cnt] =
					is_valid_txpower;
				unsafe_ch_list->chan_freq_list[
				unsafe_ch_list->chan_cnt++] =
					REG_CH_TO_FREQ(ch_loop);
			}

			if (pdev_priv_obj->avoid_chan_ext_list.chan_cnt >=
				NUM_CHANNELS) {
				reg_debug("avoid freq ext list full");
				break;
			}
		}
		/* if start == end for 5G, meanwhile it only have one valid
		 * channel updated, then disable 20M by default around
		 * this center freq. For example input [5805-5805], it
		 * will disable 20Mhz around 5805, then the range change
		 * to [5705-5815], otherwise, not sure about how many width
		 * need to disabled for such case.
		 */
		if ((ch_loop - start_ch_idx) == 1 &&
		    (range->end_freq - range->start_freq == 0) &&
			reg_is_5ghz_ch_freq(range->start_freq)) {
			range->start_freq = range->start_freq - HALF_20MHZ_BW;
			range->end_freq = range->end_freq + HALF_20MHZ_BW;
		}

		for (ch_loop = 0; ch_loop <
			unsafe_ch_list->chan_cnt; ch_loop++) {
			if (ch_loop >= NUM_CHANNELS)
				break;
			reg_debug("Unsafe freq %d",
				  unsafe_ch_list->chan_freq_list[ch_loop]);
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * reg_update_avoid_ch_ext() - Updates the current channel list that block out
 * by extended avoid frequency list
 * @psoc: Pointer to psoc structure
 * @object: Pointer to pdev structure
 * @arg: List of arguments
 *
 * Return: None
 */
static void
reg_update_avoid_ch_ext(struct wlan_objmgr_psoc *psoc,
			void *object, void *arg)
{
	struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)object;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	QDF_STATUS status;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	if (psoc_priv_obj->ch_avoid_ext_ind) {
		status = reg_process_ch_avoid_freq_ext(psoc, pdev);
		if (QDF_IS_STATUS_ERROR(status))
			psoc_priv_obj->ch_avoid_ext_ind = false;
	}

	reg_compute_pdev_current_chan_list(pdev_priv_obj);
	status = reg_send_scheduler_msg_sb(psoc, pdev);

	if (QDF_IS_STATUS_ERROR(status))
		reg_err("channel change msg schedule failed");
}

QDF_STATUS
reg_process_ch_avoid_ext_event(struct wlan_objmgr_psoc *psoc,
			       struct ch_avoid_ind_type *ch_avoid_event)
{
	uint32_t i;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	QDF_STATUS status;
	struct ch_avoid_freq_type *range;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	reg_debug("freq range count %d", ch_avoid_event->ch_avoid_range_cnt);

	qdf_mem_zero(&psoc_priv_obj->avoid_freq_ext_list,
		     sizeof(struct ch_avoid_ind_type));

	for (i = 0; i < ch_avoid_event->ch_avoid_range_cnt; i++) {
		range = &psoc_priv_obj->avoid_freq_ext_list.avoid_freq_range[i];
		range->start_freq =
			ch_avoid_event->avoid_freq_range[i].start_freq;
		range->end_freq =
			ch_avoid_event->avoid_freq_range[i].end_freq;
		range->txpower =
			ch_avoid_event->avoid_freq_range[i].txpower;
		range->is_valid_txpower =
			ch_avoid_event->avoid_freq_range[i].is_valid_txpower;
	}

	psoc_priv_obj->avoid_freq_ext_list.restriction_mask =
		ch_avoid_event->restriction_mask;
	psoc_priv_obj->avoid_freq_ext_list.ch_avoid_range_cnt =
		ch_avoid_event->ch_avoid_range_cnt;

	psoc_priv_obj->ch_avoid_ext_ind = true;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_REGULATORY_SB_ID);

	if (QDF_IS_STATUS_ERROR(status)) {
		reg_err("error taking psoc ref cnt");
		return status;
	}

	status = wlan_objmgr_iterate_obj_list(psoc, WLAN_PDEV_OP,
					      reg_update_avoid_ch_ext,
					      NULL, 1,
					      WLAN_REGULATORY_SB_ID);

	wlan_objmgr_psoc_release_ref(psoc, WLAN_REGULATORY_SB_ID);

	return status;
}

bool reg_check_coex_unsafe_nb_user_prefer(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return false;
	}

	return psoc_priv_obj->coex_unsafe_chan_nb_user_prefer;
}

bool reg_check_coex_unsafe_chan_reg_disable(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		return false;
	}

	return psoc_priv_obj->coex_unsafe_chan_reg_disable;
}
#endif

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
QDF_STATUS reg_send_afc_cmd(struct wlan_objmgr_pdev *pdev,
			    struct reg_afc_resp_rx_ind_info *afc_ind_obj)
{
	uint8_t pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		reg_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops = reg_get_psoc_tx_ops(psoc);
	if (tx_ops->send_afc_ind)
		return tx_ops->send_afc_ind(psoc, pdev_id, afc_ind_obj);

	return QDF_STATUS_E_FAILURE;
}

bool reg_is_afc_power_event_received(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return false;
	}

	return pdev_priv_obj->is_6g_afc_power_event_received;
}

bool reg_is_afc_done(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint32_t chan_flags;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return false;
	}

	chan_flags = reg_get_channel_flags_for_freq(pdev, freq);

	return !(chan_flags & REGULATORY_CHAN_AFC_NOT_DONE);
}

QDF_STATUS reg_get_afc_req_id(struct wlan_objmgr_pdev *pdev, uint64_t *req_id)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*req_id = pdev_priv_obj->afc_request_id;

	return QDF_STATUS_SUCCESS;
}

bool reg_is_afc_expiry_event_received(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("pdev reg component is NULL");
		return false;
	}

	return pdev_priv_obj->is_6g_afc_expiry_event_received;
}

bool reg_is_noaction_on_afc_pwr_evt(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return pdev_priv_obj->is_reg_noaction_on_afc_pwr_evt;
}
#endif

/**
 * struct bw_wireless_modes_pair - structure containing bandwidth and wireless
 * modes corresponding to the bandwidth
 * @ch_width: channel width
 * @wireless_modes: wireless modes bitmap corresponding to @ch_width. This
 * bitmap is a combination of enum values HOST_REGDMN_MODE
 */
struct bw_wireless_modes_pair {
	enum phy_ch_width ch_width;
	uint64_t wireless_modes;
};

/* Mapping of bandwidth to wireless modes */
static const struct bw_wireless_modes_pair bw_wireless_modes_pair_map[] = {
#ifdef WLAN_FEATURE_11BE
	{CH_WIDTH_320MHZ, WIRELESS_320_MODES},
#endif
	{CH_WIDTH_80P80MHZ, WIRELESS_80P80_MODES},
	{CH_WIDTH_160MHZ, WIRELESS_160_MODES},
	{CH_WIDTH_80MHZ, WIRELESS_80_MODES},
	{CH_WIDTH_40MHZ, WIRELESS_40_MODES},
	{CH_WIDTH_20MHZ, WIRELESS_20_MODES},
	{CH_WIDTH_10MHZ, WIRELESS_10_MODES},
	{CH_WIDTH_5MHZ, WIRELESS_5_MODES},
};

QDF_STATUS reg_is_chwidth_supported(struct wlan_objmgr_pdev *pdev,
				    enum phy_ch_width ch_width,
				    bool *is_supported)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	uint64_t wireless_modes;
	uint8_t num_bws, idx;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*is_supported = false;

	wireless_modes = pdev_priv_obj->wireless_modes;
	num_bws = QDF_ARRAY_SIZE(bw_wireless_modes_pair_map);

	for (idx = 0; idx < num_bws; ++idx) {
		if (bw_wireless_modes_pair_map[idx].ch_width == ch_width) {
			*is_supported = !!(wireless_modes &
					   bw_wireless_modes_pair_map[idx].wireless_modes);
			break;
		}
	}

	return QDF_STATUS_SUCCESS;
}

bool reg_is_state_allowed(enum channel_state chan_state)
{
	return !((chan_state == CHANNEL_STATE_INVALID) ||
		 (chan_state == CHANNEL_STATE_DISABLE));
}

static bool
reg_is_freq_idx_enabled_on_cur_chan_list(struct wlan_regulatory_pdev_priv_obj
					 *pdev_priv_obj,
					 enum channel_enum freq_idx)
{
	struct regulatory_channel *cur_chan_list;

	if (freq_idx >= NUM_CHANNELS)
		return false;

	cur_chan_list = pdev_priv_obj->cur_chan_list;

	return !reg_is_chan_disabled_and_not_nol(&cur_chan_list[freq_idx]);
}

QDF_STATUS
reg_get_min_max_bw_on_cur_chan_list(struct wlan_objmgr_pdev *pdev,
				    enum channel_enum freq_idx,
				    uint16_t *min_bw,
				    uint16_t *max_bw)
{
	struct regulatory_channel *cur_chan_list;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (freq_idx >= NUM_CHANNELS)
		return QDF_STATUS_E_FAILURE;

	cur_chan_list = pdev_priv_obj->cur_chan_list;
	if (min_bw)
		*min_bw = cur_chan_list[freq_idx].min_bw;
	if (max_bw)
		*max_bw = cur_chan_list[freq_idx].max_bw;

	return QDF_STATUS_SUCCESS;
}

static enum channel_state
reg_get_chan_state_on_cur_chan_list(struct wlan_regulatory_pdev_priv_obj
				    *pdev_priv_obj,
				    enum channel_enum freq_idx)
{
	struct regulatory_channel *cur_chan_list;
	enum channel_state chan_state;

	if (freq_idx >= NUM_CHANNELS)
		return CHANNEL_STATE_INVALID;

	cur_chan_list = pdev_priv_obj->cur_chan_list;
	chan_state = cur_chan_list[freq_idx].state;

	return chan_state;
}

static enum channel_state
reg_get_chan_state_based_on_nol_flag_cur_chan_list(struct wlan_regulatory_pdev_priv_obj
						   *pdev_priv_obj,
						   enum channel_enum freq_idx)
{
	struct regulatory_channel *cur_chan_list;
	enum channel_state chan_state;

	if (freq_idx >= NUM_CHANNELS)
		return CHANNEL_STATE_INVALID;

	cur_chan_list = pdev_priv_obj->cur_chan_list;
	chan_state = cur_chan_list[freq_idx].state;

	if ((cur_chan_list[freq_idx].nol_chan ||
	     cur_chan_list[freq_idx].nol_history) &&
	     chan_state == CHANNEL_STATE_DISABLE)
		chan_state = CHANNEL_STATE_DFS;

	return chan_state;
}

#ifdef CONFIG_BAND_6GHZ
static inline bool
reg_is_supr_entry_mode_disabled(const struct super_chan_info *super_chan_ent,
				enum supported_6g_pwr_types in_6g_pwr_mode)
{
	return ((super_chan_ent->chan_flags_arr[in_6g_pwr_mode] &
		 REGULATORY_CHAN_DISABLED) &&
		super_chan_ent->state_arr[in_6g_pwr_mode] ==
		CHANNEL_STATE_DISABLE);
}

static bool
reg_is_freq_idx_enabled_on_given_pwr_mode(struct wlan_regulatory_pdev_priv_obj
					  *pdev_priv_obj,
					  enum channel_enum freq_idx,
					  enum supported_6g_pwr_types
					  in_6g_pwr_mode)
{
	const struct super_chan_info *super_chan_ent;
	QDF_STATUS status;

	if (freq_idx >= NUM_CHANNELS)
		return false;

	if (freq_idx < MIN_6GHZ_CHANNEL)
		return reg_is_freq_idx_enabled_on_cur_chan_list(pdev_priv_obj,
								freq_idx);

	status = reg_get_superchan_entry(pdev_priv_obj->pdev_ptr, freq_idx,
					 &super_chan_ent);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_debug("Failed to get super channel entry for freq_idx %d",
			  freq_idx);
		return false;
	}

	/* If the input 6G power mode is best power mode, get the best power
	 * mode type from the super channel entry.
	 */
	if (in_6g_pwr_mode == REG_BEST_PWR_MODE)
		in_6g_pwr_mode = super_chan_ent->best_power_mode;

	return !reg_is_supr_entry_mode_disabled(super_chan_ent, in_6g_pwr_mode);
}

static QDF_STATUS
reg_get_min_max_bw_on_given_pwr_mode(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj,
				     enum channel_enum freq_idx,
				     enum supported_6g_pwr_types
				     in_6g_pwr_mode,
				     uint16_t *min_bw,
				     uint16_t *max_bw)
{
	const struct super_chan_info *super_chan_ent;
	QDF_STATUS status;

	if (freq_idx >= NUM_CHANNELS)
		return QDF_STATUS_E_FAILURE;

	if (freq_idx < MIN_6GHZ_CHANNEL)
		return reg_get_min_max_bw_on_cur_chan_list(
						       pdev_priv_obj->pdev_ptr,
						       freq_idx,
						       min_bw, max_bw);

	status = reg_get_superchan_entry(pdev_priv_obj->pdev_ptr, freq_idx,
					 &super_chan_ent);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_debug("Failed to get super channel entry for freq_idx %d",
			  freq_idx);
		return QDF_STATUS_E_FAILURE;
	}

	/* If the input 6G power mode is best power mode, get the best power
	 * mode type from the super channel entry.
	 */
	if (in_6g_pwr_mode == REG_BEST_PWR_MODE)
		in_6g_pwr_mode = super_chan_ent->best_power_mode;

	if (reg_is_supp_pwr_mode_invalid(in_6g_pwr_mode)) {
		reg_debug("pwr_type invalid");
		return QDF_STATUS_E_FAILURE;
	}

	if (min_bw)
		*min_bw = super_chan_ent->min_bw[in_6g_pwr_mode];
	if (max_bw)
		*max_bw = super_chan_ent->max_bw[in_6g_pwr_mode];

	return QDF_STATUS_SUCCESS;
}

static enum channel_state
reg_get_chan_state_on_given_pwr_mode(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj,
				     enum channel_enum freq_idx,
				     enum supported_6g_pwr_types
				     in_6g_pwr_mode)
{
	const struct super_chan_info *super_chan_ent;
	enum channel_state chan_state;
	QDF_STATUS status;

	if (freq_idx >= NUM_CHANNELS)
		return CHANNEL_STATE_INVALID;

	if (freq_idx < MIN_6GHZ_CHANNEL)
		return reg_get_chan_state_on_cur_chan_list(pdev_priv_obj,
				freq_idx);

	status = reg_get_superchan_entry(pdev_priv_obj->pdev_ptr, freq_idx,
					 &super_chan_ent);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_debug("Failed to get super channel entry for freq_idx %d",
			  freq_idx);
		return CHANNEL_STATE_INVALID;
	}

	/* If the input 6G power mode is best power mode, get the best power
	 * mode type from the super channel entry.
	 */
	if (in_6g_pwr_mode == REG_BEST_PWR_MODE)
		in_6g_pwr_mode = super_chan_ent->best_power_mode;

	if (reg_is_supp_pwr_mode_invalid(in_6g_pwr_mode)) {
		reg_debug("pwr_type invalid");
		return CHANNEL_STATE_INVALID;
	}

	chan_state = super_chan_ent->state_arr[in_6g_pwr_mode];

	return chan_state;
}

enum supported_6g_pwr_types
reg_get_best_6g_pwr_type(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_enum freq_idx;
	enum channel_enum sixg_freq_idx;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("pdev reg component is NULL");
		return REG_INVALID_PWR_MODE;
	}

	freq_idx = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(freq_idx))
		return REG_INVALID_PWR_MODE;

	sixg_freq_idx = reg_convert_enum_to_6g_idx(freq_idx);
	if (reg_is_chan_enum_invalid(sixg_freq_idx) ||
	    sixg_freq_idx >= NUM_6GHZ_CHANNELS)
		return REG_INVALID_PWR_MODE;

	return pdev_priv_obj->super_chan_list[sixg_freq_idx].best_power_mode;
}

static inline bool reg_is_6g_ap_type_invalid(enum reg_6g_ap_type ap_pwr_type)
{
	return ((ap_pwr_type < REG_INDOOR_AP) ||
		(ap_pwr_type > REG_MAX_SUPP_AP_TYPE));
}

enum supported_6g_pwr_types
reg_conv_6g_ap_type_to_supported_6g_pwr_types(enum reg_6g_ap_type ap_pwr_type)
{
	static const enum supported_6g_pwr_types reg_enum_conv[] = {
		[REG_INDOOR_AP] = REG_AP_LPI,
		[REG_STANDARD_POWER_AP] = REG_AP_SP,
		[REG_VERY_LOW_POWER_AP] = REG_AP_VLP,
	};

	if (reg_is_6g_ap_type_invalid(ap_pwr_type))
		return REG_INVALID_PWR_MODE;

	return reg_enum_conv[ap_pwr_type];
}
#else
static inline bool
reg_is_freq_idx_enabled_on_given_pwr_mode(struct wlan_regulatory_pdev_priv_obj
					  *pdev_priv_obj,
					  enum channel_enum freq_idx,
					  enum supported_6g_pwr_types
					  in_6g_pwr_mode)
{
	return reg_is_freq_idx_enabled_on_cur_chan_list(pdev_priv_obj,
							freq_idx);
}

static inline QDF_STATUS
reg_get_min_max_bw_on_given_pwr_mode(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj,
				     enum channel_enum freq_idx,
				     enum supported_6g_pwr_types
				     in_6g_pwr_mode,
				     uint16_t *min_bw,
				     uint16_t *max_bw)
{
	return reg_get_min_max_bw_on_cur_chan_list(pdev_priv_obj->pdev_ptr,
						   freq_idx,
						   min_bw, max_bw);
}

static inline enum channel_state
reg_get_chan_state_on_given_pwr_mode(struct wlan_regulatory_pdev_priv_obj
				     *pdev_priv_obj,
				     enum channel_enum freq_idx,
				     enum supported_6g_pwr_types
				     in_6g_pwr_mode)
{
	return reg_get_chan_state_on_cur_chan_list(pdev_priv_obj,
						   freq_idx);
}
#endif /* CONFIG_BAND_6GHZ */

bool
reg_is_freq_enabled(struct wlan_objmgr_pdev *pdev,
		    qdf_freq_t freq,
		    enum supported_6g_pwr_types in_6g_pwr_mode)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	enum channel_enum freq_idx;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return false;
	}

	freq_idx = reg_get_chan_enum_for_freq(freq);

	if (reg_is_chan_enum_invalid(freq_idx))
		return false;

	return reg_is_freq_idx_enabled(pdev, freq_idx, in_6g_pwr_mode);
}

bool reg_is_freq_idx_enabled(struct wlan_objmgr_pdev *pdev,
			     enum channel_enum freq_idx,
			     enum supported_6g_pwr_types in_6g_pwr_mode)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return false;
	}

	if (freq_idx < MIN_6GHZ_CHANNEL)
		return reg_is_freq_idx_enabled_on_cur_chan_list(pdev_priv_obj,
								freq_idx);

	switch (in_6g_pwr_mode) {
	case REG_CURRENT_PWR_MODE:
		return reg_is_freq_idx_enabled_on_cur_chan_list(pdev_priv_obj,
								freq_idx);

	case REG_BEST_PWR_MODE:
	default:
		return reg_is_freq_idx_enabled_on_given_pwr_mode(pdev_priv_obj,
								 freq_idx,
								 in_6g_pwr_mode
								 );
	}
}

QDF_STATUS reg_get_min_max_bw_reg_chan_list(struct wlan_objmgr_pdev *pdev,
					    enum channel_enum freq_idx,
					    enum supported_6g_pwr_types
					    in_6g_pwr_mode,
					    uint16_t *min_bw,
					    uint16_t *max_bw)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (freq_idx < MIN_6GHZ_CHANNEL)
		return reg_get_min_max_bw_on_cur_chan_list(
							pdev,
							freq_idx,
							min_bw, max_bw);

	switch (in_6g_pwr_mode) {
	case REG_CURRENT_PWR_MODE:
		return reg_get_min_max_bw_on_cur_chan_list(
							pdev,
							freq_idx,
							min_bw, max_bw);

	case REG_BEST_PWR_MODE:
	default:
		return reg_get_min_max_bw_on_given_pwr_mode(pdev_priv_obj,
							    freq_idx,
							    in_6g_pwr_mode,
							    min_bw, max_bw);
	}
}

enum channel_state reg_get_chan_state(struct wlan_objmgr_pdev *pdev,
				      enum channel_enum freq_idx,
				      enum supported_6g_pwr_types
				      in_6g_pwr_mode,
				      bool treat_nol_chan_as_disabled)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return CHANNEL_STATE_INVALID;
	}

	if (freq_idx < MIN_6GHZ_CHANNEL) {
		if (treat_nol_chan_as_disabled)
			return reg_get_chan_state_on_cur_chan_list(pdev_priv_obj,
								   freq_idx);
		else
			return reg_get_chan_state_based_on_nol_flag_cur_chan_list(
								pdev_priv_obj,
								freq_idx);
	}

	switch (in_6g_pwr_mode) {
	case REG_CURRENT_PWR_MODE:
		return reg_get_chan_state_on_cur_chan_list(pdev_priv_obj,
							   freq_idx);

	case REG_BEST_PWR_MODE:
	default:
		return reg_get_chan_state_on_given_pwr_mode(pdev_priv_obj,
							    freq_idx,
							    in_6g_pwr_mode
							   );
	}
}

#ifdef WLAN_FEATURE_11BE
enum phy_ch_width reg_find_chwidth_from_bw(uint16_t bw)
{
	switch (bw) {
	case BW_5_MHZ:
		return CH_WIDTH_5MHZ;
	case BW_10_MHZ:
		return CH_WIDTH_10MHZ;
	case BW_20_MHZ:
		return CH_WIDTH_20MHZ;
	case BW_40_MHZ:
		return CH_WIDTH_40MHZ;
	case BW_80_MHZ:
		return CH_WIDTH_80MHZ;
	case BW_160_MHZ:
		return CH_WIDTH_160MHZ;
	case BW_320_MHZ:
		return CH_WIDTH_320MHZ;
	default:
		return CH_WIDTH_INVALID;
	}
}
#else
enum phy_ch_width reg_find_chwidth_from_bw(uint16_t bw)
{
	switch (bw) {
	case BW_5_MHZ:
		return CH_WIDTH_5MHZ;
	case BW_10_MHZ:
		return CH_WIDTH_10MHZ;
	case BW_20_MHZ:
		return CH_WIDTH_20MHZ;
	case BW_40_MHZ:
		return CH_WIDTH_40MHZ;
	case BW_80_MHZ:
		return CH_WIDTH_80MHZ;
	case BW_160_MHZ:
		return CH_WIDTH_160MHZ;
	default:
		return CH_WIDTH_INVALID;
	}
}
#endif

#ifdef CONFIG_BAND_6GHZ
qdf_freq_t reg_get_thresh_priority_freq(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return 0;
	}

	return pdev_priv_obj->reg_6g_thresh_priority_freq;
}

/**
 * reg_get_eirp_from_psd_and_reg_max_eirp() - Get the EIRP by the computing the
 * minimum(max regulatory EIRP, EIRP computed from regulatory PSD)
 * @pdev: Pointer to pdev
 * @master_chan_list: Pointer to master_chan_list
 * @freq: Frequency in mhz
 * @bw: Bandwidth in mhz
 * @reg_eirp_pwr: Pointer to reg_eirp_pwr
 *
 * Return: Void
 */
static void
reg_get_eirp_from_psd_and_reg_max_eirp(struct wlan_objmgr_pdev *pdev,
				       struct regulatory_channel *mas_chan_list,
				       qdf_freq_t freq,
				       uint16_t bw,
				       uint16_t *reg_eirp_pwr)
{
	int16_t eirp_from_psd = 0, psd = 0;

	reg_get_6g_chan_psd_eirp_power(freq, mas_chan_list, &psd);
	reg_psd_2_eirp(pdev, psd, bw, &eirp_from_psd);
	*reg_eirp_pwr = QDF_MIN(*reg_eirp_pwr, eirp_from_psd);
}

/**
 * reg_get_mas_chan_list_for_lookup() - Get the AP or client master_chan_list
 * based on the is_client_list_lookup_needed flag
 * @pdev: Pointer to pdev
 * @master_chan_list: Pointer to master_chan_list
 * @ap_pwr_type: AP Power type
 * @is_client_list_lookup_needed: Boolean to indicate if client list lookup is
 * needed
 * @client_type: Client power type
 *
 * Return: Void
 */
static void
reg_get_mas_chan_list_for_lookup(struct wlan_objmgr_pdev *pdev,
				 struct regulatory_channel **master_chan_list,
				 enum reg_6g_ap_type ap_pwr_type,
				 bool is_client_list_lookup_needed,
				 enum reg_6g_client_type client_type)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return;
	}

	if (client_type > REG_MAX_CLIENT_TYPE) {
		reg_err("Invalid client type");
		return;
	}

	if (is_client_list_lookup_needed)
		*master_chan_list =
			pdev_priv_obj->mas_chan_list_6g_client[ap_pwr_type]
								[client_type];
	else
		*master_chan_list =
			pdev_priv_obj->mas_chan_list_6g_ap[ap_pwr_type];
}

/**
 * reg_get_eirp_for_non_sp() -  For the given power mode, using the bandwidth
 * and psd(from master channel entry), calculate an EIRP value. The minimum
 * of calculated EIRP and regulatory max EIRP is returned.
 * @pdev: Pointer to pdev
 * @freq: Frequency in mhz
 * @bw: Bandwidth in mhz
 * @ap_pwr_type: AP Power type
 * @is_client_list_lookup_needed: Boolean to indicate if client list lookup is
 * needed
 * @client_type: Client power type
 *
 * Return: EIRP
 */
static uint8_t
reg_get_eirp_for_non_sp(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			uint16_t bw, enum reg_6g_ap_type ap_pwr_type,
			bool is_client_list_lookup_needed,
			enum reg_6g_client_type client_type)
{
	bool is_psd;
	struct regulatory_channel *master_chan_list = NULL;
	uint16_t txpower = 0;

	if (!((ap_pwr_type == REG_INDOOR_AP) ||
	      (ap_pwr_type == REG_VERY_LOW_POWER_AP))) {
		reg_err("Only LPI and VLP are supported in this function ");
		return 0;
	}

	reg_get_mas_chan_list_for_lookup(pdev, &master_chan_list, ap_pwr_type,
					 is_client_list_lookup_needed,
					 client_type);
	if (!master_chan_list) {
		reg_err("master_chan_list is NULL");
		return 0;
	}

	is_psd = reg_is_6g_psd_power(pdev);
	reg_find_txpower_from_6g_list(freq, master_chan_list, &txpower);

	if (is_psd)
		reg_get_eirp_from_psd_and_reg_max_eirp(pdev,
						       master_chan_list,
						       freq, bw,
						       &txpower);

	return txpower;
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * reg_compute_6g_center_freq_from_cfi() - Given the IEEE value of the
 * 6 GHz center frequency, find the 6 GHz center frequency.
 * @ieee_6g_cfi: IEEE value of 6 GHz cfi
 * Return: Center frequency in MHz
 */
static qdf_freq_t
reg_compute_6g_center_freq_from_cfi(uint8_t ieee_6g_cfi)
{
	return (SIXG_START_FREQ + ieee_6g_cfi * FREQ_TO_CHAN_SCALE);
}

#ifdef WLAN_FEATURE_11BE
/**
 * reg_is_320_opclass: Find out if the opclass computed from freq and
 * width of 320 is same as the input op_class.
 * @freq: Frequency in MHz
 * @in_opclass: Input Opclass number
 * Return: true if opclass is 320 supported, false otherwise.
 */
static bool reg_is_320_opclass(qdf_freq_t freq, uint8_t in_opclass)
{
	uint8_t local_op_class =
		reg_dmn_get_opclass_from_freq_width(NULL, freq, BW_320_MHZ,
						    BIT(BEHAV_NONE));
	return (in_opclass == local_op_class);
}
#else
static inline bool reg_is_320_opclass(qdf_freq_t freq, uint8_t op_class)
{
	return false;
}
#endif

/**
 * reg_find_eirp_in_afc_eirp_obj() - Get eirp power from the AFC eirp object
 * based on the channel center frequency and operating class
 * @pdev: Pointer to pdev
 * @eirp_obj: Pointer to eirp_obj
 * @freq: Frequency in MHz
 * @cen320: 320 MHz band center frequency
 * @op_class: Operating class
 *
 * Return: EIRP power
 */
static uint8_t reg_find_eirp_in_afc_eirp_obj(struct wlan_objmgr_pdev *pdev,
					     struct chan_eirp_obj *eirp_obj,
					     qdf_freq_t freq,
					     qdf_freq_t cen320,
					     uint8_t op_class)
{
	uint8_t k;
	uint8_t subchannels[NUM_20_MHZ_CHAN_IN_320_MHZ_CHAN];
	uint8_t nchans;

	if (reg_is_320_opclass(freq, op_class)) {
		qdf_freq_t cfi_freq =
			reg_compute_6g_center_freq_from_cfi(eirp_obj->cfi);

		if (cfi_freq == cen320)
			return eirp_obj->eirp_power / EIRP_PWR_SCALE;

		return 0;
	}

	nchans = reg_get_subchannels_for_opclass(eirp_obj->cfi,
						 op_class,
						 subchannels);

	for (k = 0; k < nchans; k++)
		if (reg_chan_band_to_freq(pdev, subchannels[k],
					  BIT(REG_BAND_6G)) == freq)
			return eirp_obj->eirp_power / EIRP_PWR_SCALE;

	return 0;
}

/**
 * reg_find_eirp_in_afc_chan_obj() - Get eirp power from the AFC channel
 * object based on the channel center frequency and operating class
 * @pdev: Pointer to pdev
 * @chan_obj: Pointer to chan_obj
 * @freq: Frequency in MHz
 * @cen320: 320 MHz band center frequency
 * @op_class: Operating class
 *
 * Return: EIRP power
 */
static uint8_t reg_find_eirp_in_afc_chan_obj(struct wlan_objmgr_pdev *pdev,
					     struct afc_chan_obj *chan_obj,
					     qdf_freq_t freq,
					     qdf_freq_t cen320,
					     uint8_t op_class)
{
	uint8_t j;

	if (chan_obj->global_opclass != op_class)
		return 0;

	for (j = 0; j < chan_obj->num_chans; j++) {
		uint8_t afc_eirp;
		struct chan_eirp_obj *eirp_obj = &chan_obj->chan_eirp_info[j];

		afc_eirp = reg_find_eirp_in_afc_eirp_obj(pdev, eirp_obj,
							 freq, cen320,
							 op_class);

		if (afc_eirp)
			return afc_eirp;
	}

	return 0;
}

/**
 * reg_is_chan_punc() - Validates the input puncture pattern.
 * @in_punc_pattern: Input puncture pattern
 * @bw: Channel bandwidth in MHz
 *
 * If the in_punc_pattern has none of the subchans punctured, channel
 * is not considered as punctured. Also, if the input puncture bitmap
 * is invalid, do not consider the channel as punctured.
 *
 * Return: true if channel is punctured, false otherwise.
 */
#ifdef WLAN_FEATURE_11BE
static bool
reg_is_chan_punc(uint16_t in_punc_pattern, uint16_t bw)
{
	enum phy_ch_width ch_width = reg_find_chwidth_from_bw(bw);

	if (in_punc_pattern == NO_SCHANS_PUNC ||
	    !reg_is_punc_bitmap_valid(ch_width, in_punc_pattern))
		return false;

	return true;
}
#else
static inline bool
reg_is_chan_punc(uint16_t in_punc_pattern, uint16_t bw)
{
	return false;
}
#endif

/**
 * reg_find_non_punctured_bw() - Given the input puncture pattern and the
 * total BW of the channel, find the non-punctured bandwidth.
 * @bw: Total bandwidth of the channel
 * @in_punc_pattern: Input puncture pattern
 *
 * Return: non-punctured bw in MHz
 */
static uint16_t
reg_find_non_punctured_bw(uint16_t bw,  uint16_t in_punc_pattern)
{
	uint8_t num_punc_bw = 0;

	while (in_punc_pattern) {
		if (in_punc_pattern & 1)
			++num_punc_bw;
		in_punc_pattern >>= 1;
	}

	if (bw <= num_punc_bw * 20)
		return 0;

	return (bw - num_punc_bw * 20);
}

/**
 * reg_get_sp_eirp_for_punc_chans() - Find the standard EIRP power for
 * punctured channels.
 * @pdev: Pointer to struct wlan_objmgr_pdev
 * @freq: Frequency in MHz
 * @cen320: Center of 320 MHz channel in Mhz
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Input puncture pattern
 * @reg_sp_eirp_pwr: Regulatory defined SP power for the input frequency
 *
 * Return: Regulatory and AFC intersected SP power of punctured channel
 */
static uint8_t
reg_get_sp_eirp_for_punc_chans(struct wlan_objmgr_pdev *pdev,
			       qdf_freq_t freq,
			       qdf_freq_t cen320,
			       uint16_t bw,
			       uint16_t in_punc_pattern,
			       uint16_t reg_sp_eirp_pwr)
{
	int16_t min_psd = 0;
	uint16_t afc_eirp_pwr = 0;
	uint16_t non_punc_bw;
	struct wlan_lmac_if_reg_tx_ops *reg_tx_ops;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc)
		return 0;

	reg_tx_ops = reg_get_psoc_tx_ops(psoc);

	if (!reg_tx_ops->reg_get_min_psd)
		return 0;

	/* min_psd will be calculated here */
	status = reg_tx_ops->reg_get_min_psd(pdev, freq, cen320,
					     in_punc_pattern, bw,
					     &min_psd);
	if (status != QDF_STATUS_SUCCESS) {
		reg_debug("Could not derive min_psd power for width %u, freq; %d, cen320: %d, in_punc: 0x%x\n",
			  bw, freq, cen320, in_punc_pattern);
		return 0;
	}

	non_punc_bw = reg_find_non_punctured_bw(bw, in_punc_pattern);

	if (reg_psd_2_eirp(pdev, min_psd, non_punc_bw, &afc_eirp_pwr) !=
	    QDF_STATUS_SUCCESS) {
		reg_debug("Could not derive EIRP power for width %u, min_psd: %d\n", non_punc_bw, min_psd);
		return 0;
	}

	reg_debug("freq = %u, bw: %u, cen320: %u, punc_pattern: 0x%x "
		  "reg_sp_eirp: %d, min_psd: %d, non_punc_bw: %u, afc_eirp_pwr: %d\n",
		  freq, bw, cen320, in_punc_pattern, reg_sp_eirp_pwr, min_psd,
		  non_punc_bw, afc_eirp_pwr);

	if (afc_eirp_pwr)
		return QDF_MIN(afc_eirp_pwr, reg_sp_eirp_pwr);

	return 0;
}

/**
 * reg_get_sp_eirp() - For the given power mode, using the bandwidth, find the
 * corresponding EIRP values from the afc power info array. The minimum of found
 * EIRP and regulatory max EIRP is returned
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @cen320: 320 MHz band center frequency
 * @bw: Bandwidth in MHz
 * @in_punc_pattern: Input puncture pattern
 * @is_client_list_lookup_needed: Boolean to indicate if client list lookup is
 * needed
 * @client_type: Client power type
 *
 * Return: EIRP
 */
static uint8_t reg_get_sp_eirp(struct wlan_objmgr_pdev *pdev,
			       qdf_freq_t freq,
			       qdf_freq_t cen320,
			       uint16_t bw,
			       uint16_t in_punc_pattern,
			       bool is_client_list_lookup_needed,
			       enum reg_6g_client_type client_type)
{
	uint8_t i, op_class = 0, chan_num = 0, afc_eirp_pwr = 0;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel *sp_master_chan_list = NULL;
	struct reg_fw_afc_power_event *power_info;
	uint16_t reg_sp_eirp_pwr = 0;
	bool is_psd;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return 0;
	}

	if (!reg_is_afc_power_event_received(pdev))
		return 0;

	sp_master_chan_list =
		pdev_priv_obj->mas_chan_list_6g_ap[REG_STANDARD_POWER_AP];
	reg_find_txpower_from_6g_list(freq, sp_master_chan_list,
				      &reg_sp_eirp_pwr);

	if (!reg_sp_eirp_pwr)
		return 0;

	if (reg_is_chan_punc(in_punc_pattern, bw)) {
		reg_info("Computing SP EIRP with puncturing info");
		return reg_get_sp_eirp_for_punc_chans(pdev, freq, cen320, bw,
						      in_punc_pattern,
						      reg_sp_eirp_pwr);
	}

	power_info = pdev_priv_obj->power_info;
	if (!power_info) {
		reg_err("power_info is NULL");
		return 0;
	}

	reg_freq_width_to_chan_op_class(pdev,
					freq,
					bw,
					true,
					BIT(BEHAV_NONE),
					&op_class,
					&chan_num);
	reg_get_mas_chan_list_for_lookup(pdev, &sp_master_chan_list,
					 REG_STANDARD_POWER_AP,
					 is_client_list_lookup_needed,
					 client_type);
	if (!sp_master_chan_list) {
		reg_err("sp_master_chan_list is NULL");
		return 0;
	}

	reg_find_txpower_from_6g_list(freq, sp_master_chan_list,
				      &reg_sp_eirp_pwr);

	if (!reg_sp_eirp_pwr)
		return 0;

	for (i = 0; i < power_info->num_chan_objs; i++) {
		struct afc_chan_obj *chan_obj = &power_info->afc_chan_info[i];

		afc_eirp_pwr = reg_find_eirp_in_afc_chan_obj(pdev,
							     chan_obj,
							     freq,
							     cen320,
							     op_class);
		if (afc_eirp_pwr)
			break;
	}

	is_psd = reg_is_6g_psd_power(pdev);
	if (is_psd)
		reg_get_eirp_from_psd_and_reg_max_eirp(pdev,
						       sp_master_chan_list,
						       freq, bw,
						       &reg_sp_eirp_pwr);

	if (afc_eirp_pwr)
		return QDF_MIN(afc_eirp_pwr, reg_sp_eirp_pwr);

	return 0;
}
#else
static uint8_t reg_get_sp_eirp(struct wlan_objmgr_pdev *pdev,
			       qdf_freq_t freq,
			       qdf_freq_t cen320,
			       uint16_t bw,
			       uint16_t in_punc_pattern,
			       bool is_client_list_lookup_needed,
			       enum reg_6g_client_type client_type)
{
	struct regulatory_channel *sp_master_chan_list = NULL;
	uint16_t reg_sp_eirp_pwr = 0;
	bool is_psd;

	reg_get_mas_chan_list_for_lookup(pdev, &sp_master_chan_list,
					 REG_STANDARD_POWER_AP,
					 is_client_list_lookup_needed,
					 client_type);
	if (!sp_master_chan_list) {
		reg_err("sp_master_chan_list is NULL");
		return 0;
	}

	reg_find_txpower_from_6g_list(freq, sp_master_chan_list,
				      &reg_sp_eirp_pwr);
	is_psd = reg_is_6g_psd_power(pdev);
	if (is_psd)
		reg_get_eirp_from_psd_and_reg_max_eirp(pdev,
						       sp_master_chan_list,
						       freq, bw,
						       &reg_sp_eirp_pwr);

	return reg_sp_eirp_pwr;
}
#endif

/**
 * reg_get_best_pwr_mode_from_eirp_list() - Get best power mode from the input
 * EIRP list
 * @eirp_list: EIRP list
 * @size: Size of eirp list
 *
 * Return: Best power mode
 */
static enum reg_6g_ap_type
reg_get_best_pwr_mode_from_eirp_list(uint8_t *eirp_list, uint8_t size)
{
	uint8_t max = 0, i;
	enum reg_6g_ap_type best_pwr_mode = REG_INDOOR_AP;

	for (i = 0; i < size; i++) {
		if (eirp_list[i] > max) {
			max = eirp_list[i];
			best_pwr_mode = i;
		}
	}

	return best_pwr_mode;
}

uint8_t reg_get_eirp_pwr(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			 qdf_freq_t cen320,
			 uint16_t bw, enum reg_6g_ap_type ap_pwr_type,
			 uint16_t in_punc_pattern,
			 bool is_client_list_lookup_needed,
			 enum reg_6g_client_type client_type)
{
	if (ap_pwr_type == REG_STANDARD_POWER_AP)
		return reg_get_sp_eirp(pdev, freq, cen320, bw, in_punc_pattern,
				       is_client_list_lookup_needed,
				       client_type);

	return reg_get_eirp_for_non_sp(pdev, freq, bw, ap_pwr_type,
				       is_client_list_lookup_needed,
				       client_type);
}

enum reg_6g_ap_type reg_get_best_pwr_mode(struct wlan_objmgr_pdev *pdev,
					  qdf_freq_t freq,
					  qdf_freq_t cen320,
					  uint16_t bw,
					  uint16_t in_punc_pattern)
{
	uint8_t eirp_list[REG_MAX_SUPP_AP_TYPE + 1];
	enum reg_6g_ap_type ap_pwr_type;

	for (ap_pwr_type = REG_INDOOR_AP; ap_pwr_type <= REG_VERY_LOW_POWER_AP;
	     ap_pwr_type++)
		eirp_list[ap_pwr_type] =
				reg_get_eirp_pwr(pdev, freq, cen320, bw,
						 ap_pwr_type, in_punc_pattern,
						 false,
						 REG_MAX_CLIENT_TYPE);

	return reg_get_best_pwr_mode_from_eirp_list(eirp_list,
						    REG_MAX_SUPP_AP_TYPE + 1);
}
#endif

QDF_STATUS reg_get_regd_rules(struct wlan_objmgr_pdev *pdev,
			      struct reg_rule_info *reg_rules)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	if (!pdev) {
		reg_err("pdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spin_lock_bh(&pdev_priv_obj->reg_rules_lock);
	qdf_mem_copy(reg_rules, &pdev_priv_obj->reg_rules,
		     sizeof(struct reg_rule_info));
	qdf_spin_unlock_bh(&pdev_priv_obj->reg_rules_lock);

	return QDF_STATUS_SUCCESS;
}

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
bool
reg_is_sup_chan_entry_afc_done(struct wlan_objmgr_pdev *pdev,
			       enum channel_enum chan_idx,
			       enum supported_6g_pwr_types in_6g_pwr_mode)
{
	const struct super_chan_info *super_chan_ent;
	QDF_STATUS status;

	status = reg_get_superchan_entry(pdev, chan_idx,
					 &super_chan_ent);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_debug("Failed to get super channel entry for chan_idx %d",
			  chan_idx);
		return false;
	}

	if (in_6g_pwr_mode == REG_BEST_PWR_MODE)
		in_6g_pwr_mode = super_chan_ent->best_power_mode;

	if (in_6g_pwr_mode != REG_AP_SP)
		return false;

	return !(super_chan_ent->chan_flags_arr[in_6g_pwr_mode] &
		 REGULATORY_CHAN_AFC_NOT_DONE);
}

bool
reg_is_6ghz_freq_txable(struct wlan_objmgr_pdev *pdev,
			qdf_freq_t freq,
			enum supported_6g_pwr_types in_6ghz_pwr_mode)
{
	bool is_freq_enabled;
	enum reg_afc_dev_deploy_type reg_afc_deploy_type;

	is_freq_enabled = reg_is_freq_enabled(pdev, freq, in_6ghz_pwr_mode);
	if (!is_freq_enabled)
		return false;

	reg_get_afc_dev_deploy_type(pdev, &reg_afc_deploy_type);

	return (reg_afc_deploy_type != AFC_DEPLOYMENT_OUTDOOR) ||
		reg_is_afc_done(pdev, freq);
}
#endif

#ifdef CONFIG_BAND_6GHZ
QDF_STATUS
reg_display_super_chan_list(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct super_chan_info *super_chan_list;
	uint8_t i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err_rl("pdev reg component is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	super_chan_list = pdev_priv_obj->super_chan_list;
	for (i = 0; i < NUM_6GHZ_CHANNELS; i++) {
		struct super_chan_info *chan_info = &super_chan_list[i];
		struct regulatory_channel  cur_chan_list =
			pdev_priv_obj->cur_chan_list[MIN_6GHZ_CHANNEL + i];
		uint8_t j;

		qdf_print("Freq = %d\tPower types = 0x%x\t"
			  "Best power mode = 0x%x\n",
			  cur_chan_list.center_freq, chan_info->power_types,
			  chan_info->best_power_mode);
		for (j = REG_AP_LPI; j <= REG_CLI_SUB_VLP; j++) {
			bool afc_not_done_bit;

			afc_not_done_bit = chan_info->chan_flags_arr[j] &
						REGULATORY_CHAN_AFC_NOT_DONE;
			qdf_print("Power mode = %d\tPSD flag = %d\t"
				  "PSD power = %d\tEIRP power = %d\t"
				  "Chan flags = 0x%x\tChannel state = %d\t"
				  "Min bw = %d\tMax bw = %d\t"
				  "AFC_NOT_DONE = %d\n",
				  j, chan_info->reg_chan_pwr[j].psd_flag,
				  chan_info->reg_chan_pwr[j].psd_eirp,
				  chan_info->reg_chan_pwr[j].tx_power,
				  chan_info->chan_flags_arr[j],
				  chan_info->state_arr[j],
				  chan_info->min_bw[j], chan_info->max_bw[j],
				  afc_not_done_bit);
		}
	}

	return QDF_STATUS_SUCCESS;
}

#if defined(CONFIG_AFC_SUPPORT) && defined(CONFIG_BAND_6GHZ)
QDF_STATUS
reg_get_afc_freq_range_and_psd_limits(struct wlan_objmgr_pdev *pdev,
				      uint8_t num_freq_obj,
				      struct afc_freq_obj *afc_freq_info)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct reg_fw_afc_power_event *power_info;
	uint8_t i;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!reg_is_afc_power_event_received(pdev)) {
		reg_err("afc power event is not received\n");
		return QDF_STATUS_E_FAILURE;
	}

	power_info = pdev_priv_obj->power_info;
	if (!power_info) {
		reg_err("power_info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!num_freq_obj) {
		reg_err("num freq objs cannot be zero");
		return QDF_STATUS_E_FAILURE;
	}

	if (!afc_freq_info)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < num_freq_obj; i++) {
		struct afc_freq_obj *reg_afc_info =
			&power_info->afc_freq_info[i];

		afc_freq_info[i].low_freq = reg_afc_info->low_freq;
		afc_freq_info[i].high_freq = reg_afc_info->high_freq;
		afc_freq_info[i].max_psd  = reg_afc_info->max_psd;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
reg_get_num_afc_freq_obj(struct wlan_objmgr_pdev *pdev, uint8_t *num_freq_obj)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct reg_fw_afc_power_event *power_info;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!reg_is_afc_power_event_received(pdev)) {
		reg_err("afc power event is not received\n");
		return QDF_STATUS_E_FAILURE;
	}

	power_info = pdev_priv_obj->power_info;
	if (!power_info) {
		reg_err("power_info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!power_info->num_freq_objs) {
		reg_err("num freq objs cannot be zero");
		return QDF_STATUS_E_FAILURE;
	}

	*num_freq_obj = power_info->num_freq_objs;

	return QDF_STATUS_SUCCESS;
}
#endif

#endif

#ifdef CONFIG_AFC_SUPPORT
QDF_STATUS reg_set_afc_power_event_received(struct wlan_objmgr_pdev *pdev,
					    bool val)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);
	if (!pdev_priv_obj) {
		reg_err("pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pdev_priv_obj->is_6g_afc_power_event_received = val;

	return QDF_STATUS_SUCCESS;
}
#endif
