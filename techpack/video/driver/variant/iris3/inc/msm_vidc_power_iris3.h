/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __H_MSM_VIDC_POWER_IRIS3_H__
#define __H_MSM_VIDC_POWER_IRIS3_H__

#include "msm_vidc_power.h"
#include "msm_vidc_inst.h"

u64 msm_vidc_calc_freq_iris3(struct msm_vidc_inst* inst, u32 data_size);
int msm_vidc_calc_bw_iris3(struct msm_vidc_inst* inst,
		struct vidc_bus_vote_data* vote_data);

#endif
