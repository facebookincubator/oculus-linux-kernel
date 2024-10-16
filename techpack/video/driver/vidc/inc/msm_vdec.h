/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021,, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_VDEC_H_
#define _MSM_VDEC_H_

#include "msm_vidc_core.h"
#include "msm_vidc_inst.h"

int msm_vdec_streamoff_input(struct msm_vidc_inst *inst);
int msm_vdec_streamon_input(struct msm_vidc_inst *inst);
int msm_vdec_streamoff_output(struct msm_vidc_inst *inst);
int msm_vdec_streamon_output(struct msm_vidc_inst *inst);
int msm_vdec_qbuf(struct msm_vidc_inst *inst, struct vb2_buffer *vb2);
int msm_vdec_try_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f);
int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f);
int msm_vdec_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f);
int msm_vdec_s_selection(struct msm_vidc_inst* inst, struct v4l2_selection* s);
int msm_vdec_g_selection(struct msm_vidc_inst* inst, struct v4l2_selection* s);
int msm_vdec_subscribe_event(struct msm_vidc_inst *inst,
		const struct v4l2_event_subscription *sub);
int msm_vdec_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f);
int msm_vdec_inst_init(struct msm_vidc_inst *inst);
int msm_vdec_inst_deinit(struct msm_vidc_inst *inst);
int msm_vdec_init_input_subcr_params(struct msm_vidc_inst *inst);
int msm_vdec_input_port_settings_change(struct msm_vidc_inst *inst);
int msm_vdec_output_port_settings_change(struct msm_vidc_inst *inst);
int msm_vdec_process_cmd(struct msm_vidc_inst *inst, u32 cmd);
int msm_vdec_handle_release_buffer(struct msm_vidc_inst *inst,
	struct msm_vidc_buffer *buf);
int msm_vdec_set_num_comv(struct msm_vidc_inst *inst);
int msm_vdec_get_input_internal_buffers(struct msm_vidc_inst *inst);
int msm_vdec_create_input_internal_buffers(struct msm_vidc_inst *inst);
int msm_vdec_queue_input_internal_buffers(struct msm_vidc_inst *inst);
int msm_vdec_release_input_internal_buffers(struct msm_vidc_inst *inst);

#endif // _MSM_VDEC_H_
