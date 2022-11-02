/*
 * ak4331.h  --  audio driver for AK4331
 *
 * Copyright (C) 2018-2019 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision      DS ver.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      18/01/12	    1.0           00
 *                      19/01/10	    1.1           00
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  kernel version: 4.4
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _AK4331_H
#define _AK4331_H

/* Enable: 48kHz system, Disable: 44.1kHz system */
#define SRC_OUT_FS_48K

#define AK4331_00_POWER_MANAGEMENT1			0x00
#define AK4331_01_POWER_MANAGEMENT2			0x01
#define AK4331_02_POWER_MANAGEMENT3			0x02
#define AK4331_03_POWER_MANAGEMENT4			0x03
#define AK4331_04_OUTPUT_MODE_SETTING		0x04
#define AK4331_05_CLOCK_MODE_SELECT			0x05
#define AK4331_06_DIGITAL_FILTER_SELECT		0x06
#define AK4331_07_DAC_MONO_MIXING			0x07
#define AK4331_08_JITTER_CLEANER_SETTING1	0x08
#define AK4331_09_JITTER_CLEANER_SETTING2	0x09
#define AK4331_0A_JITTER_CLEANER_SETTING3	0x0A
#define AK4331_0B_LCH_OUTPUT_VOLUME			0x0B
#define AK4331_0C_RCH_OUTPUT_VOLUME			0x0C
#define AK4331_0D_HP_VOLUME_CONTROL			0x0D
#define AK4331_0E_PLL_CLK_SOURCE_SELECT		0x0E
#define AK4331_0F_PLL_REF_CLK_DIVIDER1		0x0F
#define AK4331_10_PLL_REF_CLK_DIVIDER2		0x10
#define AK4331_11_PLL_FB_CLK_DIVIDER1		0x11
#define AK4331_12_PLL_FB_CLK_DIVIDER2		0x12
#define AK4331_13_SRC_CLK_SOURCE			0x13
#define AK4331_14_DAC_CLK_DIVIDER			0x14
#define AK4331_15_AUDIO_IF_FORMAT			0x15
#define AK4331_16_DIGITAL_MIC				0x16
#define AK4331_17_SIDE_TONE_VOLUME_CONTROL	0x17
#define AK4331_26_DAC_ADJUSTMENT1		    0x26
#define AK4331_27_DAC_ADJUSTMENT2		    0x27
#define AK4331_29_MODE_COMTROL              0x29

#define AK4331_MAX_REGISTER	   AK4331_29_MODE_COMTROL

/* Bitfield Definitions */

/* AK4331_15_AUDIO_IF_FORMAT (0x15) Fields */
#define AK4331_DIF					0x14
#define AK4331_DIF_I2S_MODE		(0 << 2)
#define AK4331_DIF_MSB_MODE		(1 << 2)

#define AK4331_SLAVE_MODE			(0 << 4)
#define AK4331_MASTER_MODE			(1 << 4)

/*
 * AK4331_05_CLOCK_MODE_SELECT (0x05) Fields
 * AK4331_08_JITTER_CLEANER_SETTING1 (0x08) Fields
 */
#define AK4331_FS				0x1F
#define AK4331_FS_8KHZ			(0x00 << 0)
#define AK4331_FS_11_025KHZ		(0x01 << 0)
#define AK4331_FS_16KHZ			(0x04 << 0)
#define AK4331_FS_22_05KHZ		(0x05 << 0)
#define AK4331_FS_32KHZ			(0x08 << 0)
#define AK4331_FS_44_1KHZ		(0x09 << 0)
#define AK4331_FS_48KHZ			(0x0A << 0)
#define AK4331_FS_88_2KHZ		(0x0D << 0)
#define AK4331_FS_96KHZ			(0x0E << 0)
#define AK4331_FS_176_4KHZ		(0x11 << 0)
#define AK4331_FS_192KHZ		(0x12 << 0)

#define AK4331_CM		(0x03 << 5)
#define AK4331_CM_0		(0 << 5)
#define AK4331_CM_1		(1 << 5)
#define AK4331_CM_2		(2 << 5)
#define AK4331_CM_3		(3 << 5)

/*Defined Sentence for Timer */
#define LVDTM_HOLD_TIME		30	// (msec)
#define VDDTM_HOLD_TIME		500	// (msec)
#define HPTM_HOLD_TIME		15	// (msec)

/*Defined Sentence for Soft Mute Cycle(ms)*/
#define SMUTE_TIME_MODE  0    // 0:22msec 1:43msec

#define AK4331_JACK_MASK  (SND_JACK_MECHANICAL | \
		SND_JACK_LINEOUT | \
		SND_JACK_HEADPHONE)

#endif
