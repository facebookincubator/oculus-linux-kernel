/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_H_
#define _MSM_CVP_H_

#include "msm_cvp_internal.h"
#include "msm_cvp_common.h"
#include "msm_cvp_clocks.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_dsp.h"

static inline bool is_buf_param_valid(u32 buf_num, u32 offset)
{
	int max_buf_num;

	max_buf_num = sizeof(struct eva_kmd_hfi_packet) /
			sizeof(struct cvp_buf_type);

	if (buf_num > max_buf_num)
		return false;

	if ((offset + buf_num * sizeof(struct cvp_buf_type)) >
			sizeof(struct eva_kmd_hfi_packet))
		return false;

	return true;
}

int msm_cvp_handle_syscall(struct msm_cvp_inst *inst, struct eva_kmd_arg *arg);
int msm_cvp_session_init(struct msm_cvp_inst *inst);
int msm_cvp_session_deinit(struct msm_cvp_inst *inst);
int msm_cvp_session_queue_stop(struct msm_cvp_inst *inst);
int msm_cvp_session_create(struct msm_cvp_inst *inst);
int msm_cvp_session_delete(struct msm_cvp_inst *inst);
int msm_cvp_get_session_info(struct msm_cvp_inst *inst, u32 *session);
int msm_cvp_update_power(struct msm_cvp_inst *inst);
int cvp_clean_session_queues(struct msm_cvp_inst *inst);
int msm_eva_set_sw_pc(u32 data);
#endif
