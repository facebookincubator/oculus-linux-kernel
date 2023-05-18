/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2016, 2018, 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef MDSS_MDP_DEBUG_H
#define MDSS_MDP_DEBUG_H

#include <linux/msm_mdp.h>
#include <linux/stringify.h>

#include "mdss.h"
#include "mdss_mdp.h"

#define MDP_DANGER_SAFE_BIT_OFFSET	0
#define VIG_DANGER_SAFE_BIT_OFFSET	4
#define RGB_DANGER_SAFE_BIT_OFFSET	12
#define DMA_DANGER_SAFE_BIT_OFFSET	20
#define CURSOR_DANGER_SAFE_BIT_OFFSET	24

#define DANGER_SAFE_STATUS(X, Y) (((X) & (BIT(Y) | BIT((Y)+1))) >> (Y))

static inline const char *mdss_mdp_pipetype2str(u32 ptype)
{
	static const char * const strings[] = {
#define PIPE_TYPE(t)[MDSS_MDP_PIPE_TYPE_ ## t] = __stringify(t)
		PIPE_TYPE(VIG),
		PIPE_TYPE(RGB),
		PIPE_TYPE(DMA),
		PIPE_TYPE(CURSOR),
#undef PIPE_TYPE
	};

	if (ptype >= ARRAY_SIZE(strings) || !strings[ptype])
		return "UNKNOWN";

	return strings[ptype];
}

static inline const char *mdss_mdp_format2str(u32 format)
{
	static const char * const strings[] = {
#define FORMAT_NAME(f)[MDP_ ## f] = __stringify(f)
		FORMAT_NAME(RGB_565),
		FORMAT_NAME(BGR_565),
		FORMAT_NAME(RGB_888),
		FORMAT_NAME(BGR_888),
		FORMAT_NAME(RGBX_8888),
		FORMAT_NAME(RGBA_8888),
		FORMAT_NAME(ARGB_8888),
		FORMAT_NAME(XRGB_8888),
		FORMAT_NAME(BGRA_8888),
		FORMAT_NAME(BGRX_8888),
		FORMAT_NAME(Y_CBCR_H2V2_VENUS),
		FORMAT_NAME(Y_CBCR_H2V2),
		FORMAT_NAME(Y_CRCB_H2V2),
		FORMAT_NAME(Y_CB_CR_H2V2),
		FORMAT_NAME(Y_CR_CB_H2V2),
		FORMAT_NAME(Y_CR_CB_GH2V2),
		FORMAT_NAME(YCBYCR_H2V1),
		FORMAT_NAME(YCRYCB_H2V1),
		FORMAT_NAME(RGBA_8888_UBWC),
		FORMAT_NAME(RGBX_8888_UBWC),
		FORMAT_NAME(RGB_565_UBWC),
		FORMAT_NAME(Y_CBCR_H2V2_UBWC)
#undef FORMAT_NAME
	};

	if (format >= ARRAY_SIZE(strings) || !strings[format])
		return "UNKNOWN";

	return strings[format];
}
void mdss_mdp_dump(struct mdss_data_type *mdata);
void mdss_mdp_hw_rev_debug_caps_init(struct mdss_data_type *mdata);


#ifdef CONFIG_DEBUG_FS
int mdss_mdp_debugfs_init(struct mdss_data_type *mdata);
#else
static inline int mdss_mdp_debugfs_init(struct mdss_data_type *mdata)
{
	return 0;
}
#endif

#endif /* MDSS_MDP_DEBUG_H */
