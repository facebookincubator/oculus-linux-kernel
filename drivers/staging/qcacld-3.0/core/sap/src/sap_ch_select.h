/*
 * Copyright (c) 2012-2015, 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(__SAP_CH_SELECT_H)
#define __SAP_CH_SELECT_H

/*===========================================================================

				sapChSelect.h

   OVERVIEW:

   This software unit holds the implementation of the WLAN SAP modules
   functions for channel selection.

   DEPENDENCIES:

   Are listed for each API below.
   ===========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "ani_global.h"
/*--------------------------------------------------------------------------
   defines and enum
   --------------------------------------------------------------------------*/

#define SAPDFS_NORMALISE_1000      (1000/9)     /* Case of spec20 with channel diff = 0 */
#define SOFTAP_MIN_RSSI         (-100)
#define SOFTAP_MAX_RSSI         (0)
#define SOFTAP_MIN_COUNT        (0)
#define SOFTAP_MAX_COUNT        (60)

#define SOFTAP_MIN_NF           (-120)
#define SOFTAP_MAX_NF           (-60)
#define SOFTAP_MIN_CHNFREE      (0)
#define SOFTAP_MAX_CHNFREE      (1)
#define SOFTAP_MIN_TXPWR        (0)
#define SOFTAP_MAX_TXPWR        (63)

#define REG_MAX_EIRP_POWER 36
#define REG_MIN_EIRP_POWER 14

#define SAP_NORMALISE_ACS_WEIGHT 5

/* In HT40/VHT80, Effect of primary Channel RSSi on Subband1 */
#define SAP_SUBBAND1_RSSI_EFFECT_PRIMARY  (-20)
/* In VHT80, Effect of primary Channel RSSI on Subband2 */
#define SAP_SUBBAND2_RSSI_EFFECT_PRIMARY  (-30)
/* In VHT80, Effect of Primary Channel RSSI on Subband3 */
#define SAP_SUBBAND3_RSSI_EFFECT_PRIMARY  (-40)
/* In VHT80, Effect of Primary Channel RSSI on Subband4 */
#define SAP_SUBBAND4_RSSI_EFFECT_PRIMARY  (-50)
/* In VHT80, Effect of Primary Channel RSSI on Subband5 */
#define SAP_SUBBAND5_RSSI_EFFECT_PRIMARY  (-60)
/* In VHT80, Effect of Primary Channel RSSI on Subband6 */
#define SAP_SUBBAND6_RSSI_EFFECT_PRIMARY  (-70)
/* In VHT80, Effect of Primary Channel RSSI on Subband7 */
#define SAP_SUBBAND7_RSSI_EFFECT_PRIMARY  (-80)

#define SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY      (-10)     /* In 2.4GHZ, Effect of Primary  Channel RSSI on First Overlapping Channel */
#define SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY        (-20)     /* In 2.4GHZ, Effect of Primary  Channel RSSI on Second Overlapping Channel */
#define SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY      (-30)     /* In 2.4GHZ, Effect of Primary  Channel RSSI on Third Overlapping Channel */
#define SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY     (-40)     /* In 2.4GHZ, Effect of Primary  Channel RSSI on Fourth Overlapping Channel */

typedef enum {
	CHANNEL_1 = 1,
	CHANNEL_2,
	CHANNEL_3,
	CHANNEL_4,
	CHANNEL_5,
	CHANNEL_6,
	CHANNEL_7,
	CHANNEL_8,
	CHANNEL_9,
	CHANNEL_10,
	CHANNEL_11,
	CHANNEL_12,
	CHANNEL_13,
	CHANNEL_14
} tSapChannel;
#endif /* if !defined __SAP_CH_SELECT_H */
