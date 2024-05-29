/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021,, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_VIDC_IRIS2_H_
#define _MSM_VIDC_IRIS2_H_

#include "msm_vidc_core.h"

#if defined(CONFIG_MSM_VIDC_IRIS2)
int msm_vidc_init_iris2(struct msm_vidc_core *core);
int msm_vidc_deinit_iris2(struct msm_vidc_core *core);
int msm_vidc_adjust_blur_type_iris2(void *instance, struct v4l2_ctrl *ctrl);
#else
static inline int msm_vidc_init_iris2(struct msm_vidc_core *core)
{
	return -EINVAL;
}
static inline int msm_vidc_deinit_iris2(struct msm_vidc_core *core)
{
	return -EINVAL;
}
static inline int msm_vidc_adjust_blur_type_iris2(void *instance, struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}
#endif

#endif // _MSM_VIDC_IRIS2_H_
