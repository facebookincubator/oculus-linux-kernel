/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 */

#ifndef __AUDIO_EXT_CLK_UP_H_
#define __AUDIO_EXT_CLK_UP_H_

#ifdef CONFIG_SND_SOC_WCD9XXX_V2
int audio_ref_clk_platform_init(void);
void audio_ref_clk_platform_exit(void);
#endif

#endif
