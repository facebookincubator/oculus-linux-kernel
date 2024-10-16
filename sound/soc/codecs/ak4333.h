/*
 * ak4333.h  --  audio driver for AK4333
 *
 * Copyright (C) 2022 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision      DS ver.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      22/03/11	    1.0           00
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _AK4333_H
#define _AK4333_H

#define AK4333_00_POWER_MANAGEMENT1		0x00
#define AK4333_01_POWER_MANAGEMENT2		0x01
#define AK4333_02_POWER_MANAGEMENT3		0x02
#define AK4333_03_POWER_MANAGEMENT4		0x03
#define AK4333_04_OUTPUT_MODE_SETTING	0x04
#define AK4333_05_CLOCK_MODE_SELECT		0x05
#define AK4333_06_DIGITAL_FILTER_SELECT	0x06
#define AK4333_07_DAC_MONO_MIXING		0x07
#define AK4333_08_IMPEDANCE_DET_SETTING	0x08
#define AK4333_09_IMPEDANCE_DET_STATUS	0x09
#define AK4333_0A_DAC_CLK_SOURCE_SELECT	0x0A
#define AK4333_0B_LCH_OUTPUT_VOLUME		0x0B
#define AK4333_0C_RCH_OUTPUT_VOLUME		0x0C
#define AK4333_0D_HP_VOLUME_CONTROL		0x0D
#define AK4333_0E_PLL_CLK_SOURCE_SELECT	0x0E
#define AK4333_0F_PLL_REF_CLK_DIVIDER1	0x0F
#define AK4333_10_PLL_REF_CLK_DIVIDER2	0x10
#define AK4333_11_PLL_FB_CLK_DIVIDER1	0x11
#define AK4333_12_PLL_FB_CLK_DIVIDER2	0x12
#define AK4333_13_IMPEDANCE_DET_RESULT	0x13
#define AK4333_14_DAC_CLK_DIVIDER		0x14
#define AK4333_15_AUDIO_IF_FORMAT		0x15
#define AK4333_26_DAC_ADJUSTMENT1		0x26
#define AK4333_27_DAC_ADJUSTMENT2		0x27
#define AK4333_28_DAC_ADJUSTMENT3		0x28
#define AK4333_MAX_REGISTER			AK4333_28_DAC_ADJUSTMENT3

#define GPO_PDN_HIGH	1
#define GPO_PDN_LOW		0

#define AK4333_DIF			(0x05 << 2)
#define AK4333_DIF_I2S_MODE	(0x00 << 2)
#define AK4333_DIF_MSB_MODE	(0x01 << 2)
#define AK4333_SLAVE_MODE	(0x00 << 4)
#define AK4333_MASTER_MODE	(0x01 << 4)

#define AK4333_FS			(0x1F << 0)
#define AK4333_FS_8KHZ		(0x00 << 0)
#define AK4333_FS_11_025KHZ	(0x01 << 0)
#define AK4333_FS_16KHZ		(0x04 << 0)
#define AK4333_FS_22_05KHZ	(0x05 << 0)
#define AK4333_FS_32KHZ		(0x08 << 0)
#define AK4333_FS_44_1KHZ	(0x09 << 0)
#define AK4333_FS_48KHZ		(0x0A << 0)
#define AK4333_FS_88_2KHZ	(0x0D << 0)
#define AK4333_FS_96KHZ		(0x0E << 0)
#define AK4333_FS_176_4KHZ	(0x11 << 0)
#define AK4333_FS_192KHZ	(0x12 << 0)

#define AK4333_CM	(0x03 << 5)
#define AK4333_CM_0	(0 << 5)
#define AK4333_CM_1	(1 << 5)
#define AK4333_CM_2	(2 << 5)
#define AK4333_CM_3	(3 << 5)

#define AK4333_REG_26_VAL 0x02
#define AK4333_REG_27_VAL 0xC2
#define AK4333_REG_28_VAL 0x01

#define AK4333_JACK_MASK  (SND_JACK_MECHANICAL | \
		SND_JACK_LINEOUT | \
		SND_JACK_HEADPHONE)

#endif
